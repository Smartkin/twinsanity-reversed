#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/attachments.h"
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
// The area (the progress's) where Cortex's gun aims with a lock of its own
constexpr u32 GunArea = 24;
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
constexpr u32 ResetSecondCount = 5;
constexpr u32 SecondCountLimit = 10;

ReferencedObject* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? handle->object : nullptr;
}

s32 CharacterOf(const CharacterAgent* agent)
{
    return agent->properties->GetInt(CharacterKindProperty);
}

// The lock it aims with: the Mecha-Bandicoot's, Cortex's in its area, in a vehicle or on foot
u32 LockOf(const Gun* gun)
{
    if (CharacterOf(gun->agent) == CharacterMecha)
    {
        return Gun::LockMecha;
    }

    u32 area = g_AgentsGameController->progress.play.area;
    if (area == GunArea && CharacterOf(gun->agent) == CharacterCortex)
    {
        return Gun::LockArea24;
    }

    return gun->agent->vehicle != nullptr ? Gun::LockVehicle : Gun::LockOnFoot;
}

// The instance of the character played (none without one)
InstanceContext* PlayedInstance()
{
    GameProgress* progress = &g_AgentsGameController->progress;
    return progress->Instance(progress->play.character);
}

// The gun's instance taken from what hangs on the character: the last instance held
void TakeGunInstance(Gun* gun)
{
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&gun->agent->instance->nodes, NodeAttachments));
    if (attachments == nullptr || attachments->path == nullptr)
    {
        return;
    }

    Attachment* const* entries = attachments->path->entries;
    u32 count = attachments->path->Count();
    for (u32 index = 0; index < count; index++)
    {
        Attachment* attachment = entries[index];
        if (attachment != nullptr && attachment->bits.kind == Attachment::KindInstance)
        {
            AssignReference(&gun->gunInstance, ObjectOf(attachment->instanceReference));
        }
    }
}

// Whether the state has lasted its time
bool StateOver(const Gun* gun, const TimeClock* clock, s32 duration)
{
    return static_cast<s32>(clock->time - gun->stateStart) >= duration;
}

// Not while sliding (or the slide's variant kind) or crawling
bool MayCharge(const CharacterAgent* agent)
{
    auto* part = static_cast<const CharacterPart*>(agent->part);
    u8 kind = part->bits.attackKind;
    if (kind == AttackSlide || kind == AttackSlideVariant)
    {
        return false;
    }

    return part->moveBits.crawling == 0;
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
    lock->AddKind(NodeCrate, 3);
    lock->AddKind(NodeGenericObject, 2);
    lock->AddKind(NodeCreature, 1);
    lock = &gun->locks[LockVehicle];
    lock->offset = g_GunVehicleLockOffset;
    lock->SetShape(50.0f, 1.25f, 24.0f);
    lock->AddKind(NodeGenericObject, 2);
    lock->AddKind(NodeCreature, 1);
    lock = &gun->locks[LockMecha];
    lock->offset = g_GunMechaLockOffset;
    lock->SetShape(40.0f, 2.5f, 15.0f);
    lock->AddKind(NodeGenericObject, 2);
    lock->AddKind(NodeCreature, 1);
    lock = &gun->locks[LockArea24];
    lock->offset = g_GunArea24LockOffset;
    lock->SetFlatShape(50.0f, 10.0f, 10.0f);
    lock->AddKind(NodeGenericObject, 1);
    return gun;
}

void Gun::Reset()
{
    duration = 0;
    stateStart = 0;
    // Retail clears the word with memset first
    bits.value = 0;
    bits.state = StatePutAway;
    bits.next = StateNoNext;
    bits.secondCount = ResetSecondCount;
    bits.ammo = ResetAmmo;
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
    RunAgentEvent(agent, loaded != 0 ? EventGunShot : EventGunNoAmmo, 0, 0, 1);
    return StateShot;
}

void Gun::Frame(f32 square, f32 allowed, TimeClock* clock)
{
    bool squareHeld = 0.0f < square;
    bool mayAim = 0.0f < allowed;
    s32 next = NoNextState;
    TargetLock* lock = &locks[LockOf(this)];
    s32 now = clock->time;
    if (bits.state != StatePutAway && ObjectOf(gunInstance) == nullptr)
    {
        TakeGunInstance(this);
    }

    if (ObjectOf(gunInstance) != nullptr && bits.state != StatePutAway && mayAim)
    {
        lock->Search(clock, agent->instance, static_cast<InstanceContext*>(ObjectOf(gunInstance)));
    }
    else
    {
        lock->Drop();
    }

    if (bits.next != StateNoNext)
    {
        bits.state = bits.next;
        bits.next = StateNoNext;
        stateStart = now;
    }

    switch (bits.state)
    {
    case StatePutAway:
        if (PlayedInstance() == agent->instance)
        {
            RunAgentEvent(agent, EventGunDrawn, 0, 0, 0);
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
            RunAgentEvent(agent, EventGunPutAway, 0, 0, 0);
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
                RunAgentEvent(agent, EventGunCharged, 0, 0, 0);
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

    bits.squareHeld = squareHeld;
    if (next != NoNextState)
    {
        bits.next = next;
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
    auto* node = static_cast<ObjectNode*>(GetGameNode(&agent->instance->nodes, NodeObject));
    if (node->agentRef1 != nullptr && node->agentRef1->flags.asleep)
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
    u32 ammo = bits.ammo;
    if (ammo < count)
    {
        // Retail bug: without enough ammo for the shot, the ammo left is lost
        bits.ammo = 0;
        return 0;
    }

    bits.ammo = ammo - count;
    return 1;
}

u32 Gun::AddSecondCount(s32 amount)
{
    u32 count = bits.secondCount + amount;
    if (count < SecondCountLimit)
    {
        bits.secondCount = count;
        return 1;
    }

    bits.secondCount = SecondCountLimit - 1;
    return 0;
}

u32 Gun::AddAmmo(s32 amount)
{
    u32 ammo = bits.ammo + amount;
    if (ammo < AmmoLimit)
    {
        bits.ammo = ammo;
        return 1;
    }

    bits.ammo = AmmoLimit - 1;
    return 0;
}
