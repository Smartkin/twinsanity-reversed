#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/volumes.h"

class Stream;

// A convex hull (0x20 bytes, TT Lab's TwinCollisionHull, the game's ModelCollisionData): how many of each part its blob has, where
// each part but the vertexes (first) starts in it, the collision surface (only the RM2's older layout gives one), the blob's size
// and the blob. The blob holds the vertexes (W 1), one outward plane per face (a point is inside where n·p + w <= 0), the unique
// edge directions and face normals (the separating axes of hull against hull tests), every face's offset among the face bytes,
// the faces (their vertex count, then their vertexes counter-clockwise from outside) and the edges (two vertex bytes each)
struct CollisionHull
{
    u16 vertexCount;
    u16 edgeCount;
    u16 planeCount;
    u16 edgeDirectionCount;
    u16 faceNormalCount;
    u16 planesOffset;
    u16 edgeDirectionsOffset;
    u16 faceNormalsOffset;
    u16 faceOffsetsOffset;
    u16 facesOffset;
    u16 edgesOffset;
    s16 surface;
    u32 blobSize;
    u8* blob;
};
CHECK_OFFSET(CollisionHull, surface, 0x16);
CHECK_SIZE(CollisionHull, 0x20);

// The room the hull builder has: points, faces (each with a plane and at most one normal), a face's corners, and edges (each with
// at most one direction); the hulls the game tests take at most as many points
constexpr s32 MostHullPoints = 64;
constexpr s32 MostHullFaces = 64;
constexpr s32 MostFaceCorners = 24;
constexpr s32 MostHullEdges = 72;

// How much of a hull the builder has (HullBuilderCounts; the points are counted apart): faces, edge directions, face normals,
// edges
struct HullBuildCounts
{
    s32 faces;
    s32 edgeDirections;
    s32 faceNormals;
    s32 edges;
};

extern "C"
{
    // Nothing counted, no surface, no blob; the blob freed; a copy with a blob of its own (what the copy returns)
    CollisionHull* HullConstruct(CollisionHull* hull) RETAIL(InitHull);
    void HullDestroy(CollisionHull* hull, u32 flags) RETAIL(FUN_00200590);
    void* HullCopy(CollisionHull* hull, const CollisionHull* source) RETAIL(FUN_002005e0);
    // The counts and offsets, the blob's size and the blob (every vertex's W made 1)
    void ReadModelCollisionData(CollisionHull* hull, Stream* stream);
    // A vertex, a plane, the edge directions and the face normals
    Vector4* HullVertex(const CollisionHull* hull, s32 index) RETAIL(FUN_00201050);
    Vector4* HullPlane(const CollisionHull* hull, s32 index) RETAIL(FUN_00201060);
    Vector4* HullEdgeDirections(const CollisionHull* hull) RETAIL(FUN_00201078);
    Vector4* HullFaceNormals(const CollisionHull* hull) RETAIL(FUN_00201088);
    // VU0's macro mode (the platform's): the box of points moved by a matrix and the box of points, its corners' w the first
    // point's (moved). The first point counts however few there are
    void GetBbox(const Vector4* points, s32 count, Box* box, const Matrix4x4* matrix);
    void PointsBox(const Vector4* points, s32 count, Box* box) RETAIL(FUN_00201bb8);
    // The hull's box under a matrix (as a bounding volume, as a box), its own
    void GetHullBounds(const CollisionHull* hull, const Matrix4x4* matrix, BoundingVolume* volume);
    void GetHullBoundingBox(const CollisionHull* hull, const Matrix4x4* matrix, Box* box);
    void HullBox(const CollisionHull* hull, Box* box) RETAIL(GetBBFromModelColData);
    // Whether a point is inside the hull (none of its planes has it in front), a point taken through a matrix first
    bool IsPointInsideHull(const CollisionHull* hull, const Vector4* point);
    bool IsPointInsideHullAt(const CollisionHull* hull, const Vector4* point, const Matrix4x4* matrix) RETAIL(FUN_00200d28);
    // A hull moved by a matrix: its vertexes, and its planes made again through its faces' first three
    void TransformHull(CollisionHull* hull, const Matrix4x4* matrix) RETAIL(FUN_002009d8);
    // The point of a hull under a matrix furthest along a direction (the first of the furthest), and the edge whose nearer end
    // is the furthest along it
    void HullSupportPoint(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* direction, Vector4* out)
        RETAIL(FUN_00200dd8);
    void HullSupportEdge(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* direction, Vector4* start, Vector4* end)
        RETAIL(FUN_00200ec0);
    // Where a hull under a matrix is against a plane: 1 in front (or on it), -1 behind, 0 both
    s32 HullSideOfPlane(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* plane) RETAIL(FUN_00200b70);

    // Whether points don't overlap the hull along an axis (their extents along it apart): when they do, how far, signed, when
    // wanted
    u32 HullsSeparatedAlongAxis(const CollisionHull* hull, const Vector4* axis, const Vector4* points, s32 count, f32* depth);
    // Whether a hull meets another taken through a matrix into its space (at most 64 vertexes): no separating axis among the
    // first's face normals, the second's, and the cross products of their edge directions
    u32 HullsIntersect(const CollisionHull* hull, const CollisionHull* other, const Matrix4x4* matrix);
    // The same of two hulls with their places (a rigid matrix each), the second taken into the first's
    u8 HullsTouch(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix)
        RETAIL(FUN_00200b00);
    // Where a segment from start to end gets into a hull under a matrix, its start outside it (in front of a plane): the share of
    // the way and, when wanted, a hit (a CollisionHit) of the face it gets in through (its first three corners) with the hull's
    // surface. 0 when it misses or starts inside
    u32 HullRayCast(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* start, const Vector4* end, f32* share,
                    void* hit) RETAIL(FUN_001fcc00);

    // The hull builder: points (merged within 0.0001) and faces (their points' indexes, none twice) added, then edges, edge
    // directions (merged within 0.001 either way), planes (each turned to face out, failing on faces with points on both sides),
    // face normals (merged the same) and the hull packed. Its state: the points and their count, what else it has, a flag only
    // ever set (1 while points and faces are being added)
    extern s32 g_HullBuilderPointCount RETAIL(HullBuilderPointCount);
    extern HullBuildCounts g_HullBuilderCounts RETAIL(HullBuilderCounts);
    extern u8 g_HullBuilding RETAIL(D_0030A058);
    extern Vector4 g_HullBuilderPoints[MostHullPoints] RETAIL(HullBuilderPoints);
    extern Vector4 g_HullBuilderPlanes[MostHullFaces] RETAIL(HullBuilderPlanes);
    extern Vector4 g_HullBuilderEdgeDirections[MostHullEdges] RETAIL(HullBuilderEdgeDirections);
    extern Vector4 g_HullBuilderFaceNormals[MostHullFaces] RETAIL(HullBuilderFaceNormals);
    extern u8 g_HullBuilderFaceSizes[MostHullFaces] RETAIL(HullBuilderFaceSizes);
    extern u8 g_HullBuilderFaces[MostHullFaces][MostFaceCorners] RETAIL(HullBuilderFaces);
    extern u8 g_HullBuilderEdges[MostHullEdges][2] RETAIL(HullBuilderEdges);
    void BeginHullBuild() RETAIL(FUN_00200690);
    void EndHullPoints() RETAIL(FUN_002006b0);
    u32 FinishHullBuild(CollisionHull* hull) RETAIL(FUN_002006b8);
    u8 AddHullPoint(const Vector4* point) RETAIL(FUN_00200710);
    void AddHullFace(const u8* points, s32 count) RETAIL(FUN_00200848);
    void AddHullTriangleFace(u8 first, u8 second, u8 third) RETAIL(FUN_002007d0);
    void AddHullQuadFace(u8 first, u8 second, u8 third, u8 fourth) RETAIL(FUN_00200808);
    // Points and the face of them
    void AddHullTriangle(const Vector4* first, const Vector4* second, const Vector4* third);
    void AddHullQuad(const Vector4* first, const Vector4* second, const Vector4* third, const Vector4* fourth);
    u32 CollectHullEdges();
    u32 CollectHullEdgeDirections();
    u32 CollectHullPlanesAndNormals();
    u32 PackBuiltHull(CollisionHull* hull);
    // Whether a sphere meets a convex hull of planes and vertexes (the hull's own or moved ones, its faces and edges the hull's):
    // the push out of it, from inside through the nearest plane, else out of a face it's over, the nearest edge it's beside or
    // the nearest corner within its radius
    u32 SphereInPlanes(f32 radius, const CollisionHull* hull, const Vector4* centre, const Vector4* planes, const Vector4* vertices,
                       Vector4* push) RETAIL_N32(FUN_001fcfa8);
    // Whether a sphere (its centre in the hull's space) and an ellipsoid (radii along a matrix's axes; the hull taken into its
    // unit sphere) meet a hull: the push out of it (and the normal the ellipsoid's leaves by)
    u32 SphereInHull(f32 radius, const CollisionHull* hull, const Vector4* centre, Vector4* push) RETAIL_N32(FUN_00201020);
    u32 EllipsoidTouchesHull(const CollisionHull* hull, const Matrix4x4* matrix, const Vector4* radii, const Matrix4x4* hullMatrix,
                             Vector4* push, Vector4* normal) RETAIL(FUN_001fd908);
    // A box's hull (its blob made, an old one not freed), a triangle's (both its faces), a pyramid's (a point, then a base of
    // four)
    void BuildBoxHull(CollisionHull* hull, const Vector4* min, const Vector4* max);
    u32 BuildTriangleHull(CollisionHull* hull, const Vector4* triangle);
    u32 BuildPyramidHull(CollisionHull* hull, const Vector4* points);
    // The hull builder's statics made at the start (GCC 2.9x's static initialisation: its arrays of vectors, whose constructor
    // does nothing), and the static constructor that runs it
    void InitHullBuilderStatics(s32 initialise, s32 priority) RETAIL(FUN_001ff110);
    void HullStaticInit() RETAIL(FUN_00201b70);
}

// The box hulls of boxes the agents' code shares (0x130 bytes, made the first time: g_BoxHullCache): the boxes and their hulls,
// and how many there are
struct BoxHullCache
{
    static constexpr s32 Capacity = 8;

    Box boxes[Capacity];
    CollisionHull* hulls[Capacity];
    s32 count;
};
CHECK_OFFSET(BoxHullCache, hulls, 0x100);
CHECK_OFFSET(BoxHullCache, count, 0x120);
CHECK_SIZE(BoxHullCache, 0x130);

extern "C"
{
    extern BoxHullCache* g_BoxHullCache RETAIL(D_0030A05C);
    // Made empty, and the hull of a box (its corners' x, y and z the same): the one made before, else a new one (retail never
    // checks there's room for it)
    BoxHullCache* ConstructBoxHullCache(BoxHullCache* cache) RETAIL(FUN_00201168);
    CollisionHull* BoxHullOf(BoxHullCache* cache, const Vector4* min, const Vector4* max) RETAIL(FUN_001fdc30);
}
