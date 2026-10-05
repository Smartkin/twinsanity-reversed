#include "game/disk.h"
#include "game/list.h"
#include "game/memory.h"
#include "platform/memory.h"
#include "retail/libc.h"

namespace
{
using FreeList = IntrusiveList<DiskNode, &DiskNode::previousFree, &DiskNode::nextFree>;
using PoolList = IntrusiveList<DiskNode, &DiskNode::previous, &DiskNode::next>;
using ReleaseList = IntrusiveList<DiskPendingRelease, &DiskPendingRelease::previous, &DiskPendingRelease::next>;

constexpr u32 BlockAlignment = 0x40;
// How much a step of a move copies
constexpr u32 StepCopy = 0x8000;
constexpr u32 ReleasesPerRound = 16;
constexpr s32 FreeIndex = -2;
constexpr s32 FirstNodeIndex = -1;
// DiskQueueRelease's count that keeps the node's
constexpr u32 KeepWaitCount = 0xFFFFFFFF;
// The states from DiskNodeMovedAway on that wait for rounds of releases
constexpr u32 ReleaseStateCount = DiskNodeReleasedExpired - DiskNodeMovedAway + 1;

// A new node's bits: free and ready, what the allocator left in the unused ones kept
DiskNodeBits NewNodeBits(DiskNodeBits leftover)
{
    leftover.waitCount = 0;
    leftover.ready = 1;
    leftover.loadState = DiskNotReadInto;
    leftover.deferRelease = 0;
    leftover.state = DiskNodeFree;
    return leftover;
}

// A free node taken: used and ready, its count kept
void Take(DiskNode* node)
{
    node->bits.ready = 1;
    node->bits.loadState = DiskNotReadInto;
    node->bits.deferRelease = 0;
    node->bits.state = DiskNodeUsed;
}

// A used node the hardware isn't reading into
bool CanMove(const DiskNode* node)
{
    return node->bits.state == DiskNodeUsed && node->bits.loadState != DiskLoading;
}

void TrackLowestFree(DiskManager* manager, DiskNode* node)
{
    if (manager->lowestFree == nullptr || node->memory < manager->lowestFree->memory)
    {
        manager->lowestFree = node;
    }
}

DiskNode* NewFreeNode()
{
    DiskNode* node = static_cast<DiskNode*>(MemoryAllocate(sizeof(DiskNode)));
    DiskNodeBits leftover = node->bits;
    node->index = FirstNodeIndex;
    node->memory = nullptr;
    node->size = 0;
    node->previousFree = nullptr;
    node->nextFree = nullptr;
    node->previous = nullptr;
    node->next = nullptr;
    node->bits = NewNodeBits(leftover);
    return node;
}

void ClearMove(DiskMove* move)
{
    move->active = 0;
    move->source = nullptr;
    move->destination = nullptr;
    move->copied = 0;
    move->size = 0;
}
}

extern "C"
{
    // What the moves copied, all told
    extern u32 g_DiskBytesMoved RETAIL(D_0030A094);

    // The size classes of the free lists. Class 10 is never used: the retail code tells it from 11 by size < 0x100, where the
    // size is at least 0x580
    u32 DiskSizeClass(u32 size) RETAIL(GetReadTableIndex);
    void DiskPushFree(DiskManager* manager, DiskNode* node) RETAIL(FUN_00205750);
    void DiskInsertFreeAfter(DiskManager* manager, DiskNode* node, DiskNode* after, DiskNode** list) RETAIL(FUN_002057d0);
    void DiskInsertFreeBefore(DiskManager* manager, DiskNode* node, DiskNode* before, DiskNode** list) RETAIL(FUN_00205840);
    // Into its size class's list, by size
    void DiskAddFree(DiskManager* manager, DiskNode* node) RETAIL(FUN_00205688);
    void DiskRemoveFree(DiskManager* manager, DiskNode* node) RETAIL(FUN_002058b0);
    // Joins a free node with the free nodes next to it
    void DiskMerge(DiskManager* manager, DiskNode* node) RETAIL(FUN_00203250);
    // The first free node of the size or more, by size class and then by the lists' order, split to the size. It gets a handle
    DiskNode* DiskAllocateNode(DiskManager* manager, u32 size) RETAIL(FUN_00203368);
    // Frees the node and its handle
    void DiskReleaseNode(DiskManager* manager, DiskNode* node) RETAIL(FUN_002039c0);
    void DiskReleaseNow(DiskManager* manager, const s32* handle) RETAIL(FUN_002035b8);
    void DiskReleaseDeferred(DiskManager* manager, const s32* handle) RETAIL(FUN_00203680);
    // Sets the state, and waits for a round of releases when it's one of the release states. KeepWaitCount keeps the count
    void DiskQueueRelease(DiskManager* manager, DiskNode* node, u32 state, u32 count) RETAIL(FUN_00205968);

    void DiskStartMove(DiskMove* move, DiskNode* source, DiskNode* destination) RETAIL(FUN_00202cf0);
    // Moves the first movable node after the lowest free node into it. When it doesn't fit, the node right after the free one
    // goes to a free node past it instead, making the free one bigger
    bool DiskFindMove(DiskMove* move) RETAIL(FUN_00202ed8);
    void DiskFinishMove(DiskMove* move) RETAIL(FUN_00203098);
    bool DiskStepMove(DiskMove* move) RETAIL(FUN_00203168);

    u32 DiskSizeClass(u32 size)
    {
        if (size < 0x380)
        {
            if (size < 0x140)
            {
                if (size < 0xC0)
                {
                    return size < 0x80 ? 0 : 1;
                }

                return size < 0x100 ? 2 : 3;
            }

            if (size < 0x240)
            {
                return size < 0x1C0 ? 4 : 5;
            }

            return size < 0x300 ? 6 : 7;
        }

        if (size < 0xB80)
        {
            if (size < 0x580)
            {
                return size < 0x480 ? 8 : 9;
            }

            return 11;
        }

        if (size < 0x4000)
        {
            return size < 0x118C ? 12 : 13;
        }

        return size <= 0x3FFFF ? 14 : 15;
    }

    void DiskPushFree(DiskManager* manager, DiskNode* node)
    {
        u32 sizeClass = DiskSizeClass(node->size);
        manager->freeCount++;
        FreeList::PushFront(node, &manager->freeLists[sizeClass]);
        TrackLowestFree(manager, node);
    }

    void DiskInsertFreeAfter(DiskManager* manager, DiskNode* node, DiskNode* after, DiskNode**)
    {
        manager->freeCount++;
        FreeList::InsertAfter(node, after);
        TrackLowestFree(manager, node);
    }

    void DiskInsertFreeBefore(DiskManager* manager, DiskNode* node, DiskNode* before, DiskNode** list)
    {
        manager->freeCount++;
        FreeList::InsertBefore(node, list, before);
        TrackLowestFree(manager, node);
    }

    void DiskAddFree(DiskManager* manager, DiskNode* node)
    {
        DiskNode** list = &manager->freeLists[DiskSizeClass(node->size)];
        DiskNode* after = nullptr;
        for (DiskNode* free = *list; free != nullptr; free = free->nextFree)
        {
            if (free->size > node->size)
            {
                DiskInsertFreeBefore(manager, node, free, list);
                return;
            }

            after = free;
        }

        if (after == nullptr)
        {
            DiskPushFree(manager, node);
        }
        else
        {
            DiskInsertFreeAfter(manager, node, after, list);
        }
    }

    void DiskRemoveFree(DiskManager* manager, DiskNode* node)
    {
        manager->freeCount--;
        u32 sizeClass = DiskSizeClass(node->size);
        FreeList::Remove(node, &manager->freeLists[sizeClass]);
        if (node != manager->lowestFree)
        {
            return;
        }

        manager->lowestFree = nullptr;
        for (DiskNode* free = manager->freeLists[sizeClass]; free != nullptr; free = free->nextFree)
        {
            TrackLowestFree(manager, free);
        }
    }

    void DiskMerge(DiskManager* manager, DiskNode* node)
    {
        DiskNode* merged = node;
        DiskNode* previous = node->previous;
        if (previous != nullptr && previous->bits.state == DiskNodeFree)
        {
            DiskRemoveFree(manager, previous);
            previous->size += node->size;
            DiskAddFree(manager, previous);
            DiskRemoveFree(manager, node);
            PoolList::Remove(node, &manager->nodes);
            merged = previous;
            MemoryDeallocate2_(node);
        }

        DiskNode* next = merged->next;
        if (next != nullptr && next->bits.state == DiskNodeFree)
        {
            DiskRemoveFree(manager, merged);
            merged->size += next->size;
            DiskAddFree(manager, merged);
            DiskRemoveFree(manager, next);
            PoolList::Remove(next, &manager->nodes);
            MemoryDeallocate2_(next);
        }
    }

    DiskNode* DiskAllocateNode(DiskManager* manager, u32 size)
    {
        DiskNode* node = nullptr;
        for (u32 sizeClass = DiskSizeClass(size); sizeClass < DiskSizeClasses && node == nullptr; sizeClass++)
        {
            for (DiskNode* free = manager->freeLists[sizeClass]; free != nullptr; free = free->nextFree)
            {
                if (size <= free->size)
                {
                    node = free;
                    break;
                }
            }
        }

        if (node == nullptr)
        {
            return nullptr;
        }

        u32 available = node->size;
        if (available == size)
        {
            Take(node);
            DiskRemoveFree(manager, node);
        }
        else
        {
            DiskRemoveFree(manager, node);
            node->size = size;
            Take(node);
        }

        s32 index = manager->freeHandles[manager->handlesUsed++];
        node->index = index;
        manager->handles[index] = node;
        if (available != size)
        {
            DiskNode* rest = NewFreeNode();
            rest->size = available - size;
            rest->memory = node->memory + size;
            rest->index = FreeIndex;
            DiskAddFree(manager, rest);
            PoolList::InsertAfter(rest, node);
        }

        return node;
    }

    void DiskReleaseNode(DiskManager* manager, DiskNode* node)
    {
        if (node->bits.state != DiskNodeFree)
        {
            node->bits.state = DiskNodeFree;
            DiskAddFree(manager, node);
            if (node->index >= 0)
            {
                manager->handlesUsed--;
                manager->freeHandles[manager->handlesUsed] = static_cast<s16>(node->index);
            }
        }

        node->index = FreeIndex;
        DiskMerge(manager, node);
    }

    // A node being moved takes the move down with it
    void DiskReleaseNow(DiskManager* manager, const s32* handle)
    {
        DiskNode* node = manager->handles[*handle];
        DiskMove* move = manager->move;
        switch (node->bits.state)
        {
        case DiskNodeUsed:
            if (node != move->destination)
            {
                DiskReleaseNode(manager, node);
                return;
            }

            break;
        case DiskNodeMoving:
            break;
        default:
            return;
        }

        DiskReleaseNode(move->manager, move->source);
        DiskReleaseNode(move->manager, move->destination);
        ClearMove(move);
    }

    void DiskReleaseDeferred(DiskManager* manager, const s32* handle)
    {
        DiskNode* node = manager->handles[*handle];
        DiskMove* move = manager->move;
        switch (node->bits.state)
        {
        case DiskNodeUsed:
            if (node != move->destination)
            {
                DiskQueueRelease(manager, node, DiskNodeReleased, 0);
                return;
            }

            DiskQueueRelease(manager, move->source, DiskNodeReleased, 0);
            node = move->destination;
            break;
        case DiskNodeMoving:
            DiskReleaseNode(manager, move->destination);
            break;
        default:
            return;
        }

        DiskQueueRelease(manager, node, DiskNodeReleased, 0);
        ClearMove(move);
    }

    void DiskQueueRelease(DiskManager* manager, DiskNode* node, u32 state, u32 count)
    {
        node->bits.state = state;
        if (state - DiskNodeMovedAway < ReleaseStateCount)
        {
            DiskPendingRelease* pending = static_cast<DiskPendingRelease*>(MemoryAllocate(sizeof(DiskPendingRelease)));
            pending->node = node;
            pending->next = nullptr;
            pending->previous = nullptr;
            ReleaseList::PushFront(pending, &manager->pendingReleases);
            manager->pendingReleaseCount++;
        }

        if (count != KeepWaitCount)
        {
            node->bits.waitCount = count;
        }
    }

    void DiskStartMove(DiskMove* move, DiskNode* source, DiskNode* destination)
    {
        move->source = source;
        source->bits.state = DiskNodeMoving;
        move->size = source->size;
        move->copied = 0;
        u32 available = destination->size;
        move->destination = destination;
        DiskRemoveFree(move->manager, destination);
        Take(destination);
        destination->size = move->size;
        destination->bits.state = DiskNodeMoveDestination;
        destination->bits.ready = 1;
        destination->bits.deferRelease = move->source->bits.deferRelease;
        destination->bits.loadState = move->source->bits.loadState;
        if (available != move->size)
        {
            DiskNode* rest = static_cast<DiskNode*>(MemoryAllocate(sizeof(DiskNode)));
            rest->memory = destination->memory + move->size;
            rest->size = available - move->size;
            rest->index = FreeIndex;
            rest->bits = NewNodeBits(rest->bits);
            rest->previousFree = nullptr;
            rest->nextFree = nullptr;
            rest->previous = nullptr;
            rest->next = nullptr;
            DiskAddFree(move->manager, rest);
            PoolList::InsertAfter(rest, destination);
            DiskMerge(move->manager, rest);
        }

        move->active = 1;
    }

    bool DiskFindMove(DiskMove* move)
    {
        DiskManager* manager = move->manager;
        DiskNode* lowest = nullptr;
        for (u32 sizeClass = 0; sizeClass < DiskSizeClasses; sizeClass++)
        {
            for (DiskNode* free = manager->freeLists[sizeClass]; free != nullptr; free = free->nextFree)
            {
                if (lowest == nullptr || free->memory < lowest->memory)
                {
                    lowest = free;
                }
            }
        }

        if (lowest == nullptr)
        {
            return false;
        }

        DiskNode* first = lowest->next;
        for (DiskNode* node = first; node != nullptr; node = node->next)
        {
            if (!CanMove(node))
            {
                continue;
            }

            if (node->size <= lowest->size)
            {
                DiskStartMove(move, node, lowest);
                return true;
            }

            if (!CanMove(first))
            {
                return false;
            }

            for (u32 sizeClass = DiskSizeClass(first->size); sizeClass < DiskSizeClasses; sizeClass++)
            {
                for (DiskNode* free = manager->freeLists[sizeClass]; free != nullptr; free = free->nextFree)
                {
                    if (free->memory >= first->memory && first->size <= free->size)
                    {
                        DiskStartMove(move, first, free);
                        return true;
                    }
                }
            }

            return false;
        }

        return false;
    }

    void DiskFinishMove(DiskMove* move)
    {
        move->destination->index = move->source->index;
        move->destination->bits.state = DiskNodeUsed;
        move->manager->handles[move->destination->index] = move->destination;
        move->source->index = FreeIndex;
        DiskNode* source = move->source;
        if (!source->bits.deferRelease)
        {
            source->bits.state = DiskNodeFree;
            DiskAddFree(move->manager, move->source);
            DiskMerge(move->manager, move->source);
        }
        else
        {
            DiskQueueRelease(move->manager, source, DiskNodeMovedAway, 0);
        }

        ClearMove(move);
    }

    bool DiskStepMove(DiskMove* move)
    {
        if (move->active == 0)
        {
            if (!DiskFindMove(move))
            {
                return false;
            }

            move->active = 1;
            return true;
        }

        if (move->active != 1)
        {
            return false;
        }

        u32 copied = move->copied;
        u32 chunk = move->size - copied;
        if (chunk > StepCopy)
        {
            chunk = StepCopy;
        }

        RetailLibc::MemoryCopy(move->destination->memory + copied, move->source->memory + copied, chunk);
        g_DiskBytesMoved += chunk;
        move->copied = copied + chunk;
        if (move->copied == move->size)
        {
            DiskFinishMove(move);
        }

        return true;
    }

    DiskManager* DiskManagerConstruct(DiskManager* manager)
    {
        manager->pool = nullptr;
        manager->poolSize = 0;
        manager->lowestFree = nullptr;
        manager->freeCount = 0;
        manager->nodes = nullptr;
        manager->poolEnd = nullptr;
        for (u32 i = 0; i < DiskHandles; i++)
        {
            manager->handles[i] = nullptr;
            manager->freeHandles[i] = static_cast<s16>(i);
        }

        manager->handlesUsed = 0;
        manager->pendingReleases = nullptr;
        manager->pendingReleaseCount = 0;
        DiskMove* move = static_cast<DiskMove*>(MemoryAllocate(sizeof(DiskMove)));
        manager->move = move;
        move->source = nullptr;
        move->destination = nullptr;
        move->copied = 0;
        move->size = 0;
        move->active = 0;
        move->manager = manager;
        for (u32 i = 0; i < DiskSizeClasses; i++)
        {
            manager->freeLists[i] = nullptr;
        }

        return manager;
    }

    // Leaves the nodes: the pool goes with the program
    void DiskManagerDestroy(DiskManager* manager, u32 flags)
    {
        if (manager->move != nullptr)
        {
            MemoryDeallocate2_(manager->move);
        }

        if ((flags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(manager);
        }
    }

    // The pool starts at the next 64 bytes, and holds a multiple of them
    void DiskManagerSetPool(DiskManager* manager, void* pool, u32 size)
    {
        u8* memory = static_cast<u8*>(pool);
        u8* aligned = reinterpret_cast<u8*>((reinterpret_cast<u32>(memory) + BlockAlignment - 1) & ~(BlockAlignment - 1));
        manager->poolSize = (size - static_cast<u32>(aligned - memory)) & ~(BlockAlignment - 1);
        manager->pool = aligned;
        DiskNode* node = NewFreeNode();
        node->memory = manager->pool;
        node->size = manager->poolSize;
        DiskPushFree(manager, node);
        PoolList::PushFront(node, &manager->nodes);
        manager->poolEnd = manager->pool + manager->poolSize;
    }

    s32* DiskAllocate(s32* handle, DiskManager* manager, u32 size, bool readInto, u32 deferRelease)
    {
        DiskNode* node = DiskAllocateNode(manager, (size + BlockAlignment - 1) & ~(BlockAlignment - 1));
        if (readInto)
        {
            Platform::Memory::BeforeDeviceWrite(node->memory, node->size);
            node->bits.ready = 0;
            node->bits.loadState = DiskLoading;
        }

        node->bits.deferRelease = deferRelease;
        *handle = node->index;
        return handle;
    }

    void DiskRelease(DiskManager* manager, const s32* handle)
    {
        if (!manager->handles[*handle]->bits.deferRelease)
        {
            DiskReleaseNow(manager, handle);
        }
        else
        {
            DiskReleaseDeferred(manager, handle);
        }
    }

    u8* DiskMemory(DiskManager* manager, const s32* handle)
    {
        return manager->handles[*handle]->memory;
    }

    void DiskMarkLoaded(DiskManager* manager, const s32* handle)
    {
        DiskNode* node = manager->handles[*handle];
        node->bits.loadState = DiskLoaded;
    }

    u8* DiskLoadedMemory(DiskManager* manager, const s32* handle)
    {
        DiskNode* node = manager->handles[*handle];
        if (!DiskNodeIsLoaded(node))
        {
            return nullptr;
        }

        node->bits.ready = 1;
        return node->memory;
    }

    bool DiskNodeIsLoaded(const DiskNode* node)
    {
        if (node->bits.state == 0)
        {
            return false;
        }

        if (node->bits.ready)
        {
            return true;
        }

        return node->bits.loadState == DiskLoaded;
    }

    bool DiskCompactStep(DiskManager* manager, bool immediately)
    {
        DiskMove* move = manager->move;
        if (!immediately)
        {
            return DiskStepMove(move);
        }

        for (DiskNode* node = move->manager->nodes; node != nullptr; node = node->next)
        {
            if (CanMove(node) && node->previous != nullptr && node->previous->bits.state == DiskNodeFree)
            {
                DiskStartMove(move, node, node->previous);
                // Into the free node right before it: the two may overlap
                RetailLibc::MemoryMove(move->destination->memory, move->source->memory, move->size);
                DiskFinishMove(move);
                return true;
            }
        }

        return false;
    }

    void DiskCompactAll(DiskManager* manager)
    {
        while (true)
        {
            DiskMove* move = manager->move;
            bool moved = false;
            for (DiskNode* node = move->manager->nodes; node != nullptr; node = node->next)
            {
                if (CanMove(node) && node->previous != nullptr && node->previous->bits.state == DiskNodeFree)
                {
                    DiskStartMove(move, node, node->previous);
                    RetailLibc::MemoryMove(move->destination->memory, move->source->memory, move->size);
                    DiskFinishMove(move);
                    moved = true;
                    break;
                }
            }

            if (!moved && manager->pendingReleaseCount == 0)
            {
                return;
            }

            DiskProcessReleases(manager);
        }
    }

    void DiskProcessReleases(DiskManager* manager)
    {
        s32 count = manager->pendingReleaseCount;
        s32 round = count <= static_cast<s32>(ReleasesPerRound) ? count : static_cast<s32>(ReleasesPerRound);
        if (round > 0)
        {
            manager->pendingReleaseCount = count - round;
        }

        DiskPendingRelease* pending = manager->pendingReleases;
        for (s32 i = 0; i < round; i++)
        {
            DiskPendingRelease* next = pending->next;
            DiskNode* node = pending->node;
            ReleaseList::Remove(pending, &manager->pendingReleases);
            MemoryDeallocate2_(pending);
            u32 state = node->bits.state;
            switch (state)
            {
            case DiskNodeMovedAway:
            case DiskNodeReleased:
            {
                u32 rounds = node->bits.waitCount + 1;
                if (rounds == 1)
                {
                    DiskQueueRelease(manager, node, state + 1, 0);
                }
                else
                {
                    DiskQueueRelease(manager, node, state, rounds);
                }

                break;
            }
            case DiskNodeMovedAwayExpired:
                node->bits.state = DiskNodeFree;
                DiskAddFree(manager, node);
                DiskMerge(manager, node);
                break;
            case DiskNodeReleasedExpired:
                DiskReleaseNode(manager, node);
                break;
            default:
                break;
            }

            pending = next;
        }
    }

    void DiskNothing()
    {
    }
}
