#include "platform/graphics.h"

#include "frustum.h"
#include "game/math.h"
#include "game/scenery.h"

// The desktop's culling: the view's frustum and the chunks' boxes tested against it (VU0's culling set on the PS2)
// On the PS2 the frustum's planes go into VU0's memory (renderer/culling.cpp), and its microprograms 0x1C0 (a box), 0x2D0 (a
// model's matrix), 0x398 (a link) and 0x500 (a box under a model's matrix) leave the outcome in its registers; here both are this
// file's statics, worked out with the programs' operations in their order
namespace
{
// A frustum's planes: the near one, the four sides (right, left, top and bottom) and the far one. The far set's sides are five
// times as far out
constexpr u32 NearPlane = 0;
constexpr u32 FirstSidePlane = 1;
constexpr u32 SidePlaneCount = 4;
constexpr u32 FarPlane = 5;
constexpr u32 FrustumPlaneCount = 6;
constexpr f32 FarSidesScale = 5.0f;
// The programs test four planes at once, a plane a field of VU0's registers: the near and far planes go in the first two
constexpr u32 PlaneFields = 4;
constexpr u32 NearField = 0;
constexpr u32 FarField = 1;
// The fields' sign bits in VU0's MAC flags as the programs take them (FMAND with 0xF0): x's the highest, w's the lowest
constexpr u32 FirstFieldSign = 0x80;
constexpr u32 FarPlaneSign = FirstFieldSign >> FarField;
constexpr s32 MostInt = 0x7FFFFFFF;

constexpr Matrix4x4 Identity = {
    {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}};

// Four planes as the programs take them, a plane a field: their normals' absolute values and their normals (a row per axis),
// then their offsets
struct PlaneColumns
{
    Vector4 absoluteNormals[3];
    Vector4 normals[3];
    Vector4 offsets;
};

// A box against four planes, a sign bit a plane: wholly behind it, partly behind it
struct PlaneTest
{
    u32 outside;
    u32 crossing;
};

// The camera's frustum's planes in its space, and the same with the sides five times as far out
Vector4 g_ViewPlanes[FrustumPlaneCount];
Vector4 g_FarViewPlanes[FrustumPlaneCount];
// What the programs find in VU0's memory: the planes in the chunk's space (from 0x80: the view's side planes or a portal's, the
// far set's and the near and far planes), the view (from 0x98: its matrices to the clip space and to the screen, the camera's
// place) and the link's (from 0: the view's matrices through the chunk's matrix, the camera through the object's)
PlaneColumns g_SidePlanes;
PlaneColumns g_FarSidePlanes;
PlaneColumns g_NearAndFarPlanes;
Matrix4x4 g_ToClip;
Matrix4x4 g_ToScreen;
Vector4 g_Camera;
Matrix4x4 g_LinkToClip;
Matrix4x4 g_LinkToScreen;
Vector4 g_LinkCamera;
// What the last test left in VU0's registers: whether it's out of the view and whether partly (vi01, vi02), the squared distance
// from the camera (vf15's x) and the matrices to the clip space and to the screen (vf07-vf10, vf11-vf14)
u32 g_Outside;
u32 g_Clipped;
s32 g_Distance;
Matrix4x4 g_Clip;
Matrix4x4 g_Screen;

f32& FieldOf(Vector4& vector, u32 field)
{
    return (&vector.x)[field];
}

Vector4 Scaled(const Vector4& vector, f32 scale)
{
    return {vector.x * scale, vector.y * scale, vector.z * scale, vector.w * scale};
}

Vector4 Sum(const Vector4& a, const Vector4& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

Vector4 Difference(const Vector4& a, const Vector4& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

Vector4 Absolute(const Vector4& vector)
{
    return {__builtin_fabsf(vector.x), __builtin_fabsf(vector.y), __builtin_fabsf(vector.z), __builtin_fabsf(vector.w)};
}

// Three rows weighed by a vector's x, y and z, added up as VU0's accumulator does: (x · first + y · second) + z · third
Vector4 Weighed(const Vector4* rows, const Vector4& by)
{
    return Sum(Sum(Scaled(rows[0], by.x), Scaled(rows[1], by.y)), Scaled(rows[2], by.z));
}

// A row through a matrix (its w weighing the fourth row), and every row of a matrix through another
Vector4 RowThrough(const Vector4& row, const Matrix4x4& matrix)
{
    return Sum(Weighed(RowOf(&matrix, 0), row), Scaled(*RowOf(&matrix, 3), row.w));
}

Matrix4x4 Product(const Matrix4x4& a, const Matrix4x4& b)
{
    Matrix4x4 product;
    for (u32 row = 0; row < 4; row++)
    {
        *RowOf(&product, row) = RowThrough(*RowOf(&a, row), b);
    }

    return product;
}

// The fields' sign bits (a zero's too), as the programs read the MAC flags
u32 SignBits(const Vector4& value)
{
    const f32* fields = &value.x;
    u32 bits = 0;
    for (u32 field = 0; field < PlaneFields; field++)
    {
        if (__builtin_signbitf(fields[field]) != 0)
        {
            bits |= FirstFieldSign >> field;
        }
    }

    return bits;
}

// VU0's FTOI0: towards zero, held at an int's ends past them (where VU0 also puts the infinities and NaNs it doesn't have)
s32 FloatToInt(f32 value)
{
    if (__builtin_isnan(value) || !(__builtin_fabsf(value) < 0x1p31f))
    {
        return __builtin_signbitf(value) != 0 ? -MostInt - 1 : MostInt;
    }

    return static_cast<s32>(value);
}

// A plane put in a field of the columns
void SetColumn(PlaneColumns* columns, const Vector4& plane, u32 field)
{
    const f32* normal = &plane.x;
    for (u32 axis = 0; axis < 3; axis++)
    {
        FieldOf(columns->absoluteNormals[axis], field) = __builtin_fabsf(normal[axis]);
        FieldOf(columns->normals[axis], field) = normal[axis];
    }

    FieldOf(columns->offsets, field) = plane.w;
}

// A frustum's side planes (its second to fifth) as columns
PlaneColumns SideColumns(const Vector4* planes)
{
    PlaneColumns columns;
    for (u32 side = 0; side < SidePlaneCount; side++)
    {
        SetColumn(&columns, planes[FirstSidePlane + side], side);
    }

    return columns;
}

// A frustum's planes taken into a space (the matrix's)
void PlanesIn(const Vector4* frustum, const Matrix4x4* matrix, Vector4* planes)
{
    for (u32 plane = 0; plane < FrustumPlaneCount; plane++)
    {
        planes[plane] = frustum[plane];
    }

    TransformPlanes(planes, matrix);
}

// The frustum of a lens: the near rectangle's half sizes are the near distance times the tangent of half the field of view (and
// the aspect across), times the scale; the planes go through the eye and the near rectangle's corners, the far plane is the near
// one moved the depth along it and turned around
void LensFrustum(f32 near, f32 far, f32 aspect, f32 scale, s32 fieldOfView, Vector4* planes)
{
    s32 half = static_cast<s32>(static_cast<f32>(fieldOfView) * 0.5f);
    f32 height = near * TanOfAngle(&half);
    f32 width = height * aspect * scale;
    height = height * scale;
    Vector4 topRight = {width, height, near, 1.0f};
    Vector4 topLeft = {-width, height, near, 1.0f};
    Vector4 bottomRight = {width, -height, near, 1.0f};
    Vector4 bottomLeft = {-width, -height, near, 1.0f};
    Vector4 eye = {0.0f, 0.0f, 0.0f, 1.0f};
    PlaneFromTriangle(&planes[NearPlane], &topRight, &bottomLeft, &bottomRight);
    Vector4 along = {0.0f, 0.0f, far - near, 1.0f};
    PlaneAlongNormal(&planes[NearPlane], &along, &planes[FarPlane]);
    planes[FarPlane].x = -planes[FarPlane].x;
    planes[FarPlane].y = -planes[FarPlane].y;
    planes[FarPlane].z = -planes[FarPlane].z;
    planes[FarPlane].w = -planes[FarPlane].w;
    PlaneFromTriangle(&planes[FirstSidePlane], &eye, &bottomRight, &topRight);
    PlaneFromTriangle(&planes[FirstSidePlane + 1], &eye, &topLeft, &bottomLeft);
    PlaneFromTriangle(&planes[FirstSidePlane + 2], &eye, &topRight, &topLeft);
    PlaneFromTriangle(&planes[FirstSidePlane + 3], &eye, &bottomLeft, &bottomRight);
}

// A box's middle and half its size from its corners
void BoxMiddle(const Vector4& min, const Vector4& max, Vector4* middle, Vector4* half)
{
    *half = Scaled(Difference(max, min), 0.5f);
    *middle = Sum(Scaled(min, 0.5f), Scaled(max, 0.5f));
}

// A box (its middle and half its size) against four planes: wholly behind one when its middle is farther behind it than the box
// reaches toward it, partly when its middle is less far in front of it than that
PlaneTest TestPlanes(const PlaneColumns& planes, const Vector4& middle, const Vector4& half)
{
    Vector4 reach = Weighed(planes.absoluteNormals, half);
    Vector4 distance = Sum(Weighed(planes.normals, middle), planes.offsets);
    return {SignBits(Sum(distance, reach)), SignBits(Difference(distance, reach))};
}

// Planes taken into a model's space: each normal's share along the model's axes, the offset at the model's place
PlaneColumns PlanesInModel(const PlaneColumns& planes, const Matrix4x4* model)
{
    PlaneColumns moved;
    for (u32 axis = 0; axis < 3; axis++)
    {
        moved.normals[axis] = Weighed(planes.normals, *RowOf(model, axis));
        moved.absoluteNormals[axis] = Absolute(moved.normals[axis]);
    }

    moved.offsets = Sum(Weighed(planes.normals, *RowOf(model, 3)), planes.offsets);
    return moved;
}

// The squared distance between the camera (in the link's space) and a model's place: the level of detail's distance, an int
s32 CameraDistance(const Matrix4x4* model)
{
    Vector4 apart = Difference(g_LinkCamera, *RowOf(model, 3));
    return FloatToInt(apart.x * apart.x + apart.y * apart.y + apart.z * apart.z);
}
}

namespace DesktopGraphics
{
void ChunkViewPlanes(const Matrix4x4* matrix, Vector4* sides, Vector4* nearPlane)
{
    for (u32 side = 0; side < SidePlaneCount; side++)
    {
        TransformPlaneOf(g_FarViewPlanes, static_cast<s32>(FirstSidePlane + side), matrix, &sides[side]);
    }

    TransformPlaneOf(g_ViewPlanes, NearPlane, matrix, nearPlane);
}
}

namespace Platform::Graphics
{
void SetViewFrustum(f32 near, f32 far, f32 aspect, const s32* fieldOfView)
{
    LensFrustum(near, far, aspect, 1.0f, *fieldOfView, g_ViewPlanes);
    LensFrustum(near, far, aspect, FarSidesScale, *fieldOfView, g_FarViewPlanes);
}

const Vector4* ViewPlanes()
{
    return g_ViewPlanes;
}

void LoadChunkPlanes(const Matrix4x4* place)
{
    Vector4 planes[FrustumPlaneCount];
    PlanesIn(g_ViewPlanes, place, planes);
    g_SidePlanes = SideColumns(planes);
    g_NearAndFarPlanes = {};
    SetColumn(&g_NearAndFarPlanes, planes[NearPlane], NearField);
    SetColumn(&g_NearAndFarPlanes, planes[FarPlane], FarField);
    PlanesIn(g_FarViewPlanes, place, planes);
    g_FarSidePlanes = SideColumns(planes);
}

// The portal's side planes are in the chunk's space already, and there's no far plane
void LoadPortalPlanes(const Matrix4x4* place, const Vector4* portal)
{
    g_SidePlanes = SideColumns(portal);
    Vector4 planes[FrustumPlaneCount];
    PlanesIn(g_FarViewPlanes, place, planes);
    g_FarSidePlanes = SideColumns(planes);
    Vector4 nearPlane;
    TransformPlaneOf(g_ViewPlanes, NearPlane, place, &nearPlane);
    g_NearAndFarPlanes = {};
    SetColumn(&g_NearAndFarPlanes, nearPlane, NearField);
}

// The link's matrices the identity
void LoadCullingView(const Matrix4x4* toClip, const Matrix4x4* toScreen, const Vector4* camera)
{
    g_ToClip = *toClip;
    g_ToScreen = *toScreen;
    g_Camera = *camera;
    CullLoadLink(&Identity, &Identity);
}

// 0x398: the camera's place through the object's matrix, ((its fourth row + x · the first) + y · the second) + z · the third, and
// the view's matrices through the chunk's
void CullLoadLink(const Matrix4x4* chunk, const Matrix4x4* object)
{
    Vector4 camera = Sum(*RowOf(object, 3), Scaled(*RowOf(object, 0), g_Camera.x));
    camera = Sum(camera, Scaled(*RowOf(object, 1), g_Camera.y));
    g_LinkCamera = Sum(camera, Scaled(*RowOf(object, 2), g_Camera.z));
    g_LinkToClip = Product(*chunk, g_ToClip);
    g_LinkToScreen = Product(*chunk, g_ToScreen);
}

// 0x2D0: the model's matrix to the screen and its distance
void CullLoadModel(const Matrix4x4* model)
{
    g_Screen = Product(*model, g_LinkToScreen);
    g_Distance = CameraDistance(model);
}

// 0x1C0: out of the view when wholly behind a side, the near or the far plane, partly when crossing any
void CullTestBox(const Vector4* corners)
{
    Vector4 middle;
    Vector4 half;
    BoxMiddle(corners[0], corners[1], &middle, &half);
    PlaneTest ends = TestPlanes(g_NearAndFarPlanes, middle, half);
    PlaneTest sides = TestPlanes(g_SidePlanes, middle, half);
    g_Outside = (sides.outside & sides.crossing) | (ends.outside & ends.crossing);
    g_Clipped = sides.outside | sides.crossing | ends.outside | ends.crossing;
}

// 0x500: the planes taken into the model's space. Out of the view when wholly behind a side, the near or the far plane, or
// crossing the far plane (its outcome then: the sides it's behind and the far plane); else partly when crossing the far set's
// sides or the near plane, with the matrices to the screen and, partly, to the clip space. The distance either way
void CullTestBoxAt(const Box* box, const Matrix4x4* model)
{
    Vector4 middle;
    Vector4 half;
    BoxMiddle(box->min, box->max, &middle, &half);
    PlaneTest ends = TestPlanes(PlanesInModel(g_NearAndFarPlanes, model), middle, half);
    PlaneTest sides = TestPlanes(PlanesInModel(g_SidePlanes, model), middle, half);
    g_Distance = CameraDistance(model);
    g_Clipped = (sides.outside & sides.crossing) | ((ends.outside | ends.crossing) & FarPlaneSign);
    g_Outside = g_Clipped | (ends.outside & ends.crossing);
    if (g_Outside != 0)
    {
        return;
    }

    PlaneTest farSides = TestPlanes(PlanesInModel(g_FarSidePlanes, model), middle, half);
    g_Clipped = farSides.outside | farSides.crossing | ends.outside | ends.crossing;
    g_Screen = Product(*model, g_LinkToScreen);
    if (g_Clipped != 0)
    {
        g_Clip = Product(*model, g_LinkToClip);
    }
}

CullOutcome CullResult()
{
    return {g_Outside, g_Clipped, g_Distance};
}

void CullMatrices(Matrix4x4* clipped, Matrix4x4* toScreen)
{
    if (clipped != nullptr)
    {
        *clipped = g_Clip;
    }

    *toScreen = g_Screen;
}
}
