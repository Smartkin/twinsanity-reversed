#include "game/controllers.h"

#include "game/memory.h"
#include "game/pools.h"

#include <string.h>

extern "C"
{
    extern const GccVTableEntry g_RendererPoolVTable[] RETAIL(D_002F6B20);

    RendererPool* RendererPoolConstruct(RendererPool* pool)
    {
        pool->vtable = g_RendererPoolVTable;
        pool->capacity = 0;
        pool->growth = PoolGrowth;
        pool->used = 0;
        pool->freeHead = PoolNoFreeSlot;
        pool->links = nullptr;
        pool->items = nullptr;
        return pool;
    }

    void RendererPoolDestroy(RendererPool* pool, u32 destroyFlags)
    {
        pool->vtable = g_RendererPoolVTable;
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

    void RendererPoolGrow(RendererPool* pool)
    {
        if ((pool->capacity & 1) != 0)
        {
            pool->capacity++;
        }

        if ((pool->growth & 1) != 0)
        {
            pool->growth++;
        }

        auto** items = static_cast<Renderer**>(MemoryAllocate2((pool->capacity + pool->growth) * sizeof(Renderer*)));
        auto* links = static_cast<s16*>(MemoryAllocate2((pool->capacity + pool->growth) * sizeof(s16)));
        if (pool->capacity != 0)
        {
            Renderer** old = pool->items;
            pool->items = items;
            for (s32 index = 0; index < pool->capacity; index++)
            {
                if (pool->links[index] == PoolSlotUsed)
                {
                    pool->items[index] = old[index];
                }
            }

            // Every old slot in use (PoolSlotUsed's bytes: the pool only grows when it's full)
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

        // The new slots free, each linked to the next (retail: with a growth of 0 the last old slot's link is the one made -2)
        s32 index = pool->capacity;
        for (; index < pool->capacity + pool->growth; index++)
        {
            links[index] = static_cast<s16>(index + 1);
        }

        links[index - 1] = PoolFreeListEnd;
        pool->links = links;
        pool->items = items;
        s16 capacity = pool->capacity;
        pool->capacity = static_cast<s16>(capacity + pool->growth);
        pool->freeHead = capacity;
    }

    s32 RendererPoolTake(RendererPool* pool)
    {
        if (!(pool->used < pool->capacity))
        {
            RendererPoolGrow(pool);
            return RendererPoolTake(pool);
        }

        s16 index = pool->freeHead;
        s16* link = &pool->links[index];
        pool->freeHead = *link;
        *link = PoolSlotUsed;
        pool->used++;
        return index;
    }

    s32 RendererPoolAdd(RendererPool* pool, Renderer* const* renderer)
    {
        s32 index = RendererPoolTake(pool);
        pool->items[index] = *renderer;
        return index;
    }

    // Retail bug: the walk stops before the last slot, so a pool whose only renderer is in its last slot gives none
    Renderer** RendererPoolFirst(RendererPool* pool)
    {
        for (s32 index = 0; index < pool->capacity - 1; index++)
        {
            if (pool->links[index] == PoolSlotUsed)
            {
                return &pool->items[index];
            }
        }

        return nullptr;
    }
}
