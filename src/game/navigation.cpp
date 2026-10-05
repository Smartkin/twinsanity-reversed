#include "game/navigation.h"

#include "game/math.h"
#include "game/memory.h"
#include "game/stream.h"

EABI_EXPORT(FUN_00252500, SetNearFocusWeight);

namespace
{
// A path added to a position's links (the list made again one longer)
void AddLink(AiPosition* position, AiPath* path)
{
    u32 count = position->bits.linkCount;
    auto** links = static_cast<AiPath**>(MemoryAllocate2((count + 1) * sizeof(AiPath*)));
    for (u8 index = 0; index < count; index++)
    {
        links[index] = position->links[index];
    }

    links[count] = path;
    position->bits.linkCount = count + 1;
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
    bits.value = 0;
    links = nullptr;
    stream->Read(&position, sizeof(position), 1);
    stream->ReadS16(reinterpret_cast<s16*>(&flags.value));
    bits.stamp = g_PathSearchStamp;
}

void AiPath::Read(Stream* stream)
{
    stream->ReadS16(reinterpret_cast<s16*>(&positionA));
    stream->ReadS16(reinterpret_cast<s16*>(&positionB));
    stream->ReadS16(reinterpret_cast<s16*>(&flags.value));
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
    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    for (u16 index = 0; index < positionCount; index++)
    {
        positions[index]->bits.stamp = stamp;
    }
}

AiPosition* AiNavigation::Nearest(const Vector4* point)
{
    f32 best = Infinite;
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
    f32 best = Infinite;
    u16 nearestIndex = NoAiIndex;
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
    f32 best = Infinite;
    u16 nearestIndex = NoAiIndex;
    AiPosition* nearest = nullptr;
    for (u16 at = 0; at < positionCount; at++)
    {
        AiPosition* position = positions[at];
        u16 flags = position->flags.value;
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
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PathFinder::Clear()
{
    for (u32 index = 0; index < MostChunks; index++)
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
        if (steps >= static_cast<s32>(Route::MostSearchedSteps))
        {
            break;
        }

        if (step->previousPosition == AiPosition::NoPrevious)
        {
            break;
        }

        stepChunk = step->previousChunk;
        stepPosition = step->previousPosition;
        step->previousPosition = AiPosition::NoPrevious;
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
    // The open list starts in the middle, its last entry is the one before the last
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
    start->previousPosition = AiPosition::NoPrevious;
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
        for (u8 link = 0; link < at->bits.linkCount; link++)
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
                    unused11F4 = 0;
                    to = navigationA->positions[path->positionA];
                }
            }
            else if (navigationA != nullptr && navigationA->positions[path->positionA] == at && navigationB != nullptr)
            {
                toChunk = path->chunkB;
                unused11F4 = 1;
                toIndex = path->positionB;
                to = navigationB->positions[path->positionB];
            }

            if (to == nullptr || to->bits.stamp == g_PathSearchStamp)
            {
                continue;
            }

            f32 step = CallVirtual<f32>(this, vtable, StepCostSlot, path, at, to);
            if (!(0.0f < step))
            {
                continue;
            }

            to->bits.stamp = g_PathSearchStamp;
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
            request->flags.noRoute = 1;
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
    if (to->flags.blocked || from->flags.blocked)
    {
        return Blocked;
    }

    RouteRequestFlags flags = request->flags;
    if (flags.distanceOnly)
    {
        f32 x = from->position.x - to->position.x;
        f32 y = from->position.y - to->position.y;
        f32 z = from->position.z - to->position.z;
        return x * x + y * y + z * z;
    }

    AiPathFlags pathFlags = path->flags;
    if (flags.rulesOutScriptFlag6 && pathFlags.scriptFlag6)
    {
        return Blocked;
    }

    if (flags.rulesOutPlainPaths && (pathFlags.value & AiPathFlags::NotPlainMask) == 0)
    {
        return Blocked;
    }

    if (flags.rulesOutJumps)
    {
        if (pathFlags.needsJump)
        {
            return Blocked;
        }
    }
    else
    {
        if (flags.rulesOutLongJumps && pathFlags.needsLongJump)
        {
            return Blocked;
        }

        if (flags.rulesOutHighJumps && pathFlags.needsHighJump)
        {
            return Blocked;
        }
    }

    if (flags.rulesOutScriptFlag8 && pathFlags.scriptFlag8)
    {
        return Blocked;
    }

    if (flags.rulesOutScriptFlag7 && pathFlags.scriptFlag7)
    {
        return Blocked;
    }

    if (flags.rulesOutFlights && pathFlags.needsFlight)
    {
        return Blocked;
    }

    f32 x = from->position.x - to->position.x;
    f32 y = from->position.y - to->position.y;
    f32 z = from->position.z - to->position.z;
    f32 cost = x * x + y * y + z * z;
    if (flags.avoidsFocus)
    {
        f32 fx = to->position.x - focus.x;
        f32 fy = to->position.y - focus.y;
        f32 fz = to->position.z - focus.z;
        f32 radius = focusRadius;
        f32 distance = fx * fx + fy * fy + fz * fz;
        // A focus radius this small counts as none
        if (!(__builtin_fabsf(radius) <= Epsilon))
        {
            cost += (1.0f - distance / radius) * Weight;
        }
    }

    if (flags.positionCosts)
    {
        cost += static_cast<f32>(static_cast<s32>(to->bits.cost)) * Weight;
    }

    if (flags.nearFocus)
    {
        f32 fx = to->position.x - focus.x;
        f32 fy = to->position.y - focus.y;
        f32 fz = to->position.z - focus.z;
        f32 radius = focusRadius;
        f32 distance = fx * fx + fy * fy + fz * fz;
        if (__builtin_fabsf(radius) <= Epsilon)
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
    u32 count = bits.linkCount;
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
    for (u16 step = 0; step < count; step++)
    {
        AiNavigation* navigation = g_PathFinder->navigations[chunks[step]];
        AiPosition* position = navigation != nullptr ? navigation->positions[positions[step]] : nullptr;
        if (position == nullptr)
        {
            continue;
        }

        u32 cost = position->bits.cost;
        if (cost < AiPosition::MostCost)
        {
            position->bits.cost = cost + 1;
        }
    }
}

void Route::Leave()
{
    for (u16 step = 0; step < count; step++)
    {
        AiNavigation* navigation = g_PathFinder->navigations[chunks[step]];
        AiPosition* position = navigation != nullptr ? navigation->positions[positions[step]] : nullptr;
        if (position == nullptr || position->bits.cost == 0)
        {
            continue;
        }

        position->bits.cost = position->bits.cost - 1;
    }
}

void SetNearFocusWeight(PathFinder* finder, f32 weight)
{
    const Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 way = finder->routeEnd;
    way.x = way.x - finder->routeStart.x;
    way.y = way.y - finder->routeStart.y;
    way.z = way.z - finder->routeStart.z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    f32 x = way.x * inverse;
    f32 y = way.y * inverse;
    f32 z = way.z * inverse;
    Vector4 across;
    across.x = up.y * z - up.z * y;
    across.y = up.z * x - up.x * z;
    across.z = up.x * y - up.y * x;
    f32 dx = finder->routeStart.x - finder->routeEnd.x;
    f32 dy = finder->routeStart.y - finder->routeEnd.y;
    f32 dz = finder->routeStart.z - finder->routeEnd.z;
    f32 scale = weight * __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    finder->focus.x = finder->focus.x + across.x * scale;
    finder->focus.y = finder->focus.y + across.y * scale;
    finder->focus.z = finder->focus.z + across.z * scale;
}
