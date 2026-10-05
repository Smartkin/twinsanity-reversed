#include "game/renderer.h"

#include "game/camerarig.h"
#include "game/chunkdata.h"
#include "game/colour.h"
#include "game/controllers.h"
#include "game/font.h"
#include "game/instances.h"
#include "game/lights.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/overlay.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/scenery.h"
#include "game/view.h"
#include "platform/graphics.h"

namespace
{
// A width over 4:3's
constexpr f32 InverseNarrowAspect = 0.75f;

// What the TV's shape squeezes horizontal places and sizes by
f32 ScreenSqueeze()
{
    f32 aspect = g_WidescreenTv != 0 ? WideAspect : NarrowAspect;
    return aspect * InverseNarrowAspect;
}

void ToPixels(Vector2* value)
{
    value->x = value->x * static_cast<f32>(g_RendererWidth);
    value->y = value->y * static_cast<f32>(g_RendererHeight);
}

void StartNextFrame(bool fromInterrupt)
{
    Platform::Graphics::WaitSent();
    Platform::Graphics::SubmitBuckets(fromInterrupt);
}

void FinishFrame()
{
    Platform::Graphics::FinishFrame();
    g_RenderBuffer = g_RenderBuffer == 0;
}

constexpr u32 RendererStartFlags = RendererFlags::Draws | RendererFlags::ClearsColour | RendererFlags::ClearsDepth;
// The size of a render target's packet (the platform's)
constexpr u32 TargetPacketSize = 0x60;

// The renderers in use, in their slots' order (the walk the retail code makes on the stack)
template <typename Visit>
void ForEachRenderer(GameRendererController* controller, Visit visit)
{
    RendererWalk walk;
    walk.vtable = g_RendererWalkVTable;
    walk.index = 0;
    walk.passed = 0;
    walk.pool = &controller->renderers;
    for (walk.First(); walk.IsDone() == 0; walk.Next())
    {
        visit(*walk.Current());
    }
}
}

extern "C"
{
    extern const GccVTableEntry g_RendererControllerVTable[] RETAIL(D_002F68D0);
    extern const GccVTableEntry g_GameRendererControllerVTable[] RETAIL(GameRendererController_Methods);
    // The drawing renderers with a camera the controller's slot 7 counted, which nothing reads
    extern u32 g_CameraRenderers RETAIL(D_0030AB04);
}

GameRendererController* GameRendererController::Construct(void* memory, s32 width, s32 height, bool pal)
{
    GameRendererController* controller = ConstructBase(static_cast<GameRendererController*>(memory));
    controller->vtable = g_GameRendererControllerVTable;
    g_RendererWidth = static_cast<s16>(width);
    g_RendererHeight = static_cast<s16>(height);
    Platform::Graphics::StartRenderer(height, pal);
    controller->MoveScreen(pal ? &g_PalScreenOffset : &g_NtscScreenOffset);
    Platform::Graphics::FinishRendererStart();
    return controller;
}

GameRendererController* GameRendererController::ConstructBase(GameRendererController* controller)
{
    controller->vtable = g_RendererControllerVTable;
    RendererPoolConstruct(&controller->renderers);
    return controller;
}

void GameRendererController::BaseDestroy(u32 destroyFlags)
{
    vtable = g_RendererControllerVTable;
    ForEachRenderer(this, [](Renderer* renderer)
    {
        if (renderer == nullptr)
        {
            return;
        }

        if (renderer->target != nullptr)
        {
            renderer->target->Destroy(DestroyAndFree);
        }

        TextQueueDestroy(&renderer->texts, DestroyOnly);
        MemoryDeallocate2_(renderer);
    });
    RendererPoolDestroy(&renderers, DestroyOnly);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GameRendererController::Destroy(u32 destroyFlags)
{
    vtable = g_GameRendererControllerVTable;
    BaseDestroy(destroyFlags);
}

u32 GameRendererController::None2()
{
    return 0;
}

void GameRendererController::ClearFrames()
{
    CallVirtual<void>(this, vtable, BeforeDrawingSlot);
    ForEachRenderer(this, [](Renderer* renderer)
    {
        if (renderer->flags.draws != 0)
        {
            renderer->flags.clearsColour = 1;
            renderer->flags.clearsDepth = 1;
        }
    });
    CallVirtual<void>(this, vtable, AfterDrawingSlot);
}

void GameRendererController::StepAnimations(TimeClock* clock)
{
    Platform::Graphics::StepAnimations(clock);
}

void GameRendererController::CountCameraRenderers()
{
    ForEachRenderer(this, [](Renderer* renderer)
    {
        if (renderer->flags.draws == 0 || renderer->view == nullptr)
        {
            return;
        }

        Reference* camera = renderer->view->cameraObject;
        if (camera != nullptr && camera->object != nullptr)
        {
            g_CameraRenderers++;
        }
    });
}

void GameRendererController::DrawScenes()
{
    ForEachRenderer(this, [](Renderer* renderer)
    {
        DrawRendererScene(renderer);
    });
}

void GameRendererController::DrawOverlays()
{
    ForEachRenderer(this, [](Renderer* renderer)
    {
        if (renderer->flags.draws != 0)
        {
            DrawOverlay(renderer);
        }
    });
}

void GameRendererController::FinishSceneWithEffects()
{
    CallVirtual<void>(this, vtable, FinishSceneSlot, 1u);
}

// Retail reads the renderer at address 0 when no slot but the last has one (RendererPoolFirst gives none then)
void GameRendererController::PresentOverlay()
{
    CallVirtual<void>(this, vtable, BeforeDrawingSlot);
    DrawOverlay(*RendererPoolFirst(&renderers));
    CallVirtual<void>(this, vtable, RenderSlot);
    CallVirtual<void>(this, vtable, AfterDrawingSlot);
}

void GameRendererController::BaseSetScreenOffset(const Vector2* offset)
{
    f32 x = offset->x;
    if (x < -1.0f)
    {
        screenOffset.x = -1.0f;
    }
    else if (1.0f < x)
    {
        screenOffset.x = 1.0f;
    }
    else
    {
        screenOffset.x = x;
    }

    f32 y = offset->y;
    if (y < -1.0f)
    {
        screenOffset.y = -1.0f;
    }
    else if (1.0f < y)
    {
        screenOffset.y = 1.0f;
    }
    else
    {
        screenOffset.y = y;
    }
}

void GameRendererController::MoveScreen(const Vector2* offset)
{
    BaseSetScreenOffset(offset);
    Platform::Graphics::MoveDisplay(&screenOffset);
}

Renderer* GameRendererController::MakeRenderer(RenderTargetDescription* description, u32)
{
    Renderer* renderer = Renderer::Construct(static_cast<Renderer*>(MemoryAllocate(sizeof(Renderer))), this, description);
    RendererPoolAdd(&renderers, &renderer);
    return renderer;
}

void GameRendererController::Nothing5()
{
}

void GameRendererController::Nothing6()
{
}

void GameRendererController::Nothing15()
{
}

void GameRendererController::Nothing16()
{
}

void GameRendererController::Nothing17()
{
}

void GameRendererController::Nothing19()
{
}

void GameRendererController::Nothing20()
{
}

void GameRendererController::Nothing21()
{
}

void GameRendererController::BaseNothing10()
{
}

void GameRendererController::BaseNothing14()
{
}

// The walk's functions, which the controller's loops use (the retail ones inline but for Current, called through the vtable)
void RendererWalk::Destroy(u32 destroyFlags)
{
    vtable = g_RendererWalkBaseVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void RendererWalk::BaseDestroy(u32 destroyFlags)
{
    vtable = g_RendererWalkBaseVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

// The first slot in use; the last one is taken without looking (it's the one in use when no other is)
void RendererWalk::First()
{
    index = 0;
    passed = 0;
    while (index < pool->capacity - 1 && pool->links[index] != PoolSlotUsed)
    {
        index++;
    }
}

u32 RendererWalk::IsDone()
{
    return passed == pool->used;
}

Renderer** RendererWalk::Current()
{
    return &pool->items[index];
}

void RendererWalk::Next()
{
    if (!(passed < pool->used - 1))
    {
        passed = pool->used;
        return;
    }

    while (passed < pool->used)
    {
        s16 counted = passed;
        index++;
        if (pool->links[index] == PoolSlotUsed)
        {
            passed = static_cast<s16>(counted + 1);
            return;
        }
    }
}

RendererWalk* RendererWalk::Assign(const RendererWalk* other)
{
    index = other->index;
    passed = other->passed;
    pool = other->pool;
    return this;
}

s32 RendererWalk::Index()
{
    return index;
}

s32 RendererWalk::Count()
{
    return pool->used;
}

Renderer* Renderer::Construct(Renderer* renderer, GameRendererController* controller, const RenderTargetDescription* target)
{
    renderer->controller = controller;
    renderer->view = nullptr;
    renderer->target = nullptr;
    GetColor(&renderer->colour, ColourWhite);
    TextQueueConstruct(&renderer->texts);
    renderer->font = nullptr;
    renderer->textAlignment.value = TextAlignment::TopLeft;
    renderer->textScale.y = 1.0f;
    renderer->textScale.x = 1.0f;
    renderer->flags.value = RendererStartFlags;
    renderer->target = RenderTargetDescription::Copy(
        static_cast<RenderTargetDescription*>(MemoryAllocate(sizeof(RenderTargetDescription))), target);
    for (u32 layer = 0; layer < Renderer::OverlayLayers; layer++)
    {
        renderer->layers[layer] = nullptr;
        renderer->layerEnds[layer] = nullptr;
    }

    return renderer;
}

RenderTargetDescription* RenderTargetDescription::Construct(RenderTargetDescription* description,
                                                            GameRendererController* controller)
{
    description->controller = controller;
    description->offsetX = 0;
    description->offsetY = 0;
    description->width = 0;
    description->height = 0;
    GetColor(&description->clearColor, ColourBlack);
    description->displayWidth = 0;
    description->displayHeight = 0;
    description->packet = static_cast<u8*>(MemoryAllocate2(TargetPacketSize));
    return description;
}

RenderTargetDescription* RenderTargetDescription::Copy(RenderTargetDescription* description, const RenderTargetDescription* other)
{
    description->controller = other->controller;
    description->offsetX = other->offsetX;
    description->offsetY = other->offsetY;
    description->width = other->width;
    description->height = other->height;
    description->clearColor = other->clearColor;
    description->displayWidth = other->displayWidth;
    description->displayHeight = other->displayHeight;
    description->packet = static_cast<u8*>(MemoryAllocate2(TargetPacketSize));
    Platform::Graphics::SetUpRenderTarget(description);
    return description;
}

void RenderTargetDescription::Destroy(u32 flags)
{
    if (packet != nullptr)
    {
        MemoryDeallocate_(packet);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

extern "C" void SetRendererView(Renderer* renderer, RenderView* view)
{
    view->Update(g_WidescreenTv != 0 ? WideAspect : NarrowAspect);
    renderer->view = view;
}

void GameRendererController::PresentFrame()
{
    Platform::Graphics::Present(g_FrameChain);
    StartNextFrame(false);
    FinishFrame();
}

void GameRendererController::PresentFrameUnlessHeld(bool held)
{
    Platform::Graphics::Present(held ? nullptr : g_FrameChain);
    StartNextFrame(false);
    FinishFrame();
}

// No screen effects while a movie plays
void GameRendererController::EndScene(u32 effects)
{
    GameMovieController* movie = G_GameMovieController;
    Platform::Graphics::FinishScene((movie == nullptr || !movie->IsPlaying()) && effects != 0);
}

extern "C" void PresentMovieFrame()
{
    GameMovieController* movie = G_GameMovieController;
    Platform::Graphics::PresentFromInterrupt(g_FrameChain);
    StartNextFrame(true);
    if (movie != nullptr && movie->IsPlaying())
    {
        movie->QueuePicture();
    }

    FinishFrame();
}

extern "C" void SetUpFrame(RenderTargetDescription* target, u32 clearColor, u32 clearDepth)
{
    Platform::Graphics::FrameStart frame;
    frame.x = target->x;
    frame.y = target->y;
    frame.width = target->width;
    frame.height = target->height;
    frame.screenWidth = target->displayWidth;
    frame.screenHeight = target->displayHeight;
    Rgba clear;
    clear.value = target->clearColor;
    frame.red = clear.red;
    frame.green = clear.green;
    frame.blue = clear.blue;
    frame.clearColor = clearColor != 0;
    frame.clearDepth = clearDepth != 0;
    Platform::Graphics::StartFrame(frame);
}

extern "C" void FitPlaceToScreen(u32 inPixels, const Vector2* anchor, Vector2* place)
{
    f32 centre = anchor->x;
    place->x = (place->x - centre) / ScreenSqueeze() + centre;
    if (inPixels != 0)
    {
        ToPixels(place);
    }
}

extern "C" void FitSizeToScreen(u32 inPixels, Vector2* size)
{
    size->x = size->x / ScreenSqueeze();
    if (inPixels != 0)
    {
        ToPixels(size);
    }
}

// The frame's scene: a movie playing draws itself; else the camera's chunk (its frustum from the lens, its sky or a cleared frame,
// its scenery and links, then the shadows cast in them), a cleared frame without one; the overlay last
extern "C" void DrawRendererScene(Renderer* renderer)
{
    GameMovieController* movie = G_GameMovieController;
    if (movie != nullptr && movie->IsPlaying())
    {
        SetUpFrame(renderer->target, 1, 0);
        movie->Draw();
        DrawOverlay(renderer);
        return;
    }

    RenderView* view = renderer->view;
    if (view != nullptr)
    {
        Reference* reference = view->cameraObject;
        auto* camera = reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
        ChunkData* chunk = camera != nullptr ? camera->chunk : nullptr;
        if (chunk != nullptr)
        {
            ObjectPlace* place = camera->place;
            RotateAndTranslate(place);
            auto* lens = static_cast<CameraLensNode*>(GetGameNode(&camera->nodes, NodeCameraLens));
            g_RenderView = renderer->view;
            g_RenderTarget = renderer->target;
            Sky* sky = chunk->sky;
            Matrix4x4* toCamera = &renderer->view->toCamera;
            Matrix4x4 identity;
            InitIdentityMatrix(&identity);
            ClearLightingUnused();
            s32 fov = lens->fov;
            f32 aspect = g_WidescreenTv != 0 ? WideAspect : NarrowAspect;
            Platform::Graphics::SetViewFrustum(lens->nearPlane, lens->farPlane, aspect, &fov);
            renderer->view->MakeMatrices(renderer->target);
            chunk->drawMatrix = identity;
            if (sky != nullptr)
            {
                Platform::Graphics::DrawChunkSky(sky, renderer->view);
            }
            else
            {
                SetUpFrame(renderer->target, renderer->flags.clearsColour, renderer->flags.clearsDepth);
            }

            DrawScene(chunk, place, renderer->view);
            DrawShadows(chunk, toCamera, &identity);
            DrawOverlay(renderer);
            return;
        }
    }

    SetUpFrame(renderer->target, renderer->flags.clearsColour, renderer->flags.clearsDepth);
    DrawOverlay(renderer);
}
