#include "game/context.h"
#include "game/gamecontroller.h"

#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/rigidbody.h"
#include "game/filestream.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/savemanager.h"
#include "game/view.h"
#include "platform/system.h"

// The game's side of the application (GameContext's functions, at the start of .text)
namespace
{
// The game controller's camera mode (its state word's bits 19-22: 1 and 2 the cutscene camera's)
constexpr u32 CameraModeShift = 19;
constexpr u32 CameraModeMask = 0xF;
// The game's languages (English, French, German, Spanish, Italian)
constexpr u32 Languages = 5;
// The save code's messages' texts from the second message on (the first keeps the save code's own string)
constexpr s32 SaveMessageTexts[] = {0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0xB7, 0x32,
                                    0x5F, 0x33, 0x34, 0x35, 0x36, 0x37, 0xB8, 0xB4, 0xB5, 0xB6, 0x38, 0x39, 0x3A,
                                    0xB9, 0x3B, 0x3C, 0x3D, 0x3E, 0x05, 0x3F, 0xCC, 0x41, 0x03, 0x02};
// The video controller's texts, the same in every language
constexpr const char* SkipHint = "Press (JUMP) to skip movie";
constexpr const char* DiscErrorTitle = "DISC ERROR";

// The game's languages, by the console's: English is the rest
s32 GameLanguageOf(s32 consoleLanguage)
{
    switch (consoleLanguage)
    {
    case 2:
        return 1;
    case 3:
        return 3;
    case 4:
        return 2;
    case 5:
        return 4;
    default:
        return 0;
    }
}
}

extern "C"
{
    extern u32 D_0030A104;
    extern u16 D_0030AA9C;
    extern u32 D_0030A120;
    extern u32 D_0030A118;
    extern u32 D_0030A11C;
    extern u8 D_00309C51;
    extern void* G_ChunkManager;
    extern void* G_ChunkManager_0030A0C8;
    extern u32 G_DynamicSceneryClockIndex;
    extern const u8 CasingTable[];

    void MakeRate(UpdateRate* rate, u16 first, u16 second, u32 third) RETAIL(FUN_001011d8);
    void FUN_00108d70();
    void FUN_0011eaa8();
    void FUN_0011ef50();
    void FUN_00199aa0();
    void FUN_00199ae0(s32 first, s32 second);
    void FUN_00199b60(s32 first, s32 second);
    void* ConstructChunkManager(void* manager, void* resourceManager, u32 bit6, u32 bit7) RETAIL(FUN_0017a508);
    VideoController* ConstructVideoController(void* controller, void* resourceManager, u32 unknown) RETAIL(FUN_0029a7e8);
    void FUN_00100100(GameContext* context);
    // A copy of the frame's seconds nothing reads
    extern f32 g_UnusedFrameSeconds RETAIL(D_0030A0C4);
    // A step of the copy protection (still asm): a frame counter, and once its check has failed, an instruction of the game
    // broken a while later
    u32 CopyProtectionStep() RETAIL(FUN_00181c70);
}

void GameContext::GameBeginFrame(bool)
{
    gameController->BeginFrame();
}

void GameContext::GameUpdate(bool playingMovie)
{
    if (playingMovie)
    {
        gameController->Update(1);
        CopyProtectionStep();
        return;
    }

    GameTimeController* clocks = G_GameClockController;
    f32 seconds = static_cast<f32>(clocks->frameTime) * g_SecondsPerClockUnit;
    g_UnusedFrameSeconds = seconds;
    g_FrameSeconds = seconds;
    if (firstFrameStamp == 0)
    {
        firstFrameStamp = clocks->lastStamp;
    }

    if (gameController->Update(0) != 0)
    {
        // The chunks are loaded around what the camera follows in the cutscene camera's modes, else the character played
        GameController* controller = gameController;
        ReferencedObject* focus;
        u32 cameraMode = static_cast<u32>(controller->states >> CameraModeShift & CameraModeMask);
        if (cameraMode - 1 < 2)
        {
            Reference* camera = G_Renderer_->view->cameraObject;
            focus = camera != nullptr ? camera->object : nullptr;
        }
        else
        {
            GameProgress& progress = controller->progress;
            u32 character = progress.bits >> 8 & 0xF;
            Reference* reference = character != 6 ? progress.characters[character] : nullptr;
            focus = reference != nullptr ? reference->object : nullptr;
        }

        if (focus != nullptr)
        {
            SetChunkLoadingFocus(G_ChunkLoadingManager_, focus);
            ChunkData* chunk = focus->chunk;
            AssignChunkData(&GetChunkList()->current, chunk);
        }
    }

    CopyProtectionStep();
}

void GameContext::GameRender(bool)
{
    gameController->EndFrame();
}

void GameContext::LanguageChanged(u32)
{
    if (G_VideoController != nullptr)
    {
        StringAssign(&G_VideoController->skipHint, SkipHint);
        StringAssign(&G_VideoController->discErrorTitle, DiscErrorTitle);
    }

    if (G_UnkStruct_5C0 == nullptr)
    {
        return;
    }

    for (u32 message = 0; message < sizeof(SaveMessageTexts) / sizeof(SaveMessageTexts[0]); message++)
    {
        g_SaveMessageTexts[message + 1] = SaveMessageTexts[message];
    }
}

void GameContext::StartGame()
{
    UpdateRate first;
    UpdateRate second;
    MakeRate(&first, static_cast<u16>(unknownF4 >> 2 & 0xFF), static_cast<u8>(unknownF4), 2);
    MakeRate(&second, static_cast<u16>(unknownF8 >> 2 & 0xFF), static_cast<u8>(unknownF8), 3);
    u32 loadingState = (flags & 0x100) != 0 ? flags >> 9 & 0xF : 5;
    String unused;
    unused.string = nullptr;
    unused.length = 0;
    unused.capacity = 0;
    InitLanguages(Languages, g_GameLanguageNames);
    for (u32 index = 0; index < 2; index++)
    {
        GameReadersStorage* storage = InitReadersStorage(index);
        if (archivePath.length != 0)
        {
            FileStreamOpenArchive(storage->stream, archivePath.string, 1, storage);
        }
    }

    mainPad = reinterpret_cast<void**>(G_GamePadController)[1];
    FUN_00108d70();
    FUN_0011eaa8();
    FUN_0011ef50();
    g_ObjectUpdateRate.slope = first.slope;
    D_0030A838.slope = second.slope;
    g_ObjectUpdateRate.grace = first.grace;
    g_ObjectUpdateRate.cutoff = first.cutoff;
    D_0030A838.grace = second.grace;
    D_0030A104 = 0x252000;
    D_0030AA9C = 0x20;
    D_0030A838.cutoff = second.cutoff;
    D_0030A120 = 0x9000;
    D_0030A118 = 0x5A010;
    D_0030A11C = 0x52000;
    FUN_00199aa0();
    FUN_00199ae0(0x13, 3);
    FUN_00199ae0(0x16, 9);
    FUN_00199ae0(0xB, 5);
    FUN_00199b60(0xD, 0xB);
    FUN_00199b60(0xE, 0xD);
    FUN_00199b60(0xF, 0xE);
    FUN_00199b60(0x10, 0xF);
    FUN_00199b60(0x11, 0x10);
    FUN_00199b60(0x12, 0x11);
    FUN_00199b60(0xC, 0x12);
    chunkManager = ConstructChunkManager(MemoryAllocate(0x3200), resourceManager, flags >> 6 & 1, flags >> 7 & 1);
    D_00309C51 = static_cast<u8>(flags >> 3 & 1);
    G_ChunkManager_0030A0C8 = chunkManager;
    G_ChunkManager = chunkManager;
    g_PhysicsWorld = ConstructPhysicsWorld(static_cast<PhysicsWorld*>(MemoryAllocate(sizeof(PhysicsWorld))));
    auto* memory = static_cast<GameController*>(MemoryAllocate(sizeof(GameController)));
    GameController* controller = GameController::Construct(memory, static_cast<GamePad*>(mainPad), static_cast<s32>(unknownE8), G_Renderer_,
                                                           static_cast<ChunkManager*>(chunkManager), resourceManager,
                                                           reinterpret_cast<GameResources*>(resources));
    gameController = controller;
    controller->flags = (controller->flags & ~2u) | static_cast<u32>(flags >> 6 & 1) << 1;
    gameController->flags = (gameController->flags & ~4u) | static_cast<u32>(static_cast<u8>(flags) >> 7) << 2;
    StringAssign(&controller->progress.startChunk, startChunk);
    G_GameController_00309890 = gameController;
    G_GameController = gameController;
    G_GameController_00309914 = gameController;
    G_GameController_00309950 = gameController;
    G_GameController_0030988C = gameController;
    if ((flags & 0x80) != 0 && loadingState != 0)
    {
        loadingState = 2;
    }

    G_ChunkLoadingManager_ = ConstructChunkLoadingManager(static_cast<ChunkLoadingManager*>(MemoryAllocate(sizeof(ChunkLoadingManager))),
                                                          &g_GlobalClock, loadingState);
    ChunkLoadingManager* loading = G_ChunkLoadingManager_;
    StringAssign(&loading->path, unknown0C);
    // In lower case
    for (s32 i = 0; i < loading->path.length; i++)
    {
        char* character = loading->path.string + i;
        s8 value = *character;
        if ((CasingTable[value] & 1) != 0)
        {
            *character = static_cast<char>(value + 0x20);
        }
    }

    loading = G_ChunkLoadingManager_;
    u32 bits = (loading->bits & 0xEFFFFFFF) | (flags >> 6 & 1) << 28;
    loading->bits = bits;
    loading->bits = (bits & 0xDFFFFFFF) | static_cast<u32>(static_cast<u8>(flags) >> 7) << 29;
    G_GameMovieController = MoviePlayer::Construct(static_cast<MoviePlayer*>(MemoryAllocate(sizeof(MoviePlayer))));
    G_VideoController = ConstructVideoController(MemoryAllocate(0x15B0), resourceManager, 1);
    G_DynamicSceneryClockIndex = 1;
    FUN_00100100(this);
    SetGameLanguage(GameLanguageOf(Platform::System::Language()));
    StringDestroy(&unused);
}
