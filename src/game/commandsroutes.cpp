#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
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
#include "game/vehicles.h"
#include "game/view.h"

#include <cstddef>
#include <cstdint>

// The commands that set an agent's focus from what's around it (a ray's hit, the nearest AI position, the instances of a sphere
// by their objects, the linked objects near the player), its key nearest the player, its route, and the AI positions' flags

extern "C"
{
    // The AI position of a chunk nearest a point, with its index; and the nearest with any of the required flags and none of the
    // ruled out ones
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
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 GetDesignatorPositionSlot = 37;
// The referenced objects' vtable functions waking them and putting them to sleep, and the agents' giving their velocity
constexpr u32 WakeSlot = 2;
constexpr u32 SleepSlot = 3;
constexpr u32 AgentVelocitySlot = 11;
constexpr u8 NoReceiver = 0xFF;
// The instance flag the requests and the linked object searches mark the instances they took with
constexpr u32 TakenFlag = 0x100;
// The attachments node's word: the linked objects' count (bits 0-4), the instances after it
constexpr u32 LinkedCountMask = 0x1F;
// The squared lengths too short to have a direction
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 Far = Rounded(1e30);

// What a route request carries between its flags and its positions (RouteRequest's bytes 4 to 0x1C): the byte command 549
// gives, a radius, and the values that go with its flags 0x2, 0x8 (keep away from the path finder's focus), 0x10 (keep near
// it) and 0x20 (the positions' own costs)
struct RouteRequestValues
{
    u8 kind;
    u8 unknown1[3];
    f32 radius;
    f32 flag2Value;
    f32 avoidFocusValue;
    f32 nearFocusValue;
    f32 positionCostValue;
};
static_assert(sizeof(RouteRequestValues) == sizeof(RouteRequest::unknown04));

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
    return (instance->flags & ReferencedObject::FlagAsleep) != 0;
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
    if (node->agentRef2 != nullptr && Asleep(node->agentRef2) && (node->flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
    {
        node->agentRef2 = nullptr;
    }

    return node->agentRef2;
}

InstanceContext* DesignatorOf(ObjectNode* node, u32 designator)
{
    return CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, designator);
}

bool DesignatorPosition(ObjectNode* node, u32 designator, Vector4* position)
{
    return CallVirtual<u32>(node, node->vtable, GetDesignatorPositionSlot, designator, position) != 0;
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
    node->flags = (node->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
}

void SetFocusPosition(ObjectNode* node, const Vector4& position)
{
    node->focusPosition = position;
    node->flags = (node->flags | ObjectNodeBase::FlagFocusPosition) & ~ObjectNodeBase::FlagFocusInstance;
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

InstanceContext* PlayedInstance()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->Field(GameProgress::CharacterShift));
}

InstanceContext** LinkedInstances(void* attachments)
{
    return reinterpret_cast<InstanceContext**>(static_cast<u8*>(attachments) + 0x20);
}

u32 LinkedCount(void* attachments)
{
    return *reinterpret_cast<u32*>(static_cast<u8*>(attachments) + 0x18) & LinkedCountMask;
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

// A flag set (mode 1) or cleared (2), else left alone
template <typename T>
void Switch(T* value, u32 mode, T flag)
{
    if (mode == 1)
    {
        *value |= flag;
    }
    else if (mode == 2)
    {
        *value &= ~flag;
    }
}
}

// The focus the point a ray from a designator's instance or position (else the agent's) reaches through the collision: the
// ray as given (space 0 or 1) or turned by the instance's place (2 and 3), pulled back by the distance
void RaycastFocusPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 LineOfSightBits = 0x40;
    constexpr u32 InstanceKinds = 0x15B010;
    ObjectNode* node = NodeOf(runner);
    Vector4 from = g_DefaultBox.min;
    from.w = 1.0f;
    Vector4 way = g_DefaultBox.min;
    way.w = 1.0f;
    u8 designator = static_cast<u8>(target);
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

    if (mode.raw == 2 || mode.raw == 3)
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
        way = {x, y, z, value5};
    }

    if (LineOfSight(instance->chunk, &from, &way, LineOfSightBits, nullptr, InstanceKinds) == 0)
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

// The focus the AI position nearest a receiver's instance (0xFF the player) without flag 4, half a unit above it when it's
// within 30 units or has flag 2, else the instance's position (a tenth along z without a position)
void SetFocusPositionToNearestPointCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 AlwaysTaken = 0x4;
    constexpr u32 RuledOut = 0x10;
    constexpr f32 Near = 30.0f;
    u8 receiver = static_cast<u8>(target);
    InstanceContext* instance = receiver != NoReceiver ? runner->receivers->instances[receiver] : PlayerInstance();
    if (instance == nullptr)
    {
        return;
    }

    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    Vector4 focus = PositionOf(instance);
    u16 index;
    AiPosition* nearest = NearestFlaggedAiPosition(chunk, &focus, &index, 0, RuledOut);
    if (nearest == nullptr)
    {
        focus.z = focus.z + Rounded(0.1);
    }
    else
    {
        Vector4 point = nearest->position;
        point.w = 1.0f;
        if ((nearest->flags & AlwaysTaken) != 0 || __builtin_sqrtf(DistanceSquared(point, focus)) < Near)
        {
            focus = point;
            focus.y = focus.y + 0.5f;
        }
    }

    SetFocusPosition(NodeOf(runner), focus);
}

// An instance of a sphere about a target position given to the focus or an agent reference (flags10 bits 0-1): one hanging from
// an instance of the objects named (flags10 bits 3-4 count the hanging ones' objects), else of the objects named (target bits
// 16-18 count them: the first, a random one (bit 22) or the nearest (bit 19)), else the first or the nearest of all; the one it
// had left out of the nearest unless flags10 bit 5. The instances' flags wanted by target bits 23-28 and flags10 bits 2 and 6
void RequestFocusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    // The target word: the space (bits 0-3), the receiver (4-7, all set none), the designator (8-15), the objects' count (16-18),
    // the nearest (19), the offset given (20), the designator's unknown (21), a random one (22), the kinds (23-26), the taken mark
    // wanted or not (27-28)
    constexpr u32 ReceiverMask = 0xF0;
    constexpr u32 ObjectCountMask = 0x70000;
    constexpr u32 TakesNearest = 0x80000;
    constexpr u32 OffsetGiven = 0x100000;
    constexpr u32 TakesRandom = 0x400000;
    constexpr u32 HangingCountMask = 0x18;
    constexpr u32 KeepsCurrent = 0x20;
    constexpr u32 NoTriggerSignals = 0x4;
    constexpr u32 WantsVisible = 0x40;
    constexpr u32 Most = 0x80;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    ChunkData* chunk = owner->chunk;
    Vector4 sphere = {0.0f, 0.0f, 0.0f, 1.0f};
    u32 receiver = (targetFlags & ReceiverMask) == ReceiverMask ? 0xFF : targetFlags >> 4 & 0xF;
    const auto* offset = (targetFlags & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&x) : nullptr;
    DesignatedPosition(&sphere, targetFlags & 0xF, runner, offset, targetFlags >> 8 & 0xFF, receiver, targetFlags >> 21 & 1);
    u32 wanted = (flags10 & NoTriggerSignals) != 0 ? 0 : ReferencedObject::FlagTriggerSignals;
    u32 unwanted = ReferencedObject::FlagAsleep;
    switch (targetFlags >> 23 & 0xF)
    {
    case 1:
    case 7:
        wanted |= 0x80;
        break;
    case 2:
    case 6:
    case 8:
        unwanted = ReferencedObject::FlagAsleep | 0x80;
        break;
    case 3:
    case 5:
        wanted |= 0x40;
        break;
    case 4:
        unwanted = ReferencedObject::FlagAsleep | 0x40;
        break;
    default:
        break;
    }

    switch (targetFlags >> 27 & 0x3)
    {
    case 1:
        wanted |= TakenFlag;
        break;
    case 2:
        unwanted |= TakenFlag;
        break;
    default:
        break;
    }

    InstanceContext* results[Most];
    InstanceRayHit query;
    query.results = reinterpret_cast<void**>(results);
    query.count = 0;
    query.most = Most;
    query.distance = Far;
    // Retail keeps the stack's other bits (nothing reads them)
    query.bits = InstanceRayHit::BitAllWanted;
    query.wantedFlags = (flags10 & WantsVisible) != 0 ? wanted | ReferencedObject::FlagVisible : wanted;
    query.unwantedFlags = unwanted;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, owner);
    query.skipped[1] = nullptr;
    InstanceContext* current;
    switch (flags10 & 0x3)
    {
    case 0:
        current = node->AwakeFocus();
        break;
    case 1:
        current = AwakeAgentRef1(node);
        break;
    case 2:
        current = AwakeAgentRef2(node);
        break;
    default:
        current = nullptr;
        break;
    }

    sphere.w = radius;
    u32 count = ChunkInstancesInSphere(chunk, &sphere, static_cast<u32>(flags11), &query, 1);
    if (count == 0)
    {
        ForgetRequested(this, node);
        return;
    }

    bool keepsCurrent = (flags10 & KeepsCurrent) != 0;
    InstanceContext* chosen = nullptr;
    f32 best = Far;
    if ((targetFlags & ObjectCountMask) == 0)
    {
        if ((targetFlags & TakesNearest) == 0)
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
    else if ((flags10 & HangingCountMask) != 0)
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
    else if ((targetFlags & TakesNearest) == 0)
    {
        chosen = (targetFlags & TakesRandom) != 0 ? RandomRequested(this, results, count) : FirstRequested(this, results, count);
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

// Whether the instance's object is one the request names: up to seven IDs from 0x20 (the count in the target word's bits 16-18),
// or for the hanging instances up to three from 0x28 (flags10 bits 3-4)
u32 RequestMatches(const RequestFocusCommand* command, InstanceContext* instance, u32 hanging)
{
    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(NodesOf(instance), ObjectNodeKind));
    u16 id = node->agent->objectId;
    const u16* ids;
    u32 count;
    if (hanging != 0)
    {
        ids = reinterpret_cast<const u16*>(&command->ids8);
        count = command->flags10 >> 3 & 0x3;
    }
    else
    {
        ids = reinterpret_cast<const u16*>(&command->ids6);
        count = command->targetFlags >> 16 & 0x7;
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

// The instance found made AgentRef2 too (target bit 30) and marked taken (bit 29)
void MarkRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node)
{
    constexpr u32 AlsoAgentRef2 = 0x40000000;
    constexpr u32 MarksTaken = 0x20000000;
    if ((command->targetFlags & AlsoAgentRef2) != 0)
    {
        node->agentRef2 = instance;
    }

    if ((command->targetFlags & MarksTaken) != 0)
    {
        instance->flags |= TakenFlag;
    }
}

// The instance given to the focus (its flag set when there's one), AgentRef1 or AgentRef2 (flags10 bits 0-1)
void GiveRequested(const RequestFocusCommand* command, InstanceContext* instance, ObjectNode* node)
{
    switch (command->flags10 & 0x3)
    {
    case 0:
        node->focusInstance = instance;
        if (instance != nullptr)
        {
            node->flags |= ObjectNodeBase::FlagFocusInstance;
        }

        node->flags &= ~ObjectNodeBase::FlagFocusPosition;
        break;
    case 1:
        node->agentRef1 = instance;
        break;
    case 2:
        node->agentRef2 = instance;
        break;
    default:
        break;
    }
}

// The focus (its flags) or the agent reference forgotten
void ForgetRequested(const RequestFocusCommand* command, ObjectNode* node)
{
    switch (command->flags10 & 0x3)
    {
    case 0:
        node->flags = node->flags & ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
        break;
    case 1:
        node->agentRef1 = nullptr;
        break;
    case 2:
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

// The focus instance's waking (bits 0-1: woken, put to sleep), visibility (2-3), sphere contact (4-5), triggers' signals (6-7)
// and its part's damaging the character (8-9), each set (1) or cleared (2)
void SetFocusPropertiesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* focus = NodeOf(runner)->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    void* objectNode = GetGameNode(&focus->nodes, ObjectNodeKind);
    auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(focus)->agent->part);
    if (objectNode == nullptr)
    {
        return;
    }

    u32 bits = static_cast<u32>(value1.raw);
    switch (bits & 0x3)
    {
    case 1:
        CallVirtual<u32>(focus, focus->vtable, WakeSlot);
        break;
    case 2:
        CallVirtual<u32>(focus, focus->vtable, SleepSlot);
        break;
    default:
        break;
    }

    Switch(&focus->flags, bits >> 2 & 0x3, u32{ReferencedObject::FlagVisible});
    Switch(&focus->flags, bits >> 4 & 0x3, u32{ReferencedObject::FlagSphereContact});
    Switch(&focus->flags, bits >> 6 & 0x3, u32{ReferencedObject::FlagTriggerSignals});
    Switch(&part->bits, bits >> 8 & 0x3, u32{BasicAgentPart::CanDamageCharacter});
}

// The agent's key the one of its keys (from the first to the last given, not the current one unless byte 0x14) nearest where the
// played character will be in a time (its velocity times value2); with a squared distance (value1), the one nearest the agent
// of those that near the player
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
    auto* character = static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&player->nodes, NodePlayer))->agent);
    Vector4 velocity;
    if (character->vehicle != nullptr)
    {
        character->vehicle->VelocityVirtual(&velocity);
    }
    else
    {
        velocity = character->velocity;
    }

    if (value2 != 0.0f)
    {
        target.x = target.x + velocity.x * value2;
        target.y = target.y + velocity.y * value2;
        target.z = target.z + velocity.z * value2;
    }

    bool takesCurrent = static_cast<u8>(value3) != 0;
    u32 first = value3 >> 8 & 0xFF;
    u32 last = value3 >> 16 & 0xFF;
    s32 best = -1;
    f32 bestDistance = Far;
    if (value1 == 0.0f)
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

            if (DistanceSquared(point, target) < value1)
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
    waypoints->flags = (waypoints->flags & ~Waypoints::FlagWrapped) | (best == count ? Waypoints::FlagWrapped : 0);
    waypoints->key = static_cast<u8>(best < count ? best : best - count);
}

// A route from the AI position nearest the agent (or a point ahead of it) to the one nearest a target position, when they're
// near enough their positions; the path finder's focus the middle of the two points, what the request rules out and weighs
// from the command's bits
void GetShortRouteCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    // The target word: the receiver (bits 0-7), the designator (8-15), the paths with flags 0-2 kept (17; else 18 and 19 keep
    // flags 1 and 0), the paths with flag 3 kept (20), the roll radius (21), weighs a value (22: flag 2), keeps near the focus
    // (23), away from it (24), the offset given (25), the distance only (26), the positions' costs (27), the space (28-31); the
    // second word: the start ahead along the agent's z axis (bit 0), and paths with flags 4, 7, 6 and 5 kept (bits 1-4)
    constexpr u32 KeepsFlags012 = 0x20000;
    constexpr u32 KeepsFlag1 = 0x40000;
    constexpr u32 KeepsFlag0 = 0x80000;
    constexpr u32 KeepsFlag3 = 0x100000;
    constexpr u32 UsesRollRadius = 0x200000;
    constexpr u32 WeighsFlag2 = 0x400000;
    constexpr u32 NearFocus = 0x800000;
    constexpr u32 AvoidsFocus = 0x1000000;
    constexpr u32 OffsetGiven = 0x2000000;
    constexpr u32 DistanceOnly = 0x4000000;
    constexpr u32 PositionCosts = 0x8000000;
    constexpr u32 StartsAhead = 0x1;
    constexpr u32 KeepsFlag4 = 0x2;
    constexpr u32 KeepsFlag5 = 0x4;
    constexpr u32 KeepsFlag6 = 0x8;
    constexpr u32 KeepsFlag7 = 0x10;
    // The request's flags ruling out paths with flags 0 to 7 (bits 17-24)
    constexpr u32 RulesOutFlag0 = 0x20000;
    constexpr u32 RulesOutFlag1 = 0x40000;
    constexpr u32 RulesOutFlag2 = 0x80000;
    constexpr u32 RulesOutFlag3 = 0x100000;
    constexpr u32 RulesOutFlag4 = 0x200000;
    constexpr u32 RulesOutFlag5 = 0x400000;
    constexpr u32 RulesOutFlag6 = 0x800000;
    constexpr u32 RulesOutFlag7 = 0x1000000;
    constexpr u32 FindRouteSlot = 4;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, owner);
    Waypoints* waypoints = node->waypoints;
    waypoints->ReleaseRoute();
    waypoints->flags &= ~Waypoints::FlagWrapped;
    Vector4 start = PositionOf(owner);
    if ((flags7 & StartsAhead) != 0)
    {
        f32 ahead = value12.FloatWith(node->PacketProperties());
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        const f32* forward = place->matrix.m[2];
        start.x = start.x + forward[0] * ahead;
        start.y = start.y + forward[1] * ahead;
        start.z = start.z + forward[2] * ahead;
    }

    Vector4 end;
    const auto* offset = (targetFlags & OffsetGiven) != 0 ? reinterpret_cast<const Vector4*>(&x) : nullptr;
    DesignatedPosition(&end, targetFlags >> 28, runner, offset, targetFlags >> 8 & 0xFF, targetFlags & 0xFF, 0);
    u16 startIndex;
    AiPosition* from = NearestWithFlags(chunk, &start, &startIndex, unknown14 >> 16, unknown15 & 0xFFFF);
    u16 endIndex;
    AiPosition* to = NearestWithFlags(chunk, &end, &endIndex, keyAndObject >> 16, unknown14 & 0xFFFF);
    if (from == nullptr || to == nullptr)
    {
        return;
    }

    if (0.0f < value16 && value16 < EdgeDistance(from, start))
    {
        return;
    }

    if (0.0f < value17 && value17 < EdgeDistance(to, end))
    {
        return;
    }

    RouteRequest request;
    auto* values = reinterpret_cast<RouteRequestValues*>(request.unknown04);
    request.startChunk = chunk->index;
    request.startPosition = startIndex;
    request.endPosition = endIndex;
    request.endChunk = chunk->index;
    values->radius = (targetFlags & UsesRollRadius) != 0 ? node->rollRadius : 0.0f;
    values->flag2Value = 0.0f;
    if ((targetFlags & DistanceOnly) != 0)
    {
        request.flags = RouteRequest::FlagDistanceOnly;
    }
    else
    {
        u32 flags = 0;
        PropertyHolder* properties = node->PacketProperties();
        if ((targetFlags & WeighsFlag2) != 0)
        {
            flags |= 0x2;
            values->flag2Value = value10.FloatWith(properties);
        }

        if ((flags7 & KeepsFlag4) == 0)
        {
            flags |= RulesOutFlag4;
        }

        if ((flags7 & KeepsFlag7) == 0)
        {
            flags |= RulesOutFlag7;
        }

        if ((targetFlags & KeepsFlags012) == 0)
        {
            flags |= RulesOutFlag0 | RulesOutFlag1 | RulesOutFlag2;
        }
        else
        {
            if ((targetFlags & KeepsFlag0) == 0)
            {
                flags |= RulesOutFlag1;
            }

            if ((targetFlags & KeepsFlag1) == 0)
            {
                flags |= RulesOutFlag2;
            }
        }

        if ((flags7 & KeepsFlag6) == 0)
        {
            flags |= RulesOutFlag6;
        }

        if ((flags7 & KeepsFlag5) == 0)
        {
            flags |= RulesOutFlag5;
        }

        if ((targetFlags & KeepsFlag3) == 0)
        {
            flags |= RulesOutFlag3;
        }

        if ((targetFlags & AvoidsFocus) != 0)
        {
            flags |= RouteRequest::FlagAvoidFocus;
            values->avoidFocusValue = value8.FloatWith(properties);
        }

        if ((targetFlags & NearFocus) != 0)
        {
            flags |= RouteRequest::FlagNearFocus;
            values->nearFocusValue = value9.FloatWith(properties);
        }

        if ((targetFlags & PositionCosts) != 0)
        {
            flags |= RouteRequest::FlagPositionCosts;
            values->positionCostValue = value11.FloatWith(properties);
        }

        request.flags = flags;
    }

    values->kind = static_cast<u8>(keyAndObject);
    PathFinder* finder = static_cast<ChunkManager*>(G_ChunkManager)->pathFinder;
    finder->routeStart = start;
    finder->routeEnd = end;
    Vector4 middle = end;
    middle.x = (end.x + start.x) * 0.5f;
    middle.y = (end.y + start.y) * 0.5f;
    middle.z = (end.z + start.z) * 0.5f;
    finder->focus = middle;
    finder->focusRadius = DistanceSquared(start, middle);
    if ((targetFlags & NearFocus) != 0)
    {
        // With the distance only flag the value is what the stack held (retail's too)
        SetNearFocusWeight(finder, values->nearFocusValue);
    }

    auto* route = CallVirtual<Route*>(finder, finder->vtable, FindRouteSlot, &request);
    if (route != nullptr)
    {
        waypoints->SetRoute(route);
    }
}

// The flags of the AI position nearest the agent: bit 0 (blocked) cleared (bits 0-1 at 1) or set (2), bits 1 and 2 set (1) or
// cleared (2) by bits 2-3 and 4-5
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

    u32 bits = static_cast<u32>(flags.raw);
    switch (bits & 0x3)
    {
    case 1:
        nearest->flags &= ~AiPosition::FlagBlocked;
        break;
    case 2:
        nearest->flags |= AiPosition::FlagBlocked;
        break;
    default:
        break;
    }

    Switch(&nearest->flags, bits >> 2 & 0x3, s16{0x2});
    Switch(&nearest->flags, bits >> 4 & 0x3, s16{0x4});
}

// AgentRef1 the linked object (of a range: byte 0xC one past the first, 0xFF from the first; byte 0xD the end, 0xFF all) whose
// box's middle is nearest the player, marked taken; with two taken or more around, a free one next to the nearest one between
// them (or toward the free side)
void SetLinkedObjectNearestPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr s32 Near = 10;
    constexpr s32 FarSteps = 100;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* owner = node->owner;
    void* attachments = GetGameNode(NodesOf(owner), AttachmentsKind);
    InstanceContext* player = PlayerInstance();
    // Its own position worked out, which nothing reads
    PositionOf(owner);
    Vector4 target = PositionOf(player);
    s32 nearest = -1;
    f32 best = Far;
    u8 firstByte = static_cast<u8>(range);
    u8 endByte = static_cast<u8>(range >> 8);
    u32 first = firstByte != 0xFF ? firstByte - 1u : 0;
    u32 end = endByte != 0xFF ? endByte : LinkedCount(attachments);
    for (u32 index = first; index < end; index++)
    {
        const Box* box = &LinkedInstances(attachments)[index]->collision.box;
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
    else if (takenAfter != -1)
    {
        if (takenBefore == -1)
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
    else if (takenBefore != -1)
    {
        chosen = FindLinked(this, 0, static_cast<s32>((static_cast<u32>(nearest) - 1 + end) >> 1), -Near, attachments);
    }
    else
    {
        chosen = nearest;
    }

    if (chosen == -1)
    {
        node->agentRef1 = nullptr;
        return;
    }

    InstanceContext* instance = LinkedInstances(attachments)[chosen];
    if ((instance->flags & TakenFlag) != 0)
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

        instance = LinkedInstances(attachments)[nearest];
    }

    node->agentRef1 = instance;
    instance->flags |= TakenFlag;
}

// From a linked object on, the first (within the steps either way by their sign, not past either end) that's taken or free as
// asked: its index, -1 for none
s32 FindLinked(const SetLinkedObjectNearestPlayerCommand*, u32 taken, s32 index, s32 steps, void* attachments)
{
    s32 last = static_cast<s32>(LinkedCount(attachments)) - 1;
    bool backwards = steps < 1;
    s32 step = backwards ? -1 : 1;
    for (s32 done = 0;; done += step, index += step)
    {
        bool isTaken = (LinkedInstances(attachments)[index]->flags & TakenFlag) != 0;
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
    u8 firstByte = static_cast<u8>(command->range);
    u8 endByte = static_cast<u8>(command->range >> 8);
    u32 first = firstByte != 0xFF ? firstByte - 1u : 0;
    u32 end = endByte != 0xFF ? endByte : LinkedCount(attachments);
    u32 count = 0;
    for (u32 index = first; index < end; index++)
    {
        count += LinkedInstances(attachments)[index]->flags >> 8 & 1;
    }

    return count;
}

// The focus the linked object nearest the camera's view (within about 37 degrees of it, drawn) that's farther from the player
// than the player goes in a second
void LinkedObjectNearestPlayerOp637Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 InView = Rounded(0.8);
    ObjectNode* node = NodeOf(runner);
    InstanceContext* chosen = nullptr;
    f32 best = Far;
    void* attachments = GetGameNode(&node->owner->nodes, AttachmentsKind);
    Vector4 target = PositionOf(PlayerInstance());
    InstanceContext* camera = CameraInstance();
    auto* character = reinterpret_cast<Agent*>(g_PlayerCharacter);
    Vector4 velocity;
    CallVirtual<u32>(character, character->vtable, AgentVelocitySlot, &velocity);
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    f32 speedSquared = speed * speed;
    if (attachments != nullptr && camera != nullptr)
    {
        Vector4 eye = PositionOf(camera);
        ObjectPlace* place = camera->place;
        RotateAndTranslate(place);
        Vector4 view = *reinterpret_cast<const Vector4*>(place->matrix.m[2]);
        for (u32 index = 0; index < LinkedCount(attachments); index++)
        {
            InstanceContext* instance = LinkedInstances(attachments)[index];
            if (instance == nullptr || (instance->flags & ReferencedObject::FlagInDrawnCell) == 0)
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
            if (InView < way.x * view.x + way.y * view.y + way.z * view.z)
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
