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
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/shadows.h"

#include <cstddef>
#include <cstdint>

// The commands that warp and turn an agent's instance, strafe and move it toward a designator, push it away from what it
// perceives, set its keys and the trajectory controller's wobble, link it to the nearest AI position, set its head tracking's
// target, change and arrange its linked objects, give it shadow shapes, spawn instances for it and let go of the ones attached
// to it; and the module's statics

extern "C"
{
    // A copy of a halfword
    void CopyShort(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
    // A rigid body pushed by a force at a point
    void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point) RETAIL(FUN_0024c508);
    // The AI position of a chunk nearest a point, with its index
    extern void* G_ChunkManager;
    // An object node's keys and paths given another's
    // The instance the player operates (a vehicle's), and a word of the module's statics nothing reads
    extern Reference* g_OperatedInstance RETAIL(D_0030A930);
    extern u32 g_StaticWord30A920 RETAIL(D_0030A920);
    void InitMotionCommandStatics(u32 initialise, u32 priority) RETAIL(FUN_00220620);
    // The module's global constructor
    void ConstructMotionCommandsModule() RETAIL(FUN_00225878);
}

namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
// The object nodes' vtable functions: what slot 9 calls (an object node lets go of its links), whether it takes packets, the
// byte the factory gives the instances it makes for it (0xFF none), a designator's instance, an instance given to a designator
constexpr u32 ReleaseLinksSlot = 11;
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 FactoryByteSlot = 25;
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 SetDesignatorSlot = 39;
// The agents' vtable function giving a velocity of its own, and the referenced objects' sleep
constexpr u32 OwnVelocitySlot = 11;
constexpr u32 SleepSlot = 3;
constexpr u8 NoDesignator = 0xFF;
constexpr u8 NoExitPoint = 0xFF;
constexpr u8 NoKey = 0xFF;
constexpr u16 NoMessage = 0xFFFF;
constexpr u32 ResourceIndexMask = 0x7FFF;
// The messages' kinds of nodes
constexpr u32 MessageKinds = 2;
// The spaces the commands' values are in (the AgentLab tool's): the world, the instance's start, its own, its stored place
constexpr u32 WorldSpace = 0;
constexpr u32 StartSpace = 1;
constexpr u32 OwnSpace = 2;
constexpr u32 StoredSpace = 7;
// A mark of the instances' flags (bit 8) the arranging of linked objects sets and passes over
constexpr u32 Marked = 0x100;
// The attachments node's word: the linked objects' count (bits 0-4) and the current one (bits 7-11), the instances after it
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;
// The squared lengths too short to have a direction
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr f32 UnitsPerDegree = Rounded(65536.0 / 360.0);
// How far an instance turned toward where it goes leans into the turn at most (degrees)
constexpr f32 MostLean = 90.0f;
// The rigid body's 64 bits at 0x88: it moves (56) and stops once at rest (57); its word at 0x90: its steps since it was launched
// (bits 11-14)
constexpr u64 Moving = u64{1} << 56;
constexpr u64 StopsAtRest = u64{1} << 57;
constexpr u64 LaunchSteps = 0x7800;

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
    return static_cast<ObjectNode*>(GetGameNode(NodesOf(instance), ObjectNodeKind));
}

bool Asleep(const InstanceContext* instance)
{
    return (instance->flags & ReferencedObject::FlagAsleep) != 0;
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
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
    if (node->agentRef2 != nullptr && Asleep(node->agentRef2) && (node->flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
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

// The path of the instances attached to the attachments' instance (none: nullptr)
void* AttachmentPath(void* attachments)
{
    return *reinterpret_cast<void**>(static_cast<u8*>(attachments) + 0x70);
}

// A counter of an agent's (its bytes 0x18 on) changed by an amount, kept within 0 and 255
void AddToCounter(Agent* agent, u32 counter, s32 amount)
{
    constexpr s32 CounterMax = 0xFF;
    u8* byte = reinterpret_cast<u8*>(agent) + offsetof(Agent, unknown18) + counter;
    s32 value = *byte + amount;
    if (value > CounterMax)
    {
        *byte = CounterMax;
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

// An instance turned to a rotation in a space (bits 16-19 of the bits) of a node's: the rotation given in the world, the
// instance's start rotation or its stored place's, those turned by the rotation given with bit 21, or its own turned by it
// (only with bit 21; nothing in the other spaces); then with bit 22 rolled level about the way it faces, its y axis up as far
// as it goes
void TurnInSpace(ObjectNode* node, InstanceContext* instance, u32 bits, const Vector4* turn)
{
    constexpr u32 SpaceShift = 16;
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 TurnGiven = 0x200000;
    constexpr u32 Levels = 0x400000;
    switch (bits >> SpaceShift & SpaceMask)
    {
    case WorldSpace:
        TurnInstanceTo(instance, *turn);
        break;
    case StartSpace:
    {
        Vector4 rotation = node->informationPointer->rotation;
        if ((bits & TurnGiven) != 0)
        {
            MultiplyRotations(&rotation, &rotation, turn);
        }

        TurnInstanceTo(instance, rotation);
        break;
    }
    case OwnSpace:
    {
        if ((bits & TurnGiven) == 0)
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
    case StoredSpace:
    {
        ObjectPlace* stored = node->storedPlace;
        if (stored == nullptr)
        {
            break;
        }

        stored->SyncRotation();
        Vector4 rotation = stored->rotation;
        if ((bits & TurnGiven) != 0)
        {
            MultiplyRotations(&rotation, &rotation, turn);
        }

        TurnInstanceTo(instance, rotation);
        break;
    }
    default:
        break;
    }

    if ((bits & Levels) == 0)
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
        if ((node->flags & ObjectNodeBase::FlagStoredPosition) != 0)
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
        else if (focusPosition && (node->flags & ObjectNodeBase::FlagFocusPosition) != 0)
        {
            *target = node->focusPosition;
        }

        break;
    }
    case DesignatesFocusPosition:
        if ((node->flags & ObjectNodeBase::FlagFocusPosition) != 0)
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
    u8 wrapped = key == count ? Waypoints::FlagWrapped : 0;
    waypoints->flags = (waypoints->flags & ~Waypoints::FlagWrapped) | wrapped;
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

// The agent's instance turned to a rotation in a space (the command's word's bits 16-19, see TurnInSpace)
void RotationWarpCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    TurnInSpace(node, node->owner, targetAndSpace, VectorAt(&quatX));
}

// A designator's instance (subject's low byte, 0xFF the agent's own) turned like RotationWarp turns the agent's, in the spaces
// of its own node.
// Retail bug: a designator without an instance turns none (its place read at address 8) with the agent's node's spaces
void RotateAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u8 designator = static_cast<u8>(subject);
    if (designator != NoDesignator)
    {
        instance = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, static_cast<u32>(designator));
        if (instance != nullptr)
        {
            node = ObjectNodeOf(instance);
        }
    }

    TurnInSpace(node, instance, rotate, VectorAt(&quatX));
}

// The instance moved at a speed (for the time since its node's last update) along an axis, as far as the way to a designator's
// position (the target's low byte, see DesignatorTarget; else the origin) goes along it, when that's over 0.1: the space's (bits
// 8-11) x axis, the start's or its own, else the way itself. With a distance it only moves while it stays within it along the
// axis from the start. It's turned toward the target with bit 12, and the trajectory controller's position is where it went.
// Retail bug: outside the start's space the distance is measured from what the stack held (the origin here)
void StrafeTowardsTargetCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SpaceShift = 8;
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 Faces = 0x1000;
    constexpr f32 TooShort = Rounded(0.1);
    ObjectNode* node = NodeOf(runner);
    Vector4 to = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatorTarget(node, static_cast<u8>(target), &to, true);
    f32 seconds = SecondsSinceUpdate(node, clock);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOf(instance);
    Vector4 way = {to.x - position.x, to.y - position.y, to.z - position.z, 1.0f};
    Vector4 axis = way;
    Vector4 start = {};
    switch (target >> SpaceShift & SpaceMask)
    {
    case StartSpace:
    {
        InstancePlacement* information = node->informationPointer;
        Matrix4x4 matrix;
        MatrixFromRotation(&matrix, &information->rotation);
        axis = *RowOf(&matrix, 0);
        start = information->position;
        break;
    }
    case OwnSpace:
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

    if ((target & Faces) != 0)
    {
        SteerTowards(1.0f, 0.0f, MostLean, instance, &to);
    }

    if (node->trajectory != nullptr)
    {
        node->trajectory->position = moved;
    }
}

// The instance moved toward a designator's position (the target's low byte, see DesignatorTarget: the focus without the focus
// position; else the origin) at a speed (value2) for the time since its node's last update, its body turned toward where it
// goes first
void MoveTowardsDesignatorCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Vector4 position = PositionOf(instance);
    // (Retail asks for the node's velocity and never reads it)
    Vector4 velocity;
    CopyVelocity(node, &velocity);
    Vector4 to = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatorTarget(node, static_cast<u8>(target), &to, false);
    f32 seconds = SecondsSinceUpdate(node, clock);
    Vector4 way = {to.x - position.x, to.y - position.y, to.z - position.z, 1.0f};
    f32 inverse = InverseLength(&way, LengthEpsilon);
    Vector4 moved = {position.x + way.x * inverse * value2 * seconds, position.y + way.y * inverse * value2 * seconds,
                     position.z + way.z * inverse * value2 * seconds, 1.0f};
    SteerBodyTowards(1.0f, 0.0f, instance, &moved, 1);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&moved))
    {
        QueueObject(instance);
    }
}

// A counter (filter's high half) of the agent's linked objects changed by an amount, kept within 0 and 255: of every one (bit 8
// of the target) or of the one of the index in its low byte (0xFF none), which bits 10, 11 and 9 make the first, the last and
// the current one (written into the command); only those of the object of filter's low half (0xFFFF any)
void AddToLinkedObjectsByteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Every = 0x100;
    constexpr u32 Current = 0x200;
    constexpr u32 First = 0x400;
    constexpr u32 Last = 0x800;
    constexpr u16 AnyObject = 0xFFFF;
    ObjectNode* node = NodeOf(runner);
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    s32 amount = value.IntWith(node->PacketProperties());
    if ((target & Every) != 0)
    {
        for (u32 index = 0; index < LinkedCount(attachments); index++)
        {
            InstanceContext* linked = LinkedInstances(attachments)[index];
            if (linked == nullptr)
            {
                continue;
            }

            u32 counter = static_cast<u16>(filter >> 16);
            Agent* agent = ObjectNodeOf(linked)->agent;
            u16 object;
            CopyShort(&object, &agent->objectId);
            u16 wanted = static_cast<u16>(filter);
            if (wanted != AnyObject && object != wanted)
            {
                continue;
            }

            AddToCounter(agent, counter, amount);
        }

        return;
    }

    if ((target & First) != 0)
    {
        target &= ~0xFFu;
    }
    else if ((target & Last) != 0)
    {
        target = (target & ~0xFFu) | ((LinkedCount(attachments) - 1) & 0xFF);
    }
    else if ((target & Current) != 0)
    {
        target = (target & ~0xFFu) | (*LinkedWord(attachments) >> LinkedIndexShift & LinkedIndexMask);
    }

    u8 index = static_cast<u8>(target);
    if (index == NoDesignator)
    {
        return;
    }

    InstanceContext* linked = LinkedInstances(attachments)[index];
    if (linked == nullptr)
    {
        return;
    }

    u32 counter = static_cast<u16>(filter >> 16);
    Agent* agent = ObjectNodeOf(linked)->agent;
    u16 object;
    CopyShort(&object, &agent->objectId);
    u16 wanted = static_cast<u16>(filter);
    if (wanted != AnyObject && object != wanted)
    {
        return;
    }

    AddToCounter(agent, counter, amount);
}

// The linked object whose box's middle is nearest a point given to a designator of the agent's node and, with bit 23, marked
// (bit 22 passes over the marked ones): the point is the player's position (the agent's own velocity times the lead added with
// bit 21), turned about the agent's instance by an angle (degrees) with bit 20. Retail bugs: the designator is the flags' low 4
// bits, which the parser never sets and no node takes (nothing is given the object); without a player its place is read at
// address 8; with no linked object chosen the mark goes on the word at address 4
void ArrangeLinkedObjectsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 DesignatorMask = 0xF;
    constexpr u32 Turned = 0x100000;
    constexpr u32 Leads = 0x200000;
    constexpr u32 PassesMarked = 0x400000;
    constexpr u32 Marks = 0x800000;
    constexpr f32 Far = Rounded(1e30);
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    InstanceContext* player = PlayerInstance();
    Vector4 own = PositionOf(instance);
    Vector4 point = PositionOf(player);
    if ((flags & Leads) != 0)
    {
        Agent* agent = node->agent;
        Vector4 velocity;
        CallVirtual<u32>(agent, agent->vtable, OwnVelocitySlot, &velocity);
        point.x = point.x + velocity.x * value;
        point.y = point.y + velocity.y * value;
        point.z = point.z + velocity.z * value;
    }

    if ((flags & Turned) != 0)
    {
        Vector4 offset = {point.x - own.x, point.y - own.y, point.z - own.z, point.w};
        s32 angle;
        AngleFrom(&angle, angleValue, AngleDegrees);
        TurnAboutAxis(&offset, &g_YAxis, &angle, 1);
        point = own;
        point.x = own.x + offset.x;
        point.y = own.y + offset.y;
        point.z = own.z + offset.z;
    }

    InstanceContext* chosen = nullptr;
    f32 nearest = Far;
    for (u32 index = 0; index < LinkedCount(attachments); index++)
    {
        InstanceContext* linked = LinkedInstances(attachments)[index];
        const Box& box = linked->collision.box;
        f32 x = point.x - ((box.max.x - box.min.x) * 0.5f + box.min.x);
        f32 y = point.y - ((box.max.y - box.min.y) * 0.5f + box.min.y);
        f32 z = point.z - ((box.max.z - box.min.z) * 0.5f + box.min.z);
        f32 squared = x * x + y * y + z * z;
        if (!(squared < nearest) || ((flags & PassesMarked) != 0 && (linked->flags & Marked) != 0))
        {
            continue;
        }

        chosen = linked;
        nearest = squared;
    }

    CallVirtual<u32>(node, node->vtable, SetDesignatorSlot, flags & DesignatorMask, chosen);
    if ((flags & Marks) != 0)
    {
        std::uintptr_t address = reinterpret_cast<std::uintptr_t>(chosen) + offsetof(ReferencedObject, flags);
        *reinterpret_cast<u32*>(address) |= Marked;
    }
}

// The key the waypoints are at (the node's it takes its object from when there's one): the key of the low byte, else (0xFF) a
// random one of the first (byte 1) to the last key (byte 2, the last key at most; a different one from the current key with
// bit 25; key 0 with fewer than 2) with bit 27, or one below the last without a first (with neither, as without bit 27); the
// one nearest the instance from the first (made 0 for good when it's none) to before the last with bit 26; the last key with
// bit 28
void SetKeyCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FirstShift = 8;
    constexpr u32 LastShift = 16;
    constexpr u32 OtherThanCurrent = 0x2000000;
    constexpr u32 Nearest = 0x4000000;
    constexpr u32 Random = 0x8000000;
    constexpr u32 Last = 0x10000000;
    constexpr f32 Far = 1e8f;
    ObjectNode* node = NodeOf(runner);
    Waypoints* waypoints = KeyWaypoints(node);
    u8 key = static_cast<u8>(keyAndFlags);
    if (key != NoKey)
    {
        GoToKey(waypoints, key);
        return;
    }

    u8 first = static_cast<u8>(keyAndFlags >> FirstShift);
    u8 last = static_cast<u8>(keyAndFlags >> LastShift);
    if ((keyAndFlags & Random) != 0)
    {
        if (first != NoKey)
        {
            s32 lastKey = waypoints->keyCount - 1;
            s32 upTo = lastKey < last ? lastKey : last;
            s32 range = upTo - first + 1;
            s32 chosen = 0;
            if (waypoints->keyCount >= 2)
            {
                do
                {
                    chosen = static_cast<u8>(keyAndFlags >> FirstShift) + RandomBelow(range);
                } while ((keyAndFlags & OtherThanCurrent) != 0 && chosen == waypoints->key);
            }

            GoToKey(waypoints, chosen);
            return;
        }

        if (last != NoKey)
        {
            GoToKey(waypoints, RandomBelow(last));
            return;
        }
    }

    if ((keyAndFlags & Nearest) != 0)
    {
        if (first == NoKey)
        {
            keyAndFlags &= ~(0xFFu << FirstShift);
        }

        Vector4 position = PositionOf(node->owner);
        f32 nearest = Far;
        s32 chosen = 0;
        for (u32 index = static_cast<u8>(keyAndFlags >> FirstShift);
             index < waypoints->keyCount && index < static_cast<u8>(keyAndFlags >> LastShift); index++)
        {
            const Vector4& at = waypoints->positions.data[index]->position;
            f32 x = position.x - at.x;
            f32 y = position.y - at.y;
            f32 z = position.z - at.z;
            f32 squared = x * x + y * y + z * z;
            if (squared < nearest)
            {
                nearest = squared;
                chosen = static_cast<s32>(index);
            }
        }

        GoToKey(waypoints, chosen);
    }
    else if ((keyAndFlags & Last) != 0)
    {
        GoToKey(waypoints, waypoints->keyCount - 1);
    }
}

// The agent's instance attached to its chunk's AI position nearest it when that's within 10
void LinkToNearestPointCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Near = 100.0f;
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
    if (x * x + y * y + z * z < Near)
    {
        AttachToAiPosition(AttachmentsOf(instance), instance, nearest);
    }
}

// The trajectory controller's wobble phases about x, y and z set (degrees), the ones the last value's bits say
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
    const TaggedValue* phases[3] = {&value1, &value2, &value3};
    for (u32 axis = 0; axis < 3; axis++)
    {
        if ((value4.raw & 1 << axis) != 0)
        {
            trajectory->wobblePhases[axis] = static_cast<s32>(phases[axis]->FloatWith(properties) * UnitsPerDegree);
        }
    }
}

// The trajectory controller's wobble phases about x, y and z turned by angles (degrees), the ones the last value's bits say.
// Retail bug: a phase is made radians and the sum taken as degrees, so each one shrinks to 1/57.3 of itself
void AddMotionAnglesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
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
    const TaggedValue* angles[3] = {&x, &y, &z};
    for (u32 axis = 0; axis < 3; axis++)
    {
        if ((set.raw & 1 << axis) != 0)
        {
            f32 angle = angles[axis]->FloatWith(properties);
            f32 phase = static_cast<f32>(trajectory->wobblePhases[axis]) * AngleToRadians;
            trajectory->wobblePhases[axis] = static_cast<s32>((phase + angle) * UnitsPerDegree);
        }
    }
}

// The head tracking's target given with a weight (lingering with bit 11): a starter receiver's instance (the low byte, 0xFF
// none), else the focus instance (bit 10), the player (bit 8) or AgentRef2 (bit 9); nothing without one
void SetHeadTrackingTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 ToPlayer = 0x100;
    constexpr u32 ToAgentRef2 = 0x200;
    constexpr u32 ToFocus = 0x400;
    constexpr u32 LingersShift = 11;
    ObjectNode* node = NodeOf(runner);
    HeadTracking* tracking = node->headTracking;
    if (tracking == nullptr)
    {
        return;
    }

    InstanceContext* instance = nullptr;
    u8 receiver = static_cast<u8>(target);
    if (receiver != NoDesignator)
    {
        instance = ReceiverInstance(runner, receiver);
    }
    else if ((target & ToFocus) != 0)
    {
        instance = node->AwakeFocus();
    }
    else if ((target & ToPlayer) != 0)
    {
        instance = PlayerInstance();
    }
    else if ((target & ToAgentRef2) != 0)
    {
        instance = AwakeAgentRef2(node);
    }

    if (instance != nullptr)
    {
        TrackHead(weightValue, tracking, instance, node, target >> LingersShift & 1);
    }
}

// The instance's rigid body pushed at the node's middle over the ground away from what it perceives: by the perception's
// direction (0x70 bytes in) with mode 0 (any other pushes by nothing) times a factor, for the time since the node's last update;
// its trajectory controller made to follow its motion block first, the body set moving and stopping at rest
void PushFromPerceptionCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr || node->perception == nullptr)
    {
        return;
    }

    Vector4 push = {0.0f, 0.0f, 0.0f, 1.0f};
    if (mode.raw == 0)
    {
        push = *reinterpret_cast<const Vector4*>(static_cast<u8*>(node->perception) + 0x70);
    }

    push.x = push.x * factor;
    push.y = 0.0f;
    push.z = push.z * factor;
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

    body->bits88 |= Moving | StopsAtRest;
    body->bits90 &= ~LaunchSteps;
    Vector4 point = node->unknown20;
    point.w = 1.0f;
    ApplyRigidBodyForce(body, &push, &point);
}

// A circle put in front of the shapes of a shadow slot (byte 0) of the node's instance: its kind (byte 1), its joint (byte 2),
// its radius and height, its offset
void SetShadowCircleCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, ShadowNode::NodeKind));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, static_cast<u8>(meshSlotAndMode));
    auto* memory = static_cast<ShadowCircle*>(MemoryAllocate(sizeof(ShadowCircle)));
    f32 circleRadius = radius.FloatWith(properties);
    f32 height = radius2.FloatWith(properties);
    ShadowCircle* circle = ShadowCircle::Construct(circleRadius, height, memory, static_cast<u8>(meshSlotAndMode >> 8),
                                                   static_cast<u8>(meshSlotAndMode >> 16));
    circle->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->AddCircle(circle);
}

// A shadow slot (byte 0) of the node's instance given new shapes (its shadow node made when it has none): the shapes of a
// distance (size) and a strength (size2), the slot cast as strongly as height
void SetShadowCommand::ExecuteOn(GameNode* node)
{
    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    InstanceContext* instance = node->owner;
    auto* shadow = static_cast<ShadowNode*>(GetGameNode(&instance->nodes, ShadowNode::NodeKind));
    if (shadow == nullptr)
    {
        constexpr u32 AttachNode = 1;
        shadow = ShadowNode::Construct(static_cast<ShadowNode*>(MemoryAllocate(sizeof(ShadowNode))));
        RegisterNode(instance, AttachNode, shadow);
    }

    auto* memory = static_cast<ShadowShapes*>(MemoryAllocate(sizeof(ShadowShapes)));
    f32 distance = size.FloatWith(properties);
    f32 strength = size2.FloatWith(properties);
    ShadowShapes* shapes = ShadowShapes::Construct(distance, strength, memory);
    auto* slot = static_cast<ShadowSlot*>(MemoryAllocate(sizeof(ShadowSlot)));
    slot = ShadowSlot::Construct(height.FloatWith(properties), slot, shapes);
    shadow->SetSlot(static_cast<u8>(shadowSlot), slot);
}

// A capsule put in front of the shapes of a shadow slot (byte 0) of the node's instance: its kind (byte 1), its two joints (bytes
// 2 and 3), its radius, its offset
void SetShadowMeshCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, ShadowNode::NodeKind));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, static_cast<u8>(meshSlotAndMode));
    auto* memory = static_cast<ShadowCapsule*>(MemoryAllocate(sizeof(ShadowCapsule)));
    f32 capsuleRadius = size.FloatWith(properties);
    ShadowCapsule* capsule = ShadowCapsule::Construct(capsuleRadius, memory, static_cast<u8>(meshSlotAndMode >> 8),
                                                      static_cast<u8>(meshSlotAndMode >> 16),
                                                      static_cast<u8>(meshSlotAndMode >> 24));
    capsule->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->AddCapsule(capsule);
}

// The plain shape of a shadow slot (byte 0) of the node's instance set: its kind (byte 1), its width and depth, its offset
void SetShadowRectangleCommand::ExecuteOn(GameNode* node)
{
    GameNode* shadow = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, ShadowNode::NodeKind));
    if (shadow == nullptr)
    {
        return;
    }

    PropertyHolder* properties = static_cast<ObjectNodeBase*>(node)->PacketProperties();
    ShadowShapes* shapes = SlotShapes(shadow, static_cast<u8>(slot));
    auto* memory = static_cast<ShadowPlain*>(MemoryAllocate(sizeof(ShadowPlain)));
    f32 width = size.FloatWith(properties);
    f32 depth = size3.FloatWith(properties);
    ShadowPlain* plain = ShadowPlain::Construct(width, depth, memory, static_cast<u8>(slot >> 8));
    plain->offset = ShapeOffset(offsetX, offsetY, offsetZ, properties);
    shapes->SetPlain(plain);
}

// An instance made for the agent (its spawner the agent's instance, which its ID's entry links) of the object of objectAndKey's
// low half, of the agent's own object (bit 36 of the flags' 64 bits) or of AgentRef2's (bit 40), the last two with their
// instance's properties; the factory gives it the subtype with bit 47, the agent's ID with bit 39. It's turned like the agent's
// instance, or (bit 35) none, like its start or like itself by the space (bits 24-28). It's made at the agent's instance and
// attached to the agent (bit 31: at the exit point of byte 2, the two leaving each other out of their collisions), or at the
// exit point of its model of byte 2 (turned like it) or at a designator's position (the designator of objectAndKey's byte 2, or
// the focus (bit 34), the focus position (33) or the stored position (48)) in the space, the offset added with bit 30, then
// linked to the agent with bit 32. Then: the flags' low half sent it as a trigger message (0xFFFF none), its flags' trigger
// signals and sphere contact cleared (bit 29), its mark set (49) or cleared (50), its node's halfword 6 objectAndKey's byte 3
// squared (0xFFFF for 0xFF, kept for 0), and the agent's node's focus (bit 37) or AgentRef2 (38) made it; it's given the
// agent's AgentRef1 (42), AgentRef2 (43), focus (44), stored position (45), linked objects (46), keys (51) and paths (52), and
// is linked to its spawner (41). Retail bugs: turned by a space (bit 35) other than the world, the start and its own, its
// angles are what the stack held; an instance made without an ID makes the spawner the word at address 4
void SpawnResidentAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    // flags and flags2 are one word of 64 bits, read whole
    constexpr u64 MessageMask = 0xFFFF;
    constexpr u32 ExitPointShift = 16;
    constexpr u32 SpaceShift = 24;
    constexpr u64 SpaceMask = 0x1F;
    constexpr u64 Unsignalled = u64{1} << 29;
    constexpr u64 OffsetGiven = u64{1} << 30;
    constexpr u64 AtAgent = u64{1} << 31;
    constexpr u64 LinksToAgent = u64{1} << 32;
    constexpr u64 AtFocusPosition = u64{1} << 33;
    constexpr u64 AtFocus = u64{1} << 34;
    constexpr u64 TurnedBySpace = u64{1} << 35;
    constexpr u64 OwnObject = u64{1} << 36;
    constexpr u64 BecomesFocus = u64{1} << 37;
    constexpr u64 BecomesAgentRef2 = u64{1} << 38;
    constexpr u64 AgentsId = u64{1} << 39;
    constexpr u64 AgentRef2Object = u64{1} << 40;
    constexpr u64 LinksSpawner = u64{1} << 41;
    constexpr u64 SharesAgentRef1 = u64{1} << 42;
    constexpr u64 SharesAgentRef2 = u64{1} << 43;
    constexpr u64 SharesFocus = u64{1} << 44;
    constexpr u64 SharesStoredPosition = u64{1} << 45;
    constexpr u64 SharesLinked = u64{1} << 46;
    constexpr u64 SubtypeGiven = u64{1} << 47;
    constexpr u64 AtStoredPosition = u64{1} << 48;
    constexpr u64 SetsMark = u64{1} << 49;
    constexpr u64 ClearsMark = u64{1} << 50;
    constexpr u64 SharesKeys = u64{1} << 51;
    constexpr u64 SharesPaths = u64{1} << 52;
    // The factory's byte of the instances it makes (bits 5-12 of its flags, 0xFF none)
    constexpr u32 FactoryByteShift = 5;
    constexpr u32 FactoryByteMask = 0x1FE0;
    constexpr u8 RateNone = 0xFF;
    constexpr u16 RateNever = 0xFFFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    Agent* agent = node->agent;
    InstanceFactory* factory = g_InstanceFactory;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    PropertyHolder* properties = node->PacketProperties();
    const u64 bits = *reinterpret_cast<const u64*>(&flags);
    u32 factoryByte = 0;
    if ((bits & SubtypeGiven) != 0)
    {
        factoryByte = static_cast<u8>(subtype.IntWith(properties));
    }

    u8 nodeByte8C = node->unknown8C;
    InstanceContext* source = nullptr;
    u32 objectId;
    if ((bits & OwnObject) != 0)
    {
        source = instance;
        u16 id;
        CopyShort(&id, &agent->objectId);
        objectId = id & ResourceIndexMask;
    }
    else if ((bits & AgentRef2Object) != 0)
    {
        source = AwakeAgentRef2(node);
        ObjectNode* sourceNode = ObjectNodeOf(source);
        u16 id;
        CopyShort(&id, &sourceNode->agent->objectId);
        objectId = id & ResourceIndexMask;
        if ((bits & SubtypeGiven) == 0)
        {
            factoryByte = CallVirtual<u32>(sourceNode, sourceNode->vtable, FactoryByteSlot);
        }
    }
    else
    {
        objectId = static_cast<u16>(objectAndKey) & ResourceIndexMask;
    }

    u32 space = static_cast<u32>(bits >> SpaceShift & SpaceMask);
    s32 angles[3];
    if ((bits & TurnedBySpace) == 0 || space == OwnSpace)
    {
        ObjectPlace* place = instance->place;
        place->SyncRotation();
        AnglesOfRotation(&place->rotation, &angles[0], &angles[1], &angles[2]);
    }
    else if (space == WorldSpace)
    {
        angles[0] = 0;
        angles[1] = 0;
        angles[2] = 0;
    }
    else if (space == StartSpace)
    {
        AnglesOfRotation(&node->informationPointer->rotation, &angles[0], &angles[1], &angles[2]);
    }

    factory->SetFlag2();
    factory->ClearFlag1();
    factory->SetFlag0();
    if ((bits & AgentsId) != 0)
    {
        factory->SetFlag3();
    }
    else
    {
        factory->ClearFlag3();
    }

    factory->creationFlags = 0;
    if ((bits & SubtypeGiven) != 0)
    {
        factory->flags = (factory->flags & ~FactoryByteMask) | (factoryByte & 0xFF) << FactoryByteShift;
    }

    InstanceContext* spawned;
    u8 exitPoint = static_cast<u8>(flags >> ExitPointShift);
    if ((bits & AtAgent) != 0)
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
        if ((bits & AtFocus) != 0)
        {
            designator = DesignatesFocus;
        }
        else if ((bits & AtFocusPosition) != 0)
        {
            designator = DesignatesFocusPosition;
        }
        else if ((bits & AtStoredPosition) != 0)
        {
            designator = DesignatesStoredPosition;
        }
        else
        {
            designator = static_cast<u8>(objectAndKey >> 16);
        }

        Vector4 position;
        const Vector4* offset = (bits & OffsetGiven) != 0 ? VectorAt(&offsetX) : nullptr;
        if (exitPoint == NoExitPoint)
        {
            DesignatedPosition(&position, space, runner, offset, designator, NoDesignator, 0);
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

        if ((bits & (OwnObject | AgentRef2Object)) != 0)
        {
            spawned = CreateInstanceFrom(factory, chunk, source, objectId, &position, angles, 0);
        }
        else
        {
            spawned = CreateInstance(factory, chunk, objectId, &position, angles);
        }

        if ((bits & LinksToAgent) != 0)
        {
            LinkToAgent(agent, spawned, 0);
        }
    }

    if (spawned != nullptr)
    {
        s32 id = spawned->id;
        std::uintptr_t entry = id != -1 ? reinterpret_cast<std::uintptr_t>(&g_InstanceIds->entries[id]) : 0;
        *reinterpret_cast<InstanceContext**>(entry + offsetof(InstanceIds::Entry, unknown04)) = instance;
        ObjectNode* spawnedNode = ObjectNodeOf(spawned);
        Agent* spawnedAgent = spawnedNode->agent;
        if ((bits & AgentsId) != 0)
        {
            spawnedAgent->id = agent->id;
        }

        RestartAgent(spawnedAgent);
        if ((bits & LinksSpawner) != 0)
        {
            LinkInstance(AttachmentsOf(spawned), instance, 0);
        }

        u16 message = static_cast<u16>(bits & MessageMask);
        if (message != NoMessage)
        {
            Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
            GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &sender,
                                                    MessageKinds);
            Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
            QueueEvent(spawned, &handle);
        }

        if ((bits & Unsignalled) != 0)
        {
            spawned->flags &= ~ReferencedObject::FlagSphereContact & ~ReferencedObject::FlagTriggerSignals;
        }

        if ((bits & SetsMark) != 0)
        {
            spawned->flags |= Marked;
        }

        if ((bits & ClearsMark) != 0)
        {
            spawned->flags &= ~Marked;
        }

        u8 rate = static_cast<u8>(objectAndKey >> 24);
        if (rate != 0)
        {
            spawnedNode->unknown06 = rate == RateNone ? RateNever : static_cast<u16>(rate * rate);
        }

        if ((bits & BecomesFocus) != 0)
        {
            node->focusInstance = spawned;
            node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
        }

        if ((bits & BecomesAgentRef2) != 0)
        {
            node->agentRef2 = spawned;
        }

        if ((bits & SharesAgentRef1) != 0)
        {
            spawnedNode->agentRef1 = AwakeAgentRef1(node);
        }

        if ((bits & SharesAgentRef2) != 0)
        {
            spawnedNode->agentRef2 = AwakeAgentRef2(node);
        }

        if ((bits & SharesFocus) != 0)
        {
            constexpr u32 FocusFlags = ObjectNodeBase::FlagFocusInstance | ObjectNodeBase::FlagFocusPosition;
            spawnedNode->focusPosition = node->focusPosition;
            u32 kept = spawnedNode->flags & ~FocusFlags;
            spawnedNode->flags = kept;
            if ((node->flags & ObjectNodeBase::FlagFocusInstance) != 0)
            {
                spawnedNode->flags = kept | ObjectNodeBase::FlagFocusInstance;
            }
            else if ((node->flags & ObjectNodeBase::FlagFocusPosition) != 0)
            {
                spawnedNode->flags = kept | ObjectNodeBase::FlagFocusPosition;
            }
        }

        if ((bits & SharesStoredPosition) != 0 && (node->flags & ObjectNodeBase::FlagStoredPosition) != 0)
        {
            Vector4 stored = node->storedPosition;
            SetStoredPosition(spawnedNode, &stored);
        }

        if ((bits & SharesLinked) != 0)
        {
            void* linked = GetGameNode(&instance->nodes, AttachmentsKind);
            if (linked != nullptr)
            {
                LinkAllOf(AttachmentsOf(spawned), linked);
            }
        }

        if ((bits & SharesKeys) != 0 && TakesPackets(spawnedNode))
        {
            CopyKeys(spawnedNode->waypoints, node->waypoints);
        }

        if ((bits & SharesPaths) != 0 && TakesPackets(spawnedNode))
        {
            CopyPaths(spawnedNode->waypoints, node->waypoints);
        }

        spawnedNode->unknown8C = nodeByte8C;
    }

    factory->ClearFlag3();
    factory->flags |= FactoryByteMask;
}

// The instance attached at an exit point (the low byte; 0xFF the one attached without one) let go of by the agent's
// attachments and put to sleep, its object node told to let go of its links: only one with an ID (one made by a script). With
// bit 8 and no exit point every linked object with an ID is let go of and put to sleep instead
void DestroySpawnedAttachmentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 EveryLinked = 0x100;
    void* attachments = GetGameNode(&runner->agentNode->owner->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    InstanceContext* attached;
    u8 exitPoint = static_cast<u8>(value1);
    if (exitPoint != NoExitPoint)
    {
        void* path = AttachmentPath(attachments);
        attached = path != nullptr ? SlottedAttachment(path, exitPoint) : nullptr;
        if (attached == nullptr || attached->id == -1)
        {
            return;
        }

        DetachExitPoint(attachments, exitPoint, 0, 0);
    }
    else if ((value1 & EveryLinked) != 0)
    {
        UnlinkSpawned(attachments, 1);
        return;
    }
    else
    {
        void* path = AttachmentPath(attachments);
        attached = path != nullptr ? UnslottedAttachment(path) : nullptr;
        if (attached == nullptr || attached->id == -1)
        {
            return;
        }

        DetachUnslotted(attachments);
    }

    CallVirtual<u32>(attached, attached->vtable, SleepSlot);
    ObjectNode* attachedNode = ObjectNodeOf(attached);
    CallVirtual<void>(attachedNode, attachedNode->vtable, ReleaseLinksSlot);
}

// The module's static constructor: the references it keeps (the player's instance, the operated one) made empty
void InitMotionCommandStatics(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_StaticWord30A920 = 0;
    g_PlayerInstance = nullptr;
    g_OperatedInstance = nullptr;
}

void ConstructMotionCommandsModule()
{
    InitMotionCommandStatics(1, 0xFFFF);
}
