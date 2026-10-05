#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/animation.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/reference.h"
#include "game/springbody.h"

class CharacterAgent;
class PropertyHolder;
struct ChunkData;
struct ChunkLinkData;
struct InstanceContext;
struct TimeClock;

// The parts of a playable character's control its agent keeps (agents.h's CharacterAgent, 0x98-0xC0): the crouch, Nina's claw,
// the jump, the gun, the spin, the walk, the link to the character it's tied to, its procedural joints, its vehicle
// (game/vehicles.h), its look and its body's sizes. One translation unit in retail (0x141D08-0x162358, its start-up
// FUN_0015da18), the character agent's own code before it

// The playable characters' behaviour slots their code runs (ExecuteEvent's index; the names are the slots of Crash's and Cortex's
// objects): among them a long drop with no fall started, a fall that went far, the landings (still, moving, after a far fall
// (also when a chunk link turns it back) and after a body slam), tied and untied, dead, thrown by the other character from a
// spin and from a jump (both run it), the gun drawn, put away and shot (with ammo), a vehicle taken and left, the knock-backs'
// by the angle from its facing to the push (within 45 degrees, past 135, below -45 and above 45) and the gun charged and shot
// without ammo
enum CharacterEvent : u32
{
    EventLongDrop = 11,
    EventIdle = 12,
    EventShuffleFeet = 13,
    EventWalk = 14,
    EventRun = 15,
    EventStrafeLeft = 16,
    EventStrafeRight = 17,
    EventSpin = 18,
    EventSpinRecovery = 19,
    EventKneeSlideJump = 22,
    EventStandingJump = 23,
    EventRunningJump = 24,
    EventDoubleJump = 25,
    EventMaxedDoubleJump = 26,
    EventKneeDropHang = 27,
    EventKneeDrop = 28,
    EventFlyingKick = 29,
    EventRadialBlastHang = 31,
    EventRadialBlast = 32,
    EventShortFall = 33,
    EventFellFar = 34,
    EventFlyingKickFall = 37,
    EventLand = 39,
    EventLandMoving = 40,
    EventLandFromFar = 41,
    EventKneeDropLand = 42,
    EventStandToCrouch = 46,
    EventCrouchToCrawl = 47,
    EventCrawlToCrouch = 48,
    EventCrouchToStand = 49,
    EventCrawlToStand = 50,
    EventRunToKneeSlide = 51,
    EventKneeSlideToCrouch = 53,
    EventKneeSlideToStand = 54,
    EventLinked = 65,
    EventUnlinked = 66,
    EventDied = 67,
    EventBodySlam = 68,
    EventThrownFromSpin = 69,
    EventThrownFromJump = 70,
    EventGunDrawn = 71,
    EventGunPutAway = 72,
    EventGunShot = 73,
    EventVehicleTaken = 74,
    EventVehicleLeft = 75,
    EventKnockedForward = 80,
    EventKnockedBack = 81,
    EventKnockedNegative = 82,
    EventKnockedPositive = 83,
    EventLinkedStandingJump = 100,
    EventLinkedRunningJump = 101,
    EventGunCharged = 106,
    EventGunNoAmmo = 107,
    EventFailedRadialBlast = 108,
    // Past every character's slots: no event
    EventNone = 0x6F,
};

// The playable characters (their agent's first int property): Crash, Cortex, a Crash 2 units high without probes, Nina, none and
// the Mecha-Bandicoot
constexpr u32 CharacterKindProperty = 0;
enum PlayableCharacter : s32
{
    CharacterCrash = 0,
    CharacterCortex = 1,
    CharacterTallCrash = 2,
    CharacterNina = 3,
    CharacterNone = 4,
    CharacterMecha = 5,
};

// The next state a controller's frame works out when it stays in its state
constexpr s32 NoNextState = -1;

// The attack kinds a playable character's moves give its part (the part's low byte; agentparts.h's CharacterPart): walking into
// something, landing on it and hitting it from below, spinning, body slamming, sliding, tied to the other character (the second,
// and the leader's slam), the variant kinds of the spin, the slam and the slide (nothing gives them: the controllers' bits that
// would are never set), thrown by the other character from a spin and from a jump, and landing on or hitting from below while
// spinning (both a spin and the kind without it)
enum CharacterAttack : u32
{
    AttackWalkInto = 3,
    AttackLandOn = 4,
    AttackFromBelow = 5,
    AttackSpin = 6,
    AttackSlam = 7,
    AttackSlide = 8,
    AttackTied = 9,
    AttackSpinVariant = 10,
    AttackSlamVariant = 11,
    AttackSlideVariant = 12,
    AttackThrownFromSpin = 13,
    AttackThrownFromJump = 14,
    AttackLandOnSpinning = 15,
    AttackFromBelowSpinning = 16,
};

// The part's move bits (bits 32-63 of its 64 bits from 0x18) the controllers' frames give that agentparts.h doesn't name: the
// double jump (and the knee drop), the slide jump, the jump of the unused kind 8, the flying kick, the crawl and the strafe held
enum CharacterMoveBit : u32
{
    MoveDoubleJump = 0x4,
    MoveSlideJump = 0x8,
    MoveUnusedJump = 0x80,
    MoveFlyingKick = 0x100,
    MoveCrawling = 0x200,
    MoveStrafing = 0x1000,
};

// CharacterAgent::FitsAt's kinds: standing (the instances in the way told when there's a normal), crouching, crawling, taking off
// from a slide and the knee drop (the crouch's hull for crouching, crawling and the knee drop)
enum CharacterFit : u32
{
    FitStanding = 0,
    FitCrouching = 3,
    FitCrawling = 4,
    FitSlideJump = 6,
    FitKneeDrop = 7,
};

// Exit points of the playable characters' models: the hand the second of the tied characters slams with, the head, and the feet
// (footprints are left at them)
enum CharacterModelExitPoint : u32
{
    ExitPointHand = 0,
    ExitPointHead = 1,
    ExitPointRightFoot = 6,
    ExitPointLeftFoot = 7,
};

// The sphere a character splashes into water with (its vehicle's own while it rides one): its radius, 1 above its position
constexpr f32 CharacterSplashRadius = Rounded(0.9);
constexpr f32 CharacterSplashRaise = 1.0f;

// Where LiftOntoGround puts a character after a set back and the frame after its state is applied: the ground found up to the
// reach below its position, cast from the height above it
constexpr f32 CharacterLiftHeight = 2.0f;
constexpr f32 CharacterLiftReach = 20.0f;

// CharacterAgent's height states: on the ground and in a jump (from every take-off)
enum CharacterHeightState : s32
{
    HeightOnGround = 0,
    HeightJumping = 1,
};

// The objects that pose a model's joints through its animator's joint callbacks (retail vtable D_002F0380 at 0: 1 destructor,
// 2-4 abstract). HeadTracking, CharacterLink, ProceduralJoints, LookController and SpringSkeleton derive from it.
// Slot 2: its callbacks added on the joints it poses (AddJointCallback), slot 3: taken out (RemoveJointCallback); both get the
// animator of the instance's model node (AttachToModelAnimator/DetachFromModelAnimator). Slot 4: told of a joint's animation once
// TransformJoints has worked it out and before the joint's matrix is made: it adds turns or a translation to the animator, or
// makes the matrix itself (CreateJointTransform marking it done) and changes it. Its result isn't read
class JointHook
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        AttachSlot = 2,
        DetachSlot = 3,
        PoseJointSlot = 4,
    };

    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00122f40);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void AttachVirtual(OgiAnimator* animator)
    {
        CallVirtual<void>(this, vtable, AttachSlot, animator);
    }

    void DetachVirtual(OgiAnimator* animator)
    {
        CallVirtual<void>(this, vtable, DetachSlot, animator);
    }

    u32 PoseJointVirtual(JointAnimator* animator, Matrix4x4* matrix)
    {
        return CallVirtual<u32>(this, vtable, PoseJointSlot, animator, matrix);
    }
};
CHECK_SIZE(JointHook, 4);

// The crouch, the crawl and the knee slide (circle on the ground; the agent's 0x98, 0x40 bytes, plain struct). The crouch's frame
// gives the part bit 32 (not standing), 41 (crawling) and AttackSlide while sliding
struct CrouchController
{
    enum State : u8
    {
        StateStanding = 0,
        StateGoingDown = 1,
        StateCrouched = 2,
        StateCrawling = 3,
        StateGettingUp = 4,
        StateSliding = 5,
        StateSlideEnd = 6,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 8;
            u32 unused8 : 1;
            // The slide's attack kind is AttackSlideVariant rather than AttackSlide (nothing sets it)
            u32 slideVariantKind : 1;
            // The jump may start (clear for the slide's first property 0x2D seconds)
            u32 mayJump : 1;
            // Float property 0x29 (the crouch's time) isn't 0
            u32 canCrouch : 1;
            u32 unused12 : 20;
        };
    };

    // Float properties (and the tagged ones' turn rates)
    enum Property : u32
    {
        PropCrawlSpeed = 0x28,
        PropCrouchSeconds = 0x29,
        PropCrouchToStandSeconds = 0x2A,
        PropCrawlToStandSeconds = 0x2B,
        PropSlideSpeed = 0x2C,
        PropSlideSteerSeconds = 0x2D,
        PropSlideRestSeconds = 0x2E,
        PropSlideEndSeconds = 0x2F,
        PropSlideToCrouchSeconds = 0x30,
        PropSlideToStandSeconds = 0x31,
        TaggedCrawlTurn = 4,
        TaggedSlideTurn = 5,
    };

    // Retail reads and writes the bits as the u64 at 0 (the cooldown its high half, written back as it was)
    Bits bits;
    f32 cooldown;
    s32 stateStart;
    s32 stateTicks;
    f32 slideSpeed;
    u8 unused14[0xC];
    Vector4 slideDirection;
    f32 slideFade;
    CharacterAgent* agent;
    u8 unused38[8];

    static CrouchController* Construct(CrouchController* crouch, CharacterAgent* agent) RETAIL(FUN_0015ea38);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015ea68);
    // Standing, may jump, canCrouch by property 0x29, fade 1 (the cooldown kept)
    void Reset() RETAIL(FUN_0015ea90);
    // State 0: a knee slide or the crouch's start (circle pressed this frame or not), the next state written when there's one
    void Stand(u32 circle, s32* next) RETAIL(FUN_00143828);
    // State 5 (the circle unused) and state 6: the stick's turn, the clock's time
    void Slide(f32 turn, s32 time, u32 unused, s32* next) RETAIL_N32(FUN_00143a58);
    void SlideEnd(f32 turn, s32 time, u32 circle, s32* next) RETAIL_N32(FUN_00143d38);
    // The frame (CrouchFrame's): circle's press, the stick's size and turn. Whether it isn't standing
    u32 Frame(f32 circle, f32 stick, f32 turn, TimeClock* clock) RETAIL_N32(FUN_00143fd0);
};
CHECK_OFFSET(CrouchController, stateTicks, 0xC);
CHECK_OFFSET(CrouchController, slideDirection, 0x20);
CHECK_OFFSET(CrouchController, slideFade, 0x30);
CHECK_OFFSET(CrouchController, agent, 0x34);
CHECK_SIZE(CrouchController::Bits, 4);
CHECK_SIZE(CrouchController, 0x40);

// The jumps (cross) and the air attacks (circle in the air): the jump, the tied jump, the double jump, the slide jump, launches
// (PushBack's), the knee drop (body slam), Crash's flying kick and Cortex's radial blast (the agent's 0xA0, 0x30 bytes, plain
// struct). The jump's frame gives the part AttackSlam in the knee drop and bits 33-35, 39 and 40
struct JumpController
{
    enum State : u32
    {
        StateGrounded = 0,
        StateRising = 1,
        StateRisingTied = 2,
        StateRisingDouble = 3,
        StateRisingSlide = 4,
        StateRisingLaunched = 5,
        StateKneeDropHang = 6,
        StateKneeDrop = 7,
        // The unused kind's (nothing starts it)
        StateRisingUnused = 8,
        StateFallingUnused = 9,
        StateFlyingKick = 10,
        StateFallingKick = 11,
        StateBlastHang = 12,
        StateBlastRising = 13,
        StateBlastFalling = 14,
        StateFallingTied = 15,
        StateFalling = 16,
        StateFallingDouble = 17,
        StateFallingSlide = 18,
        StateFallingLaunched = 19,
        // In the next state's bits: none
        StateNone = 20,
    };

    // The kinds of jumps (Start's and the others' kind)
    enum Kind : u32
    {
        KindJump = 2,
        KindTied = 3,
        KindDouble = 4,
        KindSlide = 5,
        KindLaunch = 6,
        KindKneeDrop = 7,
        // Nothing starts it (it falls with the slide jump's gravity)
        KindUnused = 8,
        KindFlyingKick = 9,
        KindRadialBlast = 10,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 5;
            // StateNone: none
            u32 next : 5;
            u32 crossHeld : 1;
            u32 circleHeld : 1;
            u32 mayDoubleJump : 1;
            u32 mayAttack : 1;
            // A launch queued for the next frame, and its event (EventNone none)
            u32 launchQueued : 1;
            u32 launchEvent : 8;
            // A take-off's push off what it stands on still to be given (StandOnBody clears it)
            u32 jumped : 1;
            // The knee drop's attack kind is AttackSlamVariant rather than AttackSlam (nothing sets it)
            u32 slamVariantKind : 1;
            // The knee drop waits for room (cleared the next frame before it's read)
            u32 kneeDropWaits : 1;
            u32 unused26 : 6;
        };
    };

    enum Property : u32
    {
        PropGravity = 1,
        // Written: 0.95 of the flight's seconds
        PropFlightSeconds = 0xE,
        PropAirSpeed = 0xF,
        PropJumpSpeed = 0x10,
        PropJumpGravityUp = 0x11,
        PropJumpGravityDown = 0x12,
        PropDoubleAirSpeed = 0x13,
        PropDoubleSpeed = 0x14,
        PropDoubleGravityUp = 0x15,
        PropDoubleGravityDown = 0x16,
        PropSlideAirSpeed = 0x17,
        PropSlideSpeed = 0x18,
        PropSlideGravityUp = 0x19,
        PropSlideGravityDown = 0x1A,
        PropAttackWindowUp = 0x1D,
        PropAttackWindowDown = 0x1E,
        PropKneeDropHangSeconds = 0x1F,
        PropKneeDropSpeed = 0x20,
        PropKneeDropGravity = 0x21,
        PropFlyingKickSeconds = 0x22,
        PropFlyingKickSpeed = 0x23,
        PropFlyingKickGravity = 0x24,
        PropBlastHangSeconds = 0x25,
        PropBlastSpeed = 0x26,
        PropBlastGravity = 0x27,
        TaggedJumpTurn = 6,
        TaggedDoubleTurn = 7,
        TaggedSlideTurn = 8,
    };

    Bits bits;
    f32 gravity;
    f32 upSpeed;
    f32 airSpeed;
    s32 airTurn;
    s32 leftGroundTime;
    s32 crossTime;
    s32 circleTime;
    s32 stateStart;
    s32 stateTicks;
    s32 flightTicks;
    CharacterAgent* agent;

    static JumpController* Construct(JumpController* jump, CharacterAgent* agent) RETAIL(FUN_0015edf0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015ee20);
    void Reset() RETAIL(FUN_00149780);
    // A launch for the next frame (PushBack's): its upward speed and gravity, its event (EventNone none)
    void QueueLaunch(f32 upSpeed, f32 gravity, u32 event) RETAIL_N32(FUN_0015ee48);
    // When a press came within the current phase: 0 before it, 1 before its last ticks, 2 later
    u32 PressPhase(const TimeClock* clock, s32 pressTime, s32 lastTicks) RETAIL(FUN_00148560);
    // A kind's gravity rising or falling (the agent's properties passed)
    f32 KindGravity(PropertyHolder* properties, u32 kind, u32 falling) RETAIL(FUN_001485b8);
    // The event a jump of a kind begins with (the late double jump maxed); the leader's tied jumps run on the second too
    u32 KindEvent(u32 kind, u32 maxed) RETAIL(FUN_001486e8);
    // A kind starts falling (the agent's StartFalling told when asked): the falling state, -1
    s32 Fall(u32 kind, u32 tellFall) RETAIL(FUN_00148818);
    // A jump of a kind starts (the double jump maxed, or in a fall): the rising state, -1
    s32 Start(TimeClock* clock, u32 kind, u32 maxed, u32 midFall) RETAIL(FUN_00148940);
    // An air attack from a jump of a kind: its hang's state, -1 (callers pass a third argument it doesn't read)
    s32 AirAttack(u32 kind) RETAIL(FUN_00148e38);
    // The states' steps, each writing the next state when there's one: on the ground (may jump, cross pressed), rising, a
    // hang (its third argument unread), falling
    void Grounded(TimeClock* clock, u32 mayJump, u32 cross, s32* next) RETAIL(FUN_00148ff0);
    void Rise(TimeClock* clock, u32 kind, s32* next) RETAIL(FUN_001491e0);
    void Hang(TimeClock* clock, u32 kind, u32 unused, s32* next) RETAIL(FUN_001493d8);
    void FallFrame(TimeClock* clock, u32 kind, s32* next) RETAIL(FUN_001495b0);
    // The frame (JumpFrame's): cross's and circle's press, whether it may jump (the crouch's mayJump, 1 without a crouch)
    void Frame(f32 cross, f32 circle, TimeClock* clock, u32 mayJump) RETAIL_N32(FUN_00149818);
};
CHECK_OFFSET(JumpController, airTurn, 0x10);
CHECK_OFFSET(JumpController, stateStart, 0x20);
CHECK_OFFSET(JumpController, agent, 0x2C);
CHECK_SIZE(JumpController::Bits, 4);
CHECK_SIZE(JumpController, 0x30);

// The spin (square; circle for the characters with a gun), alone or tied (the agent's 0xA8, 0x14 bytes, plain struct). The spin's
// frame gives the part AttackSpin in states 1 and 2
struct SpinController
{
    enum State : u32
    {
        StateNone = 0,
        StateSpinningTied = 1,
        StateSpinning = 2,
        StateRecovering = 3,
        // In the next state's bits: none
        StateNoNext = 4,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 4;
            u32 next : 4;
            u32 buttonHeld : 1;
            u32 unused9 : 23;
        };
    };

    enum Property : u32
    {
        PropSpinSpeed = 9,
        PropSpinSeconds = 10,
        PropRecoverySeconds = 11,
        TaggedSpinTurn = 3,
    };

    Bits bits;
    s32 stateStart;
    s32 stateTicks;
    // 65536ths of a turn, 0x21D1 more a frame
    s32 sweepAngle;
    CharacterAgent* agent;

    static SpinController* Construct(SpinController* spin, CharacterAgent* agent) RETAIL(FUN_0015fe60);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015fe90);
    void Reset() RETAIL(FUN_0015feb8);
    // The four rays (their reach) pushing the character back (by the factor) and touching instances
    void Sweep(f32 reach, f32 pushBack) RETAIL_N32(FUN_0014e500);
    // A spin starts and ends (callers pass 0, unread): the next state (0 none)
    u32 Start() RETAIL(FUN_0014e950);
    u32 End(u32 unused) RETAIL(FUN_0014ea60);
    // The frame (SpinFrame's): the button's press, an unread float, the stick's size
    void Frame(f32 button, f32 unused, f32 stick, TimeClock* clock) RETAIL_N32(FUN_0014ebe0);
};
CHECK_OFFSET(SpinController, sweepAngle, 0xC);
CHECK_SIZE(SpinController::Bits, 4);
CHECK_SIZE(SpinController, 0x14);

// The stick's ground movement (idle, shuffling, walking, running, strafing with L1/R1), the air's steering and pushes (the agent's
// 0xAC, 0xA0 bytes, plain struct). The walk's frame gives the part bits 42 (walking), 43 (running) and 44 (strafe held)
struct WalkController
{
    enum State : u32
    {
        StateNone = 1,
        StateIdle = 2,
        StateShuffling = 3,
        StateWalking = 4,
        StateRunning = 5,
        StateStrafing = 6,
        StateInAir = 7,
        StateSlamming = 8,
        StatePushed = 9,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 4;
            // 0: none
            u32 next : 4;
            u32 strafeHeld : 1;
            // The push is a velocity (else a speed), and it eases over pushTicks
            u32 pushVelocity : 1;
            u32 pushEases : 1;
            u32 unused11 : 21;
        };
    };

    enum Property : u32
    {
        PropWalkSpeed = 6,
        PropRunSpeed = 7,
        PropStrafeSpeed = 8,
        TaggedIdleTurn = 0,
        TaggedWalkTurn = 1,
        TaggedRunTurn = 2,
    };

    // Retail reads and writes the bits as the u64 at 0 (the strafe its high half, reloaded before each write)
    Bits bits;
    f32 strafe;
    s32 pushTicks;
    s32 stateStart;
    s32 stoodTime;
    u8 unused14[0xC];
    Vector4 pushFrom;
    Vector4 pushTo;
    Vector4 pushVelocity;
    CharacterAgent* agent;
    f32 pushSpeedFrom;
    f32 pushSpeedTo;
    f32 airSpeed;
    s32 airTurn;
    u8 unused64[0xC];
    Vector4 moveDirection;
    Vector4 faceDirection;
    f32 speed;
    // 65536ths of a turn a second
    s32 turn;
    u8 unused98[8];

    static WalkController* Construct(WalkController* walk, CharacterAgent* agent) RETAIL(FUN_001602b8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001602e8);
    // State 1, nothing asked, no push (its low word cleared with memset); the second is a copy (Unlink's)
    void Reset() RETAIL(FUN_00160310);
    void ResetCopy() RETAIL(FUN_00160570);
    // Float property 7, else 6
    f32 TopSpeed() RETAIL(FUN_00160250);
    // Float property 8 isn't 0
    u32 Strafes() RETAIL(FUN_00160640);
    // Idle at once and the part's attack kind AttackWalkInto (LinkFrame's)
    void BeIdle() RETAIL(FUN_001603e0);
    // Pushed (next state 9) at a speed or a velocity eased from one to the other over the ticks (0: the end's at once)
    void PushAtSpeed(f32 from, f32 to, s32 ticks) RETAIL_N32(FUN_00160420);
    void PushAtVelocity(const Vector4* from, const Vector4* to, s32 ticks) RETAIL(FUN_00160468);
    // The push's velocity bounced off a surface with a restitution: 0
    u32 BouncePush(f32 restitution, const Vector4* normal) RETAIL_N32(FUN_001604b8);
    // The next state on the ground (the stick's size, the strafe): 0 no change
    u32 NextState(f32 stick, f32 strafe, TimeClock* clock) RETAIL_N32(FUN_0014efe0);
    // The turn asked (the stick's turn, at most a rate a second) and the speed and turn asked of the move (straight ahead or
    // along the move direction)
    void AskTurn(f32 turn, TimeClock* clock, const s32* rate) RETAIL_N32(FUN_0014f330);
    void AskMove(u32 straight) RETAIL(FUN_0014f628);
    // States 6 (the first float unread) and 9
    void Strafe(f32 unused, f32 strafe, TimeClock* clock, s32* next) RETAIL_N32(FUN_0014f778);
    void Pushed(f32 stick, f32 strafe, TimeClock* clock, s32* next) RETAIL_N32(FUN_0014fb10);
    // The frame (WalkFrame's): the strafe (R - L), the stick's turn, its vector. Whether it walks or runs
    u32 Frame(f32 strafe, f32 turn, TimeClock* clock, const Vector4* stick) RETAIL_N32(FUN_0014fdf8);
};
CHECK_OFFSET(WalkController, stoodTime, 0x10);
CHECK_OFFSET(WalkController, pushFrom, 0x20);
CHECK_OFFSET(WalkController, agent, 0x50);
CHECK_OFFSET(WalkController, airTurn, 0x60);
CHECK_OFFSET(WalkController, moveDirection, 0x70);
CHECK_OFFSET(WalkController, turn, 0x94);
CHECK_SIZE(WalkController::Bits, 4);
CHECK_SIZE(WalkController, 0xA0);

// Two characters tied together holding hands (Crash and Cortex; the agent's 0xB0, 0x220 bytes, vtable D_002F3610). Each has
// one: the leader's (LinkCharacters' first, part bit 53) references the second (part bit 54), the second's the leader. Circle
// slams them (AttackTied for the leader)
class CharacterLink : public JointHook
{
public:
    enum State : u32
    {
        StateDetached = 0,
        StateTied = 1,
        StateSlamming = 2,
    };

    // The second's gaits as its swing goes
    enum Gait : u32
    {
        GaitNone = 0,
        GaitIdle = 1,
        GaitWalking = 2,
        GaitRunning = 3,
        GaitShuffling = 4,
    };

    // How the arms hold: the shoulder not posed yet, its place seen, holding hands
    enum Hold : u32
    {
        HoldNone = 0,
        HoldSeen = 1,
        HoldHands = 2,
    };

    // The bits 32-45 of the u64 at 0 retail reads and writes (the vtable its low half)
    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 4;
            u32 gait : 4;
            u32 hold : 4;
            u32 leader : 1;
            // The second's instance was a projectile when they were tied (the leader's link puts that back when it's destroyed)
            u32 secondWasProjectile : 1;
            u32 unused14 : 18;
        };
    };

    Bits bits;
    CharacterAgent* character;
    // The second's instance on the leader's link, the leader's on the second's
    Reference* second;
    Reference* leader;
    u8 unused14[0xC];
    Vector4 upperArm;
    Vector4 forearm;
    f32 stretch;
    u8 unused44[0xC];
    Vector4 shoulder;
    Vector4 shoulderNow;
    Vector4 shoulderShift;
    Vector4 handPoint;
    Vector4 chain[5];
    f32 reach;
    u8 unusedE4[0xC];
    Vector4 elbow;
    Vector4 hand;
    void* ikChain;
    u8 unused114[0x8C];
    s32 slamStart;
    f32 slamSeconds;
    f32 slamCooldown;
    u8 unused1AC[4];
    Matrix4x4 secondMatrix;
    Vector4 swingPosition;
    Vector4 swingVelocity;
    f32 swingLength;
    f32 blendIn;
    u8 unused218[8];

    // Made for a character tied to another (the leader's link when asked)
    static CharacterLink* Construct(CharacterLink* link, CharacterAgent* character, CharacterAgent* other, u32 leader)
        RETAIL(FUN_0014d7f8);
    // Its vtable's slots 1 to 4
    void Destroy(u32 destroyFlags) RETAIL(FUN_0014dc18);
    void Attach(OgiAnimator* animator) RETAIL(FUN_0015f9a8);
    void Detach(OgiAnimator* animator) RETAIL(FUN_0015fa90);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_0014c748);
    // The leader's IK between the shoulders, and the chain solved from a start to an end into chain[]: the solver's result
    void SolveArms() RETAIL(FUN_0014d178);
    s32 SolveChain(const Vector4* start, const Vector4* end) RETAIL(FUN_0014d5f0);
    // The slam started at a time (not while spinning): whether it was
    u32 StartSlam(const s32* time) RETAIL(FUN_0014dd18);
    // The frame (LinkFrame's): circle's press (the leader's)
    void Frame(f32 circle, TimeClock* clock) RETAIL_N32(FUN_0014de38);
    // The second's gait from its swing's velocity and the leader's facing (its event's result left in $v0, unused)
    void SecondGait(const Vector4* velocity, const Vector4* facing) RETAIL(FUN_0014e0a8);
    void SetStretch(f32 stretch) RETAIL_N32(FUN_0015f968);
    // A quarter of the stretch (the spine's lean, radians) and three quarters
    f32 SpineLean() RETAIL(FUN_0015f970);
    f32 ThreeQuartersStretch() RETAIL(FUN_0015f988);
    // Whether the slam is 0.2 to 0.99 seconds in, and how much the spine leans during it
    u32 MidSlam() RETAIL(FUN_0015fb70);
    f32 SlamLeanWeight() RETAIL(FUN_0015fc30);
    // Through a chunk link (the chunk unread): the second's kept matrix and instance, or its swing
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0015fd10);
    // The leader from the second's link (player.h's LinkedCharacter), the second from the leader's: none for the other
    CharacterAgent* Leader() RETAIL(FUN_0015fdc0);
    CharacterAgent* Second() RETAIL(FUN_0015fe08);
};
CHECK_OFFSET(CharacterLink, bits, 0x4);
CHECK_OFFSET(CharacterLink, upperArm, 0x20);
CHECK_OFFSET(CharacterLink, stretch, 0x40);
CHECK_OFFSET(CharacterLink, shoulder, 0x50);
CHECK_OFFSET(CharacterLink, handPoint, 0x80);
CHECK_OFFSET(CharacterLink, chain, 0x90);
CHECK_OFFSET(CharacterLink, reach, 0xE0);
CHECK_OFFSET(CharacterLink, elbow, 0xF0);
CHECK_OFFSET(CharacterLink, ikChain, 0x110);
CHECK_OFFSET(CharacterLink, slamStart, 0x1A0);
CHECK_OFFSET(CharacterLink, secondMatrix, 0x1B0);
CHECK_OFFSET(CharacterLink, swingPosition, 0x1F0);
CHECK_OFFSET(CharacterLink, swingLength, 0x210);
CHECK_OFFSET(CharacterLink, blendIn, 0x214);
CHECK_SIZE(CharacterLink::Bits, 4);
CHECK_SIZE(CharacterLink, 0x220);

// The character's size (the agent's 0xC0, 0x130 bytes, plain struct): standing and crouched heights and radii, their box hulls,
// the probes (exit points, offsets, weights) keeping its limbs out of walls. The conditions read its floats as an array ([1] the
// eye height, [3] while ducking)
struct CharacterBody
{
    CharacterAgent* agent;
    f32 height;
    f32 radius;
    f32 crouchHeight;
    f32 crouchRadius;
    CollisionHull standingHull;
    CollisionHull crouchHull;
    s32 probeExitPoints[7];
    u8 unused70[0x10];
    Vector4 probeOffsets[7];
    u8 unusedF0[0x10];
    f32 probeWeights[7];
    u8 unused11C[4];
    s32 probeCount;
    u8 unused124[0xC];

    static CharacterBody* Construct(CharacterBody* body, CharacterAgent* agent) RETAIL(FUN_0013fee0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ff48);
    // By the character (its first int property), then the hulls; Crash's and Mecha-Bandicoot's values
    void SetSizes() RETAIL(FUN_0013c6c8);
    void SetCrashSizes() RETAIL(FUN_0013c3c0);
    void SetMechaSizes() RETAIL(FUN_0013c540);
};
CHECK_OFFSET(CharacterBody, standingHull, 0x14);
CHECK_OFFSET(CharacterBody, crouchHull, 0x34);
CHECK_OFFSET(CharacterBody, probeExitPoints, 0x54);
CHECK_OFFSET(CharacterBody, probeOffsets, 0x80);
CHECK_OFFSET(CharacterBody, probeWeights, 0x100);
CHECK_OFFSET(CharacterBody, probeCount, 0x120);
CHECK_SIZE(CharacterBody, 0x130);

// A hull ahead of a character picking the instance it aims at, and the marker shown on it (0x110 bytes, no vtable)
struct TargetLock
{
    union Bits
    {
        u32 value;
        struct
        {
            u32 unused0 : 8;
            u32 kindCount : 4;
            // It takes instances that aren't targettable
            u32 takesUntargettable : 1;
            u32 unused13 : 19;
        };
    };

    // In retail the 64 bits from 0 (blockingKinds the high half, written back as it was)
    Bits bits;
    u32 blockingKinds;
    u8 unused08[8];
    Vector4 offset;
    f32 length;
    f32 nearHalf;
    f32 farHalf;
    // (node kind, priority) pairs, as many as the bits' count
    u8 kinds[0x30 / 2][2];
    s32 seenTime;
    Reference* target;
    Reference* marker;
    u8 unused68[8];
    // The target's place's matrix, its translation the middle of the target's collision box
    Matrix4x4 targetMatrix;
    Matrix4x4 hullMatrix;
    CollisionHull hull;

    static TargetLock* Construct(TargetLock* lock) RETAIL(FUN_0015d098);
    // Its node kinds' count and takesUntargettable kept; no time, target or marker
    void Reset() RETAIL(FUN_0015d0e8);
    // Its hull made from its length and half sizes, its bottom flat (at the near half size) when asked
    void MakeHull(u32 flat) RETAIL(FUN_0015cd88);
    void SetShape(f32 length, f32 nearHalf, f32 farHalf) RETAIL_N32(FUN_00161c38);
    void SetFlatShape(f32 length, f32 nearHalf, f32 farHalf) RETAIL_N32(FUN_00161c68);
    // A node kind it takes and its priority (lower ranks first)
    void AddKind(u32 kind, u32 priority) RETAIL(FUN_00161d58);
    // Awake, targettable and of one of its kinds with a priority
    u32 CanTarget(InstanceContext* instance) RETAIL(FUN_0015c8e8);
    // A frame: the best instance in the hull searched for (the owner's place; the holder's first attached instance is the
    // marker), kept (only its time refreshed) or dropped (the marker hidden)
    void Search(TimeClock* clock, InstanceContext* owner, InstanceContext* holder) RETAIL(FUN_0015d1c0);
    void Keep(TimeClock* clock, InstanceContext* owner, InstanceContext* holder) RETAIL(FUN_00161c98);
    void Drop() RETAIL(FUN_0015d8a8);
    // The marker shown 2 units in front of the target toward the camera (the owner's follow node's object), hidden without one;
    // retail returns what the queue left in $v0, which no caller reads
    void PlaceMarker(InstanceContext* owner, InstanceContext* holder) RETAIL(FUN_0015c9d0);
    // The target's point (the middle of its box): whether it has a target
    u32 TargetPoint(Vector4* point) RETAIL(FUN_00161d28);
};
CHECK_OFFSET(TargetLock, offset, 0x10);
CHECK_OFFSET(TargetLock, kinds, 0x2C);
CHECK_OFFSET(TargetLock, seenTime, 0x5C);
CHECK_OFFSET(TargetLock, targetMatrix, 0x70);
CHECK_OFFSET(TargetLock, hull, 0xF0);
CHECK_SIZE(TargetLock::Bits, 4);
CHECK_SIZE(TargetLock, 0x110);

// The instances a search ranks (the targets table): a reference and its score (the kind's priority << 24, then how far off
// the hull's sideways axis times the squared distance)
struct TargetEntry
{
    Reference* instance;
    s32 score;
};
CHECK_SIZE(TargetEntry, 8);

// Nina's claw (0x170 bytes, no vtable): its claw is the graple instance hanging on her (held by one of her attachments, an
// instance's, kind 0)
struct ClawController
{
    enum State : u32
    {
        StateReady = 0,
        StateReaching = 1,
        StateFlying = 2,
        // The claw coming back to her hand: nothing enters it
        StateReturning = 3,
        StatePulled = 4,
        StateHanging = 5,
        StateLettingGo = 6,
        StateDropped = 7,
        StateJumpedOff = 8,
        StateLeaping = 9,
        StateWindingUp = 10,
        StateSwipingOut = 11,
        StateSwipingBack = 12,
        StateAfterSwipe = 13,
        // In the next state's bits: none
        StateNoNext = 14,
    };

    enum Grab : u32
    {
        GrabNone = 0,
        GrabHook = 1,
        GrabPoint = 2,
        GrabSwipe = 3,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 4;
            u32 next : 4;
            u32 grab : 3;
            u32 unused11 : 1;
            u32 circleHeld : 1;
            u32 crossHeld : 1;
            u32 unused14 : 18;
        };
    };

    // In retail the 64 bits from 0 (stateStart the high half)
    Bits bits;
    s32 stateStart;
    s32 duration;
    s32 circleTime;
    CharacterAgent* agent;
    Reference* graple;
    s32 grabYaw;
    s32 startYaw;
    Vector4 hand;
    Vector4 claw;
    Vector4 from;
    Vector4 to;
    TargetLock lock;

    static ClawController* Construct(ClawController* claw, CharacterAgent* agent) RETAIL(FUN_001466b8);
    void Reset() RETAIL(FUN_00146740);
    // The character's frame's step, with circle's and cross's press powers
    void Frame(f32 circle, f32 cross, TimeClock* clock) RETAIL_N32(FUN_001467d0);
    // Each written when asked: her hand's place, her facing's yaw, the claw's target point, the yaw to face at the grab and
    // the grab point (not written while it's zero)
    void AimPoints(Vector4* hand, s32* facingYaw, Vector4* point, s32* grabYaw, Vector4* grabPoint) RETAIL(FUN_001445f0);
    // The graple's ends set (its anchor the hand, its target the claw) and the graple turned like her hand, or (states 4-6, not
    // tied) her hand moved there; the clock isn't read, and retail's result (whether it queued) isn't either
    void PlaceGraple(TimeClock* clock, const Vector4* claw, const Vector4* hand) RETAIL(FUN_00144a90);
    // The states' steps (states 0, 1, 2, 3, 4, 6, 9, 10, 11, 12): the pressed button (circle, cross for LetGo) and where the next
    // state goes. Some don't read the clock or the button; their results (what PlaceGraple or an event returned) aren't read
    void Ready(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_00144e28);
    void Reach(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_00145050);
    void Fly(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_001451e0);
    void Retract(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_00145518);
    void Pull(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_001456e0);
    void LetGo(TimeClock* clock, u32 cross, s32* next) RETAIL(FUN_00145b58);
    void Leap(TimeClock* clock, s32* next) RETAIL(FUN_00145c30);
    void WindUp(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_00145e18);
    void SwipeOut(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_00146298);
    void SwipeBack(TimeClock* clock, u32 circle, s32* next) RETAIL(FUN_001464a8);
};
CHECK_OFFSET(ClawController, agent, 0x10);
CHECK_OFFSET(ClawController, hand, 0x20);
CHECK_OFFSET(ClawController, to, 0x50);
CHECK_OFFSET(ClawController, lock, 0x60);
CHECK_SIZE(ClawController::Bits, 4);
CHECK_SIZE(ClawController, 0x170);

// Cortex's and the Mecha-Bandicoot's gun (0x460 bytes, no vtable; the HUD's counter at the bottom right shows its ammo)
struct Gun
{
    enum State : u32
    {
        StatePutAway = 0,
        StateDrawing = 1,
        StateOut = 2,
        StatePuttingAway = 3,
        StateCharging = 4,
        StateCharged = 5,
        StateShot = 6,
        // In the next state's bits: none
        StateNoNext = 7,
    };

    union Bits
    {
        u32 value;
        struct
        {
            u32 state : 4;
            u32 next : 4;
            u32 squareHeld : 1;
            // A count nothing but its reset and an unused command class changes (5 after a reset, at most 9; the scripts'
            // condition 633 reads it)
            u32 secondCount : 4;
            u32 ammo : 7;
            u32 unused20 : 12;
        };
    };

    // Its locks: on foot, in a vehicle, the Mecha-Bandicoot's, Cortex's in area 24
    enum Lock : u32
    {
        LockOnFoot = 0,
        LockVehicle = 1,
        LockMecha = 2,
        LockArea24 = 3,
    };

    CharacterAgent* agent;
    Reference* gunInstance;
    // In retail the 64 bits from 8 (duration the high half)
    Bits bits;
    s32 duration;
    s32 stateStart;
    s32 shotTime;
    // 1 charged, 0 normal, -1 none since the reset
    f32 shotCharge;
    u8 unused1C[4];
    TargetLock locks[4];

    static Gun* Construct(Gun* gun, CharacterAgent* agent) RETAIL(FUN_00149f28);
    void Reset() RETAIL(FUN_0015ee90);
    // The character's frame's step: square's press power and whether it may aim (1 or 0)
    void Frame(f32 square, f32 allowed, TimeClock* clock) RETAIL_N32(FUN_0014a100);
    // A shot of a charge (1 charged, 0 normal): the next state (6)
    u32 Shoot(f32 charge, TimeClock* clock) RETAIL_N32(FUN_00149d78);
    InstanceContext* Target() RETAIL(FUN_0014a730);
    u32 AimPoint(Vector4* point) RETAIL(FUN_0014a7d0);
    // Ammo taken (without enough: emptied, no) and added (below 100, else made 99: no)
    u32 TakeAmmo(u32 count) RETAIL(FUN_0015ef58);
    u32 AddAmmo(s32 amount) RETAIL(FUN_0015f010);
    // The second count given an amount: whether it stays below 10, else made 9 (an amount taking it below 0 too)
    u32 AddSecondCount(s32 amount) RETAIL(FUN_0015efb8);
    // Whether it shot no more than so many ticks ago
    u32 ShotWithin(s32 ticks) RETAIL(FUN_0015f078);
};
CHECK_OFFSET(Gun, bits, 0x8);
CHECK_OFFSET(Gun, shotTime, 0x14);
CHECK_OFFSET(Gun, shotCharge, 0x18);
CHECK_OFFSET(Gun, locks, 0x20);
CHECK_SIZE(Gun::Bits, 4);
CHECK_SIZE(Gun, 0x460);

// One of the procedural joints (0x90 bytes, no vtable)
struct ProceduralJoint
{
    enum Kind : s32
    {
        KindNone = 0,
        KindDangling = 1,
        KindLeg = 2,
        KindSlopeTilt = 3,
        KindSquash = 4,
    };

    Vector4 anchor;
    Vector4 motion;
    Vector4 slowMotion;
    s32 angles[3];
    u32 unused3C;
    Vector4 normal;
    s32 kind;
    f32 normalSpeed;
    // An exit point (-1: the instance's position)
    s32 anchorExitPoint;
    s32 joint;
    f32 halfLife;
    f32 slowHalfLife;
    f32 gain;
    f32 secondGain;
    f32 unused70;
    f32 limit;
    f32 secondLimit;
    s32 axis;
    s32 secondAxis;
    s32 slot;
    s32 secondSlot;
    u8 started;
    u8 unused8D[3];

    static ProceduralJoint* Construct(ProceduralJoint* joint) RETAIL(FUN_0015f528);
    void Reset() RETAIL(FUN_0015f550);
    // Dangling from an anchor: its smoothing half-lives, its angles' gains, a value never read, their limits (radians), the lag's
    // axes and the angles they go into (0 x, 1 y, 2 z)
    void MakeDangling(f32 halfLife, f32 slowHalfLife, f32 gain, f32 secondGain, f32 unused, f32 limit, f32 secondLimit,
                      s32 anchorExitPoint, s32 joint, s32 slot, s32 secondSlot, s32 axis, s32 secondAxis) RETAIL_N32(FUN_0015f610);
    void MakeLeg(s32 joint) RETAIL(FUN_0015f798);
    void MakeSlopeTilt(f32 normalSpeed, s32 joint) RETAIL_N32(FUN_0015f7a8);
    void MakeSquash(s32 joint) RETAIL(FUN_0015f7c0);
    void Step(f32 seconds, CharacterAgent* agent) RETAIL_N32(FUN_0014bdc0);
    // Its smoothed movements made still (the caller passes the agent, which isn't read)
    void Still(CharacterAgent* unused) RETAIL(FUN_0015f860);
    // Put back at rest after a tied spin (the second character's, the spin's end): its motion the character's x axis, its slow
    // motion (0, 0, 0, 1)
    void Settle(CharacterAgent* agent) RETAIL(FUN_0015f7d0);
    // Its positions taken through a link (only with the link's bit 18): whether they were (the chunk isn't read)
    u32 ChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0015f8d0);
};
CHECK_OFFSET(ProceduralJoint, normal, 0x40);
CHECK_OFFSET(ProceduralJoint, halfLife, 0x60);
CHECK_OFFSET(ProceduralJoint, started, 0x8C);
CHECK_SIZE(ProceduralJoint, 0x90);

// The legs stretched to the ground, joints dangling behind the character's motion, its body tilted to the slope and Crash's
// landing squash (0x30 bytes, retail vtable D_002F3550)
class ProceduralJoints : public JointHook
{
public:
    CharacterAgent* agent;
    s32 count;
    // new[]'s array (its count 0x10 bytes before)
    ProceduralJoint* elements;
    u8 attached;
    u8 unused11[3];
    // The stretch and the foot's tilt of the leg on the character's -x side (A: joints 6 to 8) and of the one on its +x side (B:
    // joints 9 to 11)
    f32 legStretchA;
    f32 legStretchB;
    f32 footTiltA;
    f32 footTiltB;
    u8 placesFeet;
    u8 squashes;
    u8 unused26[2];
    f32 squash;
    f32 squashSpeed;

    static ProceduralJoints* Construct(ProceduralJoints* joints, CharacterAgent* agent) RETAIL(FUN_0015f1a8);
    // Its elements by the character
    void SetUp() RETAIL(FUN_0014a940);
    void MakeElements(s32 count) RETAIL(FUN_0015f0c8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015f1f8);
    void Attach(OgiAnimator* animator) RETAIL(FUN_0015f370);
    void Detach(OgiAnimator* animator) RETAIL(FUN_0015f3f0);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_0014b7d0);
    void Frame(TimeClock* clock) RETAIL(FUN_0014b640);
    void PlaceFeet(f32 seconds) RETAIL_N32(FUN_0014add0);
    // Squashed down to a scale (when it's lower than now)
    void Squash(f32 squash) RETAIL_N32(FUN_0015f2c0);
    // Squashed to a scale at once (no speed)
    void SetSquash(f32 squash) RETAIL_N32(FUN_0015f2f8);
    void Still() RETAIL(FUN_0015f308);
    // Every element through a link the character goes through (whether they all went; no for links without bit 18)
    u32 ChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0015f470);
};
CHECK_OFFSET(ProceduralJoints, elements, 0xC);
CHECK_OFFSET(ProceduralJoints, legStretchA, 0x14);
CHECK_OFFSET(ProceduralJoints, placesFeet, 0x24);
CHECK_OFFSET(ProceduralJoints, squash, 0x28);
CHECK_SIZE(ProceduralJoints, 0x30);

// The head and spine turned to look (springs on a pitch and a yaw) and the Mecha-Bandicoot's arm aimed (0xD0 bytes, retail
// vtable PlayableCharacterRelated_Methods)
class LookController : public JointHook
{
public:
    CharacterAgent* agent;
    u8 attached;
    u8 unused09[3];
    f32 carrySide;
    f32 carryForward;
    f32 carrySize;
    u8 carrying;
    u8 unused19[7];
    Vector4 lookPoint;
    u8 looksAtPoint;
    u8 unused31[3];
    f32 freeLookX;
    f32 freeLookY;
    f32 pitchMin;
    f32 pitchMax;
    f32 carryPitchMax;
    f32 yawMin;
    f32 yawMax;
    f32 pitch;
    f32 yaw;
    f32 pitchSpeed;
    f32 yawSpeed;
    // Not set by the constructor
    f32 restSeconds;
    u8 unused64[0xC];
    Vector4 aim;
    Vector4 aimPoint;
    u8 hasAimPoint;
    u8 unused91[3];
    s32 angles[5][3];

    static LookController* Construct(LookController* look, CharacterAgent* agent) RETAIL(FUN_00147310);
    void Reset() RETAIL(FUN_00147380);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015eb68);
    void Attach(OgiAnimator* animator) RETAIL(FUN_0015ebd8);
    void Detach(OgiAnimator* animator) RETAIL(FUN_0015ecc0);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_001480b8);
    void Frame(TimeClock* clock) RETAIL(FUN_00147618);
    void StepLook(TimeClock* clock) RETAIL(FUN_00146d98);
    void Relax(TimeClock* clock) RETAIL(FUN_00147a88);
    // The turn (in the instance's space) bringing a column of a joint's matrix (negated when asked) toward a point
    void AimRotation(const Matrix4x4* joint, u32 axis, u32 negate, const Vector4* point, Vector4* rotation) RETAIL(FUN_00147dc0);
    void SetCarried(f32 side, f32 forward, f32 size, u32 carrying) RETAIL_N32(FUN_0015eda8);
    void SetLookPoint(const Vector4* point, u32 has) RETAIL(FUN_0015edc0);
    void SetAimPoint(const Vector4* point, u32 has) RETAIL(FUN_0015edd0);
};
CHECK_OFFSET(LookController, lookPoint, 0x20);
CHECK_OFFSET(LookController, pitch, 0x50);
CHECK_OFFSET(LookController, restSeconds, 0x60);
CHECK_OFFSET(LookController, aim, 0x70);
CHECK_OFFSET(LookController, angles, 0x94);
CHECK_SIZE(LookController, 0xD0);

// A body of points and springs posing an instance's model joints, blended in and out (retail vtable D_002F5D48: 2-4 abstract):
// its flags, how far it's blended in, when its blend started and how many ticks it takes, its body (game/springbody.h), its
// instance, the instance's matrix and its inverse as the last step took them, and an axis in the world with that axis in the
// instance's space (its points keep from turning the joints about others)
class SpringSkeleton : public JointHook
{
public:
    enum Blend : u32
    {
        BlendedOut = 0,
        BlendingIn = 1,
        BlendedIn = 2,
        BlendingOut = 3,
        BlendNone = 4,
    };

    union Flags
    {
        u32 value;
        struct
        {
            // Attached to its instance's model animator
            u32 attached : 1;
            // The axis is turned into the instance's space every step, and it was
            u32 turnsAxis : 1;
            u32 axisTurned : 1;
            // Its blend and the one asked for (BlendNone: none), which the next step starts
            u32 blend : 4;
            u32 requestedBlend : 4;
            u32 unused11 : 21;
        };
    };

    Flags flags;
    f32 blend;
    s32 blendStart;
    s32 blendTicks;
    u8 unused14[0xC];
    SpringBody body;
    Reference* instance;
    u8 unused64[0xC];
    Matrix4x4 instanceMatrix;
    Matrix4x4 toInstance;
    Vector4 axis;
    Vector4 instanceAxis;

    // Made for an instance with a count of solver passes a step, and its destructor
    static SpringSkeleton* Construct(SpringSkeleton* skeleton, u32 passes, InstanceContext* instance) RETAIL(FUN_00190a18);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_001919d0);
    // Attached to its instance's model animator unless it is, and detached from it when attached: whether it was
    u32 AttachToInstance() RETAIL(FUN_00190280);
    u32 DetachFromInstance() RETAIL(FUN_00190328);
    // A joint (by ID) posed by its points while it's blended at all: moved to its point (toward 0xFF) or turned so the way to
    // another joint follows their points (TurnJointToward), as far as it's blended in
    void PoseJointFromPoints(u32 joint, u32 toward, JointAnimator* animator) RETAIL(FUN_00191900);
    void TurnJointToward(u32 joint, u32 toward, JointAnimator* animator) RETAIL(FUN_001903e0);
    // Its blend stepped (the instance's matrices taken while it's blended at all), and blended in over so many ticks (0: at once)
    // once it's attached
    void StepBlend(TimeClock* clock) RETAIL(FUN_00190738);
    void BlendIn(s32 ticks) RETAIL(FUN_00191a38);
};
CHECK_OFFSET(SpringSkeleton, body, 0x20);
CHECK_OFFSET(SpringSkeleton, instance, 0x60);
CHECK_OFFSET(SpringSkeleton, instanceMatrix, 0x70);
CHECK_OFFSET(SpringSkeleton, axis, 0xF0);
CHECK_SIZE(SpringSkeleton::Flags, 4);
CHECK_SIZE(SpringSkeleton, 0x110);

// The graple's rope (GrapleAgent's, 0x120 bytes, retail vtable D_002F35B0): one spring between two points posing
// the graple's joints 0 (at the anchor, turned toward the hook) and 1 (at the hook)
class GrapleRope : public SpringSkeleton
{
public:
    SpringChain* chain;
    u8 unused114[0xC];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00162148);
    void Attach(OgiAnimator* animator) RETAIL(FUN_00162210);
    void Detach(OgiAnimator* animator) RETAIL(FUN_00162270);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_001622d0);
    // A frame: with an anchor the rope laid from it to the hook, else its first point pinned at the hook and simulated under the
    // gravity; then its blend stepped
    void Step(TimeClock* clock, const Vector4* gravity, const Vector4* hook, const Vector4* anchor) RETAIL(FUN_00162190);
};
CHECK_OFFSET(GrapleRope, chain, 0x110);
CHECK_SIZE(GrapleRope, 0x120);

// A skeleton of 24 joints posed by a spring body (retail vtable D_002F35E0); nothing in retail makes one
class Ragdoll : public SpringSkeleton
{
public:
    void Destroy(u32 destroyFlags) RETAIL(FUN_00161dc8);
    void Attach(OgiAnimator* animator) RETAIL(FUN_00161e10);
    void Detach(OgiAnimator* animator) RETAIL(FUN_00161f50);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_0015d930);
};

extern "C"
{
    // The instance a character operates (the character code sets it; the Frogensteins' scripts look along its aim)
    extern struct Reference* g_OperatedInstance RETAIL(D_0030A930);
    // Three rows of a rotation (their indexes; retail passes 0, 1, 2) along a direction, the second as near a world axis (its
    // index) as can be, the third across them; rows 0-2's w made 0 whatever the indexes
    void RowsAlong(Vector4* rows, const Vector4* direction, u32 along, u32 second, u32 across, u32 axis) RETAIL(FUN_0014c538);
    extern const GccVTableEntry g_JointHookVTable[] RETAIL(D_002F0380);
    extern const GccVTableEntry g_ProceduralJointsVTable[] RETAIL(D_002F3550);
    extern const GccVTableEntry g_LookControllerVTable[] RETAIL(PlayableCharacterRelated_Methods);
    extern const GccVTableEntry g_GrapleRopeVTable[] RETAIL(D_002F35B0);
    extern const GccVTableEntry g_RagdollVTable[] RETAIL(D_002F35E0);
    extern const GccVTableEntry g_SpringSkeletonVTable[] RETAIL(D_002F5D48);

    // The graple's rope made for its instance, its hook end's mass the float (1)
    GrapleRope* ConstructGrapleRope(f32 mass, void* memory, InstanceContext* instance) RETAIL_N32(FUN_00162098);

    // Three angles (65536ths of a turn) moved toward targets by at most 1.6 radians a second
    void TurnAnglesToward(f32 seconds, s32* angles, const s32* targets) RETAIL_N32(FUN_00147448);
    // The targets table's qsort comparator (its address is handed to qsort)
    s32 CompareTargets(const TargetEntry* first, const TargetEntry* second) RETAIL(func_00161C28);
    // Whether a time has reached an end less 1/divisor of the clock's last step
    u32 TimeReached(const TimeClock* clock, s32 time, s32 end, s32 divisor) RETAIL(FUN_0015de38);
    // A rotation made its inverse (normalised, its vector part negated)
    void InvertRotation(Vector4* rotation) RETAIL(FUN_0015de60);

    // The character controllers' translation unit's start-up: its static initialisation (initialize 1, priority 0xFFFF) and its
    // global constructor (in the static constructors' table)
    void InitCharacterControllerGlobals(u32 initialize, u32 priority) RETAIL(FUN_0015da18);
    void CharacterControllerGlobalsConstructor() RETAIL(FUN_00162338);

    // The targets table and its count
    extern TargetEntry g_Targets[64] RETAIL(D_0030BBE0);
    extern u32 g_TargetCount RETAIL(D_0030992C);
    // The locks' offsets (the claw's, the gun's four) and the claw's grab offset
    extern Vector4 g_ClawLockOffset RETAIL(D_0030BB80);
    extern Vector4 g_ClawGrabOffset RETAIL(D_0030BB90);
    extern Vector4 g_GunOnFootLockOffset RETAIL(D_0030BBA0);
    extern Vector4 g_GunVehicleLockOffset RETAIL(D_0030BBB0);
    extern Vector4 g_GunMechaLockOffset RETAIL(D_0030BBC0);
    extern Vector4 g_GunArea24LockOffset RETAIL(D_0030BBD0);
}
