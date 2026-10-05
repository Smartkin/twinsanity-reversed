#include "game/ik.h"

#include "game/memory.h"

// The maths module's IK chains: a chain's ends taken into its plane and its solved links' ends taken back out of it, the solver
// made and destroyed with its links, a link's angle in the plane worked out, turned and limited

EABI_EXPORT(FUN_0018ba20, SolveIk);
EABI_EXPORT(FUN_0018bc30, TurnIkLink);
EABI_EXPORT(FUN_0018bcf8, SetIkLowLimit);
EABI_EXPORT(FUN_0018bd10, SetIkHighLimit);
EABI_EXPORT(FUN_0018be70, RunIkSolver);

namespace
{
// How far off its limits a turned link's angle may be (sines)
constexpr f32 LimitTolerance = Epsilon;
// The terms the scaling of a turn takes
constexpr s32 TurnScaleTerms = 2;

// A point of the chain's plane in 3D: along the chain's direction from its start, and up
void PlanePointInWorld(const IkChain* chain, f32 x, f32 y, Vector4* out)
{
    *out = chain->direction;
    out->x = out->x * x;
    out->y = out->y * x;
    out->z = out->z * x;
    out->y = out->y + y;
    out->x = out->x + chain->start.x;
    out->y = out->y + chain->start.y;
    out->z = out->z + chain->start.z;
}
}

void DestroyIkLink(IkLink* link, u32 destroyFlags)
{
    link->vtable = g_IkLinkVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(link);
    }
}

void SetIkLinks(void* chain, u32 count, const f32* lengths, const f32* angles)
{
    if (angles != nullptr)
    {
        AddIkLinks(&static_cast<IkChain*>(chain)->solver, count, lengths, angles);
    }
}

void SetIkEnds(void* chain, const Vector4* start, const Vector4* end)
{
    auto* ik = static_cast<IkChain*>(chain);
    ik->start = *start;
    ik->end = *end;
    Vector4* direction = &ik->direction;
    *direction = *end;
    direction->x = direction->x - start->x;
    direction->y = 0.0f;
    direction->z = direction->z - start->z;
    f32 inverse = InverseLength(direction, LengthEpsilon);
    direction->x = direction->x * inverse;
    direction->y = direction->y * inverse;
    direction->z = direction->z * inverse;
    IkPlanePoint(ik, &ik->solver.targetX, &ik->solver.targetY, end);
}

void DestroyIkChain(void* chain, u32 destroyFlags)
{
    DestroyIkSolver(&static_cast<IkChain*>(chain)->solver, DestroyOnly);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(chain);
    }
}

void IkPlanePoint(const IkChain* chain, f32* along, f32* height, const Vector4* point)
{
    f32 x = point->x - chain->start.x;
    f32 y = point->y - chain->start.y;
    f32 z = point->z - chain->start.z;
    *along = x * chain->direction.x + y * chain->direction.y + z * chain->direction.z;
    *height = y;
}

s32 SolveIk(void* chain, Vector4* points, u32 steps, f32 tolerance)
{
    auto* ik = static_cast<IkChain*>(chain);
    s32 result = RunIkSolver(&ik->solver, steps, tolerance);
    // Where each link meets the next, then the end
    s32 joints = 0;
    for (const IkLink* link = ik->solver.root.next; link->next != nullptr; link = link->next)
    {
        const IkLink* next = link->next;
        PlanePointInWorld(ik, next->x, next->y, &points[joints]);
        joints++;
    }

    PlanePointInWorld(ik, ik->solver.endX, ik->solver.endY, &points[joints]);
    return result;
}

void UpdateIkLink(IkLink* link)
{
    link->planeCosine = link->cosine;
    link->planeSine = link->sine;
    const IkLink* previous = link->previous;
    if (previous == nullptr)
    {
        return;
    }

    // Its angle in the plane is the one before's plus its own, and it starts where the one before ends
    TurnPair(&link->planeSine, &link->planeCosine, previous->planeSine, previous->planeCosine, IkTurnTolerance);
    link->x = previous->length * previous->planeCosine + previous->x;
    link->y = previous->length * previous->planeSine + previous->y;
}

void TurnIkLink(IkLink* link, f32* sine, f32* cosine, f32 turnSine, f32 turnCosine)
{
    f32 scaled[2] = {turnSine, turnCosine};
    if (link->turnScaled != 0)
    {
        // Retail bug: the turn is scaled in a copy it doesn't turn by, the link takes the whole turn (no link scales its turns)
        ScaleIkTurn(link->turnShare, &scaled[0], &scaled[1], TurnScaleTerms);
    }

    TurnPair(sine, cosine, turnSine, turnCosine, IkTurnTolerance);
    KeepIkLinkWithinLimits(link, sine, cosine, LimitTolerance);
}

void SetIkLowLimit(IkLink* link, f32 sine, f32 cosine)
{
    link->lowSine = sine;
    link->limited = 1;
    link->lowCosine = cosine;
}

void SetIkHighLimit(IkLink* link, f32 sine, f32 cosine)
{
    link->highSine = sine;
    link->limited = 1;
    link->highCosine = cosine;
}

IkSolver* ConstructIkSolver(void* memory)
{
    auto* solver = static_cast<IkSolver*>(memory);
    IkLink* root = &solver->root;
    root->next = nullptr;
    root->previous = nullptr;
    root->x = 0.0f;
    root->y = 0.0f;
    root->length = 0.0f;
    root->sine = 0.0f;
    root->limited = 0;
    root->turnScaled = 0;
    solver->vtable = g_IkSolverVTable;
    root->vtable = g_IkLinkVTable;
    root->turnShare = 1.0f;
    root->cosine = 1.0f;
    UpdateIkLink(root);
    solver->linkCount = 0;
    return solver;
}

void DestroyIkSolver(IkSolver* solver, u32 destroyFlags)
{
    solver->vtable = g_IkSolverVTable;
    IkLink* link = solver->root.next;
    while (link != nullptr)
    {
        IkLink* next = link->next;
        CallVirtual<void>(link, link->vtable, IkLink::DestroySlot, DestroyAndFree);
        link = next;
    }

    solver->root.vtable = g_IkLinkVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(solver);
    }
}

s32 CountIkLinks(const IkSolver* solver)
{
    s32 count = 0;
    for (const IkLink* link = solver->root.next; link != nullptr; link = link->next)
    {
        count++;
    }

    return count;
}

s32 RunIkSolver(IkSolver* solver, u32 steps, f32 tolerance)
{
    return StepIkSolver(solver, steps, tolerance);
}
