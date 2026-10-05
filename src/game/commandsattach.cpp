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
#include "game/objects.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"
#include "game/scenery.h"

// The commands that attach instances to an agent's instance and let them go (dropped, thrown, launched), link and spring them,
// turn an instance with the one it hangs from, set volume controllers, send user messages and trigger the instances around; and
// the module's statics

extern "C"
{
    // The chunk manager's chunk of an index
    extern void* G_ChunkManager;
    // The instances TriggerInstancesByRank finds, and the rank it compares with (SetTriggerRank's)
    extern InstanceContext* g_RankResults[0x400] RETAIL(D_003B5ED0);
    extern u8 g_TriggerRank RETAIL(D_0030A0E9);
    // A word of the module's statics nothing reads
    extern u32 g_UnusedCommandsWord RETAIL(D_0030A950);
    // The module's statics made, and the static constructor that runs it
    void InitCommandStatics(u32 initialise, u32 priority) RETAIL(FUN_002518b0);
    void CommandsStaticInit() RETAIL(FUN_00255430);
}

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

bool Asleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
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

bool OnPath(const AttachmentsNode* attachments, InstanceContext* instance)
{
    AttachmentsPath* path = attachments->path;
    return path != nullptr && PathIndexOf(path, instance) != -1;
}

// The node a rigid body moves (retail reads it with the body's state, as 64 bits)
ObjectNode* NodeOfBody(ObjectRigidBody* body)
{
    return body->node;
}

// A user message from an instance (a reference to it its argument), and one sent to an instance (a reference to it its handle)
GameEvent* MakeMessage(u32 message, InstanceContext* sender)
{
    Reference* argument = sender != nullptr ? AddReference(sender) : nullptr;
    return GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &argument, ObjectNodeKinds);
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
        CallVirtual<void>(event, event->vtable, GameEvent::DestroySlot, u32{DestroyAndFree});
    }
}

// A query of the awake instances of a chunk (the commands' own: no instances left out)
void MakeQuery(InstanceQuery* query, void** results, u16 most, u32 wantedFlags)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = wantedFlags;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
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

// The instance let go of what holds it (its parent forgotten, no longer busy when asked)
void LetGo(InstanceContext* instance, bool clearBusy)
{
    if (clearBusy)
    {
        instance->flags.busy = 0;
    }

    instance->parent = nullptr;
    instance->flags.attached = 0;
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
    place->MarkTurned();
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

void LinkTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceContext* linked = nullptr;
    switch (target.kind)
    {
    case SlotFocus:
        // Retail bug: without a focus instance the target is what the caller left in $s0, the command itself
        // (RunBodyCommandsAndJump's), which gets linked as an instance
        if (!node->flags.focusInstance || node->focusInstance == nullptr)
        {
            linked = reinterpret_cast<InstanceContext*>(this);
        }
        else
        {
            linked = node->AwakeFocus();
        }

        break;
    case SlotAgentRef1:
        linked = AwakeAgentRef1(node);
        break;
    case SlotAgentRef2:
        linked = AwakeAgentRef2(node);
        break;
    default:
        break;
    }

    if (instance != nullptr && linked != nullptr)
    {
        LinkInstance(AttachmentsOf(instance), linked, 0);
    }
}

// The target (the focus, AgentRef1 or AgentRef2), or a linked object, attached to the instance (or the instance to the focus) at
// an exit point, with an offset and angles when given; one that takes packets first has its rigid body taken out of the physics
// world (its hold modes above 8 made 1, the physics body flag of its node's instance cleared without a motion block) and its
// trajectory controller and motion block let go
void AttachFocusObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* attached = nullptr;
    InstanceContext* holder = nullptr;
    u32 linked = flags.linked;
    if (linked != AttachFocusFlags::NoLinked)
    {
        holder = NodeOf(runner)->owner;
        AttachmentsNode* attachments = AttachmentsNodeOf(holder);
        if (attachments == nullptr || linked >= attachments->LinkedCount())
        {
            return;
        }

        attached = LinkedInstances(attachments)[linked];
    }
    else
    {
        ObjectNode* node = NodeOf(runner);
        switch (flags.target)
        {
        case SlotFocus:
            if (flags.toFocus != 0)
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
        case SlotAgentRef1:
            holder = node->owner;
            attached = AwakeAgentRef1(node);
            break;
        case SlotAgentRef2:
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
    if (attachedNode == nullptr || attached->flags.attached)
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

            // Its physics body's kinds, which it lost, made plain
            if (body->bits.collisionKind >= FirstPhysicsBodyKind)
            {
                body->bits.collisionKind = BodyKindPlain;
            }

            if (body->bits.motionKind >= FirstPhysicsBodyKind)
            {
                body->bits.motionKind = BodyKindPlain;
            }

            ObjectNode* bodyNode = NodeOfBody(body);
            if (bodyNode->motionBlock == nullptr)
            {
                bodyNode->owner->flags.physicsBody = 0;
            }
        }

        attachedNode->ReleaseTrajectory();
        attachedNode->ReleaseMotionBlock();
    }

    u32 exitPoint = flags.exitPoint;
    u32 done;
    if (flags.offsetGiven != 0 || flags.anglesGiven != 0)
    {
        Matrix4x4 frame;
        if (flags.anglesGiven != 0)
        {
            s32 x = angleX;
            s32 y = angleY;
            s32 z = angleZ;
            MatrixFromAngles(&frame, &x, &y, &z);
        }
        else
        {
            InitIdentityMatrix(&frame);
        }

        *RowOf(&frame, 3) = offset;
        done = AttachToAgent(agent, attached, 1, exitPoint, &frame);
    }
    else
    {
        done = AttachToAgent(agent, attached, 1, exitPoint, nullptr);
    }

    if (done == 0)
    {
        return;
    }

    holder->flags.hasAttachment = 1;
    attached->collision.leftOut = holder;
    holder->collision.leftOut = attached;
    if (flags.marksBusy != 0)
    {
        attached->flags.busy = 1;
    }

    attached->parent = holder;
    attached->flags.attached = 1;
    u32 follow = flags.follow;
    if (follow == Attachment::FollowsPlace)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(holder);
    if (attachments == nullptr)
    {
        return;
    }

    // A hanging attachment is an instance held whatever kind it was made
    if (follow == Attachment::FollowsHanging && g_LastAttachment != nullptr)
    {
        g_LastAttachment->bits.kind = Attachment::KindInstance;
    }

    SetLastAttachmentMode(attachments, follow);
}

// A spring attached to the instance (its attachments made when it has none) from its joint or its place (an offset turned by its
// place added when given) to a target: a point in the focus's space at its joint (no spring without an awake focus or when the
// instance is its own focus) or in the space's, its rest length given or the distance now, with an instance of an object made at
// its start when one is given (without collisions when asked)
void AttachSpringCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    void* attachments = AttachmentsOf(instance);
    InstanceContext* focus = nullptr;
    if (flags.toFocus != 0)
    {
        focus = node->AwakeFocus();
        if (focus == nullptr || focus == instance)
        {
            return;
        }
    }

    Vector4 anchor;
    if (focus != nullptr)
    {
        ObjectPlace* place = focus->place;
        RotateAndTranslate(place);
        VuTransformPoint(&place->matrix, &target, &anchor);
    }
    else
    {
        DesignatedPosition(&anchor, flags.space, runner, &target, DesignatesNone, DesignatesNone);
    }

    const Vector4* startOffset = flags.offsetGiven != 0 ? &offset : nullptr;
    u8 joint = joints.joint;
    Vector4 start;
    if (joint != GameOGI::NoJoint)
    {
        JointPosition(instance, joint, &start, startOffset);
    }
    else
    {
        start = PositionOf(instance);
        if (startOffset != nullptr)
        {
            MoveByOffset(&start, instance->place, startOffset);
        }
    }

    f32 restLength;
    if (flags.lengthGiven != 0)
    {
        restLength = length;
    }
    else
    {
        f32 dx = start.x - anchor.x;
        f32 dy = start.y - anchor.y;
        f32 dz = start.z - anchor.z;
        restLength = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    }

    // The spring pulls toward the point in the focus's space
    if (focus != nullptr)
    {
        anchor = target;
    }

    u8 focusJoint = joints.focusJoint;
    if (flags.object == NoObjectId)
    {
        AttachSpring(power, damping, restLength, attachments, instance, &anchor, joint, startOffset, nullptr, focus, focusJoint);
        return;
    }

    ChunkEntry* chunk = ChunkOfIndex(G_ChunkManager, node->agent->chunkIndex);
    InstanceFactory* factory = g_InstanceFactory;
    factory->SetInstanceProperties();
    factory->ClearUnused1();
    factory->SetGivesIds();
    factory->ClearGivesFlagSlots();
    factory->creationFlags = 0;
    s32 angles[3] = {0, 0, 0};
    InstanceContext* end = CreateInstance(factory, chunk, flags.object, &start, angles);
    AttachSpring(power, damping, restLength, attachments, instance, &anchor, joint, startOffset, end, focus, focusJoint);
    if (flags.endUncollidable != 0)
    {
        end->flags.collisionActive = 0;
    }
}

// The instance at an exit point (or the one at none) let go of, held no longer (the instance holds none once it has no linked
// objects), pushed off first when it has a physics body, then launched or made to fall as the mode says and sent the message
void DropAttachedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 PushStrength = Rounded(0.1);
    InstanceContext* instance = NodeOf(runner)->owner;
    if (!instance->flags.hasAttachment)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    u32 mode = request.mode;
    u8 exitPoint = request.exitPoint;
    // Retail bug: the launches take this velocity, which nothing fills (retail hands it to the detach, which never writes it):
    // they go with what the stack held
    Vector4 velocity;
    InstanceContext* dropped = nullptr;
    if (mode < DropRequest::Modes)
    {
        dropped = exitPoint != GameOGI::NoExitPoint ? DetachExitPoint(attachments, exitPoint, 0, 0)
                                                    : DetachUnslotted(attachments);
    }

    if (dropped == nullptr)
    {
        return;
    }

    if (attachments->LinkedCount() == 0)
    {
        instance->flags.hasAttachment = 0;
    }

    LetGo(dropped, request.clearsBusy != 0);
    ObjectNode* droppedNode = ObjectNodeOf(dropped);
    if (droppedNode != nullptr)
    {
        if (dropped->flags.physicsBody)
        {
            CallVirtual<void>(droppedNode, droppedNode->vtable, ObjectNode::PushSlot, PushStrength, instance);
        }

        Agent* agent;
        switch (mode)
        {
        case DropRequest::Launched:
            CallVirtual<void>(droppedNode, droppedNode->vtable, ObjectNode::LaunchSlot, LaunchWithDefaultGravity, &velocity);
            break;
        case DropRequest::Falls:
            agent = droppedNode->agent;
            CallVirtual<void>(agent, agent->vtable, Agent::StartFallSlot);
            break;
        case DropRequest::FallsIfCrate:
            agent = droppedNode->agent;
            if (CallVirtual<u32>(agent, agent->vtable, Agent::IsCrateSlot) != 0)
            {
                agent = droppedNode->agent;
                CallVirtual<void>(agent, agent->vtable, Agent::StartFallSlot);
            }
            else
            {
                CallVirtual<void>(droppedNode, droppedNode->vtable, ObjectNode::LaunchSlot, LaunchWithDefaultGravity, &velocity);
            }

            break;
        default:
            break;
        }
    }

    if (request.message != NoMessage)
    {
        SendMessage(MakeMessage(request.message, instance), dropped);
    }
}

// AgentRef2 unlinked from the instance (when it holds others) and sent the message. The attachments aren't checked: an instance
// that holds others has them
void ReleaseAgentRef2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if (!instance->flags.hasAttachment)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    InstanceContext* released = AwakeAgentRef2(node);
    if (released == nullptr)
    {
        return;
    }

    UnlinkInstance(attachments, released, 0, 1, 0);
    if (release.message != NoMessage)
    {
        SendMessage(MakeMessage(release.message, instance), released);
    }
}

// The instance at an exit point (or the one at none) let go of and thrown at a target (a receiver's or a designator's position,
// or the space's with an offset, moved at random within a spread) at a speed under a gravity (30 when it's negative)
void ThrowAttachedObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if (!instance->flags.hasAttachment)
    {
        return;
    }

    AttachmentsNode* attachments = AttachmentsNodeOf(instance);
    u8 exitPoint = request.exitPoint;
    InstanceContext* thrown =
        exitPoint != GameOGI::NoExitPoint ? DetachExitPoint(attachments, exitPoint, 0, 0) : DetachUnslotted(attachments);
    if (thrown == nullptr)
    {
        return;
    }

    instance->flags.hasAttachment = 0;
    LetGo(thrown, request.clearsBusy != 0);
    ObjectNode* thrownNode = ObjectNodeOf(thrown);
    if (thrownNode == nullptr)
    {
        return;
    }

    f32 down = gravity.FloatWith(node->PacketProperties());
    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatedPosition(&target, request.space, runner, request.offsetGiven != 0 ? &offset : nullptr, request.designator,
                       request.receiver);
    if (request.spreadGiven != 0)
    {
        JitterVector(spread, 1.0f, 1.0f, 1.0f, &target);
    }

    Vector4 position = PositionOf(thrown);
    f32 throwSpeed = speed.FloatWith(node->PacketProperties());
    Vector4 velocity = ThrowVelocity(&position, &target, throwSpeed, 0.0f <= down ? down : DefaultLaunchGravity);
    CallVirtual<void>(thrownNode, thrownNode->vtable, ObjectNode::LaunchSlot, down, &velocity);
}

// AgentRef2 let go of (unlinked when the instance holds others: it holds none once it has no linked objects left) and launched by
// the motion block at a target (as the throw's), at a speed under the block's gravity, given AgentRef1 when asked (whose queries
// then leave it out)
void LaunchAgentRef2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceContext* launched = AwakeAgentRef2(node);
    if (launched == nullptr)
    {
        return;
    }

    if (instance->flags.hasAttachment)
    {
        AttachmentsNode* attachments = AttachmentsNodeOf(instance);
        if (UnlinkInstance(attachments, launched, 0, 1, 0) != 0 && attachments->LinkedCount() == 0)
        {
            instance->flags.hasAttachment = 0;
        }
    }

    LetGo(launched, flags.clearsBusy != 0);
    ObjectNode* launchedNode = ObjectNodeOf(launched);
    if (launchedNode == nullptr)
    {
        return;
    }

    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatedPosition(&target, launch.space, runner, flags.offsetGiven != 0 ? &offset : nullptr, launch.designator,
                       launch.receiver);
    if (flags.spreadGiven != 0)
    {
        JitterVector(spread, 1.0f, 1.0f, 1.0f, &target);
    }

    Vector4 position = PositionOf(launched);
    f32 launchSpeed = speed.FloatWith(node->PacketProperties());
    Vector4 velocity = ThrowVelocity(&position, &target, launchSpeed, block.gravity);
    if (flags.passesAgentRef1 != 0)
    {
        InstanceContext* agentRef1 = AwakeAgentRef1(node);
        launchedNode->agentRef1 = agentRef1;
        if (agentRef1 != nullptr)
        {
            agentRef1->collision.leftOut = launched;
        }
    }

    LaunchWithMotion(spinY, spinX, launchedNode, &block, &velocity);
}

// The instance 4 units above the target (a receiver's or a designator's position, or the space's with an offset) among those a
// segment up to it hits (32 at most, those whose collision is on) whose squared distance from the instance is the largest beyond
// 1e30 made to fall (and marked as in a drawn cell)
void UnsupportAboveCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    constexpr f32 Height = 4.0f;
    InstanceContext* instance = NodeOf(runner)->owner;
    ChunkData* chunk = instance->chunk;
    void* results[Most];
    InstanceQuery query;
    MakeQuery(&query, results, Most, ReferencedObjectFlags::CollisionActive);
    Vector4 target = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatedPosition(&target, request.space, runner, request.offsetGiven != 0 ? &offset : nullptr, request.designator,
                       request.receiver);
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
    f32 best = Infinite;
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
    CallVirtual<void>(agent, agent->vtable, Agent::StartFallSlot);
    if (!found->flags.inDrawnCell)
    {
        found->flags.inDrawnCell = 1;
    }
}

// The instance turned about its own x, y or z axis (bits 0-2) as the instance it hangs from moves along that one's x, y or z axis
// (bits 3-5): the degrees a second times its speed along the axis, for the seconds since the node's last update
void RotateWithLinkedCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* parent = node->owner->parent;
    if (parent == nullptr)
    {
        return;
    }

    Vector4 velocity = ObjectNodeOf(parent)->motion->velocity;
    // Retail bug: without an axis bit the axis is what the stack held there (taken here as none, which turns nothing)
    Vector4 axis = {0.0f, 0.0f, 0.0f, 0.0f};
    if (axes.alongX != 0 || axes.alongY != 0 || axes.alongZ != 0)
    {
        ObjectPlace* place = parent->place;
        RotateAndTranslate(place);
        axis = *RowOf(&place->matrix, axes.alongX != 0 ? 0 : axes.alongY != 0 ? 1 : 2);
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
    u32 turnAxis;
    if (axes.turnsX != 0)
    {
        turnAxis = 0;
    }
    else if (axes.turnsY != 0)
    {
        turnAxis = 1;
    }
    else if (axes.turnsZ != 0)
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

// The force (w 1, kept in the command) given to the volume controller of the first awake instance whose box overlaps the
// instance's (32 at most), turned on or off
void ForceVolumeControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x20;
    ObjectNode* node = NodeOf(runner);
    PropertyHolder* properties = node->PacketProperties();
    f32 x = forceX.FloatWith(properties);
    f32 y = forceY.FloatWith(properties);
    f32 z = forceZ.FloatWith(properties);
    force.x = x;
    force.z = z;
    force.w = 1.0f;
    force.y = y;
    InstanceContext* instance = NodeOf(runner)->owner;
    void* results[Most];
    InstanceQuery query;
    MakeQuery(&query, results, Most, 0);
    if (QueryChunkInstances(instance->chunk, &instance->collision.box, 1u << NodeMessageTrigger, &query) == 0)
    {
        return;
    }

    auto* volume = static_cast<MessageTriggerNode*>(GetGameNode(&static_cast<InstanceContext*>(results[0])->nodes,
                                                                NodeMessageTrigger));
    volume->force = force;
    if (turns.on != 0)
    {
        volume->messageBits.forceOn = 1;
    }
    else if (turns.off != 0)
    {
        volume->messageBits.forceOn = 0;
    }
}

// The user message sent from the instance to: every linked object, the linked objects a field of bits picks (a bit each, the
// field's index times its width the shift to it), the linked objects of an object (only the first when the recipient is 0), the
// linked object of an index (the recipient's, the current one or the last) or a designator's instance (a receiver's below 0xDE).
// The linked objects aimed at are unlinked when asked (all of them, the field's or the object's whether they got it or not, the
// index's only when it did, the designator's when it's linked)
void SendUserMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FieldLimit = 0x20;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    u16 message = target.message;
    // Whether a linked object gets the message: only those on the attachments' path, or only those off it (the path looked up
    // whatever the bits)
    auto picked = [this](const AttachmentsNode* attachments, InstanceContext* linked) {
        bool onPath = OnPath(attachments, linked);
        if (flags.onlyOnPath != 0)
        {
            return onPath;
        }

        return flags.onlyOffPath == 0 || !onPath;
    };

    InstanceContext* recipient = nullptr;
    if (flags.everyLinked != 0 || flags.byField != 0 || flags.object != NoLinkedObjectId)
    {
        AttachmentsNode* attachments = AttachmentsNodeOf(instance);
        if (attachments == nullptr)
        {
            return;
        }

        GameEvent* event = MakeMessage(message, instance);
        bool sent = false;
        if (flags.everyLinked != 0)
        {
            for (u32 index = 0; index < attachments->LinkedCount(); index++)
            {
                InstanceContext* linked = LinkedInstances(attachments)[index];
                if (linked != nullptr && picked(attachments, linked))
                {
                    SendMessage(event, linked);
                    sent = true;
                }
            }

            if (flags.unlinks != 0)
            {
                UnlinkAll(attachments);
            }
        }
        else if (flags.byField != 0)
        {
            u32 field = static_cast<u32>(fieldBits.IntWith(node->PacketProperties()));
            s32 width = fieldWidth.IntWith(node->PacketProperties());
            u32 shift = static_cast<u32>(width) * flags.field;
            field >>= shift & 31;
            field &= static_cast<u32>(static_cast<s32>(PowerOf(2.0f, width)) - 1);
            for (u32 index = 0; index < attachments->LinkedCount() && index < FieldLimit; index++, field >>= 1)
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

                if (flags.unlinks != 0)
                {
                    UnlinkInstance(attachments, linked, 0, 1, 0);
                }
            }
        }
        else
        {
            bool firstOnly = target.recipient == 0;
            bool more = true;
            for (u32 index = 0; index < attachments->LinkedCount() && more; index++)
            {
                InstanceContext* linked = LinkedInstances(attachments)[index];
                if (linked == nullptr)
                {
                    continue;
                }

                u16 object = ObjectNodeOf(linked)->agent->objectId;
                if (flags.object != (object & ResourceIndexMask))
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

                if (flags.unlinks != 0)
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

    if (flags.byIndex != 0)
    {
        AttachmentsNode* attachments = AttachmentsNodeOf(instance);
        if (attachments == nullptr)
        {
            return;
        }

        // The last one is kept in the command's recipient
        if (flags.lastLinked != 0)
        {
            target.recipient = attachments->LinkedCount() - 1;
        }

        u32 index = flags.currentLinked != 0 ? CurrentLinked(attachments) : target.recipient;
        recipient = LinkedInstances(attachments)[index];
        if (recipient != nullptr && !picked(attachments, recipient))
        {
            recipient = nullptr;
        }

        if (flags.unlinks != 0)
        {
            UnlinkInstance(attachments, recipient, 0, 1, 0);
        }
    }
    else
    {
        u8 designator = target.recipient;
        if (designator < FirstDesignator)
        {
            StarterReceivers* receivers = runner->receivers;
            recipient = receivers != nullptr ? receivers->instances[designator] : nullptr;
        }
        else
        {
            recipient = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, u32{designator});
            if (flags.unlinks != 0)
            {
                AttachmentsNode* attachments = AttachmentsNodeOf(instance);
                if (attachments != nullptr && IndexOfLinked(attachments, recipient) != -1)
                {
                    UnlinkInstance(attachments, recipient, 0, 1, 0);
                }
            }
        }
    }

    if (recipient != nullptr)
    {
        SendMessage(MakeMessage(message, instance), recipient);
    }
}

// The message sent to the awake instances in a sphere (128 at most, the instance left out) around a target (a receiver's or a
// designator's position, or the space's with an offset), only those of an object when one is given
void BroadcastUserMessageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x80;
    InstanceContext* instance = NodeOf(runner)->owner;
    ChunkData* chunk = instance->chunk;
    Vector4 sphere = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatedPosition(&sphere, target.space, runner, target.offsetGiven != 0 ? &offset : nullptr, target.designator,
                       target.receiver);
    void* results[Most];
    InstanceQuery query;
    MakeQuery(&query, results, Most, 0);
    SkipInQuery(&query, instance);
    sphere.w = radius;
    u32 count = ChunkInstancesInSphere(chunk, &sphere, ObjectNodeKinds, &query, 1);
    if (count == 0)
    {
        return;
    }

    u16 objectId = object.id;
    GameEvent* event = nullptr;
    for (u16 index = 0; index < count; index++)
    {
        auto* found = static_cast<InstanceContext*>(results[index]);
        if (objectId != AnyObjectId && (ObjectNodeOf(found)->agent->objectId & ResourceIndexMask) != objectId)
        {
            continue;
        }

        if (event == nullptr)
        {
            event = MakeMessage(target.message, instance);
        }

        SendMessage(event, static_cast<InstanceContext*>(results[index]));
    }
}

// The message sent to the chunk's awake instances with an object node (1024 at most, the instance left out) of the ranks the mode
// takes; none while the trigger rank is 0xFF
void TriggerInstancesByRankCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x400;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    InstanceQuery query;
    MakeQuery(&query, reinterpret_cast<void**>(g_RankResults), Most, 0);
    SkipInQuery(&query, instance);
    GameEvent* event = MakeMessage(message.message, instance);
    s32 count = QueryChunkInstancesOfKinds(instance->chunk, ObjectNodeKinds, &query);
    u8 mode = message.mode;
    u8 own = node->rank;
    u8 trigger = g_TriggerRank;
    bool sent = false;
    if (mode < RankMessage::Modes && trigger != ObjectNodeBase::NoRank)
    {
        for (u16 index = 0; index < static_cast<u32>(count); index++)
        {
            InstanceContext* found = g_RankResults[index];
            u8 rank = static_cast<ObjectNodeBase*>(GetGameNode(&found->nodes, NodeObject))->rank;
            bool picked;
            switch (mode)
            {
            case RankMessage::SameRank:
                picked = rank == own;
                break;
            case RankMessage::TriggerRank:
                picked = rank == trigger;
                break;
            case RankMessage::BelowTriggerRank:
                picked = rank < trigger;
                break;
            default:
                picked = rank <= trigger;
                break;
            }

            if (picked)
            {
                sent = true;
                SendMessage(event, g_RankResults[index]);
            }
        }
    }

    if (!sent)
    {
        DestroyMessage(event);
    }
}

void InitCommandStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_UnusedCommandsWord = 0;
    for (Reference*& agent : g_GlobalAgents)
    {
        agent = nullptr;
    }
}

void CommandsStaticInit()
{
    InitCommandStatics(1, DefaultInitPriority);
}

void SetGlobalAgent(u32 index, InstanceContext* instance)
{
    AssignReference(&g_GlobalAgents[static_cast<u8>(index)], instance);
}
