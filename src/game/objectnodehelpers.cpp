#include "game/objectnode.h"

#include "game/animation.h"
#include "game/behaviours.h"
#include "game/camerarig.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/instanceparticles.h"
#include "game/layout.h"
#include "game/navigation.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/player.h"
#include "game/reference.h"
#include "game/rigidbody.h"
#include "game/sound.h"

#include <cstddef>
#include <cstdint>

// The object node's helpers: where a script's targets are, throws, turning an instance, notifying the instances around it, its
// water touches and its surfaces' contact sounds and particles

extern "C"
{
    // The statics of the module before (the script conditions' classes from 1 to 176): GCC 2.9x's initialisation function (a
    // word nothing reads made 0, called for every priority) and the module's global constructor
    void InitConditionClassesStatics(u32 initialise, u32 priority) RETAIL(FUN_00229060);
    void ConstructConditionClassesModule() RETAIL(FUN_0022c2c8);
    extern u32 g_ConditionClassesUnused RETAIL(D_0030A938);
    // A surface's impact particles and sound played at a point
    void SpawnSurfaceImpactParticles(InstanceContext* instance, CollisionSurface* surface, const Vector4* point);
    void PlaySurfaceImpactSound(ObjectNode* node, CollisionSurface* surface, const Vector4* point);
    // The kinds of hit a touch of water is (the contact message's word)
    extern u32 g_WaterContactKinds RETAIL(D_0030ACB0);
}

EABI_EXPORT(FUN_0022c870, ThrowOverHeight);
EABI_EXPORT(FUN_0022c9a0, ThrowInTime);
EABI_EXPORT(FUN_0022d0e0, TurnFacingToward);
EABI_EXPORT(FUN_0022e410, SnapInstanceRotationX);
EABI_EXPORT(FUN_0022e598, SnapInstanceRotationY);
EABI_EXPORT(FUN_0022e730, SnapInstanceRotationZ);
EABI_EXPORT(FUN_0022ec40, SpinAlongMove);
EABI_EXPORT(FUN_0022f018, NotifyInstancesWithin);
EABI_EXPORT(PlaySurfaceContact, PlaySurfaceContact);
EABI_EXPORT(FUN_00230a28, PlayContactSound);
EABI_EXPORT(PlaySurfaceContactHard, PlaySurfaceContactHard);

namespace
{
// The surface ID of water
constexpr s32 WaterSurface = 0xC;
// The sounds' group (none), and their repeats: none for a sound at a position, followed with the instance for an instance's
constexpr s32 NoGroup = 0;
constexpr s32 NotLast = -1;
constexpr s32 FollowsInstance = 0;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

bool IsAsleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// An instance's place as the retail code reads it, also when there's no instance (the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

Vector4 Cross(const Vector4& a, const Vector4& b, f32 w)
{
    Vector4 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    result.w = w;
    return result;
}

// A vector moved by another turned by a matrix's axes
void AddTurned(Vector4* vector, const Matrix4x4* matrix, const Vector4* offset)
{
    vector->x = vector->x + (matrix->m[0][0] * offset->x + matrix->m[1][0] * offset->y + matrix->m[2][0] * offset->z);
    vector->y = vector->y + (matrix->m[0][1] * offset->x + matrix->m[1][1] * offset->y + matrix->m[2][1] * offset->z);
    vector->z = vector->z + (matrix->m[0][2] * offset->x + matrix->m[1][2] * offset->y + matrix->m[2][2] * offset->z);
}

// An instance's seen stamp (24 bits)
u32 SeenStamp(const InstanceContext* instance)
{
    return instance->seen;
}

// Seconds of an instance's clock since a time
f32 SecondsSince(InstanceContext* instance, u32 time)
{
    return static_cast<f32>(static_cast<s32>(GetContextClock(instance)->time - time)) * g_SecondsPerClockUnit;
}

// How a node moves: its agent's own velocity when it has one, else its physics body's, else its motion's (the agent's function
// is handed what the stack had)
void NodeVelocity(ObjectNode* node, Vector4* velocity)
{
    Agent* agent = node->agent;
    if (CallVirtual<u32>(agent, agent->vtable, Agent::VelocitySlot, velocity) != 0)
    {
        return;
    }

    ObjectRigidBody* body = node->rigidBody;
    if (body != nullptr && body->physicsBody != nullptr)
    {
        *velocity = body->physicsBody->velocity;
        return;
    }

    CopyVelocity(node, velocity);
}

// A surface's particles of a contact started at a point, facing the way the contact went
void StartContactParticles(ObjectNode* node, u32 system, const Vector4* point, const Vector4* velocity)
{
    s32 emitter = StartEmitterKeepingTranslation(node->owner, system, 1, nullptr);
    Vector4 direction = *velocity;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    Matrix4x4 frame;
    MatrixFacing(&frame, &direction);
    *RowOf(&frame, 3) = *point;
    SetEmitterFrame(emitter, &frame);
}

// An instance's rotation about one of its axes snapped to the nearest multiple of a step (radians). Negative angles snap a step
// too far up from half a step below a multiple on: the truncation goes toward 0
void SnapInstanceRotation(f32 step, InstanceContext* instance, u32 axis)
{
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    s32 angles[3];
    AnglesOfRotation(&place->rotation, &angles[0], &angles[1], &angles[2]);
    f32 steps = (static_cast<f32>(angles[axis]) * AngleToRadians + step * 0.5f) / step;
    s32 snapped;
    AngleFrom(&snapped, step * static_cast<f32>(static_cast<s32>(steps)), AngleRadians);
    place = instance->place;
    place->SyncRotation();
    place->MarkTurned();
    AnglesOfRotation(&place->rotation, &angles[0], &angles[1], &angles[2]);
    angles[axis] = snapped;
    GetRotationXYZ(&place->rotation, &angles[0], &angles[1], &angles[2]);
    QueueObject(instance);
}
}

void InitConditionClassesStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_ConditionClassesUnused = 0;
}

void ConstructConditionClassesModule()
{
    InitConditionClassesStatics(1, DefaultInitPriority);
}

// The surface's impact sound (contact kind 0) at the point in the node's instance's chunk, at the surface's volume scale and
// the sound's own pitch, in the voice kind of where the listener is
void PlaySurfaceImpactSound(ObjectNode* node, CollisionSurface* surface, const Vector4* point)
{
    f32 volume = -1.0f;
    u32 sound = GetSurfaceSound(surface, ContactImpact, &volume);
    if (sound == NoSoundId)
    {
        return;
    }

    s32 voiceKind = ListenerVoiceKind();
    PlaySoundByIdAt(volume, OwnScale, static_cast<u16>(sound), NoGroup, node->owner->chunk, point, voiceKind, NotLast);
}

u32 IsWithinAiPosition(const AiPosition* position, GameNode* node)
{
    // How far above and below the position the middle of the instance's collision box may be
    constexpr f32 Above = Rounded(3.7);
    constexpr f32 Below = Rounded(0.8);
    const Box* box = node->owner->CollisionBox();
    Vector4 middle = box->max;
    middle.x = (middle.x - box->min.x) * 0.5f + box->min.x;
    middle.y = (middle.y - box->min.y) * 0.5f + box->min.y;
    middle.z = (middle.z - box->min.z) * 0.5f + box->min.z;
    const Vector4& at = position->position;
    if (at.y + Above < middle.y || middle.y < at.y - Below)
    {
        return 0;
    }

    f32 x = middle.x - at.x;
    f32 z = middle.z - at.z;
    return x * x + z * z < at.w * at.w ? 1 : 0;
}

void ThrowOverHeight(f32 gravity, f32 height, const Vector4* from, const Vector4* to, Vector4* velocity)
{
    f32 twice = gravity + gravity;
    Vector4 across = *to;
    f32 rise = across.y - from->y;
    across.x = across.x - from->x;
    across.y = 0.0f;
    across.z = across.z - from->z;
    // Up to the height above the start, then down to the target
    f32 up = __builtin_sqrtf(__builtin_fabsf(twice * height));
    velocity->y = up;
    f32 down = __builtin_sqrtf(__builtin_fabsf(twice * (height - rise)));
    f32 distance = __builtin_sqrtf(across.x * across.x + across.z * across.z);
    f32 speed = distance / ((down + up) / gravity);
    f32 inverse = InverseLength(&across, LengthEpsilon);
    velocity->w = 1.0f;
    velocity->x = across.x * inverse * speed;
    velocity->z = across.z * inverse * speed;
}

void ThrowInTime(f32 gravity, f32 seconds, const Vector4* from, const Vector4* to, Vector4* velocity)
{
    // A target closer across than this is straight above or below: the default box's corner across
    constexpr f32 Least = 5e-05f;
    f32 fall = gravity * (seconds * 0.5f) * seconds;
    Vector4 thrown = *to;
    thrown.x = thrown.x - from->x;
    thrown.z = thrown.z - from->z;
    thrown.y = 0.0f;
    f32 distance = __builtin_sqrtf(thrown.x * thrown.x + thrown.z * thrown.z);
    f32 up = (to->y - from->y + fall) / seconds;
    f32 speed = distance / seconds;
    if (__builtin_fabsf(distance) <= Least)
    {
        thrown = g_DefaultBox.min;
        thrown.w = 1.0f;
    }
    else
    {
        f32 scale = speed / distance;
        thrown.z = thrown.z * scale;
        thrown.x = thrown.x * scale;
    }

    thrown.y = up;
    *velocity = thrown;
}

f32 TurnFacingToward(f32 share, f32 upShare, InstanceContext* instance, const Vector4* target)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 here = place->position;
    Vector4 toward = *target;
    toward.x = toward.x - here.x;
    toward.y = toward.y - here.y;
    toward.z = toward.z - here.z;
    f32 inverse = InverseLength(&toward, LengthEpsilon);
    share = share < 1.0f ? share : 1.0f;
    upShare = upShare < 1.0f ? upShare : 1.0f;
    toward.x = toward.x * inverse;
    toward.y = toward.y * inverse;
    toward.z = toward.z * inverse;
    place = instance->place;
    RotateAndTranslate(place);
    Vector4 facing = *RowOf(&place->matrix, 2);
    place = instance->place;
    RotateAndTranslate(place);
    Vector4 up = *RowOf(&place->matrix, 1);
    Vector4 was = facing;
    facing.x = facing.x + (toward.x - facing.x) * share;
    facing.y = facing.y + (toward.y - facing.y) * upShare;
    facing.z = facing.z + (toward.z - facing.z) * share;
    facing.w = 1.0f;
    if (facing.x == 0.0f && facing.z == 0.0f)
    {
        return 0.0f;
    }

    inverse = InverseLength(&facing, LengthEpsilon);
    facing.x = facing.x * inverse;
    facing.y = facing.y * inverse;
    facing.z = facing.z * inverse;
    Vector4 side = Cross(facing, up, 1.0f);
    side.x = -side.x;
    side.y = -side.y;
    side.z = -side.z;
    Vector4 upright = Cross(facing, side, 1.0f);
    f32 turned = 1.0f - (was.x * facing.x + was.y * facing.y + was.z * facing.z);
    Vector4 corner = {0.0f, 0.0f, 0.0f, 1.0f};
    Matrix4x4 axes;
    MatrixFromRows(&axes, &side, &upright, &facing, &corner);
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

void DesignatedPosition(Vector4* position, u32 space, BehaviourRunner* runner, const Vector4* offset, u32 designator,
                        u32 receiver, u32)
{
    designator &= 0xFF;
    receiver &= 0xFF;
    Vector4 result = {0.0f, 0.0f, 0.0f, 1.0f};
    ObjectNode* node = NodeOf(runner);
    InstancePlacement* start = node->informationPointer;
    InstanceContext* target = nullptr;
    // The place the position is an instance's (read through no instance as retail does), or the position itself
    ObjectPlace* place = nullptr;
    bool fromPlace = false;
    bool found = false;
    if (receiver != DesignatesNone)
    {
        if (receiver == DesignatesFocus)
        {
            target = node->AwakeFocus();
            fromPlace = target != nullptr;
        }
        else
        {
            target = runner->receivers->instances[receiver];
            if (target == nullptr)
            {
                // The position is left as it was
                return;
            }

            fromPlace = true;
        }

        if (fromPlace)
        {
            place = target->place;
        }
    }
    else if (designator != DesignatesNone)
    {
        Waypoints* waypoints = node->waypoints;
        LayoutPosition* key;
        switch (designator)
        {
        case DesignatesPlayer:
            target = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
            place = RetailPlaceOf(target);
            fromPlace = true;
            break;
        case DesignatesOriginator:
            target = static_cast<InstanceContext*>(runner->originator);
            place = RetailPlaceOf(target);
            fromPlace = true;
            break;
        case DesignatesStoredPosition:
            if (node->flags.storedPosition)
            {
                result = node->storedPosition;
                found = true;
            }

            break;
        case DesignatesAgentRef2:
            if (node->agentRef2 != nullptr && IsAsleep(node->agentRef2) && !node->flags.keepsAgentRef2)
            {
                node->agentRef2 = nullptr;
            }

            target = node->agentRef2;
            place = RetailPlaceOf(target);
            fromPlace = true;
            break;
        case DesignatesAgentRef1:
            if (node->agentRef1 != nullptr && IsAsleep(node->agentRef1))
            {
                node->agentRef1 = nullptr;
            }

            target = node->agentRef1;
            place = RetailPlaceOf(target);
            fromPlace = true;
            break;
        case DesignatesPreviousStep:
            break;
        case DesignatesCurrentStep:
            if (waypoints->route != nullptr)
            {
                AiPosition* step = waypoints->route->PositionAt(waypoints->routeIndex);
                if (step != nullptr)
                {
                    result = step->position;
                    result.w = 1.0f;
                    found = true;
                }
            }

            break;
        case DesignatesFocus:
            target = node->AwakeFocus();
            if (target != nullptr)
            {
                place = target->place;
                fromPlace = true;
            }

            break;
        case DesignatesFocusPosition:
            if (node->flags.focusPosition)
            {
                result.x = node->focusPosition.x;
                result.y = node->focusPosition.y;
                result.z = node->focusPosition.z;
                result.w = node->focusPosition.w;
                found = true;
            }

            break;
        case DesignatesNextKey:
        case DesignatesCurrentKey:
            // The keys of the node it stands in for when there's one
            if (node->sourceNode != nullptr)
            {
                waypoints = static_cast<ObjectNode*>(node->sourceNode)->waypoints;
            }

            if (designator == DesignatesNextKey)
            {
                waypoints->NextKey();
            }

            key = waypoints->positions.data[waypoints->key];
            result = key->position;
            result.w = 1.0f;
            found = true;
            break;
        default:
            // A key by its index
            if (node->sourceNode != nullptr)
            {
                waypoints = static_cast<ObjectNode*>(node->sourceNode)->waypoints;
            }

            key = waypoints->keyCount != 0 && static_cast<s32>(designator) < static_cast<s32>(waypoints->keyCount)
                      ? waypoints->positions.data[designator]
                      : nullptr;
            if (key != nullptr)
            {
                result = key->position;
                result.w = 1.0f;
                found = true;
            }

            break;
        }
    }

    if (fromPlace)
    {
        place->SyncPosition();
        result = place->position;
        found = true;
    }

    if (!found)
    {
        // The space's own position
        ObjectPlace* spacePlace = nullptr;
        switch (space)
        {
        case SpaceStart:
            result = start->position;
            break;
        case SpaceOwn:
            spacePlace = node->owner->place;
            fromPlace = true;
            break;
        case SpaceTracked:
            if (node->tracked == nullptr)
            {
                node->tracked = target;
            }

            spacePlace = RetailPlaceOf(node->tracked);
            fromPlace = true;
            break;
        case SpaceRigidBody:
            if (node->rigidBody != nullptr && node->rigidBody->object != nullptr)
            {
                spacePlace = node->rigidBody->object->place;
                fromPlace = true;
            }

            break;
        case SpaceStored:
            spacePlace = node->storedPlace;
            fromPlace = spacePlace != nullptr;
            break;
        default:
            break;
        }

        if (fromPlace)
        {
            spacePlace->SyncPosition();
            result = spacePlace->position;
        }
    }

    if (offset != nullptr)
    {
        // The offset turned by the space
        ObjectPlace* turnedBy = nullptr;
        bool turns = false;
        Matrix4x4 turn;
        switch (space)
        {
        case SpaceWorld:
            result.x = result.x + offset->x;
            result.y = result.y + offset->y;
            result.z = result.z + offset->z;
            break;
        case SpaceStart:
            MatrixFromRotation(&turn, &start->rotation);
            AddTurned(&result, &turn, offset);
            break;
        case SpaceOwn:
            turnedBy = node->owner->place;
            turns = true;
            break;
        case SpaceTarget:
            if (target != nullptr)
            {
                turnedBy = target->place;
                turns = true;
            }

            break;
        case SpaceTracked:
            if (node->tracked == nullptr)
            {
                node->tracked = target;
            }

            MatrixFacingFrom(node->owner, node->tracked, &turn);
            AddTurned(&result, &turn, offset);
            break;
        case SpaceRigidBody:
            if (node->rigidBody != nullptr && node->rigidBody->object != nullptr)
            {
                turnedBy = node->rigidBody->object->place;
                turns = turnedBy != nullptr;
            }

            break;
        case SpaceStored:
            turnedBy = node->storedPlace;
            turns = turnedBy != nullptr;
            break;
        default:
            break;
        }

        if (turns)
        {
            RotateAndTranslate(turnedBy);
            AddTurned(&result, &turnedBy->matrix, offset);
        }
    }

    *position = result;
}

ObjectPlace* SpaceOfRequest(BehaviourRunner* runner, u32, u32 target, u32 receiver, u32)
{
    target &= 0xFF;
    receiver &= 0xFF;
    if (target == DesignatesNone)
    {
        if (receiver == DesignatesNone)
        {
            return runner->agentNode->owner->place;
        }

        return RetailPlaceOf(runner->receivers->instances[receiver]);
    }

    if (target != DesignatesFocus)
    {
        return nullptr;
    }

    InstanceContext* focus = NodeOf(runner)->AwakeFocus();
    return focus != nullptr ? focus->place : nullptr;
}

u32 JointPosition(InstanceContext* instance, u32 joint, Vector4* position, const Vector4* offset)
{
    constexpr u32 Slots = 0x3F;
    joint &= 0xFF;
    if (joint >= Slots)
    {
        return 0;
    }

    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 0;
    }

    ExitPointAnimation* exitPoint = animator->exitPoints != nullptr ? animator->exitPoints->data[joint] : nullptr;
    if (exitPoint == nullptr)
    {
        return 0;
    }

    const Matrix4x4* matrix = &UpdateExitPointMatrix(exitPoint)->matrix;
    Vector4 at = *RowOf(matrix, 3);
    position->x = at.x;
    position->w = at.w;
    position->y = at.y;
    position->z = at.z;
    if (offset != nullptr)
    {
        position->z = at.z + (matrix->m[0][2] * offset->x + matrix->m[1][2] * offset->y + matrix->m[2][2] * offset->z);
        position->x = at.x + (matrix->m[0][0] * offset->x + matrix->m[1][0] * offset->y + matrix->m[2][0] * offset->z);
        position->y = at.y + (matrix->m[0][1] * offset->x + matrix->m[1][1] * offset->y + matrix->m[2][1] * offset->z);
    }

    return 1;
}

void MatrixFacingFrom(InstanceContext* from, InstanceContext* to, Matrix4x4* matrix)
{
    ObjectPlace* place = to->place;
    place->SyncPosition();
    Vector4 direction = place->position;
    place = from->place;
    place->SyncPosition();
    Vector4 here = place->position;
    direction.x = direction.x - here.x;
    direction.y = direction.y - here.y;
    direction.z = direction.z - here.z;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    MatrixFacing(matrix, &direction);
}

void SnapInstanceRotationX(f32 step, InstanceContext* instance)
{
    SnapInstanceRotation(step, instance, 0);
}

void SnapInstanceRotationY(f32 step, InstanceContext* instance)
{
    SnapInstanceRotation(step, instance, 1);
}

void SnapInstanceRotationZ(f32 step, InstanceContext* instance)
{
    SnapInstanceRotation(step, instance, 2);
}

void SpinAlongMove(f32, f32 degreesPerUnit, ObjectNode* node, const Vector4* moved, u32 bySize)
{
    s32 angle;
    AngleFrom(&angle, degreesPerUnit, AngleDegrees);
    InstanceContext* instance = node->owner;
    u32 axis;
    f32 amount;
    if (moved->x != 0.0f)
    {
        axis = 0;
        amount = moved->x;
    }
    else if (moved->y != 0.0f)
    {
        axis = 1;
        amount = moved->y;
    }
    else if (moved->z != 0.0f)
    {
        axis = 2;
        amount = moved->z;
    }
    else
    {
        return;
    }

    if (bySize != 0)
    {
        amount = __builtin_fabsf(amount);
    }

    s32 turn = static_cast<s32>(static_cast<f32>(angle) * amount);
    if (turn == 0)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    place->SyncRotation();
    place->MarkTurned();
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
    QueueObject(instance);
}

void NotifyInstancesWithin(f32 radius, ObjectNode* node)
{
    constexpr u16 MostFound = 64;
    // Without a radius, the instance's collision box grown by this
    constexpr f32 Margin = 0.5f;
    Box box = *node->owner->CollisionBox();
    if (0.0f < radius)
    {
        Vector4 middle = box.min;
        middle.x = (middle.x + box.max.x) * 0.5f;
        middle.y = (middle.y + box.max.y) * 0.5f;
        middle.z = (middle.z + box.max.z) * 0.5f;
        box.min = middle;
        box.max = middle;
        box.min.x = middle.x - radius;
        box.max.x = middle.x + radius;
        box.min.y = middle.y - radius;
        box.min.z = middle.z - radius;
        box.max.y = middle.y + radius;
        box.max.z = middle.z + radius;
    }
    else
    {
        GrowBox(Margin, &box);
    }

    void* results[MostFound];
    InstanceQuery query;
    query.results = results;
    query.most = MostFound;
    query.count = 0;
    query.distance = Infinite;
    query.bits.value = InstanceQueryBits::AllWanted;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = ReferencedObjectFlags::PhysicsBody | ReferencedObjectFlags::CollisionActive;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, node->owner);
    u32 found = QueryChunkInstances(node->owner->chunk, &box, g_SolidKinds, &query);
    for (u16 index = 0; index < found; index++)
    {
        auto* other = static_cast<InstanceContext*>(results[index]);
        auto* otherNode = static_cast<GameNode*>(GetGameNode(&other->nodes, NodeObject));
        CallVirtual<void>(otherNode, otherNode->vtable, ObjectNode::PushSlot, 0.0f, node->owner);
    }
}

void ObjectNode::TouchedWater(const CollisionHit* hit, const Vector4* position)
{
    CollisionSurface* touched = GetTriangleSurface(hit);
    if (touched->surfaceId != WaterSurface)
    {
        return;
    }

    // Where it touched, on the water's plane
    waterPoint = *position;
    Vector4 plane;
    PlaneThroughTriangle(&plane, &hit->vertices[0], &hit->vertices[1], &hit->vertices[2]);
    Vector4 normal;
    normal.x = plane.x;
    normal.y = plane.y;
    normal.z = plane.z;
    normal.w = 1.0f;
    ProjectOntoPlaneInPlace(&plane, &waterPoint);
    if (waterSurface != WaterSurface)
    {
        EnteredWater(this, touched, &normal);
    }
    else
    {
        MovedInWater(this, touched, &normal);
    }

    waterSurface = WaterSurface;
    if (rigidBody == nullptr || !rigidBody->state.tellsWaterTouches)
    {
        return;
    }

    ContactMessage message;
    message.point = waterPoint;
    message.hitKinds = g_WaterContactKinds;
    message.damage = 1;
    message.point.w = 0.0f;
    CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &message, owner, 1u);
}

void EnteredWater(ObjectNode* node, CollisionSurface* surface, const Vector4*)
{
    // The splash is ahead of the point by this share of the velocity across, at most every half second
    constexpr f32 Lead = 0.12f;
    constexpr f32 SplashInterval = 0.5f;
    // A body falling in while it spins faster than this skims off the water: the faster it spins and the flatter it comes in, the
    // more it bounces (none when it comes in steeper than 1 in 3.5, 1 in 2.5 for spheres)
    constexpr f32 SkimSpin = 9.0f;
    constexpr f32 SkimSpinBounce = 0.01f;
    constexpr f32 SkimBounce = Rounded(0.05);
    constexpr f32 SkimFlatBounce = 3.0f;
    constexpr f32 SteepestSkim = 3.5f;
    constexpr f32 SteepestSphereSkim = 2.5f;
    constexpr f32 MostBounce = Rounded(0.87);
    Vector4 velocity;
    NodeVelocity(node, &velocity);
    Vector4 splash = velocity;
    splash.x = velocity.x * Lead + node->waterPoint.x;
    splash.z = velocity.z * Lead + node->waterPoint.z;
    splash.y = node->waterPoint.y;
    if (SplashInterval < SecondsSince(node->owner, node->reactions.splashTime))
    {
        SpawnSurfaceImpactParticles(node->owner, surface, &splash);
        PlaySurfaceImpactSound(node, surface, &splash);
        node->reactions.splashTime = GetContextClock(node->owner)->time;
    }

    ObjectRigidBody* body = node->rigidBody;
    f32 fall = velocity.y;
    if (body == nullptr || !(fall < 0.0f))
    {
        return;
    }

    DynamicBody* physicsBody = body->physicsBody;
    if (physicsBody == nullptr)
    {
        return;
    }

    f32 spin = __builtin_fabsf(physicsBody->angularVelocity.y) - SkimSpin;
    if (!(0.0f < spin))
    {
        return;
    }

    // Lifted to the water's level
    Vector4 level = splash;
    level.y = (splash.y - physicsBody->matrix.m[3][1]) + physicsBody->matrix.m[3][1];
    level.x = physicsBody->matrix.m[3][0];
    level.z = physicsBody->matrix.m[3][2];
    physicsBody->SetPosition(&level);
    f32 across = __builtin_sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
    f32 bounce = spin * SkimSpinBounce + SkimBounce;
    f32 flatness = across / -fall;
    f32 steepest = CallVirtual<u32>(physicsBody, physicsBody->vtable, DynamicBody::IsSphereSlot) != 0 ? SteepestSphereSkim
                                                                                                   : SteepestSkim;
    if (flatness < steepest)
    {
        bounce = 0.0f;
    }
    else
    {
        bounce = bounce + flatness * SkimFlatBounce;
    }

    if (MostBounce < bounce)
    {
        bounce = MostBounce;
    }

    velocity.y = fall * -bounce;
    physicsBody->SetVelocity(&velocity);
}

void MovedInWater(ObjectNode* node, CollisionSurface* surface, const Vector4* normal)
{
    // The wake's chance of a decal grows with the squared speed (at most a half), it's ahead of the point by this share of the
    // velocity across; faster than 2 it plays the surface's sound at most every half second, and particles when it also goes
    // into the water faster than 36
    constexpr f32 ChancePerSpeed = Rounded(0.001);
    constexpr f32 LeastChance = 0.01f;
    constexpr f32 MostChance = 0.5f;
    constexpr f32 Lead = Rounded(0.14);
    constexpr f32 SoundSpeed = 4.0f;
    constexpr f32 SoundInterval = 0.5f;
    constexpr f32 SplashSpeed = 36.0f;
    Vector4 velocity;
    NodeVelocity(node, &velocity);
    f32 speed = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    f32 chance = speed * ChancePerSpeed + LeastChance;
    if (MostChance < chance)
    {
        chance = MostChance;
    }

    Vector4 wake = velocity;
    wake.x = velocity.x * Lead + node->waterPoint.x;
    wake.z = velocity.z * Lead + node->waterPoint.z;
    wake.y = node->waterPoint.y;
    // (The surface goes as the decal's unused argument)
    ScatterDecal(node->owner, static_cast<u32>(reinterpret_cast<std::uintptr_t>(surface)), &wake, chance);
    Agent* agent = node->agent;
    if (CallVirtual<u32>(agent, agent->vtable, Agent::IsCharacterSlot) != 0)
    {
        return;
    }

    f32 elapsed = SecondsSince(node->owner, node->reactions.splashTime);
    if (!(SoundSpeed < speed) || !(SoundInterval < elapsed))
    {
        return;
    }

    PlaySurfaceImpactSound(node, surface, &wake);
    node->reactions.splashTime = GetContextClock(node->owner)->time;
    if (SplashSpeed < velocity.x * normal->x + velocity.y * normal->y + velocity.z * normal->z)
    {
        SpawnSurfaceImpactParticles(node->owner, surface, &wake);
    }
}

void PlaySurfaceContact(f32 intensity, ObjectNode* node, CollisionSurface* surface, u32 kind, const Vector4* point,
                        const Vector4* velocity)
{
    // At most a contact every half second; its sound as loud as the intensity (and the surface's scale for the kind), its pitch
    // higher, the camera shaken by the speed (at most 0.3); past an intensity of 50 on a surface solid to the player what the
    // agent rides and the node's contact sound play too
    constexpr f32 Interval = 0.5f;
    constexpr f32 VolumePerStrength = 0.06f;
    constexpr f32 LeastVolume = Rounded(0.2);
    constexpr f32 MostVolume = 2.5f;
    constexpr f32 PitchPerStrength = Rounded(0.3);
    constexpr f32 LeastPitch = Rounded(0.2);
    constexpr f32 BasePitch = Rounded(0.4);
    constexpr f32 MostPitch = 2.0f;
    constexpr f32 ShakePerSpeed = Rounded(0.001);
    constexpr f32 MostShake = Rounded(0.3);
    constexpr f32 ShakeSpread = 0.5f;
    constexpr f32 ShakeFalloff = 0.02f;
    constexpr f32 RideIntensity = 50.0f;
    if (SecondsSince(node->owner, node->lastContactTime) < Interval)
    {
        return;
    }

    kind &= 0xFF;
    node->lastContactTime = GetContextClock(node->owner)->time;
    f32 volumeScale;
    u32 sound = GetSurfaceSound(surface, kind, &volumeScale);
    u32 particles = GetSurfaceParticle(surface, kind);
    u32 seen = SeenStamp(node->owner);
    s32 voiceKind = ListenerVoiceKind();
    if (seen < InstanceContext::SoundSeenLimit)
    {
        if (sound != NoSoundId && !node->reactions.noContactSounds)
        {
            f32 strength = __builtin_sqrtf(intensity);
            f32 volume = strength * VolumePerStrength + LeastVolume;
            if (0.0f < volumeScale)
            {
                volume = volume * volumeScale;
            }

            if (MostVolume < volume)
            {
                volume = MostVolume;
            }

            f32 pitch = strength * PitchPerStrength + BasePitch;
            if (MostPitch < pitch)
            {
                pitch = MostPitch;
            }
            else if (pitch < LeastPitch)
            {
                pitch = LeastPitch;
            }

            PlaySoundByIdAt(volume, pitch, sound, NoGroup, node->owner->chunk, point, voiceKind, NotLast);
            f32 shake = __builtin_sqrtf(velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z)
                        * ShakePerSpeed;
            if (MostShake < shake)
            {
                shake = MostShake;
            }

            f32 across = shake * RandomSignedTimes(ShakeSpread);
            f32 along = shake * RandomSignedTimes(ShakeSpread);
            PushCameraShakeAxes(&g_CameraShake, point, across, shake, along, ShakeFalloff);
        }

        if (RideIntensity < intensity && surface->flags.solidToPlayer != 0)
        {
            Agent* agent = node->agent;
            CallVirtual<void>(agent, agent->vtable, Agent::PlayRideSoundSlot, intensity);
            if (node->contactSoundFirst != ObjectNode::NoContactSoundSlot)
            {
                PlayContactSound(intensity, node);
            }
        }
    }

    if (particles != NoSurfaceEffect)
    {
        StartContactParticles(node, particles, point, velocity);
    }
}

void PlayContactSound(f32 strength, ObjectNode* node)
{
    constexpr f32 VolumePerStrength = 0.06f;
    constexpr f32 LeastVolume = Rounded(0.2);
    constexpr f32 PitchSpread = Rounded(0.07);
    u32 sound = NoSoundId;
    if (node->contactSoundValue < 0.0f)
    {
        u8 first = node->contactSoundFirst;
        u8 last = node->contactSoundLast;
        sound = SoundOfSlot(node, RandomFrom(first, last - first + 1) & 0xFFFF);
    }

    if (sound == NoSoundId)
    {
        return;
    }

    f32 volume = __builtin_sqrtf(strength) * VolumePerStrength + LeastVolume;
    s32 voiceKind = ListenerVoiceKind();
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    f32 pitch = RandomSignedTimes(PitchSpread) + 1.0f;
    PlaySoundByIdAt(volume, pitch, sound, NoGroup, node->owner->chunk, &position, voiceKind, NotLast);
}

void PlaySurfaceContactHard(f32 intensity, ObjectNode* node, CollisionSurface* surface, u32 kind, const Vector4* point,
                            const Vector4* velocity)
{
    // A scrape's pitch falls with the node's roll radius; the sound keeps playing in the node's slot for it, its pitch and
    // volume following the contacts
    constexpr f32 ScrapePitchPerStrength = 0.055f;
    constexpr f32 ScrapePitchPerRadius = Rounded(0.1);
    constexpr f32 ScrapeVolumePerStrength = Rounded(0.05);
    constexpr f32 ScrapeBaseVolume = 0.75f;
    constexpr f32 ScrapeMostPitch = 2.8f;
    constexpr f32 PitchPerStrength = 0.06f;
    constexpr f32 BasePitch = Rounded(0.2);
    constexpr f32 VolumePerStrength = Rounded(0.4);
    constexpr f32 BaseVolume = Rounded(0.3);
    constexpr f32 MostPitch = 2.0f;
    constexpr f32 MostVolume = 1.5f;
    f32 volumeScale;
    u32 sound = GetSurfaceSound(surface, kind & 0xFF, &volumeScale);
    u32 particles = GetSurfaceParticle(surface, kind & 0xFF);
    if (sound != NoSoundId && !node->reactions.noContactSounds && SeenStamp(node->owner) < InstanceContext::SoundSeenLimit)
    {
        f32 strength = __builtin_sqrtf(intensity);
        f32 pitch;
        f32 volume;
        if (kind == ContactScrape)
        {
            pitch = strength * ScrapePitchPerStrength + 1.0f - node->rollRadius * ScrapePitchPerRadius;
            volume = strength * ScrapeVolumePerStrength + ScrapeBaseVolume;
            if (ScrapeMostPitch < pitch)
            {
                pitch = ScrapeMostPitch;
            }
        }
        else
        {
            pitch = strength * PitchPerStrength + BasePitch;
            volume = strength * VolumePerStrength + BaseVolume;
            if (MostPitch < pitch)
            {
                pitch = MostPitch;
            }
        }

        if (0.0f < volumeScale)
        {
            volume = volume * volumeScale;
        }

        if (MostVolume < volume)
        {
            volume = MostVolume;
        }

        if (node->playingSound == NoInstanceSound)
        {
            s32 voiceKind = ListenerVoiceKind();
            node->playingSound = PlayInstanceSoundById(volume, pitch, sound, NoGroup, node->owner, voiceKind, FollowsInstance);
        }
        else
        {
            SetInstanceSoundPitch(pitch, node->playingSound);
            SetInstanceSoundVolume(volume, node->playingSound);
        }
    }

    if (particles != NoSurfaceEffect)
    {
        StartContactParticles(node, particles, point, velocity);
    }
}
