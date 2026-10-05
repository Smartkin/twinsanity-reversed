#include "game/view.h"

#include "game/camerarig.h"
#include "game/controllers.h"
#include "game/instances.h"
#include "game/place.h"
#include "game/reference.h"

EABI_EXPORT(FUN_0026f0c8, &RenderView::Update);

RenderView* RenderView::Construct(RenderView* view)
{
    view->inverseAspect = 1.0f;
    view->cameraObject = nullptr;
    InitIdentityMatrix(&view->projection);
    view->clip.x = 1.0f / g_GuardBandX;
    view->clip.w = 1.0f;
    view->clip.y = 1.0f / g_GuardBandY;
    view->clip.z = 1.0f;
    InitIdentityMatrix(&view->toClip);
    return view;
}

void RenderView::SetProjection(CameraLensNode* lens)
{
    f32 farPlane = lens->farPlane;
    f32 nearPlane = lens->nearPlane;
    projection = {};
    s32 half = static_cast<s32>(static_cast<f32>(lens->fov) * 0.5f);
    f32 cotangent = 1.0f / TanOfAngle(&half);
    f32 depth = farPlane - nearPlane;
    projection.m[2][3] = 1.0f;
    projection.m[1][1] = cotangent;
    projection.m[0][0] = inverseAspect * cotangent;
    projection.m[2][2] = (farPlane + nearPlane) / depth;
    projection.m[3][2] = -((farPlane + farPlane) * nearPlane) / depth;
    lens->bits.projectionChanged = 0;
}

void RenderView::Update(f32 aspect)
{
    constexpr f32 SameAspect = LengthEpsilon;
    static constexpr Vector4 Forward = {0.0f, 0.0f, 1.0f, 0.0f};
    ReferencedObject* object = cameraObject != nullptr ? cameraObject->object : nullptr;
    if (object == nullptr)
    {
        return;
    }

    auto* camera = static_cast<InstanceContext*>(object);
    ObjectPlace* place = camera->place;
    RotateAndTranslate(place);
    auto* lens = static_cast<CameraLensNode*>(GetGameNode(&camera->nodes, NodeCameraLens));
    f32 scaled = lens->pixelAspect * aspect;
    f32 difference = scaled - inverseAspect;
    bool same = difference * difference <= SameAspect;
    InvertMatrix(&place->matrix, 4, &toCamera);
    VuRotateVector(&place->matrix, &Forward, &forward);
    if (lens->bits.projectionChanged != 0 || same)
    {
        inverseAspect = 1.0f / scaled;
        SetProjection(lens);
    }
}

void RenderView::MakeMatrices(const RenderTargetDescription* target)
{
    VuMultiplyMatrices(&toCamera, &projection, &toClip);
    VuMultiplyMatrices(&projection, &target->clipToScreen, &cameraToScreen);
    VuMultiplyMatrices(&toClip, &target->clipToScreen, &toScreen);
}

InstanceContext* CameraInstance()
{
    RenderView* view = G_Renderer_->view;
    if (view == nullptr || view->cameraObject == nullptr)
    {
        return nullptr;
    }

    return static_cast<InstanceContext*>(view->cameraObject->object);
}
