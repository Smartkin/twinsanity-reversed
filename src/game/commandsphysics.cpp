#include "game/commands.h"

#include "game/clock.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/rigidbody.h"

// The commands that work on an object node's rigid body (its collisions, launches, impulses and forces), and the rigid body's own
// functions: made and destroyed, its kinds of motion and of collisions set (its chunk's two lists of rigid bodies, its collision
// cache, its physics body made and let go of), active again and out of action, its values (gravity, restitution, drags, friction,
// mass, centre of mass, a turn) and its contacts, sized, pushed by an impulse or a force, and its magnet's pull

extern "C"
{
    // A rigid body active again, sized (the float first), its physics body made for a kind, pushed by a force at a point over the
    // time since its node's last update, and the force its magnet puts on what's at a position
    void ActivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00246950);
    void SetRigidBodySize(f32 size, ObjectRigidBody* body) RETAIL_N32(FUN_00246a88);
    void MakePhysicsBody(ObjectRigidBody* body, u32 kind) RETAIL(FUN_00247270);
    void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point) RETAIL(FUN_0024c508);
    void MagnetPull(ObjectRigidBody* body, const Vector4* position, Vector4* pull) RETAIL(FUN_0024c690);

    // A turn (radians, the float first) from the place its node keeps, its contacts forgotten (what it rides let go of, whether
    // it touched anything kept), and its kind of motion made none (out of its chunk's first list)
    void SetRigidBodyTurn(f32 radians, ObjectRigidBody* body) RETAIL_N32(FUN_002549b8);
    void ForgetRigidBodyContacts(ObjectRigidBody* body) RETAIL(FUN_00254a10);
    void StopRigidBodyMotion(ObjectRigidBody* body) RETAIL(FUN_00254428);

    // The node kinds whose instances the physics bodies collide with (the start-up sets them)
    extern u32 g_PhysicsBodyKinds RETAIL(D_0030A11C);
}

EABI_EXPORT(FUN_00246a88, SetRigidBodySize);
EABI_EXPORT(FUN_002541e0, SetRigidBodyGravity);
EABI_EXPORT(FUN_002542a0, SetRigidBodyRestitution);
EABI_EXPORT(FUN_00254340, SetRigidBodyFriction);
EABI_EXPORT(FUN_002542e0, SetRigidBodyDrag);
EABI_EXPORT(FUN_00254310, SetRigidBodyLengthDrag);
EABI_EXPORT(FUN_00254380, SetRigidBodySpinFriction);
EABI_EXPORT(FUN_002543a8, SetRigidBodyMass);
EABI_EXPORT(FUN_002549b8, SetRigidBodyTurn);

namespace
{
// The rigid body's 64 bits at 0x88: the kind of its motion (bits 32-35) and of its collisions (36-39), 0 none. Below 9 its node's
// motion moves it (it's in its chunk's first list) and its collision cache's triangles stop it; from 9 on its physics body does
// (9 a sphere of the node's roll radius, 10 and 11 hulls sized by its instance's own box, more hulls). Then how it's launched
// (40-41), its being in the first list (44), a size given (48), bit 55 of the collisions command, its moving (56: its frame
// steps it), its stopping (57: 15 steps after its launch, once it touches something or nearly rests, it stops moving), its
// steering itself (59), a size and a restitution given without its physics body told (60, 61)
constexpr u32 MotionKindShift = 32;
constexpr u32 CollisionKindShift = 36;
constexpr u64 KindMask = 0xF;
constexpr u32 PlainKind = 1;
constexpr u32 PhysicsKinds = 9;
constexpr u32 SphereKind = 9;
constexpr u32 BoxKindsEnd = 12;
constexpr u32 LaunchShift = 40;
constexpr u64 LaunchMask = u64{3} << LaunchShift;
constexpr u64 InFirstList = u64{1} << 44;
constexpr u64 SizeGiven = u64{1} << 48;
constexpr u64 Bit55 = u64{1} << 55;
constexpr u64 Moving = u64{1} << 56;
constexpr u64 StopsAtRest = u64{1} << 57;
constexpr u64 SteersItself = u64{1} << 59;
constexpr u64 SizeGivenAlone = u64{1} << 60;
constexpr u64 RestitutionGivenAlone = u64{1} << 61;
// What the values given set: its velocity dragged (45), gripped by what it touches (46), its own restitution (47), it falls (49),
// it turns (58), its own length drag (62); and what it touched (51 an instance, 52 the world, 53 anything)
constexpr u64 Dragged = u64{1} << 45;
constexpr u64 Gripped = u64{1} << 46;
constexpr u64 OwnRestitution = u64{1} << 47;
constexpr u64 Falls = u64{1} << 49;
constexpr u64 TouchingInstance = u64{1} << 51;
constexpr u64 TouchingWorld = u64{1} << 52;
constexpr u64 Touching = u64{1} << 53;
constexpr u64 Turning = u64{1} << 58;
constexpr u64 OwnLengthDrag = u64{1} << 62;
// Made, its four bytes are set, bits 48, 49, 53 and 63 left as the memory had them and the others cleared
constexpr u64 MadeBytes = 0xFFFFFFFF;
constexpr u64 MadeKept = 0x8023000000000000 | MadeBytes;
// Its word at 0x90: bits 0, 11-14 and 19 set when it's made (1, 2 and 26-31 left, the others cleared), 2 and 3 what the commands
// set, 6-7 and 8-9 the physics sizes' modes (the magnet's strength and its way: 0 from the body, 1 its z axis), 10 it's out of
// action, 11-14 its steps since it was launched (counting up to 15), 21-23 and 25 what the collisions command sets
constexpr u64 MadeCleared90 = 0x3F787F8;
constexpr u64 MadeSet90 = 0x87801;
constexpr u64 Bit2 = 0x4;
constexpr u64 Bit3 = 0x8;
constexpr u32 StrengthShift = 6;
constexpr u32 WayShift = 8;
constexpr u64 OutOfAction = 0x400;
constexpr u64 LaunchSteps = 0x7800;
constexpr u64 Bit21 = 0x200000;
constexpr u64 Bit22 = 0x400000;
constexpr u64 Bit23 = 0x800000;
constexpr u64 Bit25 = 0x2000000;
// On the ground and against a wall this frame (bits 1 and 5), whether it touched anything when its contacts were forgotten (24)
constexpr u64 OnGround = 0x2;
constexpr u64 AgainstWall = 0x20;
constexpr u64 TouchedBefore = 0x1000000;
// The lists' room and the indexes of a body in neither
constexpr u16 MostListed = 0xFF;
constexpr u16 NoFirstIndex = 0xFFFF;
constexpr u8 NoSecondIndex = 0xFF;

constexpr u32 ObjectNodeKind = 1;
constexpr u32 PhysicsBodyKind = 5;
// The object node's vtable functions 15 (whether it takes packets) and 36 (a designator's instance), the physics body's 15
// (whether it's a sphere)
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 IsSphereSlot = 15;
// The physics body's vtable function telling it its centre of mass moved
constexpr u32 CenterOfMassSlot = 17;
constexpr u8 NoDesignator = 0xFF;
// The instance's flag 6: it's attached to its parent
constexpr u32 AttachedFlag = 0x40;
// The surfaces whose triangles a rigid body's collision cache gathers
constexpr u32 CacheMask = 0x50;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 Epsilon = 0x1.a36e2ep-15f;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0;
}

// The commands' floats that commands.h has as integers
f32 FloatOf(const s32& value)
{
    return __builtin_bit_cast(f32, value);
}

u32 MotionKind(const ObjectRigidBody* body)
{
    return body->bits88 >> MotionKindShift & KindMask;
}

u32 CollisionKind(const ObjectRigidBody* body)
{
    return body->bits88 >> CollisionKindShift & KindMask;
}

void SetMotionKind(ObjectRigidBody* body, u32 kind)
{
    body->bits88 = (body->bits88 & ~(KindMask << MotionKindShift)) | (kind & KindMask) << MotionKindShift;
}

void SetCollisionKind(ObjectRigidBody* body, u32 kind)
{
    body->bits88 = (body->bits88 & ~(KindMask << CollisionKindShift)) | (kind & KindMask) << CollisionKindShift;
}

// A body taken out of its chunk's first or second list (the last one moved into its place) and put in (no room past 255), the
// retail code's inlined TakeFirstRigidBody and co.
void TakeFromFirstList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u16 index = body->firstIndex;
    if (index == NoFirstIndex)
    {
        return;
    }

    bodies->firstCount--;
    ObjectRigidBody* last = bodies->first[bodies->firstCount];
    bodies->first[index] = last;
    last->firstIndex = index;
    ForgetFirstIndex(body);
}

void TakeFromSecondList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u8 index = body->secondIndex;
    if (index == NoSecondIndex)
    {
        return;
    }

    bodies->secondCount--;
    ObjectRigidBody* last = bodies->second[bodies->secondCount];
    bodies->second[index] = last;
    last->secondIndex = index;
    ForgetSecondIndex(body);
}

void PutInFirstList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->firstCount >= MostListed)
    {
        return;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->firstIndex = bodies->firstCount;
    bodies->first[bodies->firstCount++] = body;
}

void PutInSecondList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->secondCount >= MostListed)
    {
        return;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->secondIndex = static_cast<u8>(bodies->secondCount);
    bodies->second[bodies->secondCount++] = body;
}

// The mass of a node's physics body: its motion block's when that's above 0, else the node's first float property
f32 BodyMass(ObjectNode* node)
{
    MotionBlock* block = node->motionBlock;
    if (block != nullptr && !(block->mass <= 0.0f))
    {
        return block->mass;
    }

    return node->PacketProperties()->GetFloat(0);
}

Vector4 SidesOf(const Box* box)
{
    return {box->max.x - box->min.x, box->max.y - box->min.y, box->max.z - box->min.z, 1.0f};
}

f32 LargestSide(const Box* box)
{
    Vector4 sides = SidesOf(box);
    f32 largest = sides.x;
    if (largest < sides.y)
    {
        largest = sides.y;
    }

    if (largest < sides.z)
    {
        largest = sides.z;
    }

    return largest;
}

// The motion's velocity replaced, the one it had kept as its start
void SetVelocity(MotionState* motion, const Vector4* velocity)
{
    motion->startVelocity = motion->velocity;
    motion->velocity = *velocity;
}

// A point of the world in a place's own space
Vector4 PointInPlace(ObjectPlace* place, const Vector4* point)
{
    Vector4 local = *point;
    RotateAndTranslate(place);
    Matrix4x4 inverse = place->matrix;
    VuInvertRigidInPlace(&inverse);
    VuTransformPoint(&inverse, &local, &local);
    return local;
}

// The seconds since a node's last update (none when it never was)
f32 SecondsSinceUpdate(const GameNode* node, const TimeClock* clock)
{
    if (node->time == 0)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit;
}

// Where a launch goes, by its bits (0-3 the space, 4-11 a receiver, 12-19 the designator, 21 the offset given): the current
// route step's position (or 0xEF's), else the designated one
void LaunchTarget(u32 bits, const Vector4* offset, f32 first, f32 second, f32 third, BehaviourRunner* runner, Vector4* target)
{
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 ReceiverShift = 4;
    constexpr u32 DesignatorShift = 12;
    constexpr u32 DesignatorMask = 0xFF;
    constexpr u32 OffsetGiven = 0x200000;
    constexpr u32 RouteStep = 0xEF;
    ObjectNode* node = NodeOf(runner);
    u32 designator = bits >> DesignatorShift & DesignatorMask;
    if (designator == DesignatesCurrentStep || designator == RouteStep)
    {
        RouteStepPosition(node->waypoints, target, node, 1, first, second, third, 0.0f, 0.0f);
        return;
    }

    DesignatedPosition(target, bits & SpaceMask, runner, (bits & OffsetGiven) != 0 ? offset : nullptr, designator,
                       bits >> ReceiverShift & DesignatorMask, 0);
}
}

// Half the largest side of the instance's box as value2 (when asked), then value2 as the node's roll radius and its physics
// sphere's radius (an ellipsoid made a sphere)
void SetLogicalRadiusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FromBox = 0x1;
    ObjectNode* node = NodeOf(runner);
    if ((value1 & FromBox) != 0)
    {
        value2 = LargestSide(node->owner->CollisionBox()) * 0.5f;
    }

    if (!TakesPackets(node))
    {
        return;
    }

    node->rollRadius = value2;
    auto* body = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, PhysicsBodyKind));
    if (body == nullptr || CallVirtual<u32>(body, body->vtable, IsSphereSlot) == 0)
    {
        return;
    }

    auto* sphere = static_cast<SphereBody*>(body);
    sphere->ellipsoid = 0;
    sphere->radius = value2;
}

// The node's roll radius; its rigid body (made the first time) given kinds of motion and of collisions (bits 0-3 and 4-7), let go
// of when it's left with neither, and given the values of the flags. value2 to value4 and value13 are floats (commands.h has
// integers)
void SetCollisionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 MotionKindGiven = 0x100;
    constexpr u32 CollisionKindGiven = 0x200;
    constexpr u32 RadiusGiven = 0x400;
    constexpr u32 GravityGiven = 0x800;
    constexpr u32 Flag12 = 0x1000;
    constexpr u32 DragGiven = 0x2000;
    constexpr u32 FrictionGiven = 0x4000;
    constexpr u32 RestitutionGiven = 0x8000;
    constexpr u32 SizeGivenFlag = 0x10000;
    constexpr u32 LaunchFlagsShift = 17;
    constexpr u32 CenterOfMassGiven = 0x80000;
    constexpr u32 Steers = 0x100000;
    constexpr u32 SizeAlone = 0x200000;
    constexpr u32 RestitutionAlone = 0x400000;
    constexpr u32 SpinFrictionGiven = 0x800000;
    constexpr u32 LengthDragGiven = 0x1000000;
    constexpr u32 Flag25 = 0x2000000;
    constexpr u32 Flag26 = 0x4000000;
    constexpr u32 RadiusFromBox = 0x8000000;
    constexpr u32 RadiusFromHeight = 0x10000000;
    constexpr u32 Flag29 = 0x20000000;
    constexpr u32 Flag30 = 0x40000000;
    constexpr u32 Flag31 = 0x80000000;
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    if ((flags & RadiusGiven) != 0)
    {
        node->rollRadius = height;
    }

    if ((flags & RadiusFromBox) != 0)
    {
        node->rollRadius = LargestSide(node->owner->CollisionBox()) * 0.5f;
    }
    else if ((flags & RadiusFromHeight) != 0)
    {
        // Half the box's height, and value14 the larger of its half width and half depth over it
        Vector4 sides = SidesOf(node->owner->CollisionBox());
        f32 half = sides.y * 0.5f;
        node->rollRadius = half;
        f32 inverse = 1.0f / half;
        value14 = __builtin_fmaxf(sides.z * 0.5f, sides.x * 0.5f) * inverse;
    }

    // A body that's there keeps its kind of motion
    if ((flags & MotionKindGiven) != 0)
    {
        if ((flags & KindMask) == 0)
        {
            if (body != nullptr)
            {
                ListRigidBodyFirst(body, 0);
            }
        }
        else if (body == nullptr)
        {
            body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
            ListRigidBodyFirst(body, flags & KindMask);
            node->ReleaseRigidBody();
            node->rigidBody = body;
        }
    }

    if ((flags & CollisionKindGiven) != 0)
    {
        u32 kind = flags >> 4 & KindMask;
        if (kind == 0)
        {
            if (body == nullptr)
            {
                return;
            }

            ListRigidBodySecond(body, 0);
        }
        else
        {
            if (body == nullptr)
            {
                body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
                node->ReleaseRigidBody();
                node->rigidBody = body;
            }

            ListRigidBodySecond(body, kind);
        }
    }

    if (body == nullptr)
    {
        return;
    }

    if ((flags & Flag31) != 0)
    {
        body->bits90 |= Bit23;
    }

    if ((flags & GravityGiven) != 0)
    {
        SetRigidBodyGravity(radius.FloatWith(node->PacketProperties()), body);
    }
    else
    {
        ClearRigidBodyGravity(body);
    }

    body->bits88 = (flags & Flag12) != 0 ? body->bits88 | Bit55 : body->bits88 & ~Bit55;
    if ((flags & Steers) != 0)
    {
        body->bits88 |= SteersItself;
    }

    body->bits90 = (flags & Flag25) != 0 ? body->bits90 | Bit2 : body->bits90 & ~Bit2;
    body->bits90 = (flags & Flag26) != 0 ? body->bits90 | Bit3 : body->bits90 & ~Bit3;
    body->unknownA0 = value14;
    body->unknownBC = value15;
    if ((flags & Flag29) != 0)
    {
        body->bits90 |= Bit21;
    }
    else if ((flags & Flag30) != 0)
    {
        body->bits90 |= Bit22;
    }

    if ((body->bits88 & (KindMask << MotionKindShift | KindMask << CollisionKindShift)) == 0)
    {
        node->ReleaseRigidBody();
        return;
    }

    if ((flags & DragGiven) != 0)
    {
        SetRigidBodyDrag(value8, body);
    }

    if ((flags & LengthDragGiven) != 0)
    {
        SetRigidBodyLengthDrag(value9, body);
    }
    else if (value9 < 0.0f)
    {
        body->bits90 |= Bit25;
    }

    if ((flags & FrictionGiven) != 0)
    {
        SetRigidBodyFriction(value10, body);
    }

    if ((flags & RestitutionGiven) != 0)
    {
        SetRigidBodyRestitution(value11, body);
    }
    else if ((flags & RestitutionAlone) != 0)
    {
        body->restitution = value11;
        body->bits88 |= RestitutionGivenAlone;
    }

    if ((flags & SizeGivenFlag) != 0)
    {
        SetRigidBodySize(value12, body);
    }
    else if ((flags & SizeAlone) != 0)
    {
        body->size = value12;
        body->bits88 |= SizeGivenAlone;
    }

    if ((flags & SpinFrictionGiven) != 0)
    {
        SetRigidBodySpinFriction(FloatOf(value13), body);
    }

    if ((flags & CenterOfMassGiven) != 0)
    {
        SetRigidBodyCenterOfMass(body, reinterpret_cast<const Vector4*>(&value2));
    }

    if (body->physicsBody == nullptr)
    {
        body->bits88 = (body->bits88 & ~LaunchMask) | u64{flags >> LaunchFlagsShift & 3} << LaunchShift;
    }

    body->bits88 &= ~StopsAtRest;
    body->bits90 |= LaunchSteps;
}

// The rigid body moving, its values of value1's bits (1 the drag, 2 the friction, 3 the restitution, 4 it stops at rest, launched
// again) and the node's velocity (bit 0). value6 to value8 are floats (commands.h has integers)
void ContinueColliderMotionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 VelocityGiven = 0x1;
    constexpr u32 DragGiven = 0x2;
    constexpr u32 FrictionGiven = 0x4;
    constexpr u32 RestitutionGiven = 0x8;
    constexpr u32 StopsFlag = 0x10;
    ObjectNode* node = NodeOf(runner);
    // Retail bug: without a rigid body it writes the bits at 0x88 (and the values' setters read through it)
    ObjectRigidBody* body = node->rigidBody;
    body->bits88 |= Moving;
    if ((value1.raw & VelocityGiven) != 0)
    {
        // Retail bug: the vector it turns into the world is its stack's, never the velocity given (velX to velZ), so the node
        // gets whatever was left there (none here)
        Vector4 velocity = {};
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(node->motion, &velocity);
    }

    if ((value1.raw & FrictionGiven) != 0)
    {
        SetRigidBodyFriction(FloatOf(value7), body);
    }

    if ((value1.raw & DragGiven) != 0)
    {
        SetRigidBodyDrag(FloatOf(value6), body);
    }

    if ((value1.raw & RestitutionGiven) != 0)
    {
        SetRigidBodyRestitution(FloatOf(value8), body);
    }

    if ((value1.raw & StopsFlag) != 0)
    {
        body->bits88 |= StopsAtRest;
        body->bits90 &= ~LaunchSteps;
    }
}

// The node's velocity: the one given (turned from the instance's space into the world), or the throw to the launch's target under
// the rigid body's gravity, over a height (the target's height above the node added when it's above, with flags2's bit 3) or at
// an angle; then the rigid body's values of the flags and its contacts forgotten. value8, value10 and value12 to value14 are
// floats, value11 a float's bits (commands.h has integers)
void ColliderLaunchNowCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 VelocityGiven = 0x100000;
    constexpr u32 DragGiven = 0x400000;
    constexpr u32 FrictionGiven = 0x800000;
    constexpr u32 RestitutionGiven = 0x1000000;
    constexpr u32 TurnGiven = 0x2000000;
    constexpr u32 StopsFlag = 0x8000000;
    constexpr u32 ThrownAtAngle = 0x10000000;
    constexpr u32 ThrownOverHeight = 0x20000000;
    constexpr u32 AddsRise = 0x8;
    constexpr u32 Flag2Of90 = 0x10;
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    body->bits88 |= Moving;
    MotionState* motion = node->motion;
    if ((flags & VelocityGiven) != 0)
    {
        Vector4 velocity = {x, y, z, w};
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(motion, &velocity);
    }
    else
    {
        ObjectPlace* place = node->owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        Vector4 target = g_DefaultBox.min;
        target.w = 1.0f;
        LaunchTarget(flags, reinterpret_cast<const Vector4*>(&x), FloatOf(value12), FloatOf(value13), FloatOf(value14), runner,
                     &target);
        // Retail bug: with neither kind of throw the node gets the velocity its stack had (none here)
        Vector4 velocity = {};
        f32 gravity = body->gravity;
        if ((flags & ThrownOverHeight) != 0)
        {
            f32 height = power;
            if ((flags2 & AddsRise) != 0)
            {
                f32 rise = target.y - position.y;
                if (0.0f < rise)
                {
                    height = rise + height;
                }
            }

            ThrowOverHeight(gravity, height, &position, &target, &velocity);
        }
        else if ((flags & ThrownAtAngle) != 0)
        {
            ThrowInTime(gravity, power, &position, &target, &velocity);
        }

        SetVelocity(motion, &velocity);
    }

    if ((flags & RestitutionGiven) != 0)
    {
        SetRigidBodyRestitution(FloatOf(value10), body);
    }

    if ((flags & FrictionGiven) != 0)
    {
        SetRigidBodyFriction(value9, body);
    }

    if ((flags & DragGiven) != 0)
    {
        SetRigidBodyDrag(FloatOf(value8), body);
    }

    if ((flags & TurnGiven) != 0)
    {
        SetRigidBodyTurn(__builtin_bit_cast(f32, value11), body);
    }

    body->bits90 = (flags2 & Flag2Of90) != 0 ? body->bits90 | Bit2 : body->bits90 & ~Bit2;
    if ((flags & StopsFlag) != 0)
    {
        body->bits88 |= StopsAtRest;
        body->bits90 &= ~LaunchSteps;
    }

    ForgetRigidBodyContacts(body);
}

// ColliderLaunchNow's velocity (the designated target alone), then the physics body launched with it, spinning by value7 about
// the instance's x axis or else by value8 about its y axis. Without a rigid body retail reads its gravity at 0xA4
void LaunchAtTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 SpaceMask = 0xF;
    constexpr u32 ReceiverShift = 4;
    constexpr u32 DesignatorShift = 12;
    constexpr u32 DesignatorMask = 0xFF;
    constexpr u32 VelocityGiven = 0x100000;
    constexpr u32 OffsetGiven = 0x200000;
    constexpr u32 ThrownAtAngle = 0x400000;
    constexpr u32 ThrownOverHeight = 0x800000;
    constexpr u32 AddsRise = 0x1000000;
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    MotionState* motion = node->motion;
    u32 bits = static_cast<u32>(target.raw);
    const auto* offset = reinterpret_cast<const Vector4*>(&offsetX);
    // Retail bug: with neither kind of throw the node and its physics body get the velocity the stack had (none here)
    Vector4 velocity = {};
    if ((bits & VelocityGiven) != 0)
    {
        velocity = *offset;
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(motion, &velocity);
    }
    else
    {
        ObjectPlace* place = node->owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        Vector4 goal = g_DefaultBox.min;
        goal.w = 1.0f;
        DesignatedPosition(&goal, bits & SpaceMask, runner, (bits & OffsetGiven) != 0 ? offset : nullptr,
                           bits >> DesignatorShift & DesignatorMask, bits >> ReceiverShift & DesignatorMask, 0);
        f32 gravity = body->gravity;
        if ((bits & ThrownOverHeight) != 0)
        {
            f32 height = speedOrAngle;
            if ((bits & AddsRise) != 0)
            {
                f32 rise = goal.y - position.y;
                if (0.0f < rise)
                {
                    height = rise + height;
                }
            }

            ThrowOverHeight(gravity, height, &position, &goal, &velocity);
        }
        else if ((bits & ThrownAtAngle) != 0)
        {
            ThrowInTime(gravity, speedOrAngle, &position, &goal, &velocity);
        }

        SetVelocity(motion, &velocity);
    }

    LaunchPhysicsBody(value8, value7, node, &velocity);
}

// The designated instance's (the low byte a designator, else the originator with bit 10, else the node's own) pushed: by an
// impulse (bit 12: the velocity given, turned from the node's instance's space, on its rigid body at its position or else on its
// physics body at its middle) or else on its physics body's angular momentum (bit 11: value6 to value8)
void ApplyImpulseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FromOriginator = 0x400;
    constexpr u32 TurnGiven = 0x800;
    constexpr u32 VelocityGiven = 0x1000;
    InstanceContext* instance;
    u8 designator = static_cast<u8>(value1);
    if (designator != NoDesignator)
    {
        GameNode* agentNode = runner->agentNode;
        instance = CallVirtual<InstanceContext*>(agentNode, agentNode->vtable, GetDesignatorSlot, static_cast<u32>(designator));
    }
    else if ((value1 & FromOriginator) != 0)
    {
        instance = static_cast<InstanceContext*>(runner->originator);
    }
    else
    {
        instance = runner->agentNode->owner;
    }

    if (instance == nullptr)
    {
        return;
    }

    NodeList* nodes = &instance->nodes;
    if (GetGameNode(nodes, ObjectNodeKind) == nullptr)
    {
        return;
    }

    if ((value1 & VelocityGiven) != 0)
    {
        Vector4 impulse = {velX, velY, velZ, value5};
        ObjectPlace* place = runner->agentNode->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &impulse, &impulse);
        // Retail reads through the node when it doesn't take packets (none)
        ObjectNode* node = PacketNodeOf(instance);
        FollowOwnMotionBlock(node);
        ObjectRigidBody* body = node->rigidBody;
        if (body != nullptr)
        {
            ObjectPlace* bodyPlace = node->owner->place;
            bodyPlace->SyncPosition();
            Vector4 point = bodyPlace->position;
            PushRigidBody(body, &impulse, &point);
            return;
        }

        // Retail looks for the physics body a second time when there's none
        auto* physics = static_cast<RigidBody*>(GetGameNode(nodes, PhysicsBodyKind));
        if (physics == nullptr)
        {
            physics = static_cast<RigidBody*>(GetGameNode(nodes, PhysicsBodyKind));
            if (physics == nullptr)
            {
                return;
            }
        }

        Vector4 middle = {0.0f, 0.0f, 0.0f, 1.0f};
        physics->ApplyImpulse(&impulse, &middle);
        return;
    }

    if ((value1 & TurnGiven) == 0)
    {
        return;
    }

    auto* physics = static_cast<RigidBody*>(GetGameNode(nodes, PhysicsBodyKind));
    if (physics == nullptr)
    {
        return;
    }

    physics->angularMomentum.x = physics->angularMomentum.x + value6;
    physics->angularMomentum.y = physics->angularMomentum.y + value7;
    physics->angularMomentum.z = physics->angularMomentum.z + value8;
    physics->bodyFlags |= RigidBody::FlagMoved;
}

// The pull of the awake focus's rigid body's magnet on the node's position, over the time since the node's last update, as a
// force on the node's rigid body at its middle; when the focus is attached to an instance whose node has a rigid body, half of it
// at the middle of that one's middle and the node's position, and the other half the other way on that body
void MagnetPullToFocusCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr)
    {
        return;
    }

    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    ObjectNode* focusNode = PacketNodeOf(focus);
    if (focusNode == nullptr)
    {
        return;
    }

    ObjectRigidBody* magnet = focusNode->RigidBody();
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    Vector4 pull = {0.0f, 0.0f, 0.0f, 1.0f};
    MagnetPull(magnet, &position, &pull);
    f32 seconds = SecondsSinceUpdate(node, clock);
    pull.x = pull.x * seconds;
    pull.y = pull.y * seconds;
    pull.z = pull.z * seconds;
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
    ObjectRigidBody* holderBody = nullptr;
    if ((focus->flags & AttachedFlag) != 0)
    {
        // Retail doesn't check that the parent has an object node
        auto* holder = static_cast<ObjectNode*>(GetGameNode(&focus->parent->nodes, ObjectNodeKind));
        if (TakesPackets(holder))
        {
            holderBody = holder->rigidBody;
            if (holderBody != nullptr)
            {
                point = holder->unknown20;
                point.w = 1.0f;
            }
        }
    }

    if (holderBody == nullptr)
    {
        ApplyRigidBodyForce(body, &pull, &point);
        return;
    }

    point.x = (point.x + position.x) * 0.5f;
    point.y = (point.y + position.y) * 0.5f;
    point.z = (point.z + position.z) * 0.5f;
    pull.x = pull.x * 0.5f;
    pull.y = pull.y * 0.5f;
    pull.z = pull.z * 0.5f;
    ApplyRigidBodyForce(body, &pull, &point);
    pull.x = -pull.x;
    pull.y = -pull.y;
    pull.z = -pull.z;
    ApplyRigidBodyForce(holderBody, &pull, &point);
}

// The physics bodies of the awake instances (with nodes of kind 5, 64 at most) within the size of a point (the instance's
// position plus the offset turned from its space) pushed away from it: by value5 at their middle, or with the flag by value5 plus
// their distance times (value6 - value5) / size, 3 units above their middle
void MoveInstancesInBoxCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x40;
    constexpr u32 QueryKinds = 0x20;
    constexpr f32 Above = 3.0f;
    InstanceContext* instance = runner->agentNode->owner;
    ChunkData* chunk = instance->chunk;
    void* results[Most];
    InstanceRayHit query;
    query.results = results;
    query.count = 0;
    query.most = Most;
    query.distance = Rounded(1e30);
    // Retail keeps the stack's other bits (nothing reads them)
    query.bits = InstanceRayHit::BitAllWanted;
    query.wantedFlags = 0;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    Vector4 sphere = {offsetX, offsetY, offsetZ, size};
    SkipInQuery(&query, instance);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 centre = place->position;
    if (!(__builtin_fabsf(sphere.x) <= Epsilon && __builtin_fabsf(sphere.y) <= Epsilon && __builtin_fabsf(sphere.z) <= Epsilon))
    {
        f32 x = sphere.x;
        f32 y = sphere.y;
        f32 z = sphere.z;
        place = instance->place;
        RotateAndTranslate(place);
        const Vector4* rowX = RowOf(&place->matrix, 0);
        const Vector4* rowY = RowOf(&place->matrix, 1);
        const Vector4* rowZ = RowOf(&place->matrix, 2);
        centre.z = centre.z + ((rowX->z * x + rowY->z * y) + rowZ->z * z);
        centre.x = centre.x + ((rowX->x * x + rowY->x * y) + rowZ->x * z);
        centre.y = centre.y + ((rowX->y * x + rowY->y * y) + rowZ->y * z);
    }

    sphere.x = centre.x;
    sphere.y = centre.y;
    sphere.z = centre.z;
    u32 found = ChunkInstancesInSphere(chunk, &sphere, QueryKinds, &query, 0);
    bool byDistance = (flags & 0xFF) != 0;
    for (u16 index = 0; index < found; index++)
    {
        auto* other = static_cast<InstanceContext*>(results[index]);
        auto* physics = static_cast<RigidBody*>(GetGameNode(&other->nodes, PhysicsBodyKind));
        if (physics == nullptr)
        {
            continue;
        }

        ObjectPlace* otherPlace = other->place;
        otherPlace->SyncPosition();
        Vector4 away = otherPlace->position;
        away.x = away.x - centre.x;
        away.y = away.y - centre.y;
        away.z = away.z - centre.z;
        Vector4 impulse;
        Vector4 point;
        if (byDistance)
        {
            f32 perDistance = (value6 - value5) / size;
            Vector4 way = away;
            f32 inverse = InverseLength(&way, LengthEpsilon);
            way.x = way.x * inverse;
            way.y = way.y * inverse;
            way.z = way.z * inverse;
            impulse = {way.x * value5 + away.x * perDistance, way.y * value5 + away.y * perDistance,
                       way.z * value5 + away.z * perDistance, 1.0f};
            point = {0.0f, Above, 0.0f, 1.0f};
        }
        else
        {
            f32 inverse = InverseLength(&away, LengthEpsilon);
            impulse = {away.x * inverse * value5, away.y * inverse * value5, away.z * inverse * value5, 1.0f};
            point = {0.0f, 0.0f, 0.0f, 1.0f};
        }

        physics->ApplyImpulse(&impulse, &point);
    }
}

ObjectRigidBody* ConstructRigidBody(void* memory, ObjectNode* node)
{
    auto* body = static_cast<ObjectRigidBody*>(memory);
    body->node = node;
    body->bits90 = (body->bits90 & ~MadeCleared90) | MadeSet90;
    body->bits88 = (body->bits88 & MadeKept) | MadeBytes;
    body->cache = nullptr;
    body->physicsBody = nullptr;
    body->drag = 1.0f;
    body->friction = 1.0f;
    body->restitution = 0.0f;
    body->unknownBC = 1.0f;
    body->unknownC4 = 0.0f;
    body->unknownC8 = 0.0f;
    body->unknownA0 = 1.0f;
    body->contactNormal = {0.0f, 1.0f, 0.0f, 1.0f};
    body->unknownCC = node->motion->velocity.y;
    body->secondIndex = NoSecondIndex;
    body->firstIndex = NoFirstIndex;
    body->chunkBodies = nullptr;
    body->object = nullptr;
    body->rideMovement = nullptr;
    body->sizes[1] = 0.0f;
    body->sizes[0] = 0.0f;
    return body;
}

// Back in action: its physics body made for a kind of motion of the physics, else it put back in the first list (even with no
// kind, and whether or not it's still in it)
void ActivateRigidBody(ObjectRigidBody* body)
{
    if ((body->bits90 & OutOfAction) == 0)
    {
        return;
    }

    ChunkRigidBodies* bodies = ChunkRigidBodiesOf(body->node->owner->chunk);
    body->bits90 &= ~OutOfAction;
    u32 kind = MotionKind(body);
    if (kind >= PhysicsKinds)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, kind);
        }
    }
    else
    {
        PutInFirstList(bodies, body);
        body->bits88 |= InFirstList;
    }

    // Retail bug: it goes back into the second list only while it's still in it (a second entry then), so one put out of action
    // (FUN_00254088 takes it out) never does
    if (body->secondIndex != NoSecondIndex)
    {
        PutInSecondList(bodies, body);
    }
}

// The size kept, and the physics body's mass and size made again by its kind of collisions: a sphere's sides the size, those of
// hulls 10 and 11 the instance's own box times it (BodyMass's mass)
void SetRigidBodySize(f32 size, ObjectRigidBody* body)
{
    body->size = size;
    body->bits88 |= SizeGiven;
    if (body->physicsBody == nullptr)
    {
        return;
    }

    f32 mass = BodyMass(body->node);
    u32 kind = CollisionKind(body);
    if (kind == SphereKind)
    {
        body->physicsBody->SetMassAndSize(mass, size, size, size);
    }
    else if (kind >= PhysicsKinds && kind < BoxKindsEnd)
    {
        Vector4 sides = SidesOf(&body->node->owner->collision.ownBox);
        body->physicsBody->SetMassAndSize(mass, sides.x * size, sides.y * size, sides.z * size);
    }
}

// The kind of motion: none takes the body out of the first list (its physics body let go of unless its collisions' kind is of the
// physics), one of the physics makes the physics body (out of the first list), another puts it in the first list (the physics
// body let go of unless its collisions' kind is of the physics)
void ListRigidBodyFirst(ObjectRigidBody* body, u32 kind)
{
    if (kind == 0)
    {
        StopRigidBodyMotion(body);
        if (CollisionKind(body) < PhysicsKinds)
        {
            ReleasePhysicsBody(body);
        }

        return;
    }

    ChunkRigidBodies* bodies = ChunkRigidBodiesOf(body->node->owner->chunk);
    SetMotionKind(body, kind);
    u32 set = MotionKind(body);
    if (set >= PhysicsKinds)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, set);
        }

        TakeFromFirstList(bodies, body);
        body->bits88 &= ~InFirstList;
        return;
    }

    if ((body->bits88 & InFirstList) == 0)
    {
        PutInFirstList(bodies, body);
        body->bits88 |= InFirstList;
    }

    if (CollisionKind(body) < PhysicsKinds && body->physicsBody != nullptr)
    {
        ReleasePhysicsBody(body);
    }
}

// The kind of collisions: none takes the body out of the second list (its collision cache destroyed, its physics body let go of
// unless its motion's kind is of the physics), another puts it in the second list and makes the physics body for one of the
// physics, a collision cache (once) for another (the physics body let go of unless its motion's kind is of the physics)
void ListRigidBodySecond(ObjectRigidBody* body, u32 kind)
{
    if (kind == 0)
    {
        StopRigidBodyCollisions(body);
        if (MotionKind(body) < PhysicsKinds)
        {
            ReleasePhysicsBody(body);
        }

        return;
    }

    SetCollisionKind(body, kind);
    if (body->secondIndex == NoSecondIndex)
    {
        PutInSecondList(ChunkRigidBodiesOf(body->node->owner->chunk), body);
    }

    if (body->cache == nullptr && CollisionKind(body) < PhysicsKinds)
    {
        CollisionCache* cache = ConstructCollisionCache(static_cast<CollisionCache*>(MemoryAllocate(sizeof(CollisionCache))),
                                                        body->node->owner, CacheMask);
        body->cache = cache;
        cache->margin = 1.0f;
    }

    u32 set = CollisionKind(body);
    if (set >= PhysicsKinds)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, set);
        }

        return;
    }

    if (MotionKind(body) < PhysicsKinds && body->physicsBody != nullptr)
    {
        ReleasePhysicsBody(body);
    }
}

// A sphere of the node's roll radius for 9 (sides of 1 and BodyMass's mass), a body of hulls for the others, placed where the
// instance is, with a restitution of 0.3, a friction of 0.8 and collisions with the instances that have nodes of the kinds of
// g_PhysicsBodyKinds, 4 and 5; it places the instance, which is told of its collisions
void MakePhysicsBody(ObjectRigidBody* body, u32 kind)
{
    constexpr u32 CollisionMaskBits = 0x30;
    bool sphere = kind == SphereKind;
    DynamicBody* physics = AddPhysicsBody(g_PhysicsWorld, body->node->owner, sphere);
    body->physicsBody = physics;
    physics->MoveTo(body->node->owner, 0);
    f32 mass = BodyMass(body->node);
    if (sphere)
    {
        body->physicsBody->SetMassAndSize(mass, 1.0f, 1.0f, 1.0f);
    }

    // Retail works out twice the sides of the instance's own box for hulls 10 and 11 and never uses them: they keep the mass and
    // size they were made with
    body->physicsBody->SetRestitution(Rounded(0.3));
    body->physicsBody->SetFriction(Rounded(0.8));
    body->physicsBody->bits |= DynamicBody::BitPlacesInstance;
    body->physicsBody->collisionMask = g_PhysicsBodyKinds | CollisionMaskBits;
    if (sphere)
    {
        auto* ball = static_cast<SphereBody*>(body->physicsBody);
        ball->ellipsoid = 0;
        ball->radius = body->node->rollRadius;
    }

    body->bits88 &= ~LaunchMask;
    body->node->owner->flags |= ReferencedObject::FlagPhysicsBody;
}

// By the kind of motion: 1-5 and 8 the node's velocity takes it, 9-11 the physics body (at the point in the instance's space)
void PushRigidBody(ObjectRigidBody* body, const Vector4* impulse, const Vector4* point)
{
    switch (MotionKind(body))
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
    {
        Vector4& velocity = body->node->motion->velocity;
        velocity.x = velocity.x + impulse->x;
        velocity.y = velocity.y + impulse->y;
        velocity.z = velocity.z + impulse->z;
        break;
    }
    case 9:
    case 10:
    case 11:
    {
        Vector4 local = PointInPlace(body->node->owner->place, point);
        body->physicsBody->ApplyImpulse(impulse, &local);
        break;
    }
    default:
        break;
    }
}

// As PushRigidBody, the node's velocity taking the force times the seconds since the node's last update and the physics body the
// force
void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point)
{
    switch (MotionKind(body))
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
    {
        Vector4 push = *force;
        TimeClock* clock = GetContextClock(body->node->owner);
        f32 seconds = SecondsSinceUpdate(body->node, clock);
        push.x = push.x * seconds;
        push.y = push.y * seconds;
        push.z = push.z * seconds;
        Vector4& velocity = body->node->motion->velocity;
        velocity.x = velocity.x + push.x;
        velocity.y = velocity.y + push.y;
        velocity.z = velocity.z + push.z;
        break;
    }
    case 9:
    case 10:
    case 11:
    {
        Vector4 local = PointInPlace(body->node->owner->place, point);
        body->physicsBody->AddLocalForce(force, &local);
        break;
    }
    default:
        break;
    }
}

// The second physics size along the magnet's way (0 from the body to the position, 1 its z axis, others none) made a unit vector;
// none for its strengths 1 and 2
void MagnetPull(ObjectRigidBody* body, const Vector4* position, Vector4* pull)
{
    Vector4 none = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 way = {0.0f, 0.0f, 0.0f, 1.0f};
    ObjectPlace* place = body->node->owner->place;
    place->SyncPosition();
    Vector4 here = place->position;
    switch (body->bits90 >> WayShift & 3)
    {
    case 0:
        way = {position->x - here.x, position->y - here.y, position->z - here.z, 1.0f};
        break;
    case 1:
        place = body->node->owner->place;
        RotateAndTranslate(place);
        way = *RowOf(&place->matrix, 2);
        break;
    default:
        break;
    }

    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    u32 strength = body->bits90 >> StrengthShift & 3;
    if (strength == 1 || strength == 2)
    {
        *pull = none;
        return;
    }

    f32 size = body->sizes[1];
    *pull = {way.x * size, way.y * size, way.z * size, 1.0f};
}

void DestroyRigidBody(ObjectRigidBody* body, u32 destroyFlags)
{
    StopRigidBodyCollisions(body);
    StopRigidBodyMotion(body);
    ReleasePhysicsBody(body);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(body);
    }
}

// Out of action (once: back with ActivateRigidBody), out of its chunk's second list
void DeactivateRigidBody(ObjectRigidBody* body)
{
    if ((body->bits90 & OutOfAction) != 0)
    {
        return;
    }

    body->bits90 |= OutOfAction;
    // Retail bug: it stays in the first list, only its index forgotten (a destroyed body is left in it, an activated one twice)
    body->firstIndex = NoFirstIndex;
    body->bits88 &= ~InFirstList;
    if (body->secondIndex != NoSecondIndex)
    {
        TakeFromSecondList(ChunkRigidBodiesOf(body->node->owner->chunk), body);
    }
}

void ForgetFirstIndex(ObjectRigidBody* body)
{
    body->bits88 &= ~InFirstList;
    body->firstIndex = NoFirstIndex;
    if (body->secondIndex == NoSecondIndex)
    {
        body->chunkBodies = nullptr;
    }
}

void ForgetSecondIndex(ObjectRigidBody* body)
{
    body->secondIndex = NoSecondIndex;
    if (body->firstIndex == NoFirstIndex)
    {
        body->chunkBodies = nullptr;
    }
}

void StopRigidBody(ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->Stop();
    }
}

void SetRigidBodyGravity(f32 gravity, ObjectRigidBody* body)
{
    body->gravity = gravity;
    if (gravity == 0.0f)
    {
        ClearRigidBodyGravity(body);
        return;
    }

    body->bits88 |= Falls;
    body->node->flags |= ObjectNode::FlagLevel;
}

void ClearRigidBodyGravity(ObjectRigidBody* body)
{
    body->bits88 &= ~Falls;
    body->node->flags &= ~ObjectNode::FlagLevel;
}

void SetRigidBodyRestitution(f32 value, ObjectRigidBody* body)
{
    body->restitution = value;
    body->bits88 |= OwnRestitution;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetRestitution(value);
    }
}

void SetRigidBodyDrag(f32 value, ObjectRigidBody* body)
{
    body->drag = value;
    body->bits88 |= Dragged;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->drag = value;
    }
}

void SetRigidBodyLengthDrag(f32 value, ObjectRigidBody* body)
{
    body->lengthDrag = value;
    body->bits88 |= OwnLengthDrag;
    if (body->physicsBody != nullptr)
    {
        // Retail bug: the physics body's length drag is made the rigid body's drag, not its length drag
        body->physicsBody->lengthDrag = body->drag;
    }
}

void SetRigidBodyFriction(f32 value, ObjectRigidBody* body)
{
    body->friction = value;
    body->bits88 |= Gripped;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetFriction(value);
    }
}

void SetRigidBodySpinFriction(f32 value, ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetSpinAndRollFriction(value, value);
    }
}

// Its physics body (which the callers make first) given the mass, sized by the instance's own box
void SetRigidBodyMass(f32 mass, ObjectRigidBody* body)
{
    Vector4 sides = SidesOf(&body->node->owner->collision.ownBox);
    body->physicsBody->SetMassAndSize(mass, sides.x, sides.y, sides.z);
}

void StopRigidBodyMotion(ObjectRigidBody* body)
{
    if (body->firstIndex != NoFirstIndex)
    {
        ChunkData* chunk = body->node->owner->chunk;
        if (chunk != nullptr)
        {
            ChunkRigidBodies* bodies = ChunkRigidBodiesOf(chunk);
            if (bodies != nullptr)
            {
                TakeFromFirstList(bodies, body);
            }
        }
    }

    body->bits88 &= ~InFirstList & ~(KindMask << MotionKindShift);
}

void StopRigidBodyCollisions(ObjectRigidBody* body)
{
    if (body->cache != nullptr)
    {
        DestroyCollisionCache(body->cache, DestroyAndFree);
        body->cache = nullptr;
    }

    if (body->secondIndex != NoSecondIndex)
    {
        ChunkData* chunk = body->node->owner->chunk;
        if (chunk != nullptr)
        {
            ChunkRigidBodies* bodies = ChunkRigidBodiesOf(chunk);
            if (bodies != nullptr)
            {
                TakeFromSecondList(bodies, body);
            }
        }
    }

    body->bits88 &= ~(KindMask << CollisionKindShift);
}

void ReleasePhysicsBody(ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        RemovePhysicsBody(g_PhysicsWorld, body->physicsBody);
        body->physicsBody = nullptr;
    }

    if (CollisionKind(body) >= PhysicsKinds)
    {
        SetCollisionKind(body, PlainKind);
    }

    if (MotionKind(body) >= PhysicsKinds)
    {
        SetMotionKind(body, PlainKind);
    }

    ObjectNode* node = body->node;
    if (node->motionBlock == nullptr)
    {
        node->owner->flags &= ~ReferencedObject::FlagPhysicsBody;
    }
}

void RevertColliderMotion(ObjectRigidBody* body)
{
    body->bits88 &= ~Moving & ~StopsAtRest;
}

void SetRigidBodyCenterOfMass(ObjectRigidBody* body, const Vector4* center)
{
    DynamicBody* physics = body->physicsBody;
    if (physics != nullptr)
    {
        CallVirtual<void>(physics, physics->vtable, CenterOfMassSlot, center);
    }
}

void SetRigidBodyTurn(f32 radians, ObjectRigidBody* body)
{
    ObjectNode* node = body->node;
    SetStoredPlace(node, node->owner->place);
    body->unknownC4 = radians;
    body->unknownC0 = 0.0f;
    body->bits88 |= Turning;
}

void ForgetRigidBodyContacts(ObjectRigidBody* body)
{
    u64 touched = (body->bits88 & (TouchingInstance | TouchingWorld)) != 0 ? TouchedBefore : 0;
    body->bits90 = ((body->bits90 & ~TouchedBefore) | touched) & ~OnGround & ~AgainstWall;
    body->object = nullptr;
    body->bits88 &= ~TouchingInstance & ~TouchingWorld & ~Touching;
    body->rideMovement = nullptr;
    body->node->flags &= ~ObjectNode::FlagRiding;
}
