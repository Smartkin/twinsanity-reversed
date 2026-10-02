#include "game/movie.h"

#include "game/controllers.h"
#include "game/memory.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/sound.h"
#include "platform/graphics.h"
#include "retail/libc.h"

extern "C"
{
    extern const GccVTableEntry g_MovieControllerVTable[] RETAIL(D_00306CE8);
    extern const GccVTableEntry g_MoviePlayerVTable[] RETAIL(GameMovieController_Methods);
}

namespace
{
// The color table's clear color, which the screen is cleared to after a movie
constexpr s32 ClearColorIndex = 8;
// The volume group the movies' sound plays at
constexpr s32 MovieVolumeGroup = 3;
}

GameMovieController* GameMovieController::Construct(GameMovieController* controller)
{
    controller->path.string = nullptr;
    controller->vtable = g_MovieControllerVTable;
    controller->path.capacity = 0;
    controller->path.length = 0;
    RetailLibc::MemorySet(&controller->flags, 0, sizeof(controller->flags));
    controller->flags = (controller->flags & ~(StateMask | RequestMask)) | NothingRequested;
    return controller;
}

void GameMovieController::Destroy(u32 destroyFlags)
{
    vtable = g_MovieControllerVTable;
    StringDestroy(&path);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 GameMovieController::Play(const char* file, u32 audioChannel, u32 widescreen, s32 movieWidth, s32 movieHeight)
{
    if ((flags & StateMask) != 0)
    {
        return 0;
    }

    StringAssign(&path, file);
    u32 bits = (flags & ~Widescreen) | (widescreen & 1) << 6;
    bits = (bits & ~AudioChannelMask) | (audioChannel & AudioChannelMask);
    width = movieWidth;
    bits &= ~RequestMask;
    height = movieHeight;
    flags = bits | PlayRequested;
    return 1;
}

u32 GameMovieController::RequestStop()
{
    u32 bits = flags;
    if ((bits & StateMask) != StatePlaying)
    {
        return 0;
    }

    flags = (bits & ~RequestMask) | StopRequested;
    return 1;
}

u32 GameMovieController::Update()
{
    u32 bits = flags;
    if ((bits & RequestMask) != NothingRequested)
    {
        // The request becomes the state
        flags = (((bits & ~StateMask) | (bits << 4 & StateMask)) & ~RequestMask) | NothingRequested;
        bits = flags;
    }

    switch (bits >> 12 & 0xF)
    {
    case 1:
        // The sound pauses and the readers take their movie mode (game/readers.h)
        PauseSound();
        g_ReadersMode = 1;
        LoadQueuedSectionsIntoMemory_();
        if (Start() != 0)
        {
            flags = (flags & ~StateMask) | StatePlaying;
            return 1;
        }

        flags &= ~StateMask;
        g_ReadersMode = 2;
        g_ReadersSteps = 0;
        return 0;
    case 2:
        if (Step() != 0)
        {
            return 1;
        }

        flags &= ~StateMask;
        Stop();
        g_ReadersMode = 2;
        g_ReadersSteps = 0;
        return 0;
    case 3:
        // The state stays until the next frame takes on the request it clears
        flags = bits & ~RequestMask;
        Stop();
        g_ReadersMode = 2;
        g_ReadersSteps = 0;
        return 0;
    default:
        return 0;
    }
}

void GameMovieController::BaseLendSound()
{
    flags |= SoundLent;
}

void GameMovieController::BaseReclaimSound()
{
    flags &= ~SoundLent;
}

MoviePlayer* MoviePlayer::Construct(MoviePlayer* player)
{
    GameMovieController::Construct(player);
    player->extension.string = nullptr;
    player->vtable = g_MoviePlayerVTable;
    player->extension.capacity = 0;
    player->extension.length = 0;
    Platform::Movie::Construct(player->PlatformPlayer());
    StringAssign(&player->extension, Platform::Movie::Extension);
    return player;
}

void MoviePlayer::Destroy(u32 destroyFlags)
{
    StringDestroy(&extension);
    GameMovieController::Destroy(destroyFlags);
}

u32 MoviePlayer::Start()
{
    Platform::Movie::BeginPresenting(PlatformPlayer(), PresentMovieFrame);
    GameMovieController::LendSound();
    // The movie has the screen, and the renderer's DMA memory
    Platform::Graphics::WaitIdle();
    Platform::Graphics::ResetBuckets(true);
    // The music doesn't play during a movie: a music stream's buffer is the player's to stream through
    void* lent = LendMusicBuffer();
    String file;
    StringConstruct(&file, Platform::Movie::FilePrefix);
    if (path.length != 0)
    {
        StringAppend(&file, path.string);
    }

    StringAppend(&file, ".");
    if (extension.length != 0)
    {
        StringAppend(&file, extension.string);
    }

    StringAppend(&file, Platform::Movie::FileSuffix);
    if (Platform::Movie::CapitalFileNames)
    {
        for (s32 i = 0; i < file.length; i++)
        {
            file.string[i] = static_cast<char>(RetailLibc::ToUpper(file.string[i]));
        }
    }

    bool opened = Platform::Movie::Open(PlatformPlayer(), file.string != nullptr ? file.string : "", flags & AudioChannelMask,
                                        width, lent);
    StringDestroy(&file);
    if (!opened)
    {
        return 0;
    }

    Platform::Movie::StartSound(PlatformPlayer(), GroupVolumeLevel(MovieVolumeGroup));
    return 1;
}

u32 MoviePlayer::Stop()
{
    RenderTargetDescription* target = G_Renderer_->target;
    GameRendererController* renderer = G_GameRendererController;
    u32 clearColor = target->clearColor;
    Platform::Movie::Close(PlatformPlayer());
    ReclaimMusicBuffers();
    // The renderer has the screen again: two frames cleared to the clear color, then its DMA starts over
    u32 color;
    GetColor(&color, ClearColorIndex);
    target->clearColor = color;
    SetUpFrame(target, true, true);
    renderer->FinishScene(0);
    renderer->Present(true);
    SetUpFrame(target, true, true);
    renderer->FinishScene(0);
    renderer->Render();
    Platform::Graphics::WaitIdle();
    Platform::Graphics::ResetBuckets(false);
    renderer->FinishScene(0);
    renderer->Render();
    GameMovieController::ReclaimSound();
    target->clearColor = clearColor;
    return 1;
}

u32 MoviePlayer::Step()
{
    return Platform::Movie::Step(PlatformPlayer()) ? 1 : 0;
}

// The picture fills the screen but for its shape: a 4:3 movie on a 16:9 TV gets three quarters of the width in the middle, a 16:9
// movie on a 4:3 TV a height of 9/16 of the width in the middle
void MoviePlayer::Draw()
{
    u32 screenWidth = g_DisplayWidth;
    u32 screenHeight = g_DisplayHeight;
    f32 widthLevel = static_cast<f32>(screenWidth);
    Platform::Movie::Area area = {0, 0, static_cast<s32>(screenWidth), static_cast<s32>(screenHeight)};
    bool widescreen = (flags & Widescreen) != 0;
    if (g_WidescreenTv != 0)
    {
        if (!widescreen)
        {
            area.x = static_cast<s32>(widthLevel * 0.5f - widthLevel * 0.375f);
            area.width = static_cast<s32>(widthLevel * 0.75f);
        }
    }
    else if (widescreen)
    {
        area.y = static_cast<s32>(static_cast<f32>(screenHeight) * 0.5f - widthLevel * 0.28125f);
        area.height = static_cast<s32>(widthLevel * 0.5625f);
    }

    // A picture higher than the screen loses as many rows at its top as at its bottom
    u32 skippedRows = screenHeight < static_cast<u32>(height) ? (height - screenHeight) >> 1 : 0;
    Platform::Movie::Draw(PlatformPlayer(), area, width, height, skippedRows);
}

void MoviePlayer::WaitFrame()
{
    Platform::Movie::WaitFrame(PlatformPlayer());
}

void MoviePlayer::QueuePicture()
{
    Platform::Movie::QueuePicture(PlatformPlayer());
}
