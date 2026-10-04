#include "game/context.h"
#include "game/copyprotection.h"

#include "debug.h"

#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/rigidbody.h"
#include "game/filestream.h"
#include "game/sound.h"
#include "game/disk.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/renderer.h"
#include "platform/graphics.h"
#include "platform/memory.h"
#include "retail/libc.h"

namespace
{
// The seconds a frame must have left for more of each kind of background work. The first round of a frame keeps going while it
// has them, the rounds after do a step each
constexpr f32 ChunkLoadingTime = Rounded(0.0016954);
constexpr f32 DestroyingTime = Rounded(0.0013563);
constexpr f32 ResourcesTime = Rounded(0.0010173);
constexpr f32 CompactionTime = Rounded(0.0006782);
constexpr f32 StepTime = Rounded(0.001);

// The color table's clear color
constexpr s32 ClearColorIndex = 8;
}

extern "C"
{
    // Whether a movie played last frame, and whether the renderer has to start again after one
    extern u8 g_MoviePlayed RETAIL(D_00309AB2);
    extern bool g_RendererRestartDue RETAIL(D_00309AB1);
    extern const GccVTableEntry g_GameContextPrototypeVTable[] RETAIL(D_002F5AF0);

    // The instance contexts' frame, and the deletion of one object waiting for it
    void UpdateInstanceContexts() RETAIL(FUN_00199820);
    u32 DestroyPendingStep() RETAIL(FUN_00199a00);
    // The retail strncmp
    s32 CompareStrings(const char* first, const char* second, u32 count);
    extern const u8 CasingTable[];
}

GameContextPrototype* GameContextPrototype::Construct(GameContextPrototype* context)
{
    context->unknown00 = 0;
    context->unknown04 = 1.0f;
    context->vtable = g_GameContextPrototypeVTable;
    GetColor(&context->clearColor, ClearColorIndex);
    context->unknown0C = nullptr;
    context->unknown14 = nullptr;
    context->unknown10 = 0;
    UnkDebugFunction3();
    return context;
}

void GameContextPrototype::StartUp()
{
    if (g_GameState == GameStateStarting)
    {
        RenderTargetDescription description;
        RenderTargetDescription::Construct(&description, G_GameRendererController);
        s32 width = g_ScreenWidth;
        s32 height = g_ScreenHeight;
        description.displayWidth = width;
        description.displayHeight = height;
        u32 color;
        GetColor(&color, ClearColorIndex);
        description.width = width;
        description.height = height;
        description.clearColor = color;
        description.offsetX = 0;
        description.offsetY = 0;
        G_Renderer_ = G_GameRendererController->CreateRenderer(&description, 1);
        G_Renderer_->flags |= 1;
        G_Renderer_->target->clearColor = clearColor;
        StartGame();
        g_GameState = GameStateRunning;
        description.Destroy(DestroyOnly);
    }

    SimpleCopyrightChecksum();
}

void GameContextPrototype::BeginFrame(bool)
{
    if (g_GameState == GameStatePausing)
    {
        if (G_GamePadController != nullptr)
        {
            G_GamePadController->Pause();
        }

        if (G_VideoController != nullptr)
        {
            G_VideoController->Pause();
        }

        PauseSound();
        if (G_GameClockController != nullptr)
        {
            TimeClocksStop(G_GameClockController->clocks);
        }

        g_GameState = GameStatePaused;
    }
    else if (g_GameState == GameStateResuming)
    {
        if (G_GamePadController != nullptr)
        {
            G_GamePadController->Resume();
        }

        if (G_VideoController != nullptr)
        {
            G_VideoController->Resume();
        }

        ResumeSound();
        if (G_GameClockController != nullptr)
        {
            TimeClocksStart(G_GameClockController->clocks);
        }

        g_GameState = GameStateRunning;
    }

    if (G_GameClockController != nullptr)
    {
        GameTimeTick(G_GameClockController);
    }

    u32 movie = 0;
    if (G_GameMovieController != nullptr)
    {
        movie = G_GameMovieController->Update();
    }

    if (g_MoviePlayed != 0 && movie == 0)
    {
        g_RendererRestartDue = true;
    }

    if (g_MoviePlayed != movie)
    {
        g_MoviePlayed = static_cast<u8>(movie);
    }

    bool playing = movie != 0;

    if (!playing && G_GameRendererController != nullptr)
    {
        G_GameRendererController->BeginFrame();
    }

    if (G_GamePadController != nullptr)
    {
        G_GamePadController->Update(static_cast<f32>(static_cast<s32>(g_GlobalClock.advance)) * g_SecondsPerClockUnit);
    }

    GameBeginFrame(playing);
    if (!playing && g_GameState != GameStatePaused)
    {
        UpdateInstanceContexts();
    }
}

void GameContextPrototype::UpdatePrototype(bool playingMovie)
{
    bool paused = g_GameState == GameStatePaused;
    if (!playingMovie)
    {
        if (G_VideoController != nullptr)
        {
            G_VideoController->Update();
        }

        UpdateChunks(GetChunkList(), paused, G_GameClockController);
        if (!paused)
        {
            StepPhysicsWorld(g_PhysicsWorld);
        }
    }

    if (G_UnkStruct_5C0 != nullptr)
    {
        G_UnkStruct_5C0->Update(&g_GlobalClock);
    }

    GameUpdate(playingMovie);
    if (G_GameRendererController != nullptr)
    {
        G_GameRendererController->Update(G_GameClockController->clocks);
    }
}

void GameContextPrototype::EndFrame(bool playingMovie)
{
    G_GameRendererController->EndFrame();
    GameEndFrame(playingMovie);
    G_GameRendererController->AfterEndFrame();
    if (G_UnkStruct_5C0 != nullptr)
    {
        G_UnkStruct_5C0->Render(G_Renderer_);
    }
}

void GameContextPrototype::Render(bool playingMovie)
{
    if (!playingMovie)
    {
        UpdateSound(g_GameState == GameStatePaused, &g_GlobalClock);
    }

    GameRender(playingMovie);
    if (G_GameRendererController != nullptr)
    {
        G_GameRendererController->AfterGameRender();
    }

    if (!playingMovie)
    {
        BackgroundWork(true);
    }

    GameRendererController* renderer = G_GameRendererController;
    if (renderer != nullptr)
    {
        if (playingMovie)
        {
            G_GameMovieController->WaitFrame();
        }
        else
        {
            // The movie had the screen: the renderer's DMA starts over
            if (g_RendererRestartDue)
            {
                Platform::Graphics::WaitIdle();
                Platform::Graphics::ResetBuckets(false);
                renderer->FinishScene(0);
                renderer->Render();
                g_RendererRestartDue = false;
            }

            G_GameRendererController->Render();
        }
    }

    if (G_GameClockController != nullptr)
    {
        GameTimeFrameRendered(G_GameClockController);
    }

    DebugFrameRendered();
}

void GameContextPrototype::LanguageChangedPrototype(u32)
{
}

extern "C"
{
    bool BackgroundWork(bool budgeted)
    {
        bool worked;
        if (budgeted)
        {
            u32 round = 0;
            do
            {
                worked = false;
                UpdateStreamSystem(g_StreamSystem);
                if (G_ChunkLoadingManager_ != nullptr)
                {
                    u8 done = 0;
                    if (round == 0)
                    {
                        while (GameTimeHasTimeLeft(ChunkLoadingTime, G_GameClockController) &&
                               UpdateChunkLoading(G_ChunkLoadingManager_, false, &done))
                        {
                            worked = true;
                        }
                    }
                    else if (GameTimeHasTimeLeft(StepTime, G_GameClockController))
                    {
                        worked = UpdateChunkLoading(G_ChunkLoadingManager_, false, &done);
                    }

                    worked = worked || done != 0;
                }

                if (round == 0)
                {
                    while (GameTimeHasTimeLeft(DestroyingTime, G_GameClockController) && DestroyPendingStep() != 0)
                    {
                        worked = true;
                    }
                }
                else if (GameTimeHasTimeLeft(StepTime, G_GameClockController) && DestroyPendingStep() != 0)
                {
                    worked = true;
                }

                if (G_GameResourcesObjectPointer != nullptr)
                {
                    if (round == 0)
                    {
                        while (GameTimeHasTimeLeft(ResourcesTime, G_GameClockController) &&
                               ResourcesStep(G_GameResourcesObjectPointer) != 0)
                        {
                            worked = true;
                        }
                    }
                    else if (GameTimeHasTimeLeft(StepTime, G_GameClockController) &&
                             ResourcesStep(G_GameResourcesObjectPointer) != 0)
                    {
                        worked = true;
                    }
                }

                round++;
            } while (worked && GameTimeHasTimeLeft(StepTime, G_GameClockController));

            GetDiskManager();
            DiskNothing();
            while (GameTimeHasTimeLeft(CompactionTime, G_GameClockController) && DiskCompactStep(GetDiskManager(), false))
            {
                worked = true;
            }

            DiskProcessReleases(GetDiskManager());
        }
        else
        {
            worked = DestroyPendingStep() != 0;
            if (G_GameResourcesObjectPointer != nullptr)
            {
                worked = ResourcesStep(G_GameResourcesObjectPointer) != 0 || worked;
            }

            GetDiskManager();
            DiskNothing();
            worked = DiskCompactStep(GetDiskManager(), false) || worked;
            DiskProcessReleases(GetDiskManager());
        }

        CollectDeferredFrees(GetHeapManager());
        return worked;
    }

    bool ContextIsOption(GameContext*, const char* argument, const char* option)
    {
        return CompareStrings(argument, option, RetailLibc::StringLength(option)) == 0;
    }

    bool ContextGetOptionValue(GameContext*, const char* argument, const char* option, String* value)
    {
        u32 length = RetailLibc::StringLength(option);
        bool matches = false;
        if (CompareStrings(argument, option, length) == 0)
        {
            matches = argument[length] == '=';
        }

        if (!matches)
        {
            return false;
        }

        StringAssign(value, argument + length + 1);
        // In upper case, by the C library's character table (indexed by the signed character, like the retail code)
        for (s32 i = 0; i < value->length; i++)
        {
            s8 character = static_cast<s8>(value->string[i]);
            if ((CasingTable[character] & 0x2) != 0)
            {
                character = static_cast<s8>(character - 0x20);
            }

            value->string[i] = static_cast<char>(character);
        }

        return true;
    }

    GameContext* CreateGameContext(u32 argumentCount, char** arguments)
    {
        GameContext* context = GameContext::Construct(static_cast<GameContext*>(MemoryAllocate(sizeof(GameContext))));
        for (u32 i = 1; i < argumentCount; i++)
        {
            const char* argument = arguments[i];
            if (ContextIsOption(context, argument, "RB"))
            {
                context->flags |= GameContext::FlagRb;
            }

            String value;
            value.string = nullptr;
            value.length = 0;
            value.capacity = 0;
            if (ContextGetOptionValue(context, argument, "BATCH", &value))
            {
                StringAssign(&context->archivePath, value.string);
            }

            StringDestroy(&value);
        }

        return context;
    }
}

void PauseGame()
{
    if (g_GameState == GameStateRunning)
    {
        g_GameState = GameStatePausing;
        return;
    }

    if (g_GameState == GameStateResuming)
    {
        g_GameState = GameStatePaused;
    }
}

void ResumeGame()
{
    if (g_GameState == GameStatePausing)
    {
        g_GameState = GameStateRunning;
        return;
    }

    if (g_GameState == GameStatePaused)
    {
        g_GameState = GameStateResuming;
    }
}
