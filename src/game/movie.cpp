#include "game/movie.h"

#include "game/colour.h"
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
// A 4:3 picture's width on a 16:9 TV, and a 16:9 picture's height on a 4:3 TV as a share of the width
constexpr f32 NarrowPictureWidth = 0.75f;
constexpr f32 WidePictureHeight = 0.5625f;
}

GameMovieController* GameMovieController::Construct(GameMovieController* controller)
{
    controller->path.string = nullptr;
    controller->vtable = g_MovieControllerVTable;
    controller->path.capacity = 0;
    controller->path.length = 0;
    RetailLibc::MemorySet(&controller->flags, 0, sizeof(controller->flags));
    controller->flags.state = StateIdle;
    controller->flags.request = NothingRequested;
    return controller;
}

void GameMovieController::Destroy(u32 destroyFlags)
{
    vtable = g_MovieControllerVTable;
    StringDestroy(&path);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 GameMovieController::Play(const char* file, u32 audioChannel, u32 widescreen, s32 movieWidth, s32 movieHeight)
{
    if (flags.state != StateIdle)
    {
        return 0;
    }

    StringAssign(&path, file);
    flags.widescreen = widescreen;
    flags.audioChannel = audioChannel;
    width = movieWidth;
    height = movieHeight;
    flags.request = PlayRequested;
    return 1;
}

u32 GameMovieController::RequestStop()
{
    if (flags.state != StatePlaying)
    {
        return 0;
    }

    flags.request = StopRequested;
    return 1;
}

u32 GameMovieController::Update()
{
    if (flags.request != NothingRequested)
    {
        flags.state = flags.request;
        flags.request = NothingRequested;
    }

    switch (flags.state)
    {
    case StateStarting:
        // The sound pauses, and the readers finish what they're reading and hold
        PauseSound();
        g_ReadersMode = ReadersHeldByMovie;
        LoadQueuedSectionsIntoMemory_();
        if (Start() != 0)
        {
            flags.state = StatePlaying;
            return 1;
        }

        flags.state = StateIdle;
        g_ReadersMode = ReadersAfterMovie;
        g_ReadersSteps = 0;
        return 0;
    case StatePlaying:
        if (Step() != 0)
        {
            return 1;
        }

        flags.state = StateIdle;
        Stop();
        g_ReadersMode = ReadersAfterMovie;
        g_ReadersSteps = 0;
        return 0;
    case StateStopping:
        // The state stays until the next frame takes the idle state on
        flags.request = IdleRequested;
        Stop();
        g_ReadersMode = ReadersAfterMovie;
        g_ReadersSteps = 0;
        return 0;
    default:
        return 0;
    }
}

void GameMovieController::BaseLendSound()
{
    flags.soundLent = 1;
}

void GameMovieController::BaseReclaimSound()
{
    flags.soundLent = 0;
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

    bool opened = Platform::Movie::Open(PlatformPlayer(), file.string != nullptr ? file.string : "", flags.audioChannel, width,
                                        lent);
    StringDestroy(&file);
    if (!opened)
    {
        return 0;
    }

    Platform::Movie::StartSound(PlatformPlayer(), GroupVolumeLevel(MovieGroup));
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
    GetColor(&color, ColourBlack);
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
    bool widescreen = flags.widescreen != 0;
    if (g_WidescreenTv != 0)
    {
        if (!widescreen)
        {
            area.x = static_cast<s32>(widthLevel * 0.5f - widthLevel * (NarrowPictureWidth * 0.5f));
            area.width = static_cast<s32>(widthLevel * NarrowPictureWidth);
        }
    }
    else if (widescreen)
    {
        area.y = static_cast<s32>(static_cast<f32>(screenHeight) * 0.5f - widthLevel * (WidePictureHeight * 0.5f));
        area.height = static_cast<s32>(widthLevel * WidePictureHeight);
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
