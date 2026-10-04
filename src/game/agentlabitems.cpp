#include "game/agentlab.h"

#include "game/memory.h"
#include "game/navigation.h"
#include "game/properties.h"
#include "game/stream.h"

// The AgentLab module's items: the builder of the AgentLab and navigation items by their class IDs (the game context's, vtable
// D_002FD2C0) and the constructors it calls, a script resource written, a tagged value's destructor, and the module's statics

extern "C"
{
    // An AI position made: at the origin (w 1), no links, its previous position none (0xFF) and no flags
    AiPosition* ConstructAiPosition(AiPosition* position) RETAIL(FUN_0023ce48);
    // Settings 0x60 bytes before the states' jump table the game context's constructor sets, which nothing reads
    extern u32 g_UnusedAgentLabSettings[0x18] RETAIL(D_003D9A00);
    // A word the module's static initialisation clears, which nothing reads
    extern u32 g_UnusedAgentLabWord RETAIL(D_0030A910);
    // The module's statics made (GCC 2.9x's initialisation function, called for every priority), and its global constructor
    void InitAgentLabStatics(u32 initialise, u32 priority) RETAIL(FUN_002082e8);
    void ConstructAgentLabModule() RETAIL(FUN_00209ed8);
}

namespace
{
// The builder's class IDs (the items' ItemType)
enum ClassId : u32
{
    GraphStateId = 0x1800,
    GraphDataId = 0x1801,
    StarterId = 0x1803,
    GraphId = 0x1804,
    StateBodyId = 0x1805,
    ControlPacketId = 0x1808,
    CallConventionId = 0x1809,
    // Two words of -1, two words of 0 and 16 bytes left as the heap had them (unknown items)
    Unknown180CId = 0x180C,
    Unknown180EId = 0x180E,
    AiPositionId = 0x180F,
    AiPathId = 0x1810,
    Unknown1811Id = 0x1811,
};

constexpr u16 None = 0xFFFF;

template <typename T>
T* Allocate(u32 size = sizeof(T))
{
    return static_cast<T*>(MemoryAllocate(size));
}
}

GraphState* GraphState::Construct(GraphState* state)
{
    // No index (the high half)
    state->bits = static_cast<u32>(None) << 16;
    state->packet = nullptr;
    state->bodies = nullptr;
    state->next = nullptr;
    return state;
}

StateBody* StateBody::Construct(StateBody* body)
{
    body->condition = nullptr;
    body->jumpIndex = 0;
    body->commands = nullptr;
    body->next = nullptr;
    body->bits = 0;
    return body;
}

GraphData* GraphData::Construct(GraphData* data)
{
    data->name.string = nullptr;
    data->name.capacity = 0;
    data->name.length = 0;
    data->ClearHead();
    data->states = nullptr;
    return data;
}

void GraphData::ClearHead()
{
    bits = None;
    start = nullptr;
}

void ScriptResource::Write(Stream* stream)
{
    stream->WriteS32(static_cast<s32>(bits));
}

void TaggedValue::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

// What the builder makes of a class ID (none for an ID it doesn't make)
void* MakeAgentLabItem(void*, u32 classId)
{
    switch (classId)
    {
    case GraphStateId:
        return GraphState::Construct(Allocate<GraphState>());
    case GraphDataId:
        return GraphData::Construct(Allocate<GraphData>());
    case StarterId:
        return ScriptStarter::Construct(Allocate<ScriptStarter>());
    case GraphId:
    {
        auto* graph = Allocate<ScriptGraph>();
        ScriptResource::Construct(graph);
        graph->data = nullptr;
        graph->vtable = g_GraphVTable;
        return graph;
    }
    case StateBodyId:
        return StateBody::Construct(Allocate<StateBody>());
    case ControlPacketId:
        return ControlPacket::Construct(Allocate<ControlPacket>());
    case CallConventionId:
    {
        auto* convention = Allocate<CallConvention>();
        convention->SetDefaults();
        return convention;
    }
    case Unknown180CId:
    {
        auto* words = Allocate<s32>(2 * sizeof(s32));
        words[1] = -1;
        words[0] = -1;
        return words;
    }
    case Unknown180EId:
    {
        auto* words = Allocate<u32>(2 * sizeof(u32));
        words[0] = 0;
        words[1] = 0;
        return words;
    }
    case AiPositionId:
    {
        auto* position = Allocate<AiPosition>();
        ConstructAiPosition(position);
        return position;
    }
    case AiPathId:
    {
        auto* path = Allocate<AiPath>();
        path->chunkB = None;
        path->positionA = None;
        path->positionB = None;
        path->flags = 0;
        path->chunkA = None;
        return path;
    }
    case Unknown1811Id:
        return MemoryAllocate(0x10);
    default:
        return nullptr;
    }
}

// (The last word is left as it was)
void InitUnusedAgentLabSettings()
{
    u32* settings = g_UnusedAgentLabSettings;
    settings[0] = 2;
    settings[1] = 2;
    settings[2] = 0;
    settings[3] = 1;
    settings[4] = 0;
    settings[5] = 0;
    settings[6] = 0;
    settings[7] = 1;
    settings[8] = 1;
    settings[9] = 1;
    settings[10] = 0;
    settings[11] = 0;
    settings[12] = 0;
    settings[13] = 0;
    settings[14] = 0;
    settings[15] = 0;
    settings[16] = 0;
    settings[17] = 0;
    settings[18] = 0;
    settings[19] = 0;
    settings[20] = 0;
    settings[21] = 4;
    settings[22] = 2;
}

void InitAgentLabStatics(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_UnusedAgentLabWord = 0;
}

void ConstructAgentLabModule()
{
    InitAgentLabStatics(1, 0xFFFF);
}
