#pragma once

#include "abi.h"
#include "common.h"
#include "game/camerablender.h"
#include "game/camerarig.h"
#include "game/cameras.h"

struct CollisionCache;
struct ObjectPlace;
struct Reference;
class Camera1C0E;

// The player's camera's positioner (TT Lab's camera controller, 0x400 bytes, retail's vtable D_00304DA8: 7 its rotation tilted
// toward where the target is going). It keeps the camera behind the target at a pitch, a yaw, a field of view and a distance
// (its four blenders), or at the place its camera trigger's second subtype gives (blended to from where it was over the
// trigger's blend time), and away from walls: four probes from around the target to around the camera (above, below, left and
// right, one cast a step) turn and pull it in, it's pushed off the collision and out of the instances' hulls, and when the view
// of the target stays blocked it jumps to a clear place. Its bits and state (below), the instance it follows and the target, the
// place and the blend's start, the probes' ends (around both, between their low and high pitch values by the pitch blender's
// share), the tilt (its pitch and yaw, eased toward the target's facing and its sideways turn), the camera trigger it took and
// a camera of its own, the collision near the camera, the rates its place follows at (the trigger's or its own: the share of
// the way a second), how much of a turn it takes a step, how far it backs off when it looks at the target's front, the radiuses
// it keeps from the collision and the hulls
class FollowCameraPositioner : public CameraPositioner
{
public:
    enum Bits : u64
    {
        // It goes to its camera trigger's place (else behind the target), its distance follows the pitch (between its ends as
        // the pitch is between its), the probes start at the target point (the trigger has a first subtype; else around the
        // instance), and bit 4 the last trigger's
        BitAtPlace = 0x1,
        BitDistanceFollowsPitch = 0x2,
        BitProbesFromTarget = 0x4,
        // Its probes pull it in when only some hit and it's further than 2 (and a step without the target moving ends its
        // following unless bit 5's set), probes are on (with bit 46) while bit 7 or 47 is
        Bit3 = 0x8,
        BitHadProbesFromTarget = 0x10,
        // Its trigger's bit 11: no switch of bit 7
        BitTriggerBit11 = 0x20,
        Bit6 = 0x40,
        // Camera triggers' values aren't taken (they only change its bits)
        BitIgnoresValues = 0x80,
        // It tilts, its pitch's goal is the default pitch kept within its ends (else 0)
        BitTilts = 0x100,
        BitYawExtraSpeed = 0x400,
        // It takes its own camera (0x2F0) instead of the triggers'
        BitOwnCamera = 0x800,
        // Its trigger's bits 21, 20 and 19: the rig keeps its rotation (it doesn't move), it keeps its height and looks at the
        // target, it only looks at the target
        BitStill = 0x1000,
        BitKeepsHeight = 0x2000,
        BitOnlyLooks = 0x4000,
        // Set back this step: changing triggers cuts (and it doesn't tilt)
        BitRestarted = 0x8000,
        // Its trigger's bit 22 clear (and it doesn't tilt)
        BitTriggerBit22Clear = 0x10000,
        // Its place follows at the trigger's rate (the second value, else at its own)
        BitTriggerRate = 0x20000,
        Bit19 = 0x80000,
        // Its view of the target isn't checked
        BitViewUnchecked = 0x100000,
        // Its yaw has the extra added
        BitYawExtra = 0x200000,
        // The probes are off (the trigger's bit 23, the target still)
        BitProbesOff = 0x1'0000'0000,
        // It was pushed off the collision, out of an instance's hull
        BitPushedOffCollision = 0x2'0000'0000,
        BitPushedOutOfHull = 0x4'0000'0000,
        // Its view's been blocked over 0.2 seconds, at all (since the time kept), over 0.7 seconds
        BitBlockedAWhile = 0x8'0000'0000,
        BitBlocked = 0x10'0000'0000,
        BitBlockedLong = 0x20'0000'0000,
        // Nothing pushed it
        BitFree = 0x40'0000'0000,
        // The probes' last results (left, right, above, below)
        BitLeftHit = 0x80'0000'0000,
        BitRightHit = 0x100'0000'0000,
        BitAboveHit = 0x200'0000'0000,
        BitBelowHit = 0x400'0000'0000,
        // The probe cast next
        ProbeShift = 43,
        ProbeMask = 0x3ull << ProbeShift,
        // The push brought it no nearer the target (its view counts as blocked)
        BitPushedBack = 0x2000'0000'0000,
        BitSteers = 0x4000'0000'0000,
        // Its trigger's bit 4
        BitTriggerBit4 = 0x8000'0000'0000,
        // The target stands still
        BitTargetStill = 0x1'0000'0000'0000,
    };

    enum State : u32
    {
        // It blends to its trigger's place (from the place it was at when the blend started), the blend's started, how (0 at
        // the trigger's rates, 2 a cube of the time's share)
        StateBlending = 0x1,
        StateBlendStarted = 0x2,
        CurveShift = 2,
        CurveMask = 0x7 << CurveShift,
        CurveCubic = 2,
        // It cuts (no smoothing), it was placed by a step
        StateCut = 0x40,
        StatePlaced = 0x80,
        // A blend asked for without a trigger (2 seconds, to the defaults)
        StateBlendAsked = 0x100,
        // Its blenders' speeds were set to take the blend's time, and set back
        StateTimed = 0x200,
        StateTimeEnded = 0x400,
        // The trigger's bit 29: the place is blended along the line from where the blend started
        StateAlongLine = 0x800,
    };

    u64 bits;
    u32 state;
    InstanceContext* instance;
    CameraTarget* target;
    AngleBlender pitch;
    AngleBlender fieldOfView;
    AngleBlender yaw;
    DistanceBlender distance;
    u8 unknown194[0xC];
    Vector4 place;
    Vector4 blendFrom;
    u32 blendStart;
    s32 blendTicks;
    f32 unknown1C8;
    // How fast the probes turn it (65536ths of a turn a second)
    s32 pitchPushRate;
    s32 yawPushRate;
    // The probes' results since its last step
    u8 probeHits;
    u8 unknown1D5[3];
    // The probes' sideways ends (the first two up, the others across), around the target and around the camera, at its low pitch
    // and its high
    f32 lowTargetSides[4];
    f32 lowCameraSides[4];
    f32 highTargetSides[4];
    f32 highCameraSides[4];
    u8 unknown218[8];
    // Around the instance (when the probes don't start at the target point) and around the camera, at the low and the high pitch
    Vector4 lowTargetOffset;
    Vector4 highTargetOffset;
    Vector4 lowCameraOffset;
    Vector4 highCameraOffset;
    Vector4 probeTargetEnds[4];
    Vector4 probeCameraEnds[4];
    s32 tiltPitch;
    s32 tiltYaw;
    s32 lastYaw;
    CameraNode* trigger;
    MainCamera camera;
    CollisionCache* cache;
    u8 unknown384;
    u8 unknown385[3];
    f32 stepSeconds;
    f32 triggerRate;
    f32 ownRate;
    f32 turnShare;
    f32 unknown398;
    u32 blockedSince;
    u8 unknown3A0[0x10];
    // How much it faces the target's front (-1 to 0) and how far the target turned sideways (-1 to 1)
    f32 facing;
    f32 sideways;
    f32 backOff;
    f32 hullRadius;
    f32 collisionRadius;
    // The lowest the lower probe's end around the target may be
    f32 probeFloor;
    s32 roll;
    // A keyed camera (0x1C0E) plays
    u8 keyed;
    u8 unknown3CD[3];
    s32 yawExtra;
    u8 unknown3D4[0xC];
    Vector4 lastPosition;
    // An instance the view checks leave out
    ReferencedObject* ignored;
    u8 unknown3F4[0xC];

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

    // A camera's values taken (its bits always, its values unless it ignores them: the blenders' ranges, blended over the blend
    // time unless the camera cuts, and the second subtype's place)
    void Apply(f32 value, MainCamera* camera, CameraTarget* target) RETAIL_N32(FUN_00274e08);
    // Back to its defaults after a trigger's camera (over the blend time unless it cuts)
    void BlendBack(CameraNode* last, u32 time) RETAIL(FUN_00275cf0);
    // Its probes' and pushes' state cleared (its trigger's bits off, the blenders' ranges their own)
    void Clear() RETAIL(FUN_002749c8);
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
    // The probes cast (one a step) for a target point and a place (their results kept when asked): which hit (1 above, 2 below,
    // 4 left, 8 right, 0x20 both of a pair, 0x40 it should pull in)
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
    void FollowKeyed(Camera1C0E* camera) RETAIL(FUN_00277108);
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
CHECK_OFFSET(FollowCameraPositioner, place, 0x1A0);
CHECK_OFFSET(FollowCameraPositioner, blendTicks, 0x1C4);
CHECK_OFFSET(FollowCameraPositioner, probeHits, 0x1D4);
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

// The player's camera's target (0x210 bytes, retail's vtable D_00304DF0): the followed instance's place (or a fixed one), its
// point moved toward the centre of its camera trigger's instances by a share up to a distance, its height eased (rising at 5
// a second up to the ground's, the point's otherwise) plus the box's point at a share (its own box, or a given one, blended to
// over the trigger's blend time) turned with the object, or the trigger's first subtype's point (blended to over the blend
// time). Its bits (below), the reference to the instance, the trigger taken and the one before, the box at the blend's start,
// a camera of its own
class FollowCameraTarget : public CameraTarget
{
public:
    enum Bits : u32
    {
        // It goes to its trigger's point (the first subtype's), blends to it, blends now
        BitHasCamera = 0x1,
        BitBlends = 0x2,
        BitBlending = 0x4,
        // Its box is the given one (its trigger's bit 8: the camera's leftover vectors), it frames the trigger's instances (bit
        // 9: the camera's leftover floats are the share and the distance), its height isn't eased
        BitGivenBox = 0x8,
        BitFramesInstances = 0x10,
        BitUneased = 0x20,
        CurveMask = 0x1C0,
        CurveCubic = 0x80,
        // The trigger's point is its own (no first subtype)
        BitPointFollows = 0x200,
        BitBoxBlending = 0x1000,
        BitCut = 0x2000,
        BitReset = 0x4000,
        BitStepped = 0x8000,
        BitSettled = 0x10000,
        // Its trigger's bit 25 (it blends even when the point is near), bit 28 (the box's point isn't turned with the object),
        // the last trigger's bit 28
        BitBlendsNear = 0x20000,
        BitOffsetUnturned = 0x40000,
        BitLastOffsetUnturned = 0x80000,
        BitOwnCamera = 0x100000,
    };

    u32 bits;
    Reference* followed;
    ObjectPlace* fixedPlace;
    CameraNode* trigger;
    f32 pull;
    f32 most;
    f32 groundHeight;
    f32 height;
    f32 boxShare;
    u8 unknown94[0xC];
    Vector4 offset;
    Vector4 easedPoint;
    Vector4 cameraPoint;
    u32 blendStart;
    s32 blendTicks;
    u8 unknownD8[8];
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
    u8 unknown174[0xC];
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
    // Its trigger's state cleared (smoothed again)
    void Clear() RETAIL(FUN_0027c980);
    // A box given (and its use dropped)
    void SetBox(const Vector4* min, const Vector4* max) RETAIL(FUN_0027ca18);
    void ClearBox() RETAIL(FUN_0027ca00);
};
CHECK_OFFSET(FollowCameraTarget, bits, 0x70);
CHECK_OFFSET(FollowCameraTarget, followed, 0x74);
CHECK_OFFSET(FollowCameraTarget, groundHeight, 0x88);
CHECK_OFFSET(FollowCameraTarget, offset, 0xA0);
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
