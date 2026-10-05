#include "platform/stream.h"

#include "platform/files.h"
#include "multistream/multistream.h"

#include <kernel.h>

// The streams of the I/O processor's sound and stream module (MultiStream): its channels are the module's streams
using namespace MultiStream;
using Platform::Stream::State;

namespace
{
// The module's server on the EE (the fast load's) runs at this priority, the main thread right below it
constexpr s32 ServerPriority = 9;
constexpr s32 MainThreadPriority = 10;
// The module keeps room for so many files per channel
constexpr s32 FilesPerChannel = 4;
// The file the disc is checked with after an error: it's on every disc of the game
constexpr const char* DiscCheckFile = "SLES_525.68";

State g_State = State::Stopped;
s32 g_RecoveryFrames = 0;
// Every file opened gets the next number (the module's request)
s32 g_NextFile = 0;

void Recover()
{
    g_RecoveryFrames = 0;
    g_State = State::DiscError;
}
}

s32 Platform::Stream::Initialise(s32 channels, void* workMemory)
{
    g_State = State::Stopped;
    if (MsInitialise() == -1)
    {
        return -1;
    }

    MsInitDisc(DiscDvd);
    MsInitDisc(DiscSpinStream);
    MsInitStreamData(LoadInternal, static_cast<u16>(channels * FilesPerChannel), 0);
    MsSetStreamCount(static_cast<u32>(channels));
    ChangeThreadPriority(GetThreadId(), MainThreadPriority);
    MsInitFastLoad(ServerPriority, workMemory, WorkMemorySize);
    MsSetFastLoadMode(FastLoadOn);
    MsSendBatch(SendWait);
    s32 result = MsSetFastLoadMode(FastLoadContinue);
    g_State = State::Running;
    return result;
}

s32 Platform::Stream::Update()
{
    StreamStatus status;
    if (g_State == State::Running)
    {
        if (MsHandleDiscErrors() == -1)
        {
            Recover();
        }

        s32 busy = MsIsBusy();
        if (busy != 0)
        {
            return busy;
        }

        return MsSend(SendNoWait);
    }

    if (g_State == State::Recovering)
    {
        MsCheckDiscError();
        MsSend(SendWait);
        MsGetStreamStatus(0, &status);
        if (status.discNotReady != 0)
        {
            Recover();
            return static_cast<s32>(State::DiscError);
        }

        g_RecoveryFrames--;
        if (g_RecoveryFrames <= 0)
        {
            g_State = State::Running;
        }

        return g_RecoveryFrames;
    }

    // Reading is held until the disc can be read: the module says it's ready and a file of it opens
    MsCheckDiscError();
    MsSend(SendWait);
    MsGetStreamStatus(0, &status);
    if (status.discNotReady == 0)
    {
        s32 file = Platform::Files::Open(DiscCheckFile, Platform::Files::OpenRead);
        if (file >= 0)
        {
            Platform::Files::Close(file);
            g_State = State::Recovering;
            g_RecoveryFrames = 0;
            MsRestartFromDiscError();
        }
    }

    return MsSend(SendWait);
}

State Platform::Stream::GetState()
{
    return g_State;
}

void Platform::Stream::AttachBuffer(s32 channel, u32 size, u32 soundAddress, u32 soundSize)
{
    MsAllocateStreamBuffer(channel, soundAddress, size);
    if (soundAddress != 0 && soundSize != 0)
    {
        MsResizeSpuBuffer(channel, soundSize);
    }
}

void Platform::Stream::DetachBuffer(s32 channel)
{
    MsCloseStreamBuffer(static_cast<u32>(channel));
}

u32 Platform::Stream::FreeBufferMemory()
{
    MsQueryFreeMemory();
    MsSendBatch(SendWait);
    StreamStatus status;
    MsGetStreamStatus(0, &status);
    return status.maxIopMemory;
}

s32 Platform::Stream::OpenFile(const char* path)
{
    // The module takes the disc's path: "cdrom0:\", upper case, backslashes
    constexpr char Prefix[] = "cdrom0:\\";
    char discPath[0x100];
    u32 length = 0;
    for (; Prefix[length] != 0; length++)
    {
        discPath[length] = Prefix[length];
    }

    for (const char* c = path; *c != 0 && length < sizeof(discPath) - 1; c++)
    {
        char character = *c;
        if (character >= 'a' && character <= 'z')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
        else if (character == '/')
        {
            character = '\\';
        }

        discPath[length++] = character;
    }

    discPath[length] = 0;
    s32 file = g_NextFile++;
    MsOpenFile(file, discPath, 0);
    return file;
}

u32 Platform::Stream::FileSize()
{
    // The reply to the opening has it
    MsSend(SendWait);
    FileInfo info;
    MsGetFileInfo(&info);
    return info.size;
}

void Platform::Stream::CloseFile(s32 file)
{
    MsCloseFile(static_cast<u32>(file));
    MsSend(SendWait);
}

void Platform::Stream::Read(s32 channel, s32 file, u32 offset, u32 size, void* destination)
{
    u8* begin = static_cast<u8*>(destination);
    // The module writes the memory behind the cache's back
    InvalidDCache(begin, begin + size - 1);
    MsReadFile(static_cast<u32>(file), offset, size);
    MsLoadFile(LoadToEe, static_cast<u8>(channel), static_cast<u32>(file), reinterpret_cast<u32>(destination));
}

void Platform::Stream::ReadSoundBank(s32 channel, u32 bank, s32 file, u32 offset, u32 size)
{
    u32 address = g_MsSoundBankAddress;
    MsSetBankAddress(bank, address);
    MsReadFile(static_cast<u32>(file), offset, size);
    MsLoadFile(LoadToSpu, static_cast<u8>(channel), static_cast<u32>(file), address);
}

bool Platform::Stream::IsReading(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.state != StreamOff;
}

s32 Platform::Stream::Wait(s32 channel)
{
    MsSend(SendWait);
    return MsWaitForStream(static_cast<u8>(channel), WaitCold);
}
