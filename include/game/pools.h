#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/memory.h"

struct InstanceContext;
struct SceneryCell;

// A pool of items (0x14 bytes; its vtable, after its members, only has the destructor): slots handed out from a free list
// through the links (a used slot's -1, the free list's end -2); full, it grows by its growth, both made even first. The
// retail code has one per item type, each with its own copies of the functions
template <typename T>
struct ItemPool
{
    s16 capacity;
    s16 growth;
    s16 count;
    s16 freeHead;
    s16* links;
    T* items;
    const GccVTableEntry* vtable;

    static ItemPool* Construct(ItemPool* pool);
    void Destroy(u32 destroyFlags);
    void Grow();
    // A slot taken (the pool grown when it's full), and one given an item
    s32 Allocate();
    s32 Add(const T* item);

    void VirtualDestroy(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, 1, destroyFlags);
    }
};

// Pools made by new[] and their count
template <typename T>
struct ItemPools
{
    ItemPool<T>* pools;
    s32 count;
};
static_assert(sizeof(ItemPool<void*>) == 0x14);

// The walks over pools' used slots, on an abstract base of their own (the retail iterators; their vtables: 1 destructor, 2 to
// the first, 3 whether it's past the last, 4 the slot's item, 5 to the next)
template <typename T>
struct PoolWalkBase
{
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags);
};

// A pool's (0xC bytes; its vtable also has 6 copied from another, 7 the slot, 8 the pool's count): the slot it's at and how many
// used ones are before it
template <typename T>
struct PoolWalk : PoolWalkBase<T>
{
    s16 slot;
    s16 position;
    ItemPool<T>* pool;

    void Destroy(u32 destroyFlags);
    void First();
    u32 AtEnd();
    T* Item();
    void Next();
    PoolWalk* Assign(const PoolWalk* other);
    s32 Slot();
    s32 Count();
};

// A set of pools' (0x14 bytes): a pool's walk for each pool it was made with that has items
template <typename T>
struct PoolsWalk : PoolWalkBase<T>
{
    s32 index;
    s32 count;
    PoolWalk<T>* current;
    PoolWalk<T>** walks;

    void Destroy(u32 destroyFlags);
    void First();
    u32 AtEnd();
    T* Item();
    void Next();
};

extern "C"
{
    extern const GccVTableEntry g_InstancePoolVTable[] RETAIL(D_002FBF40);
    extern const GccVTableEntry g_CellPoolVTable[] RETAIL(D_002FBF28);
    extern const GccVTableEntry g_InstanceWalkVTable[] RETAIL(D_002FB720);
    extern const GccVTableEntry g_InstancePoolsWalkVTable[] RETAIL(D_002FB770);
    extern const GccVTableEntry g_InstanceWalkBaseVTable[] RETAIL(D_002FB7A8);
    extern const GccVTableEntry g_CellWalkVTable[] RETAIL(D_002FB7E0);
    extern const GccVTableEntry g_CellPoolsWalkVTable[] RETAIL(D_002FB830);
    extern const GccVTableEntry g_CellWalkBaseVTable[] RETAIL(D_002FB868);
    // A third kind's walks, never made (their pool walk's vtable is the 0x50 bytes before D_002FB6B0)
    extern const GccVTableEntry g_OtherPoolsWalkVTable[] RETAIL(D_002FB6B0);
    extern const GccVTableEntry g_OtherWalkBaseVTable[] RETAIL(D_002FB6E8);

    // An item put in one of the pools, and whether one has none
    s32 AddInstanceToPool(ItemPools<InstanceContext*>* pools, s32 index, InstanceContext* instance) RETAIL(FUN_00199f40);
    s32 AddCellToPool(ItemPools<SceneryCell*>* pools, s32 index, SceneryCell* cell) RETAIL(FUN_0019a030);
    bool InstancePoolEmpty(const ItemPools<InstanceContext*>* pools, s32 index) RETAIL(FUN_0019a060);
    // Every pool that has items made empty
    void ClearCellPools(ItemPools<SceneryCell*>* pools) RETAIL(FUN_001f4300);
    void ClearInstancePools(ItemPools<InstanceContext*>* pools) RETAIL(FUN_001f43b0);
}

template <> ItemPool<InstanceContext*>* ItemPool<InstanceContext*>::Construct(ItemPool* pool) RETAIL(FUN_001f4290);
template <> void ItemPool<InstanceContext*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f4460);
template <> void ItemPool<InstanceContext*>::Grow() RETAIL(FUN_00100dc8);
template <> s32 ItemPool<InstanceContext*>::Allocate() RETAIL(FUN_00199e80);
template <> s32 ItemPool<InstanceContext*>::Add(InstanceContext* const* item) RETAIL(FUN_00199ef8);

template <> ItemPool<SceneryCell*>* ItemPool<SceneryCell*>::Construct(ItemPool* pool) RETAIL(FUN_001f42c8);
template <> void ItemPool<SceneryCell*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f44d0);
template <> void ItemPool<SceneryCell*>::Grow() RETAIL(FUN_00100f58);
template <> s32 ItemPool<SceneryCell*>::Allocate() RETAIL(FUN_00199f70);
template <> s32 ItemPool<SceneryCell*>::Add(SceneryCell* const* item) RETAIL(FUN_00199fe8);

template <> void PoolWalkBase<InstanceContext*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2b90);
template <> void PoolWalk<InstanceContext*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2b60);
template <> void PoolWalk<InstanceContext*>::First() RETAIL(func_001F2C80);
template <> u32 PoolWalk<InstanceContext*>::AtEnd() RETAIL(FUN_001f2d60);
template <> InstanceContext** PoolWalk<InstanceContext*>::Item() RETAIL(FUN_001f2ec8);
template <> void PoolWalk<InstanceContext*>::Next() RETAIL(func_001F2D78);
template <> PoolWalk<InstanceContext*>* PoolWalk<InstanceContext*>::Assign(const PoolWalk* other) RETAIL(func_001F4098);
template <> s32 PoolWalk<InstanceContext*>::Slot() RETAIL(FUN_001f40b8);
template <> s32 PoolWalk<InstanceContext*>::Count() RETAIL(FUN_001f40c0);
template <> void PoolsWalk<InstanceContext*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2bc0);
template <> void PoolsWalk<InstanceContext*>::First() RETAIL(FUN_001f2cf8);
template <> u32 PoolsWalk<InstanceContext*>::AtEnd() RETAIL(FUN_001f2d50);
template <> InstanceContext** PoolsWalk<InstanceContext*>::Item() RETAIL(FUN_001f2ee0);
template <> void PoolsWalk<InstanceContext*>::Next() RETAIL(FUN_001f2df8);

template <> void PoolWalkBase<SceneryCell*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f27e0);
template <> void PoolWalk<SceneryCell*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f27b0);
template <> void PoolWalk<SceneryCell*>::First() RETAIL(func_001F28D0);
template <> u32 PoolWalk<SceneryCell*>::AtEnd() RETAIL(FUN_001f29b0);
template <> SceneryCell** PoolWalk<SceneryCell*>::Item() RETAIL(FUN_001f2b18);
template <> void PoolWalk<SceneryCell*>::Next() RETAIL(func_001F29C8);
template <> PoolWalk<SceneryCell*>* PoolWalk<SceneryCell*>::Assign(const PoolWalk* other) RETAIL(func_001F40D0);
template <> s32 PoolWalk<SceneryCell*>::Slot() RETAIL(FUN_001f40f0);
template <> s32 PoolWalk<SceneryCell*>::Count() RETAIL(FUN_001f40f8);
template <> void PoolsWalk<SceneryCell*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2810);
template <> void PoolsWalk<SceneryCell*>::First() RETAIL(FUN_001f2948);
template <> u32 PoolsWalk<SceneryCell*>::AtEnd() RETAIL(FUN_001f29a0);
template <> SceneryCell** PoolsWalk<SceneryCell*>::Item() RETAIL(FUN_001f2b30);
template <> void PoolsWalk<SceneryCell*>::Next() RETAIL(FUN_001f2a48);

// A set of pools' walk over the pools a mask picks (a walk made for each that has items) put at its first item, as the retail
// code has it inline
template <typename T>
void ConstructPoolsWalk(PoolsWalk<T>* walk, const GccVTableEntry* vtable, const GccVTableEntry* poolWalkVTable,
                        ItemPools<T>* pools, u32 mask)
{
    walk->vtable = vtable;
    walk->count = 0;
    walk->walks = static_cast<PoolWalk<T>**>(MemoryAllocate2(pools->count * sizeof(PoolWalk<T>*)));
    for (s32 index = 0; index < pools->count; index++)
    {
        if (((mask >> index) & 1) == 0 || pools->pools[index].count <= 0)
        {
            continue;
        }

        auto* poolWalk = static_cast<PoolWalk<T>*>(MemoryAllocate(sizeof(PoolWalk<T>)));
        poolWalk->vtable = poolWalkVTable;
        poolWalk->slot = 0;
        poolWalk->pool = &pools->pools[index];
        poolWalk->position = 0;
        walk->walks[walk->count++] = poolWalk;
    }

    walk->First();
}

template <> void PoolWalkBase<void*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2fa0);
template <> void PoolWalk<void*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2f70);
template <> void PoolWalk<void*>::First() RETAIL(func_001F3090);
template <> u32 PoolWalk<void*>::AtEnd() RETAIL(FUN_001f3170);
template <> void** PoolWalk<void*>::Item() RETAIL(FUN_001f32d8);
template <> void PoolWalk<void*>::Next() RETAIL(func_001F3188);
template <> PoolWalk<void*>* PoolWalk<void*>::Assign(const PoolWalk* other) RETAIL(func_001F4060);
template <> s32 PoolWalk<void*>::Slot() RETAIL(FUN_001f4080);
template <> s32 PoolWalk<void*>::Count() RETAIL(FUN_001f4088);
template <> void PoolsWalk<void*>::Destroy(u32 destroyFlags) RETAIL(FUN_001f2fd0);
template <> void PoolsWalk<void*>::First() RETAIL(FUN_001f3108);
template <> u32 PoolsWalk<void*>::AtEnd() RETAIL(FUN_001f3160);
template <> void** PoolsWalk<void*>::Item() RETAIL(FUN_001f32f0);
template <> void PoolsWalk<void*>::Next() RETAIL(FUN_001f3208);
