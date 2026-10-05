#include "game/vehicles.h"

#include "game/agents.h"
#include "game/chunkdata.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"

#include <cstddef>
#include <cstdint>

EABI_EXPORT(FUN_0015ffe8, &Vehicle::Frame);
EABI_EXPORT(FUN_001608d8, &WallClingVehicle::Frame);
EABI_EXPORT(FUN_001502c8, &WallClingVehicle::Move);
EABI_EXPORT(FUN_001504b0, &WallClingVehicle::Collide);

namespace
{
// The kinds of nodes of the instances the sphere body Start gives the character collides with
constexpr u32 BodyCollisionKinds = 1u << NodeDynamicScenery | 1u << NodeRigidBody | 1u << NodeCrate | 1u << NodeCreature
                                   | 1u << NodeGenericObject;
// The script event the wall cling runs on its character every frame it goes on
constexpr u32 EventWallCling = 0x22;

// The wall cling: its box (0.58 wide, 1.8 high, standing on its position); the fall (4.5 a second faster each second, never past
// 4.5); the stick steering it along its x axis (6 times the stick's part along it less its speed that way, within 3, times 25 a
// second)
constexpr f32 ClingHalfWidth = 0.29f;
constexpr f32 ClingHeight = 1.8f;
constexpr f32 ClingGravity = 4.5f;
constexpr f32 ClingFallSpeed = 4.5f;
constexpr f32 SteerScale = 6.0f;
constexpr f32 MaxSteer = 3.0f;
constexpr f32 SteerRate = 25.0f;
// Its contacts: gathered within a frame's motion and half a unit more, with the solid instances but the character's own
constexpr f32 ReachMargin = 0.5f;
// A push out of the contacts longer than 0.001 whose direction rises more than 0.707 is a landing; the wall must be within 0.5
// along its z axis, and it creeps 0.02 toward it when there's room
constexpr f32 PushEpsilon = Rounded(0.001);
constexpr f32 LandingRise = Rounded(0.707);
constexpr f32 WallReach = 0.5f;
constexpr f32 WallCreep = 0.02f;

DynamicBody* BodyOf(const CharacterAgent* agent)
{
    return static_cast<DynamicBody*>(GetGameNode(&agent->instance->nodes, NodeRigidBody));
}

// The place of an instance that may be none (retail reads the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}
}

void Vehicle::Start()
{
    InstanceContext* instance = agent->instance;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    PhysicsWorld* world = g_PhysicsWorld;
    InitIdentityMatrix(&exitMatrix);
    InitIdentityMatrix(&agentMatrix);
    InitIdentityMatrix(&otherMatrix);
    height = 0.0f;
    RemoveBody();
    if (HasBodyVirtual() != 0)
    {
        DynamicBody* body = AddPhysicsBody(world, instance, 1);
        body->SetMatrix(&place->matrix);
        body->bits.placesInstance = 0;
        body->collisionMask = BodyCollisionKinds;
    }

    Place();
}

void Vehicle::Destroy(u32 destroyFlags)
{
    vtable = g_VehicleVTable;
    RemoveBody();
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Vehicle::Push(const Vector4* velocity, InstanceContext* instance)
{
    if (instance == nullptr)
    {
        return;
    }

    DynamicBody* body = BodyOf(agent);
    if (body == nullptr)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    Vector4 push = *velocity;
    RotateAndTranslate(place);
    VuRotateVector(&place->matrix, &push, &push);
    Vector4 moving = body->velocity;
    moving.x = moving.x + push.x;
    moving.y = moving.y + push.y;
    moving.z = moving.z + push.z;
    body->SetVelocity(&moving);
}

void Vehicle::Knock(const Vector4*, u32, InstanceContext*)
{
}

u32 Vehicle::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    if (other == nullptr)
    {
        return 1;
    }

    InstanceContext* otherInstance = other->instance;
    if (agent->instance->chunk != otherInstance->chunk)
    {
        return 0;
    }

    return otherInstance->ChangeChunk(link) != nullptr;
}

void Vehicle::Frame(f32)
{
    PlaceRiders(this);
}

u32 Vehicle::Velocity(Vector4* velocity)
{
    DynamicBody* body = BodyOf(agent);
    if (body == nullptr)
    {
        return 0;
    }

    *velocity = body->velocity;
    return 1;
}

u32 Vehicle::KeepsGun()
{
    return 0;
}

void Vehicle::SetVelocity(const Vector4*)
{
}

void Vehicle::Draw()
{
}

u32 Vehicle::HeadFollowsCamera()
{
    return 1;
}

u32 Vehicle::HasBody()
{
    return 1;
}

void Vehicle::SplashSphere(Vector4* centre, f32* radius)
{
    agent->SplashPoint(centre, radius);
}

void Vehicle::ForgetOther()
{
    other = nullptr;
}

void Vehicle::RemoveBody()
{
    DynamicBody* body = BodyOf(agent);
    if (body != nullptr)
    {
        RemovePhysicsBody(g_PhysicsWorld, body);
    }
}

void Vehicle::SetBodyPosition(const Vector4* position)
{
    DynamicBody* body = BodyOf(agent);
    if (body != nullptr)
    {
        body->SetPosition(position);
    }
}

WallClingVehicle* WallClingVehicle::Construct(WallClingVehicle* vehicle, CharacterAgent* agent)
{
    vehicle->agent = agent;
    vehicle->bits.value = 0;
    vehicle->other = nullptr;
    vehicle->vtable = g_WallClingVehicleVTable;
    vehicle->bits.drives = 1;
    vehicle->bits.held = 0;
    HullConstruct(&vehicle->hull);
    vehicle->Start();
    return vehicle;
}

void WallClingVehicle::Start()
{
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    matrix = place->matrix;
    velocity = g_DefaultBox.min;
    velocity.w = 1.0f;
    const Vector4 min = {-ClingHalfWidth, 0.0f, -ClingHalfWidth, 1.0f};
    const Vector4 max = {ClingHalfWidth, ClingHeight, ClingHalfWidth, 1.0f};
    BuildBoxHull(&hull, &min, &max);
    Vehicle::Start();
}

u32 WallClingVehicle::Place()
{
    otherMatrix = matrix;
    agentMatrix = matrix;
    exitMatrix = matrix;
    return 1;
}

void WallClingVehicle::Destroy(u32 destroyFlags)
{
    vtable = g_WallClingVehicleVTable;
    HullDestroy(&hull, DestroyOnly);
    Vehicle::Destroy(destroyFlags);
}

void WallClingVehicle::Push(const Vector4* push, InstanceContext* instance)
{
    Vector4 turned = *push;
    // Retail doesn't check the instance for none, as the base's Push does
    ObjectPlace* place = RetailPlaceOf(instance);
    RotateAndTranslate(place);
    VuRotateVector(&place->matrix, &turned, &turned);
    velocity.x = velocity.x + turned.x;
    velocity.y = velocity.y + turned.y;
    velocity.z = velocity.z + turned.z;
}

// Through a link whose chunk isn't loaded it stays, put back at the instance's kept position and stopped
u32 WallClingVehicle::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        *RowOf(&matrix, 3) = agent->instance->box;
        velocity = g_DefaultBox.min;
        velocity.w = 1.0f;
        return 0;
    }

    TransformThroughLink(link, &matrix, 1);
    TransformVectorThroughLink(link, &velocity, 0);
    return 1;
}

void WallClingVehicle::Frame(f32 seconds)
{
    Move(seconds);
    u32 ended = Collide(seconds);
    FaceWall();
    PlaceRiders(this);
    if (ended != 0)
    {
        agent->LeaveVehicle(0);
    }
    else
    {
        RunAgentEvent(agent, EventWallCling, reinterpret_cast<u32>(agent->instance), 0, 0);
    }
}

u32 WallClingVehicle::Kind()
{
    return KindWallCling;
}

u32 WallClingVehicle::Velocity(Vector4* out)
{
    *out = velocity;
    return 1;
}

void WallClingVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-1.0f, -1.0f, -1.0f, 1.0f};
    *max = {1.0f, 1.0f, 1.0f, 1.0f};
}

// Retail ignores the starting velocity: the cling starts still (ClingToWall's push away from the wall is lost)
void WallClingVehicle::SetVelocity(const Vector4*)
{
}

void WallClingVehicle::Draw()
{
}

u32 WallClingVehicle::HasBody()
{
    return 0;
}

void WallClingVehicle::Move(f32 seconds)
{
    velocity.y = velocity.y - seconds * ClingGravity;
    if (velocity.y < -ClingFallSpeed)
    {
        velocity.y = -ClingFallSpeed;
    }

    Vector4* position = RowOf(&matrix, 3);
    position->x = position->x + velocity.x * seconds;
    position->y = position->y + velocity.y * seconds;
    position->z = position->z + velocity.z * seconds;
    // The stick (sideways along the world's x, forward along its z) along its x axis, less its speed along it
    const Vector4* side = RowOf(&matrix, 0);
    const CharacterButtons& buttons = agent->buttons;
    f32 steer = (buttons.moveX * side->x + buttons.moveZ * side->z) * SteerScale
                - (side->x * velocity.x + side->y * velocity.y + side->z * velocity.z);
    if (MaxSteer < steer)
    {
        steer = MaxSteer;
    }
    else if (steer < -MaxSteer)
    {
        steer = -MaxSteer;
    }

    velocity.x = velocity.x + side->x * steer * seconds * SteerRate;
    velocity.y = velocity.y + side->y * steer * seconds * SteerRate;
    velocity.z = velocity.z + side->z * steer * seconds * SteerRate;
}

u32 WallClingVehicle::Collide(f32 seconds)
{
    Vector4* position = RowOf(&matrix, 3);
    ContactSet* contacts = BeginContacts();
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    InstanceContext* instance = agent->instance;
    ChunkData* chunk = instance->chunk;
    // Along the world's z axis rather than the motion
    Vector4 reach = {0.0f, 0.0f, speed * seconds + ReachMargin, 1.0f};
    GatherTriangleContacts(contacts, chunk, position, &reach, &hull);
    u32 kinds = SolidNodeKinds;
    InstanceContext* skipped[] = {instance};
    GatherInstanceContacts(contacts, chunk, position, &reach, &kinds, skipped, 1, &hull);
    Vector4 push;
    f32 pushed;
    Depenetrate(contacts, position, &push, 1, 1, 1, &pushed);
    position->x = position->x + push.x;
    position->y = position->y + push.y;
    position->z = position->z + push.z;
    if (PushEpsilon < __builtin_sqrtf(push.x * push.x + push.y * push.y + push.z * push.z)
        && LandingRise < push.y * InverseLength(&push, LengthEpsilon))
    {
        EndContacts();
        return 1;
    }

    const Vector4* forward = RowOf(&matrix, 2);
    Vector4 probe = *position;
    probe.x = probe.x + forward->x * WallReach;
    probe.y = probe.y + forward->y * WallReach;
    probe.z = probe.z + forward->z * WallReach;
    if (PointInsideSolidContact(0.0f, contacts, &probe, ContactKind::SphereBit) == 0)
    {
        EndContacts();
        // Retail then steps a probe from 0 to 2 units ahead by 0.1 without testing it (a loop left empty)
        return 1;
    }

    probe = *position;
    probe.x = probe.x + forward->x * WallCreep;
    probe.y = probe.y + forward->y * WallCreep;
    probe.z = probe.z + forward->z * WallCreep;
    if (PointInsideSolidContact(0.0f, contacts, &probe, ContactKind::SphereBit) == 0)
    {
        *position = probe;
    }

    EndContacts();
    return 0;
}

void WallClingVehicle::FaceWall()
{
    Vector4 forward = *RowOf(&matrix, 2);
    Vector4 wall = wallNormal;
    Vector4 axis;
    axis.x = forward.y * wall.z - forward.z * wall.y;
    axis.z = forward.x * wall.y - forward.y * wall.x;
    axis.y = forward.z * wall.x - forward.x * wall.z;
    axis.w = 1.0f;
    f32 inverse = InverseLength(&forward, LengthEpsilon);
    f32 crossSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    forward.x = forward.x * inverse;
    forward.z = forward.z * inverse;
    forward.y = forward.y * inverse;
    // Not when its z axis already points along the wall's direction
    if (!(Epsilon < crossSquared))
    {
        return;
    }

    f32 axisInverse = InverseLength(&axis, LengthEpsilon);
    f32 cosine = forward.x * wall.x + forward.y * wall.y + forward.z * wall.z;
    axis.x = axis.x * axisInverse;
    axis.z = axis.z * axisInverse;
    axis.y = axis.y * axisInverse;
    s32 angle;
    AngleOfCosine(cosine, &angle);
    Matrix4x4 turn;
    AxisAngleMatrix(&turn, &axis, &angle, 0);
    Vector4 position = *RowOf(&matrix, 3);
    VuMultiplyMatrices(&matrix, &turn, &matrix);
    *RowOf(&matrix, 3) = position;
}

PassengerVehicle* PassengerVehicle::Construct(PassengerVehicle* vehicle, u32 kind, CharacterAgent* agent,
                                              CharacterAgent* driver)
{
    vehicle->agent = agent;
    vehicle->bits.value = 0;
    vehicle->other = driver;
    vehicle->vtable = g_PassengerVehicleVTable;
    vehicle->kind = kind;
    vehicle->bits.drives = 0;
    vehicle->bits.held = 0;
    return vehicle;
}

u32 PassengerVehicle::Place()
{
    return 0;
}

void PassengerVehicle::Destroy(u32 destroyFlags)
{
    Vehicle::Destroy(destroyFlags);
}

void PassengerVehicle::Push(const Vector4*, InstanceContext*)
{
}

u32 PassengerVehicle::Kind()
{
    return kind;
}

void PassengerVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-1.0f, -1.0f, -1.0f, 1.0f};
    *max = {1.0f, 1.0f, 1.0f, 1.0f};
}

u32 PassengerVehicle::HasBody()
{
    return 0;
}

void PassengerVehicle::SplashSphere(Vector4* centre, f32* radius)
{
    CharacterAgentOf(other->instance)->SplashPoint(centre, radius);
}
