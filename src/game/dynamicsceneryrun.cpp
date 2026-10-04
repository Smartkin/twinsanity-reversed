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
// The flags a dynamic scenery's instance is made with (triggers' signals reach it, the character's solver keeps out of its hulls,
// it's shown, and 0x4000), and the one its node gives it (the scenery cells keep such instances on a list of their own)
constexpr u32 DynamicSceneryFlags =
    ReferencedObject::FlagTriggerSignals | ReferencedObject::FlagSphereContact | ReferencedObject::FlagVisible | 0x4000;
constexpr u32 FlagDynamicScenery = 0x40000;
// Bit 0 of its collision's bits (some agents' instances set it too; no reader of it was found)
constexpr u64 CollisionBit0 = 0x1;
// The graphics tables' vtable slot of the resource of an ID with a reference taken (GraphicsTable::Acquire)
constexpr u32 AcquireSlot = 4;

constexpr f32 FramesPerSecond = 25.0f;
constexpr f32 SecondsPerFrame = Rounded(0.04);
// A channel above this shows the instance, below it hides it
constexpr f32 ShownAbove = 0.5f;
constexpr f32 TurnToRadians = 0x1.921fb6p-14f;
constexpr f32 RadiansToTurn = 0x1.45f306p+13f;
constexpr s32 Turn = 0x10000;
constexpr s32 HalfTurn = 0x8000;
// Where the static values start past the joints' settings, and the bytes they take (the count of bits 11-21, times 4)
constexpr u32 StaticBytesShift = 9;
constexpr u32 StaticBytesMask = 0x1FFC;

// The resource of an ID from a graphics table with a reference taken, through the table's vtable
template <typename Table>
typename Table::Item* AcquireById(Table* table, u32 id)
{
    return CallVirtual<typename Table::Item*>(table, table->vtable, AcquireSlot, static_cast<const u32*>(&id),
                                              static_cast<bool*>(nullptr));
}

DynamicSceneryNode* ConstructNode(void* memory)
{
    auto* node = static_cast<DynamicSceneryNode*>(memory);
    GameNode::Construct(node);
    node->vtable = g_DynamicSceneryNodeVTable;
    node->model = nullptr;
    node->unknown1C = 0;
    node->unknown20 = 0;
    node->time = 0.0f;
    node->animation = nullptr;
    node->meshes = nullptr;
    return node;
}

// Where a frame's values start: past the static values, as many values in as a frame has times the frame
const f32* FrameValues(const f32* statics, u32 sections, u16 frame)
{
    const u8* values = reinterpret_cast<const u8*>(statics) + (sections >> StaticBytesShift & StaticBytesMask);
    return reinterpret_cast<const f32*>(values + (sections >> AnimationDataInformation::FrameValuesShift) * frame * sizeof(f32));
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
    scenery->unknown04 = 0;
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
    DynamicSceneryData* data = &scenery->data;
    ReadDynamicScenery(data, stream);
    for (u32 index = 0; index < data->modelCount; index++)
    {
        DynamicSceneryModel* model = &data->models[index];
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

        instance->flags |= DynamicSceneryFlags;
        DynamicSceneryNode* node = ConstructNode(MemoryAllocate(sizeof(DynamicSceneryNode)));
        RegisterNode(instance, 1, node);
        node->SetModel(model);
        RegisterNode(instance, 1, ConstructMovementNode(MemoryAllocate(sizeof(MovementNode))));
        instance->clockIndex = static_cast<u8>(g_DynamicSceneryClockIndex);
        RotateAndTranslate(instance->place);
        ObjectCollision* collision = &instance->collision;
        StepObjectCollision(collision);
        AllocateHullSurfaces(collision, model->hullCount);
        SetAllHullSurfaces(collision, 0);
        collision->bits |= CollisionBit0;
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
    instance->flags |= FlagDynamicScenery;
    GameNode::SetOwner(instance);
}

u32 DynamicSceneryNode::Kind()
{
    return NodeKind;
}

u32 DynamicSceneryNode::Update(TimeClock* clock)
{
    if ((clock->flags & TimeClock::FlagRunning) != 0)
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
    f32 at = time * FramesPerSecond;
    while (static_cast<f32>(frames) <= at)
    {
        time -= static_cast<f32>(frames) * SecondsPerFrame;
        at = time * FramesPerSecond;
    }

    u16 frame = static_cast<u16>(static_cast<s32>(at));
    f32 share = at - static_cast<f32>(frame);
    u16 next = static_cast<u16>(frame + 1);
    if (static_cast<f32>(frames) <= static_cast<f32>(next))
    {
        next = static_cast<u16>(next - frames);
    }

    DynamicAnimationData* data = animation;
    AnimationDataInformation* information = data->information;
    u8* memory = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    data->settings = reinterpret_cast<const JointTrackSettings*>(memory);
    const f32* statics = reinterpret_cast<const f32*>(
        memory + (information->sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings));
    data->statics = statics;
    data->current = FrameValues(statics, information->sections, frame);
    data->next = FrameValues(statics, data->information->sections, next);

    data = animation;
    const JointTrackSettings* joint = data->settings;
    DynamicTrackReader reader;
    reader.statics = joint->statics;
    u32 channelCount = joint->flags >> JointTrackSettings::ChannelsShift & JointTrackSettings::ChannelsMask;
    reader.channels = static_cast<u16>((1 << channelCount) - 1);
    reader.staticValues = data->statics + joint->staticIndex;
    reader.current = data->current + joint->frameIndex;
    reader.next = data->next + joint->frameIndex;
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
    bool visible = (instance->flags & ReferencedObject::FlagVisible) != 0;
    if (ShownAbove < shown && !visible)
    {
        instance->flags |= ReferencedObject::FlagVisible;
        visible = true;
    }
    else if (shown < ShownAbove && visible)
    {
        instance->flags &= ~ReferencedObject::FlagVisible;
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
        if (difference > HalfTurn)
        {
            current = static_cast<s32>(static_cast<u32>(current) - Turn);
        }
        else if (difference < -HalfTurn)
        {
            current = static_cast<s32>(static_cast<u32>(current) + Turn);
        }

        f32 radians =
            static_cast<f32>(next) * TurnToRadians * share + static_cast<f32>(current) * TurnToRadians * (1.0f - share);
        turned = static_cast<s32>(radians * RadiansToTurn);
        reader->current++;
        reader->next++;
    }

    reader->statics >>= 1;
    reader->channels >>= 1;
    *angle = turned;
    return angle;
}
