#include "game/objectnode.h"

#include "game/animation.h"
#include "game/camerarig.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/controllers.h"
#include "game/decals.h"
#include "game/events.h"
#include "game/instanceparticles.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objects.h"
#include "game/particles.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/sound.h"

#include <cstddef>
#include <cstdint>

// The particle trails an object node leaves: each step a trail may emit (by its kind), its particle system (its surface's when it
// asks for its contact's) played from its frame, decals and a sand puff on sand and snow, a sound, a camera shake and a message

extern "C"
{
    // A decal (a footprint) at a frame's position offset in the instance's space (the first argument goes unread)
    void AddInstanceDecal(const CollisionSurface* unused, InstanceContext* instance, const Matrix4x4* frame,
                          const Vector4* offset) RETAIL(AddInstanceDecal);
}

namespace
{
constexpr u32 NoContactKind = 0xFF;
// Kind 1's speed below which it doesn't emit
constexpr f32 SlowestSpeed = Rounded(0.0001);
// The surfaces that keep tracks (sand and snow), and the system sand puffs up with
constexpr u16 SandSurface = 10;
constexpr u16 SnowSurface = 15;
constexpr s32 SandPuffSystem = 0xF5;
// The strength past the threshold makes the sound this much louder, up to 3
constexpr f32 LoudnessPerStrength = Rounded(0.1);
constexpr f32 LoudestSound = 3.0f;
// A camera shake only along x and y once one of them is above this
constexpr f32 SmallestShake = Rounded(0.01);

// The offset of its frame when it's given one
const Vector4* OffsetOf(const TrailArguments* trail)
{
    return trail->bits.offsetGiven ? reinterpret_cast<const Vector4*>(trail->offset) : nullptr;
}

// A frame turned and moved by its offset as the trail asks
void OrientFrame(const TrailArguments* trail, Matrix4x4* frame)
{
    u32 turned = trail->bits.turned;
    OrientParticleFrame(turned, trail->bits.axes, OffsetOf(trail), frame);
}

// The frame particles leave its instance from, oriented as the trail asks
Matrix4x4 TrailFrame(const TrailArguments* trail, InstanceContext* instance)
{
    Matrix4x4 frame = *ParticleFrame(instance, trail->exitPoint);
    OrientFrame(trail, &frame);
    return frame;
}

// Whether a squared size passes a threshold: above it when it's positive, below its negative anyway
bool PassesThreshold(f32 squared, f32 threshold)
{
    return (0.0f < threshold && threshold < squared) || squared < -threshold;
}

f32 SquaredLength(const Vector4* vector)
{
    return vector->x * vector->x + vector->y * vector->y + vector->z * vector->z;
}

// The seconds since a time of the clock's
f32 SecondsSince(const TimeClock* clock, s32 time)
{
    return static_cast<f32>(static_cast<s32>(clock->time - time)) * g_SecondsPerClockUnit;
}

// Whether a trail emits this step, by its kind; its node's trails' time is the trail's
bool TrailEmits(const TrailArguments* trail, TimeClock* clock, ObjectNode* node, f32 strength)
{
    switch (trail->bits.kind)
    {
    case TrailArguments::KindAlways:
        return true;
    case TrailArguments::KindTimed:
    {
        s32* time = node->particleTrails->time;
        f32 interval = trail->interval;
        if (trail->bits.bySpacing)
        {
            Vector4 velocity = node->motion->velocity;
            f32 speed = Kept(__builtin_sqrtf(SquaredLength(&velocity)));
            if (!(SlowestSpeed <= speed))
            {
                return false;
            }

            interval = 1.0f / (speed * trail->spacing);
        }

        if (!(interval < SecondsSince(clock, *time)))
        {
            return false;
        }

        // The first time (none yet) it only starts counting, unless the interval has no random extra
        bool emits = 0 < *time || trail->randomInterval == 0.0f;
        *time = clock->time;
        *time = *time - static_cast<s32>(RandomBelowFloat(trail->randomInterval) * g_ClockUnitsPerSecond);
        return emits;
    }
    case TrailArguments::KindFaster:
    {
        Vector4 velocity = node->motion->velocity;
        if (!PassesThreshold(SquaredLength(&velocity), trail->threshold))
        {
            return false;
        }

        if (trail->bits.bySpacing)
        {
            s32* time = node->particleTrails->time;
            if (!(trail->spacing < SecondsSince(clock, *time)))
            {
                return false;
            }

            *time = clock->time;
        }

        return true;
    }
    case TrailArguments::KindChanging:
    {
        const MotionState* motion = node->motion;
        Vector4 change = {motion->velocity.x - motion->startVelocity.x, motion->velocity.y - motion->startVelocity.y,
                          motion->velocity.z - motion->startVelocity.z, 1.0f};
        return PassesThreshold(SquaredLength(&change), trail->threshold);
    }
    case TrailArguments::KindUnsetVector:
    {
        // Retail bug: kind 5 compares a vector it never sets (kind 4's change of velocity, left on the stack by an earlier trail
        // at best): the C++ reads what its own frame holds there
        Vector4 leftover;
        asm volatile("" : "=m"(leftover));
        return PassesThreshold(SquaredLength(&leftover), trail->threshold);
    }
    case TrailArguments::KindAnimation:
        return TrailAnimationRestarted(trail, clock, node) != 0;
    case TrailArguments::KindStronger:
        return trail->strength < strength;
    default:
        return false;
    }
}

// The frame the trail's emitter follows: its exit point's (oriented), where its instance started or its instance's place (moved
// by its offset), or the origin
Matrix4x4 EmitterFrame(const TrailArguments* trail, ObjectNode* node, InstanceContext* instance)
{
    constexpr u32 ExitPointSpace = 0;
    constexpr u32 StartSpace = 1;
    constexpr u32 PlaceSpace = 2;
    u32 space = trail->bits.space;
    if (space == ExitPointSpace)
    {
        return TrailFrame(trail, instance);
    }

    Matrix4x4 frame;
    InitIdentityMatrix(&frame);
    if (space == StartSpace)
    {
        MatrixFromRotation(&frame, &node->information.rotation);
        *RowOf(&frame, PositionRow) = node->information.position;
    }
    else if (space == PlaceSpace)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        frame = place->matrix;
    }
    else
    {
        return frame;
    }

    if (trail->bits.offsetGiven)
    {
        MoveAlongAxes(&frame, OffsetOf(trail));
    }

    return frame;
}

// The surface ID (no surface reads the halfword at address 0x60, as retail does)
u16 SurfaceIdOf(const CollisionSurface* surface)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(surface) + offsetof(CollisionSurface, surfaceId);
    return *reinterpret_cast<const u16*>(address);
}

// The sound of one of the trail's slots (picked at random among its count): the node's object's (its source's), else its own
// object's (0xFFFF none)
u32 SlotSound(const TrailArguments* trail, ObjectNode* node)
{
    // A count past the 8 slots reads the halfwords after them
    u32 slot = trail->sounds[RandomBelow(trail->bits.soundCount)];
    GameObject* object = node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
    if (slot == NoSoundSlot)
    {
        return NoSoundId;
    }

    u16 found;
    GetObjectSoundId(&found, object, slot);
    if (found != NoSoundId)
    {
        return found & ResourceIndexMask;
    }

    GameObject* own = node->OwnObject();
    if (own == nullptr)
    {
        return NoSoundId;
    }

    GetObjectSoundId(&found, own, slot);
    return found != NoSoundId ? found & ResourceIndexMask : NoSoundId;
}
}

TrailArguments* ConstructTrailArguments(TrailArguments* trail)
{
    // Bits 29-31 and 59-63 stay as they were; bits 4-9 and 14-17 are 1, the contact's kind none, everything else 0
    TrailBits made = {};
    made.unused4 = 1;
    made.unused14 = 1;
    made.contact = NoContactKind;
    trail->bits.value = (trail->bits.value & TrailBits::KeptOnMaking) | made.value;
    trail->exitPoint = NoParticleExitPoint;
    trail->system = NoParticleSystem;
    for (u16& sound : trail->sounds)
    {
        sound = NoSoundId;
    }

    trail->extraSound = NoSoundId;
    trail->message = NoMessage;
    trail->surface = ObjectNode::NoSurface;
    trail->threshold = 0.0f;
    trail->interval = 0.0f;
    trail->randomInterval = 0.0f;
    trail->unused30 = 0;
    trail->shakeX = 0.0f;
    trail->shakeY = 0.0f;
    trail->shakeStrength = 0.0f;
    trail->unused40 = 0;
    trail->shakeFalloff = 0.0f;
    trail->volume = 1.0f;
    trail->randomPitch = 0.0f;
    trail->pitch = 1.0f;
    trail->progress = 0.0f;
    trail->strength = 0.0f;
    trail->spacing = 0.0f;
    return trail;
}

s32 StepTrail(TrailArguments* trail, TimeClock* clock, ObjectNode* node, s32 emitter)
{
    s32 result = -1;
    f32 strength = node->particleTrails->strength;
    bool emits = TrailOnItsSurface(trail, node) != 0 && TrailEmits(trail, clock, node, strength);
    InstanceContext* instance = node->owner;
    u32 kind = trail->bits.kind;
    u32 contact = trail->bits.contact;
    // The surface of the water the node is in (and the point where it touches the water), else the one it stands on
    const Vector4* waterPoint = nullptr;
    const CollisionSurface* surface = nullptr;
    if (contact != NoContactKind)
    {
        s32 index = node->waterSurface;
        if (index != ObjectNode::NoSurface)
        {
            waterPoint = &node->waterPoint;
        }
        else
        {
            index = node->surface;
        }

        if (index != ObjectNode::NoSurface)
        {
            surface = &g_CollisionSurfaces.surfaces[index];
        }
    }

    // The frame is taken from the exit point once (and put at the water's point once its decals are left)
    Matrix4x4 frame;
    bool framed = false;
    bool atWater = false;
    u32 system = surface != nullptr ? GetSurfaceParticle(surface, contact) : trail->system;
    if (system != NoParticleSystem)
    {
        if (kind == TrailArguments::KindAnimation)
        {
            if (emits)
            {
                framed = true;
                frame = *ParticleFrame(instance, trail->exitPoint);
                if (waterPoint != nullptr)
                {
                    OrientFrame(trail, &frame);
                    atWater = true;
                    *RowOf(&frame, PositionRow) = *waterPoint;
                    LeaveTrailDecals(trail, &frame, instance->chunk);
                }
                else if (SurfaceIdOf(surface) == SandSurface || SurfaceIdOf(surface) == SnowSurface)
                {
                    AddInstanceDecal(surface, instance, &frame, OffsetOf(trail));
                }

                s32 played = StartEmitterKeepingTranslation(instance, system, 1, nullptr);
                if (played >= 0)
                {
                    SetEmitterFrame(played, &frame);
                }
            }
            else
            {
                result = StopTrail(trail, emitter);
            }
        }
        else if (!emits)
        {
            result = StopTrail(trail, emitter);
        }
        else if (emitter >= 0)
        {
            result = emitter;
        }
        else
        {
            result = StartTrail(trail, system, 1, node);
        }

        if (result >= 0)
        {
            // The emitter passed in follows the frame (none on the step that started one: it's at its instance until the next)
            Matrix4x4 emitterFrame = EmitterFrame(trail, node, instance);
            if (trail->bits.gravityFrame)
            {
                SetEmitterGravityFrame(emitter, &emitterFrame);
            }
            else
            {
                SetEmitterFrame(emitter, &emitterFrame);
            }
        }
    }
    else if (surface != nullptr && kind == TrailArguments::KindAnimation && emits)
    {
        u16 id = surface->surfaceId;
        if (id == SandSurface || id == SnowSurface)
        {
            framed = true;
            frame = *ParticleFrame(instance, trail->exitPoint);
            AddInstanceDecal(surface, instance, &frame, OffsetOf(trail));
        }

        if (id == SandSurface)
        {
            s32 puff = -1;
            StartParticleEmitter(&puff, SandPuffSystem, &frame.m[PositionRow][0], 1, instance->chunk);
        }
    }

    f32 volume = trail->bits.volumeGiven ? trail->volume : -1.0f;
    u32 sound = NoSoundId;
    if (emits)
    {
        if (surface != nullptr)
        {
            sound = trail->bits.noSurfaceSound ? NoSoundId : GetSurfaceSound(surface, contact, &volume);
            if (kind == TrailArguments::KindAnimation && !atWater)
            {
                framed = true;
                frame = TrailFrame(trail, instance);
                if (waterPoint != nullptr)
                {
                    *RowOf(&frame, PositionRow) = *waterPoint;
                    LeaveTrailDecals(trail, &frame, instance->chunk);
                }
            }
        }
        else
        {
            sound = SlotSound(trail, node);
        }
    }

    u32 seen = instance->seen;
    if (sound != NoSoundId && emits && seen < InstanceContext::SoundSeenLimit)
    {
        if (!framed)
        {
            framed = true;
            frame = TrailFrame(trail, instance);
        }

        u32 group = trail->bits.soundGroup == 1 ? 1 : 0;
        if (0.0f < trail->strength)
        {
            f32 louder = (strength - trail->strength) * LoudnessPerStrength;
            if (0.0f < volume)
            {
                louder = volume * louder;
            }

            volume = louder;
            if (LoudestSound < volume)
            {
                volume = LoudestSound;
            }
        }

        f32 pitch = trail->bits.pitchGiven ? trail->pitch : 1.0f;
        if (trail->bits.randomPitch)
        {
            pitch = pitch + RandomSignedTimes(trail->randomPitch);
        }

        if (trail->bits.pitchByThreshold)
        {
            pitch = pitch * trail->threshold;
        }

        PlaySoundByIdAt(volume, pitch, sound, group, instance->chunk, RowOf(&frame, PositionRow), ListenerVoiceKind(), -1);
    }

    if (trail->bits.shakesCamera && emits)
    {
        if (!framed)
        {
            frame = TrailFrame(trail, instance);
        }

        if (SmallestShake < trail->shakeX || SmallestShake < trail->shakeY)
        {
            PushCameraShakeAxes(&g_CameraShake, RowOf(&frame, PositionRow), trail->shakeX, trail->shakeY, trail->shakeX,
                                trail->shakeFalloff);
        }
        else
        {
            PushCameraShake(&g_CameraShake, RowOf(&frame, PositionRow), trail->shakeStrength, trail->shakeFalloff);
        }
    }

    if (trail->message != NoMessage && emits)
    {
        Reference* sender = instance != nullptr ? AddReference(instance) : nullptr;
        auto* memory = static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent)));
        GameEvent* event = GameEvent::Construct(memory, trail->message, &sender, ObjectNodeKinds);
        Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
        QueueEvent(instance, &handle);
    }

    return result;
}

void LeaveTrailDecals(const TrailArguments* trail, const Matrix4x4* frame, ChunkData* chunk)
{
    if (trail->bits.decalCount == 0)
    {
        AddDecalFromDescriptor(frame, chunk);
        return;
    }

    // Each decal spread across x and z by up to the spacing either way (a third random number drawn for y goes unused)
    for (u32 index = 0; index < trail->bits.decalCount; index++)
    {
        Matrix4x4 spread = *frame;
        Vector4 offset = {0.0f, 0.0f, 0.0f, 1.0f};
        f32 range = trail->spacing;
        f32 x = RandomSigned() * range;
        RandomSigned();
        f32 z = RandomSigned() * range;
        offset.x = offset.x + x;
        offset.y = offset.y + 0.0f;
        offset.z = offset.z + z;
        spread.m[PositionRow][0] = spread.m[PositionRow][0] + offset.x;
        spread.m[PositionRow][1] = spread.m[PositionRow][1] + offset.y;
        spread.m[PositionRow][2] = spread.m[PositionRow][2] + offset.z;
        AddDecalFromDescriptor(&spread, chunk);
    }
}

u32 TrailAnimationRestarted(const TrailArguments* trail, TimeClock*, ObjectNode* node)
{
    s32* time = node->particleTrails->time;
    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    if (model->animator == nullptr)
    {
        return 0;
    }

    if (!(GetAnimationProgress(model->animator, OgiAnimator::RootJoint) < trail->progress))
    {
        *time = 0;
        return 0;
    }

    // Once until the animation is past the mark again
    if (*time != 0)
    {
        return 0;
    }

    *time = 1;
    return 1;
}
