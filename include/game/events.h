#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/memory.h"
#include "game/reference.h"

// A message sent to instances (the retail TriggerEventCaller, 0x14 bytes, still asm): the block of its references first (a
// block made for it owns it), 0x103, its type, the kinds of nodes it goes to, a reference argument and its vtable (at 0x10: 1 the
// destructor)
struct GameEvent
{
    Reference* reference;
    u16 unknown04;
    u16 type;
    // The kinds of nodes it goes to (a bit each)
    u32 kinds;
    Reference* argument;
    const GccVTableEntry* vtable;

    // The argument's handle is the constructor's, which lets it go (a reference passed by value)
    static GameEvent* Construct(GameEvent* event, u32 type, Reference** argument, u32 unknown) RETAIL(InitTriggerEvent);
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

extern "C"
{
    extern const GccVTableEntry g_GameEventVTable[] RETAIL(EventCaller_interface_);
    extern const GccVTableEntry g_CameraEventBaseVTable[] RETAIL(D_003052F8);
    extern const GccVTableEntry g_CameraEventVTable[] RETAIL(D_003052E0);

    // An event queued for an instance; the event's handle is the callee's, which lets it go (still asm)
    void QueueEvent(ReferencedObject* instance, Reference** event) RETAIL(FUN_001981c0);
}

// The event's block of references, made the first time (owning the event: the last reference destroys it, its flags are what
// the memory held but for the count), with one more reference
inline Reference* AddEventReference(GameEvent* event)
{
    if (event->reference == nullptr)
    {
        auto* block = static_cast<Reference*>(MemoryAllocate(sizeof(Reference)));
        u32 leftover = block->value;
        block->object = reinterpret_cast<ReferencedObject*>(event);
        block->value = (leftover & 0xFE000000) | ReferenceBits::Owns;
        event->reference = block;
    }

    Reference* block = event->reference;
    block->value = (block->value & ~ReferenceBits::CountMask) | (((block->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask);
    return block;
}
