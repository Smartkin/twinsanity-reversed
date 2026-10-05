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
constexpr u32 MessageTriggerItemType = 0x1814;
// The trigger's messages and the node's events of them, by what they're sent for
enum Message : u32
{
    FirstEntryMessage = 0,
    EveryEntryMessage = 1,
    EntryOrStayMessage = 2,
    ExitMessage = 3,
};

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
    for (u32 index = 0; index < node->bits.instanceCount; index++)
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
    u32 kinds = node->eventKinds | ObjectNodeKinds;
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
    u32 kinds = eventKinds | ObjectNodeKinds;
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
    ResetVector(&node->force);
    node->messageBits.forceOn = 0;
    node->messageBits.anyCharacter = 0;
    if (trigger->header.onEnterOnce != 0)
    {
        node->bits.tellsFirstEntry = 1;
        node->messages[FirstEntryMessage] = trigger->messages[FirstEntryMessage];
    }

    if (trigger->header.onEnter != 0)
    {
        node->bits.tellsEveryEntry = 1;
        node->messages[EveryEntryMessage] = trigger->messages[EveryEntryMessage];
    }

    if (trigger->header.onStay != 0)
    {
        node->bits.tellsEntryOrStay = 1;
        node->messages[EntryOrStayMessage] = trigger->messages[EntryOrStayMessage];
    }

    if (trigger->header.onExit != 0)
    {
        node->bits.tellsExit = 1;
        node->messages[ExitMessage] = trigger->messages[ExitMessage];
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
    return NodeMessageTrigger;
}

void MessageTriggerNode::Reset()
{
    TriggerNode::Reset();
    ResetVector(&force);
    messageBits.forceOn = 0;
}

u32 MessageTriggerNode::ItemType()
{
    return MessageTriggerItemType;
}

void MessageTriggerNode::BeginCheck()
{
    events[ExitMessage] = nullptr;
    events[FirstEntryMessage] = nullptr;
    events[EveryEntryMessage] = nullptr;
    events[EntryOrStayMessage] = nullptr;
}

void MessageTriggerNode::EnteredFirstTime(InstanceContext* instance)
{
    events[FirstEntryMessage] = Tell(messages[FirstEntryMessage], instance, events[FirstEntryMessage]);
}

void MessageTriggerNode::EnteredEveryTime(InstanceContext* instance)
{
    events[EveryEntryMessage] = Tell(messages[EveryEntryMessage], instance, events[EveryEntryMessage]);
}

void MessageTriggerNode::EnteredOrStayed(InstanceContext* instance)
{
    TellInto(this, EntryOrStayMessage, instance);
}

void MessageTriggerNode::Left(InstanceContext* instance)
{
    TellInto(this, ExitMessage, instance);
}
