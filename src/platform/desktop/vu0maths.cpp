// The desktop's side of the game's VU0 macro mode maths: matrixes, rotations, planes, boxes, clipping and the collision's and
// physics' helpers in C++, their multiply-adds summed in VU0's order
#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/physics.h"
#include "platform/graphics.h"
#include "platform/math.h"
#include "vu0.h"

namespace
{
// A CLIP judgment: which of a point's x, y and z are past its w either way
union ClipJudgment
{
    u32 value;
    struct
    {
        u32 pastPositiveX : 1;
        u32 pastNegativeX : 1;
        u32 pastPositiveY : 1;
        u32 pastNegativeY : 1;
        u32 pastPositiveZ : 1;
        u32 pastNegativeZ : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(ClipJudgment, 4);

// VU0's clipping flags keep the last four judgments, the newest in the low bits
constexpr u32 ClipJudgmentBits = 6;
constexpr u32 ClipFlagsMask = 0xFFFFFF;
// A box's corners
constexpr u32 BoxCornerCount = 8;
// What the groups' ranges start from: their least 2^65, their most -2^65
constexpr f32 RangeStart = 0x1.0p+65f;
// The triangle's vertexes less the box hull's
constexpr u32 TriangleVertexCount = 3;
constexpr u32 BoxHullVertexCount = 8;

// VU0's clipping flags (ClipBox is all that judges on it)
u32 g_ClipFlags;

// The edge tests' second half from the first (VU0's vf16-vf28 on the PS2): each edge's cross product with the ray and the
// ray's start's ways to the two vertexes it's tested against
struct EdgeTests
{
    Vector4 crosses[3];
    Vector4 ways[3][2];
};
EdgeTests g_EdgeTests;

// The triangle's vertexes less the box hull's (VU0's vf02-vf19 on the PS2, grouped four at a time)
Vector4 g_SupportDifferences[TriangleVertexCount][BoxHullVertexCount];

// A row vector through a matrix: the first three rows times its x, y and z, then the fourth times a w, summed in that order
Vector4 Through(const Matrix4x4* matrix, const Vector4* vector, f32 w)
{
    Vector4 through;
    f32* lanes = &through.x;
    for (u32 lane = 0; lane < 4; lane++)
    {
        lanes[lane] = matrix->m[0][lane] * vector->x + matrix->m[1][lane] * vector->y + matrix->m[2][lane] * vector->z +
                      matrix->m[3][lane] * w;
    }

    return through;
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

// A box grown to take a point in (VU0's MINI and MAX compare like the EE's MIN.S and MAX.S)
void TakeIntoBox(Box* box, const Vector4& point)
{
    box->min.x = Platform::Math::Min(box->min.x, point.x);
    box->min.y = Platform::Math::Min(box->min.y, point.y);
    box->min.z = Platform::Math::Min(box->min.z, point.z);
    box->max.x = Platform::Math::Max(box->max.x, point.x);
    box->max.y = Platform::Math::Max(box->max.y, point.y);
    box->max.z = Platform::Math::Max(box->max.z, point.z);
}

// Four values taken into the least and the most so far, and the least and the most of those four into a range
void TakeIntoRange(Vector4* least, Vector4* most, const Vector4& values)
{
    least->x = Platform::Math::Min(least->x, values.x);
    least->y = Platform::Math::Min(least->y, values.y);
    least->z = Platform::Math::Min(least->z, values.z);
    least->w = Platform::Math::Min(least->w, values.w);
    most->x = Platform::Math::Max(most->x, values.x);
    most->y = Platform::Math::Max(most->y, values.y);
    most->z = Platform::Math::Max(most->z, values.z);
    most->w = Platform::Math::Max(most->w, values.w);
}

void StoreRange(const Vector4& least, const Vector4& most, f32* range)
{
    range[0] = Platform::Math::Min(Platform::Math::Min(least.x, least.y), Platform::Math::Min(least.z, least.w));
    range[1] = Platform::Math::Max(Platform::Math::Max(most.x, most.y), Platform::Math::Max(most.z, most.w));
}

// A group's four points (their xs, ys and zs) along an axis
Vector4 GroupAlong(const Vector4* group, const Vector4* axis)
{
    const f32* xs = &group[0].x;
    const f32* ys = &group[1].x;
    const f32* zs = &group[2].x;
    Vector4 along;
    f32* lanes = &along.x;
    for (u32 lane = 0; lane < 4; lane++)
    {
        lanes[lane] = xs[lane] * axis->x + ys[lane] * axis->y + zs[lane] * axis->z;
    }

    return along;
}

void Judge(const Vector4& point)
{
    f32 w = __builtin_fabsf(point.w);
    ClipJudgment judgment = {};
    judgment.pastPositiveX = point.x > w;
    judgment.pastNegativeX = point.x < -w;
    judgment.pastPositiveY = point.y > w;
    judgment.pastNegativeY = point.y < -w;
    judgment.pastPositiveZ = point.z > w;
    judgment.pastNegativeZ = point.z < -w;
    g_ClipFlags = (g_ClipFlags << ClipJudgmentBits | judgment.value) & ClipFlagsMask;
}

// The box's corners through the matrix (their w taken as 1) and scaled lane by lane, each judged unscaled then scaled; after a
// corner the flags hold its two judgments and the corner's before (the first corner's the last ones made). Every corner's flags
// taken together, and the bits they share
void ClipCorners(const Matrix4x4* matrix, const Vector4* scale, const Box* box, u32* flags)
{
    // The two corners, the minimum with each of the maximum's x, y and z, the maximum with each of the minimum's
    Vector4 corners[BoxCornerCount];
    corners[0] = box->min;
    corners[1] = box->max;
    for (u32 axis = 0; axis < 3; axis++)
    {
        corners[2 + axis] = box->min;
        (&corners[2 + axis].x)[axis] = (&box->max.x)[axis];
        corners[5 + axis] = box->max;
        (&corners[5 + axis].x)[axis] = (&box->min.x)[axis];
    }

    u32 any = 0;
    u32 every = 0;
    for (u32 corner = 0; corner < BoxCornerCount; corner++)
    {
        Vector4 clipped = Through(matrix, &corners[corner], 1.0f);
        Vector4 scaled = {clipped.x * scale->x, clipped.y * scale->y, clipped.z * scale->z, clipped.w * scale->w};
        Judge(clipped);
        Judge(scaled);
        if (corner == 0)
        {
            any = g_ClipFlags;
            every = g_ClipFlags;
        }
        else
        {
            any |= g_ClipFlags;
            every &= g_ClipFlags;
        }
    }

    flags[0] = any;
    flags[1] = every;
}
}

void VuRotateVector(const Matrix4x4* matrix, const Vector4* vector, Vector4* out)
{
    Vector4 turned = *vector;
    f32* lanes = &turned.x;
    for (u32 lane = 0; lane < 3; lane++)
    {
        lanes[lane] = matrix->m[0][lane] * vector->x + matrix->m[1][lane] * vector->y + matrix->m[2][lane] * vector->z;
    }

    *out = turned;
}

void VuTransformPoint(const Matrix4x4* matrix, const Vector4* point, Vector4* out)
{
    *out = Through(matrix, point, 1.0f);
}

void MultiplyRotations(Vector4* out, const Vector4* a, const Vector4* b)
{
    Vector4 product;
    product.x = a->y * b->z + a->x * b->w + b->x * a->w - b->y * a->z;
    product.y = a->z * b->x + a->y * b->w + b->y * a->w - b->z * a->x;
    product.z = a->x * b->y + a->z * b->w + b->z * a->w - b->x * a->y;
    product.w = a->w * b->w - a->z * b->z - a->y * b->y - a->x * b->x;
    *out = product;
}

u32 PlaneThroughTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third)
{
    Vector4 normal = Cross(Difference(*first, *second), Difference(*first, *third));
    f32 length = Vu0::SquareRoot(Dot(normal, normal));
    f32 inverse = Vu0::Divide(1.0f, length);
    normal.x = normal.x * inverse;
    normal.y = normal.y * inverse;
    normal.z = normal.z * inverse;
    normal.w = 0.0f - Dot(normal, *first);
    *plane = normal;
    return Epsilon < length;
}

void VuTransformByRows(const Matrix4x4* rows, const Vector4* point, Vector4* out)
{
    *out = Through(rows, point, point->w);
}

void InitIdentityMatrix(Matrix4x4* matrix)
{
    for (u32 row = 0; row < 4; row++)
    {
        for (u32 column = 0; column < 4; column++)
        {
            matrix->m[row][column] = row == column ? 1.0f : 0.0f;
        }
    }
}

void VuMultiplyMatrices(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out)
{
    Matrix4x4 product;
    for (u32 row = 0; row < 4; row++)
    {
        const Vector4* vector = RowOf(a, row);
        *RowOf(&product, row) = Through(b, vector, vector->w);
    }

    *out = product;
}

void MatrixFromRotation(Matrix4x4* matrix, const Vector4* rotation)
{
    // The rotation scaled by the square root of 2, so the products of its parts come out doubled; the rows' w 0
    constexpr f32 SquareRootOf2 = 0x1.6A09E6p+0f;
    f32 x = rotation->x;
    f32 y = rotation->y;
    f32 z = rotation->z;
    f32 doubledX = x * x + x * x;
    f32 doubledY = y * y + y * y;
    f32 doubledZ = z * z + z * z;
    f32 scaledX = x * SquareRootOf2;
    f32 scaledY = y * SquareRootOf2;
    f32 scaledZ = z * SquareRootOf2;
    f32 scaledW = rotation->w * SquareRootOf2;
    f32 yz = scaledY * scaledZ;
    f32 zx = scaledZ * scaledX;
    f32 xy = scaledX * scaledY;
    f32 xw = scaledX * scaledW;
    f32 yw = scaledY * scaledW;
    f32 zw = scaledZ * scaledW;
    f32 lessX = 1.0f - doubledX;
    matrix->m[0][0] = 1.0f - doubledY - doubledZ;
    matrix->m[0][1] = xy + zw;
    matrix->m[0][2] = zx - yw;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = xy - zw;
    matrix->m[1][1] = lessX - doubledZ;
    matrix->m[1][2] = yz + xw;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = zx + yw;
    matrix->m[2][1] = yz - xw;
    matrix->m[2][2] = lessX - doubledY;
    matrix->m[2][3] = 0.0f;
}

void VuTranspose(const Matrix4x4* matrix, Matrix4x4* out)
{
    Matrix4x4 transposed;
    for (u32 row = 0; row < 4; row++)
    {
        for (u32 column = 0; column < 4; column++)
        {
            transposed.m[row][column] = matrix->m[column][row];
        }
    }

    *out = transposed;
}

void TransposeQuadwords(void* rows)
{
    auto* words = static_cast<u32(*)[4]>(rows);
    for (u32 row = 0; row < 4; row++)
    {
        for (u32 column = row + 1; column < 4; column++)
        {
            u32 kept = words[row][column];
            words[row][column] = words[column][row];
            words[column][row] = kept;
        }
    }
}

void VuTransposeRotation(const Matrix4x4* matrix, Matrix4x4* out)
{
    // The fourth row taken as (0, 0, 0, 1)
    Matrix4x4 transposed;
    InitIdentityMatrix(&transposed);
    for (u32 row = 0; row < 3; row++)
    {
        for (u32 column = 0; column < 3; column++)
        {
            transposed.m[row][column] = matrix->m[column][row];
        }
    }

    *out = transposed;
}

void VuInvertRigid(Matrix4x4* out, const Matrix4x4* matrix)
{
    Matrix4x4 inverse;
    f32 moveX = matrix->m[PositionRow][0] * -1.0f;
    f32 moveY = matrix->m[PositionRow][1] * -1.0f;
    f32 moveZ = matrix->m[PositionRow][2] * -1.0f;
    for (u32 row = 0; row < 3; row++)
    {
        for (u32 column = 0; column < 3; column++)
        {
            inverse.m[row][column] = matrix->m[column][row];
        }

        inverse.m[row][3] = 0.0f;
    }

    for (u32 lane = 0; lane < 3; lane++)
    {
        inverse.m[PositionRow][lane] = inverse.m[0][lane] * moveX + inverse.m[1][lane] * moveY + inverse.m[2][lane] * moveZ;
    }

    inverse.m[PositionRow][3] = 1.0f;
    *out = inverse;
}

void VuInvertRigidInPlace(Matrix4x4* matrix)
{
    VuInvertRigid(matrix, matrix);
}

void TriangleBounds(Box* box, const Vector4* first, const Vector4* second, const Vector4* third)
{
    // The corners' w stay as they were (VU0's registers' on the PS2)
    box->min.x = Platform::Math::Min(Platform::Math::Min(first->x, second->x), third->x);
    box->min.y = Platform::Math::Min(Platform::Math::Min(first->y, second->y), third->y);
    box->min.z = Platform::Math::Min(Platform::Math::Min(first->z, second->z), third->z);
    box->max.x = Platform::Math::Max(Platform::Math::Max(first->x, second->x), third->x);
    box->max.y = Platform::Math::Max(Platform::Math::Max(first->y, second->y), third->y);
    box->max.z = Platform::Math::Max(Platform::Math::Max(first->z, second->z), third->z);
}

void GetBbox(const Vector4* points, s32 count, Box* box, const Matrix4x4* matrix)
{
    Box bounds;
    bounds.min = Through(matrix, &points[0], points[0].w);
    bounds.max = bounds.min;
    for (s32 index = 1; index < count; index++)
    {
        TakeIntoBox(&bounds, Through(matrix, &points[index], points[index].w));
    }

    *box = bounds;
}

void PointsBox(const Vector4* points, s32 count, Box* box)
{
    Box bounds;
    bounds.min = points[0];
    bounds.max = points[0];
    for (s32 index = 1; index < count; index++)
    {
        TakeIntoBox(&bounds, points[index]);
    }

    *box = bounds;
}

// The ray's ends' sides of the triangle's plane (its normal the edges' cross product, not a unit one), and each edge's cross
// product with the ray and the ways to the vertexes it's tested against kept for the rest
void StartTriangleEdgeTests(const Vector4* start, const Vector4* end, const CollisionHit* triangle, f32* values)
{
    const Vector4& first = triangle->vertices[0];
    const Vector4& second = triangle->vertices[1];
    const Vector4& third = triangle->vertices[2];
    Vector4 ray = Difference(*start, *end);
    Vector4 normal = Cross(Difference(first, second), Difference(first, third));
    g_EdgeTests.crosses[0] = Cross(ray, Difference(first, second));
    g_EdgeTests.crosses[1] = Cross(ray, Difference(second, third));
    g_EdgeTests.crosses[2] = Cross(ray, Difference(third, first));
    g_EdgeTests.ways[0][0] = Difference(*start, first);
    g_EdgeTests.ways[0][1] = Difference(*start, third);
    g_EdgeTests.ways[1][0] = Difference(*start, second);
    g_EdgeTests.ways[1][1] = Difference(*start, first);
    g_EdgeTests.ways[2][0] = Difference(*start, third);
    g_EdgeTests.ways[2][1] = Difference(*start, second);
    values[0] = Dot(Difference(first, *start), normal);
    values[1] = Dot(Difference(first, *end), normal);
}

// Each edge's sides of the ray's two ends: the ways to its vertexes along its cross product
void FinishTriangleEdgeTests(f32* values)
{
    for (u32 edge = 0; edge < 3; edge++)
    {
        values[edge * 2] = Dot(g_EdgeTests.ways[edge][0], g_EdgeTests.crosses[edge]);
        values[edge * 2 + 1] = Dot(g_EdgeTests.ways[edge][1], g_EdgeTests.crosses[edge]);
    }
}

// Counts of 0 run on until the count wraps around, as on the PS2
void MakeVertexDifferences(const VertexDifferences* request)
{
    Vector4* moved = request->moved;
    const Vector4* vertex = request->vertices;
    u32 left = request->count;
    do
    {
        *moved = Through(request->matrix, vertex, vertex->w);
        moved++;
        vertex++;
        left--;
    } while (left != 0);

    Vector4* otherMoved = moved;
    vertex = request->otherVertices;
    left = request->otherCount;
    do
    {
        *moved = Through(request->otherMatrix, vertex, vertex->w);
        moved++;
        vertex++;
        left--;
    } while (left != 0);

    // Their w the first matrix's third row's (VU0's register kept it)
    f32 w = request->matrix->m[2][3];
    Vector4* difference = request->differences;
    const Vector4* first = request->moved;
    left = request->count;
    do
    {
        const Vector4* other = otherMoved;
        u32 others = request->otherCount;
        do
        {
            *difference = {first->x - other->x, first->y - other->y, first->z - other->z, w};
            difference++;
            other++;
            others--;
        } while (others != 0);

        first++;
        left--;
    } while (left != 0);
}

void GroupPoints(Vector4* points, s32 groups)
{
    const Vector4* from = points;
    Vector4* to = points;
    s32 left = groups;
    do
    {
        Vector4 group[4] = {from[0], from[1], from[2], from[3]};
        to[0] = {group[0].x, group[1].x, group[2].x, group[3].x};
        to[1] = {group[0].y, group[1].y, group[2].y, group[3].y};
        to[2] = {group[0].z, group[1].z, group[2].z, group[3].z};
        from += 4;
        to += 3;
        left--;
    } while (left != 0);
}

void GroupsRange(const Vector4* groups, s32 count, const Vector4* axis, f32* range)
{
    Vector4 least = {RangeStart, RangeStart, RangeStart, RangeStart};
    Vector4 most = {-RangeStart, -RangeStart, -RangeStart, -RangeStart};
    const Vector4* group = groups;
    s32 left = count;
    do
    {
        TakeIntoRange(&least, &most, GroupAlong(group, axis));
        group += 3;
        left--;
    } while (left != 0);

    StoreRange(least, most, range);
}

void SixteenGroupsRange(const Vector4* groups, const Vector4* axis, f32* range)
{
    constexpr u32 GroupCount = 16;
    Vector4 least = {RangeStart, RangeStart, RangeStart, RangeStart};
    Vector4 most = {-RangeStart, -RangeStart, -RangeStart, -RangeStart};
    for (u32 group = 0; group < GroupCount; group++)
    {
        TakeIntoRange(&least, &most, GroupAlong(&groups[group * 3], axis));
    }

    StoreRange(least, most, range);
}

void LoadTriangleHullSupport(const CollisionHit* triangle, const Vector4* hullVertices)
{
    for (u32 vertex = 0; vertex < TriangleVertexCount; vertex++)
    {
        for (u32 hullVertex = 0; hullVertex < BoxHullVertexCount; hullVertex++)
        {
            g_SupportDifferences[vertex][hullVertex] = Difference(hullVertices[hullVertex], triangle->vertices[vertex]);
        }
    }
}

void TriangleHullSupportRange(const Vector4* axis, f32* range)
{
    f32 least = Dot(g_SupportDifferences[0][0], *axis);
    f32 most = least;
    for (const auto& vertex : g_SupportDifferences)
    {
        for (const Vector4& difference : vertex)
        {
            f32 along = Dot(difference, *axis);
            least = Platform::Math::Min(least, along);
            most = Platform::Math::Max(most, along);
        }
    }

    range[0] = least;
    range[1] = most;
}

namespace Platform::Graphics
{
// The view's scale is the quadword after its matrix
void ClipBox(const Matrix4x4* view, const Box* box, u32* flags)
{
    ClipCorners(view, reinterpret_cast<const Vector4*>(view + 1), box, flags);
}

void ClipBoxAt(const Matrix4x4* view, const Box* box, u32* flags, const Matrix4x4* model)
{
    Matrix4x4 through;
    VuMultiplyMatrices(model, view, &through);
    ClipCorners(&through, reinterpret_cast<const Vector4*>(view + 1), box, flags);
}
}
