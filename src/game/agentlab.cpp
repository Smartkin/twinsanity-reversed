#include "game/agentlab.h"

#include "game/math.h"
#include "game/memory.h"
#include "game/properties.h"
#include "game/resources.h"
#include "game/stream.h"

namespace
{
// A script's priority when it's made
constexpr u32 DefaultPriority = 50;
// A command's vtable function giving its size, and the commands' and conditions' destructors
constexpr u32 SizeSlot = 5;
constexpr u32 DestroySlot = 1;

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
    script->bits = DefaultPriority << ScriptResource::PriorityShift | ScriptResource::IdMask;
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
    return static_cast<u16>(bits);
}

void ScriptResource::Resolve()
{
    bits |= Resolved;
}

void ScriptResource::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    bits &= ~Resolved;
}

u32 ScriptResource::ItemType()
{
    return 0x1802;
}

void CallConvention::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
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
    *reinterpret_cast<u32*>(&starter->assignerCount) = 0;
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
    if ((bits & Resolved) != 0)
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
        auto* graph = id != 0xFFFF ? static_cast<ScriptGraph*>(scripts->items[id & 0x7FFF]) : nullptr;
        if (graph != nullptr)
        {
            assigners[index]->graph = graph->data;
        }
    }

    bits |= Resolved;
}

void ScriptStarter::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    bits &= ~Resolved;
    // The count is read as a word
    stream->ReadS32(reinterpret_cast<s32*>(&assignerCount));
    for (u32 index = 0; index < assignerCount; index++)
    {
        auto* assigner = static_cast<Assigner*>(MemoryAllocate(sizeof(Assigner)));
        assigner->Read(stream);
        assigners[index] = assigner;
    }
}

u32 ScriptStarter::ItemType()
{
    return 0x1803;
}

u32 ScriptStarter::IsStarter()
{
    return 1;
}

ControlPacket* ControlPacket::Construct(ControlPacket* packet)
{
    packet->data = nullptr;
    *reinterpret_cast<u32*>(packet->counts) = 0;
    packet->settings = 0;
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
    stream->ReadS32(reinterpret_cast<s32*>(counts));
    stream->ReadS32(reinterpret_cast<s32*>(&settings));
    u32 size = counts[0] + counts[1] * 4;
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
    if (index >= counts[0])
    {
        return 0;
    }

    return SlotOf(index) != NoSlot;
}

u32 ControlPacket::Float(u32 index, f32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    *value = reinterpret_cast<const f32*>(data)[slot];
    return 1;
}

u32 ControlPacket::Word(u32 index, u32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    *value = reinterpret_cast<const u32*>(data)[slot];
    return 1;
}

u32 ControlPacket::Word2(u32 index, u32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    *value = reinterpret_cast<const u32*>(data)[slot];
    return 1;
}

u32 ControlPacket::GetFloat(u32 index, PropertyHolder* properties, f32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    if ((slot & PropertySlot) == 0)
    {
        *value = reinterpret_cast<const f32*>(data)[slot];
    }
    else
    {
        *value = properties->GetFloat(slot & ~PropertySlot);
    }

    return 1;
}

u32 ControlPacket::GetInt(u32 index, PropertyHolder* properties, s32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    if ((slot & PropertySlot) == 0)
    {
        *value = reinterpret_cast<const s32*>(data)[slot];
    }
    else
    {
        *value = properties->GetInt(slot & ~PropertySlot);
    }

    return 1;
}

u32 ControlPacket::GetAngle(u32 index, PropertyHolder* properties, s32* value)
{
    u32 slot = SlotOf(index);
    if (slot == NoSlot)
    {
        return 0;
    }

    TaggedValue angle;
    if ((slot & PropertySlot) == 0)
    {
        AngleFrom(&angle.raw, reinterpret_cast<const f32*>(data)[slot], 0);
    }
    else
    {
        PropertyHolder::GetTagged(&angle, properties, slot & ~PropertySlot);
    }

    *value = angle.raw;
    return 1;
}

u32 ControlPacket::GetRawAngles(u32 index, s32* angles)
{
    u32 missing = 0;
    for (u32 axis = 0; axis < 3; axis++)
    {
        u32 slot = SlotOf(index + axis);
        f32 radians = 0.0f;
        if (slot == NoSlot)
        {
            missing = 1;
        }
        else
        {
            radians = reinterpret_cast<const f32*>(data)[slot];
        }

        AngleFrom(&angles[axis], radians, 0);
    }

    return missing;
}

u32 ControlPacket::GetVector(u32 index, PropertyHolder* properties, Vector4* vector)
{
    u32 missing = 0;
    f32* values = &vector->x;
    for (u32 axis = 0; axis < 3; axis++)
    {
        u32 slot = SlotOf(index + axis);
        if (slot == NoSlot)
        {
            missing = 1;
            values[axis] = 0.0f;
        }
        else if ((slot & PropertySlot) == 0)
        {
            values[axis] = reinterpret_cast<const f32*>(data)[slot];
        }
        else
        {
            values[axis] = properties->GetFloat(slot & ~PropertySlot);
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
        u32 slot = SlotOf(index + axis);
        TaggedValue angle;
        if (slot == NoSlot)
        {
            missing = 1;
            AngleFrom(&angle.raw, 0.0f, 0);
        }
        else if ((slot & PropertySlot) == 0)
        {
            AngleFrom(&angle.raw, reinterpret_cast<const f32*>(data)[slot], 0);
        }
        else
        {
            PropertyHolder::GetTagged(&angle, properties, slot & ~PropertySlot);
        }

        angles[axis] = angle.raw;
    }

    return missing;
}

void StateBody::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    if ((bits & HasJump) != 0)
    {
        s32 index;
        stream->ReadS32(&index);
        jumpIndex = index;
    }

    condition = (bits & HasCondition) != 0 ? ReadCondition(stream) : nullptr;
    commands = (bits & CommandCountMask) != 0 ? ReadCommand(stream) : nullptr;
    if ((bits & HasNext) != 0)
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
        CallVirtual<void>(commands, commands->vtable, DestroySlot, u32{DestroyAndFree});
    }

    if (condition != nullptr)
    {
        CallVirtual<void>(condition, condition->vtable, DestroySlot, u32{DestroyAndFree});
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
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    ControlPacket* readPacket = nullptr;
    if ((bits & HasPacket) != 0)
    {
        readPacket = static_cast<ControlPacket*>(MemoryAllocate(sizeof(ControlPacket)));
        readPacket->data = nullptr;
        readPacket->Read(stream);
    }

    packet = readPacket;
    if ((bits & HasNext) != 0)
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
    if ((bits & BodyMask) == 0)
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
        if ((body->bits & StateBody::HasJump) != 0)
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
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
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
    return static_cast<u16>(data->bits);
}

void ScriptGraph::Resolve()
{
    if ((bits & Resolved) == 0)
    {
        bits |= Resolved;
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
    return 0x1804;
}

u32 ScriptGraph::IsStarter()
{
    return 0;
}

ObjectBuilder* ObjectBuilder::Construct(ObjectBuilder* builder)
{
    builder->factories = nullptr;
    for (u32& word : builder->unknown04)
    {
        word = 0;
    }

    return builder;
}

ScriptCommand* ReadCommand(Stream* stream)
{
    ObjectBuilder* builder = Builder();
    u32 bits;
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    auto* command = static_cast<ScriptCommand*>(builder->Build(bits & ScriptCommand::IdMask, ObjectBuilder::CommandKind));
    u32 size = CallVirtual<u32>(command, command->vtable, SizeSlot) - sizeof(ScriptCommand);
    if (size != 0)
    {
        stream->Read(reinterpret_cast<u8*>(command) + sizeof(ScriptCommand), size, 1);
    }

    if ((bits & ScriptCommand::HasNext) != 0)
    {
        ScriptCommand* next = ReadCommand(stream);
        command->bits = (command->bits & ~ScriptCommand::HasNext) | (next != nullptr ? ScriptCommand::HasNext : 0);
        command->next = next;
    }
    else
    {
        command->next = nullptr;
        command->bits &= ~ScriptCommand::HasNext;
    }

    return command;
}

ScriptCondition* ReadCondition(Stream* stream)
{
    ObjectBuilder* builder = Builder();
    u32 bits;
    stream->ReadS32(reinterpret_cast<s32*>(&bits));
    auto* condition = static_cast<ScriptCondition*>(builder->Build(static_cast<u16>(bits), ObjectBuilder::ConditionKind));
    condition->bits = bits;
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
        CallVirtual<void>(next, next->vtable, DestroySlot, u32{DestroyAndFree});
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
