#include "game/agentlab.h"

#include "game/math.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/properties.h"
#include "game/resources.h"
#include "game/stream.h"

namespace
{
// A script's priority when it's made
constexpr u32 DefaultPriority = 50;

ObjectBuilder* Builder()
{
    if (g_ObjectBuilder == nullptr)
    {
        g_ObjectBuilder = ObjectBuilder::Construct(static_cast<ObjectBuilder*>(MemoryAllocate(sizeof(ObjectBuilder))));
    }

    return g_ObjectBuilder;
}

void ConstructBase(ScriptResource* script)
{
    ConstructResourceHeader(script);
    script->vtable = g_ScriptVTable;
    script->name.string = nullptr;
    script->name.capacity = 0;
    script->name.length = 0;
    ScriptResourceBits made = {};
    made.id = NoScriptId;
    made.priority = DefaultPriority;
    script->bits = made;
}
}

ScriptResource* ScriptResource::Construct(ScriptResource* script)
{
    ConstructBase(script);
    return script;
}

void ScriptResource::Destroy(u32 destroyFlags)
{
    vtable = g_ScriptVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u16 ScriptResource::Id()
{
    return bits.id;
}

void ScriptResource::Resolve()
{
    bits.resolved = 1;
}

void ScriptResource::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
    bits.resolved = 0;
}

u32 ScriptResource::ItemType()
{
    return ScriptClassId;
}

// The tool's locality (anywhere), status (any state) and preference (anyhow), which the game never reads
void CallConvention::SetDefaults()
{
    constexpr u32 Anywhere = 3;
    constexpr u32 AnyState = 2;
    constexpr u32 Anyhow = 5;
    CallConventionBits defaults = {};
    defaults.assignee = AssignNone;
    defaults.unused4 = Anywhere;
    defaults.unused8 = AnyState;
    defaults.unused12 = Anyhow;
    defaults.argument = NoArgument;
    bits = defaults;
}

void CallConvention::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
}

void CallConvention::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Assigner::Read(Stream* stream)
{
    s32 index;
    stream->ReadS32(&index);
    graphIndex = index;
    auto* read = static_cast<CallConvention*>(MemoryAllocate(sizeof(CallConvention)));
    read->Read(stream);
    convention = read;
}

void Assigner::Destroy(u32 destroyFlags)
{
    if (convention != nullptr)
    {
        convention->Destroy(DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

ScriptStarter* ScriptStarter::Construct(ScriptStarter* starter)
{
    ConstructBase(starter);
    starter->vtable = g_StarterVTable;
    starter->assignerCountWord = 0;
    return starter;
}

void ScriptStarter::Destroy(u32 destroyFlags)
{
    vtable = g_StarterVTable;
    for (u32 index = 0; index < assignerCount; index++)
    {
        if (assigners[index] != nullptr)
        {
            assigners[index]->Destroy(DestroyAndFree);
        }
    }

    vtable = g_ScriptVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScriptStarter::Resolve(ResourceTable* scripts)
{
    if (bits.resolved != 0)
    {
        return;
    }

    for (u32 index = 0; index < assignerCount; index++)
    {
        Assigner* assigner = assigners[index];
        if (assigner->graphIndex <= 0)
        {
            assigner->graph = nullptr;
            continue;
        }

        u16 id = static_cast<u16>(assigner->graphIndex - 1);
        auto* graph = id != NoScriptId ? static_cast<ScriptGraph*>(scripts->items[id & ResourceIndexMask]) : nullptr;
        if (graph != nullptr)
        {
            assigners[index]->graph = graph->data;
        }
    }

    bits.resolved = 1;
}

void ScriptStarter::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
    bits.resolved = 0;
    stream->ReadS32(reinterpret_cast<s32*>(&assignerCountWord));
    for (u32 index = 0; index < assignerCount; index++)
    {
        auto* assigner = static_cast<Assigner*>(MemoryAllocate(sizeof(Assigner)));
        assigner->Read(stream);
        assigners[index] = assigner;
    }
}

u32 ScriptStarter::ItemType()
{
    return StarterClassId;
}

u32 ScriptStarter::IsStarter()
{
    return 1;
}

ControlPacket* ControlPacket::Construct(ControlPacket* packet)
{
    packet->data = nullptr;
    packet->countsWord = 0;
    packet->settings.value = 0;
    return packet;
}

void ControlPacket::Destroy(u32 destroyFlags)
{
    if (data != nullptr)
    {
        MemoryDeallocate_(data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ControlPacket::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&countsWord));
    stream->ReadS32(reinterpret_cast<s32*>(&settings.value));
    u32 size = slotCount + floatCount * sizeof(f32);
    if (data != nullptr)
    {
        MemoryDeallocate_(data);
    }

    if (size == 0)
    {
        return;
    }

    data = static_cast<u8*>(MemoryAllocate2(size));
    stream->Read(data, size, 1);
}

u32 ControlPacket::HasByte(u32 index)
{
    if (index >= slotCount)
    {
        return 0;
    }

    return SlotOf(index).value != NoSlot;
}

u32 ControlPacket::Float(u32 index, f32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    *value = Floats()[slot.value];
    return 1;
}

u32 ControlPacket::Word(u32 index, u32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    *value = reinterpret_cast<const u32*>(data)[slot.value];
    return 1;
}

u32 ControlPacket::Word2(u32 index, u32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    *value = reinterpret_cast<const u32*>(data)[slot.value];
    return 1;
}

u32 ControlPacket::GetFloat(u32 index, PropertyHolder* properties, f32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    if (slot.isProperty == 0)
    {
        *value = Floats()[slot.value];
    }
    else
    {
        *value = properties->GetFloat(slot.index);
    }

    return 1;
}

u32 ControlPacket::GetInt(u32 index, PropertyHolder* properties, s32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    if (slot.isProperty == 0)
    {
        *value = reinterpret_cast<const s32*>(data)[slot.value];
    }
    else
    {
        *value = properties->GetInt(slot.index);
    }

    return 1;
}

u32 ControlPacket::GetAngle(u32 index, PropertyHolder* properties, s32* value)
{
    ValueSlot slot = SlotOf(index);
    if (slot.value == NoSlot)
    {
        return 0;
    }

    TaggedValue angle;
    if (slot.isProperty == 0)
    {
        AngleFrom(&angle.raw, Floats()[slot.value], AngleRadians);
    }
    else
    {
        PropertyHolder::GetTagged(&angle, properties, slot.index);
    }

    *value = angle.raw;
    return 1;
}

u32 ControlPacket::GetRawAngles(u32 index, s32* angles)
{
    u32 missing = 0;
    for (u32 axis = 0; axis < 3; axis++)
    {
        ValueSlot slot = SlotOf(index + axis);
        f32 radians = 0.0f;
        if (slot.value == NoSlot)
        {
            missing = 1;
        }
        else
        {
            radians = Floats()[slot.value];
        }

        AngleFrom(&angles[axis], radians, AngleRadians);
    }

    return missing;
}

u32 ControlPacket::GetVector(u32 index, PropertyHolder* properties, Vector4* vector)
{
    u32 missing = 0;
    f32* values = &vector->x;
    for (u32 axis = 0; axis < 3; axis++)
    {
        ValueSlot slot = SlotOf(index + axis);
        if (slot.value == NoSlot)
        {
            missing = 1;
            values[axis] = 0.0f;
        }
        else if (slot.isProperty == 0)
        {
            values[axis] = Floats()[slot.value];
        }
        else
        {
            values[axis] = properties->GetFloat(slot.index);
        }
    }

    vector->w = 1.0f;
    return missing;
}

u32 ControlPacket::GetAngles(u32 index, PropertyHolder* properties, s32* angles)
{
    u32 missing = 0;
    for (u32 axis = 0; axis < 3; axis++)
    {
        ValueSlot slot = SlotOf(index + axis);
        TaggedValue angle;
        if (slot.value == NoSlot)
        {
            missing = 1;
            AngleFrom(&angle.raw, 0.0f, AngleRadians);
        }
        else if (slot.isProperty == 0)
        {
            AngleFrom(&angle.raw, Floats()[slot.value], AngleRadians);
        }
        else
        {
            PropertyHolder::GetTagged(&angle, properties, slot.index);
        }

        angles[axis] = angle.raw;
    }

    return missing;
}

void StateBody::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
    if (bits.hasJump != 0)
    {
        s32 index;
        stream->ReadS32(&index);
        jumpIndex = index;
    }

    condition = bits.hasCondition != 0 ? ReadCondition(stream) : nullptr;
    commands = bits.commandCount != 0 ? ReadCommand(stream) : nullptr;
    if (bits.hasNext != 0)
    {
        auto* following = static_cast<StateBody*>(MemoryAllocate(sizeof(StateBody)));
        following->Read(stream);
        next = following;
    }
    else
    {
        next = nullptr;
    }
}

void StateBody::Destroy(u32 destroyFlags)
{
    if (next != nullptr)
    {
        next->Destroy(DestroyAndFree);
    }

    if (commands != nullptr)
    {
        CallVirtual<void>(commands, commands->vtable, ScriptCommand::DestroySlot, u32{DestroyAndFree});
    }

    if (condition != nullptr)
    {
        CallVirtual<void>(condition, condition->vtable, ScriptCondition::DestroySlot, u32{DestroyAndFree});
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GraphState::Read(Stream* stream, u32 readBodies)
{
    g_StateJumpTable[g_StateCount] = this;
    g_StateCount++;
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
    ControlPacket* readPacket = nullptr;
    if (bits.hasPacket != 0)
    {
        readPacket = static_cast<ControlPacket*>(MemoryAllocate(sizeof(ControlPacket)));
        readPacket->data = nullptr;
        readPacket->Read(stream);
    }

    packet = readPacket;
    if (bits.hasNext != 0)
    {
        auto* following = static_cast<GraphState*>(MemoryAllocate(sizeof(GraphState)));
        following->Read(stream, 0);
        next = following;
    }
    else
    {
        next = nullptr;
    }

    if (readBodies == 0)
    {
        bodies = nullptr;
        return;
    }

    ReadBodies(stream);
}

void GraphState::ReadBodies(Stream* stream)
{
    if (bits.bodyCount == 0)
    {
        bodies = nullptr;
        return;
    }

    auto* first = static_cast<StateBody*>(MemoryAllocate(sizeof(StateBody)));
    first->Read(stream);
    bodies = first;
}

void GraphState::ResolveJumps()
{
    for (StateBody* body = bodies; body != nullptr; body = body->next)
    {
        if (body->bits.hasJump != 0)
        {
            body->jump = g_StateJumpTable[body->jumpIndex];
        }
    }
}

void GraphState::Destroy(u32 destroyFlags)
{
    if (packet != nullptr)
    {
        packet->Destroy(DestroyAndFree);
    }

    if (bodies != nullptr)
    {
        bodies->Destroy(DestroyAndFree);
    }

    if (next != nullptr)
    {
        next->Destroy(DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GraphData::Read(Stream* stream)
{
    g_StateCount = 0;
    stream->ReadS32(reinterpret_cast<s32*>(&bits.value));
    StringRead(&name, stream);
    s32 stateCount;
    stream->ReadS32(&stateCount);
    s32 startIndex;
    stream->ReadS32(&startIndex);
    auto* first = static_cast<GraphState*>(MemoryAllocate(sizeof(GraphState)));
    first->Read(stream, 0);
    states = first;
    start = nullptr;
    s32 index = 0;
    for (GraphState* state = first; state != nullptr; state = state->next)
    {
        state->ReadBodies(stream);
        state->ResolveJumps();
        if (index == startIndex)
        {
            start = state;
        }

        index++;
    }
}

void ScriptGraph::Destroy(u32 destroyFlags)
{
    vtable = g_GraphVTable;
    if (data != nullptr)
    {
        if (data->states != nullptr)
        {
            data->states->Destroy(DestroyAndFree);
        }

        StringDestroy(&data->name);
        MemoryDeallocate2_(data);
    }

    vtable = g_ScriptVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u16 ScriptGraph::Id()
{
    return data->bits.id;
}

void ScriptGraph::Resolve()
{
    if (bits.resolved == 0)
    {
        bits.resolved = 1;
    }
}

void ScriptGraph::Read(Stream* stream)
{
    auto* read = static_cast<GraphData*>(MemoryAllocate(sizeof(GraphData)));
    read->name.string = nullptr;
    read->name.capacity = 0;
    read->name.length = 0;
    read->Read(stream);
    data = read;
}

u32 ScriptGraph::ItemType()
{
    return GraphClassId;
}

u32 ScriptGraph::IsStarter()
{
    return 0;
}

ObjectBuilder* ObjectBuilder::Construct(ObjectBuilder* builder)
{
    builder->first = nullptr;
    builder->last = nullptr;
    builder->count = 0;
    builder->added = 0;
    return builder;
}

void* ObjectBuilder::Build(u32 id, s32 kind)
{
    constexpr u32 MakeSlot = 2;
    for (Node* node = first; node != nullptr; node = node->next)
    {
        void* factory = node->factory;
        void* made = CallVirtual<void*>(factory, *static_cast<const GccVTableEntry* const*>(factory), MakeSlot, id, kind);
        if (made != nullptr)
        {
            return made;
        }
    }

    return nullptr;
}

void ObjectBuilder::Add(void* factory)
{
    auto* node = static_cast<Node*>(MemoryAllocate(sizeof(Node)));
    node->factory = factory;
    node->previous = nullptr;
    node->next = nullptr;
    if (count == 0)
    {
        last = node;
        first = node;
    }
    else
    {
        node->next = first;
        first->previous = node;
        first = node;
    }

    count++;
    added++;
}

// The command's ID read isn't kept (the builder's stays)
ScriptCommand* ReadCommand(Stream* stream)
{
    ObjectBuilder* builder = Builder();
    ScriptCommandBits read;
    stream->ReadS32(reinterpret_cast<s32*>(&read.value));
    auto* command = static_cast<ScriptCommand*>(builder->Build(read.id, ObjectBuilder::CommandKind));
    u32 size = CallVirtual<u32>(command, command->vtable, ScriptCommand::SizeSlot) - sizeof(ScriptCommand);
    if (size != 0)
    {
        stream->Read(reinterpret_cast<u8*>(command) + sizeof(ScriptCommand), size, 1);
    }

    if (read.hasNext != 0)
    {
        ScriptCommand* next = ReadCommand(stream);
        command->bits.hasNext = next != nullptr;
        command->next = next;
    }
    else
    {
        command->next = nullptr;
        command->bits.hasNext = 0;
    }

    return command;
}

ScriptCondition* ReadCondition(Stream* stream)
{
    ObjectBuilder* builder = Builder();
    ScriptConditionBits read;
    stream->ReadS32(reinterpret_cast<s32*>(&read.value));
    auto* condition = static_cast<ScriptCondition*>(builder->Build(read.id, ObjectBuilder::ConditionKind));
    condition->bits = read;
    for (f32& value : condition->values)
    {
        stream->ReadF32(&value);
    }

    return condition;
}

void ScriptCommand::Destroy(u32 destroyFlags)
{
    vtable = g_ScriptCommandVTable;
    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, ScriptCommand::DestroySlot, u32{DestroyAndFree});
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScriptCommand::ParseTokens()
{
}

void ScriptCommand::ExecuteOn()
{
}

u32 ScriptCommand::GetClassId()
{
    return ClassId;
}

void ScriptCondition::Destroy(u32 destroyFlags)
{
    vtable = g_ScriptConditionVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 ScriptCondition::GetClassId()
{
    return ClassId;
}
