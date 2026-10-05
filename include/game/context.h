#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/resources.h"
#include "game/string.h"

struct GameController;
struct GamePad;

// The engine's application class: Main starts it up and runs its frames, which call the game's side through the virtual
// functions (GameContext's are at the start of .text, the engine's around Main)
class GameContextPrototype
{
public:
    enum Slot : u32
    {
        LanguageChangedSlot = 1,
        StartGameSlot = 2,
        GameBeginFrameSlot = 3,
        GameUpdateSlot = 4,
        GameEndFrameSlot = 5,
        GameRenderSlot = 6,
        UpdateSlot = 7,
    };

    u8 unused00;
    f32 unused04;
    // What the screen is cleared to
    u32 clearColor;
    // The folder the chunks' loading counts the chunks under (none: nothing sets it)
    char* chunksPath;
    u32 unused10;
    void* unused14;
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
        CallVirtual<void>(this, vtable, LanguageChangedSlot, language);
    }

    void Update(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, UpdateSlot, playingMovie);
    }

    // The game's side
    void StartGame()
    {
        CallVirtual<void>(this, vtable, StartGameSlot);
    }

    void GameBeginFrame(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, GameBeginFrameSlot, playingMovie);
    }

    void GameUpdate(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, GameUpdateSlot, playingMovie);
    }

    void GameEndFrame(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, GameEndFrameSlot, playingMovie);
    }

    void GameRender(bool playingMovie)
    {
        CallVirtual<void>(this, vtable, GameRenderSlot, playingMovie);
    }
};
CHECK_SIZE(GameContextPrototype, 0x1C);
CHECK_OFFSET(GameContextPrototype, vtable, 0x18);

// The game context's flags: the colour filter on (set when it's made: the platform's g_ColourFilterEffect), "RB" given (the files
// queued for reading: the default chunk's RM2 and the front end's sounds, the chunks' links waited for), the default chunk's
// objects' resources not taken (also OLEG's start-up skipped and the chunks loaded all at once; nothing sets it), and the chunks'
// loading mode given (else LoadingStreamed; nothing sets it)
union GameContextFlags
{
    u32 value;
    struct
    {
        u32 unused0 : 3;
        u32 colourFilter : 1;
        u32 unused4 : 2;
        u32 queuesFiles : 1;
        u32 takesNoObjects : 1;
        u32 givesLoadingMode : 1;
        // ChunkLoadingMode
        u32 loadingMode : 4;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(GameContextFlags, 4);

// Made from the launch arguments: "RB" and "BATCH=<archive>" (the .BD/.BH pair the files are read from)
class GameContext : public GameContextPrototype
{
public:

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

    GameContextFlags flags;
    // The chunk the game controller starts in (nothing sets it)
    String startChunk;
    // Chunks the start-up loads one at a time before the game starts, from the last (nothing adds any)
    StringList preloadChunks;
    // Made 10 (the preloaded chunks' list's room), nothing reads it
    u32 unused3C;
    String archivePath;
    // The game's resources, the item builders the object builder asks (but the factory's three), the instance factory
    // (InstanceFactory, retail's GameResourceManager, whose last three words are the other three builders)
    GameResources resources;
    const GccVTableEntry* builders[Builders];
    u8 instanceFactory[0xD4 - 0xAC];
    const GccVTableEntry* moreBuilders[MoreBuilders];
    // When the game's first frame began (clock units)
    s32 firstFrameStamp;
    // The game's pad (the pad controller's first) and the second player's (nothing sets it)
    GamePad* mainPad;
    GamePad* secondPad;
    void* chunkManager;
    GameController* gameController;
    // The start-up makes the update rates of the object nodes and the model nodes of them (g_ObjectUpdateRate and
    // g_ModelUpdateRate): the cutoff's square root, a quarter of it the grace's
    u32 objectUpdateCutoffRoot;
    u32 modelUpdateCutoffRoot;

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
CHECK_OFFSET(GameContext, resources, 0x4C);
CHECK_OFFSET(GameContext, builders, 0x90);
CHECK_OFFSET(GameContext, instanceFactory, 0xAC);
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
    // A cutoff of none: updated however long unseen
    static constexpr u16 NoCutoff = 0xFFFF;
    // The rarest updates' mask (every 65536 frames), for a count of stamps unseen of none
    static constexpr u32 RarestMask = 0xFFFF;
    static constexpr u32 NoCount = 0xFFFFFFFF;

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
