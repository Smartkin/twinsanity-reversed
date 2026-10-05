#include "game/agentlab.h"

#include "game/memory.h"
#include "game/navigation.h"
#include "game/properties.h"
#include "game/stream.h"

// The AgentLab module's items: the builder of the AgentLab and navigation items by their class IDs (the game context's, vtable
// D_002FD2C0) and the constructors it calls, a script resource written, a tagged value's destructor, and the module's statics

extern "C"
{
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
// The size of the block made for UnusedBlockClassId
constexpr u32 UnusedBlockSize = 0x10;

template <typename T>
T* Allocate(u32 size = sizeof(T))
{
    return static_cast<T*>(MemoryAllocate(size));
}
}

GraphState* GraphState::Construct(GraphState* state)
{
    GraphStateBits made = {};
    made.child = NoScriptId;
    state->bits = made;
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
    body->bits.value = 0;
    return body;
}

GraphData* GraphData::Construct(GraphData* graph)
{
    graph->name.string = nullptr;
    graph->name.capacity = 0;
    graph->name.length = 0;
    graph->ClearHead();
    graph->states = nullptr;
    return graph;
}

void GraphData::ClearHead()
{
    GraphDataBits cleared = {};
    cleared.id = NoScriptId;
    bits = cleared;
    start = nullptr;
}

void ScriptResource::Write(Stream* stream)
{
    stream->WriteS32(static_cast<s32>(bits.value));
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
    case GraphStateClassId:
        return GraphState::Construct(Allocate<GraphState>());
    case GraphDataClassId:
        return GraphData::Construct(Allocate<GraphData>());
    case StarterClassId:
        return ScriptStarter::Construct(Allocate<ScriptStarter>());
    case GraphClassId:
    {
        auto* graph = Allocate<ScriptGraph>();
        ScriptResource::Construct(graph);
        graph->data = nullptr;
        graph->vtable = g_GraphVTable;
        return graph;
    }
    case StateBodyClassId:
        return StateBody::Construct(Allocate<StateBody>());
    case ControlPacketClassId:
        return ControlPacket::Construct(Allocate<ControlPacket>());
    case CallConventionClassId:
    {
        auto* convention = Allocate<CallConvention>();
        convention->SetDefaults();
        return convention;
    }
    case UnusedNonesClassId:
    {
        auto* words = Allocate<s32>(2 * sizeof(s32));
        words[1] = -1;
        words[0] = -1;
        return words;
    }
    case UnusedZerosClassId:
    {
        auto* words = Allocate<u32>(2 * sizeof(u32));
        words[0] = 0;
        words[1] = 0;
        return words;
    }
    case AiPositionClassId:
    {
        auto* position = Allocate<AiPosition>();
        AiPosition::Construct(position);
        return position;
    }
    case AiPathClassId:
    {
        auto* path = Allocate<AiPath>();
        path->chunkB = NoAiIndex;
        path->positionA = NoAiIndex;
        path->positionB = NoAiIndex;
        path->flags.value = 0;
        path->chunkA = NoAiIndex;
        return path;
    }
    case UnusedBlockClassId:
        return MemoryAllocate(UnusedBlockSize);
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
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_UnusedAgentLabWord = 0;
}

void ConstructAgentLabModule()
{
    InitAgentLabStatics(1, DefaultInitPriority);
}
