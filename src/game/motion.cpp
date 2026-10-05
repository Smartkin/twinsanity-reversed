#include "game/objectnode.h"

#include "game/agentlab.h"
#include "game/animation.h"
#include "game/behaviours.h"
#include "game/clock.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"

namespace
{
// A direction whose x and z squared add up to no more than this (1e-06) is straight up or down
constexpr f32 FlatEpsilon = 0x1.0c6f7ap-20f;
// What a step or a key comes to when it's stepped back past 0
constexpr u8 SteppedBeforeFirst = 0xFF;
// Motions 11 and 12 (nameless in the AgentLab tool) are stepped as a ground chase and an air chase
constexpr u32 SecondGroundChase = 11;
constexpr u32 SecondAirChase = 12;
// A projectile's gravity when the packet gives no Power, and how far a chase leans at most when it gives no Bounce
constexpr f32 DefaultProjectileGravity = 40.0f;
constexpr f32 DefaultChaseMostLean = 80.0f;

// The physics' parameters by motion: a spring's power and damping; a projectile's speed along the ground, its speed up and its
// gravity; a chase's share of the turn toward the target made per second (the packet's Duration), how much it leans into turns
// (Power), how much turning slows it (Damping) and how far it leans at most (Bounce)
enum PhysicsParameter : u32
{
    SpringPower = 0,
    SpringDamping = 1,
    ProjectileSpeed = 0,
    ProjectileRise = 1,
    ProjectileGravity = 2,
    ChaseTurnRate = 0,
    ChaseLean = 1,
    ChaseTurnDrag = 2,
    ChaseMostLean = 3,
};

// The packet's spaces: where its target and offset are taken from
using enum ControlPacket::Space;

// A target moved by an offset turned by a matrix (the offset a row vector)
void AddTransformed(Vector4* target, const Vector4* offset, const Matrix4x4* matrix)
{
    f32 x = matrix->m[0][0] * offset->x + matrix->m[1][0] * offset->y + matrix->m[2][0] * offset->z;
    f32 y = matrix->m[0][1] * offset->x + matrix->m[1][1] * offset->y + matrix->m[2][1] * offset->z;
    f32 z = matrix->m[0][2] * offset->x + matrix->m[1][2] * offset->y + matrix->m[2][2] * offset->z;
    target->x = target->x + x;
    target->z = target->z + z;
    target->y = target->y + y;
}

// A duration's inverse (the one given for no time)
f32 InverseOf(f32 duration, f32 noTime)
{
    return __builtin_fabsf(duration) <= Epsilon ? noTime : 1.0f / duration;
}

// The packet ended (its motion's both parts done, or its delay or sync over)
void EndRunningPacket(BehaviourRunner* runner)
{
    ControlPacket* ended = runner->packet;
    runner->packet = nullptr;
    runner->lastPacket = ended;
    runner->flags.packetWaiting = 0;
    runner->flags.packetEnded = 1;
}

// An interpolation that's over: the instance at the translation's and the rotation's targets (when it moves no joint), and the
// trajectory controller holding them
u32 FinishInterpolation(ObjectNode* node, ControlPacket* packet, u32 joint)
{
    if (packet->settings.translates != 0)
    {
        if (joint == GameOGI::NoJoint)
        {
            MoveInstance(node, packet, &node->translator->target);
        }

        node->motion->bits.translationDone = 1;
        if (node->trajectory != nullptr)
        {
            node->trajectory->position = node->translator->target;
        }
    }

    node->motion->bits.rotationDone = 1;
    if (packet->settings.rotates == 0)
    {
        return 1;
    }

    if (joint == GameOGI::NoJoint)
    {
        TurnInstance(node, packet, &node->rotator->target);
    }

    Trajectory* trajectory = node->trajectory;
    if (trajectory != nullptr)
    {
        Rotator* rotator = node->rotator;
        trajectory->rotation.x = rotator->target.x;
        trajectory->rotation.y = rotator->target.y;
        trajectory->rotation.z = rotator->target.z;
        trajectory->rotation.w = rotator->target.w;
        if (joint != GameOGI::NoJoint)
        {
            AnglesOfRotation(&trajectory->rotation, &trajectory->angles[0], &trajectory->angles[1], &trajectory->angles[2]);
        }
    }

    return 1;
}

// An interpolation's frame a fraction of the way: done when within the tolerance
u32 Interpolate(ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet, u32 joint, f32 along)
{
    Vector4 point;
    if (packet->settings.translates != 0)
    {
        Translator* translator = node->translator;
        point.x = translator->start.x + (translator->target.x - translator->start.x) * along;
        point.y = translator->start.y + (translator->target.y - translator->start.y) * along;
        point.w = 1.0f;
        point.z = translator->start.z + (translator->target.z - translator->start.z) * along;
        SmoothVelocity(along, node->motion, translator);
        f32 tolerance = runner->tolerance;
        if (tolerance != 0.0f)
        {
            translator = node->translator;
            f32 x = point.x - translator->target.x;
            f32 y = point.y - translator->target.y;
            f32 z = point.z - translator->target.z;
            if (x * x + y * y + z * z < tolerance)
            {
                node->motion->bits.translationDone = 1;
                node->motion->bits.rotationDone = 1;
                return 1;
            }
        }

        if (joint == GameOGI::NoJoint)
        {
            node->flags.unused4 = 1;
            MoveInstance(node, packet, &point);
        }
    }

    if (packet->settings.rotates != 0)
    {
        Rotator* rotator = node->rotator;
        Vector4 rotation;
        SlerpRotations(along, &rotation, &rotator->start, &rotator->target);
        if (joint == GameOGI::NoJoint)
        {
            TurnInstance(node, packet, &rotation);
        }
    }

    return 0;
}

// The time a straight motion takes that accelerates and decelerates for the given times: the time at full speed plus both
f32 AcceleratedDuration(f32 distance, f32 speed, f32 acceleration, f32 deceleration)
{
    return acceleration + (distance / speed - acceleration * 0.5f - deceleration * 0.5f) + deceleration;
}

// A rotator's target made the turn about y facing the translation's direction (the identity when it's straight up or down)
void FaceDirection(Vector4* target, const Translator* translator)
{
    const Vector4& direction = translator->direction;
    if (FlatEpsilon < direction.x * direction.x + direction.z * direction.z)
    {
        s32 yaw;
        YawOfDirection(&yaw, &direction);
        s32 angle = yaw;
        RotationFromYaw(target, &angle);
        return;
    }

    target->z = 0.0f;
    target->y = 0.0f;
    target->x = 0.0f;
    target->w = 1.0f;
}
}

extern "C"
{
    // What a packet's end time is compared to first (only ever 0)
    extern s32 g_PacketTimeBase RETAIL(D_0030A918);
}

EABI_EXPORT(FUN_0020be50, SetUpProjectile);
EABI_EXPORT(FUN_0022ea88, RollAlongX);
EABI_EXPORT(FUN_0022e8c8, RollAlongY);
EABI_EXPORT(FUN_0020c0b0, StepSpringPhysics);
EABI_EXPORT(FUN_002329f0, StepSpring);
EABI_EXPORT(FUN_00232728, StepProjectile);
EABI_EXPORT(FUN_00233768, AcceleratedFraction);
EABI_EXPORT(FUN_0020efb8, SmoothVelocity);
EABI_EXPORT(FUN_00231160, StepGroundChase);
EABI_EXPORT(FUN_0020f950, TurnSlowedSpeed);
EABI_EXPORT(FUN_0022d4f8, SteerTowards);
EABI_EXPORT(FUN_0022cac0, SteerBodyTowards);
EABI_EXPORT(FUN_00231580, StepAirChase);
EABI_EXPORT(FUN_00231840, StepRiddenAirChase);
EABI_EXPORT(FUN_00231cc8, StepClimbingChase);

PropertyHolder* ObjectNodeBase::PacketProperties()
{
    if (sourceNode != nullptr)
    {
        return GetPropsHolderFromInstanceNode(sourceNode);
    }

    return properties;
}

Translator* ObjectNode::MakeTranslator()
{
    if (translator == nullptr)
    {
        translator = Translator::Construct(static_cast<Translator*>(MemoryAllocate(sizeof(Translator))));
    }

    translator->motion = motion;
    return translator;
}

Rotator* ObjectNode::MakeRotator()
{
    if (rotator == nullptr)
    {
        rotator = Rotator::Construct(static_cast<Rotator*>(MemoryAllocate(sizeof(Rotator))));
    }

    rotator->motion = motion;
    return rotator;
}

Physics* ObjectNode::MakePhysics()
{
    if (physics == nullptr)
    {
        physics = Physics::Construct(static_cast<Physics*>(MemoryAllocate(sizeof(Physics))));
    }

    physics->motion = motion;
    return physics;
}

MotionState* MotionState::Construct(MotionState* state)
{
    state->Reset();
    return state;
}

void MotionState::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void MotionState::Reset()
{
    bits.translationDone = 0;
    bits.rotationDone = 0;
    bits.unused2 = 0;
    bits.unused3 = 0;
    bits.unused4 = 0;
    duration = 0.0f;
    inverseDuration = 0.0f;
    turnSpeed = 0.0f;
    chaseSpeed = 0.0f;
    speed = 0.0f;
    startVelocity = g_DefaultBox.min;
    startVelocity.w = 1.0f;
    velocity = g_DefaultBox.min;
    velocity.w = 1.0f;
    turn.y = 0.0f;
    turn.w = 0.0f;
    turn.z = 0.0f;
    turn.x = 0.0f;
}

void MotionState::Stop()
{
    velocity = g_DefaultBox.min;
    velocity.w = 1.0f;
    speed = 0.0f;
}

Translator* Translator::Construct(Translator* translator)
{
    translator->Reset();
    return translator;
}

void Translator::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Translator::Reset()
{
    target = g_DefaultBox.min;
    target.w = 1.0f;
    wander = g_DefaultBox.min;
    wander.w = 1.0f;
}

Rotator* Rotator::Construct(Rotator* rotator)
{
    rotator->Reset();
    return rotator;
}

void Rotator::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Rotator::Reset()
{
    target.y = 0.0f;
    target.w = 0.0f;
    target.z = 0.0f;
    target.x = 0.0f;
    unused30.y = 0.0f;
    unused30.w = 0.0f;
    unused30.z = 0.0f;
    unused30.x = 0.0f;
}

Physics* Physics::Construct(Physics* physics)
{
    physics->Reset();
    return physics;
}

void Physics::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Physics::Reset()
{
    parameters[0] = 0.0f;
    parameters[1] = 0.0f;
    parameters[2] = 0.0f;
    bits.unused2 = KindMade;
    velocity = g_DefaultBox.min;
    velocity.w = 1.0f;
    unused30.y = 0.0f;
    unused30.w = 0.0f;
    unused30.z = 0.0f;
    unused30.x = 0.0f;
}

Waypoints* Waypoints::Construct(Waypoints* waypoints)
{
    constexpr u32 Growth = 10;
    waypoints->positions.capacity = Growth;
    waypoints->positions.growth = Growth;
    waypoints->positions.count = 0;
    waypoints->positions.data = static_cast<LayoutPosition**>(MemoryAllocate2(Growth * sizeof(LayoutPosition*)));
    waypoints->paths.growth = Growth;
    waypoints->paths.capacity = Growth;
    waypoints->paths.count = 0;
    waypoints->paths.data = static_cast<LayoutPath**>(MemoryAllocate2(Growth * sizeof(LayoutPath*)));
    waypoints->Reset();
    return waypoints;
}

void Waypoints::Destroy(u32 destroyFlags)
{
    ReleaseRoute();
    if (paths.data != nullptr)
    {
        MemoryDeallocate_(paths.data);
    }

    if (positions.data != nullptr)
    {
        MemoryDeallocate_(positions.data);
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Waypoints::Reset()
{
    pathIndex = 0;
    keyCount = 0;
    pathCount = 0;
    key = 0;
    route = nullptr;
    routePath = nullptr;
    flags.stopped = 0;
    flags.backwards = 0;
    flags.wrapped = 0;
    routeIndex = Waypoints::NoRouteStep;
    firstKey = 0;
    lastKey = 0;
    pathDirection = g_DefaultBox.min;
    pathDirection.w = 1.0f;
    pathParameter = 0.0f;
}

void Waypoints::AddPosition(LayoutPosition* position)
{
    positions.Append(position);
    lastKey = keyCount;
    keyCount++;
}

void Waypoints::AddPath(LayoutPath* path)
{
    paths.Append(path);
    pathCount++;
}

void Waypoints::NextKey()
{
    key++;
    if (lastKey < key)
    {
        flags.wrapped = 1;
        key = firstKey;
    }
}

void Waypoints::PreviousKey()
{
    key--;
    if (key == SteppedBeforeFirst)
    {
        flags.wrapped = 1;
        key = lastKey;
    }
}

void Waypoints::SetRoute(Route* taken)
{
    if (route != nullptr)
    {
        route->Leave();
        if (route != nullptr)
        {
            MemoryDeallocate2_(route);
        }
    }

    route = taken;
    routeIndex = taken->count - 1;
    routePath = taken->PathTo(routeIndex);
    taken->PositionAt(routeIndex);
    route->Occupy();
}

void Waypoints::ReleaseRoute()
{
    routeIndex = Waypoints::NoRouteStep;
    if (route == nullptr)
    {
        return;
    }

    route->Leave();
    if (route != nullptr)
    {
        MemoryDeallocate2_(route);
    }

    routePath = nullptr;
    route = nullptr;
}

void Waypoints::ClearRoute()
{
    routeIndex = Waypoints::NoRouteStep;
    key = 0;
    if (route != nullptr)
    {
        route->Leave();
        if (route != nullptr)
        {
            MemoryDeallocate2_(route);
        }
    }

    pathParameter = 0.0f;
    flags.wrapped = 0;
    route = nullptr;
    routePath = nullptr;
}

void Waypoints::NextRouteStep()
{
    if (route == nullptr)
    {
        return;
    }

    if (flags.wrapped)
    {
        routeIndex = 1;
        flags.wrapped = 0;
    }
    else
    {
        routeIndex++;
    }

    if (routeIndex == route->count)
    {
        routeIndex--;
    }

    routePath = route->PathTo(routeIndex);
}

void Waypoints::PreviousRouteStep()
{
    if (route == nullptr)
    {
        return;
    }

    routeIndex--;
    if (routeIndex == SteppedBeforeFirst)
    {
        routeIndex = 0;
        flags.wrapped = 1;
    }

    routePath = route->PathTo(routeIndex);
}

void Waypoints::RestartRoute()
{
    if (route != nullptr)
    {
        routePath = nullptr;
        routeIndex = 0;
    }
}

void KeyRotation(const LayoutPosition* from, const LayoutPosition* to, Vector4* rotation)
{
    Vector4 direction = to->position;
    direction.w = 1.0f;
    Vector4 start = from->position;
    direction.x -= start.x;
    direction.y -= start.y;
    direction.z -= start.z;
    if (direction.x == 0.0f && direction.z == 0.0f)
    {
        rotation->w = 1.0f;
        rotation->x = 0.0f;
        rotation->z = 0.0f;
        rotation->y = 0.0f;
        return;
    }

    s32 yaw;
    YawOfDirection(&yaw, &direction);
    s32 angle = yaw;
    RotationFromYaw(rotation, &angle);
}

void StartPacketMotion(BehaviourRunner* runner, TimeClock* clock)
{
    using Packet = ControlPacket;
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    runner->packetStart = clock->time;
    runner->lastPacket = nullptr;
    runner->syncUnit = 0;
    runner->flags.packetWaiting = 1;
    runner->flags.packetEnded = 0;
    runner->packetEnd = 0;
    PropertyHolder* properties = node->PacketProperties();
    Translator* translator = nullptr;
    Waypoints* waypoints = node->waypoints;
    ObjectPlace* place = runner->agentNode->owner->place;
    MotionState* motion = node->motion;
    place->SyncPosition();
    Vector4 position = place->position;
    u32 selector = Packet::NoSlot;
    Vector4 focusPosition = {0.0f, 0.0f, 0.0f, 1.0f};
    s32 joint = GameOGI::NoJoint;
    Vector4* focus = nullptr;
    InstanceContext* instance = nullptr;
    LayoutPosition* key = nullptr;
    AiPosition* step = nullptr;
    u32 motionKind = runner->packet->settings.motion;
    node->flags.accelerates = 0;
    runner->packet->Word(Packet::Selector, &selector);

    f32 delay;
    if (runner->packet->GetFloat(Packet::Delay, properties, &delay))
    {
        runner->packetEnd = runner->packetStart + static_cast<s32>(delay * g_ClockUnitsPerSecond);
        f32 range;
        if (runner->packet->GetFloat(Packet::RandRange, properties, &range))
        {
            runner->packetEnd += static_cast<s32>(GetRandFloat() * range * g_ClockUnitsPerSecond);
        }
    }
    else if (runner->packet->Word2(Packet::SyncUnit, &runner->syncUnit))
    {
        InstanceContext* synced = runner->receivers->instances[static_cast<u8>(selector)];
        node->focusInstance = synced;
        if (synced != nullptr)
        {
            node->flags.focusInstance = 1;
        }

        node->flags.focusPosition = 0;
    }

    // What the packet goes to: the selector's receiver or the focus, else what its key names
    bool keyed = true;
    if (selector != Packet::NoSlot && runner->syncUnit == 0)
    {
        keyed = false;
        if (selector != DesignatesFocus)
        {
            instance = runner->receivers->instances[static_cast<u8>(selector)];
        }
        else
        {
            instance = node->AwakeFocus();
            if (instance == nullptr)
            {
                runner->EndPacket();
                return;
            }
        }
    }

    u32 keyIndex;
    if (keyed && runner->packet->Word(Packet::KeyIndex, &keyIndex) && keyIndex != Packet::NoSlot)
    {
        bool ends = false;
        switch (keyIndex)
        {
        case DesignatesPlayer:
            instance = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
            break;
        case DesignatesHeadTarget:
            if (node->headTracking != nullptr)
            {
                Reference* target = node->headTracking->target;
                instance = target != nullptr ? static_cast<InstanceContext*>(target->object) : nullptr;
            }

            ends = instance == nullptr;
            break;
        case DesignatesStoredPosition:
            focus = nullptr;
            if (node->flags.storedPosition)
            {
                focusPosition = node->storedPosition;
                focus = &focusPosition;
            }

            break;
        case DesignatesAgentRef2:
            if (node->agentRef2 != nullptr && node->agentRef2->flags.asleep &&
                !node->flags.keepsAgentRef2)
            {
                node->agentRef2 = nullptr;
            }

            instance = node->agentRef2;
            ends = instance == nullptr;
            break;
        case DesignatesAgentRef1:
            if (node->agentRef1 != nullptr && node->agentRef1->flags.asleep)
            {
                node->agentRef1 = nullptr;
            }

            instance = node->agentRef1;
            ends = instance == nullptr;
            break;
        case DesignatesPreviousStep:
            waypoints->PreviousRouteStep();
            step = waypoints->route != nullptr ? waypoints->route->PositionAt(waypoints->routeIndex) : nullptr;
            break;
        case DesignatesCurrentStep:
            step = waypoints->route != nullptr ? waypoints->route->PositionAt(waypoints->routeIndex) : nullptr;
            break;
        case DesignatesFocus:
            instance = node->AwakeFocus();
            ends = instance == nullptr;
            break;
        case DesignatesFocusPosition:
            focus = nullptr;
            if (node->flags.focusPosition)
            {
                focusPosition = node->focusPosition;
                focus = &focusPosition;
            }

            break;
        case DesignatesNextKey:
            waypoints->NextKey();
            key = waypoints->positions.data[waypoints->key];
            break;
        case DesignatesCurrentKey:
            key = waypoints->positions.data[waypoints->key];
            break;
        default:
        {
            // A key's index, from the first again past the last
            u32 count = waypoints->keyCount;
            waypoints->flags.wrapped = keyIndex == count;
            count = waypoints->keyCount;
            s32 index = static_cast<s32>(keyIndex) < static_cast<s32>(count) ? keyIndex : keyIndex - count;
            waypoints->key = index;
            key = waypoints->positions.data[static_cast<u8>(index)];
            break;
        }
        }

        if (ends)
        {
            runner->EndPacket();
            return;
        }

        // Following the keys, the rotation faces the next one
        constexpr u32 FollowsKeys = 2;
        if (key != nullptr && motionKind - Packet::LinearInterpolation < FollowsKeys && !waypoints->flags.stopped)
        {
            u32 current = waypoints->key;
            s32 count = waypoints->keyCount;
            s32 next = waypoints->flags.backwards ? current - 1 : current + 1;
            if (next >= count)
            {
                next -= count;
            }

            if (next < 0)
            {
                next += count;
            }

            KeyRotation(waypoints->positions.data[current], waypoints->positions.data[next], &node->keyRotation);
        }
    }

    if (runner->packet->settings.tracksDestination != 0)
    {
        node->tracked = instance;
    }

    runner->packet->GetInt(Packet::JointIndex, properties, &joint);
    runner->joint = static_cast<u8>(joint);

    if (runner->packet->settings.translates != 0)
    {
        translator = node->MakeTranslator();
        translator->start = position;
        if (runner->joint == GameOGI::NoJoint)
        {
            SetTranslationTarget(translator, runner, key, step, instance, focus);
        }

        motion->bits.translationDone = 0;
        if (motionKind != Packet::Projectile && translator->target.x == position.x && translator->target.y == position.y &&
            translator->target.z == position.z)
        {
            motion->bits.translationDone = 1;
        }
        else
        {
            switch (motionKind)
            {
            case Packet::ConstantVelocity:
            case Packet::Accelerated:
            case Packet::SmoothPath:
            case Packet::FaceDestinationOnly:
                break;
            case Packet::Projectile:
            {
                Physics* physics = node->MakePhysics();
                f32 gravity = DefaultProjectileGravity;
                runner->packet->GetFloat(Packet::Power, properties, &gravity);
                SetUpProjectile(gravity, physics, runner->packet, properties, &translator->direction);
                node->flags.moves = 1;
                break;
            }
            case Packet::Spring:
            {
                Physics* physics = node->MakePhysics();
                physics->bits.unused2 = Physics::KindSpring;
                f32 power = 0.0f;
                f32 damping = 0.0f;
                f32 deceleration = 0.0f;
                runner->packet->GetFloat(Packet::Power, properties, &power);
                runner->packet->GetFloat(Packet::Damping, properties, &damping);
                runner->packet->GetFloat(Packet::DecDist, properties, &deceleration);
                Vector4 pull = translator->target;
                pull.x = (pull.x - position.x) * power;
                pull.y = (pull.y - position.y) * power;
                pull.z = (pull.z - position.z) * power;
                physics->parameters[SpringPower] = power;
                physics->parameters[SpringDamping] = damping;
                physics->velocity = pull;
                physics->bits.negativeDeceleration = deceleration < 0.0f;
                break;
            }
            case Packet::GroundChase:
            case Packet::AirChase:
            case SecondGroundChase:
            case SecondAirChase:
            case Packet::ClimbingChase:
            {
                Physics* physics = node->MakePhysics();
                // Left as it was in retail when the packet gives no speed
                f32 speed = 0.0f;
                runner->packet->GetFloat(Packet::MoveSpeed, properties, &speed);
                motion->chaseSpeed = speed;
                motion->speed = runner->packet->HasByte(Packet::Damping) ? 0.0f : speed;
                Packet* packet = runner->packet;
                physics->bits.unused2 = Physics::KindChase;
                physics->parameters[ChaseLean] = 0.0f;
                physics->parameters[ChaseTurnDrag] = 0.0f;
                physics->parameters[ChaseTurnRate] = 1.0f;
                physics->parameters[ChaseMostLean] = DefaultChaseMostLean;
                packet->GetFloat(Packet::Duration, properties, &physics->parameters[ChaseTurnRate]);
                packet->GetFloat(Packet::Power, properties, &physics->parameters[ChaseLean]);
                packet->GetFloat(Packet::Bounce, properties, &physics->parameters[ChaseMostLean]);
                packet->GetFloat(Packet::Damping, properties, &physics->parameters[ChaseTurnDrag]);
                ObjectRigidBody* body = node->rigidBody;
                if (body != nullptr)
                {
                    body->state.followsSurface = motionKind == Packet::ClimbingChase;
                }

                break;
            }
            default:
            {
                // Straight to the target in the time its distance takes at the speed, accelerating and decelerating for the times
                // the packet gives
                node->flags.moves = 1;
                f32 speed = 0.0f;
                runner->packet->GetFloat(Packet::MoveSpeed, properties, &speed);
                f32 deceleration = 0.0f;
                f32 acceleration = 0.0f;
                bool accelerates = false;
                if (runner->packet->GetFloat(Packet::Bounce, properties, &acceleration))
                {
                    accelerates = true;
                }

                if (runner->packet->GetFloat(Packet::DecDist, properties, &deceleration))
                {
                    accelerates = true;
                }

                Vector4 delta;
                delta.x = translator->target.x - position.x;
                delta.y = translator->target.y - position.y;
                delta.z = translator->target.z - position.z;
                delta.w = 1.0f;
                motion->speed = speed;
                f32 distance = __builtin_sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                if (accelerates)
                {
                    node->flags.accelerates = 1;
                    translator->distance = distance;
                    motion->duration = AcceleratedDuration(distance, speed, acceleration, deceleration);
                }
                else
                {
                    motion->duration = distance / speed;
                }

                motion->inverseDuration = InverseOf(motion->duration, Infinite);
                f32 inverse = motion->inverseDuration;
                Vector4 velocity;
                velocity.x = delta.x * inverse;
                velocity.y = delta.y * inverse;
                velocity.z = delta.z * inverse;
                velocity.w = 1.0f;
                motion->velocity = velocity;
                if (accelerates || runner->packet->settings.acceleration == Packet::SmoothCurve)
                {
                    // It starts at rest, the velocity kept for when it's at full speed
                    motion->startVelocity = motion->velocity;
                    motion->velocity = {0.0f, 0.0f, 0.0f, 1.0f};
                }

                break;
            }
            }
        }
    }
    else if (motion != nullptr)
    {
        motion->bits.translationDone = 1;
        if (runner->packet->settings.yawFaces != 0)
        {
            translator = node->MakeTranslator();
            ObjectPlace* current = runner->agentNode->owner->place;
            current->SyncPosition();
            translator->start = current->position;
            SetTranslationTarget(translator, runner, key, step, instance, focus);
        }
    }

    // Read uninitialized in retail when the packet moves a joint
    Vector4 rotation = {};
    if (runner->packet->settings.rotates == 0)
    {
        motion->bits.rotationDone = 1;
    }
    else
    {
        motion->bits.rotationDone = 0;
        Rotator* rotator = node->MakeRotator();
        if (runner->joint != GameOGI::NoJoint)
        {
            rotator->start.x = rotation.x;
            rotator->start.y = rotation.y;
            rotator->start.z = rotation.z;
            rotator->start.w = rotation.w;
        }
        else
        {
            ObjectPlace* current = runner->agentNode->owner->place;
            current->SyncRotation();
            rotation.x = current->rotation.x;
            rotation.y = current->rotation.y;
            rotation.z = current->rotation.z;
            rotation.w = current->rotation.w;
            rotator->start.x = rotation.x;
            rotator->start.y = rotation.y;
            rotator->start.z = rotation.z;
            rotator->start.w = rotation.w;
        }

        Vector4* target = &rotator->target;
        if (runner->packet->settings.yawFaces != 0)
        {
            // Facing the translation, turned by the packet's angles when one of them is left out
            s32 angles[3];
            Packet* packet;
            if (runner->packet->GetRawAngles(Packet::Pitch, angles))
            {
                s32 x = angles[0];
                s32 y = angles[1];
                s32 z = angles[2];
                Vector4 turn;
                GetRotationXYZ(&turn, &x, &y, &z);
                packet = runner->packet;
                FaceDirection(target, translator);
                MultiplyRotations(target, target, &turn);
            }
            else
            {
                packet = runner->packet;
                FaceDirection(target, translator);
            }

            f32 snap;
            if (packet->Float(Packet::Damping, &snap))
            {
                SnapToYaw(target, snap);
            }
        }
        else if (runner->joint == GameOGI::NoJoint)
        {
            SetRotationTarget(rotator, runner, key, step, instance);
        }

        if (runner->packet->settings.translates == 0 && motionKind == Packet::LinearInterpolation)
        {
            // A turn in place: the time the angle takes at the turn speed (accelerating and decelerating as a translation does)
            f32 turnSpeed = 0.0f;
            s32 angle;
            if (runner->packet->GetAngle(Packet::TurnSpeed, properties, &angle))
            {
                turnSpeed = static_cast<f32>(angle) * AngleToRadians;
            }

            f32 deceleration = 0.0f;
            f32 acceleration = 0.0f;
            bool accelerates = false;
            if (runner->packet->GetFloat(Packet::Bounce, properties, &acceleration))
            {
                accelerates = true;
            }

            if (runner->packet->GetFloat(Packet::DecDist, properties, &deceleration))
            {
                accelerates = true;
            }

            if (!accelerates)
            {
                motion->turnSpeed = turnSpeed;
                s32 between;
                AngleBetweenRotations(&between, &rotation, target);
                s32 turns = between;
                s32 duration = *DivideAngle(&turns, motion->turnSpeed);
                motion->duration = static_cast<f32>(duration) * AngleToRadians;
                motion->inverseDuration = InverseOf(motion->duration, Infinite);
            }
            else
            {
                if (translator == nullptr)
                {
                    translator = node->MakeTranslator();
                }

                node->flags.accelerates = 1;
                motion->turnSpeed = turnSpeed;
                motion->speed = turnSpeed;
                s32 between;
                AngleBetweenRotations(&between, &rotation, target);
                f32 radians = static_cast<f32>(between) * AngleToRadians;
                translator->distance = radians;
                f32 duration = AcceleratedDuration(radians, turnSpeed, acceleration, deceleration);
                motion->duration = duration;
                if (duration < 0.0f)
                {
                    motion->duration = 0.0f;
                }

                // A turn of no time is over in a ten thousandth of a second
                constexpr f32 InstantTurnInverse = 10000.0f;
                motion->inverseDuration = InverseOf(motion->duration, InstantTurnInverse);
            }
        }

        if (__builtin_fabsf(rotation.x - target->x) <= Epsilon && __builtin_fabsf(rotation.y - target->y) <= Epsilon &&
            __builtin_fabsf(rotation.z - target->z) <= Epsilon && __builtin_fabsf(rotation.w - target->w) <= Epsilon)
        {
            motion->bits.rotationDone = 1;
        }
    }

    Trajectory* trajectory = node->trajectory;
    if (trajectory != nullptr)
    {
        if (runner->packet->settings.rotates != 0)
        {
            trajectory->rotation.x = rotation.x;
            trajectory->rotation.y = rotation.y;
            trajectory->rotation.z = rotation.z;
            trajectory->rotation.w = rotation.w;
            if (runner->joint != GameOGI::NoJoint)
            {
                AnglesOfRotation(&trajectory->rotation, &trajectory->angles[0], &trajectory->angles[1], &trajectory->angles[2]);
            }
        }

        if (runner->packet->settings.translates != 0)
        {
            trajectory->position = position;
        }
    }

    runner->tolerance = 0.0f;
    runner->packet->GetFloat(Packet::RandRange, properties, &runner->tolerance);
}

void CheckPacketEnd(BehaviourRunner* runner, TimeClock* clock, u32, InstanceContext* leftover)
{
    ControlPacket* packet = runner->packet;
    if (packet->settings.stalls != 0)
    {
        return;
    }

    bool over = false;
    if (g_PacketTimeBase < runner->packetEnd)
    {
        over = !(static_cast<s32>(clock->time) < runner->packetEnd);
    }

    if (over)
    {
        EndRunningPacket(runner);
        return;
    }

    if (runner->syncUnit == 0)
    {
        return;
    }

    auto* node = static_cast<ObjectNodeBase*>(runner->agentNode);
    InstanceContext* synced = leftover;
    if (node->flags.focusInstance && node->focusInstance != nullptr)
    {
        InstanceContext* focus = node->focusInstance;
        if (focus->flags.asleep)
        {
            node->flags.value &= ~ObjectNodeFlags::FocusMask;
            node->focusInstance = nullptr;
            synced = nullptr;
        }
        else
        {
            synced = focus;
        }
    }

    // The nodes are read whether there's an instance or not (the retail code's)
    auto* nodes = reinterpret_cast<NodeList*>(reinterpret_cast<u32>(synced) + offsetof(InstanceContext, nodes));
    if (LeftSyncState(static_cast<GameNode*>(GetGameNode(nodes, NodeObject)), runner->syncUnit) == 0)
    {
        return;
    }

    EndRunningPacket(runner);
}

void PacketFrame(BehaviourRunner* runner, TimeClock* clock)
{
    using Packet = ControlPacket;
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 start = place->position;
    u32 motionKind = runner->packet->settings.motion;
    u32 last = runner->agentNode->time;
    f32 elapsed = 0.0f;
    if (last != 0)
    {
        elapsed = static_cast<f32>(static_cast<s32>(clock->time - last)) * g_SecondsPerClockUnit;
    }

    Trajectory* trajectory = node->trajectory;
    MotionState* motion = node->motion;
    if (trajectory != nullptr)
    {
        HoldTrajectory(trajectory, node);
    }

    if (motion != nullptr && motionKind != Packet::NoMotion)
    {
        // The target follows what it tracks, and the instance's own position in the initial and current position spaces
        ControlPacketSettings settings = runner->packet->settings;
        u32 space = settings.space;
        if (settings.tracksDestination != 0 || space == InitialPosition || space == CurrentPosition)
        {
            FollowTarget(node->translator, runner, node);
        }

        if (motionKind == Packet::LinearInterpolation)
        {
            StepInterpolation(node, runner);
        }
        else
        {
            if (runner->packet->settings.rotates != 0 && !motion->bits.rotationDone &&
                StepRotation(node, runner) != 0)
            {
                motion->bits.rotationDone = 1;
                if (trajectory != nullptr)
                {
                    Rotator* rotator = node->rotator;
                    trajectory->rotation.x = rotator->target.x;
                    trajectory->rotation.y = rotator->target.y;
                    trajectory->rotation.z = rotator->target.z;
                    trajectory->rotation.w = rotator->target.w;
                    if (runner->joint != GameOGI::NoJoint)
                    {
                        AnglesOfRotation(&trajectory->rotation, &trajectory->angles[0], &trajectory->angles[1], &trajectory->angles[2]);
                    }
                }
            }

            if (runner->packet->settings.translates != 0 && !motion->bits.translationDone &&
                StepTranslation(node, runner, clock) != 0)
            {
                motion->bits.translationDone = 1;
                motion->bits.rotationDone = 1;
                if (trajectory != nullptr)
                {
                    trajectory->position = node->translator->target;
                }
            }
        }

        settings = runner->packet->settings;
        if (settings.orientsPredicts != 0)
        {
            // The instance faces the way it moves (unless that's straight up or down)
            Vector4 direction = motion->velocity;
            f32 inverse = InverseLength(&direction, LengthEpsilon);
            f32 x = direction.x * inverse;
            f32 z = direction.z * inverse;
            f32 y = direction.y * inverse;
            direction.x = x;
            direction.z = z;
            direction.y = y;
            if (FlatEpsilon < x * x + z * z)
            {
                ParticleTrails* trails = node->particleTrails;
                InstanceContext* owner = node->owner;
                Matrix4x4 look;
                Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
                LookAlong(&look, &direction, &up);
                if (trails == nullptr || !trails->bits.measuresTurn)
                {
                    ObjectPlace* current = owner->place;
                    current->SyncRotation();
                    Vector4 facing;
                    GetRotationVec(&facing, &look);
                    if (current->TurnTo(&facing))
                    {
                        QueueObject(owner);
                    }
                }
                else
                {
                    // The turn it makes measured too (times the frame's seconds)
                    ObjectPlace* current = owner->place;
                    current->SyncRotation();
                    Vector4 previous;
                    previous.x = current->rotation.x;
                    previous.y = current->rotation.y;
                    previous.z = current->rotation.z;
                    previous.w = current->rotation.w;
                    current = owner->place;
                    current->SyncRotation();
                    Vector4 facing;
                    GetRotationVec(&facing, &look);
                    if (current->TurnTo(&facing))
                    {
                        QueueObject(owner);
                    }

                    current = owner->place;
                    current->SyncRotation();
                    motion->turn.x = (current->rotation.x - previous.x) * elapsed;
                    motion->turn.y = (current->rotation.y - previous.y) * elapsed;
                    motion->turn.z = (current->rotation.z - previous.z) * elapsed;
                    motion->turn.w = (current->rotation.w - previous.w) * elapsed;
                }
            }
        }
        else if (settings.axes != Packet::NoNatural)
        {
            // It rolls along its natural axis over what it moved
            ObjectPlace* current = node->owner->place;
            current->SyncPosition();
            Vector4 moved = current->position;
            moved.x -= start.x;
            moved.y -= start.y;
            moved.z -= start.z;
            u32 axes = runner->packet->settings.axes;
            if (axes == Packet::XNatural)
            {
                RollAlongX(node->rollRadius, node, &moved);
            }
            else if (axes == Packet::YNatural)
            {
                RollAlongY(node->rollRadius, node, &moved);
            }
        }

        if (motion->bits.translationDone && motion->bits.rotationDone)
        {
            EndRunningPacket(runner);
            motion->bits.translationDone = 0;
            motion->bits.rotationDone = 0;
        }
    }

    if (runner->packet != nullptr)
    {
        // The retail code hands on whatever its last callee left in a3; the instance stands in for it
        CheckPacketEnd(runner, clock, 0, node->owner);
    }
}

void SetTranslationTarget(Translator* translator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step,
                          InstanceContext* instance, Vector4* position)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    ControlPacket* packet = runner->packet;
    PropertyHolder* properties = node->PacketProperties();
    // The offset (the RawPos values) is only added when one of them isn't given (the retail code's)
    Vector4 offset;
    bool offsets = packet->GetVector(ControlPacket::RawPosX, properties, &offset) != 0;
    Vector4* target = &translator->target;
    switch (packet->settings.space)
    {
    case WorldSpace:
        if (key != nullptr || step != nullptr || instance != nullptr || position != nullptr)
        {
            if (key != nullptr)
            {
                *target = key->position;
                target->w = 1.0f;
            }
            else if (step != nullptr)
            {
                *target = *reinterpret_cast<const Vector4*>(step);
                target->w = 1.0f;
            }
            else if (instance != nullptr)
            {
                ObjectPlace* place = instance->place;
                place->SyncPosition();
                *target = place->position;
            }
            else
            {
                *target = *position;
            }

            if (offsets)
            {
                target->x = target->x + offset.x;
                target->y = target->y + offset.y;
                target->z = target->z + offset.z;
            }
        }
        else if (offsets)
        {
            *target = offset;
        }
        else
        {
            *target = g_DefaultBox.min;
            target->w = 1.0f;
        }

        break;
    case InitialSpace:
    {
        const InstancePlacement* information = node->informationPointer;
        *target = information->position;
        if (offsets)
        {
            Matrix4x4 matrix;
            MatrixFromRotation(&matrix, &information->rotation);
            AddTransformed(target, &offset, &matrix);
        }

        break;
    }
    case CurrentSpace:
    {
        ObjectPlace* place = node->owner->place;
        place->SyncPosition();
        *target = place->position;
        if (offsets)
        {
            RotateAndTranslate(place);
            AddTransformed(target, &offset, &place->matrix);
        }

        break;
    }
    case TargetSpace:
        if (key == nullptr && step == nullptr && instance != nullptr)
        {
            ObjectPlace* place = instance->place;
            place->SyncPosition();
            *target = place->position;
            if (offsets)
            {
                place = instance->place;
                RotateAndTranslate(place);
                AddTransformed(target, &offset, &place->matrix);
            }
        }

        break;
    case ParentSpace:
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        *target = place->position;
        if (offsets)
        {
            Matrix4x4 matrix;
            MatrixFacingFrom(node->owner, instance, &matrix);
            AddTransformed(target, &offset, &matrix);
        }

        break;
    }
    case InitialPosition:
    {
        // The offset spread over the box of the part's object (x and z, from its middle), at the instance's own height, turned by
        // the object
        ObjectRigidBody* body = static_cast<ObjectNode*>(runner->agentNode)->rigidBody;
        if (body == nullptr || body->object == nullptr)
        {
            break;
        }

        ReferencedObject* object = body->object;
        const Box* box = &object->collision.ownBox;
        Vector4 max = box->max;
        Vector4 min = box->min;
        f32 halfX = (max.x - min.x) * 0.5f;
        f32 halfZ = (max.z - min.z) * 0.5f;
        offset.y = 0.0f;
        offset.x = offset.x * halfX + (halfX + min.x);
        offset.z = offset.z * halfZ + (halfZ + min.z);
        ObjectPlace* place = object->place;
        place->SyncPosition();
        *target = place->position;
        ObjectPlace* own = node->owner->place;
        own->SyncPosition();
        Vector4 at = own->position;
        target->y = at.y;
        place = object->place;
        place->SyncRotation();
        Vector4 rotation;
        rotation.x = place->rotation.x;
        rotation.y = place->rotation.y;
        rotation.z = place->rotation.z;
        rotation.w = place->rotation.w;
        Matrix4x4 matrix;
        MatrixFromRotation(&matrix, &rotation);
        AddTransformed(target, &offset, &matrix);
        break;
    }
    case CurrentPosition:
    {
        // The nearest point of the path it's on, the offset turned by the path's direction there
        Waypoints* waypoints = node->waypoints;
        LayoutPath* path = waypoints->paths.data[waypoints->pathIndex];
        if (path == nullptr)
        {
            break;
        }

        ObjectPlace* own = node->owner->place;
        own->SyncPosition();
        Vector4 at = own->position;
        f32 along = ClampFloat(NearestPointOnPath(path, &at, target), 0.0f, 1.0f);
        Vector4 direction;
        PathDirectionAt(along, path, &direction);
        Matrix4x4 matrix;
        MatrixFacing(&matrix, &direction);
        waypoints->pathParameter = along;
        waypoints->pathDirection = direction;
        if (offsets)
        {
            AddTransformed(target, &offset, &matrix);
        }

        break;
    }
    case StoredSpace:
    {
        ObjectPlace* place = node->owner->place;
        if (place == nullptr)
        {
            if (offsets)
            {
                *target = offset;
            }
            else
            {
                *target = g_DefaultBox.min;
                target->w = 1.0f;
            }

            break;
        }

        place->SyncPosition();
        *target = place->position;
        if (offsets)
        {
            RotateAndTranslate(place);
            AddTransformed(target, &offset, &place->matrix);
        }

        break;
    }
    }

    target->w = 1.0f;
    Vector4 direction;
    direction.x = target->x - translator->start.x;
    direction.y = target->y - translator->start.y;
    direction.z = target->z - translator->start.z;
    direction.w = 1.0f;
    translator->direction = direction;
}

void SetRotationTarget(Rotator* rotator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step, InstanceContext* instance)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    ControlPacket* packet = runner->packet;
    s32 angles[3];
    AngleFrom(&angles[0], 0.0f, AngleRadians);
    AngleFrom(&angles[1], 0.0f, AngleRadians);
    AngleFrom(&angles[2], 0.0f, AngleRadians);
    PropertyHolder* properties = node->PacketProperties();
    // The angles (Pitch, Yaw and Roll) turn the rotation only when one of them isn't given (the retail code's)
    bool turns = packet->GetAngles(ControlPacket::Pitch, properties, angles) != 0;
    Vector4* target = &rotator->target;
    auto turnBy = [&]() {
        if (!turns)
        {
            return;
        }

        s32 x = angles[0];
        s32 y = angles[1];
        s32 z = angles[2];
        Vector4 turn;
        GetRotationXYZ(&turn, &x, &y, &z);
        MultiplyRotations(target, target, &turn);
    };
    auto fromPlace = [&](ObjectPlace* place) {
        place->SyncRotation();
        target->x = place->rotation.x;
        target->y = place->rotation.y;
        target->z = place->rotation.z;
        target->w = place->rotation.w;
        turnBy();
    };
    auto fromKeys = [&]() {
        target->x = node->keyRotation.x;
        target->y = node->keyRotation.y;
        target->z = node->keyRotation.z;
        target->w = node->keyRotation.w;
        turnBy();
    };
    // The angles alone (none: the identity)
    auto fromAngles = [&]() {
        if (turns)
        {
            s32 x = angles[0];
            s32 y = angles[1];
            s32 z = angles[2];
            GetRotationXYZ(target, &x, &y, &z);
            return;
        }

        target->z = 0.0f;
        target->y = 0.0f;
        target->x = 0.0f;
        target->w = 1.0f;
    };

    switch (packet->settings.space)
    {
    case WorldSpace:
        if (key != nullptr || step != nullptr)
        {
            fromKeys();
        }
        else if (instance != nullptr)
        {
            fromPlace(instance->place);
        }
        else
        {
            fromAngles();
        }

        break;
    case InitialSpace:
    {
        const InstancePlacement* information = node->informationPointer;
        target->x = information->rotation.x;
        target->y = information->rotation.y;
        target->z = information->rotation.z;
        target->w = information->rotation.w;
        turnBy();
        break;
    }
    case CurrentSpace:
        fromPlace(node->owner->place);
        break;
    case TargetSpace:
        if (key != nullptr)
        {
            fromKeys();
        }
        else if (instance != nullptr)
        {
            fromPlace(instance->place);
        }
        else
        {
            fromAngles();
        }

        break;
    case InitialPosition:
    {
        ObjectRigidBody* body = static_cast<ObjectNode*>(runner->agentNode)->rigidBody;
        if (body == nullptr)
        {
            break;
        }

        if (body->object != nullptr)
        {
            fromPlace(body->object->place);
            break;
        }

        // Without the part's object the instance's own rotation, not turned
        ObjectPlace* own = static_cast<ObjectNode*>(runner->agentNode)->owner->place;
        own->SyncRotation();
        target->x = own->rotation.x;
        target->y = own->rotation.y;
        target->z = own->rotation.z;
        target->w = own->rotation.w;
        break;
    }
    case StoredSpace:
        if (node->storedPlace != nullptr)
        {
            fromPlace(node->storedPlace);
        }
        else
        {
            fromAngles();
        }

        break;
    default:
        break;
    }

    f32 snap;
    if (packet->Float(ControlPacket::Damping, &snap))
    {
        SnapToYaw(target, snap);
    }
}

void SetUpProjectile(f32 gravity, Physics* physics, ControlPacket* packet, PropertyHolder* properties, Vector4* direction)
{
    // A throw over the translation's direction: the flight's time (Duration) gives the speed up, else the height it rises to
    // (MoveSpeed), or the rebound of its speed down (Bounce) does and the flight's time follows
    physics->bits.unused2 = Physics::KindMade;
    Vector4 flat = *direction;
    f32 rise = flat.y;
    flat.y = 0.0f;
    f32 distance = __builtin_sqrtf(flat.x * flat.x + flat.z * flat.z);
    f32 up = 0.0f;
    f32 height = 0.0f;
    f32 duration = 0.0f;
    packet->GetFloat(ControlPacket::Duration, properties, &duration);
    MotionState* motion = physics->motion;
    if (duration != 0.0f)
    {
        motion->duration = duration;
        motion->inverseDuration = 1.0f / duration;
        up = (rise + gravity * (duration * 0.5f) * duration) / duration;
    }
    else
    {
        f32 bounce = 0.0f;
        packet->GetFloat(ControlPacket::Bounce, properties, &bounce);
        f32 twice = gravity + gravity;
        if (0.0f < bounce)
        {
            up = -physics->motion->velocity.y * bounce;
            height = up * up / twice;
        }
        else
        {
            packet->GetFloat(ControlPacket::MoveSpeed, properties, &height);
            up = __builtin_sqrtf(__builtin_fabsf(twice * height));
        }

        duration = (__builtin_sqrtf(__builtin_fabsf(twice * (height - rise))) + up) / gravity;
        motion = physics->motion;
        motion->duration = duration;
        motion->inverseDuration = 1.0f / duration;
    }

    motion = physics->motion;
    physics->parameters[ProjectileGravity] = gravity;
    physics->parameters[ProjectileRise] = up;
    physics->parameters[ProjectileSpeed] = distance * motion->inverseDuration;
    motion->velocity.y = up;
    f32 inverse = InverseLength(&flat, LengthEpsilon);
    f32 speed = physics->parameters[ProjectileSpeed];
    flat.x = flat.x * inverse * speed;
    flat.y = flat.y * inverse * speed;
    flat.z = flat.z * inverse * speed;
    motion->velocity.x = flat.x;
    motion->velocity.z = flat.z;
}

void FollowTarget(Translator* translator, BehaviourRunner* runner, ObjectNode* node)
{
    // The target made again (along the path it's on, or at what it tracks), and with a chance (AcDist) a new sideways wander of
    // up to DecDist added
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 own = place->position;
    ControlPacket* packet = runner->packet;
    InstanceContext* tracked = node->tracked;
    Vector4 toward;
    if (packet->settings.space == CurrentPosition)
    {
        toward = node->waypoints->pathDirection;
        SetTranslationTarget(translator, runner, nullptr, nullptr, nullptr, nullptr);
    }
    else
    {
        if (tracked == nullptr)
        {
            return;
        }

        ObjectPlace* trackedPlace = tracked->place;
        trackedPlace->SyncPosition();
        toward = trackedPlace->position;
        SetTranslationTarget(translator, runner, nullptr, nullptr, tracked, nullptr);
    }

    f32 chance;
    if (packet->Float(ControlPacket::AcDist, &chance) == 0)
    {
        return;
    }

    f32 amount;
    if (GetRandFloat() < chance && packet->Float(ControlPacket::DecDist, &amount) != 0)
    {
        toward.x -= own.x;
        toward.y -= own.y;
        toward.z -= own.z;
        f32 inverse = InverseLength(&toward, LengthEpsilon);
        toward.x = toward.x * inverse;
        toward.y = toward.y * inverse;
        toward.z = toward.z * inverse;
        Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
        Vector4 side = toward;
        side.z = up.x * toward.y - up.y * toward.x;
        side.y = up.z * toward.x - up.x * toward.z;
        side.x = up.y * toward.z - up.z * toward.y;
        f32 scale = RandomSignedTimes(amount);
        side.x = side.x * scale;
        side.y = side.y * scale;
        side.z = side.z * scale;
        translator->wander = side;
    }

    translator->target.x = translator->target.x + translator->wander.x;
    translator->target.y = translator->target.y + translator->wander.y;
    translator->target.z = translator->target.z + translator->wander.z;
}

void MoveInstance(ObjectNode* node, ControlPacket* packet, const Vector4* position)
{
    if (!node->flags.movesStoredPlace)
    {
        InstanceContext* owner = node->owner;
        ObjectPlace* place = owner->place;
        place->SyncPosition();
        if (place->MoveTo(position))
        {
            QueueObject(owner);
        }

        return;
    }

    ObjectPlace* stored = node->storedPlace;
    if (stored == nullptr || (packet != nullptr && packet->settings.space == StoredSpace))
    {
        return;
    }

    stored->SyncPosition();
    stored->MoveTo(position);
}

void TurnInstance(ObjectNode* node, ControlPacket* packet, const Vector4* rotation)
{
    if (!node->flags.movesStoredPlace)
    {
        InstanceContext* owner = node->owner;
        ObjectPlace* place = owner->place;
        place->SyncRotation();
        if (place->TurnTo(rotation))
        {
            QueueObject(owner);
        }

        return;
    }

    ObjectPlace* stored = node->storedPlace;
    if (stored == nullptr || (packet != nullptr && packet->settings.space == StoredSpace))
    {
        return;
    }

    stored->SyncRotation();
    stored->TurnTo(rotation);
}

void StepSpringPhysics(f32 elapsed, f32 damping, Physics* physics, const Vector4* force)
{
    physics->velocity = *force;
    physics->motion->startVelocity = physics->motion->velocity;
    MotionState* motion = physics->motion;
    Vector4& velocity = motion->velocity;
    if (0.0f <= damping)
    {
        f32 slowX = velocity.x * damping;
        f32 slowY = velocity.y * damping;
        f32 slowZ = velocity.z * damping;
        physics->velocity.x = physics->velocity.x - slowX;
        physics->velocity.y = physics->velocity.y - slowY;
        physics->velocity.z = physics->velocity.z - slowZ;
        velocity.x = velocity.x + physics->velocity.x * elapsed;
        velocity.y = velocity.y + physics->velocity.y * elapsed;
        velocity.z = velocity.z + physics->velocity.z * elapsed;
        return;
    }

    velocity.x = velocity.x + force->x * elapsed;
    velocity.y = velocity.y + force->y * elapsed;
    velocity.z = velocity.z + force->z * elapsed;
    if (damping * damping < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
    {
        f32 inverse = InverseLength(&velocity, LengthEpsilon);
        velocity.x = velocity.x * inverse;
        velocity.y = velocity.y * inverse;
        velocity.z = velocity.z * inverse;
        f32 speed = -damping;
        velocity.x = velocity.x * speed;
        velocity.y = velocity.y * speed;
        velocity.z = velocity.z * speed;
    }
}

u32 StepTranslation(ObjectNode* node, BehaviourRunner* runner, TimeClock* clock)
{
    f32 elapsed = 0.0f;
    if (node->time != 0)
    {
        elapsed = static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit;
    }

    ControlPacket* packet = runner->packet;
    switch (packet->settings.motion)
    {
    case ControlPacket::Spring:
        return StepSpring(elapsed, node, runner, packet);
    case ControlPacket::Projectile:
        return StepProjectile(elapsed, node, runner, packet);
    case ControlPacket::GroundChase:
    case SecondGroundChase:
        return StepGroundChase(elapsed, node, runner);
    case ControlPacket::AirChase:
    case SecondAirChase:
    {
        // Riding something with its rigid body
        if (node->rigidBody != nullptr && node->rigidBody->physicsBody != nullptr)
        {
            return StepRiddenAirChase(elapsed, node, runner);
        }

        return StepAirChase(elapsed, node, runner);
    }
    case ControlPacket::ClimbingChase:
        return StepClimbingChase(elapsed, node, runner);
    default:
        // The other motions move by their velocity alone: done at once
        return 1;
    }
}

u32 StepSpring(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet)
{
    // Pulled to the target by the power times the distance: done once within the tolerance, else (with a negative deceleration)
    // once it doesn't overshoot any more, else once the motion's and the physics' velocities have died down
    constexpr f32 Settled = 0x1.47ae14p-4f;
    f32 tolerance = runner->tolerance;
    Translator* translator = node->translator;
    const Vector4* target = &translator->target;
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    u32 joint = runner->joint;
    Vector4 position = place->position;
    Vector4 force = translator->target;
    force.x -= position.x;
    force.y -= position.y;
    force.z -= position.z;
    if (node->flags.falls)
    {
        force.y = 0.0f;
    }

    Physics* physics = node->physics;
    f32 power = physics->parameters[SpringPower];
    force.x = force.x * power;
    force.y = force.y * power;
    force.z = force.z * power;
    StepSpringPhysics(elapsed, physics->parameters[SpringDamping], physics, &force);
    MotionState* motion = node->motion;
    Trajectory* trajectory = node->trajectory;
    Vector4 step = motion->velocity;
    step.x = step.x * elapsed;
    step.y = step.y * elapsed;
    step.z = step.z * elapsed;
    Vector4 next = position;
    next.x = next.x + step.x;
    next.y = next.y + step.y;
    next.z = next.z + step.z;
    if (trajectory != nullptr)
    {
        trajectory->position = next;
    }

    if (tolerance != 0.0f)
    {
        f32 x = next.x - target->x;
        f32 y = next.y - target->y;
        f32 z = next.z - target->z;
        if (x * x + y * y + z * z < tolerance)
        {
            MoveInstance(node, packet, &next);
            return 1;
        }
    }

    physics = node->physics;
    if (physics->bits.negativeDeceleration)
    {
        if (__builtin_fabsf(target->x - position.x) < __builtin_fabsf(step.x))
        {
            next.x = target->x;
        }

        if (__builtin_fabsf(target->y - position.y) < __builtin_fabsf(step.y))
        {
            next.y = target->y;
        }

        if (__builtin_fabsf(target->z - position.z) < __builtin_fabsf(step.z))
        {
            next.z = target->z;
        }

        if (joint == GameOGI::NoJoint)
        {
            MoveInstance(node, packet, &next);
        }

        if (next.x == target->x && next.y == target->y && next.z == target->z)
        {
            return 1;
        }

        node->flags.unused4 = 1;
        return 0;
    }

    const Vector4& moving = node->motion->velocity;
    const Vector4& pulled = physics->velocity;
    f32 energy = moving.x * moving.x + moving.y * moving.y + moving.z * moving.z +
                 (pulled.x * pulled.x + pulled.y * pulled.y + pulled.z * pulled.z);
    if (joint != GameOGI::NoJoint)
    {
        return energy < Settled ? 1 : 0;
    }

    if (energy < Settled)
    {
        MoveInstance(node, packet, target);
        return 1;
    }

    MoveInstance(node, packet, &next);
    node->flags.unused4 = 1;
    return 0;
}

u32 StepProjectile(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet)
{
    // Thrown with gravity: done within the tolerance, else once its flight's time is up (landing on the target, or where it is;
    // turned to the rotation's target when it interpolates angles and doesn't skip the turn)
    node->flags.moves = 1;
    MotionState* motion = node->motion;
    Translator* translator = node->translator;
    Vector4 velocity = motion->velocity;
    f32 gravity = node->physics->parameters[ProjectileGravity];
    TimeClock* clock = GetContextClock(node->owner);
    f32 flight = static_cast<f32>(static_cast<s32>(clock->time - runner->packetStart)) * g_SecondsPerClockUnit;
    f32 tolerance = runner->tolerance;
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 next = place->position;
    f32 stepY = velocity.y * elapsed;
    f32 stepX = velocity.x * elapsed;
    f32 stepZ = velocity.z * elapsed;
    next.y = next.y + stepY - gravity * (elapsed * elapsed * 0.5f);
    velocity.y = velocity.y - gravity * elapsed;
    next.x = next.x + stepX;
    next.z = next.z + stepZ;
    motion = node->motion;
    motion->startVelocity = motion->velocity;
    motion->velocity = velocity;
    bool done;
    if (tolerance != 0.0f)
    {
        f32 x = next.x - translator->target.x;
        f32 y = next.y - translator->target.y;
        f32 z = next.z - translator->target.z;
        done = x * x + y * y + z * z < tolerance;
        if (done)
        {
            MoveInstance(node, packet, &next);
        }
    }
    else
    {
        done = node->motion->duration <= flight;
        if (done)
        {
            MoveInstance(node, packet, packet->settings.landsWhereItIs != 0 ? &next : &translator->target);
            ControlPacketSettings settings = packet->settings;
            if (settings.interpolatesAngles != 0 && settings.skipsTurn == 0)
            {
                TurnInstance(node, packet, &node->rotator->target);
            }
        }
    }

    if (done)
    {
        node->motion->bits.rotationDone = 1;
        return 1;
    }

    MoveInstance(node, packet, &next);
    node->flags.unused4 = 1;
    return 0;
}

u32 StepRotation(ObjectNode* node, BehaviourRunner* runner)
{
    if (node->flags.accelerates)
    {
        return StepAcceleratedRotation(node, runner);
    }

    TimeClock* clock = GetContextClock(node->owner);
    f32 t = static_cast<f32>(static_cast<s32>(clock->time - runner->packetStart)) * g_SecondsPerClockUnit *
            node->motion->inverseDuration;
    u32 joint = runner->joint;
    if (1.0f <= t || t < -1.0f)
    {
        if (joint == GameOGI::NoJoint)
        {
            TurnInstance(node, runner->packet, &node->rotator->target);
        }

        node->motion->bits.rotationDone = 1;
        return 1;
    }

    ControlPacket* packet = runner->packet;
    if (packet->settings.acceleration == ControlPacket::SmoothCurve)
    {
        t = t * (t * 3.0f) - (t + t) * t * t;
    }

    Rotator* rotator = node->rotator;
    Vector4 rotation;
    SlerpRotations(t, &rotation, &rotator->start, &rotator->target);
    if (joint == GameOGI::NoJoint)
    {
        TurnInstance(node, packet, &rotation);
    }

    return 0;
}

u32 StepAcceleratedRotation(ObjectNode* node, BehaviourRunner* runner)
{
    TimeClock* clock = GetContextClock(node->owner);
    f32 seconds = static_cast<f32>(static_cast<s32>(clock->time - runner->packetStart)) * g_SecondsPerClockUnit;
    f32 t = seconds * node->motion->inverseDuration;
    if (1.0f <= t || t < -1.0f)
    {
        TurnInstance(node, runner->packet, &node->rotator->target);
        node->motion->bits.rotationDone = 1;
        return 1;
    }

    f32 along = AcceleratedFraction(node, runner->packet, seconds);
    Rotator* rotator = node->rotator;
    Vector4 rotation;
    SlerpRotations(along, &rotation, &rotator->start, &rotator->target);
    TurnInstance(node, runner->packet, &rotation);
    return 0;
}

f32 AcceleratedFraction(ObjectNode* node, ControlPacket* packet, f32 seconds)
{
    // The distance covered speeding up for the acceleration's time, at full speed, and slowing down for the deceleration's
    // time, over the whole distance (Bounce and DecDist are the times; left as they were in retail when not given, and unused)
    constexpr f32 NoDistance = 1000000.0f;
    f32 duration = node->motion->duration;
    f32 times[2] = {};
    bool accelerates = packet->GetFloat(ControlPacket::Bounce, node->properties, &times[0]) != 0;
    bool decelerates = packet->GetFloat(ControlPacket::DecDist, node->properties, &times[1]) != 0;
    f32 speeding = ClampFloat(times[0], 0.0f, duration);
    f32 slowing = ClampFloat(times[1], 0.0f, duration);
    f32 slowFrom = duration - slowing;
    f32 speed = node->motion->speed;
    f32 covered;
    if (accelerates && seconds < speeding)
    {
        covered = speed / speeding * seconds * seconds * 0.5f;
    }
    else if (decelerates && slowFrom < seconds)
    {
        f32 speedUp = 0.0f;
        if (accelerates)
        {
            speedUp = speed / speeding * speeding * speeding * 0.5f;
        }

        f32 left = slowing - (seconds - slowFrom);
        f32 rate = speed / slowing;
        covered = speedUp + speed * (duration - speeding - slowing) + rate * slowing * slowing * 0.5f - rate * left * left * 0.5f;
    }
    else if (accelerates)
    {
        covered = speed / speeding * speeding * speeding * 0.5f + speed * (seconds - speeding);
    }
    else
    {
        covered = speed * seconds;
    }

    f32 distance = node->translator->distance;
    if (__builtin_fabsf(distance) <= Epsilon)
    {
        return NoDistance;
    }

    return covered / distance;
}

void SmoothVelocity(f32 t, MotionState* motion, const Translator* translator)
{
    Vector4& velocity = motion->velocity;
    velocity = translator->target;
    velocity.x = velocity.x - translator->start.x;
    velocity.y = velocity.y - translator->start.y;
    velocity.z = velocity.z - translator->start.z;
    f32 inverse = InverseLength(&velocity, LengthEpsilon);
    velocity.x = velocity.x * inverse;
    f32 speed = (t - t * t) * 6.0f;
    velocity.y = velocity.y * inverse;
    velocity.z = velocity.z * inverse;
    velocity.x = velocity.x * speed;
    velocity.y = velocity.y * speed;
    velocity.z = velocity.z * speed;
}

u32 StepInterpolation(ObjectNode* node, BehaviourRunner* runner)
{
    node->flags.moves = 1;
    if (node->flags.accelerates)
    {
        return StepAcceleratedInterpolation(node, runner);
    }

    ControlPacket* packet = runner->packet;
    u32 joint = runner->joint;
    TimeClock* clock = GetContextClock(node->owner);
    f32 t = static_cast<f32>(static_cast<s32>(clock->time - runner->packetStart)) * g_SecondsPerClockUnit *
            node->motion->inverseDuration;
    if (1.0f <= t || t <= -1.0f)
    {
        return FinishInterpolation(node, packet, joint);
    }

    if (packet->settings.acceleration == ControlPacket::SmoothCurve)
    {
        t = t * (t * 3.0f) - (t + t) * t * t;
    }

    return Interpolate(node, runner, packet, joint, t);
}

u32 StepAcceleratedInterpolation(ObjectNode* node, BehaviourRunner* runner)
{
    ControlPacket* packet = runner->packet;
    u32 joint = runner->joint;
    TimeClock* clock = GetContextClock(node->owner);
    f32 seconds = static_cast<f32>(static_cast<s32>(clock->time - runner->packetStart)) * g_SecondsPerClockUnit;
    f32 t = seconds * node->motion->inverseDuration;
    if (1.0f <= t || t <= -1.0f)
    {
        return FinishInterpolation(node, packet, joint);
    }

    return Interpolate(node, runner, packet, joint, AcceleratedFraction(node, packet, seconds));
}

namespace
{
// The instance moved by a step unless it's too small to see, the node unsettled
void ChaseMove(ObjectNode* node, const Vector4* move)
{
    InstanceContext* owner = node->owner;
    ObjectPlace* place = owner->place;
    if (!(__builtin_fabsf(move->x) <= Epsilon && __builtin_fabsf(move->y) <= Epsilon && __builtin_fabsf(move->z) <= Epsilon))
    {
        place->SyncPosition();
        place->MarkMoved();
        place->position.x = place->position.x + move->x;
        place->position.y = place->position.y + move->y;
        place->position.z = place->position.z + move->z;
        QueueObject(owner);
    }

    node->flags.unused4 = 1;
}

// Whether a chase is within its tolerance of its target (never without one), and where it is
bool ChaseArrived(ObjectNode* node, BehaviourRunner* runner, Vector4* here)
{
    Translator* translator = node->translator;
    ObjectPlace* place = node->owner->place;
    f32 tolerance = runner->tolerance;
    place->SyncPosition();
    *here = place->position;
    if (tolerance == 0.0f)
    {
        return false;
    }

    f32 dx = here->x - translator->target.x;
    f32 dy = here->y - translator->target.y;
    f32 dz = here->z - translator->target.z;
    return dx * dx + dy * dy + dz * dz < tolerance;
}

// The cross product a × b (its W 1)
Vector4 Cross(const Vector4& a, const Vector4& b)
{
    Vector4 cross;
    cross.x = a.y * b.z - a.z * b.y;
    cross.y = a.z * b.x - a.x * b.z;
    cross.z = a.x * b.y - a.y * b.x;
    cross.w = 1.0f;
    return cross;
}
}

f32 TurnSlowedSpeed(f32 turn, Physics* physics, MotionState* motion)
{
    // The share of the speed taken away: the turn times the drag in hundredths, 90% at most
    constexpr f32 DragScale = 100.0f;
    constexpr f32 MostSlowed = Rounded(0.9);
    f32 drag = physics->parameters[ChaseTurnDrag];
    f32 speed = motion->chaseSpeed;
    if (0.0f < drag)
    {
        f32 slowed = turn * (drag * DragScale);
        if (MostSlowed < slowed)
        {
            slowed = MostSlowed;
        }

        speed = speed - speed * slowed;
    }

    motion->speed = speed;
    return speed;
}

u32 StepGroundChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner)
{
    Vector4 here;
    if (ChaseArrived(node, runner, &here))
    {
        return 1;
    }

    Translator* translator = node->translator;
    // Without a rigid body steering itself the motion's velocity is the chase's, its vertical speed kept
    bool free = node->rigidBody == nullptr || !node->rigidBody->bits.steersItself;
    Physics* physics = node->physics;
    f32 turned;
    if (free)
    {
        turned = SteerTowards(physics->parameters[ChaseTurnRate] * elapsed, physics->parameters[ChaseLean],
                              physics->parameters[ChaseMostLean], node->owner, &translator->target);
    }
    else
    {
        turned = SteerBodyTowards(physics->parameters[ChaseTurnRate] * elapsed, physics->parameters[ChaseLean], node->owner,
                                  &translator->target, 0);
    }

    f32 turn = elapsed * turned;
    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    Vector4 up = *RowOf(&place->matrix, 1);
    f32 speed = TurnSlowedSpeed(turn, node->physics, node->motion);
    Vector4 move;
    if (free)
    {
        place = node->owner->place;
        RotateAndTranslate(place);
        move = *RowOf(&place->matrix, 2);
        move.x *= speed;
        move.y *= speed;
        move.z *= speed;
        MotionState* motion = node->motion;
        f32 fall = motion->velocity.y;
        motion->startVelocity = motion->velocity;
        motion->velocity = move;
        node->motion->velocity.y = node->motion->velocity.y + fall;
    }
    else
    {
        RightRigidBody(elapsed, node->rigidBody, &up);
        place = node->owner->place;
        RotateAndTranslate(place);
        move = *RowOf(&place->matrix, 2);
        ObjectRigidBody* body = node->rigidBody;
        move.x *= speed;
        move.y *= speed;
        move.z *= speed;
        MotionState* motion = node->motion;
        motion->startVelocity = motion->velocity;
        // Having touched something it takes the whole velocity, its vertical speed included
        if (body->state.touched)
        {
            motion->velocity = move;
        }
        else
        {
            motion->velocity.x = move.x;
            motion->velocity.z = move.z;
        }
    }

    move.x *= elapsed;
    move.y *= elapsed;
    move.z *= elapsed;
    ObjectRigidBody* body = node->rigidBody;
    if (body != nullptr && (body->bits.value & ObjectRigidBodyBits::KindsMask) != 0)
    {
        HoldRigidBodyMove(body, &move);
    }

    ChaseMove(node, &move);
    return 0;
}

u32 StepAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner)
{
    Vector4 here;
    if (ChaseArrived(node, runner, &here))
    {
        return 1;
    }

    Physics* physics = node->physics;
    f32 turned = SteerBodyTowards(physics->parameters[ChaseTurnRate] * elapsed, physics->parameters[ChaseLean], node->owner,
                                  &node->translator->target, 1);
    f32 turn = elapsed * turned;
    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    Vector4 move = *RowOf(&place->matrix, 2);
    f32 speed = TurnSlowedSpeed(turn, node->physics, node->motion);
    move.x = move.x * speed * elapsed;
    move.y = move.y * speed * elapsed;
    move.z = move.z * speed * elapsed;
    ObjectRigidBody* body = node->rigidBody;
    if (body != nullptr && (body->bits.value & ObjectRigidBodyBits::KindsMask) != 0)
    {
        HoldRigidBodyMove(body, &move);
    }

    ChaseMove(node, &move);
    return 0;
}

u32 StepRiddenAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner)
{
    // A target straight above or below is taken a little to the side
    constexpr f32 Nudge = Rounded(0.01);
    Vector4 here;
    if (ChaseArrived(node, runner, &here))
    {
        return 1;
    }

    ObjectPlace* place = node->owner->place;
    DynamicBody* ridden = node->rigidBody->physicsBody;
    RotateAndTranslate(place);
    Vector4 forward = *RowOf(&place->matrix, 2);
    Physics* physics = node->physics;
    Vector4 riddenForward = *RowOf(&ridden->matrix, 2);
    f32 turn = physics->parameters[ChaseTurnRate] * elapsed * (ridden->mass * FramesPerSecond);
    Vector4 direction = node->translator->target;
    direction.x = direction.x - here.x;
    direction.y = direction.y - here.y;
    direction.z = direction.z - here.z;
    if (direction.x == 0.0f && direction.z == 0.0f)
    {
        direction.x = Nudge;
    }

    turn = -turn;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    // The ridden body spun about y toward the target
    Vector4 yaw = {0.0f, 0.0f, 0.0f, 1.0f};
    alignas(16) s32 angle[4];
    SignedAngleAboutY(angle, &riddenForward, &direction);
    yaw.y = static_cast<f32>(angle[0]) * AngleToRadians * turn;
    ridden->torque.x = ridden->torque.x + yaw.x;
    ridden->torque.y = ridden->torque.y + yaw.y;
    ridden->torque.z = ridden->torque.z + yaw.z;
    // And its up turned toward the world's up away from the direction
    Vector4 worldUp = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 side = Cross(worldUp, direction);
    Vector4 upright = Cross(side, direction);
    inverse = InverseLength(&upright, LengthEpsilon);
    upright.x = -(upright.x * inverse);
    upright.y = -(upright.y * inverse);
    upright.z = -(upright.z * inverse);
    Vector4 riddenUp = *RowOf(&ridden->matrix, 1);
    Vector4 axis = Cross(riddenUp, upright);
    ridden->torque.x = ridden->torque.x + axis.x * turn;
    ridden->torque.y = ridden->torque.y + axis.y * turn;
    ridden->torque.z = ridden->torque.z + axis.z * turn;
    // Pushed along the chaser's facing at twice its speed
    MotionState* motion = node->motion;
    f32 twice = motion->speed + motion->speed;
    forward.x = forward.x * twice;
    forward.y = forward.y * twice;
    forward.z = forward.z * twice;
    f32 scale = ridden->mass;
    ridden->force.x = ridden->force.x + forward.x * scale;
    ridden->force.y = ridden->force.y + forward.y * scale;
    ridden->force.z = ridden->force.z + forward.z * scale;
    return 0;
}

u32 StepClimbingChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner)
{
    // What it touches is a floor when its normal's y is above this
    constexpr f32 FloorSlope = Rounded(0.3);

    Vector4 here;
    if (ChaseArrived(node, runner, &here))
    {
        return 1;
    }

    const Vector4* target = &node->translator->target;
    ObjectRigidBody* body = node->rigidBody;
    // On a floor below its target it chases as on the ground, else it climbs what it touches toward the target (turned to face it
    // with its back to the surface)
    bool onFloor = body == nullptr || FloorSlope < body->contactNormal.y;
    bool grounded = onFloor && here.y <= target->y;
    Vector4 normal;
    Vector4 direction;
    if (grounded)
    {
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        Physics* physics = node->physics;
        SteerTowards(physics->parameters[ChaseTurnRate] * elapsed, physics->parameters[ChaseLean],
                     physics->parameters[ChaseMostLean], node->owner, target);
    }
    else
    {
        normal = body->contactNormal;
        f32 inverse = InverseLength(&normal, LengthEpsilon);
        normal.x = normal.x * inverse;
        normal.y = normal.y * inverse;
        normal.z = normal.z * inverse;
        // The point of its box's middle the surface touches
        const Box* box = node->owner->CollisionBox();
        Vector4 foot = box->max;
        foot.x = (foot.x - box->min.x) * 0.5f + box->min.x;
        foot.y = (foot.y - box->min.y) * 0.5f + box->min.y;
        foot.z = (foot.z - box->min.z) * 0.5f + box->min.z;
        normal.w = 1.0f;
        foot.w = 1.0f;
        f32 radius = node->rollRadius;
        Vector4 reach = {normal.x * radius, normal.y * radius, normal.z * radius, 1.0f};
        foot.x = foot.x - reach.x;
        foot.y = foot.y - reach.y;
        foot.z = foot.z - reach.z;
        Vector4 plane;
        PlaneThrough(&plane, &normal, &foot);
        Vector4 goal = *target;
        // Touching the world, the target is taken onto the plane of what it touches
        if (node->rigidBody->bits.touchingWorld)
        {
            ProjectOntoPlane(&plane, target, &goal);
        }

        direction = {goal.x - foot.x, goal.y - foot.y, goal.z - foot.z, 1.0f};
        inverse = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;
        Vector4 side = Cross(normal, direction);
        inverse = InverseLength(&side, LengthEpsilon);
        side.x = side.x * inverse;
        side.y = side.y * inverse;
        side.z = side.z * inverse;
        Vector4 forward = direction;
        Vector4 across = side;
        if (target->y - here.y < 0.0f)
        {
            forward.x = -forward.x;
            forward.y = -forward.y;
            forward.z = -forward.z;
            across.x = -across.x;
            across.y = -across.y;
            across.z = -across.z;
        }

        Vector4 back = normal;
        back.x = -back.x;
        back.y = -back.y;
        back.z = -back.z;
        f32 share = node->physics->parameters[ChaseTurnRate] * elapsed;
        Vector4 origin = {0.0f, 0.0f, 0.0f, 1.0f};
        Matrix4x4 facing;
        MatrixFromRows(&facing, &across, &forward, &back, &origin);
        share = share < 1.0f ? share : 1.0f;
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        Vector4 from;
        GetRotationVec(&from, &place->matrix);
        f32 scale = InverseLength4(0.0f, InverseEpsilon, &from);
        from.x = from.x * scale;
        from.y = from.y * scale;
        from.z = from.z * scale;
        from.w = from.w * scale;
        // (GetRotationVec reads the matrix alone)
        Vector4 to;
        GetRotationVec(&to, &facing);
        scale = InverseLength4(0.0f, InverseEpsilon, &to);
        to.x = to.x * scale;
        to.y = to.y * scale;
        to.z = to.z * scale;
        to.w = to.w * scale;
        Vector4 turned;
        SlerpRotations(share * elapsed, &turned, &from, &to);
        InstanceContext* owner = node->owner;
        place = owner->place;
        place->SyncRotation();
        if (place->TurnTo(&turned))
        {
            QueueObject(owner);
        }
    }

    // The turn doesn't slow it
    f32 speed = TurnSlowedSpeed(0.0f, node->physics, node->motion);
    Vector4 move;
    if (grounded)
    {
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        move = *RowOf(&place->matrix, 2);
        move.x *= speed;
        move.y *= speed;
        move.z *= speed;
        MotionState* motion = node->motion;
        f32 fall = motion->velocity.y;
        motion->startVelocity = motion->velocity;
        motion->velocity = move;
        node->motion->velocity.y = node->motion->velocity.y + fall;
    }
    else
    {
        Vector4 along = {direction.x, direction.y, direction.z, 1.0f};
        if (direction.y < 0.0f)
        {
            // Going down, pressed into the surface the harder the nearer its normal's y is to a half (2 on a floor or a wall, 6
            // on a slope of 60°)
            constexpr f32 LeastPress = 2.0f;
            constexpr f32 PressGrowth = 8.0f;
            f32 press = -(__builtin_fabsf(0.5f - __builtin_fabsf(0.5f - normal.y)) * PressGrowth + LeastPress);
            move = {(along.x + normal.x * press) * 0.5f, (along.y + normal.y * press) * 0.5f, (along.z + normal.z * press) * 0.5f,
                    1.0f};
        }
        else
        {
            move = {along.x + -normal.x, along.y + -normal.y, along.z + -normal.z, 1.0f};
        }

        move.x *= speed;
        move.y *= speed;
        move.z *= speed;
        MotionState* motion = node->motion;
        motion->startVelocity = motion->velocity;
        motion->velocity = move;
    }

    move.x *= elapsed;
    move.y *= elapsed;
    move.z *= elapsed;
    body = node->rigidBody;
    if (body != nullptr && (body->bits.value & ObjectRigidBodyBits::KindsMask) != 0)
    {
        HoldRigidBodyMove(body, &move);
    }

    ChaseMove(node, &move);
    return 0;
}

void RollAlongX(f32 radius, ObjectNode* node, Vector4* moved)
{
    f32 length = Kept(__builtin_sqrtf(moved->x * moved->x + moved->y * moved->y + moved->z * moved->z));
    f32 inverse = 1.0f / length;
    f32 turn = length * radius;
    moved->z = moved->z * inverse;
    moved->x = moved->x * inverse;
    moved->y = moved->y * inverse;
    s32 angle;
    AngleFrom(&angle, turn, AngleRadians);
    InstanceContext* owner = node->owner;
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    Vector4 forward = *RowOf(&place->matrix, 2);
    // Rolling forward over what it moved, backward when the move goes the other way round its facing
    Vector4 axis = Cross(*moved, forward);
    if (axis.y < 0.0f)
    {
        angle = -angle;
    }

    place = owner->place;
    if (angle == 0)
    {
        return;
    }

    place->SyncRotation();
    place->MarkTurned();
    s32 roll = angle;
    Vector4 rotation;
    RotationFromRoll(&rotation, &roll);
    MultiplyRotations(&place->rotation, &place->rotation, &rotation);
    QueueObject(owner);
}

void RollAlongY(f32 radius, ObjectNode* node, Vector4* moved)
{
    constexpr f32 Least = Rounded(0.001);
    f32 length = __builtin_sqrtf(moved->x * moved->x + moved->y * moved->y + moved->z * moved->z);
    if (!(Least < length))
    {
        return;
    }

    // A debug text the retail code makes and drops
    String text;
    StringConstruct(&text, "FRAME: BMOT ");
    String number;
    StringConstructFloat(&number, length);
    StringAppend(&text, number.string);
    StringDestroy(&number);
    f32 inverse = 1.0f / length;
    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    f32 turn = length * radius;
    moved->z = moved->z * inverse;
    moved->x = moved->x * inverse;
    moved->y = moved->y * inverse;
    s32 angle;
    AngleFrom(&angle, turn, AngleRadians);
    // Rolling about the horizontal axis across what it moved
    Vector4 axis = Cross(up, *moved);
    s32 roll = angle;
    Vector4 rotation;
    RotationAboutAxis(&rotation, &axis, &roll, 0);
    InstanceContext* owner = node->owner;
    if (TurnPlace(owner->place, &rotation) != 0)
    {
        QueueObject(owner);
    }

    StringDestroy(&text);
}

f32 SteerTowards(f32 share, f32 lean, f32 mostLean, InstanceContext* instance, const Vector4* target)
{
    // A facing within this cosine of the target is facing it
    constexpr f32 Facing = Rounded(0.99999);
    bool leans = lean != 0.0f;
    ObjectPlace* place = instance->place;
    share = share < 1.0f ? share : 1.0f;
    place->SyncPosition();
    Vector4 here = place->position;
    Vector4 toward = *target;
    toward.x = toward.x - here.x;
    toward.z = toward.z - here.z;
    toward.y = 0.0f;
    f32 inverse = InverseLength(&toward, LengthEpsilon);
    toward.x = toward.x * inverse;
    toward.y = toward.y * inverse;
    toward.z = toward.z * inverse;
    if (toward.x == 0.0f && toward.z == 0.0f)
    {
        return 0.0f;
    }

    place = instance->place;
    RotateAndTranslate(place);
    Vector4 facing = *RowOf(&place->matrix, 2);
    facing.y = 0.0f;
    Vector4 was = facing;
    if (Facing < was.x * toward.x + was.y * toward.y + was.z * toward.z)
    {
        return 0.0f;
    }

    TurnToward(share, &facing, &facing, &toward);
    inverse = InverseLength(&facing, LengthEpsilon);
    facing.x = facing.x * inverse;
    facing.y = facing.y * inverse;
    facing.z = facing.z * inverse;
    f32 sign = 1.0f;
    f32 turned = 1.0f - (was.x * facing.x + was.y * facing.y + was.z * facing.z);
    Vector4 side;
    Vector4 up;
    if (leans)
    {
        // Leaning out of the turn's side
        side = Cross(facing, toward);
        if (0.0f < side.y)
        {
            sign = -1.0f;
        }
    }

    AxesAround(&facing, &side, &up);
    // (GetRotationVec reads the matrix's rotation alone)
    Vector4 corner = {0.0f, 0.0f, 1.0f, 1.0f};
    Matrix4x4 axes;
    MatrixFromRows(&axes, &side, &up, &facing, &corner);
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &axes);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    if (!leans)
    {
        return turned;
    }

    f32 tilt = turned * sign * lean;
    if (mostLean < tilt)
    {
        tilt = mostLean;
    }
    else if (tilt < -mostLean)
    {
        tilt = -mostLean;
    }

    s32 angle;
    AngleFrom(&angle, tilt, AngleDegrees);
    place = instance->place;
    if (angle == 0)
    {
        return turned;
    }

    place->SyncRotation();
    place->MarkTurned();
    s32 roll = angle;
    Vector4 leaning;
    RotationFromRoll(&leaning, &roll);
    MultiplyRotations(&place->rotation, &place->rotation, &leaning);
    QueueObject(instance);
    return turned;
}

f32 SteerBodyTowards(f32 share, f32 lean, InstanceContext* instance, const Vector4* target, u32 facingOnly)
{
    // How far the up leans: the target's direction times the lean, raised and added to the current up
    constexpr f32 Raise = 1.5f;
    constexpr f32 Keep = 1.25f;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 toward = *target;
    Vector4 here = place->position;
    toward.x = toward.x - here.x;
    toward.y = toward.y - here.y;
    toward.z = toward.z - here.z;
    f32 inverse = InverseLength(&toward, LengthEpsilon);
    share = share < 1.0f ? share : 1.0f;
    toward.x = toward.x * inverse;
    toward.y = toward.y * inverse;
    toward.z = toward.z * inverse;
    place = instance->place;
    RotateAndTranslate(place);
    Vector4 facing = *RowOf(&place->matrix, 2);
    Matrix4x4 axes;
    InitIdentityMatrix(&axes);
    f32 turned;
    if (lean != 0.0f)
    {
        Vector4 up = toward;
        up.x = toward.x * lean;
        up.y = toward.y * lean + Raise;
        up.z = toward.z * lean;
        place = instance->place;
        RotateAndTranslate(place);
        Vector4 current = *RowOf(&place->matrix, 1);
        current.x = current.x * Keep;
        current.y = current.y * Keep;
        current.z = current.z * Keep;
        up.x = up.x + current.x;
        up.y = up.y + current.y;
        up.z = up.z + current.z;
        inverse = InverseLength(&up, LengthEpsilon);
        up.x = up.x * inverse;
        up.y = up.y * inverse;
        up.z = up.z * inverse;
        Vector4 was = facing;
        TurnToward(share, &facing, &facing, &toward);
        inverse = InverseLength(&facing, LengthEpsilon);
        facing.x = facing.x * inverse;
        facing.y = facing.y * inverse;
        facing.z = facing.z * inverse;
        Vector4 side = Cross(facing, up);
        side.x = -side.x;
        side.y = -side.y;
        side.z = -side.z;
        Vector4 upright = Cross(facing, side);
        turned = 1.0f - (was.x * facing.x + was.y * facing.y + was.z * facing.z);
        Vector4 corner = {0.0f, 0.0f, 0.0f, 1.0f};
        MatrixFromRows(&axes, &side, &upright, &facing, &corner);
    }
    else
    {
        place = instance->place;
        RotateAndTranslate(place);
        Vector4 up = *RowOf(&place->matrix, 1);
        Vector4 was = facing;
        TurnToward(share, &facing, &facing, &toward);
        inverse = InverseLength(&facing, LengthEpsilon);
        facing.x = facing.x * inverse;
        facing.y = facing.y * inverse;
        facing.z = facing.z * inverse;
        turned = 1.0f - (was.x * facing.x + was.y * facing.y + was.z * facing.z);
        if (facingOnly != 0)
        {
            MatrixFacing(&axes, &facing);
        }
        else
        {
            Vector4 side = Cross(facing, up);
            side.x = -side.x;
            side.y = -side.y;
            side.z = -side.z;
            Vector4 upright = Cross(facing, side);
            Vector4 corner = g_DefaultBox.min;
            corner.w = 1.0f;
            MatrixFromRows(&axes, &side, &upright, &facing, &corner);
        }
    }

    // (GetRotationVec reads the matrix's rotation alone)
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &axes);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    return turned;
}
