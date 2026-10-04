#include "game/vehicles.h"

#include "game/collision.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/view.h"
#include "platform/graphics.h"

#include <string.h>

EABI_EXPORT(FUN_0015bd20, LaySkidMark);

namespace
{
constexpr s32 MarkCount = 100;
constexpr s32 FreedListSize = 1024;
// A trail ends after 4 frames without a mark
constexpr s32 IdleFramesToEnd = 4;
// The surfaces that take marks (their bit 11, TT Lab's LeavesFootprints)
constexpr u32 SurfaceTakesMarks = 0x800;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;

// A trail fades in over its first 2.5 units: from 0.4 of its length (kept within 0 and 1.1) the width by its 4th root, the depth
// by its 4th power
constexpr f32 FadeLength = 2.5f;
constexpr f32 FadeScale = Rounded(0.4);
constexpr f32 FadeMost = Rounded(1.1);
// Each mark's depth is jittered within 0.65 to 1.15 of it, its point by 0.02 along the side
constexpr f32 DepthJitter = 0.25f;
constexpr f32 DepthBase = 0.9f;
constexpr f32 SideJitter = 0.02f;
// A mark is laid 0.3 from the last at least, on a stretch shorter than 1.5, on the triangles no more than 0.5 above it
constexpr f32 MarkSpacing = Rounded(0.3);
constexpr f32 LongestStretch = 1.5f;
constexpr f32 MostBelow = 0.5f;
// A strip's shade: its slope's facing toward the light (1.2, 0.8, 0.5), 48 times it less 24
constexpr Vector4 ShadeLight = {Rounded(1.2), Rounded(0.8), 0.5f, 1.0f};
constexpr f32 ShadeScale = 48.0f;
constexpr f32 ShadeOffset = 24.0f;
// White without alpha: the colour a strip's far low corner gets, and the next strip's near low one
constexpr u32 White = 0x00FFFFFF;
// The marks are drawn with the screen matrix's depth offset 15% larger, against the ground they lie on
constexpr f32 DrawnDepthBack = Rounded(0.15);

// A strip between two points on a triangle, the depth wide: its top corners on the triangle, its low ones lowered by the depth
// and moved back along the side by it, shaded by the slope (white, its alpha the shade's size, added or taken away by the
// shade's sign). Its near corners take the colours the last strip left, and it replaces the ring's oldest mark
void LayStrip(SkidMarks* marks, const Vector4* nearTop, const Vector4* farTop, const Vector4* side, f32 depth)
{
    Platform::Graphics::BeginScreenModel();
    // (Adding a vector of zeros to the top corners only turns their -0s into 0s)
    const Vector4 raise = {0.0f, 0.0f, 0.0f, 1.0f};
    const Vector4 lower = {0.0f, -depth, 0.0f, 1.0f};
    const Vector4 back = {side->x * depth, side->y * depth, side->z * depth, 1.0f};
    Vector4 corners[4];
    corners[0] = {nearTop->x + raise.x, nearTop->y + raise.y, nearTop->z + raise.z, 1.0f};
    corners[1] = {farTop->x + raise.x, farTop->y + raise.y, farTop->z + raise.z, 1.0f};
    corners[2] = {nearTop->x + lower.x - back.x, nearTop->y + lower.y - back.y, nearTop->z + lower.z - back.z, 1.0f};
    corners[3] = {farTop->x + lower.x - back.x, farTop->y + lower.y - back.y, farTop->z + lower.z - back.z, 1.0f};
    Vector4 normal;
    PlaneThroughTriangle(&normal, &corners[0], &corners[2], &corners[3]);
    Vector4 light = ShadeLight;
    f32 inverse = InverseLength(&light, LengthEpsilon);
    light.x = light.x * inverse;
    light.y = light.y * inverse;
    light.z = light.z * inverse;
    if (normal.y < 0.0f)
    {
        normal.y = -normal.y;
        normal.x = -normal.x;
        normal.z = -normal.z;
    }

    s32 shade = static_cast<s32>((normal.x * light.x + normal.y * light.y + normal.z * light.z) * ShadeScale - ShadeOffset);
    u32 alpha = shade < 0 ? 0u - static_cast<u32>(shade) : static_cast<u32>(shade);
    u32 colour = (alpha << 24) + White;
    Platform::Graphics::SetScreenModelMaterial(shade > 0 ? g_AddingSkidMaterial : g_SubtractingSkidMaterial);
    Platform::Graphics::SetScreenModelColour(marks->edgeColour);
    Platform::Graphics::AddScreenModelVertex(&corners[0]);
    Platform::Graphics::SetScreenModelColour(colour);
    Platform::Graphics::AddScreenModelVertex(&corners[1]);
    Platform::Graphics::SetScreenModelColour(marks->lowColour);
    Platform::Graphics::AddScreenModelVertex(&corners[2]);
    Platform::Graphics::SetScreenModelColour(White);
    Platform::Graphics::AddScreenModelVertex(&corners[3]);
    marks->lowColour = White;
    marks->edgeColour = colour;
    FreeSkidMark(marks->marks[marks->next]);
    marks->marks[marks->next] = Platform::Graphics::EndScreenModel();
    marks->next++;
    if (marks->next == MarkCount)
    {
        marks->next = 0;
    }
}
}

void InitFreedMemory()
{
    memset(g_FreedBlocks, 0, sizeof(g_FreedBlocks));
    memset(g_LastFreedBlocks, 0, sizeof(g_LastFreedBlocks));
    g_FreedBlockCount = 0;
    g_LastFreedBlockCount = 0;
    g_AddingSkidMaterial = Platform::Graphics::MakeSkidMaterial(false);
    g_SubtractingSkidMaterial = Platform::Graphics::MakeSkidMaterial(true);
}

void ReleaseFreedMemory()
{
    for (s32 index = 0; index < g_LastFreedBlockCount; index++)
    {
        ScreenModel* mark = g_LastFreedBlocks[index];
        if (mark != nullptr)
        {
            Platform::Graphics::DeleteScreenModel(mark);
        }
    }

    if (g_FreedBlockCount > 0 || g_LastFreedBlockCount > 0)
    {
        for (s32 index = 0; index < FreedListSize; index++)
        {
            g_LastFreedBlocks[index] = g_FreedBlocks[index];
            g_FreedBlocks[index] = nullptr;
        }
    }

    g_LastFreedBlockCount = g_FreedBlockCount;
    g_FreedBlockCount = 0;
}

SkidMarks* ConstructSkidMarks(SkidMarks* marks)
{
    memset(marks->marks, 0, sizeof(marks->marks));
    marks->next = 0;
    marks->active = 0;
    marks->length = 0.0f;
    marks->idleFrames = 0;
    return marks;
}

void DestroySkidMarks(SkidMarks* marks, u32 destroyFlags)
{
    for (s32 index = 0; index < MarkCount; index++)
    {
        FreeSkidMark(marks->marks[index]);
        marks->marks[index] = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(marks);
    }
}

void FreeSkidMark(ScreenModel* mark)
{
    if (mark != nullptr)
    {
        g_FreedBlocks[g_FreedBlockCount++] = mark;
    }
}

void ClearSkidMarks(SkidMarks* marks)
{
    for (s32 index = 0; index < MarkCount; index++)
    {
        FreeSkidMark(marks->marks[index]);
        marks->marks[index] = nullptr;
    }

    marks->length = 0.0f;
    marks->active = 0;
}

void IdleSkidMarks(SkidMarks* marks)
{
    marks->idleFrames++;
    if (marks->idleFrames >= IdleFramesToEnd)
    {
        marks->length = 0.0f;
        marks->idleFrames = 0;
        marks->active = 0;
    }
}

void LaySkidMark(f32 offset, f32 depth, SkidMarks* marks, const Vector4* point, const Vector4* side, CollisionCache* cache)
{
    if (marks->length < FadeLength)
    {
        f32 fade = marks->length * FadeScale;
        if (fade < 0.0f)
        {
            fade = 0.0f;
        }

        if (FadeMost < fade)
        {
            fade = FadeMost;
        }

        offset = offset * __builtin_sqrtf(__builtin_sqrtf(fade));
        f32 square = fade * fade;
        depth = depth * (square * square);
    }

    marks->idleFrames = 0;
    depth = depth * (RandomSignedTimes(DepthJitter) + DepthBase);
    Vector4 at = *point;
    at.x = at.x + side->x * offset;
    at.y = at.y + side->y * offset;
    at.z = at.z + side->z * offset;
    f32 jitter = RandomSignedTimes(SideJitter);
    at.x = at.x + side->x * jitter;
    at.y = at.y + side->y * jitter;
    at.z = at.z + side->z * jitter;
    f32 dx = at.x - marks->lastPoint.x;
    f32 dy = at.y - marks->lastPoint.y;
    f32 dz = at.z - marks->lastPoint.z;
    f32 distance = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    if (marks->active != 0 && !(MarkSpacing < distance))
    {
        return;
    }

    marks->length = marks->length + distance;
    if (distance < LongestStretch)
    {
        for (CollisionHit* triangle = FirstCollisionHit(cache); triangle != nullptr; triangle = NextCollisionHit(cache))
        {
            if ((GetTriangleSurface(triangle)->collisionMask & SurfaceTakesMarks) == 0)
            {
                continue;
            }

            Vector4 from;
            Vector4 to;
            if (ClipToTriangle(triangle, &marks->lastPoint, &at, &from, &to) == 0)
            {
                continue;
            }

            Vector4 nearTop;
            Vector4 farTop;
            DropOntoTriangle(&from, triangle, &nearTop);
            DropOntoTriangle(&to, triangle, &farTop);
            // Retail tests the near end twice: the far end's test was surely meant, so a far end deep under the triangle passes
            if (from.y + MostBelow < nearTop.y)
            {
                continue;
            }

            LayStrip(marks, &nearTop, &farTop, side, depth);
        }
    }

    marks->active = 1;
    marks->lastLow = at;
    marks->lastPoint = at;
    marks->lastLow.y = marks->lastLow.y - depth;
}

void DrawSkidMarks(SkidMarks* marks)
{
    RenderView* view = g_RenderView;
    Matrix4x4 toScreen = view->toScreen;
    Matrix4x4 toClip = view->toClip;
    f32& depth = toScreen.m[3][2];
    depth = depth + depth * DrawnDepthBack;
    for (s32 index = 0; index < MarkCount; index++)
    {
        if (marks->marks[index] != nullptr)
        {
            Platform::Graphics::DrawScreenModel(marks->marks[index], &toScreen, &toClip, &view->clip);
        }
    }
}

void DropOntoTriangle(const Vector4* point, const CollisionHit* triangle, Vector4* out)
{
    Vector4 plane;
    PlaneThroughTriangle(&plane, &triangle->vertices[0], &triangle->vertices[1], &triangle->vertices[2]);
    out->x = point->x;
    out->z = point->z;
    out->y = -(plane.w + plane.x * point->x + plane.z * point->z) / plane.y;
}

// The segment cut by the vertical planes through the triangle's edges (their normals the edges taken backwards crossed with up:
// the triangle is on their negative sides when it's wound with its normal down, as the game's floors mostly are). A plane with
// both ends on its positive side (past -0.001) leaves none of it
u32 ClipToTriangle(const CollisionHit* triangle, const Vector4* from, const Vector4* to, Vector4* clippedFrom,
                   Vector4* clippedTo)
{
    constexpr s32 Edges = 3;
    constexpr f32 SideEpsilon = Rounded(0.001);
    Vector4 sides[Edges];
    *clippedFrom = *from;
    *clippedTo = *to;
    for (s32 edge = 0; edge < Edges; edge++)
    {
        const Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
        if (PlaneThroughEdge(&sides[edge], &triangle->vertices[(edge + 1) % Edges], &triangle->vertices[edge], &up) == 0)
        {
            return 0;
        }
    }

    for (s32 edge = 0; edge < Edges; edge++)
    {
        const Vector4* side = &sides[edge];
        f32 fromSide = side->x * clippedFrom->x + side->y * clippedFrom->y + side->z * clippedFrom->z + side->w;
        f32 toSide = side->x * clippedTo->x + side->y * clippedTo->y + side->z * clippedTo->z + side->w;
        if (fromSide < SideEpsilon && toSide < SideEpsilon)
        {
            continue;
        }

        if (-SideEpsilon < fromSide && -SideEpsilon <= toSide)
        {
            return 0;
        }

        // Where the whole segment crosses the plane
        Vector4 along = {to->x - from->x, to->y - from->y, to->z - from->z, 1.0f};
        f32 share = -(side->w + (from->x * side->x + from->y * side->y + from->z * side->z))
                    / (along.x * side->x + along.y * side->y + along.z * side->z);
        f32 rest = 1.0f - share;
        Vector4 crossing = {from->x * rest + to->x * share, from->y * rest + to->y * share, from->z * rest + to->z * share, 1.0f};
        if (0.0f <= fromSide)
        {
            *clippedFrom = crossing;
        }
        else
        {
            *clippedTo = crossing;
        }
    }

    return 1;
}
