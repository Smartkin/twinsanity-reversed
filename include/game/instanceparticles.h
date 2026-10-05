#pragma once

#include "common.h"
#include "game/math.h"

#include "abi.h"

struct InstanceContext;

// The particles instances start (the object nodes' trails, the scripts' DoParticle, the surfaces' contacts): emitters of the
// particle systems' table (a system's index, played as entry index + 1) made in an instance's chunk, and the frames they leave
// from

// How a particle frame's rows are put back (its x, y and z rows after): as they are (any other mode too), y negated, y and z
// swapped with the new z negated, x negated with y and z swapped and the new y negated, x and y swapped and both negated, x and y
// swapped
enum ParticleFrameAxes : u32
{
    AxesAsAre,
    AxesYNegated,
    AxesYZSwapped,
    AxesXNegatedYZSwapped,
    AxesXYSwappedNegated,
    AxesXYSwapped,
};

// The exit point particles take for none (6 bits): they leave from the instance's place; and an emitter of none
constexpr u32 NoParticleExitPoint = 0x3F;
constexpr s32 NoEmitter = -1;

extern "C"
{
    // A frame made for particles: the frame's rows (not turned: the world's axes) put back by an axes mode (ParticleFrameAxes: 1
    // x, -y, z; 2 x, z, -y; 3 -x, -z, y; 4 -y, -x, z; 5 y, x, z) and its position moved by an offset along its own rows
    void OrientParticleFrame(u32 turned, u32 axes, const Vector4* offset, Matrix4x4* frame) RETAIL(FUN_0021b140);
    // The frame particles leave an instance from: its model's exit point's (NoParticleExitPoint: none), else its place's
    const Matrix4x4* ParticleFrame(InstanceContext* instance, u32 exitPoint) RETAIL(FUN_00223a28);
    // An emitter of a system made in the instance's chunk at a position (the origin without one), running with a value when given
    // one: the emitter (-1 for none); kept where it was made, or following its emission frame
    s32 StartEmitterKeepingTranslation(InstanceContext* instance, s32 system, u32 value, const Vector4* position)
        RETAIL(FUN_00223ad0);
    s32 StartEmitter(InstanceContext* instance, s32 system, u32 value, const Vector4* position) RETAIL(FUN_00223b90);
    // An emitter (none when it's negative) stopped and freed, given its emission's or its gravity's frame. Each returns the emitter
    // (stopped: what it's left as)
    s32 StopEmitter(s32 emitter) RETAIL(FUN_00223c20);
    s32 SetEmitterFrame(s32 emitter, const Matrix4x4* frame) RETAIL(FUN_00223c48);
    s32 SetEmitterGravityFrame(s32 emitter, const Matrix4x4* frame) RETAIL(FUN_00223c78);
    // With a chance, a decal of the chunk's at a position moved up to 0.4 across the ground
    void ScatterDecal(InstanceContext* instance, u32 unused, const Vector4* position, f32 chance) RETAIL_N32(FUN_00223940);
}
