#include "game/events.h"

#include "game/behaviours.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/reference.h"

// The behaviours' events made (game/events.h): a behaviour started on the node an event reaches, and the noises instances make,
// which the object nodes around them hear

extern "C"
{
    extern const GccVTableEntry g_ArgumentEventVTable[] RETAIL(D_002F0320);
    extern const GccVTableEntry g_ScriptEventVTable[] RETAIL(ScriptEventCaller_Methods);
    extern const GccVTableEntry g_NoiseEventVTable[] RETAIL(D_002FD268);
    // The kinds of nodes impacts reach (the start-up sets them)
    extern u32 g_ImpactKinds RETAIL(D_0030A120);
    u16* CopyHalfword(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
}

EABI_EXPORT(FUN_0020a090, NoiseEvent::Construct);
EABI_EXPORT(FUN_0020a358, SendImpact);

namespace
{
// GCC 2.9x's copy of a reference handle passed by value: one more reference counted
Reference* CopyHandle(Reference* handle)
{
    if (handle != nullptr)
    {
        handle->bits.count++;
    }

    return handle;
}

// The base classes' constructors of an event with an argument, which copy the handle passed by value for their parameters and
// let the copies go as each one ends
void ConstructArgumentEvent(GameEvent* event, u16 eventId, Reference* const* argument, u32 kinds)
{
    Reference* parameter = CopyHandle(*argument);
    Reference* baseParameter = CopyHandle(parameter);
    event->id = eventId;
    event->vtable = g_GameEventVTable;
    event->kinds = kinds;
    event->reference = nullptr;
    event->message = 0;
    event->argument = CopyHandle(baseParameter);
    RemoveReference(&baseParameter);
    event->vtable = g_ArgumentEventVTable;
    RemoveReference(&parameter);
}

// The handle QueueEvent takes is the callee's
void Queue(ReferencedObject* instance, GameEvent* event)
{
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(instance, &handle);
}
}

ScriptEvent* ScriptEvent::Construct(ScriptEvent* event, const u16* starter, u32 slot, u32 force, Reference** argument,
                                    InstanceContext* originator, u32 kinds)
{
    ConstructArgumentEvent(event, EventId, argument, kinds);
    event->vtable = g_ScriptEventVTable;
    CopyHalfword(&event->starter, starter);
    event->slot = static_cast<u8>(slot);
    event->bits.forced = force;
    event->originator = originator;
    RemoveReference(argument);
    return event;
}

NoiseEvent* NoiseEvent::Construct(NoiseEvent* event, f32 loudness, Reference** argument, u32 kinds)
{
    ConstructArgumentEvent(event, EventId, argument, kinds);
    event->loudness = loudness;
    event->vtable = g_NoiseEventVTable;
    RemoveReference(argument);
    return event;
}

void NoiseEvent::Apply(ObjectNodeBase* target, GameResources*)
{
    if (CallVirtual<u32>(target, target->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        return;
    }

    auto* node = static_cast<ObjectNode*>(target);
    HeadTracking* tracking = node->headTracking;
    if (tracking != nullptr && !tracking->flags.ignoredByLook && tracking->flags.tracking)
    {
        HearNoise(tracking, this, node);
    }

    if (!node->reactions.passesNoises)
    {
        return;
    }

    InstanceContext* instance = node->owner;
    Reference* handle = CopyHandle(argument);
    auto* event = static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent)));
    event = GameEvent::Construct(event, static_cast<u16>(node->reactions.noiseMessage), &handle, ObjectNodeKinds);
    Queue(instance, event);
}

// The instances in the sphere (the source left out) sent a noise event of the strength, made once with the source as its
// argument
void SendImpact(f32 radius, f32 strength, InstanceContext* source, const Vector4* position)
{
    constexpr u16 MostFound = 0x80;
    void* found[MostFound];
    InstanceQuery query;
    query.results = found;
    query.count = 0;
    query.most = MostFound;
    query.distance = NoHitDistance;
    // (Retail keeps the other bits as the stack had them, which nothing reads)
    query.bits.value = InstanceQueryBits::AllWanted;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    ChunkData* chunk = source->chunk;
    SkipInQuery(&query, source);
    Vector4 sphere = *position;
    sphere.w = radius;
    u32 count = ChunkInstancesInSphere(chunk, &sphere, g_ImpactKinds, &query, 1);
    NoiseEvent* event = nullptr;
    for (u16 index = 0; index < count; index++)
    {
        if (event == nullptr)
        {
            Reference* handle = source != nullptr ? AddReference(source) : nullptr;
            event = NoiseEvent::Construct(static_cast<NoiseEvent*>(MemoryAllocate(sizeof(NoiseEvent))), strength, &handle,
                                          ObjectNodeKinds);
        }

        Queue(static_cast<ReferencedObject*>(found[index]), event);
    }
}
