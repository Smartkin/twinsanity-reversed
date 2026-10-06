#include "game/scenery.h"
#include "game/volumes.h"

#include "game/chunkdata.h"
#include "game/dynamicscenery.h"
#include "game/collision.h"
#include "game/graphicstables.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/lights.h"
#include "game/list.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/readers.h"
#include "game/stream.h"
#include "game/string.h"
#include "platform/graphics.h"
#include "retail/libc.h"

EABI_EXPORT(FUN_001e9e90, &SceneryMeshes::SetBox);

extern "C"
{
    extern const GccVTableEntry g_SceneryCellReaderVTable[] RETAIL(SceneryTypeSection_Reader);
    extern const GccVTableEntry g_SceneryReleaseReaderVTable[] RETAIL(D_002FBE60);
    extern const GccVTableEntry g_SceneryDestroyReaderVTable[] RETAIL(D_002FBE38);
}


namespace
{
// The links an instance is on a cell's list by (InstanceContext::previous and next), as GCC 2.9x member pointers: their offsets
// plus 1
constexpr u32 ListPrevious = 0x140 + 1;
constexpr u32 ListNext = 0x144 + 1;
constexpr s32 PathEnd = -1;
// The boxes, items and matrices of a cell's meshes are aligned to the cache's lines
constexpr u32 CacheLineSize = 0x40;
// The depth left below which a node's children are leaves
constexpr s32 LeafDepth = 2;

// A cache line pulled in ahead of its use
inline void Touch(const void* address)
{
#if defined(_EE)
    asm volatile("lb $0, 0(%0)" : : "r"(address));
#else
    static_cast<void>(*static_cast<const volatile u8*>(address));
#endif
}

void SetSeen(InstanceContext* instance, u32 seen)
{
    instance->seen = seen;
}

void ClearSeen(InstanceContext* instance)
{
    instance->seen = 0;
}

bool Draws(InstanceContext* instance)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    return model != nullptr && model->drawnOgi != nullptr;
}

void QueueInView(InstanceContext* instance)
{
    g_DrawnInstances[g_DrawnInViewCount++] = instance;
    g_DrawnInstanceCount++;
}

void QueueClipped(InstanceContext* instance)
{
    g_DrawnInstances[g_DrawnClippedEnd--] = instance;
    g_DrawnInstanceCount++;
}

bool IsSolid(InstanceContext* instance)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    return model != nullptr && model->bits.solid != 0;
}

SceneryCell* MakeLeaf()
{
    auto* leaf = static_cast<SceneryLeaf*>(MemoryAllocate(sizeof(SceneryLeaf)));
    leaf->ConstructBase();
    leaf->parent = nullptr;
    leaf->meshes = nullptr;
    leaf->chunk = nullptr;
    for (u32 index = 0; index < sizeof(leaf->lights); index++)
    {
        leaf->lights[index] = 0;
    }

    leaf->vtable = g_SceneryLeafVTable;
    return leaf;
}

SceneryNode* MakeNode(u32 size)
{
    auto* node = static_cast<SceneryNode*>(MemoryAllocate(size));
    SceneryTree::Construct(node);
    node->vtable = g_SceneryNodeVTable;
    node->ClearChildren();
    return node;
}

}

extern "C"
{
// The instances a view's culling draws (the next one's test started while the last one's outcome is read): a solid model's
// instance is always drawn clipped with its seen stamp cleared, the others get the culling's distance as their stamp
u32 DrawCellInstances(InstanceContext** list, ChunkView* view)
{
    InstanceContext* instance = *list;
    u32 count = 0;
    if (instance == nullptr)
    {
        return 0;
    }

    CallVirtual<void>(view, view->vtable, ChunkView::SlotLoadModel, instance->place);
    InstanceContext* next = instance->next;
    while (true)
    {
        view->DrawnModelResult();
        s32 distance = view->distance;
        if (next != nullptr)
        {
            CallVirtual<void>(view, view->vtable, ChunkView::SlotLoadModel, next->place);
        }

        if (IsSolid(instance))
        {
            ReferencedObjectFlags flags = instance->flags;
            flags.inDrawnCell = 1;
            instance->flags = flags;
            ClearSeen(instance);
            if (flags.visible && !flags.asleep && Draws(instance))
            {
                count++;
                QueueClipped(instance);
            }
        }
        else
        {
            ReferencedObjectFlags flags = instance->flags;
            flags.inDrawnCell = 1;
            SetSeen(instance, static_cast<u32>(distance));
            instance->flags = flags;
            if (flags.visible && !flags.asleep && Draws(instance))
            {
                count++;
                QueueInView(instance);
            }
        }

        instance = next;
        if (instance == nullptr)
        {
            return count;
        }

        next = instance->next;
    }
}

// The same culled a box at a time: an instance out of view isn't drawn and leaves its drawn cell, and every one in view counts
// whether it draws or not
u32 DrawCellInstancesCulled(InstanceContext** list, ChunkView* view)
{
    InstanceContext* instance = *list;
    u32 count = 0;
    if (instance == nullptr)
    {
        return 0;
    }

    view->TestBoxOnVu0(&instance->collision.ownBox, reinterpret_cast<const Matrix4x4*>(instance->place));
    InstanceContext* next = instance->next;
    while (true)
    {
        u32 result = view->InstanceResult();
        s32 distance = view->distance;
        bool inView = result != ChunkView::OutOfView;
        if (next != nullptr)
        {
            view->TestBoxOnVu0(&next->collision.ownBox, reinterpret_cast<const Matrix4x4*>(next->place));
        }

        if (IsSolid(instance))
        {
            ReferencedObjectFlags flags = instance->flags;
            flags.inDrawnCell = 1;
            instance->flags = flags;
            ClearSeen(instance);
            if (flags.visible && !flags.asleep && Draws(instance))
            {
                count++;
                QueueClipped(instance);
            }
        }
        else
        {
            instance->flags.inDrawnCell = inView;
            SetSeen(instance, static_cast<u32>(distance));
            ReferencedObjectFlags flags = instance->flags;
            if (flags.visible && !flags.asleep && inView)
            {
                auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
                if (model != nullptr && model->drawnOgi != nullptr)
                {
                    if (result == ChunkView::InView)
                    {
                        QueueInView(instance);
                    }
                    else
                    {
                        QueueClipped(instance);
                    }
                }

                count++;
            }
        }

        instance = next;
        if (instance == nullptr)
        {
            return count;
        }

        next = instance->next;
    }
}

// The instances of a cell's two lists with a node of the kinds, in the collector's first pool
void CollectCellInstancesAll(SceneryCell* cell, InstanceCollector* collector, u32 kinds)
{
    for (InstanceContext* instance = cell->instances; instance != nullptr; instance = instance->next)
    {
        if ((instance->nodes.mask & kinds) != 0)
        {
            AddInstanceToPool(&collector->instances, InstanceCollector::WhollyInsidePool, instance);
        }
    }

    for (InstanceContext* instance = cell->dynamicInstances; instance != nullptr; instance = instance->next)
    {
        if ((instance->nodes.mask & kinds) != 0)
        {
            AddInstanceToPool(&collector->instances, InstanceCollector::WhollyInsidePool, instance);
        }
    }
}

}

namespace
{
// The same tested against the view first: wholly in view in the first pool, partly in the second
void CollectInView(InstanceContext* first, InstanceCollector* collector, u32 kinds, ChunkView* view)
{
    for (InstanceContext* instance = first; instance != nullptr; instance = instance->next)
    {
        if ((instance->nodes.mask & kinds) == 0)
        {
            continue;
        }

        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        CallVirtual<void>(view, view->vtable, ChunkView::SlotTestBoxAt, &instance->collision.ownBox, place);
        if (view->visibility == ChunkView::OutOfView)
        {
            continue;
        }

        u32 pool = view->visibility == ChunkView::PartlyInView ? InstanceCollector::PartlyInsidePool
                                                                 : InstanceCollector::WhollyInsidePool;
        AddInstanceToPool(&collector->instances, pool, instance);
    }
}

}

extern "C"
{
void CollectCellInstancesInView(SceneryCell* cell, InstanceCollector* collector, u32 kinds, ChunkView* view)
{
    CollectInView(cell->instances, collector, kinds, view);
    CollectInView(cell->dynamicInstances, collector, kinds, view);
}
}

namespace
{
constexpr u32 CollectorPools = InstanceCollector::PoolCount;

template <typename T>
ItemPool<T>* NewPools()
{
    ItemPool<T>* pools = NewArray<ItemPool<T>>(CollectorPools);
    for (u32 index = 0; index < CollectorPools; index++)
    {
        ItemPool<T>::Construct(&pools[index]);
    }

    return pools;
}

template <typename T>
void DeletePools(ItemPool<T>* pools)
{
    if (pools == nullptr)
    {
        return;
    }

    for (ItemPool<T>* pool = pools + ArrayCount(pools); pool != pools;)
    {
        pool--;
        pool->VirtualDestroy(DestroyElement);
    }

    DeleteArray(pools);
}
}

extern "C"
{
InstanceCollector* InstanceCollectorConstruct(InstanceCollector* collector, u32 wantedFlags, u32 unwantedFlags, u32 unused)
{
    collector->instances.count = CollectorPools;
    collector->instances.pools = NewPools<InstanceContext*>();
    collector->cells.count = CollectorPools;
    collector->cells.pools = NewPools<SceneryCell*>();
    collector->wantedFlags = wantedFlags;
    collector->unwantedFlags = unwantedFlags;
    collector->unused18 = unused;
    InstanceCollectorClear(collector);
    return collector;
}

void InstanceCollectorDestroy(InstanceCollector* collector, u32 flags)
{
    DeletePools(collector->cells.pools);
    DeletePools(collector->instances.pools);
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(collector);
    }
}

void InstanceCollectorClear(InstanceCollector* collector)
{
    ClearCellPools(&collector->cells);
    ClearInstancePools(&collector->instances);
}

u32 InstanceCollectorFound(const InstanceCollector* collector)
{
    for (s32 index = 0; index < static_cast<s32>(CollectorPools); index++)
    {
        if (!InstancePoolEmpty(&collector->instances, index))
        {
            return 1;
        }
    }

    return 0;
}

u32 CollectCellInstances(InstanceCollector* collector, u32 kinds)
{
    PoolsWalk<SceneryCell*> walk;
    ConstructPoolsWalk(&walk, g_CellPoolsWalkVTable, g_CellWalkVTable, &collector->cells, InstanceCollector::BothPools);
    for (walk.First(); !walk.AtEnd(); walk.Next())
    {
        for (InstanceContext* instance = (*walk.Item())->instances; instance != nullptr; instance = instance->next)
        {
            u32 flags = instance->flags.value;
            if ((flags & collector->wantedFlags) == collector->wantedFlags && (flags & collector->unwantedFlags) == 0 &&
                (instance->nodes.mask & kinds) != 0)
            {
                AddInstanceToPool(&collector->instances, InstanceCollector::WhollyInsidePool, instance);
            }
        }
    }

    u32 found = InstanceCollectorFound(collector);
    walk.Destroy(0);
    return found;
}
}

namespace
{
// The instances matching a filter (InstanceFilterWord) told a slot
void TellInstances(SceneryCell* cell, const u32* filter, u32 slot)
{
    InstanceContext* instance = cell->instances;
    while (instance != nullptr)
    {
        InstanceContext* next = instance->next;
        bool matches = false;
        u32 flags = instance->flags.value;
        if ((flags & filter[FilterWantedFlags]) == filter[FilterWantedFlags] && (flags & filter[FilterUnwantedFlags]) == 0)
        {
            matches = (instance->nodes.mask & filter[FilterKinds]) != 0;
        }

        if (matches)
        {
            CallVirtual<void>(instance, instance->vtable, slot);
        }

        instance = next;
    }
}

u32 DrawCellContents(SceneryCell* cell, ChunkView* view, bool culled)
{
    u32 count = cell->instances != nullptr;
    if (cell->instances != nullptr)
    {
        if (culled)
        {
            DrawCellInstancesCulled(&cell->instances, view);
        }
        else
        {
            DrawCellInstances(&cell->instances, view);
        }
    }

    if (cell->meshes != nullptr)
    {
        count += culled ? cell->meshes->DrawCulled(view) : cell->meshes->Draw(view);
    }

    for (InstanceContext* instance = cell->dynamicInstances; instance != nullptr; instance = instance->next)
    {
        auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, NodeDynamicScenery));
        auto* dynamic = static_cast<DynamicSceneryNode*>(node);
        count += culled ? dynamic->DrawCulled(view) : dynamic->Draw(view);
    }

    return count;
}

void DestroyCellBase(SceneryCell* cell, u32 flags)
{
    cell->vtable = g_SceneryCellVTable;
    if (cell->meshes != nullptr)
    {
        cell->meshes->Destroy(DestroyAndFree);
    }

    CellListDestroy(&cell->dynamicInstances, DestroyOnly);
    CellListDestroy(&cell->instances, DestroyOnly);
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(cell);
    }
}

void ReleaseCellMeshes(SceneryCell* cell, u32 keep)
{
    if (keep != 0)
    {
        if (cell->meshes != nullptr)
        {
            cell->meshes->Release();
        }

        return;
    }

    if (cell->meshes != nullptr)
    {
        cell->meshes->Destroy(DestroyAndFree);
    }

    cell->meshes = nullptr;
}

void SetCellItem(SceneryCell* cell, RigidModel* mesh, Lod* lod, s32 index)
{
    if (mesh != nullptr)
    {
        cell->meshes->SetMesh(mesh, index);
    }
    else if (lod != nullptr)
    {
        cell->meshes->SetLod(lod, index);
    }
}

s32 ChildCountOf(SceneryCell* cell)
{
    return CallVirtual<s32>(cell, cell->vtable, SceneryCell::SlotChildCount);
}

SceneryCell** ChildrenOf(SceneryCell* cell, u32 slot)
{
    return CallVirtual<SceneryCell**>(cell, cell->vtable, slot);
}
}

extern "C"
{
InstanceContext** CellListConstruct(InstanceContext** list)
{
    *list = nullptr;
    return list;
}

void CellListDestroy(InstanceContext** list, u32 flags)
{
    CellListRelease(list);
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(list);
    }
}

void CellListAdd(InstanceContext** list, InstanceContext* instance)
{
    InstanceListPushFront(instance, reinterpret_cast<void**>(list), ListPrevious, ListNext);
}

void CellListRemove(InstanceContext** list, InstanceContext* instance)
{
    ListRemove(instance, reinterpret_cast<void**>(list), ListPrevious, ListNext);
}

// Every instance put to sleep, which takes it off the list
void CellListRelease(InstanceContext** list)
{
    if (*list == nullptr)
    {
        return;
    }

    InstanceContext* instance = *list;
    while (true)
    {
        CallVirtual<void>(instance, instance->vtable, InstanceContext::ReleaseSlot);
        if (*list == nullptr)
        {
            return;
        }

        instance = *list;
    }
}
}

void SceneryCell::ConstructBase()
{
    vtable = g_SceneryCellVTable;
    CellListConstruct(&instances);
    CellListConstruct(&dynamicInstances);
}

void SceneryCell::Destroy(u32 flags)
{
    DestroyCellBase(this, flags);
}

void SceneryCell::Release(u32 releaseInstances, u32 queue)
{
    GameReadersStorage* storage = g_ReadersStorages[MainReaders];
    if (queue != 0)
    {
        auto* reader = static_cast<SceneryDestroyReader*>(MemoryAllocate(sizeof(SceneryDestroyReader)));
        reader->vtable = g_SceneryDestroyReaderVTable;
        reader->cell = this;
        MemoryReader* item =
            MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), reader, nullptr, 0);
        AddItemReaderToReaderStorage(storage, item, QueueFront);
        CellListRelease(&instances);
        parent = nullptr;
        return;
    }

    if (releaseInstances != 0)
    {
        CellListRelease(&instances);
    }
}

u32 SceneryCell::Render(s32 parentVisibility, ChunkView* view, ChunkData* drawn)
{
    u32 slot = SlotDrawContents;
    if (parentVisibility != ChunkView::InView)
    {
        CallVirtual<void>(view, view->vtable, ChunkView::SlotTestCell, this);
        u32 visibility = view->CellResult();
        if (visibility == ChunkView::OutOfView)
        {
            return 0;
        }

        if (visibility == ChunkView::PartlyInView)
        {
            slot = SlotDrawContentsCulled;
        }
    }

    return CallVirtual<u32>(this, vtable, slot, view, drawn);
}

void SceneryCell::CollectInstances(s32 parentVisibility, InstanceCollector* collector, u32 kinds, ChunkView* view)
{
    if (parentVisibility == ChunkView::InView)
    {
        CollectCellInstancesAll(this, collector, kinds);
        return;
    }

    CallVirtual<void>(view, view->vtable, ChunkView::SlotTestBox, this);
    u32 visibility = view->lastVisibility;
    if (visibility == ChunkView::OutOfView)
    {
        return;
    }

    if (visibility == ChunkView::PartlyInView)
    {
        CollectCellInstancesInView(this, collector, kinds, view);
    }
    else
    {
        CollectCellInstancesAll(this, collector, kinds);
    }
}

void SceneryCell::CollectCells(InstanceCollector* collector)
{
    if (CallVirtual<u32>(this, vtable, SlotHasInstances) != 0)
    {
        AddCellToPool(&collector->cells, InstanceCollector::WhollyInsidePool, this);
    }
}

SceneryCell* SceneryCell::FindCell()
{
    return this;
}

void SceneryCell::SetChunkField(ChunkData* owner)
{
    chunk = owner;
}

void SceneryCell::SetChunk(ChunkData* owner)
{
    SetChunkField(owner);
}

u32 SceneryCell::CollectVisibleCells(BoundingVolume* volume, InstanceCollector* collector)
{
    u32 inside = VolumeHoldsCell(this, volume);
    if (inside != Volume::Apart && instances != nullptr)
    {
        u32 pool = inside == Volume::Inside ? InstanceCollector::WhollyInsidePool : InstanceCollector::PartlyInsidePool;
        AddCellToPool(&collector->cells, pool, this);
    }

    return inside;
}

u32 SceneryCell::CollectVisible(BoundingVolume* volume, InstanceCollector* collector)
{
    return CollectVisibleCells(volume, collector);
}

u32 SceneryCell::DrawContents(ChunkView* view)
{
    return DrawCellContents(this, view, false);
}

u32 SceneryCell::DrawContentsCulled(ChunkView* view)
{
    return DrawCellContents(this, view, true);
}

void SceneryCell::ReleaseMeshes(u32 keep)
{
    ReleaseCellMeshes(this, keep);
}

u32 SceneryCell::Collect()
{
    return 0;
}

u32 SceneryCell::CellOf()
{
    return 0;
}

u32 SceneryCell::CellOfBox()
{
    return 0;
}

f32 SceneryCell::Unused17()
{
    return 0.0f;
}

s32 SceneryCell::Depth()
{
    return -1;
}

u16 SceneryCell::QueryInstances(InstanceQuery* query)
{
    u16 before = query->count;
    for (InstanceContext* instance = instances; instance != nullptr; instance = instance->next)
    {
        QueryAdd(query, instance);
    }

    return static_cast<u16>(query->count - before);
}

u32 SceneryCell::HasInstances()
{
    return instances != nullptr;
}

void SceneryCell::SleepInstances(const u32* filter)
{
    TellInstances(this, filter, InstanceContext::SleepSlot);
}

void SceneryCell::ReleaseInstances(const u32* filter)
{
    TellInstances(this, filter, InstanceContext::ReleaseSlot);
}

u32 SceneryCell::Type()
{
    return TypeId;
}

// Its meshes (read when the type says so), its volume (only the box is kept) and its lights' bits
void SceneryCell::Read(Stream* stream)
{
    s32 kind;
    stream->ReadS32(&kind);
    if (kind == SceneryMeshes::NoTypeId)
    {
        meshes = nullptr;
    }
    else if (kind == SceneryMeshes::TypeId)
    {
        meshes = SceneryMeshes::ConstructRead(static_cast<SceneryMeshes*>(MemoryAllocate(sizeof(SceneryMeshes))), stream);
    }

    BoundingVolume volume;
    volume.vtable = g_BoxVolumeVTable;
    ReadBoxVolume(&volume, stream);
    min = volume.min;
    max = volume.max;
    stream->Read(lights, sizeof(lights), 1);
    volume.vtable = g_VolumeVTable;
}

SceneryCell* SceneryCell::AddInstanceAt(const s16*, InstanceContext* instance)
{
    AddInstance(instance);
    return this;
}

void SceneryCell::AddInstance(InstanceContext* instance)
{
    if (instance->flags.dynamicScenery)
    {
        CellListAdd(&dynamicInstances, instance);
        return;
    }

    CellListAdd(&instances, instance);
}

SceneryCell* SceneryCell::SetItemAt(const s16*, RigidModel* mesh, Lod* lod, u32, s32 index)
{
    SetCellItem(this, mesh, lod, index);
    return this;
}

SceneryCell* SceneryCell::CellAt()
{
    return this;
}

SceneryCell* SceneryCell::Parent()
{
    return parent;
}

void SceneryCell::SetLight()
{
}

SceneryLeaf* SceneryLeaf::Construct(SceneryLeaf* leaf)
{
    leaf->ConstructBase();
    leaf->parent = nullptr;
    leaf->meshes = nullptr;
    leaf->chunk = nullptr;
    for (u32 index = 0; index < sizeof(leaf->lights); index++)
    {
        leaf->lights[index] = 0;
    }

    leaf->vtable = g_SceneryLeafVTable;
    return leaf;
}

void SceneryLeaf::Destroy(u32 flags)
{
    DestroyCellBase(this, flags);
}

u32 SceneryLeaf::IsLeaf()
{
    return 1;
}

void SceneryLeaf::RemoveChild()
{
}

u32 SceneryLeaf::DrawContents(ChunkView* view)
{
    return DrawCellContents(this, view, false);
}

u32 SceneryLeaf::DrawContentsCulled(ChunkView* view)
{
    return DrawCellContents(this, view, true);
}

u32 SceneryLeaf::IsEmpty()
{
    return meshes == nullptr && instances == nullptr;
}

u32 SceneryLeaf::Type()
{
    return TypeId;
}

SceneryCell* SceneryLeaf::SetItemAt(const s16*, RigidModel* mesh, Lod* lod, u32, s32 index, u32, u32 set)
{
    if (set != 0)
    {
        SetCellItem(this, mesh, lod, index);
    }

    return this;
}

SceneryCell* SceneryLeaf::MakeChild()
{
    return nullptr;
}

SceneryTree* SceneryTree::Construct(SceneryTree* tree)
{
    tree->ConstructBase();
    tree->parent = nullptr;
    tree->meshes = nullptr;
    tree->chunk = nullptr;
    for (u32 index = 0; index < sizeof(tree->lights); index++)
    {
        tree->lights[index] = 0;
    }

    tree->vtable = g_SceneryTreeVTable;
    return tree;
}

SceneryTree* SceneryTree::ConstructBox(SceneryTree* tree, const Vector4* middle, const Vector4* halfSize, SceneryCell* parent,
                                       s32 depth)
{
    tree->ConstructBase();
    tree->parent = parent;
    tree->meshes = nullptr;
    tree->chunk = nullptr;
    Vector4 low;
    Vector4 high;
    high.z = middle->z + halfSize->z;
    low.z = middle->z - halfSize->z;
    high.x = middle->x + halfSize->x;
    low.x = middle->x - halfSize->x;
    high.y = middle->y + halfSize->y;
    low.y = middle->y - halfSize->y;
    high.w = 1.0f;
    low.w = 1.0f;
    tree->max = high;
    tree->min = low;
    for (u32 index = 0; index < sizeof(tree->lights); index++)
    {
        tree->lights[index] = 0;
    }

    tree->depth = depth;
    tree->vtable = g_SceneryTreeVTable;
    return tree;
}

void SceneryTree::Destroy(u32 flags)
{
    DestroyCellBase(this, flags);
}

u32 SceneryTree::Render(s32 parentVisibility, ChunkView* view, ChunkData* drawn)
{
    u32 count = 0;
    s32 visibility = ChunkView::InView;
    if (parentVisibility == ChunkView::InView)
    {
        count = DrawCellContents(this, view, false);
    }
    else
    {
        CallVirtual<void>(view, view->vtable, ChunkView::SlotTestCell, this);
        visibility = static_cast<s32>(view->CellResult());
        if (visibility == ChunkView::OutOfView)
        {
            return 0;
        }

        count = DrawCellContents(this, view, true);
    }

    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            count += (*child)->VirtualRender(visibility, view, drawn);
        }
    }

    return count;
}

void SceneryTree::CollectInstances(s32 parentVisibility, InstanceCollector* collector, u32 kinds, ChunkView* view)
{
    s32 visibility = ChunkView::InView;
    if (parentVisibility == ChunkView::InView)
    {
        CollectCellInstancesAll(this, collector, kinds);
    }
    else
    {
        CollectCellInstancesInView(this, collector, kinds, view);
        CallVirtual<void>(view, view->vtable, ChunkView::SlotTestBox, this);
        visibility = static_cast<s32>(view->lastVisibility);
        if (visibility == ChunkView::OutOfView)
        {
            return;
        }
    }

    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            CallVirtual<void>(*child, (*child)->vtable, SlotCollectInstances, visibility, collector, kinds, view);
        }
    }
}

u32 SceneryTree::IsLeaf()
{
    return 0;
}

void SceneryTree::CollectCells(InstanceCollector* collector)
{
    if (CallVirtual<u32>(this, vtable, SlotHasInstances) != 0)
    {
        AddCellToPool(&collector->cells, InstanceCollector::WhollyInsidePool, this);
    }

    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotOtherChildren);
    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            CallVirtual<void>(*child, (*child)->vtable, SlotCollectCells, collector);
        }
    }
}

void SceneryTree::SetChunk(ChunkData* owner)
{
    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    chunk = owner;
    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            CallVirtual<void>(*child, (*child)->vtable, SlotSetChunk, owner);
        }
    }
}

void SceneryTree::CollectVisible(BoundingVolume* volume, InstanceCollector* collector)
{
    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotOtherChildren);
    u32 inside = VolumeHoldsCell(this, volume);
    if (inside == Volume::Apart)
    {
        return;
    }

    if (instances != nullptr)
    {
        u32 pool = inside == Volume::Inside ? InstanceCollector::WhollyInsidePool : InstanceCollector::PartlyInsidePool;
        AddCellToPool(&collector->cells, pool, this);
    }

    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            CallVirtual<void>(*child, (*child)->vtable, SlotCollectVisible, volume, collector);
        }
    }
}

void SceneryTree::ReleaseMeshes(u32 keep)
{
    ReleaseCellMeshes(this, keep);
    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    for (; children > 0; children--, child++)
    {
        if (*child != nullptr)
        {
            CallVirtual<void>(*child, (*child)->vtable, SlotReleaseMeshes, keep);
        }
    }
}

s32 SceneryTree::Depth()
{
    return depth;
}

u32 SceneryTree::HasInstances()
{
    u32 has = instances != nullptr;
    if (has != 0)
    {
        return has;
    }

    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotOtherChildren);
    for (s32 index = 0; index < children; index++, child++)
    {
        if (*child == nullptr)
        {
            continue;
        }

        has = CallVirtual<u32>(*child, (*child)->vtable, SlotHasInstances);
        if (has != 0)
        {
            return has;
        }
    }

    return has;
}

// A path is a child's index per level, -1 ending it: the cell at its end gets the instance (the children on the way made)
SceneryCell* SceneryTree::AddInstanceAt(const s16* path, InstanceContext* instance, u32 unused)
{
    if (*path == PathEnd)
    {
        AddInstance(instance);
        return this;
    }

    SceneryCell** children = ChildrenOf(this, SlotChildren);
    if (children[*path] == nullptr)
    {
        CallVirtual<SceneryCell*>(this, vtable, SlotMakeChild, static_cast<s32>(*path), depth);
    }

    SceneryCell* child = children[*path];
    return CallVirtual<SceneryCell*>(child, child->vtable, SlotAddInstanceAt, path + 1, instance, unused);
}

SceneryCell* SceneryTree::SetItemAt(const s16* path, RigidModel* mesh, Lod* lod, u32 unused4, s32 index, u32 unused6,
                                    u32 set)
{
    if (*path == PathEnd)
    {
        SetCellItem(this, mesh, lod, index);
        return this;
    }

    SceneryCell* child = ChildrenOf(this, SlotChildren)[*path];
    return CallVirtual<SceneryCell*>(child, child->vtable, SlotSetItemAt, path + 1, mesh, lod, unused4, index, unused6, set);
}

// Without making the children on the way, a missing one's cell is asked for at address 0 (retail)
SceneryCell* SceneryTree::CellAt(const s16* path, u32 make)
{
    if (*path == PathEnd)
    {
        return this;
    }

    SceneryCell** children = ChildrenOf(this, SlotChildren);
    if (children[*path] == nullptr && make != 0)
    {
        CallVirtual<SceneryCell*>(this, vtable, SlotMakeChild, static_cast<s32>(*path), depth);
    }

    SceneryCell* child = children[*path];
    return CallVirtual<SceneryCell*>(child, child->vtable, SlotCellAt, path + 1, make);
}

void SceneryTree::SetLight(u32 bit)
{
    lights[bit >> 3] |= static_cast<u8>(1 << (bit & 7));
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    for (s32 index = 0; index < ChildCountOf(this); index++)
    {
        SceneryCell* cell = *child++;
        if (cell != nullptr)
        {
            CallVirtual<void>(cell, cell->vtable, SlotSetLight, bit);
        }
    }
}

s32 SceneryTree::ChildCount()
{
    return 0;
}

SceneryCell** SceneryTree::Children()
{
    return nullptr;
}

SceneryCell** SceneryTree::OtherChildren()
{
    return nullptr;
}

void SceneryTree::ClearChildren()
{
    s32 children = ChildCountOf(this);
    SceneryCell** child = ChildrenOf(this, SlotChildren);
    for (; children > 0; children--)
    {
        *child++ = nullptr;
    }
}

SceneryNode* SceneryNode::Construct(SceneryNode* node)
{
    SceneryTree::Construct(node);
    node->vtable = g_SceneryNodeVTable;
    node->ClearChildren();
    return node;
}

void SceneryNode::Destroy(u32 flags)
{
    vtable = g_SceneryNodeVTable;
    for (u32 index = 0; index < Octants; index++)
    {
        if (children[index] != nullptr)
        {
            children[index]->VirtualDestroy(DestroyAndFree);
        }
    }

    SceneryTree::Destroy(flags);
}

// Queued, each child's release is queued too and forgotten
void SceneryNode::Release(u32 releaseInstances, u32 queue)
{
    GameReadersStorage* storage = g_ReadersStorages[MainReaders];
    if (queue != 0)
    {
        auto* reader = static_cast<SceneryDestroyReader*>(MemoryAllocate(sizeof(SceneryDestroyReader)));
        reader->vtable = g_SceneryDestroyReaderVTable;
        reader->cell = this;
        MemoryReader* item =
            MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), reader, nullptr, 0);
        AddItemReaderToReaderStorage(storage, item, QueueFront);
        CellListRelease(&instances);
        parent = nullptr;
    }
    else if (releaseInstances != 0)
    {
        CellListRelease(&instances);
    }

    for (u32 index = 0; index < Octants; index++)
    {
        SceneryCell* child = children[index];
        if (child == nullptr)
        {
            continue;
        }

        auto* reader = static_cast<SceneryReleaseReader*>(MemoryAllocate(sizeof(SceneryReleaseReader)));
        reader->cell = child;
        reader->vtable = g_SceneryReleaseReaderVTable;
        reader->instances = static_cast<u8>(releaseInstances);
        reader->queue = static_cast<u8>(queue);
        MemoryReader* item =
            MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), reader, nullptr, 0);
        AddItemReaderToReaderStorage(storage, item, QueueFront);
        if (queue != 0)
        {
            children[index] = nullptr;
        }
    }
}

SceneryCell* SceneryNode::FindCell(const Vector4* low, const Vector4* high, s32 depthLeft)
{
    // Farther than any child's middle (1e11)
    constexpr f32 Far = 0x1.74876Ep+36f;
    s32 nearest = -1;
    f32 best = Far;
    Vector4 middle = *high;
    middle.x = (middle.x - low->x) * 0.5f + low->x;
    middle.y = (middle.y - low->y) * 0.5f + low->y;
    middle.z = (middle.z - low->z) * 0.5f + low->z;
    for (s32 index = 0; index < static_cast<s32>(Octants); index++)
    {
        SceneryCell* child = children[index];
        if (child == nullptr || BoxContainsRegion(reinterpret_cast<const Box*>(child), low, high) != 1)
        {
            continue;
        }

        Vector4 centre = child->min;
        centre.x = (centre.x + child->max.x) * 0.5f;
        centre.y = (centre.y + child->max.y) * 0.5f;
        centre.z = (centre.z + child->max.z) * 0.5f;
        f32 dx = centre.x - middle.x;
        f32 dy = centre.y - middle.y;
        f32 dz = centre.z - middle.z;
        f32 distance = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
        if (distance < best)
        {
            best = distance;
            nearest = index;
        }
    }

    if (nearest == -1)
    {
        return this;
    }

    SceneryCell* child = children[nearest];
    return CallVirtual<SceneryCell*>(child, child->vtable, SlotFindCell, low, high, depthLeft - 1);
}

void SceneryNode::RemoveChild(SceneryCell* child)
{
    for (s32 index = 0; index < static_cast<s32>(Octants); index++)
    {
        if (children[index] != child)
        {
            continue;
        }

        if (child != nullptr)
        {
            child->VirtualDestroy(DestroyAndFree);
        }

        children[index] = nullptr;
    }
}

u32 SceneryNode::IsEmpty()
{
    if (meshes != nullptr || instances != nullptr)
    {
        return 0;
    }

    for (s32 index = 0; index < static_cast<s32>(Octants); index++)
    {
        if (children[index] != nullptr)
        {
            return 0;
        }
    }

    return 1;
}

void SceneryNode::SleepInstances(const u32* filter)
{
    for (u32 index = 0; index < Octants; index++)
    {
        SceneryCell* child = children[index];
        if (child != nullptr)
        {
            CallVirtual<void>(child, child->vtable, SlotSleepInstances, filter);
        }
    }

    TellInstances(this, filter, InstanceContext::SleepSlot);
}

void SceneryNode::ReleaseInstances(const u32* filter)
{
    for (u32 index = 0; index < Octants; index++)
    {
        SceneryCell* child = children[index];
        if (child != nullptr)
        {
            CallVirtual<void>(child, child->vtable, SlotReleaseInstances, filter);
        }
    }

    TellInstances(this, filter, InstanceContext::ReleaseSlot);
}

u32 SceneryNode::Type()
{
    return TypeId;
}

// The cell's base, then the types of the eight children (a node's, a leaf's, anything else none), each child read later by the
// readers
void SceneryNode::Read(Stream* stream)
{
    SceneryCell::Read(stream);
    u32 types[Octants];
    stream->Read(types, sizeof(types), 1);
    for (u32 index = 0; index < Octants; index++)
    {
        SceneryCell* child;
        if (types[index] == SceneryNode::TypeId)
        {
            child = MakeNode(sizeof(SceneryNode));
        }
        else if (types[index] == SceneryLeaf::TypeId)
        {
            child = MakeLeaf();
        }
        else
        {
            child = nullptr;
        }

        GameReadersStorage* storage = g_ReadersStorages[MainReaders];
        if (child != nullptr)
        {
            auto* reader = static_cast<SceneryCellReader*>(MemoryAllocate(sizeof(SceneryCellReader)));
            reader->vtable = g_SceneryCellReaderVTable;
            reader->stream = stream;
            reader->cell = child;
            MemoryReader* item =
                MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), reader, nullptr, 0);
            child->parent = this;
            AddItemReaderToReaderStorage(storage, item, QueueBack);
        }

        children[index] = child;
    }
}

// The octant's box is half the node's around the octant's middle; below a depth of LeafDepth the child is a leaf. Its lights'
// bits and chunk are the node's
SceneryCell* SceneryNode::MakeChild(u32 octant, s32 depthLeft)
{
    // The node's size first, then (OctantOffset writes over it) the octant's middle
    Vector4 offset;
    Vector4 size;
    size.y = max.y - min.y;
    size.x = max.x - min.x;
    size.z = max.z - min.z;
    size.w = 1.0f;
    offset.w = 1.0f;
    offset.x = size.x * 0.5f;
    offset.y = size.y * 0.5f;
    offset.z = size.z * 0.5f;
    OctantOffset(&size.x, octant);
    offset.x = offset.x * 0.5f;
    offset.y = offset.y * 0.5f;
    offset.z = offset.z * 0.5f;
    SceneryCell* child;
    if (depthLeft < LeafDepth)
    {
        auto* leaf = static_cast<SceneryLeaf*>(MemoryAllocate(sizeof(SceneryLeaf)));
        leaf->ConstructBase();
        Vector4 low;
        Vector4 high;
        low.x = size.x - offset.x;
        leaf->parent = this;
        low.y = size.y - offset.y;
        leaf->meshes = nullptr;
        low.z = size.z - offset.z;
        low.w = 1.0f;
        high.w = 1.0f;
        high.x = size.x + offset.x;
        high.y = size.y + offset.y;
        high.z = size.z + offset.z;
        leaf->min = low;
        leaf->chunk = nullptr;
        leaf->max = high;
        for (u32 index = 0; index < sizeof(leaf->lights); index++)
        {
            leaf->lights[index] = 0;
        }

        leaf->vtable = g_SceneryLeafVTable;
        child = leaf;
    }
    else
    {
        auto* node = static_cast<SceneryNode*>(MemoryAllocate(sizeof(SceneryNode)));
        SceneryTree::ConstructBox(node, &size, &offset, this, depthLeft - 1);
        node->vtable = g_SceneryNodeVTable;
        node->ClearChildren();
        child = node;
    }

    for (u32 index = 0; index < sizeof(child->lights); index++)
    {
        child->lights[index] = child->parent->lights[index];
    }

    child->chunk = chunk;
    children[octant] = child;
    return child;
}

// A quarter of the size, each axis's sign from the octant's bits (bit 0 x, 1 y, 2 z, a set bit the lower half); a node of other
// than 8 children stays level; then the node's middle added
void SceneryNode::OctantOffset(f32* offset, u32 octant)
{
    f32 quarter[4];
    Vector4 size;
    size.x = max.x - min.x;
    size.y = max.y - min.y;
    size.z = max.z - min.z;
    offset[3] = 1.0f;
    size.w = 1.0f;
    quarter[3] = 1.0f;
    quarter[0] = size.x * 0.5f * 0.5f;
    quarter[1] = size.y * 0.5f * 0.5f;
    quarter[2] = size.z * 0.5f * 0.5f;
    s32 bits = static_cast<s32>(octant);
    const f32* from = quarter;
    for (f32* to = offset; to < offset + 3; to++, from++)
    {
        *to = (bits & 1) != 0 ? -*from : *from;
        bits >>= 1;
    }

    if (ChildCountOf(this) != static_cast<s32>(Octants))
    {
        offset[1] = 0.0f;
    }

    Vector4 middle = min;
    middle.x = (middle.x + max.x) * 0.5f;
    offset[0] = offset[0] + middle.x;
    middle.y = (middle.y + max.y) * 0.5f;
    offset[1] = offset[1] + middle.y;
    middle.z = (middle.z + max.z) * 0.5f;
    offset[2] = offset[2] + middle.z;
}

s32 SceneryNode::ChildCount()
{
    return Octants;
}

SceneryCell** SceneryNode::Children()
{
    return children;
}

SceneryCell** SceneryNode::OtherChildren()
{
    return children;
}

void SceneryRoot::Destroy(u32 flags)
{
    vtable = g_SceneryNodeVTable;
    for (u32 index = 0; index < Octants; index++)
    {
        SceneryCell* child = children[index];
        if (child != nullptr)
        {
            child->VirtualDestroy(DestroyAndFree);
        }
    }

    SceneryTree::Destroy(flags);
}

void SceneryRoot::Collect(u32 kinds, InstanceCollector* collector)
{
    CallVirtual<void>(this, vtable, SlotCollectCells, collector);
    CollectCellInstances(collector, kinds);
}

SceneryCell* SceneryRoot::CellOf(InstanceContext* instance, const Matrix4x4* matrix)
{
    constexpr f32 Margin = Epsilon;
    const Vector4* low = &instance->collision.box.min;
    const Vector4* high = &instance->collision.box.max;
    Box box;
    if (matrix != nullptr)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        Matrix4x4 placed;
        VuMultiplyMatrices(reinterpret_cast<const Matrix4x4*>(place), matrix, &placed);
        GetInstanceHullBounds(&instance->collision, &placed, &box);
        low = &box.min;
        high = &box.max;
    }

    if (CellHoldsBox(Margin, this, low, high) == Volume::Apart)
    {
        return nullptr;
    }

    return FindCell(low, high, static_cast<s32>(treeDepth));
}

SceneryCell* SceneryRoot::CellOfBox(const Vector4* low, const Vector4* high)
{
    if (BoxContainsRegion(reinterpret_cast<const Box*>(this), low, high) == 0)
    {
        return nullptr;
    }

    return FindCell(low, high, static_cast<s32>(treeDepth));
}

u32 SceneryRoot::Type()
{
    return TypeId;
}

void SceneryRoot::Read(Stream* stream)
{
    stream->ReadU32(&treeDepth);
    chunk = nullptr;
    SceneryNode::Read(stream);
}

void SceneryCellReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SceneryCellReader::Read(u8*, u32, ReaderStack*)
{
    CallVirtual<void>(cell, cell->vtable, SceneryCell::SlotRead, stream);
}

void SceneryReleaseReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SceneryReleaseReader::Read(u8*, u32, ReaderStack*)
{
    cell->VirtualRelease(instances, queue);
}

void SceneryDestroyReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SceneryDestroyReader::Read(u8*, u32, ReaderStack*)
{
    if (cell != nullptr)
    {
        cell->VirtualDestroy(DestroyAndFree);
    }
}

SceneryMeshes* SceneryMeshes::Construct(SceneryMeshes* meshes, s16 meshCount, s16 lodCount)
{
    meshes->meshCount = static_cast<u16>(meshCount);
    meshes->lodCount = static_cast<u16>(lodCount);
    u32 count = static_cast<u16>(meshCount) + static_cast<u16>(lodCount);
    u32 size = count * (sizeof(Box) + sizeof(void*));
    auto* boxes = static_cast<Box*>(MemoryAllocateAligned(GetHeapManager(), size, CacheLineSize));
    meshes->items = reinterpret_cast<void**>(boxes + count);
    meshes->boxes = boxes;
    RetailLibc::MemorySet(boxes + count, 0, count * sizeof(void*));
    meshes->matrices = nullptr;
    meshes->matrices = static_cast<Matrix4x4*>(
        MemoryAllocateAligned(GetHeapManager(), (meshCount + lodCount) * sizeof(Matrix4x4), CacheLineSize));
    return meshes;
}

SceneryMeshes* SceneryMeshes::ConstructRead(SceneryMeshes* meshes, Stream* stream)
{
    meshes->boxes = nullptr;
    meshes->items = nullptr;
    meshes->Read(stream);
    return meshes;
}

void SceneryMeshes::Destroy(u32 flags)
{
    MemoryDeallocate2_(matrices);
    Release();
    if (boxes != nullptr)
    {
        MemoryDeallocate2_(boxes);
    }

    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SceneryMeshes::Read(Stream* stream)
{
    ReadItems(stream);
    u32 count = meshCount + lodCount;
    if (count == 0)
    {
        matrices = nullptr;
        return;
    }

    u32 size = count * sizeof(Matrix4x4);
    matrices = static_cast<Matrix4x4*>(MemoryAllocateAligned(GetHeapManager(), size, CacheLineSize));
    stream->Read(matrices, size, 1);
}

void SceneryMeshes::ReadItems(Stream* stream)
{
    stream->Read(&meshCount, sizeof(meshCount), 1);
    stream->Read(&lodCount, sizeof(lodCount), 1);
    s32 count = meshCount + lodCount;
    if (count == 0)
    {
        items = nullptr;
        boxes = nullptr;
        return;
    }

    auto* ids = static_cast<u32*>(MemoryAllocate2(count * sizeof(u32)));
    u32 size = count * (sizeof(Box) + sizeof(void*));
    auto* allocated = static_cast<Box*>(MemoryAllocateAligned(GetHeapManager(), size, CacheLineSize));
    boxes = allocated;
    items = reinterpret_cast<void**>(allocated + count);
    stream->Read(allocated, count * sizeof(Box), 1);
    stream->Read(ids, count * sizeof(u32), 1);
    u32 meshes = meshCount;
    u32 total = meshes + lodCount;
    void** item = items;
    u32* id = ids;
    u32 index = 0;
    for (; index < meshes; index++)
    {
        *item++ = g_MeshTable.Acquire(id++, nullptr);
    }

    for (; index < total; index++)
    {
        *item++ = g_LodTable.Acquire(id++, nullptr);
    }

    if (ids != nullptr)
    {
        MemoryDeallocate_(ids);
    }
}

void SceneryMeshes::Release()
{
    u32 index = 0;
    for (; index < meshCount; index++)
    {
        ReleaseMesh(static_cast<RigidModel*>(items[index]));
    }

    for (u32 total = meshCount + lodCount; index < total; index++)
    {
        ReleaseLod(static_cast<Lod*>(items[index]));
    }

    RetailLibc::MemorySet(items, 0, (meshCount + lodCount) * sizeof(void*));
}

void SceneryMeshes::SetMesh(RigidModel* mesh, s32 index)
{
    items[index] = mesh;
}

void SceneryMeshes::SetLod(Lod* lod, s32 index)
{
    items[index] = lod;
}

void SceneryMeshes::SetMatrix(const ObjectPlace* place, s32 index)
{
    matrices[index] = *reinterpret_cast<const Matrix4x4*>(place);
}

// The box's minimum's w is the radius around its middle, its maximum's the extra
void SceneryMeshes::SetBox(f32 extra, const Box* box, s32 index)
{
    boxes[index] = *box;
    Box* kept = &boxes[index];
    Vector4 half = kept->min;
    half.x = (half.x - kept->max.x) * 0.5f;
    half.y = (half.y - kept->max.y) * 0.5f;
    half.z = (half.z - kept->max.z) * 0.5f;
    kept->min.w = __builtin_sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
    boxes[index].max.w = extra;
}

// Every item in view (the view's outcome set so), the next one's matrix loaded while one is drawn: the LODs' meshes for the
// culling's distance
u32 SceneryMeshes::Draw(ChunkView* view)
{
    u32 total = meshCount + lodCount;
    view->lastVisibility = ChunkView::InView;
    view->visibility = ChunkView::InView;
    if (items == nullptr || items[0] == nullptr)
    {
        return total;
    }

    CallVirtual<void>(view, view->vtable, ChunkView::SlotLoadModel, &matrices[0]);
    if (!(total < 2))
    {
        Touch(&matrices[1]);
    }

    u32 index = 0;
    for (; index < meshCount;)
    {
        view->DrawnModelResult();
        u32 next = index + 1;
        if (next < total)
        {
            CallVirtual<void>(view, view->vtable, ChunkView::SlotLoadModel, &matrices[next]);
            if (index + 2 < total)
            {
                Touch(&matrices[index + 2]);
            }
        }

        Platform::Graphics::DrawPlacedModel(static_cast<RigidModel*>(items[index]), view, &matrices[index]);
        index = next;
    }

    while (index < total)
    {
        view->DrawnModelResult();
        u32 next = index + 1;
        if (next < total)
        {
            CallVirtual<void>(view, view->vtable, ChunkView::SlotLoadModel, &matrices[next]);
            if (index + 2 < total)
            {
                Touch(&matrices[index + 2]);
            }
        }

        RigidModel* model = Platform::Graphics::LodModelAt(static_cast<Lod*>(items[index]), static_cast<u32>(view->distance));
        if (model != nullptr)
        {
            Platform::Graphics::DrawPlacedModel(model, view, &matrices[index]);
        }

        index = next;
    }

    return total;
}

// Each item's box tested first: how many were in view
u32 SceneryMeshes::DrawCulled(ChunkView* view)
{
    u32 drawn = 0;
    if (items == nullptr || items[0] == nullptr)
    {
        return 0;
    }

    view->TestBoxOnVu0(&boxes[0], &matrices[0]);
    u32 index = 0;
    u32 total = meshCount + lodCount;
    for (; index < meshCount;)
    {
        u32 inView = view->MeshResult();
        u32 next = index + 1;
        if (next < total)
        {
            view->TestBoxOnVu0(&boxes[next], &matrices[next]);
            if (index + 2 < total)
            {
                Touch(&matrices[index + 2]);
            }
        }

        if (inView != 0)
        {
            drawn++;
            Platform::Graphics::DrawPlacedModel(static_cast<RigidModel*>(items[index]), view, &matrices[index]);
        }

        index = next;
    }

    while (index < total)
    {
        u32 inView = view->MeshResult();
        u32 next = index + 1;
        if (next < total)
        {
            view->TestBoxOnVu0(&boxes[next], &matrices[next]);
            if (index + 2 < total)
            {
                Touch(&matrices[index + 2]);
            }
        }

        if (inView != 0)
        {
            Matrix4x4* matrix = &matrices[index];
            RigidModel* model = Platform::Graphics::LodModelAt(static_cast<Lod*>(items[index]), static_cast<u32>(view->distance));
            if (model != nullptr)
            {
                Platform::Graphics::DrawPlacedModel(model, view, matrix);
            }

            drawn++;
        }

        index = next;
    }

    return drawn;
}

// The flags, name, colour filter palette, root type and unused byte; the sky and the lights when the flags say so;
// a root of the root's type read (made a node first, its children cleared, then made the root). The root's chunk is the chunk's
// data (when it has no root, address 0x30's word is written, retail)
void ReadScenery(ChunkData* chunk, Stream* stream)
{
    stream->Read(&chunk->flags, sizeof(chunk->flags), 1);
    StringRead(&chunk->name, stream);
    stream->ReadU32(&chunk->colourFilterPalette);
    s32 type;
    stream->ReadS32(&type);
    stream->ReadBool(reinterpret_cast<bool*>(&chunk->unusedByte));
    if (chunk->flags.hasSky != 0)
    {
        stream->ReadS32(reinterpret_cast<s32*>(&chunk->skyId));
        chunk->sky = g_SkyTable.Acquire(&chunk->skyId, nullptr);
    }

    if (chunk->flags.hasLights != 0)
    {
        ReadSceneryLights(chunk->lights, stream);
    }

    if (type == static_cast<s32>(SceneryRoot::TypeId))
    {
        auto* root = static_cast<SceneryRoot*>(MemoryAllocate(sizeof(SceneryRoot)));
        SceneryTree::Construct(root);
        root->vtable = g_SceneryNodeVTable;
        root->ClearChildren();
        root->vtable = g_SceneryRootVTable;
        root->Read(stream);
        chunk->scenery = root;
    }

    chunk->scenery->chunk = chunk;
}
