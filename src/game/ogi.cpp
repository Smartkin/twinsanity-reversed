#include "game/animation.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/lights.h"
#include "game/math.h"
#include "game/place.h"
#include "platform/graphics.h"
#include "platform/math.h"

namespace
{
Platform::Graphics::ModelLights LightsAt(const ChunkLights* lights)
{
    return {lights->directions, lights->colours, &lights->ambient};
}
}

void DrawOgi(GameOGI* ogi, const Matrix4x4* matrix, JointMatrices* joints, const BlendShapeWeights* blendShapes,
             const ChunkLights* lights, u32 mode)
{
    constexpr s32 MostShapes = 3;
    Matrix4x4* matrices = joints->start;
    if (matrices != nullptr && joints->frame + 1 < g_RenderedFrames)
    {
        matrices = nullptr;
    }

    if (matrices == nullptr)
    {
        return;
    }

    Platform::Graphics::ModelLights modelLights = LightsAt(lights);
    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        Matrix4x4 placed;
        VuMultiplyMatrices(&matrices[ogi->rigidModelJoints[index]], matrix, &placed);
        Platform::Graphics::DrawRigidModel(ogi->rigidModels[index], &placed, modelLights, mode);
    }

    if (ogi->hasSkin == 0 && ogi->hasBlendSkin == 0)
    {
        return;
    }

    for (u32 index = 0; index < ogi->jointCount; index++)
    {
        Platform::Math::MultiplyByParent(&ogi->jointMatrices[index], &matrices[index], &matrices[index]);
    }

    if (ogi->hasSkin != 0)
    {
        Platform::Graphics::DrawSkin(ogi->skin, matrices, ogi->jointCount, matrix, modelLights, mode);
    }

    if (ogi->hasBlendSkin == 0)
    {
        return;
    }

    s32 shapes[MostShapes] = {-1, -1, -1};
    f32 weights[MostShapes];
    s32 shapeCount = 0;
    for (u32 shape = 0; shape < blendShapes->count; shape++)
    {
        f32 weight = blendShapes->next[shape];
        if (weight != 0.0f && shapeCount < MostShapes)
        {
            shapes[shapeCount] = shape;
            weights[shapeCount] = weight;
            shapeCount++;
        }
    }

    Platform::Graphics::DrawBlendSkin(ogi->blendSkin, matrices, ogi->jointCount, matrix, modelLights, weights, shapes,
                                      shapeCount, mode);
}

void DrawOgiRigidModels(GameOGI* ogi, const Matrix4x4* matrix, const ChunkLights* lights, u32 mode)
{
    Platform::Graphics::ModelLights modelLights = LightsAt(lights);
    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        Platform::Graphics::DrawRigidModel(ogi->rigidModels[index], matrix, modelLights, mode);
    }
}

void ModelNode::Draw(const Matrix4x4* matrix, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode)
{
    if (drawnOgi != nullptr)
    {
        GatherStrongestLights(lights, chunkMatrix, lighting);
        if (drawnAnimator == nullptr)
        {
            DrawOgiRigidModels(drawnOgi, matrix, lights, mode);
        }
        else
        {
            DrawOgi(drawnOgi, matrix, &drawnAnimator->matrices, &drawnAnimator->blendShapes, lights, mode);
        }

        drawnOgi = nullptr;
    }

    drawnAnimator = nullptr;
}

void DrawInstanceModel(InstanceContext* instance, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode)
{
    Matrix4x4 matrix = instance->place->matrix;
    Matrix4x4 placed;
    Platform::Math::MultiplyByParent(&matrix, chunkMatrix, &placed);
    auto* node = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    if (lights != nullptr)
    {
        lights->position = *RowOf(&matrix, 3);
    }

    node->Draw(&placed, chunkMatrix, lights, mode);
}

void DrawQueuedInstances()
{
    constexpr u32 StandardPrograms = 1;
    constexpr u32 WhollyInView = 1;
    constexpr u32 Clipped = 2;
    constexpr s32 LastSlot = 0x3FF;
    if (g_DrawnInstanceCount == 0)
    {
        return;
    }

    Platform::Graphics::UseHelperPrograms(StandardPrograms, true);
    // The chunk's matrix is kept while its instances follow each other (the first instance always has a chunk)
    ChunkData* chunk = nullptr;
    ChunkLights* lights = nullptr;
    Matrix4x4 chunkMatrix;
    auto draw = [&](InstanceContext* instance, u32 mode) {
        if (instance->chunk != chunk)
        {
            g_DrawnChunk = instance->chunk;
            chunk = instance->chunk;
            lights = chunk->lights;
            chunkMatrix = chunk->drawMatrix;
        }

        DrawInstanceModel(instance, &chunkMatrix, lights, mode);
    };

    for (s32 index = 0; index < g_DrawnInViewCount; index++)
    {
        draw(g_DrawnInstances[index], WhollyInView);
    }

    for (s32 index = LastSlot; g_DrawnClippedEnd < index; index--)
    {
        draw(g_DrawnInstances[index], Clipped);
    }
}
