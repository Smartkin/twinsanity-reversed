#include "game/dynamicscenery.h"

#include "game/disk.h"
#include "game/memory.h"
#include "game/stream.h"

namespace
{
// The animation's size: its values are floats
u32 DynamicAnimationSize(const AnimationDataInformation* animation)
{
    constexpr u32 StaticBytesShift = 9;
    constexpr u32 StaticBytesMask = 0x1FFC;
    if (animation->frames == 0)
    {
        return 0;
    }

    u32 sections = animation->sections;
    u32 perFrame = sections >> AnimationDataInformation::FrameValuesShift;
    return (sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings) +
           (sections >> StaticBytesShift & StaticBytesMask) + perFrame * animation->frames * sizeof(f32);
}
}

DynamicSceneryData* ConstructDynamicSceneryData(DynamicSceneryData* data)
{
    constexpr u32 Version = 0x10009;
    data->version = Version;
    return data;
}

void DestroyDynamicSceneryData(DynamicSceneryData* data, u32 destroyFlags)
{
    if (data->models != nullptr)
    {
        DynamicSceneryModel* model = data->models + ArrayCount(data->models);
        while (model != data->models)
        {
            model--;
            DestroyDynamicSceneryModel(model, 0);
        }

        DeleteArray(data->models);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(data);
    }
}

void ReadDynamicScenery(DynamicSceneryData* data, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&data->version));
    stream->ReadS16(reinterpret_cast<s16*>(&data->modelCount));
    u32 count = data->modelCount;
    DynamicSceneryModel* models = NewArray<DynamicSceneryModel>(count);
    for (u32 index = 0; index < count; index++)
    {
        ConstructDynamicSceneryModel(&models[index]);
    }

    data->models = models;
    for (u32 index = 0; index < data->modelCount; index++)
    {
        ReadDynamicSceneryModel(&data->models[index], stream);
    }
}

DynamicSceneryModel* ConstructDynamicSceneryModel(DynamicSceneryModel* model)
{
    model->animation.diskHandle = -1;
    model->meshId = 0xFFFFFFFF;
    model->animation.sections = 0;
    model->animation.frames = 0;
    model->leftover = -1;
    model->hullCount = 0;
    model->hulls = nullptr;
    return model;
}

void DestroyDynamicSceneryModel(DynamicSceneryModel* model, u32 destroyFlags)
{
    if (model->hulls != nullptr)
    {
        CollisionHull* hull = model->hulls + ArrayCount(model->hulls);
        while (hull != model->hulls)
        {
            hull--;
            HullDestroy(hull, 0);
        }

        DeleteArray(model->hulls);
    }

    if (model->animation.diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &model->animation.diskHandle);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(model);
    }
}

void ReadDynamicSceneryModel(DynamicSceneryModel* model, Stream* stream)
{
    stream->ReadS32(&model->leftover);
    stream->ReadS32(reinterpret_cast<s32*>(&model->hullCount));
    u32 hullCount = model->hullCount;
    CollisionHull* hulls = NewArray<CollisionHull>(hullCount);
    for (u32 index = 0; index < hullCount; index++)
    {
        HullConstruct(&hulls[index]);
    }

    model->hulls = hulls;
    for (u32 index = 0; index < model->hullCount; index++)
    {
        ReadModelCollisionData(&model->hulls[index], stream);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&model->frames));
    AnimationDataInformation* animation = &model->animation;
    stream->ReadS32(reinterpret_cast<s32*>(&animation->sections));
    stream->ReadS16(reinterpret_cast<s16*>(&animation->frames));
    if (animation->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &animation->diskHandle);
    }

    u32 size = DynamicAnimationSize(animation);
    if (size != 0)
    {
        s32 handle;
        DiskAllocate(&handle, GetDiskManager(), size, false, 0);
        animation->diskHandle = handle;
        stream->Read(DiskLoadedMemory(GetDiskManager(), &animation->diskHandle), size, 1);
    }

    stream->ReadBool(&model->usesLod);
    stream->ReadS32(reinterpret_cast<s32*>(&model->meshId));
    Box bounds;
    stream->Read(&bounds.min, sizeof(Vector4), 1);
    stream->Read(&bounds.max, sizeof(Vector4), 1);
    model->bounds = bounds;
}
