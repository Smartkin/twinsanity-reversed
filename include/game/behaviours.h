#pragma once

#include "abi.h"
#include "common.h"
#include "game/agentlab.h"
#include "gcc2.h"

class GameNode;
struct Reference;
struct TimeClock;

// The behaviour scripts at work: an agent's runner keeps a stack of levels, each running a graph; a state whose child behaviour
// runs puts a level on top for it

// A level's bits: its index in the stack, whether it finished, whether the last body restarts its state, and a message the level
// above it gave it (SetParentExecutionValue; NoMessage none), which a condition takes once
union BehaviourLevelBits
{
    u32 value;
    struct
    {
        u32 index : 8;
        u32 finished : 1;
        u32 restart : 1;
        u32 unused10 : 6;
        u32 message : 16;
    };
};
CHECK_SIZE(BehaviourLevelBits, 4);

// A level of a runner's stack (retail's ExecutionState, 0x20 bytes; vtable 0x1C bytes in: 1 the destructor): the graph it runs,
// the state it's in and the one it last entered (to tell a first entry; none after a restart), the child graph its state runs,
// the body the Else condition marked while the bodies were checked (run when none passes), when its last body ran, and its bits
struct BehaviourLevel
{
    GraphData* graph;
    GraphState* state;
    GraphState* entered;
    GraphData* child;
    StateBody* elseBody;
    u32 time;
    BehaviourLevelBits bits;
    const GccVTableEntry* vtable;

    static BehaviourLevel* Construct(BehaviourLevel* level) RETAIL(ConstructExecutionState);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002206e0);
    // Made to run a graph from its start (the clock's time its last body's)
    void Start(GraphData* graph, TimeClock* clock) RETAIL(InitExecutionState);
};
CHECK_SIZE(BehaviourLevel, 0x20);

// The receivers' bits: the starter's priority (the scripts can change it), whether the same starter queued again restarts the
// running one (SetRestartable sets it), the starter's assigners, the runners using them and the assigners whose graph runs (a
// bit each)
union StarterReceiversBits
{
    u32 value;
    struct
    {
        u32 priority : 7;
        u32 restartable : 1;
        u32 assignerCount : 4;
        u32 users : 4;
        u32 running : 8;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(StarterReceiversBits, 4);

// The instances a starter's assigners run their graphs on (0x2C bytes): the starter, an instance for each assigner (the first the
// runner's agent's), the originator, and bits
struct StarterReceivers
{
    ScriptStarter* starter;
    struct InstanceContext* instances[8];
    void* originator;
    StarterReceiversBits bits;

    static StarterReceivers* Construct(StarterReceivers* receivers, ScriptStarter* starter) RETAIL(FUN_00254ad8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00254b28);
    // Made the starter's again (the originator kept)
    void Reset(ScriptStarter* starter) RETAIL(FUN_00254c40);
    // The runner stopped for each running assigner and their agents' nodes told; none run then
    void ReleaseAll(struct BehaviourRunner* runner) RETAIL(FUN_00254b50);
    // Each assigner's instance found by its call convention and its graph started on that instance's runner of the runner's slot
    // (the first on the runner itself)
    void Resolve(struct BehaviourRunner* runner) RETAIL(ResolveAssignerReceivers);
};
CHECK_SIZE(StarterReceivers, 0x2C);
CHECK_OFFSET(StarterReceivers, instances, 0x4);
CHECK_OFFSET(StarterReceivers, originator, 0x24);
CHECK_OFFSET(StarterReceivers, bits, 0x28);

// A runner's flags: run the levels again, the packet's end is due, a packet is waiting (its motion runs), a packet runs, the
// runner's slot among its node's, and whether the interrupting states are checked from the interrupt level
union BehaviourRunnerFlags
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 runLevels : 1;
        u32 unused2 : 1;
        u32 packetEnded : 1;
        u32 packetWaiting : 1;
        u32 packetRuns : 1;
        u32 slot : 3;
        u32 interruptFromLevel : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(BehaviourRunnerFlags, 4);

// An agent's behaviour runner (retail's ScriptCall, 0x60 bytes): its flags, the agent's node, the control packet that runs and the
// last one, its receivers, the starter's assigner it runs, its originator, the stack's depth and interrupt level (NoLevel none)
// and its levels
struct BehaviourRunner
{
    static constexpr u32 Levels = 8;
    static constexpr u8 NoLevel = 0xFF;
    // The designator of the originator (those below it are the receivers' indexes)
    static constexpr u32 OriginatorDesignator = 0xFF;

    BehaviourRunnerFlags flags;
    GameNode* agentNode;
    ControlPacket* packet;
    ControlPacket* lastPacket;
    // The packet's squared tolerance (its RandRange value)
    f32 tolerance;
    StarterReceivers* receivers;
    // The starter to run next and its originator (taken at the next frame)
    ScriptStarter* nextStarter;
    void* nextOriginator;
    void* originator;
    // The ID of the starter of the receivers it had when it was last started (NoScriptId none), which RestartPrevious starts
    // again
    u16 previousStarter;
    u8 unused26;
    // The joint the packet moves (0xFF the instance)
    u8 joint;
    u8 depth;
    u8 interruptLevel;
    u8 unused2A[2];
    BehaviourLevel* levels[Levels];
    // Cleared when the runner's made and started, never read
    u32 unused4C;
    // The packet's sync unit, when it ends (with its delay) and when it started (clock units)
    u32 syncUnit;
    s32 packetEnd;
    s32 packetStart;
    // The time the scripts marked (MarkTime, 0 none)
    u32 markedTime;

    static BehaviourRunner* Construct(BehaviourRunner* runner, GameNode* agentNode, u32 slot) RETAIL(ConstructScriptCall);
    // A frame: the interrupting states checked, the levels run (when asked, or once the packet ended), the packet's frame.
    // Whether the behaviour finished
    u32 Update(TimeClock* clock) RETAIL(UpdateAgentLab);
    // The levels run from the first (the packet that ended given): whether the behaviour finished (then stopped)
    u32 RunLevels(TimeClock* clock, ControlPacket* ended) RETAIL(FUN_0020fa40);
    // The packet that runs given up (the last one), and the agent told when its packets took turns
    void EndPacket() RETAIL(FUN_0020c9f8);
    // The levels above a level marked finished (their states none), the stack cut down to it
    void Unwind(BehaviourLevel* level) RETAIL(UnwindBehaviourStack);
    // A level put on top for a state's child behaviour (made the first time)
    void EnterChild(u32 level, GraphState* state) RETAIL(EnterChildBehaviour);
    // The states that interrupt checked level by level: whether one switched
    u32 CheckInterrupts(TimeClock* clock) RETAIL(CheckInterruptingStates);
    // Stopped: no graph, packet or receivers (they're let go when asked and no other runner uses them)
    void Stop(u32 release) RETAIL(FUN_0020cc38);
    // A packet's frame: the agent's route started, then the agent's frame (1)
    u32 StepPacket(TimeClock* clock) RETAIL(FUN_0020fc90);
    // A graph started for receivers (the ones it had let go when they're others): its levels made again, the packets none
    void Start(StarterReceivers* receivers, GraphData* graph) RETAIL(FUN_0020cae0);
    // The next starter taken: its receivers made again (or made) and resolved
    void TakeStarter() RETAIL(FUN_0020fbe8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020faa0);
    // Every level destroyed
    void DestroyLevels() RETAIL(FUN_0020faf8);
    // A starter to take at the next frame (forced, or of a priority above the next one's and the running one's, or of the same
    // priority as the running one's but another starter, the same one when its receivers allow it): whether it's taken
    u32 QueueStarter(ScriptStarter* starter, void* originator, u32 force) RETAIL(FUN_0020fb58);
    // The instance a designator below OriginatorDesignator stands for among the receivers, the originator for the others
    struct InstanceContext* InstanceOf(u32 designator) RETAIL(FUN_0020fef0);
};
CHECK_OFFSET(BehaviourRunner, tolerance, 0x10);
CHECK_OFFSET(BehaviourRunner, receivers, 0x14);
CHECK_OFFSET(BehaviourRunner, nextStarter, 0x18);
CHECK_OFFSET(BehaviourRunner, nextOriginator, 0x1C);
CHECK_OFFSET(BehaviourRunner, originator, 0x20);
CHECK_OFFSET(BehaviourRunner, previousStarter, 0x24);
CHECK_OFFSET(BehaviourRunner, joint, 0x27);
CHECK_OFFSET(BehaviourRunner, interruptLevel, 0x29);
CHECK_OFFSET(BehaviourRunner, syncUnit, 0x50);
CHECK_OFFSET(BehaviourRunner, packetEnd, 0x54);
CHECK_OFFSET(BehaviourRunner, depth, 0x28);
CHECK_OFFSET(BehaviourRunner, packetStart, 0x58);
CHECK_OFFSET(BehaviourRunner, levels, 0x2C);
CHECK_SIZE(BehaviourRunner, 0x60);

// The global agents' indexes (a byte)
constexpr u32 GlobalAgentSlots = 0x100;

extern "C"
{
    extern const GccVTableEntry g_BehaviourLevelVTable[] RETAIL(D_002FF950);
    // The body being checked and its condition (the conditions may look)
    extern StateBody* g_CheckedBody RETAIL(G_ScriptStateBody_Ptr);
    extern ScriptCondition* g_CheckedCondition RETAIL(G_ScriptCondition_Ptr);

    // A level's state's frame: the packet that ended runs the completion body, else the bodies' conditions are checked and the best
    // runs, else the else body; entering a state starts its packet. The state the level goes to (nullptr: finished)
    GraphState* ExecuteState(GraphState* state, BehaviourRunner* runner, BehaviourLevel* level, TimeClock* clock, ControlPacket* ended)
        RETAIL(ExecuteScripts_);
    // An interrupting state's bodies checked (the completion body left out): whether the best ran
    u32 ExecuteInterrupt(GraphState* state, BehaviourRunner* runner, BehaviourLevel* level, TimeClock* clock) RETAIL(ExecuteScripts2_);
    // A level's frame, and the levels above it
    void ExecuteLevel(BehaviourLevel* level, BehaviourRunner* runner, TimeClock* clock, u32 index, ControlPacket* ended)
        RETAIL(ExecuteBehaviourLevel);
    // A body's commands run: the state it jumps to (the level's time made the clock's, its restart flag set)
    GraphState* RunBody(StateBody* body, TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(RunBodyCommandsAndJump);
    // A state's control packet started: whether it has one
    u32 StartStatePacket(GraphState* state, BehaviourRunner* runner) RETAIL(ApplyStateControlPacket);
    // The graph a state's child behaviour is: a graph's ID, or a slot of the object's behaviours (a starter: its first assigner's)
    GraphData* ChildGraphOf(GraphState* state, void* object) RETAIL(GetChildScriptDataToCall);
    // The instance a call convention stands for: the agent's, one it links, a starter's receiver, the player, the originator
    struct InstanceContext* InstanceOfConvention(const CallConvention* convention, BehaviourRunner* runner) RETAIL(GetContextByCallConvention);
    // The global agents: the instances registered under their reference list index (an object instance's, when it's made), which
    // the call conventions' global agents stand for
    extern Reference* g_GlobalAgents[GlobalAgentSlots] RETAIL(G_InstanceContextRefsCounterArray);
    // An instance registered as the global agent of an index: a reference to it taken, the one before let go of
    // (game/commandsattach.cpp)
    void SetGlobalAgent(u32 index, struct InstanceContext* instance) RETAIL(FUN_00252af8);
}
