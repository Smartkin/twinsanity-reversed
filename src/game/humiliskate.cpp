#include "game/vehicles.h"

#include "game/agents.h"
#include "game/attachments.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"
#include "retail/libc.h"

#include <cstddef>
#include <cstdint>

// The Humiliskate: the character skating on the other one used as a board. It has no body: four substeps a frame it steers,
// collides with the rails and the ground of its collision cache and with the instances around it, keeps its speed within its
// limits, turns its up toward the ground's, squashes its suspension, falls and does its tricks (a half spin or a flip off a
// ramp), and jumps. Then it sends both characters their animation events, lays its two skid trails and places both characters

EABI_EXPORT(FUN_00155490, &HumiliskateVehicle::Jump);
EABI_EXPORT(FUN_00155600, &HumiliskateVehicle::Steer);
EABI_EXPORT(FUN_00155978, &HumiliskateVehicle::Collide);
EABI_EXPORT(FUN_00156fd8, &HumiliskateVehicle::LimitSpeed);
EABI_EXPORT(FUN_00157210, &HumiliskateVehicle::GroundResponse);
EABI_EXPORT(FUN_00157a88, &HumiliskateVehicle::TouchedHull);
EABI_EXPORT(FUN_00157c98, &HumiliskateVehicle::CollideHulls);
EABI_EXPORT(FUN_00157f30, &HumiliskateVehicle::CollideInstances);
EABI_EXPORT(FUN_00158db8, &HumiliskateVehicle::Suspension);
EABI_EXPORT(FUN_00158ef8, &HumiliskateVehicle::SendEvents);
EABI_EXPORT(FUN_001593c0, &HumiliskateVehicle::Tricks);
EABI_EXPORT(FUN_00159730, &HumiliskateVehicle::Frame);

namespace
{
// The object node's part at 0x114 when its kind byte is 3: a character's sounds (the surface it's on, its landings,
// its leans)
struct CharacterSounds
{
    u8 kind;
};
}

extern "C"
{
    // A character's sounds told the surface it's on and its velocity, a landing's fall, a lean (how far across)
    void SetCharacterSurface(CharacterSounds* sounds, CollisionSurface* surface, const Vector4* velocity);
    void PlayCharacterSurfaceSound(CharacterSounds* sounds, f32 fall) RETAIL_N32(PlayCharacterSurfaceSound);
    void PlayCharacterSurfaceSound2(CharacterSounds* sounds, f32 lean) RETAIL_N32(PlayCharacterSurfaceSound2);
}


namespace
{
constexpr u8 CharacterSoundsKind = 3;

// The nodes: an instance's object node and attachments, a playable character's agent node
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
constexpr u32 CharacterNodeKind = 0xC;

// The agent's bump and the object node's collision
constexpr u32 BumpedSlot = 8;
constexpr u32 CollidedSlot = 28;

// The script events: riding (the character and the board both; 1 more leaning left, 2 right, 3 more riding backwards), riding
// crouched (the character; 1 more leaning right, 2 left), a trick landed, the half spin and the flip (both), grinding (3 more
// backwards), the jump and the crash (both), and none
constexpr u32 EventRiding = 0x56;
constexpr u32 EventLeft = 1;
constexpr u32 EventRight = 2;
constexpr u32 EventBackwards = 3;
constexpr u32 EventCrouched = 0x5C;
constexpr u32 EventCrouchedRight = 0x5D;
constexpr u32 EventCrouchedLeft = 0x5E;
constexpr u32 EventTrickLanded = 0x5F;
constexpr u32 EventSpin = 0x60;
constexpr u32 EventFlip = 0x61;
constexpr u32 EventGrinding = 0x62;
constexpr u32 EventGrindingBackwards = 0x63;
constexpr u32 EventJump = 0x6D;
constexpr u32 EventCrash = 0x6E;
constexpr u32 EventNone = 0x6F;

// The surfaces' bits: solid to the probes (its cache's and the ray down) and solid to the player (what pushes the board)
constexpr u32 SolidToProbes = 0x10;
constexpr u32 SolidToPlayer = 0x100000;

// The instances it hits: the kinds of nodes (kind 4's hulls, characters, crates, creatures, generic objects and pay gates), those
// with either flag, at most 20, and at most 19 attached to the characters left out with the other character
constexpr u32 HitNodeKinds = 0x5B010;
constexpr u32 HitFlags = ReferencedObject::FlagTriggerSignals | ReferencedObject::FlagSphereContact;
constexpr u16 MostHits = 20;
constexpr s32 MostLeftOut = 20;
constexpr f32 NoHit = Rounded(1e30);

constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 Pi = 0x1.921fb6p+1f;
constexpr f32 TwoPi = 0x1.921fb6p+2f;
constexpr f32 InversePi = 0x1.45f306p-2f;
constexpr f32 InverseTwoPi = 0x1.45f306p-3f;

// The board: its ellipsoid's radii, how far around it it looks, gravity, how far below its middle the riders stand, how fast it
// starts along the character's heading, and its top speeds
constexpr f32 BoardRadiusX = Rounded(0.6);
constexpr f32 BoardRadiusY = Rounded(0.6);
constexpr f32 BoardRadiusZ = Rounded(0.8);
constexpr f32 BoardReach = 1.0f;
constexpr f32 BoardGravity = 35.0f;
constexpr f32 RiderHeight = Rounded(0.4);
constexpr f32 StartSpeed = 10.0f;
constexpr f32 CrouchedTopSpeed = 35.0f;
constexpr f32 TopSpeed = 25.0f;

// A frame is four substeps; in the air the up turns back upright 2.5 a second
constexpr s32 Substeps = 4;
constexpr f32 SubstepShare = 0.25f;
constexpr f32 UprightRate = 2.5f;

// The jump: 13 up the board's y axis, then 0.1 seconds before the next and 0.2 before rails count again
constexpr f32 JumpSpeed = 13.0f;
constexpr f32 JumpCooldown = Rounded(0.1);
constexpr f32 RailCooldown = Rounded(0.2);

// Steering: the root of the stick across the board times 1.2 (within 1), jittered on the ground by the speed times a random 1.4
// either way over 32; radians a second 4 on the ground (1.3 crouched) and 2 in the air. Leaning past 0.6 across, until back
// under 0.3
constexpr f32 SteerScale = Rounded(1.2);
constexpr f32 SteerJitter = Rounded(1.4);
constexpr f32 JitterShare = 1.0f / 32.0f;
constexpr f32 GroundTurnRate = 4.0f;
constexpr f32 CrouchedTurnRate = 1.3f;
constexpr f32 AirTurnRate = 2.0f;
constexpr f32 LeanStart = Rounded(0.6);
constexpr f32 LeanEnd = Rounded(0.3);

// The cache's box: the board's reach and 0.2 more around it (the instances' and the hulls': 0.1 more); its triangles
constexpr f32 CacheMargin = Rounded(0.2);
constexpr f32 InstanceMargin = Rounded(0.1);
constexpr s32 MostTriangles = 0x100;

// The rails: at most 30 among at most 32 steep triangles (their normals within 0.5 of level) across its way (within 0.707 of
// square to its heading); a ridge of two such triangles facing each other (their normals' dot product between -0.99 and -0.5)
// sharing two corners (within 0.1); a rail is the one it's on when its ends are within 0.1 of its ends
constexpr s32 MostRails = 30;
constexpr s32 MostRailTriangles = 32;
constexpr f32 SteepLimit = 0.5f;
constexpr f32 AcrossLimit = Rounded(0.707);
constexpr f32 RailBoxMargin = 0.01f;
constexpr f32 RidgeLeast = Rounded(-0.99);
constexpr f32 RidgeMost = -0.5f;
constexpr f32 SamePoint = 0.01f;
constexpr f32 NoRail = -1e10f;
// A rail it touches: its triangle (its second corner 0.1 lower) touched by the board's ellipsoid grown by a fifth
constexpr f32 RailRadiiScale = Rounded(1.2);
constexpr f32 RailHullDrop = Rounded(0.1);

// Grinding: pulled 0.4 above the rail at 5 a second at most; eased toward the speed 25 less 11 times the slope (3.5 times its
// rise, within 1) by at most 12 a second; turned toward the rail by at most 150 a second (12 times that)
constexpr f32 RailRaise = Rounded(0.4);
constexpr f32 RailPull = 5.0f;
constexpr f32 RailSlopeScale = 3.5f;
constexpr f32 RailSpeed = 25.0f;
constexpr f32 RailSlopeSpeed = 11.0f;
constexpr f32 RailAcceleration = 12.0f;
constexpr f32 RailTurnSpeed = 150.0f;
constexpr f32 RailTurnRate = 12.0f;

// The ground: a quarter of each push out of a triangle
constexpr f32 PushShare = 0.25f;

// The ground's response: the up eased by 0.04 of the normal; hitting it faster than 2 halves the speed into it, or past 3
// against the way it goes (the normal against the flat motion past -0.5) a crash bounces it 1.3 times; the slide across the
// board rubbed off by the friction (45 times it a second, over 1 to 4 the more the board faces its motion, by 1.414 and the
// up's y) and the board turned with a tenth of it (10 a second at most); on the ground when the normal's y passes 0.2
constexpr f32 UpEase = 0.04f;
constexpr f32 HardHit = -2.0f;
constexpr f32 CrashSpeed = 3.0f;
constexpr f32 CrashFacing = -0.5f;
constexpr f32 CrashBounce = 1.3f;
constexpr f32 MostGrip = 4.0f;
constexpr f32 GripScale = Rounded(3.2);
constexpr f32 LeastGrip = 1.0f;
constexpr f32 FrictionScale = 45.0f;
constexpr f32 SlideScale = Rounded(1.414);
constexpr f32 SlideTurnShare = Rounded(0.1);
constexpr f32 SlideTurnSpeed = 10.0f;
constexpr f32 StandingNormal = Rounded(0.2);

// The rider's hull test (not crouched): an ellipsoid of 0.45 by 0.6 0.8 up the board's y axis
constexpr f32 RiderRadiusXZ = 0.45f;
constexpr f32 RiderRadiusY = Rounded(0.6);
constexpr f32 RiderLift = Rounded(0.8);

// Under 5 going forward on flat ground (the up's y past 0.866) it speeds up 10 a second
constexpr f32 SlowSpeed = 5.0f;
constexpr f32 FlatUp = 0.866f;
constexpr f32 SpeedUp = 10.0f;

// The up's turn: none for an axis shorter than this
constexpr f32 AxisEpsilon = 1e-4f;

// The skid marks: on the ground moving faster than 0.1 flat, 0.866 of its y radius down, offset by half the up, wider the more
// it slides (0.3 more 0.4 times the root of 1.001 less the motion's along its heading squared), 0.16 deep jittered 0.01
constexpr f32 SkidSpeed = Rounded(0.1);
constexpr f32 SkidDrop = 0.866f;
constexpr f32 SkidUpShift = 0.5f;
constexpr f32 SkidWidthBase = Rounded(1.001);
constexpr f32 SkidWidthScale = Rounded(0.4);
constexpr f32 SkidWidthLeast = Rounded(0.3);
constexpr f32 SkidDepth = 0.16f;
constexpr f32 SkidJitter = 0.01f;

// The suspension: a landing squashes it 1.6 times the fall; a spring of -1.5 damped 0.015, within 0.5
constexpr f32 LandingSquash = Rounded(-1.6);
constexpr f32 SuspensionSpring = -1.5f;
constexpr f32 SuspensionDamping = 0.015f;
constexpr f32 MostSquash = 0.5f;
// The board character squashed by it (widened 0.4 times), the rider raised 0.6 times it
constexpr f32 SquashWiden = Rounded(0.4);
constexpr f32 RiderSquash = Rounded(0.6);

// The ground below: a ray 10 down and 4 ahead, 10 when it hits nothing
constexpr f32 RayDrop = -10.0f;
constexpr f32 RayAhead = 4.0f;
constexpr f32 NoGround = 10.0f;

// Tricks off a ramp: 0.03 seconds in the air higher than 3, a flip with more than 0.519 seconds of fall to go, a half spin with
// more than 0.362; the flip eased 5.1 times what's left and 1.4 a second, the spin 7.5 times and 1 a second. A landed trick
// shows for 0.5 seconds, leaving a rail for 0.2
constexpr f32 TrickAirTime = 0.03f;
constexpr f32 TrickHeight = 3.0f;
constexpr f32 FlipTime = 0x1.09aa84p-1f;
constexpr f32 SpinTime = 0x1.727b8ap-2f;
constexpr f32 FlipEase = 5.1f;
constexpr f32 FlipRate = 1.4f;
constexpr f32 SpinEase = 7.5f;
constexpr f32 LandedTime = 0.5f;
constexpr f32 OffRailTime = Rounded(0.2);

ObjectNode* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
}

// The board character's sounds (none without one)
CharacterSounds* BoardSounds(const HumiliskateVehicle* vehicle)
{
    if (vehicle->other == nullptr)
    {
        return nullptr;
    }

    ObjectNode* node = ObjectNodeOf(vehicle->other->instance);
    if (node == nullptr)
    {
        return nullptr;
    }

    auto* sounds = reinterpret_cast<CharacterSounds*>(node->unknown114);
    if (sounds == nullptr || sounds->kind != CharacterSoundsKind)
    {
        return nullptr;
    }

    return sounds;
}

// An event run on the character (its instance with it)
void RunOnRider(HumiliskateVehicle* vehicle, u32 event)
{
    RunAgentEvent(vehicle->agent, event, reinterpret_cast<u32>(vehicle->agent->instance), 0, 0);
}

// An event run on the board character's agent (its character node's), which may be none: retail then reads the word at address
// 0 for its instance. Retail doesn't check the other for none
void RunOnBoard(HumiliskateVehicle* vehicle, u32 event)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&vehicle->other->instance->nodes, CharacterNodeKind));
    Agent* board = node != nullptr ? node->agent : nullptr;
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(board) + offsetof(Agent, instance);
    RunAgentEvent(board, event, *reinterpret_cast<const u32*>(address), 0, 0);
}

// An event run on both characters, the character's instance with each
void RunOnBoth(HumiliskateVehicle* vehicle, u32 event)
{
    RunOnRider(vehicle, event);
    RunAgentEvent(vehicle->other, event, reinterpret_cast<u32>(vehicle->agent->instance), 0, 0);
}

// The riding event (the board's, and the character's when it isn't crouched)
u32 RidingEvent(const HumiliskateVehicle* vehicle)
{
    u32 event = EventRiding;
    if (vehicle->leanLeft != 0)
    {
        event = EventRiding + EventLeft;
    }
    else if (vehicle->leanRight != 0)
    {
        event = EventRiding + EventRight;
    }

    return vehicle->backwards != 0 ? event + EventBackwards : event;
}

f32 Dot(const Vector4* a, const Vector4* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

f32 DistanceSquared(const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return x * x + y * y + z * z;
}

f32 Length(const Vector4* vector)
{
    return __builtin_sqrtf(vector->x * vector->x + vector->y * vector->y + vector->z * vector->z);
}

void Normalize(Vector4* vector)
{
    f32 inverse = InverseLength(vector, LengthEpsilon);
    vector->x = vector->x * inverse;
    vector->y = vector->y * inverse;
    vector->z = vector->z * inverse;
}

// The board turned about an axis (its position kept, its axes made unit length again)
void TurnBoard(HumiliskateVehicle* vehicle, const Vector4* axis, const s32* angle)
{
    Matrix4x4 rotation;
    AxisAngleMatrix(&rotation, axis, angle, 1);
    Vector4 position = *RowOf(&vehicle->board, 3);
    MultiplyInPlace(&vehicle->board, &rotation);
    Normalize(RowOf(&vehicle->board, 0));
    Normalize(RowOf(&vehicle->board, 1));
    Normalize(RowOf(&vehicle->board, 2));
    *RowOf(&vehicle->board, 3) = position;
}

// A rail taken: its ends, grinding, and the tricks' angles where they were going
void TakeRail(HumiliskateVehicle* vehicle, const Vector4* start, const Vector4* end)
{
    vehicle->railStart = *start;
    vehicle->railEnd = *end;
    vehicle->grinding = 1;
    vehicle->trick = HumiliskateVehicle::TrickGrinding;
    vehicle->flip = vehicle->flipTarget;
    vehicle->spin = vehicle->spinTarget;
}

// Grinding: pulled toward the rail (0.4 above it) and slid along it, its speed eased toward the rail's; past the rail's end it's
// off it (whether it is). Its up and the ground's are the rail's, and the board turns toward the rail
bool Grind(HumiliskateVehicle* vehicle, f32 seconds)
{
    bool passedEnd = false;
    Vector4 along = vehicle->railEnd;
    along.x = along.x - vehicle->railStart.x;
    along.y = along.y - vehicle->railStart.y;
    along.z = along.z - vehicle->railStart.z;
    Normalize(&along);
    Vector4* position = RowOf(&vehicle->board, 3);
    f32 at = Dot(&along, position) - Dot(&along, &vehicle->railStart);
    Vector4 offset = {along.x * at, along.y * at, along.z * at, 1.0f};
    Vector4 onRail = {vehicle->railStart.x + offset.x, vehicle->railStart.y + offset.y, vehicle->railStart.z + offset.z, 1.0f};
    Vector4 pull = {onRail.x - position->x, onRail.y + RailRaise - position->y, onRail.z - position->z, 1.0f};
    f32 mostPull = seconds * RailPull;
    if (mostPull < Length(&pull))
    {
        f32 inverse = InverseLength(&pull, LengthEpsilon);
        pull.x = pull.x * inverse * mostPull;
        pull.y = pull.y * inverse * mostPull;
        pull.z = pull.z * inverse * mostPull;
    }

    position->x = position->x + pull.x;
    position->y = position->y + pull.y;
    position->z = position->z + pull.z;
    f32 speedAlong = Dot(&along, &vehicle->velocity);
    Vector4 slide = {along.x * speedAlong, along.y * speedAlong, along.z * speedAlong, 1.0f};
    position->x = position->x + slide.x * seconds;
    position->y = position->y + slide.y * seconds;
    position->z = position->z + slide.z * seconds;

    f32 slope = along.y * RailSlopeScale;
    if (1.0f < slope)
    {
        slope = 1.0f;
    }
    else if (slope < -1.0f)
    {
        slope = -1.0f;
    }

    f32 speed = Length(&vehicle->velocity);
    f32 change = RailSpeed - slope * RailSlopeSpeed - speed;
    f32 mostChange = seconds * RailAcceleration;
    if (mostChange < change)
    {
        change = mostChange;
    }
    else if (change < -mostChange)
    {
        change = -mostChange;
    }

    speed = speed + change;
    vehicle->velocity = {along.x * speed, along.y * speed, along.z * speed, 1.0f};
    if (Dot(&along, &vehicle->railEnd) < Dot(&along, position))
    {
        vehicle->trick = HumiliskateVehicle::TrickOffRail;
        passedEnd = true;
        vehicle->grinding = 0;
        vehicle->trickTime = 0.0f;
    }

    // The rail's up: square to it and level across it
    Vector4 worldUp = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 side;
    side.x = along.y * worldUp.z - along.z * worldUp.y;
    side.y = along.z * worldUp.x - along.x * worldUp.z;
    side.z = along.x * worldUp.y - along.y * worldUp.x;
    side.w = 1.0f;
    Normalize(&side);
    Vector4 railUp;
    railUp.x = side.y * along.z - side.z * along.y;
    railUp.y = side.z * along.x - side.x * along.z;
    railUp.z = side.x * along.y - side.y * along.x;
    railUp.w = 1.0f;
    Normalize(&railUp);
    vehicle->groundNormal = railUp;
    vehicle->up = railUp;

    // The board turned about its y toward the rail (the way the cross product of its heading and the rail lies along its y)
    const Vector4* heading = RowOf(&vehicle->board, 2);
    Vector4 cross;
    cross.x = heading->y * along.z - heading->z * along.y;
    cross.y = heading->z * along.x - heading->x * along.z;
    cross.z = heading->x * along.y - heading->y * along.x;
    f32 mostTurn = seconds * RailTurnSpeed;
    f32 turn = Length(&cross);
    if (mostTurn < turn)
    {
        turn = mostTurn;
    }
    else if (turn < -mostTurn)
    {
        turn = -mostTurn;
    }

    const Vector4* boardUp = RowOf(&vehicle->board, 1);
    f32 rate = 0.0f <= Dot(&cross, boardUp) ? RailTurnRate : -RailTurnRate;
    s32 angle;
    AngleFrom(&angle, turn * seconds * rate, AngleRadians);
    TurnBoard(vehicle, boardUp, &angle);
    return passedEnd;
}
}

HumiliskateVehicle* HumiliskateVehicle::Construct(HumiliskateVehicle* vehicle, CharacterAgent* agent, CharacterAgent* other)
{
    vehicle->agent = agent;
    vehicle->bits = 0;
    vehicle->other = other;
    vehicle->vtable = g_HumiliskateVehicleVTable;
    vehicle->Bits() = (vehicle->Bits() | BitDrives) & ~u64{BitHeld};
    ConstructCollisionCache(&vehicle->cache, agent->instance, SolidToProbes);
    ConstructSkidMarks(&vehicle->leftMarks);
    ConstructSkidMarks(&vehicle->rightMarks);
    vehicle->Start();
    return vehicle;
}

// The board at the character's place raised by its y radius, going 10 along the character's heading, upright, on the ground
void HumiliskateVehicle::Start()
{
    radiusX = BoardRadiusX;
    riderHeight = RiderHeight;
    radiusY = BoardRadiusY;
    radiusZ = BoardRadiusZ;
    reach = BoardReach;
    gravity = BoardGravity;
    InitIdentityMatrix(&board);
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    board = place->matrix;
    unknown140 = 0;
    RowOf(&board, 3)->y = RowOf(&board, 3)->y + radiusY;
    const Vector4* heading = RowOf(&place->matrix, 2);
    Vector4 start = {heading->x * StartSpeed, heading->y * StartSpeed, heading->z * StartSpeed, 1.0f};
    lastVelocity = start;
    velocity = start;
    up = {0.0f, 1.0f, 0.0f, 1.0f};
    onGround = 1;
    wasOnGround = 0;
    trick = TrickRiding;
    trickTime = 0.0f;
    suspension = 0.0f;
    suspensionSpeed = 0.0f;
    crouched = 0;
    leanLeft = 0;
    crouchedSpeed = CrouchedTopSpeed;
    unknown224 = 1.0f;
    jumpReleased = 1.0f;
    leanRight = 0;
    backwards = 0;
    airTime = 0.0f;
    spin = 0.0f;
    flip = 0.0f;
    spinTarget = 0.0f;
    flipTarget = 0.0f;
    jumped = 0;
    unknown250 = 0;
    unknown254 = 0;
    grinding = 0;
    railCooldown = 0.0f;
    unknown25C = 0;
    touched = 0;
    jumpCooldown = 0.0f;
    jumpedOffRail = 0;
    otherEvent = EventNone;
    topSpeed = TopSpeed;
    event = EventNone;
    ClearSkidMarks(&leftMarks);
    ClearSkidMarks(&rightMarks);
    railStart = g_DefaultBox.min;
    railStart.w = 1.0f;
    railEnd = g_DefaultBox.min;
    railEnd.w = 1.0f;
    surface = nullptr;
    startTime = GetContextClock(agent->instance)->time;
    Vehicle::Start();
}

// The board character at the board flipped by the flip, lowered along its y by the riders' height, squashed by the suspension
// and turned by the spin; the character at the flipped board, lowered the same and raised with the suspension; the exit upright
// on the board's heading at its position
u32 HumiliskateVehicle::Place()
{
    Matrix4x4 flipped = board;
    s32 angle;
    AngleFrom(&angle, -flip, AngleRadians);
    Matrix4x4 turn;
    MatrixAboutX(&turn, &angle);
    PreMultiply(&flipped, &turn);
    otherMatrix = flipped;
    const Vector4* boardUp = RowOf(&otherMatrix, 1);
    Vector4 drop = {boardUp->x * riderHeight, boardUp->y * riderHeight, boardUp->z * riderHeight, 1.0f};
    Vector4* position = RowOf(&otherMatrix, 3);
    position->x = position->x - drop.x;
    position->y = position->y - drop.y;
    position->z = position->z - drop.z;
    agentMatrix = otherMatrix;

    f32 squash = -suspension;
    Matrix4x4 scale;
    InitIdentityMatrix(&scale);
    if (squash < -MostSquash)
    {
        squash = -MostSquash;
    }

    if (MostSquash < squash)
    {
        squash = MostSquash;
    }

    scale.m[1][1] = 1.0f - squash;
    f32 widen = squash * SquashWiden + 1.0f;
    scale.m[2][2] = widen;
    scale.m[0][0] = widen;
    PreMultiply(&otherMatrix, &scale);
    AngleFrom(&angle, spin, AngleRadians);
    MatrixAboutY(&turn, &angle);
    PreMultiply(&otherMatrix, &turn);
    RowOf(&agentMatrix, 3)->y = RowOf(&agentMatrix, 3)->y + suspension * RiderSquash;

    Vector4* exitX = RowOf(&exitMatrix, 0);
    Vector4* exitY = RowOf(&exitMatrix, 1);
    Vector4* exitZ = RowOf(&exitMatrix, 2);
    *exitZ = *RowOf(&board, 2);
    *exitY = {0.0f, 1.0f, 0.0f, 1.0f};
    // The up's cross product with the heading (retail's compiler folded the x into the heading's z)
    exitX->x = exitZ->z;
    exitX->y = exitY->z * exitZ->x - exitY->x * exitZ->z;
    exitX->z = exitY->x * exitZ->y - exitY->y * exitZ->x;
    exitZ->x = exitX->y * exitY->z - exitX->z * exitY->y;
    exitZ->y = exitX->z * exitY->x - exitX->x * exitY->z;
    exitZ->z = exitX->x * exitY->y - exitX->y * exitY->x;
    exitX->w = 0.0f;
    exitY->w = 0.0f;
    exitZ->w = 0.0f;
    *RowOf(&exitMatrix, 3) = *RowOf(&board, 3);
    return 1;
}

void HumiliskateVehicle::Destroy(u32 destroyFlags)
{
    vtable = g_HumiliskateVehicleVTable;
    DestroySkidMarks(&rightMarks, 2);
    DestroySkidMarks(&leftMarks, 2);
    DestroyCollisionCache(&cache, 2);
    Vehicle::Destroy(destroyFlags);
}

void HumiliskateVehicle::Push(const Vector4* push, InstanceContext* instance)
{
    if (instance == nullptr)
    {
        velocity.x = velocity.x + push->x;
        velocity.y = velocity.y + push->y;
        velocity.z = velocity.z + push->z;
        return;
    }

    ObjectPlace* place = instance->place;
    Vector4 turned = *push;
    RotateAndTranslate(place);
    VuRotateVector(&place->matrix, &turned, &turned);
    velocity.x = velocity.x + turned.x;
    velocity.y = velocity.y + turned.y;
    velocity.z = velocity.z + turned.z;
}

// Through a link whose chunk isn't loaded it stays, put back at the instance's kept position and stopped
u32 HumiliskateVehicle::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        *RowOf(&board, 3) = agent->instance->box;
        velocity = g_DefaultBox.min;
        velocity.w = 1.0f;
        return 0;
    }

    TransformThroughLink(link, &board, 1);
    TransformVectorThroughLink(link, &velocity, 0);
    TransformVectorThroughLink(link, &lastVelocity, 0);
    TransformVectorThroughLink(link, &groundNormal, 0);
    TransformVectorThroughLink(link, &up, 0);
    TransformVectorThroughLink(link, &railStart, 1);
    TransformVectorThroughLink(link, &railEnd, 1);
    return 1;
}

void HumiliskateVehicle::Frame(f32 seconds)
{
    f32 step = seconds * SubstepShare;
    if ((bits & BitHeld) == 0)
    {
        for (s32 substep = 0; substep < Substeps; substep++)
        {
            Steer(step);
            touched = 0;
            onGround = 0;
            Collide(step);
            CollideInstances(step);
            if (touched == 0)
            {
                up.y = up.y + step * UprightRate;
                Normalize(&up);
            }

            if (onGround != 0 && wasOnGround == 0)
            {
                jumped = 0;
                flip = flipTarget;
                spin = spinTarget;
            }

            LimitSpeed(step);
            AlignUp(&up);
            Suspension(step);
            if (grinding == 0)
            {
                Vector4 motion = {velocity.x * step, velocity.y * step, velocity.z * step, 1.0f};
                Vector4* position = RowOf(&board, 3);
                position->x = position->x + motion.x;
                position->y = position->y + motion.y;
                position->z = position->z + motion.z;
                velocity.y = velocity.y - gravity * step;
            }

            wasOnGround = onGround;
            lastVelocity = velocity;
            if (onGround == 0)
            {
                airTime = airTime + step;
            }
            else
            {
                airTime = 0.0f;
            }

            Tricks(step);
            Jump(step);
        }

        SendEvents(seconds);
        ParticleTrails* trails = ObjectNodeOf(agent->instance)->particleTrails;
        if (trails != nullptr)
        {
            trails->strength = onGround != 0 ? 1.0f : 0.0f;
        }

        LaySkidMarks();
    }

    // Both characters placed (retail works out the rotation of agentMatrix first and drops it)
    if (Vehicle::Place() == 0)
    {
        return;
    }

    InstanceContext* instance = agent->instance;
    Vector4 rotation;
    GetRotationVec(&rotation, &agentMatrix);
    if (SetPlaceMatrix(instance->place, &agentMatrix) != 0)
    {
        QueueObject(instance);
    }

    if (other != nullptr)
    {
        InstanceContext* otherInstance = other->instance;
        if (SetPlaceMatrix(otherInstance->place, &otherMatrix) != 0)
        {
            QueueObject(otherInstance);
        }
    }
}

u32 HumiliskateVehicle::Kind()
{
    return KindHumiliskate;
}

u32 HumiliskateVehicle::Velocity(Vector4* out)
{
    *out = velocity;
    return 1;
}

void HumiliskateVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-1.0f, -1.0f, -1.0f, 1.0f};
    *max = {1.0f, 1.0f, 1.0f, 1.0f};
}

void HumiliskateVehicle::SetVelocity(const Vector4* start)
{
    lastVelocity = *start;
    velocity = *start;
}

void HumiliskateVehicle::Draw()
{
    DrawSkidMarks(&leftMarks);
    DrawSkidMarks(&rightMarks);
}

u32 HumiliskateVehicle::HasBody()
{
    return 0;
}

// Cross on the ground or a rail, once the last jump's cooldown is over and cross was let go since
void HumiliskateVehicle::Jump(f32 seconds)
{
    f32 cross = agent->buttons.cross;
    if (cross != 0.0f && (onGround != 0 || grinding != 0) && jumpCooldown == 0.0f && jumpReleased != 0.0f)
    {
        velocity.y = velocity.y + RowOf(&board, 1)->y * JumpSpeed;
        onGround = 0;
        trick = TrickRiding;
        jumpedOffRail = grinding;
        grinding = 0;
        jumped = 1;
        jumpCooldown = JumpCooldown;
        jumpReleased = 0.0f;
        railCooldown = RailCooldown;
        if (JumpTells() != 0)
        {
            RunOnBoth(this, EventJump);
        }
    }
    else if (cross == 0.0f)
    {
        jumpReleased = 1.0f;
    }

    jumpCooldown = jumpCooldown - seconds;
    if (jumpCooldown < 0.0f)
    {
        jumpCooldown = 0.0f;
    }
}

u32 HumiliskateVehicle::JumpTells()
{
    return 1;
}

// Turned about y by the stick across the board; crouched while circle is held; leaning past 0.6 across (the board character's
// sounds told on the ground)
void HumiliskateVehicle::Steer(f32 seconds)
{
    CharacterSounds* sounds = BoardSounds(this);
    // (retail inverts the board's place here and drops it)
    Matrix4x4 inverse = board;
    VuInvertRigidInPlace(&inverse);
    const CharacterButtons& buttons = agent->buttons;
    const Vector4* side = RowOf(&board, 0);
    f32 across = buttons.moveX * side->x + buttons.moveZ * side->z;
    f32 sign = 0.0f < across ? 1.0f : -1.0f;
    f32 turn = sign * (__builtin_sqrtf(__builtin_fabsf(across)) * SteerScale);
    if (turn < -1.0f)
    {
        turn = -1.0f;
    }
    else if (1.0f < turn)
    {
        turn = 1.0f;
    }

    f32 rate = AirTurnRate;
    if (onGround != 0)
    {
        f32 jitter = RandomSignedTimes(SteerJitter);
        turn = turn + jitter * Length(&velocity) * JitterShare;
        rate = crouched != 0 ? CrouchedTurnRate : GroundTurnRate;
    }

    s32 angle;
    AngleFrom(&angle, turn * rate * seconds, AngleRadians);
    Matrix4x4 turnMatrix;
    MatrixAboutY(&turnMatrix, &angle);
    Vector4 position = *RowOf(&board, 3);
    PreMultiply(&board, &turnMatrix);
    *RowOf(&board, 3) = position;
    crouched = buttons.circle != 0.0f;

    if (leanLeft != 0)
    {
        if (!(across < -LeanEnd))
        {
            leanLeft = 0;
        }
    }
    else if (across < -LeanStart)
    {
        if (sounds != nullptr && onGround != 0)
        {
            PlayCharacterSurfaceSound2(sounds, across);
        }

        leanRight = 0;
        leanLeft = 1;
    }

    if (leanRight != 0)
    {
        if (!(LeanEnd < across))
        {
            leanRight = 0;
        }
    }
    else if (LeanStart < across)
    {
        if (sounds != nullptr && onGround != 0)
        {
            PlayCharacterSurfaceSound2(sounds, across);
        }

        leanLeft = 0;
        leanRight = 1;
    }
}

// The cache refreshed around the board and its rails found. Grinding, it rides its rail. Not grinding (after the cooldown) and
// going forward, it takes a rail starting where the one it left this substep ended or the touched rail that reaches furthest
// ahead. Then the ground's triangles its ellipsoid touches push it out
void HumiliskateVehicle::Collide(f32 seconds)
{
    Vector4 start = *RowOf(&board, 3);
    Box box = {start, start};
    Vector4 position = start;
    GrowBox(reach + CacheMargin, &box);
    RefreshCollisionCache(&cache, &box);
    // (retail has no room for more than 256 triangles)
    CollisionHit* triangles[MostTriangles];
    s32 count = CachedTriangles(triangles);
    Vector4 starts[MostRails];
    Vector4 ends[MostRails];
    u8 used[MostTriangles];
    RetailLibc::MemorySet(used, 0, sizeof(used));
    s32 rails = FindRails(triangles, count, used, sizeof(used), starts, ends);
    bool passedEnd = false;
    if (grinding != 0)
    {
        passedEnd = Grind(this, seconds);
    }

    const Vector4* heading = RowOf(&board, 2);
    if (grinding == 0 && 0.0f < railCooldown)
    {
        railCooldown = railCooldown - seconds;
        if (railCooldown < 0.0f)
        {
            railCooldown = 0.0f;
        }
    }
    else if (grinding == 0 && !(Dot(&velocity, heading) < 0.0f))
    {
        // Just off a rail: the one whose nearer end (along the heading) is where the last one ended
        if (passedEnd)
        {
            for (s32 i = 0; i < rails; i++)
            {
                const Vector4* first = &ends[i];
                const Vector4* second = &starts[i];
                if (Dot(&starts[i], heading) < Dot(&ends[i], heading))
                {
                    first = &starts[i];
                    second = &ends[i];
                }

                if (DistanceSquared(first, &railEnd) < SamePoint)
                {
                    TakeRail(this, first, second);
                    break;
                }
            }
        }

        if (grinding == 0)
        {
            s32 best = -1;
            f32 furthest = NoRail;
            Vector4 bestStart;
            Vector4 bestEnd;
            for (s32 i = 0; i < rails; i++)
            {
                // Not the rail it was on, either way round
                if (DistanceSquared(&starts[i], &railStart) < SamePoint && DistanceSquared(&ends[i], &railEnd) < SamePoint)
                {
                    continue;
                }

                if (DistanceSquared(&starts[i], &railEnd) < SamePoint && DistanceSquared(&ends[i], &railStart) < SamePoint)
                {
                    continue;
                }

                CollisionHull hull;
                HullConstruct(&hull);
                // (the radii's w left as the stack had it)
                Vector4 radii;
                radii.x = radiusX * RailRadiiScale;
                radii.y = radiusY * RailRadiiScale;
                radii.z = radiusZ * RailRadiiScale;
                Matrix4x4 hullMatrix;
                InitIdentityMatrix(&hullMatrix);
                const Vector4 drop = {0.0f, RailHullDrop, 0.0f, 1.0f};
                Vector4 triangle[3];
                triangle[0] = starts[i];
                triangle[1] = ends[i];
                triangle[2] = {ends[i].x - drop.x, ends[i].y - drop.y, ends[i].z - drop.z, 1.0f};
                BuildTriangleHull(&hull, triangle);
                Vector4 push;
                Vector4 normal;
                if (EllipsoidTouchesHull(&hull, &board, &radii, &hullMatrix, &push, &normal) != 0)
                {
                    f32 startAhead = Dot(&starts[i], heading);
                    f32 endAhead = Dot(&ends[i], heading);
                    f32 ahead = startAhead < endAhead ? endAhead : startAhead;
                    if (furthest < ahead)
                    {
                        furthest = ahead;
                        best = i;
                        if (startAhead < endAhead)
                        {
                            bestStart = starts[i];
                            bestEnd = ends[i];
                        }
                        else
                        {
                            bestStart = ends[i];
                            bestEnd = starts[i];
                        }
                    }
                }

                HullDestroy(&hull, 2);
            }

            if (best != -1)
            {
                TakeRail(this, &bestStart, &bestEnd);
            }
        }
    }

    // The ground: each triangle the board's ellipsoid (the unit sphere in the board's place scaled by the radii) touches sends the
    // character its surface's contact message; a solid one pushes the board out (a quarter of it), its outward normal summed into
    // the ground's, its friction averaged, the surface of the most upward one kept
    groundNormal = g_DefaultBox.min;
    groundNormal.w = 1.0f;
    s32 pushes = 0;
    f32 friction = 0.0f;
    f32 highest = -1.0f;
    f32 x = radiusX;
    f32 y = radiusY;
    f32 z = radiusZ;
    Matrix4x4 toUnit;
    InitIdentityMatrix(&toUnit);
    surface = nullptr;
    toUnit.m[0][0] = 1.0f / x;
    toUnit.m[1][1] = 1.0f / y;
    toUnit.m[2][2] = 1.0f / z;
    for (s32 i = 0; i < count; i++)
    {
        CollisionHit* triangle = triangles[i];
        bool solid = (GetTriangleSurface(triangle)->collisionMask & SolidToPlayer) != 0;
        Vector4 normal;
        TriangleNormal(triangle, &normal);
        Vector4 outward = {-normal.x, -normal.y, -normal.z, 1.0f};
        CollisionHit local = *triangle;
        Matrix4x4 toBoard = board;
        *RowOf(&toBoard, 3) = position;
        VuInvertRigidInPlace(&toBoard);
        TransformCollisionHit(&local, &toBoard);
        TransformCollisionHit(&local, &toUnit);
        Vector4 centre = {0.0f, 0.0f, 0.0f, 1.0f};
        Vector4 push;
        if (SphereTouchesTriangle(1.0f, &local, &centre, &push) == 0)
        {
            continue;
        }

        if (solid)
        {
            push.x = push.x * x;
            push.y = push.y * y;
            push.z = push.z * z;
            pushes++;
            VuRotateVector(&board, &push, &push);
            Vector4 share = {push.x * PushShare, push.y * PushShare, push.z * PushShare, 1.0f};
            position.x = position.x + share.x;
            position.y = position.y + share.y;
            position.z = position.z + share.z;
            groundNormal.x = groundNormal.x + outward.x;
            groundNormal.y = groundNormal.y + outward.y;
            groundNormal.z = groundNormal.z + outward.z;
            friction = friction + GetTriangleSurface(triangle)->friction;
            if (highest < outward.y)
            {
                highest = outward.y;
                surface = GetTriangleSurface(triangle);
            }
        }

        agent->SendSurfaceMessage(GetTriangleSurface(triangle));
    }

    // (just off a rail, the board goes back to where the substep started: retail drops that substep's pull and slide)
    if (grinding == 0)
    {
        *RowOf(&board, 3) = position;
        if (pushes != 0)
        {
            GroundResponse(friction / static_cast<f32>(pushes), seconds, &groundNormal);
        }
    }

    CharacterSounds* sounds = BoardSounds(this);
    if (sounds != nullptr)
    {
        SetCharacterSurface(sounds, surface, &velocity);
    }
}

s32 HumiliskateVehicle::CachedTriangles(CollisionHit** triangles)
{
    s32 count = 0;
    for (CollisionHit* hit = FirstCollisionHit(&cache); hit != nullptr; hit = NextCollisionHit(&cache))
    {
        triangles[count++] = hit;
    }

    return count;
}

// The board's y axis turned onto an up (its position kept)
void HumiliskateVehicle::AlignUp(const Vector4* to)
{
    Vector4 from = *RowOf(&board, 1);
    Vector4 target = *to;
    Normalize(&from);
    Normalize(&target);
    Vector4 axis;
    axis.x = from.y * target.z - from.z * target.y;
    axis.y = from.z * target.x - from.x * target.z;
    axis.z = from.x * target.y - from.y * target.x;
    axis.w = 1.0f;
    if (!(AxisEpsilon < Length(&axis)))
    {
        return;
    }

    f32 inverse = InverseLength(&axis, LengthEpsilon);
    f32 cosine = Dot(&from, &target);
    axis.x = axis.x * inverse;
    axis.y = axis.y * inverse;
    axis.z = axis.z * inverse;
    s32 angle;
    AngleOfCosine(cosine, &angle);
    TurnBoard(this, &axis, &angle);
}

// No faster than the top speed (the crouched one while crouched); slower than 5 going forward on flat ground it speeds up
void HumiliskateVehicle::LimitSpeed(f32 seconds)
{
    f32 speed = Length(&velocity);
    f32 most = crouched != 0 ? crouchedSpeed : topSpeed;
    if (most < speed)
    {
        Normalize(&velocity);
        velocity.x = velocity.x * most;
        velocity.y = velocity.y * most;
        velocity.z = velocity.z * most;
    }

    if (speed < SlowSpeed && 0.0f < Dot(&velocity, RowOf(&board, 2)) && FlatUp < up.y)
    {
        f32 faster = speed + seconds * SpeedUp;
        Normalize(&velocity);
        velocity.x = velocity.x * faster;
        velocity.y = velocity.y * faster;
        velocity.z = velocity.z * faster;
    }
}

// The up eased toward the normal, touched; a hard hit halves the motion into it (a crash bounces it off); the slide across the
// board rubbed off and the board turned with it; on the ground when the normal points up
void HumiliskateVehicle::GroundResponse(f32 friction, f32 seconds, const Vector4* normal)
{
    Vector4 unit = *normal;
    Normalize(&unit);
    Vector4 ease = {unit.x * UpEase, unit.y * UpEase, unit.z * UpEase, 1.0f};
    up.x = up.x + ease.x;
    up.y = up.y + ease.y;
    up.z = up.z + ease.z;
    Normalize(&up);
    touched = 1;
    f32 into = Dot(&unit, &velocity);
    if (into < HardHit)
    {
        Vector4 hit = {unit.x * into, unit.y * into, unit.z * into, 1.0f};
        Vector4 taken = {hit.x * 0.5f, hit.y * 0.5f, hit.z * 0.5f, 1.0f};
        Vector4 flat = velocity;
        flat.y = 0.0f;
        f32 impact = Length(&taken);
        Normalize(&flat);
        if (CrashSpeed < impact && Dot(&unit, &flat) < CrashFacing)
        {
            RunOnBoth(this, EventCrash);
            Vector4 bounce = {unit.x * into, unit.y * into, unit.z * into, 1.0f};
            taken = {bounce.x * CrashBounce, bounce.y * CrashBounce, bounce.z * CrashBounce, 1.0f};
        }

        velocity.x = velocity.x - taken.x;
        velocity.y = velocity.y - taken.y;
        velocity.z = velocity.z - taken.z;
    }

    const Vector4* side = RowOf(&board, 0);
    f32 slide = Dot(side, &velocity);
    Vector4 direction = velocity;
    Normalize(&direction);
    f32 grip = MostGrip - Dot(RowOf(&board, 2), &direction) * GripScale;
    if (grip < LeastGrip)
    {
        grip = LeastGrip;
    }
    else if (MostGrip < grip)
    {
        grip = MostGrip;
    }

    f32 mostSlide = seconds * (friction * FrictionScale) / grip * SlideScale * up.y;
    if (mostSlide < slide)
    {
        slide = mostSlide;
    }
    else if (slide < -mostSlide)
    {
        slide = -mostSlide;
    }

    Vector4 rub = {side->x * slide, side->y * slide, side->z * slide, 1.0f};
    f32 turn = slide * SlideTurnShare;
    f32 mostTurn = seconds * SlideTurnSpeed;
    velocity.x = velocity.x - rub.x;
    velocity.y = velocity.y - rub.y;
    velocity.z = velocity.z - rub.z;
    if (mostTurn < turn)
    {
        turn = mostTurn;
    }
    else if (turn < -mostTurn)
    {
        turn = -mostTurn;
    }

    s32 angle;
    AngleFrom(&angle, turn, AngleRadians);
    TurnBoard(this, RowOf(&board, 1), &angle);
    if (StandingNormal < normal->y)
    {
        onGround = 1;
    }
}

// When their boxes meet and the ellipsoid touches the hull: the push, the normal, and the contact point in the place's space
u32 HumiliskateVehicle::TouchesHull(const Matrix4x4* place, const Vector4* radii, const Matrix4x4* hullMatrix,
                                    const CollisionHull* hull, Vector4* push, Vector4* point, Vector4* normal)
{
    const Vector4* position = RowOf(place, 3);
    Box box = {*position, *position};
    GrowBox(reach + InstanceMargin, &box);
    Box hullBox;
    GetHullBoundingBox(hull, hullMatrix, &hullBox);
    if (BoxesOverlap(&box, &hullBox) == 0 || EllipsoidTouchesHull(hull, place, radii, hullMatrix, push, normal) == 0)
    {
        return 0;
    }

    Matrix4x4 inverse = *place;
    VuInvertRigidInPlace(&inverse);
    VuRotateVector(&inverse, push, point);
    Normalize(point);
    // Retail scales the whole point by minus each radius in turn: every component by all three
    const f32 scales[] = {-radii->x, -radii->y, -radii->z};
    for (f32 scale : scales)
    {
        point->z = point->z * scale;
        point->x = point->x * scale;
        point->y = point->y * scale;
    }

    return 1;
}

// The touched instance's object node told (whether it responds), the character bumped and, with a physics body, its object node
// told; a solid hull of an instance that responds (with its FlagSphereContact) pushes the board out with friction 1
void HumiliskateVehicle::TouchedHull(f32 seconds, const Vector4* point, const Vector4* push, const Vector4* normal,
                                     InstanceContext* instance, u32, u32 solid)
{
    u32 responds = 1;
    ObjectNode* node = ObjectNodeOf(instance);
    ObjectNode* riderNode = ObjectNodeOf(agent->instance);
    Vector4 contact;
    VuTransformPoint(&board, point, &contact);
    if (node != nullptr)
    {
        responds = CallVirtual<u32>(node, node->vtable, CollidedSlot, agent->instance, &contact, &velocity);
    }

    if (riderNode != nullptr)
    {
        Vector4 back = {-velocity.x, -velocity.y, -velocity.z, 1.0f};
        CallVirtual<void>(agent, agent->vtable, BumpedSlot, instance, &velocity, &back);
        if ((agent->instance->flags & ReferencedObject::FlagPhysicsBody) != 0)
        {
            back = {-velocity.x, -velocity.y, -velocity.z, 1.0f};
            CallVirtual<u32>(riderNode, riderNode->vtable, CollidedSlot, instance, &contact, &back);
        }
    }

    if ((instance->flags & ReferencedObject::FlagSphereContact) == 0)
    {
        responds = 0;
    }

    if (solid == 0 || responds == 0)
    {
        return;
    }

    Vector4* position = RowOf(&board, 3);
    position->x = position->x + push->x;
    position->y = position->y + push->y;
    position->z = position->z + push->z;
    GroundResponse(1.0f, seconds, normal);
}

// The hulls the board's ellipsoid touches send their surface's contact message and, solid, are touched; while not crouched the
// rider's smaller ellipsoid touching one tells the instance only
void HumiliskateVehicle::CollideHulls(f32 seconds, InstanceContext* instance)
{
    ObjectCollision* collision = &instance->collision;
    s32 count = GetHullCount(collision);
    // (the radii's w left as the stack had them)
    Vector4 radii;
    radii.x = radiusX;
    radii.y = radiusY;
    radii.z = radiusZ;
    Vector4 riderRadii;
    riderRadii.y = RiderRadiusY;
    riderRadii.z = RiderRadiusXZ;
    riderRadii.x = RiderRadiusXZ;
    for (s32 index = 0; index < count; index++)
    {
        CollisionHull* hull;
        Matrix4x4 hullMatrix;
        GetInstanceHull(collision, index, &hull, &hullMatrix);
        Vector4 push;
        Vector4 point;
        Vector4 normal;
        if (TouchesHull(&board, &radii, &hullMatrix, hull, &push, &point, &normal) != 0)
        {
            CollisionSurface* hullSurface = &g_CollisionSurfaces.surfaces[HullSurfaceIndex(collision, static_cast<u8>(index))];
            if ((hullSurface->collisionMask & SolidToPlayer) != 0)
            {
                TouchedHull(seconds, &point, &push, &normal, instance, index, 1);
            }

            agent->SendSurfaceMessage(hullSurface);
        }

        if (crouched != 0)
        {
            continue;
        }

        Matrix4x4 rider = board;
        const Vector4* boardUp = RowOf(&rider, 1);
        Vector4 lift = {boardUp->x * RiderLift, boardUp->y * RiderLift, boardUp->z * RiderLift, 1.0f};
        Vector4* position = RowOf(&rider, 3);
        position->x = position->x + lift.x;
        position->y = position->y + lift.y;
        position->z = position->z + lift.z;
        if (TouchesHull(&rider, &riderRadii, &hullMatrix, hull, &push, &point, &normal) != 0)
        {
            TouchedHull(seconds, &point, &push, &normal, instance, index, 0);
        }
    }
}

// The hulls of the instances around it but those both characters have attached and the other character
void HumiliskateVehicle::CollideInstances(f32 seconds)
{
    const Vector4* position = RowOf(&board, 3);
    Box box = {*position, *position};
    GrowBox(reach + InstanceMargin, &box);
    void* results[MostHits];
    InstanceRayHit query;
    query.results = results;
    query.most = MostHits;
    query.count = 0;
    query.distance = NoHit;
    query.wantedFlags = HitFlags;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    // (retail clears bits 0 and 1 and leaves the others as the stack had them)
    query.bits = 0;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    ChunkData* chunk = agent->instance->chunk;
    SkipInQuery(&query, agent->instance);
    QueryChunkInstances(chunk, &box, HitNodeKinds, &query);

    // (more than 19 attached overflow the list: retail. Retail doesn't check the other for none)
    InstanceContext* leftOut[MostLeftOut];
    s32 leftOutCount = 0;
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&agent->instance->nodes, AttachmentsKind));
    if (attachments != nullptr && attachments->path != nullptr)
    {
        leftOutCount = AttachedInstances(attachments->path, leftOut, MostLeftOut - 1);
    }

    attachments = static_cast<AttachmentsNode*>(GetGameNode(&other->instance->nodes, AttachmentsKind));
    if (attachments != nullptr && attachments->path != nullptr)
    {
        leftOutCount += AttachedInstances(attachments->path, &leftOut[leftOutCount], MostLeftOut - 1 - leftOutCount);
    }

    leftOut[leftOutCount++] = other->instance;
    for (s32 i = 0; i < query.count; i++)
    {
        auto* hit = static_cast<InstanceContext*>(query.results[i]);
        bool left = false;
        for (s32 k = 0; k < leftOutCount; k++)
        {
            if (leftOut[k] == hit)
            {
                left = true;
                break;
            }
        }

        if (!left)
        {
            CollideHulls(seconds, hit);
        }
    }
}

// The rails among the cache's triangles: an edge shared by two steep solid triangles across the board's way that face each other
// (a ridge) and that the board's ellipsoid touches. Their ends out (the first triangle's corners), the triangles marked used
// (nothing reads them): how many. Retail has room for 32 such triangles and checks neither them nor the rails
s32 HumiliskateVehicle::FindRails(CollisionHit** triangles, s32 count, u8* used, u32, Vector4* starts, Vector4* ends)
{
    Box boxes[MostRailTriangles];
    s16 candidates[MostRailTriangles];
    s32 rails = 0;
    Matrix4x4 toBoard = board;
    VuInvertRigidInPlace(&toBoard);
    Matrix4x4 toUnit;
    InitIdentityMatrix(&toUnit);
    toUnit.m[0][0] = 1.0f / radiusX;
    toUnit.m[1][1] = 1.0f / radiusY;
    toUnit.m[2][2] = 1.0f / radiusZ;
    s32 found = 0;
    for (s16 i = 0; i < count; i++)
    {
        CollisionHit* triangle = triangles[i];
        bool solid = (GetTriangleSurface(triangle)->collisionMask & SolidToPlayer) != 0;
        Vector4 normal;
        TriangleNormal(triangle, &normal);
        Vector4 outward = {-normal.x, -normal.y, -normal.z, 1.0f};
        if (!solid || SteepLimit < outward.y || outward.y < -SteepLimit)
        {
            continue;
        }

        if (AcrossLimit < __builtin_fabsf(Dot(RowOf(&board, 2), &outward)))
        {
            continue;
        }

        CollisionHit local = *triangle;
        TransformCollisionHit(&local, &toBoard);
        TransformCollisionHit(&local, &toUnit);
        Vector4 centre = {0.0f, 0.0f, 0.0f, 1.0f};
        Vector4 push;
        if (SphereTouchesTriangle(1.0f, &local, &centre, &push) == 0)
        {
            continue;
        }

        TriangleBox(&boxes[found], &triangle->vertices[0], &triangle->vertices[1], &triangle->vertices[2]);
        GrowBox(RailBoxMargin, &boxes[found]);
        candidates[found] = i;
        found++;
    }

    for (s16 a = 0; a < found; a++)
    {
        CollisionHit* first = triangles[candidates[a]];
        Vector4 normal;
        TriangleNormal(first, &normal);
        Vector4 firstNormal = {normal.x, normal.y, normal.z, 1.0f};
        for (s16 b = a + 1; b < found; b++)
        {
            CollisionHit* second = triangles[candidates[b]];
            if (BoxesOverlap(&boxes[a], &boxes[b]) == 0)
            {
                continue;
            }

            TriangleNormal(second, &normal);
            Vector4 secondNormal = {normal.x, normal.y, normal.z, 1.0f};
            f32 facing = Dot(&firstNormal, &secondNormal);
            if (!(RidgeLeast < facing) || !(facing < RidgeMost))
            {
                continue;
            }

            // The corners they share (first's index by remainder, second's by quotient)
            s32 shared[9];
            s32 sharedCount = 0;
            for (s32 k = 0; k < 9; k++)
            {
                if (DistanceSquared(&first->vertices[k % 3], &second->vertices[k / 3]) < SamePoint)
                {
                    shared[sharedCount++] = k;
                }
            }

            if (sharedCount != 2)
            {
                continue;
            }

            starts[rails] = first->vertices[shared[0] % 3];
            ends[rails] = first->vertices[shared[1] % 3];
            rails++;
            used[candidates[a]] = 1;
            used[candidates[b]] = 1;
        }
    }

    return rails;
}

// On the ground (not grinding) moving faster than 0.1 flat, a mark either side of its bottom; otherwise both trails idle
void HumiliskateVehicle::LaySkidMarks()
{
    if (onGround != 0 && grinding == 0)
    {
        Vector4 flat = velocity;
        Vector4 point = *RowOf(&board, 3);
        f32 flatSpeed = __builtin_sqrtf(flat.x * flat.x + flat.z * flat.z);
        flat.y = 0.0f;
        point.y = point.y - radiusY * SkidDrop;
        if (SkidSpeed < flatSpeed)
        {
            Normalize(&flat);
            Vector4 worldUp = {0.0f, 1.0f, 0.0f, 1.0f};
            Vector4 side;
            side.x = flat.y * worldUp.z - flat.z * worldUp.y;
            side.y = flat.z * worldUp.x - flat.x * worldUp.z;
            side.z = flat.x * worldUp.y - flat.y * worldUp.x;
            side.w = 1.0f;
            Vector4 direction = velocity;
            Normalize(&direction);
            f32 along = Dot(&direction, RowOf(&board, 2));
            point.x = point.x - up.x * SkidUpShift;
            point.z = point.z - up.z * SkidUpShift;
            f32 offset = __builtin_sqrtf(SkidWidthBase - along * along) * SkidWidthScale + SkidWidthLeast;
            Vector4 otherSide = {-side.x, -side.y, -side.z, side.w};
            LaySkidMark(offset, RandomSignedTimes(SkidJitter) + SkidDepth, &leftMarks, &point, &side, &cache);
            LaySkidMark(offset, RandomSignedTimes(SkidJitter) + SkidDepth, &rightMarks, &point, &otherSide, &cache);
            return;
        }
    }

    IdleSkidMarks(&leftMarks);
    IdleSkidMarks(&rightMarks);
}

// Landing squashes it by the fall (the board character's sounds play it); a damped spring within 0.5
void HumiliskateVehicle::Suspension(f32 seconds)
{
    if (onGround != 0 && wasOnGround == 0)
    {
        f32 fall = velocity.y - lastVelocity.y;
        f32 squash = fall * LandingSquash;
        if (squash < suspensionSpeed)
        {
            suspensionSpeed = squash;
        }

        CharacterSounds* sounds = BoardSounds(this);
        if (sounds != nullptr)
        {
            PlayCharacterSurfaceSound(sounds, fall);
        }
    }

    suspension = suspension + suspensionSpeed * seconds;
    suspensionSpeed = suspensionSpeed + (suspension * SuspensionSpring - suspensionSpeed * SuspensionDamping);
    if (MostSquash < suspension)
    {
        suspension = MostSquash;
        suspensionSpeed = 0.0f;
    }
    else if (suspension < -MostSquash)
    {
        suspension = -MostSquash;
        suspensionSpeed = 0.0f;
    }
}

// The ground's distance below (a ray to 4 ahead and 10 down) and the time the fall to it takes
void HumiliskateVehicle::FallTime(f32* time, f32* below)
{
    if (onGround != 0)
    {
        *time = 0.0f;
        *below = 0.0f;
        return;
    }

    const Vector4* position = RowOf(&board, 3);
    Vector4 start = *position;
    Vector4 end = *position;
    const Vector4 down = {0.0f, RayDrop, 0.0f, 1.0f};
    end.x = end.x + down.x;
    end.y = end.y + down.y;
    end.z = end.z + down.z;
    const Vector4* heading = RowOf(&board, 2);
    Vector4 ahead = {heading->x * RayAhead, heading->y * RayAhead, heading->z * RayAhead, 1.0f};
    end.x = end.x + ahead.x;
    end.y = end.y + ahead.y;
    end.z = end.z + ahead.z;
    Vector4 ground;
    if (GetCollisionCheck(agent->instance->chunk, &start, &end, SolidToProbes, nullptr, &ground, nullptr) != 0)
    {
        *below = __builtin_sqrtf(DistanceSquared(&start, &ground));
    }
    else
    {
        *below = NoGround;
    }

    // The root of v t - g t² / 2 = -h
    f32 rise = velocity.y;
    *time = (-rise - __builtin_sqrtf(rise * rise + (gravity + gravity) * *below)) / -gravity;
}

// The height below kept. Off a ramp (not a jump) high enough a flip or a half spin starts by the fall's time (both characters
// told); the angles eased to their targets (another turn while there's time, else landing; each half spin turns it round). The
// targets rounded to whole half turns (spins) and turns (flips), cut toward 0 (retail: below -0.5 they round up)
void HumiliskateVehicle::Tricks(f32 seconds)
{
    f32 fallTime;
    f32 below;
    FallTime(&fallTime, &below);
    height = below;
    if (trick == TrickRiding)
    {
        if (TrickAirTime < airTime && TrickHeight < below && jumpedOffRail == 0 && jumped == 0)
        {
            if (FlipTime < fallTime)
            {
                trick = TrickFlip;
                flipTarget = flip + TwoPi;
            }
            else if (SpinTime < fallTime)
            {
                trick = TrickSpin;
                spinTarget = spin + Pi;
            }
        }

        if (trick == TrickFlip || trick == TrickSpin)
        {
            u32 started = trick == TrickFlip ? EventFlip : EventSpin;
            RunOnRider(this, started);
            RunOnBoard(this, started);
            otherEvent = EventNone;
            event = EventNone;
        }
    }

    if (trick == TrickFlip)
    {
        f32 turned = flip + seconds * ((flipTarget - flip) * FlipEase + FlipRate);
        if (!(flipTarget < turned))
        {
            flip = turned;
        }
        else if (fallTime < FlipTime)
        {
            trick = TrickLanding;
        }
        else
        {
            flipTarget = flipTarget + TwoPi;
        }
    }
    else if (trick == TrickSpin)
    {
        f32 turned = spin + ((spinTarget - spin) * SpinEase * seconds + seconds);
        if (!(spinTarget < turned))
        {
            spin = turned;
        }
        else
        {
            backwards = backwards ^ 1;
            if (fallTime < SpinTime)
            {
                trick = TrickLanding;
            }
            else
            {
                spinTarget = spinTarget + Pi;
            }
        }
    }

    spinTarget = static_cast<f32>(static_cast<s32>(spinTarget * InversePi + 0.5f)) * Pi;
    flipTarget = static_cast<f32>(static_cast<s32>(flipTarget * InverseTwoPi + 0.5f)) * TwoPi;
}

// The animation events of the trick state sent to both characters (none but riding and grinding), the state moved on: a landing
// trick tells them and shows for 0.5 seconds, leaving a rail 0.2
void HumiliskateVehicle::SendEvents(f32 seconds)
{
    u32 riderEvent = EventNone;
    u32 boardEvent = EventNone;
    switch (trick)
    {
    case TrickRiding:
        if (crouched != 0)
        {
            riderEvent = leanLeft != 0 ? EventCrouchedLeft : leanRight != 0 ? EventCrouchedRight : EventCrouched;
        }
        else
        {
            riderEvent = RidingEvent(this);
        }

        boardEvent = RidingEvent(this);
        break;
    case TrickLanding:
        trick = TrickLanded;
        trickTime = 0.0f;
        RunOnRider(this, EventTrickLanded);
        RunOnBoard(this, RidingEvent(this));
        otherEvent = EventNone;
        event = EventNone;
        break;
    case TrickLanded:
        trickTime = trickTime + seconds;
        if (LandedTime < trickTime)
        {
            trick = TrickRiding;
        }

        break;
    case TrickGrinding:
        riderEvent = backwards != 0 ? EventGrindingBackwards : EventGrinding;
        boardEvent = riderEvent;
        break;
    case TrickOffRail:
        trickTime = trickTime + seconds;
        if (OffRailTime < trickTime)
        {
            trick = TrickRiding;
        }

        break;
    }

    event = riderEvent;
    RunOnRider(this, riderEvent);
    otherEvent = boardEvent;
    RunOnBoard(this, boardEvent);
}
