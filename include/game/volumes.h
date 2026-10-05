#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/math.h"

class Stream;
struct SceneryCell;
struct SphereVolume;
struct CapsuleVolume;

// The volumes the game tests things against, their vtables the retail ones: D_002FC700 the base, D_002FC658 a box
// (BoundingVolume), D_002FC5C8 a sphere and D_002FC538 a capsule. Every one starts with the sphere around it (w its radius) and
// has its vtable 0x10 bytes in. The vtables' functions: 1 the destructor, 2 a copy (none of the base), 3 the box around it, 4
// where a point is, 5 where a sphere is, 6 where a segment is, 7 the share of a segment where it crosses the volume's surface, 8
// the same, 0 when the segment starts inside, 9 none, 10 where another volume is, 11 a copy moved by a matrix (none of a box), 12
// another volume of its type made this one moved by a matrix, 13 a contact with another volume, 14 its type, 15 read, 16
// nothing. Where something is: Volume::Placement
enum VolumeType : u32
{
    VolumeTypeBox = 0x1601,
    VolumeTypeSphere = 0x1604,
    VolumeTypeCapsule = 0x1606,
    // Tested by its sphere: the game has no volume of this type
    VolumeTypeBySphere = 0x1608,
};

// What a segment is tested as (slots 6 to 8): a line through its points, a ray from its start, or the segment
enum SegmentMode : s32
{
    SegmentLine = 0,
    SegmentRay = 1,
    SegmentBounded = 2,
};

// A contact with another volume (slot 13): the plane of a box's face nearest where it touched, or the way from the other sphere's
// centre to a sphere's (w 1)
struct alignas(16) VolumeContact
{
    enum Kind : u32
    {
        BoxFace = 1,
        BetweenSpheres = 2,
    };

    Vector4 normal;
    u32 kind;
};
CHECK_OFFSET(VolumeContact, kind, 0x10);

struct alignas(16) Volume
{
    // Where something is against a volume (or a cell's box against what's collected, VolumeHoldsCell): apart, wholly inside it,
    // partly; and another volume of a type it doesn't test
    enum Placement : s32
    {
        Apart = 0,
        Inside = 1,
        Partly = 2,
        Untested = -1,
    };

    Vector4 sphere;
    const GccVTableEntry* vtable;

    void Destroy(u32 flags) RETAIL(FUN_001ff1e8);
    Volume* Clone() RETAIL(FUN_001ff218);
    // The box around its sphere (w 1)
    void Bounds(Vector4* min, Vector4* max) RETAIL(FUN_002011b8);
    void Read(Stream* stream) RETAIL(FUN_00201228);
    void None16() RETAIL(FUN_001ff230);
};
CHECK_OFFSET(Volume, vtable, 0x10);
CHECK_SIZE(Volume, 0x20);

// A box along the axes (0x50 bytes, type 0x1601): its corners and its half size, the sphere's centre its middle. It moves by a
// matrix's position alone, and gives no share of a segment. Its vtable adds 17 where a sphere is against a box and 18 and 19, 0
struct alignas(16) BoundingVolume : Volume
{
    Vector4 min;
    Vector4 max;
    Vector4 halfSize;

    // The box (when both corners are given) made its middle, half size and sphere
    void SetBox(const Vector4* low, const Vector4* high) RETAIL(FUN_001f6ad8);

    void Destroy(u32 flags) RETAIL(FUN_001ff248);
    BoundingVolume* Clone() RETAIL(FUN_001ff2f8);
    void Bounds(Vector4* low, Vector4* high) RETAIL(FUN_001ff4c0);
    // Within its corners (faces included), and wholly inside when within its middle's half size (faces excluded)
    u32 TestPoint(const Vector4* point) RETAIL(FUN_001f6c00);
    u32 TestSphere(f32 radius, const Vector4* centre) RETAIL_N32(FUN_001ff288);
    // The mode doesn't matter: a segment is tested as one
    u32 TestSegment(const Vector4* segment, s32 mode) RETAIL(FUN_001ff610);
    u32 SegmentCrossing(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_001ff638);
    u32 SegmentFirstInside(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_001ff668);
    u32 None9() RETAIL(FUN_001ff5b8);
    s32 TestVolume(Volume* other) RETAIL(FUN_001f6da0);
    Volume* TransformedCopy(const Matrix4x4* matrix) RETAIL(FUN_001ff998);
    u32 TransformInto(const Matrix4x4* matrix, Volume* other) RETAIL(FUN_001ff9a0);
    u32 Contact(Volume* other, VolumeContact* contact) RETAIL(FUN_001ff2e0);
    u32 Type() RETAIL(FUN_001ff2e8);
    // The sphere, the corners and the half size
    void Read(Stream* stream) RETAIL(FUN_001ff908);
    // Where a sphere is against a box: apart when its centre is further than its radius out of it, wholly inside when its
    // centre is inside and every face is further than its radius
    u32 TestSphereInBox(f32 radius, const Vector4* centre, const Vector4* low, const Vector4* high) RETAIL_N32(FUN_001ff4d8);
    u32 None18() RETAIL(FUN_001ff6a8);
    u32 None19() RETAIL(FUN_001ff6b0);

    // The tests behind the slots (this unused by the first four): a segment against a box (the outcodes of its ends), a
    // segment's shares (none), a point's squared distance to a box's nearest face (when it's inside on every axis), a volume of a
    // box's type against this one (Inside when it's strictly inside), a sphere's, a capsule's (its segment's squared distance to
    // the box, or its ends' spheres when the segment touches it), and a volume of type 0x1608 moved (nothing)
    u32 SegmentInBox(const Vector4* low, const Vector4* high, const Vector4* segment) RETAIL(FUN_001f70f8);
    u32 SegmentCrossingBox(const Vector4* low, const Vector4* high, const Vector4* segment, s32 mode, f32* share)
        RETAIL(FUN_001ff698);
    u32 SegmentFirstInsideBox(const Vector4* low, const Vector4* high, const Vector4* segment, s32 mode, f32* share)
        RETAIL(FUN_001ff6a0);
    f32 FaceDistanceSquared(const Vector4* point, const Vector4* low, const Vector4* high) RETAIL(FUN_001f73d8);
    u32 TestBox(const BoundingVolume* other) RETAIL(FUN_001f6fc0);
    u32 TestSphereVolume(const SphereVolume* other) RETAIL(FUN_001ff5d0);
    u32 TestCapsule(const CapsuleVolume* other) RETAIL(FUN_001f6ed8);
    void TransformIntoBySphere(const Matrix4x4* matrix, Volume* other) RETAIL(FUN_001ffa80);
};
CHECK_OFFSET(BoundingVolume, min, 0x20);
CHECK_OFFSET(BoundingVolume, halfSize, 0x40);
CHECK_SIZE(BoundingVolume, 0x50);

// A sphere (0x30 bytes, type 0x1604): its radius squared
struct alignas(16) SphereVolume : Volume
{
    f32 radiusSquared;

    static SphereVolume* Construct(SphereVolume* volume, const Vector4* centre, f32 radius) RETAIL_N32(FUN_00200148);
    void Destroy(u32 flags) RETAIL(FUN_00200180);
    SphereVolume* Clone() RETAIL(FUN_001ff360);
    // On its surface (exactly) partly
    u32 TestPoint(const Vector4* point) RETAIL(FUN_002001b0);
    u32 TestSphere(f32 radius, const Vector4* centre) RETAIL_N32(FUN_00200220);
    // A segment too short to have a direction is its start
    u32 TestSegment(const Vector4* segment, s32 mode) RETAIL(FUN_001fa9d0);
    u32 SegmentCrossing(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_001fabc8);
    u32 SegmentFirstInside(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_002004b0);
    u32 None9() RETAIL(FUN_002002b8);
    s32 TestVolume(Volume* other) RETAIL(FUN_001fa770);
    SphereVolume* TransformedCopy(const Matrix4x4* matrix) RETAIL(FUN_002002c0);
    // Moved by the matrix's position alone: the centre isn't turned
    u32 TransformInto(const Matrix4x4* matrix, Volume* other) RETAIL(FUN_00200340);
    u32 Contact(Volume* other, VolumeContact* contact) RETAIL(FUN_001fa888);
    u32 Type() RETAIL(FUN_001ff358);

    // Another sphere against this one
    u32 TestSphereVolume(const SphereVolume* other) RETAIL(FUN_00200420);
};
CHECK_OFFSET(SphereVolume, radiusSquared, 0x20);
CHECK_SIZE(SphereVolume, 0x30);

// A capsule (0x50 bytes, type 0x1606): its radius and the radius squared, half its segment's length and the segment, its sphere
// round the segment's middle. It gives nothing of segments
struct alignas(16) CapsuleVolume : Volume
{
    f32 radius;
    f32 radiusSquared;
    f32 halfLength;
    Vector4 start;
    Vector4 end;

    // A capsule of a segment moved by a matrix
    static CapsuleVolume* Construct(CapsuleVolume* capsule, f32 boundingRadius, const Matrix4x4* matrix, const Vector4* start,
                                    const Vector4* end) RETAIL_N32(FUN_001f8390);
    static CapsuleVolume* ConstructCopy(CapsuleVolume* capsule, const CapsuleVolume* other) RETAIL(FUN_001ffa88);
    void Destroy(u32 flags) RETAIL(FUN_001ffae8);
    CapsuleVolume* Clone() RETAIL(FUN_001ff3c8);
    u32 TestPoint(const Vector4* point) RETAIL(FUN_001ffb18);
    u32 TestSphere(f32 sphereRadius, const Vector4* centre) RETAIL_N32(FUN_001ffbd0);
    u32 TestSegment(const Vector4* segment, s32 mode) RETAIL(FUN_001ffcb8);
    u32 SegmentCrossing(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_001ffcc0);
    u32 SegmentFirstInside(const Vector4* segment, s32 mode, f32* share) RETAIL(FUN_001ffcc8);
    u32 None9() RETAIL(FUN_001ffcd0);
    s32 TestVolume(Volume* other) RETAIL(FUN_001f8488);
    CapsuleVolume* TransformedCopy(const Matrix4x4* matrix) RETAIL(FUN_001ffcd8);
    u32 TransformInto(const Matrix4x4* matrix, Volume* other) RETAIL(FUN_001f85d0);
    // A box's plane where spheres stepped along the segment first touch it, or whether a sphere or another capsule touches it
    u32 Contact(Volume* other, VolumeContact* contact) RETAIL(FUN_001f8a90);
    u32 Type() RETAIL(FUN_001ff3c0);

    // Another capsule against this one (the less round one's sphere tested against the rounder one, then the segments' distance
    // and the other's ends), whether another touches it (the rounder one as a sphere of its radius against the other, then the
    // segments' distance), whether a sphere touches it, a sphere volume tested through slot 5, and a volume of a box's type
    // (always partly)
    u32 TestCapsule(const CapsuleVolume* other) RETAIL(FUN_001f8708);
    u32 TouchesCapsule(const CapsuleVolume* other) RETAIL(FUN_001f8910);
    u32 TouchesSphere(const Vector4* centre, f32 sphereRadius) const RETAIL_N32(FUN_001ffd58);
    u32 TestSphereVolume(const SphereVolume* other) RETAIL(FUN_001ffd28);
    u32 TestBox(const BoundingVolume* other) RETAIL(FUN_001ffe00);
};
CHECK_OFFSET(CapsuleVolume, radius, 0x20);
CHECK_OFFSET(CapsuleVolume, halfLength, 0x28);
CHECK_OFFSET(CapsuleVolume, start, 0x30);
CHECK_SIZE(CapsuleVolume, 0x50);

// The base of an iterator over a linked list (D_002FC500, its vtable first: 1 the destructor, 2 to 5 the derived one's first,
// done, current and next; D_002FC4B0 walks nodes whose next is 4 bytes in and whose item is 8 bytes in)
struct ListIterator
{
    const GccVTableEntry* vtable;
    void* list;
    void* node;

    void BaseDestroy(u32 flags) RETAIL(FUN_001ff418);
};
CHECK_SIZE(ListIterator, 0xC);

extern "C"
{
    extern const GccVTableEntry g_VolumeVTable[] RETAIL(D_002FC700);
    extern const GccVTableEntry g_BoxVolumeVTable[] RETAIL(D_002FC658);
    extern const GccVTableEntry g_SphereVolumeVTable[] RETAIL(D_002FC5C8);
    extern const GccVTableEntry g_CapsuleVolumeVTable[] RETAIL(D_002FC538);

    // The plane of the face of a box a point is nearest beyond (an edge's or a corner's normal when it's as far beyond several),
    // through the face
    void BoxFacePlane(const BoundingVolume* box, const Vector4* point, Vector4* plane) RETAIL(FUN_001f68d8);
    // Whether a box volume's box and a box overlap (faces included)
    u32 VolumeTouchesBox(BoundingVolume* volume, const Box* box) RETAIL(FUN_001f7458);

    // Squared distances to a box volume's box, Eberly's (Wild Magic's DistLin3Box3), the nearest point of the box returned in the
    // box's frame: a segment's (two points), a segment's in the box's frame, a point's in the box's frame and a line's through
    // a segment's points in the box's frame, with the share of the way where it's nearest
    f32 SegmentBoxDistanceSquared(const BoundingVolume* box, const Vector4* segment, f32* share, Vector4* nearest)
        RETAIL(FUN_001ff6d0);
    f32 LocalSegmentBoxDistanceSquared(const BoundingVolume* box, const Vector4* segment, f32* share, Vector4* nearest)
        RETAIL(FUN_001ff790);
    f32 PointBoxDistanceSquared(const BoundingVolume* box, const Vector4* point, Vector4* nearest) RETAIL(FUN_001ff868);
    f32 LineBoxDistanceSquared(const BoundingVolume* box, const Vector4* line, f32* share, Vector4* nearest) RETAIL(FUN_001f7510);
    // The line's cases, its direction made positive on every axis: none of its axes 0 (by the face it gets through, i0's), one of
    // them (i2), two (all but i0) and all three (the point alone). The point is moved to the nearest point of the box and the
    // squared distance added
    void LineThroughBoxFace(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction,
                            const Vector4* pointLessExtents, f32* share, f32* squared) RETAIL(FUN_001f7778);
    void LineBoxNoZeros(const BoundingVolume* box, Vector4* point, const Vector4* direction, f32* share, f32* squared)
        RETAIL(FUN_001f7d98);
    void LineBoxOneZero(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction, f32* share,
                        f32* squared) RETAIL(FUN_001f7ec8);
    void LineBoxTwoZeros(const BoundingVolume* box, s32 i0, s32 i1, s32 i2, Vector4* point, const Vector4* direction, f32* share,
                         f32* squared) RETAIL(FUN_001f8100);
    void LineBoxThreeZeros(const BoundingVolume* box, Vector4* point, f32* squared) RETAIL(FUN_001f8248);

    // Where a scenery cell's box is against what's collected against it (Volume::Placement, Inside when it's strictly inside the
    // cell). What it's given is read as a box (its min first, its max 16 bytes in), not as a volume's sphere and vtable
    u32 VolumeHoldsCell(SceneryCell* cell, BoundingVolume* volume) RETAIL(FUN_001fa640);
}

// What the C++ calls the box's functions by
inline void SetBoundingVolume(BoundingVolume* volume, const Vector4* min, const Vector4* max)
{
    volume->SetBox(min, max);
}

inline void ReadBoxVolume(BoundingVolume* volume, Stream* stream)
{
    volume->Read(stream);
}
