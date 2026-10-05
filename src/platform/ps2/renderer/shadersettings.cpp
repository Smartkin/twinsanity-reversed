#include "renderer.h"

#include "game/math.h"
#include "game/memory.h"

#include <bit>
#include <libgs.h>

// The shaders' GS settings, their vtables' sixth function (made once, when the shader is set up): the A+D writes every draw of the
// shader sends. Most types write PRMODECONT, PRMODE, FBA, ZBUF, ALPHA and TEST, and TEX1 and CLAMP when they're textured; the
// screen copies (types 0x10, 0x11 and 0x18) also TEX0 of the frame's copy. The cloth and wave shaders start their waves' phases
// here too

extern "C"
{
    // ALPHA's values of the shaders' alpha presets (InitAlphaPresets fills them)
    extern u64 g_AlphaPresets[] RETAIL(G_AlphaRegPresets);
}

namespace
{
constexpr u32 PlainWrites = 7;
constexpr u32 TexturedWrites = 9;
constexpr u32 ScreenCopyWrites = 10;
// The screen copies' TEX0: the copy at block 0x2800, 256 pixels wide and high, 24 bit (types 0x10 and 0x11 take its alpha)
constexpr GS_TEX0 ScreenCopyTexture = {.tb_addr = 0x2800, .tb_width = 4, .psm = GS_TEX_24, .tex_width = 8, .tex_height = 8};
// Their alpha formula's FIX, 1
constexpr u64 ScreenCopyFix = 0x80;

// ALPHA's selectors of its formula (A - B)·C + D: the colours A, B and D, and the alpha C
enum BlendColour : u64
{
    SourceColour = 0,
    DestinationColour = 1,
    ZeroColour = 2,
};

enum BlendAlpha : u64
{
    SourceAlpha = 0,
    DestinationAlpha = 1,
    FixedAlpha = 2,
};

constexpr u64 AlphaFormula(BlendColour a, BlendColour b, BlendAlpha c, BlendColour d)
{
    return std::bit_cast<u64>(GS_ALPHA{.a = a, .b = b, .c = c, .d = d});
}

// CLAMP with both coordinates clamped (they repeat otherwise)
u64 ClampedWrapping()
{
    return std::bit_cast<u64>(GS_CLAMP{.wrap_mode_s = 1, .wrap_mode_t = 1});
}

u64 InContext(ShaderSettings settings, GsRegister address)
{
    return address + settings.secondContext;
}

// PRMODE: Gouraud shading, texturing, fog, blending, UV coordinates unless STQ, the context. Type 0 also anti-aliases and takes
// FST from the STQ bit as it is
u64 PrimitiveMode(ShaderSettings settings, bool typeZero)
{
    GS_PRMODE mode = {};
    mode.iip = settings.gouraud;
    mode.tme = settings.textured;
    mode.fge = settings.fog;
    mode.abe = settings.blends;
    mode.ctxt = settings.secondContext;
    if (typeZero)
    {
        mode.aa1 = settings.antiAliased;
        mode.fst = settings.stq;
    }
    else
    {
        mode.fst = !settings.stq;
    }

    return std::bit_cast<u64>(mode);
}

GsWrite FrameBufferAlpha(ShaderSettings settings, u64 address)
{
    GS_FBA fba = {};
    fba.alpha = !settings.noFba;
    return {std::bit_cast<u64>(fba), address};
}

GsWrite DepthBuffer(ShaderSettings settings)
{
    GS_ZBUF zbuf = {};
    zbuf.update_mask = settings.noDepthWrites;
    return {std::bit_cast<u64>(zbuf) | g_DepthBufferPage | ZbufZ32Format, InContext(settings, GsZbuf)};
}

// The shader's own formula (A, B, C, D and its FIX) or the preset's
GsWrite AlphaBlending(ShaderSettings settings, u64 preset, u64 fix)
{
    if (!settings.ownAlpha)
    {
        return {preset, InContext(settings, GsAlpha)};
    }

    GS_ALPHA alpha = {};
    alpha.a = settings.alphaA;
    alpha.b = settings.alphaB;
    alpha.c = settings.alphaC;
    alpha.d = settings.alphaD;
    alpha.alpha = fix;
    return {std::bit_cast<u64>(alpha), InContext(settings, GsAlpha)};
}

// TEST: the alpha test (left off when asked), the destination alpha test and the depth test, which is always on
GsWrite PixelTest(ShaderSettings settings, bool alphaTested = true)
{
    GS_TEST test = {};
    test.atest_enable = alphaTested && settings.alphaTest;
    test.atest_method = settings.alphaMethod;
    test.atest_reference = settings.alphaReference;
    test.atest_fail_method = settings.alphaFail;
    test.datest_enable = settings.destinationTest;
    test.datest_mode = settings.destinationMode;
    test.ztest_enable = 1;
    test.ztest_method = settings.depthTest;
    return {std::bit_cast<u64>(test), InContext(settings, GsTest)};
}

// TEX1: the mips' count (MXL), the magnifying and minifying filters and the mip choice (L and K). The game shifts MXL, L and K in
// sign-extended, so a negative one sets every bit above its field
u64 TextureFiltering(const Shader* shader, s64 maxLevel, u64 magnifying, u64 minifying)
{
    constexpr u32 MaxLevelShift = 2;
    constexpr u32 LShift = 19;
    constexpr u32 KShift = 32;
    GS_TEX1 filters = {};
    filters.mmag = magnifying;
    filters.mmin = minifying;
    return std::bit_cast<u64>(filters) | static_cast<u64>(maxLevel) << MaxLevelShift |
           static_cast<u64>(static_cast<s64>(shader->lodL)) << LShift |
           static_cast<u64>(static_cast<s64>(shader->lodK)) << KShift;
}

s64 MaxLevel(const Shader* shader)
{
    return TextureOf(shader)->levels - 1;
}

bool Unmipped(const Shader* shader)
{
    return TextureOf(shader)->levels == 1;
}

// How a type's settings differ from the others': whether mipmapped textures minify through their mips (or always with the
// magnifying filter), whether the coordinates are clamped, type 0's PRMODE, and the sky's (type 0xA) depth test made ALWAYS in
// its settings and its FBA always the first context's
struct SettingsStyle
{
    bool minifiesThroughMips;
    bool clamped;
    bool typeZero;
    bool sky;
};

void MakeSettings(Shader* shader, const SettingsStyle& style)
{
    if (style.sky)
    {
        shader->settings.depthTest = GS_ZBUFF_ALWAYS;
    }

    ShaderSettings settings = shader->settings;
    u32 count = settings.textured ? TexturedWrites : PlainWrites;
    u64 preset = g_AlphaPresets[settings.preset];
    auto* writes = static_cast<GsWrite*>(MemoryAllocate2(count * sizeof(GsWrite)));
    writes[0] = {AddressDataTag(count - 1), GifAddressData};
    writes[1] = {AttributesFromPrmode, GsPrmodeCont};
    writes[2] = {PrimitiveMode(settings, style.typeZero), GsPrmode};
    shader->registers = Address(writes);
    writes[3] = FrameBufferAlpha(settings, style.sky ? GsFba : InContext(settings, GsFba));
    writes[4] = DepthBuffer(settings);
    writes[5] = AlphaBlending(settings, preset, settings.alphaFix);
    writes[6] = PixelTest(settings);
    if (settings.textured)
    {
        u64 linear = settings.linear;
        u64 minifying = style.minifiesThroughMips && !Unmipped(shader) ? GS_TEX_LINEAR_MIPMAP_NEAREST : linear;
        writes[7] = {TextureFiltering(shader, MaxLevel(shader), linear, minifying), InContext(settings, GsTex1)};
        writes[8] = {style.clamped ? ClampedWrapping() : 0, InContext(settings, GsClamp)};
    }

    shader->registerCount = count;
}

constexpr SettingsStyle StandardStyle = {true, false, false, false};
constexpr SettingsStyle StandardClampedStyle = {true, true, false, false};
constexpr SettingsStyle UnfilteredStyle = {false, false, false, false};
constexpr SettingsStyle UnfilteredClampedStyle = {false, true, false, false};
constexpr SettingsStyle TypeZeroStyle = {false, false, true, false};
constexpr SettingsStyle SkyStyle = {false, false, false, true};

// The screen copies: always the copy as their texture (no mips), without blending (type 0x10) or with it, clamped
void MakeScreenCopySettings(Shader* shader, bool blends, bool takesAlpha)
{
    u64 preset = g_AlphaPresets[shader->settings.preset];
    auto* writes = static_cast<GsWrite*>(MemoryAllocate2(ScreenCopyWrites * sizeof(GsWrite)));
    ShaderSettings settings = shader->settings;
    writes[0] = {AddressDataTag(ScreenCopyWrites - 1), GifAddressData};
    writes[1] = {AttributesFromPrmode, GsPrmodeCont};
    GS_PRMODE mode = {};
    mode.iip = settings.gouraud;
    mode.tme = 1;
    mode.fge = settings.fog;
    mode.ctxt = settings.secondContext;
    if (blends)
    {
        mode.abe = settings.blends;
    }

    writes[2] = {std::bit_cast<u64>(mode), GsPrmode};
    shader->registers = Address(writes);
    writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
    writes[4] = DepthBuffer(settings);
    writes[5] = AlphaBlending(settings, preset, ScreenCopyFix);
    writes[6] = PixelTest(settings);
    GS_TEX0 texture = ScreenCopyTexture;
    texture.tex_cc = takesAlpha;
    writes[7] = {std::bit_cast<u64>(texture), InContext(settings, GsTex0)};
    u64 linear = settings.linear;
    writes[8] = {TextureFiltering(shader, 0, linear, linear), InContext(settings, GsTex1)};
    writes[9] = {ClampedWrapping(), InContext(settings, GsClamp)};
    shader->registerCount = ScreenCopyWrites;
}
}

extern "C"
{
    void ShaderType01Settings(Shader* shader) RETAIL(FUN_001d4d70);
    void ShaderType02Settings(Shader* shader) RETAIL(SetShaderSettings);
    void ShaderType04Settings(Shader* shader) RETAIL(FUN_001d8370);
    void ShaderType15Settings(Shader* shader) RETAIL(FUN_001d0650);
    void ShaderType16Settings(Shader* shader) RETAIL(FUN_001d3258);
    void ShaderType19Settings(Shader* shader) RETAIL(FUN_001d2b88);
    void ShaderType1BSettings(Shader* shader) RETAIL(FUN_001d3ff8);
    void ShaderType1ESettings(Shader* shader) RETAIL(FUN_001d3928);
    void ShaderType1FSettings(Shader* shader) RETAIL(FUN_001cf8a0);
    void ShaderType20Settings(Shader* shader) RETAIL(FUN_001d7590);
    void ShaderType14Settings(Shader* shader) RETAIL(FUN_001ce0b0);
    // A shader class nothing makes (its vtable at D_002FA0A8)
    void UnusedShaderSettings(Shader* shader) RETAIL(FUN_001d5440);
    void ShaderType0CSettings(Shader* shader) RETAIL(FUN_001ce860);
    void ShaderType0ESettings(Shader* shader) RETAIL(FUN_001d46c8);
    void ShaderType12Settings(Shader* shader) RETAIL(FUN_001d10f0);
    void ShaderType13Settings(Shader* shader) RETAIL(FUN_001ce4a0);
    void ShaderType0FSettings(Shader* shader) RETAIL(FUN_001d6fb8);
    void ShaderType00Settings(Shader* shader) RETAIL(FUN_001d0d20);
    void ShaderType0ASettings(Shader* shader) RETAIL(FUN_001d8970);
    void ShaderType0BSettings(Shader* shader) RETAIL(FUN_001dc870);
    void ShaderType0DSettings(Shader* shader) RETAIL(FUN_001cef08);
    void ShaderType10Settings(Shader* shader) RETAIL(FUN_001d5db8);
    void ShaderType11Settings(Shader* shader) RETAIL(FUN_001d7b90);
    void ShaderType18Settings(Shader* shader) RETAIL(FUN_001d6720);
    void ShaderType17Settings(ClothShader* shader) RETAIL(FUN_001d2058);
    void ShaderType1ASettings(ClothShader* shader) RETAIL(FUN_001d14b0);
    void ShaderType1CSettings(WaveShader* shader) RETAIL(FUN_001db098);

    void ShaderType01Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType02Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType04Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType15Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType16Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType19Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType1BSettings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType1ESettings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType1FSettings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType20Settings(Shader* shader)
    {
        MakeSettings(shader, StandardStyle);
    }

    void ShaderType14Settings(Shader* shader)
    {
        MakeSettings(shader, StandardClampedStyle);
    }

    void UnusedShaderSettings(Shader* shader)
    {
        MakeSettings(shader, StandardClampedStyle);
    }

    // The classes nothing makes: type 3 always writes seven registers, anti-aliased and untextured without the alpha test,
    // types 5 to 9 none
    void ShaderType03Settings(Shader* shader) RETAIL(FUN_001cf5e8);
    void ShaderType05Settings(Shader* shader) RETAIL(FUN_001dcf30);
    void ShaderType06Settings(Shader* shader) RETAIL(FUN_001dca38);
    void ShaderType07Settings(Shader* shader) RETAIL(FUN_001dcc08);
    void ShaderType08Settings(Shader* shader) RETAIL(FUN_001dcd20);
    void ShaderType09Settings(Shader* shader) RETAIL(FUN_001dce38);

    void ShaderType03Settings(Shader* shader)
    {
        ShaderSettings settings = shader->settings;
        u64 preset = g_AlphaPresets[settings.preset];
        auto* writes = static_cast<GsWrite*>(MemoryAllocate2(PlainWrites * sizeof(GsWrite)));
        writes[0] = {AddressDataTag(PlainWrites - 1), GifAddressData};
        writes[1] = {AttributesFromPrmode, GsPrmodeCont};
        GS_PRMODE mode = {};
        mode.iip = settings.gouraud;
        mode.fge = settings.fog;
        mode.abe = settings.blends;
        mode.aa1 = 1;
        mode.ctxt = settings.secondContext;
        writes[2] = {std::bit_cast<u64>(mode), GsPrmode};
        shader->registers = Address(writes);
        writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
        writes[4] = DepthBuffer(settings);
        writes[5] = AlphaBlending(settings, preset, settings.alphaFix);
        writes[6] = PixelTest(settings, false);
        shader->registerCount = PlainWrites;
    }

    void ShaderType05Settings(Shader*)
    {
    }

    void ShaderType06Settings(Shader*)
    {
    }

    void ShaderType07Settings(Shader*)
    {
    }

    void ShaderType08Settings(Shader*)
    {
    }

    void ShaderType09Settings(Shader*)
    {
    }

    void InitAlphaPresets()
    {
        g_AlphaPresets[PresetMix] = AlphaFormula(SourceColour, DestinationColour, SourceAlpha, DestinationColour);
        g_AlphaPresets[PresetAdd] = AlphaFormula(SourceColour, ZeroColour, SourceAlpha, DestinationColour);
        g_AlphaPresets[PresetScale] = AlphaFormula(DestinationColour, ZeroColour, SourceAlpha, ZeroColour);
        g_AlphaPresets[PresetSubtract] = AlphaFormula(ZeroColour, SourceColour, SourceAlpha, DestinationColour);
        g_AlphaPresets[PresetBrighten] = AlphaFormula(DestinationColour, ZeroColour, SourceAlpha, DestinationColour);
        g_AlphaPresets[PresetDarken] = AlphaFormula(ZeroColour, DestinationColour, SourceAlpha, DestinationColour);
    }

    void ShaderType0CSettings(Shader* shader)
    {
        MakeSettings(shader, UnfilteredStyle);
    }

    void ShaderType0ESettings(Shader* shader)
    {
        MakeSettings(shader, UnfilteredStyle);
    }

    void ShaderType12Settings(Shader* shader)
    {
        MakeSettings(shader, UnfilteredStyle);
    }

    void ShaderType13Settings(Shader* shader)
    {
        MakeSettings(shader, UnfilteredStyle);
    }

    void ShaderType0FSettings(Shader* shader)
    {
        MakeSettings(shader, UnfilteredClampedStyle);
    }

    void ShaderType00Settings(Shader* shader)
    {
        MakeSettings(shader, TypeZeroStyle);
    }

    void ShaderType0ASettings(Shader* shader)
    {
        MakeSettings(shader, SkyStyle);
    }

    // None of its own
    void ShaderType0BSettings(Shader*)
    {
    }

    // The 2D shader: always textured (its page), the magnifying filter always linear, clamped
    void ShaderType0DSettings(Shader* shader)
    {
        auto* writes = static_cast<GsWrite*>(MemoryAllocate2(TexturedWrites * sizeof(GsWrite)));
        ShaderSettings settings = shader->settings;
        writes[0] = {AddressDataTag(TexturedWrites - 1), GifAddressData};
        writes[1] = {AttributesFromPrmode, GsPrmodeCont};
        writes[2] = {PrimitiveMode(settings, false), GsPrmode};
        shader->registers = Address(writes);
        writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
        writes[4] = DepthBuffer(settings);
        writes[5] = AlphaBlending(settings, g_AlphaPresets[settings.preset], settings.alphaFix);
        writes[6] = PixelTest(settings);
        u64 minifying = Unmipped(shader) ? settings.linear : GS_TEX_LINEAR_MIPMAP_NEAREST;
        writes[7] = {TextureFiltering(shader, MaxLevel(shader), GS_TEX_LINEAR, minifying), InContext(settings, GsTex1)};
        writes[8] = {ClampedWrapping(), InContext(settings, GsClamp)};
        shader->registerCount = TexturedWrites;
    }

    void ShaderType10Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, false, true);
    }

    void ShaderType11Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, true, true);
    }

    // The distortion's: its copy's alpha isn't taken
    void ShaderType18Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, true, false);
    }

    // Phases of up to half a turn, or in mode 2 and up of up to 1
    void ShaderType17Settings(ClothShader* shader)
    {
        MakeSettings(shader, StandardStyle);
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            f32 range = shader->mode < ClothRadians ? Pi : 1.0f;
            shader->phases[row].x = RandomSignedTimes(range);
            shader->phases[row].y = RandomSignedTimes(range);
            shader->phases[row].z = RandomSignedTimes(range);
        }
    }

    void ShaderType1ASettings(ClothShader* shader)
    {
        MakeSettings(shader, StandardStyle);
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            shader->phases[row].x = RandomSignedTimes(Pi);
            shader->phases[row].y = RandomSignedTimes(Pi);
            shader->phases[row].z = RandomSignedTimes(Pi);
            if (shader->mode >= ClothRadians)
            {
                shader->secondPhases[row].x = RandomSignedTimes(Pi);
                shader->secondPhases[row].y = RandomSignedTimes(Pi);
                shader->secondPhases[row].z = RandomSignedTimes(Pi);
            }
        }
    }

    // No GS settings, its phases alone
    void ShaderType1CSettings(WaveShader* shader)
    {
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            shader->phases[row].x = RandomSignedTimes(Pi);
            shader->phases[row].y = RandomSignedTimes(Pi);
            shader->phases[row].z = RandomSignedTimes(Pi);
        }
    }
}
