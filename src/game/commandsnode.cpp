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
#include "game/scripttokens.h"
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
ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// The instance's attachments node (none when it has none)
AttachmentsNode* AttachmentsNodeOf(InstanceContext* instance)
{
    return static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, NodeAttachments));
}

// The slot a shadow node casts
void CastShadowSlot(ShadowNode* shadow, u32 slot)
{
    shadow->bits.slot = slot;
}

void ClearTrails(GameNode* node)
{
    CallVirtual<void>(node, node->vtable, ObjectNode::DestroyParticleTrailsSlot);
}
}

void AddTrailCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ExecuteOn(runner->agentNode);
}

// The node keeps the trail's arguments
void AddTrailCommand::ExecuteOn(GameNode* node)
{
    CallVirtual<u32>(node, node->vtable, ObjectNode::AddParticleTrailSlot, &trail);
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

void SetWobbleCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->FollowMotionBlock(&block, clock);
}

void ClearWobbleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->ReleaseTrajectory();
}

void SetRestartableCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->receivers->bits.restartable = restartable.on;
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
    CallVirtual<void>(agent, agent->vtable, Agent::UnsupportedSlot);
}

void ClearFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->flags.value &= ~ObjectNodeFlags::FocusMask;
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
    NodeOf(runner)->owner->flags.attached = 0;
}

void DetachAllSpringsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(NodeOf(runner)->owner);
    if (attachments != nullptr)
    {
        DetachAllSprings(attachments);
    }
}

void SetContactSpringyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->motionBlock = &block;
    node->owner->flags.physicsBody = 1;
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

void ClearKnockCountdownCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->reactions.knockCountdown = 0;
}

// The agent's contact message forgotten (its point the default box's lowest corner)
void ClearObjectContextTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ContactMessage* contact = &NodeOf(runner)->agent->contact;
    contact->hitKinds = 0;
    contact->damage = 0;
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
    node->flags.focusInstance = 1;
    node->flags.focusPosition = 0;
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
        SetPerceptionWeight(weight, perception, static_cast<u8>(sense));
    }
}

void AddPerceptionWeightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        AddPerceptionWeight(weight, perception, static_cast<u8>(sense));
    }
}

void TurnSenseOffCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        TurnSenseOff(perception, static_cast<u8>(sense));
    }
}

void TurnSenseOnCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    void* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        TurnSenseOn(perception, static_cast<u8>(sense));
    }
}

// The value given to the level below as its message (none on the first level)
void SetParentExecutionValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    u32 index = level->bits.index;
    if (index == 0)
    {
        return;
    }

    runner->levels[index - 1]->bits.message = message.id;
}

void PreviousKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->PreviousKey();
}

void PreviousRouteNodeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->NextRouteStep();
}

void ClearTouchMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->touchMessage = 0;
        block->flags.sendsTouches = 0;
    }
}

void StopTouchMessagesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->flags.sendsTouches = 0;
    }
}

void SendTouchMessagesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    MotionBlock* block = NodeOf(runner)->motionBlock;
    if (block != nullptr)
    {
        block->flags.sendsTouches = 1;
    }
}

void SetPathIndexCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->pathIndex = path.index;
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
    runner->flags.interruptFromLevel = 1;
}

void ClearSavedScriptStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->interruptLevel = BehaviourRunner::NoLevel;
    runner->flags.interruptFromLevel = 0;
}

void MarkTimeCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->markedTime = clock->time;
}

void ClearMarkedTimeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    runner->markedTime = 0;
}

void RestartRouteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeOf(runner)->waypoints->RestartRoute();
}

void ApplyVelocityToSelfCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    ApplyVelocity(this, instance, instance);
}

void SetMotionCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->flags.searchEnded = 0;
    node->flags.foundCover = 0;
    node->flags.noCover = 0;
    node->FollowMotionBlock(&block, clock);
}

void CreateHeadTrackingCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    CreateHeadTracking(NodeOf(runner), &settings, clock);
}

void AddPerceptionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AddPerception(NodeOf(runner), &sense);
    NodeOf(runner)->flags.keepsPerception = 1;
}

void StopStickingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node != nullptr && node->motionBlock != nullptr)
    {
        StopMotionBlockSticking(node->motionBlock, keepsStuck.on);
    }
}

// A positive value counted among the instances' that have one
void ClearCountedValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    f32* stored = &NodeOf(runner)->countedValue;
    f32 old = *stored;
    *stored = 0.0f;
    if (*stored < old)
    {
        g_CountedInstances--;
        if (g_CountedInstances < 0)
        {
            g_CountedInstances = 0;
        }
    }
}

namespace
{
bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

// The linked object the linked object commands are at made another
void SetCurrentLinked(AttachmentsNode* attachments, u32 index)
{
        attachments->bits.currentLinked = index;
}

InstanceContext* DesignatorOf(ObjectNode* node, u8 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, u32{designator});
}

// Every sense of a perception switched on or off
void SwitchSenses(Perception* perception, u8 on)
{
    for (u32 index = 0; index < perception->bits.count; index++)
    {
        perception->on[index] = on;
    }
}
}

extern "C"
{
    // A rigid body made active (game/commandsphysics.cpp)
    void ActivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00246950);
}

void SnapRotationCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if (axes.x != 0)
    {
        SnapInstanceRotationX(stepX, instance);
    }

    if (axes.y != 0)
    {
        SnapInstanceRotationY(stepY, instance);
    }

    if (axes.z != 0)
    {
        SnapInstanceRotationZ(stepZ, instance);
    }
}

void PhysicsResetVelocityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (!node->owner->flags.physicsBody)
    {
        return;
    }

    FollowOwnMotionBlock(node);
    node->flags.keepsTrajectory = 1;
}

// From the last back to the first
void NextLinkedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(NodeOf(runner)->owner);
    if (attachments == nullptr)
    {
        return;
    }

    u32 index = attachments->bits.currentLinked;
    u32 next = index == attachments->LinkedCount() - 1 ? 0 : index + 1;
    SetCurrentLinked(attachments, next);
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
    AttachmentsNode* attachments = AttachmentsNodeOf(NodeOf(runner)->owner);
    if (attachments == nullptr)
    {
        return;
    }

    u32 index = static_cast<u32>(number) - 1;
    if (index < attachments->LinkedCount())
    {
        SetCurrentLinked(attachments, index);
    }
}

// The agent's part's analog value, kept between -1 and 1
void SetPresenceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentPart* part = NodeOf(runner)->agent->part;
    f32 analog = ClampFloat(presence, -1.0f, 1.0f);
    part->presence = analog;
}

void AddPresenceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentPart* part = NodeOf(runner)->agent->part;
    part->presence = ClampFloat(part->presence + delta, -1.0f, 1.0f);
}

void DisableAllPerceptionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Perception* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        SwitchSenses(perception, 0);
    }
}

void EnableAllPerceptionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Perception* perception = NodeOf(runner)->perception;
    if (perception != nullptr)
    {
        SwitchSenses(perception, 1);
    }
}

// The motion block of the arguments the node's own when it has none, the touch message given to the node's
void AttachMotionBlockCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    MotionBlock* followed = node->motionBlock;
    if (followed == nullptr)
    {
        followed = &block;
        node->motionBlock = followed;
        node->owner->flags.physicsBody = 1;
        node->motionBlock->node = node;
    }

    if (touchMessage != 0)
    {
        followed->touchMessage = static_cast<u32>(touchMessage);
        followed->flags.sendsTouches = 1;
    }
}

void NoOpNowMoveBackwardsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if (!instance->flags.hasAttachment)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    // Retail looks the instance up and drops it
    if (attachments->path != nullptr)
    {
        UnslottedAttachment(attachments->path);
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
    if (request.allHulls != 0)
    {
        SetAllHullSurfaces(collision, request.surface);
    }
    else
    {
        SetHullOwnSurface(collision, request.hull, request.surface);
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
        node->flags.focusInstance = 1;
    }

    node->flags.focusPosition = 0;
}

// The designator's instance takes its object from this node
void SetTargetOwnerToSelfCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 designator = target.designator;
    if (designator == DesignatesNone)
    {
        return;
    }

    InstanceContext* instance = DesignatorOf(node, designator);
    if (instance != nullptr)
    {
        static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject))->sourceNode = node;
    }
}

void RestoreOwnObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    ObjectNodeBase* reset;
    u8 designator = request.designator;
    if (designator != DesignatesNone)
    {
        reset = static_cast<ObjectNodeBase*>(GetGameNode(&DesignatorOf(node, designator)->nodes, NodeObject));
    }
    else
    {
        reset = request.own != 0 ? node : nullptr;
    }

    if (reset != nullptr)
    {
        reset->sourceNode = nullptr;
    }
}

namespace
{
// The highest priority a script gives (past it the starter's own is taken)
constexpr u32 MostPriority = 100;
}

extern "C"
{
    // The chunk manager's chunk of an index, and an ID's flag of a store toggled
    extern void* G_ChunkManager;
    // A crate's contents made
    void CreateCrateContents(const CreateCrateContentsCommand* command, Agent* crate, InstanceContext* instance,
                             BehaviourRunner* runner) RETAIL(FUN_0010d358);
    // The rank SetTriggerRank sets
    extern u8 g_TriggerRank RETAIL(D_0030A0E9);
}

void SetContactSoundsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    SetContactSounds(value, node, slots.first, slots.last);
    if (slots.off != 0)
    {
        node->reactions.noContactSounds = 1;
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

void SetBodyMassCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* body = static_cast<RigidBody*>(GetGameNode(&NodeOf(runner)->owner->nodes, NodeRigidBody));
    if (body != nullptr)
    {
        body->SetMassAndSize(mass, 1.0f, 1.0f, 1.0f);
    }
}

void SetRankCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    s32 given = rank.IntWith(NodeOf(runner)->PacketProperties());
    NodeOf(runner)->rank = static_cast<u8>(given);
}

void SetTriggerRankCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    g_TriggerRank = static_cast<u8>(rank.IntWith(NodeOf(runner)->PacketProperties()));
}

// The node's stored place (none when the request gives none), its angles worked out
void StoreCurrentSpaceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectPlace* place = SpaceOfRequest(runner, request.space, request.designator, request.receiver, request.unused20);
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
    if (properties->state.persistentFlag == 0)
    {
        return;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* store = properties->state.flagInChunkStore != 0 ? chunk->savedFlags : chunk->unsavedFlags;
    if (store != nullptr)
    {
        TogglePersistentFlag(store, id);
    }
}

// Past 100 the starter's own
void SetBehaviourPriorityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    StarterReceivers* receivers = runner->receivers;
    u32 given = priority.priority;
    if (given > MostPriority)
    {
        given = receivers->starter->bits.priority;
    }

    receivers->bits.priority = given;
}

// The instance's crate agent (whether it has one isn't checked)
void CreateCrateContentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCrate));
    CreateCrateContents(this, node->agent, instance, runner);
}

// The wumpa fruit in a crate (none below 0: they're left as they are)
void SetCrateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    if (static_cast<s32>(wumpaFruit) < 0)
    {
        return;
    }

    auto* node = static_cast<AgentNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, NodeCrate));
    if (node == nullptr)
    {
        return;
    }

    auto* part = static_cast<CratePart*>(node->agent->part);
    part->crate.wumpaFruit = wumpaFruit;
}

// None left below 0 (whether the instance has a creature agent isn't checked)
void ReduceHitPointsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, NodeCreature));
    auto* part = static_cast<CreaturePart*>(node->agent->part);
    s32 left = static_cast<s32>(part->flags.hitPoints) - hitPoints;
    if (left < 0)
    {
        left = 0;
    }

    part->flags.hitPoints = left;
}

namespace
{
// The most a node's counted value is, and below which it counts as none
constexpr f32 MostCountedValue = 1.0f;
constexpr f32 NoCountedValue = Rounded(1e-06);

InstanceContext* PlayedCharacter()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->play.character);
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// The instance's playable character agent's node, else the player's
AgentNode* CharacterNodeOf(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (node == nullptr)
    {
        InstanceContext* player = PlayerInstance();
        if (player != nullptr)
        {
            node = static_cast<AgentNode*>(GetGameNode(&player->nodes, NodeCharacter));
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
    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    if (settings.exitPointGiven != 0)
    {
        AttachLinkedAgentsToSlot(attachments, instance, settings.exitPoint, 0);
    }
    else
    {
        AttachLinkedAgents(attachments, instance, 0);
    }
}

void DetachAllLinkedAgentsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    ReleaseAttachmentsNode(attachments, 1, 1, 0);
    instance->flags.hasAttachment = 0;
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
    node->flags.focusInstance = 1;
    node->flags.focusPosition = 0;
}

// Whether the played character has a follow node isn't checked
void RestoreCameraDefaultsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    RestoreCameraDefaults(static_cast<FollowNode*>(GetGameNode(&PlayedCharacter()->nodes, NodeFollow)));
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
    block->flags.sticky = staysSticky.on;
    if (staysSticky.on == 0)
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
    InstanceContext* instance = G_GameController->progress.Instance(static_cast<u32>(character));
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
    auto* node = static_cast<AgentNode*>(GetGameNode(&player->nodes, NodeCharacter));
    static_cast<CharacterAgent*>(node->agent)->homeChunk = player->chunk;
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
    GameProgress* progress = &G_GameController->progress;
    progress->play.pairing = pairing;
    progress->play.character = character;
    progress->play.second = second;
}

void PlayMovieCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    s32 index = movie.IntWith(NodeOf(runner)->PacketProperties());
    G_GameController->RequestMovie(static_cast<s32>(delay * g_ClockUnitsPerSecond), static_cast<u32>(index));
}

// The level finished, the node told its packet ended, the agent restarted
void RestartDefaultBehaviourCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    ObjectNode* node = NodeOf(runner);
    Agent* agent = node->agent;
    level->bits.finished = 1;
    CallVirtual<void>(node, node->vtable, ObjectNode::PacketEndedSlot, runner);
    RestartAgent(agent);
}

// At most 1; a value set where there was none counts it among the instances' (a new 0 too)
void SetCountedValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    f32* stored = &NodeOf(runner)->countedValue;
    f32 old = *stored;
    *stored = value;
    if (MostCountedValue < value)
    {
        *stored = MostCountedValue;
    }

    if (old < NoCountedValue)
    {
        g_CountedInstances++;
    }
}

void ClearCharacterDeadCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AgentNode* character = CharacterNodeOf(NodeOf(runner)->owner);
    if (character != nullptr)
    {
        static_cast<CharacterAgent*>(character->agent)->state.dead = 0;
    }
}

// Whether the played character has a follow node isn't checked
void ResetCameraCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    auto* node = static_cast<FollowNode*>(GetGameNode(&PlayedCharacter()->nodes, NodeFollow));
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

void SetReverbCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ChunkData* chunk = NodeOf(runner)->owner->chunk;
    if (chunk == nullptr)
    {
        return;
    }

    if (boxReverb.on != 0)
    {
        SetChunkBoxReverb(chunk, &reverb);
    }
    else
    {
        SetChunkReverb(chunk, &reverb);
    }
}

// Its node and its instance put to sleep
void DestroyMeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if (settings.mode >= DestroyMeSettings::Modes)
    {
        return;
    }

    CallVirtual<void>(node, node->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
    CallVirtual<u32>(instance, instance->vtable, InstanceContext::SleepSlot);
}

namespace
{
// A physics body's flag 0x10: it touched another body this step
constexpr u32 BodyTouchedBody = 0x10;
// A physics body's flags 0x20 and 0x40: either lets the playable characters push it (game/objectnode.h's MotionBlockBody)
constexpr u32 BodyPushable20 = 0x20;
constexpr u32 BodyPushable40 = 0x40;

template <typename T>
void Switch(u32 setting, T* word, T bit)
{
    if (setting == SwitchOn)
    {
        *word |= bit;
    }
    else if (setting == SwitchOff)
    {
        *word &= ~bit;
    }
}
}

// The physics body's flags switched, and it's made to have touched another body and pushable
void SwitchBodyFlagsCommand::ExecuteOn(GameNode* node)
{
    auto* body = static_cast<RigidBody*>(GetGameNode(&node->owner->nodes, NodeRigidBody));
    if (body == nullptr)
    {
        return;
    }

    u32 bodyFlags = body->bodyFlags.value;
    Switch(switches.unused0, &bodyFlags, 1u << 13);
    Switch(switches.unused2, &bodyFlags, 1u << 15);
    Switch(switches.unused4, &bodyFlags, 1u << 16);
    Switch(switches.unused6, &bodyFlags, 1u << 12);
    body->bodyFlags.value = bodyFlags | BodyTouchedBody | BodyPushable20;
}

void SetShadowSlotCommand::ExecuteOn(GameNode* node)
{
    auto* shadow = static_cast<ShadowNode*>(GetGameNode(&node->owner->nodes, NodeShadow));
    if (shadow != nullptr)
    {
        CastShadowSlot(shadow, slot.slot);
    }
}

// The slot's shadow deleted
void ClearShadowSlotCommand::ExecuteOn(GameNode* node)
{
    auto* shadow = static_cast<ShadowNode*>(GetGameNode(&node->owner->nodes, NodeShadow));
    if (shadow != nullptr)
    {
        shadow->ClearSlot(slot.slot);
    }
}

// Two bit switches of the instance and its nodes (1 on, 2 off): its flags 0x100, 0x400 and 0x10, its movement node carrying, its
// physics body's flags 0x20 and 0x40, its model's collision bit; asleep or awake (1 woken, 2 its node's function 11 and the
// instance put to sleep); the node's and the model's squared distances (0xFF: none)
void SetObjectCommand::ExecuteOn(GameNode* node)
{
    InstanceContext* instance = node->owner;
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    Switch(switches.busy, &instance->flags.value, u32{ReferencedObjectFlags::Busy});
    u32 setting = bodySwitches.carries;
    if (setting == SwitchOn || setting == SwitchOff)
    {
        auto* movement = static_cast<MovementNode*>(GetGameNode(&instance->nodes, NodeMovement));
        if (movement != nullptr)
        {
            movement->bits.carries = setting == SwitchOn ? 1 : 0;
        }
    }

    for (auto [field, bit] : {std::pair{u32{switches.pushable20}, BodyPushable20},
                              std::pair{u32{bodySwitches.pushable40}, BodyPushable40}})
    {
        if (field == SwitchOn || field == SwitchOff)
        {
            auto* body = static_cast<RigidBody*>(GetGameNode(&instance->nodes, NodeRigidBody));
            if (body != nullptr)
            {
                Switch(field, &body->bodyFlags.value, bit);
            }
        }
    }

    Switch(switches.visible, &instance->flags.value, u32{ReferencedObjectFlags::Visible});
    Switch(switches.collision, &instance->flags.value, u32{ReferencedObjectFlags::CollisionActive});
    u32 asleep = switches.asleep;
    if (asleep == SwitchOn)
    {
        CallVirtual<void>(instance, instance->vtable, InstanceContext::WakeSlot);
    }
    else if (asleep == SwitchOff)
    {
        CallVirtual<void>(node, node->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
        CallVirtual<void>(instance, instance->vtable, InstanceContext::SleepSlot);
    }

    u32 solid = bodySwitches.solid;
    if (solid == SwitchOn)
    {
        model->SetSolid(1);
    }
    else if (solid == SwitchOff)
    {
        model->SetSolid(0);
    }

    if (switches.nodeDistanceGiven != 0)
    {
        u32 distance = switches.nodeDistance;
        node->nearDistance = static_cast<u16>(distance * distance);
        if (switches.nodeDistance == ObjectSwitches::NoDistance)
        {
            node->nearDistance = GameNode::AnyNearDistance;
        }
    }

    if (switches.modelDistanceGiven != 0)
    {
        u32 distance = switches.modelDistance;
        model->nearDistance = static_cast<u16>(distance * distance);
        if (switches.modelDistance == ObjectSwitches::NoDistance)
        {
            model->nearDistance = GameNode::AnyNearDistance;
        }
    }
}

void DoAnimationCommand::Play(PropertyHolder* properties, GameAnimation* animation, OgiAnimator* animator)
{
    f32 blend = blendTime.FloatWith(properties);
    if (animation == nullptr)
    {
        StopOgiAnimation(animator, static_cast<s32>(blend * g_ClockUnitsPerSecond), OgiAnimator::RootJoint);
        return;
    }

    f32 rate = speed.FloatWith(properties);
    if (flags.speedGiven != 0)
    {
        if (flags.speedRandomGiven != 0)
        {
            rate += RandomSignedTimes(speedRandom.FloatWith(properties));
        }
    }
    else if (flags.speedIsDuration != 0)
    {
        // The speed is how long it lasts: its frames at its rate divided by that
        GameAnimationBits bits = animation->bits;
        rate = (static_cast<f32>(static_cast<s32>(bits.frames)) / static_cast<f32>(static_cast<s32>(bits.rate))) / rate;
    }
    else
    {
        rate = 1.0f;
    }

    AnimationSettings settings;
    ConstructAnimationSettings(rate, &settings, animation);
    settings.bits.loops = flags.loops;
    settings.bits.backward = flags.backward;
    settings.bits.queuedAlways = flags.queuedAlways;
    if (flags.blendTimeGiven != 0)
    {
        s32 ticks = static_cast<s32>(blend * g_ClockUnitsPerSecond);
        settings.blendTime = ticks;
        settings.bits.queued = ticks != 0;
    }

    if (flags.continues != 0)
    {
        settings.bits.startsPartWay = 0;
        settings.bits.continues = 1;
    }
    else if (flags.startsPartWay != 0)
    {
        settings.start = startPosition;
        settings.bits.startsPartWay = 1;
        settings.bits.continues = 0;
    }

    PlayOgiAnimation(animator, &settings, flags.joint);
    DestroyAnimationSettings(&settings, DestroyOnly);
}

void DoAnimationCommand::ExecuteOn(GameNode* node)
{
    auto* agent = static_cast<ObjectNode*>(node);
    GameObject* object = agent->sourceNode != nullptr ? SourceObject(agent->sourceNode) : agent->object;
    InstanceContext* instance = agent->owner;
    u32 slot = flags.slotCount == 1 ? slots[0] : slots[RandomBelow(flags.slotCount)];
    u16 modelId;
    GetObjectModelId(&modelId, object, slot);
    if (modelId != NoModelId)
    {
        GameResources* resources = G_GameResourcesObjectPointer;
        ResourceTable* models = resources->models;
        u16 id = modelId;
        auto* ogi = id != NoModelId ? static_cast<GameOGI*>(models->items[id & ResourceIndexMask]) : nullptr;
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
        PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                                  : agent->properties;
        u16 animationId;
        GetObjectAnimationId(&animationId, object, slot);
        model->SetOgi(ogi, object->header.reactJoints, object->header.exitPoints);
        OgiAnimator* animator = model->animator;
        if (animator != nullptr)
        {
            GameAnimation* animation = nullptr;
            if (animationId != NoAnimationId)
            {
                ResourceTable* animations = resources->animations;
                id = animationId;
                animation =
                    id != NoAnimationId ? static_cast<GameAnimation*>(animations->items[id & ResourceIndexMask]) : nullptr;
            }

            Play(properties, animation, animator);
        }
    }

    if (flags.shadowGiven != 0)
    {
        auto* shadow = static_cast<ShadowNode*>(GetGameNode(&instance->nodes, NodeShadow));
        if (shadow != nullptr)
        {
            CastShadowSlot(shadow, flags.shadowSlot);
        }
    }
}

void DoParticleCommand::ExecuteOn(GameNode* node)
{
    auto* agent = static_cast<ObjectNode*>(node);
    InstanceContext* instance = agent->owner;
    s32 system = emission.system;
    if (placement.key != Waypoints::NoKey)
    {
        Waypoints* waypoints = agent->waypoints;
        u32 index = placement.key;
        LayoutPosition* key = nullptr;
        if (waypoints->keyCount != 0)
        {
            key = static_cast<s32>(index) < waypoints->keyCount ? waypoints->positions.data[index] : nullptr;
        }

        // A key past the last is read from address 0, as retail does
        position = key->position;
        position.w = 1.0f;
        emission.fromFrame = 0;
        placement.hasPosition = 1;
    }

    // The surface mode plays the system of the surface's index, the contact's (with a decal at its point) or the one under the
    // node, as retail does (the mode's byte goes unread)
    const Vector4* contact = nullptr;
    if (placement.surfaceMode != ParticlePlacement::NoSurfaceMode
        && CallVirtual<u32>(agent, agent->vtable, ObjectNode::TakesPacketsSlot) != 0)
    {
        system = agent->waterSurface;
        if (agent->waterSurface == ObjectNode::NoSurface)
        {
            system = agent->surface;
        }
        else
        {
            contact = &agent->waterPoint;
        }
    }

    u32 value = emission.emitterValue;
    if (emission.fromFrame != 0 || emission.fromExitPoint != 0)
    {
        s32 emitter = StartEmitterKeepingTranslation(instance, system, value, nullptr);
        if (emitter < 0)
        {
            return;
        }

        Matrix4x4 frame = *ParticleFrame(instance, emission.exitPoint);
        OrientParticleFrame(placement.turned, placement.axes, placement.hasPosition != 0 ? &position : nullptr, &frame);
        if (contact != nullptr)
        {
            *RowOf(&frame, 3) = *contact;
            AddDecalFromDescriptor(&frame, instance->chunk);
        }

        SetEmitterFrame(emitter, &frame);
        return;
    }

    if (placement.designator != DesignatesNone)
    {
        u32 designator = placement.designator;
        auto* target = CallVirtual<InstanceContext*>(agent, agent->vtable, ObjectNode::GetDesignatorSlot, designator);
        if (target != nullptr)
        {
            ObjectPlace* place = target->place;
            place->SyncPosition();
            position = place->position;
        }
        else
        {
            CallVirtual<u32>(agent, agent->vtable, ObjectNode::GetDesignatorPositionSlot, designator, &position);
        }
    }

    if (contact == nullptr)
    {
        StartEmitterKeepingTranslation(instance, system, value, placement.hasPosition != 0 ? &position : nullptr);
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
// No channel of the sound processor
constexpr u32 NoChannel = 0xFF;

u8& TrackedSound(ObjectNode* node)
{
    return node->trackedSound;
}

// The volume a track plays at in the main music slot: its own (the given one's ignored)
f32 MainMusicVolume(s32 track)
{
    if (static_cast<u32>(track - 0x1D) < 2 || track == 0x23 || track == 0x36 || track == 0x37 || track == 0x3D)
    {
        return 1.0f;
    }

    return track == 0x3B ? Rounded(0.8) : 0.75f;
}
}

u16 SoundOfSlot(ObjectNode* node, u32 slot)
{
    slot &= 0xFFFF;
    u16 id = NoSoundId;
    if (slot == NoSoundSlot)
    {
        return id;
    }

    GameObject* object = node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
    u16 found;
    GetObjectSoundId(&found, object, slot);
    if (found != NoSoundId)
    {
        return found & ResourceIndexMask;
    }

    GameObject* own = node->OwnObject();
    if (own != nullptr)
    {
        GetObjectSoundId(&found, own, slot);
        if (found != NoSoundId)
        {
            id = found & ResourceIndexMask;
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
    u32 channel = NoChannel;
    u16 sound = SoundOfSlot(node, slot);
    if (sound == NoSoundId)
    {
        return;
    }

    s32 kept = -1;
    s32 voiceKind = ListenerVoiceKind();
    if (tracked == 0 || CallVirtual<u32>(node, node->vtable, ObjectNode::HasNoTrackedSoundSlot) == 0)
    {
        PlaySoundByIdAt(OwnScale, OwnScale, sound, 0, instance->chunk, &position, voiceKind, -1);
    }
    else
    {
        kept = 0;
        channel = PlayInstanceSoundById(OwnScale, OwnScale, sound, 0, instance, voiceKind, 0);
    }

    if (channel != NoChannel && tracked != 0 && kept == 0)
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
    if (flags.volumeGiven != 0)
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
    u32 sound = NoSoundId;
    u32 channel = NoChannel;
    if (flags.contactKind == DoSoundFlags::SlotsKind
        || CallVirtual<u32>(agent, agent->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        s32 slot = RandomBelow(static_cast<s32>(flags.slotCount));
        sound = SoundOfSlot(agent, soundSlots[slot]);
    }
    else
    {
        s32 surface = agent->waterSurface;
        if (surface == ObjectNode::NoSurface)
        {
            surface = agent->surface;
        }

        if (surface != ObjectNode::NoSurface)
        {
            CollisionSurface* contact = &g_CollisionSurfaces.surfaces[surface];
            sound = GetSurfaceSound(contact, flags.contactKind, &volumeGiven);
            if (flags.contactKind == ContactLand)
            {
                Agent* owner = agent->agent;
                if (CallVirtual<u32>(owner, owner->vtable, Agent::IsCharacterSlot) != 0 && contact->surfaceId == SurfaceSand)
                {
                    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
                    CallVirtual<void>(owner, owner->vtable, Agent::LeaveFootprintsSlot, contact, &up);
                }
            }
        }
    }

    if (sound != NoSoundId)
    {
        u32 stamp = instance->seen;
        if (stamp < InstanceContext::SoundSeenLimit || flags.followed != 0 || flags.tracked != 0)
        {
            f32 level = 1.0f;
            bool hasLevel = false;
            if (0.0f < volumeGiven)
            {
                level = volumeGiven;
                hasLevel = true;
            }

            if (flags.randomVolume != 0)
            {
                hasLevel = true;
                level = level + RandomSignedTimes(volumeRandom);
            }

            f32 scale = 1.0f;
            bool hasScale = false;
            if (flags.pitchGiven != 0)
            {
                scale = pitch;
                hasScale = true;
            }

            if (flags.randomPitch != 0)
            {
                hasScale = true;
                scale = scale + RandomSignedTimes(pitchRandom);
            }

            s32 voiceKind = ListenerVoiceKind();
            s32 kept = -1;
            if (flags.tracked != 0 && CallVirtual<u32>(agent, agent->vtable, ObjectNode::HasNoTrackedSoundSlot) != 0)
            {
                kept = 0;
            }

            u32 group = flags.group;
            f32 played = hasLevel ? level : -1.0f;
            f32 playedScale = hasScale ? scale : -1.0f;
            if (flags.unplaced != 0)
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
            else if (flags.followed != 0 || flags.tracked != 0)
            {
                channel = PlayInstanceSoundById(played, playedScale, sound, group, instance, voiceKind, kept);
            }
            else
            {
                PlaySoundByIdAt(played, playedScale, static_cast<u16>(sound), static_cast<s32>(group), instance->chunk, &position,
                                voiceKind, kept);
            }

            if (channel != NoChannel && flags.tracked != 0 && kept == 0)
            {
                TrackedSound(agent) = static_cast<u8>(channel);
            }
        }
    }

    if (flags.shakes == 0)
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
    auto* agent = static_cast<ObjectNode*>(node);
    PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                              : agent->properties;
    s32 number = track.IntWith(properties);
    // Started at once in the music group (the spare slot's and an emitter's in the effects group); retail's request keeps what
    // the stack held above its loop bit, which nothing reads
    MusicRequest request;
    request.bits.value = 0;
    request.bits.track = static_cast<u32>(number);
    request.bits.group = MusicGroup;
    request.bits.startsAtOnce = 1;
    request.bits.loops = flags.loops;
    request.left = volume;
    request.right = volume;
    request.fadeTime = fadeTime;
    if (flags.slot == MainMusicSlot)
    {
        f32 level = MainMusicVolume(number);
        request.left = level;
        request.right = level;
    }

    u32 slot = flags.slot;
    if (slot == SpareMusicSlot)
    {
        request.bits.group = EffectsGroup;
    }
    else if (slot > SpareMusicSlot)
    {
        if (slot != BeginMusicFlags::EmitterSlot)
        {
            return;
        }

        request.bits.group = EffectsGroup;
        AddMusicEmitter(range, agent->owner, &request);
        return;
    }

    PlayMusicRequest(slot, &request);
}

void CreateDamageCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 MostHit = 0x40;
    auto* agent = static_cast<ObjectNode*>(node);
    u32 found = 0;
    ContactMessage message;
    ContactMessage::Construct(&message);
    PropertyHolder* properties = agent->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(agent->sourceNode)
                                                              : agent->properties;
    InstanceContext* instance = agent->owner;
    ChunkData* chunk = instance->chunk;
    u32 kinds = DamageableNodeKinds;
    f32 radius = reach.FloatWith(properties);
    if (flags.skipsCharacters != 0)
    {
        kinds = DamageableNodeKinds & ~(1 << NodeCharacter);
    }
    else if (flags.onlyCharacters != 0)
    {
        kinds = 1 << NodeCharacter;
    }

    void* results[MostHit];
    InstanceQuery query;
    query.most = MostHit;
    query.results = results;
    query.count = 0;
    query.distance = Infinite;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = 0;
    query.bits.unused0 = 0;
    query.bits.allWanted = 1;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, instance);
    Vector4 position;
    u32 joint = flags.joint;
    if (joint != GameOGI::NoJoint)
    {
        JointPosition(instance, joint, &position, nullptr);
    }
    else
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        position = place->position;
        if (flags.offsetGiven != 0)
        {
            place = instance->place;
            RotateAndTranslate(place);
            const Vector4* rowX = RowOf(&place->matrix, 0);
            const Vector4* rowY = RowOf(&place->matrix, 1);
            const Vector4* rowZ = RowOf(&place->matrix, 2);
            position.z = position.z + ((rowX->z * offsetX + rowY->z * offsetY) + rowZ->z * offsetZ);
            position.x = position.x + ((rowX->x * offsetX + rowY->x * offsetY) + rowZ->x * offsetZ);
            position.y = position.y + ((rowX->y * offsetX + rowY->y * offsetY) + rowZ->y * offsetZ);
        }
    }

    u32 hull = shape.hull;
    if (hull != Shape::NoHull)
    {
        found = InstancesInDamageHull(instance, hull, kinds, &query);
    }
    else
    {
        switch (shape.kind)
        {
        case ShapeSphereOnEvery:
            position.w = radius;
            found = ChunkInstancesInSphere(chunk, &position, kinds, &query, 1);
            break;
        case ShapeSphere:
            position.w = radius;
            found = ChunkInstancesInSphere(chunk, &position, kinds, &query, 0);
            break;
        case ShapeCylinder:
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

    message.damage = static_cast<u8>(damage.IntWith(properties));
    message.hitKinds = hitKinds;
    if (flags.instantDeath != 0)
    {
        message.hitKinds = hitKinds | HitGeneric;
    }

    if (flags.givesMessageW != 0)
    {
        message.point.w = messageW.FloatWith(properties);
    }

    auto send = [&](void* hit) {
        AgentNode* hitNode = AgentNodeOf(static_cast<InstanceContext*>(hit));
        if (hitNode != nullptr)
        {
            CallVirtual<void>(hitNode->agent, hitNode->agent->vtable, Agent::ContactSlot, &message, instance, 1u);
        }
    };
    if (flags.nearestOnly == 0)
    {
        for (u16 index = 0; index < found; index++)
        {
            send(results[index]);
        }

        return;
    }

    InstanceContext* nearest = nullptr;
    f32 best = Infinite;
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
// A projectile's node keeps its own state where an object node keeps its frame's move (game/projectiles.cpp's
// ProjectileObjectNode): its state bits (it homes in on its target, it doesn't bounce off what's bullets bounce back), the
// trigger message it sends, the references to its target and to who shot it; and the state it's launched in
constexpr u32 ProjectileHoming = 0x100000;
constexpr u32 ProjectileNoBounce = 0x200000;
constexpr u32 ProjectileLaunched = 2;

struct ProjectileState
{
    u32 bits;
    u16 message;
    u16 unused06;
    Reference* target;
    Reference* shooter;
};

ProjectileState* ProjectileOf(ObjectNodeBase* node)
{
    return reinterpret_cast<ProjectileState*>(&node->frameMove);
}
}

extern "C"
{
    // A projectile node's state changed
    void SetProjectileState(ObjectNodeBase* node, TimeClock* clock, u32 state) RETAIL(FUN_0010a6e8);
}

void ShootCommand::ExecuteOn(GameNode* node)
{
    auto* agent = static_cast<ObjectNode*>(node);
    InstanceContext* instance = agent->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    InstanceFactory* factory = g_InstanceFactory;
    factory->SetInstanceProperties();
    factory->ClearUnused1();
    factory->SetGivesIds();
    factory->ClearGivesFlagSlots();
    factory->ClearUnused4();
    factory->creationFlags = 0;
    Matrix4x4 frame;
    u32 exitPoint = shot.exitPoint;
    if (exitPoint == GameOGI::NoExitPoint)
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
    if (shot.offsetGiven != 0)
    {
        const Vector4* rowX = RowOf(&frame, 0);
        const Vector4* rowY = RowOf(&frame, 1);
        const Vector4* rowZ = RowOf(&frame, 2);
        position.z = position.z + ((rowX->z * offset.x + rowY->z * offset.y) + rowZ->z * offset.z);
        position.x = position.x + ((rowX->x * offset.x + rowY->x * offset.y) + rowZ->x * offset.z);
        position.y = position.y + ((rowX->y * offset.x + rowY->y * offset.y) + rowZ->y * offset.z);
    }

    s32 angles[3];
    AnglesOfMatrix(&frame, angles);
    InstanceContext* made = CreateInstance(factory, chunk, object.id & ResourceIndexMask, &position, angles);
    auto* madeNode = static_cast<ObjectNodeBase*>(GetGameNode(&made->nodes, NodeObject));
    if (shot.speedGiven != 0)
    {
        Vector4 velocity = *RowOf(&frame, 2);
        velocity.x = velocity.x * speed;
        velocity.y = velocity.y * speed;
        velocity.z = velocity.z * speed;
        CallVirtual<void>(madeNode, madeNode->vtable, ObjectNode::LaunchSlot, -1.0f, &velocity);
    }

    madeNode->nearDistance = GameNode::AnyNearDistance;
    u16 message = object.message;
    if (CallVirtual<u32>(madeNode, madeNode->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        ProjectileState* projectile = ProjectileOf(madeNode);
        if (shot.atAgentRef1 != 0)
        {
            if (agent->agentRef1 != nullptr && agent->agentRef1->flags.asleep)
            {
                agent->agentRef1 = nullptr;
            }

            AssignReference(&projectile->target, agent->agentRef1);
            projectile->bits |= ProjectileHoming;
        }

        AssignReference(&projectile->shooter, instance);
        made->collision.leftOut = instance;
        projectile->message = message;
        if (shot.noBounce != 0)
        {
            projectile->bits |= ProjectileNoBounce;
        }

        SetProjectileState(madeNode, GetContextClock(made), ProjectileLaunched);
    }

    if (made != nullptr && message != NoMessage)
    {
        Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
        GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &sender,
                                                1 << NodeObject);
        Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
        QueueEvent(made, &handle);
    }

    factory->SetUnused4();
}
