#include "game/scenery.h"
#include "game/volumes.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/controllers.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/shadows.h"
#include "game/stream.h"
#include "game/string.h"
#include "game/view.h"
#include "platform/graphics.h"

EABI_EXPORT(FUN_001e97a8, InstanceCellsInCylinder);
EABI_EXPORT(FUN_001eac90, CellListInCylinder);

// A chunk's awake instances' collision is sorted into cells (chunk->instanceCells, 0x301 lists): three levels of 16 by 16 cells of
// 4, 8 and 20 units, an instance going in the level its box's size fits (under 2, 4 and 10 units across), the cell its place's
// x and z fall in (wrapping, the grid repeats every 16 cells), and everything bigger in the last cell. A query visits the cells
// a volume overlaps at every level, each grown by the level's margin
extern "C"
{
    extern const f32 g_CellSizes[4] RETAIL(D_002FBEB0);
    extern const f32 g_CellMargins[4] RETAIL(D_002FBEC0);
    extern const s32 g_CellRows[4] RETAIL(D_002FBED0);
    extern const s32 g_CellCounts[4] RETAIL(D_002FBEE0);
    extern const s32 g_CellMasks[4] RETAIL(D_002FBEF0);
    extern s32 g_CellBases[4] RETAIL(D_003B49C0);
    extern u32 g_Unknown30A908 RETAIL(D_0030A908);
    // Set while an instance moves from one chunk to another (MoveToChunk)
    extern u8 g_ChangingChunk RETAIL(D_00309FF3);
    // Whether the chunks the links lead to are drawn
    extern u8 g_DrawLinkedChunks RETAIL(D_00309FF2);
    // The instances whose drawing waits until the chunk's scenery is drawn (D_0030ACA8 in view, D_0030ACA4 clipped)
    extern s32 g_DeferredInViewCount RETAIL(D_0030ACA8);
    extern s32 g_DeferredClippedCount RETAIL(D_0030ACA4);
    extern s32 g_Unknown30ACAC RETAIL(D_0030ACAC);
    extern InstanceContext** g_DeferredInView[256] RETAIL(D_003D8600);
    extern InstanceContext** g_DeferredClipped[256] RETAIL(D_003D8A00);
    extern const char g_NonAgentObject[] RETAIL(D_002FB610);
}

namespace
{
constexpr u32 CellCount = 0x301;
constexpr u32 LastCell = 0x300;
constexpr u32 LevelCount = 3;
constexpr u32 CellPrevious = 0x148 + 1;
constexpr u32 CellNext = 0x14C + 1;
constexpr u32 DynamicFlag = 0x40000;
constexpr u32 SolidModelFlag = 0x80000;
constexpr f32 NoHit = 0x1.93E594p+99f;
constexpr f32 CellMargin = 0x1.A36E2Ep-15f;
constexpr u32 NodeObject = 1;
constexpr u32 HitRecordSize = 0x34;
constexpr u32 LinkVisibilityMask = 0x7F;
constexpr u32 LinkWallShift = 14;
constexpr u32 LinkWallMask = 0xC000;
constexpr u32 ReleasingState = 0x80000;
constexpr u32 ReleasedState = 0xC0000;
constexpr u32 DrawnBit = 0x800000;
constexpr u32 HasRootBit = 0x400000;

enum CellSlot : u32
{
    SlotRender = 3,
    SlotCollect = 14,
    SlotCellOf = 15,
    SlotSleepInstances = 22,
};

enum ViewSlot : u32
{
    ViewTestCell = 12,
};

enum InstanceSlot : u32
{
    InstanceSleep = 3,
    InstanceRelease = 4,
};

void PushCell(InstanceContext** list, InstanceContext* instance)
{
    InstanceListPushFront(instance, reinterpret_cast<void**>(list), CellPrevious, CellNext);
}

void RemoveCell(InstanceContext** list, InstanceContext* instance)
{
    ListRemove(instance, reinterpret_cast<void**>(list), CellPrevious, CellNext);
}

bool Skipped(const InstanceRayHit* query, const InstanceContext* instance)
{
    return query->skipped[0] == instance || query->skipped[1] == instance;
}

void Add(InstanceRayHit* query, InstanceContext* instance)
{
    u16 count = query->count;
    if (count < query->most)
    {
        query->count = count + 1;
        query->results[count] = instance;
        return;
    }

    query->bits |= InstanceRayHit::BitFull;
}

SceneryCell* RootOf(ChunkData* chunk)
{
    return reinterpret_cast<SceneryCell*>(chunk->scenery);
}

SceneryCell* CellOfInstance(SceneryCell* root, InstanceContext* instance, const Matrix4x4* matrix)
{
    return CallVirtual<SceneryCell*>(root, root->vtable, SlotCellOf, instance, matrix);
}

InstanceContext** MakeCells(ChunkData* chunk)
{
    if (chunk->instanceCells == nullptr)
    {
        auto* cells = static_cast<InstanceContext**>(MemoryAllocate(CellCount * sizeof(InstanceContext*)));
        for (s32 cell = LastCell; cell >= 0; cell--)
        {
            cells[cell] = nullptr;
        }

        chunk->instanceCells = cells;
    }

    return chunk->instanceCells;
}

// The instance off its scenery cell's list and its collision cell's
void LeaveCells(InstanceContext* instance)
{
    ObjectCollision* collision = &instance->collision;
    SceneryCell* cell = collision->sceneryCell;
    if (cell != nullptr)
    {
        if ((instance->flags & DynamicFlag) == DynamicFlag)
        {
            CellListRemove(&cell->dynamicInstances, instance);
        }
        else
        {
            CellListRemove(&cell->instances, instance);
        }

        collision->sceneryCell = nullptr;
    }

    InstanceContext** cells = collision->cells;
    if (cells != nullptr && collision->cell != -1)
    {
        RemoveCell(&cells[collision->cell], instance);
        collision->cell = -1;
    }
}

// A box test of a cell list: the instances the query takes whose collision box overlaps the sphere's box
u32 SphereBoxOverlaps(const Vector4* sphere, const Box* box)
{
    f32 radius = sphere->w;
    if (sphere->x + radius < box->min.x || box->max.x < sphere->x - radius)
    {
        return 0;
    }

    if (sphere->y + radius < box->min.y || box->max.y < sphere->y - radius)
    {
        return 0;
    }

    if (sphere->z + radius < box->min.z || box->max.z < sphere->z - radius)
    {
        return 0;
    }

    return 1;
}
}

extern "C"
{
// The level a box goes in by its size across (the larger of x and z)
s32 CellLevel(const Box* box)
{
    f32 size = __builtin_fmaxf(box->max.z - box->min.z, box->max.x - box->min.x);
    if (size < g_CellMargins[0])
    {
        return 0;
    }

    if (size < g_CellMargins[1])
    {
        return 1;
    }

    if (size < g_CellMargins[2])
    {
        return 2;
    }

    return 3;
}

s32 InstanceCell(InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    s32 level = CellLevel(&instance->collision.box);
    f32 size = g_CellSizes[level];
    s32 mask = g_CellMasks[level];
    s32 z = static_cast<s32>(place->matrix.m[3][2] / size);
    s32 x = static_cast<s32>(place->matrix.m[3][0] / size);
    return g_CellBases[level] + (x & mask) + (z & mask) * g_CellRows[level];
}

// The cells a volume's box overlaps at each level (its margin around it, at most a row's worth each way), then the last cell:
// how many
s32 CellsOfVolume(s32* cells, const BoundingVolume* volume)
{
    s32 count = 0;
    for (u32 level = 0; level < LevelCount; level++)
    {
        f32 margin = g_CellMargins[level];
        f32 size = g_CellSizes[level];
        s32 rows = g_CellRows[level];
        s32 lowX = static_cast<s32>((volume->min.x - margin) / size);
        s32 lowZ = static_cast<s32>((volume->min.z - margin) / size);
        s32 highX = static_cast<s32>((volume->max.x + margin) / size);
        s32 highZ = static_cast<s32>((volume->max.z + margin) / size);
        if (!(highX < lowX + rows - 1))
        {
            highX = lowX + rows - 1;
        }

        if (!(highZ < lowZ + rows - 1))
        {
            highZ = lowZ + rows - 1;
        }

        for (s32 x = lowX; x <= highX; x++)
        {
            s32 mask = g_CellMasks[level];
            s32 column = g_CellBases[level] + (x & mask);
            for (s32 z = lowZ; z <= highZ; z++)
            {
                count++;
                *cells++ = (z & mask) * g_CellRows[level] + column;
            }
        }
    }

    *cells = LastCell;
    return count + 1;
}

void InstanceCellsInSphere(InstanceContext** cells, const Vector4* sphere, u32 kinds, InstanceRayHit* query, u32 boxTest)
{
    s32 list[CellCount];
    BoundingVolume volume;
    Vector4 high = *sphere;
    Vector4 low = *sphere;
    f32 radius = sphere->w;
    high.x = high.x + radius;
    low.x = low.x - radius;
    low.y = low.y - radius;
    low.z = low.z - radius;
    high.y = high.y + radius;
    volume.vtable = g_BoxVolumeVTable;
    high.z = high.z + radius;
    SetBoundingVolume(&volume, &low, &high);
    s32 count = CellsOfVolume(list, &volume);
    InstanceContext** lists = cells;
    for (s32 index = 0; index < count; index++)
    {
        CellListInSphere(query, lists[list[index]], sphere, kinds, boxTest);
    }

    volume.vtable = g_VolumeVTable;
}

void InstanceCellsInCylinder(f32 height, InstanceContext** cells, const Vector4* base, u32 kinds, InstanceRayHit* query)
{
    s32 list[CellCount];
    BoundingVolume volume;
    f32 half = height * 0.5f;
    Vector4 low = *base;
    Vector4 high = *base;
    f32 radius = base->w;
    low.x = low.x - radius;
    low.y = low.y - half;
    volume.vtable = g_BoxVolumeVTable;
    low.z = low.z - radius;
    low.w = 1.0f;
    high.x = high.x + radius;
    high.y = high.y + half;
    high.z = high.z + radius;
    high.w = 1.0f;
    SetBoundingVolume(&volume, &low, &high);
    s32 count = CellsOfVolume(list, &volume);
    InstanceContext** lists = cells;
    for (s32 index = 0; index < count; index++)
    {
        CellListInCylinder(height, query, lists[list[index]], base, kinds);
    }

    volume.vtable = g_VolumeVTable;
}

// The nearest hit along the segment: with a record wanted (the flags are its address), the record of the nearest hit
f32 InstanceCellsRayCast(InstanceContext** cells, const Vector4* segment, u32 mask, InstanceRayHit* hit, u32 flags)
{
    s32 list[CellCount];
    BoundingVolume volume;
    volume.vtable = g_BoxVolumeVTable;
    Box box;
    ResetBox(&box);
    f32 nearest = NoHit;
    GrowBoxByPoint(&box, &segment[0]);
    GrowBoxByPoint(&box, &segment[1]);
    SetBoundingVolume(&volume, &box.min, &box.max);
    s32 count = CellsOfVolume(list, &volume);
    if (count > 0)
    {
        alignas(16) u8 record[HitRecordSize + 0xC];
        auto* out = reinterpret_cast<u8*>(flags);
        void* wanted = out != nullptr ? record : nullptr;
        InstanceContext** lists = cells;
        for (s32 index = 0; index < count; index++)
        {
            f32 share = CellListRayCast(hit, lists[list[index]], segment, mask, wanted);
            if (share < nearest)
            {
                nearest = share;
                if (out != nullptr)
                {
                    auto* to = reinterpret_cast<Vector4*>(out);
                    const auto* from = reinterpret_cast<const Vector4*>(record);
                    to[0] = from[0];
                    to[1] = from[1];
                    to[2] = from[2];
                    reinterpret_cast<u16*>(out)[0x18] = reinterpret_cast<const u16*>(record)[0x18];
                    reinterpret_cast<u16*>(out)[0x19] = reinterpret_cast<const u16*>(record)[0x19];
                }
            }
        }
    }

    volume.vtable = g_VolumeVTable;
    return nearest;
}

// A cell list's instances in a sphere (their place within its radius) or, testing boxes, whose box overlaps the sphere's
u16 CellListInSphere(InstanceRayHit* query, InstanceContext* first, const Vector4* sphere, u32 kinds, u32 boxTest)
{
    u16 before = query->count;
    if (boxTest != 0)
    {
        for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
        {
            if (Skipped(query, instance) || QueryTakes(query, instance, kinds) == 0)
            {
                continue;
            }

            if (SphereBoxOverlaps(sphere, &instance->collision.box) != 0)
            {
                Add(query, instance);
            }
        }

        return static_cast<u16>(query->count - before);
    }

    f32 squared = sphere->w * sphere->w;
    for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
    {
        if (Skipped(query, instance) || QueryTakes(query, instance, kinds) == 0)
        {
            continue;
        }

        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 position = place->position;
        f32 dx = sphere->x - position.x;
        f32 dy = sphere->y - position.y;
        f32 dz = sphere->z - position.z;
        if ((dx * dx + dy * dy) + dz * dz < squared)
        {
            Add(query, instance);
        }
    }

    return static_cast<u16>(query->count - before);
}

// A cell list's instances whose box spans the cylinder's height somewhere and whose place is within its radius across
u16 CellListInCylinder(f32 height, InstanceRayHit* query, InstanceContext* first, const Vector4* base, u32 kinds)
{
    f32 half = height * 0.5f;
    f32 top = base->y + half;
    f32 squared = base->w * base->w;
    u16 before = query->count;
    f32 bottom = base->y - half;
    for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
    {
        if (Skipped(query, instance) || QueryTakes(query, instance, kinds) == 0)
        {
            continue;
        }

        if (!(bottom < instance->collision.box.max.y) || !(instance->collision.box.min.y < top))
        {
            continue;
        }

        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 position = place->position;
        f32 dx = base->x - position.x;
        f32 dz = base->z - position.z;
        if (dx * dx + dz * dz < squared)
        {
            Add(query, instance);
        }
    }

    return static_cast<u16>(query->count - before);
}

// A cell list's instances touching a hull at a matrix: with points, the instances whose place is inside it; else those whose box
// the volume touches and (with a hull) one of whose hulls touches it
u16 CellListInHull(InstanceRayHit* query, InstanceContext* first, BoundingVolume* volume, const CollisionHull* hull,
                   const Matrix4x4* matrix, u32 kinds, u32 points)
{
    u16 before = query->count;
    if (points != 0)
    {
        Matrix4x4 inverse = *matrix;
        VuInvertRigidInPlace(&inverse);
        for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
        {
            if (Skipped(query, instance) || QueryTakes(query, instance, kinds) == 0)
            {
                continue;
            }

            auto* place = static_cast<ObjectPlace*>(UpdateObjectMatrix(instance));
            if (IsPointInsideHullAt(hull, reinterpret_cast<const Vector4*>(&place->matrix.m[3][0]), &inverse))
            {
                Add(query, instance);
            }
        }

        return static_cast<u16>(query->count - before);
    }

    for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
    {
        if (Skipped(query, instance) || QueryTakes(query, instance, kinds) == 0)
        {
            continue;
        }

        ObjectCollision* collision = &instance->collision;
        if (VolumeTouchesBox(volume, &collision->box) == 0)
        {
            continue;
        }

        u32 touches = 0;
        if (hull == nullptr)
        {
            touches = 1;
        }
        else
        {
            s32 count = GetHullCount(collision);
            for (s32 index = 0; index < count;)
            {
                CollisionHull* own;
                Matrix4x4 ownMatrix;
                GetInstanceHull(collision, index, &own, &ownMatrix);
                touches = HullsTouch(hull, matrix, own, &ownMatrix);
                index++;
                if (touches != 0)
                {
                    break;
                }
            }
        }

        if (touches != 0)
        {
            Add(query, instance);
        }
    }

    return static_cast<u16>(query->count - before);
}

// An instance's hulls cast at (when its box overlaps the segment's): the nearest share and, when wanted, its record; the
// instance added to the query when it was hit
f32 InstanceRayCast(InstanceRayHit* query, const Box* box, const Vector4* segment, InstanceContext* instance, void* hit)
{
    f32 nearest = NoHit;
    if (BoxesOverlap(&instance->collision.box, box) == 0)
    {
        return nearest;
    }

    ObjectCollision* collision = &instance->collision;
    s32 count = GetHullCount(collision);
    if (count > 0)
    {
        alignas(16) u8 record[HitRecordSize + 0xC];
        void* wanted = hit != nullptr ? record : nullptr;
        for (s32 index = 0; index < count; index++)
        {
            Matrix4x4 matrix;
            CollisionHull* hull;
            GetInstanceHull(collision, index, &hull, &matrix);
            f32 share;
            u32 result = HullRayCast(hull, &matrix, &segment[0], &segment[1], &share, wanted);
            if (!(result - 1 < 2) || !(share < nearest))
            {
                continue;
            }

            nearest = share;
            if (hit != nullptr)
            {
                auto* to = static_cast<Vector4*>(hit);
                const auto* from = reinterpret_cast<const Vector4*>(record);
                to[0] = from[0];
                to[1] = from[1];
                to[2] = from[2];
                static_cast<u16*>(hit)[0x18] = reinterpret_cast<const u16*>(record)[0x18];
                static_cast<u16*>(hit)[0x19] = reinterpret_cast<const u16*>(record)[0x19];
            }
        }
    }

    if (nearest == NoHit)
    {
        return NoHit;
    }

    Add(query, instance);
    return nearest;
}

// A cell list cast at: the query's distance and instance the nearest hit's
f32 CellListRayCast(InstanceRayHit* query, InstanceContext* first, const Vector4* segment, u32 mask, void* hit)
{
    Box box;
    ResetBox(&box);
    GrowBoxByPoint(&box, &segment[0]);
    GrowBoxByPoint(&box, &segment[1]);
    for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
    {
        if (Skipped(query, instance) || QueryTakes(query, instance, mask) == 0)
        {
            continue;
        }

        f32 share = InstanceRayCast(query, &box, segment, instance, hit);
        if (share < query->distance)
        {
            query->distance = share;
            query->instance = instance;
        }
    }

    return query->distance;
}

s32 QueryChunkInstances(ChunkData* chunk, const Box* box, u32 mask, InstanceRayHit* query)
{
    InstanceContext** lists = chunk->instanceCells;
    if (lists == nullptr)
    {
        return 0;
    }

    s32 list[CellCount];
    BoundingVolume volume;
    volume.vtable = g_BoxVolumeVTable;
    u16 before = query->count;
    SetBoundingVolume(&volume, &box->min, &box->max);
    s32 count = CellsOfVolume(list, &volume);
    for (s32 index = 0; index < count; index++)
    {
        CellListInHull(query, lists[list[index]], &volume, nullptr, nullptr, mask, 0);
    }

    volume.vtable = g_VolumeVTable;
    return static_cast<u16>(query->count - before);
}

// Every cell's instances with the flags
s32 QueryChunkInstancesByFlags(ChunkData* chunk, u32 flags, InstanceRayHit* query)
{
    InstanceContext** lists = chunk->instanceCells;
    if (lists == nullptr)
    {
        return 0;
    }

    u16 before = query->count;
    for (s32 cell = LastCell; cell >= 0; cell--)
    {
        QueryCellList(query, *lists++, flags);
    }

    return static_cast<u16>(query->count - before);
}

u32 ChunkInstancesInHull(ChunkData* chunk, CollisionHull* hull, const Matrix4x4* matrix, u32 kinds, InstanceRayHit* query,
                         u32 points)
{
    if (chunk->instanceCells == nullptr)
    {
        return 0;
    }

    s32 list[CellCount];
    BoundingVolume volume;
    volume.vtable = g_BoxVolumeVTable;
    u16 before = query->count;
    GetHullBounds(hull, matrix, &volume);
    InstanceContext** lists = chunk->instanceCells;
    s32 count = CellsOfVolume(list, &volume);
    for (s32 index = 0; index < count; index++)
    {
        CellListInHull(query, lists[list[index]], &volume, hull, matrix, kinds, points);
    }

    volume.vtable = g_VolumeVTable;
    return static_cast<u16>(query->count - before);
}

// The levels' first cells (their counts added up), at the program's start
void InitCellBases(s32 initialise, s32 priority)
{
    if (priority != 0xFFFF || initialise == 0)
    {
        return;
    }

    s32 first = g_CellCounts[0];
    s32 second = first + g_CellCounts[1];
    g_CellBases[0] = 0;
    g_CellBases[1] = first;
    g_CellBases[3] = second + g_CellCounts[2];
    g_CellBases[2] = second;
    g_Unknown30A908 = 0;
}

void CellsStaticInit()
{
    InitCellBases(1, 0xFFFF);
}

// An instance's collision moved to the cells its box is in now: a solid model's into its chunk's root, an instance out of its
// scenery cell's box into the cell the root finds for it (the object's name made and dropped when the chunk has no root: a
// leftover of a message), else into the collision cell of its place
ChunkData* UpdateCollisionCell(ObjectCollision* collision, InstanceContext* instance)
{
    if ((collision->owner->flags & SolidModelFlag) == SolidModelFlag)
    {
        return MoveToCell(collision, instance, RootOf(collision->sceneryCell->chunk));
    }

    if (CellHoldsBox(CellMargin, collision->sceneryCell, &collision->box.min, &collision->box.max) != 1)
    {
        SceneryCell* root = RootOf(collision->sceneryCell->chunk);
        if (root == nullptr)
        {
            auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
            String name = {};
            if (node == nullptr)
            {
                StringAssign(&name, g_NonAgentObject);
            }
            else if (node->sourceNode == nullptr)
            {
                StringAssign(&name, node->object->name.string);
            }
            else
            {
                StringAssign(&name, SourceObject(node->sourceNode)->name.string);
            }

            StringDestroy(&name);
        }

        // Without a root the vtable is read at address 0x44 (retail)
        SceneryCell* volatile owner = root;
        SceneryCell* cell = CellOfInstance(owner, instance, nullptr);
        if (cell != nullptr)
        {
            return MoveToCell(collision, instance, cell);
        }

        return static_cast<ChunkData*>(ChunkNoticeInstance(instance));
    }

    InstanceContext** cells = collision->cells;
    if (cells != nullptr)
    {
        s32 cell = InstanceCell(instance);
        s32 old = collision->cell;
        if (old == -1)
        {
            PushCell(&cells[cell], instance);
            collision->cell = cell;
        }
        else if (old != cell)
        {
            RemoveCell(&cells[old], instance);
            PushCell(&cells[cell], instance);
            collision->cell = cell;
        }

        collision->cells = cells;
    }

    return static_cast<ChunkData*>(ChunkNoticeInstance(instance));
}

// An awake instance put on the list of the scenery cell holding its box (the root when none does), and in its collision cell
u32 SortIntoCells(ObjectCollision* collision, SceneryCell* root, InstanceContext** cells, InstanceContext* instance)
{
    if (collision->hullMatrix == nullptr)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        GetInstanceHullBounds(collision, &place->matrix, &collision->box);
    }

    if (collision->sceneryCell != nullptr)
    {
        return 0;
    }

    collision->sceneryCell = CellOfInstance(root, instance, nullptr);
    if (collision->sceneryCell == nullptr)
    {
        collision->sceneryCell = root;
    }

    if ((instance->flags & DynamicFlag) == DynamicFlag)
    {
        CellListAdd(&collision->sceneryCell->dynamicInstances, instance);
    }
    else
    {
        CellListAdd(&collision->sceneryCell->instances, instance);
    }

    s32 cell = InstanceCell(instance);
    s32 old = collision->cell;
    if (old != -1)
    {
        if (old == cell)
        {
            collision->cells = cells;
            return 1;
        }

        RemoveCell(&cells[old], instance);
    }

    PushCell(&cells[cell], instance);
    collision->cell = cell;
    collision->cells = cells;
    return 1;
}

// The instance off its scenery cell's list onto another's: the chunk of the cell
ChunkData* MoveToCell(ObjectCollision* collision, InstanceContext* instance, SceneryCell* cell)
{
    SceneryCell* old = collision->sceneryCell;
    if (old != nullptr)
    {
        if ((instance->flags & DynamicFlag) == DynamicFlag)
        {
            CellListRemove(&old->dynamicInstances, instance);
        }
        else
        {
            CellListRemove(&old->instances, instance);
        }
    }

    if ((instance->flags & DynamicFlag) == DynamicFlag)
    {
        CellListAdd(&cell->dynamicInstances, instance);
    }
    else
    {
        CellListAdd(&cell->instances, instance);
    }

    collision->sceneryCell = cell;
    return cell->chunk;
}

// An instance out of its chunk (an awake one out of its cells first): whether its chunk let it go
u32 ChunkRemoveInstance(ChunkData* chunk, InstanceContext* instance)
{
    if (instance->chunk == nullptr)
    {
        return 0;
    }

    if ((instance->flags & 1) != 1)
    {
        LeaveCells(instance);
    }

    instance->chunk = nullptr;
    return chunk->instances->RemoveInstance(instance) & 0xFF;
}

// An object moved into a chunk: out of the one it's in (its cells left when it's awake), then into the chunk's cells when it's
// awake (put to sleep instead when the chunk has no instances)
u32 MoveToChunk(ChunkData* chunk, ReferencedObject* object)
{
    auto* instance = static_cast<InstanceContext*>(object);
    g_ChangingChunk = 0;
    ChunkData* old = instance->chunk;
    u32 awake = (instance->flags & 1) ^ 1;
    if (old != nullptr)
    {
        g_ChangingChunk = 1;
        if (awake != 0)
        {
            LeaveCells(instance);
        }

        instance->chunk = nullptr;
        if ((old->instances->RemoveInstance(instance) & 0xFF) == 0)
        {
            return 0;
        }

        g_ChangingChunk = 0;
    }

    if (chunk->instances == nullptr)
    {
        return CallVirtual<u32>(instance, instance->vtable, InstanceRelease);
    }

    instance->chunk = chunk;
    if (awake != 0)
    {
        InstanceContext** cells = MakeCells(chunk);
        SortIntoCells(&instance->collision, RootOf(chunk), cells, instance);
    }

    return chunk->instances->AddInstance(instance) & 0xFF;
}

u32 ChunkSleepInstance(ChunkData* chunk, InstanceContext* instance)
{
    if (instance->chunk == nullptr)
    {
        return 0;
    }

    if ((instance->flags & 1) != 1)
    {
        LeaveCells(instance);
    }

    return chunk->instances->SleepInstance(instance);
}

// The chunk an instance belongs in now: the deepest keep link holding it, else this chunk when its scenery has a cell for it,
// else the first link holding it (the instance moved through it), none when nothing does
ChunkData* ChunkNoticeInstance2(ChunkData* chunk, InstanceContext* instance)
{
    u32 deepest = 1;
    ChunkLinkData* found = nullptr;
    for (ChunkLinkData* link = chunk->links; link != nullptr; link = link->next)
    {
        u32 depth = static_cast<u32>((*reinterpret_cast<const u64*>(link) << 25) >> 32) & 0x7F;
        if (deepest < depth && LinkTakesInstance(link, instance) != 0)
        {
            deepest = depth;
            found = link;
        }
    }

    if (found == nullptr)
    {
        if (CellOfInstance(RootOf(chunk), instance, nullptr) != nullptr)
        {
            return chunk;
        }

        for (found = chunk->links; found != nullptr; found = found->next)
        {
            if (LinkTakesInstance(found, instance) != 0)
            {
                break;
            }
        }

        if (found == nullptr)
        {
            return nullptr;
        }
    }

    return instance->ChangeChunk(found);
}

u32 ChunkWakeInstance(ChunkData* chunk, InstanceContext* instance)
{
    if (instance->chunk == nullptr)
    {
        return 0;
    }

    u32 woken = chunk->instances->WakeInstance(instance);
    if ((instance->flags & 1) == 0)
    {
        return woken;
    }

    InstanceContext** cells = MakeCells(chunk);
    SortIntoCells(&instance->collision, RootOf(chunk), cells, instance);
    return woken;
}

u32 ChunkReleaseInstance(ChunkData* chunk, InstanceContext* instance)
{
    u32 removed = ChunkRemoveInstance(chunk, instance);
    if (chunk->instances == nullptr)
    {
        FreeInstance(instance);
    }
    else
    {
        chunk->instances->ReleaseInstance(instance);
    }

    return removed;
}

void ChunkSceneryRead(ChunkData* chunk)
{
    SceneryCell* root = RootOf(chunk);
    CallVirtual<void>(root, root->vtable, 9, chunk);
}

// An instance moved to the instances of no chunk (put to sleep first when it's awake), not while the chunk is being released
// when asked
u32 ChunkMakeGlobal(ChunkData* chunk, u32 checkState, u32 unknown, InstanceContext* instance)
{
    if (checkState != 0)
    {
        u32 state = chunk->bits & ChunkData::StateMask;
        if (state == ReleasingState || state == ReleasedState)
        {
            return 0;
        }
    }

    if ((instance->flags & 1) != 1)
    {
        CallVirtual<void>(instance, instance->vtable, InstanceSleep);
    }

    if (chunk->instances != nullptr)
    {
        chunk->instances->MakeGlobal(unknown, instance);
    }

    return 1;
}

// The same of the instances matching a filter: the rigid bodies let go of, the scenery's instances put to sleep
u32 ChunkMakeGlobalWhere(ChunkData* chunk, u32 checkState, u32 unknown, const u32* filter)
{
    if (checkState != 0)
    {
        u32 state = chunk->bits & ChunkData::StateMask;
        if (state == ReleasingState || state == ReleasedState)
        {
            return 0;
        }
    }

    if (chunk->unknown164 != nullptr)
    {
        ReleaseChunkRigidBodies(static_cast<ChunkRigidBodies*>(chunk->unknown164));
    }

    SceneryCell* root = RootOf(chunk);
    if (root != nullptr)
    {
        CallVirtual<void>(root, root->vtable, SlotSleepInstances, filter);
    }

    if (chunk->instances != nullptr)
    {
        chunk->instances->MakeGlobalWhere(unknown, filter);
    }

    return 1;
}

u32 CollectChunksInstances(ChunkList* list, u32 kinds, InstanceCollector* collector)
{
    for (ChunkData* chunk = list->first; chunk != nullptr; chunk = chunk->next)
    {
        SceneryCell* root = chunk->scenery;
        if (root != nullptr)
        {
            CallVirtual<void>(root, root->vtable, SlotCollect, kinds, collector);
        }
    }

    return InstanceCollectorFound(collector);
}

void ChunkListMakeGlobalWhere(ChunkList* list, u32 unknown, const u32* filter)
{
    ChunkData* current = list->current != nullptr ? list->current->data : nullptr;
    if (current != nullptr)
    {
        ChunkMakeGlobalWhere(current, 1, unknown, filter);
    }

    for (ChunkData* chunk = list->first; chunk != nullptr; chunk = chunk->next)
    {
        current = list->current != nullptr ? list->current->data : nullptr;
        if (chunk != current)
        {
            ChunkMakeGlobalWhere(chunk, 1, unknown, filter);
        }
    }
}

u32 LinkedChunkTakeInstance(ChunkLinkData* link, InstanceContext* instance)
{
    if (link->linkedData == nullptr)
    {
        return 0;
    }

    return MoveToChunk(link->linkedData, instance);
}

// The instance's place taken through the link's object matrix: queued when it changed
u32 MoveThroughLink(ChunkLinkData* link, InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Matrix4x4 moved;
    VuMultiplyMatrices(&place->matrix, reinterpret_cast<const Matrix4x4*>(link->objectMatrix), &moved);
    if (SetPlaceMatrix(instance->place, &moved) == 0)
    {
        return 0;
    }

    QueueObject(instance);
    return 1;
}

ChunkRigidBodies* ChunkRigidBodiesOf(ChunkData* chunk)
{
    if (chunk->unknown164 == nullptr)
    {
        chunk->unknown164 = ConstructChunkRigidBodies(MemoryAllocate(sizeof(ChunkRigidBodies)));
    }

    return static_cast<ChunkRigidBodies*>(chunk->unknown164);
}

ChunkLinkData* FindLinkTo(ChunkData* chunk, ChunkData* linked)
{
    for (ChunkLinkData* link = chunk->links; link != nullptr; link = link->next)
    {
        if (link->linkedData == linked)
        {
            return link;
        }
    }

    return nullptr;
}

// A matrix through a link: times its object matrix, or only its turn (the translation the default box's corner, w 1)
void TransformThroughLink(const ChunkLinkData* link, Matrix4x4* matrix, u32 whole)
{
    Matrix4x4 out;
    const auto* object = reinterpret_cast<const Matrix4x4*>(link->objectMatrix);
    if (whole != 0)
    {
        VuMultiplyMatrices(matrix, object, &out);
    }
    else
    {
        Matrix4x4 turn;
        *RowOf(&turn, 0) = *RowOf(object, 0);
        *RowOf(&turn, 1) = *RowOf(object, 1);
        *RowOf(&turn, 2) = *RowOf(object, 2);
        *RowOf(&turn, 3) = g_DefaultBox.min;
        turn.m[3][3] = 1.0f;
        VuMultiplyMatrices(matrix, &turn, &out);
    }

    *matrix = out;
}

// A vector the same way: the last row of a matrix whose other rows are the axes with a w of 1
void TransformVectorThroughLink(const ChunkLinkData* link, Vector4* vector, u32 whole)
{
    Matrix4x4 rows;
    rows.m[0][0] = 1.0f;
    rows.m[0][1] = 0.0f;
    rows.m[0][2] = 0.0f;
    rows.m[0][3] = 1.0f;
    rows.m[1][0] = 0.0f;
    rows.m[1][3] = 1.0f;
    rows.m[1][1] = 1.0f;
    rows.m[1][2] = 0.0f;
    rows.m[2][0] = 0.0f;
    rows.m[2][3] = 1.0f;
    rows.m[2][1] = 0.0f;
    rows.m[2][2] = 1.0f;
    *RowOf(&rows, 3) = *vector;
    Matrix4x4 out;
    const auto* object = reinterpret_cast<const Matrix4x4*>(link->objectMatrix);
    if (whole != 0)
    {
        VuMultiplyMatrices(&rows, object, &out);
    }
    else
    {
        Matrix4x4 turn;
        *RowOf(&turn, 0) = *RowOf(object, 0);
        *RowOf(&turn, 1) = *RowOf(object, 1);
        *RowOf(&turn, 2) = *RowOf(object, 2);
        *RowOf(&turn, 3) = g_DefaultBox.min;
        turn.m[3][3] = 1.0f;
        VuMultiplyMatrices(&rows, &turn, &out);
    }

    *vector = *RowOf(&out, 3);
}

// Whether a link takes an instance: through its load wall (its place in front of the wall, within 2, and inside its edges), or
// without one when the linked chunk's RM2 is loaded and it's a keep link, when the linked scenery has a cell for it through the
// link's object matrix
u32 LinkTakesInstance(const ChunkLinkData* link, InstanceContext* instance)
{
    auto* wall = static_cast<const LoadWall*>(link->loadWall);
    if (wall != nullptr)
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 position = place->position;
        return LoadWallHolds(wall, &position);
    }

    if (link->linkedData == nullptr || (link->flags & ChunkLinkData::LinkedRm2Loaded) == 0 ||
        (link->flags & ChunkLinkData::KeepMask) == 0)
    {
        return 0;
    }

    SceneryCell* root = RootOf(link->linkedData);
    return CellOfInstance(root, instance, reinterpret_cast<const Matrix4x4*>(link->objectMatrix)) != nullptr;
}

// The wall's plane through three of its corners, then its edges' planes (each through an edge along the wall's normal, facing
// the corners' middle) as the rows of a matrix (one plane per column)
void BuildLoadWall(LoadWall* wall, const Vector4* corners)
{
    PlaneFromTriangle(&wall->plane, &corners[0], &corners[2], &corners[1]);
    PlaneSideOf(&wall->plane, &g_DefaultBox.min);
    Vector4 middle = g_DefaultBox.min;
    for (u32 corner = 0; corner < 4; corner++)
    {
        wall->corners[corner] = corners[corner];
        middle.x = middle.x + corners[corner].x;
        middle.y = middle.y + corners[corner].y;
        middle.z = middle.z + corners[corner].z;
    }

    middle.z = middle.z * 0.25f;
    middle.x = middle.x * 0.25f;
    middle.y = middle.y * 0.25f;
    Vector4 planes[4];
    Vector4 along = {wall->plane.x, wall->plane.y, wall->plane.z, 1.0f};
    for (u32 edge = 0; edge < 4; edge++)
    {
        Vector4 from = corners[edge];
        Vector4 to = corners[(edge + 1) & 3];
        PlaneThroughEdge(&planes[edge], &from, &to, &along);
        if (PlaneSideOf(&planes[edge], &middle) == 3)
        {
            planes[edge].x = -planes[edge].x;
            planes[edge].y = -planes[edge].y;
            planes[edge].z = -planes[edge].z;
            planes[edge].w = -planes[edge].w;
        }
    }

    for (u32 edge = 0; edge < 4; edge++)
    {
        const f32* plane = &planes[edge].x;
        for (u32 component = 0; component < 4; component++)
        {
            wall->edges.m[component][edge] = plane[component];
        }
    }
}

// The planes a linked chunk is seen through a wall by from the eye: the wall's (facing the corners' middle), the four through the
// eye and an edge (facing it) and a sixth through the eye and the last edge
void PortalPlanes(const LoadWall* wall, Vector4* planes, const Vector4* eye)
{
    Vector4 middle = wall->corners[0];
    for (u32 corner = 1; corner < 4; corner++)
    {
        middle.x = middle.x + wall->corners[corner].x;
        middle.y = middle.y + wall->corners[corner].y;
        middle.z = middle.z + wall->corners[corner].z;
    }

    middle.z = middle.z * 0.25f;
    middle.x = middle.x * 0.25f;
    middle.y = middle.y * 0.25f;
    Vector4 triangle[3];
    triangle[2] = wall->corners[2];
    triangle[0] = wall->corners[0];
    triangle[1] = wall->corners[1];
    PlaneOfCorners(planes, 0, triangle);
    FacePlaneTowards(planes, 0, &middle);
    triangle[0] = *eye;
    for (s32 edge = 0; edge < 4; edge++)
    {
        s32 next = edge + 1 < 4 ? edge + 1 : 0;
        triangle[1] = wall->corners[edge];
        triangle[2] = wall->corners[next];
        PlaneOfCorners(planes, edge + 1, triangle);
        FacePlaneTowards(planes, edge + 1, &middle);
    }

    PlaneOfCorners(planes, 5, triangle);
}

// A point in front of the wall's plane by less than 2 and inside its four edges
u32 LoadWallHolds(const LoadWall* wall, const Vector4* point)
{
    f32 distance = ((wall->plane.x * point->x + wall->plane.y * point->y) + wall->plane.z * point->z) + wall->plane.w;
    if (!(distance < 2.0f) || !(0.0f < distance))
    {
        return 0;
    }

    if (PlaneSideOf(&wall->plane, point) != 1)
    {
        return 0;
    }

    Vector4 sides;
    VuTransformByRows(&wall->edges, point, &sides);
    return 0.0f <= sides.x && 0.0f <= sides.y && 0.0f <= sides.z && 0.0f <= sides.w;
}

ChunkLinkData* LinkMatricesConstruct(ChunkLinkData* data)
{
    data->linkedData = nullptr;
    data->next = nullptr;
    data->previous = nullptr;
    data->loadWall = nullptr;
    data->flags = 0;
    return data;
}

void LinkMatricesDestroy(ChunkLinkData* data, u32 flags)
{
    if (data->loadWall != nullptr)
    {
        LoadWallDestroy(static_cast<LoadWall*>(data->loadWall), 3);
    }

    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(data);
    }
}

// Its flags (bits 16 to 18 cleared), its object and chunk matrices and, with bit 19, its load wall
void LinkMatricesRead(ChunkLinkData* data, Stream* reader)
{
    constexpr u32 ReadCleared = 0x70000;
    reader->Read(&data->flags, 4, 1);
    data->flags &= ~ReadCleared;
    reader->Read(data->objectMatrix, sizeof(data->objectMatrix), 1);
    reader->Read(data->chunkMatrix, sizeof(data->chunkMatrix), 1);
    if ((data->flags & ChunkLinkData::HasLoadWall) == 0)
    {
        data->loadWall = nullptr;
        return;
    }

    data->loadWall = LoadLoadWall(static_cast<LoadWall*>(MemoryAllocate(sizeof(LoadWall))), reader);
}

// A rotation turned by the link's object matrix
void TransformRotationThroughLink(const ChunkLinkData* link, Vector4* rotation)
{
    Matrix4x4 turn;
    MatrixFromRotation(&turn, rotation);
    *RowOf(&turn, 3) = g_DefaultBox.min;
    turn.m[3][3] = 1.0f;
    Matrix4x4 turned;
    VuMultiplyMatrices(&turn, reinterpret_cast<const Matrix4x4*>(link->objectMatrix), &turned);
    GetRotationVec(rotation, &turned);
}

LoadWall* LoadLoadWall(LoadWall* wall, Stream* reader)
{
    ReadLoadWall(wall, reader);
    return wall;
}

LoadWall* LoadWallConstruct(LoadWall* wall)
{
    return wall;
}

void LoadWallDestroy(LoadWall* wall, u32 flags)
{
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(wall);
    }
}

// Its four corners read, the rest worked out from them
void ReadLoadWall(LoadWall* wall, Stream* reader)
{
    reader->Read(wall->corners, sizeof(wall->corners), 1);
    BuildLoadWall(wall, wall->corners);
}

// The corners' middle
void WallMiddle(const Vector4* corners, Vector4* middle)
{
    middle->x = 0.0f;
    middle->w = 1.0f;
    middle->y = 0.0f;
    middle->z = 0.0f;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    for (u32 corner = 0; corner < 4; corner++)
    {
        x = middle->x + corners[corner].x;
        middle->x = x;
        y = middle->y + corners[corner].y;
        middle->y = y;
        z = middle->z + corners[corner].z;
        middle->z = z;
    }

    middle->z = z * 0.25f;
    middle->x = x * 0.25f;
    middle->y = y * 0.25f;
}

void TransformPlanes(Vector4* planes, const Matrix4x4* matrix)
{
    for (u32 plane = 0; plane < 6; plane++)
    {
        TransformPlaneInPlace(&planes[plane], matrix);
    }
}

void TransformPlaneOf(const Vector4* planes, s32 index, const Matrix4x4* matrix, Vector4* out)
{
    TransformPlane(&planes[index], matrix, out);
}

// The plane turned around when the point is behind it
void FacePlaneTowards(Vector4* planes, s32 index, const Vector4* point)
{
    Vector4* plane = &planes[index];
    if (PlaneSideOf(plane, point) == 3)
    {
        plane->x = -plane->x;
        plane->y = -plane->y;
        plane->z = -plane->z;
        plane->w = -plane->w;
    }
}

void PlaneOfCorners(Vector4* planes, s32 index, const Vector4* corners)
{
    PlaneFromTriangle(&planes[index], &corners[0], &corners[1], &corners[2]);
}

// The view loaded for a chunk from the camera's place in it (its matrices: the place, the view's matrix to the screen through
// it, the draw matrix's inverse)
void SetUpChunkView(Matrix4x4* matrices, ChunkView* view, const ObjectPlace* place)
{
    matrices[0] = place->matrix;
    Platform::Graphics::LoadChunkPlanes(&matrices[0]);
    view->matrices = matrices;
    VuMultiplyMatrices(&matrices[2], &view->view->toScreen, &matrices[1]);
    VuInvertRigid(&matrices[3], &matrices[2]);
}

// The same through a portal's planes
void SetUpLinkedChunkView(Matrix4x4* matrices, ChunkView* view, const ObjectPlace* place, const Vector4* portal)
{
    matrices[0] = place->matrix;
    Platform::Graphics::LoadPortalPlanes(&matrices[0], portal);
    view->matrices = matrices;
    VuMultiplyMatrices(&matrices[2], &view->view->toScreen, &matrices[1]);
    VuInvertRigid(&matrices[3], &matrices[2]);
}

// The chunk a link leads to drawn when it's shown and visible: its draw matrix the link's chunk matrix, through a linked view,
// the camera's place taken into it, seen through the load wall's portal when the wall was partly in view
void DrawLinkedChunk(ChunkLinkData* link, ChunkView* view, const ObjectPlace* place, s32 depth)
{
    ChunkData* linked = link->linkedData;
    if (linked == nullptr || (link->flags & ChunkLinkData::HasLinkedData) == 0 || (link->flags & LinkVisibilityMask) == 0)
    {
        return;
    }

    const auto* chunkMatrix = reinterpret_cast<const Matrix4x4*>(link->chunkMatrix);
    const auto* objectMatrix = reinterpret_cast<const Matrix4x4*>(link->objectMatrix);
    linked->drawMatrix = *chunkMatrix;
    LinkedChunkView linkedView;
    LinkedChunkView::Construct(&linkedView, chunkMatrix, objectMatrix);
    linkedView.camera = view->camera;
    ObjectPlace moved;
    VuMultiplyMatrices(&place->matrix, objectMatrix, &moved.matrix);
    Vector4 portal[6];
    const Vector4* through = nullptr;
    bool walled = false;
    if (link->loadWall != nullptr)
    {
        walled = !((link->flags & LinkVisibilityMask) < 2);
    }

    if (walled && ((link->flags & LinkWallMask) >> LinkWallShift) == 1)
    {
        PortalPlanes(static_cast<const LoadWall*>(link->loadWall), portal, reinterpret_cast<const Vector4*>(&place->matrix.m[3][0]));
        TransformPlanes(portal, objectMatrix);
        through = portal;
    }

    DrawChunk(link->linkedData, &linkedView, &moved, depth, through);
    linkedView.vtable = g_LinkedChunkViewVTable;
    linkedView.ChunkView::Destroy(2);
}

// Which links' walls are in view (bits 14 and 15 of their flags): a wall's box tested while the last one's outcome is read; a
// link without a wall or always shown counts as in view
void LinksVisibility(ChunkData* chunk, ChunkView* view)
{
    ChunkLinkData* tested = nullptr;
    for (ChunkLinkData* link = chunk->links; link != nullptr; link = link->next)
    {
        auto* wall = static_cast<const LoadWall*>(link->loadWall);
        if (wall == nullptr || (link->flags & LinkVisibilityMask) == 1)
        {
            link->flags = (link->flags & ~LinkWallMask) | (1 << LinkWallShift);
            continue;
        }

        Box box;
        ResetBox(&box);
        for (u32 corner = 0; corner < 4; corner++)
        {
            GrowBoxByPoint(&box, &wall->corners[corner]);
        }

        if (tested != nullptr)
        {
            u32 visibility = view->CellResult();
            tested->flags = (tested->flags & ~LinkWallMask) | (visibility & 3) << LinkWallShift;
        }

        CallVirtual<void>(view, view->vtable, ViewTestCell, &box);
        tested = link;
    }

    if (tested != nullptr)
    {
        u32 visibility = view->CellResult();
        tested->flags = (tested->flags & ~LinkWallMask) | (visibility & 3) << LinkWallShift;
    }
}

// A chunk's scenery drawn once a frame (bit 22: it has a root; the low half of its bits the frame it was drawn in; bit 23:
// something of it was), then the chunks its links lead to, a level deeper each
void DrawChunk(ChunkData* chunk, ChunkView* view, const ObjectPlace* place, s32 depth, const Vector4* portal)
{
    bool hasRoot = chunk->scenery != nullptr;
    chunk->bits = (chunk->bits & ~HasRootBit) | (hasRoot ? HasRootBit : 0);
    u32 frame = g_RenderedFrames;
    if (!hasRoot || (chunk->bits & 0xFFFF) == (frame & 0xFFFF))
    {
        return;
    }

    *reinterpret_cast<u16*>(&chunk->bits) = static_cast<u16>(frame);
    auto* matrices = &chunk->matrix;
    if (portal == nullptr)
    {
        SetUpChunkView(matrices, view, place);
    }
    else
    {
        SetUpLinkedChunkView(matrices, view, place, portal);
    }

    chunk->drawnFrame = frame;
    SceneryCell* root = RootOf(chunk);
    s32 drawn = static_cast<s32>(CallVirtual<u32>(root, root->vtable, SlotRender, 2, view, chunk));
    DrawDeferredInstances(view);
    chunk->bits = (chunk->bits & ~DrawnBit) | (0 < drawn ? DrawnBit : 0);
    if (g_DrawLinkedChunks == 0 || !(0 < depth))
    {
        return;
    }

    LinksVisibility(chunk, view);
    for (ChunkLinkData* link = chunk->links; link != nullptr; link = link->next)
    {
        if ((link->flags & LinkWallMask) != 0)
        {
            DrawLinkedChunk(link, view, place, depth - 1);
        }
    }
}

// The shadows cast in a shown chunk drawn (its lists only swapped when nothing of it was drawn), with the view's planes in its
// space; then the chunks its shown links lead to, a level deeper each
void DrawChunkShadows(ChunkData* chunk, s32 depth, const Matrix4x4* toCamera, const Matrix4x4* matrix)
{
    ChunkShadows* shadows = chunk->shadows;
    if ((chunk->bits & ChunkData::StateMask) == ChunkData::Shown && shadows != nullptr && *shadows->currentCount != 0)
    {
        if ((chunk->bits & DrawnBit) == 0)
        {
            shadows->Swap();
        }
        else
        {
            Vector4 planes[6];
            const Vector4* view = Platform::Graphics::ViewPlanes();
            for (u32 plane = 0; plane < 6; plane++)
            {
                planes[plane] = view[plane];
            }

            TransformPlanes(planes, &chunk->matrix);
            Matrix4x4 through;
            VuMultiplyMatrices(matrix, toCamera, &through);
            shadows->Draw(planes, &through, matrix);
        }
    }

    if (!(0 < depth))
    {
        return;
    }

    for (ChunkLinkData* link = chunk->links; link != nullptr; link = link->next)
    {
        ChunkData* linked = link->linkedData;
        if (linked != nullptr && (link->flags & ChunkLinkData::HasLinkedData) != 0 && (link->flags & LinkVisibilityMask) != 0)
        {
            DrawChunkShadows(linked, depth - 1, toCamera, reinterpret_cast<const Matrix4x4*>(link->chunkMatrix));
        }
    }
}

// The camera's chunk drawn: its view loaded, the frame's lists of instances to draw emptied, its scenery and links' drawn, then
// the instances
void DrawScene(ChunkData* chunk, const ObjectPlace* place, RenderView* renderView)
{
    ChunkView view;
    ChunkView::ConstructFor(&view, renderView);
    g_DeferredClippedCount = 0;
    g_DrawnClippedEnd = 0x3FF;
    g_DeferredInViewCount = 0;
    g_Unknown30ACAC = 0;
    g_DrawnInViewCount = 0;
    g_DrawnInstanceCount = 0;
    view.camera = *reinterpret_cast<const Vector4*>(&place->matrix.m[3][0]);
    view.Load();
    DrawChunk(chunk, &view, place, 1, nullptr);
    DrawQueuedInstances();
    view.Destroy(2);
}

void DrawShadows(ChunkData* chunk, const Matrix4x4* toCamera, const Matrix4x4* matrix)
{
    if (chunk->shadows == nullptr)
    {
        return;
    }

    Platform::Graphics::BeginShadows();
    DrawChunkShadows(chunk, 1, toCamera, matrix);
    Platform::Graphics::EndShadows();
}

// The instances whose drawing waited, put in the frame's lists now (the counts emptied)
void DrawDeferredInstances(ChunkView* view)
{
    if (g_DeferredInViewCount != 0)
    {
        for (s32 index = 0; index < g_DeferredInViewCount; index++)
        {
            DrawCellInstances(g_DeferredInView[index], view);
        }
    }

    if (g_DeferredClippedCount != 0)
    {
        for (s32 index = 0; index < g_DeferredClippedCount; index++)
        {
            DrawCellInstancesCulled(g_DeferredClipped[index], view);
        }
    }

    g_DeferredInViewCount = 0;
    g_DeferredClippedCount = 0;
}

// The reverb's settings (its type byte, its bits 8 and 9, delay, feedback and depth), and the box reverb's
void SetChunkReverb(ChunkData* chunk, const void* arguments)
{
    const auto* words = static_cast<const u32*>(arguments);
    ReverbSettings* reverb = &chunk->reverb;
    *reinterpret_cast<u8*>(&reverb->bits) = static_cast<u8>(words[0]);
    reverb->bits = (reverb->bits & ~0x100u) | (words[0] & 0x100);
    reverb->bits = (reverb->bits & ~0x200u) | (words[0] & 0x200);
    reverb->depth = *reinterpret_cast<const f32*>(&words[3]);
    reverb->delay = *reinterpret_cast<const f32*>(&words[1]);
    reverb->feedback = *reinterpret_cast<const f32*>(&words[2]);
}

void SetChunkBoxReverb(ChunkData* chunk, const void* arguments)
{
    const auto* words = static_cast<const u32*>(arguments);
    ReverbSettings* reverb = &chunk->boxReverb;
    *reinterpret_cast<u8*>(&reverb->bits) = static_cast<u8>(words[0]);
    reverb->bits = (reverb->bits & ~0x100u) | (words[0] & 0x100);
    reverb->bits = (reverb->bits & ~0x200u) | (words[0] & 0x200);
    reverb->depth = *reinterpret_cast<const f32*>(&words[3]);
    reverb->delay = *reinterpret_cast<const f32*>(&words[1]);
    reverb->feedback = *reinterpret_cast<const f32*>(&words[2]);
}
}
