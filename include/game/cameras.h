#pragma once

#include "abi.h"
#include "common.h"
#include "game/layout.h"
#include "game/math.h"
#include "gcc2.h"

class CameraTarget;
class Stream;
struct TimeClock;

// The splines the spline cameras follow (0x50 bytes, still asm, its vtable 0x10 bytes in: 2 its read): how many samples, a point and
// a tangent each (their Ws hold flags and values in their bits), the length between two samples, and every sample's arc length
// from the start
struct CameraSpline
{
    s32 count;
    Vector4* samples;
    f32 step;
    u32 unknown0C;
    const GccVTableEntry* vtable;
    f32* lengths;
    u8 unknown18[0x48 - 0x18];
    s32 unknown48;
    u32 unknown4C;

    void Read(Stream* stream)
    {
        CallVirtual<void>(this, vtable, 2, stream);
    }
};
CHECK_SIZE(CameraSpline, 0x50);

// Where a nearest point search over a path or a spline stands (0x30 bytes): the nearest distance so far 0x20 bytes in, then
// the share of its segment and the segment (-1 at first)
struct CurveSearch
{
    u8 unknown00[0x20];
    f32 distance;
    f32 share;
    s32 segment;
    u32 unknown2C;
};
CHECK_SIZE(CurveSearch, 0x30);

// A camera's subtype (retail's GameCameraSubtypeBase, vtable 0xC bytes in: 1 the destructor, 2 where the camera goes for a target
// (the parameter along its geometry it took, 1 or 0 for the ones without), 3 the point at a parameter (the offset added along its
// line, path or spline first), 4 (the boss camera's: the last place taken through the arena), 5 its type, 6 the read): how the
// camera's point followers move to the points it gives (its flags), the rate they move at, and the offset along its geometry the
// camera keeps from the target's nearest point
class CameraSubtype
{
public:
    enum Flags : u32
    {
        // The followers' own way (their default way and rate), at its rate (the share of the way a second); straight there
        // otherwise
        FollowMask = 0x3,
        FollowOwnWay = 0x1,
        FollowAtRate = 0x2,
    };

    enum Types : u32
    {
        TypeBoss = 0xA19,
        TypePoint = 0x1C02,
        TypeLine = 0x1C03,
        TypePath = 0x1C04,
        TypeMain = 0x1C05,
        TypeSpline = 0x1C06,
        Type1C09 = 0x1C09,
        TypePoint2 = 0x1C0B,
        Type1C0C = 0x1C0C,
        TypeLine2 = 0x1C0D,
        Type1C0E = 0x1C0E,
        TypeZone = 0x1C0F,
        // In a main camera's file: no subtype
        TypeNone = 3,
    };

    u32 flags;
    f32 rate;
    f32 offset;
    const GccVTableEntry* vtable;

    static CameraSubtype* Construct(CameraSubtype* camera) RETAIL(FUN_0027cda8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027cdc8);
    void Nothing() RETAIL(FUN_0027b9d0);
    void Read(Stream* stream) RETAIL(ReadBaseSubtypeCamera);

    f32 At(const Vector4* point, CameraTarget* target, Vector4* out)
    {
        return CallVirtual<f32>(this, vtable, 2, point, target, out);
    }

    void AtParameter(f32 along, Vector4* out)
    {
        CallVirtual<void>(this, vtable, 3, along, out);
    }

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, 1, destroyFlags);
    }

    void TakeLastVirtual(const Vector4* position, const Vector4* point)
    {
        CallVirtual<void>(this, vtable, 4, position, point);
    }

    u32 TypeVirtual()
    {
        return CallVirtual<u32>(this, vtable, 5);
    }

    void ReadVirtual(Stream* stream)
    {
        CallVirtual<void>(this, vtable, 6, stream);
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
    u8 unknownA1[3];
    // The share of the radius it has at the axis, the heights at the axis and at the radius, the turn limit (radians a unit of
    // radius; none within 5e-05 of 0)
    f32 radiusShare;
    f32 middleHeight;
    f32 edgeHeight;
    f32 turnLimit;
    u8 unknownB4[0xC0 - 0xB4];
    Vector4 last;
    // The radius' curve goes by the distance in every direction (else across)
    u8 curveAllAxes;
    u8 unknownD1[0xF];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ba30);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(BossCameraAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027cef0);
    void TakeLast(const Vector4* point) RETAIL(FUN_0027cfb8);
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

// A sampled spline the camera slides along (the offset further only when its flags say so; its samples' Ws say where along it the
// camera goes between them)
class CameraSplineCamera : public CameraSubtype
{
public:
    enum Flags : u32
    {
        FlagTakesOffset = 0x1,
    };

    CameraSpline* spline;
    u32 splineFlags;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ba60);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraSplineAt);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraSplineAtParameter);
    u32 Type() RETAIL(FUN_0027bac8);
    void Read(Stream* stream) RETAIL(ReadCameraSpline);
    // The share of the way along its samples the camera is at for a parameter (the offset when its flags say), and the point a
    // parameter puts the camera at between the samples' points
    f32 OffsetAt(f32 along) RETAIL_N32(FUN_00279f80);
    void PointBetween(f32 along, const Vector4* target, Vector4* out) RETAIL_N32(FUN_00279d78);
    // The value a sample's W keeps (from a sample, walking forwards or backwards past the ones flagged in its bit 24) in its low
    // byte and in its bits 8-23, and the sample it was in
    void SampleByte(u32 sample, f32* value, u32* found, u32 backwards) RETAIL(FUN_0027d1c0);
    void SampleShort(u32 sample, f32* value, u32* found, u32 backwards) RETAIL(FUN_0027d1f8);
    void SampleWord(u32 sample, u32* word, u32* found, u32 backwards) RETAIL(FUN_0027d230);
};
CHECK_OFFSET(CameraSplineCamera, splineFlags, 0x14);
CHECK_SIZE(CameraSplineCamera, 0x18);

// A rotation key of the 0x1C09 cameras: the rotation and when
struct CameraRotationKey
{
    Vector4 rotation;
    f32 time;
    u8 unknown14[0xC];
};
CHECK_SIZE(CameraRotationKey, 0x20);

// Rotation keys (from the latest), how many there are and room for, the room it grows by, and whether it eases between them
struct CameraRotationKeys
{
    CameraRotationKey* keys;
    u16 count;
    u16 capacity;
    u16 growth;
    u8 unknown0A[2];
    u8 eases;
    u8 unknown0D[3];
};
CHECK_SIZE(CameraRotationKeys, 0x10);

// A spline the camera goes along, 5 units out along the rotation the keys give for the parameter (no data has one)
class Camera1C09 : public CameraSubtype
{
public:
    CameraRotationKeys rotations;
    CameraSpline spline;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bad8);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027d298);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027a248);
    u32 Type() RETAIL(FUN_0027bb80);
};
CHECK_SIZE(Camera1C09, 0x70);

// A point the camera stays a share or a distance of the way between the target and (mode 0: the share of the way from the
// point, 1: the distance from the point toward the target, 2: the same, but no further than the target)
class CameraPoint2 : public CameraPoint
{
public:
    f32 distance;
    u32 mode;
    u8 unknown28[8];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bb98);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraPoint2At);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027d880);
    u32 Type() RETAIL(FUN_0027bbc0);
    void Read(Stream* stream) RETAIL(ReadCameraPoint2);
};
CHECK_SIZE(CameraPoint2, 0x30);

// A camera around a point at the target's height plus one between two by the target's flat distance (its read takes four bytes
// over its flags, nothing sets the rest: no data has one)
class Camera1C0C : public CameraSubtype
{
public:
    f32 radius;
    f32 extra;
    f32 nearHeight;
    f32 farHeight;
    f32 farDistance;
    f32 nearDistance;
    u8 unknown28[8];
    Vector4 centre;
    u8 unknown40[0x10];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bbd0);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027a770);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027da20);
    u32 Type() RETAIL(FUN_0027bc00);
    void Read(Stream* stream) RETAIL(FUN_0027da28);
};
CHECK_SIZE(Camera1C0C, 0x50);

// A line the camera slides along by the target's flat distance from its start (at the start up to the near distance, at the end
// from the far one on, and in between the share past the near distance plus the near distance itself)
class CameraLine2 : public CameraLine
{
public:
    f32 nearDistance;
    f32 farDistance;
    u8 unknown38[8];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bc18);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(CameraLine2At);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(CameraLine2AtParameter);
    u32 Type() RETAIL(FUN_0027bc40);
    void Read(Stream* stream) RETAIL(ReadCameraLine2);
};
CHECK_SIZE(CameraLine2, 0x40);

// A camera of an object and keys that does nothing (no data has one)
class Camera1C0E : public CameraSubtype
{
public:
    // A spline it plays along (none: its read takes nothing) and the rotation keys
    CameraSpline* spline;
    CameraRotationKeys keys;
    // It plays (it started), it finished, how far it got (0 to 1) and how fast it goes (a share a second)
    u8 playing;
    u8 finished;
    u8 unknown26[2];
    f32 time;
    f32 rate;
    u8 unknown30[0xC];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bc48);
    f32 At(const Vector4* point, CameraTarget* target, Vector4* out) RETAIL(FUN_0027dbe8);
    void AtParameter(f32 along, Vector4* out) RETAIL_N32(FUN_0027dbf8);
    u32 Type() RETAIL(FUN_0027bd08);
    void Read(Stream* stream) RETAIL(FUN_0027dd30);
    // A step of its play with a clock (started the first time: along its spline at 20 units a second, the rotation of its keys
    // at the share played): whether it was done already
    u32 Play(TimeClock* clock, Vector4* position, Vector4* rotation) RETAIL(FUN_0027dc00);
};
CHECK_OFFSET(Camera1C0E, playing, 0x24);
CHECK_SIZE(Camera1C0E, 0x3C);

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

// A camera trigger's camera (0x90 bytes; TT Lab's names): what the camera controller takes from it, its switches, its blend time,
// the tools' leftovers, the fov, pitch and yaw blenders' ends (65536ths of a turn) and the distance's, the two subtypes' values,
// the yaw's extra and the blend-in values, its group and its two subtypes
struct MainCamera
{
    enum Flags : u32
    {
        // The second subtype gives its place at the value along the geometry (else for the target)
        FlagSecondAtParameter = 0x1,
        // The follow camera steers around walls with it (made so)
        FlagSteers = 0x10,
        // Its second and its first value given to the rig's point followers as their rate (the camera's place's and where it
        // looks)
        FlagPassesSecondValue = 0x1000,
        FlagPassesFirstValue = 0x2000,
        // Its yaw extra given to the yaw blender
        FlagSetsYawExtra = 0x8000,
    };

    enum Switches : u16
    {
        // Taking it sets the follow camera back to its own camera
        SwitchResetsController = 0x4,
    };

    u32 flags;
    u16 switches;
    u16 unknown06;
    CameraSubtype* first;
    CameraSubtype* second;
    f32 blendTime;
    u8 unknown14[0xC];
    Vector4 leftoverVector1;
    Vector4 leftoverVector2;
    f32 leftoverFloat1;
    f32 leftoverFloat2;
    u32 fovStart;
    u32 fovEnd;
    u32 pitchStart;
    u32 pitchEnd;
    u32 yawStart;
    u32 yawEnd;
    f32 distanceStart;
    f32 distanceEnd;
    f32 secondValue;
    f32 firstValue;
    u32 yawExtra;
    u32 blendInYaw;
    u32 blendInPitch;
    f32 blendInDistance;
    s8 group;
    u8 unknown81[0xF];

    static MainCamera* Construct(MainCamera* camera) RETAIL(FUN_00279488);
    // Its subtypes destroyed (itself freed when the flags say)
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ce70);
    void Read(Stream* stream) RETAIL(ReadMainCamera);
    // Whether a yaw is at least as near its blend-in yaw as its yaw's start
    u32 NearerBlendIn(const s32* yaw) RETAIL(FUN_00279788);
};
CHECK_OFFSET(MainCamera, fovStart, 0x48);
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
    extern const GccVTableEntry g_Camera1C09VTable[] RETAIL(CameraSubtype_0x1C09_Methods);
    extern const GccVTableEntry g_CameraPoint2VTable[] RETAIL(CameraSubtype_0x1C0B_Methods);
    extern const GccVTableEntry g_Camera1C0CVTable[] RETAIL(CameraSubtype_0x1C0C_Methods);
    extern const GccVTableEntry g_CameraLine2VTable[] RETAIL(CameraSubtype_0x1C0D_Methods);
    extern const GccVTableEntry g_Camera1C0EVTable[] RETAIL(CameraSubtype_0x1C0E_Methods);
    extern const GccVTableEntry g_CameraZoneVTable[] RETAIL(CameraSubtype_0x1C0F_Methods);
    extern const GccVTableEntry g_CameraSplineVTable[] RETAIL(D_002F5C58);

    // The object builder's camera factory: a subtype (or a main camera) of a type, none for another
    CameraSubtype* MakeCameraSubtype(void* factory, u32 type) RETAIL(FUN_0026fa58);
    // A sample W's values: its low byte from -5 by 0.0390625s, its bits 8-23 from -50 by 100/65536ths
    f32 SampleByteValue(const u32* word) RETAIL(FUN_0027d960);
    f32 SampleShortValue(const u32* word) RETAIL(FUN_0027d9b8);
    // An ease between 0 and 1 of a sharpness (half a sine's turn of it, scaled to reach both)
    f32 EaseInOut(f32 share, const f32* sharpness) RETAIL_N32(FUN_0027e148);
    // The rotation keys' rotation at a parameter (the first key's from 1 on, else between the two keys it's between, eased when
    // they ease)
    Vector4* RotationAt(Vector4* out, const CameraRotationKeys* keys, f32 along) RETAIL_N32(FUN_0027a390);

    // The curves' maths (still asm): the parameter of a line nearest a point and a line read (its two ends); a path's or a
    // spline's nearest point searched and its parameter, a path's point at a parameter; a spline's segment at a distance along it
    // (and how far into it), the point that far into a segment; the fractions of a point within a box and the point at fractions
    f32 NearestLineParameter(const Vector4* line, const Vector4* point) RETAIL(FUN_001849f0);
    void ReadLine(Vector4* line, Stream* stream) RETAIL(FUN_0018d268);
    void PathNearestSearch(const LayoutPath* path, const Vector4* point, CurveSearch* search) RETAIL(FUN_001892b8);
    f32 PathNearestParameter(const LayoutPath* path, const CurveSearch* search) RETAIL(FUN_0018e890);
    void PathPointAt(f32 along, const LayoutPath* path, Vector4* out) RETAIL_N32(FUN_00189440);
    void SplineNearestSearch(const CameraSpline* spline, const Vector4* point, CurveSearch* search) RETAIL(FUN_0018af78);
    f32 SplineNearestParameter(const CameraSpline* spline, const CurveSearch* search) RETAIL(FUN_0018f300);
    s32 SplineSegmentAt(f32 distance, const CameraSpline* spline, f32* into) RETAIL_N32(FUN_0018f0e8);
    void SplinePointIn(f32 into, const CameraSpline* spline, Vector4* out, s32 segment) RETAIL_N32(FUN_0018a7a8);
    void SplineSamplePoint(const CameraSpline* spline, Vector4* out, s32 segment) RETAIL(FUN_0018f060);
    void DestroySpline(CameraSpline* spline, u32 destroyFlags) RETAIL(FUN_0018f1f0);
    void BoxFractionsOf(const Vector4* box, const Vector4* point, Vector4* projected, f32* across, f32* along) RETAIL(BoxFractionsOf);
    void BoxPointAtFractions(f32 across, f32 along, const Vector4* box, Vector4* out) RETAIL_N32(BoxPointAtFractions);
}
