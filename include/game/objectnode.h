#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/string.h"

class LayoutPath;
class NodeController;
class PropertyHolder;
struct AiPath;
struct AiPosition;
class Agent;
struct BehaviourRunner;
struct CollisionSurface;
struct ControlPacket;
struct GameObject;
struct LayoutPosition;
struct ObjectPlace;
struct Reference;
struct DynamicBody;
struct RigidBody;
struct Route;
struct ScriptStarter;
struct TimeClock;

// The script designators an object node answers (the AgentLab tool's; those below 0xDE are the starter's receivers): what its
// instance's ID entry links, the route's next step (the focus position's command only: a route is kept from its end, the next
// step's index is the current one's less one), its own instance, the player, the runner's originator, the head tracking's
// target, the stored position, AgentRef2 and AgentRef1, the route's previous and current step, the focus instance and position,
// the next and the current key, and none (the receivers' too)
enum Designator : u32
{
    // The first that isn't a receiver's index
    FirstDesignator = 0xDE,
    DesignatesLinkedById = 0xDF,
    DesignatesNextStep = 0xEF,
    DesignatesItself = 0xF0,
    DesignatesPlayer = 0xF2,
    DesignatesOriginator = 0xF4,
    DesignatesHeadTarget = 0xF5,
    DesignatesStoredPosition = 0xF6,
    DesignatesAgentRef2 = 0xF7,
    DesignatesAgentRef1 = 0xF8,
    DesignatesPreviousStep = 0xF9,
    DesignatesCurrentStep = 0xFA,
    DesignatesFocus = 0xFB,
    DesignatesFocusPosition = 0xFC,
    DesignatesNextKey = 0xFD,
    DesignatesCurrentKey = 0xFE,
    DesignatesNone = 0xFF,
};

// What the focus commands give an instance or a position to (the object node's slot 34, ForgetDesignator, forgets them by
// these): the focus, AgentRef1, AgentRef2, the stored position
enum DesignatorSlot : u32
{
    SlotFocus = 0,
    SlotAgentRef1 = 1,
    SlotAgentRef2 = 2,
    SlotStoredPosition = 3,
};

// The object instances' nodes (kind 1): the agent's node the behaviour runners drive, and its parts that move the instance along
// the control packets (the AgentLab tool's names for what a packet does are in game/agentlab.h)

// A motion's bits (the retail code reads and writes them as the 64 bits from 8 on, the speed's included): the translation is
// done, the rotation is; bit 3 is what a snap to the ground sets, and the resets clear bits 2-4 too (nothing reads them)
union MotionStateBits
{
    u32 value;
    struct
    {
        u32 translationDone : 1;
        u32 rotationDone : 1;
        u32 unused2 : 1;
        u32 unused3 : 1;
        u32 unused4 : 1;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(MotionStateBits, 4);

// How a control packet's motion goes (made the first time, kept by the node, 0x60 bytes): the time it takes and its inverse, its
// bits, the speed, the speed of a chase, the velocity (the one it starts from while it accelerates), the speed of a turn and the
// rotation's change per second
struct MotionState
{
    f32 duration;
    f32 inverseDuration;
    MotionStateBits bits;
    f32 speed;
    f32 chaseSpeed;
    u8 unused14[0xC];
    Vector4 velocity;
    Vector4 startVelocity;
    f32 turnSpeed;
    u8 unused44[0xC];
    Vector4 turn;

    static MotionState* Construct(MotionState* state) RETAIL(FUN_0020ee90);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020eeb8);
    void Reset() RETAIL(FUN_0020eee0);
    // No velocity nor speed
    void Stop() RETAIL(FUN_0020f0a0);
};
CHECK_OFFSET(MotionState, velocity, 0x20);
CHECK_OFFSET(MotionState, turnSpeed, 0x40);
CHECK_SIZE(MotionState, 0x60);

// A translation (0x60 bytes): where it starts and where it goes, a direction (the facing of a translation, a projectile's
// velocity), the sideways wander a followed target gets, the motion's state and the distance
struct Translator
{
    Vector4 start;
    Vector4 target;
    Vector4 direction;
    u8 unused30[0x10];
    Vector4 wander;
    MotionState* motion;
    f32 distance;
    u8 unused58[8];

    static Translator* Construct(Translator* translator) RETAIL(FUN_0020edd0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020edf8);
    void Reset() RETAIL(FUN_0020ee20);
};
CHECK_OFFSET(Translator, motion, 0x50);
CHECK_SIZE(Translator, 0x60);

// A rotation (0x40 bytes): the motion's state, where it starts and where it goes, and a vector its resets clear that nothing
// reads
struct Rotator
{
    MotionState* motion;
    u8 unused04[0xC];
    Vector4 start;
    Vector4 target;
    Vector4 unused30;

    static Rotator* Construct(Rotator* rotator) RETAIL(FUN_0020f9b8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020f9e0);
    void Reset() RETAIL(FUN_0020fa08);
};
CHECK_SIZE(Rotator, 0x40);

// The physics' bits (read and written as 64 bits in retail): a spring's deceleration is negative (1), and a kind nothing reads
// (2-5: its resets make it 1, a spring 0, a chase 3)
union PhysicsBits
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 negativeDeceleration : 1;
        u32 unused2 : 4;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(PhysicsBits, 4);

// The physics of the motions that aren't straight (0x60 bytes): a velocity, four parameters (a spring's power and damping; a
// chase's duration, power, damping and bounce), a vector its resets clear that nothing reads, the motion's state and its bits
struct Physics
{
    // The kinds its bits keep
    enum Kind : u32
    {
        KindSpring = 0,
        KindMade = 1,
        KindChase = 3,
    };

    u8 unused00[0x20];
    Vector4 velocity;
    Vector4 unused30;
    f32 parameters[4];
    MotionState* motion;
    u32 unused54;
    PhysicsBits bits;
    u32 unused5C;

    static Physics* Construct(Physics* physics) RETAIL(FUN_0020f878);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020f8a0);
    void Reset() RETAIL(FUN_0020f8c8);
};
CHECK_OFFSET(Physics, parameters, 0x40);
CHECK_OFFSET(Physics, bits, 0x58);
CHECK_SIZE(Physics, 0x60);

// The spaces of a script's positions and moves: the world's, the instance's start, its own place, the target's (the receiver's
// instance or the designated one), facing from the instance to what its packet tracks, its rigid body's object, none, its
// stored place
enum PositionSpace : u32
{
    SpaceWorld = 0,
    SpaceStart = 1,
    SpaceOwn = 2,
    SpaceTarget = 3,
    SpaceTracked = 4,
    SpaceRigidBody = 5,
    SpaceNone = 6,
    SpaceStored = 7,
};

// A motion block's motion bits: the space its cycles move the instance in (PositionSpace), how the cycle about each axis
// goes (MotionBlock::Cycle), its cycles turn the instance, they fade out and end after the duration, a body resting far from the
// player is put back, the instance faces the way it moves, the cycles about each axis start at a random phase, their signs
// (MotionBlock::Sign), how it rolls along the way it moves (MotionBlock::Roll) and the block's kind (MotionBlock::Kind)
union MotionBlockMotion
{
    static constexpr u32 CycleShift = 3;
    static constexpr u32 CycleMask = 0x7;
    static constexpr u32 SignShift = 20;
    static constexpr u32 SignMask = 0x3;
    static constexpr u32 RandomPhaseXMask = 0x10000;

    u32 value;
    struct
    {
        u32 space : 3;
        u32 cycleX : 3;
        u32 cycleY : 3;
        u32 cycleZ : 3;
        u32 turns : 1;
        u32 fades : 1;
        u32 putsBackStuck : 1;
        u32 facesMove : 1;
        u32 randomPhaseX : 1;
        u32 randomPhaseY : 1;
        u32 randomPhaseZ : 1;
        u32 unused19 : 1;
        u32 signX : 2;
        u32 signY : 2;
        u32 signZ : 2;
        u32 roll : 2;
        u32 kind : 3;
        u32 unused31 : 1;
    };

    // The cycle and the sign about an axis (0 x, 1 y, 2 z), and whether its cycle starts at a random phase
    u32 CycleOf(u32 axis) const
    {
        return value >> (CycleShift + 3 * axis) & CycleMask;
    }

    u32 SignOf(u32 axis) const
    {
        return value >> (SignShift + 2 * axis) & SignMask;
    }

    bool StartsAtRandom(u32 axis) const
    {
        return (value & RandomPhaseXMask << axis) != 0;
    }
};
CHECK_SIZE(MotionBlockMotion, 4);

// Where a motion block's cycles take their angles from (MotionBlockFlags::cycleSource, TrajectoryBits::cycleSource): their own
// steps, the focus's trajectory, that of the first instance attached to the node's
enum CycleSource : u32
{
    OwnCycles = 0,
    FocusCycles = 1,
    AttachedCycles = 2,
};

// A motion block's flags: where the cycles' angles come from (0 their own steps, 1 the focus's trajectory, 2 that of the first
// instance attached to the node's), the trajectory cycles about x, y and z, a spin goes by the move's size alone, the cycles
// grow in over the duration, a body has a centre of mass, the instance faces the instance its packet tracks, an instance
// launched with it is set upright (keeping its facing), a grabber holds what it touches or holds on to AgentRef1 when it touches
// it, things stick to it (handed on to the block followed next), a touch or a push makes the node follow it, the trajectory
// controller following it asks for its frame while the node isn't updated, and what touches it is sent its touch message
union MotionBlockFlags
{
    static constexpr u32 CyclesAboutXMask = 0x8;

    u32 value;
    struct
    {
        u32 cycleSource : 3;
        u32 cyclesAboutX : 1;
        u32 cyclesAboutY : 1;
        u32 cyclesAboutZ : 1;
        u32 spinsBySize : 1;
        u32 growsIn : 1;
        u32 hasCenterOfMass : 1;
        u32 facesTracked : 1;
        u32 uprightsLaunched : 1;
        u32 holdsTouched : 1;
        u32 holdsAgentRef1 : 1;
        u32 sticky : 1;
        u32 followedWhenTouched : 1;
        u32 keepsStepping : 1;
        u32 sendsTouches : 1;
        u32 unused17 : 15;
    };

    // Whether the trajectory cycles about an axis (0 x, 1 y, 2 z)
    bool CyclesAbout(u32 axis) const
    {
        return (value & CyclesAboutXMask << axis) != 0;
    }
};
CHECK_SIZE(MotionBlockFlags, 4);

// A body's bits: hinges about its own x, y and z (all three: it doesn't turn), no collisions, the axis it's slowed along (1 x, 2
// y, 3 z), its constraint (MotionBlock::Constraint), its substeps, the volume controllers push it, it's never put back when it
// rests, its physics body's flags 0x20 and 0x40 (either lets the playable characters push it), its hinges' turn is limited and
// it floats by the stiffness. Bit 10 is set with the turn toward the focus and never read
union MotionBlockBody
{
    static constexpr u32 HingesMask = 0x7;

    u32 value;
    struct
    {
        u32 hingeX : 1;
        u32 hingeY : 1;
        u32 hingeZ : 1;
        u32 noCollisions : 1;
        u32 slowedAxis : 2;
        u32 constraint : 4;
        u32 unused10 : 1;
        u32 substeps : 3;
        u32 pushedByVolumes : 1;
        u32 neverPutBack : 1;
        u32 pushable20 : 1;
        u32 pushable40 : 1;
        u32 limitsTurn : 1;
        u32 floats : 1;
        u32 unused20 : 12;
    };
};
CHECK_SIZE(MotionBlockBody, 4);

// How a cover search looks (1: among the positions in a box around the instance)
union CoverSearch
{
    u16 value;
    struct
    {
        u16 kind : 5;
        u16 unused5 : 11;
    };
};
CHECK_SIZE(CoverSearch, 2);

// A block of motion an object node can follow (its cycles, sticking and resets are game/objectnodemotion.cpp's, what the
// trajectory controller does with it game/trajectory.cpp's): what it does depends on its kind: 0 cycles move or turn the
// instance, 1-3 a rigid body of the node's own carries it (1 one the block's values shape and springs hold at its place, 2 a
// ball, 3 one that grabs what it touches), 4 it looks for cover from the player among the AI positions around. The first 0x3C
// bytes hold each kind's own values. Then the strengths kind 3 grabs with (what it touches, AgentRef1), the scale of the knocks
// its body's collisions hand the nodes it hits, the turn toward the focus a body gets, a body's constraint (an axis or a normal,
// turned by the instance's place) and its bits, its mass, the limit of its hinges' turn, its motion bits and its flags
struct MotionBlock
{
    // A cycle about an axis: 1 a sine, 2 and 3 a square wave, 4 random, 5 the angle itself (radians, turning amplitude times
    // faster), else none; and its sign: 1 negative, 2 positive, else as it comes
    enum Cycle : u32
    {
        CycleSine = 1,
        CycleSquare = 2,
        CycleSquareToo = 3,
        CycleRandom = 4,
        CycleAngle = 5,
    };

    enum Sign : u32
    {
        SignNegative = 1,
        SignPositive = 2,
    };

    // The kinds, the spaces the cycles move the instance in (its start's turn, its own axes, facing the instance its packet
    // tracks from where it's held, the stored place's axes; the world's for the others) and how it rolls along its move (the
    // turning cycles' spaces: the instance's parent's attachment, its start's, its own, the stored place's)
    enum Kind : u32
    {
        KindCycles = 0,
        KindBody = 1,
        KindBall = 2,
        KindGrabber = 3,
        KindCover = 4,
    };

    enum Roll : u32
    {
        RollFaces = 0,
        RollY = 1,
        RollX = 2,
        RollSpin = 3,
    };

    // A body's constraints: fixed where it is, kept on a line along the axis, on a plane of the normal
    enum Constraint : u32
    {
        ConstraintFixed = 1,
        ConstraintLine = 2,
        ConstraintPlane = 3,
    };

    // What SetMotionBlockConstraint takes besides a Constraint: the hinges about the body's own x, y and z, and a constraint kind
    // nothing handles
    enum GivenKind : u32
    {
        GivenHingeX = 4,
        GivenHingeY = 5,
        GivenHingeZ = 6,
        GivenOddConstraint = 7,
    };

    // The search a cover block makes among the positions in a box around the instance, and the mover that's the node's own
    // instance
    static constexpr u16 SearchInBox = 1;
    static constexpr u8 MoverOwnInstance = 0xFF;

    union
    {
        // Every kind's own values as they're kept (what the resets clear)
        f32 values[15];
        // Cycles (kind 0): the rates of the cycles about x, y and z (radians a second), the values their start is worked out of
        // (the amplitudes they fade or grow from), the phases they start at, the degrees a spin turns per unit moved, how fast
        // the amplitudes fade (a second), the duration, and who moves (0xFF: the node's own instance, else the cycles are only
        // read)
        struct
        {
            f32 cycleRates[3];
            f32 cycleRanges[3];
            f32 cyclePhases[3];
            f32 spinDegrees;
            f32 fadeRate;
            f32 duration;
            u8 mover;
        };
        // A body (kinds 1-3): its drag, the share it's slowed by along an axis, its size, gravity, friction, spin friction,
        // restitution and length drag, the damping, stiffness and sideways strength of the springs holding it, and its centre of
        // mass
        struct
        {
            f32 drag;
            f32 slowing;
            f32 size;
            f32 gravity;
            f32 friction;
            f32 spinFriction;
            f32 restitution;
            f32 lengthDrag;
            f32 springDamping;
            f32 springStiffness;
            f32 springAcross;
            u8 unused2C[4];
            f32 centerOfMass[3];
        };
        // Cover (kind 4): how it looks, the box's half width and half height, and the weights of a position's exposure and of
        // how much further from the player it is
        struct
        {
            CoverSearch search;
            u8 unused02[2];
            f32 halfWidth;
            f32 halfHeight;
            f32 exposureWeight;
            f32 distanceWeight;
        };
    };
    f32 grabStrength;
    f32 holdStrength;
    f32 knockScale;
    f32 turnStrength;
    f32 constraint[4];
    MotionBlockBody body;
    // The mass the physics body of the node it moves gets (the node's first float property's when it's 0 or less)
    f32 mass;
    f32 turnLimit;
    MotionBlockMotion motion;
    MotionBlockFlags flags;
    // What becoming sticky asks of an instance and gives it: the kinds of nodes it has (one of them, a bit per kind) or its
    // object (when given), the strength its attachments hold it with, the message what touches it is sent (its low half),
    // the message a stuck instance is sent and the object
    u32 stickyKinds;
    f32 stickyStrength;
    u32 touchMessage;
    u32 stickyMessage;
    u16 stickyObject;
    u8 unused82[2];
    // The node it's the motion block of (springy contacts')
    struct ObjectNode* node;
};
CHECK_OFFSET(MotionBlock, cycleRanges, 0xC);
CHECK_OFFSET(MotionBlock, spinDegrees, 0x24);
CHECK_OFFSET(MotionBlock, mover, 0x30);
CHECK_OFFSET(MotionBlock, springAcross, 0x28);
CHECK_OFFSET(MotionBlock, centerOfMass, 0x30);
CHECK_OFFSET(MotionBlock, distanceWeight, 0x10);
CHECK_OFFSET(MotionBlock, grabStrength, 0x3C);
CHECK_OFFSET(MotionBlock, turnStrength, 0x48);
CHECK_OFFSET(MotionBlock, body, 0x5C);
CHECK_OFFSET(MotionBlock, mass, 0x60);
CHECK_OFFSET(MotionBlock, motion, 0x68);
CHECK_OFFSET(MotionBlock, touchMessage, 0x78);
CHECK_OFFSET(MotionBlock, flags, 0x6C);
CHECK_SIZE(MotionBlock, 0x88);

// A trajectory controller's bits: a count (the frames its body rested far from the player, the cover positions left), it made
// the node's rigid body, where its cycles' angles come from (MotionBlockFlags::cycleSource); bit 16 set when it's let go of and
// 18 when it's made, neither read
union TrajectoryBits
{
    u32 value;
    struct
    {
        u32 count : 16;
        u32 unused16 : 1;
        u32 madeBody : 1;
        u32 unused18 : 1;
        u32 unused19 : 1;
        u32 cycleSource : 3;
        u32 unused23 : 9;
    };
};
CHECK_SIZE(TrajectoryBits, 4);

// The node's trajectory controller (0x110 bytes, game/trajectory.cpp): its node; what its cycles move the instance by or turn it
// to (a rotation, or angles about x, y and z), where the instance was at its last frame (for its rolls); the position and the
// rotation it holds the instance at, the rotation's angles; a body's reach (its instance's own box's corner) and its place's
// axes (x, z, y); the best cover position found, its score and the positions left to look at; the motion block it follows, the
// angles of its cycles about the three axes (65536ths of a turn) and their wobble's phases (the offsets from the cycles they
// follow), their amplitudes (what SetCycleAmplitudes sets), its bits and the time it started following
struct Trajectory
{
    struct ObjectNode* node;
    u8 unused04[0xC];
    Vector4 move;
    Vector4 turn;
    s32 turnAngles[3];
    u8 unused3C[4];
    Vector4 lastPosition;
    Vector4 position;
    Vector4 rotation;
    s32 angles[3];
    u8 unused7C[4];
    Vector4 reach;
    union
    {
        // The turn (radians) per unit moved its rolls make (1 over the node's roll radius, 4 at first); the squared distance from
        // the player past which its resting body counts as stuck (twice its reach, squared)
        f32 rollRate;
        f32 farDistance;
    };
    u8 unused94[0xC];
    Vector4 axisX;
    Vector4 axisZ;
    Vector4 axisY;
    struct AiPosition* cover;
    u32 unusedD4;
    struct Route* coverRoute;
    f32 coverScore;
    MotionBlock* followed;
    s32 cycles[3];
    s32 wobblePhases[3];
    // What SetCycleAmplitudes sets
    f32 motionFloats[3];
    TrajectoryBits bits;
    u32 startTime;
};
CHECK_OFFSET(Trajectory, move, 0x10);
CHECK_OFFSET(Trajectory, turnAngles, 0x30);
CHECK_OFFSET(Trajectory, lastPosition, 0x40);
CHECK_OFFSET(Trajectory, reach, 0x80);
CHECK_OFFSET(Trajectory, rollRate, 0x90);
CHECK_OFFSET(Trajectory, axisX, 0xA0);
CHECK_OFFSET(Trajectory, cover, 0xD0);
CHECK_OFFSET(Trajectory, coverScore, 0xDC);
CHECK_OFFSET(Trajectory, followed, 0xE0);
CHECK_OFFSET(Trajectory, motionFloats, 0xFC);
CHECK_OFFSET(Trajectory, bits, 0x108);
CHECK_SIZE(Trajectory, 0x110);

// A node's particle trails' bits: how many, and the turn a packet facing the way it moves makes is measured into the motion's
// state (a trail of kind 5 asks for it)
union ParticleTrailsBits
{
    u32 value;
    struct
    {
        u32 count : 5;
        u32 measuresTurn : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(ParticleTrailsBits, 4);

// The particle trails an object node leaves (game/objectnodeparts.cpp, a trail's step game/particletrails.cpp): up to 8 trails
// (AddTrail commands' arguments), the emitter each one plays (-1 none) and a time of each, its bits, how strong they are and the
// time of the trail being stepped
struct ParticleTrails
{
    static constexpr u32 MostTrails = 8;

    struct TrailArguments* trails[MostTrails];
    s32 emitters[MostTrails];
    s32 times[MostTrails];
    ParticleTrailsBits bits;
    // How strong the trails are (0 to 1: the skate's 1 on the ground and 0 in the air, the wrestle's how much the pushes oppose)
    f32 strength;
    s32* time;

    static ParticleTrails* Construct(ParticleTrails* trails) RETAIL(FUN_00240240);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002402b8);
    // A trail added (an AddTrail command's arguments): its slot
    u32 Add(const void* arguments) RETAIL(FUN_00240350);
    // The trails of a kind taken away (and the empty slots'), a slot's trail taken away (its emitter stopped, the last one moved
    // into its slot: whether there was one), every slot made empty
    void RemoveKind(u32 kind) RETAIL(FUN_002404c8);
    u32 RemoveSlot(u32 index) RETAIL(FUN_00240568);
    void Reset() RETAIL(FUN_00240488);
    // Its instance moving from a chunk through a link
    void ChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_00240648);
};
CHECK_SIZE(ParticleTrails, 0x6C);

// The kinds of a rigid body's motion and of its collisions with the world (ObjectRigidBodyBits; SetCollisions' keywords pick
// them): none, plain, an upright cylinder (an upright ellipsoid's collisions), pushed away from the instances around (slid out
// by its friction), an axis box, then its physics body's: a sphere, hulls sized by the instance's own box, more hulls
enum RigidBodyKind : u32
{
    BodyKindNone = 0,
    BodyKindPlain = 1,
    BodyKindUpright = 5,
    BodyKindSlide = 6,
    BodyKindAxisBox = 8,
    BodyKindSphere = 9,
    BodyKindCuboid = 10,
    BodyKindSimplex = 11,
    // The kinds from this one on are the physics body's
    FirstPhysicsBodyKind = BodyKindSphere,
};

// The float property a rigid body's mass comes from (the node's, or its packet's for a body block's body)
constexpr u32 RigidBodyMassProperty = 0;

// A rigid body's launch (ObjectRigidBodyBits::launch): none, and the tool's vertical one, which the game tells from none and from
// a full launch (ObjectRigidBodyBits::Launched) alone
enum RigidBodyLaunch : u32
{
    NoLaunch = 0,
    VerticalLaunch = 1,
};

// A node's rigid body's 64 bits at 0x88 (game/commandsphysics.cpp and game/rigidbodyframe.cpp): four bytes the constructor sets
// to 0xFF that nothing reads; the kind of its motion (0 none; below 9 its node's motion moves it, it's in its chunk's first list
// and its collision cache's triangles stop it, from 9 on its physics body does: 9 a sphere of the node's roll radius, 10 and 11
// hulls sized by its instance's own box, more hulls; 5 an upright cylinder as wide as its radius times its width scale, 6
// pushed away from the instances around it instead) and of its collisions with the world (5 an upright ellipsoid, 6 slid out by
// its friction, the others a sphere), how it's launched, no collisions with the world (nothing sets them), its being in the
// first list, its velocity dragged, gripped by what it touches, its own restitution, a size given, it falls, touching an
// instance, the world and anything, it's let go of once it nearly rests on the world (nothing sets it), it doesn't move for
// other rigid bodies or hulls, it moves (its frame steps it), it stops once it rests (15 steps after its launch, once it
// touches something or nearly rests), it turns, it steers itself, its size is the least slope it stands on and its
// restitution the rate its contact normal follows (a size and a restitution given while it had no physics body), its own
// length drag
union ObjectRigidBodyBits
{
    // The kinds' bits together: something moves it; the launch a node's launch gives it (its velocity moves it)
    static constexpr u64 KindsMask = u64{0xFF} << 32;
    static constexpr u32 Launched = 2;

    u64 value;
    struct
    {
        u64 unused0 : 32;
        u64 motionKind : 4;
        u64 collisionKind : 4;
        u64 launch : 2;
        u64 noWorldCollisions : 2;
        u64 inFirstList : 1;
        u64 dragged : 1;
        u64 gripped : 1;
        u64 ownRestitution : 1;
        u64 sizeGiven : 1;
        u64 falls : 1;
        u64 unused50 : 1;
        u64 touchingInstance : 1;
        u64 touchingWorld : 1;
        u64 touching : 1;
        u64 releasedAtRest : 1;
        u64 immovable : 1;
        u64 moving : 1;
        u64 stopsAtRest : 1;
        u64 turning : 1;
        u64 steersItself : 1;
        u64 slopeLimited : 1;
        u64 ownNormalRate : 1;
        u64 ownLengthDrag : 1;
        u64 unused63 : 1;
    };
};
CHECK_SIZE(ObjectRigidBodyBits, 8);

// A rigid body's magnet's ways (ObjectRigidBodyState::magnetWay): from the body toward the position, along its z axis
enum MagnetWay : u32
{
    MagnetFromBody = 0,
    MagnetAlongZAxis = 1,
};

// The modes of the magnet's strength the keywords set (ObjectRigidBodyState::magnetStrength): MagnetPull pulls with neither
enum MagnetStrength : u32
{
    MagnetEven = 1,
    MagnetMode2 = 2,
};

// A node's rigid body's word at 0x90 (the retail code reads and writes it as 64 bits, with its node): on the ground and against
// a wall this frame (the movement step clears them), its contact normal eases toward each new one and it keeps to what it
// touches (a climbing chase's), the modes of its magnet's strength and its way (MagnetStrength, MagnetWay), it's out of action,
// the steps since it was launched (counting up to 15) and since it last touched what it rides (the ride let go at 5), a
// surface's contact message told, the impulses it gives no longer than its impulse length or that long, the volume controllers
// push it, it touched something when its contacts were last forgotten, its agent is told when it touches water. Bits 0 and 19
// are set when it's made, 2 and 3 by the physics commands, and none of them read
union ObjectRigidBodyState
{
    static constexpr u32 MostSteps = 0xF;
    static constexpr u32 RideLostSteps = 5;

    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 onGround : 1;
        u32 unused2 : 1;
        u32 unused3 : 1;
        u32 followsSurface : 1;
        u32 againstWall : 1;
        u32 magnetStrength : 2;
        u32 magnetWay : 2;
        u32 outOfAction : 1;
        u32 launchSteps : 4;
        u32 rideSteps : 4;
        u32 unused19 : 1;
        u32 touchedMessageSurface : 1;
        u32 impulseCapped : 1;
        u32 impulseFixed : 1;
        u32 pushedByVolumes : 1;
        u32 touched : 1;
        u32 tellsWaterTouches : 1;
        u32 unused26 : 6;
    };
};
CHECK_SIZE(ObjectRigidBodyState, 4);

// The rigid body an object node moves with (0xE0 bytes): the normal of what it touches, the frame's contacts with instances, the
// instance it rides (a packet's offset spreads over its box) with its movement node and where the body is in its space, its bits
// and its state with its node, what the physics commands give it (its physics body gets them too when it has one), its
// collision cache, the physics body it moves as (one of the world's), and the lists of its chunk's rigid bodies it's in with its
// index in each
struct ObjectRigidBody
{
    static constexpr u16 NoFirstIndex = 0xFFFF;
    static constexpr u8 NoSecondIndex = 0xFF;
    // The speed (squared) it rests below
    static constexpr f32 RestingSpeedSquared = Rounded(0.001);

    u8 unused00[0x40];
    // The normal of what it touches (eased toward each new one's, back up once it has touched nothing for a while)
    Vector4 contactNormal;
    // The frame's contacts with instances: the normals of the hulls it touched added up and how many things it touched
    Vector4 contactSum;
    f32 contactCount;
    u8 unused64[0x70 - 0x64];
    // Where it is in the space of the instance it rides
    Vector4 ridePosition;
    ReferencedObject* object;
    struct MovementNode* rideMovement;
    ObjectRigidBodyBits bits;
    ObjectRigidBodyState state;
    struct ObjectNode* node;
    // What SetMagnet sets: a size nothing reads and the magnet's pull
    f32 sizes[2];
    // An upright cylinder's width over its radius
    f32 widthScale;
    f32 gravity;
    f32 drag;
    f32 lengthDrag;
    f32 friction;
    f32 restitution;
    f32 size;
    // How long its capped or fixed impulses are
    f32 impulseLength;
    // The seconds since it last touched anything
    f32 untouchedTime;
    // The turn the turn command gives it (radians), which nothing reads
    f32 unusedC4;
    // The grip of what it touches (the strongest of a frame, up to 1)
    f32 grip;
    // Its node's vertical speed when it was made, which nothing reads
    f32 unusedCC;
    struct CollisionCache* cache;
    DynamicBody* physicsBody;
    struct ChunkRigidBodies* chunkBodies;
    u16 firstIndex;
    u8 secondIndex;
};
CHECK_OFFSET(ObjectRigidBody, contactSum, 0x50);
CHECK_OFFSET(ObjectRigidBody, contactCount, 0x60);
CHECK_OFFSET(ObjectRigidBody, ridePosition, 0x70);
CHECK_OFFSET(ObjectRigidBody, rideMovement, 0x84);
CHECK_OFFSET(ObjectRigidBody, bits, 0x88);
CHECK_OFFSET(ObjectRigidBody, state, 0x90);
CHECK_OFFSET(ObjectRigidBody, node, 0x94);
CHECK_OFFSET(ObjectRigidBody, widthScale, 0xA0);
CHECK_OFFSET(ObjectRigidBody, gravity, 0xA4);
CHECK_OFFSET(ObjectRigidBody, impulseLength, 0xBC);
CHECK_OFFSET(ObjectRigidBody, grip, 0xC8);
CHECK_OFFSET(ObjectRigidBody, unusedCC, 0xCC);
CHECK_OFFSET(ObjectRigidBody, physicsBody, 0xD4);
CHECK_OFFSET(ObjectRigidBody, secondIndex, 0xDE);
CHECK_SIZE(ObjectRigidBody, 0xE0);

// A chunk's two lists of the rigid bodies in it (0x800 bytes, made the first time a body goes in): their counts and the bodies
// (taking one out moves the last into its place, a list takes 255)
struct ChunkRigidBodies
{
    static constexpr u16 MostBodies = 0xFF;

    u16 secondCount;
    u16 firstCount;
    ObjectRigidBody* first[256];
    ObjectRigidBody* second[255];
};
CHECK_SIZE(ChunkRigidBodies, 0x800);

// A head tracking's settings' bits: the joints and the exit point its head is (0xFF none), how it steers
// (HeadTrackingSettings::Steering: none, it turns its joints) and it doesn't hear noises. Bit 27 is set with the turn about y's
// limit and never read, 28-31 are what the memory had
union HeadTrackingSettingsBits
{
    u32 value;
    struct
    {
        u32 joint : 8;
        u32 secondJoint : 8;
        u32 exitPoint : 8;
        u32 steering : 2;
        u32 ignoresNoises : 1;
        u32 unused27 : 1;
        u32 unused28 : 4;
    };
};
CHECK_SIZE(HeadTrackingSettingsBits, 4);

// What a head tracking is made with (the CreateHeadTracking command's arguments from its first, which it keeps): how fast it
// turns (the share of the way an instance steers), how far its head turns about x either way and about y (65536ths of a turn),
// the directions of those limits, its bits, its damping, and the seen stamp of its instance past which it isn't stepped (0x640
// less it; none from 0xFF on)
struct HeadTrackingSettings
{
    // Its instance steered whole or by its facing alone (either of the bits)
    enum Steering : u32
    {
        SteersInstance = 1,
        SteersFacing = 2,
    };

    u32 unused00;
    f32 range;
    f32 stiffness;
    s32 negativePitch;
    s32 positivePitch;
    s32 yawLimit;
    f32 direction[3];
    u32 unused24[6];
    HeadTrackingSettingsBits bits;
    f32 damping;
    u32 unseenLimit;
};
CHECK_OFFSET(HeadTrackingSettings, bits, 0x3C);
CHECK_SIZE(HeadTrackingSettings, 0x48);

// A head tracking's bits (the low half of 64 bits in retail, its stiffness the upper half): the joints and the exit point its
// head is, a limit was hit this frame, the turn about y went below and above its limit, the turn about x did, it turns back to
// rest, its joints are hooked. Bit 31 is cleared and never read
union HeadTrackingState
{
    // The limits' bits (limited and the four below and above)
    static constexpr u32 LimitsMask = 0x1F000000;

    u32 value;
    struct
    {
        u32 joint : 8;
        u32 secondJoint : 8;
        u32 exitPoint : 8;
        u32 limited : 1;
        u32 yawBelow : 1;
        u32 yawAbove : 1;
        u32 pitchBelow : 1;
        u32 pitchAbove : 1;
        u32 returning : 1;
        u32 hooked : 1;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(HeadTrackingState, 4);

// A head tracking's flags: it tracks (the look following it only then), it stops once back at rest, it has a target, its agent
// turns the head itself, the playable characters' look ignores it. Bit 0 is cleared and never read
union HeadTrackingFlags
{
    u64 value;
    struct
    {
        u64 unused0 : 1;
        u64 tracking : 1;
        u64 stops : 1;
        u64 hasTarget : 1;
        u64 agentTurns : 1;
        u64 ignoredByLook : 1;
        u64 unused6 : 58;
    };
};
CHECK_SIZE(HeadTrackingFlags, 8);

// What a node tracks with its head (0xE0 bytes, vtable D_00300EB8: 1 the destructor, 2 its joints hooked on an animator, 3
// unhooked, 4 a joint posed): its node, its head's turns about x and y (radians), how fast they turn and their limits, the turns
// in 65536ths of a turn, its head's matrix in the world when it was last posed and the direction to its target from there, its
// bits and stiffness, its damping, what it looks at, the target it comes back to (once its weight fades below that one's) and the
// last one it looked at, its settings, when it took its target (on the target's clock), its weight and its flags
struct HeadTracking
{
    const GccVTableEntry* vtable;
    struct ObjectNode* node;
    f32 pitch;
    f32 yaw;
    f32 pitchSpeed;
    f32 yawSpeed;
    f32 maxPitch;
    f32 minPitch;
    f32 maxYaw;
    s32 pitchAngle;
    s32 yawAngle;
    s32 rollAngle;
    // Three words the turns' resets clear, which nothing reads
    u8 unused30[0x10];
    Matrix4x4 matrix;
    Vector4 direction;
    Vector4 unused90;
    HeadTrackingState bits;
    f32 stiffness;
    f32 damping;
    u32 unusedAC;
    Reference* target;
    Reference* remembered;
    Reference* last;
    const HeadTrackingSettings* settings;
    u32 time;
    // A random 2 to 10 its setting up gives it, which nothing reads
    f32 unusedC4;
    f32 weight;
    f32 rememberedWeight;
    HeadTrackingFlags flags;

    // Made for a node: its turns' base (its joints none, its damping 1), then no targets, settings or flags
    static HeadTracking* ConstructTurner(HeadTracking* tracking, struct ObjectNode* node) RETAIL(FUN_00236d00);
    static HeadTracking* Construct(HeadTracking* tracking, struct ObjectNode* node) RETAIL(FUN_00237288);
    // Its vtable's slot 4, its joint posed: the matrix the joint's animation makes (or the exit point's when its head is one)
    // kept in the world, the turns added to the joint's rotation; yes
    u32 PoseJoint(struct JointAnimator* animator, Matrix4x4* jointMatrix) RETAIL(FUN_00237138);
};
CHECK_OFFSET(HeadTracking, pitchAngle, 0x24);
CHECK_OFFSET(HeadTracking, matrix, 0x40);
CHECK_OFFSET(HeadTracking, bits, 0xA0);
CHECK_OFFSET(HeadTracking, stiffness, 0xA4);
CHECK_OFFSET(HeadTracking, damping, 0xA8);
CHECK_OFFSET(HeadTracking, settings, 0xBC);
CHECK_OFFSET(HeadTracking, weight, 0xC8);
CHECK_SIZE(HeadTracking, 0xE0);
CHECK_OFFSET(HeadTracking, target, 0xB0);
CHECK_OFFSET(HeadTracking, flags, 0xD0);

// A sense's bits: its kind (PerceptionSense::Kind), the player counts as noticed
union PerceptionSenseBits
{
    u32 value;
    struct
    {
        u32 kind : 3;
        u32 noticesPlayer : 1;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(PerceptionSenseBits, 4);

// A perception's sense (an AddPerception command's arguments from its first): its bits, what the noticed instances' presence is
// divided by, the kinds of nodes of the instances it senses (a bit per kind), the objects it notices (their IDs, how many), its
// radius, the squared distance their presence falls off over, how fast its level decays, the seconds between its steps, its
// level's limits, the speed's scale and the weight of the attention the instances pay its node
struct PerceptionSense
{
    // The instances around, a level rising by its decay, its node's speed, and a kind the scripts can give (KeywordUnusedSense)
    // that nothing steps
    enum Kind : u32
    {
        KindInstances = 0,
        KindRising = 1,
        KindSpeed = 2,
        KindUnused = 3,
    };

    // The share of its decay its level moves by in a step (rising, or falling back while nothing's sensed)
    static constexpr f32 DecayShare = Rounded(0.3);

    PerceptionSenseBits bits;
    f32 divisor;
    u32 kinds;
    u16 objects[8];
    u8 objectCount;
    u8 unused1D[3];
    f32 radius;
    f32 falloff;
    f32 decay;
    f32 interval;
    f32 lowest;
    f32 highest;
    f32 speedScale;
    f32 attentionWeight;
};
CHECK_OFFSET(PerceptionSense, objectCount, 0x1C);
CHECK_OFFSET(PerceptionSense, radius, 0x20);
CHECK_OFFSET(PerceptionSense, interval, 0x2C);
CHECK_OFFSET(PerceptionSense, attentionWeight, 0x3C);

// A perception's bits: how many senses it has
union PerceptionBits
{
    u64 value;
    struct
    {
        u64 count : 4;
        u64 unused4 : 60;
    };
};
CHECK_SIZE(PerceptionBits, 8);

// An object node's perception (ObjectNode::perception, 0x90 bytes, made the first time a sense is added): its senses (up to 8),
// each one's level (0 to 1), whether it's on and when it last stepped (on its node's clock), the direction away from what the
// instances sense pushes it, and its bits
struct Perception
{
    static constexpr u32 MostSenses = 8;

    PerceptionSense* senses[MostSenses];
    f32 levels[MostSenses];
    u8 on[MostSenses];
    u32 times[MostSenses];
    u8 unused68[8];
    Vector4 direction;
    PerceptionBits bits;
    u8 unused88[8];
};
CHECK_OFFSET(Perception, on, 0x40);
CHECK_OFFSET(Perception, times, 0x48);
CHECK_OFFSET(Perception, direction, 0x70);
CHECK_OFFSET(Perception, bits, 0x80);
CHECK_SIZE(Perception, 0x90);

// How a route's keys are stepped: they go backwards, they don't go on, they went round
union WaypointFlags
{
    u8 value;
    struct
    {
        u8 backwards : 1;
        u8 stopped : 1;
        u8 unused2 : 1;
        u8 wrapped : 1;
        u8 unused4 : 4;
    };
};
CHECK_SIZE(WaypointFlags, 1);

// The positions and the paths an object instance names (0x50 bytes), the route it follows: its keys are its positions (the key
// it's at, the first and the last), the path it's on (its direction and how far along it the instance is), the route's step
// it's at (0xFF none) and the path that led there; the flags and the counts are the bytes of a 64 bit word in retail
struct Waypoints
{
    // A key of none, and a route's step of none (what a step comes to stepped back past 0)
    static constexpr u8 NoKey = 0xFF;
    static constexpr u8 NoRouteStep = 0xFF;

    PointerArray<LayoutPosition> positions;
    PointerArray<LayoutPath> paths;
    Vector4 pathDirection;
    f32 pathParameter;
    Route* route;
    AiPath* routePath;
    u8 routeIndex;
    u8 unused3D[3];
    WaypointFlags flags;
    u8 keyCount;
    u8 pathCount;
    u8 pathIndex;
    u8 key;
    u8 firstKey;
    u8 lastKey;
    u8 unused47;
    u8 unused48[8];

    static Waypoints* Construct(Waypoints* waypoints) RETAIL(FUN_0020f0f0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020f160);
    void Reset() RETAIL(FUN_0020f1d0);
    void AddPosition(LayoutPosition* position) RETAIL(FUN_0020f448);
    void AddPath(LayoutPath* path) RETAIL(FUN_0020f360);
    // The keys stepped, going round past the last or the first one
    void NextKey() RETAIL(FUN_0020f5b0);
    void PreviousKey() RETAIL(FUN_0020f5f8);
    // A route taken (the one it had let go): its last step the one it's at; the route given up, and given up with the key
    // made the first
    void SetRoute(Route* route) RETAIL(FUN_0020f2d8);
    void ReleaseRoute() RETAIL(FUN_0020f758);
    void ClearRoute() RETAIL(FUN_0020f268);
    // The route's steps stepped (from the first again when it went round), and the route started again
    void NextRouteStep() RETAIL(FUN_0020f6d0);
    void PreviousRouteStep() RETAIL(FUN_0020f640);
    void RestartRoute() RETAIL(FUN_0020f6b0);
};
CHECK_OFFSET(Waypoints, route, 0x34);
CHECK_OFFSET(Waypoints, flags, 0x40);
CHECK_SIZE(Waypoints, 0x50);

// An object node's flags: a focus instance, a focus position, its rigid body falls (a spring then doesn't pull it up or down),
// it was updated this frame, it handled an event since its last update and its rigid body rides an instance (either updates it
// every frame), what a runner finishing leaves it (its particles but the trails of kind 1, its trajectory controller), a stored
// position, it was launched or its ball or grabber still looks for contact (its trajectory's step watches its physics body until
// it touches something), its cover search ended, found cover (the stored position) or found none, its motion moves the stored
// place, AgentRef2 is kept while it's asleep, a runner finishing leaves its perception, it's put back where it was before its
// frame unless it moved (the playable characters'), it moves, it accelerates. Bit 4 is what a motion's step that left it short
// of its target sets and 10-12 what the Keep command's bits 2-4 set, none of them read. Other nodes built on the base read the
// word their own way (pickups)
union ObjectNodeFlags
{
    // The focus instance and focus position bits, cleared together when the node forgets its focus
    static constexpr u32 FocusMask = 0x3;

    u32 value;
    struct
    {
        u32 focusInstance : 1;
        u32 focusPosition : 1;
        u32 falls : 1;
        u32 updated : 1;
        u32 unused4 : 1;
        u32 handledEvent : 1;
        u32 riding : 1;
        u32 unused7 : 1;
        u32 keepsParticles : 1;
        u32 keepsTrajectory : 1;
        u32 unused10 : 1;
        u32 unused11 : 1;
        u32 unused12 : 1;
        u32 storedPosition : 1;
        u32 seeksContact : 1;
        u32 searchEnded : 1;
        u32 foundCover : 1;
        u32 noCover : 1;
        u32 movesStoredPlace : 1;
        u32 unused19 : 1;
        u32 keepsAgentRef2 : 1;
        u32 unused21 : 1;
        u32 keepsPerception : 1;
        u32 pinned : 1;
        u32 moves : 1;
        u32 accelerates : 1;
        u32 unused26 : 6;
    };
};
CHECK_SIZE(ObjectNodeFlags, 4);

// The base of the kind 1 nodes (0xD0 bytes, vtable D_00301058 over InstanceNodePrototype_Methods): the middle of its instance's
// collision box, what it focuses on (an instance or a position, as its flags say), where its instance started, its object, its
// properties and agent, its flags, a rank scripts give it (0xFF none: the messages command 210 sends go by it), its last trigger
// message, the node it takes its object and properties from when there's one, its two behaviour runners, the instance its
// packet tracks, and where its instance was when its frame started and how far the frame moved it
struct ObjectNodeBase : GameNode
{
    // Its vtable's functions past the game node's
    enum Slot : u32
    {
        ReleasePartsUnlessUnloadingSlot = 11,
        SetAgentSlot = 12,
        ResetSlot = 13,
        TakesPacketsSlot = 15,
        UnpinCollisionSlot = 16,
        PinCollisionSlot = 17,
        StartBehaviourSlot = 18,
        TriggerMessageSlot = 19,
        DoNothingSlot = 20,
        StopRunnersSlot = 21,
        RunnerFinishedSlot = 22,
        AddParticleTrailSlot = 23,
        DestroyParticleTrailsSlot = 24,
        CustomSlotSlot = 25,
        LaunchSlot = 26,
        PushSlot = 27,
        CollidedSlot = 28,
        LandedSlot = 29,
        LandedHardSlot = 30,
        ScrapedSlot = 31,
        BumpedSlot = 32,
        ForgetDesignatorSlot = 34,
        ReleasePartsSlot = 35,
        GetDesignatorSlot = 36,
        GetDesignatorPositionSlot = 37,
        SetDesignatorPositionSlot = 38,
        SetDesignatorSlot = 39,
        HasNoTrackedSoundSlot = 40,
        CodeModelKindSlot = 41,
        PacketEndedSlot = 42,
        PacketStartedSlot = 43,
        StopSoundSlot = 44,
    };

    static constexpr u8 NoRank = 0xFF;

    u8 unused18[8];
    Vector4 middle;
    union
    {
        InstanceContext* focusInstance;
        Vector4 focusPosition;
    };
    // Where its instance started, a copy of its own the node may have (freed with it) and the one in use
    InstancePlacement information;
    InstancePlacement* ownInformation;
    InstancePlacement* informationPointer;
    GameObject* object;
    // The ID of the object it was made of (0xFFFF none), which sounds its own object lacks come from
    u16 ownObjectId;
    u8 unused7E[2];
    PropertyHolder* properties;
    Agent* agent;
    ObjectNodeFlags flags;
    u8 rank;
    u8 unused8D;
    // The last trigger message, who sent it and when (on its instance's clock)
    u16 message;
    InstanceContext* messageSender;
    u32 messageTime;
    // The node it takes its object and properties from when it has one
    GameNode* sourceNode;
    u32 unused9C;
    BehaviourRunner* runners[2];
    u32 unusedA8;
    InstanceContext* tracked;
    Vector4 frameStart;
    // (A projectile's node keeps its state here)
    Vector4 frameMove;

    // Made for a chunk's instance (its start's information named after the chunk): no agent, runners or messages
    static ObjectNodeBase* Construct(ObjectNodeBase* node, struct ChunkEntry* chunk, u32 unused) RETAIL(FUN_0023e970);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0023ea28);
    // No flags, agent, messages or source node
    void Reset() RETAIL(FUN_0023d238);
    // Its vtable's slot 12: its agent (its object and properties the agent's), none
    void SetAgent(Agent* agent) RETAIL(FUN_0023d460);
    void ClearMessages() RETAIL(FUN_0023d328);
    // Whether the last trigger message came within a window (seconds) before the time: any, that message
    u32 MessageWithin(const u32* time, f32 seconds) RETAIL_N32(SecondsSinceUserMessage);
    u32 MessageWithin(u32 message, const u32* time, f32 seconds) RETAIL_N32(UserMessageWithinSeconds);
    void DestroyRunners() RETAIL(FUN_0023eaa8);
    // The prototype's destructor (its tables' slot 2): its information let go
    void DestroyPrototype(u32 destroyFlags) RETAIL(FUN_0023d290);
    // The prototype made (its start's information named after the chunk's when there's one)
    static ObjectNodeBase* ConstructPrototype(ObjectNodeBase* node) RETAIL(FUN_0023d150);
    static ObjectNodeBase* ConstructPrototype(ObjectNodeBase* node, struct ChunkEntry* chunk) RETAIL(FUN_0023d1b8);
    // Slot 32: its agent told it bumped into an instance while moving (the agent's slot 8)
    void Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal) RETAIL(FUN_0023d490);

    // Its vtable's slot 1 (the prototype's as well): an event handled, its handle let go: a trigger message (0x103) kept with
    // its sender and time and its object's behaviour for it started, 0x100 and 0x101 applied to it with the game's resources
    void HandleEvent(Reference** event) RETAIL(FUN_0022ef00);
    // Slot 18: a starter queued on the runner of a slot, or the runner made with it (taken at its next frame)
    u32 StartBehaviour(ScriptStarter* starter, InstanceContext* originator, u32 force, u32 slot) RETAIL(FUN_0023eb88);
    // Slot 19: the behaviour its object starts for a trigger message
    void OnTriggerMessage(u32 message) RETAIL(FUN_002346b8);
    // Slot 21: its runners stopped and destroyed
    void StopRunners(u32 release) RETAIL(FUN_0023ec28);
    // The object of its own object ID (none without one)
    GameObject* OwnObject() RETAIL(FUN_0023d3a8);

    // The prototype's defaults, which its own table and the base's keep (ObjectNode's functions of the same names are its own):
    // 5 its kind (an object's), 9 taken out of its chunk's list, 11 put to sleep, 22, 24, 26, 27 and 33-35 nothing, 23 no
    // particle (0xFF), 28 yes, 29-31 and 36-40 no; the base's 41 no; the prototype's own 18 no, 19 and 21 nothing. Nothing calls
    // slot 33
    u32 Kind() RETAIL(GetNodeIndex_0023C7A8);
    void Removed() RETAIL(FUN_0023c850);
    void Sleep() RETAIL(FUN_0023c7b0);
    void RunnerFinished() RETAIL(FUN_0023c7e8);
    u32 AddParticleTrail() RETAIL(FUN_0023c7f0);
    void DestroyParticleTrails() RETAIL(FUN_0023c7f8);
    void Launch() RETAIL(FUN_0023c808);
    void Push() RETAIL(FUN_0023c810);
    u32 Collided() RETAIL(FUN_0023c818);
    u32 Landed() RETAIL(FUN_0023c820);
    u32 LandedHard() RETAIL(FUN_0023c828);
    u32 Scraped() RETAIL(FUN_0023c830);
    void UnusedDoNothing() RETAIL(FUN_0023c838);
    void ForgetDesignator() RETAIL(FUN_0023c840);
    void ReleaseParts() RETAIL(FUN_0023c848);
    u32 GetDesignator() RETAIL(FUN_0023c8e8);
    u32 GetDesignatorPosition() RETAIL(FUN_0023c8f0);
    u32 SetDesignatorPosition() RETAIL(FUN_0023c8f8);
    u32 SetDesignator() RETAIL(FUN_0023c900);
    u32 HasNoTrackedSound() RETAIL(FUN_0023c908);
    u32 CodeModelKind() RETAIL(FUN_0023c930);
    u32 PrototypeStartBehaviour() RETAIL(FUN_0023c7c8);
    void PrototypeOnTriggerMessage() RETAIL(FUN_0023c7d0);
    void PrototypeStopRunners() RETAIL(FUN_0023c7e0);

    // The properties the agent's packets read
    PropertyHolder* PacketProperties();
    // The focus instance while it's awake (one asleep forgotten with the focus)
    InstanceContext* AwakeFocus();
};
CHECK_OFFSET(ObjectNodeBase, focusPosition, 0x30);
CHECK_OFFSET(ObjectNodeBase, object, 0x78);
CHECK_OFFSET(ObjectNodeBase, flags, 0x88);
CHECK_OFFSET(ObjectNodeBase, runners, 0xA0);
CHECK_SIZE(ObjectNodeBase, 0xD0);

// An object node's 64 bits at 0x150: the clock time of its last splash into water (or of its wake's sound), a countdown its
// knocks raise and its movement step lowers while it isn't pinned, a noise it hears is passed on to its instance as an event of
// the noise message, its contacts play no sounds (command 194's)
union ObjectNodeReactions
{
    u64 value;
    struct
    {
        u64 splashTime : 32;
        u64 knockCountdown : 8;
        u64 passesNoises : 1;
        u64 noContactSounds : 1;
        u64 noiseMessage : 16;
        u64 unused58 : 6;
    };
};
CHECK_SIZE(ObjectNodeReactions, 8);

// An object instance's node (0x180 bytes, vtable InstanceNodeType0_Methods): the rotation between its keys, a stored position,
// its motion's parts, its trajectory controller, head tracking and perception, the controller command 645 gives it, its
// AgentRef1 and AgentRef2, its waypoints and its motion's state, its rigid body, the surfaces it stands on and of the water it's
// in, its sounds and a value scripts give it
struct ObjectNode : ObjectNodeBase
{
    static constexpr u32 ClassId = 0x180D;
    static constexpr u8 NoContactSoundSlot = 0xFF;
    static constexpr s32 NoSurface = -1;

    Vector4 keyRotation;
    Vector4 storedPosition;
    // The place the stored space is (nullptr none), the radius it rolls with along its natural axes
    ObjectPlace* storedPlace;
    f32 rollRadius;
    Translator* translator;
    Rotator* rotator;
    Physics* physics;
    ParticleTrails* particleTrails;
    Trajectory* trajectory;
    HeadTracking* headTracking;
    Perception* perception;
    NodeController* controller;
    InstanceContext* agentRef1;
    InstanceContext* agentRef2;
    MotionBlock* motionBlock;
    Waypoints* waypoints;
    MotionState* motion;
    ObjectRigidBody* rigidBody;
    // The surface it stands on and the surface of the water it's in (where it touches the water), -1 none
    s32 surface;
    s32 waterSurface;
    u8 unused138[8];
    Vector4 waterPoint;
    ObjectNodeReactions reactions;
    // The instance sound its hard contacts and scrapes keep playing, the one followed with it (PlaySlotSound's), 0xFF none
    u8 playingSound;
    u8 unused159[7];
    u8 trackedSound;
    u8 unused161[3];
    s32 unused164;
    // Its contact sound: its first and last slot (0xFF none) and a value it plays while it's negative
    u8 contactSoundFirst;
    u8 contactSoundLast;
    u8 unused16A[2];
    f32 contactSoundValue;
    // The clock time of its last surface contact, and a value scripts give it (at most 1; while it's positive the instance is
    // counted, g_CountedInstances)
    u32 lastContactTime;
    f32 countedValue;
    u8 unused178[8];

    // Made for a chunk's instance (with waypoints when asked), and destroyed
    static ObjectNode* Construct(ObjectNode* node, struct ChunkEntry* chunk, u32 waypoints, u32 unused) RETAIL(InitInstanceNodeType0);
    void Initialise(u32 waypoints) RETAIL(FUN_0022f2e8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0023d5e8);
    // No stored position
    void ForgetStoredPosition() RETAIL(FUN_0023e7a0);
    // Its rigid body, made the first time it's asked for
    ObjectRigidBody* RigidBody() RETAIL(FUN_0023e608);
    // Its parts let go (its vtable's slot 35) unless everything's being unloaded, and its controller told (its slot 5) and
    // destroyed
    void ReleasePartsUnlessUnloading() RETAIL(FUN_0023d668);
    void DestroyController() RETAIL(FUN_0023e4a8);

    // Its vtable's small functions: 3 given its instance (its start taken from the instance's place), 6 its instance left its
    // chunk (the comeback placement forgotten for the first two reasons), 9 taken out of its chunk's list (its parts let go
    // unless everything's being unloaded, slot 11), 10 its item type, 14 and 15 (it takes packets) yes, 16 and 17 (its collision
    // placed with its instance again or where it started: a pickup's) no, 20 nothing, 25 no custom slot (0xFF), 33 nothing, 40
    // whether it has no tracked sound, 41 its code model's kind (1), 43 a packet started (nothing), 44 its playing sound stopped.
    // Nothing calls slots 14 and 33
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0023d750);
    void LeftChunk(u32 why) RETAIL(FUN_0023d300);
    void Removed() RETAIL(FUN_0023d6a0);
    u32 ItemType() RETAIL(FUN_0023c7a0);
    u32 UnusedTakesPackets() RETAIL(FUN_0023c940);
    u32 TakesPackets() RETAIL(FUN_0023c948);
    u32 UnpinCollision() RETAIL(FUN_0023c7b8);
    u32 PinCollision() RETAIL(FUN_0023c7c0);
    void DoNothing() RETAIL(FUN_0023c7d8);
    u32 CustomSlot() RETAIL(FUN_0023c800);
    void UnusedDoNothing() RETAIL(FUN_0023ddd0);
    u32 HasNoTrackedSound() RETAIL(FUN_0023c998);
    u32 CodeModelKind() RETAIL(FUN_0023c950);
    void PacketStarted() RETAIL(FUN_0023e118);
    void StopSound() RETAIL(FUN_0023dc68);

    // Its vtable's slot 13: made as new again (its runners, motion, route, parts and links let go), and slot 42: a runner's
    // packet ended, the motion's parts destroyed unless the other runner's packet runs
    void Reset() RETAIL(FUN_0022fbf0);
    void PacketEnded(BehaviourRunner* runner) RETAIL(FUN_0023e040);
    // Its parts let go: the runners (and its focus), the trajectory controller, the rigid body, the instance's attachments (its
    // kind 6 node released, its flags 6 and 7 cleared, its parent forgotten), the perception, the stored place, the head
    // tracking, the motion block (with the trajectory controller)
    void ResetRunners() RETAIL(FUN_0023eb00);
    void ReleaseTrajectory() RETAIL(FUN_0023e510);
    void ReleaseRigidBody() RETAIL(FUN_0023e668);
    void ReleaseAttachments() RETAIL(FUN_0023e7f0);
    void ReleasePerception() RETAIL(FUN_0023e238);
    void ReleaseStoredPlace() RETAIL(FUN_0023e7b8);
    void ReleaseHeadTracking() RETAIL(FUN_0023e270);
    void ReleaseMotionBlock() RETAIL(FUN_0023e560);
    // Slot 4: its instance let move into the chunk a link leads to once the linked chunk's RM2 is loaded, its rigid body moved into
    // that chunk's lists and its trails told
    u32 CanChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_0023d7f0);
    // Slot 7 (the game node's step, once its instance starts again): its controller told, its parts let go, its runners stopped,
    // made as new, its instance put back where it started; the middle of the instance's box and its position kept
    void Restart(TimeClock* clock, u32 word) RETAIL(FUN_0022fd38);
    // Slot 28: it collided with something (what, where, the impulse): its motion block told, a hard knock while it rides
    // something (a squared impulse over 10) sent to the instances around, its agent told (its slot 7)
    void Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_0023d970);
    // Its motion block told it hit something
    void HitWhileMoving(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_0023dcb0);
    // Its trajectory controller (made the first time) made to follow a motion block, and its motion block told another instance
    // touched it
    void FollowMotionBlock(MotionBlock* block, TimeClock* clock) RETAIL(FUN_0023e308);
    void MotionBlockTouched(InstanceContext* other) RETAIL(FUN_0023bd30);
    // Slots 29 to 31, its rigid body touching a surface at a point with a velocity: landed (a squared speed over 10 plays the
    // surface's impact and knocks the instances around), landed hard (over 15) and scraped (over 0.2); whether a contact played.
    // Every one asks for the surface's impact sound
    u32 Landed(CollisionSurface* surface, const Vector4* point, const Vector4* velocity) RETAIL(OnRigidBodyLanded);
    // Its physics body touched a triangle not solid to objects: of water (surface 0xC), where it touched kept and the water
    // entered or moved in, its agent told when its rigid body asks
    void TouchedWater(const struct CollisionHit* hit, const Vector4* position) RETAIL(OnRigidBodyTouchedWater);
    // Slots 23 and 24: a particle trail added (its trails made the first time), its trails destroyed
    u32 AddParticleTrail(const void* arguments) RETAIL(CreateInstanceParticle);
    void DestroyParticleTrails() RETAIL(DestroyInstanceParticles);
    // Slots 36 to 38, a designator's instance (0xDF what its ID's entry links, 0xF0 its own, 0xF2 the player's, 0xF5 the head
    // tracking's target, 0xF7 AgentRef2, 0xF8 AgentRef1, 0xFB the focus instance while it's awake), position (0xF5 to 0xF8,
    // 0xFB, the stored position 0xF6 and the focus position 0xFC: whether it has one) and a position given (the instance moved
    // there, the stored or the focus position set: whether it took it). The positions of the head tracking's target and of
    // AgentRef2 are AgentRef1's in the retail code
    InstanceContext* GetDesignator(u32 designator) RETAIL(FUN_00233e90);
    u32 GetDesignatorPosition(u32 designator, Vector4* position) RETAIL(FUN_00233fa0);
    u32 SetDesignatorPosition(u32 designator, const Vector4* position) RETAIL(FUN_002341a8);

    u32 LandedHard(CollisionSurface* surface, const Vector4* point, const Vector4* velocity) RETAIL(OnRigidBodyLandedHard);
    u32 Scraped(CollisionSurface* surface, const Vector4* point, const Vector4* velocity) RETAIL(OnRigidBodyScraped);
    // Slot 22: a runner finished: its particles destroyed (the trails of kind 1 when it keeps its particles), its trajectory
    // controller and perception let go unless it keeps them
    void RunnerFinished() RETAIL(FUN_0023dfb0);
    // Slot 34: a designator forgotten (0 the focus, 1 AgentRef1, 2 AgentRef2, 3 the stored position)
    void ForgetDesignator(u32 designator) RETAIL(FUN_0023e720);
    // Slot 35: its parts let go (its particles, trajectory controller, rigid body, attachments, head tracking and perception), its
    // sound stopped and slot 42 told no packet runs
    void ReleaseParts() RETAIL(FUN_0023d6c8);
    // Slot 39: a designator given an instance: the head tracking's target (0xF5, when it tracks), AgentRef2 (0xF7), AgentRef1
    // (0xF8), the focus instance (0xFB); whether it took it
    u32 SetDesignator(u32 designator, InstanceContext* instance) RETAIL(FUN_0023e868);

    // Its frame (vtable slot 8): while the clock runs, at the rate g_ObjectUpdateRate gives since the instance was last seen
    // (every frame after it handled an event or while it rides), its controller, its parts and its behaviour runners step and it
    // moves; else only its trajectory (when its controller asks) and its rigid body's physics body. The base's update after
    u32 Update(TimeClock* clock) RETAIL(UpdateNode_0022FEE8);

    // The parts made the first time they're needed (their motion the node's)
    Translator* MakeTranslator();
    Rotator* MakeRotator();
    Physics* MakePhysics();
};
CHECK_OFFSET(ObjectNode, translator, 0xF8);
CHECK_OFFSET(ObjectNode, waypoints, 0x124);
CHECK_OFFSET(ObjectNode, motion, 0x128);
CHECK_SIZE(ObjectNode, 0x180);

// The object node's helpers (game/objectnodehelpers.cpp) and what works on its parts (game/objectnodeparts.cpp)
extern "C"
{
    // Whether the middle of a node's instance's collision box is within an AI position: no more than 0.8 below it nor 3.7 above
    // it, nearer across than its radius (its w)
    u32 IsWithinAiPosition(const AiPosition* position, GameNode* node) RETAIL(FUN_0022c760);
    // The velocity that throws from a position to a target under a gravity, up to a height above the start or landing after a
    // time (the floats first)
    void ThrowOverHeight(f32 gravity, f32 height, const Vector4* from, const Vector4* to, Vector4* velocity)
        RETAIL_N32(FUN_0022c870);
    void ThrowInTime(f32 gravity, f32 seconds, const Vector4* from, const Vector4* to, Vector4* velocity)
        RETAIL_N32(FUN_0022c9a0);
    // An instance's facing turned toward a target by a share of the way (each at most all of it): across by the first share, up
    // and down by the second; how much it turned (1 - the cosine; nothing when it would face straight up or down)
    f32 TurnFacingToward(f32 share, f32 upShare, InstanceContext* instance, const Vector4* target) RETAIL_N32(FUN_0022d0e0);
    // Where a script's target is: a receiver's instance (0xFF none, 0xFB the focus; the position is left as it was for a receiver
    // without one), else a designator's position (a key's by its index, the next key's stepping to it, 0xF4 the originator's),
    // else the space's (0 none, 1 the start, 2 its own place, 4 facing what its packet tracks, 5 its rigid body's object, 7 the
    // stored place), an offset turned by the space added when given. A missing player's, originator's, AgentRef1's or
    // AgentRef2's place is read from address 8
    void DesignatedPosition(Vector4* position, u32 space, BehaviourRunner* runner, const Vector4* offset, u32 designator,
                            u32 receiver, u32 unused = 0) RETAIL(FUN_0022d9e8);
    // The place of a request's target: with none (0xFF) the receiver's instance's (its own instance's without one), the focus
    // instance's while it's awake (0xFB), else none
    ObjectPlace* SpaceOfRequest(BehaviourRunner* runner, u32 space, u32 target, u32 receiver, u32 flag) RETAIL(FUN_0022e0b8);
    // An instance's rotation about one of its axes snapped to the nearest multiple of a step in radians (the float first)
    void SnapInstanceRotationX(f32 step, InstanceContext* instance) RETAIL_N32(FUN_0022e410);
    void SnapInstanceRotationY(f32 step, InstanceContext* instance) RETAIL_N32(FUN_0022e598);
    void SnapInstanceRotationZ(f32 step, InstanceContext* instance) RETAIL_N32(FUN_0022e730);
    // The node's instance spun about its x, y or z axis (the first of the move's that isn't 0) by an angle in degrees per unit
    // times that move (its size when asked; the first float unused, the roll radius RollAlongX and RollAlongY take)
    void SpinAlongMove(f32 unused, f32 degreesPerUnit, ObjectNode* node, const Vector4* moved, u32 bySize)
        RETAIL_N32(FUN_0022ec40);
    // The instances around a node's instance with a physics body or collision pushed by it with no strength: within a
    // radius of the middle of its collision box, or without one touching the box grown by half a unit (the float first)
    void NotifyInstancesWithin(f32 radius, ObjectNode* node) RETAIL_N32(FUN_0022f018);
    // Its physics body went into water: a splash ahead of where it touched (at most every half second), and a body falling in
    // while spinning fast skims off it; and moves in water: a wake, the surface's sound past a speed (at most every half second)
    void EnteredWater(ObjectNode* node, CollisionSurface* surface, const Vector4* normal) RETAIL(FUN_0022f6a8);
    void MovedInWater(ObjectNode* node, CollisionSurface* surface, const Vector4* normal) RETAIL(FUN_0022f9b0);
    // The node's contact sound (a random one of its slots while its value is negative) played as loud as a strength
    void PlayContactSound(f32 strength, ObjectNode* node) RETAIL_N32(FUN_00230a28);
    // A node's sounds' object ID taken from an object
    void SetSoundObject(ObjectNodeBase* node, const GameObject* object) RETAIL(FUN_0023d390);
    // The node's stored place given a place's copy (made the first time), and its stored position set
    void SetStoredPlace(ObjectNode* node, ObjectPlace* place) RETAIL(FUN_0023d8f8);
    void SetStoredPosition(ObjectNode* node, const Vector4* position) RETAIL(FUN_0023d958);
    // A node's trajectory controller made to follow its motion block unless it already does
    void FollowOwnMotionBlock(ObjectNode* node) RETAIL(FUN_0023ddd8);
    // The knock countdown the movement step lowers raised by 4, 3, 2 or 1 for how hard the node was knocked (past 6.4e-11,
    // 1.6e-11, 4e-12 or 1e-12), at most to 255 (the float first)
    void KnockNode(f32 strength, ObjectNode* node) RETAIL_N32(FUN_0023de38);
    // A node's perception made the first time and a sense added to it, and its tracked sound stopped
    void AddPerception(ObjectNode* node, const void* arguments) RETAIL(FUN_0023e178);
    void StopNodeSounds(ObjectNode* node) RETAIL(FUN_0023e2c0);
    // A head tracking made from a command's arguments (the node's one let go of and destroyed)
    void CreateHeadTracking(ObjectNode* node, const void* arguments, TimeClock* clock) RETAIL(FUN_0023e398);
    // The node's contact sound's value (it plays while it's negative) and its first and last slot (the float first)
    void SetContactSounds(f32 value, ObjectNode* node, u32 first, u32 last) RETAIL_N32(FUN_0023e650);
    // The node's motion stopped, its rigid body's too
    void StopNodeMotion(ObjectNode* node) RETAIL(FUN_0023e6d8);
    // The object node of an instance when it takes packets
    ObjectNode* PacketNodeOf(InstanceContext* instance) RETAIL(FUN_0023e918);
    // A CreateHeadTracking command's settings (HeadTrackingSettings) made and destroyed
    void* ConstructHeadTrackingSettings(void* settings) RETAIL(FUN_0023ecb0);
    void DestroyHeadTrackingSettings(void* settings, u32 destroyFlags) RETAIL(FUN_0023eda8);
    // A perception's senses of kinds 2 and 1 (PerceptionSense; stepped as SenseInstances is): the level the node's speed times
    // the sense's scale, and the level risen by 0.3 of the sense's decay, each kept within the sense's limits; whether a sense
    // notices an object (by its ID)
    void SenseSpeed(const void* sense, TimeClock* clock, ObjectNode* node, f32* level) RETAIL(FUN_0023eec0);
    void SenseRising(const void* sense, TimeClock* clock, ObjectNode* node, f32* level) RETAIL(FUN_0023efa0);
    u32 SenseNoticesObject(const void* sense, u32 objectId) RETAIL(FUN_0023eff0);
    // A particle trail's arguments: their system set (0xFFFF none), destroyed, their offset given (an AddTrail command's from
    // its flags on); a trail's frame (its emitter started when it has none, stopped without a system, given its exit point's
    // frame: the emitter, -1 none), whether it plays on the surface its node touches (any when it names none), its emitter
    // started where the node's instance is (following the gravity's frame when asked) and stopped
    void SetTrailSystem(TrailArguments* trail, u32 system) RETAIL(FUN_0023f038);
    void DestroyTrailArguments(TrailArguments* trail, u32 destroyFlags) RETAIL(FUN_0023f040);
    void SetTrailPosition(u32* settings, const Vector4* position) RETAIL(FUN_0023f068);
    s32 UpdateTrail(TrailArguments* trail, ObjectNode* node, s32 emitter) RETAIL(FUN_0023f088);
    u32 TrailOnItsSurface(const TrailArguments* trail, ObjectNode* node) RETAIL(FUN_0023f1a8);
    s32 StartTrail(const TrailArguments* trail, u32 system, u32 unused, ObjectNode* node) RETAIL(FUN_0023f1d8);
    s32 StopTrail(TrailArguments* trail, s32 emitter) RETAIL(FUN_0023f290);
    // A motion block made cycling, a cover search or plain (the first asked), and destroyed
    MotionBlock* ConstructMotionBlock(void* memory, u32 cycling, u32 cover) RETAIL(FUN_0023f2c0);
    void DestroyMotionBlock(MotionBlock* block, u32 destroyFlags) RETAIL(FUN_0023f318);
    // The rest of a cycling motion block worked out from what its parser read: the duration kept (a positive one also gives the
    // fade rate, the largest of the cycles' ranges less a margin over it, and makes the cycles fade), and each cycle's start that
    // isn't 0 made its phase, its flag picking (for a sine) the phase passing the start falling
    void SetUpMotionBlock(MotionBlock* block, u32 fallingX, u32 fallingY, u32 fallingZ, f32 duration, f32 startX, f32 startY,
                          f32 startZ) RETAIL_N32(FUN_0023f340);
    // What a parser gives a body block: its constraint (1 and 7, 2 and 3 with its axis or normal) or a hinge (4 to 6 about x, y
    // and z), and the limit of its hinges' turn
    void SetMotionBlockConstraint(MotionBlock* block, u32 kind, const Vector4* vector) RETAIL(FUN_0023f570);
    void SetMotionBlockTurnLimit(f32 limit, MotionBlock* block) RETAIL_N32(FUN_0023f638);
    // The fade rate of cycles: the largest of three ranges less a margin (a smaller one for turning cycles) over a duration
    f32 MotionBlockFadeRate(f32 duration, u32 turns, f32 x, f32 y, f32 z) RETAIL_N32(FUN_0023f650);
    // A cycle about an axis at an angle, of a range, as its kind and sign say (nothing calls them)
    f32 MotionBlockCycleX(MotionBlock* block, const s32* angle, f32 range) RETAIL_N32(FUN_0023f6b0);
    f32 MotionBlockCycleY(MotionBlock* block, const s32* angle, f32 range) RETAIL_N32(FUN_0023f7c0);
    f32 MotionBlockCycleZ(MotionBlock* block, const s32* angle, f32 range) RETAIL_N32(FUN_0023f8d0);
    // The instances linked to the block's node's instance let go; the block not sticky any more (what stuck to it let go too
    // unless asked)
    void MakeMotionBlockNormal(MotionBlock* block) RETAIL(FUN_0023f9e0);
    void StopMotionBlockSticking(MotionBlock* block, u32 keepStuck) RETAIL(FUN_0023fa10);
    // A motion block's kind set (MotionBlock::Kind), and the block made plain (its values cleared, its mover its node's own
    // instance) or a cover search (its trajectory asking for frames)
    void SetMotionBlockKind(MotionBlock* block, u32 kind) RETAIL(FUN_0023fa70);
    void MakePlainMotionBlock(MotionBlock* block) RETAIL(FUN_0023fa98);
    void MakeCoverMotionBlock(MotionBlock* block) RETAIL(FUN_0023fb20);
    // The head tracking's turns made none (its matrix the identity), and its vtable's functions (D_00300EB8): 1 the destructor
    // (the base's part: its references aren't let go), 2 its joints hooked on an animator, 3 unhooked
    void ResetHeadTurns(HeadTracking* tracking) RETAIL(FUN_0023fb80);
    void DestroyHeadTurner(HeadTracking* tracking, u32 destroyFlags) RETAIL(FUN_0023fbc8);
    void HookHeadTracking(HeadTracking* tracking, OgiAnimator* animator) RETAIL(FUN_0023fbf8);
    void UnhookHeadTracking(HeadTracking* tracking, OgiAnimator* animator) RETAIL(FUN_0023fc60);
    // The head tracking stopped: its flags 3 and 5 set, its target let go (stopping once back at rest)
    void StopHeadTracking(HeadTracking* tracking) RETAIL(FUN_0023fdd0);
    // A node's perception (Perception): a sense added (0xFF when it has 8 or one of the kind, else the count), made, a kind's
    // level set or added to (kept within 0 and 1; the float first) and read (whether it has the kind), the index of a kind's
    // sense (whether there's one), whether there's one, a kind's sense turned off and on
    u32 AddSense(void* perception, const void* sense) RETAIL(FUN_0023fe08);
    void ConstructPerception(void* perception) RETAIL(FUN_0023fea8);
    u32 SetPerceptionWeight(f32 weight, void* perception, u32 slot) RETAIL_N32(FUN_0023ff38);
    u32 PerceptionValue(void* perception, u32 slot, f32* value) RETAIL(FUN_0023ffb0);
    u32 AddPerceptionWeight(f32 weight, void* perception, u32 slot) RETAIL_N32(FUN_00240010);
    u32 FindSense(void* perception, u32 kind, u32* index) RETAIL(FUN_002400a0);
    u32 HasSense(void* perception, u32 kind) RETAIL(FUN_002400f8);
    void TurnSenseOff(void* perception, u32 kind) RETAIL(FUN_00240150);
    void TurnSenseOn(void* perception, u32 kind) RETAIL(FUN_002401c8);
    // The trajectory controller's kinds started: a cover search (no cover yet, the cover positions gathered when it looks in a
    // box, the node's route let go), a grabber's and a ball's rigid body (hulls and a sphere, the node's mass its first float
    // property); its body slowed along an axis (0 and 1 x; the share goes as the time too; the float first); what a grabber
    // touched held by the node's instance on a spring, or the other way round (the block's flag 11, or 12 and what it touched is
    // AgentRef1), and the node's instance held by its attachments
    void StartCoverSearch(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_00240730);
    void StartGrabber(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_00240790);
    void StartBall(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_00240880);
    void SlowBodyAlongAxis(f32 share, Trajectory* unused, DynamicBody* body, u32 axis) RETAIL_N32(FUN_00240948);
    void GrabTouched(f32 strength, Trajectory* trajectory, ObjectNode* node, InstanceContext* touched) RETAIL_N32(FUN_00240a78);
    void HoldWithAttachments(f32 strength, Trajectory* trajectory, ObjectNode* node) RETAIL_N32(FUN_00240b68);
}

// The layouts' offsets the retail code uses
CHECK_OFFSET(MotionState, bits, 0x8);
CHECK_OFFSET(MotionState, speed, 0xc);
CHECK_OFFSET(MotionState, chaseSpeed, 0x10);
CHECK_OFFSET(MotionState, startVelocity, 0x30);
CHECK_OFFSET(MotionState, turn, 0x50);
CHECK_OFFSET(Translator, target, 0x10);
CHECK_OFFSET(Translator, direction, 0x20);
CHECK_OFFSET(Translator, wander, 0x40);
CHECK_OFFSET(Translator, distance, 0x54);
CHECK_OFFSET(Rotator, start, 0x10);
CHECK_OFFSET(Rotator, target, 0x20);
CHECK_OFFSET(Physics, velocity, 0x20);
CHECK_OFFSET(Trajectory, position, 0x50);
CHECK_OFFSET(Trajectory, rotation, 0x60);
CHECK_OFFSET(Trajectory, angles, 0x70);
CHECK_OFFSET(Trajectory, wobblePhases, 0xF0);
CHECK_OFFSET(ParticleTrails, bits, 0x60);
CHECK_OFFSET(HeadTracking, target, 0xb0);
CHECK_OFFSET(Waypoints, paths, 0x10);
CHECK_OFFSET(Waypoints, pathDirection, 0x20);
CHECK_OFFSET(Waypoints, pathParameter, 0x30);
CHECK_OFFSET(Waypoints, routePath, 0x38);
CHECK_OFFSET(Waypoints, routeIndex, 0x3c);
CHECK_OFFSET(Waypoints, keyCount, 0x41);
CHECK_OFFSET(Waypoints, pathIndex, 0x43);
CHECK_OFFSET(Waypoints, key, 0x44);
CHECK_OFFSET(Waypoints, lastKey, 0x46);
CHECK_OFFSET(ObjectNodeBase, middle, 0x20);
CHECK_OFFSET(ObjectNodeBase, information, 0x40);
CHECK_OFFSET(ObjectNodeBase, ownInformation, 0x70);
CHECK_OFFSET(ObjectNodeBase, informationPointer, 0x74);
CHECK_OFFSET(ObjectNodeBase, ownObjectId, 0x7c);
CHECK_OFFSET(ObjectNodeBase, properties, 0x80);
CHECK_OFFSET(ObjectNodeBase, agent, 0x84);
CHECK_OFFSET(ObjectNodeBase, rank, 0x8c);
CHECK_OFFSET(ObjectNodeBase, message, 0x8e);
CHECK_OFFSET(ObjectNodeBase, messageSender, 0x90);
CHECK_OFFSET(ObjectNodeBase, messageTime, 0x94);
CHECK_OFFSET(ObjectNodeBase, sourceNode, 0x98);
CHECK_OFFSET(ObjectNodeBase, tracked, 0xac);
CHECK_OFFSET(ObjectNodeBase, frameStart, 0xb0);
CHECK_OFFSET(ObjectNodeBase, frameMove, 0xc0);
CHECK_OFFSET(ObjectNode, keyRotation, 0xd0);
CHECK_OFFSET(ObjectNode, storedPosition, 0xe0);
CHECK_OFFSET(ObjectNode, storedPlace, 0xf0);
CHECK_OFFSET(ObjectNode, rollRadius, 0xf4);
CHECK_OFFSET(ObjectNode, rotator, 0xfc);
CHECK_OFFSET(ObjectNode, physics, 0x100);
CHECK_OFFSET(ObjectNode, particleTrails, 0x104);
CHECK_OFFSET(ObjectNode, trajectory, 0x108);
CHECK_OFFSET(ObjectNode, headTracking, 0x10c);
CHECK_OFFSET(ObjectNode, perception, 0x110);
CHECK_OFFSET(ObjectNode, controller, 0x114);
CHECK_OFFSET(ObjectNode, agentRef1, 0x118);
CHECK_OFFSET(ObjectNode, agentRef2, 0x11c);
CHECK_OFFSET(ObjectNode, motionBlock, 0x120);
CHECK_OFFSET(ObjectNode, rigidBody, 0x12c);
CHECK_OFFSET(ObjectNode, surface, 0x130);
CHECK_OFFSET(ObjectNode, waterSurface, 0x134);
CHECK_OFFSET(ObjectNode, waterPoint, 0x140);
CHECK_OFFSET(ObjectNode, reactions, 0x150);
CHECK_OFFSET(ObjectNode, playingSound, 0x158);
CHECK_OFFSET(ObjectNode, trackedSound, 0x160);
CHECK_OFFSET(ObjectNode, unused164, 0x164);
CHECK_OFFSET(ObjectNode, contactSoundFirst, 0x168);
CHECK_OFFSET(ObjectNode, contactSoundLast, 0x169);
CHECK_OFFSET(ObjectNode, contactSoundValue, 0x16C);
CHECK_OFFSET(ObjectNode, lastContactTime, 0x170);
CHECK_OFFSET(ObjectNode, countedValue, 0x174);

// The gravity a launch (the object nodes' slot 26, LaunchNode) or a throw takes when it's given a negative one, and the gravity
// the callers give for it
constexpr f32 DefaultLaunchGravity = 30.0f;
constexpr f32 LaunchWithDefaultGravity = -1.0f;

extern "C"
{
    // The sound ID of an object's sound slot for a node: its source object's, else its own object's (0xFFFF none)
    u16 SoundOfSlot(struct ObjectNode* node, u32 slot) RETAIL(GetSoundID);
    // The sound of a slot played where a node's instance is, or followed with it and kept in its tracked sound slot when asked
    // (and it has none)
    void PlaySlotSound(struct ObjectNode* node, u32 slot, u32 tracked) RETAIL(FUN_0021eaa0);
    // The position of an exit point of an instance's model (its slot below 63, the model's animator having it), an offset turned
    // by the exit point's matrix added when given: whether it has one
    u32 JointPosition(InstanceContext* instance, u32 joint, Vector4* position, const Vector4* offset) RETAIL(FUN_0022e190);
    // The object and the properties of an object node, or of the node it stands in for (and so on)
    GameObject* SourceObject(GameNode* node) RETAIL(GetGameObjectAddress_FromInstance_);
    PropertyHolder* GetPropsHolderFromInstanceNode(GameNode* node) RETAIL(GetPropsHolderFromInstanceNode);

    // The rotation facing from a key to the next (along x and z; none when they're above each other)
    void KeyRotation(const LayoutPosition* from, const LayoutPosition* to, Vector4* rotation) RETAIL(FUN_0020f7b0);
    // A packet's motion started: its time and end, what it goes to (a receiver, a key, a route's step, the focus, AgentRef1 or 2,
    // the player, the head tracking's target or a position) and its translation, rotation and physics set up
    void StartPacketMotion(BehaviourRunner* runner, TimeClock* clock) RETAIL(FUN_0020cd10);
    // Where a translation goes and where a rotation turns to
    void SetTranslationTarget(Translator* translator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step,
                              InstanceContext* instance, Vector4* position) RETAIL(FUN_0020a728);
    void SetRotationTarget(Rotator* rotator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step, InstanceContext* instance)
        RETAIL(FUN_0020c2a0);
    // A projectile's physics from the packet (its power given): its velocity to the translation's direction
    void SetUpProjectile(f32 power, Physics* physics, ControlPacket* packet, PropertyHolder* properties, Vector4* direction)
        RETAIL_N32(FUN_0020be50);
    // A packet's frame: the trajectory holds the instance, the target follows what it tracks, the rotation and the translation
    // step (an interpolation steps both), the instance faces the way it moves or rolls along its natural axes, and the packet
    // ends once both are done (else it's checked for its delay or sync)
    void PacketFrame(BehaviourRunner* runner, TimeClock* clock) RETAIL(FUN_0020e258);
    // The packet ended once it doesn't stall and its delay is over, or once the focus instance's runners left the sync unit's
    // state. Without a focus instance the retail code reads the nodes of what the caller left in a3 (an uninitialized variable)
    void CheckPacketEnd(BehaviourRunner* runner, TimeClock* clock, u32 unused, InstanceContext* leftover) RETAIL(FUN_0020eab8);
    // Whether one of a node's runners (the second first) has a level, up to its first empty one, that entered the state and left
    // it
    u32 LeftSyncState(GameNode* node, u32 state) RETAIL(FUN_002347b0);
    // The trajectory controller holding the instance at its position, or its rotation when its cycles turn it (its parent's
    // attachment's when it hangs from one), when it follows cycles that move the node's own instance
    void HoldTrajectory(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023b8d0);
    // The translation's target following what it tracks, an interpolation's step, the rotation's and the translation's steps
    // (whether they're done)
    void FollowTarget(Translator* translator, BehaviourRunner* runner, ObjectNode* node) RETAIL(FUN_0020b130);
    u32 StepInterpolation(ObjectNode* node, BehaviourRunner* runner) RETAIL(FUN_00232e30);
    u32 StepAcceleratedInterpolation(ObjectNode* node, BehaviourRunner* runner) RETAIL(FUN_002331c0);
    // The motion's velocity along the translation for a fraction of it: its direction times 6 (t - t²)
    void SmoothVelocity(f32 t, MotionState* motion, const Translator* translator) RETAIL_N32(FUN_0020efb8);
    u32 StepRotation(ObjectNode* node, BehaviourRunner* runner) RETAIL(FUN_00233500);
    // An accelerated rotation's step, and how far along an accelerated motion is after some seconds (its distance's fraction; the
    // acceleration's and deceleration's times read with the node's own properties)
    u32 StepAcceleratedRotation(ObjectNode* node, BehaviourRunner* runner) RETAIL(FUN_00233668);
    f32 AcceleratedFraction(ObjectNode* node, ControlPacket* packet, f32 seconds) RETAIL_N32(FUN_00233768);
    u32 StepTranslation(ObjectNode* node, BehaviourRunner* runner, TimeClock* clock) RETAIL(FUN_00231050);
    // The instance turned the way it rolls along its natural x or y axis over the distance it moved
    void RollAlongX(f32 radius, ObjectNode* node, Vector4* moved) RETAIL_N32(FUN_0022ea88);
    void RollAlongY(f32 radius, ObjectNode* node, Vector4* moved) RETAIL_N32(FUN_0022e8c8);
    // The instance moved to a position and turned to a rotation (queued when it changed), or its stored place instead (unless the
    // packet's space is the stored one) when the node's motion moves it
    void MoveInstance(ObjectNode* node, ControlPacket* packet, const Vector4* position) RETAIL(FUN_0023ca10);
    void TurnInstance(ObjectNode* node, ControlPacket* packet, const Vector4* rotation) RETAIL(FUN_0023cbf8);
    // A spring's physics stepped by a force: the motion's velocity kept as it was and moved by the force, which a damping of 0 or
    // more slows by the velocity, while a negative one caps the speed at its size
    void StepSpringPhysics(f32 elapsed, f32 damping, Physics* physics, const Vector4* force) RETAIL_N32(FUN_0020c0b0);
    // The motions' translation steps: a spring, a throw, the chases
    u32 StepSpring(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet) RETAIL_N32(FUN_002329f0);
    u32 StepProjectile(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet) RETAIL_N32(FUN_00232728);
    u32 StepGroundChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231160);
    // A chase's speed slowed by the turn it made (by its turn drag, at most by 90%), kept as the motion's speed
    f32 TurnSlowedSpeed(f32 turn, Physics* physics, MotionState* motion) RETAIL_N32(FUN_0020f950);
    // The lean's limit (degrees) the scripts' steering toward a target (SteerTowards, without a lean) is given
    constexpr f32 SteerMostLean = 90.0f;
    // An instance's facing turned toward a target over the ground by a share of the way there (all of it at most), leaning into
    // the turn by a factor (in degrees) up to a limit; and turned toward a target in space, its up leaning toward the target by a
    // factor, or kept (or made from the facing alone when asked): how much it turned (1 - the cosine)
    f32 SteerTowards(f32 share, f32 lean, f32 mostLean, InstanceContext* instance, const Vector4* target) RETAIL_N32(FUN_0022d4f8);
    f32 SteerBodyTowards(f32 share, f32 lean, InstanceContext* instance, const Vector4* target, u32 facingOnly) RETAIL_N32(FUN_0022cac0);
    // A rigid body's contact normal eased back up once it has touched nothing for a while and handed out as the up, its instance
    // turned to stand on it (keeping the side its x axis points to) and pressed against it; and a move cut down to its node's roll
    // radius (game/rigidbodyframe.cpp)
    void RightRigidBody(f32 elapsed, ObjectRigidBody* body, Vector4* up) RETAIL_N32(FUN_0024b808);
    void HoldRigidBodyMove(ObjectRigidBody* body, Vector4* move) RETAIL(FUN_00254940);
    u32 StepAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231580);
    u32 StepRiddenAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231840);
    u32 StepClimbingChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231cc8);
    // The node's parts' frames: its particle trails, trajectory controller (and its frame once the runners are done), head
    // tracking and perception (each sense stepped once its interval passed), and its movement
    void StepParticleTrails(ParticleTrails* trails, TimeClock* clock, ObjectNode* node) RETAIL(FUN_002403d8);
    void StepTrajectory(Trajectory* trajectory, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00238ed8);
    // A trajectory started again from where its instance is
    void RestartTrajectory(Trajectory* trajectory) RETAIL(FUN_0023a290);
    void TrajectoryFrame(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023ae68);
    void StepHeadTracking(HeadTracking* tracking, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00237448);
    void StepPerception(void* perception, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00238050);
    // A trajectory controller made (new, following nothing, its rolls turning 4 radians a unit), and made to follow a motion
    // block from the clock's time: its kind set up (a cycling block's cycles started, a body made for the node), the place it
    // holds the instance at taken when the block moves the node's own instance (its parent's attachment's when it hangs from one)
    // and its first step taken
    Trajectory* ConstructTrajectory(Trajectory* trajectory) RETAIL(FUN_00238318);
    void StartFollowing(Trajectory* trajectory, MotionBlock* block, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00238370);
    // A cover search's AI positions: those in a box around the instance (the block's half width and height) collected into the
    // cover route (made the first time), its count the positions left to look at
    void GatherCoverPositions(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_00238708);
    // A rigid body given a body block's values (those that aren't 0): its drag, gravity, size, frictions, restitution, length
    // drag, centre of mass, its physics body's substeps, buoyancy, flags, constraint and hinges, and no collisions
    void SetUpTrajectoryBody(Trajectory* trajectory, ObjectRigidBody* body) RETAIL(FUN_00238aa8);
    // The particle trails' emitters started (stopped for a trail without a system) and given their frames, a pickup's or a
    // projectile's (game/nodeparts.cpp)
    void UpdateParticleTrails(ParticleTrails* trails, void* node) RETAIL(FUN_002381a8);
    // A sense of the instances around (a perception's sense of kind 0, game/perception.cpp): its level the instances it notices
    // add up to (falling off with their distance), rising to that at once and falling by 0.3 of its decay otherwise, the
    // direction away from them; the instances it notices (by their objects, and the player when it's asked to) of those of its
    // kinds within its radius (how many), the ones of its kinds within the radius (at most 56), and how much an instance's own
    // perception (slot 2) looks at the node's instance
    void SenseInstances(const void* sense, TimeClock* unused, ObjectNode* node, f32* level, Vector4* direction)
        RETAIL(FUN_00234ac8);
    u32 NoticedInstances(const void* sense, ObjectNode* node, InstanceContext** noticed) RETAIL(FUN_002349c0);
    u32 InstancesInSenseRange(const void* sense, ObjectNode* node, InstanceContext** found, u32 kinds) RETAIL(FUN_002348b8);
    f32 InstanceAttention(const void* sense, ObjectNode* node, InstanceContext* instance) RETAIL(FUN_00234e70);
    // A trail's step (game/particletrails.cpp): whether it emits this time by its kind, its particle system (or its surface's)
    // played from its frame (its emitter started or stopped: the emitter), its decals and surface effects, its sound, its camera
    // shake and its message; its decals left on a frame, and whether its instance's animation went back below its mark
    s32 StepTrail(struct TrailArguments* trail, TimeClock* clock, ObjectNode* node, s32 emitter) RETAIL(FUN_002352b0);
    void LeaveTrailDecals(const struct TrailArguments* trail, const Matrix4x4* frame, struct ChunkData* chunk)
        RETAIL(FUN_00236090);
    u32 TrailAnimationRestarted(const struct TrailArguments* trail, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00236220);
    // The head tracking's parts (game/headtracking.cpp): set up with its settings (its joints and limits unless it steers its
    // instance; its agent's slot 12 says whether the agent turns the head itself) and started; started (its joints hooked on its
    // instance's animator unless the agent turns the head, it tracks and doesn't stop); its head's turns stepped toward a target
    // (or back to rest), kept within their limits; nothing (where it would look around); its target let go of, turning back to
    // rest (and stopping there when asked); a noise heard (taken as its target when it's at least as loud as its weight)
    void SetUpHeadTracking(HeadTracking* tracking, const HeadTrackingSettings* settings, TimeClock* clock, ObjectNode* node)
        RETAIL(FUN_002372f8);
    void StartHeadTracking(HeadTracking* tracking, ObjectNode* node) RETAIL(FUN_00237cc0);
    u32 TurnHead(HeadTracking* tracking, InstanceContext* instance, const Vector4* target) RETAIL(FUN_00236de8);
    u32 HeadTrackingIdle(HeadTracking* tracking, TimeClock* clock, InstanceContext* instance) RETAIL(FUN_00237a60);
    void LetGoOfHeadTarget(HeadTracking* tracking, u32 stops) RETAIL(FUN_00237a70);
    void HearNoise(HeadTracking* tracking, const struct NoiseEvent* noise, ObjectNode* node) RETAIL(FUN_00237b10);
    // A motion block's cycles about x, y and z stepped by a time (the angle kept in 65536ths of a turn): its value, times an
    // amplitude, of the kind its bits give; and a cycle's starting angle of a value within a range (the falling half's, the turn
    // after, when asked)
    f32 StepCycleX(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle) RETAIL_N32(FUN_00236418);
    f32 StepCycleY(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle) RETAIL_N32(FUN_002365a0);
    f32 StepCycleZ(f32 elapsed, f32 amplitude, const MotionBlock* block, s32* angle) RETAIL_N32(FUN_00236728);
    s32* CycleStart(s32* angle, MotionBlock* unused, u32 cycle, u32 falling, f32 value, f32 range) RETAIL_N32(FUN_002362a8);
    // A motion block's sticking: whether an instance sticks to it (a node of one of its kinds, or its object), the instance stuck
    // to the block's node's attachments (held with its strength) and sent its message; and its resets: the cycles, the sticky
    // values and most flags cleared, then the rest of a cycling block's values (its trajectory asking for frames)
    u32 SticksToMotionBlock(const MotionBlock* block, InstanceContext* instance) RETAIL(FUN_00236a48);
    void StickToMotionBlock(MotionBlock* block, InstanceContext* instance) RETAIL(FUN_002368b0);
    void ResetMotionBlock(MotionBlock* block) RETAIL(FUN_00236ab8);
    void MakeCyclingMotionBlock(MotionBlock* block) RETAIL(FUN_00236bd0);
    // Its rigid body stepped (when it moves and the instance isn't held), the node's middle made its collision box's, and the
    // body's contact bits handed on
    void StepMovement(ObjectNode* node, TimeClock* clock) RETAIL(FUN_00230e58);
    // A rigid body's step (game/rigidbodyframe.cpp): its physics body pulled down by its gravity, else its node's velocity
    // pulled down, dragged and gripped and its instance moved by it (the move handed out when asked)
    void StepRigidBody(ObjectRigidBody* body, TimeClock* clock, Vector4* move) RETAIL(FUN_0024bca0);
    // Its frame against the instances around it (its sphere: its node's middle and roll radius; the bodies of its chunk's first
    // list), against its collision cache's triangles (the second list's), and two rigid bodies' spheres pushed apart (whether
    // they touched)
    void CollideRigidBodyWithInstances(ObjectRigidBody* body, Vector4* sphere) RETAIL(FUN_00247490);
    void CollideRigidBodyWithWorld(ObjectRigidBody* body) RETAIL(FUN_00249510);
    u32 CollideRigidBodies(ObjectRigidBody* body, const Vector4* sphere, const Vector4* otherSphere, ObjectRigidBody* other)
        RETAIL(FUN_002498c8);
    // Its rigid body let go once it nearly rests while it touches the world (StepMovement clears that before it asks)
    void ReleaseRigidBodyAtRest(ObjectNode* node) RETAIL(FUN_0023df20);
    // The parts' own releases and destructors
    void LetGoOfTrajectory(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_002409b0);
    void DestroyTrajectory(Trajectory* trajectory, u32 destroyFlags) RETAIL(FUN_002406d8);
    void LetGoOfHeadTracking(HeadTracking* tracking, ObjectNode* node) RETAIL(FUN_0023fd38);
    void DestroyHeadTracking(HeadTracking* tracking, u32 destroyFlags) RETAIL(FUN_0023fcd0);
    void DestroyRigidBody(ObjectRigidBody* body, u32 destroyFlags) RETAIL(FUN_00254030);
    // A chunk's rigid body lists (made the first time), a body taken out of either (whether it was in it), put in (whether there
    // was room; the body takes the lists when it has none)
    ChunkRigidBodies* ChunkRigidBodiesOf(struct ChunkData* chunk) RETAIL(FUN_001f2480);
    u32 TakeFirstRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body) RETAIL(FUN_00252ce0);
    u32 TakeSecondRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body) RETAIL(FUN_00252e68);
    u32 PutFirstRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body) RETAIL(FUN_00252c88);
    u32 PutSecondRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body) RETAIL(FUN_00252e10);
    // The lists made (empty), destroyed and emptied: the indexes of the bodies in them forgotten (game/chunkrigidbodies.cpp)
    ChunkRigidBodies* ConstructChunkRigidBodies(void* memory) RETAIL(FUN_00252bd0);
    // The frame's collisions of the bodies whose nodes were updated (or whose instances are attached to their parents): the first
    // list's (but those its physics body moves) against the instances around them, the second list's against the collision
    void CollideChunkRigidBodies(ChunkRigidBodies* bodies) RETAIL(FUN_002433e8);
    void DestroyChunkRigidBodies(ChunkRigidBodies* bodies, u32 destroyFlags) RETAIL(FUN_00252be0);
    void ReleaseChunkRigidBodies(ChunkRigidBodies* bodies) RETAIL(FUN_00252d50);
    void ReleaseAttachmentsNode(void* attachments, u32 update, u32 remove, u32 clearsLeftOut) RETAIL(FUN_00196ec8);
    // The head tracking's target given with a weight (and remembered to come back to when asked), the tracking started
    void TrackHead(f32 weight, HeadTracking* tracking, InstanceContext* target, ObjectNode* node, u32 remembered)
        RETAIL_N32(FUN_00237db0);
    // An object node's vtable slot 26 (the float first): launched with a velocity under a gravity (DefaultLaunchGravity when
    // it's negative): its rigid body made the first time (in both its chunk's lists, rolling with the reach of its instance's
    // collision box), its motion's velocity the one it had before
    void LaunchNode(f32 gravity, ObjectNode* node, const Vector4* velocity) RETAIL_N32(FUN_0022f440);
    // A node launched with a velocity, a motion block (its trajectory made to follow it) carrying it: its rigid body made again,
    // its physics body given the velocity, its instance set upright (facing along its own z axis) when the block's flag 10 asks,
    // spun about its x axis (or else its y axis) by a speed, AgentRef1 left out of its collisions; and the physics body of a
    // node's rigid body launched (its forces cleared) and spun alike
    void LaunchWithMotion(f32 spinY, f32 spinX, ObjectNode* node, MotionBlock* motion, const Vector4* velocity)
        RETAIL_N32(FUN_00233988);
    void LaunchPhysicsBody(f32 spinY, f32 spinX, ObjectNode* node, const Vector4* velocity) RETAIL_N32(FUN_00233d68);
    // A rigid body made for a node, its gravity (it falls with one), and its kind of motion and of collisions set (0 none: it's
    // put in or taken out of its chunk's first and second lists, its physics body made or let go of; game/commandsphysics.cpp)
    ObjectRigidBody* ConstructRigidBody(void* memory, ObjectNode* node) RETAIL(FUN_002465a0);
    void SetRigidBodyGravity(f32 gravity, ObjectRigidBody* body) RETAIL_N32(FUN_002541e0);
    void ListRigidBodyFirst(ObjectRigidBody* body, u32 kind) RETAIL(FUN_00246c10);
    void ListRigidBodySecond(ObjectRigidBody* body, u32 kind) RETAIL(FUN_00246f70);
    // A rigid body's index in its chunk's first or second list forgotten (the lists too once it's in neither), put out of action
    // (out of the second list, its index in the first forgotten), its physics body stopped, its kind of collisions made none (its
    // collision cache destroyed, out of the second list), and its physics body let go of (kinds of the physics made plain, its
    // instance no longer told of a physics body's collisions unless a motion block moves it)
    void ForgetFirstIndex(ObjectRigidBody* body) RETAIL(FUN_00254150);
    void ForgetSecondIndex(ObjectRigidBody* body) RETAIL(FUN_00254190);
    void DeactivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00254088);
    void StopRigidBody(ObjectRigidBody* body) RETAIL(FUN_002541b8);
    void StopRigidBodyCollisions(ObjectRigidBody* body) RETAIL(FUN_002544f8);
    void ReleasePhysicsBody(ObjectRigidBody* body) RETAIL(FUN_002545c8);
    // A rigid body's values (the float first): no gravity (its node doesn't fall), its restitution, drag, length drag and
    // friction (its physics body's too when it has one), its physics body's spin and roll friction (both the value), mass (sized
    // by its instance's own box) and centre of mass; and its moving and stopping bits cleared
    void ClearRigidBodyGravity(ObjectRigidBody* body) RETAIL(FUN_00254260);
    void SetRigidBodyRestitution(f32 value, ObjectRigidBody* body) RETAIL_N32(FUN_002542a0);
    void SetRigidBodyDrag(f32 value, ObjectRigidBody* body) RETAIL_N32(FUN_002542e0);
    void SetRigidBodyLengthDrag(f32 value, ObjectRigidBody* body) RETAIL_N32(FUN_00254310);
    void SetRigidBodyFriction(f32 value, ObjectRigidBody* body) RETAIL_N32(FUN_00254340);
    void SetRigidBodySpinFriction(f32 value, ObjectRigidBody* body) RETAIL_N32(FUN_00254380);
    void SetRigidBodyMass(f32 mass, ObjectRigidBody* body) RETAIL_N32(FUN_002543a8);
    void SetRigidBodyCenterOfMass(ObjectRigidBody* body, const Vector4* center) RETAIL(FUN_002546f0);
    void RevertColliderMotion(ObjectRigidBody* body) RETAIL(FUN_002546a0);
    // An impact of a strength sent as an event to the instances within a radius of a position in the source's chunk
    void SendImpact(f32 radius, f32 strength, InstanceContext* source, const Vector4* position) RETAIL_N32(FUN_0020a358);
    // Another's keys (its positions, the first, the last and the current key, and the flags of their stepping) and paths (and
    // where it is on its path) taken; the keys past the other's count are none
    void CopyKeys(Waypoints* waypoints, const Waypoints* other) RETAIL(FUN_0020b3f0);
    void CopyPaths(Waypoints* waypoints, const Waypoints* other) RETAIL(FUN_0020b5a0);
    // The position of the route's current step (or of the one before it), moved by five shares of the room the step's radius
    // leaves the node's roll radius (game/waypointroutes.cpp; nine arguments: the asm calls it through its own thunk)
    void RouteStepPosition(Waypoints* waypoints, Vector4* position, ObjectNode* node, u32 current, f32 corner, f32 toward,
                           f32 scatter, f32 sideways, f32 lift) RETAIL_N32(FUN_0020b6e8);
    // An object node's vtable slot 27 (the float first): pushed by another instance: its motion block told (the node following it
    // when it asks, what it rides let go of when it already does), then its rigid body pushed away from the other, the harder the
    // closer
    void PushNode(f32 strength, ObjectNode* node, InstanceContext* other) RETAIL_N32(FUN_00230428);
    // A rigid body given an impulse at a point (its node's velocity or its physics body takes it, by its kind of motion)
    void PushRigidBody(ObjectRigidBody* body, const Vector4* impulse, const Vector4* point) RETAIL(FUN_0024c3f0);
    // A surface's sound and particles of a kind of contact played for a node, as loud as how far past its threshold the contact
    // is (the hard one's, kinds 4 and 5, apart)
    void PlaySurfaceContact(f32 intensity, ObjectNode* node, CollisionSurface* surface, u32 kind, const Vector4* point,
                            const Vector4* velocity) RETAIL_N32(PlaySurfaceContact);
    void PlaySurfaceContactHard(f32 intensity, ObjectNode* node, CollisionSurface* surface, u32 kind, const Vector4* point,
                                const Vector4* velocity) RETAIL_N32(PlaySurfaceContactHard);
    // The runner whose behaviour is being run
    extern BehaviourRunner* g_CurrentRunner RETAIL(G_CurrentScriptCall);
    // The kind 1 nodes' vtables: the prototype's, the agent nodes' base's, the object nodes'
    extern const GccVTableEntry g_NodePrototypeVTable[] RETAIL(InstanceNodePrototype_Methods);
    extern const GccVTableEntry g_ObjectNodeBaseVTable[] RETAIL(D_00301058);
    extern const GccVTableEntry g_ObjectNodeVTable[] RETAIL(InstanceNodeType0_Methods);
    // Set while every chunk is being unloaded
    extern u8 g_UnloadingEverything RETAIL(D_00309FF3);
    // A matrix facing from an instance to another
    void MatrixFacingFrom(InstanceContext* from, InstanceContext* to, Matrix4x4* matrix) RETAIL(FUN_0022e2d0);
    // The place of an exit point of an instance's model (its slot below 63, the model's animator having it): its position and the
    // direction its z axis points, whether there is one
    u32 ExitPointPlace(InstanceContext* instance, u32 slot, Vector4* position, Vector4* direction) RETAIL(FUN_0023d090);
    // The node's velocity (its motion's)
    void CopyVelocity(ObjectNode* node, Vector4* velocity) RETAIL(FUN_0023d8e0);
}
