#include "game/physics.h"

#include "game/instances.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/rigidbody.h"
#include "retail/libc.h"

EABI_EXPORT(FUN_00288200, PointInsidePlanes);
EABI_EXPORT(FUN_00284560, PointInsideSolidContact);
EABI_EXPORT(FUN_00285648, PushOutOfSpheres);
EABI_EXPORT(FUN_002846a8, CountContactsContaining);
EABI_EXPORT(FUN_002871c8, Move);
EABI_EXPORT(FUN_00286f48, MoveCharacter);
EABI_EXPORT(FUN_00287cc8, CastDown);

namespace
{
// An edge cross product shorter (squared) is no axis
constexpr f32 ShortAxis = 0x1.5798ecp-27f;
// Each separating axis keeps a space's two planes (one on either side)
constexpr s32 PlanesPerAxis = 2;

enum AxisTag : u32
{
    FirstNormal = 0,
    SecondNormal = 1,
    EdgeCross = 2,
};

void TagAxis(Vector4* axis, u32 tag)
{
    *reinterpret_cast<u32*>(&axis->w) = tag;
}

// The gathered triangles made contacts: the solid ones (bit 20 of their surface) and the others (with no instance)
void AddGatheredTriangles(ContactSet* set, s32 count, const CollisionHull* hull)
{
    for (s32 index = 0; index < count; index++)
    {
        const CollisionHit* triangle = &g_GatheredTriangles[index];
        if (GetTriangleSurface(triangle)->flags.solidToPlayer != 0)
        {
            CollisionHit* kept = &set->solid.triangles[set->solid.count];
            kept->vertices[0] = triangle->vertices[0];
            kept->vertices[1] = triangle->vertices[1];
            kept->vertices[2] = triangle->vertices[2];
            kept->surface = triangle->surface;
            kept->unused32 = triangle->unused32;
            AddTriangleContact(set, triangle, hull, 1);
        }
        else
        {
            set->others.contacts[set->others.count].instance = nullptr;
            CollisionHit* kept = &set->others.triangles[set->others.count];
            kept->vertices[0] = triangle->vertices[0];
            kept->vertices[1] = triangle->vertices[1];
            kept->vertices[2] = triangle->vertices[2];
            kept->surface = triangle->surface;
            kept->unused32 = triangle->unused32;
            AddTriangleContact(set, triangle, hull, 0);
        }
    }
}

// The room a copy of so many planes takes: 22 or 23 of them take the room of 24 (and the copy reads past the source's end, as
// retail does)
u32 PlanesCopySize(s32 count)
{
    constexpr s32 FirstWidened = 22;
    constexpr s32 Widened = 24;
    u32 size = static_cast<u32>(count) * sizeof(Vector4);
    if (static_cast<u32>(count - FirstWidened) < Widened - FirstWidened)
    {
        size = Widened * sizeof(Vector4);
    }

    return size;
}

// A space's planes copied
void CopySpace(PlaneSet* to, PlaneSet* from)
{
    Vector4* planes = PlaneSetPlanes(from);
    MemoryDeallocate2_(to->planes);
    s32 count = from->count;
    to->count = count;
    u32 size = PlanesCopySize(count);
    to->planes = static_cast<Vector4*>(MemoryAllocate2(size));
    RetailLibc::MemoryCopy(to->planes, planes, size);
}

Vector4 Cross(const Vector4* a, const Vector4* b)
{
    Vector4 cross;
    cross.x = a->y * b->z - a->z * b->y;
    cross.y = a->z * b->x - a->x * b->z;
    cross.z = a->x * b->y - a->y * b->x;
    cross.w = 1.0f;
    return cross;
}

// A cross product long enough made a unit axis with its tag, at the end of the axes: whether it was
bool AddCrossAxis(Vector4* cross, Vector4** out)
{
    if (!(ShortAxis < cross->x * cross->x + cross->y * cross->y + cross->z * cross->z))
    {
        return false;
    }

    f32 inverse = InverseLength(cross, LengthEpsilon);
    cross->x = cross->x * inverse;
    cross->y = cross->y * inverse;
    cross->z = cross->z * inverse;
    **out = *cross;
    TagAxis(*out, EdgeCross);
    (*out)++;
    return true;
}
}

Vector4* PlaneSetPlanes(PlaneSet* set)
{
    return set->planes;
}

u32 PointInsidePlanes(f32 margin, const PlaneSet* set, const Vector4* point, s32* outside)
{
    const Vector4* planes = set->planes;
    s32 hint = *outside;
    if (hint >= 0 && hint < set->count)
    {
        const Vector4* plane = &planes[hint];
        if (margin <= plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w)
        {
            return 0;
        }
    }

    s32 count = set->count;
    for (s32 index = 0; index < count; index++)
    {
        const Vector4* plane = &planes[index];
        if (margin <= plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w)
        {
            *outside = index;
            return 0;
        }
    }

    return 1;
}

HullPlaneCacheEntry* ConstructHullPlaneCacheEntry(HullPlaneCacheEntry* entry)
{
    entry->space.count = 0;
    entry->space.planes = nullptr;
    for (Vector4& vertex : entry->triangle)
    {
        vertex = g_DefaultBox.min;
        vertex.w = 1.0f;
    }

    return entry;
}

HullPlaneCache* ConstructHullPlaneCache(HullPlaneCache* cache)
{
    for (HullPlaneCacheEntry& entry : cache->entries)
    {
        ConstructHullPlaneCacheEntry(&entry);
    }

    cache->next = 0;
    return cache;
}

u32 HullPlaneCacheEntryMatches(const HullPlaneCacheEntry* entry, const CollisionHit* triangle, const CollisionHull* hull)
{
    for (u32 index = 0; index < 3; index++)
    {
        const Vector4* a = &triangle->vertices[index];
        const Vector4* b = &entry->triangle[index];
        if (!(a->x == b->x && a->y == b->y && a->z == b->z))
        {
            return 0;
        }
    }

    return entry->hull == hull;
}

void* FillHullPlaneCacheEntry(HullPlaneCacheEntry* entry, const CollisionHit* triangle, const CollisionHull* hull, PlaneSet* space)
{
    entry->triangle[0] = triangle->vertices[0];
    entry->triangle[1] = triangle->vertices[1];
    entry->triangle[2] = triangle->vertices[2];
    Vector4* planes = PlaneSetPlanes(space);
    MemoryDeallocate2_(entry->space.planes);
    s32 count = space->count;
    entry->space.count = count;
    u32 size = PlanesCopySize(count);
    entry->space.planes = static_cast<Vector4*>(MemoryAllocate2(size));
    void* copied = RetailLibc::MemoryCopy(entry->space.planes, planes, size);
    entry->hull = hull;
    return copied;
}

ContactSet* BeginContacts()
{
    g_ContactsInUse = 1;
    g_Contacts.solid.count = 0;
    g_Contacts.others.count = 0;
    for (Contact& contact : g_Contacts.solid.contacts)
    {
        contact.point = g_DefaultBox.min;
        contact.point.w = 1.0f;
    }

    return &g_Contacts;
}

void EndContacts()
{
    g_ContactsInUse = 0;
}

void AddTriangleContact(ContactSet* set, const CollisionHit* triangle, const CollisionHull* hull, u32 solid)
{
    ContactList* list = solid != 0 ? &set->solid : &set->others;
    HullPlaneCacheEntry* found = nullptr;
    for (HullPlaneCacheEntry& entry : g_HullPlaneCache.entries)
    {
        if (HullPlaneCacheEntryMatches(&entry, triangle, hull) != 0)
        {
            found = &entry;
            break;
        }
    }

    Contact* contact = &list->contacts[list->count];
    if (found != nullptr)
    {
        CopySpace(&contact->space, &found->space);
    }
    else
    {
        MakeTriangleHullSpace(&contact->space, triangle, hull);
        FillHullPlaneCacheEntry(&g_HullPlaneCache.entries[g_HullPlaneCache.next], triangle, hull,
                                &list->contacts[list->count].space);
        g_HullPlaneCache.next++;
        if (g_HullPlaneCache.next >= HullPlaneCache::Entries)
        {
            g_HullPlaneCache.next = 0;
        }
    }

    list->contacts[list->count].kind.value = ContactKind::Triangle;
    list->count++;
}

u32 MakeTriangleHullSpace(PlaneSet* space, const CollisionHit* triangle, const CollisionHull* hull)
{
    constexpr s32 MostAxes = 256;
    Vector4 axes[MostAxes];
    LoadTriangleHullSupport(triangle, HullVertex(hull, 0));
    s32 count = TriangleHullAxes(triangle, hull, nullptr, axes);
    space->count = count * PlanesPerAxis;
    MemoryDeallocate2_(space->planes);
    auto* planes = static_cast<Vector4*>(MemoryAllocate2(count * PlanesPerAxis * sizeof(Vector4)));
    space->count = 0;
    space->planes = planes;
    const Vector4* axis = axes;
    for (s32 left = count; left > 0; left--, axis++)
    {
        f32 range[2];
        TriangleHullSupportRange(axis, range);
        f32 x = axis->x;
        f32 y = axis->y;
        f32 z = axis->z;
        s32 at = space->count;
        planes[at] = {-x, -y, -z, range[0]};
        planes[at + 1] = {x, y, z, -range[1]};
        space->count = at + 2;
    }

    return 1;
}

s32 HullHullAxes(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix,
                 Vector4* axes)
{
    s32 count = 0;
    u32 normalCount = hull->faceNormalCount;
    const Vector4* normals = HullFaceNormals(hull);
    u32 otherNormalCount = other->faceNormalCount;
    const Vector4* otherNormals = HullFaceNormals(other);
    Vector4* out = axes;
    for (u32 left = normalCount; left != 0; left--, normals++, out++)
    {
        count++;
        VuRotateVector(matrix, normals, out);
        TagAxis(out, FirstNormal);
    }

    out = &axes[count];
    for (u32 left = otherNormalCount; left != 0; left--, otherNormals++, out++)
    {
        count++;
        VuRotateVector(otherMatrix, otherNormals, out);
        TagAxis(out, SecondNormal);
    }

    u32 directionCount = hull->edgeDirectionCount;
    const Vector4* directions = HullEdgeDirections(hull);
    u32 otherDirectionCount = other->edgeDirectionCount;
    const Vector4* otherDirections = HullEdgeDirections(other);
    for (u32 index = 0; index < directionCount; index++)
    {
        Vector4 direction;
        VuRotateVector(matrix, &directions[index], &direction);
        out = &axes[count];
        for (u32 otherIndex = 0; otherIndex < otherDirectionCount; otherIndex++)
        {
            Vector4 otherDirection;
            VuRotateVector(otherMatrix, &otherDirections[otherIndex], &otherDirection);
            Vector4 cross = Cross(&direction, &otherDirection);
            if (AddCrossAxis(&cross, &out))
            {
                count++;
            }
        }
    }

    return count;
}

s32 TriangleHullAxes(const CollisionHit* triangle, const CollisionHull* hull, const Matrix4x4* matrix, Vector4* axes)
{
    const Vector4* vertices = triangle->vertices;
    Vector4 edges[3];
    for (u32 index = 0; index < 3; index++)
    {
        const Vector4* from = &vertices[index];
        const Vector4* to = &vertices[index == 2 ? 0 : index + 1];
        edges[index] = *to;
        edges[index].x = edges[index].x - from->x;
        edges[index].y = edges[index].y - from->y;
        edges[index].z = edges[index].z - from->z;
    }

    Vector4 normal = Cross(&edges[0], &edges[1]);
    f32 inverse = InverseLength(&normal, LengthEpsilon);
    normal.x = normal.x * inverse;
    normal.y = normal.y * inverse;
    normal.z = normal.z * inverse;
    axes[0] = normal;
    TagAxis(&axes[0], FirstNormal);
    s32 count = 1;
    u32 normalCount = hull->faceNormalCount;
    const Vector4* normals = HullFaceNormals(hull);
    Vector4* out = &axes[1];
    for (u32 left = normalCount; left != 0; left--, normals++, out++)
    {
        *out = *normals;
        if (matrix != nullptr)
        {
            VuRotateVector(matrix, out, out);
        }

        TagAxis(out, SecondNormal);
        count++;
    }

    u32 directionCount = hull->edgeDirectionCount;
    const Vector4* directions = HullEdgeDirections(hull);
    for (u32 index = 0; index < directionCount; index++)
    {
        Vector4 direction = directions[index];
        if (matrix != nullptr)
        {
            VuRotateVector(matrix, &direction, &direction);
        }

        out = &axes[count];
        for (const Vector4& edge : edges)
        {
            Vector4 cross = Cross(&edge, &direction);
            if (AddCrossAxis(&cross, &out))
            {
                count++;
            }
        }
    }

    return count;
}

u32 GatherBoxTriangleContacts(ContactSet* set, ChunkData* chunk, const Box* box, const CollisionHull* hull)
{
    constexpr s32 MostTriangles = ContactList::MostContacts;
    s32 count = ChunkBoxTriangles(chunk, box, SurfaceFlags::SolidToPlayerProbes, g_GatheredTriangles, MostTriangles);
    set->box = *box;
    u32 full = 0;
    if (count >= MostTriangles)
    {
        full = 1;
        count = MostTriangles;
    }

    AddGatheredTriangles(set, count, hull);
    return full;
}

u32 GatherTriangleContacts(ContactSet* set, ChunkData* chunk, const Vector4* position, Vector4* motion, const CollisionHull* hull)
{
    constexpr s32 MostTriangles = ContactList::MostContacts;
    // The motion cut down a fifth at a time, the box grown by it and a margin
    constexpr s32 Tries = 5;
    constexpr f32 Fifth = Rounded(0.2);
    constexpr f32 Margin = Rounded(0.3);
    Vector4 step = *motion;
    step.x = step.x * Fifth;
    step.y = step.y * Fifth;
    step.z = step.z * Fifth;
    s32 count = 0;
    for (s32 tries = 0; tries < Tries; tries++)
    {
        Box box;
        HullBox(hull, &box);
        box.max.x = box.max.x + position->x;
        f32 share = static_cast<f32>(Tries) - static_cast<f32>(tries);
        box.max.y = box.max.y + position->y;
        box.max.z = box.max.z + position->z;
        box.min.x = box.min.x + position->x;
        box.min.y = box.min.y + position->y;
        box.min.z = box.min.z + position->z;
        Vector4 moved;
        moved.x = step.x * share;
        moved.y = step.y * share;
        moved.z = step.z * share;
        moved.w = 1.0f;
        *motion = moved;
        GrowBoxByVector(&box, motion);
        GrowBox(Margin, &box);
        count = ChunkBoxTriangles(chunk, &box, SurfaceFlags::SolidToPlayerProbes, g_GatheredTriangles, MostTriangles);
        set->box = box;
        if (count < MostTriangles)
        {
            break;
        }
    }

    AddGatheredTriangles(set, count, hull);
    return 0;
}

void GatherInstanceContacts(ContactSet* set, ChunkData* chunk, const Vector4* position, const Vector4* motion, const u32* mask,
                            InstanceContext** skipped, s32 skippedCount, const CollisionHull* hull)
{
    constexpr s32 MostInstances = 0x40;
    constexpr f32 Margin = Rounded(0.1);
    Box box;
    HullBox(hull, &box);
    GrowBoxByVector(&box, motion);
    GrowBox(Margin, &box);
    box.max.x = box.max.x + position->x;
    box.max.y = box.max.y + position->y;
    box.max.z = box.max.z + position->z;
    box.min.x = box.min.x + position->x;
    box.min.y = box.min.y + position->y;
    box.min.z = box.min.z + position->z;
    InstanceContext* results[MostInstances];
    InstanceQuery query;
    query.results = reinterpret_cast<void**>(results);
    query.count = 0;
    query.most = MostInstances;
    query.distance = Infinite;
    // Retail clears bits 0 and 1 of what the stack had (nothing reads the rest)
    query.bits.value = 0;
    // Instances that triggers' signals reach or with their collision active, awake
    query.wantedFlags = ReferencedObjectFlags::ReceivesTriggerSignals | ReferencedObjectFlags::CollisionActive;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    s32 count = QueryChunkInstances(chunk, &box, *mask, &query);
    for (s32 index = 0; index < count; index++)
    {
        InstanceContext* instance = results[index];
        s32 listed = 0;
        if (skippedCount > 0 && instance != skipped[0])
        {
            listed = 1;
            while (listed < skippedCount && instance != skipped[listed])
            {
                listed++;
            }
        }

        if (listed != skippedCount)
        {
            continue;
        }

        if (!instance->flags.collisionActive)
        {
            AddInstanceHullContacts(set, &box, instance, hull, 0);
            continue;
        }

        if (set->solid.count == ContactList::MostContacts)
        {
            return;
        }

        auto* body = static_cast<DynamicBody*>(GetGameNode(&instance->nodes, NodeRigidBody));
        if (body != nullptr && CallVirtual<u32>(body, body->vtable, DynamicBody::IsSphereSlot) == 0)
        {
            body = nullptr;
        }

        if (body == nullptr)
        {
            AddInstanceHullContacts(set, &box, instance, hull, 1);
            continue;
        }

        auto* sphereBody = static_cast<SphereBody*>(body);
        Vector4 sphere = *RowOf(&sphereBody->matrix, 3);
        sphere.w = sphereBody->radius;
        Contact* contact = &set->solid.contacts[set->solid.count];
        contact->space.count = 0;
        contact->sphere = sphere;
        contact->kind.value = ContactKind::Sphere;
        contact->instance = instance;
        contact->hullIndex = 0;
        set->solid.count++;
    }
}

void AddInstanceHullContacts(ContactSet* set, const Box* box, InstanceContext* instance, const CollisionHull* hull, u32 solid)
{
    Matrix4x4 identity;
    InitIdentityMatrix(&identity);
    ObjectCollision* collision = &instance->collision;
    s32 count = GetHullCount(collision);
    ContactList* list = solid != 0 ? &set->solid : &set->others;
    for (s32 index = 0; index < count; index++)
    {
        CollisionHull* instanceHull;
        Matrix4x4 matrix;
        GetInstanceHull(collision, index, &instanceHull, &matrix);
        Box bounds;
        GetHullBoundingBox(instanceHull, &matrix, &bounds);
        if (BoxesOverlap(box, &bounds) == 0)
        {
            continue;
        }

        if (list->count >= ContactList::MostContacts)
        {
            return;
        }

        list->contacts[list->count].instance = instance;
        list->contacts[list->count].hullIndex = index;
        MakeHullHullSpace(&list->contacts[list->count].space, instanceHull, &matrix, hull, &identity);
        list->contacts[list->count].kind.value = ContactKind::Hull;
        list->count++;
    }
}

u32 MakeHullHullSpace(PlaneSet* space, const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other,
                      const Matrix4x4* otherMatrix)
{
    constexpr s32 MostMoved = 32;
    constexpr s32 MostDifferences = 256;
    constexpr s32 MostAxes = 256;
    // As retail's stack has them: the moved vertexes, the differences and the axes, each running into the next when there are
    // more than fit (the padding below writes one past the differences)
    Vector4 scratch[MostMoved + MostDifferences + MostAxes];
    Vector4* moved = scratch;
    Vector4* differences = scratch + MostMoved;
    Vector4* axes = differences + MostDifferences;
    u32 count = hull->vertexCount;
    u32 otherCount = other->vertexCount;
    VertexDifferences request;
    request.matrix = matrix;
    request.vertices = HullVertex(hull, 0);
    request.count = count;
    request.otherMatrix = otherMatrix;
    request.otherVertices = HullVertex(other, 0);
    request.otherCount = otherCount;
    request.differences = differences;
    request.moved = moved;
    s32 pairs = count * otherCount;
    MakeVertexDifferences(&request);
    space->count = 0;
    s32 axisCount = HullHullAxes(hull, matrix, other, otherMatrix, axes);
    // Up to a multiple of four: the last difference is meant to be repeated, but every copy goes to the same entry past them
    s32 extra = ((pairs + 3) & ~3) - pairs;
    if (extra > 0)
    {
        differences[pairs + extra] = differences[pairs - 1];
    }

    pairs += extra;
    space->count = axisCount * PlanesPerAxis;
    MemoryDeallocate2_(space->planes);
    auto* planes = static_cast<Vector4*>(MemoryAllocate2(axisCount * PlanesPerAxis * sizeof(Vector4)));
    space->count = 0;
    space->planes = planes;
    AddAxisPlanes(space, planes, differences, pairs, axes, axisCount);
    return 1;
}

u32 AddAxisPlanes(PlaneSet* space, Vector4* planes, Vector4* differences, s32 count, const Vector4* axes, s32 axisCount)
{
    s32 groups = (count >= 0 ? count : count + 3) >> 2;
    GroupPoints(differences, groups);
    for (s32 left = axisCount; left > 0; left--, axes++)
    {
        if (!(ShortAxis < axes->x * axes->x + axes->y * axes->y + axes->z * axes->z))
        {
            continue;
        }

        Vector4 axis = *axes;
        f32 inverse = InverseLength(&axis, LengthEpsilon);
        axis.x = axis.x * inverse;
        axis.y = axis.y * inverse;
        axis.z = axis.z * inverse;
        f32 range[2];
        GroupsRange(differences, groups, &axis, range);
        planes[space->count] = {-axis.x, -axis.y, -axis.z, range[0]};
        space->count++;
        planes[space->count] = {axis.x, axis.y, axis.z, -range[1]};
        space->count++;
    }

    return 1;
}

u32 PointInsideSolidContact(f32 margin, ContactSet* set, const Vector4* point, u32 mask)
{
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if ((contact->kind.value & mask) != 0)
        {
            continue;
        }

        if (PointInsidePlanes(margin, &contact->space, point, &contact->outside) != 0)
        {
            return 1;
        }
    }

    return 0;
}

u32 PushOutOfSpace(const PlaneSet* space, const Vector4* point, Vector4* push, void*)
{
    constexpr f32 InFront = -Epsilon;
    f32 least = Infinite;
    const Vector4* nearest = nullptr;
    const Vector4* plane = space->planes;
    s32 count = space->count;
    for (s32 index = 0; index < count; index++, plane++)
    {
        f32 distance = plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
        if (InFront < distance)
        {
            nearest = nullptr;
            break;
        }

        distance = -distance;
        if (distance < least)
        {
            least = distance;
            nearest = plane;
        }
    }

    if (nearest == nullptr)
    {
        *push = g_DefaultBox.min;
        push->w = 1.0f;
        return 0;
    }

    push->x = nearest->x * least;
    push->y = nearest->y * least;
    push->z = nearest->z * least;
    push->w = 1.0f;
    return 1;
}

u32 PushOutOfSpaceAgain(const PlaneSet* space, const Vector4* point, Vector4* push)
{
    u8 unused[0x10];
    return PushOutOfSpace(space, point, push, unused);
}

void MarkInstanceContact(ContactSet* set, InstanceContext* instance, s32 after, u32 bits)
{
    s32 count = set->solid.count;
    for (s32 index = 0; index < count; index++)
    {
        const Contact* contact = &set->solid.contacts[index];
        Contact* marked = &set->solid.contacts[index + after];
        if (contact->kind.hull != 0 && contact->instance == instance && index + after < count && marked->instance == instance)
        {
            marked->kind.value |= bits;
            return;
        }

        count = set->solid.count;
    }
}

s32 MarkSphereContactBelow(ContactSet* set)
{
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if (contact->kind.sphere != 0 && contact->spherePush.y < 0.0f)
        {
            contact->kind.marked = 1;
            return index;
        }
    }

    return -1;
}

void ClearContactMarks(ContactSet* set)
{
    for (s32 index = 0; index < set->solid.count; index++)
    {
        set->solid.contacts[index].kind.value &= ContactKind::KeptBits;
    }
}

void Depenetrate(ContactSet* set, const Vector4* position, Vector4* push, u32 hulls, u32 triangles, u32 pushBodies, f32* pushed)
{
    if (pushed != nullptr)
    {
        *pushed = 0.0f;
    }

    *push = g_DefaultBox.min;
    push->w = 1.0f;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        ContactKind kind = contact->kind;
        if (kind.sphere != 0)
        {
            continue;
        }

        if (!((hulls != 0 && kind.hull != 0) || (triangles != 0 && kind.triangle != 0)))
        {
            continue;
        }

        Vector4 at;
        at.x = position->x + push->x;
        at.y = position->y + push->y;
        at.z = position->z + push->z;
        at.w = 1.0f;
        if (PointInsidePlanes(0.0f, &contact->space, position, &contact->outside) == 0)
        {
            continue;
        }

        kind = contact->kind;
        if (kind.noPush != 0)
        {
            kind.pushRefused = 1;
            contact->kind = kind;
            continue;
        }

        Vector4 step;
        u8 unused[0x10];
        if ((PushOutOfSpace(&contact->space, &at, &step, unused) & 0xFF) == 0)
        {
            continue;
        }

        if (pushBodies != 0)
        {
            contact->kind.pushedOut = 1;
            DynamicBody* body = nullptr;
            if (contact->kind.hull != 0)
            {
                body = static_cast<DynamicBody*>(GetGameNode(&contact->instance->nodes, NodeRigidBody));
                if (body != nullptr && CallVirtual<u32>(body, body->vtable, DynamicBody::IsSphereSlot) != 0)
                {
                    body = nullptr;
                }
            }

            if (body != nullptr)
            {
                Vector4 half;
                half.x = step.x * -0.5f;
                half.y = step.y * -0.5f;
                half.z = step.z * -0.5f;
                half.w = 1.0f;
                body->MoveBy(&half);
                half.x = step.x * 0.5f;
                half.y = step.y * 0.5f;
                half.z = step.z * 0.5f;
                half.w = 1.0f;
                contact->kind.pushShared = 1;
                contact->point.x = contact->point.x + half.x;
                contact->point.y = contact->point.y + half.y;
                contact->point.z = contact->point.z + half.z;
            }
        }

        push->x = push->x + step.x;
        push->y = push->y + step.y;
        push->z = push->z + step.z;
        if (pushed != nullptr)
        {
            *pushed = *pushed + __builtin_sqrtf(step.x * step.x + step.y * step.y + step.z * step.z);
        }
    }
}

void PushOutOfSpheres(f32 height, f32 halfWidth, ContactSet* set, const Vector4* position, const Vector4* offset, Vector4* push)
{
    *push = g_DefaultBox.min;
    push->w = 1.0f;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        ContactKind kind = contact->kind;
        if (kind.hull == 0 || kind.sphere == 0)
        {
            continue;
        }

        Box box;
        box.min = {-halfWidth, 0.0f, -halfWidth, 1.0f};
        box.max = {halfWidth, height, halfWidth, 1.0f};
        Vector4 centre = contact->sphere;
        centre.x = centre.x - (position->x + offset->x);
        centre.y = centre.y - (position->y + offset->y);
        centre.z = centre.z - (position->z + offset->z);
        Vector4 out;
        if (SphereBoxPush(contact->sphere.w, &box, &centre, &out) == 0)
        {
            contact->spherePush = g_DefaultBox.min;
            contact->spherePush.w = 1.0f;
            continue;
        }

        Vector4 half;
        half.x = out.x * 0.5f;
        half.y = out.y * 0.5f;
        half.z = out.z * 0.5f;
        half.w = 1.0f;
        contact->kind.pushShared = 1;
        contact->kind.touched = 1;
        contact->spherePush = half;
        push->x = push->x - half.x;
        push->y = push->y - half.y;
        push->z = push->z - half.z;
        contact->point.x = contact->point.x - half.x;
        contact->point.y = contact->point.y - half.y;
        contact->point.z = contact->point.z - half.z;
    }
}

u32 GroundAhead(ContactSet* set, const MovingPoint* point)
{
    constexpr u32 NotSpheres = ContactKind::SphereBit;
    constexpr f32 Below = Rounded(0.8);
    // Where half of its velocity takes it, then 1.6 times it
    constexpr f32 Near = 0.5f;
    constexpr f32 Far = Rounded(1.6);
    Vector4 ahead = point->position;
    ahead.x = ahead.x + point->velocity.x * Near;
    ahead.y = (ahead.y + point->velocity.y * Near) - Below;
    ahead.z = ahead.z + point->velocity.z * Near;
    if (PointInsideSolidContact(0.0f, set, &ahead, NotSpheres) != 0)
    {
        return 1;
    }

    ahead = point->position;
    ahead.x = ahead.x + point->velocity.x * Far;
    ahead.y = (ahead.y + point->velocity.y * Far) - Below;
    ahead.z = ahead.z + point->velocity.z * Far;
    return PointInsideSolidContact(0.0f, set, &ahead, NotSpheres) != 0;
}

s32 CollectCrossedPlanes(ContactSet* set, const Vector4* from, const Vector4* motion, const Vector4** planes, s32* contacts,
                         s32 most, s32* lastContact)
{
    constexpr u32 Skipped = ContactKind::SphereBit | ContactKind::NoPush | ContactKind::PushedOut;
    constexpr f32 Near = Rounded(0.0001);
    constexpr f32 Same = Rounded(0.001);
    s32 count = 0;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if ((contact->kind.value & Skipped) != 0)
        {
            continue;
        }

        Vector4 to;
        to.x = from->x + motion->x;
        to.y = from->y + motion->y;
        to.z = from->z + motion->z;
        to.w = 1.0f;
        if ((PointInsidePlanes(0.0f, &contact->space, &to, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        s32 planeCount = contact->space.count;
        const Vector4* plane = contact->space.planes;
        for (s32 planeIndex = 0; planeIndex < planeCount; planeIndex++, plane++)
        {
            to.x = from->x + motion->x;
            to.y = from->y + motion->y;
            to.z = from->z + motion->z;
            f32 atFrom = plane->x * from->x + plane->y * from->y + plane->z * from->z + plane->w;
            f32 atTo = plane->x * to.x + plane->y * to.y + plane->z * to.z + plane->w;
            if (!(-Near < atFrom) || !(atTo < Near))
            {
                continue;
            }

            s32 known = 0;
            for (; known < count; known++)
            {
                const Vector4* other = planes[known];
                f32 x = plane->x - other->x;
                f32 y = plane->y - other->y;
                f32 z = plane->z - other->z;
                f32 w = plane->w - other->w;
                if (x * x + y * y + z * z + w * w < Same)
                {
                    break;
                }
            }

            if (known != count)
            {
                continue;
            }

            planes[count] = plane;
            contacts[count] = index;
            count++;
            *lastContact = index;
            if (count == most)
            {
                return count;
            }
        }
    }

    return count;
}

void TouchContact(ContactSet* set, s32 index, const Vector4* motion, const Vector4* from, const Vector4* to)
{
    constexpr f32 Share = 20.0f;
    constexpr f32 Margin = -Rounded(0.01);
    // Stood on when the end a bit above it is inside it
    constexpr f32 Above = Rounded(0.05);
    Contact* contact = &set->solid.contacts[index];
    Vector4 moved;
    moved.x = to->x - from->x;
    moved.y = to->y - from->y;
    moved.z = to->z - from->z;
    moved.w = 1.0f;
    ContactKind kind = contact->kind;
    if (kind.hull != 0 && kind.sphere == 0)
    {
        contact->point.x = contact->point.x + (moved.x - motion->x) * Share;
        contact->point.y = contact->point.y + (moved.y - motion->y) * Share;
        contact->point.z = contact->point.z + (moved.z - motion->z) * Share;
        contact->kind.pushShared = 1;
    }

    bool stood = false;
    if (0.0f < motion->y)
    {
        Vector4 above;
        above.x = to->x + 0.0f;
        above.y = to->y + Above;
        above.z = to->z + 0.0f;
        above.w = 1.0f;
        stood = (PointInsidePlanes(Margin, &contact->space, &above, &contact->outside) & 0xFF) != 0;
    }

    contact = &set->solid.contacts[index];
    if (stood)
    {
        contact->kind.stoodOn = 1;
    }
    else
    {
        contact->kind.touched = 1;
    }
}

u32 SlideStep(ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* to, u32 limitClimb)
{
    constexpr s32 MostPlanes = 4;
    constexpr f32 Nudge = Rounded(2e-5);
    constexpr f32 NoSlide = Infinite;
    constexpr f32 ShortSlide = 0x1.0c6f7cp-20f;
    // The contacts a slide may end inside of: spheres, those not to be pushed out of or pushed out of, the hull it rides
    constexpr u32 MayEndInside = ContactKind::SphereBit | ContactKind::NoPush | ContactKind::PushedOut | ContactKind::Ridden;
    const Vector4* planes[MostPlanes];
    s32 contacts[MostPlanes];
    s32 best[2] = {g_NoContacts[0], g_NoContacts[1]};
    s32 lastContact = -1;
    s32 count = CollectCrossedPlanes(set, from, motion, planes, contacts, MostPlanes, &lastContact);
    if (count == 0)
    {
        Vector4 end;
        end.x = from->x + motion->x;
        end.y = from->y + motion->y;
        end.z = from->z + motion->z;
        end.w = 1.0f;
        *to = end;
        return 1;
    }

    // Along one plane
    f32 nearest = NoSlide;
    Vector4 bestEnd;
    for (s32 index = 0; index < count; index++)
    {
        set->solid.contacts[contacts[index]].kind.touched = 1;
        const Vector4* plane = planes[index];
        Vector4 normal = {plane->x, plane->y, plane->z, 1.0f};
        f32 into = motion->x * normal.x + motion->y * normal.y + motion->z * normal.z;
        Vector4 slide = *motion;
        slide.x = slide.x - normal.x * into;
        slide.y = slide.y - normal.y * into;
        slide.z = slide.z - normal.z * into;
        slide.x = slide.x + normal.x * Nudge;
        slide.z = slide.z + normal.z * Nudge;
        slide.y = slide.y + normal.y * Nudge;
        if (limitClimb != 0 && __builtin_sqrtf(slide.x * slide.x + slide.z * slide.z) < slide.y)
        {
            continue;
        }

        *to = *from;
        to->x = to->x + slide.x;
        to->y = to->y + slide.y;
        to->z = to->z + slide.z;
        if (PointInsideSolidContact(0.0f, set, to, MayEndInside) != 0)
        {
            continue;
        }

        f32 x = to->x - slide.x;
        f32 y = to->y - slide.y;
        f32 z = to->z - slide.z;
        f32 distance = __builtin_sqrtf(x * x + y * y + z * z);
        if (distance < nearest)
        {
            nearest = distance;
            bestEnd = *to;
            best[0] = contacts[index];
        }
    }

    if (nearest != NoSlide)
    {
        *to = bestEnd;
        TouchContact(set, best[0], motion, from, &bestEnd);
        return 1;
    }

    // Along the crease of two planes
    nearest = NoSlide;
    for (s32 index = 0; index < count; index++)
    {
        const Vector4* plane = planes[index];
        Vector4 normal = {plane->x, plane->y, plane->z, 1.0f};
        f32 into = motion->x * normal.x + motion->y * normal.y + motion->z * normal.z;
        Vector4 slide = *motion;
        slide.x = slide.x - normal.x * into;
        slide.z = slide.z - normal.z * into;
        slide.y = slide.y - normal.y * into;
        slide.x = slide.x + normal.x * Nudge;
        slide.y = slide.y + normal.y * Nudge;
        slide.z = slide.z + normal.z * Nudge;
        const Vector4* crossed[MostPlanes];
        s32 crossedContacts[MostPlanes];
        s32 crossedCount = CollectCrossedPlanes(set, from, &slide, crossed, crossedContacts, MostPlanes, &lastContact);
        for (s32 other = 0; other < crossedCount; other++)
        {
            set->solid.contacts[crossedContacts[other]].kind.touched = 1;
            const Vector4* otherPlane = crossed[other];
            Vector4 otherNormal = {otherPlane->x, otherPlane->y, otherPlane->z, 1.0f};
            Vector4 crease;
            crease.x = normal.y * otherNormal.z - normal.z * otherNormal.y;
            crease.y = normal.z * otherNormal.x - normal.x * otherNormal.z;
            crease.z = normal.x * otherNormal.y - normal.y * otherNormal.x;
            crease.w = 1.0f;
            f32 along = crease.x * motion->x + crease.y * motion->y + crease.z * motion->z;
            Vector4 creaseSlide;
            creaseSlide.x = crease.x * along;
            creaseSlide.y = crease.y * along;
            creaseSlide.z = crease.z * along;
            creaseSlide.w = 1.0f;
            Vector4 away;
            away.x = normal.x + otherNormal.x;
            away.y = normal.y + otherNormal.y;
            away.z = normal.z + otherNormal.z;
            creaseSlide.x = creaseSlide.x + away.x * Nudge;
            creaseSlide.y = creaseSlide.y + away.y * Nudge;
            creaseSlide.z = creaseSlide.z + away.z * Nudge;
            f32 xx = creaseSlide.x * creaseSlide.x;
            f32 zz = creaseSlide.z * creaseSlide.z;
            if (xx + creaseSlide.y * creaseSlide.y + zz < ShortSlide)
            {
                continue;
            }

            if (limitClimb != 0 && __builtin_sqrtf(xx + zz) < creaseSlide.y)
            {
                continue;
            }

            *to = *from;
            to->x = to->x + creaseSlide.x;
            to->y = to->y + creaseSlide.y;
            to->z = to->z + creaseSlide.z;
            if (PointInsideSolidContact(0.0f, set, to, MayEndInside) != 0)
            {
                continue;
            }

            f32 x = to->x - creaseSlide.x;
            f32 y = to->y - creaseSlide.y;
            f32 z = to->z - creaseSlide.z;
            f32 distance = __builtin_sqrtf(x * x + y * y + z * z);
            if (distance < nearest)
            {
                nearest = distance;
                bestEnd = *to;
                best[0] = contacts[index];
                best[1] = crossedContacts[other];
            }
        }
    }

    if (nearest == NoSlide)
    {
        *to = *from;
        return 0;
    }

    *to = bestEnd;
    TouchContact(set, best[0], motion, from, &bestEnd);
    TouchContact(set, best[1], motion, from, &bestEnd);
    return 1;
}

s32 CountContactsContaining(f32 margin, ContactSet* set, const Vector4* point, u32 mask)
{
    s32 count = 0;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if ((contact->kind.value & mask) != 0)
        {
            continue;
        }

        if (PointInsidePlanes(margin, &contact->space, point, &contact->outside) != 0)
        {
            count++;
        }
    }

    return count;
}

s32 FindGround(ContactSet* set, const Vector4* point, Vector4* normal)
{
    constexpr u32 Skipped = ContactKind::SphereBit | ContactKind::NoPush;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if ((contact->kind.value & Skipped) != 0)
        {
            continue;
        }

        if ((PointInsidePlanes(0.0f, &contact->space, point, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        Vector4 push;
        u8 unused[0x10];
        PushOutOfSpace(&contact->space, point, &push, unused);
        f32 aside = __builtin_sqrtf(push.x * push.x + push.z * push.z);
        if (!(aside == 0.0f) && !(aside < push.y))
        {
            continue;
        }

        contact->kind.ground = 1;
        contact->kind.marked = 1;
        if (normal != nullptr)
        {
            f32 inverse = InverseLength(&push, LengthEpsilon);
            push.x = push.x * inverse;
            push.y = push.y * inverse;
            push.z = push.z * inverse;
            *normal = push;
        }

        return index;
    }

    return -1;
}

s32 ProbeAlong(ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* lastOutside, Vector4* normal)
{
    constexpr u32 Skipped = ContactKind::SphereBit | ContactKind::NoPush;
    constexpr f32 Smallest = Rounded(0.0001);
    constexpr f32 Upward = Rounded(0.707);
    // Below every plane's y
    constexpr f32 Lowest = -1e10f;
    Vector4 step = *motion;
    Vector4 outside = *from;
    Vector4 inside = *from;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if ((contact->kind.value & Skipped) != 0 ||
            (PointInsidePlanes(0.0f, &contact->space, from, &contact->outside) & 0xFF) != 0)
        {
            contact->kind.probeStart = 1;
        }
    }

    s32 found = -1;
    while (Smallest < __builtin_sqrtf(step.x * step.x + step.y * step.y + step.z * step.z))
    {
        Vector4 probe = outside;
        probe.x = probe.x + step.x;
        probe.y = probe.y + step.y;
        probe.z = probe.z + step.z;
        s32 count = CountContactsContaining(0.0f, set, &probe, ContactKind::ProbeStart);
        if (count > 0)
        {
            found = count;
            inside = probe;
        }
        else
        {
            outside = probe;
        }

        step.x = step.x * 0.5f;
        step.y = step.y * 0.5f;
        step.z = step.z * 0.5f;
    }

    if (found == -1)
    {
        return NoProbeHit;
    }

    s32 result = -1;
    f32 highest = Lowest;
    if (lastOutside != nullptr)
    {
        *lastOutside = outside;
    }

    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if (contact->kind.probeStart != 0)
        {
            continue;
        }

        if ((PointInsidePlanes(0.0f, &contact->space, &inside, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        s32 upward = 0;
        const Vector4* plane = contact->space.planes;
        for (s32 left = contact->space.count; left > 0; left--, plane++)
        {
            if (!(0.0f <= plane->x * outside.x + plane->y * outside.y + plane->z * outside.z + plane->w))
            {
                continue;
            }

            if (Upward < plane->y)
            {
                upward++;
            }

            if (Upward <= plane->y && highest < plane->y)
            {
                highest = plane->y;
                result = index;
                if (normal != nullptr)
                {
                    normal->x = plane->x;
                    normal->y = plane->y;
                    normal->z = plane->z;
                    normal->w = 1.0f;
                }
            }
        }

        if (upward != 0)
        {
            contact->kind.ground = 1;
            contact->kind.marked = 1;
        }
    }

    return result;
}

s32 Move(f32 frameTime, ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* end, Vector4* groundNormal,
         u32 limitClimb)
{
    constexpr s32 MostSteps = 6;
    constexpr f32 Moving = Rounded(2.5e-5);
    constexpr f32 Stuck = Rounded(0.001);
    constexpr f32 Rising = Rounded(0.001);
    constexpr f32 Short = Rounded(0.05);
    // The longest step at a frame's time, and the least the ground is probed for
    constexpr f32 StepLength = Rounded(0.05);
    constexpr f32 LeastReach = Rounded(0.1);
    constexpr s32 Directions = 9;
    f32 maxStep = StepLength / frameTime;
    Vector4 position = *from;
    Vector4 step = *motion;
    s32 stuck = 0;
    f32 length = __builtin_sqrtf(motion->x * motion->x + motion->y * motion->y + motion->z * motion->z);
    s32 steps = static_cast<s32>(static_cast<u32>(static_cast<s32>(length / maxStep)) + 1u);
    if (!(steps < MostSteps + 1))
    {
        steps = MostSteps;
    }

    f32 stepLength = length / static_cast<f32>(steps);
    f32 inverse = InverseLength(&step, LengthEpsilon);
    f32 squared = motion->x * motion->x + motion->y * motion->y + motion->z * motion->z;
    step.x = step.x * inverse * stepLength;
    step.y = step.y * inverse * stepLength;
    step.z = step.z * inverse * stepLength;
    if (Moving < squared)
    {
        s32 index = 0;
        do
        {
            Vector4 next;
            SlideStep(set, &position, &step, &next, limitClimb);
            f32 x = next.x - position.x;
            f32 y = next.y - position.y;
            f32 z = next.z - position.z;
            index++;
            if (__builtin_sqrtf(x * x + y * y + z * z) < Stuck)
            {
                stuck = 1;
                break;
            }

            position = next;
            if (index == steps)
            {
                *end = next;
            }
        } while (index != steps);
    }
    else
    {
        *end = *from;
        position = *from;
    }

    Vector4 normal;
    s32 ground = FindGround(set, &position, &normal);
    if (ground != -1)
    {
        *groundNormal = normal;
    }
    else
    {
        f32 aside = __builtin_sqrtf(motion->x * motion->x + motion->z * motion->z);
        f32 reach = LeastReach;
        if (LeastReach < aside)
        {
            reach = aside;
        }

        if (reach < stepLength)
        {
            reach = stepLength;
        }

        f32 down = -reach;
        Vector4 probe = {0.0f, down, 0.0f, 1.0f};
        Vector4 lastOutside;
        Vector4 landing;
        s32 probed = 0;
        s32 found = ProbeAlong(set, &position, &probe, &lastOutside, &normal);
        if (found != NoProbeHit && found != -1)
        {
            ground = found;
            probed = 1;
            landing = lastOutside;
            *groundNormal = normal;
        }

        if (ground == -1)
        {
            for (s32 index = 0; index < Directions; index++)
            {
                const f32 around[3] = {reach, 0.0f, down};
                Vector4 direction = {around[index % 3], down, around[index / 3], 1.0f};
                f32 scale = InverseLength(&direction, LengthEpsilon);
                direction.x = direction.x * scale * reach;
                direction.y = direction.y * scale * reach;
                direction.z = direction.z * scale * reach;
                found = ProbeAlong(set, &position, &direction, &lastOutside, &normal);
                if (found >= 0)
                {
                    ground = found;
                    probed = 1;
                    landing = lastOutside;
                    *groundNormal = normal;
                    break;
                }
            }
        }

        if (motion->y < Rising && probed != 0)
        {
            f32 x = landing.x - position.x;
            f32 y = landing.y - position.y;
            f32 z = landing.z - position.z;
            f32 distance = __builtin_sqrtf(x * x + y * y + z * z);
            if (Short < distance)
            {
                Vector4 toLanding = {x, y, z, 1.0f};
                f32 scale = InverseLength(&toLanding, LengthEpsilon);
                f32 travel = distance - Short;
                position.x = position.x + toLanding.x * scale * travel;
                position.y = position.y + toLanding.y * scale * travel;
                position.z = position.z + toLanding.z * scale * travel;
            }
        }
    }

    *end = position;
    if (ground != -1)
    {
        return ground;
    }

    s32 sphere = MarkSphereContactBelow(set);
    if (sphere != -1)
    {
        groundNormal->x = 0.0f;
        groundNormal->w = 1.0f;
        groundNormal->y = 1.0f;
        groundNormal->z = 0.0f;
        return sphere;
    }

    if (stuck != 0 && motion->y < 0.0f)
    {
        return StuckFalling;
    }

    return -1;
}

s32 MoveCharacter(f32 frameTime, ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* end, Vector4* groundNormal,
                  u32 limitClimb)
{
    // A small fall tried when it's stuck
    constexpr f32 Fall = -0.125f;
    s32 result = Move(frameTime, set, from, motion, end, groundNormal, limitClimb);
    if (result != StuckFalling)
    {
        return result;
    }

    Vector4 fall = {0.0f, Fall, 0.0f, 1.0f};
    result = Move(frameTime, set, from, &fall, end, groundNormal, limitClimb);
    if (result != StuckFalling)
    {
        return result;
    }

    Vector4 below = *from;
    below.x = below.x + fall.x;
    below.y = below.y + fall.y;
    below.z = below.z + fall.z;
    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        if (contact->kind.sphere != 0)
        {
            continue;
        }

        if ((PointInsidePlanes(0.0f, &contact->space, &below, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        if (contact->kind.hull != 0)
        {
            contact->kind.ground = 1;
            contact->kind.marked = 1;
        }

        groundNormal->x = 0.0f;
        groundNormal->y = 1.0f;
        groundNormal->z = 0.0f;
        groundNormal->w = 1.0f;
        result = index;
    }

    return result != StuckFalling ? result : 0;
}

s32 ClipSegmentToSpace(const PlaneSet* space, const Vector4* start, const Vector4* end, Vector4* entry, Vector4* plane)
{
    constexpr f32 OnPlane = Epsilon;
    constexpr f32 Outside = Rounded(1e-5);
    Vector4 from = *start;
    Vector4 to = *end;
    const Vector4* planes = space->planes;
    s32 index = 0;
    while (index < space->count && PlaneSide(OnPlane, &planes[index], &from) != InFrontOfPlane)
    {
        index++;
    }

    if (index == space->count)
    {
        return SegmentStartsInside;
    }

    for (index = 0; index < space->count; index++)
    {
        const Vector4* clip = &planes[index];
        bool fromOutside = Outside < clip->x * from.x + clip->y * from.y + clip->z * from.z + clip->w;
        bool toOutside = Outside < clip->x * to.x + clip->y * to.y + clip->z * to.z + clip->w;
        Vector4 crossing;
        if (fromOutside)
        {
            if (toOutside)
            {
                return SegmentOutside;
            }

            RayPlaneIntersection(clip, &from, &to, &crossing);
            *plane = *clip;
            from = crossing;
        }
        else if (toOutside)
        {
            RayPlaneIntersection(clip, &from, &to, &crossing);
            to = crossing;
        }
    }

    *entry = from;
    return SegmentEnters;
}

f32 CastDown(f32 above, f32 below, ContactSet* set, const Vector4* point, Vector4* hit, Vector4* normal, CollisionHit** triangle,
             InstanceContext** instance)
{
    f32 highest = -Infinite;
    f32 drop = Infinite;
    if (triangle != nullptr)
    {
        *triangle = nullptr;
    }

    if (instance != nullptr)
    {
        *instance = nullptr;
    }

    for (s32 index = 0; index < set->solid.count; index++)
    {
        Contact* contact = &set->solid.contacts[index];
        Vector4 top = *point;
        Vector4 bottom = *point;
        top.y = top.y + above;
        bottom.y = bottom.y - below;
        ContactKind kind = contact->kind;
        if (kind.sphere != 0)
        {
            if (contact->spherePush.y < 0.0f)
            {
                if (triangle != nullptr)
                {
                    *triangle = nullptr;
                }

                if (instance != nullptr)
                {
                    *instance = nullptr;
                }

                return 0.0f;
            }

            continue;
        }

        if (kind.noPush != 0)
        {
            continue;
        }

        if ((PointInsidePlanes(0.0f, &contact->space, &bottom, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        Vector4 entry;
        Vector4 plane;
        if (ClipSegmentToSpace(&contact->space, &top, &bottom, &entry, &plane) != SegmentEnters || !(highest < entry.y))
        {
            continue;
        }

        highest = entry.y;
        if (contact->kind.hull != 0)
        {
            if (instance != nullptr)
            {
                *instance = contact->instance;
            }

            if (triangle != nullptr)
            {
                *triangle = nullptr;
            }
        }
        else
        {
            if (triangle != nullptr)
            {
                *triangle = &set->solid.triangles[index];
            }

            if (instance != nullptr)
            {
                *instance = nullptr;
            }
        }

        if (hit != nullptr)
        {
            *hit = entry;
        }

        drop = point->y - entry.y;
        if (normal != nullptr)
        {
            *normal = plane;
        }
    }

    return drop;
}

u32 StepUp(ContactSet* set, const Vector4* position, const Vector4* velocity, Vector4* outVelocity, u32 allowed)
{
    constexpr f32 Small = Rounded(0.0001);
    constexpr f32 Moving = Rounded(0.01);
    constexpr f32 MostLift = Rounded(0.2);
    constexpr f32 Turned = Rounded(0.866);
    constexpr f32 Rising = Rounded(0.001);
    // Lifted by its flat speed times this, and lowered a fifth of its speed at a time
    constexpr f32 LiftPerSpeed = 3.0f;
    constexpr f32 LowerShare = Rounded(0.2);
    Vector4 flat = *velocity;
    flat.y = 0.0f;
    f32 speed = __builtin_sqrtf(velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z);
    Vector4 way = flat;
    f32 wayLength = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
    f32 outSpeed =
        __builtin_sqrtf(outVelocity->x * outVelocity->x + outVelocity->y * outVelocity->y + outVelocity->z * outVelocity->z);
    Vector4 outFlat = *outVelocity;
    outFlat.y = 0.0f;
    if (Small < wayLength)
    {
        f32 inverse = InverseLength(&way, LengthEpsilon);
        way.x = way.x * inverse;
        way.y = way.y * inverse;
        way.z = way.z * inverse;
    }

    Vector4 outWay = outFlat;
    if (Small < __builtin_sqrtf(outWay.x * outWay.x + outWay.y * outWay.y + outWay.z * outWay.z))
    {
        f32 inverse = InverseLength(&outWay, LengthEpsilon);
        outWay.x = outWay.x * inverse;
        outWay.y = outWay.y * inverse;
        outWay.z = outWay.z * inverse;
    }

    if (allowed == 0)
    {
        return 0;
    }

    if (!(Moving < __builtin_sqrtf(velocity->x * velocity->x + velocity->z * velocity->z)) || !(outSpeed < MostLift) ||
        !(outSpeed < speed) || !(outWay.x * way.x + outWay.y * way.y + outWay.z * way.z < Turned))
    {
        return 0;
    }

    Vector4 lost;
    lost.x = flat.x - outFlat.x;
    lost.y = flat.y - outFlat.y;
    lost.z = flat.z - outFlat.z;
    lost.w = 1.0f;
    Vector4 step = {velocity->x, 0.0f, velocity->z, 1.0f};
    f32 lostLength = __builtin_sqrtf(lost.x * lost.x + lost.y * lost.y + lost.z * lost.z);
    f32 inverse = InverseLength(&step, LengthEpsilon);
    f32 lift = __builtin_sqrtf(velocity->x * velocity->x + velocity->z * velocity->z) * LiftPerSpeed;
    step.x = step.x * inverse * lostLength;
    step.z = step.z * inverse * lostLength;
    step.y = MostLift < lift ? MostLift : lift;
    Vector4 probe = *position;
    probe.x = probe.x + step.x;
    probe.y = probe.y + step.y;
    probe.z = probe.z + step.z;
    if (PointInsideSolidContact(0.0f, set, &probe, ContactKind::SphereBit) != 0)
    {
        return 0;
    }

    Vector4 groundNormal = lost;
    f32 drop = CastDown(0.0f, __builtin_fabsf(step.y), set, &probe, nullptr, &groundNormal, nullptr, nullptr);
    if (!(drop == Infinite) && !(Turned < groundNormal.y))
    {
        return 0;
    }

    *outVelocity = step;
    if (!(velocity->y < Rising) || !(Rising < speed))
    {
        return 1;
    }

    // Down as far as it's free, a fifth of the speed at a time
    f32 lower = speed;
    while (true)
    {
        Vector4 test;
        test.x = position->x + outVelocity->x;
        test.y = position->y + outVelocity->y;
        test.z = position->z + outVelocity->z;
        test.w = 1.0f;
        test.y = test.y - lower;
        if (PointInsideSolidContact(0.0f, set, &test, ContactKind::SphereBit | ContactKind::NoPush) == 0)
        {
            outVelocity->y = outVelocity->y - lower;
            return 1;
        }

        lower = lower - speed * LowerShare;
        if (!(Rising < lower))
        {
            return 1;
        }
    }
}

ContactSet* ConstructContactSet(ContactSet* set)
{
    for (Contact& contact : set->solid.contacts)
    {
        contact.space.count = 0;
        contact.space.planes = nullptr;
    }

    for (Contact& contact : set->others.contacts)
    {
        contact.space.count = 0;
        contact.space.planes = nullptr;
    }

    ClearContacts(set);
    return set;
}

void ClearContacts(ContactSet* set)
{
    set->solid.count = 0;
    set->others.count = 0;
    for (Contact& contact : set->solid.contacts)
    {
        contact.point = g_DefaultBox.min;
        contact.point.w = 1.0f;
    }
}

void InitPhysicsStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    ConstructContactSet(&g_Contacts);
}

void ConstructPhysicsModule()
{
    InitPhysicsStatics(1, DefaultInitPriority);
}

namespace
{
// As retail's stacks have them: the moved vertexes, the differences and the axes, each running into the next when there are more
// than fit
struct ContactScratch
{
    static constexpr s32 MostMoved = 32;
    static constexpr s32 MostDifferences = 256;
    static constexpr s32 MostAxes = 256;
    // The axes' padding to a multiple of four
    static constexpr s32 AxisPadding = 4;
    Vector4 all[MostMoved + MostDifferences + MostAxes + AxisPadding];

    Vector4* Moved()
    {
        return all;
    }

    Vector4* Differences()
    {
        return all + MostMoved;
    }

    Vector4* Axes()
    {
        return all + MostMoved + MostDifferences;
    }
};

// The axes made up to a multiple of four with copies of the last (nothing reads them)
void PadAxes(Vector4* axes, s32 count)
{
    s32 extra = count & 3;
    if (extra == 0)
    {
        return;
    }

    extra = 4 - extra;
    for (s32 index = 0; index < extra; index++)
    {
        axes[count + index] = axes[count - 1];
    }
}

// The differences' range along every axis (sixteen groups' at once when said): apart along one (false), else the push out of the
// least overlap (the axis times it), the way out (the axis, turned around when it's the overlap's low end; its w the overlap)
// and which axis
bool LeastOverlap(const Vector4* differences, s32 groups, bool sixteenGroups, const Vector4* axes, s32 axisCount, Vector4* push,
                  Vector4* way, s32* least)
{
    f32 deepest = -Infinite;
    for (s32 index = 0; index < axisCount; index++)
    {
        const Vector4* axis = &axes[index];
        f32 range[2];
        if (sixteenGroups)
        {
            SixteenGroupsRange(differences, axis, range);
        }
        else
        {
            GroupsRange(differences, groups, axis, range);
        }

        if (0.0f < range[0] || range[1] < 0.0f)
        {
            return false;
        }

        if (deepest < range[0])
        {
            deepest = range[0];
            *least = index;
            *push = *axis;
            push->x = push->x * deepest;
            push->y = push->y * deepest;
            push->z = push->z * deepest;
            way->x = -axis->x;
            way->y = -axis->y;
            way->z = -axis->z;
            way->w = deepest;
        }

        f32 high = range[1];
        if (deepest < -high)
        {
            deepest = -high;
            *least = index;
            *push = *axis;
            push->x = push->x * high;
            push->y = push->y * high;
            push->z = push->z * high;
            way->x = axis->x;
            way->y = axis->y;
            way->z = axis->z;
            way->w = deepest;
        }
    }

    return true;
}

// A matrix moved by a push
Matrix4x4 MovedBy(const Matrix4x4* matrix, const Vector4* push)
{
    Matrix4x4 moved = *matrix;
    Vector4* position = reinterpret_cast<Vector4*>(moved.m[3]);
    position->x = position->x + push->x;
    position->y = position->y + push->y;
    position->z = position->z + push->z;
    return moved;
}

Vector4 Reversed(const Vector4* vector)
{
    return {-vector->x, -vector->y, -vector->z, 1.0f};
}

u32 AxisTagOf(const Vector4* axis)
{
    return *reinterpret_cast<const u32*>(&axis->w);
}
}

u32 HullsContact(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix,
                 Vector4* push, Vector4* point)
{
    constexpr s32 SixteenGroups = 64;
    ContactScratch scratch;
    Vector4* differences = scratch.Differences();
    Vector4* axes = scratch.Axes();
    u32 otherCount = other->vertexCount;
    u32 count = hull->vertexCount;
    VertexDifferences request;
    request.matrix = matrix;
    request.vertices = HullVertex(hull, 0);
    request.count = count;
    request.otherMatrix = otherMatrix;
    request.otherVertices = HullVertex(other, 0);
    request.otherCount = otherCount;
    request.differences = differences;
    request.moved = scratch.Moved();
    s32 pairs = count * otherCount;
    MakeVertexDifferences(&request);
    s32 axisCount = HullHullAxes(hull, matrix, other, otherMatrix, axes);
    PadAxes(axes, axisCount);
    s32 groups = pairs / 4;
    GroupPoints(differences, groups);
    Vector4 way = {0.0f, 0.0f, 0.0f, 0.0f};
    s32 least = -1;
    if (!LeastOverlap(differences, groups, pairs == SixteenGroups, axes, axisCount, push, &way, &least))
    {
        return 0;
    }

    if (least < 0)
    {
        // No axes (every hull has some): retail reads its stack's tag before them
        return 1;
    }

    way.w = 1.0f;
    Matrix4x4 moved = MovedBy(otherMatrix, push);
    switch (AxisTagOf(&axes[least]))
    {
    case FirstNormal:
    {
        Vector4 back = Reversed(&way);
        HullSupportPoint(other, &moved, &back, point);
        break;
    }
    case SecondNormal:
        HullSupportPoint(hull, matrix, &way, point);
        break;
    case EdgeCross:
    {
        Vector4 start;
        Vector4 end;
        HullSupportEdge(hull, matrix, &way, &start, &end);
        Vector4 back = Reversed(&way);
        Vector4 otherStart;
        Vector4 otherEnd;
        HullSupportEdge(other, &moved, &back, &otherStart, &otherEnd);
        NearestPointOfSegment(&start, &end, &otherStart, &otherEnd, point);
        break;
    }
    default:
        break;
    }

    return 1;
}

u32 TriangleHullContact(const CollisionHit* triangle, const CollisionHull* hull, const Matrix4x4* matrix, Vector4* push,
                        Vector4* point)
{
    constexpr u32 Corners = 3;
    const Vector4* corners = triangle->vertices;
    Vector4 plane;
    PlaneThroughTriangle(&plane, &corners[0], &corners[1], &corners[2]);
    if (HullSideOfPlane(hull, matrix, &plane) != 0)
    {
        return 0;
    }

    point->x = -Infinite;
    point->w = 1.0f;
    point->y = -Infinite;
    point->z = -Infinite;
    Matrix4x4 identity;
    InitIdentityMatrix(&identity);
    ContactScratch scratch;
    Vector4* differences = scratch.Differences();
    Vector4* axes = scratch.Axes();
    u32 count = hull->vertexCount;
    VertexDifferences request;
    request.matrix = &identity;
    request.vertices = corners;
    request.count = Corners;
    request.otherMatrix = matrix;
    request.otherVertices = HullVertex(hull, 0);
    request.otherCount = count;
    request.differences = differences;
    request.moved = scratch.Moved();
    s32 pairs = count * Corners;
    MakeVertexDifferences(&request);
    s32 axisCount = TriangleHullAxes(triangle, hull, matrix, axes);
    s32 groups = pairs / 4;
    GroupPoints(differences, groups);
    PadAxes(axes, axisCount);
    Vector4 way = {0.0f, 0.0f, 0.0f, 0.0f};
    s32 least = -1;
    if (!LeastOverlap(differences, groups, false, axes, axisCount, push, &way, &least))
    {
        return 0;
    }

    if (least < 0)
    {
        return 1;
    }

    way.w = 1.0f;
    Matrix4x4 moved = MovedBy(matrix, push);
    u32 tag = AxisTagOf(&axes[least]);
    if (tag == FirstNormal)
    {
        Vector4 back = Reversed(&way);
        HullSupportPoint(hull, &moved, &back, point);
    }

    if (tag == SecondNormal)
    {
        // A face of the hull's: the triangle's corner furthest along the way out (worked out again for every face that's more
        // against it)
        f32 against = Infinite;
        for (s32 index = 0; index < hull->planeCount; index++)
        {
            Vector4 face;
            TransformPlane(HullPlane(hull, index), &moved, &face);
            Vector4 normal = {face.x, face.y, face.z, 1.0f};
            f32 along = way.x * normal.x + way.y * normal.y + way.z * normal.z;
            if (!(along < against))
            {
                continue;
            }

            against = along;
            *point = corners[0];
            f32 furthest = corners[0].x * way.x + corners[0].y * way.y + corners[0].z * way.z;
            f32 second = corners[1].x * way.x + corners[1].y * way.y + corners[1].z * way.z;
            if (furthest < second)
            {
                furthest = second;
                *point = corners[1];
            }

            f32 third = corners[2].x * way.x + corners[2].y * way.y + corners[2].z * way.z;
            if (furthest < third)
            {
                *point = corners[2];
            }
        }
    }

    if (tag != EdgeCross)
    {
        return 1;
    }

    // An edge of the triangle's: the one without the corner least along the way out (the first two on a tie)
    f32 along[Corners];
    for (u32 index = 0; index < Corners; index++)
    {
        along[index] = corners[index].x * way.x + corners[index].y * way.y + corners[index].z * way.z;
    }

    const Vector4* start = &corners[0];
    const Vector4* end = &corners[1];
    if (along[0] < along[1] && along[0] < along[2])
    {
        start = &corners[1];
        end = &corners[2];
    }
    else if (along[1] < along[0] && along[1] < along[2])
    {
        end = &corners[2];
    }

    Vector4 edgeStart = *start;
    Vector4 edgeEnd = *end;
    Vector4 back = Reversed(&way);
    Vector4 hullStart;
    Vector4 hullEnd;
    HullSupportEdge(hull, &moved, &back, &hullStart, &hullEnd);
    NearestPointOfSegment(&edgeStart, &edgeEnd, &hullStart, &hullEnd, point);
    return 1;
}

void NearestPointOfSegment(const Vector4* lineStart, const Vector4* lineEnd, const Vector4* start, const Vector4* end, Vector4* out)
{
    constexpr f32 Parallel = Rounded(1e-4);
    Vector4 line = {lineEnd->x - lineStart->x, lineEnd->y - lineStart->y, lineEnd->z - lineStart->z, 1.0f};
    Vector4 segment = {end->x - start->x, end->y - start->y, end->z - start->z, 1.0f};
    Vector4 apart = {lineStart->x - start->x, lineStart->y - start->y, lineStart->z - start->z, 1.0f};
    f32 lineSquared = line.x * line.x + line.y * line.y + line.z * line.z;
    f32 segmentSquared = segment.x * segment.x + segment.y * segment.y + segment.z * segment.z;
    f32 both = line.x * segment.x + line.y * segment.y + line.z * segment.z;
    f32 determinant = lineSquared * segmentSquared - both * both;
    f32 apartSegment = apart.x * segment.x + apart.y * segment.y + apart.z * segment.z;
    f32 apartLine = apart.x * line.x + apart.y * line.y + apart.z * line.z;
    f32 share = 0.5f;
    if (!(__builtin_fabsf(determinant) < Parallel))
    {
        share = (apartSegment * lineSquared - apartLine * both) / determinant;
    }

    if (share < 0.0f)
    {
        share = 0.0f;
    }

    if (1.0f < share)
    {
        share = 1.0f;
    }

    Vector4 part = {segment.x * share, segment.y * share, segment.z * share, 1.0f};
    *out = {start->x + part.x, start->y + part.y, start->z + part.z, 1.0f};
}
