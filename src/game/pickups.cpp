#include "game/objectnode.h"

#include "game/agentlab.h"
#include "game/clock.h"
#include "game/context.h"
#include "game/events.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"

#include <cstddef>
#include <cstdint>

// The custom pickups (pickups of subtypes 16 and 17): their object node (kind 1), which the code model of its custom slot drives
// through its states, the code models' slots and the spin the idle ones share. The events' handle release sits here too, where
// retail has it

struct ChunkEntry;

// What a pickup code model sets up for its slot (0x18 bytes, its flags' low 11 bits the SetPickup command's): its radius (and its
// square), the pull of its focus on it, the speed it flees its focus at and its flags (bit 6: it spins while it's idle)
struct CustomPickup
{
    enum Flags : u32
    {
        FlagsMask = 0x7FF,
        Spins = 0x40,
    };

    f32 radius;
    f32 radiusSquared;
    f32 pull;
    f32 fleeSpeed;
    u32 flags;
    u32 unknown14;
};
CHECK_SIZE(CustomPickup, 0x18);

// A custom pickup's object node (0xD0 bytes, vtable InstanceNodeType1_Methods over the prototype's): the prototype's part first
// (ObjectNodeBase's first 0xA0 bytes, whose functions of the prototype work on it), then its state bits (bit fields of the 64
// bits from 0xA0 in retail, its trails' pointer in their upper half: its state, the one before it and its custom slot), its
// particle trails, the physics body it was launched as, its place in the spin's frames, its velocity and the clock time its waits
// count from. Its states are its code model's: entering 7 runs the model's pack of it
struct PickupObjectNode : GameNode
{
    enum Bits : u32
    {
        StateMask = 0x1F,
        PreviousShift = 5,
        CustomSlotShift = 10,
        // Set from outside: it isn't made idle while it is
        KeepsState = 0x80000,
    };

    enum State : u32
    {
        // Asleep with its instance
        StateAsleep = 0,
        // Made idle by its next update
        StateAppearing = 2,
        // In place, spinning when its custom pickup says
        StateIdle = 4,
        // Flying to its focus (to the focus's focus when it hands on), collected once it's there
        StateAttracted = 6,
        // Its code model's pack of the state run, then asleep
        StateCollected = 7,
        // Flying away from its focus, asleep once it's 40 units from where it started
        StateFleeing = 8,
        // Asleep at its next update
        StateGone = 9,
    };

    // Its flags beyond the prototype's
    enum Flags : u32
    {
        // It has a physics body
        FlagLaunched = 0x4,
        // It flies on to its focus's own focus once it reaches it (until that's the player's instance)
        FlagHandsOn = 0x100,
    };

    static constexpr u32 CodeModelKind = 0x11;

    u8 prototype[0xA0 - sizeof(GameNode)];
    u32 bits;
    ParticleTrails* trails;
    SphereBody* body;
    s32 spinPhase;
    Vector4 velocity;
    u32 waitStart;
    u8 unknownC4[0xD0 - 0xC4];

    // Made for a chunk's instance, and made without one (the prototype's part: game/objectnodeparts.cpp)
    static PickupObjectNode* Construct(PickupObjectNode* node, ChunkEntry* chunk) RETAIL(InitInstanceNodeType1);
    static PickupObjectNode* ConstructPrototype(PickupObjectNode* node) RETAIL(FUN_0023d150);
    static PickupObjectNode* ConstructPrototype(PickupObjectNode* node, ChunkEntry* chunk) RETAIL(FUN_0023d1b8);
    void Initialise();

    // Its vtable's functions: 1 an event handled (a script event's starter taken as the ID of a state while its instance has its
    // flag 4, its focus the event's originator), 2 the destructor, 3 given its instance (where it starts taken from it), 7 its
    // step once its instance starts again (made as new, put back where it started), 8 its update, 11 put to sleep (its trails and
    // body let go, but while every chunk is being unloaded), 12 given its agent (its custom slot its first integer property), 13
    // made as new (idle, its collision placed where it started), 14 and 15 (no packets) no, 16 its collision placed with its
    // instance again, 17 its collision placed where it started, 23 a particle trail added (its trails made the first time: the
    // trail's slot), 24 its trails let go, 25 its custom slot, 26 launched (as a sphere body pushed with a velocity, when there's
    // a gravity), 41 its code model's kind, 42 a runner's packet ended (it has no runners), 43 nothing
    void HandleEvent(Reference** handle) RETAIL(FUN_0010a298);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011e570);
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0011e708);
    void Restart(TimeClock* clock) RETAIL(FUN_0011e750);
    u32 Update(TimeClock* clock) RETAIL(FUN_00109520);
    void Sleep() RETAIL(FUN_0011e5b8);
    void SetAgent(Agent* agent) RETAIL(FUN_0011e600);
    void Reset() RETAIL(FUN_0011e7a8);
    u32 Slot14() RETAIL(FUN_0011e3d0);
    u32 TakesPackets() RETAIL(FUN_0011e3d8);
    u32 UnpinCollision() RETAIL(FUN_0011e658);
    u32 PinCollision() RETAIL(FUN_0011e698);
    u32 AddTrail(const void* arguments) RETAIL(FUN_0011ea18);
    void ClearTrails() RETAIL(FUN_0011ea70);
    u32 CustomSlot() RETAIL(FUN_0011e3f0);
    void Launch(f32 gravity, const Vector4* launchVelocity) RETAIL_N32(FUN_00109290);
    u32 CustomKind() RETAIL(FUN_0011e408);
    u32 PacketEnded() RETAIL(FUN_0011e3c8);
    void Nothing43() RETAIL(FUN_0011e3e0);

    // Its state changed (nothing when it's the state it's in): what entering the new one does, which can refuse it
    void SetState(TimeClock* clock, u32 state) RETAIL(FUN_00108e68);
    // Its frames in states 6 and 8 (the custom pickup and the clock), its sideways kick when it starts flying to its focus and
    // its physics body let go
    void FlyToFocus(CustomPickup* pickup, TimeClock* clock) RETAIL(FUN_00109828);
    void FlyAway(CustomPickup* pickup, TimeClock* clock) RETAIL(FUN_0010a048);
    void KickSideways() RETAIL(FUN_00109f10);
    void ReleaseBody() RETAIL(FUN_0011e848);

    u32 State() const
    {
        return bits & StateMask;
    }

    // Its custom slot's bits written (5 bits of a value)
    void SetCustomSlot(u32 slot)
    {
        bits = (bits & ~(StateMask << CustomSlotShift)) | (slot & StateMask) << CustomSlotShift;
    }

    // The state it's in made the one before, the new one written
    void Enter(u32 state)
    {
        bits = (bits & ~(StateMask << PreviousShift)) | (bits & StateMask) << PreviousShift;
        bits = (bits & ~StateMask) | (state & StateMask);
    }

    // Its prototype's part, which ObjectNodeBase has as its first 0xA0 bytes
    ObjectNodeBase* Prototype()
    {
        return reinterpret_cast<ObjectNodeBase*>(this);
    }
};
CHECK_OFFSET(PickupObjectNode, bits, 0xA0);
CHECK_OFFSET(PickupObjectNode, velocity, 0xB0);
CHECK_OFFSET(PickupObjectNode, waitStart, 0xC0);
CHECK_SIZE(PickupObjectNode, 0xD0);

constexpr u32 CustomPickupSlots = 5;
// The frames of the idle pickups' spin (a 0.8 second loop of sixtieths), and how far apart the pickups made one after the other
// start in it
constexpr s32 SpinFrames = 48;
constexpr s32 SpinPhaseStep = 8;

extern "C"
{
    extern const GccVTableEntry g_PickupObjectNodeVTable[] RETAIL(InstanceNodeType1_Methods);
    // Each slot's custom pickup, the packs its states run (made with new[], from its code model), the command its code model runs
    // on a node of the slot when it's set up (a ScriptCommand), and whether the slot gave the states' IDs (the IDs of the first
    // code model a slot had, for every slot)
    extern CustomPickup* g_CustomPickups[CustomPickupSlots + 1] RETAIL(D_003D1E20);
    extern ScriptPack* g_CustomPickupPacks[CustomPickupSlots + 1] RETAIL(D_003D1E38);
    extern ScriptCommand* g_CustomPickupCommands[CustomPickupSlots + 1] RETAIL(G_CodeModelCommand);
    extern u8 g_CustomPickupIdsGiven[8] RETAIL(G_AllocatedForCustomScriptIds);
    extern u16* g_CustomPickupStateIds RETAIL(D_0030AAA8);
    // The spin: a matrix per frame (turning about y, bobbing up and down), the frame now and the time into the loop, the place in
    // it the next pickup made gets
    extern Matrix4x4 g_PickupSpinMatrices[SpinFrames] RETAIL(D_0030AEA0);
    extern s32 g_PickupSpinFrame RETAIL(D_003098E0);
    extern f32 g_PickupSpinTime RETAIL(D_003098E4);
    extern s32 g_NextPickupSpinPhase RETAIL(D_003098DC);

    // The static constructor: the spin's matrices made; the slots made empty (the static constructor), and their pickups, packs
    // and commands destroyed first
    void InitPickupSpin() RETAIL(FUN_00108d70);
    void ResetCustomPickups() RETAIL(FUN_0011eaa8);
    void ClearCustomPickups() RETAIL(FUN_0010a4b0);
    // A pickup code model's slot set up: its packs and command taken, a new custom pickup (all ones), the command run on a node
    // of the slot made for it
    void SetUpCustomPickup(CodeModel* model) RETAIL(FUN_0010a380);
    void RunCustomPickupCommand(u8 slot) RETAIL(FUN_00109410);
    // A state's ID (a behaviour starter's, of the code model's) turned into the state (its index among the IDs, of 10): whether
    // there was one
    u32 PickupStateOfId(u16* id) RETAIL(FUN_0011e9c8);
}

EABI_EXPORT(FUN_00109290, &PickupObjectNode::Launch);

namespace
{
constexpr s32 NoId = -1;
constexpr u32 ObjectNodeKind = 1;
constexpr u16 NotSeen = 0xFFFF;
constexpr u16 IdMask = 0x7FFF;
constexpr u32 PickupStates = 10;
// The node's vtable functions it calls, the instance's (put to sleep) and the command's (run on a node)
constexpr u32 SleepSlot = 11;
constexpr u32 ResetSlot = 13;
constexpr u32 UnpinCollisionSlot = 16;
constexpr u32 PinCollisionSlot = 17;
constexpr u32 ClearTrailsSlot = 24;
constexpr u32 InstanceSleepSlot = 3;
constexpr u32 ExecuteOnSlot = 4;
constexpr u32 DestroySlot = 1;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// Unseen this many stamps in the player's chunk, an instance with an ID goes to sleep
constexpr u32 LongUnseen = 0x9C5;

// The seconds between two clock times
f32 SecondsBetween(u32 from, u32 to)
{
    return static_cast<f32>(static_cast<s32>(to - from)) * g_SecondsPerClockUnit;
}

// The node's seconds since its last update (none when it has no time)
f32 SecondsSinceUpdate(const GameNode* node, const TimeClock* clock)
{
    return node->time != 0 ? SecondsBetween(node->time, clock->time) : 0.0f;
}

// The instance's seen stamp past the node's own (0xFFFF none), past the update rate's grace: 0 within it
u32 StampsUnseen(const GameNode* node)
{
    const InstanceContext* owner = node->owner;
    u32 seen = owner->seen[0] | owner->seen[1] << 8 | owner->seen[2] << 16;
    u32 since = node->unknown06;
    if (since == NotSeen || !(since < seen))
    {
        return 0;
    }

    u32 gap = seen - since;
    u32 grace = g_ObjectUpdateRate.grace;
    return grace < gap ? gap - grace : 0;
}

// The chunk of the player's instance (the word at 0xA0 without a player, as retail reads it)
ChunkData* PlayerChunk()
{
    Reference* player = g_PlayerInstance;
    std::uintptr_t instance = player != nullptr ? reinterpret_cast<std::uintptr_t>(player->object) : 0;
    return *reinterpret_cast<ChunkData* const*>(instance + offsetof(ReferencedObject, chunk));
}

Vector4 DefaultVelocity()
{
    Vector4 velocity = g_DefaultBox.min;
    velocity.w = 1.0f;
    return velocity;
}
}

void PickupObjectNode::Initialise()
{
    bits = (bits & ~(StateMask | StateMask << PreviousShift)) | StateIdle;
    vtable = g_PickupObjectNodeVTable;
    trails = nullptr;
    s32 phase = g_NextPickupSpinPhase;
    s32 next = phase + SpinPhaseStep;
    if (next >= SpinFrames)
    {
        next -= SpinFrames;
    }

    g_NextPickupSpinPhase = next;
    spinPhase = phase;
    velocity = DefaultVelocity();
    body = nullptr;
}

PickupObjectNode* PickupObjectNode::Construct(PickupObjectNode* node, ChunkEntry* chunk)
{
    ConstructPrototype(node, chunk);
    node->Initialise();
    return node;
}

void PickupObjectNode::Destroy(u32 destroyFlags)
{
    vtable = g_PickupObjectNodeVTable;
    Sleep();
    Prototype()->DestroyPrototype(destroyFlags);
}

void PickupObjectNode::Sleep()
{
    if (g_UnloadingEverything != 0)
    {
        return;
    }

    CallVirtual<void>(this, vtable, ClearTrailsSlot);
    ReleaseBody();
}

void PickupObjectNode::ReleaseBody()
{
    if (body != nullptr)
    {
        RemovePhysicsBody(g_PhysicsWorld, body);
        body = nullptr;
    }

    Prototype()->flags &= ~FlagLaunched;
}

void PickupObjectNode::SetAgent(Agent* agent)
{
    ObjectNodeBase* prototype = Prototype();
    prototype->SetAgent(agent);
    if (prototype->properties != nullptr)
    {
        SetCustomSlot(static_cast<u32>(prototype->properties->GetInt(0)));
    }
}

u32 PickupObjectNode::UnpinCollision()
{
    ObjectCollision* collision = &owner->collision;
    if (collision->hullMatrix != nullptr)
    {
        MemoryDeallocate2_(collision->hullMatrix);
        collision->hullMatrix = nullptr;
    }

    return 1;
}

u32 PickupObjectNode::PinCollision()
{
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    SetMatrixPosition(&matrix, &Prototype()->information.position);
    SetCollisionMatrix(&owner->collision, &matrix);
    velocity = DefaultVelocity();
    return 1;
}

void PickupObjectNode::SetOwner(InstanceContext* instance)
{
    Prototype()->information.Take(instance, 1);
    GameNode::SetOwner(instance);
}

void PickupObjectNode::Restart(TimeClock* clock)
{
    CallVirtual<void>(this, vtable, ResetSlot, owner);
    Prototype()->information.Apply(owner);
    time = clock->time;
}

void PickupObjectNode::Reset()
{
    CallVirtual<void>(this, vtable, ClearTrailsSlot);
    ReleaseBody();
    velocity = DefaultVelocity();
    waitStart = 0;
    bits = (bits & ~(StateMask | StateMask << PreviousShift)) | StateIdle;
    CallVirtual<u32>(this, vtable, PinCollisionSlot);
}

u32 PickupObjectNode::AddTrail(const void* arguments)
{
    if (trails == nullptr)
    {
        trails = ParticleTrails::Construct(static_cast<ParticleTrails*>(MemoryAllocate(sizeof(ParticleTrails))));
    }

    return trails->Add(arguments);
}

void PickupObjectNode::ClearTrails()
{
    if (trails != nullptr)
    {
        trails->Destroy(DestroyAndFree);
        trails = nullptr;
    }
}

u32 PickupObjectNode::CustomSlot()
{
    return bits >> CustomSlotShift & StateMask;
}

u32 PickupObjectNode::Slot14()
{
    return 0;
}

u32 PickupObjectNode::TakesPackets()
{
    return 0;
}

u32 PickupObjectNode::CustomKind()
{
    return CodeModelKind;
}

u32 PickupObjectNode::PacketEnded()
{
    return 0;
}

void PickupObjectNode::Nothing43()
{
}

void PickupObjectNode::Launch(f32 gravity, const Vector4* launchVelocity)
{
    constexpr f32 MaxSpeed = 40.0f;
    constexpr f32 MaxSpin = 20.0f;
    constexpr f32 Radius = 0.25f;
    // Pushed half a unit above its place
    constexpr f32 PushHeight = 0.5f;
    velocity = *launchVelocity;
    if (!(0.0f < gravity) || body != nullptr)
    {
        return;
    }

    CallVirtual<u32>(this, vtable, UnpinCollisionSlot);
    body = static_cast<SphereBody*>(AddPhysicsBody(g_PhysicsWorld, owner, 1));
    body->SetMassAndSize(1.0f, 1.0f, 1.0f, 1.0f);
    body->maxSpeed = MaxSpeed;
    body->maxSpin = MaxSpin;
    body->SetRestitution(Rounded(0.3));
    body->SetFriction(1.0f);
    ObjectPlace* place = owner->place;
    place->SyncPosition();
    Vector4 point = place->position;
    point.y = point.y + PushHeight;
    body->radius = Radius;
    body->ellipsoid = 0;
    body->ApplyImpulse(launchVelocity, &point);
    Prototype()->flags |= FlagLaunched;
}

void PickupObjectNode::HandleEvent(Reference** handle)
{
    auto* event = *handle != nullptr ? reinterpret_cast<ScriptEvent*>((*handle)->object) : nullptr;
    if (event->unknown04 == ScriptEvent::EventId && (owner->flags & ReferencedObject::FlagSphereContact) != 0)
    {
        event = reinterpret_cast<ScriptEvent*>((*handle)->object);
        u16 state = event->starter & IdMask;
        if (PickupStateOfId(&state) != 0)
        {
            ObjectNodeBase* prototype = Prototype();
            prototype->focusInstance = event->originator;
            if (event->originator != nullptr)
            {
                prototype->flags |= ObjectNodeBase::FlagFocusInstance;
            }

            prototype->flags &= ~ObjectNodeBase::FlagFocusPosition;
            SetState(GetContextClock(owner), state);
        }
    }

    ReleaseEvent(handle);
}

void PickupObjectNode::SetState(TimeClock* clock, u32 state)
{
    constexpr f32 AttractDelay = Rounded(0.1);
    constexpr f32 FleeDelay = Rounded(0.7);
    CustomPickup* pickup = g_CustomPickups[CustomSlot()];
    if (State() == state)
    {
        return;
    }

    switch (state)
    {
    case StateAsleep:
        unknown06 = 0;
        CallVirtual<void>(this, vtable, SleepSlot);
        CallVirtual<u32>(owner, owner->vtable, InstanceSleepSlot);
        break;
    case StateIdle:
        unknown06 = 0;
        if ((bits & KeepsState) != 0)
        {
            return;
        }

        break;
    case StateAttracted:
        if (SecondsBetween(waitStart, clock->time) < AttractDelay || State() != StateIdle)
        {
            return;
        }

        CallVirtual<u32>(this, vtable, UnpinCollisionSlot);
        if (owner->id != NoId)
        {
            KickSideways();
        }

        break;
    case StateCollected:
        owner->flags &= ~ReferencedObject::FlagSphereContact;
        Enter(state);
        g_CustomPickupPacks[CustomSlot()][State()].Run(this);
        SetState(clock, StateAsleep);
        return;
    case StateFleeing:
    {
        if (SecondsBetween(waitStart, clock->time) < FleeDelay)
        {
            return;
        }

        CallVirtual<u32>(this, vtable, UnpinCollisionSlot);
        unknown06 = NotSeen;
        InstanceContext* focus = Prototype()->AwakeFocus();
        if (focus == nullptr)
        {
            // Retail bug: it goes on to flee once it's made idle (with the velocity it had), the return after it is missing
            SetState(clock, StateIdle);
            break;
        }

        // Away from the focus at the custom pickup's speed
        ObjectPlace* place = focus->place;
        place->SyncPosition();
        const Vector4* start = &Prototype()->information.position;
        velocity = place->position;
        velocity.x = -(velocity.x - start->x);
        velocity.y = -(velocity.y - start->y);
        velocity.z = -(velocity.z - start->z);
        f32 inverse = InverseLength(&velocity, LengthEpsilon);
        velocity.x = velocity.x * inverse;
        velocity.y = velocity.y * inverse;
        velocity.z = velocity.z * inverse;
        f32 speed = pickup->fleeSpeed;
        velocity.x = velocity.x * speed;
        velocity.y = velocity.y * speed;
        velocity.z = velocity.z * speed;
        break;
    }
    default:
        break;
    }

    Enter(state);
}

void PickupObjectNode::KickSideways()
{
    constexpr f32 Kick = 13.0f;
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    Vector4 facing = *RowOf(&place->matrix, 2);
    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    // Square to its facing and the up, to either side
    const Vector4* a = &facing;
    const Vector4* b = &up;
    if ((RandomInt() & 1) == 0)
    {
        a = &up;
        b = &facing;
    }

    f32 x = a->y * b->z - a->z * b->y;
    f32 z = a->x * b->y - a->y * b->x;
    f32 y = a->z * b->x - a->x * b->z;
    velocity.x = velocity.x + x * Kick;
    velocity.y = velocity.y + y * Kick;
    velocity.z = velocity.z + z * Kick;
}

u32 PickupObjectNode::Update(TimeClock* clock)
{
    if ((clock->flags & TimeClock::FlagRunning) == 0)
    {
        return GameNode::Update(clock);
    }

    u32 unseen = StampsUnseen(this);
    if (unseen >= LongUnseen && owner->id != NoId && PlayerChunk() == owner->chunk)
    {
        CallVirtual<void>(this, vtable, SleepSlot);
        CallVirtual<u32>(owner, owner->vtable, InstanceSleepSlot);
        bits &= ~StateMask;
        return GameNode::Update(clock);
    }

    if (unseen >= g_ObjectUpdateRate.cutoff)
    {
        return GameNode::Update(clock);
    }

    CustomPickup* pickup = g_CustomPickups[CustomSlot()];
    switch (State())
    {
    case StateAppearing:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        SetState(clock, StateIdle);
        break;
    case StateIdle:
        if ((pickup->flags & CustomPickup::Spins) != 0)
        {
            s32 frame = g_PickupSpinFrame + spinPhase;
            if (frame >= SpinFrames)
            {
                frame -= SpinFrames;
            }

            // The spin's matrix where it started
            const Vector4* start = &Prototype()->information.position;
            Matrix4x4 matrix = g_PickupSpinMatrices[frame];
            matrix.m[3][0] = matrix.m[3][0] + start->x;
            matrix.m[3][1] = matrix.m[3][1] + start->y;
            matrix.m[3][2] = matrix.m[3][2] + start->z;
            InstanceContext* instance = owner;
            if (SetPlaceMatrix(instance->place, &matrix) != 0)
            {
                QueueObject(instance);
            }
        }

        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        break;
    case StateAttracted:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        FlyToFocus(pickup, clock);
        break;
    case StateCollected:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        SetState(clock, StateAsleep);
        break;
    case StateFleeing:
        if (trails != nullptr)
        {
            UpdateParticleTrails(trails, this);
        }

        FlyAway(pickup, clock);
        break;
    case StateGone:
        SetState(clock, StateAsleep);
        break;
    default:
        break;
    }

    return GameNode::Update(clock);
}

void PickupObjectNode::FlyToFocus(CustomPickup* pickup, TimeClock* clock)
{
    // Closer than half a unit it's there
    constexpr f32 ReachedSquared = 0.25f;
    // While the player rides something the pull is 4 times as strong and the velocity kept more
    constexpr f32 RidingPull = 4.0f;
    // A pickup with an ID is pulled by 3 times the custom pull, one without by 5
    constexpr f32 IdPull = 3.0f;
    constexpr f32 NoIdPull = 5.0f;
    // A physics body is pushed by the pull less 0.8 of its velocity
    constexpr f32 BodyDrag = Rounded(0.8);
    ObjectNodeBase* prototype = Prototype();
    InstanceContext* focus = prototype->AwakeFocus();
    if (focus == nullptr)
    {
        SetState(clock, StateIdle);
        return;
    }

    Vector4 position;
    if (body != nullptr)
    {
        position = *RowOf(&body->matrix, 3);
    }
    else
    {
        ObjectPlace* place = owner->place;
        place->SyncPosition();
        position = place->position;
    }

    ObjectPlace* focusPlace = focus->place;
    focusPlace->SyncPosition();
    Vector4 way = focusPlace->position;
    way.x = way.x - position.x;
    way.y = way.y - position.y;
    way.z = way.z - position.z;
    bool reached = way.x * way.x + way.y * way.y + way.z * way.z < ReachedSquared;
    if ((prototype->flags & FlagHandsOn) == 0)
    {
        if (reached)
        {
            SetState(clock, StateCollected);
            return;
        }
    }
    else if (reached)
    {
        // On to the focus's own focus (one asleep forgotten: then no further)
        auto* focusNode = static_cast<ObjectNodeBase*>(GetGameNode(&focus->nodes, ObjectNodeKind));
        InstanceContext* next = focus;
        if ((focusNode->flags & ObjectNodeBase::FlagFocusInstance) != 0 && focusNode->focusInstance != nullptr)
        {
            next = focusNode->AwakeFocus();
        }

        if (next != nullptr)
        {
            if ((next->nodes.mask & 1u << NodePlayer) != 0)
            {
                prototype->flags &= ~FlagHandsOn;
            }

            prototype->focusInstance = next;
            prototype->flags = (prototype->flags | ObjectNodeBase::FlagFocusInstance) & ~ObjectNodeBase::FlagFocusPosition;
        }
    }

    f32 elapsed = SecondsSinceUpdate(this, clock);
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    bool riding = g_PlayerCharacter2->control != nullptr;
    if (riding)
    {
        way.x = way.x * RidingPull;
        way.y = way.y * RidingPull;
        way.z = way.z * RidingPull;
    }

    f32 pull;
    f32 kept;
    if (owner->id != NoId)
    {
        pull = pickup->pull * (elapsed * IdPull);
        kept = riding ? Rounded(0.92) : Rounded(0.85);
    }
    else
    {
        pull = pickup->pull * (elapsed * NoIdPull);
        kept = riding ? Rounded(0.8) : Rounded(0.85);
    }

    way.x = way.x * pull;
    way.y = way.y * pull;
    way.z = way.z * pull;
    velocity.x = velocity.x + way.x;
    velocity.y = velocity.y + way.y;
    velocity.z = velocity.z + way.z;
    velocity.x = velocity.x * kept;
    velocity.y = velocity.y * kept;
    velocity.z = velocity.z * kept;
    if (body == nullptr)
    {
        Vector4 move = {velocity.x * elapsed, velocity.y * elapsed, velocity.z * elapsed, 1.0f};
        InstanceContext* instance = owner;
        if (instance->place->MoveBy(&move))
        {
            QueueObject(instance);
        }

        return;
    }

    velocity = body->velocity;
    body->force.x = body->force.x + (way.x - velocity.x * BodyDrag);
    body->force.y = body->force.y + (way.y - velocity.y * BodyDrag);
    body->force.z = body->force.z + (way.z - velocity.z * BodyDrag);
}

void PickupObjectNode::FlyAway(CustomPickup*, TimeClock* clock)
{
    // Asleep 40 units from where it started
    constexpr f32 FarSquared = 1600.0f;
    ObjectPlace* place = owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    const Vector4* start = &Prototype()->information.position;
    f32 x = position.x - start->x;
    f32 y = position.y - start->y;
    f32 z = position.z - start->z;
    if (FarSquared < x * x + y * y + z * z)
    {
        SetState(clock, StateAsleep);
        return;
    }

    f32 elapsed = SecondsSinceUpdate(this, clock);
    Vector4 move = {velocity.x * elapsed, velocity.y * elapsed, velocity.z * elapsed, 1.0f};
    InstanceContext* instance = owner;
    if (instance->place->MoveBy(&move))
    {
        QueueObject(instance);
    }
}

void StepLoopFrame(TimeClock* clock)
{
    constexpr f32 Loop = Rounded(0.8);
    constexpr f32 FramesPerSecond = 60.0f;
    if ((clock->flags & TimeClock::FlagRunning) == 0)
    {
        return;
    }

    f32 time = g_PickupSpinTime + static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    while (Loop <= time)
    {
        time = time - Loop;
    }

    g_PickupSpinTime = time;
    g_PickupSpinFrame = static_cast<s32>(time * FramesPerSecond);
}

void InitPickupSpin()
{
    // A turn about y over the loop, bobbing an eighth of a unit up and down
    constexpr s32 FrameTurn = 0x555;
    constexpr f32 Bob = 0.125f;
    s32 angle;
    AngleFrom(&angle, 0.0f, AngleRadians);
    g_NextPickupSpinPhase = 0;
    g_PickupSpinFrame = 0;
    g_PickupSpinTime = 0.0f;
    for (Matrix4x4& matrix : g_PickupSpinMatrices)
    {
        matrix = {};
        matrix.m[3][3] = 1.0f;
        s32 yaw = angle;
        MatrixAboutY(&matrix, &yaw);
        f32 bob = SinOfAngle(&angle);
        angle += FrameTurn;
        matrix.m[3][1] = bob * Bob;
    }
}

void ResetCustomPickups()
{
    for (u8 slot = 0; slot < CustomPickupSlots; slot++)
    {
        g_CustomPickups[slot] = nullptr;
        g_CustomPickupPacks[slot] = nullptr;
        g_CustomPickupCommands[slot] = nullptr;
        g_CustomPickupIdsGiven[slot] = 0;
    }
}

void ClearCustomPickups()
{
    for (u8 slot = 0; slot < CustomPickupSlots; slot++)
    {
        if (g_CustomPickups[slot] != nullptr)
        {
            MemoryDeallocate2_(g_CustomPickups[slot]);
        }

        if (g_CustomPickupPacks[slot] != nullptr)
        {
            for (ScriptPack* pack = g_CustomPickupPacks[slot] + ArrayCount(g_CustomPickupPacks[slot]);
                 pack != g_CustomPickupPacks[slot];)
            {
                pack--;
                DestroyScriptPack(pack, 0);
            }

            DeleteArray(g_CustomPickupPacks[slot]);
        }

        ScriptCommand* command = g_CustomPickupCommands[slot];
        if (command != nullptr)
        {
            CallVirtual<void>(command, command->vtable, DestroySlot, u32{DestroyAndFree});
        }
    }

    ResetCustomPickups();
}

void SetUpCustomPickup(CodeModel* model)
{
    u8 slot = model->unknown09;
    g_CustomPickupPacks[slot] = model->packs;
    g_CustomPickupCommands[slot] = model->command;
    if (g_CustomPickups[slot] != nullptr)
    {
        MemoryDeallocate2_(g_CustomPickups[slot]);
    }

    auto* pickup = static_cast<CustomPickup*>(MemoryAllocate(sizeof(CustomPickup)));
    pickup->flags &= ~CustomPickup::FlagsMask;
    g_CustomPickups[slot] = pickup;
    pickup->fleeSpeed = 1.0f;
    pickup->radius = 1.0f;
    pickup->radiusSquared = 1.0f;
    pickup->pull = 1.0f;
    pickup->unknown14 = 0;
    RunCustomPickupCommand(slot);
    if (g_CustomPickupIdsGiven[slot] == 0)
    {
        g_CustomPickupIdsGiven[slot] = 1;
        g_CustomPickupStateIds = model->packIds;
    }
}

void RunCustomPickupCommand(u8 slot)
{
    PickupObjectNode node;
    PickupObjectNode::ConstructPrototype(&node);
    node.Initialise();
    node.SetCustomSlot(slot);
    ScriptCommand* command = g_CustomPickupCommands[slot];
    CallVirtual<void>(command, command->vtable, ExecuteOnSlot, &node);
    node.Destroy(DestroyOnly);
}

u32 PickupStateOfId(u16* id)
{
    for (u16 state = 0; state < PickupStates; state++)
    {
        if (g_CustomPickupStateIds[state] == *id)
        {
            *id = state;
            return 1;
        }
    }

    return 0;
}

void ReleaseEvent(Reference** handle)
{
    using namespace ReferenceBits;
    Reference* reference = *handle;
    if (reference == nullptr)
    {
        return;
    }

    u32 value = reference->value;
    u32 count = ((value & CountMask) - 1) & CountMask;
    value = (value & ~CountMask) | count;
    reference->value = value;
    if (count == 0 && (value & Owns) != 0)
    {
        auto* event = reinterpret_cast<GameEvent*>(reference->object);
        if (event != nullptr)
        {
            CallVirtual<void>(event, event->vtable, DestroySlot, u32{DestroyAndFree});
        }

        reference->object = nullptr;
    }

    if ((reference->value & CountMask) != 0)
    {
        return;
    }

    reference = *handle;
    auto* event = reinterpret_cast<GameEvent*>(reference->object);
    if (event == nullptr)
    {
        if (reference != nullptr)
        {
            if ((reference->value & Owns) != 0)
            {
                reference->object = nullptr;
            }

            MemoryDeallocate2_(reference);
        }

        *handle = nullptr;
        return;
    }

    // The event's own block goes, with the event when the block owns it
    Reference* block = event->reference;
    if (block != nullptr)
    {
        if ((block->value & Owns) != 0)
        {
            auto* owned = reinterpret_cast<GameEvent*>(block->object);
            if (owned != nullptr)
            {
                CallVirtual<void>(owned, owned->vtable, DestroySlot, u32{DestroyAndFree});
            }

            block->object = nullptr;
        }

        MemoryDeallocate2_(block);
        event->reference = nullptr;
    }

    *handle = nullptr;
}
