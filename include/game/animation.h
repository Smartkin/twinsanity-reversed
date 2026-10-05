#pragma once

#include "abi.h"
#include "common.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/string.h"

// The OGIs' skeletons at work: a joint's animation decoded from its animation's tracks a frame at a time, blended with what it had,
// and its matrix made in its parent's space (VU0's microprograms on the PS2, through Platform::Math)

class Stream;
struct BlendSkin;
struct ChunkData;
struct ChunkLights;
struct RigidModel;
struct Skin;
struct ChunkLinkData;
struct AnimationStatus;
struct JointAnimation;
struct TimeClock;

// A joint's detail byte, the low halves of the RM2's fourth and fifth words: its count of children (nothing reads it) and the
// level of detail its children are animated up to (an animator's detail above it leaves them out; TT Lab's Detail)
union JointDetail
{
    u8 value;
    struct
    {
        u8 unused0 : 4;
        u8 level : 4;
    };
};
CHECK_SIZE(JointDetail, 1);

// An OGI's joint (0x60 bytes): its bind pose, its place in the world at rest, the rotation its animation's turn is added to (its
// settings' additionalRotation), then its ID, its index (the one its animations' settings go by), its parent (GameOGI::NoJoint
// none) and its detail. The RM2 has its ID, index, parent, child count and detail as words, then the bind position, the world
// position, the bind rotation, the unused rotation and the additional rotation
struct OgiJoint
{
    Vector4 bindPosition;
    Vector4 bindRotation;
    Vector4 worldPosition;
    Vector4 unusedRotation;
    Vector4 additionalRotation;
    u8 id;
    u8 index;
    u8 parent;
    JointDetail detail;
    u8 unused54[0x60 - 0x54];
};
CHECK_OFFSET(OgiJoint, additionalRotation, 0x40);
CHECK_OFFSET(OgiJoint, index, 0x51);
CHECK_SIZE(OgiJoint, 0x60);

// What a joint's animator has: a rotation, a translation, a scale, independent scaling (the parent's scale divides the joint's
// matrix), the parent's matrix doesn't take part (nothing sets it), the matrix is made already
union JointAnimatorFlags
{
    u32 value;
    struct
    {
        u32 hasRotation : 1;
        u32 hasTranslation : 1;
        u32 hasScale : 1;
        u32 independentScaling : 1;
        u32 noParent : 1;
        u32 done : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(JointAnimatorFlags, 4);

// What a joint's animation comes to this frame (0x40 bytes, on the stack): which parts it has (the retail code keeps them as the
// 64 bits from the start, the joint's pointer the upper half), the joint and its animation, the rotation, translation and scale
struct JointAnimator
{
    JointAnimatorFlags flags;
    OgiJoint* joint;
    JointAnimation* animation;
    u32 unused0C;
    Vector4 rotation;
    Vector4 translation;
    Vector4 scale;
};
CHECK_OFFSET(JointAnimator, rotation, 0x10);
CHECK_SIZE(JointAnimator, 0x40);

// A reader of a joint's tracks (0x10 bytes): its settings' bits of the channels read from the static values (bit 0 the next
// channel's, shifted out as they're read), a bit for each channel left to read (shifted out with them, nothing tests it), where
// its static values, this frame's and the next frame's values are
struct TrackReader
{
    u16 statics;
    u16 unused02;
    const s16* staticValues;
    const s16* current;
    const s16* next;
};
CHECK_SIZE(TrackReader, 0x10);

// A joint's flags in an animation: its counts of static and animated channels (nothing reads them), its count of channels (9, a
// blend shapes' joint its shapes), its turn is added to the joint's additional rotation, independent scaling
union JointTrackFlags
{
    u16 value;
    struct
    {
        u16 unused0 : 4;
        u16 unused4 : 4;
        u16 channels : 4;
        u16 additionalRotation : 1;
        u16 independentScaling : 1;
        u16 unused14 : 2;
    };
};
CHECK_SIZE(JointTrackFlags, 2);

// A joint's settings in an animation (8 bytes, the AnimationTool's JointSettings): its flags, a bit per channel (translation x, y,
// z, the turns about x, y, z, scale x, y, z) set when the channel's value is static, where its static values and its frames'
// values start
struct JointTrackSettings
{
    JointTrackFlags flags;
    u16 statics;
    u16 staticIndex;
    u16 frameIndex;
};
CHECK_SIZE(JointTrackSettings, 8);

// An animation's data's layout: its joints (each a JointTrackSettings), the channels each has, how many of all of their values
// are static (16 bit values, floats in a cutscene's tracks and the dynamic scenery's) and how many every frame has
union AnimationLayout
{
    u32 value;
    struct
    {
        u32 joints : 7;
        u32 channels : 4;
        u32 staticValues : 11;
        u32 frameValues : 10;
    };
};
CHECK_SIZE(AnimationLayout, 4);

// Where an animation's data is (0xC bytes): its layout, its frames and its handle in the disk manager. The data is the joints'
// settings, the static values, then the frames' values
struct AnimationDataInformation
{
    AnimationLayout layout;
    u16 frames;
    u16 unused06;
    s32 diskHandle;
};
CHECK_SIZE(AnimationDataInformation, 0xC);

// An animation's data as a status plays it (0x14 bytes): its information, its joints' settings, its static values, and this
// frame's and the next frame's values (16 bit fixed point: translations and scales in 4096ths, turns in 4096ths of a turn), found
// again in the disk manager every frame
struct AnimationData
{
    AnimationDataInformation* information;
    const JointTrackSettings* settings;
    const s16* statics;
    const s16* current;
    const s16* next;
};
CHECK_SIZE(AnimationData, 0x14);

// An animation's data as other code makes it (12 bytes): its layout (its values a frame are worked out from the rest when it's
// copied), its frames and its disk manager node
struct AnimationDataSource
{
    AnimationLayout layout;
    u16 frames;
    s32 diskHandle;
};
CHECK_SIZE(AnimationDataSource, 0xC);

// An animation's bits (the RM2's): it has joints' data, it has blend shapes' data, its frames and its frames a second (TT Lab's
// HasAnimationData, HasFacialAnimationData, TotalFrames and DefaultFPS)
union GameAnimationBits
{
    u32 value;
    struct
    {
        u32 hasMain : 1;
        u32 hasBlendShapes : 1;
        u32 frames : 16;
        u32 rate : 5;
        u32 unused23 : 9;
    };
};
CHECK_SIZE(GameAnimationBits, 4);

// An animation's ID of none (a slot without one)
constexpr u16 NoAnimationId = 0xFFFF;

// An animation as the RM2's code section has it (0x24 bytes, a resource of the game's tables): its resource header's bits
// (game/resources.h), its ID, its bits and where each data is
struct GameAnimation
{
    u32 resourceBits;
    u32 id;
    GameAnimationBits bits;
    AnimationDataInformation main;
    AnimationDataInformation blendShapes;
};
CHECK_OFFSET(GameAnimation, main, 0xC);
CHECK_SIZE(GameAnimation, 0x24);

// How an animation is to be played: it loops, it's queued after what plays, it plays backward, it starts where the one before it
// is, it starts part way, it's queued even when the one before plays the same
union AnimationSettingsBits
{
    u32 value;
    struct
    {
        u32 loops : 1;
        u32 queued : 1;
        u32 backward : 1;
        u32 continues : 1;
        u32 startsPartWay : 1;
        u32 queuedAlways : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(AnimationSettingsBits, 4);

// How an animation is to be played (0x14 bytes): its bits, where it starts part way, the animation, how long it blends in and its
// length (clock units)
struct AnimationSettings
{
    AnimationSettingsBits bits;
    f32 start;
    GameAnimation* animation;
    s32 blendTime;
    s32 length;
};
CHECK_SIZE(AnimationSettings, 0x14);

// An animation's status: it started, it ended, it loops, it plays backward, it's blending in, its animation's frames, it starts
// part way
union AnimationStatusBits
{
    u32 value;
    struct
    {
        u32 started : 1;
        u32 ended : 1;
        u32 loops : 1;
        u32 reversed : 1;
        u32 blendsIn : 1;
        u32 frames : 16;
        u32 startsPartWay : 1;
        u32 unused22 : 10;
    };
};
CHECK_SIZE(AnimationStatusBits, 4);

// An animation playing on a joint's chain (0x30 bytes): its bits, its main and its blend shapes' data, where it starts (a share of
// its length), the clock's time it started at, its length and the time into it (clock units), the share of the way from this frame
// to the next, when it started blending in and for how long, how much it weighs, the next one played together
struct AnimationStatus
{
    AnimationStatusBits bits;
    AnimationData* main;
    AnimationData* blendShapes;
    f32 start;
    s32 began;
    s32 length;
    s32 time;
    f32 frameShare;
    s32 blendStart;
    s32 blendTime;
    f32 weight;
    AnimationStatus* next;
};
CHECK_OFFSET(AnimationStatus, frameShare, 0x1C);
CHECK_SIZE(AnimationStatus, 0x30);

// The animations a joint plays (0xC bytes): the chain's next link (the parent's, when the joint has none of its own), the
// statuses
struct AnimationChain
{
    AnimationChain* next;
    AnimationStatus* main;
    AnimationStatus* blendShapes;
};
CHECK_SIZE(AnimationChain, 0xC);

// What takes part in a joint's animation once it's worked out (a character's head turning, say): the next one, and the object
// (its vtable at its start: slot 4 told of the animator and the matrix)
struct JointCallback
{
    JointCallback* next;
    void* object;
};

// A joint animation's bits (the retail code keeps them as the 64 bits with the joint's pointer): its scale is set (nothing reads
// it), its count of children
union JointAnimationBits
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 childCount : 5;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(JointAnimationBits, 4);

// A joint of an instance's skeleton (0xA0 bytes): its matrix in the model's space, the scale its children take as their parent's,
// its bits, the joint, its callbacks, its own animations (else its parent's), its parent and children
struct JointAnimation
{
    static constexpr u32 MostChildren = 12;

    Matrix4x4 transform;
    Vector4 scale;
    JointAnimationBits bits;
    OgiJoint* joint;
    JointCallback* callbacks;
    AnimationChain* chain;
    JointAnimation* parent;
    JointAnimation* children[MostChildren];
    u8 unused94[0xA0 - 0x94];
};
CHECK_OFFSET(JointAnimation, scale, 0x40);
CHECK_OFFSET(JointAnimation, children, 0x64);
CHECK_SIZE(JointAnimation, 0xA0);

// Where an instance's joints' matrices go for its skin (0x10 bytes): the level of detail the joints' children are left out above
// (with three bytes nothing reads, cleared with it as a word), the frame they were made in (the frames rendered), the start and
// the end of what's written (memory of the frame)
struct JointMatrices
{
    union
    {
        u32 detailWord;
        struct
        {
            u8 detail;
            u8 unused01[3];
        };
    };
    u32 frame;
    Matrix4x4* start;
    Matrix4x4* end;
};
CHECK_SIZE(JointMatrices, 0x10);

// An array the game sizes once (8 bytes): its items and their count
template <typename T>
struct SizedArray
{
    T* data;
    s32 size;
};

// An OGI's exit point (0x50 bytes): its matrix, the joint it's on (GameOGI::NoJoint none), its ID
struct OgiExitPoint
{
    Matrix4x4 matrix;
    u8 joint;
    u8 id;
    u8 unused42[0x50 - 0x42];
};
CHECK_SIZE(OgiExitPoint, 0x50);

// An exit point's animation's bits (the retail code keeps them as the 64 bits with the place's pointer): its matrix is to be made
// again (the animator stepped or got a place), its joints moved this frame (made again when it's on a joint), its matrix's axes
// are kept unit long
union ExitPointBits
{
    u32 value;
    struct
    {
        u32 outdated : 1;
        u32 jointsMoved : 1;
        u32 normalizes : 1;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(ExitPointBits, 4);

// An instance's exit point at work (0x50 bytes): its matrix (made when it's asked for), its bits, the instance's place, the exit
// point and the joint it's on
struct ExitPointAnimation
{
    Matrix4x4 matrix;
    ExitPointBits bits;
    void* place;
    OgiExitPoint* exitPoint;
    JointAnimation* joint;
};
CHECK_SIZE(ExitPointAnimation, 0x50);

// A model's (an OGI's) ID of none (a slot without one)
constexpr u16 NoModelId = 0xFFFF;

// An OGI as the game keeps it (0x90 bytes, a resource of the game's tables read from the RM2's graphics): its resource header
// (game/resources.h), 16 bytes only the older layout has (read and never used), its bounding box, its counts (joints, exit points,
// react joints, two bytes nothing reads, rigid models, whether it has a skin and a blend skin, collision hulls), the joints'
// matrices in the world at rest, its rigid models' IDs, its skins' IDs, its joints, exit points, skins, rigid models, hulls and the
// joints they're on. Its react joints are the joints of IDs 0 up that code poses through callbacks (head tracking, aiming, ropes)
struct GameOGI
{
    // A joint index that names none: a joint without a parent, an exit point or a hull on no joint, an ID without a joint; and an
    // exit point's index that names none (an instance attached at no exit point, a shot or a head at the instance's place)
    static constexpr u8 NoJoint = 0xFF;
    static constexpr u8 NoExitPoint = 0xFF;

    u32 resourceBits;
    u32 id;
    u8 unused08[0x10 - 0x8];
    u8 unused10[0x10];
    Box bounds;
    // The counts, read and cleared as four words
    union
    {
        u32 countWords[4];
        struct
        {
            u8 jointCount;
            u8 exitPointCount;
            u8 reactJointCount;
            u8 unused43[2];
            u8 rigidModelCount;
            u8 hasSkin;
            u8 hasBlendSkin;
            u8 hullCount;
            u8 unused49[0x50 - 0x49];
        };
    };
    String name;
    u8* rigidModelJoints;
    Matrix4x4* jointMatrices;
    u32* rigidModelIds;
    u32 skinId;
    u32 blendSkinId;
    OgiJoint* joints;
    OgiExitPoint* exitPoints;
    Skin* skin;
    BlendSkin* blendSkin;
    RigidModel** rigidModels;
    CollisionHull* hulls;
    u8* hullJoints;
    u8 unused8C[0x90 - 0x8C];
};
CHECK_OFFSET(GameOGI, jointCount, 0x40);
CHECK_OFFSET(GameOGI, joints, 0x70);
CHECK_SIZE(GameOGI, 0x90);

// An instance's blend shapes' weights (0x10 bytes): how many it has room for, the next one written, its room (taken from a ring of
// RingFloats floats every instance shares), the weights it had and the ones written this frame
struct BlendShapeWeights
{
    static constexpr u32 RingFloats = 1000;

    u8 count;
    u8 index;
    f32* room;
    f32* current;
    f32* next;
};
CHECK_SIZE(BlendShapeWeights, 0x10);

// An animator's bits: it's animated this frame (its matrices made, the animations played on it queued after what plays), its react
// joints given their callbacks (counted, nothing reads it)
union OgiAnimatorBits
{
    u32 value;
    struct
    {
        u32 animated : 1;
        u32 unused1 : 8;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(OgiAnimatorBits, 4);

// An instance's skeleton at work (0x40 bytes): its bits, its OGI, its root joint, its place, its exit points' animations, its react
// joints (by ID, which its callbacks take part in), its joints (by index), the callbacks of its react joints, where its joints'
// matrices go, its blend shapes' weights
struct OgiAnimator
{
    // The joint the animation calls name for the root (any other names a react joint)
    static constexpr u8 RootJoint = 0xFF;

    OgiAnimatorBits bits;
    GameOGI* ogi;
    JointAnimation* root;
    void* place;
    SizedArray<ExitPointAnimation*>* exitPoints;
    SizedArray<JointAnimation*>* reactJoints;
    SizedArray<JointAnimation*>* joints;
    SizedArray<JointCallback*>* callbacks;
    JointMatrices matrices;
    BlendShapeWeights blendShapes;
};
CHECK_OFFSET(OgiAnimator, matrices, 0x20);
CHECK_SIZE(OgiAnimator, 0x40);

// An animation's data's size, its data (its layout and frames, then its bytes) read into a disk manager node of its own (the
// one it had let go first), and its pointers into what the disk manager has of it this frame (the retail code inlines them)
u32 AnimationDataSize(const AnimationDataInformation* information);
void ReadAnimationData(AnimationDataInformation* information, Stream* stream);
void FindFrames(AnimationData* data, u32 frame, u32 next);

extern "C"
{

    // A joint's scale and translation moved a share of the way toward a value from what the animator has (its joint's bind
    // translation and the joints' scale when it has none yet)
    void ScaleJoint(f32 share, JointAnimator* animator, const Vector4* scale) RETAIL_N32(ScaleJoint);
    void TranslateJoint(f32 share, JointAnimator* animator, const Vector4* translation) RETAIL_N32(TranslateJoint);
    // A turn of this frame and the next read from a joint's tracks (65536ths of a turn in radians, the shorter way round)
    void ReadJointAngle(TrackReader* reader, f32* now, f32* next) RETAIL(AnimateJointRotation);
    // The animator's rotation a share of the way toward another from what it has (its joint's bind rotation when it has none
    // yet; the retail code takes the other in VU0's vf01 and leaves the result there)
    void RotateJoint(f32 share, JointAnimator* animator, const Vector4* to, Vector4* out);
    // A joint's animation of its chain this frame, weighed (the chain's main statuses one after the other, each by its weight;
    // the bind pose without them), and one status's frame of its data, blended in by a weight
    void DoAnimation(f32 weight, JointAnimator* animator, AnimationChain* chain) RETAIL_N32(DoAnimation);
    void AnimateJoint(f32 weight, f32 frameShare, JointAnimator* animator, const AnimationData* data) RETAIL_N32(AnimateJoint);
    // A joint's matrix from what its animator has, in its parent's space (the scale it leaves its children kept in its
    // animation), marked made when asked; and the matrices of a joint and the joints under it made and written in order
    void CreateJointTransform(JointAnimator* animator, JointAnimation* parent, u32 markDone, Matrix4x4* out) RETAIL(CreateJointTransform);
    void TransformJoints(JointAnimation* joint, AnimationChain* chain, JointAnimation* parent, JointMatrices* matrices) RETAIL(TransformJoints);
    // An animator without anything of its own yet
    void ClearJointAnimator(JointAnimator* animator) RETAIL(FUN_00298518);
    // A joint's animation under its parent's (made the first time; none past the 12 children a joint has room for), and an
    // instance's joints' animations made into a tree (the joints without a parent under its root)
    JointAnimation* GetJointAnimationFromParentJoint(JointAnimation* parent, OgiJoint* joint) RETAIL(GetJointAnimationFromParentJoint);
    void SetJointAnimations(OgiAnimator* animator, u32 jointCount) RETAIL(SetJointAnimations);
    // The ring every instance's blend shapes' weights are taken from, and where its next ones start
    extern f32 g_BlendShapeRing[BlendShapeWeights::RingFloats] RETAIL(G_BLEND_SHAPE_FLOATS);
    extern f32* g_BlendShapeRingNext RETAIL(G_BLEND_SHAPE_FLOATS_ALLOCATOR);
    // An instance's blend shapes this frame: its root's chain's blend shapes' statuses one after the other, each a frame of its
    // data weighed against what the weights were (without data: what they were, by the rest of the weight); and its joints'
    // matrices made and its blend shapes worked out
    void BlendShapesFrame(f32 weight, f32 frameShare, BlendShapeWeights* weights, const AnimationData* data) RETAIL_N32(FUN_002941b0);
    void FacialAnimation(OgiAnimator* animator, BlendShapeWeights* weights) RETAIL(FacialAnimation);
    void ProcessAnimationData(OgiAnimator* animator, JointMatrices* matrices, BlendShapeWeights* weights) RETAIL(ProcessAnimationData);
    // The seconds left of a chain's longest main animation (whole clock units)
    f32 CalculateProgress(const AnimationChain* chain) RETAIL(CalculateProgress);
    // A status moved on to the clock's time (started, looped or ended), its data's frames found for its time (the next frame the
    // first again when it loops, both turned round when it plays backward), its weight while it blends in
    void ProgressAnimation(AnimationStatus* status, s32 now, u32 main) RETAIL(ProgressAnimation);
    void SetAnimationData(AnimationStatus* status, u32 main) RETAIL(SetAnimationData);
    // A status destroyed (its data with it) and a chain's main or blend shapes' statuses destroyed; a status made to start where
    // another is (backward when one plays backward and the other doesn't); whether two statuses play the same animations
    void DestroyAnimationStatus(AnimationStatus* status, u32 destroyFlags) RETAIL(FUN_002980d0);
    void ClearAnimationStatuses(AnimationChain* chain, u32 main) RETAIL(FUN_00298038);
    void ContinueAnimationStatus(AnimationStatus* status, const AnimationStatus* from) RETAIL(FUN_00298138);
    u32 SameAnimations(const AnimationStatus* status, const AnimationStatus* other) RETAIL(FUN_002981d0);
    // An instance's joints' matrices made empty, destroyed and given room for this frame; its blend shapes' weights made empty,
    // destroyed and made to take new room
    JointMatrices* ConstructJointMatrices(JointMatrices* matrices) RETAIL(FUN_00298248);
    void DestroyJointMatrices(JointMatrices* matrices, u32 destroyFlags) RETAIL(FUN_00298260);
    void StartJointMatrices(JointMatrices* matrices, const GameOGI* ogi) RETAIL(FUN_00298288);
    BlendShapeWeights* ConstructBlendShapeWeights(BlendShapeWeights* weights) RETAIL(FUN_002982d8);
    void DestroyBlendShapeWeights(BlendShapeWeights* weights, u32 destroyFlags) RETAIL(FUN_002982f8);
    void ResetBlendShapeWeights(BlendShapeWeights* weights) RETAIL(FUN_00298320);
    // An animator not animated this frame (no matrices made): 0
    u32 ForgetJointMatrices(OgiAnimator* animator) RETAIL(FUN_00297d90);
    // A chain's main or blend shapes' statuses moved on to the clock's time, each dropped once the one after it has blended in
    // (the last once it neither blends in nor has data); every joint's chain from one down moved on (a chain left without
    // statuses freed; the joints under one below a detail left alone), an animator's from its root
    void ProgressAnimationChain(AnimationChain* chain, u32 main, s32 now) RETAIL(FUN_00293b50);
    void ProgressJointChains(JointAnimation* joint, s32 now, u32 detail, AnimationChain* parentChain) RETAIL(FUN_002951d8);
    void ProgressAnimator(OgiAnimator* animator, s32 now, u32 detail) RETAIL(FUN_00298ff0);
    // The seconds left of the animations of an animator's root (OgiAnimator::RootJoint), or of a react joint's (by its ID) or the
    // nearest joint above it that has any left
    f32 GetAnimationProgress(const OgiAnimator* animator, u32 joint) RETAIL(GetAnimationProgress);
    // A joint's matrix and the matrices under it written again as they are (an animator's from its root), and a joint under
    // another found by its joint's ID (deep: also below its children)
    void CopyJointMatrices(const JointAnimation* joint, JointMatrices* matrices) RETAIL(FUN_002988c8);
    void CopyAnimatorMatrices(const OgiAnimator* animator, JointMatrices* matrices) RETAIL(FUN_00298f18);
    JointAnimation* FindChildJoint(JointAnimation* joint, u32 id, u32 deep) RETAIL(FUN_002987f8);
    // An animation's settings at a speed (its length its frames at its rate, divided by the speed) and destroyed; a status made to
    // play an animation for a length, blending in for a time (none: ended at once without an animation); an animation played on a
    // chain's main or blend shapes' statuses (replacing them, or queued after them when asked)
    AnimationSettings* ConstructAnimationSettings(f32 speed, AnimationSettings* settings, GameAnimation* animation) RETAIL_N32(FUN_00297f98);
    void DestroyAnimationSettings(AnimationSettings* settings, u32 destroyFlags) RETAIL(FUN_00298010);
    AnimationStatus* ConstructAnimationStatus(AnimationStatus* status, GameAnimation* animation, s32 length, s32 blendTime) RETAIL(FUN_00293f18);
    void PlayAnimationOnChain(AnimationChain* chain, u32 main, const AnimationSettings* settings, u32 queues) RETAIL(FUN_00293960);
    // An OGI's joint of an ID (GameOGI::NoJoint none); a joint's tree destroyed (its chain's statuses and the chain with it); an
    // animator's react joints and callbacks emptied; its exit points' animations put on their exit points' joints
    u32 GetJointIndexByID(const GameOGI* ogi, u32 id) RETAIL(GetJointIndexByID);
    void DestroyJointTree(JointAnimation* joint, u32 destroyFlags) RETAIL(FUN_00294fb0);
    void ClearReactJoints(OgiAnimator* animator) RETAIL(FUN_002989c0);
    void ClearJointCallbacks(OgiAnimator* animator) RETAIL(FUN_00298a28);
    void BindExitPoints(OgiAnimator* animator) RETAIL(FUN_00295600);
    // An animator given an OGI (none: its joints let go): its joints' tree and table made, its react joints and their callbacks
    // made the first time when asked for, its exit points' animations made the first time; made for an OGI, made with its
    // matrices and weights; given no OGI
    void SetAnimatorOgi(OgiAnimator* animator, GameOGI* ogi, u32 reactJoints, u32 exitPoints) RETAIL(FUN_00295af8);
    OgiAnimator* ConstructOgiAnimatorBase(OgiAnimator* animator, GameOGI* ogi, u32 reactJoints, u32 exitPoints) RETAIL(FUN_00298a80);
    OgiAnimator* InitOgiAnimatorService(OgiAnimator* animator, GameOGI* ogi, u32 reactJoints, u32 exitPoints) RETAIL(InitOgiAnimatorService);
    void ReleaseAnimatorOgi(OgiAnimator* animator) RETAIL(FUN_00297d68);
    // Its joints, exit points' animations, callbacks and react joints freed (the react joints' array left pointed at)
    void DestroyOgiAnimator(OgiAnimator* animator, u32 destroyFlags) RETAIL(FUN_002957a8);
    // An animator made with InitOgiAnimatorService destroyed and freed (its blend shapes' weights and joints' matrices first,
    // which the retail code inlines)
    void DeleteOgiAnimator(OgiAnimator* animator);
    // The animations of the shaders of the OGI's skin's, blend skin's and rigid models' materials started again
    void RestartOgiAnimations(GameOGI* ogi) RETAIL(FUN_00299330);
    // The instance's place given to it and its exit points' animations, which are outdated and moved
    void SetAnimatorPlace(OgiAnimator* animator, void* place) RETAIL(FUN_00295978);
    // The object's callback on the joint taken out. Returns whether it had one
    u32 RemoveJointCallback(OgiAnimator* animator, u32 joint, void* object) RETAIL(FUN_00295a50);
    // Its exit points' matrices moved through the link the instance went through, only a link with LinkedRm2Loaded. Returns
    // whether they were (yes without a place or exit points)
    u32 MoveAnimatorThroughLink(OgiAnimator* animator, ChunkData* data, const ChunkLinkData* link) RETAIL(FUN_00295fd0);
    // The exit points told of the instance's place (outdated) and whether the joints moved this frame
    void UpdateExitPoints(OgiAnimator* animator, u32 moved) RETAIL(FUN_00295ee8);
    // An animator's frame: its animations moved on to the clock and, when asked, its matrices made (down to a detail) and its
    // blend shapes worked out; or its last matrices written again: whether it made them
    u32 AnimateOgi(OgiAnimator* animator, const GameOGI* ogi, const TimeClock* clock, u32 animate, u32 detail) RETAIL(FUN_00297e68);
    u32 ReuseJointMatrices(OgiAnimator* animator, const GameOGI* ogi, u32 animate, u32 detail) RETAIL(FUN_00297da8);
    // An exit point's matrix in the world made when it's outdated or its joints moved (when it's on a joint): its own matrix on its
    // joint and the instance's place (the place's alone without an exit point)
    ExitPointAnimation* UpdateExitPointMatrix(ExitPointAnimation* animation) RETAIL(FUN_002943c8);
    // An exit point's animation off its exit point and joint
    void ClearExitPointLinks(ExitPointAnimation* animation) RETAIL(FUN_00298350);
    // An animation played on an animator's root (OgiAnimator::RootJoint) or react joint (its chain made when there's an animation
    // to play; queued when the animator was animated this frame), and what plays there faded out over a time
    void PlayOgiAnimation(OgiAnimator* animator, const AnimationSettings* settings, u32 joint) RETAIL(FUN_00298ca8);
    void StopOgiAnimation(OgiAnimator* animator, s32 blendTime, u32 joint) RETAIL(FUN_00298d70);
    // An object told of a react joint's animation (once: whether it was added), a list of callbacks destroyed, and a turn added
    // to what a joint's animator has
    u32 AddJointCallback(OgiAnimator* animator, u32 joint, void* object) RETAIL(FUN_00298bd0);
    void DestroyJointCallbacks(JointCallback* callback, u32 destroyFlags) RETAIL(FUN_00298798);
    void AddJointRotation(JointAnimator* animator, const Vector4* rotation) RETAIL(FUN_00298520);
    // A joint's matrix of an animator (by index): whether it has the joint
    u32 CopyJointTransform(const OgiAnimator* animator, u32 index, Matrix4x4* out) RETAIL(CopyJointTransform);
    // An object (its vtable at its start) told to attach itself (slot 2) to or detach itself (slot 3) from the animator of an
    // instance's model node, when it has one
    void AttachToModelAnimator(void* object, struct InstanceContext* instance) RETAIL(FUN_00299060);
    void DetachFromModelAnimator(void* object, struct InstanceContext* instance) RETAIL(FUN_002990b8);
    // An OGI and an animation made empty (their resource headers without references or ID)
    GameOGI* InitOGI(GameOGI* ogi) RETAIL(InitOGI);
    GameAnimation* InitAnimation(GameAnimation* animation) RETAIL(InitAnimation);
    // An animation read from the RM2's code section: its bits, then its joints' data and its blend shapes' data, each read into a
    // node of the disk manager's of its own (the one it had let go first)
    void ReadAnimation(GameAnimation* animation, Stream* stream) RETAIL(ReadAnimation);
    // Its data's disk manager nodes let go
    void DestroyAnimation(GameAnimation* animation, u32 destroyFlags) RETAIL(FUN_00299550);
    // An animation's data made from another's (its layout, the values a frame worked out again, a copy of its bytes in a disk
    // manager node of its own; it lets go of its old node twice, retail's, which never has one here)
    AnimationDataInformation* CopyAnimationData(AnimationDataInformation* information, const AnimationDataSource* source)
        RETAIL(FUN_00299b20);
    // A new animation without an ID made of the data given (either can be missing)
    GameAnimation* MakeAnimation(GameAnimation* animation, const AnimationDataSource* main,
                                 const AnimationDataSource* blendShapes) RETAIL(FUN_00299490);
    void ReadJoint(OgiJoint* joint, Stream* stream) RETAIL(ReadJoint);
    // An OGI read from the RM2's code section (in the RM2's layout once one is queued: its counts as bytes, no older bytes, no data
    // before each rigid model's ID, no hulls' surfaces): its counts, bounds, joints, exit points, rigid models' joints and IDs,
    // joints' matrices, skin, blend skin and hulls with their joints (a box of its bounds on no joint when it has none)
    void ReadOgi(GameOGI* ogi, Stream* stream) RETAIL(ReadOgi);
    // Its arrays freed and its models' references let go
    void DestroyOgi(GameOGI* ogi, u32 destroyFlags) RETAIL(FUN_002960e0);
    // The OGI drawn with its instance's joints' matrices (only matrices made this frame or the last, else nothing): its rigid
    // models on their joints, its skin and blend skin (the joints' matrices made the skins' in place, the blend skin's first three
    // shapes with weights blended in), lit by the lights gathered at the instance; and without them, its rigid models at the
    // instance's matrix
    void DrawOgi(GameOGI* ogi, const Matrix4x4* matrix, JointMatrices* joints, const BlendShapeWeights* blendShapes,
                 const ChunkLights* lights, u32 mode) RETAIL(SetupOGIRender);
    void DrawOgiRigidModels(GameOGI* ogi, const Matrix4x4* matrix, const ChunkLights* lights, u32 mode) RETAIL(FUN_00299280);
}
