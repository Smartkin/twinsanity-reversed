#include "game/agents.h"

#include "game/attachments.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/colour.h"
#include "game/controllers.h"
#include "game/hull.h"
#include "game/instancefactory.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/resources.h"
#include "game/rigidbody.h"
#include "game/shadows.h"

extern "C"
{
    // An instance's model's joint's position (in the world when asked). Whether it has the joint

    // The header's constants the start-up sets for this file, which nothing reads: up (0, 1, 0, 1), 0.01 twice, a black of half
    // alpha, 0 and 45 degrees (65536ths)
    extern Vector4 g_AgentsUp RETAIL(D_0030BAD0);
    extern f32 g_AgentsSmall RETAIL(D_0030A4E8);
    extern f32 g_AgentsSmall2 RETAIL(D_0030A4EC);
    extern u32 g_AgentsShade RETAIL(D_0030A4F0);
    extern u32 g_AgentsZero RETAIL(D_0030A4E0);
    extern s32 g_AgentsAngle45 RETAIL(D_0030A4F8);
}

namespace
{
// The holders' and parts' destructors, and an instance's vtable functions that wake it and put it to sleep
constexpr u32 HolderDestroySlot = 7;
constexpr u32 PartDestroySlot = 1;
constexpr u32 PartResetSlot = 2;
constexpr u32 WakeSlot = 2;
constexpr u32 SleepSlot = 3;
// The nodes an agent's functions use: the object node, the model's node and the rigid body
constexpr u32 ObjectNodeKind = 1;
constexpr u32 ModelNodeKind = 3;
constexpr u32 RigidBodyKind = 5;
// An agent's vtable function applying its instance's state flags
constexpr u32 ApplyStateSlot = 1;
// Bit 0 of an agent's word at 0x1C: its events start nothing (cleared as it takes the resources and steps)
constexpr u32 EventsOff = 0x1;
// The behaviour slot of its object an agent starts with when it has no spawn starter
constexpr u32 StartSlot = 0;
// The nodes a behaviour's script event goes to (the object nodes, kind 1: a bit each)
constexpr u32 ObjectNodeKinds = 0x2;
// An exit point of none: the instance attached without one
constexpr u8 NoExitPoint = 0xFF;
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
// A hard collision (or a crate's breaking one); what a crate falling onto an agent tells it: landed on (an agent that can be
// stood on) or slammed (one it goes through); a crate's landing after its launch
constexpr u32 HardCollisionEvent = 2;
constexpr u32 LandedOnEvent = 5;
constexpr u32 SlammedEvent = 7;
constexpr u32 CrateLandedEvent = 0xC;
// Its agent's state bits: a crate stays whole, a crate landing on it stands on it, a creature snaps to the ground
constexpr u32 StateUnbreakable = 0x800;
constexpr u32 StateCanBeStoodOn = 0x400;
constexpr u32 StateSnapsToGround = 0x40000;
// A crate's part's value: in the air (launched and not landed), resting on the ground
constexpr u32 CrateInAir = 0x1;
constexpr u32 CrateOnGround = 0x2;
// The surfaces' bits the agents' casts collide with, and the kinds of nodes (a bit each) of the instances that stop them: the
// solid agents (playable characters, crates, creatures, generic objects, pay gates and projectiles) and the crates
constexpr u32 SolidToProbes = 0x10;
constexpr u32 SolidToObjects = 0x40;
constexpr u32 SolidAgentKinds = 0x15B010;
constexpr u32 CrateKinds = 0x2000;
constexpr f32 NoHit = Rounded(1e30);
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// The bit a snap to the ground sets in the object node's motion
constexpr u32 MotionSnapped = 0x8;
// The shadows of the crates (a square), and of the creatures and generic objects (their own box's half sizes, kinds of their
// own); how far each reaches and its strength, the box's raised a little off the ground
constexpr f32 CrateShadowSize = 0.5f;
constexpr u8 CrateShadowKind = 1;
constexpr f32 CrateShadowDistance = 50.0f;
constexpr f32 CrateShadowStrength = 10.0f;
constexpr u8 CreatureShadowKind = 4;
constexpr u8 GenericShadowKind = 5;
constexpr f32 BoxShadowDistance = 30.0f;
constexpr f32 BoxShadowStrength = 16.0f;
constexpr f32 BoxShadowLift = Rounded(0.1);
// A shadow node's slot the agents' shadows take, and RegisterNode's second argument
constexpr u32 ShadowSlotIndex = 0;
constexpr u32 AttachNode = 1;

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

// A handle copied the way retail's copy constructor does: its reference counted once more
Reference* CopyHandle(Reference* const* handle)
{
    Reference* reference = *handle;
    if (reference != nullptr)
    {
        u32 count = ((reference->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask;
        reference->value = (reference->value & ~ReferenceBits::CountMask) | count;
    }

    return reference;
}

// A query of the instances along a cast: the awake ones with their collision on (retail leaves the bits nothing reads as the
// stack had them)
void StartQuery(InstanceRayHit* query, void** results, u16 most)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = NoHit;
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = ReferencedObject::FlagSphereContact;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// The shadow node a creature's or generic object's state makes the first time: a plain shadow of its own box's half sizes
void AddBoxShadow(InstanceContext* instance, u8 kind)
{
    const Box& box = instance->collision.ownBox;
    f32 width = (box.max.x - box.min.x) * 0.5f;
    f32 depth = (box.max.z - box.min.z) * 0.5f;
    auto* plain = ShadowPlain::Construct(width, depth, static_cast<ShadowPlain*>(MemoryAllocate(sizeof(ShadowPlain))), kind);
    auto* shapes = ShadowShapes::ConstructPlain(BoxShadowDistance, 0.0f,
                                                static_cast<ShadowShapes*>(MemoryAllocate(sizeof(ShadowShapes))), plain);
    auto* slot = ShadowSlot::Construct(BoxShadowStrength, static_cast<ShadowSlot*>(MemoryAllocate(sizeof(ShadowSlot))), shapes);
    auto* node = ShadowNode::Construct(static_cast<ShadowNode*>(MemoryAllocate(sizeof(ShadowNode))));
    plain->offset = {0.0f, BoxShadowLift, 0.0f, 1.0f};
    node->SetSlot(ShadowSlotIndex, slot);
    RegisterNode(instance, AttachNode, node);
}

bool HasShadowNode(InstanceContext* instance)
{
    return GetGameNode(&instance->nodes, ShadowNode::NodeKind) != nullptr;
}
}

AttackEvent* AttackEvent::Construct(AttackEvent* event, u32 kind, Reference** attacker, u32 kinds)
{
    // Retail copies the handle into its base's constructor (by value, twice) and lets the copies go again
    Reference* copy = CopyHandle(attacker);
    Reference* baseCopy = CopyHandle(&copy);
    event->unknown04 = EventId;
    event->vtable = g_GameEventVTable;
    event->kinds = kinds;
    event->reference = nullptr;
    event->type = 0;
    event->argument = CopyHandle(&baseCopy);
    RemoveReference(&baseCopy);
    event->vtable = g_AttackEventBaseVTable;
    RemoveReference(&copy);
    event->kind = kind;
    event->vtable = g_AttackEventVTable;
    RemoveReference(attacker);
    return event;
}

void AttackEvent::Destroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void AttackEvent::BaseDestroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
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

void Agent::ApplyState(u32)
{
}

namespace
{
// A script event of the starter queued for the agent's instance, its sender the instance
void QueueStarter(Agent* agent, const u16* starter, InstanceContext* originator, u32 force, u8 runner)
{
    InstanceContext* instance = agent->instance;
    Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
    auto* event = static_cast<ScriptEvent*>(MemoryAllocate(sizeof(ScriptEvent)));
    event = ScriptEvent::Construct(event, starter, runner, force, &sender, originator, ObjectNodeKinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(agent->instance, &handle);
}
}

void StartAgentBehaviour(Agent* agent, const u16* starter, InstanceContext* originator, u32 force, u32 runner)
{
    u16 id = *starter;
    QueueStarter(agent, &id, originator, force, static_cast<u8>(runner));
}

void SetAgentModel(Agent* agent, GameResources* resources)
{
    auto* node = static_cast<ModelNode*>(GetGameNode(&agent->instance->nodes, ModelNodeKind));
    if (node == nullptr)
    {
        return;
    }

    u16 id = agent->object->models.items[0];
    auto* ogi = id != NoId ? static_cast<GameOGI*>(resources->models->items[id & 0x7FFF]) : nullptr;
    GameObject* object = agent->object;
    node->SetOgi(ogi, object->ReactJoints(), object->ExitPoints());
}

u32 RunAgentEvent(Agent* agent, u32 event, u32 originator, u32 force, u32 runner)
{
    GameObject* object = agent->object;
    if (object == nullptr || (agent->unknown1C & EventsOff) != 0 || event >= object->BehaviourSlotCount())
    {
        return 0;
    }

    u16 starter = object->behaviours.items[event];
    if (starter != NoId)
    {
        QueueStarter(agent, &starter, reinterpret_cast<InstanceContext*>(originator), force, static_cast<u8>(runner));
    }

    return 1;
}

void RestartAgent(Agent* agent)
{
    if (agent->object == nullptr)
    {
        return;
    }

    if (agent->spawnScript != NoId)
    {
        u16 starter = agent->spawnScript;
        StartAgentBehaviour(agent, &starter, nullptr, 1, 0);
    }
    else
    {
        RunAgentEvent(agent, StartSlot, 0, 1, 0);
    }
}

void AgentTakeResources(Agent* agent, GameResources* resources)
{
    CallVirtual<void>(agent, agent->vtable, ApplyStateSlot, u32{0});
    SetAgentModel(agent, resources);
    RestartAgent(agent);
    agent->ClearUnknown18();
    agent->unknown1C &= ~EventsOff;
}

void AgentStep(Agent* agent, GameResources* resources, TimeClock*, u32 unknown)
{
    CallVirtual<void>(agent, agent->vtable, ApplyStateSlot, unknown);
    agent->unknown1C &= ~EventsOff;
    ContactMessage::Construct(&agent->contact);
    SetAgentModel(agent, resources);
    RestartAgent(agent);
    agent->ClearUnknown18();
}

void LinkToAgent(Agent* agent, InstanceContext* linked, u32 unknown)
{
    LinkInstance(AttachmentsOf(agent->instance), linked, unknown);
}

u32 AttachToAgent(Agent* agent, InstanceContext* instance, u32 flags, u32 exitPoint, const Matrix4x4* offset)
{
    void* attachments = AttachmentsOf(agent->instance);
    u8 point = static_cast<u8>(exitPoint);
    if (point != NoExitPoint)
    {
        return HangOnExitPoint(attachments, agent->instance, instance, point, flags, offset, 0);
    }

    return AttachInstance(attachments, agent->instance, instance, flags, offset);
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

void Agent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
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

void Agent::Recover()
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

void Agent::Freeze()
{
}

void Agent::Unfreeze()
{
}

void Agent::PlayRideSound()
{
}

void Agent::LeaveFootprints(u32)
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

void BasicAgent::Touched(InstanceContext*, const Vector4*)
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

u32 BasicAgent::LineOfSight(const Vector4* from, Vector4* way, u32 mask, InstanceRayHit* hit, u32 instanceMask)
{
    return ::LineOfSight(instance->chunk, from, way, mask, hit, instanceMask);
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

void PickupAgent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
{
}

void PickupAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32)
{
    KeepContact(this, message, sender);
}

u32 PickupAgent::Velocity(Vector4*)
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

void CrateAgent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
{
}

void CrateAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32 physical)
{
    BasicAgent::Contact(message, sender, physical);
}

u32 CrateAgent::Velocity(Vector4*)
{
    return 0;
}

void CrateAgent::Launch(const Vector4* velocity)
{
    launchSpeed = velocity->y;
    bits = (bits & ~(StateMask << StateShift)) | StateLaunched << StateShift;
}

void CrateAgent::ApplyState(u32 unknown)
{
    BasicAgent::ApplyState(unknown);
    launchSpeed = 0.0f;
    // Resting, after no state
    bits = StateMask << CurrentShift | StateResting << StateShift;
    if (unknown != 0 || HasShadowNode(instance))
    {
        return;
    }

    auto* plain = ShadowPlain::ConstructSquare(CrateShadowSize, static_cast<ShadowPlain*>(MemoryAllocate(sizeof(ShadowPlain))),
                                               CrateShadowKind);
    auto* shapes = ShadowShapes::ConstructPlain(CrateShadowDistance, 0.0f,
                                                static_cast<ShadowShapes*>(MemoryAllocate(sizeof(ShadowShapes))), plain);
    auto* slot = ShadowSlot::Construct(CrateShadowStrength, static_cast<ShadowSlot*>(MemoryAllocate(sizeof(ShadowSlot))), shapes);
    auto* node = ShadowNode::Construct(static_cast<ShadowNode*>(MemoryAllocate(sizeof(ShadowNode))));
    node->SetSlot(ShadowSlotIndex, slot);
    RegisterNode(instance, AttachNode, node);
}

u32 CrateAgent::Collided(void* other, const Vector4*, const Vector4* impulse)
{
    constexpr f32 Steep = Rounded(0.707);
    constexpr f32 BreakingSquared = 25.0f;
    constexpr f32 HardSquared = 5.0f;
    constexpr u32 HitFromBelowEvent = 4;
    f32 squared = impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z;
    Vector4 direction = *impulse;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.y = direction.y * inverse;
    bool intact = (bits & Broken) == 0;
    bool unbreakable = (properties->state & StateUnbreakable) != 0;
    if (squared > BreakingSquared)
    {
        RunAgentEvent(this, HardCollisionEvent, reinterpret_cast<u32>(other), 0, 0);
        if (!unbreakable)
        {
            bits |= Broken;
            intact = false;
        }

        return intact;
    }

    u32 event = CollisionEvent;
    if (squared > HardSquared)
    {
        if (direction.y < -Steep)
        {
            event = LandedOnEvent;
        }
        else if (direction.y > Steep)
        {
            event = HitFromBelowEvent;
        }
    }

    RunAgentEvent(this, event, reinterpret_cast<u32>(other), 0, 0);
    return intact;
}

void CrateAgent::Frame(TimeClock* clock)
{
    constexpr u32 NotAsked = 0xFFFFFFFF;
    u32 asked = NotAsked;
    u32 next = bits >> StateShift & StateMask;
    if (next != StateNone)
    {
        bits = (bits & ~(StateMask << CurrentShift)) | next << CurrentShift;
        bits = (bits & ~(StateMask << StateShift)) | StateNone << StateShift;
    }

    switch (bits >> CurrentShift & StateMask)
    {
    case StateResting:
        CheckGround(clock, &asked);
        break;
    case StateLaunched:
    {
        u32 falling = Fall(clock);
        if (falling == 0)
        {
            RunAgentEvent(this, CrateLandedEvent, 0, 0, 0);
            asked = StateSettled;
        }

        SetBit(instance->flags, ReferencedObject::FlagShadow, falling != 0);
        break;
    }
    default:
        break;
    }

    if (asked != NotAsked)
    {
        bits = (bits & ~(StateMask << StateShift)) | (asked & StateMask) << StateShift;
    }
}

u32 CrateAgent::Fall(TimeClock* clock)
{
    constexpr f32 Gravity = 50.0f;
    auto* cratePart = static_cast<CratePart*>(part);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    Vector4 position = place->position;
    f32 drop = launchSpeed * seconds;
    Vector4 move = {0.0f, drop, 0.0f, 1.0f};
    bool landed = false;
    if (launchSpeed >= 0.0f)
    {
        cratePart->value |= CrateInAir;
    }
    else
    {
        void* found[1];
        InstanceRayHit query;
        StartQuery(&query, found, 1);
        SkipInQuery(&query, instance);
        landed = LineOfSight(&position, &move, SolidToObjects, &query, CrateKinds) != 0;
        if (query.count != 0)
        {
            Agent* other = AgentNodeOf(static_cast<InstanceContext*>(found[0]))->agent;
            if ((other->properties->state & StateCanBeStoodOn) != 0)
            {
                RunAgentEvent(other, LandedOnEvent, reinterpret_cast<u32>(instance), 0, 0);
                landed = true;
            }
            else
            {
                move = {0.0f, drop, 0.0f, 1.0f};
                RunAgentEvent(other, SlammedEvent, reinterpret_cast<u32>(instance), 0, 0);
                landed = false;
            }
        }
        else if (landed)
        {
            cratePart->value |= CrateOnGround;
        }

        SetBit(cratePart->value, CrateInAir, !landed);
    }

    launchSpeed = launchSpeed - seconds * Gravity;
    position.x = position.x + move.x;
    position.y = position.y + move.y;
    position.z = position.z + move.z;
    place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&position))
    {
        QueueObject(instance);
    }

    return !landed;
}

void CrateAgent::CheckGround(TimeClock*, u32* asked)
{
    constexpr u16 MostFound = 32;
    constexpr f32 CastHeight = Rounded(0.9);
    u32 wait = bits >> WaitShift & WaitMask;
    if (wait != 0)
    {
        bits = (bits & ~(WaitMask << WaitShift)) | ((wait - 1) & WaitMask) << WaitShift;
        return;
    }

    Vector4 down = {0.0f, -1.0f, 0.0f, 1.0f};
    void* found[MostFound];
    InstanceRayHit query;
    StartQuery(&query, found, MostFound);
    auto* cratePart = static_cast<CratePart*>(part);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 from = place->position;
    SkipInQuery(&query, instance);
    from.y = from.y + CastHeight;
    u32 hit = LineOfSight(&from, &down, SolidToObjects, &query, CrateKinds);
    bool resting = hit != 0 && query.count == 0;
    SetBit(cratePart->value, CrateOnGround, resting);
    if ((instance->flags & ReferencedObject::FlagShadow) != 0)
    {
        SetBit(instance->flags, ReferencedObject::FlagShadow, hit == 0);
    }

    *asked = StateSettled;
}

void CrateAgent::StartFalling()
{
    auto* cratePart = static_cast<CratePart*>(part);
    if ((cratePart->value & CrateOnGround) != 0 || (cratePart->value & CrateInAir) != 0)
    {
        return;
    }

    launchSpeed = 0.0f;
    bits = (bits & ~(StateMask << StateShift)) | StateLaunched << StateShift;
    RunAgentEvent(this, CrateEvent, 0, 0, 0);
}

CreatureAgent* CreatureAgent::Construct(CreatureAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                        AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->vtable = g_CreatureAgentVTable;
    agent->velocity = g_DefaultBox.min;
    agent->velocity.w = 1.0f;
    return agent;
}

void CreatureAgent::Destroy(u32 destroyFlags)
{
    BasicAgent::Destroy(destroyFlags);
}

void CreatureAgent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
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

    TransformVectorThroughLink(link, &velocity, 0);
    return 1;
}

u32 CreatureAgent::Velocity(Vector4*)
{
    return 0;
}

void CreatureAgent::Attacked(const AttackEvent* event, InstanceContext* sender)
{
    BasicAgent::Attacked(event, sender);
}

void CreatureAgent::FallFrame()
{
    constexpr u32 LandSlot = 25;
    if ((static_cast<CreaturePart*>(part)->flags & CreaturePart::FlagOnGround) != 0)
    {
        CallVirtual<void>(this, vtable, LandSlot, 1u);
    }
}

void CreatureAgent::StartFalling(u32)
{
    static_cast<CreaturePart*>(part)->flags |= CreaturePart::FlagFalling;
}

void CreatureAgent::Land(u32)
{
    static_cast<CreaturePart*>(part)->flags &= ~CreaturePart::FlagFalling;
}

void CreatureAgent::ApplyState(u32 unknown)
{
    constexpr u32 HitPointsProperty = 2;
    auto* creature = static_cast<CreaturePart*>(part);
    PropertyHolder* holder = properties;
    BasicAgent::ApplyState(unknown);
    SetBit(creature->flags, CreaturePart::FlagSnapsToGround, (holder->state & StateSnapsToGround) != 0);
    u32 hitPoints = holder->GetInt(HitPointsProperty);
    creature->flags = (creature->flags & ~(CreaturePart::HitPointsMask << CreaturePart::HitPointsShift)) |
                      (hitPoints & CreaturePart::HitPointsMask) << CreaturePart::HitPointsShift;
    if ((creature->flags & CreaturePart::FlagSnapsToGround) != 0)
    {
        SnapToGround();
    }

    if (unknown == 0 && !HasShadowNode(instance))
    {
        AddBoxShadow(instance, CreatureShadowKind);
    }
}

u32 CreatureAgent::Collided(void* other, const Vector4*, const Vector4* impulse)
{
    constexpr f32 HardSquared = 200.0f;
    constexpr f32 SoftSquared = 10.0f;
    f32 squared = impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z;
    // Retail works out the impulse's direction and never uses it
    Vector4 direction = *impulse;
    InverseLength(&direction, LengthEpsilon);
    if (squared > HardSquared)
    {
        RunAgentEvent(this, CollisionEvent, reinterpret_cast<u32>(other), 0, 0);
        RunAgentEvent(this, HardCollisionEvent, reinterpret_cast<u32>(other), 0, 0);
    }
    else if (squared > SoftSquared)
    {
        RunAgentEvent(this, CollisionEvent, reinterpret_cast<u32>(other), 0, 0);
    }

    return 1;
}

void CreatureAgent::Frame(TimeClock* clock)
{
    constexpr u32 FallFrameSlot = 23;
    constexpr u32 StartFallingSlot = 24;
    constexpr u32 FallSpeedProperty = 3;
    auto* creature = static_cast<CreaturePart*>(part);
    if ((creature->flags & CreaturePart::FlagFalling) != 0)
    {
        CallVirtual<void>(this, vtable, FallFrameSlot, clock);
        return;
    }

    if ((creature->flags & CreaturePart::FlagSnapsToGround) != 0)
    {
        SnapToGround();
        return;
    }

    f32 downward = velocity.x * g_AgentDown.x + velocity.y * g_AgentDown.y + velocity.z * g_AgentDown.z;
    if (downward > properties->GetFloat(FallSpeedProperty))
    {
        CallVirtual<void>(this, vtable, StartFallingSlot, 1u);
    }
}

u32 CreatureAgent::SnapToGround()
{
    constexpr f32 Reach = 7.0f;
    constexpr f32 CastHeight = 1.0f;
    constexpr u32 LiftProperty = 5;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
    PropertyHolder* holder = properties;
    auto* creature = static_cast<CreaturePart*>(part);
    Vector4 way = {0.0f, -Reach, 0.0f, 1.0f};
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 from = place->position;
    from.y = from.y + CastHeight;
    u32 hit = LineOfSight(&from, &way, SolidToObjects, nullptr, SolidAgentKinds);
    if (hit != 0)
    {
        f32 lift = holder->GetFloat(LiftProperty);
        from.x = from.x + way.x;
        from.z = from.z + way.z;
        from.y = from.y + way.y + lift;
        place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&from))
        {
            QueueObject(instance);
        }

        node->motion->bits |= MotionSnapped;
    }

    SetBit(creature->flags, CreaturePart::FlagOnGround, hit != 0);
    return hit;
}

void CharacterButtons::Clear()
{
    turn = 0.0f;
    moveZ = 0.0f;
    moveX = 0.0f;
    cross = 0.0f;
    square = 0.0f;
    circle = 0.0f;
    shoulders = 0.0f;
    locked = 0;
}

CharacterAgent* CharacterAgent::Construct(CharacterAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                          AgentPart* part)
{
    CreatureAgent::Construct(agent, creator, holder, part);
    agent->crouch = nullptr;
    agent->claw = nullptr;
    agent->jump = nullptr;
    agent->gun = nullptr;
    agent->spin = nullptr;
    agent->walk = nullptr;
    agent->link = nullptr;
    agent->proceduralJoints = nullptr;
    agent->vehicle = nullptr;
    agent->look = nullptr;
    agent->body = nullptr;
    agent->vtable = g_CharacterAgentVTable;
    agent->buttons.Clear();
    ReferenceSet::Construct(&agent->triggers, nullptr);
    agent->standingOn = nullptr;
    agent->pushedBody = nullptr;
    agent->probedInstance = nullptr;
    ConstructCollisionCache(&agent->cache, agent->instance, CharacterCacheMask);
    agent->SetUp(1);
    agent->link = nullptr;
    return agent;
}

void CharacterAgent::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterAgentVTable;
    TearDown(1);
    DestroyCollisionCache(&cache, DestroyOnly);
    RemoveReference(&probedInstance);
    RemoveReference(&pushedBody);
    RemoveReference(&standingOn);
    triggers.Destroy(DestroyOnly);
    BasicAgent::Destroy(destroyFlags);
}

u32 CharacterAgent::Velocity(Vector4* out)
{
    *out = velocity;
    return 1;
}

u32 CharacterAgent::Slot12()
{
    return 1;
}

AgentPart* CharacterAgent::Part()
{
    return part;
}

void CharacterAgent::CollisionCenter(Vector4* center)
{
    JointPosition(instance, CharacterCenterJoint, center, nullptr);
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

void GenericObjectAgent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
{
}

u32 GenericObjectAgent::Velocity(Vector4*)
{
    return 0;
}

void GenericObjectAgent::ApplyState(u32 unknown)
{
    BasicAgent::ApplyState(unknown);
    if (unknown == 0 && !HasShadowNode(instance))
    {
        AddBoxShadow(instance, GenericShadowKind);
    }
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

u32 GrabbableAgent::Velocity(Vector4*)
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

u32 PayGateAgent::Velocity(Vector4*)
{
    return 0;
}

void PayGateAgent::Frame(TimeClock*)
{
}

GrapleAgent* GrapleAgent::Construct(GrapleAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
{
    BasicAgent::Construct(agent, creator, holder, part);
    agent->rope = nullptr;
    agent->vtable = g_GrapleAgentVTable;
    return agent;
}

void GrapleAgent::ApplyState(u32)
{
    if (rope != nullptr)
    {
        return;
    }

    rope = ConstructGrapleRope(1.0f, MemoryAllocate(sizeof(GrapleRope)), instance);
    rope->BlendIn(0);
}

void GrapleAgent::Destroy(u32 destroyFlags)
{
    vtable = g_GrapleAgentVTable;
    if (rope != nullptr)
    {
        rope->DestroyVirtual(DestroyAndFree);
    }

    BasicAgent::Destroy(destroyFlags);
}

u32 GrapleAgent::Velocity(Vector4*)
{
    return 0;
}

void GrapleAgent::Frame(TimeClock* clock)
{
    if ((instance->flags & ReferencedObject::FlagVisible) != 0)
    {
        Swing(clock);
    }
}

void GrapleAgent::Swing(TimeClock* clock)
{
    constexpr u16 MostFound = 32;
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    constexpr f32 ShortOfHit = 0.5f;
    constexpr f32 NearestHit = Rounded(0.51);
    constexpr f32 Least = Rounded(0.01);
    constexpr f32 BoxMargin = 0.5f;
    void* found[MostFound];
    InstanceRayHit query;
    StartQuery(&query, found, MostFound);
    Vector4 way = target;
    way.x = way.x - anchor.x;
    way.y = way.y - anchor.y;
    way.z = way.z - anchor.z;
    f32 length = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
    if (__builtin_fabsf(length) <= Epsilon)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Matrix4x4 toLocal = place->matrix;
    LineOfSight(&anchor, &way, SolidToProbes, &query, SolidAgentKinds);
    f32 reached = __builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z);
    f32 share;
    if (__builtin_fabsf(reached) <= Epsilon)
    {
        way = target;
        way.x = way.x - anchor.x;
        way.y = way.y - anchor.y;
        way.z = way.z - anchor.z;
        share = Least / length;
    }
    else
    {
        share = (reached < NearestHit ? Least : reached - ShortOfHit) / reached;
    }

    Vector4 hook = way;
    hook.x = way.x * share + anchor.x;
    hook.y = way.y * share + anchor.y;
    hook.z = way.z * share + anchor.z;
    place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&hook))
    {
        QueueObject(instance);
    }

    rope->Step(clock, &g_GrapleGravity, &hook, &anchor);
    VuInvertRigidInPlace(&toLocal);
    SpringChain* chain = rope->chain;
    Vector4 last;
    Vector4 first;
    VuTransformPoint(&toLocal, &chain->last->position, &last);
    VuTransformPoint(&toLocal, &chain->first->position, &first);
    Box box;
    ResetBox(&box);
    GrowBoxByPoint(&box, &last);
    GrowBoxByPoint(&box, &first);
    GrowBox(BoxMargin, &box);
    instance->collision.ownBox = box;
}

void GrapleAgent::SetEnds(const Vector4* from, const Vector4* to)
{
    anchor = *from;
    target = *to;
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

void ProjectileAgent::Bumped(InstanceContext*, const Vector4*, const Vector4*)
{
}

void ProjectileAgent::Contact(const ContactMessage*, InstanceContext*, u32)
{
}

u32 ProjectileAgent::Velocity(Vector4*)
{
    return 0;
}

void ProjectileAgent::Frame(TimeClock*)
{
}

void CharacterAgent::StartInvincibility(u32 kind)
{
    constexpr f32 LongSeconds = 8.0f;
    auto* creature = static_cast<BasicAgentPart*>(part);
    if ((creature->bits & CharacterPart::Invincible) != 0)
    {
        return;
    }

    creature->bits |= CharacterPart::Invincible;
    u64& bits = StateBits();
    bits = (bits & ~u64{ModeMask << ModeShift}) | u64{ModeInvincible << ModeShift};
    if (kind == 0)
    {
        modeTicks = static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond);
    }
    else if (kind == 1)
    {
        modeTicks = static_cast<s32>(g_ClockUnitsPerSecond * LongSeconds);
    }
}

void InitAgentsModule(u32 initialize, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    constexpr f32 Small = Rounded(0.01);
    constexpr f32 GrapleGravity = -25.0f;
    if (priority != AllPriorities || initialize == 0)
    {
        return;
    }

    g_AgentsUp.x = 0.0f;
    g_AgentsUp.w = 1.0f;
    g_AgentsSmall2 = Small;
    g_AgentsZero = 0;
    g_AgentsUp.y = 1.0f;
    g_AgentsUp.z = 0.0f;
    g_AgentsSmall = Small;
    ColourSet(&g_AgentsShade, 0.0f, 0.0f, 0.0f, 0.5f);
    AngleFrom(&g_AgentsAngle45, 0x1.921fb6p-1f, AngleRadians);
    AngleFrom(&g_AgentAngleMinus135, -135.0f, AngleDegrees);
    AngleFrom(&g_AgentAngle135, 135.0f, AngleDegrees);
    AngleFrom(&g_AgentAngleMinus45, -45.0f, AngleDegrees);
    AngleFrom(&g_AgentAngle45, 45.0f, AngleDegrees);
    g_GrapleGravity.w = 1.0f;
    g_TriggerNodeKinds = 0x180;
    g_AgentDown.x = 0.0f;
    g_AgentDown.y = -1.0f;
    g_AgentDown.w = 1.0f;
    g_GrapleGravity.x = 0.0f;
    g_GrapleGravity.y = GrapleGravity;
    g_GrapleGravity.z = 0.0f;
    g_AgentDown.z = 0.0f;
    HullConstruct(&g_DamageHulls[0]);
}

void ConstructAgentsModule()
{
    InitAgentsModule(1, 0xFFFF);
}

// The cases splat split off a switch of a function nothing calls (func_001412B0, fragments.txt), which its jump table
// (jtbl_002F2E20) still points at: a word of 4 to 14 at 0x14 into its second argument made 5, 4, 6, 7, 8, 7, 6, 7, 8, 10 or 10,
// any other 3. The other cases' labels (func_001412EC, func_00141304, the default .L0014130C) were jump labels inside these, which
// the link script defines as 0 now that their files are left out
extern "C"
{
    u32 UnreachedCase4() RETAIL(FUN_001412fc);
    u32 UnreachedCase6() RETAIL(FUN_001412dc);
    u32 UnreachedCase7() RETAIL(FUN_001412e4);
    u32 UnreachedCase5() RETAIL(FUN_001412f4);
}

u32 UnreachedCase4()
{
    return 4;
}

u32 UnreachedCase6()
{
    return 6;
}

u32 UnreachedCase7()
{
    return 7;
}

u32 UnreachedCase5()
{
    return 5;
}
