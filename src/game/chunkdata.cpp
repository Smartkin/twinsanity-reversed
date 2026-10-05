#include "game/chunkdata.h"
#include "game/dynamicscenery.h"

#include "game/archive.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/graphicstables.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/lights.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/particles.h"
#include "game/reference.h"
#include "game/scenery.h"
#include "game/shadows.h"
#include "platform/graphics.h"
#include "game/readers.h"
#include "gcc2.h"

// The job queued for a part's release: a reader of nothing runs it
class ChunkPartRelease : public SectionReader
{
public:
    ChunkDataReference* reference;
    u32 part;

    void Destroy(u32 flags) RETAIL(FUN_001ef0c0);
    void Read(u8* unused, u32 size, ReaderStack* readers) RETAIL(FUN_001edf40);
};
CHECK_SIZE(ChunkPartRelease, 0xC);

extern "C"
{
    // The empty name a released chunk gets
    extern const char g_ReleasedChunkPath[] RETAIL(D_00309FF8);
    // A reader's job releasing a part of a chunk's data (a reference to it and the part's number)
    extern const GccVTableEntry g_ChunkPartReleaseVTable[] RETAIL(D_002FBC00);
}

namespace
{
// GCC's pointers to the lists' members, passed along and unused
constexpr s32 ChunkPrevious = GCC2_MEMBER_POINTER(ChunkData, previous);
constexpr s32 ChunkNext = GCC2_MEMBER_POINTER(ChunkData, next);
constexpr s32 LinkPrevious = GCC2_MEMBER_POINTER(ChunkLinkData, previous);
constexpr s32 LinkNext = GCC2_MEMBER_POINTER(ChunkLinkData, next);
// A frame a new chunk was never drawn in
constexpr u32 NeverDrawnFrame = 0x10101015;

void ConstructReverb(ReverbSettings* reverb)
{
    reverb->bits.type = ReverbSettings::NoReverb;
    reverb->bits.unused8 = 0;
    reverb->bits.unused9 = 0;
    reverb->depth = 1.0f;
    reverb->delay = 0.0f;
    reverb->feedback = 0.0f;
}
}

ChunkDataReference* AddChunkDataReference(ChunkData* chunk)
{
    if (chunk->self == nullptr)
    {
        auto* block = static_cast<ChunkDataReference*>(MemoryAllocate(sizeof(ChunkDataReference)));
        chunk->self = block;
        // The new block's bits above the count and the owning bit are kept
        ReferenceBits leftover = block->bits;
        leftover.count = 0;
        leftover.owns = 0;
        block->bits = leftover;
        block->chunk = chunk;
    }

    ChunkDataReference* reference = chunk->self;
    reference->bits.count++;
    return reference;
}

namespace
{
void QueueParts(ChunkData* chunk, const u32* parts, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        QueueChunkPartRelease(chunk, parts[i]);
    }
}
}

extern "C"
{
    ChunkData* ConstructChunkData(ChunkData* chunk, ChunkList* list, const char* path)
    {
        chunk->self = nullptr;
        chunk->drawnFrame = NeverDrawnFrame;
        chunk->particleView = ChunkData::NoParticleView;
        InitIdentityMatrix(&chunk->matrix);
        chunk->list = list;
        chunk->holds = 0;
        StringConstruct(&chunk->name, path);
        chunk->path.string = nullptr;
        chunk->path.length = 0;
        chunk->path.capacity = 0;
        chunk->steppedKinds = ChunkData::EveryKindStepped;
        chunk->scenery = nullptr;
        chunk->previous = nullptr;
        chunk->next = nullptr;
        chunk->links = nullptr;
        chunk->clocks = nullptr;
        chunk->instances = nullptr;
        chunk->instanceCells = nullptr;
        chunk->rigidBodies = nullptr;
        ConstructReverb(&chunk->reverb);
        ConstructReverb(&chunk->boxReverb);
        chunk->soundBoxCount = 0;
        chunk->collision = nullptr;
        chunk->lights = nullptr;
        chunk->unused1C4.string = nullptr;
        chunk->unused1C4.length = 0;
        chunk->unused1C4.capacity = 0;
        chunk->skyId = 0;
        chunk->sky = nullptr;
        chunk->wind = nullptr;
        chunk->shadows = nullptr;
        chunk->particles = ChunkData::NoParticles;
        chunk->dynamicScenery = nullptr;
        chunk->unusedByte = 0;
        chunk->colourFilterPalette = 1;
        chunk->flags.value = 0;
        chunk->flags.state = ChunkHidden;
        StringAssign(&chunk->path, path);
        return chunk;
    }

    void DestroyChunkData(ChunkData* chunk, u32 destroyFlags)
    {
        if (chunk->flags.hasSky != 0)
        {
            ReleaseSky(chunk->sky);
        }

        for (u32 i = 0; i < chunk->soundBoxCount; i++)
        {
            if (chunk->soundBoxes[i] != nullptr)
            {
                MemoryDeallocate2_(chunk->soundBoxes[i]);
            }
        }

        if (chunk->scenery != nullptr)
        {
            chunk->scenery->VirtualDestroy(DestroyAndFree);
        }

        if (chunk->clocks != nullptr)
        {
            TimeClocksDestroy(chunk->clocks, DestroyAndFree);
        }

        if (chunk->instances != nullptr)
        {
            chunk->instances->Destroy(DestroyAndFree);
        }

        if (chunk->instanceCells != nullptr)
        {
            MemoryDeallocate2_(chunk->instanceCells);
        }

        if (chunk->rigidBodies != nullptr)
        {
            DestroyChunkRigidBodies(chunk->rigidBodies, DestroyAndFree);
        }

        if (chunk->collision != nullptr)
        {
            DestroyCollisionData(chunk->collision, DestroyAndFree);
        }

        if (chunk->lights != nullptr)
        {
            DestroyLights(chunk->lights, DestroyAndFree);
        }

        if (chunk->wind != nullptr)
        {
            DestroyChunkWind(chunk->wind, DestroyAndFree);
        }

        if (chunk->shadows != nullptr)
        {
            chunk->shadows->Destroy(DestroyAndFree);
        }

        UnloadParticleSection(static_cast<s8>(chunk->particles));
        if (chunk->dynamicScenery != nullptr)
        {
            DestroyDynamicScenery(chunk->dynamicScenery, DestroyAndFree);
        }

        StringDestroy(&chunk->unused1C4);
        StringDestroy(&chunk->path);
        StringDestroy(&chunk->name);
        if (chunk->self != nullptr)
        {
            chunk->self->chunk = nullptr;
        }

        if ((destroyFlags & 1) != 0)
        {
            MemoryDeallocate2_(chunk);
        }
    }

    u32 ReleaseChunkData(ChunkData* chunk, bool unload, bool start, bool inSteps)
    {
        bool destroyScenery;
        if (start)
        {
            if (inSteps && chunk->holds != 0)
            {
                return 0;
            }

            chunk->flags.state = ChunkReleasing;
            chunk->flags.releasesInSteps = inSteps;
            if (inSteps)
            {
                return 1;
            }

            chunk->holds++;
            DestroyChunkShadows(chunk);
            DestroyChunkParticles(chunk);
            DestroyChunkLights(chunk);
            DestroyChunkDynamicScenery(chunk);
            destroyScenery = true;
        }
        else
        {
            if (!unload)
            {
                return 0;
            }

            if (inSteps)
            {
                constexpr u32 Parts[] = {ChunkData::PartDropHold,         ChunkData::PartCollision, ChunkData::PartClocks,
                                         ChunkData::PartSceneryInstances, ChunkData::PartParticles, ChunkData::PartShadows,
                                         ChunkData::PartAddHold};
                QueueParts(chunk, Parts, sizeof(Parts) / sizeof(Parts[0]));
                return 1;
            }

            chunk->holds++;
            DestroyChunkShadows(chunk);
            DestroyChunkParticles(chunk);
            destroyScenery = false;
        }

        ReleaseChunkScenery(chunk, true, destroyScenery, false);
        DestroyChunkClocks(chunk);
        DestroyChunkCollision(chunk);
        chunk->holds--;
        return 1;
    }

    u32 StepChunkData(ChunkData* chunk, bool paused, GameTimeController* clock)
    {
        switch (chunk->flags.state)
        {
        case ChunkShown:
            if (chunk->clocks != nullptr)
            {
                AdvanceClocks(chunk->clocks, clock);
            }

            if (chunk->instances != nullptr && chunk->steppedKinds != 0)
            {
                chunk->instances->Update(clock, chunk->clocks, chunk->steppedKinds);
            }

            if (!paused && chunk->rigidBodies != nullptr)
            {
                CollideChunkRigidBodies(chunk->rigidBodies);
            }

            if (chunk->flags.hasRoot != 0 && chunk->wind != nullptr)
            {
                UpdateChunkWind(chunk->wind, clock);
            }

            return 1;
        case ChunkReleasing:
            if (chunk->holds != 0)
            {
                return 1;
            }

            if (chunk->flags.releasesInSteps != 0)
            {
                constexpr u32 Parts[] = {ChunkData::PartDropHold, ChunkData::PartCollision, ChunkData::PartClocks,
                                         ChunkData::PartScenery,  ChunkData::PartLights,    ChunkData::PartParticles,
                                         ChunkData::PartShadows,  ChunkData::PartAddHold};
                QueueParts(chunk, Parts, sizeof(Parts) / sizeof(Parts[0]));
            }

            if (chunk->instances != nullptr)
            {
                chunk->instances->Release();
            }

            StringAssign(&chunk->path, g_ReleasedChunkPath);
            chunk->flags.state = ChunkReleased;
            return 1;
        case ChunkReleased:
            if (chunk->collision != nullptr || chunk->lights != nullptr || chunk->shadows != nullptr ||
                chunk->particles != ChunkData::NoParticles || chunk->scenery != nullptr || chunk->clocks != nullptr)
            {
                return 1;
            }

            if (chunk->instances == nullptr)
            {
                return chunk->holds != 0;
            }

            if (chunk->instances->IsEmpty() == 0)
            {
                return 1;
            }

            if (chunk->instances != nullptr)
            {
                chunk->instances->Destroy(DestroyAndFree);
            }

            chunk->instances = nullptr;
            DestroyChunkDataRigidBodies(chunk);
            return 1;
        default:
            return 1;
        }
    }

    void QueueChunkPartRelease(ChunkData* chunk, u32 part)
    {
        GameReadersStorage* storage = g_ReadersStorages[MainReaders];
        auto* job = static_cast<ChunkPartRelease*>(MemoryAllocate(sizeof(ChunkPartRelease)));
        job->vtable = g_ChunkPartReleaseVTable;
        job->reference = chunk != nullptr ? AddChunkDataReference(chunk) : nullptr;
        job->part = part;
        MemoryReader* reader = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), job,
                                                       nullptr, 0);
        AddItemReaderToReaderStorage(storage, reader, QueueFront);
    }

    void ReleaseChunkScenery(ChunkData* chunk, bool releaseInstances, bool destroy, bool inSteps)
    {
        SceneryCell* scenery = chunk->scenery;
        if (scenery == nullptr)
        {
            return;
        }

        const GccVTableEntry* vtable = scenery->vtable;
        if (inSteps)
        {
            // The asm hands the scenery its own arguments: its release (SceneryCell::Release) queues its destruction through the
            // readers when destroy is set, so leaving that out let a stale register destroy a scenery its chunk still had
            CallVirtual<void>(scenery, vtable, SceneryCell::SlotRelease, static_cast<u32>(releaseInstances),
                              static_cast<u32>(destroy), static_cast<u32>(inSteps));
            if (destroy)
            {
                chunk->scenery = nullptr;
            }

            return;
        }

        if (destroy)
        {
            scenery->VirtualDestroy(DestroyAndFree);
            chunk->scenery = nullptr;
            return;
        }

        if (releaseInstances)
        {
            // A filter of every instance: any node kind, no flags it has to have or not
            u32 everyInstance[3] = {~0u, 0, 0};
            CallVirtual<void>(scenery, vtable, SceneryCell::SlotReleaseInstances, everyInstance);
            if (chunk->instances != nullptr)
            {
                chunk->instances->Release();
            }
        }
    }

    ChunkLights* ChunkDataLights(ChunkData* chunk)
    {
        if (chunk->flags.hasLights != 0)
        {
            return chunk->lights;
        }

        ChunkLights* lights = ConstructChunkLights(static_cast<ChunkLights*>(MemoryAllocate(sizeof(ChunkLights))), chunk);
        chunk->lights = lights;
        chunk->flags.hasLights = 1;
        return chunk->lights;
    }

    ChunkDynamicScenery* ChunkDataDynamicScenery(ChunkData* chunk)
    {
        if (chunk->dynamicScenery == nullptr)
        {
            auto* scenery = static_cast<ChunkDynamicScenery*>(MemoryAllocate(sizeof(ChunkDynamicScenery)));
            chunk->dynamicScenery = ConstructDynamicScenery(scenery, chunk);
        }

        return chunk->dynamicScenery;
    }

    ChunkInstances* ChunkDataInstances(ChunkData* chunk)
    {
        if (chunk->instances == nullptr)
        {
            chunk->instances = ChunkInstances::Construct(static_cast<ChunkInstances*>(MemoryAllocate(sizeof(ChunkInstances))));
        }

        return chunk->instances;
    }

    ChunkShadows* ChunkShadowsOf(ChunkData* chunk, s32 count)
    {
        if (chunk->shadows == nullptr)
        {
            chunk->shadows = ChunkShadows::Construct(static_cast<ChunkShadows*>(MemoryAllocate(sizeof(ChunkShadows))), count);
        }

        return chunk->shadows;
    }

    void* ChunkDataCollision(ChunkData* chunk)
    {
        if (chunk->collision == nullptr)
        {
            chunk->collision = ConstructCollisionData(static_cast<CollisionData*>(MemoryAllocate(sizeof(CollisionData))));
        }

        return chunk->collision;
    }

    void DestroyChunkClocks(ChunkData* chunk)
    {
        if (chunk->clocks != nullptr)
        {
            TimeClocksDestroy(chunk->clocks, DestroyAndFree);
        }

        chunk->clocks = nullptr;
    }

    void DestroyChunkLights(ChunkData* chunk)
    {
        if (chunk->flags.hasLights == 0)
        {
            return;
        }

        if (chunk->lights != nullptr)
        {
            DestroyLights(chunk->lights, DestroyAndFree);
        }

        chunk->lights = nullptr;
        chunk->flags.hasLights = 0;
    }

    void DestroyChunkDynamicScenery(ChunkData* chunk)
    {
        if (chunk->dynamicScenery != nullptr)
        {
            DestroyDynamicScenery(chunk->dynamicScenery, DestroyAndFree);
        }

        chunk->dynamicScenery = nullptr;
    }

    void DestroyChunkShadows(ChunkData* chunk)
    {
        if (chunk->shadows != nullptr)
        {
            chunk->shadows->Destroy(DestroyAndFree);
        }

        chunk->shadows = nullptr;
    }

    void DestroyChunkParticles(ChunkData* chunk)
    {
        if (chunk->particles == ChunkData::NoParticles)
        {
            return;
        }

        UnloadParticleSection(static_cast<s8>(chunk->particles));
        chunk->particles = ChunkData::NoParticles;
    }

    void DestroyChunkCollision(ChunkData* chunk)
    {
        if (chunk->collision != nullptr)
        {
            DestroyCollisionData(chunk->collision, DestroyAndFree);
        }

        chunk->collision = nullptr;
    }

    void DestroyChunkDataRigidBodies(ChunkData* chunk)
    {
        if (chunk->rigidBodies != nullptr)
        {
            DestroyChunkRigidBodies(chunk->rigidBodies, DestroyAndFree);
        }

        chunk->rigidBodies = nullptr;
    }
}

extern "C"
{
    extern ChunkList g_ChunkList RETAIL(D_0030A8F8);
    extern u32 g_ChunkListMade RETAIL(D_0030A900);

    // The C library's atexit, and what it's given: the list's destructor
    s32 RegisterAtExit(void (*function)()) RETAIL(FUN_002c79e8);
    void DestroyChunkListAtExit() RETAIL(FUN_001f4108);

    void DestroyChunkListAtExit()
    {
        DestroyChunkList(&g_ChunkList, DestroyOnly);
    }

    ChunkList* GetChunkList()
    {
        if (g_ChunkListMade == 0)
        {
            InitChunkList(&g_ChunkList);
            g_ChunkListMade = 1;
            RegisterAtExit(DestroyChunkListAtExit);
        }

        return &g_ChunkList;
    }

    ChunkList* InitChunkList(ChunkList* list)
    {
        list->first = nullptr;
        list->current = nullptr;
        return list;
    }

    void ChunkListAdd(ChunkList* list, ChunkData* chunk)
    {
        ChunkListPushFront(chunk, &list->first, ChunkPrevious, ChunkNext);
    }

    void ChunkListRemove(ChunkList* list, ChunkData* removed)
    {
        ChunkListTakeOut(removed, &list->first, ChunkPrevious, ChunkNext);
        for (ChunkData* chunk = list->first; chunk != nullptr; chunk = chunk->next)
        {
            ChunkDataForgetLinksTo(chunk, removed);
        }
    }

    ChunkData* FindChunkData(ChunkList* list, String* path)
    {
        for (ChunkData* chunk = list->first; chunk != nullptr; chunk = chunk->next)
        {
            if (!StringNotEqual(&chunk->path, path->string))
            {
                return chunk;
            }
        }

        return nullptr;
    }

    void ChunkDataLink(ChunkData* chunk, ChunkLinkData* link)
    {
        if (link->flags.inList != 0)
        {
            return;
        }

        link->linkedData = chunk;
        link->flags.inList = 1;
        LinkListPushFrontData(link, &chunk->links, LinkPrevious, LinkNext);
    }

    void ChunkDataUnlink(ChunkData* chunk, ChunkLinkData* link)
    {
        if (link->flags.inList == 0)
        {
            return;
        }

        link->linkedData = nullptr;
        link->flags.inList = 0;
        LinkListRemoveData(link, &chunk->links, LinkPrevious, LinkNext);
    }

    void ChunkDataForgetLinksTo(ChunkData* chunk, ChunkData* gone)
    {
        ChunkLinkData* link = chunk->links;
        while (link != nullptr)
        {
            ChunkLinkData* next = link->next;
            if (link->linkedData == gone && link->flags.inList != 0)
            {
                link->linkedData = nullptr;
                link->flags.inList = 0;
                LinkListRemoveData(link, &chunk->links, LinkPrevious, LinkNext);
            }

            link = next;
        }
    }

    void ChunkListPushFront(ChunkData* chunk, ChunkData** head, s32, s32)
    {
        if (*head == nullptr)
        {
            *head = chunk;
            chunk->previous = nullptr;
            chunk->next = nullptr;
            return;
        }

        (*head)->previous = chunk;
        chunk->next = *head;
        *head = chunk;
    }

    void ChunkListTakeOut(ChunkData* chunk, ChunkData** head, s32, s32)
    {
        if (chunk->previous == nullptr)
        {
            if (chunk->next == nullptr)
            {
                *head = nullptr;
            }
            else
            {
                chunk->next->previous = nullptr;
                *head = chunk->next;
            }
        }
        else if (chunk->next == nullptr)
        {
            chunk->previous->next = nullptr;
        }
        else
        {
            chunk->previous->next = chunk->next;
            chunk->next->previous = chunk->previous;
        }

        chunk->next = nullptr;
        chunk->previous = nullptr;
    }

    void LinkListPushFrontData(ChunkLinkData* link, ChunkLinkData** head, s32, s32)
    {
        if (*head == nullptr)
        {
            *head = link;
            link->previous = nullptr;
            link->next = nullptr;
            return;
        }

        (*head)->previous = link;
        link->next = *head;
        *head = link;
    }

    void LinkListRemoveData(ChunkLinkData* link, ChunkLinkData** head, s32, s32)
    {
        if (link->previous == nullptr)
        {
            if (link->next == nullptr)
            {
                *head = nullptr;
            }
            else
            {
                link->next->previous = nullptr;
                *head = link->next;
            }
        }
        else if (link->next == nullptr)
        {
            link->previous->next = nullptr;
        }
        else
        {
            link->previous->next = link->next;
            link->next->previous = link->previous;
        }

        link->next = nullptr;
        link->previous = nullptr;
    }
}

extern "C"
{
    void ReleaseChunkDataReference(ChunkDataReference** handle)
    {
        ChunkDataReference* reference = *handle;
        ReferenceBits bits = reference->bits;
        bits.count--;
        reference->bits = bits;
        if (bits.count == 0 && bits.owns)
        {
            if (reference->chunk != nullptr)
            {
                DestroyChunkData(reference->chunk, DestroyAndFree);
            }

            reference->chunk = nullptr;
        }

        if (reference->bits.count != 0)
        {
            return;
        }

        reference = *handle;
        ChunkData* chunk = reference->chunk;
        if (chunk == nullptr)
        {
            if (reference->bits.owns)
            {
                reference->chunk = nullptr;
            }

            MemoryDeallocate2_(reference);
            *handle = nullptr;
            return;
        }

        // The data's own block goes (with the data when it owns it), not this one
        ChunkDataReference* own = chunk->self;
        if (own != nullptr)
        {
            if (own->bits.owns)
            {
                if (own->chunk != nullptr)
                {
                    DestroyChunkData(own->chunk, DestroyAndFree);
                }

                own->chunk = nullptr;
            }

            MemoryDeallocate2_(own);
            chunk->self = nullptr;
        }

        *handle = nullptr;
    }

    ChunkDataReference** AssignChunkData(ChunkDataReference** reference, ChunkData* chunk)
    {
        ChunkData* current = *reference != nullptr ? (*reference)->chunk : nullptr;
        if (current == chunk)
        {
            return reference;
        }

        if (*reference != nullptr)
        {
            ReleaseChunkDataReference(reference);
        }

        *reference = chunk != nullptr ? AddChunkDataReference(chunk) : nullptr;
        return reference;
    }

    void DestroyChunkList(ChunkList* list, u32 destroyFlags)
    {
        ChunkData* chunk = list->first;
        while (chunk != nullptr)
        {
            ChunkData* next = chunk->next;
            ChunkListTakeOut(chunk, &list->first, ChunkPrevious, ChunkNext);
            chunk = next;
        }

        if (list->current != nullptr)
        {
            ReleaseChunkDataReference(&list->current);
        }

        if ((destroyFlags & 1) != 0)
        {
            MemoryDeallocate2_(list);
        }
    }

    void UpdateChunks(ChunkList* list, bool paused, GameTimeController* clock)
    {
        // The asm the chunks' frame still runs on takes the VU0's standard programs
        Platform::Graphics::UseHelperPrograms(Platform::Graphics::StandardPrograms, false);
        ChunkData* current = list->current != nullptr ? list->current->chunk : nullptr;
        if (current != nullptr && StepChunkData(current, paused, clock) == 0)
        {
            ChunkListRemove(list, current);
            DestroyChunkData(current, DestroyAndFree);
            AssignChunkData(&list->current, nullptr);
        }

        ChunkData* chunk = list->first;
        while (chunk != nullptr)
        {
            ChunkData* next = chunk->next;
            ChunkData* steppedFirst = list->current != nullptr ? list->current->chunk : nullptr;
            if (chunk != steppedFirst && StepChunkData(chunk, paused, clock) == 0)
            {
                ChunkListRemove(list, chunk);
                if (chunk != nullptr)
                {
                    DestroyChunkData(chunk, DestroyAndFree);
                }
            }

            chunk = next;
        }
    }
}

void ChunkPartRelease::Destroy(u32 flags)
{
    if (reference != nullptr)
    {
        ReleaseChunkDataReference(&reference);
    }

    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ChunkPartRelease::Read(u8*, u32, ReaderStack*)
{
    ChunkData* chunk = reference != nullptr ? reference->chunk : nullptr;
    if (chunk == nullptr)
    {
        return;
    }

    switch (part)
    {
    case ChunkData::PartAddHold:
        chunk->holds++;
        break;
    case ChunkData::PartDropHold:
        chunk->holds--;
        break;
    case ChunkData::PartCollision:
        DestroyChunkCollision(chunk);
        break;
    case ChunkData::PartLights:
        DestroyChunkLights(chunk);
        break;
    case ChunkData::PartShadows:
        DestroyChunkShadows(chunk);
        break;
    case ChunkData::PartParticles:
        DestroyChunkParticles(chunk);
        break;
    case ChunkData::PartSceneryInstances:
        ReleaseChunkScenery(chunk, true, false, true);
        break;
    case ChunkData::PartScenery:
        ReleaseChunkScenery(chunk, true, true, true);
        break;
    case ChunkData::PartClocks:
        DestroyChunkClocks(chunk);
        break;
    default:
        break;
    }
}

u32 ChunkData::InSoundBox(const Vector4* point)
{
    if (soundBoxCount == 0)
    {
        return 0;
    }

    u32 index = 0;
    do
    {
        if (soundBoxes[index]->Contains(point) != 0)
        {
            return 1;
        }

        index++;
    } while (index < soundBoxCount);

    return 0;
}

u32 ChunkData::AddSoundBox(ChunkData* chunk, SoundBox* box)
{
    if (chunk->soundBoxCount == MostSoundBoxes)
    {
        return 0;
    }

    chunk->soundBoxes[chunk->soundBoxCount] = box;
    chunk->soundBoxCount++;
    return 1;
}

u32 ChunkData::AddInstance(ChunkData* chunk, InstanceContext* instance)
{
    if (instance->chunk != nullptr)
    {
        return 0;
    }

    instance->chunk = chunk;
    if (!instance->flags.asleep)
    {
        if (chunk->instanceCells == nullptr)
        {
            auto* cells = static_cast<InstanceContext**>(MemoryAllocate(InstanceCells * sizeof(InstanceContext*)));
            for (s32 index = InstanceCells - 1; index >= 0; index--)
            {
                cells[index] = nullptr;
            }

            chunk->instanceCells = cells;
        }

        SortIntoCells(&instance->collision, chunk->scenery, chunk->instanceCells, instance);
    }

    return chunk->instances->AddInstance(instance) & 0xFF;
}
