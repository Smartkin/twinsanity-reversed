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
#include "game/pickups.h"
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

// The frames of the idle pickups' spin (a 0.8 second loop of sixtieths), and how far apart the pickups made one after the other
// start in it
constexpr s32 SpinFrames = 48;
constexpr s32 SpinPhaseStep = 8;

extern "C"
{
    extern const GccVTableEntry g_PickupObjectNodeVTable[] RETAIL(InstanceNodeType1_Methods);
    // Each slot's packs its states run (made with new[], from its code model; its custom pickup and the command its code model
    // runs on a node of the slot when it's set up are game/pickups.h's), and whether the slot gave the states' IDs (the IDs of
    // the first code model a slot had, for every slot)
    extern ScriptPack* g_CustomPickupPacks[CustomPickupSlots + 1] RETAIL(D_003D1E38);
    extern u8 g_CustomPickupIdsGiven[8] RETAIL(G_AllocatedForCustomScriptIds);
    extern u16* g_CustomPickupStateIds RETAIL(D_0030AAA8);
    // The spin: a matrix per frame (turning about y, bobbing up and down), the frame now and the time into the loop, the place in
    // it the next pickup made gets
    extern Matrix4x4 g_PickupSpinMatrices[SpinFrames] RETAIL(D_0030AEA0);
    extern s32 g_PickupSpinFrame RETAIL(D_003098E0);
    extern f32 g_PickupSpinTime RETAIL(D_003098E4);
    extern s32 g_NextPickupSpinPhase RETAIL(D_003098DC);

    // The command of a slot's code model run on a node of the slot made for it
    void RunCustomPickupCommand(u8 slot) RETAIL(FUN_00109410);
    // A state's ID (a behaviour starter's, of the code model's) turned into the state (its index among the IDs, of 10): whether
    // there was one
    u32 PickupStateOfId(u16* id) RETAIL(FUN_0011e9c8);
}

EABI_EXPORT(FUN_00109290, &PickupObjectNode::Launch);

namespace
{
constexpr u32 PickupStates = 10;
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
    u32 seen = owner->seen;
    u32 since = node->nearDistance;
    if (since == GameNode::AnyNearDistance || !(since < seen))
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
    MakeIdle();
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

    CallVirtual<void>(this, vtable, ObjectNodeBase::DestroyParticleTrailsSlot);
    ReleaseBody();
}

void PickupObjectNode::ReleaseBody()
{
    if (body != nullptr)
    {
        RemovePhysicsBody(g_PhysicsWorld, body);
        body = nullptr;
    }

    Prototype()->flags.value &= ~FlagLaunched;
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
    CallVirtual<void>(this, vtable, ObjectNodeBase::ResetSlot, owner);
    Prototype()->information.Apply(owner);
    time = clock->time;
}

void PickupObjectNode::Reset()
{
    CallVirtual<void>(this, vtable, ObjectNodeBase::DestroyParticleTrailsSlot);
    ReleaseBody();
    velocity = DefaultVelocity();
    waitStart = 0;
    MakeIdle();
    CallVirtual<u32>(this, vtable, ObjectNodeBase::PinCollisionSlot);
}

u32 PickupObjectNode::AddParticleTrail(const void* arguments)
{
    if (trails == nullptr)
    {
        trails = ParticleTrails::Construct(static_cast<ParticleTrails*>(MemoryAllocate(sizeof(ParticleTrails))));
    }

    return trails->Add(arguments);
}

void PickupObjectNode::DestroyParticleTrails()
{
    if (trails != nullptr)
    {
        trails->Destroy(DestroyAndFree);
        trails = nullptr;
    }
}

u32 PickupObjectNode::CustomSlot()
{
    return bits.customSlot;
}

u32 PickupObjectNode::UnusedTakesPackets()
{
    return 0;
}

u32 PickupObjectNode::TakesPackets()
{
    return 0;
}

u32 PickupObjectNode::CodeModelKind()
{
    return CodeModel::KindPickup;
}

u32 PickupObjectNode::PacketEnded()
{
    return 0;
}

void PickupObjectNode::PacketStarted()
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

    CallVirtual<u32>(this, vtable, ObjectNodeBase::UnpinCollisionSlot);
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
    Prototype()->flags.value |= FlagLaunched;
}

void PickupObjectNode::HandleEvent(Reference** handle)
{
    auto* event = *handle != nullptr ? reinterpret_cast<ScriptEvent*>((*handle)->object) : nullptr;
    if (event->id == ScriptEvent::EventId && owner->flags.collisionActive)
    {
        event = reinterpret_cast<ScriptEvent*>((*handle)->object);
        u16 state = event->starter & ResourceIndexMask;
        if (PickupStateOfId(&state) != 0)
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
        nearDistance = 0;
        CallVirtual<void>(this, vtable, ObjectNodeBase::ReleasePartsUnlessUnloadingSlot);
        CallVirtual<u32>(owner, owner->vtable, InstanceContext::SleepSlot);
        break;
    case StateIdle:
        nearDistance = 0;
        if (bits.keepsState)
        {
            return;
        }

        break;
    case StateAttracted:
        if (SecondsBetween(waitStart, clock->time) < AttractDelay || State() != StateIdle)
        {
            return;
        }

        CallVirtual<u32>(this, vtable, ObjectNodeBase::UnpinCollisionSlot);
        if (owner->id != InstanceContext::NoId)
        {
            KickSideways();
        }

        break;
    case StateCollected:
        owner->flags.collisionActive = 0;
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

        CallVirtual<u32>(this, vtable, ObjectNodeBase::UnpinCollisionSlot);
        nearDistance = GameNode::AnyNearDistance;
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
    if (clock->flags.running == 0)
    {
        return GameNode::Update(clock);
    }

    u32 unseen = StampsUnseen(this);
    if (unseen >= LongUnseen && owner->id != InstanceContext::NoId && PlayerChunk() == owner->chunk)
    {
        CallVirtual<void>(this, vtable, ObjectNodeBase::ReleasePartsUnlessUnloadingSlot);
        CallVirtual<u32>(owner, owner->vtable, InstanceContext::SleepSlot);
        bits.state = StateAsleep;
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
        if (pickup->flags.spins)
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
    // While the player rides something the pull is 4 times as strong
    constexpr f32 RidingPull = 4.0f;
    // A pickup with an ID is pulled by 3 times the custom pull, one without by 5
    constexpr f32 IdPull = 3.0f;
    constexpr f32 NoIdPull = 5.0f;
    // A physics body is pushed by the pull less 0.8 of its velocity
    constexpr f32 BodyDrag = Rounded(0.8);
    // The share of its velocity kept each update: 0.85, while the player rides something 0.92 with an ID and 0.8 without
    constexpr f32 VelocityKept = Rounded(0.85);
    constexpr f32 RidingVelocityKept = Rounded(0.92);
    constexpr f32 RidingNoIdVelocityKept = Rounded(0.8);
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
    if ((prototype->flags.value & FlagHandsOn) == 0)
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
        auto* focusNode = static_cast<ObjectNodeBase*>(GetGameNode(&focus->nodes, NodeObject));
        InstanceContext* next = focus;
        if (focusNode->flags.focusInstance && focusNode->focusInstance != nullptr)
        {
            next = focusNode->AwakeFocus();
        }

        if (next != nullptr)
        {
            if ((next->nodes.mask & 1u << NodeCharacter) != 0)
            {
                prototype->flags.value &= ~FlagHandsOn;
            }

            prototype->focusInstance = next;
            prototype->flags.focusInstance = 1;
            prototype->flags.focusPosition = 0;
        }
    }

    f32 elapsed = SecondsSinceUpdate(this, clock);
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    bool riding = g_PlayerCharacter2->vehicle != nullptr;
    if (riding)
    {
        way.x = way.x * RidingPull;
        way.y = way.y * RidingPull;
        way.z = way.z * RidingPull;
    }

    f32 pull;
    f32 kept;
    if (owner->id != InstanceContext::NoId)
    {
        pull = pickup->pull * (elapsed * IdPull);
        kept = riding ? RidingVelocityKept : VelocityKept;
    }
    else
    {
        pull = pickup->pull * (elapsed * NoIdPull);
        kept = riding ? RidingNoIdVelocityKept : VelocityKept;
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

void StepPickupSpin(TimeClock* clock)
{
    constexpr f32 Loop = Rounded(0.8);
    if (clock->flags.running == 0)
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
        g_CustomCommandSlots.pickupCommands[slot] = nullptr;
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

        ScriptCommand* command = g_CustomCommandSlots.pickupCommands[slot];
        if (command != nullptr)
        {
            CallVirtual<void>(command, command->vtable, ScriptCommand::DestroySlot, u32{DestroyAndFree});
        }
    }

    ResetCustomPickups();
}

void SetUpCustomPickup(CodeModel* model)
{
    u8 slot = model->slot;
    g_CustomPickupPacks[slot] = model->packs;
    g_CustomCommandSlots.pickupCommands[slot] = model->command;
    if (g_CustomPickups[slot] != nullptr)
    {
        MemoryDeallocate2_(g_CustomPickups[slot]);
    }

    auto* pickup = static_cast<CustomPickup*>(MemoryAllocate(sizeof(CustomPickup)));
    pickup->flags.value &= ~CustomPickupFlags::CommandBits;
    g_CustomPickups[slot] = pickup;
    pickup->fleeSpeed = 1.0f;
    pickup->radius = 1.0f;
    pickup->radiusSquared = 1.0f;
    pickup->pull = 1.0f;
    pickup->unused14 = 0;
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
    ScriptCommand* command = g_CustomCommandSlots.pickupCommands[slot];
    CallVirtual<void>(command, command->vtable, ScriptCommand::ExecuteOnSlot, &node);
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
    Reference* reference = *handle;
    if (reference == nullptr)
    {
        return;
    }

    ReferenceBits bits = reference->bits;
    bits.count--;
    reference->bits = bits;
    if (bits.count == 0 && bits.owns)
    {
        auto* event = reinterpret_cast<GameEvent*>(reference->object);
        if (event != nullptr)
        {
            CallVirtual<void>(event, event->vtable, GameEvent::DestroySlot, u32{DestroyAndFree});
        }

        reference->object = nullptr;
    }

    if (reference->bits.count != 0)
    {
        return;
    }

    reference = *handle;
    auto* event = reinterpret_cast<GameEvent*>(reference->object);
    if (event == nullptr)
    {
        if (reference != nullptr)
        {
            if (reference->bits.owns)
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
        if (block->bits.owns)
        {
            auto* owned = reinterpret_cast<GameEvent*>(block->object);
            if (owned != nullptr)
            {
                CallVirtual<void>(owned, owned->vtable, GameEvent::DestroySlot, u32{DestroyAndFree});
            }

            block->object = nullptr;
        }

        MemoryDeallocate2_(block);
        event->reference = nullptr;
    }

    *handle = nullptr;
}
