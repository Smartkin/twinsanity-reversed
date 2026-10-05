#include "game/scenery.h"

#include "game/controllers.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/view.h"
#include "platform/graphics.h"

namespace
{
// Without a view, the clip vector's x and y: 1 over a guard band of 5 screens
constexpr f32 DefaultClip = Rounded(0.2);

// A point's clip flags (VU0's CLIP's six bits: past w on either side of each axis)
union ClipFlags
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
CHECK_SIZE(ClipFlags, 4);

// A box test's flags (its corners' clip flags of its two tests together: those of any corner, then those every corner has): the
// corners scaled by the row after the view's matrix (the last test) and as they are
union BoxClipFlags
{
    u32 value;
    struct
    {
        u32 scaled : 6;
        u32 unscaled : 6;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(BoxClipFlags, 4);

// Out of view when every corner is out on a side (the unscaled bits they share), partly when any scaled corner is out
u32 BoxVisibility(const u32* flags)
{
    BoxClipFlags any;
    any.value = flags[0];
    BoxClipFlags every;
    every.value = flags[1];
    if (every.unscaled != 0)
    {
        return ChunkView::OutOfView;
    }

    return any.scaled != 0 ? ChunkView::PartlyInView : ChunkView::InView;
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

void ChunkView::Destroy(u32 destroyFlags)
{
    vtable = g_ChunkViewVTable;
    if ((destroyFlags & 1) != 0)
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
        clip.x = DefaultClip;
        clip.w = 1.0f;
        clip.y = DefaultClip;
        clip.z = 1.0f;
    }

    TakeViewMatrix();
    visibility = OutOfView;
    lastVisibility = OutOfView;
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
    ClipFlags outside = {};
    outside.pastPositiveX = clipped.w < clipped.x;
    f32 negative = -clipped.w;
    outside.pastNegativeX = clipped.x < negative;
    outside.pastPositiveY = clipped.w < clipped.y;
    outside.pastNegativeY = clipped.y < negative;
    outside.pastPositiveZ = clipped.w < clipped.z;
    outside.pastNegativeZ = clipped.z < negative;
    if (outside.value != 0)
    {
        lastVisibility = OutOfView;
        visibility = OutOfView;
        return;
    }

    lastVisibility = InView;
    visibility = InView;
}

const Matrix4x4* ChunkView::ChunkMatrix(const Matrix4x4* matrix)
{
    return matrix;
}

u32 ChunkView::IsLinked()
{
    return 0;
}

u32 ChunkView::Unused7()
{
    return 1;
}

u32 ChunkView::Unused8()
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
        lastVisibility = OutOfView;
        return lastVisibility;
    }

    lastVisibility = outcome.clipped != 0 ? PartlyInView : InView;
    return lastVisibility;
}

// Partly out, the matrix to the clip space is kept as well
u32 ChunkView::MeshResult()
{
    Platform::Graphics::CullOutcome outcome = Platform::Graphics::CullResult();
    distance = outcome.distance;
    if (outcome.outside != 0)
    {
        visibility = OutOfView;
        lastVisibility = OutOfView;
        return visibility;
    }

    if (outcome.clipped != 0)
    {
        lastVisibility = PartlyInView;
        visibility = PartlyInView;
        Platform::Graphics::CullMatrices(&clipped, &toScreen);
    }
    else
    {
        lastVisibility = InView;
        visibility = InView;
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
        visibility = OutOfView;
        lastVisibility = OutOfView;
        return visibility;
    }

    u32 result = outcome.clipped != 0 ? PartlyInView : InView;
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

void LinkedChunkView::Destroy(u32 destroyFlags)
{
    vtable = g_ChunkViewVTable;
    if ((destroyFlags & 1) != 0)
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
    Vector4 turned;
    VuRotateVector(&link, direction, &turned);
    f32 inverse = InverseLength(&turned, LengthEpsilon);
    turned.x = turned.x * inverse;
    turned.y = turned.y * inverse;
    turned.z = turned.z * inverse;
    return 0.0f < g_CullDirection.x * turned.x + g_CullDirection.y * turned.y + g_CullDirection.z * turned.z;
}
