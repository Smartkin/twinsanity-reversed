#include "renderer.h"

#include "abi.h"
#include "game/disk.h"
#include "game/effects.h"
#include "game/memory.h"

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

// The frame's buffers as the effects' sprites see them: the frame as a 1024 by 1024 texture of 512 pixels a row (its page added
// shifted), and every sprite drawn white at full alpha
constexpr u64 FrameAsTexture = 0x28020000;
constexpr u64 FrameTextureSize = 0xE80000000;
constexpr u64 White = 0x3F80000080808080;
// The sprites' depths: the blur's and the waves'
constexpr u64 FarDepth = 0x7FFFull << 32;
constexpr u64 WaveDepth = 0x7000ull << 32;

u64 DepthBuffer(u32 page)
{
    return Zeroed(page | 0x30000000) | 1ull << 32;
}

// 16 sprites side by side, each a 16th of the source's width (16ths of a pixel) drawn a 16th of the target's, the source's
// height drawn the target's (the texture coordinates half a texel in)
void Sprites(PairWriter& pairs, u32 sourceStep, u32 targetStep, u32 sourceHeight, u32 targetHeight, u64 depth)
{
    for (u32 strip = 0; strip < 0x10; strip++)
    {
        pairs.Write(Zeroed(8 + strip * sourceStep) | 0x80000, 0x03);
        pairs.Write(Zeroed(strip * targetStep) | depth, 0x05);
        pairs.Write(Zeroed(8 + (strip + 1) * sourceStep) | Zeroed(sourceHeight + 8) << 16, 0x03);
        pairs.Write(Zeroed((strip + 1) * targetStep) | Zeroed(targetHeight) << 16 | depth, 0x05);
    }
}

// The palettes' packets: a RET tag of VIF1's DIRECT of the rest: TEXFLUSH, the transfer of a 16 by 16 block of 32 bit pixels
// (64 a row) to page 0x3800 and the palette as the image
struct PaletteUpload
{
    u32 tag[4];
    u64 pairs[14];
    u32 colours[0x100];
};
CHECK_SIZE(PaletteUpload, 0x480);

constexpr u32 PaletteAlignment = 0x80;
constexpr u32 FilterPaletteCount = 8;

u8* AllocatePalette()
{
    return static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), sizeof(PaletteUpload), PaletteAlignment));
}

// The wave grid: 32 columns of a 512 pixel wide screen, each drawn by VU1 as two strips, the upper 16 rows and the lower 17 (from
// the 16th), the rows a 32nd of the screen's height apart. A strip is its GIF tag, its count, each row's two vertexes and their
// texture coordinates, and the MSCAL
constexpr u32 GridColumns = 0x20;
constexpr u32 UpperRows = 0x10;
constexpr u32 LowerFirstRow = 0xF;
constexpr u32 LowerRows = 0x11;

constexpr u32 StripQuadwords(u32 rows)
{
    return 5 + rows * 3;
}

constexpr u32 GridQuadwords = 1 + GridColumns * (StripQuadwords(UpperRows) + StripQuadwords(LowerRows));
static_assert(GridQuadwords * 0x10 == 0xDA10);

// The strips' GIF tags (their vertex count added): the end of the packet, a triangle strip (PRIM preset only in the upper
// half's), three registers a vertex (UV, RGBAQ and XYZ2)
constexpr u64 UpperStripTag = 0xC009ull << 46 | 0x8000;
constexpr u64 LowerStripTag = 0xC008ull << 46 | 0x8000;
constexpr u64 StripRegisters = 0x513;
// VIF1's codes: UNPACKs to addresses from VU1's TOPS (FLG), of V2-32 elements, and STCYCL writing one quadword of every four
constexpr u32 VifTops = 0x8000;
constexpr u32 VifUnpackV2 = 0x64010000;
constexpr u32 VifUnpackV2Count = 0x64000000;
constexpr u32 VifCycle4 = 0x01000104;
// A vertex's depth word, and its ADC (the first row draws nothing)
constexpr u32 GridDepth = 0x7000;
constexpr u32 NoDraw = 0x8000;

u32* WriteGridVertex(u32* at, u32 x, u32 y, u32 flags)
{
    auto* place = reinterpret_cast<f32*>(at);
    place[0] = static_cast<f32>(x);
    place[1] = static_cast<f32>(y);
    at[2] = GridDepth;
    at[3] = flags;
    return at + 4;
}

// A column's strip of rows from the first: its places in pixels (the screen's middle at 2048), its texture coordinates in 16ths
// of a texel (a column 16 texels wide)
u32* WriteGridStrip(u32* at, u64 tag, u32 column, u32 firstRow, u32 rows, u32 left, u32 top, u32 rowStep)
{
    u32 count = rows * 2;
    at[0] = 0;
    at[1] = VifUnpackV4 | VifTops;
    *reinterpret_cast<u64*>(at + 2) = tag | count;
    *reinterpret_cast<u64*>(at + 4) = StripRegisters;
    at[6] = VifCycle1;
    at[7] = VifUnpackV2 | VifTops | 1;
    at[8] = count * 4;
    at[9] = count | NoDraw;
    at[10] = VifCycle4;
    at[11] = VifUnpackV4Count | count << 16 | VifTops | 3;
    at += 12;
    u32 x = left + column * 0x10;
    u32 y = top + firstRow * rowStep;
    for (u32 row = 0; row < rows; row++)
    {
        u32 flags = row == 0 ? NoDraw : 0;
        at = WriteGridVertex(at, x, y, flags);
        at = WriteGridVertex(at, x + 0x10, y, flags);
        y += rowStep;
    }

    at[0] = VifCycle4;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackV2Count | count << 16 | VifTops | 4;
    at += 4;
    u32 v = firstRow * rowStep * 0x10 + 8;
    for (u32 row = 0; row < rows; row++)
    {
        at[0] = column * 0x100 + 8;
        at[1] = v;
        at[2] = column * 0x100 + 0x108;
        at[3] = v;
        v += rowStep * 0x10;
        at += 4;
    }

    at[0] = VifMscal;
    at[1] = 0;
    at[2] = VifCycle1;
    at[3] = 0;
    return at + 4;
}

// What the waves' drawing shares with a material's (materials.cpp): VU1's two buffers taking turns, program 2's entry and the
// buffer VU1 takes next
constexpr u32 EndProgram = 2;

u32 TakeVuBuffer(RenderBucket& bucket)
{
    u32 counter = bucket.vuBuffer != 0 ? g_VuBuffer1 : g_VuBuffer2;
    bucket.vuBuffer = bucket.vuBuffer != 1;
    return counter;
}

u32* WriteEndEntry(u32* at, u32 counter)
{
    at[0] = CountTag | 1;
    at[1] = 0;
    at[2] = 0;
    at[3] = counter | VifUnpackV4;
    at[4] = g_VuPrograms[EndProgram].address;
    at[5] = 0;
    at[6] = 0;
    at[7] = 0;
    return at + 8;
}

u32* WriteNextVuBuffer(u32* at, const RenderBucket& bucket)
{
    at[0] = CountTag;
    at[1] = 0;
    at[2] = g_VuBufferPlace | VifUnpackS;
    at[3] = bucket.vuBuffer != 0 ? g_VuBuffer2 : g_VuBuffer1;
    return at + 4;
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

    // The two effects never switched on (the frame's far pixels blurred, the frame drawn through the wave grid), the blend of the
    // filter's two palettes
    void BlurFarPixels(u32 base) RETAIL(FUN_001b0438);
    void WaveFrame(u32 base) RETAIL(FUN_001af6d8);
    void BlendPalettes(u32 first, u32 second, f32 amount) RETAIL_N32(FUN_001aecc0);
    // A palette's packet written at packet, the filter's palettes made, the wave grid made
    void WritePaletteUpload(const u32* palette, u8* packet) RETAIL(FUN_001ae960);
    void MakeFilterPalettes() RETAIL(FUN_001aeab8);
    void MakeWaveGrid() RETAIL(FUN_001af150);

    // The chunks' filter palettes (256 colours each), the same as pointers, the blended palettes' colours (0x80 bytes into
    // their packets), and a packet of a palette that leaves every colour as it is (nothing uses it)
    extern const u32 g_FilterPalettes[FilterPaletteCount][0x100] RETAIL(D_002F6FF8);
    extern const u8* const g_FilterPaletteColours[FilterPaletteCount] RETAIL(D_002E73C0);
    extern u32* g_BlendedColoursA RETAIL(D_0030AB58);
    extern u32* g_BlendedColoursB RETAIL(D_0030AB64);
    extern u32 g_IdentityPalette RETAIL(D_00309C48);
    // The wave grid's disk node, the wave shader, its set-up and packet, and its program
    extern s32 g_EffectsDiskNode RETAIL(D_0030A848);
    extern WaveShader g_ParticleWaveShader RETAIL(G_PrecompShader_0x1C_3323C0);
    void ShaderType1CSettings(WaveShader* shader) RETAIL(FUN_001db098);
    u8* ShaderType1CPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001db110);
    extern u32 g_ShaderType1CProgram RETAIL(D_00309DB4);

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
            BlurFarPixels(base);
        }

        if (g_ColourFilterEffect != 0)
        {
            FilterColours(base, g_ColourFilterPalette, g_ColourFilterSecondPalette, g_ColourFilterBlends, g_ColourFilterAmount);
        }

        if (g_EffectB != 0)
        {
            WaveFrame(base);
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

extern "C"
{
    // The frame shrunk to half its width into the buffer at page 0x1C0 (256 pixels wide), stretched back into the one at page
    // 0x140, and that drawn over the frame where the sprites' depth (0x7FFF) is GREATER than the pixel's, blended by the alpha
    // formula 1: (Cd - Cs) * As + Cs
    void BlurFarPixels(u32)
    {
        constexpr u32 Quadwords = 0xDD;
        RenderBucket& bucket = g_FrameBuckets.buckets[EffectBucket];
        u32 height = g_ScreenHeight;
        u32 frame = g_FrameBufferPage;
        u32 depth = g_DepthBufferPage;
        u32* at = BeginPacket(bucket);
        PairWriter pairs = Direct(at, Quadwords);
        pairs.Write(0x10034000000080DC, 0xE);
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x3F);
        pairs.Write(0, 0x09);
        pairs.Write(0, 0x19);
        pairs.Write(0xFF000000000401C0, 0x4D);
        pairs.Write(0, 0x45);
        pairs.Write(0x30000, 0x48);
        pairs.Write(Zeroed(frame << 5 | FrameAsTexture) | FrameTextureSize, 0x07);
        pairs.Write(0, 0x4B);
        pairs.Write(0x80ull << 32, 0x3B);
        pairs.Write(0x60, 0x15);
        pairs.Write(0x400000004000000, 0x41);
        pairs.Write(0x316, 0x00);
        pairs.Write(White, 0x01);
        pairs.Write(DepthBuffer(depth), 0x4F);
        Sprites(pairs, 0x200, 0x100, height << 4, height << 3, FarDepth);
        pairs.Write(0, 0x3F);
        pairs.Write(0xFF00000000080140, 0x4D);
        pairs.Write(0xEA8013800, 0x07);
        Sprites(pairs, 0x100, 0x200, height << 3, height << 4, FarDepth);
        pairs.Write(Zeroed(frame | 0x80000) | 0xFFull << 56, 0x4D);
        pairs.Write(0, 0x3F);
        pairs.Write(0x6A8022800, 0x07);
        pairs.Write(0x60, 0x15);
        pairs.Write(0x70000, 0x48);
        pairs.Write(0x356, 0x00);
        pairs.Write(1, 0x43);
        pairs.Write(White, 0x01);
        pairs.Write(DepthBuffer(depth), 0x4F);
        Sprites(pairs, 0x200, 0x200, height << 4, height << 4, FarDepth);
        pairs.Write(0, 0x1A);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    // The frame copied into the buffer at page 0x140 (512 pixels wide), then drawn back over the frame through the wave grid by
    // the wave shader's VU1 program: the program loaded for the bucket (keyed like a material of the shader), the shader's
    // packet, program 2's entry, the next buffer, a CALL of the grid, PRMODECONT cleared
    void WaveFrame(u32)
    {
        constexpr u32 CopyQuadwords = 0x50;
        constexpr u32 SetUpQuadwords = 0xC;
        constexpr u64 WaveKey = 0x200000;
        RenderBucket& bucket = g_FrameBuckets.buckets[EffectBucket];
        u32 height = g_ScreenHeight;
        u32 width = g_ScreenWidth;
        u32 frame = g_FrameBufferPage;
        u32 depth = g_DepthBufferPage;
        u32* at = BeginPacket(bucket);
        PairWriter pairs = Direct(at, CopyQuadwords);
        pairs.Write(0x100340000000804F, 0xE);
        pairs.Write(1, 0x1A);
        pairs.Write(0, 0x3F);
        pairs.Write(0, 0x09);
        pairs.Write(0, 0x19);
        pairs.Write(0xFF00000000080140, 0x4D);
        pairs.Write(0, 0x45);
        pairs.Write(0x30000, 0x48);
        pairs.Write(Zeroed(frame << 5 | FrameAsTexture) | FrameTextureSize, 0x07);
        pairs.Write(0, 0x4B);
        pairs.Write(0x80ull << 32, 0x3B);
        pairs.Write(0x60, 0x15);
        pairs.Write(0x400000004000000, 0x41);
        pairs.Write(0x316, 0x00);
        pairs.Write(White, 0x01);
        pairs.Write(DepthBuffer(depth), 0x4F);
        Sprites(pairs, 0x200, 0x200, height << 4, height << 4, WaveDepth);
        pairs = Direct(reinterpret_cast<u32*>(pairs.at), SetUpQuadwords);
        pairs.Write(0x100240000000800B, 0xE);
        pairs.Write(0, 0x1A);
        pairs.Write(Zeroed((0x800 - (width >> 1)) << 4) | Zeroed((0x800 - (height >> 1)) << 4) << 32, 0x19);
        pairs.Write(Zeroed(frame | 0x80000) | 0xFFull << 56, 0x4D);
        pairs.Write(DepthBuffer(depth), 0x4F);
        pairs.Write(0, 0x3F);
        pairs.Write(0x2000000011, 0x43);
        pairs.Write(0x70000, 0x48);
        pairs.Write(0x350, 0x1B);
        pairs.Write(White, 0x01);
        pairs.Write(0, 0x09);
        pairs.Write(0x2000000EA8022800, 0x07);
        bucket.lastKey = WaveKey;
        u32 start = bucket.programRegion == 1 ? g_VuProgramRegion1 : g_VuProgramRegion2;
        g_VuProgramBucket = EffectBucket;
        g_VuProgramNext = start;
        g_VuProgramTime++;
        g_VuProgramStart = start;
        u8* packet = FUN_001da880(reinterpret_cast<u8*>(pairs.at), g_ShaderType1CProgram);
        u32 counter = TakeVuBuffer(bucket);
        packet = ShaderType1CPacket(&g_ParticleWaveShader, packet, &counter, 0);
        at = WriteEndEntry(reinterpret_cast<u32*>(packet), counter);
        at = WriteNextVuBuffer(at, bucket);
        at[0] = CallTag;
        at[1] = Address(DiskLoadedMemory(GetDiskManager(), &g_EffectsDiskNode));
        at[2] = 0;
        at[3] = 0;
        at[4] = CountTag | 2;
        at[5] = 0;
        at[6] = 0;
        at[7] = VifDirect | 2;
        auto* tail = reinterpret_cast<u64*>(at + 8);
        tail[0] = 0x1002400000008001;
        tail[1] = 0xE;
        tail[2] = 0;
        tail[3] = 0x1A;
        EndPacket(bucket, reinterpret_cast<u8*>(tail + 4));
    }

    void WritePaletteUpload(const u32* palette, u8* packet)
    {
        constexpr u32 Quadwords = sizeof(PaletteUpload) / 0x10 - 1;
        auto* upload = reinterpret_cast<PaletteUpload*>(packet);
        upload->tag[0] = ReturnTag | Quadwords;
        upload->tag[1] = 0;
        upload->tag[2] = 0;
        upload->tag[3] = VifDirect | Quadwords;
        PairWriter pairs = {upload->pairs};
        pairs.Write(0x1000000000000005, 0xE);
        pairs.Write(1, 0x3F);
        pairs.Write(0x0001380000000000, 0x50);
        pairs.Write(0, 0x51);
        pairs.Write(0x0000001000000010, 0x52);
        pairs.Write(0, 0x53);
        pairs.Write(0x0800000000008040, 0);
        for (u32 i = 0; i < 0x100; i++)
        {
            upload->colours[i] = palette[i];
        }
    }

    // The two blended ones start as the first palette's
    void MakeFilterPalettes()
    {
        for (u32 index = 0; index < FilterPaletteCount; index++)
        {
            u8* packet = AllocatePalette();
            g_ColourFilterPalettes[index] = Address(packet);
            WritePaletteUpload(g_FilterPalettes[index], packet);
        }

        u8* packet = AllocatePalette();
        g_BlendedPaletteA = Address(packet);
        WritePaletteUpload(g_FilterPalettes[0], packet);
        g_BlendedColoursA = reinterpret_cast<PaletteUpload*>(g_BlendedPaletteA)->colours;
        packet = AllocatePalette();
        g_BlendedPaletteB = Address(packet);
        WritePaletteUpload(g_FilterPalettes[0], packet);
        g_BlendedPaletteTurn = 0;
        g_BlendedColoursB = reinterpret_cast<PaletteUpload*>(g_BlendedPaletteB)->colours;
    }

    // The two palettes blended by the amount (the first one's share) into the blended palette of this turn (each colour keeping
    // its alpha), and the turn over
    void BlendPalettes(u32 first, u32 second, f32 amount)
    {
        u32* colours = g_BlendedPaletteTurn != 0 ? g_BlendedColoursB : g_BlendedColoursA;
        f32 rest = 1.0f - amount;
        for (u32 index = 0; index < 0x100; index++)
        {
            // Retail bug: every colour is made of the two palettes' first colours, so the blend is one flat colour
            const u8* from = g_FilterPaletteColours[first];
            const u8* to = g_FilterPaletteColours[second];
            u32 colour = colours[index] & 0xFF000000;
            for (u32 channel = 0; channel < 3; channel++)
            {
                f32 blended = static_cast<f32>(from[channel]) * amount + rest * static_cast<f32>(to[channel]);
                s32 value = static_cast<s32>(blended);
                if (value > 0xFF)
                {
                    value = 0xFF;
                }

                if (value < 0)
                {
                    value = 0;
                }

                colour |= static_cast<u32>(value) << (channel * 8);
            }

            colours[index] = colour;
        }

        g_BlendedPaletteTurn = g_BlendedPaletteTurn == 0;
    }

    // Its first quadword a RET tag of the rest, then the upper strips of every column and their lower ones; the wave shader is
    // set up first
    void MakeWaveGrid()
    {
        constexpr u32 DeferRelease = 1;
        ShaderType1CSettings(&g_ParticleWaveShader);
        u32 rowStep = static_cast<u32>(g_ScreenHeight) >> 5;
        u32 left = 0x800 - (static_cast<u32>(g_ScreenWidth) >> 1);
        u32 top = 0x800 - (static_cast<u32>(g_ScreenHeight) >> 1);
        s32 handle;
        DiskAllocate(&handle, GetDiskManager(), GridQuadwords * 0x10, false, DeferRelease);
        g_EffectsDiskNode = handle;
        auto* at = reinterpret_cast<u32*>(DiskLoadedMemory(GetDiskManager(), &g_EffectsDiskNode));
        at[0] = ReturnTag | (GridQuadwords - 1);
        at[1] = 0;
        at[2] = 0;
        at[3] = 0;
        at += 4;
        for (u32 column = 0; column < GridColumns; column++)
        {
            at = WriteGridStrip(at, UpperStripTag, column, 0, UpperRows, left, top, rowStep);
        }

        for (u32 column = 0; column < GridColumns; column++)
        {
            at = WriteGridStrip(at, LowerStripTag, column, LowerFirstRow, LowerRows, left, top, rowStep);
        }
    }

    // A palette leaving every colour as it is (each index in every channel, in the CLUT's order: of every 32 indexes, 8-15 and
    // 16-23 swapped) made a packet nothing uses, then the filter's palettes and the wave grid
    void InitialiseScreenEffects()
    {
        u32 palette[0x100];
        for (u32 index = 0; index < 0x100; index++)
        {
            u32 slot = (index & 0xE7) | (index & 0x8) << 1 | (index & 0x10) >> 1;
            palette[slot] = index * 0x01010101;
        }

        u8* packet = AllocatePalette();
        g_IdentityPalette = Address(packet);
        WritePaletteUpload(palette, packet);
        MakeFilterPalettes();
        MakeWaveGrid();
    }
}

EABI_EXPORT(FUN_001aecc0, BlendPalettes);
