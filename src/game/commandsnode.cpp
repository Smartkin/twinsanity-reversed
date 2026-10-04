#include "game/commands.h"

#include "game/agents.h"
#include "game/animation.h"
#include "game/attachments.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/decals.h"
#include "game/events.h"
#include "game/followcamera.h"
#include "game/instancefactory.h"
#include "game/gamecontroller.h"
#include "game/instanceparticles.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/resources.h"
#include "game/rigidbody.h"
#include "game/shadows.h"
#include "game/camerarig.h"
#include "game/collision.h"
#include "game/layout.h"
#include "game/sound.h"

#include <utility>

// The commands that work on the agent's object node, its runner and its levels

extern "C"
{
    // The velocity commands' work: a velocity given to an instance (from another's point of view)
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
        TurnSenseOff(perception, LowByte(slot));
    }
}

void PerceptionOp142Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        TurnSenseOn(perception, LowByte(slot));
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
    // A rigid body made active (game/commandsphysics.cpp)
    void ActivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00246950);
}

void SetRotationComponentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if ((components & 1) != 0)
    {
        SnapInstanceRotationX(x, instance);
    }

    if ((components & 2) != 0)
    {
        SnapInstanceRotationY(y, instance);
    }

    if ((components & 4) != 0)
    {
        SnapInstanceRotationZ(z, instance);
    }
}

void PhysicsResetVelocityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if ((node->owner->flags & ReferencedObject::FlagPhysicsBody) == 0)
    {
        return;
    }

    FollowOwnMotionBlock(node);
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
        DeactivateRigidBody(node->rigidBody);
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
    // The chunk manager's chunk of an index, and an ID's flag of a store toggled
    extern void* G_ChunkManager;
    // A crate's contents made
    void CreateCrateContents(const CreateCrateContentsCommand* command, Agent* crate, InstanceContext* instance,
                             BehaviourRunner* runner) RETAIL(FUN_0010d358);
    // The byte command 209 sets
    extern u8 g_GlobalByte30A0E9 RETAIL(D_0030A0E9);
}

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

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* store = (properties->state & FlagInChunkStore) != 0 ? chunk->flags : chunk->otherFlags;
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
    s32 left = static_cast<s32>(part->flags >> HitPointsShift & HitPointsMask) - hitPoints;
    if (left < 0)
    {
        left = 0;
    }

    part->flags = (part->flags & ~(HitPointsMask << HitPointsShift)) | (left & HitPointsMask) << HitPointsShift;
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
    // The character's hold released
    void ReleaseCharacterHold(Agent* character) RETAIL(FUN_00132f20);
    // A chunk's sound set from a command's arguments, in one of two ways
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
        DismissPlaces(places, instance);
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
    node->camera.rig.Restart();
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
        SetChunkBoxReverb(chunk, &value1);
    }
    else
    {
        SetChunkReverb(chunk, &value1);
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


namespace
{
constexpr u32 MovementNodeKind = 0;
constexpr u32 ModelNodeKindValue = 3;
constexpr u32 RigidBodyNodeKind = 5;
constexpr u32 ShadowNodeKind = 10;

// A two bit switch of the arguments: 1 sets, 2 clears, 0 and 3 leave
template <typename T>
void Switch(u32 setting, T* word, T bit)
{
    if (setting == 1)
    {
        *word |= bit;
    }
    else if (setting == 2)
    {
        *word &= ~bit;
    }
}
}

// The physics body's flags 0x2000, 0x8000, 0x10000 and 0x1000 switched by the argument's two bit fields, 0x30 always set
void SetNode5FlagsCommand::ExecuteOn(GameNode* node)
{
    auto* body = static_cast<RigidBody*>(GetGameNode(&node->owner->nodes, RigidBodyNodeKind));
    if (body == nullptr)
    {
        return;
    }

    u32 bits = static_cast<u32>(flags.raw);
    u32 bodyFlags = body->bodyFlags;
    Switch(bits & 3, &bodyFlags, 0x2000u);
    Switch(bits >> 2 & 3, &bodyFlags, 0x8000u);
    Switch(bits >> 4 & 3, &bodyFlags, 0x10000u);
    Switch(bits >> 6 & 3, &bodyFlags, 0x1000u);
    body->bodyFlags = bodyFlags | 0x30;
}

// The shadow node's byte at 0x18
void ShadowToggleCommand::ExecuteOn(GameNode* node)
{
    auto* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, ShadowNodeKind));
    if (shadow != nullptr)
    {
        *(reinterpret_cast<u8*>(shadow) + 0x18) = static_cast<u8>(slot);
    }
}

void SetNode10SlotCommand::ExecuteOn(GameNode* node)
{
    // The slot's shadow deleted
    auto* shadow = static_cast<ShadowNode*>(GetGameNode(&node->owner->nodes, ShadowNodeKind));
    if (shadow != nullptr)
    {
        shadow->ClearSlot(slot & 0xFF);
    }
}

// Two bit switches of the instance and its nodes (1 on, 2 off): its flags 0x100, 0x400 and 0x10, its movement node's bit, its
// physics body's flags 0x20 and 0x40, its model's collision bit; asleep or awake (1 woken, 2 its node's function 11 and the
// instance put to sleep); the node's and the model's squared distances from the bytes at bits 21-28 and 13-20 (0xFF: none)
void SetObjectCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 WakeSlot = 2;
    constexpr u32 SleepSlot = 3;
    constexpr u32 NodeSleepSlot = 11;
    InstanceContext* instance = node->owner;
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNodeKindValue));
    Switch(flags & 3, &instance->flags, 0x100u);
    u32 setting = flags2 >> 2 & 3;
    if (setting == 1 || setting == 2)
    {
        auto* movement = static_cast<MovementNode*>(GetGameNode(&instance->nodes, MovementNodeKind));
        if (movement != nullptr)
        {
            Switch(setting, &movement->bits, 1u);
        }
    }

    for (auto [field, bit] : {std::pair{flags >> 10 & 3, 0x20u}, std::pair{flags2 & 3, 0x40u}})
    {
        if (field == 1 || field == 2)
        {
            auto* body = static_cast<RigidBody*>(GetGameNode(&instance->nodes, RigidBodyNodeKind));
            if (body != nullptr)
            {
                Switch(field, &body->bodyFlags, bit);
            }
        }
    }

    Switch(flags >> 2 & 3, &instance->flags, 0x400u);
    Switch(flags >> 4 & 3, &instance->flags, 0x10u);
    u32 sleep = (flags & 0xFF) >> 6;
    if (sleep == 1)
    {
        CallVirtual<void>(instance, instance->vtable, WakeSlot);
    }
    else if (sleep == 2)
    {
        CallVirtual<void>(node, node->vtable, NodeSleepSlot);
        CallVirtual<void>(instance, instance->vtable, SleepSlot);
    }

    u32 collision = flags2 >> 4 & 3;
    if (collision == 1)
    {
        model->SetSolid(1);
    }
    else if (collision == 2)
    {
        model->SetSolid(0);
    }

    if ((flags & 0x40000000) != 0)
    {
        u32 distance = flags >> 21 & 0xFF;
        node->unknown06 = static_cast<u16>(distance * distance);
        if ((flags & 0x1FE00000) == 0x1FE00000)
        {
            node->unknown06 = 0xFFFF;
        }
    }

    if ((flags & 0x20000000) != 0)
    {
        u32 distance = flags >> 13 & 0xFF;
        model->unknown06 = static_cast<u16>(distance * distance);
        if ((flags & 0x1FE000) == 0x1FE000)
        {
            model->unknown06 = 0xFFFF;
        }
    }
}

void DoAnimationCommand::Play(PropertyHolder* properties, GameAnimation* animation, OgiAnimator* animator)
{
    f32 blend = blendTime.FloatWith(properties);
    if (animation == nullptr)
    {
        StopOgiAnimation(animator, static_cast<s32>(blend * g_ClockUnitsPerSecond), 0xFF);
        return;
    }

    f32 rate = speed.FloatWith(properties);
    if ((flags & 0x4000) != 0)
    {
        if ((flags & 0x10000) != 0)
        {
            rate += RandomSignedTimes(speedRandom.FloatWith(properties));
        }
    }
    else if ((flags & 0x8000) != 0)
    {
        // The speed is how long it lasts: its frames at its rate divided by that
        u32 bits = animation->bits;
        rate = (static_cast<f32>(static_cast<s32>(bits >> GameAnimation::FramesShift & GameAnimation::FramesMask)) /
                static_cast<f32>(static_cast<s32>(bits >> GameAnimation::RateShift & GameAnimation::RateMask))) / rate;
    }
    else
    {
        rate = 1.0f;
    }

    AnimationSettings settings;
    ConstructAnimationSettings(rate, &settings, animation);
    u32 bits = settings.bits;
    bits = (bits & ~AnimationSettings::Loops) | (flags >> 12 & 1);
    bits = (bits & ~AnimationSettings::Backward) | (flags >> 17 & AnimationSettings::Backward);
    bits = (bits & ~AnimationSettings::QueuedAlways) | (flags >> 15 & AnimationSettings::QueuedAlways);
    settings.bits = bits;
    if ((flags & 0x2000) != 0)
    {
        s32 ticks = static_cast<s32>(blend * g_ClockUnitsPerSecond);
        settings.blendTime = ticks;
        settings.bits = (bits & ~AnimationSettings::Queued) | static_cast<u32>(ticks != 0) << 1;
    }

    if ((flags & 0x40000) != 0)
    {
        settings.bits = (settings.bits & ~AnimationSettings::StartsPartWay) | AnimationSettings::Continues;
    }
    else if ((flags & 0x20000) != 0)
    {
        settings.start = startPosition;
        settings.bits = (settings.bits | AnimationSettings::StartsPartWay) & ~AnimationSettings::Continues;
    }

    PlayOgiAnimation(animator, &settings, static_cast<u32>(flags) >> 4 & 0xFF);
    DestroyAnimationSettings(&settings, DestroyOnly);
}

void DoAnimationCommand::ExecuteOn(GameNode* node)
{
    auto* agent = static_cast<ObjectNode*>(node);
    GameObject* object = agent->sourceNode != nullptr ? SourceObject(agent->sourceNode) : agent->object;
    InstanceContext* instance = agent->owner;
    const u8* slots = reinterpret_cast<const u8*>(&animSlots);
    u32 slot = (flags & 0xF) == 1 ? slots[0] : slots[RandomBelow(flags & 0xF)];
    u16 modelId;
    GetObjectModelId(&modelId, object, slot);
    if (modelId != 0xFFFF)
    {
        GameResources* resources = G_GameResourcesObjectPointer;
        ResourceTable* models = resources->models;
        u16 id = modelId;
        auto* ogi = id != 0xFFFF ? static_cast<GameOGI*>(models->items[id & 0x7FFF]) : nullptr;
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
        PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                                  : agent->properties;
        u16 animationId;
        GetObjectAnimationId(&animationId, object, slot);
        u32 header = object->header[0];
        model->SetOgi(ogi, header >> 6 & 0x3F, header & 0x3F);
        OgiAnimator* animator = model->animator;
        if (animator != nullptr)
        {
            GameAnimation* animation = nullptr;
            if (animationId != 0xFFFF)
            {
                ResourceTable* animations = resources->animations;
                id = animationId;
                animation = id != 0xFFFF ? static_cast<GameAnimation*>(animations->items[id & 0x7FFF]) : nullptr;
            }

            Play(properties, animation, animator);
        }
    }

    if ((flags & 0x200000) != 0)
    {
        auto* shadow = static_cast<GameNode*>(GetGameNode(&instance->nodes, ShadowNodeKind));
        if (shadow != nullptr)
        {
            *(reinterpret_cast<u8*>(shadow) + 0x18) = static_cast<u8>(flags >> 22 & 0xF);
        }
    }
}

void DoParticleCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 GetDesignatorPositionSlot = 37;
    auto* agent = static_cast<ObjectNode*>(node);
    InstanceContext* instance = agent->owner;
    s32 system = systemAndFlags & SystemMask;
    auto* position = reinterpret_cast<Vector4*>(&posX);
    if ((flags2 >> KeyShift & NoneByte) != NoneByte)
    {
        Waypoints* waypoints = agent->waypoints;
        u32 index = flags2 >> KeyShift & NoneByte;
        LayoutPosition* key = nullptr;
        if (waypoints->keyCount != 0)
        {
            key = static_cast<s32>(index) < waypoints->keyCount ? waypoints->positions.data[index] : nullptr;
        }

        // A key past the last is read from address 0, as retail does
        *position = key->position;
        position->w = 1.0f;
        systemAndFlags &= ~FromKeyPending;
        flags2 |= HasPosition;
    }

    // The surface mode plays the system of the surface's index, the contact's (with a decal at its point) or the one under the
    // node, as retail does (the mode's byte goes unread)
    const Vector4* contact = nullptr;
    if ((flags2 >> SurfaceShift & NoneByte) != NoneByte && CallVirtual<u32>(agent, agent->vtable, TakesPacketsSlot) != 0)
    {
        system = agent->unknown134;
        if (agent->unknown134 == -1)
        {
            system = agent->surface;
        }
        else
        {
            contact = &agent->unknown140;
        }
    }

    u32 value = systemAndFlags >> ValueShift & ValueMask;
    if ((systemAndFlags & FromFrame) != 0)
    {
        s32 emitter = StartEmitterKeepingTranslation(instance, system, value, nullptr);
        if (emitter < 0)
        {
            return;
        }

        Matrix4x4 frame = *ParticleFrame(instance, systemAndFlags >> ExitPointShift & ExitPointMask);
        OrientParticleFrame(flags2 & Turned, flags2 >> AxesShift & AxesMask, (flags2 & HasPosition) != 0 ? position : nullptr,
                            &frame);
        if (contact != nullptr)
        {
            *RowOf(&frame, 3) = *contact;
            AddDecalFromDescriptor(&frame, instance->chunk);
        }

        SetEmitterFrame(emitter, &frame);
        return;
    }

    if ((flags2 >> DesignatorShift & NoneByte) != NoneByte)
    {
        u32 designator = flags2 >> DesignatorShift & NoneByte;
        auto* target = CallVirtual<InstanceContext*>(agent, agent->vtable, GetDesignatorSlot, designator);
        if (target != nullptr)
        {
            ObjectPlace* place = target->place;
            place->SyncPosition();
            *position = place->position;
        }
        else
        {
            CallVirtual<u32>(agent, agent->vtable, GetDesignatorPositionSlot, designator, position);
        }
    }

    if (contact == nullptr)
    {
        StartEmitterKeepingTranslation(instance, system, value, (flags2 & HasPosition) != 0 ? position : nullptr);
        return;
    }

    Matrix4x4 frame;
    InitIdentityMatrix(&frame);
    *RowOf(&frame, 3) = *contact;
    AddDecalFromDescriptor(&frame, instance->chunk);
    StartEmitterKeepingTranslation(instance, system, value, contact);
}

namespace
{
// The node's vtable function saying it has no tracked sound slot in use
constexpr u32 NoTrackedSoundSlot = 40;
// The agent's vtable functions told of a step on a surface: whether to be, and the surface with the up direction
constexpr u32 AgentSurfaceStepsSlot = 12;
constexpr u32 AgentSurfaceStepSlot = 18;
// The surface whose landings the agent is told of
constexpr u32 SteppedSurfaceId = 10;
constexpr u32 LandKind = 3;
// Sounds aren't played by instances whose stamp is this far along unless they're followed
constexpr u32 FarStamp = 0x1FA4;

u8& TrackedSound(ObjectNode* node)
{
    return node->unknown155[0x160 - 0x155];
}
}

u16 SoundOfSlot(ObjectNode* node, u32 slot)
{
    slot &= 0xFFFF;
    u16 id = 0xFFFF;
    if (slot == 0xFFFF)
    {
        return id;
    }

    GameObject* object = node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
    u16 found;
    GetObjectSoundId(&found, object, slot);
    if (found != 0xFFFF)
    {
        return found & 0x7FFF;
    }

    GameObject* own = node->OwnObject();
    if (own != nullptr)
    {
        GetObjectSoundId(&found, own, slot);
        if (found != 0xFFFF)
        {
            id = found & 0x7FFF;
        }
    }

    return id;
}

void PlaySlotSound(ObjectNode* node, u32 slot, u32 tracked)
{
    InstanceContext* instance = node->owner;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    u32 channel = 0xFF;
    u16 sound = SoundOfSlot(node, slot);
    if (sound == 0xFFFF)
    {
        return;
    }

    s32 kept = -1;
    s32 voiceKind = ListenerVoiceKind();
    if (tracked == 0 || CallVirtual<u32>(node, node->vtable, NoTrackedSoundSlot) == 0)
    {
        PlaySoundByIdAt(-1.0f, -1.0f, sound, 0, instance->chunk, &position, voiceKind, -1);
    }
    else
    {
        kept = 0;
        channel = PlayInstanceSoundById(-1.0f, -1.0f, sound, 0, instance, voiceKind, 0);
    }

    if (channel != 0xFF && tracked != 0 && kept == 0)
    {
        TrackedSound(node) = static_cast<u8>(channel);
    }
}

void DoSoundCommand::ExecuteOn(GameNode* node)
{
    constexpr f32 LoudestVolume = 5.0f;
    constexpr f32 ShakeThreshold = Rounded(0.01);
    auto* agent = static_cast<ObjectNode*>(node);
    f32 volumeGiven = -1.0f;
    if ((flags & HasVolume) != 0)
    {
        PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                                  : agent->properties;
        volumeGiven = volume.FloatWith(properties);
        if (LoudestVolume < volumeGiven)
        {
            volumeGiven = LoudestVolume;
        }
    }

    InstanceContext* instance = agent->owner;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    u32 sound = 0xFFFF;
    u32 channel = 0xFF;
    if ((flags & KindMask) == KindMask || CallVirtual<u32>(agent, agent->vtable, TakesPacketsSlot) == 0)
    {
        s32 slot = RandomBelow(static_cast<s32>(flags & SlotCountMask));
        sound = SoundOfSlot(agent, soundSlots[slot]);
    }
    else
    {
        s32 surface = agent->unknown134;
        if (surface == -1)
        {
            surface = agent->surface;
        }

        if (surface != -1)
        {
            CollisionSurface* contact = &g_CollisionSurfaces.surfaces[surface];
            sound = GetSurfaceSound(contact, flags >> KindShift & 0xF, &volumeGiven);
            if ((flags & KindMask) == LandKind << KindShift)
            {
                Agent* owner = agent->agent;
                if (CallVirtual<u32>(owner, owner->vtable, AgentSurfaceStepsSlot) != 0 && contact->surfaceId == SteppedSurfaceId)
                {
                    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
                    CallVirtual<void>(owner, owner->vtable, AgentSurfaceStepSlot, contact, &up);
                }
            }
        }
    }

    if (sound != 0xFFFF)
    {
        u32 stamp = instance->seen[0] | instance->seen[1] << 8 | instance->seen[2] << 16;
        if (stamp < FarStamp || (flags & Followed) != 0)
        {
            f32 level = 1.0f;
            bool hasLevel = false;
            if (0.0f < volumeGiven)
            {
                level = volumeGiven;
                hasLevel = true;
            }

            if ((flags & RandomVolume) != 0)
            {
                hasLevel = true;
                level = level + RandomSignedTimes(volumeRandom);
            }

            f32 scale = 1.0f;
            bool hasScale = false;
            if ((flags & HasPitch) != 0)
            {
                scale = pitch;
                hasScale = true;
            }

            if ((flags & RandomPitch) != 0)
            {
                hasScale = true;
                scale = scale + RandomSignedTimes(pitchRandom);
            }

            s32 voiceKind = ListenerVoiceKind();
            s32 kept = -1;
            if ((flags & Tracked) != 0 && CallVirtual<u32>(agent, agent->vtable, NoTrackedSoundSlot) != 0)
            {
                kept = 0;
            }

            u32 group = flags >> GroupShift & GroupMask;
            f32 played = hasLevel ? level : -1.0f;
            f32 playedScale = hasScale ? scale : -1.0f;
            if ((flags & Unplaced) != 0)
            {
                if (kept == 0)
                {
                    channel = PlayUnplacedSoundById(played, playedScale, sound, group, instance, voiceKind);
                }
                else
                {
                    PlaySoundById(played, playedScale, sound, static_cast<s32>(group), voiceKind, kept);
                }
            }
            else if ((flags & Followed) != 0)
            {
                channel = PlayInstanceSoundById(played, playedScale, sound, group, instance, voiceKind, kept);
            }
            else
            {
                PlaySoundByIdAt(played, playedScale, static_cast<u16>(sound), static_cast<s32>(group), instance->chunk, &position,
                                voiceKind, kept);
            }

            if (channel != 0xFF && (flags & Tracked) != 0 && kept == 0)
            {
                TrackedSound(agent) = static_cast<u8>(channel);
            }
        }
    }

    if ((flags & Shakes) == 0)
    {
        return;
    }

    if (ShakeThreshold < shakeAcross || ShakeThreshold < shakeUp)
    {
        PushCameraShakeAxes(&g_CameraShake, &position, shakeAcross, shakeUp, shakeAcross, shakeFalloff);
    }
    else
    {
        PushCameraShake(&g_CameraShake, &position, shakeStrength, shakeFalloff);
    }
}

void BeginMusicCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 SlotShift = 12;
    constexpr u32 SlotMask = 0x7;
    constexpr u32 EmitterSlot = 3;
    auto* agent = static_cast<ObjectNode*>(node);
    PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                              : agent->properties;
    s32 number = track.IntWith(properties);
    // Above its track the request's word is what the stack held, which nothing reads; group 2, at once, the loop
    MusicRequest request;
    request.bits = (static_cast<u32>(number) & 0xFFFF) | 0x80000 | 0x20000 | (flags >> 15 & 1) << 20;
    request.left = volume;
    request.right = volume;
    request.fadeTime = fadeTime;
    if ((flags & SlotMask << SlotShift) == 0)
    {
        // The main slot's tracks play at their own volume
        f32 level;
        if (static_cast<u32>(number - 0x1D) < 2 || number == 0x23 || number == 0x36 || number == 0x37 || number == 0x3D)
        {
            level = 1.0f;
        }
        else
        {
            level = number == 0x3B ? Rounded(0.8) : 0.75f;
        }

        request.left = level;
        request.right = level;
    }

    u32 slot = flags >> SlotShift & SlotMask;
    if (slot == 2)
    {
        request.bits &= ~0x70000u;
    }
    else if (slot > 1)
    {
        if (slot != EmitterSlot)
        {
            return;
        }

        request.bits &= ~0x70000u;
        AddMusicEmitter(range, agent->owner, &request);
        return;
    }

    PlayMusicRequest(slot, &request);
}

void CreateDamageCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 ShapeHullShift = 4;
    constexpr u32 NoHull = 0xF;
    constexpr u32 NoJoint = 0xFF;
    constexpr u32 MostHit = 0x40;
    constexpr u32 ContactSlot = 9;
    auto* agent = static_cast<ObjectNode*>(node);
    u32 found = 0;
    ContactMessage message;
    ContactMessage::Construct(&message);
    PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                              : agent->properties;
    InstanceContext* instance = agent->owner;
    ChunkData* chunk = instance->chunk;
    u32 kinds = 0x5F000;
    f32 radius = reach.FloatWith(properties);
    if ((flags & SearchKinds5E) != 0)
    {
        kinds = 0x5E000;
    }
    else if ((flags & SearchKinds1000) != 0)
    {
        kinds = 0x1000;
    }

    void* results[MostHit];
    InstanceRayHit query;
    query.most = MostHit;
    query.results = results;
    query.count = 0;
    query.distance = 1e30f;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.wantedFlags = 0;
    query.bits = (query.bits & ~InstanceRayHit::BitFull) | InstanceRayHit::BitAllWanted;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, instance);
    Vector4 position;
    u32 joint = flags >> JointShift & 0xFF;
    if (joint != NoJoint)
    {
        JointPosition(instance, joint, &position, nullptr);
    }
    else
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        position = place->position;
        if ((flags & Offset) != 0)
        {
            place = instance->place;
            RotateAndTranslate(place);
            const Vector4* rowX = RowOf(&place->matrix, 0);
            const Vector4* rowY = RowOf(&place->matrix, 1);
            const Vector4* rowZ = RowOf(&place->matrix, 2);
            position.z = position.z + ((rowX->z * x + rowY->z * y) + rowZ->z * z);
            position.x = position.x + ((rowX->x * x + rowY->x * y) + rowZ->x * z);
            position.y = position.y + ((rowX->y * x + rowY->y * y) + rowZ->y * z);
        }
    }

    u32 hull = shape >> ShapeHullShift & 0xF;
    if (hull != NoHull)
    {
        found = InstancesInDamageHull(instance, hull, kinds, &query);
    }
    else
    {
        switch (shape & 0xF)
        {
        case 1:
            position.w = radius;
            found = ChunkInstancesInSphere(chunk, &position, kinds, &query, 1);
            break;
        case 2:
            position.w = radius;
            found = ChunkInstancesInSphere(chunk, &position, kinds, &query, 0);
            break;
        case 3:
        {
            f32 tall = height.FloatWith(properties);
            position.w = radius;
            found = ChunkInstancesInCylinder(tall, chunk, &position, kinds, &query);
            position.w = 1.0f;
            break;
        }
        default:
            break;
        }
    }

    message.byte = static_cast<u8>(damage.IntWith(properties));
    message.word = hitKinds;
    if ((flags & Bit10Kind) != 0)
    {
        message.word = hitKinds | 0x400;
    }

    if ((flags & GivesW) != 0)
    {
        message.point.w = messageW.FloatWith(properties);
    }

    auto send = [&](void* hit) {
        AgentNode* hitNode = AgentNodeOf(static_cast<InstanceContext*>(hit));
        if (hitNode != nullptr)
        {
            CallVirtual<void>(hitNode->agent, hitNode->agent->vtable, ContactSlot, &message, instance, 1u);
        }
    };
    if ((flags & NearestOnly) == 0)
    {
        for (u16 index = 0; index < found; index++)
        {
            send(results[index]);
        }

        return;
    }

    InstanceContext* nearest = nullptr;
    f32 best = 1e30f;
    for (u16 index = 0; index < found; index++)
    {
        auto* hit = static_cast<InstanceContext*>(results[index]);
        const Box* box = hit->CollisionBox();
        f32 dx = ((box->max.x - box->min.x) * 0.5f + box->min.x) - position.x;
        f32 dy = ((box->max.y - box->min.y) * 0.5f + box->min.y) - position.y;
        f32 dz = ((box->max.z - box->min.z) * 0.5f + box->min.z) - position.z;
        f32 distance = (dx * dx + dy * dy) + dz * dz;
        if (distance < best)
        {
            nearest = hit;
            best = distance;
        }
    }

    if (nearest != nullptr)
    {
        send(nearest);
    }
}

namespace
{
// A projectile's node keeps its own state where an object node keeps its frame's move: its state bits, the trigger message it
// sends, the references to its target and to who shot it
struct ProjectileState
{
    u32 bits;
    u16 message;
    u16 unknown06;
    Reference* target;
    Reference* shooter;
};

ProjectileState* ProjectileOf(ObjectNodeBase* node)
{
    return reinterpret_cast<ProjectileState*>(&node->unknownC0);
}
}

extern "C"
{
    // A projectile node's state changed
    void SetProjectileState(ObjectNodeBase* node, TimeClock* clock, u32 state) RETAIL(FUN_0010a6e8);
}

void ShootCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 NoExitPoint = 0xFF;
    constexpr u32 SetVelocitySlot = 26;
    constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
    constexpr u32 FlyingState = 2;
    constexpr u32 Targets = 0x100000;
    constexpr u32 Bit21 = 0x200000;
    auto* agent = static_cast<ObjectNode*>(node);
    InstanceContext* instance = agent->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    InstanceFactory* factory = g_InstanceFactory;
    factory->SetFlag2();
    factory->ClearFlag1();
    factory->SetFlag0();
    factory->ClearFlag3();
    factory->ClearFlag4();
    factory->creationFlags = 0;
    Matrix4x4 frame;
    u32 exitPoint = shot & ExitPointMask;
    if (exitPoint == NoExitPoint)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        frame = place->matrix;
    }
    else
    {
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
        ExitPointAnimation* point = nullptr;
        if (model->animator != nullptr)
        {
            SizedArray<ExitPointAnimation*>* points = model->animator->exitPoints;
            point = points != nullptr ? points->data[exitPoint] : nullptr;
        }

        if (point == nullptr)
        {
            InitIdentityMatrix(&frame);
        }
        else
        {
            frame = UpdateExitPointMatrix(point)->matrix;
            for (u32 row = 0; row < 3; row++)
            {
                Vector4* axis = RowOf(&frame, row);
                f32 inverse = InverseLength(axis, LengthEpsilon);
                axis->x = axis->x * inverse;
                axis->y = axis->y * inverse;
                axis->z = axis->z * inverse;
            }
        }
    }

    Vector4 position = *RowOf(&frame, 3);
    if ((shot & Offset) != 0)
    {
        const Vector4* rowX = RowOf(&frame, 0);
        const Vector4* rowY = RowOf(&frame, 1);
        const Vector4* rowZ = RowOf(&frame, 2);
        position.z = position.z + ((rowX->z * x + rowY->z * y) + rowZ->z * z);
        position.x = position.x + ((rowX->x * x + rowY->x * y) + rowZ->x * z);
        position.y = position.y + ((rowX->y * x + rowY->y * y) + rowZ->y * z);
    }

    s32 angles[3];
    AnglesOfMatrix(&frame, angles);
    InstanceContext* made = CreateInstance(factory, chunk, objectAndMessage & 0x7FFF, &position, angles);
    auto* madeNode = static_cast<ObjectNodeBase*>(GetGameNode(&made->nodes, ObjectNodeKind));
    if ((shot & HasSpeed) != 0)
    {
        Vector4 velocity = *RowOf(&frame, 2);
        velocity.x = velocity.x * speed;
        velocity.y = velocity.y * speed;
        velocity.z = velocity.z * speed;
        CallVirtual<void>(madeNode, madeNode->vtable, SetVelocitySlot, -1.0f, &velocity);
    }

    madeNode->unknown06 = 0xFFFF;
    u16 message = static_cast<u16>(objectAndMessage >> 16);
    if (CallVirtual<u32>(madeNode, madeNode->vtable, TakesPacketsSlot) == 0)
    {
        ProjectileState* projectile = ProjectileOf(madeNode);
        if ((shot & AtTarget) != 0)
        {
            if (agent->agentRef1 != nullptr && (agent->agentRef1->flags & ReferencedObject::FlagAsleep) != 0)
            {
                agent->agentRef1 = nullptr;
            }

            AssignReference(&projectile->target, agent->agentRef1);
            projectile->bits |= Targets;
        }

        AssignReference(&projectile->shooter, instance);
        made->collision.leftOut = instance;
        projectile->message = message;
        if ((shot & Bit15) != 0)
        {
            projectile->bits |= Bit21;
        }

        SetProjectileState(madeNode, GetContextClock(made), FlyingState);
    }

    if (made != nullptr && message != 0xFFFF)
    {
        Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
        GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &sender, 2);
        Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
        QueueEvent(made, &handle);
    }

    factory->SetFlag4();
}
