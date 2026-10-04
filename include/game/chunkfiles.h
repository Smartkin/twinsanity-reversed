#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/graphicstables.h"
#include "game/readers.h"
#include "game/string.h"

class MemoryStream;
struct SoundTable;
class Sm2Loader;
struct ChunkData;
struct ChunkEntry;
struct InstanceSectionItem;
struct LayoutInstances;

// A reader of a code section's kind of resources (the retail item readers, 0xC bytes; their vtable functions: 1 the destructor, 2
// how many resources the table has, 3 (1), 4 whether it reads a section's type (1), 5 a section reader for a resource the table
// hasn't got, 6 the table cleared, 7 and 8 the table told how many are coming and how many were read): the table it adds to, and
// the objects' the chunk's list of the object IDs it brings
struct CodeKindReader
{
    const GccVTableEntry* vtable;
    struct ResourceTable* table;
    void* objects;
};
CHECK_SIZE(CodeKindReader, 0xC);

// The section reader of a resource of a code kind (0x14 bytes, each kind's vtable): the resource's ID, its item's index, the table
// and the reader's object IDs
struct ResourceSectionReader
{
    const GccVTableEntry* vtable;
    u32 id;
    s32 index;
    struct ResourceTable* table;
    void* objects;
};
CHECK_SIZE(ResourceSectionReader, 0x14);

// The RM2's code section item: the game's resources, and a reader of each kind: the objects, the scripts, the animations, the
// models (OGIs), the code models, the sounds and the voices of each language (6, none past the languages)
struct CodeItem : ItemInterface
{
    struct GameResources* resources;
    CodeKindReader objects;
    CodeKindReader behaviours;
    CodeKindReader animations;
    CodeKindReader models;
    CodeKindReader codeModels;
    CodeKindReader sounds;
    CodeKindReader voices[6];

    static CodeItem* Construct(CodeItem* item, struct GameResources* resources, u32* objects) RETAIL(InitCodeItem);
    // Every reader made a plain item again, and freed when asked
    void Unload(u32 destroyFlags) RETAIL(UnloadCode);
    // Its vtable's slots 2 (13 subsections), 3 (1), 4 (whether it reads a section's type) and 6 (every resource let go of)
    s32 SectionCount() RETAIL(GetSubSectionsAmount_0026A938);
    s32 Unknown3() RETAIL(FUN_0026a940);
    bool CanRead(u32 type) RETAIL(FUN_0026a948);
    void ReleaseResources() RETAIL(FUN_0026a958);
    // A subsection queued for its kind's reader (nothing for kind 5 and past the kinds)
    void QueueSubsection(u32 kind, u32 start) RETAIL(LoadCodeSubSection);
};
CHECK_SIZE(CodeItem, 0x98);

// The section reader of a code section's subsection (0x10 bytes, vtable CodeSubSectionReader_Methods): the subsection's kind
// (0-4 the objects, scripts, animations, models and code models, 5 none, 6 the sounds, 7-12 each language's voices), where it
// starts and the item
struct CodeSubsectionReader : SectionReader
{
    u32 kind;
    u32 start;
    CodeItem* item;

    void Destroy(u32 flags) RETAIL(FUN_00268408);
    // The subsection queued for its kind's reader
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadItem);
};
CHECK_SIZE(CodeSubsectionReader, 0x10);

// The item a section of sounds is read through into a sound table: readers for the sounds the table hasn't got,
// the samples of the sounds read queued once the section's read
struct SoundTableItem : ItemInterface
{
    SoundTable* table;
    u32 unknown08;
};
CHECK_SIZE(SoundTableItem, 0xC);

extern "C"
{
    extern const GccVTableEntry g_SoundTableItemVTable[] RETAIL(CodeSfxItem_Methods);
    // Set when the first RM2 is queued, never cleared: the code section's readers then read the RM2's layout, an older one before
    extern u8 g_Rm2Queued RETAIL(D_0030A230);
}

// The SM2's item: reads the scenery, the dynamic scenery and the links into the chunk's data, and queues the graphics
class Sm2Reader : public ItemInterface
{
public:
    String path;
    // Cleared once the file is read
    u8 reading;
    u8 unknown11[3];
    Sm2Loader* loader;
    GraphicsItem graphics;

    static Sm2Reader* Construct(Sm2Reader* reader, Sm2Loader* loader) RETAIL(CreateSM2);
    void Destroy(u32 flags) RETAIL(FUN_001f5950);
    s32 SectionCount() RETAIL(GetSubSectionsAmount_SM2);
    s32 Unknown3() RETAIL(FUN_001f5cb8);
    bool CanRead(u32 type) RETAIL(FUN_001f5cc0);
    SectionReader* GetReader(s32 index, ItemHeader* header, s32* size) RETAIL(GetSm2ItemSectionReader);
    void Unload() RETAIL(FUN_001f5db8);
    void Finish(s32 read, u32 count, u32 end) RETAIL(FUN_001f5d50);
    // Queues the file's reading (queue) and reads everything queued (now)
    void Queue(bool now, bool queue) RETAIL(FUN_001f5c30);
    // The path of another of the chunk's files, its kind's extension added to the SM2's path (0 ".sn", 2 ".lvl", 3 ".lgt", 4
    // ".sca", 5 ".lk", none for the others; nothing calls it)
    void FilePath(u32 kind, String* file) RETAIL(FUN_001f5ae8);
};
CHECK_SIZE(Sm2Reader, 0x88);

// The RM2's item: the code (objects, scripts, animations, sounds), the graphics and the instances' sections
class Rm2Reader : public ItemInterface
{
public:
    // Bit 0: the graphics finish without registering, 1: still reading
    u32 bits;
    String path;
    struct GameResources* resources;
    // The object IDs the RM2 brought (a count, then the IDs)
    u32* objects;
    struct CollisionData** collision;
    GraphicsItem graphics;
    CodeItem code;
    // Each layout's instances (sections 0-7) and the item reading them
    LayoutInstances* layouts[8];
    InstanceSectionItem* instanceItems[8];
    ChunkEntry* entry;

    static Rm2Reader* Construct(Rm2Reader* reader, ChunkEntry* entry) RETAIL(CreateRM2);
    // The default chunk's (of no chunk entry, its graphics finishing without registering)
    static Rm2Reader* ConstructDefault(Rm2Reader* reader, const char* path, struct GameResources* resources, u32* objects)
        RETAIL(CreateDefaultRM2);
    void Destroy(u32 flags) RETAIL(DestroyRM2);
    s32 SectionCount() RETAIL(GetSubSectionsAmount_RM2);
    s32 Unknown3() RETAIL(FUN_0026a348);
    bool CanRead(u32 type) RETAIL(FUN_0026a350);
    SectionReader* GetReader(s32 index, ItemHeader* header, s32* size) RETAIL(GetRm2ItemSectionReader);
    void Unload() RETAIL(FUN_0026a490);
    void Finish(s32 read, u32 count, u32 end) RETAIL(FUN_0026a418);
    void Queue(bool now, bool queue) RETAIL(AddRm2ToLoadQueue);
    // A section read (its ID, where it was in the file): 0-7 a layout's instances made and queued, 8 the particles read, 9 the
    // collision, 10 the code and 11 the graphics queued
    void ReadSection(u32 id, u32 offset, MemoryStream* stream) RETAIL(ReadRM2);
    // Every layout's instances let go of and destroyed, with the items reading them
    void DestroyInstances() RETAIL(FUN_0026a4f0);
    // The path of another of the chunk's files, its kind's extension added to the RM2's path (0 ".ma2", 1 ".mc2", 2 ".me2", 3
    // ".ga2", 4 ".gc2", 5 ".ge2", 6 ".gw2", 7 ".su2", 8 ".ptl", 9 ".tri", none for the others; nothing calls it)
    void FilePath(u32 kind, String* file) RETAIL(FUN_0026a070);
};
CHECK_SIZE(Rm2Reader, 0x16C);

// A section of a chunk's file: its ID and where it was in the file, read by its item
class ChunkSectionReader : public SectionReader
{
public:
    u32 id;
    u32 offset;
    ItemInterface* item;
};
CHECK_SIZE(ChunkSectionReader, 0x10);

class Sm2SectionReader : public ChunkSectionReader
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001f5920);
    // 0 the scenery, 4 the dynamic scenery, 5 the links, 6 the graphics (queued)
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadSM2);
};

class Rm2SectionReader : public ChunkSectionReader
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00268588);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadRM2);
};

// The scenery's section, copied into the disk manager and read from there; a reader of nothing queued after it lets it go
class ScenerySectionReader : public SectionReader
{
public:
    MemoryStream* stream;
    s32 node;

    static ScenerySectionReader* Construct(ScenerySectionReader* reader, const u8* data, u32 size) RETAIL(FUN_002acca8);
    void Destroy(u32 flags) RETAIL(FUN_002acd70);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002ace10);
    void Missing(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002ace18);
};
CHECK_SIZE(ScenerySectionReader, 0xC);
