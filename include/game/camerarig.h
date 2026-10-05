#pragma once

#include "abi.h"
#include "common.h"
#include "game/instances.h"
#include "game/math.h"
#include "gcc2.h"

class CameraSubtype;
struct ChunkData;
struct ChunkLinkData;
struct Reference;
struct TimeClock;

// The camera's shake (D_003BDB30, 0x90 bytes): an offset springing back to nothing (what's pushed into it added each step, then
// it goes on as far as it moved the step before, damped and kept under a limit), felt from the camera's instance (pushes come in
// its space). Nothing puts the offset on the camera: the rigs work out a shaken target from it and drop it
struct CameraShake
{
    Vector4 offset;
    Vector4 previous;
    Vector4 push;
    Reference* centre;
    Matrix4x4 toCentre;
    f32 limit;
    f32 unused84;
    f32 damping;
};
CHECK_OFFSET(CameraShake, centre, 0x30);
CHECK_OFFSET(CameraShake, limit, 0x80);
CHECK_SIZE(CameraShake, 0x90);

extern "C"
{
    extern CameraShake g_CameraShake RETAIL(D_003BDB30);

    // Made still (a limit of 1, a damping of 0.1), with no centre
    CameraShake* ConstructCameraShake(CameraShake* shake) RETAIL(FUN_0026fd60);
    // A push away from a point (none without a centre): a strength (falling off with the distance squared times the falloff
    // when that's above 0.0001) along the way from the point to the centre in the centre's space, jittered, and the pad
    // vibrated as strongly; the same with a strength along each axis (the way's signs taken, the strongest vibrating the pad)
    void PushCameraShake(CameraShake* shake, const Vector4* from, f32 strength, f32 falloff) RETAIL_N32(FUN_0026fe40);
    void PushCameraShakeAxes(CameraShake* shake, const Vector4* from, f32 xStrength, f32 yStrength, f32 zStrength, f32 falloff)
        RETAIL_N32(FUN_00270040);
    // A step: the centre's space taken again and the offset moved on
    void StepCameraShake(CameraShake* shake) RETAIL(FUN_00270398);
    void CameraShakeOffset(const CameraShake* shake, Vector4* offset) RETAIL(FUN_0027be10);
    // The pad vibrated for 0.4 seconds as strongly as a shake (its strength times 1024, from 20 to 255)
    void VibrateForShake(f32 strength) RETAIL(FUN_0027bd48);
}

// The curves the lens's blends and the scripted target's and positioner's moves take their share by: a smooth step (eased in and
// out) or, for any other value, even
enum CameraCurve : u32
{
    CurveEven = 0,
    CurveSmooth = 2,
};

// How a rig's point follower moves (its CameraPointFollower::Ways)
union CameraFollowerBits
{
    u32 value;
    struct
    {
        // Its way, and its own way (the one it goes back to)
        u32 way : 4;
        u32 ownWay : 4;
        // It keeps its rate while it's told others (counting it up to 100 every step instead)
        u32 keepsRate : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(CameraFollowerBits, 4);

// What smooths one of a rig's points (0x30 bytes, retail's vtable 0 bytes in: 1 the destructor, 2 taken as it is, 3 back to its own
// way, 4 a step toward a point (the point set to where it got), 5 the way a camera's subtype asks for, 6 a rate (below 0: straight
// there), 7 whether it keeps its rate while it's told others, 8 whether it lets its instance change chunks (its point moved through
// the link when the linked chunk is loaded)): its bits, its point, its rate and its own rate
class CameraPointFollower
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        TakeSlot = 2,
        ResetSlot = 3,
        StepSlot = 4,
        FollowSubtypeSlot = 5,
        SetRateSlot = 6,
        SetKeepsRateSlot = 7,
        CanChangeChunkSlot = 8,
    };

    // The share of the way to the point a step moves: all of it, the share its rate makes of the step's time, that squared, its
    // square root (any other way: none)
    enum Ways : u32
    {
        WayAtOnce = 0,
        WayLinear = 1,
        WaySquared = 2,
        WaySquareRoot = 3,
    };

    const GccVTableEntry* vtable;
    CameraFollowerBits bits;
    u8 unused08[8];
    Vector4 point;
    f32 rate;
    f32 ownRate;
    u8 unused28[8];

    static CameraPointFollower* Construct(CameraPointFollower* follower) RETAIL(FUN_0027de68);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027df20);
    // The base's destructor (its vtable's slot 1, every other slot abstract)
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0027e900);
    void Take(const Vector4* rotation, const Vector4* point) RETAIL(FUN_0027df50);
    void Reset() RETAIL(func_0027E0B0);
    void Step(TimeClock* clock, const Vector4* rotation, Vector4* point) RETAIL(func_0027A918);
    void FollowSubtype(const CameraSubtype* subtype) RETAIL(func_0027DF60);
    void SetRate(f32 rate) RETAIL_N32(func_0027E030);
    void SetKeepsRate(u32 keeps) RETAIL(FUN_0027de28);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0027e100);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void TakeVirtual(const Vector4* rotation, const Vector4* point)
    {
        CallVirtual<void>(this, vtable, TakeSlot, rotation, point);
    }

    void ResetVirtual()
    {
        CallVirtual<void>(this, vtable, ResetSlot);
    }

    void StepVirtual(TimeClock* clock, const Vector4* rotation, Vector4* point)
    {
        CallVirtual<void>(this, vtable, StepSlot, clock, rotation, point);
    }

    void FollowSubtypeVirtual(const CameraSubtype* subtype)
    {
        CallVirtual<void>(this, vtable, FollowSubtypeSlot, subtype);
    }

    void SetRateVirtual(f32 rate)
    {
        CallVirtual<void>(this, vtable, SetRateSlot, rate);
    }

    void SetKeepsRateVirtual(u32 keeps)
    {
        CallVirtual<void>(this, vtable, SetKeepsRateSlot, keeps);
    }

    u32 CanChangeChunkVirtual(ChunkData* from, ChunkLinkData* link)
    {
        return CallVirtual<u32>(this, vtable, CanChangeChunkSlot, from, link);
    }
};
CHECK_OFFSET(CameraPointFollower, point, 0x10);
CHECK_OFFSET(CameraPointFollower, rate, 0x20);
CHECK_SIZE(CameraPointFollower, 0x30);

// What a rig looks at (the base of its targets, retail's vtable 0x64 bytes in: 1 the destructor, 2 set back to its start, 3 a step,
// 4 the value it gives the positioner for a camera trigger's node, 5 whether it lets its instance change chunks (the base's moves
// its rotation and point through the link, and does)): the share along the camera trigger's geometry it's at, its rotation, the
// point looked at, the followed object's rotation, position and velocity, whether the camera goes behind the way it moves (else
// behind its rotation) and whether the point is followed smoothly
class CameraTarget
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ResetSlot = 2,
        StepSlot = 3,
        ValueSlot = 4,
        CanChangeChunkSlot = 5,
    };

    f32 along;
    u8 unused04[0xC];
    Vector4 rotation;
    Vector4 point;
    Vector4 objectRotation;
    Vector4 objectPosition;
    Vector4 velocity;
    u8 facesMovement;
    u8 smoothed;
    u8 unused62[2];
    const GccVTableEntry* vtable;

    // The base made in the subclasses' constructors: no turn, at the origin, followed smoothly
    static void ConstructBase(CameraTarget* target);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027b768);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0027b798);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void ResetVirtual()
    {
        CallVirtual<void>(this, vtable, ResetSlot);
    }

    void StepVirtual(TimeClock* clock)
    {
        CallVirtual<void>(this, vtable, StepSlot, clock);
    }

    f32 ValueVirtual(CameraNode* trigger)
    {
        return CallVirtual<f32>(this, vtable, ValueSlot, trigger);
    }

    u32 CanChangeChunkVirtual(ChunkData* from, ChunkLinkData* link)
    {
        return CallVirtual<u32>(this, vtable, CanChangeChunkSlot, from, link);
    }
};
CHECK_OFFSET(CameraTarget, point, 0x20);
CHECK_OFFSET(CameraTarget, objectPosition, 0x40);
CHECK_OFFSET(CameraTarget, facesMovement, 0x60);
CHECK_OFFSET(CameraTarget, smoothed, 0x61);
CHECK_OFFSET(CameraTarget, vtable, 0x64);

// Where a rig's camera is (the base of its positioners, 0x40 bytes, retail's vtable 0x34 bytes in: 1 the destructor, 2 set back to
// its start for an instance and a target, 3 a step with the target, 4 the target's value and a camera trigger's node taken with the target, 5
// whether it lets its instance change chunks (the base's moves its rotation and position through the link when the linked chunk
// is loaded, and does then), 6 whether the rig keeps its rotation (the base's doesn't)): its field of view (65536ths of a turn),
// its rotation and position, whether the position is followed smoothly and whether its follower keeps its rate meanwhile
class CameraPositioner
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ResetSlot = 2,
        StepSlot = 3,
        TakeSlot = 4,
        CanChangeChunkSlot = 5,
        KeepsRigRotationSlot = 6,
    };

    u32 unused00;
    s32 fov;
    u8 unused08[8];
    Vector4 rotation;
    Vector4 position;
    u8 smoothed;
    u8 keepsRate;
    u8 unused32[2];
    const GccVTableEntry* vtable;
    u8 unused38[8];

    // The base made in the subclasses' constructors: the default field of view, no turn, at the origin, followed smoothly
    static void ConstructBase(CameraPositioner* positioner);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027c210);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0027c240);
    u32 KeepsRigRotation() RETAIL(FUN_0027b760);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void ResetVirtual(InstanceContext* instance, CameraTarget* target)
    {
        CallVirtual<void>(this, vtable, ResetSlot, instance, target);
    }

    void StepVirtual(TimeClock* clock, CameraTarget* target)
    {
        CallVirtual<void>(this, vtable, StepSlot, clock, target);
    }

    void TakeVirtual(f32 value, CameraNode* trigger, CameraTarget* target)
    {
        CallVirtual<void>(this, vtable, TakeSlot, value, trigger, target);
    }

    u32 CanChangeChunkVirtual(ChunkData* from, ChunkLinkData* link)
    {
        return CallVirtual<u32>(this, vtable, CanChangeChunkSlot, from, link);
    }

    u32 KeepsRigRotationVirtual()
    {
        return CallVirtual<u32>(this, vtable, KeepsRigRotationSlot);
    }
};
CHECK_OFFSET(CameraPositioner, fov, 0x4);
CHECK_OFFSET(CameraPositioner, position, 0x20);
CHECK_OFFSET(CameraPositioner, vtable, 0x34);
CHECK_SIZE(CameraPositioner, 0x40);

// A camera rig's bits
union CameraRigBits
{
    u32 value;
    struct
    {
        // Its points are followed smoothly (where its parts ask); it takes nothing from the camera trigger
        u32 smoothed : 1;
        u32 ignoresTrigger : 1;
        // The parts it owns (destroyed with it)
        u32 ownsTargetFollower : 1;
        u32 ownsCameraFollower : 1;
        u32 ownsTarget : 1;
        u32 ownsPositioner : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(CameraRigBits, 4);

// A camera rig (the base, 0x40 bytes, retail's vtable 0x30 bytes in: 1 the destructor (the parts it owns destroyed), 2 set back to
// its start for an instance (its target and positioner, their followers jumping to them), 3 a step, 4 whether it lets its instance
// change chunks (when the linked chunk is loaded and every part does)): its bits, the camera trigger's node whose camera it takes,
// the followers smoothing what the target looks at and where the positioner puts the camera, the target and the positioner, and
// the rotation it keeps (while the positioner asks)
class CameraRig
{
public:
    // Its vtable's functions, and 5 to 7, which every rig the game makes has (the base's vtable stops at 4)
    enum Slot : u32
    {
        DestroySlot = 1,
        ResetSlot = 2,
        StepSlot = 3,
        CanChangeChunkSlot = 4,
        PrepareSlot = 5,
        RestoreDefaultsSlot = 6,
        AssembleSlot = 7,
    };

    CameraRigBits bits;
    CameraNode* trigger;
    CameraPointFollower* targetFollower;
    CameraPointFollower* cameraFollower;
    CameraTarget* target;
    CameraPositioner* positioner;
    u8 unused18[8];
    Vector4 keptRotation;
    const GccVTableEntry* vtable;
    u8 unused34[0xC];

    static CameraRig* Construct(CameraRig* rig) RETAIL(FUN_0027b948);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0026f228);
    void Reset(InstanceContext* instance) RETAIL(FUN_0026f330);
    void Step(TimeClock* clock) RETAIL(FUN_0026f3f0);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0026f778);
    // A part let go (destroyed when it owns it; the bit left for the caller)
    void DropTargetFollower() RETAIL(FUN_0027b830);
    void DropCameraFollower() RETAIL(FUN_0027b888);
    void DropTarget() RETAIL(FUN_0027b8e0);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void ResetVirtual(InstanceContext* instance)
    {
        CallVirtual<void>(this, vtable, ResetSlot, instance);
    }

    void StepVirtual(TimeClock* clock)
    {
        CallVirtual<void>(this, vtable, StepSlot, clock);
    }

    u32 CanChangeChunkVirtual(ChunkData* from, ChunkLinkData* link)
    {
        return CallVirtual<u32>(this, vtable, CanChangeChunkSlot, from, link);
    }
};
CHECK_OFFSET(CameraRig, keptRotation, 0x20);
CHECK_OFFSET(CameraRig, vtable, 0x30);
CHECK_SIZE(CameraRig, 0x40);

// The cutscenes' positioner (retail's vtable D_00304CF0): set back, stepped and given values without doing anything (the cutscenes'
// commands place it)
class CutscenePositioner : public CameraPositioner
{
public:
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027cd20);
    void Reset(InstanceContext* instance, CameraTarget* target) RETAIL(FUN_0027cd58);
    void Clear() RETAIL(FUN_0027cd88);
    void Step(TimeClock* clock, CameraTarget* target) RETAIL(FUN_0027cd90);
    void Take(f32 value, CameraNode* trigger, CameraTarget* target) RETAIL_N32(FUN_0027cd78);
};
CHECK_SIZE(CutscenePositioner, 0x40);

// The cutscenes' camera rig (the game controller's, 0x80 bytes, retail's vtable D_00304BE8: 5 made ready for play (put together
// again), 6 back to its defaults (nothing), 7 put together (with its own positioner)): the positioner the cutscenes' commands place
class CutsceneCameraRig : public CameraRig
{
public:
    CutscenePositioner ownPositioner;

    static CutsceneCameraRig* Construct(CutsceneCameraRig* rig) RETAIL(FUN_0027e7a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027e888);
    void Prepare(InstanceContext* player) RETAIL(FUN_0027e8b8);
    void RestoreDefaults() RETAIL(FUN_0027e8e0);
    void Assemble() RETAIL(FUN_0027e8e8);

    void AssembleVirtual()
    {
        CallVirtual<void>(this, vtable, AssembleSlot);
    }
};
CHECK_OFFSET(CutsceneCameraRig, ownPositioner, 0x40);
CHECK_SIZE(CutsceneCameraRig, 0x80);

// The scripted camera's target's bits
union ScriptedTargetBits
{
    u32 value;
    struct
    {
        // It moves (its move started), by its curve (a CameraCurve)
        u32 moving : 1;
        u32 curve : 3;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(ScriptedTargetBits, 4);

// The scripted camera's target (0xB0 bytes, retail's vtable D_00304D30: 4 no value): what the cutscenes' commands have it look
// at, moved from a point to another over a time (along a path when it has one: the path's point at the share, from the start
// half way there, plus its direction there), its share even or eased in and out
class ScriptedCameraTarget : public CameraTarget
{
public:
    ScriptedTargetBits bits;
    u8 unused74[0xC];
    Vector4 start;
    Vector4 end;
    u32 moveStart;
    s32 moveTicks;
    class LayoutPath* path;
    u32 unusedAC;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027cbf8);
    void Reset() RETAIL(FUN_0027cc28);
    void Step(TimeClock* clock) RETAIL(FUN_00279208);
    f32 Value(CameraNode* node) RETAIL(func_0027CBD8);
    // Its move's share of the way (started the first time: 0; 1 once it's done, the move stopped)
    f32 MoveShare(TimeClock* clock) RETAIL(FUN_002793b0);
    // Its move stopped where it is
    void Stop() RETAIL(FUN_0027ccf0);
};
CHECK_OFFSET(ScriptedCameraTarget, bits, 0x70);
CHECK_OFFSET(ScriptedCameraTarget, start, 0x80);
CHECK_OFFSET(ScriptedCameraTarget, moveStart, 0xA0);
CHECK_SIZE(ScriptedCameraTarget, 0xB0);

// The scripted camera's positioner's bits
union ScriptedPositionerBits
{
    u32 value;
    struct
    {
        // Its move eases in or out over its ease's time, by its curve (a CameraCurve), arcing round the target
        u32 easesIn : 1;
        u32 easesOut : 1;
        u32 curve : 3;
        u32 arcs : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(ScriptedPositionerBits, 4);

// The scripted camera's positioner (0x90 bytes, retail's vtable D_00304D68: 4 takes nothing): where the cutscenes' commands put
// the camera, moved from a place to another over a time (along a path when it has one, or keeping the distance from the target
// blended between the ends' when it arcs) with its field of view, looking at the target; its share even (or eased in and out),
// eased in or out over a time of its own at the start or the end
class ScriptedCameraPositioner : public CameraPositioner
{
public:
    ScriptedPositionerBits bits;
    // (The cutscene command's word 0x2C given to it)
    u32 unused44;
    u8 unused48[8];
    Vector4 start;
    Vector4 end;
    s32 startFov;
    s32 endFov;
    u32 moveStart;
    s32 moveTicks;
    u32 easeStart;
    s32 easeTicks;
    class LayoutPath* path;
    u32 unused8C;

    static ScriptedCameraPositioner* Construct(ScriptedCameraPositioner* positioner) RETAIL(FUN_00278798);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027ca40);
    void Reset(InstanceContext* instance, CameraTarget* target) RETAIL(FUN_0027ca70);
    void Step(TimeClock* clock, CameraTarget* target) RETAIL(FUN_00278960);
    void Take(f32 value, CameraNode* trigger, CameraTarget* target) RETAIL_N32(FUN_0027ca90);
    // Its move and its bits cleared (its rotation all zeros, at the origin, the default field of view), and its move stopped
    // where it is
    void Clear() RETAIL(FUN_00278840);
    void Stop() RETAIL(FUN_0027cb48);
    // Its move's share of the way at a time (started the first time; the ease's share added in or out), its even share (or its
    // smooth step) and its ease's (reversed when it eases out)
    f32 MoveShare(TimeClock* clock) RETAIL(FUN_00278da8);
    f32 LinearShare(const u32* now) RETAIL(FUN_00279040);
    f32 EaseShare(const u32* now) RETAIL(FUN_00279118);
    // A rotation made to look from an eye at a point
    void LookAt(const Vector4* point, Vector4* turn, const Vector4* eye) RETAIL(FUN_0027cac0);
};
CHECK_OFFSET(ScriptedCameraPositioner, bits, 0x40);
CHECK_OFFSET(ScriptedCameraPositioner, start, 0x50);
CHECK_OFFSET(ScriptedCameraPositioner, startFov, 0x70);
CHECK_OFFSET(ScriptedCameraPositioner, path, 0x88);
CHECK_SIZE(ScriptedCameraPositioner, 0x90);

// The cutscenes' commands' state in the game's camera rig
union GameRigScriptBits
{
    u32 value;
    struct
    {
        // (Bits 0 and 1 cleared and bit 2 set when the rig's made)
        u32 unused0 : 2;
        u32 unused2 : 1;
        // The framing's side angles and yaw turned the other way
        u32 mirrored : 1;
        // The frame's to be made (either), it changed since
        u32 frameWanted : 1;
        u32 frameChanged : 1;
        // A place is given as such (not an object's)
        u32 hasFirstPlace : 1;
        u32 hasSecondPlace : 1;
        // The frame's up is the world's (else square to the way and its side)
        u32 worldUp : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(GameRigScriptBits, 4);

// The game's camera rig (the game controller's, 0x260 bytes, retail's vtable D_00304C30: 5 made ready for play (its followers'
// own ways and rates: where it looks by the square root, where it is linear, both at 6), 6 back to its defaults (nothing), 7 put
// together (with its own parts)): the cutscenes' commands' state (its bits, the places of what they name and the paths their
// moves go along), its followers, its scripted target and positioner
class GameCameraRig : public CameraRig
{
public:
    GameRigScriptBits scriptBits;
    // The objects whose places the commands name (the first's place, its place ahead along its z axis without a second one)
    ReferencedObject* firstObject;
    ReferencedObject* secondObject;
    u32 unused4C;
    Vector4 firstPlace;
    Vector4 secondPlace;
    // Looking from the first place at the second (its columns the side, the up and the way)
    Matrix4x4 frame;
    u32 unusedB0;
    // The paths the commands move the scripted target and positioner along (an agent's waypoints')
    class LayoutPath* targetPath;
    class LayoutPath* cameraPath;
    u32 unusedBC;
    CameraPointFollower ownTargetFollower;
    CameraPointFollower ownCameraFollower;
    ScriptedCameraTarget ownTarget;
    ScriptedCameraPositioner ownPositioner;

    static GameCameraRig* Construct(GameCameraRig* rig) RETAIL(FUN_0027ad50);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027e6c8);
    void Prepare(InstanceContext* player) RETAIL(FUN_0027e718);
    void RestoreDefaults() RETAIL(FUN_0027e770);
    void Assemble() RETAIL(FUN_0027e778);
    // The commands' state set back (the places cleared, their bits off), and either place cleared
    void ResetScript() RETAIL(FUN_0027e608);
    // The frame made again from the places (the objects' ones taken first) when it's wanted
    void MakeFrame() RETAIL(FUN_0027af78);
    void ClearFirstPlace() RETAIL(FUN_0027e508);
    void ClearSecondPlace() RETAIL(FUN_0027e568);
};
CHECK_OFFSET(GameCameraRig, firstPlace, 0x50);
CHECK_OFFSET(GameCameraRig, frame, 0x70);
CHECK_OFFSET(GameCameraRig, targetPath, 0xB4);
CHECK_OFFSET(GameCameraRig, ownTargetFollower, 0xC0);
CHECK_OFFSET(GameCameraRig, ownTarget, 0x120);
CHECK_OFFSET(GameCameraRig, ownPositioner, 0x1D0);
CHECK_SIZE(GameCameraRig, 0x260);

extern "C"
{
    extern const GccVTableEntry g_CutsceneCameraRigVTable[] RETAIL(D_00304BE8);
    extern const GccVTableEntry g_CutscenePositionerVTable[] RETAIL(D_00304CF0);
    extern const GccVTableEntry g_GameCameraRigVTable[] RETAIL(D_00304C30);
    extern const GccVTableEntry g_ScriptedCameraTargetVTable[] RETAIL(D_00304D30);
    extern const GccVTableEntry g_ScriptedCameraPositionerVTable[] RETAIL(D_00304D68);

    // Where a rig puts the camera given its matrix: its positioner's field of view (the default without one), rotation and
    // position (the matrix's without one); with a target, the shake stepped (its shaken target dropped) and the rotation the one
    // kept while the positioner asks (or there's none), kept again. The field of view returned through the first argument, as
    // GCC 2.9x returns such a struct
    s32* CameraRigPlace(s32* fov, CameraRig* rig, const Matrix4x4* matrix, Vector4* rotation, Vector4* position)
        RETAIL(FUN_0026f880);
    // The field of view a positioner gives without one (the rigs' default)
    extern s32 g_DefaultFov RETAIL(D_0030A7D0);
    // Angles the camera module sets at start-up that nothing reads (8 bytes apart: 45 degrees given as radians, 135, -135, 10,
    // 0, 720, 90, 5, 360, 90, 5, 720, 180 three times, 60, 120, -45, 75, 7, then 180 and 90), and a vector set to (0, 0, -1, 1)
    extern s32 g_UnreadCameraAngle9A0 RETAIL(D_0030A9A0);
    extern s32 g_UnreadCameraAngle9B0 RETAIL(D_0030A9B0);
    extern s32 g_UnreadCameraAngle9B8 RETAIL(D_0030A9B8);
    extern s32 g_UnreadCameraAngle9C8 RETAIL(D_0030A9C8);
    extern s32 g_UnreadCameraAngle9D0 RETAIL(D_0030A9D0);
    extern s32 g_UnreadCameraAngle9E0 RETAIL(D_0030A9E0);
    extern s32 g_UnreadCameraAngle9E8 RETAIL(D_0030A9E8);
    extern s32 g_UnreadCameraAngle9F0 RETAIL(D_0030A9F0);
    extern s32 g_UnreadCameraAngle9F8 RETAIL(D_0030A9F8);
    extern s32 g_UnreadCameraAngleA00 RETAIL(D_0030AA00);
    extern s32 g_UnreadCameraAngleA08 RETAIL(D_0030AA08);
    extern s32 g_UnreadCameraAngleA10 RETAIL(D_0030AA10);
    extern s32 g_UnreadCameraAngleA18 RETAIL(D_0030AA18);
    extern s32 g_UnreadCameraAngleA20 RETAIL(D_0030AA20);
    extern s32 g_UnreadCameraAngleA28 RETAIL(D_0030AA28);
    extern s32 g_UnreadCameraAngleA30 RETAIL(D_0030AA30);
    extern s32 g_UnreadCameraAngleA38 RETAIL(D_0030AA38);
    extern s32 g_UnreadCameraAngleA40 RETAIL(D_0030AA40);
    extern s32 g_UnreadCameraAngleA48 RETAIL(D_0030AA48);
    extern s32 g_UnreadCameraAngleA50 RETAIL(D_0030AA50);
    extern s32 g_UnreadCameraAngleA58 RETAIL(D_0030AA58);
    extern s32 g_UnreadCameraAngleA60 RETAIL(D_0030AA60);
    extern Vector4 g_UnreadCameraVector RETAIL(D_003BDC00);
    // The camera module's start-up (the shake made, the angles set) and its entry in the start-up list
    void InitCameraModule(u32 initialise, u32 priority) RETAIL(FUN_0027b358);
    void ConstructCameraModule() RETAIL(FUN_0027ea00);
}

// A camera lens's bits
union CameraLensBits
{
    u32 value;
    struct
    {
        // The projection's to be made again (every update asks)
        u32 projectionChanged : 1;
        // It owns its rig, and the next one
        u32 ownsRig : 1;
        u32 ownsNext : 1;
        // The blend's curve (a CameraCurve)
        u32 curve : 3;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(CameraLensBits, 4);

// A camera's lens (its instance's node of kind 9, 0x3C bytes, retail's vtable UnkNode_0x9_Methods: 4 whether the rigs let its
// instance change chunks, 7 a step (its rigs set back to their start), 8 the update (the camera placed by its rigs), 10 its type
// 0x141F): its bits, the field of view (65536ths of a turn across), the pixels' aspect (1 on a PAL TV, 0.96 on an NTSC one), the
// near and far planes, the rig it shows and the one it blends to, when the blend started (clock units, 0 not yet) and how long it
// takes
struct CameraLensNode : GameNode
{
    static constexpr u32 TypeId = 0x141F;

    CameraLensBits bits;
    s32 fov;
    f32 pixelAspect;
    f32 nearPlane;
    f32 farPlane;
    CameraRig* rig;
    CameraRig* next;
    u32 blendStart;
    s32 blendTicks;

    static CameraLensNode* Construct(CameraLensNode* node) RETAIL(FUN_0027be38);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027bef0);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0027bfb8);
    u32 Kind() RETAIL(GetNodeIndex_0x9);
    void Step(TimeClock* clock, u32 way) RETAIL(FUN_0027c068);
    u32 Update(TimeClock* clock) RETAIL(FUN_00270ad8);
    u32 Type() RETAIL(FUN_0027b600);
    // The rig it shows (the old one destroyed when it owned it; any blend dropped, the next rig destroyed when it owned it),
    // set back to its start when asked: the rig
    CameraRig* SetRig(CameraRig* rig, u32 reset) RETAIL(FUN_0027c0e0);
    // A blend to a rig over a time with a curve (a blend going on finished at once first; none to the rig it shows; the rig shown
    // at once when it has none), the rig set back to its start: the rig it blends to
    CameraRig* BlendTo(CameraRig* rig, s32 ticks, u8 curve) RETAIL(FUN_00270918);
};
CHECK_OFFSET(CameraLensNode, bits, 0x18);
CHECK_OFFSET(CameraLensNode, rig, 0x2C);
CHECK_SIZE(CameraLensNode, 0x3C);

extern "C"
{
    extern const GccVTableEntry g_CameraLensNodeVTable[] RETAIL(UnkNode_0x9_Methods);
    extern const GccVTableEntry g_CameraRigVTable[] RETAIL(D_00305148);
    extern const GccVTableEntry g_CameraPointFollowerVTable[] RETAIL(D_00304CA0);
    extern const GccVTableEntry g_CameraFollowerBaseVTable[] RETAIL(D_00305310);
    extern const GccVTableEntry g_CameraTargetVTable[] RETAIL(D_00305208);
    extern const GccVTableEntry g_CameraPositionerVTable[] RETAIL(D_00305240);

    // A step of a lens's rigs (the blend's share of the way taken from its clock, the rig blended to taking over once it's
    // done): the field of view, the matrix given the rotation and position, both blended between the two rigs (the rotation
    // slerped). The field of view returned through the first argument, as GCC 2.9x returns such a struct
    s32* CameraLensBlend(s32* fov, CameraLensNode* lens, TimeClock* clock, Matrix4x4* matrix) RETAIL(FUN_002705d8);
}
