#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/attachments.h"
#include "game/cameras.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/controllers.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/reference.h"
#include "game/resources.h"
#include "game/scenery.h"

#include <cstdint>

// The commands that work on the agents' parts, the characters, the instances' linked objects and the follow camera

extern "C"
{
    // The chunk manager's chunk of an index
    extern void* G_ChunkManager;
    // The object of a node's instance (the node it takes its object from)
    // A character's vehicle left
    void LeaveVehicle(PlayerCharacter* character, u32 unknown) RETAIL(FUN_0013bc60);
}


namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 ModelNodeKind = 3;
constexpr u32 AttachmentsKind = 6;
constexpr u32 CharacterNodeKind = 0xC;
constexpr u32 CrateNodeKind = 0xD;
constexpr u32 CreatureNodeKind = 0xF;
constexpr u32 GenericObjectNodeKind = 0x10;
constexpr u32 GrabbableNodeKind = 0x11;
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 StartBehaviourSlot = 18;
constexpr u32 GetDesignatorSlot = 36;
// The agents' contact message function, and the referenced objects' sleep
constexpr u32 ContactSlot = 9;
constexpr u32 SleepSlot = 3;
constexpr u8 NoDesignator = 0xFF;
constexpr u16 NoBehaviour = 0xFFFF;
// The attachments node's word: the linked objects' count (bits 0-4) and the current one (bits 7-11), the instances after it
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;
// The event the linked objects are sent, and the one the nitro crates are
constexpr u32 TriggerEvent = 1;
constexpr u32 NitroEvent = 0xD;
// A creature part's hit points (bits 6-13 of its flags)
constexpr u32 HitPointsShift = 6;
constexpr u32 HitPointsMask = 0xFF;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
}

bool Asleep(const InstanceContext* instance)
{
    return (instance->flags & ReferencedObject::FlagAsleep) != 0;
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// The instance of the character played
InstanceContext* PlayedInstance()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->Field(GameProgress::CharacterShift));
}

// A character node's agent as the player's character (the same object)
PlayerCharacter* CharacterOf(AgentNode* node)
{
    return reinterpret_cast<PlayerCharacter*>(node->agent);
}

// The played character's follow node (no character played: its nodes read at 0xD4)
FollowNode* PlayedFollowNode()
{
    return static_cast<FollowNode*>(GetGameNode(NodesOf(PlayedInstance()), Node16));
}

u32* AttachmentsWord(void* attachments)
{
    return reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18);
}

InstanceContext** LinkedInstances(void* attachments)
{
    return reinterpret_cast<InstanceContext**>(static_cast<u8*>(attachments) + 0x20);
}

// The attachments' path (the entries with their slots)
void* PathOf(void* attachments)
{
    return *reinterpret_cast<void**>(static_cast<u8*>(attachments) + 0x70);
}

GameObject* ObjectOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
}

void SetFlag(ObjectNode* node, u32 flag, bool set)
{
    if (set)
    {
        node->flags |= flag;
    }
    else
    {
        node->flags &= ~flag;
    }
}

// The query of a chunk's instances the commands make (the awake ones, all the wanted flags)
void MakeQuery(InstanceRayHit* query, void** results, u16 most)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Rounded(1e30);
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = 0;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

void SetPartHitPoints(CreaturePart* part, u32 hitPoints)
{
    part->flags = (part->flags & ~(HitPointsMask << HitPointsShift)) | (hitPoints & HitPointsMask) << HitPointsShift;
}

// The instance moved along its own axes (queued to be stepped when it moved)
void MoveInstanceLocally(InstanceContext* instance, const Vector4* offset)
{
    if (MovePlaceLocally(instance->place, offset) != 0)
    {
        QueueObject(instance);
    }
}

// The flags of a character's control (bit 2) when its instance (by its number; none: its nodes read at 0xD4) has a controls node
// and a character node (whether the character has a control isn't checked)
void SetCharacterControlFlag(u32 character, bool set)
{
    constexpr u64 Flag2 = 0x4;
    NodeList* nodes = NodesOf(G_GameController->progress.Instance(character));
    void* controls = GetGameNode(nodes, NodeControls);
    auto* node = static_cast<AgentNode*>(GetGameNode(nodes, CharacterNodeKind));
    if (controls == nullptr || node == nullptr)
    {
        return;
    }

    CharacterControl* control = CharacterOf(node)->control;
    control->bits = set ? control->bits | Flag2 : control->bits & ~Flag2;
}

// Every linked object's generic object agent sent the event (the instance its sender; the count read again each time)
void TriggerLinkedGenericObjects(InstanceContext* instance)
{
    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < (*AttachmentsWord(attachments) & LinkedCountMask); index++)
    {
        auto* generic = static_cast<AgentNode*>(GetGameNode(NodesOf(LinkedInstances(attachments)[index]), GenericObjectNodeKind));
        if (generic != nullptr)
        {
            RunAgentEvent(generic->agent, TriggerEvent, reinterpret_cast<u32>(instance), 0, 0);
        }
    }
}
}

// The rigid body's sizes and modes (bits 6-7 the second, 8-9 the first), the body made when the node has none
void SetPhysicsSizesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr)
    {
        return;
    }

    ObjectRigidBody* body = node->RigidBody();
    if (body == nullptr)
    {
        return;
    }

    body->sizes[0] = size;
    body->sizes[1] = size2;
    body->bits90 = (body->bits90 & ~u64{0xC0}) | static_cast<u64>(mode2.raw & 3) << 6;
    body->bits90 = (body->bits90 & ~u64{0x300}) | u64{mode1 & 3} << 8;
}

// The node's bit fields at 0x150: bits 42-57 a value (0xFFFF keeps them; giving one sets the command's own bit 16), bit 40 the
// command's bit 16
void SetNode150FieldsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Kept = 0xFFFF;
    constexpr u32 Given = 0x10000;
    auto* bits = reinterpret_cast<u64*>(&NodeOf(runner)->unknown150);
    if ((values & 0xFFFF) != Kept)
    {
        values |= Given;
        *bits = (*bits & ~(u64{0xFFFF} << 42)) | u64{values & 0xFFFF} << 42;
    }

    *bits = (*bits & ~(u64{1} << 40)) | u64{values >> 16 & 1} << 40;
}

// Bit 57 of the player character's part's bits: the value's bit 0 clear (no player: its nodes read at 0xD4)
void SetPlayerFlag57Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayerInstance()), CharacterNodeKind));
    auto* part = static_cast<CharacterPart*>(node->agent->part);
    u64* bits = &part->Bits();
    *bits = (*bits & ~(u64{1} << 57)) | u64{((value & 0xFF) ^ 1) & 1} << 57;
}

// The agent's persistent flag set to the value's low byte (in its chunk's own store or the other one)
void SetStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Agent* agent = NodeOf(runner)->agent;
    PropertyHolder* properties = agent->properties;
    if ((properties->state & PropertyHolder::StatePersistentFlag) == 0)
    {
        return;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* flags = (properties->state & PropertyHolder::StateFlagInChunkStore) != 0 ? chunk->flags : chunk->otherFlags;
    if (flags != nullptr)
    {
        SetPersistentFlag(flags, id, value1 & 0xFF);
    }
}

// The part 0x114 bytes in while on (its first byte 1): the node it keeps told (slot 24) before and after its instance stops
// taking triggers' signals and is hidden, then off
void ResetMaskControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 NodeSlot24 = 24;
    struct Part114
    {
        u8 on;
        GameNode* node;
    };

    auto* part = reinterpret_cast<Part114*>(NodeOf(runner)->unknown114);
    if (part == nullptr || part->on != 1)
    {
        return;
    }

    CallVirtual<void>(part->node, part->node->vtable, NodeSlot24);
    part->node->owner->flags &= ~ReferencedObject::FlagTriggerSignals & ~ReferencedObject::FlagVisible;
    CallVirtual<void>(part->node, part->node->vtable, NodeSlot24);
    part->on = 0;
}

// Its distance a second, for the frame's seconds
void NowMoveForwardsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = distance.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {0.0f, 0.0f, step, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

void NowStrafeRightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = distance.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {step, 0.0f, 0.0f, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

void NowStrafeLeftCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = -distance.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {step, 0.0f, 0.0f, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

// The model's animation of a joint stopped, blending out over the seconds given (whether the instance has a model isn't checked)
void ClearAnimationCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, ModelNodeKind));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return;
    }

    s32 blend = static_cast<s32>(value2.FloatWith(node->PacketProperties()) * g_ClockUnitsPerSecond);
    StopOgiAnimation(animator, blend, value1 & 0xFF);
}

// The model node's bits: mode 1 sets bit 24, 2 bit 25, any other clears bit 25
void ForceAnimationUpdateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Bit24 = 0x1000000;
    constexpr u32 Bit25 = 0x2000000;
    auto* model = static_cast<ModelNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, ModelNodeKind));
    if (model == nullptr)
    {
        return;
    }

    u32 mode = value1.raw & 0xF;
    if (mode == 1)
    {
        model->bits |= Bit24;
    }
    else if (mode == 2)
    {
        model->bits |= Bit25;
    }
    else
    {
        model->bits &= ~Bit25;
    }
}

// The first awake instance of the chunk whose box overlaps the instance's with bit 8 of its flags (of 32 at most) put to sleep
void TriggerInstanceAtOwnBoxCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    constexpr u32 Flags = 0x100;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceRayHit query;
    MakeQuery(&query, results, Most);
    if (QueryChunkInstances(instance->chunk, &instance->collision.box, Flags, &query) != 0)
    {
        auto* found = static_cast<ReferencedObject*>(results[0]);
        CallVirtual<u32>(found, found->vtable, SleepSlot);
    }
}

// A character's instance (by its number) the focus
void SetFocusToGameActorCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (!TakesPackets(node))
    {
        return;
    }

    InstanceContext* actor = G_GameController->progress.Instance(actorIndex & 0xFF);
    if (actor == nullptr)
    {
        return;
    }

    node->focusInstance = actor;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
}

void SetCharacterFlag2Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    SetCharacterControlFlag(character, true);
}

void ClearCharacterFlag2Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    SetCharacterControlFlag(character, false);
}

// What the played character's follow node follows the focus (whether there's a follow node isn't checked; following nothing
// leaves the focus flag as it was), the focus position forgotten
void SetFocusToCameraTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    auto* followed = follow->object != nullptr ? static_cast<InstanceContext*>(follow->object->object) : nullptr;
    ObjectNode* node = NodeOf(runner);
    node->focusInstance = followed;
    if (followed != nullptr)
    {
        node->flags |= ObjectNodeBase::FlagFocusInstance;
    }

    node->flags &= ~ObjectNodeBase::FlagFocusPosition;
}

// The follow camera's target the instance (unless the follow node's bit 7 says the scripts don't set it)
void CameraNodeSetTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u64 KeepsTarget = 0x80;
    InstanceContext* played = PlayedInstance();
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* follow = static_cast<FollowNode*>(GetGameNode(NodesOf(played), Node16));
    if (follow == nullptr || (follow->camera.Bits() & KeepsTarget) != 0)
    {
        return;
    }

    follow->camera.rig.SetTarget(instance);
}

// The follow camera's positioner and target take their own cameras (cleared)
void CameraNodeEnableFlagsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    FollowCameraPositioner* positioner = &follow->camera.rig.ownPositioner;
    positioner->bits |= FollowCameraPositioner::BitOwnCamera;
    positioner->Clear();
    FollowCameraTarget* target = &follow->camera.rig.ownTarget;
    target->bits |= FollowCameraTarget::BitOwnCamera;
    target->Clear();
}

// Back to the triggers' cameras, their own cameras made to only steer
void CameraNodeClearFlagsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    FollowCameraPositioner* positioner = &follow->camera.rig.ownPositioner;
    positioner->camera.flags = MainCamera::FlagSteers;
    positioner->bits &= ~u64{FollowCameraPositioner::BitOwnCamera};
    positioner->Clear();
    FollowCameraTarget* target = &follow->camera.rig.ownTarget;
    target->camera.flags = MainCamera::FlagSteers;
    target->bits &= ~u64{FollowCameraTarget::BitOwnCamera};
    target->Clear();
}

// The follow camera's positioner's own camera given a value: mode 0 the rig's second value, 1 the yaw extra (from degrees)
void SetCameraNodeValueCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    MainCamera* camera = &follow->camera.rig.ownPositioner.camera;
    u32 which = mode.raw & 7;
    if (which == 0)
    {
        camera->flags |= MainCamera::FlagPassesSecondValue;
        camera->secondValue = value;
    }
    else if (which == 1)
    {
        s32 angle;
        AngleFrom(&angle, value, 0);
        camera->flags |= MainCamera::FlagSetsYawExtra;
        camera->yawExtra = static_cast<u32>(angle);
    }
}

// An impact of the radius and loudness sent from the middle of the instance's box (w its top corner's)
void MakeNoiseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    const Box& box = instance->collision.box;
    Vector4 middle = box.max;
    middle.x = (middle.x - box.min.x) * 0.5f + box.min.x;
    middle.y = (middle.y - box.min.y) * 0.5f + box.min.y;
    middle.z = (middle.z - box.min.z) * 0.5f + box.min.z;
    SendImpact(radius, loudness, instance, &middle);
}

void OpenAllLinkedFurnitureCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TriggerLinkedGenericObjects(NodeOf(runner)->owner);
}

// The same event as opening
void CloseAllLinkedFurnitureCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TriggerLinkedGenericObjects(NodeOf(runner)->owner);
}

// With the value's bit 0, bit 0 of the grabbable part's value its bit 16 (whether there's a grabbable node isn't checked)
void SetChiChiGrassCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if ((value1 & 1) == 0)
    {
        return;
    }

    auto* grabbable = static_cast<AgentNode*>(GetGameNode(&instance->nodes, GrabbableNodeKind));
    auto* part = static_cast<GrabbablePart*>(grabbable->agent->part);
    part->value = (part->value & ~1u) | (value1 >> 16 & 1);
}

// The character's part's hit points (more than none: bit 14 of its agent's bits at 0x70 cleared), else the creature's
void SetHitPointsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeList* nodes = &NodeOf(runner)->owner->nodes;
    auto* character = static_cast<AgentNode*>(GetGameNode(nodes, CharacterNodeKind));
    if (character != nullptr)
    {
        auto* agent = static_cast<CharacterAgent*>(character->agent);
        SetPartHitPoints(static_cast<CreaturePart*>(agent->part), hitPoints);
        if (static_cast<s32>(hitPoints) > 0)
        {
            agent->StateBits() &= ~u64{CharacterAgent::StateDead};
        }

        return;
    }

    auto* creature = static_cast<AgentNode*>(GetGameNode(nodes, CreatureNodeKind));
    if (creature != nullptr)
    {
        SetPartHitPoints(static_cast<CreaturePart*>(creature->agent->part), hitPoints);
    }
}

// The node's motion block (made when it has none, the instance then a physics body) made sticky with the values
void BecomeStickyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 StickyFlag = 0x2000;
    ObjectNode* node = NodeOf(runner);
    MotionBlock* block = node->motionBlock;
    if (block == nullptr)
    {
        block = ConstructMotionBlock(MemoryAllocate(sizeof(MotionBlock)), 0, 0);
        node->motionBlock = block;
        node->owner->flags |= ReferencedObject::FlagPhysicsBody;
        node->motionBlock->node = node;
    }

    node->owner->flags |= ReferencedObject::FlagPhysicsBody;
    block->flags |= StickyFlag;
    block->stickyFlags |= static_cast<u32>(value1);
    block->stickyValue = value2;
    block->stickyMessage = static_cast<u32>(message);
    if ((objectId & 0xFFFF) != 0)
    {
        block->stickyObject = static_cast<u16>(objectId);
    }
}

// The focus position moved by the vector (none: nothing)
void OffsetFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if ((node->flags & ObjectNodeBase::FlagFocusPosition) == 0)
    {
        return;
    }

    Vector4 position = node->focusPosition;
    position.x = position.x + x;
    position.y = position.y + y;
    position.z = position.z + z;
    node->focusPosition = position;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusPosition) & ~ObjectNodeBase::FlagFocusInstance;
}

// The runner's originator's agent told of a contact from the instance (the contact word and the hit points)
void DamageOriginatorCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    PropertyHolder* properties = node->PacketProperties();
    auto* originator = static_cast<InstanceContext*>(runner->originator);
    if (originator == nullptr)
    {
        return;
    }

    ContactMessage message;
    ContactMessage::Construct(&message);
    message.word = contactWord;
    message.byte = static_cast<u8>(hitPoints.IntWith(properties));
    AgentNode* agentNode = AgentNodeOf(originator);
    if (agentNode != nullptr)
    {
        Agent* agent = agentNode->agent;
        CallVirtual<void>(agent, agent->vtable, ContactSlot, &message, instance, 0u);
    }
}

// The node's stored position the player's
void SetFocusPositionToPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* player = PlayerInstance();
    if (player == nullptr)
    {
        return;
    }

    ObjectNode* node = NodeOf(runner);
    if (!TakesPackets(node))
    {
        return;
    }

    ObjectPlace* place = player->place;
    place->SyncPosition();
    node->storedPosition = place->position;
    node->flags |= ObjectNodeBase::FlagStoredPosition;
}

// The second agent reference the first linked object (taken out of the list, kept while asleep) when it has none; then the
// object node of the reference told (slot 11) and the reference put to sleep (an asleep one not kept forgotten first: then read
// at null)
void CacheLinkedInstanceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 NodeSlot11 = 11;
    ObjectNode* node = NodeOf(runner);
    if (node->agentRef2 == nullptr)
    {
        void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
        node->agentRef2 = TakeFirstLinked(attachments, 1);
        node->flags |= ObjectNodeBase::FlagKeepsAgentRef2;
    }

    if (node->agentRef2 == nullptr)
    {
        return;
    }

    if (Asleep(node->agentRef2) && (node->flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
    {
        node->agentRef2 = nullptr;
    }

    InstanceContext* linked = node->agentRef2;
    auto* objectNode = static_cast<GameNode*>(GetGameNode(NodesOf(linked), ObjectNodeKind));
    CallVirtual<void>(objectNode, objectNode->vtable, NodeSlot11);
    CallVirtual<u32>(linked, linked->vtable, SleepSlot);
}

// The behaviour of the object's slot (bits 0-15) started on the node, forced, in the runner of bit 16 (the object of the node it
// takes its object from when there's one)
void RunScriptSlotCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u16 id;
    GetObjectBehaviourId(&id, ObjectOf(node), slotAndFlags & 0xFFFF);
    ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
    auto* starter = id != NoBehaviour ? static_cast<ScriptStarter*>(scripts->items[id & 0x7FFF]) : nullptr;
    if (starter != nullptr)
    {
        CallVirtual<u32>(node, node->vtable, StartBehaviourSlot, starter, node->owner, 1u, slotAndFlags >> 16 & 1);
    }
}

// Every linked object's agent sent the event (its sender the runner's originator with bit 0, else the instance)
void TriggerLinkedObjectsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    u32 count = *AttachmentsWord(attachments) & LinkedCountMask;
    void* sender = (value1 & 1) != 0 ? runner->originator : node->owner;
    for (u32 index = 0; index < count; index++)
    {
        auto* linked = static_cast<ObjectNode*>(GetGameNode(NodesOf(LinkedInstances(attachments)[index]), ObjectNodeKind));
        if (linked != nullptr)
        {
            RunAgentEvent(linked->agent, TriggerEvent, reinterpret_cast<u32>(sender), 0, 0);
        }
    }
}

// The instance of the attachments path's entry of the slot (0xFF: the entry without one) the focus (none: no focus)
void RequestAttachmentFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 NoSlot = 0xFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* target = nullptr;
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments != nullptr)
    {
        void* path = PathOf(attachments);
        if (path != nullptr)
        {
            u8 slot = value1 & 0xFF;
            target = slot != NoSlot ? SlottedAttachment(path, slot) : UnslottedAttachment(path);
        }
    }

    if (target != nullptr)
    {
        node->focusInstance = target;
        node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
    }
    else
    {
        node->flags = node->flags & ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
    }
}

// The character pushed back by its node's velocity times the value (kind 0x6F) with its second float property (no character
// node: nothing)
void NowGoBackCollidableCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 PushBackKind = 0x6F;
    ObjectNode* node = NodeOf(runner);
    auto* character = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, CharacterNodeKind));
    if (character == nullptr)
    {
        return;
    }

    Agent* agent = character->agent;
    PropertyHolder* properties = agent->properties;
    f32 second = properties->GetFloat(1);
    f32 scale = value1.FloatWith(properties);
    Vector4 velocity = node->motion->velocity;
    velocity.x = velocity.x * scale;
    velocity.y = velocity.y * scale;
    velocity.z = velocity.z * scale;
    static_cast<CharacterAgent*>(agent)->PushBack(second, &velocity, PushBackKind, nullptr);
}

// The character's vehicle (kind 6) the focus instance's creature (an asleep focus forgotten)
void SetVehicleWrestleCreatureCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 WrestledCreature = 6;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    auto* character = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (character == nullptr)
    {
        return;
    }

    auto* creature = static_cast<AgentNode*>(GetGameNode(&focus->nodes, CreatureNodeKind));
    if (creature == nullptr)
    {
        return;
    }

    SetPlayerVehicle(CharacterOf(character), WrestledCreature, reinterpret_cast<PlayerCharacter*>(creature->agent), 0);
}

// The played character's (bit 8) or a character's object node given the object (the object of the node it takes its object
// from when there's one) as its sound's
void CharacterSoundProxyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 PlayedBit = 0x100;
    GameProgress* progress = &G_GameController->progress;
    u32 character = (value1 & PlayedBit) != 0 ? progress->Field(GameProgress::CharacterShift) : value1 & 0xFF;
    InstanceContext* instance = progress->Instance(character);
    if (instance == nullptr)
    {
        return;
    }

    auto* characterNode = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, ObjectNodeKind));
    SetSoundObject(characterNode, ObjectOf(NodeOf(runner)));
}

// The trajectory controller's three motion floats, the ones the last value's bits say
void SetMotionFloatsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (!TakesPackets(node))
    {
        return;
    }

    Trajectory* trajectory = node->trajectory;
    if (trajectory == nullptr)
    {
        return;
    }

    PropertyHolder* properties = node->PacketProperties();
    if ((set.raw & 1) != 0)
    {
        trajectory->motionFloats[0] = a.FloatWith(properties);
    }

    if ((set.raw & 2) != 0)
    {
        trajectory->motionFloats[1] = b.FloatWith(properties);
    }

    if ((set.raw & 4) != 0)
    {
        trajectory->motionFloats[2] = c.FloatWith(properties);
    }
}

// A designator's instance's object node made to take its object from itself again, or with bit 9 every linked object's
void StopTargetBehaviourCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 LinkedBit = 0x200;
    ObjectNode* node = NodeOf(runner);
    u8 designator = targetAndFlags & 0xFF;
    if (designator != NoDesignator)
    {
        auto* target = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, u32{designator});
        if (target != nullptr)
        {
            static_cast<ObjectNodeBase*>(GetGameNode(&target->nodes, ObjectNodeKind))->sourceNode = nullptr;
        }

        return;
    }

    if ((targetAndFlags & LinkedBit) == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < (*AttachmentsWord(attachments) & LinkedCountMask); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
        if (linked != nullptr)
        {
            static_cast<ObjectNodeBase*>(GetGameNode(&linked->nodes, ObjectNodeKind))->sourceNode = nullptr;
        }
    }
}

// The played character's vehicle of kind 3 given the value (the third of a vector, its slot 4, with the instance; whether there's
// a character node or a vehicle isn't checked)
void SetPlayerVehicleValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 ValueSlot = 4;
    constexpr u32 Humiliskate = 3;
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayedInstance()), CharacterNodeKind));
    PlayerCharacter* player = CharacterOf(character);
    if (player->control->Kind() != Humiliskate)
    {
        return;
    }

    Vector4 values = {0.0f, 0.0f, value, 0.0f};
    CharacterControl* control = player->control;
    CallVirtual<void>(control, control->vtable, ValueSlot, &values, runner->agentNode->owner);
}

// The attachments' current linked object the next one of the list's (numbers from 1, as many as the count; the current one not
// in the list: kept)
void NextLinkedObjectInListCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 NotFound = 0xFF;
    void* attachments = GetGameNode(&NodeOf(runner)->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    u32* word = AttachmentsWord(attachments);
    u32 bits = *word;
    const auto* numbers = reinterpret_cast<const u8*>(&links1);
    u8 total = static_cast<u8>(count);
    u32 current = (bits >> LinkedIndexShift & LinkedIndexMask) + 1;
    u8 found = NotFound;
    for (u8 index = 0; index < total; index++)
    {
        if (numbers[index] == current)
        {
            found = index;
        }
    }

    if (found == NotFound)
    {
        return;
    }

    u8 next = found + 1;
    if (next == total)
    {
        next = 0;
    }

    u8 linked = numbers[next] - 1;
    if (linked < (bits & LinkedCountMask))
    {
        *word = (bits & ~(LinkedIndexMask << LinkedIndexShift)) | (linked & LinkedIndexMask) << LinkedIndexShift;
    }
}

// The chunk's instances with bit 13 of their flags (128 at most) sent event 13 (whether each has a crate node isn't checked)
void TriggerAllNitroCratesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x80;
    constexpr u32 NitroFlag = 0x2000;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceRayHit query;
    MakeQuery(&query, results, Most);
    s32 count = QueryChunkInstancesByFlags(instance->chunk, NitroFlag, &query);
    for (u16 index = 0; index < static_cast<u32>(count); index++)
    {
        auto* crate = static_cast<AgentNode*>(GetGameNode(&static_cast<InstanceContext*>(results[index])->nodes, CrateNodeKind));
        RunAgentEvent(crate->agent, NitroEvent, 0, 0, 0);
    }
}

// A character's vehicle left (its controls' handler dropped); when it had one, played alone with no second character
void ExitVehicleModeCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    constexpr u32 Alone = 1;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* instance = progress->Instance(value1);
    if (instance == nullptr)
    {
        return;
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    auto* character = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (controls == nullptr || character == nullptr)
    {
        return;
    }

    PlayerCharacter* player = CharacterOf(character);
    ReplaceControlsHandler(controls, nullptr);
    bool hadVehicle = player->control != nullptr;
    LeaveVehicle(player, 0);
    if (hadVehicle)
    {
        u32 bits = (progress->bits & ~(GameProgress::FieldMask << GameProgress::PairingShift)) | Alone << GameProgress::PairingShift;
        progress->bits = (bits & ~(GameProgress::FieldMask << GameProgress::SecondShift)) |
                         GameProgress::NoCharacter << GameProgress::SecondShift;
    }
}

// The body the character holds pushed by the vector (turned by the instance's place) at its node's middle (w 1)
void ApplyVelocityToHeldBodyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    CharacterAgent* character = CharacterAgentOf(instance);
    if (character == nullptr)
    {
        return;
    }

    Reference* held = character->standingOn;
    auto* heldInstance = held != nullptr ? static_cast<InstanceContext*>(held->object) : nullptr;
    if (heldInstance == nullptr)
    {
        return;
    }

    ObjectNode* heldNode = PacketNodeOf(heldInstance);
    if (heldNode == nullptr)
    {
        return;
    }

    ObjectRigidBody* body = heldNode->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    FollowOwnMotionBlock(heldNode);
    Vector4 impulse = {x, y, z, w};
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuRotateVector(&place->matrix, &impulse, &impulse);
    // Retail stores the node's roll radius there first
    Vector4 point = heldNode->unknown20;
    point.w = 1.0f;
    PushRigidBody(body, &impulse, &point);
}

// A linked object unlinked: bit 8 every one, bit 10 the current one, bit 9 the one of the low byte's index, else the designator's
// instance when it's linked
void UnlinkTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 AllBit = 0x100;
    constexpr u32 IndexBit = 0x200;
    constexpr u32 CurrentBit = 0x400;
    ObjectNode* node = NodeOf(runner);
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    u32 bits = target.raw;
    if ((bits & AllBit) != 0)
    {
        UnlinkAll(attachments);
        return;
    }

    InstanceContext* linked;
    if ((bits & CurrentBit) != 0)
    {
        linked = LinkedInstances(attachments)[*AttachmentsWord(attachments) >> LinkedIndexShift & LinkedIndexMask];
    }
    else if ((bits & IndexBit) != 0)
    {
        linked = LinkedInstances(attachments)[bits & 0xFF];
    }
    else
    {
        linked = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, bits & 0xFF);
        if (IndexOfLinked(attachments, linked) == -1)
        {
            linked = nullptr;
        }
    }

    if (linked != nullptr)
    {
        UnlinkInstance(attachments, linked, 0, 1, 0);
    }
}

// What the runner's end leaves: bit 0 the particles, 1 the trajectory controller, 2-4 the node's bits 10, 11 and 12, 5 the
// perception
void KeepCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    SetFlag(node, ObjectNodeBase::FlagKeepsParticles, (flags.raw & 0x1) != 0);
    SetFlag(node, ObjectNodeBase::FlagKeepsTrajectory, (flags.raw & 0x2) != 0);
    SetFlag(node, 0x800, (flags.raw & 0x8) != 0);
    SetFlag(node, 0x400, (flags.raw & 0x4) != 0);
    SetFlag(node, 0x1000, (flags.raw & 0x10) != 0);
    SetFlag(node, ObjectNodeBase::FlagKeepsPerception, (flags.raw & 0x20) != 0);
}
