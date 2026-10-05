#include "game/commands.h"

#include "game/animation.h"
#include "game/attachments.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/events.h"
#include "game/instancefactory.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/shadows.h"

#include <cstddef>
#include <cstdint>

// The commands that warp and turn an agent's instance, strafe and move it toward a designator, push it along what it
// perceives, set its keys and the trajectory controller's wobble, link it to the nearest AI position, set its head tracking's
// target, change its linked objects' counters and pick the one near the player, give it shadow shapes, spawn instances for it
// and let go of the ones attached to it; and the module's statics

extern "C"
{
    // A copy of a halfword
    void CopyShort(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
    // A rigid body pushed by a force at a point
    void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point) RETAIL(FUN_0024c508);
    // The chunk manager the scripts' chunks are looked up in (the game context's)
    extern void* G_ChunkManager;
    // The instance the player operates (a vehicle's), and a word of the module's statics nothing reads
    extern Reference* g_OperatedInstance RETAIL(D_0030A930);
    extern u32 g_UnusedMotionWord RETAIL(D_0030A920);
    void InitMotionCommandStatics(u32 initialise, u32 priority) RETAIL(FUN_00220620);
    // The module's global constructor
    void ConstructMotionCommandsModule() RETAIL(FUN_00225878);
}

namespace
{

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

ObjectNode* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNode*>(GetGameNode(NodesOf(instance), NodeObject));
}

AttachmentsNode* AttachmentsNodeOf(InstanceContext* instance)
{
    return static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, NodeAttachments));
}

bool Asleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
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

// The instance a starter receiver's index stands for (retail reads past the receivers' 8 instances for the larger ones)
InstanceContext* ReceiverInstance(BehaviourRunner* runner, u8 index)
{
    return runner->receivers->instances[index];
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

// The vector of a command's four floats
const Vector4* VectorAt(const f32* x)
{
    return reinterpret_cast<const Vector4*>(x);
}

// The seconds since the node's last update (none before its first)
f32 SecondsSinceUpdate(const GameNode* node, const TimeClock* clock)
{
    s32 units = node->time != 0 ? static_cast<s32>(clock->time - node->time) : 0;
    return static_cast<f32>(units) * g_SecondsPerClockUnit;
}

u32 CurrentLinked(const AttachmentsNode* attachments)
{
    return attachments->bits.currentLinked;
}

// A counter of an agent's (its bytes 0x18 on) changed by an amount, kept within 0 and 255
void AddToCounter(Agent* agent, u32 counter, s32 amount)
{
    u8* byte = reinterpret_cast<u8*>(agent) + offsetof(Agent, counters) + counter;
    s32 value = *byte + amount;
    if (value > Agent::CounterMax)
    {
        *byte = Agent::CounterMax;
    }
    else if (value >= 0)
    {
        *byte = static_cast<u8>(value);
    }
    else
    {
        *byte = 0;
    }
}

// The place's rotation set, worked out from its matrix first when it turned: the instance queued when it changed
void TurnInstanceTo(InstanceContext* instance, const Vector4& rotation)
{
    ObjectPlace* place = RetailPlaceOf(instance);
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }
}

// An instance turned to a rotation in a space of a node's: the rotation given in the world, the instance's start rotation or its
// stored place's, those turned by the rotation when one is given, or its own turned by it (only with one; nothing in the other
// spaces); then, when it levels, rolled level about the way it faces, its y axis up as far as it goes
void TurnInSpace(ObjectNode* node, InstanceContext* instance, RotationWarpBits bits, const Vector4* turn)
{
    switch (bits.space)
    {
    case ControlPacket::WorldSpace:
        TurnInstanceTo(instance, *turn);
        break;
    case ControlPacket::InitialSpace:
    {
        Vector4 rotation = node->informationPointer->rotation;
        if (bits.rotationGiven)
        {
            MultiplyRotations(&rotation, &rotation, turn);
        }

        TurnInstanceTo(instance, rotation);
        break;
    }
    case ControlPacket::CurrentSpace:
    {
        if (!bits.rotationGiven)
        {
            break;
        }

        ObjectPlace* place = RetailPlaceOf(instance);
        place->SyncRotation();
        Vector4 rotation = place->rotation;
        MultiplyRotations(&rotation, &rotation, turn);
        TurnInstanceTo(instance, rotation);
        break;
    }
    case ControlPacket::StoredSpace:
    {
        ObjectPlace* stored = node->storedPlace;
        if (stored == nullptr)
        {
            break;
        }

        stored->SyncRotation();
        Vector4 rotation = stored->rotation;
        if (bits.rotationGiven)
        {
            MultiplyRotations(&rotation, &rotation, turn);
        }

        TurnInstanceTo(instance, rotation);
        break;
    }
    default:
        break;
    }

    if (!bits.levels)
    {
        return;
    }

    ObjectPlace* place = RetailPlaceOf(instance);
    RotateAndTranslate(place);
    Matrix4x4 matrix = place->matrix;
    Vector4 forward = *RowOf(&place->matrix, 2);
    f32 inverse = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * inverse;
    forward.y = forward.y * inverse;
    forward.z = forward.z * inverse;
    MatrixFacing(&matrix, &forward);
    place = RetailPlaceOf(instance);
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }
}

// The position of a designator the motion commands go toward: AgentRef1's instance's, the stored position, the focus
// instance's (or the focus position when asked) and the focus position. Others (AgentRef2 among them, which the parsers offer)
// leave it
void DesignatorTarget(ObjectNode* node, u8 designator, Vector4* target, bool focusPosition)
{
    switch (designator)
    {
    case DesignatesAgentRef1:
    {
        InstanceContext* ref = AwakeAgentRef1(node);
        if (ref != nullptr)
        {
            *target = PositionOf(ref);
        }

        break;
    }
    case DesignatesStoredPosition:
        if (node->flags.storedPosition)
        {
            *target = node->storedPosition;
        }

        break;
    case DesignatesFocus:
    {
        InstanceContext* focus = node->AwakeFocus();
        if (focus != nullptr)
        {
            *target = PositionOf(focus);
        }
        else if (focusPosition && node->flags.focusPosition)
        {
            *target = node->focusPosition;
        }

        break;
    }
    case DesignatesFocusPosition:
        if (node->flags.focusPosition)
        {
            *target = node->focusPosition;
        }

        break;
    default:
        break;
    }
}

// The waypoints the keys are of: the node's it takes its object from when there's one
Waypoints* KeyWaypoints(ObjectNode* node)
{
    auto* source = static_cast<ObjectNode*>(node->sourceNode);
    return source != nullptr ? source->waypoints : node->waypoints;
}

// The waypoints put at a key, taken round past the last one (its flag set when it's one past the last)
void GoToKey(Waypoints* waypoints, s32 key)
{
    s32 count = waypoints->keyCount;
    waypoints->flags.wrapped = key == count;
    waypoints->key = static_cast<u8>(key < count ? key : key - count);
}

// A shadow node's slot's shapes, the properties a command's values read and the offset a shape's at
ShadowShapes* SlotShapes(GameNode* node, u8 slot)
{
    return static_cast<ShadowNode*>(node)->slots[slot]->shapes;
}

Vector4 ShapeOffset(const TaggedValue& x, const TaggedValue& y, const TaggedValue& z, PropertyHolder* properties)
{
    f32 offsetX = x.FloatWith(properties);
    f32 offsetY = y.FloatWith(properties);
    f32 offsetZ = z.FloatWith(properties);
    return {offsetX, offsetY, offsetZ, 1.0f};
}
}

// The agent's instance turned to a rotation in a space (see TurnInSpace)
void RotationWarpCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    TurnInSpace(node, node->owner, warp, VectorAt(&quatX));
}

// A designator's instance (DesignatesNone the agent's own) turned like RotationWarp turns the agent's, in the spaces of its own
// node.
// Retail bug: a designator without an instance turns none (its place read at address 8) with the agent's node's spaces
void RotateAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u8 designator = agent.designator;
    if (designator != DesignatesNone)
    {
        instance = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot,
                                                 static_cast<u32>(designator));
        if (instance != nullptr)
        {
            node = ObjectNodeOf(instance);
        }
    }

    TurnInSpace(node, instance, warp, VectorAt(&quatX));
}

// The instance moved at a speed (for the time since its node's last update) along an axis, as far as the way to a designator's
// position (see DesignatorTarget; else the origin) goes along it, when that's over 0.1: the space's x axis, the start's or its
// own, else the way itself. With a distance it only moves while it stays within it along the axis from the start. It's turned
// toward the target when it faces it, and the trajectory controller's position is where it went.
// Retail bug: outside the start's space the distance is measured from what the stack held (the origin here)
void StrafeTowardsTargetCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 TooShort = Rounded(0.1);
    ObjectNode* node = NodeOf(runner);
    Vector4 to = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatorTarget(node, target.designator, &to, true);
    f32 seconds = SecondsSinceUpdate(node, clock);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOf(instance);
    Vector4 way = {to.x - position.x, to.y - position.y, to.z - position.z, 1.0f};
    Vector4 axis = way;
    Vector4 start = {};
    switch (target.space)
    {
    case ControlPacket::InitialSpace:
    {
        InstancePlacement* information = node->informationPointer;
        Matrix4x4 matrix;
        MatrixFromRotation(&matrix, &information->rotation);
        axis = *RowOf(&matrix, 0);
        start = information->position;
        break;
    }
    case ControlPacket::CurrentSpace:
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        axis = *RowOf(&place->matrix, 0);
        break;
    }
    default:
        break;
    }

    f32 along = way.x * axis.x + way.y * axis.y + way.z * axis.z;
    Vector4 move = {axis.x * along, axis.y * along, axis.z * along, 1.0f};
    if (!(TooShort < __builtin_sqrtf(move.x * move.x + move.y * move.y + move.z * move.z)))
    {
        return;
    }

    f32 inverse = InverseLength(&move, LengthEpsilon);
    Vector4 moved = {position.x + move.x * inverse * speed * seconds, position.y + move.y * inverse * speed * seconds,
                     position.z + move.z * inverse * speed * seconds, 1.0f};
    if (maxDistance > 0.0f)
    {
        f32 x = moved.x - start.x;
        f32 y = moved.y - start.y;
        f32 z = moved.z - start.z;
        if (!(__builtin_fabsf(axis.x * x + axis.y * y + axis.z * z) <= maxDistance))
        {
            return;
        }
    }

    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&moved))
    {
        QueueObject(instance);
    }

    if (target.faces)
    {
        SteerTowards(1.0f, 0.0f, SteerMostLean, instance, &to);
    }

    if (node->trajectory != nullptr)
    {
        node->trajectory->position = moved;
    }
}

// The instance moved toward a designator's position (see DesignatorTarget: the focus without the focus position; else the
// origin) at a speed for the time since its node's last update, its body turned toward where it goes first
void MoveTowardsDesignatorCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOf(instance);
    // (Retail asks for the node's velocity and never reads it)
    Vector4 velocity;
    CopyVelocity(node, &velocity);
    Vector4 to = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatorTarget(node, target.designator, &to, false);
    f32 seconds = SecondsSinceUpdate(node, clock);
    Vector4 way = {to.x - position.x, to.y - position.y, to.z - position.z, 1.0f};
    f32 inverse = InverseLength(&way, LengthEpsilon);
    Vector4 moved = {position.x + way.x * inverse * speed * seconds, position.y + way.y * inverse * speed * seconds,
                     position.z + way.z * inverse * speed * seconds, 1.0f};
    SteerBodyTowards(1.0f, 0.0f, instance, &moved, 1);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&moved))
    {
        QueueObject(instance);
    }
}

// A counter of the agent's linked objects changed by an amount, kept within 0 and 255: of every one or of the one of an index,
// which the choice of the first, the last or the current one writes into the command; only those of the counter's object
void AddToLinkedCounterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    if (attachments == nullptr)
    {
        return;
    }

    s32 change = amount.IntWith(node->PacketProperties());
    if (linked.every)
    {
        for (u32 index = 0; index < attachments->LinkedCount(); index++)
        {
            InstanceContext* instance = attachments->linked[index];
            if (instance == nullptr)
            {
                continue;
            }

            u32 counterIndex = counter.index;
            Agent* agent = ObjectNodeOf(instance)->agent;
            u16 object;
            CopyShort(&object, &agent->objectId);
            u16 wanted = counter.object;
            if (wanted != AnyObjectId && object != wanted)
            {
                continue;
            }

            AddToCounter(agent, counterIndex, change);
        }

        return;
    }

    if (linked.first)
    {
        linked.index = 0;
    }
    else if (linked.last)
    {
        linked.index = attachments->LinkedCount() - 1;
    }
    else if (linked.current)
    {
        linked.index = CurrentLinked(attachments);
    }

    u8 index = linked.index;
    if (index == LinkedObjectChoice::NoIndex)
    {
        return;
    }

    InstanceContext* instance = attachments->linked[index];
    if (instance == nullptr)
    {
        return;
    }

    u32 counterIndex = counter.index;
    Agent* agent = ObjectNodeOf(instance)->agent;
    u16 object;
    CopyShort(&object, &agent->objectId);
    u16 wanted = counter.object;
    if (wanted != AnyObjectId && object != wanted)
    {
        return;
    }

    AddToCounter(agent, counterIndex, change);
}

// The linked object whose box's middle is nearest a point given to a designator of the agent's node and marked busy when asked
// (the busy ones passed over when asked): the point is the player's position (the agent's own velocity times the lead added
// when it leads), turned about the agent's instance by an angle (degrees) when asked. Retail bugs: the designator is 4 bits,
// which the parser never sets and no node takes (nothing is given the object); without a player its place is read at address
// 8; with no linked object chosen the mark goes on the word at address 4
void PickLinkedObjectNearPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    if (attachments == nullptr)
    {
        return;
    }

    InstanceContext* player = PlayerInstance();
    Vector4 own = PositionOf(instance);
    Vector4 point = PositionOf(player);
    if (pick.leads)
    {
        Agent* agent = node->agent;
        Vector4 velocity;
        CallVirtual<u32>(agent, agent->vtable, Agent::VelocitySlot, &velocity);
        point.x = point.x + velocity.x * leadSeconds;
        point.y = point.y + velocity.y * leadSeconds;
        point.z = point.z + velocity.z * leadSeconds;
    }

    if (pick.turned)
    {
        Vector4 offset = {point.x - own.x, point.y - own.y, point.z - own.z, point.w};
        s32 angle;
        AngleFrom(&angle, degrees, AngleDegrees);
        TurnAboutAxis(&offset, &g_YAxis, &angle, 1);
        point = own;
        point.x = own.x + offset.x;
        point.y = own.y + offset.y;
        point.z = own.z + offset.z;
    }

    InstanceContext* chosen = nullptr;
    f32 nearest = Infinite;
    for (u32 index = 0; index < attachments->LinkedCount(); index++)
    {
        InstanceContext* linked = attachments->linked[index];
        const Box& box = linked->collision.box;
        f32 x = point.x - ((box.max.x - box.min.x) * 0.5f + box.min.x);
        f32 y = point.y - ((box.max.y - box.min.y) * 0.5f + box.min.y);
        f32 z = point.z - ((box.max.z - box.min.z) * 0.5f + box.min.z);
        f32 squared = x * x + y * y + z * z;
        if (!(squared < nearest) || (pick.passesBusy && linked->flags.busy))
        {
            continue;
        }

        chosen = linked;
        nearest = squared;
    }

    CallVirtual<u32>(node, node->vtable, ObjectNode::SetDesignatorSlot, pick.designator, chosen);
    if (pick.marksBusy)
    {
        // The instance marked busy (at address 4 without one)
        std::uintptr_t address = reinterpret_cast<std::uintptr_t>(chosen) + offsetof(ReferencedObject, flags);
        reinterpret_cast<ReferencedObjectFlags*>(address)->busy = 1;
    }
}

// The key the waypoints are at (the node's it takes its object from when there's one): the key's index, else (NoKey) a random
// one of the range when asked (the last key at most; a different one from the current key with noRepeat; key 0 with fewer than
// 2), or one below the range's end without a start (with neither, as when it isn't random); the one nearest the instance from
// the range's start (made 0 for good when it's none) to before its end; the last key
void SetKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Far = 1e8f;
    ObjectNode* node = NodeOf(runner);
    Waypoints* waypoints = KeyWaypoints(node);
    u8 index = key.index;
    if (index != Waypoints::NoKey)
    {
        GoToKey(waypoints, index);
        return;
    }

    u8 first = key.rangeStart;
    u8 last = key.rangeEnd;
    if (key.random)
    {
        if (first != Waypoints::NoKey)
        {
            s32 lastKey = waypoints->keyCount - 1;
            s32 upTo = lastKey < last ? lastKey : last;
            s32 range = upTo - first + 1;
            s32 chosen = 0;
            if (waypoints->keyCount >= 2)
            {
                do
                {
                    chosen = key.rangeStart + RandomBelow(range);
                } while (key.noRepeat && chosen == waypoints->key);
            }

            GoToKey(waypoints, chosen);
            return;
        }

        if (last != Waypoints::NoKey)
        {
            GoToKey(waypoints, RandomBelow(last));
            return;
        }
    }

    if (key.nearest)
    {
        if (first == Waypoints::NoKey)
        {
            key.rangeStart = 0;
        }

        Vector4 position = PositionOf(node->owner);
        f32 nearest = Far;
        s32 chosen = 0;
        for (u32 at = key.rangeStart; at < waypoints->keyCount && at < key.rangeEnd; at++)
        {
            const Vector4& keyPosition = waypoints->positions.data[at]->position;
            f32 x = position.x - keyPosition.x;
            f32 y = position.y - keyPosition.y;
            f32 z = position.z - keyPosition.z;
            f32 squared = x * x + y * y + z * z;
            if (squared < nearest)
            {
                nearest = squared;
                chosen = static_cast<s32>(at);
            }
        }

        GoToKey(waypoints, chosen);
    }
    else if (key.last)
    {
        GoToKey(waypoints, waypoints->keyCount - 1);
    }
}

// The agent's instance attached to its chunk's AI position nearest it when that's within 10
void LinkToNearestPointCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 NearSquared = 100.0f;
    InstanceContext* instance = runner->agentNode->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    Vector4 position = PositionOf(instance);
    AiPosition* nearest = NearestAiPosition(chunk, &position, nullptr);
    if (nearest == nullptr)
    {
        return;
    }

    f32 x = position.x - nearest->position.x;
    f32 y = position.y - nearest->position.y;
    f32 z = position.z - nearest->position.z;
    if (x * x + y * y + z * z < NearSquared)
    {
        AttachToAiPosition(AttachmentsOf(instance), instance, nearest);
    }
}

// The trajectory controller's wobble phases about x, y and z set (degrees), the ones of the axes given
void AlterWobblePhaseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
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
    const TaggedValue* phases[3] = {&phaseX, &phaseY, &phaseZ};
    for (u32 axis = 0; axis < 3; axis++)
    {
        if ((axes.value & 1 << axis) != 0)
        {
            trajectory->wobblePhases[axis] = static_cast<s32>(phases[axis]->FloatWith(properties) * DegreesToAngle);
        }
    }
}

// The trajectory controller's wobble phases about x, y and z turned by angles (degrees), the ones of the axes given.
// Retail bug: a phase is made radians and the sum taken as degrees, so each one shrinks to 1/57.3 of itself
void AddWobblePhaseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
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
    const TaggedValue* turns[3] = {&turnX, &turnY, &turnZ};
    for (u32 axis = 0; axis < 3; axis++)
    {
        if ((axes.value & 1 << axis) != 0)
        {
            f32 turn = turns[axis]->FloatWith(properties);
            f32 phase = static_cast<f32>(trajectory->wobblePhases[axis]) * AngleToRadians;
            trajectory->wobblePhases[axis] = static_cast<s32>((phase + turn) * DegreesToAngle);
        }
    }
}

// The head tracking's target given with a weight (remembered with it when asked): a starter receiver's instance, else the focus
// instance, the player or AgentRef2; nothing without one
void SetHeadTrackingTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    HeadTracking* tracking = node->headTracking;
    if (tracking == nullptr)
    {
        return;
    }

    InstanceContext* instance = nullptr;
    u8 receiver = target.receiver;
    if (receiver != DesignatesNone)
    {
        instance = ReceiverInstance(runner, receiver);
    }
    else if (target.focus)
    {
        instance = node->AwakeFocus();
    }
    else if (target.player)
    {
        instance = PlayerInstance();
    }
    else if (target.agentRef2)
    {
        instance = AwakeAgentRef2(node);
    }

    if (instance != nullptr)
    {
        TrackHead(weight, tracking, instance, node, target.remembers);
    }
}

// The instance's rigid body pushed at the node's middle over the ground: along the perception's direction (0x70 bytes in) by the
// strength for perception slot 0 (any other pushes by nothing), for the time since the node's last update; its trajectory
// controller made to follow its motion block first, the body set moving and stopping at rest
void PushFromPerceptionCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr || node->perception == nullptr)
    {
        return;
    }

    Vector4 push = {0.0f, 0.0f, 0.0f, 1.0f};
    if (perceptionSlot == 0)
    {
        push = node->perception->direction;
    }

    push.x = push.x * strength;
    push.y = 0.0f;
    push.z = push.z * strength;
    f32 seconds = SecondsSinceUpdate(node, clock);
    push.x = push.x * seconds;
    push.z = push.z * seconds;
    push.y = push.y * seconds;
    FollowOwnMotionBlock(node);
    ObjectRigidBody* body = node->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    body->bits.moving = 1;
    body->bits.stopsAtRest = 1;
    body->state.launchSteps = 0;
    Vector4 point = node->middle;
    point.w = 1.0f;
    ApplyRigidBodyForce(body, &push, &point);
}

// A circle put in front of the shapes of a shadow slot of the node's instance: its kind, its joint, its radius and height, its
// offset
void SetShadowCircleCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, NodeShadow));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, shape.slot);
    auto* memory = static_cast<ShadowCircle*>(MemoryAllocate(sizeof(ShadowCircle)));
    f32 circleRadius = radius.FloatWith(properties);
    f32 circleHeight = height.FloatWith(properties);
    ShadowCircle* circle = ShadowCircle::Construct(circleRadius, circleHeight, memory, shape.kind, shape.joint);
    circle->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->AddCircle(circle);
}

// A shadow slot of the node's instance given new shapes (its shadow node made when it has none): the shapes of a distance and a
// strength at that distance, the slot cast as strongly as its near strength up close
void SetShadowCommand::ExecuteOn(GameNode* node)
{
    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    InstanceContext* instance = node->owner;
    auto* shadow = static_cast<ShadowNode*>(GetGameNode(&instance->nodes, NodeShadow));
    if (shadow == nullptr)
    {
        shadow = ShadowNode::Construct(static_cast<ShadowNode*>(MemoryAllocate(sizeof(ShadowNode))));
        RegisterNode(instance, AttachNode, shadow);
    }

    auto* memory = static_cast<ShadowShapes*>(MemoryAllocate(sizeof(ShadowShapes)));
    f32 reach = distance.FloatWith(properties);
    f32 strength = farStrength.FloatWith(properties);
    ShadowShapes* shapes = ShadowShapes::Construct(reach, strength, memory);
    auto* shadowSlot = static_cast<ShadowSlot*>(MemoryAllocate(sizeof(ShadowSlot)));
    shadowSlot = ShadowSlot::Construct(nearStrength.FloatWith(properties), shadowSlot, shapes);
    shadow->SetSlot(slot.slot, shadowSlot);
}

// A capsule put in front of the shapes of a shadow slot of the node's instance: its kind, its two joints, its radius, its
// offset
void SetShadowMeshCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, NodeShadow));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, shape.slot);
    auto* memory = static_cast<ShadowCapsule*>(MemoryAllocate(sizeof(ShadowCapsule)));
    f32 capsuleRadius = radius.FloatWith(properties);
    ShadowCapsule* capsule = ShadowCapsule::Construct(capsuleRadius, memory, shape.kind, shape.joint, shape.secondJoint);
    capsule->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->AddCapsule(capsule);
}

// The plain shape of a shadow slot of the node's instance set: its kind, its width and depth, its offset
void SetShadowRectangleCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, NodeShadow));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, shape.slot);
    auto* memory = static_cast<ShadowPlain*>(MemoryAllocate(sizeof(ShadowPlain)));
    f32 plainWidth = width.FloatWith(properties);
    f32 plainDepth = depth.FloatWith(properties);
    ShadowPlain* plain = ShadowPlain::Construct(plainWidth, plainDepth, memory, shape.kind);
    plain->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->SetPlain(plain);
}

// An instance made for the agent (its spawner the agent's instance, which its ID's entry links) of the command's object, of the
// agent's own object or of AgentRef2's, the last two with their instance's properties; the factory gives it the subtype when it's
// given, the agent's ID when asked. It's turned like the agent's instance, or (turned by the space) none, like its start or like
// itself. It's made at the agent's instance and attached to the agent (at the exit point, the two leaving each other out of
// their collisions), or at the exit point of its model (turned like it) or at a designator's position (the object's designator,
// or the focus, the focus position or the stored position) in the space, the offset added when it's given, then linked to the
// agent when asked. Then: the message sent it as a trigger message, its flags' trigger signals and collision cleared
// (unsignalled), its busy mark set or cleared, its node's near distance the object's squared, and the agent's node's focus or
// AgentRef2 made it; it's given the agent's AgentRef1, AgentRef2, focus, stored position, linked objects, keys and paths, and is
// linked to its spawner, as the flags say. Retail bugs: turned by a space other than the world, the start and its own, its
// angles are what the stack held; an instance made without an ID makes the spawner the word at address 4
void SpawnResidentAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Agent* agent = node->agent;
    InstanceFactory* factory = g_InstanceFactory;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    PropertyHolder* properties = node->PacketProperties();
    u32 factoryByte = 0;
    if (flags.subtypeGiven)
    {
        factoryByte = static_cast<u8>(subtype.IntWith(properties));
    }

    u8 rank = node->rank;
    InstanceContext* source = nullptr;
    u32 objectId;
    if (flags.ownObject)
    {
        source = instance;
        u16 id;
        CopyShort(&id, &agent->objectId);
        objectId = id & ResourceIndexMask;
    }
    else if (flags.agentRef2Object)
    {
        source = AwakeAgentRef2(node);
        ObjectNode* sourceNode = ObjectNodeOf(source);
        u16 id;
        CopyShort(&id, &sourceNode->agent->objectId);
        objectId = id & ResourceIndexMask;
        if (!flags.subtypeGiven)
        {
            factoryByte = CallVirtual<u32>(sourceNode, sourceNode->vtable, ObjectNode::CustomSlotSlot);
        }
    }
    else
    {
        objectId = object.id & ResourceIndexMask;
    }

    u32 space = flags.space;
    s32 angles[3];
    if (!flags.turnedBySpace || space == ControlPacket::CurrentSpace)
    {
        ObjectPlace* place = instance->place;
        place->SyncRotation();
        AnglesOfRotation(&place->rotation, &angles[0], &angles[1], &angles[2]);
    }
    else if (space == ControlPacket::WorldSpace)
    {
        angles[0] = 0;
        angles[1] = 0;
        angles[2] = 0;
    }
    else if (space == ControlPacket::InitialSpace)
    {
        AnglesOfRotation(&node->informationPointer->rotation, &angles[0], &angles[1], &angles[2]);
    }

    factory->SetInstanceProperties();
    factory->ClearUnused1();
    factory->SetGivesIds();
    if (flags.agentsId)
    {
        factory->SetGivesFlagSlots();
    }
    else
    {
        factory->ClearGivesFlagSlots();
    }

    factory->creationFlags = 0;
    if (flags.subtypeGiven)
    {
        factory->flags.subtype = factoryByte;
    }

    InstanceContext* spawned;
    u8 exitPoint = flags.exitPoint;
    if (flags.atAgent)
    {
        Vector4 position = PositionOf(instance);
        spawned = CreateInstance(factory, chunk, objectId, &position, angles);
        AttachToAgent(agent, spawned, 0, exitPoint, nullptr);
        if (spawned != nullptr)
        {
            spawned->collision.leftOut = instance;
            instance->collision.leftOut = spawned;
        }
    }
    else
    {
        u8 designator;
        if (flags.atFocus)
        {
            designator = DesignatesFocus;
        }
        else if (flags.atFocusPosition)
        {
            designator = DesignatesFocusPosition;
        }
        else if (flags.atStoredPosition)
        {
            designator = DesignatesStoredPosition;
        }
        else
        {
            designator = object.designator;
        }

        Vector4 position;
        const Vector4* offset = flags.offsetGiven ? VectorAt(&offsetX) : nullptr;
        if (exitPoint == GameOGI::NoExitPoint)
        {
            DesignatedPosition(&position, space, runner, offset, designator, DesignatesNone, 0);
        }
        else
        {
            OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel))->animator;
            if (animator != nullptr)
            {
                SizedArray<ExitPointAnimation*>* points = animator->exitPoints;
                ExitPointAnimation* point = points != nullptr ? points->data[exitPoint] : nullptr;
                if (point != nullptr)
                {
                    AnglesOfMatrix(&UpdateExitPointMatrix(point)->matrix, angles);
                }
            }

            JointPosition(instance, exitPoint, &position, offset);
        }

        if (flags.ownObject || flags.agentRef2Object)
        {
            spawned = CreateInstanceFrom(factory, chunk, source, objectId, &position, angles, 0);
        }
        else
        {
            spawned = CreateInstance(factory, chunk, objectId, &position, angles);
        }

        if (flags.linksToAgent)
        {
            LinkToAgent(agent, spawned, 0);
        }
    }

    if (spawned != nullptr)
    {
        s32 id = spawned->id;
        std::uintptr_t entry = id != -1 ? reinterpret_cast<std::uintptr_t>(&g_InstanceIds->entries[id]) : 0;
        *reinterpret_cast<InstanceContext**>(entry + offsetof(InstanceIds::Entry, spawner)) = instance;
        ObjectNode* spawnedNode = ObjectNodeOf(spawned);
        Agent* spawnedAgent = spawnedNode->agent;
        if (flags.agentsId)
        {
            spawnedAgent->id = agent->id;
        }

        RestartAgent(spawnedAgent);
        if (flags.linksSpawner)
        {
            LinkInstance(AttachmentsOf(spawned), instance, 0);
        }

        u16 message = flags.message;
        if (message != NoMessage)
        {
            Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
            GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &sender,
                                                    ObjectNodeKinds);
            Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
            QueueEvent(spawned, &handle);
        }

        if (flags.unsignalled)
        {
            spawned->flags.collisionActive = 0;
            spawned->flags.receivesTriggerSignals = 0;
        }

        if (flags.marksBusy)
        {
            spawned->flags.busy = 1;
        }

        if (flags.clearsBusy)
        {
            spawned->flags.busy = 0;
        }

        u8 nearDistance = object.nearDistance;
        if (nearDistance != 0)
        {
            spawnedNode->nearDistance = nearDistance == SpawnedObject::AnyDistance
                                            ? GameNode::AnyNearDistance
                                            : static_cast<u16>(nearDistance * nearDistance);
        }

        if (flags.becomesFocus)
        {
            node->focusInstance = spawned;
            node->flags.focusInstance = 1;
            node->flags.focusPosition = 0;
        }

        if (flags.becomesAgentRef2)
        {
            node->agentRef2 = spawned;
        }

        if (flags.sharesAgentRef1)
        {
            spawnedNode->agentRef1 = AwakeAgentRef1(node);
        }

        if (flags.sharesAgentRef2)
        {
            spawnedNode->agentRef2 = AwakeAgentRef2(node);
        }

        if (flags.sharesFocus)
        {
            spawnedNode->focusPosition = node->focusPosition;
            spawnedNode->flags.value &= ~ObjectNodeFlags::FocusMask;
            if (node->flags.focusInstance)
            {
                spawnedNode->flags.focusInstance = 1;
            }
            else if (node->flags.focusPosition)
            {
                spawnedNode->flags.focusPosition = 1;
            }
        }

        if (flags.sharesStoredPosition && node->flags.storedPosition)
        {
            Vector4 stored = node->storedPosition;
            SetStoredPosition(spawnedNode, &stored);
        }

        if (flags.sharesLinked)
        {
            AttachmentsNode* linked = AttachmentsNodeOf(instance);
            if (linked != nullptr)
            {
                LinkAllOf(AttachmentsOf(spawned), linked);
            }
        }

        if (flags.sharesKeys && TakesPackets(spawnedNode))
        {
            CopyKeys(spawnedNode->waypoints, node->waypoints);
        }

        if (flags.sharesPaths && TakesPackets(spawnedNode))
        {
            CopyPaths(spawnedNode->waypoints, node->waypoints);
        }

        spawnedNode->rank = rank;
    }

    factory->ClearGivesFlagSlots();
    factory->flags.subtype = InstanceFactoryFlags::NoSubtype;
}

// The instance attached at an exit point (GameOGI::NoExitPoint the one attached without one) let go of by the agent's attachments
// and put to sleep, its object node told to let go of its links: only one with an ID (one made by a script). With no exit point
// every linked object with an ID is let go of and put to sleep instead when asked
void DestroySpawnedAttachmentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    AttachmentsNode* attachments = AttachmentsNodeOf(runner->agentNode->owner);
    if (attachments == nullptr)
    {
        return;
    }

    InstanceContext* attached;
    u8 exitPoint = attachment.exitPoint;
    if (exitPoint != GameOGI::NoExitPoint)
    {
        AttachmentsPath* path = attachments->path;
        attached = path != nullptr ? SlottedAttachment(path, exitPoint) : nullptr;
        if (attached == nullptr || attached->id == -1)
        {
            return;
        }

        DetachExitPoint(attachments, exitPoint, 0, 0);
    }
    else if (attachment.everyLinked)
    {
        UnlinkSpawned(attachments, 1);
        return;
    }
    else
    {
        AttachmentsPath* path = attachments->path;
        attached = path != nullptr ? UnslottedAttachment(path) : nullptr;
        if (attached == nullptr || attached->id == -1)
        {
            return;
        }

        DetachUnslotted(attachments);
    }

    CallVirtual<u32>(attached, attached->vtable, InstanceContext::SleepSlot);
    ObjectNode* attachedNode = ObjectNodeOf(attached);
    CallVirtual<void>(attachedNode, attachedNode->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
}

// The module's static constructor: the references it keeps (the player's instance, the operated one) made empty
void InitMotionCommandStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_UnusedMotionWord = 0;
    g_PlayerInstance = nullptr;
    g_OperatedInstance = nullptr;
}

void ConstructMotionCommandsModule()
{
    InitMotionCommandStatics(1, DefaultInitPriority);
}
