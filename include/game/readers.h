#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/resources.h"
#include "game/string.h"

struct FileStream;
struct Archive;
class SectionReader;
class GenericItemReader;

// The two storages of readers (g_ReadersStorages): the game's sections and files (its stream reads in blocks of 128 KB), and the
// files read on their own, whose readers go first (in blocks of 64 KB: the menus' pictures, MemoryStream::LoadFile's files)
enum ReadersStorageIndex : u32
{
    MainReaders = 0,
    FileReaders = 1,
    ReadersStorageCount = 2,
};

// Where AddItemReaderToReaderStorage queues a reader (any other value is the front)
enum ReaderQueuePlace : s32
{
    QueueBack = 0,
    QueueFront = 1,
};

// What the readers do (g_ReadersMode): read; hold while a movie plays (it has the disc: the readers reading finish, no others
// start); and once it's over, start the readers that were reading over (the movie emptied their streams) and read again, the
// movie's sound given back
enum ReadersMode : s32
{
    ReadersReading = 0,
    ReadersHeldByMovie = 1,
    ReadersAfterMovie = 2,
};

// Readers waiting to read: a reader that finished pushes the readers of what it found here
struct ReaderStack
{
    GenericItemReader** items;
    s16 capacity;
    u16 count;
};

// A reader of a section of a file: started on a file stream, done once the stream has read it, then it hands the section to
// its section reader
class GenericItemReader
{
public:
    SectionReader* sectionReader;
    const GccVTableEntry* vtable;

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, 1, flags);
    }

    void Begin(FileStream* stream)
    {
        CallVirtual<void>(this, vtable, 2, stream);
    }

    bool IsDone()
    {
        return CallVirtual<u32>(this, vtable, 3) != 0;
    }

    void Finish(ReaderStack* stack)
    {
        CallVirtual<void>(this, vtable, 4, stack);
    }

    void Restart()
    {
        CallVirtual<void>(this, vtable, 5);
    }

    static GenericItemReader* Construct(GenericItemReader* reader, SectionReader* sectionReader) RETAIL(FUN_002ace20);
    // The base's destructor: the section reader goes with it
    void DestroyGeneric(u32 flags) RETAIL(FUN_002ae590);
    void Nothing() RETAIL(FUN_002ae600);
};
CHECK_SIZE(GenericItemReader, 8);

// Closes the stream's file, then tells its section reader (no memory, a size of -1)
class FileCloseReader : public GenericItemReader
{
public:
    static FileCloseReader* Construct(FileCloseReader* reader, SectionReader* sectionReader) RETAIL(FUN_002abeb0);
    void Destroy(u32 flags) RETAIL(FUN_002abe38);
    void Begin(FileStream* stream) RETAIL(FUN_002abee8);
    bool IsDone() RETAIL(FUN_002abf08);
    void Finish(ReaderStack* stack) RETAIL(FUN_002abf10);
    void Restart() RETAIL(FUN_002abea8);
};

// Hands memory it has to its section reader
class MemoryReader : public GenericItemReader
{
public:
    u8* data;
    u32 size;

    static MemoryReader* Construct(MemoryReader* reader, SectionReader* sectionReader, u8* data, u32 size) RETAIL(FUN_002abfc0);
    void Destroy(u32 flags) RETAIL(FUN_002abf50);
    void Begin(FileStream* stream) RETAIL(FUN_002ac018);
    bool IsDone() RETAIL(FUN_002ac020);
    void Finish(ReaderStack* stack) RETAIL(Load);
    void Restart() RETAIL(FUN_002ac060);
};
CHECK_SIZE(MemoryReader, 0x10);

// What a sub items reader does and has done
union SubItemsReaderBits
{
    u32 value;
    struct
    {
        // Its file is there (found in the archive, open), and it was read
        u32 found : 1;
        u32 read : 1;
        // Opens its file and reads all of it
        u32 wholeFile : 1;
        // Its memory is the disk manager's (diskIndex)
        u32 onDisk : 1;
        // The options' bit 0, never read
        u32 unused4 : 1;
        // Its part is a file of the current archive, read while the stream has a file of the disc open
        u32 fromArchive : 1;
        u32 closesFile : 1;
        // Its memory goes once the section reader had it
        u32 releases : 1;
        // The size is cut to what's left of the file
        u32 clampsToFile : 1;
        // Nothing is read and the file is closed (nothing sets it)
        u32 nothingToRead : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(SubItemsReaderBits, 4);

// What a sub items reader is made to do (its constructors' flags)
union SubItemsReaderOptions
{
    u32 value;
    struct
    {
        // Kept as the reader's unused4
        u32 unused0 : 1;
        u32 fromArchive : 1;
        // The file closed once read (not a file of the archive)
        u32 closesFile : 1;
        // Read into the disk manager's memory and released once read (ConstructOnDisk's always are on the disk manager: only
        // released)
        u32 onDisk : 1;
        u32 unused4 : 1;
        u32 clampsToFile : 1;
        u32 unused6 : 26;
    };

    // The options' masks, which the readers are made with
    enum Mask : u32
    {
        Unused0 = 0x1,
        FromArchive = 0x2,
        ClosesFile = 0x4,
        OnDisk = 0x8,
        ClampsToFile = 0x20,
    };
};
CHECK_SIZE(SubItemsReaderOptions, 4);

// Reads a part of a file (its own, the stream's open one, or one of the current archive) into memory of the heap or the disk
// manager, and hands it to its section reader
class SubItemsReader : public GenericItemReader
{
public:
    SubItemsReaderBits bits;
    String path;
    u8* data;
    u32 size;
    u32 offset;
    s32 diskIndex;
    s32* diskHandle;
    FileStream* stream;

    // Made with its options (SubItemsReaderOptions): a whole file, a part of a file, a part of the stream's open file, and a part
    // of the stream's open file into the disk manager's memory of a handle given
    static SubItemsReader* ConstructFile(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags)
        RETAIL(InitUnkSecReader_0x30);
    static SubItemsReader* ConstructPart(SubItemsReader* reader, const char* path, SectionReader* sectionReader, u32 flags,
                                         u32 offset, u32 size) RETAIL(CreateSubItemsReader);
    static SubItemsReader* ConstructOpen(SubItemsReader* reader, SectionReader* sectionReader, u32 flags, u32 offset, u32 size)
        RETAIL(CreateSubItemsReader_002AC4A8);
    static SubItemsReader* ConstructOnDisk(SubItemsReader* reader, s32* diskHandle, u32 flags, u32 offset, u32 size)
        RETAIL(FUN_002ac588);
    void Destroy(u32 flags) RETAIL(FUN_002ac660);
    void Begin(FileStream* stream) RETAIL(FUN_002a98c0);
    bool IsDone() RETAIL(FUN_002ac6e0);
    void Finish(ReaderStack* stack) RETAIL(FUN_002ac748);
    void Restart() RETAIL(FUN_002ac870);
};
CHECK_SIZE(SubItemsReader, 0x30);

// Two readers of a part of the stream's open file into memory given them, which nothing makes (their vtables follow the sub items
// reader's, 0x38 and 0x70 bytes past it): the first waits for the bytes; the second polls the stream until they're there, its
// part one of the current archive's last found file when asked to and the stream has a file of the disc open (nothing read
// without a file found)
class WaitingPartReader : public GenericItemReader
{
public:
    u32 offset;
    u32 size;
    u8* data;

    void Destroy(u32 flags) RETAIL(FUN_002ac200);
    void Begin(FileStream* stream) RETAIL(FUN_002ac270);
    bool IsDone() RETAIL(FUN_002ac2a8);
    void Finish(ReaderStack* stack) RETAIL(FUN_002ac2b0);
};
CHECK_SIZE(WaitingPartReader, 0x14);

class PolledPartReader : public GenericItemReader
{
public:
    u32 offset;
    u32 size;
    u8* data;
    u32 unused14;
    // The bytes were there at once; else the stream polled
    u8 read;
    FileStream* stream;
    u8 inArchive;

    void Destroy(u32 flags) RETAIL(FUN_002ac068);
    void Begin(FileStream* stream) RETAIL(FUN_002ac0d8);
    bool IsDone() RETAIL(FUN_002ac190);
    void Finish(ReaderStack* stack) RETAIL(FUN_002ac1c0);
};
CHECK_OFFSET(PolledPartReader, read, 0x18);
CHECK_OFFSET(PolledPartReader, stream, 0x1C);
CHECK_OFFSET(PolledPartReader, inArchive, 0x20);

// A sound's flags
union SoundBankFlags
{
    u32 value;
    struct
    {
        // Its samples are in the sound processor's memory
        u32 samplesLoaded : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(SoundBankFlags, 4);

// The sound a sound bank reader reads the samples of (the head of game/sound.h's GameSound): its resource header, whose ID is the
// sound bank's, and its flags
struct SoundBankEntry
{
    ResourceHeader header;
    SoundBankFlags flags;
};

// What a sound bank reader does and has done (nothing sets bits 2 and 3)
union SoundBankReaderBits
{
    u32 value;
    struct
    {
        u32 found : 1;
        u32 read : 1;
        // Opens its file and reads all of it
        u32 wholeFile : 1;
        u32 closesFile : 1;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(SoundBankReaderBits, 4);

// Reads a sound bank's samples into the sound processor's memory
class SoundBankReader : public GenericItemReader
{
public:
    SoundBankReaderBits bits;
    SoundBankEntry* bank;
    String path;
    u32 offset;
    u32 size;
    FileStream* stream;

    static SoundBankReader* Construct(SoundBankReader* reader, SoundBankEntry* bank, u32 offset, u32 size) RETAIL(FUN_002ac958);
    void Destroy(u32 flags) RETAIL(FUN_002ac8e0);
    s32 Begin(FileStream* stream) RETAIL(FUN_002a9b48);
    bool IsDone() RETAIL(FUN_002ac9e8);
    void Finish(ReaderStack* stack) RETAIL(FUN_002aca50);
    void Restart() RETAIL(FUN_002acaa8);
};
CHECK_SIZE(SoundBankReader, 0x28);

// Opens a file (the BD) on the stream, or finds it in the current archive, and tells its section reader whether it's there
class BdReader : public GenericItemReader
{
public:
    String path;
    u8 found;
    u8 inArchive;
    // Set by the constructor, never read
    u8 unused16;
    u8 unused17;

    static BdReader* Construct(BdReader* reader, String* path, SectionReader* sectionReader, u8 inArchive, u8 unused)
        RETAIL(InitBDReader);
    void Destroy(u32 flags) RETAIL(FUN_002acab0);
    void Begin(FileStream* stream) RETAIL(FUN_002acbc0);
    bool IsDone() RETAIL(FUN_002acc30);
    void Finish(ReaderStack* stack) RETAIL(FUN_002acc38);
    void Restart() RETAIL(FUN_002acb30);
};
CHECK_SIZE(BdReader, 0x18);

// A storage's bits
union ReadersStorageBits
{
    u32 value;
    struct
    {
        // Its index, never read
        u32 unused0 : 3;
        // A reader is finishing: the readers queued go on the stack
        u32 finishing : 1;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(ReadersStorageBits, 4);

// The readers of one of the two file streams: a ring buffer of readers waiting, and the stack the one reading pushes onto
struct GameReadersStorage
{
    ReadersStorageBits bits;
    FileStream* stream;
    GenericItemReader** buffer;
    u32 bufferCapacity;
    u32 bufferStart;
    u32 bufferCount;
    GenericItemReader* current;
    ReaderStack stack;
};
CHECK_SIZE(GameReadersStorage, 0x24);

// What a section holds: the game's items read out of it (its vtable is the item type's: 1 the destructor, 2 how many items or
// resources it has, 3 the section type it reads (nothing calls it), 4 whether it reads a section's type, 5 an item's section reader,
// 6 what it holds let go of, 7 how many items there are, 8 every item read)
class ItemInterface
{
public:
    const GccVTableEntry* vtable;

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, 1, flags);
    }

    // Whether it reads the section of the type
    bool CanRead(u32 type)
    {
        return CallVirtual<u32>(this, vtable, 4, type) != 0;
    }

    // The section reader of an item (its size can be changed), nullptr for none
    SectionReader* GetReader(s32 index, struct ItemHeader* header, s32* size)
    {
        return CallVirtual<SectionReader*>(this, vtable, 5, index, header, size);
    }

    void SetCount(u32 count)
    {
        CallVirtual<void>(this, vtable, 7, count);
    }

    // Every item was read: how many had readers, how many there were, and where the last one ended
    void Finish(s32 read, u32 count, u32 end)
    {
        CallVirtual<void>(this, vtable, 8, read, count, end);
    }

    // The interface's own: its destructor, and the count and the end it's told of (it does nothing with them)
    void BaseDestroy(u32 flags) RETAIL(DestroyItem);
    void BaseSetCount(u32 count) RETAIL(FUN_00179528);
    void BaseFinish(s32 read, u32 count, u32 end) RETAIL(FUN_00179530);
};

// An item of a section's table: its place after the section's start, its size and its ID
struct ItemHeader
{
    u32 offset;
    s32 size;
    u32 id;
};

// A section's type (TT Lab's magic number), which the item reading it checks: the graphics section's subsections (a kind of
// graphics resources each) have their own, every other section the default
enum SectionType : u32
{
    DefaultSectionType = 1,
    GraphicsKindSectionType = 3,
};

// A section's first word: its type and its version (sections of later versions than 1 aren't read)
union SectionFormat
{
    u32 value;
    struct
    {
        u32 type : 16;
        u32 version : 16;
    };
};
CHECK_SIZE(SectionFormat, 4);

constexpr u32 SectionVersions = 2;

// A section's header (0xC bytes): its format, how many items its table (after the header) has and its size
struct SectionHeader
{
    SectionFormat format;
    u32 itemCount;
    u32 size;
};
CHECK_SIZE(SectionHeader, 0xC);

// A section's header (SectionHeader) at a place of a file, read for an item
class PackageSectionReader
{
public:
    const GccVTableEntry* vtable;
    ItemInterface* item;
    u32 start;
    // Whether the package is a file of the current archive, never read
    u8 unused0C;

    void Destroy(u32 flags) RETAIL(FUN_002b7390);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadSectionItems);
};
CHECK_SIZE(PackageSectionReader, 0x10);

// A section's table of items: a reader for each item its item gives a section reader for, then the item is told
class ItemSectionReader
{
public:
    const GccVTableEntry* vtable;
    ItemInterface* item;
    u32 count;
    u32 size;
    u32 start;
    // The file the items are in, the stream's open one when empty
    String path;

    void Destroy(u32 flags) RETAIL(FUN_002b6810);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadItemSection);
};
CHECK_SIZE(ItemSectionReader, 0x20);

class ItemsReadSectionReader
{
public:
    const GccVTableEntry* vtable;
    ItemInterface* item;
    u32 count;
    s32 read;
    u32 end;

    void Destroy(u32 flags) RETAIL(FUN_002b6860);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002b68b0);
};
CHECK_SIZE(ItemsReadSectionReader, 0x14);

extern "C"
{
    extern GameReadersStorage* g_ReadersStorages[ReadersStorageCount] RETAIL(G_GameReadersStorages);
    // The item interface's (its destructor is the items' base destructor)
    extern const GccVTableEntry g_ItemInterfaceVTable[] RETAIL(ItemInterface_methods);
    extern const GccVTableEntry g_PackageSectionReaderVTable[] RETAIL(PackageSecReader_methods);
    extern const GccVTableEntry g_ItemSectionReaderVTable[] RETAIL(ItemSectionReader_methods);
    extern const GccVTableEntry g_ItemsReadSectionReaderVTable[] RETAIL(D_00306EB0);

    // Queues the reading of a file's first section (the whole file is the package): a file of its own, closed once read, or one
    // of the current archive (every caller's is its own); or of the section at a place of the stream's open file
    void AddResourcePackageToLoadQueue(ItemInterface* item, const char* path, s32 inArchive);
    void AddSectionToLoadQueue(ItemInterface* item, u32 start);
    // A section's header: its table of items is queued when the item reads its type
    void ReadSectionHeader(const u8* data, ItemInterface* item, u32 start) RETAIL(FUN_002b4970);
    // An item's header from its section's table, its place made the file's (the section's start added)
    ItemHeader* ItemHeaderInFile(ItemHeader* header, const ItemHeader* entry, u32 start) RETAIL(SetNextItem);
    // A section header's count of items and size
    u32 SectionItemCount(const u8* section) RETAIL(GetItemsAmountInItem);
    u32 GetSectionSize(const u8* section);
    // The archive readers look files up in, which nothing sets
    extern Archive* g_CurrentArchive RETAIL(D_0030A438);
    extern const GccVTableEntry g_GenericItemReaderVTable[] RETAIL(ItemReaderInterface_methods);
    extern const GccVTableEntry g_FileCloseReaderVTable[] RETAIL(D_00306BC0);
    extern const GccVTableEntry g_MemoryReaderVTable[] RETAIL(D_00306B88);
    extern const GccVTableEntry g_SubItemsReaderVTable[] RETAIL(SubSectionsReader_methods);
    extern const GccVTableEntry g_SoundBankReaderVTable[] RETAIL(D_00306AA8);
    extern const GccVTableEntry g_BdReaderVTable[] RETAIL(D_00306A70);
    // ReadersMode, and the steps the readers took (both storages' at once)
    extern s32 g_ReadersMode RETAIL(D_0030A430);
    extern u32 g_ReadersSteps RETAIL(D_0030A434);
    // The disk manager compacts while everything is read
    extern u8 g_ReadersCompact RETAIL(D_0030A423);

    // A storage made (ReadersStorageIndex) with its stream, the first time
    GameReadersStorage* InitReadersStorage(u32 index);
    // At the back of the queue (on the stack while a reader finishes), or at its front (ReaderQueuePlace)
    void AddItemReaderToReaderStorage(GameReadersStorage* storage, GenericItemReader* reader, s32 front);
    // Starts the next reader. Returns whether there was one
    bool StorageStartNext(GameReadersStorage* storage) RETAIL(SetNextArchiveReader_);
    // A step of the storage's reading: whether a reader started goes to started. Returns whether anything's left
    bool StorageStep(GameReadersStorage* storage, bool* started) RETAIL(ReadSections_);
    // Reads everything queued
    void LoadQueuedSectionsIntoMemory_();
    // A step of both storages. Returns whether anything's left, whether a reader started goes to started
    u32 ReadersStep(bool* started) RETAIL(FUN_002b54a8);
}
