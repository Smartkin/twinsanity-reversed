#include "renderer.h"

#include "game/disk.h"
#include "platform/graphics.h"

namespace
{
// A skin's data (quadwords): its matrix to the screen, its lights' directions (a column each) with their colours over the
// fourth row, and the ambient light; then, drawn clipped (DrawClipped), the clipped's matrix and clip vector, or the eye's matrix
constexpr u32 SkinDataSize = 0xB;
constexpr u32 ClipDataSize = 5;
constexpr u32 MatrixSize = 4;
// The VU1 programs a blend skin's parts call: the first shape's blend, the next shapes', the part's (an odd count of shapes runs
// EndProgram first), and without shapes program 6, EndProgram and the part's. The shapes' programs are the resident ones whose
// indexes g_BlendFirstShapeProgram, g_BlendNextShapeProgram and g_BlendNoShapesProgram hold
constexpr u32 FirstShapeProgram = 8;
constexpr u32 NextShapeProgram = 7;
constexpr u32 PartProgram = 1;
constexpr u32 NoShapesProgram = 6;

u32 Mscal(u32 program)
{
    return g_VuPrograms[program].address | VifMscal;
}

// The joints' matrices (a REF of them, VIF1 waiting for VU1 first), unless the bucket's last skin sent the same
u8* WriteJoints(RenderBucket& bucket, u8* packet, const Matrix4x4* joints, u32 jointCount)
{
    if (bucket.lastJoints == Address(joints))
    {
        return packet;
    }

    bucket.lastJoints = Address(joints);
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = jointCount * MatrixSize | ReferenceTag;
    at[1] = Address(joints);
    at[2] = VifFlushE;
    at[3] = VifUnpackTo(VifUnpackV4Count, g_VuJoints, jointCount * MatrixSize);
    return packet + 0x10;
}

u32* WriteUnpack(u8* packet, u32 count, u32 vif, u32 address)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | count;
    at[1] = 0;
    at[2] = vif;
    at[3] = VifUnpackTo(VifUnpackV4Count, address, count);
    return at + 4;
}

u8* WriteSkinData(u8* packet, u32 buffer, u32 vif, const Matrix4x4& toScreen, const Matrix4x4* lights, const Vector4* colours,
                  const Vector4* ambient)
{
    WriteUnpack(packet, SkinDataSize, vif, buffer + g_VuSkinData);
    *reinterpret_cast<Matrix4x4*>(packet + 0x10) = toScreen;
    VuTranspose(lights, reinterpret_cast<Matrix4x4*>(packet + 0x50));
    auto* light = reinterpret_cast<Vector4*>(packet + 0x80);
    light[0] = colours[0];
    light[1] = colours[1];
    light[2] = colours[2];
    light[3] = *ambient;
    return packet + 0xC0;
}

u8* WriteClipData(u8* packet, u32 buffer, const Matrix4x4& toCamera)
{
    WriteUnpack(packet, ClipDataSize, 0, buffer + g_VuSkinData + g_VuSkinClip);
    *reinterpret_cast<Matrix4x4*>(packet + 0x10) = toCamera;
    *reinterpret_cast<Vector4*>(packet + 0x50) = g_RenderView->clip;
    return packet + 0x60;
}

// Every shader is asked
bool NeedsEye(const Material* material)
{
    bool needs = false;
    for (u32 i = 0; i < material->shaderCount; i++)
    {
        Shader* shader = material->shaders[i];
        needs |= CallVirtual<u32>(shader, shader->vtable, ShaderNeedsEyeSlot) != 0;
    }

    return needs;
}
}

extern "C"
{
    void SetSkinDMA(Skin* skin, const Matrix4x4* joints, u32 jointCount, u32 mode)
    {
        Matrix4x4 toScreen;
        Matrix4x4 toCamera;
        VuMultiplyMatrices(g_SkinMatrix, &g_RenderView->toScreen, &toScreen);
        if (mode == DrawClipped)
        {
            VuMultiplyMatrices(g_SkinMatrix, &g_RenderView->toClip, &toCamera);
        }

        for (u32 i = 0; i < skin->count; i++)
        {
            Material* material = skin->materials[i]->material;
            RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
            auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
            ListMaterial(material);
            material->drawnDirectly = 1;
            packet = WriteJoints(bucket, packet, joints, jointCount);
            // The buffer RenderMaterial's turn is about to give the material
            u32 buffer = bucket.vuBuffer != 0 ? g_VuBuffer1 : g_VuBuffer2;
            packet = WriteSkinData(packet, buffer, 0, toScreen, g_SkinLights, g_SkinLightColours, g_SkinAmbient);
            if (mode == DrawClipped)
            {
                packet = WriteClipData(packet, buffer, toCamera);
            }

            if (NeedsEye(material))
            {
                if (mode != DrawClipped)
                {
                    VuMultiplyMatrices(g_SkinMatrix, &g_RenderView->toClip, &toCamera);
                }

                packet = WriteSkinEyeData(skin, packet, &toCamera, buffer);
            }

            packet = RenderMaterial(material, packet, mode);
            u8* subModel = DiskLoadedMemory(GetDiskManager(), &skin->subModels[i]);
            auto* at = reinterpret_cast<u32*>(packet);
            at[0] = CallTag;
            at[1] = Address(subModel);
            at[2] = 0;
            at[3] = 0;
            EndPacket(bucket, packet + 0x10);
            material->drawnDirectly = 1;
        }
    }

    u8* WriteSkinEyeData(const Skin*, u8* packet, const Matrix4x4* toCamera, u32 buffer)
    {
        WriteUnpack(packet, MatrixSize, 0, buffer + g_VuSkinData + g_VuSkinClip);
        *reinterpret_cast<Matrix4x4*>(packet + 0x10) = *toCamera;
        WriteUnpack(packet + 0x50, 1, 0, buffer + g_VuSkinData + g_VuSkinEye);
        // Where the camera is, in the skin's space
        Vector4 eye = CameraPosition(g_RenderView);
        VuTransformPoint(g_SkinInverse, &eye, &eye);
        *reinterpret_cast<Vector4*>(packet + 0x60) = eye;
        return packet + 0x70;
    }

    void SetBlendSkinDMA(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const f32* weights, const s32* shapes,
                         const s32* shapeCount, u32 mode)
    {
        Matrix4x4 toScreen;
        Matrix4x4 toCamera;
        VuMultiplyMatrices(g_BlendSkinMatrix, &g_RenderView->toScreen, &toScreen);
        if (mode == DrawClipped)
        {
            VuMultiplyMatrices(g_BlendSkinMatrix, &g_RenderView->toClip, &toCamera);
        }

        for (u32 i = 0; i < skin->count; i++)
        {
            Material* material = skin->materials[i]->material;
            RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
            auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
            ListMaterial(material);
            material->drawnDirectly = 1;
            packet = WriteJoints(bucket, packet, joints, jointCount);
            u32 buffer = g_VuBuffer2;
            packet = WriteSkinData(packet, buffer, VifFlushE, toScreen, g_BlendSkinLights, g_BlendSkinLightColours,
                                   g_BlendSkinAmbient);
            if (mode == DrawClipped)
            {
                packet = WriteClipData(packet, buffer, toCamera);
            }

            // VU1 works the eye out of the inverse
            if (NeedsEye(material))
            {
                WriteUnpack(packet, MatrixSize, 0, buffer + g_VuSkinData + g_VuBlendInverse);
                *reinterpret_cast<Matrix4x4*>(packet + 0x10) = *g_BlendSkinInverse;
                packet += 0x50;
            }

            packet = RenderBlendSkinMaterial(material, packet, &buffer, mode);
            packet = WriteBlendSubModel(skin->subModels[i], packet, weights, shapes, shapeCount);
            EndPacket(bucket, packet);
            material->drawnDirectly = 1;
        }
    }

    u8* WriteBlendSubModel(const BlendSubModel* subModel, u8* packet, const f32* weights, const s32* shapes,
                           const s32* shapeCount)
    {
        f32 factors[4];
        const f32* first = subModel->parts[0]->shapeFactors;
        factors[0] = first[0];
        factors[1] = first[1];
        factors[2] = first[2];
        factors[3] = 0.0f;
        for (u32 i = 0; i < subModel->count; i++)
        {
            packet = WriteBlendPart(subModel->parts[i], packet, weights, shapes, shapeCount, factors);
        }

        return packet;
    }

    u8* WriteBlendPart(const BlendPart* part, u8* packet, const f32* weights, const s32* shapes, const s32* shapeCount,
                       const f32* factors)
    {
        // The part's vertexes
        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CallTag;
        at[1] = part->packet;
        at[2] = 0;
        at[3] = 0;
        at += 4;
        if (*shapeCount == 0)
        {
            at[0] = CountTag | 1;
            at[1] = 0;
            at[2] = Mscal(NoShapesProgram);
            at[3] = Mscal(EndProgram);
            at[4] = Mscal(PartProgram);
            at[5] = VifCycle1;
            at[6] = 0;
            at[7] = 0;
            return reinterpret_cast<u8*>(at + 8);
        }

        // VU1 learns the blend buffers, then each shape goes into one (the other than the last one's): the factors with its
        // weight, its offsets, and the blend
        u32 first = g_VuBlendBuffer1;
        u32 second = g_VuBlendBuffer2;
        at[0] = CountTag | 1;
        at[1] = 0;
        at[2] = VifCycle1;
        at[3] = g_VuBlendBuffersPlace | VifUnpackV4;
        at[4] = first;
        at[5] = second;
        at[6] = first;
        at[7] = second;
        at += 8;
        u32 turns = first + second;
        u32 buffer = first;
        s32 count = *shapeCount;
        for (s32 i = 0; i < *shapeCount; i++)
        {
            u32 shape = static_cast<u32>(shapes[i]);
            at[0] = CountTag | 1;
            at[1] = 0;
            at[2] = VifCycle1;
            at[3] = buffer | VifUnpackV4;
            auto* values = reinterpret_cast<f32*>(at + 4);
            values[0] = factors[0];
            values[1] = factors[1];
            values[2] = factors[2];
            values[3] = weights[i];
            at += 8;
            at[0] = part->shapeSizes[shape] | ReferenceTag;
            at[1] = part->shapeAddresses[shape];
            at[2] = 0;
            at[3] = VifUnpackTo(VifUnpackV4Bytes, buffer + 1, part->shapeCounts[shape]);
            at += 4;
            at[0] = CountTag;
            at[1] = 0;
            at[2] = Mscal(i != 0 ? NextShapeProgram : FirstShapeProgram);
            at[3] = VifCycle1;
            at += 4;
            buffer = turns - buffer;
        }

        at[0] = CountTag;
        at[1] = 0;
        if (((count + 1) & 1) != 0)
        {
            at[2] = 0;
            at[3] = Mscal(PartProgram);
        }
        else
        {
            at[2] = Mscal(EndProgram);
            at[3] = Mscal(PartProgram);
        }

        return reinterpret_cast<u8*>(at + 4);
    }
}

namespace
{
// What VU1 lights a skin in: its rotation turned back (taking the world into the skin's space) and the lights' directions turned
// into that space, a row each. The fourth row isn't written: like a rigid model's, the lights' rows get the stack's leftovers as
// their fourth words (WriteSkinData's transpose)
struct SkinSpace
{
    Matrix4x4 inverse;
    Matrix4x4 lights;
};

void MakeSkinSpace(const Matrix4x4* matrix, const Vector4* directions, SkinSpace* space)
{
    VuTransposeRotation(matrix, &space->inverse);
    for (u32 row = 0; row < 3; row++)
    {
        VuRotateVector(&space->inverse, &directions[row], reinterpret_cast<Vector4*>(space->lights.m[row]));
    }
}
}

void Platform::Graphics::DrawSkin(Skin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix,
                                  const ModelLights& lights, u32 mode)
{
    SkinSpace space;
    MakeSkinSpace(matrix, lights.directions, &space);
    g_SkinMatrix = matrix;
    g_SkinInverse = &space.inverse;
    g_SkinLights = &space.lights;
    g_SkinLightColours = lights.colours;
    g_SkinAmbient = lights.ambient;
    SetSkinDMA(skin, joints, jointCount, mode);
}

void Platform::Graphics::DrawBlendSkin(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix,
                                       const ModelLights& lights, const f32* weights, const s32* shapes, s32 shapeCount,
                                       u32 mode)
{
    SkinSpace space;
    MakeSkinSpace(matrix, lights.directions, &space);
    g_BlendSkinMatrix = matrix;
    g_BlendSkinLights = &space.lights;
    g_BlendSkinLightColours = lights.colours;
    g_BlendSkinAmbient = lights.ambient;
    g_BlendSkinInverse = &space.inverse;
    SetBlendSkinDMA(skin, joints, jointCount, weights, shapes, &shapeCount, mode);
}
