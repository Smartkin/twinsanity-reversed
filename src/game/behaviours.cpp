#include "game/behaviours.h"

#include "game/clock.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/player.h"
#include "game/reference.h"
#include "game/resources.h"

extern "C"
{
    // The instances the starters' receivers' indexes stand for
    extern Reference* g_ReceiverInstances[256] RETAIL(G_InstanceContextRefsCounterArray);
    // The object an instance is an instance of (still asm)
    void* InstanceObject(void* instance) RETAIL(GetGameObjectAddress_FromInstance_);
}

namespace
{
// The best score of no body
constexpr f32 NoScore = -0x1.93e594p+99f;
// A condition's bit 16: the body passes below the threshold
constexpr u32 ConditionInverted = 0x10000;
// The state bits: the completion body (the first, run when the packet or child behaviour ends), and the interrupting states;
// the child behaviour in the high half (0xFFFF none), a slot of the object's behaviours when bit 12 is set
constexpr u32 StateCompletion = 0x400;
constexpr u32 StateInterrupting = 0x800;
constexpr u32 StateChildSlot = 0x1000;
constexpr u16 NoChild = 0xFFFF;
// A body's bit 8: it restarts the state it jumps to
constexpr u32 BodyRestarts = 0x100;
// A command's vtable function running it, a condition's checking it
constexpr u32 ExecuteSlot = 3;
constexpr u32 CheckSlot = 2;
// The agent node's vtable functions: whether it takes packets, no packet, a packet started
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 NoPacketSlot = 42;
constexpr u32 PacketStartedSlot = 43;
// A link's node keeps the instances it links 0x20 bytes in
constexpr u32 LinksNodeKind = 6;
constexpr u32 LinksNodeInstances = 0x20;

u16 ChildOf(const GraphState* state)
{
    return static_cast<u16>(state->bits >> 16);
}

GraphState* StartPacket(GraphState* state, BehaviourRunner* runner)
{
    ControlPacket* packet = state->packet;
    if (packet != nullptr)
    {
        runner->packet = packet;
        GameNode* node = runner->agentNode;
        CallVirtual<void>(node, node->vtable, PacketStartedSlot, runner);
        runner->flags |= BehaviourRunner::FlagPacketRuns;
    }

    return state;
}

// The body of the state's (the completion body left out) that passes with the best score
StateBody* BestBody(GraphState* state, GameNode* node, BehaviourLevel* level, TimeClock* clock)
{
    f32 best = NoScore;
    StateBody* chosen = nullptr;
    g_CheckedBody = (state->bits & StateCompletion) != 0 ? state->bodies->next : state->bodies;
    for (; g_CheckedBody != nullptr; g_CheckedBody = g_CheckedBody->next)
    {
        ScriptCondition* condition = g_CheckedBody->condition;
        g_CheckedCondition = condition;
        f32 result = CallVirtual<f32>(condition, condition->vtable, CheckSlot, node, level, &clock->time);
        condition = g_CheckedCondition;
        f32 threshold = condition->values[1];
        bool inverted = (condition->bits & ConditionInverted) != 0;
        if (inverted ? !(result < threshold) : !(threshold < result))
        {
            continue;
        }

        f32 score = (inverted ? threshold - result : result - threshold) * condition->values[2];
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
    level->bits &= ~Finished;
    level->bits &= ~Restart;
    level->vtable = g_BehaviourLevelVTable;
    reinterpret_cast<u16*>(&level->bits)[1] = 0xFFFF;
    level->graph = nullptr;
    level->pendingBody = nullptr;
    level->state = nullptr;
    level->entered = nullptr;
    level->time = 0;
    reinterpret_cast<u8*>(&level->bits)[0] = 0;
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

void BehaviourLevel::Start(GraphData* data, TimeClock* clock)
{
    graph = data;
    state = data != nullptr ? data->start : nullptr;
    entered = nullptr;
    bits &= ~Finished;
    bits &= ~Restart;
    time = clock->time;
    child = nullptr;
    pendingBody = nullptr;
    reinterpret_cast<u8*>(&bits)[0] = 0;
}

BehaviourRunner* BehaviourRunner::Construct(BehaviourRunner* runner, GameNode* agentNode, u32 slot)
{
    constexpr u32 ClearedFlags = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | SlotMask << SlotShift | FlagInterruptFromLevel;
    runner->agentNode = agentNode;
    runner->unknown24 = 0xFFFF;
    runner->packet = nullptr;
    runner->interruptLevel = 0xFF;
    runner->flags = (runner->flags & ~ClearedFlags) | (slot & SlotMask) << SlotShift;
    runner->lastPacket = nullptr;
    runner->tolerance = 0.0f;
    runner->receivers = nullptr;
    runner->nextStarter = nullptr;
    runner->nextOriginator = nullptr;
    runner->originator = nullptr;
    runner->unknown26 = 0;
    runner->depth = 0;
    runner->unknown4C = 0;
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
    level->pendingBody = nullptr;
    for (ScriptCommand* command = body->commands; command != nullptr; command = command->next)
    {
        CallVirtual<void>(command, command->vtable, ExecuteSlot, clock, runner, level);
    }

    if ((body->bits & BodyRestarts) != 0)
    {
        level->bits |= BehaviourLevel::Restart;
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
    if ((state->bits & StateChildSlot) != 0)
    {
        u16 id;
        GetObjectBehaviourId(&id, static_cast<GameObject*>(object), ChildOf(state));
        auto* starter = id != 0xFFFF ? static_cast<ScriptStarter*>(g_ScriptTable->items[id & 0x7FFF]) : nullptr;
        return starter != nullptr ? starter->assigners[0]->graph : nullptr;
    }

    u16 id = ChildOf(state);
    auto* graph = id != 0xFFFF ? static_cast<ScriptGraph*>(g_ScriptTable->items[id & 0x7FFF]) : nullptr;
    return graph->data;
}

void BehaviourRunner::Unwind(BehaviourLevel* level)
{
    u8 index = static_cast<u8>(level->bits);
    for (s32 above = index + 1; above < depth; above++)
    {
        BehaviourLevel* stacked = levels[above];
        stacked->bits = (stacked->bits & ~BehaviourLevel::Finished) | BehaviourLevel::Finished;
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
    void* object = source != nullptr ? InstanceObject(source) : objectNode->object;
    GraphData* graph = ChildGraphOf(state, object);
    level->Start(graph, GetContextClock(agentNode->owner));
    reinterpret_cast<u8*>(&level->bits)[0] = at;
    level->time = parent->time;
    parent->child = graph;
    depth = at;
}

void BehaviourRunner::EndPacket()
{
    flags &= ~FlagPacketWaiting;
    ControlPacket* ended = packet;
    packet = nullptr;
    lastPacket = ended;
    GameNode* node = agentNode;
    if (CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) == 0)
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
    if (motion == nullptr || last->MotionKind() == 0)
    {
        return;
    }

    // The other slot's runner's packet: when it has tracks too the animation keeps them
    u32 other = 1 - (flags >> SlotShift & SlotMask);
    BehaviourRunner* runner = static_cast<ObjectNode*>(node)->runners[other];
    if (runner != nullptr)
    {
        ControlPacket* playing = runner->packet;
        if (playing != nullptr && playing->MotionKind() != 0)
        {
            return;
        }
    }

    motion->bits |= MotionState::TranslationDone | MotionState::RotationDone;
}

GraphState* ExecuteState(GraphState* state, BehaviourRunner* runner, BehaviourLevel* level, TimeClock* clock, ControlPacket* ended)
{
    u32 bits = state->bits;
    ControlPacket* packet = state->packet;
    GameNode* node = runner->agentNode;
    u32 completion = bits >> 10 & 1;
    g_CheckedBody = nullptr;
    g_CheckedCondition = nullptr;
    if (packet != nullptr && ended == packet)
    {
        // The state's packet ended: the completion body runs
        if ((bits & StateCompletion) == 0)
        {
            return nullptr;
        }

        level->entered = state;
        GraphState* next = RunBody(state->bodies, clock, runner, level);
        return StartPacket(next, runner);
    }

    if (completion < (state->bits & GraphState::BodyMask))
    {
        StateBody* best = BestBody(state, node, level, clock);
        if (best != nullptr)
        {
            if (ChildOf(state) != NoChild)
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

        StateBody* pending = level->pendingBody;
        if (pending != nullptr)
        {
            GraphState* next = RunBody(pending, clock, runner, level);
            return next != nullptr ? StartPacket(next, runner) : nullptr;
        }
    }

    // A first entry (no state entered, or another one) restarts when the completion body comes back to the state
    GraphState* entered = level->entered;
    if (entered != nullptr && level->state == entered)
    {
        return state;
    }

    GraphState* completionJump = (state->bits & StateCompletion) != 0 ? state->bodies->jump : nullptr;
    level->bits = (level->bits & ~BehaviourLevel::Restart) | (state == completionJump ? BehaviourLevel::Restart : 0);
    if ((level->bits & BehaviourLevel::Finished) != 0)
    {
        return nullptr;
    }

    if (state->packet != nullptr)
    {
        return StartPacket(state, runner);
    }

    runner->packet = nullptr;
    CallVirtual<void>(node, node->vtable, NoPacketSlot, runner);
    if ((state->bits & 0xFFFF0800) != 0xFFFF0000)
    {
        return state;
    }

    if ((state->bits & StateCompletion) == 0)
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
    StateBody* best = BestBody(state, runner->agentNode, level, clock);
    if (best == nullptr)
    {
        return 0;
    }

    if (ChildOf(state) != NoChild)
    {
        runner->Unwind(level);
    }

    runner->EndPacket();
    GraphState* next = RunBody(best, clock, runner, level);
    StartPacket(next, runner);
    level->state = next;
    level->bits |= BehaviourLevel::Restart;
    level->entered = state != next ? state : nullptr;
    return 1;
}

void ExecuteLevel(BehaviourLevel* level, BehaviourRunner* runner, TimeClock* clock, u32 index, ControlPacket* ended)
{
    u8 at = static_cast<u8>(index);
    u8 interrupt = runner->interruptLevel;
    if (interrupt != 0xFF && at < interrupt)
    {
        // A level above interrupted: it's the one to run
        GraphState* state = level->state;
        if (state == nullptr || ChildOf(state) == NoChild)
        {
            return;
        }

        u8 next = at + 1;
        ExecuteLevel(runner->levels[next], runner, clock, next, ended);
        return;
    }

    if ((level->bits & BehaviourLevel::Finished) != 0)
    {
        return;
    }

    GraphState* state = ExecuteState(level->state, runner, level, clock, ended);
    bool finishes;
    if (state == nullptr)
    {
        finishes = true;
    }
    else if (ChildOf(state) != NoChild || state->packet != nullptr)
    {
        finishes = false;
    }
    else
    {
        finishes = (state->bits & GraphState::BodyMask) == 0;
        if (finishes)
        {
            state = nullptr;
        }
    }

    if (finishes)
    {
        level->bits |= BehaviourLevel::Finished;
        runner->depth = at - 1;
    }

    if ((level->bits & BehaviourLevel::Finished) == 0 && ChildOf(state) != NoChild)
    {
        u8 next = at + 1;
        if (state != level->entered)
        {
            runner->EnterChild(next, state);
            level->state = state;
        }

        BehaviourLevel* child = runner->levels[next];
        ExecuteLevel(child, runner, clock, next, ended);
        if ((child->bits & BehaviourLevel::Finished) != 0)
        {
            // The child behaviour finished: the completion body runs
            state = state->bodies != nullptr ? RunBody(state->bodies, clock, runner, level) : nullptr;
            if (state == nullptr)
            {
                level->bits |= BehaviourLevel::Finished;
                runner->depth = at - 1;
            }
            else
            {
                StartStatePacket(state, runner);
            }
        }
    }

    GraphState* entered = (level->bits & BehaviourLevel::Restart) != 0 ? nullptr : level->state;
    level->state = state;
    level->entered = entered;
}

u32 BehaviourRunner::CheckInterrupts(TimeClock* clock)
{
    s32 index = (flags & FlagInterruptFromLevel) != 0 ? interruptLevel : 0;
    for (;;)
    {
        BehaviourLevel* level = levels[index];
        GraphState* state = level->state;
        u32 switched = 0;
        if (state != nullptr && (state->bits >> 11 & 1) != 0)
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
    if ((levels[0]->bits & BehaviourLevel::Finished) == 0)
    {
        return 0;
    }

    Stop(1);
    return 1;
}

InstanceContext* InstanceOfConvention(const CallConvention* convention, BehaviourRunner* runner)
{
    GameNode* node = runner->agentNode;
    u16 argument = static_cast<u16>(convention->bits >> 16);
    switch (convention->bits & 0xF)
    {
    case 0:
        return node->owner;
    case 2:
    {
        if (argument == 0xFFFF)
        {
            return nullptr;
        }

        auto* links = static_cast<u8*>(GetGameNode(&node->owner->nodes, LinksNodeKind));
        return links != nullptr ? reinterpret_cast<InstanceContext**>(links + LinksNodeInstances)[argument] : nullptr;
    }
    case 3:
    {
        if (argument == 0xFFFF)
        {
            return nullptr;
        }

        Reference* receiver = g_ReceiverInstances[static_cast<u8>(argument)];
        return receiver != nullptr ? static_cast<InstanceContext*>(receiver->object) : nullptr;
    }
    case 4:
        return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    case 8:
        return static_cast<InstanceContext*>(runner->originator);
    default:
        return nullptr;
    }
}

void BehaviourRunner::Stop(u32 release)
{
    constexpr u32 UsersShift = StarterReceivers::UsersShift;
    constexpr u32 UsersMask = StarterReceivers::FieldMask;
    if (receivers != nullptr)
    {
        u32& users = receivers->bits;
        users = (users & ~(UsersMask << UsersShift)) | ((users >> UsersShift & UsersMask) - 1 & UsersMask) << UsersShift;
        if (release != 0)
        {
            StarterReceivers* used = receivers;
            if ((used->bits & UsersMask << UsersShift) == 0 && used != nullptr)
            {
                used->Destroy(DestroyAndFree);
            }
        }
    }

    levels[0]->graph = nullptr;
    receivers = nullptr;
    flags &= ~FlagPacketEnded & ~FlagPacketWaiting & ~FlagPacketRuns & ~FlagInterruptFromLevel;
    interruptLevel = 0xFF;
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
        if (packet != nullptr && (flags & FlagPacketWaiting) != 0)
        {
            PacketFrame(this, clock);
            if (node->trajectory != nullptr)
            {
                TrajectoryFrame(node->trajectory, node);
            }

            if ((flags & FlagPacketEnded) == 0)
            {
                return 0;
            }

            flags &= ~FlagPacketRuns & ~FlagPacketEnded & ~FlagPacketWaiting;
            return RunLevels(clock, lastPacket) != 0 ? 1 : 0;
        }

        u32 runLevels = flags & FlagRunLevels;
        flags &= ~FlagPacketRuns;
        if (runLevels != 0 && RunLevels(clock, nullptr) != 0)
        {
            return 1;
        }

        if ((flags & FlagPacketRuns) == 0)
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
    u32 priority = made->bits >> ScriptResource::PriorityShift & PriorityMask;
    bits = (bits & ~PriorityMask) | priority;
    bits &= ~Restartable;
    bits = (bits & ~(FieldMask << AssignersShift)) | (made->assignerCount & FieldMask) << AssignersShift;
}

void StarterReceivers::ReleaseAll(BehaviourRunner* runner)
{
    // The agent node's vtable function told when its runner lets go
    constexpr u32 ReleasedSlot = 22;
    u8& running = reinterpret_cast<u8*>(&bits)[2];
    for (u8 index = 0; index < (bits >> AssignersShift & FieldMask); index++)
    {
        if ((running & 1 << index) == 0)
        {
            continue;
        }

        auto* node = static_cast<GameNode*>(GetGameNode(&instances[index]->nodes, 1));
        runner->Stop(0);
        CallVirtual<void>(node, node->vtable, ReleasedSlot);
    }

    running = 0;
    bits &= ~(FieldMask << UsersShift);
}

void StarterReceivers::Resolve(BehaviourRunner* runner)
{
    constexpr u32 UserBits = FieldMask << UsersShift;
    u8& running = reinterpret_cast<u8*>(&bits)[2];
    bits &= ~UserBits;
    running = 0;
    ScriptStarter* made = starter;
    instances[0] = runner->agentNode->owner;
    originator = runner->originator;
    GraphData* graph = made->assigners[0]->graph;
    u32 slot = runner->flags >> BehaviourRunner::SlotShift & BehaviourRunner::SlotMask;
    if (graph != nullptr)
    {
        running = 1;
        bits = (bits & ~UserBits) | 1 << UsersShift;
        runner->Start(this, graph);
    }

    for (u8 index = 1; index < (bits >> AssignersShift & FieldMask); index++)
    {
        InstanceContext* instance = InstanceOfConvention(starter->assigners[index]->convention, runner);
        instances[index] = instance;
        GraphData* assigned = starter->assigners[index]->graph;
        if (assigned == nullptr)
        {
            continue;
        }

        auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, 1));
        static_cast<ObjectNodeBase*>(node)->runners[slot]->Start(this, assigned);
        running |= 1 << index;
        u32 users = (bits >> UsersShift & FieldMask) + 1;
        bits = (bits & ~UserBits) | (users & FieldMask) << UsersShift;
    }
}

void BehaviourRunner::Start(StarterReceivers* started, GraphData* graph)
{
    StarterReceivers* old = receivers;
    if (old != nullptr)
    {
        // The starter's ID of the receivers it had
        ScriptStarter* starter = old->starter;
        unknown24 = CallVirtual<u16>(starter, starter->vtable, 2);
        old = receivers;
        if (started != old && old != nullptr)
        {
            old->Destroy(DestroyAndFree);
        }
    }
    else
    {
        unknown24 = 0xFFFF;
    }

    receivers = started;
    for (u32 index = 1; index < Levels; index++)
    {
        if (levels[index] != nullptr)
        {
            levels[index]->Start(nullptr, GetContextClock(agentNode->owner));
        }
    }

    flags &= ~FlagPacketEnded & ~FlagPacketWaiting & ~FlagPacketRuns & ~FlagInterruptFromLevel;
    lastPacket = nullptr;
    packet = nullptr;
    packetEnd = 0;
    syncUnit = 0;
    unknown4C = 0;
    packetStart = 0;
    nextStarter = nullptr;
    originator = started->originator;
    levels[0]->Start(graph, GetContextClock(agentNode->owner));
    interruptLevel = 0xFF;
    depth = 0;
    flags |= FlagRunLevels;
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

    flags &= ~FlagInterruptFromLevel;
    originator = nextOriginator;
    interruptLevel = 0xFF;
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
    constexpr u32 DestructorSlot = 1;
    for (BehaviourLevel*& level : levels)
    {
        if (level != nullptr)
        {
            CallVirtual<void>(level, level->vtable, DestructorSlot, DestroyAndFree);
        }

        level = nullptr;
    }
}

u32 BehaviourRunner::QueueStarter(ScriptStarter* starter, void* originator, u32 force)
{
    if (force == 0)
    {
        auto priorityOf = [](const ScriptResource* script) { return static_cast<u8>(script->bits >> ScriptResource::PriorityShift); };
        u32 priority = priorityOf(starter);
        if (nextStarter != nullptr && priority < priorityOf(nextStarter))
        {
            return 0;
        }

        if (receivers != nullptr)
        {
            u32 running = receivers->bits & StarterReceivers::PriorityMask;
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
                takes = (receivers->bits & StarterReceivers::Restartable) != 0;
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
    constexpr u32 Originator = 0xFF;
    if (designator >= Originator)
    {
        return static_cast<InstanceContext*>(originator);
    }

    return receivers->instances[designator & 0xFF];
}
