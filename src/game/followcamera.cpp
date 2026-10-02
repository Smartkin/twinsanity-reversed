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
EABI_EXPORT(FUN_00274e08, &FollowCameraPositioner::Apply);
EABI_EXPORT(FUN_00277398, &FollowCameraTarget::EaseHeight);

namespace
{
constexpr f32 TinyLength = 0x1.5798ecp-29f;
constexpr f32 NoMove = Rounded(5e-5);
constexpr f32 Far = 0x1.93e594p+99f;
// 65536ths of a turn to radians and back
constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr f32 RadiansToAngle = 0x1.45f306p+13f;
constexpr f32 DefaultRate = 6.0f;
constexpr f32 DefaultRadius = Rounded(0.4);
constexpr f32 DefaultDistance = 10.0f;
// A blend asked for without a trigger
constexpr f32 AskedBlendSeconds = 2.0f;
// The collision's surfaces the camera can't go through (bit 5), and those lines of sight can't (bit 7)
constexpr u32 CameraSurfaces = 0x20;
constexpr u32 SightSurfaces = 0x80;
constexpr u32 EveryInstance = 0xFFFFFFFF;
constexpr s32 MostQueried = 0x10;
// The instances the view checks find: ones with a sphere (bit 4), awake
constexpr u32 QueriedFlags = 0x10;

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
    return static_cast<s32>((static_cast<u32>(angle) & 0xFFFF) - 0x8000);
}

// The query of the instances a view check makes (the instance itself and its attachment left out, and the one it ignores)
void MakeQuery(InstanceRayHit* query, void** results, InstanceContext* instance)
{
    query->results = results;
    query->count = 0;
    query->most = MostQueried;
    query->distance = Far;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = QueriedFlags;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->skipped[0] = nullptr;
    query->instance = nullptr;
    query->skipped[1] = nullptr;
    SkipInQuery(query, instance);
}

// A blender's step ended: its hold starts again once it was pushed (or before it ever held), its input and rate scale cleared
template <typename Blender>
void EndStep(Blender* blender, u32 time)
{
    if ((blender->bits & AngleBlender::BitPushed) != 0 || blender->holdStart == 0)
    {
        blender->holdStart = time;
    }

    blender->input = 0.0f;
    blender->rateScale = 0.0f;
}

// The probe cast next
u64 NextProbe(u64 bits)
{
    u64 next = (((bits >> FollowCameraPositioner::ProbeShift & 3) + 1) & 3) << FollowCameraPositioner::ProbeShift;
    return (bits & ~static_cast<u64>(FollowCameraPositioner::ProbeMask)) | next;
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
    f32 inverse = InverseLength(&way, TinyLength);
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

u32 SamePoint(const Vector4* a, const Vector4* b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

// Whether a camera target's box point stays as it is (its trigger's bit 28; the last one's while it doesn't settle)
u32 OffsetUnturned(u32 bits)
{
    return (bits & FollowCameraTarget::BitOffsetUnturned) != 0 ||
           (bits & (FollowCameraTarget::BitSettled | FollowCameraTarget::BitLastOffsetUnturned)) ==
               FollowCameraTarget::BitLastOffsetUnturned;
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
    constexpr f32 ProbeTurn = 180.0f;
    constexpr u8 Unknown384 = 60;
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
    blender->unknown48 = 0.0f;
    blender->Reset();
    positioner->unknown1C8 = 1.0f;
    AngleFrom(&positioner->pitchPushRate, ProbeTurn, AngleDegrees);
    AngleFrom(&positioner->yawPushRate, ProbeTurn, AngleDegrees);
    MainCamera::Construct(&positioner->camera);
    positioner->stepSeconds = 0.0f;
    positioner->turnShare = 1.0f;
    positioner->unknown398 = 1.0f;
    positioner->cache = nullptr;
    positioner->ownRate = DefaultRate;
    positioner->unknown384 = Unknown384;
    AngleFrom(&positioner->roll, positioner->stepSeconds, AngleRadians);
    positioner->bits = (positioner->bits | BitSteers) & ~static_cast<u64>(BitDistanceFollowsPitch) & ~BitPushedOffCollision;
    positioner->keyed = 0;
    positioner->fieldOfView.bits |= AngleBlender::BitHolds;
    positioner->lowTargetOffset = Origin();
    positioner->highTargetOffset = Origin();
    positioner->lowCameraOffset = Origin();
    positioner->highCameraOffset = Origin();
    for (u32 index = 0; index < 4; index += 2)
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

    positioner->place = Origin();
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

    camera.Destroy(2);
    vtable = g_CameraPositionerVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void FollowCameraPositioner::Reset(InstanceContext* followed, CameraTarget* follower)
{
    constexpr u32 CacheSize = 0x50;
    target = follower;
    instance = followed;
    if (cache != nullptr)
    {
        DestroyCollisionCache(cache, DestroyAndFree);
    }

    cache = nullptr;
    cache = ConstructCollisionCache(static_cast<CollisionCache*>(MemoryAllocate(CacheSize)), followed, CameraSurfaces);
    Restart();
}

void FollowCameraPositioner::Restart()
{
    u64 old = bits;
    u64 keptTilts = old >> 8 & 1;
    u64 keptFollows = old >> 1 & 1;
    u64 keptSteers = old >> 46 & 1;
    EndBlend();
    pitch.initial = g_FollowCameraPitch;
    pitch.Reset();
    fieldOfView.Reset();
    s32 behind;
    TargetYaw(&behind, this);
    yaw.initial = behind;
    yaw.Reset();
    place = Origin();
    if (keptFollows != 0)
    {
        distance.initial = distance.high * pitch.share + distance.low * (1.0f - pitch.share);
    }

    distance.Reset();
    bits &= 0xFFFFFFFF'00000000;
    state &= 0xFFFF0000;
    probeHits = 0;
    bits = (bits & ~BitPushedOutOfHull & ~BitPushedBack & ~BitSteers) | keptSteers << 46;
    bits = (bits & ~static_cast<u64>(BitDistanceFollowsPitch)) | keptFollows << 1;
    bits = (bits & ~static_cast<u64>(BitTilts)) | keptTilts << 8;
    Vector4 point = target != nullptr ? target->point : Origin();
    Vector4 placed;
    PlaceFor(&point, &placed, 0);
    position = target != nullptr ? placed : Origin();
    fov = fieldOfView.current;
    bits &= ~static_cast<u64>(Bit3 | BitHadProbesFromTarget | BitIgnoresValues | Bit6);
    state &= ~StateBlendAsked;
    trigger = nullptr;
    Clear();
    AngleFrom(&tiltPitch, 0.0f, AngleRadians);
    AngleFrom(&tiltYaw, 0.0f, AngleRadians);
    blockedSince = 0;
    backOff = 0.0f;
    collisionRadius = DefaultRadius;
    bits = (bits | BitRestarted) & ~BitBlockedAWhile & ~BitBlockedLong & ~BitBlocked;
    bits |= BitFree;
    hullRadius = DefaultRadius;
    ClearProbes();
    camera.flags = 0;
    bits &= ~static_cast<u64>(BitOwnCamera);
    camera.flags = (camera.flags & ~MainCamera::FlagSteers) | MainCamera::FlagSteers;
    Clear();
    ignored = nullptr;
    if (target == nullptr)
    {
        return;
    }

    Vector4 from = target->objectPosition;
    from.y = from.y + 1.0f;
    Vector4 clear;
    if (ViewBlocked(&position, &from) != 0 && FindClearPlace(&position, &from, &clear, 0) != 0)
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

void FollowCameraPositioner::Clear()
{
    EndBlend();
    bits |= BitTriggerBit4;
    bits &= ~static_cast<u64>(BitProbesFromTarget | BitAtPlace | BitTriggerBit11 | 0x200 | BitYawExtraSpeed | BitStill |
                              BitOnlyLooks | BitKeepsHeight | BitTriggerRate | BitViewUnchecked | BitYawExtra |
                              BitTriggerBit22Clear);
    bits &= ~BitProbesOff;
    state = (state & ~(StateBlending | StateBlendStarted | StateCut)) | StatePlaced;
    state &= ~(StateTimed | StateTimeEnded | StateBlendAsked | StateAlongLine);
    pitch.bits &= ~AngleBlender::BitSecondRange;
    fieldOfView.bits &= ~AngleBlender::BitSecondRange;
    distance.bits &= ~AngleBlender::BitSecondRange;
    yaw.bits &= ~(AngleBlender::BitSecondRange | AngleBlender::BitSineSpeed);
    triggerRate = DefaultRate;
    unknown398 = 1.0f;
    keepsRate = 0;
    smoothed = 0;
}

void FollowCameraPositioner::EndBlend()
{
    state = (state & ~StateTimed) | StateTimeEnded;
    fieldOfView.speed = fieldOfView.previousSpeed;
    distance.speed = distance.previousSpeed;
    yaw.speed = yaw.previousSpeed;
    pitch.speed = pitch.previousSpeed;
}

void FollowCameraPositioner::ClearProbes()
{
    bits &= ~(BitLeftHit | BitRightHit | BitAboveHit | BitBelowHit | ProbeMask);
}

void FollowCameraPositioner::ClearBlocked()
{
    bits &= ~(BitBlocked | BitBlockedAWhile | BitBlockedLong);
}

void FollowCameraPositioner::EaseBackOff()
{
    constexpr f32 Ease = Rounded(0.05);
    backOff = backOff + (__builtin_fabsf(facing + facing) - backOff) * Ease;
}

u32 FollowCameraPositioner::KeepsRigRotation()
{
    return bits >> 12 & 1;
}

void FollowCameraPositioner::SetTilts(u32 tilts)
{
    bits = (bits & ~static_cast<u64>(BitTilts)) | static_cast<u64>(tilts & 1) << 8;
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
    return GetCollisionCheck(instance->chunk, from, to, CameraSurfaces, nullptr, nullptr, nullptr);
}

void FollowCameraPositioner::TimeBlocked(const Vector4* point, const Vector4* goal, const Vector4*, TimeClock* clock)
{
    constexpr f32 Long = Rounded(0.7);
    constexpr f32 AWhile = Rounded(0.2);
    if (ViewBlocked(point, goal) == 0)
    {
        ClearBlocked();
        return;
    }

    if ((bits & BitBlocked) == 0)
    {
        bits |= BitBlocked;
        blockedSince = clock->time;
    }

    s32 since = static_cast<s32>(clock->time - blockedSince);
    if (static_cast<s32>(g_ClockUnitsPerSecond * Long) < since)
    {
        bits |= BitBlockedLong;
    }
    else if (static_cast<s32>(g_ClockUnitsPerSecond * AWhile) < since)
    {
        bits |= BitBlockedAWhile;
    }
}

s32* FollowCameraPositioner::TargetYaw(s32* angle, FollowCameraPositioner* positioner)
{
    Matrix4x4 matrix;
    u32 turned = 1;
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
        if (LengthSquared(&way) <= TinyLength)
        {
            turned = 0;
        }

        MatrixBetween(&matrix, &ahead, &way);
    }
    else
    {
        MatrixFromRotation(&matrix, &follower->rotation);
    }

    s32 result;
    if (turned != 0)
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
    s32 x = pitch.current + pitch.delta;
    s32 view = fieldOfView.current + fieldOfView.delta;
    s32 y = yaw.current + yaw.delta;
    if ((bits & BitYawExtra) != 0)
    {
        y = y + yawExtra;
    }

    f32 length = distance.current + (distance.delta + backOff);
    x = WrapAngle(x);
    y = WrapAngle(y);
    view = WrapAngle(view);
    s32 z;
    AngleFrom(&z, 0.0f, AngleRadians);
    Matrix4x4 matrix;
    MatrixFromAngles(&matrix, &x, &y, &z);
    *RowOf(&matrix, 3) = *point;
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = -length;
    out->w = 1.0f;
    VuTransformPoint(&matrix, out, out);
    if ((bits & BitKeepsHeight) != 0)
    {
        out->y = position.y;
    }

    if (commit != 0)
    {
        pitch.current = x;
        fieldOfView.current = view;
        yaw.current = y;
        distance.current = length - backOff;
    }
}

void FollowCameraPositioner::MoveToward(const Vector4* point, const Vector4* goal, u32 jumped)
{
    constexpr f32 Steady = Rounded(0.001);
    f32 share = (bits & BitTriggerRate) != 0 ? triggerRate : ownRate;
    share = share * stepSeconds;
    if (1.0f < share || (state & StateCut) != 0)
    {
        share = 1.0f;
    }

    if ((bits & (BitOnlyLooks | BitKeepsHeight)) != 0)
    {
        Vector4 up;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        up.w = 1.0f;
        Matrix4x4 matrix;
        LookAtMatrix(&matrix, &position, point, &up);
        GetRotationVec(&rotation, &matrix);
        if ((bits & BitOnlyLooks) != 0)
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
        s32 roll0;
        AngleFrom(&roll0, 0.0f, AngleRadians);
        s32 x = newPitch;
        s32 y = newYaw;
        Matrix4x4 matrix;
        MatrixFromAngles(&matrix, &x, &y, &roll0);
        *RowOf(&matrix, 3) = *point;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = -length;
        position.w = 1.0f;
        VuTransformPoint(&matrix, &position, &position);
        if ((bits & BitKeepsHeight) != 0)
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

    s32 tilt = roll;
    s32 x = newPitch;
    s32 y = newYaw;
    Vector4 aimed;
    GetRotationXYZ(&aimed, &x, &y, &tilt);
    MultiplyRotations(&aimed, &aimed, &turn);
    SlerpRotations(turnShare, &rotation, &rotation, &aimed);
    CallVirtual<void>(this, vtable, 7, &position, &rotation);
    yaw.start = newYaw;
    fieldOfView.start = newView;
    pitch.start = newPitch;
    distance.start = length;
}

u32 FollowCameraPositioner::Probe(const Vector4* point, const Vector4* goal, u32 keep)
{
    constexpr f32 CameraDepth = Rounded(-0.42);
    constexpr f32 PullFurther = 2.0f;
    s32 cast = 0;
    u32 hits = 0;
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
    f32 inverse = InverseLength(&ahead, TinyLength);
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
    if ((bits & BitProbesFromTarget) != 0)
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
        object.y = object.y + 1.0f;
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
    f32 targetSides[4];
    f32 cameraSides[4];
    for (u32 index = 0; index < 4; index++)
    {
        targetSides[index] = highTargetSides[index] * share + lowTargetSides[index] * rest;
        cameraSides[index] = highCameraSides[index] * share + lowCameraSides[index] * rest;
    }

    if ((pitch.bits & AngleBlender::BitEnabled) != 0)
    {
        for (u32 index = 0; index < 2; index++)
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

        VuTransformPoint(&targetFrame, &probeTargetEnds[0], &probeTargetEnds[0]);
        VuTransformPoint(&targetFrame, &probeTargetEnds[1], &probeTargetEnds[1]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[0], &probeCameraEnds[0]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[1], &probeCameraEnds[1]);
        if (cast < 4 && (bits & ProbeMask) == 0)
        {
            if (SegmentBlocked(&probeCameraEnds[0], &probeTargetEnds[0]) != 0)
            {
                hits = 1;
            }

            bits = NextProbe(bits);
            cast = 1;
            if (keep != 0)
            {
                bits = (bits & ~BitAboveHit) | static_cast<u64>(hits & 1) << 41;
            }
        }
        else if ((bits & BitAboveHit) != 0)
        {
            hits = 1;
        }

        if (probeFloor < probeTargetEnds[1].y)
        {
            if (cast < 4 && (bits & ProbeMask) == 1ull << ProbeShift)
            {
                if (SegmentBlocked(&probeCameraEnds[1], &probeTargetEnds[1]) != 0)
                {
                    hits |= 2;
                }

                bits = NextProbe(bits);
                cast = static_cast<s8>(cast + 1);
                if (keep != 0)
                {
                    bits = (bits & ~BitBelowHit) | static_cast<u64>(hits >> 1 & 1) << 42;
                }
            }
            else if ((bits & BitBelowHit) != 0)
            {
                hits |= 2;
            }
        }
        else
        {
            belowFloor = 1;
        }
    }

    if ((yaw.bits & AngleBlender::BitEnabled) != 0)
    {
        for (u32 index = 2; index < 4; index++)
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

        VuTransformPoint(&targetFrame, &probeTargetEnds[2], &probeTargetEnds[2]);
        VuTransformPoint(&targetFrame, &probeTargetEnds[3], &probeTargetEnds[3]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[2], &probeCameraEnds[2]);
        VuTransformPoint(&cameraFrame, &probeCameraEnds[3], &probeCameraEnds[3]);
        if (cast < 4 && (bits & ProbeMask) == 2ull << ProbeShift)
        {
            if (SegmentBlocked(&probeCameraEnds[2], &probeTargetEnds[2]) != 0)
            {
                hits |= 4;
            }

            bits = NextProbe(bits);
            cast = static_cast<s8>(cast + 1);
            if (keep != 0)
            {
                bits = (bits & ~BitLeftHit) | static_cast<u64>(hits >> 2 & 1) << 39;
            }
        }
        else if ((bits & BitLeftHit) != 0)
        {
            hits |= 4;
        }

        if (cast < 4 && (bits & ProbeMask) == ProbeMask)
        {
            if (SegmentBlocked(&probeCameraEnds[3], &probeTargetEnds[3]) != 0)
            {
                hits |= 8;
            }

            bits &= ~ProbeMask;
            if (keep != 0)
            {
                bits = (bits & ~BitRightHit) | static_cast<u64>(hits >> 3 & 1) << 40;
            }
        }
        else if ((bits & BitRightHit) != 0)
        {
            hits |= 8;
        }
    }

    if ((distance.bits & AngleBlender::BitEnabled) != 0)
    {
        u32 bothUpright = (hits & 3) == 3;
        u32 noneUpright = (hits & 3) == 0;
        u32 bothSides = (hits & 0xC) == 0xC;
        if (bothSides != 0 && noneUpright != 0)
        {
            if (belowFloor == 0)
            {
                hits |= 0x22;
            }
        }
        else if (bothUpright != 0 || bothSides != 0)
        {
            hits |= 0x20;
        }

        if ((bits & Bit3) != 0 && hits != 0 && bothUpright == 0 && bothSides == 0 && PullFurther < distance.current)
        {
            hits |= 0x40;
        }
    }

    probeHits = static_cast<u8>(hits | probeHits);
    return hits;
}

u32 FollowCameraPositioner::ProbeRates(const Vector4* point, s32* pitchRate, s32* yawRate, f32* distanceRate, u32 keep)
{
    constexpr f32 PullRate = -20.0f;
    if ((bits & BitProbesOff) != 0)
    {
        *pitchRate = 0;
        *yawRate = 0;
        *distanceRate = 0.0f;
        return 0;
    }

    Vector4 goal;
    PlaceFor(point, &goal, 0);
    u32 hits = Probe(point, &goal, keep);
    if ((hits & 4) != 0)
    {
        *yawRate = (hits & 8) != 0 ? 0 : -yawPushRate;
    }
    else
    {
        *yawRate = (hits & 8) != 0 ? yawPushRate : 0;
    }

    if ((hits & 1) != 0)
    {
        *pitchRate = (hits & 2) != 0 ? 0 : -pitchPushRate;
    }
    else
    {
        *pitchRate = (hits & 2) != 0 ? pitchPushRate : 0;
    }

    *distanceRate = 0.0f;
    if ((hits & 0x40) != 0)
    {
        *distanceRate = PullRate;
    }

    return hits;
}

u32 FollowCameraPositioner::PushByProbes(TimeClock* clock, const Vector4* point)
{
    s32 pitchRate;
    s32 yawRate;
    f32 distanceRate;
    u32 hits = ProbeRates(point, &pitchRate, &yawRate, &distanceRate, 1);
    f32 seconds = StepSeconds(clock);
    if ((hits & 0xC) != 0)
    {
        if (yaw.delta != 0)
        {
            yawRate = 0;
        }

        s32 rate = yawRate;
        yaw.Push(seconds, &rate, 0);
    }

    if ((hits & 3) != 0)
    {
        if (pitch.delta != 0)
        {
            pitchRate = 0;
        }

        s32 rate = pitchRate;
        pitch.Push(seconds, &rate, 0);
    }

    if ((hits & 0x40) != 0)
    {
        if ((distance.bits & AngleBlender::BitEnabled) != 0)
        {
            distance.delta = distanceRate * seconds;
            distance.KeepWithin(distance.low, distance.high);
            distance.bits |= AngleBlender::BitPushed;
        }
    }
    else if ((hits & 0x30) != 0)
    {
        if (!(__builtin_fabsf(distance.delta) <= NoMove))
        {
            distanceRate = 0.0f;
        }

        if ((distance.bits & AngleBlender::BitEnabled) != 0)
        {
            distance.delta = distanceRate * seconds;
            distance.bits |= AngleBlender::BitPushed;
        }
    }

    return hits;
}

u32 FollowCameraPositioner::StepBlenders(TimeClock* clock)
{
    u32 moving = 0;
    u32 distanceMoving;
    distance.bits &= ~AngleBlender::BitBlendedGoal;
    if ((bits & BitAtPlace) == 0)
    {
        if ((state & StateTimed) != 0 && !(static_cast<s32>(clock->time - blendStart) < blendTicks))
        {
            EndBlend();
        }

        EaseBackOff();
        s32 pitchGoal;
        AngleFrom(&pitchGoal, 0.0f, AngleDegrees);
        if ((bits & BitTilts) != 0)
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
        u32 pitchMoving = pitch.Step(clock, &goal, 1, state >> 6 & 1);
        goal = yawGoal;
        u32 yawMoving = yaw.Step(clock, &goal, 0, state >> 6 & 1);
        moving = pitchMoving != 0 || yawMoving != 0;
        if ((bits & BitDistanceFollowsPitch) != 0)
        {
            distance.bits |= AngleBlender::BitBlendedGoal;
            distance.goalShare = pitch.share;
        }

        distanceMoving = distance.Step(DefaultDistance, clock, 1, state >> 6 & 1) != 0;
    }
    else
    {
        Vector4 way = target->point;
        f32 share = StepSeconds(clock);
        if ((state & StateBlendStarted) == 0)
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
                u32 curve = state >> CurveShift & 7;
                if (curve == 0)
                {
                    if ((state & StateAlongLine) != 0)
                    {
                        if ((state & StateCut) != 0)
                        {
                            share = 1.0f;
                            blendFrom = place;
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
                else if (curve == CurveCubic)
                {
                    f32 elapsed = static_cast<f32>(since) * g_SecondsPerClockUnit /
                                  (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit);
                    share = elapsed * (elapsed * elapsed);
                }
            }
        }

        if ((state & StateAlongLine) != 0)
        {
            Vector4 line;
            line.x = place.x - blendFrom.x;
            line.y = place.y - blendFrom.y;
            line.z = place.z - blendFrom.z;
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
            way.x = way.x - place.x;
            way.y = way.y - place.y;
            way.z = way.z - place.z;
        }

        f32 length = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
        s32 tilt;
        s32 turn;
        AnglesAlong(&way, &tilt, &turn);
        if ((state & StateCut) != 0)
        {
            pitch.current = tilt;
            yaw.current = turn;
            distance.current = length;
            s32 goal = fieldOfView.low;
            return 1 | (fieldOfView.Step(clock, &goal, 1, state >> 6 & 1) != 0);
        }

        if ((state & StateAlongLine) != 0)
        {
            pitch.current = tilt;
            yaw.current = turn;
            distance.current = length;
            AngleFrom(&pitch.delta, 0.0f, AngleRadians);
            AngleFrom(&yaw.delta, 0.0f, AngleRadians);
            distance.delta = 0.0f;
            s32 goal = fieldOfView.low;
            return fieldOfView.Step(clock, &goal, 1, state >> 6 & 1) != 0;
        }

        if ((pitch.bits & AngleBlender::BitEnabled) != 0)
        {
            pitch.delta = static_cast<s32>(static_cast<f32>(WrapAngle(tilt - pitch.current)) * share);
            s32 low = tilt;
            s32 high = tilt;
            pitch.KeepWithin(&low, &high);
            pitch.bits |= AngleBlender::BitPushed;
        }

        moving = pitch.bits >> 4 & 1;
        if ((yaw.bits & AngleBlender::BitEnabled) != 0)
        {
            yaw.delta = static_cast<s32>(static_cast<f32>(WrapAngle(turn - yaw.current)) * share);
            s32 low = turn;
            s32 high = turn;
            yaw.KeepWithin(&low, &high);
            yaw.bits |= AngleBlender::BitPushed;
        }

        moving |= yaw.bits >> 4 & 1;
        if ((distance.bits & AngleBlender::BitEnabled) != 0)
        {
            distance.delta = (length - distance.current) * share;
            distance.KeepWithin(length, length);
            distance.bits |= AngleBlender::BitPushed;
        }

        distanceMoving = distance.bits >> 4 & 1;
    }

    moving |= distanceMoving;
    s32 goal = fieldOfView.low;
    return moving | (fieldOfView.Step(clock, &goal, 1, state >> 6 & 1) != 0);
}

void FollowCameraPositioner::MeasureFacing()
{
    constexpr f32 Margin = Rounded(0.1);
    // Takes what's past the margin to the whole range (1 / 0.9)
    constexpr f32 Spread = 0x1.1c71c8p+0f;
    f32 cosine = 0.0f;
    f32 sine = 0.0f;
    if ((bits & (BitRestarted | BitTriggerBit22Clear | Bit19)) == 0)
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
    constexpr f32 MostRadius = 0.5f;
    Vector4 reached = *goal;
    u32 pushed = 0;
    if ((bits & BitPushedOutOfHull) != 0)
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
    InstanceRayHit query;
    MakeQuery(&query, results, instance);
    f32 share = 0.0f;
    Vector4 hit;
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
    GrowBox(Rounded(0.1), &box);
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
            if ((surface->collisionMask & CameraSurfaces) == 0)
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

    if (pushed != 0)
    {
        bits |= BitPushedOutOfHull;
    }
    else
    {
        bits &= ~BitPushedOutOfHull;
    }

    return pushed;
}

u32 FollowCameraPositioner::PushOffCollision(const Vector4* goal, const Vector4* point, Vector4* out)
{
    constexpr f32 Clearance = Rounded(0.01);
    u32 pushed = 0;
    Vector4 reached = *goal;
    if ((state & StateCut) == 0)
    {
        f32 distanceAlong;
        if (GetCollisionCheck(instance->chunk, &position, goal, CameraSurfaces, &distanceAlong, &reached, nullptr) != 0)
        {
            Vector4 away;
            away.x = position.x - reached.x;
            away.y = position.y - reached.y;
            away.z = position.z - reached.z;
            away.w = 1.0f;
            f32 inverse = InverseLength(&away, TinyLength);
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
    GrowBox(Rounded(0.1), &box);
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
        bits = (bits | BitPushedOffCollision) & ~BitFree;
        if (DistanceSquared(goal, point) - DistanceSquared(out, point) < NoMove)
        {
            bits |= BitPushedBack;
        }
        else
        {
            bits &= ~BitPushedBack;
        }
    }
    else
    {
        if ((bits & BitPushedOffCollision) != 0)
        {
            bits &= ~BitFree;
        }
        else
        {
            bits |= BitFree;
        }

        bits &= ~BitPushedOffCollision & ~BitPushedBack;
    }

    return pushed;
}

u32 FollowCameraPositioner::CastView(const Vector4* from, const Vector4* to, Vector4* hit, u32 instances, u32 lineOfSight)
{
    u32 mask = lineOfSight != 0 ? SightSurfaces : CameraSurfaces;
    if (instances == 0)
    {
        f32 distanceAlong;
        return GetCollisionCheck(instance->chunk, from, to, mask, &distanceAlong, hit, nullptr);
    }

    void* results[MostQueried];
    InstanceRayHit query;
    MakeQuery(&query, results, instance);
    if (ignored != nullptr)
    {
        query.skipped[1] = ignored;
    }

    f32 share = 0.0f;
    return SegmentHitsAnything(instance->chunk, from, to, mask, &query, EveryInstance, &share, hit, nullptr);
}

u32 FollowCameraPositioner::FindClearPlace(const Vector4* goal, const Vector4* from, Vector4* out, u32 instances)
{
    constexpr f32 Near = 4.0f;
    constexpr f32 ShortPull = Rounded(0.2);
    constexpr f32 Behind = -10.0f;
    f32 radius = collisionRadius;
    f32 longPull = (radius + 2.0f) + Rounded(0.1);
    f32 farSquared = radius * radius + Near;
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
        if (Near < lengthSquared)
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
        s32 x = attempt == 0 ? g_FollowCameraPitch : pitch.current;
        s32 y = Opposite(yaw.current);
        MatrixFromPitchYaw(&matrix, &x, &y);
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
        if (Near < lengthSquared)
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
        bits = (bits & ~BitPushedOffCollision & ~BitTargetStill) | BitFree;
        bits &= ~BitProbesOff & ~static_cast<u64>(BitTriggerRate);
    }

    return found;
}

u32 FollowCameraPositioner::ViewBlocked(const Vector4* point, const Vector4* other)
{
    constexpr f32 Near = 4.0f;
    u64 old = bits;
    if ((old & BitViewUnchecked) != 0)
    {
        return 0;
    }

    if ((old & BitPushedBack) != 0)
    {
        return 1;
    }

    if ((old & (BitPushedOffCollision | BitPushedOutOfHull)) != 0 && DistanceSquared(point, other) < Near)
    {
        return 1;
    }

    Vector4 from = target->objectPosition;
    from.y = from.y + 1.0f;
    Vector4 hit;
    return CastView(&from, point, &hit, (bits & BitPushedOutOfHull) != 0, 1);
}

void FollowCameraPositioner::FollowKeyed(Camera1C0E* keys)
{
    constexpr f32 Ease = Rounded(0.05);
    TimeClock* clock = &G_GameClockController->clocks[0];
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
        state |= StateCut;
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
    constexpr f32 Still = Rounded(1e-4);
    const Vector4* velocity = &target->velocity;
    f32 speed = __builtin_fabsf(velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z);
    u64 old = bits;
    if (!(speed <= Still) || (old & (Bit3 | BitTriggerBit11)) == Bit3)
    {
        bits = (old & ~BitTargetStill) | BitFree;
        bits &= ~BitProbesOff & ~static_cast<u64>(BitTriggerRate);
        return;
    }

    if ((old & (BitPushedOffCollision | BitIgnoresValues)) == BitPushedOffCollision)
    {
        triggerRate = 0.0f;
        bits = ((old | BitTargetStill) & ~BitFree) | BitProbesOff | BitTriggerRate;
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
    if ((bits & (BitStill | BitOnlyLooks)) != 0)
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
    pitch.bits = (pitch.bits | AngleBlender::BitEnabled) & ~AngleBlender::BitPushed;
    fieldOfView.bits = (fieldOfView.bits | AngleBlender::BitEnabled) & ~AngleBlender::BitPushed;
    yaw.bits = (yaw.bits | AngleBlender::BitEnabled) & ~AngleBlender::BitPushed;
    target = follower;
    probeHits = 0;
    pitch.delta = 0;
    fieldOfView.delta = 0;
    yaw.delta = 0;
    distance.delta = 0.0f;
    distance.bits = (distance.bits | AngleBlender::BitEnabled) & ~AngleBlender::BitPushed;
    if ((state & (StateCut | StatePlaced)) == (StateCut | StatePlaced))
    {
        state &= ~StateCut;
    }

    if ((state & (StateBlending | StateTimed)) != 0 && (state & StateBlendStarted) == 0)
    {
        blendStart = clock->time;
        blendFrom = position;
        state = (state | StateBlendStarted) & ~StateTimeEnded;
    }

    Vector4 point = target != nullptr ? target->point : Origin();
    u32 steering = (bits & (BitTriggerBit4 | BitIgnoresValues)) != 0;
    Vector4 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    hit.z = 0.0f;
    hit.w = 1.0f;
    u32 jumped = 0;
    if ((bits & BitBlockedLong) != 0)
    {
        Vector4 from = target->objectPosition;
        from.y = from.y + 1.0f;
        if (ViewBlocked(&position, &from) != 0 && FindClearPlace(&position, &target->point, &hit, 1) != 0)
        {
            jumped = 1;
            ClearBlocked();
            smoothed = 0;
            state = (state | StateCut) & ~StatePlaced;
        }
    }

    if ((bits & BitSteers) != 0 && steering != 0 && (state & StateCut) == 0)
    {
        s32 pitchRate;
        s32 yawRate;
        f32 distanceRate;
        u32 hits = ProbeRates(&point, &pitchRate, &yawRate, &distanceRate, 1);
        if (hits != 0)
        {
            f32 seconds = StepSeconds(clock);
            s32 rate = pitchRate;
            pitch.Push(seconds, &rate, 1);
            rate = yawRate;
            yaw.Push(seconds, &rate, 0);
            if ((distance.bits & AngleBlender::BitEnabled) != 0)
            {
                distance.delta = distanceRate * seconds;
                f32 toLow = distance.low;
                f32 toHigh = distance.high;
                if ((distance.bits & AngleBlender::BitSecondRange) != 0)
                {
                    toLow = distance.secondLow;
                    toHigh = distance.secondHigh;
                }

                distance.KeepWithin(toLow, toHigh);
                distance.bits |= AngleBlender::BitPushed;
            }
        }

        pitch.bits = (pitch.bits & ~AngleBlender::BitEnabled) | ((hits & 3) == 0 ? AngleBlender::BitEnabled : 0);
        yaw.bits = (yaw.bits & ~AngleBlender::BitEnabled) | ((hits & 0xC) == 0 ? AngleBlender::BitEnabled : 0);
        distance.bits = (distance.bits & ~AngleBlender::BitEnabled) | ((hits & 0x70) == 0 ? AngleBlender::BitEnabled : 0);
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

    state |= StatePlaced;
    fov = fieldOfView.current;
    bits &= ~static_cast<u64>(BitRestarted);
    CheckTargetStill();
    lastPosition = position;
}

void FollowCameraPositioner::Take(f32 value, CameraNode* node, CameraTarget* follower)
{
    if (node != nullptr && (node->camera->switches & MainCamera::SwitchResetsController) != 0)
    {
        camera.flags = 0;
        bits &= ~static_cast<u64>(BitOwnCamera);
        camera.flags = (camera.flags & ~MainCamera::FlagSteers) | MainCamera::FlagSteers;
        Clear();
    }

    if ((bits & BitOwnCamera) != 0)
    {
        Apply(value, &camera, follower);
        bits = (bits & ~static_cast<u64>(BitHadProbesFromTarget)) | (bits >> 2 & 1) << 4;
        trigger = node;
        return;
    }

    if (node != nullptr)
    {
        bits = (bits & ~static_cast<u64>(BitTriggerBit11)) | static_cast<u64>(node->camera->flags >> 11 & 1) << 5;
        u32 switched = 0;
        if ((bits & BitTriggerBit11) != 0)
        {
            bits &= ~static_cast<u64>(BitIgnoresValues);
        }
        else if ((bits & BitIgnoresValues) != 0)
        {
            if ((bits & (Bit6 | Bit3)) == Bit6)
            {
                state |= StateBlendAsked;
                bits &= ~static_cast<u64>(BitIgnoresValues);
                switched = 1;
            }
        }
        else if ((bits & (BitIgnoresValues | Bit3)) == Bit3)
        {
            state &= ~StateBlendAsked;
            bits |= BitIgnoresValues;
            switched = 1;
        }

        if (switched != 0)
        {
            Clear();
        }
    }

    if (node != trigger)
    {
        Clear();
        if (node == nullptr)
        {
            BlendBack(trigger, 1);
        }
        else
        {
            MainCamera* taken = node->camera;
            u32 cut = 0;
            if ((taken->flags >> 5 & 1) != 0 || (bits & BitRestarted) != 0)
            {
                cut = 1;
            }

            state = (state & ~StateCut) | cut << 6;
            if (trigger != nullptr)
            {
                u32 same = 0;
                if ((trigger->camera->flags >> 24 & 1) != 0)
                {
                    same = taken->flags >> 24 & 1;
                }

                state = (state & ~StateCut) | ((state >> 6 & 1) | same) << 6;
            }

            state &= ~StatePlaced;
            CameraSubtype* second = taken->second;
            if (second != nullptr)
            {
                second->TakeLastVirtual(&position, &target->point);
                if (second->TypeVirtual() == CameraSubtype::Type1C0E)
                {
                    keyed = 1;
                }
            }
        }
    }

    if (node != nullptr)
    {
        Apply(value, node->camera, follower);
    }

    trigger = node;
    bits = (bits & ~static_cast<u64>(BitHadProbesFromTarget)) | (bits >> 2 & 1) << 4;
}

void FollowCameraPositioner::Apply(f32 value, MainCamera* taken, CameraTarget* follower)
{
    CameraSubtype* second = taken->second;
    CameraSubtype* first = taken->first;
    if (keyed != 0)
    {
        FollowKeyed(reinterpret_cast<Camera1C0E*>(second));
        return;
    }

    u32 flags = taken->flags;
    u32 along = 0;
    if ((flags >> 10 & 1) != 0)
    {
        along = follower != nullptr;
    }

    bits = (bits & ~static_cast<u64>(BitStill)) | static_cast<u64>((bits >> 12 & 1) | (flags >> 21 & 1)) << 12;
    bits = (bits & ~static_cast<u64>(BitOnlyLooks)) | static_cast<u64>((bits >> 14 & 1) | (flags >> 19 & 1)) << 14;
    bits = (bits & ~static_cast<u64>(BitKeepsHeight)) | static_cast<u64>((bits >> 13 & 1) | (flags >> 20 & 1)) << 13;
    state = (state & ~StateAlongLine) | (flags >> 29 & 1) << 11;
    bits = (bits & ~static_cast<u64>(BitViewUnchecked)) | static_cast<u64>(flags >> 30 & 1) << 20;
    bits = (bits & ~static_cast<u64>(BitYawExtra)) | static_cast<u64>(flags >> 31 & 1) << 21;
    u32 blendIn = !(flags >> 5 & 1);
    if (trigger != nullptr && (flags >> 24 & 1) != 0 && (trigger->camera->flags >> 24 & 1) != 0)
    {
        blendIn = 0;
    }

    u32 timed = (state & StateBlendAsked) != 0 ? 1 : blendIn;
    if (timed != 0 && (state & StateBlending) == 0)
    {
        f32 seconds = (state & StateBlendAsked) != 0 ? AskedBlendSeconds : taken->blendTime;
        blendTicks = static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    }

    s32 currentYaw = yaw.current;
    u32 blendInValues = taken->NearerBlendIn(&currentYaw);
    // Only the blend's start sets the blenders' speeds for its time
    u32 canTime = timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0;
    if ((flags >> 7 & 1) != 0)
    {
        s32 start = static_cast<s32>(taken->fovStart);
        s32 end = static_cast<s32>(taken->fovEnd);
        if (along != 0)
        {
            fieldOfView.bits |= AngleBlender::BitSecondRange;
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

            fieldOfView.bits |= AngleBlender::BitSecondRange;
            fieldOfView.secondLow = start;
            fieldOfView.secondHigh = end;
        }
    }
    else
    {
        fieldOfView.bits &= ~AngleBlender::BitSecondRange;
    }

    if ((flags >> 3 & 1) != 0)
    {
        f32 low = taken->distanceStart;
        f32 high = taken->distanceEnd;
        if (blendInValues != 0 && (flags >> 18 & 1) != 0)
        {
            high = taken->blendInDistance;
            low = high;
        }

        if (along != 0)
        {
            f32 share = follower->along;
            distance.bits |= AngleBlender::BitSecondRange;
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

            distance.bits |= AngleBlender::BitSecondRange;
            distance.secondHigh = high;
            distance.secondLow = low;
        }
    }
    else
    {
        distance.bits &= ~AngleBlender::BitSecondRange;
    }

    u64 old = bits;
    bits = (old & ~static_cast<u64>(BitTriggerBit22Clear)) | static_cast<u64>(!(flags >> 22 & 1)) << 16;
    u64 cleared = bits & ~BitProbesOff;
    bits = cleared | static_cast<u64>(flags >> 23 & 1) << 32;
    if ((bits & (BitProbesOff | BitIgnoresValues)) == (BitProbesOff | BitIgnoresValues))
    {
        bits = cleared | static_cast<u64>(!(taken->switches >> 1 & 1)) << 32;
    }

    if ((bits & BitIgnoresValues) == 0)
    {
        bits = (bits & ~BitTriggerBit4) | static_cast<u64>(flags >> 4 & 1) << 47;
        bits = (bits & ~static_cast<u64>(BitProbesFromTarget)) | static_cast<u64>(first != nullptr) << 2;
        if (second != nullptr)
        {
            if ((flags & MainCamera::FlagSecondAtParameter) != 0)
            {
                second->AtParameter(value, &place);
            }
            else if (follower != nullptr)
            {
                second->At(first == nullptr ? &follower->point : &follower->objectPosition, follower, &place);
            }

            state &= ~StateBlendAsked;
        }

        if ((flags >> 2 & 1) != 0)
        {
            s32 start = static_cast<s32>(taken->pitchStart);
            s32 end = static_cast<s32>(taken->pitchEnd);
            if (blendInValues != 0 && (flags >> 17 & 1) != 0)
            {
                end = static_cast<s32>(taken->blendInPitch);
                start = end;
            }

            if (along != 0)
            {
                pitch.bits |= AngleBlender::BitSecondRange;
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

                pitch.bits |= AngleBlender::BitSecondRange;
                pitch.secondLow = start;
                pitch.secondHigh = end;
            }
        }
        else
        {
            pitch.bits &= ~AngleBlender::BitSecondRange;
        }

        if ((flags >> 6 & 1) != 0)
        {
            s32 start = static_cast<s32>(taken->yawStart);
            s32 end = static_cast<s32>(taken->yawEnd);
            if (blendInValues != 0 && (flags >> 16 & 1) != 0)
            {
                end = static_cast<s32>(taken->blendInYaw);
                start = end;
            }

            if (along != 0)
            {
                yaw.bits |= AngleBlender::BitSecondRange;
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

                yaw.bits |= AngleBlender::BitSecondRange;
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
            yaw.bits &= ~AngleBlender::BitSecondRange;
        }

        if ((flags >> 15 & 1) != 0)
        {
            yaw.bits |= AngleBlender::BitSineSpeed;
            yaw.speed = static_cast<s32>(taken->yawExtra);
            bits |= BitYawExtraSpeed;
        }

        if ((flags >> 12 & 1) != 0)
        {
            triggerRate = taken->secondValue;
            bits |= BitTriggerRate;
        }

        if (second != nullptr || keepsRate != 0)
        {
            bits |= BitViewUnchecked;
        }
    }

    if (second != nullptr && (bits & BitIgnoresValues) == 0)
    {
        bits |= BitAtPlace;
        state = (state & ~StateBlending) | (timed & 1);
    }

    if (timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
    {
        state |= StateTimed;
    }
}

void FollowCameraPositioner::BlendBack(CameraNode* last, u32 time)
{
    MainCamera* taken = last->camera;
    CameraSubtype* second = taken->second;
    u32 timed = (state & StateBlendAsked) != 0 ? 1 : !(taken->flags >> 5 & 1);
    if (timed != 0 && (state & StateBlending) == 0)
    {
        f32 seconds = (state & StateBlendAsked) != 0 ? AskedBlendSeconds : taken->blendTime;
        blendTicks = static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    }

    if ((taken->flags >> 7 & 1) != 0)
    {
        s32 goal = g_DefaultFov;
        if (timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
        {
            fieldOfView.TimeToward(&blendTicks, &goal, &goal);
        }
    }

    if ((taken->flags >> 2 & 1) != 0 || second != nullptr)
    {
        s32 goal;
        AngleFrom(&goal, 0.0f, AngleDegrees);
        if ((bits & BitTilts) != 0)
        {
            goal = g_FollowCameraPitch;
        }

        if (timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
        {
            pitch.TimeToward(&blendTicks, &goal, &goal);
        }
    }

    if (((taken->flags >> 3 & 1) != 0 || second != nullptr) && timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
    {
        distance.previousSpeed = distance.speed;
        distance.speed = __builtin_fabsf((DefaultDistance - distance.current) /
                                         (static_cast<f32>(blendTicks) * g_SecondsPerClockUnit));
    }

    if (((taken->flags >> 6 & 1) != 0 || second != nullptr) && timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
    {
        // 10 radians, as retail has it
        s32 low;
        AngleFrom(&low, 10.0f, AngleRadians);
        s32 high;
        AngleFrom(&high, 10.0f, AngleRadians);
        yaw.TimeToward(&blendTicks, &low, &high);
    }

    if (time != 0 && timed != 0 && (state & (StateTimed | StateTimeEnded)) == 0)
    {
        state |= StateTimed;
    }
}

u32 FollowCameraPositioner::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &place, 1);
    u32 moved = 0;
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) != 0)
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
    constexpr f32 Fast = 3.0f;
    if ((bits & BitTilts) == 0)
    {
        return;
    }

    const Vector4* velocity = &target->velocity;
    f32 speed = __builtin_sqrtf(velocity->x * velocity->x + velocity->z * velocity->z);
    f32 pitchScale = Fast < speed ? Rounded(0.2) : Rounded(0.05);
    s32 change;
    AngleFrom(&change, -facing * pitchScale - static_cast<f32>(tiltPitch) * AngleToRadians, AngleRadians);
    s32 rate = g_CameraTiltRate;
    s32* scaled = MultiplyAngle(&rate, stepSeconds);
    change = static_cast<s32>(static_cast<f32>(change) * (static_cast<f32>(*scaled) * AngleToRadians));
    tiltPitch = tiltPitch + change;
    f32 yawScale = Fast < speed ? 0.25f : Rounded(0.12);
    AngleFrom(&change, sideways * yawScale - static_cast<f32>(tiltYaw) * AngleToRadians, AngleRadians);
    rate = g_CameraTiltRate;
    scaled = MultiplyAngle(&rate, stepSeconds);
    change = static_cast<s32>(static_cast<f32>(change) * (static_cast<f32>(*scaled) * AngleToRadians));
    tiltYaw = tiltYaw + change;
    s32 x;
    AngleFrom(&x, 0.0f, AngleRadians);
    s32 z;
    AngleFrom(&z, 0.0f, AngleRadians);
    s32 y = tiltYaw;
    Vector4 turn;
    GetRotationXYZ(&turn, &x, &y, &z);
    Vector4 product;
    MultiplyRotations(&product, &turn, turned);
    *turned = product;
    f32 inverse = InverseLength4(0.0f, Rounded(1e-10), turned);
    turned->x = turned->x * inverse;
    turned->y = turned->y * inverse;
    turned->z = turned->z * inverse;
    turned->w = turned->w * inverse;
    x = tiltPitch;
    AngleFrom(&y, 0.0f, AngleRadians);
    AngleFrom(&z, 0.0f, AngleRadians);
    GetRotationXYZ(&turn, &x, &y, &z);
    MultiplyRotations(&product, turned, &turn);
    *turned = product;
    inverse = InverseLength4(0.0f, Rounded(1e-10), turned);
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
    camera.Destroy(2);
    RemoveReference(&followed);
    vtable = g_CameraTargetVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void FollowCameraTarget::Clear()
{
    bits &= ~(BitHasCamera | BitBlends | BitBlending | BitGivenBox | BitFramesInstances | BitPointFollows | BitBoxBlending |
              BitCut);
    bits |= BitStepped | BitSettled;
    bits &= ~(BitBlendsNear | BitOffsetUnturned);
    smoothed = 1;
}

void FollowCameraTarget::ClearBox()
{
    bits &= ~BitGivenBox;
}

void FollowCameraTarget::SetBox(const Vector4* min, const Vector4* max)
{
    givenMin = *min;
    bits |= BitGivenBox;
    givenMax = *max;
}

void FollowCameraTarget::Reset()
{
    bits = 0;
    offset = boxMin;
    fixedPlace = nullptr;
    trigger = nullptr;
    boxShare = 0.5f;
    offset.x = offset.x + boxMax.x;
    offset.y = offset.y + boxMax.y;
    offset.z = offset.z + boxMax.z;
    offset.x = offset.x * 0.5f;
    offset.y = offset.y * 0.5f;
    offset.z = offset.z * 0.5f;
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
        height = 0.0f;
    }
    else
    {
        ObjectPlace* place = object->place;
        RotateAndTranslate(place);
        ObjectPoint(place, &point);
        GetRotationVec(&rotation, &place->matrix);
        objectPosition = point;
        objectRotation = rotation;
        height = place->matrix.m[3][1];
        EaseHeight(0.0f, &point, &easedPoint);
        Vector4 turned;
        if (OffsetUnturned(bits) != 0)
        {
            turned = offset;
        }
        else
        {
            RotateByQuaternion(&rotation, &offset, &turned, 0);
        }

        easedPoint.x = easedPoint.x + turned.x;
        easedPoint.y = easedPoint.y + turned.y;
        easedPoint.z = easedPoint.z + turned.z;
    }

    bits &= ~BitPointFollows;
    lastTrigger = nullptr;
    groundHeight = height;
    currentMin = Origin();
    currentMax = Origin();
    bits |= BitReset;
    cameraPoint = Origin();
    Clear();
    camera.flags = 0;
    bits &= ~BitOwnCamera;
    camera.flags = (camera.flags & ~MainCamera::FlagSteers) | MainCamera::FlagSteers;
    Clear();
}

void FollowCameraTarget::ObjectPoint(ObjectPlace* place, Vector4* out)
{
    const Vector4* position = RowOf(&place->matrix, 3);
    CameraNode* node = trigger;
    if (node == nullptr || (bits & BitFramesInstances) == 0 || node->instanceCount == 0)
    {
        *out = *position;
        return;
    }

    u32 count = node->instanceCount;
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
    f32 x = (sum.x * inverse - position->x) * pull;
    f32 y = (sum.y * inverse - position->y) * pull;
    f32 z = (sum.z * inverse - position->z) * pull;
    f32 length = __builtin_sqrtf(x * x + y * y + z * z);
    along = length;
    if (most < length)
    {
        f32 scale = most / length;
        along = 1.0f;
        z = z * scale;
        x = x * scale;
        y = y * scale;
    }
    else
    {
        along = length / most;
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
    f32 eased = height;
    if (eased < ground)
    {
        f32 level = followedPoint->y - offset.y;
        f32 raised = seconds * RiseRate + eased;
        if (eased < level && NoMove < offset.y)
        {
            height = level;
        }
        else if (raised < groundHeight)
        {
            height = raised;
        }
        else
        {
            height = groundHeight;
        }
    }
    else
    {
        f32 y = followedPoint->y;
        f32 top = y + Margin;
        if (top < ground)
        {
            height = ground;
        }
        else
        {
            f32 level = top - offset.y;
            if (eased < level)
            {
                height = offset.y < NoMove ? y : level;
            }
            else if (top < eased)
            {
                height = top;
            }
        }
    }

    out->x = followedPoint->x;
    out->w = 1.0f;
    out->y = height;
    out->z = followedPoint->z;
}

void FollowCameraTarget::Step(TimeClock* clock)
{
    constexpr f32 Near = 15.0f;
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
    if ((bits & (BitCut | BitStepped)) == (BitCut | BitStepped))
    {
        smoothed = 1;
        bits &= ~BitCut;
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
    if ((bits & (BitSettled | BitBoxBlending | BitBlending | BitBlends)) == BitBlends)
    {
        blendStart = clock->time;
        bits |= BitBlending;
        if ((bits & BitBlendsNear) == 0 && DistanceSquared(&cameraPoint, &old) < Near)
        {
            bits &= ~BitBlending;
        }

        u32 differs;
        if ((bits & BitGivenBox) != 0)
        {
            differs = SamePoint(&currentMin, &givenMin) == 0 || SamePoint(&currentMax, &givenMax) == 0;
        }
        else
        {
            differs = SamePoint(&currentMin, &boxMin) == 0 || SamePoint(&currentMax, &boxMax) == 0;
        }

        bits |= differs != 0 ? BitBoxBlending : BitSettled;
    }

    if ((bits & BitGivenBox) != 0)
    {
        currentMin = givenMin;
        currentMax = givenMax;
    }
    else
    {
        currentMin = boxMin;
        currentMax = boxMax;
    }

    if ((bits & BitBoxBlending) != 0)
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
            bits = (bits | BitSettled) & ~BitBoxBlending;
        }
    }

    offset.x = currentMin.x + (currentMax.x - currentMin.x) * boxShare;
    offset.y = currentMin.y + (currentMax.y - currentMin.y) * boxShare;
    offset.w = 1.0f;
    offset.z = currentMin.z + (currentMax.z - currentMin.z) * boxShare;
    if ((bits & BitUneased) == 0)
    {
        EaseHeight(seconds, &point, &easedPoint);
    }
    else
    {
        easedPoint = point;
    }

    Vector4 turned;
    if (OffsetUnturned(bits) != 0)
    {
        turned = offset;
    }
    else
    {
        RotateByQuaternion(&rotation, &offset, &turned, 0);
    }

    easedPoint.x = easedPoint.x + turned.x;
    easedPoint.y = easedPoint.y + turned.y;
    easedPoint.z = easedPoint.z + turned.z;
    if ((bits & BitPointFollows) != 0)
    {
        cameraPoint = easedPoint;
    }

    if ((bits & BitBlending) == 0)
    {
        point = (bits & BitHasCamera) != 0 ? cameraPoint : easedPoint;
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
            if ((bits & CurveMask) == CurveCubic)
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

    bits |= BitStepped;
}

f32 FollowCameraTarget::TakeCamera(MainCamera* taken)
{
    f32 value = 0.0f;
    u32 flags = taken->flags;
    CameraSubtype* first = taken->first;
    bits = (bits & ~BitGivenBox) | (flags >> 8 & 1) << 3;
    bits = (bits & ~BitFramesInstances) | (flags >> 9 & 1) << 4;
    bits = (bits & ~BitOffsetUnturned) | (flags >> 28 & 1) << 18;
    u32 blendIn = !(flags >> 5 & 1);
    if (first != nullptr)
    {
        value = first->At(&easedPoint, this, &cameraPoint);
    }
    else
    {
        bits |= BitPointFollows;
    }

    if ((bits & BitGivenBox) != 0)
    {
        givenMin = taken->leftoverVector1;
        givenMax = taken->leftoverVector2;
    }

    if (blendIn != 0 && (bits & BitBlends) == 0)
    {
        blendTicks = static_cast<s32>(taken->blendTime * g_ClockUnitsPerSecond);
        blendFrom = point;
        startMin = currentMin;
        startMax = currentMax;
        bits = (bits & ~BitBlendsNear) | (taken->flags >> 25 & 1) << 17;
        bits &= ~BitSettled;
    }

    bits = ((bits | BitHasCamera) & ~BitBlends) | blendIn << 1;
    if ((bits & BitFramesInstances) != 0)
    {
        pull = taken->leftoverFloat2;
        most = taken->leftoverFloat1;
    }

    return value;
}

f32 FollowCameraTarget::Value(CameraNode* node)
{
    f32 value = 0.0f;
    if (node != nullptr && (node->camera->switches & MainCamera::SwitchResetsController) != 0)
    {
        camera.flags = 0;
        bits &= ~BitOwnCamera;
        camera.flags = (camera.flags & ~MainCamera::FlagSteers) | MainCamera::FlagSteers;
        Clear();
    }

    if ((bits & BitOwnCamera) != 0)
    {
        TakeCamera(&camera);
    }
    else
    {
        // The trigger before the last: a change of trigger is taken on two steps in a row
        CameraNode* last = lastTrigger;
        if (last != node)
        {
            Clear();
            if (last != nullptr && node == nullptr)
            {
                MainCamera* previous = last->camera;
                bits |= BitPointFollows;
                u32 blendIn = !(previous->flags >> 5 & 1);
                if (blendIn != 0 && (bits & BitBlends) == 0)
                {
                    blendTicks = static_cast<s32>(previous->blendTime * g_ClockUnitsPerSecond);
                    blendFrom = point;
                    startMin = currentMin;
                    startMax = currentMax;
                    bits = (bits & ~BitBlendsNear) | (previous->flags >> 25 & 1) << 17;
                    bits &= ~BitSettled;
                }

                bits = ((bits | BitHasCamera) & ~BitBlends) | blendIn << 1;
                bits = (bits & ~BitLastOffsetUnturned) | (previous->flags >> 28 & 1) << 19;
            }
            else
            {
                u32 cut = 0;
                if ((node->camera->flags >> 5 & 1) != 0 || (bits & BitReset) != 0)
                {
                    cut = 1;
                }

                bits = (bits & ~BitCut) | cut << 13;
                bits &= ~BitStepped;
                smoothed = !(bits >> 13 & 1);
                if (lastTrigger != nullptr)
                {
                    bits = (bits & ~BitLastOffsetUnturned) | (lastTrigger->camera->flags >> 28 & 1) << 19;
                }
                else
                {
                    bits &= ~BitLastOffsetUnturned;
                }
            }
        }

        if (node != nullptr)
        {
            value = TakeCamera(node->camera);
        }
    }

    bits &= ~BitReset;
    lastTrigger = trigger;
    trigger = node;
    return value;
}

u32 FollowCameraTarget::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    Vector4 ground;
    ground.y = groundHeight;
    ground.x = 0.0f;
    ground.z = 0.0f;
    ground.w = 1.0f;
    Vector4 eased;
    eased.y = height;
    eased.x = 0.0f;
    eased.z = 0.0f;
    eased.w = 1.0f;
    TransformVectorThroughLink(link, &ground, 1);
    TransformVectorThroughLink(link, &eased, 1);
    groundHeight = ground.y;
    height = eased.y;
    TransformVectorThroughLink(link, &cameraPoint, 1);
    TransformRotationThroughLink(link, &rotation);
    TransformVectorThroughLink(link, &point, 1);
    return 1;
}
