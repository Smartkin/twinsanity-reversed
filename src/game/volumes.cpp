#include "game/volumes.h"

#include "game/chunkfiles.h"
#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/scenery.h"
#include "game/stream.h"
#include "game/string.h"

EABI_EXPORT(FUN_001ff288, &BoundingVolume::TestSphere);
EABI_EXPORT(FUN_001ff4d8, &BoundingVolume::TestSphereInBox);
EABI_EXPORT(FUN_00200148, &SphereVolume::Construct);
EABI_EXPORT(FUN_00200220, &SphereVolume::TestSphere);
EABI_EXPORT(FUN_001f8390, &CapsuleVolume::Construct);
EABI_EXPORT(FUN_001ffbd0, &CapsuleVolume::TestSphere);
EABI_EXPORT(FUN_001ffd58, &CapsuleVolume::TouchesSphere);
EABI_EXPORT(FUN_001fa4b0, CellHoldsBox);

extern "C"
{
    extern const GccVTableEntry g_ListIteratorVTable[] RETAIL(D_002FC500);
    extern const GccVTableEntry g_ChunkLoadingUtilBaseVTable[] RETAIL(D_002FC2E0);
    // The extensions of a chunk's other files
    extern const char g_SnExtension[] RETAIL(D_0030A018);
    extern const char g_LvlExtension[] RETAIL(D_0030A020);
    extern const char g_LgtExtension[] RETAIL(D_0030A028);
    extern const char g_ScaExtension[] RETAIL(D_0030A030);
    extern const char g_LkExtension[] RETAIL(D_0030A038);
    // The module's static initialisation (there's nothing to make) and its static constructor
    void InitChunkViews(s32 initialise, s32 priority) RETAIL(FUN_001f57f8);
    void ChunkViewsStaticInit() RETAIL(FUN_001f68b8);
}

namespace
{
// The volumes' vtable slots called through
enum VolumeSlot : u32
{
    SlotTestPoint = 4,
    SlotTestSphere = 5,
    SlotTestSegment = 6,
    SlotSegmentCrossing = 7,
    SlotTestVolume = 10,
    SlotType = 14,
    SlotTestSphereInBox = 17,
};

// A segment whose length squared is below its sphere's radius squared times this has no direction
constexpr f32 ShortSegment = LengthEpsilon;
// How far a quadratic's root may be before a segment's start, or past its end, and still be the share where it crosses a sphere
constexpr f32 RootTolerance = Epsilon;
constexpr f32 LastRoot = 0x1.000346p+0f;
// How far a point is in front of a hull's plane to be outside it
constexpr f32 PlaneTolerance = Epsilon;
// Further beyond a box than a point ever is
constexpr f32 NoFace = 1.0e10f;

// Where a point is against a box (an outcode): beyond its max or its min along each axis
union BoxOutCode
{
    // An axis' two bits
    enum AxisMask : u32
    {
        BeyondX = 0x3,
        BeyondY = 0xC,
        BeyondZ = 0x30,
    };

    u32 value;
    struct
    {
        u32 aboveX : 1;
        u32 belowX : 1;
        u32 aboveY : 1;
        u32 belowY : 1;
        u32 aboveZ : 1;
        u32 belowZ : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(BoxOutCode, 4);

f32* Axes(Vector4* vector)
{
    return &vector->x;
}

const f32* Axes(const Vector4* vector)
{
    return &vector->x;
}

u32 TypeOf(Volume* volume)
{
    return CallVirtual<u32>(volume, volume->vtable, SlotType);
}

// Whether two volumes' spheres meet
bool SpheresMeet(const Volume* volume, const Volume* other)
{
    f32 x = volume->sphere.x - other->sphere.x;
    f32 y = volume->sphere.y - other->sphere.y;
    f32 z = volume->sphere.z - other->sphere.z;
    f32 reach = volume->sphere.w + other->sphere.w;
    return x * x + y * y + z * z <= reach * reach;
}

// The types tested as boxes: the scenery's cells' types are too (their boxes aren't where a volume's are, nothing passes one)
bool IsBoxType(u32 type)
{
    return type == SceneryNode::TypeId || type == VolumeTypeBox || type == SceneryLeaf::TypeId || type == SceneryRoot::TypeId;
}

// How round a capsule is: its radius over half its length (the radius when it has no length)
f32 Roundness(const CapsuleVolume* capsule)
{
    if (capsule->halfLength == 0.0f)
    {
        return capsule->radius;
    }

    return capsule->radius / capsule->halfLength;
}

// A coordinate of a line's point taken onto the box along its axis, the squared distance it moved added
void ClampToBox(f32* point, const f32* extents, s32 axis, f32* squared)
{
    f32 extent = extents[axis];
    f32 value = point[axis];
    if (value < -extent)
    {
        f32 delta = value + extent;
        *squared = *squared + delta * delta;
        point[axis] = -extents[axis];
    }
    else if (extent < value)
    {
        f32 delta = value - extent;
        *squared = *squared + delta * delta;
        point[axis] = extents[axis];
    }
}

// The outcode's bits go above and below for each axis in turn
BoxOutCode OutCode(const Vector4* point, const Vector4* low, const Vector4* high)
{
    constexpr u32 Faces = 6;
    BoxOutCode code = {0};
    for (u32 face = 0; face < Faces; face++)
    {
        u32 axis = face >> 1;
        bool below = (face & 1) != 0;
        bool beyond = below ? Axes(point)[axis] < Axes(low)[axis] : Axes(high)[axis] < Axes(point)[axis];
        if (beyond)
        {
            code.value |= 1u << face;
        }
    }

    return code;
}

// Where a segment crosses the plane of an axis at a coordinate: the other two axes' coordinates there
void CrossingAt(const Vector4* segment, s32 axis, f32 plane, s32 first, s32 second, f32* atFirst, f32* atSecond)
{
    const f32* start = Axes(&segment[0]);
    const f32* end = Axes(&segment[1]);
    f32 share = (plane - start[axis]) / (end[axis] - start[axis]);
    *atFirst = start[first] + share * (end[first] - start[first]);
    *atSecond = start[second] + share * (end[second] - start[second]);
}

// A capsule's sphere made the middle of its segment, its radius the end's w (what the retail code does, the end copied over it)
void SphereAroundSegment(CapsuleVolume* capsule)
{
    capsule->sphere = capsule->end;
    capsule->sphere.x = (capsule->sphere.x + capsule->start.x) * 0.5f;
    capsule->sphere.y = (capsule->sphere.y + capsule->start.y) * 0.5f;
    capsule->sphere.z = (capsule->sphere.z + capsule->start.z) * 0.5f;
}
}

void Volume::Destroy(u32 flags)
{
    vtable = g_VolumeVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

Volume* Volume::Clone()
{
    return nullptr;
}

void Volume::Bounds(Vector4* min, Vector4* max)
{
    Vector4 low = {sphere.x - sphere.w, sphere.y - sphere.w, sphere.z - sphere.w, 1.0f};
    *min = low;
    Vector4 high = {sphere.x + sphere.w, sphere.y + sphere.w, sphere.z + sphere.w, 1.0f};
    *max = high;
}

void Volume::Read(Stream* stream)
{
    stream->Read(&sphere, sizeof(sphere), 1);
}

void Volume::None16()
{
}

void BoundingVolume::SetBox(const Vector4* low, const Vector4* high)
{
    if (high != nullptr && low != nullptr)
    {
        min = *low;
        max = *high;
    }

    halfSize = max;
    f32 middleX = (halfSize.x + min.x) * 0.5f;
    f32 middleY = (halfSize.y + min.y) * 0.5f;
    f32 middleZ = (halfSize.z + min.z) * 0.5f;
    sphere.x = middleX;
    sphere.y = middleY;
    sphere.z = middleZ;
    halfSize.x = middleX - min.x;
    halfSize.y = middleY - min.y;
    halfSize.z = middleZ - min.z;
    sphere.w = __builtin_sqrtf(halfSize.x * halfSize.x + halfSize.y * halfSize.y + halfSize.z * halfSize.z);
}

void BoundingVolume::Destroy(u32 flags)
{
    vtable = g_VolumeVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

BoundingVolume* BoundingVolume::Clone()
{
    auto* copy = static_cast<BoundingVolume*>(MemoryAllocate(sizeof(BoundingVolume)));
    copy->sphere = sphere;
    copy->vtable = g_BoxVolumeVTable;
    copy->min = min;
    copy->max = max;
    copy->halfSize = halfSize;
    return copy;
}

void BoundingVolume::Bounds(Vector4* low, Vector4* high)
{
    *low = min;
    *high = max;
}

u32 BoundingVolume::TestPoint(const Vector4* point)
{
    for (s32 axis = 0; axis < 3; axis++)
    {
        f32 value = Axes(point)[axis];
        if (Axes(&max)[axis] < value || value < Axes(&min)[axis])
        {
            return Apart;
        }
    }

    f32 x = point->x - sphere.x;
    f32 y = point->y - sphere.y;
    f32 z = point->z - sphere.z;
    if (-halfSize.x < x && -halfSize.y < y && -halfSize.z < z && x < halfSize.x && y < halfSize.y && z < halfSize.z)
    {
        return Inside;
    }

    return Partly;
}

u32 BoundingVolume::TestSphere(f32 radius, const Vector4* centre)
{
    return CallVirtual<u32>(this, vtable, SlotTestSphereInBox, radius, centre, &min, &max);
}

u32 BoundingVolume::TestSegment(const Vector4* segment, s32)
{
    return SegmentInBox(&min, &max, segment);
}

u32 BoundingVolume::SegmentCrossing(const Vector4* segment, s32 mode, f32* share)
{
    return SegmentCrossingBox(&min, &max, segment, mode, share);
}

u32 BoundingVolume::SegmentFirstInside(const Vector4* segment, s32 mode, f32* share)
{
    return SegmentFirstInsideBox(&min, &max, segment, mode, share);
}

u32 BoundingVolume::None9()
{
    return 0;
}

s32 BoundingVolume::TestVolume(Volume* other)
{
    if (!SpheresMeet(this, other))
    {
        return Apart;
    }

    u32 type = TypeOf(other);
    if (IsBoxType(type))
    {
        return TestBox(static_cast<BoundingVolume*>(other));
    }

    switch (type)
    {
    case VolumeTypeSphere:
        return TestSphereVolume(static_cast<SphereVolume*>(other));
    case VolumeTypeCapsule:
        return TestCapsule(static_cast<CapsuleVolume*>(other));
    case VolumeTypeBySphere:
        return CallVirtual<u32>(this, vtable, SlotTestSphere, other->sphere.w, &other->sphere);
    default:
        return Untested;
    }
}

Volume* BoundingVolume::TransformedCopy(const Matrix4x4*)
{
    return nullptr;
}

u32 BoundingVolume::TransformInto(const Matrix4x4* matrix, Volume* other)
{
    if (TypeOf(other) == VolumeTypeBySphere)
    {
        TransformIntoBySphere(matrix, other);
        return 1;
    }

    if (TypeOf(other) != VolumeTypeBox)
    {
        return 0;
    }

    auto* box = static_cast<BoundingVolume*>(other);
    AddMatrixPosition(matrix, &min, &box->min);
    AddMatrixPosition(matrix, &max, &box->max);
    box->halfSize = halfSize;
    AddMatrixPosition(matrix, &sphere, &box->sphere);
    box->sphere.w = sphere.w;
    return 1;
}

u32 BoundingVolume::Contact(Volume*, VolumeContact*)
{
    return 0;
}

u32 BoundingVolume::Type()
{
    return VolumeTypeBox;
}

void BoundingVolume::Read(Stream* stream)
{
    Volume::Read(stream);
    stream->Read(&min, sizeof(min), 1);
    stream->Read(&max, sizeof(max), 1);
    stream->Read(&halfSize, sizeof(halfSize), 1);
}

u32 BoundingVolume::TestSphereInBox(f32 radius, const Vector4* centre, const Vector4* low, const Vector4* high)
{
    f32 radiusSquared = radius * radius;
    f32 outside = 0.0f;
    for (s32 axis = 0; axis < 3; axis++)
    {
        f32 value = Axes(centre)[axis];
        f32 delta;
        if (value < Axes(low)[axis])
        {
            delta = value - Axes(low)[axis];
        }
        else if (Axes(high)[axis] < value)
        {
            delta = value - Axes(high)[axis];
        }
        else
        {
            continue;
        }

        outside = outside + delta * delta;
    }

    if (0.0f < outside)
    {
        return radiusSquared < outside ? Apart : Partly;
    }

    return radiusSquared < FaceDistanceSquared(centre, low, high) ? Inside : Partly;
}

u32 BoundingVolume::None18()
{
    return 0;
}

u32 BoundingVolume::None19()
{
    return 0;
}

// Retail bug: the crossing tests only return early what the end returns anyway, so a segment starting outside counts as crossing
// whether or not it misses the box, and one from inside the box out of it counts as apart
u32 BoundingVolume::SegmentInBox(const Vector4* low, const Vector4* high, const Vector4* segment)
{
    BoxOutCode start = OutCode(&segment[0], low, high);
    BoxOutCode end = OutCode(&segment[1], low, high);
    if ((start.value | end.value) == 0)
    {
        return Inside;
    }

    // Both ends beyond the same face
    if ((start.value & end.value) != 0)
    {
        return Apart;
    }

    u32 result = Apart;
    f32 first;
    f32 second;
    if ((start.value & BoxOutCode::BeyondX) != 0)
    {
        result = Partly;
        CrossingAt(segment, 0, start.aboveX ? high->x : low->x, 1, 2, &first, &second);
        if (low->y <= first && first <= high->y && low->z <= second && second <= high->z)
        {
            return Partly;
        }
    }

    if ((start.value & BoxOutCode::BeyondY) != 0)
    {
        result = Partly;
        CrossingAt(segment, 1, start.aboveY ? high->y : low->y, 0, 2, &first, &second);
        if (low->x <= first && first <= high->x && low->z <= second && second <= high->z)
        {
            return Partly;
        }
    }

    if ((start.value & BoxOutCode::BeyondZ) != 0)
    {
        result = Partly;
        CrossingAt(segment, 2, start.aboveZ ? high->z : low->z, 0, 1, &first, &second);
        if (low->x <= first && first <= high->x && low->y <= second && second <= high->y)
        {
            return Partly;
        }
    }

    return result;
}

u32 BoundingVolume::SegmentCrossingBox(const Vector4*, const Vector4*, const Vector4*, s32, f32*)
{
    return 0;
}

u32 BoundingVolume::SegmentFirstInsideBox(const Vector4*, const Vector4*, const Vector4*, s32, f32*)
{
    return 0;
}

f32 BoundingVolume::FaceDistanceSquared(const Vector4* point, const Vector4* low, const Vector4* high)
{
    f32 outside = 0.0f;
    f32 nearest = Infinite;
    for (s32 axis = 0; axis < 3; axis++)
    {
        f32 value = Axes(point)[axis];
        f32 toLow = value - Axes(low)[axis];
        f32 toHigh = value - Axes(high)[axis];
        f32 squared = __builtin_fminf(toHigh * toHigh, toLow * toLow);
        if (value < Axes(low)[axis] || Axes(high)[axis] < value)
        {
            nearest = 0.0f;
            outside = outside + squared;
        }
        else
        {
            nearest = __builtin_fminf(squared, nearest);
        }
    }

    return nearest + outside;
}

u32 BoundingVolume::TestBox(const BoundingVolume* other)
{
    if (other->max.x < min.x || max.x < other->min.x || other->max.y < min.y || max.y < other->min.y || other->max.z < min.z
        || max.z < other->min.z)
    {
        return Apart;
    }

    if (min.x < other->min.x && min.y < other->min.y && min.z < other->min.z && other->max.x < max.x && other->max.y < max.y
        && other->max.z < max.z)
    {
        return Inside;
    }

    return Partly;
}

u32 BoundingVolume::TestSphereVolume(const SphereVolume* other)
{
    Vector4 centre = other->sphere;
    return CallVirtual<u32>(this, vtable, SlotTestSphere, other->sphere.w, &centre);
}

u32 BoundingVolume::TestCapsule(const CapsuleVolume* other)
{
    f32 share;
    Vector4 nearest;
    f32 squared = SegmentBoxDistanceSquared(this, &other->start, &share, &nearest);
    if (0.0f < squared)
    {
        return other->radiusSquared < squared ? Apart : Partly;
    }

    f32 radius = other->radius;
    if (CallVirtual<u32>(this, vtable, SlotTestSphere, radius, &other->start) != Inside)
    {
        return Partly;
    }

    return CallVirtual<u32>(this, vtable, SlotTestSphere, radius, &other->end) == Inside ? Inside : Partly;
}

void BoundingVolume::TransformIntoBySphere(const Matrix4x4*, Volume*)
{
}

void BoxFacePlane(const BoundingVolume* box, const Vector4* point, Vector4* plane)
{
    Vector4 offset = *point;
    Vector4 face = g_DefaultBox.min;
    offset.x = offset.x - box->sphere.x;
    offset.y = offset.y - box->sphere.y;
    offset.z = offset.z - box->sphere.z;
    f32 beyond[3];
    beyond[0] = __builtin_fabsf(offset.x) - box->halfSize.x;
    beyond[1] = __builtin_fabsf(offset.y) - box->halfSize.y;
    beyond[2] = __builtin_fabsf(offset.z) - box->halfSize.z;
    f32 nearest = NoFace;
    for (s32 axis = 0; axis < 3; axis++)
    {
        if (0.0f < beyond[axis] && beyond[axis] < nearest)
        {
            nearest = beyond[axis];
        }
    }

    s32 faces = 0;
    for (s32 axis = 0; axis < 3; axis++)
    {
        if (beyond[axis] != nearest)
        {
            Axes(plane)[axis] = 0.0f;
            continue;
        }

        if (0.0f < Axes(&offset)[axis])
        {
            Axes(plane)[axis] = 1.0f;
            Axes(&face)[axis] = Axes(&box->max)[axis];
        }
        else
        {
            Axes(plane)[axis] = -1.0f;
            Axes(&face)[axis] = Axes(&box->min)[axis];
        }

        faces++;
    }

    if (faces >= 2)
    {
        f32 scale = InverseLength(plane, LengthEpsilon);
        plane->x = plane->x * scale;
        plane->y = plane->y * scale;
        plane->z = plane->z * scale;
    }

    plane->w = -(plane->x * face.x + plane->y * face.y + plane->z * face.z);
}

u32 VolumeTouchesBox(BoundingVolume* volume, const Box* box)
{
    if (box->max.x < volume->min.x || volume->max.x < box->min.x)
    {
        return 0;
    }

    if (box->max.y < volume->min.y || volume->max.y < box->min.y)
    {
        return 0;
    }

    if (box->max.z < volume->min.z || volume->max.z < box->min.z)
    {
        return 0;
    }

    return 1;
}

u32 VolumeHoldsCell(SceneryCell* cell, BoundingVolume* volume)
{
    const Box* box = reinterpret_cast<const Box*>(volume);
    if (box->max.x < cell->min.x || cell->max.x < box->min.x || box->max.y < cell->min.y || cell->max.y < box->min.y
        || box->max.z < cell->min.z || cell->max.z < box->min.z)
    {
        return Volume::Apart;
    }

    if (cell->min.x < box->min.x && cell->min.y < box->min.y && cell->min.z < box->min.z && box->max.x < cell->max.x
        && box->max.y < cell->max.y && box->max.z < cell->max.z)
    {
        return Volume::Inside;
    }

    return Volume::Partly;
}

// Retail bug: the span of a range it compares is OverlappingRangesSpan's, the union of the cell's and the box's ranges, so any
// box overlapping the cell counts as wholly held (Inside, never Partly for a box of a positive size) and only one apart on an
// axis is Apart
u32 CellHoldsBox(f32 margin, SceneryCell* cell, const Vector4* min, const Vector4* max)
{
    Vector4 size = *max;
    size.x = size.x - min->x;
    size.y = size.y - min->y;
    size.z = size.z - min->z;
    f32 spans[3];
    for (s32 axis = 0; axis < 3; axis++)
    {
        spans[axis] = OverlappingRangesSpan(Axes(&cell->min)[axis], Axes(&cell->max)[axis], Axes(min)[axis], Axes(max)[axis]);
        if (spans[axis] <= margin * Axes(&size)[axis])
        {
            return Volume::Apart;
        }
    }

    f32 share = 1.0f - margin;
    if (size.x * share <= spans[0] && size.y * share <= spans[1] && size.z * share <= spans[2])
    {
        return Volume::Inside;
    }

    return Volume::Partly;
}

void BoxCorners(const Box* box, Vector4* corners, const Matrix4x4* matrix)
{
    // The bottom's corners round from the lowest, then the top's
    for (s32 corner = 0; corner < 8; corner++)
    {
        s32 around = corner & 3;
        Vector4 point;
        point.x = around == 1 || around == 2 ? box->max.x : box->min.x;
        point.y = corner >= 4 ? box->max.y : box->min.y;
        point.z = around >= 2 ? box->max.z : box->min.z;
        point.w = 1.0f;
        VuTransformPoint(matrix, &point, &corners[corner]);
    }
}

void GrowBoxByPoint(Box* box, const Vector4* point)
{
    if (point->x < box->min.x)
    {
        box->min.x = point->x;
    }

    if (point->y < box->min.y)
    {
        box->min.y = point->y;
    }

    if (point->z < box->min.z)
    {
        box->min.z = point->z;
    }

    if (box->max.x < point->x)
    {
        box->max.x = point->x;
    }

    if (box->max.y < point->y)
    {
        box->max.y = point->y;
    }

    if (box->max.z < point->z)
    {
        box->max.z = point->z;
    }
}

f32 SegmentBoxDistanceSquared(const BoundingVolume* box, const Vector4* segment, f32* share, Vector4* nearest)
{
    Vector4 local[2] = {segment[0], segment[1]};
    for (Vector4& point : local)
    {
        point.x = point.x - box->sphere.x;
        point.y = point.y - box->sphere.y;
        point.z = point.z - box->sphere.z;
    }

    return LocalSegmentBoxDistanceSquared(box, local, share, nearest);
}

f32 LocalSegmentBoxDistanceSquared(const BoundingVolume* box, const Vector4* segment, f32* share, Vector4* nearest)
{
    f32 along;
    Vector4 point;
    f32 squared = LineBoxDistanceSquared(box, segment, &along, &point);
    if (0.0f <= along)
    {
        if (along <= 1.0f)
        {
            *share = along;
            *nearest = point;
            return squared;
        }

        squared = PointBoxDistanceSquared(box, &segment[1], nearest);
        *share = 1.0f;
        return squared;
    }

    squared = PointBoxDistanceSquared(box, &segment[0], nearest);
    *share = 0.0f;
    return squared;
}

f32 PointBoxDistanceSquared(const BoundingVolume* box, const Vector4* point, Vector4* nearest)
{
    *nearest = *point;
    f32 squared = 0.0f;
    for (s32 axis = 0; axis < 3; axis++)
    {
        f32 extent = Axes(&box->halfSize)[axis];
        f32 value = Axes(nearest)[axis];
        f32 delta;
        if (value < -extent)
        {
            delta = value + extent;
            Axes(nearest)[axis] = -extent;
        }
        else if (extent < value)
        {
            delta = value - extent;
            Axes(nearest)[axis] = extent;
        }
        else
        {
            continue;
        }

        squared = squared + delta * delta;
    }

    return squared;
}

f32 LineBoxDistanceSquared(const BoundingVolume* box, const Vector4* line, f32* share, Vector4* nearest)
{
    Vector4 direction = line[1];
    Vector4 point = line[0];
    direction.x = direction.x - point.x;
    direction.y = direction.y - point.y;
    direction.z = direction.z - point.z;
    bool reflected[3];
    for (s32 axis = 0; axis < 3; axis++)
    {
        if (Axes(&direction)[axis] < 0.0f)
        {
            Axes(&point)[axis] = -Axes(&point)[axis];
            Axes(&direction)[axis] = -Axes(&direction)[axis];
            reflected[axis] = true;
        }
        else
        {
            reflected[axis] = false;
        }
    }

    f32 squared = 0.0f;
    if (0.0f < direction.x)
    {
        if (0.0f < direction.y)
        {
            if (0.0f < direction.z)
            {
                LineBoxNoZeros(box, &point, &direction, share, &squared);
            }
            else
            {
                LineBoxOneZero(box, 0, 1, 2, &point, &direction, share, &squared);
            }
        }
        else if (0.0f < direction.z)
        {
            LineBoxOneZero(box, 0, 2, 1, &point, &direction, share, &squared);
        }
        else
        {
            LineBoxTwoZeros(box, 0, 1, 2, &point, &direction, share, &squared);
        }
    }
    else if (0.0f < direction.y)
    {
        if (0.0f < direction.z)
        {
            LineBoxOneZero(box, 1, 2, 0, &point, &direction, share, &squared);
        }
        else
        {
            LineBoxTwoZeros(box, 1, 0, 2, &point, &direction, share, &squared);
        }
    }
    else if (0.0f < direction.z)
    {
        LineBoxTwoZeros(box, 2, 0, 1, &point, &direction, share, &squared);
    }
    else
    {
        LineBoxThreeZeros(box, &point, &squared);
        *share = 0.0f;
    }

    for (s32 axis = 0; axis < 3; axis++)
    {
        if (reflected[axis])
        {
            Axes(&point)[axis] = -Axes(&point)[axis];
        }
    }

    *nearest = point;
    return squared;
}

void LineThroughBoxFace(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction,
                        const Vector4* pointLessExtents, f32* share, f32* squared)
{
    const f32* extents = Axes(&box->halfSize);
    f32* p = Axes(point);
    const f32* d = Axes(direction);
    const f32* pme = Axes(pointLessExtents);
    f32 ppe[3];
    ppe[i1] = p[i1] + extents[i1];
    ppe[i2] = p[i2] + extents[i2];
    if (d[i1] * pme[i0] <= d[i0] * ppe[i1])
    {
        if (d[i2] * pme[i0] <= d[i0] * ppe[i2])
        {
            // Through the face: no distance
            p[i0] = extents[i0];
            f32 inverse = 1.0f / d[i0];
            p[i1] = p[i1] - d[i1] * pme[i0] * inverse;
            p[i2] = p[i2] - d[i2] * pme[i0] * inverse;
            *share = -pme[i0] * inverse;
            return;
        }

        // Below the face on i2: nearest its edge there, or the corner past the edge's end
        f32 lengthSquared = d[i0] * d[i0] + d[i2] * d[i2];
        f32 along = lengthSquared * ppe[i1] - d[i1] * (d[i0] * pme[i0] + d[i2] * ppe[i2]);
        if (along <= (lengthSquared + lengthSquared) * extents[i1])
        {
            f32 edge = along / lengthSquared;
            lengthSquared = lengthSquared + d[i1] * d[i1];
            f32 offset = ppe[i1] - edge;
            f32 delta = d[i0] * pme[i0] + d[i1] * offset + d[i2] * ppe[i2];
            f32 parameter = -delta / lengthSquared;
            *squared = *squared + (pme[i0] * pme[i0] + offset * offset + ppe[i2] * ppe[i2] + delta * parameter);
            *share = parameter;
            p[i0] = extents[i0];
            p[i1] = edge - extents[i1];
            p[i2] = -extents[i2];
            return;
        }

        lengthSquared = lengthSquared + d[i1] * d[i1];
        f32 delta = d[i0] * pme[i0] + d[i1] * pme[i1] + d[i2] * ppe[i2];
        f32 parameter = -delta / lengthSquared;
        *squared = *squared + (pme[i0] * pme[i0] + pme[i1] * pme[i1] + ppe[i2] * ppe[i2] + delta * parameter);
        *share = parameter;
        p[i0] = extents[i0];
        p[i1] = extents[i1];
        p[i2] = -extents[i2];
        return;
    }

    if (d[i2] * pme[i0] <= d[i0] * ppe[i2])
    {
        // Below the face on i1: nearest its edge there, or the corner past the edge's end
        f32 lengthSquared = d[i0] * d[i0] + d[i1] * d[i1];
        f32 along = lengthSquared * ppe[i2] - d[i2] * (d[i0] * pme[i0] + d[i1] * ppe[i1]);
        if (along <= (lengthSquared + lengthSquared) * extents[i2])
        {
            f32 edge = along / lengthSquared;
            lengthSquared = lengthSquared + d[i2] * d[i2];
            f32 offset = ppe[i2] - edge;
            f32 delta = d[i0] * pme[i0] + d[i1] * ppe[i1] + d[i2] * offset;
            f32 parameter = -delta / lengthSquared;
            *squared = *squared + (pme[i0] * pme[i0] + ppe[i1] * ppe[i1] + offset * offset + delta * parameter);
            *share = parameter;
            p[i0] = extents[i0];
            p[i1] = -extents[i1];
            p[i2] = edge - extents[i2];
            return;
        }

        lengthSquared = lengthSquared + d[i2] * d[i2];
        f32 delta = d[i0] * pme[i0] + d[i1] * ppe[i1] + d[i2] * pme[i2];
        f32 parameter = -delta / lengthSquared;
        *squared = *squared + (pme[i0] * pme[i0] + ppe[i1] * ppe[i1] + pme[i2] * pme[i2] + delta * parameter);
        *share = parameter;
        p[i0] = extents[i0];
        p[i1] = -extents[i1];
        p[i2] = extents[i2];
        return;
    }

    // Below the face on both: nearest the edge on i1, the one on i2 or the corner between them
    f32 lengthSquared = d[i0] * d[i0] + d[i2] * d[i2];
    f32 along = lengthSquared * ppe[i1] - d[i1] * (d[i0] * pme[i0] + d[i2] * ppe[i2]);
    if (0.0f <= along)
    {
        if (along <= (lengthSquared + lengthSquared) * extents[i1])
        {
            f32 edge = along / lengthSquared;
            lengthSquared = lengthSquared + d[i1] * d[i1];
            f32 offset = ppe[i1] - edge;
            f32 delta = d[i0] * pme[i0] + d[i1] * offset + d[i2] * ppe[i2];
            f32 parameter = -delta / lengthSquared;
            *squared = *squared + (pme[i0] * pme[i0] + offset * offset + ppe[i2] * ppe[i2] + delta * parameter);
            *share = parameter;
            p[i0] = extents[i0];
            p[i1] = edge - extents[i1];
            p[i2] = -extents[i2];
            return;
        }

        lengthSquared = lengthSquared + d[i1] * d[i1];
        f32 delta = d[i0] * pme[i0] + d[i1] * pme[i1] + d[i2] * ppe[i2];
        f32 parameter = -delta / lengthSquared;
        *squared = *squared + (pme[i0] * pme[i0] + pme[i1] * pme[i1] + ppe[i2] * ppe[i2] + delta * parameter);
        *share = parameter;
        p[i0] = extents[i0];
        p[i1] = extents[i1];
        p[i2] = -extents[i2];
        return;
    }

    lengthSquared = d[i0] * d[i0] + d[i1] * d[i1];
    along = lengthSquared * ppe[i2] - d[i2] * (d[i0] * pme[i0] + d[i1] * ppe[i1]);
    if (0.0f <= along)
    {
        if (along <= (lengthSquared + lengthSquared) * extents[i2])
        {
            f32 edge = along / lengthSquared;
            lengthSquared = lengthSquared + d[i2] * d[i2];
            f32 offset = ppe[i2] - edge;
            f32 delta = d[i0] * pme[i0] + d[i1] * ppe[i1] + d[i2] * offset;
            f32 parameter = -delta / lengthSquared;
            *squared = *squared + (pme[i0] * pme[i0] + ppe[i1] * ppe[i1] + offset * offset + delta * parameter);
            *share = parameter;
            p[i0] = extents[i0];
            p[i1] = -extents[i1];
            p[i2] = edge - extents[i2];
            return;
        }

        lengthSquared = lengthSquared + d[i2] * d[i2];
        f32 delta = d[i0] * pme[i0] + d[i1] * ppe[i1] + d[i2] * pme[i2];
        f32 parameter = -delta / lengthSquared;
        *squared = *squared + (pme[i0] * pme[i0] + ppe[i1] * ppe[i1] + pme[i2] * pme[i2] + delta * parameter);
        *share = parameter;
        p[i0] = extents[i0];
        p[i1] = -extents[i1];
        p[i2] = extents[i2];
        return;
    }

    lengthSquared = lengthSquared + d[i2] * d[i2];
    f32 delta = d[i0] * pme[i0] + d[i1] * ppe[i1] + d[i2] * ppe[i2];
    f32 parameter = -delta / lengthSquared;
    *squared = *squared + (pme[i0] * pme[i0] + ppe[i1] * ppe[i1] + ppe[i2] * ppe[i2] + delta * parameter);
    *share = parameter;
    p[i0] = extents[i0];
    p[i1] = -extents[i1];
    p[i2] = -extents[i2];
}

void LineBoxNoZeros(const BoundingVolume* box, Vector4* point, const Vector4* direction, f32* share, f32* squared)
{
    Vector4 lessExtents = *point;
    lessExtents.x = lessExtents.x - box->halfSize.x;
    lessExtents.y = lessExtents.y - box->halfSize.y;
    lessExtents.z = lessExtents.z - box->halfSize.z;
    if (direction->x * lessExtents.y <= direction->y * lessExtents.x)
    {
        if (direction->x * lessExtents.z <= direction->z * lessExtents.x)
        {
            LineThroughBoxFace(box, 0, 1, 2, point, direction, &lessExtents, share, squared);
        }
        else
        {
            LineThroughBoxFace(box, 2, 0, 1, point, direction, &lessExtents, share, squared);
        }
    }
    else if (direction->y * lessExtents.z <= direction->z * lessExtents.y)
    {
        LineThroughBoxFace(box, 1, 2, 0, point, direction, &lessExtents, share, squared);
    }
    else
    {
        LineThroughBoxFace(box, 2, 0, 1, point, direction, &lessExtents, share, squared);
    }
}

void LineBoxOneZero(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction, f32* share,
                    f32* squared)
{
    const f32* extents = Axes(&box->halfSize);
    f32* p = Axes(point);
    const f32* d = Axes(direction);
    f32 lessExtent0 = p[i0] - extents[i0];
    f32 lessExtent1 = p[i1] - extents[i1];
    f32 product0 = d[i1] * lessExtent0;
    f32 product1 = d[i0] * lessExtent1;
    if (product1 <= product0)
    {
        // Through the plane of i0's face
        p[i0] = extents[i0];
        f32 plusExtent1 = p[i1] + extents[i1];
        f32 delta = product0 - d[i0] * plusExtent1;
        if (0.0f <= delta)
        {
            f32 inverseLengthSquared = 1.0f / (d[i0] * d[i0] + d[i1] * d[i1]);
            *squared = *squared + delta * delta * inverseLengthSquared;
            p[i1] = -extents[i1];
            *share = -(d[i0] * lessExtent0 + d[i1] * plusExtent1) * inverseLengthSquared;
        }
        else
        {
            f32 inverse = 1.0f / d[i0];
            p[i1] = p[i1] - product0 * inverse;
            *share = -lessExtent0 * inverse;
        }
    }
    else
    {
        // Through the plane of i1's face
        p[i1] = extents[i1];
        f32 plusExtent0 = p[i0] + extents[i0];
        f32 delta = product1 - d[i1] * plusExtent0;
        if (0.0f <= delta)
        {
            f32 inverseLengthSquared = 1.0f / (d[i0] * d[i0] + d[i1] * d[i1]);
            *squared = *squared + delta * delta * inverseLengthSquared;
            p[i0] = -extents[i0];
            *share = -(d[i0] * plusExtent0 + d[i1] * lessExtent1) * inverseLengthSquared;
        }
        else
        {
            f32 inverse = 1.0f / d[i1];
            p[i0] = p[i0] - product1 * inverse;
            *share = -lessExtent1 * inverse;
        }
    }

    ClampToBox(p, extents, i2, squared);
}

void LineBoxTwoZeros(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction, f32* share,
                     f32* squared)
{
    const f32* extents = Axes(&box->halfSize);
    f32* p = Axes(point);
    *share = (extents[i0] - p[i0]) / Axes(direction)[i0];
    p[i0] = extents[i0];
    ClampToBox(p, extents, i1, squared);
    f32 value = p[i2];
    if (value < -extents[i2])
    {
        f32 delta = value + extents[i2];
        *squared = *squared + delta * delta;
        // Retail bug: i1's coordinate is given i2's lowest, i2's isn't moved: the nearest point is wrong, the distance right
        p[i1] = -extents[i2];
    }
    else if (extents[i2] < value)
    {
        f32 delta = value - extents[i2];
        *squared = *squared + delta * delta;
        p[i2] = extents[i2];
    }
}

void LineBoxThreeZeros(const BoundingVolume* box, Vector4* point, f32* squared)
{
    const f32* extents = Axes(&box->halfSize);
    ClampToBox(Axes(point), extents, 0, squared);
    ClampToBox(Axes(point), extents, 1, squared);
    ClampToBox(Axes(point), extents, 2, squared);
}

SphereVolume* SphereVolume::Construct(SphereVolume* volume, const Vector4* centre, f32 radius)
{
    volume->vtable = g_SphereVolumeVTable;
    volume->sphere = *centre;
    volume->radiusSquared = radius * radius;
    volume->sphere.w = radius;
    return volume;
}

void SphereVolume::Destroy(u32 flags)
{
    vtable = g_VolumeVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

SphereVolume* SphereVolume::Clone()
{
    auto* copy = static_cast<SphereVolume*>(MemoryAllocate(sizeof(SphereVolume)));
    copy->radiusSquared = radiusSquared;
    copy->sphere = sphere;
    copy->vtable = g_SphereVolumeVTable;
    return copy;
}

u32 SphereVolume::TestPoint(const Vector4* point)
{
    f32 x = point->x - sphere.x;
    f32 y = point->y - sphere.y;
    f32 z = point->z - sphere.z;
    f32 beyond = x * x + y * y + z * z - radiusSquared;
    if (beyond == 0.0f)
    {
        return Partly;
    }

    return 0.0f < beyond ? Apart : Inside;
}

u32 SphereVolume::TestSphere(f32 radius, const Vector4* centre)
{
    f32 x = centre->x - sphere.x;
    f32 y = centre->y - sphere.y;
    f32 z = centre->z - sphere.z;
    f32 squared = x * x + y * y + z * z;
    f32 reach = sphere.w + radius;
    if (reach * reach < squared)
    {
        return Apart;
    }

    f32 inner = sphere.w - radius;
    if (0.0f < inner && squared < inner * inner)
    {
        return Inside;
    }

    return Partly;
}

u32 SphereVolume::TestSegment(const Vector4* segment, s32 mode)
{
    f32 lengthSquared;
    if (LineAtLeast(segment, &lengthSquared, radiusSquared, ShortSegment) == 0)
    {
        return CallVirtual<u32>(this, vtable, SlotTestPoint, segment);
    }

    // How far along the segment the centre is (times its length), the centre's squared distance to the start and to the line
    // (times the length squared)
    f32 toCentre = LineDotDifference(segment, &sphere);
    f32 along = toCentre - LineDotDifference(segment, segment);
    f32 x = sphere.x - segment[0].x;
    f32 y = sphere.y - segment[0].y;
    f32 z = sphere.z - segment[0].z;
    f32 startSquared = x * x + y * y + z * z;
    f32 missSquared = PositivePart(startSquared * lengthSquared - along * along, 0.0f);
    u32 result = Partly;
    if (radiusSquared * lengthSquared < missSquared)
    {
        result = Apart;
    }

    if (mode == SegmentLine || result != Partly)
    {
        return result;
    }

    if (along < 0.0f)
    {
        return radiusSquared < startSquared ? Apart : Partly;
    }

    if (mode < SegmentBounded)
    {
        return Partly;
    }

    f32 endX = segment[1].x - sphere.x;
    f32 endY = segment[1].y - sphere.y;
    f32 endZ = segment[1].z - sphere.z;
    f32 endSquared = endX * endX + endY * endY + endZ * endZ;
    if (lengthSquared < along && radiusSquared < endSquared)
    {
        return Apart;
    }

    if (!(endSquared < radiusSquared))
    {
        return Partly;
    }

    return startSquared < radiusSquared ? Inside : Partly;
}

u32 SphereVolume::SegmentCrossing(const Vector4* segment, s32 mode, f32* share)
{
    u32 result = CallVirtual<u32>(this, vtable, SlotTestSegment, segment, mode);
    if (result != Partly)
    {
        *share = 0.0f;
        return result;
    }

    // Where the line through the segment meets the sphere: a t squared + b t + c = 0
    f32 x = segment[0].x - segment[1].x;
    f32 y = segment[0].y - segment[1].y;
    f32 z = segment[0].z - segment[1].z;
    f32 lengthSquared = x * x + y * y + z * z;
    f32 fromStart = LineDotDifference(segment, segment);
    f32 along = fromStart - LineDotDifference(segment, &sphere);
    f32 centreX = sphere.x - segment[0].x;
    f32 centreY = sphere.y - segment[0].y;
    f32 centreZ = sphere.z - segment[0].z;
    f32 roots[2];
    s32 count = SolveQuadratic(roots, lengthSquared, along + along,
                               centreX * centreX + centreY * centreY + centreZ * centreZ - radiusSquared);
    if (count > 0)
    {
        if (mode <= SegmentLine)
        {
            *share = roots[0];
            if (count >= 2)
            {
                *share = __builtin_fminf(__builtin_fabsf(roots[1]), __builtin_fabsf(roots[0]));
            }

            return result;
        }

        // A ray's roots before its start (by more than the tolerance) and a segment's past its end aren't crossings, the others
        // are kept within them
        f32 shares[2];
        s32 kept = 0;
        for (s32 index = 0; index < count; index++)
        {
            f32 root = roots[index];
            if (root < -RootTolerance)
            {
                continue;
            }

            if (root < 0.0f)
            {
                shares[kept++] = 0.0f;
                continue;
            }

            if (mode >= SegmentBounded)
            {
                if (LastRoot < root)
                {
                    continue;
                }

                if (1.0f < root)
                {
                    root = 1.0f;
                }
            }

            shares[kept++] = root;
        }

        if (kept > 0)
        {
            *share = shares[0];
            if (kept >= 2)
            {
                *share = __builtin_fminf(shares[1], shares[0]);
            }

            return result;
        }
    }

    // No crossing kept: wholly inside when both ends are
    f32 startX = segment[0].x - sphere.x;
    f32 startY = segment[0].y - sphere.y;
    f32 startZ = segment[0].z - sphere.z;
    result = Apart;
    if (startX * startX + startY * startY + startZ * startZ < radiusSquared)
    {
        f32 endX = segment[1].x - sphere.x;
        f32 endY = segment[1].y - sphere.y;
        f32 endZ = segment[1].z - sphere.z;
        if (endX * endX + endY * endY + endZ * endZ < radiusSquared)
        {
            result = Inside;
        }
    }

    *share = 0.0f;
    return result;
}

u32 SphereVolume::SegmentFirstInside(const Vector4* segment, s32 mode, f32* share)
{
    u32 result = CallVirtual<u32>(this, vtable, SlotSegmentCrossing, segment, mode, share);
    if (result == Apart)
    {
        return Apart;
    }

    f32 x = segment[0].x - sphere.x;
    f32 y = segment[0].y - sphere.y;
    f32 z = segment[0].z - sphere.z;
    if (x * x + y * y + z * z <= radiusSquared)
    {
        *share = 0.0f;
    }

    return result;
}

u32 SphereVolume::None9()
{
    return 0;
}

s32 SphereVolume::TestVolume(Volume* other)
{
    if (!SpheresMeet(this, other))
    {
        return Apart;
    }

    u32 type = TypeOf(other);
    if (IsBoxType(type))
    {
        Vector4 centre = sphere;
        return CallVirtual<u32>(other, other->vtable, SlotTestSphere, sphere.w, &centre);
    }

    if (type == VolumeTypeSphere)
    {
        return TestSphereVolume(static_cast<SphereVolume*>(other));
    }

    return Untested;
}

// Retail bug: the copy's radius is the moved centre's w (1 for an affine matrix), its radius squared the right one
SphereVolume* SphereVolume::TransformedCopy(const Matrix4x4* matrix)
{
    auto* copy = static_cast<SphereVolume*>(MemoryAllocate(sizeof(SphereVolume)));
    f32 radius = sphere.w;
    copy->vtable = g_SphereVolumeVTable;
    copy->sphere.w = radius;
    copy->radiusSquared = radius * radius;
    copy->sphere.x = 0.0f;
    copy->sphere.y = 0.0f;
    copy->sphere.z = 0.0f;
    VuTransformPoint(matrix, &sphere, &copy->sphere);
    return copy;
}

u32 SphereVolume::TransformInto(const Matrix4x4* matrix, Volume* other)
{
    u32 otherType = TypeOf(other);
    if (otherType != TypeOf(this))
    {
        return 0;
    }

    auto* moved = static_cast<SphereVolume*>(other);
    moved->sphere.w = sphere.w;
    moved->radiusSquared = radiusSquared;
    moved->sphere = sphere;
    moved->sphere.x = moved->sphere.x + matrix->m[3][0];
    moved->sphere.y = moved->sphere.y + matrix->m[3][1];
    moved->sphere.z = moved->sphere.z + matrix->m[3][2];
    return 1;
}

u32 SphereVolume::Contact(Volume* other, VolumeContact* contact)
{
    u32 type = TypeOf(other);
    if (type == VolumeTypeBox)
    {
        u32 touches = CallVirtual<s32>(other, other->vtable, SlotTestVolume, this) != Apart;
        if (touches)
        {
            contact->kind = VolumeContact::BoxFace;
            BoxFacePlane(static_cast<BoundingVolume*>(other), &sphere, &contact->normal);
        }

        return touches;
    }

    if (type != VolumeTypeSphere)
    {
        return 0;
    }

    f32 x = sphere.x - other->sphere.x;
    f32 y = sphere.y - other->sphere.y;
    f32 z = sphere.z - other->sphere.z;
    f32 reach = sphere.w + other->sphere.w;
    u32 touches = x * x + y * y + z * z <= reach * reach;
    if (touches)
    {
        contact->kind = VolumeContact::BetweenSpheres;
        contact->normal = {x, y, z, 1.0f};
    }

    return touches;
}

u32 SphereVolume::Type()
{
    return VolumeTypeSphere;
}

u32 SphereVolume::TestSphereVolume(const SphereVolume* other)
{
    f32 x = sphere.x - other->sphere.x;
    f32 y = sphere.y - other->sphere.y;
    f32 z = sphere.z - other->sphere.z;
    f32 distance = __builtin_sqrtf(x * x + y * y + z * z);
    if (sphere.w + other->sphere.w < distance)
    {
        return Apart;
    }

    return distance + other->sphere.w < sphere.w ? Inside : Partly;
}

// Retail bug: the sphere is the moved end's copy, so its radius is the end's w (1 for an affine matrix)
CapsuleVolume* CapsuleVolume::Construct(CapsuleVolume* capsule, f32 boundingRadius, const Matrix4x4* matrix, const Vector4* start,
                                        const Vector4* end)
{
    capsule->sphere.x = 0.0f;
    capsule->sphere.y = 0.0f;
    capsule->sphere.z = 0.0f;
    capsule->sphere.w = boundingRadius;
    capsule->vtable = g_CapsuleVolumeVTable;
    Vector4 point;
    VuTransformPoint(matrix, start, &point);
    capsule->start = point;
    VuTransformPoint(matrix, end, &point);
    capsule->end = point;
    SphereAroundSegment(capsule);
    return capsule;
}

CapsuleVolume* CapsuleVolume::ConstructCopy(CapsuleVolume* capsule, const CapsuleVolume* other)
{
    capsule->vtable = g_CapsuleVolumeVTable;
    capsule->sphere = other->sphere;
    capsule->radius = other->radius;
    capsule->radiusSquared = other->radiusSquared;
    capsule->halfLength = other->halfLength;
    capsule->start = other->start;
    capsule->end = other->end;
    return capsule;
}

void CapsuleVolume::Destroy(u32 flags)
{
    vtable = g_VolumeVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

CapsuleVolume* CapsuleVolume::Clone()
{
    return ConstructCopy(static_cast<CapsuleVolume*>(MemoryAllocate(sizeof(CapsuleVolume))), this);
}

u32 CapsuleVolume::TestPoint(const Vector4* point)
{
    f32 x = sphere.x - point->x;
    f32 y = sphere.y - point->y;
    f32 z = sphere.z - point->z;
    if (sphere.w * sphere.w < x * x + y * y + z * z)
    {
        return Apart;
    }

    f32 squared = PointSegmentDistanceSquared(&start, point);
    if (radiusSquared < squared)
    {
        return Apart;
    }

    return squared < radiusSquared ? Inside : Partly;
}

u32 CapsuleVolume::TestSphere(f32 sphereRadius, const Vector4* centre)
{
    f32 x = sphere.x - centre->x;
    f32 y = sphere.y - centre->y;
    f32 z = sphere.z - centre->z;
    f32 reach = sphereRadius + radius;
    f32 bound = halfLength + reach;
    if (bound * bound < x * x + y * y + z * z)
    {
        return Apart;
    }

    f32 squared = PointSegmentDistanceSquared(&start, centre);
    if (reach * reach < squared)
    {
        return Apart;
    }

    return __builtin_sqrtf(squared) + sphereRadius < radius ? Inside : Partly;
}

u32 CapsuleVolume::TestSegment(const Vector4*, s32)
{
    return 0;
}

u32 CapsuleVolume::SegmentCrossing(const Vector4*, s32, f32*)
{
    return 0;
}

u32 CapsuleVolume::SegmentFirstInside(const Vector4*, s32, f32*)
{
    return 0;
}

u32 CapsuleVolume::None9()
{
    return 0;
}

s32 CapsuleVolume::TestVolume(Volume* other)
{
    if (!SpheresMeet(this, other))
    {
        return Apart;
    }

    u32 type = TypeOf(other);
    if (IsBoxType(type))
    {
        return TestBox(static_cast<BoundingVolume*>(other));
    }

    switch (type)
    {
    case VolumeTypeSphere:
        return TestSphereVolume(static_cast<SphereVolume*>(other));
    case VolumeTypeCapsule:
        return TestCapsule(static_cast<CapsuleVolume*>(other));
    case VolumeTypeBySphere:
    {
        SphereVolume around;
        SphereVolume::Construct(&around, &other->sphere, other->sphere.w);
        u32 result = TestSphereVolume(&around);
        around.Destroy(DestroyOnly);
        return result;
    }
    default:
        return Untested;
    }
}

// Retail bug: the copy's radius, radius squared and half length are what the allocator left (its constructor doesn't take them)
CapsuleVolume* CapsuleVolume::TransformedCopy(const Matrix4x4* matrix)
{
    auto* copy = static_cast<CapsuleVolume*>(MemoryAllocate(sizeof(CapsuleVolume)));
    return Construct(copy, sphere.w, matrix, &start, &end);
}

// Retail bug: the sphere is the moved end's copy, so its radius is the end's w (1 for an affine matrix)
u32 CapsuleVolume::TransformInto(const Matrix4x4* matrix, Volume* other)
{
    u32 otherType = TypeOf(other);
    if (otherType != TypeOf(this))
    {
        return 0;
    }

    auto* capsule = static_cast<CapsuleVolume*>(other);
    capsule->radius = radius;
    capsule->radiusSquared = radiusSquared;
    capsule->halfLength = halfLength;
    Vector4 point;
    VuTransformPoint(matrix, &start, &point);
    capsule->start = point;
    VuTransformPoint(matrix, &end, &point);
    capsule->end = point;
    SphereAroundSegment(capsule);
    return 1;
}

u32 CapsuleVolume::Contact(Volume* other, VolumeContact* contact)
{
    u32 type = TypeOf(other);
    if (type == VolumeTypeSphere)
    {
        return TouchesSphere(&other->sphere, other->sphere.w);
    }

    if (type == VolumeTypeCapsule)
    {
        return TouchesCapsule(static_cast<CapsuleVolume*>(other));
    }

    if (!IsBoxType(type))
    {
        return 1;
    }

    // Spheres of its radius stepped along the segment a radius at a time from the start, the last at the end
    Vector4 at = start;
    Vector4 step = end;
    step.x = step.x - at.x;
    step.y = step.y - at.y;
    step.z = step.z - at.z;
    f32 x = start.x - end.x;
    f32 y = start.y - end.y;
    f32 z = start.z - end.z;
    f32 steps = __builtin_sqrtf(x * x + y * y + z * z) / radius;
    f32 inverse = 1.0f / steps;
    step.z = step.z * inverse;
    step.x = step.x * inverse;
    step.y = step.y * inverse;
    if (!(0.0f <= steps))
    {
        return 0;
    }

    do
    {
        SphereVolume probe;
        SphereVolume::Construct(&probe, &at, radius);
        if (CallVirtual<s32>(other, other->vtable, SlotTestVolume, &probe) != Apart)
        {
            contact->kind = VolumeContact::BoxFace;
            BoxFacePlane(static_cast<BoundingVolume*>(other), &at, &contact->normal);
            probe.Destroy(DestroyOnly);
            return 1;
        }

        steps = steps - 1.0f;
        at.x = at.x + step.x;
        at.y = at.y + step.y;
        at.z = at.z + step.z;
        if (steps < 0.0f && steps != -1.0f)
        {
            at = end;
            steps = 0.0f;
        }

        probe.Destroy(DestroyOnly);
    } while (0.0f <= steps);

    return 0;
}

u32 CapsuleVolume::Type()
{
    return VolumeTypeCapsule;
}

u32 CapsuleVolume::TestCapsule(const CapsuleVolume* other)
{
    f32 x = sphere.x - other->sphere.x;
    f32 y = sphere.y - other->sphere.y;
    f32 z = sphere.z - other->sphere.z;
    f32 reach = sphere.w + other->sphere.w;
    if (reach * reach < x * x + y * y + z * z)
    {
        return Apart;
    }

    u32 touches;
    bool same = radius == other->radius && halfLength == other->halfLength;
    if (!same && Roundness(other) < Roundness(this))
    {
        touches = CallVirtual<u32>(this, vtable, SlotTestSphere, other->sphere.w, &other->sphere);
    }
    else
    {
        touches = CallVirtual<u32>(other, other->vtable, SlotTestSphere, sphere.w, &sphere);
    }

    if (touches == Apart)
    {
        return Apart;
    }

    f32 radii = radius + other->radius;
    f32 share;
    f32 otherShare;
    f32 squared = SegmentsDistanceSquared(&start, &other->start, &share, &otherShare);
    if (radii * radii < squared)
    {
        return Apart;
    }

    if (!(squared < radii * radii))
    {
        return Partly;
    }

    f32 toStart = PointSegmentDistanceSquared(&start, &other->start);
    f32 toEnd = PointSegmentDistanceSquared(&start, &other->end);
    return __builtin_fmaxf(toEnd, toStart) < radiusSquared ? Inside : Partly;
}

u32 CapsuleVolume::TouchesCapsule(const CapsuleVolume* other)
{
    f32 x = sphere.x - other->sphere.x;
    f32 y = sphere.y - other->sphere.y;
    f32 z = sphere.z - other->sphere.z;
    f32 reach = sphere.w + other->sphere.w;
    if (reach * reach < x * x + y * y + z * z)
    {
        return 0;
    }

    u32 touches;
    bool same = radius == other->radius && halfLength == other->halfLength;
    if (same || Roundness(other) < Roundness(this))
    {
        touches = other->TouchesSphere(&sphere, radius);
    }
    else
    {
        touches = TouchesSphere(&other->sphere, other->radius);
    }

    if (touches == 0)
    {
        return 0;
    }

    f32 radii = radius + other->radius;
    f32 share;
    f32 otherShare;
    return SegmentsDistanceSquared(&start, &other->start, &share, &otherShare) <= radii * radii;
}

u32 CapsuleVolume::TouchesSphere(const Vector4* centre, f32 sphereRadius) const
{
    f32 x = sphere.x - centre->x;
    f32 y = sphere.y - centre->y;
    f32 z = sphere.z - centre->z;
    f32 reach = sphereRadius + radius;
    f32 bound = halfLength + reach;
    if (bound * bound < x * x + y * y + z * z)
    {
        return 0;
    }

    return PointSegmentDistanceSquared(&start, centre) <= reach * reach;
}

u32 CapsuleVolume::TestSphereVolume(const SphereVolume* other)
{
    return CallVirtual<u32>(this, vtable, SlotTestSphere, other->sphere.w, &other->sphere);
}

u32 CapsuleVolume::TestBox(const BoundingVolume*)
{
    return Partly;
}

u32 HullRayCast(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* start, const Vector4* end, f32* share,
                void* hit)
{
    const u8* blob = hull->blob;
    const Vector4* planes = reinterpret_cast<const Vector4*>(blob + hull->planesOffset);
    Matrix4x4 inverse = *matrix;
    VuInvertRigidInPlace(&inverse);
    Vector4 from;
    Vector4 to;
    VuTransformPoint(&inverse, start, &from);
    VuTransformPoint(&inverse, end, &to);
    bool inside = true;
    for (s32 index = 0; index < hull->planeCount; index++)
    {
        const Vector4* plane = &planes[index];
        f32 fromSide = plane->x * from.x + plane->y * from.y + plane->z * from.z + plane->w;
        f32 toSide = plane->x * to.x + plane->y * to.y + plane->z * to.z + plane->w;
        if (PlaneTolerance < fromSide && PlaneTolerance < toSide)
        {
            return 0;
        }

        if (-PlaneTolerance < fromSide)
        {
            inside = false;
        }
    }

    if (inside)
    {
        return 0;
    }

    // The segment clipped by every plane, the face it gets in through the last one it was in front of
    s32 entryFace = -1;
    f32 enter = 0.0f;
    f32 leave = 1.0f;
    for (s32 index = 0; index < hull->planeCount; index++)
    {
        const Vector4* plane = &planes[index];
        bool fromOutside = PlaneTolerance < plane->x * from.x + plane->y * from.y + plane->z * from.z + plane->w;
        bool toOutside = PlaneTolerance < plane->x * to.x + plane->y * to.y + plane->z * to.z + plane->w;
        Vector4 crossing;
        if (fromOutside)
        {
            if (toOutside)
            {
                return 0;
            }

            f32 along = RayPlaneIntersection(plane, &from, &to, &crossing);
            enter = enter + (leave - enter) * along;
            entryFace = index;
            from = crossing;
        }
        else if (toOutside)
        {
            f32 along = RayPlaneIntersection(plane, &from, &to, &crossing);
            leave = enter + (leave - enter) * along;
            to = crossing;
        }
    }

    if (entryFace == -1)
    {
        return 0;
    }

    *share = enter;
    if (hit != nullptr)
    {
        const u8* face = blob + hull->facesOffset + blob[hull->faceOffsetsOffset + entryFace];
        const Vector4* vertices = reinterpret_cast<const Vector4*>(blob);
        Vector4 corners[3];
        VuTransformPoint(matrix, &vertices[face[1]], &corners[0]);
        VuTransformPoint(matrix, &vertices[face[2]], &corners[1]);
        VuTransformPoint(matrix, &vertices[face[3]], &corners[2]);
        auto* record = static_cast<CollisionHit*>(hit);
        MakeCollisionHit(record, &corners[0], &corners[1], &corners[2]);
        record->surface = hull->surface;
    }

    return 1;
}

void ListIterator::BaseDestroy(u32 flags)
{
    vtable = g_ListIteratorVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void WantPolicy::BaseDestroy(u32 flags)
{
    vtable = g_ChunkLoadingUtilBaseVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Sm2Reader::FilePath(u32 kind, String* file)
{
    StringAssign(file, path.string);
    switch (kind)
    {
    case FileSn:
        StringAppend(file, g_SnExtension);
        break;
    case FileLvl:
        StringAppend(file, g_LvlExtension);
        break;
    case FileLgt:
        StringAppend(file, g_LgtExtension);
        break;
    case FileSca:
        StringAppend(file, g_ScaExtension);
        break;
    case FileLk:
        StringAppend(file, g_LkExtension);
        break;
    default:
        break;
    }
}

void InitChunkViews(s32, s32)
{
}

void ChunkViewsStaticInit()
{
    InitChunkViews(1, DefaultInitPriority);
}
