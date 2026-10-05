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
// How many readers a storage's queue and stack have room for, and the blocks its stream reads in
constexpr u32 BufferCapacity = 0x5DC;
constexpr s16 StackCapacity = 0x800;
constexpr u32 FileReadersBlock = 0x10000;
constexpr u32 MainReadersBlock = 0x20000;

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

    storage->bits.finishing = 1;
    storage->current->Finish(&storage->stack);
    if (storage->current != nullptr)
    {
        storage->current->Destroy(DestroyAndFree);
    }

    storage->current = nullptr;
    storage->bits.finishing = 0;
    return true;
}

bool AnythingLeft(GameReadersStorage* storage)
{
    return storage->current != nullptr || storage->bufferCount != 0 || storage->stack.count != 0;
}

// A movie lent the sound away: everything's read with it back
void ReclaimMovieSound()
{
    if (G_GameMovieController != nullptr && G_GameMovieController->flags.soundLent != 0)
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
        storage->buffer = static_cast<GenericItemReader**>(MemoryAllocate2(BufferCapacity * sizeof(GenericItemReader*)));
        storage->current = nullptr;
        storage->stack.items = nullptr;
        storage->stack.capacity = StackCapacity;
        storage->stack.count = 0;
        storage->stack.items = static_cast<GenericItemReader**>(MemoryAllocate2(StackCapacity * sizeof(GenericItemReader*)));
        storage->bits.value = 0;
        u32 blockSize = index == FileReaders ? FileReadersBlock : MainReadersBlock;
        storage->bits.unused0 = index;
        storage->stream = OpenFileStream(g_StreamSystem, blockSize, 0, 0);
        FileStreamCreateReader(storage->stream, blockSize);
        g_ReadersStorages[index] = storage;
        g_ReadersMode = ReadersReading;
        return g_ReadersStorages[index];
    }

    void AddItemReaderToReaderStorage(GameReadersStorage* storage, GenericItemReader* reader, s32 front)
    {
        if (reader == nullptr)
        {
            return;
        }

        if (front != QueueBack)
        {
            PushFront(storage, reader);
            return;
        }

        if (storage->bits.finishing != 0)
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
        if (g_ReadersMode != ReadersReading)
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
        case ReadersReading:
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
        case ReadersHeldByMovie:
            // The one reading is waited for
            while (storage->current != nullptr && !FinishCurrent(storage))
            {
                FileStreamWait(storage->stream);
            }

            break;
        case ReadersAfterMovie:
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
        if (mode == ReadersHeldByMovie)
        {
            for (u32 index = 0; index < ReadersStorageCount; index++)
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

        if (mode < ReadersReading || mode > ReadersAfterMovie)
        {
            g_ReadersMode = mode;
            return;
        }

        // Cleared once everything's read: the steps see the mode as it was
        if (mode == ReadersAfterMovie)
        {
            mode = ReadersReading;
            ReclaimMovieSound();
        }

        bool busy;
        do
        {
            // The file readers first, the main ones once they have none
            busy = g_ReadersStorages[FileReaders] != nullptr && StorageStep(g_ReadersStorages[FileReaders], &started);
            if (!busy)
            {
                busy = g_ReadersStorages[MainReaders] != nullptr && StorageStep(g_ReadersStorages[MainReaders], &started);
            }

            if (g_ReadersCompact != 0)
            {
                DiskCompactAll(GetDiskManager());
            }

            for (u32 index = 0; index < ReadersStorageCount; index++)
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
        if (g_ReadersMode < ReadersReading)
        {
            g_ReadersMode = mode;
            return 0;
        }

        // The readers that were reading start over this step, the readers read on from the next
        if (g_ReadersMode >= ReadersAfterMovie)
        {
            if (g_ReadersMode != ReadersAfterMovie)
            {
                g_ReadersMode = mode;
                return 0;
            }

            mode = ReadersReading;
            ReclaimMovieSound();
        }

        for (u32 index = 0; index < ReadersStorageCount; index++)
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
// What FileStreamRead is told: to wait for the bytes, or to have the stream read them in the background
constexpr s32 WaitForRead = 1;
constexpr s32 ReadInBackground = 0;
// The size a file close reader hands its section reader
constexpr u32 FileClosedSize = 0xFFFFFFFF;
// A sub items reader's disk handle of its memory let go of by a restart
constexpr s32 ReleasedDiskHandle = -2;

// The base's destructor, which every reader's ends with
void DestroyReader(GenericItemReader* reader, u32 flags)
{
    reader->vtable = g_GenericItemReaderVTable;
    if (reader->sectionReader != nullptr)
    {
        reader->sectionReader->Destroy(DestroyAndFree);
    }

    if ((flags & FreeAfterDestroy) != 0)
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

// The reader's done once the stream is (or when it found nothing to read)
template <typename Bits>
bool PollRead(Bits* bits, FileStream* stream)
{
    if (bits->found == 0 || bits->read != 0)
    {
        return true;
    }

    bits->read = FileStreamPoll(stream) ? 0 : 1;
    return bits->read != 0;
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
        sectionReader->Read(nullptr, FileClosedSize, stack);
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
    FileStreamRead(stream, offset, size, data, WaitForRead, &read);
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
    if (FileStreamRead(fileStream, offset, size, data, ReadInBackground, &got))
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
// The bits the options make in every constructor: on the disk manager its memory is released once read, a file of the archive
// isn't closed
SubItemsReaderBits BitsOf(u32 flags)
{
    SubItemsReaderOptions options;
    options.value = flags;
    SubItemsReaderBits bits;
    bits.value = 0;
    bits.onDisk = options.onDisk;
    bits.unused4 = options.unused0;
    bits.fromArchive = options.fromArchive;
    bits.closesFile = options.closesFile != 0 && options.fromArchive == 0;
    bits.releases = options.onDisk;
    return bits;
}

void SetUpSubItems(SubItemsReader* reader, SubItemsReaderBits bits, u32 offset, u32 size)
{
    reader->size = size;
    reader->offset = offset;
    reader->diskIndex = NoDiskHandle;
    reader->diskHandle = bits.onDisk != 0 ? &reader->diskIndex : nullptr;
    reader->data = nullptr;
    reader->bits = bits;
}

// A part's bits: its options' and the size cut to the file when they say so
SubItemsReaderBits PartBitsOf(u32 flags)
{
    SubItemsReaderOptions options;
    options.value = flags;
    SubItemsReaderBits bits = BitsOf(flags);
    bits.clampsToFile = options.clampsToFile;
    return bits;
}
}

SubItemsReader* SubItemsReader::ConstructFile(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    StringConstruct(&reader->path, path);
    SubItemsReaderBits bits = BitsOf(flags);
    bits.wholeFile = 1;
    SetUpSubItems(reader, bits, 0, 0);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructPart(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags,
                                              u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    StringConstruct(&reader->path, path);
    SetUpSubItems(reader, PartBitsOf(flags), offset, size);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructOpen(SubItemsReader* reader, SectionReader* sectionReader, u32 flags, u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->vtable = g_SubItemsReaderVTable;
    reader->path.string = nullptr;
    reader->path.capacity = 0;
    reader->path.length = 0;
    SetUpSubItems(reader, PartBitsOf(flags), offset, size);
    return reader;
}

SubItemsReader* SubItemsReader::ConstructOnDisk(SubItemsReader* reader, s32* diskHandle, u32 flags, u32 offset, u32 size)
{
    GenericItemReader::Construct(reader, nullptr);
    reader->vtable = g_SubItemsReaderVTable;
    reader->path.string = nullptr;
    reader->path.capacity = 0;
    reader->path.length = 0;
    // On the disk manager always, with the handle given: the option only says whether it's released
    SubItemsReaderBits bits = PartBitsOf(flags);
    bits.onDisk = 1;
    reader->size = size;
    reader->offset = offset;
    reader->diskHandle = diskHandle;
    reader->diskIndex = NoDiskHandle;
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
    if (bits.fromArchive != 0)
    {
        if (HasOwnFile(fileStream))
        {
            entry = ArchiveFind(g_CurrentArchive, path.string);
            bits.found = entry != nullptr ? 1 : 0;
        }
        else
        {
            bits.found = 0;
        }
    }
    else if (bits.wholeFile != 0)
    {
        bool found = FileStreamOpen(fileStream, path.string);
        offset = 0;
        bits.found = found ? 1 : 0;
        size = fileStream->size;
    }
    else if (HasOwnFile(fileStream))
    {
        bits.found = IsOpenFile(&path, fileStream) ? 1 : 0;
    }
    else
    {
        bits.found = FileStreamOpen(fileStream, path.string) ? 1 : 0;
    }

    if (bits.clampsToFile != 0)
    {
        u32 left = fileStream->size - offset;
        if (left < size)
        {
            size = left;
        }
    }

    if (bits.found == 0)
    {
        return;
    }

    if (bits.nothingToRead != 0)
    {
        bits.read = 1;
        return;
    }

    if (bits.fromArchive != 0)
    {
        offset = entry->start;
        size = entry->size;
    }

    if (bits.onDisk != 0)
    {
        s32 handle[4];
        DiskAllocate(handle, GetDiskManager(), size, true, 0);
        *diskHandle = handle[0];
        data = DiskMemory(GetDiskManager(), diskHandle);
    }
    else
    {
        data = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), size, MemoryStream::FileAlignment));
    }

    u32 read;
    bool done = FileStreamRead(fileStream, offset, size, data, ReadInBackground, &read);
    bits.read = done ? 1 : 0;
}

bool SubItemsReader::IsDone()
{
    return PollRead(&bits, stream);
}

void SubItemsReader::Finish(ReaderStack* stack)
{
    if (bits.nothingToRead != 0)
    {
        FileStreamClose(stream);
        return;
    }

    if (bits.closesFile != 0)
    {
        FileStreamClose(stream);
    }

    if (bits.found == 0)
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

    if (bits.releases != 0)
    {
        if (bits.onDisk != 0)
        {
            DiskRelease(GetDiskManager(), diskHandle);
        }
        else
        {
            FreeMemory(GetHeapManager(), data);
        }
    }
    else if (bits.onDisk != 0)
    {
        DiskMarkLoaded(GetDiskManager(), diskHandle);
    }
}

void SubItemsReader::Restart()
{
    if (bits.onDisk == 0)
    {
        FreeMemory(GetHeapManager(), data);
        data = nullptr;
        return;
    }

    DiskRelease(GetDiskManager(), diskHandle);
    diskIndex = ReleasedDiskHandle;
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
    reader->bits.value = 0;
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
    if (bits.wholeFile != 0)
    {
        bool found = FileStreamOpen(fileStream, path.string);
        offset = 0;
        bits.found = found ? 1 : 0;
        size = fileStream->size;
    }
    else if (HasOwnFile(fileStream))
    {
        bits.found = IsOpenFile(&path, fileStream) ? 1 : 0;
    }
    else
    {
        bits.found = FileStreamOpen(fileStream, path.string) ? 1 : 0;
    }

    if (bits.found == 0)
    {
        return 0;
    }

    return FileStreamReadSoundBank(fileStream, bank->header.id, offset, size, ReadInBackground);
}

bool SoundBankReader::IsDone()
{
    return PollRead(&bits, stream);
}

void SoundBankReader::Finish(ReaderStack*)
{
    Platform::Audio::SoundBankLoaded(static_cast<u16>(bank->header.id));
    bank->flags.samplesLoaded = 1;
    if (bits.closesFile != 0)
    {
        FileStreamClose(stream);
    }
}

void SoundBankReader::Restart()
{
}

BdReader* BdReader::Construct(BdReader* reader, String* path, SectionReader* sectionReader, u8 inArchive, u8 unused)
{
    GenericItemReader::Construct(reader, sectionReader);
    reader->path.string = nullptr;
    reader->vtable = g_BdReaderVTable;
    reader->path.length = 0;
    reader->path.capacity = 0;
    StringAssign(&reader->path, path->string);
    reader->inArchive = inArchive;
    reader->unused16 = unused;
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
    ItemHeader* ItemHeaderInFile(ItemHeader* header, const ItemHeader* entry, u32 start)
    {
        header->offset = entry->offset + start;
        header->size = entry->size;
        header->id = entry->id;
        return header;
    }

    u32 SectionItemCount(const u8* section)
    {
        return reinterpret_cast<const SectionHeader*>(section)->itemCount;
    }

    u32 GetSectionSize(const u8* section)
    {
        return reinterpret_cast<const SectionHeader*>(section)->size;
    }

    void ReadSectionHeader(const u8* data, ItemInterface* item, u32 start)
    {
        const auto* header = reinterpret_cast<const SectionHeader*>(data);
        SectionFormat format = header->format;
        if (format.version >= SectionVersions || !item->CanRead(format.type))
        {
            return;
        }

        GameReadersStorage* storage = g_ReadersStorages[MainReaders];
        u32 tableSize = header->itemCount * sizeof(ItemHeader);
        auto* reader = static_cast<ItemSectionReader*>(MemoryAllocate(sizeof(ItemSectionReader)));
        reader->item = item;
        reader->vtable = g_ItemSectionReaderVTable;
        reader->count = SectionItemCount(data);
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
                                                  sectionReader, SubItemsReaderOptions::OnDisk, start + sizeof(SectionHeader),
                                                  tableSize);
        }
        else
        {
            table = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), sectionReader,
                                            nullptr, 0);
        }

        AddItemReaderToReaderStorage(storage, table, QueueBack);
    }

    void AddResourcePackageToLoadQueue(ItemInterface* item, const char* path, s32 inArchive)
    {
        constexpr u32 PackageOptions = SubItemsReaderOptions::OnDisk | SubItemsReaderOptions::ClampsToFile;
        u32 flags = inArchive != 0 ? PackageOptions | SubItemsReaderOptions::FromArchive : PackageOptions;
        GameReadersStorage* storage = g_ReadersStorages[MainReaders];
        auto* package = static_cast<PackageSectionReader*>(MemoryAllocate(sizeof(PackageSectionReader)));
        package->vtable = g_PackageSectionReaderVTable;
        package->item = item;
        package->start = 0;
        package->unused0C = static_cast<u8>(inArchive);
        GenericItemReader* header = SubItemsReader::ConstructPart(
            static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))), path,
            reinterpret_cast<SectionReader*>(package), flags, 0, sizeof(SectionHeader));
        AddItemReaderToReaderStorage(storage, header, QueueBack);
        if (inArchive == 0)
        {
            AddItemReaderToReaderStorage(
                storage, FileCloseReader::Construct(static_cast<FileCloseReader*>(MemoryAllocate(sizeof(FileCloseReader))), nullptr),
                QueueBack);
        }
    }

    void AddSectionToLoadQueue(ItemInterface* item, u32 start)
    {
        GameReadersStorage* storage = g_ReadersStorages[MainReaders];
        auto* package = static_cast<PackageSectionReader*>(MemoryAllocate(sizeof(PackageSectionReader)));
        package->item = item;
        package->vtable = g_PackageSectionReaderVTable;
        package->start = start;
        package->unused0C = 0;
        GenericItemReader* header = SubItemsReader::ConstructOpen(
            static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))), reinterpret_cast<SectionReader*>(package),
            SubItemsReaderOptions::OnDisk, start, sizeof(SectionHeader));
        AddItemReaderToReaderStorage(storage, header, QueueBack);
    }
}

void PackageSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
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
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

// The data is the section's table of items
void ItemSectionReader::Read(u8* data, u32, ReaderStack*)
{
    GameReadersStorage* storage = g_ReadersStorages[MainReaders];
    auto* table = reinterpret_cast<ItemHeader*>(data);
    u32 end = start;
    if (data != nullptr)
    {
        end += table->offset;
    }

    item->SetCount(count);
    s32 read = 0;
    for (u32 index = 0; index < count; index++)
    {
        ItemHeader header;
        ItemHeaderInFile(&header, table + index, start);
        s32 itemSize = header.size;
        end = header.offset;
        SectionReader* reader = item->GetReader(read, &header, &itemSize);
        if (reader != nullptr)
        {
            auto* itemReader = static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader)));
            GenericItemReader* added;
            if (path.length == 0)
            {
                added = SubItemsReader::ConstructOpen(itemReader, reader, SubItemsReaderOptions::OnDisk, end,
                                                      static_cast<u32>(itemSize));
            }
            else
            {
                added = SubItemsReader::ConstructPart(itemReader, path.string, reader, SubItemsReaderOptions::OnDisk, end,
                                                      static_cast<u32>(itemSize));
            }

            read++;
            AddItemReaderToReaderStorage(storage, added, QueueBack);
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
        QueueBack);
}

void ItemsReadSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
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
    FileStream* stream = g_ReadersStorages[FileReaders]->stream;
    if (!FileStreamOpen(stream, path))
    {
        return false;
    }

    u32 fileSize = stream->size;
    u32 needed = terminate ? fileSize + 1 : fileSize;
    if (flags.ownsMemory)
    {
        FreeMemory(GetHeapManager(), begin);
        begin = nullptr;
        position = nullptr;
        size = 0;
        auto* memory = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), needed, alignment));
        begin = memory;
        position = memory;
        flags.ownsMemory = 1;
        size = memory != nullptr ? needed : 0;
    }

    u32 read;
    FileStreamRead(stream, 0, fileSize, begin, WaitForRead, &read);
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
    if ((flags & FreeAfterDestroy) != 0)
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
