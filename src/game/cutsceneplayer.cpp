#include "game/cutscenes.h"

#include "game/animation.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/cutscenereader.h"
#include "game/instancefactory.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/readers.h"
#include "game/resources.h"
#include "game/sound.h"
#include "game/string.h"
#include "retail/libc.h"

// The video controller playing cutscenes: starting a read cutscene once its music is prepared, taking over the instances it plays
// (or making them), playing its parts' tracks to the clock while the next part is read, skipping it to its end and letting
// everything go at its end

extern "C"
{
    // The cutscenes' file names: CutScene\_<number>_<part>.cts, the numbers in three digits
    extern const char g_CutsceneFolder[] RETAIL(D_00305AD0);
    extern const char g_CutsceneNumberFormat[] RETAIL(D_0030A270);
    extern const char g_CutsceneSeparator[] RETAIL(D_0030A278);
    extern const char g_CutsceneExtension[] RETAIL(D_0030A280);
}

namespace
{
// The destructor's vtable slot
constexpr u32 DestructorSlot = 1;
// The digits a number of a cutscene's file name is formatted into
constexpr u32 NumberDigits = 16;

// The part after the playing one asked for while the playing one has a full part's frames: read into a new cutscene
void ReadNextPart(VideoController* controller)
{
    if (controller->cutscene->partFrames < Cutscene::PartFrames)
    {
        return;
    }

    controller->bits.nextPartRead = 0;
    Cutscene* next = ConstructCutscene(static_cast<Cutscene*>(MemoryAllocate(sizeof(Cutscene))));
    controller->nextPart = next;
    Cutscene* cutscene = controller->cutscene;
    s32 part = cutscene->part + 1;
    controller->readPart = part;
    LoadCutscenePart(controller, next, cutscene->number, part);
}

// What the stop lets go of that nothing sets: destroyed through its vtable, which it starts with
void DestroyUnused(void* object)
{
    const GccVTableEntry* vtable = *static_cast<const GccVTableEntry* const*>(object);
    CallVirtual<void>(object, vtable, DestructorSlot, DestroyAndFree);
}
}

VideoController* ConstructVideoController(void* memory, void* resourceManager, u32 clockIndex)
{
    auto* controller = static_cast<VideoController*>(memory);
    controller->resourceManager = resourceManager;
    controller->object = nullptr;
    controller->chunk = nullptr;
    controller->bits.value =
        (controller->bits.value & ~VideoControllerBits::ConstructorCleared) | VideoControllerBits::Relative;
    controller->skipHint.string = nullptr;
    controller->skipHint.capacity = 0;
    controller->skipHint.length = 0;
    controller->discErrorTitle.string = nullptr;
    controller->discErrorTitle.capacity = 0;
    controller->discErrorTitle.length = 0;
    controller->cameraTrack.camera = nullptr;
    ConstructCameraTrack(&controller->cameraTrack);
    for (u32 index = 0; index < VideoController::TrackCount; index++)
    {
        ConstructInstanceTrack(&controller->instanceTracks[index]);
    }

    for (u32 index = 0; index < VideoController::TrackCount; index++)
    {
        ConstructEmitterTrack(&controller->emitterTracks[index]);
    }

    for (u32 index = 0; index < VideoController::TrackCount; index++)
    {
        ConstructSoundTrack(&controller->soundTracks[index]);
    }

    controller->cutscene = nullptr;
    controller->unused150C = nullptr;
    controller->nextPart = nullptr;
    controller->unused1514 = nullptr;
    controller->startTime = 0;
    controller->now = 0;
    controller->part = 0;
    controller->frame = 0;
    controller->partFrame = 0;
    controller->frameShare = 0.0f;
    controller->state = VideoController::StateIdle;
    InitIdentityMatrix(&controller->origin);
    controller->clockIndex = static_cast<u8>(clockIndex);
    controller->clock = nullptr;
    controller->waitedFrames = 0;
    controller->modelCount = 0;
    return controller;
}

u32 StartReadCutscene(VideoController* controller, TimeClock* clock)
{
    if (controller->state != VideoController::StateReady)
    {
        if (controller->state == VideoController::StateQueued)
        {
            controller->bits.waitingForMusic = 1;
        }

        controller->waitedFrames++;
        return 0;
    }

    if (MusicSlotPrepared(ContextMusicSlot) == 0)
    {
        controller->bits.waitingForMusic = 1;
        controller->waitedFrames++;
        return 0;
    }

    controller->clock = clock;
    controller->bits.waitingForMusic = 0;
    controller->startTime = clock->time;
    controller->partFrame = 0;
    controller->frame = 0;
    controller->frameShare = 0.0f;
    controller->part = 0;
    controller->now = clock->time;
    TakeCutsceneInstances(controller);
    PlayPreparedMusic(1.0f, 1.0f, 0.0f, ContextMusicSlot);
    controller->state = VideoController::StatePlaying;
    ReadNextPart(controller);
    u32 seen = CutsceneSeen(controller);
    controller->waitedFrames = 0;
    controller->bits.skippable = seen & 1;
    return 1;
}

void LoadCutscenePart(VideoController* controller, Cutscene* cutscene, s32 number, s32 part)
{
    String path;
    path.string = nullptr;
    path.length = 0;
    path.capacity = 0;
    char digits[NumberDigits];
    RetailLibc::Format(digits, g_CutsceneNumberFormat, number);
    StringAssign(&path, g_CutsceneFolder);
    StringAppend(&path, g_CutsceneSeparator);
    StringAppend(&path, digits);
    StringAppend(&path, g_CutsceneSeparator);
    RetailLibc::Format(digits, g_CutsceneNumberFormat, part);
    StringAppend(&path, digits);
    StringAppend(&path, g_CutsceneExtension);
    GameReadersStorage* storage = g_ReadersStorages[MainReaders];
    auto* reader = static_cast<CutsceneReader*>(MemoryAllocate(sizeof(CutsceneReader)));
    reader->controller = controller;
    reader->vtable = g_CutsceneReaderVTable;
    reader->number = number;
    reader->part = part;
    reader->cutscene = cutscene;
    SubItemsReader* file = SubItemsReader::ConstructFile(static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))),
                                                         path.string, reader,
                                                         SubItemsReaderOptions::ClosesFile | SubItemsReaderOptions::OnDisk);
    AddItemReaderToReaderStorage(storage, file, QueueBack);
    StringDestroy(&path);
}

void StopCutscene(VideoController* controller)
{
    if (controller->cutscene != nullptr)
    {
        for (u32 index = 0; index < controller->cutscene->emitterTrackCount; index++)
        {
            ReleaseEmitterTrack(&controller->emitterTracks[index]);
        }

        for (u32 index = 0; index < controller->cutscene->soundTrackCount; index++)
        {
            ReleaseSoundTrack(&controller->soundTracks[index]);
        }

        for (u32 index = 0; index < controller->cutscene->instanceTrackCount; index++)
        {
            ReleaseInstanceTrack(&controller->instanceTracks[index]);
        }
    }

    PlayedCameraTrack* camera = &controller->cameraTrack;
    if (camera->values != nullptr)
    {
        MemoryDeallocate2_(camera->values);
    }

    if (camera->cuts != nullptr)
    {
        MemoryDeallocate2_(camera->cuts);
    }

    camera->cutting = 0;
    camera->track = nullptr;
    camera->values = nullptr;
    camera->cuts = nullptr;
    camera->shot = 0;
    if (controller->unused1514 != nullptr)
    {
        DestroyUnused(controller->unused1514);
    }

    if (controller->cutscene != nullptr)
    {
        DestroyCutscene(controller->cutscene, DestroyAndFree);
        controller->cutscene = nullptr;
    }

    if (controller->nextPart != nullptr)
    {
        DestroyCutscene(controller->nextPart, DestroyAndFree);
        controller->nextPart = nullptr;
    }

    if (controller->unused150C != nullptr)
    {
        DestroyCutscene(controller->unused150C, DestroyAndFree);
        controller->unused150C = nullptr;
    }

    StopCutsceneMusic(controller);
    controller->waitedFrames = 0;
    controller->clock = nullptr;
}

void TakeCutsceneInstances(VideoController* controller)
{
    if (controller->cutscene == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < controller->cutscene->instanceTrackCount; index++)
    {
        u16 model = controller->cutscene->instanceTracks[index].model;
        if (model == NoModelId)
        {
            continue;
        }

        PlayedInstanceTrack* track = &controller->instanceTracks[index];
        InstanceContext* instance = ModelInstance(controller, model);
        if (instance != nullptr)
        {
            SetTrackInstance(track, instance, 1);
            TakeOverInstance(controller, instance);
        }
        else
        {
            instance = MakeModelInstance(controller->chunk, model, nullptr);
            instance->clockIndex = controller->clockIndex;
            SetTrackInstance(track, instance, 0);
        }

        PlayInstanceTrackAnimation(track, &controller->cutscene->instanceTracks[index]);
    }
}

void TakeOverInstance(VideoController* controller, InstanceContext* instance)
{
    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
    GameObject* object = node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
    Agent* agent = node->agent;
    CallVirtual<void>(agent, agent->vtable, Agent::FreezeSlot);
    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator != nullptr)
    {
        StopOgiAnimation(animator, 0, OgiAnimator::RootJoint);
    }

    u16 id = NoModelId;
    for (u32 index = 0; index < controller->modelCount; index++)
    {
        if (controller->modelInstances[index] == instance)
        {
            id = controller->models[index];
            break;
        }
    }

    ResourceTable* models = G_GameResourcesObjectPointer->models;
    auto* ogi =
        id != NoModelId ? static_cast<GameOGI*>(models->items[id & ResourceIndexMask]) : nullptr;
    model->SetOgi(ogi, object->header.reactJoints, object->header.exitPoints);
}

void UpdatePlayingCutscene(VideoController* controller)
{
    TimeClock* clock = controller->clock;
    if (clock != nullptr && clock->flags.running == 0)
    {
        controller->state = VideoController::StatePaused;
        return;
    }

    if (controller->bits.musicOnly != 0)
    {
        if (MusicSlotPlaying(ContextMusicSlot) != 0)
        {
            return;
        }

        StopCutscene(controller);
        controller->state = VideoController::StateFinished;
        controller->bits.musicOnly = 0;
        return;
    }

    if (controller->cutscene == nullptr)
    {
        return;
    }

    if (controller->waitedFrames >= VideoController::DiscErrorFrames)
    {
        DrawDiscError(controller);
    }
    else if (controller->bits.skippable != 0)
    {
        DrawSkipHint(controller);
    }

    if (AdvanceCutscene(controller, controller->clock) != 0)
    {
        PlayInstanceTracks(controller);
        PlayEmitterTracks(controller);
        PlaySoundTracks(controller);
        PlayCameraTrack(controller);
        return;
    }

    // Its end: marked seen in its instance's persistent flag
    GiveBackInstances(controller);
    Agent* agent = static_cast<ObjectNodeBase*>(GetGameNode(&controller->instance->nodes, NodeObject))->agent;
    ChunkEntry* chunk = ChunkOfIndex(g_ChunkManager, agent->chunkIndex);
    SetPersistentFlag(chunk->savedFlags, agent->id, 1);
    StopCutscene(controller);
    controller->state = VideoController::StateFinished;
}

void UpdateQueuedCutscene(VideoController* controller)
{
    if (controller->bits.waitingForMusic != 0)
    {
        if (controller->bits.musicOnly == 0)
        {
            StartReadCutscene(controller, controller->clock);
        }
        else
        {
            StartQueuedMovie(controller, controller->clock);
        }
    }

    if (controller->bits.musicOnly == 0 && controller->waitedFrames >= VideoController::DiscErrorFrames)
    {
        DrawDiscError(controller);
    }
}

u32 AdvanceCutscene(VideoController* controller, TimeClock* clock)
{
    u32 now = clock->time;
    controller->now = now;
    f32 frames = static_cast<f32>(static_cast<s32>(now - controller->startTime)) * g_SecondsPerClockUnit *
                 static_cast<f32>(Cutscene::FramesPerSecond);
    u16 frame = static_cast<u16>(static_cast<s32>(frames));
    controller->frame = frame;
    controller->frameShare = frames - static_cast<f32>(controller->frame);
    controller->part = static_cast<u16>(frame / Cutscene::PartFrames);
    controller->partFrame = static_cast<u16>(frame % Cutscene::PartFrames);
    Cutscene* cutscene = controller->cutscene;
    u16 cutsceneFrames = cutscene->frames;
    if (controller->part == cutscene->part + 1)
    {
        NextCutscenePart(controller);
    }

    return controller->frame < cutsceneFrames;
}

void NextCutscenePart(VideoController* controller)
{
    if (controller->bits.nextPartRead == 0)
    {
        controller->waitedFrames++;
        return;
    }

    if (controller->cutscene != nullptr)
    {
        DestroyCutscene(controller->cutscene, DestroyAndFree);
    }

    Cutscene* cutscene = controller->nextPart;
    controller->cutscene = cutscene;
    controller->nextPart = nullptr;
    controller->currentPart = cutscene->part;
    controller->partFrame = 0;
    for (u32 index = 0; index < controller->cutscene->instanceTrackCount; index++)
    {
        PlayInstanceTrackAnimation(&controller->instanceTracks[index], &controller->cutscene->instanceTracks[index]);
    }

    ReadNextPart(controller);
}

void PlayInstanceTrackAnimation(PlayedInstanceTrack* played, const CutsceneInstanceTrack* track)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(&played->instance->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    StopOgiAnimation(animator, 0, OgiAnimator::RootJoint);
    if (played->animation != nullptr)
    {
        DestroyAnimation(played->animation, DestroyAndFree);
    }

    GameAnimation* animation = MakeAnimation(static_cast<GameAnimation*>(MemoryAllocate(sizeof(GameAnimation))), &track->joints,
                                             track->hasBlendShapes != 0 ? &track->blendShapes : nullptr);
    played->animation = animation;
    animation->bits.rate = Cutscene::FramesPerSecond;
    played->animation->bits.frames = Cutscene::PartFrames;
    AnimationSettings settings;
    ConstructAnimationSettings(1.0f, &settings, played->animation);
    settings.bits.loops = 0;
    PlayOgiAnimation(animator, &settings, OgiAnimator::RootJoint);
    DestroyAnimationSettings(&settings, DestroyOnly);
}

void VideoController::Skip()
{
    if (bits.skippable == 0 || cutscene == nullptr || state != StatePlaying)
    {
        return;
    }

    const Matrix4x4* place = bits.relative != 0 ? &origin : nullptr;
    for (u32 index = 0; index < cutscene->instanceTrackCount; index++)
    {
        PlayInstanceTrackFrame(0.0f, &instanceTracks[index], &cutscene->instanceTracks[index], Cutscene::EndFrame, place);
    }

    for (u32 index = 0; index < cutscene->emitterTrackCount; index++)
    {
        PlayEmitterTrackFrame(0.0f, &emitterTracks[index], &cutscene->emitterTracks[index], Cutscene::EndFrame, place);
    }

    for (u32 index = 0; index < cutscene->soundTrackCount; index++)
    {
        PlaySoundTrackFrame(0.0f, &soundTracks[index], &cutscene->soundTracks[index], Cutscene::EndFrame, place);
    }

    PlayCameraTrackFrame(0.0f, &cameraTrack, &cutscene->camera, Cutscene::EndFrame, place);
    GiveBackInstances(this);
    StopCutscene(this);
    state = StateFinished;
}
