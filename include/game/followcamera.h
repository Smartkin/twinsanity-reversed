#pragma once

#include "abi.h"
#include "common.h"
#include "game/bindings.h"
#include "game/camerablender.h"
#include "game/camerarig.h"
#include "game/cameras.h"

class CharacterAgent;
class Vehicle;
struct ChunkEntry;
struct CollisionCache;
struct GamePad;
struct ObjectPlace;
struct Reference;
class KeyedCamera;

// The follow camera's four probes, in the order they're cast in: above and below the view, to its left and to its right (the
// order of the probes' ends)
enum FollowProbe : u32
{
    ProbeAbove = 0,
    ProbeBelow = 1,
    ProbeLeft = 2,
    ProbeRight = 3,
    ProbeCount = 4,
};

// What the follow camera's probes found (FollowCameraPositioner::Probe): which hit, both of a pair (both sides with neither above
// nor below hitting count as below too), and that it should pull in
union ProbeHits
{
    u32 value;
    struct
    {
        u32 above : 1;
        u32 below : 1;
        u32 left : 1;
        u32 right : 1;
        u32 unused4 : 1;
        u32 pair : 1;
        u32 pullIn : 1;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(ProbeHits, 4);

// The curve the follow camera's positioner blends to its trigger's place by, and its target to its trigger's point: even, or a
// cube of the time's share. Nothing sets one: the blends are even
enum FollowBlendCurve : u32
{
    FollowCurveEven = 0,
    FollowCurveCubic = 2,
};

// The follow camera's positioner's bits (retail reads and writes the 64 bits whole). A restart clears the low half (but
// distanceFollowsPitch and tilts): what it took from its camera trigger and the rig
union FollowPositionerBits
{
    u64 value;
    struct
    {
        // It goes to its camera trigger's place (else behind the target), its distance follows the pitch (between its ends as
        // the pitch is between its), the probes start at the target point (the trigger has a first subtype; else around the
        // instance)
        u64 atTriggerPlace : 1;
        u64 distanceFollowsPitch : 1;
        u64 probesFromTarget : 1;
        // The rig's look stick turns it (the rig's stickTurning): its probes pull it in when only some hit and it's further than
        // 2, and unless its trigger always takes values the target counts as moving and camera triggers' values are ignored
        u64 stickTurning : 1;
        // The last trigger's probesFromTarget (nothing reads it)
        u64 unused4 : 1;
        // Its trigger's alwaysTakesValues (the stick's input not taken either)
        u64 alwaysTakesValues : 1;
        // The character moves (the rig's moving): camera triggers' values are taken again once the stick's let go
        u64 characterMoving : 1;
        // Camera triggers' values aren't taken (they only change its bits)
        u64 ignoresValues : 1;
        // It tilts, its pitch's goal is the default pitch kept within its ends (else 0)
        u64 tilts : 1;
        // Cleared with the trigger's bits
        u64 unused9 : 1;
        // Its trigger set the yaw's speed (the rig's walk doesn't)
        u64 yawSpeedSet : 1;
        // It takes its own camera instead of the triggers'
        u64 ownCamera : 1;
        // Its trigger's holdsStill (the rig keeps its rotation: it doesn't move), keepsHeight (it keeps its height looking at the
        // target), onlyLooksAtTarget
        u64 holdsStill : 1;
        u64 keepsHeight : 1;
        u64 onlyLooksAtTarget : 1;
        // Set back this step: changing triggers cuts (and it doesn't tilt)
        u64 restarted : 1;
        // Its tilt doesn't follow the target's facing (its trigger doesn't tilt, a vehicle's view)
        u64 noFacingTilt : 1;
        // Its place follows at the trigger's rate (its camera's position follow rate, else at its own)
        u64 atTriggerRate : 1;
        u64 unused18 : 1;
        // The rig's look stick turned it (until the character moves: its tilt doesn't follow the target's facing)
        u64 stickTurned : 1;
        // Its view of the target isn't checked (its trigger's skipsViewCheck, a second subtype or a fixed yaw, the character
        // dead), its yaw has the extra added (its trigger's addsExtraYaw)
        u64 skipsViewCheck : 1;
        u64 addsExtraYaw : 1;
        u64 unused22 : 10;
        // The probes are off (its trigger's noProbes, the target still)
        u64 probesOff : 1;
        // It was pushed off the collision, out of an instance's hull
        u64 pushedOffCollision : 1;
        u64 pushedOutOfHull : 1;
        // Its view's been blocked over 0.2 seconds, at all (since the time kept), over 0.7 seconds
        u64 blockedAWhile : 1;
        u64 blocked : 1;
        u64 blockedLong : 1;
        // Nothing pushed it
        u64 unpushed : 1;
        // The probes' last results, and the probe cast next (a FollowProbe)
        u64 leftHit : 1;
        u64 rightHit : 1;
        u64 aboveHit : 1;
        u64 belowHit : 1;
        u64 nextProbe : 2;
        // The push brought it no nearer the target (its view counts as blocked)
        u64 pushedBack : 1;
        // It steers with its probes (the follow camera's steers), its trigger's steers: the probes are on while it and either
        // that or ignoresValues are
        u64 steers : 1;
        u64 triggerSteers : 1;
        // The target stands still
        u64 targetStill : 1;
        u64 unused49 : 15;
    };
};
CHECK_SIZE(FollowPositionerBits, 8);

// The follow camera's positioner's state
union FollowPositionerState
{
    u32 value;
    struct
    {
        // It blends to its trigger's place (from where it was when the blend started), the blend's started, its curve (a
        // FollowBlendCurve)
        u32 blending : 1;
        u32 blendStarted : 1;
        u32 curve : 3;
        u32 unused5 : 1;
        // It cuts (no smoothing), it was placed by a step
        u32 cut : 1;
        u32 placed : 1;
        // A blend asked for without a trigger (2 seconds, to the defaults)
        u32 blendAsked : 1;
        // Its blenders' speeds were set to take the blend's time, and set back
        u32 timed : 1;
        u32 timeEnded : 1;
        // Its trigger's blendsAlongLine: the place is blended along the line from where the blend started
        u32 alongLine : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(FollowPositionerState, 4);

// The player's camera's positioner (TT Lab's camera controller, 0x400 bytes, retail's vtable D_00304DA8: 7 its rotation tilted
// toward where the target is going). It keeps the camera behind the target at a pitch, a yaw, a field of view and a distance
// (its four blenders), or at the place its camera trigger's second subtype gives (blended to from where it was over the
// trigger's blend time), and away from walls: four probes from around the target to around the camera (above, below, left and
// right, cast in turn) turn and pull it in, it's pushed off the collision and out of the instances' hulls, and when the view of
// the target stays blocked it jumps to a clear place. Its bits and state, the instance it follows and the target, the
// trigger's place and the blend's start, the probes' ends (around both, between their low and high pitch values by the pitch
// blender's share), the tilt (its pitch and yaw, eased toward the target's facing and its sideways turn), the camera trigger it
// took and a camera of its own, the collision near the camera, the rates its place follows at (the trigger's or its own: the
// share of the way a second), how much of a turn it takes a step, how far it backs off when it looks at the target's front, the
// radiuses it keeps from the collision and the hulls
class FollowCameraPositioner : public CameraPositioner
{
public:
    // Its vtable's function past the positioners'
    static constexpr u32 TiltSlot = 7;

    FollowPositionerBits bits;
    FollowPositionerState state;
    InstanceContext* instance;
    CameraTarget* target;
    AngleBlender pitch;
    AngleBlender fieldOfView;
    AngleBlender yaw;
    DistanceBlender distance;
    u8 unused194[0xC];
    Vector4 triggerPlace;
    Vector4 blendFrom;
    u32 blendStart;
    s32 blendTicks;
    f32 unused1C8;
    // How fast the probes turn it (65536ths of a turn a second)
    s32 pitchPushRate;
    s32 yawPushRate;
    // The probes' results since its last step (their ProbeHits; nothing reads them)
    u8 unused1D4;
    u8 unused1D5[3];
    // The probes' sideways ends (by FollowProbe: the first two up, the others across), around the target and around the camera,
    // at its low pitch and its high
    f32 lowTargetSides[ProbeCount];
    f32 lowCameraSides[ProbeCount];
    f32 highTargetSides[ProbeCount];
    f32 highCameraSides[ProbeCount];
    u8 unused218[8];
    // Around the instance (when the probes don't start at the target point) and around the camera, at the low and the high pitch
    Vector4 lowTargetOffset;
    Vector4 highTargetOffset;
    Vector4 lowCameraOffset;
    Vector4 highCameraOffset;
    Vector4 probeTargetEnds[ProbeCount];
    Vector4 probeCameraEnds[ProbeCount];
    s32 tiltPitch;
    s32 tiltYaw;
    s32 lastYaw;
    CameraNode* trigger;
    MainCamera camera;
    CollisionCache* cache;
    u8 unused384;
    u8 unused385[3];
    f32 stepSeconds;
    f32 triggerRate;
    f32 ownRate;
    f32 turnShare;
    f32 unused398;
    u32 blockedSince;
    u8 unused3A0[0x10];
    // How much it faces the target's front (-1 to 0) and how far the target turned sideways (-1 to 1)
    f32 facing;
    f32 sideways;
    f32 backOff;
    f32 hullRadius;
    f32 collisionRadius;
    // The lowest the lower probe's end around the target may be
    f32 probeFloor;
    s32 roll;
    // A keyed camera (CameraSubtype::TypeKeyed) plays
    u8 keyed;
    u8 unused3CD[3];
    s32 yawExtra;
    u8 unused3D4[0xC];
    Vector4 lastPosition;
    // An instance the view checks leave out
    ReferencedObject* ignored;
    u8 unused3F4[0xC];

    static FollowCameraPositioner* Construct(FollowCameraPositioner* positioner, f32 distance, const s32* pitch)
        RETAIL_N32(FUN_00273738);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027c480);
    // Set back to its start for an instance (a cache of the collision near it made) and a target, and set back to its start:
    // its blenders at their initial values with the yaw behind the target, at the place that gives (the target point without a
    // target), the probes' and the pushes' state cleared, moved to a clear place when its view is blocked
    void Reset(InstanceContext* instance, CameraTarget* target) RETAIL(FUN_0027c4f0);
    void Restart() RETAIL(FUN_00273a70);
    // Its distance and pitch set (the distance's ends both the distance) and set back
    void SetDistanceAndPitch(f32 distance, const s32* pitch) RETAIL_N32(FUN_0027c560);
    void Step(TimeClock* clock, CameraTarget* target) RETAIL(FUN_00274048);
    // A camera trigger's node taken with the value along its geometry and the target (none: back to its defaults)
    void Take(f32 value, CameraNode* trigger, CameraTarget* target) RETAIL_N32(FUN_00274b20);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00274820);
    u32 KeepsRigRotation() RETAIL(FUN_0027c408);
    // Its rotation turned by its tilt (eased toward the target's facing and sideways turn while it tilts)
    void Tilt(Vector4* position, Vector4* rotation) RETAIL(FUN_00275978);

    void TiltVirtual(Vector4* position, Vector4* rotation)
    {
        CallVirtual<void>(this, vtable, TiltSlot, position, rotation);
    }

    // A camera's values taken (its bits always, its values unless it ignores them: the blenders' ranges, blended over the blend
    // time unless the camera cuts, and the second subtype's place)
    void TakeCamera(f32 value, MainCamera* camera, CameraTarget* target) RETAIL_N32(FUN_00274e08);
    // Back to its defaults after a trigger's camera (over the blend time unless it cuts)
    void BlendBack(CameraNode* last, u32 time) RETAIL(FUN_00275cf0);
    // What it took from its trigger's camera dropped (its bits and blend, the blenders' second ranges, the trigger's rate)
    void ClearTriggerValues() RETAIL(FUN_002749c8);
    // At the place behind the target its angles and distance give (the angles from where it is), cutting
    void PlaceBehind() RETAIL(FUN_00273e98);
    // The tilt on (its pitch's goal the default pitch)
    void SetTilts(u32 tilts) RETAIL(FUN_0027c5b8);
    void ResetTurnShare() RETAIL(FUN_0027c800);
    void ResetOwnRate() RETAIL(FUN_0027c810);

    // The yaw behind the target (its rotation's, or the way it moves when it faces that way; the last one when it doesn't move),
    // returned through the first argument
    static s32* TargetYaw(s32* yaw, FollowCameraPositioner* positioner) RETAIL(FUN_00271a10);
    // The place its blenders give behind a target point (kept when asked)
    void PlaceFor(const Vector4* target, Vector4* out, u32 commit) RETAIL(FUN_00271b28);
    // Moved toward a place looking at a target point (at its rate's share; at once when it cuts), or turned to look at it
    void MoveToward(const Vector4* target, const Vector4* place, u32 jumped) RETAIL(FUN_00271d00);
    // The probes for a target point and a place, cast in turn from the next one on (the lower one not while its end around the
    // target is below the floor, which holds the others up; their results kept when asked), the rest as they last hit: their
    // ProbeHits
    u32 Probe(const Vector4* target, const Vector4* place, u32 keep) RETAIL(FUN_00272350);
    // The turns the probes ask for (the pitch's and the yaw's rates) and the pull (-20 a second): the probes' results
    u32 ProbeRates(const Vector4* target, s32* pitchRate, s32* yawRate, f32* distanceRate, u32 keep) RETAIL(FUN_00272c48);
    u32 PushByProbes(TimeClock* clock, const Vector4* target) RETAIL(FUN_00272d98);
    // The blenders stepped toward their goals (or to the trigger's place): whether any moves
    u32 StepBlenders(TimeClock* clock) RETAIL(FUN_00273010);
    // Where it's turned from the target's facing (its facing and sideways values)
    void MeasureFacing() RETAIL(FUN_002757b0);
    // A place pushed out of the instances' hulls (the radius growing while they push): whether any did
    u32 PushOutOfHulls(const Vector4* place, const Vector4* from, Vector4* out) RETAIL(FUN_00275f80);
    // A place moved back to where the segment from its position hits the collision and pushed off the collision's triangles
    // around it: whether any pushed it
    u32 PushOffCollision(const Vector4* place, const Vector4* target, Vector4* out) RETAIL(FUN_00276358);
    // A segment cast through the collision (the camera's surfaces, or the lines of sight's), with the instances when asked:
    // whether it hits, where
    u32 CastView(const Vector4* from, const Vector4* to, Vector4* hit, u32 instances, u32 lineOfSight) RETAIL(FUN_002767d8);
    // A clear place for the camera from a point: the place, else 10 behind the target at the default pitch, else at its pitch
    // (each pulled back from where the view stops when that's further than 2): whether one was found
    u32 FindClearPlace(const Vector4* place, const Vector4* from, Vector4* out, u32 instances) RETAIL(FUN_00276910);
    // Whether its view of the target from a point is blocked (always when it was pushed back, when it was pushed and is within
    // 2 of the other point; never when its view isn't checked)
    u32 ViewBlocked(const Vector4* point, const Vector4* other) RETAIL(FUN_00276fd8);
    // A keyed camera's place taken (eased toward once it plays): when it's done, back to following
    void FollowKeyed(KeyedCamera* camera) RETAIL(FUN_00277108);
    // Whether the target stands still (the trigger's rate then 0)
    void CheckTargetStill() RETAIL(FUN_00277250);
    // How far it backs off eased toward twice its facing
    void EaseBackOff() RETAIL(FUN_0027c5d8);
    // The blend's speeds given back to the blenders
    void EndBlend() RETAIL(FUN_0027c608);
    // The probes' results and their counter cleared
    void ClearProbes() RETAIL(FUN_0027c658);
    // Whether its view stays blocked (from when it started), cleared when it isn't
    void TimeBlocked(const Vector4* place, const Vector4* target, const Vector4* unused, TimeClock* clock) RETAIL(FUN_0027c6c8);
    void ClearBlocked() RETAIL(FUN_0027c7b8);
    // Whether the segment hits the collision's camera surfaces
    u32 SegmentBlocked(const Vector4* from, const Vector4* to) RETAIL(FUN_0027c450);
};
CHECK_OFFSET(FollowCameraPositioner, bits, 0x40);
CHECK_OFFSET(FollowCameraPositioner, instance, 0x4C);
CHECK_OFFSET(FollowCameraPositioner, pitch, 0x54);
CHECK_OFFSET(FollowCameraPositioner, fieldOfView, 0xA4);
CHECK_OFFSET(FollowCameraPositioner, yaw, 0xF4);
CHECK_OFFSET(FollowCameraPositioner, distance, 0x144);
CHECK_OFFSET(FollowCameraPositioner, triggerPlace, 0x1A0);
CHECK_OFFSET(FollowCameraPositioner, blendTicks, 0x1C4);
CHECK_OFFSET(FollowCameraPositioner, unused1D4, 0x1D4);
CHECK_OFFSET(FollowCameraPositioner, lowTargetSides, 0x1D8);
CHECK_OFFSET(FollowCameraPositioner, lowTargetOffset, 0x220);
CHECK_OFFSET(FollowCameraPositioner, probeTargetEnds, 0x260);
CHECK_OFFSET(FollowCameraPositioner, tiltPitch, 0x2E0);
CHECK_OFFSET(FollowCameraPositioner, camera, 0x2F0);
CHECK_OFFSET(FollowCameraPositioner, cache, 0x380);
CHECK_OFFSET(FollowCameraPositioner, stepSeconds, 0x388);
CHECK_OFFSET(FollowCameraPositioner, blockedSince, 0x39C);
CHECK_OFFSET(FollowCameraPositioner, facing, 0x3B0);
CHECK_OFFSET(FollowCameraPositioner, roll, 0x3C8);
CHECK_OFFSET(FollowCameraPositioner, yawExtra, 0x3D0);
CHECK_OFFSET(FollowCameraPositioner, lastPosition, 0x3E0);
CHECK_OFFSET(FollowCameraPositioner, ignored, 0x3F0);
CHECK_SIZE(FollowCameraPositioner, 0x400);

// The follow camera's target's bits
union FollowTargetBits
{
    u32 value;
    struct
    {
        // It goes to its trigger's point (the first subtype's, else its own), blends to it, blends now
        u32 atCameraPoint : 1;
        u32 blends : 1;
        u32 blending : 1;
        // Its box is the given one (its trigger's givesTargetBox: the camera's target box), it frames the trigger's instances
        // (framesInstances: by the camera's framing share, up to its framing distance), its height isn't eased
        u32 givenBox : 1;
        u32 framesInstances : 1;
        u32 heightUneased : 1;
        // Its blend's curve (a FollowBlendCurve)
        u32 curve : 3;
        // The trigger's point is its own (no first subtype): the eased point
        u32 pointFollows : 1;
        // The rig's look stick turns the camera (nothing reads it)
        u32 unused10 : 1;
        u32 unused11 : 1;
        // Its box blends to the trigger's (over the blend time), and settled
        u32 boxBlending : 1;
        // It cuts (for its next step), it was set back, it stepped
        u32 cut : 1;
        u32 wasReset : 1;
        u32 stepped : 1;
        u32 settled : 1;
        // Its trigger's blendsWhenNear (it blends even when the point is near) and targetBoxUnturned (the box's point isn't
        // turned with the object), the last trigger's targetBoxUnturned
        u32 blendsWhenNear : 1;
        u32 boxUnturned : 1;
        u32 lastBoxUnturned : 1;
        // It takes its own camera instead of the triggers'
        u32 ownCamera : 1;
        u32 unused21 : 11;
    };
};
CHECK_SIZE(FollowTargetBits, 4);

// The player's camera's target (0x210 bytes, retail's vtable D_00304DF0): the followed instance's place (or a fixed one), its
// point moved toward the centre of its camera trigger's instances by a share up to a distance, its height eased (rising at 5
// a second up to the ground's, the point's otherwise) plus the box's point at a share (its own box, or a given one, blended to
// over the trigger's blend time) turned with the object, or the trigger's first subtype's point (blended to over the blend
// time). Its bits, the reference to the instance, the trigger taken and the one before, the box at the blend's start, a camera
// of its own
class FollowCameraTarget : public CameraTarget
{
public:
    FollowTargetBits bits;
    Reference* followed;
    ObjectPlace* fixedPlace;
    CameraNode* trigger;
    f32 framingShare;
    f32 framingDistance;
    f32 groundHeight;
    f32 easedHeight;
    f32 boxShare;
    u8 unused94[0xC];
    Vector4 boxOffset;
    Vector4 easedPoint;
    Vector4 cameraPoint;
    u32 blendStart;
    s32 blendTicks;
    u8 unusedD8[8];
    Vector4 blendFrom;
    Vector4 boxMin;
    Vector4 boxMax;
    Vector4 givenMin;
    Vector4 givenMax;
    Vector4 startMin;
    Vector4 startMax;
    Vector4 currentMin;
    Vector4 currentMax;
    CameraNode* lastTrigger;
    u8 unused174[0xC];
    MainCamera camera;

    static FollowCameraTarget* Construct(FollowCameraTarget* target) RETAIL(FUN_00277700);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027c830);
    void Reset() RETAIL(FUN_00277828);
    void Step(TimeClock* clock) RETAIL(FUN_00277b10);
    // A camera trigger's node taken (its camera's values; its own camera's instead when it uses it): the value along the
    // trigger's geometry
    f32 Value(CameraNode* node) RETAIL(FUN_002782e0);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0027c898);
    // A camera's values taken: the value its first subtype gives
    f32 TakeCamera(MainCamera* taken) RETAIL(FUN_002785c0);
    // The point of an object's place (moved toward the trigger's instances when it frames them)
    void ObjectPoint(ObjectPlace* place, Vector4* out) RETAIL(FUN_002774b8);
    // A point's height eased (over the seconds of a step)
    void EaseHeight(f32 seconds, const Vector4* point, Vector4* out) RETAIL_N32(FUN_00277398);
    // What it took from its trigger's camera dropped (smoothed again)
    void ClearTriggerValues() RETAIL(FUN_0027c980);
    // A box given (and its use dropped)
    void SetBox(const Vector4* min, const Vector4* max) RETAIL(FUN_0027ca18);
    void ClearBox() RETAIL(FUN_0027ca00);
};
CHECK_OFFSET(FollowCameraTarget, bits, 0x70);
CHECK_OFFSET(FollowCameraTarget, followed, 0x74);
CHECK_OFFSET(FollowCameraTarget, groundHeight, 0x88);
CHECK_OFFSET(FollowCameraTarget, boxOffset, 0xA0);
CHECK_OFFSET(FollowCameraTarget, blendStart, 0xD0);
CHECK_OFFSET(FollowCameraTarget, blendFrom, 0xE0);
CHECK_OFFSET(FollowCameraTarget, currentMin, 0x150);
CHECK_OFFSET(FollowCameraTarget, lastTrigger, 0x170);
CHECK_OFFSET(FollowCameraTarget, camera, 0x180);
CHECK_SIZE(FollowCameraTarget, 0x210);

extern "C"
{
    extern const GccVTableEntry g_FollowCameraTargetVTable[] RETAIL(D_00304DF0);
    extern const GccVTableEntry g_FollowCameraPositionerVTable[] RETAIL(D_00304DA8);
    // The pitch the follow camera goes back to (15 degrees) and how fast its tilt eases (90 degrees a second), set at start-up
    extern s32 g_FollowCameraPitch RETAIL(D_0030A9D8);
    extern s32 g_CameraTiltRate RETAIL(D_0030A9C0);
}

// A camera rig the pad steers (retail's D_002F3808, 0x50 bytes, abstract; vtable 0x30 bytes in: 1 the destructor, 2-4 the camera
// rig's, 5 made ready for a character, 6 back to its defaults, 7 put together, 8 its input cleared, 9 its input read from a pad, 10
// a frame with the character, 11 the look stick ((0, 0) here)): the bindings it reads the pad with. Only the follow camera's rig
// derives from it
class PadCameraRig : public CameraRig
{
public:
    enum Slot : u32
    {
        ClearInputSlot = 8,
        ReadPadSlot = 9,
        FrameSlot = 10,
        LookStickSlot = 11,
    };

    ButtonBindings bindings;
    u32 unused4C;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0015df20);
    void LookStick(f32* x, f32* y) RETAIL(FUN_0015df70);

    void PrepareVirtual(void* controller, InstanceContext* camera, InstanceContext* character)
    {
        CallVirtual<void>(this, vtable, PrepareSlot, controller, camera, character);
    }

    void RestoreDefaultsVirtual(CharacterAgent* character)
    {
        CallVirtual<void>(this, vtable, RestoreDefaultsSlot, character);
    }

    void AssembleVirtual()
    {
        CallVirtual<void>(this, vtable, AssembleSlot);
    }

    void ClearInputVirtual()
    {
        CallVirtual<void>(this, vtable, ClearInputSlot);
    }

    void ReadPadVirtual(GamePad* pad)
    {
        CallVirtual<void>(this, vtable, ReadPadSlot, pad);
    }

    void FrameVirtual(TimeClock* clock, CharacterAgent* character)
    {
        CallVirtual<void>(this, vtable, FrameSlot, clock, character);
    }

    void LookStickVirtual(f32* x, f32* y)
    {
        CallVirtual<void>(this, vtable, LookStickSlot, x, y);
    }
};
CHECK_OFFSET(PadCameraRig, bindings, 0x40);
CHECK_SIZE(PadCameraRig, 0x50);

// The follow camera rig's bits
union FollowRigBits
{
    u32 value;
    struct
    {
        // Its blenders hold (the yaw's, the pitch's, the distance's), it tilts
        u32 yawHolds : 1;
        u32 pitchHolds : 1;
        u32 distanceHolds : 1;
        u32 tilts : 1;
        // The kind of the vehicle it's set up for (a Vehicle::Kind, 0 on foot; a passenger's 8 is 0 too)
        u32 vehicle : 3;
        u32 unused7 : 1;
        // The look stick turns it now, turned it (until the character moves), the character moves
        u32 stickTurning : 1;
        u32 stickTurned : 1;
        u32 moving : 1;
        u32 unused11 : 21;
    };
};
CHECK_SIZE(FollowRigBits, 4);

// The follow camera's rig (retail's D_002F37A0, 0x700 bytes): the pad's input (the shoulders' pressure and two unbound actions;
// the right stick, an unbound zoom axis, the left stick and the d-pad), its own followers, target and positioner, and the walk's
// yaw speed. Its bits: the yaw's, pitch's and distance's holds, tilting, the vehicle it's set up for, the stick turning it now and
// since, the character moving
class FollowCameraRig : public PadCameraRig
{
public:
    // pressures[]: action 0 (L1, L2, R1, R2: the yaw's rate scale while the character stands), 1 and 2 (no buttons: the pitch's
    // and the distance's rate scales)
    enum Pressure : u32
    {
        PressureShoulders = 0,
        PressurePitch = 1,
        PressureDistance = 2,
    };

    // axes[]: the right stick's x and y, nothing bound (the distance's input), the left stick's (and the d-pad's) y and x
    enum Axis : u32
    {
        AxisLookX = 0,
        AxisLookY = 1,
        AxisZoom = 2,
        AxisMoveY = 3,
        AxisMoveX = 4,
    };

    FollowRigBits bits;
    f32 pressures[3];
    f32 axes[5];
    u8 unused74[0xC];
    CameraPointFollower ownTargetFollower;
    CameraPointFollower ownCameraFollower;
    FollowCameraTarget ownTarget;
    FollowCameraPositioner ownPositioner;
    // 65536ths of a turn a second, eased a tenth of the way a frame toward what the left stick's angle asks
    s32 walkYawSpeed;
    u8 unused6F4[0xC];

    static FollowCameraRig* Construct(FollowCameraRig* rig) RETAIL(FUN_00141d08);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015e090);
    // The controller and the camera's instance unread
    void Prepare(void* controller, InstanceContext* camera, InstanceContext* character) RETAIL(FUN_00141ed8);
    void RestoreDefaults(CharacterAgent* character) RETAIL(FUN_00142260);
    void Assemble() RETAIL(FUN_0015e120);
    void ClearInput() RETAIL(FUN_0015e148);
    void ReadPad(GamePad* pad) RETAIL(FUN_0015e190);
    // The clock unread
    void Frame(TimeClock* clock, CharacterAgent* character) RETAIL(FUN_00142360);
    // The right stick's x and y
    void LookStick(f32* x, f32* y) RETAIL(FUN_0015e268);

    // Tilting or not; tilting, its ranges, rates and the target's box back to the defaults
    void SetTilts(u32 tilts) RETAIL(FUN_001425d8);
    // Set up for what the character rides (its bits' vehicle), the holds and the yaw stepped, the target given the velocity
    void FollowRide(CharacterAgent* character) RETAIL(FUN_00142820);
    void SetMechaView() RETAIL(FUN_00142a88);
    // The yaw's speed by the Rollerbrawl's (the character unread)
    void RollerbrawlYaw(CharacterAgent* character, f32 speed) RETAIL_N32(FUN_00142af8);
    void SetRollerbrawlView() RETAIL(FUN_00142bb0);
    void SetHumiliskateView() RETAIL(FUN_00142c98);
    void StepWalkYaw(CharacterAgent* character) RETAIL(FUN_00142e38);
    void UpdateHolds() RETAIL(FUN_00143240);
    void SetTarget(InstanceContext* instance) RETAIL(FUN_00143368);
    // The second argument unread
    void CheckMoving(CharacterAgent* character, u32 unused) RETAIL(FUN_0015e280);
    void RollerbrawlFrame(CharacterAgent* character, f32 speed) RETAIL_N32(FUN_0015e330);
    void SetHoverboardView() RETAIL(FUN_0015e428);
    // Nothing
    void HoverboardFrame(CharacterAgent* character) RETAIL(FUN_0015e448);
    // The vehicle unread
    void HumiliskateFrame(Vehicle* vehicle) RETAIL(FUN_0015e450);
    // The linked character left out of the view checks
    void IgnoreLinked(CharacterAgent* character) RETAIL(FUN_0015e468);
    // Started again: its parts set back, put together (slot 7, through the vtable)
    void Restart() RETAIL(FUN_0015e4a8);
};
CHECK_OFFSET(FollowCameraRig, bits, 0x50);
CHECK_OFFSET(FollowCameraRig, axes, 0x60);
CHECK_OFFSET(FollowCameraRig, ownTargetFollower, 0x80);
CHECK_OFFSET(FollowCameraRig, ownTarget, 0xE0);
CHECK_OFFSET(FollowCameraRig, ownPositioner, 0x2F0);
CHECK_OFFSET(FollowCameraRig, walkYawSpeed, 0x6F0);
CHECK_SIZE(FollowCameraRig, 0x700);

// The follow node's camera's bits
union FollowCameraBits
{
    u32 value;
    struct
    {
        // Its rig's points are followed smoothly, its positioner steers with its probes, its rig takes nothing from the camera
        // triggers
        u32 smoothed : 1;
        u32 steers : 1;
        u32 ignoresTriggers : 1;
        // Nothing sets switchBack (a step would flip rigAway and blend the lens back to its rig); rigAway: its rig isn't the
        // lens's
        u32 switchBack : 1;
        u32 rigAway : 1;
        // Its rig's frame is stepped
        u32 stepsRig : 1;
        // The character died (the second slot's cameras are taken from then on)
        u32 characterDied : 1;
        // It took the second slot's camera (the character dead): the scripts don't set its target (CameraNodeSetTarget)
        u32 keepsTarget : 1;
        u32 unused8 : 24;
    };

    // The settings a restart keeps
    enum Mask : u32
    {
        Smoothed = 0x1,
        Steers = 0x2,
        IgnoresTriggers = 0x4,
        StepsRig = 0x20,
        KeptByRestart = Smoothed | Steers | IgnoresTriggers | StepsRig,
    };
};
CHECK_SIZE(FollowCameraBits, 4);

// The follow node's camera (0x730 bytes, no vtable): its bits, the camera triggers (the one chosen last frame, this frame's, the
// last step's, the second slot's), the chosen one's priority (its trigger's kind byte), its rig and the rig it put on the camera's
// lens
struct FollowCamera
{
    FollowCameraBits bits;
    CameraNode* current;
    CameraNode* pending;
    CameraNode* last;
    CameraNode* secondSlot;
    u32 priority;
    u8 unused18[8];
    FollowCameraRig rig;
    CameraRig* lensRig;
    u8 unused724[0xC];
};
CHECK_OFFSET(FollowCamera, rig, 0x20);
CHECK_OFFSET(FollowCamera, lensRig, 0x720);
CHECK_SIZE(FollowCamera, 0x730);

// The follow node's bits (retail reads and writes them as 64 bits with the chunk)
union FollowNodeBits
{
    u32 value;
    struct
    {
        // Its last update came while its clock was stopped (nothing reads it)
        u32 unused0 : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(FollowNodeBits, 4);

// The node of kind 0x16 (retail's UnkNode_0x16_Methods, 0x770 bytes), a playable character's camera: its bits, the chunk entry it
// was made for, the camera's instance, its camera and a countdown nothing reads (set to 1 second whenever the character's solver
// finds the collision near it, down by its clock's advance)
struct FollowNode : GameNode
{
    FollowNodeBits bits;
    ChunkEntry* chunk;
    Reference* cameraInstance;
    u8 unused24[0xC];
    FollowCamera camera;
    f32 unused760;
    u8 unused764[0xC];

    // Its vtable's functions (game/characternodes.cpp): 2 the destructor (what it follows released and let go of, its camera
    // destroyed), 3 given its instance (its camera started for it in its chunk), 4 whether its instance may change chunks, 5 its
    // kind, 6 its instance left its chunk (what it follows released and let go of), 7 a step (its camera started again in its
    // instance's chunk), 8 the update, 10 its type
    void Destroy(u32 destroyFlags) RETAIL(FUN_00172768);
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0017b438);
    u32 CanChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_0017b4b8);
    u32 Kind() RETAIL(GetNodeIndex_0017B218);
    void LeftChunk(u32 way) RETAIL(FUN_00172838);
    void Step(TimeClock* clock, u32 way) RETAIL(FUN_0017b520);
    u32 Update(TimeClock* clock) RETAIL(FUN_001728b8);
    u32 Type() RETAIL(FUN_0017b220);
    // A camera's instance made in a chunk, followed (its clock the chunk's first): the instance
    InstanceContext* MakeCamera(struct ChunkData* chunk) RETAIL(FUN_00172670);
    // Out of its instance's nodes, what it follows put to sleep
    void StopFollowing() RETAIL(FUN_0017b588);
};
CHECK_OFFSET(FollowNode, cameraInstance, 0x20);
CHECK_OFFSET(FollowNode, camera, 0x30);
CHECK_OFFSET(FollowNode, unused760, 0x760);
CHECK_SIZE(FollowNode, 0x770);

extern "C"
{
    extern const GccVTableEntry g_PadCameraRigVTable[] RETAIL(D_002F3808);
    extern const GccVTableEntry g_FollowCameraRigVTable[] RETAIL(D_002F37A0);

    // The follow camera rig's constants (65536ths of a turn, speeds a second; set at start-up by
    // InitCharacterControllerGlobals): the probes' push rates, the stick's pitch and yaw speeds, the pitch's range, the target's
    // box, the pitch it starts at, an unread backward axis, the yaw's and pitch's speeds and theirs while it tilts, the
    // Rollerbrawl's yaw speeds (the least, the most and between them), the Humiliskate's, an unread angle and the
    // Mecha-Bandicoot's view (its field of view, pitch and target box)
    extern s32 g_RigPitchPushRate RETAIL(D_0030A548);
    extern s32 g_RigYawPushRate RETAIL(D_0030A550);
    extern s32 g_RigPitchInputSpeed RETAIL(D_0030A558);
    extern s32 g_RigYawInputSpeed RETAIL(D_0030A560);
    extern s32 g_RigPitchLowest RETAIL(D_0030A568);
    extern s32 g_RigPitchHighest RETAIL(D_0030A570);
    extern Vector4 g_RigTargetBoxLow RETAIL(D_0030BB30);
    extern Vector4 g_RigTargetBoxHigh RETAIL(D_0030BB40);
    extern s32 g_RigStartPitch RETAIL(D_0030A578);
    extern Vector4 g_UnreadRigBack RETAIL(D_0030BB50);
    extern s32 g_RigYawSpeed RETAIL(D_0030A580);
    extern s32 g_RigPitchSpeed RETAIL(D_0030A588);
    extern s32 g_RigTiltYawSpeed RETAIL(D_0030A590);
    extern s32 g_RigTiltPitchSpeed RETAIL(D_0030A598);
    extern s32 g_RollerbrawlYawSpeedLeast RETAIL(D_0030A5A0);
    extern s32 g_RollerbrawlYawSpeedMost RETAIL(D_0030A5A8);
    extern s32 g_RollerbrawlYawSpeedRange RETAIL(D_0030A5B0);
    extern s32 g_HumiliskateYawSpeed RETAIL(D_0030A5B8);
    extern s32 g_UnreadRigAngle75 RETAIL(D_0030A5C0);
    extern s32 g_MechaFieldOfView RETAIL(D_0030A5C8);
    extern s32 g_MechaPitch RETAIL(D_0030A5D0);
    extern Vector4 g_MechaTargetBox RETAIL(D_0030BB60);

    FollowCamera* ConstructFollowCamera(FollowCamera* follow) RETAIL(FUN_0015e510);
    void DestroyFollowCamera(FollowCamera* follow, u32 destroyFlags) RETAIL(FUN_0015e560);
    // For a character: its cameras forgotten, the camera's instance moved to the character's chunk, its rig made ready (the
    // controller handed on, unread) and put on the camera's lens
    void RestartFollowCamera(FollowCamera* follow, void* controller, InstanceContext* camera, InstanceContext* character)
        RETAIL(FUN_0015e610);
    void RestoreFollowCameraDefaults(FollowCamera* follow, CharacterAgent* character) RETAIL(FUN_0015e738);
    // Its rig put on the camera's lens (set back when asked) unless its rig is away, the lens rig's defaults, the camera chosen
    // taken
    void ShowFollowCamera(FollowCamera* follow, InstanceContext* camera, InstanceContext* character, u32 reset)
        RETAIL(FUN_0015e798);
    void PutFollowCameraOnLens(FollowCamera* follow, InstanceContext* camera) RETAIL(FUN_0015e840);
    void SetFollowCameraSmoothed(FollowCamera* follow, u32 smoothed) RETAIL(FUN_0015e890);
    void SetFollowCameraSteers(FollowCamera* follow, u32 steers) RETAIL(FUN_0015e8c0);
    void SetFollowCameraIgnoresTriggers(FollowCamera* follow, u32 ignores) RETAIL(FUN_0015e910);
    // The camera chosen last frame made the current one, the choice cleared
    void TakeChosenCamera(FollowCamera* follow) RETAIL(FUN_0015e948);
    // A camera trigger the character is in, with its priority (its kind byte)
    void OfferCamera(FollowCamera* follow, CameraNode* trigger, u32 priority, CharacterAgent* character) RETAIL(FUN_0015e960);
    void FollowCameraReadPad(FollowCamera* follow, GamePad* pad) RETAIL(FUN_0015e9e8);
    // Whether it takes the trigger's camera (the second slot's kept, else the second slot cleared)
    u32 FollowCameraTakes(FollowCamera* follow, CameraNode* trigger, CharacterAgent* character) RETAIL(FUN_001434a0);
    void StepFollowCamera(FollowCamera* follow, TimeClock* clock, CharacterAgent* character, InstanceContext* camera)
        RETAIL(FUN_00143618);
    // The follow node made for a chunk entry (nothing followed), and its camera given its defaults for its instance's character
    FollowNode* ConstructFollowNode(FollowNode* node, ChunkEntry* chunk) RETAIL(FUN_0017b3e0);
    void RestoreCameraDefaults(FollowNode* node) RETAIL(FUN_0017b5f0);
}
