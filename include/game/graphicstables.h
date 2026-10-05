#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/readers.h"
#include "game/resources.h"
#include "game/string.h"

class Stream;
struct Material;
struct GameTexture;
struct RigidModelData;
struct RigidModel;
struct Skin;
struct BlendSkin;
struct Lod;
struct Sky;

// A material resource: the platform's material after the header, and a byte its constructor clears (never read)
struct MaterialResource
{
    ResourceHeader header;
    Material* material;
    u8 unused0C;
};
CHECK_SIZE(MaterialResource, 0x10);

// The kinds of graphics resources the chunks share, each with a table of its own: the textures, the materials, the models (a
// rigid model's submodels), the rigid models the OGIs draw, the skins, the blend skins, the meshes (rigid models too, the ones
// the scenery's LODs and skies draw), the LODs and the skies
struct TextureKind
{
    using Item = GameTexture;
};

struct MaterialKind
{
    using Item = MaterialResource;
};

struct ModelKind
{
    using Item = RigidModelData;
};

struct RigidModelKind
{
    using Item = RigidModel;
};

struct SkinKind
{
    using Item = Skin;
};

struct BlendSkinKind
{
    using Item = BlendSkin;
};

struct MeshKind
{
    using Item = RigidModel;
};

struct LodKind
{
    using Item = Lod;
};

struct SkyKind
{
    using Item = Sky;
};

// The IDs of a kind's resources a chunk being read took references of (0x800 bytes: how many, then the IDs). The references go
// once the chunk is read
struct PendingIds
{
    u32 count;
    u32 ids[0x1FF];
};
CHECK_SIZE(PendingIds, 0x800);

// A table of a kind's resources (static, 0x24 bytes): its resources by ID (an array of GCC 2.9x's array new, sorted by ID,
// largest first, searched by halves), how many there are and room for, how much room it grows by (0x40), the IDs a chunk being
// read took references of, the queue the resources it lets go of wait in to be deleted (none: they're deleted right away), its
// vtable and its name (the folder its resources would load from). Each kind is three classes (the tables' base, one with the name, the kind's), and their vtables have the same
// functions but the first two:
//
// 1. a resource made (with its ID) handed to the kind (the skies' load it, the others' do nothing),
// 2. the destructor,
// 3. a resource added under an ID with a reference taken,
// 4. the resource of an ID with a reference taken, made when the table hasn't got it,
// 5. a reference of an ID's resource let go of, 6. a resource's,
// 7. whether there's a resource of an ID, and 8. the resource of an ID (none: nullptr).
template <typename Kind>
struct GraphicsTable
{
    using Item = typename Kind::Item;

    struct Entry
    {
        Item* item;
        u32 id;
    };

    enum Slots : u32
    {
        MadeSlot = 1,
        DestroySlot = 2,
        AddReferenceSlot = 3,
        AcquireSlot = 4,
        ReleaseSlot = 5,
        ReleaseItemSlot = 6,
        ExistsSlot = 7,
        GetSlot = 8,
    };

    Entry* entries;
    u16 count;
    u16 capacity;
    u16 growth;
    u16 unused0A;
    PendingIds* pending;
    DeletionQueue* queue;
    const GccVTableEntry* vtable;
    String name;

    // The entry of an ID (none: nullptr)
    Entry* Find(u32 id);
    // A resource put in under an ID, in place of the one there is. Whether there was one
    s32 Insert(Item* const* item, u32 id);
    // A resource put in at an index, the table grown by its growth when it's full
    void InsertAt(s32 index, Item* const* item, u32 id);
    // Every resource's references and bits 16 and 17 cleared and the resource deleted, the array freed and every resource in the
    // queue deleted
    void ReleaseAll();

    // The vtable's functions 3 to 8
    s32 AddReference(const u32* id, Item* item);
    Item* Acquire(const u32* id, bool* created);
    void Release(const u32* id);
    void ReleaseItem(Item* item);
    bool Exists(const u32* id);
    Item* Get(const u32* id);

    // The destructors: the kind's (its vtable stored first), the class with the name's and the base's
    void Destroy(u32 flags);
    void DestroyNamed(u32 flags);
    void DestroyBase(u32 flags);

    // The static table made: no resources, growing by 0x40, named
    void Construct(const char* folder);
    // The references of the pending IDs let go of, and the list forgotten
    void ReleasePending();

private:
    // Where the search by halves for an ID ends: the ID's entry, or where it gave up
    s32 Search(u32 id) const;
    // A reference let go of. Whether the resource is still held (it has references or it's kept)
    static bool DropReference(Item* item);
    // A resource without references taken out of the table, and queued or deleted
    void Forget(Item* item, u32 id);
};

using TextureTable = GraphicsTable<TextureKind>;
using MaterialTable = GraphicsTable<MaterialKind>;
using ModelTable = GraphicsTable<ModelKind>;
using RigidModelTable = GraphicsTable<RigidModelKind>;
using SkinTable = GraphicsTable<SkinKind>;
using BlendSkinTable = GraphicsTable<BlendSkinKind>;
using MeshTable = GraphicsTable<MeshKind>;
using LodTable = GraphicsTable<LodKind>;
using SkyTable = GraphicsTable<SkyKind>;
CHECK_OFFSET(TextureTable, pending, 0xC);
CHECK_OFFSET(TextureTable, queue, 0x10);
CHECK_OFFSET(TextureTable, vtable, 0x14);
CHECK_SIZE(TextureTable, 0x24);

// A reader of a kind's subsection of a chunk's graphics section into its table (8 bytes; its vtable functions: 1 the destructor,
// which lets go of the references the reading took, 2 how many resources the table has, 3 the section type it reads
// (GraphicsKindSectionType), 4 whether it reads a section's type, 5 a section reader of a resource the table hasn't got, made with
// a reference for the reading (one it has gets the reference instead), 6 every resource of the table released and deleted, 7 and 8
// nothing)
template <typename Kind>
struct GraphicsKindReader : ItemInterface
{
    GraphicsTable<Kind>* table;

    void Destroy(u32 flags);
    // The destructor's body, which the graphics item's has inline
    void Unload();
    u32 Count();
    SectionReader* GetReader(s32 index, ItemHeader* header, s32* size);
    void Clear();
};

// The section reader of a resource of a kind (0xC bytes): it reads the resource it was made with and puts it in the table
template <typename Kind>
struct GraphicsResourceReader : SectionReader
{
    typename Kind::Item* item;
    GraphicsTable<Kind>* table;

    void Destroy(u32 flags);
    void Read(u8* data, u32 size, ReaderStack* readers);
};

// What a chunk file's graphics section is read into (0x70 bytes): a reader of each kind, and the lists of the IDs each kind's
// reading took references of, in the order of the subsections (textures, materials, models, rigid models, skins, blend skins,
// meshes, LODs and skies)
struct GraphicsItem : ItemInterface
{
    // The subsections' IDs (TT Lab's GRAPHICS_*_SECTION)
    enum Subsections : u32
    {
        TextureSubsection,
        MaterialSubsection,
        ModelSubsection,
        RigidModelSubsection,
        SkinSubsection,
        BlendSkinSubsection,
        MeshSubsection,
        LodSubsection,
        SkySubsection,
        SubsectionCount,
    };

    GraphicsKindReader<MaterialKind> materials;
    GraphicsKindReader<TextureKind> textures;
    GraphicsKindReader<ModelKind> models;
    GraphicsKindReader<RigidModelKind> rigidModels;
    GraphicsKindReader<SkinKind> skins;
    GraphicsKindReader<BlendSkinKind> blendSkins;
    GraphicsKindReader<SkyKind> skies;
    GraphicsKindReader<MeshKind> meshes;
    GraphicsKindReader<LodKind> lods;
    PendingIds* pending[SubsectionCount];
};
CHECK_OFFSET(GraphicsItem, lods, 0x44);
CHECK_SIZE(GraphicsItem, 0x70);

// The section reader of a subsection of the graphics section (0x10 bytes): the subsection's kind and place, and the item. Reading
// it queues the subsection's table for the kind's reader
struct GraphicsSubsectionReader : SectionReader
{
    u32 kind;
    u32 start;
    GraphicsItem* item;
};
CHECK_SIZE(GraphicsSubsectionReader, 0x10);

extern "C"
{
    extern TextureTable g_TextureTable RETAIL(G_TexturesTable_);
    extern MaterialTable g_MaterialTable RETAIL(G_MaterialsTables_);
    extern ModelTable g_ModelTable RETAIL(G_ModelsTable_);
    extern RigidModelTable g_RigidModelTable RETAIL(G_RigidModelsTable_);
    extern SkinTable g_SkinTable RETAIL(G_SkinsTable_);
    extern BlendSkinTable g_BlendSkinTable RETAIL(G_BlendSkinsTable_);
    extern MeshTable g_MeshTable RETAIL(G_MeshTable);
    extern LodTable g_LodTable RETAIL(G_LODsTable_);
    extern SkyTable g_SkyTable RETAIL(G_SkydomeTable_);

    // The static tables made (GCC 2.9x's static initialisation: when initialise is 1 and the priority 0xFFFF). They're never
    // destroyed
    void InitGraphicsResourceTables(s32 initialise, s32 priority) RETAIL(InitGraphicsResourceTables);
    // The static constructor that runs it
    void GraphicsTablesStaticInit() RETAIL(FUN_001c75a0);

    // The graphics item: its constructor and destructor (the references the reading took let go of first), and its vtable's
    // functions 2 to 7 (9 subsections, the section type it reads, whether it reads a section's type, a reader of a subsection,
    // every table's resources released and deleted (the skies' first, the textures' last, the item unused), and the reading
    // started)
    GraphicsItem* InitGraphicsItem(GraphicsItem* item) RETAIL(InitGraphicsItem);
    void UnloadGraphics(GraphicsItem* item, u32 flags) RETAIL(UnloadGraphics);
    u32 GraphicsItemCount(GraphicsItem* item) RETAIL(GetSubSectionsAmount);
    u32 GraphicsItemSectionType(GraphicsItem* item) RETAIL(FUN_001a2330);
    bool GraphicsItemCanRead(GraphicsItem* item, u32 type) RETAIL(FUN_001a2338);
    SectionReader* GetGraphicsSectionReader(GraphicsItem* item, s32 index, ItemHeader* header, s32* size)
        RETAIL(GetGraphicsSectionReader);
    void ReleaseGraphicsResources(GraphicsItem* item) RETAIL(FUN_001a2348);
    void GraphicsItemSetCount(GraphicsItem* item, u32 count) RETAIL(FUN_001a2308);
    // The reading started: each kind's table records the IDs it takes references of in a new list of the item's
    void StartGraphicsReading(GraphicsItem* item) RETAIL(FUN_0019efb0);
    // The reading done: every table's pending references let go of (the materials' first, the LODs' last) and the item's lists
    // freed
    void FinishGraphicsReading(GraphicsItem* item) RETAIL(FUN_0019f0c8);
    // Every table's list forgotten, its references kept
    void ForgetGraphicsReading(GraphicsItem* item) RETAIL(FUN_0019f4e8);
    // The subsection reader's destructor and its reading: the subsection of a kind queued for its reader
    void DestroyGraphicsSubsectionReader(GraphicsSubsectionReader* reader, u32 flags) RETAIL(FUN_001a0978);
    void LoadGraphicsSection(GraphicsSubsectionReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadGraphicsSection);
    void LoadGraphicsSubSection(GraphicsItem* item, u32 kind, u32 start) RETAIL(LoadGraphicsSubSection);

    // A reference of a resource let go of through its table, or the resource deleted when it has no ID (it was made outside the
    // tables)
    void ReleaseTexture(GameTexture* texture) RETAIL(FUN_001c11d0);
    void ReleaseMaterial(MaterialResource* material) RETAIL(FUN_001bc810);
    void ReleaseModel(RigidModelData* model) RETAIL(FUN_001bd1b0);
    void ReleaseRigidModel(RigidModel* model) RETAIL(FUN_001c14d8);
    void ReleaseSkin(Skin* skin) RETAIL(FUN_001c1cb0);
    void ReleaseBlendSkin(BlendSkin* blendSkin) RETAIL(FUN_001c1770);
    void ReleaseMesh(RigidModel* mesh) RETAIL(FUN_001c1ed0);
    void ReleaseLod(Lod* lod) RETAIL(FUN_001bf8b0);
    void ReleaseSky(Sky* sky) RETAIL(FUN_001c0680);
}

// The resource of an ID from its table, or made with a reference and read from the stream when the table hasn't got it (the
// game's inline code)
GameTexture* TextureFromStream(u32 id, Stream* stream);
MaterialResource* MaterialFromStream(u32 id, Stream* stream);
