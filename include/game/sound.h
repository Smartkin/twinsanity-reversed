#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"

class Stream;
struct FileStream;
struct SoundBankFiles;
struct TimeClock;
struct Matrix4x4;

// A sound of a bank
struct GameSound
{
    u32 bits;
    // Its number on the sound processor's side
    u32 soundId;
    // Bit 0: its samples are in the sound processor's memory
    u8 flags;
    u8 unknown09[3];
    u32 unknown0C;
    u32 unknown10;
    u32 unknown14;
    // The pitch it plays at
    u16 basePitch;
    u16 parameter1;
    u16 parameter2;
    u16 parameter3;
    u16 parameter4;
    u16 unknown22;
    s32 size;
    s32 offset;

    // Made empty (no ID), and made and read from a stream (still asm)
    static GameSound* ConstructEmpty(GameSound* sound) RETAIL(FUN_001e5410);
    static GameSound* Construct(GameSound* sound, Stream* stream) RETAIL(FUN_001e5498);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001e5528);
};
CHECK_SIZE(GameSound, 0x2C);

// A table of sounds by their ID (the front end's, the game's, a language's voices; the retail resource table of sounds, still
// asm): how many (bits 0-13), the sounds, the sounds' IDs in the order a section read them, and its vtable (1 the destructor,
// 2 every sound let go of, 3 nothing, 4 the samples of the sounds read queued for reading)
struct SoundTable
{
    u32 count;
    GameSound** sounds;
    u16* readOrder;
    u32 unknown0C;
    u32 unknown10;
    const GccVTableEntry* vtable;

    // With room for so many sounds
    static SoundTable* Construct(SoundTable* table, u32 count) RETAIL(FUN_00269b28);
};
CHECK_SIZE(SoundTable, 0x18);

// A request for music: the track (bits 0-15), its group (16-18), whether it starts at once (19) and loops (20), its left and
// right volumes and its fade time (seconds)
struct MusicRequest
{
    u32 bits;
    f32 left;
    f32 right;
    f32 fadeTime;
};
CHECK_SIZE(MusicRequest, 0x10);

// A voice of the sound processor as the game keeps it
struct SoundVoice
{
    enum Bits : u32
    {
        NumberMask = 0x1F,
        TakenByMusic = 0x20,
        Playing = 0x40,
        SecondCore = 0x80,
        ReverbSend = 0x100,
        // What it's used for (bits 9-13)
        UseMask = 0x3E00,
        Released = 0x4000,
    };

    u32 bits;
    f32 pitchScale;
    f32 volume;
    // Frames since it was released
    s32 frames;
    s32 left;
    s32 right;
    s32 pitch;
    f32 unknown1C;
};
CHECK_SIZE(SoundVoice, 0x20);

// A core of the sound processor: its reverb and its voices
struct SoundCore
{
    s32 reverbMode;
    // Bits 0-15 the core, 16-22 the reverb's delay, 23-29 its feedback
    u32 reverbBits;
    s32 reverbVolumeLeft;
    s32 reverbVolumeRight;
    s32 reverbDepthLeft;
    s32 reverbDepthRight;
    s32 unknown18;
    u32 voicesInUse;
    SoundVoice voices[24];
};
CHECK_SIZE(SoundCore, 0x320);

// Music streamed into one voice, or two when it's interleaved (stereo)
struct MusicPlayer
{
    enum Bits : u32
    {
        Interleaved = 0x1,
        // Bits 1-5
        StateMask = 0x3E,
        // Bits 6-8: the volume group it plays in
        GroupMask = 0x1C0,
        StartsAtOnce = 0x200,
        FadingOut = 0x400,
        Loops = 0x800,
    };

    enum State : u32
    {
        Stopped = 1,
        Preparing = 2,
        Ready = 3,
        FadingIn = 4,
        PlayingState = 5,
        FadingOutState = 6,
        Faded = 7,
        Stopping = 8,
        // Its buffer is the movie player's (LendMusicBuffer)
        LentToMovie = 9,
    };

    u32 bits;
    f32 fadeTime;
    f32 targetLeft;
    f32 targetRight;
    f32 fromLeft;
    f32 fromRight;
    f32 left;
    f32 right;
    u32 track;
    u32 rate;
    SoundVoice* voices[2];
    FileStream* stream;
    FileStream* second;
    // Where its buffers are in the sound processor's memory
    u32 location;
    u32 channelValue;
};
CHECK_SIZE(MusicPlayer, 0x40);

struct MusicSystem
{
    MusicPlayer* playing[4];
    MusicPlayer* fading[4];
    MusicPlayer players[3];
};
CHECK_SIZE(MusicSystem, 0xE0);

// A volume group's volumes in 4.12 fixed point
struct GroupVolume
{
    s16 right;
    s16 left;
};

extern "C"
{
    extern SoundCore g_SoundCores[2] RETAIL(D_003B14C0);
    extern MusicSystem* g_Music RETAIL(G_UnkStructOfSize_0xe0);
    extern GroupVolume g_GroupVolumes[4] RETAIL(D_003D6F20);
    // A movie is playing: the sound is left alone
    extern u8 g_MovieSound RETAIL(D_00309E5C);
    // Every pitch is scaled by it
    extern f32 g_PitchScale RETAIL(D_00309E74);
    // The music's voices get a side each, else both sides
    extern u32 g_MusicStereo RETAIL(D_00309E68);
    // The fade of every group's volume: its length and how far it got (1 to 0)
    extern f32 g_GroupFadeTime RETAIL(D_00309E60);
    extern f32 g_GroupFade RETAIL(D_00309E64);
    extern f32 g_SoundDistance RETAIL(D_0030AC94);
    extern s32 g_SoundFrames RETAIL(D_0030A898);
    extern u8 g_SoundRequestPending RETAIL(D_00309E80);
    extern u32 g_SoundRequest RETAIL(D_00309E7C);
    extern u8 g_SoundSlots[4] RETAIL(D_0030AC90);

    s32 VoiceNumber(const SoundVoice* voice);
    // The volume group's number (groups are numbered from 1, in the top half)
    u32 SoundGroupOf(s32 group) RETAIL(FUN_001e6580);

    void SetReverbVolume(f32 left, f32 right, SoundCore* core) RETAIL_N32(FUN_001e60b0);
    void SetReverbDepth(f32 left, f32 right, SoundCore* core) RETAIL_N32(FUN_001e6108);
    void SetReverb(SoundCore* core, s32 mode, u32 delay, u32 feedback) RETAIL(FUN_001e6138);
    // The chunk's reverb: a type (0 to 6) and three values of 0 to 1
    void SetReverbFromSettings(SoundCore* core, const u8* settings) RETAIL(FUN_001df8f0);
    void ClearReverbs() RETAIL(FUN_001e6a98);
    void RestoreReverbs() RETAIL(FUN_001e6ac0);

    // Returns whether it's still playing
    u32 UpdateVoice(SoundVoice* voice) RETAIL(FUN_001e63e0);
    s32 SetVoicePitch(SoundVoice* voice, s32 pitch) RETAIL(FUN_001e6498);
    s32 SetVoiceVolume(f32 left, f32 right, SoundVoice* voice) RETAIL_N32(FUN_001e64f0);
    void SetVoiceReverb(SoundVoice* voice, s32 on) RETAIL(FUN_001e6620);
    // Plays the sound on the voice at a volume and a pitch (0 for the sound's own). Returns the voice's number
    s32 PlaySoundOnVoice(f32 volume, f32 pitchScale, SoundVoice* voice, GameSound* sound, u32 group, s32 last)
        RETAIL_N32(FUN_001dfa38);
    // The sound played on a free voice (of the kind, its reverb set by it) in the group, at a volume and a pitch (-1 for the
    // sound's own). Returns whether it plays (not when the sound isn't loaded or no voice is free)
    u32 PlaySound(f32 volume, f32 pitchScale, GameSound* sound, s32 group, s32 voiceKind, s32 last) RETAIL_N32(FUN_001e5d98);
    // Every group's volume faded out over a time (seconds)
    void FadeSoundGroups(f32 time) RETAIL(FUN_001e68a8);
    // The sound of an ID: the game's table's, else the language's voices' (none past the game's table) (still asm)
    GameSound* SoundById(u16 id) RETAIL(GetSoundAddress);
    // The sound of an ID played the same way, 0 without one
    u32 PlaySoundById(f32 volume, f32 pitchScale, u32 id, s32 group, s32 voiceKind, s32 last) RETAIL_N32(FUN_001e5d10);
    // At a place: its volumes and pitch are worked out from where it is. Returns the voice's number, -1 when it can't be heard
    s32 PlaySoundAt(f32 volume, f32 pitchScale, SoundVoice* voice, GameSound* sound, u32 group, Matrix4x4* place, s32 last)
        RETAIL_N32(FUN_001dfbc0);
    void FreeSoundSamples(u32 unused, u32 sound) RETAIL(FUN_001e6b38);

    void PauseSound() RETAIL(FUN_001e65e0);
    void ResumeSound() RETAIL(FUN_001e6600);
    void ResetSound() RETAIL(FUN_001e66a8);
    void SetGroupVolume(f32 left, f32 right, s32 group) RETAIL_N32(FUN_001e6778);
    // Stops every sound and the music, the reverbs and the groups' fades
    void StopAllSound() RETAIL(FUN_001e0560);
    // Returns the movie's flag while a movie plays, else what a pending sound request returned (0 without one)
    u32 UpdateSound(s32 paused, TimeClock* clock) RETAIL(FUN_001e0768);
    MusicSystem* ConstructMusic(MusicSystem* music) RETAIL(FUN_001dfde8);

    // Music
    // A music request played in a slot (0 the main one) (still asm)
    s32 PlayMusicRequest(u32 slot, const MusicRequest* request) RETAIL(FUN_001ddff0);
    // The music's bank (Crash6\Music) and the language's voices' (Crash6\<language>)
    extern SoundBankFiles g_MusicBank RETAIL(D_0030A8A8);
    extern SoundBankFiles g_VoiceBank RETAIL(D_0030A8B0);
    // The music's bank read, the music players' buffers made for it again
    void SetMusicBank(const char* name) RETAIL(FUN_001e6a10);
    void SetVoiceBank(const char* name) RETAIL(FUN_001e6a70);
    void CreateMusicBuffers(MusicPlayer* player, SoundBankFiles* bank) RETAIL(FUN_001e6d48);
    void StartMusic(MusicPlayer* player, SoundBankFiles* bank, const u32* track) RETAIL(FUN_001e10f8);
    s32 PlayMusic(f32 left, f32 right, f32 fadeTime, MusicPlayer* player) RETAIL_N32(FUN_001e6fc0);
    void StopMusic(MusicPlayer* player) RETAIL(FUN_001e7060);
    s32 SetMusicPitch(f32 scale, MusicPlayer* player) RETAIL_N32(FUN_001e6e78);
    s32 SetMusicVolume(f32 left, f32 right, MusicPlayer* player) RETAIL_N32(FUN_001e14b8);
    // Returns whether it's doing anything
    bool UpdateMusic(f32 time, MusicPlayer* player, s32 paused) RETAIL_N32(FUN_001e1730);
    // A movie streams through a music player's buffer: one lent already, else a stopped one, else the one the loudest sound plays
    // on, stopped. Returns the buffer, nullptr for none
    void* LendMusicBuffer() RETAIL(FUN_001e0940);
    // Every player lent to the movie is stopped again
    void ReclaimMusicBuffers() RETAIL(FUN_001e69c0);
    // The volume group's level: the mean of its sides' volumes
    f32 GroupVolumeLevel(s32 group) RETAIL(FUN_001e6838);
    // The music's stereo mode (mono, stereo, Dolby Pro Logic II), the music players playing started again with it (still asm)
    void SetMusicStereo(u32 mode) RETAIL(FUN_001e6920);
    // The sound groups' volumes in cutscenes: 0 their own, 1 fading to the cutscenes' levels, 2 at them, 3 fading back; and how
    // much of the fade is left (1 to 0). A cutscene's start and end start the fades
    extern s32 g_CutsceneVolumeMode RETAIL(D_00309E6C);
    extern f32 g_CutsceneVolumeFade RETAIL(D_00309E70);
    void FadeToCutsceneVolumes() RETAIL(FUN_001e6068);
    void FadeFromCutsceneVolumes() RETAIL(FUN_001e6080);
}
