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
#include "game/view.h"

#include <cstdint>

// The conditions' checks converted so far: a score (1 or 0 for the yes or no ones) for the agent's node, the level and the
// clock's time

extern "C"
{
    // The counter of an index (the chunk manager's counters)
    extern void* G_ChunkManager_0030A0C8;
    // A value of an instance's vehicle, and whether an instance weighs on the character
    f32 VehicleValue(InstanceContext* instance) RETAIL(FUN_00128c58);
    u32 HasActorWeight(PlayerCharacter* character, InstanceContext* instance) RETAIL(FUN_0013f748);
    extern s32 g_InstancesWithValue174 RETAIL(D_0030A0FC);
    extern s32 g_VarPercept629 RETAIL(D_003098E8);
    extern u8 g_GlobalByte30A0E9 RETAIL(D_0030A0E9);
    // The object of a node's instance (the node it takes its object from)
    // The chunk manager's chunk of an index, and a persistent flag of a store
    // The AI position of a chunk's navigation nearest a point (with its index when wanted)
}

static_assert(offsetof(InstanceContext, id) == 0x154);
static_assert(offsetof(InstanceContext, parent) == 0xD0);

namespace
{
constexpr u32 ModelNodeKind = 3;
constexpr u32 AttachmentsKind = 6;
constexpr u32 CharacterNodeKind = 0xC;
constexpr u32 CrateNodeKind = 0xD;
constexpr u32 CreatureNodeKind = 0xF;
constexpr u32 PayGateNodeKind = 0x12;
constexpr u32 TakesPacketsSlot = 15;
constexpr u8 NoKey = 0xFF;
constexpr u8 NoByte = 0xFF;
constexpr u32 AllJoints = 0xFF;
// The instance flags the conditions test: its sphere contact (collidable), triggers' signals... (bit 8, busy), attached to an
// agent (6), an attached object (7), visible (10)
constexpr u32 AttachedToAgentFlag = 0x40;
constexpr u32 AttachedObjectFlag = 0x80;
constexpr u32 BusyFlag = 0x100;
// The loader's bits: every linked chunk loaded, every linked chunk queued or loaded
constexpr u32 LinksLoadedBit = 0x400;
constexpr u32 LinksQueuedBit = 0x800;
constexpr u32 SlammingState = 9;

f32 YesNo(bool yes)
{
    return yes ? 1.0f : 0.0f;
}

ObjectNode* Node(GameNode* node)
{
    return static_cast<ObjectNode*>(node);
}

u32 InstanceFlags(GameNode* node)
{
    return node->owner->flags;
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
}

// The player character's part (the second copy the conditions read)
CharacterPart* PlayerPart()
{
    return static_cast<CharacterPart*>(g_PlayerCharacterData2);
}

// The part's 64 bits from 0x18
u64 PartBits(const CharacterPart* part)
{
    return part->Bits();
}

// The agent's contact message's word (what the last contact that told it something was)
u32 ContactWord(GameNode* node)
{
    return Node(node)->agent->contact.word;
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

GameProgress* Progress()
{
    return &G_GameController_0030988C->progress;
}

bool Asleep(const InstanceContext* instance)
{
    return (instance->flags & ReferencedObject::FlagAsleep) != 0;
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
    if (node->agentRef2 != nullptr && Asleep(node->agentRef2) && (node->flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
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
    CharacterControl* control = character->control;
    return control != nullptr && control->Kind() == kind;
}

// The attachments node's word: the linked objects' count (bits 0-4), bit 5 set when it lost them all, the current one (bits
// 7-11)
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LostAllBit = 0x20;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;

u32* AttachmentsWord(void* attachments)
{
    return reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18);
}

// The AI position of the route's step the node is at, and of the step before (the first step's own; no route: none)
AiPosition* StepPosition(const Waypoints* waypoints)
{
    Route* route = waypoints->route;
    return route != nullptr ? route->PositionAt(waypoints->routeIndex) : nullptr;
}

AiPosition* PreviousStepPosition(const Waypoints* waypoints)
{
    Route* route = waypoints->route;
    if (route == nullptr)
    {
        return nullptr;
    }

    u8 step = waypoints->routeIndex - 1;
    return route->PositionAt(step != NoKey ? step : 0);
}

f32 PositionFlag(const AiPosition* position, u32 flag)
{
    return YesNo(position != nullptr && (position->flags & flag) != 0);
}

// A flag of the path that led to the route's step, while the keys haven't gone round
f32 RoutePathFlag(GameNode* node, u32 flag)
{
    Waypoints* waypoints = Node(node)->waypoints;
    if (waypoints->route == nullptr || (waypoints->flags & Waypoints::FlagWrapped) != 0 || waypoints->routePath == nullptr)
    {
        return 0.0f;
    }

    return YesNo((waypoints->routePath->flags & flag) != 0);
}

// The instance's position (made up to date first)
const Vector4& PositionOf(GameNode* node)
{
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    return place->position;
}

// The trajectory controller's cycle about an axis in turns
f32 CycleTurns(GameNode* node, u32 axis)
{
    constexpr f32 DegreesPerUnit = 360.0f / 65536.0f;
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

    return static_cast<f32>(trajectory->cycles[axis]) * DegreesPerUnit * TurnsPerDegree;
}

f32 RigidBodyBit88(GameNode* node, u32 bit)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectRigidBody* body = Node(node)->rigidBody;
    return YesNo(body != nullptr && (body->bits88 >> bit & 1) != 0);
}

f32 RigidBodyBit90(GameNode* node, u32 bit)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    ObjectRigidBody* body = Node(node)->rigidBody;
    return YesNo(body != nullptr && (body->bits90 >> bit & 1) != 0);
}

// The head tracking's bits having all of a mask's
f32 HeadTrackingBits(GameNode* node, u64 mask)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    HeadTracking* tracking = Node(node)->headTracking;
    return YesNo(tracking != nullptr && (tracking->bits & mask) == mask);
}

// What the perception has in a slot (none: 0)
f32 PerceptionOf(GameNode* node, u32 slot)
{
    f32 value = 0.0f;
    if (TakesPackets(node))
    {
        void* perception = Node(node)->perception;
        if (perception != nullptr)
        {
            PerceptionValue(perception, slot, &value);
        }
    }

    return value;
}

// The part of the instance's agent node
AgentPart* AgentPartOf(GameNode* node)
{
    return AgentNodeOf(node->owner)->agent->part;
}

// A creature part's hit points (bits 6-13 of its flags)
constexpr u32 HitPointsShift = 6;
constexpr u32 HitPointsMask = 0xFF;

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

// The instance of the character played
InstanceContext* PlayedInstance()
{
    return Progress()->Instance(Progress()->Field(GameProgress::CharacterShift));
}

// Bit 14 of a character agent's bits at 0x70
bool CharacterBit14(AgentNode* character)
{
    return (static_cast<CharacterAgent*>(character->agent)->StateBits() >> 14 & 1) != 0;
}

// The object ID of the agent of an instance's object node (whether there's an instance isn't checked; no node: 0xFFFF)
u16 ObjectIdOf(InstanceContext* instance)
{
    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, 1));
    return objectNode != nullptr ? objectNode->agent->objectId : 0xFFFF;
}

// An object ID (but none) the parameter, the ID's top bit left out
f32 ObjectIdIs(u16 id, u32 parameter)
{
    constexpr u16 NoObject = 0xFFFF;
    return YesNo(id != NoObject && (id & 0x7FFFu) == parameter);
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
    return YesNo(focus != nullptr && (focus->flags & flag) != 0);
}

// The pad of a player (1 the first, 2 the second, else none)
GamePad* PadOf(u32 player)
{
    GameController* controller = G_GameController_0030988C;
    if (player == 1)
    {
        return controller->pad;
    }

    if (player == 2)
    {
        return reinterpret_cast<GamePad*>(controller->unknown40);
    }

    return nullptr;
}
}

f32 NextCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 1.0f;
}

f32 IsCollidableCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((InstanceFlags(node) & ReferencedObject::FlagSphereContact) != 0);
}

// The body being checked is what runs when nothing else does
f32 ElseCondition::Check(GameNode*, BehaviourLevel* level, const u32*)
{
    level->pendingBody = g_CheckedBody;
    return 0.0f;
}

f32 RandomCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return GetRandFloat();
}

f32 IsVisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((InstanceFlags(node) & ReferencedObject::FlagVisible) != 0);
}

// Seconds since the level's last body
f32 TimeInUnitCondition::Check(GameNode*, BehaviourLevel* level, const u32* time)
{
    return static_cast<f32>(static_cast<s32>(*time - level->time)) * g_SecondsPerClockUnit;
}

// Every chunk the focus chunk links loaded (TT Lab's name is the tools')
f32 IsInExternalScriptCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((G_ChunkLoadingManager_->focusLoader->bits & LinksLoadedBit) != 0);
}

// From 1 (none 0)
f32 CurrentKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u8 key = Node(node)->waypoints->key;
    if (key == NoKey)
    {
        return 0.0f;
    }

    return static_cast<f32>(key + 1);
}

f32 AttachedToAnAgentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((InstanceFlags(node) & AttachedToAgentFlag) != 0);
}

f32 GotAttachedObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((InstanceFlags(node) & AttachedObjectFlag) != 0);
}

f32 CurrentKeyEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u32 key = (Node(node)->waypoints->key + 1) & 0xFF;
    return YesNo(key == Parameter());
}

f32 GotFocusObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Node(node)->flags & ObjectNodeBase::FlagFocusInstance));
}

f32 GotFocusPositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Node(node)->flags >> 1 & 1));
}

// Of the model's animator (whether the instance has a model isn't checked)
f32 GotAnimationTimeRemainingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, ModelNodeKind));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 0.0f;
    }

    return GetAnimationProgress(animator, AllJoints);
}

f32 CounterValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(GameCounter(G_ChunkManager_0030A0C8, Parameter()));
}

f32 SqrMoveSpeedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const Vector4& velocity = Node(node)->motion->velocity;
    return velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
}

f32 IsBusyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((InstanceFlags(node) & BusyFlag) != 0);
}

// A bit of the instance's state (bits 0-31; TT Lab's CheckInstanceFlagSet)
f32 SoftFlagSetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((Node(node)->properties->state & 1u << (Parameter() & 31)) != 0);
}

f32 GotLinkedObjectCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(GetGameNode(&node->owner->nodes, AttachmentsKind) != nullptr);
}

f32 HasInstancePositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->storedPlace != nullptr);
}

f32 HasFocusPositionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((Node(node)->flags & ObjectNodeBase::FlagStoredPosition) != 0);
}

// A byte of the agent's from 0x18
f32 ObjectInstanceByteAtCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const u8* bytes = Node(node)->agent->unknown18;
    return static_cast<f32>(bytes[Parameter()]);
}

// The rigid body's word at 0x8C (bits 0-3) and its contacts (bits 36-39 of its 64 bits at 0x88)
f32 PhysicsCount8cCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* body = reinterpret_cast<u8*>(Node(node)->rigidBody);
    bool yes = body != nullptr && (*reinterpret_cast<s32*>(body + 0x8C) & 0xF) != 0;
    return YesNo(yes);
}

f32 PhysicsHasContactsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* body = reinterpret_cast<u8*>(Node(node)->rigidBody);
    bool yes = body != nullptr && (*reinterpret_cast<u64*>(body + 0x88) >> 36 & 0xF) != 0;
    return YesNo(yes);
}

// Whether there's a rigid body isn't checked
f32 PhysicsHasGroundCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* body = reinterpret_cast<u8*>(Node(node)->rigidBody);
    return YesNo(*reinterpret_cast<void**>(body + 0x80) != nullptr);
}

f32 CharacterAnalogCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return *reinterpret_cast<const f32*>(&Node(node)->agent->part->unknown08);
}

f32 AlwaysZeroCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The value the level above gave this one: taken once
f32 GotUserMessageOnceEqualsCondition::Check(GameNode*, BehaviourLevel* level, const u32*)
{
    u16* value = &reinterpret_cast<u16*>(&level->bits)[1];
    if (*value != Parameter())
    {
        return 0.0f;
    }

    *value = 0xFFFF;
    return 1.0f;
}

f32 IsAttachedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(node->owner->parent != nullptr);
}

// The instance has an ID
f32 ContextValue154SetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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

f32 KeyPathByte42Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
    if (key == NoKey)
    {
        return 0.0f;
    }

    return YesNo(((key + 1) & 1) == 0);
}

f32 VideoStateIs5Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u32 State5 = 5;
    return YesNo(*reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(G_VideoController) + 8) == State5);
}

// The y of the instance's matrix's second row (made up to date first)
f32 UpVectorXCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    return place->matrix.m[1][1];
}

// Bit 0 of the first integer property clear
f32 IntProp0Bit0Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((Node(node)->properties->GetInt(0) & 1) == 0);
}

f32 VideoReadyCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(VideoReady(G_VideoController) != 0);
}

f32 PhysicsHasCollisionNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* body = reinterpret_cast<u8*>(Node(node)->rigidBody);
    return YesNo(body != nullptr && *reinterpret_cast<void**>(body + 0xD4) != nullptr);
}

f32 NodeValue174CountCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(g_InstancesWithValue174);
}

f32 IsFullInstanceNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(node) + 0x174);
}

f32 NodeByte8cMinusGlobalCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    u8 own = Node(node)->unknown8C;
    u8 global = g_GlobalByte30A0E9;
    if (own == NoByte || global == NoByte)
    {
        return 0.0f;
    }

    return static_cast<f32>(own - global);
}

f32 AlwaysZero173Condition::Check(GameNode*, BehaviourLevel*, const u32*)
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

f32 ChunksLoadedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((G_ChunkLoadingManager_->focusLoader->bits & LinksQueuedBit) != 0);
}

f32 PlayerIsCrouchingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PlayerPart()->moveBits & CharacterPart::Crouching) != 0);
}

f32 PlayerIsGroundedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo((part->flags >> 2 & 1) != 0);
}

// The wumpa fruit
f32 CanJumpForwardsCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->counts & GameProgress::WumpaMask));
}

f32 WillHitLowWallCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The second character's counter's value at 0x18 (none -1)
f32 NodeTrafficCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterCounter* counter = g_PlayerCharacter2->counter;
    if (counter == nullptr)
    {
        return -1.0f;
    }

    return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(counter) + 0x18);
}

// Bit 5 of the route's path's halfword at 4 (no route: no)
f32 EdgeNeedsFlyingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* path = reinterpret_cast<const u8*>(Node(node)->waypoints->routePath);
    if (path == nullptr)
    {
        return 0.0f;
    }

    return YesNo((*reinterpret_cast<const u16*>(path + 4) >> 5 & 1) != 0);
}

f32 PlayerIsMovingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PlayerPart()->flags >> 4 & 1) != 0);
}

f32 PlayerIsWalkingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PartBits(PlayerPart()) >> 42 & 1) != 0);
}

f32 PlayerIsRunningCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PartBits(PlayerPart()) >> 43 & 1) != 0);
}

// The walking bit (retail's)
f32 PlayerIsCrawlingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PartBits(PlayerPart()) >> 42 & 1) != 0);
}

f32 PlayerIsFallingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PlayerPart()->flags >> 5 & 1) != 0);
}

f32 PlayerHoldingMultiToolCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo((PartBits(part) >> 37 & 1) != 0);
}

// The part's low byte (its state)
f32 PlayerIsSlammingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo((part->bits & BasicAgentPart::LowByteMask) == SlammingState);
}

f32 HeadCanSeePlayerUnblockedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo((PartBits(part) >> 33 & 1) != 0);
}

// The agent's part may damage the character
f32 AttachedContextFlag8Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(node->owner)->agent->part);
    return YesNo((part->bits & BasicAgentPart::CanDamageCharacter) != 0);
}

f32 DUMMY_570Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 DUMMY_571Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

f32 CutsceneSkippedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return 0.0f;
}

// The character agent's control (whether the instance has one isn't checked)
f32 CharacterVehiclePointerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* character = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, CharacterNodeKind));
    auto* agent = reinterpret_cast<const u8*>(character->agent);
    return YesNo(*reinterpret_cast<void* const*>(agent + 0xB8) != nullptr);
}

f32 IsPlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    return YesNo(node->owner == player);
}

f32 ObjectContextFlag17Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x20000) != 0);
}

f32 HitByPunchCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x10000) != 0);
}

f32 HitByBodySlam2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x40000) != 0);
}

f32 HitBySpinHitboxCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x20) != 0);
}

f32 HitByBodySlamHitboxCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x1000000) != 0);
}

f32 IsVehicleRollerbrawlCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return VehicleValue(node->owner);
}

f32 HitByCortexBoltCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x80) != 0);
}

f32 ObjectContextFlag1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x2) != 0);
}

f32 PlayerFlag57ClearCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((PartBits(PlayerPart()) >> 57 & 1) == 0);
}

f32 HasActorWeightCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(HasActorWeight(g_PlayerCharacter2, node->owner) != 0);
}

f32 ObjectContextFlag25Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x2000000) != 0);
}

f32 ObjectContextFlag2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x4) != 0);
}

f32 PlayerVehicle1ValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return VehicleValue(PlayerInstance());
}

f32 GlobalInt3098e8Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(g_VarPercept629);
}

// The node's word at 0x134 isn't -1
f32 NodeValue134SetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(Node(node)->unknown134 != -1);
}

// The timed play's count
f32 GameControllerField500HighCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->counts >> GameProgress::CountShift));
}

// The timed play's time left (seconds)
f32 GameTimer57cCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(Progress()->timeLeft) * g_SecondsPerClockUnit;
}

f32 SecondCharacterGunStateCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterCounter* counter = g_PlayerCharacter2->counter;
    if (counter == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(counter->bits >> 9) & 0xF);
}

f32 HasAmmoCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterCounter* counter = g_PlayerCharacter2->counter;
    if (counter == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(counter->bits >> 13) & 0x7F);
}

f32 ObjectContextFlag19Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x80000) != 0);
}

// The pairing 5
f32 GameModeIs5Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u32 Pairing5 = 5;
    return YesNo(Progress()->Field(GameProgress::PairingShift) == Pairing5);
}

f32 ObjectContextFlags3or22Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo((ContactWord(node) & 0x400008) != 0);
}

// The area the story has got to
f32 GlobalProgressionCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(static_cast<s32>(Progress()->bits >> GameProgress::StoryShift & GameProgress::AreaMask));
}

// The second character's control's value at 0xD0 (none 0)
f32 SecondCharacterVehicleValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    auto* control = reinterpret_cast<const u8*>(g_PlayerCharacter2->control);
    if (control == nullptr)
    {
        return 0.0f;
    }

    return *reinterpret_cast<const f32*>(control + 0xD0);
}

// The area play is in
f32 GameStateIsCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo((Progress()->bits >> GameProgress::AreaShift & GameProgress::AreaMask) == Parameter());
}

// Of the model's animator (whether the instance has a model isn't checked): no animator, or nothing left of its animation
f32 AnimationFinishedCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, ModelNodeKind));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 1.0f;
    }

    return GetAnimationProgress(animator, AllJoints) > 0.0f ? 0.0f : 1.0f;
}

// The keys went round
f32 IsPathCompleteCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && (Node(node)->waypoints->flags & Waypoints::FlagWrapped) != 0);
}

f32 GetRouteCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->waypoints->route != nullptr);
}

f32 GotKeysCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && Node(node)->waypoints->keyCount != 0);
}

// The instance within the AI position of the route's step
f32 InsideEdgeStartNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* position = StepPosition(Node(node)->waypoints);
    return YesNo(position != nullptr && IsWithinAiPosition(position, node) != 0);
}

// Of the step before
f32 InsideEdgeEndNodeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AiPosition* position = PreviousStepPosition(Node(node)->waypoints);
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

    return static_cast<f32>(object->MessageWithin(time, values[0]));
}

f32 GotUserMessageEqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    ObjectNode* object = Node(node);
    if (static_cast<s32>(*time) < static_cast<s32>(object->messageTime))
    {
        return 0.0f;
    }

    return static_cast<f32>(object->MessageWithin(Parameter(), time, values[0]));
}

f32 TouchingAnyAgentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RigidBodyBit88(node, 51);
}

// The threshold doubled when the counter has its value (so it passes), else none
f32 CounterValueEqualsThresholdCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    f32 value = static_cast<f32>(GameCounter(G_ChunkManager_0030A0C8, Parameter()));
    return value == values[1] ? values[1] + values[1] : 0.0f;
}

// Taken once
f32 LostAllAttachmentsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    u32* word = AttachmentsWord(attachments);
    if ((*word & LostAllBit) == 0)
    {
        return 0.0f;
    }

    *word &= ~LostAllBit;
    return 1.0f;
}

f32 XCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, 0);
}

f32 YCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, 1);
}

f32 ZCycleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return CycleTurns(node, 2);
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

f32 NodeFlag16Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && (Node(node)->flags & 0x10000) != 0);
}

f32 NodeFlag17Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && (Node(node)->flags & 0x20000) != 0);
}

f32 NodeFlag15Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return YesNo(TakesPackets(node) && (Node(node)->flags & 0x8000) != 0);
}

// The node's countdown out of 255
f32 NodeByte154FractionCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return static_cast<f32>(Node(node)->unknown154) * Rounded(1.0 / 255.0);
}

f32 PhysicsBodyFlag1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RigidBodyBit90(node, 1);
}

// The threshold doubled when the agent's byte has it (so it passes), else none
f32 InstanceSubtypeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    const u8* bytes = Node(node)->agent->unknown18;
    return static_cast<f32>(bytes[Parameter()]) == values[1] ? values[1] + values[1] : 0.0f;
}

// Bit 30 of the head tracking's bits with bit 24 (25, 26, 27 and 28 the next ones)
f32 HeadTrackingFlag24Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return HeadTrackingBits(node, 0x41000000);
}

f32 HeadTrackingFlag25Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return HeadTrackingBits(node, 0x42000000);
}

f32 HeadTrackingFlag26Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return HeadTrackingBits(node, 0x44000000);
}

f32 HeadTrackingFlag27Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return HeadTrackingBits(node, 0x48000000);
}

f32 HeadTrackingFlag28Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return HeadTrackingBits(node, 0x50000000);
}

// The route's step (whether there's a route isn't checked)
f32 SubPathKeyRawCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return 0.0f;
    }

    return static_cast<f32>(Node(node)->waypoints->routeIndex);
}

f32 SubPathKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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

// The current linked object the last
f32 CurrentLinkIndexCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    u32 word = *AttachmentsWord(attachments);
    return YesNo((word >> LinkedIndexShift & LinkedIndexMask) == (word & LinkedCountMask) - 1);
}

// Above where the instance's placement put it
f32 HeightAboveStartCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionOf(node).y - Node(node)->informationPointer->position.y;
}

f32 HasPerception0Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PerceptionOf(node, 0);
}

f32 HasPerception2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PerceptionOf(node, 2);
}

f32 HasPerception1Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PerceptionOf(node, 1);
}

f32 PhysicsBodyFlag5Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RigidBodyBit90(node, 5);
}

// The linked objects' count
f32 HasXLinksCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    return static_cast<f32>(*AttachmentsWord(attachments) & LinkedCountMask);
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
f32 AgentRef1SpawnFlagCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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

    return YesNo((reference->flags & BusyFlag) != 0);
}

// Bit 20 of the rigid body's bits at 0x90, taken once (whether the node takes packets isn't asked)
f32 PhysicsImpactCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u64 ImpactBit = u64{1} << 20;
    ObjectRigidBody* body = Node(node)->rigidBody;
    if (body == nullptr || (body->bits90 & ImpactBit) == 0)
    {
        return 0.0f;
    }

    body->bits90 &= ~ImpactBit;
    return 1.0f;
}

// Of the node it takes its object from when there's one (no key: no)
f32 KeyPathOnLastKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    GameNode* source = Node(node)->sourceNode;
    Waypoints* waypoints = Node(source != nullptr ? source : node)->waypoints;
    if (waypoints->key == NoKey)
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
    constexpr u16 NoBehaviour = 0xFFFF;
    ObjectNode* object = Node(node);
    GameObject* gameObject = object->sourceNode != nullptr ? SourceObject(object->sourceNode) : object->object;
    u16 id;
    GetObjectBehaviourId(&id, gameObject, Parameter());
    return YesNo(id != NoBehaviour);
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
    return static_cast<f32>(PlayerPart()->flags >> HitPointsShift & HitPointsMask);
}

// The agent's state's shadow flag
f32 AgentIsOnGroundCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* agentNode = AgentNodeOf(node->owner);
    if (agentNode == nullptr)
    {
        return 0.0f;
    }

    return YesNo((agentNode->agent->properties->state & Agent::StateShadowActive) != 0);
}

// Bits 2-9 of the crate part's value
f32 CrateHasRedWumpaCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* crate = AgentNodeOfKind(node, CrateNodeKind);
    if (crate == nullptr)
    {
        return 0.0f;
    }

    auto* part = static_cast<CratePart*>(crate->agent->part);
    return YesNo((part->value >> 2 & 0xFF) != 0);
}

// Any attack within the window
f32 AgentWasTouchedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->AttackedWithin(time, values[0]) != 0);
}

// Whether there's a creature node isn't checked
f32 AgentHitPointsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CreaturePart*>(AgentNodeOfKind(node, CreatureNodeKind)->agent->part);
    return static_cast<f32>(part->flags >> HitPointsShift & HitPointsMask);
}

// The last attack, of kind 9, within the window
f32 WillHitWallCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    constexpr u32 WallKind = 9;
    AgentPart* part = AgentPartOf(node);
    return YesNo(part->AttackedWithin(time, values[0]) != 0 && part->lastAttack == WallKind);
}

// Of kinds 13 and 14
f32 WillRunOffCliffCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    constexpr u32 FirstCliffKind = 13;
    AgentPart* part = AgentPartOf(node);
    return YesNo(part->AttackedWithin(time, values[0]) != 0 && static_cast<u32>(part->lastAttack - FirstCliffKind) < 2);
}

f32 AgentWasAttackedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return YesNo(AgentPartOf(node)->HitWithin(time, values[0]) != 0);
}

f32 AgentWasJumpedOnCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    constexpr u32 JumpedOn = 4;
    return YesNo(AgentPartOf(node)->AttackedWithin(JumpedOn, time, values[0]) != 0);
}

f32 AgentWasWalkedIntoCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    constexpr u32 WalkedInto = 3;
    return YesNo(AgentPartOf(node)->AttackedWithin(WalkedInto, time, values[0]) != 0);
}

f32 AgentWasHeadbuttedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    constexpr u32 Headbutted = 5;
    return YesNo(AgentPartOf(node)->AttackedWithin(Headbutted, time, values[0]) != 0);
}

// The gate's number (its part's value's low 12 bits)
f32 WumpaNeededForPayGateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u32 NumberMask = 0xFFF;
    AgentNode* gate = AgentNodeOfKind(node, PayGateNodeKind);
    if (gate == nullptr)
    {
        return 0.0f;
    }

    auto* part = static_cast<PayGatePart*>(gate->agent->part);
    return static_cast<f32>(part->value & NumberMask);
}

// Flag 1 of the AI position of the route's step
f32 NodeIsAirborneCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 1);
}

f32 EdgeNeedsJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, 1u << 2);
}

f32 EdgeNeedsLongJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, 1u << 3);
}

f32 EdgeNeedsHighJumpCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, 1u << 4);
}

f32 PlayerIsCoOpLinkedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    u64 bits = PartBits(PlayerPart());
    return YesNo((bits >> 53 & 1) != 0 || (bits >> 54 & 1) != 0);
}

// The part's state 6 or 10
f32 PlayerIsSpinningCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    u32 state = part->bits & BasicAgentPart::LowByteMask;
    return YesNo(state == 6 || state == 10);
}

// 8 or 12
f32 PlayerIsJumpingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    u32 state = part->bits & BasicAgentPart::LowByteMask;
    return YesNo(state == 8 || state == 12);
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

// Bit 55 of the character part's bits (whether there's a character node isn't checked; 54 the next)
f32 CharacterFlag23Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CharacterPart*>(AgentNodeOfKind(node, CharacterNodeKind)->agent->part);
    return YesNo((PartBits(part) >> 55 & 1) != 0);
}

// The second character's counter's low 4 bits 6
f32 IsChargedShotCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u64 ChargedShot = 6;
    CharacterCounter* counter = g_PlayerCharacter2->counter;
    return YesNo(counter != nullptr && (counter->bits & 0xF) == ChargedShot);
}

// The second character's attack's state 12 to 14
f32 IsDownBlastCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u32 FirstDownBlast = 12;
    const u32* attack = g_PlayerCharacter2->attack;
    return YesNo(attack != nullptr && (*attack & 0x1F) - FirstDownBlast < 3);
}

// Flags of the AI position of the route's step (the b ones the same), and of the step before
f32 SubPathPointFlag0Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 0);
}

f32 CharacterFlag22Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    auto* part = static_cast<CharacterPart*>(AgentNodeOfKind(node, CharacterNodeKind)->agent->part);
    return YesNo((PartBits(part) >> 54 & 1) != 0);
}

f32 CharacterHasVehicleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, CharacterNodeKind);
    return YesNo(character != nullptr && CharacterOf(character)->control != nullptr);
}

// A vehicle of kind 2 (TT Lab's name says the opposite; 3, 4 and 5 the next ones, 5 the second character's without a character
// node)
f32 VehicleTypeNot2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, CharacterNodeKind);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), 2));
}

f32 IsVehicleHumiliskateCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, CharacterNodeKind);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), 3));
}

f32 VehicleTypeNot4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, CharacterNodeKind);
    return YesNo(character != nullptr && HasVehicleOfKind(CharacterOf(character), 4));
}

f32 IsVehicle3Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    AgentNode* character = AgentNodeOfKind(node, CharacterNodeKind);
    return YesNo(HasVehicleOfKind(character != nullptr ? CharacterOf(character) : g_PlayerCharacter2, 5));
}

f32 SubPathPointFlag5Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 5);
}

f32 SubPathPointFlag4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 4);
}

f32 SubPathPointFlag6Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 6);
}

// Flag 8 of the path that led to the route's step
f32 PathSegmentFlag0Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return RoutePathFlag(node, 1u << 8);
}

f32 SubPathPreviousPointFlag5Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(PreviousStepPosition(Node(node)->waypoints), 1u << 5);
}

f32 SubPathPreviousPointFlag4Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(PreviousStepPosition(Node(node)->waypoints), 1u << 4);
}

f32 SubPathPreviousPointFlag6Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(PreviousStepPosition(Node(node)->waypoints), 1u << 6);
}

f32 SubPathPointFlag5bCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 5);
}

f32 SubPathPointFlag4bCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 4);
}

f32 SubPathPointFlag6bCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 6);
}

f32 SubPathPointFlag2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 2);
}

f32 SubPathPreviousPointFlag2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(PreviousStepPosition(Node(node)->waypoints), 1u << 2);
}

f32 SubPathPointFlag2bCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return PositionFlag(StepPosition(Node(node)->waypoints), 1u << 2);
}

// None of flags 1, 2, 4 and 6 (no position: no)
f32 SubPathPointFlags56Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u32 Flags = 0x56;
    AiPosition* position = StepPosition(Node(node)->waypoints);
    return YesNo(position != nullptr && (position->flags & Flags) == 0);
}

// The played character's agent holds something (its handle at 0x290; no character played: the nodes read at 0xD4, retail's)
f32 IsPushingObjectCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    GameProgress* progress = Progress();
    InstanceContext* instance = progress->Instance(progress->Field(GameProgress::CharacterShift));
    auto* nodes = reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
    auto* character = static_cast<AgentNode*>(GetGameNode(nodes, CharacterNodeKind));
    Reference* handle = static_cast<CharacterAgent*>(character->agent)->pushedBody;
    return YesNo(handle != nullptr && handle->object != nullptr);
}

f32 GameFlags44Is12Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(G_GameController_0030988C->State() == GameController::StatePlaying);
}

f32 IsMoviePlayingCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return YesNo(G_GameController_0030988C->State() == GameController::StateMovie);
}

// An instance at the attachments path's entry of the slot
f32 GotAttachmentOnExitCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return 0.0f;
    }

    void* path = *reinterpret_cast<void**>(static_cast<u8*>(attachments) + 0x70);
    if (path == nullptr)
    {
        return 0.0f;
    }

    return YesNo(SlottedAttachment(path, Parameter() & 0xFF) != nullptr);
}

// The played character's agent's chunk at 0x90 not the one its instance is in (whether there's a character played or a
// character node isn't checked)
f32 CharacterHasHomeChunkCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(played), CharacterNodeKind));
    auto* agent = static_cast<CharacterAgent*>(character->agent);
    ChunkData* home = agent->homeChunk;
    return YesNo(home != played->chunk);
}

// The first runner's starter's originator a character that isn't the player, while the characters are paired 1
f32 TriggeredByOtherCharacterCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u32 Pairing1 = 1;
    BehaviourRunner* runner = Node(node)->runners[0];
    if (runner == nullptr || runner->receivers == nullptr)
    {
        return 0.0f;
    }

    auto* originator = static_cast<InstanceContext*>(runner->receivers->originator);
    if (originator == PlayerInstance() || GetGameNode(&originator->nodes, CharacterNodeKind) == nullptr)
    {
        return 0.0f;
    }

    return YesNo(Progress()->Field(GameProgress::PairingShift) == Pairing1);
}

// The physics body touched its chunk's triangles this frame (without one, bit 52 of the rigid body's bits at 0x88)
f32 PhysicsTouchingCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
        return YesNo((body->physicsBody->bodyFlags & RigidBody::FlagTouched) != 0);
    }

    return YesNo((body->bits88 >> 52 & 1) != 0);
}

// The agent's persistent flag (in its chunk's own store or the other one; none: no)
f32 IsLoadZoneStateSetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Agent* agent = Node(node)->agent;
    PropertyHolder* properties = agent->properties;
    if ((properties->state & PropertyHolder::StatePersistentFlag) == 0)
    {
        return 0.0f;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager_0030A0C8, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* flags = (properties->state & PropertyHolder::StateFlagInChunkStore) != 0 ? chunk->flags : chunk->otherFlags;
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

// Attacks of kinds 6 and 10 (7 and 11 knee drops, 8 and 12 slides)
f32 AgentWasSpunCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, values[0], 6, 10);
}

f32 AgentWasKneeDroppedCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, values[0], 7, 11);
}

f32 AgentWasSlidCondition::Check(GameNode* node, BehaviourLevel*, const u32* time)
{
    return AttackedByEither(node, time, values[0], 8, 12);
}

// The player's vehicle's byte at 0x19C while it's the Humiliskate (kind 3; no player: its nodes read at 0xD4)
f32 PlayerSplineVehicleValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u32 Humiliskate = 3;
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayerInstance()), CharacterNodeKind));
    if (character == nullptr)
    {
        return 0.0f;
    }

    CharacterControl* control = reinterpret_cast<PlayerCharacter*>(character->agent)->control;
    if (control == nullptr || control->Kind() != Humiliskate)
    {
        return 0.0f;
    }

    return static_cast<f32>(reinterpret_cast<const u8*>(control)[0x19C]);
}

f32 PlayerFlag14Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    auto* character = static_cast<AgentNode*>(GetGameNode(&played->nodes, CharacterNodeKind));
    return YesNo(character != nullptr && CharacterBit14(character));
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

// The focus instance visible (10), busy (8) and with an attached object (7)
f32 FocusFlag10Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, ReferencedObject::FlagVisible);
}

// The threshold doubled when the focus's agent's byte (at the index the condition keeps) has it (so it passes), else none
// (whether the focus has an object node isn't checked)
f32 FocusObjectByte0EqualsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* focus = Node(node)->AwakeFocus();
    if (focus == nullptr)
    {
        return 0.0f;
    }

    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&focus->nodes, 1));
    const u8* bytes = objectNode->agent->unknown18;
    return static_cast<f32>(bytes[unknown14]) == values[1] ? values[1] + values[1] : 0.0f;
}

f32 FocusIsBusyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, BusyFlag);
}

// The physics body touched another body or its chunk's triangles this frame; without one, while the parameter is none, bit 52
// or 51 of the rigid body's bits at 0x88
f32 TouchingTerrainCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
        return YesNo((body->physicsBody->bodyFlags & (RigidBody::FlagTouchedBody | RigidBody::FlagTouched)) != 0);
    }

    if (Parameter() != 0)
    {
        return 0.0f;
    }

    return YesNo((body->bits88 >> 52 & 1) != 0 || (body->bits88 >> 51 & 1) != 0);
}

f32 FocusHasAttachmentCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusFlag(node, AttachedObjectFlag);
}

// From the node's stored position (none: 1e30)
f32 FocusPositionDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    ObjectNode* object = Node(node);
    if ((object->flags & ObjectNodeBase::FlagStoredPosition) == 0)
    {
        return Rounded(1e30);
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

// Character 1's agent has bit 14 and the played character's hasn't (TT Lab's name says both)
f32 BothCharactersFlag14Condition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    constexpr u32 Character1 = 1;
    InstanceContext* first = Progress()->characters[Character1] != nullptr
                                 ? static_cast<InstanceContext*>(Progress()->characters[Character1]->object)
                                 : nullptr;
    if (first == nullptr)
    {
        return 0.0f;
    }

    auto* firstCharacter = static_cast<AgentNode*>(GetGameNode(&first->nodes, CharacterNodeKind));
    if (firstCharacter == nullptr || !CharacterBit14(firstCharacter))
    {
        return 0.0f;
    }

    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    auto* playedCharacter = static_cast<AgentNode*>(GetGameNode(&played->nodes, CharacterNodeKind));
    if (playedCharacter == nullptr)
    {
        return 0.0f;
    }

    return YesNo(!CharacterBit14(playedCharacter));
}

// How far above the first agent reference's instance the instance is (none: none)
f32 AgentRef1HeightDifferenceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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

    return YesNo((object->flags & ObjectNodeBase::FlagFocusPosition) != 0 || (object->flags & ObjectNodeBase::FlagFocusInstance) != 0);
}

namespace
{
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 Far = 1e30f;
// How far above an instance's feet the checks look from and at
constexpr f32 EyeHeight = 0.5f;
// The collision surfaces sight and walking test
constexpr u32 SightSurfaces = 0x80;
constexpr u32 WalkSurfaces = 0x40;

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
void MakeSightQuery(InstanceRayHit* query, void** results, InstanceContext* instance)
{
    query->results = results;
    query->count = 0;
    query->most = 0x80;
    query->distance = Far;
    query->wantedFlags = 0x10;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->bits = InstanceRayHit::BitAllWanted;
    query->skipped[0] = nullptr;
    query->instance = nullptr;
    query->skipped[1] = nullptr;
    SkipInQuery(query, instance);
}

// The character agent of an instance's character node
CharacterAgent* CharacterAgentOfInstance(InstanceContext* instance)
{
    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind))->agent);
}

// A character's eye height: the second of the floats its sizes have, the fourth while it's ducking (bit 32 of its part's bits)
f32 CharacterEyeHeight(CharacterAgent* agent)
{
    const f32* sizes = reinterpret_cast<const f32*>(agent->body);
    auto* part = static_cast<CharacterPart*>(agent->Part());
    return (PartBits(part) >> 32 & 1) == 1 ? sizes[3] : sizes[1];
}

// Whether a line of sight from a point along a way to an instance is clear: nothing stopped it, or the instance did
bool SightReaches(InstanceContext* instance, ChunkData* chunk, const Vector4* from, Vector4* way, InstanceRayHit* query, u32 mask)
{
    u32 blocked = LineOfSight(chunk, from, way, SightSurfaces, query, mask);
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
    return GetCollisionCheck(instance->chunk, &start, &end, WalkSurfaces, nullptr, &hit, nullptr) != 0;
}

// The position of the node's focus (an awake focus instance's, else the focus position): whether it has one
bool FocusPositionOf(ObjectNode* object, Vector4* position)
{
    if ((object->flags & ObjectNodeBase::FlagFocusInstance) != 0)
    {
        InstanceContext* focus = object->focusInstance;
        if (focus == nullptr || Asleep(focus))
        {
            if (focus != nullptr)
            {
                object->focusInstance = nullptr;
                object->flags &= ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
            }

            return false;
        }

        *position = PlacePosition(focus);
        return true;
    }

    if ((object->flags & ObjectNodeBase::FlagFocusPosition) != 0)
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
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(played, 2);
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
    Vector4 side = PlaceAxis(played, 0);
    return AxisAlongWay(side, position, PlacePosition(node->owner));
}

// The collision (of the sight's surfaces) between the eyes of the instance and of the played character
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
    return GetCollisionCheck(node->owner->chunk, &from, &to, SightSurfaces, nullptr, &hit, nullptr) != 0 ? 1.0f : 0.0f;
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
    Vector4 forward = PlaceAxis(node->owner, 2);
    Vector4 way = PlacePosition(played);
    from.y = from.y + EyeHeight;
    way.y = way.y + EyeHeight;
    Vector4 to = way;
    way.x = way.x - from.x;
    way.z = way.z - from.z;
    way.y = way.y - from.y;
    if (!(values[1] < AxisAlongUnit(forward, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &from, &to, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return values[1] + values[1];
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
    void* results[0x80];
    InstanceRayHit query;
    MakeSightQuery(&query, results, node->owner);
    if (!SightReaches(played, node->owner->chunk, &from, &way, &query, g_SolidKinds))
    {
        return 0.0f;
    }

    return values[1] + values[1];
}

// The operated instance has been operated for 0.2 seconds at most, faces the instance and the instance is nearer to its aim (a
// line 1000 units long, on the ground) than its box reaches
f32 GlobalInstanceOp581Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 OperateSeconds = Rounded(0.2);
    constexpr f32 AimLength = 1000.0f;
    InstanceContext* operated = g_OperatedInstance != nullptr ? static_cast<InstanceContext*>(g_OperatedInstance->object) : nullptr;
    if (operated == nullptr)
    {
        return 0.0f;
    }

    auto* controller = reinterpret_cast<CharacterController*>(CharacterAgentOfInstance(operated)->gun);
    if (controller == nullptr || StartedWithin(controller, static_cast<s32>(g_ClockUnitsPerSecond * OperateSeconds)) == 0)
    {
        return 0.0f;
    }

    Vector4 aimStart = PlacePosition(operated);
    Vector4 aim = PlaceAxis(operated, 2);
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
    if ((instance->flags & ReferencedObject::FlagInDrawnCell) == 0)
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
    void* results[0x80];
    InstanceRayHit query;
    MakeSightQuery(&query, results, instance);
    way.x = way.x - from.x;
    way.y = way.y - from.y;
    way.z = way.z - from.z;
    if (LineOfSight(instance->chunk, &from, &way, SightSurfaces, &query, g_CoverKinds) == 0)
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
    query.bits &= ~InstanceRayHit::BitFull;
    query.count = 0;
    query.instance = nullptr;
    query.distance = Far;
    return SightReaches(played, instance->chunk, &from, &way, &query, g_CoverKinds) ? 1.0f : 0.0f;
}
}

f32 PlayerVisible2Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return SightFromBeside(node, false);
}

f32 PlayerVisible3Condition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
    if (played == nullptr || ExitPointPlace(node->owner, Parameter() & 0xFF, &head, &direction) == 0)
    {
        return 0.0f;
    }

    return AxisAlongWay(direction, head, PlacePosition(played));
}

// Twice the threshold when the exit point of the parameter's slot points at the played character's eyes (its eye height above
// its feet) more than it and the collision doesn't block the view
f32 HeadCanSeePlayerCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(node->owner, Parameter() & 0xFF, &head, &direction) == 0)
    {
        return 0.0f;
    }

    Vector4 way = PlacePosition(played);
    const f32* sizes = reinterpret_cast<const f32*>(CharacterAgentOfInstance(played)->body);
    Vector4 eyes = way;
    eyes.y = way.y + sizes[1];
    way.y = eyes.y - head.y;
    way.x = way.x - head.x;
    way.z = way.z - head.z;
    if (!(values[1] < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &head, &eyes, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return values[1] + values[1];
}

// The exit point of the parameter's slot within 40 units of the played character, pointing at its eyes more than the threshold,
// and its line of sight reaching it: twice the threshold
f32 AmIHarmfulCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 NearSquared = 1600.0f;
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(node->owner, Parameter() & 0xFF, &head, &direction) == 0)
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
    if (!(values[1] < AxisAlongUnit(direction, &unit)))
    {
        return 0.0f;
    }

    void* results[0x80];
    InstanceRayHit query;
    MakeSightQuery(&query, results, node->owner);
    constexpr u32 HarmMask = 0x15B010;
    if (!SightReaches(played, node->owner->chunk, &head, &way, &query, HarmMask))
    {
        return 0.0f;
    }

    return values[1] + values[1];
}

// How far the played character's exit point of the parameter's slot points at the instance's eyes
f32 PlayerHeadLookingAtMeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* played = PlayedInstance();
    Vector4 head;
    Vector4 direction;
    if (played == nullptr || ExitPointPlace(played, Parameter() & 0xFF, &head, &direction) == 0)
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
    if (played == nullptr || ExitPointPlace(played, Parameter() & 0xFF, &head, &direction) == 0)
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
    if (!(values[1] < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &head, &eyes, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return values[1] + values[1];
}

// Nothing (of the walking surfaces) below the instance from 2 units above its feet down to the threshold below them: a bit more
// than twice the threshold
f32 CanFallCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 Above = 2.0f;
    constexpr f32 Margin = Rounded(0.1);
    Vector4 start = PlacePosition(node->owner);
    start.y = start.y + Above;
    Vector4 drop = {0.0f, -(values[1] + Above), 0.0f, 1.0f};
    Vector4 end;
    end.x = start.x + drop.x;
    end.y = start.y + drop.y;
    end.z = start.z + drop.z;
    end.w = 1.0f;
    Vector4 hit;
    if (GetCollisionCheck(node->owner->chunk, &start, &end, WalkSurfaces, nullptr, &hit, nullptr) != 0)
    {
        return 0.0f;
    }

    return values[1] + values[1] + Margin;
}

// The way the threshold ahead of the instance's eyes free of the walking surfaces: twice the threshold
f32 CanMoveForwardsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {0.0f, 0.0f, values[1], 1.0f};
    return WayAlongAxesBlocked(node, offset) ? 0.0f : values[1] + values[1];
}

// The way the threshold behind (left of, right of) the instance's eyes blocked: twice the threshold (retail's, the names say
// otherwise)
f32 CanMoveBackwardsCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {0.0f, 0.0f, -values[1], 1.0f};
    return WayAlongAxesBlocked(node, offset) ? values[1] + values[1] : 0.0f;
}

f32 CanStrafeLeftCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {-values[1], 0.0f, 0.0f, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? values[1] + values[1] : 0.0f;
}

f32 CanStrafeRightCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    Vector4 offset = {values[1], 0.0f, 0.0f, 1.0f};
    return WayAlongAxesBlocked(node, offset) ? values[1] + values[1] : 0.0f;
}

// The squared distance from the instance to the player in its chunk (else far)
f32 MeToPlayerSqrDistCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr || node->owner->chunk != PlayerInstance()->chunk)
    {
        return Far;
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
        return Far;
    }

    Vector4 position = PlacePosition(player);
    Vector4 focus;
    if (!FocusPositionOf(Node(node), &focus))
    {
        return Far;
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
        return Far;
    }

    Vector4 position = PlacePosition(player);
    ObjectNode* object = Node(node);
    if ((object->flags & ObjectNodeBase::FlagFocusPosition) == 0)
    {
        return Far;
    }

    f32 dx = object->focusPosition.x - position.x;
    f32 dy = object->focusPosition.y - position.y;
    f32 dz = object->focusPosition.z - position.z;
    return dx * dx + dy * dy + dz * dz;
}

// The node's speed less the played character's vehicle's (its vehicle's own velocity, without one the agent's at 0x60)
f32 PlayerVectorLengthDifferenceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr u32 VehicleVelocitySlot = 9;
    InstanceContext* played = PlayedInstance();
    if (played == nullptr)
    {
        return 0.0f;
    }

    CharacterAgent* agent = CharacterAgentOfInstance(played);
    Vector4 vehicle;
    auto* control = reinterpret_cast<InstanceContext*>(agent->vehicle);
    if (control != nullptr)
    {
        CallVirtual<void>(control, *reinterpret_cast<const GccVTableEntry* const*>(reinterpret_cast<u8*>(control) + 0xD4),
                          VehicleVelocitySlot, &vehicle);
    }
    else
    {
        vehicle = agent->velocity;
    }

    Vector4 velocity;
    CopyVelocity(Node(node), &velocity);
    return __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) -
           __builtin_sqrtf(vehicle.x * vehicle.x + vehicle.y * vehicle.y + vehicle.z * vehicle.z);
}

// 1 when the played character is nearer to another key than to the current one
f32 PlayerNearCurrentKeyCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
        return Far;
    }

    Vector4 position = PlacePosition(node->owner);
    f32 dx = position.x - step->position.x;
    f32 dy = position.y - step->position.y;
    f32 dz = position.z - step->position.z;
    return dx * dx + dy * dy + dz * dz;
}
}

// The squared distance to the route's step the node is at, and to the step before
f32 SubPathKeyDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return Far;
    }

    return DistanceSquaredToStep(node, StepPosition(Node(node)->waypoints));
}

f32 SubPathPreviousKeyDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    if (!TakesPackets(node))
    {
        return Far;
    }

    return DistanceSquaredToStep(node, PreviousStepPosition(Node(node)->waypoints));
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
    return __builtin_fabsf((parameter - along) * path->lengths[path->count - 4]);
}

// How far the instance is outside the radius of the AI position of its chunk nearest it, squared (none: far)
f32 NearestPointEdgeDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* instance = node->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager_0030A0C8, instance);
    Vector4 position = PlacePosition(instance);
    u16 index;
    AiPosition* nearest = NearestAiPosition(chunk, &position, &index);
    if (nearest == nullptr)
    {
        return Far;
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

    if ((object->flags & ObjectNodeBase::FlagFocusInstance) != 0)
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
    void* results[0x80];
    InstanceRayHit query;
    MakeSightQuery(&query, results, instance);
    if (!SightReaches(seer, instance->chunk, &start, &way, &query, g_SolidKinds))
    {
        return 0.0f;
    }

    return condition->values[1] + condition->values[1];
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
    if (!(condition->values[1] < AxisAlongUnit(axis, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(chunk, &from, &to, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return condition->values[1] + condition->values[1];
}

// 1 when nothing (of the sight's surfaces) is between two points
f32 ClearBetween(ChunkData* chunk, Vector4 from, Vector4 to)
{
    Vector4 hit;
    return GetCollisionCheck(chunk, &from, &to, SightSurfaces, nullptr, &hit, nullptr) != 0 ? 0.0f : 1.0f;
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
    Vector4 forward = PlaceAxis(node->owner, 2);
    return AxisAlongWay(forward, position, focus, FocusEyeHeight);
}

// How far the instance's forward axis points at the first agent reference's eyes
f32 AgentRef1SideOffsetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return 0.0f;
    }

    Vector4 target = PlacePosition(reference);
    Vector4 position = PlacePosition(node->owner);
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(instance, 2);
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
f32 FocusVisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
f32 AgentRef1VisibleCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(instance, 2);
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
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(node->owner, 2);
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
    Vector4 forward = PlaceAxis(camera, 2);
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
    bool inView = test.visibility == 1;
    test.Destroy(2);
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
// The parameter's slot of an exit point
u32 ExitSlot(const ScriptCondition* condition)
{
    return condition->Parameter() & 0xFF;
}

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
    if (!(condition->values[1] < AxisAlongUnit(direction, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(chunk, &head, &target, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return condition->values[1] + condition->values[1];
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
f32 AgentRef1InViewConeCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
f32 FocusFromExitPointCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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

// How far the instance's x axis points at the focus (unknown14 0) or the first agent reference (1): only on its right side
// (unknown18 1), only on its left (0), its size
f32 AngleToFocusCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* owner = node->owner;
    Vector4 target;
    if (unknown14 == 0)
    {
        if (!FocusPositionOf(Node(node), &target))
        {
            return 0.0f;
        }
    }
    else if (unknown14 == 1)
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
    Vector4 side = PlaceAxis(owner, 0);
    f32 along = AxisAlongUnit(side, &way);
    if (unknown18 == 1 && along < 0.0f)
    {
        return __builtin_fabsf(0.0f);
    }

    if (unknown18 == 0 && 0.0f < along)
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
void MakeProbeQuery(InstanceRayHit* query, void** results)
{
    query->results = results;
    query->count = 0;
    query->most = 0x40;
    query->distance = Far;
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = 0;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
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
    if ((object->flags & ObjectNodeBase::FlagFocusPosition) == 0)
    {
        return NoGround;
    }

    Vector4 start = object->focusPosition;
    Vector4 position = PlacePosition(node->owner);
    start.y = start.y + ProbeAbove;
    Vector4 way = {0.0f, ProbeDown, 0.0f, 1.0f};
    void* results[0x40];
    InstanceRayHit query;
    MakeProbeQuery(&query, results);
    if (LineOfSight(node->owner->chunk, &start, &way, WalkSurfaces, &query, g_CoverKinds) == 0)
    {
        return NoGround;
    }

    return __builtin_fabsf((way.y + ProbeAbove) - position.y);
}

// Probing 16 units down from 8 above the point 5 units ahead (behind with the parameter 1): how far below the start it stopped
// less 8 (none: 10000)
f32 ObstacleAheadCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    constexpr f32 Ahead = 5.0f;
    InstanceContext* owner = node->owner;
    Vector4 position = PlacePosition(owner);
    Vector4 forward = PlaceAxis(owner, 2);
    f32 distance = Parameter() == 1 ? -Ahead : Ahead;
    Vector4 start = position;
    start.x = position.x + forward.x * distance;
    start.z = position.z + forward.z * distance;
    start.y = position.y + forward.y * distance + ProbeAbove;
    Vector4 way = {0.0f, ProbeDown, 0.0f, 1.0f};
    void* results[0x40];
    InstanceRayHit query;
    MakeProbeQuery(&query, results);
    if (LineOfSight(owner->chunk, &start, &way, WalkSurfaces, &query, g_CoverKinds) == 0)
    {
        return NoGround;
    }

    return __builtin_fabsf(way.y + ProbeAbove);
}

// The squared distance to the first agent reference (none: far)
f32 DistanceToTargetCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* reference = AwakeAgentRef1(Node(node));
    if (reference == nullptr)
    {
        return Far;
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

    auto* objectNode = static_cast<ObjectNode*>(GetGameNode(&focus->nodes, 1));
    if (objectNode == nullptr || objectNode->agent->objectId == 0xFFFF)
    {
        return 0.0f;
    }

    return static_cast<u32>(objectNode->PacketProperties()->GetInt(0)) == Parameter() ? 1.0f : 0.0f;
}

namespace
{
// The key of the parameter: the current one (no parameter or 0xFF), else the parameter's (counted from 1) while there's such a
// key, or the next one (going round to the first after the last)
LayoutPosition* KeyOfParameter(const Waypoints* waypoints, u32 parameter, bool next)
{
    if (parameter == 0 || parameter == 0xFF)
    {
        u32 key = waypoints->key;
        if (next)
        {
            key = (key + 1) & 0xFF;
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
    void* results[0x10];
    InstanceRayHit query;
    query.results = results;
    query.most = 0x10;
    query.count = 0;
    query.distance = Far;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.bits = InstanceRayHit::BitAllWanted;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, owner);
    f32 result = ChunkInstancesInHull(owner->chunk, &hull, &matrix, g_SolidKinds, &query, 0) != 0 ? 1.0f : 0.0f;
    HullDestroy(&hull, 2);
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

f32 FocusOffXAxisDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 0);
}

f32 FocusHorizontalDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 1);
}

f32 FocusOffForwardAxisDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return FocusAlongAxis(node, 2);
}

f32 AgentRef1OffXAxisDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return AgentRef1AlongAxis(node, 0);
}

f32 AgentRef1HorizontalDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    return AgentRef1AlongAxis(node, 1);
}

f32 AgentRef1OffAxisDistanceSquaredCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
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
    Vector4 forward = PlaceAxis(played, 2);
    Vector4 to = PlacePosition(node->owner);
    from.y = from.y + EyeHeight;
    to.y = to.y + EyeHeight;
    Vector4 way = to;
    way.x = to.x - from.x;
    way.z = to.z - from.z;
    way.y = to.y - from.y;
    if (!(values[1] < AxisAlongUnit(forward, &way)))
    {
        return 0.0f;
    }

    if (GetCollisionCheck(node->owner->chunk, &from, &to, SightSurfaces, nullptr, &way, nullptr) != 0)
    {
        return 0.0f;
    }

    return values[1] + values[1];
}

// How far ahead of the instance the camera is along its forward axis (none, or the instance in a drawn cell: none)
f32 CameraForwardDistanceCondition::Check(GameNode* node, BehaviourLevel*, const u32*)
{
    InstanceContext* camera = CameraInstance();
    if (camera == nullptr || (node->owner->flags & ReferencedObject::FlagInDrawnCell) != 0)
    {
        return 0.0f;
    }

    Vector4 position = PlacePosition(camera);
    Vector4 forward = PlaceAxis(camera, 2);
    Vector4 own = PlacePosition(node->owner);
    return (position.x - own.x) * forward.x + (position.y - own.y) * forward.y + (position.z - own.z) * forward.z;
}

void ConstructConditionChecksModule()
{
    InitConditionChecksModule(1, 0xFFFF);
}
