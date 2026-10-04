#pragma once

#include "common.h"
#include "game/agentparts.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/instances.h"

struct ChunkData;
struct ChunkLinkData;
struct TimeClock;

// An attack reaching an agent (an event of type 0x1801, retail's ObjectEventCaller, 0x18 bytes, its vtable
// ObjectEventCaller_Methods over D_002F2E98): its kind (the character part's attack kinds)
struct AttackEvent : GameEvent
{
    static constexpr u16 EventId = 0x1801;

    u32 kind;

    // Made with its kind, the reference to the attacker's instance (the callee's, which lets it go) and the kinds of nodes it goes
    // to; destroyed (its base class's destructor apart)
    static AttackEvent* Construct(AttackEvent* event, u32 kind, Reference** attacker, u32 kinds) RETAIL(InitObjectEventCaller);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141c88);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_00141c28);
};
CHECK_OFFSET(AttackEvent, kind, 0x14);
CHECK_SIZE(AttackEvent, 0x18);

// An attack queued for an instance the way the playable character's code does it inline: a reference to the attacker's instance
// taken, the event made with it, its own reference block made and the event queued (the queue lets the handle go)
inline void QueueAttack(InstanceContext* target, u32 kind, InstanceContext* attacker, u32 kinds);

extern "C"
{
    extern const GccVTableEntry g_AttackEventVTable[] RETAIL(ObjectEventCaller_Methods);
    extern const GccVTableEntry g_AttackEventBaseVTable[] RETAIL(D_002F2E98);
}

// The agents of the object types (game/instances.h has their base). The basic agent is the base of every type's (the pickups' and
// pay gates' constructors have its construction inline) and the projectiles' own, given their vtable by the factory. What its
// functions do beyond the base's: 1 its instance's state flags applied, 9 a contact message kept and told its script, 20 an event
// of an attack, 21 its velocity given to its rigid body. Each type's agent overrides what follows its class; 11 is a velocity of
// its own (none but the playable characters'), 22 its frame
class BasicAgent : public Agent
{
public:
    static BasicAgent* Construct(BasicAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00141318);
    // Its properties and part destroyed, then the base's destruction
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141350);
    // Its instance asleep or awake and its flags (collision, visible, shadow, trigger signals) as the state says, its part made
    // again and given the state's damage and target bits
    void ApplyState(u32 unknown) RETAIL(FUN_0013de20);
    u32 Slot3() RETAIL(FUN_0013e8b0);
    void Nothing4() RETAIL(FUN_0013e8b8);
    void Nothing5() RETAIL(FUN_0013e8c0);
    // A physical contact hands the node of kind 1 the sender and the message's strength (while its instance has a physics body),
    // a message with a reaction is kept and tells its script (event 2)
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00141480);
    void Touched(InstanceContext* other, const Vector4* normal) RETAIL(FUN_001413d8);
    // While triggers' signals reach its instance (and it can't always damage the character), an attack that reaches its part
    // tells its script (events 3 to 10 by the kind) and is recorded; one that may damage the character is recorded and makes it
    // hit back when it always may, or when the attacker's part's low byte and the attack's kind are both 3
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_0013e010);
    // The instance's agent sent a damaging contact (0x400, physical) at this one's position
    void HitBack(InstanceContext* target) RETAIL(FUN_0013e1c8);
    // Its object node pushed by the other with no strength
    void Push(InstanceContext* other) RETAIL(FUN_00141538);
    // Callers pass a value and a float after the velocity, which only the playable characters' read
    void Launch(const Vector4* velocity) RETAIL(FUN_00141588);
    // A line of sight through its instance's chunk (LineOfSight in game/collision.h): whether something stopped it
    u32 LineOfSight(const Vector4* from, Vector4* way, u32 mask, InstanceRayHit* hit, u32 instanceMask) RETAIL(FUN_00141290);
};
CHECK_SIZE(BasicAgent, 0x60);

class PickupAgent : public BasicAgent
{
public:
    static PickupAgent* Construct(PickupAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_001419b8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ef10);
    void ApplyState(u32 unknown) RETAIL(FUN_00141998);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0013ef98);
    // Every message kept and told its script, physical or not
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_001419f0);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013efa8);
    void Frame(TimeClock* clock) RETAIL(FUN_0013efa0);
};

// The crates': its bits (bit 0 broken, the state it's in at bits 1-4, the state it's asked to go to at bits 5-8 (3 none: the
// frame takes the asked one), a count of frames bits 9-16 the resting crate waits before it checks its ground again; written as
// the 64 bits from 0x60 in retail, its vertical speed's included), its vertical speed while launched. Its frame (slot 22):
// resting (0) it checks what it stands on and settles (1), launched (2) it falls until it lands
class CrateAgent : public BasicAgent
{
public:
    enum Bits : u32
    {
        Broken = 0x1,
        CurrentShift = 1,
        StateShift = 5,
        StateMask = 0xF,
        WaitShift = 9,
        WaitMask = 0xFF,
    };

    enum States : u32
    {
        StateResting = 0,
        StateSettled = 1,
        StateLaunched = 2,
        StateNone = 3,
    };

    u32 bits;
    f32 launchSpeed;
    u8 unknown68[0x70 - 0x68];

    static CrateAgent* Construct(CrateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_001405a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013eb88);
    u32 Slot3() RETAIL(FUN_0013ec10);
    // Unless its part's value has bit 0 or 1, launched without speed and its script told event 11
    void Slot4() RETAIL(FUN_00140648);
    // Its script told event 11
    void Slot5() RETAIL(FUN_001406b8);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0013ec18);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_001406e0);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013ec20);
    void Launch(const Vector4* velocity) RETAIL(func_00140700);

    // Slot 1: the basic agent's state applied, its speed none, its states made resting; the first time, a shadow node of a 0.5
    // radius shadow
    void ApplyState(u32 unknown) RETAIL(FUN_00140490);
    // Slot 7: by the impulse's direction and size its script told event 5 (pushed down: landed on), 4 (pushed up: hit from
    // below), 3 or 2 (its square over 25: it breaks unless its state has 0x800); whether it's intact
    u32 Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_0013d190);
    // Slot 22: the state asked for taken, the resting crate's ground checked, the launched crate's fall
    void Frame(TimeClock* clock) RETAIL(FUN_0013d058);
    // The launched crate's fall for the frame: whether it still falls (landing on an agent runs its event 5 or 7)
    u32 Fall(TimeClock* clock) RETAIL(FUN_0013caf8);
    // The resting crate's ground check (its part's flag 1 whether it stands on something), the state asked for given
    void CheckGround(TimeClock* clock, u32* asked) RETAIL(FUN_0013ce58);
    // TriggerBalancedCrateFalling: unless resting or in the air (its part's flags 0 and 1), launched without speed (event 11)
    void StartFalling() RETAIL(FUN_001405d8);
};
CHECK_OFFSET(CrateAgent, bits, 0x60);
CHECK_OFFSET(CrateAgent, launchSpeed, 0x64);
CHECK_SIZE(CrateAgent, 0x70);

// The creatures' (its velocity, the default box's lowest corner with w 1 when made). Its functions 23 to 25 are its fall: 24 a fall
// started (the part's falling flag set), 23 a frame of it (landed once the part's on the ground: 25) and 25 landed (the flag
// cleared); its frame (22) starts one once its velocity along the gravity passes its fourth float property
class CreatureAgent : public BasicAgent
{
public:
    Vector4 velocity;

    static CreatureAgent* Construct(CreatureAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140950);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013e8d8);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0013e8f8);
    // A physical contact pushes it (no strength) while it has a physics body, every message is kept and tells its script
    // unless it can always damage the character
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00140ae0);
    // Only through links with bit 18, its velocity taken into the linked chunk's space
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00140ca0);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013e908);
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_00140ac0);
    // Its function 25 when its part's flags have bit 2
    void FallFrame() RETAIL(FUN_00140908);
    void StartFalling(u32 tell) RETAIL(FUN_001409b0);
    void Land(u32 tell) RETAIL(FUN_001409c8);

    // Slot 1: the basic agent's state applied, its part's snapping (the state's bit 18) and hit points (its third int property),
    // snapped to the ground when it snaps; the first time, a shadow node of a shadow of its box's half sizes
    void ApplyState(u32 unknown) RETAIL(FUN_0013d5f8);
    // Slot 7 (the playable characters' too): an impulse over sqrt(200) runs events 3 and 2, over sqrt(10) event 3: yes
    u32 Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_00140b90);
    // Slot 22: its fall's frame, its snapping to the ground, a fall started
    void Frame(TimeClock* clock) RETAIL(FUN_001409e0);
    // Put on the ground below (a cast 7 down from 1 above, raised by its sixth float property; the object node's motion flagged
    // 8): whether it found ground (the part's flag 2)
    u32 SnapToGround() RETAIL(SnapInstanceToGround);
};
CHECK_OFFSET(CreatureAgent, velocity, 0x60);
CHECK_SIZE(CreatureAgent, 0x70);

// What the controls give a playable character every frame (0xF0 bytes into its agent, zeroed when made): the inputs locked (a bit
// each, made all ones when it dies: 0 the turn, 1 and 2 the move, 3 cross, 4 square, 5 circle, 6 the shoulder buttons), the
// stick's turn from the character's facing (-1 to 1, a half turn each way), the way it moves in the world (z and x: the move
// input's, made no longer than 1; a vehicle's the stick turned by the camera; the walk's speed is their length), and how hard
// the buttons are pressed (0 to 1; the shoulder buttons R less L)
struct CharacterButtons
{
    enum Locks : u32
    {
        LockTurn = 0x1,
        LockMoveZ = 0x2,
        LockMoveX = 0x4,
        LockCross = 0x8,
        LockSquare = 0x10,
        LockCircle = 0x20,
        LockShoulders = 0x40,
        LockAll = 0xFFFFFFFF,
    };

    u32 locked;
    f32 turn;
    f32 moveZ;
    f32 moveX;
    f32 cross;
    f32 square;
    f32 circle;
    f32 shoulders;

    void Clear() RETAIL(FUN_0013e910);
};
CHECK_SIZE(CharacterButtons, 0x20);

class CharacterLink;
class GrapleRope;
class LookController;
class ProceduralJoints;
class Vehicle;
struct CharacterBody;
struct ClawController;
struct CrouchController;
struct Gun;
struct JumpController;
struct SpinController;
struct WalkController;

// The playable characters' (retail's PlayableCharacterObjectInstanceContext; game/player.h's PlayerCharacter is the same object).
// Its controllers, made by SetUpCharacter by the character (its first integer property: 0 and 2 Crash, 1 Cortex, 3 Nina, 4 none, 5
// Mecha-Bandicoot): the crouch and slide, Nina's claw, the jump and body slam, the gun, the spin, the walk, the link to the other
// character it's tied to, its procedural joints (dangling parts, legs, the slope's tilt, the squash), the vehicle it rides
// (game/vehicles.h), where its head and arms look and its body's shape. Its vtable's functions besides the creature's: 1 its state
// applied (made new), 7 a collision, 8 bumped into an instance, 9 a contact message (damage), 10 whether it may change chunks, 11
// its velocity, 13 put back on its feet, 15 frozen, 16 unfrozen, 17 a ride's contact sound, 18 footprints, 19 touched an instance,
// 21 launched, 22 its frame, 23 to 25 its fall
class CharacterAgent : public CreatureAgent
{
public:
    // Its state: bits 0-3 the mode the current one took over from, 4-7 its mode, 8-11 what it stands on, 12 it found the ground
    // under its ground point, 13 it doesn't run fall events, 14 dead, 15 its exit points make its box only (the frame does
    // nothing else). Written as the 64 bits from 0x70 in retail, the mode's start included
    enum State : u32
    {
        PreviousModeShift = 0,
        ModeShift = 4,
        ModeMask = 0xF,
        StandingShift = 8,
        StandingMask = 0xF,
        StateGroundFound = 0x1000,
        StateNoFallEvents = 0x2000,
        StateDead = 0x4000,
        StateBoxOnly = 0x8000,
    };

    // Its modes: none, invincible after a hurt (blinking), and hurt
    enum Mode : u32
    {
        ModeNone = 0,
        ModeInvincible = 1,
        ModeApplied = 2,
        ModeHurt = 3,
    };

    // What it stands on: nothing (in the air), the ground (a triangle: groundHit), an instance's hull, a moving instance's hull it
    // rides (its point and matrix kept in the hull's space)
    enum Standing : u32
    {
        StandingNothing = 0,
        StandingGround = 1,
        StandingHull = 2,
        StandingRidden = 3,
    };

    // Its attacks' movement modes (the part's attack kinds as the agents they touch take them: their properties' state bit of the
    // mode makes them stop it)
    enum MoveMode : u32
    {
        MoveNone = 0,
        MoveSlam = 10,
        MoveSlide = 11,
        MoveSpin = 12,
        MoveLinked = 13,
        MoveThrown = 14,
    };

    u32 state;
    u32 modeStart;
    s32 modeTicks;
    // When it was tied to the other character (the clock's time)
    u32 linkTime;
    // Where its fall started (y -100000 none)
    Vector4 fallStart;
    // The chunk it belongs to (none when its state is applied)
    struct ChunkData* homeChunk;
    // A vertical speed added to its velocity's at the next move (then cleared)
    f32 verticalBoost;
    CrouchController* crouch;
    ClawController* claw;
    JumpController* jump;
    Gun* gun;
    SpinController* spin;
    WalkController* walk;
    CharacterLink* link;
    ProceduralJoints* proceduralJoints;
    Vehicle* vehicle;
    LookController* look;
    CharacterBody* body;
    // The hulls its attacks hit with: alone (0xC4, a box 1.98 across and 0.85 high) and tied to the other character (0xC8, 3
    // across and 0.9 high), made by the shared hull builder (D_0030A05C)
    struct CollisionHull* attackHull;
    struct CollisionHull* linkedAttackHull;
    // How hard it's being crushed (up 4 a crush, 2 tied; past 7 it's hurt), down one a frame
    s32 crushCount;
    // The ground under its position (a cast down each frame, 0.02 up), where its checkpoint puts it back
    Vector4 groundPoint;
    // The stick's direction in the world the controls give
    Vector4 moveInput;
    CharacterButtons buttons;
    // The trigger instances it's inside (from the last check), sorted
    ReferenceSet triggers;
    // A sliding hit on a body is once a second, a spin's kick on the pushed body every 0.8 seconds
    f32 slideHitCooldown;
    f32 kickCooldown;
    u8 unknown19C[4];
    // Where the two tied characters are (the leader's)
    Vector4 linkedPoint;
    // The surface of the floor it last stood on
    CollisionSurface* floorSurface;
    u8 unknown1B4[0xC];
    // The triangle it stands on (StandingGround)
    CollisionHit groundHit;
    // The instance whose hull it stands on, the hull's index (-1 none) and the instance's collision's stamp then (the hull is
    // only taken while it's the same), and its point and matrix in the hull's space (StandingRidden)
    Reference* standingOn;
    s32 standingHull;
    u32 standingStamp;
    u8 unknown20C[4];
    Vector4 ridePoint;
    Matrix4x4 rideMatrix;
    // The normal of the ground under it
    Vector4 groundNormal;
    // Its height while it jumps: the state (1 from every take-off, 0 again once it's on the ground: MoveBy), the time in it, the
    // offset its position is above its instance's place and the offset kept to be undone
    s32 heightState;
    f32 heightStateTime;
    f32 heightOffset;
    f32 keptHeightOffset;
    // Its position last frame (GetPlayerPosition's)
    Vector4 lastPosition;
    // The physics body it pushes and how far ahead of it it holds it
    Reference* pushedBody;
    f32 pushedDistance;
    // The instance its probes last hit, and the push they gave (pushed again each frame)
    Reference* probedInstance;
    u8 unknown29C[4];
    Vector4 probePush;
    // The collision's triangles near it (a box around it, Mecha-Bandicoot's larger)
    CollisionCache cache;
    // The turn the hull it rides gave it
    f32 rideTurn;
    u8 unknown304[0xC];

    // Its state's 64 bits (with the mode's start), as retail writes them
    u64& StateBits()
    {
        return *reinterpret_cast<u64*>(&state);
    }

    static CharacterAgent* Construct(CharacterAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00136f70);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f920);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013e998);
    u32 Slot12() RETAIL(FUN_0013e938);
    AgentPart* Part() RETAIL(FUN_0013e960);
    // Invincible (the part's bit 10, its mode 1) for 2 seconds (kind 0) or 8 (kind 1), unless it already is
    void StartInvincibility(u32 kind) RETAIL(FUN_0013f9a0);
    // Its center is its model's joint 1 (not in the world)
    void CollisionCenter(Vector4* center) RETAIL(FUN_0013fd98);

    // Its vtable's functions
    void ApplyState(u32 unknown) RETAIL(FUN_00131ac8);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_001377c8);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00137510);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0013be10);
    void Recover() RETAIL(FUN_0013fa50);
    void Freeze() RETAIL(FUN_0013c088);
    void Unfreeze() RETAIL(FUN_0013c1a8);
    void PlayRideSound(f32 strength) RETAIL_N32(FUN_0013fad8);
    void LeaveFootprints(u32 kind) RETAIL(FUN_001378f8);
    void Touched(InstanceContext* other, const Vector4* normal) RETAIL(FUN_00132018);
    // A launch: pushed back (the jump's event 0x17) by the velocity in an instance's space (its own; none: the world's), the
    // float its strength
    void Launch(f32 strength, const Vector4* velocity, InstanceContext* space) RETAIL_N32(FUN_0013fb60);
    void Frame(TimeClock* clock) RETAIL(FUN_00137ab0);
    void FallFrame() RETAIL(FUN_001329c0);
    void StartFalling(u32 tell) RETAIL(FUN_0013f650);
    void Land(u32 tell) RETAIL(FUN_00132b50);

    // The frame's parts: the controllers' frames (giving the part its attack kinds and bits), the link's, the move and its
    // velocity, the hits, the hurt state, the triggers
    void CrouchFrame(TimeClock* clock, CharacterPart* part) RETAIL(FUN_0012e478);
    void JumpFrame(TimeClock* clock, CharacterPart* part) RETAIL(FUN_0012e638);
    void SpinFrame(TimeClock* clock, CharacterPart* part) RETAIL(FUN_0012e8e0);
    void LinkFrame(TimeClock* clock, CharacterPart* part) RETAIL(FUN_0012e9e8);
    void WalkFrame(TimeClock* clock, CharacterPart* part) RETAIL(FUN_0012f8c0);
    void Move(f32 seconds, CharacterPart* part, u32 controlled) RETAIL_N32(FUN_00130268);
    // The move made (its length capped at 1) with a turn (out: the turn after it, a ridden hull's added)
    void MoveBy(f32 seconds, f32 turn, CharacterPart* part, const Vector4* move, f32* turnOut, u32 mode) RETAIL_N32(FUN_0012fd08);
    void MeasureVelocity(f32 vertical, u32 measured) RETAIL_N32(FUN_0012e328);
    void EaseVelocity(f32 seconds, const Vector4* wanted) RETAIL_N32(FUN_00136168);
    void ApplyGravity(f32 seconds) RETAIL_N32(FUN_00140830);
    void LookFrame(TimeClock* clock) RETAIL(FUN_001308c8);
    void LinkedHits() RETAIL(FUN_00130af0);
    void AttackHits() RETAIL(FUN_00130cb0);
    void TouchHits(TimeClock* clock) RETAIL(FUN_00130e58);
    void Splash() RETAIL(FUN_00131040);
    void HurtFrame(TimeClock* clock) RETAIL(FUN_001315e0);
    void CheckTriggers() RETAIL(FUN_00131290);
    void TellTrigger(struct FollowCamera* camera, InstanceContext* trigger, u32 entered) RETAIL(FUN_00131188);
    void FindGroundPoint() RETAIL(FUN_00134d28);
    void RefreshCache() RETAIL(FUN_001369e8);
    void HullHits() RETAIL(FUN_00136ae0);
    void LiftOntoGround(f32 height, f32 reach) RETAIL_N32(FUN_00136800);
    void MakeBoxOfExitPoints() RETAIL(FUN_00139510);
    // How far its lowest exit point (7 and 6 taken 0.45 along their z axes, 5 and 0) is above its place, less the height
    // offset
    f32 ExitPointsHeight() RETAIL(FUN_0012fa90);

    // Its moves through the collision solver: alone (a lift of its height offset first, within 0.1, then the seconds) and the two
    // tied together (the leader's, in so many steps: MoveBy's 2); the turn in, and out with a ridden hull's added
    void Solve(f32 lift, f32 seconds, const Vector4* move, const f32* turn, f32* turnOut, u32 mode) RETAIL_N32(FUN_00139678);
    void SolveLinked(f32 seconds, const Vector4* move, const f32* turn, f32* turnOut, u32 mode, u32 steps)
        RETAIL_N32(FUN_0013a470);
    // The solver's parts: the probes (their push into the move), riding a hull, what it stands on, the agents not to be pushed
    // out of, the touches, the surfaces' messages, the non-solid contacts, a wall to cling to, crushes
    void Probe(f32 seconds, const Vector4* move, Vector4* out) RETAIL_N32(FUN_00138b00);
    u32 RideMove(Vector4* move, f32* turn) RETAIL(FUN_00135730);
    // What it stands on from the move's ground contact (-1 none) at the point it was placed at, on the ground unless the move
    // rises
    void KeepStanding(s32 contact, const Vector4* point, const Vector4* move, const Vector4* normal, struct ContactSet* contacts)
        RETAIL(FUN_00138700);
    void TellTouched(struct ContactSet* contacts) RETAIL(FUN_001358d0);
    void SendSurfaceMessages(struct ContactSet* contacts) RETAIL(FUN_00135a68);
    void TouchOthers(struct ContactSet* contacts) RETAIL(FUN_00135bc8);
    void ClingToWall(struct ContactSet* contacts) RETAIL(FUN_00135d90);
    u32 CrushedAgents(struct ContactSet* contacts) RETAIL(FUN_001363e8);
    void CheckLinkedCrush(f32 moved, struct ContactSet* contacts, const Vector4* from) RETAIL_N32(FUN_001365e8);
    // While it attacks (a movement mode), the contacts with agents that don't stop that move (their properties' state lacks the
    // mode's bit) or with thrown characters marked not to be pushed out of (the agent isn't read)
    void MarkUnpushable(struct ContactSet* contacts, u32 mode) RETAIL(FUN_00135568);
    void Crushed() RETAIL(FUN_00136740);
    // The instance placed at a position less the height offset; the height offset changed (the float first) and the instance
    // moved by it: whether it moved
    u32 PlaceAt(const Vector4* position) RETAIL(FUN_00135328);
    u32 ChangeHeight(f32 change) RETAIL_N32(FUN_00135438);
    void SetHeightState(s32 state) RETAIL(FUN_0013f3a8);

    // The physics bodies around it after a move (the seconds first): pushed, kicked, pressed and held in front of it
    void PushBodies(f32 seconds, struct ContactSet* contacts) RETAIL_N32(FUN_00134780);
    void SlideIntoBody(struct DynamicBody* body) RETAIL(FUN_001330b0);
    void WalkIntoBody(struct DynamicBody* body, const Vector4* point) RETAIL(FUN_001332d0);
    void StandOnBody(struct DynamicBody* body) RETAIL(FUN_00133900);
    void KickPushedBody() RETAIL(FUN_00133a88);
    void KeepPushedBody() RETAIL(FUN_00133c58);
    void PickPushedBody(InstanceContext** touched, s32 count) RETAIL(FUN_00133f60);
    void HoldPushedBody() RETAIL(FUN_00134288);

    // What touching instances does: the attack events queued for them
    void SendAttack(InstanceContext* other, const Vector4* normal) RETAIL(FUN_00132748);
    u32 KindOf(InstanceContext* other) RETAIL(FUN_00131e40);
    u32 ContactAttack(const Vector4* normal) RETAIL(FUN_00131f20);
    u32 MoveModeOf() RETAIL(FUN_00132fe8);
    void TouchQuery(const InstanceRayHit* query) RETAIL(FUN_001413e0);
    void TouchedNothing(InstanceContext* other) RETAIL(FUN_0013fd50);
    void AttractPickups(TimeClock* clock, const InstanceRayHit* query) RETAIL(FUN_0013f4b0);

    // Hurt (knocked back from an instance), pushed back (PushCharacterBack: the gravity of the launch it queues on the jump, the
    // push, the event the jump runs for it, the instance whose space the push is in: its own, none the world's)
    void KnockBack(InstanceContext* from) RETAIL(FUN_001371f0);
    void PushBack(f32 gravity, const Vector4* push, u32 event, InstanceContext* space) RETAIL_N32(FUN_00137010);

    // Its controllers: made, let go of, made again; a vehicle taken and left
    void SetUp(u32 made) RETAIL(FUN_00134e88);
    void TearDown(u32 destroyed) RETAIL(FUN_00135110);
    void Remake() RETAIL(FUN_0013f800);
    // SetPlayerVehicle (player.h declares it too): unlinked, the vehicle left, the new one of the kind made with the other agent
    // and a value (the hoverboard's controls), taken; with kinds 1 and 3 the other character rides along (kind 8)
    void SetVehicle(u32 kind, Agent* other, u32 value) RETAIL(SetPlayerVehicle);
    u32 TakeVehicle(Vehicle* vehicle) RETAIL(FUN_0013b668);
    void LeaveVehicle(u32 replaced) RETAIL(FUN_0013bc60);
    // Tied to another character and untied (both)
    u32 Link(CharacterAgent* other) RETAIL(FUN_00132ca8);
    void Unlink() RETAIL(FUN_00132dc0);
    // Set back (when the game controller holds and releases the player)
    void Reset() RETAIL(FUN_001317f8);

    // Small queries and settings
    void Position(Vector4* position) RETAIL(GetPlayerPosition);
    void PlacePosition(Vector4* position) RETAIL(FUN_0013f8b0);
    f32 HeightOffset() RETAIL(FUN_0013f910);
    u32 Invincible() RETAIL(FUN_0013fa38);
    u32 Linked() RETAIL(FUN_0013fb80);
    u32 HandPoints(Vector4* own, Vector4* other) RETAIL(FUN_0013fbc0);
    u32 Controlled() RETAIL(FUN_0013fc88);
    u32 IsPlayer() RETAIL(FUN_0013f488);
    void SplashPoint(Vector4* point, f32* radius) RETAIL(FUN_0013f430);
    u32 MovingVelocity(Vector4* velocity) RETAIL(FUN_0013fdc0);
    void SetFloorSurface(CollisionSurface* surface) RETAIL(FUN_0013f5c8);
    CollisionSurface* StandingSurface() RETAIL(FUN_00135650);
    void LetGoOfStanding() RETAIL(FUN_00132f20);
    u32 StandsOn(InstanceContext* instance) RETAIL(FUN_0013f748);
    void ResetFall() RETAIL(FUN_0013f790);
    void SendSurfaceMessage(CollisionSurface* surface) RETAIL(SendSurfaceContactMessage);
    u32 FitsAt(u32 kind, u32 push, const Vector4* normal, const Vector4* position) RETAIL(FUN_001380f0);
    u32 FootNormal(Vector4* normal) RETAIL(FUN_00138348);
    // The walk's top speed (1 without a walk controller)
    f32 WalkTopSpeed() RETAIL(FUN_0013fd18);
    void DrawOverlay() RETAIL(FUN_0013fe28);
};
CHECK_OFFSET(CharacterAgent, state, 0x70);
CHECK_OFFSET(CharacterAgent, modeTicks, 0x78);
CHECK_OFFSET(CharacterAgent, linkTime, 0x7C);
CHECK_OFFSET(CharacterAgent, fallStart, 0x80);
CHECK_OFFSET(CharacterAgent, homeChunk, 0x90);
CHECK_OFFSET(CharacterAgent, verticalBoost, 0x94);
CHECK_OFFSET(CharacterAgent, crouch, 0x98);
CHECK_OFFSET(CharacterAgent, claw, 0x9C);
CHECK_OFFSET(CharacterAgent, jump, 0xA0);
CHECK_OFFSET(CharacterAgent, gun, 0xA4);
CHECK_OFFSET(CharacterAgent, spin, 0xA8);
CHECK_OFFSET(CharacterAgent, walk, 0xAC);
CHECK_OFFSET(CharacterAgent, link, 0xB0);
CHECK_OFFSET(CharacterAgent, proceduralJoints, 0xB4);
CHECK_OFFSET(CharacterAgent, vehicle, 0xB8);
CHECK_OFFSET(CharacterAgent, look, 0xBC);
CHECK_OFFSET(CharacterAgent, body, 0xC0);
CHECK_OFFSET(CharacterAgent, attackHull, 0xC4);
CHECK_OFFSET(CharacterAgent, linkedAttackHull, 0xC8);
CHECK_OFFSET(CharacterAgent, crushCount, 0xCC);
CHECK_OFFSET(CharacterAgent, groundPoint, 0xD0);
CHECK_OFFSET(CharacterAgent, moveInput, 0xE0);
CHECK_OFFSET(CharacterAgent, buttons, 0xF0);
CHECK_OFFSET(CharacterAgent, triggers, 0x110);
CHECK_OFFSET(CharacterAgent, slideHitCooldown, 0x194);
CHECK_OFFSET(CharacterAgent, kickCooldown, 0x198);
CHECK_OFFSET(CharacterAgent, linkedPoint, 0x1A0);
CHECK_OFFSET(CharacterAgent, floorSurface, 0x1B0);
CHECK_OFFSET(CharacterAgent, groundHit, 0x1C0);
CHECK_OFFSET(CharacterAgent, standingOn, 0x200);
CHECK_OFFSET(CharacterAgent, standingHull, 0x204);
CHECK_OFFSET(CharacterAgent, standingStamp, 0x208);
CHECK_OFFSET(CharacterAgent, ridePoint, 0x210);
CHECK_OFFSET(CharacterAgent, rideMatrix, 0x220);
CHECK_OFFSET(CharacterAgent, groundNormal, 0x260);
CHECK_OFFSET(CharacterAgent, heightState, 0x270);
CHECK_OFFSET(CharacterAgent, heightStateTime, 0x274);
CHECK_OFFSET(CharacterAgent, heightOffset, 0x278);
CHECK_OFFSET(CharacterAgent, keptHeightOffset, 0x27C);
CHECK_OFFSET(CharacterAgent, lastPosition, 0x280);
CHECK_OFFSET(CharacterAgent, pushedBody, 0x290);
CHECK_OFFSET(CharacterAgent, pushedDistance, 0x294);
CHECK_OFFSET(CharacterAgent, probedInstance, 0x298);
CHECK_OFFSET(CharacterAgent, probePush, 0x2A0);
CHECK_OFFSET(CharacterAgent, cache, 0x2B0);
CHECK_OFFSET(CharacterAgent, rideTurn, 0x300);
CHECK_SIZE(CharacterAgent, 0x310);

class GenericObjectAgent : public BasicAgent
{
public:
    static GenericObjectAgent* Construct(GenericObjectAgent* agent, InstanceCreator* creator, PropertyHolder* holder,
                                         AgentPart* part) RETAIL(FUN_00141060);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ed40);
    // The basic agent's state applied; the first time, a shadow node (kind 0xA) made with a plain shadow of its box's half sizes
    void ApplyState(u32 unknown) RETAIL(FUN_0013dc68);
    // A collision tells its script (event 3, the other its sender) when attacks of kind 3 reach it: whether it did
    u32 Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_001410c0);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0013edc8);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013edd0);
    void Attacked(const AttackEvent* event, InstanceContext* sender) RETAIL(FUN_001410a0);
    void Frame(TimeClock* clock) RETAIL(FUN_00141098);
};

class GrabbableAgent : public BasicAgent
{
public:
    static GrabbableAgent* Construct(GrabbableAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140338);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0013ea50);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013ead8);
    void Frame(TimeClock* clock) RETAIL(FUN_00140370);
};

class PayGateAgent : public BasicAgent
{
public:
    static PayGateAgent* Construct(PayGateAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00141800);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00141bf8);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_00141c20);
    void Frame(TimeClock* clock) RETAIL(FUN_00141c18);
};

// The graples' (an object of its own, destroyed with it; its frame runs while its instance is visible): its rope, where its hook
// goes and where it hangs from (Nina's claw sets them)
class GrapleAgent : public BasicAgent
{
public:
    GrapleRope* rope;
    u8 unknown64[0xC];
    Vector4 target;
    Vector4 anchor;

    static GrapleAgent* Construct(GrapleAgent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_00140e78);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00140eb0);
    // Its object made the first time instead (its state isn't applied)
    void ApplyState(u32 unknown) RETAIL(FUN_00140e20);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013ecb0);
    void Frame(TimeClock* clock) RETAIL(FUN_00140f30);
    // Its frame while visible (GrapleFrame): the hook moved from the anchor toward the target through the collision (half a unit
    // short), the rope stepped and the instance's box made around the rope's ends
    void Swing(TimeClock* clock) RETAIL(FUN_0013d818);
    void SetEnds(const Vector4* anchor, const Vector4* target) RETAIL(FUN_00140f18);
};
CHECK_OFFSET(GrapleAgent, target, 0x70);
CHECK_OFFSET(GrapleAgent, anchor, 0x80);
CHECK_SIZE(GrapleAgent, 0x90);

// The projectiles' (the basic agent given its vtable by the factory): its instance flagged 0x20000 when its state is applied
class ProjectileAgent : public BasicAgent
{
public:
    static constexpr u32 InstanceFlag = 0x20000;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0013f090);
    void ApplyState(u32 unknown) RETAIL(FUN_00141b98);
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0013f120);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_0013f128);
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0013f138);
    void Frame(TimeClock* clock) RETAIL(FUN_0013f130);
};

extern "C"
{
    extern const GccVTableEntry g_AgentVTable[] RETAIL(ObjectInstanceContextBase_Methods);
    extern const GccVTableEntry g_BasicAgentVTable[] RETAIL(D_002F2BA8);
    extern const GccVTableEntry g_PickupAgentVTable[] RETAIL(D_002F2280);
    extern const GccVTableEntry g_CrateAgentVTable[] RETAIL(D_002F2700);
    extern const GccVTableEntry g_CreatureAgentVTable[] RETAIL(D_002F2AD0);
    extern const GccVTableEntry g_CharacterAgentVTable[] RETAIL(PlayableCharacterObjectInstanceContext_Methods);
    extern const GccVTableEntry g_GenericObjectAgentVTable[] RETAIL(D_002F2460);
    extern const GccVTableEntry g_GrabbableAgentVTable[] RETAIL(D_002F2850);
    extern const GccVTableEntry g_PayGateAgentVTable[] RETAIL(D_002F2EB0);
    extern const GccVTableEntry g_GrapleAgentVTable[] RETAIL(ObjectInstanceContextType7_Methods);
    extern const GccVTableEntry g_ProjectileAgentVTable[] RETAIL(ObjectInstanceContextType8_Methods);

    // The character agent of an instance (none: nullptr)
    CharacterAgent* CharacterAgentOf(InstanceContext* instance) RETAIL(FUN_0013fce8);

    // The agents' translation unit's start-up: its static initialisation (initialize 1, priority 0xFFFF) and its entry in the
    // static constructors' table
    void InitAgentsModule(u32 initialize, u32 priority) RETAIL(FUN_0013e290);
    void ConstructAgentsModule() RETAIL(FUN_00141ce8);
    // Its globals (the start-up sets them): KnockBack's angles (-135, 135, -45 and 45 degrees, 65536ths), the trigger and camera
    // node kinds (0x180), down (0, -1, 0, 1) and the graple rope's gravity (0, -25, 0, 1). The header's (0, 1, 0, 1) D_0030BAD0,
    // 0.01 D_0030A4E8 and D_0030A4EC, colour D_0030A4F0, 0 D_0030A4E0 and 45 degrees D_0030A4F8 are read by nothing
    extern s32 g_AgentAngleMinus135 RETAIL(D_0030A500);
    extern s32 g_AgentAngle135 RETAIL(D_0030A508);
    extern s32 g_AgentAngleMinus45 RETAIL(D_0030A510);
    extern s32 g_AgentAngle45 RETAIL(D_0030A518);
    extern u32 g_TriggerNodeKinds RETAIL(D_0030A520);
    extern Vector4 g_AgentDown RETAIL(D_0030BAE0);
    extern Vector4 g_GrapleGravity RETAIL(D_0030BAF0);
}

inline void QueueAttack(InstanceContext* target, u32 kind, InstanceContext* attacker, u32 kinds)
{
    Reference* reference = attacker != nullptr ? AddReference(attacker) : nullptr;
    auto* event = AttackEvent::Construct(static_cast<AttackEvent*>(MemoryAllocate(sizeof(AttackEvent))), kind, &reference, kinds);
    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
    QueueEvent(target, &handle);
}
