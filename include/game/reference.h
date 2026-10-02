#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/objectcollision.h"

struct ReferencedObject;

// A reference to an object (an instance context, a chunk's data, a loader): the object, and a count of 24 bits with flags above
// it. Bit 24 makes the reference own the object: the last one destroys it. An object keeps the block of its references at 0xB0
struct Reference
{
    ReferencedObject* object;
    u32 value;
};
CHECK_SIZE(Reference, 8);

struct ObjectPlace;

// What references point at (the game's objects' base, its vtable at 0xA4: 1 the destructor, 2 woken and 3 put to sleep (whether it
// was asleep, and whether it wasn't), 4 released (whether it wasn't before), 5 its step once it was queued): its flags (bit 0 asleep,
// bit 1 queued to be stepped, bit 2 released), its place (0x70 bytes of its own), its collision (0x10 bytes in, still asm),
// the chunk it's in (its data), the reference block at 0xB0
struct ReferencedObject
{
    enum Flags : u32
    {
        FlagAsleep = 0x1,
        FlagQueued = 0x2,
        FlagReleased = 0x4,
        // Its agent's state flags (game/instances.h): triggers' signals reach it, it's drawn, it casts a shadow
        FlagTriggerSignals = 0x8,
        FlagVisible = 0x400,
        FlagShadow = 0x1000,
        // A projectile's
        FlagProjectile = 0x20000,
        // The character's solver keeps the body out of its kind 5 node's sphere while that node is on, out of its hulls otherwise
        FlagSphereContact = 0x10,
        // It has a physics body whose collisions its object node hears of
        FlagPhysicsBody = 0x8000,
    };

    u32 unknown00;
    u32 flags;
    ObjectPlace* place;
    u32 unknown0C;
    ObjectCollision collision;
    struct ChunkData* chunk;
    const GccVTableEntry* vtable;
    u8 unknownA8[0xB0 - 0xA8];
    Reference* reference;

    static ReferencedObject* Construct(ReferencedObject* object) RETAIL(FUN_001976a8);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_00197720);
    u32 Wake() RETAIL(FUN_00197780);
    u32 Sleep() RETAIL(FUN_00197798);
    u32 Release() RETAIL(FUN_001977b0);
    // The base's step: no longer queued
    void StepQueued() RETAIL(FUN_00197898);

    void Destroy(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, 1, destroyFlags);
    }

    // The box of its collision
    const Box* CollisionBox() const
    {
        return &collision.box;
    }
};
CHECK_OFFSET(ReferencedObject, chunk, 0xA0);
CHECK_OFFSET(ReferencedObject, reference, 0xB0);

namespace ReferenceBits
{
constexpr u32 CountMask = 0xFFFFFF;
constexpr u32 Owns = 0x1000000;
}

// An array of references (a growable array of handles): a full one grows by its growth
struct ReferenceArray
{
    Reference** data;
    u32 count;
    u32 capacity;
    u32 growth;
};

extern "C"
{
    // Lets the reference go: the handle is cleared once the object has no references left
    void RemoveReference(Reference** handle);
    // The handle's reference added at the end (the handle's the callee's, which lets it go). Returns its index (still asm)
    u32 ReferenceArrayAppend(ReferenceArray* array, Reference** handle) RETAIL(FUN_0019a840);
    // The objects queued to be stepped
    extern ReferenceArray g_QueuedObjects RETAIL(G_InstCxtRefCountersTable_);
    extern const GccVTableEntry g_ReferencedObjectVTable[] RETAIL(InstanceContextUnkParent_methods);
    // The object queued to be stepped (once until it's stepped)
    void QueueObject(ReferencedObject* object) RETAIL(CreateContextRef);
}

// The object's reference block, made the first time (its flags are what the memory held, but for bit 24 and the count), with
// one more reference
Reference* AddReference(ReferencedObject* object);

// A handle pointed at another object (none for nullptr): the old reference let go, a new one taken
inline void AssignReference(Reference** handle, ReferencedObject* object)
{
    ReferencedObject* current = *handle != nullptr ? (*handle)->object : nullptr;
    if (current == object)
    {
        return;
    }

    RemoveReference(handle);
    *handle = object != nullptr ? AddReference(object) : nullptr;
}
