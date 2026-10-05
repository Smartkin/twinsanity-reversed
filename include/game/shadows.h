#pragma once

#include "common.h"
#include "game/instances.h"
#include "game/math.h"

struct ChunkData;
struct RigidModel;

// The characters' shadows (TT Lab's notes: verified in the PAL executable): an instance's shadow node casts its slot's shapes
// from a point above it into its chunk's list, which the chunk's draw turns into volumes over the ground beneath

// The shapes shadows are drawn with (the default meshes' kinds, g_ShadowMeshes), named after the development tools' files
enum ShadowShape : u8
{
    ShadowCylinder = 0,
    ShadowCube = 1,
    ShadowRoundedCube = 2,
    ShadowOctagon = 3,
    ShadowTaperedCylinder = 4,
    ShadowTaperedCube = 5,
    ShadowTaperedRoundedCube = 6,
    ShadowTaperedOctagon = 7,
    ShadowShapeCount = 8,
};

// A circle (a disc under a joint: its radius and how tall the volume is) and a capsule (between two joints, of a radius), each
// drawn with a ShadowShape at an offset from its joint; a plain shape (a rectangle of a width and a depth) at the instance
struct ShadowCircle
{
    Vector4 offset;
    u8 kind;
    u8 joint;
    u8 unused12[2];
    f32 radius;
    f32 height;
    ShadowCircle* next;

    // Made at the default box's corner (w 1) for a kind and a joint, none after it
    static ShadowCircle* Construct(f32 radius, f32 height, ShadowCircle* circle, u8 kind, u8 joint) RETAIL_N32(FUN_001ccbb8);
};
CHECK_SIZE(ShadowCircle, 0x20);

struct ShadowCapsule
{
    Vector4 offset;
    u8 kind;
    u8 joint;
    u8 secondJoint;
    u8 unused13;
    f32 radius;
    ShadowCapsule* next;

    static ShadowCapsule* Construct(f32 radius, ShadowCapsule* capsule, u8 kind, u8 joint, u8 secondJoint)
        RETAIL_N32(FUN_001cc018);
};
CHECK_OFFSET(ShadowCapsule, next, 0x18);

struct ShadowPlain
{
    Vector4 offset;
    u8 kind;
    u8 unused11[3];
    f32 width;
    f32 depth;

    static ShadowPlain* Construct(f32 width, f32 depth, ShadowPlain* plain, u8 kind) RETAIL_N32(FUN_001cc158);
    static ShadowPlain* ConstructSquare(f32 size, ShadowPlain* plain, u8 kind) RETAIL_N32(FUN_001cc0f0);
};
CHECK_OFFSET(ShadowPlain, depth, 0x18);

// How far a shadow's shapes reach: the distance (whole units) and its square
union ShadowReach
{
    u32 value;
    struct
    {
        u32 distance : 10;
        u32 squared : 20;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(ShadowReach, 4);

// What a shadow casts up to a distance: the joints its shapes follow (a bit each), the distance, its strength, its plain shape,
// circles and capsules, and the shapes cast beyond that distance
struct ShadowShapes
{
    u64 joints;
    ShadowReach reach;
    f32 strength;
    ShadowPlain* plain;
    ShadowCircle* circles;
    ShadowCapsule* capsules;
    ShadowShapes* next;

    // Made for a distance and a strength (with a plain shape); its shapes deleted and the shapes after it
    static ShadowShapes* Construct(f32 distance, f32 strength, ShadowShapes* shapes) RETAIL_N32(FUN_001cbde0);
    static ShadowShapes* ConstructPlain(f32 distance, f32 strength, ShadowShapes* shapes, ShadowPlain* plain)
        RETAIL_N32(FUN_001cbe80);
    void Clear() RETAIL(FUN_001cbf90);
    // A plain shape set, a circle and a capsule put in front of their lists (their joints' bits set)
    void SetPlain(ShadowPlain* plain) RETAIL(FUN_001cbf28);
    void AddCircle(ShadowCircle* circle) RETAIL(FUN_001cbf30);
    void AddCapsule(ShadowCapsule* capsule) RETAIL(FUN_001cbf58);
};
CHECK_SIZE(ShadowShapes, 0x20);

// A shadow of a slot: its strength at the start, how far it reaches (squared) and its shapes
struct ShadowSlot
{
    f32 strength;
    u32 reachSquared;
    ShadowShapes* shapes;

    static ShadowSlot* Construct(f32 strength, ShadowSlot* slot, ShadowShapes* shapes) RETAIL_N32(FUN_001cc2c8);
};
CHECK_SIZE(ShadowSlot, 0xC);

// A shadow node's bits: the slot it casts
union ShadowNodeBits
{
    u32 value;
    struct
    {
        u32 slot : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(ShadowNodeBits, 4);

// An instance's shadow node (kind 10, class 0x1428): the slot it casts and its four slots
struct ShadowNode : GameNode
{
    static constexpr u32 ClassId = 0x1428;
    static constexpr u32 Slots = 4;

    ShadowNodeBits bits;
    ShadowSlot* slots[Slots];

    static ShadowNode* Construct(ShadowNode* node) RETAIL(InitUnkNode);
    // Its vtable's slots: 2 the destructor (its slots deleted), 5 its kind, 8 its update (its slot's shapes cast while its
    // instance has its shadow, is seen and is in a drawn cell, as strongly as how far its instance's stamp is into the shapes'
    // distances), 10 its class
    void Destroy(u32 destroyFlags) RETAIL(FUN_001cc590);
    u32 Kind() RETAIL(GetNodeIndex_001CC518);
    u32 Update(TimeClock* clock) RETAIL(FUN_001caba0);
    u32 GetClassId() RETAIL(FUN_001cc520);
    // A slot deleted (made empty), and given a shadow (the one it had deleted)
    void ClearSlot(u32 slot) RETAIL(FUN_001cc640);
    void SetSlot(u32 slot, ShadowSlot* shadow) RETAIL(FUN_001cc6c0);
};
CHECK_SIZE(ShadowNode, 0x2C);

// A shadow cast this frame (0x60 bytes): its frame (its z axis along the capsule or up), its shape's offset and kind, and its size
// (x and z across, y the way to the point it's cast from, then to the ground under it)
struct ShadowEntry
{
    Matrix4x4 frame;
    f32 offset[3];
    u8 kind;
    u8 unused4D[3];
    f32 size[3];
    u32 unused5C;
};
CHECK_SIZE(ShadowEntry, 0x60);

// A chunk's shadows (made with the chunk): two lists of 100 taking turns, each frame's cast into one while the other is drawn
struct ChunkShadows
{
    ShadowEntry* first;
    ShadowEntry* second;
    u32 firstCount;
    u32 secondCount;
    ShadowEntry* current;
    u32* currentCount;
    u32 capacity;

    static ChunkShadows* Construct(ChunkShadows* shadows, s32 capacity) RETAIL(FUN_001cc310);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001cc3f8);
    // The other list made the one cast into (emptied)
    void Swap() RETAIL(FUN_001cc4e0);
    // A shadow added (none when the list is full): its kind, offset and size
    ShadowEntry* Add(u8 kind, const f32* offset, const f32* size) RETAIL(FUN_001cc460);
    // The shadows cast drawn: each frame turned to stand up, cast down onto the chunk's collision (none drawn when it misses), and
    // the lists swapped
    void Draw(const Vector4* planes, const Matrix4x4* unused, const Matrix4x4* view) RETAIL(FUN_001ca788);
};
CHECK_SIZE(ChunkShadows, 0x1C);

extern "C"
{
    // An entry made (its kind byte's word cleared)
    ShadowEntry* ConstructShadowEntry(ShadowEntry* entry) RETAIL(FUN_001cc748);
    // An instance's shapes cast from above it (the lighting constants' direction times the strength) into its chunk's shadows:
    // its circles and capsules from its joints' matrices (its place's where its model has none), its plain shape from its place
    void CastShadow(f32 strength, ShadowShapes* shapes, InstanceContext* instance) RETAIL_N32(FUN_001c9130);
    void CastCircleShadows(ShadowCircle* circle, const Vector4* from, const Matrix4x4* joints, ChunkShadows* shadows)
        RETAIL(FUN_001cb158);
    void CastCapsuleShadows(ShadowCapsule* capsule, const Vector4* from, const Matrix4x4* joints, ChunkShadows* shadows)
        RETAIL(FUN_001c9328);
    void CastPlainShadow(ShadowPlain* plain, const Vector4* from, const Matrix4x4* place, ChunkShadows* shadows)
        RETAIL(FUN_001cc1c0);
    // An entry drawn: the default mesh of its kind scaled to its size at its offset, through the matrices to the shadows' screen,
    // to the camera and to the world
    void DrawShadowEntry(const ShadowEntry* entry, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
        RETAIL(FUN_001cb048);
    // The shape a development tools keyword names (its token's value, 0xBB to 0xBE and 0x10D to 0x110), -1 for another
    s32 ShadowShapeOfToken(u32 keyword) RETAIL(FUN_001ccaf0);
    // The lighting constants' static constructor
    void LightingConstantsStaticInit() RETAIL(FUN_001cccf0);
    // The shadows made ready with the default chunk: the pass's set-up and the default meshes they're drawn with (the meshes read
    // from the development tools' files when asked)
    void InitShadows(u32 fromFiles) RETAIL(FUN_001c9668);
    void LoadShadowMeshes(u32 fromFiles) RETAIL(FUN_001cad10);
    // The default meshes the shadows are drawn with, by kind
    extern RigidModel* g_ShadowMeshes[ShadowShapeCount] RETAIL(G_DefaultMeshIDs);
    // A segment clipped by six planes (each end outside one pulled back along the segment by its distance from it): whether
    // any of it is left, and the clipped segment
    u32 ClipSegmentToPlanes(const Vector4* planes, const Vector4* segment, Vector4* clipped) RETAIL(FUN_001fdf90);
    // The shadows' screen and camera matrices
    extern Matrix4x4 g_ShadowToCamera RETAIL(D_003B0770);
    extern Matrix4x4 g_ShadowToScreen RETAIL(D_003B07B0);
}
