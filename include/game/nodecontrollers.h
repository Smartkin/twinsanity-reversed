#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/characters.h"
#include "game/math.h"

struct CollisionSurface;
struct InstanceContext;
struct ObjectNode;
struct ObjectPlace;
struct String;
struct TimeClock;

// What command 645 (CreateNodeController) gives an object node, the part it keeps 0x114 bytes in (ObjectNode::controller): its
// kind (the command's), its node and its vtable (the base's D_002F03B0, whose functions do nothing): 1 the destructor, 2 started
// once the node has it, 3 its frame (the node's update, with the clock), 4 the node restarted (the word it's given), 5 stopped
// before it's destroyed or replaced
class NodeController
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        StartSlot = 2,
        FrameSlot = 3,
        RestartSlot = 4,
        StopSlot = 5,
    };

    enum Kind : u8
    {
        KindJointAim = 0,
        KindMask = 1,
        KindSpline = 2,
        KindSkate = 3,
    };

    u8 kind;
    u8 unused01[3];
    ObjectNode* node;
    const GccVTableEntry* vtable;

    // The base's functions (the derived classes' destructors end the way its destructor does)
    void Destroy(u32 destroyFlags) RETAIL(FUN_00122ef0);
    void Start() RETAIL(FUN_00122f20);
    void Frame(TimeClock* clock) RETAIL(FUN_00122f28);
    void Restart(u32 word) RETAIL(FUN_00122f30);
    void Stop() RETAIL(FUN_00122f38);
};
CHECK_SIZE(NodeController, 0xC);

// A particle trail's bits (the AddTrail command's two words): its kind (TrailArguments::Kind), the frame's space (0 its exit
// point's, 1 where its instance started, 2 its instance's place, else the origin), how the frame turns (turned and axes:
// OrientParticleFrame), the sound's group (1 group 1, else 0), kind 1's interval and kind 3's spacing by the spacing (kind 1's
// spacing over its speed, kind 3 at most once a spacing), the pitch scaled by the threshold, a camera shake, a volume, a pitch, a
// random pitch and an offset given, the kind of the contact with the surface its node touches (0xFF none: its own system and
// sounds), how many of its sound slots it picks from, the trails taken away together (ParticleTrails::RemoveKind: 1 a runner's,
// which its end takes away when its node keeps its particles), the emitter given a gravity frame, how many decals it leaves and
// no surface sound. Bits 4-9 and 14-17 are 1 when it's made, which nothing reads
union TrailBits
{
    // The bits making a trail keeps as they were
    static constexpr u64 KeptOnMaking = 0xF8000000E0000000;

    u64 value;
    struct
    {
        u64 kind : 4;
        u64 unused4 : 6;
        u64 space : 4;
        u64 unused14 : 4;
        u64 turned : 1;
        u64 axes : 4;
        u64 soundGroup : 3;
        u64 bySpacing : 1;
        u64 pitchByThreshold : 1;
        u64 unused28 : 4;
        u64 shakesCamera : 1;
        u64 unused33 : 1;
        u64 volumeGiven : 1;
        u64 pitchGiven : 1;
        u64 randomPitch : 1;
        u64 offsetGiven : 1;
        u64 contact : 8;
        u64 soundCount : 4;
        u64 removalKind : 3;
        u64 gravityFrame : 1;
        u64 decalCount : 4;
        u64 noSurfaceSound : 1;
        u64 unused59 : 5;
    };
};
CHECK_SIZE(TrailBits, 8);

// A particle trail's arguments (0x70 bytes, the AddTrail command's from 0x10 bytes in; game/particletrails.cpp steps it): its
// bits, the surface its node has to touch for it to play (-1 any), the particle system it plays (0xFFFF none), the sound slots of
// its node's object it plays one of (a ninth after them, and a count past 9 reads the halfwords after that), the message its
// instance is sent (0xFFFF none), the exit point it leaves from, what its kind compares with (a squared speed or change of
// velocity, or a strength; the pitch's scale with pitchByThreshold), kind 1's interval and its random extra, the camera shake's
// strengths along x and y (z the first's) or its strength, and its falloff, the sound's volume, random pitch and pitch, the
// animation progress kind 7 starts again below, the strength kind 8 asks for (the sound grows louder past it), the spacing (the
// interval of a speed of 1 with bySpacing, kind 3's interval, the decals' spread) and the offset of its frame
struct TrailArguments
{
    // When it emits: every step, every interval (a random extra on top; by its spacing over its speed once it moves), while it
    // moves faster than a threshold (slower than the threshold's negative), while its velocity changed more than that, the same
    // of a vector the retail code never sets, once when its instance's animation starts again (its surface's effects then too),
    // while its trails are stronger than a strength; never else
    enum Kind : u32
    {
        KindAlways = 0,
        KindTimed = 1,
        KindFaster = 3,
        KindChanging = 4,
        KindUnsetVector = 5,
        KindAnimation = 7,
        KindStronger = 8,
    };

    TrailBits bits;
    s32 surface;
    u16 system;
    u16 sounds[8];
    u16 extraSound;
    u16 message;
    u8 exitPoint;
    u8 unused23;
    f32 threshold;
    f32 interval;
    f32 randomInterval;
    u32 unused30;
    f32 shakeX;
    f32 shakeY;
    f32 shakeStrength;
    u32 unused40;
    f32 shakeFalloff;
    f32 volume;
    f32 randomPitch;
    f32 pitch;
    f32 progress;
    f32 strength;
    f32 spacing;
    f32 offset[4];
};
CHECK_OFFSET(TrailArguments, system, 0xC);
CHECK_OFFSET(TrailArguments, extraSound, 0x1E);
CHECK_OFFSET(TrailArguments, message, 0x20);
CHECK_OFFSET(TrailArguments, exitPoint, 0x22);
CHECK_OFFSET(TrailArguments, threshold, 0x24);
CHECK_OFFSET(TrailArguments, volume, 0x48);
CHECK_OFFSET(TrailArguments, offset, 0x60);
CHECK_SIZE(TrailArguments, 0x70);

extern "C"
{
    // Made with no system, sounds, message or kind, every value 0 but the volume and the pitch (1), the exit point 0x3F
    TrailArguments* ConstructTrailArguments(TrailArguments* trail) RETAIL(FUN_00235060);
}

// A joint a JointAimer turns (0xA0 bytes): its node; whether it's turning back to its animation's pose (resting once it's there),
// resting on it, taking the pose as the matrix it's at and the inverse of its own matrix again at its next pose; its matrix's
// scale and the share of the way it turns (times 20 a second: 0 it stays, 1 at once); its joint's ID (0xFF none), the matrix
// it's at in the world (its turn) and that inverse
struct AimedJoint
{
    ObjectNode* node;
    u8 returning;
    u8 resting;
    u8 retakesMatrix;
    u8 retakesInverse;
    f32 scale;
    f32 rate;
    u8 id;
    u8 unused11[0xF];
    Matrix4x4 matrix;
    Matrix4x4 inverse;
};
CHECK_OFFSET(AimedJoint, matrix, 0x20);
CHECK_OFFSET(AimedJoint, inverse, 0x60);
CHECK_SIZE(AimedJoint, 0xA0);

// The joint hook of a JointAimController (vtable D_002EE698): up to three joints of its node's model turned toward a target, an
// instance or a position
class JointAimer : public JointHook
{
public:
    enum Target : u32
    {
        TargetNone = 0,
        TargetPosition = 1,
        TargetInstance = 2,
    };

    ObjectNode* node;
    InstanceContext* targetInstance;
    u8 unused0C[4];
    Vector4 targetPosition;
    // Not set by the constructor
    u32 target;
    u8 unused24[0xC];
    AimedJoint joints[3];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0011de20);
    void Attach(OgiAnimator* animator) RETAIL(FUN_00121848);
    void Detach(OgiAnimator* animator) RETAIL(FUN_001218d0);
    u32 PoseJoint(JointAnimator* animator, Matrix4x4* matrix) RETAIL(FUN_001160e0);
    // Its callbacks taken off its node's model (once it has a node)
    void DetachFromModel() RETAIL(FUN_001217f0);
};
CHECK_OFFSET(JointAimer, targetPosition, 0x10);
CHECK_OFFSET(JointAimer, joints, 0x30);
CHECK_SIZE(JointAimer, 0x210);

// Kind 0 (0x220 bytes, vtable D_002EE660): the final boss's weapons, which commands 641 to 644 and 653 set up. It's made by the
// command itself and only its restart does something: its joint hook taken off the model
class JointAimController : public NodeController
{
public:
    u8 unused0C[4];
    JointAimer aimer;

    static JointAimController* Construct(JointAimController* controller, ObjectNode* node, u32 kind);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011de78);
    void Start() RETAIL(FUN_0011deb8);
    void Frame(TimeClock* clock) RETAIL(FUN_0011dec0);
    void Restart(u32 word) RETAIL(FUN_0011dec8);
    void Stop() RETAIL(FUN_0011dee8);
};
CHECK_OFFSET(JointAimController, aimer, 0x10);
CHECK_SIZE(JointAimController, 0x220);

// A mask controller's flags: bit 0 (set when the character it found was Cortex, Nina or the Mecha-Bandicoot, cleared for Crash
// and the tall one, kept without a character; nothing reads it), the mask is on the character, how many IDs command 655 gave,
// hidden while the game is watched, and bit 7 (whether the character was visible when it was hidden; nothing reads it)
union MaskControllerFlags
{
    u8 value;
    struct
    {
        u8 unused0 : 1;
        u8 onCharacter : 1;
        u8 idCount : 4;
        u8 hidden : 1;
        u8 unused7 : 1;
    };
};
CHECK_SIZE(MaskControllerFlags, 1);

// Kind 1 (0x6E0 bytes, vtable D_002EE568): Aku Aku's mask, its node's instance, following the character its ID entry links while
// that's the player, by the player's hit points: it comes down from above the character (2 or more), follows it (2), shines (3
// or more, its boost trail on), flies onto the character while it's invincible (its three trails on the character), and goes
// back up and away (fewer than 2). Hidden while the game is watched (the title, the watching state). Command 655 gives its trails'
// particle systems
class MaskController : public NodeController
{
public:
    enum State : u8
    {
        StateInactive = 0,
        StateArriving = 1,
        StateLeaving = 2,
        StateGotMask = 3,
        StateGotBoostedMask = 4,
        StateInvincible = 5,
    };

    // Its trails (the invincibility's are the first)
    static constexpr u32 TrailCount = 12;
    static constexpr u32 InvincibilityTrails = 3;

    u8 state;
    MaskControllerFlags flags;
    u8 unused0E[2];
    u16 ids[3];
    u8 unused16[0xA];
    TrailArguments boostTrail;
    TrailArguments unusedTrail;
    // Only the first three are used, the invincibility's (with their emitters' slots)
    TrailArguments trails[TrailCount];
    s32 trailSlots[4];
    u8 unused650[0x20];
    InstanceContext* character;
    // How far through arriving, leaving or flying onto the character it is, and the distance it does it over
    f32 progress;
    f32 distance;
    u8 unused67C[4];
    // Where it follows the character in the character's space, where it leaves to and the character's exit point it flies to
    Vector4 offset;
    Vector4 leavePoint;
    Matrix4x4 exitPoint;

    static MaskController* Construct(MaskController* mask, ObjectNode* node, u32 kind) RETAIL(FUN_00116cb8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011df88);
    void Start() RETAIL(FUN_00121938);
    void Frame(TimeClock* clock) RETAIL(FUN_00116fe0);
    void Restart(u32 word) RETAIL(FUN_001219c0);
    void Stop() RETAIL(FUN_001219c8);
    // The state's name and the player's hit points ("ARRIVING HP 2"; nothing calls it)
    void Describe(String* text) RETAIL(FUN_00116ea8);

    // The character its instance's ID entry links taken (and what its int property 0 says)
    void FindCharacter() RETAIL(FUN_00116de0);
    // Its node's trails destroyed, its instance not drawn nor told triggers' signals
    void Hide() RETAIL(FUN_00121968);
    // The states' frames
    void StepInactive() RETAIL(FUN_00121a58);
    void StepArriving() RETAIL(FUN_001173f8);
    void StepLeaving() RETAIL(FUN_001188c8);
    void StepGotMask() RETAIL(FUN_00118280);
    void StepGotBoostedMask() RETAIL(FUN_00118ce8);
    void StepInvincible() RETAIL(FUN_00117a48);
    // Going from one state to another
    void Arrive() RETAIL(FUN_001171d8);
    void Arrived() RETAIL(FUN_00121af0);
    void Leave() RETAIL(FUN_00118798);
    void Boost() RETAIL(FUN_00121b40);
    void Unboost() RETAIL(FUN_00121bc0);
    void BecomeInvincible() RETAIL(FUN_00117858);
    void EndInvincibility() RETAIL(FUN_00118148);
    // Kept at its place by the character
    void Follow() RETAIL(FUN_00118348);
};
CHECK_OFFSET(MaskController, boostTrail, 0x20);
CHECK_OFFSET(MaskController, trails, 0x100);
CHECK_OFFSET(MaskController, trailSlots, 0x640);
CHECK_OFFSET(MaskController, character, 0x670);
CHECK_OFFSET(MaskController, offset, 0x680);
CHECK_OFFSET(MaskController, exitPoint, 0x6A0);
CHECK_SIZE(MaskController, 0x6E0);

// Kind 2 (0x30 bytes, vtable D_002EE530): its instance moved along its node's first path to stay a distance (the offset's z) from
// where the player is nearest the path, faster the further it is (by the pull), and steered toward the path's way (lowered by the
// drop) at a rate; command 658 sets them. The offset's x and y and the word at 0x28 (command 658 sets it, the constructor
// doesn't) aren't used
class SplineController : public NodeController
{
public:
    u8 unused0C[4];
    Vector4 offset;
    f32 pull;
    f32 turnRate;
    f32 unused28;
    f32 drop;

    static SplineController* Construct(SplineController* spline, ObjectNode* node, u32 kind) RETAIL(FUN_00121eb8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011e038);
    void Start() RETAIL(FUN_00121f28);
    void Frame(TimeClock* clock) RETAIL(FUN_00118db8);
    void Restart(u32 word) RETAIL(FUN_00121f30);
    void Stop() RETAIL(FUN_00121f38);
};
CHECK_OFFSET(SplineController, offset, 0x10);
CHECK_OFFSET(SplineController, drop, 0x2C);
CHECK_SIZE(SplineController, 0x30);

// A skate controller's counts of the IDs and the sounds command 661 gave
union SkateControllerCounts
{
    u8 value;
    struct
    {
        u8 idCount : 4;
        u8 soundCount : 4;
    };
};
CHECK_SIZE(SkateControllerCounts, 1);

// A skate controller's flags: paused while the game is watched, the player's skate grinds this frame and the last
union SkateControllerFlags
{
    u8 value;
    struct
    {
        u8 paused : 1;
        u8 grinding : 1;
        u8 wasGrinding : 1;
        u8 unused3 : 5;
    };
};
CHECK_SIZE(SkateControllerFlags, 1);

// Kind 3 (0x3F0 bytes, vtable D_002EE4B8): the Humiliskate's trails and sounds (command 661 gives their IDs, the vehicle's code
// uses them), which keeps whether the player's skate grinds this frame and the last; paused while the game is watched
class SkateController : public NodeController
{
public:
    static constexpr u32 TrailCount = 8;

    u8 unused0C;
    SkateControllerCounts counts;
    SkateControllerFlags flags;
    u8 unused0F;
    u16 ids[TrailCount];
    u16 sounds[13];
    u8 unused3A[6];
    TrailArguments trails[TrailCount];
    s32 trailSlots[TrailCount];
    // The surface the character skates on (game/commandscharacters.cpp's SetCharacterSurface sets it): while there's none the
    // node's sound is stopped every frame
    CollisionSurface* surface;
    // Cleared when it starts, never read
    u32 unused3E4;
    u8 unused3E8[8];

    static SkateController* Construct(SkateController* skate, ObjectNode* node, u32 kind) RETAIL(FUN_00121f40);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0011e108);
    void Start() RETAIL(FUN_00122068);
    void Frame(TimeClock* clock) RETAIL(FUN_00119748);
    void Restart(u32 word) RETAIL(FUN_001222b0);
    void Stop() RETAIL(FUN_001222b8);
};
CHECK_OFFSET(SkateController, sounds, 0x20);
CHECK_OFFSET(SkateController, trails, 0x40);
CHECK_OFFSET(SkateController, trailSlots, 0x3C0);
CHECK_OFFSET(SkateController, surface, 0x3E0);
CHECK_SIZE(SkateController, 0x3F0);

extern "C"
{
    extern const GccVTableEntry g_NodeControllerVTable[] RETAIL(D_002F03B0);
    extern const GccVTableEntry g_JointAimerVTable[] RETAIL(D_002EE698);
    extern const GccVTableEntry g_JointAimControllerVTable[] RETAIL(D_002EE660);
    extern const GccVTableEntry g_MaskControllerVTable[] RETAIL(D_002EE568);
    extern const GccVTableEntry g_SplineControllerVTable[] RETAIL(D_002EE530);
    extern const GccVTableEntry g_SkateControllerVTable[] RETAIL(D_002EE4B8);

    // The node's controller replaced: the old one stopped and destroyed, the new one started (game/objectnodeparts.cpp)
    void SetNodeController(ObjectNode* node, NodeController* controller) RETAIL(FUN_0023e420);
    // A JointAimer's joint posed: the joint's matrix (its pose's in the world) and the facing toward the target, and its own matrix
    void PoseAimedJoint(AimedJoint* aimed, Matrix4x4* matrix, const Matrix4x4* facing, const Matrix4x4* local, ObjectPlace* place)
        RETAIL(FUN_001163b8);
}
