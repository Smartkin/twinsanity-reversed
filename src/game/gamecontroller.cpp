#include "game/gamecontroller.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/camerarig.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/chunkfiles.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/colour.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/decals.h"
#include "game/effects.h"
#include "game/events.h"
#include "game/filestream.h"
#include "game/followcamera.h"
#include "game/instances.h"
#include "game/language.h"
#include "game/math.h"
#include "game/menus.h"
#include "game/overlay.h"
#include "game/movie.h"
#include "game/pads.h"
#include "game/particles.h"
#include "game/place.h"
#include "game/player.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/savemanager.h"
#include "game/shapes.h"
#include "game/sound.h"
#include "game/stream.h"
#include "game/string.h"
#include "game/vehicles.h"
#include "game/widgets.h"
#include "platform/graphics.h"
#include "platform/stream.h"
#include "retail/libc.h"

#include <cstddef>
#include <cstdint>

namespace
{
// OLEG's pictures the game controller wants: the Crash title's only one, the game over screens of the characters (the tiles'
// pictures from TilesGameOver: Cortex, Crash, Crash and Cortex, the Mecha-Bandicoot, Nina) and the loading screens (three from
// TilesLoading)
constexpr u32 CrashTitlePicture = 0;
constexpr u32 GameOverCortex = OLEG::TilesGameOver;
constexpr u32 GameOverCrash = OLEG::TilesGameOver + 1;
constexpr u32 GameOverCrashAndCortex = OLEG::TilesGameOver + 2;
constexpr u32 GameOverMecha = OLEG::TilesGameOver + 3;
constexpr u32 GameOverNina = OLEG::TilesGameOver + 4;
constexpr u32 LoadingPictures = 3;
// The start-up's files: the language's voices' folder (the language's name after it), the music, the icons, the font, the front
// end's sounds and the default chunk; the text files' folders (in the language folder) and extension
constexpr const char* SoundFolder = "Crash6\\";
constexpr const char* MusicBank = "Crash6\\Music";
constexpr const char* IconsFile = "StartUp\\Icons.psm";
constexpr const char* FontFile = "StartUp\\Fonts\\Crash_Euro";
constexpr const char* FrontEndFile = "StartUp\\Frontend.bin";
constexpr const char* DefaultChunk = "StartUp\\Default";
constexpr const char* TextFolders[TextFiles] = {"Code", "AgentLab"};
constexpr const char* TextExtension = ".txt";
// The screens' fades (seconds): most, the longer ones (to and from black, the pictures, the logos' wait), the shortest (a movie's
// black and the missing controller's screen), and how long the HUD stays when it's shown for a while
constexpr f32 FadeSeconds = 0.25f;
constexpr f32 LongFadeSeconds = 0.5f;
constexpr f32 ShortFadeSeconds = 0x1.99999Ap-4f;
constexpr f32 HudShownSeconds = 3.0f;
// Every widget slot of OLEG's
constexpr u64 EveryWidget = ~u64{0};
// The pause screens of the pause reasons from ReasonStart to ReasonExtras (none for quitting)
constexpr s32 NoScreen = -1;
constexpr u32 PauseScreenCount = GameController::ReasonExtras;
constexpr s32 PauseScreens[PauseScreenCount] = {
    OLEG::ScreenPauseMenu,      OLEG::ScreenNoController, OLEG::ScreenDiscError,  OLEG::ScreenAutosaveOff,
    OLEG::ScreenAutosaveOn,     OLEG::ScreenAutosaveFailed, OLEG::ScreenFourthNotice, NoScreen,
    OLEG::ScreenLevels,         OLEG::ScreenLevels + 1,   OLEG::ScreenLevels + 2, OLEG::ScreenLevels + 3,
    OLEG::ScreenExtras,
};
// The last areas of the levels pages but the last (the progress's last area open), and the last page with an area open
constexpr u32 LevelsPageAreas[3] = {6, 13, 20};

u32 LastLevelsPage(u32 area)
{
    if (area < LevelsPageAreas[0])
    {
        return GameController::ReasonFirstLevels;
    }

    if (area < LevelsPageAreas[1])
    {
        return GameController::ReasonFirstLevels + 1;
    }

    return area < LevelsPageAreas[2] ? GameController::ReasonFirstLevels + 2 : GameController::ReasonFirstLevels + 3;
}

// The front end's sounds
constexpr u32 FrontEndSounds = 3;
// The title: its "press start" (the text, where it's drawn (fractions of the screen), centred, at three quarters of the breathing
// scale; the frames the title waits before it) and the attract movie's wait (seconds)
constexpr u32 PressStartText = 0x46;
constexpr f32 PressStartX = 0.5f;
constexpr f32 PressStartY = Rounded(0.9);
constexpr f32 PressStartScale = 0.75f;
constexpr s32 PressStartFrames = 50;
constexpr f32 AttractSeconds = 50.0f;
// The credits: their folder, music tracks (a new one 19 seconds after the one before, faded in over 3 seconds but the first, in
// the music group, started at once, looping)
constexpr const char* CreditsFolder = "Language\\Credits\\";
constexpr u32 CreditsTracks = 11;
constexpr u16 CreditsMusic[CreditsTracks] = {0x3A, 0x1C, 0x88, 0x1E, 0x23, 0x25, 0x29, 0x36, 0x3C, 0x3D, 0x1B};
constexpr f32 CreditsTrackSeconds = 19.0f;
constexpr f32 CreditsFadeSeconds = 3.0f;
// The gallery's pictures' numbers: two digits at least
constexpr u32 TwoDigits = 10;
// The wait before OLEG adds the next wumpa fruit to the count, set again for the credits (seconds)
constexpr f32 WumpaAddSeconds = Rounded(0.15);
// The message the second character is sent when the two are tied together (to its object node)
constexpr u32 TiedMessage = 0x39;
// The instances the game's reset resets: every one with a node of a kind but the cameras' lens
constexpr u32 ResetKinds = ~(1u << NodeCameraLens);

// One of the loading screens, at random
u32 RandomLoadingScreen()
{
    f32 random = GetRandFloat();
    u32 loading = OLEG::TilesLoading + LoadingPictures - 1;
    if (random < 1.0f)
    {
        loading = static_cast<s32>(random * static_cast<f32>(LoadingPictures)) + OLEG::TilesLoading;
    }

    return loading;
}

// Every chunk's clocks stopped and the game paused
void StopChunks()
{
    for (ChunkData* chunk = GetChunkList()->first; chunk != nullptr; chunk = chunk->next)
    {
        if (chunk->clocks != nullptr)
        {
            TimeClocksStop(chunk->clocks);
        }
    }

    PauseGame();
}

// Every chunk's clocks started and the game let go
void StartChunks()
{
    for (ChunkData* chunk = GetChunkList()->first; chunk != nullptr; chunk = chunk->next)
    {
        if (chunk->clocks != nullptr)
        {
            TimeClocksStart(chunk->clocks);
        }
    }

    ResumeGame();
}

// The played character's instance, and the second character's
InstanceContext* CharacterInstance(GameProgress* progress)
{
    return progress->Instance(progress->play.character);
}

InstanceContext* SecondCharacterInstance(GameProgress* progress)
{
    return progress->Instance(progress->play.second);
}
}

GameController* GameController::Construct(GameController* controller, GamePad* pad, s32 secondPad, Renderer* renderer,
                                          ChunkManager* chunks, void* factory, GameResources* resources)
{
    controller->pad = pad;
    controller->renderer = renderer;
    controller->chunkManager = chunks;
    controller->unused34 = factory;
    controller->resources = resources;
    controller->secondPad = reinterpret_cast<GamePad*>(secondPad);
    SoundTable::Construct(&controller->frontEndSounds, FrontEndSounds);
    controller->gallery.string = nullptr;
    controller->gallery.capacity = 0;
    controller->gallery.length = 0;
    Font::Construct(&controller->font);
    RenderView::Construct(&controller->view);
    GameCameraRig::Construct(&controller->camera);
    CutsceneCameraRig::Construct(&controller->cutsceneCamera);
    GameProgress::Construct(&controller->progress);
    SaveController& save = controller->saveController;
    save.chunk.string = nullptr;
    save.chunk.capacity = 0;
    save.chunk.length = 0;
    save.place = NoInstanceId;
    RetailLibc::MemorySet(&save, 0, sizeof(save.summary) + sizeof(save.options));
    save.timePlayed = 0;
    save.data = static_cast<u8*>(MemoryAllocate2(SaveController::DataSize));
    save.progress = &controller->progress;
    OLEG::Construct(&controller->oleg, &controller->font, &controller->progress);
    controller->credits = nullptr;
    RetailLibc::MemorySet(&controller->states, 0, sizeof(controller->states) + sizeof(controller->flags));
    controller->states.value = 0;
    controller->states.nextState = NoState;
    controller->hudDelay = 0;
    controller->frame = 0;
    controller->movieFrame = 0;
    controller->stateFrame = 0;
    controller->saveTime = 0;
    g_MainGamePad = controller->pad;
    MakeFlatBoxHull();
    InitFreedMemory();
    controller->renderer->view = &controller->view;
    controller->states.unused16 = 1;
    return controller;
}

u32 GameController::Update(u32 keepFreed)
{
    states.startPressed = GetButtonState(pad, PadStart, true);
    frame++;
    UpdateSaving(&g_GlobalClock);
    if (NextState() != NoState)
    {
        stateFrame = frame;
        GameControllerStates taken = states;
        taken.lastState = taken.state;
        taken.state = taken.nextState;
        taken.nextState = NoState;
        states = taken;
        stateTime = g_GlobalClock.time;
    }

    u32 next = NoState;
    switch (State())
    {
    case StateWaitingForLoader:
        if (G_ChunkLoadingManager_->bits.mode == LoadingKnownAtOnce)
        {
            next = StateStartingPlay;
        }
        else
        {
            StopChunks();
            next = StateStarting;
        }

        break;
    case StateStarting:
        next = Starting(&g_GlobalClock);
        if (next != NoState)
        {
            oleg.SetUpParticles();
        }

        break;
    case StateWaitingForPad:
        next = WaitingForPad(&g_GlobalClock);
        break;
    case StateCheckingCard:
        next = CheckingCard(&g_GlobalClock);
        break;
    case StateVivendiLogo:
        next = PlayMovie(&g_GlobalClock, static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), GameMovie::Vivendi, 1,
                         nullptr) != 0
                   ? StateTravellersTalesLogo
                   : NoState;
        break;
    case StateTravellersTalesLogo:
        if (PlayMovie(&g_GlobalClock, 0, GameMovie::TravellersTales, 1, nullptr) == 0)
        {
            break;
        }

        if (progress.startChunk.length > 0)
        {
            // The start chunk given on the command line
            states.entry = EntrySaved;
            saveController.Take(&progress, chunkManager, nullptr);
            next = StateLoadingLevel;
        }
        else
        {
            next = StateLoadingTitle;
        }

        break;
    case StateLoadingTitle:
        next = LoadingTitle(&g_GlobalClock);
        break;
    case StateTitle:
        next = Title(&g_GlobalClock);
        break;
    case StateMainMenu:
        next = MainMenu(&g_GlobalClock);
        break;
    case StateLoadingLevel:
        next = LoadingLevel(&g_GlobalClock);
        break;
    case StateNewGame:
        next = NewGame(&g_GlobalClock);
        break;
    case StateStartingPlay:
        next = StartingPlay(&g_GlobalClock);
        break;
    case StatePlaying:
        next = Playing(&g_GlobalClock);
        break;
    case StateWatching:
        next = Watching(&g_GlobalClock);
        break;
    case StateMovie:
    {
        if (stateFrame == frame)
        {
            StopChunks();
        }

        u8 skipped;
        if (PlayMovie(&g_GlobalClock, movieDelay, movie, 1, &skipped) != 0)
        {
            MovieEnded();
        }

        break;
    }
    case StatePaused:
        next = Paused(&g_GlobalClock);
        break;
    case StateGallery:
        next = Gallery(&g_GlobalClock);
        break;
    case StateWaiting:
        if (static_cast<s32>(g_GlobalClock.time - static_cast<u32>(stateTime)) >= static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds))
        {
            next = static_cast<u32>(states.returnState);
        }

        break;
    case StateGameOver:
        next = GameOver(&g_GlobalClock);
        break;
    case StateCredits:
        next = Credits(&g_GlobalClock);
        break;
    case StateRestartingTitle:
    {
        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        u32 hidden;
        u32 shown;
        GetColor(&hidden, ColourTransparentBlack);
        GetColor(&shown, ColourBlack);
        oleg.fader.hiddenColour = hidden;
        oleg.fader.shownColour = shown;
        next = StateLoadingTitle;
        break;
    }
    case StateFadingOut:
        next = FadingOut(&g_GlobalClock);
        break;
    case StateRestarting:
        next = Restarting(&g_GlobalClock);
        break;
    default:
        break;
    }

    if (next != NoState)
    {
        SetNextState(next);
    }

    InstanceContext* player = CharacterInstance(&progress);
    auto* node = player != nullptr ? static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodeCharacter)) : nullptr;
    PlayerCharacter* character = node != nullptr ? node->character : nullptr;
    if (State() == StatePlaying)
    {
        u32 mode = progress.play.mode;
        if (mode == PlayTimed)
        {
            progress.timeLeft -= G_GameClockController->clocks[ObjectClock].advance;
        }

        // The triangle button shows the HUD
        if (mode == PlayNormal && GetButtonState(pad, PadTriangle, false))
        {
            oleg.Show(oleg.screens[OLEG::ScreenHud], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds),
                      static_cast<s32>(g_ClockUnitsPerSecond * HudShownSeconds));
        }
    }

    oleg.Update(&g_GlobalClock, pad, character);
    if (State() != StatePlaying && State() != StateWatching)
    {
        return 0;
    }

    StepPickupSpin(&g_GlobalClock);
    if (player != nullptr)
    {
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, NodeFollow));
        SetSoundListener(follow->cameraInstance != nullptr ? follow->cameraInstance->object : nullptr);
    }

    if (keepFreed == 0)
    {
        ReleaseFreedMemory();
    }

    return 1;
}

u32 GameController::Starting(TimeClock*)
{
    if (frame == stateFrame)
    {
        oleg.WantPicture(OLEG::PictureTiles, OLEG::TilesLegal);
        return NoState;
    }

    if (ShowPictureScreen(OLEG::ScreenPicture) == 0)
    {
        return NoState;
    }

    if (frame != movieFrame)
    {
        return StateWaitingForPad;
    }

    String voices;
    StringConstruct(&voices, SoundFolder);
    g_SaveManager = SaveManager::Construct(static_cast<SaveManager*>(MemoryAllocate(sizeof(SaveManager))), this);
    LoadTexts();
    StringAppend(&voices, g_LanguageNames[g_CurrentLanguage]);
    SetMusicBank(MusicBank);
    SetVoiceBank(voices.string);
    String icons;
    StringConstruct(&icons, IconsFile);
    MemoryStream stream;
    MemoryStream::ConstructFromFile(&stream, icons.string, false);
    for (u32 icon = OLEG::SpriteIcons; icon < OLEG::SpriteCount; icon++)
    {
        Sprite& sprite = oleg.sprites[icon];
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::ReadSlot, &stream);
    }

    stream.Destroy(DestroyOnly);
    StringDestroy(&icons);
    LoadGlobalResources();
    oleg.WantPicture(OLEG::PictureCrashTitle, CrashTitlePicture);
    oleg.LoadPicture(OLEG::PictureCrashTitle, 0);
    StringDestroy(&voices);
    return NoState;
}

void GameController::LoadTexts()
{
    u32 language = g_CurrentLanguage;
    const char* name = g_LanguageNames[language];
    for (u32 file = 0; file < TextFiles; file++)
    {
        String path;
        StringConstruct(&path, g_LanguageFolder);
        StringAppend(&path, TextFolders[file]);
        StringAppend(&path, "\\");
        StringAppend(&path, name);
        StringAppend(&path, TextExtension);
        texts[file] = MemoryStream::ConstructFromFile(static_cast<MemoryStream*>(MemoryAllocate(sizeof(MemoryStream))), path.string,
                                                      true);
        ReadTextFile(file, language, reinterpret_cast<char*>(texts[file]->begin));
        StringDestroy(&path);
    }

    UseLanguageTexts();
}

u32 GameController::LoadGlobalResources()
{
    SoundTableItem sounds;
    sounds.vtable = g_SoundTableItemVTable;
    sounds.table = &frontEndSounds;
    sounds.objects = nullptr;
    font.Read(FontFile);
    renderer->font = &font;
    font.width = g_FontSize.x;
    font.height = g_FontSize.y;
    if (flags.queuesFiles != 0)
    {
        AddResourcePackageToLoadQueue(&sounds, FrontEndFile, 0);
    }

    LoadQueuedSectionsIntoMemory_();
    if (flags.noMenus == 0)
    {
        oleg.StartUp(pad, &font, &font, &frontEndSounds);
    }

    chunkManager->LoadDefault(DefaultChunk);
    sounds.vtable = g_ItemInterfaceVTable;
    return 1;
}

u32 GameController::UpdateSaving(TimeClock* clock)
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (saves == nullptr)
    {
        return 0;
    }

    u32 step = static_cast<u32>(states.savingStep);
    if (step == SavingWait)
    {
        if (saveTime == 0)
        {
            saveTime = static_cast<s32>(clock->time);
        }
        else if (static_cast<s32>(clock->time - static_cast<u32>(saveTime)) >= static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds))
        {
            SaveManagerRequest(saves, SaveOperationCheckInserted, 0);
            SetSavingStep(SavingSaved);
        }

        return 0;
    }

    if (saves->bits.operation != 0)
    {
        return 0;
    }

    s32 fade = static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds);
    bool flagged = saves->results.flagged != 0;
    switch (step)
    {
    case SavingNone:
        return 0;
    case SavingSaved:
        if (flagged)
        {
            SetSavingStep(SavingWait);
            saveTime = static_cast<s32>(clock->time);
            return 0;
        }

        states.notices = NoticeAutosaveOff;
        FinishSaveDue();
        return 1;
    case SavingSavedShown:
        oleg.Hide(oleg.screens[OLEG::ScreenAutosaving], fade, 0);
        if (oleg.savingShown == 1)
        {
            oleg.Hide(oleg.screens[OLEG::ScreenDimmer], fade, 0);
            StartChunks();
        }

        if (saves->results.flagged != 0)
        {
            SetSavingStep(SavingWait);
            saveTime = static_cast<s32>(clock->time);
            return 0;
        }

        states.notices = NoticeAutosaveFailed;
        FinishSaveDue();
        return 1;
    case SavingLoaded:
        if (flagged)
        {
            states.notices = NoticeAutosaveOn;
            RequestSavedLevel(1);
            return 0;
        }

        if (saves->bits.savingDue != 0)
        {
            states.notices = NoticeAutosaveOff;
        }

        FinishSaveDue();
        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        oleg.Show(oleg.screens[State() == StateMainMenu ? OLEG::ScreenMainMenu : OLEG::ScreenPauseMenu], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        return 0;
    case SavingNewGameLoaded:
        if (flagged)
        {
            RequestNewGame(1);
            return 0;
        }

        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], fade, 0);
        oleg.Show(oleg.screens[OLEG::ScreenMainMenu], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        FinishSaveDue();
        return 0;
    case SavingPaused:
        if (flagged)
        {
            states.notices = NoticeAutosaveOn;
            if (saves->bits.operation == 0)
            {
                StartSaveDue();
            }
        }
        else
        {
            if (saves->bits.savingDue != 0)
            {
                states.notices = NoticeAutosaveOff;
            }

            FinishSaveDue();
        }

        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        oleg.Show(oleg.screens[OLEG::ScreenPauseMenu], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        return 0;
    default:
        return 1;
    }
}

void GameController::FinishSaveDue()
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (saves->bits.savingDue != 0)
    {
        SetSavingStep(SavingNone);
        saves->bits.savingDue = 0;
        return;
    }

    SetSavingStep(SavingNone);
}

void GameController::StartSaveDue()
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (saves->bits.saveDue != 0)
    {
        SetSavingStep(SavingWait);
        saves->bits.savingDue = 1;
        saveTime = 0;
        return;
    }

    FinishSaving();
}

u32 GameController::FinishSaving()
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (saves->bits.savingDue == 0)
    {
        SetSavingStep(SavingNone);
        return 0;
    }

    SetSavingStep(SavingNone);
    saves->bits.savingDue = 0;
    return 1;
}

u32 GameController::RequestNewGame(u32 flagged)
{
    if (State() == StateNewGame)
    {
        return 0;
    }

    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (flagged != 0)
    {
        bool started = false;
        if (saves->bits.operation == 0)
        {
            if (saves->bits.saveDue != 0)
            {
                SetSavingStep(SavingWait);
                saves->bits.savingDue = 1;
                saveTime = 0;
                started = true;
            }
            else
            {
                FinishSaving();
            }
        }

        if (started)
        {
            oleg.savingShown = 0;
            states.notices = NoticeAutosaveOn;
        }
        else
        {
            states.notices = NoticeAutosaveOff;
        }
    }
    else
    {
        states.notices = NoticeAutosaveOff;
        FinishSaveDue();
    }

    SetNextState(StateNewGame);
    return 1;
}

u32 GameController::RequestSavedLevel(u32 flagged)
{
    if (State() == StateLoadingLevel)
    {
        return 0;
    }

    if (flagged != 0)
    {
        StringAssign(&progress.startChunk, saveController.chunk.string);
        auto* saves = static_cast<SaveManager*>(g_SaveManager);
        if (saves->bits.operation == 0)
        {
            StartSaveDue();
        }
    }
    else
    {
        Checkpoint* checkpoint = progress.Reset(EntryNewGame, chunkManager);
        saveController.Take(&progress, chunkManager, checkpoint);
        FinishSaveDue();
    }

    SetNextState(StateLoadingLevel);
    states.entry = EntrySaved;
    return 1;
}

u32 GameController::DisableCharacter(u32 character, u32, u32 unfollow)
{
    InstanceContext* instance = progress.Instance(character);
    if (instance == nullptr)
    {
        return 0;
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    PlayerCharacter* played = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodeCharacter))->character;
    controls->pad = nullptr;
    played->moveInput = {0.0f, 0.0f, 0.0f, 1.0f};
    if (unfollow == 0)
    {
        return 1;
    }

    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, NodeFollow));
    UnregisterNode(follow->owner, 0, follow);
    ReferencedObject* object = follow->cameraInstance != nullptr ? follow->cameraInstance->object : nullptr;
    if (object != nullptr)
    {
        CallVirtual<void>(object, object->vtable, ReferencedObject::SleepSlot);
    }

    return 1;
}

u32 GameController::SwitchCharacter(u32 character, u32 played, u32 unfollow)
{
    InstanceContext* instance = progress.Instance(character);
    if (instance == nullptr)
    {
        return 0;
    }

    u32 before = played != 0 ? progress.play.character : progress.play.second;
    DisableCharacter(before, played, unfollow);
    if (played == 0)
    {
        progress.play.second = character;
        return 1;
    }

    ChunkData* chunk = instance->chunk;
    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, NodeFollow));
    ReferencedObject* followed = follow->cameraInstance != nullptr ? follow->cameraInstance->object : nullptr;
    PlayerCharacter* playerCharacter = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodeCharacter))->character;
    AssignReference(&follow->camera.rig.ownTarget.followed, instance);
    RegisterNode(follow->owner, 0, follow);
    ReferencedObject* object = follow->cameraInstance != nullptr ? follow->cameraInstance->object : nullptr;
    if (object != nullptr)
    {
        CallVirtual<void>(object, object->vtable, ReferencedObject::WakeSlot);
    }

    if (followed != nullptr)
    {
        AssignReference(&view.cameraObject, followed);
        AssignReference(&g_CameraShake.centre, followed);
        MoveToChunk(chunk, followed);
    }

    controls->pad = pad;
    AssignReference(&g_PlayerInstance, instance);
    auto* part = static_cast<CharacterPart*>(playerCharacter->part);
    g_PlayerCharacter2 = playerCharacter;
    g_PlayerPart2 = part;
    g_PlayerCharacter = playerCharacter;
    g_PlayerPart = part;
    if (chunk != nullptr)
    {
        u32 palette = chunk->colourFilterPalette;
        g_ColourFilterOn = 1;
        g_ColourFilterAmount = 1.0f;
        g_ColourFilterSecondPalette = palette;
        g_ColourFilterBlends = 0;
        g_ColourFilterPalette = palette;
    }

    progress.play.character = character;
    return 1;
}

void GameController::EnableCharacters(u32 unfollow)
{
    u32 character = progress.play.character;
    u32 second = progress.play.second;
    if (progress.Instance(character) == nullptr && SwitchToCharacterInFocus() != 0)
    {
        character = progress.play.character;
    }

    SwitchCharacter(character, 1, unfollow);
    SwitchCharacter(second, 0, 0);
}

u32 GameController::SwitchToCharacterInFocus()
{
    ChunkDataReference* focus = G_ChunkLoadingManager_->focusLoader->sm2->data;
    ChunkData* chunk = focus != nullptr ? focus->chunk : nullptr;
    u32 character = progress.play.character;
    for (u32 tried = 0; tried < GameProgress::Characters; tried++)
    {
        // From the one after the one played
        if (tried != 0)
        {
            InstanceContext* instance = progress.Instance(character);
            if (instance != nullptr && instance->chunk == chunk)
            {
                return SwitchCharacter(character, 1, 1);
            }
        }

        character++;
        if (character >= GameProgress::Characters)
        {
            character = 0;
        }
    }

    return 0;
}

void GameController::BeginFrame()
{
    oleg.BeginFrame();
}

void GameController::SetView(u32 playingMovie)
{
    if (playingMovie == 0)
    {
        SetRendererView(renderer, &view);
    }
}

void GameController::EndFrame()
{
    oleg.EndFrame();
}

void GameController::Draw(u32 playingMovie)
{
    if (playingMovie == 0)
    {
        u32 state = State();
        if (state == StateCredits)
        {
            if (credits != nullptr)
            {
                credits->Draw(renderer);
            }
        }
        else if (state == StateTitle && flags.bottomText == 0 && static_cast<u32>(stateFrame + PressStartFrames) < static_cast<u32>(frame))
        {
            const char* text = GameText(PressStartText);
            renderer->font = &font;
            u32 colour;
            GetColor(&colour, ColourWhite);
            Renderer* target = renderer;
            target->colour = colour;
            f32 size = g_BreathingScale.scale * PressStartScale;
            target->textScale.y = size;
            target->textScale.x = size;
            target->textAlignment.value = TextAlignment::Centred;
            QueueText(target, text, PressStartX, PressStartY);
        }

        for (u32 index = 0; index < GameProgress::Characters; index++)
        {
            InstanceContext* instance = progress.Instance(index);
            if (instance != nullptr)
            {
                DrawCharacterOverlay(static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodeCharacter))->character);
            }
        }
    }

    oleg.Draw(renderer);
}

u32 GameController::ReturnToPauseMenu()
{
    if (states.pauseReason == ReasonStart)
    {
        return 0;
    }

    SetNextState(StatePaused);
    states.pauseReason = ReasonNone;
    return 1;
}

u32 GameController::RequestRestart(u32 entry)
{
    if (State() == StateFadingOut)
    {
        return 0;
    }

    SetNextState(StateFadingOut);
    states.entry = entry;
    return 1;
}

u32 GameController::QuitToTitle()
{
    SetNextState(StateRestartingTitle);
    return 1;
}

u32 GameController::ShowBottomText(s32 duration)
{
    if (State() != StatePlaying)
    {
        return 0;
    }

    flags.bottomText = 1;
    StringAssign(&oleg.textLine.text, "");
    oleg.Hide(EveryWidget, duration, 0);
    oleg.Show(oleg.screens[OLEG::ScreenBottomText], duration, 0);
    return 1;
}

u32 GameController::HideBottomText(s32 duration)
{
    if (flags.bottomText == 0)
    {
        return 0;
    }

    flags.bottomText = 0;
    hudDelay = duration;
    oleg.Hide(oleg.screens[OLEG::ScreenBottomText], duration, 0);
    return 1;
}

u32 GameController::StartCutscene(s32 duration)
{
    u32 state = State();
    if (state == StatePlaying)
    {
        SetNextState(StateWatching);
        flags.bottomText = 1;
        flags.unused3 = 0;
        HoldPlayer();
        DisablePlayerControl(0);
        StringAssign(&oleg.textLine.text, "");
        oleg.Hide(EveryWidget, duration, 0);
        oleg.Show(oleg.screens[OLEG::ScreenCutscene], duration, 0);
        FadeToCutsceneVolumes();
        return 1;
    }

    if (state != StateTitle)
    {
        return 0;
    }

    flags.bottomText = 1;
    flags.unused3 = 0;
    oleg.Show(oleg.screens[OLEG::ScreenCutscene], duration, 0);
    return 1;
}

u32 GameController::EndCutscene(s32 duration)
{
    u32 state = State();
    if (state == StateWatching)
    {
        flags.unused3 = 0;
        SetNextState(StateStartingPlay);
        oleg.Hide(oleg.screens[OLEG::ScreenCutscene], duration, 0);
        FadeFromCutsceneVolumes();
        ReleasePlayer(1);
        return 1;
    }

    if (state != StateTitle)
    {
        ReleasePlayer(0);
        FadeFromCutsceneVolumes();
        return 0;
    }

    flags.bottomText = 0;
    oleg.Hide(oleg.screens[OLEG::ScreenCutscene], duration, 0);
    return 1;
}

u32 GameController::EnableBossMode(f32 barLength, u32 health, const u16* icon)
{
    progress.barLength = barLength;
    progress.play.mode = PlayHealth;
    progress.counts.mostHealth = health;
    progress.counts.health = health;
    u16 object = *icon;
    oleg.SetHudIcon(OLEG::HudIconBoss, &object);
    if (State() == StatePlaying)
    {
        oleg.Show(oleg.screens[OLEG::ScreenHealth], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }

    return 1;
}

EABI_EXPORT(FUN_00176e60, &GameController::EnableBossMode);

u32 GameController::ExitBossMode()
{
    if (progress.play.mode != PlayHealth)
    {
        return 0;
    }

    progress.play.mode = PlayNormal;
    oleg.Hide(oleg.screens[OLEG::ScreenHealth], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    return 1;
}

u32 GameController::StartWhackaworm(s32 time, u32 total, const u16* icon)
{
    progress.play.mode = PlayTimed;
    progress.timeLeft = time;
    progress.timeLimit = time;
    progress.counts.countTotal = total;
    progress.counts.count = total;
    u16 object = *icon;
    oleg.SetHudIcon(OLEG::HudIconWhackaworm, &object);
    if (State() == StatePlaying)
    {
        oleg.Show(oleg.screens[OLEG::ScreenTime], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }

    return 1;
}

u32 GameController::EndWhackaworm()
{
    if (progress.play.mode != PlayTimed)
    {
        return 0;
    }

    progress.play.mode = PlayNormal;
    oleg.Hide(oleg.screens[OLEG::ScreenTime], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    return 1;
}

u32 GameController::RestartFromCheckpoint()
{
    if (progress.counts.lives == 0)
    {
        return ForceGameOver();
    }

    if (State() == StateFadingOut)
    {
        return 0;
    }

    SetNextState(StateFadingOut);
    states.entry = EntryCheckpoint;
    return 1;
}

u32 GameController::ForceGameOver()
{
    if (State() == StateGameOver)
    {
        return 0;
    }

    SetNextState(StateGameOver);
    return 1;
}

u32 GameController::PlayCredits()
{
    if (State() == StateCredits)
    {
        return 0;
    }

    SetNextState(StateCredits);
    return 1;
}

u32 GameController::Autosave(Checkpoint* checkpoint)
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    saveController.Take(&progress, chunkManager, checkpoint);
    if (saves->bits.savingDue == 0)
    {
        return 0;
    }

    SetSavingStep(SavingSavedShown);
    oleg.savingShown++;
    oleg.Show(oleg.screens[OLEG::ScreenAutosaving], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    SaveManagerRequest(saves, SaveOperationAutosave, static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    if (oleg.savingShown == 1)
    {
        oleg.Show(oleg.screens[OLEG::ScreenDimmer], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        StopChunks();
    }

    return 1;
}

void GameController::DisablePlayerControl(u32 unfollow)
{
    PlayState play = progress.play;
    DisableCharacter(play.character, 1, unfollow);
    DisableCharacter(play.second, 0, unfollow);
}

void GameController::UnlinkCharacter(InstanceContext* instance)
{
    auto* node = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (node == nullptr)
    {
        return;
    }

    PlayerCharacter* character = node->character;
    CharacterLink* link = character->link;
    if (link == nullptr)
    {
        return;
    }

    // The first of the two unties them
    if (static_cast<CharacterPart*>(character->part)->moveBits.linkedSecond != 0)
    {
        UnlinkCharacters(LinkedCharacter(link));
    }
    else
    {
        UnlinkCharacters(character);
    }
}

u32 GameController::StopSaving()
{
    if (states.savingStep == SavingNone)
    {
        return 0;
    }

    SetNextState(StateWaiting);
    states.notices = NoticeAutosaveOff;
    return FinishSaving();
}

u32 GameController::LoadForNewGame()
{
    states.notices = 0;
    SetSavingStep(SavingNewGameLoaded);
    SaveManagerRequest(static_cast<SaveManager*>(g_SaveManager), SaveOperationNewGameSave,
                       static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    return 1;
}

u32 GameController::LoadSavedGame()
{
    states.notices = 0;
    SetSavingStep(SavingLoaded);
    SaveManagerRequest(static_cast<SaveManager*>(g_SaveManager), SaveOperationLoad,
                       static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    return 1;
}

u32 GameController::SaveFromPause()
{
    states.notices = 0;
    SetSavingStep(SavingPaused);
    oleg.savingShown = 0;
    SaveManagerRequest(static_cast<SaveManager*>(g_SaveManager), SaveOperationPauseSave,
                       static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    return 1;
}

u32 GameController::WaitingForPad(TimeClock*)
{
    if (IsPadRead(pad))
    {
        return StateCheckingCard;
    }

    if (frame == stateFrame)
    {
        oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * ShortFadeSeconds), 0);
        oleg.Show(oleg.screens[OLEG::ScreenNoController], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }

    return NoState;
}

u32 GameController::CheckingCard(TimeClock*)
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    if (frame == stateFrame)
    {
        oleg.Hide(oleg.screens[OLEG::ScreenPicture], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        SaveManagerRequest(saves, SaveOperationCheckRoom, static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    }

    return saves->RunningOperation() == SaveOperationNone ? StateVivendiLogo : NoState;
}

u32 GameController::MainMenu(TimeClock* clock)
{
    if (frame == stateFrame)
    {
        oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        oleg.Show(oleg.screens[OLEG::ScreenMainMenu], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        StopChunks();
        return NoState;
    }

    MenuWidget* menu = oleg.ShownMenu();
    if (menu != nullptr && menu->menuFlags.leaves != 0 && menu->current == &g_ResumePage)
    {
        return StateTitle;
    }

    UpdateSaving(clock);
    return NoState;
}

u32 GameController::Watching(TimeClock* clock)
{
    if (frame == stateFrame)
    {
        StartChunks();
        return NoState;
    }

    if (PauseReason(clock, 0) != 0)
    {
        return StatePaused;
    }

    bool skip = GetButtonState(pad, PadCross, true);
    if (G_VideoController->state == VideoController::StatePlaying && skip)
    {
        G_VideoController->Skip();
    }

    return NoState;
}

u32 GameController::FadingOut(TimeClock*)
{
    if (frame == stateFrame)
    {
        StopChunks();
        u32 hidden;
        u32 shown;
        GetColor(&hidden, ColourTransparentBlack);
        GetColor(&shown, ColourBlack);
        oleg.fader.hiddenColour = hidden;
        oleg.fader.shownColour = shown;
        oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        oleg.Show(oleg.screens[OLEG::ScreenBlack], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        FadeSoundGroups(LongFadeSeconds);
        return NoState;
    }

    return oleg.ScreenWidget(OLEG::ScreenBlack)->State() == Widget::StateShown ? StateRestarting : NoState;
}

u32 GameController::Paused(TimeClock* clock)
{
    if (PauseReason(clock, 1) == 0)
    {
        return NoState;
    }

    MenuWidget* menu = oleg.ShownMenu();
    if (menu == nullptr || menu->menuFlags.leaves == 0 || menu->current != &g_ResumePage)
    {
        return NoState;
    }

    oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    if (states.returnState == StateWatching)
    {
        oleg.Show(oleg.screens[OLEG::ScreenCutscene], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }
    else if (flags.bottomText != 0)
    {
        oleg.Show(oleg.screens[OLEG::ScreenBottomText], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }
    else
    {
        hudDelay = static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds);
    }

    return StateWaiting;
}

u32 GameController::GameOver(TimeClock*)
{
    if (frame != stateFrame)
    {
        ShowPictureScreen(OLEG::ScreenGameOver);
        return NoState;
    }

    u32 picture = 0;
    switch (progress.play.character)
    {
    case CharacterCrash:
        picture = progress.play.second == CharacterCortex ? GameOverCrashAndCortex : GameOverCrash;
        break;
    case CharacterCortex:
        picture = GameOverCortex;
        break;
    case CharacterNina:
        picture = GameOverNina;
        break;
    case CharacterMecha:
        picture = GameOverMecha;
        break;
    default:
        break;
    }

    if (picture != 0)
    {
        oleg.WantPicture(OLEG::PictureTiles, picture);
    }

    StopChunks();
    return NoState;
}

u32 GameController::NewGame(TimeClock*)
{
    if (frame == stateFrame)
    {
        u32 hidden;
        u32 shown;
        GetColor(&hidden, ColourTransparentBlack);
        GetColor(&shown, ColourBlack);
        oleg.fader.hiddenColour = hidden;
        oleg.fader.shownColour = shown;
        oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        oleg.Show(oleg.screens[OLEG::ScreenBlack], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        return NoState;
    }

    if (oleg.ScreenWidget(OLEG::ScreenBlack)->State() != Widget::StateShown)
    {
        return NoState;
    }

    saveController.Restore(0, &progress, chunkManager);
    ResetGame(0);
    RequestMovie(static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), GameMovie::Intro);
    return NextState();
}

u32 GameController::PlayMovie(TimeClock* clock, s32 delay, u32 index, u32 skippable, u8* skipped)
{
    GameMovieController* movies = G_GameMovieController;
    s32 started = movieFrame;
    movieFrame = 0;
    if (movies == nullptr)
    {
        if (skipped != nullptr)
        {
            *skipped = 0;
        }

        return 1;
    }

    if (frame == stateFrame)
    {
        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], delay, 0);
        oleg.Show(oleg.screens[OLEG::ScreenBlack], delay, 0);
        return 0;
    }

    s32 shortFade = static_cast<s32>(g_ClockUnitsPerSecond * ShortFadeSeconds);
    if (delay + shortFade >= static_cast<s32>(clock->time - static_cast<u32>(stateTime)))
    {
        return 0;
    }

    if (started == 0)
    {
        const GameMovie& movie = g_Movies[index];
        // The story's movies have an audio channel for each language
        u32 channel = movie.bits.audioChannels == 1 ? 0 : g_CurrentLanguage;
        movieFrame = frame;
        oleg.Hide(oleg.screens[OLEG::ScreenBlack], shortFade, 0);
        s32 width = movie.bits.width;
        s32 height = movie.bits.height;
        if (movies->Play(movie.file, channel, movie.bits.widescreen, width, height) != 0)
        {
            return 0;
        }
    }
    else
    {
        movieFrame = started;
        if (movies->IsPlaying())
        {
            if (skippable == 0 || !GetButtonState(pad, PadCross, true))
            {
                return 0;
            }

            if (skipped != nullptr)
            {
                *skipped = 1;
            }

            oleg.Show(oleg.screens[OLEG::ScreenBlack], 0, 0);
            movies->RequestStop();
            return 1;
        }
    }

    if (skipped != nullptr)
    {
        *skipped = 0;
    }

    oleg.Show(oleg.screens[OLEG::ScreenBlack], 0, 0);
    return 1;
}

u32 GameController::ShowPictureScreen(u32 screen)
{
    s32 shownSince = movieFrame;
    movieFrame = 0;
    OlegPictureState picture = oleg.pictures[OLEG::PictureTiles];
    bool reading = picture.reading != 0;
    u32 read = picture.read;
    if (reading || read == OLEG::TilesNone)
    {
        if (!reading)
        {
            oleg.LoadPicture(OLEG::PictureTiles, 0);
        }

        return 0;
    }

    s32 longFade = static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds);
    if (picture.wanted == read)
    {
        if (oleg.ScreenWidget(OLEG::ScreenPicture)->State() < Widget::StateAppearing)
        {
            oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], longFade, 0);
            oleg.Show(oleg.screens[OLEG::ScreenBlack], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
            oleg.Show(oleg.screens[screen], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
            return 0;
        }

        if (oleg.ScreenWidget(OLEG::ScreenPicture)->State() != Widget::StateShown)
        {
            return 0;
        }

        movieFrame = shownSince != 0 ? shownSince : frame;
        return 1;
    }

    // Another picture is wanted: this one faded to black and let go first
    if (oleg.ScreenWidget(OLEG::ScreenPicture)->State() == Widget::StateShown)
    {
        oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], longFade, 0);
        oleg.Show(oleg.screens[OLEG::ScreenBlack], static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        return 0;
    }

    if (oleg.ScreenWidget(OLEG::ScreenPicture)->State() < Widget::StateAppearing)
    {
        oleg.ReleasePicture(OLEG::PictureTiles);
    }

    return 0;
}

u32 GameController::LoadingTitle(TimeClock*)
{
    if (frame == stateFrame)
    {
        u32 last = static_cast<u32>(states.lastState);
        StopChunks();
        if (last == StateMovie)
        {
            ResetGame(0);
            return StateTitle;
        }

        StringAssign(&progress.startChunk, g_StartChunkPath.string);
        u32 loading = RandomLoadingScreen();
        oleg.WantPicture(OLEG::PictureTiles, loading);
        return NoState;
    }

    if (ShowPictureScreen(OLEG::ScreenLoading) == 0)
    {
        return NoState;
    }

    if (frame == movieFrame)
    {
        oleg.Reset();
        if (progress.startChunk.length != 0)
        {
            ChunkLoadingManager* loading = G_ChunkLoadingManager_;
            UnloadEverything(loading, true, chunkManager);
            QueueChunk(loading, &progress.startChunk, 0);
        }

        return NoState;
    }

    return progress.ChunkLoaded(flags.queuesFiles != 0 ? GameProgress::LoadedWithLinks : GameProgress::LoadedChunk, 0) != 0
               ? StateTitle
               : NoState;
}

u32 GameController::Title(TimeClock* clock)
{
    if (frame == stateFrame)
    {
        for (u32 character = 0; character < GameProgress::Characters; character++)
        {
            DisableCharacter(character, 0, 1);
        }

        oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), 0);
        Checkpoint* checkpoint = progress.Reset(0, nullptr);
        saveController.Take(&progress, chunkManager, checkpoint);
        if (SwitchCharacter(CharacterNone, 1, 1) == 0)
        {
            EnableCharacters(1);
        }

        StartChunks();
        return NoState;
    }

    if (GetButtonState(pad, PadStart, true))
    {
        return StateMainMenu;
    }

    if (static_cast<s32>(clock->time - static_cast<u32>(stateTime)) < static_cast<s32>(g_ClockUnitsPerSecond * AttractSeconds))
    {
        return NoState;
    }

    RequestRandomMovie(static_cast<s32>(g_ClockUnitsPerSecond * LongFadeSeconds), GameMovie::Attract, GameMovie::Attract);
    return NextState();
}

u32 GameController::LoadingLevel(TimeClock*)
{
    if (frame == stateFrame)
    {
        u32 loading = RandomLoadingScreen();
        oleg.WantPicture(OLEG::PictureTiles, loading);
        return NoState;
    }

    if (ShowPictureScreen(OLEG::ScreenLoading) == 0)
    {
        return NoState;
    }

    if (frame == movieFrame)
    {
        // From a checkpoint the chunk manager keeps its chunks
        ChunkManager* kept = states.entry != EntryCheckpoint ? chunkManager : nullptr;
        oleg.Reset();
        if (progress.startChunk.length != 0)
        {
            ChunkLoadingManager* loading = G_ChunkLoadingManager_;
            UnloadEverything(loading, true, kept);
            QueueChunk(loading, &progress.startChunk, 0);
        }

        progress.Reset(static_cast<u32>(states.entry), chunkManager);
        if (states.entry == EntrySaved)
        {
            saveController.Restore(1, &progress, chunkManager);
        }

        return NoState;
    }

    return progress.ChunkLoaded(flags.queuesFiles != 0 ? GameProgress::LoadedWithLinks : GameProgress::LoadedChunk, 0) != 0
               ? StateStartingPlay
               : NoState;
}

u32 GameController::StartingPlay(TimeClock*)
{
    VideoController* video = G_VideoController;
    u32 last = static_cast<u32>(states.lastState);
    if (frame == stateFrame)
    {
        for (u32 character = 0; character < GameProgress::Characters; character++)
        {
            DisableCharacter(character, 0, 1);
        }

        switch (last)
        {
        case StateWaitingForLoader:
        case StateLoadingLevel:
        {
            progress.Enter(static_cast<u32>(states.entry), &saveController.chunk, &saveController.place, &progress, chunkManager);
            EnableCharacters(1);
            InstanceContext* player = CharacterInstance(&progress);
            auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, NodeFollow));
            follow->camera.rig.Restart();
        }
            [[fallthrough]];
        case StateCredits:
        case StateRestarting:
            StartChunks();
            [[fallthrough]];
        case StateWatching:
        case StateMovie:
            EnableCharacters(1);
            flags.bottomText = 0;
            break;
        default:
            break;
        }

        g_CountedInstances = 0;
        return NoState;
    }

    u32 pairing = progress.play.pairing;
    InstanceContext* player = CharacterInstance(&progress);
    PlayerCharacter* character = static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodeCharacter))->character;
    InstanceContext* second = SecondCharacterInstance(&progress);
    PlayerCharacter* secondCharacter = nullptr;
    if (second != nullptr)
    {
        auto* node = static_cast<PlayerNode*>(GetGameNode(&second->nodes, NodeCharacter));
        if (node != nullptr)
        {
            secondCharacter = node->character;
        }
    }

    if (last != StateWatching && last != StateMovie)
    {
        switch (pairing)
        {
        case PairingHumiliskate:
            SetPlayerVehicle(character, Vehicle::KindHumiliskate, secondCharacter, 0);
            break;
        case PairingRollerbrawl:
            SetPlayerVehicle(character, Vehicle::KindRollerbrawl, secondCharacter, 0);
            break;
        case PairingTied:
        {
            LinkCharacters(character, secondCharacter);
            Reference* argument = second != nullptr ? AddReference(second) : nullptr;
            GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), TiedMessage, &argument,
                                                    1u << NodeObject);
            Reference* queued = event != nullptr ? AddEventReference(event) : nullptr;
            QueueEvent(second, &queued);
            break;
        }
        default:
            break;
        }
    }

    ResumeSound();
    camera.Prepare(player);
    video->cameraTrack.camera = &cutsceneCamera;
    oleg.Hide(EveryWidget, static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    hudDelay = static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds);
    StringAssign(&progress.startChunk, "");
    StringAssign(&progress.unused1C, "");
    StringAssign(&progress.unused28, "");
    return StatePlaying;
}

u32 GameController::Gallery(TimeClock*)
{
    if (frame != stateFrame)
    {
        if (ShowPictureScreen(OLEG::ScreenGallery) == 0)
        {
            return NoState;
        }

        bool back = GetButtonState(pad, PadTriangle, true);
        bool next = GetButtonState(pad, PadCross, true);
        if (!back && !next)
        {
            return NoState;
        }

        u32 shown = flags.galleryShown;
        if (back || shown == flags.galleryLast)
        {
            BackToPauseMenu();
            return StatePaused;
        }

        flags.galleryShown = shown + 1;
    }

    String name;
    name.string = nullptr;
    name.length = 0;
    name.capacity = 0;
    StringAssign(&name, gallery.string);
    u32 shown = flags.galleryShown;
    // The other of the two named pictures, so it's read again
    u32 picture = oleg.pictures[OLEG::PictureTiles].read == OLEG::TilesNamed ? OLEG::TilesNamed + 1 : OLEG::TilesNamed;
    if (shown < TwoDigits)
    {
        StringAppend(&name, "0");
    }

    String number;
    StringConstructNumber(&number, shown);
    StringAppend(&name, number.string);
    StringDestroy(&number);
    StringAssign(&oleg.pictureName, name.string);
    oleg.WantPicture(OLEG::PictureTiles, picture);
    StringDestroy(&name);
    return NoState;
}

u32 GameController::RequestGallery(s32 delay, const char* name, u32 first, u32 last)
{
    if (State() != StatePaused)
    {
        return 0;
    }

    flags.galleryShown = first;
    flags.unused5 = first;
    flags.galleryLast = last;
    movieDelay = delay;
    StringAssign(&gallery, name);
    SetNextState(StateGallery);
    return 1;
}

u32 GameController::Credits(TimeClock* clock)
{
    UpdateSound(0, clock);
    if (frame == stateFrame)
    {
        oleg.WantPicture(OLEG::PictureTiles, OLEG::TilesCredits);
        StopChunks();
        g_CreditsTrack = 0;
        g_CreditsTrackTime = CreditsTrackSeconds;
        return NoState;
    }

    if (ShowPictureScreen(OLEG::ScreenPicture) == 0)
    {
        return NoState;
    }

    if (frame == movieFrame)
    {
        String path;
        StringConstruct(&path, CreditsFolder);
        StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        StringAppend(&path, TextExtension);
        credits = CreditsRoll::Construct(static_cast<CreditsRoll*>(MemoryAllocate(sizeof(CreditsRoll))), &font, path.string);
        StringAssign(&oleg.textLine.text, "");
        oleg.wumpaToAdd = 0;
        oleg.wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * WumpaAddSeconds);
        for (u32 index = OLEG::SpriteHudIcons; index < OLEG::SpriteIcons; index++)
        {
            Sprite& sprite = oleg.sprites[index];
            CallVirtual<void>(&sprite, sprite.vtable, Shape2D::SetMaterialSlot, Platform::Graphics::FlatMaterial());
        }

        StringAssign(&progress.startChunk, g_PostCreditsChunkPath.string);
        if (progress.startChunk.length != 0)
        {
            ChunkLoadingManager* loading = G_ChunkLoadingManager_;
            UnloadEverything(loading, true, nullptr);
            QueueChunk(loading, &progress.startChunk, 0);
        }

        StringDestroy(&path);
        return NoState;
    }

    if (credits->Update(clock) != 0)
    {
        f32 time = g_CreditsTrackTime;
        if (!(CreditsTrackSeconds < time))
        {
            g_CreditsTrackTime = time + static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
        }
        else if (g_CreditsTrack < CreditsTracks)
        {
            // The retail request's bits 21-31 are what the stack held (the music doesn't read them)
            MusicRequest request;
            request.bits.value = 0;
            request.bits.track = CreditsMusic[g_CreditsTrack];
            request.bits.group = MusicGroup;
            request.bits.startsAtOnce = 1;
            request.bits.loops = 1;
            request.right = 1.0f;
            g_CreditsTrackTime = 0.0f;
            request.left = 1.0f;
            request.fadeTime = g_CreditsTrack == 0 ? 0.0f : CreditsFadeSeconds;
            PlayMusicRequest(MainMusicSlot, &request);
            g_CreditsTrack++;
        }

        return NoState;
    }

    if (progress.ChunkLoaded(flags.queuesFiles != 0 ? GameProgress::LoadedWithLinks : GameProgress::LoadedChunk, 0) == 0)
    {
        return NoState;
    }

    if (credits != nullptr)
    {
        credits->Destroy(DestroyAndFree);
    }

    credits = nullptr;
    return StateStartingPlay;
}

u32 GameController::Restarting(TimeClock*)
{
    Checkpoint* checkpoint = progress.Reset(static_cast<u32>(states.entry), chunkManager);
    if (checkpoint != nullptr)
    {
        StringAssign(&progress.startChunk, checkpoint->chunk.string);
    }

    if (states.entry == EntrySaved)
    {
        saveController.summary.lives = GameProgress::StartLives;
        GameRendererController* renderer = G_GameRendererController;
        saveController.options.vibration = static_cast<GamePadController*>(G_GamePadController)->flags.vibration;
        saveController.effectsVolume = GroupVolumeLevel(EffectsGroup);
        saveController.musicVolume = GroupVolumeLevel(MusicGroup);
        saveController.options.musicStereo = g_MusicStereo;
        saveController.options.widescreen = g_WidescreenTv;
        saveController.screenOffset.x = renderer->screenOffset.x;
        saveController.screenOffset.y = renderer->screenOffset.y;
        StringAssign(&progress.startChunk, saveController.chunk.string);
        return StateLoadingLevel;
    }

    bool loading = false;
    if (progress.startChunk.length > 0)
    {
        ChunkLoader* loader = FindChunkLoader(G_ChunkLoadingManager_, &progress.startChunk);
        loading = loader == nullptr || !ChunkLoaderIsLoaded(loader, true);
    }

    if (loading)
    {
        return StateLoadingLevel;
    }

    ResetGame(static_cast<u32>(states.entry));
    return StateStartingPlay;
}

void GameController::BackToPauseMenu()
{
    u32 reason = static_cast<u32>(states.pauseReason);
    oleg.Hide(oleg.screens[OLEG::ScreenAllButOverlays], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    u32 page = reason - ReasonStart;
    if (page < PauseScreenCount && PauseScreens[page] != NoScreen)
    {
        oleg.Show(oleg.screens[PauseScreens[page]], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
    }
}

u32 GameController::PausePage()
{
    u32 reason = static_cast<u32>(states.pauseReason);
    bool back = false;
    bool on = false;
    if (oleg.optionsMenu.State() < Widget::StateAppearing && oleg.screenPositionMenu.State() < Widget::StateAppearing)
    {
        back = GetButtonState(pad, PadL1, true) || GetButtonState(pad, PadL2, true);
        on = GetButtonState(pad, PadR1, true) || GetButtonState(pad, PadR2, true);
    }

    u32 area = progress.play.open;
    if (back)
    {
        switch (reason)
        {
        case ReasonStart:
            return ReasonExtras;
        case ReasonFirstLevels:
            return ReasonStart;
        case ReasonFirstLevels + 1:
        case ReasonFirstLevels + 2:
        case ReasonFirstLevels + 3:
            return reason - 1;
        case ReasonExtras:
            return LastLevelsPage(area);
        default:
            return reason;
        }
    }

    if (on)
    {
        switch (reason)
        {
        case ReasonStart:
            return ReasonFirstLevels;
        case ReasonFirstLevels:
        case ReasonFirstLevels + 1:
        case ReasonFirstLevels + 2:
            return area < LevelsPageAreas[reason - ReasonFirstLevels] ? ReasonExtras : reason + 1;
        case ReasonFirstLevels + 3:
            return ReasonExtras;
        case ReasonExtras:
            return ReasonStart;
        default:
            return reason;
        }
    }

    if (reason == ReasonNone || (reason >= ReasonNoPad && reason < ReasonQuitting))
    {
        return ReasonStart;
    }

    return reason;
}

u32 GameController::PauseReason(TimeClock*, u32 paused)
{
    u32 reason = static_cast<u32>(states.pauseReason);
    if (reason == ReasonQuitting)
    {
        return ReasonQuitting;
    }

    u32 next = ReasonNone;
    u32 saving = static_cast<u32>(states.savingStep);
    if ((states.notices & NoticeAutosaveFailed) != 0)
    {
        states.notices = 0;
        next = ReasonAutosaveFailed;
    }
    else if (State() == StatePaused || State() == StateGallery)
    {
        // A load's or the pause menu's save's result still to come
        if (saving == SavingLoaded || saving == SavingPaused)
        {
            return reason;
        }

        next = reason >= ReasonAutosaveOff && reason < ReasonQuitting ? reason : ReasonNone;
    }
    else if ((states.notices & NoticeAutosaveOff) != 0)
    {
        states.notices = 0;
        next = ReasonAutosaveOff;
    }
    else if ((states.notices & NoticeFourth) != 0)
    {
        states.notices = 0;
        next = ReasonFourthNotice;
    }
    else if ((states.notices & NoticeAutosaveOn) != 0)
    {
        states.notices = 0;
        next = saving != SavingNone ? ReasonAutosaveOn : ReasonNone;
    }
    else if (saving == SavingSavedShown)
    {
        // An autosave's screens are up
        return ReasonNone;
    }

    if (next == ReasonNone)
    {
        if (g_StreamSystem->state != static_cast<s32>(Platform::Stream::State::Running))
        {
            next = ReasonDiscError;
        }
        else if (!IsPadRead(pad))
        {
            next = ReasonNoPad;
        }
        else if (paused != 0)
        {
            next = PausePage();
        }
        else
        {
            next = states.startPressed != 0 ? ReasonStart : ReasonNone;
        }
    }

    states.pauseReason = next;
    if (paused != 0 && next == reason)
    {
        return next;
    }

    if (next == ReasonNone)
    {
        return ReasonNone;
    }

    BackToPauseMenu();
    if (paused != 0)
    {
        return next;
    }

    states.returnState = State();
    StopChunks();
    return next;
}

u32 GameController::Playing(TimeClock* clock)
{
    // The title's character kept asleep
    Reference* reference = progress.characters[CharacterNone];
    ReferencedObject* title = reference != nullptr ? reference->object : nullptr;
    if (title != nullptr)
    {
        CallVirtual<void>(title, title->vtable, ReferencedObject::SleepSlot);
    }

    if (frame == stateFrame)
    {
        StartChunks();
        return NoState;
    }

    if (PauseReason(clock, 0) != 0)
    {
        return StatePaused;
    }

    if (hudDelay == 0 || static_cast<s32>(clock->time - static_cast<u32>(stateTime)) < hudDelay)
    {
        return NoState;
    }

    s32 fade = static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds);
    u32 mode = progress.play.mode;
    if (mode == PlayHealth)
    {
        oleg.Show(oleg.screens[OLEG::ScreenHealth], fade, 0);
    }
    else if (mode == PlayTimed)
    {
        if (flags.bottomText == 0)
        {
            oleg.Show(oleg.screens[OLEG::ScreenTime], fade, 0);
        }
    }
    else
    {
        oleg.Show(oleg.screens[OLEG::ScreenHud], fade, static_cast<s32>(g_ClockUnitsPerSecond * HudShownSeconds));
        oleg.Show(oleg.screens[OLEG::ScreenSlider], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        if (flags.bottomText == 0)
        {
            oleg.Show(oleg.screens[OLEG::ScreenAmmo], static_cast<s32>(g_ClockUnitsPerSecond * FadeSeconds), 0);
        }
    }

    hudDelay = 0;
    return NoState;
}

u32 GameController::RequestMovie(s32 delay, u32 index)
{
    movieDelay = delay;
    movie = index;
    SetNextState(StateMovie);
    return 1;
}

u32 GameController::RequestRandomMovie(s32 delay, u32 first, u32 last)
{
    f32 random = GetRandFloat();
    u32 index = last;
    if (random < 1.0f)
    {
        index = first + static_cast<s32>(static_cast<f32>(static_cast<s32>(last - first + 1)) * random);
    }

    movieDelay = delay;
    movie = index;
    SetNextState(StateMovie);
    return 1;
}

u32 GameController::MovieEnded()
{
    if (State() != StateMovie)
    {
        return 0;
    }

    u32 last = static_cast<u32>(states.lastState);
    if (last == StateTitle)
    {
        SetNextState(StateLoadingTitle);
    }
    else if (last == StatePaused)
    {
        SetNextState(StatePaused);
        BackToPauseMenu();
    }
    else
    {
        SetNextState(StateStartingPlay);
    }

    return 1;
}

void GameController::ResetGame(u32 entry)
{
    VideoController* video = G_VideoController;
    // The instances reset (InstanceFilterWord): of any kind but the cameras', whatever their flags
    const u32 filter[3] = {ResetKinds, 0, 0};
    u32 dropInstances = entry < EntrySaved;
    StopChunks();
    for (ChunkData* chunk = GetChunkList()->first; chunk != nullptr; chunk = chunk->next)
    {
        if (chunk->clocks != nullptr)
        {
            ResetClockSpeeds();
        }
    }

    ResetClockSpeeds();
    progress.Reset(entry, chunkManager);
    oleg.Reset();
    ResetChunkInstances(chunkManager, entry, filter);
    video->Reset();
    StopAllSound();
    ClearDecals(&g_DecalData);
    KillParticles();
    BackgroundWork(false);
    ResetChunks(chunkManager, entry, dropInstances);
    UpdateGlobalInstances();
}

namespace
{
// The way into the game the character part's values are made again for when the player's let go
constexpr u32 PartResetEntry = EntryCheckpoint;

// An instance's place as the retail code reads it, also when there's no instance (the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

InstanceContext* ShownCamera(GameController* controller)
{
    Reference* shown = controller->view.cameraObject;
    return shown != nullptr ? static_cast<InstanceContext*>(shown->object) : nullptr;
}

struct HeldCharacter
{
    ControlsNode* controls;
    CharacterAgent* character;
};

HeldCharacter PlayedCharacter(GameController* controller)
{
    InstanceContext* player = CharacterInstance(&controller->progress);
    auto* controls = static_cast<ControlsNode*>(GetGameNode(&player->nodes, NodeControls));
    auto* node = static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodeCharacter));
    return {controls, reinterpret_cast<CharacterAgent*>(node->character)};
}

CharacterPart* PartOf(CharacterAgent* character)
{
    return static_cast<CharacterPart*>(character->part);
}
}

InstanceContext* ShowCamera(GameController* controller, u32 camera, u32 reset)
{
    controller->states.camera = camera;
    switch (camera)
    {
    case GameController::CameraFollow:
    {
        InstanceContext* player = CharacterInstance(&controller->progress);
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, NodeFollow));
        auto* followCamera =
            follow->cameraInstance != nullptr ? static_cast<InstanceContext*>(follow->cameraInstance->object) : nullptr;
        ShowFollowCamera(&follow->camera, followCamera, player, reset);
        // Unchecked: the follow node always has a camera once it's given its instance
        followCamera->flags.movesBetweenChunks = 0;
        AssignReference(&controller->view.cameraObject, followCamera);
        return followCamera;
    }
    case GameController::CameraGameRig:
    {
        InstanceContext* shown = ShownCamera(controller);
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&shown->nodes, NodeCameraLens));
        // The player's place (without a player played, the word at address 8 taken for it)
        ObjectPlace* place = RetailPlaceOf(CharacterInstance(&controller->progress));
        place->SyncPosition();
        Vector4 position = place->position;
        lens->SetRig(&controller->camera, reset);
        controller->camera.ownTarget.end = position;
        return shown;
    }
    case GameController::CameraCutsceneRig:
    {
        InstanceContext* shown = ShownCamera(controller);
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&shown->nodes, NodeCameraLens));
        // The cutscenes' rig without followers or a target, taking nothing from the camera trigger, not smoothed
        CutsceneCameraRig& rig = controller->cutsceneCamera;
        rig.DropCameraFollower();
        rig.cameraFollower = nullptr;
        rig.bits.ownsCameraFollower = 0;
        rig.bits.ignoresTrigger = 1;
        rig.bits.smoothed = 0;
        rig.DropTargetFollower();
        rig.targetFollower = nullptr;
        rig.bits.ownsTargetFollower = 0;
        rig.DropTarget();
        rig.target = nullptr;
        rig.bits.ownsTarget = 0;
        lens->SetRig(&rig, reset);
        return shown;
    }
    default:
        return nullptr;
    }
}

void BlendToCamera(GameController* controller, u32 camera, const s32* ticks, u32 reset, u32 curve)
{
    u8 blendCurve = static_cast<u8>(curve);
    if (camera == GameController::CameraFollow)
    {
        InstanceContext* player = CharacterInstance(&controller->progress);
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, NodeFollow));
        auto* followCamera =
            follow->cameraInstance != nullptr ? static_cast<InstanceContext*>(follow->cameraInstance->object) : nullptr;
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&followCamera->nodes, NodeCameraLens));
        CameraRig* rig = &follow->camera.rig;
        if (reset != 0)
        {
            rig->ResetVirtual(followCamera);
        }

        lens->BlendTo(rig, *ticks, blendCurve);
    }
    else if (camera == GameController::CameraGameRig)
    {
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&ShownCamera(controller)->nodes, NodeCameraLens));
        lens->BlendTo(&controller->camera, *ticks, blendCurve);
    }
}

void GameController::HoldPlayer()
{
    HeldCharacter held = PlayedCharacter(this);
    CharacterAgent* character = held.character;
    character->eventFlags.eventsOff = 1;
    held.controls->bits.motionDriven = 1;
    // What the reset loses of the part kept: being tied to the other character (only the leader isn't set back) and the hit
    // points
    CharacterPart* part = PartOf(character);
    bool leader = part->moveBits.linkedFirst != 0;
    bool second = part->moveBits.linkedSecond != 0;
    u32 hitPoints = part->flags.hitPoints;
    if (!leader)
    {
        character->Reset();
    }

    if (second)
    {
        PartOf(character)->moveBits.linkedSecond = 1;
    }
    else if (leader)
    {
        PartOf(character)->moveBits.linkedFirst = 1;
    }

    part = PartOf(character);
    part->flags.hitPoints = hitPoints;
}

void GameController::ReleasePlayer(u32 resume)
{
    HeldCharacter held = PlayedCharacter(this);
    CharacterAgent* character = held.character;
    character->eventFlags.eventsOff = 0;
    held.controls->bits.motionDriven = 0;
    CharacterPart* part = PartOf(character);
    bool leader = part->moveBits.linkedFirst != 0;
    u32 hitPoints = part->flags.hitPoints;
    if (!leader && resume != 0)
    {
        CallVirtual<void>(part, part->vtable, AgentPart::ResetSlot, PartResetEntry);
        character->Reset();
    }

    part = PartOf(character);
    part->flags.hitPoints = hitPoints;
    // Every character but the title's (none) loses its solid model when the player's let go
    for (u32 index = 0; index < GameProgress::Characters; index++)
    {
        if (index == CharacterNone)
        {
            continue;
        }

        InstanceContext* instance = progress.Instance(index);
        if (instance != nullptr)
        {
            instance->flags.solidModel = 0;
        }
    }
}

u32 CrateGivesSecondContents(void*)
{
    return 0;
}

s32 CrateContentsCount(void*, u32 least, u32 most)
{
    return RandomFrom(least, most - least);
}
