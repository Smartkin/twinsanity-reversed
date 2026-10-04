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
// bit 1 queued to be stepped, bit 2 released), its place (0x70 bytes of its own), its collision (0x10 bytes in),
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
        // It's in a scenery cell drawn this frame
        FlagInDrawnCell = 0x200,
        FlagVisible = 0x400,
        FlagShadow = 0x1000,
        // A projectile's
        FlagProjectile = 0x20000,
        // The character's solver keeps the body out of its kind 5 node's sphere while that node is on, out of its hulls otherwise
        FlagSphereContact = 0x10,
        // It has a physics body whose collisions its object node hears of
        FlagPhysicsBody = 0x8000,
        // Its model's hulls collide (ModelNode::SetSolid)
        FlagSolidModel = 0x80000,
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
    // The handle's reference added at the end (the handle's the callee's, which lets it go; a full array grown by its growth).
    // Returns its index
    u32 ReferenceArrayAppend(ReferenceArray* array, Reference** handle) RETAIL(FUN_0019a840);
    // The objects queued to be stepped
    extern ReferenceArray g_QueuedObjects RETAIL(G_InstCxtRefCountersTable_);
    extern const GccVTableEntry g_ReferencedObjectVTable[] RETAIL(InstanceContextUnkParent_methods);
    // The object queued to be stepped (once until it's stepped)
    void QueueObject(ReferencedObject* object) RETAIL(CreateContextRef);
}

// Up to 32 references (0x84 bytes): their count and the handles, kept sorted by their objects' addresses when gathered
struct ReferenceSet
{
    u32 count;
    Reference* handles[32];

    // Made empty, a reference to an object taken when there's one
    static ReferenceSet* Construct(ReferenceSet* set, ReferencedObject* first) RETAIL(FUN_001f4bc0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001f5e58);
    // Another's references taken (the same reference blocks, counted once more)
    ReferenceSet* Assign(const ReferenceSet* other) RETAIL(FUN_001f5ed8);
    void Clear() RETAIL(FUN_001f5fa0);
    void Sort() RETAIL(FUN_001f5fa8);
};
CHECK_SIZE(ReferenceSet, 0x84);

extern "C"
{
    // The order of two handles' objects (none first)
    s32 CompareHandles(const Reference* const* first, const Reference* const* second) RETAIL(func_001F5E30);
}

// A walk over a reference set's handles (retail's D_002F2DA8 over its base D_002F2DE0, 0xC bytes, made on the stack): its vtable
// (1 the destructor, 2 back to the first handle, 3 whether it's done, 4 the handle it's at, 5 on to the next), the index it's at
// and the set
class HandleWalk
{
public:
    const GccVTableEntry* vtable;
    u32 index;
    const ReferenceSet* set;

    // The vtable's functions and the base's destructor
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013e468);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0013e438);
    void First() RETAIL(FUN_0013e498);
    u32 IsDone() RETAIL(FUN_0013e4a0);
    Reference* const* Current() RETAIL(FUN_0013e4b8);
    void Next() RETAIL(FUN_0013e4d0);

    // Through the vtable: whether it's done, the object of the handle it's at
    bool AtEnd()
    {
        return (CallVirtual<u32>(this, vtable, 3) & 0xFF) != 0;
    }

    ReferencedObject* Object()
    {
        Reference* const* handle = CallVirtual<Reference* const*>(this, vtable, 4);
        return *handle != nullptr ? (*handle)->object : nullptr;
    }

    // On to the next handle, as the trigger nodes' check has it inline
    void Step()
    {
        if (index < set->count)
        {
            index++;
        }
    }
};

extern "C"
{
    extern const GccVTableEntry g_HandleWalkVTable[] RETAIL(D_002F2DA8);
    extern const GccVTableEntry g_HandleWalkBaseVTable[] RETAIL(D_002F2DE0);
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
