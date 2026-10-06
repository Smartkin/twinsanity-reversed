#include "game/cameras.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/stream.h"

// The curves' maths: the layouts' paths (uniform cubic B-splines through their points, walked by a share of a segment or by its
// forward differences) and the camera splines (cubic Hermite segments between samples of a point and a tangent): their points
// and tangents, their nearest points to a point (the segments around the nearest control point walked in steps, the best share
// refined by Brent's method) and the shares along them; a spline read; and a camera zone's box: a point's fractions within it and
// the point at fractions

extern "C"
{
    // A uniform cubic B-spline's basis: the weights of a segment's four points are (t³, t², t, 1) times it. Retail's function-local
    // static: made the first time a path's weights are worked out, and a flag set then too that nothing reads
    extern Matrix4x4 g_PathBasis RETAIL(D_00323810);
    extern u32 g_PathBasisUsed RETAIL(D_0030A7E0);
    extern u8 g_PathBasisMade RETAIL(D_0030A7E4);

    // A segment's four points weighted (the w 1; the curve unused); the weights of a path's four points a share into a segment;
    // the forward differences of a path's segment by a step (its point at 0 and the first, second and third differences) and of
    // a spline's (from its two samples)
    void PathBlendPoints(const LayoutPath* path, const Vector4* points, const Vector4* weights, Vector4* out) RETAIL(FUN_00188cb0);
    void SplineBlendPoints(const CameraSpline* spline, const Vector4* points, const Vector4* weights, Vector4* out)
        RETAIL(FUN_0018a9b8);
    void PathWeightsAt(f32 share, const LayoutPath* path, Vector4* weights) RETAIL_N32(FUN_001899d0);
    void PathStartDifferences(f32 step, const LayoutPath* path, const Vector4* points, Vector4* differences)
        RETAIL_N32(FUN_00189698);
    void SplineStartDifferences(f32 step, const CameraSpline* spline, const Vector4* samples, Vector4* differences)
        RETAIL_N32(FUN_0018b110);
    // A segment of a path or a spline searched for its nearest point to the search's point (the curve keeping the segment's
    // nearest): whether it had one nearer than the search's
    u32 PathSegmentNearest(LayoutPath* path, const Vector4* point, CurveSearch* search, s32 segment) RETAIL(FUN_00188de8);
    u32 SplineSegmentNearest(CameraSpline* spline, const Vector4* point, CurveSearch* search, s32 segment) RETAIL(FUN_0018aae0);
    // The distance squared from the path's search point to its segment's point at a share, the same as the minimum search calls it
    // (its EABI entry), and that share refined by Brent's method (4 steps, to 5e-05; the distance squared at the share given, both
    // the nearest's after: whether it converged)
    f32 PathSegmentDistanceSquared(f32 into, LayoutPath* path) RETAIL_N32(FUN_001891d8);
    f32 PathDistanceSquaredIn(f32 into, LayoutPath* path) RETAIL_N32(FUN_0018e818);
    void PathDistanceSquaredEntry() RETAIL(FUN_0018e818);
    s32 RefinePathNearest(LayoutPath* path, f32* into, f32* distanceSquared) RETAIL(FUN_0018e7b8);
    // The length of a path's segment up to a share (its chords at steps of 0.01), and the share a distance into a segment reaches
    // walking its chords (at most 1) with its point (the one before when the distance ran out)
    f32 PathLengthIn(f32 share, const LayoutPath* path, s32 segment) RETAIL_N32(FUN_00189ac8);
    f32 PathShareAtDistance(f32 distance, const LayoutPath* path, Vector4* out, s32 segment) RETAIL_N32(FUN_00189dd0);
    // A spline read (its vtable's slot 2): its count and step, its samples and its parameters
    void ReadSpline(CameraSpline* spline, Stream* stream) RETAIL(FUN_0018b390);
}

EABI_EXPORT(FUN_001899d0, PathWeightsAt);
EABI_EXPORT(FUN_00189698, PathStartDifferences);
EABI_EXPORT(FUN_0018b110, SplineStartDifferences);
EABI_EXPORT(FUN_001891d8, PathSegmentDistanceSquared);
EABI_EXPORT(FUN_0018e818, PathDistanceSquaredIn);
EABI_EXPORT(FUN_00189ac8, PathLengthIn);
EABI_EXPORT(FUN_00189dd0, PathShareAtDistance);
EABI_EXPORT(FUN_00189440, PathPointAt);
EABI_EXPORT(FUN_00189550, PathDirectionAt);
EABI_EXPORT(FUN_00189c18, PathTangentIn);
EABI_EXPORT(FUN_0018a7a8, SplinePointIn);
EABI_EXPORT(FUN_0018ae68, SplineSegmentDistanceSquared);
EABI_EXPORT(BoxPointAtFractions, BoxPointAtFractions);

namespace
{
constexpr f32 Sixth = 0x1.555556p-3f;
// The path's basis as retail has it (a sixth, a half and two thirds a few bits short)
constexpr f32 BasisSixth = 0x1.55554cp-3f;
constexpr f32 BasisHalf = 0x1.fffff2p-2f;
constexpr f32 BasisOne = 0x1.fffff2p-1f;
constexpr f32 BasisTwoThirds = 0x1.55554cp-1f;
// The share the chords of a path's segment are walked by
constexpr f32 ChordStep = 0x1.47ae14p-7f;
// A walk's point this much nearer than the segment's ends is refined
constexpr f32 RefineBelow = 0x1.fef9dcp-1f;

struct PathSegment
{
    s32 segment;
    f32 into;
};

f32 DistanceSquared(const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return x * x + y * y + z * z;
}

// Four points weighted (the w 1)
void WeightedSum(const Vector4* points, const f32* weights, Vector4* out)
{
    out->x = points[0].x * weights[0];
    out->y = points[0].y * weights[0];
    out->z = points[0].z * weights[0];
    out->w = 1.0f;
    for (u32 point = 1; point < 4; point++)
    {
        out->x = out->x + points[point].x * weights[point];
        out->y = out->y + points[point].y * weights[point];
        out->z = out->z + points[point].z * weights[point];
    }
}

// A cubic Hermite segment's weights a share into it: the first sample's point's and tangent's, the second's
void HermiteWeights(f32 share, f32* weights)
{
    f32 squared = share * share;
    f32 cubed = squared * share;
    f32 lastTangent = cubed - squared;
    f32 lastPoint = (cubed + cubed) - squared * 3.0f;
    weights[0] = lastPoint + 1.0f;
    weights[1] = (lastTangent - squared) + share;
    weights[2] = -lastPoint;
    weights[3] = lastTangent;
}

// The segment of a path a distance along it is in (searched by halving), and how far into the segment
PathSegment PathSegmentAt(const LayoutPath* path, f32 distance)
{
    const f32* lengths = path->lengths;
    s32 segments = path->count - 3;
    s32 segment;
    if (distance <= lengths[0])
    {
        segment = 0;
    }
    else if (lengths[segments - 2] <= distance)
    {
        segment = segments - 1;
    }
    else
    {
        segment = segments >> 1;
        s32 step = segment;
        while (true)
        {
            step >>= 1;
            if (step == 0)
            {
                step = 1;
            }

            if (distance <= lengths[segment] && lengths[segment - 1] < distance)
            {
                break;
            }

            if (distance < lengths[segment])
            {
                segment -= step;
            }
            else
            {
                segment += step;
            }
        }
    }

    f32 length = segment != 0 ? lengths[segment] - lengths[segment - 1] : lengths[0];
    return {segment, length - (lengths[segment] - distance)};
}

// One axis of a uniform cubic B-spline segment's forward differences by a step from its four points: its value at 0, then the
// first, second and third differences
void BSplineDifferences(f32 p0, f32 p1, f32 p2, f32 p3, f32 step, f32 squared, f32 cubed, f32* out)
{
    f32 three0 = p0 * 3.0f;
    f32 three1 = p1 * 3.0f;
    f32 three2 = p2 * 3.0f;
    f32 cubic = (((three1 - three2) - p0) + p3) * Sixth * cubed;
    f32 quadratic = (((three0 - three1) - three1) + three2) * Sixth * squared;
    f32 linear = (three2 - three0) * Sixth * step;
    out[0] = (((three1 + p1) + p0) + p2) * Sixth;
    out[1] = (cubic + quadratic) + linear;
    out[2] = cubic * 6.0f + (quadratic + quadratic);
    out[3] = cubic * 6.0f;
}

// The same of a cubic Hermite segment from its samples' points and tangents (the value at 0 left out: the first point's)
void HermiteDifferences(f32 p0, f32 m0, f32 p1, f32 m1, f32 step, f32 squared, f32 cubed, f32* out)
{
    f32 difference = p0 - p1;
    f32 tangents = m0 + m1;
    f32 cubic = ((difference + difference) + tangents) * cubed;
    f32 quadratic = (difference * -3.0f - (m0 + tangents)) * squared;
    f32 linear = m0 * step;
    out[1] = (cubic + quadratic) + linear;
    out[2] = cubic * 6.0f + (quadratic + quadratic);
    out[3] = cubic * 6.0f;
}

f32 ClampUnit(f32 value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (1.0f < value)
    {
        return 1.0f;
    }

    return value;
}

// How far a point is from a box's corner along one of its axes
f32 DistanceAlong(const Vector4* axis, const Vector4* point, const Vector4* corner)
{
    f32 x = point->x - corner->x;
    f32 y = point->y - corner->y;
    f32 z = point->z - corner->z;
    return axis->x * x + axis->y * y + axis->z * z;
}

// A corner moved along an axis by a length (the w 1)
Vector4 Along(const Vector4* corner, const Vector4* axis, f32 length)
{
    return {axis->x * length + corner->x, axis->y * length + corner->y, axis->z * length + corner->z, 1.0f};
}
}

void PathBlendPoints(const LayoutPath*, const Vector4* points, const Vector4* weights, Vector4* out)
{
    WeightedSum(points, &weights->x, out);
}

void SplineBlendPoints(const CameraSpline*, const Vector4* points, const Vector4* weights, Vector4* out)
{
    WeightedSum(points, &weights->x, out);
}

void PathWeightsAt(f32 share, const LayoutPath*, Vector4* weights)
{
    if (g_PathBasisUsed == 0)
    {
        g_PathBasisUsed = 1;
    }

    if (g_PathBasisMade == 0)
    {
        g_PathBasis = {{
            {-BasisSixth, BasisHalf, -BasisHalf, BasisSixth},
            {BasisHalf, -BasisOne, BasisHalf, 0.0f},
            {-BasisHalf, 0.0f, BasisHalf, 0.0f},
            {BasisSixth, BasisTwoThirds, BasisSixth, 0.0f},
        }};
        g_PathBasisMade = 1;
    }

    f32 squared = share * share;
    weights->x = squared * share;
    weights->y = squared;
    weights->z = share;
    weights->w = 1.0f;
    VuTransformPoint(&g_PathBasis, weights, weights);
}

void PathStartDifferences(f32 step, const LayoutPath*, const Vector4* points, Vector4* differences)
{
    f32 squared = step * step;
    f32 cubed = squared * step;
    f32 x[4];
    f32 y[4];
    f32 z[4];
    BSplineDifferences(points[0].x, points[1].x, points[2].x, points[3].x, step, squared, cubed, x);
    BSplineDifferences(points[0].y, points[1].y, points[2].y, points[3].y, step, squared, cubed, y);
    BSplineDifferences(points[0].z, points[1].z, points[2].z, points[3].z, step, squared, cubed, z);
    // The Ws are what retail's copies of the points left there
    differences[0] = {x[0], y[0], z[0], points[1].w};
    differences[1] = {x[1], y[1], z[1], 1.0f};
    differences[2] = {x[2], y[2], z[2], 1.0f};
    differences[3] = {x[3], y[3], z[3], points[2].w};
}

void SplineStartDifferences(f32 step, const CameraSpline*, const Vector4* samples, Vector4* differences)
{
    f32 squared = step * step;
    f32 cubed = squared * step;
    f32 x[4];
    f32 y[4];
    f32 z[4];
    HermiteDifferences(samples[0].x, samples[1].x, samples[2].x, samples[3].x, step, squared, cubed, x);
    HermiteDifferences(samples[0].y, samples[1].y, samples[2].y, samples[3].y, step, squared, cubed, y);
    HermiteDifferences(samples[0].z, samples[1].z, samples[2].z, samples[3].z, step, squared, cubed, z);
    differences[0] = samples[0];
    differences[1] = {x[1], y[1], z[1], 1.0f};
    differences[2] = {x[2], y[2], z[2], 1.0f};
    differences[3] = {x[3], y[3], z[3], samples[0].w};
}

u32 PathSegmentNearest(LayoutPath* path, const Vector4* point, CurveSearch* search, s32 segment)
{
    f32 step = path->steps[segment];
    Vector4 differences[4];
    PathStartDifferences(step, path, &path->points[segment], differences);
    // The segment's ends: the B-spline's point at 0 and at 1
    Vector4 at = differences[0];
    path->searchPoint = *point;
    path->nearest = at;
    path->searchSegment = segment;
    path->nearestShare = 0.0f;
    path->nearestDistance = DistanceSquared(point, &at);
    const Vector4* points = &path->points[segment];
    at = points[1];
    at.x = ((at.x + points[2].x * 4.0f) + points[3].x) * Sixth;
    at.y = ((at.y + points[2].y * 4.0f) + points[3].y) * Sixth;
    at.z = ((at.z + points[2].z * 4.0f) + points[3].z) * Sixth;
    f32 endDistance = DistanceSquared(point, &at);
    if (endDistance < path->nearestDistance)
    {
        path->nearestDistance = endDistance;
        path->nearest = at;
        path->nearestShare = 1.0f;
    }

    u32 found = 0;
    f32 share = 0.0f;
    do
    {
        at = differences[0];
        f32 distance = DistanceSquared(point, &at);
        f32 into = share;
        f32 distanceSquared = distance;
        if (0.0f < share && distance < path->nearestDistance * RefineBelow)
        {
            RefinePathNearest(path, &into, &distanceSquared);
            Vector4 segmentPoints[4];
            for (u32 index = 0; index < 4; index++)
            {
                segmentPoints[index] = path->points[segment + index];
            }

            Vector4 weights;
            PathWeightsAt(into, path, &weights);
            PathBlendPoints(path, segmentPoints, &weights, &at);
            path->nearestShare = into;
            path->nearest = at;
            path->nearestDistance = distanceSquared;
            if (!(distanceSquared < search->distance))
            {
                return found;
            }

            search->nearest = at;
            search->distance = path->nearestDistance;
            search->share = path->nearestShare;
            return 1;
        }

        if (distance < search->distance)
        {
            found = 1;
            search->distance = distance;
            search->nearest = at;
            search->share = share;
        }

        PathDifferencesStep(path, differences);
        share = share + step;
    } while (share < 1.0f);

    return found;
}

f32 PathSegmentDistanceSquared(f32 into, LayoutPath* path)
{
    Vector4 points[4];
    for (u32 index = 0; index < 4; index++)
    {
        points[index] = path->points[path->searchSegment + index];
    }

    Vector4 weights;
    PathWeightsAt(into, path, &weights);
    Vector4 point;
    PathBlendPoints(path, points, &weights, &point);
    return DistanceSquared(&path->searchPoint, &point);
}

void PathNearestSearch(LayoutPath* path, const Vector4* point, CurveSearch* search)
{
    search->distance = Infinite;
    const Vector4* points = path->points;
    s32 nearest = 0;
    f32 nearestDistance = DistanceSquared(&points[0], point);
    for (s32 index = 1; index < path->count; index++)
    {
        f32 distance = DistanceSquared(&points[index], point);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = index;
        }
    }

    // The segments the nearest point shapes: the four up to the one it starts (as many as the path has)
    s32 first = nearest - 3;
    s32 count = 4;
    if (first < 0)
    {
        count = nearest + 1;
        first = 0;
    }

    s32 segments = path->count - 3;
    if (segments < first + count)
    {
        count -= first + count - segments;
    }

    for (s32 searched = 0; searched < count; searched++)
    {
        if (PathSegmentNearest(path, point, search, first) != 0)
        {
            search->segment = first;
        }

        first++;
    }
}

void PathPointAt(f32 along, const LayoutPath* path, Vector4* out)
{
    PathSegment at = PathSegmentAt(path, path->lengths[path->count - 4] * along);
    PathShareAtDistance(at.into, path, out, at.segment);
}

void PathDirectionAt(f32 along, LayoutPath* path, Vector4* direction)
{
    PathSegment at = PathSegmentAt(path, path->lengths[path->count - 4] * along);
    // The point found is overwritten by the direction
    f32 share = PathShareAtDistance(at.into, path, direction, at.segment);
    PathDirectionIn(share, path, direction, at.segment);
}

f32 PathLengthIn(f32 share, const LayoutPath* path, s32 segment)
{
    Vector4 points[4];
    for (u32 index = 0; index < 4; index++)
    {
        points[index] = path->points[segment + index];
    }

    f32 length = 0.0f;
    if (!(0.0f <= share))
    {
        return length;
    }

    Vector4 last;
    bool started = false;
    f32 at = 0.0f;
    do
    {
        Vector4 weights;
        PathWeightsAt(at, path, &weights);
        Vector4 point;
        VuTransformByRows(reinterpret_cast<const Matrix4x4*>(points), &weights, &point);
        if (started)
        {
            length = length + __builtin_sqrtf(DistanceSquared(&point, &last));
        }
        else
        {
            started = true;
        }

        at = at + ChordStep;
        last = point;
    } while (at <= share);

    return length;
}

void PathTangentIn(f32 into, const LayoutPath* path, Vector4* tangent, s32 segment)
{
    // The B-spline's weights' derivatives
    f32 squared = into * into;
    f32 half = squared * 0.5f;
    f32 threeHalves = squared + half;
    f32 weights[4];
    weights[0] = (into - half) - 0.5f;
    weights[1] = threeHalves - (into + into);
    weights[2] = (into - threeHalves) + 0.5f;
    weights[3] = half;
    WeightedSum(&path->points[segment], weights, tangent);
}

f32 PathShareAtDistance(f32 distance, const LayoutPath* path, Vector4* out, s32 segment)
{
    Vector4 points[4];
    for (u32 index = 0; index < 4; index++)
    {
        points[index] = path->points[segment + index];
    }

    f32 walked = 0.0f;
    f32 share = 0.0f;
    bool started = false;
    Vector4 point;
    while (true)
    {
        Vector4 weights;
        PathWeightsAt(share, path, &weights);
        VuTransformByRows(reinterpret_cast<const Matrix4x4*>(points), &weights, &point);
        if (started)
        {
            walked = walked + __builtin_sqrtf(DistanceSquared(&point, out));
            if (distance <= walked)
            {
                break;
            }
        }

        started = true;
        share = share + ChordStep;
        *out = point;
        if (!(share <= 1.0f))
        {
            break;
        }
    }

    if (1.0f <= share)
    {
        share = 1.0f;
        *out = point;
    }

    return share;
}

u32 BoxFractionsOf(const Vector4* box, const Vector4* point, Vector4* projected, f32* across, f32* along)
{
    const Vector4* acrossAxis = &box[1];
    const Vector4* alongAxis = &box[2];
    const Vector4* corner = &box[3];
    const Vector4* size = &box[4];
    *across = DistanceAlong(acrossAxis, point, corner);
    *along = DistanceAlong(alongAxis, point, corner);
    if (0.0f <= *across && *across <= size->x && 0.0f <= *along && *along <= size->y)
    {
        // Inside: the point taken into the box's plane
        f32 acrossDistance = *across;
        f32 alongDistance = *along;
        projected->x = (acrossAxis->x * acrossDistance + alongAxis->x * alongDistance) + corner->x;
        projected->y = (acrossAxis->y * acrossDistance + alongAxis->y * alongDistance) + corner->y;
        projected->z = (acrossAxis->z * acrossDistance + alongAxis->z * alongDistance) + corner->z;
        projected->w = 1.0f;
        *across = *across / size->x;
        *along = *along / size->y;
        return 1;
    }

    // Outside: the nearest point of its four edges (from the corner across, along from that end, across from the corner's along
    // end and along from the corner) and its fractions
    f32 nearestDistance = Infinite;
    Vector4 edge[2];
    Vector4 nearest;
    Vector4 acrossEnd = Along(corner, acrossAxis, size->x);
    edge[0] = *corner;
    edge[1] = acrossEnd;
    f32 distance = NearestSegmentPoint(edge, point, &nearest);
    if (distance < nearestDistance)
    {
        nearestDistance = distance;
        *projected = nearest;
        *across = ClampUnit(NearestLineParameter(edge, &nearest));
        *along = 0.0f;
    }

    Vector4 farCorner = Along(&acrossEnd, alongAxis, size->y);
    edge[0] = acrossEnd;
    edge[1] = farCorner;
    distance = NearestSegmentPoint(edge, point, &nearest);
    if (distance < nearestDistance)
    {
        nearestDistance = distance;
        *projected = nearest;
        *along = ClampUnit(NearestLineParameter(edge, &nearest));
        *across = 1.0f;
    }

    edge[0] = Along(corner, alongAxis, size->y);
    edge[1] = farCorner;
    distance = NearestSegmentPoint(edge, point, &nearest);
    if (distance < nearestDistance)
    {
        nearestDistance = distance;
        *projected = nearest;
        *across = ClampUnit(NearestLineParameter(edge, &nearest));
        *along = 1.0f;
    }

    edge[0] = *corner;
    edge[1] = Along(corner, alongAxis, size->y);
    distance = NearestSegmentPoint(edge, point, &nearest);
    if (distance < nearestDistance)
    {
        *projected = nearest;
        *along = ClampUnit(NearestLineParameter(edge, &nearest));
        *across = 0.0f;
    }

    return 0;
}

void BoxPointAtFractions(f32 across, f32 along, const Vector4* box, Vector4* out)
{
    const Vector4* acrossAxis = &box[1];
    const Vector4* alongAxis = &box[2];
    const Vector4* corner = &box[3];
    const Vector4* size = &box[4];
    out->x = (acrossAxis->x * across * size->x + alongAxis->x * along * size->y) + corner->x;
    out->y = (acrossAxis->y * across * size->x + alongAxis->y * along * size->y) + corner->y;
    out->z = (acrossAxis->z * across * size->x + alongAxis->z * along * size->y) + corner->z;
    out->w = 1.0f;
}

void SplinePointIn(f32 into, const CameraSpline* spline, Vector4* out, s32 segment)
{
    if (spline->step < into)
    {
        into = spline->step;
    }

    f32 weights[4];
    HermiteWeights(into * spline->inverseStep, weights);
    WeightedSum(&spline->samples[segment * 2], weights, out);
}

u32 SplineSegmentNearest(CameraSpline* spline, const Vector4* point, CurveSearch* search, s32 segment)
{
    f32 step = spline->steps[segment];
    Vector4 differences[4];
    SplineStartDifferences(step, spline, &spline->samples[segment * 2], differences);
    // The segment's ends: its samples' points
    Vector4 at = differences[0];
    spline->searchPoint = *point;
    spline->nearest = at;
    f32 startDistance = DistanceSquared(point, &at);
    spline->searchSegment = segment;
    spline->nearestShare = 0.0f;
    spline->nearestDistance = startDistance;
    at = spline->samples[segment * 2 + 2];
    f32 endDistance = DistanceSquared(point, &at);
    if (endDistance < startDistance)
    {
        spline->nearest = at;
        spline->nearestDistance = endDistance;
        spline->nearestShare = 1.0f;
    }

    u32 found = 0;
    f32 share = 0.0f;
    do
    {
        at = differences[0];
        f32 distance = DistanceSquared(point, &at);
        f32 into = share;
        f32 distanceSquared = distance;
        f32 next = share + step;
        if (0.0f < share && distance < spline->nearestDistance * RefineBelow)
        {
            // Unlike a path's, the walk goes on after a refined point
            RefineSplineNearest(spline, &into, &distanceSquared);
            Vector4 samples[4];
            for (u32 index = 0; index < 4; index++)
            {
                samples[index] = spline->samples[segment * 2 + index];
            }

            Vector4 weights;
            HermiteWeights(into, &weights.x);
            SplineBlendPoints(spline, samples, &weights, &at);
            spline->nearestShare = into;
            spline->nearest = at;
            spline->nearestDistance = distanceSquared;
            if (distanceSquared < search->distance)
            {
                search->nearest = at;
                found = 1;
                search->distance = spline->nearestDistance;
                search->share = spline->nearestShare;
            }
        }
        else if (distance < search->distance)
        {
            found = 1;
            search->distance = distance;
            search->nearest = at;
            search->share = share;
        }

        SplineDifferencesStep(spline, differences);
        share = next;
    } while (share < 1.0f);

    return found;
}

f32 SplineSegmentDistanceSquared(f32 into, CameraSpline* spline)
{
    const Vector4* samples = &spline->samples[spline->searchSegment * 2];
    Vector4 points[4];
    for (u32 index = 0; index < 4; index++)
    {
        points[index] = samples[index];
    }

    Vector4 weights;
    HermiteWeights(into, &weights.x);
    Vector4 point;
    SplineBlendPoints(spline, points, &weights, &point);
    return DistanceSquared(&spline->searchPoint, &point);
}

void SplineNearestSearch(CameraSpline* spline, const Vector4* point, CurveSearch* search)
{
    search->distance = Infinite;
    const Vector4* samples = spline->samples;
    s32 nearest = 0;
    f32 nearestDistance = DistanceSquared(&samples[0], point);
    // Every sample's point (the samples counted in 32 bits)
    s32 end = static_cast<s32>(static_cast<u32>(spline->count) * 2 + 2);
    for (s32 index = 2; index < end; index += 2)
    {
        f32 distance = DistanceSquared(&samples[index], point);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = index;
        }
    }

    // The segments the nearest sample ends and starts (as many as the spline has)
    s32 sample = nearest / 2;
    s32 first = sample - 1;
    s32 count = 2;
    if (first < 0)
    {
        count = sample + 1;
        first = 0;
    }

    if (spline->count < first + count)
    {
        count -= first + count - spline->count;
    }

    for (s32 searched = 0; searched < count; searched++)
    {
        if (SplineSegmentNearest(spline, point, search, first) != 0)
        {
            search->segment = first;
        }

        first++;
    }
}

void ReadSpline(CameraSpline* spline, Stream* stream)
{
    stream->ReadU32(reinterpret_cast<u32*>(&spline->count));
    stream->ReadF32(&spline->step);
    spline->inverseStep = 1.0f / spline->step;
    // A sample at each end of every segment, a point and a tangent each
    u32 samples = static_cast<u32>(spline->count) * 2 + 2;
    spline->samples = static_cast<Vector4*>(MemoryAllocate2(samples * sizeof(Vector4)));
    stream->Read(spline->samples, samples * sizeof(Vector4), 1);
    u32 parameters = spline->count;
    spline->lengths = static_cast<f32*>(MemoryAllocate2(parameters * 2 * sizeof(f32)));
    spline->steps = spline->lengths + parameters;
    stream->Read(spline->lengths, parameters * 2 * sizeof(f32), 1);
}

s32 RefinePathNearest(LayoutPath* path, f32* into, f32* distanceSquared)
{
    MinimumSearch search;
    search.steps = CurveRefineSteps;
    search.tolerance = CurveRefineTolerance;
    search.closeness = CurveRefineTolerance;
    search.low = 0.0f;
    search.high = 1.0f;
    return FindMinimum(&search, path, reinterpret_cast<const void*>(&PathDistanceSquaredEntry), into, distanceSquared, 1);
}

f32 PathDistanceSquaredIn(f32 into, LayoutPath* path)
{
    return PathSegmentDistanceSquared(into, path);
}

f32 NearestPointOnPath(LayoutPath* path, const Vector4* position, Vector4* nearest)
{
    CurveSearch search;
    search.segment = -1;
    PathNearestSearch(path, position, &search);
    *nearest = search.nearest;
    return PathNearestParameter(path, &search);
}

f32 PathNearestParameter(const LayoutPath* path, const CurveSearch* search)
{
    f32 before = search->segment != 0 ? path->lengths[search->segment - 1] : 0.0f;
    f32 along = before + PathLengthIn(search->share, path, search->segment);
    return along / path->lengths[path->count - 4];
}
