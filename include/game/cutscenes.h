#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/animation.h"
#include "game/archive.h"
#include "game/math.h"

class CutsceneCameraRig;
struct AnimationData;
struct DynamicAnimationData;
struct InstanceContext;
struct TimeClock;
struct VideoController;

// The cutscenes (CutScene\_<number>_<part>.cts, a part of 150 frames at 25 a second each, read while the part before plays) and
// what the video controller (game/controllers.h) keeps of them while they play. Every track's values are a float animation of one
// joint (game/dynamicscenery.h's), played a frame of the part at a time; the frame 0xFFFF plays a track's values at the end (what
// skipping a cutscene leaves)

// A cutscene's track of an instance (0x4C bytes): the model it's played on (the instance the video controller was given for the
// model, else one made for it; NoModelId none), whether its animation has blend shapes, its values (the position's x, y and z,
// the turns about x, y and z, then shown above a half), its animation's joints and blend shapes, its values at the end, and two
// more animations' data the reading loads (the second with blend shapes only) and nothing plays
struct CutsceneInstanceTrack
{
    u16 model;
    bool hasBlendShapes;
    u8 unused03;
    AnimationDataInformation values;
    AnimationDataSource joints;
    AnimationDataSource blendShapes;
    AnimationDataInformation endValues;
    AnimationDataSource unused34;
    AnimationDataSource unused40;
};
CHECK_OFFSET(CutsceneInstanceTrack, joints, 0x10);
CHECK_OFFSET(CutsceneInstanceTrack, endValues, 0x28);
CHECK_SIZE(CutsceneInstanceTrack, 0x4C);

// A cutscene's track of a particle emitter (0x1C bytes): a halfword and a flag the reading reads and nothing uses, its values
// (the position's x, y and z, the turns about x, y and z, then on above a half) and its values at the end
struct CutsceneEmitterTrack
{
    u16 unused00;
    bool unused02;
    u8 unused03;
    AnimationDataInformation values;
    AnimationDataInformation endValues;
};
CHECK_SIZE(CutsceneEmitterTrack, 0x1C);

// A cutscene's track of a sound (0x1C bytes): its sound (read as a slot of the cutscene's object, kept as the ID of the slot's
// sound), its values (the position's x, y and z, a channel nothing reads, the volume) and its values at the end
struct CutsceneSoundTrack
{
    u16 sound;
    // 1 when made
    u8 unused02;
    u8 unused03;
    AnimationDataInformation values;
    AnimationDataInformation endValues;
};
CHECK_SIZE(CutsceneSoundTrack, 0x1C);

// A cutscene's track of the camera (0x20 bytes): its shots (each the camera's values over its frames: the position's x, y and z,
// the turns about x, y and z, the field of view in radians), its values at the end, and the cuts (16 bit values in 4096ths: the
// next shot starts where they go above 0)
struct CutsceneCameraTrack
{
    u8 shotCount;
    AnimationDataInformation* shots;
    AnimationDataInformation endValues;
    AnimationDataInformation cuts;
};
CHECK_OFFSET(CutsceneCameraTrack, cuts, 0x14);
CHECK_SIZE(CutsceneCameraTrack, 0x20);

// A part of a cutscene (0x68 bytes): the cutscene's frames, the part's (another part is read while it has 150 or more), the
// cutscene's number and the part's (0xFFFF none), a halfword and four words the reading reads and nothing uses, the counts of its
// tracks of instances, sounds and emitters, its camera's track and its tracks
struct Cutscene
{
    static constexpr u32 PartFrames = 150;
    static constexpr u32 FramesPerSecond = 25;
    // The frame a track plays its values at the end at (skipping the cutscene)
    static constexpr u16 EndFrame = 0xFFFF;
    static constexpr u16 NoNumber = 0xFFFF;

    u16 frames;
    u16 partFrames;
    u16 number;
    u16 part;
    u16 unused08;
    u8 instanceTrackCount;
    u8 soundTrackCount;
    u8 emitterTrackCount;
    u8 unused0D[3];
    u32 unused10[4];
    CutsceneCameraTrack camera;
    u8 unused40[0x5C - 0x40];
    CutsceneInstanceTrack* instanceTracks;
    CutsceneSoundTrack* soundTracks;
    CutsceneEmitterTrack* emitterTracks;
};
CHECK_OFFSET(Cutscene, camera, 0x20);
CHECK_OFFSET(Cutscene, instanceTracks, 0x5C);
CHECK_SIZE(Cutscene, 0x68);

// The section reader of a part of a cutscene's file (0x14 bytes, retail's CutsceneReader_Methods: 1 the destructor, 2 the part
// read into its cutscene and handed to the video controller, 3 its file missing): the video controller, the cutscene's number and
// the part's, and the cutscene it's read into
class CutsceneReader : public SectionReader
{
public:
    VideoController* controller;
    s32 number;
    s32 part;
    Cutscene* cutscene;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0029e3f8);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadCutscene);
    // Nothing when the file isn't there
    void Missing(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_0029e590);
};
CHECK_SIZE(CutsceneReader, 0x14);

// A played instance track's bits: whether the instance was the controller's, given back at the end, rather than made for the
// cutscene
union PlayedInstanceTrackBits
{
    u64 value;
    struct
    {
        u64 given : 1;
        u64 unused1 : 63;
    };
};
CHECK_SIZE(PlayedInstanceTrackBits, 8);

// What the video controller keeps of an instance's track (0x40 bytes): the instance, the track it plays and the data of its
// values (made again when the track changes), the animation made of the track's joints, its bits, and the rotation and position
// the values gave it
struct PlayedInstanceTrack
{
    InstanceContext* instance;
    const CutsceneInstanceTrack* track;
    DynamicAnimationData* values;
    GameAnimation* animation;
    PlayedInstanceTrackBits bits;
    u8 unused18[8];
    Vector4 rotation;
    Vector4 position;
};
CHECK_OFFSET(PlayedInstanceTrack, bits, 0x10);
CHECK_OFFSET(PlayedInstanceTrack, position, 0x30);
CHECK_SIZE(PlayedInstanceTrack, 0x40);

// What the video controller keeps of an emitter's track (0x30 bytes): the emitter (NoEmitter, which it stays: nothing makes one;
// it's dropped once the values turn it off), the track it plays and the data of its values, the turns about x, y and z (65536ths
// of a turn) and the position
struct PlayedEmitterTrack
{
    s32 emitter;
    const CutsceneEmitterTrack* track;
    DynamicAnimationData* values;
    s32 angles[3];
    u8 unused18[8];
    Vector4 position;
};
CHECK_OFFSET(PlayedEmitterTrack, angles, 0xC);
CHECK_OFFSET(PlayedEmitterTrack, position, 0x20);
CHECK_SIZE(PlayedEmitterTrack, 0x30);

// What the video controller keeps of a sound's track (0x30 bytes): a word made -1 and never read, the track it plays and the data
// of its values, the position and the volume they give (the player plays no sound with them)
struct PlayedSoundTrack
{
    s32 unused00;
    const CutsceneSoundTrack* track;
    DynamicAnimationData* values;
    u32 unused0C;
    Vector4 position;
    f32 volume;
    u8 unused24[0xC];
};
CHECK_OFFSET(PlayedSoundTrack, position, 0x10);
CHECK_OFFSET(PlayedSoundTrack, volume, 0x20);
CHECK_SIZE(PlayedSoundTrack, 0x30);

// What the video controller keeps of the camera's track (0x1C bytes): the track it plays, the data of its shot's values and of
// its cuts, the camera it places (the game controller's cutscene camera), the shot it's at and whether the cuts are above 0
struct PlayedCameraTrack
{
    u32 unused00;
    const CutsceneCameraTrack* track;
    DynamicAnimationData* values;
    AnimationData* cuts;
    u32 unused10;
    CutsceneCameraRig* camera;
    u8 shot;
    u8 cutting;
    u8 unused1A[2];
};
CHECK_OFFSET(PlayedCameraTrack, camera, 0x14);
CHECK_SIZE(PlayedCameraTrack, 0x1C);

extern "C"
{
    extern const GccVTableEntry g_CutsceneReaderVTable[] RETAIL(CutsceneReader_Methods);

    // The video controller made (for its resource manager, the clock the instances it makes go by): nothing to play, its tracks
    // made, its bits but 1 (the tracks placed in the origin's space) cleared
    VideoController* ConstructVideoController(void* controller, void* resourceManager, u32 clockIndex) RETAIL(FUN_0029a7e8);
    // The read cutscene started once its music is prepared (the instances taken over and given their animations, the music
    // played, the next part asked for, the cutscene skippable once it was seen): whether it started. Else it waits on (and its
    // music is waited for while it was queued)
    u32 StartReadCutscene(VideoController* controller, TimeClock* clock) RETAIL(FUN_0029aa10);
    // The reading of a cutscene's part into a cutscene queued
    void LoadCutscenePart(VideoController* controller, Cutscene* cutscene, s32 number, s32 part) RETAIL(FUN_0029abb8);
    // Everything played let go of: the tracks, the camera's data, the cutscenes and the music, and the frames waited and the
    // clock forgotten
    void StopCutscene(VideoController* controller) RETAIL(FUN_0029acf8);
    // Each instance track given its instance (the controller's for the model, taken over, else one made in the controller's chunk
    // for the model) and its animation
    void TakeCutsceneInstances(VideoController* controller) RETAIL(FUN_0029aea8);
    // An instance taken over for a cutscene: its agent frozen, its animation stopped, and its model the OGI of the model the
    // controller was given it for
    void TakeOverInstance(VideoController* controller, InstanceContext* instance) RETAIL(FUN_0029afd8);
    // A playing cutscene's frame: paused while its clock stops; a music-only one ends with its music; else its tracks played on
    // to the clock, the skip hint (or the disc error once the next part kept it waiting) shown, and at its end its instances
    // given back, it marked seen and everything let go of
    void UpdatePlayingCutscene(VideoController* controller) RETAIL(FUN_0029b128);
    // A queued cutscene's frame: started once its music is prepared, the disc error shown while it kept it waiting
    void UpdateQueuedCutscene(VideoController* controller) RETAIL(FUN_0029b2a0);
    // The cutscene's frame worked out from the clock (the next part taken when it gets there): whether the cutscene goes on
    u32 AdvanceCutscene(VideoController* controller, TimeClock* clock) RETAIL(FUN_0029b438);
    // The next part played once it was read (its instance tracks given their animations, the part after it asked for); else a
    // frame waited
    void NextCutscenePart(VideoController* controller) RETAIL(FUN_0029b508);
    // An instance track given the animation of its track's joints, played on its instance's root
    void PlayInstanceTrackAnimation(PlayedInstanceTrack* played, const CutsceneInstanceTrack* track) RETAIL(FUN_0029b888);

    // A frame of a track played (a share of the way to the next frame; the frame 0xFFFF its values at the end), placed in the
    // origin's space when there's one: an instance moved, turned and shown or hidden; an emitter placed (and let go of once its
    // values turn it off); a sound's position and volume; the camera's position, turn and field of view (a new shot once the cuts
    // go above 0)
    void PlayInstanceTrackFrame(f32 share, PlayedInstanceTrack* played, const CutsceneInstanceTrack* track, u16 frame,
                                const Matrix4x4* origin) RETAIL_N32(FUN_0029b990);
    void PlayEmitterTrackFrame(f32 share, PlayedEmitterTrack* played, const CutsceneEmitterTrack* track, u16 frame,
                               const Matrix4x4* origin) RETAIL_N32(FUN_0029c340);
    void PlaySoundTrackFrame(f32 share, PlayedSoundTrack* played, const CutsceneSoundTrack* track, u16 frame,
                             const Matrix4x4* origin) RETAIL_N32(FUN_0029c870);
    void PlayCameraTrackFrame(f32 share, PlayedCameraTrack* played, const CutsceneCameraTrack* track, u16 frame,
                              const Matrix4x4* origin) RETAIL_N32(FUN_0029a068);
}
