#include "game/characters.h"

#include "game/agentparts.h"
#include "game/attachments.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/followcamera.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/place.h"
#include "game/reference.h"
#include "retail/libc.h"

#include <cstddef>
#include <cstdint>

// The target lock Nina's claw and the gun aim with: a hull ahead of the character that picks the best instance of its node kinds
// in it, keeps it a little while it's out of sight, and shows a marker on it

EABI_EXPORT(FUN_00161c38, &TargetLock::SetShape);
EABI_EXPORT(FUN_00161c68, &TargetLock::SetFlatShape);

namespace
{
// The kinds of the nodes the search asks for (characters, crates, pickups, creatures, generic objects, grabbables, pay gates,
// graples, projectiles and kind 0x15) and how many instances it ranks at most
constexpr u32 SearchedKinds = 1u << NodeCharacter | 1u << NodeCrate | 1u << NodePickup | 1u << NodeCreature |
                              1u << NodeGenericObject | 1u << NodeGrabbable | 1u << NodePayGate | 1u << NodeGraple |
                              1u << NodeProjectile | 1u << NodeUnusedObjectType;
constexpr u16 MostFound = 0x40;
// How much being off the hull's middle counts against an instance (times its squared distance), and where a kind's priority goes
// in an instance's score (its top byte, above any distance)
constexpr f32 OffMiddleWeight = 100.0f;
constexpr u32 PriorityShift = 24;
// A target is kept for a quarter of a second after it was last the best
constexpr f32 KeepSeconds = 0.25f;
// The marker stands this far in front of the target, toward the camera
constexpr f32 MarkerDistance = 2.0f;

InstanceContext* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
}

// An object's place as the retail code reads it, also when there's no object (the word at address 8 then)
ObjectPlace* RetailPlaceOf(const ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// The agent of an agent node as the retail code reads it, also when there's no node (the word at address 0x18 then)
Agent* RetailAgentOf(const AgentNode* node)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(node) + offsetof(AgentNode, agent);
    return *reinterpret_cast<Agent* const*>(address);
}

// The middle of a box the way the lock works it out (half its size on from its lowest corner; w the top corner's)
Vector4 MiddleOf(const Box* box)
{
    Vector4 middle = box->max;
    middle.x = (middle.x - box->min.x) * 0.5f + box->min.x;
    middle.y = (middle.y - box->min.y) * 0.5f + box->min.y;
    middle.z = (middle.z - box->min.z) * 0.5f + box->min.z;
    return middle;
}

// A query of a chunk's instances into the results: none of the asleep ones, all the wanted flags needed (none)
void StartQuery(InstanceQuery* query, InstanceContext** results, u16 most)
{
    query->results = reinterpret_cast<void**>(results);
    query->most = most;
    query->count = 0;
    query->distance = NoHitDistance;
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = 0;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}
}

static_assert(offsetof(TargetLock, blockingKinds) == 0x4);
static_assert(offsetof(TargetLock, target) == 0x60);
static_assert(offsetof(TargetLock, hullMatrix) == 0xB0);

TargetLock* TargetLock::Construct(TargetLock* lock)
{
    lock->blockingKinds = 0;
    lock->target = nullptr;
    lock->marker = nullptr;
    HullConstruct(&lock->hull);
    RetailLibc::MemorySet(&lock->bits, 0, sizeof(lock->bits));
    lock->Reset();
    return lock;
}

void TargetLock::Reset()
{
    u32 kindCount = bits.kindCount;
    u32 untargettable = bits.takesUntargettable;
    RetailLibc::MemorySet(&bits, 0, sizeof(bits));
    bits.kindCount = kindCount;
    bits.takesUntargettable = untargettable;
    seenTime = 0;
    AssignReference(&target, nullptr);
    AssignReference(&marker, nullptr);
}

void TargetLock::MakeHull(u32 flat)
{
    // A square of the near half size half a unit behind, the middle (the mean of the half sizes) a quarter short of halfway
    // along, the far end at the far half size across; the top is at the mean from the middle on, the bottom at the near half
    // size when it's flat, else at the mean
    f32 mean = farHalf * 0.5f + nearHalf * 0.5f;
    f32 middleZ = length * 0.5f + -0.25f;
    f32 bottom = flat != 0 ? -nearHalf : -mean;
    Vector4 points[12] = {
        {-nearHalf, -nearHalf, -0.5f, 1.0f}, {nearHalf, -nearHalf, -0.5f, 1.0f}, {nearHalf, nearHalf, -0.5f, 1.0f},
        {-nearHalf, nearHalf, -0.5f, 1.0f},  {-mean, bottom, middleZ, 1.0f},     {mean, bottom, middleZ, 1.0f},
        {mean, mean, middleZ, 1.0f},         {-mean, mean, middleZ, 1.0f},       {-farHalf, bottom, length, 1.0f},
        {farHalf, bottom, length, 1.0f},     {farHalf, mean, length, 1.0f},      {-farHalf, mean, length, 1.0f},
    };

    BeginHullBuild();
    for (const Vector4& point : points)
    {
        AddHullPoint(&point);
    }

    AddHullQuad(&points[0], &points[1], &points[2], &points[3]);
    AddHullQuad(&points[0], &points[4], &points[5], &points[1]);
    AddHullQuad(&points[1], &points[5], &points[6], &points[2]);
    AddHullQuad(&points[3], &points[2], &points[6], &points[7]);
    AddHullQuad(&points[3], &points[7], &points[4], &points[0]);
    AddHullQuad(&points[4], &points[8], &points[9], &points[5]);
    AddHullQuad(&points[5], &points[9], &points[10], &points[6]);
    AddHullQuad(&points[7], &points[6], &points[10], &points[11]);
    AddHullQuad(&points[7], &points[11], &points[8], &points[4]);
    AddHullQuad(&points[9], &points[8], &points[11], &points[10]);
    EndHullPoints();
    FinishHullBuild(&hull);
}

void TargetLock::SetShape(f32 length, f32 nearHalf, f32 farHalf)
{
    this->length = length;
    this->nearHalf = nearHalf;
    this->farHalf = farHalf;
    MakeHull(0);
}

void TargetLock::SetFlatShape(f32 length, f32 nearHalf, f32 farHalf)
{
    this->length = length;
    this->nearHalf = nearHalf;
    this->farHalf = farHalf;
    MakeHull(1);
}

void TargetLock::AddKind(u32 kind, u32 priority)
{
    u32 count = bits.kindCount;
    kinds[count][0] = kind;
    kinds[count][1] = priority;
    bits.kindCount = count + 1;
}

u32 TargetLock::CanTarget(InstanceContext* instance)
{
    if (instance == nullptr || instance->flags.asleep)
    {
        return 0;
    }

    // Retail reads the agent without checking the instance has an agent node
    Agent* agent = RetailAgentOf(AgentNodeOf2(instance));
    if (bits.takesUntargettable == 0
        && static_cast<BasicAgentPart*>(agent->part)->bits.targettable == 0)
    {
        return 0;
    }

    // The first of its kinds the instance has a node of gives the priority; 0 and 128 up can't be targeted
    u32 kindCount = bits.kindCount;
    for (u32 index = 0; index < kindCount; index++)
    {
        if (instance->nodes.nodes[kinds[index][0]] != nullptr)
        {
            return static_cast<s32>(u32{kinds[index][1]} << PriorityShift) > 0;
        }
    }

    return 0;
}

void TargetLock::Search(TimeClock* clock, InstanceContext* owner, InstanceContext* holder)
{
    InstanceContext* found[MostFound];
    InstanceQuery query;
    u32 now = clock->time;
    bool targetFound = false;
    StartQuery(&query, found, MostFound);
    ChunkData* chunk = owner->chunk;
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    hullMatrix = place->matrix;
    Vector4 turned;
    VuRotateVector(&hullMatrix, &offset, &turned);
    Vector4* start = RowOf(&hullMatrix, 3);
    start->x = start->x + turned.x;
    start->y = start->y + turned.y;
    start->z = start->z + turned.z;
    SkipInQuery(&query, owner);
    u32 count = ChunkInstancesInHull(chunk, &hull, &hullMatrix, SearchedKinds, &query, 1);
    g_TargetCount = 0;
    for (u32 index = 0; index < count; index++)
    {
        InstanceContext* instance = found[index];
        if (CanTarget(instance) == 0 && (instance->nodes.mask & blockingKinds) == 0)
        {
            continue;
        }

        Vector4 middle = MiddleOf(instance->CollisionBox());
        if (GetCollisionCheck(chunk, start, &middle, SurfaceFlags::SolidToPlayerProbes, nullptr, nullptr, nullptr) != 0)
        {
            continue;
        }

        // Ranked by the priority of its kind, then by how far off the hull's middle it is (along the hull's sideways axis, times
        // 100) times its squared distance
        u32 priority = 0;
        u32 kindCount = bits.kindCount;
        for (u32 kind = 0; kind < kindCount; kind++)
        {
            if (instance->nodes.nodes[kinds[kind][0]] != nullptr)
            {
                priority = u32{kinds[kind][1]} << PriorityShift;
                break;
            }
        }

        Vector4 way = middle;
        way.x = way.x - start->x;
        way.y = way.y - start->y;
        way.z = way.z - start->z;
        f32 distanceSquared = way.x * way.x + way.y * way.y + way.z * way.z;
        f32 inverse = InverseLength(&way, LengthEpsilon);
        way.x = way.x * inverse;
        way.y = way.y * inverse;
        way.z = way.z * inverse;
        const Vector4* sideways = RowOf(&hullMatrix, 0);
        f32 offMiddle = __builtin_fabsf(way.x * sideways->x + way.y * sideways->y + way.z * sideways->z) * OffMiddleWeight;
        TargetEntry* entry = &g_Targets[g_TargetCount];
        AssignReference(&entry->instance, instance);
        entry->score = priority + static_cast<s32>(offMiddle * distanceSquared);
        g_TargetCount++;
        if (ObjectOf(target) == instance)
        {
            targetFound = true;
        }
    }

    InstanceContext* best = nullptr;
    if (g_TargetCount == 1)
    {
        best = ObjectOf(g_Targets[0].instance);
    }
    else if (g_TargetCount != 0)
    {
        RetailLibc::QuickSort(g_Targets, g_TargetCount, sizeof(TargetEntry),
                              reinterpret_cast<s32 (*)(const void*, const void*)>(CompareTargets));
        best = ObjectOf(g_Targets[0].instance);
    }

    if (best != nullptr && (best->nodes.mask & blockingKinds) != 0)
    {
        g_TargetCount = 0;
        best = nullptr;
    }

    // A new best replaces the target once the target wasn't the best for a while, or when nothing is in the hull any more; before
    // that only once the target can't be targeted
    InstanceContext* current = ObjectOf(target);
    if (current == best)
    {
        seenTime = now;
    }
    else if (static_cast<s32>(now - seenTime) >= static_cast<s32>(g_ClockUnitsPerSecond * KeepSeconds)
             || (!targetFound && best == nullptr))
    {
        AssignReference(&target, best);
    }
    else if (CanTarget(current) == 0)
    {
        AssignReference(&target, best);
        seenTime = now;
    }

    PlaceMarker(owner, holder);
}

void TargetLock::Keep(TimeClock* clock, InstanceContext* owner, InstanceContext* holder)
{
    InstanceContext* current = ObjectOf(target);
    if (current != nullptr && CanTarget(current) != 0)
    {
        seenTime = clock->time;
    }

    PlaceMarker(owner, holder);
}

void TargetLock::Drop()
{
    AssignReference(&target, nullptr);
    InstanceContext* shown = ObjectOf(marker);
    if (shown != nullptr)
    {
        shown->flags.visible = 0;
    }
}

void TargetLock::PlaceMarker(InstanceContext* owner, InstanceContext* holder)
{
    // The marker is the first instance linked to the holder
    if (ObjectOf(marker) == nullptr && holder != nullptr)
    {
        auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&holder->nodes, NodeAttachments));
        if (attachments != nullptr && attachments->LinkedCount() != 0)
        {
            AssignReference(&marker, attachments->linked[0]);
        }
    }

    InstanceContext* shown = ObjectOf(marker);
    if (shown == nullptr)
    {
        return;
    }

    InstanceContext* aimed = ObjectOf(target);
    if (aimed == nullptr)
    {
        shown->flags.visible = 0;
        return;
    }

    // Turned like the camera, 2 units in front of the target's middle toward it (on it when the camera is nearer)
    auto* follow = static_cast<FollowNode*>(GetGameNode(&owner->nodes, NodeFollow));
    ObjectPlace* cameraPlace = RetailPlaceOf(ObjectOf(follow->cameraInstance));
    ObjectPlace* aimedPlace = aimed->place;
    RotateAndTranslate(cameraPlace);
    Matrix4x4 matrix = cameraPlace->matrix;
    RotateAndTranslate(aimedPlace);
    targetMatrix = aimedPlace->matrix;
    *RowOf(&targetMatrix, 3) = MiddleOf(aimed->CollisionBox());
    Vector4* at = RowOf(&matrix, 3);
    Vector4 way = *RowOf(&targetMatrix, 3);
    way.x = way.x - at->x;
    way.y = way.y - at->y;
    way.z = way.z - at->z;
    f32 distance = Kept(__builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z));
    if (MarkerDistance < distance)
    {
        f32 share = (distance - MarkerDistance) / distance;
        way.x = way.x * share;
        way.y = way.y * share;
        way.z = way.z * share;
    }

    at->x = at->x + way.x;
    at->y = at->y + way.y;
    at->z = at->z + way.z;
    shown->flags.visible = 1;
    if (SetPlaceMatrix(shown->place, &matrix) != 0)
    {
        QueueObject(shown);
    }
}

s32 CompareTargets(const TargetEntry* first, const TargetEntry* second)
{
    return static_cast<s32>(static_cast<u32>(first->score) - static_cast<u32>(second->score));
}

u32 TargetLock::TargetPoint(Vector4* point)
{
    if (ObjectOf(target) == nullptr)
    {
        return 0;
    }

    *point = *RowOf(&targetMatrix, 3);
    return 1;
}
