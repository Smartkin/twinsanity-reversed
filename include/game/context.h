#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/string.h"

struct GameController;

// The engine's application class: Main starts it up and runs its frames, which call the game's side through the virtual
// functions (GameContext's are at the start of .text, the engine's around Main)
class GameContextPrototype
{
public:
    u8 unknown00;
    f32 unknown04;
    // What the screen is cleared to
    u32 clearColor;
    char* unknown0C;
    u32 unknown10;
    void* unknown14;
    const GccVTableEntry* vtable;

    static GameContextPrototype* Construct(GameContextPrototype* context) RETAIL(FUN_001816f8);

    // Makes the renderer and starts the game, the first time
    void StartUp() RETAIL(FUN_00181880);
    // Pauses or resumes what the game state asks for, ticks the clocks and the pads, and begins the game's frame (the movie
    // playing is the movie controller's to say: the argument is unused)
    void BeginFrame(bool playingMovie) RETAIL(FUN_0017d9a8);
    void EndFrame(bool playingMovie) RETAIL(FUN_00181968);
    // The game's drawing, the loading done in the frame's time left, and the renderer's or the movie's frame
    void Render(bool playingMovie) RETAIL(MainRender);

    // Its vtable's: the language changed (slot 1, nothing here), and Update (slot 7): the video, the chunks and the renderer's
    // update around the game's
    void LanguageChangedPrototype(u32 language) RETAIL(FUN_001816f0);
    void UpdatePrototype(bool playingMovie) RETAIL(FUN_0017db88);

    void LanguageChanged(u32 language)
    {
        CallVirtual<void>(this, vtable, 1, language);
    }

    void Update(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, 7, playingMovie);
    }

    // The game's side
    void StartGame()
    {
        CallVirtual<void>(this, vtable, 2);
    }

    void GameBeginFrame(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, 3, playingMovie);
    }

    void GameUpdate(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, 4, playingMovie);
    }

    void GameEndFrame(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, 5, playingMovie);
    }

    void GameRender(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, 6, playingMovie);
    }
};
CHECK_SIZE(GameContextPrototype, 0x1C);
CHECK_OFFSET(GameContextPrototype, vtable, 0x18);

// Made from the launch arguments: "RB" and "BATCH=<archive>" (the .BD/.BH pair the files are read from)
class GameContext : public GameContextPrototype
{
public:
    enum Flags : u32
    {
        // Set when it's made
        Flag3 = 0x8,
        // "RB" was given
        FlagRb = 0x40,
    };

    // The object builder's item builders (a vtable alone each), by the kinds of items they make
    enum Builder : u32
    {
        BuilderObjects,
        BuilderBehaviours,
        BuilderCommands,
        BuilderConditions,
        BuilderMaths,
        BuilderModels,
        BuilderScenery,
        Builders,
    };

    enum MoreBuilder : u32
    {
        BuilderCameras,
        BuilderSounds,
        BuilderChunkLinks,
        MoreBuilders,
    };

    // Bit 3 ..., 6 "RB", 7 ..., 8 the chunks' loading starts in the state of bits 9-12
    u32 flags;
    // The chunk the game controller starts in (nothing sets it)
    String startChunk;
    // Chunks the start-up loads one at a time before the game starts, from the last (nothing adds any)
    StringList preloadChunks;
    // Made 10, nothing reads it
    u32 unknown3C;
    String archivePath;
    // The game's resources (GameResources), the item builders the object builder asks (but the factory's three), the instance
    // factory (InstanceFactory) and the other three
    u8 resources[0x90 - 0x4C];
    const GccVTableEntry* builders[Builders];
    u8 resourceManager[0xD4 - 0xAC];
    const GccVTableEntry* moreBuilders[MoreBuilders];
    // When the game's first frame began (clock units)
    s32 firstFrameStamp;
    void* mainPad;
    u32 unknownE8;
    void* chunkManager;
    GameController* gameController;
    // The start-up makes the two update rates of them (g_ObjectUpdateRate and g_ModelUpdateRate)
    u32 unknownF4;
    u32 unknownF8;

    // Made empty: the game's resources, the instance factory and the item builders (the object builder made the first time and
    // given them), the update rates' bytes 100, the module statics set (the AgentLab's, the projectiles' hull, the particles'
    // emitters)
    static GameContext* Construct(GameContext* context) RETAIL(GameContextConstructor);

    // Its vtable's slot 1: the language changed, the video's texts and the save code's messages set (their texts the same in
    // every language)
    void LanguageChanged(u32 language) RETAIL(FUN_001004c8);
    // Its vtable's slot 2: makes the game's systems (the readers, the chunks, the game controller, the movies and the video)
    // and picks the language
    void StartGame() RETAIL(FUN_00100698);
    // Its vtable's slots 3, 4 and 6: the game controller's frame begun; its update (while a movie plays, with the frame's freed
    // memory kept), the frame's seconds set for the scripts and the chunks' loading following what the game's focused on; and
    // its frame ended
    void GameBeginFrame(bool playingMovie) RETAIL(FUN_001014e0);
    void GameUpdate(bool playingMovie) RETAIL(FUN_00100b98);
    void GameRender(bool playingMovie) RETAIL(FUN_00101500);
    // Its vtable's slot 5: the game controller's view set and every renderer's scene drawn, then (unless a movie plays) the frame's
    // particles and decals (aged with VU0's third set of microcode) and the game controller's drawing
    void GameEndFrame(bool playingMovie) RETAIL(FUN_00100cc8);
    // The chunks to preload, with the loading in state 1 (no linked chunks queued) and the game controller's global resources
    // loaded: each made the start chunk, loaded and waited for, then forgotten. The characters enabled after
    void PreloadChunks() RETAIL(FUN_00100100);
};
CHECK_SIZE(GameContext, 0xFC);
CHECK_OFFSET(GameContext, startChunk, 0x20);
CHECK_OFFSET(GameContext, preloadChunks, 0x2C);
CHECK_OFFSET(GameContext, builders, 0x90);
CHECK_OFFSET(GameContext, moreBuilders, 0xD4);
CHECK_OFFSET(GameContext, archivePath, 0x40);
CHECK_OFFSET(GameContext, firstFrameStamp, 0xE0);
CHECK_OFFSET(GameContext, mainPad, 0xE4);

enum GameState : s32
{
    GameStateStarting = 0,
    GameStateRunning = 1,
    GameStatePausing = 2,
    GameStatePaused = 3,
    GameStateResuming = 4,
    GameStateQuitting = 5,
};

extern "C"
{
    extern GameContext* g_GameContext;
    extern GameState g_GameState RETAIL(G_GameState_);
    // The game paused (running: pausing; resuming: paused) and let go again (pausing: running; paused: resuming)
    void PauseGame() RETAIL(FUN_00181b90);
    void ResumeGame() RETAIL(FUN_00181bc8);

    GameContext* CreateGameContext(u32 argumentCount, char** arguments);
    GameContext* ParseArguments(u32 argc, char** argv);
    // Whether the argument starts with the option
    bool ContextIsOption(GameContext* context, const char* argument, const char* option) RETAIL(StringEqualToRB);
    // "OPTION=value": the value, in upper case
    bool ContextGetOptionValue(GameContext* context, const char* argument, const char* option, String* value)
        RETAIL(GetArchivePath);

    // The work a frame does in the time it has left: the sound driver, the chunks' loading, the resources, and the disk
    // manager's compaction and releases. Without a budget, a step of each. Returns whether there was work
    bool BackgroundWork(bool budgeted) RETAIL(FUN_0017ddc8);
}

// How often what wasn't seen for a while is updated: within a grace of stamps every frame, then every 2^n frames, n growing by the
// slope with the stamps past the grace, and not at all from the cutoff on
struct UpdateRate
{
    u16 grace;
    u16 cutoff;
    f32 slope;
};
CHECK_SIZE(UpdateRate, 8);

extern "C"
{
    // A rate of a grace and a cutoff given as their square roots (the grace's square, the cutoff's less it) and the steps
    // spread over what's past the grace (the slope that many over the cutoff plus 1)
    void MakeRate(UpdateRate* rate, u8 graceRoot, u8 cutoffRoot, u32 steps) RETAIL(FUN_001011d8);
}

extern "C"
{
    // The object nodes' update rate, and the model nodes'
    extern UpdateRate g_ObjectUpdateRate RETAIL(D_0030A948);
    extern UpdateRate g_ModelUpdateRate RETAIL(D_0030A838);
}
