#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/math.h"
#include "game/reference.h"

struct ChunkData;
struct CollisionSurface;

// A chunk's collision (0x28 bytes, TT Lab's collision data: the game's CheckCollisionRayCast 0x27fb58): its version (3001, read
// and never checked), its tree's nodes, the groups of triangles its leaves are, the triangles and the vertexes, each read into
// a disk manager node of its own, and whether they were queued
struct CollisionData
{
    u32 version;
    s32 nodeCount;
    s32 groupCount;
    s32 triangleCount;
    s32 vertexCount;
    s32 nodesHandle;
    s32 groupsHandle;
    s32 trianglesHandle;
    s32 verticesHandle;
    u8 queued;
};
CHECK_SIZE(CollisionData, 0x28);

// A node of the collision tree (0x20 bytes, the tools' collision trigger): its box, the index of its first child (a leaf has
// the bitwise not of its group instead) in the min's w and of its second in the max's
struct CollisionNode
{
    f32 min[3];
    s32 first;
    f32 max[3];
    s32 second;

    const Box* Bounds() const
    {
        return reinterpret_cast<const Box*>(this);
    }
};
CHECK_SIZE(CollisionNode, 0x20);

// The triangles a leaf has: how many, from which
struct CollisionGroup
{
    s32 count;
    u32 first;
};

// A triangle as the collision has it: three 18 bit vertex indexes and a 10 bit surface index
struct CollisionTriangle
{
    enum Bits : u64
    {
        VertexMask = 0x3FFFF,
        SecondShift = 18,
        ThirdShift = 36,
        SurfaceShift = 54,
    };

    u64 packed;
};

// A triangle with its vertexes (0x34 bytes the game copies): the three vertexes, its surface and two bytes nobody writes
struct CollisionHit
{
    Vector4 vertices[3];
    u16 surface;
    u16 unknown32;
};
CHECK_OFFSET(CollisionHit, surface, 0x30);
// Lists of hits are 0x40 apart
CHECK_SIZE(CollisionHit, 0x40);

// Hits kept in blocks of 8 (0x210 bytes: the next one, then its hits 16 bytes in)
struct CollisionHitBlock
{
    CollisionHitBlock* next;
    CollisionHit hits[8];
};
CHECK_OFFSET(CollisionHitBlock, hits, 0x10);
CHECK_SIZE(CollisionHitBlock, 0x210);

// The collision's triangles near an object (still asm but its refresh): the object, the box they were gathered in (the object's
// box grown by the margin, gathered again once the object's box leaves it), the surfaces' bits, how many and the iteration's
// index and block, the first block
struct CollisionCache
{
    ReferencedObject* owner;
    u8 unknown04[0x10 - 0x4];
    Box box;
    f32 margin;
    u32 mask;
    s16 count;
    u16 index;
    CollisionHitBlock* block;
    CollisionHitBlock* first;
};
CHECK_OFFSET(CollisionCache, box, 0x10);
CHECK_OFFSET(CollisionCache, first, 0x40);
CHECK_SIZE(CollisionCache, 0x50);

// A ray cast's result: the distance along the ray (1e30 without a hit), where and the triangle hit when they're wanted
struct RayCastResult
{
    f32 distance;
    Vector4* position;
    CollisionHit* triangle;
};

// A precise ray cast through a collision tree: its arrays, the ray, the surfaces' bits it collides with and the result
struct RayCast
{
    const Vector4* vertices;
    const CollisionNode* nodes;
    const CollisionGroup* groups;
    const CollisionTriangle* triangles;
    const Vector4* start;
    const Vector4* end;
    u32 mask;
    u8 unknown1C[4];
    RayCastResult result;
};
CHECK_OFFSET(RayCast, result, 0x20);

// A fast ray cast through a collision tree (VU0's): the nearest hit (its w the distance), the ray, the arrays and the surfaces'
// bits it collides with
struct FastRayCast
{
    Vector4 nearest;
    Vector4 start;
    Vector4 end;
    const Vector4* vertices;
    const CollisionNode* nodes;
    const CollisionGroup* groups;
    const CollisionTriangle* triangles;
    u32 mask;
};
CHECK_OFFSET(FastRayCast, mask, 0x40);

// What a ray cast through a chunk's instances hit (still asm), and a box query of them: its distance (1e30 none), a bit set when it
// hit, the instance
struct InstanceRayHit
{
    enum Bits : u32
    {
        BitFull = 0x1,
        BitAllWanted = 0x2,
    };

    // The instances found (a box query's), how many and how many fit
    void** results;
    u16 count;
    u16 most;
    f32 distance;
    // Bit 0 set when it hit (a query's results didn't all fit), bit 1: an instance needs all the wanted flags (one of them
    // otherwise)
    u32 bits;
    // The flags of the instances taken (ReferencedObject's), those of the instances left out, two instances left out
    u32 wantedFlags;
    u32 unwantedFlags;
    void* skipped[2];
    void* instance;
};
CHECK_OFFSET(InstanceRayHit, instance, 0x20);

// Reads the collision's header (the section's first 0x14 bytes) and queues its arrays' readers (0x10 bytes)
class CollisionSectionReader : public SectionReader
{
public:
    u8 flags;
    s32 offset;
    CollisionData* data;

    void Destroy(u32 flags) RETAIL(FUN_00282508);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadCollisionData);
};
CHECK_SIZE(CollisionSectionReader, 0x10);

extern "C"
{
    extern const GccVTableEntry g_CollisionSectionReaderVTable[] RETAIL(CollisionDataSectionReader_Methods);
    // How many leaves the last box query found (their groups are in the scripts' state jump table's second half)
    extern s32 g_CollisionLeafCount RETAIL(D_0030A20C);

    CollisionData* ConstructCollisionData(CollisionData* data) RETAIL(FUN_002823c8);
    void DestroyCollisionData(CollisionData* data, u32 destroyFlags) RETAIL(FUN_0027f0c0);
    // The header given read, the readers of the arrays queued (the vertexes with flags 5 rather than 1 when asked)
    void ReadCollisionData(CollisionData* data, u32 flags, s32 offset, const u8* header) RETAIL(FUN_0027f190);
    // The collision's section queued to be read through the holder's collision
    void QueueCollisionSection(CollisionData** holder, s32 offset) RETAIL(FUN_00282580);
    CollisionData** ConstructCollisionHolder(CollisionData** holder, CollisionData* data) RETAIL(FUN_00282568);
    // A triangle's temporary on the stack of the fast ray cast: made and destroyed
    void* ConstructCollisionScratch(void* scratch) RETAIL(FUN_00282628);
    void DestroyCollisionScratch(void* scratch, u32 destroyFlags) RETAIL(FUN_00282630);

    // The leaves whose boxes the box overlaps (at most 256), into g_CollisionLeafCount and the list
    void QueryCollisionBox(CollisionData* data, const Box* box) RETAIL(FUN_0027f310);
    void QueryCollisionNode(CollisionData* data, s32 node, const CollisionNode* nodes, const Box* box) RETAIL(FUN_00282408);
    // A triangle's surface, a hit's
    CollisionSurface* GetCollisionSurface(const CollisionTriangle* triangle);
    CollisionSurface* GetTriangleSurface(const CollisionHit* hit);
    void GetCollisionTriangleWithCoordinates(CollisionHit* hit, const CollisionTriangle* triangle, const Vector4* vertices);
    // A hit's vertexes moved by a matrix, a hit made of three vertexes (surface 0)
    void TransformCollisionHit(CollisionHit* hit, const Matrix4x4* matrix) RETAIL(FUN_002826a8);
    void MakeCollisionHit(CollisionHit* hit, const Vector4* first, const Vector4* second, const Vector4* third)
        RETAIL(FUN_00282708);

    // A surface as the game makes them before reading: no ID, physics values 0.5, no volume scales (-1), no sounds or particles
    // but the surface ID and the scrape sound (left as they were), the bits 12-19 every surface has
    CollisionSurface* ConstructCollisionSurface(CollisionSurface* surface) RETAIL(InitCollisionSurface);
    // The module's static constructor: the game's surfaces made, none stored, the physics' hull plane cache made
    void InitCollisionStatics(u32 initialise, u32 priority) RETAIL(FUN_00281ff0);

    // A surface's particle system (the default chunk's, 0xFFFF none) and sound of a contact kind (0 impact, 1 and 2 steps, 3
    // land, 4 hard impact, 5 scrape), the sound with its volume scale
    u32 GetSurfaceParticle(const CollisionSurface* surface, u32 kind);
    u32 GetSurfaceSoundId(const CollisionSurface* surface, u32 kind) RETAIL(func_002821B8);
    u32 GetSurfaceSound(const CollisionSurface* surface, u32 kind, f32* volume);

    // Whether two boxes overlap (their faces included)
    u32 BoxesOverlap(const Box* box, const Box* other) RETAIL(FUN_001f8db0);
    // Where a ray from start to end crosses a plane: the share of the way, the point
    f32 RayPlaneIntersection(const Vector4* plane, const Vector4* start, const Vector4* end, Vector4* point) RETAIL(FUN_001fcb08);
    // A plane with a normal through a point
    void PlaneFromNormal(Vector4* plane, const Vector4* normal, const Vector4* point) RETAIL(FUN_0018d908);
    // The plane through an edge along a direction (its normal the edge's cross the direction): whether it has one (without, the
    // cross product and the end's W)
    u32 PlaneThroughEdge(Vector4* plane, const Vector4* from, const Vector4* to, const Vector4* direction) RETAIL(FUN_001867e0);
    // VU0's half (still asm): a triangle's plane through its vertexes (whether it has an area), and the six edge tests of a ray
    // against a triangle, started (the plane's sides of the ray's ends into the first two) and gathered from VU0's registers
    u32 PlaneThroughTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third)
        RETAIL(FUN_0018d830);
    void StartTriangleEdgeTests(const Vector4* start, const Vector4* end, const CollisionHit* triangle, f32* values)
        RETAIL(FUN_00293340);
    void FinishTriangleEdgeTests(f32* values) RETAIL(FUN_002933e4);
    // Where a ray from start to end hits a triangle: the share of the way (1e30 when it doesn't), the point
    f32 CheckTriangleIntersection(const CollisionHit* triangle, const Vector4* start, const Vector4* end, Vector4* point);
    // Whether a ray from start to end gets into a box: where it does (its start when it's inside) and the share of the way
    u32 CheckRayIsInsideVolume(const CollisionNode* node, const Vector4* start, const Vector4* end, Vector4* entry, f32* share);
    // The nearest hit of a ray through the node's subtree (the triangles of the surfaces with the ray's bits): precisely, and
    // fast (the triangles tested on Platform::Math's ray tests, a leaf's from its last)
    void CheckCollisionRayCast(CollisionData* data, s32 node, RayCast* cast, RayCastResult* result);
    void CheckCollisionRayCastFast(CollisionData* data, s32 node, FastRayCast* cast);
    // A triangle's box: VU0's (still asm), and through a call
    void TriangleBounds(Box* box, const Vector4* first, const Vector4* second, const Vector4* third) RETAIL(FUN_00201b90);
    void TriangleBox(Box* box, const Vector4* first, const Vector4* second, const Vector4* third) RETAIL(FUN_001fff90);
    // A box's eight corners under a matrix (still asm)
    void BoxCorners(const Box* box, Vector4* corners, const Matrix4x4* matrix) RETAIL(FUN_001fa308);
    // Whether a plane goes through a box (its corners on both sides), whether a box is wholly on the plane's front (none of its
    // corners behind it), whether a triangle touches a box (its plane goes through the box and the box is inside its edges)
    u32 PlaneCrossesBox(const Box* box, const Vector4* plane) RETAIL(FUN_001f8f98);
    u32 BoxInFrontOfPlane(const Box* box, const Vector4* plane) RETAIL(FUN_001f9060);
    u32 TriangleTouchesBox(const Vector4* first, const Vector4* second, const Vector4* third, const Box* box) RETAIL(FUN_00280e20);
    // Whether a sphere touches a triangle and, when wanted, where it pushes the sphere: back through the triangle's plane (by its
    // distance and the radius) when its centre is within the edges, to touching the nearest edge's line, or the radius from the
    // nearest vertex toward the centre
    u32 SphereTouchesTriangle(f32 radius, const CollisionHit* triangle, const Vector4* centre, Vector4* out)
        RETAIL_N32(FUN_002801d8);
    // The same of an ellipsoid with its radii along the axes of a place (a rigid matrix): the triangle taken into the unit
    // sphere's space and the place back out of it
    u32 EllipsoidTouchesTriangle(f32 radiusX, f32 radiusY, f32 radiusZ, const CollisionHit* triangle, const Matrix4x4* matrix,
                                 Vector4* out) RETAIL_N32(FUN_00280ab0);
    // The triangles of the surfaces with the bits that touch a box, at most so many: how many (with their vertexes, or the
    // collision's triangles themselves); of a chunk's collision (none without), and its vertexes
    s32 GatherBoxTriangles(CollisionData* data, const Box* box, u32 mask, CollisionHit* triangles, s32 most) RETAIL(FUN_0027f3f8);
    s32 GatherBoxTrianglePointers(CollisionData* data, const Box* box, u32 mask, const CollisionTriangle** triangles, s32 most)
        RETAIL(FUN_0027f648);
    s32 ChunkBoxTriangles(ChunkData* chunk, const Box* box, u32 mask, CollisionHit* triangles, s32 most) RETAIL(FUN_002828b8);
    s32 ChunkBoxTrianglePointers(ChunkData* chunk, const Box* box, u32 mask, const CollisionTriangle** triangles, s32 most)
        RETAIL(FUN_002828f0);
    const Vector4* ChunkCollisionVertices(ChunkData* chunk) RETAIL(FUN_00282928);
    // Whether a box is inside another (their faces included), a box grown by a margin each way, by a vector's largest coordinate
    // (its absolute value), a box grown to hold another
    u32 BoxInsideBox(const Box* box, const Box* outer) RETAIL(FUN_001f8e58);
    void GrowBox(f32 margin, Box* box) RETAIL_N32(FUN_00200088);
    void GrowBoxByVector(Box* box, const Vector4* vector) RETAIL(FUN_001fffb0);
    void MergeBox(Box* into, const Box* box) RETAIL(MergeBBox);
    // A box holding nothing (its min 1e30, its max -1e30), a box made to hold its corners taken through a matrix
    void ResetBox(Box* box) RETAIL(CreateDefaultBBox);
    void TransformBox(Box* box, const Matrix4x4* matrix) RETAIL(CreateBBoxFromModelMatrix);
    // Whether a sphere and a box overlap and the push that takes the sphere out: through the nearest face when its centre is inside
    // (none when every distance is NaN), straight back beside one face, away from an edge's line or a corner otherwise
    u32 SphereBoxPush(f32 radius, const Box* box, const Vector4* centre, Vector4* push) RETAIL_N32(FUN_001f9118);
    // A box's largest extent along an axis; whether a box holds the space between two corners (none of their faces touching)
    f32 GetBoxReach(const Box* box) RETAIL(GetMaxCoordDistInBB);
    u32 BoxContainsRegion(const Box* box, const Vector4* min, const Vector4* max) RETAIL(FUN_002000e0);
    // A segment (two points) cast through the collision of a chunk's awake instances of the bits (the cells they're sorted
    // into): the share of the way to the nearest hit (1e30 without the cells; the cells' cast is still asm)
    f32 ChunkInstancesRayCast(ChunkData* chunk, const Vector4* segment, u32 mask, InstanceRayHit* hit, u32 flags) RETAIL(FUN_001f1ed0);
    f32 InstanceCellsRayCast(u32* cells, const Vector4* segment, u32 mask, InstanceRayHit* hit, u32 flags) RETAIL(FUN_001e98f8);
    // Whether a segment from start to end hits a chunk's instances: the share of the way and the point when wanted
    u32 SegmentHitsInstances(ChunkData* chunk, const Vector4* start, const Vector4* end, InstanceRayHit* hit, u32 mask, f32* share,
                             Vector4* point, u32 flags) RETAIL(FUN_00282770);
    // Whether a segment from start to end hits the chunk's collision or its instances: the collision first (precise when the
    // triangle hit is wanted), then the instances (the triangle's pointer their cast's flags) up to where the collision stopped it
    // (the retail code takes that point mirrored: start · d + end · (1 - d)), the share of the way (the instances' share of that
    // part) and the point when wanted
    u32 SegmentHitsAnything(ChunkData* chunk, const Vector4* start, const Vector4* end, u32 mask, InstanceRayHit* hit,
                            u32 instanceMask, f32* share, Vector4* point, CollisionHit* triangle) RETAIL(FUN_00281c60);
    // A cache's hits from its first (none when it's empty), and the next one (none past the count)
    CollisionHit* FirstCollisionHit(CollisionCache* cache) RETAIL(FUN_00293298);
    CollisionHit* NextCollisionHit(CollisionCache* cache) RETAIL(FUN_002932c0);
    // The nearest hit of a ray among a cache's triangles: whether there is one, its distance, point and triangle when wanted
    u32 TriangleListRayCast(CollisionCache* cache, const Vector4* start, const Vector4* end, f32* distance, Vector4* point,
                            CollisionHit* triangle) RETAIL(FUN_00293140);
    // SegmentHitsAnything with a cache's triangles in place of the collision
    // A cache's blocks freed, enough of them (empty) made for so many hits
    void FreeCollisionCacheBlocks(CollisionCache* cache) RETAIL(FUN_00292fb8);
    // An instance's query of the instances around it skips it and the one its collision is attached to; the surface of a hull of
    // an object's collision (still asm)
    void SkipInQuery(InstanceRayHit* query, ReferencedObject* object) RETAIL(FUN_001f0458);
    // Whether an instance is one a query takes (none of the unwanted flags, a node of a kind of the mask, all or one of the
    // wanted flags), an instance taken (none when the query is full: its bit 0 set), and the ones a list of a cell's instances
    // has that it takes (how many)
    u32 QueryTakes(const InstanceRayHit* query, const struct InstanceContext* instance, u32 kinds) RETAIL(FUN_001ea978);
    u32 QueryAdd(InstanceRayHit* query, struct InstanceContext* instance) RETAIL(FUN_001f0478);
    u16 QueryCellList(InstanceRayHit* query, struct InstanceContext* first, u32 kinds) RETAIL(FUN_001f05b8);
    // A cache of an object's with a mask (an empty box at the origin, a margin of 1, no blocks), and destroyed (its blocks freed,
    // itself too when the flags say)
    CollisionCache* ConstructCollisionCache(CollisionCache* cache, ReferencedObject* owner, u32 mask) RETAIL(FUN_00292f28);
    void DestroyCollisionCache(CollisionCache* cache, u32 destroyFlags) RETAIL(FUN_00292f70);
    void MakeCollisionCacheBlocks(CollisionCache* cache, s32 count) RETAIL(FUN_00290c58);
    // The cache's triangles gathered again when the object's box isn't inside its box any more (the box then grown by the
    // margin): whether they were
    u32 RefreshCollisionCache(CollisionCache* cache, const Box* box) RETAIL(FUN_00293048);
    u32 SegmentHitsTrianglesOrInstances(ChunkData* chunk, CollisionCache* cache, const Vector4* start, const Vector4* end,
                                        InstanceRayHit* hit, u32 instanceMask, f32* share, Vector4* point, CollisionHit* triangle)
        RETAIL(FUN_002818e8);
    // How far one can see from a point along a way: the way shortened to where the collision (of its bits) stops it and, when
    // there's a hit to fill, the instances (of theirs) do. Whether something stopped it (a way of nothing: nothing, the way
    // and the hit emptied)
    u32 LineOfSight(ChunkData* chunk, const Vector4* from, Vector4* way, u32 mask, InstanceRayHit* hit, u32 instanceMask)
        RETAIL(FUN_002811b0);
    // Whether a ray gets into a node's box (either end inside it, or crossing one of its faces)
    u32 RayCrossesBox(const CollisionNode* node, const Vector4* start, const Vector4* end) RETAIL(FUN_001f97e8);
    // A ray through the collision, precise (when the triangle hit is wanted) or fast. Whether it hit, the distance and the point
    // (its w 1 by the fast one) when wanted
    u32 CheckCollision(CollisionData* data, const Vector4* start, const Vector4* end, u32 mask, f32* distance, Vector4* position,
                       CollisionHit* triangle);
    // The same through a chunk's collision (none without a chunk or a collision)
    u32 GetCollisionCheck(ChunkData* chunk, const Vector4* start, const Vector4* end, u32 mask, f32* distance, Vector4* position,
                          CollisionHit* triangle);
}
