#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "gcc2.h"

// The maths module's IK chains (the characters' arms): links in a plane, each at an angle from the one before (within limits
// when it has them), turned until the chain's end reaches a target; a chain in 3D keeps them in the upright plane through its
// start toward its end

// How far off a unit pair a link's sine and cosine may get (turned, or a limit's) before they're made one again
constexpr f32 IkTurnTolerance = 0x1.0624dep-11f;

// A link (0x44 bytes, vtable 0x40 bytes in: 1 the destructor): the next link and the one before (none for the first), where it
// starts in the plane, its length, its angle from the one before's (cosine and sine) and in the plane, whether it has limits and
// their angles' cosines and sines (the low one's and the high one's), and whether its turns are scaled and by what (made 1;
// nothing sets them)
struct IkLink
{
    static constexpr u32 DestroySlot = 1;

    IkLink* next;
    IkLink* previous;
    f32 x;
    f32 y;
    f32 length;
    f32 cosine;
    f32 sine;
    f32 planeCosine;
    f32 planeSine;
    u8 limited;
    u8 unused25[3];
    f32 lowCosine;
    f32 lowSine;
    f32 highCosine;
    f32 highSine;
    u8 turnScaled;
    u8 unused39[3];
    f32 turnShare;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(IkLink, limited, 0x24);
CHECK_OFFSET(IkLink, turnShare, 0x3C);
CHECK_SIZE(IkLink, 0x44);

// A chain's solver (0x5C bytes, its own vtable 0x58 bytes in: 1 the destructor, which destroys its links too): a link of no length
// at the origin the chain's links follow, where the chain's end got to, how many links there are and the target
struct IkSolver
{
    IkLink root;
    f32 endX;
    f32 endY;
    s32 linkCount;
    f32 targetX;
    f32 targetY;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(IkSolver, linkCount, 0x4C);
CHECK_OFFSET(IkSolver, vtable, 0x58);
CHECK_SIZE(IkSolver, 0x5C);

// A chain in 3D (0x90 bytes): its start and its end, the flat direction from the start toward the end (a unit one) and the solver,
// whose plane's x goes along that direction and its y up
struct IkChain
{
    Vector4 start;
    Vector4 end;
    Vector4 direction;
    IkSolver solver;
};
CHECK_OFFSET(IkChain, solver, 0x30);
CHECK_SIZE(IkChain, 0x90);

extern "C"
{
    extern const GccVTableEntry g_IkLinkVTable[] RETAIL(D_002F5CF8);
    extern const GccVTableEntry g_IkSolverVTable[] RETAIL(D_002F5CE0);

    // A chain's solver made (no links yet; the solver), the chain's links made and added (lengths and angles in radians, nothing
    // without angles), its ends set (the target where the end is in its plane), solved (by so many of the solver's steps, to a
    // tolerance: the solver's result) into the points its links end at, and destroyed (its links too)
    IkSolver* ConstructIkSolver(void* solver) RETAIL(FUN_0018bd30);
    void SetIkLinks(void* chain, u32 count, const f32* lengths, const f32* angles) RETAIL(FUN_0018b860);
    void SetIkEnds(void* chain, const Vector4* start, const Vector4* end) RETAIL(FUN_0018b880);
    s32 SolveIk(void* chain, Vector4* points, u32 steps, f32 tolerance) RETAIL_N32(FUN_0018ba20);
    void DestroyIkChain(void* chain, u32 destroyFlags) RETAIL(FUN_0018b960);
    // Where a point is in the chain's plane: how far from its start along its direction, and how high
    void IkPlanePoint(const IkChain* chain, f32* along, f32* height, const Vector4* point) RETAIL(FUN_0018b9b0);

    // A link's angle in the plane and its start worked out from the one before's, its angle turned by a turn's sine and cosine (and
    // kept within its limits), its limits set (an angle's sine and cosine each), the solver's links counted, the solver's steps run,
    // and the destructors
    void UpdateIkLink(IkLink* link) RETAIL(FUN_0018bb98);
    void TurnIkLink(IkLink* link, f32* sine, f32* cosine, f32 turnSine, f32 turnCosine) RETAIL_N32(FUN_0018bc30);
    void SetIkLowLimit(IkLink* link, f32 sine, f32 cosine) RETAIL_N32(FUN_0018bcf8);
    void SetIkHighLimit(IkLink* link, f32 sine, f32 cosine) RETAIL_N32(FUN_0018bd10);
    s32 CountIkLinks(const IkSolver* solver) RETAIL(FUN_0018be40);
    s32 RunIkSolver(IkSolver* solver, u32 steps, f32 tolerance) RETAIL_N32(FUN_0018be70);
    void DestroyIkLink(IkLink* link, u32 destroyFlags) RETAIL(FUN_0018b820);
    void DestroyIkSolver(IkSolver* solver, u32 destroyFlags) RETAIL(FUN_0018bda8);

    // The links made and added after the solver's last, a chain's link's angle limits set (radians, links from 1; nothing is no
    // limit: 0), the solver's steps (how many it took: the result RunIkSolver gives), a turn's sine and cosine scaled by a share
    // (the angle's 65536ths of a turn times it), a link's angle (its sine and cosine) kept within its limits to a tolerance, and
    // an angle's sine and cosine made those of 0 to 4 times it (nothing for more)
    void AddIkLinks(IkSolver* solver, u32 count, const f32* lengths, const f32* angles) RETAIL(FUN_001828e0);
    void SetIkLimits(void* chain, u32 linkNumber, const f32* low, const f32* high) RETAIL(FUN_001824b0);
    s32 StepIkSolver(IkSolver* solver, u32 steps, f32 tolerance) RETAIL_N32(FUN_00182b08);
    void ScaleIkTurn(f32 share, f32* sine, f32* cosine, s32 terms) RETAIL_N32(FUN_001847b0);
    void KeepIkLinkWithinLimits(IkLink* link, f32* sine, f32* cosine, f32 tolerance) RETAIL_N32(FUN_00182680);
    void SinCosTimes(f32* sine, f32* cosine, u32 times) RETAIL(FUN_00184660);
}
