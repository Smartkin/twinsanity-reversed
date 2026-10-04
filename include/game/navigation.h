#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "gcc2.h"

class Stream;

// An AI position of a layout (0x20 bytes): where it is (W its radius, which only the condition of the distance to the nearest
// point's edge reads), the path finder's stamp of its last search (bits 0-14), how many paths link it (bits 15-19) and its cost
// for the routes that count it (bits 20-27), those paths, the step a search came to it from (a chunk's index and a position's, 0xFF none) and its flags (the searches can ask for
// some and rule some out)
struct AiPosition
{
    enum Bits : u32
    {
        StampMask = 0x7FFF,
        LinkCountShift = 15,
        LinkCountMask = 0x1F,
        CostShift = 20,
        CostMask = 0xFF,
        // Flag 0: no route goes through it
        FlagBlocked = 0x1,
    };

    Vector4 position;
    u32 bits;
    struct AiPath** links;
    u16 previousChunk;
    u16 previousPosition;
    s16 flags;
    u16 unknown1E;

    // Made at the default box's corner (w 1) with no stamp, links nor flags, no previous position (0xFF)
    static AiPosition* Construct(AiPosition* position) RETAIL(FUN_0023ce48);
    // Read from a stream (its position and flags), the stamp the path finder's current one and no links
    void Read(Stream* stream) RETAIL(FUN_0023cf48);
    // Its path to a position (a chunk's index and its own), nullptr when none links them
    struct AiPath* PathTo(u32 chunk, u32 index) RETAIL(FUN_0023cea0);
};
CHECK_OFFSET(AiPosition, flags, 0x1C);
CHECK_SIZE(AiPosition, 0x20);

// An AI path of a layout (0xA bytes): the two positions it joins (their indexes), its flags (which kinds of routes may take it),
// and the chunks the positions are in (the path's own, set when the chunk's navigation is linked)
struct AiPath
{
    u16 positionA;
    u16 positionB;
    u16 flags;
    u16 chunkA;
    u16 chunkB;

    void Read(Stream* stream) RETAIL(FUN_002526a0);
};
CHECK_SIZE(AiPath, 0xA);

// A route of AI positions the path finder finds or collects (0x402 bytes): each step's chunk index and position index, and the
// count of steps
struct Route
{
    u16 chunks[256];
    u16 positions[256];
    u8 count;
    u8 unknown401;

    // A step's position (nullptr when its chunk has no navigation), and the path that led to it (none for the first)
    struct AiPosition* PositionAt(u32 step) RETAIL(FUN_00253de0);
    struct AiPath* PathTo(u32 step) RETAIL(FUN_00253e28);
    // Every step's position made to cost one more for the routes that count them (up to 255), and one less again
    void Occupy() RETAIL(FUN_00253eb8);
    void Leave() RETAIL(FUN_00253f70);
};
CHECK_SIZE(Route, 0x402);

// A chunk's AI navigation (0x14 bytes; vtable 0x10 in: 1 the destructor): the AI positions and paths of its layouts by their
// index, their counts, and the chunk manager's path finder (retail's MiniBigBoi)
struct AiNavigation
{
    AiPosition** positions;
    AiPath** paths;
    u16 positionCount;
    u16 pathCount;
    struct PathFinder* pathFinder;
    const GccVTableEntry* vtable;

    static AiNavigation* Construct(AiNavigation* navigation, struct PathFinder* pathFinder) RETAIL(FUN_0023bf40);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0023bf68);
    // Every position (and its links) or path let go, and the list
    void ClearPositions() RETAIL(FUN_0023bfc0);
    void ClearPaths() RETAIL(FUN_0023c068);
    // The positions or paths let go and a list of the count made, empty (the count's low 16 bits)
    void SetPositionCount(u32 count) RETAIL(FUN_0023c0e8);
    void SetPathCount(u32 count) RETAIL(FUN_0023c1c8);
    // The position or path at an index (the low 16 bits) replaced, the one there let go
    void SetPosition(u32 index, AiPosition* position) RETAIL(FUN_0023c578);
    void SetPath(u32 index, AiPath* path) RETAIL(FUN_0023c5f8);
    void SetPathFinder(struct PathFinder* finder) RETAIL(FUN_0023c510);
    // Every position given a stamp (the low 15 bits)
    void StampPositions(u32 stamp) RETAIL(FUN_0023c518);
    // The position nearest a point by the coordinates alone (nullptr without positions); with its index (0xFFFF none); and the
    // nearest with any of the required flags (all of them when none are) and none of the ruled out ones
    AiPosition* Nearest(const Vector4* point) RETAIL(FUN_0023c288);
    AiPosition* NearestIndex(const Vector4* point, u16* index) RETAIL(FUN_0023c348);
    AiPosition* NearestWithFlags(const Vector4* point, u16* index, u32 required, u32 ruledOut) RETAIL(FUN_0023c410);
    // The positions in a box added to a route (the chunk's index given)
    void CollectInside(u32 chunkIndex, const Box* box, Route* route) RETAIL(FUN_0023c650);
    // Given to the path finder as the chunk's (a new search started), each path made the chunk's and added to the links of the two
    // positions it joins
    void Link(struct PathFinder* finder, u32 chunkIndex) RETAIL(FUN_0022c2e8);
};
CHECK_SIZE(AiNavigation, 0x14);

// What asks the path finder for a route: its flags (bit 2: the steps cost their distance alone; bits 3 and 4: away
// from and near the path finder's focus; bit 5: the positions' own costs; bit 10: no route was found; bits 17-24 rule out paths
// with some flags), the positions it starts and ends at and their chunks' indexes
struct RouteRequest
{
    enum Flags : u32
    {
        FlagDistanceOnly = 0x4,
        FlagAvoidFocus = 0x8,
        FlagNearFocus = 0x10,
        FlagPositionCosts = 0x20,
        FlagNoRoute = 0x400,
    };

    u32 flags;
    u8 unknown04[0x1C - 0x4];
    u16 startPosition;
    u16 endPosition;
    u16 startChunk;
    u16 endChunk;
};
CHECK_OFFSET(RouteRequest, startChunk, 0x20);

// A position the path finder's search reached (16 bytes): it, its chunk's index and its own, the cost of getting there and that
// plus the estimate of what's left
struct SearchEntry
{
    AiPosition* position;
    u16 chunk;
    u16 index;
    f32 cost;
    f32 estimate;
};
CHECK_SIZE(SearchEntry, 0x10);

// The chunk manager's path finder (the base of retail's MiniBigBoi, 0x1240 bytes; vtable 0x1238 bytes in: 1 the destructor, 2 the
// cost of a step along a path between two positions, 3 the estimate between two positions (both their squared distance here), 4
// the search): every chunk's AI navigation by the chunk's index, how many of the indexes it goes through, the search's open list
// (it starts in the middle and grows both ways), which way the last step went along its path, and the request being searched for
struct PathFinder
{
    AiNavigation* navigations[128];
    u16 count;
    u16 unknown202;
    SearchEntry entries[255];
    u8 backwards;
    u8 unknown11F5[0x1200 - 0x11F5];
    // The ends of the route being searched for (the GetShortRoute command sets them)
    Vector4 routeStart;
    Vector4 routeEnd;
    // A point the game's step costs can keep routes near or away from, and its radius
    Vector4 focus;
    f32 focusRadius;
    RouteRequest* request;
    const GccVTableEntry* vtable;
    u32 unknown123C;

    static PathFinder* Construct(PathFinder* finder) RETAIL(FUN_00252750);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002527b0);
    // No navigation for any chunk
    void Clear() RETAIL(FUN_00252818);
    // Every navigation's positions in a box added to a route: the route's count
    u8 CollectInside(const Box* box, Route* route) RETAIL(FUN_00252848);
    // A new search: the stamp moves on, and every position's stamp is made 0 when it would reach 0x8000 (then it's 1)
    void NewSearch() RETAIL(FUN_002528d8);
    // The route a search left from a position (a chunk's index and a position's) back to the start (at most 255 steps), each
    // step's previous one cleared on the way
    Route* MakeRoute(RouteRequest* request, AiPosition* start, u32 chunk, u32 position) RETAIL(FUN_00252960);
    // A route from the request's start to its end (nullptr when there's none or the open list ran out of room: without one the
    // request is flagged): the positions are reached along their links' paths, the cheapest estimate first while it beats the
    // open list's head, else at its tail
    Route* Search(RouteRequest* request) RETAIL(FUN_00242ff8);
    f32 SquaredDistance(const AiPath* path, const Vector4* a, const Vector4* b) RETAIL(FUN_00252ab8);
    f32 SquaredDistanceOf(const Vector4* a, const Vector4* b) RETAIL(FUN_00252a78);
};
CHECK_OFFSET(PathFinder, count, 0x200);
CHECK_OFFSET(PathFinder, routeStart, 0x1200);
CHECK_OFFSET(PathFinder, focus, 0x1220);
CHECK_OFFSET(PathFinder, vtable, 0x1238);
CHECK_SIZE(PathFinder, 0x1240);

// The game's path finder (retail's MiniBigBoi, the chunk manager's): a step's cost goes by the request's flags, the path's and the
// positions' (-1 for a path the request can't take)
struct GamePathFinder : PathFinder
{
    static GamePathFinder* Construct(GamePathFinder* finder) RETAIL(FUN_0017c378);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017c3b0);
    f32 StepCost(const AiPath* path, const AiPosition* from, const AiPosition* to) RETAIL(FUN_00178588);
    f32 Estimate(const Vector4* a, const Vector4* b) RETAIL(FUN_0017c3d8);
};

extern "C"
{
    extern const GccVTableEntry g_AiNavigationVTable[] RETAIL(D_00301318);
    extern const GccVTableEntry g_PathFinderVTable[] RETAIL(D_00302178);
    extern const GccVTableEntry g_GamePathFinderVTable[] RETAIL(MiniBigBoi_Methods);
    // The chunk manager's path finder
    extern PathFinder* g_PathFinder RETAIL(G_MiniBigBoi);
    // The path finder's stamp of its current search
    extern u32 g_PathSearchStamp RETAIL(D_0030A10C);
    // The focus moved across the way from the route's start to its end (along the y axis crossed with the way) by the weight
    // times the distance between them, for the routes kept near it
    void SetNearFocusWeight(PathFinder* finder, f32 weight) RETAIL_N32(FUN_00252500);
}
