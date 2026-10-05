#include "game/chunkdata.h"
#include "game/hull.h"
#include "game/list.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/scenery.h"

// The rest of the dynamic scenery's retail module: the builder of the SM2's items by their class IDs (scenery cells, chunks' data,
// load walls, hulls) and the list iterator whose vtable follows its own, the box hulls the agents' code shares, an object's own box
// hull let go of, the hull builder's statics, and a segment clipped by six planes

extern "C"
{
    extern const GccVTableEntry g_LinkedListIteratorBaseVTable[] RETAIL(D_002FC500);
    // The items' builders' base (BuilderBaseFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);

    // The SM2 items' builder (a vtable alone, D_002FC490, the game context's): an item of a class (the scenery's cells: a node, a
    // root or a leaf, for the base cell's class too; a chunk's data, a load wall or a hull), none for another; its destructor
    void* MakeSceneryItem(void* builder, u32 classId) RETAIL(FUN_001fdde0);
    void DestroySceneryItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_00201268);
    // A segment (two points) clipped by six planes, a point inside one where n·p + w >= 0: an end outside a plane is moved along the
    // segment towards the other end by its distance from the plane (which only puts it on the plane when the segment is square to
    // it), the length left going down as much. Whether any length is left, the clipped segment in the third argument
    u32 ClipSegmentToPlanes(const Vector4* planes, const Vector4* segment, Vector4* clipped) RETAIL(FUN_001fdf90);
}

namespace
{
constexpr u32 LoadWallClassId = 0x160F;
constexpr u32 ChunkDataClassId = 0x1611;
constexpr u32 HullClassId = 0x1616;
constexpr s32 PlaneCount = 6;
// A segment this short has no direction to move its ends along
constexpr f32 NoLength = Epsilon;
}

void* MakeSceneryItem(void*, u32 classId)
{
    switch (classId)
    {
    case SceneryNode::TypeId:
        return SceneryNode::Construct(static_cast<SceneryNode*>(MemoryAllocate(sizeof(SceneryNode))));
    case SceneryLeaf::TypeId:
    case SceneryCell::TypeId:
        return SceneryLeaf::Construct(static_cast<SceneryLeaf*>(MemoryAllocate(sizeof(SceneryLeaf))));
    case SceneryRoot::TypeId:
    {
        auto* root = static_cast<SceneryRoot*>(MemoryAllocate(sizeof(SceneryRoot)));
        SceneryNode::Construct(root);
        root->vtable = g_SceneryRootVTable;
        return root;
    }
    case LoadWallClassId:
        return LoadWallConstruct(static_cast<LoadWall*>(MemoryAllocate(sizeof(LoadWall))));
    case ChunkDataClassId:
    {
        auto* chunk = static_cast<ChunkData*>(MemoryAllocate(sizeof(ChunkData)));
        return ConstructChunkData(chunk, GetChunkList(), nullptr);
    }
    case HullClassId:
        return HullConstruct(static_cast<CollisionHull*>(MemoryAllocate(sizeof(CollisionHull))));
    default:
        return nullptr;
    }
}

void DestroySceneryItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void LinkedListIterator::Destroy(u32 destroyFlags)
{
    vtable = g_LinkedListIteratorBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void LinkedListIterator::First()
{
    current = list->first;
}

u32 LinkedListIterator::IsDone()
{
    return current == nullptr;
}

void* LinkedListIterator::Current()
{
    return current->item;
}

void LinkedListIterator::Next()
{
    current = current->next;
}

void LinkedListIterator::Previous()
{
    current = current->previous;
}

void LinkedListIterator::Last()
{
    current = list->last;
}

LinkedListIterator* LinkedListIterator::Assign(const LinkedListIterator* other)
{
    list = other->list;
    current = other->current;
    return this;
}

BoxHullCache* ConstructBoxHullCache(BoxHullCache* cache)
{
    cache->count = 0;
    return cache;
}

CollisionHull* BoxHullOf(BoxHullCache* cache, const Vector4* min, const Vector4* max)
{
    s32 count = cache->count;
    s32 index = 0;
    for (; index < count; index++)
    {
        const Box* box = &cache->boxes[index];
        if (box->min.x == min->x && box->min.y == min->y && box->min.z == min->z && box->max.x == max->x &&
            box->max.y == max->y && box->max.z == max->z)
        {
            break;
        }
    }

    if (index != cache->count)
    {
        return cache->hulls[index];
    }

    // Retail doesn't check for room: a ninth box would be written over the hulls, and its hull over the count
    cache->boxes[cache->count].min = *min;
    cache->boxes[cache->count].max = *max;
    CollisionHull** slot = &cache->hulls[cache->count];
    CollisionHull* hull = HullConstruct(static_cast<CollisionHull*>(MemoryAllocate(sizeof(CollisionHull))));
    *slot = hull;
    BuildBoxHull(hull, min, max);
    cache->count++;
    return hull;
}

void ReleaseCollisionHull(ObjectCollision* collision)
{
    if (collision->hull != nullptr)
    {
        HullDestroy(collision->hull, DestroyAndFree);
    }

    collision->hull = nullptr;
}

void InitHullBuilderStatics(s32, s32)
{
}

void HullStaticInit()
{
    InitHullBuilderStatics(1, DefaultInitPriority);
}

u32 ClipSegmentToPlanes(const Vector4* planes, const Vector4* segment, Vector4* clipped)
{
    Vector4 direction = segment[1];
    direction.x -= segment[0].x;
    direction.y -= segment[0].y;
    direction.z -= segment[0].z;
    f32 length = __builtin_sqrtf((direction.x * direction.x + direction.y * direction.y) + direction.z * direction.z);
    if (NoLength < length)
    {
        f32 inverse = 1.0f / length;
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
    }

    clipped[0] = segment[0];
    clipped[1] = segment[1];
    for (s32 index = 0; index < PlaneCount; index++)
    {
        const Vector4* plane = &planes[index];
        f32 startDistance = plane->x * clipped[0].x + plane->y * clipped[0].y + plane->z * clipped[0].z + plane->w;
        f32 endDistance = plane->x * clipped[1].x + plane->y * clipped[1].y + plane->z * clipped[1].z + plane->w;
        if (startDistance < 0.0f)
        {
            if (length <= -startDistance)
            {
                return 0;
            }

            length += startDistance;
            clipped[0].x -= direction.x * startDistance;
            clipped[0].y -= direction.y * startDistance;
            clipped[0].z -= direction.z * startDistance;
        }

        if (endDistance < 0.0f)
        {
            if (length <= -endDistance)
            {
                return 0;
            }

            length += endDistance;
            clipped[1].x += direction.x * endDistance;
            clipped[1].y += direction.y * endDistance;
            clipped[1].z += direction.z * endDistance;
        }
    }

    return 1;
}
