#pragma once

#include "common.h"

// The game's allocator, over a pool the platform gives it: a general heap of 16 byte aligned blocks, and 4 KB pages handing out
// entries of 63 sizes for what's smaller than 4 KB (MemoryController). Frees can wait for the GPU to be done with the memory
// (FreeDeferred). The disk manager (game/disk.h) has a pool of its own for the chunks' data.

// Bit 0 of a GCC 2.9x destructor's flags (gcc2.h's DestructorFlags): the object is freed once it's destroyed
constexpr u32 FreeAfterDestroy = 1;

// A heap block's size: its bytes, and whether it's free
union HeapBlockSize
{
    u32 value;
    struct
    {
        u32 bytes : 31;
        u32 free : 1;
    };
};
CHECK_SIZE(HeapBlockSize, 4);

// A block of the heap: this header, then its bytes
struct HeapBlock
{
    // In the free list
    HeapBlock* next;
    HeapBlock* previous;
    // The block before it in memory
    HeapBlock* before;
    HeapBlockSize size;
};
CHECK_OFFSET(HeapBlock, size, 0xC);
CHECK_SIZE(HeapBlock, 0x10);

// Entries of one size in a run of memory, the free ones linked through their first word
struct FixedAllocator
{
    void* memory;
    u32 size;
    u32 entrySize;
    void* nextFree;
    u32 capacity;
    u32 freeCount;
};
CHECK_SIZE(FixedAllocator, 0x18);

struct FixedAllocatorNode
{
    FixedAllocator allocator;
    FixedAllocatorNode* newer;
    FixedAllocatorNode* older;
};
CHECK_SIZE(FixedAllocatorNode, 0x20);

constexpr u32 SmallAllocationLimit = 0x1000;
constexpr u32 SizeClassCount = 63;

// A page's node comes from freeNodes. sizeClasses has the newest page of each size, the older ones behind it
struct MemoryController
{
    u32 pageCount;
    FixedAllocatorNode* nodes;
    u16* pageSizeClasses;
    u8* pages;
    FixedAllocatorNode freeNodes;
    FixedAllocatorNode* sizeClasses[SizeClassCount];
    // Where the pages start: what's below is the heap's
    u8* start;
};
CHECK_SIZE(MemoryController, 0x130);

// A free waiting for its frame
struct DeferredFree
{
    void* memory;
    DeferredFree* next;
};
CHECK_SIZE(DeferredFree, 8);

constexpr u32 DeferredFrames = 3;

struct HeapManager
{
    HeapBlock* freeList;
    HeapBlock* first;
    u8* end;
    s32 freeBlocks;
    s32 usedBlocks;
    s32 freeBytes;
    s32 usedBytes;
    u8* poolStart;
    u32 poolSize;
    s32 searchSteps;
    s32 allocations;
    s32 peakUsedBytes;
    s32 ready;
    // The free block made last
    HeapBlock* recentFree;
    u32 unused38;
    u32 unused3C;
    MemoryController small;
    DeferredFree* deferredFrees[DeferredFrames];
    u32 deferredFrame;
};
CHECK_OFFSET(HeapManager, unused38, 0x38);
CHECK_OFFSET(HeapManager, small, 0x40);
CHECK_SIZE(HeapManager, 0x180);

extern "C"
{
    HeapManager* GetHeapManager() RETAIL(GetHeapManager_);

    // The game's operator new, new[], delete and delete[] (GCC 2.9x's __builtin_new and co.)
    void* MemoryAllocate(u32 size);
    void* MemoryAllocate2(u32 size);
    void MemoryDeallocate2_(void* memory);
    void MemoryDeallocate_(void* memory);

    void* MemoryAllocateAligned(HeapManager* heap, u32 size, u32 alignment);
    void FreeMemory(HeapManager* heap, void* memory);
    // Frees it once the frames that may still use it (the GPU's DMA) are done: at the third CollectDeferredFrees from now
    void FreeDeferred(HeapManager* heap, void* memory) RETAIL(FUN_00204d78);
    void CollectDeferredFrees(HeapManager* heap) RETAIL(FUN_00204dd0);
}

// GCC 2.9x's new[] of a type with a destructor: a cookie of 16 bytes counting the elements before them
constexpr u32 ArrayCookieSize = 0x10;
constexpr u32 ArrayCookieWords = ArrayCookieSize / sizeof(u32);

template <typename T>
T* NewArray(u32 count)
{
    auto* block = static_cast<u32*>(MemoryAllocate2(count * sizeof(T) + ArrayCookieSize));
    block[0] = count;
    return reinterpret_cast<T*>(block + ArrayCookieWords);
}

template <typename T>
u32 ArrayCount(T* array)
{
    return *(reinterpret_cast<u32*>(array) - ArrayCookieWords);
}

template <typename T>
void DeleteArray(T* array)
{
    MemoryDeallocate_(reinterpret_cast<u32*>(array) - ArrayCookieWords);
}
