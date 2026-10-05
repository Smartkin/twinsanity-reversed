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

using StorageOperation = Platform::Saves::Operation;

// The storage's operation of a request
StorageOperation StorageOperationOf(u32 request)
{
    switch (request)
    {
    case SaveDevice::RequestFormat:
        return StorageOperation::Format;
    case SaveDevice::RequestMeasureSave:
        return StorageOperation::MeasureSave;
    case SaveDevice::RequestCreateSave:
        return StorageOperation::CreateSave;
    case SaveDevice::RequestWriteFolder:
    case SaveDevice::RequestWriteFile:
        return StorageOperation::Write;
    case SaveDevice::RequestReadFolder:
    case SaveDevice::RequestReadFile:
        return StorageOperation::Read;
    case SaveDevice::RequestFindFile:
        return StorageOperation::FindFile;
    default:
        return StorageOperation::None;
    }
}

// The steps the operations go through (the flags' step): an operation's first step asks for its request, the next polls it until
// it's done, the third ends the operation (back to StepDone, or StepFailed when it failed). Reads and writes go through the request
// files one at a time, the reads after the file was found
enum OperationStep : u32
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
    StepWaitFormatted = 29,
    StepWaitSaved = 30,
    StepWaitLoaded = 31,
    StepWaitCancelled = 32,
};

// The state failed: the failed step
u32 Failed(SaveDevice* device)
{
    device->flags.state = SaveDevice::StateFailed;
    return StepFailed;
}

// The step's wait is over
u32 Waited(const SaveDevice* device, const TimeClock* clock)
{
    return static_cast<s32>(clock->time - device->stepStart) >= device->wait;
}

void SetRequestFiles(SaveDevice* device, u32 count)
{
    device->requestFile.index = 0;
    device->flags.requestCount = count;
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

SaveDevice* SaveDevice::Construct(SaveDevice* device, u32 fileCount, SaveIconFiles* icons, SaveFile* folder, const char* name)
{
    device->icons = icons;
    device->wait = 0;
    device->folder = static_cast<FolderFile*>(folder);
    device->vtable = g_SaveDeviceVTable;
    // The flags and the request file index
    RetailLibc::MemorySet(device, 0, sizeof(SaveDeviceFlags) + sizeof(RequestFileIndex));
    device->flags.step = StepDone;
    device->flags.running = OperationNone;
    device->flags.asked = AskTaken;
    device->flags.fileCount = fileCount;
    device->files = static_cast<SaveFile**>(MemoryAllocate2(fileCount * sizeof(SaveFile*)));
    for (u32 i = 0; i < fileCount; i++)
    {
        device->files[i] = nullptr;
    }

    // The request files: room for the save's files twice over
    u32 requestFiles = fileCount * 2;
    device->requestFiles = static_cast<SaveFile**>(MemoryAllocate2(requestFiles * sizeof(SaveFile*)));
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
    date.bits |= SaveDate::Valid;
    date.day = modified[4];
    date.month = modified[5];
    date.year = static_cast<u16>(modified[6] | modified[7] << 8);
    date.second = modified[1];
    date.hour = modified[3];
    date.minute = modified[2];
}

// Without a memory card the request failed. Still waiting, or the request not over: the step it's at
u32 SaveDevice::Poll(u32 request, u32 next, u32 waited)
{
    if (CallVirtual<u32>(this, vtable, HasCardSlot) == 0)
    {
        return Failed(this);
    }

    if (waited != 0)
    {
        Platform::Saves::Result result = Platform::Saves::GetResult(StorageOperationOf(request));
        if (result.succeeded == 0)
        {
            return Failed(this);
        }

        if (result.succeeded == 1)
        {
            return next;
        }
    }

    return flags.step;
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
    case RequestWriteFolder:
    case RequestWriteFile:
    {
        SaveFile* file = requestFiles[requestFile.index];
        started = Platform::Saves::Write(port, slot, name.string, file->name.string, FileData(file), file->size);
        break;
    }
    case RequestReadFolder:
    case RequestReadFile:
    {
        SaveFile* file = requestFiles[requestFile.index];
        started = Platform::Saves::Read(port, slot, name.string, file->name.string, FileData(file), file->size);
        break;
    }
    case RequestFindFile:
    {
        SaveFile* file = requestFiles[requestFile.index];
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

void SaveDevice::Stepped()
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

    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    return static_cast<u32>(saveKilobytes) << KilobyteShift;
}

s32 SaveDevice::FileSize()
{
    return fileSize;
}

// The icons, the folder's file and the save's own files, a directory entry each
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

    kilobytes += Platform::Saves::FileKilobytes(folder->size);
    count++;
    for (u32 i = 0; i < flags.fileCount; i++)
    {
        kilobytes += Platform::Saves::FileKilobytes(files[i]->size);
        count++;
    }

    return Platform::Saves::SaveBytes(kilobytes, count);
}

u32 SaveDevice::FreeSpace()
{
    return static_cast<u32>(info.freeKilobytes) << KilobyteShift;
}

u32 SaveDevice::Ask(u32 asked, u32 file)
{
    if (flags.running == asked)
    {
        return 1;
    }

    flags.asked = asked;
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
        flags.file = file;
        step = StepWriteFile;
        break;
    case OperationReadFile:
        flags.file = file;
        step = StepReadFile;
        break;
    case OperationWaitFormatted:
        step = StepWaitFormatted;
        break;
    case OperationWaitSaved:
        step = StepWaitSaved;
        break;
    case OperationWaitLoaded:
        step = StepWaitLoaded;
        break;
    case OperationWaitCancelled:
        step = StepWaitCancelled;
        break;
    default:
        return 0;
    }

    flags.step = step;
    return 1;
}

u32 SaveDevice::Step(TimeClock* clock)
{
    u32 step = flags.step;
    CallVirtual<void>(this, vtable, UpdateSlot);
    u32 asked = flags.asked;
    if (asked < AskTaken)
    {
        flags.running = asked;
        flags.asked = AskTaken;
        stepStart = clock->time;
    }

    flags.state = StateOk;
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
            step = flags.step;
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
        folder->ClearSummaries();
        step = RequestThrough(RequestCreateSave, StepCreating);
        break;
    case StepCreating:
        step = PollThrough(RequestCreateSave, StepCreated, Waited(this, clock));
        break;
    case StepCreated:
        flags.writesEverything = 1;
        step = StepWriteFolder;
        break;
    case StepWriteFolder:
        step = StartWritingFiles();
        break;
    case StepWritingFolder:
        step = PollWrites(RequestWriteFolder, StepWroteFolder, clock);
        break;
    case StepWroteFolder:
        flags.writesEverything = 0;
        step = StepDone;
        break;
    case StepReadFolder:
        folder->ClearSummaries();
        requestFiles[0] = folder;
        SetRequestFiles(this, 1);
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepReadingFolder:
        step = PollReads(RequestReadFolder, StepReadFolderDone, clock);
        break;
    case StepFind:
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepFinding:
        step = PollFind(clock);
        break;
    case StepWriteFile:
    {
        SaveFile* file = files[flags.file];
        requestFiles[0] = file;
        requestFiles[1] = folder;
        SetRequestFiles(this, 2);
        if (FileThrough(file, SaveFile::BeginWriteSlot) == 0)
        {
            step = Failed(this);
        }
        else
        {
            step = RequestThrough(RequestWriteFile, StepWritingFile);
        }

        break;
    }
    case StepWritingFile:
        step = PollWrites(RequestWriteFile, StepWroteFile, clock);
        break;
    case StepReadFile:
        requestFiles[0] = files[flags.file];
        SetRequestFiles(this, 1);
        step = RequestThrough(RequestFindFile, StepFinding);
        break;
    case StepReadingFile:
        step = PollReads(RequestReadFile, StepReadFileDone, clock);
        break;
    case StepWaitFormatted:
    case StepWaitSaved:
    case StepWaitLoaded:
        if (HasCard() == 0 || Waited(this, clock) != 0)
        {
            step = StepDone;
        }

        break;
    case StepWaitCancelled:
        step = Waited(this, clock) != 0 ? StepDone : StepWaitCancelled;
        break;
    default:
        break;
    }

    CallVirtual<void>(this, vtable, SteppedSlot);
    flags.step = step;
    if (step == StepDone)
    {
        flags.asked = OperationNone;
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

    if (FileThrough(requestFiles[requestFile.index], SaveFile::EndReadSlot) != 0)
    {
        requestFile.index++;
        u32 index = requestFile.index;
        if (index >= flags.requestCount)
        {
            return step;
        }

        if (FileThrough(requestFiles[index], SaveFile::BeginReadSlot) != 0)
        {
            return RequestThrough(request, flags.step);
        }
    }

    return SetState(StateFailed);
}

u32 SaveDevice::PollWrites(u32 request, u32 next, TimeClock* clock)
{
    u32 step = PollThrough(request, next, Waited(this, clock));
    if (step != next)
    {
        return step;
    }

    if (FileThrough(requestFiles[requestFile.index], SaveFile::EndWriteSlot) != 0)
    {
        requestFile.index++;
        u32 index = requestFile.index;
        if (index >= flags.requestCount)
        {
            return step;
        }

        if (FileThrough(requestFiles[index], SaveFile::BeginWriteSlot) != 0)
        {
            return RequestThrough(request, flags.step);
        }
    }

    return SetState(StateFailed);
}

// Once the file was found, a read of it begun and asked for (the read operations go on to their reading step, a find alone ends)
u32 SaveDevice::PollFind(TimeClock* clock)
{
    u32 running = flags.running;
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

    if (FileThrough(requestFiles[requestFile.index], SaveFile::BeginReadSlot) == 0)
    {
        return Failed(this);
    }

    // The read operation's request
    return RequestThrough(running, step);
}

// The files written: the folder's alone, or once the save was made every file (icon.sys, the icons, the save's files, the
// folder's)
u32 SaveDevice::StartWritingFiles()
{
    u32 count = 0;
    if (flags.writesEverything != 0)
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

        for (u32 i = 0; i < flags.fileCount; i++)
        {
            requestFiles[count++] = files[i];
        }

        requestFiles[count++] = folder;
    }
    else
    {
        requestFiles[0] = folder;
        count = 1;
    }

    SetRequestFiles(this, count);
    if (FileThrough(requestFiles[0], SaveFile::BeginWriteSlot) == 0)
    {
        return Failed(this);
    }

    return RequestThrough(RequestWriteFolder, StepWritingFolder);
}

u32 SaveDevice::SetState(u32 state)
{
    flags.state = state;
    return StepFailed;
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

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

extern "C"
{
    void SetIconLightDirection(u8* file, s32 light, const Vector4* direction)
    {
        f32* to = reinterpret_cast<IconSysFile*>(file)->iconSys.lightDirections[light];
        to[0] = direction->x;
        to[1] = direction->y;
        to[2] = direction->z;
        to[3] = direction->w;
    }
}
