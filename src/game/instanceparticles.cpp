#include "game/instanceparticles.h"

#include "game/animation.h"
#include "game/chunkdata.h"
#include "game/decals.h"
#include "game/instances.h"
#include "game/particles.h"
#include "game/place.h"

namespace
{
void NegateAxis(Vector4* axis)
{
    axis->x = -axis->x;
    axis->y = -axis->y;
    axis->z = -axis->z;
}
}

void OrientParticleFrame(u32 turned, u32 axes, const Vector4* offset, Matrix4x4* frame)
{
    Vector4 x;
    Vector4 y;
    Vector4 z;
    if (turned != 0)
    {
        x = *RowOf(frame, 0);
        y = *RowOf(frame, 1);
        z = *RowOf(frame, 2);
    }
    else
    {
        x = {1.0f, 0.0f, 0.0f, 1.0f};
        y = {0.0f, 1.0f, 0.0f, 1.0f};
        z = {0.0f, 0.0f, 1.0f, 1.0f};
    }

    Vector4 position = *RowOf(frame, 3);
    if (offset != nullptr)
    {
        const Vector4* rowX = RowOf(frame, 0);
        const Vector4* rowY = RowOf(frame, 1);
        const Vector4* rowZ = RowOf(frame, 2);
        position.x = position.x + ((rowX->x * offset->x + rowY->x * offset->y) + rowZ->x * offset->z);
        position.y = position.y + ((rowX->y * offset->x + rowY->y * offset->y) + rowZ->y * offset->z);
        position.z = position.z + ((rowX->z * offset->x + rowY->z * offset->y) + rowZ->z * offset->z);
    }

    switch (axes)
    {
    case 1:
        NegateAxis(&y);
        MatrixFromAxes(frame, &x, &y, &z);
        break;
    case 2:
        NegateAxis(&y);
        MatrixFromAxes(frame, &x, &z, &y);
        break;
    case 3:
        NegateAxis(&x);
        NegateAxis(&z);
        MatrixFromAxes(frame, &x, &z, &y);
        break;
    case 4:
        NegateAxis(&y);
        NegateAxis(&x);
        MatrixFromAxes(frame, &y, &x, &z);
        break;
    case 5:
        MatrixFromAxes(frame, &y, &x, &z);
        break;
    default:
        MatrixFromAxes(frame, &x, &y, &z);
        break;
    }

    *RowOf(frame, 3) = position;
}

const Matrix4x4* ParticleFrame(InstanceContext* instance, u32 exitPoint)
{
    constexpr u32 NoExitPoint = 0x3F;
    const Matrix4x4* frame = nullptr;
    if (exitPoint != NoExitPoint)
    {
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
        if (model->animator != nullptr)
        {
            SizedArray<ExitPointAnimation*>* exitPoints = model->animator->exitPoints;
            ExitPointAnimation* point = exitPoints != nullptr ? exitPoints->data[exitPoint & 0xFF] : nullptr;
            if (point != nullptr)
            {
                frame = &UpdateExitPointMatrix(point)->matrix;
            }
        }
    }

    if (frame == nullptr)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        frame = &place->matrix;
    }

    return frame;
}

s32 StartEmitter(InstanceContext* instance, s32 system, u32 value, const Vector4* position)
{
    s32 emitter = -1;
    f32 at[3];
    if (position == nullptr)
    {
        at[0] = 0.0f;
        at[1] = 0.0f;
        at[2] = 0.0f;
    }
    else
    {
        at[0] = position->x;
        at[1] = position->y;
        at[2] = position->z;
    }

    if (value == 0)
    {
        CreateParticleEmitter(at[0], at[1], at[2], &emitter, system + 1, instance->chunk);
    }
    else
    {
        StartParticleEmitter(&emitter, system + 1, at, value, instance->chunk);
    }

    return emitter;
}

s32 StartEmitterKeepingTranslation(InstanceContext* instance, s32 system, u32 value, const Vector4* position)
{
    s32 emitter = StartEmitter(instance, system, value, position);
    if (emitter != -1)
    {
        KeepEmitterTranslation(emitter);
    }

    return emitter;
}

s32 StopEmitter(s32 emitter)
{
    if (emitter >= 0)
    {
        FreeParticleEmitterBlocks(&emitter);
    }

    return emitter;
}

s32 SetEmitterFrame(s32 emitter, const Matrix4x4* frame)
{
    if (emitter >= 0)
    {
        SetEmitterEmission(emitter, frame);
    }

    return emitter;
}

s32 SetEmitterGravityFrame(s32 emitter, const Matrix4x4* frame)
{
    if (emitter >= 0)
    {
        SetEmitterGravity(emitter, frame);
    }

    return emitter;
}

void ScatterDecal(InstanceContext* instance, u32, const Vector4* position, f32 chance)
{
    if (!(GetRandFloat() <= chance))
    {
        return;
    }

    Matrix4x4 frame;
    InitIdentityMatrix(&frame);
    *RowOf(&frame, 3) = *position;
    Vector4 jitter = {0.0f, 0.0f, 0.0f, 1.0f};
    JitterVector(Rounded(0.4), 0.0f, 1.0f, 1.0f, &jitter);
    Vector4* at = RowOf(&frame, 3);
    at->x = at->x + jitter.x;
    at->y = at->y + jitter.y;
    at->z = at->z + jitter.z;
    AddDecalFromDescriptor(&frame, instance->chunk);
}

EABI_EXPORT(FUN_00223940, ScatterDecal);
