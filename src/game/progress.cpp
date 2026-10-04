#include "game/progress.h"

#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/memory.h"
#include "game/oleg.h"
#include "game/pads.h"
#include "game/renderer.h"
#include "game/sound.h"
#include "game/stream.h"
#include "retail/libc.h"

namespace
{
constexpr u16 NoInstance = 0xFFFF;
// The node kinds a checkpoint reads: where a character comes back to, and the agent whose state flags say where the instance's
// persistent flag is
constexpr u32 NodeComeback = 1;
constexpr u32 NodeAgent = 0xD;
// The event a checkpoint's instance is told it isn't the checkpoint any more with
constexpr u32 CheckpointReleasedEvent = 0xF;
// The clock the time played is counted by
constexpr u32 PlayClock = 2;
// A new game starts with the second character paired alone and five lives
constexpr u32 PairingAlone = 1;
constexpr u32 StartLives = 5;

// The instance the checkpoint is at, when its chunk is one of the chunk manager's (and the chunk)
InstanceContext* InstanceAt(const Checkpoint* checkpoint, ChunkManager* chunks, ChunkEntry** chunk)
{
    ChunkEntry* found = chunks != nullptr ? FindChunkEntry(chunks, checkpoint->chunk.string) : nullptr;
    if (chunk != nullptr)
    {
        *chunk = found;
    }

    return found != nullptr ? FindChunkInstance(found, checkpoint->id) : nullptr;
}

// The store the instance's persistent flag is in (none when it has none)
PersistentFlags* PersistentFlagsOf(InstanceContext* instance, ChunkEntry* chunk)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeAgent));
    if (node == nullptr)
    {
        return nullptr;
    }

    u32 state = node->agent->properties->state;
    if ((state & PropertyHolder::StatePersistentFlag) == 0)
    {
        return nullptr;
    }

    return (state & PropertyHolder::StateFlagInChunkStore) != 0 ? chunk->flags : chunk->otherFlags;
}

// The character the agent node's instance is (6 for none)
u32 CharacterOf(AgentNode* node)
{
    if (node == nullptr)
    {
        return GameProgress::NoCharacter;
    }

    return static_cast<u32>(node->agent->properties->GetInt(0)) & Checkpoint::FieldMask;
}

u32 WithField(u32 bits, u32 shift, u32 value)
{
    return (bits & ~(u32{0xF} << shift)) | (value & 0xF) << shift;
}

// Whether the loader's loaded as asked: 1 the chunk, 2 with every linked chunk when the manager queues them (its state isn't
// 0), anything else yes; 0 isn't at first, then (retail) whether every loader under the manager's path is
bool LoadedAs(ChunkLoadingManager* loading, ChunkLoader* loader, s32 how, bool again)
{
    switch (how)
    {
    case 0:
        return again && ((loading->bits ^ loading->bits >> 12) & 0xFFF) == 0;
    case 1:
        return ChunkLoaderIsLoaded(loader, true);
    case 2:
        return ChunkLoaderIsLoaded(loader, true) && ((loading->bits >> 24 & 0xF) == 0 || LinkedChunksLoaded(loader, true));
    default:
        return true;
    }
}
}

GameProgress* GameProgress::Construct(GameProgress* progress)
{
    String* const strings[] = {&progress->startChunk, &progress->chunk1C, &progress->chunk28};
    for (String* string : strings)
    {
        string->string = nullptr;
        string->capacity = 0;
        string->length = 0;
    }

    for (u32& level : progress->levels)
    {
        level = 0;
    }

    for (Reference*& character : progress->characters)
    {
        character = nullptr;
    }

    RetailLibc::MemorySet(progress, 0, 8);
    progress->startTime = 0;
    progress->timePlayed = 0;
    for (Checkpoint*& checkpoint : progress->checkpoints)
    {
        checkpoint = Checkpoint::Construct(static_cast<Checkpoint*>(MemoryAllocate(sizeof(Checkpoint))));
    }

    progress->Reset(0, nullptr);
    return progress;
}

Checkpoint* GameProgress::Enter(u32 way, String* chunk, u16* place, GameProgress* progress, ChunkManager* chunks)
{
    Checkpoint* start;
    if (way == 2)
    {
        u32 pairing = Field(PairingShift);
        InstanceContext* character = Instance(Field(CharacterShift));
        InstanceContext* second = Instance(Field(SecondShift));
        ChunkEntry* entry = FindChunkEntry(chunks, chunk->string);
        InstanceContext* instance = entry != nullptr ? FindChunkInstance(entry, *place) : nullptr;
        checkpoints[1]->Clear(0, this, chunks);
        checkpoints[2]->Clear(0, this, chunks);
        checkpoints[0]->Clear(0, this, chunks);
        checkpoints[1]->Set(pairing, chunks, character, character, second);
        if (instance != nullptr)
        {
            start = checkpoints[2]->Set(pairing, chunks, instance, character, second) ? checkpoints[2] : checkpoints[1];
        }
        else
        {
            start = checkpoints[1];
        }
    }
    else
    {
        start = Reset(way, chunks);
    }

    if (start == nullptr)
    {
        return nullptr;
    }

    start->Restore(way, progress, chunks);
    return start;
}

Checkpoint* GameProgress::Reset(u32 way, ChunkManager* chunks)
{
    timeLimit = 0;
    timeLeft = 0;
    counts &= ~(HealthMask << MostHealthShift) & ~(HealthMask << HealthShift);
    bits &= ~ModeMask;
    for (Reference* reference : characters)
    {
        auto* instance = reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
        void* node = instance != nullptr ? GetGameNode(&instance->nodes, NodeComeback) : nullptr;
        if (node != nullptr)
        {
            ClearComebackPlacement(node);
        }
    }

    Checkpoint* start = nullptr;
    switch (way)
    {
    case 0:
    case 1:
    {
        u32 state = WithField(bits, PairingShift, PairingAlone);
        state = WithField(state, CharacterShift, NoCharacter);
        state = WithField(state, SecondShift, NoCharacter);
        state &= ~(AreaMask << AreaShift) & ~(AreaMask << StoryShift) & ~(AreaMask << OpenShift);
        counts = (counts & ~WumpaMask & ~(LivesMask << LivesShift)) | StartLives << LivesShift;
        bits = state;
        SetTimePlayed(0);
        bool started;
        if (way == 1)
        {
            started = checkpoints[1]->Restore(1, this, chunks);
        }
        else if (chunks == nullptr)
        {
            checkpoints[1]->Clear(way, this, nullptr);
            started = false;
        }
        else
        {
            InstanceContext* character = Instance(Field(CharacterShift));
            started = checkpoints[1]->Set(1, chunks, character, character, Instance(Field(SecondShift)));
        }

        if (started)
        {
            start = checkpoints[1];
        }

        checkpoints[0]->Clear(way, this, chunks);
        checkpoints[2]->Clear(way, this, chunks);
        for (u32& level : levels)
        {
            RetailLibc::MemorySet(&level, 0, sizeof(level));
        }

        break;
    }
    case 2:
        counts &= ~WumpaMask;
        if (checkpoints[2]->Restore(2, this, chunks))
        {
            checkpoints[0]->Clear(2, this, chunks);
            start = checkpoints[2];
        }
        else if (checkpoints[1]->Restore(2, this, chunks))
        {
            start = checkpoints[1];
        }

        break;
    case 3:
        if (checkpoints[0]->Restore(3, this, chunks))
        {
            start = checkpoints[0];
        }
        else if (checkpoints[2]->Restore(3, this, chunks))
        {
            start = checkpoints[2];
        }
        else if (checkpoints[1]->Restore(3, this, chunks))
        {
            start = checkpoints[1];
        }

        break;
    default:
        break;
    }

    if (start == nullptr)
    {
        return nullptr;
    }

    bits = WithField(bits, PairingShift, start->bits >> Checkpoint::PairingShift);
    bits = WithField(bits, CharacterShift, start->bits >> Checkpoint::CharacterShift);
    bits = WithField(bits, SecondShift, start->bits >> Checkpoint::SecondShift);
    return start;
}

s32 TokenArea(u32 token)
{
    // The tokens 0x26F to 0x286 but 0x278 name the areas 0 to 24 but 11 and 14
    static const s8 Areas[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, -1, 9, 10, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    u32 index = token - 0x26F;
    return index < sizeof(Areas) ? Areas[index] : -1;
}

u32 GameProgress::ChunkLoaded(u32 how, u32 wait)
{
    ChunkLoadingManager* loading = G_ChunkLoadingManager_;
    ChunkLoader* loader = FindChunkLoader(loading, &startChunk);
    bool loaded = LoadedAs(loading, loader, static_cast<s32>(how), false);
    while (!loaded)
    {
        if (wait != 0)
        {
            u8 read;
            UpdateChunkLoading(loading, true, &read);
        }

        loaded = LoadedAs(loading, loader, static_cast<s32>(how), true);
        if (wait == 0)
        {
            break;
        }
    }

    return loaded;
}

void SaveController::Read(Stream* stream)
{
    stream->Read(this, 8, 1);
    stream->ReadU32(reinterpret_cast<u32*>(&timePlayed));
    StringRead(&chunk, stream);
    stream->ReadS16(reinterpret_cast<s16*>(&place));
    stream->Read(&screenOffset, sizeof(screenOffset), 1);
    stream->ReadF32(&effectsVolume);
    stream->ReadF32(&musicVolume);
    stream->Read(data, DataSize, 1);
}

void SaveController::Write(Stream* stream)
{
    stream->Write(this, 8);
    stream->WriteU32(static_cast<u32>(timePlayed));
    StringWrite(&chunk, stream);
    stream->WriteS16(static_cast<s16>(place));
    stream->Write(&screenOffset, sizeof(screenOffset));
    stream->WriteF32(effectsVolume);
    stream->WriteF32(musicVolume);
    stream->Write(data, DataSize);
}

void SaveController::Restore(u32 withData, GameProgress* progress, ChunkManager* chunks)
{
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    GameRendererController* renderer = G_GameRendererController;
    progress->counts = (progress->counts & ~(GameProgress::LivesMask << GameProgress::LivesShift)) |
                       (summary & SummaryLivesMask) << GameProgress::LivesShift;
    u32 bits = progress->bits & ~(GameProgress::AreaMask << GameProgress::AreaShift);
    bits |= (options & GameProgress::AreaMask) << GameProgress::AreaShift;
    bits = (bits & ~(GameProgress::AreaMask << GameProgress::StoryShift)) | (options >> 5 & GameProgress::AreaMask) << GameProgress::StoryShift;
    bits = (bits & ~(GameProgress::AreaMask << GameProgress::OpenShift)) | (options >> 10 & GameProgress::AreaMask) << GameProgress::OpenShift;
    bits = WithField(bits, GameProgress::PairingShift, summary >> SummaryPairingShift);
    bits = WithField(bits, GameProgress::CharacterShift, summary >> SummaryCharacterShift);
    bits = WithField(bits, GameProgress::SecondShift, summary >> SummarySecondShift);
    progress->bits = bits;
    progress->timePlayed = timePlayed;
    progress->startTime = static_cast<s32>(G_GameClockController->clocks[PlayClock].time);
    if (withData != 0)
    {
        MemoryStream reader;
        MemoryStream::Construct(&reader, data, DataSize, 0, 1);
        Stream* stream = &reader;
        for (u32& level : progress->levels)
        {
            stream->Read(&level, sizeof(level), 1);
        }

        ReadChunkStates(chunks, stream);
        reader.Destroy(DestroyOnly);
    }

    if ((options & OptionVibration) != 0)
    {
        pads->flags |= GamePadController::FlagVibration;
    }
    else
    {
        pads->DisableVibration();
    }

    SetGroupVolume(effectsVolume, effectsVolume, 0);
    SetGroupVolume(effectsVolume, effectsVolume, 1);
    SetGroupVolume(effectsVolume, effectsVolume, 3);
    SetGroupVolume(musicVolume, musicVolume, 2);
    SetMusicStereo(options >> OptionMusicStereoShift & OptionMusicStereoMask);
    g_WidescreenTv = static_cast<u8>(options >> 16 & 1);
    renderer->SetScreenOffset(&screenOffset);
}

void SaveController::Take(GameProgress* progress, ChunkManager* chunks, Checkpoint* checkpoint)
{
    MemoryStream writer;
    MemoryStream::Construct(&writer, data, DataSize, 0, 1);
    ChunkLoader* focus = G_ChunkLoadingManager_->focusLoader;
    StringAssign(&chunk, (focus != nullptr ? &focus->path : &progress->startChunk)->string);
    place = checkpoint != nullptr ? checkpoint->id : NoInstance;
    summary = (summary & ~SummaryLivesMask) | (progress->counts >> GameProgress::LivesShift & GameProgress::LivesMask);
    u32 crystals = 0;
    for (u32 level : progress->levels)
    {
        crystals += level >> 8 & 1;
    }

    summary = (summary & ~(SummaryCrystalsMask << SummaryCrystalsShift)) | (crystals & SummaryCrystalsMask) << SummaryCrystalsShift;
    summary = (summary & ~(SummaryDoneMask << SummaryDoneShift)) | (progress->Done() & SummaryDoneMask) << SummaryDoneShift;
    u32 areas = progress->bits >> GameProgress::AreaShift;
    options = (options & ~0x1Fu) | (areas & 0x1F);
    options = (options & ~0x3E0u) | (areas & 0x3E0);
    options = (options & ~0x7C00u) | (areas & 0x7C00);
    summary = WithField(summary, SummaryPairingShift, progress->Field(GameProgress::PairingShift));
    summary = WithField(summary, SummaryCharacterShift, progress->Field(GameProgress::CharacterShift));
    summary = (summary & 0x0FFFFFFF) | (progress->bits >> GameProgress::SecondShift) << SummarySecondShift;
    s32 played = progress->startTime != 0 ? static_cast<s32>(G_GameClockController->clocks[PlayClock].time) - progress->startTime : 0;
    timePlayed = played + progress->timePlayed;
    Stream* stream = &writer;
    for (u32& level : progress->levels)
    {
        stream->Write(&level, sizeof(level));
    }

    WriteChunkStates(chunks, stream);
    TakeOptions();
    writer.Destroy(DestroyOnly);
}

bool Checkpoint::Restore(u32 pairing, GameProgress* progress, ChunkManager* chunks)
{
    if ((bits & BitSet) == 0)
    {
        return false;
    }

    ChunkEntry* entry;
    InstanceContext* instance = InstanceAt(this, chunks, &entry);
    InstanceContext* first = progress->Instance(bits >> CharacterShift & FieldMask);
    if (instance != nullptr)
    {
        PersistentFlags* flags = PersistentFlagsOf(instance, entry);
        if (flags != nullptr)
        {
            SetPersistentFlag(flags, id, 1);
        }
    }

    bits |= BitUsed;
    if (first == nullptr)
    {
        return true;
    }

    void* node = GetGameNode(&first->nodes, NodeComeback);
    InstanceContext* other = progress->Instance(bits >> SecondShift & FieldMask);
    SetComebackPlacement(node, &character);
    character.Apply(first);
    if (other != nullptr)
    {
        void* otherNode = GetGameNode(&other->nodes, NodeComeback);
        second.Assign(&character);
        SetComebackPlacement(otherNode, &second);
        second.Apply(other);
    }

    return true;
}

bool Checkpoint::Set(u32 pairing, ChunkManager* chunks, InstanceContext* instance, InstanceContext* first, InstanceContext* other)
{
    if (InstanceAt(this, chunks, nullptr) == instance)
    {
        return instance != nullptr && (bits & BitUsed) == 0;
    }

    AgentNode* agentNode = AgentNodeOf(instance);
    Agent* agent = agentNode != nullptr ? agentNode->agent : nullptr;
    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&first->nodes, NodePlayer));
    AgentNode* otherNode = other != nullptr ? static_cast<AgentNode*>(GetGameNode(&other->nodes, NodePlayer)) : nullptr;
    ChunkLoader* focus = G_ChunkLoadingManager_->focusLoader;
    Release(0, chunks, instance);
    bits = WithField(bits | BitSet, PairingShift, pairing);
    character.Take(instance, 0);
    StringAssign(&chunk, focus->path.string);
    id = agent->id;
    bits = WithField(bits, CharacterShift, CharacterOf(firstNode));
    bits = WithField(bits, SecondShift, CharacterOf(otherNode));
    return true;
}

AgentNode* AgentNodeOf(InstanceContext* instance)
{
    static const u32 Kinds[] = {0xD, 0xE, 0xF, 0xC, 0x10, 0x11, 0x14, 0x12, 0x13};
    for (u32 kind : Kinds)
    {
        if (instance->nodes.nodes[kind] != nullptr)
        {
            return static_cast<AgentNode*>(GetGameNode(&instance->nodes, kind));
        }
    }

    return nullptr;
}

AgentNode* AgentNodeOf2(InstanceContext* instance)
{
    return AgentNodeOf(instance);
}

s32 GameProgress::GemsFound(u32 gem)
{
    s32 count = 0;
    for (u32 level : levels)
    {
        if ((static_cast<u8>(level) & 1 << gem) != 0)
        {
            count++;
        }
    }

    return count;
}

u32 GameProgress::Done()
{
    u32 gems = 0;
    for (u32 gem = 0; gem < Gems; gem++)
    {
        gems += GemsFound(gem);
    }

    return g_StoryDone[bits >> StoryShift & AreaMask] + gems / 3;
}

void GameProgress::SetTimePlayed(s32 time)
{
    timePlayed = time;
    startTime = static_cast<s32>(G_GameClockController->clocks[PlayClock].time);
}

u32 GameProgress::AddHealth(s32 amount)
{
    s32 health = static_cast<s32>(counts >> HealthShift & HealthMask) + amount;
    if (health < 0)
    {
        counts &= ~(HealthMask << HealthShift);
        return 1;
    }

    s32 most = static_cast<s32>(counts >> MostHealthShift & HealthMask);
    if (health < most)
    {
        counts = (counts & ~(HealthMask << HealthShift)) | (static_cast<u32>(health) & HealthMask) << HealthShift;
        return 0;
    }

    counts = (counts & ~(HealthMask << HealthShift)) | static_cast<u32>(most) << HealthShift;
    return 1;
}

u32 GameProgress::AddToCount(s32 amount)
{
    s32 count = static_cast<s32>(counts >> CountShift) + amount;
    if (count < 0)
    {
        counts &= ~(CountMask << CountShift);
        return 1;
    }

    s32 total = static_cast<s32>(counts >> CountTotalShift & CountMask);
    if (count < total)
    {
        counts = (counts & ~(CountMask << CountShift)) | static_cast<u32>(count) << CountShift;
        return 0;
    }

    counts = (counts & ~(CountMask << CountShift)) | static_cast<u32>(total) << CountShift;
    return 1;
}

u32 TokenPlayerMode(u32 token)
{
    u32 index = token - 0x291;
    return index < 6 ? index + 1 : 0;
}

void GameProgress::ForgetChunks()
{
    StringAssign(&startChunk, "");
    StringAssign(&chunk1C, "");
    StringAssign(&chunk28, "");
}

u32 GameProgress::LoadStartChunk(ChunkManager* chunks)
{
    if (startChunk.length == 0)
    {
        return 0;
    }

    ChunkLoadingManager* loading = G_ChunkLoadingManager_;
    UnloadEverything(loading, true, chunks);
    QueueChunk(loading, &startChunk, 0);
    return 1;
}

void SaveController::TakeOptions()
{
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    GameRendererController* renderer = G_GameRendererController;
    options = (options & ~OptionVibration) | (pads->flags << 6 & OptionVibration);
    effectsVolume = GroupVolumeLevel(0);
    musicVolume = GroupVolumeLevel(2);
    options = (options & ~(OptionMusicStereoMask << OptionMusicStereoShift)) | (g_MusicStereo & OptionMusicStereoMask) << OptionMusicStereoShift;
    options = (options & ~OptionWidescreen) | (g_WidescreenTv & 1u) << 16;
    screenOffset.x = renderer->screenOffset.x;
    screenOffset.y = renderer->screenOffset.y;
}

Checkpoint* Checkpoint::Construct(Checkpoint* checkpoint)
{
    checkpoint->chunk.string = nullptr;
    checkpoint->chunk.capacity = 0;
    checkpoint->chunk.length = 0;
    checkpoint->id = NoInstance;
    InstancePlacement::Construct(&checkpoint->character, nullptr);
    InstancePlacement::Construct(&checkpoint->second, nullptr);
    RetailLibc::MemorySet(checkpoint, 0, 4);
    return checkpoint;
}

void Checkpoint::Clear(u32 way, GameProgress* progress, ChunkManager* chunks)
{
    ChunkEntry* entry;
    InstanceContext* instance = InstanceAt(this, chunks, &entry);
    if (instance != nullptr)
    {
        PersistentFlags* flags = PersistentFlagsOf(instance, entry);
        if (flags != nullptr)
        {
            SetPersistentFlag(flags, id, 0);
        }
    }

    id = NoInstance;
    RetailLibc::MemorySet(this, 0, 4);
}

bool Checkpoint::Release(u32 forget, ChunkManager* chunks, InstanceContext* keep)
{
    InstanceContext* current = InstanceAt(this, chunks, nullptr);
    bool released = current != nullptr && current != keep;
    if (released)
    {
        auto* node = static_cast<AgentNode*>(GetGameNode(&current->nodes, NodeAgent));
        if (node != nullptr)
        {
            RunAgentEvent(node->agent, CheckpointReleasedEvent, 0, 0, 0);
        }
    }

    if (forget != 0)
    {
        id = NoInstance;
        RetailLibc::MemorySet(this, 0, 4);
    }

    bits &= ~BitUsed;
    return released;
}

u32 MarkGem(u8* level, u32 gem)
{
    u8 bit = static_cast<u8>(1u << gem);
    if ((*level & bit) != 0)
    {
        return 0;
    }

    *level |= bit;
    return 1;
}

u32 MarkCrystal(u32* level)
{
    constexpr u32 Crystal = 0x100;
    if ((*level & Crystal) != 0)
    {
        return 0;
    }

    *level |= Crystal;
    return 1;
}

u32* ConstructLevelWord(u32* level)
{
    RetailLibc::MemorySet(level, 0, sizeof(*level));
    return level;
}

void ReadLevelWord(u32* level, Stream* stream)
{
    stream->Read(level, sizeof(*level), 1);
}

void WriteLevelWord(const u32* level, Stream* stream)
{
    stream->Write(level, sizeof(*level));
}

s32 TokenGem(u32 token)
{
    // The keywords 0x288 to 0x28D name the gems 0 to 5
    constexpr u32 FirstGemToken = 0x288;
    u32 gem = token - FirstGemToken;
    return gem < GameProgress::Gems ? static_cast<s32>(gem) : -1;
}
