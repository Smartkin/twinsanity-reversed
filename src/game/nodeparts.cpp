#include "game/objectnode.h"

#include "game/clock.h"
#include "game/instanceparticles.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/nodecontrollers.h"
#include "game/particles.h"

// The object node's parts' frames that aren't the trajectory controller's: its perception's senses stepped, and its particle
// trails' emitters (the ones pickups and projectiles keep as well) given their frames

void StepPerception(void* perceptionPart, TimeClock* clock, ObjectNode* node)
{
    auto* perception = static_cast<Perception*>(perceptionPart);
    for (u32 index = 0; index < perception->bits.count; index++)
    {
        if (perception->on[index] == 0)
        {
            continue;
        }

        PerceptionSense* sense = perception->senses[index];
        u32 time = clock->time;
        f32 elapsed = static_cast<f32>(static_cast<s32>(time - perception->times[index])) * g_SecondsPerClockUnit;
        if (!(sense->interval <= elapsed))
        {
            continue;
        }

        perception->times[index] = time;
        switch (sense->bits.kind)
        {
        case PerceptionSense::KindRising:
            SenseRising(sense, clock, node, &perception->levels[index]);
            break;
        case PerceptionSense::KindInstances:
            SenseInstances(sense, clock, node, &perception->levels[index], &perception->direction);
            break;
        case PerceptionSense::KindSpeed:
            SenseSpeed(sense, clock, node, &perception->levels[index]);
            break;
        default:
            break;
        }
    }
}

void UpdateParticleTrails(ParticleTrails* trails, void* node)
{
    auto* owner = static_cast<ObjectNode*>(node);
    for (u32 index = 0; index < trails->bits.count; index++)
    {
        TrailArguments* trail = trails->trails[index];
        s32 emitter = trails->emitters[index];
        if (trail->system == NoParticleSystem)
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
            TrailBits bits = trail->bits;
            const Vector4* offset = bits.offsetGiven ? reinterpret_cast<const Vector4*>(trail->offset) : nullptr;
            u32 axes = bits.axes;
            OrientParticleFrame(bits.turned, axes, offset, &frame);
            if (trail->bits.gravityFrame)
            {
                emitter = SetEmitterGravityFrame(emitter, &frame);
            }
            else
            {
                emitter = SetEmitterFrame(emitter, &frame);
            }
        }

        trails->emitters[index] = emitter;
    }
}
