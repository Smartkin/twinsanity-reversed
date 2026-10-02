#include "game/instances.h"

#include "game/agentparts.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
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

    // The agent given the game's resources when its node gets its instance, and its step with them (still asm)
    void AgentTakeResources(Agent* agent, GameResources* resources) RETAIL(FUN_00263390);
    void AgentStep(Agent* agent, GameResources* resources, TimeClock* clock, u32 unknown) RETAIL(FUN_00263428);

    // The vtable functions: 1 an event handled (the agent's when it's of type 0x1801), 2 the destructor (the agent deleted with
    // it; the kinds' are copies of the base's but the character's, grabbables' and projectiles'), 3 given its instance (the agent
    // given the game's resources), 4 whether its instance may change chunks (the agent's say), 5 its kind, 7 a step (the
    // agent's), 8 its update (the agent's frame when its clock runs; its instance's bit 9 cleared once its chunk lacks bit 23),
    // 10 its type
    void AgentNodeHandleEvent(AgentNode* node, Reference** event) RETAIL(HandleEvent);
    void AgentNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017a9c0);
    void AgentNodeSetOwner(AgentNode* node, InstanceContext* instance) RETAIL(FUN_0017aa30);
    u32 AgentNodeCanChangeChunk(AgentNode* node, ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0017ab60);
    void AgentNodeStep(AgentNode* node, TimeClock* clock, u32 unknown) RETAIL(FUN_0017aa78);
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
    void PayGateNodeStep(AgentNode* node, TimeClock* clock, u32 unknown) RETAIL(FUN_0017b0a8);
    u32 PayGateNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017AF60);
    u32 PayGateNodeType(AgentNode* node) RETAIL(FUN_0017af68);
    void GrapleNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0017ae28);
    u32 GrapleNodeKind(AgentNode* node) RETAIL(GetNodeIndex_0017ADD0);
    u32 GrapleNodeType(AgentNode* node) RETAIL(FUN_0017add8);
    void ProjectileNodeDestroy(AgentNode* node, u32 flags) RETAIL(FUN_0013f140);
    u32 ProjectileNodeUpdate(AgentNode* node, TimeClock* clock) RETAIL(FUN_0013f168);
    u32 ProjectileNodeKind(AgentNode* node) RETAIL(GetNodeIndex);
    u32 ProjectileNodeType(AgentNode* node) RETAIL(FUN_0013f178);
}

namespace
{
// The events the agent handles itself
constexpr u16 AgentEventType = 0x1801;
// The agent's vtable functions: the destructor, whether its instance may change chunks, an event of its type and its frame
constexpr u32 AgentDestroySlot = 2;
constexpr u32 AgentCanChangeChunkSlot = 10;
constexpr u32 AgentEventSlot = 20;
constexpr u32 AgentFrameSlot = 22;
// An instance's bit 9 (every instance the factory makes has it) and its chunk's bit 23
constexpr u32 InstanceBit9 = 0x200;
constexpr u32 ChunkBit23 = 0x800000;
// The clock runs
constexpr u32 ClockRunning = 0x1;
// The pay gates' number: their third integer, into the low 12 bits of their part's word 0x14 bytes in
constexpr u32 PayGateNumberIndex = 2;
constexpr u32 PayGateNumberMask = 0xFFF;

// The kinds and their types
constexpr u32 CharacterKind = 0xC;
constexpr u32 CrateKind = 0xD;
constexpr u32 PickupKind = 0xE;
constexpr u32 CreatureKind = 0xF;
constexpr u32 GenericObjectKind = 0x10;
constexpr u32 GrabbableKind = 0x11;
constexpr u32 PayGateKind = 0x12;
constexpr u32 GrapleKind = 0x13;
constexpr u32 ProjectileKind = 0x14;
constexpr u32 BaseType = 0x1423;
constexpr u32 CharacterType = 0x9002;
constexpr u32 CrateType = 0x9003;
constexpr u32 CreatureType = 0x9004;
constexpr u32 PickupType = 0x9005;
constexpr u32 GrabbableType = 0x9008;
constexpr u32 GenericObjectType = 0x9009;
constexpr u32 PayGateType = 0x900A;
constexpr u32 GrapleType = 0x900B;
constexpr u32 ProjectileType = 0x900C;

AgentNode* ConstructWith(AgentNode* node, Agent* agent, const GccVTableEntry* vtable)
{
    GameNode::Construct(node);
    node->agent = agent;
    node->vtable = vtable;
    return node;
}

GameResources* GameResourcesOf()
{
    return G_GameController_00309890->resources;
}

void GivePayGateNumber(AgentNode* node)
{
    Agent* agent = node->agent;
    s32 number = agent->properties->GetInt(PayGateNumberIndex);
    auto* part = static_cast<PayGatePart*>(agent->part);
    part->value = (part->value & ~PayGateNumberMask) | (static_cast<u32>(number) & PayGateNumberMask);
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

// The event's sender (what its object's 0xC bytes point at) goes with an event the agent handles; another's reference is taken
// for the game node's handling. The event is let go of either way
void AgentNodeHandleEvent(AgentNode* node, Reference** event)
{
    Agent* agent = node->agent;
    Reference* reference = *event;
    auto* object = reference != nullptr ? reinterpret_cast<u8*>(reference->object) : nullptr;
    auto* sender = *reinterpret_cast<void***>(object + 0xC);
    void* senderObject = sender != nullptr ? *sender : nullptr;
    auto* type = reinterpret_cast<u16*>(object + 4);
    if (*type == AgentEventType)
    {
        CallVirtual<void>(agent, agent->vtable, AgentEventSlot, reference->object, senderObject);
    }
    else
    {
        Reference* taken = reference;
        if (reference != nullptr)
        {
            u32 count = ((reference->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask;
            reference->value = (reference->value & ~ReferenceBits::CountMask) | count;
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
        CallVirtual<void>(agent, agent->vtable, AgentDestroySlot, DestroyAndFree);
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
    return CallVirtual<u32>(agent, agent->vtable, AgentCanChangeChunkSlot, from, link);
}

void AgentNodeStep(AgentNode* node, TimeClock* clock, u32 unknown)
{
    AgentStep(node->agent, GameResourcesOf(), clock, unknown);
    node->time = clock->time;
}

u32 AgentNodeUpdate(AgentNode* node, TimeClock* clock)
{
    InstanceContext* instance = node->owner;
    ChunkData* chunk = instance->chunk;
    Agent* agent = node->agent;
    if (chunk != nullptr && (chunk->bits & ChunkBit23) == 0)
    {
        instance->flags &= ~InstanceBit9;
    }

    if ((*reinterpret_cast<u8*>(&clock->flags) & ClockRunning) != 0)
    {
        CallVirtual<void>(agent, agent->vtable, AgentFrameSlot, clock);
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
    return CharacterKind;
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
    return CrateKind;
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
    return PickupKind;
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
    return CreatureKind;
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
    return GenericObjectKind;
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
    return GrabbableKind;
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

void PayGateNodeStep(AgentNode* node, TimeClock* clock, u32 unknown)
{
    AgentNodeStep(node, clock, unknown);
    GivePayGateNumber(node);
}

u32 PayGateNodeKind(AgentNode*)
{
    return PayGateKind;
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
    return GrapleKind;
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
    return ProjectileKind;
}

u32 ProjectileNodeType(AgentNode*)
{
    return ProjectileType;
}
