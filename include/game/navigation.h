#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "gcc2.h"

class Stream;

// An AI position's flags (TT Lab's AiPositionFlags; the searches with flags can ask for some and rule some out): no route goes
// through it, the scripts' NodeIsAirborne condition tests it on a route's step, SetFocusPositionToNearestPoint takes it however
// far it is or never takes it, it's attached to an instance it moves along with (g_AttachedPositionFlag, which AttachToAiPosition
// sets), and flags 3 and 6, which only the scripts give a meaning to (the searches with flags they ask for and their conditions
// read them)
union AiPositionFlags
{
    u16 value;
    struct
    {
        u16 blocked : 1;
        u16 airborne : 1;
        u16 alwaysTaken : 1;
        u16 scriptFlag3 : 1;
        u16 neverTaken : 1;
        u16 attached : 1;
        u16 scriptFlag6 : 1;
        u16 unused7 : 9;
    };

    // The flags' masks: the searches' required and ruled out flags
    enum Mask : u16
    {
        Blocked = 0x1,
        Airborne = 0x2,
        AlwaysTaken = 0x4,
        ScriptFlag3 = 0x8,
        NeverTaken = 0x10,
        Attached = 0x20,
        ScriptFlag6 = 0x40,
    };
};
CHECK_SIZE(AiPositionFlags, 2);

// An AI position's bits: the path finder's stamp of its last search, how many paths link it and its cost for the routes that
// count it
union AiPositionBits
{
    u32 value;
    struct
    {
        u32 stamp : 15;
        u32 linkCount : 5;
        u32 cost : 8;
        u32 unused28 : 4;
    };
};
CHECK_SIZE(AiPositionBits, 4);

// An AI position of a layout (0x20 bytes): where it is (W its radius, which only the condition of the distance to the nearest
// point's edge reads), its bits, the paths linking it, the step a search came to it from (a chunk's index and a position's) and
// its flags
struct AiPosition
{
    // The cost the routes occupying it raise it to at most, and no step before it
    static constexpr u32 MostCost = 0xFF;
    static constexpr u16 NoPrevious = 0xFF;

    Vector4 position;
    AiPositionBits bits;
    struct AiPath** links;
    u16 previousChunk;
    u16 previousPosition;
    AiPositionFlags flags;
    u16 unused1E;

    // Made at the default box's corner (w 1) with no stamp, links nor flags, no previous position
    static AiPosition* Construct(AiPosition* position) RETAIL(FUN_0023ce48);
    // Read from a stream (its position and flags), the stamp the path finder's current one and no links
    void Read(Stream* stream) RETAIL(FUN_0023cf48);
    // Its path to a position (a chunk's index and its own), nullptr when none links them
    struct AiPath* PathTo(u32 chunk, u32 index) RETAIL(FUN_0023cea0);
};
CHECK_OFFSET(AiPosition, flags, 0x1C);
CHECK_SIZE(AiPosition, 0x20);

// An AI path's flags (TT Lab's AiPathFlags): which routes may take it (a route request rules out the paths with some, and the
// plain ones, with none of flags 5-8) and what the scripts' conditions find on the path to a route's step: crossing it takes a
// jump, a long jump, a high jump or flying, and flags 6, 7 and 8, which only the scripts give a meaning to. The tools set bits 0
// and 1 together, which nothing reads
union AiPathFlags
{
    // The flags a plain path has none of
    static constexpr u16 NotPlainMask = 0x1E0;

    u16 value;
    struct
    {
        u16 unused0 : 1;
        u16 unused1 : 1;
        u16 needsJump : 1;
        u16 needsLongJump : 1;
        u16 needsHighJump : 1;
        u16 needsFlight : 1;
        u16 scriptFlag6 : 1;
        u16 scriptFlag7 : 1;
        u16 scriptFlag8 : 1;
        u16 unused9 : 7;
    };

    // The flags' masks (the scripts' conditions test one each)
    enum Mask : u16
    {
        NeedsJump = 0x4,
        NeedsLongJump = 0x8,
        NeedsHighJump = 0x10,
        NeedsFlight = 0x20,
        ScriptFlag6 = 0x40,
        ScriptFlag7 = 0x80,
        ScriptFlag8 = 0x100,
    };
};
CHECK_SIZE(AiPathFlags, 2);

// An AI position's index of none (and a chunk's, an AI path's until its chunk's navigation is linked)
constexpr u16 NoAiIndex = 0xFFFF;

// An AI path of a layout (0xA bytes): the two positions it joins (their indexes), its flags, and the chunks the positions are in
// (the path's own, set when the chunk's navigation is linked)
struct AiPath
{
    u16 positionA;
    u16 positionB;
    AiPathFlags flags;
    u16 chunkA;
    u16 chunkB;

    void Read(Stream* stream) RETAIL(FUN_002526a0);
};
CHECK_SIZE(AiPath, 0xA);

// A route of AI positions the path finder finds or collects (0x402 bytes): each step's chunk index and position index, and the
// count of steps (a search's route has 255 at most)
struct Route
{
    static constexpr u32 MostSearchedSteps = 255;

    u16 chunks[256];
    u16 positions[256];
    u8 count;
    u8 unused401;

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

// A route request's flags: bit 1, which GetShortRoute sets with a weight and nothing reads, the steps costing their distance
// alone, kept away from and near the path finder's focus, the positions' own costs counted, no route found (the search sets
// it), and the paths it rules out (GamePathFinder::StepCost): those needing a jump (the high and long jumps' bits only read
// without it), a high jump, a long jump, flying, those with AI path flags 6, 7 and 8, and the plain ones (none of flags 5-8)
union RouteRequestFlags
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 unused1 : 1;
        u32 distanceOnly : 1;
        u32 avoidsFocus : 1;
        u32 nearFocus : 1;
        u32 positionCosts : 1;
        u32 unused6 : 4;
        u32 noRoute : 1;
        u32 unused11 : 6;
        u32 rulesOutJumps : 1;
        u32 rulesOutHighJumps : 1;
        u32 rulesOutLongJumps : 1;
        u32 rulesOutFlights : 1;
        u32 rulesOutScriptFlag6 : 1;
        u32 rulesOutScriptFlag7 : 1;
        u32 rulesOutScriptFlag8 : 1;
        u32 rulesOutPlainPaths : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(RouteRequestFlags, 4);

// What asks the path finder for a route (GetShortRoute's, on its stack): its flags, what GetShortRoute gives besides (the kind
// byte its end flags give, a radius (the agent's roll radius), its weight and the weights of keeping away from the path finder's
// focus, of keeping near it and of the positions' own costs: nothing reads them but GetShortRoute itself the near focus weight,
// the step costs weigh by 100), the positions it starts and ends at and their chunks' indexes
struct RouteRequest
{
    RouteRequestFlags flags;
    u8 unused04;
    u8 unused05[3];
    f32 unused08;
    f32 unused0C;
    f32 unused10;
    f32 nearFocusWeight;
    f32 unused18;
    u16 startPosition;
    u16 endPosition;
    u16 startChunk;
    u16 endChunk;
};
CHECK_OFFSET(RouteRequest, nearFocusWeight, 0x14);
CHECK_OFFSET(RouteRequest, startPosition, 0x1C);
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
// (it starts in the middle and grows both ways), which way the last step went along its path (1 from its position A to B, which
// nothing reads), and the request being searched for
struct PathFinder
{
    // The chunks' indexes it keeps a navigation for, and its vtable's step cost, estimate and search
    static constexpr u32 MostChunks = 128;
    static constexpr u32 StepCostSlot = 2;
    static constexpr u32 EstimateSlot = 3;
    static constexpr u32 FindRouteSlot = 4;

    AiNavigation* navigations[MostChunks];
    u16 count;
    u16 unused202;
    SearchEntry entries[255];
    u8 unused11F4;
    u8 unused11F5[0x1200 - 0x11F5];
    // The ends of the route being searched for (the GetShortRoute command sets them)
    Vector4 routeStart;
    Vector4 routeEnd;
    // A point the game's step costs can keep routes near or away from, and its radius
    Vector4 focus;
    f32 focusRadius;
    RouteRequest* request;
    const GccVTableEntry* vtable;
    u32 unused123C;

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
