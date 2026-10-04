#include "game/savedevice.h"

#include "game/clock.h"
#include "game/memory.h"
#include "game/stream.h"
#include "retail/libc.h"

extern "C"
{
    extern const GccVTableEntry g_SaveDeviceVTable[] RETAIL(D_003065C8);
    extern const GccVTableEntry g_MemoryCardDeviceVTable[] RETAIL(D_003062D0);
}

namespace
{
// The game's region and product codes, which name its saves
constexpr char Region[] = "BE";
constexpr char Product[] = "SLES-52568";

using Platform::Saves::Operation;

Operation OperationOf(u32 request)
{
    switch (request)
    {
    case SaveDevice::RequestFormat:
        return Operation::Format;
    case SaveDevice::RequestMeasureSave:
        return Operation::MeasureSave;
    case SaveDevice::RequestCreateSave:
        return Operation::CreateSave;
    case SaveDevice::RequestWrite:
    case SaveDevice::RequestWriteAgain:
        return Operation::Write;
    case SaveDevice::RequestRead:
    case SaveDevice::RequestReadAgain:
        return Operation::Read;
    case SaveDevice::RequestFindFile:
        return Operation::FindFile;
    default:
        return Operation::None;
    }
}

u32 Failed(SaveDevice* device)
{
    device->flags = (device->flags & ~SaveDevice::StateMask) | SaveDevice::StateFailed;
    return 1;
}

// The device's operations (asked of it by the save code): the card checked for after the wait, the card formatted, the save
// measured, the save made (then every file written), the folder's file written, the folder's file found and read, a file found, a
// file (bits 4-7 of the flags) and the folder's file written, a file read, a wait while there's a card (10 to 12), a wait
enum DeviceOperation : u32
{
    OperationCheck = 1,
    OperationFormat = 2,
    OperationMeasure = 3,
    OperationCreate = 4,
    OperationWriteFolder = 5,
    OperationReadFolder = 6,
    OperationFind = 7,
    OperationWriteFile = 8,
    OperationReadFile = 9,
    OperationWaitCard = 10,
    OperationWaitCard2 = 11,
    OperationWaitCard3 = 12,
    OperationWait = 13,
};

// The steps the operations go through (bits 20-25 of the flags): an operation's first step asks for its request, the next polls it
// until it's done, the third ends the operation (back to 0, or 1 when it failed). Reads and writes go through the request files one
// at a time, the reads after the file was found
enum Step : u32
{
    StepDone = 0,
    StepFailed = 1,
    StepCheck = 2,
    StepChecking = 3,
    StepChecked = 4,
    StepFormat = 5,
    StepFormatting = 6,
    StepFormatted = 7,
    StepMeasure = 8,
    StepMeasuring = 9,
    StepMeasured = 10,
    StepCreate = 11,
    StepCreating = 12,
    StepCreated = 13,
    StepWriteFolder = 14,
    StepWritingFolder = 15,
    StepWroteFolder = 16,
    StepReadFolder = 17,
    StepReadingFolder = 18,
    StepReadFolderDone = 19,
    StepFind = 20,
    StepFinding = 21,
    StepFound = 22,
    StepWriteFile = 23,
    StepWritingFile = 24,
    StepWroteFile = 25,
    StepReadFile = 26,
    StepReadingFile = 27,
    StepReadFileDone = 28,
    StepWaitCard = 29,
    StepWaitCard2 = 30,
    StepWaitCard3 = 31,
    StepWait = 32,
};

// The device's vtable's update (4) and what it does after a step (5)
constexpr u32 UpdateSlot = 4;
constexpr u32 SteppedSlot = 5;
// Bits 27-30 of the flags: how many request files
constexpr u32 RequestCountMask = 0x78000000;

// The step's wait is over
u32 Waited(const SaveDevice* device, const TimeClock* clock)
{
    return static_cast<s32>(clock->time - device->stepStart) >= device->wait;
}

void SetRequestFiles(SaveDevice* device, u32 count)
{
    device->current &= ~SaveDevice::OperationMask;
    device->flags = (device->flags & ~RequestCountMask) | (count & 0xF) << SaveDevice::RequestCountShift;
}

u32 FileThrough(SaveFile* file, u32 slot)
{
    return CallVirtual<u32>(file, file->vtable, slot);
}

// The bytes the file holds
void* FileData(const SaveFile* file)
{
    return file->stream != nullptr ? file->stream->begin : nullptr;
}
}

SaveDevice* SaveDevice::Construct(SaveDevice* device, u32 fileCount, SaveIconFiles* icons, SaveFile* mainFile, const char* name)
{
    device->icons = icons;
    device->wait = 0;
    device->mainFile = mainFile;
    device->vtable = g_SaveDeviceVTable;
    RetailLibc::MemorySet(device, 0, 8);
    u32 bits = device->flags & 0xFC0FFFFF & ~0xF00u & 0xFFFF0FFF;
    device->flags = ((bits | 0xE000) & ~FileCountMask) | (fileCount & FileCountMask);
    device->files = static_cast<SaveFile**>(MemoryAllocate2(fileCount << 2));
    for (u32 i = 0; i < fileCount; i++)
    {
        device->files[i] = nullptr;
    }

    u32 requestFiles = fileCount << 1;
    device->requestFiles = static_cast<SaveFile**>(MemoryAllocate2(requestFiles << 2));
    for (u32 i = 0; i < requestFiles; i++)
    {
        device->requestFiles[i] = nullptr;
    }

    device->port = 0;
    device->slot = 0;
    device->vtable = g_MemoryCardDeviceVTable;
    device->product = {nullptr, 0, 0};
    device->name = {nullptr, 0, 0};
    device->saveKilobytes = 0;
    device->fileSize = 0;
    Platform::Saves::Initialise(Region, Product);
    StringAssign(&device->product, Region);
    StringAppend(&device->product, Product);
    StringAssign(&device->name, name);
    return device;
}

void SaveDevice::FileDate(SaveFile* file)
{
    SaveDate& date = file->date;
    const u8* modified = fileEntry.modified;
    date.bits |= 3;
    date.day = modified[4];
    date.month = modified[5];
    date.year = static_cast<u16>(modified[6] | modified[7] << 8);
    date.second = modified[1];
    date.hour = modified[3];
    date.minute = modified[2];
}

// Without a memory card the request failed. Not asked: the state
u32 SaveDevice::Poll(u32 request, u32 next, u32 asked)
{
    if (CallVirtual<u32>(this, vtable, 7) == 0)
    {
        return Failed(this);
    }

    if (asked != 0)
    {
        Platform::Saves::Result result = Platform::Saves::GetResult(OperationOf(request));
        if (result.succeeded == 0)
        {
            return Failed(this);
        }

        if (result.succeeded == 1)
        {
            return next;
        }
    }

    return flags >> 20 & 0x3F;
}

u32 SaveDevice::Request(u32 request, u32 next)
{
    bool started;
    switch (request)
    {
    case RequestFormat:
        started = Platform::Saves::Format(port, slot);
        break;
    case RequestMeasureSave:
        started = Platform::Saves::MeasureSave(port, slot, name.string, &saveKilobytes);
        break;
    case RequestCreateSave:
        started = Platform::Saves::CreateSave(port, slot, name.string);
        break;
    case RequestWrite:
    case RequestWriteAgain:
    {
        SaveFile* file = requestFiles[current & 0xF];
        started = Platform::Saves::Write(port, slot, name.string, file->name.string, FileData(file), file->size);
        break;
    }
    case RequestRead:
    case RequestReadAgain:
    {
        SaveFile* file = requestFiles[current & 0xF];
        started = Platform::Saves::Read(port, slot, name.string, file->name.string, FileData(file), file->size);
        break;
    }
    case RequestFindFile:
    {
        SaveFile* file = requestFiles[current & 0xF];
        started = Platform::Saves::FindFile(port, slot, file->name.string, name.string, &fileSize, &fileEntry);
        break;
    }
    default:
        return Failed(this);
    }

    return started ? next : Failed(this);
}

void SaveDevice::Update()
{
    operation = Platform::Saves::Update(port, slot, &info);
}

void SaveDevice::Unknown5()
{
}

void SaveDevice::Destroy(u32 destroyFlags)
{
    vtable = g_MemoryCardDeviceVTable;
    StringDestroy(&name);
    StringDestroy(&product);
    vtable = g_SaveDeviceVTable;
    if (files != nullptr)
    {
        MemoryDeallocate_(files);
    }

    if (requestFiles != nullptr)
    {
        MemoryDeallocate_(requestFiles);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 SaveDevice::IsMemoryCard()
{
    return info.type == Platform::Saves::StorageMemoryCard;
}

u32 SaveDevice::IsFormatted()
{
    return info.formatted == 1;
}

u32 SaveDevice::SaveExists()
{
    return saveKilobytes >= 0;
}

u32 SaveDevice::SaveSize()
{
    return static_cast<u32>(saveKilobytes) << 10;
}

s32 SaveDevice::FileSize()
{
    return fileSize;
}

// The icons, the main file and the save's own files, a directory entry each
u32 SaveDevice::SaveSpace()
{
    u32 kilobytes = 0;
    u32 count = 0;
    if (icons->iconSys != nullptr)
    {
        count = 1;
        kilobytes = Platform::Saves::FileKilobytes(icons->iconSys->size);
    }

    for (u32 i = 0; i < icons->count; i++)
    {
        kilobytes += Platform::Saves::FileKilobytes(icons->files[i]->size);
        count++;
    }

    kilobytes += Platform::Saves::FileKilobytes(mainFile->size);
    count++;
    for (u32 i = 0; i < (flags & FileCountMask); i++)
    {
        kilobytes += Platform::Saves::FileKilobytes(files[i]->size);
        count++;
    }

    return Platform::Saves::SaveBytes(kilobytes, count);
}

u32 SaveDevice::FreeSpace()
{
    return static_cast<u32>(info.freeKilobytes) << 10;
}

u32 SaveDevice::Ask(u32 asked, u32 file)
{
    if ((flags >> RunningShift & OperationMask) == asked)
    {
        return 1;
    }

    flags = (flags & ~(OperationMask << AskedShift)) | (asked & OperationMask) << AskedShift;
    u32 step;
    switch (asked)
    {
    case OperationCheck:
        step = StepCheck;
        break;
    case OperationFormat:
        step = StepFormat;
        break;
    case OperationMeasure:
        step = StepMeasure;
        break;
    case OperationCreate:
        step = StepCreate;
        break;
    case OperationWriteFolder:
        step = StepWriteFolder;
        break;
    case OperationReadFolder:
        step = StepReadFolder;
        break;
    case OperationFind:
        step = StepFind;
        break;
    case OperationWriteFile:
        flags = (flags & ~(OperationMask << FileShift)) | (file & OperationMask) << FileShift;
        step = StepWriteFile;
        break;
    case OperationReadFile:
        flags = (flags & ~(OperationMask << FileShift)) | (file & OperationMask) << FileShift;
        step = StepReadFile;
        break;
    case OperationWaitCard:
        step = StepWaitCard;
        break;
    case OperationWaitCard2:
        step = StepWaitCard2;
        break;
    case OperationWaitCard3:
        step = StepWaitCard3;
        break;
    case OperationWait:
        step = StepWait;
        break;
    default:
        return 0;
    }

    flags = (flags & ~(StepMask << StepShift)) | step << StepShift;
    return 1;
}

u32 SaveDevice::Step(TimeClock* clock)
{
    u32 step = flags >> StepShift & StepMask;
    CallVirtual<void>(this, vtable, UpdateSlot);
    u32 asked = flags >> AskedShift & OperationMask;
    if (asked < AskTaken)
    {
        flags = (flags & ~(OperationMask << RunningShift)) | asked << RunningShift;
        flags = (flags & ~(OperationMask << AskedShift)) | AskTaken << AskedShift;
        stepStart = clock->time;
    }

    flags &= ~StateMask;
    switch (step)
    {
    case StepFailed:
    case StepChecked:
    case StepFormatted:
    case StepMeasured:
    case StepReadFolderDone:
    case StepFound:
    case StepWroteFile:
    case StepReadFileDone:
        step = StepDone;
        break;
    case StepCheck:
        step = StepChecking;
        break;
    case StepChecking:
        if (Waited(this, clock) == 0)
        {
            step = flags >> StepShift & StepMask;
        }
        else if (HasCard() == 0)
        {
            step = Failed(this);
        }
        else
        {
            step = StepChecked;
        }

        break;
    case StepFormat:
        step = RequestThrough(RequestFormat, StepFormatting);
        break;
    case StepFormatting:
        step = PollThrough(RequestFormat, StepFormatted, Waited(this, clock));
        break;
    case StepMeasure:
        step = RequestThrough(RequestMeasureSave, StepMeasuring);
        break;
    case StepMeasuring:
        step = PollThrough(RequestMeasureSave, StepMeasured, Waited(this, clock));
        break;
    case StepCreate:
        static_cast<FolderFile*>(mainFile)->ClearSummaries();
        step = RequestThrough(RequestCreateSave, StepCreating);
        break;
    case StepCreating:
        step = PollThrough(RequestCreateSave, StepCreated, Waited(this, clock));
        break;
    case StepCreated:
        flags |= WritesEverything;
        step = StepWriteFolder;
        break;
    case StepWriteFolder:
        step = StartWritingFiles();
        break;
    case StepWritingFolder:
        step = PollWrites(RequestWrite, StepWroteFolder, clock);
        break;
    case StepWroteFolder:
        flags &= ~WritesEverything;
        step = StepDone;
        break;
    case StepReadFolder:
        static_cast<FolderFile*>(mainFile)->ClearSummaries();
        requestFiles[0] = mainFile;
        SetRequestFiles(this, 1);
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepReadingFolder:
        step = PollReads(RequestRead, StepReadFolderDone, clock);
        break;
    case StepFind:
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepFinding:
        step = PollFind(clock);
        break;
    case StepWriteFile:
    {
        SaveFile* file = files[(flags & 0xFF) >> FileShift];
        requestFiles[0] = file;
        requestFiles[1] = mainFile;
        SetRequestFiles(this, 2);
        if (FileThrough(file, SaveFile::BeginWriteSlot) == 0)
        {
            step = Failed(this);
        }
        else
        {
            step = RequestThrough(RequestWriteAgain, StepWritingFile);
        }

        break;
    }
    case StepWritingFile:
        step = PollWrites(RequestWriteAgain, StepWroteFile, clock);
        break;
    case StepReadFile:
        requestFiles[0] = files[(flags & 0xFF) >> FileShift];
        SetRequestFiles(this, 1);
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepReadingFile:
        step = PollReads(RequestReadAgain, StepReadFileDone, clock);
        break;
    case StepWaitCard:
    case StepWaitCard2:
    case StepWaitCard3:
        if (HasCard() == 0 || Waited(this, clock) != 0)
        {
            step = StepDone;
        }

        break;
    case StepWait:
        step = Waited(this, clock) != 0 ? StepDone : StepWait;
        break;
    default:
        break;
    }

    CallVirtual<void>(this, vtable, SteppedSlot);
    flags = (flags & ~(StepMask << StepShift)) | (step & StepMask) << StepShift;
    if (step == StepDone)
    {
        flags &= ~(OperationMask << AskedShift);
        return 0;
    }

    return step != StepFailed;
}

// The request polled; once it's done with the file, the read checked and the next file's read asked for (in the same step), the
// next step once every file was read
u32 SaveDevice::PollReads(u32 request, u32 next, TimeClock* clock)
{
    u32 step = PollThrough(request, next, Waited(this, clock));
    if (step != next)
    {
        return step;
    }

    if (FileThrough(requestFiles[current & OperationMask], SaveFile::EndReadSlot) != 0)
    {
        current = (current & ~OperationMask) | (((current & OperationMask) + 1) & OperationMask);
        u32 index = current & OperationMask;
        if (index >= (flags >> RequestCountShift & 0xF))
        {
            return step;
        }

        if (FileThrough(requestFiles[index], SaveFile::BeginReadSlot) != 0)
        {
            return RequestThrough(request, flags >> StepShift & StepMask);
        }
    }

    return SetState(StateFailed >> StateShift);
}

u32 SaveDevice::PollWrites(u32 request, u32 next, TimeClock* clock)
{
    u32 step = PollThrough(request, next, Waited(this, clock));
    if (step != next)
    {
        return step;
    }

    if (FileThrough(requestFiles[current & OperationMask], SaveFile::EndWriteSlot) != 0)
    {
        current = (current & ~OperationMask) | (((current & OperationMask) + 1) & OperationMask);
        u32 index = current & OperationMask;
        if (index >= (flags >> RequestCountShift & 0xF))
        {
            return step;
        }

        if (FileThrough(requestFiles[index], SaveFile::BeginWriteSlot) != 0)
        {
            return RequestThrough(request, flags >> StepShift & StepMask);
        }
    }

    return SetState(StateFailed >> StateShift);
}

// Once the file was found, a read of it begun and asked for (the read operations go on to their reading step, a find alone ends)
u32 SaveDevice::PollFind(TimeClock* clock)
{
    u32 running = flags >> RunningShift & OperationMask;
    u32 next = StepFound;
    if (running == OperationReadFolder)
    {
        next = StepReadingFolder;
    }
    else if (running == OperationReadFile)
    {
        next = StepReadingFile;
    }

    u32 step = PollThrough(RequestFindFile, next, Waited(this, clock));
    if (step != StepReadingFolder && step != StepReadingFile)
    {
        return step;
    }

    if (FileThrough(requestFiles[current & OperationMask], SaveFile::BeginReadSlot) == 0)
    {
        return Failed(this);
    }

    return RequestThrough(running, step);
}

// The files written: the folder's alone, or once the save was made every file (icon.sys, the icons, the save's files, the
// folder's)
u32 SaveDevice::StartWritingFiles()
{
    u32 count = 0;
    if ((flags & WritesEverything) != 0)
    {
        if (icons->iconSys != nullptr)
        {
            requestFiles[0] = icons->iconSys;
            count = 1;
        }

        for (u32 i = 0; i < icons->count; i++)
        {
            requestFiles[count++] = icons->files[i];
        }

        for (u32 i = 0; i < (flags & FileCountMask); i++)
        {
            requestFiles[count++] = files[i];
        }

        requestFiles[count++] = mainFile;
    }
    else
    {
        requestFiles[0] = mainFile;
        count = 1;
    }

    SetRequestFiles(this, count);
    if (FileThrough(requestFiles[0], SaveFile::BeginWriteSlot) == 0)
    {
        return Failed(this);
    }

    return RequestThrough(RequestWrite, StepWritingFolder);
}

u32 SaveDevice::SetState(u32 state)
{
    flags = (flags & ~StateMask) | (state & 0xF) << StateShift;
    return 1;
}

void SaveDevice::DestroyBase(u32 destroyFlags)
{
    vtable = g_SaveDeviceVTable;
    if (files != nullptr)
    {
        MemoryDeallocate_(files);
    }

    if (requestFiles != nullptr)
    {
        MemoryDeallocate_(requestFiles);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

extern "C"
{
    void SetIconLightDirection(u8* iconSys, s32 light, const Vector4* direction)
    {
        f32* to = reinterpret_cast<f32*>(iconSys + (light << 4) + 0x88);
        to[0] = direction->x;
        to[1] = direction->y;
        to[2] = direction->z;
        to[3] = direction->w;
    }
}
