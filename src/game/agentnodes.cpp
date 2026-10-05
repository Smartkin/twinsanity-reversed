#include "game/instances.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"

// The nodes of an instance's agent (kinds 0xC to 0x14): a game node and the agent it runs. The kinds' classes differ in their
// kind and type numbers; the pay gates' also give their part a number of their properties when they get their instance and every
// step, and the projectiles' aren't updated

extern "C"
{
    // The vtables: the base's, a game node class between the base and the game node's, and the kinds'
    extern const GccVTableEntry g_AgentNodeVTable[] RETAIL(InstanceNodeBase_Methods);
    extern const GccVTableEntry g_AgentNodeBaseVTable[] RETAIL(D_002F54B8);
    extern const GccVTableEntry g_CharacterNodeVTable[] RETAIL(PlayableCharacterNode_Methods);
    extern const GccVTableEntry g_CrateNodeVTable[] RETAIL(UnkNode_0xD_Methods);
    extern const GccVTableEntry g_PickupNodeVTable[] RETAIL(UnkNode_0xE_Methods);
    extern const GccVTableEntry g_CreatureNodeVTable[] RETAIL(UnkNode_0xF_Methods);
    extern const GccVTableEntry g_GenericObjectNodeVTable[] RETAIL(UnkNode_0x10_Methods);
    extern const GccVTableEntry g_GrabbableNodeVTable[] RETAIL(UnkNode_0x11_Methods);
    extern const GccVTableEntry g_PayGateNodeVTable[] RETAIL(UnkNode_0x12_Methods);
    extern const GccVTableEntry g_GrapleNodeVTable[] RETAIL(UnkNode_0x13_Methods);
    extern const GccVTableEntry g_ProjectileNodeVTable[] RETAIL(UnkNode_0x14_Methods);

    // The vtable functions: 1 an event handled (the agent's when it's an attack), 2 the destructor (the agent deleted with it;
    // the kinds' are copies of the base's but the character's, grabbables' and projectiles'), 3 given its instance (the agent
    // given the game's resources), 4 whether its instance may change chunks (the agent's say), 5 its kind, 7 a step (the
    // agent's), 8 its update (the agent's frame when its clock runs; its instance no longer in a drawn cell once its chunk's
    // drawn cells hold none of its instances), 10 its type
    void AgentNodeHandleEvent(AgentNode* node, Reference** event) RETAIL(HandleEvent);
    void AgentNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017a9c0);
    void AgentNodeSetOwner(AgentNode* node, InstanceContext* instance) RETAIL(FUN_0017aa30);
    u32 AgentNodeCanChangeChunk(AgentNode* node, ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0017ab60);
    void AgentNodeStep(AgentNode* node, TimeClock* clock, u32 resetEntry) RETAIL(FUN_0017aa78);
    u32 AgentNodeUpdate(AgentNode* node, TimeClock* clock) RETAIL(FUN_0017aac8);
    u32 AgentNodeType(AgentNode* node) RETAIL(FUN_00179a08);
    void CharacterNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017c870);
    u32 CharacterNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0xc);
    u32 CharacterNodeType(AgentNode* node) RETAIL(FUN_0017c898);
    void CrateNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017ac98);
    u32 CrateNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017A750);
    u32 CrateNodeType(AgentNode* node) RETAIL(FUN_0017a758);
    void PickupNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017b188);
    u32 PickupNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017B130);
    u32 PickupNodeType(AgentNode* node) RETAIL(FUN_0017b138);
    void CreatureNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017ad60);
    u32 CreatureNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017AD08);
    u32 CreatureNodeType(AgentNode* node) RETAIL(FUN_0017ad10);
    void GenericObjectNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017aef0);
    u32 GenericObjectNodeKind(AgentNode* node) RETAIL(FUN_0017ae98);
    u32 GenericObjectNodeType(AgentNode* node) RETAIL(FUN_0017aea0);
    void GrabbableNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017c8a0);
    u32 GrabbableNodeKind(AgentNode* node) RETAIL(FUN_0017c8c0);
    u32 GrabbableNodeType(AgentNode* node) RETAIL(FUN_0017c8c8);
    void PayGateNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017afb8);
    void PayGateNodeSetOwner(AgentNode* node, InstanceContext* instance) RETAIL(FUN_0017b028);
    void PayGateNodeStep(AgentNode* node, TimeClock* clock, u32 resetEntry) RETAIL(FUN_0017b0a8);
    u32 PayGateNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017AF60);
    u32 PayGateNodeType(AgentNode* node) RETAIL(FUN_0017af68);
    void GrapleNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017ae28);
    u32 GrapleNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017ADD0);
    u32 GrapleNodeType(AgentNode* node) RETAIL(FUN_0017add8);
    void ProjectileNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0013f140);
    u32 ProjectileNodeUpdate(AgentNode* node, TimeClock* clock) RETAIL(FUN_0013f168);
    u32 ProjectileNodeKind(AgentNode* node) RETAIL(GetNodeIndex);
    u32 ProjectileNodeType(AgentNode* node) RETAIL(FUN_0013f178);
    // The game's nodes' base (D_002F54B8, between the game node and the agents', controls' and follow nodes): its destructor
    // and its type
    void GameNodeBaseDestroy(GameNode* node, u32 flags) RETAIL(FUN_00179568);
    u32 GameNodeBaseType(GameNode* node) RETAIL(FUN_00179590);
}

namespace
{
// The pay gates' number: their third integer, given to their part
constexpr u32 PayGateNumberIndex = 2;

// The agents' nodes' types
constexpr u32 CharacterType = 0x9002;
constexpr u32 CrateType = 0x9003;
constexpr u32 CreatureType = 0x9004;
constexpr u32 PickupType = 0x9005;
constexpr u32 GrabbableType = 0x9008;
constexpr u32 GenericObjectType = 0x9009;
constexpr u32 PayGateType = 0x900A;
constexpr u32 GrapleType = 0x900B;
constexpr u32 ProjectileType = 0x900C;
// The types of the game nodes' base and of the agents' nodes' base
constexpr u32 GameNodeType = 0x9001;
constexpr u32 BaseType = 0x1423;
// A grabbable's first integer: 1 a hook (else how many points it can be landed on from)
constexpr u32 GrabbableKindIndex = 0;
constexpr s32 HookGrabbable = 1;

AgentNode* ConstructWith(AgentNode* node, Agent* agent, const GccVTableEntry* vtable)
{
    GameNode::Construct(node);
    node->agent = agent;
    node->vtable = vtable;
    return node;
}

GameResources* GameResourcesOf()
{
    return g_AgentNodesGameController->resources;
}

void GivePayGateNumber(AgentNode* node)
{
    Agent* agent = node->agent;
    s32 number = agent->properties->GetInt(PayGateNumberIndex);
    auto* part = static_cast<PayGatePart*>(agent->part);
    part->payGate.number = number;
}
}

AgentNode* ConstructAgentNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_AgentNodeVTable);
}

AgentNode* ConstructCharacterNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_CharacterNodeVTable);
}

AgentNode* ConstructCrateNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_CrateNodeVTable);
}

AgentNode* ConstructPickupAgentNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_PickupNodeVTable);
}

AgentNode* ConstructCreatureNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_CreatureNodeVTable);
}

AgentNode* ConstructGenericObjectNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_GenericObjectNodeVTable);
}

AgentNode* ConstructGrabbableNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_GrabbableNodeVTable);
}

AgentNode* ConstructPayGateNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_PayGateNodeVTable);
}

AgentNode* ConstructGrapleNode(AgentNode* node, Agent* agent)
{
    return ConstructWith(node, agent, g_GrapleNodeVTable);
}

// An attack goes to the agent with its sender (the event's argument); another event's reference is taken for the game node's
// handling. The event is let go of either way
void AgentNodeHandleEvent(AgentNode* node, Reference** event)
{
    Agent* agent = node->agent;
    Reference* reference = *event;
    auto* handled = reference != nullptr ? reinterpret_cast<GameEvent*>(reference->object) : nullptr;
    Reference* sender = handled->argument;
    ReferencedObject* senderObject = sender != nullptr ? sender->object : nullptr;
    if (handled->id == AttackEvent::EventId)
    {
        CallVirtual<void>(agent, agent->vtable, Agent::AttackedSlot, reference->object, senderObject);
    }
    else
    {
        Reference* taken = reference;
        if (reference != nullptr)
        {
            reference->bits.count++;
        }

        node->HandleEvent(&taken);
    }

    ReleaseEvent(event);
}

void AgentNodeDestroy(AgentNode* node, u32 flags)
{
    node->vtable = g_AgentNodeVTable;
    Agent* agent = node->agent;
    if (agent != nullptr)
    {
        CallVirtual<void>(agent, agent->vtable, Agent::DestroySlot, DestroyAndFree);
    }

    node->vtable = g_AgentNodeBaseVTable;
    node->Destroy(flags);
}

void AgentNodeSetOwner(AgentNode* node, InstanceContext* instance)
{
    GameResources* resources = GameResourcesOf();
    Agent* agent = node->agent;
    node->SetOwner(instance);
    AgentTakeResources(agent, resources);
}

u32 AgentNodeCanChangeChunk(AgentNode* node, ChunkData* from, ChunkLinkData* link)
{
    Agent* agent = node->agent;
    return CallVirtual<u32>(agent, agent->vtable, Agent::CanChangeChunkSlot, from, link);
}

void AgentNodeStep(AgentNode* node, TimeClock* clock, u32 resetEntry)
{
    AgentStep(node->agent, GameResourcesOf(), clock, resetEntry);
    node->time = clock->time;
}

u32 AgentNodeUpdate(AgentNode* node, TimeClock* clock)
{
    InstanceContext* instance = node->owner;
    ChunkData* chunk = instance->chunk;
    Agent* agent = node->agent;
    if (chunk != nullptr && chunk->flags.drawn == 0)
    {
        instance->flags.inDrawnCell = 0;
    }

    if (clock->flags.running != 0)
    {
        CallVirtual<void>(agent, agent->vtable, Agent::FrameSlot, clock);
    }

    return node->Update(clock);
}

u32 AgentNodeType(AgentNode*)
{
    return BaseType;
}

void CharacterNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 CharacterNodeKind(AgentNode*)
{
    return NodeCharacter;
}

u32 CharacterNodeType(AgentNode*)
{
    return CharacterType;
}

void CrateNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 CrateNodeKind(AgentNode*)
{
    return NodeCrate;
}

u32 CrateNodeType(AgentNode*)
{
    return CrateType;
}

void PickupNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 PickupNodeKind(AgentNode*)
{
    return NodePickup;
}

u32 PickupNodeType(AgentNode*)
{
    return PickupType;
}

void CreatureNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 CreatureNodeKind(AgentNode*)
{
    return NodeCreature;
}

u32 CreatureNodeType(AgentNode*)
{
    return CreatureType;
}

void GenericObjectNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 GenericObjectNodeKind(AgentNode*)
{
    return NodeGenericObject;
}

u32 GenericObjectNodeType(AgentNode*)
{
    return GenericObjectType;
}

void GrabbableNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 GrabbableNodeKind(AgentNode*)
{
    return NodeGrabbable;
}

u32 GrabbableNodeType(AgentNode*)
{
    return GrabbableType;
}

void PayGateNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

void PayGateNodeSetOwner(AgentNode* node, InstanceContext* instance)
{
    AgentNodeSetOwner(node, instance);
    GivePayGateNumber(node);
}

void PayGateNodeStep(AgentNode* node, TimeClock* clock, u32 resetEntry)
{
    AgentNodeStep(node, clock, resetEntry);
    GivePayGateNumber(node);
}

u32 PayGateNodeKind(AgentNode*)
{
    return NodePayGate;
}

u32 PayGateNodeType(AgentNode*)
{
    return PayGateType;
}

void GrapleNodeDestroy(AgentNode* node, u32 flags)
{
    AgentNodeDestroy(node, flags);
}

u32 GrapleNodeKind(AgentNode*)
{
    return NodeGraple;
}

u32 GrapleNodeType(AgentNode*)
{
    return GrapleType;
}

void ProjectileNodeDestroy(AgentNode* node, u32 flags)
{
    node->vtable = g_ProjectileNodeVTable;
    AgentNodeDestroy(node, flags);
}

u32 ProjectileNodeUpdate(AgentNode*, TimeClock*)
{
    return 0;
}

u32 ProjectileNodeKind(AgentNode*)
{
    return NodeProjectile;
}

u32 ProjectileNodeType(AgentNode*)
{
    return ProjectileType;
}

void GameNodeBaseDestroy(GameNode* node, u32 flags)
{
    node->vtable = g_AgentNodeBaseVTable;
    node->Destroy(flags);
}

u32 GameNodeBaseType(GameNode*)
{
    return GameNodeType;
}

u32 IsHookGrabbable(AgentNode* node)
{
    return node->agent->properties->GetInt(GrabbableKindIndex) == HookGrabbable;
}

Vector4* GrabbableLandingPoint(AgentNode* node, ObjectPlace* place)
{
    // The first integer: 1 a hook, else how many of its object node's waypoints' first keys it can be landed on from (0 one)
    s32 kind = node->agent->properties->GetInt(GrabbableKindIndex);
    if (kind == HookGrabbable)
    {
        return nullptr;
    }

    Waypoints* waypoints = static_cast<ObjectNode*>(GetGameNode(&node->owner->nodes, NodeObject))->waypoints;
    if (waypoints == nullptr)
    {
        return nullptr;
    }

    u32 count = kind != 0 ? static_cast<u32>(kind) : 1;
    if (waypoints->keyCount < count)
    {
        return nullptr;
    }

    // The point the place faces most (its z axis, from its translation)
    const Matrix4x4& matrix = place->matrix;
    Vector4* landing = nullptr;
    f32 best = -Infinite;
    for (u32 index = 0; index < count; index++)
    {
        LayoutPosition* position = index < waypoints->keyCount ? waypoints->positions.data[index] : nullptr;
        Vector4 direction = position->position;
        direction.x -= matrix.m[3][0];
        direction.y -= matrix.m[3][1];
        direction.z -= matrix.m[3][2];
        direction.w = 1.0f;
        f32 inverse = InverseLength(&direction, LengthEpsilon);
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
        f32 facing = direction.x * matrix.m[2][0] + direction.y * matrix.m[2][1] + direction.z * matrix.m[2][2];
        if (best < facing)
        {
            best = facing;
            landing = &position->position;
        }
    }

    return landing;
}
