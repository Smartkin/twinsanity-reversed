#pragma once

#include "common.h"

// The disk manager: the pool the chunks' data is read into (models, textures, animations, collision), handed out in blocks
// that callers hold by handle. It keeps the pool compact by moving used blocks down into free ones before them, a step at a time
// (DiskCompactStep) or all at once (DiskCompactAll), which is why callers keep handles and not addresses. Blocks the graphics
// hardware may still be reading (DMA) are released a couple of frames later (DiskProcessReleases, once a frame)
struct DiskNode;

enum DiskNodeState : u32
{
    DiskNodeFree = 1,
    DiskNodeUsed = 2,
    // Being moved: its copy's source and destination
    DiskNodeMoving = 3,
    DiskNodeMoveDestination = 4,
    // Moved away while it could still be read: freed two rounds of releases later
    DiskNodeMovedAway = 5,
    DiskNodeMovedAwayExpired = 6,
    // Released while it could still be read: released two rounds of releases later
    DiskNodeReleased = 7,
    DiskNodeReleasedExpired = 8,
};

// What reading into a node did: the hardware (DMA) reads into it from DiskAllocate until DiskMarkLoaded
enum DiskLoadState : u32
{
    DiskNotReadInto = 0,
    DiskLoading = 1,
    DiskLoaded = 2,
};

union DiskNodeBits
{
    u32 value;
    struct
    {
        // DiskNodeState
        u32 state : 4;
        // Counted up by every round of releases, which moves a waiting release on when it finds it 0 (it wraps at 10 bits)
        u32 waitCount : 10;
        // Its memory can be used: it wasn't read into, or reading it was seen done (DiskLoadedMemory)
        u32 ready : 1;
        // DiskLoadState
        u32 loadState : 2;
        // Released a couple of frames late, the hardware may still read it
        u32 deferRelease : 1;
        u32 unused18 : 14;
    };
};
CHECK_SIZE(DiskNodeBits, 4);

struct DiskNode
{
    // Its handle, -1 for the pool's first node and -2 for other free ones
    s32 index;
    u8* memory;
    u32 size;
    // In its free list, by size
    DiskNode* previousFree;
    DiskNode* nextFree;
    // In the pool, by address
    DiskNode* previous;
    DiskNode* next;
    DiskNodeBits bits;
};
CHECK_OFFSET(DiskNode, bits, 0x1C);
CHECK_SIZE(DiskNode, 0x20);

// The move in progress
struct DiskMove
{
    DiskNode* source;
    DiskNode* destination;
    u32 copied;
    u32 size;
    // 1 while copying
    u32 active;
    struct DiskManager* manager;
};
CHECK_SIZE(DiskMove, 0x18);

// A node waiting for its release
struct DiskPendingRelease
{
    DiskNode* node;
    DiskPendingRelease* previous;
    DiskPendingRelease* next;
};
CHECK_SIZE(DiskPendingRelease, 0xC);

// A handle of no memory (a reader's before it has any)
constexpr s32 NoDiskHandle = -1;
constexpr u32 DiskSizeClasses = 16;
constexpr u32 DiskHandles = 5500;

struct DiskManager
{
    u8* pool;
    u32 poolSize;
    // The lowest free node, as the free lists last saw it (only its own list is searched again when it's taken)
    DiskNode* lowestFree;
    // Free nodes by size class, each list by size
    DiskNode* freeLists[DiskSizeClasses];
    s32 freeCount;
    // Every node, by address
    DiskNode* nodes;
    u8* poolEnd;
    DiskNode* handles[DiskHandles];
    // The handles not in use, a stack from handlesUsed up
    s16 freeHandles[DiskHandles];
    s32 handlesUsed;
    DiskMove* move;
    DiskPendingRelease* pendingReleases;
    s32 pendingReleaseCount;
};
CHECK_SIZE(DiskManager, 0x8150);
CHECK_OFFSET(DiskManager, handles, 0x58);
CHECK_OFFSET(DiskManager, move, 0x8144);

extern "C"
{
    DiskManager* GetDiskManager() RETAIL(GetDiskManager_);
    // The singleton's destructor, registered with atexit
    void DestroyTheDiskManager() RETAIL(FUN_00182460);

    DiskManager* DiskManagerConstruct(DiskManager* manager) RETAIL(FUN_00205a28);
    void DiskManagerDestroy(DiskManager* manager, u32 flags) RETAIL(FUN_00205af8);
    void DiskManagerSetPool(DiskManager* manager, void* pool, u32 size) RETAIL(FUN_00203888);

    // A block of at least size bytes (in steps of 64). One the hardware is about to read into (DMA) is loading until
    // DiskMarkLoaded. Returns handle
    s32* DiskAllocate(s32* handle, DiskManager* manager, u32 size, bool readInto, u32 deferRelease) RETAIL(GetDiskNodeIndex);
    void DiskRelease(DiskManager* manager, const s32* handle) RETAIL(FUN_00205c18);
    u8* DiskMemory(DiskManager* manager, const s32* handle) RETAIL(FUN_00205c70);
    void DiskMarkLoaded(DiskManager* manager, const s32* handle) RETAIL(FUN_00205c88);
    // Its memory once it's loaded (it's ready from then on), null before
    u8* DiskLoadedMemory(DiskManager* manager, const s32* handle) RETAIL(GetReadDiskDataPosition_);
    bool DiskNodeIsLoaded(const DiskNode* node) RETAIL(IsDiskNodeLoaded_);

    // Copies a step of the move in progress, or starts one. With immediately, moves one used block into the free one before it
    // at once. Returns whether it did anything
    bool DiskCompactStep(DiskManager* manager, bool immediately) RETAIL(FUN_00205d18);
    // Moves every used block down and processes the releases until nothing's left to do
    void DiskCompactAll(DiskManager* manager) RETAIL(FUN_00203de0);
    // A round of the releases waiting: at most 16 of them
    void DiskProcessReleases(DiskManager* manager) RETAIL(FUN_00203ae8);
    void DiskNothing() RETAIL(FUN_00205d10);
}
