#include "renderer.h"

#include "game/math.h"
#include "game/memory.h"

// The shaders' GS settings, their vtables' sixth function (made once, when the shader is set up): the A+D writes every draw of the
// shader sends. Most types write PRMODECONT, PRMODE, FBA, ZBUF, ALPHA and TEST, and TEX1 and CLAMP when they're textured; the
// screen copies (types 0x10, 0x11 and 0x18) also TEX0 of the frame's copy. The cloth and wave shaders start their waves' phases
// here too

extern "C"
{
    // ALPHA's values of the shaders' alpha presets (FUN_001daa68 fills them)
    extern u64 g_AlphaPresets[] RETAIL(G_AlphaRegPresets);
    extern u32 g_DepthBufferPage RETAIL(D_0030AB00);
}

namespace
{
// The GS's registers (the first context's, the second context's are one past them)
enum GsRegister : u64
{
    GsTex0 = 0x06,
    GsClamp = 0x08,
    GsTex1 = 0x14,
    GsPrmodeCont = 0x1A,
    GsPrmode = 0x1B,
    GsAlpha = 0x42,
    GsTest = 0x47,
    GsFba = 0x4A,
    GsZbuf = 0x4E,
};

// A register's value and address, a quadword of the packet
struct GsWrite
{
    u64 value;
    u64 address;
};

constexpr u32 PlainWrites = 7;
constexpr u32 TexturedWrites = 9;
constexpr u32 ScreenCopyWrites = 10;
// ZBUF's 32 bit depth format, CLAMP's wrapping (both coordinates clamped) and TEX1's minifying through the mips (MMIN 4)
constexpr u64 DepthFormat = 0x30000000;
constexpr u64 Clamped = 5;
constexpr u64 MipsMinify = 4;
// The screen copies' TEX0: the copy at block 0x2800, 256 pixels wide and high, 24 bit (types 0x10 and 0x11 take its alpha)
constexpr u64 ScreenCopyTexture = 0x220112800;
constexpr u64 ScreenCopyAlpha = 0x400000000;
// Their alpha formula's FIX, 1
constexpr u64 ScreenCopyFix = 0x80;
constexpr f32 Pi = 0x1.921FB6p+1f;

u64 Bit(u64 settings, u32 bit)
{
    return settings >> bit & 1;
}

u64 InContext(u64 settings, u64 address)
{
    return address + Bit(settings, SettingSecondContext);
}

// The GIF tag of a packet of A+D writes (its quadwords, the tag counted)
GsWrite AddressDataTag(u32 count)
{
    return {(count - 1) | 0x8000 | 0x8000ull << 45, 0xE};
}

// PRMODE: Gouraud shading, texturing, fog, blending, UV coordinates unless STQ, the context. Type 0 also anti-aliases and takes
// FST from the STQ bit as it is
u64 PrimitiveMode(u64 settings, bool typeZero)
{
    u64 mode = Bit(settings, SettingGouraud) << 3 | Bit(settings, SettingTextured) << 4 | Bit(settings, SettingFog) << 5 |
               Bit(settings, SettingBlends) << 6 | Bit(settings, SettingSecondContext) << 9;
    if (typeZero)
    {
        return mode | Bit(settings, SettingAntiAliased) << 7 | Bit(settings, SettingStq) << 8;
    }

    return mode | (Bit(settings, SettingStq) ^ 1) << 8;
}

GsWrite FrameBufferAlpha(u64 settings, u64 address)
{
    return {Bit(settings, SettingNoFba) ^ 1, address};
}

GsWrite DepthBuffer(u64 settings)
{
    return {(g_DepthBufferPage | DepthFormat) | Bit(settings, SettingNoDepthWrites) << 32, InContext(settings, GsZbuf)};
}

// The shader's own formula (A, B, C, D and its FIX) or the preset's
GsWrite AlphaBlending(u64 settings, u64 preset, u64 fix)
{
    if (Bit(settings, SettingOwnAlpha) == 0)
    {
        return {preset, InContext(settings, GsAlpha)};
    }

    u64 formula = settings >> SettingAlphaFormula;
    u64 value = (formula & 3) | (formula >> 2 & 3) << 2 | (formula >> 4 & 3) << 4 | (formula >> 6 & 3) << 6 | fix << 32;
    return {value, InContext(settings, GsAlpha)};
}

u64 OwnFix(u64 settings)
{
    return settings >> SettingAlphaFix & 0xFF;
}

// TEST: the alpha test, the destination alpha test and the depth test, which is always on
GsWrite PixelTest(u64 settings)
{
    u64 value = Bit(settings, SettingAlphaTest) | (settings >> SettingAlphaMethod & 7) << 1 |
                (settings >> SettingAlphaReference & 0xFF) << 4 | (settings >> SettingAlphaFail & 3) << 12 |
                Bit(settings, SettingDestinationTest) << 14 | Bit(settings, SettingDestinationMode) << 15 |
                (settings >> SettingDepthTest & 3) << 17 | 0x10000;
    return {value, InContext(settings, GsTest)};
}

// TEX1: the mips' count (levels from the texture), the magnifying and minifying filters and the mip choice
u64 TextureFiltering(const Shader* shader, u64 levels, u64 magnifying, u64 minifying)
{
    return levels | magnifying << 5 | minifying << 6 | static_cast<u64>(static_cast<s64>(shader->lodL)) << 19 |
           static_cast<u64>(static_cast<s64>(shader->lodK)) << 32;
}

u64 Levels(const Shader* shader)
{
    const auto* texture = reinterpret_cast<const Texture*>(shader->texture + 0xC);
    return static_cast<u64>(static_cast<s64>(texture->levels - 1)) << 2;
}

bool Unmipped(const Shader* shader)
{
    return reinterpret_cast<const Texture*>(shader->texture + 0xC)->levels == 1;
}

// How a type's settings differ from the others': whether mipmapped textures minify through their mips (or always with the
// magnifying filter), the wrapping, type 0's PRMODE, and the sky's (type 0xA) depth test made ALWAYS in its settings and its FBA
// always the first context's
struct SettingsStyle
{
    bool minifiesThroughMips;
    u64 clamp;
    bool typeZero;
    bool sky;
};

void MakeSettings(Shader* shader, const SettingsStyle& style)
{
    if (style.sky)
    {
        shader->settings = (shader->settings & ~(3ull << SettingDepthTest)) | 1ull << SettingDepthTest;
    }

    u64 settings = shader->settings;
    u32 count = Bit(settings, SettingTextured) != 0 ? TexturedWrites : PlainWrites;
    u64 preset = g_AlphaPresets[settings >> SettingPreset & 0xF];
    auto* writes = static_cast<GsWrite*>(MemoryAllocate2(count * sizeof(GsWrite)));
    writes[0] = AddressDataTag(count);
    writes[1] = {0, GsPrmodeCont};
    writes[2] = {PrimitiveMode(settings, style.typeZero), GsPrmode};
    shader->registers = Address(writes);
    writes[3] = FrameBufferAlpha(settings, style.sky ? GsFba : InContext(settings, GsFba));
    writes[4] = DepthBuffer(settings);
    writes[5] = AlphaBlending(settings, preset, OwnFix(settings));
    writes[6] = PixelTest(settings);
    if (Bit(settings, SettingTextured) != 0)
    {
        u64 linear = Bit(settings, SettingLinear);
        u64 minifying = style.minifiesThroughMips && !Unmipped(shader) ? MipsMinify : linear;
        writes[7] = {TextureFiltering(shader, Levels(shader), linear, minifying), InContext(settings, GsTex1)};
        writes[8] = {style.clamp, InContext(settings, GsClamp)};
    }

    shader->registerCount = count;
}

constexpr SettingsStyle StandardStyle = {true, 0, false, false};
constexpr SettingsStyle StandardClampedStyle = {true, Clamped, false, false};
constexpr SettingsStyle UnfilteredStyle = {false, 0, false, false};
constexpr SettingsStyle UnfilteredClampedStyle = {false, Clamped, false, false};
constexpr SettingsStyle TypeZeroStyle = {false, 0, true, false};
constexpr SettingsStyle SkyStyle = {false, 0, false, true};

// The screen copies: always the copy as their texture (no mips), without blending (type 0x10) or with it, clamped
void MakeScreenCopySettings(Shader* shader, bool blends, u64 texture)
{
    u64 preset = g_AlphaPresets[shader->settings >> SettingPreset & 0xF];
    auto* writes = static_cast<GsWrite*>(MemoryAllocate2(ScreenCopyWrites * sizeof(GsWrite)));
    u64 settings = shader->settings;
    writes[0] = AddressDataTag(ScreenCopyWrites);
    writes[1] = {0, GsPrmodeCont};
    u64 mode = Bit(settings, SettingGouraud) << 3 | 0x10 | Bit(settings, SettingFog) << 5 | Bit(settings, SettingSecondContext) << 9;
    if (blends)
    {
        mode |= Bit(settings, SettingBlends) << 6;
    }

    writes[2] = {mode, GsPrmode};
    shader->registers = Address(writes);
    writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
    writes[4] = DepthBuffer(settings);
    writes[5] = AlphaBlending(settings, preset, ScreenCopyFix);
    writes[6] = PixelTest(settings);
    writes[7] = {texture, InContext(settings, GsTex0)};
    u64 linear = Bit(settings, SettingLinear);
    writes[8] = {TextureFiltering(shader, 0, linear, linear), InContext(settings, GsTex1)};
    writes[9] = {Clamped, InContext(settings, GsClamp)};
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
        constexpr u64 AntiAliased = 0x80;
        u64 settings = shader->settings;
        u64 preset = g_AlphaPresets[settings >> SettingPreset & 0xF];
        auto* writes = static_cast<GsWrite*>(MemoryAllocate2(PlainWrites * sizeof(GsWrite)));
        writes[0] = AddressDataTag(PlainWrites);
        writes[1] = {0, GsPrmodeCont};
        u64 mode = Bit(settings, SettingGouraud) << 3 | Bit(settings, SettingFog) << 5 | Bit(settings, SettingBlends) << 6 |
                   AntiAliased | Bit(settings, SettingSecondContext) << 9;
        writes[2] = {mode, GsPrmode};
        shader->registers = Address(writes);
        writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
        writes[4] = DepthBuffer(settings);
        writes[5] = AlphaBlending(settings, preset, OwnFix(settings));
        writes[6] = PixelTest(settings);
        writes[6].value &= ~Bit(settings, SettingAlphaTest);
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

    // ALPHA's value of each preset: 0 (Cs - Cd) As + Cd, 1 Cs As + Cd, 2 Cd - Cs As, 3 Cd As + Cd, 4 Cd - Cd As, 5 Cd As
    void InitAlphaPresets()
    {
        g_AlphaPresets[0] = 0x44;
        g_AlphaPresets[1] = 0x48;
        g_AlphaPresets[5] = 0x89;
        g_AlphaPresets[2] = 0x42;
        g_AlphaPresets[3] = 0x49;
        g_AlphaPresets[4] = 0x46;
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
        u64 settings = shader->settings;
        writes[0] = AddressDataTag(TexturedWrites);
        writes[1] = {0, GsPrmodeCont};
        writes[2] = {PrimitiveMode(settings, false), GsPrmode};
        shader->registers = Address(writes);
        writes[3] = FrameBufferAlpha(settings, InContext(settings, GsFba));
        writes[4] = DepthBuffer(settings);
        writes[5] = AlphaBlending(settings, g_AlphaPresets[settings >> SettingPreset & 0xF], OwnFix(settings));
        writes[6] = PixelTest(settings);
        u64 minifying = Unmipped(shader) ? Bit(settings, SettingLinear) : MipsMinify;
        writes[7] = {TextureFiltering(shader, Levels(shader), 1, minifying), InContext(settings, GsTex1)};
        writes[8] = {Clamped, InContext(settings, GsClamp)};
        shader->registerCount = TexturedWrites;
    }

    void ShaderType10Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, false, ScreenCopyTexture | ScreenCopyAlpha);
    }

    void ShaderType11Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, true, ScreenCopyTexture | ScreenCopyAlpha);
    }

    // The distortion's: its copy's alpha isn't taken
    void ShaderType18Settings(Shader* shader)
    {
        MakeScreenCopySettings(shader, true, ScreenCopyTexture);
    }

    // Phases of up to half a turn, or in mode 2 and up of up to 1
    void ShaderType17Settings(ClothShader* shader)
    {
        MakeSettings(shader, StandardStyle);
        for (u32 row = 0; row < 16; row++)
        {
            f32 range = shader->mode < 2 ? Pi : 1.0f;
            shader->phases[row].x = RandomSignedTimes(range);
            shader->phases[row].y = RandomSignedTimes(range);
            shader->phases[row].z = RandomSignedTimes(range);
        }
    }

    void ShaderType1ASettings(ClothShader* shader)
    {
        MakeSettings(shader, StandardStyle);
        for (u32 row = 0; row < 16; row++)
        {
            shader->phases[row].x = RandomSignedTimes(Pi);
            shader->phases[row].y = RandomSignedTimes(Pi);
            shader->phases[row].z = RandomSignedTimes(Pi);
            if (shader->mode >= 2)
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
        for (u32 row = 0; row < 16; row++)
        {
            shader->phases[row].x = RandomSignedTimes(Pi);
            shader->phases[row].y = RandomSignedTimes(Pi);
            shader->phases[row].z = RandomSignedTimes(Pi);
        }
    }
}
