#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/string.h"

class GenericItemReader;
struct GameReadersStorage;
struct ReaderStack;

// Where a file of an archive is in its data file (the BD)
struct ArchiveEntry
{
    u32 start;
    u32 size;
};

struct ArchiveFile
{
    ArchiveEntry entry;
    String path;
};
CHECK_SIZE(ArchiveFile, 0x14);

// A list of strings growing by growth (GCC's new[] of them, with a cookie of 16 bytes counting them)
struct StringList
{
    String* items;
    u32 count;
    u32 capacity;
    u32 growth;
};

// The retail iterator over the string lists (D_002EC458, its base D_002EC4A8; the game context's preloaded chunks), which the C++
// walks with loops: the list and the index it's at, done outside the list
struct StringListIterator
{
    const GccVTableEntry* vtable;
    StringList* list;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_00101458);
    void BaseDestroy(u32 flags) RETAIL(FUN_00101428);
    void First() RETAIL(FUN_00101488);
    u32 IsDone() RETAIL(FUN_00101620);
    String* Current() RETAIL(FUN_001014b8);
    void Next() RETAIL(FUN_00101650);
    void Previous() RETAIL(FUN_001014a8);
    void Last() RETAIL(FUN_00101490);
    StringListIterator* Assign(const StringListIterator* other) RETAIL(FUN_00101660);
};
CHECK_SIZE(StringListIterator, 0xC);

// An archive: the table of its files (read from the BH), sorted by path, growing by growth files
struct Archive
{
    // What a new archive's table grows by, and its list of lookups' room and growth
    static constexpr u16 FilesGrowth = 0x40;
    static constexpr u32 LookupsGrowth = 10;

    ArchiveFile* files;
    u16 count;
    u16 capacity;
    u16 growth;
    // The last file looked up
    ArchiveFile* lastFound;
    // Every path looked up, as asked for
    StringList lookups;
    String path;
};
CHECK_SIZE(Archive, 0x2C);
CHECK_OFFSET(Archive, path, 0x20);

// What reads a section the readers read: its vtable's Read gets the section's bytes
class SectionReader
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ReadSlot = 2,
        MissingSlot = 3,
    };

    const GccVTableEntry* vtable;

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, flags);
    }

    // The readers of what's in the section go on the stack
    void Read(u8* data, u32 size, ReaderStack* readers)
    {
        CallVirtual<void>(this, vtable, ReadSlot, data, size, readers);
    }

    // The section's file wasn't there
    void Missing(u8* data, u32 size, ReaderStack* readers)
    {
        CallVirtual<void>(this, vtable, MissingSlot, data, size, readers);
    }

    // The base's destructor (its vtable's slot 1)
    void BaseDestroy(u32 flags) RETAIL(SectionReaderInterface_dtor);
};

// Reads an archive's table
class ArchiveSectionReader : public SectionReader
{
public:
    Archive* archive;
    String path;

    void Destroy(u32 flags) RETAIL(FUN_002b6de0);
    // The table: a word, then per file its path's length, its path, its start and its size. The memory is freed
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(UnpackArchiveIntoMemory);
};
CHECK_SIZE(ArchiveSectionReader, 0x14);

// A section reader nothing makes (its vtable the 16 bytes past "cdrom0:\", D_00306E50): whatever the section, the current
// archive's files let go of (the archive emptied); its Missing the base's
class ArchiveFilesReleaser : public SectionReader
{
public:
    void Destroy(u32 flags) RETAIL(FUN_002b70d0);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002b7100);
};

extern "C"
{
    extern const GccVTableEntry g_SectionReaderVTable[] RETAIL(SectionReaderInterface_Methods);
    extern const GccVTableEntry g_ArchiveSectionReaderVTable[] RETAIL(ArchiveLoader_Methods);

    // Queues the readers of the archive's table (path.BH) and data (path.BD) in the storage, and reads them when asked to
    void ArchiveLoad(Archive* archive, const char* path, s32 readNow, GameReadersStorage* storage) RETAIL(FUN_002b55b0);
    // The file of the path (either case, without "HOST0:"), nullptr when there's none
    ArchiveEntry* ArchiveFind(Archive* archive, const char* path) RETAIL(FUN_002b7258);
    ArchiveFile* ArchiveSearch(Archive* archive, String* path) RETAIL(FUN_002b57e8);
    // Adds the file, or sets it when it's there. Returns whether it was. Both take the path's string and destroy it
    s32 ArchiveAddFile(Archive* archive, const ArchiveEntry* entry, String* path) RETAIL(LoadFileIntoMemory);
    void ArchiveInsertFile(Archive* archive, s32 index, const ArchiveEntry* entry, String* path) RETAIL(FUN_002b73e8);
    // Adds a copy of the string, which it destroys. Returns its index
    s32 StringListAdd(StringList* list, String* string) RETAIL(FUN_002b76d8);
}
