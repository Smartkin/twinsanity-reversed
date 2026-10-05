#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/attachments.h"
#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/scripttokens.h"
#include "game/vehicles.h"
#include "game/view.h"

#include <cstddef>
#include <cstdint>

// The commands that set an agent's focus from what's around it (a ray's hit, the nearest AI position, the instances of a sphere
// by their objects, the linked objects near the player), its key nearest the player, its route, and the AI positions' flags

extern "C"
{
    // The chunk manager the scripts' chunks are looked up in (the game context's)
    extern void* G_ChunkManager;

    // Command 557's parts: whether an instance's object is one it names (of the instances hanging from them when asked), the
    // instance found given to the node and marked, none given, and the first or a random instance of a list it names
    u32 RequestMatches(const RequestFocusCommand* command, InstanceContext* instance, u32 hanging) RETAIL(FUN_00114048);
    void MarkRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node) RETAIL(FUN_00121438);
    void GiveRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node) RETAIL(FUN_00121480);
    void ForgetRequested(const RequestFocusCommand* command, ObjectNode* node) RETAIL(FUN_001214e8);
    InstanceContext* FirstRequested(const RequestFocusCommand* command, InstanceContext** instances, u32 count)
        RETAIL(FUN_00121548);
    InstanceContext* RandomRequested(const RequestFocusCommand* command, InstanceContext** instances, u32 count)
        RETAIL(FUN_001215d8);
    // Command 632's parts: from a linked object on, the nearest taken or free one within the steps, and how many of its range are
    // taken
    s32 FindLinked(const SetLinkedObjectNearestPlayerCommand* command, u32 taken, s32 index, s32 steps, void* attachments)
        RETAIL(FUN_00129b68);
    u32 CountTakenLinked(const SetLinkedObjectNearestPlayerCommand* command, void* attachments) RETAIL(FUN_00129c18);
}

namespace
{
ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

bool Asleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// The agent references, forgotten once their instances are asleep (the second kept while the node's flag says so)
InstanceContext* AwakeAgentRef1(ObjectNode* node)
{
    if (node->agentRef1 != nullptr && Asleep(node->agentRef1))
    {
        node->agentRef1 = nullptr;
    }

    return node->agentRef1;
}

InstanceContext* AwakeAgentRef2(ObjectNode* node)
{
    if (node->agentRef2 != nullptr && Asleep(node->agentRef2) && !node->flags.keepsAgentRef2)
    {
        node->agentRef2 = nullptr;
    }

    return node->agentRef2;
}

InstanceContext* DesignatorOf(ObjectNode* node, u32 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, designator);
}

bool DesignatorPosition(ObjectNode* node, u32 designator, Vector4* position)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::GetDesignatorPositionSlot, designator, position) != 0;
}

// The place of an instance that may be none (retail reads the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// The instance's place's position, worked out from its matrix first when it moved
Vector4 PositionOf(const InstanceContext* instance)
{
    ObjectPlace* place = RetailPlaceOf(instance);
    place->SyncPosition();
    return place->position;
}

void SetFocusInstance(ObjectNode* node, InstanceContext* instance)
{
    node->focusInstance = instance;
    node->flags.focusInstance = 1;
    node->flags.focusPosition = 0;
}

void SetFocusPosition(ObjectNode* node, const Vector4& position)
{
    node->focusPosition = position;
    node->flags.focusPosition = 1;
    node->flags.focusInstance = 0;
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

InstanceContext* PlayedInstance()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->play.character);
}

AttachmentsNode* AttachmentsNodeOf(InstanceContext* instance)
{
    return static_cast<AttachmentsNode*>(GetGameNode(NodesOf(instance), NodeAttachments));
}

// The squared distance between two points
f32 DistanceSquared(const Vector4& a, const Vector4& b)
{
    f32 dx = a.x - b.x;
    f32 dy = a.y - b.y;
    f32 dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// The nearest AI position of a chunk to a point with the flags asked for (none: any)
AiPosition* NearestWithFlags(ChunkEntry* chunk, const Vector4* point, u16* index, u32 required, u32 ruledOut)
{
    if ((required | ruledOut) == 0)
    {
        return NearestAiPosition(chunk, point, index);
    }

    return NearestFlaggedAiPosition(chunk, point, index, required, ruledOut);
}

// How far a point is from an AI position's edge (its radius in w)
f32 EdgeDistance(const AiPosition* position, const Vector4& point)
{
    f32 dx = position->position.x - point.x;
    f32 dy = position->position.y - point.y;
    f32 dz = position->position.z - point.z;
    return __builtin_sqrtf(dx * dx + dy * dy + dz * dz) - position->position.w;
}

// A flag switched on or off, else left alone
template <typename T>
void Switch(T* value, u32 mode, T flag)
{
    if (mode == SwitchOn)
    {
        *value |= flag;
    }
    else if (mode == SwitchOff)
    {
        *value &= ~flag;
    }
}
}

// The focus the point a ray from a designator's instance or position (else the agent's) reaches through the collision: the
// ray as given (the world's and the start's spaces) or turned by the instance's place (its own and the target's), pulled back by
// the distance
void RaycastFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    Vector4 from = g_DefaultBox.min;
    from.w = 1.0f;
    Vector4 way = g_DefaultBox.min;
    way.w = 1.0f;
    u8 designator = target.designator;
    InstanceContext* instance = DesignatorOf(node, designator);
    if (instance != nullptr)
    {
        from = PositionOf(instance);
    }
    else
    {
        instance = node->owner;
        if (!DesignatorPosition(node, designator, &from))
        {
            from = PositionOf(instance);
        }
    }

    if (space == ControlPacket::CurrentSpace || space == ControlPacket::TargetSpace)
    {
        ObjectPlace* place = RetailPlaceOf(instance);
        RotateAndTranslate(place);
        const Matrix4x4& matrix = place->matrix;
        way.x = way.x + (x * matrix.m[0][0] + y * matrix.m[1][0] + z * matrix.m[2][0]);
        way.y = way.y + (x * matrix.m[0][1] + y * matrix.m[1][1] + z * matrix.m[2][1]);
        way.z = way.z + (x * matrix.m[0][2] + y * matrix.m[1][2] + z * matrix.m[2][2]);
    }
    else
    {
        way = {x, y, z, w};
    }

    if (LineOfSight(instance->chunk, &from, &way, SurfaceFlags::SolidToObjects, nullptr, SolidOrProjectileNodeKinds) == 0)
    {
        return;
    }

    way.x = way.x + from.x;
    way.y = way.y + from.y;
    way.z = way.z + from.z;
    if (distance != 0.0f)
    {
        // Retail bug: the point is pulled toward the world's origin by the distance, not back along the ray
        f32 length = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
        f32 share = (length - distance) / length;
        way.z = way.z * share;
        way.x = way.x * share;
        way.y = way.y * share;
    }

    SetFocusPosition(node, way);
}

// The focus the AI position nearest a receiver's instance (or the player's) that isn't flagged never taken, half a unit above it
// when it's within 30 units or flagged always taken, else the instance's position (a tenth along z without a position)
void SetFocusPositionToNearestPointCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Near = 30.0f;
    constexpr f32 Above = 0.5f;
    constexpr f32 NudgeWithoutPosition = Rounded(0.1);
    u8 receiver = target.receiver;
    InstanceContext* instance =
        receiver != DesignatesNone ? runner->receivers->instances[receiver] : PlayerInstance();
    if (instance == nullptr)
    {
        return;
    }

    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    Vector4 focus = PositionOf(instance);
    u16 index;
    AiPosition* nearest = NearestFlaggedAiPosition(chunk, &focus, &index, 0, AiPositionFlags::NeverTaken);
    if (nearest == nullptr)
    {
        focus.z = focus.z + NudgeWithoutPosition;
    }
    else
    {
        Vector4 point = nearest->position;
        point.w = 1.0f;
        if (nearest->flags.alwaysTaken || __builtin_sqrtf(DistanceSquared(point, focus)) < Near)
        {
            focus = point;
            focus.y = focus.y + Above;
        }
    }

    SetFocusPosition(NodeOf(runner), focus);
}

// An instance of a sphere about a target position given to the focus or an agent reference: one hanging from an instance of the
// objects named (when it names the hanging ones' objects), else of the objects named (the first, a random one or the nearest),
// else the first or the nearest of all; the one it had left out of the nearest unless it keeps it. The instances' flags wanted
// by the target word and the choice
void RequestFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Most = 0x80;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    ChunkData* chunk = owner->chunk;
    Vector4 sphere = {0.0f, 0.0f, 0.0f, 1.0f};
    u32 receiver = target.receiver == RequestTarget::NoReceiver ? DesignatesNone : target.receiver;
    const auto* offset = target.offsetGiven ? reinterpret_cast<const Vector4*>(&offsetX) : nullptr;
    DesignatedPosition(&sphere, target.space, runner, offset, target.designator, receiver, target.unused21);
    u32 wanted = choice.ignoresSignals ? 0 : ReferencedObjectFlags::ReceivesTriggerSignals;
    u32 unwanted = ReferencedObjectFlags::Asleep;
    // (6 and 8 take what 2 takes, 7 what 1 takes)
    switch (target.attachment)
    {
    case Holding:
    case 7:
        wanted |= ReferencedObjectFlags::HasAttachment;
        break;
    case HoldingNothing:
    case 6:
    case 8:
        unwanted = ReferencedObjectFlags::Asleep | ReferencedObjectFlags::HasAttachment;
        break;
    case Hanging:
    case HangingFromObjects:
        wanted |= ReferencedObjectFlags::Attached;
        break;
    case HangingFromNothing:
        unwanted = ReferencedObjectFlags::Asleep | ReferencedObjectFlags::Attached;
        break;
    default:
        break;
    }

    switch (target.busy)
    {
    case OnlyBusy:
        wanted |= ReferencedObjectFlags::Busy;
        break;
    case NoneBusy:
        unwanted |= ReferencedObjectFlags::Busy;
        break;
    default:
        break;
    }

    InstanceContext* results[Most];
    InstanceQuery query;
    query.results = reinterpret_cast<void**>(results);
    query.count = 0;
    query.most = Most;
    query.distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query.bits.value = InstanceQueryBits::AllWanted;
    query.wantedFlags = choice.visibleOnly ? wanted | ReferencedObjectFlags::Visible : wanted;
    query.unwantedFlags = unwanted;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, owner);
    query.skipped[1] = nullptr;
    InstanceContext* current;
    switch (choice.slot)
    {
    case SlotFocus:
        current = node->AwakeFocus();
        break;
    case SlotAgentRef1:
        current = AwakeAgentRef1(node);
        break;
    case SlotAgentRef2:
        current = AwakeAgentRef2(node);
        break;
    default:
        current = nullptr;
        break;
    }

    sphere.w = radius;
    u32 count = ChunkInstancesInSphere(chunk, &sphere, kinds, &query, 1);
    if (count == 0)
    {
        ForgetRequested(this, node);
        return;
    }

    bool keepsCurrent = choice.keepsCurrent;
    InstanceContext* chosen = nullptr;
    f32 best = Infinite;
    if (target.objectCount == 0)
    {
        if (!target.nearest)
        {
            GiveRequested(this, results[0], node);
            MarkRequested(this, results[0], node);
            return;
        }

        for (s32 left = static_cast<s32>(count); left > 0; left--)
        {
            InstanceContext* instance = results[count - left];
            if (!keepsCurrent && instance == current)
            {
                continue;
            }

            f32 distanceSquared = DistanceSquared(sphere, PositionOf(instance));
            if (distanceSquared < best)
            {
                chosen = instance;
                best = distanceSquared;
            }
        }
    }
    else if (choice.hangingCount != 0)
    {
        for (u16 index = 0; index < count; index++)
        {
            InstanceContext* instance = results[index];
            if (RequestMatches(this, instance, 1) == 0)
            {
                continue;
            }

            InstanceContext* parent = instance->parent;
            if (parent != nullptr && RequestMatches(this, parent, 0) != 0)
            {
                chosen = parent;
                break;
            }
        }
    }
    else if (!target.nearest)
    {
        chosen = target.random ? RandomRequested(this, results, count) : FirstRequested(this, results, count);
    }
    else
    {
        for (u16 index = 0; index < count; index++)
        {
            InstanceContext* instance = results[index];
            if ((!keepsCurrent && instance == current) || RequestMatches(this, instance, 0) == 0)
            {
                continue;
            }

            f32 distanceSquared = DistanceSquared(sphere, PositionOf(instance));
            if (distanceSquared < best)
            {
                chosen = instance;
                best = distanceSquared;
            }
        }
    }

    if (chosen == nullptr)
    {
        ForgetRequested(this, node);
        return;
    }

    GiveRequested(this, chosen, node);
    MarkRequested(this, chosen, node);
}

// Whether the instance's object is one the request names: one of its objects, or for the hanging instances one of theirs
u32 RequestMatches(const RequestFocusCommand* command, InstanceContext* instance, u32 hanging)
{
    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(NodesOf(instance), NodeObject));
    u16 id = node->agent->objectId;
    const u16* ids;
    u32 count;
    if (hanging != 0)
    {
        ids = &command->objects[RequestFocusCommand::HangingObjects];
        count = command->choice.hangingCount;
    }
    else
    {
        ids = command->objects;
        count = command->target.objectCount;
    }

    for (u32 index = 0; index < count; index++)
    {
        if (ids[index] == id)
        {
            return 1;
        }
    }

    return 0;
}

// The instance found made AgentRef2 too and marked busy, when asked
void MarkRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node)
{
    if (command->target.alsoAgentRef2)
    {
        node->agentRef2 = instance;
    }

    if (command->target.marksBusy)
    {
        instance->flags.busy = 1;
    }
}

// The instance given to the focus (its flag set when there's one), AgentRef1 or AgentRef2
void GiveRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node)
{
    switch (command->choice.slot)
    {
    case SlotFocus:
        node->focusInstance = instance;
        if (instance != nullptr)
        {
            node->flags.focusInstance = 1;
        }

        node->flags.focusPosition = 0;
        break;
    case SlotAgentRef1:
        node->agentRef1 = instance;
        break;
    case SlotAgentRef2:
        node->agentRef2 = instance;
        break;
    default:
        break;
    }
}

// The focus (its flags) or the agent reference forgotten
void ForgetRequested(const RequestFocusCommand* command, ObjectNode* node)
{
    switch (command->choice.slot)
    {
    case SlotFocus:
        node->flags.value &= ~ObjectNodeFlags::FocusMask;
        break;
    case SlotAgentRef1:
        node->agentRef1 = nullptr;
        break;
    case SlotAgentRef2:
        node->agentRef2 = nullptr;
        break;
    default:
        break;
    }
}

InstanceContext* FirstRequested(const RequestFocusCommand* command, InstanceContext** instances, u32 count)
{
    u16 total = static_cast<u16>(count);
    for (u16 index = 0; index < total; index++)
    {
        if (RequestMatches(command, instances[index], 0) != 0)
        {
            return instances[index];
        }
    }

    return nullptr;
}

// One of the first 128 that match, at random
InstanceContext* RandomRequested(const RequestFocusCommand* command, InstanceContext** instances, u32 count)
{
    constexpr s32 Most = 0x80;
    InstanceContext* matching[Most];
    s32 found = 0;
    u16 total = static_cast<u16>(count);
    for (u16 index = 0; index < total && found < Most; index++)
    {
        if (RequestMatches(command, instances[index], 0) != 0)
        {
            matching[found] = instances[index];
            found++;
        }
    }

    if (found == 0)
    {
        return nullptr;
    }

    return matching[RandomBelow(found)];
}

// The focus instance woken or put to sleep, its visibility, collision and triggers' signals and its part's damaging the
// character switched on or off
void SetFocusPropertiesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* focus = NodeOf(runner)->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    void* objectNode = GetGameNode(&focus->nodes, NodeObject);
    auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(focus)->agent->part);
    if (objectNode == nullptr)
    {
        return;
    }

    FocusProperties switches = properties;
    switch (switches.awake)
    {
    case SwitchOn:
        CallVirtual<u32>(focus, focus->vtable, InstanceContext::WakeSlot);
        break;
    case SwitchOff:
        CallVirtual<u32>(focus, focus->vtable, InstanceContext::SleepSlot);
        break;
    default:
        break;
    }

    Switch(&focus->flags.value, switches.visible, u32{ReferencedObjectFlags::Visible});
    Switch(&focus->flags.value, switches.collisionActive, u32{ReferencedObjectFlags::CollisionActive});
    Switch(&focus->flags.value, switches.receivesTriggerSignals, u32{ReferencedObjectFlags::ReceivesTriggerSignals});
    switch (switches.canDamageCharacter)
    {
    case SwitchOn:
        part->bits.canDamageCharacter = 1;
        break;
    case SwitchOff:
        part->bits.canDamageCharacter = 0;
        break;
    default:
        break;
    }
}

// The agent's key the one of its keys (from the first to the last given, not the current one unless it may) nearest where the
// played character will be in a time (its velocity times the lead); with a squared distance, the one nearest the agent of those
// that near the player
void SetKeyNearestPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* player = PlayedInstance();
    if (player == nullptr)
    {
        return;
    }

    ObjectNode* node = NodeOf(runner);
    Waypoints* waypoints = node->waypoints;
    InstanceContext* owner = node->owner;
    LayoutPosition* current = waypoints->positions.data[waypoints->key];
    Vector4 target = PositionOf(player);
    auto* character = static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&player->nodes, NodeCharacter))->agent);
    Vector4 velocity;
    if (character->vehicle != nullptr)
    {
        character->vehicle->VelocityVirtual(&velocity);
    }
    else
    {
        velocity = character->velocity;
    }

    if (leadSeconds != 0.0f)
    {
        target.x = target.x + velocity.x * leadSeconds;
        target.y = target.y + velocity.y * leadSeconds;
        target.z = target.z + velocity.z * leadSeconds;
    }

    bool takesCurrent = keys.takesCurrent != 0;
    u32 first = keys.first;
    u32 last = keys.last;
    s32 best = -1;
    f32 bestDistance = Infinite;
    if (nearDistanceSquared == 0.0f)
    {
        for (u32 key = 0; key < waypoints->keyCount; key++)
        {
            if (key < first || last < key)
            {
                continue;
            }

            LayoutPosition* position = waypoints->positions.data[key];
            if (!takesCurrent && position == current)
            {
                continue;
            }

            Vector4 point = position->position;
            point.w = 1.0f;
            f32 distanceSquared = DistanceSquared(point, target);
            if (distanceSquared < bestDistance)
            {
                bestDistance = distanceSquared;
                best = static_cast<s32>(key);
            }
        }
    }
    else
    {
        Vector4 home = PositionOf(owner);
        for (u32 key = 0; key < waypoints->keyCount; key++)
        {
            if (key < first || last < key)
            {
                continue;
            }

            LayoutPosition* position = waypoints->positions.data[key];
            Vector4 point = position->position;
            point.w = 1.0f;
            if (!takesCurrent && position == current)
            {
                continue;
            }

            if (DistanceSquared(point, target) < nearDistanceSquared)
            {
                f32 distanceSquared = DistanceSquared(point, home);
                if (distanceSquared < bestDistance)
                {
                    bestDistance = distanceSquared;
                    best = static_cast<s32>(key);
                }
            }
        }
    }

    if (best == -1)
    {
        return;
    }

    s32 count = waypoints->keyCount;
    waypoints->flags.wrapped = best == count;
    waypoints->key = static_cast<u8>(best < count ? best : best - count);
}

// A route from the AI position nearest the agent (or a point ahead of it) to the one nearest a target position, when they're
// near enough their positions; the path finder's focus the middle of the two points, the paths it takes and how it weighs them
// from the command's bits
void GetShortRouteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, owner);
    Waypoints* waypoints = node->waypoints;
    waypoints->ReleaseRoute();
    waypoints->flags.wrapped = 0;
    Vector4 start = PositionOf(owner);
    if (options.startsAhead)
    {
        f32 distance = ahead.FloatWith(node->PacketProperties());
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        const f32* forward = place->matrix.m[2];
        start.x = start.x + forward[0] * distance;
        start.y = start.y + forward[1] * distance;
        start.z = start.z + forward[2] * distance;
    }

    Vector4 end;
    const auto* offset = target.offsetGiven ? reinterpret_cast<const Vector4*>(&offsetX) : nullptr;
    DesignatedPosition(&end, target.space, runner, offset, target.designator, target.receiver, 0);
    u16 startIndex;
    AiPosition* from = NearestWithFlags(chunk, &start, &startIndex, positionFlags.startRequired, startFlags.startRuledOut);
    u16 endIndex;
    AiPosition* to = NearestWithFlags(chunk, &end, &endIndex, endFlags.endRequired, positionFlags.endRuledOut);
    if (from == nullptr || to == nullptr)
    {
        return;
    }

    if (0.0f < startRange && startRange < EdgeDistance(from, start))
    {
        return;
    }

    if (0.0f < endRange && endRange < EdgeDistance(to, end))
    {
        return;
    }

    RouteRequest request;
    request.startChunk = chunk->index;
    request.startPosition = startIndex;
    request.endPosition = endIndex;
    request.endChunk = chunk->index;
    request.unused08 = target.givesRollRadius ? node->rollRadius : 0.0f;
    request.unused0C = 0.0f;
    RouteRequestFlags flags = {};
    if (target.distanceOnly)
    {
        flags.distanceOnly = 1;
    }
    else
    {
        PropertyHolder* properties = node->PacketProperties();
        if (target.givesWeight)
        {
            flags.unused1 = 1;
            request.unused0C = weight.FloatWith(properties);
        }

        if (!options.takesPathFlag6)
        {
            flags.rulesOutScriptFlag6 = 1;
        }

        if (!options.takesPlainPaths)
        {
            flags.rulesOutPlainPaths = 1;
        }

        if (!target.takesJumps)
        {
            flags.rulesOutJumps = 1;
            flags.rulesOutHighJumps = 1;
            flags.rulesOutLongJumps = 1;
        }
        else
        {
            if (!target.takesHighJumps)
            {
                flags.rulesOutHighJumps = 1;
            }

            if (!target.takesLongJumps)
            {
                flags.rulesOutLongJumps = 1;
            }
        }

        if (!options.takesPathFlag8)
        {
            flags.rulesOutScriptFlag8 = 1;
        }

        if (!options.takesPathFlag7)
        {
            flags.rulesOutScriptFlag7 = 1;
        }

        if (!target.takesFlights)
        {
            flags.rulesOutFlights = 1;
        }

        if (target.avoidsFocus)
        {
            flags.avoidsFocus = 1;
            request.unused10 = avoidFocusWeight.FloatWith(properties);
        }

        if (target.nearFocus)
        {
            flags.nearFocus = 1;
            request.nearFocusWeight = nearFocusWeight.FloatWith(properties);
        }

        if (target.positionCosts)
        {
            flags.positionCosts = 1;
            request.unused18 = positionCostWeight.FloatWith(properties);
        }
    }

    request.flags = flags;
    request.unused04 = endFlags.kind;
    PathFinder* finder = static_cast<ChunkManager*>(G_ChunkManager)->pathFinder;
    finder->routeStart = start;
    finder->routeEnd = end;
    Vector4 middle = end;
    middle.x = (end.x + start.x) * 0.5f;
    middle.y = (end.y + start.y) * 0.5f;
    middle.z = (end.z + start.z) * 0.5f;
    finder->focus = middle;
    finder->focusRadius = DistanceSquared(start, middle);
    if (target.nearFocus)
    {
        // With the distance only flag the weight is what the stack held (retail's too)
        SetNearFocusWeight(finder, request.nearFocusWeight);
    }

    auto* route = CallVirtual<Route*>(finder, finder->vtable, PathFinder::FindRouteSlot, &request);
    if (route != nullptr)
    {
        waypoints->SetRoute(route);
    }
}

// The flags of the AI position nearest the agent: blocked cleared (its switch on) or set (off), airborne and always taken
// switched
void SetNearestPointFlagsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* owner = runner->agentNode->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, owner);
    Vector4 position = PositionOf(owner);
    AiPosition* nearest = NearestAiPosition(chunk, &position, nullptr);
    if (nearest == nullptr)
    {
        return;
    }

    NearestPointSwitches bits = switches;
    switch (bits.blocked)
    {
    case SwitchOn:
        nearest->flags.blocked = 0;
        break;
    case SwitchOff:
        nearest->flags.blocked = 1;
        break;
    default:
        break;
    }

    Switch(&nearest->flags.value, bits.airborne, u16{AiPositionFlags::Airborne});
    Switch(&nearest->flags.value, bits.alwaysTaken, u16{AiPositionFlags::AlwaysTaken});
}

// AgentRef1 the linked object (of the range) whose box's middle is nearest the player, marked busy; with two busy or more around, a
// free one next to the nearest one between them (or toward the free side)
void SetLinkedObjectNearestPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr s32 Near = 10;
    constexpr s32 FarSteps = 100;
    constexpr s32 NoneFound = -1;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    AttachmentsNode* attachments = AttachmentsNodeOf(owner);
    InstanceContext* player = PlayerInstance();
    // Its own position worked out, which nothing reads
    PositionOf(owner);
    Vector4 target = PositionOf(player);
    s32 nearest = NoneFound;
    f32 best = Infinite;
    u8 firstByte = range.first;
    u8 endByte = range.end;
    u32 first = firstByte != LinkedRange::Whole ? firstByte - 1u : 0;
    u32 end = endByte != LinkedRange::Whole ? endByte : attachments->LinkedCount();
    for (u32 index = first; index < end; index++)
    {
        const Box* box = &attachments->linked[index]->collision.box;
        Vector4 middle;
        middle.x = (box->max.x - box->min.x) * 0.5f + box->min.x;
        middle.y = (box->max.y - box->min.y) * 0.5f + box->min.y;
        middle.z = (box->max.z - box->min.z) * 0.5f + box->min.z;
        f32 distanceSquared = DistanceSquared(target, middle);
        if (distanceSquared < best)
        {
            best = distanceSquared;
            nearest = static_cast<s32>(index);
        }
    }

    s32 takenAfter = FindLinked(this, 1, nearest, Near, attachments);
    s32 takenBefore = FindLinked(this, 1, nearest, -Near, attachments);
    s32 direction = 1;
    s32 chosen;
    if (CountTakenLinked(this, attachments) < 2)
    {
        chosen = nearest;
    }
    else if (takenAfter != NoneFound)
    {
        if (takenBefore == NoneFound)
        {
            chosen = FindLinked(this, 0, static_cast<s32>((static_cast<u32>(nearest) + first) >> 1), Near, attachments);
            direction = -1;
        }
        else if (static_cast<u32>(nearest - takenBefore) < static_cast<u32>(takenAfter - nearest))
        {
            chosen = FindLinked(this, 0, nearest, Near, attachments);
        }
        else
        {
            chosen = FindLinked(this, 0, nearest, -Near, attachments);
            direction = -1;
        }
    }
    else if (takenBefore != NoneFound)
    {
        chosen = FindLinked(this, 0, static_cast<s32>((static_cast<u32>(nearest) - 1 + end) >> 1), -Near, attachments);
    }
    else
    {
        chosen = nearest;
    }

    if (chosen == NoneFound)
    {
        node->agentRef1 = nullptr;
        return;
    }

    InstanceContext* instance = attachments->linked[chosen];
    if (instance->flags.busy)
    {
        // Retail bug: a free one is looked for from the end it moved toward and dropped: the nearest is taken again
        if (direction > 0)
        {
            FindLinked(this, 0, 0, FarSteps, attachments);
        }
        else
        {
            FindLinked(this, 0, static_cast<s32>(end - 1), -FarSteps, attachments);
        }

        instance = attachments->linked[nearest];
    }

    node->agentRef1 = instance;
    instance->flags.busy = 1;
}

// From a linked object on, the first (within the steps either way by their sign, not past either end) that's busy or free as
// asked: its index, -1 for none
s32 FindLinked(const SetLinkedObjectNearestPlayerCommand*, u32 taken, s32 index, s32 steps, void* attachments)
{
    auto* node = static_cast<AttachmentsNode*>(attachments);
    s32 last = static_cast<s32>(node->LinkedCount()) - 1;
    bool backwards = steps < 1;
    s32 step = backwards ? -1 : 1;
    for (s32 done = 0;; done += step, index += step)
    {
        bool isTaken = node->linked[index]->flags.busy;
        if (isTaken == (taken != 0))
        {
            return index;
        }

        if (done == steps || (index == 0 && steps < 0) || (index == last && !backwards))
        {
            return -1;
        }
    }
}

u32 CountTakenLinked(const SetLinkedObjectNearestPlayerCommand* command, void* attachments)
{
    auto* node = static_cast<AttachmentsNode*>(attachments);
    u8 firstByte = command->range.first;
    u8 endByte = command->range.end;
    u32 first = firstByte != LinkedRange::Whole ? firstByte - 1u : 0;
    u32 end = endByte != LinkedRange::Whole ? endByte : node->LinkedCount();
    u32 count = 0;
    for (u32 index = first; index < end; index++)
    {
        count += node->linked[index]->flags.busy;
    }

    return count;
}

// The focus the linked object nearest the player (within about 37 degrees of the camera's view, drawn) that's farther from the
// player than the player goes in a second
void SetFocusToLinkedObjectInViewCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    // The cosine of the angle from the camera's view
    constexpr f32 InViewCosine = Rounded(0.8);
    ObjectNode* node = NodeOf(runner);
    InstanceContext* chosen = nullptr;
    f32 best = Infinite;
    AttachmentsNode* attachments = AttachmentsNodeOf(node->owner);
    Vector4 target = PositionOf(PlayerInstance());
    InstanceContext* camera = CameraInstance();
    auto* character = reinterpret_cast<Agent*>(g_PlayerCharacter);
    Vector4 velocity;
    CallVirtual<u32>(character, character->vtable, Agent::VelocitySlot, &velocity);
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    f32 speedSquared = speed * speed;
    if (attachments != nullptr && camera != nullptr)
    {
        Vector4 eye = PositionOf(camera);
        ObjectPlace* place = camera->place;
        RotateAndTranslate(place);
        Vector4 view = *reinterpret_cast<const Vector4*>(place->matrix.m[2]);
        for (u32 index = 0; index < attachments->LinkedCount(); index++)
        {
            InstanceContext* instance = attachments->linked[index];
            if (instance == nullptr || !instance->flags.inDrawnCell)
            {
                continue;
            }

            Vector4 position = PositionOf(instance);
            f32 distanceSquared = DistanceSquared(position, target);
            if (!(speedSquared < distanceSquared) || !(distanceSquared < best))
            {
                continue;
            }

            Vector4 way = position;
            way.x = position.x - eye.x;
            way.y = position.y - eye.y;
            way.z = position.z - eye.z;
            f32 inverse = InverseLength(&way, LengthEpsilon);
            way.x = way.x * inverse;
            way.y = way.y * inverse;
            way.z = way.z * inverse;
            if (InViewCosine < way.x * view.x + way.y * view.y + way.z * view.z)
            {
                best = distanceSquared;
                chosen = instance;
            }
        }
    }

    if (chosen != nullptr)
    {
        SetFocusInstance(node, chosen);
    }
}
