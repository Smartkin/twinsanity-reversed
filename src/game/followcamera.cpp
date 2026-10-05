#include "game/followcamera.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/controllers.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"

EABI_EXPORT(FUN_00273738, &FollowCameraPositioner::Construct);
EABI_EXPORT(FUN_0027c560, &FollowCameraPositioner::SetDistanceAndPitch);
EABI_EXPORT(FUN_00274b20, &FollowCameraPositioner::Take);
EABI_EXPORT(FUN_00274e08, &FollowCameraPositioner::TakeCamera);
EABI_EXPORT(FUN_00277398, &FollowCameraTarget::EaseHeight);

namespace
{
// The rate the positioner's place follows at (the share of the way a second), the radius it keeps from the collision and from the
// instances' hulls, and the distance it goes back to after a trigger's camera
constexpr f32 DefaultRate = 6.0f;
constexpr f32 DefaultRadius = Rounded(0.4);
constexpr f32 DefaultDistance = 10.0f;
// A blend asked for without a trigger
constexpr f32 AskedBlendSeconds = 2.0f;
constexpr u32 EveryInstance = 0xFFFFFFFF;
constexpr s32 MostQueried = 0x10;
// What the box around the camera that the collision and the instances near it are gathered in is grown by
constexpr f32 GatherMargin = Rounded(0.1);
// The view checks look from a unit above the followed object
constexpr f32 EyeHeight = 1.0f;
// What a restart clears of the positioner's bits and state: their low halves
constexpr u64 PositionerLowBits = 0xFFFFFFFF;
constexpr u32 PositionerStateLowBits = 0xFFFF;

f32 StepSeconds(const TimeClock* clock)
{
    return static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
}

Vector4 Origin()
{
    Vector4 point = g_DefaultBox.min;
    point.w = 1.0f;
    return point;
}

// The half turn from an angle (its 16 bits less half a turn)
s32 Opposite(s32 angle)
{
    return static_cast<s32>((static_cast<u32>(angle) & (FullTurnAngle - 1)) - HalfTurnAngle);
}

// The query of the instances a view check makes (the instance itself and its attachment left out, and the one it ignores)
void MakeQuery(InstanceQuery* query, void** results, InstanceContext* instance)
{
    query->results = results;
    query->count = 0;
    query->most = MostQueried;
    query->distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = ReferencedObjectFlags::CollisionActive;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->instance = nullptr;
    query->skipped[1] = nullptr;
    SkipInQuery(query, instance);
}

// A blender's step ended: its hold starts again once it was pushed (or before it ever held), its input and rate scale cleared
template <typename Blender>
void EndStep(Blender* blender, u32 time)
{
    if (blender->bits.pushed != 0 || blender->holdStart == 0)
    {
        blender->holdStart = time;
    }

    blender->input = 0.0f;
    blender->rateScale = 0.0f;
}


// A point pulled back along the way from a point to where the view stopped
Vector4 PullBack(const Vector4* from, const Vector4* hit, f32 pull)
{
    Vector4 way;
    way.x = hit->x - from->x;
    way.y = hit->y - from->y;
    way.z = hit->z - from->z;
    way.w = 1.0f;
    Vector4 kept = way;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    f32 x = way.x * inverse * pull;
    f32 y = way.y * inverse * pull;
    f32 z = way.z * inverse * pull;
    Vector4 result;
    result.x = from->x + (kept.x - x);
    result.y = from->y + (kept.y - y);
    result.z = from->z + (kept.z - z);
    result.w = 1.0f;
    return result;
}

f32 DistanceSquared(const Vector4* point, const Vector4* from)
{
    f32 x = point->x - from->x;
    f32 y = point->y - from->y;
    f32 z = point->z - from->z;
    return x * x + y * y + z * z;
}

// The pitch and the yaw looking along a way
void AnglesAlong(Vector4* way, s32* pitch, s32* yaw)
{
    s32 turn;
    YawOfDirection(&turn, way);
    s32 back = -turn;
    Matrix4x4 aboutY;
    MatrixAboutY(&aboutY, &back);
    VuRotateVector(&aboutY, way, way);
    PitchOfDirection(pitch, way);
    *yaw = turn;
}

u32 SamePoint(const Vector4* point, const Vector4* other)
{
    return point->x == other->x && point->y == other->y && point->z == other->z;
}

// Whether a camera target's box point stays as it is (its trigger's targetBoxUnturned; the last one's while its box doesn't
// settle)
u32 BoxUnturned(FollowTargetBits bits)
{
    return bits.boxUnturned != 0 || (bits.settled == 0 && bits.lastBoxUnturned != 0);
}

// The ends blended by a share the way the cameras' angles are (through radians)
s32 BlendAngles(s32 start, s32 end, f32 share)
{
    return static_cast<s32>((static_cast<f32>(end) * AngleToRadians * share +
                             static_cast<f32>(start) * AngleToRadians * (1.0f - share)) *
                            RadiansToAngle);
}
}

FollowCameraPositioner* FollowCameraPositioner::Construct(FollowCameraPositioner* positioner, f32 distance, const s32* pitch)
{
    constexpr f32 DistanceSpeed = 5.0f;
    // Degrees a second
    constexpr f32 ProbePushRate = 180.0f;
    constexpr u8 Unused384 = 60;
    CameraPositioner::ConstructBase(positioner);
    positioner->instance = nullptr;
    positioner->vtable = g_FollowCameraPositionerVTable;
    positioner->target = nullptr;
    s32 angle = *pitch;
    AngleBlender::Construct(&positioner->pitch, &angle);
    angle = g_DefaultFov;
    AngleBlender::Construct(&positioner->fieldOfView, &angle);
    AngleFrom(&angle, 0.0f, AngleRadians);
    AngleBlender::Construct(&positioner->yaw, &angle);
    DistanceBlender* blender = &positioner->distance;
    blender->initial = distance;
    blender->inputSpeed = DistanceSpeed;
    blender->speed = DistanceSpeed;
    blender->low = distance;
    blender->high = distance;
    blender->secondLow = distance;
    blender->secondHigh = distance;
    blender->unused48 = 0.0f;
    blender->Reset();
    positioner->unused1C8 = 1.0f;
    AngleFrom(&positioner->pitchPushRate, ProbePushRate, AngleDegrees);
    AngleFrom(&positioner->yawPushRate, ProbePushRate, AngleDegrees);
    MainCamera::Construct(&positioner->camera);
    positioner->stepSeconds = 0.0f;
    positioner->turnShare = 1.0f;
    positioner->unused398 = 1.0f;
    positioner->cache = nullptr;
    positioner->ownRate = DefaultRate;
    positioner->unused384 = Unused384;
    AngleFrom(&positioner->roll, positioner->stepSeconds, AngleRadians);
    positioner->bits.steers = 1;
    positioner->bits.distanceFollowsPitch = 0;
    positioner->bits.pushedOffCollision = 0;
    positioner->keyed = 0;
    positioner->fieldOfView.bits.holds = 1;
    positioner->lowTargetOffset = Origin();
    positioner->highTargetOffset = Origin();
    positioner->lowCameraOffset = Origin();
    positioner->highCameraOffset = Origin();
    // Each pair of sides at 1 and -1
    for (u32 index = 0; index < ProbeCount; index += 2)
    {
        positioner->lowTargetSides[index] = 1.0f;
        positioner->lowTargetSides[index + 1] = -1.0f;
        positioner->lowCameraSides[index] = 1.0f;
        positioner->lowCameraSides[index + 1] = -1.0f;
        positioner->highTargetSides[index] = 1.0f;
        positioner->highTargetSides[index + 1] = -1.0f;
        positioner->highCameraSides[index] = 1.0f;
        positioner->highCameraSides[index + 1] = -1.0f;
    }

    positioner->triggerPlace = Origin();
    s32 start = *pitch;
    positioner->SetDistanceAndPitch(distance, &start);
    return positioner;
}

void FollowCameraPositioner::Destroy(u32 destroyFlags)
{
    vtable = g_FollowCameraPositionerVTable;
    if (cache != nullptr)
    {
        DestroyCollisionCache(cache, DestroyAndFree);
    }

    camera.Destroy(DestroyOnly);
    vtable = g_CameraPositionerVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void FollowCameraPositioner::Reset(InstanceContext* followed, CameraTarget* follower)
{
    target = follower;
    instance = followed;
    if (cache != nullptr)
    {
        DestroyCollisionCache(cache, DestroyAndFree);
    }

    cache = nullptr;
    auto* memory = static_cast<CollisionCache*>(MemoryAllocate(sizeof(CollisionCache)));
    cache = ConstructCollisionCache(memory, followed, SurfaceFlags::BlocksCamera);
    Restart();
}

void FollowCameraPositioner::Restart()
{
    FollowPositionerBits kept = bits;
    EndBlend();
    pitch.initial = g_FollowCameraPitch;
    pitch.Reset();
    fieldOfView.Reset();
    s32 behind;
    TargetYaw(&behind, this);
    yaw.initial = behind;
    yaw.Reset();
    triggerPlace = Origin();
    if (kept.distanceFollowsPitch != 0)
    {
        distance.initial = distance.high * pitch.share + distance.low * (1.0f - pitch.share);
    }

    distance.Reset();
    bits.value &= ~PositionerLowBits;
    state.value &= ~PositionerStateLowBits;
    unused1D4 = 0;
    bits.pushedOutOfHull = 0;
    bits.pushedBack = 0;
    bits.steers = kept.steers;
    bits.distanceFollowsPitch = kept.distanceFollowsPitch;
    bits.tilts = kept.tilts;
    Vector4 point = target != nullptr ? target->point : Origin();
    Vector4 placed;
    PlaceFor(&point, &placed, 0);
    position = target != nullptr ? placed : Origin();
    fov = fieldOfView.current;
    bits.stickTurning = 0;
    bits.unused4 = 0;
    bits.characterMoving = 0;
    bits.ignoresValues = 0;
    state.blendAsked = 0;
    trigger = nullptr;
    ClearTriggerValues();
    AngleFrom(&tiltPitch, 0.0f, AngleRadians);
    AngleFrom(&tiltYaw, 0.0f, AngleRadians);
    blockedSince = 0;
    backOff = 0.0f;
    collisionRadius = DefaultRadius;
    bits.restarted = 1;
    bits.blockedAWhile = 0;
    bits.blockedLong = 0;
    bits.blocked = 0;
    bits.unpushed = 1;
    hullRadius = DefaultRadius;
    ClearProbes();
    camera.flags.value = 0;
    bits.ownCamera = 0;
    camera.flags.steers = 1;
    ClearTriggerValues();
    ignored = nullptr;
    if (target == nullptr)
    {
        return;
    }

    Vector4 eye = target->objectPosition;
    eye.y = eye.y + EyeHeight;
    Vector4 clear;
    if (ViewBlocked(&position, &eye) != 0 && FindClearPlace(&position, &eye, &clear, 0) != 0)
    {
        position = clear;
        PlaceBehind();
    }
}

void FollowCameraPositioner::SetDistanceAndPitch(f32 length, const s32* angle)
{
    distance.high = length;
    distance.low = length;
    distance.initial = (length + length) * 0.5f;
    pitch.initial = *angle;
    Restart();
}

void FollowCameraPositioner::ClearTriggerValues()
{
    EndBlend();
    bits.triggerSteers = 1;
    bits.atTriggerPlace = 0;
    bits.probesFromTarget = 0;
    bits.alwaysTakesValues = 0;
    bits.unused9 = 0;
    bits.yawSpeedSet = 0;
    bits.holdsStill = 0;
    bits.keepsHeight = 0;
    bits.onlyLooksAtTarget = 0;
    bits.noFacingTilt = 0;
    bits.atTriggerRate = 0;
    bits.skipsViewCheck = 0;
    bits.addsExtraYaw = 0;
    bits.probesOff = 0;
    state.blending = 0;
    state.blendStarted = 0;
    state.cut = 0;
    state.placed = 1;
    state.blendAsked = 0;
    state.timed = 0;
    state.timeEnded = 0;
    state.alongLine = 0;
    pitch.bits.secondRange = 0;
    fieldOfView.bits.secondRange = 0;
    distance.bits.secondRange = 0;
    yaw.bits.secondRange = 0;
    yaw.bits.sineSpeed = 0;
    triggerRate = DefaultRate;
    unused398 = 1.0f;
    keepsRate = 0;
    smoothed = 0;
}

void FollowCameraPositioner::EndBlend()
{
    state.timed = 0;
    state.timeEnded = 1;
    fieldOfView.speed = fieldOfView.previousSpeed;
    distance.speed = distance.previousSpeed;
    yaw.speed = yaw.previousSpeed;
    pitch.speed = pitch.previousSpeed;
}

void FollowCameraPositioner::ClearProbes()
{
    bits.leftHit = 0;
    bits.rightHit = 0;
    bits.aboveHit = 0;
    bits.belowHit = 0;
    bits.nextProbe = ProbeAbove;
}

void FollowCameraPositioner::ClearBlocked()
{
    bits.blockedAWhile = 0;
    bits.blocked = 0;
    bits.blockedLong = 0;
}

void FollowCameraPositioner::EaseBackOff()
{
    constexpr f32 Ease = Rounded(0.05);
    backOff = backOff + (__builtin_fabsf(facing + facing) - backOff) * Ease;
}

u32 FollowCameraPositioner::KeepsRigRotation()
{
    return bits.holdsStill;
}

void FollowCameraPositioner::SetTilts(u32 tilts)
{
    bits.tilts = tilts;
}

void FollowCameraPositioner::ResetTurnShare()
{
    turnShare = 1.0f;
}

void FollowCameraPositioner::ResetOwnRate()
{
    ownRate = DefaultRate;
}

u32 FollowCameraPositioner::SegmentBlocked(const Vector4* from, const Vector4* to)
{
    return GetCollisionCheck(instance->chunk, from, to, SurfaceFlags::BlocksCamera, nullptr, nullptr, nullptr);
}

void FollowCameraPositioner::TimeBlocked(const Vector4* point, const Vector4* goal, const Vector4*, TimeClock* clock)
{
    constexpr f32 LongSeconds = Rounded(0.7);
    constexpr f32 AWhileSeconds = Rounded(0.2);
    if (ViewBlocked(point, goal) == 0)
    {
        ClearBlocked();
        return;
    }

    if (bits.blocked == 0)
    {
        bits.blocked = 1;
        blockedSince = clock->time;
    }

    s32 since = static_cast<s32>(clock->time - blockedSince);
    if (static_cast<s32>(g_ClockUnitsPerSecond * LongSeconds) < since)
    {
        bits.blockedLong = 1;
    }
    else if (static_cast<s32>(g_ClockUnitsPerSecond * AWhileSeconds) < since)
    {
        bits.blockedAWhile = 1;
    }
}

s32* FollowCameraPositioner::TargetYaw(s32* angle, FollowCameraPositioner* positioner)
{
    Matrix4x4 matrix;
    u32 hasDirection = 1;
    CameraTarget* follower = positioner->target;
    if (follower == nullptr)
    {
        InitIdentityMatrix(&matrix);
    }
    else if (follower->facesMovement != 0)
    {
        Vector4 ahead;
        ahead.x = 0.0f;
        ahead.y = 0.0f;
        ahead.w = 1.0f;
        ahead.z = 1.0f;
        Vector4 way = follower->velocity;
        way.y = 0.0f;
        if (LengthSquared(&way) <= LengthEpsilon)
        {
            hasDirection = 0;
        }

        MatrixBetween(&matrix, &ahead, &way);
    }
    else
    {
        MatrixFromRotation(&matrix, &follower->rotation);
    }

    s32 result;
    if (hasDirection != 0)
    {
        YawOfDirection(&result, RowOf(&matrix, 2));
        positioner->lastYaw = result;
    }
    else
    {
        result = positioner->lastYaw;
    }

    *angle = result;
    return angle;
}

void FollowCameraPositioner::PlaceFor(const Vector4* point, Vector4* out, u32 commit)
{
    s32 aboutX = pitch.current + pitch.delta;
    s32 view = fieldOfView.current + fieldOfView.delta;
    s32 aboutY = yaw.current + yaw.delta;
    if (bits.addsExtraYaw != 0)
    {
        aboutY = aboutY + yawExtra;
    }

    f32 length = distance.current + (distance.delta + backOff);
    aboutX = WrapAngle(aboutX);
    aboutY = WrapAngle(aboutY);
    view = WrapAngle(view);
    s32 aboutZ;
    AngleFrom(&aboutZ, 0.0f, AngleRadians);
    Matrix4x4 matrix;
    MatrixFromAngles(&matrix, &aboutX, &aboutY, &aboutZ);
    *RowOf(&matrix, 3) = *point;
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = -length;
    out->w = 1.0f;
    VuTransformPoint(&matrix, out, out);
    if (bits.keepsHeight != 0)
    {
        out->y = position.y;
    }

    if (commit != 0)
    {
        pitch.current = aboutX;
        fieldOfView.current = view;
        yaw.current = aboutY;
        distance.current = length - backOff;
    }
}

void FollowCameraPositioner::MoveToward(const Vector4* point, const Vector4* goal, u32 jumped)
{
    // A shake that turns the view less than this leaves it as it is
    constexpr f32 Steady = Rounded(0.001);
    f32 share = bits.atTriggerRate != 0 ? triggerRate : ownRate;
    share = share * stepSeconds;
    if (1.0f < share || state.cut != 0)
    {
        share = 1.0f;
    }

    if (bits.onlyLooksAtTarget != 0 || bits.keepsHeight != 0)
    {
        Vector4 up;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        up.w = 1.0f;
        Matrix4x4 matrix;
        LookAtMatrix(&matrix, &position, point, &up);
        GetRotationVec(&rotation, &matrix);
        if (bits.onlyLooksAtTarget != 0)
        {
            return;
        }

        Vector4 move;
        move.x = goal->x - position.x;
        move.y = goal->y - position.y;
        move.z = goal->z - position.z;
        move.w = 1.0f;
        move.x = move.x * share;
        move.z = move.z * share;
        move.y = move.y * share;
        position.x = position.x + move.x;
        position.y = position.y + move.y;
        position.z = position.z + move.z;
        return;
    }

    s32 pitchStart = pitch.start;
    s32 viewStart = fieldOfView.start;
    s32 yawStart = yaw.start;
    s32 pitchMove = static_cast<s32>(static_cast<f32>(WrapAngle(pitch.current - pitchStart)) * share);
    s32 yawMove = static_cast<s32>(static_cast<f32>(WrapAngle(yaw.current - yawStart)) * share);
    s32 viewMove = static_cast<s32>(static_cast<f32>(WrapAngle(fieldOfView.current - viewStart)) * share);
    s32 newPitch = WrapAngle(pitchStart + pitchMove);
    s32 newYaw = WrapAngle(yawStart + yawMove);
    s32 newView = WrapAngle(viewStart + viewMove);
    f32 length = distance.start + (distance.current - distance.start) * share;
    if (keepsRate == 0 || jumped != 0)
    {
        Vector4 move;
        move.x = goal->x - position.x;
        move.y = goal->y - position.y;
        move.z = goal->z - position.z;
        move.w = 1.0f;
        move.x = move.x * share;
        move.y = move.y * share;
        move.z = move.z * share;
        position.x = position.x + move.x;
        position.y = position.y + move.y;
        position.z = position.z + move.z;
        Vector4 way = *point;
        way.x = way.x - position.x;
        way.y = way.y - position.y;
        way.z = way.z - position.z;
        length = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
        AnglesAlong(&way, &newPitch, &newYaw);
    }
    else
    {
        f32 height = position.y;
        s32 aboutZ;
        AngleFrom(&aboutZ, 0.0f, AngleRadians);
        s32 aboutX = newPitch;
        s32 aboutY = newYaw;
        Matrix4x4 matrix;
        MatrixFromAngles(&matrix, &aboutX, &aboutY, &aboutZ);
        *RowOf(&matrix, 3) = *point;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = -length;
        position.w = 1.0f;
        VuTransformPoint(&matrix, &position, &position);
        if (bits.keepsHeight != 0)
        {
            position.y = height;
        }
    }

    StepCameraShake(&g_CameraShake);
    Vector4 ahead;
    ahead.x = 0.0f;
    ahead.y = 0.0f;
    ahead.z = length;
    ahead.w = 1.0f;
    Vector4 turn;
    turn.z = 0.0f;
    turn.y = 0.0f;
    turn.x = 0.0f;
    turn.w = 1.0f;
    Vector4 shaken = g_CameraShake.offset;
    shaken.x = shaken.x + ahead.x;
    shaken.y = shaken.y + ahead.y;
    shaken.z = shaken.z + ahead.z;
    if (AreParallel(Steady, &shaken, &ahead, nullptr) == 0)
    {
        RotationBetween(&turn, &ahead, &shaken);
    }

    s32 aboutZ = roll;
    s32 aboutX = newPitch;
    s32 aboutY = newYaw;
    Vector4 aimed;
    GetRotationXYZ(&aimed, &aboutX, &aboutY, &aboutZ);
    MultiplyRotations(&aimed, &aimed, &turn);
    SlerpRotations(turnShare, &rotation, &rotation, &aimed);
    TiltVirtual(&position, &rotation);
    yaw.start = newYaw;
    fieldOfView.start = newView;
    pitch.start = newPitch;
    distance.start = length;
}

u32 FollowCameraPositioner::Probe(const Vector4* point, const Vector4* goal, u32 keep)
{
    // The side probes' ends around the camera are behind it; it pulls in only further than PullFurther; it casts at most
    // MostCasts probes
    constexpr f32 CameraDepth = Rounded(-0.42);
    constexpr f32 PullFurther = 2.0f;
    constexpr s32 MostCasts = 4;
    s32 cast = 0;
    ProbeHits hits;
    hits.value = 0;
    u32 belowFloor = 0;
    Vector4 ahead = *point;
    ahead.x = ahead.x - goal->x;
    ahead.y = ahead.y - goal->y;
    ahead.z = ahead.z - goal->z;
    f32 share = pitch.share;
    Vector4 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    up.w = 1.0f;
    f32 inverse = InverseLength(&ahead, LengthEpsilon);
    ahead.x = ahead.x * inverse;
    ahead.y = ahead.y * inverse;
    ahead.z = ahead.z * inverse;
    Vector4 side;
    side.y = ahead.z * up.x - ahead.x * up.z;
    side.z = ahead.x * up.y - ahead.y * up.x;
    side.x = ahead.y * up.z - ahead.z * up.y;
    side.w = 1.0f;
    Vector4 upward;
    upward.y = side.z * ahead.x - side.x * ahead.z;
    upward.z = side.x * ahead.y - side.y * ahead.x;
    upward.x = side.y * ahead.z - side.z * ahead.y;
    upward.w = up.w;
    Matrix4x4 targetFrame;
    *RowOf(&targetFrame, 0) = side;
    *RowOf(&targetFrame, 1) = upward;
    *RowOf(&targetFrame, 2) = ahead;
    Vector4* targetOrigin = RowOf(&targetFrame, 3);
    if (bits.probesFromTarget != 0)
    {
        *targetOrigin = *point;
    }
    else
    {
        targetOrigin->x = lowTargetOffset.x + (highTargetOffset.x - lowTargetOffset.x) * share;
        targetOrigin->y = lowTargetOffset.y + (highTargetOffset.y - lowTargetOffset.y) * share;
        targetOrigin->w = 1.0f;
        targetOrigin->z = lowTargetOffset.z + (highTargetOffset.z - lowTargetOffset.z) * share;
        VuRotateVector(&targetFrame, targetOrigin, targetOrigin);
        Vector4 object = target->objectPosition;
        object.y = object.y + EyeHeight;
        targetOrigin->x = targetOrigin->x + object.x;
        targetOrigin->y = targetOrigin->y + object.y;
        targetOrigin->z = targetOrigin->z + object.z;
    }

    Matrix4x4 cameraFrame;
    *RowOf(&cameraFrame, 0) = side;
    *RowOf(&cameraFrame, 1) = upward;
    *RowOf(&cameraFrame, 2) = ahead;
    Vector4* cameraOrigin = RowOf(&cameraFrame, 3);
    cameraOrigin->x = lowCameraOffset.x + (highCameraOffset.x - lowCameraOffset.x) * share;
    cameraOrigin->y = lowCameraOffset.y + (highCameraOffset.y - lowCameraOffset.y) * share;
    cameraOrigin->w = 1.0f;
    cameraOrigin->z = lowCameraOffset.z + (highCameraOffset.z - lowCameraOffset.z) * share;
    VuRotateVector(&cameraFrame, cameraOrigin, cameraOrigin);
    cameraOrigin->x = cameraOrigin->x + goal->x;
    cameraOrigin->y = cameraOrigin->y + goal->y;
    cameraOrigin->z = cameraOrigin->z + goal->z;
    f32 rest = 1.0f - share;
    f32 targetSides[ProbeCount];
    f32 cameraSides[ProbeCount];
    for (u32 index = 0; index < ProbeCount; index++)
    {
        targetSides[index] = highTargetSides[index] * share + lowTargetSides[index] * rest;
        cameraSides[index] = highCameraSides[index] * share + lowCameraSides[index] * rest;
    }

    if (pitch.bits.enabled != 0)
    {
        for (u32 index = ProbeAbove; index <= ProbeBelow; index++)
        {
            probeTargetEnds[index].x = 0.0f;
            probeTargetEnds[index].y = targetSides[index];
            probeTargetEnds[index].z = 0.0f;
            probeTargetEnds[index].w = 1.0f;
            probeCameraEnds[index].x = 0.0f;
            probeCameraEnds[index].y = cameraSides[index];
            probeCameraEnds[index].z = 0.0f;
            probeCameraEnds[index].w = 1.0f;
        }

        VuTransformPoint(&targetFrame, &probeTargetEnds[ProbeAbove], &probeTargetEnds[ProbeAbove]);
        VuTransformPoint(&targetFrame, &probeTargetEnds[ProbeBelow], &probeTargetEnds[ProbeBelow]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[ProbeAbove], &probeCameraEnds[ProbeAbove]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[ProbeBelow], &probeCameraEnds[ProbeBelow]);
        if (cast < MostCasts && bits.nextProbe == ProbeAbove)
        {
            if (SegmentBlocked(&probeCameraEnds[ProbeAbove], &probeTargetEnds[ProbeAbove]) != 0)
            {
                hits.above = 1;
            }

            bits.nextProbe = bits.nextProbe + 1;
            cast = 1;
            if (keep != 0)
            {
                bits.aboveHit = hits.above;
            }
        }
        else if (bits.aboveHit != 0)
        {
            hits.above = 1;
        }

        if (probeFloor < probeTargetEnds[ProbeBelow].y)
        {
            if (cast < MostCasts && bits.nextProbe == ProbeBelow)
            {
                if (SegmentBlocked(&probeCameraEnds[ProbeBelow], &probeTargetEnds[ProbeBelow]) != 0)
                {
                    hits.below = 1;
                }

                bits.nextProbe = bits.nextProbe + 1;
                cast = static_cast<s8>(cast + 1);
                if (keep != 0)
                {
                    bits.belowHit = hits.below;
                }
            }
            else if (bits.belowHit != 0)
            {
                hits.below = 1;
            }
        }
        else
        {
            belowFloor = 1;
        }
    }

    if (yaw.bits.enabled != 0)
    {
        for (u32 index = ProbeLeft; index <= ProbeRight; index++)
        {
            probeTargetEnds[index].x = targetSides[index];
            probeTargetEnds[index].y = 0.0f;
            probeTargetEnds[index].z = 0.0f;
            probeTargetEnds[index].w = 1.0f;
            probeCameraEnds[index].x = cameraSides[index];
            probeCameraEnds[index].y = 0.0f;
            probeCameraEnds[index].z = CameraDepth;
            probeCameraEnds[index].w = 1.0f;
        }

        VuTransformPoint(&targetFrame, &probeTargetEnds[ProbeLeft], &probeTargetEnds[ProbeLeft]);
        VuTransformPoint(&targetFrame, &probeTargetEnds[ProbeRight], &probeTargetEnds[ProbeRight]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[ProbeLeft], &probeCameraEnds[ProbeLeft]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[ProbeRight], &probeCameraEnds[ProbeRight]);
        if (cast < MostCasts && bits.nextProbe == ProbeLeft)
        {
            if (SegmentBlocked(&probeCameraEnds[ProbeLeft], &probeTargetEnds[ProbeLeft]) != 0)
            {
                hits.left = 1;
            }

            bits.nextProbe = bits.nextProbe + 1;
            cast = static_cast<s8>(cast + 1);
            if (keep != 0)
            {
                bits.leftHit = hits.left;
            }
        }
        else if (bits.leftHit != 0)
        {
            hits.left = 1;
        }

        if (cast < MostCasts && bits.nextProbe == ProbeRight)
        {
            if (SegmentBlocked(&probeCameraEnds[ProbeRight], &probeTargetEnds[ProbeRight]) != 0)
            {
                hits.right = 1;
            }

            bits.nextProbe = ProbeAbove;
            if (keep != 0)
            {
                bits.rightHit = hits.right;
            }
        }
        else if (bits.rightHit != 0)
        {
            hits.right = 1;
        }
    }

    if (distance.bits.enabled != 0)
    {
        u32 bothUpright = hits.above != 0 && hits.below != 0;
        u32 noneUpright = hits.above == 0 && hits.below == 0;
        u32 bothSides = hits.left != 0 && hits.right != 0;
        if (bothSides != 0 && noneUpright != 0)
        {
            if (belowFloor == 0)
            {
                hits.below = 1;
                hits.pair = 1;
            }
        }
        else if (bothUpright != 0 || bothSides != 0)
        {
            hits.pair = 1;
        }

        if (bits.stickTurning != 0 && hits.value != 0 && bothUpright == 0 && bothSides == 0 && PullFurther < distance.current)
        {
            hits.pullIn = 1;
        }
    }

    unused1D4 = static_cast<u8>(hits.value | unused1D4);
    return hits.value;
}

u32 FollowCameraPositioner::ProbeRates(const Vector4* point, s32* pitchRate, s32* yawRate, f32* distanceRate, u32 keep)
{
    constexpr f32 PullRate = -20.0f;
    if (bits.probesOff != 0)
    {
        *pitchRate = 0;
        *yawRate = 0;
        *distanceRate = 0.0f;
        return 0;
    }

    Vector4 goal;
    PlaceFor(point, &goal, 0);
    ProbeHits hits;
    hits.value = Probe(point, &goal, keep);
    // Turned away from the probe that hit (not when both of the pair did)
    if (hits.left != 0)
    {
        *yawRate = hits.right != 0 ? 0 : -yawPushRate;
    }
    else
    {
        *yawRate = hits.right != 0 ? yawPushRate : 0;
    }

    if (hits.above != 0)
    {
        *pitchRate = hits.below != 0 ? 0 : -pitchPushRate;
    }
    else
    {
        *pitchRate = hits.below != 0 ? pitchPushRate : 0;
    }

    *distanceRate = 0.0f;
    if (hits.pullIn != 0)
    {
        *distanceRate = PullRate;
    }

    return hits.value;
}

u32 FollowCameraPositioner::PushByProbes(TimeClock* clock, const Vector4* point)
{
    s32 pitchRate;
    s32 yawRate;
    f32 distanceRate;
    ProbeHits hits;
    hits.value = ProbeRates(point, &pitchRate, &yawRate, &distanceRate, 1);
    f32 seconds = StepSeconds(clock);
    if (hits.left != 0 || hits.right != 0)
    {
        if (yaw.delta != 0)
        {
            yawRate = 0;
        }

        s32 rate = yawRate;
        yaw.Push(seconds, &rate, 0);
    }

    if (hits.above != 0 || hits.below != 0)
    {
        if (pitch.delta != 0)
        {
            pitchRate = 0;
        }

        s32 rate = pitchRate;
        pitch.Push(seconds, &rate, 0);
    }

    if (hits.pullIn != 0)
    {
        if (distance.bits.enabled != 0)
        {
            distance.delta = distanceRate * seconds;
            distance.KeepWithin(distance.low, distance.high);
            distance.bits.pushed = 1;
        }
    }
    // (Retail also tests bit 4, which Probe never sets)
    else if (hits.pair != 0)
    {
        if (!(__builtin_fabsf(distance.delta) <= Epsilon))
        {
            distanceRate = 0.0f;
        }

        if (distance.bits.enabled != 0)
        {
            distance.delta = distanceRate * seconds;
            distance.bits.pushed = 1;
        }
    }

    return hits.value;
}

u32 FollowCameraPositioner::StepBlenders(TimeClock* clock)
{
    u32 moving = 0;
    u32 distanceMoving;
    distance.bits.blendedGoal = 0;
    if (bits.atTriggerPlace == 0)
    {
        if (state.timed != 0 && !(static_cast<s32>(clock->time - blendStart) < blendTicks))
        {
            EndBlend();
        }

        EaseBackOff();
        s32 pitchGoal;
        AngleFrom(&pitchGoal, 0.0f, AngleDegrees);
        if (bits.tilts != 0)
        {
            s32 standard = g_FollowCameraPitch;
            pitchGoal = standard;
            if (pitch.high < standard)
            {
                pitchGoal = pitch.high;
            }
            else if (standard < pitch.low)
            {
                pitchGoal = pitch.low;
            }
        }

        s32 yawGoal;
        TargetYaw(&yawGoal, this);
        s32 goal = pitchGoal;
        u32 pitchMoving = pitch.Step(clock, &goal, 1, state.cut);
        goal = yawGoal;
        u32 yawMoving = yaw.Step(clock, &goal, 0, state.cut);
        moving = pitchMoving != 0 || yawMoving != 0;
        if (bits.distanceFollowsPitch != 0)
        {
            distance.bits.blendedGoal = 1;
            distance.goalShare = pitch.share;
        }

        distanceMoving = distance.Step(DefaultDistance, clock, 1, state.cut) != 0;
    }
    else
    {
        Vector4 way = target->point;
        f32 share = StepSeconds(clock);
        if (state.blendStarted == 0)
        {
            share = 1.0f;
        }
        else
        {
            s32 since = static_cast<s32>(clock->time - blendStart);
            if (!(since < blendTicks))
            {
                share = 1.0f;
                EndBlend();
            }
            else
            {
                u32 curve = state.curve;
                if (curve == FollowCurveEven)
                {
                    if (state.alongLine != 0)
                    {
                        if (state.cut != 0)
                        {
                            share = 1.0f;
                            blendFrom = triggerPlace;
                        }
                        else
                        {
                            f32 total = static_cast<f32>(blendTicks) * g_SecondsPerClockUnit;
                            f32 elapsed = ClockUnitsToSeconds(&since);
                            share = static_cast<f32>(static_cast<s32>(elapsed / total * g_ClockUnitsPerSecond)) *
                                    g_SecondsPerClockUnit;
                        }
                    }
                    else
                    {
                        share = share / (static_cast<f32>(blendTicks - since) * g_SecondsPerClockUnit);
                    }
                }
                else if (curve == FollowCurveCubic)
                {
                    f32 elapsed = static_cast<f32>(since) * g_SecondsPerClockUnit /
                                  (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit);
                    share = elapsed * (elapsed * elapsed);
                }
            }
        }

        if (state.alongLine != 0)
        {
            Vector4 line;
            line.x = triggerPlace.x - blendFrom.x;
            line.y = triggerPlace.y - blendFrom.y;
            line.z = triggerPlace.z - blendFrom.z;
            line.w = 1.0f;
            Vector4 along = line;
            along.x = along.x * share + blendFrom.x;
            along.y = along.y * share + blendFrom.y;
            along.z = along.z * share + blendFrom.z;
            way.x = way.x - along.x;
            way.y = way.y - along.y;
            way.z = way.z - along.z;
        }
        else
        {
            way.x = way.x - triggerPlace.x;
            way.y = way.y - triggerPlace.y;
            way.z = way.z - triggerPlace.z;
        }

        f32 length = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
        s32 tilt;
        s32 turn;
        AnglesAlong(&way, &tilt, &turn);
        if (state.cut != 0)
        {
            pitch.current = tilt;
            yaw.current = turn;
            distance.current = length;
            s32 goal = fieldOfView.low;
            return 1 | (fieldOfView.Step(clock, &goal, 1, state.cut) != 0);
        }

        if (state.alongLine != 0)
        {
            pitch.current = tilt;
            yaw.current = turn;
            distance.current = length;
            AngleFrom(&pitch.delta, 0.0f, AngleRadians);
            AngleFrom(&yaw.delta, 0.0f, AngleRadians);
            distance.delta = 0.0f;
            s32 goal = fieldOfView.low;
            return fieldOfView.Step(clock, &goal, 1, state.cut) != 0;
        }

        if (pitch.bits.enabled != 0)
        {
            pitch.delta = static_cast<s32>(static_cast<f32>(WrapAngle(tilt - pitch.current)) * share);
            s32 low = tilt;
            s32 high = tilt;
            pitch.KeepWithin(&low, &high);
            pitch.bits.pushed = 1;
        }

        moving = pitch.bits.pushed;
        if (yaw.bits.enabled != 0)
        {
            yaw.delta = static_cast<s32>(static_cast<f32>(WrapAngle(turn - yaw.current)) * share);
            s32 low = turn;
            s32 high = turn;
            yaw.KeepWithin(&low, &high);
            yaw.bits.pushed = 1;
        }

        moving |= yaw.bits.pushed;
        if (distance.bits.enabled != 0)
        {
            distance.delta = (length - distance.current) * share;
            distance.KeepWithin(length, length);
            distance.bits.pushed = 1;
        }

        distanceMoving = distance.bits.pushed;
    }

    moving |= distanceMoving;
    s32 goal = fieldOfView.low;
    return moving | (fieldOfView.Step(clock, &goal, 1, state.cut) != 0);
}

void FollowCameraPositioner::MeasureFacing()
{
    constexpr f32 Margin = Rounded(0.1);
    // Takes what's past the margin to the whole range (1 / 0.9)
    constexpr f32 Spread = 0x1.1c71c8p+0f;
    f32 cosine = 0.0f;
    f32 sine = 0.0f;
    if (bits.restarted == 0 && bits.noFacingTilt == 0 && bits.stickTurned == 0)
    {
        Vector4 cameraAhead;
        cameraAhead.x = 0.0f;
        cameraAhead.y = 0.0f;
        cameraAhead.w = 1.0f;
        cameraAhead.z = 1.0f;
        Vector4 targetAhead = cameraAhead;
        Matrix4x4 targetMatrix;
        MatrixFromRotation(&targetMatrix, &target->rotation);
        Matrix4x4 cameraMatrix;
        MatrixFromRotation(&cameraMatrix, &rotation);
        VuRotateVector(&targetMatrix, &targetAhead, &targetAhead);
        VuRotateVector(&cameraMatrix, &cameraAhead, &cameraAhead);
        cameraAhead.y = 0.0f;
        targetAhead.y = 0.0f;
        s32 angle;
        SignedAngleAboutY(&angle, &cameraAhead, &targetAhead);
        s32 turn = angle;
        cosine = CosOfAngle(&turn);
        sine = SinOfAngle(&turn);
    }

    facing = cosine;
    if (cosine < -Margin)
    {
        facing = cosine + Margin;
    }
    else
    {
        facing = 0.0f;
    }

    sideways = sine;
    facing = facing * Spread;
    if (Margin < sine)
    {
        sideways = sine - Margin;
    }
    else if (sine < -Margin)
    {
        sideways = sine + Margin;
    }
    else
    {
        sideways = 0.0f;
    }

    sideways = sideways * Spread;
}

u32 FollowCameraPositioner::PushOutOfHulls(const Vector4* goal, const Vector4* from, Vector4* out)
{
    // The radius grows a unit a second while the hulls push it, up to MostRadius
    constexpr f32 MostRadius = 0.5f;
    Vector4 reached = *goal;
    u32 pushed = 0;
    if (bits.pushedOutOfHull != 0)
    {
        hullRadius = hullRadius + stepSeconds;
        if (MostRadius < hullRadius)
        {
            hullRadius = MostRadius;
        }
    }
    else
    {
        hullRadius = DefaultRadius;
    }

    void* results[MostQueried];
    InstanceQuery query;
    MakeQuery(&query, results, instance);
    f32 share = 0.0f;
    Vector4 hit;
    // Up to where the segment from where it was hits an instance
    if (SegmentHitsInstances(instance->chunk, from, goal, &query, EveryInstance, &share, &hit, 0) != 0)
    {
        reached = hit;
    }

    *out = reached;
    f32 radius = hullRadius;
    Box box;
    box.min.x = reached.x - radius;
    box.min.y = reached.y - radius;
    box.min.z = reached.z - radius;
    box.min.w = 1.0f;
    box.max.x = reached.x + radius;
    box.max.y = reached.y + radius;
    box.max.z = reached.z + radius;
    box.max.w = 1.0f;
    GrowBox(GatherMargin, &box);
    QueryChunkInstances(instance->chunk, &box, EveryInstance, &query);
    for (s32 index = 0; index < query.count; index++)
    {
        auto* found = static_cast<InstanceContext*>(query.results[index]);
        ObjectCollision* collision = &found->collision;
        for (u8 hull = 0; static_cast<s32>(hull) < GetHullCount(collision); hull++)
        {
            CollisionHull* model;
            Matrix4x4 matrix;
            GetInstanceHull(collision, hull, &model, &matrix);
            const CollisionSurface* surface = &g_CollisionSurfaces.surfaces[static_cast<u16>(model->surface)];
            if (surface->flags.blocksCamera == 0)
            {
                continue;
            }

            Vector4 centre = *out;
            Matrix4x4 inverse = matrix;
            VuInvertRigidInPlace(&inverse);
            VuTransformPoint(&inverse, &centre, &centre);
            Vector4 push;
            if (SphereInHull(hullRadius, model, &centre, &push) != 0)
            {
                pushed = 1;
                VuRotateVector(&matrix, &push, &push);
                out->x = out->x + push.x;
                out->y = out->y + push.y;
                out->z = out->z + push.z;
            }
        }
    }

    bits.pushedOutOfHull = pushed;
    return pushed;
}

u32 FollowCameraPositioner::PushOffCollision(const Vector4* goal, const Vector4* point, Vector4* out)
{
    constexpr f32 Clearance = Rounded(0.01);
    u32 pushed = 0;
    Vector4 reached = *goal;
    if (state.cut == 0)
    {
        f32 distanceAlong;
        if (GetCollisionCheck(instance->chunk, &position, goal, SurfaceFlags::BlocksCamera, &distanceAlong, &reached, nullptr)
            != 0)
        {
            Vector4 away;
            away.x = position.x - reached.x;
            away.y = position.y - reached.y;
            away.z = position.z - reached.z;
            away.w = 1.0f;
            f32 inverse = InverseLength(&away, LengthEpsilon);
            away.x = away.x * inverse * Clearance;
            away.y = away.y * inverse * Clearance;
            away.z = away.z * inverse * Clearance;
            reached.x = reached.x + away.x;
            reached.y = reached.y + away.y;
            reached.z = reached.z + away.z;
        }
    }

    *out = Origin();
    *out = reached;
    f32 radius = collisionRadius;
    Box box;
    box.min.x = reached.x - radius;
    box.min.y = reached.y - radius;
    box.min.z = reached.z - radius;
    box.min.w = 1.0f;
    box.max.x = reached.x + radius;
    box.max.y = reached.y + radius;
    box.max.z = reached.z + radius;
    box.max.w = 1.0f;
    GrowBox(GatherMargin, &box);
    RefreshCollisionCache(cache, &box);
    for (CollisionHit* hit = FirstCollisionHit(cache); hit != nullptr; hit = NextCollisionHit(cache))
    {
        CollisionHit triangle = *hit;
        Vector4 to;
        if (SphereTouchesTriangle(collisionRadius, &triangle, out, &to) != 0)
        {
            pushed = 1;
            to.x = to.x - reached.x;
            to.y = to.y - reached.y;
            to.z = to.z - reached.z;
            out->x = out->x + to.x;
            out->y = out->y + to.y;
            out->z = out->z + to.z;
        }
    }

    if (pushed != 0)
    {
        bits.pushedOffCollision = 1;
        bits.unpushed = 0;
        bits.pushedBack = DistanceSquared(goal, point) - DistanceSquared(out, point) < Epsilon;
    }
    else
    {
        // Free once a step goes by without a push
        bits.unpushed = bits.pushedOffCollision == 0;
        bits.pushedOffCollision = 0;
        bits.pushedBack = 0;
    }

    return pushed;
}

u32 FollowCameraPositioner::CastView(const Vector4* from, const Vector4* to, Vector4* hit, u32 instances, u32 lineOfSight)
{
    u32 surfaces = lineOfSight != 0 ? SurfaceFlags::BlocksLineOfSight : SurfaceFlags::BlocksCamera;
    if (instances == 0)
    {
        f32 distanceAlong;
        return GetCollisionCheck(instance->chunk, from, to, surfaces, &distanceAlong, hit, nullptr);
    }

    void* results[MostQueried];
    InstanceQuery query;
    MakeQuery(&query, results, instance);
    if (ignored != nullptr)
    {
        query.skipped[1] = ignored;
    }

    f32 share = 0.0f;
    return SegmentHitsAnything(instance->chunk, from, to, surfaces, &query, EveryInstance, &share, hit, nullptr);
}

u32 FollowCameraPositioner::FindClearPlace(const Vector4* goal, const Vector4* from, Vector4* out, u32 instances)
{
    // Where the view stops within NearDistance of the point is no place; the pull back from where it stops further than the
    // radius and NearDistance (squared) is the radius, NearDistance and PullMargin, else ShortPull
    constexpr f32 NearDistance = 2.0f;
    constexpr f32 NearSquared = NearDistance * NearDistance;
    constexpr f32 PullMargin = Rounded(0.1);
    constexpr f32 ShortPull = Rounded(0.2);
    constexpr f32 Behind = -10.0f;
    f32 radius = collisionRadius;
    f32 longPull = (radius + NearDistance) + PullMargin;
    f32 farSquared = radius * radius + NearSquared;
    u32 found = 0;
    Vector4 hit;
    if (CastView(from, goal, &hit, instances, 0) == 0)
    {
        *out = *goal;
        found = 1;
    }
    else
    {
        f32 lengthSquared = DistanceSquared(&hit, from);
        if (NearSquared < lengthSquared)
        {
            found = 1;
            *out = PullBack(from, &hit, farSquared < lengthSquared ? longPull : ShortPull);
        }
    }

    for (u32 attempt = 0; found == 0 && attempt < 2; attempt++)
    {
        // 10 behind the target at the default pitch, then at its pitch (the pull's ends swapped there, as retail has them)
        Vector4 back;
        back.x = 0.0f;
        back.y = 0.0f;
        back.z = Behind;
        back.w = 1.0f;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        s32 aboutX = attempt == 0 ? g_FollowCameraPitch : pitch.current;
        s32 aboutY = Opposite(yaw.current);
        MatrixFromPitchYaw(&matrix, &aboutX, &aboutY);
        *RowOf(&matrix, 3) = *from;
        Vector4 candidate;
        VuTransformPoint(&matrix, &back, &candidate);
        if (CastView(from, &candidate, &hit, instances, 0) == 0)
        {
            *out = candidate;
            found = 1;
            break;
        }

        f32 lengthSquared = DistanceSquared(&hit, from);
        if (NearSquared < lengthSquared)
        {
            found = 1;
            f32 pull;
            if (attempt == 0)
            {
                pull = farSquared < lengthSquared ? longPull : ShortPull;
            }
            else
            {
                pull = longPull < lengthSquared ? farSquared : ShortPull;
            }

            *out = PullBack(from, &hit, pull);
        }
    }

    if (found != 0)
    {
        bits.pushedOffCollision = 0;
        bits.targetStill = 0;
        bits.unpushed = 1;
        bits.probesOff = 0;
        bits.atTriggerRate = 0;
    }

    return found;
}

u32 FollowCameraPositioner::ViewBlocked(const Vector4* point, const Vector4* other)
{
    // Within 2 (squared)
    constexpr f32 NearSquared = 4.0f;
    if (bits.skipsViewCheck != 0)
    {
        return 0;
    }

    if (bits.pushedBack != 0)
    {
        return 1;
    }

    if ((bits.pushedOffCollision != 0 || bits.pushedOutOfHull != 0) && DistanceSquared(point, other) < NearSquared)
    {
        return 1;
    }

    Vector4 eye = target->objectPosition;
    eye.y = eye.y + EyeHeight;
    Vector4 hit;
    return CastView(&eye, point, &hit, bits.pushedOutOfHull, 1);
}

void FollowCameraPositioner::FollowKeyed(KeyedCamera* keys)
{
    constexpr f32 Ease = Rounded(0.05);
    TimeClock* clock = &G_GameClockController->clocks[FirstClock];
    if (keys->playing == 0)
    {
        keys->Play(clock, &position, &rotation);
        return;
    }

    Vector4 at;
    Vector4 turn;
    if (keys->Play(clock, &at, &turn) != 0)
    {
        keys->playing = 0;
        keys->finished = 1;
        keyed = 0;
        state.cut = 1;
        return;
    }

    Vector4 move;
    move.x = at.x - position.x;
    move.y = at.y - position.y;
    move.z = at.z - position.z;
    move.w = 1.0f;
    move.x = move.x * Ease;
    move.y = move.y * Ease;
    move.z = move.z * Ease;
    position.x = position.x + move.x;
    position.y = position.y + move.y;
    position.z = position.z + move.z;
    SlerpRotations(Ease, &rotation, &rotation, &turn);
}

void FollowCameraPositioner::CheckTargetStill()
{
    // A squared speed
    constexpr f32 Still = Rounded(1e-4);
    const Vector4* velocity = &target->velocity;
    f32 speedSquared = __builtin_fabsf(velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z);
    if (!(speedSquared <= Still) || (bits.stickTurning != 0 && bits.alwaysTakesValues == 0))
    {
        bits.targetStill = 0;
        bits.unpushed = 1;
        bits.probesOff = 0;
        bits.atTriggerRate = 0;
        return;
    }

    // Against the collision it stays where it is (at the trigger's rate, 0) with its probes off
    if (bits.pushedOffCollision != 0 && bits.ignoresValues == 0)
    {
        triggerRate = 0.0f;
        bits.targetStill = 1;
        bits.unpushed = 0;
        bits.probesOff = 1;
        bits.atTriggerRate = 1;
    }
}

void FollowCameraPositioner::PlaceBehind()
{
    Vector4 way;
    way.x = target->point.x - position.x;
    way.z = target->point.z - position.z;
    way.y = target->point.y - position.y;
    way.w = 1.0f;
    distance.current = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
    s32 tilt;
    s32 turn;
    AnglesAlong(&way, &tilt, &turn);
    pitch.current = WrapAngle(tilt);
    yaw.current = WrapAngle(turn);
    AngleFrom(&pitch.delta, 0.0f, AngleRadians);
    AngleFrom(&yaw.delta, 0.0f, AngleRadians);
    distance.delta = 0.0f;
    PlaceFor(&target->point, &position, 1);
    MoveToward(&target->point, &position, 0);
}

void FollowCameraPositioner::Step(TimeClock* clock, CameraTarget* follower)
{
    if (keyed != 0)
    {
        lastPosition = position;
        return;
    }

    stepSeconds = StepSeconds(clock);
    if (bits.holdsStill != 0 || bits.onlyLooksAtTarget != 0)
    {
        Vector4 point = target != nullptr ? target->point : Origin();
        // Retail hands over a place its stack had (read only without the look-only bit, which no camera of the game's has
        // set): where it is stands in
        Vector4 kept = position;
        MoveToward(&point, &kept, 0);
        CheckTargetStill();
        lastPosition = position;
        return;
    }

    MeasureFacing();
    pitch.bits.enabled = 1;
    pitch.bits.pushed = 0;
    fieldOfView.bits.enabled = 1;
    fieldOfView.bits.pushed = 0;
    yaw.bits.enabled = 1;
    yaw.bits.pushed = 0;
    target = follower;
    unused1D4 = 0;
    pitch.delta = 0;
    fieldOfView.delta = 0;
    yaw.delta = 0;
    distance.delta = 0.0f;
    distance.bits.enabled = 1;
    distance.bits.pushed = 0;
    if (state.cut != 0 && state.placed != 0)
    {
        state.cut = 0;
    }

    if ((state.blending != 0 || state.timed != 0) && state.blendStarted == 0)
    {
        blendStart = clock->time;
        blendFrom = position;
        state.blendStarted = 1;
        state.timeEnded = 0;
    }

    Vector4 point = target != nullptr ? target->point : Origin();
    u32 steering = bits.triggerSteers != 0 || bits.ignoresValues != 0;
    Vector4 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    hit.z = 0.0f;
    hit.w = 1.0f;
    u32 jumped = 0;
    if (bits.blockedLong != 0)
    {
        Vector4 eye = target->objectPosition;
        eye.y = eye.y + EyeHeight;
        if (ViewBlocked(&position, &eye) != 0 && FindClearPlace(&position, &target->point, &hit, 1) != 0)
        {
            jumped = 1;
            ClearBlocked();
            smoothed = 0;
            state.cut = 1;
            state.placed = 0;
        }
    }

    if (bits.steers != 0 && steering != 0 && state.cut == 0)
    {
        s32 pitchRate;
        s32 yawRate;
        f32 distanceRate;
        ProbeHits hits;
        hits.value = ProbeRates(&point, &pitchRate, &yawRate, &distanceRate, 1);
        if (hits.value != 0)
        {
            f32 seconds = StepSeconds(clock);
            s32 rate = pitchRate;
            pitch.Push(seconds, &rate, 1);
            rate = yawRate;
            yaw.Push(seconds, &rate, 0);
            if (distance.bits.enabled != 0)
            {
                distance.delta = distanceRate * seconds;
                f32 toLow = distance.low;
                f32 toHigh = distance.high;
                if (distance.bits.secondRange != 0)
                {
                    toLow = distance.secondLow;
                    toHigh = distance.secondHigh;
                }

                distance.KeepWithin(toLow, toHigh);
                distance.bits.pushed = 1;
            }
        }

        // The blenders the probes push don't move on their own this step (retail also tests bit 4 for the distance's, which
        // Probe never sets)
        pitch.bits.enabled = hits.above == 0 && hits.below == 0;
        yaw.bits.enabled = hits.left == 0 && hits.right == 0;
        distance.bits.enabled = hits.pair == 0 && hits.pullIn == 0;
        if (StepBlenders(clock) != 0)
        {
            PushByProbes(clock, &point);
            Vector4 goal;
            PlaceFor(&point, &goal, 0);
            u32 pushed = PushOutOfHulls(&goal, &position, &hit);
            jumped = jumped | (pushed != 0);
            Vector4 off = hit;
            u32 slid = PushOffCollision(&hit, &point, &off);
            jumped = (jumped != 0) | (slid != 0);
            hit = off;
            TimeBlocked(&hit, &point, &hit, clock);
        }
    }
    else
    {
        StepBlenders(clock);
    }

    EndStep(&pitch, clock->time);
    EndStep(&fieldOfView, clock->time);
    EndStep(&yaw, clock->time);
    EndStep(&distance, clock->time);
    if (jumped != 0)
    {
        Vector4 way;
        way.x = target->point.x - hit.x;
        way.z = target->point.z - hit.z;
        way.y = target->point.y - hit.y;
        way.w = 1.0f;
        s32 tilt;
        s32 turn;
        AnglesAlong(&way, &tilt, &turn);
        pitch.current = WrapAngle(tilt);
        yaw.current = WrapAngle(turn);
        AngleFrom(&pitch.delta, 0.0f, AngleRadians);
        AngleFrom(&yaw.delta, 0.0f, AngleRadians);
        distance.delta = 0.0f;
        MoveToward(&point, &hit, jumped);
    }
    else
    {
        Vector4 goal;
        PlaceFor(&point, &goal, 1);
        MoveToward(&point, &goal, 0);
    }

    state.placed = 1;
    fov = fieldOfView.current;
    bits.restarted = 0;
    CheckTargetStill();
    lastPosition = position;
}

void FollowCameraPositioner::Take(f32 value, CameraNode* node, CameraTarget* follower)
{
    if (node != nullptr && node->camera->switches.resetsController != 0)
    {
        camera.flags.value = 0;
        bits.ownCamera = 0;
        camera.flags.steers = 1;
        ClearTriggerValues();
    }

    if (bits.ownCamera != 0)
    {
        TakeCamera(value, &camera, follower);
        bits.unused4 = bits.probesFromTarget;
        trigger = node;
        return;
    }

    if (node != nullptr)
    {
        bits.alwaysTakesValues = node->camera->flags.alwaysTakesValues;
        u32 switched = 0;
        if (bits.alwaysTakesValues != 0)
        {
            bits.ignoresValues = 0;
        }
        else if (bits.ignoresValues != 0)
        {
            // The character moving with the stick let go: the triggers' values taken again (blended to)
            if (bits.characterMoving != 0 && bits.stickTurning == 0)
            {
                state.blendAsked = 1;
                bits.ignoresValues = 0;
                switched = 1;
            }
        }
        // The stick turning it: the triggers' values ignored
        else if (bits.stickTurning != 0)
        {
            state.blendAsked = 0;
            bits.ignoresValues = 1;
            switched = 1;
        }

        if (switched != 0)
        {
            ClearTriggerValues();
        }
    }

    if (node != trigger)
    {
        ClearTriggerValues();
        if (node == nullptr)
        {
            BlendBack(trigger, 1);
        }
        else
        {
            MainCamera* taken = node->camera;
            u32 cut = 0;
            if (taken->flags.noBlendIn != 0 || bits.restarted != 0)
            {
                cut = 1;
            }

            state.cut = cut;
            if (trigger != nullptr)
            {
                u32 same = 0;
                if (trigger->camera->flags.cutsFromSameKind != 0)
                {
                    same = taken->flags.cutsFromSameKind;
                }

                state.cut = state.cut | same;
            }

            state.placed = 0;
            CameraSubtype* second = taken->second;
            if (second != nullptr)
            {
                second->TakeLastVirtual(&position, &target->point);
                if (second->TypeVirtual() == CameraSubtype::TypeKeyed)
                {
                    keyed = 1;
                }
            }
        }
    }

    if (node != nullptr)
    {
        TakeCamera(value, node->camera, follower);
    }

    trigger = node;
    bits.unused4 = bits.probesFromTarget;
}

void FollowCameraPositioner::TakeCamera(f32 value, MainCamera* taken, CameraTarget* follower)
{
    CameraSubtype* second = taken->second;
    CameraSubtype* first = taken->first;
    if (keyed != 0)
    {
        FollowKeyed(reinterpret_cast<KeyedCamera*>(second));
        return;
    }

    MainCameraFlags flags = taken->flags;
    u32 along = 0;
    if (flags.valuesAlongGeometry != 0)
    {
        along = follower != nullptr;
    }

    bits.holdsStill |= flags.holdsStill;
    bits.onlyLooksAtTarget |= flags.onlyLooksAtTarget;
    bits.keepsHeight |= flags.keepsHeight;
    state.alongLine = flags.blendsAlongLine;
    bits.skipsViewCheck = flags.skipsViewCheck;
    bits.addsExtraYaw = flags.addsExtraYaw;
    u32 blendIn = !flags.noBlendIn;
    if (trigger != nullptr && flags.cutsFromSameKind != 0 && trigger->camera->flags.cutsFromSameKind != 0)
    {
        blendIn = 0;
    }

    u32 timed = state.blendAsked != 0 ? 1 : blendIn;
    if (timed != 0 && state.blending == 0)
    {
        f32 seconds = state.blendAsked != 0 ? AskedBlendSeconds : taken->blendTime;
        blendTicks = static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    }

    s32 currentYaw = yaw.current;
    u32 blendInValues = taken->NearerBlendIn(&currentYaw);
    // Only the blend's start sets the blenders' speeds for its time
    u32 canTime = timed != 0 && state.timed == 0 && state.timeEnded == 0;
    if (flags.setsFov != 0)
    {
        s32 start = static_cast<s32>(taken->fovStart);
        s32 end = static_cast<s32>(taken->fovEnd);
        if (along != 0)
        {
            fieldOfView.bits.secondRange = 1;
            s32 blended = BlendAngles(start, end, follower->along);
            fieldOfView.secondHigh = blended;
            fieldOfView.secondLow = blended;
        }
        else
        {
            if (canTime != 0)
            {
                fieldOfView.TimeToward(&blendTicks, &start, &end);
            }

            fieldOfView.bits.secondRange = 1;
            fieldOfView.secondLow = start;
            fieldOfView.secondHigh = end;
        }
    }
    else
    {
        fieldOfView.bits.secondRange = 0;
    }

    if (flags.setsDistance != 0)
    {
        f32 low = taken->distanceStart;
        f32 high = taken->distanceEnd;
        if (blendInValues != 0 && flags.blendsInFromDistance != 0)
        {
            high = taken->blendInDistance;
            low = high;
        }

        if (along != 0)
        {
            f32 share = follower->along;
            distance.bits.secondRange = 1;
            f32 blended = high * share + low * (1.0f - share);
            distance.secondHigh = blended;
            distance.secondLow = blended;
        }
        else
        {
            if (canTime != 0)
            {
                f32 toLow = low - distance.current;
                f32 toHigh = high - distance.current;
                f32 nearer = toLow < toHigh ? toLow : toHigh;
                distance.previousSpeed = distance.speed;
                distance.speed = __builtin_fabsf(nearer / (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit));
            }

            distance.bits.secondRange = 1;
            distance.secondHigh = high;
            distance.secondLow = low;
        }
    }
    else
    {
        distance.bits.secondRange = 0;
    }

    bits.noFacingTilt = !flags.tilts;
    bits.probesOff = flags.noProbes;
    if (bits.probesOff != 0 && bits.ignoresValues != 0)
    {
        bits.probesOff = !taken->switches.keepsProbesWhileIgnoring;
    }

    if (bits.ignoresValues == 0)
    {
        bits.triggerSteers = flags.steers;
        bits.probesFromTarget = first != nullptr;
        if (second != nullptr)
        {
            if (flags.secondAtParameter != 0)
            {
                second->AtParameter(value, &triggerPlace);
            }
            else if (follower != nullptr)
            {
                second->At(first == nullptr ? &follower->point : &follower->objectPosition, follower, &triggerPlace);
            }

            state.blendAsked = 0;
        }

        if (flags.setsPitch != 0)
        {
            s32 start = static_cast<s32>(taken->pitchStart);
            s32 end = static_cast<s32>(taken->pitchEnd);
            if (blendInValues != 0 && flags.blendsInFromPitch != 0)
            {
                end = static_cast<s32>(taken->blendInPitch);
                start = end;
            }

            if (along != 0)
            {
                pitch.bits.secondRange = 1;
                s32 blended = BlendAngles(start, end, follower->along);
                pitch.secondHigh = blended;
                pitch.secondLow = blended;
            }
            else
            {
                if (canTime != 0)
                {
                    pitch.TimeToward(&blendTicks, &start, &end);
                }

                pitch.bits.secondRange = 1;
                pitch.secondLow = start;
                pitch.secondHigh = end;
            }
        }
        else
        {
            pitch.bits.secondRange = 0;
        }

        if (flags.setsYaw != 0)
        {
            s32 start = static_cast<s32>(taken->yawStart);
            s32 end = static_cast<s32>(taken->yawEnd);
            if (blendInValues != 0 && flags.blendsInFromYaw != 0)
            {
                end = static_cast<s32>(taken->blendInYaw);
                start = end;
            }

            if (along != 0)
            {
                yaw.bits.secondRange = 1;
                s32 blended = BlendAngles(start, end, follower->along);
                yaw.secondHigh = blended;
                yaw.secondLow = blended;
            }
            else
            {
                if (canTime != 0)
                {
                    yaw.TimeToward(&blendTicks, &start, &end);
                }

                yaw.bits.secondRange = 1;
                yaw.secondLow = start;
                yaw.secondHigh = end;
                if (start == end)
                {
                    keepsRate = 1;
                }
            }
        }
        else
        {
            yaw.bits.secondRange = 0;
        }

        if (flags.setsYawSpeed != 0)
        {
            yaw.bits.sineSpeed = 1;
            yaw.speed = static_cast<s32>(taken->yawSpeed);
            bits.yawSpeedSet = 1;
        }

        if (flags.setsPositionFollowRate != 0)
        {
            triggerRate = taken->positionFollowRate;
            bits.atTriggerRate = 1;
        }

        if (second != nullptr || keepsRate != 0)
        {
            bits.skipsViewCheck = 1;
        }
    }

    if (second != nullptr && bits.ignoresValues == 0)
    {
        bits.atTriggerPlace = 1;
        state.blending = timed;
    }

    if (timed != 0 && state.timed == 0 && state.timeEnded == 0)
    {
        state.timed = 1;
    }
}

void FollowCameraPositioner::BlendBack(CameraNode* last, u32 time)
{
    MainCamera* taken = last->camera;
    CameraSubtype* second = taken->second;
    u32 timed = state.blendAsked != 0 ? 1 : !taken->flags.noBlendIn;
    if (timed != 0 && state.blending == 0)
    {
        f32 seconds = state.blendAsked != 0 ? AskedBlendSeconds : taken->blendTime;
        blendTicks = static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    }

    // Only the blend's start sets the blenders' speeds for its time
    u32 canTime = timed != 0 && state.timed == 0 && state.timeEnded == 0;
    if (taken->flags.setsFov != 0)
    {
        s32 goal = g_DefaultFov;
        if (canTime != 0)
        {
            fieldOfView.TimeToward(&blendTicks, &goal, &goal);
        }
    }

    if (taken->flags.setsPitch != 0 || second != nullptr)
    {
        s32 goal;
        AngleFrom(&goal, 0.0f, AngleDegrees);
        if (bits.tilts != 0)
        {
            goal = g_FollowCameraPitch;
        }

        if (canTime != 0)
        {
            pitch.TimeToward(&blendTicks, &goal, &goal);
        }
    }

    if ((taken->flags.setsDistance != 0 || second != nullptr) && canTime != 0)
    {
        distance.previousSpeed = distance.speed;
        distance.speed = __builtin_fabsf((DefaultDistance - distance.current) /
                                         (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit));
    }

    if ((taken->flags.setsYaw != 0 || second != nullptr) && canTime != 0)
    {
        // 10 radians, as retail has it
        s32 low;
        AngleFrom(&low, 10.0f, AngleRadians);
        s32 high;
        AngleFrom(&high, 10.0f, AngleRadians);
        yaw.TimeToward(&blendTicks, &low, &high);
    }

    if (time != 0 && canTime != 0)
    {
        state.timed = 1;
    }
}

u32 FollowCameraPositioner::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &triggerPlace, 1);
    u32 moved = 0;
    if (link->flags.linkedRm2Loaded != 0)
    {
        TransformRotationThroughLink(link, &rotation);
        TransformVectorThroughLink(link, &position, 1);
        moved = 1;
    }

    if (moved != 0)
    {
        Vector4 way;
        way.x = target->point.x - position.x;
        way.z = target->point.z - position.z;
        way.y = target->point.y - position.y;
        way.w = 1.0f;
        s32 tilt;
        s32 turn;
        AnglesAlong(&way, &tilt, &turn);
        pitch.current = WrapAngle(tilt);
        yaw.current = WrapAngle(turn);
    }

    return moved;
}

void FollowCameraPositioner::Tilt(Vector4*, Vector4* turned)
{
    // The tilt toward the target's facing (radians, by its facing and sideways values), more past the target's fast speed
    constexpr f32 FastSpeed = 3.0f;
    constexpr f32 FastPitchTilt = Rounded(0.2);
    constexpr f32 PitchTilt = Rounded(0.05);
    constexpr f32 FastYawTilt = 0.25f;
    constexpr f32 YawTilt = Rounded(0.12);
    if (bits.tilts == 0)
    {
        return;
    }

    const Vector4* velocity = &target->velocity;
    f32 speed = __builtin_sqrtf(velocity->x * velocity->x + velocity->z * velocity->z);
    f32 pitchScale = FastSpeed < speed ? FastPitchTilt : PitchTilt;
    s32 change;
    AngleFrom(&change, -facing * pitchScale - static_cast<f32>(tiltPitch) * AngleToRadians, AngleRadians);
    s32 rate = g_CameraTiltRate;
    s32* scaled = MultiplyAngle(&rate, stepSeconds);
    change = static_cast<s32>(static_cast<f32>(change) * (static_cast<f32>(*scaled) * AngleToRadians));
    tiltPitch = tiltPitch + change;
    f32 yawScale = FastSpeed < speed ? FastYawTilt : YawTilt;
    AngleFrom(&change, sideways * yawScale - static_cast<f32>(tiltYaw) * AngleToRadians, AngleRadians);
    rate = g_CameraTiltRate;
    scaled = MultiplyAngle(&rate, stepSeconds);
    change = static_cast<s32>(static_cast<f32>(change) * (static_cast<f32>(*scaled) * AngleToRadians));
    tiltYaw = tiltYaw + change;
    s32 aboutX;
    AngleFrom(&aboutX, 0.0f, AngleRadians);
    s32 aboutZ;
    AngleFrom(&aboutZ, 0.0f, AngleRadians);
    s32 aboutY = tiltYaw;
    Vector4 turn;
    GetRotationXYZ(&turn, &aboutX, &aboutY, &aboutZ);
    Vector4 product;
    MultiplyRotations(&product, &turn, turned);
    *turned = product;
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, turned);
    turned->x = turned->x * inverse;
    turned->y = turned->y * inverse;
    turned->z = turned->z * inverse;
    turned->w = turned->w * inverse;
    aboutX = tiltPitch;
    AngleFrom(&aboutY, 0.0f, AngleRadians);
    AngleFrom(&aboutZ, 0.0f, AngleRadians);
    GetRotationXYZ(&turn, &aboutX, &aboutY, &aboutZ);
    MultiplyRotations(&product, turned, &turn);
    *turned = product;
    inverse = InverseLength4(0.0f, InverseEpsilon, turned);
    turned->x = turned->x * inverse;
    turned->y = turned->y * inverse;
    turned->z = turned->z * inverse;
    turned->w = turned->w * inverse;
}

FollowCameraTarget* FollowCameraTarget::Construct(FollowCameraTarget* target)
{
    CameraTarget::ConstructBase(target);
    target->followed = nullptr;
    target->vtable = g_FollowCameraTargetVTable;
    MainCamera::Construct(&target->camera);
    target->boxMin = Origin();
    target->boxMax = Origin();
    target->startMin = Origin();
    target->startMax = Origin();
    target->facesMovement = 0;
    target->Reset();
    return target;
}

void FollowCameraTarget::Destroy(u32 destroyFlags)
{
    vtable = g_FollowCameraTargetVTable;
    camera.Destroy(DestroyOnly);
    RemoveReference(&followed);
    vtable = g_CameraTargetVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void FollowCameraTarget::ClearTriggerValues()
{
    bits.atCameraPoint = 0;
    bits.blends = 0;
    bits.blending = 0;
    bits.givenBox = 0;
    bits.framesInstances = 0;
    bits.pointFollows = 0;
    bits.boxBlending = 0;
    bits.cut = 0;
    bits.stepped = 1;
    bits.settled = 1;
    bits.blendsWhenNear = 0;
    bits.boxUnturned = 0;
    smoothed = 1;
}

void FollowCameraTarget::ClearBox()
{
    bits.givenBox = 0;
}

void FollowCameraTarget::SetBox(const Vector4* min, const Vector4* max)
{
    givenMin = *min;
    bits.givenBox = 1;
    givenMax = *max;
}

void FollowCameraTarget::Reset()
{
    bits.value = 0;
    boxOffset = boxMin;
    fixedPlace = nullptr;
    trigger = nullptr;
    boxShare = 0.5f;
    boxOffset.x = boxOffset.x + boxMax.x;
    boxOffset.y = boxOffset.y + boxMax.y;
    boxOffset.z = boxOffset.z + boxMax.z;
    boxOffset.x = boxOffset.x * 0.5f;
    boxOffset.y = boxOffset.y * 0.5f;
    boxOffset.z = boxOffset.z * 0.5f;
    ReferencedObject* object = followed != nullptr ? followed->object : nullptr;
    if (object == nullptr)
    {
        rotation.z = 0.0f;
        rotation.y = 0.0f;
        rotation.x = 0.0f;
        rotation.w = 1.0f;
        point = Origin();
        easedPoint = Origin();
        along = 0.0f;
        easedHeight = 0.0f;
    }
    else
    {
        ObjectPlace* place = object->place;
        RotateAndTranslate(place);
        ObjectPoint(place, &point);
        GetRotationVec(&rotation, &place->matrix);
        objectPosition = point;
        objectRotation = rotation;
        easedHeight = place->matrix.m[3][1];
        EaseHeight(0.0f, &point, &easedPoint);
        Vector4 turned;
        if (BoxUnturned(bits) != 0)
        {
            turned = boxOffset;
        }
        else
        {
            RotateByQuaternion(&rotation, &boxOffset, &turned, 0);
        }

        easedPoint.x = easedPoint.x + turned.x;
        easedPoint.y = easedPoint.y + turned.y;
        easedPoint.z = easedPoint.z + turned.z;
    }

    bits.pointFollows = 0;
    lastTrigger = nullptr;
    groundHeight = easedHeight;
    currentMin = Origin();
    currentMax = Origin();
    bits.wasReset = 1;
    cameraPoint = Origin();
    ClearTriggerValues();
    camera.flags.value = 0;
    bits.ownCamera = 0;
    camera.flags.steers = 1;
    ClearTriggerValues();
}

void FollowCameraTarget::ObjectPoint(ObjectPlace* place, Vector4* out)
{
    const Vector4* position = RowOf(&place->matrix, 3);
    CameraNode* node = trigger;
    if (node == nullptr || bits.framesInstances == 0 || node->bits.instanceCount == 0)
    {
        *out = *position;
        return;
    }

    u32 count = node->bits.instanceCount;
    Vector4 sum;
    sum.x = 0.0f;
    sum.w = 1.0f;
    sum.y = 0.0f;
    sum.z = 0.0f;
    for (u32 index = 0; index < count; index++)
    {
        ObjectPlace* other = trigger->instances[index]->place;
        RotateAndTranslate(other);
        sum.x = sum.x + other->matrix.m[3][0];
        sum.y = sum.y + other->matrix.m[3][1];
        sum.z = sum.z + other->matrix.m[3][2];
    }

    f32 inverse = 1.0f / static_cast<f32>(static_cast<s32>(count));
    f32 x = (sum.x * inverse - position->x) * framingShare;
    f32 y = (sum.y * inverse - position->y) * framingShare;
    f32 z = (sum.z * inverse - position->z) * framingShare;
    f32 length = __builtin_sqrtf(x * x + y * y + z * z);
    along = length;
    if (framingDistance < length)
    {
        f32 scale = framingDistance / length;
        along = 1.0f;
        z = z * scale;
        x = x * scale;
        y = y * scale;
    }
    else
    {
        along = length / framingDistance;
    }

    *out = *position;
    out->x = out->x + x;
    out->y = out->y + y;
    out->z = out->z + z;
}

void FollowCameraTarget::EaseHeight(f32 seconds, const Vector4* followedPoint, Vector4* out)
{
    constexpr f32 RiseRate = 5.0f;
    constexpr f32 Margin = Rounded(0.1);
    f32 ground = groundHeight;
    f32 eased = easedHeight;
    if (eased < ground)
    {
        f32 level = followedPoint->y - boxOffset.y;
        f32 raised = seconds * RiseRate + eased;
        if (eased < level && Epsilon < boxOffset.y)
        {
            easedHeight = level;
        }
        else if (raised < groundHeight)
        {
            easedHeight = raised;
        }
        else
        {
            easedHeight = groundHeight;
        }
    }
    else
    {
        f32 y = followedPoint->y;
        f32 top = y + Margin;
        if (top < ground)
        {
            easedHeight = ground;
        }
        else
        {
            f32 level = top - boxOffset.y;
            if (eased < level)
            {
                easedHeight = boxOffset.y < Epsilon ? y : level;
            }
            else if (top < eased)
            {
                easedHeight = top;
            }
        }
    }

    out->x = followedPoint->x;
    out->w = 1.0f;
    out->y = easedHeight;
    out->z = followedPoint->z;
}

void FollowCameraTarget::Step(TimeClock* clock)
{
    // A camera point this near (squared) the last point isn't blended to (unless the trigger blends when near)
    constexpr f32 NearSquared = 15.0f;
    ReferencedObject* object = followed != nullptr ? followed->object : nullptr;
    if (object == nullptr)
    {
        rotation.z = 0.0f;
        rotation.y = 0.0f;
        rotation.x = 0.0f;
        rotation.w = 1.0f;
        point = Origin();
        easedPoint = Origin();
        along = 0.0f;
        return;
    }

    f32 seconds = StepSeconds(clock);
    Vector4 old = point;
    if (bits.cut != 0 && bits.stepped != 0)
    {
        smoothed = 1;
        bits.cut = 0;
    }

    if (fixedPlace == nullptr)
    {
        ObjectPlace* place = (followed != nullptr ? followed->object : nullptr)->place;
        RotateAndTranslate(place);
        ObjectPoint(place, &objectPosition);
        GetRotationVec(&objectRotation, &place->matrix);
    }
    else
    {
        GetRotationVec(&objectRotation, &fixedPlace->matrix);
        along = 0.0f;
        objectPosition = *RowOf(&fixedPlace->matrix, 3);
    }

    rotation = objectRotation;
    point = objectPosition;
    // A blend asked for starts (the box blending too unless it's the same box)
    if (bits.blends != 0 && bits.blending == 0 && bits.boxBlending == 0 && bits.settled == 0)
    {
        blendStart = clock->time;
        bits.blending = 1;
        if (bits.blendsWhenNear == 0 && DistanceSquared(&cameraPoint, &old) < NearSquared)
        {
            bits.blending = 0;
        }

        u32 differs;
        if (bits.givenBox != 0)
        {
            differs = SamePoint(&currentMin, &givenMin) == 0 || SamePoint(&currentMax, &givenMax) == 0;
        }
        else
        {
            differs = SamePoint(&currentMin, &boxMin) == 0 || SamePoint(&currentMax, &boxMax) == 0;
        }

        if (differs != 0)
        {
            bits.boxBlending = 1;
        }
        else
        {
            bits.settled = 1;
        }
    }

    if (bits.givenBox != 0)
    {
        currentMin = givenMin;
        currentMax = givenMax;
    }
    else
    {
        currentMin = boxMin;
        currentMax = boxMax;
    }

    if (bits.boxBlending != 0)
    {
        s32 since = static_cast<s32>(clock->time - blendStart);
        if (since < blendTicks)
        {
            f32 share = static_cast<f32>(since) * g_SecondsPerClockUnit / (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit);
            Vector4* corners[2] = {&currentMin, &currentMax};
            const Vector4* starts[2] = {&startMin, &startMax};
            for (u32 index = 0; index < 2; index++)
            {
                Vector4 way;
                way.x = corners[index]->x - starts[index]->x;
                way.y = corners[index]->y - starts[index]->y;
                way.z = corners[index]->z - starts[index]->z;
                way.w = 1.0f;
                Vector4 part;
                part.x = way.x * share;
                part.y = way.y * share;
                part.z = way.z * share;
                part.w = 1.0f;
                *corners[index] = part;
                corners[index]->x = corners[index]->x + starts[index]->x;
                corners[index]->y = corners[index]->y + starts[index]->y;
                corners[index]->z = corners[index]->z + starts[index]->z;
            }
        }
        else
        {
            bits.settled = 1;
            bits.boxBlending = 0;
        }
    }

    boxOffset.x = currentMin.x + (currentMax.x - currentMin.x) * boxShare;
    boxOffset.y = currentMin.y + (currentMax.y - currentMin.y) * boxShare;
    boxOffset.w = 1.0f;
    boxOffset.z = currentMin.z + (currentMax.z - currentMin.z) * boxShare;
    if (bits.heightUneased == 0)
    {
        EaseHeight(seconds, &point, &easedPoint);
    }
    else
    {
        easedPoint = point;
    }

    Vector4 turned;
    if (BoxUnturned(bits) != 0)
    {
        turned = boxOffset;
    }
    else
    {
        RotateByQuaternion(&rotation, &boxOffset, &turned, 0);
    }

    easedPoint.x = easedPoint.x + turned.x;
    easedPoint.y = easedPoint.y + turned.y;
    easedPoint.z = easedPoint.z + turned.z;
    if (bits.pointFollows != 0)
    {
        cameraPoint = easedPoint;
    }

    if (bits.blending == 0)
    {
        point = bits.atCameraPoint != 0 ? cameraPoint : easedPoint;
    }
    else
    {
        s32 since = static_cast<s32>(clock->time - blendStart);
        if (!(since < blendTicks))
        {
            point = cameraPoint;
            if (trigger == nullptr)
            {
                easedPoint = cameraPoint;
            }
        }
        else
        {
            f32 share = static_cast<f32>(since) * g_SecondsPerClockUnit / (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit);
            if (bits.curve == FollowCurveCubic)
            {
                share = share * (share * share);
            }

            point = easedPoint;
            point.x = old.x + (cameraPoint.x - old.x) * share;
            point.y = old.y + (cameraPoint.y - old.y) * share;
            point.w = 1.0f;
            point.z = old.z + (cameraPoint.z - old.z) * share;
        }
    }

    bits.stepped = 1;
}

f32 FollowCameraTarget::TakeCamera(MainCamera* taken)
{
    f32 value = 0.0f;
    MainCameraFlags flags = taken->flags;
    CameraSubtype* first = taken->first;
    bits.givenBox = flags.givesTargetBox;
    bits.framesInstances = flags.framesInstances;
    bits.boxUnturned = flags.targetBoxUnturned;
    u32 blendIn = !flags.noBlendIn;
    if (first != nullptr)
    {
        value = first->At(&easedPoint, this, &cameraPoint);
    }
    else
    {
        bits.pointFollows = 1;
    }

    if (bits.givenBox != 0)
    {
        givenMin = taken->targetBoxMin;
        givenMax = taken->targetBoxMax;
    }

    if (blendIn != 0 && bits.blends == 0)
    {
        blendTicks = static_cast<s32>(taken->blendTime * g_ClockUnitsPerSecond);
        blendFrom = point;
        startMin = currentMin;
        startMax = currentMax;
        bits.blendsWhenNear = taken->flags.blendsWhenNear;
        bits.settled = 0;
    }

    bits.atCameraPoint = 1;
    bits.blends = blendIn;
    if (bits.framesInstances != 0)
    {
        framingShare = taken->framingShare;
        framingDistance = taken->framingDistance;
    }

    return value;
}

f32 FollowCameraTarget::Value(CameraNode* node)
{
    f32 value = 0.0f;
    if (node != nullptr && node->camera->switches.resetsController != 0)
    {
        camera.flags.value = 0;
        bits.ownCamera = 0;
        camera.flags.steers = 1;
        ClearTriggerValues();
    }

    if (bits.ownCamera != 0)
    {
        TakeCamera(&camera);
    }
    else
    {
        // The trigger before the last: a change of trigger is taken on two steps in a row
        CameraNode* last = lastTrigger;
        if (last != node)
        {
            ClearTriggerValues();
            if (last != nullptr && node == nullptr)
            {
                MainCamera* previous = last->camera;
                bits.pointFollows = 1;
                u32 blendIn = !previous->flags.noBlendIn;
                if (blendIn != 0 && bits.blends == 0)
                {
                    blendTicks = static_cast<s32>(previous->blendTime * g_ClockUnitsPerSecond);
                    blendFrom = point;
                    startMin = currentMin;
                    startMax = currentMax;
                    bits.blendsWhenNear = previous->flags.blendsWhenNear;
                    bits.settled = 0;
                }

                bits.atCameraPoint = 1;
                bits.blends = blendIn;
                bits.lastBoxUnturned = previous->flags.targetBoxUnturned;
            }
            else
            {
                u32 cut = 0;
                if (node->camera->flags.noBlendIn != 0 || bits.wasReset != 0)
                {
                    cut = 1;
                }

                bits.cut = cut;
                bits.stepped = 0;
                smoothed = !bits.cut;
                if (lastTrigger != nullptr)
                {
                    bits.lastBoxUnturned = lastTrigger->camera->flags.targetBoxUnturned;
                }
                else
                {
                    bits.lastBoxUnturned = 0;
                }
            }
        }

        if (node != nullptr)
        {
            value = TakeCamera(node->camera);
        }
    }

    bits.wasReset = 0;
    lastTrigger = trigger;
    trigger = node;
    return value;
}

u32 FollowCameraTarget::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    Vector4 ground;
    ground.y = groundHeight;
    ground.x = 0.0f;
    ground.z = 0.0f;
    ground.w = 1.0f;
    Vector4 eased;
    eased.y = easedHeight;
    eased.x = 0.0f;
    eased.z = 0.0f;
    eased.w = 1.0f;
    TransformVectorThroughLink(link, &ground, 1);
    TransformVectorThroughLink(link, &eased, 1);
    groundHeight = ground.y;
    easedHeight = eased.y;
    TransformVectorThroughLink(link, &cameraPoint, 1);
    TransformRotationThroughLink(link, &rotation);
    TransformVectorThroughLink(link, &point, 1);
    return 1;
}
