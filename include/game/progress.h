#pragma once

#include "abi.h"
#include "common.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/reference.h"
#include "game/string.h"

struct ChunkManager;
struct GameProgress;
class Stream;

// How the second character is paired with the first (the progress's, a checkpoint's and a save's pairing): alone, the second on
// the Humiliskate or the Rollerbrawl with the first, the two tied together, 5 (only the scripts set it and test it: SetPlayerMode
// and PairingIs5Condition), on the hoverboard, and on it with its controls
enum Pairing : u32
{
    PairingAlone = 1,
    PairingHumiliskate = 2,
    PairingRollerbrawl = 3,
    PairingTied = 4,
    Pairing5 = 5,
    PairingHoverboard = 6,
    PairingHoverboardControls = 7,
};

// The ways into the game (the progress's Reset and Enter, the game controller's entry): a new game, a new game from the start's
// checkpoint (nothing asks for it), from a save's chunk and place (Enter; Reset, also the game over's continue: from the saved
// checkpoint, else the start's) and from the latest checkpoint of any (the scripts' RestartFromCheckpoint). The first two drop
// the chunks' instances
enum GameEntry : u32
{
    EntryNewGame = 0,
    EntryNewGameFromStart = 1,
    EntrySaved = 2,
    EntryCheckpoint = 3,
};

// The play modes: normal, with a health bar (boss mode) and timed with a count (whack-a-worm)
enum PlayMode : u32
{
    PlayNormal = 0,
    PlayHealth = 1,
    PlayTimed = 2,
};

// A level's gems (its progress's bits, the order of TokenGem's keywords)
enum Gem : u32
{
    GemBlue = 0,
    GemClear = 1,
    GemGreen = 2,
    GemPurple = 3,
    GemRed = 4,
    GemYellow = 5,
};

// A checkpoint's bits: set, play started from it since it was set, and the progress's pairing, character and second character
// when it was set (in retail the low bits of a 64 bit word the chunk's string shares)
union CheckpointBits
{
    u32 value;
    struct
    {
        u32 set : 1;
        u32 used : 1;
        u32 pairing : 4;
        u32 character : 4;
        u32 second : 4;
        u32 unused14 : 18;
    };
};
CHECK_SIZE(CheckpointBits, 4);

// Checkpoints (0x80 bytes of the heap): an instance play can start from again (the chunk it's in and its ID, NoInstanceId none),
// how the characters were paired and which they were when it was set, and where the characters start from it
struct Checkpoint
{
    CheckpointBits bits;
    String chunk;
    u16 id;
    u8 unused12[0x20 - 0x12];
    InstancePlacement character;
    InstancePlacement second;

    static Checkpoint* Construct(Checkpoint* checkpoint) RETAIL(FUN_0017a760);
    // Play starting from it a way into the game: its instance's persistent flag set, and the progress's characters put where they
    // start and told to come back there (the second where the first does). Whether it's set (the way isn't read)
    bool Restore(u32 way, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_00171d00);
    // Set at the instance for the pairing and the characters (the chunk the loading follows taken as its chunk); when it's at the
    // instance already, only whether it hasn't been used
    bool Set(u32 pairing, ChunkManager* chunks, InstanceContext* instance, InstanceContext* character, InstanceContext* second)
        RETAIL(FUN_00171ee0);
    // The instance it was at (unless it's the one kept) told it isn't any more, and it's forgotten when asked. Whether there was
    // such an instance
    bool Release(u32 forget, ChunkManager* chunks, InstanceContext* keep) RETAIL(FUN_0017a898);
    // Forgotten, its instance's persistent flag cleared (the way and the progress aren't read)
    void Clear(u32 way, GameProgress* progress, ChunkManager* chunks) RETAIL(ClearInstancePersistentFlag);
};
CHECK_OFFSET(Checkpoint, character, 0x20);
CHECK_SIZE(Checkpoint, 0x80);

// The progress's counts: the wumpa fruit, the lives, the health play's most health and health, and the timed play's (whack-a-
// worm's) total and count
union ProgressCounts
{
    u32 value;
    struct
    {
        u32 wumpa : 7;
        u32 lives : 7;
        u32 mostHealth : 3;
        u32 health : 3;
        u32 countTotal : 6;
        u32 count : 6;
    };
};
CHECK_SIZE(ProgressCounts, 4);

// The play's state (the progress's): the play mode (PlayMode), how the second character is paired with the first (Pairing), the
// character played and the second (PlayableCharacter, GameProgress::NoCharacter none), the area play is in, the area the story
// has got to and the last area open
union PlayState
{
    u32 value;
    struct
    {
        u32 mode : 4;
        u32 pairing : 4;
        u32 character : 4;
        u32 second : 4;
        u32 area : 5;
        u32 story : 5;
        u32 open : 5;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(PlayState, 4);

// A level's progress: the gems found (a bit each, Gem; the code reads them as the word's low byte) and its crystal
union LevelProgress
{
    u32 value;
    struct
    {
        u32 gems : 8;
        u32 crystal : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(LevelProgress, 4);

// The game's progress (the game controller's, made by its constructor): its counts; the play's state; the time played; the chunk
// play starts in; each level's gems and crystal; the timed play's time left; the characters' instances and three checkpoints
struct GameProgress
{
    // The checkpoints: the last one a script set without saving, the one play started from (a new game's or a loaded save's) and
    // the last one a script saved at
    enum CheckpointSlot : u32
    {
        CheckpointRespawn = 0,
        CheckpointStart = 1,
        CheckpointSaved = 2,
        Checkpoints = 3,
    };

    // How ChunkLoaded waits for the start chunk: every loader under the loading's path loaded (only once asked again), the chunk,
    // the chunk and its links
    enum LoadedHow : u32
    {
        LoadedAll = 0,
        LoadedChunk = 1,
        LoadedWithLinks = 2,
    };

    static constexpr u32 NoCharacter = 6;
    static constexpr u32 Levels = 16;
    static constexpr u32 Characters = 6;
    static constexpr u32 Gems = 6;
    // A new game's lives
    static constexpr u32 StartLives = 5;

    ProgressCounts counts;
    PlayState play;
    // The time played before the clock's time play started at (clock units; no start time: not counting)
    s32 timePlayed;
    s32 startTime;
    // The chunk the game starts in, and two more chunks' names (only emptied)
    String startChunk;
    String unused1C;
    String unused28;
    LevelProgress levels[Levels];
    // The health bar's length
    f32 barLength;
    // The timed play's time and the time left of it (clock units)
    s32 timeLimit;
    s32 timeLeft;
    // The characters' instances (by the character)
    Reference* characters[Characters];
    Checkpoint* checkpoints[Checkpoints];

    static GameProgress* Construct(GameProgress* progress) RETAIL(FUN_00167720);
    // Reset for a way into the game (GameEntry: a new game, the start's checkpoint set again or, the second way, restored; from the
    // saved checkpoint, else the start's; from the respawn checkpoint, else the saved one, else the start's): the health and the
    // timed play's time, every character's place to come back to, and what the way starts over (a new game's counts, areas,
    // levels and pairing, the wumpa fruit from a save). Returns the checkpoint play starts from, the progress's pairing and
    // characters its
    Checkpoint* Reset(u32 way, ChunkManager* chunks) RETAIL(FUN_00167a18);
    // Play entered a way (EntrySaved: from a save's chunk and place, the start's checkpoint at the character played and the saved
    // one at the place; else Reset's), the characters put at the checkpoint it starts from (the other progress's). Returns the
    // checkpoint
    Checkpoint* Enter(u32 way, String* chunk, u16* place, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_00167840);
    // Whether the start chunk is loaded as asked (LoadedHow), waiting for it when asked
    u32 ChunkLoaded(u32 how, u32 wait) RETAIL(FUN_00167ed8);
    // How many levels have the gem
    s32 GemsFound(u32 gem) RETAIL(FUN_001796c0);
    // How much of the game is done (percent): the story's share where it has got to, and a third of the gems found
    u32 Done() RETAIL(FUN_00179700);
    // The time played, counted from now
    void SetTimePlayed(s32 time) RETAIL(FUN_00179798);
    // The health changed by an amount, kept between none and the most: whether it got to either. The timed play's count the
    // same way, between none and its total
    u32 AddHealth(s32 amount) RETAIL(FUN_001797d0);
    u32 AddToCount(s32 amount) RETAIL(FUN_00179860);
    // The start chunk's and the two more chunks' names emptied
    void ForgetChunks() RETAIL(FUN_00179938);
    // With a start chunk, every chunk unloaded and the start chunk queued. Whether there's one
    u32 LoadStartChunk(ChunkManager* chunks) RETAIL(FUN_001799a8);

    // A character's instance (none for no character)
    InstanceContext* Instance(u32 character) const
    {
        if (character == NoCharacter)
        {
            return nullptr;
        }

        Reference* reference = characters[character];
        return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
    }
};
CHECK_OFFSET(GameProgress, levels, 0x34);
CHECK_OFFSET(GameProgress, characters, 0x80);
CHECK_SIZE(GameProgress, 0xA4);

// What a save keeps of the progress: the lives, the levels' crystals, how much of the game is done (percent), the pairing, the
// character and the second character
union SaveControllerSummary
{
    u32 value;
    struct
    {
        u32 lives : 7;
        u32 crystals : 6;
        u32 done : 7;
        u32 pairing : 4;
        u32 character : 4;
        u32 second : 4;
    };
};
CHECK_SIZE(SaveControllerSummary, 4);

// A save's options: the progress's areas (the area play is in, the story's and the last one open), the vibration, the widescreen
// TV and the music's stereo mode
union SaveControllerOptions
{
    u32 value;
    struct
    {
        u32 area : 5;
        u32 story : 5;
        u32 open : 5;
        u32 vibration : 1;
        u32 widescreen : 1;
        u32 musicStereo : 4;
        u32 unused21 : 11;
    };
};
CHECK_SIZE(SaveControllerOptions, 4);

// What a save holds of the game (the game controller's): its summary, the progress's areas and the options, the time played, the
// chunk and the place (a checkpoint's ID, NoInstanceId none) play starts from, the screen's position and the volumes,
// the save's data (the levels' progress and the chunks' persistent flags) and the progress
struct SaveController
{
    static constexpr u32 DataSize = 0xF000;

    SaveControllerSummary summary;
    SaveControllerOptions options;
    // The time played (clock units)
    s32 timePlayed;
    String chunk;
    u16 place;
    u8 unused1A[0x1C - 0x1A];
    // The screen's position, and the sound effects' and the music's volumes (volume groups 0 and 2)
    Vector2 screenOffset;
    f32 effectsVolume;
    f32 musicVolume;
    u8* data;
    GameProgress* progress;
    u8 unused34[0x3C - 0x34];

    // Read from and written to a save's file
    void Read(Stream* stream) RETAIL(FUN_001680b8);
    void Write(Stream* stream) RETAIL(FUN_001681b0);
    // The game given what it holds: the progress's lives, areas, pairing, characters and time played (counted from now), the
    // levels' progress and the chunks' persistent flags from its data when asked (every chunk unloaded first), and the options
    void Restore(u32 withData, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_001682a0);
    // What it takes of the game: the chunk the loading follows (the start chunk without one) and the checkpoint's ID, the
    // summary, the areas, the time played, the levels' progress and the chunks' persistent flags, and the options
    void Take(GameProgress* progress, ChunkManager* chunks, Checkpoint* checkpoint) RETAIL(FUN_001684f0);
    // The options taken from the game: the vibration, the volumes, the music's stereo mode, the widescreen TV and the screen's
    // position
    void TakeOptions() RETAIL(FUN_00179a10);
};
CHECK_OFFSET(SaveController, screenOffset, 0x1C);
CHECK_OFFSET(SaveController, data, 0x2C);
CHECK_SIZE(SaveController, 0x3C);

extern "C"
{
    // The area a script's keyword names (0x26F to 0x286; the play area commands' argument), -1 for none
    s32 TokenArea(u32 token) RETAIL(FUN_00167df0);
    // A gem marked found in a level's progress (its gems' byte): whether it wasn't before. The level's crystal the same way
    u32 MarkGem(u8* level, u32 gem) RETAIL(FUN_0017a2e8);
    u32 MarkCrystal(u32* level) RETAIL(FUN_0017a2c0);
    // A level's progress made (no gem, no crystal), read from a stream and written to one (the progress's constructor and the
    // save controller have them inline)
    u32* ConstructLevelWord(u32* level) RETAIL(FUN_0017a220);
    void ReadLevelWord(u32* level, Stream* stream) RETAIL(FUN_0017a250);
    void WriteLevelWord(const u32* level, Stream* stream) RETAIL(FUN_0017a288);
    // The gem a keyword names (0x288 to 0x28D: Gem's), -1 for any other
    s32 TokenGem(u32 token) RETAIL(FUN_0017a318);
    // The pairing a script's keyword names (0x291 to 0x296: 1 to 6, the SetPlayerMode command's first argument), 0 for none
    u32 TokenPlayerMode(u32 token) RETAIL(FUN_001798d8);
}
