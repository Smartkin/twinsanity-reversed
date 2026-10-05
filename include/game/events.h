#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/memory.h"
#include "game/reference.h"

// A message of none (trigger messages, the messages scripts and objects send)
constexpr u16 NoMessage = 0xFFFF;

// A message sent to instances (the retail TriggerEventCaller, 0x14 bytes): the block of its references first (a block made for it
// owns it), its event's ID (EventId, or the derived event's), the message (its derived events leave it 0), the kinds of nodes it
// goes to, a reference argument and its vtable (at 0x10: 1 the destructor)
struct GameEvent
{
    enum Slot : u32
    {
        DestroySlot = 1,
        ApplySlot = 2,
    };

    static constexpr u16 EventId = 0x103;

    Reference* reference;
    u16 id;
    u16 message;
    // The kinds of nodes it goes to (a bit each)
    u32 kinds;
    Reference* argument;
    const GccVTableEntry* vtable;

    // The argument's handle is the constructor's, which lets it go (a reference passed by value)
    static GameEvent* Construct(GameEvent* event, u32 message, Reference** argument, u32 kinds) RETAIL(InitTriggerEvent);
    // Its destructor (TriggerEventCaller_Methods_'s), and the same code under its base classes' vtables: the event with an argument's
    // (D_002F0320) and the event caller's (EventCaller_interface_). Its argument let go, its reference block told it's gone
    void Destroy(u32 destroyFlags) RETAIL(FUN_00123068);
    void ArgumentEventDestroy(u32 destroyFlags) RETAIL(FUN_00123008);
    void EventCallerDestroy(u32 destroyFlags) RETAIL(FUN_00122fa0);
    // Its and the event with an argument's slot 2 (applied to a node): nothing
    void ApplyNothing() RETAIL(FUN_00123000);
};
CHECK_SIZE(GameEvent, 0x14);

struct CameraNode;

// A camera trigger's event (0x18 bytes, retail's vtable D_003052E0 over D_003052F8): what entered (or the node's instance) and
// the node
struct CameraEvent : GameEvent
{
    static constexpr u16 EventId = 0x900;

    CameraNode* node;

    // The argument's handle is the constructor's (a reference passed by value)
    static CameraEvent* Construct(CameraEvent* event, Reference** argument, u32 kinds, CameraNode* node) RETAIL(FUN_0027e298);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027e998);
    // Its base class's destructor
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0027e938);
};
CHECK_SIZE(CameraEvent, 0x18);

struct GameResources;
struct InstanceContext;
struct ObjectNodeBase;

// A script event's bits: whether its starter is forced on the runner (taken whatever runs)
union ScriptEventBits
{
    u8 value;
    struct
    {
        u8 forced : 1;
        u8 unused1 : 7;
    };
};
CHECK_SIZE(ScriptEventBits, 1);

// A behaviour started on the object node it reaches (retail's ScriptEventCaller, 0x1C bytes, vtable ScriptEventCaller_Methods:
// 1 the destructor, 2 applied): the index of the starter among the game's behaviours (0xFFFF none), the runner's slot, its bits
// and its originator
struct ScriptEvent : GameEvent
{
    static constexpr u16 EventId = 0x100;

    u16 starter;
    u8 slot;
    ScriptEventBits bits;
    InstanceContext* originator;

    // The argument's handle is the constructor's (a reference passed by value)
    static ScriptEvent* Construct(ScriptEvent* event, const u16* starter, u32 slot, u32 force, Reference** argument,
                                  InstanceContext* originator, u32 kinds) RETAIL(FUN_0020a5a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020ecd8);
    // The starter started on the node in the runner of its slot
    void Apply(ObjectNodeBase* node, GameResources* resources) RETAIL(FUN_0020ed38);
};
CHECK_SIZE(ScriptEvent, 0x1C);

// A noise an instance made, which the object nodes around it hear (retail's vtable D_002FD268, 0x18 bytes): how loud it is
struct NoiseEvent : GameEvent
{
    static constexpr u16 EventId = 0x101;

    f32 loudness;

    // The argument's handle is the constructor's (a reference passed by value)
    static NoiseEvent* Construct(NoiseEvent* event, f32 loudness, Reference** argument, u32 kinds) RETAIL_N32(FUN_0020a090);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020ec68);
    // Heard by the object node it reaches (by its head tracking while that tracks and looks), and passed on to its instance as the
    // message of the node's bits 42-57 (made of the noise's argument) when its bit 40 says so
    void Apply(ObjectNodeBase* node, GameResources* resources) RETAIL(FUN_0020a1c0);
};
CHECK_SIZE(NoiseEvent, 0x18);

extern "C"
{
    extern const GccVTableEntry g_GameEventVTable[] RETAIL(EventCaller_interface_);
    extern const GccVTableEntry g_CameraEventBaseVTable[] RETAIL(D_003052F8);
    extern const GccVTableEntry g_CameraEventVTable[] RETAIL(D_003052E0);

    // An event queued for an instance; the event's handle is the callee's, which lets it go
    void QueueEvent(ReferencedObject* instance, Reference** event) RETAIL(FUN_001981c0);
}

// The event's block of references, made the first time (owning the event: the last reference destroys it, its unused bits what
// the memory held), with one more reference
inline Reference* AddEventReference(GameEvent* event)
{
    if (event->reference == nullptr)
    {
        auto* block = static_cast<Reference*>(MemoryAllocate(sizeof(Reference)));
        ReferenceBits leftover = block->bits;
        block->object = reinterpret_cast<ReferencedObject*>(event);
        leftover.count = 0;
        leftover.owns = 1;
        block->bits = leftover;
        event->reference = block;
    }

    Reference* block = event->reference;
    block->bits.count++;
    return block;
}
