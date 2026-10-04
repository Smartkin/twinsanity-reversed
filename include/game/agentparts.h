#pragma once

#include "common.h"
#include "game/math.h"
#include "gcc2.h"

// The part of its type an agent keeps, made by the factory with the agent's holder (retail's InstanceCreationHelper classes,
// vtable 0xC bytes in: 1 the destructor, 2 its values made again): the kind of the last attack that reached it and the time of its
// clock then, and a word made 0. The basic part's bits extend it in every type's part (pickups' and pay gates' copy the basic
// part's code instead of calling it; the projectiles' part is the basic one given its own vtable by the factory, which makes
// nothing again)
class AgentPart
{
public:
    u8 lastAttack;
    u32 lastAttackTime;
    u32 unknown08;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00263108);
    void Reset(u32 unknown) RETAIL(FUN_002631d8);
    void RecordAttack(u32 kind, const u32* time) RETAIL(FUN_00263138);
    // Whether the last attack reached it within a window (seconds) before the time: any, of a kind, of the kinds from 6 (3 to 5
    // are walking into it, jumping on it and headbutting it)
    u32 AttackedWithin(const u32* time, f32 seconds) RETAIL_N32(EventWithinSeconds);
    u32 AttackedWithin(u32 kind, const u32* time, f32 seconds) RETAIL_N32(FUN_00263148);
    u32 HitWithin(const u32* time, f32 seconds) RETAIL_N32(FUN_001416b8);
};
CHECK_SIZE(AgentPart, 0x10);

// The basic part's bits: its low byte (3 when made: the playable character's 3 makes a creature that may damage it hit back
// when attacked by kind 3), what its agent's state gives (bits 8-12), and the attacks that reach it (bits 13-19, all when made)
class BasicAgentPart : public AgentPart
{
public:
    enum Bits : u32
    {
        LowByteMask = 0xFF,
        CanDamageCharacter = 0x100,
        Targettable = 0x200,
        CanAlwaysDamageCharacter = 0x400,
        BulletsBounceBack = 0x1000,
        // Attacks of kind 3, 4, 5, 6, 8 and 7 or 9 (10 to 14 always reach it)
        HitByKind3 = 0x2000,
        HitByKind5 = 0x8000,
        HitByKind6 = 0x10000,
        HitByKind8 = 0x20000,
        HitByKind7Or9 = 0x40000,
        HitByKind4 = 0x80000,
    };

    u32 bits;

    static BasicAgentPart* Construct(BasicAgentPart* part) RETAIL(FUN_00141648);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141688);
    void Reset(u32 unknown) RETAIL(FUN_001417a8);
    // Whether an attack of the kind reaches it (kinds below 3 or past 14 do)
    u32 HitBy(u32 kind) RETAIL(FUN_00141710);
};
CHECK_SIZE(BasicAgentPart, 0x14);

// The parts of 0x18 bytes: the basic part's values and one more (graples' part never sets it)
class AgentPartWithValue : public BasicAgentPart
{
public:
    u32 value;
};
CHECK_SIZE(AgentPartWithValue, 0x18);

class PickupPart : public AgentPartWithValue
{
public:
    static PickupPart* Construct(PickupPart* part) RETAIL(FUN_00141ab0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141b08);
    void Reset(u32 unknown) RETAIL(FUN_00141b38);
};

class CratePart : public AgentPartWithValue
{
public:
    static CratePart* Construct(CratePart* part) RETAIL(FUN_001407a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001407e0);
    void Reset(u32 unknown) RETAIL(FUN_00140808);
};

// Its flags (2 when made): 0 it snaps to the ground (its state's bit 18), 1 it rests on something (a crate's: landed), 2 on the
// ground (the creature's function 23 then lands it: 25), 4 moving, 5 falling (its functions 24 and 25 set and clear it), 6-13 its
// hit points; and a value (the playable character's gravity)
class CreaturePart : public BasicAgentPart
{
public:
    enum Flags : u32
    {
        FlagSnapsToGround = 0x1,
        FlagResting = 0x2,
        FlagOnGround = 0x4,
        FlagMoving = 0x10,
        FlagFalling = 0x20,
        HitPointsShift = 6,
        HitPointsMask = 0xFF,
    };

    u32 flags;
    u32 unknown18;

    static CreaturePart* Construct(CreaturePart* part) RETAIL(FUN_00140d88);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140dc8);
    void Reset(u32 unknown) RETAIL(FUN_00140df0);
};
CHECK_SIZE(CreaturePart, 0x1C);

// The playable characters' (its two vectors made the default box's lowest corner with w 1). Its low byte is the attack kind its
// moves give (3 walking into something; 4 landing on it, 5 hitting it from below; 6 and 10 spinning, 7 and 11 body slamming, 8 and
// 12 sliding, 9 tied to the other character, 13 and 14 thrown, 15 and 16 landing on it or hitting it from below while spinning),
// bit 10 of its bits it's invincible and bit 20 frozen. The value at 0x18 is its gravity's strength, and the word after it bits
// mirroring its controllers' states, which the conditions read as bits 32-63 of the 64 bits from 0x18 (Bits()): 32 crouching, 33-35
// and 39-40 the jump's, 36 Nina's claw, 37 the gun, 38 the spin, 41 the crouch's state 3, 42 walking, 43 running, 44 the walk's bit
// 8, 45 a fall started and 46 it fell far (10 or more, from 45), 47 hurt, 48-52 the move's requests this frame (a turn, forward,
// sideways and vertical speeds and a speed scale: the first request of each wins), 53 the leader and 54 the second of two
// characters tied together, 56 no fall events, 57 the scripts' flag. The requests' values, the share of the last move it made, the
// push its probes give and its average
class CharacterPart : public CreaturePart
{
public:
    enum Bits : u32
    {
        AttackKindMask = 0xFF,
        Invincible = 0x400,
        Frozen = 0x100000,
    };

    enum MoveBits : u32
    {
        Crouching = 0x1,
        Jumping = 0x2,
        Clawing = 0x10,
        Shooting = 0x20,
        Spinning = 0x40,
        Walking = 0x400,
        Running = 0x800,
        Falling = 0x2000,
        FellFar = 0x4000,
        Hurt = 0x8000,
        TurnRequested = 0x10000,
        ForwardRequested = 0x20000,
        SidewaysRequested = 0x40000,
        VerticalRequested = 0x80000,
        ScaleRequested = 0x100000,
        LinkedFirst = 0x200000,
        LinkedSecond = 0x400000,
        NoFallEvents = 0x1000000,
        ScriptFlag = 0x2000000,
    };

    u32 moveBits;
    s32 wantedTurn;
    f32 wantedForward;
    f32 wantedSideways;
    f32 wantedVertical;
    f32 speedScale;
    f32 moveShare;
    u8 unknown38[8];
    Vector4 push;
    Vector4 smoothedPush;

    // The 64 bits from 0x18 (its gravity's, then its move bits)
    u64& Bits()
    {
        return *reinterpret_cast<u64*>(&unknown18);
    }

    const u64& Bits() const
    {
        return *reinterpret_cast<const u64*>(&unknown18);
    }

    f32& Gravity()
    {
        return *reinterpret_cast<f32*>(&unknown18);
    }

    static CharacterPart* Construct(CharacterPart* part) RETAIL(FUN_00140070);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001400b8);
    void Reset(u32 unknown) RETAIL(FUN_001401a0);

    // The move's requests: a turn (an angle, 65536ths of a turn), a forward, sideways and vertical speed and a speed scale (the
    // floats first), each taken unless one was this frame (whether it was), and cleared
    u32 RequestTurn(const s32* angle) RETAIL(FUN_00140238);
    u32 RequestForward(f32 speed) RETAIL_N32(FUN_00140278);
    u32 RequestSideways(f32 speed) RETAIL_N32(FUN_001402a8);
    u32 RequestVertical(f32 speed) RETAIL_N32(FUN_001402d8);
    u32 RequestScale(f32 scale) RETAIL_N32(FUN_00140308);
    void ClearSpeedRequests() RETAIL(FUN_001400e0);
    void ClearTurnRequest() RETAIL(FUN_00140170);
    // The push averaged with this frame's (none below 0.1): the average
    Vector4* SmoothPush() RETAIL(FUN_0013c9d0);
};
CHECK_OFFSET(CharacterPart, moveBits, 0x1C);
CHECK_OFFSET(CharacterPart, wantedTurn, 0x20);
CHECK_OFFSET(CharacterPart, speedScale, 0x30);
CHECK_OFFSET(CharacterPart, moveShare, 0x34);
CHECK_OFFSET(CharacterPart, push, 0x40);
CHECK_OFFSET(CharacterPart, smoothedPush, 0x50);
CHECK_SIZE(CharacterPart, 0x60);

class GenericObjectPart : public AgentPartWithValue
{
public:
    static GenericObjectPart* Construct(GenericObjectPart* part) RETAIL(FUN_00141180);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001411c0);
    void Reset(u32 unknown) RETAIL(FUN_001411e8);
};

// Its values made again without the basic part's
class GrabbablePart : public AgentPartWithValue
{
public:
    static GrabbablePart* Construct(GrabbablePart* part) RETAIL(FUN_00140418);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140458);
    void Reset(u32 unknown) RETAIL(FUN_00140480);
};

// Its value's low 12 bits are the gate's number (its third integer property, given by its node)
class PayGatePart : public AgentPartWithValue
{
public:
    static PayGatePart* Construct(PayGatePart* part) RETAIL(FUN_001418b0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141908);
    void Reset(u32 unknown) RETAIL(FUN_00141938);
};

class GraplePart : public AgentPartWithValue
{
public:
    static GraplePart* Construct(GraplePart* part) RETAIL(FUN_00140fd8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141018);
    void Reset(u32 unknown) RETAIL(FUN_00141040);
};

class ProjectilePart : public BasicAgentPart
{
public:
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f060);
    void Reset(u32 unknown) RETAIL(FUN_0013f088);
};

extern "C"
{
    extern const GccVTableEntry g_AgentPartVTable[] RETAIL(InstanceCreationHelperBase_Methods);
    extern const GccVTableEntry g_BasicAgentPartVTable[] RETAIL(InstanceCreationHelperType8_Methods);
    extern const GccVTableEntry g_PickupPartVTable[] RETAIL(InstanceCreationHelperType1_Methods);
    extern const GccVTableEntry g_CratePartVTable[] RETAIL(InstanceCreationHelperType2_Methods);
    extern const GccVTableEntry g_CreaturePartVTable[] RETAIL(InstanceCreationHelperType3_Methods);
    extern const GccVTableEntry g_GenericObjectPartVTable[] RETAIL(InstanceCreationHelperType4_Methods);
    extern const GccVTableEntry g_GrabbablePartVTable[] RETAIL(InstanceCreationHelperType5_Methods);
    extern const GccVTableEntry g_PayGatePartVTable[] RETAIL(InstanceCreationHelperType6_Methods);
    extern const GccVTableEntry g_GraplePartVTable[] RETAIL(InstanceCreationHelperType7_Methods);
    extern const GccVTableEntry g_CharacterPartVTable[] RETAIL(InstanceCreationHelperPlayableCharacter_Methods);
    extern const GccVTableEntry g_ProjectilePartVTable[] RETAIL(D_002F21F0);
}
