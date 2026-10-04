#include "renderer.h"

#include "game/disk.h"
#include "platform/graphics.h"

namespace
{
// The sky is drawn into a buffer of half the screen's size in the first bucket, then drawn over the screen from it two pixels for
// one in strips of 32 pixels
constexpr u32 SkyBucket = 0;
constexpr s32 StripShift = 5;

u64 Zeroed(u32 value)
{
    return value;
}

struct PairWriter
{
    u64* at;

    void Write(u64 data, u64 address)
    {
        at[0] = data;
        at[1] = address;
        at += 2;
    }
};

// A sky model's data before its material's (VU1's program 10 takes it from 78 quadwords past the buffer's base, which only the sky
// writes): its matrices to the half-size screen and to camera space, and the view's clip vector
u8* WriteSkyMatrices(u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera, u32 buffer)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | 9;
    at[1] = 0;
    at[2] = 0;
    at[3] = (buffer + g_VuSkyData) | 9 << 16 | VifUnpackV4Count;
    auto* matrices = reinterpret_cast<Matrix4x4*>(packet + 0x10);
    matrices[0] = *toScreen;
    matrices[1] = *toCamera;
    *reinterpret_cast<Vector4*>(packet + 0x90) = g_RenderView->clip;
    return packet + 0xA0;
}
}

extern "C"
{
    void HalfSizeScreenMatrix(const RenderTargetDescription* target, Matrix4x4* matrix)
    {
        f32 x = 2048.0f - static_cast<f32>(target->displayWidth >> 2);
        f32 y = 2048.0f - static_cast<f32>(target->displayHeight >> 2);
        f32 halfWidth = static_cast<f32>(target->width >> 1);
        f32 halfHeight = static_cast<f32>(target->height >> 1);
        x = x + static_cast<f32>(target->offsetX);
        y = y + static_cast<f32>(target->offsetY);
        InitIdentityMatrix(matrix);
        matrix->m[0][0] = halfWidth * 8.0f;
        matrix->m[1][1] = -halfHeight * 8.0f;
        matrix->m[2][2] = -8388607.0f;
        matrix->m[3][0] = (x + halfWidth * 0.5f) * 16.0f;
        matrix->m[3][1] = (y + halfHeight * 0.5f) * 16.0f;
        matrix->m[3][2] = 8388607.0f;
    }

    void DrawSky(Sky* sky, RenderView* view)
    {
        StartSkyBuffer(sky);
        const Matrix4x4* toCamera = &g_RenderView->toClip;
        Matrix4x4 translation;
        InitIdentityMatrix(&translation);
        // The camera's node takes its position when it moved (bit 2)
        Reference* reference = view->cameraObject;
        u8* camera = reference != nullptr ? reinterpret_cast<u8*>(reference->object) : nullptr;
        u8* node = *reinterpret_cast<u8**>(camera + 8);
        auto& flags = *reinterpret_cast<u64*>(node + 0x60);
        if ((flags & 4) != 0)
        {
            const auto* from = reinterpret_cast<const f32*>(node + 0x30);
            auto* to = reinterpret_cast<f32*>(node + 0x40);
            to[0] = from[0];
            to[3] = from[3];
            to[1] = from[1];
            to[2] = from[2];
            flags = flags & ~1ull & ~4ull;
        }

        *reinterpret_cast<Vector4*>(translation.m[3]) = *reinterpret_cast<const Vector4*>(node + 0x40);
        Matrix4x4 toScreen;
        Matrix4x4 skyToCamera;
        VuMultiplyMatrices(&translation, &sky->toScreen, &toScreen);
        VuMultiplyMatrices(&translation, toCamera, &skyToCamera);
        for (u32 i = 0; i < sky->count; i++)
        {
            DrawSkyModel(sky->models[i], view, &toScreen, &skyToCamera);
        }

        FinishSky(sky);
    }

    void StartSkyBuffer(Sky* sky)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[SkyBucket];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        const Matrix4x4* toCamera = &g_RenderView->toClip;
        u32* tag = BeginPacket(bucket);
        Matrix4x4 projection;
        HalfSizeScreenMatrix(g_RenderTarget, &projection);
        VuMultiplyMatrices(toCamera, &projection, &sky->toScreen);

        // The half-size buffer (past ten screens of pixels in GS memory), its depth buffer's writes off, the screen's middle at
        // its middle, its scissor
        u32 pixels = width * height;
        u64 frame = Zeroed(pixels * 10 >> 13) | static_cast<u64>(width >> 7) << 16;
        u64 depth = Zeroed(pixels >> 12 | 0x30000000) | 1ull << 32;
        u64 offset = Zeroed((0x800 - (width >> 2)) << 4) | static_cast<u64>((0x800 - (height >> 2)) << 4) << 32;
        u64 scissor = static_cast<u64>((width >> 1) - 1) << 16 | static_cast<u64>((height >> 1) - 1) << 48;
        tag[0] = CountTag | 6;
        tag[1] = 0;
        tag[2] = VifFlushE;
        tag[3] = VifDirect | 6;
        PairWriter pairs{reinterpret_cast<u64*>(tag + 4)};
        pairs.Write(0x8000ull << 45 | 0x8005, 0xE);
        pairs.Write(frame, 0x4C);
        pairs.Write(depth, 0x4E);
        pairs.Write(offset, 0x18);
        pairs.Write(scissor, 0x40);
        pairs.Write(0, 0x45);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    void DrawSkyModel(RigidModel* model, RenderView* view, const Matrix4x4* toScreen, const Matrix4x4* toCamera)
    {
        constexpr u32 SkyMode = 2;
        u32 count = model->model->subModelCount < model->count ? model->model->subModelCount : model->count;
        Reference* reference = view->cameraObject;
        if ((reference != nullptr ? reference->object : nullptr) == nullptr)
        {
            return;
        }

        for (u32 i = 0; i < count; i++)
        {
            Material* material = model->materials[i]->material;
            RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
            auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
            ListMaterial(material);
            material->drawnDirectly = 1;
            u32 buffer = bucket.vuBuffer != 0 ? g_VuBuffer1 : g_VuBuffer2;
            packet = WriteSkyMatrices(packet, toScreen, toCamera, buffer);
            packet = RenderSkyMaterial(material, packet, SkyMode);
            s32 handle = model->model->subModels[i];
            u8* subModel = DiskLoadedMemory(GetDiskManager(), &handle);
            auto* at = reinterpret_cast<u32*>(packet);
            at[0] = CallTag;
            at[1] = Address(subModel);
            at[2] = 0;
            at[3] = 0;
            EndPacket(bucket, packet + 0x10);
            material->drawnDirectly = 1;
        }
    }

    void FinishSky(Sky*)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[SkyBucket];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        u32 strips = width >> StripShift;
        u32 pairCount = (strips << 2) + 0x11;
        u32* tag = BeginPacket(bucket);
        tag[0] = CountTag | (pairCount + 1);
        tag[1] = 0;
        tag[2] = VifFlushE;
        tag[3] = VifDirect | (pairCount + 1);

        // The screen drawn again with its depth written, the half-size buffer (bilinear off) as its texture
        u32 pixels = width * height;
        u64 frame = Zeroed(pixels >> 12) | static_cast<u64>(width >> 6) << 16;
        u64 depth = Zeroed(pixels * 6 >> 13 | 0x30000000);
        u64 texture = Zeroed(pixels * 10 >> 8) | static_cast<u64>(width >> 7) << 14 | 0x6A8000000;
        PairWriter pairs{reinterpret_cast<u64*>(tag + 4)};
        pairs.Write(Zeroed(pairCount | 0x8000) | 0x8000ull << 45, 0xE);
        pairs.Write(0, 0x3F);
        pairs.Write(0, 0x45);
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x19);
        pairs.Write(frame, 0x4D);
        pairs.Write(depth, 0x4F);
        pairs.Write(0, 0x09);
        pairs.Write(1, 0x46);
        pairs.Write(0, 0x4B);
        pairs.Write(0x80000000, 0x3B);
        pairs.Write(0x400000004000000, 0x41);
        pairs.Write(texture, 0x07);
        pairs.Write(0, 0x15);
        pairs.Write(0x30000, 0x48);
        pairs.Write(0x316, 0x00);
        pairs.Write(0x3F80000000808080, 0x01);
        u64 bottomV = static_cast<u64>(height << 3) << 16;
        u64 bottomY = static_cast<u64>(height << 4) << 16;
        for (u32 strip = 0; strip < strips; strip++)
        {
            pairs.Write(Zeroed(strip << 8), 0x03);
            pairs.Write(Zeroed(strip << 9), 0x04);
            pairs.Write(Zeroed(0x100 + strip * 0x100) | bottomV, 0x03);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | bottomY, 0x04);
        }

        pairs.Write(0, 0x1A);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    u8* FUN_001c1f90(const RigidModel*, u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera, u32 buffer)
    {
        return WriteSkyMatrices(packet, toScreen, toCamera, buffer);
    }
}

void Platform::Graphics::DrawChunkSky(Sky* sky, RenderView* view)
{
    DrawSky(sky, view);
}
