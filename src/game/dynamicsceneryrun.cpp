#include "game/dynamicscenery.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/disk.h"
#include "game/graphicstables.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/scenery.h"

// The dynamic scenery at work: an instance for each model of a chunk's SM2, whose node plays the model's animation of float
// channels on it (moving, turning, showing and hiding it) and draws the model's mesh or LOD where it is

EABI_EXPORT(DoDynamicSceneryAnimation, &DynamicSceneryNode::Animate);
EABI_EXPORT(GetDynamicSceneryRotationResult, ReadTrackAngle);

namespace
{
// A model's animation frames a second, and a frame's seconds
constexpr f32 ModelFramesPerSecond = 25.0f;
constexpr f32 SecondsPerModelFrame = Rounded(0.04);

// The resource of an ID from a graphics table with a reference taken, through the table's vtable
template <typename Table>
typename Table::Item* AcquireById(Table* table, u32 id)
{
    return CallVirtual<typename Table::Item*>(table, table->vtable, Table::AcquireSlot, static_cast<const u32*>(&id),
                                              static_cast<bool*>(nullptr));
}

DynamicSceneryNode* ConstructNode(void* memory)
{
    auto* node = static_cast<DynamicSceneryNode*>(memory);
    GameNode::Construct(node);
    node->vtable = g_DynamicSceneryNodeVTable;
    node->model = nullptr;
    node->unused1C = 0;
    node->unused20 = 0;
    node->time = 0.0f;
    node->animation = nullptr;
    node->meshes = nullptr;
    return node;
}

// Where a frame's values start: past the static values, as many values in as a frame has times the frame
const f32* FrameValues(const f32* statics, AnimationLayout layout, u16 frame)
{
    return statics + layout.staticValues + layout.frameValues * frame;
}

// The next channel's value: its static value, or the share of the way from this frame's value to the next frame's
f32 ReadTrackValue(DynamicTrackReader* reader, f32 share)
{
    f32 value;
    if ((reader->statics & 1) == 0)
    {
        value = *reader->next * share + *reader->current * (1.0f - share);
        reader->current++;
        reader->next++;
    }
    else
    {
        value = *reader->staticValues++;
    }

    reader->statics >>= 1;
    reader->channels >>= 1;
    return value;
}
}

ChunkDynamicScenery* ConstructDynamicScenery(ChunkDynamicScenery* scenery, ChunkData* chunk)
{
    scenery->unused04 = 0;
    scenery->chunk = chunk;
    ConstructDynamicSceneryData(&scenery->data);
    return scenery;
}

void DestroyDynamicScenery(ChunkDynamicScenery* scenery, u32 destroyFlags)
{
    DestroyDynamicSceneryData(&scenery->data, DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(scenery);
    }
}

u32 LoadDynamicScenery(ChunkDynamicScenery* scenery, Stream* stream)
{
    DynamicSceneryData* sm2 = &scenery->data;
    ReadDynamicScenery(sm2, stream);
    for (u32 index = 0; index < sm2->modelCount; index++)
    {
        DynamicSceneryModel* model = &sm2->models[index];
        auto* instance = InstanceContext::Construct(static_cast<InstanceContext*>(MemoryAllocate(sizeof(InstanceContext))));
        Vector4 position = g_DefaultBox.min;
        position.w = 1.0f;
        Vector4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        ObjectPlace* place = instance->place;
        place->SyncRotation();
        if (place->TurnTo(&rotation))
        {
            QueueObject(instance);
        }

        place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&position))
        {
            QueueObject(instance);
        }

        // Triggers' signals reach it, the character's solver keeps out of its hulls, it's shown and what stands on it rides along
        instance->flags.receivesTriggerSignals = 1;
        instance->flags.collisionActive = 1;
        instance->flags.visible = 1;
        instance->flags.carriesRiders = 1;
        DynamicSceneryNode* node = ConstructNode(MemoryAllocate(sizeof(DynamicSceneryNode)));
        RegisterNode(instance, AttachNode, node);
        node->SetModel(model);
        RegisterNode(instance, AttachNode, ConstructMovementNode(MemoryAllocate(sizeof(MovementNode))));
        instance->clockIndex = static_cast<u8>(g_DynamicSceneryClockIndex);
        RotateAndTranslate(instance->place);
        ObjectCollision* collision = &instance->collision;
        StepObjectCollision(collision);
        AllocateHullSurfaces(collision, model->hullCount);
        SetAllHullSurfaces(collision, 0);
        collision->bits.stopsBodies = 1;
        node->Animate(0.0f);
        ChunkData::AddInstance(scenery->chunk, instance);
    }

    return 1;
}

void DynamicSceneryNode::Destroy(u32 destroyFlags)
{
    vtable = g_DynamicSceneryNodeVTable;
    if (animation != nullptr)
    {
        MemoryDeallocate2_(animation);
    }

    if (meshes != nullptr)
    {
        meshes->Destroy(DestroyAndFree);
    }

    GameNode::Destroy(destroyFlags);
}

void DynamicSceneryNode::SetOwner(InstanceContext* instance)
{
    instance->flags.dynamicScenery = 1;
    GameNode::SetOwner(instance);
}

u32 DynamicSceneryNode::Kind()
{
    return NodeDynamicScenery;
}

u32 DynamicSceneryNode::Update(TimeClock* clock)
{
    if (clock->flags.running != 0)
    {
        Animate(static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit);
    }

    return 1;
}

u32 DynamicSceneryNode::GetClassId()
{
    return ClassId;
}

void DynamicSceneryNode::SetModel(DynamicSceneryModel* newModel)
{
    if (animation != nullptr)
    {
        MemoryDeallocate2_(animation);
    }

    if (meshes != nullptr)
    {
        meshes->Destroy(DestroyAndFree);
    }

    model = newModel;
    animation = static_cast<DynamicAnimationData*>(MemoryAllocate(sizeof(DynamicAnimationData)));
    animation->information = &model->animation;
    if (model->usesLod)
    {
        meshes = SceneryMeshes::Construct(static_cast<SceneryMeshes*>(MemoryAllocate(sizeof(SceneryMeshes))), 0, 1);
        Lod* lod = AcquireById(&g_LodTable, model->meshId);
        meshes->SetLod(lod, 0);
    }
    else
    {
        meshes = SceneryMeshes::Construct(static_cast<SceneryMeshes*>(MemoryAllocate(sizeof(SceneryMeshes))), 1, 0);
        RigidModel* mesh = AcquireById(&g_MeshTable, model->meshId);
        meshes->SetMesh(mesh, 0);
    }

    meshes->SetBox(1.0f, &model->bounds, 0);
    owner->collision.ownBox = model->bounds;
}

u32 DynamicSceneryNode::Animate(f32 seconds)
{
    // Taken back a loop at a time once it's past the last frame (a model of no frames would loop for ever)
    time += seconds;
    u32 frames = model->frames;
    f32 at = time * ModelFramesPerSecond;
    while (static_cast<f32>(frames) <= at)
    {
        time -= static_cast<f32>(frames) * SecondsPerModelFrame;
        at = time * ModelFramesPerSecond;
    }

    u16 frame = static_cast<u16>(static_cast<s32>(at));
    f32 share = at - static_cast<f32>(frame);
    u16 next = static_cast<u16>(frame + 1);
    if (static_cast<f32>(frames) <= static_cast<f32>(next))
    {
        next = static_cast<u16>(next - frames);
    }

    DynamicAnimationData* animationData = animation;
    AnimationDataInformation* information = animationData->information;
    u8* memory = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    animationData->settings = reinterpret_cast<const JointTrackSettings*>(memory);
    const f32* statics = reinterpret_cast<const f32*>(memory + information->layout.joints * sizeof(JointTrackSettings));
    animationData->statics = statics;
    animationData->current = FrameValues(statics, information->layout, frame);
    animationData->next = FrameValues(statics, animationData->information->layout, next);

    animationData = animation;
    const JointTrackSettings* joint = animationData->settings;
    DynamicTrackReader reader;
    reader.statics = joint->statics;
    u32 channelCount = joint->flags.channels;
    reader.channels = static_cast<u16>((1 << channelCount) - 1);
    reader.staticValues = animationData->statics + joint->staticIndex;
    reader.current = animationData->current + joint->frameIndex;
    reader.next = animationData->next + joint->frameIndex;
    Vector4 position;
    position.x = ReadTrackValue(&reader, share);
    position.y = ReadTrackValue(&reader, share);
    position.z = ReadTrackValue(&reader, share);
    position.w = 1.0f;
    s32 x;
    s32 y;
    s32 z;
    ReadTrackAngle(&x, &reader, share);
    ReadTrackAngle(&y, &reader, share);
    ReadTrackAngle(&z, &reader, share);
    f32 shown = ReadTrackValue(&reader, share);
    Vector4 rotation;
    GetRotationXYZ(&rotation, &x, &y, &z);

    InstanceContext* instance = owner;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&position))
    {
        QueueObject(instance);
    }

    instance = owner;
    place = instance->place;
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    instance = owner;
    bool visible = instance->flags.visible;
    if (ShownAbove < shown && !visible)
    {
        instance->flags.visible = 1;
        visible = true;
    }
    else if (shown < ShownAbove && visible)
    {
        instance->flags.visible = 0;
        visible = false;
    }

    if (visible)
    {
        PlaceMeshes();
    }

    return 1;
}

void DynamicSceneryNode::PlaceMeshes()
{
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    meshes->SetMatrix(place, 0);
}

u32 DynamicSceneryNode::Draw(ChunkView* view)
{
    return meshes->Draw(view);
}

u32 DynamicSceneryNode::DrawCulled(ChunkView* view)
{
    return meshes->DrawCulled(view);
}

s32* ReadTrackAngle(s32* angle, DynamicTrackReader* reader, f32 share)
{
    s32 turned;
    if ((reader->statics & 1) != 0)
    {
        AngleFrom(&turned, *reader->staticValues, AngleRadians);
        reader->staticValues++;
    }
    else
    {
        s32 current;
        s32 next;
        AngleFrom(&current, *reader->current, AngleRadians);
        AngleFrom(&next, *reader->next, AngleRadians);
        // The shorter way round: a turn taken off or added when they're more than half a turn apart (as words, wrapping round)
        s32 difference = static_cast<s32>(static_cast<u32>(current) - static_cast<u32>(next));
        if (difference > HalfTurnAngle)
        {
            current = static_cast<s32>(static_cast<u32>(current) - FullTurnAngle);
        }
        else if (difference < -HalfTurnAngle)
        {
            current = static_cast<s32>(static_cast<u32>(current) + FullTurnAngle);
        }

        f32 radians =
            static_cast<f32>(next) * AngleToRadians * share + static_cast<f32>(current) * AngleToRadians * (1.0f - share);
        turned = static_cast<s32>(radians * RadiansToAngle);
        reader->current++;
        reader->next++;
    }

    reader->statics >>= 1;
    reader->channels >>= 1;
    *angle = turned;
    return angle;
}
