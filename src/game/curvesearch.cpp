#include "game/cameras.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"

// The curves' last helpers: stepping a segment's cubic by its forward differences, a path's direction, a spline's samples, its
// segment at a distance and the refining of its nearest point by Brent's method; and the maths module's global constructor

extern "C"
{
    // SplineDistanceSquaredIn's EABI entry, which the minimum search calls
    void SplineDistanceSquaredEntry() RETAIL(FUN_0018f2d8);
}

EABI_EXPORT(FUN_0018ea78, PathDirectionIn);
EABI_EXPORT(FUN_0018f0e8, SplineSegmentAt);
EABI_EXPORT(FUN_0018f2d8, SplineDistanceSquaredIn);

namespace
{
// Brent's golden section step: the share of the larger half (2 less the golden ratio)
constexpr f32 GoldenSection = 0x1.872218p-2f;

// The forward differences of a cubic one step on: each of the first three rows plus the next
void StepDifferences(Vector4* differences)
{
    for (u32 row = 0; row < 3; row++)
    {
        differences[row].x = differences[row].x + differences[row + 1].x;
        differences[row].y = differences[row].y + differences[row + 1].y;
        differences[row].z = differences[row].z + differences[row + 1].z;
    }
}
}

void PathDifferencesStep(const LayoutPath*, Vector4* differences)
{
    StepDifferences(differences);
}

void SplineDifferencesStep(const CameraSpline*, Vector4* differences)
{
    StepDifferences(differences);
}

void PathDirectionIn(f32 into, const LayoutPath* path, Vector4* direction, s32 segment)
{
    PathTangentIn(into, path, direction, segment);
    f32 lengthSquared = LengthSquared(direction);
    f32 inverse = 0.0f;
    if (LengthEpsilon < lengthSquared)
    {
        inverse = 1.0f / Kept(__builtin_sqrtf(lengthSquared));
    }

    direction->x = direction->x * inverse;
    direction->y = direction->y * inverse;
    direction->z = direction->z * inverse;
}

void SplineSamplePoint(const CameraSpline* spline, Vector4* out, s32 segment)
{
    // A sample is its point and its tangent
    *out = spline->samples[segment * 2];
}

void DestroySplineSamples(CameraSpline* spline, u32 destroyFlags)
{
    spline->vtable = g_SplineSamplesVTable;
    if (spline->samples != nullptr)
    {
        MemoryDeallocate_(spline->samples);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(spline);
    }
}

s32 SplineSegmentAt(f32 distance, const CameraSpline* spline, f32* into)
{
    if (distance <= 0.0f)
    {
        *into = 0.0f;
        return 0;
    }

    s32 count = spline->count;
    f32 step = spline->step;
    if (step * static_cast<f32>(count) <= distance)
    {
        *into = step;
        return count - 1;
    }

    // Retail leaves how far into it unset when no segment holds the distance
    if (count <= 0)
    {
        return 0;
    }

    if (distance < step)
    {
        *into = distance;
        return 0;
    }

    f32 end = step;
    for (s32 segment = 1; segment < count; segment++)
    {
        f32 start = end;
        end = step * static_cast<f32>(segment + 1);
        if (distance < end)
        {
            *into = distance - start;
            return segment;
        }
    }

    return 0;
}

void DestroySpline(CameraSpline* spline, u32 destroyFlags)
{
    spline->vtable = g_CameraSplineVTable;
    if (spline->lengths != nullptr)
    {
        MemoryDeallocate_(spline->lengths);
    }

    DestroySplineSamples(spline, destroyFlags);
}

s32 RefineSplineNearest(CameraSpline* spline, f32* into, f32* distanceSquared)
{
    MinimumSearch search;
    search.steps = CurveRefineSteps;
    search.tolerance = CurveRefineTolerance;
    search.closeness = CurveRefineTolerance;
    search.low = 0.0f;
    search.high = 1.0f;
    return FindMinimum(&search, spline, reinterpret_cast<const void*>(&SplineDistanceSquaredEntry), into, distanceSquared, 1);
}

f32 SplineDistanceSquaredIn(f32 into, CameraSpline* spline)
{
    return SplineSegmentDistanceSquared(into, spline);
}

f32 SplineNearestParameter(const CameraSpline* spline, const CurveSearch* search)
{
    return (search->share + static_cast<f32>(search->segment)) / static_cast<f32>(spline->count);
}

s32 FindMinimum(MinimumSearch* search, void* argument, const void* function, f32* at, f32* value, u32 valueKnown)
{
    search->previous = *at;
    search->second = *at;
    if (valueKnown == 0)
    {
        *value = Abi::CallEabi<f32>(function, argument, *at);
    }

    search->previousValue = *value;
    search->secondValue = *value;
    // The step taken and the one before
    f32 step = 0.0f;
    f32 lastStep = 0.0f;
    for (s32 iteration = 1; iteration <= search->steps; iteration++)
    {
        f32 best = *at;
        f32 low = search->low;
        f32 high = search->high;
        f32 doubleTolerance = search->closeness * __builtin_fabsf(best);
        if (doubleTolerance < search->tolerance)
        {
            doubleTolerance = search->tolerance;
        }

        f32 middle = (low + high) * 0.5f;
        f32 tolerance = doubleTolerance * 0.5f;
        if (__builtin_fabsf(best - middle) < doubleTolerance - (high - low) * 0.5f)
        {
            return 0;
        }

        // A parabola through the three best points, when the step before last was large enough
        f32 p = 0.0f;
        f32 q = 0.0f;
        f32 stepBefore = 0.0f;
        if (tolerance < __builtin_fabsf(lastStep))
        {
            f32 bestValue = *value;
            f32 fromSecond = best - search->second;
            f32 fromPrevious = best - search->previous;
            f32 r = fromSecond * (bestValue - search->previousValue);
            q = fromPrevious * (bestValue - search->secondValue);
            p = fromPrevious * q - fromSecond * r;
            q = (q - r) + (q - r);
            if (0.0f < q)
            {
                p = -p;
            }

            q = __builtin_fabsf(q);
            stepBefore = lastStep;
            lastStep = step;
        }

        if (__builtin_fabsf(p) < __builtin_fabsf(q * stepBefore * 0.5f) && q * (low - best) < p && p < q * (high - best))
        {
            step = p / q;
            f32 tried = best + step;
            search->tried = tried;
            if (tried - low < doubleTolerance || high - tried < doubleTolerance)
            {
                step = middle <= *at ? -tolerance : tolerance;
            }
        }
        else
        {
            // A golden section step into the larger part of the bracket
            lastStep = best < middle ? high - best : low - best;
            step = lastStep * GoldenSection;
        }

        if (tolerance <= __builtin_fabsf(step))
        {
            search->tried = *at + step;
        }
        else if (step < 0.0f)
        {
            search->tried = *at - tolerance;
        }
        else
        {
            search->tried = *at + tolerance;
        }

        f32 triedValue = Abi::CallEabi<f32>(function, argument, search->tried);
        search->triedValue = triedValue;
        if (triedValue <= *value)
        {
            if (search->tried < *at)
            {
                search->high = *at;
            }
            else
            {
                search->low = *at;
            }

            search->previous = search->second;
            search->previousValue = search->secondValue;
            search->second = *at;
            search->secondValue = *value;
            *at = search->tried;
            *value = search->triedValue;
            continue;
        }

        if (search->tried < *at)
        {
            search->low = search->tried;
        }
        else
        {
            search->high = search->tried;
        }

        if (search->triedValue <= search->secondValue || search->second == *at)
        {
            search->previous = search->second;
            search->previousValue = search->secondValue;
            search->second = search->tried;
            search->secondValue = search->triedValue;
        }
        else if (search->triedValue <= search->previousValue || search->previous == *at || search->previous == search->second)
        {
            // Retail bug: the point tried takes the third place with the second's value instead of its own, so the next
            // parabola goes through a wrong point
            search->previous = search->tried;
            search->previousValue = search->secondValue;
        }
    }

    return -1;
}

void MathConstantsConstructor()
{
    InitMathConstants(1, DefaultInitPriority);
}
