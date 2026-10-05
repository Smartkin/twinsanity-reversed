#include "game/math.h"

#include "platform/math.h"

// The maths module's turns: rotations (quaternions) about the axes, of Euler angles and about an axis, matrices about x and y, of
// Euler angles and about an axis, a vector turned about an axis, a direction and a rotation turned toward another, whether two
// rotations are the same turn, the signed angle between two directions, a rotation's yaw snapped to steps and an angle's tangent.
// The sines and cosines are VU0's (Platform::Math::SinCos)

EABI_EXPORT(FUN_00186930, SameRotation);
EABI_EXPORT(FUN_00187890, SlerpRotations);
EABI_EXPORT(FUN_00187c48, SnapToYaw);
EABI_EXPORT(FUN_00188950, TurnBySineCosine);
EABI_EXPORT(FUN_00188a90, TurnToward);

namespace
{
// Rotations this near each other (1 less their cosine) are slerped along a straight line
constexpr f32 StraightWithin = 0x1.99999ap-5f;

// The arc cosine of any value (radians), by its magnitude's
f32 ArcCosine(f32 cosine)
{
    f32 value = __builtin_fabsf(cosine);
    f32 complement = 1.0f - value;
    if (complement < 0.0f)
    {
        complement = 0.0f;
    }

    f32 radians = ArcCosineOfPositive(value) * __builtin_sqrtf(complement);
    if (cosine < 0.0f)
    {
        radians = Pi - radians;
    }

    return radians;
}

// The sine and the cosine of half an angle (65536ths of a turn)
void SinCosOfHalf(s32 angle, f32* sinCos)
{
    SinCosRadians(static_cast<f32>(angle) * AngleToRadians * 0.5f, sinCos);
}

// Half an angle (65536ths of a turn) in radians
f32 HalfRadians(s32 angle)
{
    return static_cast<f32>(angle) * AngleToRadians * 0.5f;
}

void NoRotation(Vector4* rotation)
{
    rotation->x = 0.0f;
    rotation->y = 0.0f;
    rotation->z = 0.0f;
    rotation->w = 1.0f;
}
}

void SnapToYaw(Vector4* rotation, f32 step)
{
    Matrix4x4 matrix;
    MatrixFromRotation(&matrix, rotation);
    s32 pitch;
    s32 yaw;
    s32 roll;
    EulerAnglesOfMatrix(&matrix, &pitch, &yaw, &roll);
    f32 steps = (static_cast<f32>(yaw) * AngleToRadians + step * 0.5f) / step;
    AngleFrom(&yaw, step * static_cast<f32>(static_cast<s32>(steps)), AngleRadians);
    if (yaw == 0)
    {
        NoRotation(rotation);
        return;
    }

    f32 sinCos[2];
    SinCosOfHalf(yaw, sinCos);
    rotation->w = sinCos[1];
    rotation->y = sinCos[0];
    rotation->x = 0.0f;
    rotation->z = 0.0f;
}

s32* AngleBetweenDirections(s32* angle, const Vector4* from, const Vector4* to)
{
    f32 fromSquared = from->x * from->x + from->y * from->y + from->z * from->z;
    f32 toSquared = to->x * to->x + to->y * to->y + to->z * to->z;
    f32 inverse = Kept(__builtin_sqrtf(fromSquared * toSquared));
    if (inverse != 0.0f)
    {
        inverse = 1.0f / inverse;
    }

    f32 cosine = (from->x * to->x + from->y * to->y + from->z * to->z) * inverse;
    s32 turned = static_cast<s32>(ArcCosine(cosine) * RadiansToAngle);
    if (turned != 0 && turned != HalfTurnAngle)
    {
        Vector4 cross;
        cross.x = from->y * to->z - from->z * to->y;
        cross.y = from->z * to->x - from->x * to->z;
        cross.z = from->x * to->y - from->y * to->x;
        if (__builtin_fabsf(cross.x) <= Epsilon && __builtin_fabsf(cross.y) <= Epsilon && __builtin_fabsf(cross.z) <= Epsilon)
        {
            // Parallel: none or a half turn
            AngleFrom(&turned, turned < QuarterTurnAngle ? 0.0f : Pi, AngleRadians);
        }
        else
        {
            f32 by = cross.y;
            if (__builtin_fabsf(cross.y) <= Epsilon)
            {
                by = __builtin_fabsf(cross.x) <= Epsilon ? cross.z : cross.x;
            }

            f32 sign = by < 0.0f ? -1.0f : 1.0f;
            turned = static_cast<s32>(static_cast<f32>(turned) * sign);
        }
    }

    *angle = turned;
    return angle;
}

void TurnBySineCosine(f32 sine, f32 cosine, Vector4* vector, const Vector4* axis, u32 unitAxis)
{
    f32 x = axis->x;
    f32 y = axis->y;
    f32 z = axis->z;
    f32 scale = 1.0f;
    if (unitAxis == 0)
    {
        f32 lengthSquared = x * x + y * y + z * z;
        scale = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            scale = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
        }

        x = x * scale;
        y = y * scale;
        z = z * scale;
    }

    if (!(0.0f < scale))
    {
        return;
    }

    // Rodrigues' formula: its part along the axis kept, the rest turned
    f32 vx = vector->x;
    f32 vy = vector->y;
    f32 vz = vector->z;
    f32 along = (vx * x + vy * y + vz * z) * (1.0f - cosine);
    f32 crossX = (y * vz - z * vy) * sine;
    f32 crossY = (z * vx - x * vz) * sine;
    f32 crossZ = (x * vy - y * vx) * sine;
    vector->z = (crossZ + z * along) + vz * cosine;
    vector->x = (crossX + x * along) + vx * cosine;
    vector->y = (crossY + y * along) + vy * cosine;
}

void TurnToward(f32 share, Vector4* out, const Vector4* from, const Vector4* to)
{
    f32 angle = ArcCosine(from->x * to->x + from->y * to->y + from->z * to->z);
    f32 sinCos[2];
    SinCosRadians(angle, sinCos);
    if (sinCos[0] == 0.0f)
    {
        *out = *to;
        return;
    }

    // A slerp: each weighted by the sine of the other's share of the angle over the angle's sine
    f32 inverse = 1.0f / sinCos[0];
    f32 shares[4];
    Platform::Math::SinCos(angle * (1.0f - share), angle * share, shares);
    out->x = from->x * inverse * shares[0];
    out->y = from->y * inverse * shares[0];
    out->z = from->z * inverse * shares[0];
    out->w = from->w;
    out->x = out->x + to->x * inverse * shares[2];
    out->y = out->y + to->y * inverse * shares[2];
    out->z = out->z + to->z * inverse * shares[2];
}

u32 SameRotation(const Vector4* rotation, const Vector4* other, f32 tolerance)
{
    // A quaternion and its opposite are the same turn
    bool same = __builtin_fabsf(rotation->x - other->x) <= tolerance && __builtin_fabsf(rotation->y - other->y) <= tolerance &&
                __builtin_fabsf(rotation->z - other->z) <= tolerance && __builtin_fabsf(rotation->w - other->w) <= tolerance;
    bool opposite = __builtin_fabsf(rotation->x + other->x) <= tolerance &&
                    __builtin_fabsf(rotation->y + other->y) <= tolerance &&
                    __builtin_fabsf(rotation->z + other->z) <= tolerance && __builtin_fabsf(rotation->w + other->w) <= tolerance;
    return same || opposite;
}

void SlerpRotations(f32 t, Vector4* out, const Vector4* from, const Vector4* to)
{
    // The shorter way round: from's opposite when they're more than half a turn apart
    f32 cosine = from->x * to->x + from->y * to->y + from->z * to->z + from->w * to->w;
    bool opposite = cosine < 0.0f;
    if (opposite)
    {
        cosine = -cosine;
    }

    if (StraightWithin < 1.0f - cosine)
    {
        // Each weighted by the sine of the other's share of the angle over the angle's sine (from's by sin((t - 1) angle),
        // negated)
        f32 angle = ArcCosine(cosine);
        f32 sinCos[2];
        SinCosRadians(angle, sinCos);
        f32 shares[4];
        Platform::Math::SinCos(angle * t, angle * t - angle, shares);
        f32 inverse = 1.0f / sinCos[0];
        f32 toShare = shares[0] * inverse;
        f32 fromShare = shares[2] * inverse;
        if (!opposite)
        {
            fromShare = -fromShare;
        }

        out->x = from->x * fromShare;
        out->y = from->y * fromShare;
        out->z = from->z * fromShare;
        out->w = from->w * fromShare;
        out->x = out->x + to->x * toShare;
        out->y = out->y + to->y * toShare;
        out->z = out->z + to->z * toShare;
        out->w = out->w + to->w * toShare;
        return;
    }

    // Along a straight line, made a unit one again (from at no share or less, to at the whole way or more)
    Vector4 start = *from;
    if (opposite)
    {
        start.x = -start.x;
        start.y = -start.y;
        start.z = -start.z;
        start.w = -start.w;
    }

    if (!(0.0f < t))
    {
        *out = start;
        return;
    }

    out->x = to->x;
    out->y = to->y;
    out->z = to->z;
    out->w = to->w;
    if (t < 1.0f)
    {
        out->x = (to->x - start.x) * t + start.x;
        out->y = (to->y - start.y) * t + start.y;
        out->z = (to->z - start.z) * t + start.z;
        out->w = (to->w - start.w) * t + start.w;
        f32 inverse = InverseLength4(0.0f, InverseEpsilon, out);
        out->x = out->x * inverse;
        out->y = out->y * inverse;
        out->z = out->z * inverse;
        out->w = out->w * inverse;
    }
}

f32 TanOfAngle(const s32* angle)
{
    f32 sinCos[2];
    SinCos16(*angle, sinCos);
    if (sinCos[1] == 0.0f)
    {
        return 0.0f < sinCos[0] ? Infinite : -Infinite;
    }

    return sinCos[0] / sinCos[1];
}

void MatrixAboutX(Matrix4x4* matrix, const s32* angle)
{
    if (*angle == 0)
    {
        InitIdentityMatrix(matrix);
        return;
    }

    f32 sinCos[2];
    SinCos16(*angle, sinCos);
    *matrix = {};
    matrix->m[3][3] = 1.0f;
    matrix->m[0][0] = 1.0f;
    matrix->m[1][1] = sinCos[1];
    matrix->m[2][2] = sinCos[1];
    matrix->m[2][1] = -sinCos[0];
    matrix->m[1][2] = sinCos[0];
}

void MatrixAboutY(Matrix4x4* matrix, const s32* angle)
{
    if (*angle == 0)
    {
        InitIdentityMatrix(matrix);
        return;
    }

    f32 sinCos[2];
    SinCos16(*angle, sinCos);
    *matrix = {};
    matrix->m[3][3] = 1.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[0][0] = sinCos[1];
    matrix->m[2][2] = sinCos[1];
    matrix->m[0][2] = -sinCos[0];
    matrix->m[2][0] = sinCos[0];
}

void MatrixFromPitchYaw(Matrix4x4* matrix, const s32* pitch, const s32* yaw)
{
    if (*pitch == 0)
    {
        MatrixAboutY(matrix, yaw);
        return;
    }

    if (*yaw == 0)
    {
        MatrixAboutX(matrix, pitch);
        return;
    }

    f32 yawSinCos[2];
    SinCos16(*yaw, yawSinCos);
    f32 pitchSinCos[2];
    SinCos16(*pitch, pitchSinCos);
    f32 sinY = yawSinCos[0];
    f32 cosY = yawSinCos[1];
    f32 sinX = pitchSinCos[0];
    f32 cosX = pitchSinCos[1];
    *matrix = {};
    matrix->m[0][0] = cosY;
    matrix->m[0][2] = -sinY;
    matrix->m[1][0] = sinY * sinX;
    matrix->m[1][1] = cosX;
    matrix->m[1][2] = sinX * cosY;
    matrix->m[2][0] = sinY * cosX;
    matrix->m[2][1] = -sinX;
    matrix->m[2][2] = cosX * cosY;
    matrix->m[3][3] = 1.0f;
}

void MatrixFromPitchRoll(Matrix4x4* matrix, const s32* pitch, const s32* roll)
{
    if (*pitch == 0)
    {
        MatrixRotationZ(matrix, roll);
        return;
    }

    if (*roll == 0)
    {
        MatrixAboutX(matrix, pitch);
        return;
    }

    f32 rollSinCos[2];
    SinCos16(*roll, rollSinCos);
    f32 pitchSinCos[2];
    SinCos16(*pitch, pitchSinCos);
    f32 sinZ = rollSinCos[0];
    f32 cosZ = rollSinCos[1];
    f32 sinX = pitchSinCos[0];
    f32 cosX = pitchSinCos[1];
    *matrix = {};
    matrix->m[0][0] = cosZ;
    matrix->m[0][1] = sinZ;
    matrix->m[1][0] = -cosX * sinZ;
    matrix->m[1][1] = cosX * cosZ;
    matrix->m[1][2] = sinX;
    matrix->m[2][0] = sinX * sinZ;
    matrix->m[2][1] = -sinX * cosZ;
    matrix->m[2][2] = cosX;
    matrix->m[3][3] = 1.0f;
}

void MatrixFromYawRoll(Matrix4x4* matrix, const s32* yaw, const s32* roll)
{
    if (*yaw == 0)
    {
        MatrixRotationZ(matrix, roll);
        return;
    }

    if (*roll == 0)
    {
        MatrixAboutY(matrix, yaw);
        return;
    }

    f32 rollSinCos[2];
    SinCos16(*roll, rollSinCos);
    f32 yawSinCos[2];
    SinCos16(*yaw, yawSinCos);
    f32 sinZ = rollSinCos[0];
    f32 cosZ = rollSinCos[1];
    f32 sinY = yawSinCos[0];
    f32 cosY = yawSinCos[1];
    *matrix = {};
    matrix->m[0][0] = cosY * cosZ;
    matrix->m[0][1] = cosY * sinZ;
    matrix->m[0][2] = -sinY;
    matrix->m[1][0] = -sinZ;
    matrix->m[1][1] = cosZ;
    matrix->m[2][0] = sinY * cosZ;
    matrix->m[2][1] = sinY * sinZ;
    matrix->m[2][2] = cosY;
    matrix->m[3][3] = 1.0f;
}

void MatrixFromAngles(Matrix4x4* matrix, const s32* x, const s32* y, const s32* z)
{
    if (*x == 0)
    {
        MatrixFromYawRoll(matrix, y, z);
        return;
    }

    if (*y == 0)
    {
        MatrixFromPitchRoll(matrix, x, z);
        return;
    }

    if (*z == 0)
    {
        MatrixFromPitchYaw(matrix, x, y);
        return;
    }

    f32 sinCosXZ[4];
    SinCos16Pair(*x, *z, sinCosXZ);
    f32 sinCosY[2];
    SinCos16(*y, sinCosY);
    f32 sinX = sinCosXZ[0];
    f32 cosX = sinCosXZ[1];
    f32 sinZ = sinCosXZ[2];
    f32 cosZ = sinCosXZ[3];
    f32 sinY = sinCosY[0];
    f32 cosY = sinCosY[1];
    f32 cosXcosZ = cosX * cosZ;
    f32 cosXsinZ = cosX * sinZ;
    f32 sinXsinZ = sinX * sinZ;
    f32 sinXcosZ = sinX * cosZ;
    *matrix = {};
    matrix->m[0][0] = cosY * cosZ;
    matrix->m[0][1] = cosY * sinZ;
    matrix->m[0][2] = -sinY;
    matrix->m[1][0] = sinY * sinXcosZ - cosXsinZ;
    matrix->m[1][1] = sinY * sinXsinZ + cosXcosZ;
    matrix->m[1][2] = sinX * cosY;
    matrix->m[2][0] = sinY * cosXcosZ + sinXsinZ;
    matrix->m[2][1] = sinY * cosXsinZ - sinXcosZ;
    matrix->m[2][2] = cosX * cosY;
    matrix->m[3][3] = 1.0f;
}

void AxisAngleMatrix(Matrix4x4* matrix, const Vector4* axis, const s32* angle, u32 unitAxis)
{
    if (*angle == 0)
    {
        InitIdentityMatrix(matrix);
        return;
    }

    f32 sinCos[2];
    SinCos16(*angle, sinCos);
    MatrixAboutAxis(sinCos[0], sinCos[1], matrix, axis, unitAxis);
}

void RotationFromPitch(Vector4* rotation, const s32* pitch)
{
    if (*pitch == 0)
    {
        NoRotation(rotation);
        return;
    }

    f32 sinCos[2];
    SinCosOfHalf(*pitch, sinCos);
    rotation->w = sinCos[1];
    rotation->y = 0.0f;
    rotation->x = sinCos[0];
    rotation->z = 0.0f;
}

void RotationFromYaw(Vector4* rotation, const s32* yaw)
{
    if (*yaw == 0)
    {
        NoRotation(rotation);
        return;
    }

    f32 sinCos[2];
    SinCosOfHalf(*yaw, sinCos);
    rotation->w = sinCos[1];
    rotation->x = 0.0f;
    rotation->y = sinCos[0];
    rotation->z = 0.0f;
}

void RotationFromRoll(Vector4* rotation, const s32* roll)
{
    if (*roll == 0)
    {
        NoRotation(rotation);
        return;
    }

    f32 sinCos[2];
    SinCosOfHalf(*roll, sinCos);
    rotation->w = sinCos[1];
    rotation->x = 0.0f;
    rotation->z = sinCos[0];
    rotation->y = 0.0f;
}

void GetRotationXY(Vector4* rotation, const s32* x, const s32* y)
{
    if (*x == 0)
    {
        RotationFromYaw(rotation, y);
        return;
    }

    if (*y == 0)
    {
        RotationFromPitch(rotation, x);
        return;
    }

    f32 sinCos[4];
    Platform::Math::SinCos(HalfRadians(*x), HalfRadians(*y), sinCos);
    rotation->x = sinCos[0] * sinCos[3];
    rotation->y = sinCos[1] * sinCos[2];
    rotation->z = -sinCos[0] * sinCos[2];
    rotation->w = sinCos[1] * sinCos[3];
}

void GetRotationXZ(Vector4* rotation, const s32* x, const s32* z)
{
    if (*x == 0)
    {
        RotationFromRoll(rotation, z);
        return;
    }

    if (*z == 0)
    {
        RotationFromPitch(rotation, x);
        return;
    }

    f32 sinCos[4];
    Platform::Math::SinCos(HalfRadians(*x), HalfRadians(*z), sinCos);
    rotation->x = sinCos[0] * sinCos[3];
    rotation->y = sinCos[0] * sinCos[2];
    rotation->z = sinCos[1] * sinCos[2];
    rotation->w = sinCos[1] * sinCos[3];
}

void GetRotationYZ(Vector4* rotation, const s32* y, const s32* z)
{
    if (*y == 0)
    {
        RotationFromRoll(rotation, z);
        return;
    }

    if (*z == 0)
    {
        RotationFromYaw(rotation, y);
        return;
    }

    f32 sinCos[4];
    Platform::Math::SinCos(HalfRadians(*y), HalfRadians(*z), sinCos);
    rotation->x = -sinCos[0] * sinCos[2];
    rotation->y = sinCos[0] * sinCos[3];
    rotation->z = sinCos[1] * sinCos[2];
    rotation->w = sinCos[1] * sinCos[3];
}

void GetRotationXYZ(Vector4* rotation, const s32* x, const s32* y, const s32* z)
{
    if (*x == 0)
    {
        GetRotationYZ(rotation, y, z);
        return;
    }

    if (*y == 0)
    {
        GetRotationXZ(rotation, x, z);
        return;
    }

    if (*z == 0)
    {
        GetRotationXY(rotation, x, y);
        return;
    }

    f32 sinCosXZ[4];
    Platform::Math::SinCos(HalfRadians(*x), HalfRadians(*z), sinCosXZ);
    f32 sinCosY[2];
    SinCosRadians(HalfRadians(*y), sinCosY);
    f32 cosXcosZ = sinCosXZ[1] * sinCosXZ[3];
    f32 sinXsinZ = sinCosXZ[0] * sinCosXZ[2];
    f32 sinXcosZ = sinCosXZ[0] * sinCosXZ[3];
    f32 cosXsinZ = sinCosXZ[1] * sinCosXZ[2];
    rotation->x = sinXcosZ * sinCosY[1] - cosXsinZ * sinCosY[0];
    rotation->y = sinXsinZ * sinCosY[1] + cosXcosZ * sinCosY[0];
    rotation->z = cosXsinZ * sinCosY[1] - sinXcosZ * sinCosY[0];
    rotation->w = cosXcosZ * sinCosY[1] + sinXsinZ * sinCosY[0];
}

void RotationAboutAxis(Vector4* rotation, const Vector4* axis, const s32* angle, u32 unitAxis)
{
    // The half angle cut down to whole 65536ths of a turn
    s32 half = static_cast<s32>(static_cast<f32>(*angle) * 0.5f);
    f32 sinCos[2];
    SinCos16(half, sinCos);
    RotationFromAxisSine(sinCos[0], sinCos[1], rotation, axis, unitAxis);
}

void TurnAboutAxis(Vector4* vector, const Vector4* axis, const s32* angle, u32 unitAxis)
{
    if (*angle == 0)
    {
        return;
    }

    f32 sinCos[2];
    SinCos16(*angle, sinCos);
    TurnBySineCosine(sinCos[0], sinCos[1], vector, axis, unitAxis);
}
