#include "game/renderer.h"

#include "game/controllers.h"
#include "game/math.h"
#include "game/movie.h"
#include "platform/graphics.h"

namespace
{
// The widths the game's places were made for (4:3) and a 16:9 TV's, as the game has them; a width over 4:3's
constexpr f32 NarrowAspect = 0x1.555556p+0f;
constexpr f32 WideAspect = 0x1.C71C72p+0f;
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
    frame.red = static_cast<u8>(target->clearColor);
    frame.green = static_cast<u8>(target->clearColor >> 8);
    frame.blue = static_cast<u8>(target->clearColor >> 16);
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
