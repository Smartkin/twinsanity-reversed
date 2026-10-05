#include "game/filestream.h"

#include "debug.h"

#include "game/memory.h"
#include "game/sound.h"
#include "platform/stream.h"
#include "retail/libc.h"

namespace
{
constexpr const char* NoPath = "Nothing";
constexpr const char* NewPath = "Initialising";
// A read too big for the reader takes what the reader has in whole blocks of this, the rest straight from the file
constexpr u32 ReadBlockSize = 0x40;
constexpr u32 SoundBankBufferSize = 0x800;
constexpr u32 SamplesPathSize = 0x100;

// The reader through its vtable, as the retail code calls it
Stream* Reader(FileStream* stream)
{
    return stream->reader;
}

// The reader's bytes the read was waiting for go where they were asked for
void HandOver(FileStream* stream)
{
    if (stream->pending != nullptr)
    {
        Reader(stream)->Read(stream->pending, stream->pendingSize, 1);
        stream->pending = nullptr;
    }

    stream->flags.reading = 0;
}
}

extern "C"
{
    extern const GccVTableEntry g_StreamSystemVTable[] RETAIL(D_00306C78);
}

StreamSystem* StreamSystem::Construct(StreamSystem* system, u16 capacity)
{
    system->capacity = capacity;
    system->vtable = g_StreamSystemVTable;
    system->growth = static_cast<u16>((capacity >> 2) + PoolGrowth);
    system->used = 0;
    system->firstFree = 0;
    system->links = nullptr;
    system->streams = nullptr;
    // Both stay even
    if ((system->capacity & 1) != 0)
    {
        system->capacity++;
    }

    if ((system->growth & 1) != 0)
    {
        system->growth++;
    }

    s32 slots = static_cast<s16>(system->capacity);
    system->links = static_cast<s16*>(MemoryAllocate2(static_cast<u32>(slots) * sizeof(s16)));
    system->streams = static_cast<FileStream**>(MemoryAllocate2(static_cast<u32>(slots) * sizeof(FileStream*)));
    for (s32 i = 0; i < static_cast<s16>(system->capacity); i++)
    {
        system->links[i] = static_cast<s16>(i + 1);
    }

    system->links[static_cast<s16>(system->capacity) - 1] = PoolFreeListEnd;
    return system;
}

void StreamSystem::Destroy(u32 flags)
{
    vtable = g_StreamSystemVTable;
    if (links != nullptr)
    {
        MemoryDeallocate_(links);
    }

    if (streams != nullptr)
    {
        MemoryDeallocate_(streams);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u16 StreamSystem::Allocate()
{
    if (static_cast<s16>(used) >= static_cast<s16>(capacity))
    {
        Grow();
        return Allocate();
    }

    s16 slot = static_cast<s16>(firstFree);
    firstFree = static_cast<u16>(links[slot]);
    links[slot] = PoolSlotUsed;
    used++;
    return static_cast<u16>(slot);
}

u16 StreamSystem::Add(FileStream** stream)
{
    s16 slot = static_cast<s16>(Allocate());
    streams[slot] = *stream;
    return static_cast<u16>(slot);
}

void StreamSystem::Free(s32 slot)
{
    links[slot] = static_cast<s16>(firstFree);
    firstFree = static_cast<u16>(slot);
    used--;
}

void StreamSystem::Grow()
{
    if ((capacity & 1) != 0)
    {
        capacity++;
    }

    if ((growth & 1) != 0)
    {
        growth++;
    }

    s32 slots = static_cast<s16>(capacity) + static_cast<s16>(growth);
    FileStream** newStreams = static_cast<FileStream**>(MemoryAllocate2(static_cast<u32>(slots) * sizeof(FileStream*)));
    s16* newLinks = static_cast<s16*>(MemoryAllocate2(static_cast<u32>(slots) * sizeof(s16)));
    if (capacity != 0)
    {
        FileStream** oldStreams = streams;
        streams = newStreams;
        for (s32 i = 0; i < static_cast<s16>(capacity); i++)
        {
            if (links[i] == PoolSlotUsed)
            {
                streams[i] = oldStreams[i];
            }
        }

        // The old slots are all in use
        for (s32 i = 0; i < static_cast<s16>(capacity); i++)
        {
            newLinks[i] = PoolSlotUsed;
        }

        if (oldStreams != nullptr)
        {
            MemoryDeallocate_(oldStreams);
        }

        if (links != nullptr)
        {
            MemoryDeallocate_(links);
        }
    }

    s32 slot = static_cast<s16>(capacity);
    s32 end = slot + static_cast<s16>(growth);
    for (; slot < end; slot++)
    {
        newLinks[slot] = static_cast<s16>(slot + 1);
    }

    newLinks[slot - 1] = PoolFreeListEnd;
    u16 oldCapacity = capacity;
    links = newLinks;
    streams = newStreams;
    capacity = static_cast<u16>(oldCapacity + growth);
    firstFree = oldCapacity;
}

extern "C"
{
    s32 InitStreamSystem(s32 channels)
    {
        auto* system = static_cast<StreamSystem*>(MemoryAllocate(sizeof(StreamSystem)));
        StreamSystem::Construct(system, static_cast<u16>(channels));
        system->state = static_cast<s32>(Platform::Stream::State::Stopped);
        void* work = MemoryAllocateAligned(GetHeapManager(), Platform::Stream::WorkMemorySize, Platform::Stream::WorkMemoryAlignment);
        s32 result = Platform::Stream::Initialise(channels, work);
        system->state = static_cast<s32>(Platform::Stream::GetState());
        g_StreamSystem = system;
        return result;
    }

    s32 UpdateStreamSystem(StreamSystem* system)
    {
        s32 result = Platform::Stream::Update();
        system->state = static_cast<s32>(Platform::Stream::GetState());
        return result;
    }

    u32 StreamFreeBufferMemory()
    {
        return Platform::Stream::FreeBufferMemory();
    }

    FileStream* OpenFileStream(StreamSystem* system, u32 bufferSize, u32 location, u32 used)
    {
        auto* stream = static_cast<FileStream*>(MemoryAllocate(sizeof(FileStream)));
        stream->system = system;
        stream->readerSize = 0;
        stream->readerStart = 0;
        stream->pending = nullptr;
        stream->reader = nullptr;
        stream->pendingSize = 0;
        StringConstruct(&stream->path, NewPath);
        stream->file = Platform::Stream::NoFile;
        stream->start = 0;
        stream->size = 0;
        StringConstruct(&stream->archivePath, NewPath);
        stream->archiveFile = Platform::Stream::NoFile;
        stream->archive = nullptr;
        stream->channel = 0;
        stream->archiveState = FileStream::NoArchive;
        stream->flags.value = 0;
        stream->channel = static_cast<u8>(system->Add(&stream));
        FileStreamAttachBuffer(stream, bufferSize, location, used);
        return stream;
    }

    void CloseFileStream(StreamSystem* system, FileStream* stream)
    {
        u8 channel = stream->channel;
        FileStreamDetachBuffer(stream);
        system->Free(channel);
        if (stream != nullptr)
        {
            FileStreamRelease(stream);
            StringDestroy(&stream->archivePath);
            StringDestroy(&stream->path);
            MemoryDeallocate2_(stream);
        }
    }

    void FileStreamAttachBuffer(FileStream* stream, u32 size, u32 location, u32 used)
    {
        u8 channel = stream->channel;
        FileStreamDetachBuffer(stream);
        if (size == 0)
        {
            return;
        }

        Platform::Stream::AttachBuffer(channel, size, location, used);
        stream->flags.hasBuffer = 1;
    }

    void FileStreamDetachBuffer(FileStream* stream)
    {
        if (stream->flags.hasBuffer)
        {
            Platform::Stream::DetachBuffer(stream->channel);
            stream->flags.hasBuffer = 0;
        }
    }

    void FileStreamRelease(FileStream* stream)
    {
        FileStreamDetachBuffer(stream);
        if (stream->file != Platform::Stream::NoFile)
        {
            Platform::Stream::CloseFile(stream->file);
            stream->file = Platform::Stream::NoFile;
        }

        FileStreamDestroyReader(stream);
    }

    bool FileStreamOpen(FileStream* stream, const char* path)
    {
        if (stream->archiveState == FileStream::Archived)
        {
            if (path == nullptr || *path == 0)
            {
                return true;
            }

            ArchiveEntry* entry = ArchiveFind(stream->archive, path);
            if (entry == nullptr)
            {
                return false;
            }

            StringAssign(&stream->path, path);
            stream->start = entry->start;
            stream->size = entry->size;
            return true;
        }

        if (stream->file == Platform::Stream::NoFile)
        {
            stream->file = Platform::Stream::OpenFile(path);
            if (stream->file != Platform::Stream::NoFile)
            {
                stream->flags.reading = 1;
                StringAssign(&stream->path, path);
            }

            stream->size = Platform::Stream::FileSize();
            stream->start = 0;
        }

        if (stream->size == 0)
        {
            StringAssign(&stream->path, NoPath);
            return false;
        }

        // An archive's table was read, then its data file opened: its files are read from it from now on
        if (stream->archiveState == FileStream::OpeningTable)
        {
            stream->archiveState = FileStream::OpeningData;
        }
        else if (stream->archiveState == FileStream::OpeningData)
        {
            stream->archiveFile = stream->file;
            stream->archiveState = FileStream::Archived;
            FileStreamClose(stream);
        }

        return true;
    }

    void FileStreamClose(FileStream* stream)
    {
        if (stream->archiveState == FileStream::ClosingArchive)
        {
            Platform::Stream::CloseFile(stream->archiveFile);
            stream->archiveFile = Platform::Stream::NoFile;
            StringAssign(&stream->archivePath, NoPath);
            return;
        }

        // An archive's data file stays open
        if (stream->archiveState != FileStream::Archived)
        {
            Platform::Stream::CloseFile(stream->file);
            stream->file = Platform::Stream::NoFile;
        }

        stream->flags.reading = 0;
        StringAssign(&stream->path, NoPath);
        if (stream->reader != nullptr)
        {
            Reader(stream)->Rewind();
            stream->readerStart = 0;
            stream->flags.buffered = 0;
        }
    }

    void FileStreamOpenArchive(FileStream* stream, const char* path, s32 readNow, GameReadersStorage* storage)
    {
        if (stream->archiveState != FileStream::NoArchive)
        {
            return;
        }

        stream->archiveState = FileStream::OpeningTable;
        StringAssign(&stream->archivePath, path);
        auto* archive = static_cast<Archive*>(MemoryAllocate(sizeof(Archive)));
        archive->files = nullptr;
        archive->growth = Archive::FilesGrowth;
        archive->count = 0;
        archive->capacity = 0;
        archive->lastFound = nullptr;
        archive->lookups.count = 0;
        archive->lookups.capacity = Archive::LookupsGrowth;
        archive->lookups.growth = Archive::LookupsGrowth;
        String* lookups = NewArray<String>(Archive::LookupsGrowth);
        for (u32 i = 0; i < Archive::LookupsGrowth; i++)
        {
            lookups[i].string = nullptr;
            lookups[i].length = 0;
            lookups[i].capacity = 0;
        }

        archive->lookups.items = lookups;
        stream->archive = archive;
        archive->path.string = nullptr;
        archive->path.capacity = 0;
        archive->path.length = 0;
        ArchiveLoad(archive, path, readNow, storage);
    }

    bool FileStreamRead(FileStream* stream, u32 offset, u32 size, u8* destination, s32 wait, u32* read)
    {
        *read = size;
        u32 position = stream->start + offset;
        u32 end = stream->start + stream->size;
        bool done = false;
        u32 available = 0;
        if (stream->reader != nullptr)
        {
            // The reader goes to the position when it holds it, and starts over when it doesn't
            u32 readerEnd = stream->readerStart + Reader(stream)->Tell();
            if (readerEnd < position)
            {
                s32 ahead = static_cast<s32>(position - readerEnd);
                MemoryStream* reader = stream->reader;
                if (ahead < static_cast<s32>(reader->size - Reader(stream)->Tell()))
                {
                    Reader(stream)->Skip(ahead);
                }
                else
                {
                    Reader(stream)->Rewind();
                    stream->flags.buffered = 0;
                }
            }
            else if (position < readerEnd)
            {
                if (position < stream->readerStart)
                {
                    Reader(stream)->Rewind();
                    stream->flags.buffered = 0;
                }
                else
                {
                    Reader(stream)->Skip(static_cast<s32>(position - readerEnd));
                }
            }

            MemoryStream* reader = stream->reader;
            if (reader != nullptr)
            {
                available = reader->size - Reader(stream)->Tell();
            }
        }

        if (stream->flags.buffered)
        {
            if (available >= size)
            {
                done = true;
                Reader(stream)->Read(destination, size, 1);
            }
            else
            {
                u32 next = position + available;
                u32 left = end - next;
                u32 rest = size - available;
                if (rest < Reader(stream)->Size())
                {
                    // What the reader has, then the reader filled from there on and the rest out of it
                    Reader(stream)->Read(destination, available, 1);
                    destination += available;
                    if (left >= Reader(stream)->Size())
                    {
                        left = Reader(stream)->Size();
                    }

                    Reader(stream)->Rewind();
                    stream->readerStart = next;
                    stream->flags.reading = 1;
                    FileStreamStartRead(stream, next, left, stream->reader->begin);
                    if (wait)
                    {
                        done = true;
                        FileStreamWait(stream);
                        Reader(stream)->Read(destination, rest, 1);
                    }
                    else
                    {
                        stream->pendingSize = rest;
                        stream->pending = destination;
                    }
                }
                else
                {
                    // Too much for the reader: what it has, in whole blocks, and the rest straight into the destination
                    u32 buffered = available & ~(ReadBlockSize - 1);
                    Reader(stream)->Read(destination, buffered, 1);
                    stream->flags.reading = 1;
                    FileStreamStartRead(stream, position + buffered, size - buffered, destination + buffered);
                    if (wait)
                    {
                        FileStreamWait(stream);
                    }

                    done = wait != 0;
                    stream->flags.buffered = 0;
                }
            }
        }
        else if (stream->reader != nullptr && size < Reader(stream)->Size())
        {
            // The reader gets as much of the file from here on as it holds
            u32 left = end - position;
            Reader(stream)->Rewind();
            stream->readerStart = position;
            u32 fill = left;
            if (left >= Reader(stream)->Size())
            {
                fill = Reader(stream)->Size();
            }

            stream->flags.reading = 1;
            FileStreamStartRead(stream, position, fill, stream->reader->begin);
            if (wait)
            {
                done = true;
                FileStreamWait(stream);
                Reader(stream)->Read(destination, size, 1);
            }
            else
            {
                stream->pendingSize = size;
                stream->pending = destination;
            }

            stream->flags.buffered = 1;
        }
        else
        {
            stream->flags.reading = 1;
            FileStreamStartRead(stream, position, size, destination);
            if (wait)
            {
                FileStreamWait(stream);
            }

            done = wait != 0;
        }

        MemoryStream* reader = stream->reader;
        if (reader != nullptr)
        {
            u32 told = Reader(stream)->Tell();
            if (reader->size == told)
            {
                stream->flags.buffered = 0;
            }
        }

        return done;
    }

    s32 FileStreamReadSoundBank(FileStream* stream, u32 bank, u32 offset, u32 size, s32 wait)
    {
        stream->flags.reading = 1;
        FileStreamStartSoundBankRead(stream, bank, stream->start + offset, size);
        if (wait)
        {
            FileStreamWait(stream);
        }

        return 1;
    }

    bool FileStreamPoll(FileStream* stream)
    {
        if (stream->flags.reading && !FileStreamIsReading(stream))
        {
            HandOver(stream);
        }

        return stream->flags.reading;
    }

    void FileStreamWait(FileStream* stream)
    {
        while (FileStreamPoll(stream))
        {
            FileStreamWaitRead(stream);
        }
    }

    void FileStreamStartRead(FileStream* stream, u32 offset, u32 size, u8* destination)
    {
        Platform::Stream::Read(stream->channel, stream->file, offset, size, destination);
    }

    void FileStreamStartSoundBankRead(FileStream* stream, u32 bank, u32 offset, u32 size)
    {
        Platform::Stream::ReadSoundBank(stream->channel, bank, stream->file, offset, size);
    }

    s32 FileStreamWaitRead(FileStream* stream)
    {
        return Platform::Stream::Wait(stream->channel);
    }

    bool FileStreamIsReading(FileStream* stream)
    {
        // Test time (debug.h): a read is done once it's asked about, however long frames took
        if (g_DebugFixedTime != 0)
        {
            Platform::Stream::Wait(stream->channel);
        }

        return Platform::Stream::IsReading(stream->channel);
    }

    void FileStreamCreateReader(FileStream* stream, u32 size)
    {
        stream->readerSize = size;
        void* memory = MemoryAllocateAligned(GetHeapManager(), stream->readerSize, MemoryStream::FileAlignment);
        auto* reader = static_cast<MemoryStream*>(MemoryAllocate(sizeof(MemoryStream)));
        stream->reader = MemoryStream::Construct(reader, memory, stream->readerSize, 1, MemoryStream::FileAlignment);
        Reader(stream)->Rewind();
        stream->flags.buffered = 0;
    }

    void FileStreamEmptyReader(FileStream* stream)
    {
        if (stream->reader != nullptr)
        {
            Reader(stream)->SeekToEnd();
            stream->flags.buffered = 0;
        }
    }

    void FileStreamDestroyReader(FileStream* stream)
    {
        if (stream->reader != nullptr)
        {
            Reader(stream)->Destroy(DestroyAndFree);
        }

        stream->reader = nullptr;
    }

    void LoadSoundBank(SoundBankFiles* bank, const char* name)
    {
        FileStream* stream = OpenFileStream(g_StreamSystem, SoundBankBufferSize, 0, 0);
        if (bank->header != nullptr)
        {
            FreeMemory(GetHeapManager(), bank->header);
            bank->header = nullptr;
        }

        String path;
        StringConstruct(&path, name);
        char dot[2] = {'.', 0};
        StringAppend(&path, dot);
        StringAppend(&path, "mh");
        u8* header = nullptr;
        if (FileStreamOpen(stream, path.string))
        {
            header = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), stream->size, MemoryStream::FileAlignment));
            stream->flags.reading = 1;
            FileStreamStartRead(stream, 0, stream->size, header);
            FileStreamWait(stream);
            FileStreamClose(stream);
        }

        StringDestroy(&path);
        bank->header = header;
        CloseFileStream(g_StreamSystem, stream);
        char samplesPath[SamplesPathSize];
        RetailLibc::Format(samplesPath, "%s.mb", name);
        if (bank->samples != Platform::Stream::NoFile)
        {
            Platform::Stream::CloseFile(bank->samples);
        }

        bank->samples = Platform::Stream::OpenFile(samplesPath);
    }

    MusicTrack* FindMusicTrack(SoundBankFiles* bank, u32 track)
    {
        // The header: how many tracks, a word, then the tracks
        constexpr u32 TracksOffset = 8;
        u8* header = bank->header;
        u32 last = *reinterpret_cast<const u32*>(header) - 1;
        u32 index = track < last ? track : last;
        return reinterpret_cast<MusicTrack*>(header + TracksOffset) + index;
    }
}
