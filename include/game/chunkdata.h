#pragma once

#include "common.h"
#include "game/math.h"
#include "game/reference.h"
#include "game/string.h"

struct ChunkData;
struct ChunkList;
struct ChunkShadows;
struct GameTimeController;

// A reference to a chunk's data, like the objects' (game/reference.h): the data, and a count of 24 bits with flags above it
// (ReferenceBits: bit 24 owns the data, the last owning reference destroys it)
struct ChunkDataReference
{
    ChunkData* chunk;
    ReferenceBits bits;
};

// A chunk link's flags (ChunkLinkData's)
union ChunkLinkFlags
{
    u32 value;
    struct
    {
        // ChunkLinkData::Visibility: whether and how the linked chunk is drawn
        u32 visibility : 7;
        // Nonzero: a keep link, which takes the linked chunk's RM2 along (from the focus chunk) and, without a load wall,
        // instances across (TT Lab's KeepLoaded and IsLoadWallActive, the tools' bits 7 and 8)
        u32 keep : 7;
        // How much of its load wall was in view when it was last drawn (game/scenery.h's ChunkView::Visibility, the view's cell
        // test; a link without a wall, or always drawn, counts as wholly in view)
        u32 wallView : 2;
        // In its chunk's list of links
        u32 inList : 1;
        // The linked chunk's data is there, and its RM2 is loaded
        u32 hasLinkedData : 1;
        u32 linkedRm2Loaded : 1;
        // A load wall follows the matrices
        u32 hasLoadWall : 1;
        u32 unused20 : 12;
    };
};
CHECK_SIZE(ChunkLinkFlags, 4);

// A chunk link's flags and matrices (ReadChunkLinkData). The chunk's data keeps a list of its links
struct ChunkLinkData
{
    // How the linked chunk is drawn: not, always, or (any value from 2 up) while its load wall is in view, through the wall as a
    // portal while it's wholly in view
    enum Visibility : u32
    {
        VisibilityHidden = 0,
        VisibilityAlways = 1,
        VisibilityThroughWall = 2,
    };

    ChunkLinkFlags flags;
    // The linked chunk's data
    ChunkData* linkedData;
    ChunkLinkData* next;
    ChunkLinkData* previous;
    // What an object crossing the link is moved through, and where the linked chunk is drawn
    Matrix4x4 objectMatrix;
    Matrix4x4 chunkMatrix;
    struct LoadWall* loadWall;
    u8 unused94[0xA0 - 0x94];
};
CHECK_OFFSET(ChunkLinkData, objectMatrix, 0x10);
CHECK_OFFSET(ChunkLinkData, loadWall, 0x90);
CHECK_SIZE(ChunkLinkData, 0xA0);

// A link's load wall (0x90 bytes): its corners, its plane and its edges' planes (inward) as a matrix's columns
struct alignas(16) LoadWall
{
    Vector4 corners[4];
    Vector4 plane;
    Matrix4x4 edges;
};
CHECK_SIZE(LoadWall, 0x90);

// A reverb's type and bits (the low byte read by the sound code)
union ReverbBits
{
    u32 value;
    struct
    {
        // 0 to 6, ReverbSettings::NoReverb none
        u32 type : 8;
        // Set from the command's arguments, never read
        u32 unused8 : 1;
        u32 unused9 : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(ReverbBits, 4);

// A chunk's reverb (0x18 bytes, set by command 86, SetReverb): its type, its delay, feedback and depth. Made none, with a depth
// of 1
struct ReverbSettings
{
    static constexpr u8 NoReverb = 0xFF;

    ReverbBits bits;
    f32 delay;
    f32 feedback;
    f32 depth;
    u32 unused10;
    u32 unused14;
};
CHECK_SIZE(ReverbSettings, 0x18);

// A chunk's state (ChunkFlags::state)
enum ChunkState : u32
{
    // Its files aren't all loaded
    ChunkHidden = 0,
    ChunkShown = 1,
    // Its release has started: its parts are let go of, then it's released
    ChunkReleasing = 2,
    ChunkReleased = 3,
};

// A chunk's flags (ChunkData's). The scenery's reader sets the whole word from the SM2 (0x20000 or 0x30000 in retail)
union ChunkFlags
{
    u32 value;
    struct
    {
        // The low half of the frame its scenery was last drawn in (it's drawn once a frame)
        u32 drawStamp : 16;
        // A sky follows in the SM2 (the destructor releases it), and its lights do
        u32 hasSky : 1;
        u32 hasLights : 1;
        // ChunkState
        u32 state : 4;
        // It had a scenery root when it was last drawn (its wind only blows then)
        u32 hasRoot : 1;
        // Something of its scenery was drawn the last time (the shadows cast in it are drawn then)
        u32 drawn : 1;
        // Its parts are released through the readers, a part at a time
        u32 releasesInSteps : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(ChunkFlags, 4);

// A chunk's data, read from its SM2: the scenery, the instances' contexts, the lights, the collision, the particles and the dynamic
// scenery of the chunk, made as they're read and let go of a part at a time when the chunk is released
struct ChunkData
{
    // The parts of its release queued through the readers (QueueChunkPartRelease): a batch drops a hold first and adds it back
    // last, so its holds aren't 0 while the batch is under way
    enum Part : u32
    {
        PartAddHold = 0,
        PartDropHold = 1,
        PartCollision = 3,
        PartLights = 4,
        PartShadows = 5,
        PartParticles = 6,
        // The scenery's instances released, and the scenery released with its destruction queued
        PartSceneryInstances = 8,
        PartScenery = 9,
        PartClocks = 10,
    };

    static constexpr u32 MostSoundBoxes = 7;
    // The lists its instances' collision is sorted into (game/chunkscenery.cpp)
    static constexpr u32 InstanceCells = 0x301;
    static constexpr s32 NoParticles = -1;
    static constexpr s32 NoParticleView = -1;
    // Its steppedKinds near the focus chunk: every node kind
    static constexpr s32 EveryKindStepped = -1;

    // Its own reference block, like a loader's
    ChunkDataReference* self;
    u8 unused004[0xC];
    // Its last row is where the camera is (in the world) for its things' drawing
    Matrix4x4 matrix;
    // Its draw matrix to the screen
    Matrix4x4 toScreen;
    // What its instances' matrices are drawn through (times it), and its inverse
    Matrix4x4 drawMatrix;
    Matrix4x4 drawInverse;
    // Where its view is for the frame's particles (the platform's, NoParticleView none yet: loaded by the first of its particles
    // to need it)
    s32 particleView;
    // The frame it was last drawn in (g_RenderedFrames)
    u32 drawnFrame;
    u8 unused118[0x8];
    ChunkFlags flags;
    // Its release waits while this isn't 0: its RM2 while it's loaded, a release under way, the batches of parts queued (Part)
    u32 holds;
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
    struct TimeClock* clocks;
    // The kinds of its instances' nodes stepped (a bit per NodeKind): every kind in the focus chunk and the chunks linked to it,
    // the models' at a depth of 2, none beyond
    s32 steppedKinds;
    struct ChunkInstances* instances;
    // The lists its awake instances' collision is sorted into over its scenery (InstanceCells, made with the first)
    struct InstanceContext** instanceCells;
    struct ChunkRigidBodies* rigidBodies;
    // Its reverb, and the one while the sound's listener is in one of its sound boxes (its triggers of kind 0, up to
    // MostSoundBoxes)
    ReverbSettings reverb;
    ReverbSettings boxReverb;
    struct SoundBox* soundBoxes[8];
    u32 soundBoxCount;
    struct CollisionData* collision;
    struct ChunkLights* lights;
    String unused1C4;
    // The ID of its sky
    u32 skyId;
    // Its sky (released by its destructor when it has one)
    struct Sky* sky;
    // Its wind (nothing makes one in retail)
    struct ChunkWind* wind;
    // The shadows cast in it (made the first time one is asked for)
    struct ChunkShadows* shadows;
    // The particles' section, NoParticles for none
    s32 particles;
    struct ChunkDynamicScenery* dynamicScenery;
    // The colour filter's palette while the character played is in the chunk
    u32 colourFilterPalette;
    // A byte of the scenery's, never read
    u8 unusedByte;
    u8 unused1ED[3];

    // A box added unless it has MostSoundBoxes. Whether it was
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
    u32 unused0C;
    Vector4 sway[16];
};
CHECK_SIZE(ChunkWind, 0x110);
CHECK_OFFSET(ChunkData, toScreen, 0x50);
CHECK_OFFSET(ChunkData, drawInverse, 0xD0);
CHECK_OFFSET(ChunkData, flags, 0x120);
CHECK_OFFSET(ChunkData, path, 0x138);
CHECK_OFFSET(ChunkData, scenery, 0x144);
CHECK_OFFSET(ChunkData, steppedKinds, 0x158);
CHECK_OFFSET(ChunkData, rigidBodies, 0x164);
CHECK_OFFSET(ChunkData, collision, 0x1BC);
CHECK_OFFSET(ChunkData, unused1C4, 0x1C4);
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
    // A link made (no data, links or wall), destroyed (its wall too) and read (its flags, what the loading sets cleared, its
    // object and chunk matrices, its wall when the flags say so)
    ChunkLinkData* ConstructChunkLinkData(ChunkLinkData* link) RETAIL(FUN_001f00b8);
    void DestroyChunkLinkData(ChunkLinkData* link, u32 destroyFlags) RETAIL(FUN_001f00e0);
    void ReadChunkLinkData(ChunkLinkData* link, class Stream* reader) RETAIL(FUN_001f0260);
    // A load wall read (its corners, the rest worked out from them), made from its corners, made, destroyed, and its corners'
    // middle
    LoadWall* LoadLoadWall(LoadWall* wall, class Stream* reader) RETAIL(LoadLoadWall);
    void ReadLoadWall(LoadWall* wall, class Stream* reader) RETAIL(FUN_001f1518);
    void BuildLoadWall(LoadWall* wall, const Vector4* corners) RETAIL(FUN_001ec470);
    LoadWall* LoadWallConstruct(LoadWall* wall) RETAIL(FUN_001f14c0);
    void LoadWallDestroy(LoadWall* wall, u32 destroyFlags) RETAIL(FUN_001f14f0);
    void WallMiddle(const Vector4* corners, Vector4* middle) RETAIL(FUN_001f1568);
    // Whether a point is in front of the wall (within 2) and inside its edges, and the planes a chunk is seen through it by
    u32 LoadWallHolds(const LoadWall* wall, const Vector4* point) RETAIL(FUN_001ec858);
    void PortalPlanes(const LoadWall* wall, Vector4* planes, const Vector4* eye) RETAIL(FUN_001ec6c8);
    // The chunk's reverb set from a command's arguments (ReverbSettings' first four words: its type and bits, delay, feedback,
    // depth), and its box reverb
    void SetChunkReverb(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2500);
    void SetChunkBoxReverb(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2560);
    // The link of the chunk's that leads to another chunk (none)
    ChunkLinkData* FindLinkTo(ChunkData* chunk, ChunkData* linked) RETAIL(FUN_001f2450);
    // A chunk's wind blown on by the game clock (while it runs), and deleted
    void UpdateChunkWind(ChunkWind* wind, struct GameTimeController* time) RETAIL(FUN_0019f5e0);
    void DestroyChunkWind(ChunkWind* wind, u32 destroyFlags) RETAIL(FUN_001a2430);
    ChunkData* ConstructChunkData(ChunkData* chunk, ChunkList* list, const char* path) RETAIL(CreateChunkData);
    void DestroyChunkData(ChunkData* chunk, u32 destroyFlags) RETAIL(FUN_001ecfc0);
    // Starts letting the data go, now (its parts destroyed at once) or in steps through the readers: the whole chunk (start, its
    // release under way) or, when unload is set, what its RM2 made. Returns whether it started
    u32 ReleaseChunkData(ChunkData* chunk, bool unload, bool start, bool inSteps) RETAIL(FUN_001ee130);
    // The data's frame: a shown chunk's clocks and instances move on, a released one lets go of its parts. Returns 0 once it's
    // released and nothing holds it (it can be destroyed)
    u32 StepChunkData(ChunkData* chunk, bool paused, GameTimeController* clock) RETAIL(FUN_001edbd8);
    // Queues the release of one of its parts (ChunkData::Part) through the readers
    void QueueChunkPartRelease(ChunkData* chunk, u32 part) RETAIL(FUN_001ecd30);
    // The scenery let go of: destroyed now (destroy), or its instances released (releaseInstances: by a filter of every
    // instance, then the chunk's instances); in steps through its own release, which queues its destruction when destroy is set
    void ReleaseChunkScenery(ChunkData* chunk, bool releaseInstances, bool destroy, bool inSteps) RETAIL(FUN_001ecc60);

    // Its parts, made when it has none
    struct ChunkLights* ChunkDataLights(ChunkData* chunk) RETAIL(FUN_001f1b60);
    struct ChunkDynamicScenery* ChunkDataDynamicScenery(ChunkData* chunk) RETAIL(FUN_001f1bc8);
    struct ChunkInstances* ChunkDataInstances(ChunkData* chunk) RETAIL(FUN_001f1ca8);
    ChunkShadows* ChunkShadowsOf(ChunkData* chunk, s32 count) RETAIL(FUN_001f1cf0);
    void* ChunkDataCollision(ChunkData* chunk) RETAIL(FUN_001f1dc0);
    // And destroyed
    void DestroyChunkClocks(ChunkData* chunk) RETAIL(FUN_001f1b28);
    void DestroyChunkLights(ChunkData* chunk) RETAIL(FUN_001f1c10);
    void DestroyChunkDynamicScenery(ChunkData* chunk) RETAIL(FUN_001f1c70);
    void DestroyChunkShadows(ChunkData* chunk) RETAIL(FUN_001f1d40);
    void DestroyChunkParticles(ChunkData* chunk) RETAIL(FUN_001f1d78);
    void DestroyChunkCollision(ChunkData* chunk) RETAIL(FUN_001f1e08);
    void DestroyChunkDataRigidBodies(ChunkData* chunk) RETAIL(FUN_001f24c8);

    // Points the reference at the data (its own reference block counted), letting go of what it had
    ChunkDataReference** AssignChunkData(ChunkDataReference** reference, ChunkData* chunk) RETAIL(FUN_00101248);
    // One count less: the data goes with the last owning reference, the reference with the last one
    void ReleaseChunkDataReference(ChunkDataReference** reference);

    // The list of the chunks' data
    ChunkList* GetChunkList() RETAIL(GetChunkListData_);
    void DestroyChunkList(ChunkList* list, u32 destroyFlags) RETAIL(FUN_001ec968);
    // Every chunk's frame (the current chunk's first): the released ones leave the list and are destroyed
    void UpdateChunks(ChunkList* list, bool paused, GameTimeController* clock) RETAIL(FUN_001ecae0);
    ChunkList* InitChunkList(ChunkList* list) RETAIL(InitChunkListData_);
    void ChunkListAdd(ChunkList* list, ChunkData* chunk) RETAIL(FUN_001f1648);
    // Takes the data out, and every chunk's links to it
    void ChunkListRemove(ChunkList* list, ChunkData* removed) RETAIL(FUN_001f1678);
    // The data of the path, nullptr for none
    ChunkData* FindChunkData(ChunkList* list, String* path) RETAIL(GetChunkData_);
    // A link of the chunk goes in its list (its linked data the chunk's own until the SM2's loader sets it) or out of it
    void ChunkDataLink(ChunkData* chunk, ChunkLinkData* link) RETAIL(FUN_001f18a0);
    void ChunkDataUnlink(ChunkData* chunk, ChunkLinkData* link) RETAIL(FUN_001f1908);
    // The chunk's links to the data are taken out
    void ChunkDataForgetLinksTo(ChunkData* chunk, ChunkData* gone) RETAIL(FUN_001f1a80);

    // The lists' templates (the last two arguments are GCC's pointers to the members, unused)
    void ChunkListPushFront(ChunkData* chunk, ChunkData** head, s32 previous, s32 next) RETAIL(FUN_001f4198);
    void ChunkListTakeOut(ChunkData* chunk, ChunkData** head, s32 previous, s32 next) RETAIL(UnloadChunk_);
    void LinkListPushFrontData(ChunkLinkData* link, ChunkLinkData** head, s32 previous, s32 next) RETAIL(FUN_001f41d8);
    void LinkListRemoveData(ChunkLinkData* link, ChunkLinkData** head, s32 previous, s32 next) RETAIL(FUN_001f4218);
}

// The data's own reference block (made the first time, its top bits kept) with one more reference
ChunkDataReference* AddChunkDataReference(ChunkData* chunk);

// The chunks' data, in a list made the first time it's asked for
struct ChunkList
{
    ChunkData* first;
    // Updated before the others
    ChunkDataReference* current;
};
CHECK_SIZE(ChunkList, 0x8);
