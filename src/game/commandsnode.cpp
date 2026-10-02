#include "game/commands.h"

#include "game/agents.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/rigidbody.h"

// The commands that work on the agent's object node, its runner and its levels

extern "C"
{
    // The node's motion stopped, a head tracking and a perception made from a command's arguments, a motion block's flag (still
    // asm)
    void StopNodeMotion(ObjectNode* node) RETAIL(FUN_0023e6d8);
    void CreateHeadTracking(ObjectNode* node, const void* arguments, TimeClock* clock) RETAIL(FUN_0023e398);
    void AddPerception(ObjectNode* node, const void* arguments) RETAIL(FUN_0023e178);
    void SetMotionBlockFlag(MotionBlock* block, u32 set) RETAIL(FUN_0023fa10);
    // The head tracking stopped, and started following the node's focus (still asm)
    void StopHeadTracking(HeadTracking* tracking) RETAIL(FUN_0023fdd0);
    void StartHeadTracking(HeadTracking* tracking, ObjectNode* node) RETAIL(FUN_00237cc0);
    // A perception slot's weight set and added to (the float first), and the two other slot functions (still asm)
    void SetPerceptionWeight(f32 weight, void* perception, u32 slot) RETAIL_N32(FUN_0023ff38);
    void AddPerceptionWeight(f32 weight, void* perception, u32 slot) RETAIL_N32(FUN_00240010);
    void PerceptionSlot141(void* perception, u32 slot) RETAIL(FUN_00240150);
    void PerceptionSlot142(void* perception, u32 slot) RETAIL(FUN_002401c8);
    // The springs of the instance's attachments node (kind 6) let go, a rigid body's motion put back (still asm)
    void DetachAllSprings(void* attachments) RETAIL(FUN_00197140);
    void RevertColliderMotion(ObjectRigidBody* body) RETAIL(FUN_002546a0);
    // The velocity commands' work: a velocity given to an instance (from another's point of view; still asm)
    void ApplyVelocity(const ApplyVelocityToSelfCommand* command, InstanceContext* target, InstanceContext* source)
        RETAIL(FUN_0010c570);
}

namespace
{
// The node's vtable functions: a particle trail added and the trails destroyed
constexpr u32 AddTrailSlot = 23;
constexpr u32 ClearTrailsSlot = 24;
// The agent's vtable function 5
constexpr u32 AgentSlot5 = 5;
constexpr u32 AttachmentsKind = 6;
// The instance flags a detach request clears, and the one a springy contact sets
constexpr u32 AttachedFlag = 0x40;
// The node's flags a motion clears
constexpr u32 MotionClearedFlags = 0x8000 | 0x10000 | 0x20000;
// The motion block's flag the commands 159 to 161 clear and set
constexpr u32 MotionBlockFlag16 = 0x10000;
// The receivers' bit the threats designator's bit 0 goes to
constexpr u32 ThreatsBit = 0x80;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

void ClearTrails(GameNode* node)
{
    CallVirtual<void>(node, node->vtable, ClearTrailsSlot);
}

// A word's low byte (the slots are angles' tagged values in TT Lab's definitions, read a byte)
u32 LowByte(const TaggedValue& value)
{
    return static_cast<u8>(value.raw);
}

// Where the commands' perception weights are (TT Lab's definitions have them as integers)
f32 FloatOf(const s32& value)
{
    return *reinterpret_cast<const f32*>(&value);
}
}

EABI_IMPORT(FUN_0023ff38, SetPerceptionWeight);
EABI_IMPORT(FUN_00240010, AddPerceptionWeight);

void AddTrailCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ExecuteOn(runner->agentNode);
}

// The trail's arguments start after its lifetime
void AddTrailCommand::ExecuteOn(GameNode* node)
{
    CallVirtual<u32>(node, node->vtable, AddTrailSlot, &flags);
}

void ClearTrailCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ClearTrails(runner->agentNode);
}

void ClearTrailCommand::ExecuteOn(GameNode* node)
{
    ClearTrails(node);
}

// The keys of the node it takes its object from when there's one
void NextKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    auto* source = static_cast<ObjectNode*>(node->sourceNode);
    (source != nullptr ? source->waypoints : node->waypoints)->NextKey();
}

// The arguments are a motion block
void SetWobbleCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->FollowMotionBlock(reinterpret_cast<MotionBlock*>(&amplitudeX), clock);
}

void ClearWobbleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ReleaseTrajectory();
}

void ClearThreatsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    StarterReceivers* receivers = runner->receivers;
    receivers->bits = (receivers->bits & ~ThreatsBit) | ((designator & 1) != 0 ? ThreatsBit : 0);
}

// The route's index goes down as it's followed
void NextRouteNodeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->PreviousRouteStep();
}

void DiscardRouteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Waypoints* waypoints = NodeOf(runner)->waypoints;
    if (waypoints != nullptr)
    {
        waypoints->ReleaseRoute();
    }
}

void UnsupportOverFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Agent* agent = NodeOf(runner)->agent;
    CallVirtual<void>(agent, agent->vtable, AgentSlot5);
}

void ClearFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->flags = node->flags & ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
}

void ClearCollisionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ReleaseRigidBody();
}

void StopMovingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    StopNodeMotion(NodeOf(runner));
}

void ClearUserMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ClearMessages();
}

void RevertColliderMotionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    RevertColliderMotion(NodeOf(runner)->rigidBody);
}

void RequestDetachCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->owner->flags &= ~AttachedFlag;
}

void DetachAllSpringsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* attachments = GetGameNode(&NodeOf(runner)->owner->nodes, AttachmentsKind);
    if (attachments != nullptr)
    {
        DetachAllSprings(attachments);
    }
}

// The arguments become the node's motion block (pointing back at the node) and its instance gets a physics body
void SetContactSpringyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->motionBlock = reinterpret_cast<MotionBlock*>(&drag);
    node->owner->flags |= ReferencedObject::FlagPhysicsBody;
    node->motionBlock->node = node;
}

void ClearAgentRef1Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->agentRef1 = nullptr;
}

void ClearAgentRef2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->agentRef2 = nullptr;
}

void ClearFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ForgetStoredPosition();
}

void StopHeadTrackingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    HeadTracking* tracking = NodeOf(runner)->headTracking;
    if (tracking != nullptr)
    {
        StopHeadTracking(tracking);
    }
}

void StartHeadTrackingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node->headTracking != nullptr)
    {
        StartHeadTracking(node->headTracking, node);
    }
}

void DestroyHeadTrackingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ReleaseHeadTracking();
}

void ClearNodeByte154Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->unknown154 = 0;
}

// The agent's contact message forgotten (its point the default box's lowest corner)
void ClearObjectContextTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ContactMessage* contact = &NodeOf(runner)->agent->contact;
    contact->word = 0;
    contact->byte = 0;
    contact->point = g_DefaultBox.min;
}

void SetFocusToOriginatorCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* originator = static_cast<InstanceContext*>(runner->originator);
    if (originator == nullptr)
    {
        return;
    }

    ObjectNode* node = NodeOf(runner);
    node->focusInstance = originator;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
}

void DestroyPerceptionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ReleasePerception();
}

void SetPerceptionWeightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        SetPerceptionWeight(FloatOf(weightValue), perception, LowByte(slot));
    }
}

void AddPerceptionWeightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        AddPerceptionWeight(FloatOf(value), perception, LowByte(slot));
    }
}

void PerceptionOp141Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        PerceptionSlot141(perception, LowByte(slot));
    }
}

void PerceptionOp142Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        PerceptionSlot142(perception, LowByte(slot));
    }
}

// The value goes in the upper half of the bits of the level below (none on the first level)
void SetParentExecutionValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    u32 index = level->bits & BehaviourLevel::LevelMask;
    if (index == 0)
    {
        return;
    }

    reinterpret_cast<u16*>(&runner->levels[index - 1]->bits)[1] = static_cast<u16>(value);
}

void PreviousKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->PreviousKey();
}

void NextKeyOfPath34Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->NextRouteStep();
}

void ResetNode120Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->unknown78 = 0;
        block->flags &= ~MotionBlockFlag16;
    }
}

void ClearMotionBlockFlag16Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->flags &= ~MotionBlockFlag16;
    }
}

void SetMotionBlockFlag16Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->flags = (block->flags & ~MotionBlockFlag16) | MotionBlockFlag16;
    }
}

void SetKeyPathByte43Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->pathIndex = static_cast<u8>(value);
}

void SetAgentRef1ToOwnerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node->sourceNode != nullptr)
    {
        node->agentRef1 = node->sourceNode->owner;
    }
}

// The interrupting states checked from the level the runner is at
void SaveScriptStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->interruptLevel = runner->depth;
    runner->flags |= BehaviourRunner::FlagInterruptFromLevel;
}

void ClearSavedScriptStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 NoLevel = 0xFF;
    runner->interruptLevel = NoLevel;
    runner->flags &= ~BehaviourRunner::FlagInterruptFromLevel;
}

void MarkTimeCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->markedTime = clock->time;
}

void ClearMarkedTimeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->markedTime = 0;
}

void KeyOfPath34Op213Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->RestartRoute();
}

void ApplyVelocityToSelfCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    ApplyVelocity(this, instance, instance);
}

// The arguments are a motion block
void SetMotionCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->flags &= ~MotionClearedFlags;
    node->FollowMotionBlock(reinterpret_cast<MotionBlock*>(&flags), clock);
}

void CreateHeadTrackingCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    CreateHeadTracking(NodeOf(runner), &value0, clock);
}

void AddPerceptionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AddPerception(NodeOf(runner), &type);
    NodeOf(runner)->flags |= ObjectNodeBase::FlagKeepsPerception;
}

void SetNode120FlagCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node != nullptr && node->motionBlock != nullptr)
    {
        SetMotionBlockFlag(node->motionBlock, value & 1);
    }
}

// A positive value counted among the instances' that have one
void ClearNodeValue174Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* value = reinterpret_cast<f32*>(reinterpret_cast<u8*>(NodeOf(runner)) + 0x174);
    f32 old = *value;
    *value = 0.0f;
    if (*value < old)
    {
        g_InstancesWithValue174--;
        if (g_InstancesWithValue174 < 0)
        {
            g_InstancesWithValue174 = 0;
        }
    }
}

namespace
{
// The kind 1 nodes (the agents' object nodes), and their vtable function that says whether they take packets
constexpr u32 ObjectNodeKind = 1;
constexpr u32 TakesPacketsSlot = 15;
// The attachments node's word: the count of linked objects (bits 0-4) and the one it's at (7-11)
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;
// A designator meaning none
constexpr u8 NoDesignator = 0xFF;
// The node's vtable function giving a designator's instance
constexpr u32 GetDesignatorSlot = 36;
// The instance flag that makes it move backwards along its attachments' path
constexpr u32 BackwardsFlag = 0x80;
// A node's surface word: a surface ID (bits 0-15) for every hull, unless bit 16 is clear: then for the hull of bits 17-24
constexpr u32 AllHullsBit = 0x10000;
constexpr u32 HullShift = 17;
constexpr u32 PerceptionCountMask = 0xF;
// The timer reset applies to the agent's own node with this bit when no designator's given
constexpr u32 OwnTimerBit = 0x100;

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
}

u32* LinkedObjectsWord(void* attachments)
{
    return reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18);
}

InstanceContext* DesignatorOf(ObjectNode* node, u8 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, u32{designator});
}

// A perception's slots (as many as its word at 0x80 says, bits 0-3) switched on or off, 0x40 bytes in
void SwitchPerceptions(u8* perception, u8 on)
{
    u8* slots = perception + 0x40;
    for (u32 index = 0; index < (*reinterpret_cast<u32*>(perception + 0x80) & PerceptionCountMask); index++)
    {
        slots[index] = on;
    }
}
}

extern "C"
{
    // An instance turned to an angle about one of its axes (the float first; still asm)
    void SetInstanceRotationX(f32 angle, InstanceContext* instance) RETAIL_N32(FUN_0022e410);
    void SetInstanceRotationY(f32 angle, InstanceContext* instance) RETAIL_N32(FUN_0022e598);
    void SetInstanceRotationZ(f32 angle, InstanceContext* instance) RETAIL_N32(FUN_0022e730);
    // The node's velocity reset, the instance of the attachments' path's entry without a slot (none: nullptr), and the
    // instances within a radius told (still asm)
    void ResetNodeVelocity(ObjectNode* node) RETAIL(FUN_0023ddd8);
    InstanceContext* UnslottedAttachment(void* path) RETAIL(FUN_001955a0);
    void NotifyInstancesWithin(f32 radius, ObjectNode* node) RETAIL_N32(FUN_0022f018);
    // A rigid body reset, and made active (still asm)
    void ResetRigidBody(ObjectRigidBody* body) RETAIL(FUN_00254088);
    void ActivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00246950);
}

EABI_IMPORT(FUN_0022e410, SetInstanceRotationX);
EABI_IMPORT(FUN_0022e598, SetInstanceRotationY);
EABI_IMPORT(FUN_0022e730, SetInstanceRotationZ);
EABI_IMPORT(FUN_0022f018, NotifyInstancesWithin);

void SetRotationComponentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if ((components & 1) != 0)
    {
        SetInstanceRotationX(x, instance);
    }

    if ((components & 2) != 0)
    {
        SetInstanceRotationY(y, instance);
    }

    if ((components & 4) != 0)
    {
        SetInstanceRotationZ(z, instance);
    }
}

void PhysicsResetVelocityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if ((node->owner->flags & ReferencedObject::FlagPhysicsBody) == 0)
    {
        return;
    }

    ResetNodeVelocity(node);
    node->flags |= ObjectNodeBase::FlagKeepsTrajectory;
}

// From the last back to the first
void NextLinkedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* attachments = GetGameNode(&NodeOf(runner)->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    u32* word = LinkedObjectsWord(attachments);
    u32 index = *word >> LinkedIndexShift & LinkedIndexMask;
    u32 next = index == (*word & LinkedCountMask) - 1 ? 0 : index + 1;
    *word = (*word & ~(LinkedIndexMask << LinkedIndexShift)) | (next & LinkedIndexMask) << LinkedIndexShift;
}

void PhysicsSetGravityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node) && node->rigidBody != nullptr)
    {
        SetRigidBodyGravity(gravity, node->rigidBody);
    }
}

void PhysicsBodyResetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node) && node->rigidBody != nullptr)
    {
        ResetRigidBody(node->rigidBody);
    }
}

void PhysicsBodyActivateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node) && node->rigidBody != nullptr)
    {
        ActivateRigidBody(node->rigidBody);
    }
}

// The number counts from 1, past the count it's left as it is
void SetLinkedObjectIndexCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* attachments = GetGameNode(&NodeOf(runner)->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    u32* word = LinkedObjectsWord(attachments);
    u32 index = static_cast<u32>(number) - 1;
    if (index < (*word & LinkedCountMask))
    {
        *word = (*word & ~(LinkedIndexMask << LinkedIndexShift)) | (index & LinkedIndexMask) << LinkedIndexShift;
    }
}

// The agent's part's analog value, kept between -1 and 1
void SetCharacterAnalogCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentPart* part = NodeOf(runner)->agent->part;
    f32 analog = ClampFloat(value, -1.0f, 1.0f);
    *reinterpret_cast<f32*>(&part->unknown08) = analog;
}

void AddCharacterAnalogCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentPart* part = NodeOf(runner)->agent->part;
    auto* analog = reinterpret_cast<f32*>(&part->unknown08);
    *analog = ClampFloat(*analog + delta, -1.0f, 1.0f);
}

void DisableAllPerceptionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* perception = static_cast<u8*>(NodeOf(runner)->perception);
    if (perception != nullptr)
    {
        SwitchPerceptions(perception, 0);
    }
}

void EnableAllPerceptionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* perception = static_cast<u8*>(NodeOf(runner)->perception);
    if (perception != nullptr)
    {
        SwitchPerceptions(perception, 1);
    }
}

// The arguments after the ID are the motion block (when the node has none), the ID (when there's one) given to the node's
void AttachMotionBlockCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    MotionBlock* block = node->motionBlock;
    if (block == nullptr)
    {
        block = reinterpret_cast<MotionBlock*>(&block1);
        node->motionBlock = block;
        node->owner->flags |= ReferencedObject::FlagPhysicsBody;
        node->motionBlock->node = node;
    }

    if (id != 0)
    {
        block->unknown78 = static_cast<u32>(id);
        block->flags = (block->flags & ~MotionBlockFlag16) | MotionBlockFlag16;
    }
}

void NowMoveBackwardsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if ((instance->flags & BackwardsFlag) == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    // Retail looks the instance up and drops it
    void* path = *reinterpret_cast<void**>(static_cast<u8*>(attachments) + 0x70);
    if (path != nullptr)
    {
        UnslottedAttachment(path);
    }
}

void NotifyInstancesWithinCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    NotifyInstancesWithin(radius.FloatWith(node->PacketProperties()), node);
}

void SetSurfaceCommand::ExecuteOn(GameNode* node)
{
    ObjectCollision* collision = &node->owner->collision;
    if ((surface & AllHullsBit) != 0)
    {
        SetAllHullSurfaces(collision, static_cast<u16>(surface));
    }
    else
    {
        SetHullOwnSurface(collision, static_cast<u8>(surface >> HullShift), static_cast<u16>(surface));
    }
}

// The focus is the instance of the node it takes its object from (focus and flag set when there's one)
void SetFocusToOwnerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node->sourceNode == nullptr)
    {
        return;
    }

    InstanceContext* owner = node->sourceNode->owner;
    node->focusInstance = owner;
    if (owner != nullptr)
    {
        node->flags |= ObjectNodeBase::FlagFocusInstance;
    }

    node->flags &= ~ObjectNodeBase::FlagFocusPosition;
}

// The designator's instance takes its object from this node
void SetTargetOwnerToSelfCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 designator = static_cast<u8>(target.raw);
    if (designator == NoDesignator)
    {
        return;
    }

    InstanceContext* instance = DesignatorOf(node, designator);
    if (instance != nullptr)
    {
        static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, ObjectNodeKind))->sourceNode = node;
    }
}

// The node the designator's instance has (whether there's one isn't checked), or the agent's own with bit 8: it stops taking
// its object from another
void ResetTimerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    ObjectNodeBase* reset;
    u8 designator = static_cast<u8>(target);
    if (designator != NoDesignator)
    {
        reset = static_cast<ObjectNodeBase*>(GetGameNode(&DesignatorOf(node, designator)->nodes, ObjectNodeKind));
    }
    else
    {
        reset = (target & OwnTimerBit) != 0 ? node : nullptr;
    }

    if (reset != nullptr)
    {
        reset->sourceNode = nullptr;
    }
}

namespace
{
// The node kinds of a crate's and a creature's agent, and the rigid body's
constexpr u32 CrateNodeKind = 0xD;
constexpr u32 RigidBodyKind = 5;
constexpr u32 CreatureNodeKind = 0xF;
// A crate part's kind (bits 2-9 of its value) and a creature part's hit points (bits 6-13 of its flags)
constexpr u32 CrateKindShift = 2;
constexpr u32 CrateKindMask = 0xFF;
constexpr u32 HitPointsShift = 6;
constexpr u32 HitPointsMask = 0xFF;
// The node bit command 194 sets (bit 41 of the 64 bits from 0x150)
constexpr u64 NodeBit41 = u64{1} << 41;
constexpr u32 NodeBit41Request = 0x10000;
// The highest priority a script gives (past it the starter's own is taken)
constexpr u32 MostPriority = 100;
constexpr u32 PriorityMask = 0x7F;
// A space's request: the space (bits 16-19), two bytes and a flag (bit 20)
constexpr u32 SpaceShift = 16;
constexpr u32 SpaceMask = 0xF;
constexpr u32 SpaceFlagShift = 20;
// The persistent flag bits of a holder's state
constexpr u32 PersistentFlag = 0x40;
constexpr u32 FlagInChunkStore = 0x80;
}

extern "C"
{
    // The node's bytes 0x168 and up and a value set (the float first), its sounds stopped (still asm)
    void SetNodeBytes168(f32 value, ObjectNode* node, u32 first, u32 second) RETAIL_N32(FUN_0023e650);
    void StopNodeSounds(ObjectNode* node) RETAIL(FUN_0023e2c0);
    // A place for the runner's space of the request (still asm), and the node's stored place set to it
    ObjectPlace* SpaceOfRequest(BehaviourRunner* runner, u32 space, u32 second, u32 first, u32 flag) RETAIL(FUN_0022e0b8);
    void SetStoredPlace(ObjectNode* node, ObjectPlace* place) RETAIL(FUN_0023d8f8);
    // The chunk manager's chunk of an index, and an ID's flag of a store toggled (still asm)
    void* ChunkOfIndex(void* manager, u16 index) RETAIL(FUN_00268e60);
    void TogglePersistentFlag(void* store, u16 id) RETAIL(FUN_00269330);
    extern void* G_ChunkManager;
    // A crate's contents made (still asm)
    void CreateCrateContents(const CreateCrateContentsCommand* command, Agent* crate, InstanceContext* instance,
                             BehaviourRunner* runner) RETAIL(FUN_0010d358);
    // The byte command 209 sets
    extern u8 g_GlobalByte30A0E9 RETAIL(D_0030A0E9);
}

EABI_IMPORT(FUN_0023e650, SetNodeBytes168);

void SetNodeBytes168Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    SetNodeBytes168(value, node, bytes & 0xFF, bytes >> 8 & 0xFF);
    if ((bytes & NodeBit41Request) != 0)
    {
        *reinterpret_cast<u64*>(&node->unknown150) |= NodeBit41;
    }
}

void StopSoundCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node))
    {
        StopNodeSounds(node);
    }
}

// Half the sizes given, as a box centred on the instance (made a hull)
void SetCollisionBoxSizeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Vector4 extent;
    extent.x = x * 0.5f;
    extent.y = y * 0.5f;
    extent.z = z * 0.5f;
    extent.w = 1.0f;
    ObjectCollision* collision = &NodeOf(runner)->owner->collision;
    SetCentredCollisionBox(collision, &extent, 1, 0);
    StepObjectCollision(collision);
}

// The rigid body's mass, its size 1
void ScaleModelNodeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* body = static_cast<RigidBody*>(GetGameNode(&NodeOf(runner)->owner->nodes, RigidBodyKind));
    if (body != nullptr)
    {
        body->SetMassAndSize(value, 1.0f, 1.0f, 1.0f);
    }
}

void SetNodeByte8cCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    s32 number = value.IntWith(NodeOf(runner)->PacketProperties());
    NodeOf(runner)->unknown8C = static_cast<u8>(number);
}

void SetGlobalByte30a0e9Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    g_GlobalByte30A0E9 = static_cast<u8>(value.IntWith(NodeOf(runner)->PacketProperties()));
}

// The node's stored place (none when the request gives none), its angles worked out
void StoreCurrentSpaceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectPlace* place = SpaceOfRequest(runner, targetAndSpace >> SpaceShift & SpaceMask, targetAndSpace >> 8 & 0xFF,
                                        targetAndSpace & 0xFF, targetAndSpace >> SpaceFlagShift & 1);
    if (place != nullptr)
    {
        RotateAndTranslate(place);
        s32 angles[3];
        AnglesOfMatrix(&place->matrix, angles);
    }

    SetStoredPlace(NodeOf(runner), place);
}

// The agent's persistent flag (in its chunk's own store or the level's)
void ToggleStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Agent* agent = NodeOf(runner)->agent;
    PropertyHolder* properties = agent->properties;
    if ((properties->state & PersistentFlag) == 0)
    {
        return;
    }

    auto* chunk = static_cast<u8*>(ChunkOfIndex(G_ChunkManager, agent->chunkIndex));
    u16 id = agent->id;
    void* store = *reinterpret_cast<void**>(chunk + ((properties->state & FlagInChunkStore) != 0 ? 0x1C : 0x20));
    if (store != nullptr)
    {
        TogglePersistentFlag(store, id);
    }
}

// Past 100 the starter's own
void SetBehaviourPriorityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    StarterReceivers* receivers = runner->receivers;
    u32 priority = static_cast<u8>(priorityValue);
    if (priority > MostPriority)
    {
        priority = receivers->starter->bits >> ScriptResource::PriorityShift & 0xFF;
    }

    receivers->bits = (receivers->bits & ~PriorityMask) | (priority & PriorityMask);
}

// The instance's crate agent (whether it has one isn't checked)
void CreateCrateContentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CrateNodeKind));
    CreateCrateContents(this, node->agent, instance, runner);
}

// A kind of crate (none below 0)
void SetCrateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    if (static_cast<s32>(value2) < 0)
    {
        return;
    }

    auto* node = static_cast<AgentNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, CrateNodeKind));
    if (node == nullptr)
    {
        return;
    }

    auto* part = static_cast<CratePart*>(node->agent->part);
    part->value = (part->value & ~(CrateKindMask << CrateKindShift)) | (value2 & CrateKindMask) << CrateKindShift;
}

// None left below 0 (whether the instance has a creature agent isn't checked)
void ReduceHitPointsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, CreatureNodeKind));
    auto* part = static_cast<CreaturePart*>(node->agent->part);
    s32 left = static_cast<s32>(part->unknown14 >> HitPointsShift & HitPointsMask) - hitPoints;
    if (left < 0)
    {
        left = 0;
    }

    part->unknown14 = (part->unknown14 & ~(HitPointsMask << HitPointsShift)) | (left & HitPointsMask) << HitPointsShift;
}

namespace
{
constexpr u32 CharacterNodeKind = 0xC;
constexpr u32 FollowNodeKind = 0x16;
// The node's vtable function told a runner's packet ended, and the one command 3 (DestroyMe) calls; the instance's that puts it
// to sleep
constexpr u32 PacketEndedSlot = 42;
constexpr u32 NodeSlot11 = 11;
constexpr u32 InstanceSleepSlot = 3;
// DestroyMe's modes that do anything (bits 0-2 below 3)
constexpr u32 DestroyModeMask = 0x7;
constexpr u32 DestroyModes = 3;
// The motion block's flag command 581 sets (bit 13)
constexpr u32 MotionBlockNormalFlag = 0x2000;
// Attach all's own slot (bits 1-6 of the argument with bit 0)
constexpr u32 AttachSlotShift = 1;
constexpr u32 AttachSlotMask = 0x3F;
// The character agent's flag command 660 clears (bit 14 of its 64 bits at 0x70)
constexpr u64 CharacterFlag14 = 0x4000;
// The values node value 174 is kept within, and below which it counts as none
constexpr f32 MostValue174 = 1.0f;
constexpr f32 NoValue174 = Rounded(1e-06);

InstanceContext* PlayedCharacter()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->Field(GameProgress::CharacterShift));
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// The instance's playable character agent's node, else the player's
AgentNode* CharacterNodeOf(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (node == nullptr)
    {
        InstanceContext* player = PlayerInstance();
        if (player != nullptr)
        {
            node = static_cast<AgentNode*>(GetGameNode(&player->nodes, CharacterNodeKind));
        }
    }

    return node;
}
}

extern "C"
{
    // The attachments node's linked agents attached (to one slot of theirs, or all), a motion block made normal, an agent
    // restarted, the follow node's camera given its defaults, the character's hold released (still asm)
    void AttachLinkedAgentsToSlot(void* attachments, InstanceContext* instance, u32 slot, u32 unknown) RETAIL(FUN_00196c08);
    void AttachLinkedAgents(void* attachments, InstanceContext* instance, u32 unknown) RETAIL(FUN_00196b78);
    void MakeMotionBlockNormal(MotionBlock* block) RETAIL(FUN_0023f9e0);
    void RestartAgent(Agent* agent) RETAIL(FUN_002634c8);
    void RestoreCameraDefaults(FollowNode* node) RETAIL(FUN_0017b5f0);
    void ReleaseCharacterHold(Agent* character) RETAIL(FUN_00132f20);
    // An instance's places dismissed, and placed in a chunk (still asm)
    void DismissPlaces(InstancePlaces* places) RETAIL(FUN_00198a18);
    void PlacePlacesInChunk(InstancePlaces* places, InstanceContext* instance, ChunkData* chunk) RETAIL(FUN_00198a50);
    // A chunk's sound set from a command's arguments, in one of two ways (still asm)
    void SetChunkSoundA(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2560);
    void SetChunkSoundB(ChunkData* chunk, const void* arguments) RETAIL(FUN_001f2500);
}

void AttachAllLinkedAgentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    if ((value1 & 1) != 0)
    {
        AttachLinkedAgentsToSlot(attachments, instance, value1 >> AttachSlotShift & AttachSlotMask, 0);
    }
    else
    {
        AttachLinkedAgents(attachments, instance, 0);
    }
}

void DetachAllLinkedAgentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    ReleaseAttachmentsNode(attachments, 1, 1, 0);
    instance->flags &= ~BackwardsFlag;
}

void SetFocusToPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* player = PlayedCharacter();
    if (player == nullptr)
    {
        return;
    }

    ObjectNode* node = NodeOf(runner);
    node->focusInstance = player;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
}

// Whether the played character has a follow node isn't checked
void RestoreCameraDefaultsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    RestoreCameraDefaults(static_cast<FollowNode*>(GetGameNode(&PlayedCharacter()->nodes, FollowNodeKind)));
}

void SetAgentRef1ToPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr)
    {
        return;
    }

    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node))
    {
        node->agentRef1 = player;
    }
}

void BecomeNormalCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr || node->motionBlock == nullptr)
    {
        return;
    }

    MotionBlock* block = node->motionBlock;
    block->flags = (block->flags & ~MotionBlockNormalFlag) | ((value1 & 1) != 0 ? MotionBlockNormalFlag : 0);
    if ((value1 & 1) == 0)
    {
        MakeMotionBlockNormal(block);
    }
}

void ReleasePlayerHoldCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentNode* character = CharacterNodeOf(NodeOf(runner)->owner);
    if (character != nullptr)
    {
        ReleaseCharacterHold(character->agent);
    }
}

void DismissCharacterCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    InstanceContext* instance = G_GameController->progress.Instance(static_cast<u32>(value1));
    InstancePlaces* places = instance != nullptr ? instance->places : nullptr;
    if (places != nullptr)
    {
        DismissPlaces(places);
    }
}

// The played character's agent's home chunk is the one it's in (whether there's a character isn't checked)
void SetCharacterHomeChunkCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    InstanceContext* player = PlayedCharacter();
    auto* node = static_cast<AgentNode*>(GetGameNode(&player->nodes, CharacterNodeKind));
    *reinterpret_cast<ChunkData**>(reinterpret_cast<u8*>(node->agent) + 0x90) = player->chunk;
}

// In the chunk of the agent's instance
void PlaceCharacterInChunkCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = G_GameController->progress.Instance(static_cast<u32>(character));
    InstancePlaces* places = instance != nullptr ? instance->places : nullptr;
    if (places != nullptr)
    {
        PlacePlacesInChunk(places, instance, NodeOf(runner)->owner->chunk);
    }
}

// The pairing, the character and the second one
void SetPlayerModeCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    constexpr u32 Field = GameProgress::FieldMask;
    GameProgress* progress = &G_GameController->progress;
    progress->bits = (progress->bits & ~(Field << GameProgress::PairingShift)) | (value1 & Field) << GameProgress::PairingShift;
    progress->bits = (progress->bits & ~(Field << GameProgress::CharacterShift)) | (value2 & Field) << GameProgress::CharacterShift;
    progress->bits = (progress->bits & ~(Field << GameProgress::SecondShift)) | (value3 & Field) << GameProgress::SecondShift;
}

void PlayMovieCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    s32 movie = value.IntWith(NodeOf(runner)->PacketProperties());
    G_GameController->RequestMovie(static_cast<s32>(value2 * g_ClockUnitsPerSecond), static_cast<u32>(movie));
}

// The level finished, the node told its packet ended, the agent restarted
void RestartDefaultBehaviourCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    ObjectNode* node = NodeOf(runner);
    Agent* agent = node->agent;
    level->bits |= BehaviourLevel::Finished;
    CallVirtual<void>(node, node->vtable, PacketEndedSlot, runner);
    RestartAgent(agent);
}

// At most 1; a value set where there was none counts it among the instances' (a new 0 too)
void SetNodeValue174Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* stored = reinterpret_cast<f32*>(reinterpret_cast<u8*>(NodeOf(runner)) + 0x174);
    f32 old = *stored;
    *stored = value;
    if (MostValue174 < value)
    {
        *stored = MostValue174;
    }

    if (old < NoValue174)
    {
        g_InstancesWithValue174++;
    }
}

void ClearPlayerFlag14Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentNode* character = CharacterNodeOf(NodeOf(runner)->owner);
    if (character != nullptr)
    {
        *reinterpret_cast<u64*>(reinterpret_cast<u8*>(character->agent) + 0x70) &= ~CharacterFlag14;
    }
}

// Whether the played character has a follow node isn't checked
void ResetCameraCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    auto* node = static_cast<FollowNode*>(GetGameNode(&PlayedCharacter()->nodes, FollowNodeKind));
    ResetFollowCamera(node->camera);
}

void ClearContactResponseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (TakesPackets(node))
    {
        node->ReleaseMotionBlock();
    }
}

void SetSoundCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ChunkData* chunk = NodeOf(runner)->owner->chunk;
    if (chunk == nullptr)
    {
        return;
    }

    if ((value7 & 1) != 0)
    {
        SetChunkSoundA(chunk, &value1);
    }
    else
    {
        SetChunkSoundB(chunk, &value1);
    }
}

// Its node's function 11 and its instance put to sleep
void DestroyMeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if ((mode & DestroyModeMask) >= DestroyModes)
    {
        return;
    }

    CallVirtual<void>(node, node->vtable, NodeSlot11);
    CallVirtual<u32>(instance, instance->vtable, InstanceSleepSlot);
}
