#include "game/progress.h"

#include "game/characters.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/memory.h"
#include "game/oleg.h"
#include "game/pads.h"
#include "game/renderer.h"
#include "game/scripttokens.h"
#include "game/sound.h"
#include "game/stream.h"
#include "retail/libc.h"

namespace
{
// The node a character comes back to a checkpoint through (its object node's comeback placement), and the checkpoint's own
// agent node (a checkpoint crate's), whose state flags say where the instance's persistent flag is
constexpr u32 ComebackNode = NodeObject;
constexpr u32 CheckpointNode = NodeCrate;
// The event a checkpoint's crate is told it isn't the checkpoint any more with
constexpr u32 CheckpointReleasedEvent = 0xF;
// The keywords naming the pairings (from KeywordFirstPlayerMode: 1 to 6)
constexpr u32 PairingTokens = 6;

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
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CheckpointNode));
    if (node == nullptr)
    {
        return nullptr;
    }

    InstanceState state = node->agent->properties->state;
    if (state.persistentFlag == 0)
    {
        return nullptr;
    }

    return state.flagInChunkStore != 0 ? chunk->savedFlags : chunk->unsavedFlags;
}

// The character the agent node's instance is (none for no node)
u32 CharacterOf(AgentNode* node)
{
    if (node == nullptr)
    {
        return GameProgress::NoCharacter;
    }

    return static_cast<u32>(node->agent->properties->GetInt(CharacterKindProperty));
}

// Whether the loader's loaded as asked: the chunk, with every linked chunk when the manager queues them (its mode isn't
// LoadingQueuedOnly), anything else yes; LoadedAll isn't at first, then (retail) whether every loader under the manager's path is
bool LoadedAs(ChunkLoadingManager* loading, ChunkLoader* loader, s32 how, bool again)
{
    switch (how)
    {
    case GameProgress::LoadedAll:
        return again && loading->bits.loadedCount == loading->bits.loaderCount;
    case GameProgress::LoadedChunk:
        return ChunkLoaderIsLoaded(loader, true);
    case GameProgress::LoadedWithLinks:
        return ChunkLoaderIsLoaded(loader, true) && (loading->bits.mode == LoadingQueuedOnly || LinkedChunksLoaded(loader, true));
    default:
        return true;
    }
}
}

GameProgress* GameProgress::Construct(GameProgress* progress)
{
    String* const strings[] = {&progress->startChunk, &progress->unused1C, &progress->unused28};
    for (String* string : strings)
    {
        string->string = nullptr;
        string->capacity = 0;
        string->length = 0;
    }

    for (LevelProgress& level : progress->levels)
    {
        level.value = 0;
    }

    for (Reference*& character : progress->characters)
    {
        character = nullptr;
    }

    // The counts and the play's state
    RetailLibc::MemorySet(progress, 0, sizeof(progress->counts) + sizeof(progress->play));
    progress->startTime = 0;
    progress->timePlayed = 0;
    for (Checkpoint*& checkpoint : progress->checkpoints)
    {
        checkpoint = Checkpoint::Construct(static_cast<Checkpoint*>(MemoryAllocate(sizeof(Checkpoint))));
    }

    progress->Reset(EntryNewGame, nullptr);
    return progress;
}

Checkpoint* GameProgress::Enter(u32 way, String* chunk, u16* place, GameProgress* progress, ChunkManager* chunks)
{
    Checkpoint* start;
    if (way == EntrySaved)
    {
        u32 pairing = play.pairing;
        InstanceContext* character = Instance(play.character);
        InstanceContext* second = Instance(play.second);
        ChunkEntry* entry = FindChunkEntry(chunks, chunk->string);
        InstanceContext* instance = entry != nullptr ? FindChunkInstance(entry, *place) : nullptr;
        checkpoints[CheckpointStart]->Clear(EntryNewGame, this, chunks);
        checkpoints[CheckpointSaved]->Clear(EntryNewGame, this, chunks);
        checkpoints[CheckpointRespawn]->Clear(EntryNewGame, this, chunks);
        checkpoints[CheckpointStart]->Set(pairing, chunks, character, character, second);
        if (instance != nullptr)
        {
            start = checkpoints[CheckpointSaved]->Set(pairing, chunks, instance, character, second) ? checkpoints[CheckpointSaved]
                                                                                                    : checkpoints[CheckpointStart];
        }
        else
        {
            start = checkpoints[CheckpointStart];
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
    counts.mostHealth = 0;
    counts.health = 0;
    play.mode = PlayNormal;
    for (Reference* reference : characters)
    {
        auto* instance = reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
        void* node = instance != nullptr ? GetGameNode(&instance->nodes, ComebackNode) : nullptr;
        if (node != nullptr)
        {
            ClearComebackPlacement(node);
        }
    }

    Checkpoint* start = nullptr;
    switch (way)
    {
    case EntryNewGame:
    case EntryNewGameFromStart:
    {
        PlayState state = play;
        state.pairing = PairingAlone;
        state.character = NoCharacter;
        state.second = NoCharacter;
        state.area = 0;
        state.story = 0;
        state.open = 0;
        counts.wumpa = 0;
        counts.lives = StartLives;
        play = state;
        SetTimePlayed(0);
        bool started;
        if (way == EntryNewGameFromStart)
        {
            started = checkpoints[CheckpointStart]->Restore(EntryNewGameFromStart, this, chunks);
        }
        else if (chunks == nullptr)
        {
            checkpoints[CheckpointStart]->Clear(way, this, nullptr);
            started = false;
        }
        else
        {
            // The characters are none by now
            InstanceContext* character = Instance(play.character);
            started = checkpoints[CheckpointStart]->Set(PairingAlone, chunks, character, character, Instance(play.second));
        }

        if (started)
        {
            start = checkpoints[CheckpointStart];
        }

        checkpoints[CheckpointRespawn]->Clear(way, this, chunks);
        checkpoints[CheckpointSaved]->Clear(way, this, chunks);
        for (LevelProgress& level : levels)
        {
            RetailLibc::MemorySet(&level, 0, sizeof(level));
        }

        break;
    }
    case EntrySaved:
        counts.wumpa = 0;
        if (checkpoints[CheckpointSaved]->Restore(EntrySaved, this, chunks))
        {
            checkpoints[CheckpointRespawn]->Clear(EntrySaved, this, chunks);
            start = checkpoints[CheckpointSaved];
        }
        else if (checkpoints[CheckpointStart]->Restore(EntrySaved, this, chunks))
        {
            start = checkpoints[CheckpointStart];
        }

        break;
    case EntryCheckpoint:
        if (checkpoints[CheckpointRespawn]->Restore(EntryCheckpoint, this, chunks))
        {
            start = checkpoints[CheckpointRespawn];
        }
        else if (checkpoints[CheckpointSaved]->Restore(EntryCheckpoint, this, chunks))
        {
            start = checkpoints[CheckpointSaved];
        }
        else if (checkpoints[CheckpointStart]->Restore(EntryCheckpoint, this, chunks))
        {
            start = checkpoints[CheckpointStart];
        }

        break;
    default:
        break;
    }

    if (start == nullptr)
    {
        return nullptr;
    }

    play.pairing = start->bits.pairing;
    play.character = start->bits.character;
    play.second = start->bits.second;
    return start;
}

s32 TokenArea(u32 token)
{
    // The keywords 0x26F to 0x286 but 0x278 name the areas 0 to 24 but 11 and 14
    static const s8 Areas[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, -1, 9, 10, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    u32 index = token - KeywordFirstArea;
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
    stream->Read(this, sizeof(summary) + sizeof(options), 1);
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
    stream->Write(this, sizeof(summary) + sizeof(options));
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
    progress->counts.lives = summary.lives;
    PlayState play = progress->play;
    play.area = options.area;
    play.story = options.story;
    play.open = options.open;
    play.pairing = summary.pairing;
    play.character = summary.character;
    play.second = summary.second;
    progress->play = play;
    progress->timePlayed = timePlayed;
    progress->startTime = static_cast<s32>(G_GameClockController->clocks[CharacterClock].time);
    if (withData != 0)
    {
        MemoryStream reader;
        MemoryStream::Construct(&reader, data, DataSize, 0, 1);
        Stream* stream = &reader;
        for (LevelProgress& level : progress->levels)
        {
            stream->Read(&level, sizeof(level), 1);
        }

        ReadChunkStates(chunks, stream);
        reader.Destroy(DestroyOnly);
    }

    if (options.vibration != 0)
    {
        pads->flags.vibration = 1;
    }
    else
    {
        pads->DisableVibration();
    }

    SetGroupVolume(effectsVolume, effectsVolume, EffectsGroup);
    SetGroupVolume(effectsVolume, effectsVolume, SecondEffectsGroup);
    SetGroupVolume(effectsVolume, effectsVolume, MovieGroup);
    SetGroupVolume(musicVolume, musicVolume, MusicGroup);
    SetMusicStereo(options.musicStereo);
    g_WidescreenTv = static_cast<u8>(options.widescreen);
    renderer->SetScreenOffset(&screenOffset);
}

void SaveController::Take(GameProgress* progress, ChunkManager* chunks, Checkpoint* checkpoint)
{
    MemoryStream writer;
    MemoryStream::Construct(&writer, data, DataSize, 0, 1);
    ChunkLoader* focus = G_ChunkLoadingManager_->focusLoader;
    StringAssign(&chunk, (focus != nullptr ? &focus->path : &progress->startChunk)->string);
    place = checkpoint != nullptr ? checkpoint->id : NoInstanceId;
    summary.lives = progress->counts.lives;
    u32 crystals = 0;
    for (LevelProgress level : progress->levels)
    {
        crystals += level.crystal;
    }

    summary.crystals = crystals;
    summary.done = progress->Done();
    options.area = progress->play.area;
    options.story = progress->play.story;
    options.open = progress->play.open;
    summary.pairing = progress->play.pairing;
    summary.character = progress->play.character;
    summary.second = progress->play.second;
    s32 played = progress->startTime != 0
                     ? static_cast<s32>(G_GameClockController->clocks[CharacterClock].time) - progress->startTime
                     : 0;
    timePlayed = played + progress->timePlayed;
    Stream* stream = &writer;
    for (LevelProgress& level : progress->levels)
    {
        stream->Write(&level, sizeof(level));
    }

    WriteChunkStates(chunks, stream);
    TakeOptions();
    writer.Destroy(DestroyOnly);
}

bool Checkpoint::Restore(u32, GameProgress* progress, ChunkManager* chunks)
{
    if (bits.set == 0)
    {
        return false;
    }

    ChunkEntry* entry;
    InstanceContext* instance = InstanceAt(this, chunks, &entry);
    InstanceContext* first = progress->Instance(bits.character);
    if (instance != nullptr)
    {
        PersistentFlags* flags = PersistentFlagsOf(instance, entry);
        if (flags != nullptr)
        {
            SetPersistentFlag(flags, id, 1);
        }
    }

    bits.used = 1;
    if (first == nullptr)
    {
        return true;
    }

    void* node = GetGameNode(&first->nodes, ComebackNode);
    InstanceContext* other = progress->Instance(bits.second);
    SetComebackPlacement(node, &character);
    character.Apply(first);
    if (other != nullptr)
    {
        void* otherNode = GetGameNode(&other->nodes, ComebackNode);
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
        return instance != nullptr && bits.used == 0;
    }

    AgentNode* agentNode = AgentNodeOf(instance);
    Agent* agent = agentNode != nullptr ? agentNode->agent : nullptr;
    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&first->nodes, NodeCharacter));
    AgentNode* otherNode = other != nullptr ? static_cast<AgentNode*>(GetGameNode(&other->nodes, NodeCharacter)) : nullptr;
    ChunkLoader* focus = G_ChunkLoadingManager_->focusLoader;
    Release(0, chunks, instance);
    bits.set = 1;
    bits.pairing = pairing;
    character.Take(instance, 0);
    StringAssign(&chunk, focus->path.string);
    id = agent->id;
    bits.character = CharacterOf(firstNode);
    bits.second = CharacterOf(otherNode);
    return true;
}

AgentNode* AgentNodeOf(InstanceContext* instance)
{
    static const u32 Kinds[] = {NodeCrate,     NodePickup,     NodeCreature, NodeCharacter, NodeGenericObject,
                                NodeGrabbable, NodeProjectile, NodePayGate,  NodeGraple};
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
    for (LevelProgress level : levels)
    {
        if ((level.gems & 1 << gem) != 0)
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

    // A third of a percent a gem
    return g_StoryDone[play.story] + gems / 3;
}

void GameProgress::SetTimePlayed(s32 time)
{
    timePlayed = time;
    startTime = static_cast<s32>(G_GameClockController->clocks[CharacterClock].time);
}

u32 GameProgress::AddHealth(s32 amount)
{
    s32 health = static_cast<s32>(counts.health) + amount;
    if (health < 0)
    {
        counts.health = 0;
        return 1;
    }

    s32 most = static_cast<s32>(counts.mostHealth);
    if (health < most)
    {
        counts.health = static_cast<u32>(health);
        return 0;
    }

    counts.health = static_cast<u32>(most);
    return 1;
}

u32 GameProgress::AddToCount(s32 amount)
{
    s32 count = static_cast<s32>(counts.count) + amount;
    if (count < 0)
    {
        counts.count = 0;
        return 1;
    }

    s32 total = static_cast<s32>(counts.countTotal);
    if (count < total)
    {
        counts.count = static_cast<u32>(count);
        return 0;
    }

    counts.count = static_cast<u32>(total);
    return 1;
}

u32 TokenPlayerMode(u32 token)
{
    u32 index = token - KeywordFirstPlayerMode;
    return index < PairingTokens ? index + PairingAlone : 0;
}

void GameProgress::ForgetChunks()
{
    StringAssign(&startChunk, "");
    StringAssign(&unused1C, "");
    StringAssign(&unused28, "");
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
    options.vibration = pads->flags.vibration;
    effectsVolume = GroupVolumeLevel(EffectsGroup);
    musicVolume = GroupVolumeLevel(MusicGroup);
    options.musicStereo = g_MusicStereo;
    options.widescreen = g_WidescreenTv;
    screenOffset.x = renderer->screenOffset.x;
    screenOffset.y = renderer->screenOffset.y;
}

Checkpoint* Checkpoint::Construct(Checkpoint* checkpoint)
{
    checkpoint->chunk.string = nullptr;
    checkpoint->chunk.capacity = 0;
    checkpoint->chunk.length = 0;
    checkpoint->id = NoInstanceId;
    InstancePlacement::Construct(&checkpoint->character, nullptr);
    InstancePlacement::Construct(&checkpoint->second, nullptr);
    RetailLibc::MemorySet(&checkpoint->bits, 0, sizeof(checkpoint->bits));
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

    id = NoInstanceId;
    RetailLibc::MemorySet(&bits, 0, sizeof(bits));
}

bool Checkpoint::Release(u32 forget, ChunkManager* chunks, InstanceContext* keep)
{
    InstanceContext* current = InstanceAt(this, chunks, nullptr);
    bool released = current != nullptr && current != keep;
    if (released)
    {
        auto* node = static_cast<AgentNode*>(GetGameNode(&current->nodes, CheckpointNode));
        if (node != nullptr)
        {
            RunAgentEvent(node->agent, CheckpointReleasedEvent, 0, 0, 0);
        }
    }

    if (forget != 0)
    {
        id = NoInstanceId;
        RetailLibc::MemorySet(&bits, 0, sizeof(bits));
    }

    bits.used = 0;
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
    LevelProgress& progress = *reinterpret_cast<LevelProgress*>(level);
    if (progress.crystal != 0)
    {
        return 0;
    }

    progress.crystal = 1;
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
    u32 gem = token - KeywordFirstGem;
    return gem < GameProgress::Gems ? static_cast<s32>(gem) : -1;
}
