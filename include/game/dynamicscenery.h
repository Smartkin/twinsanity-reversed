#pragma once

#include "common.h"
#include "game/animation.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"

class Stream;
struct ChunkData;
struct ChunkView;
struct SceneryMeshes;
struct TimeClock;

// A model of a chunk's dynamic scenery (0x50 bytes, TT Lab's TwinDynamicSceneryModel): a leftover word (8, never read), its
// collision hulls, its frames, its animation (joints' settings like an animation's, its values floats), whether it draws by its
// LOD, its mesh's ID and its bounds
struct DynamicSceneryModel
{
    // A new model's mesh ID
    static constexpr u32 NoMesh = 0xFFFFFFFF;

    s32 leftover;
    u32 hullCount;
    CollisionHull* hulls;
    u32 frames;
    AnimationDataInformation animation;
    bool usesLod;
    u32 meshId;
    u8 unused24[0x30 - 0x24];
    Box bounds;
};
CHECK_OFFSET(DynamicSceneryModel, animation, 0x10);
CHECK_OFFSET(DynamicSceneryModel, bounds, 0x30);
CHECK_SIZE(DynamicSceneryModel, 0x50);

// A chunk's dynamic scenery as its SM2 has it (12 bytes, 8 bytes into the chunk's dynamic scenery): its version (0x10009, read
// and never checked) and its models
struct DynamicSceneryData
{
    static constexpr u32 Version = 0x10009;

    u32 version;
    u16 modelCount;
    DynamicSceneryModel* models;
};
CHECK_SIZE(DynamicSceneryData, 0xC);

extern "C"
{
    DynamicSceneryData* ConstructDynamicSceneryData(DynamicSceneryData* scenery) RETAIL(FUN_00299748);
    // Its models destroyed, last to first
    void DestroyDynamicSceneryData(DynamicSceneryData* scenery, u32 destroyFlags) RETAIL(FUN_00299760);
    void ReadDynamicScenery(DynamicSceneryData* scenery, Stream* stream);
    DynamicSceneryModel* ConstructDynamicSceneryModel(DynamicSceneryModel* model) RETAIL(FUN_00299900);
    // Its hulls destroyed (last to first) and its animation's disk manager node let go
    void DestroyDynamicSceneryModel(DynamicSceneryModel* model, u32 destroyFlags) RETAIL(FUN_00299930);
    // The leftover, the hulls, the frames, the animation (read into a disk manager node of its own, the one it had let go), the
    // LOD flag, the mesh's ID and the bounds
    void ReadDynamicSceneryModel(DynamicSceneryModel* model, Stream* stream) RETAIL(FUN_00297a38);
}

// A chunk's dynamic scenery (0x14 bytes, made the first time the chunk's data is asked for it): its chunk, a word nothing reads
// (0) and what its SM2 has
struct ChunkDynamicScenery
{
    ChunkData* chunk;
    u32 unused04;
    DynamicSceneryData data;
};
CHECK_SIZE(ChunkDynamicScenery, 0x14);

// A model's animation as its node plays it (0x14 bytes, an OGI animation's AnimationData of floats): its information, its joints'
// settings, its static values, and this frame's and the next frame's values, found again in the disk manager every frame
struct DynamicAnimationData
{
    // The bytes its static values take in its information's sections: their count (bits 11-21) times a float's size
    static constexpr u32 StaticBytesShift = 9;
    static constexpr u32 StaticBytesMask = 0x1FFC;

    AnimationDataInformation* information;
    const JointTrackSettings* settings;
    const f32* statics;
    const f32* current;
    const f32* next;
};
CHECK_SIZE(DynamicAnimationData, 0x14);

// A reader of a joint's channels of floats (0x10 bytes, the OGI animations' TrackReader): its settings' bits of the channels read
// from the static values (bit 0 the next channel's, shifted out as they're read), the mask of its channels (shifted along, never
// read), where its static values, this frame's and the next frame's values are
struct DynamicTrackReader
{
    u16 statics;
    u16 channels;
    const f32* staticValues;
    const f32* current;
    const f32* next;
};
CHECK_SIZE(DynamicTrackReader, 0x10);

// A joint's channel of whether it's shown shows its instance above this and hides it below (the cutscenes' tracks' too, whose
// emitters are kept above it)
constexpr f32 ShownAbove = 0.5f;

// A dynamic scenery instance's node (kind 4, class 0x1617, 0x30 bytes, its vtable D_002FC430): its model, two words nothing reads,
// the seconds into its animation, the animation it plays and the meshes the model is drawn with (a mesh or a LOD of the model's
// mesh ID, at its instance's place). The model's first joint's channels move the instance: its position (x, y, z), its turns
// about x, y and z, then whether it's shown (above a half)
struct DynamicSceneryNode : GameNode
{
    static constexpr u32 ClassId = 0x1617;

    DynamicSceneryModel* model;
    u32 unused1C;
    u32 unused20;
    f32 time;
    DynamicAnimationData* animation;
    SceneryMeshes* meshes;

    // Its vtable's slots: 2 the destructor (its animation and meshes with it), 3 given its instance (marked dynamic scenery,
    // ReferencedObjectFlags::dynamicScenery), 5 its kind, 8 its update (its animation played on by the clock's last advance while
    // the clock runs; returns 1), 10 its class
    void Destroy(u32 destroyFlags) RETAIL(FUN_002017c0);
    void SetOwner(InstanceContext* instance) RETAIL(FUN_002019a8);
    u32 Kind() RETAIL(GetNodeIndex_002017A8);
    u32 Update(TimeClock* clock) RETAIL(AnimateDynamicScenery);
    u32 GetClassId() RETAIL(FUN_002017b0);
    // Given a model (the animation and meshes it had let go of): its animation, its meshes (their box the model's bounds) and its
    // instance's collision's own box
    void SetModel(DynamicSceneryModel* model) RETAIL(FUN_001fe4d0);
    // Its animation played on by seconds (the model's frames at 25 a second, looped): its instance moved and turned to the frame's
    // values, shown or hidden, and its meshes placed while it's shown. Returns 1
    u32 Animate(f32 seconds) RETAIL_N32(DoDynamicSceneryAnimation);
    // Its meshes placed at its instance's place (the place's matrix made again first)
    void PlaceMeshes() RETAIL(FUN_00201a18);
    // Its meshes drawn through a view, taken as wholly in view and culled a box at a time: how many were
    u32 Draw(ChunkView* view) RETAIL(FUN_00201a60);
    u32 DrawCulled(ChunkView* view) RETAIL(FUN_00201a80);
};
CHECK_OFFSET(DynamicSceneryNode, model, 0x18);
CHECK_OFFSET(DynamicSceneryNode, time, 0x24);
CHECK_SIZE(DynamicSceneryNode, 0x30);

extern "C"
{
    extern const GccVTableEntry g_DynamicSceneryNodeVTable[] RETAIL(D_002FC430);
    // The clock of a chunk's clocks the dynamic scenery's instances go by (the game's start-up sets it to 1)
    extern u32 g_DynamicSceneryClockIndex RETAIL(G_DynamicSceneryClockIndex);

    // A chunk's dynamic scenery made (empty), and destroyed with its models
    ChunkDynamicScenery* ConstructDynamicScenery(ChunkDynamicScenery* scenery, ChunkData* chunk) RETAIL(FUN_00201aa0);
    void DestroyDynamicScenery(ChunkDynamicScenery* scenery, u32 destroyFlags) RETAIL(FUN_00201ad8);
    // The SM2's dynamic scenery read into a chunk's, and an instance put in the chunk for each model: at the origin, not turned,
    // shown, with the model's node (its first frame played) and a movement node, going by the dynamic scenery's clock, its
    // collision with a surface for each of the model's hulls. Returns 1
    u32 LoadDynamicScenery(ChunkDynamicScenery* scenery, Stream* stream);
    // The next channel of a reader as an angle (whole 65536ths of a turn, returned through the first argument as GCC 2.9x returns
    // such a struct): its static value's, or the share of the way from this frame's angle to the next frame's, the shorter way
    // round
    s32* ReadTrackAngle(s32* angle, DynamicTrackReader* reader, f32 share) RETAIL_N32(GetDynamicSceneryRotationResult);
}
