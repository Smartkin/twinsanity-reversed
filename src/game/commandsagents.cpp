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
#include "game/events.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/reference.h"
#include "game/resources.h"
#include "game/scenery.h"
#include "game/vehicles.h"

#include <cstdint>

// The commands that work on the agents' parts, the characters, the instances' linked objects and the follow camera

extern "C"
{
    // The chunk manager's chunk of an index
    extern void* G_ChunkManager;
    // A character's vehicle left (CharacterAgent::LeaveVehicle)
    void LeaveVehicle(PlayerCharacter* character, u32 replaced) RETAIL(FUN_0013bc60);
}

namespace
{
// The event the linked objects are sent, and the one the nitro crates are
constexpr u32 TriggerEvent = 1;
constexpr u32 NitroEvent = 0xD;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

bool Asleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
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
    return progress->Instance(progress->play.character);
}

// A character node's agent as the player's character (the same object)
PlayerCharacter* CharacterOf(AgentNode* node)
{
    return reinterpret_cast<PlayerCharacter*>(node->agent);
}

// The played character's follow node (no character played: its nodes read at 0xD4)
FollowNode* PlayedFollowNode()
{
    return static_cast<FollowNode*>(GetGameNode(NodesOf(PlayedInstance()), NodeFollow));
}

// The instance's attachments node (none when it has none)
AttachmentsNode* AttachmentsNodeOf(InstanceContext* instance)
{
    return static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, NodeAttachments));
}

InstanceContext** LinkedInstances(AttachmentsNode* attachments)
{
    return attachments->linked;
}

// The linked object the linked object commands are at
u32 CurrentLinked(const AttachmentsNode* attachments)
{
    return attachments->bits.currentLinked;
}

GameObject* ObjectOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
}

// The query of a chunk's instances the commands make (the awake ones, all the wanted flags)
void MakeQuery(InstanceQuery* query, void** results, u16 most)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = 0;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

void SetPartHitPoints(CreaturePart* part, u32 hitPoints)
{
    part->flags.hitPoints = hitPoints;
}

// The instance moved along its own axes (queued to be stepped when it moved)
void MoveInstanceLocally(InstanceContext* instance, const Vector4* offset)
{
    if (MovePlaceLocally(instance->place, offset) != 0)
    {
        QueueObject(instance);
    }
}

// The held flag of the vehicle a character rides when its instance (by its number; none: its nodes read at 0xD4) has a controls
// node and a character node (whether the character rides one isn't checked)
void SetVehicleHeld(u32 character, bool set)
{
    NodeList* nodes = NodesOf(G_GameController->progress.Instance(character));
    void* controls = GetGameNode(nodes, NodeControls);
    auto* node = static_cast<AgentNode*>(GetGameNode(nodes, NodeCharacter));
    if (controls == nullptr || node == nullptr)
    {
        return;
    }

    Vehicle* vehicle = CharacterOf(node)->vehicle;
    vehicle->bits.held = set;
}

// Every linked object's generic object agent sent the event (the instance its sender; the count read again each time)
void TriggerLinkedGenericObjects(InstanceContext* instance)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < attachments->LinkedCount(); index++)
    {
        auto* generic = static_cast<AgentNode*>(GetGameNode(NodesOf(LinkedInstances(attachments)[index]), NodeGenericObject));
        if (generic != nullptr)
        {
            RunAgentEvent(generic->agent, TriggerEvent, reinterpret_cast<u32>(instance), 0, 0);
        }
    }
}
}

// The rigid body's sizes and its magnet's modes (its strength the second, its way the first), the body made when the node has
// none
void SetMagnetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
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
    body->sizes[1] = magnetPull;
    body->state.magnetStrength = magnetStrength;
    body->state.magnetWay = magnetWay;
}

void SetNoiseMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNodeReactions& reactions = NodeOf(runner)->reactions;
    if (settings.message != NoMessage)
    {
        settings.passesNoises = 1;
        reactions.noiseMessage = settings.message;
    }

    reactions.passesNoises = settings.passesNoises;
}

// No player: its nodes read at 0xD4
void SetPlayerScriptFlagCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayerInstance()), NodeCharacter));
    auto* part = static_cast<CharacterPart*>(node->agent->part);
    part->moveBits.scriptFlag = clears.on ^ 1;
}

// The agent's persistent flag set to the value's low byte (in its chunk's own store or the other one)
void SetStateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    Agent* agent = NodeOf(runner)->agent;
    PropertyHolder* properties = agent->properties;
    if (properties->state.persistentFlag == 0)
    {
        return;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, agent->chunkIndex);
    u16 id = agent->id;
    PersistentFlags* flags = properties->state.flagInChunkStore != 0 ? chunk->savedFlags : chunk->unsavedFlags;
    if (flags != nullptr)
    {
        SetPersistentFlag(flags, id, state.flag);
    }
}

// The node's mask controller: the particle trails of the node it keeps destroyed before and after its instance stops taking
// triggers' signals and is hidden, then the controller's kind made 0 (what looks for a mask controller doesn't find it any more)
void ResetMaskControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeController* controller = NodeOf(runner)->controller;
    if (controller == nullptr || controller->kind != NodeController::KindMask)
    {
        return;
    }

    CallVirtual<void>(controller->node, controller->node->vtable, ObjectNode::DestroyParticleTrailsSlot);
    controller->node->owner->flags.receivesTriggerSignals = 0;
    controller->node->owner->flags.visible = 0;
    CallVirtual<void>(controller->node, controller->node->vtable, ObjectNode::DestroyParticleTrailsSlot);
    controller->kind = NodeController::KindJointAim;
}

// Its distance a second, for the frame's seconds
void NowMoveForwardsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = speed.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {0.0f, 0.0f, step, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

void NowStrafeRightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = speed.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {step, 0.0f, 0.0f, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

void NowStrafeLeftCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    f32 step = -speed.FloatWith(NodeOf(runner)->PacketProperties()) * g_FrameSeconds;
    Vector4 offset = {step, 0.0f, 0.0f, 1.0f};
    MoveInstanceLocally(instance, &offset);
}

// The model's animation of a joint stopped, blending out over the seconds given (whether the instance has a model isn't checked)
void ClearAnimationCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return;
    }

    s32 blend = static_cast<s32>(blendTime.FloatWith(node->PacketProperties()) * g_ClockUnitsPerSecond);
    StopOgiAnimation(animator, blend, joint.id);
}

// The model node animated at its next update however long its instance went unseen (mode 1), always animated (2) or not any
// more (any other)
void ForceAnimationUpdateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, NodeModel));
    if (model == nullptr)
    {
        return;
    }

    u32 mode = update.mode;
    if (mode == AnimationUpdate::UpdateOnce)
    {
        model->bits.ogiChanged = 1;
    }
    else if (mode == AnimationUpdate::UpdateAlways)
    {
        model->bits.alwaysAnimated = 1;
    }
    else
    {
        model->bits.alwaysAnimated = 0;
    }
}

// The first awake instance of the chunk with a camera trigger whose box overlaps the instance's (of 32 at most) put to sleep
void TriggerInstanceAtOwnBoxCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    constexpr u32 CameraTriggers = 1 << NodeCameraTrigger;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceQuery query;
    MakeQuery(&query, results, Most);
    if (QueryChunkInstances(instance->chunk, &instance->collision.box, CameraTriggers, &query) != 0)
    {
        auto* found = static_cast<ReferencedObject*>(results[0]);
        CallVirtual<u32>(found, found->vtable, ReferencedObject::SleepSlot);
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

    InstanceContext* actor = G_GameController->progress.Instance(character.character);
    if (actor == nullptr)
    {
        return;
    }

    node->focusInstance = actor;
    node->flags.focusInstance = 1;
    node->flags.focusPosition = 0;
}

void HoldVehicleCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    SetVehicleHeld(character, true);
}

void ReleaseVehicleCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    SetVehicleHeld(character, false);
}

// What the played character's follow node follows the focus (whether there's a follow node isn't checked; following nothing
// leaves the focus flag as it was), the focus position forgotten
void SetFocusToCameraTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    auto* followed = follow->cameraInstance != nullptr ? static_cast<InstanceContext*>(follow->cameraInstance->object) : nullptr;
    ObjectNode* node = NodeOf(runner);
    node->focusInstance = followed;
    if (followed != nullptr)
    {
        node->flags.focusInstance = 1;
    }

    node->flags.focusPosition = 0;
}

// The follow camera's target the instance (unless the follow camera keeps its target)
void SetFollowCameraTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* played = PlayedInstance();
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* follow = static_cast<FollowNode*>(GetGameNode(NodesOf(played), NodeFollow));
    if (follow == nullptr || follow->camera.bits.keepsTarget != 0)
    {
        return;
    }

    follow->camera.rig.SetTarget(instance);
}

// The follow camera's positioner and target take their own cameras (cleared)
void UseOwnFollowCamerasCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    FollowCameraPositioner* positioner = &follow->camera.rig.ownPositioner;
    positioner->bits.ownCamera = 1;
    positioner->ClearTriggerValues();
    FollowCameraTarget* target = &follow->camera.rig.ownTarget;
    target->bits.ownCamera = 1;
    target->ClearTriggerValues();
}

// Back to the triggers' cameras, their own cameras made to only steer
void UseTriggerCamerasCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    FollowCameraPositioner* positioner = &follow->camera.rig.ownPositioner;
    positioner->camera.flags.value = 0;
    positioner->camera.flags.steers = 1;
    positioner->bits.ownCamera = 0;
    positioner->ClearTriggerValues();
    FollowCameraTarget* target = &follow->camera.rig.ownTarget;
    target->camera.flags.value = 0;
    target->camera.flags.steers = 1;
    target->bits.ownCamera = 0;
    target->ClearTriggerValues();
}

// The follow camera's positioner's own camera given a rate: the one its place is followed at, or the yaw blender's speed (from
// radians a second)
void SetFollowCameraRateCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    MainCamera* camera = &follow->camera.rig.ownPositioner.camera;
    u32 kind = which.kind;
    if (kind == FollowCameraRate::PositionRate)
    {
        camera->flags.setsPositionFollowRate = 1;
        camera->positionFollowRate = rate;
    }
    else if (kind == FollowCameraRate::YawSpeed)
    {
        s32 angle;
        AngleFrom(&angle, rate, 0);
        camera->flags.setsYawSpeed = 1;
        camera->yawSpeed = static_cast<u32>(angle);
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

// Whether there's a grabbable node isn't checked
void SetChiChiGrassCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    if (setting.given == 0)
    {
        return;
    }

    auto* grabbable = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeGrabbable));
    auto* part = static_cast<GrabbablePart*>(grabbable->agent->part);
    part->grabbable.unused0 = setting.on;
}

// The character's part's hit points (more than none: its agent no longer dead), else the creature's
void SetHitPointsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    NodeList* nodes = &NodeOf(runner)->owner->nodes;
    auto* character = static_cast<AgentNode*>(GetGameNode(nodes, NodeCharacter));
    if (character != nullptr)
    {
        auto* agent = static_cast<CharacterAgent*>(character->agent);
        SetPartHitPoints(static_cast<CreaturePart*>(agent->part), hitPoints);
        if (static_cast<s32>(hitPoints) > 0)
        {
            agent->state.dead = 0;
        }

        return;
    }

    auto* creature = static_cast<AgentNode*>(GetGameNode(nodes, NodeCreature));
    if (creature != nullptr)
    {
        SetPartHitPoints(static_cast<CreaturePart*>(creature->agent->part), hitPoints);
    }
}

// The node's motion block (made when it has none, the instance then a physics body) made sticky with the values
void BecomeStickyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    MotionBlock* block = node->motionBlock;
    if (block == nullptr)
    {
        block = ConstructMotionBlock(MemoryAllocate(sizeof(MotionBlock)), 0, 0);
        node->motionBlock = block;
        node->owner->flags.physicsBody = 1;
        node->motionBlock->node = node;
    }

    node->owner->flags.physicsBody = 1;
    block->flags.sticky = 1;
    block->stickyKinds |= static_cast<u32>(kinds);
    block->stickyStrength = strength;
    block->stickyMessage = static_cast<u32>(message);
    if (object.object != 0)
    {
        block->stickyObject = object.object;
    }
}

// The focus position moved by the vector (none: nothing)
void OffsetFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (!node->flags.focusPosition)
    {
        return;
    }

    Vector4 position = node->focusPosition;
    position.x = position.x + offset.x;
    position.y = position.y + offset.y;
    position.z = position.z + offset.z;
    node->focusPosition = position;
    node->flags.focusPosition = 1;
    node->flags.focusInstance = 0;
}

// The runner's originator's agent told of a contact of damage from the instance
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
    message.hitKinds = hitKinds;
    message.damage = static_cast<u8>(damage.IntWith(properties));
    AgentNode* agentNode = AgentNodeOf(originator);
    if (agentNode != nullptr)
    {
        Agent* agent = agentNode->agent;
        CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &message, instance, 0u);
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
    node->flags.storedPosition = 1;
}

// The second agent reference the first linked object (taken out of the list, kept while asleep) when it has none; then the
// parts of the reference's object node let go and the reference put to sleep (an asleep one not kept forgotten first: then read
// at null)
void CacheLinkedInstanceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node->agentRef2 == nullptr)
    {
        AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
        node->agentRef2 = TakeFirstLinked(attachments, 1);
        node->flags.keepsAgentRef2 = 1;
    }

    if (node->agentRef2 == nullptr)
    {
        return;
    }

    if (Asleep(node->agentRef2) && !node->flags.keepsAgentRef2)
    {
        node->agentRef2 = nullptr;
    }

    InstanceContext* linked = node->agentRef2;
    auto* objectNode = static_cast<GameNode*>(GetGameNode(NodesOf(linked), NodeObject));
    CallVirtual<void>(objectNode, objectNode->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
    CallVirtual<u32>(linked, linked->vtable, InstanceContext::SleepSlot);
}

// The behaviour of the object's slot (bits 0-15) started on the node, forced, in the runner of bit 16 (the object of the node it
// takes its object from when there's one)
void RunScriptSlotCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u16 id;
    GetObjectBehaviourId(&id, ObjectOf(node), request.slot);
    ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
    auto* starter = id != NoScriptId ? static_cast<ScriptStarter*>(scripts->items[id & ResourceIndexMask]) : nullptr;
    if (starter != nullptr)
    {
        CallVirtual<u32>(node, node->vtable, ObjectNode::StartBehaviourSlot, starter, node->owner, 1u,
                         request.runner);
    }
}

// Every linked object's agent sent the event (its sender the runner's originator with bit 0, else the instance)
void TriggerLinkedObjectsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments == nullptr)
    {
        return;
    }

    u32 count = attachments->LinkedCount();
    void* sender = fromOriginator.on != 0 ? runner->originator : node->owner;
    for (u32 index = 0; index < count; index++)
    {
        auto* linked = static_cast<ObjectNode*>(GetGameNode(NodesOf(LinkedInstances(attachments)[index]), NodeObject));
        if (linked != nullptr)
        {
            RunAgentEvent(linked->agent, TriggerEvent, reinterpret_cast<u32>(sender), 0, 0);
        }
    }
}

// The instance hanging on the exit point (0xFF: the one at none) the focus (none: no focus)
void RequestAttachmentFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* target = nullptr;
    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments != nullptr)
    {
        AttachmentsPath* path = attachments->path;
        if (path != nullptr)
        {
            u8 index = hanging.exitPoint;
            target = index != GameOGI::NoExitPoint ? SlottedAttachment(path, index) : UnslottedAttachment(path);
        }
    }

    if (target != nullptr)
    {
        node->focusInstance = target;
        node->flags.focusInstance = 1;
        node->flags.focusPosition = 0;
    }
    else
    {
        node->flags.focusPosition = 0;
        node->flags.focusInstance = 0;
    }
}

// The character pushed back by its node's velocity times the value (kind 0x6F) with its second float property (no character
// node: nothing)
void NowGoBackCollidableCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 PushBackKind = 0x6F;
    ObjectNode* node = NodeOf(runner);
    auto* character = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, NodeCharacter));
    if (character == nullptr)
    {
        return;
    }

    Agent* agent = character->agent;
    PropertyHolder* properties = agent->properties;
    f32 second = properties->GetFloat(1);
    f32 times = scale.FloatWith(properties);
    Vector4 velocity = node->motion->velocity;
    velocity.x = velocity.x * times;
    velocity.y = velocity.y * times;
    velocity.z = velocity.z * times;
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

    auto* character = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (character == nullptr)
    {
        return;
    }

    auto* creature = static_cast<AgentNode*>(GetGameNode(&focus->nodes, NodeCreature));
    if (creature == nullptr)
    {
        return;
    }

    SetPlayerVehicle(CharacterOf(character), WrestledCreature, reinterpret_cast<PlayerCharacter*>(creature->agent), 0);
}

// The played character's or a character's object node given the object (the object of the node it takes its object from when
// there's one) as its sound's
void CharacterSoundProxyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    GameProgress* progress = &G_GameController->progress;
    u32 character = choice.played != 0 ? progress->play.character : choice.character;
    InstanceContext* instance = progress->Instance(character);
    if (instance == nullptr)
    {
        return;
    }

    auto* characterNode = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
    SetSoundObject(characterNode, ObjectOf(NodeOf(runner)));
}

// The trajectory's cycles' amplitudes (its motion floats) about the axes picked
void SetCycleAmplitudesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
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
    if (axes.x != 0)
    {
        trajectory->motionFloats[0] = amplitudeX.FloatWith(properties);
    }

    if (axes.y != 0)
    {
        trajectory->motionFloats[1] = amplitudeY.FloatWith(properties);
    }

    if (axes.z != 0)
    {
        trajectory->motionFloats[2] = amplitudeZ.FloatWith(properties);
    }
}

// A designator's instance's object node made to take its object from itself again, or every linked object's
void StopTargetBehaviourCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 designator = request.designator;
    if (designator != DesignatesNone)
    {
        auto* target = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, u32{designator});
        if (target != nullptr)
        {
            static_cast<ObjectNodeBase*>(GetGameNode(&target->nodes, NodeObject))->sourceNode = nullptr;
        }

        return;
    }

    if (request.everyLinked == 0)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < attachments->LinkedCount(); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
        if (linked != nullptr)
        {
            static_cast<ObjectNodeBase*>(GetGameNode(&linked->nodes, NodeObject))->sourceNode = nullptr;
        }
    }
}

// The played character's Humiliskate pushed along z by the instance (whether there's a character node or a vehicle isn't checked)
void PushPlayerVehicleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* character = static_cast<AgentNode*>(GetGameNode(NodesOf(PlayedInstance()), NodeCharacter));
    PlayerCharacter* player = CharacterOf(character);
    if (player->vehicle->Kind() != Vehicle::KindHumiliskate)
    {
        return;
    }

    Vector4 pushed = {0.0f, 0.0f, push, 0.0f};
    Vehicle* vehicle = player->vehicle;
    CallVirtual<void>(vehicle, vehicle->vtable, Vehicle::SlotPush, &pushed, runner->agentNode->owner);
}

// The attachments' current linked object the next one of the list's (numbers from 1, as many as the count; the current one not
// in the list: kept)
void NextLinkedObjectInListCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 NotFound = 0xFF;
    AttachmentsNode* attachments = AttachmentsNodeOf(NodeOf(runner)->owner);
    if (attachments == nullptr)
    {
        return;
    }

    AttachmentsNodeBits bits = attachments->bits;
    u8 total = count.numbers;
    u32 current = bits.currentLinked + 1;
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
    if (linked < bits.linkedCount)
    {
        bits.currentLinked = linked;
        attachments->bits = bits;
    }
}

// The chunk's crates (128 at most) sent the nitro event
void TriggerAllNitroCratesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x80;
    constexpr u32 Crates = 1 << NodeCrate;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceQuery query;
    MakeQuery(&query, results, Most);
    s32 count = QueryChunkInstancesOfKinds(instance->chunk, Crates, &query);
    for (u16 index = 0; index < static_cast<u32>(count); index++)
    {
        auto* crate = static_cast<AgentNode*>(GetGameNode(&static_cast<InstanceContext*>(results[index])->nodes, NodeCrate));
        RunAgentEvent(crate->agent, NitroEvent, 0, 0, 0);
    }
}

// A character's vehicle left (its controls' handler dropped); when it had one, played alone with no second character
void ExitVehicleModeCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* instance = progress->Instance(character);
    if (instance == nullptr)
    {
        return;
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    auto* character = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (controls == nullptr || character == nullptr)
    {
        return;
    }

    PlayerCharacter* player = CharacterOf(character);
    ReplaceControlsHandler(controls, nullptr);
    bool hadVehicle = player->vehicle != nullptr;
    LeaveVehicle(player, 0);
    if (hadVehicle)
    {
        PlayState play = progress->play;
        play.pairing = PairingAlone;
        play.second = GameProgress::NoCharacter;
        progress->play = play;
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
    Vector4 push = impulse;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuRotateVector(&place->matrix, &push, &push);
    // Retail stores the node's roll radius there first
    Vector4 point = heldNode->middle;
    point.w = 1.0f;
    PushRigidBody(body, &push, &point);
}

// A linked object unlinked: every one, the current one, the one of the index, else the designator's instance when it's linked
void UnlinkTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments == nullptr)
    {
        return;
    }

    if (request.all != 0)
    {
        UnlinkAll(attachments);
        return;
    }

    InstanceContext* linked;
    if (request.current != 0)
    {
        linked = LinkedInstances(attachments)[CurrentLinked(attachments)];
    }
    else if (request.byIndex != 0)
    {
        linked = LinkedInstances(attachments)[request.target];
    }
    else
    {
        linked = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, u32{request.target});
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

// What the runner's end leaves: bit 0 the particles, 1 the trajectory controller, 2-4 the node's bits 10, 11 and 12 (nothing
// reads them), 5 the perception
void KeepCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    node->flags.keepsParticles = keeps.particles;
    node->flags.keepsTrajectory = keeps.trajectory;
    node->flags.unused11 = keeps.unused3;
    node->flags.unused10 = keeps.unused2;
    node->flags.unused12 = keeps.unused4;
    node->flags.keepsPerception = keeps.perception;
}
