#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/stream.h"
#include "game/string.h"
#include "game/archive.h"
#include "game/pools.h"

struct FileStream;
struct GameReadersStorage;

// The file streams, in a pool of slots whose numbers are the streams' channels (Platform::Stream's). The free slots are chained
// through the links like an ItemPool's (game/pools.h), and the pool grows by growth slots when it's full. The game's pools are
// copies of one template, each with only a destructor in its vtable. It also keeps the state of the disc for the frame loop
class StreamSystem
{
public:
    u16 capacity;
    u16 growth;
    u16 used;
    u16 firstFree;
    s16* links;
    FileStream** streams;
    const GccVTableEntry* vtable;
    // Platform::Stream::State, as of the last update
    s32 state;
    s32 unused18;

    static StreamSystem* Construct(StreamSystem* system, u16 capacity) RETAIL(FUN_002ae608);
    void Destroy(u32 flags) RETAIL(FUN_002ae6f0);
    // Returns the slot the stream got
    u16 Add(FileStream** stream) RETAIL(FUN_002ae760);
    u16 Allocate() RETAIL(FUN_002ae820);
    void Free(s32 slot) RETAIL(FUN_002ae7f8);
    void Grow() RETAIL(FUN_002abca8);
};
CHECK_SIZE(StreamSystem, 0x1C);
CHECK_OFFSET(StreamSystem, vtable, 0x10);

union FileStreamFlags
{
    u16 value;
    struct
    {
        // A read was started and its bytes aren't handed over yet (FileStreamPoll)
        u16 reading : 1;
        // Its channel has a buffer (FileStreamAttachBuffer)
        u16 hasBuffer : 1;
        // The reader holds the file's bytes from readerStart on
        u16 buffered : 1;
        u16 unused3 : 13;
    };
};
CHECK_SIZE(FileStreamFlags, 2);

// A file read on a channel, through a reader of its own buffering what's read in parts: a file of the disc, or the files of an
// archive (once the archive's table is read, the data file is opened and stays open)
struct FileStream
{
    enum ArchiveState : u8
    {
        NoArchive = 0,
        // Opening the table, then the data file
        OpeningTable = 1,
        OpeningData = 2,
        Archived = 3,
        ClosingArchive = 4,
    };

    u8 channel;
    u8 archiveState;
    FileStreamFlags flags;
    StreamSystem* system;
    u32 readerSize;
    u32 readerStart;
    // Where to copy what's being read into the reader once it's there, and how much
    u8* pending;
    MemoryStream* reader;
    u32 pendingSize;
    String path;
    s32 file;
    // The file's place in the file read (an archive's data file) and its size
    u32 start;
    u32 size;
    String archivePath;
    s32 archiveFile;
    Archive* archive;
};
CHECK_SIZE(FileStream, 0x48);
CHECK_OFFSET(FileStream, flags, 2);
CHECK_OFFSET(FileStream, path, 0x1C);
CHECK_OFFSET(FileStream, archive, 0x44);

// A sound bank's files: its header (the MH file, in memory) and its samples (the MB file, kept open)
struct SoundBankFiles
{
    u8* header;
    s32 samples;
};
CHECK_SIZE(SoundBankFiles, 8);

extern "C"
{
    extern StreamSystem* g_StreamSystem RETAIL(G_SomeUnkStruct_27);

    // Makes the stream system with so many channels and starts reading. Returns Platform::Stream::Initialise's result
    s32 InitStreamSystem(s32 channels) RETAIL(FUN_002adde0);
    s32 UpdateStreamSystem(StreamSystem* system) RETAIL(FUN_002abb48);
    u32 StreamFreeBufferMemory() RETAIL(FUN_002ae138);

    // A stream on a channel of its own, with the channel's buffer (Platform::Stream::AttachBuffer, none for a size of 0)
    FileStream* OpenFileStream(StreamSystem* system, u32 bufferSize, u32 location, u32 used) RETAIL(FUN_002aded0);
    void CloseFileStream(StreamSystem* system, FileStream* stream) RETAIL(FUN_002adfd0);
    void FileStreamAttachBuffer(FileStream* stream, u32 size, u32 location, u32 used) RETAIL(FUN_002ae048);
    void FileStreamDetachBuffer(FileStream* stream) RETAIL(FUN_002ae0e8);
    // Closes the file and drops the reader
    void FileStreamRelease(FileStream* stream) RETAIL(FUN_002ad808);

    // Opens the file, a file of the disc or of the archive. Returns whether there is one
    bool FileStreamOpen(FileStream* stream, const char* path) RETAIL(FUN_002ab3a8);
    void FileStreamClose(FileStream* stream) RETAIL(FUN_002ad860);
    // Opens an archive: its table's file, read by the storage's readers
    void FileStreamOpenArchive(FileStream* stream, const char* path, s32 readNow, GameReadersStorage* storage)
        RETAIL(FUN_002adc10);

    // Reads size bytes of the file from offset into the destination, through the reader when they fit in it. Without waiting
    // the bytes are there once the stream is done reading (FileStreamPoll). Returns whether they're there
    bool FileStreamRead(FileStream* stream, u32 offset, u32 size, u8* destination, s32 wait, u32* read) RETAIL(FUN_002ab500);
    // The file's bytes into the sound processor's memory, as the bank's samples. Returns 1
    s32 FileStreamReadSoundBank(FileStream* stream, u32 bank, u32 offset, u32 size, s32 wait) RETAIL(FUN_002ad930);
    // Whether it's still reading (it hands the bytes over once it's done)
    bool FileStreamPoll(FileStream* stream) RETAIL(FUN_002ad990);
    void FileStreamWait(FileStream* stream) RETAIL(FUN_002ada20);

    // The channel's side of reading: the file's bytes straight into memory (the stream's start isn't added)
    void FileStreamStartRead(FileStream* stream, u32 offset, u32 size, u8* destination) RETAIL(FUN_002ae228);
    void FileStreamStartSoundBankRead(FileStream* stream, u32 bank, u32 offset, u32 size) RETAIL(FUN_002ae2e8);
    s32 FileStreamWaitRead(FileStream* stream) RETAIL(FUN_002ae2b0);
    bool FileStreamIsReading(FileStream* stream) RETAIL(FUN_002ae370);

    // The reader, of size bytes of memory of its own
    void FileStreamCreateReader(FileStream* stream, u32 size) RETAIL(FUN_002adad8);
    // Forgets what the reader holds
    void FileStreamEmptyReader(FileStream* stream) RETAIL(FUN_002adb68);
    void FileStreamDestroyReader(FileStream* stream) RETAIL(FUN_002adbc0);

    // Reads the bank's header (name.mh) and opens its samples (name.mb), closing the bank's samples before
    void LoadSoundBank(SoundBankFiles* bank, const char* name) RETAIL(FUN_002ab9e0);
}
