#include "game/objectnode.h"

#include "game/attachments.h"
#include "game/behaviours.h"
#include "game/events.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"

// An object node's launches, the sync check of a packet's end, and its motion block's cycles, sticking and resets

EABI_EXPORT(FUN_00233988, LaunchWithMotion);
EABI_EXPORT(FUN_00233d68, LaunchPhysicsBody);
EABI_EXPORT(FUN_002362a8, CycleStart);
EABI_EXPORT(FUN_00236418, StepCycleX);
EABI_EXPORT(FUN_002365a0, StepCycleY);
EABI_EXPORT(FUN_00236728, StepCycleZ);

namespace
{
// An angle's 65536ths of a turn kept within a turn
constexpr u32 AngleMask = 0xFFFF;
// What sticking to a block attaches with
constexpr u32 StuckFlags = 9;

// The instance's matrix, its place's made up to date first
Matrix4x4 PlaceMatrix(InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    return place->matrix;
}

// A physics body spun about an axis of a matrix (its x axis by one speed, or else its y axis by the other; none when both are 0)
void SpinBody(DynamicBody* body, const Matrix4x4* matrix, f32 spinY, f32 spinX)
{
    Vector4 spin;
    if (spinX != 0.0f)
    {
        spin = *RowOf(matrix, 0);
        spin.x = spin.x * spinX;
        spin.y = spin.y * spinX;
        spin.z = spin.z * spinX;
    }
    else if (spinY != 0.0f)
    {
        spin = *RowOf(matrix, 1);
        spin.x = spin.x * spinY;
        spin.y = spin.y * spinY;
        spin.z = spin.z * spinY;
    }
    else
    {
        return;
    }

    body->SetAngularVelocity(&spin);
}

// Whether a runner has a level, up to its first empty one, that entered the state and left it
bool LevelLeftState(const BehaviourRunner* runner, u32 state)
{
    for (u32 index = 0; index < BehaviourRunner::Levels; index++)
    {
        const BehaviourLevel* level = runner->levels[index];
        if (level == nullptr)
        {
            return false;
        }

        if (reinterpret_cast<u32>(level->entered) == state && reinterpret_cast<u32>(level->state) != state)
        {
            return true;
        }
    }

    return false;
}

// A cycle about an axis (0 x, 1 y, 2 z) stepped
f32 StepCycle(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle, u32 axis)
{
    u32 cycle = block->motion.CycleOf(axis);
    f32 rate = block->cycleRates[axis];
    if (cycle == MotionBlock::CycleAngle)
    {
        *angle = *angle + static_cast<s32>(amplitude * elapsed * rate * RadiansToAngle);
    }
    else
    {
        *angle = *angle + static_cast<s32>(elapsed * rate * RadiansToAngle);
    }

    *angle = *angle & AngleMask;
    f32 value;
    switch (cycle)
    {
    case MotionBlock::CycleSine:
        value = amplitude * SinOfAngle(angle);
        break;
    case MotionBlock::CycleSquare:
    case MotionBlock::CycleSquareToo:
        value = (static_cast<s32>(rate * (static_cast<f32>(*angle) * AngleToRadians)) & 1) != 0 ? -amplitude : amplitude;
        break;
    case MotionBlock::CycleRandom:
        value = RandomSignedTimes(amplitude);
        break;
    case MotionBlock::CycleAngle:
        value = static_cast<f32>(*angle) * AngleToRadians;
        break;
    default:
        return 0.0f;
    }

    u32 sign = block->motion.SignOf(axis);
    if (sign == MotionBlock::SignPositive)
    {
        return __builtin_fabsf(value);
    }

    if (sign == MotionBlock::SignNegative)
    {
        return -__builtin_fabsf(value);
    }

    return value;
}
}

void LaunchWithMotion(f32 spinY, f32 spinX, ObjectNode* node, MotionBlock* motion, const Vector4* velocity)
{
    node->ReleaseRigidBody();
    node->rigidBody = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
    node->flags.seeksContact = 1;
    node->ReleaseMotionBlock();
    node->ReleaseTrajectory();
    node->FollowMotionBlock(motion, GetContextClock(node->owner));
    node->motionBlock = motion;
    node->owner->flags.physicsBody = 1;
    node->motionBlock->node = node;
    DynamicBody* body = node->rigidBody->physicsBody;
    body->SetVelocity(velocity);
    Matrix4x4 matrix = PlaceMatrix(node->owner);
    if (motion->flags.uprightsLaunched)
    {
        Vector4 facing = *RowOf(&matrix, 2);
        f32 scale = InverseLength(&facing, LengthEpsilon);
        facing.x = facing.x * scale;
        facing.y = facing.y * scale;
        facing.z = facing.z * scale;
        MatrixFacing(&matrix, &facing);
        InstanceContext* owner = node->owner;
        ObjectPlace* place = owner->place;
        place->SyncRotation();
        Vector4 rotation;
        GetRotationVec(&rotation, &matrix);
        if (place->TurnTo(&rotation))
        {
            QueueObject(owner);
        }
    }

    SpinBody(body, &matrix, spinY, spinX);
    if (node->agentRef1 != nullptr)
    {
        body->ignoredInstance = node->agentRef1;
    }
}

void LaunchPhysicsBody(f32 spinY, f32 spinX, ObjectNode* node, const Vector4* velocity)
{
    DynamicBody* body = node->rigidBody->physicsBody;
    body->ClearForces();
    body->SetVelocity(velocity);
    Matrix4x4 matrix = PlaceMatrix(node->owner);
    SpinBody(body, &matrix, spinY, spinX);
}

u32 LeftSyncState(GameNode* node, u32 state)
{
    auto* objectNode = static_cast<ObjectNodeBase*>(node);
    BehaviourRunner* second = objectNode->runners[1];
    if (second != nullptr && LevelLeftState(second, state))
    {
        return 1;
    }

    BehaviourRunner* first = objectNode->runners[0];
    return first != nullptr && LevelLeftState(first, state) ? 1 : 0;
}

s32* CycleStart(s32* angle, MotionBlock*, u32 cycle, u32 falling, f32 value, f32 range)
{
    s32 start;
    AngleFrom(&start, 0.0f, AngleRadians);
    switch (cycle)
    {
    case MotionBlock::CycleSine:
    case MotionBlock::CycleAngle:
        AngleOfSine(value / range, &start);
        if (falling != 0)
        {
            start = HalfTurnAngle - start;
        }

        if (value < 0.0f)
        {
            start = start + FullTurnAngle;
        }

        break;
    case MotionBlock::CycleSquare:
        AngleFrom(&start, 0.0f < value ? Pi : 0.0f, AngleRadians);
        break;
    case MotionBlock::CycleSquareToo:
        AngleFrom(&start, value / range, AngleRadians);
        if (falling != 0)
        {
            start = start + FullTurnAngle;
        }

        break;
    case MotionBlock::CycleRandom:
        AngleFrom(&start, RandomBelowFloat(TwoPi), AngleRadians);
        break;
    default:
        break;
    }

    *angle = start;
    return angle;
}

f32 StepCycleX(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle)
{
    return StepCycle(elapsed, amplitude, block, angle, 0);
}

f32 StepCycleY(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle)
{
    return StepCycle(elapsed, amplitude, block, angle, 1);
}

f32 StepCycleZ(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle)
{
    return StepCycle(elapsed, amplitude, block, angle, 2);
}

u32 SticksToMotionBlock(const MotionBlock* block, InstanceContext* instance)
{
    if (block->stickyObject == 0)
    {
        return (instance->nodes.mask & block->stickyKinds) != 0 ? 1 : 0;
    }

    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
    return block->stickyObject == (node->agent->objectId & ResourceIndexMask) ? 1 : 0;
}

void StickToMotionBlock(MotionBlock* block, InstanceContext* instance)
{
    InstanceContext* holder = block->node->owner;
    auto* attachments = static_cast<AttachmentsNode*>(AttachmentsOf(holder));
    attachments->stickiness = block->stickyStrength;
    AttachInstance(attachments, holder, instance, StuckFlags, 0);
    if (block->stickyMessage == 0)
    {
        return;
    }

    Reference* sender = holder != nullptr ? AddReference(holder) : nullptr;
    GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))),
                                            static_cast<u16>(block->stickyMessage), &sender, ObjectNodeKinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(instance, &handle);
}

void ResetMotionBlock(MotionBlock* block)
{
    // Every flag but the trajectory's cycles about the axes and those from 15 on goes, and the motion bits but bit 31
    constexpr u32 KeptFlags = 0xFFFF8038;
    constexpr u32 KeptMotion = 0x80000000;
    block->flags.value &= KeptFlags;
    block->motion.value &= KeptMotion;
    block->stickyObject = 0;
    block->stickyKinds = 0;
    block->stickyStrength = 0.0f;
    block->touchMessage = 0;
    block->stickyMessage = 0;
}

void MakeCyclingMotionBlock(MotionBlock* block)
{
    // A body's bits 0-19 none but one substep
    constexpr u32 KeptBodyBits = 0xFFF00000;
    ResetMotionBlock(block);
    block->flags.keepsStepping = 1;
    block->constraint[3] = 1.0f;
    block->body.value &= KeptBodyBits;
    block->body.substeps = 1;
    for (f32& value : block->values)
    {
        value = 0.0f;
    }

    block->grabStrength = 0.0f;
    block->holdStrength = 0.0f;
    block->knockScale = 1.0f;
    block->turnStrength = 0.0f;
    for (u32 axis = 0; axis < 3; axis++)
    {
        block->constraint[axis] = 0.0f;
    }

    block->mass = 0.0f;
    block->turnLimit = 0.0f;
}
