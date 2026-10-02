#include "game/chunkloading.h"

#include "game/array.h"

#include "game/clock.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/readers.h"
#include "game/reference.h"
#include "game/renderer.h"
#include "game/sound.h"
#include "game/stream.h"

// The chunks' loading: loaders of the chunks wanted around what the loading follows, and the links between chunks
namespace
{
constexpr u32 CountMask = 0xFFFFFF;
constexpr u32 Owns = 0x1000000;
// GCC's pointers to the lists' members (their offsets + 1), passed along and unused
constexpr s32 LinkPrevious = 0xC9;
constexpr s32 LinkNext = 0xC5;
constexpr s32 LoaderPrevious = 0x31;
constexpr s32 LoaderNext = 0x2D;

constexpr u32 Unloading = 0x40000000;
// The chunk data's bits 18-21
constexpr u32 ShownMask = 0x3C0000;
constexpr u32 Shown = 0x40000;
}

extern "C"
{
    extern const GccVTableEntry g_ChunkLoaderBaseVTable[] RETAIL(D_003069F0);
    extern const u8 CasingTable[];

    void LinkMatricesConstruct(ChunkLinkData* data) RETAIL(FUN_001f00b8);
    void LinkMatricesDestroy(ChunkLinkData* data, u32 flags) RETAIL(FUN_001f00e0);
    void LinkMatricesRead(ChunkLinkData* data, Stream* reader) RETAIL(FUN_001f0260);
    extern const GccVTableEntry g_Sm2LoaderVTable[] RETAIL(ChunkSm2Loader_Methods);
    extern const GccVTableEntry g_Rm2LoaderVTable[] RETAIL(ChunkRm2Loader_Methods);
    extern const GccVTableEntry g_ChunkLoadingUtilVTable[] RETAIL(D_002FC2E0);
    extern const GccVTableEntry g_Sm2LoadingUtilVTable[] RETAIL(ChunkLoadUtil_Sm2_Methods);
    extern const GccVTableEntry g_Rm2LoadingUtilVTable[] RETAIL(ChunkLoadUtil_Rm2_Methods);

    // The resources take the objects the RM2 brought
    void AddChunkObjects(void* resources, u32* objects) RETAIL(FUN_00266180);

    void DestroyPendingInstances() RETAIL(FUN_001996b0);
    void UnloadAllChunks(ChunkManager* chunks) RETAIL(UnloadAllChunks_);
}

namespace
{
// One reference less: the loader goes with the last owning one, the reference with the last one
void ReleaseLoaderReference(LoaderReference** handle)
{
    LoaderReference* reference = *handle;
    u32 value = reference->value;
    u32 count = ((value & CountMask) - 1) & CountMask;
    reference->value = (value & ~CountMask) | count;
    if (count == 0 && (value & Owns) != 0)
    {
        if (reference->loader != nullptr)
        {
            reference->loader->Destroy(3);
        }

        reference->loader = nullptr;
    }

    if ((reference->value & CountMask) != 0)
    {
        return;
    }

    reference = *handle;
    ChunkLoader* loader = reference->loader;
    if (loader == nullptr)
    {
        if ((reference->value & Owns) != 0)
        {
            reference->loader = nullptr;
        }

        MemoryDeallocate2_(reference);
        *handle = nullptr;
        return;
    }

    LoaderReference* block = loader->self;
    if (block != nullptr)
    {
        if ((block->value & Owns) != 0)
        {
            if (block->loader != nullptr)
            {
                block->loader->Destroy(3);
            }

            block->loader = nullptr;
        }

        MemoryDeallocate2_(block);
        loader->self = nullptr;
    }

    *handle = nullptr;
}

LoaderReference* AddLoaderReference(ChunkLoader* loader)
{
    if (loader->self == nullptr)
    {
        auto* block = static_cast<LoaderReference*>(MemoryAllocate(sizeof(LoaderReference)));
        block->loader = loader;
        // The new block's top bits are kept
        block->value &= 0xFE000000;
        loader->self = block;
    }

    LoaderReference* reference = loader->self;
    reference->value = (reference->value & ~CountMask) | (((reference->value & CountMask) + 1) & CountMask);
    return reference;
}

void LowerCase(String* string)
{
    for (s32 i = 0; i < string->length; i++)
    {
        char* character = string->string + i;
        s8 value = *character;
        if ((CasingTable[value] & 1) != 0)
        {
            *character = static_cast<char>(value + 0x20);
        }
    }
}

// The loaders of chunks under the manager's path are counted and stepped
bool UnderManagerPath(ChunkLoader* loader)
{
    ChunkLoadingManager* manager = loader->manager;
    return manager->path.length == 0 || StringFind(&loader->path, 0, manager->path.string) == 0;
}

void UnlinkAll(ChunkLoader* loader)
{
    GameChunkLink* link = loader->links;
    while (link != nullptr)
    {
        GameChunkLink* next = link->next;
        if (loader->sm2 != nullptr)
        {
            loader->sm2->Unlink(link);
        }

        if (loader->rm2 != nullptr)
        {
            loader->rm2->Unlink(link);
        }

        LinkListRemove(link, &loader->links, LinkPrevious, LinkNext);
        if (link != nullptr)
        {
            DestroyChunkLink(link, 3);
        }

        link = next;
    }
}
}

extern "C"
{
    ChunkLoadingManager* ConstructChunkLoadingManager(ChunkLoadingManager* manager, TimeClock* clock, u32 state)
    {
        manager->path.string = nullptr;
        manager->path.capacity = 0;
        manager->path.length = 0;
        manager->clock = clock;
        manager->frame = 0;
        manager->loaders = nullptr;
        manager->focusLoader = nullptr;
        manager->focus = nullptr;
        manager->bits = 0;
        manager->bits = (manager->bits & 0xF0FFFFFF) | (state & 0xF) << 24;
        return manager;
    }

    ChunkLoader* FindChunkLoader(ChunkLoadingManager* manager, String* path)
    {
        ChunkLoader* loader = manager->loaders;
        String lower;
        lower.string = nullptr;
        lower.length = 0;
        lower.capacity = 0;
        StringAssign(&lower, path->string);
        LowerCase(&lower);

        while (loader != nullptr && StringNotEqual(&loader->path, lower.string))
        {
            loader = loader->next;
        }

        StringDestroy(&lower);
        return loader;
    }

    ChunkLoader* FindChunkLoaderOf(ChunkLoadingManager* manager, ReferencedObject* object)
    {
        for (ChunkLoader* loader = manager->loaders; loader != nullptr; loader = loader->next)
        {
            bool holds = loader->rm2 != nullptr && loader->rm2->Holds(object);
            if (loader->sm2 != nullptr)
            {
                holds |= loader->sm2->Holds(object);
            }

            if (holds)
            {
                return loader;
            }
        }

        return nullptr;
    }

    void SetChunkLoadingFocus(ChunkLoadingManager* manager, ReferencedObject* object)
    {
        if ((manager->bits & 0x40000000) != 0)
        {
            return;
        }

        ChunkLoader* loader = FindChunkLoaderOf(manager, object);
        ReferencedObject* current = manager->focus != nullptr ? manager->focus->object : nullptr;
        if (current != object)
        {
            RemoveReference(&manager->focus);
            manager->focus = object != nullptr ? AddReference(object) : nullptr;
        }

        if (loader != nullptr)
        {
            manager->focusLoader = loader;
        }
    }

    bool ChunkLoaderIsLoaded(ChunkLoader* loader, bool any)
    {
        bool loaded = true;
        if (loader->rm2 != nullptr)
        {
            loaded = loader->rm2->IsLoaded(any);
        }

        if (loader->sm2 != nullptr)
        {
            loaded &= loader->sm2->IsLoaded(any);
        }

        return loaded;
    }

    void ChunkLoaderRemoveLinks(ChunkLoader* loader)
    {
        UnlinkAll(loader);
    }

    bool ChunkLoaderStep(ChunkLoader* loader, s32 frame, bool now)
    {
        bool busy = false;
        if (UnderManagerPath(loader))
        {
            // Not reached by the links this frame: wanted at no depth
            if (loader->frame != frame)
            {
                loader->bits |= 0xFF;
            }

            busy = loader->sm2->Step(now);
            busy |= loader->rm2->Step(now);
        }

        ChunkData* data = nullptr;
        if (loader->sm2 != nullptr && loader->sm2->data != nullptr)
        {
            data = loader->sm2->data->data;
        }

        if (data == nullptr)
        {
            return busy;
        }

        u64 shown = data->bits & ShownMask;
        if (ChunkLoaderIsLoaded(loader, false))
        {
            if (shown == 0)
            {
                data->bits = (data->bits & ~ShownMask) | Shown;
            }
        }
        else if (shown == Shown)
        {
            data->bits &= ~ShownMask;
        }

        return busy;
    }

    bool LinkedChunksLoaded(ChunkLoader* loader, bool any)
    {
        for (GameChunkLink* link = loader->links; link != nullptr; link = link->next)
        {
            ChunkLoader* linked = link->loader != nullptr ? link->loader->loader : nullptr;
            if (linked == nullptr)
            {
                return false;
            }

            if (ChunkLoaderIsLoaded(linked, any))
            {
                continue;
            }

            Rm2Loader* rm2 = linked->rm2;
            Sm2Loader* sm2 = linked->sm2;
            bool rm2Unwanted = rm2 != nullptr && rm2->IsUnwanted();
            bool sm2Unwanted = sm2 != nullptr && sm2->IsUnwanted();
            if (rm2Unwanted && sm2Unwanted)
            {
                continue;
            }

            // A chunk always has both loaders (the game doesn't check the other one)
            if (rm2Unwanted && sm2->IsLoaded(any))
            {
                continue;
            }

            if (sm2Unwanted && rm2->IsLoaded(any))
            {
                continue;
            }

            return false;
        }

        return true;
    }

    ChunkLoader* QueueChunk(ChunkLoadingManager* manager, String* path, u32 depth)
    {
        ChunkLoader* loader = FindChunkLoader(manager, path);
        u32 depths = (depth & 0xF) << 4 | (depth & 0xF);
        if (loader != nullptr)
        {
            loader->bits = (loader->bits & 0xFFFFFF00) | depths;
        }
        else
        {
            loader = static_cast<ChunkLoader*>(MemoryAllocate(sizeof(ChunkLoader)));
            loader->frame = -1;
            loader->self = nullptr;
            loader->path.string = nullptr;
            loader->path.length = 0;
            loader->path.capacity = 0;
            StringAssign(&loader->path, path->string);
            loader->clock = manager->clock;
            loader->manager = manager;
            loader->links = nullptr;
            loader->next = nullptr;
            loader->previous = nullptr;
            loader->bits = 0;
            loader->bits = (loader->bits & 0xFFFFFF00) | depths;
            LowerCase(&loader->path);
            loader->sm2 = Sm2Loader::Construct(static_cast<Sm2Loader*>(MemoryAllocate(sizeof(Sm2Loader))), loader);
            loader->rm2 = Rm2Loader::Construct(static_cast<Rm2Loader*>(MemoryAllocate(sizeof(Rm2Loader))), loader);
            u32 bits = (loader->bits & ~0x100u) | (static_cast<s32>(manager->bits) >> 20 & 0x100);
            loader->bits = bits;
            loader->bits = (bits & ~0x200u) | (static_cast<s32>(manager->bits) >> 20 & 0x200);
            if (!LoaderListContains(loader, &manager->loaders, LoaderPrevious, LoaderNext))
            {
                if (UnderManagerPath(loader))
                {
                    manager->bits = (manager->bits & ~0xFFFu) | (((manager->bits & 0xFFF) + 1) & 0xFFF);
                }

                LoaderListPushFront(loader, &manager->loaders, LoaderPrevious, LoaderNext);
            }
        }

        if (depth == 0)
        {
            manager->focusLoader = loader;
        }

        return loader;
    }

    bool UpdateChunkLoading(ChunkLoadingManager* manager, bool now, u8* read)
    {
        u32 firstState = manager->bits >> 24 & 0xF;
        bool loadAll = firstState - 1 < 2;
        bool pending;
        do
        {
            u32 bits = manager->bits;
            manager->bits = bits & 0xFF000FFF;
            u32 state = bits >> 24 & 0xF;
            // The loaders the links queue now are stepped next time
            ChunkLoader* loader = manager->loaders;
            if (manager->focusLoader != nullptr)
            {
                LoadLinkedChunks(manager->focusLoader, manager->frame, 0, 0);
            }

            while (loader != nullptr)
            {
                ChunkLoader* next = loader->next;
                if (ChunkLoaderStep(loader, manager->frame, now))
                {
                    if (ChunkLoaderIsLoaded(loader, false))
                    {
                        u32 loaded = ((manager->bits >> 12 & 0xFFF) + 1) & 0xFFF;
                        manager->bits = (manager->bits & 0xFF000FFF) | loaded << 12;
                    }
                }
                else if (state == 5)
                {
                    if (LoaderListContains(loader, &manager->loaders, LoaderPrevious, LoaderNext))
                    {
                        if (UnderManagerPath(loader))
                        {
                            manager->bits = (manager->bits & ~0xFFFu) | (((manager->bits & 0xFFF) - 1) & 0xFFF);
                        }

                        LoaderListRemove(loader, &manager->loaders, LoaderPrevious, LoaderNext);
                    }

                    loader->Destroy(3);
                }

                loader = next;
            }

            if (now)
            {
                LoadQueuedSectionsIntoMemory_();
                *read = 0;
            }
            else
            {
                bool started;
                *read = static_cast<u8>(ReadersStep(&started));
            }

            // Fewer loaded than there are
            pending = ((manager->bits ^ manager->bits >> 12) & 0xFFF) != 0;
            manager->frame++;
        } while (now && loadAll && pending);

        return pending && *read != 0;
    }

    void UnloadEverything(ChunkLoadingManager* manager, bool now, ChunkManager* chunks)
    {
        if ((manager->bits & Unloading) != 0)
        {
            return;
        }

        manager->focusLoader = nullptr;
        manager->bits |= Unloading;
        if (manager->focus != nullptr && manager->focus->object != nullptr)
        {
            RemoveReference(&manager->focus);
            manager->focus = nullptr;
        }

        // Without now the focus isn't followed until the next unloading clears the bit
        if (!now)
        {
            return;
        }

        StopAllSound();
        RenderTargetDescription* target = G_Renderer_->target;
        GameRendererController* renderer = G_GameRendererController;
        for (s32 i = 0; i < 2; i++)
        {
            SetUpFrame(target, false, false);
            renderer->FinishScene(0);
            renderer->Present(false);
        }

        while (manager->loaders != nullptr)
        {
            u8 read;
            UpdateChunkLoading(manager, now, &read);
            BackgroundWork(false);
        }

        DestroyPendingInstances();
        while (BackgroundWork(false))
        {
        }

        if (chunks != nullptr)
        {
            UnloadAllChunks(chunks);
        }

        manager->bits &= ~Unloading;
    }

    void LoadLinkedChunks(ChunkLoader* loader, s32 frame, u32 keepDepth, u32 depth)
    {
        // Lower depths than this frame's, or a new frame
        bool newFrame = loader->frame != frame;
        bool keepLower = keepDepth < (loader->bits >> 4 & 0xF);
        bool lower = depth < (loader->bits & 0xF);
        if (!newFrame && !keepLower && !lower)
        {
            return;
        }

        u32 state = loader->manager->bits >> 24 & 0xF;
        if (newFrame)
        {
            loader->bits = (loader->bits & 0xFFFFFF00) | (keepDepth & 0xF) << 4 | (depth & 0xF);
            loader->frame = frame;
        }
        else
        {
            if (keepLower)
            {
                loader->bits = (loader->bits & ~0xF0u) | (keepDepth & 0xF) << 4;
            }

            if (lower)
            {
                loader->bits = (loader->bits & ~0xFu) | (depth & 0xF);
            }
        }

        if (state == 0)
        {
            return;
        }

        // A chunk no file loader wants isn't followed while unloading
        if ((loader->bits & 0x200) == 0 && state == 5)
        {
            bool unwanted = loader->rm2 == nullptr || loader->rm2->IsUnwanted();
            if (loader->sm2 != nullptr)
            {
                unwanted &= loader->sm2->IsUnwanted();
            }

            if (unwanted)
            {
                return;
            }
        }

        GameChunkLink* link = loader->links;
        bool allLoaded = link != nullptr;
        bool allQueued = link != nullptr;
        for (; link != nullptr; link = link->next)
        {
            bool keep = (link->data.flags & ChunkLinkData::KeepMask) != 0;
            u32 nextKeepDepth = keepDepth != 0xF && keep ? keepDepth + 1 : 0xF;
            u32 nextDepth = depth != 0xF ? depth + 1 : 0xF;
            ChunkLoader* linked = link->loader != nullptr ? link->loader->loader : nullptr;
            if (loader->sm2 != nullptr)
            {
                loader->sm2->UpdateLink(link);
            }

            if (loader->rm2 != nullptr)
            {
                loader->rm2->UpdateLink(link);
            }

            if (linked == nullptr)
            {
                linked = FindChunkLoader(loader->manager, &link->path);
                if (linked == nullptr && state != 1)
                {
                    linked = QueueChunk(loader->manager, &link->path, 0xF);
                }

                LoaderReference* reference = linked != nullptr ? AddLoaderReference(linked) : nullptr;
                AssignLoaderReference(&link->loader, &reference);
                if (reference != nullptr)
                {
                    ReleaseLoaderReference(&reference);
                }

                if (linked == nullptr)
                {
                    allLoaded = false;
                    allQueued = false;
                    continue;
                }
            }

            bool follow = true;
            if ((loader->bits & 0x200) == 0 && link->hulls != nullptr)
            {
                ReferencedObject* focus = loader->manager->focus != nullptr ? loader->manager->focus->object : nullptr;
                if (focus == nullptr)
                {
                    follow = (link->type >> 1 & 1) != 0;
                }
                else
                {
                    // The focus is an instance: its place is at 8
                    ObjectPlace* place = focus->place;
                    if ((place->bits & 4) != 0)
                    {
                        place->position.x = place->matrix.m[3][0];
                        place->position.w = place->matrix.m[3][3];
                        place->position.y = place->matrix.m[3][1];
                        place->position.z = place->matrix.m[3][2];
                        place->bits &= ~static_cast<u64>(5);
                    }

                    Vector4 position = place->position;
                    follow = IsPositionInLinkHulls(link->hulls, &position);
                }
            }

            if (follow)
            {
                LoadLinkedChunks(linked, frame, nextKeepDepth, nextDepth);
            }

            // A keep link's chunk counts with its RM2, the others with their SM2 alone
            if (keep)
            {
                Rm2Loader* rm2 = linked->rm2;
                allLoaded &= rm2 != nullptr && rm2->IsLoaded(false);
                allQueued &= rm2 != nullptr && (rm2->IsQueued(false) || rm2->IsLoaded(false));
            }

            Sm2Loader* sm2 = linked->sm2;
            allLoaded &= sm2 != nullptr && sm2->IsLoaded(false);
            allQueued &= sm2 != nullptr && (sm2->IsQueued(false) || sm2->IsLoaded(false));
        }

        loader->bits = (loader->bits & ~0xC00u) | static_cast<u32>(allLoaded) << 10 | static_cast<u32>(allQueued) << 11;
    }

    void LoadChunkLinks(ChunkLoader* loader, Stream* reader)
    {
        u32 count;
        reader->ReadS32(reinterpret_cast<s32*>(&count));
        for (u32 i = 0; i < count; i++)
        {
            auto* link = static_cast<GameChunkLink*>(MemoryAllocate(sizeof(GameChunkLink)));
            link->path.string = nullptr;
            link->path.capacity = 0;
            link->path.length = 0;
            link->loader = nullptr;
            LinkMatricesConstruct(&link->data);
            link->hulls = nullptr;
            link->next = nullptr;
            link->previous = nullptr;
            ReadChunkLink(link, reader);
            LinkListPushFront(link, &loader->links, LinkPrevious, LinkNext);
        }
    }

    void ReadChunkLink(GameChunkLink* link, Stream* reader)
    {
        if (link->hulls != nullptr)
        {
            FreeLinkHullList(link->hulls, 3);
        }

        reader->Read(&link->type, 4, 1);
        StringRead(&link->path, reader);
        LinkMatricesRead(&link->data, reader);
        if ((link->type & 1) != 0)
        {
            link->hulls = InitLinkHullList(static_cast<LinkHullList*>(MemoryAllocate(sizeof(LinkHullList))), reader);
        }
        else
        {
            link->hulls = nullptr;
        }
    }

    void DestroyChunkLink(GameChunkLink* link, u32 flags)
    {
        if (link->hulls != nullptr)
        {
            FreeLinkHullList(link->hulls, 3);
        }

        LinkMatricesDestroy(&link->data, 2);
        if (link->loader != nullptr)
        {
            ReleaseLoaderReference(&link->loader);
        }

        StringDestroy(&link->path);
        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(link);
        }
    }

    LoaderReference** AssignLoaderReference(LoaderReference** to, LoaderReference** from)
    {
        if (*to == *from)
        {
            return to;
        }

        if (*to != nullptr)
        {
            ReleaseLoaderReference(to);
        }

        *to = *from;
        if (*to != nullptr)
        {
            (*to)->value = ((*to)->value & ~CountMask) | ((((*to)->value & CountMask) + 1) & CountMask);
        }

        return to;
    }

    LinkHullList* InitLinkHullList(LinkHullList* list, Stream* reader)
    {
        list->next = nullptr;
        HullConstruct(&list->hull);
        ReadLinkHullList(list, reader);
        return list;
    }

    void FreeLinkHullList(LinkHullList* list, u32 flags)
    {
        if (list->next != nullptr)
        {
            FreeLinkHullList(list->next, 3);
        }

        HullDestroy(&list->hull, 2);
        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(list);
        }
    }

    bool IsPositionInLinkHulls(LinkHullList* list, const Vector4* position)
    {
        if (IsPointInsideHull(&list->hull, position))
        {
            return true;
        }

        return list->next != nullptr && IsPositionInLinkHulls(list->next, position);
    }

    void ReadLinkHullList(LinkHullList* list, Stream* reader)
    {
        LinkHullList* old = list->next;
        if (old != nullptr)
        {
            if (old->next != nullptr)
            {
                FreeLinkHullList(old->next, 3);
            }

            HullDestroy(&old->hull, 2);
            MemoryDeallocate2_(old);
        }

        reader->Read(&list->flags, 4, 1);
        ReadModelCollisionData(&list->hull, reader);
        if ((list->flags & 1) == 0)
        {
            list->next = nullptr;
            return;
        }

        auto* next = static_cast<LinkHullList*>(MemoryAllocate(sizeof(LinkHullList)));
        next->next = nullptr;
        HullConstruct(&next->hull);
        ReadLinkHullList(next, reader);
        list->next = next;
    }

    void LinkListRemove(GameChunkLink* link, GameChunkLink** head, s32, s32)
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

    void LinkListPushFront(GameChunkLink* link, GameChunkLink** head, s32, s32)
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

    bool LoaderListContains(ChunkLoader* loader, ChunkLoader** head, s32, s32)
    {
        if (loader->previous == nullptr && loader->next == nullptr)
        {
            return *head == loader;
        }

        return true;
    }

    void LoaderListPushFront(ChunkLoader* loader, ChunkLoader** head, s32, s32)
    {
        if (*head == nullptr)
        {
            *head = loader;
            loader->previous = nullptr;
            loader->next = nullptr;
            return;
        }

        (*head)->previous = loader;
        loader->next = *head;
        *head = loader;
    }

    void LoaderListRemove(ChunkLoader* loader, ChunkLoader** head, s32, s32)
    {
        if (loader->previous == nullptr)
        {
            if (loader->next == nullptr)
            {
                *head = nullptr;
            }
            else
            {
                loader->next->previous = nullptr;
                *head = loader->next;
            }
        }
        else if (loader->next == nullptr)
        {
            loader->previous->next = nullptr;
        }
        else
        {
            loader->previous->next = loader->next;
            loader->next->previous = loader->previous;
        }

        loader->next = nullptr;
        loader->previous = nullptr;
    }
}

void ChunkLoader::Destroy(u32 flags)
{
    UnlinkAll(this);
    if (rm2 != nullptr)
    {
        rm2->Destroy(3);
    }

    if (sm2 != nullptr)
    {
        sm2->Destroy(3);
    }

    StringDestroy(&path);
    if (self != nullptr)
    {
        self->loader = nullptr;
    }

    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

ChunkLoaderBase* ChunkLoaderBase::Construct(ChunkLoaderBase* base, ChunkLoader* loader)
{
    base->loader = loader;
    base->vtable = g_ChunkLoaderBaseVTable;
    base->state = None;
    base->util = nullptr;
    return base;
}

void ChunkLoaderBase::DestroyBase(u32 flags)
{
    vtable = g_ChunkLoaderBaseVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

bool ChunkLoaderBase::StepStates(bool now)
{
    u32 managerState = loader->manager->bits >> 24 & 0xF;
    u32 clockTime = loader->clock->time;
    if (state != None)
    {
        s32 elapsed = static_cast<s32>(clockTime - static_cast<u32>(time));
        s32 next = -1;
        switch (state)
        {
        case Queued:
            if (managerState - 1 < 3)
            {
                if (StartLoading(0, now))
                {
                    next = Loading;
                }
            }
            else if (loader->rm2 != nullptr && loader->rm2->state == Queued && util->Wants(loader))
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond);
                next = WaitingToLoad;
            }

            break;
        case WaitingToLoad:
            if (util->DoesNotWant(loader))
            {
                next = Queued;
            }
            else if ((now || delay < elapsed) && StartLoading(0, now))
            {
                next = Loading;
            }

            break;
        case Loading:
            if (!ContinueLoading(0, now))
            {
                next = Loaded;
            }

            break;
        case Loaded:
        {
            ChunkLoader* chunk = loader;
            bool loading = chunk->rm2 == nullptr || chunk->rm2->IsLoading(true);
            if (chunk->sm2 != nullptr)
            {
                loading &= chunk->sm2->IsLoading(true);
            }

            if (loading || managerState != 5)
            {
                break;
            }

            // Not reached by any link this frame: gone soon
            if ((chunk->bits & 0xFF) == 0xFF)
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond * Rounded(0.1));
                next = WaitingToUnload;
            }
            else if (util->DoesNotWant(chunk))
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond * 4.0f);
                next = WaitingToUnload;
            }

            break;
        }
        case WaitingToUnload:
            if (util->Wants(loader))
            {
                next = Loaded;
            }
            else if ((now || delay < elapsed) && StartUnloading(0, now))
            {
                next = Unloading;
            }

            break;
        case Unloading:
            if (!ContinueUnloading(0, now))
            {
                next = Unloaded;
            }

            break;
        case Unloaded:
            if ((loader->rm2 != nullptr && loader->rm2->state == Loaded) || (loader->sm2 != nullptr && loader->sm2->state == Loaded))
            {
                next = Queued;
            }

            break;
        default:
            break;
        }

        if (next != -1)
        {
            state = next;
            time = static_cast<s32>(clockTime);
        }
    }

    return state != Queued && state != Unloaded;
}

bool ChunkLoaderBase::AnyUtilWants()
{
    return util->Wants(loader);
}

bool ChunkLoaderBase::NoUtilWants()
{
    return util->DoesNotWant(loader);
}

bool ChunkLoaderBase::IsQueued(bool any)
{
    return any ? static_cast<u32>(state - Queued) < 2 : state == Queued;
}

bool ChunkLoaderBase::IsLoading(bool any)
{
    return any ? static_cast<u32>(state - WaitingToLoad) < 2 : state == Loading;
}

bool ChunkLoaderBase::IsLoaded(bool any)
{
    return any ? static_cast<u32>(state - Loaded) < 2 : state == Loaded;
}

namespace
{
ChunkData* DataOf(const ChunkDataReference* reference)
{
    return reference != nullptr ? reference->data : nullptr;
}

ChunkData* ChunkDataOf(ChunkLoader* loader)
{
    return DataOf(loader->sm2->data);
}
}

void Sm2LoadingUtil::Destroy(u32 flags)
{
    vtable = g_ChunkLoadingUtilVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

bool Sm2LoadingUtil::Wants(ChunkLoader* loader)
{
    return (loader->bits & 0xF) < 2;
}

bool Sm2LoadingUtil::DoesNotWant(ChunkLoader* loader)
{
    return !((loader->bits & 0xF) < 2);
}

bool Sm2LoadingUtil::AtSecondDepth(ChunkLoader* loader)
{
    return (loader->bits & 0xF) == 2;
}

void Rm2LoadingUtil::Destroy(u32 flags)
{
    vtable = g_ChunkLoadingUtilVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

bool Rm2LoadingUtil::Wants(ChunkLoader* loader)
{
    return (loader->bits >> 4 & 0xF) < 2;
}

bool Rm2LoadingUtil::DoesNotWant(ChunkLoader* loader)
{
    return !((loader->bits >> 4 & 0xF) < 2);
}

bool Rm2LoadingUtil::AtSecondDepth(ChunkLoader* loader)
{
    return (loader->bits >> 4 & 0xF) == 2;
}

Sm2Loader* Sm2Loader::Construct(Sm2Loader* loader, ChunkLoader* chunk)
{
    ChunkLoaderBase::Construct(loader, chunk);
    loader->reader = nullptr;
    loader->data = nullptr;
    loader->vtable = g_Sm2LoaderVTable;
    loader->state = Queued;
    auto* util = static_cast<ChunkLoadingUtil*>(MemoryAllocate(sizeof(ChunkLoadingUtil)));
    loader->util = util;
    util->vtable = g_Sm2LoadingUtilVTable;
    return loader;
}

void Sm2Loader::Destroy(u32 flags)
{
    vtable = g_Sm2LoaderVTable;
    if (reader != nullptr)
    {
        reader->Destroy(3);
    }

    if (util != nullptr)
    {
        util->Destroy(3);
    }

    if (data != nullptr)
    {
        ReleaseChunkDataReference(&data);
    }

    DestroyBase(flags);
}

bool Sm2Loader::StartLoading(s32 index, bool now)
{
    if (index != 0 || DataOf(data) != nullptr)
    {
        return false;
    }

    String path;
    path.string = nullptr;
    path.length = 0;
    path.capacity = 0;
    StringAssign(&path, loader->path.string);
    auto* chunk = static_cast<ChunkData*>(MemoryAllocate(0x1F0));
    AssignChunkData(&data, ConstructChunkData(chunk, GetChunkList(), path.string));
    ChunkDataLights(DataOf(data));
    ChunkDataContext(DataOf(data));
    reader = Sm2Reader::Construct(static_cast<Sm2Reader*>(MemoryAllocate(sizeof(Sm2Reader))), this);
    // The loader's bit 8 queues the file's reading
    reader->Queue(now, (loader->bits >> 8 & 1) != 0);
    StringDestroy(&path);
    return true;
}

bool Sm2Loader::ContinueLoading(s32, bool)
{
    if (reader->reading != 0)
    {
        return true;
    }

    if (reader != nullptr)
    {
        reader->Destroy(3);
    }

    reader = nullptr;
    return false;
}

bool Sm2Loader::StartUnloading(s32, bool)
{
    return ReleaseChunkData(DataOf(data), true, true, true) != 0;
}

bool Sm2Loader::ContinueUnloading(s32, bool)
{
    if (DataOf(data) == nullptr)
    {
        return false;
    }

    if (ChunkDataReleasing(DataOf(data), true, nullptr) != 0)
    {
        return true;
    }

    ChunkListRemove(GetChunkList(), data->data);
    ChunkData* chunk = DataOf(data);
    if (chunk != nullptr)
    {
        DestroyChunkData(chunk, 3);
    }

    AssignChunkData(&data, nullptr);
    ChunkLoaderRemoveLinks(loader);
    return false;
}

bool Sm2Loader::Holds(ReferencedObject* object)
{
    ChunkData* chunk = DataOf(data);
    if (chunk == nullptr)
    {
        return false;
    }

    // An object's chunk is at 0xA0
    return *reinterpret_cast<ChunkData**>(reinterpret_cast<u8*>(object) + 0xA0) == chunk;
}

bool Sm2Loader::Step(bool now)
{
    ChunkData* chunk = DataOf(data);
    if (chunk != nullptr)
    {
        u32 depth = loader->bits & 0xF;
        s32 detail = 0;
        if (depth < 2)
        {
            detail = -1;
        }
        else if (depth == 2)
        {
            detail = 8;
        }

        chunk->detail = detail;
    }

    return StepStates(now);
}

void Sm2Loader::UpdateLink(GameChunkLink* link)
{
    ChunkLoader* linked = link->loader != nullptr ? link->loader->loader : nullptr;
    ChunkDataLink(DataOf(data), &link->data);
    if (linked == nullptr || linked->sm2 == nullptr)
    {
        return;
    }

    ChunkData* linkedData = DataOf(linked->sm2->data);
    link->data.linkedData = linkedData;
    link->data.flags = (link->data.flags & ~ChunkLinkData::HasLinkedData) | static_cast<u32>(linkedData != nullptr) << 17;
}

void Sm2Loader::Unlink(GameChunkLink* link)
{
    if (DataOf(data) != nullptr)
    {
        ChunkDataUnlink(DataOf(data), &link->data);
    }
}

Rm2Loader* Rm2Loader::Construct(Rm2Loader* loader, ChunkLoader* chunk)
{
    ChunkLoaderBase::Construct(loader, chunk);
    loader->reader = nullptr;
    loader->entry = nullptr;
    loader->vtable = g_Rm2LoaderVTable;
    loader->state = Queued;
    auto* util = static_cast<ChunkLoadingUtil*>(MemoryAllocate(sizeof(ChunkLoadingUtil)));
    loader->util = util;
    util->vtable = g_Rm2LoadingUtilVTable;
    return loader;
}

void Rm2Loader::Destroy(u32 flags)
{
    vtable = g_Rm2LoaderVTable;
    if (util != nullptr)
    {
        util->Destroy(3);
    }

    DestroyBase(flags);
}

bool Rm2Loader::StartLoading(s32 index, bool now)
{
    if (index != 0)
    {
        return false;
    }

    ChunkData* chunk = ChunkDataOf(loader);
    if (chunk == nullptr)
    {
        return false;
    }

    entry = g_ChunkManager->AddChunk(loader->path.string, chunk);
    chunk->rm2Loads++;
    ChunkDataContext(chunk);
    ChunkDataUnknown(chunk, 100);
    ChunkDataCollision(chunk);
    ChunkEntry* added = entry;
    auto* objects = static_cast<u32*>(MemoryAllocate(0x404));
    objects[0] = 0;
    added->objects = objects;
    reader = Rm2Reader::Construct(static_cast<Rm2Reader*>(MemoryAllocate(sizeof(Rm2Reader))), entry);
    reader->Queue(now, (loader->bits >> 8 & 1) != 0);
    return true;
}

bool Rm2Loader::ContinueLoading(s32, bool)
{
    if ((static_cast<s32>(reader->bits) >> 1 & 1) != 0)
    {
        return true;
    }

    if ((static_cast<s32>(loader->bits) >> 9 & 1) == 0)
    {
        ChunkEntry* chunk = entry;
        ChunkManager* owner = chunk->manager;
        GameResources* resources = owner->resources;
        if (chunk->navigation == nullptr)
        {
            PathFinder* pathFinder = owner->pathFinder;
            AiNavigation* made = AiNavigation::Construct(static_cast<AiNavigation*>(MemoryAllocate(sizeof(AiNavigation))), nullptr);
            chunk->navigation = made;
            pathFinder->navigations[chunk->index] = made;
            if (made != nullptr)
            {
                made->SetPathFinder(pathFinder);
            }
        }

        AddChunkObjects(resources, chunk->objects);
    }

    if (reader != nullptr)
    {
        reader->Destroy(3);
    }

    reader = nullptr;
    return false;
}

bool Rm2Loader::StartUnloading(s32, bool)
{
    ChunkData* chunk = ChunkDataOf(loader);
    if (entry != nullptr)
    {
        g_ChunkManager->RemoveChunk(entry);
        entry = nullptr;
    }

    if (chunk != nullptr)
    {
        chunk->rm2Loads--;
        ReleaseChunkData(chunk, true, false, true);
    }

    return true;
}

bool Rm2Loader::ContinueUnloading(s32, bool)
{
    return false;
}

bool Rm2Loader::Holds(ReferencedObject* object)
{
    if (entry == nullptr)
    {
        return false;
    }

    Reference* reference = entry->data;
    ReferencedObject* chunk = reference != nullptr ? reference->object : nullptr;
    return *reinterpret_cast<ReferencedObject**>(reinterpret_cast<u8*>(object) + 0xA0) == chunk;
}

void Rm2Loader::UpdateLink(GameChunkLink* link)
{
    ChunkLoader* linked = link->loader != nullptr ? link->loader->loader : nullptr;
    u32 loaded = 0;
    if (linked != nullptr)
    {
        if (linked->rm2 == nullptr)
        {
            loaded = 0;
        }
        else
        {
            loaded = linked->rm2->IsLoaded(false) ? 1 : 0;
        }
    }

    link->data.flags = (link->data.flags & ~ChunkLinkData::LinkedRm2Loaded) | (loaded & 1) << 18;
}

void Rm2Loader::Unlink(GameChunkLink*)
{
}

void* ChunkEntry::OtherLayoutPosition(u32 index)
{
    if (positions == nullptr)
    {
        return nullptr;
    }

    return positions->data[positionCount + index];
}

u16 ChunkEntry::NextFlagSlot()
{
    return nextFlagSlot++;
}
