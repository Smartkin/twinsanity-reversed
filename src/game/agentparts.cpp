#include "game/agentparts.h"

#include "game/characters.h"
#include "game/clock.h"
#include "game/memory.h"

EABI_EXPORT(EventWithinSeconds, static_cast<u32 (AgentPart::*)(const u32*, f32)>(&AgentPart::AttackedWithin));
EABI_EXPORT(FUN_00263148, static_cast<u32 (AgentPart::*)(u32, const u32*, f32)>(&AgentPart::AttackedWithin));
EABI_EXPORT(FUN_001416b8, &AgentPart::HitWithin);
EABI_EXPORT(FUN_00140278, &CharacterPart::RequestForward);
EABI_EXPORT(FUN_001402a8, &CharacterPart::RequestSideways);
EABI_EXPORT(FUN_001402d8, &CharacterPart::RequestVertical);
EABI_EXPORT(FUN_00140308, &CharacterPart::RequestScale);

namespace
{
// What the constructors pass their part's Reset
constexpr u32 Made = 1;

// The basic part's construction, which pickups' and pay gates' have inline
void StartBasicPart(BasicAgentPart* part)
{
    part->lastAttack = 0;
    part->presence = 0.0f;
    part->vtable = g_BasicAgentPartVTable;
    part->Reset(Made);
}

// The base's destruction, which every part with the basic part's code inline has
void EndPart(AgentPart* part, u32 destroyFlags)
{
    part->vtable = g_AgentPartVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(part);
    }
}

// Every attack reaches it
void ResetBasicValues(BasicAgentPart* part, u32 resetEntry)
{
    AgentPartBits made = {};
    made.attackKind = AttackWalkInto;
    made.hitByWalkInto = 1;
    made.unused14 = 1;
    made.hitFromBelow = 1;
    made.hitBySpin = 1;
    made.hitBySlide = 1;
    made.hitBySlamOrTied = 1;
    made.hitByLandOn = 1;
    part->bits = made;
    part->AgentPart::Reset(resetEntry);
}
}

void AgentPart::Destroy(u32 destroyFlags)
{
    EndPart(this, destroyFlags);
}

void AgentPart::Reset(u32)
{
    presence = 0.0f;
    lastAttack = 0;
    lastAttackTime = 0;
}

void AgentPart::RecordAttack(u32 kind, const u32* time)
{
    lastAttack = static_cast<u8>(kind);
    lastAttackTime = *time;
}

u32 AgentPart::AttackedWithin(const u32* time, f32 seconds)
{
    return static_cast<f32>(static_cast<s32>(*time - lastAttackTime)) * g_SecondsPerClockUnit <= seconds;
}

u32 AgentPart::AttackedWithin(u32 kind, const u32* time, f32 seconds)
{
    if (lastAttack != kind)
    {
        return 0;
    }

    return AttackedWithin(time, seconds);
}

u32 AgentPart::HitWithin(const u32* time, f32 seconds)
{
    if (lastAttack < AttackSpin)
    {
        return 0;
    }

    return AttackedWithin(time, seconds);
}

BasicAgentPart* BasicAgentPart::Construct(BasicAgentPart* part)
{
    StartBasicPart(part);
    return part;
}

void BasicAgentPart::Destroy(u32 destroyFlags)
{
    EndPart(this, destroyFlags);
}

void BasicAgentPart::Reset(u32 resetEntry)
{
    ResetBasicValues(this, resetEntry);
}

// HitBy's cases splat split off, which the retail jump table (jtbl_002F2E50) points at: kinds 10 to 12 (and the default) and
// 13 and 14 always reach the part
extern "C"
{
    u32 HitByKinds10To12() RETAIL(FUN_001417a0);
    u32 HitByKinds13And14() RETAIL(FUN_00141798);
}

u32 HitByKinds10To12()
{
    return 1;
}

u32 HitByKinds13And14()
{
    return 1;
}

u32 BasicAgentPart::HitBy(u32 kind)
{
    switch (kind)
    {
    case AttackWalkInto:
        return bits.hitByWalkInto != 0;
    case AttackLandOn:
        return bits.hitByLandOn != 0;
    case AttackFromBelow:
        return bits.hitFromBelow != 0;
    case AttackSpin:
        return bits.hitBySpin != 0;
    case AttackSlam:
    case AttackTied:
        return bits.hitBySlamOrTied != 0;
    case AttackSlide:
        return bits.hitBySlide != 0;
    default:
        return 1;
    }
}

PickupPart* PickupPart::Construct(PickupPart* part)
{
    StartBasicPart(part);
    part->vtable = g_PickupPartVTable;
    part->Reset(Made);
    return part;
}

void PickupPart::Destroy(u32 destroyFlags)
{
    EndPart(this, destroyFlags);
}

void PickupPart::Reset(u32 resetEntry)
{
    ResetBasicValues(this, resetEntry);
    value = 0;
}

CratePart* CratePart::Construct(CratePart* part)
{
    BasicAgentPart::Construct(part);
    part->vtable = g_CratePartVTable;
    part->Reset(Made);
    return part;
}

void CratePart::Destroy(u32 destroyFlags)
{
    vtable = g_CratePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void CratePart::Reset(u32 resetEntry)
{
    BasicAgentPart::Reset(resetEntry);
    value = 0;
}

CreaturePart* CreaturePart::Construct(CreaturePart* part)
{
    BasicAgentPart::Construct(part);
    part->vtable = g_CreaturePartVTable;
    part->Reset(Made);
    return part;
}

void CreaturePart::Destroy(u32 destroyFlags)
{
    vtable = g_CreaturePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void CreaturePart::Reset(u32 resetEntry)
{
    BasicAgentPart::Reset(resetEntry);
    gravity = 0.0f;
    CreaturePartFlags made = {};
    made.resting = 1;
    flags = made;
}

CharacterPart* CharacterPart::Construct(CharacterPart* part)
{
    CreaturePart::Construct(part);
    part->moveBits.value = 0;
    part->vtable = g_CharacterPartVTable;
    part->Reset(Made);
    return part;
}

void CharacterPart::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterPartVTable;
    CreaturePart::Destroy(destroyFlags);
}

void CharacterPart::Reset(u32 resetEntry)
{
    CreaturePart::Reset(resetEntry);
    moveBits.value = 0;
    wantedTurn = 0;
    push = g_DefaultBox.min;
    push.w = 1.0f;
    smoothedPush = g_DefaultBox.min;
    smoothedPush.w = 1.0f;
    moveShare = 0.0f;
    wantedForward = 0.0f;
    wantedSideways = 0.0f;
    wantedVertical = 0.0f;
    speedScale = 0.0f;
}

u32 CharacterPart::RequestTurn(const s32* angle)
{
    if (moveBits.turnRequested != 0)
    {
        return 0;
    }

    moveBits.turnRequested = 1;
    wantedTurn = *angle;
    return 1;
}

u32 CharacterPart::RequestForward(f32 speed)
{
    if (moveBits.forwardRequested != 0)
    {
        return 0;
    }

    wantedForward = speed;
    moveBits.forwardRequested = 1;
    return 1;
}

u32 CharacterPart::RequestSideways(f32 speed)
{
    if (moveBits.sidewaysRequested != 0)
    {
        return 0;
    }

    wantedSideways = speed;
    moveBits.sidewaysRequested = 1;
    return 1;
}

u32 CharacterPart::RequestVertical(f32 speed)
{
    if (moveBits.verticalRequested != 0)
    {
        return 0;
    }

    wantedVertical = speed;
    moveBits.verticalRequested = 1;
    return 1;
}

u32 CharacterPart::RequestScale(f32 scale)
{
    if (moveBits.scaleRequested != 0)
    {
        return 0;
    }

    speedScale = scale;
    moveBits.scaleRequested = 1;
    return 1;
}

void CharacterPart::ClearSpeedRequests()
{
    moveBits.forwardRequested = 0;
    moveBits.sidewaysRequested = 0;
    moveBits.verticalRequested = 0;
    moveBits.scaleRequested = 0;
}

void CharacterPart::ClearTurnRequest()
{
    moveBits.turnRequested = 0;
}

GenericObjectPart* GenericObjectPart::Construct(GenericObjectPart* part)
{
    BasicAgentPart::Construct(part);
    part->vtable = g_GenericObjectPartVTable;
    part->Reset(Made);
    return part;
}

void GenericObjectPart::Destroy(u32 destroyFlags)
{
    vtable = g_GenericObjectPartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void GenericObjectPart::Reset(u32 resetEntry)
{
    BasicAgentPart::Reset(resetEntry);
    value = 0;
}

GrabbablePart* GrabbablePart::Construct(GrabbablePart* part)
{
    BasicAgentPart::Construct(part);
    part->vtable = g_GrabbablePartVTable;
    part->Reset(Made);
    return part;
}

void GrabbablePart::Destroy(u32 destroyFlags)
{
    vtable = g_GrabbablePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void GrabbablePart::Reset(u32)
{
    value = 0;
}

PayGatePart* PayGatePart::Construct(PayGatePart* part)
{
    StartBasicPart(part);
    part->vtable = g_PayGatePartVTable;
    part->Reset(Made);
    return part;
}

void PayGatePart::Destroy(u32 destroyFlags)
{
    EndPart(this, destroyFlags);
}

void PayGatePart::Reset(u32 resetEntry)
{
    ResetBasicValues(this, resetEntry);
    value = 0;
}

GraplePart* GraplePart::Construct(GraplePart* part)
{
    BasicAgentPart::Construct(part);
    part->vtable = g_GraplePartVTable;
    part->Reset(Made);
    return part;
}

void GraplePart::Destroy(u32 destroyFlags)
{
    vtable = g_GraplePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void GraplePart::Reset(u32 resetEntry)
{
    BasicAgentPart::Reset(resetEntry);
}

void ProjectilePart::Destroy(u32 destroyFlags)
{
    vtable = g_ProjectilePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void ProjectilePart::Reset(u32)
{
}
