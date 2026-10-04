#include "game/readers.h"

#include "debug.h"

#include "game/controllers.h"
#include "game/disk.h"
#include "game/filestream.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/stream.h"
#include "game/archive.h"
#include "platform/audio.h"
#include "platform/stream.h"

namespace
{
constexpr u32 BufferCapacity = 0x5DC;
constexpr s16 StackCapacity = 0x800;

void PushFront(GameReadersStorage* storage, GenericItemReader* reader)
{
    storage->bufferStart--;
    if (storage->bufferStart == static_cast<u32>(-1))
    {
        storage->bufferStart = storage->bufferCapacity - 1;
    }

    storage->buffer[storage->bufferStart] = reader;
    storage->bufferCount++;
}

// The reader's done: it hands its section over (readers of what's in it go on the stack) and goes
bool FinishCurrent(GameReadersStorage* storage)
{
    if (!storage->current->IsDone())
    {
        return false;
    }

    storage->bits |= 8;
    storage->current->Finish(&storage->stack);
    if (storage->current != nullptr)
    {
        storage->current->Destroy(3);
    }

    storage->current = nullptr;
    storage->bits &= ~8u;
    return true;
}

bool AnythingLeft(GameReadersStorage* storage)
{
    return storage->current != nullptr || storage->bufferCount != 0 || storage->stack.count != 0;
}

// A movie lent the sound away: everything's read with it back
void ReclaimMovieSound()
{
    if (G_GameMovieController != nullptr && (G_GameMovieController->flags & GameMovieController::SoundLent) != 0)
    {
        G_GameMovieController->ReclaimSound();
    }
}
}

extern "C"
{
    GameReadersStorage* InitReadersStorage(u32 index)
    {
        if (g_ReadersStorages[index] != nullptr)
        {
            return g_ReadersStorages[index];
        }

        auto* storage = static_cast<GameReadersStorage*>(MemoryAllocate(sizeof(GameReadersStorage)));
        storage->bufferCapacity = BufferCapacity;
        storage->bufferCount = 0;
        storage->bufferStart = 0;
        storage->buffer = static_cast<GenericItemReader**>(MemoryAllocate2(0x1770));
        storage->current = nullptr;
        storage->stack.items = nullptr;
        storage->stack.capacity = StackCapacity;
        storage->stack.count = 0;
        storage->stack.items = static_cast<GenericItemReader**>(MemoryAllocate2(0x2000));
        storage->bits = 0;
        u32 bufferSize = index == 1 ? 0x10000 : 0x20000;
        storage->bits = (storage->bits & ~7u) | (index & 7);
        storage->stream = OpenFileStream(g_StreamSystem, bufferSize, 0, 0);
        FileStreamCreateReader(storage->stream, bufferSize);
        g_ReadersStorages[index] = storage;
        g_ReadersMode = 0;
        return g_ReadersStorages[index];
    }

    void AddItemReaderToReaderStorage(GameReadersStorage* storage, GenericItemReader* reader, s32 front)
    {
        if (reader == nullptr)
        {
            return;
        }

        if (front != 0)
        {
            PushFront(storage, reader);
            return;
        }

        if ((storage->bits & 8) != 0)
        {
            storage->stack.items[storage->stack.count++] = reader;
            return;
        }

        u32 index = storage->bufferCount + storage->bufferStart;
        if (index >= storage->bufferCapacity)
        {
            index -= storage->bufferCapacity;
        }

        storage->buffer[index] = reader;
        storage->bufferCount++;
    }

    bool StorageStartNext(GameReadersStorage* storage)
    {
        if (g_ReadersMode != 0)
        {
            return false;
        }

        // What the last reader found is read first
        while (storage->stack.count != 0)
        {
            storage->stack.count--;
            PushFront(storage, storage->stack.items[storage->stack.count]);
        }

        if (storage->bufferCount == 0)
        {
            storage->current = nullptr;
            return false;
        }

        GenericItemReader* reader = storage->buffer[storage->bufferStart];
        storage->bufferStart++;
        if (storage->bufferStart == storage->bufferCapacity)
        {
            storage->bufferStart = 0;
        }

        storage->bufferCount--;
        storage->current = reader;
        reader->Begin(storage->stream);
        return true;
    }

    bool StorageStep(GameReadersStorage* storage, bool* started)
    {
        switch (g_ReadersMode)
        {
        case 0:
            // Test time (debug.h): what's being read is waited for, so it's there at the same frame however long frames took
            while (g_DebugFixedTime != 0 && storage->current != nullptr && !FinishCurrent(storage))
            {
                FileStreamWait(storage->stream);
            }

            if (storage->current != nullptr && !FinishCurrent(storage))
            {
                *started = false;
                break;
            }

            *started = StorageStartNext(storage);
            break;
        case 1:
            // The one reading is waited for
            while (storage->current != nullptr && !FinishCurrent(storage))
            {
                FileStreamWait(storage->stream);
            }

            break;
        case 2:
            if (storage->current != nullptr)
            {
                FileStreamEmptyReader(storage->stream);
                storage->current->Restart();
                storage->current->Begin(storage->stream);
            }

            break;
        default:
            break;
        }

        return AnythingLeft(storage);
    }

    void LoadQueuedSectionsIntoMemory_()
    {
        s32 mode = g_ReadersMode;
        bool started;
        if (mode == 1)
        {
            for (u32 index = 0; index < 2; index++)
            {
                if (g_ReadersStorages[index] != nullptr)
                {
                    StorageStep(g_ReadersStorages[index], &started);
                }
            }

            g_ReadersSteps++;
            g_ReadersMode = mode;
            return;
        }

        if (mode < 0 || mode > 2)
        {
            g_ReadersMode = mode;
            return;
        }

        if (mode == 2)
        {
            mode = 0;
            ReclaimMovieSound();
        }

        bool busy;
        do
        {
            // Storage 1's readers first, storage 0's once it has none
            busy = g_ReadersStorages[1] != nullptr && StorageStep(g_ReadersStorages[1], &started);
            if (!busy)
            {
                busy = g_ReadersStorages[0] != nullptr && StorageStep(g_ReadersStorages[0], &started);
            }

            if (g_ReadersCompact != 0)
            {
                DiskCompactAll(GetDiskManager());
            }

            for (u32 index = 0; index < 2; index++)
            {
                FileStream* stream = g_ReadersStorages[index] != nullptr ? g_ReadersStorages[index]->stream : nullptr;
                if (stream != nullptr)
                {
                    FileStreamWait(stream);
                }
            }
        } while (busy);

        g_ReadersSteps++;
        g_ReadersMode = mode;
    }

    u32 ReadersStep(bool* started)
    {
        s32 mode = g_ReadersMode;
        *started = false;
        u32 busy = 0;
        if (g_ReadersMode < 0)
        {
            g_ReadersMode = mode;
            return 0;
        }

        if (g_ReadersMode >= 2)
        {
            if (g_ReadersMode != 2)
            {
                g_ReadersMode = mode;
                return 0;
            }

            mode = 0;
            ReclaimMovieSound();
        }

        for (u32 index = 0; index < 2; index++)
        {
            if (g_ReadersStorages[index] == nullptr)
            {
                continue;
            }

            bool storageStarted = false;
            busy |= StorageStep(g_ReadersStorages[index], &storageStarted) ? 1 : 0;
            *started = *started || storageStarted;
        }

        g_ReadersSteps++;
        g_ReadersMode = mode;
        return busy;
    }
}

namespace
{
// The base's destructor, which every reader's ends with
void DestroyReader(GenericItemReader* reader, u32 flags)
{
    reader->vtable = g_GenericItemReaderVTable;
    if (reader->sectionReader != nullptr)
    {
        reader->sectionReader->Destroy(3);
    }

    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}

// The stream has a file of the disk open
bool HasOwnFile(const FileStream* stream)
{
    return stream->archiveState == FileStream::NoArchive && stream->file != Platform::Stream::NoFile;
}

// Whether the stream's open file is the path (any file for no path)
bool IsOpenFile(const String* path, const FileStream* stream)
{
    return path->length == 0 || !StringNotEqual(path, stream->path.string);
}

// The reader's done once the stream is
bool PollRead(u32* bits, FileStream* stream)
{
    if ((*bits & 3) != 1)
    {
        return true;
    }

    bool reading = FileStreamPoll(stream);
    *bits = (*bits & ~2u) | (reading ? 0 : 1) << 1;
    return (*bits >> 1 & 1) != 0;
}
}

GenericItemReader* GenericItemReader::Construct(GenericItemReader* reader, SectionReader* sectionReader)
{
    reader->sectionReader = sectionReader;
    reader->vtable = g_GenericItemReaderVTable;
    return reader;
}

void GenericItemReader::DestroyGeneric(u32 flags)
{
    DestroyReader(this, flags);
}

void GenericItemReader::Nothing()
{
}

FileCloseReader* FileCloseReader::Construct(FileCloseReader* reader, SectionReader* sectionReader)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_FileCloseReaderVTable;
    return reader;
}

void FileCloseReader::Destroy(u32 flags)
{
    DestroyReader(this, flags);
}

void FileCloseReader::Begin(FileStream* stream)
{
    FileStreamClose(stream);
}

bool FileCloseReader::IsDone()
{
    return true;
}

void FileCloseReader::Finish(ReaderStack* stack)
{
    if (sectionReader != nullptr)
    {
        sectionReader->Read(nullptr, 0xFFFFFFFF, stack);
    }
}

void FileCloseReader::Restart()
{
}

MemoryReader* MemoryReader::Construct(MemoryReader* reader, SectionReader* sectionReader, u8* data, u32 size)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->data = data;
    reader->size = size;
    reader->vtable = g_MemoryReaderVTable;
    return reader;
}

void MemoryReader::Destroy(u32 flags)
{
    DestroyReader(this, flags);
}

void MemoryReader::Begin(FileStream*)
{
}

bool MemoryReader::IsDone()
{
    return true;
}

void MemoryReader::Finish(ReaderStack* stack)
{
    sectionReader->Read(data, size, stack);
}

void MemoryReader::Restart()
{
}

void WaitingPartReader::Destroy(u32 flags)
{
    DestroyReader(this, flags);
}

void WaitingPartReader::Begin(FileStream* stream)
{
    u32 read;
    FileStreamRead(stream, offset, size, data, 1, &read);
}

bool WaitingPartReader::IsDone()
{
    return true;
}

void WaitingPartReader::Finish(ReaderStack* stack)
{
    if (sectionReader != nullptr)
    {
        sectionReader->Read(data, size, stack);
    }
}

void PolledPartReader::Destroy(u32 flags)
{
    DestroyReader(this, flags);
}

void PolledPartReader::Begin(FileStream* fileStream)
{
    Archive* archive = g_CurrentArchive;
    if (inArchive != 0 && HasOwnFile(fileStream))
    {
        ArchiveFile* file = archive->lastFound;
        if (file == nullptr)
        {
            return;
        }

        offset += file->entry.start;
    }

    u32 got;
    if (FileStreamRead(fileStream, offset, size, data, 0, &got))
    {
        read = 1;
        return;
    }

    stream = fileStream;
}

bool PolledPartReader::IsDone()
{
    return read != 0 || !FileStreamPoll(stream);
}

void PolledPartReader::Finish(ReaderStack* stack)
{
    if (sectionReader != nullptr)
    {
        sectionReader->Read(data, size, stack);
    }
}

namespace
{
// The constructors' flags: bit 0 release, 1 ..., 2 and 1 together ..., 3 on the disk manager, 5 clamp to the file
u32 SubItemsBits(u32 flags)
{
    u32 onDisk = flags >> 3 & 1;
    return onDisk << 3 | (flags & 1) << 4 | (flags << 4 & 0x20) | static_cast<u32>((flags & 6) == 4) << 6 | onDisk << 7;
}

void SetUpSubItems(SubItemsReader* reader, u32 bits, u32 offset, u32 size)
{
    reader->size = size;
    reader->offset = offset;
    reader->diskIndex = -1;
    reader->diskHandle = (bits & SubItemsReader::OnDisk) != 0 ? &reader->diskIndex : nullptr;
    reader->data = nullptr;
    reader->bits = bits;
}
}

SubItemsReader* SubItemsReader::ConstructFile(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    StringConstruct(&reader->path, path);
    SetUpSubItems(reader, SubItemsBits(flags) | WholeFile, 0, 0);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructPart(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags,
                                              u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    StringConstruct(&reader->path, path);
    SetUpSubItems(reader, SubItemsBits(flags) | (flags << 3 & ClampToFile), offset, size);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructOpen(SubItemsReader* reader, SectionReader* sectionReader, u32 flags, u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    reader->path.string = nullptr;
    reader->path.capacity = 0;
    reader->path.length = 0;
    SetUpSubItems(reader, SubItemsBits(flags) | (flags << 3 & ClampToFile), offset, size);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructOnDisk(SubItemsReader* reader, s32* diskHandle, u32 flags, u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, nullptr);
    reader->vtable = g_SubItemsReaderVTable;
    reader->path.string = nullptr;
    reader->path.capacity = 0;
    reader->path.length = 0;
    // On the disk manager always, with the handle given
    u32 bits = (flags & 1) << 4 | OnDisk | (flags << 4 & 0x20) | static_cast<u32>((flags & 6) == 4) << 6 | (flags << 4 & Release) |
               (flags << 3 & ClampToFile);
    reader->size = size;
    reader->offset = offset;
    reader->diskHandle = diskHandle;
    reader->diskIndex = -1;
    reader->bits = bits;
    reader->data = nullptr;
    return reader;
}

void SubItemsReader::Destroy(u32 flags)
{
    vtable = g_SubItemsReaderVTable;
    StringDestroy(&path);
    DestroyReader(this, flags);
}

void SubItemsReader::Begin(FileStream* fileStream)
{
    stream = fileStream;
    ArchiveEntry* entry = nullptr;
    if ((bits & FromArchive) != 0)
    {
        if (HasOwnFile(fileStream))
        {
            entry = ArchiveFind(g_CurrentArchive, path.string);
            bits = (bits & ~Found) | (entry != nullptr ? Found : 0);
        }
        else
        {
            bits &= ~Found;
        }
    }
    else if ((bits & WholeFile) != 0)
    {
        bool found = FileStreamOpen(fileStream, path.string);
        offset = 0;
        bits = (bits & ~Found) | (found ? Found : 0);
        size = fileStream->size;
    }
    else if (HasOwnFile(fileStream))
    {
        bits = (bits & ~Found) | (IsOpenFile(&path, fileStream) ? Found : 0);
    }
    else
    {
        bits = (bits & ~Found) | (FileStreamOpen(fileStream, path.string) ? Found : 0);
    }

    if ((bits & ClampToFile) != 0)
    {
        u32 left = fileStream->size - offset;
        if (left < size)
        {
            size = left;
        }
    }

    if ((bits & Found) == 0)
    {
        return;
    }

    if ((bits & NothingToRead) != 0)
    {
        bits |= Read;
        return;
    }

    if ((bits & FromArchive) != 0)
    {
        offset = entry->start;
        size = entry->size;
    }

    if ((bits & OnDisk) != 0)
    {
        s32 handle[4];
        DiskAllocate(handle, GetDiskManager(), size, true, 0);
        *diskHandle = handle[0];
        data = DiskMemory(GetDiskManager(), diskHandle);
    }
    else
    {
        data = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), size, 0x40));
    }

    u32 read;
    bool done = FileStreamRead(fileStream, offset, size, data, 0, &read);
    bits = (bits & ~Read) | (done ? Read : 0);
}

bool SubItemsReader::IsDone()
{
    return PollRead(&bits, stream);
}

void SubItemsReader::Finish(ReaderStack* stack)
{
    if ((bits & NothingToRead) != 0)
    {
        FileStreamClose(stream);
        return;
    }

    if ((bits & CloseFile) != 0)
    {
        FileStreamClose(stream);
    }

    if ((bits & Found) == 0)
    {
        if (sectionReader != nullptr)
        {
            sectionReader->Missing(data, size, stack);
        }

        return;
    }

    if (sectionReader != nullptr)
    {
        sectionReader->Read(data, size, stack);
    }

    if ((bits & Release) != 0)
    {
        if ((bits & OnDisk) != 0)
        {
            DiskRelease(GetDiskManager(), diskHandle);
        }
        else
        {
            FreeMemory(GetHeapManager(), data);
        }
    }
    else if ((bits & OnDisk) != 0)
    {
        DiskMarkLoaded(GetDiskManager(), diskHandle);
    }
}

void SubItemsReader::Restart()
{
    if ((bits & OnDisk) == 0)
    {
        FreeMemory(GetHeapManager(), data);
        data = nullptr;
        return;
    }

    DiskRelease(GetDiskManager(), diskHandle);
    diskIndex = -2;
    diskHandle = &diskIndex;
}

SoundBankReader* SoundBankReader::Construct(SoundBankReader* reader, SoundBankEntry* bank, u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, nullptr);
    reader->bank = bank;
    reader->path.string = nullptr;
    reader->vtable = g_SoundBankReaderVTable;
    reader->path.capacity = 0;
    reader->path.length = 0;
    reader->offset = offset;
    reader->size = size;
    reader->stream = nullptr;
    reader->bits = 0;
    return reader;
}

void SoundBankReader::Destroy(u32 flags)
{
    StringDestroy(&path);
    DestroyReader(this, flags);
}

s32 SoundBankReader::Begin(FileStream* fileStream)
{
    stream = fileStream;
    if ((bits & 4) != 0)
    {
        bool found = FileStreamOpen(fileStream, path.string);
        offset = 0;
        bits = (bits & ~1u) | (found ? 1 : 0);
        size = fileStream->size;
    }
    else if (HasOwnFile(fileStream))
    {
        bits = (bits & ~1u) | (IsOpenFile(&path, fileStream) ? 1 : 0);
    }
    else
    {
        bits = (bits & ~1u) | (FileStreamOpen(fileStream, path.string) ? 1 : 0);
    }

    if ((bits & 1) == 0)
    {
        return 0;
    }

    return FileStreamReadSoundBank(fileStream, bank->id, offset, size, 0);
}

bool SoundBankReader::IsDone()
{
    return PollRead(&bits, stream);
}

void SoundBankReader::Finish(ReaderStack*)
{
    Platform::Audio::SoundBankLoaded(static_cast<u16>(bank->id));
    bank->flags |= 1;
    if ((bits & 8) != 0)
    {
        FileStreamClose(stream);
    }
}

void SoundBankReader::Restart()
{
}

BdReader* BdReader::Construct(BdReader* reader, String* path, SectionReader* sectionReader, u8 inArchive, u8 unknown16)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->path.string = nullptr;
    reader->vtable = g_BdReaderVTable;
    reader->path.length = 0;
    reader->path.capacity = 0;
    StringAssign(&reader->path, path->string);
    reader->inArchive = inArchive;
    reader->unknown16 = unknown16;
    reader->found = 0;
    return reader;
}

void BdReader::Destroy(u32 flags)
{
    vtable = g_BdReaderVTable;
    StringDestroy(&path);
    DestroyReader(this, flags);
}

void BdReader::Begin(FileStream* fileStream)
{
    if (inArchive == 0)
    {
        found = FileStreamOpen(fileStream, path.string) ? 1 : 0;
        return;
    }

    if (!HasOwnFile(fileStream))
    {
        return;
    }

    found = ArchiveFind(g_CurrentArchive, path.string) != nullptr ? 1 : 0;
}

bool BdReader::IsDone()
{
    return true;
}

void BdReader::Finish(ReaderStack* stack)
{
    if (sectionReader == nullptr)
    {
        return;
    }

    if (found != 0)
    {
        sectionReader->Read(nullptr, 0, stack);
    }
    else
    {
        sectionReader->Missing(nullptr, 0, stack);
    }
}

void BdReader::Restart()
{
}

extern "C"
{
    ItemHeader* SetNextItem(ItemHeader* header, const ItemHeader* next, u32 start)
    {
        header->offset = next->offset + start;
        header->size = next->size;
        header->id = next->id;
        return header;
    }

    u32 GetItemsAmountInItem(const u8* section)
    {
        return reinterpret_cast<const u32*>(section)[1];
    }

    u32 GetSectionSize(const u8* section)
    {
        return reinterpret_cast<const u32*>(section)[2];
    }

    void ReadSectionHeader(const u8* data, ItemInterface* item, u32 start)
    {
        u32 header = *reinterpret_cast<const u32*>(data);
        if ((header >> 16) >= 2 || !item->CanRead(header & 0xFFFF))
        {
            return;
        }

        GameReadersStorage* storage = g_ReadersStorages[0];
        u32 tableSize = reinterpret_cast<const u32*>(data)[1] * sizeof(ItemHeader);
        auto* reader = static_cast<ItemSectionReader*>(MemoryAllocate(sizeof(ItemSectionReader)));
        reader->item = item;
        reader->vtable = g_ItemSectionReaderVTable;
        reader->count = GetItemsAmountInItem(data);
        reader->size = GetSectionSize(data);
        reader->start = start;
        reader->path.string = nullptr;
        reader->path.capacity = 0;
        reader->path.length = 0;
        auto* sectionReader = reinterpret_cast<SectionReader*>(reader);
        GenericItemReader* table;
        if (tableSize != 0)
        {
            // The table follows the header
            table = SubItemsReader::ConstructOpen(static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))),
                                                  sectionReader, 8, start + 0xC, tableSize);
        }
        else
        {
            table = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), sectionReader,
                                            nullptr, 0);
        }

        AddItemReaderToReaderStorage(storage, table, 0);
    }

    void AddResourcePackageToLoadQueue(ItemInterface* item, const char* path, s32 keepOpen)
    {
        u32 flags = keepOpen != 0 ? 0x2A : 0x28;
        GameReadersStorage* storage = g_ReadersStorages[0];
        auto* package = static_cast<PackageSectionReader*>(MemoryAllocate(sizeof(PackageSectionReader)));
        package->vtable = g_PackageSectionReaderVTable;
        package->item = item;
        package->start = 0;
        package->keepOpen = static_cast<u8>(keepOpen);
        GenericItemReader* header = SubItemsReader::ConstructPart(
            static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))), path,
            reinterpret_cast<SectionReader*>(package), flags, 0, 0xC);
        AddItemReaderToReaderStorage(storage, header, 0);
        if (keepOpen == 0)
        {
            AddItemReaderToReaderStorage(
                storage, FileCloseReader::Construct(static_cast<FileCloseReader*>(MemoryAllocate(sizeof(FileCloseReader))), nullptr),
                0);
        }
    }

    void AddSectionToLoadQueue(ItemInterface* item, u32 start)
    {
        GameReadersStorage* storage = g_ReadersStorages[0];
        auto* package = static_cast<PackageSectionReader*>(MemoryAllocate(sizeof(PackageSectionReader)));
        package->item = item;
        package->vtable = g_PackageSectionReaderVTable;
        package->start = start;
        package->keepOpen = 0;
        GenericItemReader* header = SubItemsReader::ConstructOpen(
            static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))), reinterpret_cast<SectionReader*>(package), 8,
            start, 0xC);
        AddItemReaderToReaderStorage(storage, header, 0);
    }
}

void PackageSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PackageSectionReader::Read(u8* data, u32, ReaderStack*)
{
    ReadSectionHeader(data, item, start);
}

void ItemSectionReader::Destroy(u32 flags)
{
    StringDestroy(&path);
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ItemSectionReader::Read(u8* data, u32, ReaderStack*)
{
    GameReadersStorage* storage = g_ReadersStorages[0];
    u32 end = start;
    if (data != nullptr)
    {
        end += *reinterpret_cast<u32*>(data);
    }

    item->SetCount(count);
    s32 read = 0;
    for (u32 index = 0; index < count; index++)
    {
        ItemHeader header;
        SetNextItem(&header, reinterpret_cast<ItemHeader*>(data) + index, start);
        s32 itemSize = header.size;
        end = header.offset;
        SectionReader* reader = item->GetReader(read, &header, &itemSize);
        if (reader != nullptr)
        {
            auto* itemReader = static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader)));
            GenericItemReader* added;
            if (path.length == 0)
            {
                added = SubItemsReader::ConstructOpen(itemReader, reader, 8, end, static_cast<u32>(itemSize));
            }
            else
            {
                added = SubItemsReader::ConstructPart(itemReader, path.string, reader, 8, end, static_cast<u32>(itemSize));
            }

            read++;
            AddItemReaderToReaderStorage(storage, added, 0);
        }

        end += header.size;
    }

    // The item's told once every item was read
    auto* done = static_cast<ItemsReadSectionReader*>(MemoryAllocate(sizeof(ItemsReadSectionReader)));
    done->item = item;
    done->vtable = g_ItemsReadSectionReaderVTable;
    done->count = count;
    done->read = read;
    done->end = end;
    AddItemReaderToReaderStorage(
        storage,
        MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), reinterpret_cast<SectionReader*>(done),
                                nullptr, 0),
        0);
}

void ItemsReadSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ItemsReadSectionReader::Read(u8*, u32, ReaderStack*)
{
    item->Finish(read, count, end);
}

// The whole file, read now on storage 1's stream, into the stream's memory: memory of its own is made again for the file (and a
// terminator after it when asked for), other memory is read into as it is
bool MemoryStream::LoadFile(const char* path, bool terminate)
{
    FileStream* stream = g_ReadersStorages[1]->stream;
    if (!FileStreamOpen(stream, path))
    {
        return false;
    }

    u32 fileSize = stream->size;
    u32 needed = terminate ? fileSize + 1 : fileSize;
    if ((flags & FlagOwnsMemory) != 0)
    {
        FreeMemory(GetHeapManager(), begin);
        begin = nullptr;
        position = nullptr;
        size = 0;
        auto* memory = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), needed, alignment));
        begin = memory;
        position = memory;
        flags |= FlagOwnsMemory;
        size = memory != nullptr ? needed : 0;
    }

    u32 read;
    FileStreamRead(stream, 0, fileSize, begin, 1, &read);
    FileStreamClose(stream);
    if (terminate)
    {
        begin[fileSize] = 0;
    }

    static_cast<Stream*>(this)->Rewind();
    return true;
}

void ItemInterface::BaseDestroy(u32 flags)
{
    vtable = g_ItemInterfaceVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ItemInterface::BaseSetCount(u32)
{
}

void ItemInterface::BaseFinish(s32, u32, u32)
{
}
