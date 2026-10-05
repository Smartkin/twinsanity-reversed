#include "renderer.h"

#include "game/math.h"
#include "game/scenery.h"
#include "platform/graphics.h"

// The chunks' culling runs on VU0: its microprograms of the second set test what the scenery draws against the view loaded into
// its memory (the frustum's planes as columns from 0x80, the view's matrices from 0x98) and leave the outcome in its registers
// (vi01 set when it's out of the view, vi02 when it's partly out, the level of detail's distance in vf15's x, the matrices it
// worked out in vf07-vf10 and vf11-vf14); macro mode clips a box's corners against the view's matrix

EABI_EXPORT(FUN_00201368, ViewFrustumPlanes);
EABI_EXPORT(FUN_00201480, FrustumPlanes);
EABI_EXPORT(FUN_001ef410, Platform::Graphics::SetViewFrustum);


namespace
{
// Where the culling's data goes in VU0's memory
constexpr s32 PlanesAddress = 0x80;
constexpr s32 ViewAddress = 0x98;
// A plane columns' quadwords
constexpr s32 PlaneColumnRows = sizeof(PlaneColumns) / sizeof(Vector4);
// The far set's sides are five times as far out
constexpr f32 FarSidesScale = 5.0f;
}

extern "C"
{
BigVu0Packet* StartBigVu0Packet(BigVu0Packet* packet)
{
    packet->capacity = BigVu0Packet::Capacity;
    packet->count = 0;
    packet->unused04 = 0;
    return packet;
}

// A plane's normal (and its absolute value) and offset put in a column of the four
void SetPlaneColumn(PlaneColumns* columns, const Vector4* plane, s32 column)
{
    Vector4 normal = *plane;
    Vector4 absolute = normal;
    absolute.x = __builtin_fabsf(absolute.x);
    absolute.y = __builtin_fabsf(absolute.y);
    absolute.z = __builtin_fabsf(absolute.z);
    const f32* from = &absolute.x;
    const f32* signedFrom = &normal.x;
    for (s32 axis = 0; axis < 3; axis++)
    {
        columns->absoluteNormals[axis][column] = from[axis];
        columns->normals[axis][column] = signedFrom[axis];
    }

    columns->offsets[column] = plane->w;
}

void SetSidePlaneColumns(PlaneColumns* columns, const Vector4* planes)
{
    for (u32 column = 0; column < SidePlaneCount; column++)
    {
        SetPlaneColumn(columns, &planes[column + FirstSidePlane], static_cast<s32>(column));
    }
}

void AddPlaneColumns(const PlaneColumns* columns, BigVu0Packet* packet)
{
    const auto* rows = reinterpret_cast<const Vector4*>(columns);
    for (s32 row = 0; row < PlaneColumnRows; row++)
    {
        *reinterpret_cast<Vector4*>(packet->data[packet->count]) = rows[row];
        packet->unused04 = 0;
        packet->count++;
    }
}

void ClearPlaneColumns(PlaneColumns* columns)
{
    auto* rows = reinterpret_cast<Vector4*>(columns);
    for (s32 row = 0; row < PlaneColumnRows; row++)
    {
        rows[row] = {0.0f, 0.0f, 0.0f, 0.0f};
    }
}





// The frustum's planes through the eye and the near rectangle's corners, the far plane the near one moved the depth along it
// and turned around
void FrustumPlanes(f32 depth, Vector4* planes, const Vector4* eye, const Vector4* topRight, const Vector4* bottomRight,
                   const Vector4* bottomLeft, const Vector4* topLeft)
{
    PlaneFromTriangle(&planes[NearPlane], topRight, bottomLeft, bottomRight);
    Vector4 along = {0.0f, 0.0f, depth, 1.0f};
    PlaneAlongNormal(&planes[NearPlane], &along, &planes[FarPlane]);
    planes[FarPlane].x = -planes[FarPlane].x;
    planes[FarPlane].y = -planes[FarPlane].y;
    planes[FarPlane].z = -planes[FarPlane].z;
    planes[FarPlane].w = -planes[FarPlane].w;
    // The sides: right, left, top and bottom
    PlaneFromTriangle(&planes[FirstSidePlane], eye, bottomRight, topRight);
    PlaneFromTriangle(&planes[FirstSidePlane + 1], eye, topLeft, bottomLeft);
    PlaneFromTriangle(&planes[FirstSidePlane + 2], eye, topRight, topLeft);
    PlaneFromTriangle(&planes[FirstSidePlane + 3], eye, bottomLeft, bottomRight);
}

// The near rectangle's half sizes are the near distance times the tangent of half the field of view (and the aspect across),
// times the scale
void ViewFrustumPlanes(f32 near, f32 far, f32 aspect, f32 scale, Vector4* planes, const s32* fieldOfView)
{
    s32 half = static_cast<s32>(static_cast<f32>(*fieldOfView) * 0.5f);
    f32 height = near * TanOfAngle(&half);
    f32 width = height * aspect * scale;
    height = height * scale;
    Vector4 topRight = {width, height, near, 1.0f};
    Vector4 topLeft = {-width, height, near, 1.0f};
    Vector4 bottomRight = {width, -height, near, 1.0f};
    Vector4 bottomLeft = {-width, -height, near, 1.0f};
    Vector4 eye = {0.0f, 0.0f, 0.0f, 1.0f};
    FrustumPlanes(far - near, planes, &eye, &topRight, &bottomRight, &bottomLeft, &topLeft);
}

// The view's planes taken into a space: the side planes as columns, the near and far ones as they are
void ViewPlanesIn(const Matrix4x4* matrix, PlaneColumns* columns, Vector4* nearAndFar)
{
    Vector4 planes[FrustumPlaneCount];
    for (u32 plane = 0; plane < FrustumPlaneCount; plane++)
    {
        planes[plane] = g_ViewPlanes[plane];
    }

    TransformPlanes(planes, matrix);
    SetSidePlaneColumns(columns, planes);
    nearAndFar[0] = planes[NearPlane];
    nearAndFar[1] = planes[FarPlane];
}

void FarViewPlanesIn(const Matrix4x4* matrix, PlaneColumns* columns)
{
    Vector4 planes[FrustumPlaneCount];
    for (u32 plane = 0; plane < FrustumPlaneCount; plane++)
    {
        planes[plane] = g_FarViewPlanes[plane];
    }

    TransformPlanes(planes, matrix);
    SetSidePlaneColumns(columns, planes);
}

// The far set's side planes taken into a space as rows, and the near plane
u32* WriteChunkViewRows(u32* nearPlane, const Matrix4x4* matrix, u32* rows)
{
    auto* row = reinterpret_cast<Vector4*>(rows);
    for (s32 plane = FirstSidePlane; plane < static_cast<s32>(FarPlane); plane++)
    {
        TransformPlaneOf(g_FarViewPlanes, plane, matrix, row);
        row++;
    }

    Vector4 near;
    TransformPlaneOf(g_ViewPlanes, NearPlane, matrix, &near);
    *reinterpret_cast<Vector4*>(nearPlane) = near;
    return nearPlane;
}

// The particles' view: the frustum's planes in the chunk's space (the sides as columns, then the near and far planes as columns),
// the camera's place and the second matrix, sent as the index'th view
s32 UploadParticleView(Matrix4x4* matrices, s32 index)
{
    PlaneColumns columns;
    Vector4 nearAndFar[2];
    BigVu0Packet packet;
    ViewPlanesIn(&matrices[0], &columns, nearAndFar);
    StartBigVu0Packet(&packet);
    AddPlaneColumns(&columns, &packet);
    ClearPlaneColumns(&columns);
    SetPlaneColumn(&columns, &nearAndFar[0], 0);
    SetPlaneColumn(&columns, &nearAndFar[1], 1);
    AddPlaneColumns(&columns, &packet);
    // The camera's place (its matrix's fourth row) and the second matrix
    *reinterpret_cast<Vector4*>(packet.data[packet.count]) = *RowOf(&matrices[0], 3);
    packet.unused04 = 0;
    packet.count++;
    for (s32 row = 0; row < 4; row++)
    {
        *reinterpret_cast<Vector4*>(packet.data[packet.count + row]) = *RowOf(&matrices[1], row);
    }

    packet.unused04 = 0;
    packet.count += 4;
    s32 address = index * packet.count;
    SendToVu0(g_Vu0Programs, packet.data, packet.count, address);
    *reinterpret_cast<s32*>(&matrices[4]) = address;
    return address;
}

// A box's eight corners through the view's matrix (vf10-vf13) and scaled by its first guard row (vf09), clipped: the flags of
// either set or of every corner, the corners' outside bits taken together (the scaled ones in the low six) and their common bits
void ClipCorners(u32* flags)
{
    asm volatile("vmulax.xyzw $ACC, $vf10, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf1y\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf1z\n\t"
                 "vmaddw.xyzw $vf1, $vf13, $vf0w\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf2x\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf2y\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf2z\n\t"
                 "vmul.xyzw $vf17, $vf1, $vf9\n\t"
                 "vmaddw.xyzw $vf2, $vf13, $vf0w\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf3x\n\t"
                 "vclipw.xyz $vf1, $vf1w\n\t"
                 "vclipw.xyz $vf17, $vf17w\n\t"
                 "vmul.xyzw $vf18, $vf2, $vf9\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf3y\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf3z\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a4, $a4\n\t"
                 "and $a6, $a4, $a4\n\t"
                 "vclipw.xyz $vf2, $vf2w\n\t"
                 "vclipw.xyz $vf18, $vf18w\n\t"
                 "vmaddw.xyzw $vf3, $vf13, $vf0w\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf4x\n\t"
                 "vmul.xyzw $vf19, $vf3, $vf9\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vclipw.xyz $vf3, $vf3w\n\t"
                 "vclipw.xyz $vf19, $vf19w\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf4y\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf4z\n\t"
                 "vmaddw.xyzw $vf4, $vf13, $vf0w\n\t"
                 "vmul.xyzw $vf20, $vf4, $vf9\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf5x\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf5y\n\t"
                 "vclipw.xyz $vf4, $vf4w\n\t"
                 "vclipw.xyz $vf20, $vf20w\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf5z\n\t"
                 "vmaddw.xyzw $vf5, $vf13, $vf0w\n\t"
                 "vmul.xyzw $vf21, $vf5, $vf9\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf6x\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf6y\n\t"
                 "vclipw.xyz $vf5, $vf5w\n\t"
                 "vclipw.xyz $vf21, $vf21w\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf6z\n\t"
                 "vmaddw.xyzw $vf6, $vf13, $vf0w\n\t"
                 "vmul.xyzw $vf22, $vf6, $vf9\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf7x\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf7y\n\t"
                 "vclipw.xyz $vf6, $vf6w\n\t"
                 "vclipw.xyz $vf22, $vf22w\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf7z\n\t"
                 "vmaddw.xyzw $vf7, $vf13, $vf0w\n\t"
                 "vmul.xyzw $vf23, $vf7, $vf9\n\t"
                 "vmulax.xyzw $ACC, $vf10, $vf8x\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vmadday.xyzw $ACC, $vf11, $vf8y\n\t"
                 "vclipw.xyz $vf7, $vf7w\n\t"
                 "vclipw.xyz $vf23, $vf23w\n\t"
                 "vmaddaz.xyzw $ACC, $vf12, $vf8z\n\t"
                 "vmaddw.xyzw $vf8, $vf13, $vf0w\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "vmul.xyzw $vf24, $vf8, $vf9\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vclipw.xyz $vf8, $vf8w\n\t"
                 "vclipw.xyz $vf24, $vf24w\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "cfc2.ni $a4, $vi18\n\t"
                 "or $a5, $a5, $a4\n\t"
                 "and $a6, $a6, $a4\n\t"
                 "sw $a5, 0(%0)\n\t"
                 "sw $a6, 4(%0)"
                 :
                 : "r"(flags)
                 : "$8", "$9", "$10", "memory");
}

// The box's corners: each takes the box's maximum in one axis (the first three) or its minimum in one axis (the last three) and
// the other end in the others, the two corners themselves first
static void LoadBoxCorners(const Box* box)
{
    asm volatile("lqc2 $vf1, 0x0(%0)\n\t"
                 "lqc2 $vf2, 0x10(%0)\n\t"
                 "vmove.xyzw $vf3, $vf1\n\t"
                 "vmove.xyzw $vf4, $vf1\n\t"
                 "vmove.xyzw $vf5, $vf1\n\t"
                 "vmove.xyzw $vf6, $vf2\n\t"
                 "vmove.xyzw $vf7, $vf2\n\t"
                 "vmove.xyzw $vf8, $vf2\n\t"
                 "vmove.x $vf3, $vf2\n\t"
                 "vmove.y $vf4, $vf2\n\t"
                 "vmove.z $vf5, $vf2\n\t"
                 "vmove.x $vf6, $vf1\n\t"
                 "vmove.y $vf7, $vf1\n\t"
                 "vmove.z $vf8, $vf1"
                 :
                 : "r"(box)
                 : "memory");
}

}

namespace Platform::Graphics
{
const Vector4* ViewPlanes()
{
    return g_ViewPlanes;
}

void ClipBoxAt(const Matrix4x4* view, const Box* box, u32* flags, const Matrix4x4* model)
{
    LoadBoxCorners(box);
    asm volatile("lqc2 $vf9, 0x40(%0)\n\t"
                 "lqc2 $vf24, 0x0(%0)\n\t"
                 "lqc2 $vf25, 0x10(%0)\n\t"
                 "lqc2 $vf26, 0x20(%0)\n\t"
                 "lqc2 $vf27, 0x30(%0)\n\t"
                 "lqc2 $vf28, 0x0(%1)\n\t"
                 "lqc2 $vf29, 0x10(%1)\n\t"
                 "lqc2 $vf30, 0x20(%1)\n\t"
                 "lqc2 $vf31, 0x30(%1)\n\t"
                 "vmulax.xyzw $ACC, $vf24, $vf28x\n\t"
                 "vmadday.xyzw $ACC, $vf25, $vf28y\n\t"
                 "vmaddaz.xyzw $ACC, $vf26, $vf28z\n\t"
                 "vmaddw.xyzw $vf10, $vf27, $vf28w\n\t"
                 "vmulax.xyzw $ACC, $vf24, $vf29x\n\t"
                 "vmadday.xyzw $ACC, $vf25, $vf29y\n\t"
                 "vmaddaz.xyzw $ACC, $vf26, $vf29z\n\t"
                 "vmaddw.xyzw $vf11, $vf27, $vf29w\n\t"
                 "vmulax.xyzw $ACC, $vf24, $vf30x\n\t"
                 "vmadday.xyzw $ACC, $vf25, $vf30y\n\t"
                 "vmaddaz.xyzw $ACC, $vf26, $vf30z\n\t"
                 "vmaddw.xyzw $vf12, $vf27, $vf30w\n\t"
                 "vmulax.xyzw $ACC, $vf24, $vf31x\n\t"
                 "vmadday.xyzw $ACC, $vf25, $vf31y\n\t"
                 "vmaddaz.xyzw $ACC, $vf26, $vf31z\n\t"
                 "vmaddw.xyzw $vf13, $vf27, $vf31w"
                 :
                 : "r"(view), "r"(model)
                 : "memory");
    ClipCorners(flags);
}

void ClipBox(const Matrix4x4* view, const Box* box, u32* flags)
{
    LoadBoxCorners(box);
    asm volatile("lqc2 $vf9, 0x40(%0)\n\t"
                 "lqc2 $vf10, 0x0(%0)\n\t"
                 "lqc2 $vf11, 0x10(%0)\n\t"
                 "lqc2 $vf12, 0x20(%0)\n\t"
                 "lqc2 $vf13, 0x30(%0)"
                 :
                 : "r"(view)
                 : "memory");
    ClipCorners(flags);
}

void SetViewFrustum(f32 near, f32 far, f32 aspect, const s32* fieldOfView)
{
    s32 angle = *fieldOfView;
    ViewFrustumPlanes(near, far, aspect, 1.0f, g_ViewPlanes, &angle);
    angle = *fieldOfView;
    ViewFrustumPlanes(near, far, aspect, FarSidesScale, g_FarViewPlanes, &angle);
}

void LoadChunkPlanes(const Matrix4x4* place)
{
    PlaneColumns columns;
    Vector4 nearAndFar[2];
    BigVu0Packet packet;
    ViewPlanesIn(place, &columns, nearAndFar);
    StartBigVu0Packet(&packet);
    AddPlaneColumns(&columns, &packet);
    FarViewPlanesIn(place, &columns);
    AddPlaneColumns(&columns, &packet);
    ClearPlaneColumns(&columns);
    SetPlaneColumn(&columns, &nearAndFar[0], 0);
    SetPlaneColumn(&columns, &nearAndFar[1], 1);
    AddPlaneColumns(&columns, &packet);
    SendToVu0(g_Vu0Programs, packet.data, packet.count, PlanesAddress);
}

void LoadPortalPlanes(const Matrix4x4* place, const Vector4* portal)
{
    PlaneColumns columns;
    Vector4 near;
    BigVu0Packet packet;
    SetSidePlaneColumns(&columns, portal);
    StartBigVu0Packet(&packet);
    AddPlaneColumns(&columns, &packet);
    FarViewPlanesIn(place, &columns);
    AddPlaneColumns(&columns, &packet);
    ClearPlaneColumns(&columns);
    TransformPlaneOf(g_ViewPlanes, NearPlane, place, &near);
    SetPlaneColumn(&columns, &near, 0);
    AddPlaneColumns(&columns, &packet);
    SendToVu0(g_Vu0Programs, packet.data, packet.count, PlanesAddress);
}

// The view's matrix, the renderer view's world to screen matrix and the camera's place, the second set selected and the model
// and link matrices made the identity
void LoadCullingView(const Matrix4x4* toClip, const Matrix4x4* toScreen, const Vector4* camera)
{
    BigVu0Packet packet;
    StartBigVu0Packet(&packet);
    for (s32 row = 0; row < 4; row++)
    {
        *reinterpret_cast<Vector4*>(packet.data[packet.count + row]) = reinterpret_cast<const Vector4*>(toClip)[row];
    }

    packet.unused04 = 0;
    packet.count += 4;
    for (s32 row = 0; row < 4; row++)
    {
        *reinterpret_cast<Vector4*>(packet.data[packet.count + row]) = reinterpret_cast<const Vector4*>(toScreen)[row];
    }

    packet.unused04 = 0;
    packet.count += 4;
    *reinterpret_cast<Vector4*>(packet.data[packet.count]) = *camera;
    packet.unused04 = 0;
    packet.count++;
    SendToVu0(g_Vu0Programs, packet.data, packet.count, ViewAddress);
    SelectVu0Programs(g_Vu0Programs, CullingPrograms, true);
    Matrix4x4 identity;
    InitIdentityMatrix(&identity);
    CullLoadLink(&identity, &identity);
}

void CullLoadLink(const Matrix4x4* chunk, const Matrix4x4* object)
{
    asm volatile("lqc2 $vf3, 0x0(%0)\n\t"
                 "lqc2 $vf4, 0x10(%0)\n\t"
                 "lqc2 $vf5, 0x20(%0)\n\t"
                 "lqc2 $vf6, 0x30(%0)\n\t"
                 "lqc2 $vf7, 0x0(%1)\n\t"
                 "lqc2 $vf8, 0x10(%1)\n\t"
                 "lqc2 $vf9, 0x20(%1)\n\t"
                 "lqc2 $vf10, 0x30(%1)\n\t"
                 "vcallms 0x398"
                 :
                 : "r"(chunk), "r"(object)
                 : "memory");
}

void CullLoadModel(const Matrix4x4* model)
{
    asm volatile("vnop\n\t"
                 "lqc2 $vf3, 0x0(%0)\n\t"
                 "lqc2 $vf4, 0x10(%0)\n\t"
                 "lqc2 $vf5, 0x20(%0)\n\t"
                 "lqc2 $vf6, 0x30(%0)\n\t"
                 "vcallms 0x2D0"
                 :
                 : "r"(model)
                 : "memory");
}

void CullTestBox(const Vector4* corners)
{
    asm volatile("lq $2, 0x10(%0)\n\t"
                 "lq $3, 0x0(%0)\n\t"
                 "vnop\n\t"
                 "qmtc2.ni $3, $vf1\n\t"
                 "qmtc2.ni $2, $vf2\n\t"
                 "vcallms 0x1C0"
                 :
                 : "r"(corners)
                 : "$2", "$3", "memory");
}

// Loaded through the integer registers after the wait, like the box of CullTestBox: the last test may still be using them
void CullTestBoxAt(const Box* box, const Matrix4x4* model)
{
    asm volatile("lq $12, 0x10(%0)\n\t"
                 "lq $2, 0x0(%0)\n\t"
                 "lq $13, 0x30(%1)\n\t"
                 "lq $3, 0x0(%1)\n\t"
                 "lq $14, 0x10(%1)\n\t"
                 "lq $15, 0x20(%1)\n\t"
                 "vnop\n\t"
                 "qmtc2.ni $2, $vf1\n\t"
                 "qmtc2.ni $12, $vf2\n\t"
                 "qmtc2.ni $3, $vf3\n\t"
                 "qmtc2.ni $14, $vf4\n\t"
                 "qmtc2.ni $15, $vf5\n\t"
                 "qmtc2.ni $13, $vf6\n\t"
                 "vcallms 0x500"
                 :
                 : "r"(box), "r"(model)
                 : "$2", "$3", "$12", "$13", "$14", "$15", "memory");
}

CullOutcome CullResult()
{
    u32 outside;
    u32 clipped;
    u32 distance;
    asm volatile("vnop\n\t"
                 "qmfc2.ni %2, $vf15\n\t"
                 "sll %2, %2, 0\n\t"
                 "cfc2.ni %1, $vi2\n\t"
                 "cfc2.ni %0, $vi1"
                 : "=r"(outside), "=r"(clipped), "=r"(distance));
    return {outside, clipped, static_cast<s32>(distance)};
}

void CullMatrices(Matrix4x4* clipped, Matrix4x4* toScreen)
{
    if (clipped != nullptr)
    {
        asm volatile("sqc2 $vf7, 0x0(%0)\n\t"
                     "sqc2 $vf8, 0x10(%0)\n\t"
                     "sqc2 $vf9, 0x20(%0)\n\t"
                     "sqc2 $vf10, 0x30(%0)"
                     :
                     : "r"(clipped)
                     : "memory");
    }

    asm volatile("sqc2 $vf11, 0x0(%0)\n\t"
                 "sqc2 $vf12, 0x10(%0)\n\t"
                 "sqc2 $vf13, 0x20(%0)\n\t"
                 "sqc2 $vf14, 0x30(%0)"
                 :
                 : "r"(toScreen)
                 : "memory");
}
}
