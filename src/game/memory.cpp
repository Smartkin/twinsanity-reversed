#include "game/memory.h"
#include "game/disk.h"
#include "gcc2.h"
#include "platform/memory.h"
#include "retail/libc.h"

namespace
{
constexpr u32 FreeBit = 0x80000000;
constexpr u32 SizeMask = 0x7FFFFFFF;
constexpr u32 PageSize = 0x1000;
// A page's share of the pages' memory: its node, its entries and its size class
constexpr u32 PageCost = sizeof(FixedAllocatorNode) + PageSize + sizeof(u16);
// MemoryAllocateAligned leaves it in front of an aligned allocation, with the block's own address after it
constexpr u32 AlignedMarker = 0xFEDCBA98;
constexpr s32 NoFit = -1;
constexpr s32 WorstFit = 1000000000;
// The disk manager's pool; the heap manager's takes what the platform has left after it
constexpr u32 DiskPoolSize = 0x10A3D70;

// The entries' sizes: 4 bytes apart up to 148, then fewer the bigger they get
constexpr u32 SizeClassSizes[SizeClassCount] = {
    4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  60,   64,   68,   72,   76,   80,  84,
    88,  92,  96,  100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,  148,  156,  160,  168,  176, 184,
    192, 204, 208, 224, 240, 256, 272, 288, 304, 336, 368, 400, 448, 512, 576,  672,  816,  1024, 1360, 2048, 4096,
};
constexpr u32 EvenSizeClasses = 37;

u8* Bytes(void* memory)
{
    return static_cast<u8*>(memory);
}

HeapBlock* BlockAt(void* memory)
{
    return static_cast<HeapBlock*>(memory);
}

// The block after it in memory
HeapBlock* After(HeapBlock* block, u32 size)
{
    return BlockAt(Bytes(block + 1) + size);
}

void UnlinkFree(HeapManager* heap, HeapBlock* block)
{
    if (block->previous == nullptr)
    {
        if (block->next == nullptr)
        {
            heap->freeList = nullptr;
        }
        else
        {
            block->next->previous = nullptr;
            heap->freeList = block->next;
        }
    }
    else if (block->next == nullptr)
    {
        block->previous->next = nullptr;
    }
    else
    {
        block->previous->next = block->next;
        block->next->previous = block->previous;
    }

    block->next = nullptr;
    block->previous = nullptr;
}

void PushFree(HeapManager* heap, HeapBlock* block)
{
    if (heap->freeList == nullptr)
    {
        heap->freeList = block;
        block->previous = nullptr;
        block->next = nullptr;
    }
    else
    {
        heap->freeList->previous = block;
        block->next = heap->freeList;
        heap->freeList = block;
    }
}

void CountTaken(HeapManager* heap, u32 bytes)
{
    heap->freeBytes -= bytes;
    heap->usedBytes += bytes;
    if (heap->peakUsedBytes < heap->usedBytes)
    {
        heap->peakUsedBytes = heap->usedBytes;
    }
}

void*& Field(void* object, u32 memberPointer)
{
    return *reinterpret_cast<void**>(Bytes(object) + memberPointer - 1);
}

constexpr u32 NewerField = GCC2_MEMBER_POINTER(FixedAllocatorNode, newer);
constexpr u32 OlderField = GCC2_MEMBER_POINTER(FixedAllocatorNode, older);
}

extern "C"
{
    extern HeapManager g_HeapManager RETAIL(D_0031B338);
    extern s32 g_HeapManagerConstructed RETAIL(D_0030A7BC);
    // Whether the heap manager's and the disk manager's pools are allocated
    extern bool g_PoolsAllocated[2] RETAIL(D_00309B20);
    extern void* g_HeapPool RETAIL(D_00309B18);
    extern DiskManager g_DiskManager RETAIL(D_0031B4B8);
    extern s32 g_DiskManagerConstructed RETAIL(D_0030A7C0);
    extern void* g_DiskPool RETAIL(D_00309B1C);

    // The retail names of this file's functions
    void UnlinkFromChain(void* node, void** head, u32 newerField, u32 olderField) RETAIL(FUN_00205f70);
    void MoveToChainFront(void* node, void** head) RETAIL(FUN_002055a8);
    FixedAllocatorNode* InitRootNode(FixedAllocatorNode* node) RETAIL(InitRootNode_);
    void DestroyAllocatorNode(FixedAllocatorNode* node, u32 flags) RETAIL(FUN_002054b0);
    MemoryController* InitMemController(MemoryController* small) RETAIL(InitMemController_);
    void* CacheAlloc(MemoryController* small, u32 size) RETAIL(CacheAlloc_);
    void FreeCache(MemoryController* small, void* memory) RETAIL(FreeCache_);
    void DestroyMemoryController(MemoryController* small, u32 flags) RETAIL(FUN_002052f0);
    u32 BlockSize(HeapBlock* block) RETAIL(FUN_00204e98);
    void InitHeapManager(HeapManager* heap) RETAIL(InitHeapManager_);
    HeapManager* CreateHeapManager(HeapManager* heap) RETAIL(CreateHeapManager_);
    void DestroyHeapManager(HeapManager* heap, u32 flags) RETAIL(FUN_00204ef8);
    void InitHeap(HeapManager* heap, u8* memory, u32 size, u32 unknown) RETAIL(FUN_00204f60);
    void SetHeapPool(HeapManager* heap, u8* memory, u32 size) RETAIL(FUN_00204c00);
    s32 BlockFitSlack(HeapManager*, HeapBlock* block, u32 size) RETAIL(FUN_00205158);
    void* TakeBlock(HeapManager* heap, HeapBlock* block, u32 size) RETAIL(FUN_002027c8);
    void* HeapBestFit(HeapManager* heap, HeapBlock* block, u32 size) RETAIL(FUN_00205090);
    void* HeapAlloc(HeapManager* heap, u32 size) RETAIL(HeapAlloc_);
    s32 MergeFreeBlock(HeapManager* heap, HeapBlock* block) RETAIL(FUN_002025b8);
    void FreeHeap(HeapManager* heap, void* memory) RETAIL(FreeHeap_);
    void DestroyTheHeapManager() RETAIL(FUN_00182420);

    // The retail code's intrusive list templates, GCC 2.9x's: the node's fields come as member pointers. The list's head is its
    // newest node, each node's older field leads to the one before
    void StoreInCacheChain(void* node, void** head, u32 newerField, u32 olderField)
    {
        if (*head == nullptr)
        {
            *head = node;
            Field(node, newerField) = nullptr;
            Field(node, olderField) = nullptr;
            return;
        }

        Field(*head, newerField) = node;
        Field(node, olderField) = *head;
        *head = node;
    }

void UnlinkFromChain(void* node, void** head, u32 newerField, u32 olderField)
    {
        void* newer = Field(node, newerField);
        void* older = Field(node, olderField);
        if (newer == nullptr)
        {
            if (older == nullptr)
            {
                *head = nullptr;
            }
            else
            {
                Field(older, newerField) = nullptr;
                *head = older;
            }
        }
        else if (older == nullptr)
        {
            Field(newer, olderField) = nullptr;
        }
        else
        {
            Field(newer, olderField) = older;
            Field(older, newerField) = newer;
        }

        Field(node, olderField) = nullptr;
        Field(node, newerField) = nullptr;
    }

void MoveToChainFront(void* node, void** head)
    {
        UnlinkFromChain(node, head, NewerField, OlderField);
        StoreInCacheChain(node, head, NewerField, OlderField);
    }

    // The fixed size entries

FixedAllocatorNode* InitRootNode(FixedAllocatorNode* node)
    {
        node->allocator.memory = nullptr;
        node->allocator.size = 0;
        node->allocator.entrySize = 0;
        node->allocator.nextFree = nullptr;
        node->allocator.freeCount = 0;
        node->newer = nullptr;
        node->older = nullptr;
        return node;
    }

    void AllocateMemory(FixedAllocatorNode* node, u32 size, u32 entrySize, void* memory)
    {
        u32 capacity = size / entrySize;
        node->allocator.size = size;
        node->allocator.entrySize = entrySize;
        node->allocator.memory = memory;
        node->newer = nullptr;
        node->older = nullptr;
        node->allocator.freeCount = capacity;
        node->allocator.capacity = capacity;
        if (capacity != 0)
        {
            void* entry = memory;
            for (u32 i = 0; i < node->allocator.freeCount; i++)
            {
                void* next = i < node->allocator.freeCount - 1 ? Bytes(entry) + node->allocator.entrySize : nullptr;
                *static_cast<void**>(entry) = next;
                entry = next;
            }
        }

        node->allocator.nextFree = node->allocator.memory;
    }

    void* GetAvailableAddress(FixedAllocatorNode* node)
    {
        u32 freeCount = node->allocator.freeCount;
        if (freeCount == 0)
        {
            return nullptr;
        }

        void* entry = node->allocator.nextFree;
        node->allocator.freeCount = freeCount - 1;
        node->allocator.nextFree = *static_cast<void**>(entry);
        return entry;
    }

    void FreeAddress(FixedAllocatorNode* node, void* entry)
    {
        if (entry == nullptr)
        {
            return;
        }

        void* next = node->allocator.nextFree;
        node->allocator.nextFree = entry;
        *static_cast<void**>(entry) = next;
        node->allocator.freeCount++;
    }

void DestroyAllocatorNode(FixedAllocatorNode* node, u32 flags)
    {
        if (flags & 1)
        {
            MemoryDeallocate2_(node);
        }
    }

    // The small allocations

    // The first size class that fits a size (a multiple of 4)
    u32 GetIndexBasedOnAllocSize(u32 size)
    {
        if (size <= SizeClassSizes[EvenSizeClasses - 1])
        {
            return (size - 1) >> 2;
        }

        u32 low = EvenSizeClasses;
        u32 high = SizeClassCount - 1;
        while (low < high)
        {
            u32 middle = (low + high) / 2;
            if (SizeClassSizes[middle] < size)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        return low;
    }

MemoryController* InitMemController(MemoryController* small)
    {
        InitRootNode(&small->freeNodes);
        return small;
    }

    void InitAllocatorCache(MemoryController* small, u8* memory, u32 size)
    {
        u32 pageCount = size / PageCost;
        small->nodes = reinterpret_cast<FixedAllocatorNode*>(memory);
        small->pageCount = pageCount;
        small->pages = memory + pageCount * sizeof(FixedAllocatorNode);
        small->pageSizeClasses = reinterpret_cast<u16*>(small->pages + pageCount * PageSize);
        AllocateMemory(&small->freeNodes, pageCount * sizeof(FixedAllocatorNode), sizeof(FixedAllocatorNode), memory);
        for (u32 i = 0; i < SizeClassCount; i++)
        {
            small->sizeClasses[i] = nullptr;
        }
    }

void* CacheAlloc(MemoryController* small, u32 size)
    {
        if (size == 0)
        {
            return nullptr;
        }

        u32 sizeClass = GetIndexBasedOnAllocSize((size + 3) & ~3u);
        for (FixedAllocatorNode* node = small->sizeClasses[sizeClass]; node != nullptr; node = node->older)
        {
            if (void* entry = GetAvailableAddress(node))
            {
                return entry;
            }
        }

        // A new page for the size
        FixedAllocatorNode* node = static_cast<FixedAllocatorNode*>(GetAvailableAddress(&small->freeNodes));
        if (node == nullptr)
        {
            return nullptr;
        }

        s32 page = node - small->nodes;
        small->pageSizeClasses[page] = sizeClass;
        AllocateMemory(node, PageSize, SizeClassSizes[sizeClass], small->pages + page * PageSize);
        StoreInCacheChain(node, reinterpret_cast<void**>(&small->sizeClasses[sizeClass]), NewerField, OlderField);
        return GetAvailableAddress(node);
    }

void FreeCache(MemoryController* small, void* memory)
    {
        u32 page = static_cast<u32>(Bytes(memory) - small->pages) >> 12;
        FixedAllocatorNode* node = small->nodes + page;
        FreeAddress(node, memory);
        void** sizeClass = reinterpret_cast<void**>(&small->sizeClasses[small->pageSizeClasses[page]]);
        if (node->allocator.capacity == node->allocator.freeCount)
        {
            // All free: the page goes back
            FreeAddress(&small->freeNodes, node);
            UnlinkFromChain(node, sizeClass, NewerField, OlderField);
        }
        else if (node != *sizeClass)
        {
            MoveToChainFront(node, sizeClass);
        }
    }

void DestroyMemoryController(MemoryController* small, u32 flags)
    {
        DestroyAllocatorNode(&small->freeNodes, DestroyOnly);
        if (flags & 1)
        {
            MemoryDeallocate2_(small);
        }
    }

    // The heap

u32 BlockSize(HeapBlock* block)
    {
        return block->size & SizeMask;
    }

void InitHeapManager(HeapManager* heap)
    {
        heap->unknown3C = 0;
        heap->end = nullptr;
        heap->first = nullptr;
        heap->recentFree = nullptr;
        heap->freeList = nullptr;
        heap->usedBlocks = 0;
        heap->freeBlocks = 0;
        heap->usedBytes = 0;
        heap->freeBytes = 0;
        heap->searchSteps = 0;
        heap->allocations = 0;
        heap->ready = 0;
        heap->peakUsedBytes = 0;
    }

HeapManager* CreateHeapManager(HeapManager* heap)
    {
        InitHeapManager(heap);
        return heap;
    }

void DestroyHeapManager(HeapManager* heap, u32 flags)
    {
        if (flags & 1)
        {
            MemoryDeallocate2_(heap);
        }
    }

    // One free block of all the memory
void InitHeap(HeapManager* heap, u8* memory, u32 size, u32 unknown)
    {
        HeapBlock* first = BlockAt(reinterpret_cast<u8*>((reinterpret_cast<u32>(memory) + 15) & ~15u));
        first->next = nullptr;
        first->previous = nullptr;
        first->before = nullptr;
        first->size = (first->size & FreeBit) | ((size - (Bytes(first) - memory + sizeof(HeapBlock))) & ~15u);
        first->size = BlockSize(first) | FreeBit;
        heap->poolStart = memory;
        heap->poolSize = size;
        heap->recentFree = first;
        heap->freeList = first;
        heap->first = first;
        heap->end = nullptr;
        heap->usedBlocks = 0;
        heap->freeBlocks = 0;
        heap->usedBytes = 0;
        heap->freeBytes = 0;
        heap->searchSteps = 0;
        heap->allocations = 0;
        heap->ready = 0;
        heap->peakUsedBytes = 0;
        heap->unknown3C = 0;
        heap->freeBytes = size;
        heap->end = Bytes(After(first, BlockSize(first)));
        heap->unknown38 = unknown;
        heap->ready = 1;
        heap->freeBlocks = 1;
    }

    // The heap gets two thirds of the pool, the small allocations' pages the rest
void SetHeapPool(HeapManager* heap, u8* memory, u32 size)
    {
        u32 third = size / 3;
        u32 heapSize = size - third - 0x10;
        heap->small.start = reinterpret_cast<u8*>((reinterpret_cast<u32>(memory) + heapSize + 15) & ~15u);
        InitHeap(heap, memory, heapSize, 0);
        InitAllocatorCache(&heap->small, heap->small.start, third - 0x10);
        heap->deferredFrame = 0;
        heap->deferredFrees[2] = nullptr;
        heap->deferredFrees[1] = nullptr;
        heap->deferredFrees[0] = nullptr;
    }

    // How much bigger the block is than the size, or NoFit
s32 BlockFitSlack(HeapManager*, HeapBlock* block, u32 size)
    {
        u32 blockSize = block->size & SizeMask;
        if (blockSize < size)
        {
            return NoFit;
        }

        return blockSize - size;
    }

    // Takes the size from the free block, the rest a free block of its own when more than 32 bytes are left
void* TakeBlock(HeapManager* heap, HeapBlock* block, u32 size)
    {
        u32 blockSize = block->size & SizeMask;
        if (blockSize < size)
        {
            return nullptr;
        }

        void* memory = block + 1;
        if (size + 0x20 < blockSize)
        {
            HeapBlock* rest = After(block, size);
            rest->size = (rest->size & FreeBit) | (blockSize - size - sizeof(HeapBlock));
            rest->next = nullptr;
            rest->previous = nullptr;
            UnlinkFree(heap, block);
            heap->freeBlocks--;
            block->size = BlockSize(block);
            heap->usedBlocks++;
            PushFree(heap, rest);
            rest->size = BlockSize(rest) | FreeBit;
            heap->recentFree = rest;
            heap->freeBlocks++;
            block->size = (block->size & FreeBit) | size;
            CountTaken(heap, BlockSize(block) + sizeof(HeapBlock));
            rest->before = block;
            HeapBlock* after = After(rest, BlockSize(rest));
            if (Bytes(after) < heap->end)
            {
                after->before = rest;
            }
        }
        else
        {
            heap->recentFree = block->next;
            UnlinkFree(heap, block);
            heap->freeBlocks--;
            block->size = BlockSize(block);
            heap->usedBlocks++;
            CountTaken(heap, BlockSize(block) + sizeof(HeapBlock));
        }

        return memory;
    }

    // The free block that fits the size best
void* HeapBestFit(HeapManager* heap, HeapBlock* block, u32 size)
    {
        s32 bestSlack = WorstFit;
        HeapBlock* best = nullptr;
        do
        {
            s32 slack = BlockFitSlack(heap, block, size);
            if (slack != NoFit && slack < bestSlack)
            {
                bestSlack = slack;
                best = block;
            }

            heap->searchSteps++;
            block = block->next;
        } while (block != nullptr);

        return best != nullptr ? TakeBlock(heap, best, size) : nullptr;
    }

void* HeapAlloc(HeapManager* heap, u32 size)
    {
        if (!heap->ready)
        {
            return nullptr;
        }

        heap->allocations++;
        return HeapBestFit(heap, heap->freeList, (size + 15) & ~15u);
    }

    // Joins the free block with the free blocks around it. Returns whether it joined any
s32 MergeFreeBlock(HeapManager* heap, HeapBlock* block)
    {
        s32 merged = 0;
        HeapBlock* after = After(block, BlockSize(block));
        if (Bytes(after) < heap->end && (after->size & FreeBit))
        {
            HeapBlock* afterAfter = After(after, BlockSize(after));
            if (heap->recentFree == after)
            {
                heap->recentFree = block;
            }

            block->size = (block->size & FreeBit) | ((block->size & SizeMask) + (after->size & SizeMask) + sizeof(HeapBlock));
            if (Bytes(afterAfter) < heap->end)
            {
                afterAfter->before = block;
            }

            UnlinkFree(heap, after);
            merged = 1;
            heap->freeBlocks--;
        }

        HeapBlock* before = block->before;
        if (before == nullptr || !(before->size & FreeBit))
        {
            return merged;
        }

        before->size = FreeBit | ((before->size & SizeMask) + (block->size & SizeMask) + sizeof(HeapBlock));
        after = After(block, BlockSize(block));
        if (Bytes(after) < heap->end)
        {
            after->before = block->before;
        }

        if (heap->recentFree == block)
        {
            heap->recentFree = block->before;
        }

        UnlinkFree(heap, block);
        heap->freeBlocks--;
        return 1;
    }

void FreeHeap(HeapManager* heap, void* memory)
    {
        if (memory == nullptr || !heap->ready)
        {
            return;
        }

        u32* words = static_cast<u32*>(memory);
        HeapBlock* block = words[-3] == AlignedMarker ? reinterpret_cast<HeapBlock*>(words[-2]) : BlockAt(memory) - 1;
        heap->usedBlocks--;
        PushFree(heap, block);
        block->size = BlockSize(block) | FreeBit;
        heap->freeBlocks++;
        u32 freed = BlockSize(block) + sizeof(HeapBlock);
        heap->freeBytes += freed;
        heap->usedBytes -= freed;
        MergeFreeBlock(heap, block);
    }

    // The allocator

    void* GetMemoryAddress(HeapManager* heap, u32 size)
    {
        if (size < SmallAllocationLimit)
        {
            return CacheAlloc(&heap->small, size);
        }

        return HeapAlloc(heap, size);
    }

    void FreeMemory(HeapManager* heap, void* memory)
    {
        if (Bytes(memory) < heap->small.start)
        {
            FreeHeap(heap, memory);
        }
        else
        {
            FreeCache(&heap->small, memory);
        }
    }

    // From the heap whatever its size: the block's address and a marker go in front of an allocation it had to align
    void* MemoryAllocateAligned(HeapManager* heap, u32 size, u32 alignment)
    {
        u32 mask = alignment - 1;
        u8* memory = static_cast<u8*>(HeapAlloc(heap, size + mask + (alignment - (size & mask))));
        if ((reinterpret_cast<u32>(memory) & mask) == 0)
        {
            return memory;
        }

        u32* aligned = reinterpret_cast<u32*>((reinterpret_cast<u32>(memory) + alignment - 1) & ~mask);
        aligned[-2] = reinterpret_cast<u32>(memory - sizeof(HeapBlock));
        aligned[-3] = AlignedMarker;
        return aligned;
    }

    void FreeDeferred(HeapManager* heap, void* memory)
    {
        DeferredFree* entry = static_cast<DeferredFree*>(MemoryAllocate(sizeof(DeferredFree)));
        entry->memory = memory;
        DeferredFree** list = &heap->deferredFrees[heap->deferredFrame];
        entry->next = *list;
        *list = entry;
    }

    void CollectDeferredFrees(HeapManager* heap)
    {
        heap->deferredFrame++;
        if (heap->deferredFrame == DeferredFrames)
        {
            heap->deferredFrame = 0;
        }

        for (DeferredFree* entry = heap->deferredFrees[heap->deferredFrame]; entry != nullptr;)
        {
            FreeMemory(heap, entry->memory);
            DeferredFree* next = entry->next;
            MemoryDeallocate2_(entry);
            entry = next;
        }

        heap->deferredFrees[heap->deferredFrame] = nullptr;
    }

void DestroyTheHeapManager()
    {
        DestroyMemoryController(&g_HeapManager.small, DestroyOnly);
        DestroyHeapManager(&g_HeapManager, DestroyOnly);
    }

    HeapManager* GetHeapManager()
    {
        if (!g_HeapManagerConstructed)
        {
            CreateHeapManager(&g_HeapManager);
            InitMemController(&g_HeapManager.small);
            g_HeapManagerConstructed = 1;
            RetailLibc::AtExit(DestroyTheHeapManager);
        }

        if (!g_PoolsAllocated[0])
        {
            u32 size = Platform::Memory::PoolSpace() - DiskPoolSize;
            g_PoolsAllocated[0] = true;
            g_HeapPool = Platform::Memory::AllocatePool(size);
            SetHeapPool(&g_HeapManager, static_cast<u8*>(g_HeapPool), size);
        }

        return &g_HeapManager;
    }

    void DestroyTheDiskManager()
    {
        DiskManagerDestroy(&g_DiskManager, DestroyOnly);
    }

    DiskManager* GetDiskManager()
    {
        if (!g_DiskManagerConstructed)
        {
            DiskManagerConstruct(&g_DiskManager);
            g_DiskManagerConstructed = 1;
            RetailLibc::AtExit(DestroyTheDiskManager);
        }

        if (!g_PoolsAllocated[1])
        {
            g_PoolsAllocated[1] = true;
            g_DiskPool = Platform::Memory::AllocatePool(DiskPoolSize);
            DiskManagerSetPool(&g_DiskManager, g_DiskPool, DiskPoolSize);
        }

        return &g_DiskManager;
    }

    void* MemoryAllocate(u32 size)
    {
        return GetMemoryAddress(GetHeapManager(), size);
    }

    void* MemoryAllocate2(u32 size)
    {
        return GetMemoryAddress(GetHeapManager(), size);
    }

    void MemoryDeallocate2_(void* memory)
    {
        FreeMemory(GetHeapManager(), memory);
    }

    void MemoryDeallocate_(void* memory)
    {
        FreeMemory(GetHeapManager(), memory);
    }
}
