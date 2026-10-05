#include "game/ik.h"

#include "game/memory.h"

// The maths module's IK solver: links added to a solver and a chain's link given its limits, a link's angle kept within its
// limits, the solver's steps (cyclic coordinate descent: from the last link to the first, each turned so that the chain's end
// points at the target, until it's near enough) and a turn scaled by a share

EABI_EXPORT(FUN_00182680, KeepIkLinkWithinLimits);
EABI_EXPORT(FUN_00182b08, StepIkSolver);
EABI_EXPORT(FUN_001847b0, ScaleIkTurn);

namespace
{
constexpr f32 TwoOverPi = 0x1.45f306p-1f;
constexpr f32 SquareRootOfHalf = 0x1.6a09e6p-1f;
// How far off a unit pair a multiple's sine and cosine may be before they're made one again
constexpr f32 MultipleTolerance = 0x1.5798ecp-29f;

f32 InverseUnlessWithin(f32 value, f32 epsilon)
{
    if (epsilon < value || value < -epsilon)
    {
        return 1.0f / value;
    }

    return 0.0f;
}

f32 InverseLengthOf(f32 x, f32 y)
{
    return InverseUnlessWithin(Kept(__builtin_sqrtf(x * x + y * y)), InverseEpsilon);
}

// A limit's sine and cosine of its angle (radians), made a unit pair unless they're near enough to one
void LimitSinCos(f32 radians, f32* sine, f32* cosine)
{
    s32 angle;
    AngleFrom(&angle, radians, AngleRadians);
    CosSin16(&angle, cosine, sine);
    f32 off = *cosine * *cosine + *sine * *sine - 1.0f;
    if (!(off * off <= IkTurnTolerance))
    {
        NormalizePair(sine, cosine, IkTurnTolerance);
    }
}

// UpdateIkLink of a link and every one after it
void UpdateIkLinksFrom(IkLink* link)
{
    for (; link != nullptr; link = link->next)
    {
        link->planeCosine = link->cosine;
        link->planeSine = link->sine;
        if (link->previous == nullptr)
        {
            continue;
        }

        TurnPair(&link->planeSine, &link->planeCosine, link->previous->planeSine, link->previous->planeCosine, IkTurnTolerance);
        const IkLink* previous = link->previous;
        link->x = previous->length * previous->planeCosine + previous->x;
        link->y = previous->length * previous->planeSine + previous->y;
    }
}

// A link turned from one direction from its start to another (unit ones), by the sine and cosine of the angle between them made
// a unit pair (no turn when they're too short)
void TurnIkLinkBetween(IkLink* link, f32 fromX, f32 fromY, f32 toX, f32 toY)
{
    f32 sine = fromX * toY - fromY * toX;
    f32 cosine = fromX * toX + fromY * toY;
    f32 inverse = InverseUnlessWithin(Kept(__builtin_sqrtf(cosine * cosine + sine * sine)), InverseEpsilon);
    if (0.0f < inverse)
    {
        TurnIkLink(link, &link->sine, &link->cosine, sine * inverse, cosine * inverse);
    }
}

// The sine and the cosine of an angle (65536ths of a turn), each of its own CosSin16
f32 SineOf(const s32* angle)
{
    f32 cosine;
    f32 sine;
    CosSin16(angle, &cosine, &sine);
    return sine;
}

f32 CosineOf(const s32* angle)
{
    f32 cosine;
    f32 sine;
    CosSin16(angle, &cosine, &sine);
    return cosine;
}
}

void SetIkLimits(void* chain, u32 linkNumber, const f32* low, const f32* high)
{
    IkLink* limited = static_cast<IkChain*>(chain)->solver.root.next;
    for (s32 skipped = 1; skipped < static_cast<s32>(linkNumber); skipped++)
    {
        limited = limited->next;
    }

    f32 sine;
    f32 cosine;
    if (low != nullptr)
    {
        LimitSinCos(*low, &sine, &cosine);
        SetIkLowLimit(limited, sine, cosine);
    }
    else
    {
        SetIkLowLimit(limited, 0.0f, 1.0f);
    }

    if (high != nullptr)
    {
        LimitSinCos(*high, &sine, &cosine);
        SetIkHighLimit(limited, sine, cosine);
    }
    else
    {
        SetIkHighLimit(limited, 0.0f, 1.0f);
    }
}

void KeepIkLinkWithinLimits(IkLink* link, f32* sine, f32* cosine, f32 tolerance)
{
    if (link->limited == 0)
    {
        return;
    }

    // The sines of the angles from the angle to the high limit and to the low one: within them, it's kept as it is
    f32 toHigh = *cosine * link->highSine - *sine * link->highCosine;
    if (-tolerance <= toHigh && *cosine * link->lowSine - *sine * link->lowCosine <= tolerance)
    {
        return;
    }

    // The middle between the limits: their sum over twice the cosine of half the angle between them (the other way round when
    // they're more than half a turn apart), a quarter turn on from the low one when they're half a turn apart
    f32 middleSine;
    f32 middleCosine;
    f32 cosinePlusOne = link->lowCosine * link->highCosine + link->lowSine * link->highSine + 1.0f;
    f32 scale = __builtin_sqrtf(InverseUnlessWithin(cosinePlusOne, tolerance)) * SquareRootOfHalf;
    if (0.0f < scale)
    {
        middleSine = (link->lowSine + link->highSine) * scale;
        middleCosine = (link->lowCosine + link->highCosine) * scale;
        if (link->lowCosine * link->highSine - link->lowSine * link->highCosine < -tolerance)
        {
            middleSine = -middleSine;
            middleCosine = -middleCosine;
        }
    }
    else
    {
        middleSine = link->lowCosine;
        middleCosine = -link->lowSine;
    }

    // Outside them it goes to the nearer one: the low one between it and the middle's opposite, the high one between that and
    // it, the middle itself at the opposite
    f32 s = *sine;
    f32 c = *cosine;
    f32 toOpposite = c * -middleSine - s * -middleCosine;
    bool atOpposite = -tolerance <= toOpposite && toOpposite <= tolerance;
    if (!atOpposite)
    {
        if (-tolerance <= c * link->lowSine - s * link->lowCosine && toOpposite <= tolerance)
        {
            *sine = link->lowSine;
            *cosine = link->lowCosine;
            return;
        }

        if (-tolerance <= toOpposite && c * link->highSine - s * link->highCosine <= tolerance)
        {
            *sine = link->highSine;
            *cosine = link->highCosine;
            return;
        }
    }

    *sine = middleSine;
    *cosine = middleCosine;
}

void AddIkLinks(IkSolver* solver, u32 count, const f32* lengths, const f32* angles)
{
    // The first link added has none before it (the root, of no length and no angle, would change nothing)
    IkLink* previous = nullptr;
    for (s32 index = 0; index < static_cast<s32>(count); index++)
    {
        f32 length = lengths[index];
        f32 radians = angles != nullptr ? angles[index] : 0.0f;
        IkLink* last = &solver->root;
        while (last->next != nullptr)
        {
            last = last->next;
        }

        auto* link = static_cast<IkLink*>(MemoryAllocate(sizeof(IkLink)));
        link->x = 0.0f;
        link->previous = previous;
        link->length = length;
        link->vtable = g_IkLinkVTable;
        link->next = nullptr;
        link->y = 0.0f;
        link->limited = 0;
        link->turnScaled = 0;
        link->turnShare = 1.0f;
        s32 angle;
        AngleFrom(&angle, radians, AngleRadians);
        CosSin16(&angle, &link->cosine, &link->sine);
        UpdateIkLink(link);
        last->next = link;
        previous = link;
    }

    solver->linkCount = CountIkLinks(solver);
}

s32 StepIkSolver(IkSolver* solver, u32 steps, f32 tolerance)
{
    s32 step = 0;
    // Retail reads the first link's start before it checks there's one
    IkLink* first = solver->root.next;
    f32 toTargetX = solver->targetX - first->x;
    f32 toTargetY = solver->targetY - first->y;
    f32 distanceSquared = toTargetX * toTargetX + toTargetY * toTargetY;
    f32 reach = 0.0f;
    IkLink* last = first;
    for (IkLink* link = first; link != nullptr; link = link->next)
    {
        reach = reach + link->length;
        last = link;
    }

    if (distanceSquared < reach * reach)
    {
        IkLink* link = last;
        while (link != nullptr && step < static_cast<s32>(steps))
        {
            f32 endX = last->length * last->planeCosine + last->x;
            solver->endX = endX;
            f32 endY = last->length * last->planeSine + last->y;
            solver->endY = endY;
            f32 targetX = solver->targetX;
            f32 targetY = solver->targetY;
            f32 missX = targetX - endX;
            f32 missY = targetY - endY;
            if (missX * missX + missY * missY <= tolerance * (targetX * endX + targetY * endY))
            {
                break;
            }

            // The link turned from where its start sees the end to where it sees the target
            f32 toX = targetX - link->x;
            f32 toY = targetY - link->y;
            f32 fromX = endX - link->x;
            f32 fromY = endY - link->y;
            f32 toInverse = InverseLengthOf(toX, toY);
            f32 fromInverse = InverseLengthOf(fromX, fromY);
            TurnIkLinkBetween(link, fromX * fromInverse, fromY * fromInverse, toX * toInverse, toY * toInverse);
            NormalizePair(&link->sine, &link->cosine, IkTurnTolerance);
            NormalizePair(&link->planeSine, &link->planeCosine, IkTurnTolerance);
            UpdateIkLinksFrom(link);
            if (link != solver->root.next)
            {
                link = link->previous;
            }
            else
            {
                step++;
                link = last;
            }
        }
    }
    else
    {
        // Out of reach: the first link turned toward the target, the others straight on
        IkLink* link = first;
        f32 alongX = solver->targetX - link->x;
        f32 alongY = solver->targetY - link->y;
        f32 sine = -alongX * link->planeSine + alongY * link->planeCosine;
        f32 cosine = alongX * link->planeCosine + alongY * link->planeSine;
        f32 inverse = InverseUnlessWithin(Kept(__builtin_sqrtf(cosine * cosine + sine * sine)), InverseEpsilon);
        if (0.0f < inverse)
        {
            TurnIkLink(link, &link->sine, &link->cosine, sine * inverse, cosine * inverse);
        }

        for (IkLink* straight = link->next; straight != nullptr; straight = straight->next)
        {
            straight->cosine = 1.0f;
            straight->sine = 0.0f;
        }
    }

    UpdateIkLinksFrom(solver->root.next);
    solver->endX = last->length * last->planeCosine + last->x;
    solver->endY = last->length * last->planeSine + last->y;
    return step;
}

void SinCosTimes(f32* sine, f32* cosine, u32 times)
{
    switch (times)
    {
    case 0:
        *sine = 0.0f;
        *cosine = 1.0f;
        break;
    case 2:
    {
        f32 twiceCosine = *cosine + *cosine;
        *sine = *sine * twiceCosine;
        *cosine = *cosine * twiceCosine - 1.0f;
        break;
    }
    case 3:
    {
        f32 s = *sine;
        *sine = s * -4.0f * (s * s) + s * 3.0f;
        f32 c = *cosine;
        *cosine = c * 4.0f * (c * c) - c * 3.0f;
        break;
    }
    case 4:
    {
        f32 s = *sine;
        f32 c = *cosine;
        f32 fourSineCosine = c * 4.0f * s;
        f32 cosineSquared = c * c;
        *sine = fourSineCosine - (fourSineCosine + fourSineCosine) * (s * s);
        *cosine = (cosineSquared * cosineSquared - cosineSquared) * 8.0f + 1.0f;
        NormalizePair(sine, cosine, MultipleTolerance);
        break;
    }
    default:
        break;
    }
}

void ScaleIkTurn(f32 share, f32* sine, f32* cosine, s32 terms)
{
    // Twice as many terms (retail's shift, wrapping round)
    s32 count = static_cast<s32>(static_cast<u32>(terms) << 1);
    f32 shareSquared = share * share;
    s32 angle;
    AngleOfPoint(&angle, *sine, *cosine);
    f32 multipleSine = *sine;
    f32 multipleCosine = *cosine;
    // Retail bug: a series of the turn's multiples is summed and normalized, then thrown away for the sine and cosine of the angle
    // times the share (and its multiples are of the one before, not of the turn: 1, 2, 6 and 24 times it)
    s32 shareOfHalfTurn;
    AngleFrom(&shareOfHalfTurn, share * Pi, AngleRadians);
    f32 shareCosine;
    f32 shareSine;
    CosSin16(&shareOfHalfTurn, &shareCosine, &shareSine);
    f32 scale = shareSine * TwoOverPi;
    *sine = 0.0f;
    *cosine = 0.5f / share;
    f32 sign = 1.0f;
    for (s32 term = 1; term <= count; term++)
    {
        SinCosTimes(&multipleSine, &multipleCosine, term);
        f32 inverse = 1.0f / (static_cast<f32>(static_cast<s32>(static_cast<u32>(term) * term)) - shareSquared);
        *sine = *sine + sign * (static_cast<f32>(term) * multipleSine * inverse);
        *cosine = *cosine + sign * (share * multipleCosine * inverse);
        sign = -sign;
    }

    *sine = *sine * scale;
    *cosine = *cosine * scale;
    NormalizePair(sine, cosine, MultipleTolerance);
    s32 scaled = static_cast<s32>(static_cast<f32>(angle) * share);
    *sine = SineOf(&scaled);
    *cosine = CosineOf(&scaled);
}
