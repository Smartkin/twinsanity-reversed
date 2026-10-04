#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"

// The look: a pitch and a yaw (radians) on springs, shared out over the head's and the spine's joints 0-4 as turns, following the
// point the head tracks, the camera's free look or a carried body; and the Mecha-Bandicoot's gun arm turned toward its aim

EABI_EXPORT(FUN_00147448, TurnAnglesToward);
EABI_EXPORT(FUN_0015eda8, &LookController::SetCarried);

namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 ModelNodeKind = 3;
// The characters (their first int property): Crash's second joint takes the yaw about y, the others' about z
constexpr s32 Crash = 0;
constexpr s32 MechaBandicoot = 5;
// The head's exit point
constexpr u32 HeadExitPoint = 1;
// The joints the look turns (its angles' joints), the Mecha-Bandicoot's three of them, its gun arm and the joint made with it
constexpr u32 LookJointCount = 5;
constexpr u32 MechaLookJointCount = 3;
constexpr u32 MechaLookJoints[MechaLookJointCount] = {0, 2, 4};
constexpr u32 MechaArm = 14;
constexpr u32 MechaArmEnd = 18;
constexpr u32 MechaJoints[] = {0, 2, 4, MechaArm, MechaArmEnd};

constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr f32 RadiansToAngle = 0x1.45f306p+13f;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// How fast the joints' angles follow their targets and the arm its aim, and the pitch and the yaw relax (radians a second)
constexpr f32 TurnRadiansPerSecond = Rounded(1.6);
constexpr f32 ArmRadiansPerSecond = Rounded(1.3);
constexpr f32 RelaxRadiansPerSecond = 32.0f;
// The springs: toward a carried body's lean, the tracked point and back to rest (stiffness and damping)
constexpr f32 FollowStiffness = 26.0f;
constexpr f32 FollowDamping = Rounded(5.6);
constexpr f32 RestStiffness = 20.0f;
constexpr f32 RestDamping = 5.0f;
// A carried body: its forward offset grows by its lean (from its size) every frame, and the yaw and pitch lean from its offsets
constexpr f32 CarryLeanBase = Rounded(0.56);
constexpr f32 CarryLeanScale = -5.0f;
constexpr f32 CarryYawShare = Rounded(-1.1);
constexpr f32 CarryPitchShare = Rounded(-1.2);
// The camera's free look past this (its length squared) turns the look directly
constexpr f32 FreeLookLeast = Rounded(0.01);
constexpr f32 FreeLookPitchSpeed = 5.0f;
constexpr f32 FreeLookYawSpeed = -8.0f;
// The tracked point is followed within about 45 degrees of the facing (the cosine)
constexpr f32 FollowCosine = Rounded(0.7);
// The arm aims this far ahead without an aim point
constexpr f32 ArmAhead = 1000.0f;
// AimRotation's turn below this (its axis's length squared) is none
constexpr f32 AimAxisLeast = Rounded(5e-05);

// The joints' shares of the pitch and the yaw: standing (and relaxing), carrying, the Mecha-Bandicoot's
constexpr f32 StandingShares[LookJointCount] = {0.25f, Rounded(0.4), Rounded(0.3), Rounded(0.2), Rounded(0.2)};
constexpr f32 CarryShares[LookJointCount] = {Rounded(-0.3), Rounded(-0.3), Rounded(0.05), Rounded(0.35), Rounded(0.35)};
constexpr f32 MechaShare = 0.25f;

s32 CharacterOf(const CharacterAgent* agent)
{
    return agent->properties->GetInt(0);
}

ObjectPlace* PlaceOf(const CharacterAgent* agent)
{
    return agent->instance->place;
}

f32 SecondsOf(const TimeClock* clock)
{
    return static_cast<s32>(clock->advance) * g_SecondsPerClockUnit;
}

f32 Dot(const Vector4* a, const Vector4* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// Radians cut down to whole 65536ths of a turn, and a share of such an angle cut down again
s32 AngleOf(f32 radians)
{
    return static_cast<s32>(radians * RadiansToAngle);
}

s32 Share(s32 angle, f32 share)
{
    return static_cast<s32>(static_cast<f32>(angle) * share);
}

void SetTurn(s32* turn, s32 x, s32 y, s32 z)
{
    turn[0] = x;
    turn[1] = y;
    turn[2] = z;
}

// The joints' targets standing (and relaxing): the pitch about x, the yaw about -z for the lower joints (Crash's second about y)
// and about y for the upper ones
void StandingTargets(const LookController* look, s32 (*targets)[3])
{
    s32 pitch = AngleOf(look->pitch);
    s32 yaw = AngleOf(look->yaw);
    s32 yawBack = AngleOf(-look->yaw);
    SetTurn(targets[0], Share(pitch, StandingShares[0]), 0, Share(yawBack, StandingShares[0]));
    if (CharacterOf(look->agent) == Crash)
    {
        SetTurn(targets[1], Share(pitch, StandingShares[1]), Share(yaw, StandingShares[1]), 0);
    }
    else
    {
        SetTurn(targets[1], Share(pitch, StandingShares[1]), 0, Share(yawBack, StandingShares[1]));
    }

    for (u32 joint = 2; joint < LookJointCount; joint++)
    {
        SetTurn(targets[joint], Share(pitch, StandingShares[joint]), Share(yaw, StandingShares[joint]), 0);
    }
}

// Carrying: the first joint takes the yaw about -z, the others about y, by other shares
void CarryTargets(const LookController* look, s32 (*targets)[3])
{
    s32 pitch = AngleOf(look->pitch);
    s32 yaw = AngleOf(look->yaw);
    s32 yawBack = AngleOf(-look->yaw);
    SetTurn(targets[0], Share(pitch, CarryShares[0]), 0, Share(yawBack, CarryShares[0]));
    for (u32 joint = 1; joint < LookJointCount; joint++)
    {
        SetTurn(targets[joint], Share(pitch, CarryShares[joint]), Share(yaw, CarryShares[joint]), 0);
    }
}

void TurnJoints(LookController* look, f32 seconds, s32 (*targets)[3])
{
    for (u32 joint = 0; joint < LookJointCount; joint++)
    {
        TurnAnglesToward(seconds, look->angles[joint], targets[joint]);
    }
}

// An angle brought toward 0 by a step, not past it
void RelaxAngle(f32* angle, f32 step)
{
    if (0.0f < *angle)
    {
        *angle = *angle - step;
        if (*angle < 0.0f)
        {
            *angle = 0.0f;
        }
    }
    else if (*angle < 0.0f)
    {
        *angle = *angle + step;
        if (0.0f < *angle)
        {
            *angle = 0.0f;
        }
    }
}

// The springs pulled toward the point the head tracks while it's within about 45 degrees of the facing, measured on the head's
// axes: whether they were
bool FollowPoint(LookController* look, f32 seconds)
{
    CharacterAgent* agent = look->agent;
    OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&agent->instance->nodes, ModelNodeKind))->animator;
    // Retail asks which character it is and takes the same exit point for each
    static_cast<void>(CharacterOf(agent));
    ExitPointAnimation* exitPoint = animator->exitPoints != nullptr ? animator->exitPoints->data[HeadExitPoint] : nullptr;
    const Matrix4x4* head = &UpdateExitPointMatrix(exitPoint)->matrix;
    const Vector4* at = RowOf(head, 3);
    Vector4 toward = {look->lookPoint.x - at->x, look->lookPoint.y - at->y, look->lookPoint.z - at->z, 1.0f};
    f32 inverse = InverseLength(&toward, LengthEpsilon);
    toward.x = toward.x * inverse;
    toward.y = toward.y * inverse;
    toward.z = toward.z * inverse;
    ObjectPlace* place = PlaceOf(agent);
    RotateAndTranslate(place);
    if (!(FollowCosine < Dot(&toward, RowOf(&place->matrix, 2))))
    {
        return false;
    }

    f32 across = Dot(RowOf(head, 0), &toward);
    f32 up = Dot(RowOf(head, 1), &toward);
    look->yawSpeed = look->yawSpeed + (across * FollowStiffness - look->yawSpeed * FollowDamping) * seconds;
    look->pitchSpeed = look->pitchSpeed + (up * -FollowStiffness - look->pitchSpeed * FollowDamping) * seconds;
    return true;
}

// The Mecha-Bandicoot's gun arm (its joint's matrix made already) turned toward its aim point, or 1000 units straight ahead
// without one, by at most 1.3 radians a second: the aim kept turns the joint's matrix in place
void AimArm(LookController* look, Matrix4x4* matrix)
{
    Vector4 turn;
    if (look->hasAimPoint != 0)
    {
        look->AimRotation(matrix, 0, 1, &look->aimPoint, &turn);
    }
    else
    {
        ObjectPlace* place = PlaceOf(look->agent);
        RotateAndTranslate(place);
        Vector4 position = *RowOf(&place->matrix, 3);
        place = PlaceOf(look->agent);
        RotateAndTranslate(place);
        Vector4 forward = *RowOf(&place->matrix, 2);
        Vector4 ahead = {forward.x * ArmAhead, forward.y * ArmAhead, forward.z * ArmAhead, 1.0f};
        Vector4 point = {position.x + ahead.x, position.y + ahead.y, position.z + ahead.z, 1.0f};
        look->AimRotation(matrix, 0, 1, &point, &turn);
    }

    Vector4 undo = look->aim;
    InvertRotation(&undo);
    Vector4 change;
    MultiplyRotations(&change, &turn, &undo);
    Vector4 axis;
    s32 angle;
    AxisAngleOfRotation(&change, &axis, &angle, 0);
    f32 most = SecondsOf(GetContextClock(look->agent->instance)) * ArmRadiansPerSecond;
    if (AngleOf(most) < angle)
    {
        AngleFrom(&angle, most, AngleRadians);
    }
    else
    {
        most = -most;
        if (angle < AngleOf(most))
        {
            AngleFrom(&angle, most, AngleRadians);
        }
    }

    RotationAboutAxis(&change, &axis, &angle, 0);
    Vector4 aimed = look->aim;
    MultiplyRotations(&look->aim, &change, &aimed);
    Matrix4x4 turned;
    MatrixFromRotation(&turned, &look->aim);
    *RowOf(&turned, 3) = g_DefaultBox.min;
    RowOf(&turned, 3)->w = 1.0f;
    Matrix4x4 joint = *matrix;
    Vector4 position = *RowOf(matrix, 3);
    *RowOf(&joint, 3) = g_DefaultBox.min;
    RowOf(&joint, 3)->w = 1.0f;
    VuMultiplyMatrices(&joint, &turned, matrix);
    *RowOf(matrix, 3) = position;
}
}

void TurnAnglesToward(f32 seconds, s32* angles, const s32* targets)
{
    for (u32 axis = 0; axis < 3; axis++)
    {
        f32 most = seconds * TurnRadiansPerSecond;
        f32 turn = static_cast<f32>(targets[axis] - angles[axis]) * AngleToRadians;
        if (most < turn)
        {
            turn = most;
        }
        else if (turn < -most)
        {
            turn = -most;
        }

        angles[axis] += AngleOf(turn);
    }
}

LookController* LookController::Construct(LookController* look, CharacterAgent* agent)
{
    look->vtable = g_LookControllerVTable;
    look->agent = agent;
    look->attached = 0;
    look->pitchMin = -1.0f;
    look->pitchMax = 0.5f;
    look->yawMin = -1.0f;
    look->yawMax = 1.0f;
    look->Reset();
    look->carryPitchMax = look->pitchMax + look->pitchMax;
    // Retail bug: restSeconds is left as the heap had it, so a new look relaxes for that long when it's above 0
    return look;
}

void LookController::Reset()
{
    pitch = 0.0f;
    yaw = 0.0f;
    pitchSpeed = 0.0f;
    yawSpeed = 0.0f;
    carrySide = 0.0f;
    carryForward = 0.0f;
    carrying = 0;
    lookPoint = g_DefaultBox.min;
    lookPoint.w = 1.0f;
    looksAtPoint = 0;
    freeLookY = 0.0f;
    freeLookX = 0.0f;
    for (u32 joint = 0; joint < LookJointCount; joint++)
    {
        SetTurn(angles[joint], 0, 0, 0);
    }

    aim = {0.0f, 0.0f, 0.0f, 1.0f};
    hasAimPoint = 0;
}

void LookController::Destroy(u32 destroyFlags)
{
    vtable = g_LookControllerVTable;
    if (attached != 0)
    {
        DetachFromModelAnimator(this, agent->instance);
    }

    vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void LookController::Attach(OgiAnimator* animator)
{
    if (CharacterOf(agent) == MechaBandicoot)
    {
        for (u32 joint : MechaJoints)
        {
            AddJointCallback(animator, joint, this);
        }

        return;
    }

    for (u32 joint = 0; joint < LookJointCount; joint++)
    {
        AddJointCallback(animator, joint, this);
    }
}

void LookController::Detach(OgiAnimator* animator)
{
    if (CharacterOf(agent) == MechaBandicoot)
    {
        for (u32 joint : MechaJoints)
        {
            RemoveJointCallback(animator, joint, this);
        }

        return;
    }

    for (u32 joint = 0; joint < LookJointCount; joint++)
    {
        RemoveJointCallback(animator, joint, this);
    }
}

void LookController::SetCarried(f32 side, f32 forward, f32 size, u32 carrying)
{
    this->carrying = carrying;
    carrySide = side;
    carryForward = forward;
    carrySize = size;
}

void LookController::SetLookPoint(const Vector4* point, u32 has)
{
    lookPoint = *point;
    looksAtPoint = has;
}

void LookController::SetAimPoint(const Vector4* point, u32 has)
{
    aimPoint = *point;
    hasAimPoint = has;
}

void LookController::StepLook(TimeClock* clock)
{
    if (CharacterOf(agent) == MechaBandicoot && hasAimPoint != 0)
    {
        looksAtPoint = 1;
        lookPoint = aimPoint;
    }

    u32 state = G_GameController_0030988C->State();
    bool lookingNowhere = state == GameController::StateWatching || state == GameController::StateTitle;
    if (!(restSeconds <= 0.0f))
    {
        return;
    }

    f32 seconds = SecondsOf(clock);
    bool moved = false;
    if (lookingNowhere)
    {
        looksAtPoint = 0;
        carrying = 0;
    }
    else
    {
        HeadTracking* tracking = static_cast<ObjectNode*>(GetGameNode(&agent->instance->nodes, ObjectNodeKind))->headTracking;
        if (tracking == nullptr || (tracking->flags & HeadTracking::FlagIgnoredByLook) != 0
            || (tracking->flags & HeadTracking::FlagTracking) == 0)
        {
            looksAtPoint = 0;
        }

        if (carrying != 0)
        {
            // Retail adds the lean to carryForward every frame (harmless: the carry code sets it again every frame)
            f32 lean = (1.0f / (carrySize + 1.0f) - CarryLeanBase) * CarryLeanScale;
            carryForward = carryForward + lean;
            yawSpeed = yawSpeed + ((carrySide * CarryYawShare - yaw) * FollowStiffness - yawSpeed * FollowDamping) * seconds;
            pitchSpeed = pitchSpeed
                + ((carryForward * CarryPitchShare + lean - pitch) * FollowStiffness - pitchSpeed * FollowDamping) * seconds;
            moved = true;
        }
        else if (FreeLookLeast < freeLookX * freeLookX + freeLookY * freeLookY)
        {
            pitchSpeed = freeLookY * FreeLookPitchSpeed;
            yawSpeed = freeLookX * FreeLookYawSpeed;
            moved = true;
        }
        else if (looksAtPoint != 0 && (static_cast<CharacterPart*>(agent->part)->moveBits & CharacterPart::Crouching) == 0)
        {
            moved = FollowPoint(this, seconds);
        }
    }

    if (!moved)
    {
        yawSpeed = yawSpeed + (-yaw * RestStiffness - yawSpeed * RestDamping) * seconds;
        pitchSpeed = pitchSpeed + (-pitch * RestStiffness - pitchSpeed * RestDamping) * seconds;
    }

    pitch = pitch + pitchSpeed * seconds;
    yaw = yaw + yawSpeed * seconds;
    f32 highest = carrying != 0 ? carryPitchMax : pitchMax;
    if (pitch < pitchMin)
    {
        pitch = pitchMin;
    }
    else if (highest < pitch)
    {
        pitch = highest;
    }

    if (yaw < yawMin)
    {
        yaw = yawMin;
    }
    else if (yawMax < yaw)
    {
        yaw = yawMax;
    }
}

void LookController::Frame(TimeClock* clock)
{
    f32 seconds = SecondsOf(clock);
    if (0.0f < restSeconds)
    {
        Relax(clock);
        restSeconds = restSeconds - seconds;
        return;
    }

    if (attached == 0)
    {
        AttachToModelAnimator(this, agent->instance);
        attached = 1;
    }

    StepLook(clock);
    s32 targets[LookJointCount][3];
    if (CharacterOf(agent) == MechaBandicoot)
    {
        s32 pitchTurn = Share(AngleOf(pitch), MechaShare);
        s32 yawTurn = Share(AngleOf(yaw), MechaShare);
        for (u32 joint = 0; joint < MechaLookJointCount; joint++)
        {
            SetTurn(targets[joint], pitchTurn, yawTurn, 0);
        }

        // Retail bug: the last two targets are left as the stack had them, their angles turn toward leftovers (never read for it)
    }
    else if (carrying != 0)
    {
        CarryTargets(this, targets);
    }
    else
    {
        StandingTargets(this, targets);
    }

    TurnJoints(this, seconds, targets);
}

void LookController::Relax(TimeClock* clock)
{
    if (attached == 0)
    {
        return;
    }

    f32 seconds = SecondsOf(clock);
    RelaxAngle(&pitch, seconds * RelaxRadiansPerSecond);
    RelaxAngle(&yaw, seconds * RelaxRadiansPerSecond);
    s32 targets[LookJointCount][3];
    StandingTargets(this, targets);
    TurnJoints(this, seconds, targets);
}

void LookController::AimRotation(const Matrix4x4* joint, u32 axis, u32 negate, const Vector4* point, Vector4* rotation)
{
    ObjectPlace* place = PlaceOf(agent);
    RotateAndTranslate(place);
    Vector4 along;
    VuRotateVector(&place->matrix, RowOf(joint, axis), &along);
    f32 inverse = InverseLength(&along, LengthEpsilon);
    along.x = along.x * inverse;
    along.y = along.y * inverse;
    along.z = along.z * inverse;
    if (negate != 0)
    {
        along.x = -along.x;
        along.y = -along.y;
        along.z = -along.z;
    }

    place = PlaceOf(agent);
    RotateAndTranslate(place);
    Vector4 from;
    VuTransformPoint(&place->matrix, RowOf(joint, 3), &from);
    Vector4 toward = {point->x - from.x, point->y - from.y, point->z - from.z, 1.0f};
    inverse = InverseLength(&toward, LengthEpsilon);
    toward.x = toward.x * inverse;
    toward.y = toward.y * inverse;
    toward.z = toward.z * inverse;
    Vector4 about = {along.y * toward.z - along.z * toward.y, along.z * toward.x - along.x * toward.z,
                     along.x * toward.y - along.y * toward.x, 1.0f};
    if (!(AimAxisLeast < about.x * about.x + about.y * about.y + about.z * about.z))
    {
        *rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        return;
    }

    inverse = InverseLength(&about, LengthEpsilon);
    f32 cosine = Dot(&along, &toward);
    about.x = about.x * inverse;
    about.y = about.y * inverse;
    about.z = about.z * inverse;
    s32 angle;
    AngleOfCosine(cosine, &angle);
    place = PlaceOf(agent);
    RotateAndTranslate(place);
    Matrix4x4 into = place->matrix;
    VuInvertRigidInPlace(&into);
    VuRotateVector(&into, &about, &about);
    RotationAboutAxis(rotation, &about, &angle, 0);
}

u32 LookController::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    u32 joint = animator->animation->joint->id;
    s32 turned = -1;
    if (CharacterOf(agent) == MechaBandicoot)
    {
        if (joint == MechaArm || joint == MechaArmEnd)
        {
            CreateJointTransform(animator, animator->animation->parent, 1, matrix);
            if (joint == MechaArm)
            {
                AimArm(this, matrix);
            }

            return 1;
        }

        for (u32 index = 0; index < MechaLookJointCount; index++)
        {
            if (joint == MechaLookJoints[index])
            {
                turned = static_cast<s32>(index);
            }
        }
    }
    else if (joint < LookJointCount)
    {
        turned = static_cast<s32>(joint);
    }

    // Retail turns any other joint by what its stack held (none has the look's callback)
    Vector4 rotation;
    if (turned >= 0)
    {
        s32 x = angles[turned][0];
        s32 y = angles[turned][1];
        s32 z = angles[turned][2];
        GetRotationXYZ(&rotation, &x, &y, &z);
    }

    AddJointRotation(animator, &rotation);
    return 1;
}
