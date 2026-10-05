#include "game/commands.h"

#include "game/animation.h"
#include "game/attachments.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/events.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/resources.h"

#include <cstddef>
#include <cstdint>

// The commands that set an agent's focus (an instance or a position), its agent references and its stored position from
// designators, keys, messages and linked objects, set and change counters, unlink and run behaviours on linked objects, restart
// the behaviour, turn and warp; and the object node's slot animation

extern "C"
{
    // The game's counters (the chunk manager's, 0x1018 bytes in) set and added to
    extern void* G_ChunkManager;
    // The animation of the object's slot played on the node's model (the slot's model put on it first), blending in over some
    // seconds, looping when asked; stopped when the slot has no animation
    void PlaySlotAnimation(f32 blendSeconds, ObjectNode* node, u32 slot, u32 loops) RETAIL_N32(FUN_002124c0);
}

EABI_EXPORT(FUN_002124c0, PlaySlotAnimation);

namespace
{

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

ObjectNode* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
}

AttachmentsNode* AttachmentsNodeOf(InstanceContext* instance)
{
    return static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, NodeAttachments));
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

InstanceContext* DesignatorOf(ObjectNode* node, u32 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, designator);
}

bool DesignatorPosition(ObjectNode* node, u32 designator, Vector4* position)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::GetDesignatorPositionSlot, designator, position) != 0;
}

void ForgetDesignator(ObjectNode* node, u32 slot)
{
    CallVirtual<void>(node, node->vtable, ObjectNode::ForgetDesignatorSlot, slot);
}

// The object the node's behaviours come from: the one of the node it takes its object from when there's one
GameObject* ObjectOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
}

// The place of an instance that may be none (retail reads the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// The instance's place's position, worked out from its matrix first when it moved
Vector4 PositionOf(const InstanceContext* instance)
{
    ObjectPlace* place = RetailPlaceOf(instance);
    place->SyncPosition();
    return place->position;
}

const Vector4& RowOf(const Matrix4x4& matrix, u32 row)
{
    return *reinterpret_cast<const Vector4*>(matrix.m[row]);
}

void SetFocusInstance(ObjectNode* node, InstanceContext* instance)
{
    node->focusInstance = instance;
    node->flags.focusInstance = 1;
    node->flags.focusPosition = 0;
}

void SetFocusPosition(ObjectNode* node, const Vector4& position)
{
    node->focusPosition = position;
    node->flags.focusPosition = 1;
    node->flags.focusInstance = 0;
}

// An instance given to one of the node's designators: the focus, an agent reference or the stored position (its position)
void GiveInstance(ObjectNode* node, u32 slot, InstanceContext* instance)
{
    switch (slot)
    {
    case SlotFocus:
        SetFocusInstance(node, instance);
        break;
    case SlotAgentRef1:
        node->agentRef1 = instance;
        break;
    case SlotAgentRef2:
        node->agentRef2 = instance;
        break;
    case SlotStoredPosition:
    {
        Vector4 position = PositionOf(instance);
        SetStoredPosition(node, &position);
        break;
    }
    default:
        break;
    }
}

u32 CurrentLinked(const AttachmentsNode* attachments)
{
    return attachments->bits.currentLinked;
}

void SetCurrentLinked(AttachmentsNode* attachments, u32 index)
{
    attachments->bits.currentLinked = index;
}

// A counter of an agent's (its bytes 0x18 on)
u8* CounterOf(Agent* agent, s32 counter)
{
    return reinterpret_cast<u8*>(agent) + offsetof(Agent, counters) + counter;
}

void AddToCounter(u8* counter, s32 amount)
{
    s32 value = *counter + amount;
    if (value > Agent::CounterMax)
    {
        *counter = Agent::CounterMax;
    }
    else if (value >= 0)
    {
        *counter = static_cast<u8>(value);
    }
    else
    {
        *counter = 0;
    }
}

// The node of the instance a designator (0xFF none) gives, else the node itself
ObjectNode* DesignatedNode(ObjectNode* node, u8 designator)
{
    if (designator == DesignatesNone)
    {
        return node;
    }

    InstanceContext* designated = DesignatorOf(node, designator);
    return designated != nullptr ? ObjectNodeOf(designated) : node;
}

// The instance a starter receiver's index stands for (retail reads past the receivers' 8 instances for the larger ones)
InstanceContext* ReceiverInstance(BehaviourRunner* runner, u8 index)
{
    return runner->receivers->instances[index];
}

// The vector of a command's four floats
const Vector4* VectorAt(const f32* x)
{
    return reinterpret_cast<const Vector4*>(x);
}

// The instance turned about y by an angle a second times the frame's seconds
void TurnBy(BehaviourRunner* runner, const TaggedValue* turnRate, bool left)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    TaggedValue angle;
    TaggedValue::AngleWith(&angle, turnRate, node->PacketProperties());
    f32 rate = left ? -g_FrameSeconds : g_FrameSeconds;
    s32 turn = static_cast<s32>(static_cast<f32>(angle.raw) * rate);
    ObjectPlace* place = instance->place;
    if (turn == 0)
    {
        return;
    }

    place->SyncRotation();
    place->MarkTurned();
    Vector4 rotation;
    s32 yaw = turn;
    RotationFromYaw(&rotation, &yaw);
    MultiplyRotations(&rotation, &rotation, &place->rotation);
    place->rotation.x = rotation.x;
    place->rotation.y = rotation.y;
    place->rotation.z = rotation.z;
    place->rotation.w = rotation.w;
    QueueObject(instance);
}

// The instance's position, or a unit ahead of it along its z axis
Vector4 PositionOrAhead(InstanceContext* instance, bool ahead)
{
    Vector4 position = PositionOf(instance);
    if (!ahead)
    {
        return position;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    const Vector4& forward = RowOf(place->matrix, 2);
    position.x = position.x + forward.x;
    position.y = position.y + forward.y;
    position.z = position.z + forward.z;
    return position;
}

// An instance warped to a position: turned toward it instead when asked (its body too, else over the ground), else moved there
// (the position kept as where its frame started); the agent told
void WarpTo(ObjectNode* node, InstanceContext* instance, const Vector4& position, bool turns, bool body)
{
    if (turns)
    {
        if (body)
        {
            SteerBodyTowards(1.0f, 0.0f, instance, &position, 1);
        }
        else
        {
            SteerTowards(1.0f, 0.0f, SteerMostLean, instance, &position);
        }
    }
    else
    {
        ObjectPlace* place = RetailPlaceOf(instance);
        place->SyncPosition();
        if (place->MoveTo(&position))
        {
            QueueObject(instance);
        }

        node->frameStart = position;
    }
}
}

// The starter the runner runs started again on its instance by an event (forced, in the runner's slot, the instance its
// originator), once the level finished and the node was told no packet runs
void RestartPreviousCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    GameNode* node = runner->agentNode;
    InstanceContext* instance = node->owner;
    u16 starter = runner->previousStarter;
    level->bits.finished = 1;
    CallVirtual<void>(node, node->vtable, ObjectNode::PacketEndedSlot, runner);
    if (starter == NoScriptId)
    {
        return;
    }

    u16 index = starter;
    Reference* argument = instance != nullptr ? AddReference(instance) : nullptr;
    ScriptEvent* event = ScriptEvent::Construct(static_cast<ScriptEvent*>(MemoryAllocate(sizeof(ScriptEvent))), &index,
                                                runner->flags.slot, 1,
                                                &argument, instance, ObjectNodeKinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(instance, &handle);
}

// A designator given what another designates (of the agent's own or of another agent's designators), an instance or else its
// position
void CopyDesignatorCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 source = designators.source;
    u8 destination = designators.destination;
    u8 agent = designators.sourceAgent;
    ObjectNode* from = node;
    if (agent != DesignatesItself)
    {
        from = PacketNodeOf(DesignatorOf(node, agent));
    }

    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    InstanceContext* instance = DesignatorOf(from, source);
    if (instance != nullptr)
    {
        position = PositionOf(instance);
    }
    else if (!DesignatorPosition(from, source, &position))
    {
        return;
    }

    if (CallVirtual<u32>(node, node->vtable, ObjectNode::SetDesignatorSlot, u32{destination}, instance) == 0)
    {
        CallVirtual<u32>(node, node->vtable, ObjectNode::SetDesignatorPositionSlot, u32{destination}, &position);
    }
}

namespace
{
// The instance let go of by the attachments of another (by force when asked)
void UnlinkFrom(InstanceContext* other, InstanceContext* instance, u32 force)
{
    if (other == nullptr)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(other);
    if (attachments != nullptr)
    {
        UnlinkInstance(attachments, instance, 0, 1, force);
    }
}
}

// The agent's instance let go of by what links it: a designator's instance or, by the agent's own linked objects, every one, the
// current one or the one of the index
void UnlinkFromTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u8 designator = target.designatorOrIndex;
    if (designator != DesignatesNone && !target.byIndex)
    {
        UnlinkFrom(DesignatorOf(node, designator), instance, target.force);
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    if (target.everyLinked)
    {
        if (attachments->LinkedCount() == 0)
        {
            return;
        }

        SetCurrentLinked(attachments, 0);
        while (true)
        {
            UnlinkFrom(attachments->linked[CurrentLinked(attachments)], instance, target.force);
            u32 index = CurrentLinked(attachments);
            if (index == attachments->LinkedCount() - 1)
            {
                return;
            }

            SetCurrentLinked(attachments, index + 1);
        }
    }

    InstanceContext* linked;
    if (target.current)
    {
        linked = attachments->linked[CurrentLinked(attachments)];
    }
    else if (target.byIndex)
    {
        linked = attachments->linked[designator];
    }
    else
    {
        return;
    }

    UnlinkFrom(linked, instance, target.force);
}

namespace
{
// A starter started on an instance's object node, forced, the instance its originator, in the runner slot given (the node made
// to take its object from the agent's when the command says so)
void RunOn(ObjectNode* node, ObjectNode* other, InstanceContext* instance, ScriptStarter* starter,
           RunSlotBehaviourOnLinkedCommand::Slot slot)
{
    if (slot.givesSource)
    {
        other->sourceNode = node;
    }

    CallVirtual<u32>(other, other->vtable, ObjectNode::StartBehaviourSlot, starter, instance, 1u, slot.runnerSlot);
}
}

// The behaviour of the object's slot run on a designator's instance, else on every linked object or on the linked objects of an
// object
void RunSlotBehaviourOnLinkedCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u16 id;
    GetObjectBehaviourId(&id, ObjectOf(node), slot.slot);
    ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
    auto* starter = id != NoScriptId ? static_cast<ScriptStarter*>(scripts->items[id & ResourceIndexMask]) : nullptr;
    if (starter == nullptr)
    {
        return;
    }

    u8 designator = target.designator;
    if (designator != DesignatesNone)
    {
        InstanceContext* designated = DesignatorOf(node, designator);
        if (designated != nullptr)
        {
            RunOn(node, ObjectNodeOf(designated), designated, starter, slot);
        }

        return;
    }

    bool every = slot.everyLinked;
    if (!every && target.objectId == NoLinkedObjectId)
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
        InstanceContext* linked = attachments->linked[index];
        if (linked == nullptr)
        {
            continue;
        }

        ObjectNode* other = ObjectNodeOf(linked);
        if (!every && (other->agent->objectId & ResourceIndexMask) != target.objectId)
        {
            continue;
        }

        RunOn(node, other, linked, starter, slot);
    }
}

void PlaySlotAnimation(f32 blendSeconds, ObjectNode* node, u32 slot, u32 loops)
{
    u16 objectSlot = static_cast<u16>(slot);
    GameObject* object = node->object;
    InstanceContext* instance = node->owner;
    u16 modelId;
    GetObjectModelId(&modelId, object, objectSlot);
    if (modelId == NoModelId)
    {
        return;
    }

    GameResources* resources = G_GameResourcesObjectPointer;
    u16 id = modelId;
    auto* ogi = id != NoModelId ? static_cast<GameOGI*>(resources->models->items[id & ResourceIndexMask]) : nullptr;
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    u16 animationId;
    GetObjectAnimationId(&animationId, object, objectSlot);
    model->SetOgi(ogi, object->header.reactJoints, object->header.exitPoints);
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return;
    }

    GameAnimation* animation = nullptr;
    if (animationId != NoAnimationId)
    {
        id = animationId;
        ResourceTable* animations = resources->animations;
        animation = id != NoAnimationId ? static_cast<GameAnimation*>(animations->items[id & ResourceIndexMask]) : nullptr;
    }

    if (animation == nullptr)
    {
        StopOgiAnimation(animator, static_cast<s32>(blendSeconds * g_ClockUnitsPerSecond), OgiAnimator::RootJoint);
        return;
    }

    AnimationSettings settings;
    ConstructAnimationSettings(1.0f, &settings, animation);
    settings.bits.loops = loops;
    settings.bits.backward = 0;
    settings.bits.queuedAlways = 1;
    if (0.0f < blendSeconds)
    {
        s32 ticks = static_cast<s32>(blendSeconds * g_ClockUnitsPerSecond);
        settings.blendTime = ticks;
        settings.bits.queued = ticks != 0;
    }

    PlayOgiAnimation(animator, &settings, OgiAnimator::RootJoint);
    DestroyAnimationSettings(&settings, DestroyOnly);
}

// A counter set to a value (or a random one below it): the game's, else the agent's, a designator's or every linked object's
void SetCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    s32 number = value.IntWith(node->PacketProperties());
    if (target.randomBelow)
    {
        number = RandomBelow(number);
    }

    u16 counter = target.counter;
    if (!target.agentCounter)
    {
        SetGameCounter(G_ChunkManager, counter, number);
        return;
    }

    if (!target.everyLinked)
    {
        *CounterOf(DesignatedNode(node, target.designator)->agent, counter) = static_cast<u8>(number);
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < attachments->LinkedCount(); index++)
    {
        InstanceContext* linked = attachments->linked[index];
        if (linked != nullptr)
        {
            *CounterOf(ObjectNodeOf(linked)->agent, counter) = static_cast<u8>(number);
        }
    }
}

// A counter changed by an amount, kept within 0 and 255 for an agent's: the game's, else the agent's, a designator's or those of
// the linked objects of an object (AnyObjectId any)
void ModifyCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    s32 amount = delta.IntWith(node->PacketProperties());
    u16 counter = target.counter;
    if (!target.agentCounter)
    {
        AddToGameCounter(G_ChunkManager, counter, amount);
        return;
    }

    if (!target.linkedObjects)
    {
        AddToCounter(CounterOf(DesignatedNode(node, target.designator)->agent, counter), amount);
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < attachments->LinkedCount(); index++)
    {
        InstanceContext* linked = attachments->linked[index];
        if (linked == nullptr)
        {
            continue;
        }

        ObjectNode* other = ObjectNodeOf(linked);
        if (other == nullptr)
        {
            continue;
        }

        Agent* agent = other->agent;
        u16 object = linkedObject.id;
        if (object != AnyObjectId && (agent->objectId & ResourceIndexMask) != object)
        {
            continue;
        }

        AddToCounter(CounterOf(agent, counter), amount);
    }
}

// A counter of the focus instance's agent changed by an amount, kept within 0 and 255
void AddToFocusCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    s32 change = amount.IntWith(node->PacketProperties());
    AddToCounter(CounterOf(ObjectNodeOf(focus)->agent, counter.counter), change);
}

// A counter of the focus instance's agent set to a value, or to a counter of the agent's (the source)
void SetFocusCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    s32 number = source == FromValue ? value.IntWith(node->PacketProperties()) : *CounterOf(node->agent, source);
    *CounterOf(ObjectNodeOf(focus)->agent, counter.counter) = static_cast<u8>(number);
}

// The focus position (or the stored one) made a target's position, then kept to some axes of the instance's frame and moved. The
// route's current step and the one before it are tuned by the route's values (and moved by the offset when it's given); for the
// other designators the position is alongPerception times the perception's direction from the instance, else awayFromPlayer
// away from the player, else a joint's, else the receiver's or the designator's, noise added when asked
void SetFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Axes = 3;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    u8 designator = target.designator;
    if (designator == DesignatesCurrentStep || designator == DesignatesNextStep)
    {
        RouteStepPosition(node->waypoints, &position, node, designator == DesignatesCurrentStep ? 1 : 0, routeCorner,
                          routeToward, routeScatter, routeSideways, routeLift);
        if (target.offsetGiven)
        {
            position.x = position.x + offsetX;
            position.y = position.y + offsetY;
            position.z = position.z + offsetZ;
        }
    }
    else
    {
        if (alongPerception != 0.0f)
        {
            if (node->perception != nullptr)
            {
                Vector4 direction = node->perception->direction;
                Vector4 own = PositionOf(node->owner);
                position.x = direction.x * alongPerception + own.x;
                position.y = direction.y * alongPerception + own.y;
                position.z = direction.z * alongPerception + own.z;
                position.w = 1.0f;
            }
        }
        else if (awayFromPlayer != 0.0f)
        {
            auto* player = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
            Vector4 own = PositionOf(node->owner);
            // A missing player's place is read at address 8
            Vector4 from = PositionOf(player);
            Vector4 away = {own.x - from.x, own.y - from.y, own.z - from.z, 1.0f};
            f32 inverse = InverseLength(&away, LengthEpsilon);
            position.x = own.x + away.x * inverse * awayFromPlayer;
            position.y = own.y + away.y * inverse * awayFromPlayer;
            position.z = own.z + away.z * inverse * awayFromPlayer;
            position.w = 1.0f;
        }
        else if (joint.id != GameOGI::NoJoint)
        {
            InstanceContext* jointOf = options.focusJoint ? node->AwakeFocus() : node->owner;
            JointPosition(jointOf, joint.id, &position, nullptr);
        }
        else
        {
            const Vector4* offset = target.offsetGiven ? VectorAt(&offsetX) : nullptr;
            DesignatedPosition(&position, target.space, runner, offset, designator, target.receiver, target.unused20);
        }

        if (target.noise)
        {
            JitterVector(noiseSpread, noiseUpScale, 1.0f, 1.0f, &position);
        }
    }

    if (target.keptAxes != 0)
    {
        Matrix4x4 frame;
        Vector4 origin;
        if (target.fromStart)
        {
            InstancePlacement* start = node->informationPointer;
            MatrixFromRotation(&frame, &start->rotation);
            origin = start->position;
        }
        else
        {
            ObjectPlace* place = instance->place;
            RotateAndTranslate(place);
            frame = place->matrix;
            origin = PositionOf(instance);
        }

        Vector4 offset = {position.x - origin.x, position.y - origin.y, position.z - origin.z, 1.0f};
        position = origin;
        for (u32 axis = 0; axis < Axes; axis++)
        {
            if ((target.keptAxes & 1u << axis) == 0)
            {
                continue;
            }

            const Vector4& along = RowOf(frame, axis);
            f32 dot = offset.x * along.x + offset.y * along.y + offset.z * along.z;
            position.z = position.z + along.z * dot;
            position.y = position.y + along.y * dot;
            position.x = position.x + along.x * dot;
        }

        if (target.fromStart && target.addsMove)
        {
            Vector4 current = PositionOf(instance);
            position.x = current.x + (position.x - origin.x);
            position.y = current.y + (position.y - origin.y);
            position.z = current.z + (position.z - origin.z);
            position.w = 1.0f;
        }
    }

    if (options.straightAhead)
    {
        ObjectPlace* place = instance->place;
        Vector4 own = PositionOf(instance);
        RotateAndTranslate(place);
        f32 x = position.x - own.x;
        f32 y = position.y - own.y;
        f32 z = position.z - own.z;
        const Vector4& forward = RowOf(place->matrix, 2);
        f32 distance = __builtin_sqrtf(x * x + y * y + z * z);
        position.x = own.x + forward.x * distance;
        position.y = own.y + forward.y * distance;
        position.z = own.z + forward.z * distance;
        position.w = 1.0f;
    }

    switch (target.slot)
    {
    case SlotFocus:
        SetFocusPosition(NodeOf(runner), position);
        break;
    case SlotStoredPosition:
        SetStoredPosition(NodeOf(runner), &position);
        break;
    default:
        break;
    }
}

// The focus position put a distance from the agent's instance square to the way from the player to it over the ground, on one
// side or the other by the side of the player's x axis the instance is on
void SetFocusPositionBesidePlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* player = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    ObjectNode* node = NodeOf(runner);
    Vector4 own = PositionOf(node->owner);
    // A missing player's place is read at address 8
    Vector4 from = PositionOf(player);
    Vector4 away = own;
    away.x = own.x - from.x;
    away.y = 0.0f;
    away.z = own.z - from.z;
    ObjectPlace* place = RetailPlaceOf(player);
    RotateAndTranslate(place);
    Vector4 side = RowOf(place->matrix, 0);
    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x = away.x * inverse;
    away.y = away.y * inverse;
    away.z = away.z * inverse;
    f32 facing = side.x * away.x + side.y * away.y + side.z * away.z;
    const Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 beside = {up.y * away.z - up.z * away.y, up.z * away.x - up.x * away.z, up.x * away.y - up.y * away.x, 1.0f};
    f32 scale = facing < 0.0f ? -distance : distance;
    beside.x = beside.x * scale;
    beside.y = beside.y * scale;
    beside.z = beside.z * scale;
    Vector4 position = {beside.x + own.x, beside.y + own.y, beside.z + own.z, beside.w};
    SetFocusPosition(node, position);
}

// The focus position (or the stored one) moved at random, by up to the spread times a scale along each axis
void AddNoiseToFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    f32 amount = spread.FloatWith(node->PacketProperties());
    u32 slot = moved.slot;
    if (slot == SlotFocus)
    {
        if (!node->flags.focusPosition)
        {
            return;
        }

        Vector4 position = node->focusPosition;
        JitterVector(amount, yScale, xScale, zScale, &position);
        SetFocusPosition(node, position);
    }
    else if (slot == SlotStoredPosition)
    {
        if (!node->flags.storedPosition)
        {
            return;
        }

        Vector4 position = node->storedPosition;
        JitterVector(amount, yScale, xScale, zScale, &position);
        SetStoredPosition(node, &position);
    }
}

namespace
{
// The instance an ID entry of the agent's instance links (none without an ID)
InstanceContext* LinkedById(const InstanceContext* owner)
{
    s32 id = owner->id;
    std::uintptr_t entries = reinterpret_cast<std::uintptr_t>(g_InstanceIds);
    std::uintptr_t entry = id != -1 ? entries + static_cast<u32>(id) * sizeof(InstanceIds::Entry) : 0;
    if (entry == 0)
    {
        return nullptr;
    }

    return *reinterpret_cast<InstanceContext* const*>(entry + offsetof(InstanceIds::Entry, spawner));
}
}

// A designator's instance (the parent, what the ID entry links, AgentRef2, AgentRef1, the focus, else a receiver's index) given to
// the focus, an agent reference or the stored position
void SetFocusToAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    u8 designator = target.designator;
    InstanceContext* instance;
    switch (designator)
    {
    case DesignatesParent:
        instance = NodeOf(runner)->owner->parent;
        break;
    case DesignatesLinkedById:
        instance = LinkedById(NodeOf(runner)->owner);
        break;
    case DesignatesAgentRef2:
        instance = AwakeAgentRef2(NodeOf(runner));
        break;
    case DesignatesAgentRef1:
        instance = AwakeAgentRef1(NodeOf(runner));
        break;
    case DesignatesFocus:
        instance = NodeOf(runner)->AwakeFocus();
        break;
    default:
        // Retail bug: every other designator (the agent's own 0xF0, the player 0xF2 and the rest) is taken for a receiver's index
        instance = ReceiverInstance(runner, designator);
        break;
    }

    if (instance != nullptr)
    {
        GiveInstance(NodeOf(runner), target.slot, instance);
    }
}

// A key's position (the current key, the next one (stepped to), else its index) of the agent's waypoints (the focus instance's,
// AgentRef1's, the source node's when there's one) made the focus position (or the stored one)
void SetFocusToKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* other = nullptr;
    if (target.focusKeys)
    {
        other = node->AwakeFocus();
    }
    else if (target.agentRef1Keys)
    {
        other = AwakeAgentRef1(node);
    }

    Waypoints* waypoints;
    if (other != nullptr)
    {
        waypoints = PacketNodeOf(other)->waypoints;
    }
    else if (node->sourceNode != nullptr)
    {
        waypoints = static_cast<ObjectNode*>(node->sourceNode)->waypoints;
    }
    else
    {
        waypoints = node->waypoints;
    }

    u8 key = target.key;
    LayoutPosition* keyPosition = nullptr;
    if (key == DesignatesCurrentKey || key == DesignatesNextKey)
    {
        if (key == DesignatesNextKey)
        {
            waypoints->NextKey();
        }

        keyPosition = waypoints->positions.data[waypoints->key];
    }
    else if (key < waypoints->keyCount)
    {
        keyPosition = waypoints->positions.data[key];
    }

    if (keyPosition == nullptr)
    {
        return;
    }

    Vector4 position = keyPosition->position;
    position.w = 1.0f;
    switch (target.slot)
    {
    case SlotFocus:
        SetFocusPosition(node, position);
        break;
    case SlotStoredPosition:
        SetStoredPosition(node, &position);
        break;
    default:
        break;
    }
}

// The last messenger's focus, agent references or stored position copied, which is forgotten without a messenger
void RequestMessengersFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u32 slot = copied.slot;
    InstanceContext* sender = node->messageSender;
    ObjectNode* other = sender != nullptr ? ObjectNodeOf(sender) : nullptr;
    if (other == nullptr)
    {
        ForgetDesignator(node, slot);
        return;
    }

    switch (slot)
    {
    case SlotFocus:
        node->focusPosition = other->focusPosition;
        node->flags.focusPosition = 0;
        node->flags.focusInstance = 0;
        if (other->flags.focusInstance)
        {
            node->flags.focusInstance = 1;
        }
        else if (other->flags.focusPosition)
        {
            node->flags.focusPosition = 1;
        }

        break;
    case SlotAgentRef1:
        node->agentRef1 = AwakeAgentRef1(other);
        break;
    case SlotAgentRef2:
        node->agentRef2 = AwakeAgentRef2(other);
        break;
    case SlotStoredPosition:
        if (other->flags.storedPosition)
        {
            Vector4 position = other->storedPosition;
            SetStoredPosition(node, &position);
        }

        break;
    default:
        break;
    }
}

// The last messenger given to the focus, an agent reference or the stored position, which is forgotten without one
void RequestMessSourceAsFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* sender = node->messageSender;
    u32 to = given.slot;
    if (sender == nullptr)
    {
        ForgetDesignator(node, to);
        return;
    }

    GiveInstance(node, to, sender);
}

// A linked object (of the focus instance, of AgentRef1, else of the agent's instance or of its source node's: the current one, the
// last or the one of the index) or else a receiver's or designator's instance (the index) given to the focus, AgentRef1 or the
// stored position
void SetFocusToLinkedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* whose;
    if (target.focusLinked)
    {
        whose = node->AwakeFocus();
    }
    else if (target.agentRef1Linked)
    {
        whose = AwakeAgentRef1(node);
    }
    else
    {
        whose = node->sourceNode != nullptr ? node->sourceNode->owner : node->owner;
    }

    u8 index = target.index;
    InstanceContext* found = nullptr;
    if (target.linked)
    {
        AttachmentsNode* attachments = AttachmentsNodeOf(whose);
        if (attachments == nullptr)
        {
            return;
        }

        if (target.current)
        {
            found = attachments->linked[CurrentLinked(attachments)];
        }
        else if (target.last)
        {
            // Without linked objects it's the 255th
            found = attachments->linked[static_cast<u8>(attachments->LinkedCount() - 1)];
        }
        else if (index < attachments->LinkedCount())
        {
            found = attachments->linked[index];
        }
    }
    else if (index < FirstDesignator)
    {
        if (runner->receivers == nullptr)
        {
            return;
        }

        found = ReceiverInstance(runner, index);
    }
    else
    {
        found = DesignatorOf(node, index);
    }

    if (found == nullptr)
    {
        return;
    }

    switch (target.slot)
    {
    case SlotFocus:
        SetFocusInstance(node, found);
        break;
    case SlotAgentRef1:
        node->agentRef1 = found;
        break;
    case SlotStoredPosition:
    {
        Vector4 position = PositionOf(found);
        SetStoredPosition(node, &position);
        break;
    }
    default:
        break;
    }
}

// A designator given the place a distance from one designator's position towards another's; the agent's own position when the
// first has none in OwnPositionMode
void SetFocusPositionAlongCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 from = designators.from;
    u8 to = designators.to;
    Vector4 start;
    InstanceContext* fromInstance = DesignatorOf(node, from);
    if (fromInstance != nullptr)
    {
        start = PositionOf(fromInstance);
    }
    else if (!DesignatorPosition(node, from, &start) && designators.mode == OwnPositionMode)
    {
        start = PositionOf(node->owner);
    }

    // Retail bug: a start or an end the designators have no position of is what the stack held
    Vector4 end;
    InstanceContext* toInstance = DesignatorOf(node, to);
    if (toInstance != nullptr)
    {
        end = PositionOf(toInstance);
    }
    else
    {
        DesignatorPosition(node, to, &end);
    }

    Vector4 way = {end.x - start.x, end.y - start.y, end.z - start.z, 1.0f};
    f32 inverse = InverseLength(&way, LengthEpsilon);
    f32 x = way.x * inverse * distance;
    f32 y = way.y * inverse * distance;
    f32 z = way.z * inverse * distance;
    Vector4 position = {start.x + x, start.y + y, start.z + z, 1.0f};
    CallVirtual<u32>(node, node->vtable, ObjectNode::SetDesignatorPositionSlot, designators.destination, &position);
}

// The focus position (or the origin) with the coordinates given, then, with a distance, put that far from the agent's instance over
// the ground the way the position is (keeping its height) and moved by the offsets across that way, up and back along it;
// without a distance, moved by the offsets along x, y and z
void SetFocusPositionOffsetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    if (node->flags.focusPosition)
    {
        position = node->focusPosition;
    }

    if (uses.x)
    {
        position.x = x;
    }

    if (uses.y)
    {
        position.y = y;
    }

    if (uses.z)
    {
        position.z = z;
    }

    if (0.0f < distance)
    {
        Vector4 own = PositionOf(NodeOf(runner)->owner);
        own.y = position.y;
        f32 zero = 0.0f;
        Vector4 way = {position.x - own.x, zero, position.z - own.z, 1.0f};
        f32 inverse = InverseLength(&way, LengthEpsilon);
        way.x = way.x * inverse;
        way.y = way.y * inverse;
        way.z = way.z * inverse;
        position.x = own.x + way.x * distance;
        position.y = own.y + way.y * distance;
        position.z = own.z + way.z * distance;
        position.w = 1.0f;
        if (uses.offsetX)
        {
            const Vector4 up = {zero, 1.0f, zero, 1.0f};
            Vector4 side = {up.y * way.z - up.z * way.y, up.z * way.x - up.x * way.z, up.x * way.y - up.y * way.x, 1.0f};
            f32 scale = -offsetX;
            position.x = position.x + side.x * scale;
            position.y = position.y + side.y * scale;
            position.z = position.z + side.z * scale;
        }

        if (uses.offsetY)
        {
            position.y = position.y + offsetY;
        }

        if (uses.offsetZ)
        {
            f32 scale = -offsetZ;
            position.x = position.x + way.x * scale;
            position.y = position.y + way.y * scale;
            position.z = position.z + way.z * scale;
        }
    }
    else
    {
        if (uses.offsetX)
        {
            position.x = position.x + offsetX;
        }

        if (uses.offsetY)
        {
            position.y = position.y + offsetY;
        }

        if (uses.offsetZ)
        {
            position.z = position.z + offsetZ;
        }
    }

    SetFocusPosition(NodeOf(runner), position);
}

// The stored position set to the focus position (or the origin) moved a distance along the way from the agent's instance to it
// over the ground, turned about y by an angle
void SetStoredPositionAtAngleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    if (node->flags.focusPosition)
    {
        position = node->focusPosition;
    }

    Vector4 own = PositionOf(NodeOf(runner)->owner);
    f32 zero = 0.0f;
    Vector4 way = {position.x - own.x, zero, position.z - own.z, 1.0f};
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    s32 turnAngle;
    AngleFrom(&turnAngle, angle, AngleDegrees);
    s32 turn = turnAngle;
    const Vector4 up = {zero, 1.0f, zero, 1.0f};
    TurnAboutAxis(&way, &up, &turn, 1);
    position.x = position.x + way.x * distance;
    position.y = position.y + way.y * distance;
    position.z = position.z + way.z * distance;
    SetStoredPosition(node, &position);
}

void NowTurnLeftCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TurnBy(runner, &turnRate, true);
}

void NowTurnRightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TurnBy(runner, &turnRate, false);
}

// The agent's instance warped to its target, or to its last contact's point moved by the offset in its own frame (1.5 above the
// instance without a contact); turning toward it instead when asked
void PositionWarpCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 AboveWithoutContact = 1.5f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOrAhead(instance, target.turns);
    if (target.toContact)
    {
        position = node->waterPoint;
        if (__builtin_fabsf(position.x) <= Epsilon && __builtin_fabsf(position.y) <= Epsilon &&
            __builtin_fabsf(position.z) <= Epsilon)
        {
            position = PositionOf(instance);
            position.y = position.y + AboveWithoutContact;
        }
        else if (target.offsetGiven)
        {
            ObjectPlace* place = instance->place;
            RotateAndTranslate(place);
            const f32 (*m)[4] = place->matrix.m;
            f32 dx = m[0][0] * offsetX + m[1][0] * offsetY + m[2][0] * offsetZ;
            f32 dy = m[0][1] * offsetX + m[1][1] * offsetY + m[2][1] * offsetZ;
            f32 dz = m[0][2] * offsetX + m[1][2] * offsetY + m[2][2] * offsetZ;
            position.z = position.z + dz;
            position.x = position.x + dx;
            position.y = position.y + dy;
        }
    }
    else
    {
        const Vector4* offset = target.offsetGiven ? VectorAt(&offsetX) : nullptr;
        DesignatedPosition(&position, target.space, runner, offset, target.designator, target.receiver, target.unused20);
    }

    WarpTo(node, instance, position, target.turns, target.turnsBody);
}

// An agent's instance warped like PositionWarp does to a target, or to the position of the source designator (its instance's or
// its own) when there's one; its agent told after
void WarpAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOrAhead(instance, warp.turns);
    u8 from = source.designator;
    if (from != DesignatesNone)
    {
        InstanceContext* fromInstance = DesignatorOf(node, from);
        if (fromInstance != nullptr)
        {
            position = PositionOf(fromInstance);
        }
        else
        {
            DesignatorPosition(node, from, &position);
        }
    }
    else
    {
        const Vector4* offset = warp.offsetGiven ? VectorAt(&offsetX) : nullptr;
        DesignatedPosition(&position, warp.space, runner, offset, warp.designator, warp.receiver, warp.unused20);
    }

    ObjectNode* warped = node;
    u8 agent = warp.agent;
    if (agent != DesignatesNone)
    {
        // Retail bug: a designator without an instance warps none (its place read at address 8) with the agent's own node
        instance = DesignatorOf(node, agent);
        if (instance != nullptr)
        {
            warped = ObjectNodeOf(instance);
        }
    }

    WarpTo(warped, instance, position, warp.turns, warp.turnsBody);
    Agent* told = warped->agent;
    CallVirtual<void>(told, told->vtable, Agent::RecoverSlot);
}
