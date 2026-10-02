#include "game/navigation.h"

#include "game/memory.h"
#include "game/stream.h"

namespace
{
constexpr u32 StampMask = AiPosition::StampMask;
// Further than any point (the searches' first best)
constexpr f32 Far = 0x1.93e594p+99f;
constexpr u16 NoIndex = 0xFFFF;
constexpr u16 NoStep = 0xFF;
// The navigations the path finder keeps and the searches' routes' longest
constexpr u32 Navigations = 128;
constexpr s32 RouteSteps = 255;

// A path added to a position's links (the list made again one longer)
void AddLink(AiPosition* position, AiPath* path)
{
    constexpr u32 CountBits = AiPosition::LinkCountMask << AiPosition::LinkCountShift;
    u32 count = position->bits >> AiPosition::LinkCountShift & AiPosition::LinkCountMask;
    auto** links = static_cast<AiPath**>(MemoryAllocate2((count + 1) * sizeof(AiPath*)));
    for (u8 index = 0; index < count; index++)
    {
        links[index] = position->links[index];
    }

    links[count] = path;
    position->bits = (position->bits & ~CountBits) | ((count + 1) & AiPosition::LinkCountMask) << AiPosition::LinkCountShift;
    if (position->links != nullptr)
    {
        MemoryDeallocate_(position->links);
    }

    position->links = links;
}

// The squared distance from a point to a position's, by the coordinates
f32 SquaredDistanceTo(const Vector4* point, const AiPosition* position)
{
    f32 x = point->x - position->position.x;
    f32 y = point->y - position->position.y;
    f32 z = point->z - position->position.z;
    return x * x + y * y + z * z;
}

void DestroyPosition(AiPosition* position)
{
    if (position == nullptr)
    {
        return;
    }

    if (position->links != nullptr)
    {
        MemoryDeallocate_(position->links);
    }

    MemoryDeallocate2_(position);
}
}

void AiPosition::Read(Stream* stream)
{
    bits = 0;
    links = nullptr;
    stream->Read(&position, sizeof(position), 1);
    stream->ReadS16(&flags);
    bits = (bits & ~StampMask) | (g_PathSearchStamp & StampMask);
}

void AiPath::Read(Stream* stream)
{
    stream->ReadS16(reinterpret_cast<s16*>(&positionA));
    stream->ReadS16(reinterpret_cast<s16*>(&positionB));
    stream->ReadS16(reinterpret_cast<s16*>(&flags));
    stream->ReadS16(reinterpret_cast<s16*>(&chunkA));
    stream->ReadS16(reinterpret_cast<s16*>(&chunkB));
}

AiNavigation* AiNavigation::Construct(AiNavigation* navigation, PathFinder* pathFinder)
{
    navigation->pathFinder = pathFinder;
    navigation->positionCount = 0;
    navigation->vtable = g_AiNavigationVTable;
    navigation->pathCount = 0;
    navigation->positions = nullptr;
    navigation->paths = nullptr;
    return navigation;
}

void AiNavigation::Destroy(u32 destroyFlags)
{
    vtable = g_AiNavigationVTable;
    ClearPositions();
    ClearPaths();
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void AiNavigation::ClearPositions()
{
    for (u16 index = 0; index < positionCount; index++)
    {
        DestroyPosition(positions[index]);
    }

    if (positions != nullptr)
    {
        MemoryDeallocate_(positions);
    }

    positionCount = 0;
    positions = nullptr;
}

void AiNavigation::ClearPaths()
{
    for (u16 index = 0; index < pathCount; index++)
    {
        MemoryDeallocate2_(paths[index]);
    }

    if (paths != nullptr)
    {
        MemoryDeallocate_(paths);
    }

    pathCount = 0;
    paths = nullptr;
}

void AiNavigation::SetPositionCount(u32 count)
{
    u16 newCount = static_cast<u16>(count);
    for (u16 index = 0; index < positionCount; index++)
    {
        DestroyPosition(positions[index]);
    }

    if (positions != nullptr)
    {
        MemoryDeallocate_(positions);
    }

    positions = nullptr;
    positionCount = newCount;
    positions = static_cast<AiPosition**>(MemoryAllocate2(newCount * sizeof(AiPosition*)));
    for (u32 index = 0; index < newCount; index++)
    {
        positions[index] = nullptr;
    }
}

void AiNavigation::SetPathCount(u32 count)
{
    u16 newCount = static_cast<u16>(count);
    for (u16 index = 0; index < pathCount; index++)
    {
        MemoryDeallocate2_(paths[index]);
    }

    if (paths != nullptr)
    {
        MemoryDeallocate_(paths);
    }

    paths = nullptr;
    pathCount = newCount;
    paths = static_cast<AiPath**>(MemoryAllocate2(newCount * sizeof(AiPath*)));
    for (u32 index = 0; index < newCount; index++)
    {
        paths[index] = nullptr;
    }
}

void AiNavigation::SetPosition(u32 index, AiPosition* position)
{
    u16 slot = static_cast<u16>(index);
    DestroyPosition(positions[slot]);
    positions[slot] = position;
}

void AiNavigation::SetPath(u32 index, AiPath* path)
{
    u16 slot = static_cast<u16>(index);
    MemoryDeallocate2_(paths[slot]);
    paths[slot] = path;
}

void AiNavigation::SetPathFinder(PathFinder* finder)
{
    pathFinder = finder;
}

void AiNavigation::StampPositions(u32 stamp)
{
    u32 bits = stamp & StampMask;
    for (u16 index = 0; index < positionCount; index++)
    {
        AiPosition* position = positions[index];
        position->bits = (position->bits & ~StampMask) | bits;
    }
}

AiPosition* AiNavigation::Nearest(const Vector4* point)
{
    f32 best = Far;
    AiPosition* nearest = nullptr;
    for (u16 index = 0; index < positionCount; index++)
    {
        AiPosition* position = positions[index];
        f32 distance = SquaredDistanceTo(point, position);
        if (distance < best)
        {
            best = distance;
            nearest = position;
        }
    }

    return nearest;
}

AiPosition* AiNavigation::NearestIndex(const Vector4* point, u16* index)
{
    f32 best = Far;
    u16 nearestIndex = NoIndex;
    AiPosition* nearest = nullptr;
    for (u16 at = 0; at < positionCount; at++)
    {
        AiPosition* position = positions[at];
        f32 distance = SquaredDistanceTo(point, position);
        if (distance < best)
        {
            best = distance;
            nearest = position;
            nearestIndex = at;
        }
    }

    *index = nearestIndex;
    return nearest;
}

AiPosition* AiNavigation::NearestWithFlags(const Vector4* point, u16* index, u32 required, u32 ruledOut)
{
    u16 requiredFlags = static_cast<u16>(required);
    u16 ruledOutFlags = static_cast<u16>(ruledOut);
    f32 best = Far;
    u16 nearestIndex = NoIndex;
    AiPosition* nearest = nullptr;
    for (u16 at = 0; at < positionCount; at++)
    {
        AiPosition* position = positions[at];
        u16 flags = static_cast<u16>(position->flags);
        if (requiredFlags != 0 && (flags & requiredFlags) == 0)
        {
            continue;
        }

        if ((flags & ruledOutFlags) != 0)
        {
            continue;
        }

        f32 distance = SquaredDistanceTo(point, position);
        if (distance < best)
        {
            best = distance;
            nearest = position;
            nearestIndex = at;
        }
    }

    *index = nearestIndex;
    return nearest;
}

void AiNavigation::CollectInside(u32 chunkIndex, const Box* box, Route* route)
{
    u16 chunk = static_cast<u16>(chunkIndex);
    for (u16 index = 0; index < positionCount; index++)
    {
        // The position as a point (its W the radius, made 1)
        Vector4 point = positions[index]->position;
        point.w = 1.0f;
        if (box->Contains(&point) != 0)
        {
            route->chunks[route->count] = chunk;
            route->positions[route->count] = index;
            route->count++;
        }
    }
}

PathFinder* PathFinder::Construct(PathFinder* finder)
{
    finder->vtable = g_PathFinderVTable;
    finder->Clear();
    g_PathSearchStamp = 0;
    return finder;
}

void PathFinder::Destroy(u32 destroyFlags)
{
    vtable = g_PathFinderVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PathFinder::Clear()
{
    for (u32 index = 0; index < Navigations; index++)
    {
        navigations[index] = nullptr;
    }
}

u8 PathFinder::CollectInside(const Box* box, Route* route)
{
    for (u16 index = 0; index < count; index++)
    {
        AiNavigation* navigation = navigations[index];
        if (navigation != nullptr)
        {
            navigation->CollectInside(index, box, route);
        }
    }

    return route->count;
}

void PathFinder::NewSearch()
{
    constexpr u32 StampLimit = 0x8000;
    g_PathSearchStamp++;
    if (g_PathSearchStamp != StampLimit)
    {
        return;
    }

    for (u16 index = 0; index < count; index++)
    {
        AiNavigation* navigation = navigations[index];
        if (navigation != nullptr)
        {
            navigation->StampPositions(0);
        }
    }

    g_PathSearchStamp = 1;
}

Route* PathFinder::MakeRoute(RouteRequest*, AiPosition* start, u32 chunk, u32 position)
{
    auto* route = static_cast<Route*>(MemoryAllocate(sizeof(Route)));
    u16 stepChunk = static_cast<u16>(chunk);
    u16 stepPosition = static_cast<u16>(position);
    route->count = 0;
    for (s32 steps = 1;; steps++)
    {
        route->chunks[route->count] = stepChunk;
        route->positions[route->count] = stepPosition;
        route->count++;
        AiPosition* step = navigations[stepChunk]->positions[stepPosition];
        if (steps >= RouteSteps)
        {
            break;
        }

        if (step->previousPosition == NoStep)
        {
            break;
        }

        stepChunk = step->previousChunk;
        stepPosition = step->previousPosition;
        step->previousPosition = NoStep;
        if (step == start)
        {
            break;
        }
    }

    return route;
}

f32 PathFinder::SquaredDistance(const AiPath*, const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return x * x + y * y + z * z;
}

f32 PathFinder::SquaredDistanceOf(const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return x * x + y * y + z * z;
}

void AiNavigation::Link(PathFinder* finder, u32 chunkIndex)
{
    u16 chunk = static_cast<u16>(chunkIndex);
    SetPathFinder(finder);
    finder->navigations[chunk] = this;
    SetPathFinder(finder);
    finder->NewSearch();
    for (s32 index = 0; index < pathCount; index++)
    {
        AiPath* path = paths[index];
        path->chunkA = chunk;
        path->chunkB = chunk;
        u16 positionA = path->positionA;
        u16 positionB = path->positionB;
        u16 chunkA = path->chunkA;
        u16 chunkB = path->chunkB;
        if (chunkA == chunk && chunkB == chunk)
        {
            AddLink(positions[positionA], path);
            AddLink(positions[positionB], path);
            continue;
        }

        AiNavigation* first = finder->navigations[chunkA];
        AiNavigation* second = finder->navigations[chunkB];
        if (first != nullptr)
        {
            AddLink(first->positions[positionA], path);
        }

        if (second != nullptr)
        {
            AddLink(second->positions[positionB], path);
        }
    }
}

Route* PathFinder::Search(RouteRequest* request)
{
    // The vtable's step cost and estimate; the open list starts in the middle, its last entry is the one before the last
    constexpr u32 StepCostSlot = 2;
    constexpr u32 EstimateSlot = 3;
    constexpr u32 Middle = 0x7F;
    constexpr u32 LastTail = 0xFE;
    NewSearch();
    this->request = request;
    u32 tail = Middle;
    AiPosition* start = navigations[request->startChunk]->positions[request->startPosition];
    AiPosition* end = navigations[request->endChunk]->positions[request->endPosition];
    u32 head = Middle;
    SearchEntry* first = &entries[Middle];
    first->cost = 0.0f;
    first->estimate = CallVirtual<f32>(this, vtable, EstimateSlot, start, end);
    first->position = start;
    first->chunk = request->startChunk;
    first->index = request->startPosition;
    start->previousPosition = NoStep;
    SearchEntry* next = first;
    AiPosition* at = next->position;
    for (;;)
    {
        if (at == end)
        {
            return MakeRoute(request, start, next->chunk, next->index);
        }

        SearchEntry* from = next;
        next++;
        head++;
        u16 fromChunk = from->chunk;
        u16 fromIndex = from->index;
        for (u8 link = 0; link < (at->bits >> AiPosition::LinkCountShift & AiPosition::LinkCountMask); link++)
        {
            AiPath* path = at->links[link];
            AiNavigation* navigationB = navigations[path->chunkB];
            AiNavigation* navigationA = navigations[path->chunkA];
            AiPosition* to = nullptr;
            u16 toChunk = 0;
            u16 toIndex = 0;
            if (navigationB != nullptr && navigationB->positions[path->positionB] == at)
            {
                if (navigationA != nullptr)
                {
                    toChunk = path->chunkA;
                    toIndex = path->positionA;
                    backwards = 0;
                    to = navigationA->positions[path->positionA];
                }
            }
            else if (navigationA != nullptr && navigationA->positions[path->positionA] == at && navigationB != nullptr)
            {
                toChunk = path->chunkB;
                backwards = 1;
                toIndex = path->positionB;
                to = navigationB->positions[path->positionB];
            }

            if (to == nullptr || (to->bits & StampMask) == g_PathSearchStamp)
            {
                continue;
            }

            f32 step = CallVirtual<f32>(this, vtable, StepCostSlot, path, at, to);
            if (!(0.0f < step))
            {
                continue;
            }

            to->bits = (to->bits & ~StampMask) | (g_PathSearchStamp & StampMask);
            if (to == end)
            {
                end->previousPosition = fromIndex;
                end->previousChunk = fromChunk;
                return MakeRoute(request, start, toChunk, toIndex);
            }

            f32 cost = from->cost + step;
            f32 left = CallVirtual<f32>(this, vtable, EstimateSlot, to, end);
            u32 slot;
            if (cost + left < entries[head].estimate)
            {
                if (head == 0)
                {
                    return nullptr;
                }

                head--;
                next--;
                slot = head;
            }
            else
            {
                if (tail == LastTail)
                {
                    return nullptr;
                }

                tail++;
                slot = tail;
            }

            SearchEntry& entry = entries[slot];
            entry.estimate = cost + left;
            entry.index = toIndex;
            entry.position = to;
            entry.cost = cost;
            entry.chunk = toChunk;
            to->previousPosition = fromIndex;
            to->previousChunk = fromChunk;
        }

        if (tail < head)
        {
            request->flags |= RouteRequest::FlagNoRoute;
            return nullptr;
        }

        at = next->position;
    }
}

GamePathFinder* GamePathFinder::Construct(GamePathFinder* finder)
{
    PathFinder::Construct(finder);
    finder->vtable = g_GamePathFinderVTable;
    return finder;
}

void GamePathFinder::Destroy(u32 destroyFlags)
{
    vtable = g_GamePathFinderVTable;
    PathFinder::Destroy(destroyFlags);
}

f32 GamePathFinder::Estimate(const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return x * x + y * y + z * z;
}

f32 GamePathFinder::StepCost(const AiPath* path, const AiPosition* from, const AiPosition* to)
{
    constexpr f32 Blocked = -1.0f;
    constexpr f32 Weight = 100.0f;
    // A focus radius this small counts as none
    constexpr f32 NoRadius = 0x1.a36e2ep-15f;
    if ((to->flags & AiPosition::FlagBlocked) != 0 || (from->flags & AiPosition::FlagBlocked) != 0)
    {
        return Blocked;
    }

    u32 flags = request->flags;
    if ((flags & RouteRequest::FlagDistanceOnly) != 0)
    {
        f32 x = from->position.x - to->position.x;
        f32 y = from->position.y - to->position.y;
        f32 z = from->position.z - to->position.z;
        return x * x + y * y + z * z;
    }

    // The flags of the paths each request flag rules out (bit 24 rules out the paths with none of 5-8)
    u16 pathFlags = path->flags;
    if ((flags & 0x200000) != 0 && (pathFlags & 0x40) != 0)
    {
        return Blocked;
    }

    if ((flags & 0x1000000) != 0 && (pathFlags & 0x1E0) == 0)
    {
        return Blocked;
    }

    if ((flags & 0x20000) != 0)
    {
        if ((pathFlags & 0x4) != 0)
        {
            return Blocked;
        }
    }
    else
    {
        if ((flags & 0x80000) != 0 && (pathFlags & 0x8) != 0)
        {
            return Blocked;
        }

        if ((flags & 0x40000) != 0 && (pathFlags & 0x10) != 0)
        {
            return Blocked;
        }
    }

    if ((flags & 0x800000) != 0 && (pathFlags & 0x100) != 0)
    {
        return Blocked;
    }

    if ((flags & 0x400000) != 0 && (pathFlags & 0x80) != 0)
    {
        return Blocked;
    }

    if ((flags & 0x100000) != 0 && (pathFlags & 0x20) != 0)
    {
        return Blocked;
    }

    f32 x = from->position.x - to->position.x;
    f32 y = from->position.y - to->position.y;
    f32 z = from->position.z - to->position.z;
    f32 cost = x * x + y * y + z * z;
    if ((flags & RouteRequest::FlagAvoidFocus) != 0)
    {
        f32 fx = to->position.x - focus.x;
        f32 fy = to->position.y - focus.y;
        f32 fz = to->position.z - focus.z;
        f32 radius = focusRadius;
        f32 distance = fx * fx + fy * fy + fz * fz;
        if (!(__builtin_fabsf(radius) <= NoRadius))
        {
            cost += (1.0f - distance / radius) * Weight;
        }
    }

    if ((flags & RouteRequest::FlagPositionCosts) != 0)
    {
        cost += static_cast<f32>(static_cast<s32>(to->bits >> AiPosition::CostShift & AiPosition::CostMask)) * Weight;
    }

    if ((flags & RouteRequest::FlagNearFocus) != 0)
    {
        f32 fx = to->position.x - focus.x;
        f32 fy = to->position.y - focus.y;
        f32 fz = to->position.z - focus.z;
        f32 radius = focusRadius;
        f32 distance = fx * fx + fy * fy + fz * fz;
        if (__builtin_fabsf(radius) <= NoRadius)
        {
            return cost;
        }

        cost += distance * Weight / radius;
    }

    return cost;
}

AiPath* AiPosition::PathTo(u32 chunk, u32 index)
{
    u16 toChunk = static_cast<u16>(chunk);
    u16 toIndex = static_cast<u16>(index);
    u32 count = bits >> LinkCountShift & LinkCountMask;
    for (u16 link = 0; link < count; link++)
    {
        AiPath* path = links[link];
        if ((path->chunkA == toChunk && path->positionA == toIndex) || (path->chunkB == toChunk && path->positionB == toIndex))
        {
            return path;
        }
    }

    return nullptr;
}

AiPosition* Route::PositionAt(u32 step)
{
    u8 at = static_cast<u8>(step);
    AiNavigation* navigation = g_PathFinder->navigations[chunks[at]];
    return navigation != nullptr ? navigation->positions[positions[at]] : nullptr;
}

AiPath* Route::PathTo(u32 step)
{
    u8 at = static_cast<u8>(step);
    if (at == 0)
    {
        return nullptr;
    }

    AiNavigation* navigation = g_PathFinder->navigations[chunks[at]];
    AiPosition* position = navigation != nullptr ? navigation->positions[positions[at]] : nullptr;
    return position->PathTo(chunks[at - 1], positions[at - 1]);
}

void Route::Occupy()
{
    constexpr u32 CostBits = AiPosition::CostMask << AiPosition::CostShift;
    for (u16 step = 0; step < count; step++)
    {
        AiNavigation* navigation = g_PathFinder->navigations[chunks[step]];
        AiPosition* position = navigation != nullptr ? navigation->positions[positions[step]] : nullptr;
        if (position == nullptr)
        {
            continue;
        }

        u32 cost = position->bits >> AiPosition::CostShift & AiPosition::CostMask;
        if (cost < AiPosition::CostMask)
        {
            position->bits = (position->bits & ~CostBits) | ((cost + 1) & AiPosition::CostMask) << AiPosition::CostShift;
        }
    }
}

void Route::Leave()
{
    constexpr u32 CostBits = AiPosition::CostMask << AiPosition::CostShift;
    for (u16 step = 0; step < count; step++)
    {
        AiNavigation* navigation = g_PathFinder->navigations[chunks[step]];
        AiPosition* position = navigation != nullptr ? navigation->positions[positions[step]] : nullptr;
        if (position == nullptr || (position->bits & CostBits) == 0)
        {
            continue;
        }

        u32 cost = position->bits >> AiPosition::CostShift & AiPosition::CostMask;
        position->bits = (position->bits & ~CostBits) | ((cost - 1) & AiPosition::CostMask) << AiPosition::CostShift;
    }
}
