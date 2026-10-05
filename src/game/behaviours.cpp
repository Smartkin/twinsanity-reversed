#include "game/behaviours.h"

#include "game/attachments.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/player.h"
#include "game/reference.h"
#include "game/resources.h"

namespace
{
// The best score of no body
constexpr f32 NoScore = -Infinite;
// A level's destructor
constexpr u32 LevelDestroySlot = 1;

GraphState* StartPacket(GraphState* state, BehaviourRunner* runner)
{
    ControlPacket* packet = state->packet;
    if (packet != nullptr)
    {
        runner->packet = packet;
        GameNode* node = runner->agentNode;
        CallVirtual<void>(node, node->vtable, ObjectNode::PacketStartedSlot, runner);
        runner->flags.packetRuns = 1;
    }

    return state;
}

// The body of the state's (the completion body left out) that passes with the best score
StateBody* BestBody(GraphState* state, GameNode* node, BehaviourLevel* level, TimeClock* clock)
{
    f32 best = NoScore;
    StateBody* chosen = nullptr;
    g_CheckedBody = state->bits.hasCompletion != 0 ? state->bodies->next : state->bodies;
    for (; g_CheckedBody != nullptr; g_CheckedBody = g_CheckedBody->next)
    {
        ScriptCondition* condition = g_CheckedBody->condition;
        g_CheckedCondition = condition;
        f32 result = CallVirtual<f32>(condition, condition->vtable, ScriptCondition::CheckSlot, node, level, &clock->time);
        condition = g_CheckedCondition;
        f32 threshold = condition->threshold;
        bool inverted = condition->bits.inverted != 0;
        if (inverted ? !(result < threshold) : !(threshold < result))
        {
            continue;
        }

        f32 score = (inverted ? threshold - result : result - threshold) * condition->weight;
        if (best < score)
        {
            chosen = g_CheckedBody;
            best = score;
        }
    }

    return chosen;
}
}

BehaviourLevel* BehaviourLevel::Construct(BehaviourLevel* level)
{
    level->bits.finished = 0;
    level->bits.restart = 0;
    level->vtable = g_BehaviourLevelVTable;
    level->bits.message = NoMessage;
    level->graph = nullptr;
    level->elseBody = nullptr;
    level->state = nullptr;
    level->entered = nullptr;
    level->time = 0;
    level->bits.index = 0;
    return level;
}

void BehaviourLevel::Destroy(u32 destroyFlags)
{
    vtable = g_BehaviourLevelVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void BehaviourLevel::Start(GraphData* started, TimeClock* clock)
{
    graph = started;
    state = started != nullptr ? started->start : nullptr;
    entered = nullptr;
    bits.finished = 0;
    bits.restart = 0;
    time = clock->time;
    child = nullptr;
    elseBody = nullptr;
    bits.index = 0;
}

BehaviourRunner* BehaviourRunner::Construct(BehaviourRunner* runner, GameNode* agentNode, u32 slot)
{
    runner->agentNode = agentNode;
    runner->previousStarter = NoScriptId;
    runner->packet = nullptr;
    runner->interruptLevel = NoLevel;
    BehaviourRunnerFlags flags = runner->flags;
    flags.unused0 = 0;
    flags.runLevels = 0;
    flags.unused2 = 0;
    flags.packetEnded = 0;
    flags.packetWaiting = 0;
    flags.packetRuns = 0;
    flags.slot = slot;
    flags.interruptFromLevel = 0;
    runner->flags = flags;
    runner->lastPacket = nullptr;
    runner->tolerance = 0.0f;
    runner->receivers = nullptr;
    runner->nextStarter = nullptr;
    runner->nextOriginator = nullptr;
    runner->originator = nullptr;
    runner->unused26 = 0;
    runner->depth = 0;
    runner->unused4C = 0;
    runner->syncUnit = 0;
    runner->packetEnd = 0;
    runner->packetStart = 0;
    runner->markedTime = 0;

    for (u32 index = 1; index < Levels; index++)
    {
        runner->levels[index] = nullptr;
    }

    runner->levels[0] = BehaviourLevel::Construct(static_cast<BehaviourLevel*>(MemoryAllocate(sizeof(BehaviourLevel))));
    return runner;
}

GraphState* RunBody(StateBody* body, TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level)
{
    level->elseBody = nullptr;
    for (ScriptCommand* command = body->commands; command != nullptr; command = command->next)
    {
        CallVirtual<void>(command, command->vtable, ScriptCommand::ExecuteSlot, clock, runner, level);
    }

    if (body->bits.restarts != 0)
    {
        level->bits.restart = 1;
    }

    level->time = clock->time;
    return body->jump;
}

u32 StartStatePacket(GraphState* state, BehaviourRunner* runner)
{
    if (state->packet == nullptr)
    {
        return 0;
    }

    StartPacket(state, runner);
    return 1;
}

GraphData* ChildGraphOf(GraphState* state, void* object)
{
    if (state->bits.childIsSlot != 0)
    {
        u16 id;
        GetObjectBehaviourId(&id, static_cast<GameObject*>(object), state->bits.child);
        auto* starter = id != NoScriptId ? static_cast<ScriptStarter*>(g_ScriptTable->items[id & ResourceIndexMask]) : nullptr;
        return starter != nullptr ? starter->assigners[0]->graph : nullptr;
    }

    u16 id = state->bits.child;
    auto* graph = id != NoScriptId ? static_cast<ScriptGraph*>(g_ScriptTable->items[id & ResourceIndexMask]) : nullptr;
    return graph->data;
}

void BehaviourRunner::Unwind(BehaviourLevel* level)
{
    u8 index = level->bits.index;
    for (s32 above = index + 1; above < depth; above++)
    {
        levels[above]->bits.finished = 1;
        levels[above]->state = nullptr;
    }

    depth = index;
}

void BehaviourRunner::EnterChild(u32 index, GraphState* state)
{
    u8 at = static_cast<u8>(index);
    BehaviourLevel*& slot = levels[at];
    if (slot == nullptr)
    {
        slot = BehaviourLevel::Construct(static_cast<BehaviourLevel*>(MemoryAllocate(sizeof(BehaviourLevel))));
    }

    BehaviourLevel* parent = levels[at - 1];
    BehaviourLevel* level = slot;
    GameNode* node = agentNode;
    auto* objectNode = static_cast<ObjectNodeBase*>(node);
    GameNode* source = objectNode->sourceNode;
    void* object = source != nullptr ? SourceObject(source) : objectNode->object;
    GraphData* graph = ChildGraphOf(state, object);
    level->Start(graph, GetContextClock(agentNode->owner));
    level->bits.index = at;
    level->time = parent->time;
    parent->child = graph;
    depth = at;
}

void BehaviourRunner::EndPacket()
{
    flags.packetWaiting = 0;
    ControlPacket* ended = packet;
    packet = nullptr;
    lastPacket = ended;
    GameNode* node = agentNode;
    if (CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        return;
    }

    node = agentNode;
    ControlPacket* last = lastPacket;
    if (last == nullptr)
    {
        return;
    }

    MotionState* motion = static_cast<ObjectNode*>(node)->motion;
    if (motion == nullptr || last->settings.motion == ControlPacket::NoMotion)
    {
        return;
    }

    // The other slot's runner's packet: when it has tracks too the animation keeps them
    u32 other = 1 - flags.slot;
    BehaviourRunner* runner = static_cast<ObjectNode*>(node)->runners[other];
    if (runner != nullptr)
    {
        ControlPacket* playing = runner->packet;
        if (playing != nullptr && playing->settings.motion != ControlPacket::NoMotion)
        {
            return;
        }
    }

    motion->bits.translationDone = 1;
    motion->bits.rotationDone = 1;
}

GraphState* ExecuteState(GraphState* state, BehaviourRunner* runner, BehaviourLevel* level, TimeClock* clock, ControlPacket* ended)
{
    GraphStateBits bits = state->bits;
    ControlPacket* packet = state->packet;
    GameNode* node = runner->agentNode;
    u32 completion = bits.hasCompletion;
    g_CheckedBody = nullptr;
    g_CheckedCondition = nullptr;
    if (packet != nullptr && ended == packet)
    {
        // The state's packet ended: the completion body runs
        if (bits.hasCompletion == 0)
        {
            return nullptr;
        }

        level->entered = state;
        GraphState* next = RunBody(state->bodies, clock, runner, level);
        return StartPacket(next, runner);
    }

    if (completion < state->bits.bodyCount)
    {
        StateBody* best = BestBody(state, node, level, clock);
        if (best != nullptr)
        {
            if (state->bits.child != NoScriptId)
            {
                runner->Unwind(level);
            }
            else
            {
                runner->EndPacket();
            }

            GraphState* next = RunBody(best, clock, runner, level);
            return next != nullptr ? StartPacket(next, runner) : nullptr;
        }

        StateBody* fallback = level->elseBody;
        if (fallback != nullptr)
        {
            GraphState* next = RunBody(fallback, clock, runner, level);
            return next != nullptr ? StartPacket(next, runner) : nullptr;
        }
    }

    // A first entry (no state entered, or another one) restarts when the completion body comes back to the state
    GraphState* entered = level->entered;
    if (entered != nullptr && level->state == entered)
    {
        return state;
    }

    GraphState* completionJump = state->bits.hasCompletion != 0 ? state->bodies->jump : nullptr;
    level->bits.restart = state == completionJump;
    if (level->bits.finished != 0)
    {
        return nullptr;
    }

    if (state->packet != nullptr)
    {
        return StartPacket(state, runner);
    }

    runner->packet = nullptr;
    CallVirtual<void>(node, node->vtable, ObjectNode::PacketEndedSlot, runner);
    if (state->bits.child != NoScriptId || state->bits.interrupting != 0)
    {
        return state;
    }

    if (state->bits.hasCompletion == 0)
    {
        return nullptr;
    }

    GraphState* next = RunBody(state->bodies, clock, runner, level);
    if (next != nullptr)
    {
        StartPacket(next, runner);
    }

    level->entered = level->state;
    return next;
}

u32 ExecuteInterrupt(GraphState* state, BehaviourRunner* runner, BehaviourLevel* level, TimeClock* clock)
{
    // No condition checked yet, even when the state has no bodies to check (ExecuteState clears it before)
    g_CheckedCondition = nullptr;
    StateBody* best = BestBody(state, runner->agentNode, level, clock);
    if (best == nullptr)
    {
        return 0;
    }

    if (state->bits.child != NoScriptId)
    {
        runner->Unwind(level);
    }

    runner->EndPacket();
    GraphState* next = RunBody(best, clock, runner, level);
    StartPacket(next, runner);
    level->state = next;
    level->bits.restart = 1;
    level->entered = state != next ? state : nullptr;
    return 1;
}

void ExecuteLevel(BehaviourLevel* level, BehaviourRunner* runner, TimeClock* clock, u32 index, ControlPacket* ended)
{
    u8 at = static_cast<u8>(index);
    u8 interrupt = runner->interruptLevel;
    if (interrupt != BehaviourRunner::NoLevel && at < interrupt)
    {
        // A level above interrupted: it's the one to run
        GraphState* state = level->state;
        if (state == nullptr || state->bits.child == NoScriptId)
        {
            return;
        }

        u8 next = at + 1;
        ExecuteLevel(runner->levels[next], runner, clock, next, ended);
        return;
    }

    if (level->bits.finished != 0)
    {
        return;
    }

    GraphState* state = ExecuteState(level->state, runner, level, clock, ended);
    bool finishes;
    if (state == nullptr)
    {
        finishes = true;
    }
    else if (state->bits.child != NoScriptId || state->packet != nullptr)
    {
        finishes = false;
    }
    else
    {
        finishes = state->bits.bodyCount == 0;
        if (finishes)
        {
            state = nullptr;
        }
    }

    if (finishes)
    {
        level->bits.finished = 1;
        runner->depth = at - 1;
    }

    if (level->bits.finished == 0 && state->bits.child != NoScriptId)
    {
        u8 next = at + 1;
        if (state != level->entered)
        {
            runner->EnterChild(next, state);
            level->state = state;
        }

        BehaviourLevel* child = runner->levels[next];
        ExecuteLevel(child, runner, clock, next, ended);
        if (child->bits.finished != 0)
        {
            // The child behaviour finished: the completion body runs
            state = state->bodies != nullptr ? RunBody(state->bodies, clock, runner, level) : nullptr;
            if (state == nullptr)
            {
                level->bits.finished = 1;
                runner->depth = at - 1;
            }
            else
            {
                StartStatePacket(state, runner);
            }
        }
    }

    GraphState* entered = level->bits.restart != 0 ? nullptr : level->state;
    level->state = state;
    level->entered = entered;
}

u32 BehaviourRunner::CheckInterrupts(TimeClock* clock)
{
    s32 index = flags.interruptFromLevel != 0 ? interruptLevel : 0;
    for (;;)
    {
        BehaviourLevel* level = levels[index];
        GraphState* state = level->state;
        u32 switched = 0;
        if (state != nullptr && state->bits.interrupting != 0)
        {
            switched = ExecuteInterrupt(state, this, level, clock);
        }

        index++;
        if (depth < index || switched != 0)
        {
            return switched;
        }
    }
}

u32 BehaviourRunner::RunLevels(TimeClock* clock, ControlPacket* ended)
{
    ExecuteLevel(levels[0], this, clock, 0, ended);
    if (levels[0]->bits.finished == 0)
    {
        return 0;
    }

    Stop(1);
    return 1;
}

InstanceContext* InstanceOfConvention(const CallConvention* convention, BehaviourRunner* runner)
{
    GameNode* node = runner->agentNode;
    u16 argument = convention->bits.argument;
    switch (convention->bits.assignee)
    {
    case AssignMe:
        return node->owner;
    case AssignLinkedObject:
    {
        if (argument == CallConvention::NoArgument)
        {
            return nullptr;
        }

        auto* links = static_cast<AttachmentsNode*>(GetGameNode(&node->owner->nodes, NodeAttachments));
        return links != nullptr ? links->linked[argument] : nullptr;
    }
    case AssignGlobalAgent:
    {
        if (argument == CallConvention::NoArgument)
        {
            return nullptr;
        }

        Reference* agent = g_GlobalAgents[static_cast<u8>(argument)];
        return agent != nullptr ? static_cast<InstanceContext*>(agent->object) : nullptr;
    }
    case AssignHumanPlayer:
        return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    case AssignOriginator:
        return static_cast<InstanceContext*>(runner->originator);
    default:
        return nullptr;
    }
}

void BehaviourRunner::Stop(u32 release)
{
    if (receivers != nullptr)
    {
        receivers->bits.users--;
        if (release != 0)
        {
            StarterReceivers* used = receivers;
            if (used->bits.users == 0 && used != nullptr)
            {
                used->Destroy(DestroyAndFree);
            }
        }
    }

    levels[0]->graph = nullptr;
    receivers = nullptr;
    flags.packetEnded = 0;
    flags.packetWaiting = 0;
    flags.packetRuns = 0;
    flags.interruptFromLevel = 0;
    interruptLevel = NoLevel;
    lastPacket = packet;
    packet = nullptr;
    packetStart = 0;
    packetEnd = 0;
    markedTime = 0;
    depth = 0;
}

u32 BehaviourRunner::StepPacket(TimeClock* clock)
{
    auto* node = static_cast<ObjectNode*>(agentNode);
    if (node->trajectory != nullptr)
    {
        HoldTrajectory(node->trajectory, node);
    }

    StartPacketMotion(this, clock);
    return 1;
}

u32 BehaviourRunner::Update(TimeClock* clock)
{
    if (nextStarter != nullptr)
    {
        TakeStarter();
    }

    if (receivers == nullptr)
    {
        return 0;
    }

    auto* node = static_cast<ObjectNode*>(agentNode);
    if (CheckInterrupts(clock) == 0)
    {
        if (packet != nullptr && flags.packetWaiting != 0)
        {
            PacketFrame(this, clock);
            if (node->trajectory != nullptr)
            {
                TrajectoryFrame(node->trajectory, node);
            }

            if (flags.packetEnded == 0)
            {
                return 0;
            }

            flags.packetRuns = 0;
            flags.packetEnded = 0;
            flags.packetWaiting = 0;
            return RunLevels(clock, lastPacket) != 0 ? 1 : 0;
        }

        u32 runLevels = flags.runLevels;
        flags.packetRuns = 0;
        if (runLevels != 0 && RunLevels(clock, nullptr) != 0)
        {
            return 1;
        }

        if (flags.packetRuns == 0)
        {
            if (node->trajectory != nullptr)
            {
                TrajectoryFrame(node->trajectory, node);
            }

            return 0;
        }
    }

    if (packet != nullptr)
    {
        StepPacket(clock);
    }

    if (node->trajectory != nullptr)
    {
        TrajectoryFrame(node->trajectory, node);
    }

    return 0;
}

StarterReceivers* StarterReceivers::Construct(StarterReceivers* receivers, ScriptStarter* starter)
{
    receivers->Reset(starter);
    receivers->originator = nullptr;
    return receivers;
}

void StarterReceivers::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void StarterReceivers::Reset(ScriptStarter* made)
{
    starter = made;
    bits.priority = made->bits.priority;
    bits.restartable = 0;
    bits.assignerCount = made->assignerCount;
}

void StarterReceivers::ReleaseAll(BehaviourRunner* runner)
{
    for (u8 index = 0; index < bits.assignerCount; index++)
    {
        if ((bits.running & 1 << index) == 0)
        {
            continue;
        }

        auto* node = static_cast<GameNode*>(GetGameNode(&instances[index]->nodes, NodeObject));
        runner->Stop(0);
        CallVirtual<void>(node, node->vtable, ObjectNode::RunnerFinishedSlot);
    }

    bits.running = 0;
    bits.users = 0;
}

void StarterReceivers::Resolve(BehaviourRunner* runner)
{
    bits.users = 0;
    bits.running = 0;
    ScriptStarter* made = starter;
    instances[0] = runner->agentNode->owner;
    originator = runner->originator;
    GraphData* graph = made->assigners[0]->graph;
    u32 slot = runner->flags.slot;
    if (graph != nullptr)
    {
        bits.running = 1;
        bits.users = 1;
        runner->Start(this, graph);
    }

    for (u8 index = 1; index < bits.assignerCount; index++)
    {
        InstanceContext* instance = InstanceOfConvention(starter->assigners[index]->convention, runner);
        instances[index] = instance;
        GraphData* assigned = starter->assigners[index]->graph;
        if (assigned == nullptr)
        {
            continue;
        }

        auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, NodeObject));
        static_cast<ObjectNodeBase*>(node)->runners[slot]->Start(this, assigned);
        bits.running |= 1 << index;
        bits.users++;
    }
}

void BehaviourRunner::Start(StarterReceivers* started, GraphData* graph)
{
    StarterReceivers* old = receivers;
    if (old != nullptr)
    {
        ScriptStarter* starter = old->starter;
        previousStarter = CallVirtual<u16>(starter, starter->vtable, ScriptResource::IdSlot);
        old = receivers;
        if (started != old && old != nullptr)
        {
            old->Destroy(DestroyAndFree);
        }
    }
    else
    {
        previousStarter = NoScriptId;
    }

    receivers = started;
    for (u32 index = 1; index < Levels; index++)
    {
        if (levels[index] != nullptr)
        {
            levels[index]->Start(nullptr, GetContextClock(agentNode->owner));
        }
    }

    flags.packetEnded = 0;
    flags.packetWaiting = 0;
    flags.packetRuns = 0;
    flags.interruptFromLevel = 0;
    lastPacket = nullptr;
    packet = nullptr;
    packetEnd = 0;
    syncUnit = 0;
    unused4C = 0;
    packetStart = 0;
    nextStarter = nullptr;
    originator = started->originator;
    levels[0]->Start(graph, GetContextClock(agentNode->owner));
    interruptLevel = NoLevel;
    depth = 0;
    flags.runLevels = 1;
}

void BehaviourRunner::TakeStarter()
{
    StarterReceivers* taken = receivers;
    if (taken != nullptr)
    {
        taken->ReleaseAll(this);
        receivers = taken;
        taken->Reset(nextStarter);
    }
    else
    {
        receivers = StarterReceivers::Construct(static_cast<StarterReceivers*>(MemoryAllocate(sizeof(StarterReceivers))), nextStarter);
    }

    flags.interruptFromLevel = 0;
    originator = nextOriginator;
    interruptLevel = NoLevel;
    receivers->Resolve(this);
    markedTime = 0;
    nextStarter = nullptr;
    nextOriginator = nullptr;
}

void BehaviourRunner::Destroy(u32 destroyFlags)
{
    DestroyLevels();
    if (receivers != nullptr)
    {
        receivers->Destroy(DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void BehaviourRunner::DestroyLevels()
{
    for (BehaviourLevel*& level : levels)
    {
        if (level != nullptr)
        {
            CallVirtual<void>(level, level->vtable, LevelDestroySlot, DestroyAndFree);
        }

        level = nullptr;
    }
}

u32 BehaviourRunner::QueueStarter(ScriptStarter* starter, void* originator, u32 force)
{
    if (force == 0)
    {
        u32 priority = starter->bits.priority;
        if (nextStarter != nullptr && priority < nextStarter->bits.priority)
        {
            return 0;
        }

        if (receivers != nullptr)
        {
            u32 running = receivers->bits.priority;
            u32 takes;
            if (running < priority)
            {
                takes = 1;
            }
            else if (priority != running)
            {
                takes = 0;
            }
            else if (starter != receivers->starter)
            {
                takes = 1;
            }
            else
            {
                takes = receivers->bits.restartable;
            }

            if (takes == 0)
            {
                return 0;
            }
        }
    }

    nextOriginator = originator;
    nextStarter = starter;
    return 1;
}

InstanceContext* BehaviourRunner::InstanceOf(u32 designator)
{
    if (designator >= OriginatorDesignator)
    {
        return static_cast<InstanceContext*>(originator);
    }

    return receivers->instances[static_cast<u8>(designator)];
}
