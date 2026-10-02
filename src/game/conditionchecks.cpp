#include "game/conditions.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/gamecontroller.h"
#include "game/math.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/pads.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/rigidbody.h"

#include <cstdint>

// The conditions' checks converted so far: a score (1 or 0 for the yes or no ones) for the agent's node, the level and the
// clock's time

extern "C"
{
    // The counter of an index (the chunk manager's counters; still asm)
    s32 CounterValue(void* manager, u32 counter) RETAIL(FUN_00269130);
    extern void* G_ChunkManager_0030A0C8;
    // Whether the video controller's movie is ready (still asm)
    u32 VideoReady(VideoController* controller) RETAIL(FUN_0029e8a8);
    // A value of an instance's vehicle, and whether an instance weighs on the character (still asm)
    f32 VehicleValue(InstanceContext* instance) RETAIL(FUN_00128c58);
    u32 HasActorWeight(PlayerCharacter* character, InstanceContext* instance) RETAIL(FUN_0013f748);
    extern s32 g_InstancesWithValue174 RETAIL(D_0030A0FC);
    extern s32 g_VarPercept629 RETAIL(D_003098E8);
    extern u8 g_GlobalByte30A0E9 RETAIL(D_0030A0E9);
    // The value of a perception's slot: whether it has the slot (still asm)
    u32 PerceptionValue(void* perception, u32 slot, f32* value) RETAIL(FUN_0023ffb0);
    // Whether the center of a node's instance's box is within an AI position: from 0.8 below it to 3.7 above, inside its radius
    // across (still asm)
    u32 IsWithinAiPosition(const AiPosition* position, GameNode* node) RETAIL(FUN_0022c760);
    // The object of a node's instance (the node it takes its object from)
    GameObject* ObjectOfNode(GameNode* node) RETAIL(GetGameObjectAddress_FromInstance_);
    // The instance of the attachments path's entry of a slot (none: nullptr; still asm)
    InstanceContext* SlottedAttachment(void* path, u32 slot) RETAIL(FUN_00195600);
    // The chunk manager's chunk of an index, and a persistent flag of a store (still asm)
    ChunkEntry* ChunkOfIndex(void* manager, u16 index) RETAIL(FUN_00268e60);
    u32 GetPersistentFlag(PersistentFlags* flags, u32 index) RETAIL(GetPersistentFlag);
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
    return *reinterpret_cast<const u64*>(&part->unknown18);
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
    return (*reinterpret_cast<const u64*>(static_cast<CharacterAgent*>(character->agent)->unknown70) >> 14 & 1) != 0;
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
    auto* animator = static_cast<OgiAnimator*>(model->unknown24);
    if (animator == nullptr)
    {
        return 0.0f;
    }

    return GetAnimationProgress(animator, AllJoints);
}

f32 CounterValueCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    return static_cast<f32>(CounterValue(G_ChunkManager_0030A0C8, Parameter()));
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
    return YesNo((PlayerPart()->unknown1C & 1) != 0);
}

f32 PlayerIsGroundedCondition::Check(GameNode*, BehaviourLevel*, const u32*)
{
    CharacterPart* part = PlayerPart();
    if (part == nullptr)
    {
        return 0.0f;
    }

    return YesNo((part->unknown14 >> 2 & 1) != 0);
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
    return YesNo((PlayerPart()->unknown14 >> 4 & 1) != 0);
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
    return YesNo((PlayerPart()->unknown14 >> 5 & 1) != 0);
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
    auto* animator = static_cast<OgiAnimator*>(model->unknown24);
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
    f32 value = static_cast<f32>(CounterValue(G_ChunkManager_0030A0C8, Parameter()));
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
    GameObject* gameObject = object->sourceNode != nullptr ? ObjectOfNode(object->sourceNode) : object->object;
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
    return static_cast<f32>(PlayerPart()->unknown14 >> HitPointsShift & HitPointsMask);
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
    return static_cast<f32>(part->unknown14 >> HitPointsShift & HitPointsMask);
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
    Reference* handle = static_cast<CharacterAgent*>(character->agent)->handle290;
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
    ChunkData* home = *reinterpret_cast<ChunkData* const*>(agent->unknown70 + 0x20);
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
