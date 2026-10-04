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

// Checkpoints (0x80 bytes of the heap): an instance play can start from again (the chunk it's in and its ID, 0xFFFF none), how the
// characters were paired and which they were when it was set, and where the characters start from it
struct Checkpoint
{
    enum Bits : u32
    {
        BitSet = 0x1,
        // Play started from it since it was set
        BitUsed = 0x2,
        // The pairing (bits 2-5), the character (6-9) and the second character (10-13), as the progress has them
        PairingShift = 2,
        CharacterShift = 6,
        SecondShift = 10,
        FieldMask = 0xF,
    };

    // In retail the low bits of a 64 bit word the chunk's string shares
    u32 bits;
    String chunk;
    u16 id;
    u8 unknown12[0x20 - 0x12];
    InstancePlacement character;
    InstancePlacement second;

    static Checkpoint* Construct(Checkpoint* checkpoint) RETAIL(FUN_0017a760);
    // Play starting from it: its instance's persistent flag set, and the progress's characters put where they start and told to
    // come back there (the second where the first does). Whether it's set (the pairing isn't read)
    bool Restore(u32 pairing, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_00171d00);
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

// The game's progress (the game controller's, made by its constructor): its first word's bits 0-6 the wumpa fruit, 7-13 the lives,
// 14-16 the most health, 17-19 the health; the play's state; the time played; the chunk play starts in; each level's gems and
// crystal; the timed play's time left; the characters' instances and three checkpoints
struct GameProgress
{
    enum Counts : u32
    {
        WumpaMask = 0x7F,
        LivesShift = 7,
        LivesMask = 0x7F,
        MostHealthShift = 14,
        HealthShift = 17,
        HealthMask = 7,
        // The timed play's count (whack-a-worm's) and its total
        CountTotalShift = 20,
        CountShift = 26,
        CountMask = 0x3F,
    };

    enum Bits : u32
    {
        // The play mode: 0 normal, 1 with health, 2 timed
        ModeMask = 0xF,
        // How the second character is paired with the first (1 alone), the character played (0 Crash) and the second one, 6
        // none
        PairingShift = 4,
        CharacterShift = 8,
        SecondShift = 12,
        FieldMask = 0xF,
        // The area play is in, the area the story has got to, the last area open
        AreaShift = 16,
        StoryShift = 21,
        OpenShift = 26,
        AreaMask = 0x1F,
    };

    static constexpr u32 NoCharacter = 6;
    static constexpr u32 Levels = 16;
    static constexpr u32 Characters = 6;
    static constexpr u32 Gems = 6;

    u32 counts;
    u32 bits;
    // The time played before the clock's time play started at (clock units; no start time: not counting)
    s32 timePlayed;
    s32 startTime;
    // The chunk the game starts in, and two more chunks' names (forgotten when play starts)
    String startChunk;
    String chunk1C;
    String chunk28;
    // Each level's word: bits 0-5 the gems found (blue, clear, green, purple, red, yellow), bit 8 its crystal
    u32 levels[Levels];
    // The health bar's length
    f32 barLength;
    // The timed play's time and the time left of it (clock units)
    s32 timeLimit;
    s32 timeLeft;
    // The characters' instances (by the character)
    Reference* characters[Characters];
    Checkpoint* checkpoints[3];

    static GameProgress* Construct(GameProgress* progress) RETAIL(FUN_00167720);
    // Reset for a way into the game (0 a new game and 1 its levels kept, from the start's checkpoint; 2 from the last checkpoint,
    // else the start's; 3 from any): the health and the timed play's time, every character's place to come back to, and what the
    // way starts over. Returns the checkpoint play starts from, the progress's pairing and characters its
    Checkpoint* Reset(u32 way, ChunkManager* chunks) RETAIL(FUN_00167a18);
    // Play entered a way (2: from a save's chunk and place, the start's checkpoint set there; else Reset's), the characters put
    // at the checkpoint it starts from (the other progress's). Returns the checkpoint
    Checkpoint* Enter(u32 way, String* chunk, u16* place, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_00167840);
    // Whether the start chunk is loaded as asked (1 the chunk, 2 with its links), waiting for it when asked
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

    u32 Field(u32 shift) const
    {
        return bits >> shift & FieldMask;
    }

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

// What a save holds of the game (the game controller's): its summary, the progress's areas and the options, the time played, the
// chunk and the place (a checkpoint's ID, 0xFFFF none) play starts from, the screen's position and the volumes, the save's data
// (the levels' words and the chunks' persistent flags) and the progress
struct SaveController
{
    static constexpr u32 DataSize = 0xF000;

    enum Summary : u32
    {
        SummaryLivesMask = 0x7F,
        SummaryCrystalsShift = 7,
        SummaryCrystalsMask = 0x3F,
        SummaryDoneShift = 13,
        SummaryDoneMask = 0x7F,
        // The progress's pairing and characters
        SummaryPairingShift = 20,
        SummaryCharacterShift = 24,
        SummarySecondShift = 28,
    };

    enum Options : u32
    {
        // Bits 0-14: the progress's areas (its bits 16-30)
        OptionAreas = 0x7FFF,
        OptionVibration = 0x8000,
        OptionWidescreen = 0x10000,
        OptionMusicStereoShift = 17,
        OptionMusicStereoMask = 0xF,
    };

    u32 summary;
    u32 options;
    // The time played (clock units)
    s32 timePlayed;
    String chunk;
    u16 place;
    u8 unknown1A[0x1C - 0x1A];
    // The screen's position, and the sound effects' and the music's volumes (volume groups 0 and 2)
    Vector2 screenOffset;
    f32 effectsVolume;
    f32 musicVolume;
    u8* data;
    GameProgress* progress;
    u8 unknown34[0x3C - 0x34];

    // Read from and written to a save's file
    void Read(Stream* stream) RETAIL(FUN_001680b8);
    void Write(Stream* stream) RETAIL(FUN_001681b0);
    // The game given what it holds: the progress's lives, areas, pairing, characters and time played (counted from now), the
    // levels' words and the chunks' persistent flags from its data when asked (every chunk unloaded first), and the options
    void Restore(u32 withData, GameProgress* progress, ChunkManager* chunks) RETAIL(FUN_001682a0);
    // What it takes of the game: the chunk the loading follows (the start chunk without one) and the checkpoint's ID, the
    // summary, the areas, the time played, the levels' words and the chunks' persistent flags, and the options
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
    // The area a script's token names (the global progression commands' argument), -1 for none
    s32 TokenArea(u32 token) RETAIL(FUN_00167df0);
    // A gem marked found in a level's word (its low byte): whether it wasn't before. The level's crystal (bit 8) the same way
    u32 MarkGem(u8* level, u32 gem) RETAIL(FUN_0017a2e8);
    u32 MarkCrystal(u32* level) RETAIL(FUN_0017a2c0);
    // A level's word made (no gem, no crystal), read from a stream and written to one (the progress's constructor and the save
    // controller have them inline)
    u32* ConstructLevelWord(u32* level) RETAIL(FUN_0017a220);
    void ReadLevelWord(u32* level, Stream* stream) RETAIL(FUN_0017a250);
    void WriteLevelWord(const u32* level, Stream* stream) RETAIL(FUN_0017a288);
    // The gem a keyword names (0x288 to 0x28D: 0 to 5), -1 for any other
    s32 TokenGem(u32 token) RETAIL(FUN_0017a318);
    // The player mode a script's token names (1 to 6, the player mode command's argument), 0 for none
    u32 TokenPlayerMode(u32 token) RETAIL(FUN_001798d8);
}
