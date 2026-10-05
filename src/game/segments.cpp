#include "game/cameras.h"
#include "game/math.h"

// Lines and segments (two points each): the parameter of a line nearest a point, the squared distances from a point to a line
// and to a segment (and the segment's nearest point), and between two segments

extern "C"
{
    // A quadword copied
    Vector4* CopyQuadword(Vector4* to, const Vector4* from) RETAIL(MovePositionFromPos2ToPos1);
}

namespace
{
f32 InverseUnlessShort(f32 value)
{
    if (InverseEpsilon < value || value < -InverseEpsilon)
    {
        return 1.0f / value;
    }

    return 0.0f;
}

f32 Dot(const Vector4& a, const Vector4& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

f32 DistanceSquared(const Vector4& a, const Vector4& b)
{
    f32 x = a.x - b.x;
    f32 y = a.y - b.y;
    f32 z = a.z - b.z;
    return x * x + y * y + z * z;
}

// How far along a line (two points) its nearest point to a point is, times the line's length squared: (end - start) · (point -
// start) as retail works it out
f32 AlongTimesLengthSquared(const Vector4& start, const Vector4& end, const Vector4& point)
{
    return (Dot(end, point) - Dot(start, point)) - (Dot(end, start) - Dot(start, start));
}
}

f32 NearestLineParameter(const Vector4* line, const Vector4* point)
{
    f32 along = AlongTimesLengthSquared(line[0], line[1], *point);
    if (along * along == 0.0f)
    {
        return 0.0f;
    }

    return along / DistanceSquared(line[0], line[1]);
}

f32 SegmentPointDistanceSquared(const Vector4* segment, const Vector4* point)
{
    const Vector4& start = segment[0];
    f32 along = AlongTimesLengthSquared(start, segment[1], *point);
    f32 negativeSquared = along * -along;
    if (!(negativeSquared < 0.0f))
    {
        return DistanceSquared(*point, start);
    }

    // The distance to the start less the part along the line (the line through the points, not the segment)
    negativeSquared = negativeSquared * InverseUnlessShort(DistanceSquared(start, segment[1]));
    return __builtin_fabsf(negativeSquared + DistanceSquared(*point, start));
}

f32 PointSegmentDistanceSquared(const Vector4* segment, const Vector4* point)
{
    const Vector4& start = segment[0];
    const Vector4& end = segment[1];
    f32 along = AlongTimesLengthSquared(start, end, *point);
    if (!(0.0f < along))
    {
        return DistanceSquared(*point, start);
    }

    f32 lengthSquared = DistanceSquared(start, end);
    if (!(along < lengthSquared))
    {
        return DistanceSquared(*point, end);
    }

    f32 alongSquared = along * along * InverseUnlessShort(lengthSquared);
    return __builtin_fabsf(DistanceSquared(*point, start) - alongSquared);
}

f32 NearestSegmentPoint(const Vector4* segment, const Vector4* point, Vector4* nearest)
{
    CopyQuadword(nearest, &segment[0]);
    const Vector4& start = segment[0];
    const Vector4& end = segment[1];
    f32 along = AlongTimesLengthSquared(start, end, *point);
    if (!(0.0f < along))
    {
        return DistanceSquared(*point, start);
    }

    f32 lengthSquared = DistanceSquared(start, end);
    if (!(along < lengthSquared))
    {
        CopyQuadword(nearest, &end);
        return DistanceSquared(*point, end);
    }

    // The start (the copy in nearest) moved the share of the way toward the end
    f32 away = -InverseUnlessShort(lengthSquared) * along;
    nearest->x = (nearest->x - end.x) * away + start.x;
    nearest->y = (nearest->y - end.y) * away + start.y;
    nearest->z = (nearest->z - end.z) * away + start.z;
    return __builtin_fabsf(away * along + DistanceSquared(*point, start));
}

f32 SegmentsDistanceSquared(const Vector4* first, const Vector4* second, f32* firstShare, f32* secondShare)
{
    // Eberly's distance between two segments (Magic Software's, the second segment's share decided first): the shares s and t
    // along them minimise |(first + s d0) - (second + t d1)|², a quadratic of a00 s² + 2 a01 s t + a11 t² + 2 b0 s + 2 b1 t + c.
    // The slopes are halves of its derivatives along a share at a corner of the square: tSlopeAtS1 along t at s = 1, t = 0,
    // sSlopeAtT1 along s at s = 0, t = 1 and tSlopeAtT1 along t at s = 0, t = 1.
    // Retail bug: in four of its regions the distance is worked out wrong (the shares are right): noted where
    Vector4 firstDirection;
    firstDirection.x = first[1].x - first[0].x;
    firstDirection.y = first[1].y - first[0].y;
    firstDirection.z = first[1].z - first[0].z;
    Vector4 secondDirection;
    secondDirection.x = second[1].x - second[0].x;
    secondDirection.y = second[1].y - second[0].y;
    secondDirection.z = second[1].z - second[0].z;
    Vector4 between;
    between.x = second[0].x - first[0].x;
    between.y = second[0].y - first[0].y;
    between.z = second[0].z - first[0].z;
    f32 a00 = Dot(firstDirection, firstDirection);
    f32 directionsDot = Dot(secondDirection, firstDirection);
    f32 a11 = Dot(secondDirection, secondDirection);
    f32 a01 = -directionsDot;
    f32 determinant = __builtin_fabsf(a11 * a00 - a01 * a01);
    f32 b1 = Dot(between, secondDirection);
    f32 c = Dot(between, between);
    f32 s;
    f32 t;
    f32 distance;
    // Segments this near parallel are taken for parallel
    if (InverseEpsilon <= determinant)
    {
        f32 betweenDot0 = Dot(between, firstDirection);
        f32 b0 = -betweenDot0;
        t = a01 * b0 - a00 * b1;
        s = a01 * b1 - a11 * b0;
        if (0.0f <= t)
        {
            if (t <= determinant)
            {
                if (0.0f <= s)
                {
                    if (s <= determinant)
                    {
                        f32 inverse = 1.0f / determinant;
                        t = t * inverse;
                        s = s * inverse;
                        f32 firstTerm = (a01 * t + a00 * s + (b0 + b0)) * s;
                        f32 secondTerm = (a11 * t + a01 * s + (b1 + b1)) * t;
                        distance = secondTerm + firstTerm + c;
                    }
                    else
                    {
                        s = 1.0f;
                        f32 tSlopeAtS1 = a01 + b1;
                        t = 0.0f;
                        if (0.0f <= tSlopeAtS1)
                        {
                            distance = b0 + b0 + a00 + c;
                        }
                        else if (a11 <= -tSlopeAtS1)
                        {
                            t = 1.0f;
                            f32 sum = tSlopeAtS1 + b0;
                            distance = sum + sum + a11 + a00 + c;
                        }
                        else
                        {
                            t = -tSlopeAtS1 / a11;
                            distance = b0 + b0 + c + a00 + tSlopeAtS1 * t;
                        }
                    }
                }
                else
                {
                    s = 0.0f;
                    if (0.0f <= b1)
                    {
                        t = 0.0f;
                        distance = c;
                    }
                    else if (a11 <= -b1)
                    {
                        t = 1.0f;
                        distance = b1 + b1 + c + a11;
                    }
                    else
                    {
                        t = -b1 / a11;
                        distance = b1 * t + c;
                    }
                }
            }
            else if (0.0f <= s)
            {
                if (s <= determinant)
                {
                    f32 sSlopeAtT1 = a01 + b0;
                    t = 1.0f;
                    s = 0.0f;
                    if (0.0f <= sSlopeAtT1)
                    {
                        distance = b1 + b1 + c + a11;
                    }
                    else if (a00 <= -sSlopeAtT1)
                    {
                        s = 1.0f;
                        f32 sum = b1 + sSlopeAtT1;
                        distance = sum + sum + c + a00 + a11;
                    }
                    else
                    {
                        s = -sSlopeAtT1 / a00;
                        distance = b1 + b1 + c + a11 + sSlopeAtT1 * s;
                    }
                }
                else
                {
                    f32 tSlopeAtS1 = a01 + b1;
                    if (-tSlopeAtS1 <= a11)
                    {
                        s = 1.0f;
                        t = 0.0f;
                        if (0.0f <= tSlopeAtS1)
                        {
                            distance = b0 + b0 + c + a00;
                        }
                        else
                        {
                            t = -tSlopeAtS1 / a11;
                            // Retail bug: three times b0 where a00 + 2 b0 + c belongs
                            distance = b0 + b0 + b0 + tSlopeAtS1 * t;
                        }
                    }
                    else
                    {
                        f32 sSlopeAtT1 = a01 + b0;
                        t = 1.0f;
                        s = 0.0f;
                        if (0.0f <= sSlopeAtT1)
                        {
                            distance = b1 + b1 + c + a11;
                        }
                        else if (a00 <= -sSlopeAtT1)
                        {
                            s = 1.0f;
                            f32 sum = b1 + sSlopeAtT1;
                            distance = sum + sum + c + a00 + a11;
                        }
                        else
                        {
                            s = -sSlopeAtT1 / a00;
                            distance = b1 + b1 + c + a11 + sSlopeAtT1 * s;
                        }
                    }
                }
            }
            else
            {
                s = 0.0f;
                if (-b1 < a11)
                {
                    if (0.0f <= b1)
                    {
                        t = 0.0f;
                        distance = c;
                    }
                    else
                    {
                        t = -b1 / a11;
                        distance = b1 * t + c;
                    }
                }
                else
                {
                    f32 sSlopeAtT1 = a01 + b0;
                    t = 1.0f;
                    if (0.0f <= sSlopeAtT1)
                    {
                        distance = b1 + b1 + c + a11;
                    }
                    else if (a00 <= -sSlopeAtT1)
                    {
                        s = 1.0f;
                        // Retail bug: sSlopeAtT1 + b1 not doubled
                        distance = sSlopeAtT1 + b1 + c + a00 + a11;
                    }
                    else
                    {
                        s = -sSlopeAtT1 / a00;
                        // Retail bug: a11 + 2 b1 + c left out
                        distance = sSlopeAtT1 * s;
                    }
                }
            }
        }
        else if (0.0f <= s)
        {
            t = 0.0f;
            if (s <= determinant)
            {
                if (t <= b0)
                {
                    s = 0.0f;
                    distance = c;
                }
                else if (a00 <= betweenDot0)
                {
                    s = 1.0f;
                    distance = b0 + b0 + c + a00;
                }
                else
                {
                    s = betweenDot0 / a00;
                    distance = s * b0 + c;
                }
            }
            else
            {
                f32 tSlopeAtS1 = a01 + b1;
                if (tSlopeAtS1 < 0.0f)
                {
                    s = 1.0f;
                    t = 1.0f;
                    if (a11 <= -tSlopeAtS1)
                    {
                        f32 sum = tSlopeAtS1 + b0;
                        distance = sum + sum + c + a00 + a11;
                    }
                    else
                    {
                        t = -tSlopeAtS1 / a11;
                        distance = b0 + b0 + c + a00 + tSlopeAtS1 * t;
                    }
                }
                else if (t <= b0)
                {
                    s = 0.0f;
                    distance = c;
                }
                else if (a00 <= betweenDot0)
                {
                    s = 1.0f;
                    distance = b0 + b0 + c + a00;
                }
                else
                {
                    s = betweenDot0 / a00;
                    distance = s * b0 + c;
                }
            }
        }
        else if (b1 < 0.0f)
        {
            t = 0.0f;
            s = 0.0f;
            if (a11 <= -b1)
            {
                t = 1.0f;
                distance = b1 + b1 + c + a11;
            }
            else
            {
                t = -b1 / a11;
                distance = b1 * t + c;
            }
        }
        else
        {
            t = 0.0f;
            if (t <= b0)
            {
                s = 0.0f;
                distance = c;
            }
            else if (a00 <= betweenDot0)
            {
                s = 1.0f;
                distance = b0 + b0 + a00 + c;
            }
            else
            {
                s = betweenDot0 / a00;
                distance = b0 * s + c;
            }
        }
    }
    else if (0.0f < a01)
    {
        // Parallel, the directions opposite
        s = 0.0f;
        t = 0.0f;
        if (0.0f <= b1)
        {
            distance = c;
        }
        else if (-b1 <= a11)
        {
            t = -b1 / a11;
            distance = b1 * t + c;
        }
        else
        {
            t = 1.0f;
            f32 b0 = -Dot(between, firstDirection);
            f32 tSlopeAtT1 = a11 + b1;
            if (a01 <= -tSlopeAtT1)
            {
                s = 1.0f;
                f32 sum = a01 + b1 + b0;
                distance = sum + sum + c + a00 + a11;
            }
            else
            {
                s = -tSlopeAtT1 / a01;
                // Retail bug: b1 where c belongs
                distance = (a01 + b0 + (a01 + b0) + a00 * s) * s + b1 + (b1 + b1) + a11;
            }
        }
    }
    else
    {
        // Parallel, the directions alike
        s = 0.0f;
        if (a11 <= -b1)
        {
            t = 1.0f;
            distance = b1 + b1 + c + a11;
        }
        else if (b1 <= 0.0f)
        {
            t = -b1 / a11;
            distance = b1 * t + c;
        }
        else
        {
            t = 0.0f;
            f32 b0 = -Dot(between, firstDirection);
            if (directionsDot <= b1)
            {
                s = 1.0f;
                distance = b0 + b0 + c + a00;
            }
            else
            {
                s = -b1 / a01;
                distance = (s * a00 + (b0 + b0)) * s + c;
            }
        }
    }

    if (firstShare != nullptr)
    {
        *firstShare = s;
    }

    if (secondShare != nullptr)
    {
        *secondShare = t;
    }

    return __builtin_fabsf(distance);
}
