#include "game/commands.h"

#include "game/attachment.h"
#include "game/attachments.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/instancefactory.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"
#include "game/scenery.h"

// The commands that attach instances to an agent's instance and let them go (dropped, thrown, launched), link and spring them,
// turn an instance with the one it hangs from, set volume controllers, send user messages and trigger the instances around; and
// the module's statics

// A volume controller's node (kind 7): the force it applies and its bits (bit 0: on)
struct VolumeControllerNode
{
    u8 unknown00[0x170];
    Vector4 force;
    u64 bits;
};

extern "C"
{
    // The chunk manager's chunk of an index
    extern void* G_ChunkManager;
    // The instances the trigger command finds, and the byte the scripts set that it compares with
    extern InstanceContext* g_RangeResults[0x400] RETAIL(D_003B5ED0);
    extern u8 g_GlobalByte30A0E9 RETAIL(D_0030A0E9);
    // The instances the starters' receivers' indexes stand for, and a word of the module's statics nothing reads
    extern Reference* g_ReceiverInstances[256] RETAIL(G_InstanceContextRefsCounterArray);
    extern u32 g_StaticWord30A950 RETAIL(D_0030A950);
    // The module's statics made, and the static constructor that runs it
    void InitCommandStatics(u32 initialise, u32 priority) RETAIL(FUN_002518b0);
    void CommandsStaticInit() RETAIL(FUN_00255430);
}

namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
constexpr u32 VolumeControllerKind = 7;
// The object nodes' vtable functions: whether it takes packets, launched (a gravity and a velocity), pushed (a strength and the
// other), a designator's instance
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 LaunchSlot = 26;
constexpr u32 PushSlot = 27;
constexpr u32 GetDesignatorSlot = 36;
// The agents' vtable functions: whether it falls once its support goes (the crates), its support gone (a crate falls)
constexpr u32 FallsSlot = 3;
constexpr u32 UnsupportedSlot = 4;
constexpr u32 EventDestroySlot = 1;
// The launches' gravity that means the default one (30)
constexpr f32 OwnGravity = -1.0f;
constexpr u8 NoExitPoint = 0xFF;
constexpr u16 NoMessage = 0xFFFF;
// A message's kinds of nodes
constexpr u32 MessageKinds = 2;
// An instance's flags: it's held by another (bit 6), it holds others (bit 7), and bit 8, which attaching sets and letting go
// clears when the commands say so
constexpr u32 InstanceHeld = 0x40;
constexpr u32 InstanceHolds = 0x80;
constexpr u32 InstanceFlag8 = 0x100;
// The attachments node's word: the linked objects' count (bits 0-4) and the current one (bits 7-11), the instances after it
constexpr u32 LinkedCountMask = 0x1F;
constexpr u32 LinkedIndexShift = 7;
constexpr u32 LinkedIndexMask = 0x1F;
// The queries' instances: the awake ones, with every wanted flag
constexpr u32 QueryTakesAll = 0x2;

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

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
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

u32* AttachmentsWord(void* attachments)
{
    return reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18);
}

InstanceContext** LinkedInstances(void* attachments)
{
    return reinterpret_cast<InstanceContext**>(static_cast<u8*>(attachments) + 0x20);
}

// The attachments' path (the entries with their exit points)
void* PathOf(void* attachments)
{
    return *reinterpret_cast<void**>(static_cast<u8*>(attachments) + 0x70);
}

u32 LinkedCount(void* attachments)
{
    return *AttachmentsWord(attachments) & LinkedCountMask;
}

bool OnPath(void* attachments, InstanceContext* instance)
{
    void* path = PathOf(attachments);
    return path != nullptr && PathIndexOf(path, instance) != -1;
}

// The node a rigid body moves (retail keeps it in the high half of the bits at 0x90)
ObjectNode* NodeOfBody(ObjectRigidBody* body)
{
    return reinterpret_cast<ObjectNode*>(static_cast<u32>(body->bits90 >> 32));
}

// A user message from an instance (a reference to it its argument), and one sent to an instance (a reference to it its handle)
GameEvent* MakeMessage(u32 message, InstanceContext* sender)
{
    Reference* argument = sender != nullptr ? AddReference(sender) : nullptr;
    return GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &argument, MessageKinds);
}

void SendMessage(GameEvent* event, InstanceContext* target)
{
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(target, &handle);
}

void DestroyMessage(GameEvent* event)
{
    if (event != nullptr)
    {
        CallVirtual<void>(event, event->vtable, EventDestroySlot, u32{DestroyAndFree});
    }
}

// A query of the awake instances of a chunk (the commands' own: no instances left out)
void MakeQuery(InstanceRayHit* query, void** results, u16 most, u32 wantedFlags)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Rounded(1e30);
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits = QueryTakesAll;
    query->wantedFlags = wantedFlags;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// The instance's place's position, worked out from its matrix first when it moved
Vector4 PositionOf(InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    return place->position;
}

// The velocity that throws from a position to a target at a speed along the ground under a gravity: up as fast as reaching the
// target's height in the time the ground's distance takes needs (the target's w kept)
Vector4 ThrowVelocity(const Vector4* position, const Vector4* target, f32 speed, f32 gravity)
{
    Vector4 velocity = *target;
    velocity.x = velocity.x - position->x;
    velocity.y = 0.0f;
    velocity.z = velocity.z - position->z;
    f32 distance = __builtin_sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
    f32 rise = target->y - position->y;
    f32 time = distance / speed;
    f32 perDistance = speed / distance;
    velocity.x = velocity.x * perDistance;
    velocity.z = velocity.z * perDistance;
    velocity.y = (rise + gravity * (time * 0.5f) * time) / time;
    return velocity;
}

// The instance let go of what holds it (its parent forgotten, bit 8 cleared when asked)
void LetGo(InstanceContext* instance, bool clearFlag8)
{
    if (clearFlag8)
    {
        instance->flags &= ~InstanceFlag8;
    }

    instance->parent = nullptr;
    instance->flags &= ~InstanceHeld;
}

// The instance turned about its own x (0), y (1) or z axis (2) by an angle: whether it turned (not by none)
bool TurnInstance(InstanceContext* instance, s32 angle, u32 axis)
{
    ObjectPlace* place = instance->place;
    if (angle == 0)
    {
        return false;
    }

    place->SyncRotation();
    place->bits = (place->bits | ObjectPlace::BitTurned) & ~u64{ObjectPlace::BitMatrixTurned};
    s32 turn = angle;
    Vector4 rotation;
    if (axis == 0)
    {
        RotationFromPitch(&rotation, &turn);
    }
    else if (axis == 1)
    {
        RotationFromYaw(&rotation, &turn);
    }
    else
    {
        RotationFromRoll(&rotation, &turn);
    }

    MultiplyRotations(&place->rotation, &place->rotation, &rotation);
    return true;
}

// A position moved by an offset turned by a place (its matrix made up to date first)
void MoveByOffset(Vector4* position, ObjectPlace* place, const Vector4* offset)
{
    RotateAndTranslate(place);
    const Vector4* rowX = RowOf(&place->matrix, 0);
    const Vector4* rowY = RowOf(&place->matrix, 1);
    const Vector4* rowZ = RowOf(&place->matrix, 2);
    position->z = position->z + ((rowX->z * offset->x + rowY->z * offset->y) + rowZ->z * offset->z);
    position->x = position->x + ((rowX->x * offset->x + rowY->x * offset->y) + rowZ->x * offset->z);
    position->y = position->y + ((rowX->y * offset->x + rowY->y * offset->y) + rowZ->y * offset->z);
}

}

// The focus (0), AgentRef1 (1) or AgentRef2 (2) linked to the instance (its attachments made when it has none)
void NowRotateJointCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceContext* target = nullptr;
    switch (value1.raw & 7)
    {
    case 0:
        // Retail bug: without a focus instance the target is what the caller left in $s0, the command itself
        // (RunBodyCommandsAndJump's), which gets linked as an instance
        if ((node->flags & ObjectNodeBase::FlagFocusInstance) == 0 || node->focusInstance == nullptr)
        {
            target = reinterpret_cast<InstanceContext*>(this);
        }
        else
        {
            target = node->AwakeFocus();
        }

        break;
    case 1:
        target = AwakeAgentRef1(node);
        break;
    case 2:
        target = AwakeAgentRef2(node);
        break;
    default:
        break;
    }

    if (instance != nullptr && target != nullptr)
    {
        LinkInstance(AttachmentsOf(instance), target, 0);
    }
}

// A designator's instance (the focus, AgentRef1 or AgentRef2), or a linked object, attached to the instance (or the instance to
// the focus with bit 23) at an exit point, with an offset and angles when given; one that takes packets first has its rigid body
// taken out of the physics world (its hold modes above 8 made 1, the physics body flag of its node's instance cleared without a
// motion block) and its trajectory controller and motion block let go. The flags' bits: the designator (0-2), the exit point
// (3-10, 0xFF none), how it follows (11-14), an offset (15) and angles (16) given, a linked object's index (17-20, 0xF: the
// designator's instance), bit 8 of the attached instance's flags set (22)
void AttachFocusObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 ExitPointShift = 3;
    constexpr u32 FollowShift = 11;
    constexpr u32 FollowMask = 0xF;
    constexpr u32 OffsetGiven = 0x8000;
    constexpr u32 AnglesGiven = 0x10000;
    constexpr u32 LinkedShift = 17;
    constexpr u32 LinkedMask = 0xF;
    constexpr u32 SetsFlag8 = 0x400000;
    constexpr u32 ToFocus = 0x800000;
    // A follow mode whose last attachment loses bits 4-6
    constexpr u32 FollowMode2 = 2;
    constexpr u64 Mode2Cleared = 0x70;
    // The rigid body's two 4 bit modes that hold its moves (bits 32-35 and 36-39): ones from 9 up made 1
    constexpr u32 FirstHold = 32;
    constexpr u32 SecondHold = 36;
    constexpr u64 HoldMask = 0xF;
    constexpr u64 HighestHold = 8;
    InstanceContext* attached = nullptr;
    InstanceContext* holder = nullptr;
    u32 linked = flags >> LinkedShift & LinkedMask;
    if (linked != LinkedMask)
    {
        holder = NodeOf(runner)->owner;
        void* attachments = GetGameNode(&holder->nodes, AttachmentsKind);
        if (attachments == nullptr || linked >= LinkedCount(attachments))
        {
            return;
        }

        attached = LinkedInstances(attachments)[linked];
    }
    else
    {
        ObjectNode* node = NodeOf(runner);
        switch (flags & 7)
        {
        case 0:
            if ((flags & ToFocus) != 0)
            {
                attached = node->owner;
                holder = node->AwakeFocus();
            }
            else
            {
                holder = node->owner;
                attached = node->AwakeFocus();
            }

            break;
        case 1:
            holder = node->owner;
            attached = AwakeAgentRef1(node);
            break;
        case 2:
            holder = node->owner;
            attached = AwakeAgentRef2(node);
            break;
        default:
            break;
        }
    }

    if (holder == nullptr || attached == nullptr)
    {
        return;
    }

    ObjectNode* holderNode = ObjectNodeOf(holder);
    if (holderNode == nullptr)
    {
        return;
    }

    ObjectNode* attachedNode = ObjectNodeOf(attached);
    if (attachedNode == nullptr || (attached->flags & InstanceHeld) != 0)
    {
        return;
    }

    Agent* agent = holderNode->agent;
    if (TakesPackets(attachedNode))
    {
        ObjectRigidBody* body = attachedNode->rigidBody;
        if (body != nullptr)
        {
            if (body->physicsBody != nullptr)
            {
                RemovePhysicsBody(g_PhysicsWorld, body->physicsBody);
                body->physicsBody = nullptr;
            }

            if ((body->bits88 >> SecondHold & HoldMask) > HighestHold)
            {
                body->bits88 = (body->bits88 & ~(HoldMask << SecondHold)) | u64{1} << SecondHold;
            }

            if ((body->bits88 >> FirstHold & HoldMask) > HighestHold)
            {
                body->bits88 = (body->bits88 & ~(HoldMask << FirstHold)) | u64{1} << FirstHold;
            }

            ObjectNode* bodyNode = NodeOfBody(body);
            if (bodyNode->motionBlock == nullptr)
            {
                bodyNode->owner->flags &= ~ReferencedObject::FlagPhysicsBody;
            }
        }

        attachedNode->ReleaseTrajectory();
        attachedNode->ReleaseMotionBlock();
    }

    u32 exitPoint = flags >> ExitPointShift & 0xFF;
    u32 done;
    if ((flags & (OffsetGiven | AnglesGiven)) != 0)
    {
        Matrix4x4 offset;
        if ((flags & AnglesGiven) != 0)
        {
            s32 x = static_cast<s32>(rotX);
            s32 y = static_cast<s32>(rotY);
            s32 z = static_cast<s32>(rotZ);
            MatrixFromAngles(&offset, &x, &y, &z);
        }
        else
        {
            InitIdentityMatrix(&offset);
        }

        *RowOf(&offset, 3) = *reinterpret_cast<const Vector4*>(&offsetX);
        done = AttachToAgent(agent, attached, 1, exitPoint, &offset);
    }
    else
    {
        done = AttachToAgent(agent, attached, 1, exitPoint, nullptr);
    }

    if (done == 0)
    {
        return;
    }

    holder->flags |= InstanceHolds;
    attached->collision.leftOut = holder;
    holder->collision.leftOut = attached;
    if ((flags & SetsFlag8) != 0)
    {
        attached->flags |= InstanceFlag8;
    }

    attached->parent = holder;
    attached->flags |= InstanceHeld;
    u32 follow = flags >> FollowShift & FollowMask;
    if (follow == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&holder->nodes, AttachmentsKind);
    if (attachments == nullptr)
    {
        return;
    }

    if (follow == FollowMode2 && g_LastAttachment != nullptr)
    {
        g_LastAttachment->bits &= ~Mode2Cleared;
    }

    SetLastAttachmentMode(attachments, follow);
}

// A spring attached to the instance (its attachments made when it has none) from its joint or its place (an offset turned by its
// place added when given) to a target: a point in the focus's space (bit 21; no spring without an awake focus or when the
// instance is its own focus) or in the space's, its rest length given (bit 22) or the distance now, with an instance of an object
// made at its start when one is given (bits 4-19), which isn't solid with bit 23. offsetX2's bits: the space (0-3), the object
// (4-19, 0xFFFF none), an offset given (20); value11's bytes: the spring's kind and the joint (0xFF none)
void AttachSpringCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 ObjectShift = 4;
    constexpr u32 ObjectMask = 0xFFFF;
    constexpr u32 OffsetGiven = 0x100000;
    constexpr u32 ToFocus = 0x200000;
    constexpr u32 LengthGiven = 0x400000;
    constexpr u32 EndNotSolid = 0x800000;
    constexpr u8 NoJoint = 0xFF;
    constexpr u32 NoDesignator = 0xFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    void* attachments = AttachmentsOf(instance);
    u32 bits = static_cast<u32>(offsetX2.raw);
    InstanceContext* focus = nullptr;
    if ((bits & ToFocus) != 0)
    {
        focus = node->AwakeFocus();
        if (focus == nullptr || focus == instance)
        {
            return;
        }
    }

    const auto* point = reinterpret_cast<const Vector4*>(&x);
    Vector4 target;
    if (focus != nullptr)
    {
        ObjectPlace* place = focus->place;
        RotateAndTranslate(place);
        VuTransformPoint(&place->matrix, point, &target);
    }
    else
    {
        DesignatedPosition(&target, bits & SpaceMask, runner, point, NoDesignator, NoDesignator);
    }

    const auto* offset = (bits & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&offsetX) : nullptr;
    u8 joint = static_cast<u8>(value11 >> 8);
    Vector4 start;
    if (joint != NoJoint)
    {
        JointPosition(instance, joint, &start, offset);
    }
    else
    {
        start = PositionOf(instance);
        if (offset != nullptr)
        {
            MoveByOffset(&start, instance->place, offset);
        }
    }

    f32 length;
    if ((bits & LengthGiven) != 0)
    {
        length = value14;
    }
    else
    {
        f32 dx = start.x - target.x;
        f32 dy = start.y - target.y;
        f32 dz = start.z - target.z;
        length = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    }

    // The spring pulls toward the point in the focus's space
    if (focus != nullptr)
    {
        target = *point;
    }

    u8 kind = static_cast<u8>(value11);
    if ((bits & ObjectMask << ObjectShift) == ObjectMask << ObjectShift)
    {
        AttachSpring(value12, value13, length, attachments, instance, &target, joint, offset, nullptr, focus, kind);
        return;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, node->agent->chunkIndex);
    InstanceFactory* factory = g_InstanceFactory;
    factory->SetFlag2();
    factory->ClearFlag1();
    factory->SetFlag0();
    factory->ClearFlag3();
    factory->creationFlags = 0;
    s32 angles[3] = {0, 0, 0};
    InstanceContext* end = CreateInstance(factory, chunk, bits >> ObjectShift & ObjectMask, &start, angles);
    AttachSpring(value12, value13, length, attachments, instance, &target, joint, offset, end, focus, kind);
    if ((bits & EndNotSolid) != 0)
    {
        end->flags &= ~ReferencedObject::FlagSphereContact;
    }
}

// The instance at an exit point (or the one at none) let go of, held no longer (the instance holds none once it has no linked
// objects): mode 0 just let go, 1 launched with the velocity it had, 2 its support gone, 3 its support gone when it falls by
// itself, launched otherwise; pushed off first when it has a physics body. Then sent the message (bits 0-15; 0xFFFF none). The
// message's bits: the exit point (16-23, 0xFF none), the mode (24-26), bit 8 of its flags cleared (29)
void DropAttachedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 ExitPointShift = 16;
    constexpr u32 ModeShift = 24;
    constexpr u32 ModeMask = 7;
    constexpr u32 ClearsFlag8 = 0x20000000;
    constexpr u32 Launched = 1;
    constexpr u32 Unsupported = 2;
    constexpr u32 Falls = 3;
    constexpr u32 Modes = 4;
    constexpr f32 PushStrength = Rounded(0.1);
    InstanceContext* instance = NodeOf(runner)->owner;
    if ((instance->flags & InstanceHolds) == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    u32 mode = message >> ModeShift & ModeMask;
    u8 exitPoint = static_cast<u8>(message >> ExitPointShift);
    // Retail bug: the launches take this velocity, which nothing fills (retail hands it to the detach, which never writes it):
    // they go with what the stack held
    Vector4 velocity;
    InstanceContext* dropped = nullptr;
    if (mode < Modes)
    {
        dropped = exitPoint != NoExitPoint ? DetachExitPoint(attachments, exitPoint, 0, 0) : DetachUnslotted(attachments);
    }

    if (dropped == nullptr)
    {
        return;
    }

    if (LinkedCount(attachments) == 0)
    {
        instance->flags &= ~InstanceHolds;
    }

    LetGo(dropped, (message & ClearsFlag8) != 0);
    ObjectNode* droppedNode = ObjectNodeOf(dropped);
    if (droppedNode != nullptr)
    {
        if ((dropped->flags & ReferencedObject::FlagPhysicsBody) != 0)
        {
            CallVirtual<void>(droppedNode, droppedNode->vtable, PushSlot, PushStrength, instance);
        }

        Agent* agent;
        switch (mode)
        {
        case Launched:
            CallVirtual<void>(droppedNode, droppedNode->vtable, LaunchSlot, OwnGravity, &velocity);
            break;
        case Unsupported:
            agent = droppedNode->agent;
            CallVirtual<void>(agent, agent->vtable, UnsupportedSlot);
            break;
        case Falls:
            agent = droppedNode->agent;
            if (CallVirtual<u32>(agent, agent->vtable, FallsSlot) != 0)
            {
                agent = droppedNode->agent;
                CallVirtual<void>(agent, agent->vtable, UnsupportedSlot);
            }
            else
            {
                CallVirtual<void>(droppedNode, droppedNode->vtable, LaunchSlot, OwnGravity, &velocity);
            }

            break;
        default:
            break;
        }
    }

    if (static_cast<u16>(message) != NoMessage)
    {
        SendMessage(MakeMessage(static_cast<u16>(message), instance), dropped);
    }
}

// AgentRef2 unlinked from the instance (when it holds others) and sent the message (bits 0-15; 0xFFFF none). The attachments
// aren't checked: an instance that holds others has them
void ReleaseAgentRef2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if ((instance->flags & InstanceHolds) == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    InstanceContext* released = AwakeAgentRef2(node);
    if (released == nullptr)
    {
        return;
    }

    UnlinkInstance(attachments, released, 0, 1, 0);
    if (static_cast<u16>(event) != NoMessage)
    {
        SendMessage(MakeMessage(static_cast<u16>(event), instance), released);
    }
}

// The instance at an exit point (or the one at none) let go of and thrown at a target (a receiver's or a designator's position,
// or the space's with an offset, moved at random within a spread) at a speed under a gravity (30 when it's negative). value1's
// bits: the exit point (0-7, 0xFF none), the space (8-11), the receiver (12-19), the designator (20-27), an offset (29) and a
// spread (30) given, bit 8 of the instance's flags cleared (31); retail hands bit 28 to the target's search as well, which
// doesn't read it
void ThrowAttachedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SpaceShift = 8;
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 ReceiverShift = 12;
    constexpr u32 DesignatorShift = 20;
    constexpr u32 OffsetGiven = 0x20000000;
    constexpr u32 SpreadGiven = 0x40000000;
    constexpr u32 ClearsFlag8 = 0x80000000;
    constexpr f32 DefaultGravity = 30.0f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if ((instance->flags & InstanceHolds) == 0)
    {
        return;
    }

    void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
    u8 exitPoint = static_cast<u8>(value1);
    InstanceContext* thrown =
        exitPoint != NoExitPoint ? DetachExitPoint(attachments, exitPoint, 0, 0) : DetachUnslotted(attachments);
    if (thrown == nullptr)
    {
        return;
    }

    instance->flags &= ~InstanceHolds;
    LetGo(thrown, (value1 & ClearsFlag8) != 0);
    ObjectNode* thrownNode = ObjectNodeOf(thrown);
    if (thrownNode == nullptr)
    {
        return;
    }

    f32 gravity = radius.FloatWith(node->PacketProperties());
    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto* offset = (value1 & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&x) : nullptr;
    DesignatedPosition(&target, value1 >> SpaceShift & SpaceMask, runner, offset, value1 >> DesignatorShift & 0xFF,
                       value1 >> ReceiverShift & 0xFF);
    if ((value1 & SpreadGiven) != 0)
    {
        JitterVector(value8, 1.0f, 1.0f, 1.0f, &target);
    }

    Vector4 position = PositionOf(thrown);
    f32 speed = angleValue.FloatWith(node->PacketProperties());
    Vector4 velocity = ThrowVelocity(&position, &target, speed, 0.0f <= gravity ? gravity : DefaultGravity);
    CallVirtual<void>(thrownNode, thrownNode->vtable, LaunchSlot, gravity, &velocity);
}

// AgentRef2 let go of (unlinked when the instance holds others: it holds none once it has no linked objects left) and launched by
// the motion block at a target (as the throw's), at a speed under the block's gravity, given AgentRef1 when asked (whose queries
// then leave it out). The launch's bits: the space (8-11), the receiver (12-19), the designator (20-27); launchFlags': an offset
// (1) and a spread (2) given, bit 8 of the instance's flags cleared (3), AgentRef1 passed on (7); retail hands bit 0 to the
// target's search as well, which doesn't read it
void LaunchAgentRef2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SpaceShift = 8;
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 ReceiverShift = 12;
    constexpr u32 DesignatorShift = 20;
    constexpr u32 OffsetGiven = 0x2;
    constexpr u32 SpreadGiven = 0x4;
    constexpr u32 ClearsFlag8 = 0x8;
    constexpr u32 PassesAgentRef1 = 0x80;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceContext* launched = AwakeAgentRef2(node);
    if (launched == nullptr)
    {
        return;
    }

    if ((instance->flags & InstanceHolds) != 0)
    {
        void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
        if (UnlinkInstance(attachments, launched, 0, 1, 0) != 0 && LinkedCount(attachments) == 0)
        {
            instance->flags &= ~InstanceHolds;
        }
    }

    u32 flags = static_cast<u32>(launchFlags.raw);
    LetGo(launched, (flags & ClearsFlag8) != 0);
    ObjectNode* launchedNode = ObjectNodeOf(launched);
    if (launchedNode == nullptr)
    {
        return;
    }

    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto* offset = (flags & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&offsetX) : nullptr;
    DesignatedPosition(&target, launch >> SpaceShift & SpaceMask, runner, offset, launch >> DesignatorShift & 0xFF,
                       launch >> ReceiverShift & 0xFF);
    if ((flags & SpreadGiven) != 0)
    {
        JitterVector(scaleFactor, 1.0f, 1.0f, 1.0f, &target);
    }

    Vector4 position = PositionOf(launched);
    f32 speed = timeOrDistance.FloatWith(node->PacketProperties());
    Vector4 velocity = ThrowVelocity(&position, &target, speed, gravity);
    if ((flags & PassesAgentRef1) != 0)
    {
        InstanceContext* agentRef1 = AwakeAgentRef1(node);
        launchedNode->agentRef1 = agentRef1;
        if (agentRef1 != nullptr)
        {
            agentRef1->collision.leftOut = launched;
        }
    }

    LaunchWithMotion(value44, height, launchedNode, reinterpret_cast<MotionBlock*>(&motion0), &velocity);
}

// The instance 4 units above the target (a receiver's or a designator's position, or the space's with an offset) among those a
// segment up to it hits (32 at most, with bit 4 of their flags) whose squared distance from the instance is the largest beyond
// 1e30 told its support is gone (and given bit 9 of its flags). value1's bits: the receiver (0-7), the designator (8-15), the
// space (16-19), an offset given (21); retail hands bit 20 to the target's search as well, which doesn't read it
void UnsupportAboveCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    constexpr u32 WantedFlags = 0x10;
    constexpr u32 SpaceShift = 16;
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 OffsetGiven = 0x200000;
    constexpr f32 Height = 4.0f;
    InstanceContext* instance = NodeOf(runner)->owner;
    ChunkData* chunk = instance->chunk;
    void* results[Most];
    InstanceRayHit query;
    MakeQuery(&query, results, Most, WantedFlags);
    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto* offset = (value1 & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&x) : nullptr;
    DesignatedPosition(&target, value1 >> SpaceShift & SpaceMask, runner, offset, value1 >> 8 & 0xFF, value1 & 0xFF);
    Vector4 segment[2];
    segment[0] = target;
    target.y = target.y + Height;
    segment[1] = target;
    SkipInQuery(&query, instance);
    ChunkInstancesRayCast(chunk, segment, 2, &query, 0);
    u16 count = query.count;
    if (count == 0)
    {
        return;
    }

    // Retail bug: the search keeps an instance only when it's farther than the best so far, which starts at 1e30: it finds none
    f32 best = Rounded(1e30);
    InstanceContext* found = nullptr;
    Vector4 position = PositionOf(instance);
    for (u16 index = 0; index < count; index++)
    {
        auto* hit = static_cast<InstanceContext*>(results[index]);
        Vector4 away = PositionOf(hit);
        away.x = away.x - position.x;
        away.y = away.y - position.y;
        away.z = away.z - position.z;
        f32 distance = away.x * away.x + away.y * away.y + away.z * away.z;
        if (best < distance)
        {
            best = distance;
            found = hit;
        }
    }

    if (found == nullptr)
    {
        return;
    }

    Agent* agent = ObjectNodeOf(found)->agent;
    CallVirtual<void>(agent, agent->vtable, UnsupportedSlot);
    if ((found->flags & ReferencedObject::FlagInDrawnCell) == 0)
    {
        found->flags |= ReferencedObject::FlagInDrawnCell;
    }
}

// The instance turned about its own x, y or z axis (bits 0-2) as the instance it hangs from moves along that one's x, y or z axis
// (bits 3-5): the degrees a second times its speed along the axis, for the seconds since the node's last update
void RotateWithLinkedCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 TurnsX = 0x1;
    constexpr u32 TurnsY = 0x2;
    constexpr u32 TurnsZ = 0x4;
    constexpr u32 AlongX = 0x8;
    constexpr u32 AlongY = 0x10;
    constexpr u32 AlongZ = 0x20;
    constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* parent = node->owner->parent;
    if (parent == nullptr)
    {
        return;
    }

    Vector4 velocity = ObjectNodeOf(parent)->motion->velocity;
    // Retail bug: without an axis bit the axis is what the stack held there (taken here as none, which turns nothing)
    Vector4 axis = {0.0f, 0.0f, 0.0f, 0.0f};
    u32 bits = static_cast<u32>(axes.raw);
    if ((bits & (AlongX | AlongY | AlongZ)) != 0)
    {
        ObjectPlace* place = parent->place;
        RotateAndTranslate(place);
        axis = *RowOf(&place->matrix, (bits & AlongX) != 0 ? 0 : (bits & AlongY) != 0 ? 1 : 2);
    }

    axis.w = 1.0f;
    f32 inverse = InverseLength(&axis, LengthEpsilon);
    axis.x = axis.x * inverse;
    axis.y = axis.y * inverse;
    axis.z = axis.z * inverse;
    f32 along = axis.x * velocity.x + axis.y * velocity.y + axis.z * velocity.z;
    f32 seconds = node->time != 0 ? static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit : 0.0f;
    s32 angle;
    AngleFrom(&angle, along * degreesPerSecond * seconds, AngleDegrees);
    bits = static_cast<u32>(axes.raw);
    u32 turnAxis;
    if ((bits & TurnsX) != 0)
    {
        turnAxis = 0;
    }
    else if ((bits & TurnsY) != 0)
    {
        turnAxis = 1;
    }
    else if ((bits & TurnsZ) != 0)
    {
        turnAxis = 2;
    }
    else
    {
        return;
    }

    InstanceContext* instance = NodeOf(runner)->owner;
    if (TurnInstance(instance, angle, turnAxis))
    {
        QueueObject(instance);
    }
}

// The force (x, y and z, w 1; kept in value6 to value9) given to the volume controller of the first awake instance whose box
// overlaps the instance's (32 at most), turned on (value10's bit 0) or off (bit 1)
void ForceVolumeControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    constexpr u32 TurnsOn = 0x1;
    constexpr u32 TurnsOff = 0x2;
    constexpr u64 On = 0x1;
    constexpr u32 VolumeControllers = 1 << VolumeControllerKind;
    ObjectNode* node = NodeOf(runner);
    PropertyHolder* properties = node->PacketProperties();
    f32 forceX = x.FloatWith(properties);
    f32 forceY = y.FloatWith(properties);
    f32 forceZ = z.FloatWith(properties);
    auto* force = reinterpret_cast<Vector4*>(&value6);
    force->x = forceX;
    force->z = forceZ;
    force->w = 1.0f;
    force->y = forceY;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceRayHit query;
    MakeQuery(&query, results, Most, 0);
    if (QueryChunkInstances(instance->chunk, &instance->collision.box, VolumeControllers, &query) == 0)
    {
        return;
    }

    auto* volume = static_cast<VolumeControllerNode*>(GetGameNode(&static_cast<InstanceContext*>(results[0])->nodes,
                                                                  VolumeControllerKind));
    volume->force = *force;
    u32 bits = static_cast<u32>(value10.raw);
    if ((bits & TurnsOn) != 0)
    {
        volume->bits = (volume->bits & ~On) | On;
    }
    else if ((bits & TurnsOff) != 0)
    {
        volume->bits &= ~On;
    }
}

// The user message (messageTarget's bits 0-15) sent from the instance to: every linked object (messageFlags' bit 17), the linked
// objects a field of value1 picks (bit 24: a bit each, value2 bits wide, the field's index bits 25-29), the linked objects of an
// object (messageFlags' low half, 0xFF none; only the first when messageTarget's bits 16-23 are 0), the linked object of an index
// (bit 16: messageTarget's bits 16-23, the current one with bit 18, the last with bit 20) or a designator's instance (a
// receiver's below 0xDE). Bit 31 sends only to linked objects on the attachments' path, bit 30 only to those off it; bit 19
// unlinks the linked objects aimed at (all of them, the field's or the object's whether they got it or not, the index's only when
// it did, the designator's when it's linked)
void SendUserMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 NoObject = 0xFF;
    constexpr u32 ByIndex = 0x10000;
    constexpr u32 EveryLinked = 0x20000;
    constexpr u32 CurrentLinked = 0x40000;
    constexpr u32 Unlinks = 0x80000;
    constexpr u32 LastLinked = 0x100000;
    constexpr u32 ByField = 0x1000000;
    constexpr u32 FieldShift = 25;
    constexpr u32 FieldMask = 0x1F;
    constexpr u32 OnlyOffPath = 0x40000000;
    constexpr u32 OnlyOnPath = 0x80000000;
    constexpr u32 LastReceiver = 0xDE;
    constexpr u32 FieldLimit = 0x20;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u16 message = static_cast<u16>(messageTarget);
    // Whether a linked object gets the message: bit 31 takes only those on the attachments' path, bit 30 only those off it (the
    // path looked up whatever the bits)
    auto picked = [this](void* attachments, InstanceContext* linked) {
        bool onPath = OnPath(attachments, linked);
        if ((messageFlags & OnlyOnPath) != 0)
        {
            return onPath;
        }

        return (messageFlags & OnlyOffPath) == 0 || !onPath;
    };

    InstanceContext* target = nullptr;
    if ((messageFlags & EveryLinked) != 0 || (messageFlags & ByField) != 0 || static_cast<u16>(messageFlags) != NoObject)
    {
        void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
        if (attachments == nullptr)
        {
            return;
        }

        GameEvent* event = MakeMessage(message, instance);
        bool sent = false;
        if ((messageFlags & EveryLinked) != 0)
        {
            for (u32 index = 0; index < LinkedCount(attachments); index++)
            {
                InstanceContext* linked = LinkedInstances(attachments)[index];
                if (linked != nullptr && picked(attachments, linked))
                {
                    SendMessage(event, linked);
                    sent = true;
                }
            }

            if ((messageFlags & Unlinks) != 0)
            {
                UnlinkAll(attachments);
            }
        }
        else if ((messageFlags & ByField) != 0)
        {
            u32 field = static_cast<u32>(value1.IntWith(node->PacketProperties()));
            s32 width = value2.IntWith(node->PacketProperties());
            u32 shift = static_cast<u32>(width) * (messageFlags >> FieldShift & FieldMask);
            field >>= shift & 31;
            field &= static_cast<u32>(static_cast<s32>(PowerOf(2.0f, width)) - 1);
            for (u32 index = 0; index < LinkedCount(attachments) && index < FieldLimit; index++, field >>= 1)
            {
                InstanceContext* linked = LinkedInstances(attachments)[index];
                if ((field & 1) == 0 || linked == nullptr)
                {
                    continue;
                }

                if (picked(attachments, linked))
                {
                    SendMessage(event, linked);
                    sent = true;
                }

                if ((messageFlags & Unlinks) != 0)
                {
                    UnlinkInstance(attachments, linked, 0, 1, 0);
                }
            }
        }
        else
        {
            bool firstOnly = static_cast<u8>(messageTarget >> 16) == 0;
            bool more = true;
            for (u32 index = 0; index < LinkedCount(attachments) && more; index++)
            {
                InstanceContext* linked = LinkedInstances(attachments)[index];
                if (linked == nullptr)
                {
                    continue;
                }

                u16 object = ObjectNodeOf(linked)->agent->objectId;
                if (static_cast<u16>(messageFlags) != (object & 0x7FFF))
                {
                    continue;
                }

                if (picked(attachments, linked))
                {
                    SendMessage(event, linked);
                    sent = true;
                    if (firstOnly)
                    {
                        more = false;
                    }
                }

                if ((messageFlags & Unlinks) != 0)
                {
                    UnlinkInstance(attachments, linked, 0, 1, 0);
                }
            }
        }

        if (!sent)
        {
            DestroyMessage(event);
        }

        return;
    }

    if ((messageFlags & ByIndex) != 0)
    {
        void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
        if (attachments == nullptr)
        {
            return;
        }

        if ((messageFlags & LastLinked) != 0)
        {
            messageTarget = (messageTarget & ~0xFF0000u) | ((LinkedCount(attachments) - 1) & 0xFF) << 16;
        }

        u32 index = (messageFlags & CurrentLinked) != 0 ? *AttachmentsWord(attachments) >> LinkedIndexShift & LinkedIndexMask
                                                        : messageTarget >> 16 & 0xFF;
        target = LinkedInstances(attachments)[index];
        if (target != nullptr && !picked(attachments, target))
        {
            target = nullptr;
        }

        if ((messageFlags & Unlinks) != 0)
        {
            UnlinkInstance(attachments, target, 0, 1, 0);
        }
    }
    else
    {
        u8 designator = static_cast<u8>(messageTarget >> 16);
        if (designator < LastReceiver)
        {
            StarterReceivers* receivers = runner->receivers;
            target = receivers != nullptr ? receivers->instances[designator] : nullptr;
        }
        else
        {
            target = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, u32{designator});
            if ((messageFlags & Unlinks) != 0)
            {
                void* attachments = GetGameNode(&instance->nodes, AttachmentsKind);
                if (attachments != nullptr && IndexOfLinked(attachments, target) != -1)
                {
                    UnlinkInstance(attachments, target, 0, 1, 0);
                }
            }
        }
    }

    if (target != nullptr)
    {
        SendMessage(MakeMessage(message, instance), target);
    }
}

// The message (messageTarget's bits 0-10) sent to the awake instances in a sphere (128 at most, the instance left out) around a
// target (a receiver's or a designator's position, or the space's with an offset; the radius its w), only those of an object when
// one is given (unknown10's low half, 0xFFFF none). messageTarget's bits: an offset given (11), the space (12-15), the receiver
// (16-23), the designator (24-31)
void BroadcastUserMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x80;
    constexpr u32 MessageMask = 0x7FF;
    constexpr u32 OffsetGiven = 0x800;
    constexpr u32 SpaceShift = 12;
    constexpr u32 SpaceMask = 0xF;
    constexpr u16 EveryObject = 0xFFFF;
    InstanceContext* instance = NodeOf(runner)->owner;
    ChunkData* chunk = instance->chunk;
    Vector4 sphere = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto* offset = (messageTarget & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&x) : nullptr;
    DesignatedPosition(&sphere, messageTarget >> SpaceShift & SpaceMask, runner, offset, messageTarget >> 24,
                       messageTarget >> 16 & 0xFF);
    void* results[Most];
    InstanceRayHit query;
    MakeQuery(&query, results, Most, 0);
    SkipInQuery(&query, instance);
    sphere.w = radius;
    u32 count = ChunkInstancesInSphere(chunk, &sphere, 2, &query, 1);
    if (count == 0)
    {
        return;
    }

    u16 object = static_cast<u16>(unknown10);
    GameEvent* event = nullptr;
    for (u16 index = 0; index < count; index++)
    {
        auto* found = static_cast<InstanceContext*>(results[index]);
        if (object != EveryObject && (ObjectNodeOf(found)->agent->objectId & 0x7FFF) != object)
        {
            continue;
        }

        if (event == nullptr)
        {
            event = MakeMessage(messageTarget & MessageMask, instance);
        }

        SendMessage(event, static_cast<InstanceContext*>(results[index]));
    }
}

// The message (bits 0-15) sent to the chunk's awake instances with bit 1 of their flags (1024 at most, the instance left out)
// whose object node's byte 0x8C is the same as the instance's (mode 0), the byte the scripts set (1), below it (2) or not above
// it (3); none while the scripts' byte is 0xFF
void TriggerInstancesInRangeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x400;
    constexpr u32 Flags = 0x2;
    constexpr u32 ModeShift = 16;
    constexpr u8 NoByte = 0xFF;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceRayHit query;
    MakeQuery(&query, reinterpret_cast<void**>(g_RangeResults), Most, 0);
    SkipInQuery(&query, instance);
    GameEvent* message = MakeMessage(static_cast<u16>(event), instance);
    s32 count = QueryChunkInstancesByFlags(instance->chunk, Flags, &query);
    u8 mode = static_cast<u8>(event >> ModeShift);
    u8 own = node->unknown8C;
    u8 global = g_GlobalByte30A0E9;
    bool sent = false;
    if (mode < 4 && global != NoByte)
    {
        for (u16 index = 0; index < static_cast<u32>(count); index++)
        {
            InstanceContext* found = g_RangeResults[index];
            u8 byte = static_cast<ObjectNodeBase*>(GetGameNode(&found->nodes, ObjectNodeKind))->unknown8C;
            bool picked;
            switch (mode)
            {
            case 0:
                picked = byte == own;
                break;
            case 1:
                picked = byte == global;
                break;
            case 2:
                picked = byte < global;
                break;
            default:
                picked = byte <= global;
                break;
            }

            if (picked)
            {
                sent = true;
                SendMessage(message, g_RangeResults[index]);
            }
        }
    }

    if (!sent)
    {
        DestroyMessage(message);
    }
}

void InitCommandStatics(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_StaticWord30A950 = 0;
    for (Reference*& receiver : g_ReceiverInstances)
    {
        receiver = nullptr;
    }
}

void CommandsStaticInit()
{
    InitCommandStatics(1, 0xFFFF);
}

void SetReceiverInstance(u32 index, InstanceContext* instance)
{
    AssignReference(&g_ReceiverInstances[static_cast<u8>(index)], instance);
}
