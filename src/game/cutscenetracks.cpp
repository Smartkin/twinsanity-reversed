#include "game/cutscenes.h"

#include "game/camerarig.h"
#include "game/cutscenereader.h"
#include "game/disk.h"
#include "game/dynamicscenery.h"
#include "game/instanceparticles.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/reference.h"

// A frame of a cutscene's tracks played: each track's values read like a dynamic scenery's (its first joint's float channels, a
// share of the way from the frame to the next), moving an instance, an emitter, a sound's position or the camera

EABI_EXPORT(FUN_0029b990, PlayInstanceTrackFrame);
EABI_EXPORT(FUN_0029c340, PlayEmitterTrackFrame);
EABI_EXPORT(FUN_0029c870, PlaySoundTrackFrame);
EABI_EXPORT(FUN_0029a068, PlayCameraTrackFrame);

namespace
{
// The cuts' values are in 4096ths
constexpr f32 CutUnit = 0x1p-12f;

DynamicAnimationData* NewValues(const AnimationDataInformation* information)
{
    auto* values = static_cast<DynamicAnimationData*>(MemoryAllocate(sizeof(DynamicAnimationData)));
    values->information = const_cast<AnimationDataInformation*>(information);
    return values;
}

// The data of the values a track plays: the track's end values at the end frame (made again every time), else its values, made
// again once it plays another track
template <typename Played, typename Track>
void TakeValues(Played* played, const Track* track, u16 frame)
{
    if (frame == Cutscene::EndFrame)
    {
        if (played->values != nullptr)
        {
            MemoryDeallocate2_(played->values);
        }

        played->values = NewValues(&track->endValues);
    }
    else if (played->track != nullptr && played->track != track)
    {
        if (played->values != nullptr)
        {
            MemoryDeallocate2_(played->values);
        }

        played->values = nullptr;
    }

    played->track = track;
    if (played->values == nullptr)
    {
        played->values = NewValues(&track->values);
    }
}

// Where a frame's float values start: past the static values, as many values in as a frame has times the frame
const f32* FrameValues(const f32* statics, AnimationLayout layout, u32 frame)
{
    return statics + layout.staticValues + layout.frameValues * frame;
}

// Where a frame's 16 bit values start
const s16* ShortFrameValues(const s16* statics, AnimationLayout layout, u32 frame)
{
    return statics + layout.staticValues + layout.frameValues * frame;
}

// The values found in the disk manager for a frame (the end frame's values are the first frame's)
void FindFrameValues(DynamicAnimationData* values, u16 frame)
{
    AnimationDataInformation* information = values->information;
    u8* memory = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    values->settings = reinterpret_cast<const JointTrackSettings*>(memory);
    const f32* statics = reinterpret_cast<const f32*>(memory + information->layout.joints * sizeof(JointTrackSettings));
    values->statics = statics;
    if (frame == Cutscene::EndFrame)
    {
        values->current = FrameValues(statics, information->layout, 0);
        values->next = FrameValues(statics, values->information->layout, 0);
        return;
    }

    values->current = FrameValues(statics, information->layout, frame);
    values->next = FrameValues(statics, values->information->layout, frame + 1);
}

// The reader of the values' first joint's channels
DynamicTrackReader ReaderOf(const DynamicAnimationData* values)
{
    const JointTrackSettings* joint = values->settings;
    DynamicTrackReader reader;
    reader.statics = joint->statics;
    u32 channelCount = joint->flags.channels;
    reader.channels = static_cast<u16>((1 << channelCount) - 1);
    reader.staticValues = values->statics + joint->staticIndex;
    reader.current = values->current + joint->frameIndex;
    reader.next = values->next + joint->frameIndex;
    return reader;
}

// The next channel's value (as the dynamic scenery's): its static value, or the share of the way from this frame's value to the
// next frame's
f32 ReadTrackValue(DynamicTrackReader* reader, f32 share)
{
    f32 value;
    if ((reader->statics & 1) == 0)
    {
        value = *reader->next * share + *reader->current * (1.0f - share);
        reader->current++;
        reader->next++;
    }
    else
    {
        value = *reader->staticValues++;
    }

    reader->statics >>= 1;
    reader->channels >>= 1;
    return value;
}

// The next channel passed over unread
void SkipTrackValue(DynamicTrackReader* reader)
{
    if ((reader->statics & 1) == 0)
    {
        reader->current++;
        reader->next++;
    }
    else
    {
        reader->staticValues++;
    }

    reader->statics >>= 1;
    reader->channels >>= 1;
}

// The first channel of the cuts at a frame. Retail bug: the frame isn't checked for the end frame, so skipping a cutscene reads
// its cuts 0xFFFF frames in, past their data, and a value above 0 there takes a shot's values in place of the end's
f32 CutValue(AnimationData* cuts, u32 frame, f32 share)
{
    AnimationDataInformation* information = cuts->information;
    u8* memory = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    cuts->settings = reinterpret_cast<const JointTrackSettings*>(memory);
    const s16* statics = reinterpret_cast<const s16*>(memory + information->layout.joints * sizeof(JointTrackSettings));
    cuts->statics = statics;
    cuts->current = ShortFrameValues(statics, information->layout, frame);
    cuts->next = ShortFrameValues(statics, cuts->information->layout, frame + 1);
    const JointTrackSettings* joint = cuts->settings;
    if ((joint->statics & 1) != 0)
    {
        return static_cast<f32>(cuts->statics[joint->staticIndex]) * CutUnit;
    }

    return static_cast<f32>(cuts->next[joint->frameIndex]) * CutUnit * share +
           static_cast<f32>(cuts->current[joint->frameIndex]) * CutUnit * (1.0f - share);
}
}

void PlayInstanceTrackFrame(f32 share, PlayedInstanceTrack* played, const CutsceneInstanceTrack* track, u16 frame,
                            const Matrix4x4* origin)
{
    TakeValues(played, track, frame);
    FindFrameValues(played->values, frame);
    DynamicTrackReader reader = ReaderOf(played->values);
    played->position.x = ReadTrackValue(&reader, share);
    played->position.y = ReadTrackValue(&reader, share);
    played->position.z = ReadTrackValue(&reader, share);
    played->position.w = 1.0f;
    s32 x;
    s32 y;
    s32 z;
    ReadTrackAngle(&x, &reader, share);
    ReadTrackAngle(&y, &reader, share);
    ReadTrackAngle(&z, &reader, share);
    f32 shown = ReadTrackValue(&reader, share);
    InstanceContext* instance = played->instance;
    if (ShownAbove < shown && !instance->flags.visible)
    {
        instance->flags.visible = 1;
    }
    else if (shown < ShownAbove && instance->flags.visible)
    {
        instance->flags.visible = 0;
    }

    GetRotationXYZ(&played->rotation, &x, &y, &z);
    if (origin == nullptr)
    {
        instance = played->instance;
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&played->position))
        {
            QueueObject(instance);
        }

        instance = played->instance;
        place = instance->place;
        place->SyncRotation();
        if (place->TurnTo(&played->rotation))
        {
            QueueObject(instance);
        }

        return;
    }

    // In the origin's space: the instance placed, and the played track given the instance's place
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    MatrixFromRotation(&matrix, &played->rotation);
    *RowOf(&matrix, PositionRow) = played->position;
    MultiplyInPlace(&matrix, origin);
    instance = played->instance;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(RowOf(&matrix, PositionRow)))
    {
        QueueObject(instance);
    }

    instance = played->instance;
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    place = played->instance->place;
    place->SyncPosition();
    played->position = place->position;
    place = played->instance->place;
    place->SyncRotation();
    played->rotation = place->rotation;
}

void PlayEmitterTrackFrame(f32 share, PlayedEmitterTrack* played, const CutsceneEmitterTrack* track, u16 frame,
                           const Matrix4x4* origin)
{
    TakeValues(played, track, frame);
    FindFrameValues(played->values, frame);
    DynamicTrackReader reader = ReaderOf(played->values);
    played->position.x = ReadTrackValue(&reader, share);
    played->position.y = ReadTrackValue(&reader, share);
    played->position.z = ReadTrackValue(&reader, share);
    played->position.w = 1.0f;
    s32 angle;
    ReadTrackAngle(&angle, &reader, share);
    played->angles[0] = angle;
    ReadTrackAngle(&angle, &reader, share);
    played->angles[1] = angle;
    ReadTrackAngle(&angle, &reader, share);
    played->angles[2] = angle;
    f32 on = ReadTrackValue(&reader, share);
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    s32 x = played->angles[0];
    s32 y = played->angles[1];
    s32 z = played->angles[2];
    MatrixFromAngles(&matrix, &x, &y, &z);
    *RowOf(&matrix, PositionRow) = played->position;
    if (origin != nullptr)
    {
        MultiplyInPlace(&matrix, origin);
        played->position = *RowOf(&matrix, PositionRow);
        AnglesOfMatrix(&matrix, played->angles);
    }

    SetEmitterFrame(played->emitter, &matrix);
    if (on < ShownAbove && played->emitter != NoEmitter)
    {
        played->emitter = NoEmitter;
    }
}

void PlaySoundTrackFrame(f32 share, PlayedSoundTrack* played, const CutsceneSoundTrack* track, u16 frame,
                         const Matrix4x4* origin)
{
    TakeValues(played, track, frame);
    FindFrameValues(played->values, frame);
    DynamicTrackReader reader = ReaderOf(played->values);
    played->position.x = ReadTrackValue(&reader, share);
    played->position.y = ReadTrackValue(&reader, share);
    played->position.z = ReadTrackValue(&reader, share);
    played->position.w = 1.0f;
    SkipTrackValue(&reader);
    played->volume = ReadTrackValue(&reader, share);
    if (origin != nullptr)
    {
        VuTransformPoint(origin, &played->position, &played->position);
    }
}

void PlayCameraTrackFrame(f32 share, PlayedCameraTrack* played, const CutsceneCameraTrack* track, u16 frame,
                          const Matrix4x4* origin)
{
    // The shot's values as the other tracks' (the end's at the end frame); the cuts kept while it plays the same track
    if (frame == Cutscene::EndFrame)
    {
        if (played->values != nullptr)
        {
            MemoryDeallocate2_(played->values);
        }

        played->values = NewValues(&track->endValues);
    }
    else if (played->track != nullptr && played->track != track)
    {
        if (played->values != nullptr)
        {
            MemoryDeallocate2_(played->values);
        }

        if (played->cuts != nullptr)
        {
            MemoryDeallocate2_(played->cuts);
        }

        played->values = nullptr;
        played->cuts = nullptr;
    }

    played->track = track;
    if (played->cuts == nullptr)
    {
        auto* cuts = static_cast<AnimationData*>(MemoryAllocate(sizeof(AnimationData)));
        cuts->information = const_cast<AnimationDataInformation*>(&track->cuts);
        played->cuts = cuts;
    }

    if (0.0f < CutValue(played->cuts, frame, share))
    {
        if (played->cutting == 0)
        {
            NextShot(played);
            played->cutting = 1;
        }
    }
    else
    {
        played->cutting = 0;
    }

    if (played->values == nullptr)
    {
        played->values = NewValues(&track->shots[played->shot]);
    }

    FindFrameValues(played->values, frame);
    DynamicTrackReader reader = ReaderOf(played->values);
    Vector4 position;
    position.x = ReadTrackValue(&reader, share);
    position.y = ReadTrackValue(&reader, share);
    position.z = ReadTrackValue(&reader, share);
    position.w = 1.0f;
    s32 x;
    s32 y;
    s32 z;
    ReadTrackAngle(&x, &reader, share);
    ReadTrackAngle(&y, &reader, share);
    ReadTrackAngle(&z, &reader, share);
    f32 fieldOfView = ReadTrackValue(&reader, share);
    Vector4 rotation;
    GetRotationXYZ(&rotation, &x, &y, &z);
    if (origin != nullptr)
    {
        // The angles worked out again from the turn in the origin's space, which nothing reads
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        MatrixFromRotation(&matrix, &rotation);
        *RowOf(&matrix, PositionRow) = position;
        MultiplyInPlace(&matrix, origin);
        position = *RowOf(&matrix, PositionRow);
        GetRotationVec(&rotation, &matrix);
        AnglesOfRotation(&rotation, &x, &y, &z);
    }

    CutsceneCameraRig* camera = played->camera;
    camera->ownPositioner.position = position;
    camera = played->camera;
    camera->ownPositioner.rotation = rotation;
    s32 angle;
    AngleFrom(&angle, fieldOfView, AngleRadians);
    played->camera->ownPositioner.fov = angle;
}
