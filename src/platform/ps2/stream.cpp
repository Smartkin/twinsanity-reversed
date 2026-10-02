#include "platform/stream.h"

#include "platform/files.h"
#include "multistream/multistream.h"

#include <kernel.h>

// The streams of the I/O processor's sound and stream module (MultiStream): its channels are the module's streams
using namespace MultiStream;
using Platform::Stream::State;

namespace
{
// The module's server on the EE runs at this priority, the main thread right below it
constexpr s32 ServerPriority = 9;
constexpr s32 MainThreadPriority = 10;
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

    MsCommand2A(2);
    MsCommand2A(5);
    MsConfigure(0, static_cast<u16>(channels * 4), 0);
    MsSetStreamCount(static_cast<u32>(channels));
    ChangeThreadPriority(GetThreadId(), MainThreadPriority);
    MsStartServer(ServerPriority, workMemory, WorkMemorySize);
    MsSetServerMode(1);
    MsSendBatch(0);
    s32 result = MsSetServerMode(4);
    g_State = State::Running;
    return result;
}

s32 Platform::Stream::Update()
{
    StreamStatus status;
    if (g_State == State::Running)
    {
        if (MsCheckDisc() == -1)
        {
            Recover();
        }

        s32 busy = MsIsBusy();
        if (busy != 0)
        {
            return busy;
        }

        return MsSend(1);
    }

    if (g_State == State::Recovering)
    {
        MsHoldReading();
        MsSend(0);
        MsGetStreamStatus(0, &status);
        if (status.discError != 0)
        {
            Recover();
            return 2;
        }

        g_RecoveryFrames--;
        if (g_RecoveryFrames <= 0)
        {
            g_State = State::Running;
        }

        return g_RecoveryFrames;
    }

    // Reading is held until the disc can be read: the module says it can and a file of it opens
    MsHoldReading();
    MsSend(0);
    MsGetStreamStatus(0, &status);
    if (status.discError == 0)
    {
        s32 file = Platform::Files::Open(DiscCheckFile, Platform::Files::OpenRead);
        if (file >= 0)
        {
            Platform::Files::Close(file);
            g_State = State::Recovering;
            g_RecoveryFrames = 0;
            MsRestartReading();
        }
    }

    return MsSend(0);
}

State Platform::Stream::GetState()
{
    return g_State;
}

void Platform::Stream::AttachBuffer(s32 channel, u32 size, u32 location, u32 used)
{
    MsSetStreamBuffer(channel, location, size);
    if (location != 0 && used != 0)
    {
        MsSetStreamBufferSize(channel, used);
    }
}

void Platform::Stream::DetachBuffer(s32 channel)
{
    MsReleaseStreamBuffer(static_cast<u32>(channel));
}

u32 Platform::Stream::FreeBufferMemory()
{
    MsQueryFreeMemory();
    MsSendBatch(0);
    StreamStatus status;
    MsGetStreamStatus(0, &status);
    return status.status50;
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
            character = static_cast<char>(character - 0x20);
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
    MsSend(0);
    u32 info[6];
    MsGetFileInfo(info);
    return info[1];
}

void Platform::Stream::CloseFile(s32 file)
{
    MsCloseFile(static_cast<u32>(file));
    MsSend(0);
}

void Platform::Stream::Read(s32 channel, s32 file, u32 offset, u32 size, void* destination)
{
    u8* begin = static_cast<u8*>(destination);
    // The module writes the memory behind the cache's back
    InvalidDCache(begin, begin + size - 1);
    MsReadFile(static_cast<u32>(file), offset, size);
    MsTransfer(MsTransferToEe, static_cast<u8>(channel), static_cast<u32>(file), reinterpret_cast<u32>(destination));
}

void Platform::Stream::ReadSoundBank(s32 channel, u32 bank, s32 file, u32 offset, u32 size)
{
    u32 address = g_MsSoundBankAddress;
    MsSetBankAddress(bank, address);
    MsReadFile(static_cast<u32>(file), offset, size);
    MsTransfer(MsTransferToSound, static_cast<u8>(channel), static_cast<u32>(file), address);
}

bool Platform::Stream::IsReading(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.state != 0;
}

s32 Platform::Stream::Wait(s32 channel)
{
    MsSend(0);
    return MsWaitForStream(static_cast<u8>(channel), 0);
}
