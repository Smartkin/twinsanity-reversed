#include "game/instances.h"

#include "game/events.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/reference.h"

// The message triggers' nodes (game/instances.h): what enters, stays in and leaves a trigger's box sent its messages as events

extern "C"
{
    extern const GccVTableEntry g_MessageTriggerNodeVTable[] RETAIL(TriggerNode_Methods);
}

namespace
{
constexpr u32 MessageTriggerKind = 7;
constexpr u32 MessageTriggerItemType = 0x1814;
// The kinds of nodes the events go to besides the node's own: the agents'
constexpr u32 AgentEventKind = 0x2;
// The trigger's header's message bits: each sends one of its messages
constexpr u32 SendsEntered = 0x800;
constexpr u32 SendsEnteredSecond = 0x100;
constexpr u32 SendsEnteredAndStayed = 0x200;
constexpr u32 SendsLeft = 0x400;

GameEvent* MakeEvent(u16 message, InstanceContext* argument, u32 kinds)
{
    Reference* handle = argument != nullptr ? AddReference(argument) : nullptr;
    return GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &handle, kinds);
}

// The handle QueueEvent takes is the callee's
void Queue(InstanceContext* instance, GameEvent* event)
{
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(instance, &handle);
}

// Each instance the trigger tells sent an event of the message with the instance as its argument, made once (the message read
// when it's made)
void TellInstances(MessageTriggerNode* node, const u16* message, InstanceContext* instance, u32 kinds)
{
    GameEvent* told = nullptr;
    for (u32 index = 0; index < node->instanceCount; index++)
    {
        if (told == nullptr)
        {
            told = MakeEvent(*message, instance, kinds);
        }

        Queue(node->instances[index], told);
    }
}

// What Tell does, the event of the message kept as soon as it's made (EnteredOrStayed and Left have it inline). The trigger's
// instances aren't capped at the 35 the node has room for (TriggerNode::AddInstance): past them they're the vector, the bits, the
// messages and the events
void TellInto(MessageTriggerNode* node, u32 index, InstanceContext* instance)
{
    u32 kinds = node->eventKinds | AgentEventKind;
    if (node->events[index] == nullptr)
    {
        node->events[index] = MakeEvent(node->messages[index], node->owner, kinds);
    }

    Queue(instance, node->events[index]);
    TellInstances(node, &node->messages[index], instance, kinds);
}

void ResetVector(Vector4* vector)
{
    vector->x = 0.0f;
    vector->w = 1.0f;
    vector->y = 0.0f;
    vector->z = 0.0f;
}
}

GameEvent* MessageTriggerNode::Tell(u16 message, InstanceContext* instance, GameEvent* event)
{
    u32 kinds = eventKinds | AgentEventKind;
    if (event == nullptr)
    {
        event = MakeEvent(message, owner, kinds);
    }

    Queue(instance, event);
    TellInstances(this, &message, instance, kinds);
    return event;
}

TriggerNode* ConstructMessageTriggerNode(void* memory, ChunkData* chunk, LayoutTrigger* layoutTrigger)
{
    auto* node = static_cast<MessageTriggerNode*>(memory);
    TriggerNode::Construct(node, chunk, layoutTrigger);
    auto* trigger = static_cast<MessageTrigger*>(layoutTrigger);
    node->vtable = g_MessageTriggerNodeVTable;
    ResetVector(&node->unknown170);
    node->messageBits &= ~(MessageTriggerNode::Bit0 | MessageTriggerNode::BitAnyCharacter);
    if ((trigger->header & SendsEntered) != 0)
    {
        node->Bits() |= TriggerNode::TellsEntered;
        node->messages[0] = trigger->messages[0];
    }

    if ((trigger->header & SendsEnteredSecond) != 0)
    {
        node->Bits() |= TriggerNode::TellsEnteredSecond;
        node->messages[1] = trigger->messages[1];
    }

    if ((trigger->header & SendsEnteredAndStayed) != 0)
    {
        node->Bits() |= TriggerNode::TellsEnteredAndStayed;
        node->messages[2] = trigger->messages[2];
    }

    if ((trigger->header & SendsLeft) != 0)
    {
        node->Bits() |= TriggerNode::TellsLeft;
        node->messages[3] = trigger->messages[3];
    }

    return node;
}

void MessageTriggerNode::Destroy(u32 destroyFlags)
{
    vtable = g_MessageTriggerNodeVTable;
    TriggerNode::Destroy(destroyFlags);
}

u32 MessageTriggerNode::Kind()
{
    return MessageTriggerKind;
}

void MessageTriggerNode::Reset()
{
    TriggerNode::Reset();
    ResetVector(&unknown170);
    messageBits &= ~Bit0;
}

u32 MessageTriggerNode::ItemType()
{
    return MessageTriggerItemType;
}

void MessageTriggerNode::BeginCheck()
{
    events[3] = nullptr;
    events[0] = nullptr;
    events[1] = nullptr;
    events[2] = nullptr;
}

void MessageTriggerNode::Entered(InstanceContext* instance)
{
    events[0] = Tell(messages[0], instance, events[0]);
}

void MessageTriggerNode::EnteredSecond(InstanceContext* instance)
{
    events[1] = Tell(messages[1], instance, events[1]);
}

void MessageTriggerNode::EnteredOrStayed(InstanceContext* instance)
{
    TellInto(this, 2, instance);
}

void MessageTriggerNode::Left(InstanceContext* instance)
{
    TellInto(this, 3, instance);
}
