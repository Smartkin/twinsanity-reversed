#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/string.h"
#include "platform/audio.h"

class Stream;
struct FileStream;
struct SoundBankFiles;
struct TimeClock;
struct Matrix4x4;
struct Vector4;
struct ChunkData;
struct InstanceContext;
struct Reference;
struct DeletionQueue;

// The sound processor's volume groups: the effects volume sets 0, 1 and 3, the music volume 2 (a movie's sound and the cutscenes'
// music play in 3)
enum VolumeGroup : s32
{
    EffectsGroup = 0,
    SecondEffectsGroup = 1,
    MusicGroup = 2,
    MovieGroup = 3,
    VolumeGroupCount = 4,
};

// The SPU2's reverb modes (libsd's SD_REV_MODE_*): a core's, and the voice kind sounds play in (ReverbOff any core)
enum ReverbMode : s32
{
    ReverbOff = 0,
    ReverbRoom = 1,
    ReverbStudioA = 2,
    ReverbStudioB = 3,
    ReverbStudioC = 4,
    ReverbHall = 5,
    ReverbSpace = 6,
    ReverbEcho = 7,
    ReverbDelay = 8,
    ReverbPipe = 9,
};

// The music's slots (PlayMusicRequest), each playing one music player: the main music's, the context music's (the cutscenes'
// too) and the one whose player a new track takes when none is free
enum MusicSlot : u32
{
    MainMusicSlot = 0,
    ContextMusicSlot = 1,
    SpareMusicSlot = 2,
    MusicSlotCount = 4,
};

// How sounds are heard (g_MusicStereo, the options' and the saves'): the same on both sides, each side what's ahead of it, or
// panned around the listener (Dolby Pro Logic II)
enum StereoMode : u32
{
    StereoOff = 0,
    StereoSides = 1,
    StereoProLogic2 = 2,
};

// How far sounds are heard without a range of their own: their volume falls by their whole level over it
constexpr f32 DefaultSoundRange = 60.0f;
// The level a sound played at a place starts from, at most
constexpr f32 LoudestLevel = 1.5f;
// A volume or pitch scale that leaves the sound's own
constexpr f32 OwnScale = -1.0f;
// The sound processor's cores
constexpr s32 SoundCoreCount = Platform::Audio::Cores;

// A sound's flags
union GameSoundFlags
{
    u8 value;
    struct
    {
        // Its samples are in the sound processor's memory (its sound bank reader's)
        u8 loaded : 1;
        u8 unused1 : 7;
    };
};
CHECK_SIZE(GameSoundFlags, 1);

// A sound's ID of none (a slot without one)
constexpr u16 NoSoundId = 0xFFFF;

// A sound of a bank: its resource header (game/resources.h's ResourceHeader: its ID is its number on the sound processor's
// side), its flags, name, pitch and parameters, and its samples' size and offset in its bank
struct GameSound
{
    u32 resourceBits;
    u32 soundId;
    GameSoundFlags flags;
    u8 unused09[3];
    String name;
    // The pitch it plays at
    u16 basePitch;
    // Handed to the sound processor with every play (the low byte of the play's word)
    u16 parameter1;
    // Read with the pitch, never used
    u16 unused1C;
    u16 unused1E;
    u16 unused20;
    u16 unused22;
    s32 size;
    s32 offset;

    // Made empty (no ID), and made and read from a stream
    static GameSound* ConstructEmpty(GameSound* sound) RETAIL(FUN_001e5410);
    static GameSound* Construct(GameSound* sound, Stream* stream) RETAIL(FUN_001e5498);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001e5528);
    // Its header word (dropped), pitch and parameters, size and offset in its bank read
    void Read(Stream* stream) RETAIL(FUN_001e5580);
};
CHECK_SIZE(GameSound, 0x2C);

// A table of sounds by their ID (the front end's, the game's, a language's voices; the retail resource table of sounds,
// game/resources.h's ResourceTable): its capacity (bits 0-13) and bit 15, the sounds, the sounds' IDs in the order a section read
// them, its deletion queue and its vtable (1 the destructor, 2 every sound let go of, 3 nothing, 4 the samples of the sounds read
// queued for reading)
struct SoundTable
{
    u32 bits;
    GameSound** sounds;
    u16* order;
    u32 unused0C;
    DeletionQueue* queue;
    const GccVTableEntry* vtable;

    // With room for so many sounds
    static SoundTable* Construct(SoundTable* table, u32 count) RETAIL(FUN_00269b28);
};
CHECK_SIZE(SoundTable, 0x18);

// A music request's bits: the track, its volume group, whether it plays at once when it's prepared (else the slot holds it
// prepared until PlayPreparedMusic) and whether it loops
union MusicRequestBits
{
    u32 value;
    struct
    {
        u32 track : 16;
        u32 group : 3;
        u32 startsAtOnce : 1;
        u32 loops : 1;
        u32 unused21 : 11;
    };
};
CHECK_SIZE(MusicRequestBits, 4);

// A request for music: its bits, its left and right volumes and its fade time (seconds)
struct MusicRequest
{
    MusicRequestBits bits;
    f32 left;
    f32 right;
    f32 fadeTime;
};
CHECK_SIZE(MusicRequest, 0x10);

// A voice's bits: its number in its core, whether a music player has it, whether it plays, its core, whether it sends to its
// core's reverb, what it's used for (SoundVoice::Use) and whether it was silenced to be let go of
union SoundVoiceBits
{
    u32 value;
    struct
    {
        u32 number : 5;
        u32 takenByMusic : 1;
        u32 playing : 1;
        u32 secondCore : 1;
        u32 reverbSend : 1;
        u32 use : 5;
        u32 released : 1;
        u32 unused15 : 17;
    };
};
CHECK_SIZE(SoundVoiceBits, 4);

// A voice of the sound processor as the game keeps it: its bits, the pitch scale and volume it was played with (-1 for the
// sound's own), the frames it has played (a silenced voice is let go of once it played more than 10), its volumes and pitch, and
// the angle around the listener it was last heard at (SoundAtPlace's)
struct SoundVoice
{
    // What a voice is used for: a voice used for nothing can be taken from what it plays
    enum Use : u32
    {
        UseNone = 0,
        UseInstanceSound = 1,
        UseMusic = 2,
    };

    SoundVoiceBits bits;
    f32 pitchScale;
    f32 volume;
    s32 frames;
    s32 left;
    s32 right;
    s32 pitch;
    f32 lastAngle;
};
CHECK_SIZE(SoundVoice, 0x20);

// A core's bits: its number, and its reverb's delay and feedback (0 to 127)
union SoundCoreBits
{
    u32 value;
    struct
    {
        u32 number : 16;
        u32 reverbDelay : 7;
        u32 reverbFeedback : 7;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(SoundCoreBits, 4);

// A core of the sound processor: its reverb (its mode, its bits, its volumes and depths, the frames since a voice last sent to
// it) and its voices (a bit each while in use)
struct SoundCore
{
    static constexpr u32 VoiceCount = 24;

    s32 reverbMode;
    SoundCoreBits bits;
    s32 reverbVolumeLeft;
    s32 reverbVolumeRight;
    s32 reverbDepthLeft;
    s32 reverbDepthRight;
    s32 reverbIdleFrames;
    u32 voicesInUse;
    SoundVoice voices[VoiceCount];
};
CHECK_SIZE(SoundCore, 0x320);

// A music player's bits: whether its track is interleaved (stereo), its state (MusicPlayer::State), the volume group it plays in,
// whether its track plays at once when it's prepared, whether it stops at the end of its fade and whether its track loops
union MusicPlayerBits
{
    u32 value;
    struct
    {
        u32 interleaved : 1;
        u32 state : 5;
        u32 group : 3;
        u32 startsAtOnce : 1;
        u32 stopsAfterFade : 1;
        u32 loops : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(MusicPlayerBits, 4);

// Music streamed into one voice, or two when it's interleaved (stereo): its bits, its fade (its time, its target volumes, the
// volumes it started from) and volumes, its track and its rate's pitch, its voices, its streams, where its buffers are in the
// sound processor's memory (a stream's half each) and its stream's buffer in the I/O processor's memory (a movie streams through
// it, LendMusicBuffer)
struct MusicPlayer
{
    enum State : u32
    {
        // Made, its buffers not yet (CreateMusicBuffers)
        WithoutBuffers = 0,
        Stopped = 1,
        Preparing = 2,
        Ready = 3,
        FadingIn = 4,
        Playing = 5,
        FadingOut = 6,
        Faded = 7,
        Stopping = 8,
        // Its buffer is the movie player's (LendMusicBuffer)
        LentToMovie = 9,
    };

    MusicPlayerBits bits;
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
    FileStream* secondStream;
    u32 soundBuffer;
    u32 iopBuffer;
};
CHECK_SIZE(MusicPlayer, 0x40);

// The music: each slot's player, prepared or being prepared until PlayPreparedMusic plays it, or started (playing once it's
// prepared), and the players
struct MusicSystem
{
    static constexpr u32 PlayerCount = 3;

    MusicPlayer* prepared[MusicSlotCount];
    MusicPlayer* playing[MusicSlotCount];
    MusicPlayer players[PlayerCount];
};
CHECK_SIZE(MusicSystem, 0xE0);

// A volume group's volumes in 4.12 fixed point
struct GroupVolume
{
    s16 right;
    s16 left;
};

// A bank's music track (the bank's header: its track count, the size of the blocks an interleaved track's sides alternate in,
// then the tracks): its kind, its samples' size and offset in the bank, its sample rate and a value handed to an interleaved
// track's stream
struct MusicTrack
{
    enum Kind : s32
    {
        Mono = 0,
        // Stereo, its sides alternating in blocks
        Interleaved = 1,
        // The music bank's entry of a track of the voices' bank
        InVoiceBank = 2,
    };

    s32 kind;
    u32 size;
    u32 offset;
    u32 rate;
    u32 value;
};

// A pool of items the sound code hands out by index (the instances' sounds and the music emitters): how many it has room for,
// how many more it grows by (a quarter of its first room plus ten, both made even), how many are in use, the first free one,
// each item's link (the next free one; -1 one in use, -2 the last free one), the items (new[]) and its vtable (1 the destructor)
struct SoundPoolHeader
{
    s16 capacity;
    s16 growth;
    s16 used;
    s16 freeHead;
    s16* links;
};

template <typename T>
struct SoundPool : SoundPoolHeader
{
    T* items;
    const GccVTableEntry* vtable;
};

// A walk over a pool's items in use (its vtable: 1 the destructor, 2 to the first, 3 whether it's past the last, 4 the item, 5
// to the next, 6 copied from another, 7 the item's index; its base's are abstract): the item's index, how many it went past, the
// pool
template <typename T>
struct SoundPoolIterator
{
    const GccVTableEntry* vtable;
    s16 index;
    s16 position;
    SoundPool<T>* pool;
};

// An instance sound's bits: whether it plays again (heard again after it ended or went out of hearing), its volume group, its
// voice kind, its state and whether it's left where it started (not placed at the instance)
union InstanceSoundBits
{
    u32 value;
    struct
    {
        u32 repeats : 1;
        u32 group : 3;
        u32 voiceKind : 5;
        u32 state : 3;
        u32 notPlaced : 1;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(InstanceSoundBits, 4);

// An instance sound's index of none (a node playing none; what fails to play one)
constexpr u8 NoInstanceSound = 0xFF;

// A sound an instance plays: the instance, its voice (none while it waits), the sound, its volume and pitch scale, its bits and
// the frames it has played
struct InstanceSound
{
    enum State : u32
    {
        StatePlaying = 1,
        // Until it can be heard to play again
        StateWaiting = 2,
        StateDone = 3,
    };

    Reference* instance;
    SoundVoice* voice;
    GameSound* sound;
    f32 volume;
    f32 pitch;
    InstanceSoundBits bits;
    s32 frames;
};
CHECK_SIZE(InstanceSound, 0x1C);

// A music emitter's bits: its track, whether a music player plays it, whether it loops and whether it's interleaved (its volume
// then falls off with the distance alone, the same on both sides)
union MusicEmitterBits
{
    u32 value;
    struct
    {
        u32 track : 16;
        u32 hasPlayer : 1;
        u32 loops : 1;
        u32 interleaved : 1;
        u32 unused19 : 13;
    };
};
CHECK_SIZE(MusicEmitterBits, 4);

// A music track an instance plays where it is (BeginMusic): the instance, its bits, the music player playing it, how far it was
// last, how far it's heard (0: 60 units), its volume (its request's sides' mean) and the angle around the listener it was last
// heard at (SoundAtPlace's)
struct MusicEmitter
{
    Reference* instance;
    MusicEmitterBits bits;
    MusicPlayer* player;
    f32 distance;
    f32 range;
    f32 volume;
    f32 lastAngle;
};
CHECK_SIZE(MusicEmitter, 0x1C);

// The volume groups' volumes in cutscenes: their own, fading to the cutscenes' levels, at them, fading back
enum CutsceneVolumeMode : s32
{
    OwnVolumes = 0,
    FadingToCutsceneVolumes = 1,
    CutsceneVolumes = 2,
    FadingFromCutsceneVolumes = 3,
};

// What SoundAtPlace works out of a sound at a place (its out): the left and right volumes, the pitch, the distance and the angle
// around the listener (the next call's last angle)
struct HeardSound
{
    f32 left;
    f32 right;
    f32 pitch;
    f32 distance;
    f32 angle;
    f32 unused14[3];
};
CHECK_SIZE(HeardSound, 0x20);

extern "C"
{
    // The instances' sounds (room for 128 first) and the music emitters (32)
    extern SoundPool<InstanceSound> g_InstanceSounds RETAIL(D_003B1B00);
    extern SoundPool<MusicEmitter> g_MusicEmitters RETAIL(D_003B1B18);
    // The pools made with their room, destroyed (their items' instances let go of), grown (their items in use copied into the new
    // items: an instance sound's copy, a music emitter's at once), an item taken (the pool grown when it's full), given back,
    // every item given back
    SoundPool<InstanceSound>* ConstructInstanceSounds(SoundPool<InstanceSound>* pool, s32 capacity) RETAIL(FUN_001e4f70);
    SoundPool<MusicEmitter>* ConstructMusicEmitters(SoundPool<MusicEmitter>* pool, s32 capacity) RETAIL(FUN_001e50a0);
    void DestroyMusicEmitters(SoundPool<MusicEmitter>* pool, u32 destroyFlags) RETAIL(FUN_001e8d88);
    void DestroyInstanceSounds(SoundPool<InstanceSound>* pool, u32 destroyFlags) RETAIL(FUN_001e8e38);
    void GrowMusicEmitters(SoundPool<MusicEmitter>* pool) RETAIL(FUN_001e4a70);
    void CopyMusicEmitters(SoundPool<MusicEmitter>* pool, const MusicEmitter* from) RETAIL(FUN_001e48f0);
    void GrowInstanceSounds(SoundPool<InstanceSound>* pool) RETAIL(FUN_001e4d50);
    InstanceSound* AssignInstanceSound(InstanceSound* to, const InstanceSound* from) RETAIL(FUN_001e4c30);
    s32 AllocateMusicEmitter(SoundPool<MusicEmitter>* pool) RETAIL(FUN_001e8ae0);
    s32 AllocateInstanceSound(SoundPool<InstanceSound>* pool) RETAIL(FUN_001e8bf8);
    void FreeInstanceSound(SoundPoolHeader* pool, s32 index) RETAIL(FUN_001e8ab8);
    void FreeMusicEmitter(SoundPoolHeader* pool, s32 index) RETAIL(FUN_001e8b58);
    void ClearMusicEmitters(SoundPoolHeader* pool) RETAIL(FUN_001e8b80);
    void ClearInstanceSounds(SoundPoolHeader* pool) RETAIL(FUN_001e8c70);
    // The pools' iterators' vtable functions (the music emitters' and the instance sounds')
    void MusicEmitterIteratorDestroy(SoundPoolIterator<MusicEmitter>* iterator, u32 destroyFlags) RETAIL(FUN_001e5ad8);
    void MusicEmitterIteratorBaseDestroy(SoundPoolIterator<MusicEmitter>* iterator, u32 destroyFlags) RETAIL(FUN_001e5b08);
    void MusicEmitterIteratorFirst(SoundPoolIterator<MusicEmitter>* iterator) RETAIL(func_001E5B38);
    bool MusicEmitterIteratorDone(const SoundPoolIterator<MusicEmitter>* iterator) RETAIL(FUN_001e8ce8);
    MusicEmitter* MusicEmitterIteratorCurrent(const SoundPoolIterator<MusicEmitter>* iterator) RETAIL(FUN_001e5bb0);
    void MusicEmitterIteratorNext(SoundPoolIterator<MusicEmitter>* iterator) RETAIL(FUN_001e5bd0);
    SoundPoolIterator<MusicEmitter>* MusicEmitterIteratorAssign(SoundPoolIterator<MusicEmitter>* iterator,
                                                                const SoundPoolIterator<MusicEmitter>* from) RETAIL(func_001E8D00);
    s32 MusicEmitterIteratorIndex(const SoundPoolIterator<MusicEmitter>* iterator) RETAIL(FUN_001e8d20);
    s32 MusicEmitterIteratorCount(const SoundPoolIterator<MusicEmitter>* iterator) RETAIL(FUN_001e8d28);
    void InstanceSoundIteratorDestroy(SoundPoolIterator<InstanceSound>* iterator, u32 destroyFlags) RETAIL(FUN_001e56e0);
    void InstanceSoundIteratorBaseDestroy(SoundPoolIterator<InstanceSound>* iterator, u32 destroyFlags) RETAIL(FUN_001e5710);
    void InstanceSoundIteratorFirst(SoundPoolIterator<InstanceSound>* iterator) RETAIL(func_001E5740);
    bool InstanceSoundIteratorDone(const SoundPoolIterator<InstanceSound>* iterator) RETAIL(FUN_001e8d38);
    InstanceSound* InstanceSoundIteratorCurrent(const SoundPoolIterator<InstanceSound>* iterator) RETAIL(FUN_001e57b8);
    void InstanceSoundIteratorNext(SoundPoolIterator<InstanceSound>* iterator) RETAIL(FUN_001e57d8);
    SoundPoolIterator<InstanceSound>* InstanceSoundIteratorAssign(SoundPoolIterator<InstanceSound>* iterator,
                                                                  const SoundPoolIterator<InstanceSound>* from) RETAIL(func_001E8D50);
    s32 InstanceSoundIteratorIndex(const SoundPoolIterator<InstanceSound>* iterator) RETAIL(FUN_001e8d70);
    s32 InstanceSoundIteratorCount(const SoundPoolIterator<InstanceSound>* iterator) RETAIL(FUN_001e8d78);

    // The sound items' builder (class 0x1E00: an empty sound), its destructor
    GameSound* MakeSoundItem(void* builder, u32 classId) RETAIL(FUN_001e5638);
    void DestroySoundItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_001e5608);

    // The sound's statics made: the cores' voices free and their reverbs at full volume, no listener, the pools, no banks; and
    // the static constructor running it (GCC 2.9x's: when initialise is 1 and the priority 0xFFFF)
    void InitSoundStatics(s32 initialise, s32 priority) RETAIL(FUN_001e51d0);
    void SoundStaticInit() RETAIL(FUN_001e8ee8);

    // A sound played at a position of a chunk drawn this frame or the last: on a free voice of the kind, its volumes from where
    // the position is from the listener; the voice's number, -1 when it couldn't (the listener's chunk's other chunks through their
    // draw matrices)
    s32 PlaySoundAtPosition(f32 volume, f32 pitch, GameSound* sound, u32 group, ChunkData* chunk, const Vector4* position,
                            s32 voiceKind, s32 last) RETAIL_N32(FUN_001df3c8);
    // A sound played on an instance where it is (followed while it plays, or waiting until it can be heard when it repeats, last
    // 0), or not placed: the instance sound's index, 0xFF when it couldn't
    u32 PlayInstanceSoundAt(f32 volume, f32 pitch, GameSound* sound, u32 group, InstanceContext* instance, s32 voiceKind,
                            s32 last) RETAIL_N32(FUN_001dee98);
    u32 PlayInstanceSound(f32 volume, f32 pitch, GameSound* sound, u32 group, InstanceContext* instance, s32 voiceKind)
        RETAIL_N32(FUN_001df178);
    // The same of a sound's ID (sound 0x1BF never repeats), and a sound of an ID at a position (whether it plays)
    u32 PlayInstanceSoundById(f32 volume, f32 pitch, u32 id, u32 group, InstanceContext* instance, s32 voiceKind, s32 last)
        RETAIL_N32(FUN_001e5c68);
    u32 PlayUnplacedSoundById(f32 volume, f32 pitch, u32 id, u32 group, InstanceContext* instance, s32 voiceKind)
        RETAIL_N32(FUN_001e5e78);
    u32 PlaySoundByIdAt(f32 volume, f32 pitch, u16 id, s32 group, ChunkData* chunk, const Vector4* position, s32 voiceKind,
                        s32 last) RETAIL_N32(PlaySound_);
    // An instance sound stopped (silenced when it plays), its volume and its pitch scale set (its voice's too while it plays)
    void StopInstanceSound(s32 index) RETAIL(FUN_001e5858);
    void SetInstanceSoundVolume(f32 volume, s32 index) RETAIL_N32(FUN_001e58f8);
    void SetInstanceSoundPitch(f32 pitch, s32 index) RETAIL_N32(FUN_001e5950);
    // A playing voice's volumes and pitch made again for where it's heard from: whether it's heard
    u32 UpdateVoiceAt(SoundVoice* voice, GameSound* sound, const Vector4* position) RETAIL(FUN_001e6298);
    // What's heard of a sound at a position from the listener (out a HeardSound: its volumes as g_MusicStereo has them), falling
    // off over its range (0: DefaultSoundRange): whether it's heard (above 0.005 of its volume)
    u32 SoundAtPlace(f32 volume, f32 range, f32 lastAngle, s32 pitch, const Vector4* position, f32* out)
        RETAIL_N32(FUN_001e0a20);
    // Every frame: the instances' sounds followed and given back once done, the music emitters' players volumes for where they
    // are; and the music emitters stopped
    void UpdateInstanceSounds() RETAIL(FUN_001dd730);
    void UpdateMusicEmitters() RETAIL(FUN_001de488);
    void StopMusicEmitters() RETAIL(FUN_001dea10);
    // An instance made to play a music request where it is, heard to a distance: whether it could
    u32 AddMusicEmitter(f32 range, InstanceContext* instance, const MusicRequest* request) RETAIL_N32(FUN_001de2a0);
    // The matrix taking a chunk's things into the listener's space this frame (the draw matrices of the chunks drawn this frame
    // kept on the stack, the chunks' particle view field holding their slot meanwhile): whether the chunk was drawn
    u32 ChunkToListener(struct ChunkMatrixCache* cache, ChunkData* chunk, const Matrix4x4* view, Matrix4x4* out) RETAIL(FUN_001dd640);

    extern SoundCore g_SoundCores[SoundCoreCount] RETAIL(D_003B14C0);
    // The sound's listener (game/gamecontroller.h's SetSoundListener)
    extern Reference* g_SoundListener RETAIL(D_0030A8A0);
    extern MusicSystem* g_Music RETAIL(G_UnkStructOfSize_0xe0);
    extern GroupVolume g_GroupVolumes[VolumeGroupCount] RETAIL(D_003D6F20);
    // The sound is lent to the movie playing: the sound is left alone
    extern u8 g_SoundLentToMovie RETAIL(D_00309E5C);
    // Every pitch is scaled by it
    extern f32 g_PitchScale RETAIL(D_00309E74);
    // How sounds are heard (StereoMode): the music's interleaved voices get a side each while it's not StereoOff
    extern u32 g_MusicStereo RETAIL(D_00309E68);
    // The fade of every group's volume: its length and how far it got (1 to 0)
    extern f32 g_GroupFadeTime RETAIL(D_00309E60);
    extern f32 g_GroupFade RETAIL(D_00309E64);
    // How far the sounds played at a place are heard (DefaultSoundRange)
    extern f32 g_SoundRange RETAIL(D_0030AC94);
    // The frames since the sound processor was last kept alive
    extern s32 g_FramesSinceKeepAlive RETAIL(D_0030A898);
    // A music request the sound's update plays in a slot (track 3 looped at half volume in the music group, faded in over a
    // second) while it's pending, which nothing sets
    extern u8 g_MusicRequestPending RETAIL(D_00309E80);
    extern u32 g_PendingMusicSlot RETAIL(D_00309E7C);
    extern const MusicRequest g_PendingMusicRequest RETAIL(D_002E7918);
    // The music slots with a request waiting for a free player
    extern u8 g_MusicSlotsPending[MusicSlotCount] RETAIL(D_0030AC90);

    s32 VoiceNumber(const SoundVoice* voice);
    // The volume group's number (groups are numbered from 1, in the top half)
    u32 SoundGroupOf(s32 group) RETAIL(FUN_001e6580);

    void SetReverbVolume(f32 left, f32 right, SoundCore* core) RETAIL_N32(FUN_001e60b0);
    void SetReverbDepth(f32 left, f32 right, SoundCore* core) RETAIL_N32(FUN_001e6108);
    void SetReverb(SoundCore* core, s32 mode, u32 delay, u32 feedback) RETAIL(FUN_001e6138);
    // The chunk's reverb (a ReverbSettings): a type (0 to 6) and its delay, feedback and depth (0 to 1)
    void SetReverbFromSettings(SoundCore* core, const u8* settings) RETAIL(FUN_001df8f0);
    // A voice (none when none is free): of a core with the reverb mode (ReverbOff any), else of the first core without reverb that
    // has one free, else of the first core that has one; free, or when stealing one used for nothing (muted first). It's given
    // its number and core
    SoundVoice* FindFreeVoice(s32 reverbMode, s32 steal) RETAIL(FUN_001dff30);
    // A voice silenced at once (its release fast), and let go of (its use, volume and pitch scale forgotten, its core's bit
    // cleared)
    void MuteVoice(SoundVoice* voice) RETAIL(FUN_001e6398);
    void ReleaseVoice(SoundVoice* voice) RETAIL(FUN_001e68c0);
    // The voice of a number (the second core's from 24)
    SoundVoice* VoiceOfNumber(u32 number) RETAIL(FUN_001e6988);
    // A core's voices updated (those done let go of), and its reverb turned off after 301 frames without a voice sending to it
    void UpdateSoundCore(SoundCore* core) RETAIL(FUN_001df7b0);
    // The voice kind the chunk's reverb settings play in: a core already set to them, else the one with the most free voices
    // (one without reverb first) set to them
    s32 UseReverb(const struct ReverbSettings* settings) RETAIL(FUN_001e0368);
    // The voice kind sounds play in where the sound's listener is (game/gamecontroller.h's SetSoundListener): its chunk's reverb,
    // the box reverb while it's in one of the chunk's sound boxes (ReverbOff without a listener or a reverb)
    s32 ListenerVoiceKind() RETAIL(FUN_0022f208);
    // The groups' volumes moved along the cutscene fade (by the time over what's left of it: faster towards its end)
    void UpdateCutsceneVolumes(f32 time) RETAIL(FUN_001df670);
    // The music: made at start-up, a player's state for a movie lent its buffer (or given back), its volume applied again, faded
    // out over a time (at least 0.3 s; stopping it at the end when asked), faded to volumes over a time, made empty; a fade's step
    // (whether both sides got there)
    void CreateMusic() RETAIL(FUN_001e6880);
    void LendMusicPlayer(MusicPlayer* player, u32 lend) RETAIL(FUN_001e6cd0);
    s32 ApplyMusicVolume(MusicPlayer* player) RETAIL(FUN_001e6e10);
    void FadeMusicOut(f32 time, MusicPlayer* player, u32 stops) RETAIL_N32(FUN_001e6c20);
    void FadeMusicTo(f32 left, f32 right, f32 time, MusicPlayer* player) RETAIL_N32(FUN_001e6e30);
    MusicPlayer* ConstructMusicPlayer(MusicPlayer* player) RETAIL(FUN_001e6c90);
    u32 FadeMusic(f32 time, MusicPlayer* player) RETAIL_N32(FUN_001e0fd0);
    // A sound's pitch and parameters read (signed halfwords)
    void ReadSoundParameters(u16* parameters, Stream* stream) RETAIL(FUN_001e6b58);
    // The value a share of the way down: total - part / whole
    f32 RemainingShare(f32 part, f32 whole, f32 total) RETAIL(FUN_001e5c50);
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
    // The sound of an ID: the game's table's, else the language's voices' (none past the game's table)
    GameSound* SoundById(u16 id) RETAIL(GetSoundAddress);
    // The sound of an ID played the same way, 0 without one
    u32 PlaySoundById(f32 volume, f32 pitchScale, u32 id, s32 group, s32 voiceKind, s32 last) RETAIL_N32(FUN_001e5d10);
    // At a place: its volumes and pitch are worked out from where it is. Returns the voice's number, -1 when it can't be heard
    s32 PlaySoundAt(f32 volume, f32 pitchScale, SoundVoice* voice, GameSound* sound, u32 group, const Vector4* position, s32 last)
        RETAIL_N32(FUN_001dfbc0);
    void FreeSoundSamples(u32 unused, u32 sound) RETAIL(FUN_001e6b38);

    void PauseSound() RETAIL(FUN_001e65e0);
    void ResumeSound() RETAIL(FUN_001e6600);
    void ResetSound() RETAIL(FUN_001e66a8);
    void SetGroupVolume(f32 left, f32 right, s32 group) RETAIL_N32(FUN_001e6778);
    // Stops every sound and the music, the reverbs and the groups' fades
    void StopAllSound() RETAIL(FUN_001e0560);
    // Returns g_SoundLentToMovie while the sound is lent to a movie, else what the pending music request returned (0 without one)
    u32 UpdateSound(s32 paused, TimeClock* clock) RETAIL(FUN_001e0768);
    MusicSystem* ConstructMusic(MusicSystem* music) RETAIL(FUN_001dfde8);

    // Music
    // A music request played in a slot (the main one's fade time the track's): what the slot plays faded out (the context slot's
    // stopped), a stopped player started with it, prepared or playing (its startsAtOnce). Returns 1, 0 without a player
    s32 PlayMusicRequest(u32 slot, const MusicRequest* request) RETAIL(FUN_001ddff0);
    // The block of .bss Ghidra named after the renderer's six alpha presets at its start (0x80 bytes): the music requests the
    // slots wait to play once a player is free (flagged in g_MusicSlotsPending) and the volume groups' scales (the cutscenes'
    // levels) follow the presets in it
    extern u8 g_AlphaPresetsBlock[] RETAIL(G_AlphaRegPresets);
    constexpr u32 PendingMusicRequestsOffset = 0x30;
    constexpr u32 GroupScalesOffset = 0x70;
    // A slot's prepared player played with volumes and a fade, a slot's player faded out (and stopped at the end), whether a
    // slot's player is prepared, whether it plays
    u32 PlayPreparedMusic(f32 left, f32 right, f32 fadeTime, s32 slot) RETAIL_N32(FUN_001e59b0);
    u32 FadeOutMusicSlot(f32 time, s32 slot) RETAIL_N32(FUN_001e5a20);
    u32 MusicSlotPrepared(s32 slot) RETAIL(FUN_001e5a68);
    u32 MusicSlotPlaying(s32 slot) RETAIL(FUN_001e5aa0);
    // A music player for a new track: the one of the music emitter farthest past a distance, else the first player stopping or
    // fading out to stop, else the spare slot's; stopped and taken from the slots
    MusicPlayer* TakeMusicPlayer(f32 distance) RETAIL(FUN_001debc0);
    // The players playing updated, and the slots' pending requests started on a free player
    void UpdatePlayingSounds(f32 time, bool paused) RETAIL_N32(FUN_001dddd0);
    // A track's file in a bank (the voices' bank's when the music bank's entry is InVoiceBank): the bank's last for any past it
    struct MusicTrack* FindMusicTrack(SoundBankFiles* bank, u32 track) RETAIL(FUN_002addb0);
    // The music's bank (Crash6\Music) and the language's voices' (Crash6\<language>)
    extern SoundBankFiles g_MusicBank RETAIL(D_0030A8A8);
    extern SoundBankFiles g_VoiceBank RETAIL(D_0030A8B0);
    // The music's bank read, the music players' buffers made for it again
    void SetMusicBank(const char* name) RETAIL(FUN_001e6a10);
    void SetVoiceBank(const char* name) RETAIL(FUN_001e6a70);
    void CreateMusicBuffers(MusicPlayer* player, SoundBankFiles* bank) RETAIL(FUN_001e6d48);
    // A music request (request a MusicRequest) started on a player: its track read from the bank and prepared
    void StartMusic(MusicPlayer* player, SoundBankFiles* bank, const u32* request) RETAIL(FUN_001e10f8);
    s32 PlayMusic(f32 left, f32 right, f32 fadeTime, MusicPlayer* player) RETAIL_N32(FUN_001e6fc0);
    void StopMusic(MusicPlayer* player) RETAIL(FUN_001e7060);
    s32 SetMusicPitch(f32 scale, MusicPlayer* player) RETAIL_N32(FUN_001e6e78);
    s32 SetMusicVolume(f32 left, f32 right, MusicPlayer* player) RETAIL_N32(FUN_001e14b8);
    // Returns whether it's doing anything
    bool UpdateMusic(f32 time, MusicPlayer* player, s32 paused) RETAIL_N32(FUN_001e1730);
    // A movie streams through a music player's buffer: one lent already, else a stopped one, else the one TakeMusicPlayer takes.
    // Returns the buffer, nullptr for none
    void* LendMusicBuffer() RETAIL(FUN_001e0940);
    // Every player lent to the movie is stopped again
    void ReclaimMusicBuffers() RETAIL(FUN_001e69c0);
    // The volume group's level: the mean of its sides' volumes
    f32 GroupVolumeLevel(s32 group) RETAIL(FUN_001e6838);
    // How sounds are heard (StereoMode), the volume of the music players playing applied again with it
    void SetMusicStereo(u32 mode) RETAIL(FUN_001e6920);
    // The sound groups' volumes in cutscenes (CutsceneVolumeMode) and how much of the fade is left (1 to 0). A cutscene's start
    // and end start the fades
    extern s32 g_CutsceneVolumeMode RETAIL(D_00309E6C);
    extern f32 g_CutsceneVolumeFade RETAIL(D_00309E70);
    void FadeToCutsceneVolumes() RETAIL(FUN_001e6068);
    void FadeFromCutsceneVolumes() RETAIL(FUN_001e6080);
}
