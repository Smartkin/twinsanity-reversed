#pragma once

#include "common.h"
#include "game/math.h"

struct CollisionHull;
struct CollisionSurface;
struct GameOGI;
struct InstanceContext;
struct ReferencedObject;

// An object's collision (0x90 bytes, 0x10 into the object): the object, a box hull of its own or a hull it's given, the scenery
// cell it's sorted into (its link, the node, the cell's index: -1 none), an object its queries leave out, a matrix its hulls are
// placed with instead of its place's, its bits, its box in the world and its own, the OGI whose hulls it has with the matrices of
// those hulls' joints (kept for the OGI they were made for, how many: -1 once destroyed) and every hull's surface. Its hulls are
// its own or given one, else its OGI's (placed at their joints), else those of its kind 4 node
struct ObjectCollision
{
    enum Bits : u64
    {
        // Its OGI's hulls' joints' matrices are kept (for when its model has no animator), a side of its own box is under 0.05
        BitKeepsJointMatrices = 0x2,
        BitThin = 0x4,
    };

    ReferencedObject* owner;
    CollisionHull* hull;
    CollisionHull* givenHull;
    void* cellLink;
    void* cellNode;
    s32 cell;
    ReferencedObject* leftOut;
    Matrix4x4* hullMatrix;
    u64 bits;
    u8 unknown28[8];
    Box box;
    Box ownBox;
    GameOGI* ogi;
    Matrix4x4* jointMatrices;
    s32 jointMatrixCount;
    GameOGI* jointMatricesOgi;
    u16* surfaces;
    u8 unknown84[0xC];
};
CHECK_OFFSET(ObjectCollision, bits, 0x20);
CHECK_OFFSET(ObjectCollision, box, 0x30);
CHECK_OFFSET(ObjectCollision, ogi, 0x70);
CHECK_OFFSET(ObjectCollision, surfaces, 0x80);
CHECK_SIZE(ObjectCollision, 0x90);

extern "C"
{
    // Made for its object (no hulls, no cell, its bits 0 and 2 clear and 1 set) and destroyed (its hull, matrices and surfaces
    // freed)
    ObjectCollision* ConstructObjectCollision(ObjectCollision* collision, ReferencedObject* owner) RETAIL(InitCollisionInformation_);
    void DestroyObjectCollision(ObjectCollision* collision, u32 destroyFlags) RETAIL(FUN_001f0870);
    // How many hulls it has (one of its own or given, its OGI's, its kind 4 node's, none), a hull, a hull's own surface (0xFFFF
    // none), the surface it has for a hull (the game's surfaces', none without hulls), its surfaces made (for so many hulls) and
    // set from the hulls' own, all set to one, a hull's own set (none for a hull it doesn't have), and the one it has for a hull
    // (0 past its hulls)
    s32 GetHullCount(ObjectCollision* collision) RETAIL(GetHullCount);
    CollisionHull* GetCollisionModel(ObjectCollision* collision, u32 index) RETAIL(FUN_001f0d30);
    u16 HullOwnSurface(ObjectCollision* collision, u32 index) RETAIL(GetHullSurface);
    CollisionSurface* GetHullSurface(ObjectCollision* collision, u32 index) RETAIL(FUN_001ebb40);
    void AllocateHullSurfaces(ObjectCollision* collision, s32 count) RETAIL(FUN_001f0e78);
    void CopyHullSurfaces(ObjectCollision* collision) RETAIL(FUN_001f07a8);
    void SetAllHullSurfaces(ObjectCollision* collision, u16 surface) RETAIL(FUN_001f0dc8);
    void SetHullOwnSurface(ObjectCollision* collision, u8 index, u16 surface) RETAIL(FUN_001f0ec0);
    u16 HullSurfaceIndex(ObjectCollision* collision, u8 index) RETAIL(FUN_001f0f68);
    // A hull and the matrix it's placed with under a matrix (an OGI's at its joint, the identity's in the model's space), the same
    // under its object's matrix, and the box of all of them (its own box moved when it has none)
    void GetHullAndMatrix(ObjectCollision* collision, const Matrix4x4* matrix, s32 index, CollisionHull** hull, Matrix4x4* out)
        RETAIL(GetHullAndMatrix);
    void GetInstanceHull(ObjectCollision* collision, s32 index, CollisionHull** hull, Matrix4x4* matrix) RETAIL(FUN_001f0cc8);
    void GetInstanceHullBounds(ObjectCollision* collision, const Matrix4x4* matrix, Box* box) RETAIL(GetInstanceHullBounds);
    // A joint's matrix for a hull: its model's animator's (kept for the hull when its bits say so), else the one kept (the
    // identity without one, or when it was kept for another OGI); and the kept ones made again for its OGI (the identity's), with
    // its surfaces
    void HullJointMatrix(ObjectCollision* collision, u32 joint, s32 hull, Matrix4x4* out) RETAIL(CopyMatrixAtIndex);
    void MakeJointMatrices(ObjectCollision* collision) RETAIL(FUN_001eb468);
    // Its boxes (its own as well when asked) and its box hull when asked: from corners, from half sizes around its place; a
    // matrix of its own its hulls are placed with (its box worked out with it); an OGI's hulls (its own box the OGI's, the
    // object queued), and its own box's thinness marked and its surfaces made from its hulls
    void SetCollisionBox(ObjectCollision* collision, const Vector4* min, const Vector4* max, u32 makeHull, u32 ownBoxToo)
        RETAIL(FUN_001f08f8);
    void SetCentredCollisionBox(ObjectCollision* collision, const Vector4* extent, u32 makeHull, u32 ownBoxToo)
        RETAIL(FUN_001f09a8);
    void SetCollisionMatrix(ObjectCollision* collision, const Matrix4x4* matrix) RETAIL(FUN_001f0a78);
    void SetCollisionOgi(ObjectCollision* collision, GameOGI* ogi) RETAIL(FUN_001eb400);
    void MarkThinAndCopySurfaces(ObjectCollision* collision) RETAIL(FUN_001eb6a0);
    // A frame: its joints' matrices made again for a new OGI, its box worked out from its place (without a matrix of its own),
    // and its cell found (the chunk told of it when it has no cell; still asm the cell's part)
    void* StepObjectCollision(ObjectCollision* collision) RETAIL(FUN_001f0af8);
    void* UpdateCollisionCell(ObjectCollision* collision, InstanceContext* owner) RETAIL(FUN_001eb250);
}
