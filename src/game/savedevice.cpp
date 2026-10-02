#include "game/savedevice.h"

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

// The bytes the file holds
void* FileData(const SaveFile* file)
{
    return file->stream != nullptr ? file->stream->begin : nullptr;
}
}

SaveDevice* SaveDevice::Construct(SaveDevice* device, u32 fileCount, SaveIconFiles* icons, SaveFile* mainFile, const char* name)
{
    device->icons = icons;
    device->unknown0C = 0;
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

void SaveDevice::FileDate(SaveSummary* summary)
{
    SaveDate& date = summary->date;
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
