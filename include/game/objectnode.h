#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/string.h"

class LayoutPath;
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
struct Route;
struct ScriptStarter;
struct TimeClock;

// The script designators an object node answers (the AgentLab tool's): what its instance's ID entry links, its own instance, the
// player, the head tracking's target, the stored position, AgentRef2 and AgentRef1, the route's previous and current step, the
// focus instance and position, the next and the current key
enum Designator : u32
{
    DesignatesLinkedById = 0xDF,
    DesignatesItself = 0xF0,
    DesignatesPlayer = 0xF2,
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
};

// The object instances' nodes (kind 1): the agent's node the behaviour runners drive, and its parts that move the instance along
// the control packets (the AgentLab tool's names for what a packet does are in game/agentlab.h)

// How a control packet's motion goes (made the first time, kept by the node, 0x60 bytes): the time it takes and its inverse, the
// bits (0: the translation is done, 1: the rotation; bit fields the retail code reads and writes as the 64 bits from 8 on, the
// speed's included), the speed, the speed of a chase, the velocity (the one it starts from while it accelerates), the speed of a
// turn and the rotation's change per second
struct MotionState
{
    enum Bits : u32
    {
        TranslationDone = 0x1,
        RotationDone = 0x2,
    };

    f32 duration;
    f32 inverseDuration;
    u32 bits;
    f32 speed;
    f32 chaseSpeed;
    u8 unknown14[0xC];
    Vector4 velocity;
    Vector4 startVelocity;
    f32 turnSpeed;
    u8 unknown44[0xC];
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
    u8 unknown30[0x10];
    Vector4 wander;
    MotionState* motion;
    f32 distance;
    u8 unknown58[8];

    static Translator* Construct(Translator* translator) RETAIL(FUN_0020edd0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020edf8);
    void Reset() RETAIL(FUN_0020ee20);
};
CHECK_OFFSET(Translator, motion, 0x50);
CHECK_SIZE(Translator, 0x60);

// A rotation (0x40 bytes): the motion's state, where it starts and where it goes
struct Rotator
{
    MotionState* motion;
    u8 unknown04[0xC];
    Vector4 start;
    Vector4 target;
    Vector4 unknown30;

    static Rotator* Construct(Rotator* rotator) RETAIL(FUN_0020f9b8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020f9e0);
    void Reset() RETAIL(FUN_0020fa08);
};
CHECK_SIZE(Rotator, 0x40);

// The physics of the motions that aren't straight (0x60 bytes): a velocity, four parameters (a spring's power and damping; a
// chase's duration, power, damping and bounce), the motion's state and bits (1: a spring's deceleration is negative, 2-5 the kind:
// 1 made, 3 a chase; read and written as 64 bits in retail)
struct Physics
{
    enum Bits : u32
    {
        NegativeDeceleration = 0x2,
        KindMask = 0x3C,
        KindMade = 0x4,
        KindChase = 0xC,
    };

    u8 unknown00[0x20];
    Vector4 velocity;
    Vector4 unknown30;
    f32 parameters[4];
    MotionState* motion;
    u32 unknown54;
    u32 bits;
    u32 unknown5C;

    static Physics* Construct(Physics* physics) RETAIL(FUN_0020f878);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0020f8a0);
    void Reset() RETAIL(FUN_0020f8c8);
};
CHECK_OFFSET(Physics, parameters, 0x40);
CHECK_OFFSET(Physics, bits, 0x58);
CHECK_SIZE(Physics, 0x60);

// A block of motion an object node can follow (still asm): its flags (14: a touch or a push makes the node follow it, 15: the
// trajectory controller following it asks for its frame while the node isn't updated)
struct MotionBlock
{
    enum Flags : u32
    {
        FollowedWhenTouched = 0x4000,
        KeepsStepping = 0x8000,
    };

    u8 unknown00[0x6C];
    u32 flags;
    // What becoming sticky gives it: flags, a value, the message and the object
    u32 stickyFlags;
    f32 stickyValue;
    u32 unknown78;
    u32 stickyMessage;
    u16 stickyObject;
    u8 unknown82[2];
    // The node it's the motion block of (springy contacts')
    struct ObjectNode* node;
};
CHECK_OFFSET(MotionBlock, flags, 0x6C);
CHECK_SIZE(MotionBlock, 0x88);

// The node's trajectory controller (still asm): the position and the rotation it holds the instance at, the rotation's angles,
// the motion block it follows, and the angles of its cycles about the three axes (65536ths of a turn)
struct Trajectory
{
    u8 unknown00[0x50];
    Vector4 position;
    Vector4 rotation;
    s32 angles[3];
    u8 unknown7C[0xE0 - 0x7C];
    MotionBlock* followed;
    s32 cycles[3];
    u8 unknownF0[0xC];
    // What the motion floats command sets
    f32 motionFloats[3];
};
CHECK_OFFSET(Trajectory, followed, 0xE0);

// The particle trails an object node leaves (still asm): with bit 5 of its bits the turn a packet facing the way it moves makes is
// measured into the motion's state
struct ParticleTrails
{
    enum Bits : u32
    {
        MeasuresTurn = 0x20,
    };

    u8 unknown00[0x60];
    u32 bits;
    u8 unknown64[0x6C - 0x64];

    static ParticleTrails* Construct(ParticleTrails* trails) RETAIL(FUN_00240240);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002402b8);
    // A trail added (an AddTrail command's arguments): its slot
    u32 Add(const void* arguments) RETAIL(FUN_00240350);
    // The trails of a kind taken away (and the empty slots')
    void RemoveKind(u32 kind) RETAIL(FUN_002404c8);
    // Its instance moving from a chunk through a link
    void ChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_00240648);
};

// The rigid body an object node moves with (still asm): an object (a packet's offset spreads over its box), 64 bits each 0x88 and
// 0x90 bytes in, the physics body it moves as (0xD4 bytes in, one of the world's), and the lists of its chunk's rigid bodies it's
// in with its index in each (none 0xFFFF and 0xFF)
struct ObjectRigidBody
{
    u8 unknown00[0x40];
    // The normal of what it touches
    Vector4 contactNormal;
    u8 unknown50[0x80 - 0x50];
    ReferencedObject* object;
    u8 unknown84[4];
    u64 bits88;
    u64 bits90;
    // What the physics sizes command sets
    f32 sizes[2];
    u8 unknownA0[0xD4 - 0xA0];
    DynamicBody* physicsBody;
    struct ChunkRigidBodies* chunkBodies;
    u16 firstIndex;
    u8 secondIndex;
};
CHECK_OFFSET(ObjectRigidBody, bits88, 0x88);
CHECK_OFFSET(ObjectRigidBody, physicsBody, 0xD4);
CHECK_OFFSET(ObjectRigidBody, secondIndex, 0xDE);
CHECK_SIZE(ObjectRigidBody, 0xE0);

// A chunk's two lists of the rigid bodies in it (0x800 bytes, made the first time a body goes in, still asm): their counts and the
// bodies (taking one out moves the last into its place, a list takes 255)
struct ChunkRigidBodies
{
    u16 secondCount;
    u16 firstCount;
    ObjectRigidBody* first[256];
    ObjectRigidBody* second[255];
};
CHECK_SIZE(ChunkRigidBodies, 0x800);

// The object a node tracks with its head (still asm): its 64 bits at 0xA0, the reference to what it looks at
struct HeadTracking
{
    u8 unknown00[0xA0];
    u64 bits;
    u8 unknownA8[8];
    Reference* target;
};
CHECK_OFFSET(HeadTracking, target, 0xB0);

// The positions and the paths an object instance names (0x50 bytes), the route it follows: its keys are its positions (the key
// it's at, the first and the last), the path it's on (its direction and how far along it the instance is), the route's step
// it's at (0xFF none) and the path that led there; the flags (bit 0: the keys go backwards, bit 1: they don't go on, bit 3: they
// went round) and the counts are the bytes of a 64 bit word in retail
struct Waypoints
{
    enum Flags : u8
    {
        FlagBackwards = 0x1,
        FlagStopped = 0x2,
        FlagWrapped = 0x8,
    };

    PointerArray<LayoutPosition> positions;
    PointerArray<LayoutPath> paths;
    Vector4 pathDirection;
    f32 pathParameter;
    Route* route;
    AiPath* routePath;
    u8 routeIndex;
    u8 unknown3D[3];
    u8 flags;
    u8 keyCount;
    u8 pathCount;
    u8 pathIndex;
    u8 key;
    u8 firstKey;
    u8 lastKey;
    u8 unknown47;
    u8 unknown48[8];

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

// The base of the kind 1 nodes (0xD0 bytes, vtable D_00301058 over InstanceNodePrototype_Methods): what it focuses on (an instance
// with flag 0, a position with flag 1), its object, its properties and agent, its flags (0: a focus instance, 1: a focus position,
// 2: springs stay level, 4: a step fell short, 13: a stored position, 18: its motion moves the stored place, 20: AgentRef2 kept while asleep, 24: it moves, 25: it
// accelerates), the node it takes its object and
// properties from when there's one, its two behaviour runners and the instance its packet tracks
struct ObjectNodeBase : GameNode
{
    enum Flags : u32
    {
        FlagFocusInstance = 0x1,
        FlagFocusPosition = 0x2,
        // A spring doesn't pull it up or down; a step left it short of its target
        FlagLevel = 0x4,
        FlagUnsettled = 0x10,
        FlagHandledEvent = 0x20,
        // What a runner finishing leaves: its particles (but the trails of kind 1), its trajectory controller, its perception
        FlagKeepsParticles = 0x100,
        FlagKeepsTrajectory = 0x200,
        FlagStoredPosition = 0x2000,
        FlagMovesStoredPlace = 0x40000,
        FlagKeepsAgentRef2 = 0x100000,
        FlagKeepsPerception = 0x400000,
        // Put back where it was before its frame unless it moved (the playable characters')
        FlagPinned = 0x800000,
        FlagMoves = 0x1000000,
        FlagAccelerates = 0x2000000,
    };

    u8 unknown18[8];
    Vector4 unknown20;
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
    u8 unknown7C[4];
    PropertyHolder* properties;
    Agent* agent;
    u32 flags;
    u8 unknown8C;
    u8 unknown8D;
    // The last trigger message, who sent it and when (on its instance's clock)
    u16 message;
    InstanceContext* messageSender;
    u32 messageTime;
    // The node it takes its object and properties from when it has one
    GameNode* sourceNode;
    u32 unknown9C;
    BehaviourRunner* runners[2];
    u32 unknownA8;
    InstanceContext* tracked;
    Vector4 unknownB0;
    Vector4 unknownC0;

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

    // Its vtable's slot 1 (the prototype's as well): an event handled, its handle let go: a trigger message (0x103) kept with
    // its sender and time and its object's behaviour for it started, 0x100 and 0x101 applied to it with the game's resources
    void HandleEvent(Reference** event) RETAIL(FUN_0022ef00);
    // Slot 18: a starter queued on the runner of a slot, or the runner made with it (taken at its next frame)
    u32 StartBehaviour(ScriptStarter* starter, InstanceContext* originator, u32 force, u32 slot) RETAIL(FUN_0023eb88);
    // Slot 19: the behaviour its object starts for a trigger message
    void OnTriggerMessage(u32 message) RETAIL(FUN_002346b8);
    // Slot 21: its runners stopped and destroyed
    void StopRunners(u32 release) RETAIL(FUN_0023ec28);

    // The prototype's defaults, which its own table and the base's keep: 5 its kind (an object's), 9, 11, 22, 24, 26, 27 and 33-35
    // nothing, 23 no particle (0xFF), 28 yes, 29-31 and 36-40 no; the base's 41 no; the prototype's own 18 no, 19 and 21 nothing
    u32 Kind() RETAIL(GetNodeIndex_0023C7A8);
    void DefaultSlot9() RETAIL(FUN_0023c850);
    void DefaultSlot11() RETAIL(FUN_0023c7b0);
    void DefaultSlot22() RETAIL(FUN_0023c7e8);
    u32 DefaultSlot23() RETAIL(FUN_0023c7f0);
    void DefaultSlot24() RETAIL(FUN_0023c7f8);
    void DefaultSlot26() RETAIL(FUN_0023c808);
    void DefaultSlot27() RETAIL(FUN_0023c810);
    u32 DefaultSlot28() RETAIL(FUN_0023c818);
    u32 DefaultSlot29() RETAIL(FUN_0023c820);
    u32 DefaultSlot30() RETAIL(FUN_0023c828);
    u32 DefaultSlot31() RETAIL(FUN_0023c830);
    void DefaultSlot33() RETAIL(FUN_0023c838);
    void DefaultSlot34() RETAIL(FUN_0023c840);
    void DefaultSlot35() RETAIL(FUN_0023c848);
    u32 DefaultSlot36() RETAIL(FUN_0023c8e8);
    u32 DefaultSlot37() RETAIL(FUN_0023c8f0);
    u32 DefaultSlot38() RETAIL(FUN_0023c8f8);
    u32 DefaultSlot39() RETAIL(FUN_0023c900);
    u32 DefaultSlot40() RETAIL(FUN_0023c908);
    u32 DefaultSlot41() RETAIL(FUN_0023c930);
    u32 PrototypeSlot18() RETAIL(FUN_0023c7c8);
    void PrototypeSlot19() RETAIL(FUN_0023c7d0);
    void PrototypeSlot21() RETAIL(FUN_0023c7e0);

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

// An object instance's node (0x180 bytes, vtable InstanceNodeType0_Methods): the rotation between its keys, a stored position,
// its motion's parts, its trajectory controller and head tracking, its AgentRef1 and AgentRef2, its waypoints and its motion's
// state
struct ObjectNode : ObjectNodeBase
{
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
    void* perception;
    u32 unknown114;
    InstanceContext* agentRef1;
    InstanceContext* agentRef2;
    MotionBlock* motionBlock;
    Waypoints* waypoints;
    MotionState* motion;
    ObjectRigidBody* rigidBody;
    s32 surface;
    s32 unknown134;
    u8 unknown138[8];
    Vector4 unknown140;
    u32 unknown150;
    // A countdown the movement step lowers while the node isn't pinned (bit fields of the 64 bits from 0x150 in retail)
    u8 unknown154;
    u8 unknown155[0x180 - 0x155];

    // Made for a chunk's instance (with waypoints when asked), and destroyed
    static ObjectNode* Construct(ObjectNode* node, struct ChunkEntry* chunk, u32 waypoints, u32 unused) RETAIL(InitInstanceNodeType0);
    void Initialise(u32 waypoints) RETAIL(FUN_0022f2e8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0023d5e8);
    // No stored position
    void ForgetStoredPosition() RETAIL(FUN_0023e7a0);
    // Its rigid body, made the first time it's asked for
    ObjectRigidBody* RigidBody() RETAIL(FUN_0023e608);
    // Its vtable's slot 35 called unless everything's being unloaded, and the part 0x114 bytes in told (slot 5) and destroyed
    void ReleaseLinks() RETAIL(FUN_0023d668);
    void DestroyPart114() RETAIL(FUN_0023e4a8);

    // Its vtable's small functions: 3 given its instance (its start taken from the instance's place), 6 its instance left its
    // chunk (the comeback placement forgotten for the first two reasons), 9 slot 11 called, 10 its item type, 14 and 15 (it takes
    // packets) yes, 16 and 17 no, 20 nothing, 25 none (0xFF), 33 nothing, 40
    // whether it has no byte 0x160 bytes in, 41 yes, 43 nothing, 44 its sound (the byte 0x158 bytes in) stopped
    void SetOwner(InstanceContext* instance) RETAIL(FUN_0023d750);
    void LeftChunk(u32 why) RETAIL(FUN_0023d300);
    void CallSlot11() RETAIL(FUN_0023d6a0);
    u32 ItemType() RETAIL(FUN_0023c7a0);
    u32 Slot14() RETAIL(FUN_0023c940);
    u32 TakesPackets() RETAIL(FUN_0023c948);
    u32 Slot16() RETAIL(FUN_0023c7b8);
    u32 Slot17() RETAIL(FUN_0023c7c0);
    void Slot20() RETAIL(FUN_0023c7d8);
    u32 Slot25() RETAIL(FUN_0023c800);
    void Slot33() RETAIL(FUN_0023ddd0);
    u32 HasNoByte160() RETAIL(FUN_0023c998);
    u32 Slot41() RETAIL(FUN_0023c950);
    void Slot43() RETAIL(FUN_0023e118);
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
    // Slot 7 (the game node's step, once its instance starts again): the part 0x114 bytes in told, its parts let go, its runners
    // stopped, made as new, its instance put back where it started; the middle of the instance's box and its position kept
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
    // Its physics body touched water (a triangle not solid to objects; still asm)
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
    // (every frame while bits 5 or 6 of its flags are set), its parts and its behaviour runners step and it moves; else only its
    // trajectory (when its controller asks) and the part 0x12C bytes in. The base's update after
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
CHECK_OFFSET(ObjectNodeBase, unknown20, 0x20);
CHECK_OFFSET(ObjectNodeBase, information, 0x40);
CHECK_OFFSET(ObjectNodeBase, ownInformation, 0x70);
CHECK_OFFSET(ObjectNodeBase, informationPointer, 0x74);
CHECK_OFFSET(ObjectNodeBase, unknown7C, 0x7c);
CHECK_OFFSET(ObjectNodeBase, properties, 0x80);
CHECK_OFFSET(ObjectNodeBase, agent, 0x84);
CHECK_OFFSET(ObjectNodeBase, unknown8C, 0x8c);
CHECK_OFFSET(ObjectNodeBase, message, 0x8e);
CHECK_OFFSET(ObjectNodeBase, messageSender, 0x90);
CHECK_OFFSET(ObjectNodeBase, messageTime, 0x94);
CHECK_OFFSET(ObjectNodeBase, sourceNode, 0x98);
CHECK_OFFSET(ObjectNodeBase, tracked, 0xac);
CHECK_OFFSET(ObjectNodeBase, unknownB0, 0xb0);
CHECK_OFFSET(ObjectNodeBase, unknownC0, 0xc0);
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
CHECK_OFFSET(ObjectNode, unknown114, 0x114);
CHECK_OFFSET(ObjectNode, agentRef1, 0x118);
CHECK_OFFSET(ObjectNode, agentRef2, 0x11c);
CHECK_OFFSET(ObjectNode, motionBlock, 0x120);
CHECK_OFFSET(ObjectNode, rigidBody, 0x12c);
CHECK_OFFSET(ObjectNode, surface, 0x130);
CHECK_OFFSET(ObjectNode, unknown134, 0x134);
CHECK_OFFSET(ObjectNode, unknown140, 0x140);
CHECK_OFFSET(ObjectNode, unknown150, 0x150);
CHECK_OFFSET(ObjectNode, unknown154, 0x154);

extern "C"
{
    // The properties of a node that has them (still asm)
    PropertyHolder* GetPropsHolderFromInstanceNode(GameNode* node) RETAIL(GetPropsHolderFromInstanceNode);

    // The rotation facing from a key to the next (along x and z; none when they're above each other)
    void KeyRotation(const LayoutPosition* from, const LayoutPosition* to, Vector4* rotation) RETAIL(FUN_0020f7b0);
    // A packet's motion started: its time and end, what it goes to (a receiver, a key, a route's step, the focus, AgentRef1 or 2,
    // the player, the head tracking's target or a position) and its translation, rotation and physics set up
    void StartPacketMotion(BehaviourRunner* runner, TimeClock* clock) RETAIL(FUN_0020cd10);
    // Where a translation goes and where a rotation turns to (still asm)
    void SetTranslationTarget(Translator* translator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step,
                              InstanceContext* instance, Vector4* position) RETAIL(FUN_0020a728);
    void SetRotationTarget(Rotator* rotator, BehaviourRunner* runner, LayoutPosition* key, AiPosition* step, InstanceContext* instance)
        RETAIL(FUN_0020c2a0);
    // A projectile's physics from the packet (its power given; still asm): its velocity to the translation's direction
    void SetUpProjectile(f32 power, Physics* physics, ControlPacket* packet, PropertyHolder* properties, Vector4* direction)
        RETAIL_N32(FUN_0020be50);
    // A packet's frame: the trajectory holds the instance, the target follows what it tracks, the rotation and the translation
    // step (an interpolation steps both), the instance faces the way it moves or rolls along its natural axes, and the packet
    // ends once both are done (else it's checked for its delay or sync)
    void PacketFrame(BehaviourRunner* runner, TimeClock* clock) RETAIL(FUN_0020e258);
    // The packet ended once it doesn't stall and its delay is over, or once the focus instance's runners left the sync unit's
    // state. Without a focus instance the retail code reads the nodes of what the caller left in a3 (an uninitialized variable)
    void CheckPacketEnd(BehaviourRunner* runner, TimeClock* clock, u32 unused, InstanceContext* leftover) RETAIL(FUN_0020eab8);
    // Whether one of a node's runners has a level that entered the state and left it (still asm)
    u32 LeftSyncState(GameNode* node, u32 state) RETAIL(FUN_002347b0);
    // The trajectory controller holding the instance at its position and rotation (still asm)
    u32 HoldTrajectory(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023b8d0);
    // The translation's target following what it tracks, an interpolation's step, the rotation's and the translation's steps
    // (whether they're done; still asm)
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
    // The instance turned the way it rolls along its natural x or y axis over the distance it moved (still asm)
    void RollAlongX(f32 radius, ObjectNode* node, Vector4* moved) RETAIL_N32(FUN_0022ea88);
    void RollAlongY(f32 radius, ObjectNode* node, Vector4* moved) RETAIL_N32(FUN_0022e8c8);
    // The instance moved to a position and turned to a rotation (queued when it changed), or its stored place instead (unless the
    // packet's space is the stored one) when the node's motion moves it
    void MoveInstance(ObjectNode* node, ControlPacket* packet, const Vector4* position) RETAIL(FUN_0023ca10);
    void TurnInstance(ObjectNode* node, ControlPacket* packet, const Vector4* rotation) RETAIL(FUN_0023cbf8);
    // A spring's physics stepped by a force: the motion's velocity kept as it was and moved by the force, which a damping of 0 or
    // more slows by the velocity, while a negative one caps the speed at its size
    void StepSpringPhysics(f32 elapsed, f32 damping, Physics* physics, const Vector4* force) RETAIL_N32(FUN_0020c0b0);
    // The motions' translation steps (still asm): a spring, a throw, the chases
    u32 StepSpring(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet) RETAIL_N32(FUN_002329f0);
    u32 StepProjectile(f32 elapsed, ObjectNode* node, BehaviourRunner* runner, ControlPacket* packet) RETAIL_N32(FUN_00232728);
    u32 StepGroundChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231160);
    // A chase's speed slowed by the turn it made (by its turn drag, at most by 90%), kept as the motion's speed
    f32 TurnSlowedSpeed(f32 turn, Physics* physics, MotionState* motion) RETAIL_N32(FUN_0020f950);
    // An instance's facing turned toward a target over the ground by a share of the way there (all of it at most), leaning into
    // the turn by a factor (in degrees) up to a limit; and turned toward a target in space, its up leaning toward the target by a
    // factor, or kept (or made from the facing alone when asked): how much it turned (1 - the cosine)
    f32 SteerTowards(f32 share, f32 lean, f32 mostLean, InstanceContext* instance, const Vector4* target) RETAIL_N32(FUN_0022d4f8);
    f32 SteerBodyTowards(f32 share, f32 lean, InstanceContext* instance, const Vector4* target, u32 facingOnly) RETAIL_N32(FUN_0022cac0);
    // A rigid body righted toward an up direction over a time, and a move kept within what holds it (still asm)
    u32 RightRigidBody(f32 elapsed, ObjectRigidBody* body, const Vector4* up) RETAIL_N32(FUN_0024b808);
    void HoldRigidBodyMove(ObjectRigidBody* body, Vector4* move) RETAIL(FUN_00254940);
    u32 StepAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231580);
    u32 StepRiddenAirChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231840);
    u32 StepLastChase(f32 elapsed, ObjectNode* node, BehaviourRunner* runner) RETAIL_N32(FUN_00231cc8);
    // The node's parts' frames (still asm): its particle trails, trajectory controller (and its frame once the runners are done),
    // head tracking and perception, and its movement
    void StepParticleTrails(ParticleTrails* trails, TimeClock* clock, ObjectNode* node) RETAIL(FUN_002403d8);
    void StepTrajectory(Trajectory* trajectory, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00238ed8);
    // A trajectory started again from where its instance is (still asm)
    void RestartTrajectory(Trajectory* trajectory) RETAIL(FUN_0023a290);
    void TrajectoryFrame(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023ae68);
    void StepHeadTracking(HeadTracking* tracking, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00237448);
    void StepPerception(void* perception, TimeClock* clock, ObjectNode* node) RETAIL(FUN_00238050);
    // Its rigid body stepped (when it moves and the instance isn't held), the node's middle made its collision box's, and the
    // body's contact bits handed on
    void StepMovement(ObjectNode* node, TimeClock* clock) RETAIL(FUN_00230e58);
    void StepRigidBody(ObjectRigidBody* body, TimeClock* clock, u32 unknown) RETAIL(FUN_0024bca0);
    void RigidBodyEvent54(ObjectNode* node) RETAIL(FUN_0023df20);
    // The parts' own releases and destructors (still asm)
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
    void ReleaseAttachmentsNode(void* node, u32 first, u32 second, u32 third) RETAIL(FUN_00196ec8);
    // The head tracking's target given with a weight (and lingering when asked)
    void TrackHead(f32 weight, HeadTracking* tracking, InstanceContext* target, ObjectNode* node, u32 lingers) RETAIL_N32(FUN_00237db0);
    // An object node's vtable slot 26 (the float first): launched with a velocity under a gravity (30 when it's negative): its
    // rigid body made the first time (in both its chunk's lists, rolling with the reach of its instance's collision box), its
    // motion's velocity the one it had before
    void LaunchNode(f32 gravity, ObjectNode* node, const Vector4* velocity) RETAIL_N32(FUN_0022f440);
    // A rigid body made for a node, its gravity (it falls with one), and it put in or taken out of its chunk's lists
    ObjectRigidBody* ConstructRigidBody(void* memory, ObjectNode* node) RETAIL(FUN_002465a0);
    void SetRigidBodyGravity(f32 gravity, ObjectRigidBody* body) RETAIL_N32(FUN_002541e0);
    void ListRigidBodyFirst(ObjectRigidBody* body, u32 listed) RETAIL(FUN_00246c10);
    void ListRigidBodySecond(ObjectRigidBody* body, u32 listed) RETAIL(FUN_00246f70);
    // An impact of a strength sent as an event to the instances within a radius of a position in the source's chunk
    void SendImpact(f32 radius, f32 strength, InstanceContext* source, const Vector4* position) RETAIL_N32(FUN_0020a358);
    // An object node's vtable slot 27 (the float first): pushed by another instance: its motion block told (the node following it
    // when it asks, what it rides let go of when it already does), then its rigid body pushed away from the other, the harder the
    // closer
    void PushNode(f32 strength, ObjectNode* node, InstanceContext* other) RETAIL_N32(FUN_00230428);
    // A rigid body given an impulse at a point
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
    // A matrix facing from an instance to another (still asm)
    void MatrixFacingFrom(InstanceContext* from, InstanceContext* to, Matrix4x4* matrix) RETAIL(FUN_0022e2d0);
}
