#include "game/objectcollision.h"

#include "game/animation.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/reference.h"

namespace
{
// The node kind with hulls of its own, and what it keeps them in: how many, the hulls
constexpr u32 HullsNodeKind = 4;

struct HullsHolder
{
    u32 unknown00;
    s32 count;
    CollisionHull* hulls;
};

struct HullsNode : GameNode
{
    HullsHolder* holder;
};

HullsNode* HullsNodeOf(ObjectCollision* collision)
{
    return static_cast<HullsNode*>(GetGameNode(&static_cast<InstanceContext*>(collision->owner)->nodes, HullsNodeKind));
}

void FreeHull(CollisionHull* hull)
{
    constexpr u32 DestroyAndFree = 3;
    HullDestroy(hull, DestroyAndFree);
}

// A box hull of its own in place of the one it had
void MakeBoxHull(ObjectCollision* collision, const Vector4* min, const Vector4* max)
{
    if (collision->hull != nullptr)
    {
        FreeHull(collision->hull);
    }

    collision->hull = HullConstruct(static_cast<CollisionHull*>(MemoryAllocate(sizeof(CollisionHull))));
    BuildBoxHull(collision->hull, min, max);
}

void SetBoxes(ObjectCollision* collision, const Vector4* min, const Vector4* max, u32 ownBoxToo)
{
    collision->box.min = *min;
    collision->box.max = *max;
    if (ownBoxToo != 0)
    {
        collision->ownBox.min = *min;
        collision->ownBox.max = *max;
    }
}
}

ObjectCollision* ConstructObjectCollision(ObjectCollision* collision, ReferencedObject* owner)
{
    // Bit 0 cleared, the rest of the bits are what the memory had
    collision->bits = ((collision->bits & ~u64{1}) | ObjectCollision::BitKeepsJointMatrices) & ~u64{ObjectCollision::BitThin};
    collision->cell = -1;
    collision->owner = owner;
    collision->hull = nullptr;
    collision->givenHull = nullptr;
    collision->cellLink = nullptr;
    collision->cellNode = nullptr;
    collision->leftOut = nullptr;
    collision->hullMatrix = nullptr;
    collision->ogi = nullptr;
    collision->jointMatrices = nullptr;
    collision->jointMatrixCount = 0;
    collision->jointMatricesOgi = nullptr;
    collision->surfaces = nullptr;
    return collision;
}

void DestroyObjectCollision(ObjectCollision* collision, u32 destroyFlags)
{
    if (collision->hull != nullptr)
    {
        FreeHull(collision->hull);
    }

    MemoryDeallocate2_(collision->hullMatrix);
    if (collision->jointMatrices != nullptr)
    {
        MemoryDeallocate_(collision->jointMatrices);
    }

    collision->jointMatrixCount = -1;
    if (collision->surfaces != nullptr)
    {
        MemoryDeallocate_(collision->surfaces);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(collision);
    }
}

s32 GetHullCount(ObjectCollision* collision)
{
    if (collision->hull != nullptr || collision->givenHull != nullptr)
    {
        return 1;
    }

    if (collision->ogi != nullptr)
    {
        return collision->ogi->hullCount;
    }

    if (HullsNodeOf(collision) == nullptr)
    {
        return 0;
    }

    return HullsNodeOf(collision)->holder->count;
}

CollisionHull* GetCollisionModel(ObjectCollision* collision, u32 index)
{
    if (collision->hull != nullptr)
    {
        return collision->hull;
    }

    if (collision->givenHull != nullptr)
    {
        return collision->givenHull;
    }

    if (collision->ogi != nullptr)
    {
        return &collision->ogi->hulls[index];
    }

    if (HullsNodeOf(collision) == nullptr)
    {
        return nullptr;
    }

    return &HullsNodeOf(collision)->holder->hulls[index];
}

u16 HullOwnSurface(ObjectCollision* collision, u32 index)
{
    CollisionHull* hull = collision->hull;
    if (hull == nullptr)
    {
        hull = collision->givenHull;
    }

    if (hull == nullptr)
    {
        if (collision->ogi == nullptr)
        {
            return 0xFFFF;
        }

        hull = &collision->ogi->hulls[index];
    }

    return static_cast<u16>(hull->surface);
}

CollisionSurface* GetHullSurface(ObjectCollision* collision, u32 index)
{
    u8 hull = static_cast<u8>(index);
    if (GetHullCount(collision) == 0)
    {
        return nullptr;
    }

    return &g_CollisionSurfaces.surfaces[collision->surfaces[hull]];
}

void AllocateHullSurfaces(ObjectCollision* collision, s32 count)
{
    if (collision->surfaces != nullptr)
    {
        MemoryDeallocate_(collision->surfaces);
    }

    collision->surfaces = static_cast<u16*>(MemoryAllocate2(count << 1));
}

void CopyHullSurfaces(ObjectCollision* collision)
{
    s32 count = GetHullCount(collision);
    if (count <= 0)
    {
        return;
    }

    AllocateHullSurfaces(collision, count);
    for (s32 index = 0; index < count; index++)
    {
        collision->surfaces[index] = HullOwnSurface(collision, index);
    }
}

void SetAllHullSurfaces(ObjectCollision* collision, u16 surface)
{
    s32 count = GetHullCount(collision);
    for (s32 index = 0; index < count; index++)
    {
        collision->surfaces[index] = surface;
    }
}

void SetHullOwnSurface(ObjectCollision* collision, u8 index, u16 surface)
{
    CollisionHull* hull = collision->hull;
    if (hull == nullptr)
    {
        if (collision->givenHull != nullptr)
        {
            collision->givenHull->surface = static_cast<s16>(surface);
            return;
        }

        if (collision->ogi != nullptr)
        {
            hull = &collision->ogi->hulls[index];
        }
        else if (HullsNodeOf(collision) != nullptr)
        {
            hull = &HullsNodeOf(collision)->holder->hulls[index];
        }
    }

    if (hull != nullptr)
    {
        hull->surface = static_cast<s16>(surface);
    }
}

u16 HullSurfaceIndex(ObjectCollision* collision, u8 index)
{
    if (!(index < GetHullCount(collision)))
    {
        return 0;
    }

    return collision->surfaces[index];
}

void GetHullAndMatrix(ObjectCollision* collision, const Matrix4x4* matrix, s32 index, CollisionHull** hull, Matrix4x4* out)
{
    constexpr s8 ModelSpace = -1;
    if (collision->hull != nullptr)
    {
        *hull = collision->hull;
        *out = *matrix;
        return;
    }

    if (collision->givenHull != nullptr)
    {
        *hull = collision->givenHull;
        *out = *matrix;
        return;
    }

    GameOGI* ogi = collision->ogi;
    if (ogi != nullptr)
    {
        s8 joint = static_cast<s8>(ogi->hullJoints[index]);
        *hull = &ogi->hulls[index];
        if (joint == ModelSpace)
        {
            *out = *matrix;
            return;
        }

        HullJointMatrix(collision, joint, index, out);
        MultiplyInPlace(out, matrix);
        return;
    }

    if (HullsNodeOf(collision) == nullptr)
    {
        *hull = nullptr;
        return;
    }

    *hull = &HullsNodeOf(collision)->holder->hulls[index];
    *out = *matrix;
}

void GetInstanceHull(ObjectCollision* collision, s32 index, CollisionHull** hull, Matrix4x4* matrix)
{
    auto* placed = static_cast<const Matrix4x4*>(UpdateObjectMatrix(collision->owner));
    GetHullAndMatrix(collision, placed, index, hull, matrix);
}

void GetInstanceHullBounds(ObjectCollision* collision, const Matrix4x4* matrix, Box* box)
{
    s32 count = GetHullCount(collision);
    if (count == 1)
    {
        CollisionHull* hull;
        Matrix4x4 placed;
        GetHullAndMatrix(collision, matrix, 0, &hull, &placed);
        GetHullBoundingBox(hull, &placed, box);
        return;
    }

    if (count <= 0)
    {
        box->min = collision->ownBox.min;
        box->max = collision->ownBox.max;
        TransformBox(box, matrix);
        return;
    }

    ResetBox(box);
    for (s32 index = 0; index < count; index++)
    {
        CollisionHull* hull;
        Matrix4x4 placed;
        GetHullAndMatrix(collision, matrix, index, &hull, &placed);
        Box hullBox;
        GetHullBoundingBox(hull, &placed, &hullBox);
        MergeBox(box, &hullBox);
    }
}

void HullJointMatrix(ObjectCollision* collision, u32 joint, s32 hull, Matrix4x4* out)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&static_cast<InstanceContext*>(collision->owner)->nodes, NodeModel));
    auto* animator = static_cast<OgiAnimator*>(model->unknown24);
    if (animator != nullptr && CopyJointTransform(animator, joint, out) != 0)
    {
        if ((collision->bits & ObjectCollision::BitKeepsJointMatrices) == 0)
        {
            return;
        }

        if (collision->jointMatrices == nullptr || collision->jointMatricesOgi != collision->ogi)
        {
            MakeJointMatrices(collision);
        }

        collision->jointMatrices[hull] = *out;
        return;
    }

    if (collision->ogi != collision->jointMatricesOgi || collision->jointMatrices == nullptr)
    {
        InitIdentityMatrix(out);
        return;
    }

    *out = collision->jointMatrices[hull];
}

void MakeJointMatrices(ObjectCollision* collision)
{
    if (collision->jointMatrices != nullptr)
    {
        MemoryDeallocate_(collision->jointMatrices);
    }

    GameOGI* ogi = collision->ogi;
    collision->jointMatricesOgi = ogi;
    s32 count = ogi->hullCount;
    collision->jointMatrixCount = count;
    collision->jointMatrices = static_cast<Matrix4x4*>(MemoryAllocate2(count << 6));
    for (s32 index = 0; index < collision->jointMatrixCount; index++)
    {
        InitIdentityMatrix(&collision->jointMatrices[index]);
    }

    AllocateHullSurfaces(collision, collision->jointMatrixCount);
    for (s32 index = 0; index < collision->jointMatrixCount; index++)
    {
        collision->surfaces[index] = HullOwnSurface(collision, index);
    }
}

void SetCollisionBox(ObjectCollision* collision, const Vector4* min, const Vector4* max, u32 makeHull, u32 ownBoxToo)
{
    if (makeHull != 0)
    {
        MakeBoxHull(collision, min, max);
    }

    SetBoxes(collision, min, max, ownBoxToo);
}

void SetCentredCollisionBox(ObjectCollision* collision, const Vector4* extent, u32 makeHull, u32 ownBoxToo)
{
    Vector4 low = *extent;
    low.x = -low.x;
    low.y = -low.y;
    low.z = -low.z;
    if (makeHull != 0)
    {
        MakeBoxHull(collision, &low, extent);
    }

    SetBoxes(collision, &low, extent, ownBoxToo);
}

void SetCollisionMatrix(ObjectCollision* collision, const Matrix4x4* matrix)
{
    if (collision->hullMatrix != nullptr)
    {
        MemoryDeallocate2_(collision->hullMatrix);
        collision->hullMatrix = nullptr;
    }

    collision->hullMatrix = static_cast<Matrix4x4*>(MemoryAllocate(sizeof(Matrix4x4)));
    *collision->hullMatrix = *matrix;
    GetInstanceHullBounds(collision, matrix, &collision->box);
}

void SetCollisionOgi(ObjectCollision* collision, GameOGI* ogi)
{
    if (ogi == nullptr)
    {
        collision->ownBox.max = g_DefaultBox.min;
        collision->ownBox.min = g_DefaultBox.min;
    }
    else
    {
        collision->ownBox.min = ogi->bounds.min;
        collision->ownBox.max = ogi->bounds.max;
    }

    collision->ogi = ogi;
    CopyHullSurfaces(collision);
    QueueObject(collision->owner);
}

void MarkThinAndCopySurfaces(ObjectCollision* collision)
{
    constexpr f32 Thin = Rounded(0.05);
    const Box& own = collision->ownBox;
    f32 x = __builtin_fabsf(own.max.x - own.min.x);
    f32 y = __builtin_fabsf(own.max.y - own.min.y);
    f32 z = __builtin_fabsf(own.max.z - own.min.z);
    f32 thinnest = x <= y ? __builtin_fminf(x, z) : __builtin_fminf(y, z);
    if (thinnest < Thin)
    {
        collision->bits |= ObjectCollision::BitThin;
    }

    CopyHullSurfaces(collision);
}

void* StepObjectCollision(ObjectCollision* collision)
{
    auto* owner = static_cast<InstanceContext*>(collision->owner);
    if (collision->jointMatricesOgi != collision->ogi)
    {
        MakeJointMatrices(collision);
    }

    if (collision->hullMatrix == nullptr)
    {
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        GetInstanceHullBounds(collision, &place->matrix, &collision->box);
    }

    if (collision->cellLink == nullptr)
    {
        return ChunkNoticeInstance(owner);
    }

    if (collision->hullMatrix != nullptr)
    {
        return owner->chunk;
    }

    return UpdateCollisionCell(collision, owner);
}

void SkipInQuery(InstanceRayHit* query, ReferencedObject* object)
{
    query->skipped[0] = object;
    query->skipped[1] = object->collision.leftOut;
}

u32 QueryTakes(const InstanceRayHit* query, const InstanceContext* instance, u32 kinds)
{
    u32 flags = instance->flags;
    if ((flags & query->unwantedFlags) != 0 || (instance->nodes.mask & kinds) == 0)
    {
        return 0;
    }

    if ((query->bits & InstanceRayHit::BitAllWanted) != 0)
    {
        return (flags & query->wantedFlags) == query->wantedFlags;
    }

    return (flags & query->wantedFlags) != 0;
}

u32 QueryAdd(InstanceRayHit* query, InstanceContext* instance)
{
    u16 count = query->count;
    if (!(count < query->most))
    {
        query->bits |= InstanceRayHit::BitFull;
        return 0;
    }

    query->count = count + 1;
    query->results[count] = instance;
    return 1;
}

u16 QueryCellList(InstanceRayHit* query, InstanceContext* first, u32 kinds)
{
    u16 before = query->count;
    for (InstanceContext* instance = first; instance != nullptr; instance = instance->cellNext)
    {
        if (QueryTakes(query, instance, kinds) != 0)
        {
            QueryAdd(query, instance);
        }
    }

    return query->count - before;
}
