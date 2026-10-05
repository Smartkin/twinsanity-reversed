#include "renderer.h"
#include "gsvalues.h"

#include "game/disk.h"
#include "platform/graphics.h"

#include <bit>
#include <libgs.h>

namespace
{
// The sky is drawn into a buffer of half the screen's size in the first bucket, then drawn over the screen from it two pixels for
// one in strips of 32 pixels
constexpr s32 StripShift = 5;
// A sky model's data before its material's: its matrices to the half-size screen and to camera space, and the view's clip vector
constexpr u32 SkyDataQuadwords = 9;

u64 Zeroed(u32 value)
{
    return value;
}

// A sky model's data (VU1's program 10 takes it from 78 quadwords past the buffer's base, which only the sky writes)
u8* WriteSkyMatrices(u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera, u32 buffer)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | SkyDataQuadwords;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, buffer + g_VuSkyData, SkyDataQuadwords);
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
        f32 x = GsScreenMiddleFloat - static_cast<f32>(target->displayWidth >> 2);
        f32 y = GsScreenMiddleFloat - static_cast<f32>(target->displayHeight >> 2);
        f32 halfWidth = static_cast<f32>(target->width >> 1);
        f32 halfHeight = static_cast<f32>(target->height >> 1);
        x = x + static_cast<f32>(target->offsetX);
        y = y + static_cast<f32>(target->offsetY);
        InitIdentityMatrix(matrix);
        matrix->m[0][0] = halfWidth * GsHalfSubpixels;
        matrix->m[1][1] = -halfHeight * GsHalfSubpixels;
        matrix->m[2][2] = -GsDepthRange;
        matrix->m[3][0] = (x + halfWidth * 0.5f) * GsSubpixels;
        matrix->m[3][1] = (y + halfHeight * 0.5f) * GsSubpixels;
        matrix->m[3][2] = GsDepthRange;
    }

    void DrawSky(Sky* sky, RenderView* view)
    {
        StartSkyBuffer(sky);
        const Matrix4x4* toCamera = &g_RenderView->toClip;
        Matrix4x4 translation;
        InitIdentityMatrix(&translation);
        // The camera's place takes its position from its matrix when the matrix moved
        ObjectPlace* place = CameraPlace(view);
        place->SyncPosition();
        *reinterpret_cast<Vector4*>(translation.m[3]) = place->position;
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

    // The half-size buffer (32 bits a pixel), the frame's memory its depth buffer (not written), the screen's middle at its
    // middle, its scissor, no dithering
    void StartSkyBuffer(Sky* sky)
    {
        constexpr u32 SetUpWrites = 5;
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketSky];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        const Matrix4x4* toCamera = &g_RenderView->toClip;
        u32* tag = BeginPacket(bucket);
        Matrix4x4 projection;
        HalfSizeScreenMatrix(g_RenderTarget, &projection);
        VuMultiplyMatrices(toCamera, &projection, &sky->toScreen);

        u32 pixels = width * height;
        u64 frame = Zeroed(pixels * HalfSizeBufferPixelOffset >> GsPageShift) |
                    static_cast<u64>(width >> (GsWidthShift + 1)) << GsFrameWidthShift;
        u64 depth = Zeroed(pixels >> FrameBufferPagesShift) | ZbufZ32Format | DepthNotWritten;
        u64 offset = Zeroed((GsScreenMiddle - (width >> 2)) << GsSubpixelShift) |
                     static_cast<u64>((GsScreenMiddle - (height >> 2)) << GsSubpixelShift) << GsOffsetYShift;
        u64 scissor = static_cast<u64>((width >> 1) - 1) << GsScissorRightShift |
                      static_cast<u64>((height >> 1) - 1) << GsScissorBottomShift;
        tag[0] = CountTag | (SetUpWrites + 1);
        tag[1] = 0;
        tag[2] = VifFlushE;
        tag[3] = VifDirect | (SetUpWrites + 1);
        GsWriter pairs{reinterpret_cast<GsWrite*>(tag + 4)};
        pairs.Write(AddressDataTag(SetUpWrites), GifAddressData);
        pairs.Write(frame, GsFrame);
        pairs.Write(depth, GsZbuf);
        pairs.Write(offset, GsXyOffset);
        pairs.Write(scissor, GsScissor);
        pairs.Write(0, GsDither);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    void DrawSkyModel(RigidModel* model, RenderView* view, const Matrix4x4* toScreen, const Matrix4x4* toCamera)
    {
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
            packet = RenderSkyMaterial(material, packet, DrawClipped);
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

    // The screen drawn again with its depth written, the half-size buffer (bilinear off) as its texture, a sprite of 32 pixels
    // and 16 texels at a time
    void FinishSky(Sky*)
    {
        // 17 settings and 4 a strip
        constexpr u32 SettingWrites = 0x11;
        constexpr u32 StripWrites = 4;
        constexpr u32 TexelStep = 0x100;
        constexpr u32 PixelStep = 0x200;
        const u64 HalfSizeTexture = std::bit_cast<u64>(GS_TEX0{.tex_width = 10, .tex_height = 10, .tex_cc = 1});
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketSky];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        u32 strips = width >> StripShift;
        u32 pairCount = strips * StripWrites + SettingWrites;
        u32* tag = BeginPacket(bucket);
        tag[0] = CountTag | (pairCount + 1);
        tag[1] = 0;
        tag[2] = VifFlushE;
        tag[3] = VifDirect | (pairCount + 1);

        u32 pixels = width * height;
        u64 frame = Zeroed(pixels >> FrameBufferPagesShift) | static_cast<u64>(width >> GsWidthShift) << GsFrameWidthShift;
        u64 depth = Zeroed(pixels * DepthBufferPixelOffset >> GsPageShift) | ZbufZ32Format;
        u64 texture = Zeroed(pixels * HalfSizeBufferPixelOffset >> GsBlockShift) |
                      static_cast<u64>(width >> (GsWidthShift + 1)) << GsTextureWidthShift | HalfSizeTexture;
        GsWriter pairs{reinterpret_cast<GsWrite*>(tag + 4)};
        pairs.Write(AddressDataTag(pairCount), GifAddressData);
        pairs.Write(0, GsTexFlush);
        pairs.Write(0, GsDither);
        pairs.Write(AttributesFromPrim, GsPrmodeCont);
        pairs.Write(0, SecondContext(GsXyOffset));
        pairs.Write(frame, SecondContext(GsFrame));
        pairs.Write(depth, SecondContext(GsZbuf));
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(ClampColours, GsColClamp);
        pairs.Write(0, SecondContext(GsFba));
        pairs.Write(SecondAlpha, GsTexa);
        pairs.Write(WholeScissor, SecondContext(GsScissor));
        pairs.Write(texture, SecondContext(GsTex0));
        pairs.Write(0, SecondContext(GsTex1));
        pairs.Write(DepthAlways, SecondContext(GsTest));
        pairs.Write(TexturedSprite, GsPrim);
        pairs.Write(GreyQOne, GsRgbaq);
        u64 bottomV = static_cast<u64>(height << (GsSubpixelShift - 1)) << GsUvVShift;
        u64 bottomY = static_cast<u64>(height << GsSubpixelShift) << GsXyzYShift;
        for (u32 strip = 0; strip < strips; strip++)
        {
            pairs.Write(Zeroed(strip * TexelStep), GsUv);
            pairs.Write(Zeroed(strip * PixelStep), GsXyzf2);
            pairs.Write(Zeroed(TexelStep + strip * TexelStep) | bottomV, GsUv);
            pairs.Write(Zeroed(PixelStep + strip * PixelStep) | bottomY, GsXyzf2);
        }

        pairs.Write(AttributesFromPrmode, GsPrmodeCont);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    u8* WriteSkyModelMatrices(const RigidModel*, u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera, u32 buffer)
    {
        return WriteSkyMatrices(packet, toScreen, toCamera, buffer);
    }
}

void Platform::Graphics::DrawChunkSky(Sky* sky, RenderView* view)
{
    DrawSky(sky, view);
}
