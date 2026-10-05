#pragma once

#include "common.h"
#include "game/instances.h"
#include "game/math.h"

struct CollisionCache;
struct CollisionHull;
struct CollisionSurface;
struct RigidBody;

// What a rigid body's constraint holds: its turns to the hinge's axis, its position at a point, on a line or on a plane (one of
// the three at a time), its turns within the limit
union BodyConstraintFlags
{
    u32 value;
    struct
    {
        u32 hinge : 1;
        u32 unused1 : 3;
        u32 fixed : 1;
        u32 onLine : 1;
        u32 onPlane : 1;
        u32 unused7 : 1;
        u32 limited : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(BodyConstraintFlags, 4);

// What a rigid body is held to (0xC0 bytes, the body's own): turns only about an axis (the hinge: its axis in the body's space
// and in the world's, and the two rotations spanning the turns about it, a rotation taken to the nearest of them), turns kept
// within an angle (radians) of a rotation, and its position held at a point, on a line (a point and a unit direction) or on a
// plane. Velocity and momentum are held the same way (no motion, along the line, along the plane)
struct BodyConstraint
{
    BodyConstraintFlags flags;
    u8 unused04[0xC];
    Vector4 hingeLocalAxis;
    Vector4 hingeAxis;
    f32 limit;
    u8 unused34[0xC];
    Vector4 hinge[2];
    Vector4 limitCenter;
    Vector4 fixedPosition;
    Vector4 linePoint;
    Vector4 lineDirection;
    Vector4 plane;
    RigidBody* body;
    u8 unusedB4[0xC];
};
CHECK_OFFSET(BodyConstraint, hinge, 0x40);
CHECK_OFFSET(BodyConstraint, body, 0xB0);
CHECK_SIZE(BodyConstraint, 0xC0);

// A rigid body's flags: its velocity is out of date (UpdateState makes it again from the momentum), and so are its rotation's
// matrices and its angular velocity; it touched a triangle of its chunk this frame, the world's frame doesn't step it, it touched
// another body or an instance's hulls this frame; the characters push it while either of its two pushable bits is set (a
// script's and its trajectory's motion block's). A script command switches bits 12, 13, 15 and 16, which nothing reads
union RigidBodyFlags
{
    u32 value;
    struct
    {
        u32 velocityStale : 1;
        u32 rotationStale : 1;
        u32 touchedWorld : 1;
        u32 leftOut : 1;
        u32 touchedBody : 1;
        u32 pushable : 2;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(RigidBodyFlags, 4);

// A rigid body (0x2E0 bytes, a node of kind 5, vtable D_00305608: 2 the destructor, 4 its instance doesn't change chunks, 5 its
// kind, 8 no update, 10 its class): a box's mass and inertia (the inverse inertia the steps use, which a step resets to the one
// kept), the most it moves and turns, its momentum, rotation and angular momentum, and what they make (its place's matrix and
// its inverse, the rotation's rate of change, the angular velocity, the velocity and the last step's, the world's inverse
// inertia), what pushes and turns it (kept, and only for a step), how far the impulses of a step pushed it and its constraint
struct RigidBody : GameNode
{
    // The movement node's class too
    static constexpr u32 ClassId = MovementNode::ClassId;

    RigidBodyFlags bodyFlags;
    f32 mass;
    f32 inverseMass;
    f32 maxSpeed;
    f32 maxSpin;
    u8 unused2C[4];
    Vector4 inertia;
    Vector4 inverseInertia;
    Vector4 restInverseInertia;
    // What the impulses it hands the object nodes of the instances it touches are scaled by (1, or its trajectory's motion
    // block's)
    f32 knockScale;
    // The steps a frame's time is cut into
    s32 substeps;
    u8 unused68[8];
    Vector4 momentum;
    Vector4 rotation;
    Vector4 angularMomentum;
    // The instance whose hulls it last touched, and one whose hulls it doesn't answer
    InstanceContext* lastTouched;
    InstanceContext* ignoredInstance;
    u8 unusedA8[8];
    Matrix4x4 matrix;
    Matrix4x4 inverseMatrix;
    Vector4 rotationRate;
    Vector4 angularVelocity;
    Vector4 velocity;
    Vector4 lastVelocity;
    Matrix4x4 worldInverseInertia;
    // The turns of the impulses, each the last one's 0.7 and the new one's 0.3
    Vector4 recentTurn;
    Vector4 force;
    Vector4 torque;
    Vector4 stepForce;
    Vector4 stepTorque;
    // Made the origin by the constructor, never read
    Vector4 unused200;
    // The water level the hull bodies float at (from the deepest point they touched)
    f32 waterLevel;
    f32 impulseTotal;
    u8 unused218[8];
    BodyConstraint constraint;

    static RigidBody* Construct(RigidBody* body) RETAIL(FUN_002898e8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00291ab0);
    u32 CanChangeChunk() RETAIL(FUN_002918a0);
    u32 Kind() RETAIL(FUN_00291878);
    u32 Update() RETAIL(FUN_00291aa8);
    u32 GetClassId() RETAIL(FUN_00291880);

    // The velocity (no faster than the most) from the momentum when it's out of date, and the rotation's matrices, the world's
    // inverse inertia, the angular velocity (no faster than the most) and the rotation's rate of change when they are
    void UpdateState() RETAIL(FUN_00289368);
    // The constraint applied to the position, the momentum, the velocity and the rotation (and the angular momentum and
    // velocity taken onto the hinge's axis)
    void ApplyConstraint() RETAIL(FUN_002897b0);
    // A box's mass and inertia (each side at least a third of the others), its state made again
    void SetMassAndSize(f32 mass, f32 x, f32 y, f32 z) RETAIL_N32(FUN_00289e88);
    // The world's inertia, the velocity of a point of the world on it, the angular velocity set (the angular momentum from it)
    void WorldInertia(Matrix4x4* out) RETAIL(FUN_00289b90);
    void VelocityAt(const Vector4* point, Vector4* out) RETAIL(FUN_00289c60);
    void SetAngularVelocity(const Vector4* angularVelocity) RETAIL(FUN_00289d80);
    // The momentum and the angular momentum made shorter by a share of an amount (the velocity marked out of date when the angular
    // one isn't tiny)
    void Damp(f32 amount, f32 share) RETAIL_N32(FUN_00289fc0);
    // A step of a time: moved by its velocity (at most half a unit), pushed and turned by its forces, turned by its rate of
    // change, its state made again and the step's forces cleared
    void Step(f32 seconds) RETAIL_N32(FUN_0028a160);
    // An impulse at a point of the body's own (its turn kept in the recent turn and taken off the angular momentum)
    void ApplyImpulse(const Vector4* impulse, const Vector4* point) RETAIL(FUN_0028a518);
    // A force at a point, kept and for a step only (taken into the body's space through its matrices as they were)
    void AddForce(const Vector4* force, const Vector4* point) RETAIL(FUN_0028a760);
    void AddStepForce(const Vector4* force, const Vector4* point) RETAIL(FUN_0028a8a8);
    // A spring from a point of the body's own to one of the world's (its stretch past its length times the stiffness, less the
    // point's velocity along it times the damping; nothing when it pulls only and isn't stretched), and one pulling up or down
    // and across apart (the height difference times the first value less the vertical velocity times the second, the distance
    // across times the first and third less the velocity across times the second and third), added as forces
    void Spring(f32 length, f32 stiffness, f32 damping, const Vector4* target, const Vector4* localPoint, u32 pullOnly)
        RETAIL_N32(FUN_0028a9f0);
    void LevelSpring(f32 stiffness, f32 damping, f32 across, const Vector4* target, const Vector4* localPoint)
        RETAIL_N32(FUN_0028ad48);
    // A force of the world's at a point of the body's own added to its kept forces (taken into the body's space before its state
    // is made again)
    void AddLocalForce(const Vector4* force, const Vector4* localPoint) RETAIL(FUN_00292108);
    // How much a unit impulse along a normal at a point of the body's own changes the point's velocity along it
    f32 ImpulseResponse(const Vector4* localPoint, const Vector4* normal) RETAIL(FUN_0028b0b8);
    // The impulse along a normal that stops a point of the body's own going into a surface (the restitution, softened by the
    // softness for slow contacts), into a moving point, and into a point of another body: none when they part
    f32 PointImpulse(f32 restitution, f32 softness, const Vector4* localPoint, const Vector4* normal) RETAIL_N32(FUN_0028b228);
    f32 TwoBodyImpulse(f32 restitution, f32 softness, const Vector4* localPoint, RigidBody* other, const Vector4* otherLocalPoint,
                       const Vector4* normal) RETAIL_N32(FUN_0028b3c0);
    f32 MovingPointImpulse(f32 restitution, f32 softness, const Vector4* localPoint, const Vector4* pointVelocity,
                           const Vector4* normal) RETAIL_N32(FUN_0028b690);
    // The angular momentum pushed toward an angular velocity, about the impulse's direction (at most the impulse's length times
    // the first share) and across it (times the second)
    void AngularFriction(f32 along, f32 across, const Vector4* impulse, const Vector4* target) RETAIL_N32(FUN_0028ba78);

    // Left out of the world's frames while a trajectory carries its instance: its momentums stopped (their w kept), the instance
    // it touched forgotten; and back in them (the trajectory started again from where the instance is when its object node takes
    // packets), made up to date with its kept forces cleared
    void StartRide() RETAIL(FUN_00291ad8);
    void ReleaseRide() RETAIL(FUN_00291b60);
    // No velocity and no angular velocity
    void Stop() RETAIL(FUN_00291bf0);
    // Made up to date, its kept forces cleared
    void ClearForces() RETAIL(FUN_00291c40);
    // Put at a matrix's position and turned as the matrix is (both out of date), only moved to a position or by an offset (its
    // inverse matrix made again), only turned as a matrix is
    void SetMatrix(const Matrix4x4* placed) RETAIL(FUN_00291cb8);
    void SetPosition(const Vector4* position) RETAIL(FUN_00291d08);
    void MoveBy(const Vector4* offset) RETAIL(FUN_00291d58);
    void SetTurn(const Matrix4x4* turned) RETAIL(FUN_00291de0);
    // Its velocity (the momentum made from it)
    void SetVelocity(const Vector4* moving) RETAIL(FUN_00291e28);
    // Its momentums slowed by a share for every 60th of a second of a time (none left past all of it), or only its momentum
    // along its instance's x, y or z axis
    void Slow(f32 share, f32 seconds) RETAIL_N32(FUN_00291ea8);
    void SlowAlongX(f32 share, f32 seconds) RETAIL_N32(FUN_00291f38);
    void SlowAlongY(f32 share, f32 seconds) RETAIL_N32(FUN_00291fd0);
    void SlowAlongZ(f32 share, f32 seconds) RETAIL_N32(FUN_00292068);
};
CHECK_OFFSET(RigidBody, bodyFlags, 0x18);
CHECK_OFFSET(RigidBody, momentum, 0x70);
CHECK_OFFSET(RigidBody, matrix, 0xB0);
CHECK_OFFSET(RigidBody, worldInverseInertia, 0x170);
CHECK_OFFSET(RigidBody, waterLevel, 0x210);
CHECK_OFFSET(RigidBody, constraint, 0x220);
CHECK_SIZE(RigidBody, 0x2E0);

extern "C"
{
    extern const GccVTableEntry g_RigidBodyVTable[] RETAIL(D_00305608);

    // The constraint held: a position, a velocity (or momentum), a rotation (the hinge's nearest, then within the limit)
    void ConstrainPosition(BodyConstraint* constraint, Vector4* position) RETAIL(FUN_00288c88);
    void ConstrainVelocity(BodyConstraint* constraint, Vector4* velocity) RETAIL(FUN_00288e08);
    void ConstrainRotation(BodyConstraint* constraint, Vector4* rotation) RETAIL(FUN_00288f50);
    void LimitRotation(BodyConstraint* constraint, Vector4* rotation) RETAIL(FUN_00289190);
    // The hinge set from its axis in the body's space and in the world's; from the one in the body's space
    void SetHinge(BodyConstraint* constraint, const Vector4* localAxis, const Vector4* axis) RETAIL(FUN_00288950);
    void SetLocalHinge(BodyConstraint* constraint, const Vector4* localAxis) RETAIL(FUN_002918c0);
    // Turns kept within an angle of the body's rotation
    void SetRotationLimit(f32 limit, BodyConstraint* constraint) RETAIL_N32(FUN_00291910);
    // The position held where the body is, at a point, on a line through the body along a direction, on a plane through it with
    // a normal and on a plane
    void FixHere(BodyConstraint* constraint) RETAIL(FUN_00291960);
    void FixAt(BodyConstraint* constraint, const Vector4* position) RETAIL(FUN_00291980);
    void KeepOnLine(BodyConstraint* constraint, const Vector4* direction) RETAIL(FUN_002919a0);
    void KeepOnPlaneThrough(BodyConstraint* constraint, const Vector4* normal) RETAIL(FUN_00291a38);
    void KeepOnPlane(BodyConstraint* constraint, const Vector4* plane) RETAIL(FUN_00291a78);
}


// A body's bits: it puts its instance where it is (only its position with placesPositionOnly), it doesn't collide with the
// world, its landings are told, and this frame a landing was told, a hard one was and a scrape was (the world's step clears
// those three). Retail reads and writes the word as 64 bits, with the instance after it
union DynamicBodyBits
{
    // For the code that writes the word back with the instance (as retail does)
    enum Mask : u32
    {
        PlacesInstance = 0x1,
    };

    u32 value;
    struct
    {
        u32 placesInstance : 1;
        u32 noCollisions : 1;
        u32 tellsLanding : 1;
        u32 landed : 1;
        u32 landedHard : 1;
        u32 scraped : 1;
        u32 placesPositionOnly : 1;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(DynamicBodyBits, 4);

// A body that collides (0x380 bytes, vtable D_00305570 over the rigid body's: 2 the destructor, 3 given its instance (kept here
// too), 4 whether its instance may change chunks, 11 floated on water (nothing), 12-16 its kind's (12 and 13 a step's collisions
// with the world and with the instances around it, 14 a collision with another body, 15 whether it's a sphere, 16 whether it may
// touch a box), 17 its centre of mass moved): bits (placesInstance and tellsLanding set, noCollisions and placesPositionOnly clear
// at first), its instance, its centre of mass in its own space, its collision cache, how it bounces and rubs (scaled by the
// surface's values: restitution, softness, friction, and friction against spinning about the contact's normal and against
// rolling), the slot it has in the world and the collision mask its contacts are found with
struct DynamicBody : RigidBody
{
    enum Slots : u32
    {
        FloatSlot = 11,
        CollideWithWorldSlot = 12,
        CollideWithInstancesSlot = 13,
        CollideSlot = 14,
        IsSphereSlot = 15,
        TouchesBoxSlot = 16,
        SetCenterOfMassSlot = 17,
    };

    DynamicBodyBits bits;
    InstanceContext* instance;
    u8 unused2E8[8];
    Vector4 centerOfMass;
    CollisionCache* cache;
    // Frames without a hard landing or a scrape
    u8 restingFrames;
    u8 unused305[3];
    f32 restitution;
    f32 softness;
    f32 friction;
    f32 spinFriction;
    f32 rollFriction;
    // Drags: scaling the momentums down each step and cutting their lengths down; how much water lifts it (none when negative:
    // then the first drag works all the time, else only while a step force pushes)
    f32 drag;
    f32 lengthDrag;
    f32 buoyancy;
    s16 slot;
    u8 unused32A[2];
    // 1 when it's made, never read
    f32 unused32C;
    u32 collisionMask;
    u8 unused334[0xC];
    // The frame's pushes out of things: the limited ones, all of them (halved) and their length
    Vector4 limitedPushes;
    Vector4 pushes;
    f32 pushedLength;
    // Told of every triangle it touches (with the argument), and of every instance it touches (with the hull's index when it's
    // a sphere touching hulls, and the argument)
    void (*contactCallback)(const struct CollisionHit* hit, void* argument);
    void* contactArgument;
    void (*touchCallback)(InstanceContext* other, u32 hull, void* argument);
    void* touchArgument;
    u8 unused374[0xC];

    static DynamicBody* Construct(DynamicBody* body) RETAIL(FUN_0028be00);
    // Whether its instance may go through a link into another chunk: when the linked chunk is loaded it goes along (its momentums,
    // rotation and position taken through the link), else it's put back where its instance is and slowed down
    u32 CanChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_0028def0);
    // Out of the world, its collision cache freed
    void Destroy(u32 destroyFlags) RETAIL(FUN_00292300);
    void SetOwner(InstanceContext* owner) RETAIL(FUN_00292240);
    // Floated at a water level: not the spheres
    void Float() RETAIL(FUN_00292238);
    // Moved so that its centre of mass is at a new point of its own space
    void SetCenterOfMass(const Vector4* center) RETAIL(FUN_0028bf48);
    // A contact answered: an impulse along the normal, one against the sliding (at most the first one times the friction), and
    // the spinning and rolling rubbed off. With a surface (its values scaling the body's, the object node told of landings and
    // scrapes: the impulse it took, turned around, when wanted), with another body at a point of the world (both object nodes
    // told), with a point of the world that moves (spinning the body toward an angular velocity) and with an instance (moving
    // when it has a movement node, a surface otherwise; both object nodes told, and nothing more when the other's says so).
    // Whether it answered
    u32 SurfaceContact(const Vector4* localPoint, const Vector4* normal, CollisionSurface* surface, Vector4* impulseOut)
        RETAIL(FUN_0028c040);
    u32 BodyContact(const Vector4* point, const Vector4* normal, DynamicBody* other) RETAIL(FUN_0028c668);
    u32 MovingPointContact(const Vector4* point, const Vector4* normal, const Vector4* pointVelocity, const Vector4* spinTarget,
                           Vector4* impulseOut) RETAIL(FUN_0028cbb0);
    u32 InstanceContact(const Vector4* localPoint, const Vector4* normal, CollisionSurface* surface, InstanceContext* other)
        RETAIL(FUN_0028cf90);
    // Put where an instance is (its centre of mass where the instance's place puts it, turned as the place is), and when asked
    // moving and turning as the instance's movement node did over its last frame
    void MoveTo(InstanceContext* instance, u32 withMotion) RETAIL(FUN_0028d6e0);
    // After a frame: its instance put where its centre of mass is (only its position when its bits say so; the instance queued
    // when that moved it), and the object node's sound stopped once it has gone 5 frames (2 a sphere) without a hard landing
    // or a scrape
    void PlaceInstance() RETAIL(FUN_0028d490);
    // A step's damping: its momentums made shorter by the step's share of its length drag, then scaled down by its drag (times 70,
    // or 4 a sphere, when its buoyancy isn't negative, and then only while a step force pushes it)
    void StepDamping(f32 seconds) RETAIL_N32(FUN_0028da10);
    // Pushed out of something: half the push kept in the frame's pushes, and moved by it, or (when limited) by half of it
    // again while the frame's pushes stay within 0.2, by what's left of 0.2 then
    void PushOut(const Vector4* push, u32 limited) RETAIL(FUN_0028db88);
    void LimitedPush(const Vector4* push) RETAIL(FUN_0028dca8);

    // Its collision cache, made when it has none (its instance's, the surfaces solid to the player's probes and to objects), and
    // the mask set
    CollisionCache* Cache() RETAIL(FUN_002922a0);
    void SetCacheMask(u32 mask) RETAIL(FUN_00292468);
    // How it bounces and rubs (the softness squared)
    void SetRestitution(f32 value) RETAIL_N32(FUN_00292378);
    void SetSoftness(f32 value) RETAIL_N32(FUN_00292380);
    void SetFriction(f32 value) RETAIL_N32(FUN_00292390);
    void SetSpinAndRollFriction(f32 spin, f32 roll) RETAIL_N32(FUN_00292398);
    // Moved so that its instance is at a position (its centre of mass added as it is), its inverse matrix made again
    void SetInstancePosition(const Vector4* position) RETAIL(FUN_002923b8);
};
CHECK_OFFSET(DynamicBody, centerOfMass, 0x2F0);
CHECK_OFFSET(DynamicBody, restitution, 0x308);
CHECK_OFFSET(DynamicBody, slot, 0x328);
CHECK_OFFSET(DynamicBody, collisionMask, 0x330);
CHECK_OFFSET(DynamicBody, touchCallback, 0x36C);
CHECK_SIZE(DynamicBody, 0x380);

// A sphere (0x3A0 bytes, vtable D_003054D8): its radius, and an ellipsoid's scales along its axes when it's one
struct SphereBody : DynamicBody
{
    f32 radius;
    f32 scaleX;
    f32 scaleY;
    f32 scaleZ;
    u8 ellipsoid;
    u8 unused391[0xF];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00292a68);
    // Collided with another body: a sphere (as ellipsoids when either is one) or a hull; each pair once (from its first body in
    // memory), but from either side when the other one is out of the world's steps
    u32 Collide(DynamicBody* other) RETAIL(FUN_00292ae0);
    // Collided with its chunk's triangles (none when it doesn't collide): solid ones push it out and are answered as surfaces,
    // others are water, which lifts it by how deep its centre is below the highest (times its mass and buoyancy, less a 0.3
    // of its rising speed)
    u32 CollideWithWorld() RETAIL(FUN_0028e140);
    // Collided with the instances around it (not its own children): bodies through their Collide, others' hulls answered
    void CollideWithInstances() RETAIL(FUN_0028efd8);
    // An instance's hulls it touches answered (but the ignored instance's) and pushed out of: whether it touched one
    u32 TouchHulls(InstanceContext* other) RETAIL(FUN_0028f428);
    u32 IsSphere() RETAIL(FUN_002924c0);
    u32 TouchesBox(const Box* box) RETAIL(FUN_002924c8);
    // An ellipsoid of three radii along its axes: its radius the largest, its scales each over it
    void SetEllipsoid(f32 x, f32 y, f32 z) RETAIL_N32(FUN_00292ba0);
};
CHECK_OFFSET(SphereBody, radius, 0x380);
CHECK_SIZE(SphereBody, 0x3A0);

// A body of hulls (0x390 bytes, vtable D_00305440): copies of its instance's collision hulls (moved by the centre of mass)
struct HullBody : DynamicBody
{
    CollisionHull* hulls;
    s32 hullCount;
    u8 unused388[8];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00292c28);
    u32 Collide(DynamicBody* other) RETAIL(FUN_00292d40);
    u32 IsSphere() RETAIL(FUN_002924d8);
    // Whether its instance's collision box meets a box
    u32 TouchesBox(const Box* box) RETAIL(FUN_00292de0);
    void SetCenterOfMass(const Vector4* center) RETAIL(FUN_00292d08);
    // Its hulls copied again from its instance's collision under the identity (its centre of mass taken off)
    void CopyHulls(ObjectCollision* collision) RETAIL(FUN_0028f590);
    // Collided with its chunk's triangles, a hull at a time (none when it doesn't collide): solid ones push it out and are
    // answered as surfaces, water sets the level it floats at (from the deepest point below its instance's box)
    u32 CollideWithWorld() RETAIL(FUN_0028f728);
    // Floated: every corner of its instance's box more than 0.1 below the water level lifted (by the depth times its mass and
    // buoyancy, less a 0.3 of the corner's rising speed)
    void Float(f32 waterLevel) RETAIL_N32(FUN_0028fb00);
    // Collided with the instances around its hulls (not its own children or what its collision is attached to): bodies through
    // their Collide, instances with hulls answered, water volumes floated on (the top of their box)
    void CollideWithInstances() RETAIL(FUN_0028fc50);
    // An instance's hulls its own go into answered (but the ignored instance's) and pushed out of, its instance placed again
    // before every one
    u32 TouchInstanceHulls(InstanceContext* other) RETAIL(FUN_0028fec0);
};
CHECK_OFFSET(HullBody, hulls, 0x380);
CHECK_SIZE(HullBody, 0x390);

// What the test of two ellipsoids works with (0x190 bytes): each one's radii, their inverses, squares and inverse squares, the
// products of the other two axes' inverse radii (the first) and squares (the second), all three's (inverse squares, squares),
// the direction between them in the first's unit sphere and its length squared, the point a step of the search finds, the
// cosines between the two's axes, and the two matrices (and sums) the search's polynomial is made of
struct EllipsoidPair
{
    f32 radii[3];
    f32 inverseRadii[3];
    f32 squares[3];
    f32 inverseSquares[3];
    f32 otherRadii[3];
    f32 otherInverseRadii[3];
    f32 otherSquares[3];
    f32 otherInverseSquares[3];
    f32 inverseProducts[3];
    f32 otherProducts[3];
    f32 inverseVolume;
    f32 otherVolume;
    f32 distanceSquared;
    u8 unused84[0xC];
    Vector4 direction;
    Vector4 point;
    Vector4 cosines[3];
    u8 unusedE0[0x10];
    f32 sum;
    u8 unusedF4[0xC];
    Matrix4x4 first;
    f32 otherSum;
    u8 unused144[0xC];
    Matrix4x4 second;
};
CHECK_OFFSET(EllipsoidPair, direction, 0x90);
CHECK_OFFSET(EllipsoidPair, cosines, 0xB0);
CHECK_OFFSET(EllipsoidPair, first, 0x100);
CHECK_OFFSET(EllipsoidPair, second, 0x150);
CHECK_SIZE(EllipsoidPair, 0x190);

// The world the bodies are stepped in (0x14 bytes, made by the game context): 200 slots of bodies, the free slots' indexes (a
// stack, the next one at the count of used slots) and the count
struct PhysicsWorld
{
    static constexpr s32 Slots = 200;

    DynamicBody** bodies;
    s32 capacity;
    s16* freeSlots;
    s32 freeCapacity;
    s32 used;
};
CHECK_SIZE(PhysicsWorld, 0x14);

extern "C"
{
    extern const GccVTableEntry g_DynamicBodyVTable[] RETAIL(D_00305570);
    extern const GccVTableEntry g_SphereBodyVTable[] RETAIL(D_003054D8);
    extern const GccVTableEntry g_HullBodyVTable[] RETAIL(D_00305440);
    extern PhysicsWorld* g_PhysicsWorld RETAIL(UnkStruct_0x14_HasSomeDataPTRs);

    PhysicsWorld* ConstructPhysicsWorld(PhysicsWorld* world) RETAIL(FUN_00292e00);
    // A body for an instance (a sphere of 1.3 sides and a radius of 0.5, or one of hulls) in the next free slot, a node of the
    // instance's
    DynamicBody* AddPhysicsBody(PhysicsWorld* world, InstanceContext* instance, u32 sphere) RETAIL(FUN_00290880);
    // A body out of its slot (its node taken out of its instance when the instance is in a chunk)
    void RemovePhysicsBody(PhysicsWorld* world, DynamicBody* body) RETAIL(FUN_00292e90);
    // A frame of every body whose instance is awake and whose clock runs (but those left out): each in its substeps of the
    // clock's last advance (a step, its collisions with instances and then with the world), then damped, made up to date, its
    // forces cleared and its instance placed
    void StepPhysicsWorld(PhysicsWorld* world) RETAIL(FUN_002909c8);
    // The velocity a point of a place moves at and the angular velocity the place turns at, from where it was before to where
    // it is now in a time
    void VelocitiesBetween(f32 seconds, const Matrix4x4* before, const Matrix4x4* now, const Vector4* point, Vector4* velocity,
                           Vector4* angularVelocity) RETAIL_N32(FUN_0028b878);
    // Collisions of two ellipsoids (when either is one), of two spheres, of a sphere and a hull body, of two hull bodies (every
    // pair of their hulls: both pushed, the second a half as far back)
    u32 CollideEllipsoids(SphereBody* body, SphereBody* other) RETAIL(FUN_0028ea70);
    u32 CollideSpheres(SphereBody* body, SphereBody* other) RETAIL(FUN_0028e798);
    u32 CollideSphereWithHulls(SphereBody* sphere, DynamicBody* other) RETAIL(FUN_0028edb0);
    u32 CollideHullBodies(HullBody* body, DynamicBody* other) RETAIL(FUN_00290320);
    // Whether a sphere under a matrix touches a hull under another: the push out of it, where it touches (in the sphere's space)
    // and the normal
    u32 SphereTouchesHull(SphereBody* sphere, const Matrix4x4* matrix, const Matrix4x4* hullMatrix, const CollisionHull* hull,
                          Vector4* push, Vector4* point, Vector4* normal) RETAIL(FUN_0028f158);
    // 1 over a vector's length (0 when it's within the epsilon of none), its length squared kept
    f32 InverseLengthKeepingSquare(const Vector4* vector, f32* lengthSquared) RETAIL(FUN_00291800);
    // A word the module's static constructor clears (nothing reads it), that constructor and the module's global constructor
    extern u32 g_RigidBodyStatic RETAIL(D_0030AA68);
    void InitRigidBodyStatics(u32 initialise, u32 priority) RETAIL(FUN_002917d0);
    void ConstructRigidBodyModule() RETAIL(FUN_00293320);
    // Whether two ellipsoids (radii along their matrices' axes, kept when none are given) meet: not when their largest radii
    // don't reach, else by the search (from the first's unit sphere): how far the first goes into the second and a point of the
    // first's surface toward it
    u32 EllipsoidsMeet(EllipsoidPair* pair, Vector4* separation, Vector4* other, const Matrix4x4* matrix, const Vector4* radii,
                       const Matrix4x4* otherMatrix, const Vector4* otherRadii) RETAIL(FUN_00290f28);
    // The search's parts: the products of the other axes, the matrices and sums from the two's axes, the polynomial's value at
    // a point of the way from one to the other (and the point found there; the minimum search takes it turned around), and
    // the search: twelve steps that turn back halved past the largest value, then the minimum search from there; apart when
    // the largest value times the distance squared goes past 1
    void EllipsoidProducts(EllipsoidPair* pair) RETAIL(FUN_00292500);
    void EllipsoidMatrices(EllipsoidPair* pair, const Matrix4x4* matrix, const Matrix4x4* otherMatrix) RETAIL(FUN_00292598);
    f32 EllipsoidValue(EllipsoidPair* pair, f32 along) RETAIL_N32(FUN_00290d38);
    f32 NegativeEllipsoidValue(EllipsoidPair* pair, f32 along) RETAIL_N32(FUN_002924e0);
    u32 EllipsoidsApart(EllipsoidPair* pair, Vector4* separation, Vector4* other, const Matrix4x4* matrix,
                        const Matrix4x4* otherMatrix, u32 noDirection) RETAIL(FUN_002913a8);
}
