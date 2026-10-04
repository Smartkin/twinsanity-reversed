#pragma once

#include "common.h"
#include "game/math.h"
#include "game/string.h"

struct ChunkData;
struct ChunkList;
struct ChunkShadows;
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
    struct LoadWall* loadWall;
    u8 unknown94[0xA0 - 0x94];
};
CHECK_SIZE(ChunkLinkData, 0xA0);

// A link's load wall (0x90 bytes): its corners, its plane and its edges' planes (inward) as a matrix's columns
struct alignas(16) LoadWall
{
    Vector4 corners[4];
    Vector4 plane;
    Matrix4x4 edges;
};
CHECK_SIZE(LoadWall, 0x90);

// Settings of 0x18 bytes the chunk starts with: a byte of 0xFF, two bits, and a scale of 1
struct ReverbSettings
{
    // The low byte: the reverb's type (0 to 6, 0xFF none)
    u32 bits;
    f32 delay;
    f32 feedback;
    f32 depth;
    u32 unknown10;
    u32 unknown14;
};
CHECK_SIZE(ReverbSettings, 0x18);

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
        UpdateWind = 0x400000,
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
    // The scenery's root
    struct SceneryCell* scenery;
    // The chunk list's links
    ChunkData* next;
    ChunkData* previous;
    ChunkLinkData* links;
    void* clocks;
    // What its depth in the links lets it show: -1 everything (the focus chunk and its neighbours), 8 at a depth of 2, 0 beyond
    s32 detail;
    struct ChunkInstances* instances;
    // The cells its awake instances' collision is sorted into over its scenery (0x301 words, made with the first)
    struct InstanceContext** instanceCells;
    void* unknown164;
    // Its reverb, and the one while the sound's listener is in one of its sound boxes (its triggers of kind 0, up to 7)
    ReverbSettings reverb;
    ReverbSettings boxReverb;
    struct SoundBox* soundBoxes[8];
    u32 soundBoxCount;
    void* collision;
    struct ChunkLights* lights;
    String unknown1C4;
    // The ID of its sky
    u32 skyId;
    // Its sky (released by its destructor when bit 16 is set)
    struct Sky* sky;
    // Its wind (nothing makes one in retail)
    struct ChunkWind* wind;
    // The shadows cast in it (made the first time one is asked for)
    struct ChunkShadows* shadows;
    // The particles' section, -1 for none
    s32 particles;
    struct ChunkDynamicScenery* dynamicScenery;
    // The colour filter's palette while the character played is in the chunk
    u32 colourFilterPalette;
    // A byte of the scenery's, never read
    u8 unusedByte;
    u8 unknown1ED[3];

    // A box added unless it has 7. Whether it was
    static u32 AddSoundBox(ChunkData* chunk, struct SoundBox* box) RETAIL(FUN_001f25c0);
    // Whether a point is in one of its sound boxes
    u32 InSoundBox(const Vector4* point) RETAIL(FUN_001f25f8);
    // An instance put in the chunk (not when it's in one): an awake one's collision sorted into the chunk's cells, then the
    // chunk's instances told. Whether it was
    static u32 AddInstance(ChunkData* chunk, struct InstanceContext* instance) RETAIL(FUN_001f20d0);
};
CHECK_SIZE(ChunkData, 0x1F0);

// A chunk's wind, by its look (0x110 bytes, the chunk's to update and delete, which nothing makes in retail): the seconds it
// has blown, the way it blows on x and z (its angle about y swinging between 45 and 105 degrees once a minute), and the push of
// each of 16 phases of its sway (the way times the sway's cosine, the phases a 16th of a turn apart, 35 degrees a second; their
// y and w never set)
struct ChunkWind
{
    f32 seconds;
    f32 directionX;
    f32 directionZ;
    u32 unknown0C;
    Vector4 sway[16];
};
CHECK_SIZE(ChunkWind, 0x110);
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
    // matrix's turn
    void TransformVectorThroughLink(const ChunkLinkData* link, Vector4* vector, u32 whole) RETAIL(FUN_001ea1f0);
    void TransformRotationThroughLink(const ChunkLinkData* link, Vector4* rotation) RETAIL(FUN_001f01d0);
    // A link made (no data, links or wall), destroyed (its wall too) and read (its flags with bits 16-18 cleared, its object and
    // chunk matrices, its wall when bit 19 says so)
    ChunkLinkData* LinkMatricesConstruct(ChunkLinkData* data) RETAIL(FUN_001f00b8);
    void LinkMatricesDestroy(ChunkLinkData* data, u32 flags) RETAIL(FUN_001f00e0);
    void LinkMatricesRead(ChunkLinkData* data, class Stream* reader) RETAIL(FUN_001f0260);
    // A load wall read (its corners, the rest worked out from them), made from its corners, made, destroyed, and its corners'
    // middle
    LoadWall* LoadLoadWall(LoadWall* wall, class Stream* reader) RETAIL(LoadLoadWall);
    void ReadLoadWall(LoadWall* wall, class Stream* reader) RETAIL(FUN_001f1518);
    void BuildLoadWall(LoadWall* wall, const Vector4* corners) RETAIL(FUN_001ec470);
    LoadWall* LoadWallConstruct(LoadWall* wall) RETAIL(FUN_001f14c0);
    void LoadWallDestroy(LoadWall* wall, u32 flags) RETAIL(FUN_001f14f0);
    void WallMiddle(const Vector4* corners, Vector4* middle) RETAIL(FUN_001f1568);
    // Whether a point is in front of the wall (within 2) and inside its edges, and the planes a chunk is seen through it by
    u32 LoadWallHolds(const LoadWall* wall, const Vector4* point) RETAIL(FUN_001ec858);
    void PortalPlanes(const LoadWall* wall, Vector4* planes, const Vector4* eye) RETAIL(FUN_001ec6c8);
    // The chunk's reverb set from a command's arguments (its type byte, bits 8 and 9, delay, feedback, depth), and its box reverb
    void SetChunkReverb(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2500);
    void SetChunkBoxReverb(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2560);
    // The link of the chunk's that leads to another chunk (none)
    ChunkLinkData* FindLinkTo(ChunkData* chunk, ChunkData* linked) RETAIL(FUN_001f2450);
    // A chunk's wind blown on by the game clock (while it runs), and deleted
    void UpdateChunkWind(ChunkWind* wind, struct GameTimeController* time) RETAIL(FUN_0019f5e0);
    void DestroyChunkWind(ChunkWind* wind, u32 destroyFlags) RETAIL(FUN_001a2430);
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
    struct ChunkDynamicScenery* ChunkDataDynamicScenery(ChunkData* data) RETAIL(FUN_001f1bc8);
    struct ChunkInstances* ChunkDataContext(ChunkData* data) RETAIL(FUN_001f1ca8);
    ChunkShadows* ChunkShadowsOf(ChunkData* data, s32 count) RETAIL(FUN_001f1cf0);
    void* ChunkDataCollision(ChunkData* data) RETAIL(FUN_001f1dc0);
    // And destroyed
    void DestroyChunkClocks(ChunkData* data) RETAIL(FUN_001f1b28);
    void DestroyChunkLights(ChunkData* data) RETAIL(FUN_001f1c10);
    void DestroyChunkDynamicScenery(ChunkData* data) RETAIL(FUN_001f1c70);
    void DestroyChunkShadows(ChunkData* data) RETAIL(FUN_001f1d40);
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

// The data's own reference block (made the first time, its top bits kept) with one more reference
ChunkDataReference* AddChunkDataReference(ChunkData* data);

// The chunks' data, in a list made the first time it's asked for
struct ChunkList
{
    ChunkData* first;
    // Updated before the others
    ChunkDataReference* current;
};
CHECK_SIZE(ChunkList, 0x8);
