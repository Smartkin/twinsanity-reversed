#include "game/collision.h"

#include "game/agentlab.h"
#include "game/chunkdata.h"
#include "game/disk.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/readers.h"
#include "platform/math.h"
#include "retail/libc.h"

EABI_EXPORT(FUN_00200088, GrowBox);
EABI_EXPORT(FUN_002801d8, SphereTouchesTriangle);
EABI_EXPORT(FUN_00280ab0, EllipsoidTouchesTriangle);
EABI_EXPORT(FUN_001f9118, SphereBoxPush);
EABI_EXPORT(FUN_0027eb40, AccelerateOnSurface);

namespace
{
constexpr f32 NoHit = Rounded(1e30);
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr s32 MostLeaves = 0x100;
// The leaves' groups go after the scripts' state jump table's first 256 entries
u32* const CollisionLeaves = reinterpret_cast<u32*>(&g_StateJumpTable[256]);

void AddLeaf(s32 node)
{
    if (g_CollisionLeafCount < MostLeaves)
    {
        CollisionLeaves[g_CollisionLeafCount] = ~static_cast<u32>(node);
        g_CollisionLeafCount++;
    }
}

// The 0x34 bytes the game copies of a hit (the struct is padded to 0x40)
void CopyHit(CollisionHit* to, const CollisionHit* from)
{
    to->vertices[0] = from->vertices[0];
    to->vertices[1] = from->vertices[1];
    to->vertices[2] = from->vertices[2];
    to->surface = from->surface;
    to->unknown32 = from->unknown32;
}

u32 VertexIndex(u64 packed, u32 shift)
{
    return static_cast<s32>(static_cast<u32>(packed >> shift & CollisionTriangle::VertexMask));
}
}

CollisionData* ConstructCollisionData(CollisionData* data)
{
    constexpr u32 Version = 3001;
    data->version = Version;
    data->verticesHandle = -1;
    data->nodesHandle = -1;
    data->groupsHandle = -1;
    data->trianglesHandle = -1;
    data->queued = 0;
    data->nodeCount = 0;
    data->groupCount = 0;
    data->triangleCount = 0;
    data->vertexCount = 0;
    return data;
}

void DestroyCollisionData(CollisionData* data, u32 destroyFlags)
{
    s32* handles[] = {&data->nodesHandle, &data->groupsHandle, &data->trianglesHandle, &data->verticesHandle};
    for (s32* handle : handles)
    {
        if (*handle >= 0)
        {
            DiskRelease(GetDiskManager(), handle);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(data);
    }
}

void ReadCollisionData(CollisionData* data, u32 flags, s32 offset, const u8* header)
{
    constexpr u32 HeaderSize = 0x14;
    constexpr u32 OnDisk = 1;
    constexpr u32 VerticesFlags = 5;
    GameReadersStorage* storage = g_ReadersStorages[0];
    RetailLibc::MemoryCopy(data, header, HeaderSize);
    u32 verticesFlags = flags == 0 ? OnDisk : VerticesFlags;
    s32 at = offset + HeaderSize;
    struct Array
    {
        s32* handle;
        u32 flags;
        u32 size;
    };

    const Array arrays[] = {
        {&data->nodesHandle, OnDisk, data->nodeCount * static_cast<u32>(sizeof(CollisionNode))},
        {&data->groupsHandle, OnDisk, data->groupCount * static_cast<u32>(sizeof(CollisionGroup))},
        {&data->trianglesHandle, OnDisk, data->triangleCount * static_cast<u32>(sizeof(CollisionTriangle))},
        {&data->verticesHandle, verticesFlags, data->vertexCount * static_cast<u32>(sizeof(Vector4))},
    };
    for (const Array& array : arrays)
    {
        auto* reader = static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader)));
        reader = SubItemsReader::ConstructOnDisk(reader, array.handle, array.flags, at, array.size);
        at += array.size;
        AddItemReaderToReaderStorage(storage, reader, 0);
    }

    data->queued = 1;
}

void CollisionSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CollisionSectionReader::Read(u8* header, u32, ReaderStack*)
{
    ReadCollisionData(data, flags, offset, header);
}

void QueueCollisionSection(CollisionData** holder, s32 offset)
{
    constexpr u32 OnDisk = 8;
    constexpr u32 HeaderSize = 0x14;
    GameReadersStorage* storage = g_ReadersStorages[0];
    auto* section = static_cast<CollisionSectionReader*>(MemoryAllocate(sizeof(CollisionSectionReader)));
    section->vtable = g_CollisionSectionReaderVTable;
    section->data = *holder;
    section->offset = offset;
    section->flags = 0;
    auto* reader = static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader)));
    reader = SubItemsReader::ConstructOpen(reader, section, OnDisk, offset, HeaderSize);
    AddItemReaderToReaderStorage(storage, reader, 0);
}

CollisionData** ConstructCollisionHolder(CollisionData** holder, CollisionData* data)
{
    *holder = data;
    return holder;
}

void* ConstructCollisionScratch(void* scratch)
{
    return scratch;
}

void DestroyCollisionScratch(void* scratch, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(scratch);
    }
}

void QueryCollisionBox(CollisionData* data, const Box* box)
{
    g_CollisionLeafCount = 0;
    auto* nodes = reinterpret_cast<const CollisionNode*>(DiskLoadedMemory(GetDiskManager(), &data->nodesHandle));
    s32 first = nodes[0].first;
    s32 second = nodes[0].second;
    if (first < 0)
    {
        AddLeaf(first);
        return;
    }

    if (BoxesOverlap(box, nodes[first].Bounds()) != 0)
    {
        QueryCollisionNode(data, first, nodes, box);
    }

    if (BoxesOverlap(box, nodes[second].Bounds()) != 0)
    {
        QueryCollisionNode(data, second, nodes, box);
    }
}

void QueryCollisionNode(CollisionData* data, s32 node, const CollisionNode* nodes, const Box* box)
{
    s32 first = nodes[node].first;
    s32 second = nodes[node].second;
    if (first < 0)
    {
        AddLeaf(first);
        return;
    }

    if (BoxesOverlap(box, nodes[first].Bounds()) != 0)
    {
        QueryCollisionNode(data, first, nodes, box);
    }

    if (BoxesOverlap(box, nodes[second].Bounds()) != 0)
    {
        QueryCollisionNode(data, second, nodes, box);
    }
}

CollisionSurface* GetCollisionSurface(const CollisionTriangle* triangle)
{
    return &g_CollisionSurfaces.surfaces[triangle->packed >> CollisionTriangle::SurfaceShift];
}

CollisionSurface* GetTriangleSurface(const CollisionHit* hit)
{
    return &g_CollisionSurfaces.surfaces[hit->surface];
}

void GetCollisionTriangleWithCoordinates(CollisionHit* hit, const CollisionTriangle* triangle, const Vector4* vertices)
{
    hit->vertices[0] = vertices[VertexIndex(triangle->packed, 0)];
    hit->vertices[1] = vertices[VertexIndex(triangle->packed, CollisionTriangle::SecondShift)];
    hit->vertices[2] = vertices[VertexIndex(triangle->packed, CollisionTriangle::ThirdShift)];
    hit->surface = static_cast<u16>(triangle->packed >> CollisionTriangle::SurfaceShift);
}

void TransformCollisionHit(CollisionHit* hit, const Matrix4x4* matrix)
{
    for (Vector4& vertex : hit->vertices)
    {
        VuTransformPoint(matrix, &vertex, &vertex);
    }
}

void MakeCollisionHit(CollisionHit* hit, const Vector4* first, const Vector4* second, const Vector4* third)
{
    hit->vertices[0] = *first;
    hit->vertices[1] = *second;
    hit->surface = 0;
    hit->vertices[2] = *third;
}

void TriangleNormal(const CollisionHit* hit, Vector4* normal)
{
    PlaneThroughTriangle(normal, &hit->vertices[0], &hit->vertices[1], &hit->vertices[2]);
}

CollisionSurface* ConstructCollisionSurface(CollisionSurface* surface)
{
    constexpr u32 KeptHeader = 0xFFFC0000;
    // Set on every surface, never read
    constexpr u32 UnreadBits = 0xFF000;
    constexpr u16 None = 0xFFFF;
    surface->header &= KeptHeader;
    surface->id = -1;
    surface->unknown18 = 1.0f;
    surface->unknown1C = 1.0f;
    surface->physics9 = 0.5f;
    surface->physics5 = 0.5f;
    surface->friction = 0.5f;
    surface->physics7 = 0.5f;
    surface->physics8 = 0.5f;
    surface->flow.x = 0.0f;
    surface->flow.w = 1.0f;
    surface->flow.y = 0.0f;
    surface->flow.z = 0.0f;
    ContactMessage::Construct(&surface->contact);
    surface->stepParticles = None;
    for (f32& scale : surface->volumeScales)
    {
        scale = -1.0f;
    }

    surface->impactSound = None;
    surface->hardImpactSound = None;
    surface->stepSound1 = None;
    surface->stepSound2 = None;
    surface->landSound = None;
    surface->impactParticles = None;
    surface->hardImpactParticles = None;
    surface->contact.word = 0;
    surface->contact.byte = 0;
    surface->contact.point = g_DefaultBox.min;
    surface->collisionMask = UnreadBits;
    return surface;
}

void InitCollisionStatics(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    for (CollisionSurface& surface : g_CollisionSurfaces.surfaces)
    {
        ConstructCollisionSurface(&surface);
    }

    g_CollisionSurfaces.count = 0;
    ConstructHullPlaneCache(&g_HullPlaneCache);
}

void ConstructCollisionModule()
{
    InitCollisionStatics(1, 0xFFFF);
}

u32 GetSurfaceParticle(const CollisionSurface* surface, u32 kind)
{
    constexpr u32 None = 0xFFFF;
    switch (kind & 0xFF)
    {
    case 0:
        return surface->impactParticles;
    case 1:
    case 2:
        return surface->stepParticles;
    case 4:
        return surface->hardImpactParticles;
    default:
        return None;
    }
}

u32 GetSurfaceSoundId(const CollisionSurface* surface, u32 kind)
{
    constexpr u32 None = 0xFFFF;
    switch (kind & 0xFF)
    {
    case 0:
        return surface->impactSound;
    case 1:
        return surface->stepSound1;
    case 2:
        return surface->stepSound2;
    case 3:
        return surface->landSound;
    case 4:
        return surface->hardImpactSound;
    case 5:
        return surface->scrapeSound;
    default:
        return None;
    }
}

u32 GetSurfaceSound(const CollisionSurface* surface, u32 kind, f32* volume)
{
    // The volume scales: impact, hard impact, scrape, steps, land
    constexpr u32 None = 0xFFFF;
    switch (kind & 0xFF)
    {
    case 0:
        *volume = surface->volumeScales[0];
        return surface->impactSound;
    case 1:
        *volume = surface->volumeScales[3];
        return surface->stepSound1;
    case 2:
        *volume = surface->volumeScales[3];
        return surface->stepSound2;
    case 3:
        *volume = surface->volumeScales[4];
        return surface->landSound;
    case 4:
        *volume = surface->volumeScales[1];
        return surface->hardImpactSound;
    case 5:
        *volume = surface->volumeScales[2];
        return surface->scrapeSound;
    default:
        return None;
    }
}

u32 BoxesOverlap(const Box* box, const Box* other)
{
    return !(other->max.x < box->min.x || other->max.y < box->min.y || other->max.z < box->min.z || box->max.x < other->min.x ||
             box->max.y < other->min.y || box->max.z < other->min.z);
}

f32 RayPlaneIntersection(const Vector4* plane, const Vector4* start, const Vector4* end, Vector4* point)
{
    f32 x = start->x;
    f32 y = start->y;
    f32 z = start->z;
    f32 dx = end->x - x;
    f32 dy = end->y - y;
    f32 dz = end->z - z;
    f32 share = -(plane->x * x + plane->y * y + plane->z * z + plane->w) / (plane->x * dx + plane->y * dy + plane->z * dz);
    point->x = x + dx * share;
    point->y = y + dy * share;
    point->z = z + dz * share;
    point->w = 1.0f;
    return share;
}

void PlaneFromNormal(Vector4* plane, const Vector4* normal, const Vector4* point)
{
    plane->x = normal->x;
    plane->w = 1.0f;
    plane->y = normal->y;
    plane->z = normal->z;
    plane->w = -(normal->x * point->x + normal->y * point->y + normal->z * point->z);
}

u32 PlaneThroughEdge(Vector4* plane, const Vector4* from, const Vector4* to, const Vector4* direction)
{
    *plane = *to;
    plane->x = plane->x - from->x;
    plane->y = plane->y - from->y;
    plane->z = plane->z - from->z;
    f32 x = plane->x;
    f32 y = plane->y;
    f32 z = plane->z;
    plane->x = y * direction->z - z * direction->y;
    plane->y = z * direction->x - x * direction->z;
    plane->z = x * direction->y - y * direction->x;
    f32 squared = plane->x * plane->x + plane->y * plane->y + plane->z * plane->z;
    f32 inverse = 0.0f;
    if (LengthEpsilon < squared)
    {
        inverse = 1.0f / Kept(__builtin_sqrtf(squared));
    }

    if (inverse == 0.0f)
    {
        return 0;
    }

    plane->x = plane->x * inverse;
    plane->y = plane->y * inverse;
    plane->z = plane->z * inverse;
    plane->w = -(plane->x * from->x + plane->y * from->y + plane->z * from->z);
    return 1;
}

f32 CheckTriangleIntersection(const CollisionHit* triangle, const Vector4* start, const Vector4* end, Vector4* point)
{
    constexpr f32 Epsilon = Rounded(0.0001);
    f32 values[6];
    StartTriangleEdgeTests(start, end, triangle, values);
    // The ray's ends on the plane's two sides
    bool hit = !(-Epsilon < values[0] * values[1]);
    if (hit)
    {
        FinishTriangleEdgeTests(values);
        // Both ends of the ray on the same side of an edge: outside the triangle
        for (u32 edge = 0; edge < 6 && hit; edge += 2)
        {
            if ((Epsilon < values[edge] && Epsilon < values[edge + 1]) ||
                (values[edge] < -Epsilon && values[edge + 1] < -Epsilon))
            {
                hit = false;
            }
        }
    }

    if (!hit)
    {
        return NoHit;
    }

    Vector4 plane;
    PlaneThroughTriangle(&plane, &triangle->vertices[0], &triangle->vertices[1], &triangle->vertices[2]);
    return RayPlaneIntersection(&plane, start, end, point);
}

namespace
{
// The ray wholly on one side of the box
bool RayMissesBox(const CollisionNode* node, const Vector4* start, const Vector4* end)
{
    for (u32 axis = 0; axis < 3; axis++)
    {
        f32 min = node->min[axis];
        if ((&start->x)[axis] < min && (&end->x)[axis] < min)
        {
            return true;
        }
    }

    for (u32 axis = 0; axis < 3; axis++)
    {
        f32 max = node->max[axis];
        if (max < (&start->x)[axis] && max < (&end->x)[axis])
        {
            return true;
        }
    }

    return false;
}

// Where the ray crosses the plane of a box's face of an axis (at the face's value), when that's along the ray and strictly inside
// the box's other two axes: the share of the way and the point
bool FaceCrossing(const CollisionNode* node, const Vector4* start, const Vector4* end, const Vector4& delta, u32 axis, f32 face,
                  f32* share, Vector4* point)
{
    u32 other = axis == 0 ? 1 : axis == 1 ? 2 : 0;
    u32 last = axis == 0 ? 2 : axis == 1 ? 0 : 1;
    const f32* from = &start->x;
    const f32* to = &end->x;
    f32 s = from[axis];
    f32 e = to[axis];
    if (s == e)
    {
        return false;
    }

    if (!((s < face && face < e) || (e < face && face < s)))
    {
        return false;
    }

    f32 t = (face - from[axis]) / (to[axis] - from[axis]);
    if (!(0.0f <= t) || !(t < 1.0f))
    {
        return false;
    }

    Vector4 along;
    along.x = delta.x * t;
    along.y = delta.y * t;
    along.z = delta.z * t;
    point->x = start->x + along.x;
    point->y = start->y + along.y;
    point->z = start->z + along.z;
    point->w = 1.0f;
    const f32* at = &point->x;
    *share = t;
    return node->min[other] < at[other] && at[other] < node->max[other] && node->min[last] < at[last] && at[last] < node->max[last];
}

Vector4 RayDelta(const Vector4* start, const Vector4* end)
{
    Vector4 delta;
    delta.x = end->x - start->x;
    delta.y = end->y - start->y;
    delta.z = end->z - start->z;
    return delta;
}
}

u32 CheckRayIsInsideVolume(const CollisionNode* node, const Vector4* start, const Vector4* end, Vector4* entry, f32* share)
{
    if (node->Bounds()->Contains(start) != 0)
    {
        *entry = *start;
        *share = 0.0f;
        return 1;
    }

    if (RayMissesBox(node, start, end))
    {
        return 0;
    }

    *share = NoHit;
    Vector4 delta = RayDelta(start, end);
    for (u32 axis = 0; axis < 3; axis++)
    {
        const f32 faces[] = {node->min[axis], node->max[axis]};
        for (f32 face : faces)
        {
            f32 t;
            Vector4 point;
            if (FaceCrossing(node, start, end, delta, axis, face, &t, &point) && t < *share)
            {
                *share = t;
                *entry = point;
            }
        }
    }

    return *share < NoHit;
}

u32 RayCrossesBox(const CollisionNode* node, const Vector4* start, const Vector4* end)
{
    if (node->Bounds()->Contains(start) != 0 || node->Bounds()->Contains(end) != 0)
    {
        return 1;
    }

    if (RayMissesBox(node, start, end))
    {
        return 0;
    }

    Vector4 delta = RayDelta(start, end);
    for (u32 axis = 0; axis < 3; axis++)
    {
        const f32 faces[] = {node->min[axis], node->max[axis]};
        for (f32 face : faces)
        {
            f32 t;
            Vector4 point;
            if (FaceCrossing(node, start, end, delta, axis, face, &t, &point))
            {
                return 1;
            }
        }
    }

    return 0;
}

void CheckCollisionRayCast(CollisionData* data, s32 node, RayCast* cast, RayCastResult* result)
{
    // The hit triangle's place, where the box test also leaves its entry point (the retail stack's)
    CollisionHit triangle;
    const CollisionNode* nodes = cast->nodes;
    s32 first = nodes[node].first;
    s32 second = nodes[node].second;
    if (first >= 0)
    {
        f32 share;
        if (CheckRayIsInsideVolume(&cast->nodes[first], cast->start, cast->end, &triangle.vertices[0], &share) != 0)
        {
            CheckCollisionRayCast(data, first, cast, result);
        }

        if (CheckRayIsInsideVolume(&cast->nodes[second], cast->start, cast->end, &triangle.vertices[0], &share) != 0)
        {
            CheckCollisionRayCast(data, second, cast, result);
        }

        return;
    }

    const CollisionGroup* group = &cast->groups[~first];
    for (s32 index = 0; index < group->count; index++)
    {
        const CollisionTriangle* packed = &cast->triangles[group->first + index];
        if ((GetCollisionSurface(packed)->collisionMask & cast->mask) == 0)
        {
            continue;
        }

        GetCollisionTriangleWithCoordinates(&triangle, packed, cast->vertices);
        Vector4 point;
        f32 distance = CheckTriangleIntersection(&triangle, cast->start, cast->end, &point);
        if (!(distance < NoHit) || !(distance < result->distance))
        {
            continue;
        }

        result->distance = distance;
        if (result->position != nullptr)
        {
            *result->position = point;
        }

        if (result->triangle != nullptr)
        {
            CopyHit(result->triangle, &triangle);
        }
    }
}

void CheckCollisionRayCastFast(CollisionData* data, s32 node, FastRayCast* cast)
{
    // The triangle being tested's vertexes (the retail stack's)
    CollisionHit triangle;
    s32 first = cast->nodes[node].first;
    s32 second = cast->nodes[node].second;
    if (first >= 0)
    {
        const CollisionNode* nodes = cast->nodes;
        if (RayCrossesBox(&nodes[first], &cast->start, &cast->end) != 0)
        {
            CheckCollisionRayCastFast(data, first, cast);
        }

        if (RayCrossesBox(&cast->nodes[second], &cast->start, &cast->end) != 0)
        {
            CheckCollisionRayCastFast(data, second, cast);
        }

        return;
    }

    const CollisionGroup* group = &cast->groups[~first];
    s32 index = group->count - 1;
    // The first triangle of the ray's surfaces from the leaf's end starts the tests
    bool started = false;
    while (!started && index >= 0)
    {
        CollisionTriangle packed;
        ConstructCollisionScratch(&packed);
        u32 at = group->first + index;
        index--;
        packed = cast->triangles[at];
        if ((GetCollisionSurface(&packed)->collisionMask & cast->mask) != 0)
        {
            GetCollisionTriangleWithCoordinates(&triangle, &packed, cast->vertices);
            started = true;
        }

        DestroyCollisionScratch(&packed, DestroyOnly);
    }

    if (!started)
    {
        return;
    }

    Platform::Math::StartRayTriangle(triangle.vertices, &cast->nearest);
    while (index >= 0)
    {
        CollisionTriangle packed;
        ConstructCollisionScratch(&packed);
        u32 at = group->first + index;
        index--;
        packed = cast->triangles[at];
        if ((GetCollisionSurface(&packed)->collisionMask & cast->mask) != 0)
        {
            // The next triangle's vertexes made while the last one is tested
            GetCollisionTriangleWithCoordinates(&triangle, &packed, cast->vertices);
            Platform::Math::FinishRayTriangle(&cast->nearest);
            Platform::Math::StartRayTriangle(triangle.vertices, &cast->nearest);
        }

        DestroyCollisionScratch(&packed, DestroyOnly);
    }

    Platform::Math::FinishRayTriangle(&cast->nearest);
}

u32 CheckCollision(CollisionData* data, const Vector4* start, const Vector4* end, u32 mask, f32* distance, Vector4* position,
                   CollisionHit* triangle)
{
    auto* vertices = reinterpret_cast<const Vector4*>(DiskLoadedMemory(GetDiskManager(), &data->verticesHandle));
    auto* nodes = reinterpret_cast<const CollisionNode*>(DiskLoadedMemory(GetDiskManager(), &data->nodesHandle));
    auto* groups = reinterpret_cast<const CollisionGroup*>(DiskLoadedMemory(GetDiskManager(), &data->groupsHandle));
    auto* triangles = reinterpret_cast<const CollisionTriangle*>(DiskLoadedMemory(GetDiskManager(), &data->trianglesHandle));
    if (triangle != nullptr)
    {
        RayCast cast;
        cast.vertices = vertices;
        cast.nodes = nodes;
        cast.groups = groups;
        cast.triangles = triangles;
        cast.start = start;
        cast.end = end;
        cast.mask = mask;
        cast.result.position = position;
        cast.result.triangle = triangle;
        cast.result.distance = NoHit;
        CheckCollisionRayCast(data, 0, &cast, &cast.result);
        if (distance != nullptr)
        {
            *distance = cast.result.distance;
        }

        return cast.result.distance < NoHit;
    }

    FastRayCast cast;
    cast.start = *start;
    cast.end = *end;
    cast.nearest.w = NoHit;
    cast.vertices = vertices;
    cast.nodes = nodes;
    cast.groups = groups;
    cast.triangles = triangles;
    cast.mask = mask;
    Platform::Math::SetRay(&cast.start, &cast.end);
    CheckCollisionRayCastFast(data, 0, &cast);
    if (!(cast.nearest.w < NoHit))
    {
        return 0;
    }

    if (position != nullptr)
    {
        *position = cast.nearest;
        position->w = 1.0f;
    }

    if (distance != nullptr)
    {
        *distance = cast.nearest.w;
    }

    return 1;
}

u32 GetCollisionCheck(ChunkData* chunk, const Vector4* start, const Vector4* end, u32 mask, f32* distance, Vector4* position,
                      CollisionHit* triangle)
{
    if (chunk == nullptr || chunk->collision == nullptr)
    {
        return 0;
    }

    return CheckCollision(static_cast<CollisionData*>(chunk->collision), start, end, mask, distance, position, triangle);
}

void TriangleBox(Box* box, const Vector4* first, const Vector4* second, const Vector4* third)
{
    TriangleBounds(box, first, second, third);
}

namespace
{
// A box's corner by three bits: the min's coordinate for a bit set, the max's otherwise (x bit 0, y bit 1, z bit 2)
f32 PlaneAtCorner(const Box* box, const Vector4* plane, u32 corner)
{
    f32 x = (corner & 1) != 0 ? box->min.x : box->max.x;
    f32 y = (corner & 2) != 0 ? box->min.y : box->max.y;
    f32 z = (corner & 4) != 0 ? box->min.z : box->max.z;
    return plane->x * x + plane->y * y + plane->z * z + plane->w;
}

f32 PlaneAt(const Vector4* plane, const Vector4* point)
{
    return plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
}

// The box in front of the plane of an edge and a point the triangle's normal away from it, the plane turned to face away from
// the triangle's third vertex (only for an edge plane with an area)
bool OutsideEdge(const Vector4* from, const Vector4* to, const Vector4* opposite, const Vector4& normal, const Box* box)
{
    Vector4 away;
    away.x = from->x + normal.x;
    away.y = from->y + normal.y;
    away.z = from->z + normal.z;
    away.w = 1.0f;
    Vector4 plane;
    if (PlaneThroughTriangle(&plane, from, to, &away) == 0)
    {
        return false;
    }

    if (0.0f < PlaneAt(&plane, opposite))
    {
        plane.x = -plane.x;
        plane.y = -plane.y;
        plane.z = -plane.z;
        plane.w = -plane.w;
    }

    return BoxInFrontOfPlane(box, &plane) != 0;
}
}

u32 PlaneCrossesBox(const Box* box, const Vector4* plane)
{
    u32 above = 0;
    u32 below = 0;
    for (u32 corner = 0; corner < 8; corner++)
    {
        if (0.0f < PlaneAtCorner(box, plane, corner))
        {
            above++;
        }
        else
        {
            below++;
        }

        if (above != 0 && below != 0)
        {
            return 1;
        }
    }

    return 0;
}

u32 BoxInFrontOfPlane(const Box* box, const Vector4* plane)
{
    for (u32 corner = 0; corner < 8; corner++)
    {
        if (PlaneAtCorner(box, plane, corner) < 0.0f)
        {
            return 0;
        }
    }

    return 1;
}

u32 TriangleTouchesBox(const Vector4* first, const Vector4* second, const Vector4* third, const Box* box)
{
    Vector4 plane;
    PlaneThroughTriangle(&plane, first, second, third);
    if (PlaneCrossesBox(box, &plane) == 0)
    {
        return 0;
    }

    Vector4 normal;
    normal.x = plane.x;
    normal.y = plane.y;
    normal.z = plane.z;
    if (OutsideEdge(first, second, third, normal, box) || OutsideEdge(second, third, first, normal, box) ||
        OutsideEdge(third, first, second, normal, box))
    {
        return 0;
    }

    return 1;
}

s32 GatherBoxTriangles(CollisionData* data, const Box* box, u32 mask, CollisionHit* triangles, s32 most)
{
    s32 count = 0;
    auto* groups = reinterpret_cast<const CollisionGroup*>(DiskLoadedMemory(GetDiskManager(), &data->groupsHandle));
    auto* vertices = reinterpret_cast<const Vector4*>(DiskLoadedMemory(GetDiskManager(), &data->verticesHandle));
    auto* packed = reinterpret_cast<const CollisionTriangle*>(DiskLoadedMemory(GetDiskManager(), &data->trianglesHandle));
    QueryCollisionBox(data, box);
    for (s32 leaf = 0; leaf < g_CollisionLeafCount; leaf++)
    {
        const CollisionGroup* group = &groups[CollisionLeaves[leaf]];
        for (s32 index = 0; index < group->count; index++)
        {
            const CollisionTriangle* triangle = &packed[group->first + index];
            if ((GetCollisionSurface(triangle)->collisionMask & mask) == 0)
            {
                continue;
            }

            const Vector4* first = &vertices[VertexIndex(triangle->packed, 0)];
            const Vector4* second = &vertices[VertexIndex(triangle->packed, CollisionTriangle::SecondShift)];
            const Vector4* third = &vertices[VertexIndex(triangle->packed, CollisionTriangle::ThirdShift)];
            Box bounds;
            TriangleBox(&bounds, first, second, third);
            if (BoxesOverlap(&bounds, box) == 0 || TriangleTouchesBox(first, second, third, box) == 0)
            {
                continue;
            }

            GetCollisionTriangleWithCoordinates(&triangles[count], triangle, vertices);
            count++;
            if (count == most)
            {
                return count;
            }
        }
    }

    return count;
}

f32 ChunkInstancesRayCast(ChunkData* chunk, const Vector4* segment, u32 mask, InstanceRayHit* hit, u32 flags)
{
    if (chunk->instanceCells == nullptr)
    {
        return NoHit;
    }

    return InstanceCellsRayCast(chunk->instanceCells, segment, mask, hit, flags);
}

u32 ChunkInstancesInSphere(ChunkData* chunk, const Vector4* sphere, u32 kinds, InstanceRayHit* query, u32 flag)
{
    if (chunk->instanceCells == nullptr)
    {
        return 0;
    }

    u16 before = query->count;
    InstanceCellsInSphere(chunk->instanceCells, sphere, kinds, query, flag);
    return static_cast<u16>(query->count - before);
}

u32 ChunkInstancesInCylinder(f32 height, ChunkData* chunk, const Vector4* base, u32 kinds, InstanceRayHit* query)
{
    if (chunk->instanceCells == nullptr)
    {
        return 0;
    }

    u16 before = query->count;
    InstanceCellsInCylinder(height, chunk->instanceCells, base, kinds, query);
    return static_cast<u16>(query->count - before);
}

u32 InstancesInDamageHull(InstanceContext* instance, u32 hull, u32 kinds, InstanceRayHit* query)
{
    ObjectPlace* place = instance->place;
    ChunkData* chunk = instance->chunk;
    RotateAndTranslate(place);
    return ChunkInstancesInHull(chunk, &g_DamageHulls[hull], &place->matrix, kinds, query, 0);
}

void MakeFlatBoxHull()
{
    constexpr f32 HalfSize = 2.0f;
    constexpr f32 HalfHeight = Rounded(0.15);
    Vector4 min = {-HalfSize, -HalfHeight, -HalfSize, 1.0f};
    Vector4 max = {HalfSize, HalfHeight, HalfSize, 1.0f};
    BuildBoxHull(&g_DamageHulls[0], &min, &max);
}

EABI_EXPORT(FUN_001f2080, ChunkInstancesInCylinder);

u32 SegmentHitsInstances(ChunkData* chunk, const Vector4* start, const Vector4* end, InstanceRayHit* hit, u32 mask, f32* share,
                         Vector4* point, u32 flags)
{
    Vector4 segment[2] = {*start, *end};
    f32 t = ChunkInstancesRayCast(chunk, segment, mask, hit, flags);
    if (!(0.0f <= t) || !(t <= 1.0f))
    {
        return 0;
    }

    if (share != nullptr)
    {
        *share = t;
    }

    if (point == nullptr)
    {
        return 1;
    }

    f32 rest = 1.0f - t;
    point->x = start->x * rest + end->x * t;
    point->y = start->y * rest + end->y * t;
    point->z = start->z * rest + end->z * t;
    point->w = 1.0f;
    return 1;
}

namespace
{
bool NoWay(const Vector4* way)
{
    constexpr f32 Epsilon = Rounded(5e-05);
    return __builtin_fabsf(way->x) <= Epsilon && __builtin_fabsf(way->y) <= Epsilon && __builtin_fabsf(way->z) <= Epsilon;
}

void ClearWay(Vector4* way, InstanceRayHit* hit)
{
    constexpr u32 Hit = 1;
    *way = g_DefaultBox.min;
    way->w = 1.0f;
    if (hit != nullptr)
    {
        hit->count = 0;
        hit->distance = NoHit;
        hit->bits &= ~Hit;
        hit->instance = nullptr;
    }
}
}

u32 LineOfSight(ChunkData* chunk, const Vector4* from, Vector4* way, u32 mask, InstanceRayHit* hit, u32 instanceMask)
{
    if (NoWay(way))
    {
        ClearWay(way, hit);
        return 0;
    }

    Vector4 to = *from;
    to.x = to.x + way->x;
    to.y = to.y + way->y;
    to.z = to.z + way->z;
    Vector4 stop;
    u32 blocked = GetCollisionCheck(chunk, from, &to, mask, nullptr, &stop, nullptr);
    if (blocked != 0)
    {
        *way = stop;
        way->x = way->x - from->x;
        way->y = way->y - from->y;
        way->z = way->z - from->z;
    }

    if (NoWay(way))
    {
        ClearWay(way, hit);
        return blocked;
    }

    if (hit == nullptr)
    {
        return blocked;
    }

    Vector4 segment[2];
    segment[0] = *from;
    segment[1].x = from->x + way->x;
    segment[1].y = from->y + way->y;
    segment[1].z = from->z + way->z;
    segment[1].w = 1.0f;
    f32 t = ChunkInstancesRayCast(chunk, segment, instanceMask, hit, 0);
    if (!(0.0f <= t) || !(t <= 1.0f))
    {
        return blocked;
    }

    way->x = way->x * t;
    way->y = way->y * t;
    way->z = way->z * t;
    return 1;
}

namespace
{
// The instances along a segment the collision (or a list of triangles) stopped or didn't: SegmentHitsAnything's second half
u32 FinishSegmentCast(ChunkData* chunk, const Vector4* start, const Vector4* end, bool blocked, f32 distance, InstanceRayHit* hit,
                      u32 instanceMask, f32* share, Vector4* point, u32 flags)
{
    if (!blocked)
    {
        return SegmentHitsInstances(chunk, start, end, hit, instanceMask, share, point, flags);
    }

    if (!(0.0f < distance))
    {
        if (share != nullptr)
        {
            *share = 0.0f;
        }

        return 1;
    }

    f32 rest = 1.0f - distance;
    Vector4 stop;
    stop.x = start->x * distance + end->x * rest;
    stop.y = start->y * distance + end->y * rest;
    stop.z = start->z * distance + end->z * rest;
    stop.w = 1.0f;
    Vector4 segment[2] = {*start, stop};
    f32 t = ChunkInstancesRayCast(chunk, segment, instanceMask, hit, flags);
    bool hitInstance = false;
    if (0.0f <= t && t <= 1.0f)
    {
        if (point != nullptr)
        {
            f32 before = 1.0f - t;
            point->x = start->x * before + stop.x * t;
            point->y = start->y * before + stop.y * t;
            point->z = start->z * before + stop.z * t;
            point->w = 1.0f;
        }

        hitInstance = true;
    }

    if (share != nullptr)
    {
        *share = hitInstance ? t / distance : distance;
    }

    return 1;
}
}

u32 SegmentHitsAnything(ChunkData* chunk, const Vector4* start, const Vector4* end, u32 mask, InstanceRayHit* hit,
                        u32 instanceMask, f32* share, Vector4* point, CollisionHit* triangle)
{
    f32 distance;
    u32 blocked = 0;
    if (chunk != nullptr && chunk->collision != nullptr)
    {
        blocked = CheckCollision(static_cast<CollisionData*>(chunk->collision), start, end, mask, &distance, point, triangle);
    }

    // Only the result's low byte counts here. The retail code hands the triangle's pointer on as the instances' cast's flags
    return FinishSegmentCast(chunk, start, end, (blocked & 0xFF) != 0, distance, hit, instanceMask, share, point,
                             reinterpret_cast<u32>(triangle));
}

u32 SegmentHitsTrianglesOrInstances(ChunkData* chunk, CollisionCache* cache, const Vector4* start, const Vector4* end,
                                    InstanceRayHit* hit, u32 instanceMask, f32* share, Vector4* point, CollisionHit* triangle)
{
    f32 distance;
    u32 blocked = TriangleListRayCast(cache, start, end, &distance, point, triangle);
    return FinishSegmentCast(chunk, start, end, blocked != 0, distance, hit, instanceMask, share, point,
                             reinterpret_cast<u32>(triangle));
}

CollisionHit* FirstCollisionHit(CollisionCache* cache)
{
    if (cache->count == 0)
    {
        return nullptr;
    }

    cache->index = 0;
    cache->block = cache->first;
    return &cache->first->hits[0];
}

CollisionHit* NextCollisionHit(CollisionCache* cache)
{
    constexpr u32 PerBlock = 8;
    u16 index = cache->index + 1;
    cache->index = index;
    if (static_cast<s16>(index) == cache->count)
    {
        return nullptr;
    }

    u32 slot = index & (PerBlock - 1);
    if (slot == 0)
    {
        cache->block = cache->block->next;
    }

    return &cache->block->hits[slot];
}

u32 TriangleListRayCast(CollisionCache* cache, const Vector4* start, const Vector4* end, f32* distance, Vector4* point,
                        CollisionHit* triangle)
{
    f32 nearest = NoHit;
    CollisionHit* hit = FirstCollisionHit(cache);
    while (hit != nullptr)
    {
        CollisionHit* next = NextCollisionHit(cache);
        Vector4 at;
        f32 d = CheckTriangleIntersection(hit, start, end, &at);
        if (d < NoHit && d < nearest)
        {
            nearest = d;
            if (point != nullptr)
            {
                *point = at;
            }

            if (triangle != nullptr)
            {
                CopyHit(triangle, hit);
            }
        }

        hit = next;
    }

    if (distance != nullptr)
    {
        *distance = nearest;
    }

    return nearest < NoHit;
}

s32 GatherBoxTrianglePointers(CollisionData* data, const Box* box, u32 mask, const CollisionTriangle** triangles, s32 most)
{
    s32 count = 0;
    auto* groups = reinterpret_cast<const CollisionGroup*>(DiskLoadedMemory(GetDiskManager(), &data->groupsHandle));
    auto* vertices = reinterpret_cast<const Vector4*>(DiskLoadedMemory(GetDiskManager(), &data->verticesHandle));
    auto* packed = reinterpret_cast<const CollisionTriangle*>(DiskLoadedMemory(GetDiskManager(), &data->trianglesHandle));
    QueryCollisionBox(data, box);
    for (s32 leaf = 0; leaf < g_CollisionLeafCount; leaf++)
    {
        const CollisionGroup* group = &groups[CollisionLeaves[leaf]];
        for (s32 index = 0; index < group->count; index++)
        {
            const CollisionTriangle* triangle = &packed[group->first + index];
            if ((GetCollisionSurface(triangle)->collisionMask & mask) == 0)
            {
                continue;
            }

            const Vector4* first = &vertices[VertexIndex(triangle->packed, 0)];
            const Vector4* second = &vertices[VertexIndex(triangle->packed, CollisionTriangle::SecondShift)];
            const Vector4* third = &vertices[VertexIndex(triangle->packed, CollisionTriangle::ThirdShift)];
            Box bounds;
            TriangleBox(&bounds, first, second, third);
            if (BoxesOverlap(&bounds, box) == 0 || TriangleTouchesBox(first, second, third, box) == 0)
            {
                continue;
            }

            triangles[count] = triangle;
            count++;
            if (count == most)
            {
                return count;
            }
        }
    }

    return count;
}

s32 ChunkBoxTriangles(ChunkData* chunk, const Box* box, u32 mask, CollisionHit* triangles, s32 most)
{
    if (chunk == nullptr || chunk->collision == nullptr)
    {
        return 0;
    }

    return GatherBoxTriangles(static_cast<CollisionData*>(chunk->collision), box, mask, triangles, most);
}

s32 ChunkBoxTrianglePointers(ChunkData* chunk, const Box* box, u32 mask, const CollisionTriangle** triangles, s32 most)
{
    if (chunk == nullptr || chunk->collision == nullptr)
    {
        return 0;
    }

    return GatherBoxTrianglePointers(static_cast<CollisionData*>(chunk->collision), box, mask, triangles, most);
}

const Vector4* ChunkCollisionVertices(ChunkData* chunk)
{
    if (chunk == nullptr || chunk->collision == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<const Vector4*>(
        DiskLoadedMemory(GetDiskManager(), &static_cast<CollisionData*>(chunk->collision)->verticesHandle));
}

u32 BoxInsideBox(const Box* box, const Box* outer)
{
    return !(box->min.x < outer->min.x || box->min.y < outer->min.y || box->min.z < outer->min.z || outer->max.x < box->max.x ||
             outer->max.y < box->max.y || outer->max.z < box->max.z);
}

void GrowBox(f32 margin, Box* box)
{
    box->min.x = box->min.x - margin;
    box->min.y = box->min.y - margin;
    box->min.z = box->min.z - margin;
    box->max.x = box->max.x + margin;
    box->max.z = box->max.z + margin;
    box->max.y = box->max.y + margin;
}

void GrowBoxByVector(Box* box, const Vector4* vector)
{
    f32 margin = __builtin_fabsf(vector->x);
    f32 y = __builtin_fabsf(vector->y);
    if (margin < y)
    {
        margin = y;
    }

    f32 z = __builtin_fabsf(vector->z);
    if (margin < z)
    {
        margin = z;
    }

    GrowBox(margin, box);
}

void MergeBox(Box* into, const Box* box)
{
    if (box->min.x < into->min.x)
    {
        into->min.x = box->min.x;
    }

    if (box->min.y < into->min.y)
    {
        into->min.y = box->min.y;
    }

    if (box->min.z < into->min.z)
    {
        into->min.z = box->min.z;
    }

    if (into->max.x < box->max.x)
    {
        into->max.x = box->max.x;
    }

    if (into->max.y < box->max.y)
    {
        into->max.y = box->max.y;
    }

    if (into->max.z < box->max.z)
    {
        into->max.z = box->max.z;
    }
}

void ResetBox(Box* box)
{
    box->min.z = NoHit;
    box->max.x = -NoHit;
    box->min.x = NoHit;
    box->min.y = NoHit;
    box->min.w = 1.0f;
    box->max.w = 1.0f;
    box->max.z = -NoHit;
    box->max.y = -NoHit;
}

void TransformBox(Box* box, const Matrix4x4* matrix)
{
    Box old = *box;
    ResetBox(box);
    for (u32 corner = 0; corner < 8; corner++)
    {
        // The min's coordinate for a bit set, the max's otherwise
        Vector4 point;
        point.x = (corner & 1) != 0 ? old.min.x : old.max.x;
        point.y = (corner & 2) != 0 ? old.min.y : old.max.y;
        point.z = (corner & 4) != 0 ? old.min.z : old.max.z;
        point.w = 1.0f;
        VuTransformPoint(matrix, &point, &point);
        if (point.x < box->min.x)
        {
            box->min.x = point.x;
        }

        if (point.y < box->min.y)
        {
            box->min.y = point.y;
        }

        if (point.z < box->min.z)
        {
            box->min.z = point.z;
        }

        if (box->max.x < point.x)
        {
            box->max.x = point.x;
        }

        if (box->max.y < point.y)
        {
            box->max.y = point.y;
        }

        if (box->max.z < point.z)
        {
            box->max.z = point.z;
        }
    }
}

u32 SphereBoxPush(f32 radius, const Box* box, const Vector4* centre, Vector4* push)
{
    if (centre->x < box->min.x - radius || box->max.x + radius < centre->x || centre->y < box->min.y - radius ||
        box->max.y + radius < centre->y || centre->z < box->min.z - radius || box->max.z + radius < centre->z)
    {
        return 0;
    }

    // How far outside each face the centre is (min x, max x, min y, max y, min z, max z): those it's outside of, the nearest of
    // the others
    bool outside[6];
    s32 outsideCount = 0;
    s32 nearest = -1;
    f32 nearestDistance = -Rounded(1e30);
    for (s32 face = 0; face < 6; face++)
    {
        f32 distance;
        switch (face)
        {
        case 0:
            distance = box->min.x - centre->x;
            break;
        case 1:
            distance = centre->x - box->max.x;
            break;
        case 2:
            distance = box->min.y - centre->y;
            break;
        case 3:
            distance = centre->y - box->max.y;
            break;
        case 4:
            distance = box->min.z - centre->z;
            break;
        default:
            distance = centre->z - box->max.z;
            break;
        }

        if (0.0f < distance)
        {
            outside[face] = true;
            outsideCount++;
        }
        else
        {
            outside[face] = false;
            if (nearestDistance < distance)
            {
                nearestDistance = distance;
                nearest = face;
            }
        }
    }

    if (outsideCount == 0)
    {
        if (nearest < 0 || nearest > 5)
        {
            return 1;
        }

        f32 out = (nearest & 1) == 0 ? nearestDistance - radius : radius - nearestDistance;
        push->x = nearest < 2 ? out : 0.0f;
        push->y = nearest >= 2 && nearest < 4 ? out : 0.0f;
        push->z = nearest >= 4 ? out : 0.0f;
        push->w = 1.0f;
        return 1;
    }

    if (outsideCount == 1)
    {
        push->x = 0.0f;
        push->y = 0.0f;
        push->z = 0.0f;
        push->w = 1.0f;
        if (outside[0])
        {
            push->x = (box->min.x - radius) - centre->x;
        }
        else if (outside[1])
        {
            push->x = (box->max.x + radius) - centre->x;
        }
        else if (outside[2])
        {
            push->y = (box->min.y - radius) - centre->y;
        }
        else if (outside[3])
        {
            push->y = (box->max.y + radius) - centre->y;
        }
        else if (outside[4])
        {
            push->z = (box->min.z - radius) - centre->z;
        }
        else if (outside[5])
        {
            push->z = (box->max.z + radius) - centre->z;
        }

        return 1;
    }

    f32 squaredRadius = radius * radius;
    Vector4 corner;
    corner.x = outside[0] ? box->min.x : box->max.x;
    corner.y = outside[2] ? box->min.y : box->max.y;
    corner.z = outside[4] ? box->min.z : box->max.z;
    corner.w = 1.0f;
    if (outsideCount == 2)
    {
        // Beside an edge: along the axis the centre is within
        Vector4 along;
        if (!outside[0] && !outside[1])
        {
            corner.x = box->min.x;
            along = {1.0f, 0.0f, 0.0f, 1.0f};
        }
        else if (!outside[2] && !outside[3])
        {
            corner.y = box->min.y;
            along = {0.0f, 1.0f, 0.0f, 1.0f};
        }
        else
        {
            corner.z = box->min.z;
            along = {0.0f, 0.0f, 1.0f, 1.0f};
        }

        Vector4 toEdge;
        toEdge.x = corner.x - centre->x;
        toEdge.y = corner.y - centre->y;
        toEdge.z = corner.z - centre->z;
        toEdge.w = 1.0f;
        f32 share = toEdge.x * along.x + toEdge.y * along.y + toEdge.z * along.z;
        toEdge.x = toEdge.x - along.x * share;
        toEdge.y = toEdge.y - along.y * share;
        toEdge.z = toEdge.z - along.z * share;
        f32 squared = toEdge.x * toEdge.x + toEdge.y * toEdge.y + toEdge.z * toEdge.z;
        if (!(squared < squaredRadius))
        {
            return 0;
        }

        f32 inverse = InverseLength(&toEdge, LengthEpsilon);
        f32 gap = __builtin_sqrtf(squared) - radius;
        push->x = toEdge.x * inverse * gap;
        push->y = toEdge.y * inverse * gap;
        push->z = toEdge.z * inverse * gap;
        push->w = 1.0f;
        return 1;
    }

    // Beside a corner
    Vector4 away;
    away.x = centre->x - corner.x;
    away.y = centre->y - corner.y;
    away.z = centre->z - corner.z;
    away.w = 1.0f;
    f32 squared = away.x * away.x + away.y * away.y + away.z * away.z;
    if (!(squared < squaredRadius))
    {
        return 0;
    }

    f32 inverse = InverseLength(&away, LengthEpsilon);
    f32 depth = radius - __builtin_sqrtf(squared);
    push->x = away.x * inverse * depth;
    push->y = away.y * inverse * depth;
    push->z = away.z * inverse * depth;
    push->w = 1.0f;
    return 1;
}

f32 GetBoxReach(const Box* box)
{
    f32 x = box->max.x - box->min.x;
    f32 y = box->max.y - box->min.y;
    f32 z = box->max.z - box->min.z;
    f32 reach = x;
    if (x < y)
    {
        reach = y;
    }

    if (reach < z)
    {
        reach = z;
    }

    return reach;
}

u32 BoxContainsRegion(const Box* box, const Vector4* min, const Vector4* max)
{
    const f32 low[3] = {box->min.x, box->min.y, box->min.z};
    const f32 high[3] = {box->max.x, box->max.y, box->max.z};
    const f32 from[3] = {min->x, min->y, min->z};
    const f32 to[3] = {max->x, max->y, max->z};
    for (u32 axis = 0; axis < 3; axis++)
    {
        if (from[axis] <= low[axis] || high[axis] <= to[axis])
        {
            return 0;
        }
    }

    return 1;
}

void FreeCollisionCacheBlocks(CollisionCache* cache)
{
    CollisionHitBlock* block = cache->first;
    while (block != nullptr)
    {
        CollisionHitBlock* next = block->next;
        MemoryDeallocate2_(block);
        block = next;
    }

    cache->first = nullptr;
}

void MakeCollisionCacheBlocks(CollisionCache* cache, s32 count)
{
    FreeCollisionCacheBlocks(cache);
    if (count == 0)
    {
        return;
    }

    auto* block = static_cast<CollisionHitBlock*>(MemoryAllocate(sizeof(CollisionHitBlock)));
    cache->first = block;
    block->next = nullptr;
    for (s32 made = 1; made < count; made++)
    {
        auto* next = static_cast<CollisionHitBlock*>(MemoryAllocate(sizeof(CollisionHitBlock)));
        next->next = nullptr;
        block->next = next;
        block = next;
    }
}

CollisionCache* ConstructCollisionCache(CollisionCache* cache, ReferencedObject* owner, u32 mask)
{
    cache->index = 0xFFFF;
    cache->owner = owner;
    cache->margin = 1.0f;
    cache->mask = mask;
    cache->count = 0;
    cache->block = nullptr;
    cache->first = nullptr;
    cache->box.min = g_DefaultBox.min;
    cache->box.max = g_DefaultBox.min;
    return cache;
}

void DestroyCollisionCache(CollisionCache* cache, u32 destroyFlags)
{
    FreeCollisionCacheBlocks(cache);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(cache);
    }
}

u32 RefreshCollisionCache(CollisionCache* cache, const Box* box)
{
    constexpr s32 MostTriangles = 0x400;
    if (BoxInsideBox(box, &cache->box) != 0)
    {
        return 0;
    }

    cache->box = *box;
    GrowBox(cache->margin, &cache->box);
    ChunkData* chunk = cache->owner->chunk;
    const CollisionTriangle* triangles[MostTriangles];
    s32 count = ChunkBoxTrianglePointers(chunk, &cache->box, cache->mask, triangles, MostTriangles);
    const Vector4* vertices = ChunkCollisionVertices(chunk);
    cache->count = count;
    MakeCollisionCacheBlocks(cache, (static_cast<s16>(count) + 7) >> 3);
    const CollisionTriangle** next = triangles;
    for (CollisionHit* hit = FirstCollisionHit(cache); hit != nullptr; hit = NextCollisionHit(cache))
    {
        GetCollisionTriangleWithCoordinates(hit, *next, vertices);
        next++;
    }

    return 1;
}

u32 SphereTouchesTriangle(f32 radius, const CollisionHit* triangle, const Vector4* centre, Vector4* out)
{
    constexpr f32 WithinEdge = Rounded(0.001);
    const Vector4* vertices = triangle->vertices;
    Box bounds;
    TriangleBox(&bounds, &vertices[0], &vertices[1], &vertices[2]);
    Box sphere;
    sphere.min = *centre;
    sphere.max = *centre;
    sphere.min.x = sphere.min.x - radius;
    sphere.min.y = sphere.min.y - radius;
    sphere.min.z = sphere.min.z - radius;
    sphere.max.x = sphere.max.x + radius;
    sphere.max.y = sphere.max.y + radius;
    sphere.max.z = sphere.max.z + radius;
    if (BoxesOverlap(&bounds, &sphere) == 0)
    {
        return 0;
    }

    Vector4 plane;
    PlaneThroughTriangle(&plane, &vertices[0], &vertices[1], &vertices[2]);
    f32 distance = PlaneAt(&plane, centre);
    if (radius < distance)
    {
        return 0;
    }

    f32 negative = -radius;
    if (distance < negative)
    {
        return 0;
    }

    Vector4 normal;
    normal.x = plane.x;
    normal.y = plane.y;
    normal.z = plane.z;
    normal.w = 1.0f;
    // How far the centre is outside each edge (the edges' planes along the normal face away from the triangle)
    f32 outside[3];
    Vector4 edgePlane;
    PlaneThroughEdge(&edgePlane, &vertices[0], &vertices[1], &normal);
    outside[0] = PlaneAt(&edgePlane, centre);
    PlaneThroughEdge(&edgePlane, &vertices[1], &vertices[2], &normal);
    outside[1] = PlaneAt(&edgePlane, centre);
    PlaneThroughEdge(&edgePlane, &vertices[2], &vertices[0], &normal);
    outside[2] = PlaneAt(&edgePlane, centre);
    if (outside[0] < WithinEdge && outside[1] < WithinEdge && outside[2] < WithinEdge)
    {
        if (out != nullptr)
        {
            f32 depth = PlaneAt(&plane, centre) + radius;
            *out = *centre;
            out->z = out->z - normal.z * depth;
            out->y = out->y - normal.y * depth;
            out->x = out->x - normal.x * depth;
        }

        return 1;
    }

    for (s32 edge = 0; edge < 3; edge++)
    {
        if (radius < outside[edge])
        {
            return 0;
        }
    }

    f32 squaredRadius = radius * radius;
    Vector4 edges[3];
    for (s32 edge = 0; edge < 3; edge++)
    {
        const Vector4* from = &vertices[edge];
        const Vector4* to = &vertices[edge == 2 ? 0 : edge + 1];
        edges[edge].x = to->x - from->x;
        edges[edge].y = to->y - from->y;
        edges[edge].z = to->z - from->z;
        edges[edge].w = 1.0f;
    }

    for (s32 edge = 0; edge < 3; edge++)
    {
        f32 inverse = InverseLength(&edges[edge], LengthEpsilon);
        edges[edge].x = edges[edge].x * inverse;
        edges[edge].y = edges[edge].y * inverse;
        edges[edge].z = edges[edge].z * inverse;
    }

    // The edge the centre is beside: outside it and between its ends
    s32 nearest = 0;
    for (; nearest < 3; nearest++)
    {
        if (0.0f < outside[nearest])
        {
            Vector4 start;
            Vector4 finish;
            PlaneFromNormal(&start, &edges[nearest], &vertices[nearest]);
            PlaneFromNormal(&finish, &edges[nearest], &vertices[nearest == 0 ? 1 : nearest == 1 ? 2 : 0]);
            if (0.0f < PlaneAt(&start, centre) && PlaneAt(&finish, centre) < 0.0f)
            {
                break;
            }
        }
    }

    if (nearest != 3)
    {
        const Vector4* vertex = &vertices[nearest];
        const Vector4* along = &edges[nearest];
        Vector4 toVertex;
        toVertex.x = vertex->x - centre->x;
        toVertex.y = vertex->y - centre->y;
        toVertex.z = vertex->z - centre->z;
        toVertex.w = 1.0f;
        f32 share = toVertex.x * along->x + toVertex.y * along->y + toVertex.z * along->z;
        Vector4 toEdge;
        toEdge.x = toVertex.x - along->x * share;
        toEdge.y = toVertex.y - along->y * share;
        toEdge.z = toVertex.z - along->z * share;
        toEdge.w = 1.0f;
        f32 squared = toEdge.x * toEdge.x + toEdge.y * toEdge.y + toEdge.z * toEdge.z;
        if (!(squared < squaredRadius))
        {
            return 0;
        }

        if (out == nullptr)
        {
            return 1;
        }

        f32 inverse = InverseLength(&toEdge, LengthEpsilon);
        f32 gap = __builtin_sqrtf(squared) - radius;
        *out = *centre;
        out->x = out->x + toEdge.x * inverse * gap;
        out->y = out->y + toEdge.y * inverse * gap;
        out->z = out->z + toEdge.z * inverse * gap;
        return 1;
    }

    // The nearest vertex (none when no distance is below 1e30: the one before the first, as retail)
    s32 vertexIndex = -1;
    f32 best = NoHit;
    for (s32 index = 0; index < 3; index++)
    {
        f32 x = vertices[index].x - centre->x;
        f32 y = vertices[index].y - centre->y;
        f32 z = vertices[index].z - centre->z;
        f32 squared = x * x + y * y + z * z;
        if (squared < best)
        {
            best = squared;
            vertexIndex = index;
        }
    }

    const Vector4* vertex = &vertices[vertexIndex];
    Vector4 toVertex;
    toVertex.x = vertex->x - centre->x;
    toVertex.y = vertex->y - centre->y;
    toVertex.z = vertex->z - centre->z;
    toVertex.w = 1.0f;
    if (!(toVertex.x * toVertex.x + toVertex.y * toVertex.y + toVertex.z * toVertex.z < squaredRadius))
    {
        return 0;
    }

    if (out == nullptr)
    {
        return 1;
    }

    f32 inverse = InverseLength(&toVertex, LengthEpsilon);
    *out = *vertex;
    out->x = out->x + toVertex.x * inverse * negative;
    out->y = out->y + toVertex.y * inverse * negative;
    out->z = out->z + toVertex.z * inverse * negative;
    return 1;
}

u32 EllipsoidTouchesTriangle(f32 radiusX, f32 radiusY, f32 radiusZ, const CollisionHit* triangle, const Matrix4x4* matrix,
                             Vector4* out)
{
    Matrix4x4 inverse = *matrix;
    VuInvertRigidInPlace(&inverse);
    CollisionHit local;
    CopyHit(&local, triangle);
    TransformCollisionHit(&local, &inverse);
    Matrix4x4 scale;
    InitIdentityMatrix(&scale);
    scale.m[0][0] = 1.0f / radiusX;
    scale.m[1][1] = 1.0f / radiusY;
    scale.m[2][2] = 1.0f / radiusZ;
    TransformCollisionHit(&local, &scale);
    if (SphereTouchesTriangle(1.0f, &local, &g_DefaultBox.min, out) == 0)
    {
        return 0;
    }

    if (out != nullptr)
    {
        out->x = out->x * radiusX;
        out->y = out->y * radiusY;
        out->z = out->z * radiusZ;
        VuTransformPoint(matrix, out, out);
    }

    return 1;
}

namespace
{
// The way out of the contacts' spaces looked for once a point is in them, the inverse lengths' epsilon, and the motion the
// contacts are gathered for a hull standing still with
constexpr f32 NoMargin = 0.0f;
constexpr f32 InverseEpsilon = 0x1.5798ecp-29f;
constexpr f32 StillMotion = Rounded(5e-5);
}

void AccelerateOnSurface(CollisionSurface* surface, f32 seconds, Vector4* velocity, const Vector4* wanted, const Vector4* normal)
{
    Vector4 target = *wanted;
    Vector4 pull = g_DefaultBox.min;
    pull.w = 1.0f;
    if (0.0f < surface->physics8)
    {
        Vector4 downhill = *normal;
        if (normal->y < surface->physics9)
        {
            downhill.y = 0.0f;
            f32 inverse = InverseLength(&downhill, InverseEpsilon);
            f32 across = __builtin_sqrtf(normal->x * normal->x + normal->z * normal->z);
            downhill.x = downhill.x * inverse;
            downhill.y = downhill.y * inverse;
            downhill.z = downhill.z * inverse;
            // All of it from a normal half way down from up (30 degrees)
            f32 share = across + across;
            if (1.0f < share)
            {
                share = 1.0f;
            }

            f32 strength = surface->physics8;
            pull.x = downhill.x * strength * share;
            pull.y = downhill.y * strength * share;
            pull.z = downhill.z * strength * share;
            pull.w = 1.0f;
        }
    }

    velocity->x = velocity->x + pull.x * seconds;
    velocity->z = velocity->z + pull.z * seconds;
    Vector4 way;
    way.x = (target.x - velocity->x) + surface->flow.x;
    way.y = 0.0f;
    way.z = (target.z - velocity->z) + surface->flow.z;
    way.w = 1.0f;
    f32 most = surface->physics5 * seconds;
    f32 length = __builtin_sqrtf(way.x * way.x + way.z * way.z);
    if (length != 0.0f)
    {
        f32 inverse = InverseLength(&way, InverseEpsilon);
        f32 step = most < length ? most : length;
        velocity->x = velocity->x + way.x * inverse * step;
        velocity->z = velocity->z + way.z * inverse * step;
    }

    velocity->y = target.y;
}

u32 HullOverlaps(ChunkData* chunk, const CollisionHull* hull, const Vector4* position, u32, u32 instanceMask,
                 ReferencedObject* const* leftOut, s32 leftOutCount, InstanceContext** touched, s32 mostTouched, s32* touchedCount,
                 Vector4* away)
{
    if (chunk == nullptr)
    {
        return 0;
    }

    u32 overlaps = 0;
    ContactSet* contacts = BeginContacts();
    Vector4 motion = {StillMotion, StillMotion, StillMotion, 1.0f};
    if (touchedCount != nullptr)
    {
        *touchedCount = 0;
    }

    if (away != nullptr)
    {
        *away = g_DefaultBox.min;
        away->w = 1.0f;
    }

    GatherTriangleContacts(contacts, chunk, position, &motion, hull);
    GatherInstanceContacts(contacts, chunk, position, &motion, &instanceMask,
                           reinterpret_cast<InstanceContext**>(const_cast<ReferencedObject**>(leftOut)), leftOutCount, hull);
    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        Contact* contact = &contacts->solid.contacts[index];
        if ((contact->kind & Contact::SphereBit) != 0)
        {
            continue;
        }

        s32 outside = -1;
        if (PointInsidePlanes(NoMargin, &contact->space, position, &outside) == 0)
        {
            continue;
        }

        if (away != nullptr)
        {
            Vector4 push;
            PushOutOfSpaceAgain(&contact->space, position, &push);
            away->x = away->x + push.x;
            away->y = away->y + push.y;
            away->z = away->z + push.z;
        }

        overlaps++;
        if ((contact->kind & Contact::KindHull) == 0 || touched == nullptr)
        {
            continue;
        }

        *touched = contact->instance;
        (*touchedCount)++;
        if (*touchedCount == mostTouched)
        {
            break;
        }

        touched++;
    }

    EndContacts();
    if (away != nullptr)
    {
        f32 inverse = InverseLength(away, InverseEpsilon);
        away->x = away->x * inverse;
        away->y = away->y * inverse;
        away->z = away->z * inverse;
    }

    return overlaps != 0;
}

u32 CastHullDown(f32 distance, ChunkData* chunk, const CollisionHull* hull, const Vector4* from, u32, u32 instanceMask,
                 Vector4* found, InstanceContext* const* leftOut, s32 leftOutCount)
{
    // The steps down stop being halved below this
    constexpr f32 FinestStep = Rounded(0.002);
    if (chunk == nullptr)
    {
        return 0;
    }

    Box box;
    HullBox(hull, &box);
    // A hull of no height never steps down: unless it starts inside something the loop never ends (retail's; the game's hulls
    // have height)
    f32 step = (box.max.y - box.min.y) * 0.5f;
    Vector4 motion = {StillMotion, StillMotion, StillMotion, 1.0f};
    Vector4 at = *from;
    while (true)
    {
        Vector4 position = at;
        position.y = position.y - step;
        ContactSet* contacts = BeginContacts();
        GatherTriangleContacts(contacts, chunk, &position, &motion, hull);
        GatherInstanceContacts(contacts, chunk, &position, &motion, &instanceMask, const_cast<InstanceContext**>(leftOut),
                               leftOutCount, hull);
        if (PointInsideSolidContact(NoMargin, contacts, &position, Contact::SphereBit) == 0)
        {
            at = position;
            if (distance < from->y - at.y)
            {
                EndContacts();
                return 0;
            }
        }
        else
        {
            step = step * 0.5f;
            if (step < FinestStep)
            {
                *found = at;
                EndContacts();
                return 1;
            }
        }

        EndContacts();
    }
}

// The EABI's call of CastHullDown (its integers in $a0-$a7, the distance in $f12) made n32's: the ninth argument (the count of the
// instances left out) goes on the stack, which Abi::Thunk doesn't do
asm(R"(
    .pushsection .text.FUN_002816f8, "ax", @progbits
    .globl FUN_002816f8
    .type FUN_002816f8, @function
    .set push
    .set noreorder
FUN_002816f8:
    addiu $sp, $sp, -16
    sd $31, 8($sp)
    sd $11, 0($sp)
    move $11, $10
    move $10, $9
    move $9, $8
    move $8, $7
    move $7, $6
    move $6, $5
    jal FUN_002816f8_n32
    move $5, $4
    ld $31, 8($sp)
    jr $31
    addiu $sp, $sp, 16
    .set pop
    .size FUN_002816f8, . - FUN_002816f8
    .popsection
)");
