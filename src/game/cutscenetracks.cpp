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
constexpr u16 EndFrame = 0xFFFF;
constexpr s32 NoEmitter = -1;
// Above it an instance is shown and an emitter kept, below it an instance is hidden and an emitter let go of
constexpr f32 ShownAbove = 0.5f;
// The cuts' values are in 4096ths
constexpr f32 CutUnit = 0x1p-12f;
// The bytes the static float values take (the count of bits 11-21, times 4)
constexpr u32 StaticBytesShift = 9;
constexpr u32 StaticBytesMask = 0x1FFC;

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
    if (frame == EndFrame)
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
const f32* FrameValues(const f32* statics, u32 sections, u32 frame)
{
    const u8* values = reinterpret_cast<const u8*>(statics) + (sections >> StaticBytesShift & StaticBytesMask);
    return reinterpret_cast<const f32*>(values + (sections >> AnimationDataInformation::FrameValuesShift) * frame * sizeof(f32));
}

// Where a frame's 16 bit values start
const s16* ShortFrameValues(const s16* statics, u32 sections, u32 frame)
{
    const u8* values = reinterpret_cast<const u8*>(statics) +
                       (sections >> AnimationDataInformation::StaticBytesShift & AnimationDataInformation::StaticBytesMask);
    return reinterpret_cast<const s16*>(values + (sections >> AnimationDataInformation::FrameValuesShift) * frame * sizeof(s16));
}

// The values found in the disk manager for a frame (the end frame's values are the first frame's)
void FindFrameValues(DynamicAnimationData* values, u16 frame)
{
    AnimationDataInformation* information = values->information;
    u8* memory = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    values->settings = reinterpret_cast<const JointTrackSettings*>(memory);
    const f32* statics = reinterpret_cast<const f32*>(
        memory + (information->sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings));
    values->statics = statics;
    if (frame == EndFrame)
    {
        values->current = FrameValues(statics, information->sections, 0);
        values->next = FrameValues(statics, values->information->sections, 0);
        return;
    }

    values->current = FrameValues(statics, information->sections, frame);
    values->next = FrameValues(statics, values->information->sections, frame + 1);
}

// The reader of the values' first joint's channels
DynamicTrackReader ReaderOf(const DynamicAnimationData* values)
{
    const JointTrackSettings* joint = values->settings;
    DynamicTrackReader reader;
    reader.statics = joint->statics;
    u32 channelCount = joint->flags >> JointTrackSettings::ChannelsShift & JointTrackSettings::ChannelsMask;
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
    const s16* statics = reinterpret_cast<const s16*>(
        memory + (information->sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings));
    cuts->statics = statics;
    cuts->current = ShortFrameValues(statics, information->sections, frame);
    cuts->next = ShortFrameValues(statics, cuts->information->sections, frame + 1);
    const JointTrackSettings* joint = cuts->settings;
    if ((joint->statics & 1) != 0)
    {
        return static_cast<f32>(cuts->statics[joint->staticIndex]) * CutUnit;
    }

    return static_cast<f32>(cuts->next[joint->frameIndex]) * CutUnit * share +
           static_cast<f32>(cuts->current[joint->frameIndex]) * CutUnit * (1.0f - share);
}
}

void PlayInstanceTrackFrame(f32 share, PlayedInstanceTrack* track, const CutsceneInstanceTrack* data, u16 frame,
                            const Matrix4x4* origin)
{
    TakeValues(track, data, frame);
    FindFrameValues(track->values, frame);
    DynamicTrackReader reader = ReaderOf(track->values);
    track->position.x = ReadTrackValue(&reader, share);
    track->position.y = ReadTrackValue(&reader, share);
    track->position.z = ReadTrackValue(&reader, share);
    track->position.w = 1.0f;
    s32 x;
    s32 y;
    s32 z;
    ReadTrackAngle(&x, &reader, share);
    ReadTrackAngle(&y, &reader, share);
    ReadTrackAngle(&z, &reader, share);
    f32 shown = ReadTrackValue(&reader, share);
    InstanceContext* instance = track->instance;
    if (ShownAbove < shown && (instance->flags & ReferencedObject::FlagVisible) == 0)
    {
        instance->flags |= ReferencedObject::FlagVisible;
    }
    else if (shown < ShownAbove && (instance->flags & ReferencedObject::FlagVisible) != 0)
    {
        instance->flags &= ~ReferencedObject::FlagVisible;
    }

    GetRotationXYZ(&track->rotation, &x, &y, &z);
    if (origin == nullptr)
    {
        instance = track->instance;
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&track->position))
        {
            QueueObject(instance);
        }

        instance = track->instance;
        place = instance->place;
        place->SyncRotation();
        if (place->TurnTo(&track->rotation))
        {
            QueueObject(instance);
        }

        return;
    }

    // In the origin's space: the instance placed, and the track given the instance's place
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    MatrixFromRotation(&matrix, &track->rotation);
    *RowOf(&matrix, 3) = track->position;
    MultiplyInPlace(&matrix, origin);
    instance = track->instance;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(RowOf(&matrix, 3)))
    {
        QueueObject(instance);
    }

    instance = track->instance;
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    place = track->instance->place;
    place->SyncPosition();
    track->position = place->position;
    place = track->instance->place;
    place->SyncRotation();
    track->rotation = place->rotation;
}

void PlayEmitterTrackFrame(f32 share, PlayedEmitterTrack* track, const CutsceneEmitterTrack* data, u16 frame,
                           const Matrix4x4* origin)
{
    TakeValues(track, data, frame);
    FindFrameValues(track->values, frame);
    DynamicTrackReader reader = ReaderOf(track->values);
    track->position.x = ReadTrackValue(&reader, share);
    track->position.y = ReadTrackValue(&reader, share);
    track->position.z = ReadTrackValue(&reader, share);
    track->position.w = 1.0f;
    s32 angle;
    ReadTrackAngle(&angle, &reader, share);
    track->angles[0] = angle;
    ReadTrackAngle(&angle, &reader, share);
    track->angles[1] = angle;
    ReadTrackAngle(&angle, &reader, share);
    track->angles[2] = angle;
    f32 on = ReadTrackValue(&reader, share);
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    s32 x = track->angles[0];
    s32 y = track->angles[1];
    s32 z = track->angles[2];
    MatrixFromAngles(&matrix, &x, &y, &z);
    *RowOf(&matrix, 3) = track->position;
    if (origin != nullptr)
    {
        MultiplyInPlace(&matrix, origin);
        track->position = *RowOf(&matrix, 3);
        AnglesOfMatrix(&matrix, track->angles);
    }

    SetEmitterFrame(track->emitter, &matrix);
    if (on < ShownAbove && track->emitter != NoEmitter)
    {
        track->emitter = NoEmitter;
    }
}

void PlaySoundTrackFrame(f32 share, PlayedSoundTrack* track, const CutsceneSoundTrack* data, u16 frame,
                         const Matrix4x4* origin)
{
    TakeValues(track, data, frame);
    FindFrameValues(track->values, frame);
    DynamicTrackReader reader = ReaderOf(track->values);
    track->position.x = ReadTrackValue(&reader, share);
    track->position.y = ReadTrackValue(&reader, share);
    track->position.z = ReadTrackValue(&reader, share);
    track->position.w = 1.0f;
    SkipTrackValue(&reader);
    track->volume = ReadTrackValue(&reader, share);
    if (origin != nullptr)
    {
        VuTransformPoint(origin, &track->position, &track->position);
    }
}

void PlayCameraTrackFrame(f32 share, PlayedCameraTrack* track, const CutsceneCameraTrack* data, u16 frame,
                          const Matrix4x4* origin)
{
    // The shot's values as the other tracks' (the end's at the end frame); the cuts kept while it plays the same track
    if (frame == EndFrame)
    {
        if (track->values != nullptr)
        {
            MemoryDeallocate2_(track->values);
        }

        track->values = NewValues(&data->endValues);
    }
    else if (track->track != nullptr && track->track != data)
    {
        if (track->values != nullptr)
        {
            MemoryDeallocate2_(track->values);
        }

        if (track->cuts != nullptr)
        {
            MemoryDeallocate2_(track->cuts);
        }

        track->values = nullptr;
        track->cuts = nullptr;
    }

    track->track = data;
    if (track->cuts == nullptr)
    {
        auto* cuts = static_cast<AnimationData*>(MemoryAllocate(sizeof(AnimationData)));
        cuts->information = const_cast<AnimationDataInformation*>(&data->cuts);
        track->cuts = cuts;
    }

    if (0.0f < CutValue(track->cuts, frame, share))
    {
        if (track->cutting == 0)
        {
            NextShot(track);
            track->cutting = 1;
        }
    }
    else
    {
        track->cutting = 0;
    }

    if (track->values == nullptr)
    {
        track->values = NewValues(&data->shots[track->shot]);
    }

    FindFrameValues(track->values, frame);
    DynamicTrackReader reader = ReaderOf(track->values);
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
        *RowOf(&matrix, 3) = position;
        MultiplyInPlace(&matrix, origin);
        position = *RowOf(&matrix, 3);
        GetRotationVec(&rotation, &matrix);
        AnglesOfRotation(&rotation, &x, &y, &z);
    }

    CutsceneCameraRig* camera = track->camera;
    camera->ownPositioner.position = position;
    camera = track->camera;
    camera->ownPositioner.rotation = rotation;
    s32 angle;
    AngleFrom(&angle, fieldOfView, AngleRadians);
    track->camera->ownPositioner.fov = angle;
}
