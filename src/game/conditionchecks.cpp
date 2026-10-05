#include "game/conditions.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/attachments.h"
#include "game/characters.h"
#include "game/animation.h"
#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/cutscenereader.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/pads.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/rigidbody.h"
#include "game/scenery.h"
#include "game/vehicles.h"
#include "game/view.h"

#include <cstdint>

// The conditions' checks: a score (1 or 0 for the yes or no ones) for the agent's node, the level and the clock's time

extern "C"
{
    // The conditions' copy of the chunk manager (the game context sets it with the others): its counters and its chunks
    extern void* g_ConditionsChunkManager RETAIL(G_ChunkManager_0030A0C8);
    // Whether an instance's character rides a Rollerbrawl, as its driver or a passenger (1 or 0)
    f32 RidesRollerbrawl(InstanceContext* instance) RETAIL(FUN_00128c58);
    // Set by the scripts' SetScriptGlobalFlag (command 624)
    extern s32 g_ScriptGlobalFlag RETAIL(D_003098E8);
    // The rank the scripts give every instance at once (0xFF none), which the nodes' own ranks are compared with
    extern u8 g_TriggerRank RETAIL(D_0030A0E9);
}

static_assert(offsetof(InstanceContext, id) == 0x154);
static_assert(offsetof(InstanceContext, parent) == 0xD0);

namespace
{
// The axes of a place's matrix (its rows): x the side, y up and z forward
enum Axis : u32
{
    XAxis = 0,
    YAxis = 1,
    ZAxis = 2,
};

f32 YesNo(bool yes)
{
    return yes ? 1.0f : 0.0f;
}

ObjectNode* Node(GameNode* node)
{
    return static_cast<ObjectNode*>(node);
}

ReferencedObjectFlags InstanceFlags(GameNode* node)
{
    return node->owner->flags;
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

// The player's character's part (the copy the conditions read, game/player.h)
CharacterPart* PlayerPart()
{
    return g_PlayerPart2;
}

CharacterMoveBits MoveBits(const CharacterPart* part)
{
    return part->moveBits;
}

// The kinds of hit of the agent's contact message (the last contact that told it something)
u32 HitKindsOf(GameNode* node)
{
    return Node(node)->agent->contact.hitKinds;
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

GameProgress* Progress()
{
    return &g_ConditionsGameController->progress;
}

bool Asleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// The agent references, forgotten once their instances are asleep (the second kept while the node's flag says so)
InstanceContext* AwakeAgentRef1(ObjectNode* node)
{
    if (node->agentRef1 != nullptr && Asleep(node->agentRef1))
    {
        node->agentRef1 = nullptr;
    }

    return node->agentRef1;
}

InstanceContext* AwakeAgentRef2(ObjectNode* node)
{
    if (node->agentRef2 != nullptr && Asleep(node->agentRef2) && !node->flags.keepsAgentRef2)
    {
        node->agentRef2 = nullptr;
    }

    return node->agentRef2;
}

AgentNode* AgentNodeOfKind(GameNode* node, u32 kind)
{
    return static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, kind));
}

// A character node's agent as the player's character (the same object)
PlayerCharacter* CharacterOf(AgentNode* node)
{
    return reinterpret_cast<PlayerCharacter*>(node->agent);
}

bool HasVehicleOfKind(PlayerCharacter* character, u32 kind)
{
    Vehicle* vehicle = character->vehicle;
    return vehicle != nullptr && vehicle->Kind() == kind;
}

// The instance's attachments node (none without one)
AttachmentsNode* AttachmentsNodeOf(GameNode* node)
{
    return static_cast<AttachmentsNode*>(GetGameNode(&node->owner->nodes, NodeAttachments));
}

// The AI positions at the ends of the route's edge: the route's step the node is at, and the next step toward the route's end
// (the steps go down to the end, step 0; at the end, the end's own). No route: none
AiPosition* StepPosition(const Waypoints* waypoints)
{
    Route* route = waypoints->route;
    return route != nullptr ? route->PositionAt(waypoints->routeIndex) : nullptr;
}

AiPosition* NextStepPosition(const Waypoints* waypoints)
{
    Route* route = waypoints->route;
    if (route == nullptr)
    {
        return nullptr;
    }

    u8 next = waypoints->routeIndex - 1;
    return route->PositionAt(next != Waypoints::NoRouteStep ? next : 0);
}

f32 PositionFlag(const AiPosition* position, u32 flag)
{
    return YesNo(position != nullptr && (position->flags.value & flag) != 0);
}

// A flag of the route's edge (the path from its step to the next), while the route hasn't gone past its end
f32 RoutePathFlag(GameNode* node, u32 flag)
{
    Waypoints* waypoints = Node(node)->waypoints;
    if (waypoints->route == nullptr || waypoints->flags.wrapped || waypoints->routePath == nullptr)
    {
        return 0.0f;
    }

    return YesNo((waypoints->routePath->flags.value & flag) != 0);
}

// The instance's position (made up to date first)
const Vector4& PositionOf(GameNode* node)
{
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    return place->position;
}

// The trajectory controller's cycle about an axis in turns (its 65536ths of a turn made degrees, then turns)
f32 CycleTurns(GameNode* node, u32 axis)
{
    constexpr f32 DegreesPerAngleUnit = 360.0f / FullTurnAngle;
    constexpr f32 TurnsPerDegree = Rounded(1.0 / 360.0);
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    Trajectory* trajectory = Node(node)->trajectory;
    if (trajectory == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(trajectory->cycles[axis]) * DegreesPerAngleUnit * TurnsPerDegree;
}

// The node's rigid body and head tracking (none while the node takes no packets)
ObjectRigidBody* RigidBodyOf(GameNode* node)
{
    if (!TakesPackets(node))
    {
        return nullptr;
    }

    return Node(node)->rigidBody;
}

HeadTracking* HeadTrackingOf(GameNode* node)
{
    if (!TakesPackets(node))
    {
        return nullptr;
    }

    return Node(node)->headTracking;
}

// The level of the perception's sense of a kind (PerceptionSense::Kind; none: 0)
f32 SenseLevel(GameNode* node, u32 kind)
{
    f32 level = 0.0f;
    if (TakesPackets(node))
    {
        void* perception = Node(node)->perception;
        if (perception != nullptr)
        {
            PerceptionValue(perception, kind, &level);
        }
    }

    return level;
}

// The part of the instance's agent node
AgentPart* AgentPartOf(GameNode* node)
{
    return AgentNodeOf(node->owner)->agent->part;
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

// The instance of the character played
InstanceContext* PlayedInstance()
{
    return Progress()->Instance(Progress()->play.character);
}

bool CharacterDead(AgentNode* character)
{
    return static_cast<CharacterAgent*>(character->agent)->state.dead != 0;
}

// The object ID of the agent of an instance's object node (whether there's an instance isn't checked; no node: none)
u16 ObjectIdOf(InstanceContext* instance)
{
    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    return objectNode != nullptr ? objectNode->agent->objectId : NoObjectId;
}

// An object ID (but none) the parameter, by its index in the objects' table (the ID's top bit left out)
f32 ObjectIdIs(u16 id, u32 parameter)
{
    return YesNo(id != NoObjectId && (id & ResourceIndexMask) == parameter);
}

// The last attack of either kind within the window
f32 AttackedByEither(GameNode* node, const u32* time, f32 seconds, u32 kind, u32 other)
{
    AgentPart* part = AgentPartOf(node);
    if (part->AttackedWithin(time, seconds) == 0)
    {
        return 0.0f;
    }

    return YesNo(part->lastAttack == kind || part->lastAttack == other);
}

// A flag of the awake focus instance's (none: no)
f32 FocusFlag(GameNode* node, u32 flag)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    return YesNo(focus != nullptr && (focus->flags.value & flag) != 0);
}

// The pad of a player (else none): the first's, and the second's
GamePad* PadOf(u32 player)
{
    constexpr u32 PlayerOne = 1;
    constexpr u32 PlayerTwo = 2;
    GameController* controller = g_ConditionsGameController;
    if (player == PlayerOne)
    {
        return controller->pad;
    }

    if (player == PlayerTwo)
    {
        return controller->secondPad;
    }

    return nullptr;
}

// The exit point's slot the parameter gives (its low byte)
u32 ExitSlot(const ScriptCondition* condition)
{
    return static_cast<u8>(condition->Parameter());
}
}

f32 NextCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 1.0f;
}

f32 IsCollidableCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(InstanceFlags(node).collisionActive);
}

// The body being checked is what runs when nothing else does
f32 ElseCondition::Check(GameNode*, BehaviourLevel* level, const u32*)
{
    level->elseBody = g_CheckedBody;
    return 0.0f;
}

f32 RandomCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetRandFloat();
}

f32 IsVisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(InstanceFlags(node).visible);
}

// Seconds since the level's last body
f32 TimeInUnitCondition::Check(GameNode*, BehaviourLevel* level, const u32* time)
{
    return static_cast<f32>(static_cast<s32>(*time - level->time)) * g_SecondsPerClockUnit;
}

// Every chunk the focus chunk links loaded
f32 LinkedChunksLoadedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(G_ChunkLoadingManager_->focusLoader->bits.linkedLoaded != 0);
}

// From 1 (none 0)
f32 CurrentKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u8 key = Node(node)->waypoints->key;
    if (key == Waypoints::NoKey)
    {
        return 0.0f;
    }

    return static_cast<f32>(key + 1);
}

f32 AttachedToAnAgentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(InstanceFlags(node).attached);
}

f32 GotAttachedObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(InstanceFlags(node).hasAttachment);
}

// Counted from 1 (none 0)
f32 CurrentKeyEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u32 key = static_cast<u8>(Node(node)->waypoints->key + 1);
    return YesNo(key == Parameter());
}

f32 GotFocusObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Node(node)->flags.focusInstance));
}

f32 GotFocusPositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Node(node)->flags.focusPosition));
}

// Of the model's animator (whether the instance has a model isn't checked)
f32 GotAnimationTimeRemainingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 0.0f;
    }

    return GetAnimationProgress(animator, OgiAnimator::RootJoint);
}

f32 CounterValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(GameCounter(g_ConditionsChunkManager, Parameter()));
}

f32 SqrMoveSpeedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const Vector4& velocity = Node(node)->motion->velocity;
    return velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
}

f32 IsBusyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(InstanceFlags(node).busy);
}

// A bit of the instance's state (bits 0-31; TT Lab's CheckInstanceFlagSet)
f32 SoftFlagSetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((Node(node)->properties->state.value & 1u << (Parameter() & 31)) != 0);
}

f32 GotLinkedObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(GetGameNode(&node->owner->nodes, NodeAttachments) != nullptr);
}

f32 HasStoredPlaceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->storedPlace != nullptr);
}

f32 HasStoredPositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->flags.storedPosition);
}

// The agent's counter of the parameter
f32 InstanceCounterValueCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const u8* counters = Node(node)->agent->counters;
    return static_cast<f32>(counters[Parameter()]);
}

// The rigid body has a kind of motion, and a kind of collisions
f32 RigidBodyHasMotionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = Node(node)->rigidBody;
    bool yes = body != nullptr && body->bits.motionKind != 0;
    return YesNo(yes);
}

f32 RigidBodyCollidesCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = Node(node)->rigidBody;
    bool yes = body != nullptr && body->bits.collisionKind != 0;
    return YesNo(yes);
}

// The rigid body rides an instance (whether there's a rigid body isn't checked)
f32 RigidBodyRidesInstanceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->rigidBody->object != nullptr);
}

f32 PresenceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return Node(node)->agent->part->presence;
}

f32 AlwaysZeroCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The message the level above gave this one: taken once
f32 GotChildMessageOnceEqualsCondition::Check(GameNode*, BehaviourLevel* level, const u32*)
{
    if (level->bits.message != Parameter())
    {
        return 0.0f;
    }

    level->bits.message = NoMessage;
    return 1.0f;
}

f32 IsAttachedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(node->owner->parent != nullptr);
}

// The instance has an ID
f32 HasInstanceIdCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(node->owner->id != -1);
}

f32 KeyPathProgressCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Waypoints* waypoints = Node(node)->waypoints;
    if (waypoints == nullptr)
    {
        return 0.0f;
    }

    return waypoints->pathParameter;
}

f32 KeyPathNumPathsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Waypoints* waypoints = Node(node)->waypoints;
    if (waypoints == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(waypoints->pathCount);
}

f32 KeyPathNumKeysCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(Node(node)->waypoints->keyCount);
}

// Counted from 1
f32 CurrentKeyIsEvenCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u8 key = Node(node)->waypoints->key;
    if (key == Waypoints::NoKey)
    {
        return 0.0f;
    }

    return YesNo(((key + 1) & 1) == 0);
}

f32 CutsceneFinishedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(G_VideoController->state == VideoController::StateFinished);
}

// The y of the instance's up axis (its matrix made up to date first)
f32 UpAxisYCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    return place->matrix.m[YAxis][1];
}

// Bit 0 of the first integer property clear
f32 IntProperty0Bit0ClearCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((Node(node)->properties->GetInt(0) & 1) == 0);
}

f32 CutsceneMusicReadyCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(VideoReady(G_VideoController) != 0);
}

// The rigid body has a physics body
f32 HasPhysicsBodyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = Node(node)->rigidBody;
    return YesNo(body != nullptr && body->physicsBody != nullptr);
}

f32 CountedInstancesCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(g_CountedInstances);
}

f32 CountedValueCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return Node(node)->countedValue;
}

f32 RankAboveGlobalRankCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u8 own = Node(node)->rank;
    u8 global = g_TriggerRank;
    if (own == ObjectNodeBase::NoRank || global == ObjectNodeBase::NoRank)
    {
        return 0.0f;
    }

    return static_cast<f32>(own - global);
}

f32 NoOp173Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 AlwaysCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 1.0f;
}

f32 NeverCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 LinkedChunksQueuedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(G_ChunkLoadingManager_->focusLoader->bits.linkedQueued != 0);
}

f32 PlayerIsCrouchingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(PlayerPart()->moveBits.crouching != 0);
}

f32 PlayerIsGroundedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo(part->flags.onGround != 0);
}

// The wumpa fruit
f32 WumpaFruitCountCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->counts.wumpa));
}

f32 NoOpWillHitLowWallCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The player's gun's last shot's charge (none -1)
f32 PlayerGunShotChargeCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    Gun* gun = g_PlayerCharacter2->gun;
    if (gun == nullptr)
    {
        return -1.0f;
    }

    return gun->shotCharge;
}

// Crossing the route's edge takes flying (no edge: no; whether there's a route or it went past its end isn't asked)
f32 EdgeNeedsFlyingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const AiPath* path = Node(node)->waypoints->routePath;
    if (path == nullptr)
    {
        return 0.0f;
    }

    return YesNo(path->flags.needsFlight != 0);
}

f32 PlayerIsMovingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(PlayerPart()->flags.moving != 0);
}

f32 PlayerIsWalkingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(MoveBits(PlayerPart()).walking != 0);
}

f32 PlayerIsRunningCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(MoveBits(PlayerPart()).running != 0);
}

// The walking bit (retail's)
f32 PlayerIsWalkingDuplicateCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(MoveBits(PlayerPart()).walking != 0);
}

f32 PlayerIsFallingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(PlayerPart()->flags.falling != 0);
}

f32 PlayerHoldingMultiToolCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo(MoveBits(part).shooting != 0);
}

// The attack the player's moves give its part: tied to the other character (the second, and the leader's slam)
f32 PlayerIsSlammingTiedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo(part->bits.attackKind == AttackTied);
}

f32 PlayerIsAirborneCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo(MoveBits(part).jumping != 0);
}

// The agent's part may damage the character
f32 CanDamageCharacterCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(node->owner)->agent->part);
    return YesNo(part->bits.canDamageCharacter != 0);
}

f32 NoOp570Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 NoOp571Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 NoOpCutsceneSkippedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The character agent's vehicle (whether the instance has a character node isn't checked)
f32 RidesVehicleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* character = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, NodeCharacter));
    return YesNo(static_cast<CharacterAgent*>(character->agent)->vehicle != nullptr);
}

f32 IsPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    return YesNo(node->owner == player);
}

f32 HitByKickCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitKick) != 0);
}

f32 HitBySpinCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitSpin) != 0);
}

f32 HitByKind18Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitKind18) != 0);
}

f32 HitByProjectileCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitProjectile) != 0);
}

f32 HitByKneeDropCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitKneeDrop) != 0);
}

f32 IsVehicleRollerbrawlCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RidesRollerbrawl(node->owner);
}

f32 HitByElectricCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitElectric) != 0);
}

f32 HitByExplosionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitExplosion) != 0);
}

f32 PlayerScriptFlagClearCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(MoveBits(PlayerPart()).scriptFlag == 0);
}

// The player stands on the instance (on its hull, or riding it)
f32 HasActorWeightCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(g_PlayerCharacter2->StandsOn(node->owner) != 0);
}

f32 HitByWaterCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitWater) != 0);
}

f32 HitByFallThroughCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitFallingThrough) != 0);
}

f32 PlayerRidesRollerbrawlCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return RidesRollerbrawl(PlayerInstance());
}

f32 ScriptGlobalFlagCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(g_ScriptGlobalFlag);
}

// The node is in water (it has the surface of the water it's in)
f32 InWaterCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->waterSurface != ObjectNode::NoSurface);
}

// The timed play's count
f32 TimedPlayCountCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->counts.count));
}

// The timed play's time left (seconds)
f32 TimedPlayTimeLeftCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(Progress()->timeLeft) * g_SecondsPerClockUnit;
}

f32 PlayerGunSecondCountCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    Gun* gun = g_PlayerCharacter2->gun;
    if (gun == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(gun->bits.secondCount));
}

f32 PlayerAmmoCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    Gun* gun = g_PlayerCharacter2->gun;
    if (gun == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(gun->bits.ammo));
}

f32 HitByHeavyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & HitHeavy) != 0);
}

// Pairing 5 of the second character with the first (the progress's, which the scripts' SetPlayerMode sets)
f32 PairingIs5Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(Progress()->play.pairing == Pairing5);
}

f32 HitByBurningCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((HitKindsOf(node) & (HitBurning | HitKind22)) != 0);
}

// The area the story has got to
f32 StoryAreaCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->play.story));
}

// The height of the player's vehicle above the ground (the Humiliskate's; no vehicle: 0)
f32 PlayerVehicleHeightCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    const Vehicle* vehicle = g_PlayerCharacter2->vehicle;
    if (vehicle == nullptr)
    {
        return 0.0f;
    }

    return vehicle->height;
}

// The area play is in
f32 PlayAreaIsCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(Progress()->play.area == Parameter());
}

// Of the model's animator (whether the instance has a model isn't checked): no animator, or nothing left of its animation
f32 AnimationFinishedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 1.0f;
    }

    return GetAnimationProgress(animator, OgiAnimator::RootJoint) > 0.0f ? 0.0f : 1.0f;
}

// The keys went round (or the route went past its end)
f32 IsPathCompleteCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->waypoints->flags.wrapped);
}

f32 GetRouteCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->waypoints->route != nullptr);
}

f32 GotKeysCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->waypoints->keyCount != 0);
}

// The instance within the AI position of the route's step it's at (its edge's start)
f32 InsideEdgeStartNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* position = StepPosition(Node(node)->waypoints);
    return YesNo(position != nullptr && IsWithinAiPosition(position, node) != 0);
}

// Of the next step (the edge's end)
f32 InsideEdgeEndNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* position = NextStepPosition(Node(node)->waypoints);
    return YesNo(position != nullptr && IsWithinAiPosition(position, node) != 0);
}

// The first integer property
f32 ActorSubtypeEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(static_cast<u32>(Node(node)->properties->GetInt(0)) == Parameter());
}

// The last trigger message within the window (its time past the clock's: no)
f32 GotAnyUserMessageCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    ObjectNode* object = Node(node);
    if (static_cast<s32>(*time) < static_cast<s32>(object->messageTime))
    {
        return 0.0f;
    }

    return static_cast<f32>(object->MessageWithin(time, window));
}

f32 GotUserMessageEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    ObjectNode* object = Node(node);
    if (static_cast<s32>(*time) < static_cast<s32>(object->messageTime))
    {
        return 0.0f;
    }

    return static_cast<f32>(object->MessageWithin(Parameter(), time, window));
}

// The rigid body touches an instance (whether the node takes packets is asked first)
f32 TouchingAnyAgentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = RigidBodyOf(node);
    return YesNo(body != nullptr && body->bits.touchingInstance);
}

// The threshold doubled when the counter has its value (so it passes), else none
f32 CounterValueEqualsThresholdCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    f32 count = static_cast<f32>(GameCounter(g_ConditionsChunkManager, Parameter()));
    return count == threshold ? threshold + threshold : 0.0f;
}

// The attachments' path was freed with the last attachment taken off: taken once
f32 LostAllAttachmentsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(node);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    if (attachments->bits.noPath == 0)
    {
        return 0.0f;
    }

    attachments->bits.noPath = 0;
    return 1.0f;
}

f32 XCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, XAxis);
}

f32 YCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, YAxis);
}

f32 ZCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, ZAxis);
}

// An agent reference whose instance is awake (one asleep forgotten: the second's even with the node's flag)
f32 GotAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    InstanceContext* reference = AwakeAgentRef1(object);
    if (reference == nullptr)
    {
        return 0.0f;
    }

    if (Asleep(reference))
    {
        object->agentRef1 = nullptr;
        return 0.0f;
    }

    return 1.0f;
}

f32 GotAgentRef2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    InstanceContext* reference = AwakeAgentRef2(object);
    if (reference == nullptr)
    {
        return 0.0f;
    }

    if (Asleep(reference))
    {
        object->agentRef2 = nullptr;
        return 0.0f;
    }

    return 1.0f;
}

f32 FoundCoverCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->flags.foundCover);
}

f32 FoundNoCoverCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->flags.noCover);
}

f32 CoverSearchEndedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->flags.searchEnded);
}

// The node's knock countdown, a share of its most (255)
f32 KnockCountdownCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 PerCount = Rounded(1.0 / 255.0);
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return static_cast<f32>(Node(node)->reactions.knockCountdown) * PerCount;
}

// The rigid body on the ground this frame (whether the node takes packets is asked first)
f32 RigidBodyOnGroundCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = RigidBodyOf(node);
    return YesNo(body != nullptr && body->state.onGround);
}

// The threshold doubled when the agent's counter of the parameter has it (so it passes), else none
f32 InstanceCounterEqualsThresholdCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const u8* counters = Node(node)->agent->counters;
    return static_cast<f32>(counters[Parameter()]) == threshold ? threshold + threshold : 0.0f;
}

// The head tracking's joints hooked with a limit hit this frame: any, the turn about y below or above its limit, the turn about x
// below or above its limit
f32 HeadAtLimitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    HeadTracking* tracking = HeadTrackingOf(node);
    return YesNo(tracking != nullptr && tracking->bits.hooked && tracking->bits.limited);
}

f32 HeadYawBelowLimitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    HeadTracking* tracking = HeadTrackingOf(node);
    return YesNo(tracking != nullptr && tracking->bits.hooked && tracking->bits.yawBelow);
}

f32 HeadYawAboveLimitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    HeadTracking* tracking = HeadTrackingOf(node);
    return YesNo(tracking != nullptr && tracking->bits.hooked && tracking->bits.yawAbove);
}

f32 HeadPitchBelowLimitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    HeadTracking* tracking = HeadTrackingOf(node);
    return YesNo(tracking != nullptr && tracking->bits.hooked && tracking->bits.pitchBelow);
}

f32 HeadPitchAboveLimitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    HeadTracking* tracking = HeadTrackingOf(node);
    return YesNo(tracking != nullptr && tracking->bits.hooked && tracking->bits.pitchAbove);
}

// The route's step (whether there's a route isn't checked)
f32 RouteStepUncheckedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return static_cast<f32>(Node(node)->waypoints->routeIndex);
}

f32 RouteStepCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    Waypoints* waypoints = Node(node)->waypoints;
    if (waypoints->route == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(waypoints->routeIndex);
}

// The current linked instance the last
f32 OnLastLinkedObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(node);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    AttachmentsNodeBits bits = attachments->bits;
    return YesNo(bits.currentLinked == bits.linkedCount - 1);
}

// Above where the instance's placement put it
f32 HeightAboveStartCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionOf(node).y - Node(node)->informationPointer->position.y;
}

// The levels of the perception's senses: of the instances around, of its node's speed and the rising one
f32 Sense0LevelCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SenseLevel(node, PerceptionSense::KindInstances);
}

f32 Sense2LevelCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SenseLevel(node, PerceptionSense::KindSpeed);
}

f32 Sense1LevelCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SenseLevel(node, PerceptionSense::KindRising);
}

// The rigid body against a wall this frame (whether the node takes packets is asked first)
f32 RigidBodyAgainstWallCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = RigidBodyOf(node);
    return YesNo(body != nullptr && body->state.againstWall);
}

// The linked instances' count
f32 HasXLinksCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(node);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(attachments->LinkedCount());
}

f32 PositionXCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionOf(node).x;
}

f32 PositionYCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionOf(node).y;
}

f32 PositionZCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionOf(node).z;
}

// The first agent reference's instance busy (none: yes)
f32 AgentRef1IsBusyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    InstanceContext* reference = AwakeAgentRef1(object);
    if (reference == nullptr)
    {
        return 1.0f;
    }

    if (Asleep(reference))
    {
        object->agentRef1 = nullptr;
        return 0.0f;
    }

    return YesNo(reference->flags.busy);
}

// The rigid body touched a surface whose contact message it was told, taken once (whether the node takes packets isn't asked)
f32 TouchedMessageSurfaceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectRigidBody* body = Node(node)->rigidBody;
    if (body == nullptr || !body->state.touchedMessageSurface)
    {
        return 0.0f;
    }

    body->state.touchedMessageSurface = 0;
    return 1.0f;
}

// Of the node it takes its object from when there's one (no key: no)
f32 KeyPathOnLastKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    GameNode* source = Node(node)->sourceNode;
    Waypoints* waypoints = Node(source != nullptr ? source : node)->waypoints;
    if (waypoints->key == Waypoints::NoKey)
    {
        return 0.0f;
    }

    return YesNo(waypoints->key >= waypoints->keyCount - 1);
}

// No player: the instances in no chunk
f32 IsInPlayerChunkCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    ChunkData* chunk = player != nullptr ? player->chunk : nullptr;
    return YesNo(node->owner->chunk == chunk);
}

// A behaviour in the object's slot (the object of the node it takes its object from when there's one)
f32 HasScriptInSlotCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    GameObject* gameObject = object->sourceNode != nullptr ? SourceObject(object->sourceNode) : object->object;
    u16 id;
    GetObjectBehaviourId(&id, gameObject, Parameter());
    return YesNo(id != NoScriptId);
}

// Seconds since the running runner's mark, never below none (no mark: none)
f32 TimeSinceMarkCondition::Check(GameNode*, BehaviourLevel*, const u32* time)
{
    s32 mark = static_cast<s32>(g_CurrentRunner->markedTime);
    f32 seconds = -1.0f;
    if (static_cast<f32>(mark) * g_SecondsPerClockUnit != 0.0f)
    {
        seconds = static_cast<f32>(static_cast<s32>(*time - mark)) * g_SecondsPerClockUnit;
    }

    return seconds < 0.0f ? 0.0f : seconds;
}

f32 PlayerHitPointsCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(PlayerPart()->flags.hitPoints);
}

// The agent's state's shadow flag
f32 ShadowActiveCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* agentNode = AgentNodeOf(node->owner);
    if (agentNode == nullptr)
    {
        return 0.0f;
    }

    return YesNo(agentNode->agent->properties->state.shadowActive != 0);
}

// The crate holds wumpa fruit (no crate node: no)
f32 CrateHasWumpaCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* crate = AgentNodeOfKind(node, NodeCrate);
    if (crate == nullptr)
    {
        return 0.0f;
    }

    auto* part = static_cast<CratePart*>(crate->agent->part);
    return YesNo(part->crate.wumpaFruit != 0);
}

// Any attack within the window
f32 AgentWasTouchedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->AttackedWithin(time, window) != 0);
}

// Whether there's a creature node isn't checked
f32 AgentHitPointsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CreaturePart*>(AgentNodeOfKind(node, NodeCreature)->agent->part);
    return static_cast<f32>(part->flags.hitPoints);
}

// The last attack within the window by the tied characters
f32 AgentWasHitByTiedPairCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    AgentPart* part = AgentPartOf(node);
    return YesNo(part->AttackedWithin(time, window) != 0 && part->lastAttack == AttackTied);
}

// By a character the other threw, from a spin or a jump
f32 AgentWasHitByThrownCharacterCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    AgentPart* part = AgentPartOf(node);
    return YesNo(part->AttackedWithin(time, window) != 0 &&
                 (part->lastAttack == AttackThrownFromSpin || part->lastAttack == AttackThrownFromJump));
}

f32 AgentWasAttackedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->HitWithin(time, window) != 0);
}

// An attack of a kind within the window: landed on, walked into, hit from below
f32 AgentWasJumpedOnCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->AttackedWithin(AttackLandOn, time, window) != 0);
}

f32 AgentWasWalkedIntoCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->AttackedWithin(AttackWalkInto, time, window) != 0);
}

f32 AgentWasHeadbuttedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->AttackedWithin(AttackFromBelow, time, window) != 0);
}

// The gate's number
f32 PayGateNumberCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* gate = AgentNodeOfKind(node, NodePayGate);
    if (gate == nullptr)
    {
        return 0.0f;
    }

    auto* part = static_cast<PayGatePart*>(gate->agent->part);
    return static_cast<f32>(part->payGate.number);
}

// The AI position of the route's step it's at airborne
f32 NodeIsAirborneCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::Airborne);
}

// Crossing the route's edge takes a jump, a long jump, a high jump
f32 EdgeNeedsJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, AiPathFlags::NeedsJump);
}

f32 EdgeNeedsLongJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, AiPathFlags::NeedsLongJump);
}

f32 EdgeNeedsHighJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, AiPathFlags::NeedsHighJump);
}

f32 PlayerIsCoOpLinkedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterMoveBits bits = MoveBits(PlayerPart());
    return YesNo(bits.linkedFirst != 0 || bits.linkedSecond != 0);
}

// The attack the player's moves give its part a spin (or its variant), a slide
f32 PlayerIsSpinningCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    u32 attack = part->bits.attackKind;
    return YesNo(attack == AttackSpin || attack == AttackSpinVariant);
}

f32 PlayerIsSlidingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    u32 attack = part->bits.attackKind;
    return YesNo(attack == AttackSlide || attack == AttackSlideVariant);
}

// On the pad of the player the parameter gives
f32 IsCirclePressedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetButtonPressure(PadOf(Parameter()), PadCircle);
}

f32 IsSquarePressedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetButtonPressure(PadOf(Parameter()), PadSquare);
}

f32 IsTrianglePressedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetButtonPressure(PadOf(Parameter()), PadTriangle);
}

f32 IsR1PressedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetButtonPressure(PadOf(Parameter()), PadR1);
}

// The character part's move bits (whether there's a character node isn't checked)
f32 NoGroundAheadCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CharacterPart*>(AgentNodeOfKind(node, NodeCharacter)->agent->part);
    return YesNo(MoveBits(part).noGroundAhead != 0);
}

// The player's gun just shot (a normal or a charged shot)
f32 PlayerJustShotCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    Gun* gun = g_PlayerCharacter2->gun;
    return YesNo(gun != nullptr && gun->bits.state == Gun::StateShot);
}

// The player's jump in Cortex's radial blast (its hang, rise or fall)
f32 IsDownBlastCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    const JumpController* jump = g_PlayerCharacter2->jump;
    if (jump == nullptr)
    {
        return 0.0f;
    }

    u32 state = jump->bits.state;
    return YesNo(state >= JumpController::StateBlastHang && state <= JumpController::StateBlastFalling);
}

// Flags of the AI positions of the route's edge: of its start (the route's step it's at; the duplicates the same) and of its end
// (the next step)
f32 EdgeStartNodeBlockedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::Blocked);
}

f32 IsTiedSecondCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CharacterPart*>(AgentNodeOfKind(node, NodeCharacter)->agent->part);
    return YesNo(MoveBits(part).linkedSecond != 0);
}

f32 CharacterHasVehicleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, NodeCharacter);
    return YesNo(character != nullptr && CharacterOf(character)->vehicle != nullptr);
}

// The character's vehicle of a kind: 2 and 4 (no vehicle has either), the Humiliskate, the hoverboard (the player's without a
// character node)
f32 IsVehicleKind2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, NodeCharacter);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), Vehicle::KindUnused2));
}

f32 IsVehicleHumiliskateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, NodeCharacter);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), Vehicle::KindHumiliskate));
}

f32 IsVehicleKind4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, NodeCharacter);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), Vehicle::KindUnused4));
}

f32 IsVehicleHoverboardCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, NodeCharacter);
    return YesNo(HasVehicleOfKind(character != nullptr ? CharacterOf(character) : g_PlayerCharacter2, Vehicle::KindHoverboard));
}

f32 EdgeStartNodeFlag5Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::Attached);
}

f32 EdgeStartNodeFlag4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::NeverTaken);
}

f32 EdgeStartNodeFlag6Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::ScriptFlag6);
}

// Flag 8 of the route's edge
f32 EdgeFlag8Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, AiPathFlags::ScriptFlag8);
}

f32 EdgeEndNodeFlag5Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(NextStepPosition(Node(node)->waypoints), AiPositionFlags::Attached);
}

f32 EdgeEndNodeFlag4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(NextStepPosition(Node(node)->waypoints), AiPositionFlags::NeverTaken);
}

f32 EdgeEndNodeFlag6Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(NextStepPosition(Node(node)->waypoints), AiPositionFlags::ScriptFlag6);
}

f32 EdgeStartNodeFlag5DuplicateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::Attached);
}

f32 EdgeStartNodeFlag4DuplicateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::NeverTaken);
}

f32 EdgeStartNodeFlag6DuplicateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::ScriptFlag6);
}

f32 EdgeStartNodeFlag2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::AlwaysTaken);
}

f32 EdgeEndNodeFlag2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(NextStepPosition(Node(node)->waypoints), AiPositionFlags::AlwaysTaken);
}

f32 EdgeStartNodeFlag2DuplicateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), AiPositionFlags::AlwaysTaken);
}

// None of the airborne flag and flags 2, 4 and 6 (no position: no)
f32 EdgeStartNodeFlagsClearCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u32 Flags =
        AiPositionFlags::Airborne | AiPositionFlags::AlwaysTaken | AiPositionFlags::NeverTaken | AiPositionFlags::ScriptFlag6;
    AiPosition* position = StepPosition(Node(node)->waypoints);
    return YesNo(position != nullptr && (position->flags.value & Flags) == 0);
}

// The played character pushes a body (no character played: the nodes read at 0xD4, retail's)
f32 IsPushingObjectCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    GameProgress* progress = Progress();
    InstanceContext* instance = progress->Instance(progress->play.character);
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(instance), NodeCharacter));
    Reference* pushed = static_cast<CharacterAgent*>(character->agent)->pushedBody;
    return YesNo(pushed != nullptr && pushed->object != nullptr);
}

f32 GameIsPlayingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(g_ConditionsGameController->State() == GameController::StatePlaying);
}

f32 IsMoviePlayingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(g_ConditionsGameController->State() == GameController::StateMovie);
}

// An instance hanging on the exit point of the parameter's slot
f32 GotAttachmentOnExitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(node);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    AttachmentsPath* path = attachments->path;
    if (path == nullptr)
    {
        return 0.0f;
    }

    return YesNo(SlottedAttachment(path, ExitSlot(this)) != nullptr);
}

// The played character's home chunk not the one its instance is in (whether there's a character played or a character node
// isn't checked)
f32 PlayerOutsideHomeChunkCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(played), NodeCharacter));
    auto* agent = static_cast<CharacterAgent*>(character->agent);
    ChunkData* home = agent->homeChunk;
    return YesNo(home != played->chunk);
}

// The first runner's starter's originator a character that isn't the player, while play is alone (PairingAlone)
f32 TriggeredByOtherCharacterCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    BehaviourRunner* runner = Node(node)->runners[0];
    if (runner == nullptr || runner->receivers == nullptr)
    {
        return 0.0f;
    }

    auto* originator = static_cast<InstanceContext*>(runner->receivers->originator);
    if (originator == PlayerInstance() || GetGameNode(&originator->nodes, NodeCharacter) == nullptr)
    {
        return 0.0f;
    }

    return YesNo(Progress()->play.pairing == PairingAlone);
}

// The physics body touched its chunk's triangles this frame (without one, the rigid body touches the world)
f32 TouchingWorldCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectRigidBody* body = Node(node)->rigidBody;
    if (body == nullptr)
    {
        return 0.0f;
    }

    if (body->physicsBody != nullptr)
    {
        return YesNo(body->physicsBody->bodyFlags.touchedWorld != 0);
    }

    return YesNo(body->bits.touchingWorld);
}

// The agent's persistent flag (in its chunk's own store or the other one; none: no)
f32 IsLoadZoneStateSetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Agent* agent = Node(node)->agent;
    PropertyHolder* properties = agent->properties;
    if (properties->state.persistentFlag == 0)
    {
        return 0.0f;
    }

    ChunkEntry* chunk = ChunkOfIndex(g_ConditionsChunkManager, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* flags = properties->state.flagInChunkStore != 0 ? chunk->savedFlags : chunk->unsavedFlags;
    if (flags == nullptr)
    {
        return 0.0f;
    }

    return YesNo(GetPersistentFlag(flags, id) != 0);
}

f32 FocusInSameChunkCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    return YesNo(focus != nullptr && focus->chunk == node->owner->chunk);
}

// A spin, a knee drop (the slam), a slide, or their variants
f32 AgentWasSpunCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, window, AttackSpin, AttackSpinVariant);
}

f32 AgentWasKneeDroppedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, window, AttackSlam, AttackSlamVariant);
}

f32 AgentWasSlidCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, window, AttackSlide, AttackSlideVariant);
}

// Whether the player's Humiliskate is crouched (no player: its nodes read at 0xD4)
f32 PlayerHumiliskateCrouchedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayerInstance()), NodeCharacter));
    if (character == nullptr)
    {
        return 0.0f;
    }

    Vehicle* vehicle = reinterpret_cast<PlayerCharacter*>(character->agent)->vehicle;
    if (vehicle == nullptr || vehicle->Kind() != Vehicle::KindHumiliskate)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<HumiliskateVehicle*>(vehicle)->crouched);
}

f32 PlayerIsDeadCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    auto* character = static_cast<AgentNode*>(GetGameNode(&played->nodes, NodeCharacter));
    return YesNo(character != nullptr && CharacterDead(character));
}

// The object of the agent reference's instance (its object node's agent's; the parameter an ID)
f32 AgentRef1ActorEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    return ObjectIdIs(ObjectIdOf(reference), Parameter());
}

// The squared distance from where the instance's placement put it
f32 MeToInitPosSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 start = Node(node)->informationPointer->position;
    const Vector4& position = PositionOf(node);
    f32 x = position.x - start.x;
    f32 y = position.y - start.y;
    f32 z = position.z - start.z;
    return x * x + y * y + z * z;
}

f32 AgentRef2ActorEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef2(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    return ObjectIdIs(ObjectIdOf(reference), Parameter());
}

// The awake focus instance the first agent reference (forgotten once asleep)
f32 FocusIsAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    if (focus == nullptr)
    {
        return 0.0f;
    }

    return YesNo(focus == AwakeAgentRef1(Node(node)));
}

// The awake focus instance visible, busy, holding an attached object
f32 FocusIsVisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, ReferencedObjectFlags::Visible);
}

// The threshold doubled when the focus's agent's counter (the one the condition keeps) has it (so it passes), else none
// (whether the focus has an object node isn't checked)
f32 FocusInstanceCounterEqualsThresholdCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    if (focus == nullptr)
    {
        return 0.0f;
    }

    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&focus->nodes, NodeObject));
    const u8* counters = objectNode->agent->counters;
    return static_cast<f32>(counters[counter]) == threshold ? threshold + threshold : 0.0f;
}

f32 FocusIsBusyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, ReferencedObjectFlags::Busy);
}

// The physics body touched another body or its chunk's triangles this frame; without one, while the parameter is none, the rigid
// body touches the world or an instance
f32 TouchingAnythingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectRigidBody* body = Node(node)->rigidBody;
    if (body == nullptr)
    {
        return 0.0f;
    }

    if (body->physicsBody != nullptr)
    {
        return YesNo(body->physicsBody->bodyFlags.touchedBody != 0 || body->physicsBody->bodyFlags.touchedWorld != 0);
    }

    if (Parameter() != 0)
    {
        return 0.0f;
    }

    return YesNo(body->bits.touchingWorld || body->bits.touchingInstance);
}

f32 FocusHasAttachmentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, ReferencedObjectFlags::HasAttachment);
}

// From the node's stored position (none: far)
f32 MeToStoredPositionSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    if (!object->flags.storedPosition)
    {
        return Infinite;
    }

    Vector4 stored = object->storedPosition;
    const Vector4& position = PositionOf(node);
    f32 x = position.x - stored.x;
    f32 y = position.y - stored.y;
    f32 z = position.z - stored.z;
    return x * x + y * y + z * z;
}

f32 FocusActorEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    if (focus == nullptr)
    {
        return 0.0f;
    }

    return ObjectIdIs(ObjectIdOf(focus), Parameter());
}

// Cortex's character dead and the played one alive
f32 CortexDeadPlayerAliveCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    InstanceContext* cortex = Progress()->characters[CharacterCortex] != nullptr
                                  ? static_cast<InstanceContext*>(Progress()->characters[CharacterCortex]->object)
                                  : nullptr;
    if (cortex == nullptr)
    {
        return 0.0f;
    }

    auto* cortexCharacter = static_cast<AgentNode*>(GetGameNode(&cortex->nodes, NodeCharacter));
    if (cortexCharacter == nullptr || !CharacterDead(cortexCharacter))
    {
        return 0.0f;
    }

    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    auto* playedCharacter = static_cast<AgentNode*>(GetGameNode(&played->nodes, NodeCharacter));
    if (playedCharacter == nullptr)
    {
        return 0.0f;
    }

    return YesNo(!CharacterDead(playedCharacter));
}

// How far above the first agent reference's instance the instance is (none: none)
f32 HeightAboveAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    ObjectPlace* place = reference->place;
    place->SyncPosition();
    Vector4 other = place->position;
    return PositionOf(node).y - other.y;
}

// An awake focus instance, a focus position, or the focus instance flag without an instance
f32 GotAnyFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    if (object->AwakeFocus() != nullptr)
    {
        return 1.0f;
    }

    return YesNo(object->flags.focusPosition || object->flags.focusInstance);
}

namespace
{
// How far above an instance's feet the checks look from and at
constexpr f32 EyeHeight = 0.5f;
// How many instances the queries of the lines of sight, of the ground probes and of the overlaps find at most
constexpr u32 SightResults = 0x80;
constexpr u32 ProbeResults = 0x40;
constexpr u32 OverlapResults = 0x10;

// An instance's place's position (made up to date first)
Vector4 PlacePosition(InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    return place->position;
}

// An axis of an instance's place (its matrix made up to date first)
Vector4 PlaceAxis(InstanceContext* instance, u32 axis)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    return *RowOf(&place->matrix, axis);
}

// How far an axis points along the way from a point to another raised to the eyes (the way made a unit long first)
f32 AxisAlongWay(const Vector4& axis, const Vector4& from, Vector4 to, f32 eyeHeight = EyeHeight)
{
    to.x = to.x - from.x;
    to.y = (to.y + eyeHeight) - from.y;
    to.z = to.z - from.z;
    f32 inverse = InverseLength(&to, LengthEpsilon);
    to.x *= inverse;
    to.y *= inverse;
    to.z *= inverse;
    return axis.x * to.x + axis.y * to.y + axis.z * to.z;
}

// The way made a unit long, and how far an axis points along it
f32 AxisAlongUnit(const Vector4& axis, Vector4* way)
{
    f32 inverse = InverseLength(way, LengthEpsilon);
    way->x *= inverse;
    way->y *= inverse;
    way->z *= inverse;
    return axis.x * way->x + axis.y * way->y + axis.z * way->z;
}

// The middle of an instance's collision box
Vector4 BoxMiddle(InstanceContext* instance)
{
    Vector4 middle = instance->collision.box.min;
    middle.x = (middle.x + instance->collision.box.max.x) * 0.5f;
    middle.y = (middle.y + instance->collision.box.max.y) * 0.5f;
    middle.z = (middle.z + instance->collision.box.max.z) * 0.5f;
    return middle;
}

// The query of the instances in a line of sight, the instance itself and its attachment left out
void MakeSightQuery(InstanceQuery* query, void** results, InstanceContext* instance)
{
    query->results = results;
    query->count = 0;
    query->most = SightResults;
    query->distance = Infinite;
    query->wantedFlags = ReferencedObjectFlags::CollisionActive;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->bits.value = InstanceQueryBits::AllWanted;
    query->skipped[0] = nullptr;
    query->instance = nullptr;
    query->skipped[1] = nullptr;
    SkipInQuery(query, instance);
}

// The character agent of an instance's character node
CharacterAgent* CharacterAgentOfInstance(InstanceContext* instance)
{
    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter))->agent);
}

// A character's eye height: its body's height, its crouching height while it crouches
f32 CharacterEyeHeight(CharacterAgent* agent)
{
    const CharacterBody* body = agent->body;
    auto* part = static_cast<CharacterPart*>(agent->Part());
    return part->moveBits.crouching == 1 ? body->crouchHeight : body->height;
}

// Whether a line of sight from a point along a way to an instance is clear: nothing stopped it, or the instance did (the
// instances of the node kinds given stop it, a bit per kind)
bool SightReaches(InstanceContext* instance, ChunkData* chunk, const Vector4* from, Vector4* way, InstanceQuery* query,
                  u32 nodeKinds)
{
    u32 blocked = LineOfSight(chunk, from, way, SurfaceFlags::BlocksLineOfSight, query, nodeKinds);
    if (blocked != 0 && query->instance == instance)
    {
        blocked = 0;
    }

    return blocked == 0;
}

// The end of a way moved along a place's axes (its matrix made up to date first): from a point half a unit above its
// position, the axes times the offset added
bool WayAlongAxesBlocked(GameNode* node, const Vector4& offset)
{
    InstanceContext* instance = node->owner;
    Vector4 start = PlacePosition(instance);
    start.y = start.y + EyeHeight;
    Vector4 end = start;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    const Matrix4x4& matrix = place->matrix;
    end.x = end.x + (matrix.m[0][0] * offset.x + matrix.m[1][0] * offset.y + matrix.m[2][0] * offset.z);
    end.y = end.y + (matrix.m[0][1] * offset.x + matrix.m[1][1] * offset.y + matrix.m[2][1] * offset.z);
    end.z = end.z + (matrix.m[0][2] * offset.x + matrix.m[1][2] * offset.y + matrix.m[2][2] * offset.z);
    Vector4 hit;
    return GetCollisionCheck(instance->chunk, &start, &end, SurfaceFlags::SolidToObjects, nullptr, &hit, nullptr) != 0;
}

// The position of the node's focus (an awake focus instance's, else the focus position): whether it has one
bool FocusPositionOf(ObjectNode* object, Vector4* position)
{
    if (object->flags.focusInstance)
    {
        InstanceContext* focus = object->focusInstance;
        if (focus == nullptr || Asleep(focus))
        {
            if (focus != nullptr)
            {
                object->focusInstance = nullptr;
                object->flags.value &= ~ObjectNodeFlags::FocusMask;
            }

            return false;
        }

        *position = PlacePosition(focus);
        return true;
    }

    if (object->flags.focusPosition)
    {
        *position = object->focusPosition;
        return true;
    }

    return false;
}
}

// How far the instance's forward axis points at the played character's eyes (none: none)
f32 MeFacingPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    return AxisAlongWay(forward, position, PlacePosition(played));
}

f32 PlayerFacingMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(played);
    Vector4 forward = PlaceAxis(played, ZAxis);
    return AxisAlongWay(forward, position, PlacePosition(node->owner));
}

// How far the played character's x axis points at the instance's eyes
f32 PlayerSideOffsetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(played);
    Vector4 side = PlaceAxis(played, XAxis);
    return AxisAlongWay(side, position, PlacePosition(node->owner));
}

// 1 when nothing (of the sight's surfaces) is between the eyes of the instance and of the played character
f32 ClearLineOfSightToPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 to = PlacePosition(played);
    Vector4 from = PlacePosition(node->owner);
    to.y = to.y + EyeHeight;
    from.y = from.y + EyeHeight;
    Vector4 hit;
    return GetCollisionCheck(node->owner->chunk, &from, &to, SurfaceFlags::BlocksLineOfSight, nullptr, &hit, nullptr) != 0
               ? 0.0f
               : 1.0f;
}

// Twice the threshold when the instance faces the played character more than it and the collision doesn't block the view
f32 CanSeePlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 from = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    Vector4 way = PlacePosition(played);
    from.y = from.y + EyeHeight;
    way.y = way.y + EyeHeight;
    Vector4 to = way;
    way.x = way.x - from.x;
    way.z = way.z - from.z;
    way.y = way.y - from.y;
    if (!(threshold < AxisAlongUnit(forward, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &from, &to, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// Twice the threshold when the line of sight between the middles of their boxes reaches the played character (whether there's
// one isn't checked once it's found)
f32 PlayerVisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 to = BoxMiddle(played);
    Vector4 from = BoxMiddle(node->owner);
    Vector4 way = to;
    way.x = way.x - from.x;
    way.y = way.y - from.y;
    way.z = way.z - from.z;
    void* results[SightResults];
    InstanceQuery query;
    MakeSightQuery(&query, results, node->owner);
    if (!SightReaches(played, node->owner->chunk, &from, &way, &query, g_SolidKinds))
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// The operated instance's gun shot no more than 0.2 seconds ago, it faces the instance and the instance is nearer to its aim (a
// line 1000 units long, on the ground) than its box reaches
f32 ShotAtMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 ShotSeconds = Rounded(0.2);
    constexpr f32 AimLength = 1000.0f;
    InstanceContext* operated = g_OperatedInstance != nullptr ? static_cast<InstanceContext*>(g_OperatedInstance->object) : nullptr;
    if (operated == nullptr)
    {
        return 0.0f;
    }

    Gun* gun = CharacterAgentOfInstance(operated)->gun;
    if (gun == nullptr || gun->ShotWithin(static_cast<s32>(g_ClockUnitsPerSecond * ShotSeconds)) == 0)
    {
        return 0.0f;
    }

    Vector4 aimStart = PlacePosition(operated);
    Vector4 aim = PlaceAxis(operated, ZAxis);
    InstanceContext* instance = node->owner;
    Vector4 position = PlacePosition(instance);
    Vector4 way = position;
    way.x = way.x - aimStart.x;
    way.y = way.y - aimStart.y;
    way.z = way.z - aimStart.z;
    if (!(0.0f < AxisAlongUnit(aim, &way)))
    {
        return 0.0f;
    }

    f32 reach = GetBoxReach(&instance->collision.box);
    f32 reachSquared = reach * reach;
    Vector4 segment[2];
    aim.x = aim.x * AimLength + aimStart.x;
    aim.z = aim.z * AimLength + aimStart.z;
    aimStart.y = 0.0f;
    aim.y = 0.0f;
    position.y = 0.0f;
    segment[0] = aimStart;
    segment[1] = aim;
    return SegmentPointDistanceSquared(segment, &position) < reachSquared ? 1.0f : 0.0f;
}

namespace
{
// Of an instance in a drawn cell near the played character (the middles of their boxes nearer than 25 units) whose line of
// sight to it is stopped within 1.8 times its box's reach: 1 when a second line of sight, from its box's reach to the right
// (the up axis across the way) or the left, reaches the character
f32 SightFromBeside(GameNode* node, bool right)
{
    constexpr f32 NearSquared = 625.0f;
    constexpr f32 BlockedShare = Rounded(1.8);
    InstanceContext* played = PlayedInstance();
    InstanceContext* instance = node->owner;
    if (!instance->flags.inDrawnCell)
    {
        return 0.0f;
    }

    Vector4 target = BoxMiddle(played);
    Vector4 from = BoxMiddle(instance);
    f32 dx = target.x - from.x;
    f32 dy = target.y - from.y;
    f32 dz = target.z - from.z;
    if (!(dx * dx + dy * dy + dz * dz < NearSquared))
    {
        return 0.0f;
    }

    f32 reach = GetBoxReach(&instance->collision.box);
    Vector4 direction = target;
    Vector4 way = target;
    direction.x = direction.x - from.x;
    direction.y = direction.y - from.y;
    direction.z = direction.z - from.z;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x *= inverse;
    direction.y *= inverse;
    direction.z *= inverse;
    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 side;
    side.y = (up.z * direction.x - up.x * direction.z) * reach;
    side.z = (up.x * direction.y - up.y * direction.x) * reach;
    side.x = (up.y * direction.z - up.z * direction.y) * reach;
    void* results[SightResults];
    InstanceQuery query;
    MakeSightQuery(&query, results, instance);
    way.x = way.x - from.x;
    way.y = way.y - from.y;
    way.z = way.z - from.z;
    if (LineOfSight(instance->chunk, &from, &way, SurfaceFlags::BlocksLineOfSight, &query, g_CoverKinds) == 0)
    {
        return 0.0f;
    }

    f32 blocked = reach * BlockedShare;
    if (!(way.x * way.x + way.y * way.y + way.z * way.z < blocked * blocked))
    {
        return 0.0f;
    }

    if (right)
    {
        from.x = from.x + side.x;
        from.y = from.y + side.y;
        from.z = from.z + side.z;
    }
    else
    {
        from.x = from.x - side.x;
        from.y = from.y - side.y;
        from.z = from.z - side.z;
    }
    way = target;
    way.x = way.x - from.x;
    way.y = way.y - from.y;
    way.z = way.z - from.z;
    query.bits.unused0 = 0;
    query.count = 0;
    query.instance = nullptr;
    query.distance = Infinite;
    return SightReaches(played, instance->chunk, &from, &way, &query, g_CoverKinds) ? 1.0f : 0.0f;
}
}

f32 PlayerVisibleFromLeftCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SightFromBeside(node, false);
}

f32 PlayerVisibleFromRightCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SightFromBeside(node, true);
}

// How far above the played character the instance is (none: none)
f32 HeightAbovePlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    f32 height = PlacePosition(played).y;
    return PlacePosition(node->owner).y - height;
}

// How far the exit point of the parameter's slot points at the played character's eyes
f32 HeadLookingAtPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    return AxisAlongWay(direction, head, PlacePosition(played));
}

// Twice the threshold when the exit point of the parameter's slot points at the played character's eyes (its body's height above
// its feet) more than it and the collision doesn't block the view
f32 HeadCanSeePlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    Vector4 way = PlacePosition(played);
    const CharacterBody* body = CharacterAgentOfInstance(played)->body;
    Vector4 eyes = way;
    eyes.y = way.y + body->height;
    way.y = eyes.y - head.y;
    way.x = way.x - head.x;
    way.z = way.z - head.z;
    if (!(threshold < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &head, &eyes, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// The exit point of the parameter's slot within 40 units of the played character, pointing at its eyes more than the threshold,
// and its line of sight reaching it: twice the threshold
f32 HeadCanSeeNearPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 NearSquared = 1600.0f;
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    Vector4 way = PlacePosition(played);
    f32 dx = head.x - way.x;
    f32 dy = head.y - way.y;
    f32 dz = head.z - way.z;
    if (!(dx * dx + dy * dy + dz * dz < NearSquared))
    {
        return 0.0f;
    }

    f32 eyeHeight = CharacterEyeHeight(CharacterAgentOfInstance(played));
    way.x = way.x - head.x;
    way.y = (way.y + eyeHeight) - head.y;
    way.z = way.z - head.z;
    Vector4 unit = way;
    if (!(threshold < AxisAlongUnit(direction, &unit)))
    {
        return 0.0f;
    }

    void* results[SightResults];
    InstanceQuery query;
    MakeSightQuery(&query, results, node->owner);
    if (!SightReaches(played, node->owner->chunk, &head, &way, &query, SolidOrProjectileNodeKinds))
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// How far the played character's exit point of the parameter's slot points at the instance's eyes
f32 PlayerHeadLookingAtMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(played, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    return AxisAlongWay(direction, head, PlacePosition(node->owner));
}

// Twice the threshold when the played character's exit point of the parameter's slot points at the instance more than it (the
// character's own eye height above the instance's feet) and the collision doesn't block the view
f32 PlayerHeadCanSeeMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(played, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    Vector4 way = PlacePosition(node->owner);
    f32 eyeHeight = CharacterEyeHeight(CharacterAgentOfInstance(played));
    Vector4 eyes = way;
    eyes.y = way.y + eyeHeight;
    way.y = eyes.y - head.y;
    way.x = way.x - head.x;
    way.z = way.z - head.z;
    if (!(threshold < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &head, &eyes, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// Nothing (of the surfaces solid to objects) below the instance from 2 units above its feet down to the threshold below them: a
// bit more than twice the threshold
f32 CanFallCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 Above = 2.0f;
    constexpr f32 Margin = Rounded(0.1);
    Vector4 start = PlacePosition(node->owner);
    start.y = start.y + Above;
    Vector4 drop = {0.0f, -(threshold + Above), 0.0f, 1.0f};
    Vector4 end;
    end.x = start.x + drop.x;
    end.y = start.y + drop.y;
    end.z = start.z + drop.z;
    end.w = 1.0f;
    Vector4 hit;
    if (GetCollisionCheck(node->owner->chunk, &start, &end, SurfaceFlags::SolidToObjects, nullptr, &hit, nullptr) != 0)
    {
        return 0.0f;
    }

    return threshold + threshold + Margin;
}

// The way the threshold ahead of the instance's eyes free of the surfaces solid to objects: twice the threshold
f32 CanMoveForwardsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {0.0f, 0.0f, threshold, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? 0.0f : threshold + threshold;
}

// The way the threshold behind (left of, right of) the instance's eyes blocked: twice the threshold
f32 BlockedBehindCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {0.0f, 0.0f, -threshold, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? threshold + threshold : 0.0f;
}

f32 BlockedLeftCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {-threshold, 0.0f, 0.0f, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? threshold + threshold : 0.0f;
}

f32 BlockedRightCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {threshold, 0.0f, 0.0f, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? threshold + threshold : 0.0f;
}

// The squared distance from the instance to the player in its chunk (else far)
f32 MeToPlayerSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr || node->owner->chunk != PlayerInstance()->chunk)
    {
        return Infinite;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 other = PlacePosition(player);
    f32 dx = position.x - other.x;
    f32 dy = position.y - other.y;
    f32 dz = position.z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

// The squared distance from the player to the node's focus (an asleep focus instance forgotten; none: far)
f32 PlayerToMyFocusSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr)
    {
        return Infinite;
    }

    Vector4 position = PlacePosition(player);
    Vector4 focus;
    if (!FocusPositionOf(Node(node), &focus))
    {
        return Infinite;
    }

    f32 dx = focus.x - position.x;
    f32 dy = focus.y - position.y;
    f32 dz = focus.z - position.z;
    return dx * dx + dy * dy + dz * dz;
}

// The squared distance from the focus position (not a focus instance's) to the player (none: far)
f32 FocusPositionToPlayerDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr)
    {
        return Infinite;
    }

    Vector4 position = PlacePosition(player);
    ObjectNode* object = Node(node);
    if (!object->flags.focusPosition)
    {
        return Infinite;
    }

    f32 dx = object->focusPosition.x - position.x;
    f32 dy = object->focusPosition.y - position.y;
    f32 dz = object->focusPosition.z - position.z;
    return dx * dx + dy * dy + dz * dz;
}

// The node's speed less the played character's (its vehicle's velocity when it rides one, else its own)
f32 SpeedAbovePlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    CharacterAgent* agent = CharacterAgentOfInstance(played);
    Vector4 playerVelocity;
    Vehicle* vehicle = agent->vehicle;
    if (vehicle != nullptr)
    {
        vehicle->VelocityVirtual(&playerVelocity);
    }
    else
    {
        playerVelocity = agent->velocity;
    }

    Vector4 velocity;
    CopyVelocity(Node(node), &velocity);
    return __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) -
           __builtin_sqrtf(playerVelocity.x * playerVelocity.x + playerVelocity.y * playerVelocity.y +
                           playerVelocity.z * playerVelocity.z);
}

// 1 when the played character is nearer to another key than to the current one
f32 PlayerNearerAnotherKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Waypoints* waypoints = Node(node)->waypoints;
    LayoutPosition* current = waypoints->positions.data[waypoints->key];
    Vector4 position = PlacePosition(played);
    f32 cx = current->position.x - position.x;
    f32 cy = current->position.y - position.y;
    f32 cz = current->position.z - position.z;
    for (u32 index = 0; index < waypoints->keyCount; index++)
    {
        LayoutPosition* key = waypoints->positions.data[index];
        if (key == current)
        {
            continue;
        }

        f32 dx = key->position.x - position.x;
        f32 dy = key->position.y - position.y;
        f32 dz = key->position.z - position.z;
        if (dx * dx + dy * dy + dz * dz < cx * cx + cy * cy + cz * cz)
        {
            return 1.0f;
        }
    }

    return 0.0f;
}

namespace
{
// The squared distance from the instance to a step's position (none: far)
f32 DistanceSquaredToStep(GameNode* node, AiPosition* step)
{
    if (step == nullptr)
    {
        return Infinite;
    }

    Vector4 position = PlacePosition(node->owner);
    f32 dx = position.x - step->position.x;
    f32 dy = position.y - step->position.y;
    f32 dz = position.z - step->position.z;
    return dx * dx + dy * dy + dz * dz;
}
}

// The squared distance to the route's step the node is at (its edge's start), and to the next step (the edge's end)
f32 MeToEdgeStartNodeSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return Infinite;
    }

    return DistanceSquaredToStep(node, StepPosition(Node(node)->waypoints));
}

f32 MeToEdgeEndNodeSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return Infinite;
    }

    return DistanceSquaredToStep(node, NextStepPosition(Node(node)->waypoints));
}

// How far along the current path the first agent reference is from the node (where it's nearest the path, within the path, the
// parameters' difference times the path's length; no path or reference: -1)
f32 SplineDistanceToAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    Waypoints* waypoints = object->waypoints;
    InstanceContext* reference = object->agentRef1;
    if (reference != nullptr && Asleep(reference))
    {
        object->agentRef1 = nullptr;
    }

    if (waypoints == nullptr || object->agentRef1 == nullptr)
    {
        return -1.0f;
    }

    LayoutPath* path = waypoints->paths.data[waypoints->pathIndex];
    f32 along = waypoints->pathParameter;
    if (path == nullptr)
    {
        return -1.0f;
    }

    Vector4 position = PlacePosition(object->agentRef1);
    Vector4 nearest;
    f32 parameter = ClampFloat(NearestPointOnPath(path, &position, &nearest), 0.0f, 1.0f);
    // The path's length: the arc length to the end of its last segment (a path of n points has n - 3)
    s32 segments = path->count - 3;
    return __builtin_fabsf((parameter - along) * path->lengths[segments - 1]);
}

// How far the instance is outside the radius of the AI position of its chunk nearest it, squared (none: far)
f32 NearestPointEdgeDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* instance = node->owner;
    ChunkEntry* chunk = ChunkOfInstance(g_ConditionsChunkManager, instance);
    Vector4 position = PlacePosition(instance);
    u16 index;
    AiPosition* nearest = NearestAiPosition(chunk, &position, &index);
    if (nearest == nullptr)
    {
        return Infinite;
    }

    f32 dx = nearest->position.x - position.x;
    f32 dy = nearest->position.y - position.y;
    f32 dz = nearest->position.z - position.z;
    f32 outside = __builtin_sqrtf(dx * dx + dy * dy + dz * dz) - nearest->position.w;
    return outside * outside;
}

namespace
{
// How far above their feet the checks of the focus, the route and the camera look from and at
constexpr f32 FocusEyeHeight = Rounded(0.8);

// The node's focus as the conditions take it: whether it has one (an asleep focus instance forgotten), its position (a focus
// instance's place's) and the focus instance (none for a focus position)
bool FocusOf(ObjectNode* object, Vector4* position, InstanceContext** instance)
{
    *instance = nullptr;
    if (!FocusPositionOf(object, position))
    {
        return false;
    }

    if (object->flags.focusInstance)
    {
        *instance = object->focusInstance;
    }

    return true;
}

// Twice the threshold when the line of sight from a point to the middle of the instance's box reaches (nothing stopped it, or
// what's given did: retail compares the instance hit with it even when it's none)
f32 SeenFrom(ScriptCondition* condition, GameNode* node, const Vector4& from, InstanceContext* seer)
{
    InstanceContext* instance = node->owner;
    Vector4 start = from;
    Vector4 middle = BoxMiddle(instance);
    Vector4 way = middle;
    way.x = middle.x - start.x;
    way.y = middle.y - start.y;
    way.z = middle.z - start.z;
    void* results[SightResults];
    InstanceQuery query;
    MakeSightQuery(&query, results, instance);
    if (!SightReaches(seer, instance->chunk, &start, &way, &query, g_SolidKinds))
    {
        return 0.0f;
    }

    return condition->threshold + condition->threshold;
}

// Twice the threshold when an axis points from a point at another (both at the eyes) more than it and the collision doesn't
// block the view
f32 SeesAlongAxis(ScriptCondition* condition, ChunkData* chunk, const Vector4& axis, Vector4 from, Vector4 to)
{
    from.y = from.y + FocusEyeHeight;
    to.y = to.y + FocusEyeHeight;
    Vector4 way = to;
    way.x = to.x - from.x;
    way.z = to.z - from.z;
    way.y = to.y - from.y;
    if (!(condition->threshold < AxisAlongUnit(axis, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(chunk, &from, &to, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return condition->threshold + condition->threshold;
}

// 1 when nothing (of the sight's surfaces) is between two points
f32 ClearBetween(ChunkData* chunk, Vector4 from, Vector4 to)
{
    Vector4 hit;
    return GetCollisionCheck(chunk, &from, &to, SurfaceFlags::BlocksLineOfSight, nullptr, &hit, nullptr) != 0 ? 0.0f : 1.0f;
}

}

// How far the instance's forward axis points at the focus's eyes
f32 MeFacingFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    return AxisAlongWay(forward, position, focus, FocusEyeHeight);
}

// How far the instance's forward axis points at the first agent reference's eyes
f32 MeFacingAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = PlacePosition(reference);
    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    return AxisAlongWay(forward, position, target, FocusEyeHeight);
}

// How far the instance's forward axis points at the focus along the ground (the way made a unit long with its height first)
f32 FocusForwardDotCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 way;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &way, &instance))
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    way.x = way.x - position.x;
    way.y = way.y - position.y;
    way.z = way.z - position.z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x *= inverse;
    way.z *= inverse;
    return forward.x * way.x + forward.z * way.z;
}

// How far the focus instance's forward axis points at the instance's eyes (a focus position: none)
f32 FocusFacingMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance) || instance == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(instance);
    Vector4 forward = PlaceAxis(instance, ZAxis);
    return AxisAlongWay(forward, position, PlacePosition(node->owner), FocusEyeHeight);
}

// 1 when nothing is between the instance's eyes and the focus's
f32 ClearLineOfSightToFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    focus.y = focus.y + FocusEyeHeight;
    position.y = position.y + FocusEyeHeight;
    return ClearBetween(node->owner->chunk, position, focus);
}

// Twice the threshold when the line of sight from the focus to the middle of the instance's box reaches it
f32 VisibleFromFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    return SeenFrom(this, node, focus, instance);
}

// The same from the first agent reference
f32 VisibleFromAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    return SeenFrom(this, node, PlacePosition(reference), reference);
}

// Twice the threshold when the instance faces the focus more than it and nothing blocks the view (their eyes)
f32 CanSeeFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    return SeesAlongAxis(this, node->owner->chunk, forward, position, focus);
}

// The same with the focus instance looking at the instance (a focus position: none)
f32 FocusAgentCanSeeMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance) || instance == nullptr)
    {
        return 0.0f;
    }

    Vector4 from = PlacePosition(instance);
    Vector4 forward = PlaceAxis(instance, ZAxis);
    return SeesAlongAxis(this, node->owner->chunk, forward, from, PlacePosition(node->owner));
}

// How far above the focus the instance is
f32 HeightAboveFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    return PlacePosition(node->owner).y - focus.y;
}

// How far the instance's forward axis points at the eyes of the route's step it's at
f32 MeFacingRouteNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* step = StepPosition(Node(node)->waypoints);
    if (step == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = step->position;
    target.w = 1.0f;
    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    return AxisAlongWay(forward, position, target, FocusEyeHeight);
}

// 1 when nothing is between the instance's eyes and the step's
f32 ClearLineOfSightToRouteNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* step = StepPosition(Node(node)->waypoints);
    if (step == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = step->position;
    target.w = 1.0f;
    Vector4 position = PlacePosition(node->owner);
    target.y = target.y + FocusEyeHeight;
    position.y = position.y + FocusEyeHeight;
    return ClearBetween(node->owner->chunk, position, target);
}

// How far the instance's forward axis points at the camera
f32 MeFacingCameraCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr)
    {
        return 0.0f;
    }

    Vector4 way = PlacePosition(camera);
    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, ZAxis);
    way.x = way.x - position.x;
    way.y = way.y - position.y;
    way.z = way.z - position.z;
    return AxisAlongUnit(forward, &way);
}

// How far the camera's forward axis points at the instance's eyes
f32 CameraFacingMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(camera);
    Vector4 forward = PlaceAxis(camera, ZAxis);
    return AxisAlongWay(forward, position, PlacePosition(node->owner), FocusEyeHeight);
}

namespace
{
// Whether the middle of the instance's box (retail's: half of its size above its lowest corner) is in the camera's view
bool MiddleInView(InstanceContext* instance, Vector4* middle)
{
    ChunkView test;
    ChunkView::Construct(&test);
    const Box& box = instance->collision.box;
    middle->w = box.max.w;
    middle->x = (box.max.x - box.min.x) * 0.5f + box.min.x;
    middle->y = (box.max.y - box.min.y) * 0.5f + box.min.y;
    middle->z = (box.max.z - box.min.z) * 0.5f + box.min.z;
    test.TestPoint(middle);
    bool inView = test.visibility == ChunkView::InView;
    test.Destroy(DestroyOnly);
    return inView;
}
}

// 1 while there's a camera and the middle of the instance's box is in its view
f32 InCameraFrustrumCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (CameraInstance() == nullptr)
    {
        return 0.0f;
    }

    Vector4 middle;
    return MiddleInView(node->owner, &middle) ? 1.0f : 0.0f;
}

// 1 when nothing is between the instance's eyes and the camera
f32 ClearLineOfSightToCameraCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = PlacePosition(camera);
    Vector4 position = PlacePosition(node->owner);
    position.y = position.y + FocusEyeHeight;
    return ClearBetween(node->owner->chunk, position, target);
}

// 1 when the middle of the instance's box is in the camera's view and nothing is between the camera and it
f32 CameraCanSeeMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr)
    {
        return 0.0f;
    }

    Vector4 middle;
    if (!MiddleInView(node->owner, &middle))
    {
        return 0.0f;
    }

    return ClearBetween(node->owner->chunk, PlacePosition(camera), middle);
}

namespace
{
// How far a direction points along the way from a point to another (the way made a unit long first)
f32 DirectionAlongWay(const Vector4& direction, const Vector4& from, Vector4 to)
{
    to.x = to.x - from.x;
    to.y = to.y - from.y;
    to.z = to.z - from.z;
    return AxisAlongUnit(direction, &to);
}

// Twice the threshold when a head (a point and the direction it looks) looks at a point more than it and nothing is between
// them
f32 HeadSees(ScriptCondition* condition, ChunkData* chunk, Vector4 head, const Vector4& direction, Vector4 target)
{
    Vector4 way = target;
    way.x = target.x - head.x;
    way.y = target.y - head.y;
    way.z = target.z - head.z;
    if (!(condition->threshold < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(chunk, &head, &target, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return condition->threshold + condition->threshold;
}
}

// How far the exit point of the parameter's slot points at the focus's eyes
f32 HeadLookingAtFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    focus.y = focus.y + FocusEyeHeight;
    Vector4 head;
    Vector4 direction;
    if (ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    return DirectionAlongWay(direction, head, focus);
}

// Twice the threshold when the exit point of the parameter's slot looks at the focus's eyes more than it and nothing blocks the
// view
f32 HeadCanSeeFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 head;
    Vector4 direction;
    if (ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    focus.y = focus.y + FocusEyeHeight;
    return HeadSees(this, node->owner->chunk, head, direction, focus);
}

// The same with the first agent reference
f32 HeadCanSeeAgentRef1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = PlacePosition(reference);
    Vector4 head;
    Vector4 direction;
    if (ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    target.y = target.y + FocusEyeHeight;
    return HeadSees(this, node->owner->chunk, head, direction, target);
}

// How far the exit point of the parameter's slot points at the eyes of the route's step the node is at
f32 HeadLookingAtRouteNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* step = StepPosition(Node(node)->waypoints);
    if (step == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = step->position;
    target.y = target.y + FocusEyeHeight;
    target.w = 1.0f;
    Vector4 head;
    Vector4 direction;
    if (ExitPointPlace(node->owner, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    return DirectionAlongWay(direction, head, target);
}

// The squared distance from the joint of the parameter's index (in the world) to the focus
f32 ExitPointToFocusSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 joint;
    if (JointPosition(node->owner, ExitSlot(this), &joint, nullptr) == 0)
    {
        return 0.0f;
    }

    f32 dx = focus.x - joint.x;
    f32 dy = focus.y - joint.y;
    f32 dz = focus.z - joint.z;
    return dx * dx + dy * dy + dz * dz;
}

// How far the focus instance's exit point of the parameter's slot points at the instance's eyes
f32 FocusHeadLookingAtMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    Vector4 head;
    Vector4 direction;
    if (!FocusOf(Node(node), &focus, &instance) || instance == nullptr ||
        ExitPointPlace(instance, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    return AxisAlongWay(direction, head, PlacePosition(node->owner), FocusEyeHeight);
}

// Twice the threshold when it looks at the instance's eyes more than it and nothing blocks the view
f32 FocusHeadCanSeeMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    Vector4 head;
    Vector4 direction;
    if (!FocusOf(Node(node), &focus, &instance) || instance == nullptr ||
        ExitPointPlace(instance, ExitSlot(this), &head, &direction) == 0)
    {
        return 0.0f;
    }

    Vector4 target = PlacePosition(node->owner);
    target.y = target.y + FocusEyeHeight;
    return HeadSees(this, node->owner->chunk, head, direction, target);
}

// How far the instance's x axis points at the target (the focus or the first agent reference): its size while the target is on
// the condition's side
f32 TargetToSideCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* owner = node->owner;
    Vector4 target;
    if (targetKind == TargetFocus)
    {
        if (!FocusPositionOf(Node(node), &target))
        {
            return 0.0f;
        }
    }
    else if (targetKind == TargetAgentRef1)
    {
        InstanceContext* reference = AwakeAgentRef1(Node(node));
        if (reference == nullptr)
        {
            return 0.0f;
        }

        target = PlacePosition(reference);
    }
    else
    {
        // Retail reads stack leftovers here; the builders only make 0 and 1
        target = {};
    }

    Vector4 position = PlacePosition(owner);
    Vector4 way;
    way.x = target.x - position.x;
    way.y = target.y - position.y;
    way.z = target.z - position.z;
    way.w = 1.0f;
    Vector4 side = PlaceAxis(owner, XAxis);
    f32 along = AxisAlongUnit(side, &way);
    if (targetSide == SideRight && along < 0.0f)
    {
        return __builtin_fabsf(0.0f);
    }

    if (targetSide == SideLeft && 0.0f < along)
    {
        return __builtin_fabsf(0.0f);
    }

    return __builtin_fabsf(along);
}

// The physics body slower than about 0.22 units a second and turning slower than about 0.22 (its squares below 0.05); without
// one, no motion block and the motion's velocity's square below 0.001
f32 IsRestingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 BodyRest = Rounded(0.05);
    constexpr f32 MotionRest = Rounded(0.001);
    ObjectNode* object = Node(node);
    if (object->rigidBody != nullptr)
    {
        DynamicBody* body = object->rigidBody->physicsBody;
        if (body != nullptr)
        {
            const Vector4& velocity = body->velocity;
            const Vector4& turning = body->angularVelocity;
            return velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < BodyRest &&
                   turning.x * turning.x + turning.y * turning.y + turning.z * turning.z < BodyRest ? 1.0f : 0.0f;
        }
    }

    if (object->motionBlock != nullptr)
    {
        return 1.0f;
    }

    const Vector4& velocity = object->motion->velocity;
    return velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < MotionRest ? 1.0f : 0.0f;
}

namespace
{
// The query of a ground probe (64 instances at most, none left out)
void MakeProbeQuery(InstanceQuery* query, void** results)
{
    query->results = results;
    query->count = 0;
    query->most = ProbeResults;
    query->distance = Infinite;
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = 0;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->instance = nullptr;
    query->skipped[1] = nullptr;
}

constexpr f32 ProbeAbove = 8.0f;
constexpr f32 ProbeDown = -16.0f;
constexpr f32 NoGround = 10000.0f;
}

// Probing 16 units down from 8 above the focus position (not a focus instance's), where it stopped (8 units below the start
// plus the way down it went) against the instance's height (none: 10000)
f32 GroundBelowFocusPositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    if (!object->flags.focusPosition)
    {
        return NoGround;
    }

    Vector4 start = object->focusPosition;
    Vector4 position = PlacePosition(node->owner);
    start.y = start.y + ProbeAbove;
    Vector4 way = {0.0f, ProbeDown, 0.0f, 1.0f};
    void* results[ProbeResults];
    InstanceQuery query;
    MakeProbeQuery(&query, results);
    if (LineOfSight(node->owner->chunk, &start, &way, SurfaceFlags::SolidToObjects, &query, g_CoverKinds) == 0)
    {
        return NoGround;
    }

    return __builtin_fabsf((way.y + ProbeAbove) - position.y);
}

// Probing 16 units down from 8 above the point 5 units ahead (behind with the parameter 1): how far below the start it stopped
// less 8 (none: 10000)
f32 GroundBelowPointAheadCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 Ahead = 5.0f;
    constexpr u32 Behind = 1;
    InstanceContext* owner = node->owner;
    Vector4 position = PlacePosition(owner);
    Vector4 forward = PlaceAxis(owner, ZAxis);
    f32 distance = Parameter() == Behind ? -Ahead : Ahead;
    Vector4 start = position;
    start.x = position.x + forward.x * distance;
    start.z = position.z + forward.z * distance;
    start.y = position.y + forward.y * distance + ProbeAbove;
    Vector4 way = {0.0f, ProbeDown, 0.0f, 1.0f};
    void* results[ProbeResults];
    InstanceQuery query;
    MakeProbeQuery(&query, results);
    if (LineOfSight(owner->chunk, &start, &way, SurfaceFlags::SolidToObjects, &query, g_CoverKinds) == 0)
    {
        return NoGround;
    }

    return __builtin_fabsf(way.y + ProbeAbove);
}

// The squared distance to the first agent reference (none: far)
f32 MeToAgentRef1SqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return Infinite;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 other = PlacePosition(reference);
    f32 dx = position.x - other.x;
    f32 dy = position.y - other.y;
    f32 dz = position.z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

// The focus instance's first int property (from its object node's packet properties) the parameter, its agent having an object
f32 FocusObjectProp0EqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    if (focus == nullptr)
    {
        return 0.0f;
    }

    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&focus->nodes, NodeObject));
    if (objectNode == nullptr || objectNode->agent->objectId == NoObjectId)
    {
        return 0.0f;
    }

    return static_cast<u32>(objectNode->PacketProperties()->GetInt(0)) == Parameter() ? 1.0f : 0.0f;
}

namespace
{
// The key of the parameter: without one (0 or 0xFF) the current one, or the next one when asked (going round to the first after
// the last); else the parameter's (counted from 1) while there's such a key
LayoutPosition* KeyOfParameter(const Waypoints* waypoints, u32 parameter, bool next)
{
    if (parameter == 0 || parameter == Waypoints::NoKey)
    {
        u32 key = waypoints->key;
        if (next)
        {
            key = static_cast<u8>(key + 1);
            if (waypoints->lastKey < key)
            {
                key = waypoints->firstKey;
            }
        }

        return waypoints->positions.data[key];
    }

    s32 index = static_cast<s32>(parameter) - 1;
    if (waypoints->keyCount == 0 || index >= static_cast<s32>(waypoints->keyCount))
    {
        return nullptr;
    }

    return waypoints->positions.data[index];
}
}

// The squared distance to the key of the parameter, of the node's source node's keys when it has one (none: none)
f32 MeToCurrentKeySqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectNode* object = Node(node);
    Waypoints* waypoints = object->sourceNode != nullptr ? static_cast<ObjectNode*>(object->sourceNode)->waypoints : object->waypoints;
    if (waypoints == nullptr)
    {
        return 0.0f;
    }

    LayoutPosition* key = KeyOfParameter(waypoints, Parameter(), false);
    if (key == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    f32 dx = position.x - key->position.x;
    f32 dy = position.y - key->position.y;
    f32 dz = position.z - key->position.z;
    return dx * dx + dy * dy + dz * dz;
}

// How fast the node goes towards the key of the parameter (the next one by default): its velocity (its physics body's, else its
// motion's) along the way there
f32 SpeedTowardsNextKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectNode* object = Node(node);
    Waypoints* waypoints = object->waypoints;
    if (waypoints == nullptr)
    {
        return 0.0f;
    }

    LayoutPosition* key = KeyOfParameter(waypoints, Parameter(), true);
    if (key == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    Vector4 way;
    way.w = 1.0f;
    way.x = key->position.x - position.x;
    way.y = key->position.y - position.y;
    way.z = key->position.z - position.z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x *= inverse;
    way.y *= inverse;
    way.z *= inverse;
    Vector4 velocity = {0.0f, 0.0f, 0.0f, 1.0f};
    DynamicBody* body = object->rigidBody != nullptr ? object->rigidBody->physicsBody : nullptr;
    if (body != nullptr)
    {
        velocity = body->velocity;
    }
    else if (object->motion != nullptr)
    {
        velocity = object->motion->velocity;
    }

    return velocity.x * way.x + velocity.y * way.y + velocity.z * way.z;
}

// 1 when the instance's own box, raised by its height (and 0.01) at its place, overlaps solid instances
f32 BoxAboveIsOverlappedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 Margin = Rounded(0.01);
    InstanceContext* owner = node->owner;
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    Matrix4x4 matrix = place->matrix;
    CollisionHull hull;
    HullConstruct(&hull);
    Vector4 min = owner->collision.ownBox.min;
    Vector4 max = owner->collision.ownBox.max;
    BuildBoxHull(&hull, &min, &max);
    Vector4 raise = {0.0f, (max.y + Margin) - min.y, 0.0f, 1.0f};
    matrix.m[3][0] = matrix.m[3][0] + raise.x;
    matrix.m[3][2] = matrix.m[3][2] + raise.z;
    matrix.m[3][1] = matrix.m[3][1] + raise.y;
    void* results[OverlapResults];
    InstanceQuery query;
    query.results = results;
    query.most = OverlapResults;
    query.count = 0;
    query.distance = Infinite;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.bits.value = InstanceQueryBits::AllWanted;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, owner);
    f32 result = ChunkInstancesInHull(owner->chunk, &hull, &matrix, g_SolidKinds, &query, 0) != 0 ? 1.0f : 0.0f;
    HullDestroy(&hull, DestroyOnly);
    return result;
}

namespace
{
// The squared distance between the focus and a point (no focus: none)
f32 FocusDistanceSquaredFrom(ObjectNode* object, const Vector4& point, bool pointFirst)
{
    Vector4 focus;
    if (!FocusPositionOf(object, &focus))
    {
        return 0.0f;
    }

    f32 dx = pointFirst ? point.x - focus.x : focus.x - point.x;
    f32 dy = pointFirst ? point.y - focus.y : focus.y - point.y;
    f32 dz = pointFirst ? point.z - focus.z : focus.z - point.z;
    return dx * dx + dy * dy + dz * dz;
}

// The squared distance from the instance to the point on the line through it along one of its axes nearest a point (retail's:
// how far along the axis the point is, not how far off it)
f32 DistanceAlongAxisSquared(const Vector4& position, const Vector4& axis, const Vector4& target)
{
    f32 along = (target.x - position.x) * axis.x + (target.y - position.y) * axis.y + (target.z - position.z) * axis.z;
    Vector4 nearest = position;
    nearest.x = nearest.x + axis.x * along;
    nearest.y = nearest.y + axis.y * along;
    nearest.z = nearest.z + axis.z * along;
    f32 dx = position.x - nearest.x;
    f32 dy = position.y - nearest.y;
    f32 dz = position.z - nearest.z;
    return dx * dx + dy * dy + dz * dz;
}

f32 FocusAlongAxis(GameNode* node, u32 axis)
{
    Vector4 direction = PlaceAxis(node->owner, axis);
    Vector4 position = PlacePosition(node->owner);
    Vector4 focus;
    if (!FocusPositionOf(Node(node), &focus))
    {
        return 0.0f;
    }

    return DistanceAlongAxisSquared(position, direction, focus);
}

f32 AgentRef1AlongAxis(GameNode* node, u32 axis)
{
    Vector4 direction = PlaceAxis(node->owner, axis);
    Vector4 position = PlacePosition(node->owner);
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    return DistanceAlongAxisSquared(position, direction, PlacePosition(reference));
}
}

// The squared distance between the focus and the first agent reference (either missing: none)
f32 FocusToAgentRef1DistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    bool hasFocus = FocusPositionOf(Node(node), &focus);
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(reference);
    if (!hasFocus)
    {
        return 0.0f;
    }

    f32 dx = position.x - focus.x;
    f32 dy = position.y - focus.y;
    f32 dz = position.z - focus.z;
    return dx * dx + dy * dy + dz * dz;
}

// The squared distance between the focus and where the instance started
f32 FocusDistanceFromStartSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusDistanceSquaredFrom(Node(node), Node(node)->informationPointer->position, true);
}

f32 FocusAlongXAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 0);
}

f32 FocusAlongYAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 1);
}

f32 FocusAlongZAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 2);
}

f32 AgentRef1AlongXAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return AgentRef1AlongAxis(node, 0);
}

f32 AgentRef1AlongYAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return AgentRef1AlongAxis(node, 1);
}

f32 AgentRef1AlongZAxisSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return AgentRef1AlongAxis(node, 2);
}

// The squared distance from the instance to the focus
f32 MeToFocusSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 focus;
    InstanceContext* instance;
    if (!FocusOf(Node(node), &focus, &instance))
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(node->owner);
    f32 dx = position.x - focus.x;
    f32 dy = position.y - focus.y;
    f32 dz = position.z - focus.z;
    return dx * dx + dy * dy + dz * dz;
}

// Twice the threshold when the played character faces the instance more than it and nothing blocks the view (their eyes)
f32 PlayerCanSeeMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    Vector4 from = PlacePosition(played);
    Vector4 forward = PlaceAxis(played, ZAxis);
    Vector4 to = PlacePosition(node->owner);
    from.y = from.y + EyeHeight;
    to.y = to.y + EyeHeight;
    Vector4 way = to;
    way.x = to.x - from.x;
    way.z = to.z - from.z;
    way.y = to.y - from.y;
    if (!(threshold < AxisAlongUnit(forward, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &from, &to, SurfaceFlags::BlocksLineOfSight, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return threshold + threshold;
}

// How far ahead of the instance the camera is along its forward axis (none, or the instance in a drawn cell: none)
f32 CameraForwardDistanceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr || node->owner->flags.inDrawnCell)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(camera);
    Vector4 forward = PlaceAxis(camera, ZAxis);
    Vector4 own = PlacePosition(node->owner);
    return (position.x - own.x) * forward.x + (position.y - own.y) * forward.y + (position.z - own.z) * forward.z;
}

void ConstructConditionChecksModule()
{
    InitConditionChecksModule(1, DefaultInitPriority);
}
