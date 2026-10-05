#pragma once

#include "common.h"
#include "game/cutscenes.h"

class Stream;
struct GameObject;
struct InstanceContext;
struct TimeClock;
struct VideoController;

// The cutscenes' reading side (game/cutscenes.h has the playing side): a part read from its file into a cutscene (its values'
// data each in a disk manager node of its own) and destroyed, and what the video controller does around the playing: cutscenes
// and music-only ones queued, started and stopped by the scripts, a part read handed over, the tracks made, played a frame at a
// time and let go of, the instances it was given for models and the update of each of its states

extern "C"
{
    // A part made empty (150 frames, no number, no part, no tracks) and destroyed (its tracks' data let go of, last to first)
    Cutscene* ConstructCutscene(Cutscene* cutscene) RETAIL(FUN_0029f580);
    void DestroyCutscene(Cutscene* cutscene, u32 destroyFlags) RETAIL(FUN_0029ddb0);
    void DestroyCameraTrack(CutsceneCameraTrack* track, u32 destroyFlags) RETAIL(FUN_0029ccf0);
    void DestroyInstanceTrack(CutsceneInstanceTrack* track, u32 destroyFlags) RETAIL(FUN_0029e428);
    // A part read: a word nothing keeps, its frames, the part's frames, the cutscene's number and the part's, its counts of
    // tracks of instances, sounds and emitters, a halfword and four words, then those tracks and the camera's
    void ReadCutscene(Cutscene* cutscene, Stream* stream) RETAIL(ReadCutscene);
    // The part's arrays of tracks made for the counts, each track empty (an array of none isn't made: the old one stays)
    void MakeCutsceneTracks(Cutscene* cutscene, u8 instanceTracks, u8 soundTracks, u8 emitterTracks) RETAIL(FUN_0029df50);
    // The tracks read, each after a word nothing keeps: the camera's count of shots, its shots' values, its cuts and its values at
    // the end; an instance's model (the ID of the queuing object's model slot it names), whether it has blend shapes, its values,
    // its values at the end, its joints, a data nothing plays, and with blend shapes its blend shapes and another such data; an
    // emitter's halfword and flag, its values and its values at the end; a sound's sound (the queuing object's sound slot it names),
    // its values and its values at the end
    void ReadCameraTrack(CutsceneCameraTrack* track, Stream* stream) RETAIL(FUN_0029cde8);
    void ReadInstanceTrack(CutsceneInstanceTrack* track, Stream* stream) RETAIL(FUN_0029d200);
    void ReadEmitterTrack(CutsceneEmitterTrack* track, Stream* stream) RETAIL(FUN_0029d898);
    void ReadSoundTrack(CutsceneSoundTrack* track, Stream* stream) RETAIL(FUN_0029db18);
    // A part read handed to the video controller: dropped while it plays nothing, has finished or was stopped (and the part
    // being read with it while a cutscene is queued); the first part makes the cutscene ready (started when a start waits for it),
    // the next part is marked read
    void CutscenePartRead(VideoController* controller, Cutscene* cutscene) RETAIL(FUN_0029eac8);

    // The module's start-up (a word of its statics nothing reads cleared) and its entry in the start-up list
    void InitCutsceneModule(u32 initialise, u32 priority) RETAIL(FUN_0029e340);
    void ConstructCutsceneModule() RETAIL(FUN_0029f600);

    // The cutscene of a number queued for an object's instance (the object's slots the tracks name, the instance's place the
    // tracks' space, its chunk the one the cutscene's instances are made in): its first part read, and the context music slot's
    // music asked for. Returns 1
    u32 QueueObjectMovie(VideoController* controller, GameObject* object, InstanceContext* instance, s32 number)
        RETAIL(FUN_0029e5f8);
    // A music-only cutscene queued: a track asked for in the context music slot (looped with the flag), queued with the clock
    // when it was taken. Returns whether it was
    u32 QueueMovie(VideoController* controller, s32 track, TimeClock* clock, u32 loops) RETAIL(FUN_0029e790);
    // Whether a music-only cutscene's music is prepared
    u32 VideoReady(VideoController* controller) RETAIL(FUN_0029e8a8);
    // A queued music-only cutscene played at the clock's time once its music is prepared (else it waits a frame): whether it was
    u32 StartQueuedMovie(VideoController* controller, TimeClock* clock) RETAIL(FUN_0029e8e0);
    // The cutscene stopped by a script: what was read let go of while it's ready (and what's read later), the music stopped
    void StopScriptMovie(VideoController* controller) RETAIL(FUN_0029ea38);
    // Whether the cutscene was seen (the persistent flag of the instance it was queued for)
    u32 CutsceneSeen(VideoController* controller) RETAIL(FUN_0029e9e0);
    void StopCutsceneMusic(VideoController* controller) RETAIL(FUN_0029ebe8);

    // The instance the controller was given for a model (none), and the instance given for a model (replacing the one it had, a
    // 16th model left out)
    InstanceContext* ModelInstance(VideoController* controller, u16 model) RETAIL(FUN_0029ec08);
    void SetVideoModelInstance(VideoController* controller, u32 model, InstanceContext* instance) RETAIL(FUN_0029f1e0);
    // Every instance the controller was given that its instance tracks took given back, and one given back: its agent unfrozen,
    // its animation stopped
    void GiveBackInstances(VideoController* controller) RETAIL(FUN_0029ecf8);
    void GiveBackInstance(VideoController* controller, InstanceContext* instance) RETAIL(FUN_0029ed70);
    // The skip hint and the disc error's title drawn
    void DrawSkipHint(VideoController* controller) RETAIL(FUN_0029ede8);
    void DrawDiscError(VideoController* controller) RETAIL(FUN_0029ee60);
    // The update of the idle, ready and finished states (nothing), and of the paused one: while its clock is stopped the state it
    // was paused in put back (a playing cutscene pauses again)
    void UpdateIdleCutscene(VideoController* controller) RETAIL(FUN_0029edd8);
    void UpdateReadyCutscene(VideoController* controller) RETAIL(FUN_0029ede0);
    void UpdatePausedCutscene(VideoController* controller) RETAIL(FUN_0029ef00);
    void UpdateFinishedCutscene(VideoController* controller) RETAIL(FUN_0029ef30);
    // The playing part's frame of each track of a kind played (in the origin's space when its tracks are placed there)
    void PlayInstanceTracks(VideoController* controller) RETAIL(FUN_0029ef38);
    void PlayEmitterTracks(VideoController* controller) RETAIL(FUN_0029eff8);
    void PlaySoundTracks(VideoController* controller) RETAIL(FUN_0029f0c8);
    void PlayCameraTrack(VideoController* controller) RETAIL(FUN_0029f198);

    // The controller's tracks made empty (the turns none, the places the origin), let go of (an instance made for the cutscene
    // released, an emitter stopped) and given an instance (and whether it's one the controller was given), and the camera's
    // next shot (its values' data let go of)
    void ConstructCameraTrack(PlayedCameraTrack* track) RETAIL(FUN_0029e388);
    void NextShot(PlayedCameraTrack* track) RETAIL(FUN_0029e3b0);
    PlayedInstanceTrack* ConstructInstanceTrack(PlayedInstanceTrack* track) RETAIL(FUN_0029f280);
    void ResetInstanceTrack(PlayedInstanceTrack* track) RETAIL(FUN_0029f2a8);
    void ReleaseInstanceTrack(PlayedInstanceTrack* track) RETAIL(FUN_0029f328);
    void SetTrackInstance(PlayedInstanceTrack* track, InstanceContext* instance, u32 given) RETAIL(FUN_0029f3f0);
    PlayedEmitterTrack* ConstructEmitterTrack(PlayedEmitterTrack* track) RETAIL(FUN_0029f410);
    void ReleaseEmitterTrack(PlayedEmitterTrack* track) RETAIL(FUN_0029f488);
    PlayedSoundTrack* ConstructSoundTrack(PlayedSoundTrack* track) RETAIL(FUN_0029f4d0);
    void ResetSoundTrack(PlayedSoundTrack* track) RETAIL(FUN_0029f500);
    void ReleaseSoundTrack(PlayedSoundTrack* track) RETAIL(FUN_0029f570);
}
