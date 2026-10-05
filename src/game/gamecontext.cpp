#include "game/context.h"
#include "game/copyprotection.h"
#include "game/gamecontroller.h"

#include "game/agentlab.h"
#include "game/attachments.h"
#include "game/chunkdata.h"
#include "game/dynamicscenery.h"
#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/cutscenes.h"
#include "game/decals.h"
#include "game/instancefactory.h"
#include "game/navigation.h"
#include "game/pads.h"
#include "game/particles.h"
#include "game/pickups.h"
#include "game/resources.h"
#include "game/rigidbody.h"
#include "game/filestream.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/savemanager.h"
#include "game/view.h"
#include "platform/graphics.h"
#include "platform/system.h"
#include "retail/libc.h"

// The game's side of the application (GameContext's functions, at the start of .text)
namespace
{
// The game's languages (English, French, German, Spanish, Italian)
constexpr u32 Languages = 5;
constexpr s32 GameEnglish = 0;
constexpr s32 GameFrench = 1;
constexpr s32 GameGerman = 2;
constexpr s32 GameSpanish = 3;
constexpr s32 GameItalian = 4;
// The save code's messages' texts from the second message on (the first keeps the save code's own string)
constexpr s32 SaveMessageTexts[] = {0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0xB7, 0x32,
                                    0x5F, 0x33, 0x34, 0x35, 0x36, 0x37, 0xB8, 0xB4, 0xB5, 0xB6, 0x38, 0x39, 0x3A,
                                    0xB9, 0x3B, 0x3C, 0x3D, 0x3E, 0x05, 0x3F, 0xCC, 0x41, 0x03, 0x02};
// The video controller's texts, the same in every language
constexpr const char* SkipHint = "Press (JUMP) to skip movie";
constexpr const char* DiscErrorTitle = "DISC ERROR";
// The room the preloaded chunks' list starts with and grows by, and the update rates' cutoffs' square roots and steps (the object
// nodes' and the model nodes')
constexpr u32 PreloadRoom = 10;
constexpr u32 UpdateCutoffRoot = 100;
constexpr u32 ObjectUpdateSteps = 2;
constexpr u32 ModelUpdateSteps = 3;
// The game's chunk manager's bytes (game/gamechunkmanager.cpp's)
constexpr u32 GameChunkManagerSize = 0x3200;
// The node kinds the start-up gives the instance queries: the lines of sight's cover (with kind 0x15, which no node is) and the
// solid instances, the instances impacts reach and whose bodies keep their gravity, and the ones the physics bodies collide with
constexpr u32 CoverKinds = 1u << NodeCrate | 1u << NodeGenericObject | 1u << NodePayGate | 1u << NodeUnusedObjectType;
constexpr u32 ImpactKinds = 1u << NodeCharacter | 1u << NodeCreature;
constexpr u32 PhysicsBodyKinds = 1u << NodeCrate | 1u << NodeGenericObject | 1u << NodePayGate;

// The game's languages, by the console's: English is the rest
s32 GameLanguageOf(Platform::System::ConsoleLanguage consoleLanguage)
{
    switch (consoleLanguage)
    {
    case Platform::System::LanguageFrench:
        return GameFrench;
    case Platform::System::LanguageSpanish:
        return GameSpanish;
    case Platform::System::LanguageGerman:
        return GameGerman;
    case Platform::System::LanguageItalian:
        return GameItalian;
    default:
        return GameEnglish;
    }
}
}

extern "C"
{
    // The node kinds of the instances impacts reach and whose bodies keep their gravity (game/behaviourevents.cpp's g_ImpactKinds,
    // game/trajectory.cpp's g_GravityKinds) and of the ones the physics bodies collide with (game/commandsphysics.cpp's)
    extern u32 g_ImpactKinds RETAIL(D_0030A120);
    extern u32 g_PhysicsBodyKinds RETAIL(D_0030A11C);
    // The colour filter effect on (the platform's renderer's)
    extern u8 g_ColourFilterEffect RETAIL(D_00309C51);
    extern void* G_ChunkManager;
    extern void* g_ConditionsChunkManager RETAIL(G_ChunkManager_0030A0C8);
    extern const u8 CasingTable[];

    // A copy of the frame's seconds nothing reads
    extern f32 g_UnusedFrameSeconds RETAIL(D_0030A0C4);

    extern const GccVTableEntry g_GameContextVTable[] RETAIL(GameArchivesReader_Methods);
    extern const GccVTableEntry g_StringListIteratorBaseVTable[] RETAIL(D_002EC4A8);
    // The item builders' vtables (a builder is its vtable alone): the game objects', the AgentLab items', the script commands'
    // and conditions', the maths', the models', the SM2's, the cameras', the sounds' and the chunk links' hulls'
    extern const GccVTableEntry g_ObjectItemBuilderVTable[] RETAIL(D_00303628);
    extern const GccVTableEntry g_AgentLabItemBuilderVTable[] RETAIL(D_002FD2C0);
    extern const GccVTableEntry g_CommandBuilderVTable[] RETAIL(ScriptBuilderFunctions);
    extern const GccVTableEntry g_ConditionBuilderVTable[] RETAIL(D_002EE368);
    extern const GccVTableEntry g_MathsItemBuilderVTable[] RETAIL(D_002F5CC0);
    extern const GccVTableEntry g_ModelItemBuilderVTable[] RETAIL(D_002F6268);
    extern const GccVTableEntry g_SceneryItemBuilderVTable[] RETAIL(D_002FC490);
    extern const GccVTableEntry g_CameraItemBuilderVTable[] RETAIL(D_00305128);
    extern const GccVTableEntry g_SoundItemBuilderVTable[] RETAIL(D_002FB558);
    extern const GccVTableEntry g_ChunkLinkItemBuilderVTable[] RETAIL(D_003069D0);
    // A byte the constructor clears, which nothing reads
    extern u8 g_UnusedContextByte RETAIL(D_0030A0F8);
    // The projectiles' pyramid hull made (game/projectiles.cpp)
    void MakePyramidHull() RETAIL(FUN_0010c160);
}

GameContext* GameContext::Construct(GameContext* context)
{
    GameContextPrototype::Construct(context);
    context->vtable = g_GameContextVTable;
    context->startChunk.string = nullptr;
    context->startChunk.capacity = 0;
    context->startChunk.length = 0;
    StringList& preload = context->preloadChunks;
    preload.count = 0;
    preload.capacity = PreloadRoom;
    preload.growth = PreloadRoom;
    String* items = NewArray<String>(PreloadRoom);
    for (u32 i = 0; i < PreloadRoom; i++)
    {
        items[i].string = nullptr;
        items[i].length = 0;
        items[i].capacity = 0;
    }

    preload.items = items;
    context->archivePath.string = nullptr;
    context->archivePath.capacity = 0;
    context->archivePath.length = 0;
    GameResources* resources = &context->resources;
    GameResources::Construct(resources);
    context->builders[BuilderObjects] = g_ObjectItemBuilderVTable;
    context->builders[BuilderBehaviours] = g_AgentLabItemBuilderVTable;
    context->builders[BuilderCommands] = g_CommandBuilderVTable;
    context->builders[BuilderConditions] = g_ConditionBuilderVTable;
    context->builders[BuilderMaths] = g_MathsItemBuilderVTable;
    context->builders[BuilderModels] = g_ModelItemBuilderVTable;
    context->builders[BuilderScenery] = g_SceneryItemBuilderVTable;
    ConstructGameFactory(reinterpret_cast<InstanceFactory*>(context->instanceFactory), resources);
    context->moreBuilders[BuilderCameras] = g_CameraItemBuilderVTable;
    context->moreBuilders[BuilderSounds] = g_SoundItemBuilderVTable;
    context->moreBuilders[BuilderChunkLinks] = g_ChunkLinkItemBuilderVTable;
    if (g_ObjectBuilder == nullptr)
    {
        g_ObjectBuilder = ObjectBuilder::Construct(static_cast<ObjectBuilder*>(MemoryAllocate(sizeof(ObjectBuilder))));
    }

    ObjectBuilder* builder = g_ObjectBuilder;
    RetailLibc::MemorySet(&context->flags, 0, sizeof(context->flags));
    context->mainPad = nullptr;
    context->firstFrameStamp = 0;
    context->chunkManager = nullptr;
    context->gameController = nullptr;
    context->unused3C = PreloadRoom;
    context->modelUpdateCutoffRoot = UpdateCutoffRoot;
    context->objectUpdateCutoffRoot = UpdateCutoffRoot;
    for (const GccVTableEntry*& factory : context->builders)
    {
        builder->Add(&factory);
    }

    for (const GccVTableEntry*& factory : context->moreBuilders)
    {
        builder->Add(&factory);
    }

    InitUnusedAgentLabSettings();
    g_UnusedContextByte = 0;
    MakePyramidHull();
    ResetParticleEmitters();
    context->flags.colourFilter = 1;
    return context;
}

void GameContext::PreloadChunks()
{
    GameController* controller = gameController;
    GameProgress* progress = &controller->progress;
    if (G_ChunkLoadingManager_->bits.mode != LoadingKnownAtOnce ||
        controller->LoadGlobalResources() == 0)
    {
        return;
    }

    for (s32 index = static_cast<s32>(preloadChunks.count) - 1;
         index >= 0 && static_cast<u32>(index) < preloadChunks.count; index--)
    {
        StringAssign(&progress->startChunk, preloadChunks.items[index].string);
        if (progress->LoadStartChunk(nullptr) != 0)
        {
            while (progress->ChunkLoaded(GameProgress::LoadedAll, 1) == 0)
            {
            }

            progress->ForgetChunks();
        }
    }

    gameController->EnableCharacters(1);
}

void GameContext::GameEndFrame(bool playingMovie)
{
    GameRendererController* renderers = G_GameRendererController;
    GameTimeController* clocks = G_GameClockController;
    g_RenderTarget = G_Renderer_->target;
    gameController->SetView(playingMovie);
    renderers->DrawRendererScenes();
    if (playingMovie)
    {
        gameController->Draw(playingMovie);
        return;
    }

    f32 seconds = static_cast<f32>(static_cast<s32>(clocks->clocks[FirstClock].advance)) * g_SecondsPerClockUnit;
    UpdateAndDrawParticles(g_GameState == GameStatePaused, seconds);
    Platform::Graphics::UseHelperPrograms(Platform::Graphics::DecalPrograms, true);
    UpdateDecalsVU0(&g_DecalData, seconds);
    Platform::Graphics::DrawDecals(&g_DecalData);
    gameController->Draw(0);
}

void MakeRate(UpdateRate* rate, u8 graceRoot, u8 cutoffRoot, u32 steps)
{
    u32 grace = graceRoot * graceRoot;
    rate->grace = static_cast<u16>(grace);
    rate->cutoff = static_cast<u16>(cutoffRoot * cutoffRoot - grace);
    rate->slope = static_cast<f32>(steps) / static_cast<f32>(rate->cutoff + 1);
}

namespace
{
void DestroyStringListIterator(StringListIterator* iterator, u32 flags)
{
    iterator->vtable = g_StringListIteratorBaseVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}
}

void StringListIterator::Destroy(u32 flags)
{
    DestroyStringListIterator(this, flags);
}

void StringListIterator::BaseDestroy(u32 flags)
{
    DestroyStringListIterator(this, flags);
}

void StringListIterator::First()
{
    index = 0;
}

u32 StringListIterator::IsDone()
{
    return index < 0 || static_cast<u32>(index) >= list->count;
}

String* StringListIterator::Current()
{
    return &list->items[index];
}

void StringListIterator::Next()
{
    index++;
}

void StringListIterator::Previous()
{
    index--;
}

void StringListIterator::Last()
{
    index = static_cast<s32>(list->count - 1);
}

StringListIterator* StringListIterator::Assign(const StringListIterator* other)
{
    list = other->list;
    index = other->index;
    return this;
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
        // The chunks are loaded around the view's camera while the camera shown is 1 or 2 (which ShowCamera never shows: its
        // cameras are GameController::Camera's), else around the character played
        GameController* controller = gameController;
        ReferencedObject* focus;
        u32 camera = static_cast<u32>(controller->states.camera);
        if (camera - 1 < 2)
        {
            Reference* shown = G_Renderer_->view->cameraObject;
            focus = shown != nullptr ? shown->object : nullptr;
        }
        else
        {
            GameProgress& progress = controller->progress;
            u32 character = progress.play.character;
            Reference* reference = character != GameProgress::NoCharacter ? progress.characters[character] : nullptr;
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

    if (g_SaveManager == nullptr)
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
    // The grace's square root a quarter of the cutoff's
    UpdateRate objectRate;
    UpdateRate modelRate;
    MakeRate(&objectRate, static_cast<u8>(objectUpdateCutoffRoot >> 2), static_cast<u8>(objectUpdateCutoffRoot), ObjectUpdateSteps);
    MakeRate(&modelRate, static_cast<u8>(modelUpdateCutoffRoot >> 2), static_cast<u8>(modelUpdateCutoffRoot), ModelUpdateSteps);
    u32 loadingMode = flags.givesLoadingMode != 0 ? static_cast<u32>(flags.loadingMode) : LoadingStreamed;
    String unused;
    unused.string = nullptr;
    unused.length = 0;
    unused.capacity = 0;
    InitLanguages(Languages, g_GameLanguageNames);
    for (u32 index = 0; index < ReadersStorageCount; index++)
    {
        GameReadersStorage* storage = InitReadersStorage(index);
        if (archivePath.length != 0)
        {
            FileStreamOpenArchive(storage->stream, archivePath.string, 1, storage);
        }
    }

    mainPad = static_cast<GamePadController*>(G_GamePadController)->pads[0];
    InitPickupSpin();
    ResetCustomPickups();
    ResetCustomProjectiles();
    g_ObjectUpdateRate.slope = objectRate.slope;
    g_ModelUpdateRate.slope = modelRate.slope;
    g_ObjectUpdateRate.grace = objectRate.grace;
    g_ObjectUpdateRate.cutoff = objectRate.cutoff;
    g_ModelUpdateRate.grace = modelRate.grace;
    g_CoverKinds = CoverKinds;
    g_AttachedPositionFlag = AiPositionFlags::Attached;
    g_ModelUpdateRate.cutoff = modelRate.cutoff;
    g_ImpactKinds = ImpactKinds;
    g_SolidKinds = SolidObjectNodeKinds;
    g_PhysicsBodyKinds = PhysicsBodyKinds;
    // The kinds' step order: graples before the models, follow nodes before the cameras' lenses, controls before the rigid bodies,
    // then the agents after the controls
    InitStepKinds();
    InsertStepKindBefore(NodeGraple, NodeModel);
    InsertStepKindBefore(NodeFollow, NodeCameraLens);
    InsertStepKindBefore(NodeControls, NodeRigidBody);
    InsertStepKindAfter(NodeCrate, NodeControls);
    InsertStepKindAfter(NodePickup, NodeCrate);
    InsertStepKindAfter(NodeCreature, NodePickup);
    InsertStepKindAfter(NodeGenericObject, NodeCreature);
    InsertStepKindAfter(NodeGrabbable, NodeGenericObject);
    InsertStepKindAfter(NodePayGate, NodeGrabbable);
    InsertStepKindAfter(NodeCharacter, NodePayGate);
    chunkManager = ConstructChunkManager(MemoryAllocate(GameChunkManagerSize), instanceFactory, flags.queuesFiles,
                                         flags.takesNoObjects);
    g_ColourFilterEffect = static_cast<u8>(flags.colourFilter);
    g_ConditionsChunkManager = chunkManager;
    G_ChunkManager = chunkManager;
    g_PhysicsWorld = ConstructPhysicsWorld(static_cast<PhysicsWorld*>(MemoryAllocate(sizeof(PhysicsWorld))));
    auto* memory = static_cast<GameController*>(MemoryAllocate(sizeof(GameController)));
    GameController* controller =
        GameController::Construct(memory, mainPad, reinterpret_cast<s32>(secondPad), G_Renderer_,
                                  static_cast<ChunkManager*>(chunkManager), instanceFactory, &resources);
    gameController = controller;
    controller->flags.queuesFiles = flags.queuesFiles;
    gameController->flags.noMenus = flags.takesNoObjects;
    StringAssign(&controller->progress.startChunk, startChunk.string);
    g_AgentNodesGameController = gameController;
    G_GameController = gameController;
    g_AgentsGameController = gameController;
    g_OlegGameController = gameController;
    g_ConditionsGameController = gameController;
    if (flags.takesNoObjects != 0 && loadingMode != LoadingQueuedOnly)
    {
        loadingMode = LoadingAllAtOnce;
    }

    G_ChunkLoadingManager_ = ConstructChunkLoadingManager(static_cast<ChunkLoadingManager*>(MemoryAllocate(sizeof(ChunkLoadingManager))),
                                                          &g_GlobalClock, loadingMode);
    ChunkLoadingManager* loading = G_ChunkLoadingManager_;
    StringAssign(&loading->path, chunksPath);
    // In lower case
    for (s32 i = 0; i < loading->path.length; i++)
    {
        char* character = loading->path.string + i;
        s8 value = *character;
        if ((CasingTable[value] & RetailLibc::CasingUpperCase) != 0)
        {
            *character = static_cast<char>(value + ('a' - 'A'));
        }
    }

    loading = G_ChunkLoadingManager_;
    loading->bits.queuesFiles = flags.queuesFiles;
    loading->bits.takesNoObjects = flags.takesNoObjects;
    G_GameMovieController = MoviePlayer::Construct(static_cast<MoviePlayer*>(MemoryAllocate(sizeof(MoviePlayer))));
    G_VideoController = ConstructVideoController(MemoryAllocate(sizeof(VideoController)), instanceFactory, ObjectClock);
    g_DynamicSceneryClockIndex = ObjectClock;
    PreloadChunks();
    SetGameLanguage(GameLanguageOf(Platform::System::Language()));
    StringDestroy(&unused);
}
