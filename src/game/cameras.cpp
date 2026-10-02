#include "game/cameras.h"

#include "game/agentlab.h"
#include "game/camerarig.h"
#include "game/clock.h"
#include "game/events.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/properties.h"
#include "game/stream.h"

EABI_EXPORT(CameraPointAtParameter, &CameraPoint::AtParameter);
EABI_EXPORT(CameraLineAtParameter, &CameraLine::AtParameter);
EABI_EXPORT(CameraPathAtParameter, &CameraPath::AtParameter);
EABI_EXPORT(FUN_0027cef0, &BossCamera::AtParameter);
EABI_EXPORT(BossCameraLimitTurn, &BossCamera::LimitTurn);
EABI_EXPORT(CameraSplineAtParameter, &CameraSplineCamera::AtParameter);
EABI_EXPORT(FUN_00279f80, &CameraSplineCamera::OffsetAt);
EABI_EXPORT(FUN_00279d78, &CameraSplineCamera::PointBetween);
EABI_EXPORT(FUN_0027a248, &Camera1C09::AtParameter);
EABI_EXPORT(FUN_0027d880, &CameraPoint2::AtParameter);
EABI_EXPORT(FUN_0027da20, &Camera1C0C::AtParameter);
EABI_EXPORT(CameraLine2AtParameter, &CameraLine2::AtParameter);
EABI_EXPORT(FUN_0027dbf8, &Camera1C0E::AtParameter);
EABI_EXPORT(FUN_0027ddb0, &CameraZone::AtParameter);
EABI_EXPORT(FUN_0027e148, EaseInOut);
EABI_EXPORT(FUN_0027a390, RotationAt);
EABI_IMPORT(FUN_00189440, PathPointAt);
EABI_IMPORT(FUN_0018f0e8, SplineSegmentAt);
EABI_IMPORT(FUN_0018a7a8, SplinePointIn);
EABI_IMPORT(BoxPointAtFractions, BoxPointAtFractions);

namespace
{
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;

// The main camera's angles are tagged values' (65536ths of a turn)
void ReadAngle(u32* angle, Stream* stream)
{
    reinterpret_cast<TaggedValue*>(angle)->Read(stream);
}

void ReadVector(Stream* stream, Vector4* vector)
{
    stream->Read(vector, sizeof(Vector4), 1);
}

// What every subtype's read starts with
void ReadBase(CameraSubtype* camera, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&camera->flags));
    stream->ReadF32(&camera->rate);
    stream->ReadF32(&camera->offset);
}

// The base's destructor's part every subtype's has
void DestroyBase(CameraSubtype* camera, u32 destroyFlags)
{
    camera->vtable = g_CameraSubtypeVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(camera);
    }
}

f32 Clamped(f32 along)
{
    if (1.0f < along)
    {
        return 1.0f;
    }

    if (along < 0.0f)
    {
        return 0.0f;
    }

    return along;
}

// The keys' array freed (its elements have nothing to destroy), and the keys emptied
void FreeKeys(CameraRotationKeys* keys)
{
    if (keys->keys != nullptr)
    {
        DeleteArray(keys->keys);
    }

    keys->capacity = 0;
    keys->keys = nullptr;
    keys->count = 0;
}

// Where along its samples a parameter is: the sample before it (a floor of the parameter times the count)
s32 SampleBefore(const CameraSpline* spline, f32 along)
{
    f32 at = along * static_cast<f32>(spline->count);
    s32 sample = TruncateToInt(at);
    if (at < static_cast<f32>(sample))
    {
        sample--;
    }

    return sample;
}

f32 SampleShare(u32 sample, const CameraSpline* spline)
{
    return static_cast<f32>(sample) / static_cast<f32>(spline->count * 2 + 2);
}
}

void ReadLine(Vector4* line, Stream* stream)
{
    stream->Read(&line[0], sizeof(Vector4), 1);
    stream->Read(&line[1], sizeof(Vector4), 1);
}

CameraSubtype* CameraSubtype::Construct(CameraSubtype* camera)
{
    camera->flags = 1;
    camera->vtable = g_CameraSubtypeVTable;
    camera->offset = 0.0f;
    return camera;
}

void CameraSubtype::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

void CameraSubtype::Nothing()
{
}

void CameraSubtype::Read(Stream* stream)
{
    ReadBase(this, stream);
}

CameraPoint* CameraPoint::Construct(CameraPoint* camera)
{
    camera->flags = 1;
    camera->vtable = g_CameraPointVTable;
    camera->offset = 0.0f;
    return camera;
}

void CameraPoint::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

f32 CameraPoint::At(const Vector4*, CameraTarget*, Vector4* out)
{
    *out = point;
    return 1.0f;
}

void CameraPoint::AtParameter(f32, Vector4* out)
{
    *out = point;
}

u32 CameraPoint::Type()
{
    return TypePoint;
}

void CameraPoint::Read(Stream* stream)
{
    ReadBase(this, stream);
    ReadVector(stream, &point);
}

CameraLine* CameraLine::Construct(CameraLine* camera)
{
    camera->flags = 1;
    camera->vtable = g_CameraLineVTable;
    camera->offset = 0.0f;
    return camera;
}

void CameraLine::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

f32 CameraLine::At(const Vector4* target, CameraTarget*, Vector4* out)
{
    f32 along = NearestLineParameter(&start, target);
    CameraSubtype::AtParameter(along, out);
    return along;
}

void CameraLine::AtParameter(f32 along, Vector4* out)
{
    f32 x = start.x - end.x;
    f32 y = start.y - end.y;
    f32 z = start.z - end.z;
    f32 length = Kept(__builtin_sqrtf(x * x + y * y + z * z));
    LinePointAt(Clamped(along + offset / length), &start, out);
}

u32 CameraLine::Type()
{
    return TypeLine;
}

void CameraLine::Read(Stream* stream)
{
    ReadBase(this, stream);
    ReadLine(&start, stream);
}

CameraPath* CameraPath::Construct(CameraPath* camera)
{
    camera->flags = 1;
    camera->vtable = g_CameraPathVTable;
    camera->offset = 0.0f;
    // (Its point count is what the memory had)
    camera->path.vtable = g_LayoutPathVTable;
    camera->path.lengths = nullptr;
    camera->path.points = nullptr;
    camera->path.unknown48 = -1;
    return camera;
}

void CameraPath::Destroy(u32 destroyFlags)
{
    constexpr u32 DestroyOnly = 2;
    vtable = g_CameraPathVTable;
    path.Destroy(DestroyOnly);
    DestroyBase(this, destroyFlags);
}

f32 CameraPath::At(const Vector4* target, CameraTarget*, Vector4* out)
{
    CurveSearch search;
    search.segment = -1;
    PathNearestSearch(&path, target, &search);
    f32 along = PathNearestParameter(&path, &search);
    CameraSubtype::AtParameter(along, out);
    return along;
}

void CameraPath::AtParameter(f32 along, Vector4* out)
{
    f32 length = path.lengths[path.count - 4];
    PathPointAt(Clamped(along + offset / length), &path, out);
}

u32 CameraPath::Type()
{
    return TypePath;
}

void CameraPath::Read(Stream* stream)
{
    constexpr u32 ReadSlot = 3;
    ReadBase(this, stream);
    CallVirtual<void>(&path, path.vtable, ReadSlot, stream);
}

void BossCamera::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

f32 BossCamera::At(const Vector4* target, CameraTarget* follower, Vector4* out)
{
    Vector4 place = follower->objectPosition;
    VuTransformPoint(&worldToArena, &place, &place);
    Vector4 inArena = place;
    if (!(0.0f < place.y) || !(place.y < orbit.x))
    {
        *out = *target;
        return 0.0f;
    }

    out->y = place.y;
    if (curves != 0)
    {
        out->y = out->y + HeightAt(&inArena);
    }
    else
    {
        out->y = place.y + orbit.z;
    }

    place.y = 0.0f;
    f32 inverse = InverseLength(&place, LengthEpsilon);
    place.x = place.x * inverse;
    place.y = place.y * inverse;
    place.z = place.z * inverse;
    f32 radius = curves != 0 ? RadiusAt(&inArena) : orbit.y;
    out->x = place.x * radius;
    out->z = place.z * radius;
    out->w = 1.0f;
    LimitTurn(radius, out);
    VuTransformPoint(&arenaToWorld, out, out);
    return 0.0f;
}

void BossCamera::AtParameter(f32, Vector4*)
{
}

void BossCamera::TakeLast(const Vector4* point)
{
    last = *point;
    VuTransformPoint(&worldToArena, &last, &last);
}

u32 BossCamera::Type()
{
    return TypeBoss;
}

void BossCamera::Read(Stream* stream)
{
    ReadBase(this, stream);
    stream->Read(&worldToArena, sizeof(Matrix4x4), 1);
    stream->Read(&arenaToWorld, sizeof(Matrix4x4), 1);
    ReadVector(stream, &orbit);
    stream->ReadBool(reinterpret_cast<bool*>(&curves));
    stream->ReadF32(&radiusShare);
    stream->ReadF32(&middleHeight);
    stream->ReadF32(&edgeHeight);
    stream->ReadF32(&turnLimit);
    stream->ReadBool(reinterpret_cast<bool*>(&curveAllAxes));
}

f32 BossCamera::RadiusAt(const Vector4* place)
{
    f32 squared;
    if (curveAllAxes != 0)
    {
        squared = place->x * place->x + place->y * place->y + place->z * place->z;
    }
    else
    {
        squared = place->x * place->x + place->z * place->z;
    }

    f32 distance = __builtin_sqrtf(squared);
    return (orbit.y - distance) * radiusShare + distance;
}

f32 BossCamera::HeightAt(const Vector4* place)
{
    f32 distance = __builtin_sqrtf(place->x * place->x + place->y * place->y + place->z * place->z);
    return (middleHeight - edgeHeight) * ((orbit.y - distance) / orbit.y) + edgeHeight;
}

void BossCamera::LimitTurn(f32 radius, Vector4* place)
{
    constexpr f32 NoLimit = Rounded(5e-5);
    constexpr f32 AnglePerRadian = 0x1.45f306p+13f;
    f32 limit = turnLimit;
    if (limit < -NoLimit || NoLimit < limit)
    {
        f32 lengths = __builtin_sqrtf(place->x * place->x + place->z * place->z) * __builtin_sqrtf(last.x * last.x + last.z * last.z);
        f32 turn = turnLimit / radius;
        f32 cosine = (place->x * last.x + place->z * last.z) / lengths;
        s32 angle;
        AngleOfCosine(cosine, &angle);
        if (static_cast<s32>(turn * AnglePerRadian) < angle)
        {
            // Turned the limit either way from the last place: the nearer one
            f32 step = turnLimit / radius;
            Matrix4x4 turning;
            InitIdentityMatrix(&turning);
            s32 by;
            AngleFrom(&by, step, AngleRadians);
            MatrixAboutY(&turning, &by);
            Vector4 one;
            TurnAboutY(&turning, &last, &one);
            f32 x = place->x - one.x;
            f32 y = place->y - one.y;
            f32 z = place->z - one.z;
            f32 toOne = __builtin_sqrtf(x * x + y * y + z * z);
            InitIdentityMatrix(&turning);
            AngleFrom(&by, -step, AngleRadians);
            MatrixAboutY(&turning, &by);
            Vector4 other;
            TurnAboutY(&turning, &last, &other);
            x = place->x - other.x;
            y = place->y - other.y;
            z = place->z - other.z;
            f32 toOther = __builtin_sqrtf(x * x + y * y + z * z);
            *place = toOne < toOther ? one : other;
        }
    }

    last = *place;
}

void CameraSplineCamera::Destroy(u32 destroyFlags)
{
    constexpr u32 DestroyAndFree = 3;
    vtable = g_CameraSplineCameraVTable;
    if (spline != nullptr)
    {
        CallVirtual<void>(spline, spline->vtable, 1, DestroyAndFree);
    }

    DestroyBase(this, destroyFlags);
}

f32 CameraSplineCamera::At(const Vector4* target, CameraTarget*, Vector4* out)
{
    CurveSearch search;
    search.segment = -1;
    SplineNearestSearch(spline, target, &search);
    f32 along = SplineNearestParameter(spline, &search);
    CameraSubtype::AtParameter(along, out);
    f32 ahead = OffsetAt(along);
    f32 length = spline->lengths[spline->count - 1];
    PointBetween(Clamped(along + ahead / length), target, out);
    return along;
}

void CameraSplineCamera::AtParameter(f32 along, Vector4* out)
{
    f32 ahead = OffsetAt(along);
    f32 length = spline->lengths[spline->count - 1];
    f32 at = Clamped(along + ahead / length);
    CameraSpline* followed = spline;
    f32 into;
    s32 segment = SplineSegmentAt(at * (static_cast<f32>(followed->count) * followed->step), followed, &into);
    SplinePointIn(into, followed, out, segment);
}

u32 CameraSplineCamera::Type()
{
    return TypeSpline;
}

void CameraSplineCamera::Read(Stream* stream)
{
    ReadBase(this, stream);
    auto* made = static_cast<CameraSpline*>(MemoryAllocate(sizeof(CameraSpline)));
    made->unknown48 = -1;
    made->vtable = g_CameraSplineVTable;
    made->samples = nullptr;
    made->lengths = nullptr;
    spline = made;
    made->Read(stream);
    // Into the flags' low half (their high half is what the memory had)
    stream->ReadS16(reinterpret_cast<s16*>(&splineFlags));
}

f32 CameraSplineCamera::OffsetAt(f32 along)
{
    if ((splineFlags & FlagTakesOffset) != 0)
    {
        return offset;
    }

    s32 sample = SampleBefore(spline, along);
    if (!(sample < spline->count))
    {
        return 0.0f;
    }

    Vector4 point;
    SplineSamplePoint(spline, &point, sample);
    u32 pair = static_cast<u32>(sample) << 1;
    f32 before = 0.0f;
    u32 beforeSample = 0;
    SampleShort(pair, &before, &beforeSample, 1);
    f32 after = 0.0f;
    u32 afterSample = 0;
    SampleShort(pair, &after, &afterSample, 0);
    f32 afterShare = SampleShare(afterSample, spline);
    f32 beforeShare = SampleShare(beforeSample, spline);
    f32 past = along - beforeShare;
    if (past < 0.0f)
    {
        past = 0.0f;
    }

    past = past / (afterShare - beforeShare);
    return (after - before) * past + before;
}

void CameraSplineCamera::PointBetween(f32 along, const Vector4* target, Vector4* out)
{
    s32 sample = SampleBefore(spline, along);
    if (!(sample < spline->count))
    {
        return;
    }

    Vector4 point;
    SplineSamplePoint(spline, &point, sample);
    u32 pair = static_cast<u32>(sample) << 1;
    f32 before = 0.0f;
    u32 beforeSample = 0;
    SampleByte(pair, &before, &beforeSample, 1);
    f32 after = 0.0f;
    u32 afterSample = 0;
    SampleByte(pair, &after, &afterSample, 0);
    f32 afterShare = SampleShare(afterSample, spline);
    f32 beforeShare = SampleShare(beforeSample, spline);
    f32 past = along - beforeShare;
    if (past < 0.0f)
    {
        past = 0.0f;
    }

    // From where the spline puts it toward the target
    Vector4 line[2];
    line[0] = *out;
    line[1] = *target;
    past = past / (afterShare - beforeShare);
    LinePointAt((after - before) * past + before, line, out);
}

void CameraSplineCamera::SampleByte(u32 sample, f32* value, u32* found, u32 backwards)
{
    u32 word = 0;
    SampleWord(sample, &word, found, backwards);
    *value = SampleByteValue(&word);
}

void CameraSplineCamera::SampleShort(u32 sample, f32* value, u32* found, u32 backwards)
{
    u32 word = 0;
    SampleWord(sample, &word, found, backwards);
    *value = SampleShortValue(&word);
}

void CameraSplineCamera::SampleWord(u32 sample, u32* word, u32* found, u32 backwards)
{
    constexpr u32 Skipped = 1u << 24;
    const CameraSpline* followed = spline;
    u32 at = backwards != 0 ? sample : sample + 2;
    u32 end = followed->count * 2 + 2;
    s32 step = backwards != 0 ? -2 : 2;
    const Vector4* samples = followed->samples;
    while (at < end)
    {
        *word = __builtin_bit_cast(u32, samples[at].w);
        if ((*word & Skipped) == 0)
        {
            *found = at;
            return;
        }

        at += step;
    }
}

f32 SampleByteValue(const u32* word)
{
    constexpr f32 Unit = 0x1.4p-5f;
    return static_cast<f32>(*word & 0xFF) * Unit + -5.0f;
}

f32 SampleShortValue(const u32* word)
{
    constexpr f32 Unit = 0x1.9p-10f;
    return static_cast<f32>((*word >> 8) & 0xFFFF) * Unit + -50.0f;
}

void Camera1C09::Destroy(u32 destroyFlags)
{
    constexpr u32 DestroyOnly = 2;
    vtable = g_Camera1C09VTable;
    DestroySpline(&spline, DestroyOnly);
    FreeKeys(&rotations);
    DestroyBase(this, destroyFlags);
}

f32 Camera1C09::At(const Vector4* target, CameraTarget*, Vector4* out)
{
    CurveSearch search;
    search.segment = -1;
    SplineNearestSearch(&spline, target, &search);
    f32 along = SplineNearestParameter(&spline, &search);
    CameraSubtype::AtParameter(along, out);
    return along + offset / spline.lengths[spline.count - 1];
}

void Camera1C09::AtParameter(f32 along, Vector4* out)
{
    constexpr f32 Out = 5.0f;
    f32 at = Clamped(along + offset / spline.lengths[spline.count - 1]);
    f32 into;
    s32 segment = SplineSegmentAt(at * (static_cast<f32>(spline.count) * spline.step), &spline, &into);
    SplinePointIn(into, &spline, out, segment);
    Vector4 rotation;
    RotationAt(&rotation, &rotations, at);
    Matrix4x4 turned;
    InitIdentityMatrix(&turned);
    MatrixFromRotation(&turned, &rotation);
    *reinterpret_cast<Vector4*>(turned.m[3]) = *out;
    Vector4 ahead = {0.0f, 0.0f, Out, 1.0f};
    VuTransformPoint(&turned, &ahead, &ahead);
    *out = ahead;
}

u32 Camera1C09::Type()
{
    return Type1C09;
}

void CameraPoint2::Destroy(u32 destroyFlags)
{
    vtable = g_CameraPoint2VTable;
    CameraPoint::Destroy(destroyFlags);
}

f32 CameraPoint2::At(const Vector4* target, CameraTarget*, Vector4* out)
{
    enum Modes : u32
    {
        ShareOfTheWay = 0,
        FromTheTarget = 1,
        NoFurtherThanThePoint = 2,
    };

    // The line from the target to the point
    Vector4 line[2];
    line[0] = *target;
    line[1] = point;
    s32 how = static_cast<s32>(mode);
    if (how == ShareOfTheWay)
    {
        LinePointAt(distance, line, out);
        return 1.0f;
    }

    if (how != FromTheTarget && how != NoFurtherThanThePoint)
    {
        return 1.0f;
    }

    *out = line[1];
    out->x = out->x - line[0].x;
    out->y = out->y - line[0].y;
    out->z = out->z - line[0].z;
    f32 inverse = InverseLength(out, LengthEpsilon);
    out->x = out->x * inverse;
    out->y = out->y * inverse;
    out->z = out->z * inverse;
    out->z = out->z * distance + line[0].z;
    out->x = out->x * distance + line[0].x;
    out->y = out->y * distance + line[0].y;
    if (how == NoFurtherThanThePoint && 1.0f < NearestLineParameter(line, out))
    {
        *out = line[1];
    }

    return 1.0f;
}

void CameraPoint2::AtParameter(f32, Vector4* out)
{
    *out = point;
}

u32 CameraPoint2::Type()
{
    return TypePoint2;
}

void CameraPoint2::Read(Stream* stream)
{
    ReadBase(this, stream);
    ReadVector(stream, &point);
    stream->ReadF32(&distance);
    s8 how;
    stream->ReadS8(&how);
    mode = static_cast<u8>(how);
}

void Camera1C0C::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

f32 Camera1C0C::At(const Vector4*, CameraTarget* follower, Vector4* out)
{
    Vector4 target = follower->objectPosition;
    target.y = centre.y;
    Vector4 away;
    away.x = target.x - centre.x;
    away.y = target.y - centre.y;
    away.z = target.z - centre.z;
    away.w = 1.0f;
    f32 flat = __builtin_sqrtf(away.x * away.x + away.y * away.y + away.z * away.z);
    f32 reach = radius;
    if (flat < extra)
    {
        reach = reach + extra;
    }
    else
    {
        reach = reach + flat;
    }

    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x = away.x * inverse * reach + centre.x;
    away.y = away.y * inverse * reach + centre.y;
    away.z = away.z * inverse * reach + centre.z;
    f32 height;
    if (flat < nearDistance)
    {
        height = nearHeight;
    }
    else if (farDistance < flat)
    {
        height = farHeight;
    }
    else
    {
        height = (farHeight - nearHeight) * ((flat - nearDistance) / (farDistance - nearDistance)) + nearHeight;
    }

    away.y = follower->objectPosition.y + height;
    *out = away;
    return 0.0f;
}

void Camera1C0C::AtParameter(f32, Vector4*)
{
}

u32 Camera1C0C::Type()
{
    return Type1C0C;
}

void Camera1C0C::Read(Stream* stream)
{
    // Four bytes over its flags
    stream->Read(this, 4, 1);
}

void CameraLine2::Destroy(u32 destroyFlags)
{
    vtable = g_CameraLine2VTable;
    CameraLine::Destroy(destroyFlags);
}

f32 CameraLine2::At(const Vector4*, CameraTarget* follower, Vector4* out)
{
    Vector4 target = follower->objectPosition;
    f32 x = target.x - start.x;
    f32 z = target.z - start.z;
    f32 flat = __builtin_sqrtf(x * x + z * z);
    f32 along = 0.0f;
    if (farDistance < flat)
    {
        along = 1.0f;
    }
    else if (!(flat < nearDistance))
    {
        along = (flat - nearDistance) / (farDistance - nearDistance) + nearDistance;
    }

    LinePointAt(along, &start, out);
    return along;
}

void CameraLine2::AtParameter(f32 along, Vector4* out)
{
    LinePointAt(along, &start, out);
}

u32 CameraLine2::Type()
{
    return TypeLine2;
}

void CameraLine2::Read(Stream* stream)
{
    ReadBase(this, stream);
    ReadLine(&start, stream);
    stream->ReadF32(&nearDistance);
    stream->ReadF32(&farDistance);
}

void Camera1C0E::Destroy(u32 destroyFlags)
{
    constexpr u32 DestroyAndFree = 3;
    vtable = g_Camera1C0EVTable;
    if (spline != nullptr)
    {
        CallVirtual<void>(spline, spline->vtable, 1, DestroyAndFree);
    }

    FreeKeys(&keys);
    DestroyBase(this, destroyFlags);
}

f32 Camera1C0E::At(const Vector4*, CameraTarget*, Vector4*)
{
    return 0.0f;
}

void Camera1C0E::AtParameter(f32, Vector4*)
{
}

u32 Camera1C0E::Type()
{
    return Type1C0E;
}

void Camera1C0E::Read(Stream*)
{
}

u32 Camera1C0E::Play(TimeClock* clock, Vector4* position, Vector4* rotation)
{
    constexpr f32 Speed = 20.0f;
    if (playing == 0)
    {
        playing = 1;
        time = 0.0f;
        rate = Speed / spline->lengths[spline->count - 1];
    }

    if (!(time < 1.0f))
    {
        return 1;
    }

    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    time = time + rate * seconds;
    CameraSpline* along = spline;
    f32 into;
    s32 segment = SplineSegmentAt(time * (static_cast<f32>(along->count) * along->step), along, &into);
    SplinePointIn(into, along, position, segment);
    Vector4 turn;
    RotationAt(&turn, &keys, time);
    *rotation = turn;
    return 0;
}

void CameraZone::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

f32 CameraZone::At(const Vector4*, CameraTarget* follower, Vector4* out)
{
    Vector4 target = follower->objectPosition;
    f32 across;
    f32 along;
    BoxFractionsOf(targetBox, &target, out, &across, &along);
    BoxPointAtFractions(across, along, cameraBox, out);
    return across;
}

void CameraZone::AtParameter(f32, Vector4*)
{
}

u32 CameraZone::Type()
{
    return TypeZone;
}

void CameraZone::Read(Stream* stream)
{
    stream->Read(cameraBox, sizeof(cameraBox), 1);
    stream->Read(targetBox, sizeof(targetBox), 1);
}

CameraSubtype* MakeCameraSubtype(void*, u32 type)
{
    switch (type)
    {
    case CameraSubtype::TypeMain:
        return reinterpret_cast<CameraSubtype*>(MainCamera::Construct(static_cast<MainCamera*>(MemoryAllocate(sizeof(MainCamera)))));
    case CameraSubtype::TypeLine:
        return CameraLine::Construct(static_cast<CameraLine*>(MemoryAllocate(sizeof(CameraLine))));
    case CameraSubtype::TypePath:
        return CameraPath::Construct(static_cast<CameraPath*>(MemoryAllocate(sizeof(CameraPath))));
    case CameraSubtype::TypePoint:
        return CameraPoint::Construct(static_cast<CameraPoint*>(MemoryAllocate(sizeof(CameraPoint))));
    case CameraSubtype::TypeBoss:
    {
        auto* camera = static_cast<BossCamera*>(CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(BossCamera)))));
        camera->curves = 0;
        camera->vtable = g_BossCameraVTable;
        camera->turnLimit = 0.0f;
        camera->curveAllAxes = 0;
        return camera;
    }
    case CameraSubtype::TypeSpline:
    {
        auto* camera = static_cast<CameraSplineCamera*>(
            CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(CameraSplineCamera)))));
        camera->vtable = g_CameraSplineCameraVTable;
        camera->offset = 6.0f;
        camera->splineFlags |= CameraSplineCamera::FlagTakesOffset;
        camera->spline = nullptr;
        return camera;
    }
    case CameraSubtype::Type1C09:
    {
        auto* camera = static_cast<Camera1C09*>(CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(Camera1C09)))));
        camera->rotations.keys = nullptr;
        camera->vtable = g_Camera1C09VTable;
        camera->rotations.eases = 1;
        camera->rotations.growth = 0x40;
        camera->rotations.count = 0;
        camera->rotations.capacity = 0;
        camera->spline.vtable = g_CameraSplineVTable;
        camera->spline.lengths = nullptr;
        camera->spline.samples = nullptr;
        camera->spline.unknown48 = -1;
        return camera;
    }
    case CameraSubtype::TypePoint2:
    {
        auto* camera = static_cast<CameraPoint2*>(CameraPoint::Construct(static_cast<CameraPoint*>(MemoryAllocate(sizeof(CameraPoint2)))));
        camera->vtable = g_CameraPoint2VTable;
        camera->distance = 0.5f;
        camera->mode = 0;
        return camera;
    }
    case CameraSubtype::Type1C0C:
    {
        CameraSubtype* camera = CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(Camera1C0C))));
        camera->vtable = g_Camera1C0CVTable;
        return camera;
    }
    case CameraSubtype::TypeLine2:
    {
        CameraLine* camera = CameraLine::Construct(static_cast<CameraLine*>(MemoryAllocate(sizeof(CameraLine2))));
        camera->vtable = g_CameraLine2VTable;
        return camera;
    }
    case CameraSubtype::Type1C0E:
    {
        auto* camera = static_cast<Camera1C0E*>(CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(Camera1C0E)))));
        camera->spline = nullptr;
        camera->vtable = g_Camera1C0EVTable;
        camera->keys.keys = nullptr;
        camera->keys.eases = 1;
        camera->keys.growth = 0x40;
        camera->keys.count = 0;
        camera->keys.capacity = 0;
        camera->playing = 0;
        camera->finished = 0;
        return camera;
    }
    case CameraSubtype::TypeZone:
    {
        CameraSubtype* camera = CameraSubtype::Construct(static_cast<CameraSubtype*>(MemoryAllocate(sizeof(CameraZone))));
        camera->vtable = g_CameraZoneVTable;
        return camera;
    }
    default:
        return nullptr;
    }
}

f32 EaseInOut(f32 share, const f32* sharpness)
{
    constexpr f32 QuarterTurn = 0x1.921fb6p+0f;
    constexpr f32 HalfTurn = 0x1.921fb6p+1f;
    s32 angle;
    AngleFrom(&angle, *sharpness * QuarterTurn, AngleRadians);
    f32 scale = 1.0f / SinOfAngle(&angle);
    AngleFrom(&angle, (share - 0.5f) * *sharpness * HalfTurn, AngleRadians);
    return (SinOfAngle(&angle) * scale + 1.0f) * 0.5f;
}

Vector4* RotationAt(Vector4* out, const CameraRotationKeys* keys, f32 along)
{
    constexpr f32 Sharpness = Rounded(0.9);
    if (1.0f <= along)
    {
        *out = keys->keys[0].rotation;
        return out;
    }

    // The two keys around it (from the latest: retail reads before the keys when it's before them all)
    s32 later = -1;
    for (s32 index = keys->count - 1; index != 0; index--)
    {
        if (keys->keys[index].time <= along && along <= keys->keys[index - 1].time)
        {
            later = index;
            break;
        }
    }

    const CameraRotationKey& from = keys->keys[later];
    const CameraRotationKey& to = keys->keys[later - 1];
    f32 share = (along - from.time) * (1.0f / (to.time - from.time));
    if (keys->eases != 0)
    {
        f32 sharpness = Sharpness;
        share = EaseInOut(share, &sharpness);
    }

    Vector4 start = from.rotation;
    Vector4 end = to.rotation;
    Vector4 turned;
    SlerpRotations(share, &turned, &start, &end);
    *out = turned;
    return out;
}

MainCamera* MainCamera::Construct(MainCamera* camera)
{
    camera->flags = 0;
    camera->switches = 0;
    camera->blendTime = 1.0f;
    camera->flags = (camera->flags & ~FlagSteers) | FlagSteers;
    camera->first = nullptr;
    camera->second = nullptr;
    camera->group = 0;
    return camera;
}

void MainCamera::Destroy(u32 destroyFlags)
{
    if (first != nullptr)
    {
        first->DestroyVirtual(DestroyAndFree);
    }

    if (second != nullptr)
    {
        second->DestroyVirtual(DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 MainCamera::NearerBlendIn(const s32* yaw)
{
    s32 current = WrapAngle(*yaw);
    s32 fromBlendIn = WrapAngle(current - static_cast<s32>(blendInYaw));
    s32 fromStart = WrapAngle(current - static_cast<s32>(yawStart));
    if (fromBlendIn < 0)
    {
        fromBlendIn = -fromBlendIn;
    }

    if (fromStart < 0)
    {
        fromStart = -fromStart;
    }

    return !(fromStart < fromBlendIn);
}

void MainCamera::Read(Stream* stream)
{
    constexpr s32 MadeByAnyFactory = -1;
    if (g_ObjectBuilder == nullptr)
    {
        g_ObjectBuilder = ObjectBuilder::Construct(static_cast<ObjectBuilder*>(MemoryAllocate(sizeof(ObjectBuilder))));
    }

    ObjectBuilder* builder = g_ObjectBuilder;
    stream->ReadS32(reinterpret_cast<s32*>(&flags));
    stream->ReadS16(reinterpret_cast<s16*>(&switches));
    stream->ReadF32(&blendTime);
    ReadVector(stream, &leftoverVector1);
    ReadVector(stream, &leftoverVector2);
    stream->ReadF32(&leftoverFloat1);
    stream->ReadF32(&leftoverFloat2);
    ReadAngle(&fovStart, stream);
    ReadAngle(&fovEnd, stream);
    ReadAngle(&pitchStart, stream);
    ReadAngle(&pitchEnd, stream);
    ReadAngle(&yawStart, stream);
    ReadAngle(&yawEnd, stream);
    stream->ReadF32(&distanceStart);
    stream->ReadF32(&distanceEnd);
    stream->ReadF32(&secondValue);
    stream->ReadF32(&firstValue);
    ReadAngle(&yawExtra, stream);
    ReadAngle(&blendInYaw, stream);
    ReadAngle(&blendInPitch, stream);
    stream->ReadF32(&blendInDistance);
    s32 types[2];
    stream->ReadS32(&types[0]);
    stream->ReadS32(&types[1]);
    stream->ReadS8(&group);
    CameraSubtype** subtypes[2] = {&first, &second};
    for (u32 index = 0; index < 2; index++)
    {
        if (types[index] == CameraSubtype::TypeNone)
        {
            *subtypes[index] = nullptr;
            continue;
        }

        auto* made = static_cast<CameraSubtype*>(builder->Build(types[index], MadeByAnyFactory));
        *subtypes[index] = made;
        made->ReadVirtual(stream);
    }
}

CameraEvent* CameraEvent::Construct(CameraEvent* event, Reference** argument, u32 kinds, CameraNode* node)
{
    event->unknown04 = EventId;
    event->vtable = g_GameEventVTable;
    event->kinds = kinds;
    event->reference = nullptr;
    event->type = 0;
    // Retail copies the handle into its base's constructor and lets the copies go again: the event keeps the caller's reference
    event->argument = *argument;
    event->vtable = g_CameraEventBaseVTable;
    event->node = node;
    event->vtable = g_CameraEventVTable;
    return event;
}

void CameraEvent::Destroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CameraEvent::BaseDestroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

CameraNode* CameraNode::Construct(CameraNode* node, ChunkData* chunk, CameraTrigger* trigger)
{
    TriggerNode::Construct(node, chunk, trigger);
    node->vtable = g_CameraNodeVTable;
    node->nodeBits |= NodeBitCamera;
    node->camera = trigger->camera;
    return node;
}

void CameraNode::Destroy(u32 destroyFlags)
{
    vtable = g_CameraNodeVTable;
    if (camera != nullptr)
    {
        camera->Destroy(DestroyAndFree);
    }

    TriggerNode::Destroy(destroyFlags);
}

u32 CameraNode::Kind()
{
    return 8;
}

u32 CameraNode::Type()
{
    return TypeId;
}

void CameraNode::ForgetEvent()
{
    event = nullptr;
}

void CameraNode::Enter(InstanceContext* entering)
{
    constexpr u32 EventSize = 0x18;
    CameraEvent* forEntering = nullptr;
    if (event == nullptr)
    {
        Reference* handle = owner != nullptr ? AddReference(owner) : nullptr;
        event = CameraEvent::Construct(static_cast<CameraEvent*>(MemoryAllocate(EventSize)), &handle, eventKinds, this);
    }

    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(entering, &handle);
    for (u32 index = 0; index < instanceCount; index++)
    {
        if (forEntering == nullptr)
        {
            Reference* other = entering != nullptr ? AddReference(entering) : nullptr;
            forEntering = CameraEvent::Construct(static_cast<CameraEvent*>(MemoryAllocate(EventSize)), &other, eventKinds, this);
        }

        handle = forEntering != nullptr ? AddEventReference(forEntering) : nullptr;
        QueueEvent(instances[index], &handle);
    }
}
