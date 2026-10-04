#include "game/instances.h"

#include "game/clock.h"
#include "game/collision.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/reference.h"

extern "C"
{
    extern const GccVTableEntry g_TriggerNodeBaseVTable[] RETAIL(BaseTriggerNode_Methods);
    // The C library's sort
    void QuickSort(void* items, u32 count, u32 size, s32 (*compare)(const void*, const void*)) RETAIL(FUN_002c7bc0);
}

namespace
{
constexpr u32 NoHit = 0x7149F2CA;
constexpr u32 SlotBeginCheck = 11;
constexpr u32 SlotEntered = 12;
constexpr u32 SlotEnteredSecond = 13;
constexpr u32 SlotEnteredOrStayed = 14;
constexpr u32 SlotLeft = 15;
constexpr u32 InstanceWake = 2;

void ReleaseAll(ReferenceSet* set)
{
    for (s32 index = 31; index >= 0; index--)
    {
        RemoveReference(&set->handles[index]);
    }
}
}

extern "C"
{
s32 CompareHandles(const Reference* const* first, const Reference* const* second)
{
    const ReferencedObject* a = *first != nullptr ? (*first)->object : nullptr;
    const ReferencedObject* b = *second != nullptr ? (*second)->object : nullptr;
    return reinterpret_cast<s32>(a) - reinterpret_cast<s32>(b);
}

// The instances (awake, of the node kinds) inside an instance's first hull, the instance itself left out, sorted
ReferenceSet* GatherTriggerInstances(ReferenceSet* set, ChunkData* chunk, u32 kinds, InstanceContext* owner)
{
    for (u32 index = 0; index < 32; index++)
    {
        set->handles[index] = nullptr;
    }

    void* results[32];
    InstanceRayHit query;
    query.most = 32;
    query.results = results;
    query.count = 0;
    query.distance = __builtin_bit_cast(f32, NoHit);
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.bits = InstanceRayHit::BitAllWanted;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    Matrix4x4 matrix;
    CollisionHull* hull;
    GetInstanceHull(&owner->collision, 0, &hull, &matrix);
    SkipInQuery(&query, owner);
    u32 count = ChunkInstancesInHull(chunk, hull, &matrix, kinds, &query, 0);
    set->count = count;
    for (u32 index = 0; index < set->count; index++)
    {
        AssignReference(&set->handles[index], static_cast<ReferencedObject*>(results[index]));
    }

    set->Sort();
    return set;
}
}

ReferenceSet* ReferenceSet::Construct(ReferenceSet* set, ReferencedObject* first)
{
    for (u32 index = 0; index < 32; index++)
    {
        set->handles[index] = nullptr;
    }

    set->Clear();
    if (first == nullptr)
    {
        return set;
    }

    Reference** handle = &set->handles[set->count++];
    AssignReference(handle, first);
    return set;
}

void ReferenceSet::Destroy(u32 destroyFlags)
{
    ReleaseAll(this);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

ReferenceSet* ReferenceSet::Assign(const ReferenceSet* other)
{
    count = other->count;
    for (u32 index = 0; index < count; index++)
    {
        if (handles[index] == other->handles[index])
        {
            continue;
        }

        RemoveReference(&handles[index]);
        Reference* handle = other->handles[index];
        handles[index] = handle;
        if (handle != nullptr)
        {
            handle->value = (handle->value & ~ReferenceBits::CountMask) | (((handle->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask);
        }
    }

    return this;
}

void ReferenceSet::Clear()
{
    count = 0;
}

void ReferenceSet::Sort()
{
    if (1 < count)
    {
        QuickSort(handles, count, sizeof(Reference*), reinterpret_cast<s32 (*)(const void*, const void*)>(CompareHandles));
    }
}

void HandleWalk::Destroy(u32 destroyFlags)
{
    vtable = g_HandleWalkBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void HandleWalk::BaseDestroy(u32 destroyFlags)
{
    vtable = g_HandleWalkBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void HandleWalk::First()
{
    index = 0;
}

u32 HandleWalk::IsDone()
{
    return index >= set->count;
}

Reference* const* HandleWalk::Current()
{
    return &set->handles[index];
}

void HandleWalk::Next()
{
    Step();
}

TriggerNode* TriggerNode::Construct(TriggerNode* node, ChunkData* chunk, LayoutTrigger* trigger)
{
    constexpr u32 TriggerNotPolled = 12;
    GameNode::Construct(node);
    node->chunk = chunk;
    node->eventKinds = 0;
    node->vtable = g_TriggerNodeBaseVTable;
    InstancePlacement::Construct(&node->placement, chunk);
    ReferenceSet::Construct(&node->inside, nullptr);
    node->Bits() = 0;
    node->eventKinds = 0;
    node->unknown18 = *reinterpret_cast<const u8*>(trigger);
    u64 header = *reinterpret_cast<const u64*>(trigger);
    node->Bits() = (node->Bits() & ~NeverPolled) | static_cast<u32>((header >> TriggerNotPolled) & 1) << 20;
    return node;
}

void TriggerNode::Destroy(u32 destroyFlags)
{
    vtable = g_TriggerNodeBaseVTable;
    ReleaseAll(&inside);
    StringDestroy(&placement.chunk);
    GameNode::Destroy(destroyFlags);
}

void TriggerNode::SetOwner(InstanceContext* instance)
{
    placement.Take(instance, 1);
    GameNode::SetOwner(instance);
}

void TriggerNode::Reset()
{
    inside.count = 0;
    Bits() &= ~WasEntered;
    placement.Apply(owner);
    CallVirtual<void>(owner, owner->vtable, InstanceWake);
}

// Polled with the clock running, a check once the interval since the last one passed (else the node's time is kept)
u32 TriggerNode::Update(TimeClock* clock)
{
    if ((Bits() & NeverPolled) != 0)
    {
        return 0;
    }

    if ((static_cast<u8>(clock->flags) & TimeClock::FlagRunning) != 0)
    {
        if (!(static_cast<s32>(clock->time - time) < checkTicks))
        {
            ReferenceSet now;
            GatherTriggerInstances(&now, chunk, eventKinds, owner);
            Compare(&now);
            ReleaseAll(&now);
        }
        else
        {
            flags |= FlagKeepTime;
        }
    }

    return GameNode::Update(clock);
}

void TriggerNode::Nothing16()
{
}

// Both sets are sorted by the objects' addresses, so they're walked side by side
void TriggerNode::Compare(const ReferenceSet* now)
{
    HandleWalk fresh = {g_HandleWalkVTable, 0, now};
    HandleWalk old = {g_HandleWalkVTable, 0, &inside};
    CallVirtual<void>(this, vtable, SlotBeginCheck);
    while (!old.AtEnd() && !fresh.AtEnd())
    {
        ReferencedObject* was = old.Object();
        ReferencedObject* is = fresh.Object();
        if (was == is)
        {
            if ((Bits() & TellsEnteredAndStayed) != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredOrStayed, is);
            }

            old.Step();
            fresh.Step();
        }
        else if (reinterpret_cast<u32>(was) < reinterpret_cast<u32>(is))
        {
            if ((Bits() & TellsLeft) != 0)
            {
                CallVirtual<void>(this, vtable, SlotLeft, was);
            }

            old.Step();
        }
        else
        {
            if ((Bits() & (TellsEntered | WasEntered)) == TellsEntered)
            {
                CallVirtual<void>(this, vtable, SlotEntered, is);
            }
            else if ((Bits() & TellsEnteredSecond) != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredSecond, is);
            }
            else if ((Bits() & TellsEnteredAndStayed) != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredOrStayed, is);
            }

            fresh.Step();
        }
    }

    if ((Bits() & TellsLeft) != 0)
    {
        while (!old.AtEnd())
        {
            CallVirtual<void>(this, vtable, SlotLeft, old.Object());
            old.Step();
        }
    }

    u32 slot = 0;
    if ((Bits() & (TellsEntered | WasEntered)) == TellsEntered)
    {
        slot = SlotEntered;
    }
    else if ((Bits() & TellsEnteredSecond) != 0)
    {
        slot = SlotEnteredSecond;
    }
    else if ((Bits() & TellsEnteredAndStayed) != 0)
    {
        slot = SlotEnteredOrStayed;
    }

    if (slot != 0)
    {
        while (!fresh.AtEnd())
        {
            CallVirtual<void>(this, vtable, slot, fresh.Object());
            fresh.Step();
        }
    }

    inside.count = now->count;
    for (u32 index = 0; index < inside.count; index++)
    {
        if (inside.handles[index] == now->handles[index])
        {
            continue;
        }

        RemoveReference(&inside.handles[index]);
        Reference* handle = now->handles[index];
        inside.handles[index] = handle;
        if (handle != nullptr)
        {
            handle->value = (handle->value & ~ReferenceBits::CountMask) | (((handle->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask);
        }
    }

    u32 entered = ((Bits() >> 21) & 1) | (now->count != 0);
    Bits() = (Bits() & ~WasEntered) | entered << 21;
    fresh.vtable = g_HandleWalkBaseVTable;
    old.vtable = g_HandleWalkBaseVTable;
}

void TriggerNode::CheckWith(InstanceContext* instance)
{
    TimeClock* clock = GetContextClock(owner);
    ReferenceSet now;
    ReferenceSet::Construct(&now, instance);
    Compare(&now);
    time = clock->time;
    ReleaseAll(&now);
}
