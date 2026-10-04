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
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
// The object nodes' vtable functions: a behaviour started (a starter, its originator, forced, the runner's slot), a designator
// forgotten, a designator's instance and position, a position and an instance given to a designator (whether it took it), no
// packet runs
constexpr u32 StartBehaviourSlot = 18;
constexpr u32 ForgetDesignatorSlot = 34;
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 GetDesignatorPositionSlot = 37;
constexpr u32 SetDesignatorPositionSlot = 38;
constexpr u32 SetDesignatorSlot = 39;
constexpr u32 NoPacketSlot = 42;
// The agents' vtable function 13, which the warp calls once it's done (the base's does nothing)
constexpr u32 AgentWarpedSlot = 13;
constexpr u8 NoDesignator = 0xFF;
// The designators the focus commands answer besides the node's: the instance it hangs from; the ones below the first designator
// are the starter's receivers
constexpr u8 DesignatesParent = 0xDE;
constexpr u8 FirstDesignator = 0xDE;
constexpr u8 DesignatesRouteStep = 0xEF;
constexpr u16 NoBehaviour = 0xFFFF;
constexpr u16 NoAnimation = 0xFFFF;
constexpr u16 NoModel = 0xFFFF;
constexpr u16 AnyObject = 0xFFFF;
constexpr u32 ResourceIndexMask = 0x7FFF;
// What the designator commands give an instance or a position to: the focus, AgentRef1, AgentRef2, the stored position
constexpr u32 ToFocus = 0;
constexpr u32 ToAgentRef1 = 1;
constexpr u32 ToAgentRef2 = 2;
constexpr u32 ToStoredPosition = 3;
constexpr u32 DesignatorSlotMask = 0x7;
// The attachments node's word: the linked objects' count (bits 0-4) and the current one (bits 7-11), the instances after it
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;
// The messages' kinds of nodes
constexpr u32 MessageKinds = 2;
constexpr u16 NoStarter = 0xFFFF;
// The squared lengths too short to have a direction
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// The counters are bytes
constexpr s32 CounterMax = 0xFF;
// How far an instance warped by turning leans into the turn at most (degrees)
constexpr f32 MostLean = 90.0f;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

ObjectNode* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
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

InstanceContext* DesignatorOf(ObjectNode* node, u32 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, designator);
}

bool DesignatorPosition(ObjectNode* node, u32 designator, Vector4* position)
{
    return CallVirtual<u32>(node, node->vtable, GetDesignatorPositionSlot, designator, position) != 0;
}

void ForgetDesignator(ObjectNode* node, u32 slot)
{
    CallVirtual<void>(node, node->vtable, ForgetDesignatorSlot, slot);
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
    node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
}

void SetFocusPosition(ObjectNode* node, const Vector4& position)
{
    node->focusPosition = position;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusPosition) & ~ObjectNodeBase::FlagFocusInstance;
}

// An instance given to one of the node's designators: the focus, an agent reference or the stored position (its position)
void GiveInstance(ObjectNode* node, u32 slot, InstanceContext* instance)
{
    switch (slot)
    {
    case ToFocus:
        SetFocusInstance(node, instance);
        break;
    case ToAgentRef1:
        node->agentRef1 = instance;
        break;
    case ToAgentRef2:
        node->agentRef2 = instance;
        break;
    case ToStoredPosition:
    {
        Vector4 position = PositionOf(instance);
        SetStoredPosition(node, &position);
        break;
    }
    default:
        break;
    }
}

u32* LinkedWord(void* attachments)
{
    return reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18);
}

InstanceContext** LinkedInstances(void* attachments)
{
    return reinterpret_cast<InstanceContext**>(static_cast<u8*>(attachments) + 0x20);
}

u32 LinkedCount(void* attachments)
{
    return *LinkedWord(attachments) & LinkedCountMask;
}

u32 CurrentLinked(void* attachments)
{
    return *LinkedWord(attachments) >> LinkedIndexShift & LinkedIndexMask;
}

// A counter of an agent's (its bytes 0x18 on)
u8* CounterOf(Agent* agent, s32 counter)
{
    return reinterpret_cast<u8*>(agent) + offsetof(Agent, unknown18) + counter;
}

void AddToCounter(u8* counter, s32 amount)
{
    s32 value = *counter + amount;
    if (value > CounterMax)
    {
        *counter = CounterMax;
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
    if (designator == NoDesignator)
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

f32 FloatOf(const s32& value)
{
    return __builtin_bit_cast(f32, value);
}

// The instance turned about y by an angle times the frame's seconds
void TurnBy(BehaviourRunner* runner, const TaggedValue* angleValue, bool left)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    TaggedValue angle;
    TaggedValue::AngleWith(&angle, angleValue, node->PacketProperties());
    f32 rate = left ? -g_FrameSeconds : g_FrameSeconds;
    s32 turn = static_cast<s32>(static_cast<f32>(angle.raw) * rate);
    ObjectPlace* place = instance->place;
    if (turn == 0)
    {
        return;
    }

    place->SyncRotation();
    place->bits = (place->bits | ObjectPlace::BitTurned) & ~u64{ObjectPlace::BitMatrixTurned};
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
            SteerTowards(1.0f, 0.0f, MostLean, instance, &position);
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

        node->unknownB0 = position;
    }
}
}

// The starter the runner runs started again on its instance by an event (forced, in the runner's slot, the instance its
// originator), once the level finished and the node was told no packet runs
void RestartPreviousCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    GameNode* node = runner->agentNode;
    InstanceContext* instance = node->owner;
    u16 starter = runner->unknown24;
    level->bits |= BehaviourLevel::Finished;
    CallVirtual<void>(node, node->vtable, NoPacketSlot, runner);
    if (starter == NoStarter)
    {
        return;
    }

    u16 index = starter;
    Reference* argument = instance != nullptr ? AddReference(instance) : nullptr;
    ScriptEvent* event = ScriptEvent::Construct(static_cast<ScriptEvent*>(MemoryAllocate(sizeof(ScriptEvent))), &index,
                                                runner->flags >> BehaviourRunner::SlotShift & BehaviourRunner::SlotMask, 1,
                                                &argument, instance, MessageKinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(instance, &handle);
}

// A designator given what another designates, an instance or else its position: the designators' bytes are the source (0), the
// destination (1) and the agent whose designators the source is (2, 0xF0 its own)
void SetFocusPositionToAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u8 source = static_cast<u8>(targets);
    u8 destination = static_cast<u8>(targets >> 8);
    u8 agent = static_cast<u8>(targets >> 16);
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

    if (CallVirtual<u32>(node, node->vtable, SetDesignatorSlot, u32{destination}, instance) == 0)
    {
        CallVirtual<u32>(node, node->vtable, SetDesignatorPositionSlot, u32{destination}, &position);
    }
}

namespace
{
// The instance let go of by the attachments of another (passing bit 11 of the command's word on)
void UnlinkFrom(InstanceContext* other, InstanceContext* instance, u32 target)
{
    constexpr u32 FlagShift = 11;
    if (other == nullptr)
    {
        return;
    }

    void* attachments = GetGameNode(&other->nodes, AttachmentsKind);
    if (attachments != nullptr)
    {
        UnlinkInstance(attachments, instance, 0, 1, target >> FlagShift & 1);
    }
}
}

// The agent's instance let go of by what links it: a designator's instance (0-7, 0xFF none) or, by the agent's own linked
// objects, every one (bit 8), the current one (bit 10) or the one of the index in bits 0-7 (bit 9); bit 11 goes to the unlinking
void UnlinkFromTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 EveryLinked = 0x100;
    constexpr u32 ByIndex = 0x200;
    constexpr u32 Current = 0x400;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u8 designator = static_cast<u8>(target);
    if (designator != NoDesignator && (target & ByIndex) == 0)
    {
        UnlinkFrom(DesignatorOf(node, designator), instance, target);
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    if ((target & EveryLinked) != 0)
    {
        u32* word = LinkedWord(attachments);
        if ((*word & LinkedCountMask) == 0)
        {
            return;
        }

        *word &= ~(LinkedIndexMask << LinkedIndexShift);
        while (true)
        {
            UnlinkFrom(LinkedInstances(attachments)[CurrentLinked(attachments)], instance, target);
            u32 index = CurrentLinked(attachments);
            if (index == (*word & LinkedCountMask) - 1)
            {
                return;
            }

            *word = (*word & ~(LinkedIndexMask << LinkedIndexShift)) | ((index + 1) & LinkedIndexMask) << LinkedIndexShift;
        }
    }

    InstanceContext* linked;
    if ((target & Current) != 0)
    {
        linked = LinkedInstances(attachments)[CurrentLinked(attachments)];
    }
    else if ((target & ByIndex) != 0)
    {
        linked = LinkedInstances(attachments)[designator];
    }
    else
    {
        return;
    }

    UnlinkFrom(linked, instance, target);
}

namespace
{
// A starter started on an instance's object node, forced, the instance its originator, in the runner of bit 16 (the node made
// the one it takes its object from when bit 18 says so)
void RunOn(ObjectNode* node, ObjectNode* other, InstanceContext* instance, ScriptStarter* starter, u32 slotAndFlags)
{
    constexpr u32 TakesSource = 0x40000;
    if ((slotAndFlags & TakesSource) != 0)
    {
        other->sourceNode = node;
    }

    CallVirtual<u32>(other, other->vtable, StartBehaviourSlot, starter, instance, 1u, slotAndFlags >> 16 & 1);
}
}

// The behaviour of the object's slot (bits 0-15) run on a designator's instance (0xFF none), else on every linked object (bit 17)
// or on the linked objects of an object (the high half of targetAndObject)
void RunSlotBehaviourOnLinkedCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 EveryLinked = 0x20000;
    // Retail takes an object ID of 0xFF for none (the IDs' none is 0xFFFF, which finds nothing)
    constexpr u16 NoObject = 0xFF;
    ObjectNode* node = NodeOf(runner);
    u16 id;
    GetObjectBehaviourId(&id, ObjectOf(node), static_cast<u16>(slotAndFlags));
    ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
    auto* starter = id != NoBehaviour ? static_cast<ScriptStarter*>(scripts->items[id & ResourceIndexMask]) : nullptr;
    if (starter == nullptr)
    {
        return;
    }

    u8 designator = static_cast<u8>(targetAndObject);
    if (designator != NoDesignator)
    {
        InstanceContext* target = DesignatorOf(node, designator);
        if (target != nullptr)
        {
            RunOn(node, ObjectNodeOf(target), target, starter, slotAndFlags);
        }

        return;
    }

    bool every = (slotAndFlags & EveryLinked) != 0;
    if (!every && static_cast<u16>(targetAndObject >> 16) == NoObject)
    {
        return;
    }

    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < LinkedCount(attachments); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
        if (linked == nullptr)
        {
            continue;
        }

        ObjectNode* other = ObjectNodeOf(linked);
        if (!every && (other->agent->objectId & ResourceIndexMask) != static_cast<u16>(targetAndObject >> 16))
        {
            continue;
        }

        RunOn(node, other, linked, starter, slotAndFlags);
    }
}

void PlaySlotAnimation(f32 blendSeconds, ObjectNode* node, u32 slot, u32 loops)
{
    u16 objectSlot = static_cast<u16>(slot);
    GameObject* object = node->object;
    InstanceContext* instance = node->owner;
    u16 modelId;
    GetObjectModelId(&modelId, object, objectSlot);
    if (modelId == NoModel)
    {
        return;
    }

    GameResources* resources = G_GameResourcesObjectPointer;
    u16 id = modelId;
    auto* ogi = id != NoModel ? static_cast<GameOGI*>(resources->models->items[id & ResourceIndexMask]) : nullptr;
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    u16 animationId;
    GetObjectAnimationId(&animationId, object, objectSlot);
    u32 header = object->header[0];
    model->SetOgi(ogi, header >> 6 & 0x3F, header & 0x3F);
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return;
    }

    constexpr u32 AllJoints = 0xFF;
    GameAnimation* animation = nullptr;
    if (animationId != NoAnimation)
    {
        id = animationId;
        ResourceTable* animations = resources->animations;
        animation = id != NoAnimation ? static_cast<GameAnimation*>(animations->items[id & ResourceIndexMask]) : nullptr;
    }

    if (animation == nullptr)
    {
        StopOgiAnimation(animator, static_cast<s32>(blendSeconds * g_ClockUnitsPerSecond), AllJoints);
        return;
    }

    AnimationSettings settings;
    ConstructAnimationSettings(1.0f, &settings, animation);
    u32 bits = (settings.bits & ~AnimationSettings::Loops) | (loops & AnimationSettings::Loops);
    bits = (bits & ~AnimationSettings::Backward) | AnimationSettings::QueuedAlways;
    settings.bits = bits;
    if (0.0f < blendSeconds)
    {
        s32 ticks = static_cast<s32>(blendSeconds * g_ClockUnitsPerSecond);
        settings.blendTime = ticks;
        settings.bits = (bits & ~AnimationSettings::Queued) | static_cast<u32>(ticks != 0) << 1;
    }

    PlayOgiAnimation(animator, &settings, AllJoints);
    DestroyAnimationSettings(&settings, DestroyOnly);
}

// A counter (bits 0-15) set to a value (a random one below it with bit 25): the game's, else (bit 24) the agent's, a designator's
// (bits 16-23, 0xFF none) or every linked object's (bit 26)
void SetCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 AgentCounter = 0x1000000;
    constexpr u32 RandomBelowValue = 0x2000000;
    constexpr u32 EveryLinked = 0x4000000;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    s32 number = value.IntWith(node->PacketProperties());
    if ((counterTarget & RandomBelowValue) != 0)
    {
        number = RandomBelow(number);
    }

    u16 counter = static_cast<u16>(counterTarget);
    if ((counterTarget & AgentCounter) == 0)
    {
        SetGameCounter(G_ChunkManager, counter, number);
        return;
    }

    if ((counterTarget & EveryLinked) == 0)
    {
        *CounterOf(DesignatedNode(node, static_cast<u8>(counterTarget >> 16))->agent, counter) = static_cast<u8>(number);
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < LinkedCount(attachments); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
        if (linked != nullptr)
        {
            *CounterOf(ObjectNodeOf(linked)->agent, counter) = static_cast<u8>(number);
        }
    }
}

// A counter (bits 0-15) changed by an amount, kept within 0 and 255 for an agent's: the game's, else (bit 24) the agent's, a
// designator's (bits 16-23, 0xFF none) or those of the linked objects of an object (bit 25: unknown3's low half, 0xFFFF any)
void ModifyCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 AgentCounter = 0x1000000;
    constexpr u32 EveryLinked = 0x2000000;
    ObjectNode* node = NodeOf(runner);
    s32 amount = delta.IntWith(node->PacketProperties());
    u16 counter = static_cast<u16>(counterTarget);
    if ((counterTarget & AgentCounter) == 0)
    {
        AddToGameCounter(G_ChunkManager, counter, amount);
        return;
    }

    if ((counterTarget & EveryLinked) == 0)
    {
        AddToCounter(CounterOf(DesignatedNode(node, static_cast<u8>(counterTarget >> 16))->agent, counter), amount);
        return;
    }

    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < LinkedCount(attachments); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
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
        u16 object = static_cast<u16>(unknown3);
        if (object != AnyObject && (agent->objectId & ResourceIndexMask) != object)
        {
            continue;
        }

        AddToCounter(CounterOf(agent, counter), amount);
    }
}

// A counter of the focus instance's agent (the index's low byte) changed by an amount, kept within 0 and 255
void AddToFocusObjectByteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    s32 amount = value.IntWith(node->PacketProperties());
    AddToCounter(CounterOf(ObjectNodeOf(focus)->agent, static_cast<u8>(index)), amount);
}

// A counter of the focus instance's agent (the index's low byte) set to a value, or (source not 0xFF) to a counter of the agent's
void SetFocusObjectByteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr s32 FromValue = 0xFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    s32 number = source == FromValue ? value.IntWith(node->PacketProperties()) : *CounterOf(node->agent, source);
    *CounterOf(ObjectNodeOf(focus)->agent, static_cast<u8>(index)) = static_cast<u8>(number);
}

// The focus position (or, with bits 23-25 3, the stored one) made a target's position, then kept to some axes and moved.
// targetFlags: the receiver (bits 0-7), the designator (8-15: the route's current step (0xFA) and 0xEF take value10, value16,
// value11, value14 and value15 tuning them), the space (16-19), bit 20 (passed on), the offset given (21), noise (22:
// value8 the spread, scale the up's), the axes of the instance's frame (its start's with bit 29) the position keeps (26-28: x, y,
// z), the instance's move since its start added (29 and 31). For the other designators the position is value12 times the
// perception's direction from the instance, else value13 away from the player, else a joint's (keyAndObject's low byte, 0xFF
// none; the focus instance's with angleValue's bit 0), else the receiver's or designator's; angleValue's bit 1 puts it straight
// ahead of the instance as far away
void SetFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OffsetGiven = 0x200000;
    constexpr u32 Noise = 0x400000;
    constexpr u32 ModeShift = 23;
    constexpr u32 ModeMask = 0x7;
    constexpr u32 KeepX = 0x4000000;
    constexpr u32 KeepY = 0x8000000;
    constexpr u32 KeepZ = 0x10000000;
    constexpr u32 FromStart = 0x20000000;
    constexpr u32 MovedSinceStart = FromStart | 0x80000000;
    constexpr u32 FocusJoint = 0x1;
    constexpr u32 Ahead = 0x2;
    constexpr u8 NoJoint = 0xFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    u8 designator = static_cast<u8>(targetFlags >> 8);
    if (designator == DesignatesCurrentStep || designator == DesignatesRouteStep)
    {
        RouteStepPosition(node->waypoints, &position, node, designator == DesignatesCurrentStep ? 1 : 0, value10,
                          FloatOf(value16), value11, FloatOf(value14), FloatOf(value15));
        if ((targetFlags & OffsetGiven) != 0)
        {
            position.x = position.x + offsetX;
            position.y = position.y + offsetY;
            position.z = position.z + offsetZ;
        }
    }
    else
    {
        if (value12 != 0.0f)
        {
            if (node->perception != nullptr)
            {
                Vector4 direction = *reinterpret_cast<const Vector4*>(static_cast<u8*>(node->perception) + 0x70);
                Vector4 own = PositionOf(node->owner);
                position.x = direction.x * value12 + own.x;
                position.y = direction.y * value12 + own.y;
                position.z = direction.z * value12 + own.z;
                position.w = 1.0f;
            }
        }
        else if (value13 != 0.0f)
        {
            auto* player = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
            Vector4 own = PositionOf(node->owner);
            // A missing player's place is read at address 8
            Vector4 from = PositionOf(player);
            Vector4 away = {own.x - from.x, own.y - from.y, own.z - from.z, 1.0f};
            f32 inverse = InverseLength(&away, LengthEpsilon);
            position.x = own.x + away.x * inverse * value13;
            position.y = own.y + away.y * inverse * value13;
            position.z = own.z + away.z * inverse * value13;
            position.w = 1.0f;
        }
        else if (static_cast<u8>(keyAndObject) != NoJoint)
        {
            InstanceContext* jointOf = (angleValue & FocusJoint) != 0 ? node->AwakeFocus() : node->owner;
            JointPosition(jointOf, static_cast<u8>(keyAndObject), &position, nullptr);
        }
        else
        {
            const Vector4* offset = (targetFlags & OffsetGiven) != 0 ? VectorAt(&offsetX) : nullptr;
            DesignatedPosition(&position, targetFlags >> 16 & 0xF, runner, offset, designator, static_cast<u8>(targetFlags),
                               targetFlags >> 20 & 1);
        }

        if ((targetFlags & Noise) != 0)
        {
            JitterVector(value8, scale, 1.0f, 1.0f, &position);
        }
    }

    if ((targetFlags & (KeepX | KeepY | KeepZ)) != 0)
    {
        Matrix4x4 frame;
        Vector4 origin;
        if ((targetFlags & FromStart) != 0)
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
        const u32 keeps[3] = {KeepX, KeepY, KeepZ};
        for (u32 axis = 0; axis < 3; axis++)
        {
            if ((targetFlags & keeps[axis]) == 0)
            {
                continue;
            }

            const Vector4& along = RowOf(frame, axis);
            f32 dot = offset.x * along.x + offset.y * along.y + offset.z * along.z;
            position.z = position.z + along.z * dot;
            position.y = position.y + along.y * dot;
            position.x = position.x + along.x * dot;
        }

        if ((targetFlags & MovedSinceStart) == MovedSinceStart)
        {
            Vector4 current = PositionOf(instance);
            position.x = current.x + (position.x - origin.x);
            position.y = current.y + (position.y - origin.y);
            position.z = current.z + (position.z - origin.z);
            position.w = 1.0f;
        }
    }

    if ((angleValue & Ahead) != 0)
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

    switch (targetFlags >> ModeShift & ModeMask)
    {
    case 0:
        SetFocusPosition(NodeOf(runner), position);
        break;
    case ToStoredPosition:
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

// The focus position (or the stored one: flags' bits 0-2 3) moved at random, by up to an amount times factorA along x, factorB
// along y and factorC along z
void AddNoiseToFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    f32 spread = amount.FloatWith(node->PacketProperties());
    u32 mode = static_cast<u16>(flags) & DesignatorSlotMask;
    if (mode == ToFocus)
    {
        if ((node->flags & ObjectNodeBase::FlagFocusPosition) == 0)
        {
            return;
        }

        Vector4 position = node->focusPosition;
        JitterVector(spread, factorB, factorA, factorC, &position);
        SetFocusPosition(node, position);
    }
    else if (mode == ToStoredPosition)
    {
        if ((node->flags & ObjectNodeBase::FlagStoredPosition) == 0)
        {
            return;
        }

        Vector4 position = node->storedPosition;
        JitterVector(spread, factorB, factorA, factorC, &position);
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

    return *reinterpret_cast<InstanceContext* const*>(entry + offsetof(InstanceIds::Entry, unknown04));
}
}

// A designator's instance (bits 0-7: the parent, what the ID entry links, AgentRef2, AgentRef1, the focus, else a receiver's
// index) given to the focus, an agent reference or the stored position (bits 8-10)
void SetFocusToAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    u8 designator = static_cast<u8>(targetAndSlot);
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
        GiveInstance(NodeOf(runner), targetAndSlot >> 8 & DesignatorSlotMask, instance);
    }
}

// A key's position (bits 0-7: the current key, the next one (stepped to), else its index) of the agent's waypoints (the focus
// instance's with bit 12, AgentRef1's with bit 13, the source node's when there's one) made the focus position (or the stored
// one: bits 9-11 3)
void SetFocusToKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FocusKeys = 0x1000;
    constexpr u32 AgentRef1Keys = 0x2000;
    constexpr u32 ModeShift = 9;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* other = nullptr;
    if ((keyAndFlags & FocusKeys) != 0)
    {
        other = node->AwakeFocus();
    }
    else if ((keyAndFlags & AgentRef1Keys) != 0)
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

    u8 key = static_cast<u8>(keyAndFlags);
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
    switch (keyAndFlags >> ModeShift & DesignatorSlotMask)
    {
    case ToFocus:
        SetFocusPosition(node, position);
        break;
    case ToStoredPosition:
        SetStoredPosition(node, &position);
        break;
    default:
        break;
    }
}

// The last messenger's focus, agent references or stored position (bits 0-2) copied, which is forgotten without a messenger
void RequestMessengersFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    u32 slot = static_cast<u16>(value1) & DesignatorSlotMask;
    InstanceContext* sender = node->messageSender;
    ObjectNode* other = sender != nullptr ? ObjectNodeOf(sender) : nullptr;
    if (other == nullptr)
    {
        ForgetDesignator(node, slot);
        return;
    }

    switch (slot)
    {
    case ToFocus:
        node->focusPosition = other->focusPosition;
        node->flags &= ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
        if ((other->flags & ObjectNodeBase::FlagFocusInstance) != 0)
        {
            node->flags |= ObjectNodeBase::FlagFocusInstance;
        }
        else if ((other->flags & ObjectNodeBase::FlagFocusPosition) != 0)
        {
            node->flags |= ObjectNodeBase::FlagFocusPosition;
        }

        break;
    case ToAgentRef1:
        node->agentRef1 = AwakeAgentRef1(other);
        break;
    case ToAgentRef2:
        node->agentRef2 = AwakeAgentRef2(other);
        break;
    case ToStoredPosition:
        if ((other->flags & ObjectNodeBase::FlagStoredPosition) != 0)
        {
            Vector4 position = other->storedPosition;
            SetStoredPosition(node, &position);
        }

        break;
    default:
        break;
    }
}

// The last messenger given to the focus, an agent reference or the stored position (bits 0-2), which is forgotten without one
void RequestMessSourceAsFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* sender = node->messageSender;
    u32 to = static_cast<u16>(slot) & DesignatorSlotMask;
    if (sender == nullptr)
    {
        ForgetDesignator(node, to);
        return;
    }

    GiveInstance(node, to, sender);
}

// A linked object (bit 8: of the focus instance with bit 10, of AgentRef1 with bit 11, else of the agent's instance or of its
// source node's: the current one (bit 9), the last (bit 15) or the index in bits 0-7) or else a receiver's or designator's
// instance (bits 0-7) given to the focus, AgentRef1 or the stored position (bits 12-14)
void SetFocusToLinkedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Linked = 0x100;
    constexpr u32 Current = 0x200;
    constexpr u32 FocusLinked = 0x400;
    constexpr u32 AgentRef1Linked = 0x800;
    constexpr u32 Last = 0x8000;
    constexpr u32 SlotShift = 12;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* whose;
    if ((target & FocusLinked) != 0)
    {
        whose = node->AwakeFocus();
    }
    else if ((target & AgentRef1Linked) != 0)
    {
        whose = AwakeAgentRef1(node);
    }
    else
    {
        whose = node->sourceNode != nullptr ? node->sourceNode->owner : node->owner;
    }

    u8 index = static_cast<u8>(target);
    InstanceContext* found = nullptr;
    if ((target & Linked) != 0)
    {
        void* attachments = GetGameNode(&whose->nodes, AttachmentsKind);
        if (attachments == nullptr)
        {
            return;
        }

        if ((target & Current) != 0)
        {
            found = LinkedInstances(attachments)[CurrentLinked(attachments)];
        }
        else if ((target & Last) != 0)
        {
            // Without linked objects it's the 255th
            found = LinkedInstances(attachments)[static_cast<u8>(LinkedCount(attachments) - 1)];
        }
        else if (index < LinkedCount(attachments))
        {
            found = LinkedInstances(attachments)[index];
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

    switch (target >> SlotShift & DesignatorSlotMask)
    {
    case ToFocus:
        SetFocusInstance(node, found);
        break;
    case ToAgentRef1:
        node->agentRef1 = found;
        break;
    case ToStoredPosition:
    {
        Vector4 position = PositionOf(found);
        SetStoredPosition(node, &position);
        break;
    }
    default:
        break;
    }
}

// A designator (byte 0) given the place a distance from one designator's position (byte 1) towards another's (byte 2); the
// agent's own position when the first has none and byte 3's low half is 2
void SetFocusPositionAlongCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OwnPositionMode = 2;
    ObjectNode* node = NodeOf(runner);
    u8 from = static_cast<u8>(targets >> 8);
    u8 to = static_cast<u8>(targets >> 16);
    Vector4 start;
    InstanceContext* fromInstance = DesignatorOf(node, from);
    if (fromInstance != nullptr)
    {
        start = PositionOf(fromInstance);
    }
    else if (!DesignatorPosition(node, from, &start) && (targets >> 24 & 0xF) == OwnPositionMode)
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
    CallVirtual<u32>(node, node->vtable, SetDesignatorPositionSlot, static_cast<u32>(static_cast<u8>(targets)), &position);
}

// The focus position (or the origin) with the coordinates given (use's bits 0-2), then, with a distance, put that far from the
// agent's instance over the ground the way the position is (keeping its height) and moved by the offsets across that way (bit
// 3), up (bit 4) and back along it (bit 5); without a distance, moved by the offsets along x, y and z
void SetFocusPositionOffsetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SetsX = 0x1;
    constexpr u32 SetsY = 0x2;
    constexpr u32 SetsZ = 0x4;
    constexpr u32 OffsetX = 0x8;
    constexpr u32 OffsetY = 0x10;
    constexpr u32 OffsetZ = 0x20;
    ObjectNode* node = NodeOf(runner);
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    if ((node->flags & ObjectNodeBase::FlagFocusPosition) != 0)
    {
        position = node->focusPosition;
    }

    if ((use.raw & SetsX) != 0)
    {
        position.x = x;
    }

    if ((use.raw & SetsY) != 0)
    {
        position.y = y;
    }

    if ((use.raw & SetsZ) != 0)
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
        if ((use.raw & OffsetX) != 0)
        {
            const Vector4 up = {zero, 1.0f, zero, 1.0f};
            Vector4 side = {up.y * way.z - up.z * way.y, up.z * way.x - up.x * way.z, up.x * way.y - up.y * way.x, 1.0f};
            f32 scale = -offsetX;
            position.x = position.x + side.x * scale;
            position.y = position.y + side.y * scale;
            position.z = position.z + side.z * scale;
        }

        if ((use.raw & OffsetY) != 0)
        {
            position.y = position.y + offsetY;
        }

        if ((use.raw & OffsetZ) != 0)
        {
            f32 scale = -offsetZ;
            position.x = position.x + way.x * scale;
            position.y = position.y + way.y * scale;
            position.z = position.z + way.z * scale;
        }
    }
    else
    {
        if ((use.raw & OffsetX) != 0)
        {
            position.x = position.x + offsetX;
        }

        if ((use.raw & OffsetY) != 0)
        {
            position.y = position.y + offsetY;
        }

        if ((use.raw & OffsetZ) != 0)
        {
            position.z = position.z + offsetZ;
        }
    }

    SetFocusPosition(NodeOf(runner), position);
}

// The stored position set to the focus position (or the origin) moved a distance along the way from the agent's instance to it
// over the ground, turned about y by an angle (degrees)
void SetFocusPositionAtAngleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Degrees = 1;
    ObjectNode* node = NodeOf(runner);
    Vector4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    if ((node->flags & ObjectNodeBase::FlagFocusPosition) != 0)
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
    s32 angle;
    AngleFrom(&angle, angleValue, Degrees);
    s32 turn = angle;
    const Vector4 up = {zero, 1.0f, zero, 1.0f};
    TurnAboutAxis(&way, &up, &turn, 1);
    position.x = position.x + way.x * distance;
    position.y = position.y + way.y * distance;
    position.z = position.z + way.z * distance;
    SetStoredPosition(node, &position);
}

void NowTurnLeftCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TurnBy(runner, &angleValue, true);
}

void NowTurnRightCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    TurnBy(runner, &angleValue, false);
}

// The agent's instance warped to a target (a receiver's (bits 0-7) or a designator's (8-15) position, else the space's (16-19)
// with the offset when bit 21 says so; bit 20 is passed on) or, with bit 24, to its last contact's point moved by the offset in
// its own frame (bit 21; 1.5 above the instance without a contact); with bit 22 it turns toward the target instead (its body too
// with bit 23)
void PositionWarpCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OffsetGiven = 0x200000;
    constexpr u32 Turns = 0x400000;
    constexpr u32 TurnsBody = 0x800000;
    constexpr u32 ToContact = 0x1000000;
    constexpr f32 NoContact = Rounded(5e-5);
    constexpr f32 AboveWithoutContact = 1.5f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOrAhead(instance, (targetAndSpace & Turns) != 0);
    if ((targetAndSpace & ToContact) != 0)
    {
        position = node->unknown140;
        if (__builtin_fabsf(position.x) <= NoContact && __builtin_fabsf(position.y) <= NoContact &&
            __builtin_fabsf(position.z) <= NoContact)
        {
            position = PositionOf(instance);
            position.y = position.y + AboveWithoutContact;
        }
        else if ((targetAndSpace & OffsetGiven) != 0)
        {
            ObjectPlace* place = instance->place;
            RotateAndTranslate(place);
            const f32 (*m)[4] = place->matrix.m;
            f32 dx = m[0][0] * x + m[1][0] * y + m[2][0] * z;
            f32 dy = m[0][1] * x + m[1][1] * y + m[2][1] * z;
            f32 dz = m[0][2] * x + m[1][2] * y + m[2][2] * z;
            position.z = position.z + dz;
            position.x = position.x + dx;
            position.y = position.y + dy;
        }
    }
    else
    {
        const Vector4* offset = (targetAndSpace & OffsetGiven) != 0 ? VectorAt(&x) : nullptr;
        DesignatedPosition(&position, targetAndSpace >> 16 & 0xF, runner, offset, static_cast<u8>(targetAndSpace >> 8),
                           static_cast<u8>(targetAndSpace), targetAndSpace >> 20 & 1);
    }

    WarpTo(node, instance, position, (targetAndSpace & Turns) != 0, (targetAndSpace & TurnsBody) != 0);
}

// An agent's instance (warp's byte 3: a designator's, 0xFF the agent's own) warped like PositionWarp does to a target, or to the
// position of the designator in source's low byte (its instance's or its own) when it isn't 0xFF; its agent told after
void WarpAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OffsetGiven = 0x200000;
    constexpr u32 Turns = 0x400000;
    constexpr u32 TurnsBody = 0x800000;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOrAhead(instance, (warp & Turns) != 0);
    u8 from = static_cast<u8>(source);
    if (from != NoDesignator)
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
        const Vector4* offset = (warp & OffsetGiven) != 0 ? VectorAt(&offsetX) : nullptr;
        DesignatedPosition(&position, warp >> 16 & 0xF, runner, offset, static_cast<u8>(warp >> 8), static_cast<u8>(warp),
                           warp >> 20 & 1);
    }

    ObjectNode* warped = node;
    u8 agent = static_cast<u8>(warp >> 24);
    if (agent != NoDesignator)
    {
        // Retail bug: a designator without an instance warps none (its place read at address 8) with the agent's own node
        instance = DesignatorOf(node, agent);
        if (instance != nullptr)
        {
            warped = ObjectNodeOf(instance);
        }
    }

    WarpTo(warped, instance, position, (warp & Turns) != 0, (warp & TurnsBody) != 0);
    Agent* told = warped->agent;
    CallVirtual<void>(told, told->vtable, AgentWarpedSlot);
}
