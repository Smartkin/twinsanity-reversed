#pragma once

#include "common.h"
#include "game/camerarig.h"
#include "game/font.h"
#include "game/instances.h"
#include "game/oleg.h"
#include "game/sound.h"
#include "game/view.h"

class MemoryStream;
struct ChunkManager;
struct InstanceContext;
struct GamePad;
struct GameResources;
struct Renderer;
struct TimeClock;


// The game controller (the retail GameController, 0x5140 bytes of the heap): the game's flow as a state machine stepped every
// frame, the game's progress and OLEG among its members. Its state word: bits 19-22 the camera shown (ShowCamera), bit 31 the
// start button pressed this frame, bits 23-26 how a level is entered (the progress's reset), 32-37 the state a wait returns to,
// 38-43 the last state, 44-49 the state, 50-55 the next one (applied the next frame, 24 none), 60-63 the saving's step
struct GameController
{
    enum State : u32
    {
        StateWaitingForLoader = 0,
        StateStarting = 1,
        StateWaitingForPad = 2,
        StateCheckingCard = 3,
        StateVivendiLogo = 4,
        StateTravellersTalesLogo = 5,
        StateLoadingTitle = 6,
        StateTitle = 7,
        StateMainMenu = 8,
        StateLoadingLevel = 9,
        StateNewGame = 10,
        StateStartingPlay = 11,
        StatePlaying = 12,
        StateWatching = 13,
        StateMovie = 14,
        StatePaused = 15,
        StateGallery = 16,
        StateWaiting = 17,
        StateGameOver = 18,
        StateCredits = 19,
        StateRestartingTitle = 20,
        StateFadingOut = 21,
        StateRestarting = 22,
        NoState = 24,
    };

    enum StateBits : u64
    {
        // Why play stopped (bits 27-30): see PauseReason
        PauseReasonShift = 27,
        PauseReasonMask = 0xF,
        StartPressed = 1ull << 31,
        ReturnShift = 32,
        LastShift = 38,
        CurrentShift = 44,
        NextShift = 50,
        StateMask = 0x3F,
        // Notices asked for (bits 56-59, the pause reasons 4, 5, 6 and 7)
        Notice4 = 1ull << 56,
        Notice5 = 1ull << 57,
        Notice6 = 1ull << 58,
        Notice7 = 1ull << 59,
        Notices = 0xFull << 56,
        // The saving's step (bits 60-63)
        SavingShift = 60,
        SavingMask = 0xF,
    };

    // Why play stops (the state word's bits 27-30): the start button, the pad missing, a disc error, notices (4 to 7: 5 only
    // while saving), quitting (8, kept); while paused, the pages the shoulder buttons go to (9 to 12 the areas' levels, 13 the
    // extras, 1 the pause menu)
    enum PauseReasons : u32
    {
        ReasonNone = 0,
        ReasonStart = 1,
        ReasonNoPad = 2,
        ReasonDiscError = 3,
        ReasonNotice4 = 4,
        ReasonNotice5 = 5,
        ReasonNotice6 = 6,
        ReasonNotice7 = 7,
        ReasonQuitting = 8,
        ReasonFirstLevels = 9,
        ReasonExtras = 13,
    };

    // The language's text files (the code's and AgentLab's), in memory
    MemoryStream* texts[2];
    u64 states;
    // Bit 0: the bottom text's screen up (a cutscene's or the scripts'), bit 1: the chunk manager's "RB", bit 2: the context's
    // bit 7, bit 3 cleared when cutscenes start and end, bits 5-12, 13-20 and 21-28 the gallery's first picture, its last and the
    // one shown (the retail code writes it with the HUD's delay as one 64 bit word)
    u32 flags;
    // Clock units the HUD waits for when play starts
    s32 hudDelay;
    // The frames run, the frame a movie was started at and the frame and the time (clock units) the state began at
    s32 frame;
    s32 movieFrame;
    s32 stateFrame;
    s32 stateTime;
    // When the saving began waiting (clock units)
    s32 saveTime;
    Renderer* renderer;
    ChunkManager* chunkManager;
    void* resourceManager;
    GameResources* resources;
    GamePad* pad;
    s32 unknown40;
    // The front end's sounds (StartUp\Frontend.bin's)
    SoundTable frontEndSounds;
    // How long the movie state waits before its movie (clock units; the gallery's request sets it too), the movie (an index of
    // the game's movies) and the gallery's pictures' name (the picture's number after it, two digits at least)
    s32 movieDelay;
    u32 movie;
    String gallery;
    // The game's font (StartUp\Fonts\Crash_Euro)
    Font font;
    // The view the renderer draws the scene with, the game's camera and the cutscenes' (the video controller's)
    RenderView view;
    GameCameraRig camera;
    CutsceneCameraRig cutsceneCamera;
    GameProgress progress;
    SaveController saveController;
    OLEG oleg;
    // The credits rolling in the credits state
    CreditsRoll* credits;
    u8 unknown5134[0x5140 - 0x5134];

    u32 State() const
    {
        return static_cast<u32>(states >> CurrentShift & StateMask);
    }

    u32 NextState() const
    {
        return static_cast<u32>(states >> NextShift & StateMask);
    }

    void SetNextState(u32 state)
    {
        states = (states & ~(u64{StateMask} << NextShift)) | static_cast<u64>(state & StateMask) << NextShift;
    }

    // Made with the game's pad, the renderer (given its view), the chunk manager, the resource manager and the resources:
    // its members made, the save's data allocated (0xF000 bytes), no next state; the flat box hull and the freed memory's pools
    // made too
    static GameController* Construct(GameController* controller, GamePad* pad, s32 unknown40, Renderer* renderer,
                                     ChunkManager* chunks, void* resourceManager, GameResources* resources) RETAIL(FUN_00175808);
    // A frame of the game: the start button read, the saving stepped, the next state taken on and the state's frame (its next
    // state when it has one), OLEG's frame with the character played; while playing (or watching), the effects' frame and the
    // sound's listener, and the frame's freed memory let go unless asked not to. Returns whether it's playing
    u32 Update(u32 keepFreed) RETAIL(FUN_00177278);
    // The saving's step (the state word's bits 60-63), once the save code is idle: 1 a save asked for a quarter of a second after
    // it began waiting, the steps after an operation its result (2 a save: done, or flagged and waiting to save again; 3 the
    // same, the screens it showed hidden; 4 a load: the pause or main menu again, or flagged and the saved game's level asked
    // for; 5 a new game's: the main menu again, or flagged and a new game; 6 the pause menu again). Returns 1 when a step's done
    // with (and past 6)
    u32 UpdateSaving(TimeClock* clock) RETAIL(FUN_001731c0);
    // The saving's step set
    void SetSavingStep(u32 step)
    {
        states = (states & ~(u64{SavingMask} << SavingShift)) | static_cast<u64>(step) << SavingShift;
    }

    // The menus' requests: back to the pause menu (unless it's the start button's pause), back to the title, the saving stopped
    // (a wait, notice 4: whether a save due was under way), and the save code asked for a new game's operation, a load or a
    // save from the pause menu (two seconds' wait each). Each returns whether it was asked for
    u32 ReturnToPauseMenu() RETAIL(FUN_0017b860);
    u32 QuitToTitle() RETAIL(FUN_0017b990);
    u32 StopSaving() RETAIL(FUN_0017be18);
    u32 LoadForNewGame() RETAIL(FUN_0017c018);
    u32 LoadSavedGame() RETAIL(FUN_0017bf38);
    u32 SaveFromPause() RETAIL(FUN_0017c0f8);
    // The game restarted a way (fading out first; not while it's fading out already). Whether it was asked for
    u32 RequestRestart(u32 entry) RETAIL(FUN_0017b738);
    // The saving's step back to none, and the save due's being under way forgotten
    void FinishSaveDue();
    // The save due started (waiting to save) when the save code wants one, the saving finished otherwise
    void StartSaveDue();
    // The saving's step back to none, the save due finished: whether it was under way
    u32 FinishSaving() RETAIL(FUN_0017b640);
    // A new game asked for (not again while it's starting): after a flagged operation, the save due started (or the saving
    // finished), notice 5 when it was; otherwise notice 4, the saving's step none
    u32 RequestNewGame(u32 flagged) RETAIL(FUN_00175e50);
    // The saved game's level asked for (not while loading a level): after a flagged operation, the save controller's chunk the
    // start chunk and the save due started (or the saving finished); otherwise the save controller taking the progress at its
    // reset, the saving's step none
    u32 RequestSavedLevel(u32 flagged) RETAIL(FUN_001760e0);
    // A movie of the game's played after a delay (clock units) and a tenth of a second on a black screen (none without a movie
    // controller), the cross button skipping it when it's skippable: whether it's over, and whether it was skipped
    u32 PlayMovie(TimeClock* clock, s32 delay, u32 index, u32 skippable, u8* skipped) RETAIL(FUN_00172ae8);
    // The movie state asked for with a movie (or a random one of a range), and its end: back to loading the title from the
    // title's attract movie, back to the pause menu, or on to start playing
    u32 RequestMovie(s32 delay, u32 index) RETAIL(FUN_0017b9d0);
    u32 RequestRandomMovie(s32 delay, u32 first, u32 last) RETAIL(FUN_0017ba18);
    u32 MovieEnded() RETAIL(FUN_0017bae8);
    // Back to the pause screen of the pause reason (none for quitting), every other screen hidden
    void BackToPauseMenu() RETAIL(FUN_00176270);
    // While paused, the pause screen the shoulder buttons page to (L1 or L2 back, R1 or R2 on: the pause menu, the levels of the
    // areas open, the extras), only while the options and the screen position aren't shown; else the pause menu's (the
    // notices', none's), or the reason as it is
    u32 PausePage() RETAIL(FUN_001764a0);
    // The scripts' requests, each returning whether it was taken: the bottom text's screen shown over a duration while playing
    // (every other screen hidden, the text emptied) and hidden (the HUD back after the duration); a cutscene started while
    // playing (watching it, the player held, the bottom text's screen; on the title only the screen) and ended (back to playing,
    // the player let go); boss mode (the health play mode with a bar of a length, the health the boss has and its icon) and its
    // end; whack-a-worm (the timed play mode with a time, a count down from a total and its icon) and its end; play again from
    // the last checkpoint (the game over without lives), the game over, the credits
    u32 ShowBottomText(s32 duration) RETAIL(FUN_00176db0);
    u32 HideBottomText(s32 duration) RETAIL(FUN_0017bbe0);
    u32 StartCutscene(s32 duration) RETAIL(FUN_00176b78);
    u32 EndCutscene(s32 duration) RETAIL(FUN_00176cb8);
    u32 EnableBossMode(f32 barLength, u32 health, const u16* icon) RETAIL_N32(FUN_00176e60);
    u32 ExitBossMode() RETAIL(FUN_0017bc38);
    u32 StartWhackaworm(s32 time, u32 total, const u16* icon) RETAIL(FUN_00176f50);
    u32 EndWhackaworm() RETAIL(FUN_0017bcb8);
    u32 RestartFromCheckpoint() RETAIL(FUN_0017b7b8);
    u32 ForceGameOver() RETAIL(FUN_0017b8d0);
    u32 PlayCredits() RETAIL(FUN_0017b930);
    // The game saved at a checkpoint while the save code's save is under way (the saving's screens shown, the chunks stopped by
    // the first), the save controller taking the progress at it. Whether it saves
    u32 Autosave(Checkpoint* checkpoint) RETAIL(FUN_00177048);
    // Both characters let go of their controls
    void DisablePlayerControl(u32 unfollow) RETAIL(FUN_0017c310);
    // The character's link to another undone
    void UnlinkCharacter(InstanceContext* instance) RETAIL(FUN_0017b6b8);
    // The player held for a cutscene (the character's held bit, its controls motion driven, its character set back unless it
    // leads two tied together: what that loses of being tied and the hit points kept), and let go after it (its part and its
    // character set back when asked, unless it leads; the hit points kept; every character's instance but the fifth's loses its
    // flag 0x80000)
    void HoldPlayer() RETAIL(FUN_00178150);
    void ReleasePlayer(u32 resume) RETAIL(FUN_00178290);

    // The frame begun (OLEG's widgets told, their slot 11), the renderer given the scene's view (not while a movie plays), the
    // frame ended (the widgets' slot 13)
    void BeginFrame() RETAIL(FUN_0017c1e0);
    void SetView(u32 playingMovie) RETAIL(FUN_0017c200);
    void EndFrame() RETAIL(FUN_0017c228);
    // Drawn (but OLEG while a movie plays): the credits rolling, the title's "press start" (breathing, two thirds of a second
    // after the title began, until bit 0), the characters' own parts of the overlay; then OLEG's widgets and its text line
    void Draw(u32 playingMovie) RETAIL(FUN_00177a18);

    // The states' frames, each returning the next state (NoState: none): waiting for the pad to be read (the screen asking for
    // a controller), the memory card checked through the save manager, the main menu (the title again once it's left)
    u32 WaitingForPad(TimeClock* clock) RETAIL(FUN_00173bc8);
    u32 CheckingCard(TimeClock* clock) RETAIL(FUN_00173c80);
    u32 MainMenu(TimeClock* clock) RETAIL(FUN_00174130);
    // Watching a cutscene (the cross button skips it), and fading out to restart from the last checkpoint
    u32 Watching(TimeClock* clock) RETAIL(FUN_00174f78);
    u32 FadingOut(TimeClock* clock) RETAIL(FUN_001754f8);
    // Loading the title (a random loading screen, then the start chunk), and the title (start opens the main menu, the attract
    // movie after 50 seconds)
    u32 LoadingTitle(TimeClock* clock) RETAIL(FUN_00173d10);
    u32 Title(TimeClock* clock) RETAIL(FUN_00173f60);
    // Loading a level (a random loading screen, the progress's chunk queued and the progress reset for the way in)
    u32 LoadingLevel(TimeClock* clock) RETAIL(FUN_00174268);
    // Playing: the fifth character's instance stepped, the pause when play stops, the HUD for the play mode once the HUD's
    // delay is over
    u32 Playing(TimeClock* clock) RETAIL(FUN_00174a90);
    // The characters: one let go of its controls (no pad, told to do nothing; its follow node out of its chunk and what it
    // follows told, when asked). Whether it has an instance
    u32 DisableCharacter(u32 character, u32 played, u32 unfollow) RETAIL(FUN_00178040);
    // One given a role: the role's character before let go of, then played (its follow camera on it, what it follows the view's
    // camera and the camera shakes' centre and put in its chunk, the pad its controls', the player's globals, the chunk's
    // colour filter) or the second. Whether it has an instance
    u32 SwitchCharacter(u32 character, u32 played, u32 unfollow) RETAIL(FUN_00177ba8);
    // The one played (another one in the chunk followed when it has no instance) and the second given their roles again
    void EnableCharacters(u32 unfollow) RETAIL(FUN_0017c248);
    // The next character after the one played that's in the chunk the loading follows, switched to. Whether there was one
    u32 SwitchToCharacterInFocus() RETAIL(FUN_001771b0);
    // Paused until the pause menu is left by its resume page (then a wait, the screen it goes back to shown), the game over
    // screen (the character's picture), and a new game (black, the game reset and the intro movie asked for)
    u32 Paused(TimeClock* clock) RETAIL(FUN_00174ca8);
    u32 GameOver(TimeClock* clock) RETAIL(FUN_00175048);
    u32 NewGame(TimeClock* clock) RETAIL(FUN_001744a0);
    // A screen of the tiles' picture (legal, loading, game over) shown: the wanted picture read, the one there faded to black and
    // let go first. Whether it's shown, the frame it was first shown at kept (in movieFrame)
    u32 ShowPictureScreen(u32 screen) RETAIL(FUN_00172d10);
    // The game reset for a way into it: the chunks' clocks, the progress, OLEG, the chunks and their instances, video, sound
    // and particles
    void ResetGame(u32 entry) RETAIL(FUN_00172fc8);
    // Why play stops (see PauseReasons; 0 it goes on), kept in the state word: a new reason shows its pause screen and, unless
    // it's paused already, pauses the game (the state to return to the current one, the chunks stopped)
    u32 PauseReason(TimeClock* clock, u32 paused) RETAIL(FUN_001766d0);
    // The start-up: the legal screen shown, and on its first frame the save manager, the language's texts and sounds, the
    // icons (StartUp\Icons.psm into OLEG's sprites 6 to 45), the global resources and the Crash title; then the pad waited for
    u32 Starting(TimeClock* clock) RETAIL(FUN_00173a38);
    // The language's text files read, their texts the current ones
    void LoadTexts() RETAIL(FUN_001729d0);
    // The font (StartUp\Fonts\Crash_Euro, the renderer's too), the front end's sounds (with "RB"), OLEG's start-up (unless the
    // context's bit 7) and the default chunk (StartUp\Default). Returns 1
    u32 LoadGlobalResources() RETAIL(LoadGlobalResources);
    // Play starting after the state before it: the characters let go of, then (from loading a level or the start) the progress
    // entered, the characters given their roles and the follow camera reset, (from the credits or restarting) the chunks
    // started, (from a cutscene or a movie) the characters' roles again; the next frame the second character paired with the
    // first as the progress says (but after a cutscene or a movie), the sound resumed, the cameras reset, every screen hidden
    // and the HUD's delay set, the progress's chunks forgotten
    u32 StartingPlay(TimeClock* clock) RETAIL(FUN_001745f8);
    // The gallery: its picture shown (the tiles' picture named by OLEG's text, the gallery's name and the picture's number),
    // the cross button the next one until the last, the triangle button back to the pause menu
    u32 Gallery(TimeClock* clock) RETAIL(FUN_00174dd0);
    // The gallery asked for (only while paused) with its pictures' name, its first picture and its last. Returns whether it was
    u32 RequestGallery(s32 delay, const char* name, u32 first, u32 last) RETAIL(FUN_0017bd38);
    // The credits: the chunks stopped, then the credits' picture, the credits rolling with eleven music tracks (one 19 seconds
    // after the one before), the chunk after the credits loaded meanwhile; once they're over and it's loaded, play starts
    u32 Credits(TimeClock* clock) RETAIL(FUN_001751a0);
    // Restarting the way the state word says: the progress reset (its checkpoint's chunk the start chunk); from the start chunk
    // the save controller's defaults (5 lives, the options as they are) and loading the level, otherwise the game reset and
    // play started once the chunk's loaded (loading the level until then)
    u32 Restarting(TimeClock* clock) RETAIL(FUN_00175658);
    // The other states'
};
CHECK_OFFSET(GameController, states, 0x8);
CHECK_OFFSET(GameController, renderer, 0x2C);
CHECK_OFFSET(GameController, frontEndSounds, 0x44);
CHECK_OFFSET(GameController, movieDelay, 0x5C);
CHECK_OFFSET(GameController, font, 0x70);
CHECK_OFFSET(GameController, progress, 0x500);
CHECK_OFFSET(GameController, saveController, 0x5A4);
CHECK_OFFSET(GameController, oleg, 0x5E0);
CHECK_SIZE(GameController, 0x5140);

// A movie of the game's: its file and its bits (bit 0 its pictures 16:9, bits 1-3 1 for a movie without a language's sound, 4-15
// its width, 16-27 its height)
struct GameMovie
{
    enum Bits : u32
    {
        Widescreen = 1,
        KindShift = 1,
        KindMask = 7,
        NoLanguage = 1,
        WidthShift = 4,
        HeightShift = 16,
        SizeMask = 0xFFF,
    };

    const char* file;
    u32 bits;
};

extern "C"
{
    // The chunk the game starts in (the start-up's) and the one after the credits
    extern String g_StartChunkPath RETAIL(G_StartChunk_Path);
    extern String g_PostCreditsChunkPath RETAIL(G_PostCreditsChunk_Path);
    // The credits' music track playing (an index of their tracks) and the time since it started (seconds)
    extern u32 g_CreditsTrack RETAIL(D_00309A90);
    extern f32 g_CreditsTrackTime RETAIL(D_00309A94);
    // The game font's size (fractions of the screen) and a smaller one's (set by OLEG's file's static initialiser)
    extern Vector2 g_FontSize RETAIL(D_0030A7A8);
    extern Vector2 g_SmallFontSize RETAIL(D_0030A7B0);
    // The game's movies: FMV\TTIdent, the story's (H01_a to H04_db), the attract movie, the bonus movies, FMV\Complete and
    // FMV\Vivendi
    extern const GameMovie g_Movies[20] RETAIL(D_002E70F8);
    // The game controller, and the copies of it the game's other files keep
    extern GameController* G_GameController;
    extern GameController* G_GameController_0030988C;
    extern GameController* G_GameController_00309890;
    extern GameController* G_GameController_00309914;
    extern GameController* G_GameController_00309950;

    // The flat box hull (4 by 0.3 by 4 units, D_0030BB00) made
    void MakeFlatBoxHull() RETAIL(FUN_00141210);
    // The camera the game shows (the state word's bits 19-22: 0 the played character's follow camera, 3 the game's rig, 4 the
    // cutscenes' rig) at once, set back to its start or not: the follow camera's instance (the view's camera then, its bit 17
    // cleared), the game's rig on the shown camera's lens (its target's end the player's position), the cutscenes' rig there
    // without followers or a target. Returns the camera's instance (none for another camera)
    InstanceContext* ShowCamera(GameController* controller, u32 camera, u32 reset) RETAIL(FUN_001759e0);
    // A blend to the follow camera's rig (set back first when asked) or the game's rig over a time (clock units) with a curve, on
    // the camera's lens (nothing for another camera)
    void BlendToCamera(GameController* controller, u32 camera, const s32* ticks, u32 reset, u32 curve) RETAIL(FUN_00175d10);
    // The chooser of a crate's contents (0x5D4 bytes into the game controller, which neither reads): whether the second contents
    // come out when the crate has both (never), and how many of the first (RandomFrom(least, most - least))
    u32 CrateGivesSecondContents(void* chooser) RETAIL(FUN_00179690);
    s32 CrateContentsCount(void* chooser, u32 least, u32 most) RETAIL(GetInstanceAmountSpawn);
    // The delayed frees' lists made empty and the skid marks' two materials made
    void InitFreedMemory() RETAIL(FUN_0015b9d0);
    // How many instances have a value at 0x174 above 0 (the scripts' commands 648 and 649 count them, condition 169 reads it)
    extern s32 g_InstancesWithValue174 RETAIL(D_0030A0FC);
    // A 0.8 second loop's frame (sixtieths) while the clock runs: the custom pickups' spin (game/pickups.cpp)
    void StepLoopFrame(TimeClock* clock) RETAIL(FUN_0011e428);
    // The sound's listener: the object the sounds are heard from (game/sound.h's ListenerVoiceKind)
    void SetSoundListener(ReferencedObject* object) RETAIL(FUN_001e5fa8);
    // The last frame's freed skid marks deleted, this frame's kept for the next
    void ReleaseFreedMemory() RETAIL(FUN_0015bc30);
    // The skid marks (the platform's screen models) to delete after this frame and the last, their counts, and the skid marks'
    // materials (adding where the shade is above 0, taking away otherwise; the platform's)
    extern struct ScreenModel* g_FreedBlocks[1024] RETAIL(D_003D1EF0);
    extern struct ScreenModel* g_LastFreedBlocks[1024] RETAIL(D_003D2EF0);
    extern s32 g_FreedBlockCount RETAIL(D_0030AABC);
    extern s32 g_LastFreedBlockCount RETAIL(D_0030AAC0);
    extern struct Material* g_AddingSkidMaterial RETAIL(D_00309924);
    extern struct Material* g_SubtractingSkidMaterial RETAIL(D_00309928);
}
