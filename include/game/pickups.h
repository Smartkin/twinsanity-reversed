#pragma once

#include "abi.h"
#include "common.h"
#include "game/objectnode.h"

struct ChunkEntry;
struct CodeModel;
struct ScriptCommand;
struct SphereBody;

// The custom pickups and projectiles the object scripts set up through code models, one per slot (game/pickups.cpp and
// game/projectiles.cpp; their nodes' function 25 gives the slot, SetCustomPickup and SetCustomProjectile set them up)

constexpr u32 CustomPickupSlots = 5;
constexpr u32 CustomProjectileSlots = 9;

// A custom pickup's flags, bits 0-10 set by SetCustomPickup (the command's word has the same bits): only its spinning while it's
// idle is read (the development tools' parser puts hit points in bits 8 and 9 and marks a radius or a pull given in bit 10)
union CustomPickupFlags
{
    // The bits SetCustomPickup sets, which a new custom pickup clears
    static constexpr u32 CommandBits = 0x7FF;

    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 unused1 : 1;
        u32 unused2 : 1;
        u32 unused3 : 1;
        u32 unused4 : 1;
        u32 unused5 : 1;
        u32 spins : 1;
        u32 unused7 : 1;
        u32 unused8 : 2;
        u32 unused10 : 1;
        u32 unused11 : 21;
    };
};
CHECK_SIZE(CustomPickupFlags, 4);

// What a pickup code model sets up for its slot (0x18 bytes): its radius (and its square), the pull of its focus on it, the
// speed it flees its focus at and its flags
struct CustomPickup
{
    f32 radius;
    f32 radiusSquared;
    f32 pull;
    f32 fleeSpeed;
    CustomPickupFlags flags;
    u32 unused14;
};
CHECK_SIZE(CustomPickup, 0x18);

// A custom projectile's flags (SetCustomProjectile's settings: game/commands.h's CustomProjectileSettings has them in other
// bits): it falls (by its gravity), stops homing in once it flew its homing time, homes in on its target, and is the player's
// shot (aimed with the player's targeting when launched, it doesn't hit characters)
union CustomProjectileFlags
{
    // The bits a new custom projectile clears
    static constexpr u32 ClearedBits = 0x3FF;

    u32 value;
    struct
    {
        u32 unused0 : 6;
        u32 falls : 1;
        u32 homesForATime : 1;
        u32 homes : 1;
        u32 playersShot : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(CustomProjectileFlags, 4);

// What a projectile code model sets up for its slot (0x24 bytes): a scale of the way on a hit that nothing uses, its speed, how
// long it homes in on its target, how much it turns toward it (across times the scale, up and down as it is), the gravity it
// falls with and its flags
struct CustomProjectile
{
    f32 hitScale;
    f32 speed;
    u32 unused08;
    f32 homingTime;
    f32 turn;
    f32 sideTurnScale;
    f32 gravity;
    CustomProjectileFlags flags;
    u32 unused20;
};
CHECK_SIZE(CustomProjectile, 0x24);

// The command each pickup slot's code model runs on a node of the slot when it's set up, then each projectile slot's custom
// projectile (retail reaches those past the commands' label)
struct CustomCommandSlots
{
    ScriptCommand* pickupCommands[CustomPickupSlots + 1];
    CustomProjectile* projectiles[CustomProjectileSlots + 1];
};
CHECK_SIZE(CustomCommandSlots, 0x40);

// A custom pickup's object node's bits: its state, the one before it, its custom slot, and bit 19, set from outside its code: it
// isn't made idle while it is
union PickupNodeBits
{
    u32 value;
    struct
    {
        u32 state : 5;
        u32 previous : 5;
        u32 customSlot : 5;
        u32 unused15 : 4;
        u32 keepsState : 1;
        u32 unused20 : 12;
    };
};
CHECK_SIZE(PickupNodeBits, 4);

// A custom pickup's object node (0xD0 bytes, vtable InstanceNodeType1_Methods over the prototype's): the prototype's part first
// (ObjectNodeBase's first 0xA0 bytes, whose functions of the prototype work on it), then its bits (bit fields of the 64 bits
// from 0xA0 in retail, its trails' pointer in their upper half), its particle trails, the physics body it was launched as, its
// place in the spin's frames, its velocity and the clock time its waits count from. Its states are its code model's: entering 7
// runs the model's pack of it
struct PickupObjectNode : GameNode
{
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

    // What its prototype's flags' bits 2 and 8 mean to it (ObjectNodeFlags' falls and keepsParticles to the other nodes): it has
    // a physics body, it flies on to its focus's own focus once it reaches it (until that's the player's instance)
    enum Flags : u32
    {
        FlagLaunched = 0x4,
        FlagHandsOn = 0x100,
    };


    u8 prototype[0xA0 - sizeof(GameNode)];
    PickupNodeBits bits;
    ParticleTrails* trails;
    SphereBody* body;
    s32 spinPhase;
    Vector4 velocity;
    u32 waitStart;
    u8 unusedC4[0xD0 - 0xC4];

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
    // a gravity), 41 its code model's kind, 42 a runner's packet ended (it has no runners), 43 a packet started (nothing)
    void HandleEvent(Reference** handle) RETAIL(FUN_0010a298);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011e570);
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0011e708);
    void Restart(TimeClock* clock) RETAIL(FUN_0011e750);
    u32 Update(TimeClock* clock) RETAIL(FUN_00109520);
    void Sleep() RETAIL(FUN_0011e5b8);
    void SetAgent(Agent* agent) RETAIL(FUN_0011e600);
    void Reset() RETAIL(FUN_0011e7a8);
    u32 UnusedTakesPackets() RETAIL(FUN_0011e3d0);
    u32 TakesPackets() RETAIL(FUN_0011e3d8);
    u32 UnpinCollision() RETAIL(FUN_0011e658);
    u32 PinCollision() RETAIL(FUN_0011e698);
    u32 AddParticleTrail(const void* arguments) RETAIL(FUN_0011ea18);
    void DestroyParticleTrails() RETAIL(FUN_0011ea70);
    u32 CustomSlot() RETAIL(FUN_0011e3f0);
    void Launch(f32 gravity, const Vector4* launchVelocity) RETAIL_N32(FUN_00109290);
    u32 CodeModelKind() RETAIL(FUN_0011e408);
    u32 PacketEnded() RETAIL(FUN_0011e3c8);
    void PacketStarted() RETAIL(FUN_0011e3e0);

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
        return bits.state;
    }

    // Its custom slot written (5 bits of a value)
    void SetCustomSlot(u32 slot)
    {
        bits.customSlot = slot;
    }

    // The state it's in made the one before, the new one written
    void Enter(u32 state)
    {
        bits.previous = bits.state;
        bits.state = state;
    }

    // Idle, nothing before
    void MakeIdle()
    {
        bits.state = StateIdle;
        bits.previous = 0;
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

extern "C"
{
    // Each pickup slot's custom pickup, and the slots' commands and custom projectiles
    extern CustomPickup* g_CustomPickups[CustomPickupSlots + 1] RETAIL(D_003D1E20);
    extern CustomCommandSlots g_CustomCommandSlots RETAIL(G_CodeModelCommand);

    // The slots made empty (the static constructors), and their pickups or projectiles, packs and commands destroyed first
    void ResetCustomPickups() RETAIL(FUN_0011eaa8);
    void ClearCustomPickups() RETAIL(FUN_0010a4b0);
    void ResetCustomProjectiles() RETAIL(FUN_0011ef50);
    void ClearCustomProjectiles() RETAIL(FUN_0010c240);
    // A pickup code model's slot set up: its packs and command taken, a new custom pickup (all ones), the command run on a node
    // of the slot made for it; and a projectile code model's (a new custom projectile of a hit scale of 10, the rest none)
    void SetUpCustomPickup(CodeModel* model) RETAIL(FUN_0010a380);
    void SetUpCustomProjectile(CodeModel* model) RETAIL(FUN_0010c3b8);
    // The idle pickups' spin's matrices made (the static constructor; game/gamecontroller.h's StepPickupSpin steps it)
    void InitPickupSpin() RETAIL(FUN_00108d70);
}
