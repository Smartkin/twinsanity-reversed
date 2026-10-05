#include "game/objectnode.h"

#include "game/animation.h"
#include "game/attachments.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/context.h"
#include "game/events.h"
#include "game/instanceparticles.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/nodecontrollers.h"
#include "game/objects.h"
#include "game/particles.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"
#include "game/scripttokens.h"
#include "game/sound.h"

#include <cstddef>
#include <cstdint>

// The object node's parts and what works on them: the motion blocks it follows and its trajectory controller's kinds, the head
// tracking, the perception, the particle trails, its prototype's construction and a few of its own functions

extern "C"
{
    // Set by the module's statics and never read
    extern u32 g_ObjectNodeUnused RETAIL(D_0030A940);
    // The module's statics (GCC 2.9x's initialisation function, called for every priority) and its global constructor
    void InitObjectNodeStatics(u32 initialise, u32 priority) RETAIL(FUN_0023bee0);
    void ConstructObjectNodeModule() RETAIL(FUN_00240bc8);
}

EABI_EXPORT(FUN_0023cfd8, JitterVector);
EABI_EXPORT(FUN_0023de38, KnockNode);
EABI_EXPORT(FUN_0023e650, SetContactSounds);
EABI_EXPORT(FUN_0023edd0, SetHeadTrackingPositivePitch);
EABI_EXPORT(FUN_0023ee18, SetHeadTrackingNegativePitch);
EABI_EXPORT(FUN_0023ee60, SetHeadTrackingYawLimit);
EABI_EXPORT(FUN_0023f340, SetUpMotionBlock);
EABI_EXPORT(FUN_0023f638, SetMotionBlockTurnLimit);
EABI_EXPORT(FUN_0023f650, MotionBlockFadeRate);
EABI_EXPORT(FUN_0023f6b0, MotionBlockCycleX);
EABI_EXPORT(FUN_0023f7c0, MotionBlockCycleY);
EABI_EXPORT(FUN_0023f8d0, MotionBlockCycleZ);
EABI_EXPORT(FUN_0023ff38, SetPerceptionWeight);
EABI_EXPORT(FUN_00240010, AddPerceptionWeight);
EABI_EXPORT(FUN_00240948, SlowBodyAlongAxis);
EABI_EXPORT(FUN_00240a78, GrabTouched);
EABI_EXPORT(FUN_00240b68, HoldWithAttachments);

namespace
{
// The margins of the largest cycle's range its fade rate is worked out of: a moving cycle's and a turning one's
constexpr f32 MoveMargin = 0.01f;
constexpr f32 TurnMargin = 0x1.c98714p-10f;

// A cycle about an axis at an angle, as the block's cycle and sign for the axis say
f32 MotionBlockCycle(MotionBlock* block, u32 axis, const s32* angle, f32 range)
{
    f32 value;
    switch (block->motion.CycleOf(axis))
    {
    case MotionBlock::CycleSine:
        value = range * SinOfAngle(angle);
        break;
    case MotionBlock::CycleSquare:
    case MotionBlock::CycleSquareToo:
    {
        s32 turns = static_cast<s32>(block->cycleRates[axis] * (static_cast<f32>(*angle) * AngleToRadians));
        value = (turns & 1) != 0 ? -range : range;
        break;
    }
    case MotionBlock::CycleRandom:
        value = RandomSignedTimes(range);
        break;
    case MotionBlock::CycleAngle:
        value = static_cast<f32>(*angle) * AngleToRadians;
        break;
    default:
        return 0.0f;
    }

    switch (block->motion.SignOf(axis))
    {
    case MotionBlock::SignPositive:
        return __builtin_fabsf(value);
    case MotionBlock::SignNegative:
        return -__builtin_fabsf(value);
    default:
        return value;
    }
}

void UnhookJoints(HeadTracking* tracking, OgiAnimator* animator)
{
    RemoveJointCallback(animator, tracking->bits.joint, tracking);
    tracking->bits.hooked = 0;
    if (tracking->bits.secondJoint != GameOGI::NoJoint)
    {
        RemoveJointCallback(animator, tracking->bits.secondJoint, tracking);
    }
}

Perception* PerceptionOf(void* perception)
{
    return static_cast<Perception*>(perception);
}

const PerceptionSense* SenseOf(const void* sense)
{
    return static_cast<const PerceptionSense*>(sense);
}

// A sense of a kind turned on or off
void SetSenseOn(void* perception, u32 kind, u8 on)
{
    Perception* senses = PerceptionOf(perception);
    u32 count = senses->bits.count;
    for (u32 index = 0; index < count; index++)
    {
        if (senses->senses[index]->bits.kind == kind)
        {
            senses->on[index] = on;
            return;
        }
    }
}

// A trail's kind that measures the turn a packet facing the way it moves makes
constexpr u64 MeasuresTurnKind = 5;

// The flags of a physics body as the retail code reads them, also when there's none (the word at address 0x18 then)
RigidBodyFlags BodyFlagsOf(const DynamicBody* body)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(body) + offsetof(RigidBody, bodyFlags);
    return *reinterpret_cast<const RigidBodyFlags*>(address);
}

bool IsAsleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// The properties a node's object gives it (the node it stands in for has them when there's one)
PropertyHolder* NodeProperties(ObjectNode* node)
{
    return node->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(node->sourceNode) : node->properties;
}

// A rigid body of a kind made for the node (the one it had let go of) for its trajectory, in its chunk's first list
ObjectRigidBody* MakeTrajectoryBody(ObjectNode* node, u32 kind)
{
    node->ReleaseRigidBody();
    ObjectRigidBody* body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
    node->rigidBody = body;
    ListRigidBodyFirst(body, kind);
    return body;
}
}

void ObjectNode::MotionBlockTouched(InstanceContext* other)
{
    MotionBlock* block = motionBlock;
    if (block->flags.sticky && other->flags.collisionActive
        && SticksToMotionBlock(block, other) != 0)
    {
        StickToMotionBlock(block, other);
    }

    if (!block->flags.sendsTouches)
    {
        return;
    }

    // An event of the block's touch message (the low half of its word) from the instance
    Reference* argument = owner != nullptr ? AddReference(owner) : nullptr;
    auto* memory = static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent)));
    GameEvent* event = GameEvent::Construct(memory, static_cast<u16>(block->touchMessage), &argument, ObjectNodeKinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(other, &handle);
}

void InitObjectNodeStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    // The nodes are updated however long their instances weren't seen
    g_ObjectUpdateRate.cutoff = UpdateRate::NoCutoff;
    g_ObjectUpdateRate.slope = 1.0f;
    g_ObjectNodeUnused = 0;
    g_ObjectUpdateRate.grace = 0;
}

void ConstructObjectNodeModule()
{
    InitObjectNodeStatics(1, DefaultInitPriority);
}

AiPosition* AiPosition::Construct(AiPosition* position)
{
    position->links = nullptr;
    position->flags.value = 0;
    position->bits.value = 0;
    position->position = g_DefaultBox.min;
    position->previousPosition = AiPosition::NoPrevious;
    position->position.w = 1.0f;
    return position;
}

void JitterVector(f32 spread, f32 yScale, f32 xScale, f32 zScale, Vector4* vector)
{
    f32 x = RandomSigned() * (spread * xScale);
    f32 y = RandomSigned() * (spread * yScale);
    f32 z = RandomSigned() * (spread * zScale);
    vector->x = vector->x + x;
    vector->y = vector->y + y;
    vector->z = vector->z + z;
}

ObjectNodeBase* ObjectNodeBase::ConstructPrototype(ObjectNodeBase* node)
{
    GameNode::Construct(node);
    node->vtable = g_NodePrototypeVTable;
    InstancePlacement::Construct(&node->information, nullptr);
    node->informationPointer = &node->information;
    node->ownInformation = nullptr;
    SetUndefinedId(&node->ownObjectId);
    node->Reset();
    return node;
}

ObjectNodeBase* ObjectNodeBase::ConstructPrototype(ObjectNodeBase* node, ChunkEntry* chunk)
{
    GameNode::Construct(node);
    node->vtable = g_NodePrototypeVTable;
    ChunkDataReference* data = chunk->data;
    InstancePlacement::Construct(&node->information, data != nullptr ? data->chunk : nullptr);
    node->ownInformation = nullptr;
    node->informationPointer = &node->information;
    SetUndefinedId(&node->ownObjectId);
    node->Reset();
    return node;
}

void SetSoundObject(ObjectNodeBase* node, const GameObject* object)
{
    node->ownObjectId = static_cast<u16>(object->id);
}

void ObjectNodeBase::Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal)
{
    CallVirtual<void>(agent, agent->vtable, Agent::BumpedSlot, other, motion, normal);
}

void ClearComebackPlacement(void* node)
{
    auto* object = static_cast<ObjectNodeBase*>(node);
    InstancePlacement* own = object->ownInformation;
    object->informationPointer = &object->information;
    if (own != nullptr)
    {
        StringDestroy(&own->chunk);
        MemoryDeallocate2_(own);
    }

    object->ownInformation = nullptr;
}

void SetComebackPlacement(void* node, const InstancePlacement* placement)
{
    auto* object = static_cast<ObjectNodeBase*>(node);
    if (object->ownInformation == nullptr)
    {
        object->ownInformation = CopyInstancePlacement(static_cast<InstancePlacement*>(MemoryAllocate(sizeof(InstancePlacement))),
                                                       placement);
    }
    else
    {
        object->ownInformation->Assign(placement);
    }

    object->informationPointer = object->ownInformation;
}

void SetStoredPlace(ObjectNode* node, ObjectPlace* place)
{
    if (node->storedPlace != nullptr)
    {
        CopyObjectPlace(node->storedPlace, place);
    }
    else
    {
        node->storedPlace = AssignObjectPlace(static_cast<ObjectPlace*>(MemoryAllocate(sizeof(ObjectPlace))), place);
    }
}

void SetStoredPosition(ObjectNode* node, const Vector4* position)
{
    node->flags.storedPosition = 1;
    node->storedPosition = *position;
}

void ObjectNode::HitWhileMoving(void* other, const Vector4* point, const Vector4* impulse)
{
    // A squared impulse the rigid body is pushed by past this
    constexpr f32 LeastPush = 0x1.0c6f7cp-20f;
    MotionBlock* block = motionBlock;
    if (block == nullptr)
    {
        return;
    }

    f32 strength = impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z;
    if (trajectory != nullptr && trajectory->followed == block)
    {
        // Riding it, the physics body (when the world leaves it out) comes back and takes the impulse
        ObjectRigidBody* body = rigidBody;
        if (body != nullptr && BodyFlagsOf(body->physicsBody).leftOut != 0)
        {
            body->physicsBody->ReleaseRide();
            if (LeastPush < strength)
            {
                PushRigidBody(rigidBody, impulse, point);
            }
        }
    }
    else if (block->flags.followedWhenTouched)
    {
        TimeClock* clock = GetContextClock(owner);
        FollowMotionBlock(motionBlock, clock);
    }

    MotionBlockTouched(static_cast<InstanceContext*>(other));
}

void FollowOwnMotionBlock(ObjectNode* node)
{
    MotionBlock* block = node->motionBlock;
    if (block == nullptr)
    {
        return;
    }

    Trajectory* trajectory = node->trajectory;
    if (trajectory != nullptr && trajectory->followed == block)
    {
        return;
    }

    TimeClock* clock = GetContextClock(node->owner);
    node->FollowMotionBlock(node->motionBlock, clock);
}

void KnockNode(f32 strength, ObjectNode* node)
{
    // Knocks past these strengths raise the countdown by 4, 3, 2 and 1, which keeps it at most 255
    constexpr f32 HardestKnock = 6.4e-11f;
    constexpr f32 HardKnock = 1.6e-11f;
    constexpr f32 Knock = 4e-12f;
    constexpr f32 SoftKnock = 1e-12f;
    constexpr u32 MostCountdown = 0xFF;
    ObjectNodeReactions& reactions = node->reactions;
    if (HardestKnock < strength)
    {
        if (reactions.knockCountdown < MostCountdown - 3)
        {
            reactions.knockCountdown = reactions.knockCountdown + 4;
        }
    }
    else if (HardKnock < strength)
    {
        if (reactions.knockCountdown < MostCountdown - 2)
        {
            reactions.knockCountdown = reactions.knockCountdown + 3;
        }
    }
    else if (Knock < strength)
    {
        if (reactions.knockCountdown < MostCountdown - 1)
        {
            reactions.knockCountdown = reactions.knockCountdown + 2;
        }
    }
    else if (SoftKnock < strength)
    {
        if (reactions.knockCountdown < MostCountdown)
        {
            reactions.knockCountdown = reactions.knockCountdown + 1;
        }
    }
}

void ReleaseRigidBodyAtRest(ObjectNode* node)
{
    if (!node->rigidBody->bits.touchingWorld)
    {
        return;
    }

    const Vector4& velocity = node->motion->velocity;
    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < ObjectRigidBody::RestingSpeedSquared)
    {
        node->ReleaseRigidBody();
    }
}

void AddPerception(ObjectNode* node, const void* arguments)
{
    if (node->perception == nullptr)
    {
        void* perception = MemoryAllocate(sizeof(Perception));
        ConstructPerception(perception);
        node->perception = static_cast<Perception*>(perception);
    }

    AddSense(node->perception, arguments);
}

void StopNodeSounds(ObjectNode* node)
{
    if (node->trackedSound == NoInstanceSound)
    {
        return;
    }

    StopInstanceSound(node->trackedSound);
    node->trackedSound = NoInstanceSound;
}

void ObjectNode::FollowMotionBlock(MotionBlock* block, TimeClock* clock)
{
    if (trajectory == nullptr)
    {
        trajectory = ConstructTrajectory(static_cast<Trajectory*>(MemoryAllocate(sizeof(Trajectory))));
        trajectory->node = this;
        StartFollowing(trajectory, block, clock, this);
    }
    else if (trajectory->followed != block)
    {
        StartFollowing(trajectory, block, clock, this);
    }
}

void CreateHeadTracking(ObjectNode* node, const void* arguments, TimeClock* clock)
{
    if (node->headTracking != nullptr)
    {
        LetGoOfHeadTracking(node->headTracking, node);
        if (node->headTracking != nullptr)
        {
            DestroyHeadTracking(node->headTracking, DestroyAndFree);
        }
    }

    HeadTracking* tracking = HeadTracking::Construct(static_cast<HeadTracking*>(MemoryAllocate(sizeof(HeadTracking))), node);
    node->headTracking = tracking;
    SetUpHeadTracking(tracking, static_cast<const HeadTrackingSettings*>(arguments), clock, node);
}

void SetNodeController(ObjectNode* node, NodeController* controller)
{
    NodeController* current = node->controller;
    if (current != nullptr)
    {
        CallVirtual<void>(current, current->vtable, NodeController::StopSlot);
        current = node->controller;
        if (current != nullptr)
        {
            CallVirtual<void>(current, current->vtable, NodeController::DestroySlot, DestroyAndFree);
        }
    }

    node->controller = controller;
    CallVirtual<void>(controller, controller->vtable, NodeController::StartSlot);
}

void SetContactSounds(f32 value, ObjectNode* node, u32 first, u32 last)
{
    node->contactSoundValue = value;
    node->contactSoundFirst = static_cast<u8>(first);
    node->contactSoundLast = static_cast<u8>(last);
}

void StopNodeMotion(ObjectNode* node)
{
    if (node->motion != nullptr)
    {
        node->motion->Stop();
    }

    if (node->rigidBody != nullptr)
    {
        StopRigidBody(node->rigidBody);
    }
}

ObjectNode* PacketNodeOf(InstanceContext* instance)
{
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    if (node == nullptr)
    {
        return nullptr;
    }

    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0 ? node : nullptr;
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void* ConstructHeadTrackingSettings(void* memory)
{
    // A range of 10, a stiffness of a half, the joints and the exit point none, no steering (bits 28-31 as the memory had them)
    constexpr f32 Range = 10.0f;
    auto* settings = static_cast<HeadTrackingSettings*>(memory);
    settings->unused00 = 0;
    settings->range = Range;
    settings->stiffness = 0.5f;
    AngleFrom(&settings->negativePitch, 0.0f, AngleRadians);
    AngleFrom(&settings->positivePitch, 0.0f, AngleRadians);
    AngleFrom(&settings->yawLimit, 0.0f, AngleRadians);
    settings->bits.joint = GameOGI::NoJoint;
    settings->bits.secondJoint = GameOGI::NoJoint;
    settings->bits.exitPoint = GameOGI::NoExitPoint;
    settings->bits.steering = 0;
    settings->bits.ignoresNoises = 0;
    settings->bits.unused27 = 0;
    settings->direction[2] = -1.0f;
    settings->damping = 1.0f;
    settings->direction[0] = -1.0f;
    settings->direction[1] = 1.0f;
    for (u32& value : settings->unused24)
    {
        value = 0;
    }

    settings->unseenLimit = 0;
    return settings;
}

void DestroyHeadTrackingSettings(void* settings, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(settings);
    }
}

void SetHeadTrackingNegativePitch(void* settings, f32 degrees)
{
    auto* limits = static_cast<HeadTrackingSettings*>(settings);
    limits->negativePitch = static_cast<s32>(degrees * DegreesToAngle);
    limits->direction[1] = SinOfAngle(&limits->negativePitch);
}

void SetHeadTrackingPositivePitch(void* settings, f32 degrees)
{
    auto* limits = static_cast<HeadTrackingSettings*>(settings);
    limits->positivePitch = static_cast<s32>(degrees * DegreesToAngle);
    limits->direction[0] = -SinOfAngle(&limits->positivePitch);
}

void SetHeadTrackingYawLimit(void* settings, f32 degrees)
{
    auto* limits = static_cast<HeadTrackingSettings*>(settings);
    limits->yawLimit = static_cast<s32>(degrees * DegreesToAngle);
    limits->direction[2] = CosOfAngle(&limits->yawLimit);
    limits->bits.unused27 = 1;
}

void SenseSpeed(const void* sense, TimeClock*, ObjectNode* node, f32* level)
{
    Vector4 velocity = {0.0f, 0.0f, 0.0f, 1.0f};
    Agent* agent = node->agent;
    if (CallVirtual<u32>(agent, agent->vtable, Agent::VelocitySlot, &velocity) == 0)
    {
        ObjectRigidBody* body = node->rigidBody;
        if (body != nullptr && body->physicsBody != nullptr)
        {
            velocity = body->physicsBody->velocity;
        }
        else
        {
            velocity = node->motion->velocity;
        }
    }

    const PerceptionSense* speed = SenseOf(sense);
    f32 length = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    *level = ClampFloat(length * speed->speedScale, speed->lowest, speed->highest);
}

void SenseRising(const void* sense, TimeClock*, ObjectNode*, f32* level)
{
    const PerceptionSense* rising = SenseOf(sense);
    *level = ClampFloat(*level + rising->decay * PerceptionSense::DecayShare, rising->lowest, rising->highest);
}

u32 SenseNoticesObject(const void* sense, u32 objectId)
{
    const PerceptionSense* noticing = SenseOf(sense);
    u32 count = noticing->objectCount;
    for (u8 index = 0; index < count; index++)
    {
        if (noticing->objects[index] == objectId)
        {
            return 1;
        }
    }

    return 0;
}

void SetTrailSystem(TrailArguments* trail, u32 system)
{
    trail->system = static_cast<u16>(system);
}

void DestroyTrailArguments(TrailArguments* trail, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(trail);
    }
}

void SetTrailPosition(u32* settings, const Vector4* position)
{
    auto* trail = reinterpret_cast<TrailArguments*>(settings);
    *reinterpret_cast<Vector4*>(trail->offset) = *position;
    trail->bits.offsetGiven = 1;
}

s32 UpdateTrail(TrailArguments* trail, ObjectNode* node, s32 emitter)
{
    if (trail->system == NoParticleSystem)
    {
        emitter = StopTrail(trail, emitter);
    }
    else if (emitter < 0)
    {
        emitter = StartTrail(trail, trail->system, 1, node);
    }

    if (emitter < 0)
    {
        return emitter;
    }

    Matrix4x4 frame = *ParticleFrame(node->owner, trail->exitPoint);
    TrailBits bits = trail->bits;
    OrientParticleFrame(bits.turned, bits.axes, bits.offsetGiven ? reinterpret_cast<const Vector4*>(trail->offset) : nullptr,
                        &frame);
    if (trail->bits.gravityFrame)
    {
        return SetEmitterGravityFrame(emitter, &frame);
    }

    return SetEmitterFrame(emitter, &frame);
}

u32 TrailOnItsSurface(const TrailArguments* trail, ObjectNode* node)
{
    if (trail->surface == -1)
    {
        return 1;
    }

    return trail->surface == node->surface ? 1 : 0;
}

s32 StartTrail(const TrailArguments* trail, u32 system, u32, ObjectNode* node)
{
    InstanceContext* instance = node->owner;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    system &= 0xFFFF;
    if (trail->bits.gravityFrame)
    {
        return StartEmitter(instance, system, 0, &position);
    }

    return StartEmitterKeepingTranslation(instance, system, 0, &position);
}

s32 StopTrail(TrailArguments*, s32 emitter)
{
    if (emitter < 0)
    {
        return -1;
    }

    return StopEmitter(emitter);
}

MotionBlock* ConstructMotionBlock(void* memory, u32 cycling, u32 cover)
{
    auto* block = static_cast<MotionBlock*>(memory);
    if (cycling != 0)
    {
        MakeCyclingMotionBlock(block);
    }
    else if (cover != 0)
    {
        MakeCoverMotionBlock(block);
    }
    else
    {
        MakePlainMotionBlock(block);
    }

    return block;
}

void DestroyMotionBlock(MotionBlock* block, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(block);
    }
}

void SetUpMotionBlock(MotionBlock* block, u32 fallingX, u32 fallingY, u32 fallingZ, f32 duration, f32 startX, f32 startY,
                      f32 startZ)
{
    const u32 falling[3] = {fallingX, fallingY, fallingZ};
    const f32 starts[3] = {startX, startY, startZ};
    block->duration = duration;
    // The angle itself turns by a range of 1
    for (u32 axis = 0; axis < 3; axis++)
    {
        if (block->motion.CycleOf(axis) == MotionBlock::CycleAngle)
        {
            block->cycleRanges[axis] = 1.0f;
        }
    }

    if (0.0f < duration)
    {
        block->fadeRate = MotionBlockFadeRate(duration, block->motion.turns ? 1 : 0, block->cycleRanges[0],
                                              block->cycleRanges[1], block->cycleRanges[2]);
        block->motion.fades = 1;
    }
    else
    {
        block->fadeRate = 0.0f;
        block->motion.fades = 0;
    }

    for (u32 axis = 0; axis < 3; axis++)
    {
        if (starts[axis] == 0.0f)
        {
            continue;
        }

        s32 phase;
        CycleStart(&phase, block, block->motion.CycleOf(axis), falling[axis], starts[axis], block->cycleRanges[axis]);
        block->cyclePhases[axis] = static_cast<f32>(phase) * AngleToRadians;
    }
}

void SetMotionBlockConstraint(MotionBlock* block, u32 kind, const Vector4* vector)
{
    switch (kind)
    {
    case MotionBlock::ConstraintLine:
    case MotionBlock::ConstraintPlane:
        block->constraint[0] = vector->x;
        block->constraint[1] = vector->y;
        block->constraint[2] = vector->z;
        block->constraint[3] = vector->w;
        block->body.constraint = kind;
        break;
    case MotionBlock::GivenHingeX:
        block->body.hingeX = 1;
        break;
    case MotionBlock::GivenHingeY:
        block->body.hingeY = 1;
        break;
    case MotionBlock::GivenHingeZ:
        block->body.hingeZ = 1;
        break;
    case MotionBlock::ConstraintFixed:
    case MotionBlock::GivenOddConstraint:
        block->body.constraint = kind;
        break;
    default:
        break;
    }
}

void SetMotionBlockTurnLimit(f32 limit, MotionBlock* block)
{
    block->turnLimit = limit;
    block->body.limitsTurn = 1;
}

f32 MotionBlockFadeRate(f32 duration, u32 turns, f32 x, f32 y, f32 z)
{
    f32 largest = x;
    if (largest < y)
    {
        largest = y;
    }

    if (largest < z)
    {
        largest = z;
    }

    return (largest - (turns != 0 ? TurnMargin : MoveMargin)) / duration;
}

f32 MotionBlockCycleX(MotionBlock* block, const s32* angle, f32 range)
{
    return MotionBlockCycle(block, 0, angle, range);
}

f32 MotionBlockCycleY(MotionBlock* block, const s32* angle, f32 range)
{
    return MotionBlockCycle(block, 1, angle, range);
}

f32 MotionBlockCycleZ(MotionBlock* block, const s32* angle, f32 range)
{
    return MotionBlockCycle(block, 2, angle, range);
}

void MakeMotionBlockNormal(MotionBlock* block)
{
    ReleaseLinkedInstances(AttachmentsOf(block->node->owner), AttachmentLinkFlags::Marked);
}

void StopMotionBlockSticking(MotionBlock* block, u32 keepStuck)
{
    // Not sticky any more: what stuck to it let go unless asked
    if (keepStuck == 0)
    {
        ReleaseLinkedInstances(AttachmentsOf(block->node->owner), AttachmentLinkFlags::Marked);
    }

    block->stickyObject = 0;
    block->stickyKinds = 0;
    block->flags.sticky = 0;
    block->stickyStrength = 0.0f;
    block->stickyMessage = 0;
}

void SetMotionBlockKind(MotionBlock* block, u32 kind)
{
    block->motion.kind = kind;
}

void MakePlainMotionBlock(MotionBlock* block)
{
    // Its cycles' rates, ranges and phases, spin, fade rate and duration none, its mover the node's own instance, its grab and
    // hold strengths none
    ResetMotionBlock(block);
    block->flags.keepsStepping = 0;
    block->flags.sendsTouches = 0;
    block->mover = MotionBlock::MoverOwnInstance;
    for (u32 axis = 0; axis < 3; axis++)
    {
        block->cycleRates[axis] = 0.0f;
        block->cyclePhases[axis] = 0.0f;
        block->cycleRanges[axis] = 0.0f;
    }

    block->fadeRate = 0.0f;
    block->duration = 0.0f;
    block->spinDegrees = 0.0f;
    block->grabStrength = 0.0f;
    block->holdStrength = 0.0f;
}

void MakeCoverMotionBlock(MotionBlock* block)
{
    ResetMotionBlock(block);
    block->motion.kind = MotionBlock::KindCover;
    block->flags.keepsStepping = 1;
    block->search.kind = 0;
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void ResetHeadTurns(HeadTracking* tracking)
{
    tracking->pitch = 0.0f;
    tracking->yaw = 0.0f;
    tracking->pitchSpeed = 0.0f;
    tracking->yawSpeed = 0.0f;
    tracking->pitchAngle = 0;
    tracking->yawAngle = 0;
    tracking->rollAngle = 0;
    // The three words after the angles
    auto* words = reinterpret_cast<u32*>(tracking->unused30);
    for (u32 index = 0; index < 3; index++)
    {
        words[index] = 0;
    }

    InitIdentityMatrix(&tracking->matrix);
}

void DestroyHeadTurner(HeadTracking* tracking, u32 destroyFlags)
{
    tracking->vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(tracking);
    }
}

void HookHeadTracking(HeadTracking* tracking, OgiAnimator* animator)
{
    AddJointCallback(animator, tracking->bits.joint, tracking);
    tracking->bits.hooked = 1;
    if (tracking->bits.secondJoint != GameOGI::NoJoint)
    {
        AddJointCallback(animator, tracking->bits.secondJoint, tracking);
    }
}

void UnhookHeadTracking(HeadTracking* tracking, OgiAnimator* animator)
{
    UnhookJoints(tracking, animator);
}

void DestroyHeadTracking(HeadTracking* tracking, u32 destroyFlags)
{
    RemoveReference(&tracking->last);
    RemoveReference(&tracking->remembered);
    RemoveReference(&tracking->target);
    tracking->vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(tracking);
    }
}

void LetGoOfHeadTracking(HeadTracking* tracking, ObjectNode* node)
{
    if (!tracking->bits.hooked)
    {
        return;
    }

    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    UnhookJoints(tracking, model->animator);
}

void StopHeadTracking(HeadTracking* tracking)
{
    constexpr u32 StopsAtRest = 1;
    tracking->flags.hasTarget = 1;
    tracking->flags.ignoredByLook = 1;
    LetGoOfHeadTarget(tracking, StopsAtRest);
}

u32 AddSense(void* perception, const void* sense)
{
    constexpr u32 NoSense = 0xFF;
    Perception* senses = PerceptionOf(perception);
    auto* added = static_cast<PerceptionSense*>(const_cast<void*>(sense));
    if (senses->bits.count == Perception::MostSenses || HasSense(perception, added->bits.kind) != 0)
    {
        return NoSense;
    }

    u32 count = senses->bits.count;
    senses->bits.count = count + 1;
    senses->senses[count] = added;
    return senses->bits.count;
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void ConstructPerception(void* memory)
{
    Perception* senses = PerceptionOf(memory);
    for (u32 index = 0; index < Perception::MostSenses; index++)
    {
        senses->senses[index] = nullptr;
        senses->levels[index] = 0.0f;
        senses->on[index] = 1;
        senses->times[index] = 0;
    }

    senses->direction = g_DefaultBox.min;
    senses->direction.w = 1.0f;
    senses->bits.count = 0;
}

u32 SetPerceptionWeight(f32 weight, void* perception, u32 slot)
{
    u32 index = 0;
    if (FindSense(perception, slot & 0xFF, &index) == 0)
    {
        return 0;
    }

    PerceptionOf(perception)->levels[index & 0xFF] = ClampFloat(weight, 0.0f, 1.0f);
    return 1;
}

u32 PerceptionValue(void* perception, u32 slot, f32* value)
{
    u32 index = 0;
    if (FindSense(perception, slot & 0xFF, &index) == 0)
    {
        return 0;
    }

    *value = PerceptionOf(perception)->levels[index];
    return 1;
}

u32 AddPerceptionWeight(f32 weight, void* perception, u32 slot)
{
    u32 index = 0;
    if (FindSense(perception, slot & 0xFF, &index) == 0)
    {
        return 0;
    }

    Perception* senses = PerceptionOf(perception);
    f32 sum = senses->levels[index] + weight;
    u32 kept = index & 0xFF;
    senses->levels[kept] = ClampFloat(sum, 0.0f, 1.0f);
    return 1;
}

u32 FindSense(void* perception, u32 kind, u32* index)
{
    Perception* senses = PerceptionOf(perception);
    for (u32 at = 0; at < senses->bits.count; at++)
    {
        if (senses->senses[at]->bits.kind == kind)
        {
            *index = at;
            return 1;
        }
    }

    return 0;
}

u32 HasSense(void* perception, u32 kind)
{
    Perception* senses = PerceptionOf(perception);
    u32 count = senses->bits.count;
    for (u32 index = 0; index < count; index++)
    {
        if (senses->senses[index]->bits.kind == kind)
        {
            return 1;
        }
    }

    return 0;
}

void TurnSenseOff(void* perception, u32 kind)
{
    SetSenseOn(perception, kind & 0xFF, 0);
}

void TurnSenseOn(void* perception, u32 kind)
{
    SetSenseOn(perception, kind & 0xFF, 1);
}

ParticleTrails* ParticleTrails::Construct(ParticleTrails* trails)
{
    trails->Reset();
    trails->bits.measuresTurn = 0;
    trails->time = nullptr;
    trails->strength = 1.0f;
    return trails;
}

void ParticleTrails::Destroy(u32 destroyFlags)
{
    for (u32 index = 0; index < bits.count; index++)
    {
        if (emitters[index] >= 0)
        {
            StopEmitter(emitters[index]);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 ParticleTrails::Add(const void* arguments)
{
    constexpr u32 Full = 0xFF;
    ParticleTrailsBits added = bits;
    u32 count = added.count;
    if (count == MostTrails)
    {
        return Full;
    }

    auto* trail = static_cast<TrailArguments*>(const_cast<void*>(arguments));
    trails[count] = trail;
    times[count] = 0;
    added.count = count + 1;
    bits = added;
    if (trail->bits.kind == MeasuresTurnKind)
    {
        bits.measuresTurn = 1;
    }

    return bits.count;
}

void StepParticleTrails(ParticleTrails* trails, TimeClock* clock, ObjectNode* node)
{
    for (u32 index = 0; index < trails->bits.count; index++)
    {
        trails->time = &trails->times[index];
        trails->emitters[index] = StepTrail(trails->trails[index], clock, node, trails->emitters[index]);
    }
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void ParticleTrails::Reset()
{
    for (u32 index = 0; index < MostTrails; index++)
    {
        trails[index] = nullptr;
        emitters[index] = NoEmitter;
        times[index] = 0;
    }

    bits.count = 0;
}

void ParticleTrails::RemoveKind(u32 kind)
{
    // Taking a trail out moves the last one into its slot, which is looked at next
    u32 index = 0;
    while (index < bits.count)
    {
        const TrailArguments* trail = trails[index];
        if (trail == nullptr || trail->bits.removalKind == kind)
        {
            RemoveSlot(index & 0xFF);
        }
        else
        {
            index++;
        }
    }
}

u32 ParticleTrails::RemoveSlot(u32 index)
{
    index &= 0xFF;
    if (index >= bits.count)
    {
        return 0;
    }

    if (emitters[index] >= 0)
    {
        StopEmitter(emitters[index]);
    }

    bits.count = bits.count - 1;
    u32 last = bits.count;
    if (last != 0)
    {
        trails[index] = trails[last];
        emitters[index] = emitters[last];
        times[index] = times[last];
    }

    return 1;
}

void ParticleTrails::ChangeChunk(ChunkData*, ChunkLinkData* link)
{
    for (u32 index = 0; index < bits.count; index++)
    {
        if (emitters[index] != NoEmitter)
        {
            SetEmitterChunk(emitters[index], link->linkedData);
        }
    }
}

void DestroyTrajectory(Trajectory* trajectory, u32 destroyFlags)
{
    Route* route = trajectory->coverRoute;
    if (route != nullptr)
    {
        MemoryDeallocate2_(route);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(trajectory);
    }
}

void StartCoverSearch(Trajectory* trajectory, ObjectNode* node)
{
    trajectory->coverScore = 0.0f;
    if (trajectory->followed->search.kind != MotionBlock::SearchInBox)
    {
        return;
    }

    trajectory->cover = nullptr;
    trajectory->bits.count = 0;
    node->waypoints->ReleaseRoute();
    GatherCoverPositions(trajectory, node);
}

void StartGrabber(Trajectory* trajectory, ObjectNode* node)
{
    constexpr u32 HullsKind = 10;
    ObjectRigidBody* body = MakeTrajectoryBody(node, HullsKind);
    if (trajectory->followed->body.noCollisions)
    {
        StopRigidBodyCollisions(body);
        if (body->physicsBody != nullptr)
        {
            body->physicsBody->bits.noCollisions = 1;
        }
    }
    else
    {
        ListRigidBodySecond(body, HullsKind);
    }

    SetRigidBodyMass(NodeProperties(node)->GetFloat(RigidBodyMassProperty), body);
    SetUpTrajectoryBody(trajectory, body);
    node->reactions.knockCountdown = 0;
}

void StartBall(Trajectory* trajectory, ObjectNode* node)
{
    constexpr u32 SphereKind = 9;
    ObjectRigidBody* body = MakeTrajectoryBody(node, SphereKind);
    ListRigidBodySecond(body, SphereKind);
    f32 mass = NodeProperties(node)->GetFloat(RigidBodyMassProperty);
    body->physicsBody->SetMassAndSize(mass, 1.0f, 1.0f, 1.0f);
    SetUpTrajectoryBody(trajectory, body);
    node->reactions.knockCountdown = 0;
}

void SlowBodyAlongAxis(f32 share, Trajectory*, DynamicBody* body, u32 axis)
{
    // The share goes as the time too
    switch (axis)
    {
    case 0:
    case 1:
        body->SlowAlongX(share, share);
        break;
    case 2:
        body->SlowAlongY(share, share);
        break;
    case 3:
        body->SlowAlongZ(share, share);
        break;
    default:
        break;
    }
}

void LetGoOfTrajectory(Trajectory* trajectory, ObjectNode* node)
{
    trajectory->bits.unused16 = 1;
    u32 kind = trajectory->followed->motion.kind;
    // The body kinds let go of the rigid body
    if (kind != MotionBlock::KindCycles && kind < MotionBlock::KindCover && node->rigidBody != nullptr)
    {
        ChunkRigidBodies* lists = node->rigidBody->chunkBodies;
        if (lists != nullptr)
        {
            TakeFirstRigidBody(lists, node->rigidBody);
            TakeSecondRigidBody(lists, node->rigidBody);
        }

        if (node->rigidBody != nullptr)
        {
            DestroyRigidBody(node->rigidBody, DestroyAndFree);
        }

        node->rigidBody = nullptr;
    }

    if (trajectory->coverRoute != nullptr)
    {
        MemoryDeallocate2_(trajectory->coverRoute);
        trajectory->coverRoute = nullptr;
    }
}

void GrabTouched(f32 strength, Trajectory* trajectory, ObjectNode* node, InstanceContext* touched)
{
    node->flags.seeksContact = 0;
    InstanceContext* instance = node->owner;
    MotionBlockFlags blockFlags = trajectory->followed->flags;
    bool holds;
    if (blockFlags.holdsTouched)
    {
        holds = true;
    }
    else if (!blockFlags.holdsAgentRef1)
    {
        holds = false;
    }
    else
    {
        if (node->agentRef1 != nullptr && IsAsleep(node->agentRef1))
        {
            node->agentRef1 = nullptr;
        }

        holds = touched == node->agentRef1;
    }

    // The node's instance holds what it touched, or is held by it
    InstanceContext* holder = holds ? instance : touched;
    InstanceContext* held = holds ? touched : instance;
    void* attachments = AttachmentsOf(holder);
    HoldOnSpring(strength, trajectory->followed->springStiffness, attachments, holder, held);
}

void HoldWithAttachments(f32 strength, Trajectory* trajectory, ObjectNode* node)
{
    InstanceContext* instance = node->owner;
    void* attachments = AttachmentsOf(instance);
    HoldInPlace(strength, trajectory->followed->springStiffness, attachments, instance);
}
