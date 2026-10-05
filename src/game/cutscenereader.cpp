#include "game/cutscenereader.h"

#include "game/archive.h"
#include "game/controllers.h"
#include "game/disk.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/sound.h"
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

// The size of an animation's data of floats: its joints' settings, its static values and the values a frame of every frame,
// 4 bytes each
u32 FloatDataSize(const AnimationDataInformation* information)
{
    if (information->frames == 0)
    {
        return 0;
    }

    AnimationLayout layout = information->layout;
    return layout.joints * sizeof(JointTrackSettings) + layout.staticValues * sizeof(f32) +
           layout.frameValues * information->frames * sizeof(f32);
}

// An animation's data of floats read: its layout and frames, then its bytes into a disk manager node of its own (the one it had
// let go first)
void ReadFloatData(AnimationDataInformation* information, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&information->layout.value));
    stream->ReadS16(reinterpret_cast<s16*>(&information->frames));
    if (information->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &information->diskHandle);
    }

    u32 size = FloatDataSize(information);
    if (size == 0)
    {
        return;
    }

    s32 handle;
    DiskAllocate(&handle, GetDiskManager(), size, false, 0);
    information->diskHandle = handle;
    stream->Read(DiskLoadedMemory(GetDiskManager(), &information->diskHandle), size, 1);
}

// The instances' joints and blend shapes are animations' data of 16 bit values
void ReadShortData(AnimationDataSource* source, Stream* stream)
{
    ReadAnimationData(reinterpret_cast<AnimationDataInformation*>(source), stream);
}

void ConstructData(AnimationDataInformation* information)
{
    information->diskHandle = -1;
    information->layout.value = 0;
    information->frames = 0;
}

void ConstructData(AnimationDataSource* source)
{
    source->diskHandle = -1;
    source->layout.value = 0;
    source->frames = 0;
}

void ReleaseData(AnimationDataInformation* information)
{
    if (information->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &information->diskHandle);
    }
}

void ReleaseData(AnimationDataSource* source)
{
    if (source->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &source->diskHandle);
    }
}

// The ID of the queuing object's slot a track names: 0xFFFF for none, else the slot's ID
u16 SlotId(u16 id)
{
    return id == UndefinedId ? UndefinedId : id & ResourceIndexMask;
}
}

Cutscene* ConstructCutscene(Cutscene* cutscene)
{
    cutscene->partFrames = Cutscene::PartFrames;
    cutscene->part = Cutscene::NoNumber;
    cutscene->camera.cuts.diskHandle = -1;
    cutscene->frames = 0;
    cutscene->number = Cutscene::NoNumber;
    cutscene->instanceTrackCount = 0;
    cutscene->soundTrackCount = 0;
    cutscene->emitterTrackCount = 0;
    cutscene->camera.shotCount = 0;
    cutscene->camera.shots = nullptr;
    cutscene->camera.endValues.diskHandle = -1;
    cutscene->camera.endValues.layout.value = 0;
    cutscene->camera.endValues.frames = 0;
    cutscene->camera.cuts.layout.value = 0;
    cutscene->camera.cuts.frames = 0;
    cutscene->instanceTracks = nullptr;
    cutscene->soundTracks = nullptr;
    cutscene->emitterTracks = nullptr;
    cutscene->unused10[3] = 0;
    cutscene->unused10[2] = 0;
    cutscene->unused10[1] = 0;
    cutscene->unused10[0] = 0;
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
    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(track);
    }
}

void DestroyInstanceTrack(CutsceneInstanceTrack* track, u32 destroyFlags)
{
    ReleaseData(&track->unused40);
    ReleaseData(&track->unused34);
    ReleaseData(&track->endValues);
    ReleaseData(&track->blendShapes);
    ReleaseData(&track->joints);
    ReleaseData(&track->values);
    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    stream->ReadS16(reinterpret_cast<s16*>(&cutscene->unused08));
    for (u32& word : cutscene->unused10)
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
            track->model = NoModelId;
            track->hasBlendShapes = false;
            ConstructData(&track->values);
            ConstructData(&track->joints);
            ConstructData(&track->blendShapes);
            ConstructData(&track->endValues);
            ConstructData(&track->unused34);
            ConstructData(&track->unused40);
        }

        cutscene->instanceTracks = tracks;
    }

    if (soundTracks != 0)
    {
        CutsceneSoundTrack* tracks = NewArray<CutsceneSoundTrack>(soundTracks);
        for (u32 index = 0; index < soundTracks; index++)
        {
            CutsceneSoundTrack* track = &tracks[index];
            track->sound = NoSoundId;
            track->unused02 = SoundTrackMade;
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
            track->unused00 = UndefinedId;
            track->unused02 = false;
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
    ReadShortData(&track->unused34, stream);
    if (track->hasBlendShapes)
    {
        ReadShortData(&track->blendShapes, stream);
        ReadShortData(&track->unused40, stream);
    }
}

void ReadEmitterTrack(CutsceneEmitterTrack* track, Stream* stream)
{
    u32 unread;
    stream->ReadU32(&unread);
    stream->ReadS16(reinterpret_cast<s16*>(&track->unused00));
    stream->ReadBool(&track->unused02);
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

void CutsceneReader::Destroy(u32 destroyFlags)
{
    vtable = g_SectionReaderVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    stream.Destroy(DestroyOnly);
}

void CutsceneReader::Missing(u8*, u32, ReaderStack*)
{
}

void InitCutsceneModule(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_UnreadCutsceneWord = 0;
}

void ConstructCutsceneModule()
{
    InitCutsceneModule(1, DefaultInitPriority);
}
