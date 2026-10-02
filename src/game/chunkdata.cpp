#include "game/chunkdata.h"

#include "game/archive.h"
#include "game/collision.h"
#include "game/graphicstables.h"
#include "game/instances.h"
#include "game/lights.h"
#include "game/memory.h"
#include "platform/graphics.h"
#include "game/readers.h"
#include "gcc2.h"

// The job queued for a part's release: a reader of nothing runs it
class ChunkPartRelease : public SectionReader
{
public:
    ChunkDataReference* data;
    u32 part;

    void Destroy(u32 flags) RETAIL(FUN_001ef0c0);
    void Read(u8* unused, u32 size, ReaderStack* readers) RETAIL(FUN_001edf40);
};
CHECK_SIZE(ChunkPartRelease, 0xC);

extern "C"
{
    // An instance's collision sorted into a chunk's cells over its scenery (still asm)
    void SortIntoCells(ObjectCollision* collision, u8* scenery, u32* cells, InstanceContext* instance) RETAIL(FUN_001eb7b0);
    // The empty name a released chunk gets
    extern const char g_ReleasedChunkPath[] RETAIL(D_00309FF8);
    // A reader's job releasing a part of a chunk's data (a reference to it and the part's number)
    extern const GccVTableEntry g_ChunkPartReleaseVTable[] RETAIL(D_002FBC00);

    // The parts' constructors and destructors
    void* ConstructDynamicScenery(void* memory, ChunkData* data) RETAIL(FUN_00201aa0);
    void DestroyDynamicScenery(void* scenery, u32 flags) RETAIL(FUN_00201ad8);
    void* ConstructUnknown1DC(void* memory, s32 count) RETAIL(FUN_001cc310);
    void DestroyUnknown1DC(void* object, u32 flags) RETAIL(FUN_001cc3f8);
    void DestroyClocks(void* clocks, u32 flags) RETAIL(FUN_00191ea8);
    void DestroyUnknown164(void* object, u32 flags) RETAIL(FUN_00252be0);
    void DestroyUnknown1D8(void* object, u32 flags) RETAIL(FUN_001a2430);
    void UnloadParticleSection(s8 section);

    // A shown chunk's frame
    void AdvanceClocks(void* clocks, GameTimeController* clock);
    void UpdateUnknown164(void* object) RETAIL(FUN_002433e8);
    void UpdateUnknown1D8(void* object, GameTimeController* clock) RETAIL(FUN_0019f5e0);
}

namespace
{
constexpr u32 CountMask = 0xFFFFFF;

void ConstructSetting(ChunkDataSetting* setting)
{
    *reinterpret_cast<u8*>(&setting->bits) = 0xFF;
    setting->bits &= ~0x100u;
    setting->bits &= ~0x200u;
    setting->scale = 1.0f;
    setting->unknown04 = 0;
    setting->unknown08 = 0;
}

ChunkDataReference* AddChunkDataReference(ChunkData* data)
{
    if (data->self == nullptr)
    {
        auto* block = static_cast<ChunkDataReference*>(MemoryAllocate(sizeof(ChunkDataReference)));
        data->self = block;
        // The new block's top bits are kept
        block->value &= 0xFE000000;
        block->data = data;
    }

    ChunkDataReference* reference = data->self;
    reference->value = (reference->value & ~CountMask) | (((reference->value & CountMask) + 1) & CountMask);
    return reference;
}


void QueueParts(ChunkData* data, const u32* parts, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        QueueChunkPartRelease(data, parts[i]);
    }
}

void SetState(ChunkData* data, u32 state)
{
    data->bits = (data->bits & ~ChunkData::StateMask) | state;
}
}

extern "C"
{
    ChunkData* ConstructChunkData(ChunkData* data, ChunkList* list, const char* path)
    {
        data->self = nullptr;
        // A frame it was never drawn in
        data->drawnFrame = 0x10101015;
        data->particleView = -1;
        InitIdentityMatrix(&data->matrix);
        data->list = list;
        data->rm2Loads = 0;
        StringConstruct(&data->name, path);
        data->path.string = nullptr;
        data->path.length = 0;
        data->path.capacity = 0;
        data->detail = -1;
        data->scenery = nullptr;
        data->previous = nullptr;
        data->next = nullptr;
        data->links = nullptr;
        data->clocks = nullptr;
        data->instances = nullptr;
        data->instanceCells = nullptr;
        data->unknown164 = nullptr;
        ConstructSetting(&data->setting168);
        ConstructSetting(&data->setting180);
        data->soundBoxCount = 0;
        data->collision = nullptr;
        data->lights = nullptr;
        data->unknown1C4.string = nullptr;
        data->unknown1C4.length = 0;
        data->unknown1C4.capacity = 0;
        data->unknown1D0 = 0;
        data->sky = nullptr;
        data->unknown1D8 = nullptr;
        data->unknown1DC = nullptr;
        data->particles = -1;
        data->dynamicScenery = nullptr;
        data->unknown1EC = 0;
        data->colourFilterPalette = 1;
        data->bits = 0;
        data->bits &= ~ChunkData::StateMask;
        StringAssign(&data->path, path);
        return data;
    }

    void DestroyChunkData(ChunkData* data, u32 flags)
    {
        if ((data->bits & ChunkData::UpdateDetail) != 0)
        {
            ReleaseSky(data->sky);
        }

        for (u32 i = 0; i < data->soundBoxCount; i++)
        {
            if (data->soundBoxes[i] != nullptr)
            {
                MemoryDeallocate2_(data->soundBoxes[i]);
            }
        }

        if (data->scenery != nullptr)
        {
            CallVirtual<void>(data->scenery, *reinterpret_cast<const GccVTableEntry**>(data->scenery + 0x44), 1, 3u);
        }

        if (data->clocks != nullptr)
        {
            DestroyClocks(data->clocks, 3);
        }

        if (data->instances != nullptr)
        {
            data->instances->Destroy(DestroyAndFree);
        }

        if (data->instanceCells != nullptr)
        {
            MemoryDeallocate2_(data->instanceCells);
        }

        if (data->unknown164 != nullptr)
        {
            DestroyUnknown164(data->unknown164, 3);
        }

        if (data->collision != nullptr)
        {
            DestroyCollisionData(static_cast<CollisionData*>(data->collision), 3);
        }

        if (data->lights != nullptr)
        {
            DestroyLights(data->lights, 3);
        }

        if (data->unknown1D8 != nullptr)
        {
            DestroyUnknown1D8(data->unknown1D8, 3);
        }

        if (data->unknown1DC != nullptr)
        {
            DestroyUnknown1DC(data->unknown1DC, 3);
        }

        UnloadParticleSection(static_cast<s8>(data->particles));
        if (data->dynamicScenery != nullptr)
        {
            DestroyDynamicScenery(data->dynamicScenery, 3);
        }

        StringDestroy(&data->unknown1C4);
        StringDestroy(&data->path);
        StringDestroy(&data->name);
        if (data->self != nullptr)
        {
            data->self->data = nullptr;
        }

        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(data);
        }
    }

    u32 ReleaseChunkData(ChunkData* data, bool unload, bool start, bool inSteps)
    {
        bool destroyScenery;
        if (start)
        {
            if (inSteps && data->rm2Loads != 0)
            {
                return 0;
            }

            SetState(data, ChunkData::Releasing);
            data->bits = (data->bits & ~ChunkData::ReleaseInSteps) | static_cast<u32>(inSteps) << 24;
            if (inSteps)
            {
                return 1;
            }

            data->rm2Loads++;
            DestroyChunkUnknown1DC(data);
            DestroyChunkParticles(data);
            DestroyChunkLights(data);
            DestroyChunkDynamicScenery(data);
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
                constexpr u32 Parts[] = {1, 3, 10, 8, 6, 5, 0};
                QueueParts(data, Parts, sizeof(Parts) / sizeof(Parts[0]));
                return 1;
            }

            data->rm2Loads++;
            DestroyChunkUnknown1DC(data);
            DestroyChunkParticles(data);
            destroyScenery = false;
        }

        ReleaseChunkScenery(data, true, destroyScenery, false);
        DestroyChunkClocks(data);
        DestroyChunkCollision(data);
        data->rm2Loads--;
        return 1;
    }

    u32 ChunkDataReleasing(ChunkData* data, bool paused, GameTimeController* clock)
    {
        switch (data->bits >> 18 & 0xF)
        {
        case ChunkData::Shown >> 18:
            if (data->clocks != nullptr)
            {
                AdvanceClocks(data->clocks, clock);
            }

            if (data->instances != nullptr && data->detail != 0)
            {
                data->instances->Update(clock, data->clocks, data->detail);
            }

            if (!paused && data->unknown164 != nullptr)
            {
                UpdateUnknown164(data->unknown164);
            }

            if ((data->bits & ChunkData::UpdateUnknown1D8) != 0 && data->unknown1D8 != nullptr)
            {
                UpdateUnknown1D8(data->unknown1D8, clock);
            }

            return 1;
        case ChunkData::Releasing >> 18:
            if (data->rm2Loads != 0)
            {
                return 1;
            }

            if ((data->bits & ChunkData::ReleaseInSteps) != 0)
            {
                constexpr u32 Parts[] = {1, 3, 10, 9, 4, 6, 5, 0};
                QueueParts(data, Parts, sizeof(Parts) / sizeof(Parts[0]));
            }

            if (data->instances != nullptr)
            {
                data->instances->Release();
            }

            StringAssign(&data->path, g_ReleasedChunkPath);
            SetState(data, ChunkData::Released);
            return 1;
        case ChunkData::Released >> 18:
            if (data->collision != nullptr || data->lights != nullptr || data->unknown1DC != nullptr || data->particles != -1 ||
                data->scenery != nullptr || data->clocks != nullptr)
            {
                return 1;
            }

            if (data->instances == nullptr)
            {
                return data->rm2Loads != 0;
            }

            if (data->instances->IsEmpty() == 0)
            {
                return 1;
            }

            if (data->instances != nullptr)
            {
                data->instances->Destroy(DestroyAndFree);
            }

            data->instances = nullptr;
            DestroyChunkUnknown164(data);
            return 1;
        default:
            return 1;
        }
    }

    void QueueChunkPartRelease(ChunkData* data, u32 part)
    {
        GameReadersStorage* storage = g_ReadersStorages[0];
        auto* job = static_cast<ChunkPartRelease*>(MemoryAllocate(sizeof(ChunkPartRelease)));
        job->vtable = g_ChunkPartReleaseVTable;
        job->data = data != nullptr ? AddChunkDataReference(data) : nullptr;
        job->part = part;
        MemoryReader* reader = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), job,
                                                       nullptr, 0);
        AddItemReaderToReaderStorage(storage, reader, 1);
    }

    void ReleaseChunkScenery(ChunkData* data, bool unknown, bool destroy, bool tell)
    {
        u8* scenery = data->scenery;
        if (scenery == nullptr)
        {
            return;
        }

        const GccVTableEntry* vtable = *reinterpret_cast<const GccVTableEntry**>(scenery + 0x44);
        if (tell)
        {
            // The asm hands the scenery its own arguments: its release (FUN_001efb40) queues its destruction through the readers
            // when destroy is set, so leaving that out let a stale register destroy a scenery its chunk still had
            CallVirtual<void>(scenery, vtable, 2, static_cast<u32>(unknown), static_cast<u32>(destroy), static_cast<u32>(tell));
            if (destroy)
            {
                data->scenery = nullptr;
            }

            return;
        }

        if (destroy)
        {
            CallVirtual<void>(scenery, vtable, 1, 3u);
            data->scenery = nullptr;
            return;
        }

        if (unknown)
        {
            s32 range[3] = {-1, 0, 0};
            CallVirtual<void>(scenery, vtable, 23, range);
            if (data->instances != nullptr)
            {
                data->instances->Release();
            }
        }
    }

    ChunkLights* ChunkDataLights(ChunkData* data)
    {
        if ((data->bits & ChunkData::HasLights) != 0)
        {
            return data->lights;
        }

        ChunkLights* lights = ConstructChunkLights(static_cast<ChunkLights*>(MemoryAllocate(sizeof(ChunkLights))), data);
        data->lights = lights;
        data->bits |= ChunkData::HasLights;
        return data->lights;
    }

    void* ChunkDataDynamicScenery(ChunkData* data)
    {
        if (data->dynamicScenery == nullptr)
        {
            data->dynamicScenery = ConstructDynamicScenery(MemoryAllocate(0x14), data);
        }

        return data->dynamicScenery;
    }

    ChunkInstances* ChunkDataContext(ChunkData* data)
    {
        if (data->instances == nullptr)
        {
            data->instances = ChunkInstances::Construct(static_cast<ChunkInstances*>(MemoryAllocate(sizeof(ChunkInstances))));
        }

        return data->instances;
    }

    void* ChunkDataUnknown(ChunkData* data, s32 count)
    {
        if (data->unknown1DC == nullptr)
        {
            data->unknown1DC = ConstructUnknown1DC(MemoryAllocate(0x1C), count);
        }

        return data->unknown1DC;
    }

    void* ChunkDataCollision(ChunkData* data)
    {
        if (data->collision == nullptr)
        {
            data->collision = ConstructCollisionData(static_cast<CollisionData*>(MemoryAllocate(sizeof(CollisionData))));
        }

        return data->collision;
    }

    void DestroyChunkClocks(ChunkData* data)
    {
        if (data->clocks != nullptr)
        {
            DestroyClocks(data->clocks, 3);
        }

        data->clocks = nullptr;
    }

    void DestroyChunkLights(ChunkData* data)
    {
        if ((data->bits & ChunkData::HasLights) == 0)
        {
            return;
        }

        if (data->lights != nullptr)
        {
            DestroyLights(data->lights, 3);
        }

        data->lights = nullptr;
        data->bits &= ~ChunkData::HasLights;
    }

    void DestroyChunkDynamicScenery(ChunkData* data)
    {
        if (data->dynamicScenery != nullptr)
        {
            DestroyDynamicScenery(data->dynamicScenery, 3);
        }

        data->dynamicScenery = nullptr;
    }

    void DestroyChunkUnknown1DC(ChunkData* data)
    {
        if (data->unknown1DC != nullptr)
        {
            DestroyUnknown1DC(data->unknown1DC, 3);
        }

        data->unknown1DC = nullptr;
    }

    void DestroyChunkParticles(ChunkData* data)
    {
        if (data->particles == -1)
        {
            return;
        }

        UnloadParticleSection(static_cast<s8>(data->particles));
        data->particles = -1;
    }

    void DestroyChunkCollision(ChunkData* data)
    {
        if (data->collision != nullptr)
        {
            DestroyCollisionData(static_cast<CollisionData*>(data->collision), 3);
        }

        data->collision = nullptr;
    }

    void DestroyChunkUnknown164(ChunkData* data)
    {
        if (data->unknown164 != nullptr)
        {
            DestroyUnknown164(data->unknown164, 3);
        }

        data->unknown164 = nullptr;
    }
}

extern "C"
{
    extern ChunkList g_ChunkList RETAIL(D_0030A8F8);
    extern u32 g_ChunkListMade RETAIL(D_0030A900);

    // The C library's atexit, and what it's given: the list's destructor
    s32 RegisterAtExit(void (*function)()) RETAIL(FUN_002c79e8);
    void DestroyChunkListAtExit() RETAIL(FUN_001f4108);

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

    void ChunkListAdd(ChunkList* list, ChunkData* data)
    {
        ChunkListPushFront(data, &list->first, 0x14D, 0x149);
    }

    void ChunkListRemove(ChunkList* list, ChunkData* data)
    {
        ChunkListTakeOut(data, &list->first, 0x14D, 0x149);
        for (ChunkData* chunk = list->first; chunk != nullptr; chunk = chunk->next)
        {
            ChunkDataForgetLinksTo(chunk, data);
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

    void ChunkDataLink(ChunkData* data, ChunkLinkData* link)
    {
        if ((link->flags & ChunkLinkData::InList) != 0)
        {
            return;
        }

        link->linkedData = data;
        link->flags |= ChunkLinkData::InList;
        LinkListPushFrontData(link, &data->links, 0xD, 0x9);
    }

    void ChunkDataUnlink(ChunkData* data, ChunkLinkData* link)
    {
        if ((link->flags & ChunkLinkData::InList) == 0)
        {
            return;
        }

        link->linkedData = nullptr;
        link->flags &= ~ChunkLinkData::InList;
        LinkListRemoveData(link, &data->links, 0xD, 0x9);
    }

    void ChunkDataForgetLinksTo(ChunkData* data, ChunkData* gone)
    {
        ChunkLinkData* link = data->links;
        while (link != nullptr)
        {
            ChunkLinkData* next = link->next;
            if (link->linkedData == gone && (link->flags & ChunkLinkData::InList) != 0)
            {
                link->linkedData = nullptr;
                link->flags &= ~ChunkLinkData::InList;
                LinkListRemoveData(link, &data->links, 0xD, 0x9);
            }

            link = next;
        }
    }

    void ChunkListPushFront(ChunkData* data, ChunkData** head, s32, s32)
    {
        if (*head == nullptr)
        {
            *head = data;
            data->previous = nullptr;
            data->next = nullptr;
            return;
        }

        (*head)->previous = data;
        data->next = *head;
        *head = data;
    }

    void ChunkListTakeOut(ChunkData* data, ChunkData** head, s32, s32)
    {
        if (data->previous == nullptr)
        {
            if (data->next == nullptr)
            {
                *head = nullptr;
            }
            else
            {
                data->next->previous = nullptr;
                *head = data->next;
            }
        }
        else if (data->next == nullptr)
        {
            data->previous->next = nullptr;
        }
        else
        {
            data->previous->next = data->next;
            data->next->previous = data->previous;
        }

        data->next = nullptr;
        data->previous = nullptr;
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
        u32 value = reference->value;
        u32 count = ((value & CountMask) - 1) & CountMask;
        reference->value = (value & ~CountMask) | count;
        if (count == 0 && (value & 0x1000000) != 0)
        {
            if (reference->data != nullptr)
            {
                DestroyChunkData(reference->data, 3);
            }

            reference->data = nullptr;
        }

        if ((reference->value & CountMask) != 0)
        {
            return;
        }

        reference = *handle;
        ChunkData* data = reference->data;
        if (data == nullptr)
        {
            if ((reference->value & 0x1000000) != 0)
            {
                reference->data = nullptr;
            }

            MemoryDeallocate2_(reference);
            *handle = nullptr;
            return;
        }

        // The data's own block goes (with the data when it owns it), not this one
        ChunkDataReference* own = data->self;
        if (own != nullptr)
        {
            if ((own->value & 0x1000000) != 0)
            {
                if (own->data != nullptr)
                {
                    DestroyChunkData(own->data, 3);
                }

                own->data = nullptr;
            }

            MemoryDeallocate2_(own);
            data->self = nullptr;
        }

        *handle = nullptr;
    }

    ChunkDataReference** AssignChunkData(ChunkDataReference** reference, ChunkData* data)
    {
        ChunkData* current = *reference != nullptr ? (*reference)->data : nullptr;
        if (current == data)
        {
            return reference;
        }

        if (*reference != nullptr)
        {
            ReleaseChunkDataReference(reference);
        }

        *reference = data != nullptr ? AddChunkDataReference(data) : nullptr;
        return reference;
    }

    void DestroyChunkList(ChunkList* list, u32 flags)
    {
        ChunkData* chunk = list->first;
        while (chunk != nullptr)
        {
            ChunkData* next = chunk->next;
            ChunkListTakeOut(chunk, &list->first, 0x14D, 0x149);
            chunk = next;
        }

        if (list->current != nullptr)
        {
            ReleaseChunkDataReference(&list->current);
        }

        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(list);
        }
    }

    void UpdateChunks(ChunkList* list, bool paused, GameTimeController* clock)
    {
        // The asm the chunks' frame still runs on takes the VU0's standard programs
        Platform::Graphics::UseHelperPrograms(1, false);
        ChunkData* current = list->current != nullptr ? list->current->data : nullptr;
        if (current != nullptr && ChunkDataReleasing(current, paused, clock) == 0)
        {
            ChunkListRemove(list, current);
            DestroyChunkData(current, 3);
            AssignChunkData(&list->current, nullptr);
        }

        ChunkData* chunk = list->first;
        while (chunk != nullptr)
        {
            ChunkData* next = chunk->next;
            ChunkData* first = list->current != nullptr ? list->current->data : nullptr;
            if (chunk != first && ChunkDataReleasing(chunk, paused, clock) == 0)
            {
                ChunkListRemove(list, chunk);
                if (chunk != nullptr)
                {
                    DestroyChunkData(chunk, 3);
                }
            }

            chunk = next;
        }
    }
}

void ChunkPartRelease::Destroy(u32 flags)
{
    if (data != nullptr)
    {
        ReleaseChunkDataReference(&data);
    }

    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ChunkPartRelease::Read(u8*, u32, ReaderStack*)
{
    ChunkData* chunk = data != nullptr ? data->data : nullptr;
    if (chunk == nullptr)
    {
        return;
    }

    switch (part)
    {
    case 0:
        // Held until the parts queued before this are let go of
        chunk->rm2Loads++;
        break;
    case 1:
        chunk->rm2Loads--;
        break;
    case 3:
        DestroyChunkCollision(chunk);
        break;
    case 4:
        DestroyChunkLights(chunk);
        break;
    case 5:
        DestroyChunkUnknown1DC(chunk);
        break;
    case 6:
        DestroyChunkParticles(chunk);
        break;
    case 8:
        ReleaseChunkScenery(chunk, true, false, true);
        break;
    case 9:
        ReleaseChunkScenery(chunk, true, true, true);
        break;
    case 10:
        DestroyChunkClocks(chunk);
        break;
    default:
        break;
    }
}

u32 ChunkData::AddSoundBox(ChunkData* chunk, void* box)
{
    constexpr u32 MaxSoundBoxes = 7;
    if (chunk->soundBoxCount == MaxSoundBoxes)
    {
        return 0;
    }

    chunk->soundBoxes[chunk->soundBoxCount] = box;
    chunk->soundBoxCount++;
    return 1;
}

u32 ChunkData::AddInstance(ChunkData* chunk, InstanceContext* instance)
{
    constexpr u32 CellWords = 0x301;
    if (instance->chunk != nullptr)
    {
        return 0;
    }

    instance->chunk = chunk;
    if ((instance->flags & ReferencedObject::FlagAsleep) == 0)
    {
        if (chunk->instanceCells == nullptr)
        {
            auto* cells = static_cast<u32*>(MemoryAllocate(CellWords * sizeof(u32)));
            for (s32 index = CellWords - 1; index >= 0; index--)
            {
                cells[index] = 0;
            }

            chunk->instanceCells = cells;
        }

        SortIntoCells(&instance->collision, chunk->scenery, chunk->instanceCells, instance);
    }

    return chunk->instances->AddInstance(instance) & 0xFF;
}
