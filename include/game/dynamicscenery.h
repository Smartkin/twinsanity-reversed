#pragma once

#include "common.h"
#include "game/animation.h"
#include "game/hull.h"
#include "game/math.h"

class Stream;

// A model of a chunk's dynamic scenery (0x50 bytes, TT Lab's TwinDynamicSceneryModel): a leftover word (8, never read), its
// collision hulls, its frames, its animation (joints' settings like an animation's, its values floats: bits 9-21 of its sections
// the static values' bytes, bits 22-31 the values a frame), whether it draws by its LOD, its mesh's ID and its bounds
struct DynamicSceneryModel
{
    s32 leftover;
    u32 hullCount;
    CollisionHull* hulls;
    u32 frames;
    AnimationDataInformation animation;
    bool usesLod;
    u32 meshId;
    u8 unknown24[0x30 - 0x24];
    Box bounds;
};
CHECK_OFFSET(DynamicSceneryModel, animation, 0x10);
CHECK_OFFSET(DynamicSceneryModel, bounds, 0x30);
CHECK_SIZE(DynamicSceneryModel, 0x50);

// A chunk's dynamic scenery as its SM2 has it (12 bytes, 8 bytes into the chunk's dynamic scenery): its version (0x10009, read
// and never checked) and its models
struct DynamicSceneryData
{
    u32 version;
    u16 modelCount;
    DynamicSceneryModel* models;
};
CHECK_SIZE(DynamicSceneryData, 0xC);

extern "C"
{
    DynamicSceneryData* ConstructDynamicSceneryData(DynamicSceneryData* data) RETAIL(FUN_00299748);
    // Its models destroyed, last to first
    void DestroyDynamicSceneryData(DynamicSceneryData* data, u32 destroyFlags) RETAIL(FUN_00299760);
    void ReadDynamicScenery(DynamicSceneryData* data, Stream* stream);
    DynamicSceneryModel* ConstructDynamicSceneryModel(DynamicSceneryModel* model) RETAIL(FUN_00299900);
    // Its hulls destroyed (last to first) and its animation's disk manager node let go
    void DestroyDynamicSceneryModel(DynamicSceneryModel* model, u32 destroyFlags) RETAIL(FUN_00299930);
    // The leftover, the hulls, the frames, the animation (read into a disk manager node of its own, the one it had let go), the
    // LOD flag, the mesh's ID and the bounds
    void ReadDynamicSceneryModel(DynamicSceneryModel* model, Stream* stream) RETAIL(FUN_00297a38);
}
