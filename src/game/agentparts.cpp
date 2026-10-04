#include "game/agentparts.h"

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
constexpr u8 BasicLowByte = 3;
constexpr u32 BasicBits = 0x1E000 | 0x20000 | 0x40000 | 0x80000;
constexpr u32 CreatureValue = 2;
// What the constructors pass their part's Reset
constexpr u32 Made = 1;

// The basic part's construction, which pickups' and pay gates' have inline
void StartBasicPart(BasicAgentPart* part)
{
    part->lastAttack = 0;
    part->unknown08 = 0;
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

void ResetBasicValues(BasicAgentPart* part, u32 unknown)
{
    part->bits = 0;
    *reinterpret_cast<u8*>(&part->bits) = BasicLowByte;
    part->bits |= BasicBits;
    part->AgentPart::Reset(unknown);
}
}

void AgentPart::Destroy(u32 destroyFlags)
{
    EndPart(this, destroyFlags);
}

void AgentPart::Reset(u32)
{
    unknown08 = 0;
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
    constexpr u32 FirstHit = 6;
    if (lastAttack < FirstHit)
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

void BasicAgentPart::Reset(u32 unknown)
{
    ResetBasicValues(this, unknown);
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
    case 3:
        return (bits & HitByKind3) != 0;
    case 4:
        return (bits & HitByKind4) != 0;
    case 5:
        return (bits & HitByKind5) != 0;
    case 6:
        return (bits & HitByKind6) != 0;
    case 7:
    case 9:
        return (bits & HitByKind7Or9) != 0;
    case 8:
        return (bits & HitByKind8) != 0;
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

void PickupPart::Reset(u32 unknown)
{
    ResetBasicValues(this, unknown);
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

void CratePart::Reset(u32 unknown)
{
    BasicAgentPart::Reset(unknown);
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

void CreaturePart::Reset(u32 unknown)
{
    BasicAgentPart::Reset(unknown);
    unknown18 = 0;
    flags = CreatureValue;
}

CharacterPart* CharacterPart::Construct(CharacterPart* part)
{
    CreaturePart::Construct(part);
    part->moveBits = 0;
    part->vtable = g_CharacterPartVTable;
    part->Reset(Made);
    return part;
}

void CharacterPart::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterPartVTable;
    CreaturePart::Destroy(destroyFlags);
}

void CharacterPart::Reset(u32 unknown)
{
    CreaturePart::Reset(unknown);
    moveBits = 0;
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
    if ((moveBits & TurnRequested) != 0)
    {
        return 0;
    }

    moveBits |= TurnRequested;
    wantedTurn = *angle;
    return 1;
}

u32 CharacterPart::RequestForward(f32 speed)
{
    if ((moveBits & ForwardRequested) != 0)
    {
        return 0;
    }

    wantedForward = speed;
    moveBits |= ForwardRequested;
    return 1;
}

u32 CharacterPart::RequestSideways(f32 speed)
{
    if ((moveBits & SidewaysRequested) != 0)
    {
        return 0;
    }

    wantedSideways = speed;
    moveBits |= SidewaysRequested;
    return 1;
}

u32 CharacterPart::RequestVertical(f32 speed)
{
    if ((moveBits & VerticalRequested) != 0)
    {
        return 0;
    }

    wantedVertical = speed;
    moveBits |= VerticalRequested;
    return 1;
}

u32 CharacterPart::RequestScale(f32 scale)
{
    if ((moveBits & ScaleRequested) != 0)
    {
        return 0;
    }

    speedScale = scale;
    moveBits |= ScaleRequested;
    return 1;
}

void CharacterPart::ClearSpeedRequests()
{
    moveBits &= ~(ForwardRequested | SidewaysRequested | VerticalRequested | ScaleRequested);
}

void CharacterPart::ClearTurnRequest()
{
    moveBits &= ~TurnRequested;
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

void GenericObjectPart::Reset(u32 unknown)
{
    BasicAgentPart::Reset(unknown);
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

void PayGatePart::Reset(u32 unknown)
{
    ResetBasicValues(this, unknown);
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

void GraplePart::Reset(u32 unknown)
{
    BasicAgentPart::Reset(unknown);
}

void ProjectilePart::Destroy(u32 destroyFlags)
{
    vtable = g_ProjectilePartVTable;
    BasicAgentPart::Destroy(destroyFlags);
}

void ProjectilePart::Reset(u32)
{
}
