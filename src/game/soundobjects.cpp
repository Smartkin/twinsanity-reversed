#include "game/sound.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/filestream.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/view.h"
#include "platform/math.h"
#include "retail/libc.h"

// The sounds the game plays on instances and the music emitters, kept in pools handed out by index and followed every frame from
// where the sound's listener is
extern "C"
{
    extern const GccVTableEntry g_MusicEmitterIteratorVTable[] RETAIL(D_002FB448);
    extern const GccVTableEntry g_MusicEmitterIteratorBaseVTable[] RETAIL(D_002FB498);
    extern const GccVTableEntry g_InstanceSoundIteratorVTable[] RETAIL(D_002FB4D0);
    extern const GccVTableEntry g_InstanceSoundIteratorBaseVTable[] RETAIL(D_002FB520);
    extern const GccVTableEntry g_MusicEmitterPoolVTable[] RETAIL(D_002FB5B8);
    extern const GccVTableEntry g_InstanceSoundPoolVTable[] RETAIL(D_002FB5D0);
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
}

// The chunks drawn this frame and their draw matrices taken into the listener's space, made once a frame by each update that
// follows sounds (14 at most)
struct ChunkMatrixCache
{
    s32 count;
    u32 frame;
    ChunkData* listenerChunk;
    ChunkData* chunks[17];
    Matrix4x4 matrices[14];
};
CHECK_OFFSET(ChunkMatrixCache, matrices, 0x50);

namespace
{
constexpr s16 InUse = -1;
constexpr s16 LastFree = -2;
// A music emitter's range when it has none
constexpr f32 DefaultRange = 60.0f;
// The share of its volume a music emitter is still heard at
constexpr f32 HeardShare = Rounded(0.001);
constexpr f32 Heard = Rounded(0.005);
constexpr f32 DirectionEpsilon = 0x1.5798ecp-29f;
constexpr f32 Pi = 0x1.921fb6p+1f;
constexpr f32 HalfPi = 0x1.921fb6p+0f;
constexpr f32 TwoPi = 0x1.921fb6p+2f;
constexpr f32 ThreePi = 0x1.2d97c8p+3f;
constexpr f32 FourPi = 0x1.921fb6p+3f;
constexpr f32 QuarterPi = 0x1.921fb6p-1f;
constexpr f32 TurnToRadians = 0x1.921fb6p-14f;
constexpr f32 Root2Half = 0x1.6a09e6p-1f;
// How much a sound's pitch falls when it's behind the listener
constexpr f32 BehindPitch = Rounded(0.01);

// A reference taken once more
void Retain(Reference* reference)
{
    reference->value = (reference->value & 0xFF000000) | (((reference->value & 0xFFFFFF) + 1) & 0xFFFFFF);
}

// A handle given the other's reference (counted once more)
void CopyReference(Reference** to, Reference* from)
{
    if (*to == from)
    {
        return;
    }

    RemoveReference(to);
    *to = from;
    if (from != nullptr)
    {
        Retain(from);
    }
}

InstanceContext* InstanceOf(Reference* reference)
{
    return static_cast<InstanceContext*>(reference != nullptr ? reference->object : nullptr);
}

InstanceContext* Listener()
{
    return InstanceOf(g_SoundListener);
}

template <typename T>
SoundPool<T>* ConstructPool(SoundPool<T>* pool, s32 capacity, const GccVTableEntry* vtable)
{
    pool->capacity = static_cast<s16>(capacity);
    pool->vtable = vtable;
    pool->growth = static_cast<s16>((capacity >> 2) + 10);
    pool->used = 0;
    pool->freeHead = 0;
    pool->links = nullptr;
    pool->items = nullptr;
    if ((pool->capacity & 1) != 0)
    {
        pool->capacity++;
    }

    if ((pool->growth & 1) != 0)
    {
        pool->growth++;
    }

    pool->links = static_cast<s16*>(MemoryAllocate2(pool->capacity << 1));
    T* items = NewArray<T>(pool->capacity);
    for (s32 index = 0; index < pool->capacity; index++)
    {
        items[index].instance = nullptr;
    }

    pool->items = items;
    for (s32 index = 0; index < pool->capacity; index++)
    {
        pool->links[index] = static_cast<s16>(index + 1);
    }

    pool->links[pool->capacity - 1] = LastFree;
    return pool;
}

template <typename T>
void DeleteItems(T* items)
{
    T* item = items + ArrayCount(items);
    while (items != item)
    {
        item--;
        RemoveReference(&item->instance);
    }

    DeleteArray(items);
}

template <typename T>
void DestroyPool(SoundPool<T>* pool, u32 destroyFlags, const GccVTableEntry* vtable)
{
    pool->vtable = vtable;
    if (pool->links != nullptr)
    {
        MemoryDeallocate_(pool->links);
    }

    if (pool->items != nullptr)
    {
        DeleteItems(pool->items);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(pool);
    }
}

// Room for its growth more: the items in use copied into the new items (all in use at the front), the new ones free after them
template <typename T>
void GrowPool(SoundPool<T>* pool, void (*copy)(SoundPool<T>*, T* items))
{
    if ((pool->capacity & 1) != 0)
    {
        pool->capacity++;
    }

    if ((pool->growth & 1) != 0)
    {
        pool->growth++;
    }

    s32 count = pool->capacity + pool->growth;
    T* items = NewArray<T>(count);
    for (s32 index = 0; index < count; index++)
    {
        items[index].instance = nullptr;
    }

    auto* links = static_cast<s16*>(MemoryAllocate2((pool->capacity + pool->growth) * 2));
    s32 index = pool->capacity;
    if (pool->capacity != 0)
    {
        T* old = pool->items;
        copy(pool, items);
        RetailLibc::MemorySet(links, 0xFF, pool->capacity << 1);
        if (old != nullptr)
        {
            DeleteItems(old);
        }

        if (pool->links != nullptr)
        {
            MemoryDeallocate_(pool->links);
        }

        index = pool->capacity;
    }

    s32 end = index + pool->growth;
    while (index < end)
    {
        links[index] = static_cast<s16>(index + 1);
        index++;
    }

    s16 capacity = pool->capacity;
    pool->links = links;
    links[index - 1] = LastFree;
    pool->items = items;
    pool->capacity = static_cast<s16>(capacity + pool->growth);
    pool->freeHead = capacity;
}

template <typename T>
s32 AllocateItem(SoundPool<T>* pool, void (*grow)(SoundPool<T>*))
{
    if (!(pool->used < pool->capacity))
    {
        grow(pool);
        return AllocateItem(pool, grow);
    }

    s16 index = pool->freeHead;
    s16* link = &pool->links[index];
    pool->freeHead = *link;
    *link = InUse;
    pool->used++;
    return index;
}

void FreeItem(SoundPoolHeader* pool, s32 index)
{
    pool->links[index] = pool->freeHead;
    pool->freeHead = static_cast<s16>(index);
    pool->used--;
}

void ClearPool(SoundPoolHeader* pool)
{
    if (pool->capacity <= 0 || pool->used == 0)
    {
        return;
    }

    s32 index = 0;
    while (index < pool->capacity - 1)
    {
        pool->links[index] = static_cast<s16>(index + 1);
        index++;
    }

    pool->links[index] = LastFree;
    pool->freeHead = 0;
    pool->used = 0;
}

template <typename T>
void IteratorFirst(SoundPoolIterator<T>* iterator)
{
    SoundPool<T>* pool = iterator->pool;
    iterator->index = 0;
    iterator->position = 0;
    s16 index = 0;
    while (index < pool->capacity - 1 && pool->links[index] != InUse)
    {
        index++;
        iterator->index = index;
    }
}

template <typename T>
void IteratorNext(SoundPoolIterator<T>* iterator)
{
    SoundPool<T>* pool = iterator->pool;
    if (!(iterator->position < pool->used - 1))
    {
        iterator->position = pool->used;
        return;
    }

    while (iterator->position < pool->used)
    {
        s16 position = iterator->position;
        iterator->index++;
        if (pool->links[iterator->index] == InUse)
        {
            iterator->position = static_cast<s16>(position + 1);
            return;
        }
    }
}

template <typename T>
void IteratorDestroy(SoundPoolIterator<T>* iterator, u32 destroyFlags, const GccVTableEntry* base)
{
    iterator->vtable = base;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}

template <typename T>
SoundPoolIterator<T>* IteratorAssign(SoundPoolIterator<T>* iterator, const SoundPoolIterator<T>* from)
{
    iterator->index = from->index;
    iterator->position = from->position;
    iterator->pool = from->pool;
    return iterator;
}

void CopyMusicEmittersInto(SoundPool<MusicEmitter>* pool, MusicEmitter* items)
{
    MusicEmitter* old = pool->items;
    pool->items = items;
    CopyMusicEmitters(pool, old);
}

void CopyInstanceSoundsInto(SoundPool<InstanceSound>* pool, InstanceSound* items)
{
    InstanceSound* old = pool->items;
    pool->items = items;
    for (s32 index = 0; index < pool->capacity; index++)
    {
        if (pool->links[index] == InUse)
        {
            AssignInstanceSound(&pool->items[index], &old[index]);
        }
    }
}

// The listener's matrix taken back to the world (identity without a listener)
void ListenerView(Matrix4x4* view)
{
    InstanceContext* listener = Listener();
    if (listener == nullptr)
    {
        InitIdentityMatrix(view);
        return;
    }

    ObjectPlace* place = listener->place;
    RotateAndTranslate(place);
    *view = place->matrix;
    VuInvertRigidInPlace(view);
}

void StartTrack(MusicPlayer* player, const MusicRequest* request)
{
    if (FindMusicTrack(&g_MusicBank, request->bits & 0xFFFF)->interleaved == 2)
    {
        StartMusic(player, &g_VoiceBank, &request->bits);
    }
    else
    {
        StartMusic(player, &g_MusicBank, &request->bits);
    }
}

MusicPlayer* FirstStoppedPlayer()
{
    for (MusicPlayer& player : g_Music->players)
    {
        if ((player.bits >> 1 & 0x1F) == MusicPlayer::Stopped)
        {
            return &player;
        }
    }

    return nullptr;
}

MusicRequest* PendingMusicRequests()
{
    return reinterpret_cast<MusicRequest*>(&G_AlphaRegPresets[6]);
}

MusicPlayer* SlotPlayer(u32 slot)
{
    MusicPlayer* player = g_Music->playing[slot];
    return player != nullptr ? player : g_Music->fading[slot];
}

u32 MusicPlayerState(const MusicPlayer* player)
{
    return player->bits >> 1 & 0x1F;
}
}

SoundPool<InstanceSound>* ConstructInstanceSounds(SoundPool<InstanceSound>* pool, s32 capacity)
{
    return ConstructPool(pool, capacity, g_InstanceSoundPoolVTable);
}

SoundPool<MusicEmitter>* ConstructMusicEmitters(SoundPool<MusicEmitter>* pool, s32 capacity)
{
    return ConstructPool(pool, capacity, g_MusicEmitterPoolVTable);
}

void DestroyMusicEmitters(SoundPool<MusicEmitter>* pool, u32 destroyFlags)
{
    DestroyPool(pool, destroyFlags, g_MusicEmitterPoolVTable);
}

void DestroyInstanceSounds(SoundPool<InstanceSound>* pool, u32 destroyFlags)
{
    DestroyPool(pool, destroyFlags, g_InstanceSoundPoolVTable);
}

void GrowMusicEmitters(SoundPool<MusicEmitter>* pool)
{
    GrowPool(pool, CopyMusicEmittersInto);
}

void CopyMusicEmitters(SoundPool<MusicEmitter>* pool, const MusicEmitter* from)
{
    for (s32 index = 0; index < pool->capacity; index++)
    {
        if (pool->links[index] != InUse)
        {
            continue;
        }

        MusicEmitter* to = &pool->items[index];
        const MusicEmitter* source = &from[index];
        CopyReference(&to->instance, source->instance);
        u32 bits = (to->bits & ~MusicEmitter::TrackMask) | (source->bits & MusicEmitter::TrackMask);
        bits = (bits & ~(MusicEmitter::HasPlayer | MusicEmitter::Loops | MusicEmitter::Stereo)) |
               (source->bits & (MusicEmitter::HasPlayer | MusicEmitter::Loops | MusicEmitter::Stereo));
        to->bits = bits;
        to->player = source->player;
        to->distance = source->distance;
        to->range = source->range;
        to->volume = source->volume;
        to->previous = source->previous;
    }
}

void GrowInstanceSounds(SoundPool<InstanceSound>* pool)
{
    GrowPool(pool, CopyInstanceSoundsInto);
}

InstanceSound* AssignInstanceSound(InstanceSound* to, const InstanceSound* from)
{
    CopyReference(&to->instance, from->instance);
    to->voice = from->voice;
    to->sound = from->sound;
    to->volume = from->volume;
    to->pitch = from->pitch;
    to->bits = (to->bits & ~0x1FFFu) | (from->bits & 0x1FFF);
    to->frames = from->frames;
    return to;
}

s32 AllocateMusicEmitter(SoundPool<MusicEmitter>* pool)
{
    return AllocateItem(pool, GrowMusicEmitters);
}

s32 AllocateInstanceSound(SoundPool<InstanceSound>* pool)
{
    return AllocateItem(pool, GrowInstanceSounds);
}

void FreeInstanceSound(SoundPoolHeader* pool, s32 index)
{
    FreeItem(pool, index);
}

void FreeMusicEmitter(SoundPoolHeader* pool, s32 index)
{
    FreeItem(pool, index);
}

void ClearMusicEmitters(SoundPoolHeader* pool)
{
    ClearPool(pool);
}

void ClearInstanceSounds(SoundPoolHeader* pool)
{
    ClearPool(pool);
}

void MusicEmitterIteratorDestroy(SoundPoolIterator<MusicEmitter>* iterator, u32 destroyFlags)
{
    IteratorDestroy(iterator, destroyFlags, g_MusicEmitterIteratorBaseVTable);
}

void MusicEmitterIteratorBaseDestroy(SoundPoolIterator<MusicEmitter>* iterator, u32 destroyFlags)
{
    IteratorDestroy(iterator, destroyFlags, g_MusicEmitterIteratorBaseVTable);
}

void MusicEmitterIteratorFirst(SoundPoolIterator<MusicEmitter>* iterator)
{
    IteratorFirst(iterator);
}

bool MusicEmitterIteratorDone(const SoundPoolIterator<MusicEmitter>* iterator)
{
    return iterator->position == iterator->pool->used;
}

MusicEmitter* MusicEmitterIteratorCurrent(const SoundPoolIterator<MusicEmitter>* iterator)
{
    return &iterator->pool->items[iterator->index];
}

void MusicEmitterIteratorNext(SoundPoolIterator<MusicEmitter>* iterator)
{
    IteratorNext(iterator);
}

SoundPoolIterator<MusicEmitter>* MusicEmitterIteratorAssign(SoundPoolIterator<MusicEmitter>* iterator,
                                                            const SoundPoolIterator<MusicEmitter>* from)
{
    return IteratorAssign(iterator, from);
}

s32 MusicEmitterIteratorIndex(const SoundPoolIterator<MusicEmitter>* iterator)
{
    return iterator->index;
}

s32 MusicEmitterIteratorCount(const SoundPoolIterator<MusicEmitter>* iterator)
{
    return iterator->pool->used;
}

void InstanceSoundIteratorDestroy(SoundPoolIterator<InstanceSound>* iterator, u32 destroyFlags)
{
    IteratorDestroy(iterator, destroyFlags, g_InstanceSoundIteratorBaseVTable);
}

void InstanceSoundIteratorBaseDestroy(SoundPoolIterator<InstanceSound>* iterator, u32 destroyFlags)
{
    IteratorDestroy(iterator, destroyFlags, g_InstanceSoundIteratorBaseVTable);
}

void InstanceSoundIteratorFirst(SoundPoolIterator<InstanceSound>* iterator)
{
    IteratorFirst(iterator);
}

bool InstanceSoundIteratorDone(const SoundPoolIterator<InstanceSound>* iterator)
{
    return iterator->position == iterator->pool->used;
}

InstanceSound* InstanceSoundIteratorCurrent(const SoundPoolIterator<InstanceSound>* iterator)
{
    return &iterator->pool->items[iterator->index];
}

void InstanceSoundIteratorNext(SoundPoolIterator<InstanceSound>* iterator)
{
    IteratorNext(iterator);
}

SoundPoolIterator<InstanceSound>* InstanceSoundIteratorAssign(SoundPoolIterator<InstanceSound>* iterator,
                                                              const SoundPoolIterator<InstanceSound>* from)
{
    return IteratorAssign(iterator, from);
}

s32 InstanceSoundIteratorIndex(const SoundPoolIterator<InstanceSound>* iterator)
{
    return iterator->index;
}

s32 InstanceSoundIteratorCount(const SoundPoolIterator<InstanceSound>* iterator)
{
    return iterator->pool->used;
}

GameSound* MakeSoundItem(void*, u32 classId)
{
    constexpr u32 SoundClassId = 0x1E00;
    if (classId != SoundClassId)
    {
        return nullptr;
    }

    return GameSound::ConstructEmpty(static_cast<GameSound*>(MemoryAllocate(sizeof(GameSound))));
}

void DestroySoundItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void InitSoundStatics(s32 initialise, s32 priority)
{
    if (priority != 0xFFFF || initialise == 0)
    {
        return;
    }

    for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
    {
        core->reverbMode = 0;
        core->unknown18 = 0;
        core->voicesInUse = 0;
        for (SoundVoice& voice : core->voices)
        {
            voice.pitchScale = -1.0f;
            voice.volume = -1.0f;
            voice.frames = 0;
            voice.bits &= 0xFFFF8080;
        }

        SetReverbVolume(1.0f, 1.0f, core);
        SetReverbDepth(1.0f, 1.0f, core);
        core->reverbBits |= 0x3FFF0000;
    }

    g_SoundListener = nullptr;
    ConstructInstanceSounds(&g_InstanceSounds, 0x80);
    ConstructMusicEmitters(&g_MusicEmitters, 0x20);
    g_MusicBank.header = nullptr;
    g_VoiceBank.samples = -1;
    g_MusicBank.samples = -1;
    g_VoiceBank.header = nullptr;
}

void SoundStaticInit()
{
    InitSoundStatics(1, 0xFFFF);
}

s32 PlaySoundAtPosition(f32 volume, f32 pitch, GameSound* sound, u32 group, ChunkData* chunk, const Vector4* position,
                        s32 voiceKind, s32 last)
{
    if ((sound->flags & 1) == 0)
    {
        return -1;
    }

    InstanceContext* listener = Listener();
    if (listener == nullptr || chunk == nullptr || !(g_RenderedFrames - chunk->drawnFrame < 2))
    {
        return -1;
    }

    SoundVoice* voice = FindFreeVoice(voiceKind, 0);
    if (voice == nullptr)
    {
        return -1;
    }

    ObjectPlace* place = listener->place;
    ChunkData* listenerChunk = listener->chunk;
    RotateAndTranslate(place);
    Matrix4x4 view = place->matrix;
    VuInvertRigidInPlace(&view);
    if (listenerChunk != chunk)
    {
        Matrix4x4 draw = chunk->drawMatrix;
        VuMultiplyMatrices(&draw, &view, &view);
    }

    Vector4 local;
    VuTransformPoint(&view, position, &local);
    s32 number = PlaySoundAt(volume, pitch, voice, sound, SoundGroupOf(static_cast<s32>(group)), &local, last);
    if (number == -1)
    {
        ReleaseVoice(voice);
    }
    else
    {
        SetVoiceReverb(voice, voiceKind);
    }

    return number;
}

u32 PlayInstanceSoundAt(f32 volume, f32 pitch, GameSound* sound, u32 group, InstanceContext* instance, s32 voiceKind, s32 last)
{
    if ((sound->flags & 1) == 0)
    {
        return 0xFF;
    }

    if (g_InstanceSounds.capacity - g_InstanceSounds.used <= 0 && last != -1)
    {
        return 0xFF;
    }

    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    s32 number = PlaySoundAtPosition(volume, pitch, sound, group, instance->chunk, &position, voiceKind, last);
    if (!(number != -1 || last == 0) || g_InstanceSounds.capacity - g_InstanceSounds.used <= 0)
    {
        return 0xFF;
    }

    s32 index = AllocateInstanceSound(&g_InstanceSounds);
    InstanceSound* item = &g_InstanceSounds.items[static_cast<s16>(index)];
    if (number == -1)
    {
        item->voice = nullptr;
        item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StateWaiting;
    }
    else
    {
        item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StatePlaying;
        SoundVoice* voice = VoiceOfNumber(static_cast<u32>(number));
        item->voice = voice;
        voice->bits = (voice->bits & ~SoundVoice::UseMask) | 0x200;
        item->frames = 0;
    }

    item->bits = last == 0 ? item->bits | InstanceSound::Repeats : item->bits & ~InstanceSound::Repeats;
    AssignReference(&item->instance, instance);
    item->sound = sound;
    item->bits = (item->bits & 0xFFFFEE01) | (group & InstanceSound::GroupMask) << InstanceSound::GroupShift |
                 (static_cast<u32>(voiceKind) & InstanceSound::KindMask) << InstanceSound::KindShift;
    item->volume = volume;
    item->pitch = pitch;
    return static_cast<u32>(index) & 0xFF;
}

u32 PlayInstanceSound(f32 volume, f32 pitch, GameSound* sound, u32 group, InstanceContext* instance, s32 voiceKind)
{
    if ((sound->flags & 1) == 0)
    {
        return 0xFF;
    }

    if (g_InstanceSounds.capacity - g_InstanceSounds.used <= 0)
    {
        return 0xFF;
    }

    SoundVoice* voice = FindFreeVoice(voiceKind, 0);
    if (voice == nullptr)
    {
        return 0xFF;
    }

    if (PlaySoundOnVoice(volume, pitch, voice, sound, SoundGroupOf(static_cast<s32>(group)), 0) == -1)
    {
        ReleaseVoice(voice);
        return 0xFF;
    }

    SetVoiceReverb(voice, voiceKind);
    if (g_InstanceSounds.capacity - g_InstanceSounds.used <= 0)
    {
        return 0xFF;
    }

    s32 index = AllocateInstanceSound(&g_InstanceSounds);
    InstanceSound* item = &g_InstanceSounds.items[static_cast<s16>(index)];
    item->voice = voice;
    item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StatePlaying;
    voice->bits = (voice->bits & ~SoundVoice::UseMask) | 0x200;
    item->bits |= InstanceSound::Repeats;
    AssignReference(&item->instance, instance);
    u32 bits = (item->bits & ~0xEu) | (group & InstanceSound::GroupMask) << InstanceSound::GroupShift;
    bits = (bits & ~0x1F0u) | (static_cast<u32>(voiceKind) & InstanceSound::KindMask) << InstanceSound::KindShift;
    item->sound = sound;
    item->volume = volume;
    item->pitch = pitch;
    item->bits = bits | InstanceSound::NotPlaced;
    item->frames = 0;
    return static_cast<u32>(index) & 0xFF;
}

u32 PlayInstanceSoundById(f32 volume, f32 pitch, u32 id, u32 group, InstanceContext* instance, s32 voiceKind, s32 last)
{
    constexpr u32 NeverRepeats = 0x1BF;
    GameSound* sound = SoundById(static_cast<u16>(id));
    if (sound == nullptr)
    {
        return 0xFF;
    }

    return PlayInstanceSoundAt(volume, pitch, sound, group, instance, voiceKind, (id & 0xFFFF) != NeverRepeats ? last : -1);
}

u32 PlayUnplacedSoundById(f32 volume, f32 pitch, u32 id, u32 group, InstanceContext* instance, s32 voiceKind)
{
    GameSound* sound = SoundById(static_cast<u16>(id));
    if (sound == nullptr)
    {
        return 0xFF;
    }

    return PlayInstanceSound(volume, pitch, sound, group, instance, voiceKind);
}

u32 PlaySoundByIdAt(f32 volume, f32 pitch, u16 id, s32 group, ChunkData* chunk, const Vector4* position, s32 voiceKind, s32 last)
{
    GameSound* sound = SoundById(id);
    if (sound == nullptr)
    {
        return 0;
    }

    return PlaySoundAtPosition(volume, pitch, sound, static_cast<u32>(group), chunk, position, voiceKind, last) != -1;
}

void StopInstanceSound(s32 index)
{
    if (g_InstanceSounds.links[index] != InUse)
    {
        return;
    }

    InstanceSound* item = &g_InstanceSounds.items[index];
    if ((item->bits & InstanceSound::StateMask) == InstanceSound::StatePlaying && (item->voice->bits >> 6 & 1) != 0)
    {
        SetVoiceVolume(0.0f, 0.0f, item->voice);
    }

    item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StateDone;
}

void SetInstanceSoundVolume(f32 volume, s32 index)
{
    if (g_InstanceSounds.links[index] != InUse)
    {
        return;
    }

    InstanceSound* item = &g_InstanceSounds.items[index];
    item->volume = volume;
    if ((item->bits & InstanceSound::StateMask) == InstanceSound::StatePlaying)
    {
        item->voice->volume = volume;
    }
}

void SetInstanceSoundPitch(f32 pitch, s32 index)
{
    if (g_InstanceSounds.links[index] != InUse)
    {
        return;
    }

    InstanceSound* item = &g_InstanceSounds.items[index];
    item->pitch = pitch;
    if ((item->bits & InstanceSound::StateMask) == InstanceSound::StatePlaying)
    {
        item->voice->pitchScale = pitch;
    }
}

u32 UpdateVoiceAt(SoundVoice* voice, GameSound* sound, const Vector4* position)
{
    f32 level = 1.0f;
    f32 volume = voice->volume;
    if (1.5f < volume)
    {
        level = 1.5f;
    }
    else if (0.0f < volume)
    {
        level = volume;
    }

    s32 pitch = sound->basePitch;
    if (0.0f < voice->pitchScale)
    {
        pitch = static_cast<s32>(static_cast<f32>(pitch) * voice->pitchScale);
    }

    f32 out[8];
    u32 heard = SoundAtPlace(level, g_SoundDistance, voice->unknown1C, pitch, position, out);
    if (heard != 0)
    {
        voice->unknown1C = out[4];
    }

    if ((heard & 0xFF) == 0)
    {
        return 0;
    }

    SetVoicePitch(voice, static_cast<s32>(out[2]));
    SetVoiceVolume(out[0], out[1], voice);
    return 1;
}

u32 SoundAtPlace(f32 volume, f32 distance, f32 previous, s32 pitch, const Vector4* position, f32* out)
{
    constexpr f32 ShareOfFront = 0.75f;
    constexpr f32 Behind = 0.25f;
    f32 range = distance;
    if (range == 0.0f)
    {
        range = DefaultRange;
    }

    f32 length = __builtin_sqrtf((position->x * position->x + position->y * position->y) + position->z * position->z);
    f32 level = volume - length / range;
    out[3] = length;
    if (!(Heard < level))
    {
        return 0;
    }

    f32 scale = static_cast<f32>(pitch);
    out[2] = scale;
    switch (g_MusicStereo)
    {
    case 0:
        out[1] = level;
        if (1.0f < level)
        {
            out[1] = 1.0f;
        }

        out[0] = out[1];
        return 1;
    case 1:
    {
        // Each ear hears what's ahead of it from three quarters of the volume up (the way's height left out)
        Vector4 way = *position;
        way.y = 0.0f;
        f32 inverse = InverseLength(&way, DirectionEpsilon);
        way.x = way.x * inverse;
        way.y = way.y * inverse;
        way.z = way.z * inverse;
        Vector4 earA = {Root2Half, 0.0f, Root2Half, 1.0f};
        Vector4 earB = {-Root2Half, 0.0f, Root2Half, 1.0f};
        f32 alongA = (earA.x * way.x + earA.y * way.y) + earA.z * way.z;
        f32 alongB = (earB.x * way.x + earB.y * way.y) + earB.z * way.z;
        f32 right = ((alongA * 0.5f + 0.5f) * ShareOfFront + Behind) * level;
        f32 left = ((alongB * 0.5f + 0.5f) * ShareOfFront + Behind) * level;
        out[1] = right;
        out[0] = left;
        if (1.0f < right)
        {
            out[1] = 1.0f;
            f32 spill = left + (right - 1.0f);
            out[0] = spill;
            if (1.0f < spill)
            {
                out[0] = 1.0f;
            }
        }
        else if (1.0f < left)
        {
            out[0] = 1.0f;
            f32 spill = right + (left - 1.0f);
            out[1] = spill;
            if (1.0f < spill)
            {
                out[1] = 1.0f;
            }
        }

        // A sound behind the listener plays up to a hundredth lower
        Vector4 forward = g_ZAxis;
        f32 ahead = (forward.x * way.x + forward.y * way.y) + forward.z * way.z;
        out[2] = scale;
        if (ahead < 0.0f)
        {
            out[2] = ahead * BehindPitch * scale + scale;
        }

        return 1;
    }
    case 2:
    {
        // Dolby Pro Logic II: the angle around the listener, unwound against the last one so the sides don't jump, panned
        // between the sides by its half angle
        Vector4 way = *position;
        way.y = 0.0f;
        f32 inverse = InverseLength(&way, DirectionEpsilon);
        way.x = way.x * inverse;
        way.y = way.y * inverse;
        way.z = way.z * inverse;
        Vector4 side = g_XAxis;
        f32 across = (side.x * way.x + side.y * way.y) + side.z * way.z;
        Vector4 forward = g_ZAxis;
        f32 ahead = (forward.x * way.x + forward.y * way.y) + forward.z * way.z;
        s32 turn;
        AngleOfPoint(&turn, across, ahead);
        f32 angle = static_cast<f32>(turn) * TurnToRadians;
        f32 wind = 0.0f;
        if (Pi <= previous)
        {
            // The last angle was past half a turn: this one is taken a turn on too
            wind = TwoPi;
            previous = previous - wind;
        }

        if (HalfPi < previous && angle < -HalfPi)
        {
            wind = wind + TwoPi;
        }

        if (previous < -HalfPi && HalfPi < angle)
        {
            wind = wind - TwoPi;
        }

        angle = angle + wind;
        if (ThreePi < angle)
        {
            angle = angle - FourPi;
        }
        else if (angle <= -Pi)
        {
            angle = angle + FourPi;
        }

        out[4] = angle;
        f32 sides[4];
        SinCosRadians(angle * 0.5f + QuarterPi, sides);
        if (1.0f < level)
        {
            level = 1.0f;
        }

        out[0] = sides[1] * level;
        out[1] = sides[0] * level;
        return 1;
    }
    default:
        return 1;
    }
}

u32 ChunkToListener(ChunkMatrixCache* cache, ChunkData* chunk, const Matrix4x4* view, Matrix4x4* out)
{
    if (chunk == nullptr || cache->frame != chunk->drawnFrame)
    {
        return 0;
    }

    if (cache->listenerChunk == chunk)
    {
        *out = *view;
        return 1;
    }

    s32 slot = chunk->particleView;
    if (slot < 0)
    {
        cache->chunks[cache->count] = chunk;
        slot = cache->count;
        chunk->particleView = slot;
        cache->matrices[slot] = chunk->drawMatrix;
        cache->count++;
    }

    VuMultiplyMatrices(&cache->matrices[slot], view, out);
    return 1;
}

void UpdateInstanceSounds()
{
    if (g_InstanceSounds.used <= 0)
    {
        return;
    }

    InstanceContext* listener = Listener();
    Matrix4x4 view;
    ListenerView(&view);
    ChunkData* listenerChunk = nullptr;
    if (g_RenderView != nullptr)
    {
        InstanceContext* camera = InstanceOf(g_RenderView->cameraObject);
        if (camera != nullptr)
        {
            listenerChunk = camera->chunk;
        }
    }

    if (listenerChunk == nullptr)
    {
        return;
    }

    ChunkMatrixCache cache;
    cache.listenerChunk = listenerChunk;
    cache.count = 0;
    cache.frame = g_RenderedFrames;
    SoundPoolIterator<InstanceSound> iterator = {g_InstanceSoundIteratorVTable, 0, 0, &g_InstanceSounds};
    InstanceSoundIteratorFirst(&iterator);
    while (!InstanceSoundIteratorDone(&iterator))
    {
        InstanceSound* item = InstanceSoundIteratorCurrent(&iterator);
        SoundVoice* voice = item->voice;
        GameSound* sound = item->sound;
        auto done = [item]() { item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StateDone; };
        auto silence = [&]() {
            SetVoiceVolume(0.0f, 0.0f, voice);
            done();
        };
        if (listener == nullptr || sound == nullptr)
        {
            if (voice != nullptr)
            {
                SetVoiceVolume(0.0f, 0.0f, voice);
            }

            done();
        }

        u32 bits = item->bits;
        if ((bits & InstanceSound::NotPlaced) != 0)
        {
            if ((voice->bits >> 6 & 1) == 0)
            {
                item->voice = nullptr;
                item->bits = (bits & ~InstanceSound::StateMask) | InstanceSound::StateDone;
            }
            else
            {
                item->frames++;
                if (InstanceOf(item->instance) == nullptr)
                {
                    silence();
                }
            }
        }
        else
        {
            switch (bits >> 9 & 7)
            {
            case 1:
                if ((voice->bits >> 6 & 1) == 0)
                {
                    item->bits = (bits & ~InstanceSound::StateMask) |
                                 ((bits & InstanceSound::Repeats) != 0 ? InstanceSound::StateWaiting : InstanceSound::StateDone);
                    item->voice = nullptr;
                    break;
                }

                item->frames++;
                if (InstanceContext* instance = InstanceOf(item->instance); instance == nullptr)
                {
                    if ((item->bits & InstanceSound::Repeats) != 0)
                    {
                        silence();
                    }
                }
                else
                {
                    Matrix4x4 toListener;
                    if (ChunkToListener(&cache, instance->chunk, &view, &toListener) == 0)
                    {
                        silence();
                        break;
                    }

                    ObjectPlace* place = instance->place;
                    RotateAndTranslate(place);
                    Vector4 at = *RowOf(&place->matrix, 3);
                    Vector4 local;
                    VuTransformPoint(&toListener, &at, &local);
                    if (UpdateVoiceAt(voice, sound, &local) == 0)
                    {
                        if ((item->bits & InstanceSound::Repeats) == 0)
                        {
                            silence();
                        }
                        else
                        {
                            item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StateWaiting;
                            SetVoiceVolume(0.0f, 0.0f, voice);
                        }
                    }
                }

                break;
            case 2:
            {
                if ((bits & InstanceSound::Repeats) == 0)
                {
                    item->bits = (bits & ~InstanceSound::StateMask) | InstanceSound::StateDone;
                    break;
                }

                if (item->voice != nullptr)
                {
                    item->frames++;
                    if (item->frames == 0)
                    {
                        break;
                    }

                    MuteVoice(item->voice);
                    item->voice = nullptr;
                }

                InstanceContext* instance = InstanceOf(item->instance);
                if (instance == nullptr)
                {
                    done();
                    break;
                }

                ObjectPlace* place = instance->place;
                place->SyncPosition();
                Vector4 position = place->position;
                s32 number = PlaySoundAtPosition(item->volume, item->pitch, item->sound,
                                                 item->bits >> InstanceSound::GroupShift & InstanceSound::GroupMask,
                                                 instance->chunk, &position,
                                                 static_cast<s32>(item->bits >> InstanceSound::KindShift & InstanceSound::KindMask), 0);
                if (number != -1)
                {
                    SoundVoice* played = VoiceOfNumber(static_cast<u32>(number));
                    item->voice = played;
                    item->bits = (item->bits & ~InstanceSound::StateMask) | InstanceSound::StatePlaying;
                    played->bits = (played->bits & ~SoundVoice::UseMask) | 0x200;
                    item->frames = 0;
                }

                break;
            }
            case 3:
                item->frames++;
                break;
            default:
                break;
            }
        }

        if ((item->bits & InstanceSound::StateMask) == InstanceSound::StateDone)
        {
            SoundVoice* playing = item->voice;
            if (playing == nullptr || item->frames != 0)
            {
                if (playing != nullptr && (playing->bits >> 6 & 1) != 0)
                {
                    MuteVoice(playing);
                }

                FreeInstanceSound(&g_InstanceSounds, iterator.index);
                iterator.position--;
                InstanceSoundIteratorNext(&iterator);
                continue;
            }
        }

        InstanceSoundIteratorNext(&iterator);
    }

    for (s32 index = 0; index < cache.count; index++)
    {
        cache.chunks[index]->particleView = -1;
    }
}

void UpdateMusicEmitters()
{
    if (g_MusicEmitters.used <= 0)
    {
        return;
    }

    Matrix4x4 view;
    ListenerView(&view);
    // Unlike the instances' sounds, there's no check for a view or a camera: without one the chunk is read from address 0xA0
    ChunkMatrixCache cache;
    cache.count = 0;
    cache.listenerChunk = InstanceOf(g_RenderView->cameraObject)->chunk;
    cache.frame = g_RenderedFrames;
    SoundPoolIterator<MusicEmitter> iterator = {g_MusicEmitterIteratorVTable, 0, 0, &g_MusicEmitters};
    MusicEmitterIteratorFirst(&iterator);
    while (!MusicEmitterIteratorDone(&iterator))
    {
        MusicEmitter* emitter = MusicEmitterIteratorCurrent(&iterator);
        InstanceContext* instance = InstanceOf(emitter->instance);
        if (instance == nullptr)
        {
            FreeMusicEmitter(&g_MusicEmitters, iterator.index);
            iterator.position--;
            MusicEmitterIteratorNext(&iterator);
            continue;
        }

        u32 heard = 0;
        f32 out[8];
        Matrix4x4 toListener;
        if (ChunkToListener(&cache, instance->chunk, &view, &toListener) != 0)
        {
            ObjectPlace* place = instance->place;
            RotateAndTranslate(place);
            Vector4 at = *RowOf(&place->matrix, 3);
            Vector4 local;
            VuTransformPoint(&toListener, &at, &local);
            if ((emitter->bits & MusicEmitter::Stereo) != 0)
            {
                // Retail copies what the stack held as its previous angle, which an interleaved track never reads
                f32 length = __builtin_sqrtf((local.x * local.x + local.y * local.y) + local.z * local.z);
                out[3] = length;
                f32 share = RemainingShare(length, DefaultRange, emitter->volume);
                if (HeardShare < share)
                {
                    out[1] = share;
                    out[2] = 1.0f;
                    if (1.0f < share)
                    {
                        out[1] = 1.0f;
                    }

                    out[0] = out[1];
                    heard = 1;
                }
            }
            else
            {
                heard = SoundAtPlace(emitter->volume, emitter->range, emitter->previous, 1, &local, out);
                emitter->previous = out[4];
            }

            emitter->distance = out[3];
        }

        if ((emitter->bits & MusicEmitter::HasPlayer) != 0)
        {
            if (heard != 0)
            {
                SetMusicVolume(out[0], out[1], emitter->player);
                SetMusicPitch(out[2], emitter->player);
            }
            else
            {
                MusicPlayer* player = emitter->player;
                player->bits = (player->bits & ~MusicPlayer::StateMask) | MusicPlayer::Stopping << 1;
                emitter->player = nullptr;
                emitter->bits &= ~MusicEmitter::HasPlayer;
            }
        }
        else if (heard != 0)
        {
            MusicPlayer* player = FirstStoppedPlayer();
            if (player == nullptr)
            {
                player = TakeMusicPlayer(out[3]);
            }

            if (player != nullptr)
            {
                // The request's word above its track is what the stack held (the instance's position), which nothing reads:
                // its group cleared, bit 19 and the loop set
                MusicRequest request;
                request.bits = (emitter->bits & MusicEmitter::TrackMask) | 0x80000 | (emitter->bits >> 17 & 1) << 20;
                request.left = out[0];
                request.right = out[1];
                request.fadeTime = 0.0f;
                StartTrack(player, &request);
                emitter->player = player;
                emitter->bits |= MusicEmitter::HasPlayer;
                SetMusicPitch(out[2], player);
            }
        }

        MusicEmitterIteratorNext(&iterator);
    }

    for (s32 index = 0; index < cache.count; index++)
    {
        cache.chunks[index]->particleView = -1;
    }
}

MusicPlayer* TakeMusicPlayer(f32 distance)
{
    MusicPlayer* taken = nullptr;
    f32 farthest = 0.0f;
    MusicEmitter* farthestEmitter = nullptr;
    SoundPoolIterator<MusicEmitter> iterator = {g_MusicEmitterIteratorVTable, 0, 0, &g_MusicEmitters};
    MusicEmitterIteratorFirst(&iterator);
    while (!MusicEmitterIteratorDone(&iterator))
    {
        MusicEmitter* emitter = MusicEmitterIteratorCurrent(&iterator);
        if ((emitter->bits & MusicEmitter::HasPlayer) != 0 && distance < emitter->distance && farthest < emitter->distance)
        {
            farthest = emitter->distance;
            farthestEmitter = emitter;
        }

        MusicEmitterIteratorNext(&iterator);
    }

    if (farthestEmitter != nullptr)
    {
        taken = farthestEmitter->player;
        StopMusic(taken);
        farthestEmitter->player = nullptr;
        farthestEmitter->bits &= ~MusicEmitter::HasPlayer;
    }

    if (taken == nullptr)
    {
        for (MusicPlayer& player : g_Music->players)
        {
            if (MusicPlayerState(&player) == MusicPlayer::Stopping ||
                (player.bits & (MusicPlayer::FadingOut | MusicPlayer::StateMask)) == (MusicPlayer::FadingOut | MusicPlayer::FadingOutState << 1))
            {
                StopMusic(&player);
                taken = &player;
            }
        }

        if (taken == nullptr)
        {
            taken = g_Music->playing[2] != nullptr ? g_Music->playing[2] : g_Music->fading[2];
            if (taken != nullptr)
            {
                StopMusic(taken);
            }
        }
    }

    for (u32 slot = 0; slot < 4; slot++)
    {
        if (taken == SlotPlayer(slot))
        {
            g_Music->playing[slot] = nullptr;
            g_Music->fading[slot] = nullptr;
        }
    }

    return taken;
}

void UpdatePlayingSounds(f32 time, bool paused)
{
    for (MusicPlayer& player : g_Music->players)
    {
        if (MusicPlayerState(&player) != MusicPlayer::Stopped)
        {
            UpdateMusic(time, &player, paused);
        }
    }

    MusicRequest* pending = PendingMusicRequests();
    for (u32 slot = 0; slot < 4; slot++)
    {
        MusicPlayer* player = SlotPlayer(slot);
        if (player != nullptr && MusicPlayerState(player) == MusicPlayer::Stopped)
        {
            g_Music->playing[slot] = nullptr;
            g_Music->fading[slot] = nullptr;
        }

        if (g_SoundSlots[slot] == 0 || SlotPlayer(slot) != nullptr)
        {
            continue;
        }

        MusicPlayer* free = FirstStoppedPlayer();
        if (free == nullptr)
        {
            free = TakeMusicPlayer(0.0f);
        }

        g_Music->playing[slot] = nullptr;
        g_Music->fading[slot] = free;
        StartTrack(free, &pending[slot]);
        g_SoundSlots[slot] = 0;
    }
}

s32 PlayMusicRequest(u32 slot, const MusicRequest* source)
{
    constexpr u32 PreparedBit = 0x80000;
    MusicPlayer* current = SlotPlayer(slot);
    MusicPlayer* player = FirstStoppedPlayer();
    MusicRequest request = *source;
    u32 track = source->bits & 0xFFFF;
    if (slot == 0)
    {
        if (track == 7 || track == 0x1E)
        {
            request.fadeTime = 0.0f;
        }
        else
        {
            request.fadeTime = static_cast<u16>(track - 0x3D) < 2 ? 1.0f : 3.0f;
        }
    }

    if (current == nullptr)
    {
        if (player == nullptr)
        {
            player = TakeMusicPlayer(0.0f);
        }

        if (slot == 0)
        {
            request.fadeTime = track == 0x1B ? 1.0f : 0.0f;
        }
    }
    else
    {
        if (current->track == (request.bits & 0xFFFF))
        {
            FadeMusicTo(request.left, request.right, request.fadeTime, current);
            return 1;
        }

        if (slot == 1)
        {
            StopMusic(current);
            player = current;
        }
        else if (slot == 0 || slot == 2)
        {
            FadeMusicOut(request.fadeTime, current, 1);
            if (player == nullptr)
            {
                g_SoundSlots[slot] = 1;
                PendingMusicRequests()[slot] = *source;
            }
        }
    }

    if (player == nullptr)
    {
        return 0;
    }

    if ((request.bits & PreparedBit) != 0)
    {
        g_Music->playing[slot] = nullptr;
        g_Music->fading[slot] = player;
    }
    else
    {
        g_Music->playing[slot] = player;
        g_Music->fading[slot] = nullptr;
    }

    StartTrack(player, &request);
    return 1;
}

void StopMusicEmitters()
{
    SoundPoolIterator<MusicEmitter> iterator = {g_MusicEmitterIteratorVTable, 0, 0, &g_MusicEmitters};
    MusicEmitterIteratorFirst(&iterator);
    while (!MusicEmitterIteratorDone(&iterator))
    {
        MusicEmitter* emitter = MusicEmitterIteratorCurrent(&iterator);
        if ((emitter->bits & MusicEmitter::HasPlayer) != 0)
        {
            StopMusic(emitter->player);
            emitter->player = nullptr;
            emitter->bits &= ~MusicEmitter::HasPlayer;
        }

        MusicEmitterIteratorNext(&iterator);
    }

    ClearMusicEmitters(&g_MusicEmitters);
}

u32 AddMusicEmitter(f32 range, InstanceContext* instance, const MusicRequest* request)
{
    if (g_MusicEmitters.capacity - g_MusicEmitters.used <= 0)
    {
        return 0;
    }

    s32 index = AllocateMusicEmitter(&g_MusicEmitters);
    MusicEmitter* emitter = &g_MusicEmitters.items[static_cast<s16>(index)];
    AssignReference(&emitter->instance, instance);
    emitter->player = nullptr;
    emitter->bits = (emitter->bits & ~MusicEmitter::TrackMask) | (request->bits & 0xFFFF);
    emitter->distance = 0.0f;
    emitter->bits &= ~MusicEmitter::HasPlayer;
    emitter->range = range;
    emitter->bits = (emitter->bits & ~MusicEmitter::Loops) | (request->bits >> 20 & 1) << 17;
    emitter->previous = 0.0f;
    emitter->volume = (request->left + request->right) * 0.5f;
    u32 track = request->bits & 0xFFFF;
    SoundBankFiles* bank = FindMusicTrack(&g_MusicBank, track)->interleaved == 2 ? &g_VoiceBank : &g_MusicBank;
    emitter->bits = (emitter->bits & ~MusicEmitter::Stereo) | static_cast<u32>(FindMusicTrack(bank, track)->interleaved == 1) << 18;
    return 1;
}

void* LendMusicBuffer()
{
    for (MusicPlayer& player : g_Music->players)
    {
        if (MusicPlayerState(&player) == MusicPlayer::LentToMovie)
        {
            return reinterpret_cast<void*>(player.channelValue);
        }
    }

    MusicPlayer* player = FirstStoppedPlayer();
    if (player == nullptr)
    {
        player = TakeMusicPlayer(0.0f);
        if (player == nullptr)
        {
            return nullptr;
        }
    }

    auto* buffer = reinterpret_cast<void*>(player->channelValue);
    if (buffer != nullptr)
    {
        LendMusicPlayer(player, 1);
    }

    return buffer;
}

u32 PlayPreparedMusic(f32 left, f32 right, f32 fadeTime, s32 slot)
{
    MusicPlayer* player = SlotPlayer(static_cast<u32>(slot));
    if (player == nullptr)
    {
        return 0;
    }

    if (g_Music->playing[slot] == nullptr)
    {
        return 1;
    }

    if (MusicPlayerState(player) != MusicPlayer::Ready)
    {
        return 0;
    }

    g_Music->fading[slot] = g_Music->playing[slot];
    g_Music->playing[slot] = nullptr;
    PlayMusic(left, right, fadeTime, player);
    return 1;
}

u32 FadeOutMusicSlot(f32 time, s32 slot)
{
    MusicPlayer* player = SlotPlayer(static_cast<u32>(slot));
    if (player == nullptr)
    {
        return 0;
    }

    FadeMusicOut(time, player, 1);
    return 1;
}

u32 MusicSlotPrepared(s32 slot)
{
    MusicPlayer* player = g_Music->playing[slot];
    return player != nullptr ? MusicPlayerState(player) == MusicPlayer::Ready : 0;
}

u32 MusicSlotPlaying(s32 slot)
{
    MusicPlayer* player = g_Music->fading[slot];
    return player != nullptr ? MusicPlayerState(player) - MusicPlayer::Ready < 5 : 0;
}

EABI_EXPORT(FUN_001df3c8, PlaySoundAtPosition);
EABI_EXPORT(FUN_001dee98, PlayInstanceSoundAt);
EABI_EXPORT(FUN_001df178, PlayInstanceSound);
EABI_EXPORT(FUN_001e5c68, PlayInstanceSoundById);
EABI_EXPORT(FUN_001e5e78, PlayUnplacedSoundById);
EABI_EXPORT(PlaySound_, PlaySoundByIdAt);
EABI_EXPORT(FUN_001e58f8, SetInstanceSoundVolume);
EABI_EXPORT(FUN_001e5950, SetInstanceSoundPitch);
EABI_EXPORT(FUN_001e0a20, SoundAtPlace);
EABI_EXPORT(FUN_001dddd0, UpdatePlayingSounds);
EABI_EXPORT(FUN_001de2a0, AddMusicEmitter);
EABI_EXPORT(FUN_001e59b0, PlayPreparedMusic);
EABI_EXPORT(FUN_001e5a20, FadeOutMusicSlot);
