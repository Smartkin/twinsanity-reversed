#include "game/math.h"

#include "game/place.h"
#include "gcc2.h"

#include "platform/math.h"

extern "C"
{
    // The C library's expf
    f32 Exponential(f32 value) RETAIL(FUN_002c1e98);
}

namespace
{
// Numerical Recipes' ran0: Park and Miller's multiplier and modulus, the modulus's quotient and remainder by the multiplier
// (Schrage's method), and the mask its seeds are kept with
constexpr s32 ParkMillerMultiplier = 16807;
constexpr s32 ParkMillerModulus = 0x7FFFFFFF;
constexpr s32 ParkMillerQuotient = 127773;
constexpr s32 ParkMillerRemainder = 2836;
constexpr s32 SeedMask = 0x75BD924;
// Numerical Recipes' quick generator's multiplier and increment
constexpr u64 QuickMultiplier = 1664525;
constexpr u64 QuickIncrement = 0x3C6EF35F;
// rand's 2^31 values made [0, 1), and less their middle [-1, 1)
constexpr f32 RandScale = 0x1.0p-31f;
constexpr f32 RandMiddle = 0x1.0p+30f;
constexpr f32 SignedRandScale = 0x1.0p-30f;
// 65536ths of a turn in a turn (AngleFrom's turns)
constexpr f32 TurnsToAngle = FullTurnAngle;
// What a value within this of none isn't divided by, and InverseEpsilon squared (the float product, a bit above 1e-20): a
// rotation's length squared up to it is none
constexpr f32 DivisionEpsilon = InverseEpsilon;
constexpr f32 InverseEpsilonSquared = 0x1.79ca12p-67f;

// VU0's (the platform's)
void SinCos(f32 radians, f32* out)
{
    f32 values[4];
    Platform::Math::SinCos(radians, 0.0f, values);
    out[0] = values[0];
    out[1] = values[1];
}
}

extern "C"
{
    void MatrixIdentity(Matrix4x4* matrix)
    {
        *matrix = g_IdentityMatrix;
    }

    Vector2* CopyVector2(Vector2* out, const Vector2* vector)
    {
        out->x = vector->x;
        out->y = vector->y;
        return out;
    }

    void MatrixRotateX(Matrix4x4* matrix, s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        for (auto& row : matrix->m)
        {
            f32 y = row[1];
            f32 z = row[2];
            row[1] = y * sinCos[1] - z * sinCos[0];
            row[2] = y * sinCos[0] + z * sinCos[1];
        }
    }

    void MatrixRotateY(Matrix4x4* matrix, s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        for (auto& row : matrix->m)
        {
            f32 x = row[0];
            f32 z = row[2];
            row[0] = x * sinCos[1] + z * sinCos[0];
            row[2] = z * sinCos[1] - x * sinCos[0];
        }
    }

    void MatrixRotateZ(Matrix4x4* matrix, s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        for (auto& row : matrix->m)
        {
            f32 x = row[0];
            f32 y = row[1];
            row[0] = x * sinCos[1] - y * sinCos[0];
            row[1] = x * sinCos[0] + y * sinCos[1];
        }
    }

    s32 RandomNext(s32* seed)
    {
        if (seed == nullptr)
        {
            seed = &g_RandomSeed;
        }
        else if (*seed == 0)
        {
            *seed = 1;
        }

        s32 next = *seed ^ SeedMask;
        s32 high = next / ParkMillerQuotient;
        next = ParkMillerMultiplier * (next - high * ParkMillerQuotient) - ParkMillerRemainder * high;
        if (next < 0)
        {
            next += ParkMillerModulus;
        }

        *seed = next ^ SeedMask;
        return *seed;
    }

    f32 RandomFloat01(u64* state)
    {
        u64 next = *state * QuickMultiplier + QuickIncrement;
        *state = next;
        // Its low 23 bits the mantissa of a float of [1, 2)
        FloatBits bits = {};
        bits.mantissa = static_cast<u32>(next);
        bits.exponent = FloatExponentBias;
        return __builtin_bit_cast(f32, bits.value) - 1.0f;
    }

    void SinCosRadians(f32 radians, f32* out)
    {
        SinCos(radians, out);
    }

    void SinCos16(s32 angle, f32* out)
    {
        SinCos(static_cast<f32>(angle) * AngleToRadians, out);
    }

    f32 Sin16(s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        return sinCos[0];
    }

    f32 Cos16(s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        return sinCos[1];
    }

    void SinCos16Pair(s32 first, s32 second, f32* out)
    {
        Platform::Math::SinCos(static_cast<f32>(first) * AngleToRadians, static_cast<f32>(second) * AngleToRadians, out);
    }

    void AngleFrom(s32* angle, f32 value, u32 unit)
    {
        f32 scale;
        switch (unit)
        {
        case AngleDegrees:
            scale = DegreesToAngle;
            break;
        case AngleTurns:
            scale = TurnsToAngle;
            break;
        default:
            scale = RadiansToAngle;
            break;
        }

        *angle = static_cast<s32>(value * scale);
    }

    s32* DivideAngle(s32* angle, f32 divisor)
    {
        *angle = static_cast<s32>(static_cast<f32>(*angle) / divisor);
        return angle;
    }

    s32* MultiplyAngle(s32* angle, f32 scale)
    {
        *angle = static_cast<s32>(static_cast<f32>(*angle) * scale);
        return angle;
    }

    s32* AddRadiansToAngle(s32* angle, f32 radians)
    {
        *angle = *angle + static_cast<s32>(radians * RadiansToAngle);
        return angle;
    }

    void CosSin16(const s32* angle, f32* cosine, f32* sine)
    {
        f32 sinCos[2];
        SinCos16(*angle, sinCos);
        *cosine = sinCos[1];
        *sine = sinCos[0];
    }

    f32 SinOfAngle(const s32* angle)
    {
        f32 cosine;
        f32 sine;
        CosSin16(angle, &cosine, &sine);
        return sine;
    }

    f32 CosOfAngle(const s32* angle)
    {
        f32 cosine;
        f32 sine;
        CosSin16(angle, &cosine, &sine);
        return cosine;
    }

    void MatrixRotationZ(Matrix4x4* matrix, const s32* angle)
    {
        if (*angle == 0)
        {
            InitIdentityMatrix(matrix);
            return;
        }

        f32 sinCos[2];
        SinCos16(*angle, sinCos);
        *matrix = {};
        matrix->m[0][0] = sinCos[1];
        matrix->m[0][1] = sinCos[0];
        matrix->m[1][0] = -sinCos[0];
        matrix->m[1][1] = sinCos[1];
        matrix->m[2][2] = 1.0f;
        matrix->m[3][3] = 1.0f;
    }

    void MatrixScaleRows(Matrix4x4* matrix, const Vector4* scale)
    {
        const f32 factors[3] = {scale->x, scale->y, scale->z};
        for (u32 row = 0; row < 3; row++)
        {
            for (u32 column = 0; column < 3; column++)
            {
                matrix->m[row][column] = matrix->m[row][column] * factors[row];
            }
        }
    }

    void MatrixScaleColumns(Matrix4x4* matrix, const Vector4* scale)
    {
        const f32 factors[3] = {scale->x, scale->y, scale->z};
        for (auto& row : matrix->m)
        {
            for (u32 column = 0; column < 3; column++)
            {
                row[column] = row[column] * factors[column];
            }
        }
    }

    void SampleVector2Curve(const Vector2Curve* curve, f32 t, Vector2* out)
    {
        if (!(t < 1.0f))
        {
            *out = curve->points[curve->count - 1];
            return;
        }

        f32 place = static_cast<f32>(static_cast<s32>(curve->count - 1)) * t;
        s32 index = static_cast<s32>(place);
        f32 fraction = place - static_cast<f32>(index);
        const Vector2& from = curve->points[index];
        const Vector2& to = curve->points[index + 1];
        out->x = from.x + (to.x - from.x) * fraction;
        out->y = from.y + (to.y - from.y) * fraction;
    }

    void SampleVector4Curve(const Vector4Curve* curve, f32 t, Vector4* out)
    {
        if (!(t < 1.0f))
        {
            *out = curve->points[curve->count - 1];
            return;
        }

        f32 place = static_cast<f32>(static_cast<s32>(curve->count - 1)) * t;
        s32 index = static_cast<s32>(place);
        f32 fraction = place - static_cast<f32>(index);
        const Vector4& from = curve->points[index];
        const Vector4& to = curve->points[index + 1];
        out->x = from.x + (to.x - from.x) * fraction;
        out->y = from.y + (to.y - from.y) * fraction;
        out->z = from.z + (to.z - from.z) * fraction;
        out->w = from.w + (to.w - from.w) * fraction;
    }

    Vector4* CopyVector4(Vector4* out, const Vector4* vector)
    {
        out->x = vector->x;
        out->y = vector->y;
        out->z = vector->z;
        out->w = vector->w;
        return out;
    }

    f32 GetRandFloat()
    {
        return static_cast<f32>(GetRand()) * RandScale;
    }

    f32 RandomSigned()
    {
        return (static_cast<f32>(GetRand()) - RandMiddle) * SignedRandScale;
    }

    f32 RandomSignedTimes(f32 range)
    {
        return (static_cast<f32>(GetRand()) - RandMiddle) * SignedRandScale * range;
    }

    s32 NextPowerOfTwo(s32 value)
    {
        s32 power = 1;
        while (power < value)
        {
            power = static_cast<s32>(static_cast<u32>(power) << 1);
        }

        return power;
    }

    void TransformPoint(Vector4* out, const Vector4* point, const Matrix4x4* matrix)
    {
        const auto& m = matrix->m;
        f32 x = point->x * m[0][0] + point->y * m[1][0] + point->z * m[2][0] + m[3][0];
        f32 y = point->x * m[0][1] + point->y * m[1][1] + point->z * m[2][1] + m[3][1];
        f32 z = point->x * m[0][2] + point->y * m[1][2] + point->z * m[2][2] + m[3][2];
        out->x = x;
        out->y = y;
        out->z = z;
    }

    void TransformVector(Vector4* out, const Vector4* vector, const Matrix4x4* matrix)
    {
        const auto& m = matrix->m;
        f32 x = vector->x * m[0][0] + vector->y * m[1][0] + vector->z * m[2][0];
        f32 y = vector->x * m[0][1] + vector->y * m[1][1] + vector->z * m[2][1];
        f32 z = vector->x * m[0][2] + vector->y * m[1][2] + vector->z * m[2][2];
        out->x = x;
        out->y = y;
        out->z = z;
    }

    void RotateVectorY(Vector4* out, const Vector4* vector, s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        f32 x = vector->x;
        f32 y = vector->y;
        f32 z = vector->z;
        out->x = x * sinCos[1] + z * sinCos[0];
        out->y = y;
        out->z = z * sinCos[1] - x * sinCos[0];
    }

    void RotateVectorZ(Vector4* out, const Vector4* vector, s32 angle)
    {
        f32 sinCos[2];
        SinCos16(angle, sinCos);
        f32 x = vector->x;
        f32 y = vector->y;
        f32 z = vector->z;
        out->x = x * sinCos[1] - y * sinCos[0];
        out->y = x * sinCos[0] + y * sinCos[1];
        out->z = z;
    }

    void ScaleVector(Vector4* out, const Vector4* vector, f32 scale)
    {
        out->x = vector->x * scale;
        out->y = vector->y * scale;
        out->z = vector->z * scale;
    }

    void SubtractVectors(Vector4* out, const Vector4* from, const Vector4* vector)
    {
        out->x = from->x - vector->x;
        out->y = from->y - vector->y;
        out->z = from->z - vector->z;
    }

    f32 SquaredDistance(const Vector4* a, const Vector4* b)
    {
        f32 x = a->x - b->x;
        f32 y = a->y - b->y;
        f32 z = a->z - b->z;
        return x * x + y * y + z * z;
    }

    f32 DotProduct(const Vector4* a, const Vector4* b)
    {
        return a->x * b->x + a->y * b->y + a->z * b->z;
    }

    f32 InverseLength(const Vector4* vector, f32 epsilon)
    {
        f32 squared = vector->x * vector->x + vector->y * vector->y + vector->z * vector->z;
        if (epsilon < squared)
        {
            // A square root and a division: GCC makes them one RSQRT.S otherwise, which rounds differently
            f32 root = Kept(__builtin_sqrtf(squared));
            return 1.0f / root;
        }

        return 0.0f;
    }
}

EABI_EXPORT(FUN_002c6fb0, ScaleVector);
EABI_EXPORT(FUN_0011c028, InverseLength4);
EABI_EXPORT(RSQRT, InverseLength);
EABI_EXPORT(FUN_0018d9d0, PlaneSide);
EABI_EXPORT(FUN_0018e310, ScaleAlongAxis);
EABI_EXPORT(FUN_0018d530, ScaleRotation);
EABI_EXPORT(FUN_0018d4e8, AddToDiagonal);
EABI_EXPORT(FUN_0011bf10, AngleFrom);
EABI_EXPORT(FUN_0011bf78, DivideAngle);
EABI_EXPORT(FUN_0015de00, MultiplyAngle);
EABI_EXPORT(FUN_0015ddd0, AddRadiansToAngle);
EABI_EXPORT(FUN_0018bec8, AngleOfCosine);
EABI_EXPORT(ArcSin16, AngleOfSine);
EABI_EXPORT(FUN_0018ca78, SolveQuadratic);
EABI_EXPORT(FUN_0018cb38, PowerOf);
EABI_EXPORT(FUN_0018ccd8, NormalizePair);
EABI_EXPORT(FUN_0018ce00, TurnPair);
EABI_EXPORT(FUN_0018d0d8, LinePointAt);
EABI_EXPORT(FUN_0018d1f0, LineAtLeast);
EABI_EXPORT(FUN_0018dfa8, RotationFromAxisSine);
EABI_EXPORT(FUN_0018e608, AreParallel);
EABI_EXPORT(FUN_001830a8, AngleOfPoint);
EABI_EXPORT(FUN_001857e8, MatrixAboutAxis);
EABI_EXPORT(FUN_0017c668, SampleVector2Curve);
EABI_EXPORT(FUN_001ac110, SampleVector4Curve);

u32 Box::Contains(const Vector4* point) const
{
    if (point->x < min.x || point->y < min.y || point->z < min.z)
    {
        return 0;
    }

    if (max.x < point->x || max.y < point->y || max.z < point->z)
    {
        return 0;
    }

    return 1;
}

f32 InverseLength4(f32 fallback, f32 epsilon, const Vector4* vector)
{
    f32 squared = vector->x * vector->x + vector->y * vector->y + vector->z * vector->z + vector->w * vector->w;
    if (epsilon * epsilon < squared)
    {
        // One RSQRT.S, as retail
        return 1.0f / __builtin_sqrtf(squared);
    }

    return fallback;
}

Vector4* PlaneThrough(Vector4* plane, const Vector4* normal, const Vector4* point)
{
    *plane = *normal;
    plane->w = -(normal->x * point->x + normal->y * point->y + normal->z * point->z);
    return plane;
}

void ProjectOntoPlane(const Vector4* plane, const Vector4* point, Vector4* out)
{
    f32 distance = plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
    out->x = out->x - plane->x * distance;
    out->y = out->y - plane->y * distance;
    out->z = out->z - plane->z * distance;
}

void MatrixFromRows(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z, const Vector4* w)
{
    *reinterpret_cast<Vector4*>(matrix->m[0]) = *x;
    *reinterpret_cast<Vector4*>(matrix->m[1]) = *y;
    *reinterpret_cast<Vector4*>(matrix->m[2]) = *z;
    *reinterpret_cast<Vector4*>(matrix->m[3]) = *w;
}

void ScaleAlongAxis(f32 share, Vector4* vector, const Vector4* axis, u32 unitAxis)
{
    f32 along = vector->x * axis->x + vector->y * axis->y + vector->z * axis->z;
    if (along == 0.0f)
    {
        return;
    }

    Vector4 taken = *axis;
    f32 rest = 1.0f - share;
    taken.x = taken.x * rest;
    taken.y = taken.y * rest;
    taken.z = taken.z * rest;
    if (unitAxis == 0)
    {
        f32 lengthSquared = axis->x * axis->x + axis->y * axis->y + axis->z * axis->z;
        f32 inverse = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            inverse = 1.0f / lengthSquared;
        }

        along = along * inverse;
    }

    vector->x = vector->x - taken.x * along;
    vector->y = vector->y - taken.y * along;
    vector->z = vector->z - taken.z * along;
}

void MultiplyInPlace(Matrix4x4* matrix, const Matrix4x4* by)
{
    Matrix4x4 product;
    VuMultiplyMatrices(matrix, by, &product);
    *matrix = product;
}

void TransformPlane(const Vector4* plane, const Matrix4x4* matrix, Vector4* out)
{
    VuRotateVector(matrix, plane, out);
    const Vector4* position = RowOf(matrix, 3);
    out->w = out->w - (out->x * position->x + out->y * position->y + out->z * position->z);
}

void TransposeInPlace(Matrix4x4* matrix, s32 size)
{
    f32 kept = matrix->m[0][1];
    matrix->m[0][1] = matrix->m[1][0];
    matrix->m[1][0] = kept;
    kept = matrix->m[0][2];
    matrix->m[0][2] = matrix->m[2][0];
    matrix->m[2][0] = kept;
    kept = matrix->m[1][2];
    matrix->m[1][2] = matrix->m[2][1];
    matrix->m[2][1] = kept;
    if (size != 4)
    {
        return;
    }

    kept = matrix->m[0][3];
    matrix->m[0][3] = matrix->m[3][0];
    matrix->m[3][0] = kept;
    kept = matrix->m[1][3];
    matrix->m[1][3] = matrix->m[3][1];
    matrix->m[3][1] = kept;
    kept = matrix->m[2][3];
    matrix->m[2][3] = matrix->m[3][2];
    matrix->m[3][2] = kept;
}

void SortThree(const f32* values, s32* largest, s32* middle, s32* smallest)
{
    *largest = 0;
    *smallest = 1;
    *middle = 2;
    if (values[0] < values[1])
    {
        *largest = 1;
        *smallest = 0;
    }

    if (values[*largest] < values[2])
    {
        *middle = *largest;
        *largest = 2;
        return;
    }

    s32 low = *smallest;
    *middle = low;
    if (values[2] < values[low])
    {
        *smallest = 2;
    }
}

void OuterProduct(Matrix4x4* out, const f32* column, const Vector4* row)
{
    for (u32 index = 0; index < 3; index++)
    {
        Vector4 line;
        line.x = row->x * column[index];
        line.y = row->y * column[index];
        line.z = row->z * column[index];
        line.w = 1.0f;
        *reinterpret_cast<Vector4*>(out->m[index]) = line;
    }

    out->m[3][2] = 0.0f;
    out->m[3][3] = 1.0f;
    out->m[3][1] = 0.0f;
    out->m[3][0] = 0.0f;
    out->m[2][3] = 0.0f;
    out->m[1][3] = 0.0f;
    out->m[0][3] = 0.0f;
}

void ScaleRotation(f32 scale, Matrix4x4* matrix)
{
    for (u32 index = 0; index < 3; index++)
    {
        matrix->m[index][0] = matrix->m[index][0] * scale;
        matrix->m[index][1] = matrix->m[index][1] * scale;
        matrix->m[index][2] = matrix->m[index][2] * scale;
    }
}

void AddRotation(Matrix4x4* matrix, const Matrix4x4* add)
{
    for (u32 index = 0; index < 3; index++)
    {
        matrix->m[index][0] = matrix->m[index][0] + add->m[index][0];
        matrix->m[index][1] = matrix->m[index][1] + add->m[index][1];
        matrix->m[index][2] = matrix->m[index][2] + add->m[index][2];
    }
}

void AddToDiagonal(f32 value, Matrix4x4* matrix, u32 all)
{
    matrix->m[0][0] = matrix->m[0][0] + value;
    matrix->m[1][1] = matrix->m[1][1] + value;
    matrix->m[2][2] = matrix->m[2][2] + value;
    if (all != 0)
    {
        matrix->m[3][3] = matrix->m[3][3] + value;
    }
}

void TransposeMatrix(const Matrix4x4* matrix, s32 size, Matrix4x4* out)
{
    // An element at a time, in retail's order (nothing calls it in place)
    out->m[0][0] = matrix->m[0][0];
    out->m[0][1] = matrix->m[1][0];
    out->m[0][2] = matrix->m[2][0];
    out->m[1][0] = matrix->m[0][1];
    out->m[1][1] = matrix->m[1][1];
    out->m[1][2] = matrix->m[2][1];
    out->m[2][0] = matrix->m[0][2];
    out->m[2][1] = matrix->m[1][2];
    out->m[2][2] = matrix->m[2][2];
    if (size == 3)
    {
        out->m[3][2] = 0.0f;
        out->m[3][3] = 1.0f;
        out->m[3][1] = 0.0f;
        out->m[3][0] = 0.0f;
        out->m[2][3] = 0.0f;
        out->m[1][3] = 0.0f;
        out->m[0][3] = 0.0f;
        return;
    }

    out->m[0][3] = matrix->m[3][0];
    out->m[1][3] = matrix->m[3][1];
    out->m[2][3] = matrix->m[3][2];
    out->m[3][0] = matrix->m[0][3];
    out->m[3][1] = matrix->m[1][3];
    out->m[3][2] = matrix->m[2][3];
    out->m[3][3] = matrix->m[3][3];
}

namespace
{
bool TooShort(const Vector4* vector)
{
    return __builtin_fabsf(vector->x) <= Epsilon && __builtin_fabsf(vector->y) <= Epsilon && __builtin_fabsf(vector->z) <= Epsilon;
}
}

void PlaneFromTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third)
{
    Vector4 toSecond = *second;
    Vector4 toThird = *third;
    toThird.x = toThird.x - first->x;
    toSecond.y = toSecond.y - first->y;
    toSecond.x = toSecond.x - first->x;
    toSecond.z = toSecond.z - first->z;
    toThird.y = toThird.y - first->y;
    toThird.z = toThird.z - first->z;
    Vector4 normal;
    normal.x = toSecond.y * toThird.z - toSecond.z * toThird.y;
    normal.y = toSecond.z * toThird.x - toSecond.x * toThird.z;
    normal.z = toSecond.x * toThird.y - toSecond.y * toThird.x;
    normal.w = 1.0f;
    if (TooShort(&normal))
    {
        if (!TooShort(&toSecond))
        {
            PerpendicularOf(&toSecond, &normal);
        }
        else if (!TooShort(&toThird))
        {
            PerpendicularOf(&toThird, &normal);
        }
        else
        {
            normal = g_YAxis;
        }
    }

    f32 inverse = InverseLength(&normal, LengthEpsilon);
    f32 x = normal.x * inverse;
    f32 y = normal.y * inverse;
    f32 z = normal.z * inverse;
    plane->w = 1.0f;
    plane->x = x;
    plane->y = y;
    plane->z = z;
    plane->w = -(x * first->x + y * first->y + z * first->z);
}

u32 PerpendicularOf(const Vector4* vector, Vector4* out)
{
    // A vector this short (squared) has no perpendicular
    constexpr f32 PerpendicularEpsilon = Rounded(1e-20);
    f32 x = vector->x;
    f32 y = vector->y;
    f32 z = vector->z;
    f32 xx = x * x;
    f32 zz = z * z;
    if (!(PerpendicularEpsilon < xx + y * y + zz))
    {
        *out = *vector;
        return 0;
    }

    if (zz <= xx)
    {
        out->z = 0.0f;
        out->w = 1.0f;
        out->y = x * 3.0f;
        out->x = -y * 3.0f;
    }
    else
    {
        out->x = 0.0f;
        out->w = 1.0f;
        out->y = z * 3.0f;
        out->z = -y * 3.0f;
    }

    return 1;
}

u32 PlaneSide(f32 epsilon, const Vector4* plane, const Vector4* point)
{
    f32 distance = plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
    if (!(distance < -epsilon) && !(epsilon < distance))
    {
        return OnPlane;
    }

    return 0.0f < distance ? InFrontOfPlane : BehindPlane;
}

void HalfAngleSinCos(f32* sine, f32* cosine)
{
    // Both halves of the angle's cosine get their own sums
    if (*sine < 0.0f)
    {
        *sine = __builtin_sqrtf((1.0f - *cosine) * 0.5f);
        *cosine = -__builtin_sqrtf((*cosine + 1.0f) * 0.5f);
        return;
    }

    *sine = __builtin_sqrtf((1.0f - *cosine) * 0.5f);
    *cosine = __builtin_sqrtf(*cosine * 0.5f + 0.5f);
}

void AxisAngleOfRotation(Vector4* rotation, Vector4* axis, s32* angle, u32 normalized)
{
    if (normalized == 0)
    {
        f32 squared = rotation->x * rotation->x + rotation->y * rotation->y + rotation->z * rotation->z + rotation->w * rotation->w;
        f32 inverse = 0.0f;
        if (InverseEpsilonSquared < squared)
        {
            // One RSQRT.S, as retail
            inverse = 1.0f / __builtin_sqrtf(squared);
        }

        rotation->x = rotation->x * inverse;
        rotation->y = rotation->y * inverse;
        rotation->z = rotation->z * inverse;
        rotation->w = rotation->w * inverse;
    }

    f32 w = rotation->w;
    f32 size = __builtin_fabsf(w);
    if (!(size < 1.0f))
    {
        *angle = 0;
        *axis = g_YAxis;
        return;
    }

    f32 rest = 1.0f - size;
    if (rest < 0.0f)
    {
        rest = 0.0f;
    }

    // Half the angle, in whole 65536ths of a turn before it's doubled
    f32 half = ArcCosineOfPositive(size) * __builtin_sqrtf(rest);
    if (w < 0.0f)
    {
        half = Pi - half;
    }

    f32 halfAngle = static_cast<f32>(static_cast<s32>(half * RadiansToAngle));
    *angle = static_cast<s32>(halfAngle + halfAngle);
    f32 x = rotation->x;
    f32 y = rotation->y;
    f32 z = rotation->z;
    axis->x = x;
    axis->y = y;
    axis->z = z;
    axis->w = 1.0f;
    f32 scale = 1.0f / Kept(__builtin_sqrtf(1.0f - rotation->w * rotation->w));
    axis->z = z * scale;
    axis->x = x * scale;
    axis->y = y * scale;
    f32 inverse = InverseLength(axis, LengthEpsilon);
    axis->x = axis->x * inverse;
    axis->y = axis->y * inverse;
    axis->z = axis->z * inverse;
}

void InitMathConstants(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_ZAxis = {0.0f, 0.0f, 1.0f, 1.0f};
    g_DefaultBox.min = {0.0f, 0.0f, 0.0f, 1.0f};
    g_DefaultBox.max = {1.0f, 1.0f, 1.0f, 0.0f};
    g_JointScale = {1.0f, 1.0f, 1.0f, 1.0f};
    g_XAxis = {1.0f, 0.0f, 0.0f, 1.0f};
    g_YAxis = {0.0f, 1.0f, 0.0f, 1.0f};
}

namespace
{
f32 RootOfComplement(f32 value)
{
    f32 complement = 1.0f - value;
    if (complement < 0.0f)
    {
        complement = 0.0f;
    }

    return __builtin_sqrtf(complement);
}

// Made a unit vector unless its length squared is within a tolerance of 1 (squared)
void NormalizeTwo(f32* x, f32* y, f32 tolerance)
{
    f32 lengthSquared = *x * *x + *y * *y;
    f32 off = lengthSquared - 1.0f;
    if (off * off <= tolerance)
    {
        return;
    }

    f32 length = __builtin_sqrtf(lengthSquared);
    f32 scale = 0.0f;
    if (DivisionEpsilon < length || length < -DivisionEpsilon)
    {
        scale = 1.0f / length;
    }

    *x = *x * scale;
    *y = *y * scale;
}
}

s32* AngleOfCosine(f32 cosine, s32* angle)
{
    f32 value = __builtin_fabsf(cosine);
    f32 root = RootOfComplement(value);
    f32 radians = ArcCosineOfPositive(value) * root;
    if (cosine < 0.0f)
    {
        radians = Pi - radians;
    }

    *angle = static_cast<s32>(radians * RadiansToAngle);
    return angle;
}

s32* AngleOfSine(f32 sine, s32* angle)
{
    f32 value = __builtin_fabsf(sine);
    f32 root = RootOfComplement(value);
    f32 radians = HalfPi - root * ArcCosineOfPositive(value);
    if (sine < 0.0f)
    {
        radians = -radians;
    }

    *angle = static_cast<s32>(radians * RadiansToAngle);
    return angle;
}

void MatrixFromColumns(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z, const Vector4* w)
{
    const Vector4* columns[] = {x, y, z, w};
    for (u32 row = 0; row < 4; row++)
    {
        for (u32 column = 0; column < 4; column++)
        {
            matrix->m[row][column] = (&columns[column]->x)[row];
        }
    }
}

void TurnAboutY(const Matrix4x4* matrix, const Vector4* vector, Vector4* out)
{
    f32 x = vector->x;
    f32 z = vector->z;
    out->x = x * matrix->m[0][0] + z * matrix->m[2][0];
    out->y = vector->y;
    out->z = x * matrix->m[0][2] + z * matrix->m[2][2];
    out->w = vector->w;
}

void MoveAlongAxes(Matrix4x4* matrix, const Vector4* move)
{
    Vector4 turned = *move;
    VuRotateVector(matrix, &turned, &turned);
    Vector4* position = RowOf(matrix, 3);
    position->x = position->x + turned.x;
    position->y = position->y + turned.y;
    position->z = position->z + turned.z;
}

void AddMatrixPosition(const Matrix4x4* matrix, const Vector4* vector, Vector4* out)
{
    const Vector4* position = RowOf(matrix, 3);
    out->x = position->x + vector->x;
    out->y = position->y + vector->y;
    out->z = position->z + vector->z;
    out->w = vector->w;
}

s32 RandomInt()
{
    return GetRand();
}

s32 RandomBelow(s32 count)
{
    if (count < 2)
    {
        return 0;
    }

    return GetRand() % count;
}

s32 RandomFrom(s32 low, s32 count)
{
    s32 random = 0;
    if (!(count < 2))
    {
        random = GetRand() % count;
    }

    return low + random;
}

f32 RandomBelowFloat(f32 range)
{
    return static_cast<f32>(GetRand()) * RandScale * range;
}

f32 RandomFromFloat(f32 low, f32 range)
{
    return low + static_cast<f32>(GetRand()) * RandScale * range;
}

f32 RandomAround(f32 centre, f32 range)
{
    return centre + (static_cast<f32>(GetRand()) - RandMiddle) * SignedRandScale * range;
}

s32 SolveQuadratic(f32* roots, f32 a, f32 b, f32 c)
{
    f32 discriminant = b * b - a * (c * 4.0f);
    if (discriminant < 0.0f)
    {
        roots[0] = 0.0f;
        roots[1] = 0.0f;
        return 0;
    }

    f32 sign = b < 0.0f ? -1.0f : 1.0f;
    f32 q = (b + sign * __builtin_sqrtf(discriminant)) * -0.5f;
    s32 count = 0;
    if (a != 0.0f)
    {
        roots[count] = q / a;
        count++;
    }

    if (q != 0.0f)
    {
        roots[count] = c / q;
        count++;
    }

    return count;
}

f32 PowerOf(f32 base, s32 exponent)
{
    if (exponent < 0)
    {
        exponent = -exponent;
        if (base != 0.0f)
        {
            base = 1.0f / base;
        }
    }

    if (base == 0.0f)
    {
        return 0.0f;
    }

    f32 result = 1.0f;
    while (exponent != 0)
    {
        if ((exponent & 1) != 0)
        {
            result = result * base;
        }

        exponent >>= 1;
        base = base * base;
    }

    return result;
}

f32 PositivePart(f32 value, f32 scale)
{
    if (!(0.0f <= value))
    {
        return 0.0f;
    }

    if (!(value <= 0.0f))
    {
        return value;
    }

    return value * scale;
}

void NormalizePair(f32* x, f32* y, f32 tolerance)
{
    NormalizeTwo(x, y, tolerance);
}

void TurnPair(f32* x, f32* y, f32 cosine, f32 sine, f32 tolerance)
{
    f32 oldX = *x;
    f32 oldY = *y;
    *y = oldY * sine - oldX * cosine;
    *x = oldY * cosine + oldX * sine;
    NormalizeTwo(x, y, tolerance);
}

f32 OverlappingRangesSpan(f32 firstLow, f32 firstHigh, f32 secondLow, f32 secondHigh)
{
    if (!(secondLow < firstHigh) || !(firstLow < secondHigh))
    {
        return 0.0f;
    }

    return __builtin_fabsf(__builtin_fminf(secondLow, firstLow) - __builtin_fmaxf(secondHigh, firstHigh));
}

void YawOfDirection(s32* yaw, const Vector4* direction)
{
    Vector4 forward = {0.0f, 0.0f, 1.0f, 1.0f};
    Vector4 flat = {direction->x, 0.0f, direction->z, 1.0f};
    SignedAngleAboutY(yaw, &forward, &flat);
}

void PitchOfDirection(s32* pitch, const Vector4* direction)
{
    Vector4 forward = {0.0f, 0.0f, 1.0f, 1.0f};
    Vector4 side = {0.0f, direction->y, direction->z, 1.0f};
    SignedAngleAboutX(pitch, &forward, &side);
}

s32* SignedAngleAboutX(s32* angle, const Vector4* from, const Vector4* to)
{
    SignedAngleAbout(angle, from, to, 0);
    return angle;
}

s32* SignedAngleAboutY(s32* angle, const Vector4* from, const Vector4* to)
{
    SignedAngleAbout(angle, from, to, 1);
    return angle;
}

void LinePointAt(f32 along, const Vector4* line, Vector4* out)
{
    if (along == 0.0f)
    {
        *out = line[0];
        return;
    }

    if (along == 1.0f)
    {
        *out = line[1];
        return;
    }

    *out = line[1];
    out->x = (out->x - line[0].x) * along + line[0].x;
    out->y = (out->y - line[0].y) * along + line[0].y;
    out->z = (out->z - line[0].z) * along + line[0].z;
}

f32 LineDotDifference(const Vector4* line, const Vector4* point)
{
    f32 end = line[1].x * point->x + line[1].y * point->y + line[1].z * point->z;
    f32 start = line[0].x * point->x + line[0].y * point->y + line[0].z * point->z;
    return end - start;
}

u32 LineAtLeast(const Vector4* line, f32* lengthSquared, f32 length, f32 scale)
{
    f32 x = line[0].x - line[1].x;
    f32 y = line[0].y - line[1].y;
    f32 z = line[0].z - line[1].z;
    f32 squared = x * x + y * y + z * z;
    if (lengthSquared != nullptr)
    {
        *lengthSquared = squared;
    }

    return !(squared < length * scale);
}

s32 TruncateToInt(f32 value)
{
    return static_cast<s32>(value);
}

f32 LengthSquared(const Vector4* vector)
{
    return vector->x * vector->x + vector->y * vector->y + vector->z * vector->z;
}

void MatrixFromAxes(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z)
{
    const Vector4* axes[] = {x, y, z};
    for (u32 row = 0; row < 3; row++)
    {
        matrix->m[row][0] = axes[row]->x;
        matrix->m[row][1] = axes[row]->y;
        matrix->m[row][2] = axes[row]->z;
        matrix->m[row][3] = 0.0f;
    }

    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}

void SetMatrixPosition(Matrix4x4* matrix, const Vector4* position)
{
    matrix->m[3][0] = position->x;
    matrix->m[3][1] = position->y;
    matrix->m[3][2] = position->z;
}

void PreMultiply(Matrix4x4* matrix, const Matrix4x4* by)
{
    Matrix4x4 product;
    VuMultiplyMatrices(by, matrix, &product);
    *matrix = product;
}

void MultiplyReversed(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out)
{
    VuMultiplyMatrices(b, a, out);
}

void AnglesOfMatrix(const Matrix4x4* matrix, s32* angles)
{
    s32 x;
    s32 y;
    s32 z;
    EulerAnglesOfMatrix(matrix, &x, &y, &z);
    angles[2] = z;
    angles[0] = x;
    angles[1] = y;
}

void TurnMatrix(Matrix4x4* matrix, const Vector4* rotation)
{
    Matrix4x4 turn;
    MatrixFromRotation(&turn, rotation);
    turn.m[3][2] = 0.0f;
    turn.m[3][3] = 1.0f;
    turn.m[3][1] = 0.0f;
    turn.m[3][0] = 0.0f;
    turn.m[2][3] = 0.0f;
    turn.m[1][3] = 0.0f;
    turn.m[0][3] = 0.0f;
    Matrix4x4 product;
    VuMultiplyMatrices(&turn, matrix, &product);
    *matrix = product;
}

void AnglesOfRotation(const Vector4* rotation, s32* x, s32* y, s32* z)
{
    Matrix4x4 matrix;
    MatrixFromRotation(&matrix, rotation);
    EulerAnglesOfMatrix(&matrix, x, y, z);
}

u32 PlaneSideOf(const Vector4* plane, const Vector4* point)
{
    f32 distance = plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
    if (0.0f < distance)
    {
        return InFrontOfPlane;
    }

    if (distance < 0.0f)
    {
        return BehindPlane;
    }

    return OnPlane;
}

void PlaneAlongNormal(const Vector4* normal, const Vector4* point, Vector4* plane)
{
    *plane = *normal;
    plane->w = plane->w - (normal->x * point->x + normal->y * point->y + normal->z * point->z);
}

void TransformPlaneInPlace(Vector4* plane, const Matrix4x4* matrix)
{
    VuRotateVector(matrix, plane, plane);
    const Vector4* position = RowOf(matrix, 3);
    plane->w = plane->w - (plane->x * position->x + plane->y * position->y + plane->z * position->z);
}

void ProjectOntoPlaneInPlace(const Vector4* plane, Vector4* point)
{
    f32 distance = plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
    Vector4 normal = *plane;
    point->z = point->z - normal.z * distance;
    point->x = point->x - normal.x * distance;
    point->y = point->y - normal.y * distance;
}

void RotationFromAxisSine(f32 sine, f32 cosine, Vector4* rotation, const Vector4* axis, u32 unitAxis)
{
    f32 scale = sine;
    if (unitAxis == 0)
    {
        f32 lengthSquared = axis->x * axis->x + axis->y * axis->y + axis->z * axis->z;
        f32 inverse = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            inverse = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
        }

        scale = sine * inverse;
    }

    rotation->w = cosine;
    rotation->x = scale * axis->x;
    rotation->y = scale * axis->y;
    rotation->z = scale * axis->z;
}

namespace
{
// A vector less its part along a normal times a factor (the normal made a unit one when it isn't)
void TakeAlong(Vector4* vector, const Vector4* normal, u32 unitNormal, f32 factor)
{
    f32 along = vector->x * normal->x + vector->y * normal->y + vector->z * normal->z;
    if (along == 0.0f)
    {
        return;
    }

    Vector4 axis = *normal;
    along = along * factor;
    if (unitNormal == 0)
    {
        f32 lengthSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        f32 inverse = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            inverse = 1.0f / lengthSquared;
        }

        along = along * inverse;
    }

    vector->y = vector->y - axis.y * along;
    vector->z = vector->z - axis.z * along;
    vector->x = vector->x - axis.x * along;
}
}

void ReflectAcross(Vector4* vector, const Vector4* normal, u32 unitNormal)
{
    TakeAlong(vector, normal, unitNormal, 2.0f);
}

void RemoveComponentAlong(Vector4* vector, const Vector4* normal, u32 unitNormal)
{
    TakeAlong(vector, normal, unitNormal, 1.0f);
}

f32 TripleProduct(const Vector4* a, const Vector4* b, const Vector4* c, Vector4* terms)
{
    f32 crossX = b->y * c->z - c->y * b->z;
    f32 crossY = b->z * c->x - c->z * b->x;
    f32 crossZ = b->x * c->y - c->x * b->y;
    f32 x = a->x * crossX;
    f32 y = a->y * crossY;
    f32 z = a->z * crossZ;
    if (terms != nullptr)
    {
        *terms = {x, y, z, 1.0f};
    }

    return x + y + z;
}

u32 AreParallel(f32 tolerance, const Vector4* a, const Vector4* b, f32* dot)
{
    f32 along = a->x * b->x + a->y * b->y + a->z * b->z;
    if (dot != nullptr)
    {
        *dot = along;
    }

    f32 alongSquared = along * along;
    f32 allowed = tolerance * tolerance * alongSquared;
    f32 aSquared = a->x * a->x + a->y * a->y + a->z * a->z;
    f32 bSquared = b->x * b->x + b->y * b->y + b->z * b->z;
    return __builtin_fabsf(aSquared * bSquared - alongSquared) <= allowed;
}

void GetRotationVec(Vector4* rotation, const Matrix4x4* matrix)
{
    const auto& m = matrix->m;
    f32 trace = m[0][0] + m[1][1] + m[2][2];
    if (0.0f < trace)
    {
        f32 root = Kept(__builtin_sqrtf(trace + 1.0f));
        rotation->w = root * 0.5f;
        root = 0.5f / root;
        rotation->x = (m[1][2] - m[2][1]) * root;
        rotation->y = (m[2][0] - m[0][2]) * root;
        rotation->z = (m[0][1] - m[1][0]) * root;
        return;
    }

    // From the largest of the diagonal
    s32 i = 0;
    if (m[0][0] < m[1][1])
    {
        i = 1;
    }

    if (m[i][i] < m[2][2])
    {
        i = 2;
    }

    s32 j = NextAxis[i];
    s32 k = NextAxis[j];
    f32 quaternion[4];
    f32 root = Kept(__builtin_sqrtf(m[i][i] - (m[j][j] + m[k][k]) + 1.0f));
    quaternion[i] = root * 0.5f;
    if (root != 0.0f)
    {
        root = 0.5f / root;
    }

    quaternion[3] = (m[j][k] - m[k][j]) * root;
    quaternion[j] = (m[i][j] + m[j][i]) * root;
    quaternion[k] = (m[i][k] + m[k][i]) * root;
    rotation->x = quaternion[0];
    rotation->w = quaternion[3];
    rotation->y = quaternion[1];
    rotation->z = quaternion[2];
}

s32* SignedAngleAbout(s32* angle, const Vector4* from, const Vector4* to, u32 axis)
{
    f32 fromSquared = from->x * from->x + from->y * from->y + from->z * from->z;
    f32 toSquared = to->x * to->x + to->y * to->y + to->z * to->z;
    f32 lengths = Kept(__builtin_sqrtf(fromSquared * toSquared));
    if (lengths != 0.0f)
    {
        lengths = 1.0f / lengths;
    }

    f32 cosine = (from->x * to->x + from->y * to->y + from->z * to->z) * lengths;
    s32 result;
    AngleOfCosine(cosine, &result);
    if (result == 0 || result == HalfTurnAngle)
    {
        *angle = result;
        return angle;
    }

    Vector4 cross = {from->y * to->z - from->z * to->y, from->z * to->x - from->x * to->z, from->x * to->y - from->y * to->x,
                     from->w};
    bool parallel =
        __builtin_fabsf(cross.x) <= Epsilon && __builtin_fabsf(cross.y) <= Epsilon && __builtin_fabsf(cross.z) <= Epsilon;
    if (parallel)
    {
        AngleFrom(&result, result < QuarterTurnAngle ? 0.0f : Pi, AngleRadians);
        *angle = result;
        return angle;
    }

    f32 along = (&cross.x)[axis];
    if (__builtin_fabsf(along) <= Epsilon)
    {
        AngleFrom(&result, 0.0f, AngleRadians);
        *angle = result;
        return angle;
    }

    f32 sign = along < 0.0f ? -1.0f : 1.0f;
    result = static_cast<s32>(static_cast<f32>(result) * sign);
    *angle = result;
    return angle;
}

s32* AngleOfPoint(s32* angle, f32 y, f32 x)
{
    constexpr f32 A4 = 0x1.555cbep-6f;
    constexpr f32 A3 = 0x1.5cb46cp-4f;
    constexpr f32 A2 = 0x1.70edc4p-3f;
    constexpr f32 A1 = 0x1.523a08p-2f;
    constexpr f32 A0 = 0x1.ffee7p-1f;
    // atan2(y, x) is twice atan(y / (r + x))
    f32 beyond = Kept(__builtin_sqrtf(x * x + y * y)) + x;
    if (!(0.0f < beyond))
    {
        *angle = HalfTurnAngle;
        return angle;
    }

    f32 tangent = y / beyond;
    f32 squared = tangent * tangent;
    f32 half;
    if (squared < 1.0f)
    {
        half = ((((squared * A4 - A3) * squared + A2) * squared - A1) * squared + A0) * tangent;
    }
    else
    {
        f32 inverse = 1.0f / tangent;
        f32 inverseSquared = inverse * inverse;
        f32 rest = ((((inverseSquared * A4 - A3) * inverseSquared + A2) * inverseSquared - A1) * inverseSquared + A0) * inverse;
        half = 0.0f < tangent ? HalfPi - rest : -HalfPi - rest;
    }

    s32 halfAngle = static_cast<s32>(half * RadiansToAngle);
    f32 radians = static_cast<f32>(halfAngle) * AngleToRadians;
    *angle = static_cast<s32>((radians + radians) * RadiansToAngle);
    return angle;
}

void EulerAnglesOfMatrix(const Matrix4x4* matrix, s32* x, s32* y, s32* z)
{
    constexpr f32 Near = 0x1.0624dep-11f;
    const auto& m = matrix->m;
    // A turn about y alone: x and z none
    if (__builtin_fabsf(m[0][1]) <= Near && __builtin_fabsf(m[1][0]) <= Near && __builtin_fabsf(m[1][2]) <= Near &&
        __builtin_fabsf(m[2][1]) <= Near && __builtin_fabsf(1.0f - m[1][1]) <= Near && __builtin_fabsf(m[0][0] - m[2][2]) <= Near &&
        __builtin_fabsf(m[0][2] + m[2][0]) <= Near)
    {
        AngleOfPoint(y, m[2][0], m[0][0]);
        AngleFrom(z, 0.0f, AngleRadians);
        *x = *z;
        return;
    }

    AngleOfSine(-m[0][2], y);
    if (*y < QuarterTurnAngle)
    {
        if (-QuarterTurnAngle < *y)
        {
            AngleOfPoint(z, m[0][1], m[0][0]);
            AngleOfPoint(x, m[1][2], m[2][2]);
            return;
        }

        // Straight down: the turn about z the whole of it
        AngleOfPoint(z, -m[1][0], -m[2][0]);
        AngleFrom(x, 0.0f, AngleRadians);
        return;
    }

    AngleOfPoint(z, m[1][0], m[2][0]);
    *z = -*z;
    AngleFrom(x, 0.0f, AngleRadians);
}

void MatrixFacing(Matrix4x4* matrix, const Vector4* direction)
{
    *RowOf(matrix, 2) = *direction;
    f32 flat = 1.0f - direction->y * direction->y;
    if (flat == 0.0f)
    {
        // Straight up or down
        f32 sign = 0.0f < direction->y ? 1.0f : -1.0f;
        matrix->m[1][1] = 0.0f;
        matrix->m[1][0] = sign;
        matrix->m[0][1] = 0.0f;
        matrix->m[0][0] = 0.0f;
        matrix->m[0][2] = sign;
        matrix->m[1][2] = 0.0f;
    }
    else
    {
        // The up axis: the world's up made square to the direction, then the side axis square to both
        Vector4* up = RowOf(matrix, 1);
        up->x = -direction->x * direction->y;
        up->w = 1.0f;
        up->y = flat;
        up->z = -direction->y * direction->z;
        f32 inverse = InverseLength(up, LengthEpsilon);
        up->x = up->x * inverse;
        up->y = up->y * inverse;
        up->z = up->z * inverse;
        Vector4* side = RowOf(matrix, 0);
        side->x = up->y * direction->z - up->z * direction->y;
        side->y = up->z * direction->x - up->x * direction->z;
        side->w = 1.0f;
        side->z = up->x * direction->y - up->y * direction->x;
        f32 sideInverse = InverseLength(side, LengthEpsilon);
        side->x = side->x * sideInverse;
        side->y = side->y * sideInverse;
        side->z = side->z * sideInverse;
    }

    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[0][3] = 0.0f;
}

void BasisAround(Matrix4x4* matrix, const Vector4* axis, s32 row)
{
    // The row after each, a copy on the stack like retail's (a row past 2 reads the stack after it)
    const s32 next[3] = {NextAxis[0], NextAxis[1], NextAxis[2]};
    s32 second = next[row];
    *RowOf(matrix, row) = *axis;
    s32 third = next[second];
    Vector4* square = RowOf(matrix, second);
    PerpendicularOf(axis, square);
    f32 inverse = InverseLength(square, LengthEpsilon);
    square->x = square->x * inverse;
    square->y = square->y * inverse;
    square->z = square->z * inverse;
    Vector4* cross = RowOf(matrix, third);
    cross->x = axis->y * square->z - axis->z * square->y;
    cross->y = axis->z * square->x - axis->x * square->z;
    cross->w = 1.0f;
    cross->z = axis->x * square->y - axis->y * square->x;
    f32 crossInverse = InverseLength(cross, LengthEpsilon);
    cross->x = cross->x * crossInverse;
    cross->y = cross->y * crossInverse;
    cross->z = cross->z * crossInverse;
    matrix->m[3][3] = 1.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[0][3] = 0.0f;
}

void LookAlong(Matrix4x4* matrix, const Vector4* direction, const Vector4* up)
{
    Matrix4x4 frame;
    BasisAround(&frame, up, 1);
    Vector4 local = *direction;
    f32 x = local.x * frame.m[0][0] + local.y * frame.m[0][1] + local.z * frame.m[0][2];
    f32 y = local.x * frame.m[1][0] + local.y * frame.m[1][1] + local.z * frame.m[1][2];
    f32 z = local.x * frame.m[2][0] + local.y * frame.m[2][1] + local.z * frame.m[2][2];
    local.x = x;
    local.z = z;
    local.y = y;
    MatrixFacing(matrix, &local);
    Matrix4x4 product;
    VuMultiplyMatrices(matrix, &frame, &product);
    *matrix = product;
    matrix->m[3][3] = 1.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[0][3] = 0.0f;
}

void AxesAround(const Vector4* direction, Vector4* side, Vector4* up)
{
    f32 off = direction->y * direction->y - 1.0f;
    if (off == 0.0f)
    {
        // Straight up or down
        if (0.0f < direction->y)
        {
            *side = g_ZAxis;
            *up = g_XAxis;
            return;
        }

        *side = g_XAxis;
        side->x = -side->x;
        side->y = -side->y;
        side->z = -side->z;
        *up = g_ZAxis;
        up->x = -up->x;
        up->y = -up->y;
        up->z = -up->z;
        return;
    }

    f32 flat = -off;
    up->y = flat;
    up->w = 1.0f;
    up->x = -(direction->x * direction->y);
    up->z = -(direction->y * direction->z);
    side->x = up->y * direction->z - up->z * direction->y;
    side->y = up->z * direction->x - up->x * direction->z;
    side->w = 1.0f;
    side->z = up->x * direction->y - up->y * direction->x;
    f32 inverse = InverseLength(up, LengthEpsilon);
    up->x = up->x * inverse;
    up->y = up->y * inverse;
    up->z = up->z * inverse;
    f32 sideInverse = InverseLength(side, LengthEpsilon);
    side->x = side->x * sideInverse;
    side->y = side->y * sideInverse;
    side->z = side->z * sideInverse;
}

void AngleBetweenRotations(s32* angle, Vector4* a, Vector4* b)
{
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, a);
    a->x = a->x * inverse;
    a->y = a->y * inverse;
    a->z = a->z * inverse;
    a->w = a->w * inverse;
    f32 otherInverse = InverseLength4(0.0f, InverseEpsilon, b);
    b->x = b->x * otherInverse;
    b->y = b->y * otherInverse;
    b->z = b->z * otherInverse;
    b->w = b->w * otherInverse;
    f32 dot = a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
    if (dot < 0.0f)
    {
        dot = -dot;
    }

    AngleOfCosine(dot, angle);
}

void RotateByQuaternion(Vector4* rotation, const Vector4* vector, Vector4* out, u32 normalized)
{
    if (normalized == 0)
    {
        f32 x = rotation->x;
        f32 lengthSquared = x * x + rotation->y * rotation->y + rotation->z * rotation->z + rotation->w * rotation->w;
        f32 scale = 0.0f;
        if (InverseEpsilonSquared < lengthSquared)
        {
            scale = Platform::Math::DivideBySquareRoot(1.0f, lengthSquared);
        }

        rotation->x = x * scale;
        rotation->y = rotation->y * scale;
        rotation->z = rotation->z * scale;
        rotation->w = rotation->w * scale;
    }

    f32 x = rotation->x;
    f32 y = rotation->y;
    f32 z = rotation->z;
    f32 w = rotation->w;
    f32 x2 = x + x;
    f32 y2 = y + y;
    f32 z2 = z + z;
    f32 yy = y * y2;
    f32 wx = w * x2;
    f32 wy = w * y2;
    f32 xy = x * y2;
    f32 wz = w * z2;
    f32 zz = z * z2;
    f32 xx = x * x2;
    f32 xz = x * z2;
    f32 yz = y * z2;
    out->x = vector->x * (1.0f - yy - zz) + vector->y * (xy - wz) + vector->z * (xz + wy);
    out->y = vector->x * (xy + wz) + vector->y * (1.0f - xx - zz) + vector->z * (yz - wx);
    out->z = vector->x * (xz - wy) + vector->y * (yz + wx) + vector->z * (1.0f - xx - yy);
    out->w = vector->w;
}

void MatrixAboutAxis(f32 sine, f32 cosine, Matrix4x4* matrix, const Vector4* axis, u32 unitAxis)
{
    f32 x = axis->x;
    f32 y = axis->y;
    f32 z = axis->z;
    if (unitAxis == 0)
    {
        f32 lengthSquared = x * x + y * y + z * z;
        f32 scale = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            scale = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
        }

        z = z * scale;
        x = x * scale;
        y = y * scale;
    }

    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[0][3] = 0.0f;
    f32 rest = 1.0f - cosine;
    f32 restX = rest * x;
    f32 restZ = rest * z;
    f32 restY = rest * y;
    f32 xz = restX * z;
    f32 xy = restX * y;
    f32 yz = restY * z;
    f32 zz = restZ * z;
    f32 xx = restX * x;
    f32 yy = restY * y;
    f32 sineZ = sine * z;
    f32 sineX = sine * x;
    f32 sineY = sine * y;
    matrix->m[1][0] = xy - sineZ;
    matrix->m[2][1] = yz - sineX;
    matrix->m[2][0] = xz + sineY;
    matrix->m[2][2] = zz + cosine;
    matrix->m[0][0] = xx + cosine;
    matrix->m[1][1] = yy + cosine;
    matrix->m[0][1] = xy + sineZ;
    matrix->m[0][2] = xz - sineY;
    matrix->m[1][2] = yz + sineX;
}

namespace
{
// Two directions' cross product, its length squared and their dot product, as the rotations between them work them out
struct CrossAndDot
{
    Vector4 cross;
    f32 crossSquared;
    f32 dot;
};

CrossAndDot CrossAndDotOf(const Vector4* from, const Vector4* to)
{
    CrossAndDot between;
    Vector4 copy = *from;
    between.cross.y = copy.z * to->x - copy.x * to->z;
    between.cross.x = copy.y * to->z - copy.z * to->y;
    between.cross.z = copy.x * to->y - copy.y * to->x;
    between.cross.w = copy.w;
    between.crossSquared = between.cross.x * between.cross.x + between.cross.y * between.cross.y + between.cross.z * between.cross.z;
    between.dot = from->x * to->x + from->y * to->y + from->z * to->z;
    return between;
}

f32 SquaredLength3(const Vector4* vector)
{
    return vector->x * vector->x + vector->y * vector->y + vector->z * vector->z;
}

// An axis square to the longer of two directions (y when there's none)
Vector4 SquareToLonger(const Vector4* from, const Vector4* to)
{
    Vector4 axis;
    const Vector4* longer = SquaredLength3(to) < SquaredLength3(from) ? from : to;
    if (PerpendicularOf(longer, &axis) == 0)
    {
        axis = g_YAxis;
    }

    return axis;
}
}

void MatrixBetween(Matrix4x4* matrix, const Vector4* from, const Vector4* to)
{
    CrossAndDot between = CrossAndDotOf(from, to);
    if (0.0f < between.crossSquared)
    {
        f32 cross = __builtin_sqrtf(between.crossSquared);
        f32 inverse = __builtin_sqrtf(1.0f / (SquaredLength3(from) * SquaredLength3(to)));
        MatrixAboutAxis(cross * inverse, between.dot * inverse, matrix, &between.cross, 0);
        return;
    }

    if (between.dot * __builtin_fabsf(between.dot) < 0.0f)
    {
        Vector4 axis = SquareToLonger(from, to);
        MatrixAboutAxis(0.0f, -1.0f, matrix, &axis, 0);
        return;
    }

    InitIdentityMatrix(matrix);
}

void RotationBetween(Vector4* rotation, const Vector4* from, const Vector4* to)
{
    CrossAndDot between = CrossAndDotOf(from, to);
    if (0.0f < between.crossSquared)
    {
        f32 lengths = SquaredLength3(from) * SquaredLength3(to);
        f32 inverse = 0.0f;
        if (DivisionEpsilon < lengths || lengths < -DivisionEpsilon)
        {
            inverse = 1.0f / lengths;
        }

        f32 root = __builtin_sqrtf(inverse);
        f32 sine = __builtin_sqrtf(between.crossSquared) * root;
        f32 cosine = between.dot * root;
        f32 halfSine;
        f32 halfCosine;
        if (sine < 0.0f)
        {
            halfSine = __builtin_sqrtf((1.0f - cosine) * 0.5f);
            halfCosine = -__builtin_sqrtf((cosine + 1.0f) * 0.5f);
        }
        else
        {
            halfSine = __builtin_sqrtf((1.0f - cosine) * 0.5f);
            halfCosine = __builtin_sqrtf(cosine * 0.5f + 0.5f);
        }

        f32 lengthSquared = SquaredLength3(&between.cross);
        f32 scale = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            scale = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
        }

        scale = halfSine * scale;
        rotation->w = halfCosine;
        rotation->z = scale * between.cross.z;
        rotation->x = scale * between.cross.x;
        rotation->y = scale * between.cross.y;
        return;
    }

    if (between.dot * __builtin_fabsf(between.dot) < 0.0f)
    {
        // A half turn about an axis square to them
        Vector4 axis = SquareToLonger(from, to);
        f32 lengthSquared = SquaredLength3(&axis);
        f32 scale = 0.0f;
        if (LengthEpsilon < lengthSquared)
        {
            scale = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
        }

        scale = 1.0f * scale;
        rotation->w = 0.0f;
        rotation->z = scale * axis.z;
        rotation->x = scale * axis.x;
        rotation->y = scale * axis.y;
        return;
    }

    rotation->x = 0.0f;
    rotation->w = 1.0f;
    rotation->z = 0.0f;
    rotation->y = 0.0f;
}

void LookAtMatrix(Matrix4x4* matrix, const Vector4* eye, const Vector4* target, const Vector4* up)
{
    Vector4 forward = *target;
    forward.x = forward.x - eye->x;
    forward.y = forward.y - eye->y;
    forward.z = forward.z - eye->z;
    f32 inverse = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * inverse;
    forward.y = forward.y * inverse;
    forward.z = forward.z * inverse;
    Vector4 side = up != nullptr ? *up : Vector4{0.0f, 1.0f, 0.0f, 1.0f};
    f32 sideX = side.y * forward.z - side.z * forward.y;
    f32 sideY = side.z * forward.x - side.x * forward.z;
    f32 sideZ = side.x * forward.y - side.y * forward.x;
    side.x = sideX;
    side.y = sideY;
    side.z = sideZ;
    f32 sideInverse = InverseLength(&side, LengthEpsilon);
    side.x = side.x * sideInverse;
    side.y = side.y * sideInverse;
    side.z = side.z * sideInverse;
    Vector4 upAxis;
    upAxis.z = forward.x * side.y - forward.y * side.x;
    upAxis.y = forward.z * side.x - forward.x * side.z;
    upAxis.x = forward.y * side.z - forward.z * side.y;
    upAxis.w = 1.0f;
    f32 upInverse = InverseLength(&upAxis, LengthEpsilon);
    upAxis.x = upAxis.x * upInverse;
    upAxis.y = upAxis.y * upInverse;
    upAxis.z = upAxis.z * upInverse;
    matrix->m[0][0] = side.x;
    matrix->m[0][1] = side.y;
    matrix->m[0][2] = side.z;
    matrix->m[1][0] = upAxis.x;
    matrix->m[1][1] = upAxis.y;
    matrix->m[1][2] = upAxis.z;
    matrix->m[2][0] = forward.x;
    matrix->m[2][1] = forward.y;
    matrix->m[2][2] = forward.z;
    matrix->m[3][0] = -(upAxis.x * eye->x + upAxis.y * eye->y + upAxis.z * eye->z);
    matrix->m[3][1] = -(side.x * eye->x + side.y * eye->y + side.z * eye->z);
    matrix->m[3][2] = forward.x * eye->x + forward.y * eye->y + forward.z * eye->z;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][3] = 1.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
}

f32 ClampFloat(f32 value, f32 low, f32 high)
{
    if (value < low)
    {
        return low;
    }

    if (high < value)
    {
        return high;
    }

    return value;
}

f32 HalfLifeShare(f32 halfLife, f32 seconds)
{
    constexpr f32 MinusLn2 = -0x1.62e43p-1f;
    return Exponential(seconds * MinusLn2 / halfLife);
}

void MathsDebugStub()
{
}
