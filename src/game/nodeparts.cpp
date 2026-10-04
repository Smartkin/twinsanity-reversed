#include "game/objectnode.h"

#include "game/clock.h"
#include "game/instanceparticles.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/nodecontrollers.h"

// The object node's parts' frames that aren't the trajectory controller's: its perception's senses stepped, and its particle
// trails' emitters (the ones pickups and projectiles keep as well) given their frames

namespace
{
// What a trail's bits say (the 64 bits it starts with): its frame keeps the world's axes (bit 18), the axes mode its frame is put
// in (bits 19-22), it leaves from an offset (bit 37) and its frame is its emitter's gravity's (bit 53)
constexpr u32 TrailUnturnedShift = 18;
constexpr u32 TrailAxesShift = 19;
constexpr u32 TrailAxesMask = 0xF;
constexpr u64 TrailOffset = u64{1} << 37;
constexpr u64 TrailGravityFrame = u64{1} << 53;
constexpr u16 NoSystem = 0xFFFF;

// A trail's bits and its offset
u64 TrailBits(const TrailArguments* trail)
{
    return *reinterpret_cast<const u64*>(trail);
}

const Vector4* TrailOffsetOf(const TrailArguments* trail)
{
    return reinterpret_cast<const Vector4*>(reinterpret_cast<const u8*>(trail) + 0x60);
}
}

void StepPerception(void* perceptionPart, TimeClock* clock, ObjectNode* node)
{
    auto* perception = static_cast<Perception*>(perceptionPart);
    for (u32 i = 0; i < (perception->bits & Perception::CountMask); i++)
    {
        if (perception->on[i] == 0)
        {
            continue;
        }

        PerceptionSense* sense = perception->senses[i];
        u32 time = clock->time;
        f32 elapsed = static_cast<f32>(static_cast<s32>(time - perception->times[i])) * g_SecondsPerClockUnit;
        if (!(sense->interval <= elapsed))
        {
            continue;
        }

        perception->times[i] = time;
        switch (sense->bits & PerceptionSense::KindMask)
        {
        case PerceptionSense::KindRising:
            SenseRising(sense, clock, node, &perception->levels[i]);
            break;
        case PerceptionSense::KindInstances:
            SenseInstances(sense, clock, node, &perception->levels[i], &perception->direction);
            break;
        case PerceptionSense::KindSpeed:
            SenseSpeed(sense, clock, node, &perception->levels[i]);
            break;
        default:
            break;
        }
    }
}

void UpdateParticleTrails(ParticleTrails* trails, void* node)
{
    auto* owner = static_cast<ObjectNode*>(node);
    for (u32 i = 0; i < (trails->bits & ParticleTrails::CountMask); i++)
    {
        TrailArguments* trail = trails->trails[i];
        s32 emitter = trails->emitters[i];
        if (trail->system == NoSystem)
        {
            emitter = StopTrail(trail, emitter);
        }
        else if (emitter < 0)
        {
            emitter = StartTrail(trail, trail->system, 1, owner);
        }

        if (emitter >= 0)
        {
            Matrix4x4 frame = *ParticleFrame(owner->owner, trail->exitPoint);
            u64 bits = TrailBits(trail);
            const Vector4* offset = (bits & TrailOffset) != 0 ? TrailOffsetOf(trail) : nullptr;
            OrientParticleFrame(bits >> TrailUnturnedShift & 1, bits >> TrailAxesShift & TrailAxesMask, offset, &frame);
            if ((TrailBits(trail) & TrailGravityFrame) != 0)
            {
                emitter = SetEmitterGravityFrame(emitter, &frame);
            }
            else
            {
                emitter = SetEmitterFrame(emitter, &frame);
            }
        }

        trails->emitters[i] = emitter;
    }
}
