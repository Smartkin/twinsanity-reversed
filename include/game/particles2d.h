#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/math.h"

class Shape2D;

// A pool of 16 byte items in slots (its vtable the retail one, after 0x10 bytes: 1 the destructor). A slot's link is the next free
// slot while it's free (-2 ends the free list) and -1 while it's used. When it's full it grows by its growth: an odd capacity and
// an odd growth are made even first, the used items are copied over and every old slot is marked used
class SlotPool
{
public:
    s16 capacity;
    s16 growth;
    s16 count;
    s16 firstFree;
    s16* links;
    Vector4* items;
    const GccVTableEntry* vtable;

    static SlotPool* Construct(SlotPool* pool) RETAIL(FUN_001ad270);
    void Destroy(u32 flags) RETAIL(FUN_001ad2a8);
    // A free slot made used (the pool grown when it's full)
    s16 Allocate() RETAIL(FUN_001ad008);
    void Free(s16 slot) RETAIL(FUN_001ad080);
    // Grown to the capacity when it's smaller
    void Reserve(u32 size) RETAIL(FUN_001ad318);
    // Every slot freed (when any is used), the free list in their order again
    void Clear() RETAIL(FUN_0017c418);
    void Grow() RETAIL(FUN_001ab268);
};
CHECK_SIZE(SlotPool, 0x14);

// The retail walk over a pool's used slots (its iterator classes D_002F6E18 and D_002F6D90, the same template's code): the slot it's
// at and how many used slots it has passed. Freeing the slot it's at and counting one less passed keeps it going
class SlotIterator
{
public:
    const GccVTableEntry* vtable;
    s16 slot;
    s16 passed;
    SlotPool* pool;

    void Destroy(u32 flags) RETAIL(FUN_001abd98);
    void BaseDestroy(u32 flags) RETAIL(FUN_001abdc8);
    void First() RETAIL(func_001ABDF8);
    u32 IsDone() RETAIL(FUN_001ad1a8);
    Vector4* Current() RETAIL(FUN_001abe70);
    void Next() RETAIL(FUN_001abe88);
    SlotIterator* Assign(const SlotIterator* other) RETAIL(func_001AD1C0);
    s32 Slot() RETAIL(FUN_001ad1e0);
    s32 Count() RETAIL(FUN_001ad1e8);

    // The other instance's
    void OtherDestroy(u32 flags) RETAIL(FUN_001abfa0);
    void OtherBaseDestroy(u32 flags) RETAIL(FUN_001abfd0);
    void OtherFirst() RETAIL(func_001AC000);
    u32 OtherIsDone() RETAIL(FUN_001ad158);
    Vector4* OtherCurrent() RETAIL(FUN_001ac0f8);
    void OtherNext() RETAIL(func_001AC078);
    SlotIterator* OtherAssign(const SlotIterator* other) RETAIL(func_001AD170);
    s32 OtherSlot() RETAIL(FUN_001ad190);
    s32 OtherCount() RETAIL(FUN_001ad198);
};
CHECK_SIZE(SlotIterator, 0xC);

// Values spread evenly over [0, 1] like the curves of the maths library's
struct FloatCurve
{
    u32 count;
    f32* values;
};

// The UI's 2D particles (their vtables the retail ones, after 0x48 bytes: 1 a particle made between two places, 2 the particles
// aged by the seconds (the ones past their lifetime freed), 3 the destructor, 4 drawn placed by a matrix). A particle is a place
// and its age (x, y, z). Each step tries to make one a number of times, each try making one by its chance when there's room: a
// spot within the spread of the places' middle (fractions of the half distance between them each way) moved by the offset. They're
// drawn as the sprite at their size (its x widened by the TV's shape), turned and coloured by curves of their age's fraction of the
// lifetime
class Emitter2D
{
public:
    enum Slot : u32
    {
        SpawnSlot = 1,
        StepSlot = 2,
        DestroySlot = 3,
        DrawPlacedSlot = 4,
    };

    SlotPool particles;
    Vector2 size;
    Vector2 spread;
    Vector2 offset;
    Shape2D* sprite;
    f32 lifetime;
    f32 chance;
    u32 tries;
    Vector4Curve* colours;
    Vector2Curve* sizes;
    // Radians
    FloatCurve* turns;
    const GccVTableEntry* vtable;

    static Emitter2D* Construct(Emitter2D* emitter) RETAIL(FUN_001abcc8);
    void Destroy(u32 flags) RETAIL(FUN_001abd20);
    // The sprite, and room for as many particles
    void SetSprite(Shape2D* newSprite, u32 room) RETAIL(FUN_001abd78);
    void Spawn(const Vector2* start, const Vector2* end) RETAIL(FUN_001a7418);
    void Step(f32 seconds) RETAIL_N32(FUN_001a7528);
    void Draw(const Matrix4x4* placed) RETAIL(FUN_001a76f0);
};
CHECK_SIZE(Emitter2D, 0x4C);

// Particles thrown out from the middle in every direction: made on a ring as big as the spread's half distance (its length) in a
// random direction, which they fly in at a quarter of a unit a second, drawn in one colour (its curves of size and turn needed)
class RadialEmitter2D : public Emitter2D
{
public:
    u32 unused4C;
    Vector4 colour;

    void Destroy(u32 flags) RETAIL(FUN_001ac268);
    void Spawn(const Vector2* start, const Vector2* end) RETAIL(FUN_001a7a88);
    void Step(f32 seconds) RETAIL_N32(FUN_001a7c20);
    void Draw(const Matrix4x4* placed) RETAIL(FUN_001a7e40);
};
CHECK_SIZE(RadialEmitter2D, 0x60);

extern "C"
{
    extern const GccVTableEntry g_SlotPoolVTable[] RETAIL(D_002F6FB0);
    extern const GccVTableEntry g_Emitter2DVTable[] RETAIL(D_002F6EA0);
    extern const GccVTableEntry g_SlotIteratorBaseVTable[] RETAIL(D_002F6E68);
    extern const GccVTableEntry g_OtherSlotIteratorBaseVTable[] RETAIL(D_002F6DE0);

    // The emitter stepped by the seconds, then its tries at making a particle between the places; returns how many it has
    s32 Emitter2DStep(Emitter2D* emitter, f32 seconds, const Vector2* start, const Vector2* end) RETAIL_N32(FUN_001abf08);
}
