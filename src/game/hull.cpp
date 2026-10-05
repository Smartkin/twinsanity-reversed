#include "game/hull.h"

#include "game/collision.h"
#include "game/memory.h"
#include "game/stream.h"
#include "retail/libc.h"

EABI_EXPORT(FUN_00201020, SphereInHull);
EABI_EXPORT(FUN_001fcfa8, SphereInPlanes);

namespace
{
// Points nearer than this are one to the builder, directions one direction (either way), and a plane has points in front or
// behind beyond this
constexpr f32 SamePoint = Rounded(0.0001);
constexpr f32 SameDirection = Rounded(0.001);
constexpr f32 OffPlane = Rounded(0.001);
// A point this near a hull's plane is on it
constexpr f32 OnHullPlane = Epsilon;
// The counts and offsets that start a hull (and its file's part)
constexpr u32 HullHeaderSize = offsetof(CollisionHull, surface);
// An edge's two vertex bytes, and a face's byte of its corner count before its corners
constexpr s32 EdgeSize = sizeof(g_HullBuilderEdges[0]);
constexpr s32 FaceCountSize = 1;

f32 Distance(const Vector4* a, const Vector4* b)
{
    f32 x = a->x - b->x;
    f32 y = a->y - b->y;
    f32 z = a->z - b->z;
    return __builtin_sqrtf(x * x + y * y + z * z);
}

// A unit direction added to a list unless it or its opposite is in it already
void AddDirection(Vector4* directions, s32* count, const Vector4* direction)
{
    Vector4 opposite = *direction;
    opposite.x = -opposite.x;
    opposite.y = -opposite.y;
    opposite.z = -opposite.z;
    s32 known = *count;
    s32 index = 0;
    for (; index < known; index++)
    {
        if (Distance(direction, &directions[index]) < SameDirection || Distance(&opposite, &directions[index]) < SameDirection)
        {
            break;
        }
    }

    if (index == *count)
    {
        directions[index] = *direction;
        *count = index + 1;
    }
}
}

CollisionHull* HullConstruct(CollisionHull* hull)
{
    hull->blob = nullptr;
    RetailLibc::MemorySet(hull, 0, HullHeaderSize);
    hull->surface = 0;
    hull->blobSize = 0;
    return hull;
}

void HullDestroy(CollisionHull* hull, u32 flags)
{
    if (hull->blob != nullptr)
    {
        MemoryDeallocate_(hull->blob);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(hull);
    }
}

void* HullCopy(CollisionHull* hull, const CollisionHull* source)
{
    hull->vertexCount = source->vertexCount;
    hull->edgeCount = source->edgeCount;
    hull->planeCount = source->planeCount;
    hull->edgeDirectionCount = source->edgeDirectionCount;
    hull->faceNormalCount = source->faceNormalCount;
    hull->planesOffset = source->planesOffset;
    hull->edgeDirectionsOffset = source->edgeDirectionsOffset;
    hull->faceNormalsOffset = source->faceNormalsOffset;
    hull->faceOffsetsOffset = source->faceOffsetsOffset;
    hull->facesOffset = source->facesOffset;
    hull->edgesOffset = source->edgesOffset;
    hull->surface = source->surface;
    hull->blobSize = source->blobSize;
    hull->blob = static_cast<u8*>(MemoryAllocate2(source->blobSize));
    return RetailLibc::MemoryCopy(hull->blob, source->blob, source->blobSize);
}

void ReadModelCollisionData(CollisionHull* hull, Stream* stream)
{
    stream->Read(hull, HullHeaderSize, 1);
    s32 size;
    stream->ReadS32(&size);
    hull->blobSize = size;
    hull->blob = static_cast<u8*>(MemoryAllocate2(size));
    stream->Read(hull->blob, size, 1);
    auto* vertices = reinterpret_cast<Vector4*>(hull->blob);
    for (s32 index = 0; index < hull->vertexCount; index++)
    {
        vertices[index].w = 1.0f;
    }
}

Vector4* HullVertex(const CollisionHull* hull, s32 index)
{
    return reinterpret_cast<Vector4*>(hull->blob) + index;
}

Vector4* HullPlane(const CollisionHull* hull, s32 index)
{
    return reinterpret_cast<Vector4*>(hull->blob + hull->planesOffset) + index;
}

Vector4* HullEdgeDirections(const CollisionHull* hull)
{
    return reinterpret_cast<Vector4*>(hull->blob + hull->edgeDirectionsOffset);
}

Vector4* HullFaceNormals(const CollisionHull* hull)
{
    return reinterpret_cast<Vector4*>(hull->blob + hull->faceNormalsOffset);
}

void GetHullBounds(const CollisionHull* hull, const Matrix4x4* matrix, BoundingVolume* volume)
{
    Box box;
    GetBbox(reinterpret_cast<const Vector4*>(hull->blob), hull->vertexCount, &box, matrix);
    SetBoundingVolume(volume, &box.min, &box.max);
}

void GetHullBoundingBox(const CollisionHull* hull, const Matrix4x4* matrix, Box* box)
{
    Box bounds;
    GetBbox(reinterpret_cast<const Vector4*>(hull->blob), hull->vertexCount, &bounds, matrix);
    *box = bounds;
}

void HullBox(const CollisionHull* hull, Box* box)
{
    Box bounds;
    PointsBox(reinterpret_cast<const Vector4*>(hull->blob), hull->vertexCount, &bounds);
    *box = bounds;
}

bool IsPointInsideHull(const CollisionHull* hull, const Vector4* point)
{
    const Vector4* plane = HullPlane(hull, 0);
    for (s32 index = 0; index < hull->planeCount; index++, plane++)
    {
        if (PlaneSide(OnHullPlane, plane, point) == InFrontOfPlane)
        {
            return false;
        }
    }

    return true;
}

bool IsPointInsideHullAt(const CollisionHull* hull, const Vector4* point, const Matrix4x4* matrix)
{
    Vector4 moved;
    VuTransformPoint(matrix, point, &moved);
    const Vector4* plane = HullPlane(hull, 0);
    for (s32 index = 0; index < hull->planeCount; index++, plane++)
    {
        if (PlaneSide(OnHullPlane, plane, &moved) == InFrontOfPlane)
        {
            return false;
        }
    }

    return true;
}

s32 HullSideOfPlane(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* plane)
{
    bool front = false;
    bool behind = false;
    const Vector4* vertex = reinterpret_cast<const Vector4*>(hull->blob);
    for (s32 index = 0; index < hull->vertexCount; index++, vertex++)
    {
        Vector4 moved;
        VuTransformPoint(matrix, vertex, &moved);
        if (plane->x * moved.x + plane->y * moved.y + plane->z * moved.z + plane->w < 0.0f)
        {
            behind = true;
        }
        else
        {
            front = true;
        }

        if (behind && front)
        {
            return 0;
        }
    }

    return front ? 1 : -1;
}

u32 HullsSeparatedAlongAxis(const CollisionHull* hull, const Vector4* axis, const Vector4* points, s32 count, f32* depth)
{
    f32 hullMin = Infinite;
    f32 hullMax = -Infinite;
    f32 pointsMin = Infinite;
    f32 pointsMax = -Infinite;
    const Vector4* vertex = reinterpret_cast<const Vector4*>(hull->blob);
    for (u32 left = hull->vertexCount; left != 0; left--, vertex++)
    {
        f32 along = axis->x * vertex->x + axis->y * vertex->y + axis->z * vertex->z;
        if (along < hullMin)
        {
            hullMin = along;
        }

        if (hullMax < along)
        {
            hullMax = along;
        }
    }

    for (s32 left = count; left > 0; left--, points++)
    {
        f32 along = axis->x * points->x + axis->y * points->y + axis->z * points->z;
        if (along < pointsMin)
        {
            pointsMin = along;
        }

        if (pointsMax < along)
        {
            pointsMax = along;
        }
    }

    if (hullMin < pointsMin)
    {
        if (hullMax < pointsMin)
        {
            return 1;
        }

        if (depth != nullptr)
        {
            if (pointsMax < hullMax)
            {
                f32 back = pointsMax - hullMin;
                f32 forward = hullMax - pointsMin;
                *depth = back <= forward ? -back : forward;
            }
            else
            {
                *depth = pointsMin - hullMax;
            }
        }

        return 0;
    }

    if (pointsMax < hullMin)
    {
        return 1;
    }

    if (depth != nullptr)
    {
        if (pointsMax < hullMax)
        {
            *depth = pointsMax - hullMin;
        }
        else
        {
            f32 forward = hullMax - pointsMin;
            f32 back = pointsMax - hullMin;
            *depth = forward <= back ? -forward : back;
        }
    }

    return 0;
}

u32 HullsIntersect(const CollisionHull* hull, const CollisionHull* other, const Matrix4x4* matrix)
{
    Vector4 points[MostHullPoints];
    const Vector4* normals = HullFaceNormals(hull);
    const Vector4* directions = HullEdgeDirections(hull);
    const Vector4* otherNormals = HullFaceNormals(other);
    const Vector4* otherDirections = HullEdgeDirections(other);
    const Vector4* vertex = reinterpret_cast<const Vector4*>(other->blob);
    for (s32 index = 0; index < other->vertexCount; index++)
    {
        VuTransformPoint(matrix, &vertex[index], &points[index]);
    }

    for (s32 index = 0; index < hull->faceNormalCount; index++)
    {
        if (HullsSeparatedAlongAxis(hull, &normals[index], points, other->vertexCount, nullptr) != 0)
        {
            return 0;
        }
    }

    Vector4 axis;
    for (s32 index = 0; index < other->faceNormalCount; index++)
    {
        VuRotateVector(matrix, &otherNormals[index], &axis);
        if (HullsSeparatedAlongAxis(hull, &axis, points, other->vertexCount, nullptr) != 0)
        {
            return 0;
        }
    }

    for (s32 index = 0; index < hull->edgeDirectionCount; index++)
    {
        const Vector4* direction = &directions[index];
        for (s32 otherIndex = 0; otherIndex < other->edgeDirectionCount; otherIndex++)
        {
            VuRotateVector(matrix, &otherDirections[otherIndex], &axis);
            Vector4 cross;
            cross.x = direction->y * axis.z - direction->z * axis.y;
            cross.y = direction->z * axis.x - direction->x * axis.z;
            cross.z = direction->x * axis.y - direction->y * axis.x;
            cross.w = 1.0f;
            if (LengthEpsilon < cross.x * cross.x + cross.y * cross.y + cross.z * cross.z &&
                HullsSeparatedAlongAxis(hull, &cross, points, other->vertexCount, nullptr) != 0)
            {
                return 0;
            }
        }
    }

    return 1;
}

u8 HullsTouch(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix)
{
    Matrix4x4 inverse;
    VuInvertRigid(&inverse, matrix);
    Matrix4x4 relative;
    VuMultiplyMatrices(otherMatrix, &inverse, &relative);
    return static_cast<u8>(HullsIntersect(hull, other, &relative));
}

void BeginHullBuild()
{
    g_HullBuilderPointCount = 0;
    g_HullBuilding = 1;
    g_HullBuilderCounts.edges = 0;
    g_HullBuilderCounts.faces = 0;
    g_HullBuilderCounts.edgeDirections = 0;
    g_HullBuilderCounts.faceNormals = 0;
}

void EndHullPoints()
{
    g_HullBuilding = 0;
}

u32 FinishHullBuild(CollisionHull* hull)
{
    if (CollectHullEdges() == 0 || CollectHullEdgeDirections() == 0 || CollectHullPlanesAndNormals() == 0)
    {
        return 0;
    }

    return PackBuiltHull(hull) != 0;
}

u8 AddHullPoint(const Vector4* point)
{
    s32 count = g_HullBuilderPointCount;
    for (s32 index = 0; index < count; index++)
    {
        if (Distance(&g_HullBuilderPoints[index], point) < SamePoint)
        {
            return static_cast<u8>(index);
        }
    }

    g_HullBuilderPointCount = count + 1;
    g_HullBuilderPoints[count] = *point;
    return static_cast<u8>(count);
}

void AddHullFace(const u8* points, s32 count)
{
    for (s32 index = 0; index < count; index++)
    {
        for (s32 other = index + 1; other < count; other++)
        {
            if (points[index] == points[other])
            {
                return;
            }
        }
    }

    s32 face = g_HullBuilderCounts.faces;
    g_HullBuilderFaceSizes[face] = static_cast<u8>(count);
    for (s32 index = 0; index < count; index++)
    {
        g_HullBuilderFaces[face][index] = points[index];
    }

    g_HullBuilderCounts.faces++;
}

void AddHullTriangleFace(u8 first, u8 second, u8 third)
{
    u8 points[3] = {first, second, third};
    AddHullFace(points, 3);
}

void AddHullQuadFace(u8 first, u8 second, u8 third, u8 fourth)
{
    u8 points[4] = {first, second, third, fourth};
    AddHullFace(points, 4);
}

void AddHullTriangle(const Vector4* first, const Vector4* second, const Vector4* third)
{
    u8 firstPoint = AddHullPoint(first);
    u8 secondPoint = AddHullPoint(second);
    u8 thirdPoint = AddHullPoint(third);
    AddHullTriangleFace(firstPoint, secondPoint, thirdPoint);
}

void AddHullQuad(const Vector4* first, const Vector4* second, const Vector4* third, const Vector4* fourth)
{
    u8 firstPoint = AddHullPoint(first);
    u8 secondPoint = AddHullPoint(second);
    u8 thirdPoint = AddHullPoint(third);
    u8 fourthPoint = AddHullPoint(fourth);
    AddHullQuadFace(firstPoint, secondPoint, thirdPoint, fourthPoint);
}

u32 CollectHullEdges()
{
    s32 faces = g_HullBuilderCounts.faces;
    g_HullBuilderCounts.edges = 0;
    for (s32 face = 0; face < faces; face++)
    {
        u32 size = g_HullBuilderFaceSizes[face];
        if (size == 0)
        {
            continue;
        }

        for (u32 corner = 0; corner < size; corner++)
        {
            u8 from = g_HullBuilderFaces[face][corner];
            u8 to = g_HullBuilderFaces[face][(corner + 1) % size];
            s32 known = g_HullBuilderCounts.edges;
            s32 index = 0;
            for (; index < known; index++)
            {
                const u8* edge = g_HullBuilderEdges[index];
                if ((edge[0] == from && edge[1] == to) || (edge[0] == to && edge[1] == from))
                {
                    break;
                }
            }

            if (index == known)
            {
                g_HullBuilderEdges[index][0] = from;
                g_HullBuilderEdges[index][1] = to;
                g_HullBuilderCounts.edges = index + 1;
            }
        }
    }

    return 1;
}

u32 CollectHullEdgeDirections()
{
    g_HullBuilderCounts.edgeDirections = 0;
    for (s32 face = 0; face < g_HullBuilderCounts.faces; face++)
    {
        if (g_HullBuilderFaceSizes[face] == 0)
        {
            continue;
        }

        for (u32 corner = 0; corner < g_HullBuilderFaceSizes[face]; corner++)
        {
            const Vector4* from = &g_HullBuilderPoints[g_HullBuilderFaces[face][corner]];
            const Vector4* to = &g_HullBuilderPoints[g_HullBuilderFaces[face][(corner + 1) % g_HullBuilderFaceSizes[face]]];
            Vector4 direction;
            direction.x = from->x - to->x;
            direction.y = from->y - to->y;
            direction.z = from->z - to->z;
            direction.w = 1.0f;
            f32 inverse = InverseLength(&direction, LengthEpsilon);
            direction.x = direction.x * inverse;
            direction.y = direction.y * inverse;
            direction.z = direction.z * inverse;
            AddDirection(g_HullBuilderEdgeDirections, &g_HullBuilderCounts.edgeDirections, &direction);
        }
    }

    return 1;
}

u32 CollectHullPlanesAndNormals()
{
    g_HullBuilderCounts.faceNormals = 0;
    for (s32 face = 0; face < g_HullBuilderCounts.faces; face++)
    {
        u8* corners = g_HullBuilderFaces[face];
        Vector4 plane;
        PlaneFromTriangle(&plane, &g_HullBuilderPoints[corners[0]], &g_HullBuilderPoints[corners[1]],
                          &g_HullBuilderPoints[corners[2]]);
        bool front = false;
        bool behind = false;
        const Vector4* point = g_HullBuilderPoints;
        for (s32 left = g_HullBuilderPointCount; left > 0; left--, point++)
        {
            f32 distance = plane.x * point->x + plane.y * point->y + plane.z * point->z + plane.w;
            if (OffPlane < distance)
            {
                front = true;
            }

            if (distance < -OffPlane)
            {
                behind = true;
            }
        }

        // The hull on the plane's front: the face turned around (a face with points on both sides is no face of a convex hull)
        if (front)
        {
            if (behind)
            {
                return 0;
            }

            plane.x = -plane.x;
            plane.y = -plane.y;
            plane.z = -plane.z;
            plane.w = -plane.w;
            u32 size = g_HullBuilderFaceSizes[face];
            for (u32 index = 0; index < size >> 1; index++)
            {
                u8 swapped = corners[index];
                corners[index] = corners[size - 1 - index];
                corners[size - 1 - index] = swapped;
            }
        }

        g_HullBuilderPlanes[face] = plane;
        Vector4 normal;
        normal.x = plane.x;
        normal.y = plane.y;
        normal.z = plane.z;
        normal.w = 1.0f;
        f32 inverse = InverseLength(&normal, LengthEpsilon);
        normal.x = normal.x * inverse;
        normal.y = normal.y * inverse;
        normal.z = normal.z * inverse;
        AddDirection(g_HullBuilderFaceNormals, &g_HullBuilderCounts.faceNormals, &normal);
    }

    return 1;
}

u32 PackBuiltHull(CollisionHull* hull)
{
    constexpr u32 VectorSize = sizeof(Vector4);
    hull->vertexCount = static_cast<u16>(g_HullBuilderPointCount);
    hull->edgeCount = static_cast<u16>(g_HullBuilderCounts.edges);
    hull->planeCount = static_cast<u16>(g_HullBuilderCounts.faces);
    hull->edgeDirectionCount = static_cast<u16>(g_HullBuilderCounts.edgeDirections);
    hull->faceNormalCount = static_cast<u16>(g_HullBuilderCounts.faceNormals);
    s32 faceBytes = 0;
    s32 faces = g_HullBuilderCounts.faces;
    for (s32 face = 0; face < faces; face++)
    {
        faceBytes += FaceCountSize + g_HullBuilderFaceSizes[face];
    }

    u32 verticesSize = hull->vertexCount * VectorSize;
    u32 planesSize = hull->planeCount * VectorSize;
    u32 directionsSize = hull->edgeDirectionCount * VectorSize;
    u32 normalsSize = hull->faceNormalCount * VectorSize;
    u32 edgesSize = hull->edgeCount * EdgeSize;
    u32 planesOffset = verticesSize;
    u32 directionsOffset = planesOffset + planesSize;
    u32 normalsOffset = directionsOffset + directionsSize;
    u32 faceOffsetsOffset = normalsOffset + normalsSize;
    u32 facesOffset = faceOffsetsOffset + hull->planeCount;
    u32 edgesOffset = facesOffset + faceBytes;
    hull->planesOffset = static_cast<u16>(planesOffset);
    hull->edgesOffset = static_cast<u16>(edgesOffset);
    hull->faceOffsetsOffset = static_cast<u16>(faceOffsetsOffset);
    hull->facesOffset = static_cast<u16>(facesOffset);
    hull->edgeDirectionsOffset = static_cast<u16>(directionsOffset);
    hull->faceNormalsOffset = static_cast<u16>(normalsOffset);
    u32 size = static_cast<u16>(edgesOffset) + edgesSize;
    hull->blobSize = size;
    u8* blob = static_cast<u8*>(MemoryAllocate2(size));
    hull->blob = blob;
    RetailLibc::MemoryCopy(blob, g_HullBuilderPoints, verticesSize);
    RetailLibc::MemoryCopy(blob + hull->planesOffset, g_HullBuilderPlanes, planesSize);
    RetailLibc::MemoryCopy(blob + hull->edgeDirectionsOffset, g_HullBuilderEdgeDirections, directionsSize);
    RetailLibc::MemoryCopy(blob + hull->faceNormalsOffset, g_HullBuilderFaceNormals, normalsSize);
    // Every face's offset among the face bytes, then the faces (a byte offset, as the game finds them)
    u8 at = 0;
    for (s32 face = 0; face < g_HullBuilderCounts.faces; face++)
    {
        u32 size = g_HullBuilderFaceSizes[face];
        blob[hull->faceOffsetsOffset + face] = at;
        blob[hull->facesOffset + at] = static_cast<u8>(size);
        at += FaceCountSize;
        for (u32 corner = 0; corner < size; corner++)
        {
            blob[hull->facesOffset + at] = g_HullBuilderFaces[face][corner];
            at++;
        }
    }

    RetailLibc::MemoryCopy(blob + hull->edgesOffset, g_HullBuilderEdges, edgesSize);
    return 1;
}

void BuildBoxHull(CollisionHull* hull, const Vector4* min, const Vector4* max)
{
    // Corners from the bottom of the near side around, the planes of the bottom, near, right, far, left and top faces (their four
    // corners counter-clockwise from outside), an edge direction and a face normal along each axis, and the edges
    constexpr u16 CornerCount = 8;
    constexpr u16 FaceCount = 6;
    constexpr u16 FaceCorners = 4;
    constexpr u16 AxisCount = 3;
    constexpr u16 EdgeCount = 12;
    static const u8 FaceOffsets[FaceCount] = {0, 5, 10, 15, 20, 25};
    static const u8 FaceBytes[FaceCount * (FaceCountSize + FaceCorners)] = {4, 0, 1, 2, 3, 4, 0, 4, 5, 1, 4, 1, 5, 6, 2,
                                                                            4, 3, 2, 6, 7, 4, 3, 7, 4, 0, 4, 5, 4, 7, 6};
    static const u8 EdgeBytes[EdgeCount * EdgeSize] = {0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 4, 5, 5, 1, 5, 6, 6, 2, 6, 7, 7, 3, 7, 4};
    // The blob's parts one after the other
    constexpr u16 PlanesOffset = CornerCount * sizeof(Vector4);
    constexpr u16 EdgeDirectionsOffset = PlanesOffset + FaceCount * sizeof(Vector4);
    constexpr u16 FaceNormalsOffset = EdgeDirectionsOffset + AxisCount * sizeof(Vector4);
    constexpr u16 FaceOffsetsOffset = FaceNormalsOffset + AxisCount * sizeof(Vector4);
    constexpr u16 FacesOffset = FaceOffsetsOffset + sizeof(FaceOffsets);
    constexpr u16 EdgesOffset = FacesOffset + sizeof(FaceBytes);
    constexpr u32 BlobSize = EdgesOffset + sizeof(EdgeBytes);
    hull->planeCount = FaceCount;
    hull->edgeDirectionCount = AxisCount;
    hull->faceNormalCount = AxisCount;
    hull->vertexCount = CornerCount;
    hull->edgeCount = EdgeCount;
    hull->planesOffset = PlanesOffset;
    hull->faceNormalsOffset = FaceNormalsOffset;
    hull->faceOffsetsOffset = FaceOffsetsOffset;
    hull->facesOffset = FacesOffset;
    hull->edgesOffset = EdgesOffset;
    hull->blobSize = BlobSize;
    hull->edgeDirectionsOffset = EdgeDirectionsOffset;
    u8* blob = static_cast<u8*>(MemoryAllocate2(BlobSize));
    hull->blob = blob;
    auto* vertices = reinterpret_cast<Vector4*>(blob);
    const Vector4* corners[CornerCount][3] = {
        {min, min, min}, {max, min, min}, {max, min, max}, {min, min, max},
        {min, max, min}, {max, max, min}, {max, max, max}, {min, max, max},
    };
    for (u32 index = 0; index < CornerCount; index++)
    {
        vertices[index].x = corners[index][0]->x;
        vertices[index].y = corners[index][1]->y;
        vertices[index].z = corners[index][2]->z;
        vertices[index].w = 1.0f;
    }

    auto* planes = reinterpret_cast<Vector4*>(blob + hull->planesOffset);
    planes[0] = {0.0f, -1.0f, 0.0f, min->y};
    planes[1] = {0.0f, 0.0f, -1.0f, min->z};
    planes[2] = {1.0f, 0.0f, 0.0f, -max->x};
    planes[3] = {0.0f, 0.0f, 1.0f, -max->z};
    planes[4] = {-1.0f, 0.0f, 0.0f, min->x};
    planes[5] = {0.0f, 1.0f, 0.0f, -max->y};
    auto* directions = reinterpret_cast<Vector4*>(blob + hull->edgeDirectionsOffset);
    directions[0] = {-1.0f, 0.0f, 0.0f, 1.0f};
    directions[1] = {0.0f, 0.0f, -1.0f, 1.0f};
    directions[2] = {0.0f, -1.0f, 0.0f, 1.0f};
    auto* normals = reinterpret_cast<Vector4*>(blob + hull->faceNormalsOffset);
    normals[0] = {0.0f, -1.0f, 0.0f, 1.0f};
    normals[1] = {0.0f, 0.0f, -1.0f, 1.0f};
    normals[2] = {1.0f, 0.0f, 0.0f, 1.0f};
    for (u32 index = 0; index < sizeof(FaceOffsets); index++)
    {
        blob[hull->faceOffsetsOffset + index] = FaceOffsets[index];
    }

    for (u32 index = 0; index < sizeof(FaceBytes); index++)
    {
        blob[hull->facesOffset + index] = FaceBytes[index];
    }

    for (u32 index = 0; index < sizeof(EdgeBytes); index++)
    {
        blob[hull->edgesOffset + index] = EdgeBytes[index];
    }
}

u32 BuildTriangleHull(CollisionHull* hull, const Vector4* triangle)
{
    g_HullBuilding = 1;
    g_HullBuilderCounts.edges = 0;
    g_HullBuilderCounts.faces = 0;
    g_HullBuilderCounts.edgeDirections = 0;
    g_HullBuilderCounts.faceNormals = 0;
    g_HullBuilderPointCount = 0;
    AddHullPoint(&triangle[0]);
    AddHullPoint(&triangle[1]);
    AddHullPoint(&triangle[2]);
    // Both sides
    AddHullTriangle(&triangle[0], &triangle[1], &triangle[2]);
    AddHullTriangle(&triangle[0], &triangle[2], &triangle[1]);
    g_HullBuilding = 0;
    return FinishHullBuild(hull);
}

u32 BuildPyramidHull(CollisionHull* hull, const Vector4* points)
{
    g_HullBuilding = 1;
    g_HullBuilderPointCount = 0;
    g_HullBuilderCounts.edges = 0;
    g_HullBuilderCounts.faces = 0;
    g_HullBuilderCounts.edgeDirections = 0;
    g_HullBuilderCounts.faceNormals = 0;
    // The apex, then the base's four corners
    constexpr u32 PyramidPoints = 5;
    for (u32 index = 0; index < PyramidPoints; index++)
    {
        AddHullPoint(&points[index]);
    }

    AddHullTriangle(&points[0], &points[2], &points[1]);
    AddHullTriangle(&points[0], &points[1], &points[4]);
    AddHullTriangle(&points[0], &points[4], &points[3]);
    AddHullTriangle(&points[0], &points[3], &points[2]);
    AddHullQuad(&points[1], &points[2], &points[3], &points[4]);
    g_HullBuilding = 0;
    return FinishHullBuild(hull);
}

void TransformHull(CollisionHull* hull, const Matrix4x4* matrix)
{
    u8* blob = hull->blob;
    auto* planes = reinterpret_cast<Vector4*>(blob + hull->planesOffset);
    const u8* faceOffsets = blob + hull->faceOffsetsOffset;
    const u8* faces = blob + hull->facesOffset;
    auto* vertices = reinterpret_cast<Vector4*>(blob);
    for (s32 index = 0; index < hull->vertexCount; index++)
    {
        VuTransformPoint(matrix, &vertices[index], &vertices[index]);
    }

    for (s32 index = 0; index < hull->planeCount; index++)
    {
        const u8* face = faces + faceOffsets[index];
        PlaneThroughTriangle(&planes[index], &vertices[face[1]], &vertices[face[2]], &vertices[face[3]]);
    }
}

void HullSupportPoint(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* direction, Vector4* out)
{
    f32 furthest = -Infinite;
    const Vector4* vertex = HullVertex(hull, 0);
    for (s32 index = 0; index < hull->vertexCount; index++, vertex++)
    {
        Vector4 point;
        VuTransformPoint(matrix, vertex, &point);
        f32 along = point.x * direction->x + point.y * direction->y + point.z * direction->z;
        if (furthest < along)
        {
            furthest = along;
            *out = point;
        }
    }
}

void HullSupportEdge(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* direction, Vector4* start, Vector4* end)
{
    f32 furthest = -Infinite;
    const Vector4* vertices = HullVertex(hull, 0);
    const u8* edge = hull->blob + hull->edgesOffset;
    for (s32 index = 0; index < hull->edgeCount; index++, edge += EdgeSize)
    {
        Vector4 first;
        VuTransformPoint(matrix, &vertices[edge[0]], &first);
        Vector4 second;
        VuTransformPoint(matrix, &vertices[edge[1]], &second);
        f32 firstAlong = first.x * direction->x + first.y * direction->y + first.z * direction->z;
        f32 secondAlong = second.x * direction->x + second.y * direction->y + second.z * direction->z;
        f32 nearer = __builtin_fminf(secondAlong, firstAlong);
        if (furthest < nearer)
        {
            furthest = nearer;
            *start = first;
            *end = second;
        }
    }
}

namespace
{
f32 PlaneDistance(const Vector4* plane, const Vector4* point)
{
    return plane->x * point->x + plane->y * point->y + plane->z * point->z + plane->w;
}

f32 Dot3(const Vector4* first, const Vector4* second)
{
    return first->x * second->x + first->y * second->y + first->z * second->z;
}
}

u32 SphereInPlanes(f32 radius, const CollisionHull* hull, const Vector4* centre, const Vector4* planes, const Vector4* vertices,
                   Vector4* push)
{
    f32 distances[MostHullFaces];
    s32 planeCount = hull->planeCount;
    for (s32 index = 0; index < planeCount; index++)
    {
        f32 distance = PlaneDistance(&planes[index], centre);
        distances[index] = distance;
        if (radius < distance)
        {
            return 0;
        }
    }

    // Behind the planes up to the first it's in front of: the nearest of them
    s32 nearest = -1;
    f32 largest = -Infinite;
    s32 behind = 0;
    if (planeCount != 0 && !(0.0f < distances[0]))
    {
        while (true)
        {
            if (largest < distances[behind])
            {
                largest = distances[behind];
                nearest = behind;
            }

            behind++;
            if (behind >= planeCount || 0.0f < distances[behind])
            {
                break;
            }
        }
    }

    if (behind == planeCount)
    {
        // Inside: out through the nearest plane
        const Vector4* plane = &planes[nearest];
        f32 out = radius - largest;
        push->w = 1.0f;
        push->z = plane->z * out;
        push->x = plane->x * out;
        push->y = plane->y * out;
        return 1;
    }

    // Over a face it's in front of: inside every plane through the face's edges along its normal
    const u8* faceOffsets = hull->blob + hull->faceOffsetsOffset;
    const u8* faces = hull->blob + hull->facesOffset;
    for (s32 index = 0; index < hull->planeCount; index++)
    {
        if (!(0.0f < distances[index]))
        {
            continue;
        }

        const Vector4* plane = &planes[index];
        Vector4 normal = {plane->x, plane->y, plane->z, 1.0f};
        const u8* face = faces + faceOffsets[index];
        u32 corners = face[0];
        u32 corner = 0;
        for (; corner < corners; corner++)
        {
            const Vector4* from = &vertices[face[1 + corner]];
            const Vector4* to = &vertices[face[corner == corners - 1 ? 1 : 2 + corner]];
            Vector4 above = {from->x + normal.x, from->y + normal.y, from->z + normal.z, 1.0f};
            Vector4 side;
            PlaneFromTriangle(&side, from, to, &above);
            if (0.0f < PlaneDistance(&side, centre))
            {
                break;
            }
        }

        if (corner == corners)
        {
            f32 out = radius - distances[index];
            push->w = 1.0f;
            push->z = plane->z * out;
            push->x = plane->x * out;
            push->y = plane->y * out;
            return 1;
        }
    }

    // Beside an edge (between its ends along it): the nearest within the radius
    f32 radiusSquared = radius * radius;
    f32 nearestSquared = radiusSquared;
    nearest = -1;
    const u8* edges = hull->blob + hull->edgesOffset;
    for (s32 index = 0; index < hull->edgeCount; index++)
    {
        const Vector4* from = &vertices[edges[index * EdgeSize]];
        const Vector4* to = &vertices[edges[index * EdgeSize + 1]];
        Vector4 direction = {to->x - from->x, to->y - from->y, to->z - from->z, 1.0f};
        f32 inverse = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;
        f32 centreAlong = Dot3(centre, &direction);
        f32 fromAlong = Dot3(from, &direction);
        f32 toAlong = Dot3(to, &direction);
        if (!((centreAlong - fromAlong) * (centreAlong - toAlong) < 0.0f))
        {
            continue;
        }

        Vector4 apart = {from->x - centre->x, from->y - centre->y, from->z - centre->z, 1.0f};
        f32 along = Dot3(&apart, &direction);
        Vector4 onLine = {direction.x * along, direction.y * along, direction.z * along, 1.0f};
        Vector4 off = {apart.x - onLine.x, apart.y - onLine.y, apart.z - onLine.z, 1.0f};
        f32 squared = off.x * off.x + off.y * off.y + off.z * off.z;
        if (squared < nearestSquared)
        {
            nearestSquared = squared;
            nearest = index;
        }
    }

    if (nearest != -1)
    {
        // Out of the edge: away from its nearest point
        const Vector4* from = &vertices[edges[nearest * EdgeSize]];
        const Vector4* to = &vertices[edges[nearest * EdgeSize + 1]];
        Vector4 direction = {to->x - from->x, to->y - from->y, to->z - from->z, 1.0f};
        f32 inverse = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;
        Vector4 apart = {from->x - centre->x, from->y - centre->y, from->z - centre->z, 1.0f};
        f32 along = Dot3(&apart, &direction);
        Vector4 onLine = {direction.x * along, direction.y * along, direction.z * along, 1.0f};
        Vector4 toward = {apart.x - onLine.x, apart.y - onLine.y, apart.z - onLine.z, 1.0f};
        f32 inverseToward = InverseLength(&toward, LengthEpsilon);
        f32 into = __builtin_sqrtf(nearestSquared) - radius;
        toward.x = toward.x * inverseToward;
        toward.z = toward.z * inverseToward;
        toward.y = toward.y * inverseToward;
        *push = {toward.x * into, toward.y * into, toward.z * into, 1.0f};
        return 1;
    }

    // The nearest corner within the radius
    nearestSquared = radiusSquared;
    for (s32 index = 0; index < hull->vertexCount; index++)
    {
        Vector4 apart = {vertices[index].x - centre->x, vertices[index].y - centre->y, vertices[index].z - centre->z, 1.0f};
        f32 squared = apart.x * apart.x + apart.y * apart.y + apart.z * apart.z;
        if (squared < nearestSquared)
        {
            nearestSquared = squared;
            nearest = index;
        }
    }

    if (nearest == -1)
    {
        return 0;
    }

    const Vector4* corner = &vertices[nearest];
    *push = {centre->x - corner->x, centre->y - corner->y, centre->z - corner->z, 1.0f};
    f32 inverse = InverseLength(push, LengthEpsilon);
    f32 out = radius - __builtin_sqrtf(nearestSquared);
    push->x = push->x * inverse * out;
    push->z = push->z * inverse * out;
    push->y = push->y * inverse * out;
    return 1;
}

u32 SphereInHull(f32 radius, const CollisionHull* hull, const Vector4* centre, Vector4* push)
{
    return SphereInPlanes(radius, hull, centre, HullPlane(hull, 0), HullVertex(hull, 0), push);
}

u32 EllipsoidTouchesHull(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* radii, const Matrix4x4* hullMatrix,
                         Vector4* push, Vector4* normal)
{
    // The hull in the ellipsoid's space, scaled into its unit sphere
    Matrix4x4 inverse = *matrix;
    VuInvertRigidInPlace(&inverse);
    Matrix4x4 local;
    VuMultiplyMatrices(hullMatrix, &inverse, &local);
    Matrix4x4 unscale;
    InitIdentityMatrix(&unscale);
    unscale.m[0][0] = 1.0f / radii->x;
    unscale.m[1][1] = 1.0f / radii->y;
    unscale.m[2][2] = 1.0f / radii->z;
    Matrix4x4 unit;
    VuMultiplyMatrices(&local, &unscale, &unit);
    Vector4 vertices[MostHullPoints];
    Vector4 planes[MostHullFaces];
    const Vector4* vertex = HullVertex(hull, 0);
    for (s32 index = 0; index < hull->vertexCount; index++)
    {
        VuTransformPoint(&unit, &vertex[index], &vertices[index]);
    }

    const u8* faceOffsets = hull->blob + hull->faceOffsetsOffset;
    const u8* faces = hull->blob + hull->facesOffset;
    for (s32 index = 0; index < hull->planeCount; index++)
    {
        const u8* face = faces + faceOffsets[index];
        PlaneThroughTriangle(&planes[index], &vertices[face[1]], &vertices[face[2]], &vertices[face[3]]);
    }

    u32 touches = SphereInPlanes(1.0f, hull, &g_DefaultBox.min, planes, vertices, push);
    if (touches == 0)
    {
        return 0;
    }

    // Back out of the unit sphere: the push scaled up, the normal down
    *normal = *push;
    push->x = push->x * radii->x;
    push->y = push->y * radii->y;
    push->z = push->z * radii->z;
    normal->x = normal->x / radii->x;
    normal->y = normal->y / radii->y;
    normal->z = normal->z / radii->z;
    VuRotateVector(matrix, push, push);
    VuRotateVector(matrix, normal, normal);
    f32 inverseLength = InverseLength(normal, LengthEpsilon);
    normal->x = normal->x * inverseLength;
    normal->y = normal->y * inverseLength;
    normal->z = normal->z * inverseLength;
    return touches;
}
