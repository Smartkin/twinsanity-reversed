#include "game/pools.h"

#include "game/memory.h"

#include <string.h>

// Each item type has its own copy of every function in the retail code: the shared code is here, each copy calls it

namespace
{
// The walks' vtable slots
constexpr u32 SlotDestroy = 1;
constexpr u32 SlotFirst = 2;
constexpr u32 SlotAtEnd = 3;
constexpr u32 SlotItem = 4;
constexpr u32 SlotNext = 5;

template <typename T>
ItemPool<T>* ConstructPool(ItemPool<T>* pool, const GccVTableEntry* vtable)
{
    pool->vtable = vtable;
    pool->growth = PoolGrowth;
    pool->freeHead = PoolNoFreeSlot;
    pool->capacity = 0;
    pool->count = 0;
    pool->links = nullptr;
    pool->items = nullptr;
    return pool;
}

template <typename T>
void DestroyPool(ItemPool<T>* pool, const GccVTableEntry* vtable, u32 destroyFlags)
{
    pool->vtable = vtable;
    if (pool->links != nullptr)
    {
        MemoryDeallocate_(pool->links);
    }

    if (pool->items != nullptr)
    {
        MemoryDeallocate_(pool->items);
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(pool);
    }
}

// Only grown when it's full, so every slot it had is used. A growth of 0 would mark the last used slot the free list's end
template <typename T>
void GrowPool(ItemPool<T>* pool)
{
    if ((pool->capacity & 1) != 0)
    {
        pool->capacity++;
    }

    if ((pool->growth & 1) != 0)
    {
        pool->growth++;
    }

    T* items = static_cast<T*>(MemoryAllocate2((pool->capacity + pool->growth) * sizeof(T)));
    s16* links = static_cast<s16*>(MemoryAllocate2((pool->capacity + pool->growth) * sizeof(s16)));
    if (pool->capacity != 0)
    {
        T* old = pool->items;
        pool->items = items;
        for (s32 slot = 0; slot < pool->capacity; slot++)
        {
            if (pool->links[slot] == PoolSlotUsed)
            {
                pool->items[slot] = old[slot];
            }
        }

        // The old slots are all used: every byte of their links 0xFF makes them PoolSlotUsed
        memset(links, 0xFF, pool->capacity * sizeof(s16));
        if (old != nullptr)
        {
            MemoryDeallocate_(old);
        }

        if (pool->links != nullptr)
        {
            MemoryDeallocate_(pool->links);
        }
    }

    s32 slot = pool->capacity;
    for (; slot < pool->capacity + pool->growth; slot++)
    {
        links[slot] = static_cast<s16>(slot + 1);
    }

    s16 first = pool->capacity;
    pool->links = links;
    links[slot - 1] = PoolFreeListEnd;
    pool->items = items;
    pool->capacity = first + pool->growth;
    pool->freeHead = first;
}

template <typename T>
s32 AllocateSlot(ItemPool<T>* pool)
{
    if (!(pool->count < pool->capacity))
    {
        pool->Grow();
        return pool->Allocate();
    }

    s32 slot = pool->freeHead;
    pool->freeHead = pool->links[slot];
    pool->links[slot] = PoolSlotUsed;
    pool->count++;
    return slot;
}

template <typename T>
s32 AddItem(ItemPool<T>* pool, const T* item)
{
    s32 slot = pool->Allocate();
    pool->items[slot] = *item;
    return slot;
}

template <typename T>
void ClearPools(ItemPools<T>* pools)
{
    for (s32 index = 0; index < pools->count; index++)
    {
        ItemPool<T>* pool = &pools->pools[index];
        if (pool->capacity <= 0 || pool->count == 0)
        {
            continue;
        }

        s32 slot = 0;
        for (; slot < pool->capacity - 1; slot++)
        {
            pool->links[slot] = static_cast<s16>(slot + 1);
        }

        pool->links[slot] = PoolFreeListEnd;
        pool->freeHead = 0;
        pool->count = 0;
    }
}

template <typename T>
void DestroyWalkBase(PoolWalkBase<T>* walk, const GccVTableEntry* baseVTable, u32 destroyFlags)
{
    walk->vtable = baseVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(walk);
    }
}

template <typename T>
void FirstSlot(PoolWalk<T>* walk)
{
    walk->slot = 0;
    walk->position = 0;
    while (walk->slot < walk->pool->capacity - 1 && walk->pool->links[walk->slot] != PoolSlotUsed)
    {
        walk->slot++;
    }
}

template <typename T>
void NextSlot(PoolWalk<T>* walk)
{
    if (!(walk->position < walk->pool->count - 1))
    {
        walk->position = walk->pool->count;
        return;
    }

    while (walk->position < walk->pool->count)
    {
        walk->slot++;
        if (walk->pool->links[walk->slot] == PoolSlotUsed)
        {
            walk->position++;
            return;
        }
    }
}

template <typename T>
PoolWalk<T>* AssignWalk(PoolWalk<T>* walk, const PoolWalk<T>* other)
{
    walk->slot = other->slot;
    walk->position = other->position;
    walk->pool = other->pool;
    return walk;
}

template <typename T>
void DestroyPoolsWalk(PoolsWalk<T>* walk, const GccVTableEntry* vtable, const GccVTableEntry* baseVTable, u32 destroyFlags)
{
    walk->vtable = vtable;
    for (s32 index = 0; index < walk->count; index++)
    {
        PoolWalk<T>* poolWalk = walk->walks[index];
        if (poolWalk != nullptr)
        {
            CallVirtual<void>(poolWalk, poolWalk->vtable, SlotDestroy, u32{DestroyAndFree});
        }
    }

    if (walk->walks != nullptr)
    {
        MemoryDeallocate_(walk->walks);
    }

    DestroyWalkBase(walk, baseVTable, destroyFlags);
}

template <typename T>
void FirstPool(PoolsWalk<T>* walk)
{
    if (walk->count <= 0)
    {
        walk->current = nullptr;
        walk->index = 0;
        return;
    }

    walk->index = 0;
    walk->current = walk->walks[0];
    CallVirtual<void>(walk->current, walk->current->vtable, SlotFirst);
}

template <typename T>
T* CurrentItem(PoolsWalk<T>* walk)
{
    return CallVirtual<T*>(walk->current, walk->current->vtable, SlotItem);
}

// The current pool's next item, or the next pool's first
template <typename T>
void NextItem(PoolsWalk<T>* walk)
{
    PoolWalk<T>* current = walk->current;
    if (current == nullptr)
    {
        return;
    }

    if (CallVirtual<u32>(current, current->vtable, SlotAtEnd) == 0)
    {
        CallVirtual<void>(walk->current, walk->current->vtable, SlotNext);
        if (CallVirtual<u32>(walk->current, walk->current->vtable, SlotAtEnd) == 0)
        {
            return;
        }
    }

    walk->index++;
    if (!(walk->index < walk->count))
    {
        walk->current = nullptr;
        return;
    }

    walk->current = walk->walks[walk->index];
    CallVirtual<void>(walk->current, walk->current->vtable, SlotFirst);
}
}

template <>
ItemPool<InstanceContext*>* ItemPool<InstanceContext*>::Construct(ItemPool* pool)
{
    return ConstructPool(pool, g_InstancePoolVTable);
}

template <>
void ItemPool<InstanceContext*>::Destroy(u32 destroyFlags)
{
    DestroyPool(this, g_InstancePoolVTable, destroyFlags);
}

template <>
void ItemPool<InstanceContext*>::Grow()
{
    GrowPool(this);
}

template <>
s32 ItemPool<InstanceContext*>::Allocate()
{
    return AllocateSlot(this);
}

template <>
s32 ItemPool<InstanceContext*>::Add(InstanceContext* const* item)
{
    return AddItem(this, item);
}

template <>
ItemPool<SceneryCell*>* ItemPool<SceneryCell*>::Construct(ItemPool* pool)
{
    return ConstructPool(pool, g_CellPoolVTable);
}

template <>
void ItemPool<SceneryCell*>::Destroy(u32 destroyFlags)
{
    DestroyPool(this, g_CellPoolVTable, destroyFlags);
}

template <>
void ItemPool<SceneryCell*>::Grow()
{
    GrowPool(this);
}

template <>
s32 ItemPool<SceneryCell*>::Allocate()
{
    return AllocateSlot(this);
}

template <>
s32 ItemPool<SceneryCell*>::Add(SceneryCell* const* item)
{
    return AddItem(this, item);
}

s32 AddInstanceToPool(ItemPools<InstanceContext*>* pools, s32 index, InstanceContext* instance)
{
    return pools->pools[index].Add(&instance);
}

s32 AddCellToPool(ItemPools<SceneryCell*>* pools, s32 index, SceneryCell* cell)
{
    return pools->pools[index].Add(&cell);
}

bool InstancePoolEmpty(const ItemPools<InstanceContext*>* pools, s32 index)
{
    return pools->pools[index].count == 0;
}

void ClearCellPools(ItemPools<SceneryCell*>* pools)
{
    ClearPools(pools);
}

void ClearInstancePools(ItemPools<InstanceContext*>* pools)
{
    ClearPools(pools);
}

template <>
void PoolWalkBase<InstanceContext*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_InstanceWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<InstanceContext*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_InstanceWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<InstanceContext*>::First()
{
    FirstSlot(this);
}

template <>
u32 PoolWalk<InstanceContext*>::AtEnd()
{
    return position == pool->count;
}

template <>
InstanceContext** PoolWalk<InstanceContext*>::Item()
{
    return &pool->items[slot];
}

template <>
void PoolWalk<InstanceContext*>::Next()
{
    NextSlot(this);
}

template <>
PoolWalk<InstanceContext*>* PoolWalk<InstanceContext*>::Assign(const PoolWalk* other)
{
    return AssignWalk(this, other);
}

template <>
s32 PoolWalk<InstanceContext*>::Slot()
{
    return slot;
}

template <>
s32 PoolWalk<InstanceContext*>::Count()
{
    return pool->count;
}

template <>
void PoolsWalk<InstanceContext*>::Destroy(u32 destroyFlags)
{
    DestroyPoolsWalk(this, g_InstancePoolsWalkVTable, g_InstanceWalkBaseVTable, destroyFlags);
}

template <>
void PoolsWalk<InstanceContext*>::First()
{
    FirstPool(this);
}

template <>
u32 PoolsWalk<InstanceContext*>::AtEnd()
{
    return current == nullptr;
}

template <>
InstanceContext** PoolsWalk<InstanceContext*>::Item()
{
    return CurrentItem(this);
}

template <>
void PoolsWalk<InstanceContext*>::Next()
{
    NextItem(this);
}

template <>
void PoolWalkBase<SceneryCell*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_CellWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<SceneryCell*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_CellWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<SceneryCell*>::First()
{
    FirstSlot(this);
}

template <>
u32 PoolWalk<SceneryCell*>::AtEnd()
{
    return position == pool->count;
}

template <>
SceneryCell** PoolWalk<SceneryCell*>::Item()
{
    return &pool->items[slot];
}

template <>
void PoolWalk<SceneryCell*>::Next()
{
    NextSlot(this);
}

template <>
PoolWalk<SceneryCell*>* PoolWalk<SceneryCell*>::Assign(const PoolWalk* other)
{
    return AssignWalk(this, other);
}

template <>
s32 PoolWalk<SceneryCell*>::Slot()
{
    return slot;
}

template <>
s32 PoolWalk<SceneryCell*>::Count()
{
    return pool->count;
}

template <>
void PoolsWalk<SceneryCell*>::Destroy(u32 destroyFlags)
{
    DestroyPoolsWalk(this, g_CellPoolsWalkVTable, g_CellWalkBaseVTable, destroyFlags);
}

template <>
void PoolsWalk<SceneryCell*>::First()
{
    FirstPool(this);
}

template <>
u32 PoolsWalk<SceneryCell*>::AtEnd()
{
    return current == nullptr;
}

template <>
SceneryCell** PoolsWalk<SceneryCell*>::Item()
{
    return CurrentItem(this);
}

template <>
void PoolsWalk<SceneryCell*>::Next()
{
    NextItem(this);
}

template <>
void PoolWalkBase<void*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_OtherWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<void*>::Destroy(u32 destroyFlags)
{
    DestroyWalkBase(this, g_OtherWalkBaseVTable, destroyFlags);
}

template <>
void PoolWalk<void*>::First()
{
    FirstSlot(this);
}

template <>
u32 PoolWalk<void*>::AtEnd()
{
    return position == pool->count;
}

template <>
void** PoolWalk<void*>::Item()
{
    return &pool->items[slot];
}

template <>
void PoolWalk<void*>::Next()
{
    NextSlot(this);
}

template <>
PoolWalk<void*>* PoolWalk<void*>::Assign(const PoolWalk* other)
{
    return AssignWalk(this, other);
}

template <>
s32 PoolWalk<void*>::Slot()
{
    return slot;
}

template <>
s32 PoolWalk<void*>::Count()
{
    return pool->count;
}

template <>
void PoolsWalk<void*>::Destroy(u32 destroyFlags)
{
    DestroyPoolsWalk(this, g_OtherPoolsWalkVTable, g_OtherWalkBaseVTable, destroyFlags);
}

template <>
void PoolsWalk<void*>::First()
{
    FirstPool(this);
}

template <>
u32 PoolsWalk<void*>::AtEnd()
{
    return current == nullptr;
}

template <>
void** PoolsWalk<void*>::Item()
{
    return CurrentItem(this);
}

template <>
void PoolsWalk<void*>::Next()
{
    NextItem(this);
}
