#include "renderer.h"
#include "gsvalues.h"

#include "abi.h"
#include "game/disk.h"
#include "game/effects.h"
#include "game/memory.h"

#include <bit>
#include <libgs.h>

namespace
{
u64 Zeroed(u32 value)
{
    return value;
}

// A CNT tag of VIF1's DIRECT (a FLUSHA first) of a GIF packet of A+D writes: its GIF tag (PRIM preset to the primitive), then the
// writes
GsWriter Direct(u32* at, u32 writes, u32 prim)
{
    u32 quadwords = writes + 1;
    at[0] = CountTag | quadwords;
    at[1] = 0;
    at[2] = VifFlushA;
    at[3] = VifDirect | quadwords;
    GsWriter pairs = {reinterpret_cast<GsWrite*>(at + 4)};
    pairs.Write(AddressDataTag(writes, prim), GifAddressData);
    return pairs;
}

// The GS memory the effects use besides the frame's buffers: a copy's buffer at page 0x140 (512 pixels wide) and a half width one
// at page 0x1C0 (256 pixels wide), where the palettes' uploads go too. Buffers' widths in 64 pixels
constexpr u32 CopyPage = 0x140;
constexpr u32 HalfPage = 0x1C0;
constexpr u32 CopyBlock = CopyPage << GsPageBlocksShift;
constexpr u32 PaletteBlock = HalfPage << GsPageBlocksShift;
constexpr u32 FullWidth = 8;
constexpr u32 HalfWidth = 4;

// The frame's buffers as the effects' sprites see them: the frame as a 1024 by 1024 texture of 512 pixels a row (its page added
// in blocks) as it is, at their depths (the blur's, the waves'), the depth test passing what's nearer. The rest of their
// settings are gsvalues.h's: white sprites at full alpha, the depth buffer not written (its page added), the depth test always
// passing, textured sprites of the second context in UV (blended), TEXA's alpha, the scissor, bilinear filtering and the colour
// without its alpha
const u64 FrameAsTexture = std::bit_cast<u64>(
    GS_TEX0{.tb_width = FullWidth, .tex_width = 10, .tex_height = 10, .tex_cc = 1, .tex_funtion = GS_TEX_DECAL});
const u64 FarDepth = std::bit_cast<u64>(GS_XYZ{.z = 0x7FFF});
const u64 WaveDepth = std::bit_cast<u64>(GS_XYZ{.z = 0x7000});
const u64 DepthGreater = std::bit_cast<u64>(GS_TEST{.ztest_enable = 1, .ztest_method = GS_ZBUFF_GREATER});
// ALPHA's (Cd - Cs) * As + Cs
const u64 OverSource = std::bit_cast<u64>(GS_ALPHA{.a = 1});

u64 DepthBuffer(u32 page)
{
    return Zeroed(page) | ZbufZ32Format | DepthNotWritten;
}

// 16 sprites side by side, each a 16th of the source's width (16ths of a pixel) drawn a 16th of the target's, the source's
// height drawn the target's (the texture coordinates half a texel in)
void Sprites(GsWriter& pairs, u32 sourceStep, u32 targetStep, u32 sourceHeight, u32 targetHeight, u64 depth)
{
    constexpr u32 Strips = 0x10;
    for (u32 strip = 0; strip < Strips; strip++)
    {
        pairs.Write(Zeroed(GsHalfTexel + strip * sourceStep) | HalfTexelDown, GsUv);
        pairs.Write(Zeroed(strip * targetStep) | depth, GsXyz2);
        pairs.Write(Zeroed(GsHalfTexel + (strip + 1) * sourceStep) | Zeroed(sourceHeight + GsHalfTexel) << GsUvVShift, GsUv);
        pairs.Write(Zeroed((strip + 1) * targetStep) | Zeroed(targetHeight) << GsXyzYShift | depth, GsXyz2);
    }
}

// The palettes' packets: a RET tag of VIF1's DIRECT of the rest: TEXFLUSH, the transfer of a 16 by 16 block of 32 bit pixels
// (64 a row) to the half width buffer's page and the palette as the image
struct PaletteUpload
{
    u32 tag[4];
    GsWrite writes[7];
    u32 colours[0x100];
};
CHECK_SIZE(PaletteUpload, 0x480);

constexpr u32 PaletteAlignment = 0x80;
constexpr u32 FilterPaletteCount = 8;

u8* AllocatePalette()
{
    return static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), sizeof(PaletteUpload), PaletteAlignment));
}

// The wave grid: 32 columns of a 512 pixel wide screen (16 pixels and 16 texels each), each drawn by VU1 as two strips, the upper
// 16 rows and the lower 17 (from the 16th), the rows a 32nd of the screen's height apart. A strip is its GIF tag, its count, each
// row's two vertexes and their texture coordinates, and the MSCAL
constexpr u32 GridColumns = 0x20;
constexpr u32 ColumnWidth = 0x10;
constexpr u32 ColumnSubtexels = ColumnWidth << GsSubpixelShift;
constexpr u32 UpperRows = 0x10;
constexpr u32 LowerFirstRow = 0xF;
constexpr u32 LowerRows = 0x11;
constexpr u32 RowsShift = 5;

constexpr u32 StripQuadwords(u32 rows)
{
    return 5 + rows * 3;
}

constexpr u32 GridQuadwords = 1 + GridColumns * (StripQuadwords(UpperRows) + StripQuadwords(LowerRows));
static_assert(GridQuadwords * 0x10 == 0xDA10);

// The strips' GIF tags (their vertex count in them): the end of the packet, triangle strips (PRIM preset only by the upper
// half's), three registers a vertex (UV, RGBAQ and XYZ2)
constexpr u32 StripVertexRegisters = 3;
constexpr u64 StripRegisters = GifDescriptors(GifUv, GifRgbaq, GifXyz2);

u64 StripTag(bool setsPrim)
{
    GifTag tag = {};
    tag.endOfPacket = 1;
    tag.setsPrim = setsPrim;
    tag.prim = GS_PRIM_TRI_STRIP;
    tag.registerCount = StripVertexRegisters;
    return tag.value;
}

// Where VU1 gets a strip in its double buffer: the GIF tag, the count, then every vertex's place every four quadwords and its
// texture coordinates after it. A vertex's depth word
constexpr u32 TagPlace = 0;
constexpr u32 CountPlace = 1;
constexpr u32 VertexPlace = 3;
constexpr u32 CoordinatePlace = 4;
constexpr u32 GridDepth = 0x7000;

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
// of a texel (the first row draws nothing)
u32* WriteGridStrip(u32* at, u64 tag, u32 column, u32 firstRow, u32 rows, u32 left, u32 top, u32 rowStep)
{
    u32 count = rows * 2;
    at[0] = 0;
    at[1] = VifUnpackV4 | VifTops | TagPlace;
    *reinterpret_cast<u64*>(at + 2) = tag | count;
    *reinterpret_cast<u64*>(at + 4) = StripRegisters;
    at[6] = VifCycle1;
    at[7] = VifUnpackV2 | VifTops | CountPlace;
    at[8] = count * 4;
    at[9] = count | GifEndOfPacket;
    at[10] = VifCycle4;
    at[11] = VifUnpackTo(VifUnpackV4Count | VifTops, VertexPlace, count);
    at += 12;
    u32 x = left + column * ColumnWidth;
    u32 y = top + firstRow * rowStep;
    for (u32 row = 0; row < rows; row++)
    {
        u32 flags = row == 0 ? VertexNoDraw : 0;
        at = WriteGridVertex(at, x, y, flags);
        at = WriteGridVertex(at, x + ColumnWidth, y, flags);
        y += rowStep;
    }

    at[0] = VifCycle4;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV2Count | VifTops, CoordinatePlace, count);
    at += 4;
    u32 v = (firstRow * rowStep << GsSubpixelShift) + GsHalfTexel;
    for (u32 row = 0; row < rows; row++)
    {
        at[0] = column * ColumnSubtexels + GsHalfTexel;
        at[1] = v;
        at[2] = column * ColumnSubtexels + ColumnSubtexels + GsHalfTexel;
        at[3] = v;
        v += rowStep << GsSubpixelShift;
        at += 4;
    }

    at[0] = VifMscal;
    at[1] = 0;
    at[2] = VifCycle1;
    at[3] = 0;
    return at + 4;
}

// What the waves' drawing shares with a material's (materials.cpp): VU1's two buffers taking turns, EndProgram's entry and the
// buffer VU1 takes next

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
    // The effects' switches: the far pixels' blur's (never set), the colour filter's (a start-up flag), the waves' (never set)
    // (the filter's state: game/effects.h)
    extern u8 g_BlurEffect RETAIL(D_00309C50);
    extern u8 g_ColourFilterEffect RETAIL(D_00309C51);
    extern u8 g_WaveEffect RETAIL(D_00309C52);
    // The filter's palette packets: one of each palette, the two blended ones (taking turns)
    extern u32 g_ColourFilterPalettes[] RETAIL(D_003D5850);
    extern u32 g_BlendedPaletteA RETAIL(D_0030AB54);
    extern u32 g_BlendedPaletteB RETAIL(D_0030AB5C);
    extern u8 g_BlendedPaletteTurn RETAIL(D_0030AB60);

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

    // The effects switched on, after the texture slots are let go of (their uses counted on). They're handed the GS memory past
    // the frame's buffers (where the half-size buffer goes), in blocks
    void DrawScreenEffects()
    {
        if (g_BlurEffect == 0 && g_ColourFilterEffect == 0 && g_WaveEffect == 0)
        {
            return;
        }

        u32 base = static_cast<u32>(g_ScreenWidth * g_ScreenHeight * HalfSizeBufferPixelOffset) >> GsBlockShift;
        FreeTextureSlots(&g_TextureUploadContext, 1);
        CopyDepthBuffer(base);
        if (g_BlurEffect != 0)
        {
            BlurFarPixels(base);
        }

        if (g_ColourFilterEffect != 0)
        {
            FilterColours(base, g_ColourFilterPalette, g_ColourFilterSecondPalette, g_ColourFilterBlends, g_ColourFilterAmount);
        }

        if (g_WaveEffect != 0)
        {
            WaveFrame(base);
        }
    }

    // The depth buffer copied as a 16 bit texture into the copy's buffer (16 bits a pixel, only the pixels' top two bits kept),
    // in two halves of 32 strips of 8 pixels every 16 (the base isn't used)
    void CopyDepthBuffer(u32)
    {
        constexpr u32 Strips = 0x20;
        constexpr u32 StripStep = 0x100;
        constexpr u32 StripWidth = 0x80;
        // The upper half's writes: 16 settings and 4 a strip; the lower half's: 6 settings, the strips and PRMODECONT
        constexpr u32 UpperWrites = 0x10 + Strips * 4;
        constexpr u32 LowerWrites = 6 + Strips * 4 + 1;
        const u64 CopyFrame = std::bit_cast<u64>(
            GS_FRAME{.fb_addr = CopyPage, .fb_width = FullWidth, .psm = GS_PIXMODE_16, .draw_mask = 0x3FFF});
        const u64 LowerFrame = std::bit_cast<u64>(GS_FRAME{.fb_width = FullWidth, .psm = GS_PIXMODE_16, .draw_mask = 0x3FFF});
        const u64 DepthAsTexture = std::bit_cast<u64>(GS_TEX0{.tb_width = FullWidth,
                                                              .psm = GS_ZBUFF_16,
                                                              .tex_width = 10,
                                                              .tex_height = 10,
                                                              .tex_cc = 1,
                                                              .tex_funtion = GS_TEX_DECAL});
        // A half's 32 bit pixels as pages and blocks (four bytes a pixel), and its rows as 16 bit ones (twice as many, in 16ths)
        constexpr u32 PixelPagesShift = GsPageShift - 2;
        constexpr u32 PixelBlocksShift = GsBlockShift - 2;
        constexpr u32 HalfRowsShift = GsSubpixelShift + 1;
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketScreenEffects];
        u32 depth = g_DepthBufferPage;
        u32 texture = depth << GsPageBlocksShift;
        u32 height = g_ScreenHeight;
        u32 width = g_ScreenWidth;
        u64 bottom = static_cast<u64>(height >> 1 << HalfRowsShift) << GsXyzYShift;
        u64 textureBottom = static_cast<u64>((height >> 1 << HalfRowsShift) + GsHalfTexel) << GsUvVShift;
        auto strips = [&](GsWriter& pairs)
        {
            for (u32 strip = 0; strip < Strips; strip++)
            {
                pairs.Write(Zeroed(GsHalfTexel + strip * StripStep) | HalfTexelDown, GsUv);
                pairs.Write(StripWidth + strip * StripStep, GsXyz2);
                pairs.Write(Zeroed(StripWidth + GsHalfTexel + strip * StripStep) | textureBottom, GsUv);
                pairs.Write(Zeroed(StripStep + strip * StripStep) | bottom, GsXyz2);
            }
        };

        u32* at = BeginPacket(bucket);
        GsWriter pairs = Direct(at, UpperWrites, GS_PRIM_SPRITE);
        pairs.Write(0, GsTexFlush);
        pairs.Write(AttributesFromPrim, GsPrmodeCont);
        pairs.Write(0, SecondContext(GsXyOffset));
        pairs.Write(CopyFrame, SecondContext(GsFrame));
        pairs.Write(DepthBuffer(depth), SecondContext(GsZbuf));
        pairs.Write(0, GsDither);
        pairs.Write(DepthAlways, SecondContext(GsTest));
        pairs.Write(0, GsTexFlush);
        pairs.Write(Zeroed(texture) | DepthAsTexture, SecondContext(GsTex0));
        pairs.Write(0, SecondContext(GsFba));
        // A 16 bit texel with its alpha bit set has 0x80 (TA1), without 0
        pairs.Write(SecondAlpha, GsTexa);
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(0, SecondContext(GsTex1));
        pairs.Write(WholeScissor, SecondContext(GsScissor));
        pairs.Write(TexturedSprite, GsPrim);
        pairs.Write(White, GsRgbaq);
        strips(pairs);

        // The lower half: its pages (32 bit pixels) and blocks past the upper half's
        u32 half = width * (height >> 1);
        pairs = Direct(reinterpret_cast<u32*>(pairs.at), LowerWrites, GS_PRIM_SPRITE);
        pairs.Write(Zeroed((half >> PixelPagesShift) + CopyPage) | LowerFrame, SecondContext(GsFrame));
        pairs.Write(DepthBuffer(depth + (half >> PixelPagesShift)), SecondContext(GsZbuf));
        pairs.Write(0, GsTexFlush);
        pairs.Write(Zeroed(texture + (half >> PixelBlocksShift)) | DepthAsTexture, SecondContext(GsTex0));
        pairs.Write(TexturedSprite, GsPrim);
        pairs.Write(White, GsRgbaq);
        strips(pairs);
        pairs.Write(AttributesFromPrmode, GsPrmodeCont);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    // With the filter on: its palette (the blend of its two, the one of this turn, or one of them) called, then the frame drawn
    // over itself through it in 16 strips, blended by the palette's alpha: the copy's buffer as 8 bit indexes (their pixels' top
    // bytes, PSMT8H) of the palette uploaded to the half width buffer's page
    void FilterColours(u32, u32 palette, u32 second, u32 blends, f32 amount)
    {
        constexpr u32 Strips = 0x10;
        constexpr u32 StripStep = 0x200;
        constexpr u32 StripShift = 9;
        // 10 settings, 4 a strip and PRMODECONT
        constexpr u32 Writes = 0xA + Strips * 4 + 1;
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

        const u64 AlphaTopKept = std::bit_cast<u64>(GS_FRAME{.fb_width = FullWidth, .draw_mask = 0x80000000});
        const u64 Indexes = std::bit_cast<u64>(GS_TEX0{.tb_addr = CopyBlock,
                                                       .tb_width = FullWidth,
                                                       .psm = GS_TEX_8H,
                                                       .tex_width = 10,
                                                       .tex_height = 10,
                                                       .tex_cc = 1,
                                                       .tex_funtion = GS_TEX_DECAL,
                                                       .cb_addr = PaletteBlock,
                                                       .clut_loadmode = 1});
        const u64 Depth = std::bit_cast<u64>(GS_XYZ{.z = 0x10000});
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketScreenEffects];
        u32 height = g_ScreenHeight << GsSubpixelShift;
        u32* at = BeginPacket(bucket);
        at[0] = CallTag;
        at[1] = packet;
        at[2] = 0;
        at[3] = 0;
        GsWriter pairs = Direct(at + 4, Writes, GS_PRIM_SPRITE);
        pairs.Write(AttributesFromPrim, GsPrmodeCont);
        // The frame's alpha's top bit not written
        pairs.Write(Zeroed(g_FrameBufferPage) | AlphaTopKept, SecondContext(GsFrame));
        pairs.Write(DepthBuffer(g_DepthBufferPage), SecondContext(GsZbuf));
        pairs.Write(0, GsTexFlush);
        pairs.Write(OverSource, SecondContext(GsAlpha));
        pairs.Write(DepthGreater, SecondContext(GsTest));
        pairs.Write(BlendedSprite, GsPrim);
        pairs.Write(White, GsRgbaq);
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(Indexes, SecondContext(GsTex0));
        u64 textureBottom = static_cast<u64>(height + GsHalfTexel) << GsUvVShift;
        u64 bottom = static_cast<u64>(height) << GsXyzYShift;
        for (u32 strip = 0; strip < Strips; strip++)
        {
            pairs.Write(Zeroed(GsHalfTexel + strip * StripStep) | HalfTexelDown, GsUv);
            pairs.Write(Zeroed(strip << StripShift) | Depth, GsXyz2);
            pairs.Write(Zeroed(StripStep + GsHalfTexel + strip * StripStep) | textureBottom, GsUv);
            pairs.Write(Zeroed(StripStep + strip * StripStep) | bottom | Depth, GsXyz2);
        }

        pairs.Write(AttributesFromPrmode, GsPrmodeCont);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }
}

extern "C"
{
    // The frame shrunk to half its width into the half width buffer, stretched back into the copy's buffer, and that drawn over
    // the frame where the sprites' depth (0x7FFF) is GREATER than the pixel's, blended by the alpha formula 1: (Cd - Cs) * As +
    // Cs
    void BlurFarPixels(u32)
    {
        // 15 settings, the shrink's sprites, 3 settings, the stretch's sprites, 9 settings, the blend's sprites and PRMODECONT
        constexpr u32 SpriteWrites = 0x10 * 4;
        constexpr u32 Writes = 0xF + SpriteWrites + 3 + SpriteWrites + 9 + SpriteWrites + 1;
        constexpr u32 FullStep = 0x200;
        constexpr u32 HalfStep = 0x100;
        const u64 HalfFrame = std::bit_cast<u64>(GS_FRAME{.fb_addr = HalfPage, .fb_width = HalfWidth, .draw_mask = 0xFF000000});
        const u64 CopyFrame = std::bit_cast<u64>(GS_FRAME{.fb_addr = CopyPage, .fb_width = FullWidth, .draw_mask = 0xFF000000});
        const u64 HalfAsTexture = std::bit_cast<u64>(GS_TEX0{.tb_addr = PaletteBlock,
                                                             .tb_width = HalfWidth,
                                                             .tex_width = 10,
                                                             .tex_height = 10,
                                                             .tex_cc = 1,
                                                             .tex_funtion = GS_TEX_DECAL});
        const u64 CopyAsTexture = std::bit_cast<u64>(
            GS_TEX0{.tb_addr = CopyBlock, .tb_width = FullWidth, .tex_width = 10, .tex_height = 10, .tex_cc = 1});
        const u64 FullWidthFrame = std::bit_cast<u64>(GS_FRAME{.fb_width = FullWidth});
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketScreenEffects];
        u32 height = g_ScreenHeight;
        u32 frame = g_FrameBufferPage;
        u32 depth = g_DepthBufferPage;
        u32* at = BeginPacket(bucket);
        GsWriter pairs = Direct(at, Writes, GS_PRIM_SPRITE);
        pairs.Write(AttributesFromPrim, GsPrmodeCont);
        pairs.Write(0, GsTexFlush);
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(0, SecondContext(GsXyOffset));
        pairs.Write(HalfFrame, SecondContext(GsFrame));
        pairs.Write(0, GsDither);
        pairs.Write(DepthAlways, SecondContext(GsTest));
        pairs.Write(Zeroed(frame << GsPageBlocksShift) | FrameAsTexture, SecondContext(GsTex0));
        pairs.Write(0, SecondContext(GsFba));
        pairs.Write(SecondAlpha, GsTexa);
        pairs.Write(Bilinear, SecondContext(GsTex1));
        pairs.Write(WholeScissor, SecondContext(GsScissor));
        pairs.Write(TexturedSprite, GsPrim);
        pairs.Write(White, GsRgbaq);
        pairs.Write(DepthBuffer(depth), SecondContext(GsZbuf));
        Sprites(pairs, FullStep, HalfStep, height << GsSubpixelShift, height << (GsSubpixelShift - 1), FarDepth);
        pairs.Write(0, GsTexFlush);
        pairs.Write(CopyFrame, SecondContext(GsFrame));
        pairs.Write(HalfAsTexture, SecondContext(GsTex0));
        Sprites(pairs, HalfStep, FullStep, height << (GsSubpixelShift - 1), height << GsSubpixelShift, FarDepth);
        pairs.Write(Zeroed(frame) | FullWidthFrame | FrameColourOnly, SecondContext(GsFrame));
        pairs.Write(0, GsTexFlush);
        pairs.Write(CopyAsTexture, SecondContext(GsTex0));
        pairs.Write(Bilinear, SecondContext(GsTex1));
        pairs.Write(DepthGreater, SecondContext(GsTest));
        pairs.Write(BlendedSprite, GsPrim);
        pairs.Write(OverSource, SecondContext(GsAlpha));
        pairs.Write(White, GsRgbaq);
        pairs.Write(DepthBuffer(depth), SecondContext(GsZbuf));
        Sprites(pairs, FullStep, FullStep, height << GsSubpixelShift, height << GsSubpixelShift, FarDepth);
        pairs.Write(AttributesFromPrmode, GsPrmodeCont);
        EndPacket(bucket, reinterpret_cast<u8*>(pairs.at));
    }

    // The frame copied into the copy's buffer (512 pixels wide), then drawn back over the frame through the wave grid by the wave
    // shader's VU1 program: the program loaded for the bucket (keyed like a material of the shader), the shader's packet, program
    // 2's entry, the next buffer, a CALL of the grid, PRMODECONT cleared
    void WaveFrame(u32)
    {
        // The copy's 15 settings and sprites, the drawing's 11 settings
        constexpr u32 CopyWrites = 0xF + 0x10 * 4;
        constexpr u32 SetUpWrites = 0xB;
        constexpr u32 Step = 0x200;
        constexpr u64 WaveKey = 0x200000;
        const u64 CopyFrame = std::bit_cast<u64>(GS_FRAME{.fb_addr = CopyPage, .fb_width = FullWidth, .draw_mask = 0xFF000000});
        const u64 FullWidthFrame = std::bit_cast<u64>(GS_FRAME{.fb_width = FullWidth});
        // ALPHA's (Cd - Cs) * Ad + Cs (its FIX 32 unused), PRMODE's textured blended UV in the second context, the copy as a
        // texture as it is (its palette not loaded: CLD 1 with no palette format)
        const u64 OverSourceByFrame = std::bit_cast<u64>(GS_ALPHA{.a = 1, .c = 1, .alpha = 0x20});
        const u64 Attributes = std::bit_cast<u64>(GS_PRMODE{.tme = 1, .abe = 1, .fst = 1, .ctxt = 1});
        const u64 CopyAsTexture = std::bit_cast<u64>(GS_TEX0{.tb_addr = CopyBlock,
                                                             .tb_width = FullWidth,
                                                             .tex_width = 10,
                                                             .tex_height = 10,
                                                             .tex_cc = 1,
                                                             .tex_funtion = GS_TEX_DECAL,
                                                             .clut_loadmode = 1});
        RenderBucket& bucket = g_FrameBuckets.buckets[BucketScreenEffects];
        u32 height = g_ScreenHeight;
        u32 width = g_ScreenWidth;
        u32 frame = g_FrameBufferPage;
        u32 depth = g_DepthBufferPage;
        u32* at = BeginPacket(bucket);
        GsWriter pairs = Direct(at, CopyWrites, GS_PRIM_SPRITE);
        pairs.Write(AttributesFromPrim, GsPrmodeCont);
        pairs.Write(0, GsTexFlush);
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(0, SecondContext(GsXyOffset));
        pairs.Write(CopyFrame, SecondContext(GsFrame));
        pairs.Write(0, GsDither);
        pairs.Write(DepthAlways, SecondContext(GsTest));
        pairs.Write(Zeroed(frame << GsPageBlocksShift) | FrameAsTexture, SecondContext(GsTex0));
        pairs.Write(0, SecondContext(GsFba));
        pairs.Write(SecondAlpha, GsTexa);
        pairs.Write(Bilinear, SecondContext(GsTex1));
        pairs.Write(WholeScissor, SecondContext(GsScissor));
        pairs.Write(TexturedSprite, GsPrim);
        pairs.Write(White, GsRgbaq);
        pairs.Write(DepthBuffer(depth), SecondContext(GsZbuf));
        Sprites(pairs, Step, Step, height << GsSubpixelShift, height << GsSubpixelShift, WaveDepth);
        pairs = Direct(reinterpret_cast<u32*>(pairs.at), SetUpWrites, GS_PRIM_TRI_STRIP);
        pairs.Write(AttributesFromPrmode, GsPrmodeCont);
        u64 offsetX = Zeroed((GsScreenMiddle - (width >> 1)) << GsSubpixelShift);
        u64 offsetY = Zeroed((GsScreenMiddle - (height >> 1)) << GsSubpixelShift);
        pairs.Write(offsetX | offsetY << GsOffsetYShift, SecondContext(GsXyOffset));
        pairs.Write(Zeroed(frame) | FullWidthFrame | FrameColourOnly, SecondContext(GsFrame));
        pairs.Write(DepthBuffer(depth), SecondContext(GsZbuf));
        pairs.Write(0, GsTexFlush);
        pairs.Write(OverSourceByFrame, SecondContext(GsAlpha));
        pairs.Write(DepthGreater, SecondContext(GsTest));
        pairs.Write(Attributes, GsPrmode);
        pairs.Write(White, GsRgbaq);
        pairs.Write(0, SecondContext(GsClamp));
        pairs.Write(CopyAsTexture, SecondContext(GsTex0));
        bucket.lastKey = WaveKey;
        u32 start = bucket.programRegion == 1 ? g_VuProgramRegion1 : g_VuProgramRegion2;
        g_VuProgramBucket = BucketScreenEffects;
        g_VuProgramNext = start;
        g_VuProgramTime++;
        g_VuProgramStart = start;
        u8* packet = LoadProgramForMaterial(reinterpret_cast<u8*>(pairs.at), g_ShaderType1CProgram);
        u32 counter = TakeVuBuffer(bucket);
        packet = ShaderType1CPacket(&g_ParticleWaveShader, packet, &counter, 0);
        at = WriteEndEntry(reinterpret_cast<u32*>(packet), counter);
        at = WriteNextVuBuffer(at, bucket);
        at[0] = CallTag;
        at[1] = Address(DiskLoadedMemory(GetDiskManager(), &g_EffectsDiskNode));
        at[2] = 0;
        at[3] = 0;
        // PRMODECONT cleared again by a GIF packet of its own (PRIM preset to triangle strips)
        constexpr u32 TailWrites = 1;
        at[4] = CountTag | (TailWrites + 1);
        at[5] = 0;
        at[6] = 0;
        at[7] = VifDirect | (TailWrites + 1);
        auto* tail = reinterpret_cast<GsWrite*>(at + 8);
        tail[0] = {AddressDataTag(TailWrites, GS_PRIM_TRI_STRIP), GifAddressData};
        tail[1] = {AttributesFromPrmode, GsPrmodeCont};
        EndPacket(bucket, reinterpret_cast<u8*>(tail + 2));
    }

    void WritePaletteUpload(const u32* palette, u8* packet)
    {
        constexpr u32 Quadwords = sizeof(PaletteUpload) / 0x10 - 1;
        constexpr u32 SetUpWrites = 5;
        constexpr u32 ImageQuadwords = sizeof(PaletteUpload::colours) / 0x10;
        const u64 ToPalette =
            std::bit_cast<u64>(GS_BITBLTBUF{.dest_addr = PaletteBlock, .dest_width = 1, .dest_pixmode = GS_PIXMODE_32});
        auto* upload = reinterpret_cast<PaletteUpload*>(packet);
        upload->tag[0] = ReturnTag | Quadwords;
        upload->tag[1] = 0;
        upload->tag[2] = 0;
        upload->tag[3] = VifDirect | Quadwords;
        GifTag setUp = {};
        setUp.loops = SetUpWrites;
        setUp.registerCount = 1;
        GsWriter pairs = {upload->writes};
        pairs.Write(setUp.value, GifAddressData);
        pairs.Write(1, GsTexFlush);
        pairs.Write(ToPalette, GsBitBltBuf);
        pairs.Write(0, GsTrxPos);
        pairs.Write(PaletteSize, GsTrxReg);
        pairs.Write(0, GsTrxDir);
        pairs.Write(ImageDataTag(ImageQuadwords), 0);
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
        constexpr u32 Alpha = 0xFF000000;
        u32* colours = g_BlendedPaletteTurn != 0 ? g_BlendedColoursB : g_BlendedColoursA;
        f32 rest = 1.0f - amount;
        for (u32 index = 0; index < 0x100; index++)
        {
            // Retail bug: every colour is made of the two palettes' first colours, so the blend is one flat colour
            const u8* from = g_FilterPaletteColours[first];
            const u8* to = g_FilterPaletteColours[second];
            u32 colour = colours[index] & Alpha;
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
        ShaderType1CSettings(&g_ParticleWaveShader);
        u32 rowStep = static_cast<u32>(g_ScreenHeight) >> RowsShift;
        u32 left = GsScreenMiddle - (static_cast<u32>(g_ScreenWidth) >> 1);
        u32 top = GsScreenMiddle - (static_cast<u32>(g_ScreenHeight) >> 1);
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
            at = WriteGridStrip(at, StripTag(true), column, 0, UpperRows, left, top, rowStep);
        }

        for (u32 column = 0; column < GridColumns; column++)
        {
            at = WriteGridStrip(at, StripTag(false), column, LowerFirstRow, LowerRows, left, top, rowStep);
        }
    }

    // A palette leaving every colour as it is (each index in every channel, in the CLUT's order: of every 32 indexes, 8-15 and
    // 16-23 swapped) made a packet nothing uses, then the filter's palettes and the wave grid
    void InitialiseScreenEffects()
    {
        u32 palette[0x100];
        for (u32 index = 0; index < 0x100; index++)
        {
            palette[PaletteSlot(index)] = index * 0x01010101;
        }

        u8* packet = AllocatePalette();
        g_IdentityPalette = Address(packet);
        WritePaletteUpload(palette, packet);
        MakeFilterPalettes();
        MakeWaveGrid();
    }
}

EABI_EXPORT(FUN_001aecc0, BlendPalettes);
