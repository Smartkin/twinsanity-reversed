#include "game/agents.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/instancefactory.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/rigidbody.h"

extern "C"
{
    // An instance's model's joint's position (in the world when asked; still asm). Whether it has the joint
    u32 JointPosition(InstanceContext* instance, u32 joint, Vector4* position, u32 inWorld) RETAIL(FUN_0022e190);
}

namespace
{
// The holders' and parts' destructors, and an instance's vtable functions that wake it and put it to sleep
constexpr u32 HolderDestroySlot = 7;
constexpr u32 PartDestroySlot = 1;
constexpr u32 PartResetSlot = 2;
constexpr u32 WakeSlot = 2;
constexpr u32 SleepSlot = 3;
// The nodes an agent's functions use: the object node and the rigid body
constexpr u32 ObjectNodeKind = 1;
constexpr u32 RigidBodyKind = 5;
// The object node's function given a physical contact's sender and strength
constexpr u32 ObjectNodeContactSlot = 27;
// The events the agents tell their scripts: a collision or an attack of no other kind, a contact message, the crates' functions
// 4 and 5
constexpr u32 CollisionEvent = 3;
constexpr u32 ContactEvent = 2;
constexpr u32 CrateEvent = 0xB;
// An agent's function told contact messages, and the playable characters' agent node's kind
constexpr u32 AgentContactSlot = 9;
constexpr u32 CharacterNodeKind = 0xC;
// What an agent hits back with
constexpr u32 HitBackWord = 0x400;
// The attack the character's part's low byte and the attack's kind both are when a creature that may damage it hits back
constexpr u32 HitBackAttack = 3;
// Through which links creatures change chunks
constexpr u64 CreatureLinkBit = 0x40000;

// The script event of an attack's kind
u32 AttackScriptEvent(u32 kind)
{
    switch (kind)
    {
    case 4:
        return 5;
    case 5:
        return 4;
    case 6:
    case 10:
        return 6;
    case 7:
    case 9:
    case 11:
        return 7;
    case 8:
    case 12:
        return 8;
    case 13:
    case 14:
        return 0xA;
    default:
        return CollisionEvent;
    }
}

u32 ClockTime(InstanceContext* instance)
{
    return GetContextClock(instance)->time;
}
constexpr u16 NoId = 0xFFFF;
constexpr u32 CharacterCacheMask = 0x10;
constexpr u32 CharacterCenterJoint = 1;

static_assert(offsetof(ReferencedObject, collision) + offsetof(ObjectCollision, box) == 0x40);

EABI_IMPORT(FUN_00162098, ConstructGrapleOwned);

void SetBit(u32& bits, u32 bit, bool set)
{
    bits = set ? bits | bit : bits & ~bit;
}

void DestroyHolder(PropertyHolder* holder)
{
    CallVirtual<void>(holder, holder->vtable, HolderDestroySlot, u32{DestroyAndFree});
}

void DestroyPart(AgentPart* part)
{
    CallVirtual<void>(part, part->vtable, PartDestroySlot, u32{DestroyAndFree});
}

// The message kept (its point, word and reaction) and told the script
void KeepContact(Agent* agent, const ContactMessage* message, InstanceContext* sender)
{
    agent->contact.point = message->point;
    agent->contact.word = message->word;
    agent->contact.byte = message->byte;
    RunAgentEvent(agent, ContactEvent, reinterpret_cast<u32>(sender), 0, 0);
}
}

Agent* Agent::Construct(Agent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    agent->vtable = g_AgentVTable;
    agent->instance = creator->context;
    SetUndefinedId(&agent->spawnScript);
    agent->objectId = static_cast<u16>(creator->instance->objectId);
    agent->unknown1C &= ~1u;
    agent->properties = holder;
    agent->part = part;
    agent->object = creator->object;
    agent->id = NoId;
    agent->chunkIndex = NoId;
    ContactMessage::Construct(&agent->contact);
    GameResources* resources = G_GameResourcesObjectPointer;
    agent->ClearUnknown18();
    u16 objectId = agent->objectId;
    LoadObjectResources(agent->object->references, &objectId, resources);
    return agent;
}

void Agent::Destroy(u32 destroyFlags)
{
    vtable = g_AgentVTable;
    GameResources* resources = G_GameResourcesObjectPointer;
    u16 id = objectId;
    ReleaseObjectResources(object->references, &id, resources);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Agent::ClearUnknown18()
{
    unknown18[3] = 0;
    unknown18[0] = 0;
    unknown18[1] = 0;
    unknown18[2] = 0;
}

void Agent::Nothing6()
{
}

u32 Agent::Collided(void*, const Vector4*, const Vector4*)
{
    return 1;
}

void Agent::Nothing8()
{
}

void Agent::Contact(const ContactMessage*, InstanceContext*, u32)
{
}

u32 Agent::CanChangeChunk(ChunkData*, ChunkLinkData*)
{
    return 1;
}

u32 Agent::Slot12()
{
    return 0;
}

void Agent::Nothing13()
{
}

void Agent::CollisionCenter(Vector4* center)
{
    const Box* box = instance->CollisionBox();
    *center = box->max;
    center->x = (center->x - box->min.x) * 0.5f + box->min.x;
    center->y = (center->y - box->min.y) * 0.5f + box->min.y;
    center->z = (center->z - box->min.z) * 0.5f + box->min.z;
}

void Agent::Nothing15()
{
}

void Agent::Nothing16()
{
}

void Agent::Nothing17()
{
}

void Agent::Nothing18()
{
}

BasicAgent* BasicAgent::Construct(BasicAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    Agent::Construct(agent, creator, holder, part);
    agent->vtable = g_BasicAgentVTable;
    return agent;
}

void BasicAgent::Destroy(u32 destroyFlags)
{
    vtable = g_BasicAgentVTable;
    if (properties != nullptr)
    {
        DestroyHolder(properties);
    }

    if (part != nullptr)
    {
        DestroyPart(part);
    }

    Agent::Destroy(destroyFlags);
}

void BasicAgent::ApplyState(u32 unknown)
{
    PropertyHolder* holder = properties;
    auto* basicPart = static_cast<BasicAgentPart*>(part);
    CallVirtual<u32>(instance, instance->vtable, (holder->state & StateDeactivated) != 0 ? SleepSlot : WakeSlot);
    SetBit(instance->flags, ReferencedObject::FlagSphereContact, (holder->state & StateCollisionActive) != 0);
    SetBit(instance->flags, ReferencedObject::FlagVisible, (holder->state & StateVisible) != 0);
    SetBit(instance->flags, ReferencedObject::FlagShadow, (holder->state & StateShadowActive) != 0);
    CallVirtual<void>(basicPart, basicPart->vtable, PartResetSlot, unknown);
    SetBit(instance->flags, ReferencedObject::FlagTriggerSignals, (holder->state & StateReceivesTriggerSignals) != 0);
    SetBit(basicPart->bits, BasicAgentPart::CanDamageCharacter, (holder->state & StateCanDamageCharacter) != 0);
    SetBit(basicPart->bits, BasicAgentPart::Targettable, (holder->state & StateTargettable) != 0);
    SetBit(basicPart->bits, BasicAgentPart::CanAlwaysDamageCharacter, (holder->state & StateCanAlwaysDamageCharacter) != 0);
    SetBit(basicPart->bits, BasicAgentPart::BulletsBounceBack, (holder->state & StateBulletsBounceBack) != 0);
}

u32 BasicAgent::Slot3()
{
    return 0;
}

void BasicAgent::Nothing4()
{
}

void BasicAgent::Nothing5()
{
}

void BasicAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32 physical)
{
    if (physical != 0 && (instance->flags & ReferencedObject::FlagPhysicsBody) != 0)
    {
        auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
        CallVirtual<void>(node, node->vtable, ObjectNodeContactSlot, sender, message->point.w);
    }

    if (message->byte != 0)
    {
        KeepContact(this, message, sender);
    }
}

void BasicAgent::Nothing19()
{
}

void BasicAgent::Attacked(const AttackEvent* event, InstanceContext* sender)
{
    auto* basicPart = static_cast<BasicAgentPart*>(part);
    if ((instance->flags & ReferencedObject::FlagTriggerSignals) != 0 &&
        (basicPart->bits & BasicAgentPart::CanAlwaysDamageCharacter) == 0)
    {
        u32 kind = event->kind;
        u32 scriptEvent = AttackScriptEvent(kind);
        if (basicPart->HitBy(kind))
        {
            u32 time = ClockTime(instance);
            basicPart->RecordAttack(kind, &time);
            RunAgentEvent(this, scriptEvent, reinterpret_cast<u32>(sender), 0, 0);
        }
    }

    if ((basicPart->bits & BasicAgentPart::CanDamageCharacter) == 0)
    {
        return;
    }

    if ((basicPart->bits & BasicAgentPart::CanAlwaysDamageCharacter) != 0)
    {
        HitBack(sender);
        return;
    }

    u32 kind = event->kind;
    u32 time = ClockTime(instance);
    basicPart->RecordAttack(kind, &time);
    auto* node = static_cast<AgentNode*>(GetGameNode(&sender->nodes, CharacterNodeKind));
    auto* attacker = static_cast<BasicAgentPart*>(node->agent->part);
    if ((attacker->bits & BasicAgentPart::LowByteMask) == HitBackAttack && kind == HitBackAttack)
    {
        HitBack(sender);
    }
}

void BasicAgent::HitBack(InstanceContext* target)
{
    AgentNode* node = AgentNodeOf(target);
    if (node == nullptr)
    {
        return;
    }

    Agent* other = node->agent;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    ContactMessage message;
    message.point = place->position;
    message.word = HitBackWord;
    message.byte = 1;
    message.point.w = 0.0f;
    CallVirtual<void>(other, other->vtable, AgentContactSlot, &message, instance, 1u);
}

void BasicAgent::Push(InstanceContext* other)
{
    auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
    CallVirtual<void>(node, node->vtable, ObjectNodeContactSlot, other, 0.0f);
}

void BasicAgent::Launch(const Vector4* velocity)
{
    auto* body = static_cast<RigidBody*>(GetGameNode(&instance->nodes, RigidBodyKind));
    if (body != nullptr)
    {
        body->SetVelocity(velocity);
    }
}

// The basic agent's construction inline
PickupAgent* PickupAgent::Construct(PickupAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    Agent::Construct(agent, creator, holder, part);
    agent->vtable = g_PickupAgentVTable;
    return agent;
}

void PickupAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

void PickupAgent::ApplyState(u32 unknown)
{
    BasicAgent::ApplyState(unknown);
}

void PickupAgent::Nothing8()
{
}

void PickupAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32)
{
    KeepContact(this, message, sender);
}

u32 PickupAgent::Position(Vector4*)
{
    return 0;
}

void PickupAgent::Frame(TimeClock*)
{
}

CrateAgent* CrateAgent::Construct(CrateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->vtable = g_CrateAgentVTable;
    return agent;
}

void CrateAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

u32 CrateAgent::Slot3()
{
    return 1;
}

void CrateAgent::Slot4()
{
    auto* cratePart = static_cast<CratePart*>(part);
    if ((cratePart->value & 2) != 0 || (cratePart->value & 1) != 0)
    {
        return;
    }

    launchSpeed = 0.0f;
    bits = (bits & ~(StateMask << StateShift)) | StateLaunched << StateShift;
    RunAgentEvent(this, CrateEvent, 0, 0, 0);
}

void CrateAgent::Slot5()
{
    RunAgentEvent(this, CrateEvent, 0, 0, 0);
}

void CrateAgent::Nothing8()
{
}

void CrateAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32 physical)
{
    BasicAgent::Contact(message, sender, physical);
}

u32 CrateAgent::Position(Vector4*)
{
    return 0;
}

void CrateAgent::Launch(const Vector4* velocity)
{
    launchSpeed = velocity->y;
    bits = (bits & ~(StateMask << StateShift)) | StateLaunched << StateShift;
}

CreatureAgent* CreatureAgent::Construct(CreatureAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                        AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->vtable = g_CreatureAgentVTable;
    agent->position = g_DefaultBox.min;
    agent->position.w = 1.0f;
    return agent;
}

void CreatureAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

void CreatureAgent::Nothing8()
{
}

void CreatureAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32 physical)
{
    auto* basicPart = static_cast<BasicAgentPart*>(part);
    if (physical != 0 && (instance->flags & ReferencedObject::FlagPhysicsBody) != 0)
    {
        Push(sender);
    }

    if ((basicPart->bits & BasicAgentPart::CanAlwaysDamageCharacter) == 0)
    {
        KeepContact(this, message, sender);
    }
}

u32 CreatureAgent::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((*reinterpret_cast<const u64*>(link) & CreatureLinkBit) == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &position, 0);
    return 1;
}

u32 CreatureAgent::Position(Vector4*)
{
    return 0;
}

void CreatureAgent::Attacked(const AttackEvent* event, InstanceContext* sender)
{
    BasicAgent::Attacked(event, sender);
}

void CreatureAgent::Slot23()
{
    constexpr u32 PartFlag4 = 0x4;
    constexpr u32 ClearPartFlagSlot = 25;
    if ((static_cast<CreaturePart*>(part)->unknown14 & PartFlag4) != 0)
    {
        CallVirtual<void>(this, vtable, ClearPartFlagSlot, 1u);
    }
}

void CreatureAgent::SetPartFlag()
{
    static_cast<CreaturePart*>(part)->unknown14 |= PartFlag20;
}

void CreatureAgent::ClearPartFlag()
{
    static_cast<CreaturePart*>(part)->unknown14 &= ~PartFlag20;
}

void CharacterWords::Clear()
{
    for (u32 index = 1; index < 8; index++)
    {
        words[index] = 0;
    }

    words[0] = 0;
}

CharacterAgent* CharacterAgent::Construct(CharacterAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                          AgentPart* part)
{
    CreatureAgent::Construct(agent, creator, holder, part);
    for (void*& controller : agent->controllers)
    {
        controller = nullptr;
    }

    agent->vtable = g_CharacterAgentVTable;
    agent->words.Clear();
    CharacterReferences::Construct(&agent->references, nullptr);
    agent->handle200 = nullptr;
    agent->handle290 = nullptr;
    agent->handle298 = nullptr;
    ConstructCollisionCache(&agent->cache, agent->instance, CharacterCacheMask);
    SetUpCharacter(agent, 1);
    agent->controllers[6] = nullptr;
    return agent;
}

void CharacterAgent::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterAgentVTable;
    TearDownCharacter(this, 1);
    DestroyCollisionCache(&cache, DestroyOnly);
    RemoveReference(&handle298);
    RemoveReference(&handle290);
    RemoveReference(&handle200);
    references.Destroy(DestroyOnly);
    BasicAgent::Destroy(destroyFlags);
}

u32 CharacterAgent::Position(Vector4* out)
{
    *out = position;
    return 1;
}

u32 CharacterAgent::Slot12()
{
    return 1;
}

void CharacterAgent::CollisionCenter(Vector4* center)
{
    JointPosition(instance, CharacterCenterJoint, center, 0);
}

GenericObjectAgent* GenericObjectAgent::Construct(GenericObjectAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                                  AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->vtable = g_GenericObjectAgentVTable;
    return agent;
}

void GenericObjectAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

u32 GenericObjectAgent::Collided(void* other, const Vector4*, const Vector4*)
{
    if ((static_cast<BasicAgentPart*>(part)->bits & BasicAgentPart::HitByKind3) == 0)
    {
        return 0;
    }

    RunAgentEvent(this, CollisionEvent, reinterpret_cast<u32>(other), 0, 0);
    return 1;
}

void GenericObjectAgent::Nothing8()
{
}

u32 GenericObjectAgent::Position(Vector4*)
{
    return 0;
}

void GenericObjectAgent::Attacked(const AttackEvent* event, InstanceContext* sender)
{
    BasicAgent::Attacked(event, sender);
}

void GenericObjectAgent::Frame(TimeClock*)
{
}

GrabbableAgent* GrabbableAgent::Construct(GrabbableAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                          AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->vtable = g_GrabbableAgentVTable;
    return agent;
}

void GrabbableAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

u32 GrabbableAgent::Position(Vector4*)
{
    return 0;
}

void GrabbableAgent::Frame(TimeClock*)
{
}

// The basic agent's construction inline
PayGateAgent* PayGateAgent::Construct(PayGateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    Agent::Construct(agent, creator, holder, part);
    agent->vtable = g_PayGateAgentVTable;
    return agent;
}

void PayGateAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

u32 PayGateAgent::Position(Vector4*)
{
    return 0;
}

void PayGateAgent::Frame(TimeClock*)
{
}

GrapleAgent* GrapleAgent::Construct(GrapleAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->owned = nullptr;
    agent->vtable = g_GrapleAgentVTable;
    return agent;
}

void GrapleAgent::ApplyState(u32)
{
    constexpr u32 SizeOfOwned = 0x120;
    if (owned != nullptr)
    {
        return;
    }

    owned = ConstructGrapleOwned(1.0f, MemoryAllocate(SizeOfOwned), instance);
    SetGrapleOwnedValue(owned, 0);
}

void GrapleAgent::Destroy(u32 destroyFlags)
{
    vtable = g_GrapleAgentVTable;
    if (owned != nullptr)
    {
        CallVirtual<void>(owned, owned->vtable, 1, u32{DestroyAndFree});
    }

    BasicAgent::Destroy(destroyFlags);
}

u32 GrapleAgent::Position(Vector4*)
{
    return 0;
}

void GrapleAgent::Frame(TimeClock* clock)
{
    if ((instance->flags & ReferencedObject::FlagVisible) != 0)
    {
        GrapleFrame(this, clock);
    }
}

void ProjectileAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

void ProjectileAgent::ApplyState(u32 unknown)
{
    BasicAgent::ApplyState(unknown);
    instance->flags |= ReferencedObject::FlagProjectile;
}

void ProjectileAgent::Nothing8()
{
}

void ProjectileAgent::Contact(const ContactMessage*, InstanceContext*, u32)
{
}

u32 ProjectileAgent::Position(Vector4*)
{
    return 0;
}

void ProjectileAgent::Frame(TimeClock*)
{
}
