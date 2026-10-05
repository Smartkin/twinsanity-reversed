#include "game/objectnode.h"

#include "game/clock.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/objectcollision.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"

// An object node's rigid body over a frame (the bodies its node's motion moves, motion kinds below 9): stepped by its node's
// velocity, pushed out of the instances around it (physics bodies' spheres and hulls, other rigid bodies, or only pushed away
// from them), slid along what it touched, carried by what it rides, collided with its collision cache's triangles, and righted

extern "C"
{
    // A body's sphere against a physics body's sphere, its instance against another instance's hulls, its velocity's impulse
    // shared with a physics body it touched, and the agents of two instances told they collided: whether they touched
    u32 CollideWithSphereBody(ObjectRigidBody* body, const Vector4* sphere, SphereBody* other) RETAIL(FUN_00247ba0);
    u32 CollideWithHulls(ObjectRigidBody* body, const Vector4* center, const Box* box, InstanceContext* other,
                         ObjectCollision* collision) RETAIL(FUN_00248bb8);
    void PushPhysicsBody(ObjectRigidBody* body, DynamicBody* other) RETAIL(FUN_00248270);
    void BumpInstance(ObjectRigidBody* body, InstanceContext* other) RETAIL(FUN_002485a0);
    // The forces of the volume controllers its instance is in, applied at its instance's position
    void ApplyVolumeForces(ObjectRigidBody* body) RETAIL(FUN_002480c0);
    // Its sphere pushed away from the instances a query found (the push added up, the other way round), and its instance moved
    // by it
    void RepelFrom(ObjectRigidBody* body, Vector4* sphere, InstanceQuery* query, Vector4* push) RETAIL(FUN_00248838);
    void RepelFromInstances(ObjectRigidBody* body, Vector4* sphere, InstanceQuery* query) RETAIL(FUN_00248a68);
    // Carried by an instance's movement node: where it is in that instance's space kept when it starts riding it, else its
    // instance moved and turned (about y) as the instance did over its last frame
    void RideMovement(ObjectRigidBody* body, MovementNode* movement) RETAIL(FUN_002490a8);
    // Its sphere against its collision cache's triangles: slid out of them by its friction (collision kind 6), pushed out of
    // them as a sphere and as an upright ellipsoid (kind 5): whether it touched one
    u32 SlideAgainstWorld(ObjectRigidBody* body, const Vector4* sphere) RETAIL(FUN_00249618);
    u32 CollideSphereWithWorld(ObjectRigidBody* body, const Vector4* sphere) RETAIL(FUN_0024a7b8);
    u32 CollideUprightWithWorld(ObjectRigidBody* body, const Vector4* sphere) RETAIL(FUN_0024ac68);
    // Two rigid bodies' upright cylinders (either one's motion kind 5) pushed apart: whether they touched
    u32 CollideUprightBodies(ObjectRigidBody* body, const Vector4* sphere, const Vector4* otherSphere, ObjectRigidBody* other)
        RETAIL(FUN_0024a078);
    // What a hull's push out of another instance tells it (whether it counts), and its velocity slid along a normal (whether
    // it stands on it): the ground and wall bits, the contact normal and the grip
    u32 TouchHull(ObjectRigidBody* body, InstanceContext* other, Vector4* push, Vector4* velocity) RETAIL(FUN_0024b168);
    u32 SlideAlong(f32 elapsed, ObjectRigidBody* body, Vector4* normal, Vector4* velocity) RETAIL_N32(FUN_0024b408);

    // An instance's object node pushed by the body's instance (strength 0), the body's ride let go of (its node's FlagRiding
    // cleared), and where it is in the space of the instance it rides
    void PushInstanceNode(ObjectRigidBody* body, InstanceContext* other) RETAIL(FUN_00254730);
    void StopRiding(ObjectRigidBody* body) RETAIL(FUN_00254788);
    void KeepRidePosition(ObjectRigidBody* body, const Vector4* position) RETAIL(FUN_002547a8);
    // The body against another rigid body (its sphere its node's middle and roll radius)
    u32 CollideWithRigidBody(ObjectRigidBody* body, const Vector4* sphere, ObjectRigidBody* other) RETAIL(FUN_00254830);
    // A vector no longer than a length, or than the shorter of two (the body is left alone)
    void ClampLength(f32 most, ObjectRigidBody* body, Vector4* vector) RETAIL_N32(FUN_002548d8);
    void ClampLengthToShorter(f32 first, f32 second, ObjectRigidBody* body, Vector4* vector) RETAIL_N32(FUN_00254868);
    // What a move or a push of the body does while its instance is attached: nothing (the retail code calls it, the C++ leaves it
    // out)
    void IgnoreAttachedMove(ObjectRigidBody* body, const Vector4* move) RETAIL(FUN_002549b0);
    // A force at a point over the time since the body's node's last update (game/commandsphysics.cpp)
    void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point) RETAIL(FUN_0024c508);
}

EABI_EXPORT(FUN_0024b408, SlideAlong);
EABI_EXPORT(FUN_0024b808, RightRigidBody);
EABI_EXPORT(FUN_002548d8, ClampLength);
EABI_EXPORT(FUN_00254868, ClampLengthToShorter);

namespace
{

constexpr u16 MostTouched = 64;
constexpr u16 MostVolumes = 32;
// The share of a push out of a triangle or a hull the instance moves by
constexpr f32 PushShare = Rounded(0.99);
// The impulses (squared) past which the agents are told of a collision, with a physics body's sphere
constexpr f32 ToldImpulse = Rounded(0.001);
constexpr f32 ToldSphereImpulse = Rounded(0.01);
// A push's knock: the push along the velocity times this, doubled
constexpr f32 PushKnock = Rounded(0.3);
// A slide's knock: the slope times this
constexpr f32 SlopeKnock = Rounded(0.3);
// What it grips with once it touches something
constexpr f32 TouchGrip = Rounded(0.8);
// A normal's y above which it's ground, and below which (either way) a wall
constexpr f32 GroundNormalY = Rounded(0.7);
constexpr f32 WallNormalY = Rounded(0.3);
// Below any rise out of a triangle
constexpr f32 NoRise = -10000.0f;
// The rates its contact normal follows a normal at (while it steers itself, while it follows the surface) and turns upright at
// (or its restitution times the share) once it has touched nothing for the time
constexpr f32 SteeringNormalRate = 6.0f;
constexpr f32 SurfaceNormalRate = 5.0f;
constexpr f32 UprightRate = 21.0f;
constexpr f32 UprightRateShare = 1.5f;
constexpr f32 UprightAfter = Rounded(0.2);

u32 MotionKind(const ObjectRigidBody* body)
{
    return body->bits.motionKind;
}

u32 CollisionKind(const ObjectRigidBody* body)
{
    return body->bits.collisionKind;
}

// The node's first float property, through its agent
f32 BodyMass(const ObjectRigidBody* body)
{
    return body->node->agent->properties->GetFloat(0);
}

f32 FrameSeconds(InstanceContext* instance)
{
    return static_cast<f32>(static_cast<s32>(GetContextClock(instance)->advance)) * g_SecondsPerClockUnit;
}

void MoveInstance(InstanceContext* instance, const Vector4* move)
{
    if (instance->place->MoveBy(move))
    {
        QueueObject(instance);
    }
}

void Normalize(Vector4* vector)
{
    f32 inverse = InverseLength(vector, LengthEpsilon);
    vector->x = vector->x * inverse;
    vector->y = vector->y * inverse;
    vector->z = vector->z * inverse;
}

// A share of the way to a target (its w made 1)
void Ease(Vector4* vector, const Vector4* target, f32 share)
{
    vector->x = vector->x + (target->x - vector->x) * share;
    vector->y = vector->y + (target->y - vector->y) * share;
    vector->w = 1.0f;
    vector->z = vector->z + (target->z - vector->z) * share;
}

void RaiseGrip(ObjectRigidBody* body, f32 grip)
{
    if (body->grip < grip)
    {
        body->grip = grip;
    }
}

// An impulse the body gives: cut down to its impulse length, made that long, or scaled by it
void ScaleImpulse(const ObjectRigidBody* body, Vector4* impulse)
{
    f32 size = body->impulseLength;
    if (body->state.impulseCapped)
    {
        f32 length = __builtin_sqrtf(impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z);
        if (!(size < length))
        {
            return;
        }
    }
    else if (!body->state.impulseFixed)
    {
        impulse->x = impulse->x * size;
        impulse->y = impulse->y * size;
        impulse->z = impulse->z * size;
        return;
    }

    f32 inverse = InverseLength(impulse, LengthEpsilon);
    size = body->impulseLength;
    impulse->x = impulse->x * inverse * size;
    impulse->y = impulse->y * inverse * size;
    impulse->z = impulse->z * inverse * size;
}

void StartQuery(InstanceQuery* query, void** results, u16 most, u32 wanted)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = wanted;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// A contact with an instance counted (its normal, if any, is added up by TouchHull)
void CountContact(ObjectRigidBody* body)
{
    body->contactCount = body->contactCount + 1.0f;
    body->bits.touchingInstance = 1;
    body->bits.touching = 1;
}

// What touching an instance's hulls or rigid body tells: its object node pushed, its physics body sharing the impulse (else the
// two agents told unless the body rides something), and its own node pushing back
void TellTouched(ObjectRigidBody* body, InstanceContext* instance, InstanceContext* other)
{
    if (other->flags.physicsBody)
    {
        PushInstanceNode(body, other);
    }

    auto* physics = static_cast<DynamicBody*>(GetGameNode(&other->nodes, NodeRigidBody));
    if (physics != nullptr)
    {
        PushPhysicsBody(body, physics);
    }
    else if (body->object == nullptr && body->rideMovement == nullptr)
    {
        BumpInstance(body, other);
    }

    if (instance->flags.physicsBody)
    {
        ObjectNode* node = body->node;
        CallVirtual<void>(node, node->vtable, ObjectNode::PushSlot, 0.0f, other);
    }
}

// The surfaces of both instances' first hulls: each one sending a contact message tells the other's agent
void TellSurfaces(ObjectRigidBody* body, InstanceContext* instance, ObjectRigidBody* other, InstanceContext* otherInstance)
{
    CollisionSurface* surface = GetHullSurface(&instance->collision, 0);
    CollisionSurface* otherSurface = GetHullSurface(&otherInstance->collision, 0);
    if (surface != nullptr && surface->flags.sendsContactMessageToObjects != 0)
    {
        Agent* agent = other->node->agent;
        CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &surface->contact, instance, 0u);
    }

    if (otherSurface != nullptr && otherSurface->flags.sendsContactMessageToObjects != 0)
    {
        Agent* agent = body->node->agent;
        CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &otherSurface->contact, otherInstance, 0u);
    }
}

// Two bodies pushed apart knock the first one's node by how much the push goes along its velocity and across the ground
void KnockByPush(ObjectRigidBody* body, const Vector4* push)
{
    const Vector4* velocity = &body->node->motion->velocity;
    f32 knock = (push->x * velocity->x + push->y * velocity->y + push->z * velocity->z) * PushKnock;
    knock = knock + knock;
    KnockNode(knock * (push->x * push->x + push->z * push->z), body->node);
}

// The triangles' outcome: the surface it was pushed up from most kept by its node (its contact message told), its instance
// moved by most of the way it was pushed and its velocity slid along that, or its time without touching counted while it
// steers itself; its node's water forgotten when it touched none
u32 FinishWorldContacts(ObjectRigidBody* body, u32 touched, u32 inWater, u16 surfaceId, const Vector4* start, Vector4* center,
                        Vector4* velocity)
{
    InstanceContext* instance = body->node->owner;
    f32 seconds = FrameSeconds(instance);
    if (touched != 0)
    {
        body->node->surface = surfaceId;
        if (surfaceId != NoSurfaceId)
        {
            CollisionSurface* surface = &g_CollisionSurfaces.surfaces[surfaceId];
            if (surface->contact.hitKinds != 0)
            {
                body->state.touchedMessageSurface = 1;
                Agent* agent = body->node->agent;
                CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &surface->contact, instance, 0u);
            }
        }

        center->x = (center->x - start->x) * PushShare;
        center->y = (center->y - start->y) * PushShare;
        center->z = (center->z - start->z) * PushShare;
        MoveInstance(instance, center);
        // (Retail hands the move to IgnoreAttachedMove here while the instance is attached)
        SlideAlong(seconds, body, center, velocity);
    }
    else if (body->bits.steersItself)
    {
        body->untouchedTime = body->untouchedTime + seconds;
    }

    if (inWater == 0)
    {
        body->node->waterSurface = ObjectNode::NoSurface;
    }

    return touched;
}
} // namespace

void CollideRigidBodyWithInstances(ObjectRigidBody* body, Vector4* sphere)
{
    InstanceContext* instance = body->node->owner;
    ChunkData* chunk = instance->chunk;
    if (chunk == nullptr)
    {
        return;
    }

    f32 sumW = body->contactSum.w;
    body->contactSum = g_DefaultBox.min;
    body->contactSum.w = sumW;
    body->contactCount = 0.0f;
    MovementNode* rideMovement = body->rideMovement;
    body->rideMovement = nullptr;
    Vector4 center = *sphere;
    f32 radius = sphere->w;
    center.w = 1.0f;
    Box box;
    box.min = center;
    box.max = center;
    box.min.x = box.min.x - radius;
    box.min.y = box.min.y - radius;
    box.min.z = box.min.z - radius;
    box.max.x = box.max.x + radius;
    box.max.y = box.max.y + radius;
    box.max.z = box.max.z + radius;
    if (MotionKind(body) == BodyKindUpright)
    {
        f32 across = radius * body->widthScale;
        f32 middleX = (box.min.x + box.max.x) * 0.5f;
        f32 middleZ = (box.min.z + box.max.z) * 0.5f;
        box.max.x = middleX + across;
        box.max.z = middleZ + across;
        box.min.x = middleX - across;
        box.min.z = middleZ - across;
    }

    void* results[MostTouched];
    InstanceQuery query;
    StartQuery(&query, results, MostTouched, ReferencedObjectFlags::CollisionActive);
    SkipInQuery(&query, instance);
    QueryChunkInstances(chunk, &box, g_SolidKinds, &query);
    if (body->state.pushedByVolumes)
    {
        ApplyVolumeForces(body);
    }

    if (MotionKind(body) == BodyKindSlide)
    {
        RepelFromInstances(body, sphere, &query);
    }
    else
    {
        InstanceContext* parent = instance->parent;
        for (s32 index = 0; index < query.count; index++)
        {
            auto* other = static_cast<InstanceContext*>(results[index]);
            if (other->collision.leftOut == instance || other == parent)
            {
                continue;
            }

            auto* physics = static_cast<DynamicBody*>(GetGameNode(&other->nodes, NodeRigidBody));
            if (physics != nullptr && CallVirtual<u32>(physics, physics->vtable, DynamicBody::IsSphereSlot) != 0)
            {
                if (CollideWithSphereBody(body, sphere, static_cast<SphereBody*>(physics)) != 0)
                {
                    CountContact(body);
                }

                continue;
            }

            if (other->collision.bits.stopsBodies)
            {
                if (CollideWithHulls(body, &center, &box, other, &other->collision) != 0)
                {
                    CountContact(body);
                    TellTouched(body, instance, other);
                }

                continue;
            }

            // Two rigid bodies meet once, from the instance first in memory
            if (!(instance < other))
            {
                continue;
            }

            auto* node = static_cast<ObjectNode*>(GetGameNode(&other->nodes, NodeObject));
            if (CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) == 0)
            {
                continue;
            }

            ObjectRigidBody* otherBody = node->rigidBody;
            if (otherBody != nullptr && CollideWithRigidBody(body, sphere, otherBody) != 0)
            {
                CountContact(body);
                TellTouched(body, instance, other);
            }
        }
    }

    // Slid along the touched hulls' normals on average
    if (body->bits.touchingInstance)
    {
        f32 seconds = FrameSeconds(instance);
        f32 share = 1.0f / body->contactCount;
        body->contactSum.x = body->contactSum.x * share;
        body->contactSum.y = body->contactSum.y * share;
        body->contactSum.z = body->contactSum.z * share;
        SlideAlong(seconds, body, &body->contactSum, &body->node->motion->velocity);
    }

    // Carried by what it stands on, and for a few frames after it last touched it
    MovementNode* movement = body->rideMovement;
    if (movement != nullptr && movement->bits.carries != 0)
    {
        ObjectPlace* place = body->node->owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        KeepRidePosition(body, &position);
        RideMovement(body, body->rideMovement);
        return;
    }

    if (rideMovement == nullptr || rideMovement->bits.carries == 0)
    {
        return;
    }

    body->state.rideSteps++;
    if (body->state.rideSteps == ObjectRigidBodyState::RideLostSteps)
    {
        StopRiding(body);
        return;
    }

    ObjectPlace* place = body->node->owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    KeepRidePosition(body, &position);
    RideMovement(body, rideMovement);
    body->rideMovement = rideMovement;
}

// The spheres pushed apart by the share of the overlap the body's mass has of both (the physics body moved), the physics body
// given the share of the velocities' difference while the body moves toward it, which the body loses the rest of; both told
u32 CollideWithSphereBody(ObjectRigidBody* body, const Vector4* sphere, SphereBody* other)
{
    f32 otherRadius = other->radius;
    Vector4 away = *RowOf(&other->matrix, 3);
    away.x = away.x - sphere->x;
    away.y = away.y - sphere->y;
    away.z = away.z - sphere->z;
    f32 reach = sphere->w + otherRadius;
    f32 distance = Kept(__builtin_sqrtf(away.x * away.x + away.y * away.y + away.z * away.z));
    if (!(distance < reach))
    {
        return 0;
    }

    bool pushes = true;
    f32 mass = BodyMass(body);
    f32 inverse = 1.0f / distance;
    f32 share = mass / (mass + other->mass);
    f32 depth = (reach - distance) * share;
    Vector4 push = {away.x * inverse * depth, away.y * inverse * depth, away.z * inverse * depth, 1.0f};
    const Vector4* at = RowOf(&other->matrix, 3);
    Vector4 position = {at->x + push.x, at->y + push.y, at->z + push.z, 1.0f};
    other->SetInstancePosition(&position);
    ObjectNode* node = body->node;
    Vector4 relative = node->motion->startVelocity;
    relative.x = relative.x - other->velocity.x;
    relative.y = relative.y - other->velocity.y;
    relative.z = relative.z - other->velocity.z;
    Vector4 impulse = relative;
    impulse.x = impulse.x * share;
    impulse.y = impulse.y * share;
    impulse.z = impulse.z * share;
    const Vector4* velocity = &node->motion->startVelocity;
    if (!(0.0f < velocity->x * impulse.x + velocity->y * impulse.y + velocity->z * impulse.z))
    {
        pushes = false;
    }

    Vector4 middle = node->middle;
    middle.w = 1.0f;
    Vector4 point = middle;
    if (pushes)
    {
        Matrix4x4 inverseMatrix = other->matrix;
        VuInvertRigidInPlace(&inverseMatrix);
        VuTransformPoint(&inverseMatrix, &middle, &middle);
        other->ApplyImpulse(&impulse, &middle);
    }

    ScaleImpulse(body, &impulse);
    if (ToldSphereImpulse < impulse.x * impulse.x + impulse.y * impulse.y + impulse.z * impulse.z)
    {
        impulse.x = -impulse.x;
        impulse.y = -impulse.y;
        impulse.z = -impulse.z;
        node = body->node;
        CallVirtual<void>(node, node->vtable, ObjectNode::CollidedSlot, other->owner, &point, &impulse);
        Agent* agent = body->node->agent;
        CallVirtual<u32>(agent, agent->vtable, Agent::CollidedSlot, other->owner, &point, &impulse);
    }

    if (pushes)
    {
        f32 kept = -(1.0f - share);
        relative.x = relative.x * kept;
        relative.y = relative.y * kept;
        relative.z = relative.z * kept;
        Vector4* moving = &body->node->motion->velocity;
        moving->x = moving->x + relative.x;
        moving->y = moving->y + relative.y;
        moving->z = moving->z + relative.z;
    }

    return 1;
}

void ApplyVolumeForces(ObjectRigidBody* body)
{
    InstanceContext* instance = body->node->owner;
    void* results[MostVolumes];
    InstanceQuery query;
    StartQuery(&query, results, MostVolumes, 0);
    s32 count = QueryChunkInstances(instance->chunk, &instance->collision.box, 1u << NodeMessageTrigger, &query);
    for (u32 index = 0; index < static_cast<u32>(count); index++)
    {
        auto* volume = static_cast<MessageTriggerNode*>(GetGameNode(&static_cast<InstanceContext*>(results[index])->nodes,
                                                                    NodeMessageTrigger));
        if (volume->messageBits.forceOn == 0)
        {
            continue;
        }

        if (body->node->rigidBody->physicsBody != nullptr)
        {
            FollowOwnMotionBlock(body->node);
        }

        ObjectPlace* place = volume->owner->place;
        RotateAndTranslate(place);
        Vector4 force;
        VuRotateVector(&place->matrix, &volume->force, &force);
        ObjectPlace* own = instance->place;
        own->SyncPosition();
        Vector4 position = own->position;
        ApplyRigidBodyForce(body, &force, &position);
    }
}

// The physics body given the share of the velocities' difference the body's mass has of both (at the body's middle), which the
// body loses the rest of; the body's agent told
void PushPhysicsBody(ObjectRigidBody* body, DynamicBody* other)
{
    f32 mass = BodyMass(body);
    ObjectNode* node = body->node;
    f32 share = mass / (mass + other->mass);
    Vector4 relative = node->motion->startVelocity;
    relative.x = relative.x - other->velocity.x;
    relative.y = relative.y - other->velocity.y;
    relative.z = relative.z - other->velocity.z;
    Vector4 lost = relative;
    f32 rest = 1.0f - share;
    Vector4 impulse = relative;
    impulse.x = impulse.x * share;
    impulse.y = impulse.y * share;
    impulse.z = impulse.z * share;
    Vector4 middle = node->middle;
    middle.w = 1.0f;
    Vector4 point = middle;
    Matrix4x4 inverseMatrix = other->matrix;
    VuInvertRigidInPlace(&inverseMatrix);
    VuTransformPoint(&inverseMatrix, &middle, &middle);
    other->ApplyImpulse(&impulse, &middle);
    ScaleImpulse(body, &impulse);
    if (ToldImpulse < impulse.x * impulse.x + impulse.y * impulse.y + impulse.z * impulse.z)
    {
        impulse.x = -impulse.x;
        impulse.y = -impulse.y;
        impulse.z = -impulse.z;
        Agent* agent = body->node->agent;
        CallVirtual<u32>(agent, agent->vtable, Agent::CollidedSlot, other->owner, &point, &impulse);
    }

    f32 kept = -rest;
    lost.x = lost.x * kept;
    lost.y = lost.y * kept;
    lost.z = lost.z * kept;
    Vector4* velocity = &body->node->motion->velocity;
    velocity->x = velocity->x + lost.x;
    velocity->y = velocity->y + lost.y;
    velocity->z = velocity->z + lost.z;
}

// The instance's agent told the body's instance hit it (with the velocities' difference times the body's mass), and the body's
// agent told it hit the instance
void BumpInstance(ObjectRigidBody* body, InstanceContext* other)
{
    auto* otherNode = static_cast<ObjectNode*>(GetGameNode(&other->nodes, NodeObject));
    if (otherNode == nullptr)
    {
        return;
    }

    f32 mass = BodyMass(body);
    Vector4 impulse = body->node->motion->startVelocity;
    Vector4 otherVelocity = {0.0f, 0.0f, 0.0f, 1.0f};
    Agent* otherAgent = otherNode->agent;
    if (CallVirtual<u32>(otherAgent, otherAgent->vtable, Agent::VelocitySlot, &otherVelocity) == 0)
    {
        CopyVelocity(otherNode, &otherVelocity);
    }

    impulse.x = (impulse.x - otherVelocity.x) * mass;
    impulse.y = (impulse.y - otherVelocity.y) * mass;
    impulse.z = (impulse.z - otherVelocity.z) * mass;
    ScaleImpulse(body, &impulse);
    if (!(ToldImpulse < impulse.x * impulse.x + impulse.y * impulse.y + impulse.z * impulse.z))
    {
        return;
    }

    ObjectNode* node = body->node;
    Vector4 point = node->middle;
    point.w = 1.0f;
    otherAgent = otherNode->agent;
    CallVirtual<u32>(otherAgent, otherAgent->vtable, Agent::CollidedSlot, node->owner, &point, &impulse);
    impulse.x = -impulse.x;
    impulse.y = -impulse.y;
    impulse.z = -impulse.z;
    Agent* agent = body->node->agent;
    CallVirtual<u32>(agent, agent->vtable, Agent::CollidedSlot, other, &point, &impulse);
}

// Each instance pushes the sphere toward its box's middle by the body's friction times the radius squared over how far apart
// they are beyond the box's reach (squared, at least 0.01), at most the radius
void RepelFrom(ObjectRigidBody* body, Vector4* sphere, InstanceQuery* query, Vector4* push)
{
    constexpr f32 LeastApart = Rounded(0.01);
    *push = *sphere;
    f32 radius = sphere->w;
    f32 radiusSquared = radius * radius;
    for (s32 index = 0; index < query->count; index++)
    {
        auto* other = static_cast<InstanceContext*>(query->results[index]);
        const Box* box = &other->collision.box;
        Vector4 away = box->min;
        away.x = (away.x + box->max.x) * 0.5f - sphere->x;
        away.y = (away.y + box->max.y) * 0.5f - sphere->y;
        away.z = (away.z + box->max.z) * 0.5f - sphere->z;
        f32 reach = GetBoxReach(box);
        f32 apart = away.x * away.x + away.y * away.y + away.z * away.z - reach * reach;
        f32 strength = body->friction * radiusSquared * (1.0f / __builtin_fmaxf(apart, LeastApart));
        if (radius < strength)
        {
            strength = radius;
        }

        f32 inverse = InverseLength(&away, LengthEpsilon);
        sphere->x = sphere->x + away.x * inverse * strength;
        sphere->y = sphere->y + away.y * inverse * strength;
        sphere->z = sphere->z + away.z * inverse * strength;
    }

    push->x = push->x - sphere->x;
    push->y = push->y - sphere->y;
    push->z = push->z - sphere->z;
}

void RepelFromInstances(ObjectRigidBody* body, Vector4* sphere, InstanceQuery* query)
{
    InstanceContext* instance = body->node->owner;
    Vector4 push;
    RepelFrom(body, sphere, query, &push);
    MoveInstance(instance, &push);
}

// The body (a sphere of the box's height, or an upright ellipsoid) pushed out of each of the instance's hulls whose box meets its
// box (almost all the way), unless it doesn't move for them; whether it met one
u32 CollideWithHulls(ObjectRigidBody* body, const Vector4* center, const Box* box, InstanceContext* other,
                     ObjectCollision* collision)
{
    InstanceContext* instance = body->node->owner;
    u32 touched = 0;
    if (instance->flags.attached && HasParent(instance, other) != 0)
    {
        return 0;
    }

    if (other->flags.attached && HasParent(other, instance) != 0)
    {
        return 0;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    // Its place's matrix moved to the sphere's middle
    Matrix4x4 matrix = place->matrix;
    Vector4 at = *RowOf(&matrix, 3);
    f32 radius = box->max.y - center->y;
    Vector4 offset = {center->x - at.x, center->y - at.y, center->z - at.z, 1.0f};
    Vector4* position = RowOf(&matrix, 3);
    position->x = position->x + offset.x;
    position->y = position->y + offset.y;
    position->z = position->z + offset.z;
    // (Retail works out the frame's seconds here for TouchHull, which never reads them)
    GetContextClock(instance);
    Vector4* velocity = &body->node->motion->velocity;
    for (u8 index = 0; index < GetHullCount(collision); index++)
    {
        CollisionHull* hull;
        Matrix4x4 hullMatrix;
        GetInstanceHull(collision, index, &hull, &hullMatrix);
        Box hullBox;
        GetHullBoundingBox(hull, &hullMatrix, &hullBox);
        if (BoxesOverlap(box, &hullBox) == 0)
        {
            continue;
        }

        Vector4 push;
        u32 hit;
        if (MotionKind(body) == BodyKindUpright)
        {
            f32 across = radius * body->widthScale;
            Vector4 radii = {across, radius, across, offset.w};
            Vector4 normal;
            hit = EllipsoidTouchesHull(hull, &matrix, &radii, &hullMatrix, &push, &normal);
        }
        else
        {
            Matrix4x4 inverseMatrix = hullMatrix;
            Vector4 local = *center;
            VuInvertRigidInPlace(&inverseMatrix);
            VuTransformPoint(&inverseMatrix, &local, &local);
            hit = SphereInHull(radius, hull, &local, &push);
        }

        if (hit == 0)
        {
            continue;
        }

        touched = 1;
        if (body->bits.immovable)
        {
            continue;
        }

        // The ellipsoid's push is the world's already
        if (MotionKind(body) != BodyKindUpright)
        {
            VuRotateVector(&hullMatrix, &push, &push);
        }

        push.x = push.x * PushShare;
        push.y = push.y * PushShare;
        push.z = push.z * PushShare;
        MoveInstance(instance, &push);
        // (Retail hands the push to IgnoreAttachedMove here while the instance is attached)
        MotionState* motion = body->node->motion;
        motion->startVelocity = motion->velocity;
        TouchHull(body, other, &push, velocity);
        CollisionSurface* surface = GetHullSurface(collision, index);
        if (surface != nullptr && surface->flags.sendsContactMessageToObjects != 0)
        {
            Agent* agent = body->node->agent;
            CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &surface->contact, other, 0u);
        }
    }

    return touched;
}

void RideMovement(ObjectRigidBody* body, MovementNode* movement)
{
    InstanceContext* instance = body->node->owner;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    InstanceContext* ridden = movement->owner;
    if (ridden != body->object)
    {
        body->node->flags.riding = 1;
        body->object = ridden;
        ObjectPlace* riddenPlace = ridden->place;
        RotateAndTranslate(riddenPlace);
        Matrix4x4 inverseMatrix = riddenPlace->matrix;
        VuInvertRigidInPlace(&inverseMatrix);
        place = instance->place;
        place->SyncPosition();
        body->ridePosition = place->position;
        VuTransformPoint(&inverseMatrix, &body->ridePosition, &body->ridePosition);
        return;
    }

    Matrix4x4* matrix = movement->CurrentMatrix();
    Vector4 carried;
    VuTransformPoint(matrix, &body->ridePosition, &carried);
    Vector4 move = carried;
    move.x = move.x - position.x;
    move.y = move.y - position.y;
    move.z = move.z - position.z;
    // The turn about y over the last frame (from the turn's matrix's x axis's z)
    Matrix4x4 inverseMatrix = *movement->PreviousMatrix();
    VuInvertRigidInPlace(&inverseMatrix);
    Matrix4x4 turn;
    VuMultiplyMatrices(&inverseMatrix, matrix, &turn);
    s32 yaw;
    AngleOfSine(turn.m[0][2], &yaw);
    yaw = -yaw;
    MoveInstance(instance, &move);
    bool turned = false;
    if (yaw != 0)
    {
        place = instance->place;
        place->SyncRotation();
        place->MarkTurned();
        s32 angle = yaw;
        Vector4 rotation;
        RotationFromYaw(&rotation, &angle);
        MultiplyRotations(&rotation, &rotation, &place->rotation);
        place->rotation.x = rotation.x;
        place->rotation.y = rotation.y;
        place->rotation.z = rotation.z;
        place->rotation.w = rotation.w;
        turned = true;
    }

    if (turned)
    {
        QueueObject(instance);
    }

    // The move taken into the node's velocity, the velocities swapped (retail multiplies the move by the frame's seconds where a
    // velocity would divide: the ride adds next to nothing)
    f32 seconds = FrameSeconds(instance);
    move.x = move.x * seconds;
    move.y = move.y * seconds;
    move.z = move.z * seconds;
    MotionState* motion = body->node->motion;
    Vector4 velocity = motion->startVelocity;
    velocity.x = velocity.x + move.x;
    velocity.y = velocity.y + move.y;
    velocity.z = velocity.z + move.z;
    motion->startVelocity = motion->velocity;
    motion->velocity = velocity;
}

void CollideRigidBodyWithWorld(ObjectRigidBody* body)
{
    InstanceContext* instance = body->node->owner;
    if (GetContextClock(instance)->flags.running == 0 || body->cache == nullptr)
    {
        return;
    }

    RefreshCollisionCache(body->cache, &instance->collision.box);
    if (body->bits.noWorldCollisions != 0)
    {
        return;
    }

    ObjectNode* node = body->node;
    Vector4 sphere = node->middle;
    sphere.w = node->rollRadius;
    u32 touched;
    switch (CollisionKind(body))
    {
    case BodyKindUpright:
        touched = CollideUprightWithWorld(body, &sphere);
        break;
    case BodyKindSlide:
        touched = SlideAgainstWorld(body, &sphere);
        break;
    default:
        touched = CollideSphereWithWorld(body, &sphere);
        break;
    }

    if (touched != 0)
    {
        body->bits.touching = 1;
        body->bits.touchingWorld = 1;
    }
}

// Each triangle the sphere touches takes it its friction's share of the way out
u32 SlideAgainstWorld(ObjectRigidBody* body, const Vector4* sphere)
{
    constexpr f32 SlideKnock = 25.0f;
    f32 radius = sphere->w;
    Vector4 start = *sphere;
    start.w = 1.0f;
    Vector4 center = *sphere;
    center.w = 1.0f;
    Vector4* velocity = &body->node->motion->velocity;
    Vector4 before = *velocity;
    u32 touched = 0;
    for (CollisionHit* hit = FirstCollisionHit(body->cache); hit != nullptr; hit = NextCollisionHit(body->cache))
    {
        Vector4 out = center;
        if (SphereTouchesTriangle(radius, hit, &center, &out) == 0)
        {
            continue;
        }

        touched = 1;
        Vector4 slide = center;
        slide.x = (slide.x - out.x) * body->friction;
        slide.y = (slide.y - out.y) * body->friction;
        slide.z = (slide.z - out.z) * body->friction;
        center.x = center.x - slide.x;
        center.y = center.y - slide.y;
        center.z = center.z - slide.z;
    }

    if (touched == 0)
    {
        return 0;
    }

    center.x = center.x - start.x;
    center.y = center.y - start.y;
    center.z = center.z - start.z;
    MoveInstance(body->node->owner, &center);
    // Retail bug: nothing changed the velocity since it was taken, so the knock is always 0
    f32 acrossX = before.x - velocity->x;
    f32 acrossZ = before.z - velocity->z;
    KnockNode((acrossX * acrossX + acrossZ * acrossZ) * SlideKnock, body->node);
    return 1;
}

// The spheres pushed apart, half the overlap each (all of it the other's when one doesn't move for the other), no further than
// a radius; the body's node knocked and both told of the other's contact message
u32 CollideRigidBodies(ObjectRigidBody* body, const Vector4* sphere, const Vector4* otherSphere, ObjectRigidBody* other)
{
    InstanceContext* instance = body->node->owner;
    InstanceContext* otherInstance = other->node->owner;
    if (instance->flags.attached && HasParent(instance, otherInstance) != 0)
    {
        return 0;
    }

    if (otherInstance->flags.attached && HasParent(otherInstance, instance) != 0)
    {
        return 0;
    }

    if (MotionKind(body) == BodyKindUpright || MotionKind(other) == BodyKindUpright)
    {
        return CollideUprightBodies(body, sphere, otherSphere, other);
    }

    Vector4 push = *sphere;
    push.x = push.x - otherSphere->x;
    push.y = push.y - otherSphere->y;
    push.z = push.z - otherSphere->z;
    f32 distance = Kept(__builtin_sqrtf(push.x * push.x + push.y * push.y + push.z * push.z));
    if (!(0.0f < distance))
    {
        return 0;
    }

    f32 radius = sphere->w;
    f32 otherRadius = otherSphere->w;
    f32 depth = radius + otherRadius - distance;
    if (!(0.0f < depth))
    {
        return 0;
    }

    f32 half = depth * 0.5f / distance;
    push.x = push.x * half;
    push.y = push.y * half;
    push.z = push.z * half;
    // Retail bug: each case scales the halved push again (by the overlap over the distance, or half of it) where all of the
    // overlap or half of it was meant: the spheres come apart by a fraction of it a frame
    if (body->bits.immovable)
    {
        f32 scale = -depth / distance;
        push.x = push.x * scale;
        push.y = push.y * scale;
        push.z = push.z * scale;
        ClampLength(otherRadius, body, &push);
        MoveInstance(otherInstance, &push);
    }
    else if (other->bits.immovable)
    {
        f32 scale = depth / distance;
        push.x = push.x * scale;
        push.y = push.y * scale;
        push.z = push.z * scale;
        ClampLength(radius, body, &push);
        MoveInstance(instance, &push);
    }
    else
    {
        push.x = push.x * half;
        push.y = push.y * half;
        push.z = push.z * half;
        ClampLengthToShorter(radius, otherRadius, body, &push);
        MoveInstance(instance, &push);
        push.x = -push.x;
        push.y = -push.y;
        push.z = -push.z;
        MoveInstance(otherInstance, &push);
    }

    KnockByPush(body, &push);
    TellSurfaces(body, instance, other, otherInstance);
    return 1;
}

// The same across the ground: the cylinders meet while their heights overlap
u32 CollideUprightBodies(ObjectRigidBody* body, const Vector4* sphere, const Vector4* otherSphere, ObjectRigidBody* other)
{
    f32 radius = sphere->w;
    f32 otherRadius = otherSphere->w;
    f32 top = sphere->y + radius;
    f32 otherBottom = otherSphere->y - otherRadius;
    f32 otherTop = otherSphere->y + otherRadius;
    f32 bottom = sphere->y - radius;
    InstanceContext* instance = body->node->owner;
    InstanceContext* otherInstance = other->node->owner;
    f32 otherAcross = otherRadius * other->widthScale;
    if (otherTop < bottom)
    {
        return 0;
    }

    f32 across = radius * body->widthScale;
    if (top < otherBottom)
    {
        return 0;
    }

    Vector4 push = *sphere;
    push.x = push.x - otherSphere->x;
    push.y = 0.0f;
    push.z = push.z - otherSphere->z;
    f32 distance = Kept(__builtin_sqrtf(push.x * push.x + push.z * push.z));
    f32 depth = across + otherAcross - distance;
    if (!(0.0f < depth))
    {
        return 0;
    }

    f32 half = depth * 0.5f / distance;
    push.x = push.x * half;
    push.z = push.z * half;
    // Retail bug: as with spheres, the halved push is scaled again
    if (body->bits.immovable)
    {
        f32 scale = -depth / distance;
        push.z = push.z * scale;
        push.x = push.x * scale;
        ClampLength(otherRadius, body, &push);
        MoveInstance(otherInstance, &push);
    }
    else if (other->bits.immovable)
    {
        f32 scale = depth / distance;
        push.y = 0.0f;
        push.z = push.z * scale;
        push.x = push.x * scale;
        ClampLength(radius, body, &push);
        MoveInstance(instance, &push);
    }
    else
    {
        push.z = push.z * half;
        push.x = push.x * half;
        push.y = 0.0f;
        ClampLengthToShorter(radius, otherRadius, body, &push);
        MoveInstance(instance, &push);
        push.x = -push.x;
        push.y = -push.y;
        push.z = -push.z;
        MoveInstance(otherInstance, &push);
    }

    KnockByPush(body, &push);
    TellSurfaces(body, instance, other, otherInstance);
    return 1;
}

// Pushed out of every triangle it moves into (any while it steers itself), its node told of the water ones
u32 CollideSphereWithWorld(ObjectRigidBody* body, const Vector4* sphere)
{
    f32 radius = sphere->w;
    Vector4 start = *sphere;
    start.w = 1.0f;
    f32 highest = NoRise;
    u16 surfaceId = NoSurfaceId;
    u32 touched = 0;
    u32 inWater = 0;
    Vector4 center = *sphere;
    center.w = 1.0f;
    Vector4* velocity = &body->node->motion->velocity;
    Vector4 before = *velocity;
    for (CollisionHit* hit = FirstCollisionHit(body->cache); hit != nullptr; hit = NextCollisionHit(body->cache))
    {
        // The triangles' normals point into what's solid
        Vector4 normal;
        TriangleNormal(hit, &normal);
        Vector4 into = {normal.x, normal.y, normal.z, 1.0f};
        if (!body->bits.steersItself && !(0.0f <= into.x * before.x + into.y * before.y + into.z * before.z))
        {
            continue;
        }

        Vector4 out = center;
        if (SphereTouchesTriangle(radius, hit, &center, &out) == 0)
        {
            continue;
        }

        CollisionSurface* surface = GetTriangleSurface(hit);
        if (surface->flags.solidToObjects == 0)
        {
            body->node->TouchedWater(hit, &out);
            inWater = 1;
            continue;
        }

        touched = 1;
        f32 rise = center.y - out.y;
        center = out;
        if (highest < rise)
        {
            surfaceId = surface->surfaceId;
            highest = rise;
        }

        if (body->bits.gripped)
        {
            RaiseGrip(body, body->friction * surface->friction);
        }
    }

    return FinishWorldContacts(body, touched, inWater, surfaceId, &start, &center, velocity);
}

// The same as an upright ellipsoid as wide as its radius times its width scale
u32 CollideUprightWithWorld(ObjectRigidBody* body, const Vector4* sphere)
{
    f32 radius = sphere->w;
    Vector4 start = *sphere;
    start.w = 1.0f;
    f32 highest = NoRise;
    u16 surfaceId = NoSurfaceId;
    u32 touched = 0;
    u32 inWater = 0;
    f32 across = radius * body->widthScale;
    Vector4* velocity = &body->node->motion->velocity;
    Vector4 center = *sphere;
    center.w = 1.0f;
    Vector4 before = *velocity;
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    *RowOf(&matrix, 3) = center;
    for (CollisionHit* hit = FirstCollisionHit(body->cache); hit != nullptr; hit = NextCollisionHit(body->cache))
    {
        Vector4 normal;
        TriangleNormal(hit, &normal);
        Vector4 into = {normal.x, normal.y, normal.z, 1.0f};
        if (!body->bits.steersItself && !(0.0f <= into.x * before.x + into.y * before.y + into.z * before.z))
        {
            continue;
        }

        Vector4 out = center;
        if (EllipsoidTouchesTriangle(across, radius, across, hit, &matrix, &out) == 0)
        {
            continue;
        }

        CollisionSurface* surface = GetTriangleSurface(hit);
        if (surface->flags.solidToObjects == 0)
        {
            body->node->TouchedWater(hit, &out);
            inWater = 1;
            continue;
        }

        touched = 1;
        f32 rise = center.y - out.y;
        center = out;
        *RowOf(&matrix, 3) = center;
        if (highest < rise)
        {
            surfaceId = surface->surfaceId;
            highest = rise;
        }

        if (body->bits.gripped)
        {
            RaiseGrip(body, body->friction * surface->friction);
        }
    }

    return FinishWorldContacts(body, touched, inWater, surfaceId, &start, &center, velocity);
}

// A push out of a hull counts while the velocity goes into it (always while it steers itself): it's made a normal, standing on a
// moving instance rides it, and a slope steeper than the least one it stands on stops its fall instead
u32 TouchHull(ObjectRigidBody* body, InstanceContext* other, Vector4* push, Vector4* velocity)
{
    if (!body->bits.steersItself &&
        !(velocity->x * push->x + velocity->y * push->y + velocity->z * push->z < 0.0f))
    {
        return 1;
    }

    // The pushes that count toward the contact normal: those not against it
    constexpr f32 AgainstNormal = Rounded(-0.1);
    Normalize(push);
    if (body->bits.gripped && body->grip == 0.0f)
    {
        RaiseGrip(body, TouchGrip);
    }

    if (GroundNormalY < push->y)
    {
        body->state.onGround = 1;
        if (other->flags.carriesRiders)
        {
            auto* movement = static_cast<MovementNode*>(GetGameNode(&other->nodes, NodeMovement));
            if (movement != nullptr)
            {
                body->rideMovement = movement;
            }
        }
    }
    else if (__builtin_fabsf(push->y) < WallNormalY)
    {
        body->state.againstWall = 1;
    }

    ObjectRigidBodyBits bits = body->bits;
    if (bits.slopeLimited && push->y < body->size)
    {
        velocity->y = 0.0f;
        return 0;
    }

    if (!bits.steersItself && !body->state.followsSurface)
    {
        return 1;
    }

    const Vector4* normal = &body->contactNormal;
    if (AgainstNormal < push->x * normal->x + push->y * normal->y + push->z * normal->z)
    {
        body->contactSum.x = body->contactSum.x + push->x;
        body->contactSum.y = body->contactSum.y + push->y;
        body->contactSum.z = body->contactSum.z + push->z;
        body->contactCount = body->contactCount + 1.0f;
    }

    body->untouchedTime = 0.0f;
    return 1;
}

// While the velocity goes into the surface (always while it steers itself): what goes into it taken off, the node knocked by the
// slope, the ground and wall bits set, the contact normal eased toward the normal (else made it), and on a slope steeper than the
// least it stands on its fall stopped (and while it steers itself, a contact normal that steep doesn't count)
u32 SlideAlong(f32 elapsed, ObjectRigidBody* body, Vector4* normal, Vector4* velocity)
{
    f32 into = velocity->x * normal->x + velocity->y * normal->y + velocity->z * normal->z;
    if (!body->bits.steersItself && !(into < 0.0f))
    {
        return 1;
    }

    Normalize(normal);
    RemoveComponentAlong(velocity, normal, 1);
    f32 along = normal->x * velocity->x + normal->y * velocity->y + normal->z * velocity->z;
    f32 slope = (normal->x * normal->x + normal->z * normal->z) * SlopeKnock;
    KnockNode((1.0f - along) * slope, body->node);
    if (body->bits.gripped && body->grip == 0.0f)
    {
        RaiseGrip(body, TouchGrip);
    }

    if (GroundNormalY < normal->y)
    {
        body->state.onGround = 1;
    }
    else if (__builtin_fabsf(normal->y) < WallNormalY)
    {
        body->state.againstWall = 1;
    }

    if (body->bits.slopeLimited && normal->y < body->size)
    {
        velocity->y = 0.0f;
    }

    ObjectRigidBodyBits bits = body->bits;
    if (bits.steersItself)
    {
        f32 rate = bits.ownNormalRate ? body->restitution : SteeringNormalRate;
        f32 share = elapsed * rate;
        if (1.0f < share)
        {
            share = 1.0f;
        }

        Ease(&body->contactNormal, normal, share);
        Normalize(&body->contactNormal);
        if (body->bits.slopeLimited && body->contactNormal.y < body->size)
        {
            return 0;
        }

        body->untouchedTime = 0.0f;
        return 1;
    }

    if (body->state.followsSurface)
    {
        Ease(&body->contactNormal, normal, elapsed * SurfaceNormalRate);
    }
    else
    {
        body->contactNormal = *normal;
    }

    Normalize(&body->contactNormal);
    return 1;
}

void RightRigidBody(f32 elapsed, ObjectRigidBody* body, Vector4* up)
{
    // How far it's pressed against what it stands on each frame
    constexpr f32 Press = Rounded(-0.07);
    if (!body->bits.touchingInstance && !body->bits.touchingWorld && UprightAfter < body->untouchedTime)
    {
        f32 rate = body->bits.ownNormalRate ? body->restitution * UprightRateShare : UprightRate;
        Vector4 upright = {0.0f, 1.0f, 0.0f, 1.0f};
        Ease(&body->contactNormal, &upright, rate * elapsed);
        Normalize(&body->contactNormal);
    }

    InstanceContext* instance = body->node->owner;
    *up = body->contactNormal;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    // Its x axis kept to the side: z = x across up (the other way round, then turned around), x = up across z
    Vector4 x = *RowOf(&place->matrix, 0);
    Vector4 side = {up->y * x.z - up->z * x.y, up->z * x.x - up->x * x.z, up->x * x.y - up->y * x.x, 1.0f};
    Vector4 z = {-side.x, -side.y, -side.z, 1.0f};
    Matrix4x4 matrix;
    *RowOf(&matrix, 1) = *up;
    *RowOf(&matrix, 2) = z;
    *RowOf(&matrix, 0) = {up->y * z.z - up->z * z.y, up->z * z.x - up->x * z.z, up->x * z.y - up->y * z.x, 1.0f};
    Vector4 origin = g_DefaultBox.min;
    origin.w = 1.0f;
    *RowOf(&matrix, 3) = origin;
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    // Pressed against what it stands on
    const Vector4* normal = &body->contactNormal;
    Vector4 press = {normal->x * Press, normal->y * Press, normal->z * Press, 1.0f};
    MoveInstance(instance, &press);
}

void StepRigidBody(ObjectRigidBody* body, TimeClock* clock, Vector4* move)
{
    body->node->flags.unused4 = 1;
    DynamicBody* physics = body->physicsBody;
    if (physics != nullptr && physics->bodyFlags.leftOut == 0)
    {
        if (clock->flags.running == 0)
        {
            return;
        }

        ObjectNode* node = body->node;
        if (node != nullptr && node->motionBlock != nullptr && node->motionBlock->body.pushedByVolumes)
        {
            ApplyVolumeForces(body);
        }

        if (!body->bits.falls)
        {
            return;
        }

        f32 down = -body->gravity;
        physics = body->physicsBody;
        Vector4 weight = {0.0f, down * physics->mass, 0.0f, 1.0f};
        physics->force.x = physics->force.x + weight.x;
        physics->force.y = physics->force.y + weight.y;
        physics->force.z = physics->force.z + weight.z;
        return;
    }

    if (body->bits.launch == 0 && !body->bits.moving)
    {
        return;
    }

    MotionState* motion = body->node->motion;
    if (body->state.launchSteps != ObjectRigidBodyState::MostSteps)
    {
        body->state.launchSteps++;
    }

    // Stopped once it touches something or nearly rests, 15 steps after its launch
    ObjectRigidBodyBits bits = body->bits;
    if (bits.stopsAtRest)
    {
        bool resting = bits.touchingInstance || bits.touchingWorld;
        if (!resting)
        {
            const Vector4* velocity = &motion->velocity;
            resting = velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z <
                      ObjectRigidBody::RestingSpeedSquared;
        }

        if (resting && body->state.launchSteps == ObjectRigidBodyState::MostSteps)
        {
            if (move != nullptr)
            {
                *move = g_DefaultBox.min;
                move->w = 1.0f;
            }

            body->bits.moving = 0;
            body->bits.stopsAtRest = 0;
            return;
        }
    }

    // The time since its node's last update (none the first time)
    ObjectNode* node = body->node;
    f32 seconds = 0.0f;
    if (node->time != 0)
    {
        seconds = static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit;
    }

    motion->startVelocity = motion->velocity;
    InstanceContext* instance = body->node->owner;
    // (Retail makes an empty String here and destroys it at the end, unused)
    if (!body->bits.falls)
    {
        return;
    }

    Vector4* velocity = &motion->velocity;
    velocity->y = velocity->y - body->gravity * seconds;
    if (body->bits.dragged)
    {
        f32 kept = 1.0f - body->drag;
        velocity->x = velocity->x * kept;
        velocity->y = velocity->y * kept;
        velocity->z = velocity->z * kept;
    }

    bits = body->bits;
    if (bits.gripped && (bits.touchingInstance || bits.touchingWorld) && 0.0f < body->grip)
    {
        if (1.0f < body->grip)
        {
            body->grip = 1.0f;
        }

        f32 kept = 1.0f - body->grip;
        velocity->x = velocity->x * kept;
        velocity->y = velocity->y * kept;
        velocity->z = velocity->z * kept;
    }

    body->grip = 0.0f;
    bits = body->bits;
    Vector4 step = *velocity;
    if (bits.launch == ObjectRigidBodyBits::Launched || bits.moving)
    {
        step.x = step.x * seconds;
        step.y = step.y * seconds;
        step.z = step.z * seconds;
        // No further than its roll radius while it collides
        if (CollisionKind(body) != 0)
        {
            f32 most = body->node->rollRadius;
            f32 length = Kept(__builtin_sqrtf(step.x * step.x + step.y * step.y + step.z * step.z));
            if (most < length)
            {
                f32 scale = most / length;
                step.x = step.x * scale;
                step.y = step.y * scale;
                step.z = step.z * scale;
            }
        }
    }
    else
    {
        // Only falling
        step.x = 0.0f;
        step.y = step.y * seconds;
        step.z = 0.0f;
        if (CollisionKind(body) != 0)
        {
            f32 length = Kept(__builtin_sqrtf(step.y * step.y));
            f32 most = body->node->rollRadius;
            if (most < length)
            {
                // (Retail takes x and z from the grip it cleared above: 0)
                f32 scale = most / length;
                step.z = body->grip;
                step.x = body->grip;
                step.y = step.y * scale;
            }
        }
    }

    MoveInstance(instance, &step);
    if (move != nullptr)
    {
        *move = step;
    }
}

namespace
{
// A vector made no longer than a length (x, y and z)
void CutDownTo(Vector4* vector, f32 most)
{
    f32 x = vector->x;
    f32 y = vector->y;
    f32 z = vector->z;
    f32 length = Kept(__builtin_sqrtf(x * x + y * y + z * z));
    if (most < length)
    {
        f32 scale = most / length;
        vector->z = z * scale;
        vector->x = x * scale;
        vector->y = y * scale;
    }
}
}

void PushInstanceNode(ObjectRigidBody* body, InstanceContext* other)
{
    auto* node = static_cast<GameNode*>(GetGameNode(&other->nodes, NodeObject));
    if (node != nullptr)
    {
        CallVirtual<void>(node, node->vtable, ObjectNode::PushSlot, 0.0f, body->node->owner);
    }
}

void StopRiding(ObjectRigidBody* body)
{
    body->object = nullptr;
    body->rideMovement = nullptr;
    body->node->flags.riding = 0;
}

// The position taken into the space of the instance it rides (through the inverse of its movement node's matrix now)
void KeepRidePosition(ObjectRigidBody* body, const Vector4* position)
{
    auto* ridden = static_cast<InstanceContext*>(body->object);
    if (ridden == nullptr)
    {
        return;
    }

    auto* movement = static_cast<MovementNode*>(GetGameNode(&ridden->nodes, NodeMovement));
    Matrix4x4 inverse = *movement->CurrentMatrix();
    VuInvertRigidInPlace(&inverse);
    body->ridePosition = *position;
    VuTransformPoint(&inverse, &body->ridePosition, &body->ridePosition);
}

u32 CollideWithRigidBody(ObjectRigidBody* body, const Vector4* sphere, ObjectRigidBody* other)
{
    ObjectNode* node = other->node;
    Vector4 otherSphere = node->middle;
    otherSphere.w = node->rollRadius;
    return CollideRigidBodies(body, sphere, &otherSphere, other);
}

void ClampLengthToShorter(f32 first, f32 second, ObjectRigidBody*, Vector4* vector)
{
    CutDownTo(vector, __builtin_fminf(second, first));
}

void ClampLength(f32 most, ObjectRigidBody*, Vector4* vector)
{
    CutDownTo(vector, most);
}

void HoldRigidBodyMove(ObjectRigidBody* body, Vector4* move)
{
    CutDownTo(move, body->node->rollRadius);
}

void IgnoreAttachedMove(ObjectRigidBody*, const Vector4*)
{
}
