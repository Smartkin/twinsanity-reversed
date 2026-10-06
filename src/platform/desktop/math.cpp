// The desktop's side of Platform::Math: VU0's microprograms and the EE FPU's instructions in C++, their steps in their order
#include "platform/math.h"

#include "game/math.h"
#include "game/particles.h"
#include "game/scenery.h"
#include "platform/graphics.h"
#include "vu0.h"

namespace
{
// The frustum's planes in a particle view: the near one, the four sides, the far one
constexpr u32 ViewPlaneCount = 6;

// A chunk's view for the frame's particles (the PS2 keeps it in VU0's memory): the frustum's planes in the chunk's space, where
// the camera is in it and its matrix to the screen
struct ParticleView
{
    Vector4 planes[ViewPlaneCount];
    Vector4 camera;
    Matrix4x4 toScreen;
};

ParticleView g_ParticleViews[ParticleViews::MostChunks];

// The ray the triangles are tested against (VU0's vf04 and vf05 on the PS2), and the last test's outcome: whether the triangle
// was hit nearer, and where (its w the share of the way along the ray)
Vector4 g_RayStart;
Vector4 g_RayEnd;
bool g_RayHitsNearer;
Vector4 g_RayHit;

// VU0's sign flags: a result's sign bit
bool IsNegative(f32 value)
{
    return (__builtin_bit_cast(u32, value) >> 31) != 0;
}

// A float's bits ordered the way the EE's and VU0's comparisons order them: as signed magnitudes, -0 below +0
s32 SignedMagnitude(f32 value)
{
    FloatBits bits;
    bits.value = __builtin_bit_cast(u32, value);
    u32 sign = bits.sign;
    bits.sign = 0;
    s32 magnitude = static_cast<s32>(bits.value);
    return sign != 0 ? -magnitude - 1 : magnitude;
}

// The microprograms' sine: the cosine of the angle less their quarter turn (SinCosPolynomial's first value)
f32 ProgramSine(f32 radians)
{
    f32 values[4];
    Platform::Math::SinCosPolynomial(radians, 0.0f, values);
    return values[0];
}

// A row vector through a matrix: the matrix's rows times its x, y and z and a w, summed in that order
void Through(const Matrix4x4* matrix, const f32* row, f32 w, f32* out)
{
    for (u32 lane = 0; lane < 4; lane++)
    {
        out[lane] = matrix->m[0][lane] * row[0] + matrix->m[1][lane] * row[1] + matrix->m[2][lane] * row[2] +
                    matrix->m[3][lane] * w;
    }
}

// Microprogram 0x790: the rotation rows through the parent with their own w, the translation with a w of 1 (its own left
// out) and 1 for its w
void PutInParentSpace(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out)
{
    Matrix4x4 moved;
    for (u32 row = 0; row < 3; row++)
    {
        Through(parent, matrix->m[row], matrix->m[row][3], moved.m[row]);
    }

    Through(parent, matrix->m[PositionRow], 1.0f, moved.m[PositionRow]);
    moved.m[PositionRow][3] = 1.0f;
    *out = moved;
}

// Whether a box of the extents around a place is wholly behind one of the view's planes: VU0's sign flags of the place's side
// of the plane with the box's reach along the plane's normal added and taken away both set
bool BoxOutsideView(const ParticleView* view, const f32* extents, const f32* place)
{
    bool outside = false;
    for (const Vector4& plane : view->planes)
    {
        f32 reach = __builtin_fabsf(plane.x) * extents[0] + __builtin_fabsf(plane.y) * extents[1] +
                    __builtin_fabsf(plane.z) * extents[2];
        f32 side = plane.x * place[0] + plane.y * place[1] + plane.z * place[2] + plane.w;
        if (IsNegative(side + reach) && IsNegative(side - reach))
        {
            outside = true;
        }
    }

    return outside;
}

f32 LengthSquaredOf(const Vector4& vector)
{
    return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

// VU0's outer product pair (OPMULA, then OPMSUB): a cross b, each part a's product less b's
Vector4 Cross(const Vector4& a, const Vector4& b)
{
    Vector4 cross = {};
    cross.x = a.y * b.z - b.y * a.z;
    cross.y = a.z * b.x - b.z * a.x;
    cross.z = a.x * b.y - b.x * a.y;
    return cross;
}

f32 Dot(const Vector4& a, const Vector4& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector4 Difference(const Vector4& from, const Vector4& to)
{
    Vector4 difference = {};
    difference.x = to.x - from.x;
    difference.y = to.y - from.y;
    difference.z = to.z - from.z;
    return difference;
}

Vector4 Scaled(const Vector4& vector, f32 scale)
{
    Vector4 scaled = vector;
    scaled.x = vector.x * scale;
    scaled.y = vector.y * scale;
    scaled.z = vector.z * scale;
    return scaled;
}
}

namespace Platform::Math
{
void SinCos(f32 first, f32 second, f32* out)
{
    SinCosPolynomial(first, second, out);
}

f32 Max(f32 first, f32 second)
{
    return SignedMagnitude(first) < SignedMagnitude(second) ? second : first;
}

f32 Min(f32 first, f32 second)
{
    return SignedMagnitude(second) < SignedMagnitude(first) ? second : first;
}

void SlerpRotations(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    // Nearer each other than this, they're blended along a straight line (0.999)
    constexpr f32 Straight = 0x1.FF7CEEp-1f;
    f32 dot = from->x * to->x + from->y * to->y + from->z * to->z + from->w * to->w;
    Vector4 toward = *to;
    if (IsNegative(dot))
    {
        dot = dot * -1.0f;
        toward = {to->x * -1.0f, to->y * -1.0f, to->z * -1.0f, to->w * -1.0f};
    }

    f32 fromShare = 1.0f - share;
    f32 towardShare = share;
    if (IsNegative(dot - Straight))
    {
        // The angle between them, the program's arc cosine of their dot product: π (1 - dot) less π / 2 (1 - sin(π / 2 dot))
        f32 sine = CosinePolynomial(dot * HalfPi - HalfPi);
        f32 rest = HalfPi - dot * HalfPi;
        f32 angle = (rest + rest) - (HalfPi - sine * HalfPi);
        f32 inverse = Vu0::Divide(1.0f, ProgramSine(angle));
        towardShare = ProgramSine(share * angle) * inverse;
        fromShare = ProgramSine((1.0f - share) * angle) * inverse;
    }

    out->x = from->x * fromShare + toward.x * towardShare;
    out->y = from->y * fromShare + toward.y * towardShare;
    out->z = from->z * fromShare + toward.z * towardShare;
    out->w = from->w * fromShare + toward.w * towardShare;
}

void EulerRotation(const Vector4* anglesNow, const Vector4* anglesNext, const Vector4* moveNow, const Vector4* moveNext,
                   Vector4* rotation, Vector4* move)
{
    f32 share = anglesNext->w;
    f32 x = anglesNow->x - anglesNow->x * share + anglesNext->x * share;
    f32 y = anglesNow->y - anglesNow->y * share + anglesNext->y * share;
    f32 z = anglesNow->z - anglesNow->z * share + anglesNext->z * share;
    Vector4 moved = {moveNow->x - moveNow->x * share + moveNext->x * share,
                     moveNow->y - moveNow->y * share + moveNext->y * share,
                     moveNow->z - moveNow->z * share + moveNext->z * share, 1.0f};
    // The half angles' sines and cosines: x's and z's, then y's
    f32 halvesXZ[4];
    SinCosPolynomial(x * 0.5f, z * 0.5f, halvesXZ);
    f32 halvesY[4];
    SinCosPolynomial(y * 0.5f, 0.0f, halvesY);
    f32 sinX = halvesXZ[0];
    f32 cosX = halvesXZ[1];
    f32 sinZ = halvesXZ[2];
    f32 cosZ = halvesXZ[3];
    f32 sinY = halvesY[0];
    f32 cosY = halvesY[1];
    rotation->x = sinX * cosZ * cosY - cosX * sinZ * sinY;
    rotation->y = sinX * sinZ * cosY + cosX * cosZ * sinY;
    rotation->z = cosX * sinZ * cosY - sinX * cosZ * sinY;
    rotation->w = cosX * cosZ * cosY + sinX * sinZ * sinY;
    *move = moved;
}

void TurnRotation(const Vector4* by, Vector4* rotation)
{
    MultiplyRotations(rotation, by, rotation);
}

void Lerp(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    Vector4 between = {from->x - from->x * share + to->x * share, from->y - from->y * share + to->y * share,
                       from->z - from->z * share + to->z * share, 1.0f};
    *out = between;
}

void JointMatrix(const Vector4* rotation, const Vector4* parentScale, const Vector4* scale, const Vector4* translation,
                 const Matrix4x4* parent, Matrix4x4* out)
{
    Matrix4x4 joint;
    InitIdentityMatrix(&joint);
    if (rotation != nullptr)
    {
        // The rotation's rows, each scaled by its own scale, every column times the inverse of the parent's scale along it
        MatrixFromRotation(&joint, rotation);
        f32 inverses[3] = {1.0f, 1.0f, 1.0f};
        if (parentScale != nullptr)
        {
            inverses[0] = Vu0::Divide(1.0f, parentScale->x);
            inverses[1] = Vu0::Divide(1.0f, parentScale->y);
            inverses[2] = Vu0::Divide(1.0f, parentScale->z);
        }

        for (u32 row = 0; row < 3; row++)
        {
            for (u32 column = 0; column < 3; column++)
            {
                f32 value = joint.m[row][column];
                if (scale != nullptr)
                {
                    value = value * (&scale->x)[row];
                }

                joint.m[row][column] = value * inverses[column];
            }
        }
    }

    if (translation != nullptr)
    {
        *RowOf(&joint, PositionRow) = *translation;
    }

    if (parent != nullptr)
    {
        PutInParentSpace(&joint, parent, &joint);
    }

    *out = joint;
}

void MultiplyByParent(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out)
{
    PutInParentSpace(matrix, parent, out);
}

f32 ViewDistance(s32 view, const f32* point)
{
    const Vector4& camera = g_ParticleViews[view].camera;
    Vector4 way = {camera.x - point[0], camera.y - point[1], camera.z - point[2], 0.0f};
    return Vu0::SquareRoot(LengthSquaredOf(way));
}

bool ParticleBlockView(s32 view, bool keepsTranslation, const f32* extents, const Matrix4x4* gravity, const f32* place, f32 scale,
                       Matrix4x4* matrix, f32* distance)
{
    const ParticleView& loaded = g_ParticleViews[view];
    const Vector4& camera = loaded.camera;
    bool outside = BoxOutsideView(&loaded, extents, place);
    f32 far;
    Matrix4x4 made;
    if (keepsTranslation)
    {
        // The view's first three rows, and the way towards the camera, the scale long, through the view (its w 1)
        Vector4 toCamera = {camera.x - place[0], camera.y - place[1], camera.z - place[2], 0.0f};
        far = Vu0::SquareRoot(LengthSquaredOf(toCamera));
        f32 inverse = Vu0::Divide(1.0f, far);
        f32 pull[3] = {toCamera.x * inverse * scale, toCamera.y * inverse * scale, toCamera.z * inverse * scale};
        for (u32 row = 0; row < 3; row++)
        {
            *RowOf(&made, row) = *RowOf(&loaded.toScreen, row);
        }

        Through(&loaded.toScreen, pull, 1.0f, made.m[PositionRow]);
    }
    else
    {
        // The gravity's rows through the view, and the place pulled the scale towards the camera. The program makes the way
        // from the camera a unit one in x, y and z only, so the place's w (1) less the camera's is scaled into the pulled w
        Vector4 fromCamera = {place[0] - camera.x, place[1] - camera.y, place[2] - camera.z, 1.0f - camera.w};
        far = Vu0::SquareRoot(LengthSquaredOf(fromCamera));
        f32 inverse = Vu0::Divide(1.0f, far);
        f32 pulled[4] = {place[0] - fromCamera.x * inverse * scale, place[1] - fromCamera.y * inverse * scale,
                         place[2] - fromCamera.z * inverse * scale, 1.0f - fromCamera.w * scale};
        for (u32 row = 0; row < 3; row++)
        {
            Through(&loaded.toScreen, gravity->m[row], gravity->m[row][3], made.m[row]);
        }

        Through(&loaded.toScreen, pulled, pulled[3], made.m[PositionRow]);
    }

    *matrix = made;
    *distance = far;
    return outside;
}

f32 DivideBySquareRoot(f32 value, f32 square)
{
    if (square == 0.0f)
    {
        return Vu0::Divide(value, square);
    }

    return static_cast<f32>(static_cast<double>(value) / __builtin_sqrt(__builtin_fabs(static_cast<double>(square))));
}

void SetRay(const Vector4* start, const Vector4* end)
{
    g_RayStart = *start;
    g_RayEnd = *end;
}

// The ray's ends on the two sides of the triangle's plane, where it crosses the plane (the share of the way their distances
// from it give) nearer than the nearest so far, and inside every edge's plane (square to the triangle's through the edge,
// facing in) or within 0.1 of it. The microprogram ends with the flags of those tests, which FinishRayTriangle compares
void StartRayTriangle(const Vector4* vertices, const Vector4* nearest)
{
    constexpr f32 EdgeTolerance = Rounded(0.1);
    constexpr u32 EdgeCount = 3;
    const Vector4& first = vertices[0];
    const Vector4& second = vertices[1];
    const Vector4& third = vertices[2];
    Vector4 normal = Cross(Difference(first, second), Difference(first, third));
    Vector4 edgeNormals[EdgeCount] = {Cross(normal, Difference(first, second)), Cross(normal, Difference(second, third)),
                                      Cross(normal, Difference(third, first))};
    // The edges' planes go through these ends of them
    const Vector4* edgeEnds[EdgeCount] = {&first, &third, &first};
    normal = Scaled(normal, DivideBySquareRoot(1.0f, LengthSquaredOf(normal)));
    f32 offset = 0.0f - Dot(normal, first);
    f32 startSide = Dot(g_RayStart, normal) + offset;
    f32 endSide = Dot(g_RayEnd, normal) + offset;
    f32 startDistance = __builtin_fabsf(startSide);
    f32 share = Vu0::Divide(startDistance, startDistance + __builtin_fabsf(endSide));
    Vector4 hit = {g_RayStart.x - g_RayStart.x * share + g_RayEnd.x * share,
                   g_RayStart.y - g_RayStart.y * share + g_RayEnd.y * share,
                   g_RayStart.z - g_RayStart.z * share + g_RayEnd.z * share, share};
    bool inside = true;
    for (u32 edge = 0; edge < EdgeCount; edge++)
    {
        Vector4 plane = Scaled(edgeNormals[edge], DivideBySquareRoot(1.0f, LengthSquaredOf(edgeNormals[edge])));
        f32 side = (0.0f - Dot(plane, *edgeEnds[edge])) + plane.x * hit.x;
        side = side + plane.y * hit.y + plane.z * hit.z + EdgeTolerance;
        if (IsNegative(side))
        {
            inside = false;
        }
    }

    g_RayHitsNearer = IsNegative(startSide) != IsNegative(endSide) && IsNegative(share - nearest->w) && inside;
    g_RayHit = hit;
}

bool FinishRayTriangle(Vector4* hit)
{
    if (!g_RayHitsNearer)
    {
        return false;
    }

    *hit = g_RayHit;
    return true;
}
}

namespace Platform::Graphics
{
// The planes taken into the chunk's space through the matrix the camera's place is the last row of, and the matrix to the
// screen after it. The handle is the view's index
s32 LoadParticleView(const Matrix4x4* matrices, s32 index)
{
    ParticleView& view = g_ParticleViews[index];
    const Vector4* planes = ViewPlanes();
    for (u32 plane = 0; plane < ViewPlaneCount; plane++)
    {
        view.planes[plane] = planes[plane];
    }

    TransformPlanes(view.planes, &matrices[0]);
    view.camera = *RowOf(&matrices[0], PositionRow);
    view.toScreen = matrices[1];
    return index;
}
}
