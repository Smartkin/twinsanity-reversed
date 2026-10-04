#include "game/scenery.h"

#include "game/controllers.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/view.h"
#include "platform/graphics.h"

namespace
{
constexpr u32 OutsideBits = 0x3F;

// A box test's flags: out of view when every corner is out on a side (the unscaled bits they share), partly when any scaled
// corner is out
u32 BoxVisibility(const u32* flags)
{
    if ((static_cast<s32>(flags[1]) >> 6 & OutsideBits) != 0)
    {
        return 0;
    }

    return (flags[0] & OutsideBits) != 0 ? 2 : 1;
}
}

ChunkView* ChunkView::Construct(ChunkView* view)
{
    view->vtable = g_ChunkViewVTable;
    view->UseRendererView();
    view->Reset();
    return view;
}

ChunkView* ChunkView::ConstructFor(ChunkView* view, RenderView* renderView)
{
    view->view = renderView;
    view->vtable = g_ChunkViewVTable;
    view->Reset();
    return view;
}

void ChunkView::Destroy(u32 flags)
{
    vtable = g_ChunkViewVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ChunkView::UseRendererView()
{
    view = G_Renderer_->view;
}

void ChunkView::Reset()
{
    if (view != nullptr)
    {
        clip = view->clip;
    }
    else
    {
        clip.x = Rounded(0.2);
        clip.w = 1.0f;
        clip.y = Rounded(0.2);
        clip.z = 1.0f;
    }

    TakeViewMatrix();
    visibility = 0;
    lastVisibility = 0;
    distance = 0;
    origin = g_DefaultBox.min;
}

void ChunkView::TakeViewMatrix()
{
    toClip = view->toClip;
}

void ChunkView::TestBox(const Box* box)
{
    alignas(16) u32 flags[2];
    Platform::Graphics::ClipBox(&toClip, box, flags);
    u32 result = BoxVisibility(flags);
    lastVisibility = result;
    visibility = result;
}

void ChunkView::TestBoxAt(const Box* box, const Matrix4x4* matrix)
{
    alignas(16) u32 flags[2];
    Platform::Graphics::ClipBoxAt(&toClip, box, flags, matrix);
    u32 result = BoxVisibility(flags);
    lastVisibility = result;
    visibility = result;
}

// Through the view's matrix: in view when no coordinate is past w either way
void ChunkView::TestPoint(const Vector4* point)
{
    Vector4 clipped;
    VuTransformPoint(&toClip, point, &clipped);
    u32 outside = clipped.w < clipped.x;
    f32 negative = -clipped.w;
    if (clipped.x < negative)
    {
        outside |= 0x2;
    }

    if (clipped.w < clipped.y)
    {
        outside |= 0x4;
    }

    if (clipped.y < negative)
    {
        outside |= 0x8;
    }

    if (clipped.w < clipped.z)
    {
        outside |= 0x10;
    }

    if (clipped.z < negative)
    {
        outside |= 0x20;
    }

    if (outside != 0)
    {
        lastVisibility = 0;
        visibility = 0;
        return;
    }

    lastVisibility = 1;
    visibility = 1;
}

const Matrix4x4* ChunkView::ChunkMatrix(const Matrix4x4* matrix)
{
    return matrix;
}

u32 ChunkView::IsLinked()
{
    return 0;
}

u32 ChunkView::Unknown7()
{
    return 1;
}

u32 ChunkView::Unknown8()
{
    return 0;
}

u32 ChunkView::Faces(const Vector4* direction)
{
    return 0.0f < g_CullDirection.x * direction->x + g_CullDirection.y * direction->y + g_CullDirection.z * direction->z;
}

// Into VU0: the view's matrix, the renderer view's matrix to the screen and the camera
void ChunkView::Load()
{
    Platform::Graphics::LoadCullingView(&toClip, &view->toScreen, &camera);
}

void ChunkView::LoadModel(const Matrix4x4* model)
{
    Platform::Graphics::CullLoadModel(model);
}

void ChunkView::TestCell(const SceneryCell* cell)
{
    Platform::Graphics::CullTestBox(&cell->min);
}

u32 ChunkView::CellResult()
{
    Platform::Graphics::CullOutcome outcome = Platform::Graphics::CullResult();
    if (outcome.outside != 0)
    {
        lastVisibility = 0;
        return lastVisibility;
    }

    lastVisibility = outcome.clipped != 0 ? 2 : 1;
    return lastVisibility;
}

// Partly out, the matrix to the clip space is kept as well
u32 ChunkView::MeshResult()
{
    Platform::Graphics::CullOutcome outcome = Platform::Graphics::CullResult();
    distance = outcome.distance;
    if (outcome.outside != 0)
    {
        visibility = 0;
        lastVisibility = 0;
        return visibility;
    }

    if (outcome.clipped != 0)
    {
        lastVisibility = 2;
        visibility = 2;
        Platform::Graphics::CullMatrices(&clipped, &toScreen);
    }
    else
    {
        lastVisibility = 1;
        visibility = 1;
        Platform::Graphics::CullMatrices(nullptr, &toScreen);
    }

    return visibility;
}

u32 ChunkView::InstanceResult()
{
    Platform::Graphics::CullOutcome outcome = Platform::Graphics::CullResult();
    distance = outcome.distance;
    if (outcome.outside != 0)
    {
        visibility = 0;
        lastVisibility = 0;
        return visibility;
    }

    u32 result = outcome.clipped != 0 ? 2 : 1;
    lastVisibility = result;
    visibility = result;
    return visibility;
}

void ChunkView::DrawnModelResult()
{
    distance = Platform::Graphics::CullResult().distance;
    Platform::Graphics::CullMatrices(nullptr, &toScreen);
}

void ChunkView::TestBoxOnVu0(const Box* box, const Matrix4x4* matrix)
{
    Platform::Graphics::CullTestBoxAt(box, matrix);
}

LinkedChunkView* LinkedChunkView::Construct(LinkedChunkView* view, const Matrix4x4* chunkMatrix, const Matrix4x4* objectMatrix)
{
    view->vtable = g_ChunkViewVTable;
    view->view = g_RenderView;
    view->Reset();
    view->vtable = g_LinkedChunkViewVTable;
    view->link = *chunkMatrix;
    Platform::Graphics::CullLoadLink(chunkMatrix, objectMatrix);
    return view;
}

void LinkedChunkView::Destroy(u32 flags)
{
    vtable = g_ChunkViewVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void LinkedChunkView::TestBox(const Box* box)
{
    ChunkView::TestBoxAt(box, &link);
}

void LinkedChunkView::TestBoxAt(const Box* box, const Matrix4x4* matrix)
{
    Matrix4x4 through;
    VuMultiplyMatrices(matrix, &link, &through);
    ChunkView::TestBoxAt(box, &through);
}

void LinkedChunkView::TestPoint(const Vector4* point)
{
    Vector4 through;
    VuTransformPoint(&link, point, &through);
    ChunkView::TestPoint(&through);
}

Matrix4x4* LinkedChunkView::ChunkMatrix(const Matrix4x4* matrix)
{
    VuMultiplyMatrices(matrix, &link, &toScreen);
    return &toScreen;
}

u32 LinkedChunkView::IsLinked()
{
    return 1;
}

u32 LinkedChunkView::Faces(const Vector4* direction)
{
    constexpr f32 Epsilon = 0x1.5798ECp-29f;
    Vector4 turned;
    VuRotateVector(&link, direction, &turned);
    f32 inverse = InverseLength(&turned, Epsilon);
    turned.x = turned.x * inverse;
    turned.y = turned.y * inverse;
    turned.z = turned.z * inverse;
    return 0.0f < g_CullDirection.x * turned.x + g_CullDirection.y * turned.y + g_CullDirection.z * turned.z;
}
