#include "game/chunkloading.h"

#include "game/array.h"

#include "game/clock.h"
#include "game/collision.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/particles.h"
#include "game/pools.h"
#include "game/readers.h"
#include "game/reference.h"
#include "game/renderer.h"
#include "game/resources.h"
#include "game/scenery.h"
#include "game/shadows.h"
#include "game/sound.h"
#include "game/stream.h"
#include "retail/libc.h"

// The chunks' loading: loaders of the chunks wanted around what the loading follows, and the links between chunks
namespace
{
// GCC's pointers to the lists' members, passed along and unused
constexpr s32 LinkPrevious = GCC2_MEMBER_POINTER(GameChunkLink, previous);
constexpr s32 LinkNext = GCC2_MEMBER_POINTER(GameChunkLink, next);
constexpr s32 LoaderPrevious = GCC2_MEMBER_POINTER(ChunkLoader, previous);
constexpr s32 LoaderNext = GCC2_MEMBER_POINTER(ChunkLoader, next);
// The seconds a loader waits before loading a wanted file and before unloading an unwanted one (a chunk no link reached this
// frame's sooner)
constexpr f32 LoadDelay = 1.0f;
constexpr f32 UnloadDelay = 4.0f;
constexpr f32 UnreachedUnloadDelay = Rounded(0.1);
// The frames drawn empty before everything's unloaded
constexpr s32 EmptyFrames = 2;
// The shadows a chunk's made with (its constructor ignores it: its lists hold 100)
constexpr s32 ShadowCount = 100;
// A chunk's list of the object IDs its RM2 brings: a count, then room for MostObjects IDs
constexpr u32 MostObjects = 0x200;
constexpr u32 ObjectListSize = sizeof(u32) + MostObjects * sizeof(u16);
}

extern "C"
{
    extern const GccVTableEntry g_ChunkLoaderBaseVTable[] RETAIL(D_003069F0);
    // The items' builders' base (BuilderBaseFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    extern const u8 CasingTable[];

    extern const GccVTableEntry g_Sm2LoaderVTable[] RETAIL(ChunkSm2Loader_Methods);
    extern const GccVTableEntry g_Rm2LoaderVTable[] RETAIL(ChunkRm2Loader_Methods);
    extern const GccVTableEntry g_WantPolicyVTable[] RETAIL(D_002FC2E0);
    extern const GccVTableEntry g_Sm2WantPolicyVTable[] RETAIL(ChunkLoadUtil_Sm2_Methods);
    extern const GccVTableEntry g_Rm2WantPolicyVTable[] RETAIL(ChunkLoadUtil_Rm2_Methods);

    void DestroyPendingInstances() RETAIL(FUN_001996b0);
    // The chunk manager's base and the persistent flags' stores' base
    extern const GccVTableEntry g_ChunkManagerBaseVTable[] RETAIL(D_00304208);
    extern const GccVTableEntry g_PersistentFlagsBaseVTable[] RETAIL(UnkChunkInterface_methods);
    // "Particle", the particles' file without the default chunk's RM2
    extern const char g_DefaultParticlesFile[] RETAIL(D_00303690);
}

namespace
{
// One reference less: the loader goes with the last owning one, the reference with the last one
void ReleaseLoaderReference(LoaderReference** handle)
{
    LoaderReference* reference = *handle;
    ReferenceBits bits = reference->bits;
    bits.count--;
    reference->bits = bits;
    if (bits.count == 0 && bits.owns)
    {
        if (reference->loader != nullptr)
        {
            reference->loader->Destroy(DestroyAndFree);
        }

        reference->loader = nullptr;
    }

    if (reference->bits.count != 0)
    {
        return;
    }

    reference = *handle;
    ChunkLoader* loader = reference->loader;
    if (loader == nullptr)
    {
        if (reference->bits.owns)
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
        if (block->bits.owns)
        {
            if (block->loader != nullptr)
            {
                block->loader->Destroy(DestroyAndFree);
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
        // The new block's bits above the count and the owning bit are kept
        ReferenceBits leftover = block->bits;
        leftover.count = 0;
        leftover.owns = 0;
        block->bits = leftover;
        loader->self = block;
    }

    LoaderReference* reference = loader->self;
    reference->bits.count++;
    return reference;
}

void LowerCase(String* string)
{
    for (s32 i = 0; i < string->length; i++)
    {
        char* character = string->string + i;
        s8 value = *character;
        if ((CasingTable[value] & RetailLibc::CasingUpperCase) != 0)
        {
            *character = static_cast<char>(value + ('a' - 'A'));
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
            DestroyChunkLink(link, DestroyAndFree);
        }

        link = next;
    }
}
}

extern "C"
{
    ChunkLoadingManager* ConstructChunkLoadingManager(ChunkLoadingManager* manager, TimeClock* clock, u32 mode)
    {
        manager->path.string = nullptr;
        manager->path.capacity = 0;
        manager->path.length = 0;
        manager->clock = clock;
        manager->frame = 0;
        manager->loaders = nullptr;
        manager->focusLoader = nullptr;
        manager->focus = nullptr;
        manager->bits.value = 0;
        manager->bits.mode = mode;
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
        if (manager->bits.unloading != 0)
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
                loader->bits.depth = ChunkLoader::NoDepth;
                loader->bits.keepDepth = ChunkLoader::NoDepth;
            }

            busy = loader->sm2->Step(now);
            busy |= loader->rm2->Step(now);
        }

        ChunkData* chunk = nullptr;
        if (loader->sm2 != nullptr && loader->sm2->data != nullptr)
        {
            chunk = loader->sm2->data->chunk;
        }

        if (chunk == nullptr)
        {
            return busy;
        }

        u32 state = chunk->flags.state;
        if (ChunkLoaderIsLoaded(loader, false))
        {
            if (state == ChunkHidden)
            {
                chunk->flags.state = ChunkShown;
            }
        }
        else if (state == ChunkShown)
        {
            chunk->flags.state = ChunkHidden;
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
        if (loader != nullptr)
        {
            loader->bits.depth = depth;
            loader->bits.keepDepth = depth;
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
            loader->bits.value = 0;
            loader->bits.depth = depth;
            loader->bits.keepDepth = depth;
            LowerCase(&loader->path);
            loader->sm2 = Sm2Loader::Construct(static_cast<Sm2Loader*>(MemoryAllocate(sizeof(Sm2Loader))), loader);
            loader->rm2 = Rm2Loader::Construct(static_cast<Rm2Loader*>(MemoryAllocate(sizeof(Rm2Loader))), loader);
            loader->bits.queuesFiles = manager->bits.queuesFiles;
            loader->bits.takesNoObjects = manager->bits.takesNoObjects;
            if (!LoaderListContains(loader, &manager->loaders, LoaderPrevious, LoaderNext))
            {
                if (UnderManagerPath(loader))
                {
                    manager->bits.loaderCount++;
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
        u32 firstMode = manager->bits.mode;
        bool waits = firstMode == LoadingKnownAtOnce || firstMode == LoadingAllAtOnce;
        bool pending;
        do
        {
            u32 mode = manager->bits.mode;
            manager->bits.loadedCount = 0;
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
                        manager->bits.loadedCount++;
                    }
                }
                else if (mode == LoadingStreamed)
                {
                    if (LoaderListContains(loader, &manager->loaders, LoaderPrevious, LoaderNext))
                    {
                        if (UnderManagerPath(loader))
                        {
                            manager->bits.loaderCount--;
                        }

                        LoaderListRemove(loader, &manager->loaders, LoaderPrevious, LoaderNext);
                    }

                    loader->Destroy(DestroyAndFree);
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
            pending = manager->bits.loadedCount != manager->bits.loaderCount;
            manager->frame++;
        } while (now && waits && pending);

        return pending && *read != 0;
    }

    void UnloadEverything(ChunkLoadingManager* manager, bool now, ChunkManager* chunks)
    {
        if (manager->bits.unloading != 0)
        {
            return;
        }

        manager->focusLoader = nullptr;
        manager->bits.unloading = 1;
        if (manager->focus != nullptr && manager->focus->object != nullptr)
        {
            RemoveReference(&manager->focus);
            manager->focus = nullptr;
        }

        // Without now (no caller asks for it) unloading stays set: the focus isn't followed again and later calls return at once
        if (!now)
        {
            return;
        }

        StopAllSound();
        RenderTargetDescription* target = G_Renderer_->target;
        GameRendererController* renderer = G_GameRendererController;
        for (s32 i = 0; i < EmptyFrames; i++)
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

        manager->bits.unloading = 0;
    }

    void LoadLinkedChunks(ChunkLoader* loader, s32 frame, u32 keepDepth, u32 depth)
    {
        // Lower depths than this frame's, or a new frame
        bool newFrame = loader->frame != frame;
        bool keepLower = keepDepth < loader->bits.keepDepth;
        bool lower = depth < loader->bits.depth;
        if (!newFrame && !keepLower && !lower)
        {
            return;
        }

        u32 mode = loader->manager->bits.mode;
        if (newFrame)
        {
            loader->bits.depth = depth;
            loader->bits.keepDepth = keepDepth;
            loader->frame = frame;
        }
        else
        {
            if (keepLower)
            {
                loader->bits.keepDepth = keepDepth;
            }

            if (lower)
            {
                loader->bits.depth = depth;
            }
        }

        if (mode == LoadingQueuedOnly)
        {
            return;
        }

        // Streamed, a chunk no file loader wants isn't followed
        if (loader->bits.takesNoObjects == 0 && mode == LoadingStreamed)
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
            bool keep = link->data.flags.keep != 0;
            u32 nextKeepDepth = keepDepth != ChunkLoader::NoDepth && keep ? keepDepth + 1 : ChunkLoader::NoDepth;
            u32 nextDepth = depth != ChunkLoader::NoDepth ? depth + 1 : ChunkLoader::NoDepth;
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
                if (linked == nullptr && mode != LoadingKnownAtOnce)
                {
                    linked = QueueChunk(loader->manager, &link->path, ChunkLoader::NoDepth);
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
            if (loader->bits.takesNoObjects == 0 && link->hulls != nullptr)
            {
                ReferencedObject* focus = loader->manager->focus != nullptr ? loader->manager->focus->object : nullptr;
                if (focus == nullptr)
                {
                    follow = link->type.loadsWithoutPlayer != 0;
                }
                else
                {
                    // The focus is an instance: its place is at 8
                    ObjectPlace* place = focus->place;
                    if (place->bits.matrixMoved != 0)
                    {
                        place->position.x = place->matrix.m[3][0];
                        place->position.w = place->matrix.m[3][3];
                        place->position.y = place->matrix.m[3][1];
                        place->position.z = place->matrix.m[3][2];
                        place->MarkPositionSynced();
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

        loader->bits.linkedLoaded = allLoaded;
        loader->bits.linkedQueued = allQueued;
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
            ConstructChunkLinkData(&link->data);
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
            FreeLinkHullList(link->hulls, DestroyAndFree);
        }

        reader->Read(&link->type, sizeof(link->type), 1);
        StringRead(&link->path, reader);
        ReadChunkLinkData(&link->data, reader);
        if (link->type.hasHulls != 0)
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
            FreeLinkHullList(link->hulls, DestroyAndFree);
        }

        DestroyChunkLinkData(&link->data, DestroyOnly);
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
            (*to)->bits.count++;
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
            FreeLinkHullList(list->next, DestroyAndFree);
        }

        HullDestroy(&list->hull, DestroyOnly);
        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(list);
        }
    }

    void* MakeChunkLinkItem(void*, u32 type)
    {
        if (type != LinkHullList::TypeId)
        {
            return nullptr;
        }

        auto* list = static_cast<LinkHullList*>(MemoryAllocate(sizeof(LinkHullList)));
        list->next = nullptr;
        HullConstruct(&list->hull);
        RetailLibc::MemorySet(&list->flags, 0, sizeof(list->flags));
        return list;
    }

    void DestroyChunkLinkItemBuilder(void* builder, u32 destroyFlags)
    {
        *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
        if ((destroyFlags & 1) != 0)
        {
            MemoryDeallocate2_(builder);
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
                FreeLinkHullList(old->next, DestroyAndFree);
            }

            HullDestroy(&old->hull, DestroyOnly);
            MemoryDeallocate2_(old);
        }

        reader->Read(&list->flags, sizeof(list->flags), 1);
        ReadModelCollisionData(&list->hull, reader);
        if (list->flags.hasNext == 0)
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
        rm2->Destroy(DestroyAndFree);
    }

    if (sm2 != nullptr)
    {
        sm2->Destroy(DestroyAndFree);
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
    base->policy = nullptr;
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
    u32 managerMode = loader->manager->bits.mode;
    u32 clockTime = loader->clock->time;
    if (state != None)
    {
        s32 elapsed = static_cast<s32>(clockTime - static_cast<u32>(stateTime));
        s32 next = -1;
        switch (state)
        {
        case Queued:
            if (managerMode >= LoadingKnownAtOnce && managerMode <= LoadingAtOnce)
            {
                if (StartLoading(0, now))
                {
                    next = Loading;
                }
            }
            else if (loader->rm2 != nullptr && loader->rm2->state == Queued && policy->Wants(loader))
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond * LoadDelay);
                next = WaitingToLoad;
            }

            break;
        case WaitingToLoad:
            if (policy->DoesNotWant(loader))
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

            if (loading || managerMode != LoadingStreamed)
            {
                break;
            }

            // Not reached by any link this frame: gone soon
            if (chunk->bits.depth == ChunkLoader::NoDepth && chunk->bits.keepDepth == ChunkLoader::NoDepth)
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond * UnreachedUnloadDelay);
                next = WaitingToUnload;
            }
            else if (policy->DoesNotWant(chunk))
            {
                delay = static_cast<s32>(g_ClockUnitsPerSecond * UnloadDelay);
                next = WaitingToUnload;
            }

            break;
        }
        case WaitingToUnload:
            if (policy->Wants(loader))
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
            stateTime = static_cast<s32>(clockTime);
        }
    }

    return state != Queued && state != Unloaded;
}

bool ChunkLoaderBase::PolicyWants()
{
    return policy->Wants(loader);
}

bool ChunkLoaderBase::PolicyDoesNotWant()
{
    return policy->DoesNotWant(loader);
}

bool ChunkLoaderBase::IsQueued(bool any)
{
    return any ? state == Queued || state == WaitingToLoad : state == Queued;
}

bool ChunkLoaderBase::IsLoading(bool any)
{
    return any ? state == WaitingToLoad || state == Loading : state == Loading;
}

bool ChunkLoaderBase::IsLoaded(bool any)
{
    return any ? state == Loaded || state == WaitingToUnload : state == Loaded;
}

namespace
{
ChunkData* DataOf(const ChunkDataReference* reference)
{
    return reference != nullptr ? reference->chunk : nullptr;
}

ChunkData* ChunkDataOf(ChunkLoader* loader)
{
    return DataOf(loader->sm2->data);
}
}

void Sm2WantPolicy::Destroy(u32 flags)
{
    vtable = g_WantPolicyVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

bool Sm2WantPolicy::Wants(ChunkLoader* loader)
{
    return loader->bits.depth < ChunkLoader::WantedDepths;
}

bool Sm2WantPolicy::DoesNotWant(ChunkLoader* loader)
{
    return !(loader->bits.depth < ChunkLoader::WantedDepths);
}

bool Sm2WantPolicy::JustPastWanted(ChunkLoader* loader)
{
    return loader->bits.depth == ChunkLoader::WantedDepths;
}

void Rm2WantPolicy::Destroy(u32 flags)
{
    vtable = g_WantPolicyVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

bool Rm2WantPolicy::Wants(ChunkLoader* loader)
{
    return loader->bits.keepDepth < ChunkLoader::WantedDepths;
}

bool Rm2WantPolicy::DoesNotWant(ChunkLoader* loader)
{
    return !(loader->bits.keepDepth < ChunkLoader::WantedDepths);
}

bool Rm2WantPolicy::JustPastWanted(ChunkLoader* loader)
{
    return loader->bits.keepDepth == ChunkLoader::WantedDepths;
}

Sm2Loader* Sm2Loader::Construct(Sm2Loader* loader, ChunkLoader* chunk)
{
    ChunkLoaderBase::Construct(loader, chunk);
    loader->reader = nullptr;
    loader->data = nullptr;
    loader->vtable = g_Sm2LoaderVTable;
    loader->state = Queued;
    auto* policy = static_cast<WantPolicy*>(MemoryAllocate(sizeof(WantPolicy)));
    loader->policy = policy;
    policy->vtable = g_Sm2WantPolicyVTable;
    return loader;
}

void Sm2Loader::Destroy(u32 flags)
{
    vtable = g_Sm2LoaderVTable;
    if (reader != nullptr)
    {
        reader->Destroy(DestroyAndFree);
    }

    if (policy != nullptr)
    {
        policy->Destroy(DestroyAndFree);
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
    auto* chunk = static_cast<ChunkData*>(MemoryAllocate(sizeof(ChunkData)));
    AssignChunkData(&data, ConstructChunkData(chunk, GetChunkList(), path.string));
    ChunkDataLights(DataOf(data));
    ChunkDataInstances(DataOf(data));
    reader = Sm2Reader::Construct(static_cast<Sm2Reader*>(MemoryAllocate(sizeof(Sm2Reader))), this);
    reader->Queue(now, loader->bits.queuesFiles != 0);
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
        reader->Destroy(DestroyAndFree);
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

    if (StepChunkData(DataOf(data), true, nullptr) != 0)
    {
        return true;
    }

    ChunkListRemove(GetChunkList(), data->chunk);
    ChunkData* chunk = DataOf(data);
    if (chunk != nullptr)
    {
        DestroyChunkData(chunk, DestroyAndFree);
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

    return object->chunk == chunk;
}

bool Sm2Loader::Step(bool now)
{
    ChunkData* chunk = DataOf(data);
    if (chunk != nullptr)
    {
        u32 depth = loader->bits.depth;
        s32 steppedKinds = 0;
        if (depth < ChunkLoader::WantedDepths)
        {
            steppedKinds = ChunkData::EveryKindStepped;
        }
        else if (depth == ChunkLoader::WantedDepths)
        {
            // Its models still animate
            steppedKinds = 1 << NodeModel;
        }

        chunk->steppedKinds = steppedKinds;
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
    link->data.flags.hasLinkedData = linkedData != nullptr;
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
    auto* policy = static_cast<WantPolicy*>(MemoryAllocate(sizeof(WantPolicy)));
    loader->policy = policy;
    policy->vtable = g_Rm2WantPolicyVTable;
    return loader;
}

void Rm2Loader::Destroy(u32 flags)
{
    vtable = g_Rm2LoaderVTable;
    if (policy != nullptr)
    {
        policy->Destroy(DestroyAndFree);
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
    chunk->holds++;
    ChunkDataInstances(chunk);
    ChunkShadowsOf(chunk, ShadowCount);
    ChunkDataCollision(chunk);
    ChunkEntry* added = entry;
    auto* objects = static_cast<u32*>(MemoryAllocate(ObjectListSize));
    objects[0] = 0;
    added->objects = objects;
    reader = Rm2Reader::Construct(static_cast<Rm2Reader*>(MemoryAllocate(sizeof(Rm2Reader))), entry);
    reader->Queue(now, loader->bits.queuesFiles != 0);
    return true;
}

bool Rm2Loader::ContinueLoading(s32, bool)
{
    if (reader->bits.reading != 0)
    {
        return true;
    }

    if (loader->bits.takesNoObjects == 0)
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

        resources->TakeObjects(chunk->objects);
    }

    if (reader != nullptr)
    {
        reader->Destroy(DestroyAndFree);
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
        chunk->holds--;
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

    ChunkDataReference* reference = entry->data;
    ChunkData* chunk = reference != nullptr ? reference->chunk : nullptr;
    return object->chunk == chunk;
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

    link->data.flags.linkedRm2Loaded = loaded;
}

void Rm2Loader::Unlink(GameChunkLink*)
{
}

namespace
{
// The most instances a chunk's search for an ID looks at (those with an object node)
constexpr u16 MostInstances = 0x400;

// Every element of a list destroyed (none skipped), then the list freed and forgotten (no list: nothing)
template <typename Destroy>
void DestroyList(PointerArray<void>** list, Destroy destroy)
{
    PointerArray<void>* array = *list;
    if (array == nullptr)
    {
        return;
    }

    for (s32 index = 0; index >= 0 && static_cast<u32>(index) < array->count; index++)
    {
        void* element = array->data[index];
        if (element != nullptr)
        {
            destroy(element);
        }
    }

    array = *list;
    if (array != nullptr)
    {
        if (array->data != nullptr)
        {
            MemoryDeallocate_(array->data);
        }

        MemoryDeallocate2_(array);
    }

    *list = nullptr;
}
}

void ChunkEntry::DestroyPositions()
{
    DestroyList(&positions, [](void* position) { static_cast<LayoutPosition*>(position)->Destroy(DestroyAndFree); });
}

void ChunkEntry::DestroyPaths()
{
    DestroyList(&paths, [](void* path) {
        auto* points = static_cast<PointList*>(path);
        CallVirtual<void>(points, points->vtable, PointList::DestroySlot, u32{DestroyAndFree});
    });
}

void ChunkEntry::Destroy(u32 destroyFlags)
{
    UnloadChunkEntry(this);
    if (savedFlags != nullptr)
    {
        savedFlags->Destroy(DestroyAndFree);
    }

    if (data != nullptr)
    {
        ReleaseChunkDataReference(&data);
    }

    StringDestroy(&path);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

InstanceContext* FindChunkInstance(ChunkEntry* chunk, u16 id)
{
    if (id == NoInstanceId)
    {
        return nullptr;
    }

    ChunkDataReference* reference = chunk->data;
    if (reference == nullptr || reference->chunk == nullptr)
    {
        return nullptr;
    }

    InstanceContext* found[MostInstances];
    InstanceQuery query;
    query.results = reinterpret_cast<void**>(found);
    query.count = 0;
    query.most = MostInstances;
    query.distance = Infinite;
    query.bits.value = InstanceQueryBits::AllWanted;
    query.wantedFlags = 0;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    u32 count = QueryChunkInstancesOfKinds(reference->chunk, ObjectNodeKinds, &query);
    for (u32 index = 0; index < count; index++)
    {
        auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&found[index]->nodes, NodeObject));
        if (node->agent->id == id)
        {
            return found[index];
        }
    }

    return nullptr;
}

ChunkEntry* ChunkManager::AddChunkEntry(const char* path, ChunkData* chunk)
{
    ChunkEntry* entry = FindChunkEntry(this, path);
    if (entry != nullptr)
    {
        AssignChunkData(&entry->data, chunk);
        return entry;
    }

    entry = static_cast<ChunkEntry*>(MemoryAllocate(sizeof(ChunkEntry)));
    u16 index = count;
    StringConstruct(&entry->path, path);
    entry->index = index;
    entry->manager = this;
    entry->data = chunk != nullptr ? AddChunkDataReference(chunk) : nullptr;
    entry->objects = nullptr;
    entries[count] = entry;
    count++;
    entry->savedFlags = nullptr;
    entry->unsavedFlags = nullptr;
    entry->nextFlagSlot = 0;
    entry->navigation = nullptr;
    entry->positions = nullptr;
    entry->positionCount = 0;
    entry->paths = nullptr;
    return entry;
}

void ReadChunkStates(ChunkManager* chunks, Stream* stream)
{
    UnloadAllChunks(chunks);
    u32 count;
    stream->ReadS32(reinterpret_cast<s32*>(&count));
    for (u32 index = 0; index < count; index++)
    {
        String path;
        path.string = nullptr;
        path.length = 0;
        path.capacity = 0;
        StringRead(&path, stream);
        ChunkEntry* chunk = chunks->AddChunk(path.string, nullptr);
        bool savedStore;
        stream->ReadBool(&savedStore);
        if (savedStore)
        {
            chunk->savedFlags->Read(stream);
        }

        StringDestroy(&path);
    }
}

void StopFilteredObjectNodes(ChunkManager*, const u32* filter)
{
    InstanceCollector collector;
    InstanceCollectorConstruct(&collector, filter[1], filter[2], 0);
    if (CollectChunksInstances(GetChunkList(), filter[0], &collector) != 0)
    {
        PoolsWalk<InstanceContext*> walk;
        ConstructPoolsWalk(&walk, g_InstancePoolsWalkVTable, g_InstanceWalkVTable, &collector.instances,
                           InstanceCollector::BothPools);
        for (walk.First(); !walk.AtEnd(); walk.Next())
        {
            auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&(*walk.Item())->nodes, NodeObject));
            if (node != nullptr)
            {
                CallVirtual<void>(node, node->vtable, ObjectNode::ReleasePartsSlot);
                CallVirtual<void>(node, node->vtable, ObjectNode::DoNothingSlot);
                CallVirtual<void>(node, node->vtable, ObjectNode::StopRunnersSlot, u32{1});
            }
        }

        walk.Destroy(DestroyOnly);
    }

    InstanceCollectorDestroy(&collector, DestroyOnly);
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

namespace
{
// A chunk's AI navigation's destructor
constexpr u32 NavigationDestroySlot = 1;
}

void UnloadChunkEntry(ChunkEntry* chunk)
{
    GameResources* resources = chunk->manager->resources;
    if (chunk->navigation != nullptr)
    {
        chunk->navigation->pathFinder->navigations[chunk->index] = nullptr;
        AiNavigation* navigation = chunk->navigation;
        if (navigation != nullptr)
        {
            CallVirtual<void>(navigation, navigation->vtable, NavigationDestroySlot, u32{DestroyAndFree});
        }

        chunk->navigation = nullptr;
    }

    chunk->DestroyPositions();
    chunk->DestroyPaths();
    PersistentFlags* unsavedFlags = chunk->unsavedFlags;
    if (unsavedFlags != nullptr)
    {
        unsavedFlags->Destroy(DestroyAndFree);
    }

    chunk->unsavedFlags = nullptr;
    if (chunk->objects != nullptr)
    {
        resources->ReleaseObjects(chunk->objects);
        MemoryDeallocate2_(chunk->objects);
        chunk->objects = nullptr;
    }

    AssignChunkData(&chunk->data, nullptr);
    chunk->nextFlagSlot = 0;
}

void SetUnsavedFlags(ChunkEntry* entry, PersistentFlags* flags)
{
    entry->unsavedFlags = flags;
}

AiPosition* NearestAiPosition(ChunkEntry* chunk, const Vector4* point, u16* index)
{
    AiNavigation* navigation = chunk->navigation;
    if (navigation == nullptr)
    {
        return nullptr;
    }

    if (index == nullptr)
    {
        return navigation->Nearest(point);
    }

    return navigation->NearestIndex(point, index);
}

AiPosition* NearestFlaggedAiPosition(ChunkEntry* chunk, const Vector4* point, u16* index, u32 required, u32 ruledOut)
{
    AiNavigation* navigation = chunk->navigation;
    if (navigation == nullptr)
    {
        return nullptr;
    }

    return navigation->NearestWithFlags(point, index, required & 0xFFFF, ruledOut & 0xFFFF);
}

ChunkEntry* ChunkManager::NewEntry(const char* path, ChunkData* chunk, void*, PersistentFlags* store)
{
    auto* entry = static_cast<ChunkEntry*>(MemoryAllocate(sizeof(ChunkEntry)));
    u16 index = count;
    StringConstruct(&entry->path, path);
    entry->index = index;
    entry->manager = this;
    entry->data = chunk != nullptr ? AddChunkDataReference(chunk) : nullptr;
    entry->savedFlags = store;
    entries[count] = entry;
    count++;
    entry->objects = nullptr;
    entry->unsavedFlags = nullptr;
    entry->nextFlagSlot = 0;
    entry->navigation = nullptr;
    entry->positions = nullptr;
    entry->positionCount = 0;
    entry->paths = nullptr;
    return entry;
}

u32 ChunkManager::RemoveChunkEntry(ChunkEntry* entry)
{
    UnloadChunkEntry(entry);
    return 1;
}

ChunkManager* ChunkManager::Construct(ChunkManager* manager, GameResources* resources, u32 queuesDefaultRm2, u32 takesNoObjects)
{
    manager->defaultReader = nullptr;
    manager->defaultObjects = nullptr;
    manager->pathFinder = nullptr;
    manager->resources = resources;
    manager->vtable = g_ChunkManagerBaseVTable;
    InstanceIds::Construct(&manager->instanceIds);
    g_ChunkManager = manager;
    RetailLibc::MemorySet(manager, 0, sizeof(manager->count) + sizeof(manager->flags));
    manager->flags.queuesDefaultRm2 = queuesDefaultRm2;
    manager->flags.takesNoObjects = takesNoObjects;
    g_InstanceIds = &manager->instanceIds;
    return manager;
}

void ChunkManager::Destroy(u32 destroyFlags)
{
    vtable = g_ChunkManagerBaseVTable;
    UnloadAllChunks(this);
    if (defaultReader != nullptr)
    {
        static_cast<ItemInterface*>(defaultReader)->Destroy(DestroyAndFree);
    }

    MemoryDeallocate2_(defaultObjects);
    instanceIds.Destroy(DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ChunkManager::LoadDefault(const char* path)
{
    ClearCounters();
    defaultObjects = static_cast<u32*>(MemoryAllocate(ObjectListSize));
    defaultObjects[0] = 0;
    auto* reader = static_cast<Rm2Reader*>(MemoryAllocate(sizeof(Rm2Reader)));
    defaultReader = Rm2Reader::ConstructDefault(reader, path, resources, defaultObjects);
    if (flags.queuesDefaultRm2 != 0)
    {
        SetUpDefaultParticles();
        defaultReader->Queue(true, true);
        InitShadows(0);
    }
    else
    {
        InitShadows(1);
        LoadParticles(g_DefaultParticlesFile);
        defaultReader->Queue(true, false);
    }

    if (flags.takesNoObjects == 0)
    {
        resources->TakeObjects(defaultObjects);
    }

    // The resources told they were read: the code models' slots set up again
    CallVirtual<void>(resources, resources->vtable, GameResources::SetUpCodeModelsSlot, 0u);
}

void ChunkManager::ClearCounters()
{
    for (s32& counter : counters)
    {
        counter = 0;
    }
}

ChunkEntry* FindChunkEntry(ChunkManager* chunks, const char* path)
{
    for (u32 index = 0; index < chunks->count; index++)
    {
        ChunkEntry* entry = chunks->entries[index];
        if (!StringNotEqual(&entry->path, path))
        {
            return entry;
        }
    }

    return nullptr;
}

ChunkEntry* ChunkOfIndex(void* manager, u16 index)
{
    auto* chunks = static_cast<ChunkManager*>(manager);
    u32 count = chunks->count;
    for (u32 i = 0; i < count; i++)
    {
        ChunkEntry* entry = chunks->entries[i];
        if (entry->index == index)
        {
            return entry;
        }
    }

    return nullptr;
}

ChunkEntry* ChunkOfInstance(void* chunks, InstanceContext* instance)
{
    auto* manager = static_cast<ChunkManager*>(chunks);
    u32 count = manager->count;
    ChunkData* chunk = instance->chunk;
    for (u32 index = 0; index < count; index++)
    {
        ChunkDataReference* reference = manager->entries[index]->data;
        if ((reference != nullptr ? reference->chunk : nullptr) == chunk)
        {
            return manager->entries[index];
        }
    }

    return nullptr;
}

void WriteChunkStates(ChunkManager* chunks, Stream* stream)
{
    u32 count = chunks->count;
    stream->WriteS32(static_cast<s32>(count));
    for (u32 index = 0; index < count; index++)
    {
        ChunkEntry* chunk = chunks->entries[index];
        StringWrite(&chunk->path, stream);
        bool savedStore = chunk->savedFlags != nullptr;
        stream->WriteBool(savedStore);
        if (savedStore)
        {
            chunk->savedFlags->Write(stream);
        }
    }
}

void ResetChunkInstances(ChunkManager* chunks, u32 entry, const u32* filter)
{
    StopFilteredObjectNodes(chunks, filter);
    ChunkListMakeGlobalWhere(GetChunkList(), entry, filter);
    FreeAll(entry);
}

void ResetChunks(ChunkManager* chunks, u32, u32 dropInstances)
{
    for (u32 index = 0; index < chunks->count; index++)
    {
        ChunkEntry* chunk = chunks->entries[index];
        if (chunk->unsavedFlags != nullptr)
        {
            chunk->unsavedFlags->Clear();
        }

        if (dropInstances != 0 && chunk->savedFlags != nullptr)
        {
            chunk->savedFlags->Clear();
        }
    }

    chunks->ClearCounters();
}

void SetGameCounter(void* manager, u32 counter, s32 value)
{
    static_cast<ChunkManager*>(manager)->counters[counter] = value;
}

void AddToGameCounter(void* manager, u32 counter, s32 value)
{
    static_cast<ChunkManager*>(manager)->counters[counter] += value;
}

s32 GameCounter(void* manager, u32 counter)
{
    return static_cast<ChunkManager*>(manager)->counters[counter];
}

void UnloadAllChunks(ChunkManager* chunks)
{
    for (u32 index = 0; index < chunks->count; index++)
    {
        ChunkEntry* entry = chunks->entries[index];
        if (entry != nullptr)
        {
            entry->Destroy(DestroyAndFree);
        }
    }

    chunks->count = 0;
}

PersistentFlags* PersistentFlags::ConstructBase(PersistentFlags* flags)
{
    flags->vtable = g_PersistentFlagsBaseVTable;
    return flags;
}

void PersistentFlags::BaseDestroy(u32 destroyFlags)
{
    vtable = g_PersistentFlagsBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PersistentFlags::Clear()
{
    u32 count = WordCount();
    u32* words = Words();
    for (u32 index = 0; index < count; index++)
    {
        words[index] = 0;
    }
}

void SetPersistentFlag(PersistentFlags* flags, u32 index, u32 value)
{
    if (index >= flags->Count())
    {
        return;
    }

    u32* words = flags->Words();
    u32 bit = 1u << (index & ShiftMask);
    if (value != 0)
    {
        words[index >> 5] |= bit;
    }
    else
    {
        words[index >> 5] &= ~bit;
    }
}

void TogglePersistentFlag(PersistentFlags* flags, u32 index)
{
    if (index >= flags->Count())
    {
        return;
    }

    flags->Words()[index >> 5] ^= 1u << (index & ShiftMask);
}

u32 GetPersistentFlag(PersistentFlags* flags, u32 index)
{
    if (index >= flags->Count())
    {
        return 0;
    }

    return (flags->WordsToRead()[index >> 5] & 1u << (index & ShiftMask)) != 0 ? 1 : 0;
}
