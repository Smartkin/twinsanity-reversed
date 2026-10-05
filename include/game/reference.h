#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/objectcollision.h"

struct ReferencedObject;

union ReferenceBits
{
    u32 value;
    struct
    {
        u32 count : 24;
        // The reference owns the object: letting the last one go destroys it
        u32 owns : 1;
        // What the memory held when the block was made
        u32 unused25 : 7;
    };
};
CHECK_SIZE(ReferenceBits, 4);

// A reference to an object (an instance context, a chunk's data, a loader): the object, and a count with flags above it. An
// object keeps the block of its references at 0xB0
struct Reference
{
    ReferencedObject* object;
    ReferenceBits bits;
};
CHECK_SIZE(Reference, 8);

struct ObjectPlace;

// A referenced object's flags. An instance's bits 3, 4, 10 and 12 follow its agent's state (game/properties.h's InstanceState)
union ReferencedObjectFlags
{
    u32 value;
    struct
    {
        u32 asleep : 1;
        // Queued to be stepped (QueueObject), until its step
        u32 queued : 1;
        u32 released : 1;
        // Triggers' signals reach it
        u32 receivesTriggerSignals : 1;
        // The collision queries that want it find it: the character's solver keeps out of its kind 5 node's sphere while that
        // node is on, of its hulls otherwise
        u32 collisionActive : 1;
        u32 unused5 : 1;
        // Attached to an agent that holds it, and holding an attached object (the scripts' AttachedToAnAgent and
        // GotAttachedObject)
        u32 attached : 1;
        u32 hasAttachment : 1;
        // Taken by a request or a linked object search (the scripts' IsBusy)
        u32 busy : 1;
        // It's in a scenery cell drawn this frame
        u32 inDrawnCell : 1;
        u32 visible : 1;
        u32 unused11 : 1;
        u32 shadowActive : 1;
        // Set on the instances of layouts that aren't their chunk's own (InstanceFactory::NotChunkOwn)
        u32 unused13 : 1;
        // What stands on it rides along
        u32 carriesRiders : 1;
        // It has a physics body whose collisions its object node hears of
        u32 physicsBody : 1;
        u32 unused16 : 1;
        // Its chunk follows where it is (UpdateInstanceChunk: projectiles, thrown characters), and the follow node doesn't move
        // such a camera through links
        u32 movesBetweenChunks : 1;
        // A dynamic scenery's (the scenery cells keep such instances on a list of their own)
        u32 dynamicScenery : 1;
        // Its model's hulls collide (ModelNode::SetSolid)
        u32 solidModel : 1;
        u32 unused20 : 12;
    };

    // The bits' masks: the collision queries' wanted and unwanted flags
    enum Mask : u32
    {
        Asleep = 0x1,
        Queued = 0x2,
        Released = 0x4,
        ReceivesTriggerSignals = 0x8,
        CollisionActive = 0x10,
        Attached = 0x40,
        HasAttachment = 0x80,
        Busy = 0x100,
        InDrawnCell = 0x200,
        Visible = 0x400,
        ShadowActive = 0x1000,
        CarriesRiders = 0x4000,
        PhysicsBody = 0x8000,
        MovesBetweenChunks = 0x20000,
        DynamicScenery = 0x40000,
        SolidModel = 0x80000,
    };
};
CHECK_SIZE(ReferencedObjectFlags, 4);

// What references point at (the game's objects' base, its vtable at 0xA4: 1 the destructor, 2 woken and 3 put to sleep (whether
// it was asleep, and whether it wasn't), 4 released (whether it wasn't before), 5 its step once it was queued): its flags, its
// place (0x70 bytes of its own), its collision (0x10 bytes in), the chunk it's in (its data), the reference block at 0xB0
struct ReferencedObject
{
    enum Slot : u32
    {
        DestroySlot = 1,
        WakeSlot = 2,
        SleepSlot = 3,
        ReleaseSlot = 4,
        StepQueuedSlot = 5,
    };

    u32 unused00;
    ReferencedObjectFlags flags;
    ObjectPlace* place;
    u32 unused0C;
    ObjectCollision collision;
    struct ChunkData* chunk;
    const GccVTableEntry* vtable;
    u8 unusedA8[0xB0 - 0xA8];
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
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    // The box of its collision
    const Box* CollisionBox() const
    {
        return &collision.box;
    }
};
CHECK_OFFSET(ReferencedObject, flags, 4);
CHECK_OFFSET(ReferencedObject, unused0C, 0xC);
CHECK_OFFSET(ReferencedObject, chunk, 0xA0);
CHECK_OFFSET(ReferencedObject, reference, 0xB0);

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
    static constexpr u32 MostHandles = 32;

    u32 count;
    Reference* handles[MostHandles];

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
    enum Slot : u32
    {
        SlotIsDone = 3,
        SlotCurrent = 4,
    };

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

    // Through the vtable: whether it's done (a bool: its low byte), the object of the handle it's at
    bool AtEnd()
    {
        return (CallVirtual<u32>(this, vtable, SlotIsDone) & 0xFF) != 0;
    }

    ReferencedObject* Object()
    {
        Reference* const* handle = CallVirtual<Reference* const*>(this, vtable, SlotCurrent);
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
CHECK_SIZE(HandleWalk, 0xC);

extern "C"
{
    extern const GccVTableEntry g_HandleWalkVTable[] RETAIL(D_002F2DA8);
    extern const GccVTableEntry g_HandleWalkBaseVTable[] RETAIL(D_002F2DE0);
}

// The object's reference block, made the first time (its unused bits what the memory held), with one more reference
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
