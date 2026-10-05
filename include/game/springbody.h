#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "game/math.h"
#include "gcc2.h"

struct SpringBody;
struct TimeClock;

// The spring bodies the SpringSkeletons pose their joints by (the graple's rope): points of a mass joined by springs, moved by
// their springs' forces and the gravity a few passes a step

// A point (0x40 bytes): its mass, the share of its velocity it loses a second, whether it's pinned (it doesn't move and has no
// force), where it is, its velocity and the force on it this pass (their w 1)
struct SpringPoint
{
    f32 mass;
    f32 drag;
    u8 pinned;
    u8 unused09[7];
    Vector4 position;
    Vector4 velocity;
    Vector4 force;
};
CHECK_OFFSET(SpringPoint, position, 0x10);
CHECK_SIZE(SpringPoint, 0x40);

// A spring between two points (0x14 bytes, no vtable): the length it pulls them to, its stiffness and its damping
struct Spring
{
    SpringPoint* start;
    SpringPoint* end;
    f32 restLength;
    f32 stiffness;
    f32 damping;
};
CHECK_SIZE(Spring, 0x14);

// A chain of springs (0x20 bytes; retail vtable D_002F5D30 over its base's D_002F5F28, the springs it owns: 1 the destructor):
// its springs, how many segments it was made of, its first point and its last
class SpringChain
{
public:
    static constexpr u32 DestroySlot = 1;

    PointerArray<Spring> springs;
    const GccVTableEntry* vtable;
    u32 segments;
    SpringPoint* first;
    SpringPoint* last;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00191b90);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0018f7d8);
    // Every spring's force put on its points
    void ApplySprings() RETAIL(FUN_0018f8c8);
    // Made of points (its segments and one more, of mass 1) from a start along a vector, each new one added to a body and joined
    // to the one before by a spring of the segments' length
    void Build(const Vector4* along, const Vector4* start, SpringBody* body) RETAIL(FUN_00190b90);
};
CHECK_OFFSET(SpringChain, vtable, 0x10);
CHECK_SIZE(SpringChain, 0x20);

// A body (0x40 bytes, no vtable): the passes a step takes (one per 50th of a second for a longer step), an axis its points keep
// from moving along (none when it's within 5e-05 of 0 on each axis), its points and its chains (it owns both)
struct SpringBody
{
    u32 passes;
    u8 unused04[0xC];
    Vector4 fixedAxis;
    PointerArray<SpringPoint> points;
    PointerArray<SpringChain> chains;
};
CHECK_OFFSET(SpringBody, fixedAxis, 0x10);
CHECK_OFFSET(SpringBody, points, 0x20);
CHECK_SIZE(SpringBody, 0x40);

extern "C"
{
    extern const GccVTableEntry g_SpringChainVTable[] RETAIL(D_002F5D30);
    extern const GccVTableEntry g_SpringListVTable[] RETAIL(D_002F5F28);
    // The iterators' bases (array.h): over a chain's springs, a body's points and its chains
    extern const GccVTableEntry g_SpringsIteratorBaseVTable[] RETAIL(D_002F5EF0);
    extern const GccVTableEntry g_PointsIteratorBaseVTable[] RETAIL(D_002F5E60);
    extern const GccVTableEntry g_ChainsIteratorBaseVTable[] RETAIL(D_002F5DD0);

    // A point stopped: no velocity and no force (their w 1)
    void StopSpringPoint(SpringPoint* point) RETAIL(FUN_00191580);
    // A spring made between two points (stiffness 1000, no damping), and its destructor
    Spring* ConstructSpring(f32 restLength, Spring* spring, SpringPoint* start, SpringPoint* end) RETAIL_N32(FUN_00191520);
    void DestroySpring(Spring* spring, u32 destroyFlags) RETAIL(FUN_00191548);
    // A spring's force (its stretch times its stiffness and the speed its ends part at times its damping, along it) taken off its
    // end's force and added to its start's; none when its ends are within 0.001 of each other
    void ApplySpring(Spring* spring) RETAIL(FUN_0018f960);

    // A chain made of a count of segments in a body (from the origin downwards, 1 long), and given its own springs' stiffness and
    // damping
    SpringChain* ConstructSpringChain(void* memory, u32 segments, SpringBody* body) RETAIL(FUN_00191ac8);
    void SetChainSprings(f32 stiffness, f32 damping, SpringChain* chain) RETAIL_N32(FUN_00191470);
    // Its first point pinned at a point (stopped) or let go (none)
    void PinChainStart(SpringChain* chain, const Vector4* point) RETAIL(FUN_00191bb8);
    // Its points laid evenly from a start to an end, stopped, its springs' rest length a segment's
    void LayChain(SpringChain* chain, const Vector4* start, const Vector4* end) RETAIL(FUN_00191130);

    // A chain added to a body, and every point's drag set
    void AddSpringChain(SpringBody* body, SpringChain* chain) RETAIL(FUN_00191828);
    void SetPointsDrag(f32 drag, SpringBody* body) RETAIL_N32(FUN_00191780);
    // The forces a pass starts from: none on pinned points, the others' gravity times their mass (none without one) and a force
    // when there's one
    void StartSpringForces(SpringBody* body, const Vector4* force, const Vector4* gravity) RETAIL(FUN_0018fc88);
    // A point moved by its force over a time (its velocity kept off an axis when there's one), then slowed by its drag (stopped
    // when that's all of it), and every point of a body that isn't pinned moved
    void MoveSpringPoint(f32 seconds, SpringPoint* point, const Vector4* fixedAxis) RETAIL_N32(FUN_0018fb18);
    void MoveSpringPoints(f32 seconds, SpringBody* body) RETAIL_N32(FUN_0018fe08);
    // A body stepped by its clock's last advance (when it runs): each pass the forces started, every chain's springs applied and
    // the points moved
    void SimulateSpringBody(SpringBody* body, TimeClock* clock, const Vector4* force, const Vector4* gravity) RETAIL(FUN_001900c8);
    // Its points freed and its chains destroyed (the body freed too when the flags say)
    void DestroySpringBody(SpringBody* body, u32 destroyFlags) RETAIL(FUN_0018ff28);
}
