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
// The trigger nodes' vtable functions: a check begins, an instance entered (and the second kind of entry), entered or stayed,
// and left
constexpr u32 SlotBeginCheck = 11;
constexpr u32 SlotEntered = 12;
constexpr u32 SlotEnteredSecond = 13;
constexpr u32 SlotEnteredOrStayed = 14;
constexpr u32 SlotLeft = 15;
// The handles a reference set holds
constexpr u32 SetHandles = sizeof(ReferenceSet::handles) / sizeof(Reference*);

void ReleaseAll(ReferenceSet* set)
{
    for (s32 index = SetHandles - 1; index >= 0; index--)
    {
        RemoveReference(&set->handles[index]);
    }
}
}

extern "C"
{
s32 CompareHandles(const Reference* const* first, const Reference* const* second)
{
    const ReferencedObject* firstObject = *first != nullptr ? (*first)->object : nullptr;
    const ReferencedObject* secondObject = *second != nullptr ? (*second)->object : nullptr;
    return reinterpret_cast<s32>(firstObject) - reinterpret_cast<s32>(secondObject);
}

// The instances (awake, of the node kinds) inside an instance's first hull, the instance itself left out, sorted
ReferenceSet* GatherTriggerInstances(ReferenceSet* set, ChunkData* chunk, u32 kinds, InstanceContext* owner)
{
    for (u32 index = 0; index < ReferenceSet::MostHandles; index++)
    {
        set->handles[index] = nullptr;
    }

    void* results[ReferenceSet::MostHandles];
    InstanceQuery query;
    query.most = ReferenceSet::MostHandles;
    query.results = results;
    query.count = 0;
    query.distance = NoHitDistance;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.bits.value = InstanceQueryBits::AllWanted;
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
    for (u32 index = 0; index < MostHandles; index++)
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
            handle->bits.count++;
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
    GameNode::Construct(node);
    node->chunk = chunk;
    node->eventKinds = 0;
    node->vtable = g_TriggerNodeBaseVTable;
    InstancePlacement::Construct(&node->placement, chunk);
    ReferenceSet::Construct(&node->inside, nullptr);
    node->bits.value = 0;
    node->eventKinds = 0;
    node->bits.kind = trigger->header.kind;
    node->bits.neverPolled = trigger->header.notPolled;
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
    bits.wasEntered = 0;
    placement.Apply(owner);
    CallVirtual<void>(owner, owner->vtable, InstanceContext::WakeSlot);
}

// Polled with the clock running, a check once the interval since the last one passed (else the node's time is kept)
u32 TriggerNode::Update(TimeClock* clock)
{
    if (bits.neverPolled != 0)
    {
        return 0;
    }

    if (clock->flags.running != 0)
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
            flags.keepsTime = 1;
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
            if (bits.tellsEntryOrStay != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredOrStayed, is);
            }

            old.Step();
            fresh.Step();
        }
        else if (reinterpret_cast<u32>(was) < reinterpret_cast<u32>(is))
        {
            if (bits.tellsExit != 0)
            {
                CallVirtual<void>(this, vtable, SlotLeft, was);
            }

            old.Step();
        }
        else
        {
            if (bits.tellsFirstEntry != 0 && bits.wasEntered == 0)
            {
                CallVirtual<void>(this, vtable, SlotEntered, is);
            }
            else if (bits.tellsEveryEntry != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredSecond, is);
            }
            else if (bits.tellsEntryOrStay != 0)
            {
                CallVirtual<void>(this, vtable, SlotEnteredOrStayed, is);
            }

            fresh.Step();
        }
    }

    if (bits.tellsExit != 0)
    {
        while (!old.AtEnd())
        {
            CallVirtual<void>(this, vtable, SlotLeft, old.Object());
            old.Step();
        }
    }

    u32 slot = 0;
    if (bits.tellsFirstEntry != 0 && bits.wasEntered == 0)
    {
        slot = SlotEntered;
    }
    else if (bits.tellsEveryEntry != 0)
    {
        slot = SlotEnteredSecond;
    }
    else if (bits.tellsEntryOrStay != 0)
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
            handle->bits.count++;
        }
    }

    bits.wasEntered = bits.wasEntered | (now->count != 0);
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
