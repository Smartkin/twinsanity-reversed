#include "renderer.h"

#include "abi.h"
#include "game/effects.h"

namespace
{
// The screen effects are in bucket 4
constexpr u32 EffectBucket = 4;

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

// A CNT tag of VIF1's DIRECT of the quadwords after it (a FLUSHA first)
PairWriter Direct(u32* at, u32 quadwords)
{
    at[0] = CountTag | quadwords;
    at[1] = 0;
    at[2] = VifFlushA;
    at[3] = VifDirect | quadwords;
    return {reinterpret_cast<u64*>(at + 4)};
}
}

extern "C"
{
    // The effects' switches: never set, the colour filter's (a start-up flag), never set (the filter's state: game/effects.h)
    extern u8 g_EffectA RETAIL(D_00309C50);
    extern u8 g_ColourFilterEffect RETAIL(D_00309C51);
    extern u8 g_EffectB RETAIL(D_00309C52);
    // The filter's palette packets: one of each palette, the two blended ones (taking turns)
    extern u32 g_ColourFilterPalettes[] RETAIL(D_003D5850);
    extern u32 g_BlendedPaletteA RETAIL(D_0030AB54);
    extern u32 g_BlendedPaletteB RETAIL(D_0030AB5C);
    extern u8 g_BlendedPaletteTurn RETAIL(D_0030AB60);
    // The frame and depth buffers' pages (in 2048 words)
    extern u32 g_FrameBufferPage RETAIL(D_0030AAFC);
    extern u32 g_DepthBufferPage RETAIL(D_0030AB00);

    // The effects still in asm: the two never switched on, the blend of the filter's two palettes
    void FUN_001b0438(u32 base);
    void FUN_001af6d8(u32 base);
    void BlendPalettes(u32 first, u32 second, f32 amount) RETAIL_N32(FUN_001aecc0);

    void CopyDepthBuffer(u32 base) RETAIL(FUN_001aff18);
    void FilterColours(u32 base, u32 palette, u32 second, u32 blends, f32 amount) RETAIL(FUN_001aee28);

    // The effects switched on, after the texture slots are let go of (their uses counted on)
    void FUN_001b9a68()
    {
        if (g_EffectA == 0 && g_ColourFilterEffect == 0 && g_EffectB == 0)
        {
            return;
        }

        u32 base = static_cast<u32>(g_ScreenWidth * g_ScreenHeight * 10) >> 8;
        FUN_001c0f08(&D_0030A820, 1);
        CopyDepthBuffer(base);
        if (g_EffectA != 0)
        {
            FUN_001b0438(base);
        }

        if (g_ColourFilterEffect != 0)
        {
            FilterColours(base, g_ColourFilterPalette, g_ColourFilterSecondPalette, g_ColourFilterBlends, g_ColourFilterAmount);
        }

        if (g_EffectB != 0)
        {
            FUN_001af6d8(base);
        }
    }

    // The depth buffer copied as a 16 bit texture into the buffer at page 0x140, in two halves of 32 strips of 8 pixels every
    // 16 (the base isn't used)
    void CopyDepthBuffer(u32)
    {
        constexpr u64 Frame = 0x3FFF02080140;
        constexpr u64 FrameMask = 0x3FFF00000000;
        constexpr u64 DepthAsTexture = 0x2B220000;
        constexpr u64 DepthTextureSize = 0xE80000000;
        RenderBucket& bucket = g_FrameBuckets.buckets[EffectBucket];
        u32 depth = g_DepthBufferPage;
        u32 texture = depth << 5;
        u32 height = g_ScreenHeight;
        u32 width = g_ScreenWidth;
        u64 bottom = static_cast<u64>(height >> 1 << 5) << 16;
        u64 textureBottom = static_cast<u64>((height >> 1 << 5) + 8) << 16;
        auto strips = [&](PairWriter& pairs)
        {
            for (u32 strip = 0; strip < 0x20; strip++)
            {
                pairs.Write(Zeroed((8 + strip * 0x100) | 0x80000), 0x03);
                pairs.Write(0x80 + strip * 0x100, 0x05);
                pairs.Write(Zeroed(0x88 + strip * 0x100) | textureBottom, 0x03);
                pairs.Write(Zeroed(0x100 + strip * 0x100) | bottom, 0x05);
            }
        };

        u32* at = BeginPacket(bucket);
        PairWriter pairs = Direct(at, 0x91);
        pairs.Write(0x1003400000008090, 0xE);
        pairs.Write(0, 0x3F);
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x19);
        pairs.Write(Frame, 0x4D);
        pairs.Write(Zeroed(depth | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(0, 0x45);
        pairs.Write(0x30000, 0x48);
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed(texture | DepthAsTexture) | DepthTextureSize, 0x07);
        pairs.Write(0, 0x4B);
        // TEXA: a 16 bit texel with its alpha bit set has 0x80 (TA1), without 0
        pairs.Write(0x80ull << 32, 0x3B);
        pairs.Write(0, 0x09);
        pairs.Write(0, 0x15);
        pairs.Write(0x400000004000000, 0x41);
        pairs.Write(0x316, 0x00);
        pairs.Write(0x3F80000080808080, 0x01);
        strips(pairs);

        // The lower half
        u32 half = width * (height >> 1);
        pairs = Direct(reinterpret_cast<u32*>(pairs.at), 0x88);
        pairs.Write(0x1003400000008087, 0xE);
        pairs.Write(Zeroed(((half >> 11) + 0x140) | 0x2080000) | FrameMask, 0x4D);
        pairs.Write(Zeroed((depth + (half >> 11)) | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(0, 0x3F);
        pairs.Write(Zeroed((texture + (half >> 6)) | DepthAsTexture) | DepthTextureSize, 0x07);
        pairs.Write(0x316, 0x00);
        pairs.Write(0x3F80000080808080, 0x01);
        strips(pairs);
        pairs.Write(0, 0x1A);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    // With the filter on: its palette (the blend of its two, the one of this turn, or one of them) called, then the frame drawn
    // over itself through it in 16 strips, blended by the palette's alpha
    void FilterColours(u32, u32 palette, u32 second, u32 blends, f32 amount)
    {
        if (g_ColourFilterOn == 0)
        {
            return;
        }

        u32 packet;
        if (blends != 0)
        {
            BlendPalettes(palette, second, amount);
            packet = g_BlendedPaletteTurn != 0 ? g_BlendedPaletteA : g_BlendedPaletteB;
        }
        else
        {
            packet = g_ColourFilterPalettes[palette];
        }

        RenderBucket& bucket = g_FrameBuckets.buckets[EffectBucket];
        u32 height = g_ScreenHeight << 4;
        u32* at = BeginPacket(bucket);
        at[0] = CallTag;
        at[1] = packet;
        at[2] = 0;
        at[3] = 0;
        PairWriter pairs = Direct(at + 4, 0x4C);
        constexpr u64 Depth = 1ull << 48;
        pairs.Write(0x100340000000804B, 0xE);
        pairs.Write(1, 0x1A);
        pairs.Write(Zeroed(g_FrameBufferPage | 0x80000) | 1ull << 63, 0x4D);
        pairs.Write(Zeroed(g_DepthBufferPage | 0x30000000) | 1ull << 32, 0x4F);
        pairs.Write(0, 0x3F);
        pairs.Write(1, 0x43);
        pairs.Write(0x70000, 0x48);
        pairs.Write(0x356, 0x00);
        pairs.Write(0x3F80000080808080, 0x01);
        pairs.Write(0, 0x09);
        pairs.Write(0x2007000EA9B22800, 0x07);
        u64 textureBottom = static_cast<u64>(height + 8) << 16;
        u64 bottom = static_cast<u64>(height) << 16;
        for (u32 strip = 0; strip < 0x10; strip++)
        {
            pairs.Write(Zeroed((8 + strip * 0x200) | 0x80000), 0x03);
            pairs.Write(Zeroed(strip << 9) | Depth, 0x05);
            pairs.Write(Zeroed(0x208 + strip * 0x200) | textureBottom, 0x03);
            pairs.Write(Zeroed(0x200 + strip * 0x200) | bottom | Depth, 0x05);
        }

        pairs.Write(0, 0x1A);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }
}

EABI_IMPORT(FUN_001aecc0, BlendPalettes);
