#pragma once

#include "common.h"
#include "game/math.h"
#include "game/string.h"

struct ChunkData;
struct ChunkList;
struct GameTimeController;

struct ChunkDataReference
{
    ChunkData* data;
    u32 value;
};

// A chunk link's flags and matrices (LinkMatricesRead). The chunk's data keeps a list of its links
struct ChunkLinkData
{
    enum Flags : u32
    {
        // Bits 7-13 nonzero: a keep link, which takes the linked chunk's RM2 along
        KeepMask = 0x3F80,
        InList = 0x10000,
        HasLinkedData = 0x20000,
        LinkedRm2Loaded = 0x40000,
        HasLoadWall = 0x80000,
    };

    u32 flags;
    // The linked chunk's data
    ChunkData* linkedData;
    ChunkLinkData* next;
    ChunkLinkData* previous;
    f32 objectMatrix[16];
    f32 chunkMatrix[16];
    void* loadWall;
    u8 unknown94[0xA0 - 0x94];
};
CHECK_SIZE(ChunkLinkData, 0xA0);

// Settings of 0x18 bytes the chunk starts with: a byte of 0xFF, two bits, and a scale of 1
struct ChunkDataSetting
{
    u32 bits;
    u32 unknown04;
    u32 unknown08;
    f32 scale;
    u32 unknown10;
    u32 unknown14;
};
CHECK_SIZE(ChunkDataSetting, 0x18);

// A chunk's data, read from its SM2: the scenery, the instances' contexts, the lights, the collision, the particles and the dynamic
// scenery of the chunk, made as they're read and let go of a part at a time when the chunk is released
struct ChunkData
{
    enum State : u32
    {
        // Bits 18-21
        StateMask = 0x3C0000,
        Shown = 0x40000,
        Releasing = 0x80000,
        Released = 0xC0000,
    };

    enum Bits : u32
    {
        UpdateDetail = 0x10000,
        HasLights = 0x20000,
        UpdateUnknown1D8 = 0x400000,
        // Its parts are released through the readers, a part at a time
        ReleaseInSteps = 0x1000000,
    };

    // Its own reference block, like a loader's
    ChunkDataReference* self;
    u8 unknown004[0xC];
    // Its last row is where the camera is (in the world) for its things' drawing
    Matrix4x4 matrix;
    Matrix4x4 unknown050;
    // What its instances' matrices are drawn through (times it)
    Matrix4x4 drawMatrix;
    Matrix4x4 unknown0D0;
    // Where its view is for the frame's particles (the platform's, -1 none yet: loaded by the first of its particles to need it)
    s32 particleView;
    // The frame it was last drawn in (g_RenderedFrames)
    u32 drawnFrame;
    u8 unknown118[0x8];
    // The game reads it with the next word as 64 bits
    u32 bits;
    // The RM2 loaders that loaded it, and the releases under way
    u32 rm2Loads;
    ChunkList* list;
    String name;
    // The path, emptied once it's released
    String path;
    // The scenery: its vtable is at 0x44
    u8* scenery;
    // The chunk list's links
    ChunkData* next;
    ChunkData* previous;
    ChunkLinkData* links;
    void* clocks;
    // What its depth in the links lets it show: -1 everything (the focus chunk and its neighbours), 8 at a depth of 2, 0 beyond
    s32 detail;
    struct ChunkInstances* instances;
    // The cells its awake instances' collision is sorted into over its scenery (0x301 words, made with the first)
    u32* instanceCells;
    void* unknown164;
    ChunkDataSetting setting168;
    ChunkDataSetting setting180;
    // The boxes of its triggers of kind 0 the sound code tests the player against (up to 7)
    void* soundBoxes[8];
    u32 soundBoxCount;
    void* collision;
    struct ChunkLights* lights;
    String unknown1C4;
    u32 unknown1D0;
    // Its sky (released by its destructor when bit 16 is set)
    struct Sky* sky;
    void* unknown1D8;
    void* unknown1DC;
    // The particles' section, -1 for none
    s32 particles;
    void* dynamicScenery;
    // The colour filter's palette while the character played is in the chunk
    u32 colourFilterPalette;
    u8 unknown1EC;
    u8 unknown1ED[3];

    // A box added unless it has 7. Whether it was
    static u32 AddSoundBox(ChunkData* chunk, void* box) RETAIL(FUN_001f25c0);
    // An instance put in the chunk (not when it's in one): an awake one's collision sorted into the chunk's cells, then the
    // chunk's instances told. Whether it was
    static u32 AddInstance(ChunkData* chunk, struct InstanceContext* instance) RETAIL(FUN_001f20d0);
};
CHECK_SIZE(ChunkData, 0x1F0);
CHECK_OFFSET(ChunkData, bits, 0x120);
CHECK_OFFSET(ChunkData, path, 0x138);
CHECK_OFFSET(ChunkData, scenery, 0x144);
CHECK_OFFSET(ChunkData, detail, 0x158);
CHECK_OFFSET(ChunkData, collision, 0x1BC);
CHECK_OFFSET(ChunkData, particles, 0x1E0);

extern "C"
{
    // The chunk whose things are being drawn (the renderer finds the camera in it)
    extern ChunkData* g_DrawnChunk RETAIL(G_UnkChunkData_);
    // A matrix moved through a link: times its object matrix (only its turn when not whole)
    void TransformThroughLink(const ChunkLinkData* link, Matrix4x4* matrix, u32 whole) RETAIL(FUN_001ea138);
    // A vector moved through a link the same way (a point when whole, else turned only), and a rotation turned by its object
    // matrix's turn (still asm)
    void TransformVectorThroughLink(const ChunkLinkData* link, Vector4* vector, u32 whole) RETAIL(FUN_001ea1f0);
    void TransformRotationThroughLink(const ChunkLinkData* link, Vector4* rotation) RETAIL(FUN_001f01d0);
    ChunkData* ConstructChunkData(ChunkData* data, ChunkList* list, const char* path) RETAIL(CreateChunkData);
    void DestroyChunkData(ChunkData* data, u32 flags) RETAIL(FUN_001ecfc0);
    // Starts letting the data go: now (its parts destroyed at once) or in steps through the readers. Returns whether it started
    u32 ReleaseChunkData(ChunkData* data, bool unload, bool start, bool inSteps) RETAIL(FUN_001ee130);
    // The data's frame: a shown chunk's clocks and instances move on, a released one lets go of its parts. Returns whether it's
    // still being let go
    u32 ChunkDataReleasing(ChunkData* data, bool paused, GameTimeController* clock) RETAIL(FUN_001edbd8);
    // Queues the release of one of its parts (by number) through the readers
    void QueueChunkPartRelease(ChunkData* data, u32 part) RETAIL(FUN_001ecd30);
    void ReleaseChunkScenery(ChunkData* data, bool unknown, bool destroy, bool tell) RETAIL(FUN_001ecc60);

    // Its parts, made when it has none
    struct ChunkLights* ChunkDataLights(ChunkData* data) RETAIL(FUN_001f1b60);
    void* ChunkDataDynamicScenery(ChunkData* data) RETAIL(FUN_001f1bc8);
    struct ChunkInstances* ChunkDataContext(ChunkData* data) RETAIL(FUN_001f1ca8);
    void* ChunkDataUnknown(ChunkData* data, s32 count) RETAIL(FUN_001f1cf0);
    void* ChunkDataCollision(ChunkData* data) RETAIL(FUN_001f1dc0);
    // And destroyed
    void DestroyChunkClocks(ChunkData* data) RETAIL(FUN_001f1b28);
    void DestroyChunkLights(ChunkData* data) RETAIL(FUN_001f1c10);
    void DestroyChunkDynamicScenery(ChunkData* data) RETAIL(FUN_001f1c70);
    void DestroyChunkUnknown1DC(ChunkData* data) RETAIL(FUN_001f1d40);
    void DestroyChunkParticles(ChunkData* data) RETAIL(FUN_001f1d78);
    void DestroyChunkCollision(ChunkData* data) RETAIL(FUN_001f1e08);
    void DestroyChunkUnknown164(ChunkData* data) RETAIL(FUN_001f24c8);

    // Points the reference at the data (its own reference block counted), letting go of what it had
    ChunkDataReference** AssignChunkData(ChunkDataReference** reference, ChunkData* data) RETAIL(FUN_00101248);
    // One count less: the data goes with the last owning reference, the reference with the last one
    void ReleaseChunkDataReference(ChunkDataReference** reference);

    // The list of the chunks' data
    ChunkList* GetChunkList() RETAIL(GetChunkListData_);
    void DestroyChunkList(ChunkList* list, u32 flags) RETAIL(FUN_001ec968);
    // Every chunk's frame (the current chunk's first): the released ones leave the list and are destroyed
    void UpdateChunks(ChunkList* list, bool paused, GameTimeController* clock) RETAIL(FUN_001ecae0);
    ChunkList* InitChunkList(ChunkList* list) RETAIL(InitChunkListData_);
    void ChunkListAdd(ChunkList* list, ChunkData* data) RETAIL(FUN_001f1648);
    // Takes the data out, and every chunk's links to it
    void ChunkListRemove(ChunkList* list, ChunkData* data) RETAIL(FUN_001f1678);
    // The data of the path, nullptr for none
    ChunkData* FindChunkData(ChunkList* list, String* path) RETAIL(GetChunkData_);
    // A link of the chunk goes in its list (its linked data the chunk's own until the SM2's loader sets it) or out of it
    void ChunkDataLink(ChunkData* data, ChunkLinkData* link) RETAIL(FUN_001f18a0);
    void ChunkDataUnlink(ChunkData* data, ChunkLinkData* link) RETAIL(FUN_001f1908);
    // The chunk's links to the data are taken out
    void ChunkDataForgetLinksTo(ChunkData* data, ChunkData* gone) RETAIL(FUN_001f1a80);

    // The lists' templates (the last two arguments are GCC's pointers to the members, unused)
    void ChunkListPushFront(ChunkData* data, ChunkData** head, s32 previous, s32 next) RETAIL(FUN_001f4198);
    void ChunkListTakeOut(ChunkData* data, ChunkData** head, s32 previous, s32 next) RETAIL(UnloadChunk_);
    void LinkListPushFrontData(ChunkLinkData* link, ChunkLinkData** head, s32 previous, s32 next) RETAIL(FUN_001f41d8);
    void LinkListRemoveData(ChunkLinkData* link, ChunkLinkData** head, s32 previous, s32 next) RETAIL(FUN_001f4218);
}

// The chunks' data, in a list made the first time it's asked for
struct ChunkList
{
    ChunkData* first;
    // Updated before the others
    ChunkDataReference* current;
};
CHECK_SIZE(ChunkList, 0x8);
