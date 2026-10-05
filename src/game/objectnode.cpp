#include "game/objectnode.h"

#include "game/animation.h"
#include "game/behaviours.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/context.h"
#include "game/controllers.h"
#include "game/events.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objects.h"
#include "game/player.h"
#include "game/resources.h"
#include "game/rigidbody.h"
#include "game/sound.h"
#include "game/string.h"
#include "game/place.h"
#include "game/reference.h"

#include <cstddef>
#include <cstdint>

namespace
{

// Whether the trajectory controller asks for its frame while the node isn't updated
bool TrajectoryKeepsStepping(const Trajectory* trajectory)
{
    return trajectory->followed->flags.keepsStepping;
}

// The instance's seen stamp past the node's own (none: 0), past the grace: 0 within it
u32 StampsUnseen(const ObjectNode* node)
{
    const InstanceContext* owner = node->owner;
    u32 seen = owner->seen;
    u32 since = node->nearDistance;
    if (since == GameNode::AnyNearDistance || !(since < seen))
    {
        return 0;
    }

    u32 gap = seen - since;
    u32 grace = g_ObjectUpdateRate.grace;
    return grace < gap ? gap - grace : 0;
}
}

u32 ObjectNode::Update(TimeClock* clock)
{
    InstanceContext* instance = owner;
    u32 running = clock->flags.running;
    if (instance->chunk == nullptr)
    {
        return 0;
    }

    if (running != 0)
    {
        u32 unseen = StampsUnseen(this);
        bool always = flags.handledEvent || flags.riding;
        if (always || unseen < g_ObjectUpdateRate.cutoff)
        {
            bool now = always || unseen == 0;
            if (!now)
            {
                // Every 2^n frames, n growing with the stamps it went unseen (every 65536 frames without a count), staggered by
                // the node's address
                u32 mask = UpdateRate::RarestMask;
                if (unseen != UpdateRate::NoCount)
                {
                    s32 power = static_cast<s32>(g_ObjectUpdateRate.slope * static_cast<f32>(unseen)) + 1;
                    mask = (1u << (power & ShiftMask)) - 1;
                }

                now = ((g_RenderedFrames + (reinterpret_cast<u32>(this) >> 8)) & mask) == 0;
            }

            if (!now)
            {
                GameNode::flags.keepsTime = 1;
            }
            else
            {
                ObjectPlace* place = owner->place;
                place->SyncPosition();
                frameStart = place->position;
                flags.updated = 1;
                flags.moves = 0;
                flags.unused4 = 0;
                if (controller != nullptr)
                {
                    CallVirtual<void>(controller, controller->vtable, NodeController::FrameSlot, clock);
                }

                if (particleTrails != nullptr)
                {
                    StepParticleTrails(particleTrails, clock, this);
                }

                if (trajectory != nullptr)
                {
                    StepTrajectory(trajectory, clock, this);
                }

                if (headTracking != nullptr)
                {
                    StepHeadTracking(headTracking, clock, this);
                }

                if (perception != nullptr)
                {
                    StepPerception(perception, clock, this);
                }

                // The runners, the second first: whether one still runs a behaviour
                bool runs = false;
                g_CurrentRunner = runners[1];
                if (g_CurrentRunner != nullptr)
                {
                    if (g_CurrentRunner->Update(clock) != 0)
                    {
                        CallVirtual<void>(this, vtable, RunnerFinishedSlot);
                    }
                    else
                    {
                        runs = g_CurrentRunner->receivers != nullptr;
                    }
                }

                g_CurrentRunner = runners[0];
                if (g_CurrentRunner != nullptr)
                {
                    if (g_CurrentRunner->Update(clock) != 0)
                    {
                        CallVirtual<void>(this, vtable, RunnerFinishedSlot);
                    }
                    else if (g_CurrentRunner->receivers != nullptr)
                    {
                        runs = true;
                    }
                }

                if (!runs && trajectory != nullptr)
                {
                    TrajectoryFrame(trajectory, this);
                }

                StepMovement(this, clock);
                if (flags.pinned)
                {
                    // The frame's move kept, and the instance put back unless it moves by itself
                    place = owner->place;
                    place->SyncPosition();
                    frameMove = place->position;
                    frameMove.x = frameMove.x - frameStart.x;
                    frameMove.y = frameMove.y - frameStart.y;
                    frameMove.z = frameMove.z - frameStart.z;
                    if (!flags.moves)
                    {
                        InstanceContext* pinned = owner;
                        place = pinned->place;
                        place->SyncPosition();
                        if (place->MoveTo(&frameStart))
                        {
                            QueueObject(pinned);
                        }
                    }
                }

                flags.handledEvent = 0;
                return GameNode::Update(clock);
            }
        }

        flags.updated = 0;
        if (trajectory != nullptr && TrajectoryKeepsStepping(trajectory))
        {
            StepTrajectory(trajectory, clock, this);
        }

        if (rigidBody != nullptr && rigidBody->physicsBody != nullptr)
        {
            StepMovement(this, clock);
        }
    }

    flags.handledEvent = 0;
    return GameNode::Update(clock);
}

void StepMovement(ObjectNode* node, TimeClock* clock)
{
    ObjectRigidBody* body = node->rigidBody;
    if (body != nullptr)
    {
        // An attached instance's body is held still
        if ((body->bits.dragged || body->bits.falls || body->bits.moving) && !node->owner->flags.attached)
        {
            StepRigidBody(body, clock, nullptr);
        }

        // The middle of the instance's collision box
        const Box* box = node->owner->CollisionBox();
        Vector4& middle = node->middle;
        middle = box->max;
        middle.x = middle.x - box->min.x;
        middle.y = middle.y - box->min.y;
        middle.z = middle.z - box->min.z;
        middle.x = middle.x * 0.5f;
        middle.y = middle.y * 0.5f;
        middle.z = middle.z * 0.5f;
        middle.x = middle.x + box->min.x;
        middle.y = middle.y + box->min.y;
        middle.z = middle.z + box->min.z;
        // The frame's contacts forgotten (whether it touched an instance or the world kept), not on the ground nor against a wall
        body = node->rigidBody;
        ObjectRigidBodyBits bits = body->bits;
        body->bits.touchingInstance = 0;
        body->bits.touchingWorld = 0;
        body->bits.touching = 0;
        body->state.touched = bits.touchingInstance || bits.touchingWorld;
        body->state.onGround = 0;
        body->state.againstWall = 0;
        if (node->rigidBody->bits.releasedAtRest)
        {
            ReleaseRigidBodyAtRest(node);
        }
    }

    if (!node->flags.pinned && node->reactions.knockCountdown != 0)
    {
        node->reactions.knockCountdown--;
    }
}

InstancePlacement* InstancePlacement::Construct(InstancePlacement* information, ChunkData* chunk)
{
    information->chunk.string = nullptr;
    information->chunk.length = 0;
    information->chunk.capacity = 0;
    information->flags.value = 0;
    if (chunk != nullptr)
    {
        StringAssign(&information->chunk, chunk->path.string);
    }

    return information;
}

ObjectNodeBase* ObjectNodeBase::Construct(ObjectNodeBase* node, ChunkEntry* chunk, u32)
{
    GameNode::Construct(node);
    node->vtable = g_NodePrototypeVTable;
    ChunkDataReference* data = chunk->data;
    InstancePlacement::Construct(&node->information, data != nullptr ? data->chunk : nullptr);
    node->ownInformation = nullptr;
    node->informationPointer = &node->information;
    node->ownObjectId = NoObjectId;
    node->Reset();
    node->tracked = nullptr;
    node->vtable = g_ObjectNodeBaseVTable;
    for (BehaviourRunner*& runner : node->runners)
    {
        runner = nullptr;
    }

    return node;
}

void ObjectNodeBase::Destroy(u32 destroyFlags)
{
    vtable = g_ObjectNodeBaseVTable;
    DestroyRunners();
    InstancePlacement* own = ownInformation;
    tracked = nullptr;
    vtable = g_NodePrototypeVTable;
    if (own != nullptr)
    {
        StringDestroy(&own->chunk);
        MemoryDeallocate2_(own);
    }

    StringDestroy(&information.chunk);
    GameNode::Destroy(destroyFlags);
}

void ObjectNodeBase::DestroyPrototype(u32 destroyFlags)
{
    vtable = g_NodePrototypeVTable;
    InstancePlacement* own = ownInformation;
    if (own != nullptr)
    {
        StringDestroy(&own->chunk);
        MemoryDeallocate2_(own);
    }

    StringDestroy(&information.chunk);
    GameNode::Destroy(destroyFlags);
}

void ObjectNodeBase::HandleEvent(Reference** handle)
{
    // The events below 0x100 aren't for it, 0x100 and 0x101 apply themselves (their vtable's slot 2), 0x103 is a trigger message
    constexpr u16 FirstNodeEvent = 0x100;
    constexpr u16 LastAppliedEvent = 0x101;
    constexpr u16 TriggerMessage = 0x103;
    flags.handledEvent = 1;
    auto* event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
    u16 kind = event->id;
    if (kind >= FirstNodeEvent)
    {
        if (kind <= LastAppliedEvent)
        {
            auto* applied = reinterpret_cast<GameEvent*>((*handle)->object);
            CallVirtual<void>(applied, applied->vtable, GameEvent::ApplySlot, this, G_GameResourcesObjectPointer);
        }
        else if (kind == TriggerMessage)
        {
            event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
            Reference* sender = event->argument;
            messageSender = sender != nullptr ? static_cast<InstanceContext*>(sender->object) : nullptr;
            event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
            message = event->message;
            messageTime = GetContextClock(owner)->time;
            if (object->header.triggerBehaviourCount != 0)
            {
                CallVirtual<void>(this, vtable, TriggerMessageSlot, static_cast<u32>(message));
            }
        }
    }

    ReleaseEvent(handle);
}

u32 ObjectNodeBase::StartBehaviour(ScriptStarter* starter, InstanceContext* originator, u32 force, u32 slot)
{
    BehaviourRunner*& runner = runners[static_cast<u8>(slot)];
    if (runner != nullptr)
    {
        return runner->QueueStarter(starter, originator, force);
    }

    auto* made = BehaviourRunner::Construct(static_cast<BehaviourRunner*>(MemoryAllocate(sizeof(BehaviourRunner))), this,
                                            static_cast<u8>(slot));
    runner = made;
    made->nextOriginator = originator;
    made->nextStarter = starter;
    return 1;
}

void ObjectNodeBase::OnTriggerMessage(u32 received)
{
    u32 count = object->header.triggerBehaviourCount;
    u32 wanted = received & 0xFFFF;
    for (u32 index = 0; index < count;)
    {
        const TriggerBehaviour* behaviour = &object->triggerBehaviours.items[index];
        index++;
        if (behaviour->message != wanted)
        {
            continue;
        }

        u16 id = behaviour->starter;
        ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
        auto* starter = id != NoScriptId ? static_cast<ScriptStarter*>(scripts->items[id & ResourceIndexMask]) : nullptr;
        u32 runner = behaviour->runner;
        CallVirtual<u32>(this, vtable, StartBehaviourSlot, starter, messageSender, 0u, runner);
        return;
    }
}

void ObjectNodeBase::StopRunners(u32 release)
{
    for (BehaviourRunner*& runner : runners)
    {
        if (runner != nullptr)
        {
            runner->Stop(release);
            runner->Destroy(DestroyAndFree);
            runner = nullptr;
        }
    }
}

u32 ObjectNodeBase::Kind()
{
    return NodeObject;
}

void ObjectNodeBase::Removed()
{
}

void ObjectNodeBase::Sleep()
{
}

void ObjectNodeBase::RunnerFinished()
{
}

u32 ObjectNodeBase::AddParticleTrail()
{
    constexpr u32 NoTrail = 0xFF;
    return NoTrail;
}

void ObjectNodeBase::DestroyParticleTrails()
{
}

void ObjectNodeBase::Launch()
{
}

void ObjectNodeBase::Push()
{
}

u32 ObjectNodeBase::Collided()
{
    return 1;
}

u32 ObjectNodeBase::Landed()
{
    return 0;
}

u32 ObjectNodeBase::LandedHard()
{
    return 0;
}

u32 ObjectNodeBase::Scraped()
{
    return 0;
}

void ObjectNodeBase::UnusedDoNothing()
{
}

void ObjectNodeBase::ForgetDesignator()
{
}

void ObjectNodeBase::ReleaseParts()
{
}

u32 ObjectNodeBase::GetDesignator()
{
    return 0;
}

u32 ObjectNodeBase::GetDesignatorPosition()
{
    return 0;
}

u32 ObjectNodeBase::SetDesignatorPosition()
{
    return 0;
}

u32 ObjectNodeBase::SetDesignator()
{
    return 0;
}

u32 ObjectNodeBase::HasNoTrackedSound()
{
    return 0;
}

u32 ObjectNodeBase::CodeModelKind()
{
    return 0;
}

u32 ObjectNodeBase::PrototypeStartBehaviour()
{
    return 0;
}

void ObjectNodeBase::PrototypeOnTriggerMessage()
{
}

void ObjectNodeBase::PrototypeStopRunners()
{
}

void ObjectNodeBase::Reset()
{
    flags.value = 0;
    CallVirtual<void>(this, vtable, SetAgentSlot, static_cast<Agent*>(nullptr));
    ClearMessages();
    ownObjectId = NoObjectId;
    rank = NoRank;
    sourceNode = nullptr;
}

void ObjectNodeBase::SetAgent(Agent* made)
{
    if (made == nullptr)
    {
        agent = nullptr;
        object = nullptr;
        properties = nullptr;
        return;
    }

    object = made->object;
    agent = made;
    properties = made->properties;
}

ObjectRigidBody* ObjectNode::RigidBody()
{
    if (rigidBody == nullptr)
    {
        rigidBody = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), this);
    }

    return rigidBody;
}

void ObjectNodeBase::ClearMessages()
{
    messageTime = 0;
    message = NoMessage;
    messageSender = nullptr;
}

EABI_EXPORT(SecondsSinceUserMessage, static_cast<u32 (ObjectNodeBase::*)(const u32*, f32)>(&ObjectNodeBase::MessageWithin));
EABI_EXPORT(UserMessageWithinSeconds,
            static_cast<u32 (ObjectNodeBase::*)(u32, const u32*, f32)>(&ObjectNodeBase::MessageWithin));

u32 ObjectNodeBase::MessageWithin(const u32* time, f32 seconds)
{
    return static_cast<f32>(static_cast<s32>(*time - messageTime)) * g_SecondsPerClockUnit <= seconds;
}

u32 ObjectNodeBase::MessageWithin(u32 message, const u32* time, f32 seconds)
{
    if (static_cast<u16>(message) != this->message)
    {
        return 0;
    }

    return MessageWithin(time, seconds);
}

void ObjectNodeBase::DestroyRunners()
{
    for (BehaviourRunner*& runner : runners)
    {
        if (runner != nullptr)
        {
            runner->Destroy(DestroyAndFree);
        }

        runner = nullptr;
    }
}

ObjectNode* ObjectNode::Construct(ObjectNode* node, ChunkEntry* chunk, u32 waypoints, u32 unused)
{
    ObjectNodeBase::Construct(node, chunk, unused);
    node->vtable = g_ObjectNodeVTable;
    node->Initialise(waypoints);
    return node;
}

void ObjectNode::Initialise(u32 wanted)
{
    storedPlace = nullptr;
    motion = MotionState::Construct(static_cast<MotionState*>(MemoryAllocate(sizeof(MotionState))));
    waypoints = wanted != 0 ? Waypoints::Construct(static_cast<Waypoints*>(MemoryAllocate(sizeof(Waypoints)))) : nullptr;
    rollRadius = 2.0f;
    waterSurface = NoSurface;
    rigidBody = nullptr;
    translator = nullptr;
    rotator = nullptr;
    physics = nullptr;
    controller = nullptr;
    particleTrails = nullptr;
    trajectory = nullptr;
    motionBlock = nullptr;
    headTracking = nullptr;
    perception = nullptr;
    surface = NoSurface;
    flags.value = 0;
    agentRef1 = nullptr;
    agentRef2 = nullptr;
    reactions.knockCountdown = 0;
    f32 w = frameMove.w;
    frameMove = g_DefaultBox.min;
    frameMove.w = w;
    ForgetStoredPosition();
    reactions.passesNoises = 0;
    reactions.noiseMessage = NoMessage;
    reactions.noContactSounds = 0;
    unused164 = -1;
    contactSoundValue = -1.0f;
    playingSound = NoInstanceSound;
    reactions.splashTime = 0;
    lastContactTime = 0;
    contactSoundFirst = NoContactSoundSlot;
    contactSoundLast = NoContactSoundSlot;
    countedValue = 0.0f;
    trackedSound = NoInstanceSound;
}

void ObjectNode::Destroy(u32 destroyFlags)
{
    vtable = g_ObjectNodeVTable;
    if (motion != nullptr)
    {
        motion->Destroy(DestroyAndFree);
    }

    if (waypoints != nullptr)
    {
        waypoints->Destroy(DestroyAndFree);
    }

    MemoryDeallocate2_(storedPlace);
    ReleasePartsUnlessUnloading();
    DestroyController();
    ObjectNodeBase::Destroy(destroyFlags);
}

void ObjectNode::ForgetStoredPosition()
{
    flags.storedPosition = 0;
}

void ObjectNode::ReleasePartsUnlessUnloading()
{
    if (g_UnloadingEverything == 0)
    {
        CallVirtual<void>(this, vtable, ReleasePartsSlot, this);
    }
}

void ObjectNode::DestroyController()
{
    if (controller == nullptr)
    {
        return;
    }

    CallVirtual<void>(controller, controller->vtable, NodeController::StopSlot);
    if (controller != nullptr)
    {
        CallVirtual<void>(controller, controller->vtable, NodeController::DestroySlot, DestroyAndFree);
    }

    controller = nullptr;
}

void ObjectNode::SetOwner(InstanceContext* instance)
{
    information.Take(instance, 1);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    frameStart = place->position;
    GameNode::SetOwner(instance);
    middle = frameStart;
}

void ObjectNode::LeftChunk(u32 why)
{
    if (why < 2)
    {
        ClearComebackPlacement(this);
    }
}

void ObjectNode::Removed()
{
    CallVirtual<void>(this, vtable, ReleasePartsUnlessUnloadingSlot);
}

u32 ObjectNode::ItemType()
{
    return ClassId;
}

u32 ObjectNode::UnusedTakesPackets()
{
    return 1;
}

u32 ObjectNode::TakesPackets()
{
    return 1;
}

u32 ObjectNode::UnpinCollision()
{
    return 0;
}

u32 ObjectNode::PinCollision()
{
    return 0;
}

void ObjectNode::DoNothing()
{
}

u32 ObjectNode::CustomSlot()
{
    constexpr u32 NoCustomSlot = 0xFF;
    return NoCustomSlot;
}

void ObjectNode::UnusedDoNothing()
{
}

u32 ObjectNode::HasNoTrackedSound()
{
    return trackedSound == NoInstanceSound;
}

u32 ObjectNode::CodeModelKind()
{
    return 1;
}

void ObjectNode::PacketStarted()
{
}

void ObjectNode::StopSound()
{
    if (playingSound != NoInstanceSound)
    {
        StopInstanceSound(playingSound);
        playingSound = NoInstanceSound;
    }
}

void ObjectNode::Reset()
{
    ResetRunners();
    if (motion != nullptr)
    {
        motion->Reset();
    }

    if (waypoints != nullptr)
    {
        waypoints->ClearRoute();
    }

    CallVirtual<void>(this, vtable, PacketEndedSlot, static_cast<BehaviourRunner*>(nullptr));
    f32 w = frameMove.w;
    frameMove = g_DefaultBox.min;
    frameMove.w = w;
    sourceNode = nullptr;
    DestroyController();
    ReleaseTrajectory();
    CallVirtual<void>(this, vtable, DestroyParticleTrailsSlot);
    ReleaseRigidBody();
    ReleaseAttachments();
    agentRef1 = nullptr;
    ReleasePerception();
    if (!flags.keepsAgentRef2)
    {
        agentRef2 = nullptr;
    }

    ForgetStoredPosition();
    ReleaseStoredPlace();
    ReleaseHeadTracking();
    ReleaseMotionBlock();
    ClearMessages();
    owner->flags.busy = 0;
    playingSound = NoInstanceSound;
    unused164 = -1;
    countedValue = 0.0f;
    trackedSound = NoInstanceSound;
}

void ObjectNode::PacketEnded(BehaviourRunner* runner)
{
    if (runner != nullptr)
    {
        u32 slot = runner->flags.slot;
        BehaviourRunner* other = runners[static_cast<u8>(1 - slot)];
        if (other != nullptr && other->receivers != nullptr && other->packet != nullptr)
        {
            return;
        }
    }

    if (translator != nullptr)
    {
        translator->Destroy(DestroyAndFree);
        translator = nullptr;
    }

    if (rotator != nullptr)
    {
        rotator->Destroy(DestroyAndFree);
        rotator = nullptr;
    }

    if (physics != nullptr)
    {
        physics->Destroy(DestroyAndFree);
        physics = nullptr;
    }
}

void ObjectNode::ResetRunners()
{
    DestroyRunners();
    flags.value &= ~ObjectNodeFlags::FocusMask;
    tracked = nullptr;
}

void ObjectNode::ReleaseTrajectory()
{
    if (trajectory != nullptr)
    {
        LetGoOfTrajectory(trajectory, this);
        if (trajectory != nullptr)
        {
            DestroyTrajectory(trajectory, DestroyAndFree);
        }
    }

    trajectory = nullptr;
}

void ObjectNode::ReleaseRigidBody()
{
    ObjectRigidBody* body = rigidBody;
    if (body == nullptr)
    {
        return;
    }

    ChunkRigidBodies* bodies = body->chunkBodies;
    if (bodies != nullptr)
    {
        TakeFirstRigidBody(bodies, body);
        TakeSecondRigidBody(bodies, rigidBody);
    }

    if (rigidBody != nullptr)
    {
        DestroyRigidBody(rigidBody, DestroyAndFree);
    }

    rigidBody = nullptr;
}

void ObjectNode::ReleaseAttachments()
{
    void* attachments = GetGameNode(&owner->nodes, NodeAttachments);
    if (attachments != nullptr)
    {
        ReleaseAttachmentsNode(attachments, 0, 0, 0);
    }

    owner->flags.attached = 0;
    owner->flags.hasAttachment = 0;
    owner->parent = nullptr;
}

void ObjectNode::ReleasePerception()
{
    if (perception != nullptr)
    {
        MemoryDeallocate2_(perception);
        perception = nullptr;
    }
}

void ObjectNode::ReleaseStoredPlace()
{
    if (storedPlace != nullptr)
    {
        MemoryDeallocate2_(storedPlace);
    }

    storedPlace = nullptr;
}

void ObjectNode::ReleaseHeadTracking()
{
    if (headTracking != nullptr)
    {
        LetGoOfHeadTracking(headTracking, this);
        if (headTracking != nullptr)
        {
            DestroyHeadTracking(headTracking, DestroyAndFree);
        }
    }

    headTracking = nullptr;
}

void ObjectNode::ReleaseMotionBlock()
{
    if (motionBlock != nullptr)
    {
        if (trajectory != nullptr)
        {
            LetGoOfTrajectory(trajectory, this);
            if (trajectory != nullptr)
            {
                DestroyTrajectory(trajectory, DestroyAndFree);
            }
        }

        trajectory = nullptr;
        motionBlock = nullptr;
        flags.movesStoredPlace = 0;
    }

    owner->flags.physicsBody = 0;
    CallVirtual<void>(this, vtable, StopSoundSlot);
}

void ObjectNode::RunnerFinished()
{
    constexpr u32 RunnerTrails = 1;
    if (!flags.keepsParticles)
    {
        CallVirtual<void>(this, vtable, DestroyParticleTrailsSlot);
    }
    else if (particleTrails != nullptr)
    {
        particleTrails->RemoveKind(RunnerTrails);
    }

    if (!flags.keepsTrajectory)
    {
        ReleaseTrajectory();
    }

    if (!flags.keepsPerception)
    {
        ReleasePerception();
    }
}

void ObjectNode::ForgetDesignator(u32 designator)
{
    if (designator == SlotAgentRef1)
    {
        agentRef1 = nullptr;
    }
    else if (designator == SlotFocus)
    {
        flags.value &= ~ObjectNodeFlags::FocusMask;
    }
    else if (designator == SlotAgentRef2)
    {
        agentRef2 = nullptr;
    }
    else if (designator == SlotStoredPosition)
    {
        ForgetStoredPosition();
    }
}

void ObjectNode::ReleaseParts()
{
    CallVirtual<void>(this, vtable, DestroyParticleTrailsSlot);
    ReleaseTrajectory();
    ReleaseRigidBody();
    ReleaseAttachments();
    ReleaseHeadTracking();
    ReleasePerception();
    CallVirtual<void>(this, vtable, StopSoundSlot);
    CallVirtual<void>(this, vtable, PacketEndedSlot, static_cast<BehaviourRunner*>(nullptr));
}

u32 ObjectNode::SetDesignator(u32 designator, InstanceContext* instance)
{
    switch (designator)
    {
    case DesignatesHeadTarget:
        if (headTracking == nullptr)
        {
            return 0;
        }

        TrackHead(1.0f, headTracking, instance, this, 0);
        return 1;
    case DesignatesAgentRef2:
        agentRef2 = instance;
        return 1;
    case DesignatesAgentRef1:
        agentRef1 = instance;
        return 1;
    case DesignatesFocus:
        focusInstance = instance;
        if (instance != nullptr)
        {
            flags.focusInstance = 1;
        }

        flags.focusPosition = 0;
        return 1;
    default:
        return 0;
    }
}

u32 ObjectNode::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    if (rigidBody != nullptr)
    {
        ChunkRigidBodies* entered = ChunkRigidBodiesOf(link->linkedData);
        ChunkRigidBodies* left = rigidBody->chunkBodies;
        u32 inFirst = TakeFirstRigidBody(left, rigidBody);
        u32 inSecond = TakeSecondRigidBody(left, rigidBody);
        if (inFirst != 0)
        {
            PutFirstRigidBody(entered, rigidBody);
        }

        if (inSecond != 0)
        {
            PutSecondRigidBody(entered, rigidBody);
        }
    }

    if (particleTrails != nullptr)
    {
        particleTrails->ChangeChunk(from, link);
    }

    return 1;
}

void ObjectNode::Restart(TimeClock* clock, u32 word)
{
    if (controller != nullptr)
    {
        CallVirtual<void>(controller, controller->vtable, NodeController::RestartSlot, word);
    }

    CallVirtual<void>(this, vtable, ReleasePartsSlot);
    CallVirtual<void>(this, vtable, DoNothingSlot);
    CallVirtual<void>(this, vtable, StopRunnersSlot, 1u);
    CallVirtual<void>(this, vtable, ResetSlot, owner);
    informationPointer->Apply(owner);
    const Box* box = owner->CollisionBox();
    middle = box->max;
    middle.x -= box->min.x;
    middle.y -= box->min.y;
    middle.z -= box->min.z;
    middle.x *= 0.5f;
    middle.y *= 0.5f;
    middle.z *= 0.5f;
    middle.x += box->min.x;
    middle.y += box->min.y;
    middle.z += box->min.z;
    ObjectPlace* place = owner->place;
    place->SyncPosition();
    frameStart = place->position;
    time = clock->time;
}

void ObjectNode::Collided(void* other, const Vector4* point, const Vector4* impulse)
{
    constexpr f32 HardKnock = 10.0f;
    constexpr f32 ImpactRadius = 12.0f;
    if (other != nullptr)
    {
        HitWhileMoving(other, point, impulse);
    }

    f32 strength = impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z;
    if (rigidBody != nullptr && rigidBody->physicsBody != nullptr && HardKnock < strength)
    {
        strength *= 3.0f;
        if (1.0f < strength)
        {
            strength = 1.0f;
        }

        SendImpact(ImpactRadius, strength, owner, point);
    }

    CallVirtual<void>(agent, agent->vtable, Agent::CollidedSlot, other, point, impulse);
}

EABI_EXPORT(FUN_0022f440, LaunchNode);

void LaunchNode(f32 gravity, ObjectNode* node, const Vector4* velocity)
{
    // In both lists: its node's motion moves it and its collision cache's triangles stop it
    constexpr u32 PlainKind = 1;
    if (node->rigidBody == nullptr)
    {
        ObjectRigidBody* body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
        node->rigidBody = body;
        SetRigidBodyGravity(0.0f <= gravity ? gravity : DefaultLaunchGravity, body);
        ListRigidBodyFirst(node->rigidBody, PlainKind);
        ListRigidBodySecond(node->rigidBody, PlainKind);
        Box box;
        HullBox(GetCollisionModel(&node->owner->collision, 0), &box);
        node->rollRadius = GetBoxReach(&box);
    }
    else
    {
        SetRigidBodyGravity(0.0f <= gravity ? gravity : DefaultLaunchGravity, node->rigidBody);
    }

    ObjectRigidBody* body = node->rigidBody;
    body->bits.launch = ObjectRigidBodyBits::Launched;
    MotionState* motion = node->motion;
    motion->startVelocity = motion->velocity;
    motion->velocity = *velocity;
}

EABI_EXPORT(FUN_00230428, PushNode);

void PushNode(f32 strength, ObjectNode* node, InstanceContext* other)
{
    MotionBlock* block = node->motionBlock;
    if (block != nullptr)
    {
        Trajectory* trajectory = node->trajectory;
        if (trajectory != nullptr && trajectory->followed == block)
        {
            ObjectRigidBody* body = node->rigidBody;
            if (body != nullptr && body->physicsBody != nullptr)
            {
                body->physicsBody->ReleaseRide();
            }
        }
        else if (block->flags.followedWhenTouched)
        {
            node->FollowMotionBlock(node->motionBlock, GetContextClock(node->owner));
        }

        node->MotionBlockTouched(other);
    }

    if (!(0.0f < strength) || node->rigidBody == nullptr)
    {
        return;
    }

    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 away = place->position;
    ObjectPlace* otherPlace = other->place;
    otherPlace->SyncPosition();
    Vector4 from = otherPlace->position;
    away.x -= from.x;
    away.y -= from.y;
    away.z -= from.z;
    f32 push = strength * 4.0f / (away.x * away.x + away.y * away.y + away.z * away.z + 4.0f);
    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x = away.x * inverse * push;
    away.y = away.y * inverse * push;
    away.z = away.z * inverse * push;
    PushRigidBody(node->rigidBody, &away, &from);
}

namespace
{
f32 SquaredSpeed(const Vector4* velocity)
{
    return velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z;
}
}

u32 ObjectNode::Landed(CollisionSurface* surface, const Vector4* point, const Vector4* velocity)
{
    constexpr f32 Threshold = 10.0f;
    constexpr f32 ImpactRadius = 12.0f;
    u32 played = 0;
    if (surface == nullptr)
    {
        return 0;
    }

    f32 speed = SquaredSpeed(velocity);
    if (surface->impactSound != NoSoundId && Threshold < speed)
    {
        PlaySurfaceContact(speed - Threshold, this, surface, ContactImpact, point, velocity);
        played = 1;
    }

    if (Threshold < speed)
    {
        f32 strength = speed * 3.0f;
        if (1.0f < strength)
        {
            strength = 1.0f;
        }

        SendImpact(ImpactRadius, strength, owner, point);
    }

    return played;
}

u32 ObjectNode::LandedHard(CollisionSurface* surface, const Vector4* point, const Vector4* velocity)
{
    constexpr f32 Threshold = 15.0f;
    if (surface == nullptr)
    {
        return 0;
    }

    f32 speed = SquaredSpeed(velocity);
    if (surface->impactSound == NoSoundId || !(Threshold < speed))
    {
        return 0;
    }

    PlaySurfaceContactHard(speed - Threshold, this, surface, ContactHardImpact, point, velocity);
    return 1;
}

u32 ObjectNode::Scraped(CollisionSurface* surface, const Vector4* point, const Vector4* velocity)
{
    constexpr f32 Threshold = Rounded(0.2);
    if (surface == nullptr)
    {
        return 0;
    }

    f32 speed = SquaredSpeed(velocity);
    if (surface->impactSound == NoSoundId || !(Threshold < speed))
    {
        return 0;
    }

    PlaySurfaceContactHard(speed - Threshold, this, surface, ContactScrape, point, velocity);
    return 1;
}

u32 ObjectNode::AddParticleTrail(const void* arguments)
{
    if (particleTrails == nullptr)
    {
        particleTrails = ParticleTrails::Construct(static_cast<ParticleTrails*>(MemoryAllocate(sizeof(ParticleTrails))));
    }

    return particleTrails->Add(arguments);
}

void ObjectNode::DestroyParticleTrails()
{
    if (particleTrails != nullptr)
    {
        particleTrails->Destroy(DestroyAndFree);
    }

    particleTrails = nullptr;
}

namespace
{
// An instance's place as the retail code reads it, also when there's no instance (the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

bool IsAsleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// Whether the head tracking has a target (the instance its reference is to)
bool HasHeadTarget(const HeadTracking* tracking)
{
    return tracking->target != nullptr && tracking->target->object != nullptr;
}

// A place moved to a position (once it took its matrix's), the instance queued to be stepped when it moved
void MoveInstanceTo(InstanceContext* instance, ObjectPlace* place, const Vector4* position)
{
    place->SyncPosition();
    if (place->MoveTo(position))
    {
        QueueObject(instance);
    }
}
}

InstanceContext* ObjectNodeBase::AwakeFocus()
{
    if (!flags.focusInstance || focusInstance == nullptr)
    {
        return nullptr;
    }

    if (!IsAsleep(focusInstance))
    {
        return focusInstance;
    }

    flags.value &= ~ObjectNodeFlags::FocusMask;
    focusInstance = nullptr;
    return nullptr;
}

InstanceContext* ObjectNode::GetDesignator(u32 designator)
{
    switch (designator)
    {
    case DesignatesLinkedById:
    {
        // An instance without an ID reads the word at address 4
        s32 id = owner->id;
        std::uintptr_t entry = id != -1 ? reinterpret_cast<std::uintptr_t>(&g_InstanceIds->entries[id]) : 0;
        return *reinterpret_cast<InstanceContext* const*>(entry + offsetof(InstanceIds::Entry, spawner));
    }
    case DesignatesItself:
        return owner;
    case DesignatesPlayer:
        return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    case DesignatesHeadTarget:
        if (headTracking == nullptr)
        {
            return nullptr;
        }

        return headTracking->target != nullptr ? static_cast<InstanceContext*>(headTracking->target->object) : nullptr;
    case DesignatesAgentRef2:
        return agentRef2;
    case DesignatesAgentRef1:
        return agentRef1;
    case DesignatesFocus:
        return AwakeFocus();
    default:
        return nullptr;
    }
}

u32 ObjectNode::GetDesignatorPosition(u32 designator, Vector4* position)
{
    ObjectPlace* place;
    switch (designator)
    {
    case DesignatesHeadTarget:
        if (headTracking == nullptr || !HasHeadTarget(headTracking))
        {
            return 0;
        }

        place = RetailPlaceOf(agentRef1);
        break;
    case DesignatesStoredPosition:
        if (!flags.storedPosition)
        {
            return 0;
        }

        *position = storedPosition;
        return 1;
    case DesignatesAgentRef2:
        if (agentRef2 != nullptr && IsAsleep(agentRef2) && !flags.keepsAgentRef2)
        {
            agentRef2 = nullptr;
        }

        if (agentRef2 == nullptr)
        {
            return 0;
        }

        place = RetailPlaceOf(agentRef1);
        break;
    case DesignatesAgentRef1:
        if (agentRef1 != nullptr && IsAsleep(agentRef1))
        {
            agentRef1 = nullptr;
        }

        if (agentRef1 == nullptr)
        {
            return 0;
        }

        place = agentRef1->place;
        break;
    case DesignatesFocus:
    {
        InstanceContext* focus = AwakeFocus();
        if (focus == nullptr)
        {
            return 0;
        }

        place = focus->place;
        break;
    }
    case DesignatesFocusPosition:
        if (!flags.focusPosition)
        {
            return 0;
        }

        position->x = focusPosition.x;
        position->y = focusPosition.y;
        position->z = focusPosition.z;
        position->w = focusPosition.w;
        return 1;
    default:
        return 0;
    }

    place->SyncPosition();
    *position = place->position;
    return 1;
}

u32 ObjectNode::SetDesignatorPosition(u32 designator, const Vector4* position)
{
    InstanceContext* moved;
    switch (designator)
    {
    case DesignatesFocusPosition:
        focusPosition.x = position->x;
        focusPosition.y = position->y;
        focusPosition.z = position->z;
        focusPosition.w = position->w;
        flags.focusPosition = 1;
        flags.focusInstance = 0;
        return 1;
    case DesignatesStoredPosition:
        flags.storedPosition = 1;
        storedPosition = *position;
        return 1;
    case DesignatesFocus:
        moved = AwakeFocus();
        if (moved == nullptr)
        {
            return 0;
        }

        MoveInstanceTo(moved, moved->place, position);
        return 1;
    case DesignatesAgentRef1:
        if (agentRef1 != nullptr && IsAsleep(agentRef1))
        {
            agentRef1 = nullptr;
        }

        moved = agentRef1;
        if (moved == nullptr)
        {
            return 0;
        }

        MoveInstanceTo(moved, moved->place, position);
        return 1;
    case DesignatesAgentRef2:
        if (agentRef2 != nullptr && IsAsleep(agentRef2) && !flags.keepsAgentRef2)
        {
            agentRef2 = nullptr;
        }

        if (agentRef2 == nullptr)
        {
            return 0;
        }

        moved = agentRef1;
        MoveInstanceTo(moved, RetailPlaceOf(moved), position);
        return 1;
    case DesignatesHeadTarget:
        if (headTracking == nullptr || !HasHeadTarget(headTracking))
        {
            return 0;
        }

        moved = agentRef1;
        MoveInstanceTo(moved, RetailPlaceOf(moved), position);
        return 1;
    default:
        return 0;
    }
}

u32 ExitPointPlace(InstanceContext* instance, u32 slot, Vector4* position, Vector4* direction)
{
    constexpr u32 ExitPointSlots = 0x3F;
    slot &= 0xFF;
    if (slot >= ExitPointSlots)
    {
        return 0;
    }

    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    OgiAnimator* animator = model->animator;
    if (animator == nullptr)
    {
        return 0;
    }

    ExitPointAnimation* exitPoint = animator->exitPoints != nullptr ? animator->exitPoints->data[slot] : nullptr;
    if (exitPoint == nullptr)
    {
        return 0;
    }

    ExitPointAnimation* updated = UpdateExitPointMatrix(exitPoint);
    *position = *RowOf(&updated->matrix, 3);
    *direction = *RowOf(&updated->matrix, 2);
    return 1;
}

void CopyVelocity(ObjectNode* node, Vector4* velocity)
{
    *velocity = node->motion->velocity;
}

GameObject* SourceObject(GameNode* node)
{
    auto* object = static_cast<ObjectNodeBase*>(node);
    return object->sourceNode != nullptr ? SourceObject(object->sourceNode) : object->object;
}

PropertyHolder* GetPropsHolderFromInstanceNode(GameNode* node)
{
    auto* object = static_cast<ObjectNodeBase*>(node);
    return object->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(object->sourceNode) : object->properties;
}

GameObject* ObjectNodeBase::OwnObject()
{
    if (ownObjectId == NoObjectId)
    {
        return nullptr;
    }

    ResourceTable* objects = G_GameResourcesObjectPointer->objects;
    if (objects == nullptr)
    {
        return nullptr;
    }

    u16 id = ownObjectId;
    return id != NoObjectId ? static_cast<GameObject*>(objects->items[id & ResourceIndexMask]) : nullptr;
}
