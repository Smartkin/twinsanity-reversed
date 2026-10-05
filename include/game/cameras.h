#pragma once

#include "abi.h"
#include "common.h"
#include "game/layout.h"
#include "game/math.h"
#include "gcc2.h"

class CameraTarget;
class Stream;
struct TimeClock;

// The splines the spline cameras follow (0x50 bytes, its vtable 0x10 bytes in: 1 the destructor, 2 its read): how many segments,
// the samples at their ends (one more than the segments, a point and a tangent each: the points' Ws are CameraSplineKeys), the
// length of a segment and 1 over it, every segment's arc length from the start and 1 over the steps it takes, and a nearest point
// search's state as a path has it (the segment searched -1 at first)
struct CameraSpline
{
    enum Slot : u32
    {
        DestroySlot = 1,
        ReadSlot = 2,
    };

    // A sample's vectors: its point, then its tangent
    static constexpr s32 VectorsPerSample = 2;

    s32 count;
    Vector4* samples;
    f32 step;
    f32 inverseStep;
    const GccVTableEntry* vtable;
    f32* lengths;
    f32* steps;
    u8 unused1C[4];
    Vector4 searchPoint;
    Vector4 nearest;
    f32 nearestDistance;
    f32 nearestShare;
    s32 searchSegment;
    u32 unused4C;

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void Read(Stream* stream)
    {
        CallVirtual<void>(this, vtable, ReadSlot, stream);
    }
};
CHECK_OFFSET(CameraSpline, searchPoint, 0x20);
CHECK_OFFSET(CameraSpline, searchSegment, 0x48);
CHECK_SIZE(CameraSpline, 0x50);

// The W of a spline camera's sample's point (TT Lab's): a key of where the camera goes there (interpolated between the keys), or
// none
union CameraSplineKey
{
    u32 value;
    struct
    {
        // The share of the way from the spline toward the target (from -5 by 5/128ths) and the offset along the spline (from -50
        // by 100/65536ths of a unit)
        u32 towardTarget : 8;
        u32 offset : 16;
        // Not a key: the keys around it are taken
        u32 passedOver : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(CameraSplineKey, 4);

// Where a nearest point search over a path or a spline stands (0x30 bytes): the nearest point so far 0x10 bytes in and its
// distance squared, then the share of its segment and the segment (-1 at first)
struct CurveSearch
{
    u8 unused00[0x10];
    Vector4 nearest;
    f32 distance;
    f32 share;
    s32 segment;
    u32 unused2C;
};
CHECK_SIZE(CurveSearch, 0x30);

// How the camera rig's point followers move to the points a camera's subtype gives (TT Lab's FollowMode)
union CameraSubtypeFlags
{
    u32 value;
    struct
    {
        // Its CameraSubtype::Follows
        u32 follow : 2;
        u32 unused2 : 30;
    };
};
CHECK_SIZE(CameraSubtypeFlags, 4);

// A camera's subtype (retail's GameCameraSubtypeBase, vtable 0xC bytes in: 1 the destructor, 2 where the camera goes for a target
// (the parameter along its geometry it took, 1 or 0 for the ones without), 3 the point at a parameter (the offset added along its
// line, path or spline first), 4 the last place taken (only the boss camera's takes it, through the arena), 5 its type, 6 the
// read): how the camera's point followers move to the points it gives (its flags), the rate they move at, and the offset along
// its geometry the camera keeps from the target's nearest point
class CameraSubtype
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        AtSlot = 2,
        AtParameterSlot = 3,
        TakeLastSlot = 4,
        TypeSlot = 5,
        ReadSlot = 6,
    };

    // How the followers move: straight there (3 as well), their own way (their default way and rate), or at its rate (the share
    // of the way a second)
    enum Follows : u32
    {
        FollowStraight = 0,
        FollowOwnWay = 1,
        FollowAtRate = 2,
    };

    enum Types : u32
    {
        TypeBoss = 0xA19,
        TypePoint = 0x1C02,
        TypeLine = 0x1C03,
        TypePath = 0x1C04,
        TypeMain = 0x1C05,
        TypeSpline = 0x1C06,
        TypeSplineArm = 0x1C09,
        TypePoint2 = 0x1C0B,
        TypeOrbit = 0x1C0C,
        TypeLine2 = 0x1C0D,
        TypeKeyed = 0x1C0E,
        TypeZone = 0x1C0F,
        // In a main camera's file: no subtype
        TypeNone = 3,
    };

    CameraSubtypeFlags flags;
    f32 rate;
    f32 offset;
    const GccVTableEntry* vtable;

    static CameraSubtype* Construct(CameraSubtype* camera) RETAIL(FUN_0027cda8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027cdc8);
    // (The base's takes nothing)
    void TakeLast() RETAIL(FUN_0027b9d0);
    void Read(Stream* stream) RETAIL(ReadBaseSubtypeCamera);

    f32 At(const Vector4* point, CameraTarget* target, Vector4* out)
    {
        return CallVirtual<f32>(this, vtable, AtSlot, point, target, out);
    }

    void AtParameter(f32 along, Vector4* out)
    {
        CallVirtual<void>(this, vtable, AtParameterSlot, along, out);
    }

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void TakeLastVirtual(const Vector4* position, const Vector4* point)
    {
        CallVirtual<void>(this, vtable, TakeLastSlot, position, point);
    }

    u32 TypeVirtual()
    {
        return CallVirtual<u32>(this, vtable, TypeSlot);
    }

    void ReadVirtual(Stream* stream)
    {
        CallVirtual<void>(this, vtable, ReadSlot, stream);
    }
};
CHECK_SIZE(CameraSubtype, 0x10);

// A point the camera stays at
class CameraPoint : public CameraSubtype
{
public:
    Vector4 point;

    static CameraPoint* Construct(CameraPoint* camera) RETAIL(FUN_0027d530);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027d550);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraPointAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraPointAtParameter);
    u32 Type() RETAIL(FUN_0027ba20);
    void Read(Stream* stream) RETAIL(ReadCameraPoint);
};
CHECK_SIZE(CameraPoint, 0x20);

// A line the camera slides along to the target's nearest point (the offset further, between its ends)
class CameraLine : public CameraSubtype
{
public:
    Vector4 start;
    Vector4 end;

    static CameraLine* Construct(CameraLine* camera) RETAIL(FUN_0027d348);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027d368);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraLineAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraLineAtParameter);
    u32 Type() RETAIL(FUN_0027b9f8);
    void Read(Stream* stream) RETAIL(ReadCameraLine);
};
CHECK_SIZE(CameraLine, 0x30);

// A path (a layout's) the camera slides along the same way
class CameraPath : public CameraSubtype
{
public:
    LayoutPath path;

    static CameraPath* Construct(CameraPath* camera) RETAIL(FUN_0027d640);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027d688);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraPathAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraPathAtParameter);
    u32 Type() RETAIL(FUN_0027ba08);
    void Read(Stream* stream) RETAIL(ReadCameraPath);
};
CHECK_SIZE(CameraPath, 0x60);

// The boss's arena camera (0xE0 bytes): the target taken into the arena's space, the camera at its height (plus a height, or
// one going down the further out it is when it has curves) a radius out from the arena's axis (the radius going down with the
// distance when it has curves, across only or in every direction), turned no faster than a limit, while the target is above the
// floor and below the ceiling (else where it was)
class BossCamera : public CameraSubtype
{
public:
    Matrix4x4 worldToArena;
    Matrix4x4 arenaToWorld;
    // The ceiling, the radius, the height above the target
    Vector4 orbit;
    u8 curves;
    u8 unusedA1[3];
    // The share of the radius it has at the axis, the heights at the axis and at the radius, the turn limit (radians a unit of
    // radius; none within 5e-05 of 0)
    f32 radiusShare;
    f32 middleHeight;
    f32 edgeHeight;
    f32 turnLimit;
    u8 unusedB4[0xC0 - 0xB4];
    // The camera's last place in the arena
    Vector4 last;
    // The radius' curve goes by the distance in every direction (else across)
    u8 curveAllAxes;
    u8 unusedD1[0xF];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ba30);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(BossCameraAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027cef0);
    // The camera's place taken into the arena as its last
    void TakeLast(const Vector4* place) RETAIL(FUN_0027cfb8);
    u32 Type() RETAIL(FUN_0027ba58);
    void Read(Stream* stream) RETAIL(ReadBossCamera);
    // The radius and the height for a place in the arena, and a camera place turned no further from the last than the limit
    // allows at a radius (to the nearer of the two at the limit)
    f32 RadiusAt(const Vector4* place) RETAIL(BossCameraRadiusCurve);
    f32 HeightAt(const Vector4* place) RETAIL(BossCameraHeightCurve);
    void LimitTurn(f32 radius, Vector4* place) RETAIL_N32(BossCameraLimitTurn);
};
CHECK_OFFSET(BossCamera, orbit, 0x90);
CHECK_OFFSET(BossCamera, last, 0xC0);
CHECK_SIZE(BossCamera, 0xE0);

// A spline camera's flags (its read takes the low half; TT Lab's SplineCameraFlags, the rest the tools' leftovers)
union CameraSplineFlags
{
    u32 value;
    struct
    {
        // Its offset along the spline is the subtype's (else its samples' keys')
        u32 takesOffset : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(CameraSplineFlags, 4);

// A sampled spline the camera slides along (the offset further, the subtype's when its flags say so, else its samples' keys'),
// then the keys' share of the way toward the target
class CameraSplineCamera : public CameraSubtype
{
public:
    CameraSpline* spline;
    CameraSplineFlags splineFlags;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ba60);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraSplineAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraSplineAtParameter);
    u32 Type() RETAIL(FUN_0027bac8);
    void Read(Stream* stream) RETAIL(ReadCameraSpline);
    // The offset along the spline for a parameter (the subtype's when its flags say, else the keys' around it), and the point a
    // parameter puts the camera at: from the spline's point (given in out) the keys' share of the way toward the target
    f32 OffsetAt(f32 along) RETAIL_N32(FUN_00279f80);
    void PointBetween(f32 along, const Vector4* target, Vector4* out) RETAIL_N32(FUN_00279d78);
    // The key at a sample (its point's index among the samples' vectors; walking back from it, or forward from the next sample,
    // past the samples that aren't keys) and the index it was at: its share toward the target, its offset and the key itself
    void KeyShareAt(u32 index, f32* share, u32* found, u32 backwards) RETAIL(FUN_0027d1c0);
    void KeyOffsetAt(u32 index, f32* offset, u32* found, u32 backwards) RETAIL(FUN_0027d1f8);
    void KeyAt(u32 index, u32* key, u32* found, u32 backwards) RETAIL(FUN_0027d230);
};
CHECK_OFFSET(CameraSplineCamera, splineFlags, 0x14);
CHECK_SIZE(CameraSplineCamera, 0x18);

// A rotation key of the spline arm and keyed cameras: the rotation and when
struct CameraRotationKey
{
    Vector4 rotation;
    f32 time;
    u8 unused14[0xC];
};
CHECK_SIZE(CameraRotationKey, 0x20);

// Rotation keys (from the latest), how many there are and room for, the room it grows by, and whether it eases between them
struct CameraRotationKeys
{
    CameraRotationKey* keys;
    u16 count;
    u16 capacity;
    u16 growth;
    u8 unused0A[2];
    u8 eases;
    u8 unused0D[3];

    // The room the subtypes' keys grow by
    static constexpr u16 Growth = 0x40;
};
CHECK_SIZE(CameraRotationKeys, 0x10);

// A spline the camera goes along at the end of an arm (5 units out along the rotation the keys give for the parameter, from the
// spline's point; retail's 0x1C09, no data has one)
class SplineArmCamera : public CameraSubtype
{
public:
    CameraRotationKeys rotations;
    CameraSpline spline;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bad8);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027d298);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027a248);
    u32 Type() RETAIL(FUN_0027bb80);
};
CHECK_SIZE(SplineArmCamera, 0x70);

// A point the camera stays a share or a distance of the way from the target toward (by its mode; any other leaves the camera where
// it was)
class CameraPoint2 : public CameraPoint
{
public:
    enum Modes : u32
    {
        // The share of the way from the target toward the point, the distance from the target toward it, the same no further
        // than the point
        ModeShareOfTheWay = 0,
        ModeFromTheTarget = 1,
        ModeNoFurtherThanThePoint = 2,
    };

    f32 distance;
    u32 mode;
    u8 unused28[8];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bb98);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraPoint2At);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027d880);
    u32 Type() RETAIL(FUN_0027bbc0);
    void Read(Stream* stream) RETAIL(ReadCameraPoint2);
};
CHECK_SIZE(CameraPoint2, 0x30);

// A camera around a centre (retail's 0x1C0C, the scripts' top-down mode's; its read takes four bytes over its flags, no data has
// one): the radius beyond the target the way the target is from the centre (beyond the extra while the target is within it), at
// the target's height plus one between two by the target's flat distance
class OrbitCamera : public CameraSubtype
{
public:
    f32 radius;
    f32 extra;
    f32 nearHeight;
    f32 farHeight;
    f32 farDistance;
    f32 nearDistance;
    u8 unused28[8];
    Vector4 centre;
    u8 unused40[0x10];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bbd0);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027a770);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027da20);
    u32 Type() RETAIL(FUN_0027bc00);
    void Read(Stream* stream) RETAIL(FUN_0027da28);
};
CHECK_SIZE(OrbitCamera, 0x50);

// A line the camera slides along by the target's flat distance from its start (at the start up to the near distance, at the end
// from the far one on, and in between the share past the near distance plus the near distance itself)
class CameraLine2 : public CameraLine
{
public:
    f32 nearDistance;
    f32 farDistance;
    u8 unused38[8];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bc18);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraLine2At);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraLine2AtParameter);
    u32 Type() RETAIL(FUN_0027bc40);
    void Read(Stream* stream) RETAIL(ReadCameraLine2);
};
CHECK_SIZE(CameraLine2, 0x40);

// A camera the follow camera plays along a spline with rotation keys (retail's 0x1C0E; as a subtype it does nothing, no data has
// one)
class KeyedCamera : public CameraSubtype
{
public:
    // A spline it plays along (none: its read takes nothing) and the rotation keys
    CameraSpline* spline;
    CameraRotationKeys keys;
    // It plays (it started), it finished, how far it got (0 to 1) and how fast it goes (a share a second)
    u8 playing;
    u8 finished;
    u8 unused26[2];
    f32 time;
    f32 rate;
    u8 unused30[0xC];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bc48);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027dbe8);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027dbf8);
    u32 Type() RETAIL(FUN_0027bd08);
    void Read(Stream* stream) RETAIL(FUN_0027dd30);
    // A step of its play with a clock (started the first time: along its spline at 20 units a second, the rotation of its keys
    // at the share played): whether it was done already
    u32 Play(TimeClock* clock, Vector4* position, Vector4* rotation) RETAIL(FUN_0027dc00);
};
CHECK_OFFSET(KeyedCamera, playing, 0x24);
CHECK_SIZE(KeyedCamera, 0x3C);

// Two boxes (axes, corner and sizes, the tools'): where the target is in the target box is where the camera goes in the camera box
// (its read takes the boxes only)
class CameraZone : public CameraSubtype
{
public:
    Vector4 cameraBox[5];
    Vector4 targetBox[5];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bd18);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraZoneAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027ddb0);
    u32 Type() RETAIL(FUN_0027bd40);
    void Read(Stream* stream) RETAIL(ReadCameraZone);
};
CHECK_SIZE(CameraZone, 0xB0);

// What the camera controller and the follow camera take from a camera trigger's camera (TT Lab's CameraFlags)
union MainCameraFlags
{
    u32 value;
    struct
    {
        // The second subtype gives its place at the value along the geometry (else for the target)
        u32 secondAtParameter : 1;
        // The follow camera's switch back may blend to its rig (nothing starts one)
        u32 allowsSwitchBack : 1;
        // The pitch's and the distance's blenders take its ends
        u32 setsPitch : 1;
        u32 setsDistance : 1;
        // The follow camera steers around walls with it (its probes stay on)
        u32 steers : 1;
        // It cuts in instead of blending in over its blend time
        u32 noBlendIn : 1;
        // The yaw's and the field of view's blenders take its ends
        u32 setsYaw : 1;
        u32 setsFov : 1;
        // The follow camera's target takes its target box, and moves toward the middle of the trigger's instances (its framing
        // share of the way, up to its framing distance)
        u32 givesTargetBox : 1;
        u32 framesInstances : 1;
        // Its ends are the values at the start and the end of its geometry (taken by how far along it the target is), not a
        // blend's over time
        u32 valuesAlongGeometry : 1;
        // The follow camera takes its values even while it ignores cameras' values
        u32 alwaysTakesValues : 1;
        // The rig's point followers move the camera's place and where it looks at its rates
        u32 setsPositionFollowRate : 1;
        u32 setsTargetFollowRate : 1;
        u32 unused14 : 1;
        // The yaw blender turns at its yaw speed (slowed by the sine of what it has left to turn)
        u32 setsYawSpeed : 1;
        // The yaw, the pitch and the distance go to its blend-in values while the yaw is at least as near its blend-in yaw as
        // its yaw's start
        u32 blendsInFromYaw : 1;
        u32 blendsInFromPitch : 1;
        u32 blendsInFromDistance : 1;
        // The follow camera only turns to look at the target, keeps its height looking at it, doesn't move, tilts toward the way
        // the target faces, has its probes off
        u32 onlyLooksAtTarget : 1;
        u32 keepsHeight : 1;
        u32 holdsStill : 1;
        u32 tilts : 1;
        u32 noProbes : 1;
        // It cuts in when the camera it replaces has this too
        u32 cutsFromSameKind : 1;
        // The follow camera's target blends to its point even when it's near
        u32 blendsWhenNear : 1;
        // The follow camera takes it only after another the step before; and on foot whatever the character does
        u32 needsRunningCamera : 1;
        u32 ignoresPlayerState : 1;
        // The target box isn't turned with the followed object
        u32 targetBoxUnturned : 1;
        // The follow camera blends its place along the line from where the blend started, doesn't check its view of the target,
        // adds its own extra yaw
        u32 blendsAlongLine : 1;
        u32 skipsViewCheck : 1;
        u32 addsExtraYaw : 1;
    };
};
CHECK_SIZE(MainCameraFlags, 4);

// A camera trigger's camera's switches (TT Lab's CameraSwitches, 0 on nearly every camera)
union MainCameraSwitches
{
    u16 value;
    struct
    {
        // A camera for the character's death: the follow camera takes it only once the character died, keeps it apart and
        // switches to it while the character is dead
        u16 secondSlot : 1;
        // With noProbes, the follow camera's probes stay on while it ignores cameras' values
        u16 keepsProbesWhileIgnoring : 1;
        // Taking it sets the follow camera back to its own camera
        u16 resetsController : 1;
        u16 unused3 : 13;
    };
};
CHECK_SIZE(MainCameraSwitches, 2);

// A camera trigger's camera (0x90 bytes; TT Lab's names): what the camera controller takes from it, its switches, its blend time,
// the follow camera's target box and framing (the tools' leftovers without their flags), the fov, pitch and yaw blenders' ends
// (65536ths of a turn) and the distance's, the rates of the rig's point followers (shares of the way a second), the yaw blender's
// speed (65536ths of a turn a second), the blend-in values, its group (cameras of two groups don't replace each other while both
// are nonzero) and its two subtypes
struct MainCamera
{
    MainCameraFlags flags;
    MainCameraSwitches switches;
    u16 unused06;
    CameraSubtype* first;
    CameraSubtype* second;
    f32 blendTime;
    u8 unused14[0xC];
    Vector4 targetBoxMin;
    Vector4 targetBoxMax;
    f32 framingDistance;
    f32 framingShare;
    u32 fovStart;
    u32 fovEnd;
    u32 pitchStart;
    u32 pitchEnd;
    u32 yawStart;
    u32 yawEnd;
    f32 distanceStart;
    f32 distanceEnd;
    f32 positionFollowRate;
    f32 targetFollowRate;
    u32 yawSpeed;
    u32 blendInYaw;
    u32 blendInPitch;
    f32 blendInDistance;
    s8 group;
    u8 unused81[0xF];

    static MainCamera* Construct(MainCamera* camera) RETAIL(FUN_00279488);
    // Its subtypes destroyed (itself freed when the flags say)
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ce70);
    void Read(Stream* stream) RETAIL(ReadMainCamera);
    // Whether a yaw is at least as near its blend-in yaw as its yaw's start
    u32 NearerBlendIn(const s32* yaw) RETAIL(FUN_00279788);
};
CHECK_OFFSET(MainCamera, targetBoxMin, 0x20);
CHECK_OFFSET(MainCamera, fovStart, 0x48);
CHECK_OFFSET(MainCamera, positionFollowRate, 0x68);
CHECK_OFFSET(MainCamera, group, 0x80);
CHECK_SIZE(MainCamera, 0x90);

extern "C"
{
    extern const GccVTableEntry g_CameraSubtypeVTable[] RETAIL(CameraSubtypeBase_Methods);
    extern const GccVTableEntry g_CameraPointVTable[] RETAIL(CameraSubtype_0x1C02_Methods);
    extern const GccVTableEntry g_CameraLineVTable[] RETAIL(CameraSubtype_0x1C03_Methods);
    extern const GccVTableEntry g_CameraPathVTable[] RETAIL(CameraSubtype_0x1C04_Methods);
    extern const GccVTableEntry g_BossCameraVTable[] RETAIL(CameraSubtype_0xA19_Methods);
    extern const GccVTableEntry g_CameraSplineCameraVTable[] RETAIL(CameraSubtype_0x1C06_Methods);
    extern const GccVTableEntry g_SplineArmCameraVTable[] RETAIL(CameraSubtype_0x1C09_Methods);
    extern const GccVTableEntry g_CameraPoint2VTable[] RETAIL(CameraSubtype_0x1C0B_Methods);
    extern const GccVTableEntry g_OrbitCameraVTable[] RETAIL(CameraSubtype_0x1C0C_Methods);
    extern const GccVTableEntry g_CameraLine2VTable[] RETAIL(CameraSubtype_0x1C0D_Methods);
    extern const GccVTableEntry g_KeyedCameraVTable[] RETAIL(CameraSubtype_0x1C0E_Methods);
    extern const GccVTableEntry g_CameraZoneVTable[] RETAIL(CameraSubtype_0x1C0F_Methods);
    extern const GccVTableEntry g_CameraSplineVTable[] RETAIL(D_002F5C58);
    extern const GccVTableEntry g_SplineSamplesVTable[] RETAIL(D_002F5C78);

    // The object builder's camera factory (a vtable alone, D_00305128, the game context's): a subtype (or a main camera) of a type,
    // none for another, and its destructor
    CameraSubtype* MakeCameraSubtype(void* factory, u32 type) RETAIL(FUN_0026fa58);
    void DestroyCameraItemBuilder(void* factory, u32 destroyFlags) RETAIL(FUN_0027b9a0);
    // A spline camera's key's share toward the target and offset along the spline
    f32 KeyShare(const u32* key) RETAIL(FUN_0027d960);
    f32 KeyOffset(const u32* key) RETAIL(FUN_0027d9b8);
    // An ease between 0 and 1 of a sharpness (half a sine's turn of it, scaled to reach both)
    f32 EaseInOut(f32 share, const f32* sharpness) RETAIL_N32(FUN_0027e148);
    // The rotation keys' rotation at a parameter (the first key's from 1 on, else between the two keys it's between, eased when
    // they ease)
    Vector4* RotationAt(Vector4* out, const CameraRotationKeys* keys, f32 along) RETAIL_N32(FUN_0027a390);

    // The curves' maths: the parameter of a line nearest a point (0 for a line of no length) and a line read (its two ends); a
    // path's or a spline's nearest point searched and its parameter, a path's point at a parameter; a spline's segment at a
    // distance along it (and how far into it), the point that far into a segment (a cubic Hermite one, the w 1); the fractions of
    // a point within a box (the point taken into its plane, or onto its nearest edge when it's outside: whether it was inside)
    // and the point at fractions
    f32 NearestLineParameter(const Vector4* line, const Vector4* point) RETAIL(FUN_001849f0);
    void ReadLine(Vector4* line, Stream* stream) RETAIL(FUN_0018d268);
    void PathNearestSearch(LayoutPath* path, const Vector4* point, CurveSearch* search) RETAIL(FUN_001892b8);
    f32 PathNearestParameter(const LayoutPath* path, const CurveSearch* search) RETAIL(FUN_0018e890);
    void PathPointAt(f32 along, const LayoutPath* path, Vector4* out) RETAIL_N32(FUN_00189440);
    void SplineNearestSearch(CameraSpline* spline, const Vector4* point, CurveSearch* search) RETAIL(FUN_0018af78);
    f32 SplineNearestParameter(const CameraSpline* spline, const CurveSearch* search) RETAIL(FUN_0018f300);
    s32 SplineSegmentAt(f32 distance, const CameraSpline* spline, f32* into) RETAIL_N32(FUN_0018f0e8);
    void SplinePointIn(f32 into, const CameraSpline* spline, Vector4* out, s32 segment) RETAIL_N32(FUN_0018a7a8);
    void SplineSamplePoint(const CameraSpline* spline, Vector4* out, s32 segment) RETAIL(FUN_0018f060);
    void DestroySpline(CameraSpline* spline, u32 destroyFlags) RETAIL(FUN_0018f1f0);
    // The destructor of the splines' base (its samples; its vtable D_002F5C78), a segment's cubic stepped on by its forward
    // differences (as a path's), the share of the search's segment nearest the search's point refined by Brent's method between 0
    // and 1 (4 steps, to 5e-05; the distance squared at the share given, both the nearest's after: whether it converged), and that
    // distance squared at a share (the method's function)
    void DestroySplineSamples(CameraSpline* spline, u32 destroyFlags) RETAIL(FUN_0018f088);
    void SplineDifferencesStep(const CameraSpline* spline, Vector4* differences) RETAIL(FUN_0018f330);
    s32 RefineSplineNearest(CameraSpline* spline, f32* into, f32* distanceSquared) RETAIL(FUN_0018f278);
    f32 SplineDistanceSquaredIn(f32 into, CameraSpline* spline) RETAIL_N32(FUN_0018f2d8);
    // The distance squared from the search's point to its segment's point at a share (of the segment's Hermite cubic)
    f32 SplineSegmentDistanceSquared(f32 into, CameraSpline* spline) RETAIL_N32(FUN_0018ae68);
    u32 BoxFractionsOf(const Vector4* box, const Vector4* point, Vector4* projected, f32* across, f32* along) RETAIL(BoxFractionsOf);
    void BoxPointAtFractions(f32 across, f32 along, const Vector4* box, Vector4* out) RETAIL_N32(BoxPointAtFractions);
}
