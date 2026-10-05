#pragma once

#include "common.h"
#include "game/math.h"
#include "gcc2.h"

// The part of its type an agent keeps, made by the factory with the agent's holder (retail's InstanceCreationHelper classes,
// vtable 0xC bytes in: 1 the destructor, 2 its values made again, given the game's reset entry (0 when its agent takes its
// resources)): the kind of the last attack that reached it and the time of its clock then, and how much its instance makes itself
// felt to the perceptions around it (the scripts' "character analog": SetPresence, AddPresence and the Presence condition,
// any agent's), 0 when made. The basic part's bits extend it in every type's part (pickups' and pay
// gates' copy the basic part's code instead of calling it; the projectiles' part is the basic one given its own vtable by the
// factory, which makes nothing again)
class AgentPart
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ResetSlot = 2,
    };

    u8 lastAttack;
    u32 lastAttackTime;
    f32 presence;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00263108);
    void Reset(u32 resetEntry) RETAIL(FUN_002631d8);
    void RecordAttack(u32 kind, const u32* time) RETAIL(FUN_00263138);
    // Whether the last attack reached it within a window (seconds) before the time: any, of a kind, of the kinds from 6 (3 to 5
    // are walking into it, landing on it and hitting it from below)
    u32 AttackedWithin(const u32* time, f32 seconds) RETAIL_N32(EventWithinSeconds);
    u32 AttackedWithin(u32 kind, const u32* time, f32 seconds) RETAIL_N32(FUN_00263148);
    u32 HitWithin(const u32* time, f32 seconds) RETAIL_N32(FUN_001416b8);
};
CHECK_OFFSET(AgentPart, presence, 0x8);
CHECK_SIZE(AgentPart, 0x10);

// The basic part's bits: the attack kind the playable characters' moves give (characters.h's CharacterAttack; 3 when made: a
// creature that may damage the character hits back when the character walks into it), what its agent's state gives it (it may
// damage the playable characters, it's a target, it's invulnerable, bullets bounce back off it), the attacks that reach it (all
// when made; kinds below 3 or past 9 always do) and the playable character's freezing. An invulnerable agent's script isn't told
// of attacks, a creature's of contact messages, and one that may damage the character hits back at any attack (the tool's
// "can always damage character"); the playable characters' own is their invincibility
union AgentPartBits
{
    u32 value;
    struct
    {
        u32 attackKind : 8;
        u32 canDamageCharacter : 1;
        u32 targettable : 1;
        u32 invulnerable : 1;
        u32 unused11 : 1;
        u32 bulletsBounceBack : 1;
        u32 hitByWalkInto : 1;
        // Set with the walk into's and from below's (SetAttacksTaken), never read
        u32 unused14 : 1;
        u32 hitFromBelow : 1;
        u32 hitBySpin : 1;
        u32 hitBySlide : 1;
        u32 hitBySlamOrTied : 1;
        u32 hitByLandOn : 1;
        u32 frozen : 1;
        u32 unused21 : 11;
    };
};
CHECK_SIZE(AgentPartBits, 4);

class BasicAgentPart : public AgentPart
{
public:
    AgentPartBits bits;

    static BasicAgentPart* Construct(BasicAgentPart* part) RETAIL(FUN_00141648);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141688);
    void Reset(u32 resetEntry) RETAIL(FUN_001417a8);
    // Whether an attack of the kind reaches it (kinds below 3 or past 9 do)
    u32 HitBy(u32 kind) RETAIL(FUN_00141710);
};
CHECK_SIZE(BasicAgentPart, 0x14);

// A crate's part's word: in the air (launched and not landed), resting on the ground, and the wumpa fruit in it (the scripts set
// them, SetCrate, picking them up takes them, the CrateHasWumpa condition tests for any)
union CratePartState
{
    u32 value;
    struct
    {
        u32 inAir : 1;
        u32 resting : 1;
        u32 wumpaFruit : 8;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(CratePartState, 4);

// A pay gate's part's word: the gate's number (its third integer property, given by its node)
union PayGatePartNumber
{
    u32 value;
    struct
    {
        u32 number : 12;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(PayGatePartNumber, 4);

// A grabbable's part's word: what SetChiChiGrass sets, which nothing reads
union GrabbablePartFlags
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(GrabbablePartFlags, 4);

// The parts of 0x18 bytes: the basic part's values and a word of the type's (graples' part never sets it; pickups' and generic
// objects' only clear it)
class AgentPartWithValue : public BasicAgentPart
{
public:
    union
    {
        u32 value;
        CratePartState crate;
        PayGatePartNumber payGate;
        GrabbablePartFlags grabbable;
    };
};
CHECK_SIZE(AgentPartWithValue, 0x18);

class PickupPart : public AgentPartWithValue
{
public:
    static PickupPart* Construct(PickupPart* part) RETAIL(FUN_00141ab0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141b08);
    void Reset(u32 resetEntry) RETAIL(FUN_00141b38);
};

class CratePart : public AgentPartWithValue
{
public:
    static CratePart* Construct(CratePart* part) RETAIL(FUN_001407a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001407e0);
    void Reset(u32 resetEntry) RETAIL(FUN_00140808);
};

// A creature's part's flags (resting when made): it snaps to the ground (its state's SnapToGround), it rests on something, it's
// on the ground (the creature's function 23 then lands it: 25), moving, falling (its functions 24 and 25 set and clear it), and
// its hit points
union CreaturePartFlags
{
    u32 value;
    struct
    {
        u32 snapsToGround : 1;
        u32 resting : 1;
        u32 onGround : 1;
        u32 unused3 : 1;
        u32 moving : 1;
        u32 falling : 1;
        u32 hitPoints : 8;
        u32 unused14 : 18;
    };
};
CHECK_SIZE(CreaturePartFlags, 4);

// Its flags and the strength of its gravity (the playable characters'; 0 when made)
class CreaturePart : public BasicAgentPart
{
public:
    CreaturePartFlags flags;
    f32 gravity;

    static CreaturePart* Construct(CreaturePart* part) RETAIL(FUN_00140d88);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140dc8);
    void Reset(u32 resetEntry) RETAIL(FUN_00140df0);
};
CHECK_OFFSET(CreaturePart, gravity, 0x18);
CHECK_SIZE(CreaturePart, 0x1C);

// A playable character's part's move bits, mirroring its controllers' states (the conditions read them): crouching, the jump's
// (jumping, the double jump and the knee drop, the slide jump, the jump of the unused kind 8, the flying kick), Nina's claw, the
// gun, the spin, crawling (the crouch's state 3), walking, running, strafing (the walk's strafe held), a fall started and it fell
// far (10 or more from where it started), hurt, the move's requests this frame (a turn, forward, sideways and vertical speeds and
// a speed scale: the first request of each wins), the leader and the second of two characters tied together, no ground ahead
// (the solver's), no fall events, and the scripts' flag (SetPlayerScriptFlag sets or clears it, the PlayerScriptFlagClear
// condition tests it)
union CharacterMoveBits
{
    u32 value;
    struct
    {
        u32 crouching : 1;
        u32 jumping : 1;
        u32 doubleJump : 1;
        u32 slideJump : 1;
        u32 clawing : 1;
        u32 shooting : 1;
        u32 spinning : 1;
        u32 unusedJump : 1;
        u32 flyingKick : 1;
        u32 crawling : 1;
        u32 walking : 1;
        u32 running : 1;
        u32 strafing : 1;
        u32 falling : 1;
        u32 fellFar : 1;
        u32 hurt : 1;
        u32 turnRequested : 1;
        u32 forwardRequested : 1;
        u32 sidewaysRequested : 1;
        u32 verticalRequested : 1;
        u32 scaleRequested : 1;
        u32 linkedFirst : 1;
        u32 linkedSecond : 1;
        u32 noGroundAhead : 1;
        u32 noFallEvents : 1;
        u32 scriptFlag : 1;
        u32 unused26 : 6;
    };
};
CHECK_SIZE(CharacterMoveBits, 4);

// The playable characters' (its two vectors made the default box's lowest corner with w 1): its move bits (retail writes them
// with the gravity before them as 64 bits), the requests' values, the share of the last move it made, the push its probes give
// and its average
class CharacterPart : public CreaturePart
{
public:
    CharacterMoveBits moveBits;
    s32 wantedTurn;
    f32 wantedForward;
    f32 wantedSideways;
    f32 wantedVertical;
    f32 speedScale;
    f32 moveShare;
    u8 unused38[8];
    Vector4 push;
    Vector4 smoothedPush;

    static CharacterPart* Construct(CharacterPart* part) RETAIL(FUN_00140070);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001400b8);
    void Reset(u32 resetEntry) RETAIL(FUN_001401a0);

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
    void Reset(u32 resetEntry) RETAIL(FUN_001411e8);
};

// Its values made again without the basic part's
class GrabbablePart : public AgentPartWithValue
{
public:
    static GrabbablePart* Construct(GrabbablePart* part) RETAIL(FUN_00140418);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140458);
    void Reset(u32 resetEntry) RETAIL(FUN_00140480);
};

class PayGatePart : public AgentPartWithValue
{
public:
    static PayGatePart* Construct(PayGatePart* part) RETAIL(FUN_001418b0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141908);
    void Reset(u32 resetEntry) RETAIL(FUN_00141938);
};

class GraplePart : public AgentPartWithValue
{
public:
    static GraplePart* Construct(GraplePart* part) RETAIL(FUN_00140fd8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141018);
    void Reset(u32 resetEntry) RETAIL(FUN_00141040);
};

class ProjectilePart : public BasicAgentPart
{
public:
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f060);
    void Reset(u32 resetEntry) RETAIL(FUN_0013f088);
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
