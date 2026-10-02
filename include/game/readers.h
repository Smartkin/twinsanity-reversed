#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/string.h"

struct FileStream;
struct Archive;
class SectionReader;
class GenericItemReader;

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

// Reads a part of a file (its own, the stream's open one, or one of the current archive) into memory of the heap or the disk
// manager, and hands it to its section reader
class SubItemsReader : public GenericItemReader
{
public:
    enum Bits : u32
    {
        Found = 0x1,
        Read = 0x2,
        // Its memory is the disk manager's (diskIndex)
        OnDisk = 0x8,
        FromArchive = 0x20,
        // Opens its file and reads all of it
        WholeFile = 0x4,
        CloseFile = 0x40,
        // Its memory goes once the section reader had it
        Release = 0x80,
        // The size is cut to what's left of the file
        ClampToFile = 0x100,
        NothingToRead = 0x200,
    };

    u32 bits;
    String path;
    u8* data;
    u32 size;
    u32 offset;
    s32 diskIndex;
    s32* diskHandle;
    FileStream* stream;

    // The flags: bit 0 release, 1 ..., 3 on the disk manager, 5 clamp to the file
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

// The sound bank a sound bank reader reads the samples of
struct SoundBankEntry
{
    u32 unknown00;
    u32 id;
    // Bit 0: its samples are in the sound processor's memory
    u32 flags;
};

// Reads a sound bank's samples into the sound processor's memory
class SoundBankReader : public GenericItemReader
{
public:
    // Bit 0 found, 1 read, 2 opens its file and reads all of it, 3 closes the file once read
    u32 bits;
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
    u8 unknown16;
    u8 unknown17;

    static BdReader* Construct(BdReader* reader, String* path, SectionReader* sectionReader, u8 inArchive, u8 unknown16)
        RETAIL(InitBDReader);
    void Destroy(u32 flags) RETAIL(FUN_002acab0);
    void Begin(FileStream* stream) RETAIL(FUN_002acbc0);
    bool IsDone() RETAIL(FUN_002acc30);
    void Finish(ReaderStack* stack) RETAIL(FUN_002acc38);
    void Restart() RETAIL(FUN_002acb30);
};
CHECK_SIZE(BdReader, 0x18);

// The readers of one of the two file streams: a ring buffer of readers waiting, and the stack the one reading pushes onto
struct GameReadersStorage
{
    // Bits 0-2 its number, 3 a reader is finishing (readers queued go on the stack)
    u32 bits;
    FileStream* stream;
    GenericItemReader** buffer;
    u32 bufferCapacity;
    u32 bufferStart;
    u32 bufferCount;
    GenericItemReader* current;
    ReaderStack stack;
};
CHECK_SIZE(GameReadersStorage, 0x24);

// What a section holds: the game's items read out of it (its vtable is the item type's)
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
};

// An item of a section's table: its place after the section's start, its size and its ID
struct ItemHeader
{
    u32 offset;
    s32 size;
    u32 id;
};

// A section's header (a type of 16 bits and a version below 2, the items and the size) at a place of a file, read for an item
class PackageSectionReader
{
public:
    const GccVTableEntry* vtable;
    ItemInterface* item;
    u32 start;
    // The file stays open after the header's read
    u8 keepOpen;

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
    extern GameReadersStorage* g_ReadersStorages[2] RETAIL(G_GameReadersStorages);
    // The item interface's (its destructor is the items' base destructor)
    extern const GccVTableEntry g_ItemInterfaceVTable[] RETAIL(ItemInterface_methods);
    extern const GccVTableEntry g_PackageSectionReaderVTable[] RETAIL(PackageSecReader_methods);
    extern const GccVTableEntry g_ItemSectionReaderVTable[] RETAIL(ItemSectionReader_methods);
    extern const GccVTableEntry g_ItemsReadSectionReaderVTable[] RETAIL(D_00306EB0);

    // Queues the reading of a file's first section (the whole file is the package), closing the file afterwards unless asked not
    // to, or of the section at a place of the stream's open file
    void AddResourcePackageToLoadQueue(ItemInterface* item, const char* path, s32 keepOpen);
    void AddSectionToLoadQueue(ItemInterface* item, u32 start);
    // A section's header: its table of items is queued when the item reads its type
    void ReadSectionHeader(const u8* data, ItemInterface* item, u32 start) RETAIL(FUN_002b4970);
    ItemHeader* SetNextItem(ItemHeader* header, const ItemHeader* next, u32 start);
    u32 GetItemsAmountInItem(const u8* section);
    u32 GetSectionSize(const u8* section);
    // The archive readers look files up in, which nothing sets
    extern Archive* g_CurrentArchive RETAIL(D_0030A438);
    extern const GccVTableEntry g_GenericItemReaderVTable[] RETAIL(ItemReaderInterface_methods);
    extern const GccVTableEntry g_FileCloseReaderVTable[] RETAIL(D_00306BC0);
    extern const GccVTableEntry g_MemoryReaderVTable[] RETAIL(D_00306B88);
    extern const GccVTableEntry g_SubItemsReaderVTable[] RETAIL(SubSectionsReader_methods);
    extern const GccVTableEntry g_SoundBankReaderVTable[] RETAIL(D_00306AA8);
    extern const GccVTableEntry g_BdReaderVTable[] RETAIL(D_00306A70);
    // 0 a step a frame, 1 everything now, 2 everything now once a movie gave the sound back
    extern s32 g_ReadersMode RETAIL(D_0030A430);
    extern u32 g_ReadersSteps RETAIL(D_0030A434);
    // The disk manager compacts while everything is read
    extern u8 g_ReadersCompact RETAIL(D_0030A423);

    // Storage 1 reads in blocks of 64 KB, storage 0 in blocks of 128 KB
    GameReadersStorage* InitReadersStorage(u32 index);
    // At the back of the queue (on the stack while a reader finishes), or at its front
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
