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
#include "game/objects.h"
#include "game/player.h"
#include "game/resources.h"
#include "game/rigidbody.h"
#include "game/string.h"
#include "game/place.h"
#include "game/reference.h"

#include <cstddef>
#include <cstdint>

extern "C"
{
    // A playing sound's slot let go
    void StopSoundChannel(u32 channel) RETAIL(FUN_001e5858);
}

namespace
{
// The node's flags: it's updated every frame (bits 5 and 6), it was updated this frame (3), its frame left it unsettled (4), it
// moved (24), and it's put back where it was before its frame unless it moved (23)
constexpr u32 FlagAlwaysUpdated = 0x60;
constexpr u32 FlagUpdated = 0x8;
constexpr u32 FlagPinned = 0x800000;
constexpr u32 FlagUpdating = 0x20;
// The node's vtable function told when a runner's behaviour finished
constexpr u32 RunnerFinishedSlot = 22;
// The object 0x114 bytes into the node steps with the clock through its vtable (8 bytes in) function 3
constexpr u32 PartStepSlot = 3;

struct SteppedPart
{
    u8 unknown00[8];
    const GccVTableEntry* vtable;
};

// Whether the trajectory controller asks for its frame while the node isn't updated (bit 7 of the byte 0x6D bytes into what it
// follows, 0xE0 bytes in)
bool TrajectoryKeepsStepping(const Trajectory* trajectory)
{
    return (trajectory->followed->flags & MotionBlock::KeepsStepping) != 0;
}

// The instance's seen stamp past the node's own (0xFFFF none), past the grace: 0 within it
u32 StampsUnseen(const ObjectNode* node)
{
    const InstanceContext* owner = node->owner;
    u32 seen = owner->seen[0] | owner->seen[1] << 8 | owner->seen[2] << 16;
    u32 since = node->unknown06;
    if (since == 0xFFFF || !(since < seen))
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
    u32 running = clock->flags & TimeClock::FlagRunning;
    if (instance->chunk == nullptr)
    {
        return 0;
    }

    if (running != 0)
    {
        u32 unseen = StampsUnseen(this);
        bool always = (flags & FlagAlwaysUpdated) != 0;
        if (always || unseen < g_ObjectUpdateRate.cutoff)
        {
            bool now = always || unseen == 0;
            if (!now)
            {
                // Every 2^n frames, staggered by the node's address
                u32 mask = 0xFFFF;
                if (unseen != 0xFFFFFFFF)
                {
                    s32 power = static_cast<s32>(g_ObjectUpdateRate.slope * static_cast<f32>(unseen)) + 1;
                    mask = (1u << (power & 0x1F)) - 1;
                }

                now = ((g_RenderedFrames + (reinterpret_cast<u32>(this) >> 8)) & mask) == 0;
            }

            if (!now)
            {
                GameNode::flags |= FlagKeepTime;
            }
            else
            {
                ObjectPlace* place = owner->place;
                place->SyncPosition();
                unknownB0 = place->position;
                flags = (flags | FlagUpdated) & ~FlagMoves & ~FlagUnsettled;
                auto* part = static_cast<SteppedPart*>(reinterpret_cast<void*>(unknown114));
                if (part != nullptr)
                {
                    CallVirtual<void>(part, part->vtable, PartStepSlot, clock);
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
                if ((flags & FlagPinned) != 0)
                {
                    // The frame's move kept (unknownC0), and the instance put back unless it moves by itself
                    place = owner->place;
                    place->SyncPosition();
                    unknownC0 = place->position;
                    unknownC0.x = unknownC0.x - unknownB0.x;
                    unknownC0.y = unknownC0.y - unknownB0.y;
                    unknownC0.z = unknownC0.z - unknownB0.z;
                    if ((flags & FlagMoves) == 0)
                    {
                        InstanceContext* pinned = owner;
                        place = pinned->place;
                        place->SyncPosition();
                        if (place->MoveTo(&unknownB0))
                        {
                            QueueObject(pinned);
                        }
                    }
                }

                flags &= ~FlagUpdating;
                return GameNode::Update(clock);
            }
        }

        flags &= ~FlagUpdated;
        if (trajectory != nullptr && TrajectoryKeepsStepping(trajectory))
        {
            StepTrajectory(trajectory, clock, this);
        }

        if (rigidBody != nullptr && rigidBody->physicsBody != nullptr)
        {
            StepMovement(this, clock);
        }
    }

    flags &= ~FlagUpdating;
    return GameNode::Update(clock);
}

void StepMovement(ObjectNode* node, TimeClock* clock)
{
    // The rigid body's bits: it moves (8, 13, 15 of the high word), its contacts (19-21 of the high word, 51 and 52 handed on as
    // bit 24 of the other word), and what makes the node told (54)
    constexpr u64 Moves = 0x102200000000000;
    constexpr u64 Touching = 0x18000000000000;
    constexpr u64 Contacts = 0x38000000000000;
    constexpr u64 TellsNode = 0x40000000000000;
    constexpr u64 TouchingHandedOn = 0x1000000;
    constexpr u64 Cleared90 = 0x2 | 0x20;
    // The instance's flag 6 holds its body still
    constexpr u32 InstanceHeld = 0x40;
    ObjectRigidBody* body = node->rigidBody;
    if (body != nullptr)
    {
        if ((body->bits88 & Moves) != 0 && (node->owner->flags & InstanceHeld) == 0)
        {
            StepRigidBody(body, clock, nullptr);
        }

        // The middle of the instance's collision box (its bounds 0x40 bytes in)
        InstanceContext* instance = node->owner;
        const auto* bounds = reinterpret_cast<const Vector4*>(reinterpret_cast<const u8*>(instance) + 0x40);
        Vector4& middle = node->unknown20;
        middle = bounds[1];
        middle.x = middle.x - bounds[0].x;
        middle.y = middle.y - bounds[0].y;
        middle.z = middle.z - bounds[0].z;
        middle.x = middle.x * 0.5f;
        middle.y = middle.y * 0.5f;
        middle.z = middle.z * 0.5f;
        middle.x = middle.x + bounds[0].x;
        middle.y = middle.y + bounds[0].y;
        middle.z = middle.z + bounds[0].z;
        body = node->rigidBody;
        u64 bits88 = body->bits88;
        u64 touching = (bits88 & Touching) != 0 ? TouchingHandedOn : 0;
        body->bits88 = bits88 & ~Contacts;
        body->bits90 = ((body->bits90 & ~TouchingHandedOn) | touching) & ~Cleared90;
        if ((node->rigidBody->bits88 & TellsNode) != 0)
        {
            ReleaseRigidBodyAtRest(node);
        }
    }

    if ((node->flags & FlagPinned) == 0 && node->unknown154 != 0)
    {
        node->unknown154--;
    }
}

InstancePlacement* InstancePlacement::Construct(InstancePlacement* information, ChunkData* chunk)
{
    information->chunk.string = nullptr;
    information->chunk.length = 0;
    information->chunk.capacity = 0;
    information->flags = 0;
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
    Reference* data = chunk->data;
    InstancePlacement::Construct(&node->information, data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr);
    node->ownInformation = nullptr;
    node->informationPointer = &node->information;
    node->ownObjectId = 0xFFFF;
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
    constexpr u32 ApplySlot = 2;
    constexpr u32 TriggerMessageSlot = 19;
    flags |= FlagHandledEvent;
    auto* event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
    u16 kind = event->unknown04;
    if (kind >= FirstNodeEvent)
    {
        if (kind <= LastAppliedEvent)
        {
            auto* applied = reinterpret_cast<GameEvent*>((*handle)->object);
            CallVirtual<void>(applied, applied->vtable, ApplySlot, this, G_GameResourcesObjectPointer);
        }
        else if (kind == TriggerMessage)
        {
            event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
            Reference* sender = event->argument;
            messageSender = sender != nullptr ? static_cast<InstanceContext*>(sender->object) : nullptr;
            event = *handle != nullptr ? reinterpret_cast<GameEvent*>((*handle)->object) : nullptr;
            message = event->type;
            messageTime = GetContextClock(owner)->time;
            if (object->TriggerBehaviourCount() != 0)
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
    constexpr u32 StartBehaviourSlot = 18;
    constexpr u16 NoStarter = 0xFFFF;
    u32 count = object->TriggerBehaviourCount();
    u32 wanted = received & 0xFFFF;
    for (u32 index = 0; index < count;)
    {
        const u32* behaviour = &object->triggerBehaviours.items[index];
        index++;
        if ((*behaviour & GameObject::MessageMask) != wanted)
        {
            continue;
        }

        u16 id = *behaviour >> GameObject::StarterShift & GameObject::StarterMask;
        ResourceTable* scripts = G_GameResourcesObjectPointer->scripts;
        auto* starter = id != NoStarter ? static_cast<ScriptStarter*>(scripts->items[id & 0x7FFF]) : nullptr;
        u32 runner = reinterpret_cast<const u8*>(behaviour)[GameObject::RunnerShift / 8] & GameObject::RunnerMask;
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
    return 1;
}

void ObjectNodeBase::DefaultSlot9()
{
}

void ObjectNodeBase::DefaultSlot11()
{
}

void ObjectNodeBase::DefaultSlot22()
{
}

u32 ObjectNodeBase::DefaultSlot23()
{
    return 0xFF;
}

void ObjectNodeBase::DefaultSlot24()
{
}

void ObjectNodeBase::DefaultSlot26()
{
}

void ObjectNodeBase::DefaultSlot27()
{
}

u32 ObjectNodeBase::DefaultSlot28()
{
    return 1;
}

u32 ObjectNodeBase::DefaultSlot29()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot30()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot31()
{
    return 0;
}

void ObjectNodeBase::DefaultSlot33()
{
}

void ObjectNodeBase::DefaultSlot34()
{
}

void ObjectNodeBase::DefaultSlot35()
{
}

u32 ObjectNodeBase::DefaultSlot36()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot37()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot38()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot39()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot40()
{
    return 0;
}

u32 ObjectNodeBase::DefaultSlot41()
{
    return 0;
}

u32 ObjectNodeBase::PrototypeSlot18()
{
    return 0;
}

void ObjectNodeBase::PrototypeSlot19()
{
}

void ObjectNodeBase::PrototypeSlot21()
{
}

void ObjectNodeBase::Reset()
{
    constexpr u32 SetAgentSlot = 12;
    flags = 0;
    CallVirtual<void>(this, vtable, SetAgentSlot, static_cast<Agent*>(nullptr));
    ClearMessages();
    ownObjectId = 0xFFFF;
    unknown8C = 0xFF;
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
    message = 0xFFFF;
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
    unknown134 = -1;
    rigidBody = nullptr;
    translator = nullptr;
    rotator = nullptr;
    physics = nullptr;
    unknown114 = 0;
    particleTrails = nullptr;
    trajectory = nullptr;
    motionBlock = nullptr;
    headTracking = nullptr;
    perception = nullptr;
    surface = -1;
    flags = 0;
    agentRef1 = nullptr;
    agentRef2 = nullptr;
    unknown154 = 0;
    f32 w = unknownC0.w;
    unknownC0 = g_DefaultBox.min;
    unknownC0.w = w;
    ForgetStoredPosition();
    // Bit fields of the 64 bits from 0x150: bits 40 and 41 clear, the 16 bits from 42 on none
    u64& bits = *reinterpret_cast<u64*>(&unknown150);
    bits = ((bits & ~(u64{1} << 40)) | u64{0xFFFF} << 42) & ~(u64{1} << 41);
    *reinterpret_cast<s32*>(&unknown155[0x164 - 0x155]) = -1;
    *reinterpret_cast<f32*>(&unknown155[0x16C - 0x155]) = -1.0f;
    unknown155[0x158 - 0x155] = 0xFF;
    unknown150 = 0;
    *reinterpret_cast<u32*>(&unknown155[0x170 - 0x155]) = 0;
    unknown155[0x168 - 0x155] = 0xFF;
    unknown155[0x169 - 0x155] = 0xFF;
    *reinterpret_cast<u32*>(&unknown155[0x174 - 0x155]) = 0;
    unknown155[0x160 - 0x155] = 0xFF;
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
    ReleaseLinks();
    DestroyPart114();
    ObjectNodeBase::Destroy(destroyFlags);
}

void ObjectNode::ForgetStoredPosition()
{
    flags &= ~ObjectNodeBase::FlagStoredPosition;
}

void ObjectNode::ReleaseLinks()
{
    constexpr u32 ReleaseLinksSlot = 35;
    if (g_UnloadingEverything == 0)
    {
        CallVirtual<void>(this, vtable, ReleaseLinksSlot, this);
    }
}

void ObjectNode::DestroyPart114()
{
    constexpr u32 TellSlot = 5;
    constexpr u32 DestructorSlot = 1;
    auto* part = reinterpret_cast<SteppedPart*>(unknown114);
    if (part == nullptr)
    {
        return;
    }

    CallVirtual<void>(part, part->vtable, TellSlot);
    part = reinterpret_cast<SteppedPart*>(unknown114);
    if (part != nullptr)
    {
        CallVirtual<void>(part, part->vtable, DestructorSlot, DestroyAndFree);
    }

    unknown114 = 0;
}

void ObjectNode::SetOwner(InstanceContext* instance)
{
    information.Take(instance, 1);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    unknownB0 = place->position;
    GameNode::SetOwner(instance);
    unknown20 = unknownB0;
}

void ObjectNode::LeftChunk(u32 why)
{
    if (why < 2)
    {
        ClearComebackPlacement(this);
    }
}

void ObjectNode::CallSlot11()
{
    constexpr u32 Slot11 = 11;
    CallVirtual<void>(this, vtable, Slot11);
}

u32 ObjectNode::ItemType()
{
    return 0x180D;
}

u32 ObjectNode::Slot14()
{
    return 1;
}

u32 ObjectNode::TakesPackets()
{
    return 1;
}

u32 ObjectNode::Slot16()
{
    return 0;
}

u32 ObjectNode::Slot17()
{
    return 0;
}

void ObjectNode::Slot20()
{
}

u32 ObjectNode::Slot25()
{
    return 0xFF;
}

void ObjectNode::Slot33()
{
}

u32 ObjectNode::HasNoByte160()
{
    return unknown155[0x160 - 0x155] == 0xFF;
}

u32 ObjectNode::Slot41()
{
    return 1;
}

void ObjectNode::Slot43()
{
}

void ObjectNode::StopSound()
{
    constexpr u8 NoSound = 0xFF;
    u8& sound = unknown155[0x158 - 0x155];
    if (sound != NoSound)
    {
        StopSoundChannel(sound);
        sound = NoSound;
    }
}

void ObjectNode::Reset()
{
    constexpr u32 PacketEndedSlot = 42;
    constexpr u32 DestroyParticlesSlot = 24;
    // The instance's flag 8 (cleared)
    constexpr u32 InstanceFlag8 = 0x100;
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
    f32 w = unknownC0.w;
    unknownC0 = g_DefaultBox.min;
    unknownC0.w = w;
    sourceNode = nullptr;
    DestroyPart114();
    ReleaseTrajectory();
    CallVirtual<void>(this, vtable, DestroyParticlesSlot);
    ReleaseRigidBody();
    ReleaseAttachments();
    agentRef1 = nullptr;
    ReleasePerception();
    if ((flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
    {
        agentRef2 = nullptr;
    }

    ForgetStoredPosition();
    ReleaseStoredPlace();
    ReleaseHeadTracking();
    ReleaseMotionBlock();
    ClearMessages();
    owner->flags &= ~InstanceFlag8;
    unknown155[0x158 - 0x155] = 0xFF;
    *reinterpret_cast<s32*>(&unknown155[0x164 - 0x155]) = -1;
    *reinterpret_cast<u32*>(&unknown155[0x174 - 0x155]) = 0;
    unknown155[0x160 - 0x155] = 0xFF;
}

void ObjectNode::PacketEnded(BehaviourRunner* runner)
{
    if (runner != nullptr)
    {
        u32 slot = runner->flags >> BehaviourRunner::SlotShift & BehaviourRunner::SlotMask;
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
    flags &= ~ObjectNodeBase::FlagFocusPosition & ~ObjectNodeBase::FlagFocusInstance;
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
    constexpr u32 InstanceFlag6 = 0x40;
    constexpr u32 InstanceFlag7 = 0x80;
    constexpr u32 AttachmentsKind = 6;
    void* attachments = GetGameNode(&owner->nodes, AttachmentsKind);
    if (attachments != nullptr)
    {
        ReleaseAttachmentsNode(attachments, 0, 0, 0);
    }

    owner->flags &= ~InstanceFlag6;
    owner->flags &= ~InstanceFlag7;
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
    // The instance's flag 15 (cleared), the node's vtable slot 44 (its sound stopped)
    constexpr u32 InstanceFlag15 = 0x8000;
    constexpr u32 StopSoundSlot = 44;
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
        flags &= ~ObjectNodeBase::FlagMovesStoredPlace;
    }

    owner->flags &= ~InstanceFlag15;
    CallVirtual<void>(this, vtable, StopSoundSlot);
}

void ObjectNode::RunnerFinished()
{
    constexpr u32 DestroyParticlesSlot = 24;
    constexpr u32 RunnerTrails = 1;
    if ((flags & FlagKeepsParticles) == 0)
    {
        CallVirtual<void>(this, vtable, DestroyParticlesSlot);
    }
    else if (particleTrails != nullptr)
    {
        particleTrails->RemoveKind(RunnerTrails);
    }

    if ((flags & FlagKeepsTrajectory) == 0)
    {
        ReleaseTrajectory();
    }

    if ((flags & FlagKeepsPerception) == 0)
    {
        ReleasePerception();
    }
}

void ObjectNode::ForgetDesignator(u32 designator)
{
    constexpr u32 Focus = 0;
    constexpr u32 AgentRef1 = 1;
    constexpr u32 AgentRef2 = 2;
    constexpr u32 StoredPosition = 3;
    if (designator == AgentRef1)
    {
        agentRef1 = nullptr;
    }
    else if (designator == Focus)
    {
        flags &= ~FlagFocusPosition & ~FlagFocusInstance;
    }
    else if (designator == AgentRef2)
    {
        agentRef2 = nullptr;
    }
    else if (designator == StoredPosition)
    {
        ForgetStoredPosition();
    }
}

void ObjectNode::ReleaseParts()
{
    constexpr u32 DestroyParticlesSlot = 24;
    constexpr u32 StopSoundSlot = 44;
    constexpr u32 PacketEndedSlot = 42;
    CallVirtual<void>(this, vtable, DestroyParticlesSlot);
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
            flags |= FlagFocusInstance;
        }

        flags &= ~FlagFocusPosition;
        return 1;
    default:
        return 0;
    }
}

u32 ObjectNode::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
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
    constexpr u32 PartRestartSlot = 4;
    constexpr u32 ReleasePartsSlot = 35;
    constexpr u32 Slot20 = 20;
    constexpr u32 StopRunnersSlot = 21;
    constexpr u32 ResetSlot = 13;
    auto* part = static_cast<SteppedPart*>(reinterpret_cast<void*>(unknown114));
    if (part != nullptr)
    {
        CallVirtual<void>(part, part->vtable, PartRestartSlot, word);
    }

    CallVirtual<void>(this, vtable, ReleasePartsSlot);
    CallVirtual<void>(this, vtable, Slot20);
    CallVirtual<void>(this, vtable, StopRunnersSlot, 1u);
    CallVirtual<void>(this, vtable, ResetSlot, owner);
    informationPointer->Apply(owner);
    const Box* box = owner->CollisionBox();
    unknown20 = box->max;
    unknown20.x -= box->min.x;
    unknown20.y -= box->min.y;
    unknown20.z -= box->min.z;
    unknown20.x *= 0.5f;
    unknown20.y *= 0.5f;
    unknown20.z *= 0.5f;
    unknown20.x += box->min.x;
    unknown20.y += box->min.y;
    unknown20.z += box->min.z;
    ObjectPlace* place = owner->place;
    place->SyncPosition();
    unknownB0 = place->position;
    time = clock->time;
}

void ObjectNode::Collided(void* other, const Vector4* point, const Vector4* impulse)
{
    constexpr f32 HardKnock = 10.0f;
    constexpr f32 ImpactRadius = 12.0f;
    constexpr u32 AgentCollidedSlot = 7;
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

    CallVirtual<void>(agent, agent->vtable, AgentCollidedSlot, other, point, impulse);
}

EABI_EXPORT(FUN_0022f440, LaunchNode);

void LaunchNode(f32 gravity, ObjectNode* node, const Vector4* velocity)
{
    constexpr f32 DefaultGravity = 30.0f;
    // The rigid body's bits 40-41
    constexpr u64 LaunchMask = u64{3} << 40;
    constexpr u64 Launched = u64{2} << 40;
    if (node->rigidBody == nullptr)
    {
        ObjectRigidBody* body = ConstructRigidBody(MemoryAllocate(0xE0), node);
        node->rigidBody = body;
        SetRigidBodyGravity(0.0f <= gravity ? gravity : DefaultGravity, body);
        ListRigidBodyFirst(node->rigidBody, 1);
        ListRigidBodySecond(node->rigidBody, 1);
        Box box;
        HullBox(GetCollisionModel(&node->owner->collision, 0), &box);
        node->rollRadius = GetBoxReach(&box);
    }
    else
    {
        SetRigidBodyGravity(0.0f <= gravity ? gravity : DefaultGravity, node->rigidBody);
    }

    ObjectRigidBody* body = node->rigidBody;
    body->bits88 = (body->bits88 & ~LaunchMask) | Launched;
    MotionState* motion = node->motion;
    motion->startVelocity = motion->velocity;
    motion->velocity = *velocity;
}

EABI_EXPORT(FUN_00230428, PushNode);

void PushNode(f32 strength, ObjectNode* node, InstanceContext* other)
{
    constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
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
        else if ((block->flags & MotionBlock::FollowedWhenTouched) != 0)
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
// The contact kinds of a surface's sounds and particles
constexpr u32 ImpactContact = 0;
constexpr u32 HardImpactContact = 4;
constexpr u32 ScrapeContact = 5;
constexpr u16 NoSound = 0xFFFF;

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
    if (surface->impactSound != NoSound && Threshold < speed)
    {
        PlaySurfaceContact(speed - Threshold, this, surface, ImpactContact, point, velocity);
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
    if (surface->impactSound == NoSound || !(Threshold < speed))
    {
        return 0;
    }

    PlaySurfaceContactHard(speed - Threshold, this, surface, HardImpactContact, point, velocity);
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
    if (surface->impactSound == NoSound || !(Threshold < speed))
    {
        return 0;
    }

    PlaySurfaceContactHard(speed - Threshold, this, surface, ScrapeContact, point, velocity);
    return 1;
}

u32 ObjectNode::AddParticleTrail(const void* arguments)
{
    constexpr u32 TrailsSize = 0x6C;
    if (particleTrails == nullptr)
    {
        particleTrails = ParticleTrails::Construct(static_cast<ParticleTrails*>(MemoryAllocate(TrailsSize)));
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
    return (instance->flags & ReferencedObject::FlagAsleep) != 0;
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
    if ((flags & FlagFocusInstance) == 0 || focusInstance == nullptr)
    {
        return nullptr;
    }

    if (!IsAsleep(focusInstance))
    {
        return focusInstance;
    }

    flags &= ~FlagFocusPosition & ~FlagFocusInstance;
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
        return *reinterpret_cast<InstanceContext* const*>(entry + offsetof(InstanceIds::Entry, unknown04));
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
        if ((flags & FlagStoredPosition) == 0)
        {
            return 0;
        }

        *position = storedPosition;
        return 1;
    case DesignatesAgentRef2:
        if (agentRef2 != nullptr && IsAsleep(agentRef2) && (flags & FlagKeepsAgentRef2) == 0)
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
        if ((flags & FlagFocusPosition) == 0)
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
        flags = (flags | FlagFocusPosition) & ~FlagFocusInstance;
        return 1;
    case DesignatesStoredPosition:
        flags |= FlagStoredPosition;
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
        if (agentRef2 != nullptr && IsAsleep(agentRef2) && (flags & FlagKeepsAgentRef2) == 0)
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
    constexpr u32 ModelNodeKind = 3;
    constexpr u32 Slots = 0x3F;
    slot &= 0xFF;
    if (slot >= Slots)
    {
        return 0;
    }

    auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNodeKind));
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
    if (ownObjectId == 0xFFFF)
    {
        return nullptr;
    }

    ResourceTable* objects = G_GameResourcesObjectPointer->objects;
    if (objects == nullptr)
    {
        return nullptr;
    }

    u16 id = ownObjectId;
    return id != 0xFFFF ? static_cast<GameObject*>(objects->items[id & 0x7FFF]) : nullptr;
}
