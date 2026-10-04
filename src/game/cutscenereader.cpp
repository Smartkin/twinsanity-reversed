#include "game/cutscenereader.h"

#include "game/archive.h"
#include "game/controllers.h"
#include "game/disk.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/stream.h"

// A cutscene's part read from its file (CutScene\_<number>_<part>.cts) into a cutscene, and destroyed. Its values are animations'
// data, each read into a disk manager node of its own: the tracks' values and values at the end and the camera's shots are floats
// (game/dynamicscenery.h's), the instances' joints and blend shapes and the camera's cuts 16 bit values (game/animation.h's)

extern "C"
{
    // A word of the module's statics nothing reads
    extern u32 g_UnreadCutsceneWord RETAIL(D_0030AA70);
}

namespace
{
constexpr u16 NoId = 0xFFFF;
constexpr u16 IdMask = 0x7FFF;

// The size of an animation's data of floats: bits 9-21 of its sections the static values' bytes, bits 22-31 the values a frame
u32 FloatDataSize(const AnimationDataInformation* data)
{
    constexpr u32 StaticBytesShift = 9;
    constexpr u32 StaticBytesMask = 0x1FFC;
    if (data->frames == 0)
    {
        return 0;
    }

    u32 sections = data->sections;
    u32 perFrame = sections >> AnimationDataInformation::FrameValuesShift;
    return (sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings) +
           (sections >> StaticBytesShift & StaticBytesMask) + perFrame * data->frames * sizeof(f32);
}

// An animation's data of floats read: its sections and frames, then its bytes into a disk manager node of its own (the one it had
// let go first)
void ReadFloatData(AnimationDataInformation* data, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&data->sections));
    stream->ReadS16(reinterpret_cast<s16*>(&data->frames));
    if (data->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &data->diskHandle);
    }

    u32 size = FloatDataSize(data);
    if (size == 0)
    {
        return;
    }

    s32 handle;
    DiskAllocate(&handle, GetDiskManager(), size, false, 0);
    data->diskHandle = handle;
    stream->Read(DiskLoadedMemory(GetDiskManager(), &data->diskHandle), size, 1);
}

// The instances' joints and blend shapes are animations' data of 16 bit values
void ReadShortData(AnimationDataSource* data, Stream* stream)
{
    ReadAnimationData(reinterpret_cast<AnimationDataInformation*>(data), stream);
}

void ConstructData(AnimationDataInformation* data)
{
    data->diskHandle = -1;
    data->sections = 0;
    data->frames = 0;
}

void ConstructData(AnimationDataSource* data)
{
    data->diskHandle = -1;
    data->layout = 0;
    data->frames = 0;
}

void ReleaseData(AnimationDataInformation* data)
{
    if (data->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &data->diskHandle);
    }
}

void ReleaseData(AnimationDataSource* data)
{
    if (data->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &data->diskHandle);
    }
}

// The ID of the queuing object's slot a track names: 0xFFFF for none, else the slot's ID
u16 SlotId(u16 id)
{
    return id == NoId ? NoId : id & IdMask;
}
}

Cutscene* ConstructCutscene(Cutscene* cutscene)
{
    cutscene->partFrames = Cutscene::PartFrames;
    cutscene->part = NoId;
    cutscene->camera.cuts.diskHandle = -1;
    cutscene->frames = 0;
    cutscene->number = NoId;
    cutscene->instanceTrackCount = 0;
    cutscene->soundTrackCount = 0;
    cutscene->emitterTrackCount = 0;
    cutscene->camera.shotCount = 0;
    cutscene->camera.shots = nullptr;
    cutscene->camera.endValues.diskHandle = -1;
    cutscene->camera.endValues.sections = 0;
    cutscene->camera.endValues.frames = 0;
    cutscene->camera.cuts.sections = 0;
    cutscene->camera.cuts.frames = 0;
    cutscene->instanceTracks = nullptr;
    cutscene->soundTracks = nullptr;
    cutscene->emitterTracks = nullptr;
    cutscene->unknown10[3] = 0;
    cutscene->unknown10[2] = 0;
    cutscene->unknown10[1] = 0;
    cutscene->unknown10[0] = 0;
    return cutscene;
}

void DestroyCutscene(Cutscene* cutscene, u32 destroyFlags)
{
    if (cutscene->instanceTracks != nullptr)
    {
        CutsceneInstanceTrack* track = cutscene->instanceTracks + ArrayCount(cutscene->instanceTracks);
        while (track != cutscene->instanceTracks)
        {
            track--;
            DestroyInstanceTrack(track, 0);
        }

        DeleteArray(cutscene->instanceTracks);
    }

    if (cutscene->soundTracks != nullptr)
    {
        CutsceneSoundTrack* track = cutscene->soundTracks + ArrayCount(cutscene->soundTracks);
        while (track != cutscene->soundTracks)
        {
            track--;
            ReleaseData(&track->endValues);
            ReleaseData(&track->values);
        }

        DeleteArray(cutscene->soundTracks);
    }

    if (cutscene->emitterTracks != nullptr)
    {
        CutsceneEmitterTrack* track = cutscene->emitterTracks + ArrayCount(cutscene->emitterTracks);
        while (track != cutscene->emitterTracks)
        {
            track--;
            ReleaseData(&track->endValues);
            ReleaseData(&track->values);
        }

        DeleteArray(cutscene->emitterTracks);
    }

    DestroyCameraTrack(&cutscene->camera, DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(cutscene);
    }
}

void DestroyCameraTrack(CutsceneCameraTrack* track, u32 destroyFlags)
{
    if (track->shots != nullptr)
    {
        AnimationDataInformation* shot = track->shots + ArrayCount(track->shots);
        while (shot != track->shots)
        {
            shot--;
            ReleaseData(shot);
        }

        DeleteArray(track->shots);
    }

    ReleaseData(&track->cuts);
    ReleaseData(&track->endValues);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(track);
    }
}

void DestroyInstanceTrack(CutsceneInstanceTrack* track, u32 destroyFlags)
{
    ReleaseData(&track->unknown40);
    ReleaseData(&track->unknown34);
    ReleaseData(&track->endValues);
    ReleaseData(&track->blendShapes);
    ReleaseData(&track->joints);
    ReleaseData(&track->values);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(track);
    }
}

void ReadCutscene(Cutscene* cutscene, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->frames));
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->partFrames));
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->number));
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->part));
    stream->ReadS8(reinterpret_cast<s8*>(&cutscene->instanceTrackCount));
    stream->ReadS8(reinterpret_cast<s8*>(&cutscene->soundTrackCount));
    stream->ReadS8(reinterpret_cast<s8*>(&cutscene->emitterTrackCount));
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->unknown08));
    for (u32& word : cutscene->unknown10)
    {
        stream->ReadS32(reinterpret_cast<s32*>(&word));
    }

    MakeCutsceneTracks(cutscene, cutscene->instanceTrackCount, cutscene->soundTrackCount, cutscene->emitterTrackCount);
    for (u32 index = 0; index < cutscene->instanceTrackCount; index++)
    {
        ReadInstanceTrack(&cutscene->instanceTracks[index], stream);
    }

    for (u32 index = 0; index < cutscene->soundTrackCount; index++)
    {
        ReadSoundTrack(&cutscene->soundTracks[index], stream);
    }

    for (u32 index = 0; index < cutscene->emitterTrackCount; index++)
    {
        ReadEmitterTrack(&cutscene->emitterTracks[index], stream);
    }

    ReadCameraTrack(&cutscene->camera, stream);
}

void MakeCutsceneTracks(Cutscene* cutscene, u8 instanceTracks, u8 soundTracks, u8 emitterTracks)
{
    constexpr u8 SoundTrackMade = 1;
    if (instanceTracks != 0)
    {
        CutsceneInstanceTrack* tracks = NewArray<CutsceneInstanceTrack>(instanceTracks);
        for (u32 index = 0; index < instanceTracks; index++)
        {
            CutsceneInstanceTrack* track = &tracks[index];
            track->model = NoId;
            track->hasBlendShapes = false;
            ConstructData(&track->values);
            ConstructData(&track->joints);
            ConstructData(&track->blendShapes);
            ConstructData(&track->endValues);
            ConstructData(&track->unknown34);
            ConstructData(&track->unknown40);
        }

        cutscene->instanceTracks = tracks;
    }

    if (soundTracks != 0)
    {
        CutsceneSoundTrack* tracks = NewArray<CutsceneSoundTrack>(soundTracks);
        for (u32 index = 0; index < soundTracks; index++)
        {
            CutsceneSoundTrack* track = &tracks[index];
            track->sound = NoId;
            track->unknown02 = SoundTrackMade;
            ConstructData(&track->values);
            ConstructData(&track->endValues);
        }

        cutscene->soundTracks = tracks;
    }

    if (emitterTracks != 0)
    {
        CutsceneEmitterTrack* tracks = NewArray<CutsceneEmitterTrack>(emitterTracks);
        for (u32 index = 0; index < emitterTracks; index++)
        {
            CutsceneEmitterTrack* track = &tracks[index];
            track->unknown00 = NoId;
            track->unknown02 = false;
            ConstructData(&track->values);
            ConstructData(&track->endValues);
        }

        cutscene->emitterTracks = tracks;
    }

    cutscene->emitterTrackCount = emitterTracks;
    cutscene->instanceTrackCount = instanceTracks;
    cutscene->soundTrackCount = soundTracks;
}

void ReadCameraTrack(CutsceneCameraTrack* track, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS8(reinterpret_cast<s8*>(&track->shotCount));
    u8 count = track->shotCount;
    AnimationDataInformation* shots = NewArray<AnimationDataInformation>(count);
    for (u32 index = 0; index < count; index++)
    {
        ConstructData(&shots[index]);
    }

    track->shots = shots;
    track->shotCount = count;
    for (u32 index = 0; index < track->shotCount; index++)
    {
        ReadFloatData(&track->shots[index], stream);
    }

    ReadAnimationData(&track->cuts, stream);
    ReadFloatData(&track->endValues, stream);
}

void ReadInstanceTrack(CutsceneInstanceTrack* track, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS16(reinterpret_cast<s16*>(&track->model));
    u16 model;
    GetObjectModelId(&model, G_VideoController->object, track->model);
    track->model = SlotId(model);
    stream->ReadBool(&track->hasBlendShapes);
    ReadFloatData(&track->values, stream);
    ReadFloatData(&track->endValues, stream);
    ReadShortData(&track->joints, stream);
    ReadShortData(&track->unknown34, stream);
    if (track->hasBlendShapes)
    {
        ReadShortData(&track->blendShapes, stream);
        ReadShortData(&track->unknown40, stream);
    }
}

void ReadEmitterTrack(CutsceneEmitterTrack* track, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS16(reinterpret_cast<s16*>(&track->unknown00));
    stream->ReadBool(&track->unknown02);
    ReadFloatData(&track->values, stream);
    ReadFloatData(&track->endValues, stream);
}

void ReadSoundTrack(CutsceneSoundTrack* track, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS16(reinterpret_cast<s16*>(&track->sound));
    u16 sound;
    GetObjectSoundId(&sound, G_VideoController->object, track->sound);
    track->sound = SlotId(sound);
    ReadFloatData(&track->values, stream);
    ReadFloatData(&track->endValues, stream);
}

void CutsceneReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CutsceneReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, 1);
    ReadCutscene(cutscene, &stream);
    CutscenePartRead(controller, cutscene);
    stream.Destroy(2);
}

void CutsceneReader::Missing(u8*, u32, ReaderStack*)
{
}

void InitCutsceneModule(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_UnreadCutsceneWord = 0;
}

void ConstructCutsceneModule()
{
    InitCutsceneModule(1, 0xFFFF);
}
