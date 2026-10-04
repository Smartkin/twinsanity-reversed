#include "renderer.h"

namespace
{
// The darkening pass is in bucket 21
constexpr u32 ShadowBucket = 21;

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
}

extern "C"
{
    // The view's matrix to the screen while the shadows are drawn (the half-size buffer's), and the matrix kept to put back
    extern Matrix4x4 g_ShadowToCamera RETAIL(D_003B0770);
    extern Matrix4x4 g_ShadowToScreen RETAIL(D_003B07B0);
    extern Matrix4x4 g_SavedToScreen RETAIL(D_003BDBC0);
    // The shadows' own set-up packet, and the GS memory the frame's alpha blends with (the first texture slot, let go of)
    extern u8 g_ShadowSetUp[] RETAIL(D_003D6840);
    extern u32 g_ShadowTextureAddress RETAIL(D_0030ABA8);

    void UseHalfSizeScreen(RenderView* view, const RenderTargetDescription* target)
    {
        g_SavedToScreen = view->toScreen;
        Matrix4x4 projection;
        HalfSizeScreenMatrix(target, &projection);
        VuMultiplyMatrices(&view->toClip, &projection, &view->toScreen);
    }

    void RestoreViewScreenMatrix(RenderView* view)
    {
        view->toScreen = g_SavedToScreen;
    }

    void StartShadows(void*)
    {
        constexpr u64 Depth = 0xFFFFull << 32;
        UseHalfSizeScreen(g_RenderView, g_RenderTarget);
        RenderBucket& bucket = g_FrameBuckets.buckets[ShadowBucket];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        u32 narrowStrips = width >> 5;
        u32* at = BeginPacket(bucket);
        g_ShadowTextureAddress = FUN_001c0fe0();
        u32 wideStrips = width >> 6;
        // The asm counts the third set of strips as wide ones (the same at 512 pixels)
        u32 pairCount = 6 * wideStrips + 8 * narrowStrips + 0x2B;
        u32 pixels = width * height;
        at[0] = CallTag;
        at[1] = Address(g_ShadowSetUp);
        at[2] = 0;
        at[3] = 0;
        at[4] = (pairCount + 1) | CountTag;
        at[5] = 0;
        at[6] = VifFlushA;
        at[7] = (pairCount + 1) | VifDirect;
        PairWriter pairs{reinterpret_cast<u64*>(at + 8)};
        pairs.Write(Zeroed(pairCount | 0x8000) | 0x8000ull << 45, 0xE);

        // The half-size buffer (16 bit) cleared to grey
        pairs.Write(0, 0x3F);
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x19);
        pairs.Write(Zeroed(pixels * 10 >> 13) | static_cast<u64>(width >> 7) << 16 | 0xA000000, 0x4D);
        pairs.Write(Zeroed(pixels * 6 >> 13 | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(static_cast<u64>(width >> 1) << 16 | static_cast<u64>(height >> 1) << 48, 0x41);
        pairs.Write(5, 0x09);
        pairs.Write(1, 0x46);
        pairs.Write(0, 0x4B);
        pairs.Write(0x80, 0x3B);
        pairs.Write(0, 0x45);
        pairs.Write(0x30000, 0x48);
        pairs.Write(0x306, 0x00);
        pairs.Write(0x3F80000078787878, 0x01);
        u64 halfBottom = static_cast<u64>(height << 3) << 16;
        for (u32 strip = 0; strip < wideStrips; strip++)
        {
            pairs.Write(Zeroed(strip << 9) | Depth, 0x05);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | halfBottom | Depth, 0x05);
        }

        // The frame shrunk into it, its colour masked, then the depth buffer into the buffer past it (as 32 bit depth)
        u64 frameBottom = static_cast<u64>((height << 4) + 8) << 16;
        auto shrink = [&]()
        {
            for (u32 strip = 0; strip < narrowStrips; strip++)
            {
                pairs.Write(Zeroed((8 + strip * 0x200) | 0x80000), 0x03);
                pairs.Write(Zeroed(strip << 8) | Depth, 0x04);
                pairs.Write(Zeroed(0x208 + strip * 0x200) | frameBottom, 0x03);
                pairs.Write(Zeroed(0x100 + strip * 0x100) | halfBottom | Depth, 0x04);
            }
        };
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed(pixels * 10 >> 13) | static_cast<u64>(width >> 7) << 16 | 0xFFFFFF0A000000, 0x4D);
        pairs.Write(0x316, 0x00);
        pairs.Write(Zeroed(pixels >> 7) | static_cast<u64>(width >> 6) << 14 | 0x6A8000000, 0x07);
        pairs.Write(0, 0x15);
        pairs.Write(0x3F80000080808080, 0x01);
        shrink();
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed(pixels * 11 >> 13 | static_cast<u32>(width >> 7) << 16 | 0x30000000), 0x4D);
        pairs.Write(0x316, 0x00);
        pairs.Write(Zeroed(pixels * 6 >> 8) | static_cast<u64>(width >> 6) << 14 | 0x6AB000000, 0x07);
        pairs.Write(0, 0x15);
        pairs.Write(0x3F80000080808080, 0x01);
        shrink();

        // The half-size buffer blended with the texture slot's memory through the depth's palette
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed(pixels * 11 >> 13) | static_cast<u64>(width >> 7) << 16, 0x4D);
        pairs.Write(0x356, 0x00);
        pairs.Write(Zeroed(pixels * 11 >> 8) | static_cast<u64>(width >> 7) << 14 |
                        static_cast<u64>(g_ShadowTextureAddress) << 37 | 0x1EA9B00000 | 1ull << 61,
                    0x07);
        pairs.Write(0x80000000A4, 0x43);
        pairs.Write(0x808080, 0x01);
        u64 copyBottom = static_cast<u64>((height << 3) + 8) << 16;
        for (u32 strip = 0; strip < narrowStrips >> 1; strip++)
        {
            pairs.Write(Zeroed((8 + strip * 0x200) | 0x80000), 0x03);
            pairs.Write(Zeroed(strip << 9) | Depth, 0x04);
            pairs.Write(Zeroed(0x208 + strip * 0x200) | copyBottom, 0x03);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | halfBottom | Depth, 0x04);
        }

        // Drawing set up for the shadows into the half-size buffer
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed(pixels * 11 >> 13 | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(0x344, 0x00);
        pairs.Write(Zeroed((0x800 - (width >> 2)) << 4) | static_cast<u64>((0x800 - (height >> 2)) << 4) << 32, 0x19);
        pairs.Write(static_cast<u64>((width >> 1) - 1) << 16 | static_cast<u64>((height >> 1) - 1) << 48, 0x41);
        pairs.Write(0, 0x46);
        pairs.Write(0x8000000068, 0x43);
        pairs.Write(0x54000, 0x48);
        pairs.Write(0, 0x1A);
        pairs.Write(0x340, 0x1B);
        pairs.Write(Zeroed(pixels * 10 >> 13) | static_cast<u64>(width >> 7) << 16 | 0xFF0000000A000000, 0x4D);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));

        // The shadows' matrix to the half-size screen
        g_ShadowToCamera = g_RenderView->toClip;
        Matrix4x4 projection;
        HalfSizeScreenMatrix(g_RenderTarget, &projection);
        VuMultiplyMatrices(&g_ShadowToCamera, &projection, &g_ShadowToScreen);
    }

    void ApplyShadows(void*)
    {
        RestoreViewScreenMatrix(g_RenderView);
        RenderBucket& bucket = g_FrameBuckets.buckets[ShadowBucket];
        u32 width = g_DisplayWidth;
        u32 height = g_DisplayHeight;
        u32 narrowStrips = width >> 5;
        u32 wideStrips = width >> 6;
        u32 pairCount = (wideStrips << 1) + (narrowStrips << 2) + 0x16;
        u32 pixels = width * height;
        u32* tag = BeginPacket(bucket);
        // The GIF's data and a FLUSHA after it
        tag[0] = (pairCount + 2) | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = pairCount | VifDirect | 1;
        PairWriter pairs{reinterpret_cast<u64*>(tag + 4)};
        pairs.Write(Zeroed(pairCount | 0x8000) | 0x8000ull << 45, 0xE);

        // The half-size buffer (16 bit, past ten screens of pixels) cleared to black in strips of 32 pixels
        constexpr u64 Depth = 0xFFFFull << 32;
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x3F);
        pairs.Write(0, 0x19);
        pairs.Write(static_cast<u64>(width >> 1) << 16 | static_cast<u64>(height >> 1) << 48, 0x41);
        pairs.Write(Zeroed(pixels * 10 >> 13) | static_cast<u64>(width >> 7) << 16 | 0xFF8080800A000000, 0x4D);
        pairs.Write(0x30000, 0x48);
        pairs.Write(0x306, 0x00);
        pairs.Write(0x3F80000000000000, 0x01);
        u64 halfBottom = static_cast<u64>(height << 3) << 16;
        for (u32 strip = 0; strip < wideStrips; strip++)
        {
            pairs.Write(Zeroed(strip << 9) | Depth, 0x04);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | halfBottom | Depth, 0x04);
        }

        // The screen again, a quarter of the buffer taken away from it (Cd - Cs * 32 / 128) through bilinear sprites
        pairs.Write(0, 0x3F);
        pairs.Write(1, 0x1A);
        pairs.Write(Zeroed(pixels >> 12) | static_cast<u64>(width >> 6) << 16, 0x4D);
        pairs.Write(Zeroed(pixels * 6 >> 13 | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(0, 0x19);
        pairs.Write(0x400000004000000, 0x41);
        pairs.Write(1, 0x46);
        pairs.Write(Zeroed(pixels * 10 >> 8) | static_cast<u64>(width >> 7) << 14 | 0x664A00000, 0x07);
        pairs.Write(0x60, 0x15);
        pairs.Write(0x30000, 0x48);
        pairs.Write(0x356, 0x00);
        pairs.Write(0x2000000062, 0x43);
        pairs.Write(0x3F80000080808080, 0x01);
        u64 textureBottom = static_cast<u64>((height << 3) + 8) << 16;
        u64 screenBottom = static_cast<u64>(height << 4) << 16;
        for (u32 strip = 0; strip < narrowStrips; strip++)
        {
            pairs.Write(Zeroed((8 + strip * 0x100) | 0x80000), 0x03);
            pairs.Write(Zeroed(strip << 9) | Depth, 0x05);
            pairs.Write(Zeroed(0x108 + strip * 0x100) | textureBottom, 0x03);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | screenBottom | Depth, 0x05);
        }

        pairs.Write(0, 0x1A);
        auto* flush = reinterpret_cast<u32*>(pairs.at);
        flush[0] = VifFlushA;
        flush[1] = 0;
        flush[2] = 0;
        flush[3] = 0;
        EndPacket(bucket, reinterpret_cast<u8*>(flush + 4));
    }
}

namespace Platform::Graphics
{
void SetUpShadowPass()
{
    constexpr u32 PaletteEntries = 0x100;
    g_ShadowTextureAddress = FUN_001c0fe0();
    auto* words = reinterpret_cast<u32*>(g_ShadowSetUp);
    words[0] = 0x60000046;
    words[1] = 0;
    words[2] = 0x13000000;
    words[3] = 0x50000046;
    PairWriter pairs{reinterpret_cast<u64*>(g_ShadowSetUp + 0x10)};
    // The palette's upload: BITBLTBUF (to the slot, 32 bit), TRXPOS, TRXREG (16 by 16), TRXDIR, then the image of 64 quad words
    pairs.Write(0x1000000000000004ull, 0xE);
    pairs.Write(static_cast<u64>(g_ShadowTextureAddress) << 32 | 0x1000000000000ull, 0x50);
    pairs.Write(0, 0x51);
    pairs.Write(0x1000000010ull, 0x52);
    pairs.Write(0, 0x53);
    pairs.Write(0x0800000000008040ull, 0);
    auto* palette = reinterpret_cast<u32*>(pairs.at);
    u32 alpha = 0xFF000000;
    for (u32 index = 0; index < PaletteEntries; index++)
    {
        // The GS's palette order (CSM1): bits 3 and 4 of the index swapped
        u32 slot = (index & 0xE7) | (index & 0x8) << 1 | (index & 0x10) >> 1;
        palette[slot] = alpha | 0xFFFFFF;
        alpha = alpha + 0xFF000000;
    }
}

void DrawShadowMesh(RigidModel* mesh, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
{
    SetDefaultMeshDMA(mesh, toScreen, toCamera, world);
}
}

namespace Platform::Graphics
{
void BeginShadows()
{
    StartShadows(nullptr);
}

void EndShadows()
{
    ApplyShadows(nullptr);
}
}
