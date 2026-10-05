#include "renderer.h"
#include "gsvalues.h"

#include <bit>
#include <libgs.h>

namespace
{
// A shader's entry for VU1 (its program, where in it, where VU1 goes on to), its colour and its UV scroll
constexpr u32 EntrySize = 3;
// The middle of the GS's coordinates in 16ths of a pixel
constexpr s32 GsCentre = GsScreenMiddle << GsSubpixelShift;

// A CNT tag of an UNPACK of the quadwords after it to the counter's places (it counts them)
u32* Unpack(u8* packet, u32* counter, u32 count)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | count;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, *counter, count);
    *counter += count;
    return at + 4;
}

// Where the program was loaded for the current material's bucket
u32 ProgramAddress(u32 program)
{
    return g_VuPrograms[program].bucketAddresses[g_VuProgramBucket];
}

// The entry, and VU1's two register values twice
u8* WriteRegisterValues(u8* packet, u32* counter, u32 program, s16 entry)
{
    u32* at = Unpack(packet, counter, 5);
    u32 address = ProgramAddress(program);
    at[0] = address;
    at[1] = address + entry;
    at[2] = *counter;
    at[3] = 0;
    at += 4;
    const u32 values[4] = {g_VuRegisterValue1, g_VuRegisterValue2, g_VuRegisterValue1, g_VuRegisterValue2};
    for (u32 value : values)
    {
        at[0] = value;
        at[1] = 0;
        at[2] = 0;
        at[3] = 0;
        at += 4;
    }

    return reinterpret_cast<u8*>(at);
}

// Where VU1 goes on to past the registers (and the texture's), whose GIF tag goes at the counter
u32 NextAfterRegisters(const Shader* shader, u32 counter)
{
    u32 next = counter + shader->registerCount + 1;
    if (shader->texture != nullptr)
    {
        next += TextureRegisterCount(&g_TextureUploadContext, TextureOf(shader));
    }

    return next;
}

u32* WriteColour(u32* at, const Shader* shader)
{
    at[0] = shader->colour[0];
    at[1] = shader->colour[1];
    at[2] = shader->colour[2];
    at[3] = shader->colour[3];
    return at + 4;
}

u32* WriteScroll(u32* at, const Shader* shader, f32 last)
{
    auto* scroll = reinterpret_cast<f32*>(at);
    scroll[0] = shader->scroll[0];
    scroll[1] = shader->scroll[1];
    at[2] = 0;
    scroll[3] = last;
    return at + 4;
}

// The registers by REF after the place of their GIF tag, then the texture's registers with the tag, or the tag alone
u8* WriteRegistersToVu(u32* at, const Shader* shader, u32* counter)
{
    u32 tagPlace = *counter;
    at[0] = shader->registerCount | ReferenceTag;
    at[1] = shader->registers;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, tagPlace + 1, shader->registerCount);
    *counter = tagPlace + 1 + shader->registerCount;
    at += 4;
    if (shader->texture != nullptr)
    {
        return WriteTextureRegistersToVu(&g_TextureUploadContext, reinterpret_cast<u8*>(at), TextureOf(shader), counter,
                                         tagPlace);
    }

    at[0] = CountTag | 1;
    at[1] = 0;
    at[2] = 0;
    at[3] = tagPlace | VifUnpackV4;
    auto* gif = reinterpret_cast<u64*>(at + 4);
    gif[1] = GifAddressData;
    gif[0] = AddressDataTag(*counter - tagPlace - 1);
    return reinterpret_cast<u8*>(at + 8);
}

// The shader's data at the counter's places (it counts them): its entry into its program (loaded where the current material's
// bucket has it), its colour and its UV scroll. With its registers, VU1 also gets them (and the texture's) after a GIF tag of
// them, which its entry goes on past; without, the material sends them to the GIF itself. Type 0x1F's colour without them is
// four floats of its own, made integers
u8* WriteShaderPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters, u32 program, s16 offset,
                      const f32* floatColour = nullptr)
{
    u32* at = Unpack(packet, counter, EntrySize);
    u32 address = ProgramAddress(program);
    at[0] = address;
    at[1] = address + offset;
    at[2] = withRegisters != 0 ? NextAfterRegisters(shader, *counter) : *counter;
    at[3] = withRegisters != 0 ? 1 : 0;
    at += 4;
    if (withRegisters == 0 && floatColour != nullptr)
    {
        for (u32 i = 0; i < 4; i++)
        {
            at[i] = static_cast<u32>(static_cast<s32>(floatColour[i]));
        }

        at += 4;
    }
    else
    {
        at = WriteColour(at, shader);
    }

    at = WriteScroll(at, shader, 0.0f);
    if (withRegisters == 0)
    {
        return reinterpret_cast<u8*>(at);
    }

    return WriteRegistersToVu(at, shader, counter);
}

// The frame copied at half its width into the buffer at page 0x140 (256 pixels wide, its alpha left), in 16 strips of sprites
// through the frame as a texture: the screen-copy shaders' packet (types 0x10 and 0x11), sent to the GIF directly. It draws in
// the second context with PRIM's attributes, the frame's buffer as a 512 by 512 texture of 32 bit pixels, bilinear, always
// passing the depth test without writing depth, then gives PRMODE the attributes back
u8* WriteScreenCopy(u8* packet)
{
    constexpr u32 Quadwords = 0x51;
    constexpr u32 Strips = 0x10;
    const u64 CopyFrame =
        std::bit_cast<u64>(GS_FRAME{.fb_addr = 0x140, .fb_width = 4, .psm = GS_PIXMODE_24, .draw_mask = 0xFF000000});
    // The frame as a texture: its page in blocks of 64 words ORed in whole
    const u64 FrameTexture = std::bit_cast<u64>(
        GS_TEX0{.tb_width = 8, .psm = GS_TEX_32, .tex_width = 9, .tex_height = 9, .tex_cc = 1, .tex_funtion = 1});
    constexpr u32 BlocksPerPage = 32;
    constexpr u16 Depth = 0x7FFF;
    auto* tag = reinterpret_cast<u32*>(packet);
    tag[0] = CountTag | Quadwords;
    tag[1] = 0;
    tag[2] = VifFlushA;
    tag[3] = VifDirect | Quadwords;
    auto* at = reinterpret_cast<GsWrite*>(packet + 0x10);
    GifTag gif = {};
    gif.loops = Quadwords - 1;
    gif.endOfPacket = 1;
    gif.setsPrim = 1;
    gif.prim = GS_PRIM_SPRITE;
    gif.format = GifPacked;
    gif.registerCount = 1;
    *at++ = {gif.value, GifAddressData};
    *at++ = {AttributesFromPrim, GsPrmodeCont};
    *at++ = {0, SecondContext(GsClamp)};
    *at++ = {0, SecondContext(GsXyOffset)};
    *at++ = {CopyFrame, SecondContext(GsFrame)};
    GS_ZBUF zbuf = {};
    zbuf.update_mask = 1;
    *at++ = {std::bit_cast<u64>(zbuf) | g_DepthBufferPage | ZbufZ32Format, SecondContext(GsZbuf)};
    *at++ = {FrameTexture | g_FrameBufferPage * BlocksPerPage, SecondContext(GsTex0)};
    *at++ = {0, GsDither};
    *at++ = {DepthAlways, SecondContext(GsTest)};
    *at++ = {0, SecondContext(GsFba)};
    *at++ = {SecondAlpha, GsTexa};
    *at++ = {Bilinear, SecondContext(GsTex1)};
    *at++ = {WholeScissor, SecondContext(GsScissor)};
    *at++ = {TexturedSprite, GsPrim};
    *at++ = {White, GsRgbaq};
    *at++ = {0, GsTexFlush};
    // Each strip (in 16ths of a texel and pixel): 32 texels of the frame from half a texel in, down to the screen's bottom (its
    // V ORed in whole), drawn 16 pixels wide and 256 high
    constexpr u32 StripTexels = 0x200;
    constexpr u32 StripPixels = 0x100;
    constexpr u16 CopyHeight = 0x1000;
    u64 bottom = static_cast<u64>(static_cast<u32>((g_ScreenHeight << GsSubpixelShift) + GsHalfTexel)) << GsUvVShift;
    for (u32 strip = 0; strip < Strips; strip++)
    {
        GS_UV start = {};
        start.u = GsHalfTexel + strip * StripTexels;
        start.v = GsHalfTexel;
        *at++ = {std::bit_cast<u64>(start), GsUv};
        GS_XYZ corner = {};
        corner.x = strip * StripPixels;
        corner.z = Depth;
        *at++ = {std::bit_cast<u64>(corner), GsXyz2};
        GS_UV end = {};
        end.u = GsHalfTexel + (strip + 1) * StripTexels;
        *at++ = {std::bit_cast<u64>(end) | bottom, GsUv};
        GS_XYZ farCorner = {};
        farCorner.x = (strip + 1) * StripPixels;
        farCorner.y = CopyHeight;
        farCorner.z = Depth;
        *at++ = {std::bit_cast<u64>(farCorner), GsXyz2};
    }

    *at++ = {AttributesFromPrmode, GsPrmodeCont};
    return reinterpret_cast<u8*>(at);
}

// The screen copy, then the shader's entry, colour and scroll, where the screen's corner is in the GS's coordinates (with a float
// of the shader's) and 1 over its size there (the last two quadwords' fourth words, the last one's third, left as the buffer had
// them). With its registers, those by REF too (their GIF tag is theirs)
u8* WriteScreenCopyPacket(const ScreenCopyShader* shader, u8* packet, u32* counter, u32 withRegisters, u32 program, s16 offset)
{
    u32* at = Unpack(WriteScreenCopy(packet), counter, 5);
    u32 address = ProgramAddress(program);
    at[0] = address;
    at[1] = address + offset;
    at[2] = withRegisters != 0 ? *counter + shader->registerCount + 1 : *counter;
    at[3] = withRegisters != 0 ? 1 : 0;
    at = WriteColour(at + 4, shader);
    at = WriteScroll(at, shader, 0.0f);
    auto* values = reinterpret_cast<f32*>(at);
    values[0] = static_cast<f32>(GsCentre - (g_ScreenWidth << (GsSubpixelShift - 1)));
    values[1] = static_cast<f32>(GsCentre - (g_ScreenHeight << (GsSubpixelShift - 1)));
    values[2] = shader->cornerValue;
    values[4] = 1.0f / static_cast<f32>(g_ScreenWidth << GsSubpixelShift);
    values[5] = 1.0f / static_cast<f32>(g_ScreenHeight << GsSubpixelShift);
    at += 8;
    if (withRegisters == 0)
    {
        return reinterpret_cast<u8*>(at);
    }

    at[0] = shader->registerCount | ReferenceTag;
    at[1] = shader->registers;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, *counter, shader->registerCount);
    *counter += 1 + shader->registerCount;
    return reinterpret_cast<u8*>(at + 4);
}

// The cloth shaders' (0x17 and 0x1A): the same with their waves by REF and their amplitude after the scroll (0x1A's three in a
// quadword of their own, after an entry of four). With their registers the REF is mistyped as a REFE (its ID four bits short of
// its place, the ID bits 0), which ends the DMA chain
u8* WriteClothPacket(const ClothShader* shader, u8* packet, u32* counter, u32 withRegisters, u32 program, s16 offset,
                     bool threeAmplitudes)
{
    constexpr u32 MistypedReference = ReferenceTag >> 4;
    u32* at = Unpack(packet, counter, threeAmplitudes ? EntrySize + 1 : EntrySize);
    u32 address = ProgramAddress(program);
    at[0] = address;
    at[1] = address + offset;
    at[2] = withRegisters != 0 ? NextAfterRegisters(shader, *counter + ShaderWaveRows) : *counter + ShaderWaveRows;
    at[3] = withRegisters != 0 ? 1 : 0;
    at = WriteColour(at + 4, shader);
    at = WriteScroll(at, shader, threeAmplitudes ? 0.0f : shader->amplitudes[0]);
    if (threeAmplitudes)
    {
        auto* values = reinterpret_cast<f32*>(at);
        values[0] = shader->amplitudes[0];
        values[1] = shader->amplitudes[1];
        values[2] = shader->amplitudes[2];
        at[3] = 0;
        at += 4;
    }

    at[0] = (withRegisters != 0 ? MistypedReference : ReferenceTag) | ShaderWaveRows;
    at[1] = shader->waves;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, *counter, ShaderWaveRows);
    *counter += ShaderWaveRows;
    at += 4;
    if (withRegisters == 0)
    {
        return reinterpret_cast<u8*>(at);
    }

    return WriteRegistersToVu(at, shader, counter);
}
}

extern "C"
{
    // The shader types' programs (their indexes are set when the programs are registered) and their second entries
    extern u32 g_ShaderType01Program RETAIL(D_00309DFC);
    extern u32 g_ShaderType02Program RETAIL(D_00309DC8);
    extern u32 g_ShaderType0AProgram RETAIL(D_00309E34);
    extern u32 g_ShaderType0CProgram RETAIL(D_00309DAC);
    extern u32 g_ShaderType0EProgram RETAIL(D_00309DF8);
    extern u32 g_ShaderType15Program RETAIL(D_00309DCC);
    extern u32 g_ShaderType16Program RETAIL(D_00309DEC);
    extern u32 g_ShaderType19Program RETAIL(D_00309DE8);
    extern u32 g_ShaderType1BProgram RETAIL(D_00309DF4);
    extern u32 g_ShaderType1EProgram RETAIL(D_00309DF0);
    extern s16 g_ShaderType01Entry RETAIL(D_002EC38A);
    extern s16 g_ShaderType02Entry RETAIL(D_002EC36A);
    extern s16 g_ShaderType0AEntry RETAIL(D_002EC3CA);
    extern s16 g_ShaderType0CEntry RETAIL(D_002EC352);
    extern s16 g_ShaderType0EEntry RETAIL(D_002EC38E);
    extern s16 g_ShaderType15Entry RETAIL(D_002EC36E);
    extern s16 g_ShaderType16Entry RETAIL(D_002EC382);
    extern s16 g_ShaderType19Entry RETAIL(D_002EC37E);
    extern s16 g_ShaderType1EEntry RETAIL(D_002EC386);
    extern u32 g_ShaderType04Program RETAIL(D_00309E30);
    extern u32 g_ShaderType0FProgram RETAIL(D_00309E24);
    extern u32 g_ShaderType1FProgram RETAIL(D_00309DC4);
    extern u32 g_ShaderType20Program RETAIL(D_00309E28);
    extern s16 g_ShaderType04Entry RETAIL(D_002EC3C6);
    extern s16 g_ShaderType0FEntry RETAIL(D_002EC3BA);
    extern s16 g_ShaderType1FEntry RETAIL(D_002EC366);
    extern s16 g_ShaderType20Entry RETAIL(D_002EC3BE);
    // The shaders of their own kinds: their programs and entries
    extern u32 g_ShaderType0BProgram RETAIL(D_00309E0C);
    extern u32 g_ShaderType0DProgram RETAIL(D_00309DB0);
    extern u32 g_ShaderType12Program RETAIL(D_00309DDC);
    extern u32 g_ShaderType13Program RETAIL(D_00309DA8);
    extern u32 g_ShaderType1CProgram RETAIL(D_00309DB4);
    extern u32 g_ShaderType03Program RETAIL(D_00309DC0);
    // Type 0x0B takes the second of these
    extern s16 g_ShaderType0BEntries[2] RETAIL(D_002EC39C);
    extern s16 g_ShaderType0DEntry RETAIL(D_002EC356);
    extern s16 g_ShaderType12Entry RETAIL(D_002EC372);
    extern s16 g_ShaderType13Entry RETAIL(D_002EC34A);
    extern s16 g_ShaderType1CEntry RETAIL(D_002EC35E);
    extern s16 g_ShaderType03Entry RETAIL(D_002EC362);
    extern u32 g_ShaderType17Program RETAIL(D_00309DE4);
    extern u32 g_ShaderType1AProgram RETAIL(D_00309DE0);
    extern s16 g_ShaderType17Entry RETAIL(D_002EC37A);
    extern s16 g_ShaderType1AEntry RETAIL(D_002EC376);
    extern u32 g_ShaderType10Program RETAIL(D_00309E04);
    extern u32 g_ShaderType11Program RETAIL(D_00309E2C);
    // Types 0x10 and 0x11 take the second of theirs
    extern s16 g_ShaderType10Entries[2] RETAIL(D_002EC398);
    extern s16 g_ShaderType11Entries[2] RETAIL(D_002EC3C0);

    // The shader types' packets (their vtables' seventh function), all one writer of their own program and entry. Type 0x1B
    // shares type 1's entry
    u8* ShaderType01Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d5158);
    u8* ShaderType02Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d0368);
    u8* ShaderType0APacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d8d30);
    u8* ShaderType0CPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001cec20);
    u8* ShaderType0EPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d4a88);
    u8* ShaderType15Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d0a38);
    u8* ShaderType16Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d3640);
    u8* ShaderType19Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d2f70);
    u8* ShaderType1BPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d43e0);
    u8* ShaderType1EPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d3d10);

    u8* ShaderType01Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType01Program, g_ShaderType01Entry);
    }

    u8* ShaderType02Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType02Program, g_ShaderType02Entry);
    }

    u8* ShaderType0APacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType0AProgram, g_ShaderType0AEntry);
    }

    u8* ShaderType0CPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType0CProgram, g_ShaderType0CEntry);
    }

    u8* ShaderType0EPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType0EProgram, g_ShaderType0EEntry);
    }

    u8* ShaderType15Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType15Program, g_ShaderType15Entry);
    }

    u8* ShaderType16Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType16Program, g_ShaderType16Entry);
    }

    u8* ShaderType19Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType19Program, g_ShaderType19Entry);
    }

    u8* ShaderType1BPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType1BProgram, g_ShaderType01Entry);
    }

    u8* ShaderType1EPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType1EProgram, g_ShaderType1EEntry);
    }

    u8* ShaderType04Packet(const Shader* shader, u8* packet, u32* counter, u32) RETAIL(FUN_001d8758);
    u8* ShaderType0FPacket(const Shader* shader, u8* packet, u32* counter, u32) RETAIL(FUN_001d7378);
    u8* ShaderType20Packet(const Shader* shader, u8* packet, u32* counter, u32) RETAIL(FUN_001d7978);
    u8* ShaderType1FPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001cfc88);
    u8* ShaderType03Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(func_001DB3C8);
    u8* ShaderType0BPacket(const Shader* shader, u8* packet, u32* counter, u32) RETAIL(FUN_001dc878);
    u8* ShaderType0DPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001cf2a0);
    u8* ShaderType12Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001db830);
    u8* ShaderType13Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dad00);
    u8* ShaderType1CPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001db110);

    // These always send their registers
    u8* ShaderType04Packet(const Shader* shader, u8* packet, u32* counter, u32)
    {
        return WriteShaderPacket(shader, packet, counter, 1, g_ShaderType04Program, g_ShaderType04Entry);
    }

    u8* ShaderType0FPacket(const Shader* shader, u8* packet, u32* counter, u32)
    {
        return WriteShaderPacket(shader, packet, counter, 1, g_ShaderType0FProgram, g_ShaderType0FEntry);
    }

    u8* ShaderType20Packet(const Shader* shader, u8* packet, u32* counter, u32)
    {
        return WriteShaderPacket(shader, packet, counter, 1, g_ShaderType20Program, g_ShaderType20Entry);
    }

    u8* ShaderType1FPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteShaderPacket(shader, packet, counter, withRegisters, g_ShaderType1FProgram, g_ShaderType1FEntry,
                                 shader->shaderColour);
    }

    // The entry alone, its fourth word left as the buffer had it
    u8* ShaderType03Packet(const Shader*, u8* packet, u32* counter, u32 withRegisters)
    {
        if (withRegisters != 0)
        {
            return packet;
        }

        u32* at = Unpack(packet, counter, 1);
        u32 address = ProgramAddress(g_ShaderType03Program);
        at[0] = address;
        at[1] = address + g_ShaderType03Entry;
        at[2] = *counter;
        return reinterpret_cast<u8*>(at + 4);
    }

    // Two entries into the program, each with a colour of its own (a dark grey, then a light one) and a quadword of nothing (the
    // last one's fourth word 1)
    u8* ShaderType0BPacket(const Shader*, u8* packet, u32* counter, u32)
    {
        constexpr u32 DarkGrey = 8;
        constexpr u32 LightGrey = 0xF8;
        constexpr u32 OpaqueAlpha = 0x80;
        u32 start = *counter;
        u32* at = Unpack(packet, counter, 2 * EntrySize);
        u32 address = ProgramAddress(g_ShaderType0BProgram);
        at[0] = address;
        at[1] = address + g_ShaderType0BEntries[1];
        at[2] = start + EntrySize;
        at[3] = 1;
        at[4] = DarkGrey;
        at[5] = DarkGrey;
        at[6] = DarkGrey;
        at[7] = OpaqueAlpha;
        at[8] = 0;
        at[9] = 0;
        at[10] = 0;
        at[11] = 0;
        at[12] = address;
        at[13] = address + g_ShaderType0BEntries[1];
        at[14] = start + 2 * EntrySize;
        at[15] = 1;
        at[16] = LightGrey;
        at[17] = LightGrey;
        at[18] = LightGrey;
        at[19] = OpaqueAlpha;
        at[20] = 0;
        at[21] = 0;
        at[22] = 0;
        at[23] = 1;
        return reinterpret_cast<u8*>(at + 24);
    }

    // A 2D shader's data: its entry (its fourth word left as the buffer had it), VU1's two register values, where the screen's
    // corner is in the GS's coordinates (with a fourth word of the stack's), a GIF tag of a sprite's registers (UV, RGBAQ and
    // XYZ2 for each corner, VU1 sets the loops), nothing, and a quadword for each of a font's three pages, of which the shader's
    // page's is 5 and 1 (the rest of it left)
    u8* ShaderType0DPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        constexpr u32 SpriteRegisters = 5;
        constexpr u64 SpriteDescriptors =
            GifUv | GifRgbaq << 4 | GifXyz2 << 8 | GifUv << 12 | GifXyz2 << 16;
        constexpr u8 FontPages = 3;
        if (withRegisters != 0)
        {
            return packet;
        }

        u32* at = Unpack(packet, counter, 9);
        Vector4 corner;
        corner.x = static_cast<f32>(GsCentre) - static_cast<f32>(g_RendererWidth << GsSubpixelShift) * 0.5f;
        corner.y = static_cast<f32>(GsCentre) - static_cast<f32>(g_RendererHeight << GsSubpixelShift) * 0.5f;
        corner.z = 1.0f;
        u32 address = ProgramAddress(g_ShaderType0DProgram);
        at[0] = address;
        at[1] = address + g_ShaderType0DEntry;
        at[2] = *counter;
        at += 4;
        at[0] = g_VuRegisterValue1;
        at[1] = 0;
        at[2] = 0;
        at[3] = 0;
        at += 4;
        at[0] = g_VuRegisterValue2;
        at[1] = 0;
        at[2] = 0;
        at[3] = 0;
        at += 4;
        *reinterpret_cast<Vector4*>(at) = corner;
        at += 4;
        GifTag sprite = {};
        sprite.endOfPacket = 1;
        sprite.setsPrim = 1;
        sprite.prim = GS_PRIM_SPRITE;
        sprite.format = GifPacked;
        sprite.registerCount = SpriteRegisters;
        *reinterpret_cast<u64*>(at) = sprite.value;
        at[2] = SpriteDescriptors;
        at[3] = 0;
        at += 4;
        at[0] = 0;
        at[1] = 0;
        at[2] = 0;
        at[3] = 0;
        at += 4;
        for (u8 page = 0; page < FontPages; page++)
        {
            if (shader->fontPage == page)
            {
                at[0] = 5;
                at[1] = 1;
            }
            else
            {
                at[0] = 0;
                at[1] = 0;
                at[2] = 0;
                at[3] = 0;
            }

            at += 4;
        }

        return reinterpret_cast<u8*>(at);
    }

    u8* ShaderType12Packet(const Shader*, u8* packet, u32* counter, u32 withRegisters)
    {
        return withRegisters != 0 ? packet : WriteRegisterValues(packet, counter, g_ShaderType12Program, g_ShaderType12Entry);
    }

    u8* ShaderType13Packet(const Shader*, u8* packet, u32* counter, u32 withRegisters)
    {
        return withRegisters != 0 ? packet : WriteRegisterValues(packet, counter, g_ShaderType13Program, g_ShaderType13Entry);
    }

    // The entry, colour and scroll (with the amplitude), and the waves by REF
    u8* ShaderType1CPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        if (withRegisters != 0)
        {
            return packet;
        }

        const auto* waveShader = static_cast<const WaveShader*>(shader);
        u32* at = Unpack(packet, counter, EntrySize);
        u32 address = ProgramAddress(g_ShaderType1CProgram);
        at[0] = address;
        at[1] = address + g_ShaderType1CEntry;
        at[2] = *counter + ShaderWaveRows;
        at[3] = 0;
        at = WriteColour(at + 4, shader);
        at = WriteScroll(at, shader, waveShader->amplitude);
        at[0] = ReferenceTag | ShaderWaveRows;
        at[1] = waveShader->waves;
        at[2] = 0;
        at[3] = VifUnpackTo(VifUnpackV4Count, *counter, ShaderWaveRows);
        *counter += ShaderWaveRows;
        return reinterpret_cast<u8*>(at + 4);
    }

    u8* ShaderType17Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d24d8);
    u8* ShaderType1APacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d1948);

    u8* ShaderType17Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteClothPacket(static_cast<const ClothShader*>(shader), packet, counter, withRegisters, g_ShaderType17Program,
                                g_ShaderType17Entry, false);
    }

    u8* ShaderType1APacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteClothPacket(static_cast<const ClothShader*>(shader), packet, counter, withRegisters, g_ShaderType1AProgram,
                                g_ShaderType1AEntry, true);
    }

    u8* ShaderType10Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d6130);
    u8* ShaderType11Packet(const Shader* shader, u8* packet, u32* counter, u32) RETAIL(FUN_001d7f18);

    u8* ShaderType10Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        return WriteScreenCopyPacket(static_cast<const ScreenCopyShader*>(shader), packet, counter, withRegisters,
                                     g_ShaderType10Program, g_ShaderType10Entries[1]);
    }

    // Always with its registers
    u8* ShaderType11Packet(const Shader* shader, u8* packet, u32* counter, u32)
    {
        return WriteScreenCopyPacket(static_cast<const ScreenCopyShader*>(shader), packet, counter, 1, g_ShaderType11Program,
                                     g_ShaderType11Entries[1]);
    }

    // The VU1 programs the shader types draw with (their vtables' third function)
    extern u32 g_ShaderType00Program RETAIL(D_00309DD8);
    extern u32 g_ShaderType18Program RETAIL(D_00309E08);
    extern u32 g_ShaderType14Program RETAIL(D_00309DA4);
    extern u32 g_UnusedShaderProgram RETAIL(D_00309E00);

    u32 ShaderType01ProgramOf(const Shader* shader) RETAIL(FUN_001d9270);
    u32 ShaderType02ProgramOf(const Shader* shader) RETAIL(FUN_001d9330);
    u32 ShaderType00ProgramOf(const Shader* shader) RETAIL(FUN_001d93f0);
    u32 ShaderType04ProgramOf(const Shader* shader) RETAIL(FUN_001d9638);
    u32 ShaderType0AProgramOf(const Shader* shader) RETAIL(FUN_001d99f8);
    u32 ShaderType0BProgramOf(const Shader* shader) RETAIL(FUN_001d9ab8);
    u32 ShaderType0CProgramOf(const Shader* shader) RETAIL(FUN_001d9b70);
    u32 ShaderType0FProgramOf(const Shader* shader) RETAIL(FUN_001d9c40);
    u32 ShaderType10ProgramOf(const Shader* shader) RETAIL(FUN_001d9d10);
    u32 ShaderType11ProgramOf(const Shader* shader) RETAIL(FUN_001d9de8);
    u32 ShaderType12ProgramOf(const Shader* shader) RETAIL(FUN_001d9eb0);
    u32 ShaderType13ProgramOf(const Shader* shader) RETAIL(FUN_001d9f68);
    u32 ShaderType15ProgramOf(const Shader* shader) RETAIL(FUN_001da020);
    u32 ShaderType16ProgramOf(const Shader* shader) RETAIL(FUN_001da0e0);
    u32 ShaderType17ProgramOf(const Shader* shader) RETAIL(FUN_001da128);
    u32 ShaderType18ProgramOf(const Shader* shader) RETAIL(FUN_001da1e8);
    u32 ShaderType19ProgramOf(const Shader* shader) RETAIL(FUN_001da2b0);
    u32 ShaderType1AProgramOf(const Shader* shader) RETAIL(FUN_001da2e8);
    u32 ShaderType1BProgramOf(const Shader* shader) RETAIL(FUN_001da3a8);
    u32 ShaderType1CProgramOf(const Shader* shader) RETAIL(FUN_001da3f0);
    u32 ShaderType1EProgramOf(const Shader* shader) RETAIL(FUN_001da4b0);
    u32 ShaderType1FProgramOf(const Shader* shader) RETAIL(FUN_001da570);
    u32 ShaderType20ProgramOf(const Shader* shader) RETAIL(FUN_001da630);
    u32 ShaderType0DProgramOf(const Shader* shader) RETAIL(FUN_001da6f0);
    u32 ShaderType14ProgramOf(const Shader* shader) RETAIL(FUN_001dabe0);
    u32 ShaderType0EProgramOf(const Shader* shader) RETAIL(FUN_001dc2b0);
    u32 UnusedShaderProgram(const Shader* shader) RETAIL(FUN_001dc4c8);

    u32 ShaderType01ProgramOf(const Shader*)
    {
        return g_ShaderType01Program;
    }

    u32 ShaderType02ProgramOf(const Shader*)
    {
        return g_ShaderType02Program;
    }

    u32 ShaderType00ProgramOf(const Shader*)
    {
        return g_ShaderType00Program;
    }

    u32 ShaderType04ProgramOf(const Shader*)
    {
        return g_ShaderType04Program;
    }

    u32 ShaderType0AProgramOf(const Shader*)
    {
        return g_ShaderType0AProgram;
    }

    u32 ShaderType0BProgramOf(const Shader*)
    {
        return g_ShaderType0BProgram;
    }

    u32 ShaderType0CProgramOf(const Shader*)
    {
        return g_ShaderType0CProgram;
    }

    u32 ShaderType0FProgramOf(const Shader*)
    {
        return g_ShaderType0FProgram;
    }

    u32 ShaderType10ProgramOf(const Shader*)
    {
        return g_ShaderType10Program;
    }

    u32 ShaderType11ProgramOf(const Shader*)
    {
        return g_ShaderType11Program;
    }

    u32 ShaderType12ProgramOf(const Shader*)
    {
        return g_ShaderType12Program;
    }

    u32 ShaderType13ProgramOf(const Shader*)
    {
        return g_ShaderType13Program;
    }

    u32 ShaderType15ProgramOf(const Shader*)
    {
        return g_ShaderType15Program;
    }

    u32 ShaderType16ProgramOf(const Shader*)
    {
        return g_ShaderType16Program;
    }

    u32 ShaderType17ProgramOf(const Shader*)
    {
        return g_ShaderType17Program;
    }

    u32 ShaderType18ProgramOf(const Shader*)
    {
        return g_ShaderType18Program;
    }

    u32 ShaderType19ProgramOf(const Shader*)
    {
        return g_ShaderType19Program;
    }

    u32 ShaderType1AProgramOf(const Shader*)
    {
        return g_ShaderType1AProgram;
    }

    u32 ShaderType1BProgramOf(const Shader*)
    {
        return g_ShaderType1BProgram;
    }

    u32 ShaderType1CProgramOf(const Shader*)
    {
        return g_ShaderType1CProgram;
    }

    u32 ShaderType1EProgramOf(const Shader*)
    {
        return g_ShaderType1EProgram;
    }

    u32 ShaderType1FProgramOf(const Shader*)
    {
        return g_ShaderType1FProgram;
    }

    u32 ShaderType20ProgramOf(const Shader*)
    {
        return g_ShaderType20Program;
    }

    u32 ShaderType0DProgramOf(const Shader*)
    {
        return g_ShaderType0DProgram;
    }

    u32 ShaderType14ProgramOf(const Shader*)
    {
        return g_ShaderType14Program;
    }

    u32 ShaderType0EProgramOf(const Shader*)
    {
        return g_ShaderType0EProgram;
    }

    u32 UnusedShaderProgram(const Shader*)
    {
        return g_UnusedShaderProgram;
    }

    // The classes nothing makes: type 3 draws with its own program, types 5 to 9 name the renderer's resident programs

    u32 ShaderType03ProgramOf(const Shader* shader) RETAIL(FUN_001d94b0);
    u32 ShaderType05ProgramOf(const Shader* shader) RETAIL(FUN_001d9578);
    u32 ShaderType06ProgramOf(const Shader* shader) RETAIL(FUN_001d96f8);
    u32 ShaderType07ProgramOf(const Shader* shader) RETAIL(FUN_001d97b8);
    u32 ShaderType08ProgramOf(const Shader* shader) RETAIL(FUN_001d9878);
    u32 ShaderType09ProgramOf(const Shader* shader) RETAIL(FUN_001d9938);

    u32 ShaderType03ProgramOf(const Shader*)
    {
        return g_ShaderType03Program;
    }

    u32 ShaderType05ProgramOf(const Shader*)
    {
        return g_ParameterProgram;
    }

    u32 ShaderType06ProgramOf(const Shader*)
    {
        return g_BlendParameterProgram;
    }

    u32 ShaderType07ProgramOf(const Shader*)
    {
        return g_BlendNoShapesProgram;
    }

    u32 ShaderType08ProgramOf(const Shader*)
    {
        return g_BlendFirstShapeProgram;
    }

    u32 ShaderType09ProgramOf(const Shader*)
    {
        return g_BlendNextShapeProgram;
    }

    // Types 5 to 9 add nothing to the packet
    u8* ShaderType05Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dcf38);
    u8* ShaderType06Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dca40);
    u8* ShaderType07Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dcc10);
    u8* ShaderType08Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dcd28);
    u8* ShaderType09Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dce40);

    u8* ShaderType05Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    u8* ShaderType06Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    u8* ShaderType07Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    u8* ShaderType08Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    u8* ShaderType09Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    // Types 0 and 0x14 add nothing to the material's packet
    u8* ShaderType00Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001db740);
    u8* ShaderType14Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001dac10);

    u8* ShaderType00Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    u8* ShaderType14Packet(const Shader*, u8* packet, u32*, u32)
    {
        return packet;
    }

    // Type 0x18's (the distortion's) second entry, and the unused program's
    extern s16 g_ShaderType18Entry RETAIL(D_002EC396);
    extern s16 g_UnusedShaderEntry RETAIL(D_002EC392);

    u8* ShaderType18Packet(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d6aa8);
    u8* UnusedShaderPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters) RETAIL(FUN_001d5830);

    // The screen copy, then (without its registers: with them it's the copy alone) the entry, VU1's two register values twice,
    // where the screen's corner is in the frame's texture (its sides the powers of two that hold the screen's) with 2^-17, and
    // 1 over the texture's sides in the GS's coordinates and the screen's last pixel in it
    u8* ShaderType18Packet(const Shader*, u8* packet, u32* counter, u32 withRegisters)
    {
        constexpr u32 Quadwords = 7;
        constexpr f32 CornerScale = 0x1p-17f;
        packet = WriteScreenCopy(packet);
        if (withRegisters != 0)
        {
            return packet;
        }

        u32* at = Unpack(packet, counter, Quadwords);
        u32 address = ProgramAddress(g_ShaderType18Program);
        at[0] = address;
        at[1] = address + g_ShaderType18Entry;
        at[2] = *counter;
        at[3] = 0;
        at += 4;
        const u32 values[4] = {g_VuRegisterValue1, g_VuRegisterValue2, g_VuRegisterValue1, g_VuRegisterValue2};
        for (u32 value : values)
        {
            at[0] = value;
            at[1] = 0;
            at[2] = 0;
            at[3] = 0;
            at += 4;
        }

        auto* corner = reinterpret_cast<f32*>(at);
        corner[0] = (static_cast<f32>(g_ScreenWidth) * GsHalfSubpixels - static_cast<f32>(GsCentre)) /
                    (static_cast<f32>(NextPowerOfTwo(g_ScreenWidth)) * GsSubpixels);
        corner[1] = (static_cast<f32>(g_ScreenHeight) * GsHalfSubpixels - static_cast<f32>(GsCentre)) /
                    (static_cast<f32>(NextPowerOfTwo(g_ScreenHeight)) * GsSubpixels);
        corner[2] = CornerScale;
        at[3] = 0;
        auto* sizes = reinterpret_cast<f32*>(at + 4);
        sizes[0] = 1.0f / (static_cast<f32>(NextPowerOfTwo(g_ScreenWidth)) * GsSubpixels);
        sizes[1] = 1.0f / (static_cast<f32>(NextPowerOfTwo(g_ScreenHeight)) * GsSubpixels);
        sizes[2] = (static_cast<f32>(g_ScreenWidth) - 1.0f) / static_cast<f32>(NextPowerOfTwo(g_ScreenWidth));
        sizes[3] = (static_cast<f32>(g_ScreenHeight) - 1.0f) / static_cast<f32>(NextPowerOfTwo(g_ScreenHeight));
        return reinterpret_cast<u8*>(at + 8);
    }

    // A shader packet no type's vtable has (nothing calls it): the entry of the unused program, its colour as floats, its scroll;
    // with its registers those (and the texture's), without them three quadwords of constants
    u8* UnusedShaderPacket(const Shader* shader, u8* packet, u32* counter, u32 withRegisters)
    {
        constexpr u32 QuadwordsWithoutRegisters = 6;
        u32* at = Unpack(packet, counter, withRegisters != 0 ? EntrySize : QuadwordsWithoutRegisters);
        u32 address = ProgramAddress(g_UnusedShaderProgram);
        at[0] = address;
        at[1] = address + g_UnusedShaderEntry;
        at[2] = withRegisters != 0 ? NextAfterRegisters(shader, *counter) : *counter;
        at[3] = withRegisters != 0 ? 1 : 0;
        auto* colour = reinterpret_cast<f32*>(at + 4);
        for (u32 channel = 0; channel < 4; channel++)
        {
            // Retail bug: each float's integer is masked as though it held all four bytes, so only red survives up to 255
            u32 shift = channel * 8;
            u32 bits = static_cast<u32>(static_cast<s32>(shader->shaderColour[channel])) & 0xFFu << shift;
            colour[channel] = static_cast<f32>(bits >> shift);
        }

        at = WriteScroll(at + 8, shader, 0.0f);
        if (withRegisters != 0)
        {
            return WriteRegistersToVu(at, shader, counter);
        }

        auto* values = reinterpret_cast<f32*>(at);
        for (u32 i = 0; i < 4; i++)
        {
            values[i] = 0x1.4p-6f;
        }

        values[4] = 5.0f;
        values[5] = 5.0f;
        values[6] = 5.0f;
        at[7] = 0;
        values[8] = 128.0f;
        values[9] = 0.5f;
        values[10] = 128.0f;
        values[11] = 0x1.6A09E6p-1f;
        return reinterpret_cast<u8*>(at + 12);
    }
}
