#include "game/gamecontroller.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/camerarig.h"
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
#include "game/widgets.h"
#include "platform/graphics.h"
#include "platform/stream.h"
#include "retail/libc.h"

#include <cstddef>
#include <cstdint>

namespace
{
// The screens OLEG shows: the HUD's wumpa fruit and lives (the triangle button shows them while playing), the tiles' picture,
// every screen but the first three
constexpr u32 HudScreen = 0x20;
constexpr u32 PictureScreen = 0x29;
constexpr u32 AllButFirstScreens = 0x2B;
// The tiles' picture: OLEG's third, none when its picture is 12, the legal screen 0; the second sprite's the Crash title
constexpr u32 TilesPicture = 2;
constexpr u32 NoTilesPicture = 12;
constexpr u32 LegalPicture = 0;
constexpr u32 TitlePicture = 0;
constexpr u32 CrashTitle = 0;
// OLEG's sprites 6 to 45: the icons (StartUp\Icons.psm), read through the sprites' vtable function
constexpr u32 FirstIcon = 6;
constexpr u32 EndIcons = 46;
constexpr u32 ShapeReadSlot = 9;
// A destructor's flags: a member destroyed
constexpr u32 Member = 2;
// The game controller's flags: "RB" (the front end's sounds read), OLEG's menus left alone (the context's bit 7)
constexpr u32 FlagRb = 2;
constexpr u32 FlagNoMenus = 4;
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
// The screens of the main menu, a missing controller, the fader (black) and the memory card's check
constexpr u32 MainMenuScreen = 0xB;
constexpr u32 BlackScreen = 0x28;
// The HUD's screens for the play modes: the health bar and lives, the time, the slider, the counter at the bottom right
constexpr u32 HealthScreen = 0x22;
constexpr u32 TimeScreen = 0x21;
constexpr u32 SliderScreen = 0x23;
constexpr u32 CounterScreen = 0x27;
constexpr u32 ModeHealth = 1;
// The instances' vtable function the playing state steps the fifth character's with
constexpr u32 InstanceStepSlot = 3;
constexpr u32 FifthCharacter = 4;
// The screens of a cutscene (its bars and text) and of the bottom text, which play goes back to, and the game over screen
constexpr u32 WatchingScreen = 0;
constexpr u32 BottomTextScreen = 2;
// The game controller's flags: the bottom text's screen up, and bit 3 (cleared when cutscenes start and end)
constexpr u32 FlagBottomText = 1;
constexpr u32 FlagCutsceneCleared = 8;
// The HUD's icons: the boss's, and whack-a-worm's
constexpr u32 BossIcon = 0;
constexpr u32 WhackawormIcon = 1;
// The save code's operation for an autosave
constexpr u32 SaveAutosave = 6;
constexpr u32 GameOverScreen = 0x1B;
// The intro movie and the attract movie
constexpr u32 IntroMovie = 1;
constexpr u32 AttractMovie = 13;
// The loading screens (the tiles' pictures 1 to 3) and their screen
constexpr u32 LoadingScreens = 3;
constexpr u32 LoadingScreen = 0x2A;
// The characters, and the one the title plays
constexpr u32 Characters = 6;
constexpr u32 TitleCharacter = 4;
// The game over pictures (the tiles' pictures): Cortex, Crash, Crash and Cortex, Mecha-Bandicoot, Nina
constexpr u32 GameOverCortex = 4;
constexpr u32 GameOverCrash = 5;
constexpr u32 GameOverCrashAndCortex = 6;
constexpr u32 GameOverMecha = 7;
constexpr u32 GameOverNina = 8;
constexpr u32 NoControllerScreen = 15;
constexpr u32 CardCheckScreen = 0x29;
// The save code's operations the game controller asks for: the memory card checked, the game saved, a new game's, the game
// saved from the pause menu, a load
constexpr u32 SaveCheckCard = 1;
constexpr u32 SaveSave = 2;
constexpr u32 SaveNewGame = 3;
constexpr u32 SavePauseSave = 4;
constexpr u32 SaveLoad = 5;
// The saving's steps (the state word's bits 60-63): none, waiting to save, then after a save, after a save its screens showed,
// after a load, after a new game's load, after the pause menu's
constexpr u32 SavingNone = 0;
constexpr u32 SavingWait = 1;
constexpr u32 SavingSaved = 2;
constexpr u32 SavingSavedShown = 3;
constexpr u32 SavingLoaded = 4;
constexpr u32 SavingNewGameLoaded = 5;
constexpr u32 SavingPaused = 6;
// The screens the saving shows and hides: the pause menu, and the saving's
constexpr u32 PauseMenuScreen = 12;
constexpr u32 SavingScreen = 31;
constexpr u32 SavingScreen2 = 3;
// The movies the start-up plays: the Vivendi logo, Traveller's Tales' ident
constexpr u32 VivendiLogo = 19;
constexpr u32 TravellersTalesIdent = 0;
// How a level is entered (the state word's bits 23-26): from the save controller's chunk and place (a save loaded, or the
// start chunk given on the command line)
constexpr u64 EntryMask = 0x7800000;
constexpr u64 EntrySaved = 0x1000000;
constexpr u64 EntryKeepChunks = 0x1800000;
constexpr u32 EntryShift = 23;
// The screens of the pause reasons 1 to 13 (none for quitting): the pause menu, no controller, the disc error, the notices,
// the levels pages and the extras
constexpr u32 PauseScreenCount = 13;
constexpr s32 PauseScreens[PauseScreenCount] = {12, 15, 16, 17, 18, 19, 20, -1, 21, 22, 23, 24, 25};
// The last areas of the levels pages but the last (the story's areas open: bits 26-30 of the progress), and the last page with
// an area open
constexpr u32 LevelsPageAreas[3] = {6, 13, 20};

u32 LastLevelsPage(u32 area)
{
    if (area < LevelsPageAreas[0])
    {
        return 9;
    }

    if (area < LevelsPageAreas[1])
    {
        return 10;
    }

    return area < LevelsPageAreas[2] ? 11 : 12;
}

// A new game's lives, and the volume groups of the sound effects and the music
constexpr u32 StartLives = 5;
constexpr s32 EffectsGroup = 0;
constexpr s32 MusicGroup = 2;
// The front end's sounds, the save's data's size, and a bit of the state word the constructor sets
constexpr u32 FrontEndSounds = 3;
constexpr u64 ConstructedBit = 0x10000;
// The title's "press start": its text, and the frames the title waits before it
constexpr u32 PressStartText = 0x46;
constexpr s32 PressStartFrames = 50;
// The credits: their picture (the tiles' 11), folder, music tracks (a new one 19 seconds after the one before: track, group 2,
// started at once, looping) and the sprites (the fourth to the sixth) drawn flat again
constexpr u32 CreditsPicture = 11;
constexpr const char* CreditsFolder = "Language\\Credits\\";
constexpr u32 CreditsTracks = 11;
constexpr u16 CreditsMusic[CreditsTracks] = {0x3A, 0x1C, 0x88, 0x1E, 0x23, 0x25, 0x29, 0x36, 0x3C, 0x3D, 0x1B};
constexpr u32 CreditsTrackBits = 0x1A0000;
constexpr f32 CreditsTrackSeconds = 19.0f;
constexpr u32 FirstHudSprite = 3;
constexpr u32 EndHudSprites = 6;
constexpr u32 SpriteSetMaterialSlot = 2;
// The gallery: its screen, the controller's flags' bits of its first picture, its last and the one shown, and the first of
// the two pictures named by OLEG's text
constexpr u32 GalleryScreen = 0x1A;
constexpr u32 GalleryFirstShift = 5;
constexpr u32 GalleryLastShift = 13;
constexpr u32 GalleryShownShift = 21;
constexpr u32 GalleryPictureMask = 0xFF;
constexpr u32 NamedPicture = 9;
// The referenced objects' vtable functions what a follow node follows is told with: it's followed again, it isn't any more
constexpr u32 FollowStartedSlot = 2;
constexpr u32 FollowStoppedSlot = 3;
// How the second character is paired with the first (the progress's bits 4-7): its vehicle of kind 3 or 1, or the two tied
// together (the second sent event 0x39)
constexpr u32 PairingVehicle3 = 2;
constexpr u32 PairingVehicle1 = 3;
constexpr u32 PairingLinked = 4;
constexpr u32 LinkedEvent = 0x39;
// The play modes (the progress's bits 0-3): normal, and timed
constexpr u32 ModeNormal = 0;
constexpr u32 ModeTimed = 2;

// One of the loading screens, at random
u32 RandomLoadingScreen()
{
    f32 random = GetRandFloat();
    u32 loading = LoadingScreens;
    if (random < 1.0f)
    {
        loading = static_cast<s32>(random * 3.0f) + 1;
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
            TimeClocksStop(static_cast<TimeClock*>(chunk->clocks));
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
            TimeClocksStart(static_cast<TimeClock*>(chunk->clocks));
        }
    }

    ResumeGame();
}

// The played character's instance, and the second character's
InstanceContext* CharacterInstance(GameProgress* progress)
{
    return progress->Instance(progress->Field(GameProgress::CharacterShift));
}

InstanceContext* SecondCharacterInstance(GameProgress* progress)
{
    return progress->Instance(progress->Field(GameProgress::SecondShift));
}
}

GameController* GameController::Construct(GameController* controller, GamePad* pad, s32 unknown40, Renderer* renderer,
                                          ChunkManager* chunks, void* resourceManager, GameResources* resources)
{
    controller->pad = pad;
    controller->renderer = renderer;
    controller->chunkManager = chunks;
    controller->resourceManager = resourceManager;
    controller->resources = resources;
    controller->unknown40 = unknown40;
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
    save.place = 0xFFFF;
    RetailLibc::MemorySet(&save, 0, 8);
    save.timePlayed = 0;
    save.data = static_cast<u8*>(MemoryAllocate2(SaveController::DataSize));
    save.progress = &controller->progress;
    OLEG::Construct(&controller->oleg, &controller->font, &controller->progress);
    controller->credits = nullptr;
    RetailLibc::MemorySet(&controller->states, 0, 0xC);
    controller->states = u64{NoState} << NextShift;
    controller->hudDelay = 0;
    controller->frame = 0;
    controller->movieFrame = 0;
    controller->stateFrame = 0;
    controller->saveTime = 0;
    g_MainGamePad = controller->pad;
    MakeFlatBoxHull();
    InitFreedMemory();
    controller->renderer->view = &controller->view;
    controller->states |= ConstructedBit;
    return controller;
}

u32 GameController::Update(u32 keepFreed)
{
    bool start = GetButtonState(pad, PadStart, true);
    states = (states & ~u64{StartPressed}) | static_cast<u64>(start) << 31;
    frame++;
    UpdateSaving(&g_GlobalClock);
    if (NextState() != NoState)
    {
        stateFrame = frame;
        u64 bits = (states & ~(u64{StateMask} << LastShift)) | static_cast<u64>(State()) << LastShift;
        bits = (bits & ~(u64{StateMask} << CurrentShift)) | (bits >> NextShift & StateMask) << CurrentShift;
        states = (bits & ~(u64{StateMask} << NextShift)) | u64{NoState} << NextShift;
        stateTime = g_GlobalClock.time;
    }

    u32 next = NoState;
    switch (State())
    {
    case StateWaitingForLoader:
        if ((G_ChunkLoadingManager_->bits >> 24 & 0xF) == 1)
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
        next = PlayMovie(&g_GlobalClock, static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), VivendiLogo, 1, nullptr) != 0
                   ? StateTravellersTalesLogo
                   : NoState;
        break;
    case StateTravellersTalesLogo:
        if (PlayMovie(&g_GlobalClock, 0, TravellersTalesIdent, 1, nullptr) == 0)
        {
            break;
        }

        if (progress.startChunk.length > 0)
        {
            states = (states & ~EntryMask) | EntrySaved;
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
        if (static_cast<s32>(g_GlobalClock.time - static_cast<u32>(stateTime)) >= static_cast<s32>(g_ClockUnitsPerSecond * 0.25f))
        {
            next = static_cast<u32>(states >> ReturnShift & StateMask);
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
        oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        u32 hidden;
        u32 shown;
        GetColor(&hidden, 0);
        GetColor(&shown, 8);
        oleg.sprite13B8.hiddenColour = hidden;
        oleg.sprite13B8.shownColour = shown;
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
    auto* node = player != nullptr ? static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodePlayer)) : nullptr;
    PlayerCharacter* character = node != nullptr ? node->character : nullptr;
    if (State() == StatePlaying)
    {
        u32 mode = progress.bits & 0xF;
        if (mode == ModeTimed)
        {
            progress.timeLeft -= G_GameClockController->clocks[1].advance;
        }

        if (mode == ModeNormal && GetButtonState(pad, PadTriangle, false))
        {
            oleg.Show(oleg.masks[HudScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f),
                      static_cast<s32>(g_ClockUnitsPerSecond * 3.0f));
        }
    }

    oleg.Update(&g_GlobalClock, pad, character);
    if (State() != StatePlaying && State() != StateWatching)
    {
        return 0;
    }

    StepLoopFrame(&g_GlobalClock);
    if (player != nullptr)
    {
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, Node16));
        SetSoundListener(follow->object != nullptr ? follow->object->object : nullptr);
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
        oleg.WantPicture(TilesPicture, LegalPicture);
        return NoState;
    }

    if (ShowPictureScreen(PictureScreen) == 0)
    {
        return NoState;
    }

    if (frame != movieFrame)
    {
        return StateWaitingForPad;
    }

    String voices;
    StringConstruct(&voices, SoundFolder);
    G_UnkStruct_5C0 = SaveManager::Construct(static_cast<SaveManager*>(MemoryAllocate(sizeof(SaveManager))), this);
    LoadTexts();
    StringAppend(&voices, g_LanguageNames[g_CurrentLanguage]);
    SetMusicBank(MusicBank);
    SetVoiceBank(voices.string);
    String icons;
    StringConstruct(&icons, IconsFile);
    MemoryStream stream;
    MemoryStream::ConstructFromFile(&stream, icons.string, false);
    for (u32 icon = FirstIcon; icon < EndIcons; icon++)
    {
        Sprite& sprite = oleg.sprites[icon];
        CallVirtual<void>(&sprite, sprite.vtable, ShapeReadSlot, &stream);
    }

    stream.Destroy(Member);
    StringDestroy(&icons);
    LoadGlobalResources();
    oleg.WantPicture(TitlePicture, CrashTitle);
    oleg.LoadPicture(TitlePicture, 0);
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
    sounds.unknown08 = 0;
    font.Read(FontFile);
    renderer->font = &font;
    font.width = g_FontSize.x;
    font.height = g_FontSize.y;
    if ((flags & FlagRb) != 0)
    {
        AddResourcePackageToLoadQueue(&sounds, FrontEndFile, 0);
    }

    LoadQueuedSectionsIntoMemory_();
    if ((flags & FlagNoMenus) == 0)
    {
        oleg.StartUp(pad, &font, &font, &frontEndSounds);
    }

    chunkManager->LoadDefault(DefaultChunk);
    sounds.vtable = g_ItemInterfaceVTable;
    return 1;
}

u32 GameController::UpdateSaving(TimeClock* clock)
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if (saves == nullptr)
    {
        return 0;
    }

    u32 step = static_cast<u32>(states >> SavingShift);
    if (step == SavingWait)
    {
        if (saveTime == 0)
        {
            saveTime = static_cast<s32>(clock->time);
        }
        else if (static_cast<s32>(clock->time - static_cast<u32>(saveTime)) >= static_cast<s32>(g_ClockUnitsPerSecond * 0.25f))
        {
            SaveManagerRequest(saves, SaveSave, 0);
            SetSavingStep(SavingSaved);
        }

        return 0;
    }

    if ((saves->bits & SaveCode::OperationMask) != 0)
    {
        return 0;
    }

    s32 quarter = static_cast<s32>(g_ClockUnitsPerSecond * 0.25f);
    bool flagged = (saves->results & SaveCode::Flagged) != 0;
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

        states = (states & ~u64{Notices}) | Notice4;
        FinishSaveDue();
        return 1;
    case SavingSavedShown:
        oleg.Hide(oleg.masks[SavingScreen], quarter, 0);
        if (oleg.savingShown == 1)
        {
            oleg.Hide(oleg.masks[SavingScreen2], quarter, 0);
            StartChunks();
        }

        if ((saves->results & SaveCode::Flagged) != 0)
        {
            SetSavingStep(SavingWait);
            saveTime = static_cast<s32>(clock->time);
            return 0;
        }

        states = (states & ~u64{Notices}) | Notice6;
        FinishSaveDue();
        return 1;
    case SavingLoaded:
        if (flagged)
        {
            states = (states & ~u64{Notices}) | Notice5;
            RequestSavedLevel(1);
            return 0;
        }

        if ((saves->bits & SaveCode::SavingDue) != 0)
        {
            states = (states & ~u64{Notices}) | Notice4;
        }

        FinishSaveDue();
        oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        oleg.Show(oleg.masks[State() == StateMainMenu ? MainMenuScreen : PauseMenuScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        return 0;
    case SavingNewGameLoaded:
        if (flagged)
        {
            RequestNewGame(1);
            return 0;
        }

        oleg.Hide(oleg.masks[AllButFirstScreens], quarter, 0);
        oleg.Show(oleg.masks[MainMenuScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        FinishSaveDue();
        return 0;
    case SavingPaused:
        if (flagged)
        {
            states = (states & ~u64{Notices}) | Notice5;
            if ((saves->bits & SaveCode::OperationMask) == 0)
            {
                StartSaveDue();
            }
        }
        else
        {
            if ((saves->bits & SaveCode::SavingDue) != 0)
            {
                states = (states & ~u64{Notices}) | Notice4;
            }

            FinishSaveDue();
        }

        oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        oleg.Show(oleg.masks[PauseMenuScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        return 0;
    default:
        return 1;
    }
}

void GameController::FinishSaveDue()
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if ((saves->bits & SaveCode::SavingDue) != 0)
    {
        SetSavingStep(SavingNone);
        saves->bits &= ~SaveCode::SavingDue;
        return;
    }

    SetSavingStep(SavingNone);
}

void GameController::StartSaveDue()
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if ((saves->bits & SaveCode::SaveDue) != 0)
    {
        SetSavingStep(SavingWait);
        saves->bits |= SaveCode::SavingDue;
        saveTime = 0;
        return;
    }

    FinishSaving();
}

u32 GameController::FinishSaving()
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if ((saves->bits & SaveCode::SavingDue) == 0)
    {
        SetSavingStep(SavingNone);
        return 0;
    }

    SetSavingStep(SavingNone);
    saves->bits &= ~SaveCode::SavingDue;
    return 1;
}

u32 GameController::RequestNewGame(u32 flagged)
{
    if (State() == StateNewGame)
    {
        return 0;
    }

    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if (flagged != 0)
    {
        bool started = false;
        if ((saves->bits & SaveCode::OperationMask) == 0)
        {
            if ((saves->bits & SaveCode::SaveDue) != 0)
            {
                SetSavingStep(SavingWait);
                saves->bits |= SaveCode::SavingDue;
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
            states = (states & ~u64{Notices}) | Notice5;
        }
        else
        {
            states = (states & ~u64{Notices}) | Notice4;
        }
    }
    else
    {
        states = (states & ~u64{Notices}) | Notice4;
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
        auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
        if ((saves->bits & SaveCode::OperationMask) == 0)
        {
            StartSaveDue();
        }
    }
    else
    {
        Checkpoint* checkpoint = progress.Reset(0, chunkManager);
        saveController.Take(&progress, chunkManager, checkpoint);
        FinishSaveDue();
    }

    SetNextState(StateLoadingLevel);
    states = (states & ~EntryMask) | EntrySaved;
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
    PlayerCharacter* played = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodePlayer))->character;
    controls->pad = nullptr;
    played->input = {0.0f, 0.0f, 0.0f, 1.0f};
    if (unfollow == 0)
    {
        return 1;
    }

    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, Node16));
    UnregisterNode(follow->owner, 0, follow);
    ReferencedObject* object = follow->object != nullptr ? follow->object->object : nullptr;
    if (object != nullptr)
    {
        CallVirtual<void>(object, object->vtable, FollowStoppedSlot);
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

    u32 before = played != 0 ? progress.bits >> 8 & 0xF : progress.bits >> 12 & 0xF;
    DisableCharacter(before, played, unfollow);
    if (played == 0)
    {
        progress.bits = (progress.bits & ~0xF000u) | (character & 0xF) << 12;
        return 1;
    }

    ChunkData* chunk = instance->chunk;
    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, Node16));
    ReferencedObject* followed = follow->object != nullptr ? follow->object->object : nullptr;
    PlayerCharacter* playerCharacter = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodePlayer))->character;
    AssignReference(&follow->camera.rig.ownTarget.followed, instance);
    RegisterNode(follow->owner, 0, follow);
    ReferencedObject* object = follow->object != nullptr ? follow->object->object : nullptr;
    if (object != nullptr)
    {
        CallVirtual<void>(object, object->vtable, FollowStartedSlot);
    }

    if (followed != nullptr)
    {
        AssignReference(&view.cameraObject, followed);
        AssignReference(&g_CameraShake.centre, followed);
        MoveToChunk(chunk, followed);
    }

    controls->pad = pad;
    AssignReference(&g_PlayerInstance, instance);
    void* data = playerCharacter->data;
    g_PlayerCharacter2 = playerCharacter;
    g_PlayerCharacterData2 = data;
    g_PlayerCharacter = playerCharacter;
    g_PlayerCharacterData = data;
    if (chunk != nullptr)
    {
        u32 palette = chunk->colourFilterPalette;
        g_ColourFilterOn = 1;
        g_ColourFilterAmount = 1.0f;
        g_ColourFilterSecondPalette = palette;
        g_ColourFilterBlends = 0;
        g_ColourFilterPalette = palette;
    }

    progress.bits = (progress.bits & ~0xF00u) | (character & 0xF) << 8;
    return 1;
}

void GameController::EnableCharacters(u32 unfollow)
{
    u32 character = progress.bits >> 8 & 0xF;
    u32 second = progress.bits >> 12 & 0xF;
    if (progress.Instance(character) == nullptr && SwitchToCharacterInFocus() != 0)
    {
        character = progress.bits >> 8 & 0xF;
    }

    SwitchCharacter(character, 1, unfollow);
    SwitchCharacter(second, 0, 0);
}

u32 GameController::SwitchToCharacterInFocus()
{
    ChunkDataReference* focus = G_ChunkLoadingManager_->focusLoader->sm2->data;
    ChunkData* chunk = focus != nullptr ? focus->data : nullptr;
    u32 character = progress.bits >> 8 & 0xF;
    for (u32 tried = 0; tried < Characters; tried++)
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
        if (character >= Characters)
        {
            character = 0;
        }
    }

    return 0;
}

void GameController::BeginFrame()
{
    oleg.Unknown3();
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
    oleg.Unknown6();
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
        else if (state == StateTitle && (flags & 1) == 0 && static_cast<u32>(stateFrame + PressStartFrames) < static_cast<u32>(frame))
        {
            const char* text = GameText(PressStartText);
            renderer->font = &font;
            u32 colour;
            GetColor(&colour, 0xF);
            Renderer* target = renderer;
            target->colour = colour;
            f32 size = g_OlegScaler.scale * 0.75f;
            target->textScale.y = size;
            target->textScale.x = size;
            target->textFlags = 0x22;
            QueueText(target, text, 0.5f, Rounded(0.9));
        }

        for (u32 index = 0; index < Characters; index++)
        {
            InstanceContext* instance = progress.Instance(index);
            if (instance != nullptr)
            {
                DrawCharacterOverlay(static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodePlayer))->character);
            }
        }
    }

    oleg.Draw(renderer);
}

u32 GameController::ReturnToPauseMenu()
{
    if ((states >> PauseReasonShift & PauseReasonMask) == ReasonStart)
    {
        return 0;
    }

    SetNextState(StatePaused);
    states &= ~(u64{PauseReasonMask} << PauseReasonShift);
    return 1;
}

u32 GameController::RequestRestart(u32 entry)
{
    if (State() == StateFadingOut)
    {
        return 0;
    }

    SetNextState(StateFadingOut);
    states = (states & ~EntryMask) | static_cast<u64>(entry & 0xF) << EntryShift;
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

    flags |= FlagBottomText;
    StringAssign(&oleg.textLine.text, "");
    oleg.Hide(~u64{0}, duration, 0);
    oleg.Show(oleg.masks[BottomTextScreen], duration, 0);
    return 1;
}

u32 GameController::HideBottomText(s32 duration)
{
    if ((flags & FlagBottomText) == 0)
    {
        return 0;
    }

    flags &= ~FlagBottomText;
    hudDelay = duration;
    oleg.Hide(oleg.masks[BottomTextScreen], duration, 0);
    return 1;
}

u32 GameController::StartCutscene(s32 duration)
{
    u32 state = State();
    if (state == StatePlaying)
    {
        SetNextState(StateWatching);
        flags = (flags | FlagBottomText) & ~FlagCutsceneCleared;
        HoldPlayer();
        DisablePlayerControl(0);
        StringAssign(&oleg.textLine.text, "");
        oleg.Hide(~u64{0}, duration, 0);
        oleg.Show(oleg.masks[WatchingScreen], duration, 0);
        FadeToCutsceneVolumes();
        return 1;
    }

    if (state != StateTitle)
    {
        return 0;
    }

    flags = (flags | FlagBottomText) & ~FlagCutsceneCleared;
    oleg.Show(oleg.masks[WatchingScreen], duration, 0);
    return 1;
}

u32 GameController::EndCutscene(s32 duration)
{
    u32 state = State();
    if (state == StateWatching)
    {
        flags &= ~FlagCutsceneCleared;
        SetNextState(StateStartingPlay);
        oleg.Hide(oleg.masks[WatchingScreen], duration, 0);
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

    flags &= ~FlagBottomText;
    oleg.Hide(oleg.masks[WatchingScreen], duration, 0);
    return 1;
}

u32 GameController::EnableBossMode(f32 barLength, u32 health, const u16* icon)
{
    progress.barLength = barLength;
    progress.bits = (progress.bits & ~GameProgress::ModeMask) | ModeHealth;
    u32 most = health & GameProgress::HealthMask;
    progress.counts = (progress.counts & ~(GameProgress::HealthMask << GameProgress::MostHealthShift)) | most << GameProgress::MostHealthShift;
    progress.counts = (progress.counts & ~(GameProgress::HealthMask << GameProgress::HealthShift)) | most << GameProgress::HealthShift;
    u16 object = *icon;
    oleg.SetHudIcon(BossIcon, &object);
    if (State() == StatePlaying)
    {
        oleg.Show(oleg.masks[HealthScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }

    return 1;
}

EABI_EXPORT(FUN_00176e60, &GameController::EnableBossMode);

u32 GameController::ExitBossMode()
{
    if ((progress.bits & GameProgress::ModeMask) != ModeHealth)
    {
        return 0;
    }

    progress.bits &= ~GameProgress::ModeMask;
    oleg.Hide(oleg.masks[HealthScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    return 1;
}

u32 GameController::StartWhackaworm(s32 time, u32 total, const u16* icon)
{
    progress.bits = (progress.bits & ~GameProgress::ModeMask) | ModeTimed;
    progress.timeLeft = time;
    progress.timeLimit = time;
    progress.counts = (progress.counts & ~(GameProgress::CountMask << GameProgress::CountTotalShift)) |
                      (total & GameProgress::CountMask) << GameProgress::CountTotalShift;
    progress.counts = (progress.counts & ~(GameProgress::CountMask << GameProgress::CountShift)) | total << GameProgress::CountShift;
    u16 object = *icon;
    oleg.SetHudIcon(WhackawormIcon, &object);
    if (State() == StatePlaying)
    {
        oleg.Show(oleg.masks[TimeScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }

    return 1;
}

u32 GameController::EndWhackaworm()
{
    if ((progress.bits & GameProgress::ModeMask) != ModeTimed)
    {
        return 0;
    }

    progress.bits &= ~GameProgress::ModeMask;
    oleg.Hide(oleg.masks[TimeScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    return 1;
}

u32 GameController::RestartFromCheckpoint()
{
    if ((progress.counts >> GameProgress::LivesShift & GameProgress::LivesMask) == 0)
    {
        return ForceGameOver();
    }

    if (State() == StateFadingOut)
    {
        return 0;
    }

    SetNextState(StateFadingOut);
    states = (states & ~EntryMask) | EntryKeepChunks;
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
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    saveController.Take(&progress, chunkManager, checkpoint);
    if ((saves->bits & SaveCode::SavingDue) == 0)
    {
        return 0;
    }

    SetSavingStep(SavingSavedShown);
    oleg.savingShown++;
    oleg.Show(oleg.masks[SavingScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    SaveManagerRequest(saves, SaveAutosave, static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    if (oleg.savingShown == 1)
    {
        oleg.Show(oleg.masks[SavingScreen2], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        StopChunks();
    }

    return 1;
}

void GameController::DisablePlayerControl(u32 unfollow)
{
    u32 bits = progress.bits;
    DisableCharacter(bits >> GameProgress::CharacterShift & GameProgress::FieldMask, 1, unfollow);
    DisableCharacter(bits >> GameProgress::SecondShift & GameProgress::FieldMask, 0, unfollow);
}

void GameController::UnlinkCharacter(InstanceContext* instance)
{
    auto* node = static_cast<PlayerNode*>(GetGameNode(&instance->nodes, NodePlayer));
    if (node == nullptr)
    {
        return;
    }

    PlayerCharacter* character = node->character;
    void* link = character->link;
    if (link == nullptr)
    {
        return;
    }

    // The first of the two unties them
    if ((character->data->bits & CharacterData::BitLinkedSecond) != 0)
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
    if ((states & u64{SavingMask} << SavingShift) == 0)
    {
        return 0;
    }

    SetNextState(StateWaiting);
    states = (states & ~u64{Notices}) | Notice4;
    return FinishSaving();
}

u32 GameController::LoadForNewGame()
{
    states &= ~u64{Notices};
    SetSavingStep(SavingNewGameLoaded);
    SaveManagerRequest(static_cast<SaveManager*>(G_UnkStruct_5C0), SaveNewGame,
                       static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    return 1;
}

u32 GameController::LoadSavedGame()
{
    states &= ~u64{Notices};
    SetSavingStep(SavingLoaded);
    SaveManagerRequest(static_cast<SaveManager*>(G_UnkStruct_5C0), SaveLoad,
                       static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    return 1;
}

u32 GameController::SaveFromPause()
{
    states &= ~u64{Notices};
    SetSavingStep(SavingPaused);
    oleg.savingShown = 0;
    SaveManagerRequest(static_cast<SaveManager*>(G_UnkStruct_5C0), SavePauseSave,
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
        oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0x1.99999Ap-4f), 0);
        oleg.Show(oleg.masks[NoControllerScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }

    return NoState;
}

u32 GameController::CheckingCard(TimeClock*)
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    if (frame == stateFrame)
    {
        oleg.Hide(oleg.masks[CardCheckScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        SaveManagerRequest(saves, SaveCheckCard, static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond));
    }

    return saves->Step() == 0 ? StateVivendiLogo : NoState;
}

u32 GameController::MainMenu(TimeClock* clock)
{
    if (frame == stateFrame)
    {
        oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        oleg.Show(oleg.masks[MainMenuScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        StopChunks();
        return NoState;
    }

    MenuWidget* menu = oleg.ShownMenu();
    if (menu != nullptr && (menu->menuFlags & MenuWidget::Leaves) != 0 && menu->current == &g_ResumePage)
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
        GetColor(&hidden, 0);
        GetColor(&shown, 8);
        oleg.sprite13B8.hiddenColour = hidden;
        oleg.sprite13B8.shownColour = shown;
        oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        oleg.Show(oleg.masks[BlackScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        FadeSoundGroups(0.5f);
        return NoState;
    }

    return oleg.ScreenWidget(BlackScreen)->State() == Widget::StateShown ? StateRestarting : NoState;
}

u32 GameController::Paused(TimeClock* clock)
{
    if (PauseReason(clock, 1) == 0)
    {
        return NoState;
    }

    MenuWidget* menu = oleg.ShownMenu();
    if (menu == nullptr || (menu->menuFlags & MenuWidget::Leaves) == 0 || menu->current != &g_ResumePage)
    {
        return NoState;
    }

    oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    if ((states >> ReturnShift & StateMask) == StateWatching)
    {
        oleg.Show(oleg.masks[WatchingScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }
    else if ((flags & FlagBottomText) != 0)
    {
        oleg.Show(oleg.masks[BottomTextScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }
    else
    {
        hudDelay = static_cast<s32>(g_ClockUnitsPerSecond * 0.25f);
    }

    return StateWaiting;
}

u32 GameController::GameOver(TimeClock*)
{
    if (frame != stateFrame)
    {
        ShowPictureScreen(GameOverScreen);
        return NoState;
    }

    u32 picture = 0;
    switch (progress.bits >> 8 & 0xF)
    {
    case 0:
        picture = (progress.bits >> 12 & 0xF) == 1 ? GameOverCrashAndCortex : GameOverCrash;
        break;
    case 1:
        picture = GameOverCortex;
        break;
    case 3:
        picture = GameOverNina;
        break;
    case 5:
        picture = GameOverMecha;
        break;
    default:
        break;
    }

    if (picture != 0)
    {
        oleg.WantPicture(TilesPicture, picture);
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
        GetColor(&hidden, 0);
        GetColor(&shown, 8);
        oleg.sprite13B8.hiddenColour = hidden;
        oleg.sprite13B8.shownColour = shown;
        oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        oleg.Show(oleg.masks[BlackScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        return NoState;
    }

    if (oleg.ScreenWidget(BlackScreen)->State() != Widget::StateShown)
    {
        return NoState;
    }

    saveController.Restore(0, &progress, chunkManager);
    ResetGame(0);
    RequestMovie(static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), IntroMovie);
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
        oleg.Hide(oleg.masks[AllButFirstScreens], delay, 0);
        oleg.Show(oleg.masks[BlackScreen], delay, 0);
        return 0;
    }

    s32 tenth = static_cast<s32>(g_ClockUnitsPerSecond * 0x1.99999Ap-4f);
    if (delay + tenth >= static_cast<s32>(clock->time - static_cast<u32>(stateTime)))
    {
        return 0;
    }

    if (started == 0)
    {
        const GameMovie& movie = g_Movies[index];
        u32 language = (movie.bits >> GameMovie::KindShift & GameMovie::KindMask) == GameMovie::NoLanguage ? 0 : g_CurrentLanguage;
        movieFrame = frame;
        oleg.Hide(oleg.masks[BlackScreen], tenth, 0);
        s32 width = movie.bits >> GameMovie::WidthShift & GameMovie::SizeMask;
        s32 height = movie.bits >> GameMovie::HeightShift & GameMovie::SizeMask;
        if (movies->Play(movie.file, language, movie.bits & GameMovie::Widescreen, width, height) != 0)
        {
            return 0;
        }
    }
    else
    {
        movieFrame = started;
        if ((movies->flags & GameMovieController::StateMask) == GameMovieController::StatePlaying)
        {
            if (skippable == 0 || !GetButtonState(pad, PadCross, true))
            {
                return 0;
            }

            if (skipped != nullptr)
            {
                *skipped = 1;
            }

            oleg.Show(oleg.masks[BlackScreen], 0, 0);
            movies->RequestStop();
            return 1;
        }
    }

    if (skipped != nullptr)
    {
        *skipped = 0;
    }

    oleg.Show(oleg.masks[BlackScreen], 0, 0);
    return 1;
}

u32 GameController::ShowPictureScreen(u32 screen)
{
    s32 shownSince = movieFrame;
    movieFrame = 0;
    u32 picture = oleg.pictures[TilesPicture];
    bool reading = (picture & 1) != 0;
    u32 read = picture >> 1 & 0xFF;
    if (reading || read == NoTilesPicture)
    {
        if (!reading)
        {
            oleg.LoadPicture(TilesPicture, 0);
        }

        return 0;
    }

    s32 half = static_cast<s32>(g_ClockUnitsPerSecond * 0.5f);
    if ((picture >> 9 & 0xFF) == read)
    {
        if (oleg.ScreenWidget(PictureScreen)->State() < Widget::StateAppearing)
        {
            oleg.Hide(oleg.masks[AllButFirstScreens], half, 0);
            oleg.Show(oleg.masks[BlackScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
            oleg.Show(oleg.masks[screen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
            return 0;
        }

        if (oleg.ScreenWidget(PictureScreen)->State() != Widget::StateShown)
        {
            return 0;
        }

        movieFrame = shownSince != 0 ? shownSince : frame;
        return 1;
    }

    // Another picture is wanted: this one faded to black and let go first
    if (oleg.ScreenWidget(PictureScreen)->State() == Widget::StateShown)
    {
        oleg.Hide(oleg.masks[AllButFirstScreens], half, 0);
        oleg.Show(oleg.masks[BlackScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        return 0;
    }

    if (oleg.ScreenWidget(PictureScreen)->State() < Widget::StateAppearing)
    {
        oleg.ReleasePicture(TilesPicture);
    }

    return 0;
}

u32 GameController::LoadingTitle(TimeClock*)
{
    if (frame == stateFrame)
    {
        u32 last = static_cast<u32>(states >> LastShift & StateMask);
        StopChunks();
        if (last == StateMovie)
        {
            ResetGame(0);
            return StateTitle;
        }

        StringAssign(&progress.startChunk, g_StartChunkPath.string);
        u32 loading = RandomLoadingScreen();
        oleg.WantPicture(TilesPicture, loading);
        return NoState;
    }

    if (ShowPictureScreen(LoadingScreen) == 0)
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

    return progress.ChunkLoaded((flags & 2) != 0 ? 2 : 1, 0) != 0 ? StateTitle : NoState;
}

u32 GameController::Title(TimeClock* clock)
{
    if (frame == stateFrame)
    {
        for (u32 character = 0; character < Characters; character++)
        {
            DisableCharacter(character, 0, 1);
        }

        oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
        Checkpoint* checkpoint = progress.Reset(0, nullptr);
        saveController.Take(&progress, chunkManager, checkpoint);
        if (SwitchCharacter(TitleCharacter, 1, 1) == 0)
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

    if (static_cast<s32>(clock->time - static_cast<u32>(stateTime)) < static_cast<s32>(g_ClockUnitsPerSecond * 50.0f))
    {
        return NoState;
    }

    RequestRandomMovie(static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), AttractMovie, AttractMovie);
    return NextState();
}

u32 GameController::LoadingLevel(TimeClock*)
{
    if (frame == stateFrame)
    {
        u32 loading = RandomLoadingScreen();
        oleg.WantPicture(TilesPicture, loading);
        return NoState;
    }

    if (ShowPictureScreen(LoadingScreen) == 0)
    {
        return NoState;
    }

    if (frame == movieFrame)
    {
        ChunkManager* kept = (states & EntryMask) != EntryKeepChunks ? chunkManager : nullptr;
        oleg.Reset();
        if (progress.startChunk.length != 0)
        {
            ChunkLoadingManager* loading = G_ChunkLoadingManager_;
            UnloadEverything(loading, true, kept);
            QueueChunk(loading, &progress.startChunk, 0);
        }

        progress.Reset(static_cast<u32>(states >> EntryShift & 0xF), chunkManager);
        if ((states & EntryMask) == EntrySaved)
        {
            saveController.Restore(1, &progress, chunkManager);
        }

        return NoState;
    }

    return progress.ChunkLoaded((flags & FlagRb) != 0 ? 2 : 1, 0) != 0 ? StateStartingPlay : NoState;
}

u32 GameController::StartingPlay(TimeClock*)
{
    VideoController* video = G_VideoController;
    u32 last = static_cast<u32>(states >> LastShift & StateMask);
    if (frame == stateFrame)
    {
        for (u32 character = 0; character < Characters; character++)
        {
            DisableCharacter(character, 0, 1);
        }

        switch (last)
        {
        case StateWaitingForLoader:
        case StateLoadingLevel:
        {
            progress.Enter(static_cast<u32>(states >> EntryShift & 0xF), &saveController.chunk, &saveController.place, &progress,
                           chunkManager);
            EnableCharacters(1);
            InstanceContext* player = CharacterInstance(&progress);
            auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, Node16));
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
            flags &= ~1u;
            break;
        default:
            break;
        }

        g_InstancesWithValue174 = 0;
        return NoState;
    }

    u32 pairing = progress.bits >> 4 & 0xF;
    InstanceContext* player = CharacterInstance(&progress);
    PlayerCharacter* character = static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodePlayer))->character;
    InstanceContext* second = SecondCharacterInstance(&progress);
    PlayerCharacter* secondCharacter = nullptr;
    if (second != nullptr)
    {
        auto* node = static_cast<PlayerNode*>(GetGameNode(&second->nodes, NodePlayer));
        if (node != nullptr)
        {
            secondCharacter = node->character;
        }
    }

    if (last != StateWatching && last != StateMovie)
    {
        switch (pairing)
        {
        case PairingVehicle3:
            SetPlayerVehicle(character, 3, secondCharacter, 0);
            break;
        case PairingVehicle1:
            SetPlayerVehicle(character, 1, secondCharacter, 0);
            break;
        case PairingLinked:
        {
            LinkCharacters(character, secondCharacter);
            Reference* argument = second != nullptr ? AddReference(second) : nullptr;
            GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), LinkedEvent, &argument,
                                                    2);
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
    oleg.Hide(~u64{0}, static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    hudDelay = static_cast<s32>(g_ClockUnitsPerSecond * 0.25f);
    StringAssign(&progress.startChunk, "");
    StringAssign(&progress.chunk1C, "");
    StringAssign(&progress.chunk28, "");
    return StatePlaying;
}

u32 GameController::Gallery(TimeClock*)
{
    if (frame != stateFrame)
    {
        if (ShowPictureScreen(GalleryScreen) == 0)
        {
            return NoState;
        }

        bool back = GetButtonState(pad, PadTriangle, true);
        bool next = GetButtonState(pad, PadCross, true);
        if (!back && !next)
        {
            return NoState;
        }

        u32 shown = flags >> GalleryShownShift & GalleryPictureMask;
        if (back || shown == (flags >> GalleryLastShift & GalleryPictureMask))
        {
            BackToPauseMenu();
            return StatePaused;
        }

        flags = (flags & ~(GalleryPictureMask << GalleryShownShift)) | ((shown + 1) & GalleryPictureMask) << GalleryShownShift;
    }

    String name;
    name.string = nullptr;
    name.length = 0;
    name.capacity = 0;
    StringAssign(&name, gallery.string);
    u32 shown = flags >> GalleryShownShift & GalleryPictureMask;
    // The other of the two named pictures, so it's read again
    u32 picture = (oleg.pictures[TilesPicture] >> 1 & 0xFF) == NamedPicture ? NamedPicture + 1 : NamedPicture;
    if (shown < 10)
    {
        StringAppend(&name, "0");
    }

    String number;
    StringConstructNumber(&number, shown);
    StringAppend(&name, number.string);
    StringDestroy(&number);
    StringAssign(&oleg.text, name.string);
    oleg.WantPicture(TilesPicture, picture);
    StringDestroy(&name);
    return NoState;
}

u32 GameController::RequestGallery(s32 delay, const char* name, u32 first, u32 last)
{
    if (State() != StatePaused)
    {
        return 0;
    }

    u32 bits = (flags & ~(GalleryPictureMask << GalleryShownShift)) | (first & GalleryPictureMask) << GalleryShownShift;
    bits = (bits & ~(GalleryPictureMask << GalleryFirstShift)) | (first & GalleryPictureMask) << GalleryFirstShift;
    flags = (bits & ~(GalleryPictureMask << GalleryLastShift)) | (last & GalleryPictureMask) << GalleryLastShift;
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
        oleg.WantPicture(TilesPicture, CreditsPicture);
        StopChunks();
        g_CreditsTrack = 0;
        g_CreditsTrackTime = CreditsTrackSeconds;
        return NoState;
    }

    if (ShowPictureScreen(PictureScreen) == 0)
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
        oleg.wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * Rounded(0.15));
        for (u32 index = FirstHudSprite; index < EndHudSprites; index++)
        {
            Sprite& sprite = oleg.sprites[index];
            CallVirtual<void>(&sprite, sprite.vtable, SpriteSetMaterialSlot, Platform::Graphics::FlatMaterial());
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
            request.bits = CreditsMusic[g_CreditsTrack] | CreditsTrackBits;
            request.right = 1.0f;
            g_CreditsTrackTime = 0.0f;
            request.left = 1.0f;
            request.fadeTime = g_CreditsTrack == 0 ? 0.0f : 3.0f;
            PlayMusicRequest(0, &request);
            g_CreditsTrack++;
        }

        return NoState;
    }

    if (progress.ChunkLoaded((flags & FlagRb) != 0 ? 2 : 1, 0) == 0)
    {
        return NoState;
    }

    if (credits != nullptr)
    {
        credits->Destroy(3);
    }

    credits = nullptr;
    return StateStartingPlay;
}

u32 GameController::Restarting(TimeClock*)
{
    Checkpoint* checkpoint = progress.Reset(static_cast<u32>(states >> EntryShift & 0xF), chunkManager);
    if (checkpoint != nullptr)
    {
        StringAssign(&progress.startChunk, checkpoint->chunk.string);
    }

    if ((states & EntryMask) == EntrySaved)
    {
        saveController.summary = (saveController.summary & ~0x7Fu) | StartLives;
        GameRendererController* renderer = G_GameRendererController;
        u32 vibration = (static_cast<GamePadController*>(G_GamePadController)->flags << 6) & SaveController::OptionVibration;
        saveController.options = (saveController.options & ~SaveController::OptionVibration) | vibration;
        saveController.effectsVolume = GroupVolumeLevel(EffectsGroup);
        saveController.musicVolume = GroupVolumeLevel(MusicGroup);
        u32 options = (saveController.options & ~(SaveController::OptionMusicStereoMask << SaveController::OptionMusicStereoShift)) |
                      (g_MusicStereo & SaveController::OptionMusicStereoMask) << SaveController::OptionMusicStereoShift;
        saveController.options = options;
        saveController.options = (options & ~SaveController::OptionWidescreen) | (g_WidescreenTv & 1u) << 16;
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

    ResetGame(static_cast<u32>(states >> EntryShift & 0xF));
    return StateStartingPlay;
}

void GameController::BackToPauseMenu()
{
    u32 reason = static_cast<u32>(states >> PauseReasonShift & PauseReasonMask);
    oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    if (reason - 1 < PauseScreenCount && PauseScreens[reason - 1] >= 0)
    {
        oleg.Show(oleg.masks[PauseScreens[reason - 1]], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
    }
}

u32 GameController::PausePage()
{
    u32 reason = static_cast<u32>(states >> PauseReasonShift & PauseReasonMask);
    bool back = false;
    bool on = false;
    if (oleg.optionsMenu.State() < Widget::StateAppearing && oleg.screenPositionMenu.State() < Widget::StateAppearing)
    {
        back = GetButtonState(pad, PadL1, true) || GetButtonState(pad, PadL2, true);
        on = GetButtonState(pad, PadR1, true) || GetButtonState(pad, PadR2, true);
    }

    u32 area = progress.bits >> 26 & 0x1F;
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
    u32 reason = static_cast<u32>(states >> PauseReasonShift & PauseReasonMask);
    if (reason == ReasonQuitting)
    {
        return ReasonQuitting;
    }

    u32 next = ReasonNone;
    u64 saving = states & u64{SavingMask} << SavingShift;
    if ((states & Notice6) != 0)
    {
        states &= ~u64{Notices};
        next = ReasonNotice6;
    }
    else if (State() == StatePaused || State() == StateGallery)
    {
        if (saving == u64{4} << SavingShift || saving == u64{0xC} << SavingShift)
        {
            return reason;
        }

        next = reason >= ReasonNotice4 && reason < ReasonQuitting ? reason : ReasonNone;
    }
    else if ((states & Notice4) != 0)
    {
        states &= ~u64{Notices};
        next = ReasonNotice4;
    }
    else if ((states & Notice7) != 0)
    {
        states &= ~u64{Notices};
        next = ReasonNotice7;
    }
    else if ((states & Notice5) != 0)
    {
        states &= ~u64{Notices};
        next = saving != 0 ? ReasonNotice5 : ReasonNone;
    }
    else if (saving == u64{6} << SavingShift)
    {
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
            next = (states & StartPressed) != 0 ? ReasonStart : ReasonNone;
        }
    }

    states = (states & ~(u64{PauseReasonMask} << PauseReasonShift)) | static_cast<u64>(next & PauseReasonMask) << PauseReasonShift;
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

    states = (states & ~(u64{StateMask} << ReturnShift)) | static_cast<u64>(State()) << ReturnShift;
    StopChunks();
    return next;
}

u32 GameController::Playing(TimeClock* clock)
{
    Reference* reference = progress.characters[FifthCharacter];
    ReferencedObject* fifth = reference != nullptr ? reference->object : nullptr;
    if (fifth != nullptr)
    {
        CallVirtual<void>(fifth, fifth->vtable, InstanceStepSlot);
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

    s32 quarter = static_cast<s32>(g_ClockUnitsPerSecond * 0.25f);
    u32 mode = progress.bits & 0xF;
    if (mode == ModeHealth)
    {
        oleg.Show(oleg.masks[HealthScreen], quarter, 0);
    }
    else if (mode == ModeTimed)
    {
        if ((flags & 1) == 0)
        {
            oleg.Show(oleg.masks[TimeScreen], quarter, 0);
        }
    }
    else
    {
        oleg.Show(oleg.masks[HudScreen], quarter, static_cast<s32>(g_ClockUnitsPerSecond * 3.0f));
        oleg.Show(oleg.masks[SliderScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
        if ((flags & 1) == 0)
        {
            oleg.Show(oleg.masks[CounterScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.25f), 0);
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

    u32 last = static_cast<u32>(states >> LastShift & StateMask);
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
    // The instances kept: every flag but bit 9
    const u32 filter[3] = {0xFFFFFDFF, 0, 0};
    u32 dropInstances = entry < 2;
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
// The camera shown, the state word's bits 19-22 (0 the played character's follow camera, 3 the game's rig, 4 the cutscenes')
constexpr u32 CameraShownShift = 19;
constexpr u64 CameraShownMask = 0xF;
constexpr u32 ShowsFollowCamera = 0;
constexpr u32 ShowsGameRig = 3;
constexpr u32 ShowsCutsceneRig = 4;
// The camera's instance's bit 17 keeps it in its chunk (the follow node's CanChangeChunk)
constexpr u32 CameraStaysFlag = 0x20000;
// The player's held: the character's bit at 0x1C (its agent's), the instance's flag 0x80000 every character but the fifth loses
// when it's let go
constexpr u32 CharacterHeld = 0x1;
constexpr u32 SolidModelFlag = 0x80000;
// The character part's reset slot (its vtable's 2), the kind it's given when the player's let go
constexpr u32 PartResetSlot = 2;
constexpr u32 PartResetKind = 3;

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
    auto* node = static_cast<PlayerNode*>(GetGameNode(&player->nodes, NodePlayer));
    return {controls, reinterpret_cast<CharacterAgent*>(node->character)};
}

CharacterPart* PartOf(CharacterAgent* character)
{
    return static_cast<CharacterPart*>(character->part);
}
}

InstanceContext* ShowCamera(GameController* controller, u32 camera, u32 reset)
{
    controller->states = (controller->states & ~(CameraShownMask << CameraShownShift)) |
                         static_cast<u64>(camera & CameraShownMask) << CameraShownShift;
    switch (camera)
    {
    case ShowsFollowCamera:
    {
        InstanceContext* player = CharacterInstance(&controller->progress);
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, Node16));
        auto* followCamera = follow->object != nullptr ? static_cast<InstanceContext*>(follow->object->object) : nullptr;
        ShowFollowCamera(&follow->camera, followCamera, player, reset);
        // Unchecked: the follow node always has a camera once it's given its instance
        followCamera->flags &= ~CameraStaysFlag;
        AssignReference(&controller->view.cameraObject, followCamera);
        return followCamera;
    }
    case ShowsGameRig:
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
    case ShowsCutsceneRig:
    {
        InstanceContext* shown = ShownCamera(controller);
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&shown->nodes, NodeCameraLens));
        // The cutscenes' rig without followers or a target, taking nothing from the camera trigger, not smoothed
        CutsceneCameraRig& rig = controller->cutsceneCamera;
        rig.DropCameraFollower();
        rig.cameraFollower = nullptr;
        rig.bits = ((rig.bits & ~CameraRig::BitOwnsCameraFollower & ~CameraRig::BitIgnoresTrigger) | CameraRig::BitIgnoresTrigger) &
                   ~CameraRig::BitSmoothed;
        rig.DropTargetFollower();
        rig.targetFollower = nullptr;
        rig.bits &= ~CameraRig::BitOwnsTargetFollower;
        rig.DropTarget();
        rig.target = nullptr;
        rig.bits &= ~CameraRig::BitOwnsTarget;
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
    if (camera == ShowsFollowCamera)
    {
        InstanceContext* player = CharacterInstance(&controller->progress);
        auto* follow = static_cast<FollowNode*>(GetGameNode(&player->nodes, Node16));
        auto* followCamera = follow->object != nullptr ? static_cast<InstanceContext*>(follow->object->object) : nullptr;
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&followCamera->nodes, NodeCameraLens));
        CameraRig* rig = &follow->camera.rig;
        if (reset != 0)
        {
            rig->ResetVirtual(followCamera);
        }

        lens->BlendTo(rig, *ticks, blendCurve);
    }
    else if (camera == ShowsGameRig)
    {
        auto* lens = static_cast<CameraLensNode*>(GetGameNode(&ShownCamera(controller)->nodes, NodeCameraLens));
        lens->BlendTo(&controller->camera, *ticks, blendCurve);
    }
}

void GameController::HoldPlayer()
{
    HeldCharacter held = PlayedCharacter(this);
    CharacterAgent* character = held.character;
    character->unknown1C |= CharacterHeld;
    held.controls->bits |= ControlsNode::BitMotionDriven;
    // What the reset loses of the part kept: being tied to the other character (only the leader isn't set back) and the hit
    // points
    CharacterPart* part = PartOf(character);
    bool leader = (part->moveBits & CharacterPart::LinkedFirst) != 0;
    bool second = (part->moveBits & CharacterPart::LinkedSecond) != 0;
    u32 hitPoints = part->flags >> CreaturePart::HitPointsShift & CreaturePart::HitPointsMask;
    if (!leader)
    {
        character->Reset();
    }

    if (second)
    {
        PartOf(character)->moveBits |= CharacterPart::LinkedSecond;
    }
    else if (leader)
    {
        PartOf(character)->moveBits |= CharacterPart::LinkedFirst;
    }

    part = PartOf(character);
    part->flags = (part->flags & ~(CreaturePart::HitPointsMask << CreaturePart::HitPointsShift)) |
                  hitPoints << CreaturePart::HitPointsShift;
}

void GameController::ReleasePlayer(u32 resume)
{
    HeldCharacter held = PlayedCharacter(this);
    CharacterAgent* character = held.character;
    character->unknown1C &= ~CharacterHeld;
    held.controls->bits &= ~ControlsNode::BitMotionDriven;
    CharacterPart* part = PartOf(character);
    bool leader = (part->moveBits & CharacterPart::LinkedFirst) != 0;
    u32 hitPoints = part->flags >> CreaturePart::HitPointsShift & CreaturePart::HitPointsMask;
    if (!leader && resume != 0)
    {
        CallVirtual<void>(part, part->vtable, PartResetSlot, PartResetKind);
        character->Reset();
    }

    part = PartOf(character);
    part->flags = (part->flags & ~(CreaturePart::HitPointsMask << CreaturePart::HitPointsShift)) |
                  hitPoints << CreaturePart::HitPointsShift;
    for (u32 index = 0; index < GameProgress::Characters; index++)
    {
        if (index == FifthCharacter)
        {
            continue;
        }

        InstanceContext* instance = progress.Instance(index);
        if (instance != nullptr)
        {
            instance->flags &= ~SolidModelFlag;
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
