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

// What command 645 (CreateNodeController) gives an object node, the part it keeps 0x114 bytes in (ObjectNode::unknown114): its
// kind (the command's), its node and its vtable (the base's D_002F03B0, whose functions do nothing): 1 the destructor, 2 started
// once the node has it, 3 its frame (the node's update, with the clock), 4 the node restarted (the word it's given), 5 stopped
// before it's destroyed or replaced
class NodeController
{
public:
    enum Kind : u8
    {
        KindJointAim = 0,
        KindMask = 1,
        KindSpline = 2,
        KindSkate = 3,
    };

    u8 kind;
    u8 unknown01[3];
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

// A particle trail's arguments (0x70 bytes, the AddTrail command's from 0x10 bytes in; game/particletrails.cpp steps it): its
// bits, the surface its node has to touch for it to play (-1 any), the particle system it plays (0xFFFF none), the sound slots of
// its node's object it plays one of, the message its instance is sent (0xFFFF none), the exit point it leaves from, what its kind
// compares with (a squared speed or change of velocity, or a strength; the pitch's scale with bit 27), kind 1's interval and its
// random extra, the camera shake's strengths along x and y (z the first's) or its strength, and its falloff, the sound's volume,
// random pitch and pitch, the animation progress kind 7 starts again below, the strength kind 8 asks for (the sound grows louder
// past it), the spacing (the interval of a speed of 1 with bit 26, kind 3's interval, the decals' spread) and the offset of its
// frame
struct TrailArguments
{
    // 64 bits in retail. Its kind (bits 0-3): 0 it emits every step, 1 every interval (a random extra on top; with bit 26 its
    // spacing over its speed, once it moves), 3 while it moves faster than a threshold (slower than the threshold's negative; at
    // most once a spacing with bit 26), 4 while its velocity changed more than that, 5 the same of a vector the retail code never
    // sets, 7 once when its instance's animation starts again (its surface's effects then too), 8 while its trails are stronger
    // than a strength; never else. Then the frame's space (bits 10-13: 0 its exit point's, 1 where its instance started, 2 its
    // instance's place, else the origin), how the frame turns (bit 18 and bits 19-22), the sound's group (bits 23-25: 1 group 1,
    // else 0), a camera shake (32), a volume (34), a pitch (35), a random pitch (36) and an offset (37) given, the kind of the
    // contact with the surface its node touches (bits 38-45, 0xFF none), how many sound slots (46-49), the emitter's gravity
    // frame (53), how many decals (54-57) and no surface sound (58). Bits 4-9 and 14-17 are 1 when it's made
    enum Bits : u64
    {
        KindMask = 0xF,
        SpaceShift = 10,
        SpaceMask = 0xF,
        TurnedShift = 18,
        AxesShift = 19,
        AxesMask = 0xF,
        GroupMask = u64{0x7} << 23,
        GroupOne = u64{0x1} << 23,
        BySpacing = u64{0x1} << 26,
        PitchByThreshold = u64{0x1} << 27,
        ShakesCamera = u64{0x1} << 32,
        VolumeGiven = u64{0x1} << 34,
        PitchGiven = u64{0x1} << 35,
        RandomPitch = u64{0x1} << 36,
        OffsetGiven = u64{0x1} << 37,
        ContactShift = 38,
        ContactMask = 0xFF,
        SoundsShift = 46,
        SoundsMask = 0xF,
        GravityFrame = u64{0x1} << 53,
        DecalsShift = 54,
        DecalsMask = 0xF,
        NoSurfaceSound = u64{0x1} << 58,
    };

    u64 bits;
    s32 surface;
    u16 system;
    u16 sounds[8];
    u16 unknown1E;
    u16 message;
    u8 exitPoint;
    u8 unknown23;
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
    u8 unknown11[0xF];
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
    u8 unknown0C[4];
    Vector4 targetPosition;
    // Not set by the constructor
    u32 target;
    u8 unknown24[0xC];
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
    u8 unknown0C[4];
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

    // The character's int property 0 was 1, 3 or 5 when it started (4 keeps it; nothing here reads it), the mask is on the
    // character, the IDs command 655 gave, hidden while the game is watched and the character was visible when it was hidden
    // (nothing reads it)
    enum Flags : u8
    {
        FlagCharacterProperty = 0x1,
        FlagOnCharacter = 0x2,
        IdCountShift = 2,
        IdCountMask = 0xF,
        FlagHidden = 0x40,
        FlagCharacterVisible = 0x80,
    };

    static constexpr u32 TrailCount = 12;

    u8 state;
    u8 flags;
    u8 unknown0E[2];
    u16 ids[3];
    u8 unknown16[0xA];
    TrailArguments boostTrail;
    TrailArguments unusedTrail;
    // Only the first three are used, the invincibility's (with their emitters' slots)
    TrailArguments trails[TrailCount];
    s32 trailSlots[4];
    u8 unknown650[0x20];
    InstanceContext* character;
    // How far through arriving, leaving or flying onto the character it is, and the distance it does it over
    f32 progress;
    f32 distance;
    u8 unknown67C[4];
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
// drop) at a rate; command 658 sets them. The offset's x and y and the word at 0x28 aren't used
class SplineController : public NodeController
{
public:
    u8 unknown0C[4];
    Vector4 offset;
    f32 pull;
    f32 turnRate;
    // Not set by the constructor
    f32 unknown28;
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

// Kind 3 (0x3F0 bytes, vtable D_002EE4B8): the Humiliskate's trails and sounds (command 661 gives their IDs, the vehicle's code
// uses them), which keeps whether the player's skate grinds this frame and the last; paused while the game is watched
class SkateController : public NodeController
{
public:
    // The IDs' and the sounds' counts command 661 gave
    enum Counts : u8
    {
        IdCountMask = 0xF,
        SoundCountShift = 4,
        SoundCountMask = 0xF,
    };

    enum Flags : u8
    {
        FlagPaused = 0x1,
        FlagGrinding = 0x2,
        FlagWasGrinding = 0x4,
    };

    static constexpr u32 TrailCount = 8;

    u8 unknown0C;
    u8 counts;
    u8 flags;
    u8 unknown0F;
    u16 ids[TrailCount];
    u16 sounds[13];
    u8 unknown3A[6];
    TrailArguments trails[TrailCount];
    s32 trailSlots[TrailCount];
    // The surface the character skates on (game/commandscharacters.cpp's SetCharacterSurface sets it): while there's none the
    // node's sound is stopped every frame
    union
    {
        CollisionSurface* surface;
        u32 unknown3E0;
    };
    u32 unknown3E4;
    u8 unknown3E8[8];

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
CHECK_OFFSET(SkateController, unknown3E0, 0x3E0);
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
