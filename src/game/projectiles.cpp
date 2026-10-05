#include "game/objectnode.h"

#include "game/agentlab.h"
#include "game/agentparts.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/pickups.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/vehicles.h"

// The custom projectiles (the objects of type projectile): their object node (kind 1), which flies the way its code model's
// custom projectile says and tells what it hits, and the code models' slots. The game's events' constructor sits here too, where
// retail has it, and the thin pyramid hull the game context makes

// A custom projectile's object node's bits: its state, the one before it, its custom slot, its velocity given (slot 26) before it
// was launched, homing in on its target, and not bouncing off agents whose bullets bounce back (bit 21, set from outside its
// code)
union ProjectileNodeBits
{
    u32 value;
    struct
    {
        u32 state : 5;
        u32 previous : 5;
        u32 customSlot : 5;
        u32 unused15 : 4;
        u32 velocityGiven : 1;
        u32 homing : 1;
        u32 noBounce : 1;
        u32 unused22 : 10;
    };
};
CHECK_SIZE(ProjectileNodeBits, 4);

// A custom projectile's object node (0xF0 bytes, vtable InstanceNodeType8_Methods over the prototype's): the prototype's part
// first (ObjectNodeBase's first 0xA0 bytes, whose functions of the prototype work on it), then its velocity, its bits (bit fields
// of the 64 bits from 0xC0 in retail, the message in their upper half), the trigger message it sends what it hits (0xFFFF none),
// its target and who shot it, the clock time it was launched at (or started ending at), its particle trails, its speed, when it
// last bounced (seconds of the clock) and how far above its target's place it aims. Its states are its code model's, whose pack
// of a state runs as it enters it
struct ProjectileObjectNode : GameNode
{
    enum State : u32
    {
        StateAsleep = 0,
        // Launched: flying from its next frame
        StateLaunched = 2,
        StateFlying = 3,
        // What it hit (made ending by its next update): a crate, a creature, a generic object, a pay gate or something of kind
        // 0x15, a character; nothing or nothing for 2.2 seconds
        StateHitCrate = 4,
        StateHitCreature = 5,
        StateHitObject = 6,
        StateHitCharacter = 7,
        StateMissed = 8,
        // Hidden and asleep half a second after
        StateEnding = 11,
        // Bounced off its focus, flying again
        StateBouncing = 12,
    };

    u8 prototype[0xA0 - sizeof(GameNode)];
    Vector4 velocity;
    u8 unusedB0[0x10];
    ProjectileNodeBits bits;
    u16 message;
    u16 unusedC6;
    Reference* target;
    Reference* shooter;
    u32 launchTime;
    ParticleTrails* trails;
    f32 speed;
    f32 lastBounce;
    u32 unusedE0;
    f32 aimHeight;
    u8 unusedE8[8];

    // Made (the prototype's part: game/objectnodeparts.cpp)
    static ProjectileObjectNode* Construct(ProjectileObjectNode* node) RETAIL(InitInstanceNodeType8);
    static ProjectileObjectNode* ConstructPrototype(ProjectileObjectNode* node) RETAIL(FUN_0023d150);

    // Its vtable's functions: 1 an event handled (a script event's starter taken as the ID of a state, its focus the event's
    // originator), 2 the destructor (its references let go), 3 given its instance, 4 whether its instance may change chunks (yes,
    // its velocity turned through the link), 7 its step once its instance starts again (its trails let go), 8 its update, 11 put
    // to sleep (its trails let go, but while every chunk is being unloaded), 12 given its agent (its custom slot its first
    // integer property), 13 made as new (nothing), 14 and 15 (no packets) no, 23 a particle trail added (its trails made the
    // first time: the trail's slot), 24 its trails let go, 25 its custom slot, 26 given its velocity (its speed the velocity's
    // length; the gravity the other nodes' take isn't read), 41 its code model's kind, 42 a runner's packet ended (it has no
    // runners), 43 a packet started (nothing)
    void HandleEvent(Reference** handle) RETAIL(FUN_0010c090);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011eca0);
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0011edd8);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0011ef28);
    void Restart(TimeClock* clock) RETAIL(FUN_0011edf8);
    u32 Update(TimeClock* clock) RETAIL(FUN_0010a938);
    void Sleep() RETAIL(FUN_0011ecf8);
    void SetAgent(Agent* agent) RETAIL(FUN_0011ed30);
    void Reset() RETAIL(FUN_0011ee40);
    u32 UnusedTakesPackets() RETAIL(FUN_0011eb20);
    u32 TakesPackets() RETAIL(FUN_0011eb28);
    u32 AddParticleTrail(const void* arguments) RETAIL(FUN_0011ee98);
    void DestroyParticleTrails() RETAIL(FUN_0011eef0);
    u32 CustomSlot() RETAIL(FUN_0011eb40);
    void SetVelocity(const Vector4* given) RETAIL(FUN_0011ed88);
    u32 CodeModelKind() RETAIL(FUN_0011eb38);
    u32 PacketEnded() RETAIL(FUN_0011eb18);
    void PacketStarted() RETAIL(FUN_0011eb30);

    // Its state set: what entering a new one does, then its code model's pack of the state it's in after run (also when it was
    // in the state, but not when it went to sleep)
    void SetState(TimeClock* clock, u32 state) RETAIL(FUN_0010a6e8);
    // A frame of its flight (the instance a line along its way hits told, its state the kind of what it hit)
    void Fly(TimeClock* clock) RETAIL(FUN_0010aa50);
    // Bounced off an instance from a point (at most every 0.3 seconds: its velocity reflected off the way from the instance's
    // middle, a little at random, and its frame's move along it), off its focus from where it is
    void BounceOff(TimeClock* clock, InstanceContext* other, const Vector4* point) RETAIL(FUN_0010b298);
    void BounceOffFocus(TimeClock* clock) RETAIL(FUN_0010b790);
    // Aimed as it's launched, by the player's character that shot it
    void Aim(CustomProjectile* projectile) RETAIL(FUN_0010b880);

    u32 State() const
    {
        return bits.state;
    }

    void SetCustomSlot(u32 slot)
    {
        bits.customSlot = slot;
    }

    // The state it's in made the one before, the new one written
    void Enter(u32 state)
    {
        bits.previous = bits.state;
        bits.state = state;
    }

    // Its prototype's part, which ObjectNodeBase has as its first 0xA0 bytes
    ObjectNodeBase* Prototype()
    {
        return reinterpret_cast<ObjectNodeBase*>(this);
    }
};
CHECK_OFFSET(ProjectileObjectNode, velocity, 0xA0);
CHECK_OFFSET(ProjectileObjectNode, bits, 0xC0);
CHECK_OFFSET(ProjectileObjectNode, target, 0xC8);
CHECK_OFFSET(ProjectileObjectNode, trails, 0xD4);
CHECK_OFFSET(ProjectileObjectNode, aimHeight, 0xE4);
CHECK_SIZE(ProjectileObjectNode, 0xF0);

extern "C"
{
    extern const GccVTableEntry g_ProjectileObjectNodeVTable[] RETAIL(InstanceNodeType8_Methods);
    // The instance the player's character operates (game/characters.h has it too)
    extern Reference* g_OperatedInstance RETAIL(D_0030A930);
    // The events' vtables: the base's, the one with an argument's and the game's events'
    extern const GccVTableEntry g_ArgumentEventVTable[] RETAIL(D_002F0320);
    extern const GccVTableEntry g_TriggerEventVTable[] RETAIL(TriggerEventCaller_Methods_);
    // Each slot's packs its states run (made with new[], from its code model; its custom projectile is game/pickups.h's), the
    // command its code model runs on a node of the slot when it's set up, and whether the slot gave the states' IDs (the IDs of
    // the first code model a slot had, for every slot)
    extern ScriptPack* g_CustomProjectilePacks[CustomProjectileSlots + 1] RETAIL(D_003D1E90);
    extern ScriptCommand* g_CustomProjectileCommands[CustomProjectileSlots + 1] RETAIL(D_003D1EB8);
    extern u8 g_CustomProjectileIdsGiven[0x10] RETAIL(D_003D1EE0);
    extern u16* g_CustomProjectileStateIds RETAIL(D_0030AAAC);
    // A thin pyramid's hull: its point at the origin, its base 0.2 across and 8 high a unit along z
    extern CollisionHull g_PyramidHull RETAIL(PyramidHull);

    // What the character's targeting has locked on, and where its target is (whether it has one)
    InstanceContext* LockedOnInstance(Gun* gun) RETAIL(FUN_0014a730);
    u32 TargetPosition(Gun* gun, Vector4* position) RETAIL(FUN_0014a7d0);

    // A state's ID (a behaviour starter's, of the code model's) turned into the state (its index among the IDs, of 13): whether
    // there was one
    u32 ProjectileStateOfId(u16* id) RETAIL(FUN_0011ee48);
    // The pyramid hull made (the game context's constructor)
    void MakePyramidHull() RETAIL(FUN_0010c160);
}

namespace
{
constexpr u32 ProjectileStates = 13;
// What its flight is stopped by: the collision's surfaces solid to objects, and the instances with a node of the kinds
// (characters, crates, creatures, generic objects, grabbables, pay gates and type 9's; but characters for the player's shots)
// and the flag 4
constexpr u32 PlayersShotHitKinds = 1u << NodeCrate | 1u << NodeCreature | 1u << NodeGenericObject | 1u << NodeGrabbable |
                                    1u << NodePayGate | 1u << NodeUnusedObjectType;
constexpr u32 HitKinds = PlayersShotHitKinds | 1u << NodeCharacter;
// The character's jump states of the radial blast (its hang to its fall): a downward blast
constexpr u32 DownwardBlastFirst = JumpController::StateBlastHang;
constexpr u32 DownwardBlastStates = JumpController::StateBlastFalling - JumpController::StateBlastHang + 1;

f32 SecondsBetween(u32 from, u32 to)
{
    return static_cast<f32>(static_cast<s32>(to - from)) * g_SecondsPerClockUnit;
}

// The clock's last advance in seconds
f32 LastAdvance(const TimeClock* clock)
{
    return static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
}

ReferencedObject* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? handle->object : nullptr;
}

// A query of the instances its way meets (the stack's other bits kept in retail; nothing reads them)
void MakeQuery(InstanceQuery* query, void** results, u16 most)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Infinite;
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = ReferencedObjectFlags::CollisionActive;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// A place turned about its x axis by an angle unless it's none: whether it turned
bool TurnByPitch(ObjectPlace* place, s32 pitch)
{
    if (pitch == 0)
    {
        return false;
    }

    place->SyncRotation();
    place->MarkTurned();
    Vector4 turn;
    RotationFromPitch(&turn, &pitch);
    MultiplyRotations(&place->rotation, &place->rotation, &turn);
    return true;
}

// GCC 2.9x's copy of a reference handle passed by value: one more reference counted
Reference* CopyHandle(Reference* handle)
{
    if (handle != nullptr)
    {
        handle->bits.count++;
    }

    return handle;
}
}

ProjectileObjectNode* ProjectileObjectNode::Construct(ProjectileObjectNode* node)
{
    constexpr f32 AimHeight = Rounded(0.6);
    ConstructPrototype(node);
    node->vtable = g_ProjectileObjectNodeVTable;
    node->velocity = {0.0f, 0.0f, 0.0f, 1.0f};
    node->aimHeight = AimHeight;
    node->target = nullptr;
    node->shooter = nullptr;
    node->launchTime = 0;
    node->trails = nullptr;
    node->speed = 0.0f;
    node->lastBounce = 0.0f;
    node->bits.state = StateAsleep;
    node->bits.previous = 0;
    node->bits.velocityGiven = 0;
    node->bits.homing = 0;
    node->bits.noBounce = 0;
    node->message = NoMessage;
    return node;
}

void ProjectileObjectNode::Destroy(u32 destroyFlags)
{
    vtable = g_ProjectileObjectNodeVTable;
    Sleep();
    RemoveReference(&shooter);
    RemoveReference(&target);
    Prototype()->DestroyPrototype(destroyFlags);
}

void ProjectileObjectNode::Sleep()
{
    if (g_UnloadingEverything == 0)
    {
        CallVirtual<void>(this, vtable, ObjectNodeBase::DestroyParticleTrailsSlot);
    }
}

void ProjectileObjectNode::SetAgent(Agent* agent)
{
    ObjectNodeBase* prototype = Prototype();
    prototype->SetAgent(agent);
    if (prototype->properties != nullptr)
    {
        SetCustomSlot(static_cast<u32>(prototype->properties->GetInt(0)));
    }
}

void ProjectileObjectNode::SetVelocity(const Vector4* given)
{
    velocity = *given;
    bits.velocityGiven = 1;
    speed = __builtin_sqrtf(given->x * given->x + given->y * given->y + given->z * given->z);
}

void ProjectileObjectNode::SetOwner(InstanceContext* instance)
{
    GameNode::SetOwner(instance);
}

void ProjectileObjectNode::Restart(TimeClock* clock)
{
    CallVirtual<void>(this, vtable, ObjectNodeBase::DestroyParticleTrailsSlot);
    time = clock->time;
}

void ProjectileObjectNode::Reset()
{
}

u32 ProjectileObjectNode::AddParticleTrail(const void* arguments)
{
    if (trails == nullptr)
    {
        trails = ParticleTrails::Construct(static_cast<ParticleTrails*>(MemoryAllocate(sizeof(ParticleTrails))));
    }

    return trails->Add(arguments);
}

void ProjectileObjectNode::DestroyParticleTrails()
{
    if (trails != nullptr)
    {
        trails->Destroy(DestroyAndFree);
        trails = nullptr;
    }
}

u32 ProjectileObjectNode::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    TransformVectorThroughLink(link, &velocity, 0);
    return 1;
}

u32 ProjectileObjectNode::CustomSlot()
{
    return bits.customSlot;
}

u32 ProjectileObjectNode::UnusedTakesPackets()
{
    return 0;
}

u32 ProjectileObjectNode::TakesPackets()
{
    return 0;
}

u32 ProjectileObjectNode::CodeModelKind()
{
    return CodeModel::KindProjectile;
}

u32 ProjectileObjectNode::PacketEnded()
{
    return 0;
}

void ProjectileObjectNode::PacketStarted()
{
}

void ProjectileObjectNode::HandleEvent(Reference** handle)
{
    auto* event = *handle != nullptr ? reinterpret_cast<ScriptEvent*>((*handle)->object) : nullptr;
    if (event->id == ScriptEvent::EventId)
    {
        event = reinterpret_cast<ScriptEvent*>((*handle)->object);
        u16 state = event->starter & ResourceIndexMask;
        if (ProjectileStateOfId(&state) != 0)
        {
            ObjectNodeBase* prototype = Prototype();
            prototype->focusInstance = event->originator;
            if (event->originator != nullptr)
            {
                prototype->flags.focusInstance = 1;
            }

            prototype->flags.focusPosition = 0;
            SetState(GetContextClock(owner), state);
        }
    }

    ReleaseEvent(handle);
}

void ProjectileObjectNode::SetState(TimeClock* clock, u32 state)
{
    CustomProjectile* projectile = g_CustomCommandSlots.projectiles[CustomSlot()];
    if (State() != state)
    {
        Enter(state);
        switch (State())
        {
        case StateAsleep:
            nearDistance = 0;
            CallVirtual<void>(this, vtable, ObjectNodeBase::ReleasePartsUnlessUnloadingSlot);
            CallVirtual<u32>(owner, owner->vtable, InstanceContext::SleepSlot);
            return;
        case StateLaunched:
            if (!bits.velocityGiven)
            {
                // Along its facing at the custom projectile's speed
                speed = projectile->speed;
                ObjectPlace* place = owner->place;
                RotateAndTranslate(place);
                velocity = *RowOf(&place->matrix, 2);
                velocity.x = velocity.x * speed;
                velocity.y = velocity.y * speed;
                velocity.z = velocity.z * speed;
            }

            lastBounce = 0.0f;
            bits.state = StateFlying;
            launchTime = clock->time;
            if (projectile->flags.playersShot)
            {
                Aim(projectile);
            }

            break;
        case StateEnding:
            launchTime = clock->time;
            owner->flags.visible = 0;
            owner->flags.collisionActive = 0;
            CallVirtual<void>(this, vtable, ObjectNodeBase::DestroyParticleTrailsSlot);
            break;
        case StateBouncing:
            BounceOffFocus(clock);
            bits.state = StateFlying;
            break;
        default:
            break;
        }
    }

    g_CustomProjectilePacks[CustomSlot()][State()].Run(this);
}

u32 ProjectileObjectNode::Update(TimeClock* clock)
{
    constexpr f32 EndingTime = 0.5f;
    if (clock->flags.running == 0)
    {
        return GameNode::Update(clock);
    }

    switch (State())
    {
    case StateLaunched:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        break;
    case StateFlying:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        Fly(clock);
        break;
    case StateHitCrate:
    case StateHitCreature:
    case StateHitObject:
    case StateHitCharacter:
    case StateMissed:
        SetState(clock, StateEnding);
        break;
    case StateEnding:
        if (EndingTime < SecondsBetween(launchTime, clock->time))
        {
            SetState(clock, StateAsleep);
        }

        break;
    default:
        break;
    }

    return GameNode::Update(clock);
}

void ProjectileObjectNode::Fly(TimeClock* clock)
{
    constexpr f32 MostFlight = Rounded(2.2);
    // The query allows 40 results where retail's array has room for 32 (the line of sight keeps only the instance it hits)
    constexpr u16 MostResults = 40;
    constexpr u32 ResultsRoom = 32;
    f32 flown = SecondsBetween(launchTime, clock->time);
    f32 step = LastAdvance(clock);
    if (MostFlight < flown)
    {
        SetState(clock, StateMissed);
        return;
    }

    CustomProjectile* projectile = g_CustomCommandSlots.projectiles[CustomSlot()];
    ObjectPlace* place = owner->place;
    place->SyncPosition();
    Vector4 start = place->position;
    place = owner->place;
    RotateAndTranslate(place);
    Vector4 way = *RowOf(&place->matrix, 2);
    if (projectile->flags.homes && bits.homing)
    {
        auto* homedOn = static_cast<InstanceContext*>(ObjectOf(target));
        if (homedOn == nullptr)
        {
            bits.homing = 0;
            way = velocity;
        }
        else if (projectile->flags.homesForATime && !(flown < projectile->homingTime))
        {
            way = velocity;
        }
        else
        {
            // Its facing turned toward the target (the facing it had before taken along)
            ObjectPlace* targetPlace = homedOn->place;
            targetPlace->SyncPosition();
            Vector4 aim = targetPlace->position;
            aim.y = aim.y + aimHeight;
            TurnFacingToward(projectile->turn * projectile->sideTurnScale, projectile->turn, owner, &aim);
            way.x = way.x * speed;
            way.y = way.y * speed;
            way.z = way.z * speed;
            velocity = way;
        }
    }
    else
    {
        if (projectile->flags.falls)
        {
            velocity.y = velocity.y - projectile->gravity * step;
        }

        way = velocity;
    }

    way.x = way.x * step;
    way.y = way.y * step;
    way.z = way.z * step;
    void* results[ResultsRoom];
    InstanceQuery query;
    MakeQuery(&query, results, MostResults);
    SkipInQuery(&query, owner);
    u32 kinds = projectile->flags.playersShot ? PlayersShotHitKinds : HitKinds;
    if (LineOfSight(owner->chunk, &start, &way, SurfaceFlags::SolidToObjects, &query, kinds) == 0)
    {
        InstanceContext* instance = owner;
        if (instance->place->MoveBy(&way))
        {
            QueueObject(instance);
        }

        return;
    }

    // The way shortened to where it hit (retail also works out its end, and later scales it by the hit scale, never reading
    // either)
    InstanceContext* instance = owner;
    if (instance->place->MoveBy(&way))
    {
        QueueObject(instance);
    }

    auto* hit = static_cast<InstanceContext*>(query.instance);
    if (hit == nullptr)
    {
        SetState(clock, StateMissed);
        return;
    }

    if (message != NoMessage)
    {
        InstanceContext* sender = owner;
        Reference* handle = sender != nullptr ? AddReference(sender) : nullptr;
        GameEvent* event = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))), message, &handle,
                                                ObjectNodeKinds);
        handle = event != nullptr ? AddEventReference(event) : nullptr;
        QueueEvent(hit, &handle);
    }

    AgentNode* agentNode = AgentNodeOf(hit);
    auto* part = static_cast<BasicAgentPart*>(agentNode->agent->part);
    if (!bits.noBounce && part->bits.bulletsBounceBack != 0)
    {
        BounceOff(clock, hit, &start);
        return;
    }

    GameNode* const* nodes = hit->nodes.nodes;
    if (nodes[NodeCreature] != nullptr)
    {
        SetState(clock, StateHitCreature);
    }
    else if (nodes[NodeCrate] != nullptr)
    {
        SetState(clock, StateHitCrate);
    }
    else if (nodes[NodeGenericObject] != nullptr || nodes[NodePayGate] != nullptr || nodes[NodeUnusedObjectType] != nullptr)
    {
        SetState(clock, StateHitObject);
    }
    else if (nodes[NodeCharacter] != nullptr)
    {
        SetState(clock, StateHitCharacter);
    }
}

void ProjectileObjectNode::BounceOff(TimeClock* clock, InstanceContext* other, const Vector4* point)
{
    constexpr f32 BounceInterval = Rounded(0.3);
    // The way from the middle scaled by 0.25 to 1 on each axis
    constexpr f32 Spread = 0.75f;
    constexpr f32 LeastSpread = 0.25f;
    f32 now = static_cast<f32>(static_cast<s32>(clock->time)) * g_SecondsPerClockUnit;
    if (!(BounceInterval < now - lastBounce))
    {
        return;
    }

    lastBounce = now;
    g_CustomProjectilePacks[CustomSlot()][StateBouncing].Run(this);
    // (retail reads its facing here and never uses it)
    RotateAndTranslate(owner->place);
    const Box* box = other->CollisionBox();
    Vector4 middle = box->max;
    Vector4 away = *point;
    middle.x = (middle.x - box->min.x) * 0.5f + box->min.x;
    middle.y = (middle.y - box->min.y) * 0.5f + box->min.y;
    middle.z = (middle.z - box->min.z) * 0.5f + box->min.z;
    away.x = away.x - middle.x;
    away.y = away.y - middle.y;
    away.z = away.z - middle.z;
    f32 spreadX = RandomBelowFloat(Spread) + LeastSpread;
    f32 spreadY = RandomBelowFloat(Spread) + LeastSpread;
    f32 spreadZ = RandomBelowFloat(Spread) + LeastSpread;
    away.x = away.x * spreadX;
    away.y = away.y * spreadY;
    away.z = away.z * spreadZ;
    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x = away.x * inverse;
    away.y = away.y * inverse;
    away.z = away.z * inverse;
    ReflectAcross(&velocity, &away, 0);

    // Turned to face the way it goes
    Vector4 direction = velocity;
    inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    Vector4 side;
    Vector4 up;
    AxesAround(&direction, &side, &up);
    Matrix4x4 matrix;
    MatrixFromAxes(&matrix, &side, &up, &direction);
    InstanceContext* instance = owner;
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    f32 distance = speed * LastAdvance(clock);
    Vector4 move = {direction.x * distance, direction.y * distance, direction.z * distance, direction.w};
    instance = owner;
    if (instance->place->MoveBy(&move))
    {
        QueueObject(instance);
    }

    bits.homing = 0;
}

void ProjectileObjectNode::BounceOffFocus(TimeClock* clock)
{
    InstanceContext* focus = Prototype()->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    ObjectPlace* place = owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    BounceOff(clock, focus, &position);
}

void ProjectileObjectNode::Aim(CustomProjectile*)
{
    // Closer than a hundredth of a unit it isn't turned to the target
    constexpr f32 NearSquared = Rounded(1e-4);
    constexpr f32 BlastPitch = 90.0f;
    // The character that operates the shooter (its parent when it has one), whose instance the game keeps as the one operated
    auto* shooterInstance = static_cast<InstanceContext*>(owner->collision.leftOut);
    InstanceContext* operated = shooterInstance->parent != nullptr ? shooterInstance->parent : shooterInstance;
    AssignReference(&g_OperatedInstance, operated);
    NodeList* nodes = &operated->nodes;
    PlayerCharacter* character = static_cast<PlayerNode*>(GetGameNode(nodes, NodeCharacter))->character;
    Vehicle* vehicle = character->vehicle;
    Gun* gun = character->gun;
    if (vehicle != nullptr)
    {
        // What it rides moves the shot along
        Vector4 carried;
        CallVirtual<void>(vehicle, vehicle->vtable, Vehicle::SlotVelocity, &carried);
        velocity.x = velocity.x + carried.x;
        velocity.y = velocity.y + carried.y;
        velocity.z = velocity.z + carried.z;
    }

    Vector4 aim;
    if (TargetPosition(gun, &aim) != 0)
    {
        // Turned to face the target, homing in on what's locked on (else what the character's object node has as AgentRef1)
        ObjectPlace* place = owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        Vector4 way = aim;
        way.x = way.x - position.x;
        way.y = way.y - position.y;
        way.z = way.z - position.z;
        if (NearSquared < way.x * way.x + way.y * way.y + way.z * way.z)
        {
            f32 inverse = InverseLength(&way, LengthEpsilon);
            way.x = way.x * inverse;
            way.y = way.y * inverse;
            way.z = way.z * inverse;
            Matrix4x4 matrix;
            MatrixFacing(&matrix, &way);
            InstanceContext* instance = owner;
            place = instance->place;
            place->SyncRotation();
            Vector4 rotation;
            GetRotationVec(&rotation, &matrix);
            if (place->TurnTo(&rotation))
            {
                QueueObject(instance);
            }
        }

        AssignReference(&target, LockedOnInstance(gun));
        if (ObjectOf(target) == nullptr)
        {
            auto* objectNode = static_cast<ObjectNode*>(GetGameNode(nodes, NodeObject));
            if (objectNode != nullptr)
            {
                InstanceContext* agentRef1 = objectNode->agentRef1;
                if (agentRef1 != nullptr && agentRef1->flags.asleep)
                {
                    objectNode->agentRef1 = nullptr;
                }

                AssignReference(&target, objectNode->agentRef1);
            }
        }

        bits.homing = ObjectOf(target) != nullptr;
        return;
    }

    // Without a target: turned the way the operated character is, and a quarter turn down during a downward blast
    ObjectPlace* operatedPlace = operated->place;
    const JumpController* jump = character->jump;
    operatedPlace->SyncRotation();
    Vector4 rotation = operatedPlace->rotation;
    InstanceContext* instance = owner;
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    if (jump == nullptr || jump->bits.state - DownwardBlastFirst >= DownwardBlastStates)
    {
        return;
    }

    s32 pitch;
    AngleFrom(&pitch, BlastPitch, AngleDegrees);
    instance = owner;
    if (TurnByPitch(instance->place, pitch))
    {
        QueueObject(instance);
    }
}

void ClearCustomProjectiles()
{
    for (u8 slot = 0; slot < CustomProjectileSlots; slot++)
    {
        if (g_CustomCommandSlots.projectiles[slot] != nullptr)
        {
            MemoryDeallocate2_(g_CustomCommandSlots.projectiles[slot]);
        }

        if (g_CustomProjectilePacks[slot] != nullptr)
        {
            for (ScriptPack* pack = g_CustomProjectilePacks[slot] + ArrayCount(g_CustomProjectilePacks[slot]);
                 pack != g_CustomProjectilePacks[slot];)
            {
                pack--;
                DestroyScriptPack(pack, 0);
            }

            DeleteArray(g_CustomProjectilePacks[slot]);
        }

        ScriptCommand* command = g_CustomProjectileCommands[slot];
        if (command != nullptr)
        {
            CallVirtual<void>(command, command->vtable, ScriptCommand::DestroySlot, u32{DestroyAndFree});
        }
    }

    ResetCustomProjectiles();
}

void ResetCustomProjectiles()
{
    for (u8 slot = 0; slot < CustomProjectileSlots; slot++)
    {
        g_CustomCommandSlots.projectiles[slot] = nullptr;
        g_CustomProjectilePacks[slot] = nullptr;
        g_CustomProjectileCommands[slot] = nullptr;
        g_CustomProjectileIdsGiven[slot] = 0;
    }
}

void SetUpCustomProjectile(CodeModel* model)
{
    constexpr f32 HitScale = 10.0f;
    u8 slot = model->slot;
    g_CustomProjectilePacks[slot] = model->packs;
    g_CustomProjectileCommands[slot] = model->command;
    if (g_CustomCommandSlots.projectiles[slot] != nullptr)
    {
        MemoryDeallocate2_(g_CustomCommandSlots.projectiles[slot]);
    }

    auto* projectile = static_cast<CustomProjectile*>(MemoryAllocate(sizeof(CustomProjectile)));
    projectile->flags.value &= ~CustomProjectileFlags::ClearedBits;
    g_CustomCommandSlots.projectiles[slot] = projectile;
    projectile->hitScale = HitScale;
    projectile->speed = 0.0f;
    projectile->unused08 = 0;
    projectile->homingTime = 0.0f;
    projectile->unused20 = 0;
    projectile->gravity = 0.0f;
    projectile->turn = 0.0f;
    ProjectileObjectNode node;
    ProjectileObjectNode::Construct(&node);
    node.SetCustomSlot(slot);
    ScriptCommand* command = g_CustomProjectileCommands[slot];
    CallVirtual<void>(command, command->vtable, ScriptCommand::ExecuteOnSlot, &node);
    node.Destroy(DestroyOnly);
    if (g_CustomProjectileIdsGiven[slot] == 0)
    {
        g_CustomProjectileIdsGiven[slot] = 1;
        g_CustomProjectileStateIds = model->packIds;
    }
}

u32 ProjectileStateOfId(u16* id)
{
    for (u16 state = 0; state < ProjectileStates; state++)
    {
        if (g_CustomProjectileStateIds[state] == *id)
        {
            *id = state;
            return 1;
        }
    }

    return 0;
}

void MakePyramidHull()
{
    constexpr f32 HalfWidth = Rounded(0.1);
    constexpr f32 HalfHeight = 4.0f;
    Vector4 points[5] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {-HalfWidth, -HalfHeight, 1.0f, 1.0f},
        {HalfWidth, -HalfHeight, 1.0f, 1.0f},
        {HalfWidth, HalfHeight, 1.0f, 1.0f},
        {-HalfWidth, HalfHeight, 1.0f, 1.0f},
    };
    BuildPyramidHull(&g_PyramidHull, points);
}

GameEvent* GameEvent::Construct(GameEvent* event, u32 message, Reference** argument, u32 kinds)
{
    // The handle passed by value is copied for the constructor's parameter and again for its base class's, which keeps one more;
    // the copies are let go as each constructor ends, the handle last
    Reference* parameter = CopyHandle(*argument);
    Reference* baseParameter = CopyHandle(parameter);
    event->id = EventId;
    event->vtable = g_GameEventVTable;
    event->kinds = kinds;
    event->reference = nullptr;
    event->message = 0;
    event->argument = CopyHandle(baseParameter);
    RemoveReference(&baseParameter);
    event->vtable = g_ArgumentEventVTable;
    RemoveReference(&parameter);
    event->message = static_cast<u16>(message);
    event->vtable = g_TriggerEventVTable;
    RemoveReference(argument);
    return event;
}
