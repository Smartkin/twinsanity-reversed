#include "game/instances.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/chunkloading.h"
#include "game/controllers.h"
#include "game/events.h"
#include "game/list.h"
#include "game/memory.h"
#include "game/place.h"

namespace
{
// The queued events' links
constexpr u32 EventPrevious = 0x4 + 1;
constexpr u32 EventNext = 0x8 + 1;
// The node iterator's own vtable functions: whether it's done, to the next node
constexpr u32 IteratorIsDoneSlot = 3;
constexpr u32 IteratorNextSlot = 5;
// The size of an object's place
constexpr u32 ObjectPlaceSize = 0x70;
// The links of the nodes' lists and of the instances' (GCC 2.9x member pointers: their offsets plus 1)
constexpr u32 NodePrevious = 0xC + 1;
constexpr u32 NodeNext = 0x10 + 1;
constexpr u32 InstancePrevious = 0x140 + 1;
constexpr u32 InstanceNext = 0x144 + 1;
constexpr u8 NoKind = 0xFF;
// UnregisterNode's second argument: the node detached from its instance too
constexpr u32 DetachNode = 1;
// A new instance's clock (none yet)
constexpr u8 NoClock = 0xFF;
// The kinds stepped at the start, in their order (0 to 10)
constexpr u32 FirstStepKinds = 11;

u32 KindOf(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, GameNode::KindSlot);
}

// The reference arrays' start: 10 handles, none set
constexpr u32 ReferenceArrayGrowth = 10;
}

extern "C"
{
    // The items' builders' base (BuilderBaseFunctions), and a word the module's static initialisation clears, which nothing reads
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    extern u32 g_UnreadInstancesWord RETAIL(D_0030A7F0);

    // The module's static initialisation (GCC 2.9x's: the call to initialise with priority 0xFFFF) and the constructor running it
    void InitInstancesStatics(u32 initialize, u32 priority) RETAIL(FUN_0019a238);
    void InstancesStaticConstructor() RETAIL(FUN_0019b148);

    // An item builder of the module's that makes nothing (its vtable D_002F5F60 alone, which nothing makes): no item of any
    // class, and its destructor
    void* MakeNoItem(void* builder, u32 classId) RETAIL(FUN_00199d38);
    void DestroyNoItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0019aae0);
}

namespace
{
// A handle pointed at another's reference (a reference taken, its old one let go), as GCC 2.9x's handles assign
void CopyHandle(Reference** to, Reference* const* from)
{
    if (*to == *from)
    {
        return;
    }

    RemoveReference(to);
    *to = *from;
    Reference* reference = *from;
    if (reference != nullptr)
    {
        reference->bits.count++;
    }
}

void ConstructReferenceArray(ReferenceArray* array)
{
    array->count = 0;
    array->capacity = ReferenceArrayGrowth;
    array->growth = ReferenceArrayGrowth;
    Reference** data = NewArray<Reference*>(ReferenceArrayGrowth);
    for (u32 index = 0; index < ReferenceArrayGrowth; index++)
    {
        data[index] = nullptr;
    }

    array->data = data;
}
}

GameNode* GameNode::Construct(GameNode* node)
{
    node->owner = nullptr;
    node->vtable = g_GameNodeVTable;
    node->flags.value = 0;
    node->nearDistance = 0;
    node->time = 0;
    node->previous = nullptr;
    node->next = nullptr;
    return node;
}

void GameNode::Destroy(u32 destroyFlags)
{
    vtable = g_GameNodeVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GameNode::HandleEvent(Reference** event)
{
    ReleaseEvent(event);
}

void GameNode::SetOwner(InstanceContext* instance)
{
    if (owner == nullptr)
    {
        owner = instance;
    }
}

u32 GameNode::CanChangeChunk(ChunkData*, ChunkLinkData*)
{
    return 1;
}

void GameNode::LeftChunk(u32)
{
}

void GameNode::Step(TimeClock* clock, u32)
{
    time = clock->time;
}

void GameNode::Removed()
{
}

u32 GameNode::Update(TimeClock* clock)
{
    if (clock->flags.running == 0)
    {
        return 1;
    }

    if (flags.keepsTime == 0 || time == 0)
    {
        time = clock->time;
    }
    else
    {
        flags.keepsTime = 0;
    }

    return 1;
}

NodeList* NodeList::Construct(NodeList* list)
{
    list->mask = 0;
    for (GameNode*& node : list->nodes)
    {
        node = nullptr;
    }

    return list;
}

void NodeList::Destroy(u32 destroyFlags)
{
    for (GameNode*& node : nodes)
    {
        if (node != nullptr)
        {
            CallVirtual<void>(node, node->vtable, GameNode::DestroySlot, u32{DestroyAndFree});
        }

        node = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void NodeList::LeftChunk(u32 way)
{
    for (GameNode* node : nodes)
    {
        if (node != nullptr)
        {
            CallVirtual<void>(node, node->vtable, GameNode::LeftChunkSlot, way);
        }
    }
}

void NodeList::Step(TimeClock* clock, u32 way)
{
    for (GameNode* node : nodes)
    {
        if (node != nullptr)
        {
            CallVirtual<void>(node, node->vtable, GameNode::StepSlot, clock, way);
        }
    }
}

u32 NodeList::Add(GameNode* node)
{
    u32 kind = KindOf(node);
    if (nodes[kind] != nullptr)
    {
        return 0;
    }

    mask |= 1u << kind;
    nodes[kind] = node;
    return 1;
}

u32 NodeList::Remove(GameNode* node)
{
    u32 kind = KindOf(node);
    if (nodes[kind] == nullptr)
    {
        return 0;
    }

    mask &= ~(1u << kind);
    nodes[kind] = nullptr;
    return 1;
}

void* GetGameNode(NodeList* list, u32 kind)
{
    return list->nodes[kind & 0xFF];
}

ReferencedObject* ReferencedObject::Construct(ReferencedObject* object)
{
    object->unused00 = 0;
    object->flags.value = 0;
    object->vtable = g_ReferencedObjectVTable;
    ConstructObjectCollision(&object->collision, object);
    object->chunk = nullptr;
    object->place = ConstructObjectPlace(static_cast<ObjectPlace*>(MemoryAllocate(ObjectPlaceSize)));
    ResetObjectPlace(object->place);
    return object;
}

void ReferencedObject::BaseDestroy(u32 destroyFlags)
{
    vtable = g_ReferencedObjectVTable;
    MemoryDeallocate2_(place);
    DestroyObjectCollision(&collision, DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 ReferencedObject::Wake()
{
    u32 wasAsleep = flags.asleep;
    flags.asleep = 0;
    return wasAsleep;
}

u32 ReferencedObject::Sleep()
{
    u32 wasAwake = !flags.asleep;
    flags.asleep = 1;
    return wasAwake;
}

u32 ReferencedObject::Release()
{
    if (flags.released)
    {
        return 0;
    }

    flags.released = 1;
    return 1;
}

void ReferencedObject::StepQueued()
{
    flags.queued = 0;
}

void QueueObject(ReferencedObject* object)
{
    if (object->flags.queued)
    {
        return;
    }

    object->flags.queued = 1;
    Reference* handle = object != nullptr ? AddReference(object) : nullptr;
    ReferenceArrayAppend(&g_QueuedObjects, &handle);
}

u32 ReferenceArrayAppend(ReferenceArray* array, Reference** handle)
{
    if (array->count == array->capacity)
    {
        u32 grown = array->count + array->growth;
        Reference** larger = NewArray<Reference*>(grown);
        for (u32 index = 0; index < grown; index++)
        {
            larger[index] = nullptr;
        }

        for (u32 index = 0; index < array->count; index++)
        {
            CopyHandle(&larger[index], &array->data[index]);
        }

        if (array->data != nullptr)
        {
            Reference** old = array->data + ArrayCount(array->data);
            while (old != array->data)
            {
                old--;
                RemoveReference(old);
            }

            DeleteArray(array->data);
        }

        array->data = larger;
        array->capacity = grown;
    }

    u32 index = array->count++;
    CopyHandle(&array->data[index], handle);
    u32 last = array->count - 1;
    RemoveReference(handle);
    return last;
}

InstanceContext* InstanceContext::Construct(InstanceContext* instance)
{
    ReferencedObject::Construct(instance);
    instance->reference = nullptr;
    instance->parent = nullptr;
    instance->vtable = g_InstanceContextVTable;
    NodeList::Construct(&instance->nodes);
    instance->events = nullptr;
    instance->seen = InstanceContext::NeverSeen;
    instance->places = nullptr;
    instance->previous = nullptr;
    instance->next = nullptr;
    instance->cellPrevious = nullptr;
    instance->cellNext = nullptr;

    instance->id = InstanceContext::NoId;
    instance->box = g_DefaultBox.min;
    instance->clockIndex = NoClock;
    instance->box.w = 1.0f;
    return instance;
}

void InstanceContext::Destroy(u32 destroyFlags)
{
    vtable = g_InstanceContextVTable;
    InstancePlaces* block = places;
    if (block != nullptr)
    {
        for (s32 index = InstancePlaces::MostPlaces - 1; index >= 0; index--)
        {
            StringDestroy(&block->places[index].chunk);
        }

        StringDestroy(&block->lastChunk);
        MemoryDeallocate2_(block);
    }

    ClearEvents();
    nodes.Destroy(DestroyOnly);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    BaseDestroy(destroyFlags);
}

void InstanceContext::LeaveChunk(u32 way)
{
    nodes.LeftChunk(way);
    chunk = nullptr;
}

void InstanceContext::Step(TimeClock* clocks, u32 way)
{
    TimeClock* clock = &clocks[clockIndex];
    ClearEvents();
    nodes.Step(clock, way);
}

void InstanceContext::ClearEvents()
{
    QueuedEvent* event = events;
    events = nullptr;
    while (event != nullptr)
    {
        QueuedEvent* next = event->next;
        ReleaseEvent(&event->event);
        MemoryDeallocate2_(event);
        event = next;
    }
}

u32 RegisterNode(InstanceContext* instance, u32 attach, void* node)
{
    if (attach != 0)
    {
        auto* gameNode = static_cast<GameNode*>(node);
        instance->nodes.Add(gameNode);
        CallVirtual<void>(gameNode, gameNode->vtable, GameNode::SetOwnerSlot, instance);
    }

    ChunkData* chunk = instance->chunk;
    if (chunk == nullptr)
    {
        return 1;
    }

    return chunk->instances->AddNode(static_cast<GameNode*>(node));
}

u32 UnregisterNode(InstanceContext* instance, u32 detach, void* node)
{
    if (detach != 0)
    {
        instance->nodes.Remove(static_cast<GameNode*>(node));
    }

    ChunkData* chunk = instance->chunk;
    if (chunk == nullptr)
    {
        return 1;
    }

    return chunk->instances->RemoveNode(static_cast<GameNode*>(node));
}

u32 RemoveNode(InstanceContext* instance, void* node)
{
    u32 removed = UnregisterNode(instance, DetachNode, node) & 0xFF;
    FreeNode(node);
    return removed;
}

TimeClock* GetContextClock(InstanceContext* instance)
{
    TimeClock* clocks = G_GameClockController->clocks;
    ChunkData* chunk = instance->chunk;
    if (chunk != nullptr && chunk->clocks != nullptr)
    {
        clocks = chunk->clocks;
    }

    return &clocks[instance->clockIndex];
}

void ListPushFront(void* node, void** head, u32 previous, u32 next)
{
    RetailList::PushFront(node, head, previous, next);
}

void ListRemove(void* node, void** head, u32 previous, u32 next)
{
    RetailList::Remove(node, head, previous, next);
}

void InstanceListPushFront(void* node, void** head, u32 previous, u32 next)
{
    RetailList::PushFront(node, head, previous, next);
}

void NodeListPushFront(void* node, void** head, u32 previous, u32 next)
{
    RetailList::PushFront(node, head, previous, next);
}

void NodeListRemove(void* node, void** head, u32 previous, u32 next)
{
    RetailList::Remove(node, head, previous, next);
}

void FreeInstance(InstanceContext* instance)
{
    InstanceListPushFront(instance, reinterpret_cast<void**>(&g_InstancesToFree), InstancePrevious, InstanceNext);
}

void FreeNode(void* node)
{
    NodeListPushFront(node, reinterpret_cast<void**>(&g_NodesToFree), NodePrevious, NodeNext);
}

s32 StepIndexOf(u32 kind)
{
    u8 wanted = static_cast<u8>(kind);
    for (u32 index = 0; index < g_StepKindCount; index++)
    {
        if (g_StepKinds[index] == wanted)
        {
            return static_cast<s32>(index);
        }
    }

    return -1;
}

void NodeIterator::BaseDestroy(u32 destroyFlags)
{
    vtable = g_NodeIteratorBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void NodeIterator::Destroy(u32 destroyFlags)
{
    vtable = g_NodeIteratorBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void NodeIterator::First()
{
    index = 0;
    if ((list->mask & 1) == 0)
    {
        CallVirtual<void>(this, vtable, IteratorNextSlot);
    }
}

u32 NodeIterator::IsDone()
{
    return (list->mask & ~0u << (index & ShiftMask)) == 0;
}

GameNode** NodeIterator::Current()
{
    return &list->nodes[index];
}

void NodeIterator::Next()
{
    if (CallVirtual<u32>(this, vtable, IteratorIsDoneSlot) != 0)
    {
        return;
    }

    do
    {
        index++;
    } while ((list->mask & 1u << (index & ShiftMask)) == 0 && CallVirtual<u32>(this, vtable, IteratorIsDoneSlot) == 0);
}

ChunkInstances* ChunkInstances::Construct(ChunkInstances* chunkInstances)
{
    chunkInstances->sleeping = nullptr;
    chunkInstances->steppingKind = NoKind;
    for (GameNode*& node : chunkInstances->nodes)
    {
        node = nullptr;
    }

    return chunkInstances;
}

void ChunkInstances::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 ChunkInstances::HasNoNodes()
{
    for (GameNode* node : nodes)
    {
        if (node != nullptr)
        {
            return 0;
        }
    }

    return 1;
}

u32 ChunkInstances::IsEmpty()
{
    if (sleeping != nullptr)
    {
        return 0;
    }

    return HasNoNodes();
}

u32 ChunkInstances::AddNode(GameNode* node)
{
    u32 kind = KindOf(node);
    if (node->flags.listed != 0)
    {
        return 0;
    }

    node->flags.listed = 1;
    NodeListPushFront(node, reinterpret_cast<void**>(&nodes[kind]), NodePrevious, NodeNext);
    return 1;
}

u32 ChunkInstances::RemoveNode(GameNode* node)
{
    if (node->flags.listed == 0)
    {
        return 0;
    }

    u32 kind = KindOf(node);
    if (kind == steppingKind)
    {
        g_NodesTakenOut[g_NodesTakenOutCount] = node;
        g_NodesTakenOutCount++;
    }
    else
    {
        NodeListRemove(node, reinterpret_cast<void**>(&nodes[kind]), NodePrevious, NodeNext);
        CallVirtual<void>(node, node->vtable, GameNode::RemovedSlot);
    }

    node->flags.listed = 0;
    return 1;
}

void ChunkInstances::AddNodes(InstanceContext* instance)
{
    NodeList& list = instance->nodes;
    for (u32 kind = 0; (list.mask & ~0u << (kind & ShiftMask)) != 0; kind++)
    {
        if ((list.mask & 1u << (kind & ShiftMask)) != 0)
        {
            AddNode(list.nodes[kind]);
        }
    }
}

void ChunkInstances::RemoveNodes(InstanceContext* instance)
{
    NodeList& list = instance->nodes;
    for (u32 kind = 0; (list.mask & ~0u << (kind & ShiftMask)) != 0; kind++)
    {
        if ((list.mask & 1u << (kind & ShiftMask)) != 0)
        {
            RemoveNode(list.nodes[kind]);
        }
    }
}

u32 ChunkInstances::WakeInstance(InstanceContext* instance)
{
    if (instance->flags.asleep)
    {
        ListRemove(instance, reinterpret_cast<void**>(&sleeping), InstancePrevious, InstanceNext);
    }

    AddNodes(instance);
    return 1;
}

u32 ChunkInstances::SleepInstance(InstanceContext* instance)
{
    if (!instance->flags.asleep)
    {
        InstanceListPushFront(instance, reinterpret_cast<void**>(&sleeping), InstancePrevious, InstanceNext);
    }

    RemoveNodes(instance);
    return 1;
}

u32 ChunkInstances::AddInstance(InstanceContext* instance)
{
    if (instance->flags.asleep)
    {
        InstanceListPushFront(instance, reinterpret_cast<void**>(&sleeping), InstancePrevious, InstanceNext);
    }

    AddNodes(instance);
    return 1;
}

u32 ChunkInstances::RemoveInstance(InstanceContext* instance)
{
    if (instance->flags.asleep)
    {
        ListRemove(instance, reinterpret_cast<void**>(&sleeping), InstancePrevious, InstanceNext);
    }

    RemoveNodes(instance);
    return 1;
}

void ChunkInstances::ReleaseInstance(InstanceContext* instance)
{
    RemoveNodes(instance);
    FreeInstance(instance);
}

u32 ChunkInstances::Update(GameTimeController* clock, void* clocks, s32 kindMask)
{
    u32 updated = 0;
    if (g_StepKindCount == 0)
    {
        return 0;
    }

    auto* kindClocks = static_cast<TimeClock*>(clocks);
    if (kindClocks == nullptr)
    {
        kindClocks = clock->clocks;
    }

    for (u32 index = 0; index < g_StepKindCount; index++)
    {
        u32 kind = g_StepKinds[index];
        u32 bit = 1u << (kind & ShiftMask);
        if ((static_cast<u32>(kindMask) & bit) != bit)
        {
            continue;
        }

        GameNode* node = nodes[kind];
        if (node == nullptr)
        {
            continue;
        }

        steppingKind = static_cast<u8>(kind);
        while (node != nullptr)
        {
            GameNode* next = node->next;
            TimeClock* nodeClock = &kindClocks[node->owner->clockIndex];
            if (CallVirtual<u32>(node, node->vtable, GameNode::UpdateSlot, nodeClock) != 0)
            {
                updated++;
            }
            else
            {
                RemoveNode(node);
            }

            node = next;
        }

        for (u32 taken = 0; taken < g_NodesTakenOutCount; taken++)
        {
            GameNode* out = g_NodesTakenOut[taken];
            NodeListRemove(out, reinterpret_cast<void**>(&nodes[steppingKind]), NodePrevious, NodeNext);
            CallVirtual<void>(out, out->vtable, GameNode::RemovedSlot);
        }

        steppingKind = NoKind;
        g_NodesTakenOutCount = 0;
        if (g_QueuedObjects.count != 0)
        {
            StepQueuedObjects();
        }
    }

    return updated;
}

void ChunkInstances::Release()
{
    InstanceContext* instance = sleeping;
    while (instance != nullptr)
    {
        InstanceContext* next = instance->next;
        CallVirtual<void>(instance, instance->vtable, InstanceContext::ReleaseSlot);
        instance = next;
    }
}

u32 FreeOne()
{
    GameNode* node = g_NodesToFree;
    if (node != nullptr)
    {
        g_NodesToFree = node->next;
        CallVirtual<void>(node, node->vtable, GameNode::DestroySlot, u32{DestroyAndFree});
        return 1;
    }

    InstanceContext* instance = g_InstancesToFree;
    if (instance == nullptr)
    {
        return 0;
    }

    ListRemove(instance, reinterpret_cast<void**>(&g_InstancesToFree), InstancePrevious, InstanceNext);
    instance->Destroy(DestroyAndFree);
    return 1;
}

void MakeGlobal(u32 way, InstanceContext* instance)
{
    instance->LeaveChunk(way);
    InstanceListPushFront(instance, reinterpret_cast<void**>(&g_GlobalInstances), InstancePrevious, InstanceNext);
}

void StepQueuedObjects()
{
    for (s32 index = 0; index >= 0 && static_cast<u32>(index) < g_QueuedObjects.count; index++)
    {
        Reference* handle = g_QueuedObjects.data[index];
        ReferencedObject* object = handle != nullptr ? handle->object : nullptr;
        if (object != nullptr)
        {
            CallVirtual<void>(object, object->vtable, ReferencedObject::StepQueuedSlot);
        }
    }

    g_QueuedObjects.count = 0;
}

void StepQueuedObjects2()
{
    StepQueuedObjects();
}

void DeliverEvents()
{
    for (s32 index = 0; index >= 0 && static_cast<u32>(index) < g_InstancesWithEvents.count; index++)
    {
        Reference* handle = g_InstancesWithEvents.data[index];
        auto* instance = handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
        if (instance != nullptr)
        {
            HandOutEvents(instance);
        }
    }

    g_InstancesWithEvents.count = 0;
}

void DropEvents()
{
    for (s32 index = 0; index >= 0 && static_cast<u32>(index) < g_InstancesWithEvents.count; index++)
    {
        Reference* handle = g_InstancesWithEvents.data[index];
        auto* instance = handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
        if (instance != nullptr)
        {
            instance->ClearEvents();
        }
    }

    g_InstancesWithEvents.count = 0;
}

void UpdateGlobalInstances()
{
    InstanceContext* instance = g_GlobalInstances;
    if (instance == nullptr)
    {
        return;
    }

    TimeClock* clocks = G_GameClockController->clocks;
    while (instance != nullptr)
    {
        InstanceContext* next = instance->next;
        ListRemove(instance, reinterpret_cast<void**>(&g_GlobalInstances), InstancePrevious, InstanceNext);
        instance->Step(clocks, g_ResetWay);
        if (instance->chunk == nullptr && instance->flags.released)
        {
            CallVirtual<void>(instance, instance->vtable, InstanceContext::ReleaseSlot);
        }

        instance = next;
    }
}

void UpdateInstances()
{
    UpdateGlobalInstances();
    DeliverEvents();
}

void FreeAll(u32 way)
{
    g_ResetWay = way;
    DropEvents();
    StepQueuedObjects2();
    while (FreeOne() != 0)
    {
    }
}

void ReleaseGlobalInstances()
{
    InstanceContext* instance = g_GlobalInstances;
    while (instance != nullptr)
    {
        InstanceContext* next = instance->next;
        ListRemove(instance, reinterpret_cast<void**>(&g_GlobalInstances), InstancePrevious, InstanceNext);
        CallVirtual<void>(instance, instance->vtable, InstanceContext::ReleaseSlot);
        instance = next;
    }

    FreeOne();
}

u32 InstanceContext::Wake()
{
    if (chunk != nullptr)
    {
        ChunkWakeInstance(chunk, this);
    }

    ReferencedObject::Wake();
    return 1;
}

u32 InstanceContext::Sleep()
{
    if (id != NoId)
    {
        return CallVirtual<u32>(this, vtable, ReleaseSlot);
    }

    if (chunk != nullptr)
    {
        ChunkSleepInstance(chunk, this);
    }

    return ReferencedObject::Sleep();
}

u32 InstanceContext::Release()
{
    u32 placesLetGo = 1;
    if (places != nullptr && id == NoId)
    {
        placesLetGo = ReleasePlaces(places, this);
    }

    if (placesLetGo == 0)
    {
        return 1;
    }

    if (ReferencedObject::Release() == 0)
    {
        return 0;
    }

    if (id != NoId)
    {
        g_InstanceIds->Remove(this);
    }

    if (chunk == nullptr)
    {
        FreeInstance(this);
    }
    else
    {
        ChunkReleaseInstance(chunk, this);
    }

    return ReferencedObject::Sleep();
}

void InstanceContext::StepQueued()
{
    bool stepped = !flags.asleep && StepObjectCollision(&collision) == nullptr;
    ObjectPlace* objectPlace = place;
    if (stepped)
    {
        objectPlace->SyncPosition();
        bool moved = false;
        if (!(box.x == objectPlace->position.x && box.y == objectPlace->position.y && objectPlace->position.z == box.z))
        {
            objectPlace->MarkMoved();
            objectPlace->position = box;
            moved = true;
        }

        if (moved)
        {
            QueueObject(this);
        }

        StepObjectCollision(&collision);
    }
    else
    {
        objectPlace->SyncPosition();
        box = objectPlace->position;
    }

    ReferencedObject::StepQueued();
}

ChunkData* InstanceContext::ChangeChunk(ChunkLinkData* link)
{
    u32 allowed = 1;
    for (u32 kind = 0; (nodes.mask & ~0u << (kind & ShiftMask)) != 0; kind++)
    {
        if ((nodes.mask & 1u << (kind & ShiftMask)) == 0)
        {
            continue;
        }

        GameNode* node = nodes.nodes[kind];
        allowed &= CallVirtual<u32>(node, node->vtable, GameNode::CanChangeChunkSlot, chunk, link) != 0 ? 1 : 0;
    }

    if (allowed == 0)
    {
        return nullptr;
    }

    MoveThroughLink(link, this);
    LinkedChunkTakeInstance(link, this);
    return chunk;
}

// A copy of an event's handle (one more reference)
Reference* CopyEvent(Reference* event)
{
    if (event != nullptr)
    {
        event->bits.count++;
    }

    return event;
}

void QueueEvent(ReferencedObject* object, Reference** event)
{
    auto* instance = static_cast<InstanceContext*>(object);
    auto* entry = static_cast<QueuedEvent*>(MemoryAllocate(sizeof(QueuedEvent)));
    Reference* copy = CopyEvent(*event);
    entry->event = CopyEvent(copy);
    entry->previous = nullptr;
    entry->next = nullptr;
    ReleaseEvent(&copy);
    if (instance->events == nullptr)
    {
        Reference* handle = instance != nullptr ? AddReference(instance) : nullptr;
        ReferenceArrayAppend(&g_InstancesWithEvents, &handle);
    }

    ListPushFront(entry, reinterpret_cast<void**>(&instance->events), EventPrevious, EventNext);
    ReleaseEvent(event);
}

void HandOutEvents(InstanceContext* instance)
{
    QueuedEvent* entry = instance->events;
    instance->events = nullptr;
    while (entry != nullptr)
    {
        QueuedEvent* next = entry->next;
        Reference* event = CopyEvent(entry->event);
        NodeList& list = instance->nodes;
        for (u32 kind = 0; (list.mask & ~0u << (kind & ShiftMask)) != 0; kind++)
        {
            if ((list.mask & 1u << (kind & ShiftMask)) == 0)
            {
                continue;
            }

            GameNode* node = list.nodes[kind];
            if (node == nullptr)
            {
                continue;
            }

            u32 bit = 1u << (KindOf(node) & 0x1F);
            auto* gameEvent = reinterpret_cast<GameEvent*>(event != nullptr ? event->object : nullptr);
            if ((bit & gameEvent->kinds) != 0)
            {
                Reference* copy = CopyEvent(event);
                CallVirtual<void>(node, node->vtable, GameNode::HandleEventSlot, &copy);
            }
        }

        ReleaseEvent(&entry->event);
        MemoryDeallocate2_(entry);
        ReleaseEvent(&event);
        entry = next;
    }
}

void ChunkInstances::MakeGlobal(u32 way, InstanceContext* instance)
{
    ListRemove(instance, reinterpret_cast<void**>(&sleeping), InstancePrevious, InstanceNext);
    ::MakeGlobal(way, instance);
}

void ChunkInstances::MakeGlobalWhere(u32 way, const u32* filter)
{
    InstanceContext* instance = sleeping;
    while (FreeOne() != 0)
    {
    }

    while (instance != nullptr)
    {
        InstanceContext* next = instance->next;
        bool matches = false;
        if ((instance->flags.value & filter[1]) == filter[1] && (instance->flags.value & filter[2]) == 0)
        {
            matches = (instance->nodes.mask & filter[0]) != 0;
        }

        if (matches)
        {
            MakeGlobal(way, instance);
        }

        instance = next;
    }
}

void InitStepKinds()
{
    g_StepKindCount = FirstStepKinds;
    for (u32 kind = 0; kind < FirstStepKinds; kind++)
    {
        g_StepKinds[kind] = static_cast<u8>(kind);
    }
}

// Without the loops made into calls of memmove, which the game doesn't have
__attribute__((optimize("no-tree-loop-distribute-patterns"))) u32 InsertStepKindBefore(u32 kind, u32 before)
{
    s32 index = StepIndexOf(before & 0xFF);
    for (s32 at = static_cast<s32>(g_StepKindCount); index < at; at--)
    {
        g_StepKinds[at] = g_StepKinds[at - 1];
    }

    g_StepKinds[index] = static_cast<u8>(kind);
    g_StepKindCount++;
    return index >= 0;
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) u32 InsertStepKindAfter(u32 kind, u32 after)
{
    s32 index = StepIndexOf(after & 0xFF);
    for (s32 at = static_cast<s32>(g_StepKindCount); index + 1 < at; at--)
    {
        g_StepKinds[at] = g_StepKinds[at - 1];
    }

    g_StepKinds[index + 1] = static_cast<u8>(kind);
    g_StepKindCount++;
    return index >= 0;
}

u32 FreeOneAgain()
{
    return FreeOne() != 0;
}

u32 HasParent(InstanceContext* instance, InstanceContext* other)
{
    InstanceContext* parent = instance->parent;
    while (parent != nullptr && parent != other)
    {
        parent = parent->parent;
    }

    return parent == other;
}

void SetObjectPlace(ReferencedObject* object, const ObjectPlace* place)
{
    *object->place = *place;
}

void* UpdateInstanceChunk(InstanceContext* instance)
{
    ChunkData* chunk = instance->chunk;
    if (chunk == nullptr)
    {
        return nullptr;
    }

    if (!instance->flags.movesBetweenChunks)
    {
        return chunk;
    }

    return ChunkUpdateInstanceChunk(chunk, instance);
}

void* UpdateObjectMatrix(ReferencedObject* object)
{
    void* value = object->collision.hullMatrix;
    if (value != nullptr)
    {
        return value;
    }

    ObjectPlace* place = object->place;
    RotateAndTranslate(place);
    return place;
}

void MovementNode::Capture()
{
    seconds = SecondsPerFrame;
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    previousMatrix = place->matrix;
    place = owner->place;
    RotateAndTranslate(place);
    matrix = place->matrix;
    bits.captured = 1;
}

Matrix4x4* MovementNode::PreviousMatrix()
{
    if (bits.captured == 0)
    {
        Capture();
    }

    return &previousMatrix;
}

Matrix4x4* MovementNode::CurrentMatrix()
{
    if (bits.captured == 0)
    {
        Capture();
    }

    return &matrix;
}

f32 MovementNode::Seconds()
{
    if (bits.captured == 0)
    {
        Capture();
    }

    return seconds;
}

InstanceIds* InstanceIds::Construct(InstanceIds* ids)
{
    ids->count = 0;
    for (Entry& entry : ids->entries)
    {
        entry.instance = nullptr;
        entry.spawner = nullptr;
    }

    return ids;
}

void InstanceIds::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void InstanceIds::Add(InstanceContext* instance)
{
    if (count >= MostIds)
    {
        return;
    }

    entries[count].instance = instance;
    u16 id = count;
    count = id + 1;
    instance->id = id;
}

void InstanceIds::Remove(InstanceContext* instance)
{
    if (count == 0)
    {
        return;
    }

    count--;
    u32 last = count;
    u32 id = static_cast<u32>(instance->id);
    if (id < last)
    {
        entries[id] = entries[last];
        entries[id].instance->id = static_cast<s32>(id);
    }

    instance->id = InstanceContext::NoId;
}

void TriggerNode::AddInstance(InstanceContext* instance)
{
    instances[bits.instanceCount++] = instance;
}

void InitInstancesStatics(u32 initialize, u32 priority)
{
    if (priority != DefaultInitPriority || initialize == 0)
    {
        return;
    }

    TimeClockReset(&g_GlobalClock);
    g_UnreadInstancesWord = 0;
    ConstructReferenceArray(&g_QueuedObjects);
    ConstructReferenceArray(&g_InstancesWithEvents);
}

void InstancesStaticConstructor()
{
    InitInstancesStatics(1, DefaultInitPriority);
}

void* MakeNoItem(void*, u32)
{
    return nullptr;
}

void DestroyNoItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}
