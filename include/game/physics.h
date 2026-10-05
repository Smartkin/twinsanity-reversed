#pragma once

#include "abi.h"
#include "common.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"

// A point moving at a velocity, laid out as a matrix's rows (the third the velocity, the fourth the position): the character's
// solver hands GroundAhead its place's matrix, its facing as the velocity
struct MovingPoint
{
    u8 unused00[0x20];
    Vector4 velocity;
    Vector4 position;
};

// A convex space: the planes it's inside of (n·p + w below a margin), how many and the planes (memory of their own)
struct PlaneSet
{
    s32 count;
    Vector4* planes;
};

// A contact's kind and the solver's marks on it: an instance's hull, a triangle, an instance's sphere (with the hull's bit); pushed
// out of, the hull the character rides (the slides' ends may be inside it), touched, stood on, ground, not to be pushed out of
// (refused a push when it would have been), marked, its push shared with its instance's body, left out of a probe (the probe
// started inside it). The marks the solver clears each time are all but the kinds' and the touches' (KeptBits)
union ContactKind
{
    // The masks of the bits tested together (what the solver's tests leave out) and of the kinds a contact is made with
    enum Mask : u32
    {
        Hull = 0x1,
        Triangle = 0x2,
        PushedOut = 0x4,
        Ridden = 0x8,
        Touched = 0x10,
        StoodOn = 0x20,
        Ground = 0x40,
        NoPush = 0x80,
        PushRefused = 0x100,
        SphereBit = 0x800,
        Sphere = Hull | SphereBit,
        ProbeStart = 0x1000,
        KeptBits = Hull | Triangle | Touched | StoodOn | Ground | SphereBit,
    };

    u32 value;
    struct
    {
        u32 hull : 1;
        u32 triangle : 1;
        u32 pushedOut : 1;
        u32 ridden : 1;
        u32 touched : 1;
        u32 stoodOn : 1;
        u32 ground : 1;
        u32 noPush : 1;
        u32 pushRefused : 1;
        u32 marked : 1;
        u32 pushShared : 1;
        u32 sphere : 1;
        u32 probeStart : 1;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(ContactKind, 4);

// A contact the character's solver works with (0x50 bytes): the space it keeps the body out of, the instance and hull it's of, its
// kind and marks, a point (the origin when the contacts start, moved by the pushes shared), the sphere of an instance (its centre
// and radius) and its share of the push
struct Contact
{
    PlaneSet space;
    struct InstanceContext* instance;
    s32 hullIndex;
    ContactKind kind;
    // The plane a point was last found in front of
    s32 outside;
    u8 unused18[0x20 - 0x18];
    Vector4 point;
    Vector4 sphere;
    // An instance's sphere's share of the push out of the body (the origin when there was none)
    Vector4 spherePush;
};
CHECK_SIZE(Contact, 0x50);

// The contacts of one kind of surface (32 at most) and the triangles they come from
struct ContactList
{
    static constexpr s32 MostContacts = 32;

    Contact contacts[MostContacts];
    CollisionHit triangles[MostContacts];
    s32 count;
};
CHECK_OFFSET(ContactList, triangles, 0xA00);
CHECK_OFFSET(ContactList, count, 0x1200);
CHECK_SIZE(ContactList, 0x1210);

// The character's contacts (D_003C4030, one set the solver starts and ends): the box they were gathered in, those of the surfaces
// solid to the player and the others
struct ContactSet
{
    Box box;
    ContactList solid;
    ContactList others;
};
CHECK_OFFSET(ContactSet, solid, 0x20);
CHECK_OFFSET(ContactSet, others, 0x1230);
CHECK_SIZE(ContactSet, 0x2440);

// The planes of a triangle and a hull kept for when they meet again (0x50 bytes): the space, the triangle's vertexes and the hull
struct HullPlaneCacheEntry
{
    PlaneSet space;
    u8 unused08[0x10 - 0x8];
    Vector4 triangle[3];
    const CollisionHull* hull;
};
CHECK_OFFSET(HullPlaneCacheEntry, hull, 0x40);
CHECK_SIZE(HullPlaneCacheEntry, 0x50);

// The last 64 triangles and hulls' planes, the next to be replaced
struct HullPlaneCache
{
    static constexpr s32 Entries = 64;

    HullPlaneCacheEntry entries[Entries];
    s32 next;
};
CHECK_OFFSET(HullPlaneCache, next, 0x1400);

// What Move and MoveCharacter return besides a ground contact's index (-1 none): stuck while falling; what ProbeAlong returns
// when the motion goes into no contact; where a segment is against a space (ClipSegmentToSpace)
constexpr s32 StuckFalling = -2;
constexpr s32 NoProbeHit = -2;
enum SegmentClip : s32
{
    SegmentOutside = 0,
    SegmentEnters = 1,
    SegmentStartsInside = 2,
};

extern "C"
{
    extern HullPlaneCache g_HullPlaneCache RETAIL(D_003C2420);
    extern ContactSet g_Contacts RETAIL(D_003C4030);
    // Set while the contacts are in use (never read)
    extern u8 g_ContactsInUse RETAIL(D_0030A211);

    // A space's planes; whether a point is inside it by a margin (the plane a point was last found in front of tried first, and
    // the one found kept)
    Vector4* PlaneSetPlanes(PlaneSet* set) RETAIL(FUN_002881c0);
    u32 PointInsidePlanes(f32 margin, const PlaneSet* set, const Vector4* point, s32* outside) RETAIL_N32(FUN_00288200);

    // The cache: entries without planes (a triangle at the origin), every entry made and the first next, whether an entry is of a
    // triangle (its vertexes the same to the bit) and hull, an entry made of a triangle, a hull and a space (a copy of its planes)
    HullPlaneCacheEntry* ConstructHullPlaneCacheEntry(HullPlaneCacheEntry* entry) RETAIL(FUN_00288368);
    HullPlaneCache* ConstructHullPlaneCache(HullPlaneCache* cache) RETAIL(FUN_002882e8);
    u32 HullPlaneCacheEntryMatches(const HullPlaneCacheEntry* entry, const CollisionHit* triangle, const CollisionHull* hull)
        RETAIL(FUN_002884e8);
    void* FillHullPlaneCacheEntry(HullPlaneCacheEntry* entry, const CollisionHit* triangle, const CollisionHull* hull,
                                  PlaneSet* space) RETAIL(FUN_00288430);

    // Whether a point is inside a solid contact without the bits by a margin; the least push out of a space (along the plane the
    // point is least deep behind; none when it's in front of one, 5e-5 counting): whether there was one (the last argument is never
    // read), the same through a wrapper
    u32 PointInsideSolidContact(f32 margin, ContactSet* set, const Vector4* point, u32 mask) RETAIL_N32(FUN_00284560);
    u32 PushOutOfSpace(const PlaneSet* space, const Vector4* point, Vector4* push, void* unused) RETAIL(FUN_00283d60);
    u32 PushOutOfSpaceAgain(const PlaneSet* space, const Vector4* point, Vector4* push) RETAIL(FUN_002881e0);
    // The solid contacts' marks: bits added to the contact so many after an instance's first hull contact (when it's of that
    // instance too), the first sphere contact below (its sphere's push's y negative) marked (its index, -1 none), the marks cleared
    void MarkInstanceContact(ContactSet* set, struct InstanceContext* instance, s32 after, u32 bits) RETAIL(FUN_002886e8);
    s32 MarkSphereContactBelow(ContactSet* set) RETAIL(FUN_00288778);
    // A body's box (from its place and an offset: a half width either way along x and z, from 0 up to a height) pushed out of the
    // instances' spheres: each sphere's push out of the box shared, half to the sphere (its contact's push and point) and half the
    // other way to the body (the push)
    void PushOutOfSpheres(f32 height, f32 halfWidth, ContactSet* set, const Vector4* position, const Vector4* offset, Vector4* push)
        RETAIL_N32(FUN_00285648);
    // The planes a motion from a place crosses (at most so many, none twice): of the solid contacts (not spheres, those pushed out
    // of or not to be) the end is inside of, those the start isn't behind and the end isn't in front of (1e-4 either way), with
    // their contacts (the last one too): how many
    s32 CollectCrossedPlanes(ContactSet* set, const Vector4* from, const Vector4* motion, const Vector4** planes, s32* contacts,
                             s32 most, s32* lastContact) RETAIL(FUN_00285958);
    // A contact a slide went along marked (stood on when the place a bit above the end is inside it, touched otherwise); an
    // instance's hull gets 20 times what the slide took off the motion
    void TouchContact(ContactSet* set, s32 index, const Vector4* motion, const Vector4* from, const Vector4* to) RETAIL(FUN_00285cb8);
    // A step of a motion slid along the planes it crosses: the end (the start when it can't move), whether it moved. Along one
    // plane, or else along the crease of two, the slide (nudged off the planes) that ends outside every solid contact nearest
    // (by the end's distance from the slide, as retail measures it); with the climb limit, slides steeper up than 45 degrees are
    // refused
    extern const s32 g_NoContacts[2] RETAIL(D_0030A218);
    u32 SlideStep(ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* to, u32 limitClimb) RETAIL(FUN_00285f88);
    // How many solid contacts without the bits a point is inside of by a margin
    s32 CountContactsContaining(f32 margin, ContactSet* set, const Vector4* point, u32 mask) RETAIL_N32(FUN_002846a8);
    // The ground a point is in: the first solid contact (not a sphere or not to be pushed out of) whose push out goes up more than
    // aside (marked ground), its push's direction when wanted; its index, -1 none
    s32 FindGround(ContactSet* set, const Vector4* point, Vector4* normal) RETAIL(FUN_002867f8);
    // Where a motion from a place first goes into a solid contact the place isn't in (halving the steps down to 1e-4): -2 none,
    // otherwise the last place outside (when wanted) and the contact whose plane facing it is the most upward of those 0.707 up
    // or more (-1 none), with that plane's normal when wanted; contacts with such a plane are marked ground
    s32 ProbeAlong(ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* lastOutside, Vector4* normal)
        RETAIL(FUN_00286a50);
    // A body's motion over a frame: slid a step at a time (as many as 0.05 / the frame's time takes, at most 6; stuck when a step
    // moves less than 0.001), then the ground found under the end, or probed for straight down and then around (as far as its
    // flat speed, 0.1 or a step), the end taken to 0.05 short of the probe's landing unless it rises; a sphere below makes ground
    // too. The end and the ground's normal: the ground's contact, -1 none, -2 stuck while falling
    s32 Move(f32 frameTime, ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* end, Vector4* groundNormal,
             u32 limitClimb) RETAIL_N32(FUN_002871c8);
    // The same, tried again with a small fall when stuck, then the ground just below taken (up, its contacts marked; 0 none)
    s32 MoveCharacter(f32 frameTime, ContactSet* set, const Vector4* from, const Vector4* motion, Vector4* end,
                      Vector4* groundNormal, u32 limitClimb) RETAIL_N32(FUN_00286f48);
    // A segment clipped to a space: 2 when its start is inside (on counts), 0 when both ends are in front of one plane, 1
    // otherwise with the start moved to where it goes in (and the plane it went in by)
    s32 ClipSegmentToSpace(const PlaneSet* space, const Vector4* start, const Vector4* end, Vector4* entry, Vector4* plane)
        RETAIL(FUN_00283b80);
    // The highest of the solid contacts (not those not to be pushed out of) a vertical segment from above a point to below it
    // goes into: how far below the point it is (1e30 none; 0 when a sphere is pushed down), where, its plane and its instance or
    // triangle when wanted
    f32 CastDown(f32 above, f32 below, ContactSet* set, const Vector4* point, Vector4* hit, Vector4* normal,
                 CollisionHit** triangle, struct InstanceContext** instance) RETAIL_N32(FUN_00287cc8);
    // A body stopped by what's ahead of it (slow, turned more than 30 degrees) stepped up it when allowed: the velocity taking it
    // there (the body's flat way, as fast as it lost, lifted by 3 times its flat speed up to 0.2) when the place is free and the
    // ground under it flat (0.866 up), lowered as far as it's free when the body isn't rising: whether it stepped up
    u32 StepUp(ContactSet* set, const Vector4* position, const Vector4* velocity, Vector4* outVelocity, u32 allowed)
        RETAIL(FUN_00287798);
    // Whether there's ground ahead of a moving point: inside a solid contact (not a sphere) 0.8 below where half of its velocity
    // or 1.6 times it takes it
    u32 GroundAhead(ContactSet* set, const MovingPoint* point) RETAIL(FUN_00287fd8);
    void ClearContactMarks(ContactSet* set) RETAIL(FUN_002887e0);

    // A place pushed out of the solid contacts it's inside (hull contacts, triangle contacts or both; not spheres): the push (each
    // contact's push out of its space from the place already pushed, added up) and how far, all told, when wanted; an instance's
    // hull contact whose instance has its kind 5 node off takes half of its push the other way (its body pushed, its point moved)
    void Depenetrate(ContactSet* set, const Vector4* position, Vector4* push, u32 hulls, u32 triangles, u32 pushBodies,
                     f32* pushed) RETAIL(FUN_00285220);

    // The contacts made (no spaces), emptied (the solid ones' points at the origin); the module's static constructor (the
    // character's contacts made) and its global constructor
    ContactSet* ConstructContactSet(ContactSet* set) RETAIL(FUN_00288608);
    void ClearContacts(ContactSet* set) RETAIL(FUN_002888b8);
    void InitPhysicsStatics(u32 initialise, u32 priority) RETAIL(FUN_00288158);
    void ConstructPhysicsModule() RETAIL(FUN_00288930);
    // The contacts started (none of either kind, the solid ones' points at the origin) and ended
    ContactSet* BeginContacts() RETAIL(FUN_00288818);
    void EndContacts() RETAIL(FUN_002888b0);
    // A triangle's contact with a hull (a box: 8 vertexes) added to the solid or the other contacts: its space from the cache, or
    // worked out and cached; the space a hull at the origin can't go into the triangle by (two planes per separating axis, the
    // extents of the triangle and the hull along it apart)
    void AddTriangleContact(ContactSet* set, const CollisionHit* triangle, const CollisionHull* hull, u32 solid) RETAIL(FUN_00284da8);
    u32 MakeTriangleHullSpace(PlaneSet* space, const CollisionHit* triangle, const CollisionHull* hull) RETAIL(FUN_00284278);
    // The triangles near a body's hull gathered into the contacts: in a box (whether there were more than fit), and around a place
    // the body moves from by a motion (its box grown by the motion and 0.3; the motion cut down a fifth at a time while too many
    // triangles are found, the last kept)
    extern CollisionHit g_GatheredTriangles[ContactList::MostContacts] RETAIL(D_003C3830);
    u32 GatherBoxTriangleContacts(ContactSet* set, ChunkData* chunk, const Box* box, const CollisionHull* hull) RETAIL(FUN_002847f0);
    u32 GatherTriangleContacts(ContactSet* set, ChunkData* chunk, const Vector4* position, Vector4* motion, const CollisionHull* hull)
        RETAIL(FUN_00284978);
    // The instances near a body's hull gathered into the solid contacts (their box queried with the bits, those listed skipped):
    // the sphere of those with one, their hulls otherwise; the hulls of an instance whose boxes overlap a box added to the solid
    // contacts or the others
    void GatherInstanceContacts(ContactSet* set, ChunkData* chunk, const Vector4* position, const Vector4* motion, const u32* mask,
                                struct InstanceContext** skipped, s32 skippedCount, const CollisionHull* hull) RETAIL(FUN_00284f40);
    void AddInstanceHullContacts(ContactSet* set, const Box* box, struct InstanceContext* instance, const CollisionHull* hull,
                                 u32 solid) RETAIL(FUN_00284c40);
    // The space one hull under a matrix can't go into another by (two planes per separating axis, the extents of every vertex's
    // difference along it apart), and the planes of the axes long enough added to a space (the differences in groups of four)
    u32 MakeHullHullSpace(PlaneSet* space, const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other,
                          const Matrix4x4* otherMatrix) RETAIL(FUN_002843a8);
    u32 AddAxisPlanes(PlaneSet* space, Vector4* planes, Vector4* differences, s32 count, const Vector4* axes, s32 axisCount)
        RETAIL(FUN_002840d8);
    // VU0's (src/platform/ps2/collisionmaths.cpp): every vertex of the first hull less every vertex of the second (both under
    // their matrices), points made groups of four (each coordinate's four together), and the range along an axis of so many
    // groups (of 16, 64 points)
    struct VertexDifferences
    {
        const Matrix4x4* matrix;
        const Vector4* vertices;
        u32 count;
        const Matrix4x4* otherMatrix;
        const Vector4* otherVertices;
        u32 otherCount;
        Vector4* differences;
        Vector4* moved;
    };
    void MakeVertexDifferences(const VertexDifferences* request) RETAIL(FUN_0029345c);
    void GroupPoints(Vector4* points, s32 groups) RETAIL(FUN_00293540);
    void GroupsRange(const Vector4* groups, s32 count, const Vector4* axis, f32* range) RETAIL(FUN_0029359c);
    void SixteenGroupsRange(const Vector4* groups, const Vector4* axis, f32* range) RETAIL(FUN_00293624);
    // Whether two hulls under their matrices go into each other (no separating axis between their vertexes' differences): the
    // push along the axis they overlap the least on and where they touch (the support point of the hull whose face that axis
    // is, the other one moved by the push, or between two edges)
    u32 HullsContact(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix,
                     Vector4* push, Vector4* point) RETAIL(FUN_00282fd0);
    // Whether a triangle goes into a hull under a matrix (never when the hull is on one side of its plane): the same, the point
    // the triangle's corner furthest along the way out when the axis is a face of the hull's
    u32 TriangleHullContact(const CollisionHit* triangle, const CollisionHull* hull, const Matrix4x4* matrix, Vector4* push,
                            Vector4* point) RETAIL(FUN_00283470);
    // The point of a segment nearest a line (half way when they're parallel)
    void NearestPointOfSegment(const Vector4* lineStart, const Vector4* lineEnd, const Vector4* start, const Vector4* end,
                               Vector4* out) RETAIL(FUN_00283e98);
    // The instances of a chunk whose boxes overlap a box with the bits (how many)
    s32 QueryChunkInstances(ChunkData* chunk, const Box* box, u32 mask, InstanceQuery* query) RETAIL(FUN_001ed750);
    // VU0's (src/platform/ps2/collisionmaths.cpp): a triangle's vertexes and a box hull's 8 loaded, and the range along an axis
    // of the triangle less the hull (its min and max)
    void LoadTriangleHullSupport(const CollisionHit* triangle, const Vector4* hullVertices) RETAIL(FUN_002936d8);
    void TriangleHullSupportRange(const Vector4* axis, f32* range) RETAIL(FUN_002938bc);

    // The separating axes of two hulls with their places (the first's face normals, W 0, the second's, W 1, then their edge
    // directions' cross products long enough, W 2; the Ws are integers) and of a triangle and a hull (the triangle's normal
    // first, a place for the hull or none): how many
    s32 HullHullAxes(const CollisionHull* hull, const Matrix4x4* matrix, const CollisionHull* other, const Matrix4x4* otherMatrix,
                     Vector4* axes) RETAIL(FUN_002829a0);
    s32 TriangleHullAxes(const CollisionHit* triangle, const CollisionHull* hull, const Matrix4x4* matrix, Vector4* axes)
        RETAIL(FUN_00282c30);
}
