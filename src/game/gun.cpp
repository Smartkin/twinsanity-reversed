#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/objectnode.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/reference.h"

// Cortex's and the Mecha-Bandicoot's gun: drawn while its character is the one played, its target lock searching while it's out,
// square firing a shot, or held long enough, a charged one; its bits keep the ammo and a second count

EABI_EXPORT(FUN_00149d78, &Gun::Shoot);
EABI_EXPORT(FUN_0014a100, &Gun::Frame);

namespace
{
// A spring of an instance's attachments: the instance at its other end and its bits (4-6 its kind)
struct Spring
{
    static constexpr u32 KindMask = 0x70;

    u8 unknown00[0x94];
    Reference* other;
    u8 unknown98[0x20];
    u32 bits;
};

// The springs tying an instance to others: their count in bits 0-4
struct SpringSet
{
    static constexpr u32 CountMask = 0x1F;

    Spring* springs[16];
    u32 count;
};

// An instance's attachments (its kind 6 node): its springs
struct AttachmentsNode
{
    u8 unknown00[0x70];
    SpringSet* springs;
};

constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
// The agents' node kinds the locks take
constexpr u32 CrateNodeKind = 0xD;
constexpr u32 CreatureNodeKind = 0xF;
constexpr u32 GenericObjectNodeKind = 0x10;
// The characters (their first int property)
constexpr s32 Cortex = 1;
constexpr s32 MechaBandicoot = 5;
// The area (the progress's) where Cortex's gun aims with a lock of its own
constexpr u32 GunArea = 24;
// The part's attack kinds of the slide and its move bit of the crawl: the gun doesn't start charging during them
constexpr u8 AttackSlide = 8;
constexpr u8 AttackSlideKind12 = 12;
constexpr u32 MoveCrawling = 0x200;
// The events run on the character
constexpr u32 EventDrawn = 0x47;
constexpr u32 EventPutAway = 0x48;
constexpr u32 EventShot = 0x49;
constexpr u32 EventCharged = 0x6A;
constexpr u32 EventNoAmmo = 0x6B;
// Its float properties (seconds): how long square is held to charge, the charged shot's delay, and the time after a shot (charged
// and normal, with and without the ammo)
constexpr u32 ChargeSecondsProperty = 0x32;
constexpr u32 ChargedDelayProperty = 0x33;
constexpr u32 ChargedShotSecondsProperty = 0x34;
constexpr u32 ShotSecondsProperty = 0x35;
constexpr u32 ChargedEmptySecondsProperty = 0x36;
constexpr u32 EmptySecondsProperty = 0x37;
// The ammo a shot takes, the ammo after a reset and its most, the second count's after a reset and its most
constexpr u32 ChargedShotAmmo = 5;
constexpr u32 ShotAmmo = 1;
constexpr u32 ResetAmmo = 15;
constexpr u32 AmmoLimit = 100;
constexpr u32 ResetValue9 = 5;
constexpr u32 Value9Mask = 0xF;
constexpr u32 Value9Limit = 10;
constexpr u32 NextMask = Gun::StateMask << Gun::NextShift;
constexpr u32 AmmoBits = Gun::AmmoMask << Gun::AmmoShift;
constexpr u32 Value9Bits = Value9Mask << Gun::Value9Shift;
constexpr s32 NoState = -1;

ReferencedObject* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? handle->object : nullptr;
}

s32 CharacterOf(const CharacterAgent* agent)
{
    return agent->properties->GetInt(0);
}

u32 AmmoOf(const Gun* gun)
{
    return gun->bits >> Gun::AmmoShift & Gun::AmmoMask;
}

void SetAmmo(Gun* gun, u32 ammo)
{
    gun->bits = (gun->bits & ~AmmoBits) | (ammo & Gun::AmmoMask) << Gun::AmmoShift;
}

// The lock it aims with: the Mecha-Bandicoot's, Cortex's in its area, in a vehicle or on foot
u32 LockOf(const Gun* gun)
{
    if (CharacterOf(gun->agent) == MechaBandicoot)
    {
        return Gun::LockMecha;
    }

    u32 area = G_GameController_00309914->progress.bits >> GameProgress::AreaShift & GameProgress::AreaMask;
    if (area == GunArea && CharacterOf(gun->agent) == Cortex)
    {
        return Gun::LockArea24;
    }

    return gun->agent->vehicle != nullptr ? Gun::LockVehicle : Gun::LockOnFoot;
}

// The instance of the character played (none without one)
InstanceContext* PlayedInstance()
{
    GameProgress* progress = &G_GameController_00309914->progress;
    u32 played = progress->bits >> GameProgress::CharacterShift & GameProgress::FieldMask;
    if (played == GameProgress::NoCharacter)
    {
        return nullptr;
    }

    return static_cast<InstanceContext*>(ObjectOf(progress->characters[played]));
}

// The gun's instance taken from the character's attachments' springs: the instance at the other end of the last one of no kind
void TakeGunInstance(Gun* gun)
{
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&gun->agent->instance->nodes, AttachmentsKind));
    if (attachments == nullptr || attachments->springs == nullptr)
    {
        return;
    }

    Spring* const* springs = attachments->springs->springs;
    u32 count = attachments->springs->count & SpringSet::CountMask;
    for (u32 index = 0; index < count; index++)
    {
        Spring* spring = springs[index];
        if (spring != nullptr && (spring->bits & Spring::KindMask) == 0)
        {
            AssignReference(&gun->gunInstance, ObjectOf(spring->other));
        }
    }
}

// Whether the state has lasted its time
bool StateOver(const Gun* gun, const TimeClock* clock, s32 duration)
{
    return static_cast<s32>(clock->time - gun->stateStart) >= duration;
}

// Not while sliding or crawling
bool MayCharge(const CharacterAgent* agent)
{
    auto* part = static_cast<const CharacterPart*>(agent->part);
    u8 kind = static_cast<u8>(part->bits);
    if (kind == AttackSlide || kind == AttackSlideKind12)
    {
        return false;
    }

    return (part->moveBits & MoveCrawling) == 0;
}
}

Gun* Gun::Construct(Gun* gun, CharacterAgent* agent)
{
    gun->agent = agent;
    gun->gunInstance = nullptr;
    for (TargetLock& lock : gun->locks)
    {
        TargetLock::Construct(&lock);
    }

    gun->Reset();
    TargetLock* lock = &gun->locks[LockOnFoot];
    lock->offset = g_GunOnFootLockOffset;
    lock->SetFlatShape(20.0f, 1.25f, 7.5f);
    lock->AddKind(CrateNodeKind, 3);
    lock->AddKind(GenericObjectNodeKind, 2);
    lock->AddKind(CreatureNodeKind, 1);
    lock = &gun->locks[LockVehicle];
    lock->offset = g_GunVehicleLockOffset;
    lock->SetShape(50.0f, 1.25f, 24.0f);
    lock->AddKind(GenericObjectNodeKind, 2);
    lock->AddKind(CreatureNodeKind, 1);
    lock = &gun->locks[LockMecha];
    lock->offset = g_GunMechaLockOffset;
    lock->SetShape(40.0f, 2.5f, 15.0f);
    lock->AddKind(GenericObjectNodeKind, 2);
    lock->AddKind(CreatureNodeKind, 1);
    lock = &gun->locks[LockArea24];
    lock->offset = g_GunArea24LockOffset;
    lock->SetFlatShape(50.0f, 10.0f, 10.0f);
    lock->AddKind(GenericObjectNodeKind, 1);
    return gun;
}

void Gun::Reset()
{
    duration = 0;
    stateStart = 0;
    // Retail clears the word with memset first
    bits = StatePutAway | NoNext << NextShift | ResetValue9 << Value9Shift | ResetAmmo << AmmoShift;
    shotTime = 0;
    shotCharge = -1.0f;
    for (TargetLock& lock : locks)
    {
        lock.Reset();
    }
}

u32 Gun::Shoot(f32 charge, TimeClock* clock)
{
    PropertyHolder* properties = agent->properties;
    AssignReference(&g_OperatedInstance, agent->instance);
    shotTime = clock->time;
    shotCharge = charge;
    u32 loaded;
    u32 property;
    if (0.0f < charge)
    {
        loaded = TakeAmmo(ChargedShotAmmo);
        property = loaded != 0 ? ChargedShotSecondsProperty : ChargedEmptySecondsProperty;
    }
    else
    {
        loaded = TakeAmmo(ShotAmmo);
        property = loaded != 0 ? ShotSecondsProperty : EmptySecondsProperty;
    }

    duration = static_cast<s32>(properties->GetFloat(property) * g_ClockUnitsPerSecond);
    RunAgentEvent(agent, loaded != 0 ? EventShot : EventNoAmmo, 0, 0, 1);
    return StateShot;
}

void Gun::Frame(f32 square, f32 allowed, TimeClock* clock)
{
    bool squareHeld = 0.0f < square;
    bool mayAim = 0.0f < allowed;
    s32 next = NoState;
    TargetLock* lock = &locks[LockOf(this)];
    s32 now = clock->time;
    if ((bits & StateMask) != StatePutAway && ObjectOf(gunInstance) == nullptr)
    {
        TakeGunInstance(this);
    }

    if (ObjectOf(gunInstance) != nullptr && (bits & StateMask) != StatePutAway && mayAim)
    {
        lock->Search(clock, agent->instance, static_cast<InstanceContext*>(ObjectOf(gunInstance)));
    }
    else
    {
        lock->Drop();
    }

    if ((bits & NextMask) != NoNext << NextShift)
    {
        bits = (bits & ~StateMask) | (bits >> NextShift & StateMask);
        bits = (bits & ~NextMask) | NoNext << NextShift;
        stateStart = now;
    }

    switch (bits & StateMask)
    {
    case StatePutAway:
        if (PlayedInstance() == agent->instance)
        {
            RunAgentEvent(agent, EventDrawn, 0, 0, 0);
            next = StateDrawing;
        }

        break;

    case StateDrawing:
    case StateShot:
        // Retail bug: drawing waits for the last shot's time (duration; none after a reset), not a time of its own
        if (StateOver(this, clock, duration))
        {
            next = StateOut;
        }

        break;

    case StateOut:
        if (PlayedInstance() != agent->instance)
        {
            RunAgentEvent(agent, EventPutAway, 0, 0, 0);
            next = StatePuttingAway;
        }
        else if (MayCharge(agent) && squareHeld)
        {
            next = StateCharging;
        }

        break;

    case StatePuttingAway:
        // Retail bug: so does putting it away
        if (StateOver(this, clock, duration))
        {
            next = StatePutAway;
        }

        break;

    case StateCharging:
        if (!squareHeld)
        {
            next = Shoot(0.0f, clock);
        }
        else
        {
            PropertyHolder* properties = agent->properties;
            f32 seconds = properties->GetFloat(ChargeSecondsProperty);
            if (StateOver(this, clock, static_cast<s32>(seconds * g_ClockUnitsPerSecond)))
            {
                RunAgentEvent(agent, EventCharged, 0, 0, 0);
                f32 delay = properties->GetFloat(ChargedDelayProperty);
                next = StateCharged;
                duration = static_cast<s32>(delay * g_ClockUnitsPerSecond);
            }
        }

        break;

    case StateCharged:
    {
        // No speed of its own forward or sideways until the charged shot is fired
        auto* part = static_cast<CharacterPart*>(agent->part);
        if (StateOver(this, clock, duration))
        {
            next = Shoot(1.0f, clock);
        }

        part->RequestForward(0.0f);
        part->RequestSideways(0.0f);
        break;
    }
    }

    bits = (bits & ~SquareHeld) | (squareHeld ? SquareHeld : 0);
    if (next != NoState)
    {
        bits = (bits & ~NextMask) | (next & StateMask) << NextShift;
    }
}

InstanceContext* Gun::Target()
{
    return static_cast<InstanceContext*>(ObjectOf(locks[LockOf(this)].target));
}

u32 Gun::AimPoint(Vector4* point)
{
    if (locks[LockOf(this)].TargetPoint(point) != 0)
    {
        return 1;
    }

    // Else the middle of AgentRef1's box, an asleep one forgotten first
    auto* node = static_cast<ObjectNode*>(GetGameNode(&agent->instance->nodes, ObjectNodeKind));
    if (node->agentRef1 != nullptr && (node->agentRef1->flags & ReferencedObject::FlagAsleep) != 0)
    {
        node->agentRef1 = nullptr;
    }

    InstanceContext* aimed = node->agentRef1;
    if (aimed == nullptr)
    {
        return 0;
    }

    const Box* box = aimed->CollisionBox();
    *point = box->max;
    point->x = (point->x - box->min.x) * 0.5f + box->min.x;
    point->y = (point->y - box->min.y) * 0.5f + box->min.y;
    point->z = (point->z - box->min.z) * 0.5f + box->min.z;
    return 1;
}

u32 Gun::TakeAmmo(u32 count)
{
    u32 ammo = AmmoOf(this);
    if (ammo < count)
    {
        // Retail bug: without enough ammo for the shot, the ammo left is lost
        SetAmmo(this, 0);
        return 0;
    }

    SetAmmo(this, ammo - count);
    return 1;
}

u32 Gun::AddValue9(s32 amount)
{
    u32 value = (bits >> Value9Shift & Value9Mask) + amount;
    if (value < Value9Limit)
    {
        bits = (bits & ~Value9Bits) | (value & Value9Mask) << Value9Shift;
        return 1;
    }

    bits = (bits & ~Value9Bits) | (Value9Limit - 1) << Value9Shift;
    return 0;
}

u32 Gun::AddAmmo(s32 amount)
{
    u32 ammo = AmmoOf(this) + amount;
    if (ammo < AmmoLimit)
    {
        SetAmmo(this, ammo);
        return 1;
    }

    SetAmmo(this, AmmoLimit - 1);
    return 0;
}
