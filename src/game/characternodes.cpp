#include "game/followcamera.h"

#include "game/agents.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controls.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/objectnode.h"
#include "game/reference.h"

#include <cstddef>
#include <cstdint>

// A playable character's own nodes: the controls (kind 0xB, the pad read into the character's controls, or the instance's motion
// driving them while it's held) and the follow camera (kind 0x16, the camera's instance it makes in the character's chunk)

extern "C"
{
    extern const GccVTableEntry g_ControlsNodeVTable[] RETAIL(D_002F4D90);
    extern const GccVTableEntry g_FollowNodeVTable[] RETAIL(UnkNode_0x16_Methods);
    // The game's nodes' base (game/agentnodes.cpp)
    extern const GccVTableEntry g_GameNodeBaseVTable[] RETAIL(D_002F54B8);
    // A camera's instance made in a chunk (placed by a matrix when there's one), with its lens
    InstanceContext* MakeCameraInstance(ChunkData* chunk, const Matrix4x4* matrix) RETAIL(FUN_00259ce0);
}

namespace
{
constexpr u32 ControlsKind = 0xB;
constexpr u32 ControlsType = 0x9006;
constexpr u32 FollowKind = 0x16;
constexpr u32 FollowType = 0x9007;
// The instance's object node and the playable character's node
constexpr u32 ObjectNodeKind = 1;
constexpr u32 PlayerNodeKind = 0xC;
// The controls' handlers' destructor
constexpr u32 HandlerDestroySlot = 1;
// The referenced objects' vtable functions the follow node's camera is told with: released, put to sleep
constexpr u32 ObjectReleaseSlot = 4;
constexpr u32 ObjectSleepSlot = 3;
// The camera's instance's bit 17 (ShowCamera clears it) keeps it from following the character into another chunk
constexpr u32 CameraStaysFlag = 0x20000;
// The follow node's bit 0: its last update came while its clock was stopped
constexpr u32 UpdatedStopped = 0x1;

// The instance's move this frame (its object node's vector at 0xC0)
const Vector4* FrameMove(InstanceContext* instance)
{
    return &static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, ObjectNodeKind))->unknownC0;
}

void DestroyReplacement(ControlsNode* node)
{
    ControlsHandler* replacement = node->replacement;
    if (replacement != nullptr)
    {
        CallVirtual<void>(replacement, replacement->vtable, HandlerDestroySlot, u32{DestroyAndFree});
    }
}

ReferencedObject* FollowedOf(const FollowNode* node)
{
    return node->object != nullptr ? node->object->object : nullptr;
}

// What it follows released, and let go of when it's still there
void ReleaseFollowed(FollowNode* node)
{
    ReferencedObject* object = FollowedOf(node);
    if (object == nullptr)
    {
        return;
    }

    CallVirtual<u32>(object, object->vtable, ObjectReleaseSlot);
    if (FollowedOf(node) != nullptr)
    {
        RemoveReference(&node->object);
        node->object = nullptr;
    }
}

// Its camera started for its character again, the camera's instance made in a chunk
void RestartCamera(FollowNode* node, ChunkData* chunk)
{
    InstanceContext* camera = node->MakeCamera(chunk);
    RestartFollowCamera(&node->camera, G_GameController_00309890, camera, node->owner);
    RestoreCameraDefaults(node);
    node->timer = 0.0f;
}
}

ControlsNode* ConstructControlsNode(void* memory)
{
    auto* node = static_cast<ControlsNode*>(memory);
    GameNode::Construct(node);
    node->pad = nullptr;
    node->vtable = g_ControlsNodeVTable;
    CharacterControls::Construct(&node->handler);
    node->replacement = nullptr;
    node->bits &= ~ControlsNode::BitMotionDriven;
    return node;
}

void ReplaceControlsHandler(ControlsNode* controls, void* handler)
{
    DestroyReplacement(controls);
    controls->replacement = static_cast<ControlsHandler*>(handler);
}

void ControlsNode::Destroy(u32 flags)
{
    vtable = g_ControlsNodeVTable;
    DestroyReplacement(this);
    handler.Destroy(DestroyOnly);
    vtable = g_GameNodeBaseVTable;
    GameNode::Destroy(flags);
}

u32 ControlsNode::Kind()
{
    return ControlsKind;
}

void ControlsNode::Step(TimeClock*, u32)
{
    DestroyReplacement(this);
    replacement = nullptr;
    bits &= ~BitMotionDriven;
    handler.Reset();
}

// While the clock runs: motion driven, the controls are given the instance's move this frame in units a second; else, with a
// pad, the controls in use read it for the instance, and its follow camera reads it
u32 ControlsNode::Update(TimeClock* clock)
{
    if ((clock->flags & TimeClock::FlagRunning) != 0)
    {
        if ((bits & BitMotionDriven) != 0)
        {
            Vector4 velocity = *FrameMove(owner);
            f32 inverse = 1.0f / (static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit);
            velocity.x *= inverse;
            velocity.y *= inverse;
            velocity.z *= inverse;
            handler.MoveBy(clock, &velocity, owner);
        }
        else if (pad != nullptr)
        {
            auto* follow = static_cast<FollowNode*>(GetGameNode(&owner->nodes, FollowKind));
            ControlsHandler* controls = replacement != nullptr ? replacement : &handler;
            controls->FrameVirtual(clock, pad, owner);
            controls->ReadButtons(pad, owner);
            FollowCameraReadPad(&follow->camera, pad);
        }
    }

    return GameNode::Update(clock);
}

u32 ControlsNode::Type()
{
    return ControlsType;
}

FollowNode* ConstructFollowNode(FollowNode* node, ChunkEntry* chunk)
{
    GameNode::Construct(node);
    node->chunk = chunk;
    node->object = nullptr;
    node->vtable = g_FollowNodeVTable;
    ConstructFollowCamera(&node->camera);
    node->nodeBits = 0;
    return node;
}

void RestoreCameraDefaults(FollowNode* node)
{
    auto* player = static_cast<PlayerNode*>(GetGameNode(&node->owner->nodes, PlayerNodeKind));
    RestoreFollowCameraDefaults(&node->camera, reinterpret_cast<CharacterAgent*>(player->character));
}

InstanceContext* FollowNode::MakeCamera(ChunkData* chunk)
{
    AssignReference(&object, MakeCameraInstance(chunk, nullptr));
    // Its clock the chunk's first (unchecked: without a camera, a byte of low memory at 0x153 is written)
    static_cast<InstanceContext*>(FollowedOf(this))->clockIndex = 0;
    return static_cast<InstanceContext*>(FollowedOf(this));
}

void FollowNode::StopFollowing()
{
    UnregisterNode(owner, 0, this);
    ReferencedObject* object = FollowedOf(this);
    if (object != nullptr)
    {
        CallVirtual<u32>(object, object->vtable, ObjectSleepSlot);
    }
}

void FollowNode::Destroy(u32 flags)
{
    vtable = g_FollowNodeVTable;
    ReleaseFollowed(this);
    DestroyFollowCamera(&camera, DestroyOnly);
    RemoveReference(&object);
    vtable = g_GameNodeBaseVTable;
    GameNode::Destroy(flags);
}

void FollowNode::SetOwner(InstanceContext* instance)
{
    Reference* data = chunk->data;
    auto* chunkData = data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr;
    GameNode::SetOwner(instance);
    RestartCamera(this, chunkData);
}

// Into a chunk the link leads to, once the linked chunk's RM2 is loaded: its camera's instance moved along (not while it stays):
// whether it moved
u32 FollowNode::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    // The camera's flags (at address 4 without a camera)
    std::uintptr_t followed = reinterpret_cast<std::uintptr_t>(FollowedOf(this));
    if ((*reinterpret_cast<const u32*>(followed + offsetof(ReferencedObject, flags)) & CameraStaysFlag) == CameraStaysFlag)
    {
        return 0;
    }

    return static_cast<InstanceContext*>(FollowedOf(this))->ChangeChunk(link) != nullptr;
}

u32 FollowNode::Kind()
{
    return FollowKind;
}

void FollowNode::LeftChunk(u32)
{
    ReleaseFollowed(this);
}

void FollowNode::Step(TimeClock* clock, u32)
{
    ChunkData* chunk = owner->chunk;
    if (chunk == nullptr)
    {
        return;
    }

    time = clock->time;
    RestartCamera(this, chunk);
}

// While its instance is the player's (or there's none), its camera's step (the countdown down by the clock's advance, to 0)
// while the clock runs; otherwise it stops following
u32 FollowNode::Update(TimeClock* clock)
{
    GameProgress& progress = G_GameController_00309890->progress;
    InstanceContext* player = progress.Instance(progress.Field(GameProgress::CharacterShift));
    if (player != owner && player != nullptr)
    {
        StopFollowing();
        return GameNode::Update(clock);
    }

    auto* playerNode = static_cast<PlayerNode*>(GetGameNode(&owner->nodes, PlayerNodeKind));
    PlayerCharacter* character = playerNode->character;
    if ((clock->flags & TimeClock::FlagRunning) == 0)
    {
        nodeBits |= UpdatedStopped;
        return GameNode::Update(clock);
    }

    timer -= static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    if (timer < 0.0f)
    {
        timer = 0.0f;
    }

    // The camera's instance read without checking the reference (it always has one: SetOwner and Step make it)
    StepFollowCamera(&camera, clock, reinterpret_cast<CharacterAgent*>(character), static_cast<InstanceContext*>(object->object));
    nodeBits &= ~UpdatedStopped;
    return GameNode::Update(clock);
}

u32 FollowNode::Type()
{
    return FollowType;
}
