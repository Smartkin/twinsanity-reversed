#include "renderer.h"

#include "game/memory.h"
#include "game/graphicstables.h"
#include "game/shaderanimation.h"
#include "game/stream.h"

#include <libgs.h>

// The shader classes' making, reading and comparing: the base constructor, each type's set-up (its vtable's function 13, which
// gives the shader its type number), reader (14: the base's fields, some types' own before them), sameness (15, the base's 11
// with the type's own fields first; 16 and 12 their opposites, 11 of the types with fields of their own goes to 15) and the
// models' questions (8: whether the shader needs the eye and the model's matrix, 9 a flag of the instance block, 10 unasked);
// and the materials' reader, which makes the shaders

extern "C"
{
    extern const GccVTableEntry g_ShaderBaseVTable[] RETAIL(PrecompiledShaderBaseInterface_Methods);
    extern const GccVTableEntry g_ShaderType01VTable[] RETAIL(PrecompiledShader__Type_0x1_Methods);
    extern const GccVTableEntry g_ShaderType02VTable[] RETAIL(PrecompiledShader__Type_0x2_Methods);
    extern const GccVTableEntry g_ShaderType04VTable[] RETAIL(PrecompiledShader__Type_0x4_Methods);
    extern const GccVTableEntry g_ShaderType0AVTable[] RETAIL(PrecompiledShader__Type_0xA_Methods);
    extern const GccVTableEntry g_ShaderType0BVTable[] RETAIL(PrecompiledShader__Type_0xB_Methods);
    extern const GccVTableEntry g_ShaderType0CVTable[] RETAIL(PrecompiledShader__Type_0xC_Methods);
    extern const GccVTableEntry g_ShaderType0DVTable[] RETAIL(PrecompiledShader__Type_0xD_Methods);
    extern const GccVTableEntry g_ShaderType0FVTable[] RETAIL(PrecompiledShader__Type_0xF_Methods);
    extern const GccVTableEntry g_ShaderType10VTable[] RETAIL(PrecompiledShader__Type_0x10_Methods);
    extern const GccVTableEntry g_ShaderType11VTable[] RETAIL(PrecompiledShader__Type_0x11_Methods);
    extern const GccVTableEntry g_ShaderType12VTable[] RETAIL(PrecompiledShader__Type_0x12_Methods);
    extern const GccVTableEntry g_ShaderType13VTable[] RETAIL(PrecompiledShader__Type_0x13_Methods);
    extern const GccVTableEntry g_ShaderType14VTable[] RETAIL(PrecompiledShader__Type_0x14_Methods);
    extern const GccVTableEntry g_ShaderType15VTable[] RETAIL(PrecompiledShader__Type_0x15_Methods);
    extern const GccVTableEntry g_ShaderType16VTable[] RETAIL(PrecompiledShader__Type_0x16_Methods);
    extern const GccVTableEntry g_ShaderType17VTable[] RETAIL(PrecompiledShader__Type_0x17_Methods);
    extern const GccVTableEntry g_ShaderType19VTable[] RETAIL(PrecompiledShader__Type_0x19_Methods);
    extern const GccVTableEntry g_ShaderType1AVTable[] RETAIL(PrecompiledShader__Type_0x1A_Methods);
    extern const GccVTableEntry g_ShaderType1BVTable[] RETAIL(PrecompiledShader__Type_0x1B_Methods);
    extern const GccVTableEntry g_ShaderType1EVTable[] RETAIL(PrecompiledShader__Type_0x1E_Methods);
    extern const GccVTableEntry g_ShaderType1FVTable[] RETAIL(PrecompiledShader__Type_0x1F_Methods);
    extern const GccVTableEntry g_ShaderType20VTable[] RETAIL(PrecompiledShader__Type_0x20_Methods);
}

namespace
{
// The settings two equal shaders have the same (all but bits 31 and 57-63; 58-61 are compared last)
constexpr u64 ComparedSettings = 0x1FFFFFF7FFFFFFF;
constexpr u64 ComparedLastSettings = 0x3C00000000000000;
constexpr u32 AnimationSize = 0x40;
constexpr u32 NoTexture = 0;
// What the shader factory allocates of the screen copies and the cloth shaders (type 0x17's ends after its first amplitude),
// and the cloth shaders' defaults: sines, a turn a second, an amplitude of 0.1
constexpr u32 ScreenCopyShaderSize = 0x80;
constexpr u32 ClothShaderSize = 0x280;
constexpr u32 ClothShader2Size = 0x290;
constexpr f32 ClothSpeed = 1.0f;
constexpr f32 ClothAmplitude = 0x1.99999Ap-4f;

// The settings' fields in the order the files have them, a byte each
u8 ReadSettingField(Stream* stream)
{
    s8 value;
    stream->ReadS8(&value);
    return static_cast<u8>(value);
}

// A shader's vtable read from its place in it, also for none (retail reads address 0x6C then)
const GccVTableEntry* VTableOf(const Shader* shader)
{
    return *reinterpret_cast<const GccVTableEntry* const*>(reinterpret_cast<u32>(shader) + offsetof(Shader, vtable));
}

u32 Same(Shader* shader, Shader* other, u32 slot)
{
    return CallVirtual<u32>(shader, shader->vtable, slot, other);
}

u32 Opposite(u32 same)
{
    return (same ^ 1) & 0xFF;
}
}

extern "C"
{
    // A new shader keeps the rest of its settings from what the memory held
    Shader* ShaderConstruct(Shader* shader)
    {
        shader->vtable = g_ShaderBaseVTable;
        ShaderSettings& settings = shader->settings;
        settings.blends = 0;
        settings.alphaTest = 0;
        settings.destinationTest = 0;
        settings.depthTest = GS_ZBUFF_GEQUAL;
        settings.gouraud = 1;
        settings.textured = 0;
        settings.fog = 0;
        settings.secondContext = 0;
        settings.uScroll = ScrollNone;
        settings.vScroll = ScrollNone;
        settings.ownAlpha = 0;
        settings.linear = 1;
        settings.noFba = 0;
        settings.unused57 = 0;
        settings.antiAliased = 0;
        settings.noDepthWrites = 0;
        settings.animatedColour = 0;
        settings.hasAnimation = 0;
        shader->textureId = NoTexture;
        shader->texture = nullptr;
        shader->scroll[0] = 0.0f;
        shader->scroll[1] = 0.0f;
        shader->animation = nullptr;
        return shader;
    }

    void SetShaderTexture(Shader* shader, u32 id)
    {
        shader->textureId = id;
        if (id != 0)
        {
            shader->texture = g_TextureTable.Acquire(&id, nullptr);
        }
    }

    // The base's reader: the settings' fields, TEX1's K and L, the leftover bytes, the colour, the scrolls' phases and speeds, the
    // texture's ID and the type, the animation when the settings say so; then the texture taken, the colour's byte made and the
    // GS settings (the type's function 5)
    void ReadShader(Shader* shader, Stream* stream) RETAIL(ReadBasePrecompShader_);

    void ReadShader(Shader* shader, Stream* stream)
    {
        ShaderSettings& settings = shader->settings;
        settings.blends = ReadSettingField(stream);
        settings.preset = ReadSettingField(stream);
        settings.alphaTest = ReadSettingField(stream);
        settings.alphaMethod = ReadSettingField(stream);
        settings.alphaReference = ReadSettingField(stream);
        settings.alphaFail = ReadSettingField(stream);
        settings.destinationTest = ReadSettingField(stream);
        settings.destinationMode = ReadSettingField(stream);
        settings.depthTest = ReadSettingField(stream);
        settings.unused23 = ReadSettingField(stream);
        settings.gouraud = ReadSettingField(stream);
        settings.textured = ReadSettingField(stream);
        settings.stq = ReadSettingField(stream);
        settings.fog = ReadSettingField(stream);
        settings.secondContext = ReadSettingField(stream);
        settings.uScroll = ReadSettingField(stream);
        settings.vScroll = ReadSettingField(stream);
        settings.ownAlpha = ReadSettingField(stream);
        settings.alphaA = ReadSettingField(stream);
        settings.alphaB = ReadSettingField(stream);
        settings.alphaC = ReadSettingField(stream);
        settings.alphaD = ReadSettingField(stream);
        settings.alphaFix = ReadSettingField(stream);
        settings.linear = ReadSettingField(stream);
        settings.noFba = ReadSettingField(stream);
        settings.unused57 = ReadSettingField(stream);
        settings.antiAliased = ReadSettingField(stream);
        settings.noDepthWrites = ReadSettingField(stream);
        settings.animatedColour = ReadSettingField(stream);
        settings.hasAnimation = ReadSettingField(stream);

        stream->ReadU16(reinterpret_cast<u16*>(&shader->lodK));
        stream->ReadU16(reinterpret_cast<u16*>(&shader->lodL));
        stream->Read(shader->leftover, sizeof(shader->leftover), 1);
        stream->Read(shader->shaderColour, sizeof(shader->shaderColour), 1);
        stream->ReadF32(&shader->scrollPhases[0]);
        stream->ReadF32(&shader->scrollPhases[1]);
        stream->ReadF32(&shader->scrollSpeeds[0]);
        stream->ReadF32(&shader->scrollSpeeds[1]);
        stream->ReadS32(reinterpret_cast<s32*>(&shader->textureId));
        s32 type;
        stream->ReadS32(&type);
        shader->type = type;
        if (shader->settings.hasAnimation)
        {
            auto* animation = ConstructShaderAnimation(static_cast<ShaderAnimation*>(MemoryAllocate(AnimationSize)));
            shader->animation = animation;
            ReadShaderAnimation(animation, stream);
        }

        u32 id = shader->textureId;
        if (id != NoTexture)
        {
            shader->texture = g_TextureTable.Acquire(&id, nullptr);
        }

        SetShaderColourByte(shader);
        CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    }

    // The base's function 11: the same texture and type, leftover bytes and colour (but their fourth), settings (bits 58-61 last),
    // TEX1's K and L and scrolls, and no animations that differ
    u32 SameShaders(const Shader* shader, const Shader* other) RETAIL(FUN_001cd550);

    u32 SameShaders(const Shader* shader, const Shader* other)
    {
        u32 same = 0;
        if (shader->textureId == other->textureId && shader->type == other->type && shader->leftover[0] == other->leftover[0] &&
            shader->leftover[1] == other->leftover[1] && shader->leftover[2] == other->leftover[2] &&
            shader->shaderColour[0] == other->shaderColour[0] && shader->shaderColour[1] == other->shaderColour[1] &&
            shader->shaderColour[2] == other->shaderColour[2] &&
            (shader->settings.value & ComparedSettings) == (other->settings.value & ComparedSettings) &&
            shader->lodK == other->lodK && shader->lodL == other->lodL && shader->scrollPhases[0] == other->scrollPhases[0] &&
            shader->scrollPhases[1] == other->scrollPhases[1] && shader->scrollSpeeds[0] == other->scrollSpeeds[0] &&
            shader->scrollSpeeds[1] == other->scrollSpeeds[1])
        {
            same = ((shader->settings.value ^ other->settings.value) & ComparedLastSettings) == 0 ? 1 : 0;
        }

        if (shader->animation != nullptr && other->animation != nullptr &&
            DifferentShaderAnimations(shader->animation, other->animation))
        {
            same = 0;
        }

        return same;
    }

    // Function 12 of every type: the opposite of its 11
    u32 DifferentShaders(Shader* shader, Shader* other) RETAIL(FUN_001dab00);

    u32 DifferentShaders(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderSameSlot));
    }

    // Functions 9 and 10 of most types: no (the shader doesn't make its model a billboard, and the unasked one)
    u32 ShaderMakesBillboard(const Shader* shader) RETAIL(FUN_001d9078);
    u32 ShaderSlot10(const Shader* shader) RETAIL(FUN_001d9080);

    u32 ShaderMakesBillboard(const Shader*)
    {
        return 0;
    }

    u32 ShaderSlot10(const Shader*)
    {
        return 0;
    }

    // The set-ups: the type number
    void ShaderType00SetUp(Shader* shader) RETAIL(FUN_001d9400);
    void ShaderType01SetUp(Shader* shader) RETAIL(FUN_001d9278);
    void ShaderType02SetUp(Shader* shader) RETAIL(SetShaderType);
    void ShaderType04SetUp(Shader* shader) RETAIL(FUN_001d9640);
    void ShaderType0ASetUp(Shader* shader) RETAIL(FUN_001d9a00);
    void ShaderType0BSetUp(Shader* shader) RETAIL(FUN_001d9ac8);
    void ShaderType0CSetUp(Shader* shader) RETAIL(FUN_001d9b88);
    void ShaderType0DSetUp(Shader* shader) RETAIL(FUN_001da708);
    void ShaderType0ESetUp(Shader* shader) RETAIL(FUN_001dc2b8);
    void ShaderType0FSetUp(Shader* shader) RETAIL(FUN_001d9c58);
    void ShaderType10SetUp(ScreenCopyShader* shader) RETAIL(FUN_001d9d28);
    void ShaderType11SetUp(ScreenCopyShader* shader) RETAIL(FUN_001d9e00);
    void ShaderType12SetUp(Shader* shader) RETAIL(FUN_001d9eb8);
    void ShaderType13SetUp(Shader* shader) RETAIL(FUN_001d9f70);
    void ShaderType14SetUp(Shader* shader) RETAIL(FUN_001dabe8);
    void ShaderType15SetUp(Shader* shader) RETAIL(FUN_001da028);
    void ShaderType16SetUp(Shader* shader) RETAIL(FUN_001da0f8);
    void ShaderType17SetUp(ClothShader* shader) RETAIL(FUN_001da130);
    void ShaderType18SetUp(ScreenCopyShader* shader) RETAIL(FUN_001da1f8);
    void ShaderType19SetUp(Shader* shader) RETAIL(FUN_001da2b8);
    void ShaderType1ASetUp(ClothShader* shader) RETAIL(FUN_001da2f0);
    void ShaderType1BSetUp(Shader* shader) RETAIL(FUN_001da3c0);
    void ShaderType1CSetUp(WaveShader* shader) RETAIL(FUN_001da3f8);
    void ShaderType1DSetUp(Shader* shader) RETAIL(func_001DC4D0);
    void ShaderType1ESetUp(Shader* shader) RETAIL(FUN_001da4b8);
    void ShaderType1FSetUp(Shader* shader) RETAIL(FUN_001da578);
    void ShaderType20SetUp(Shader* shader) RETAIL(FUN_001da638);

    void ShaderType00SetUp(Shader* shader)
    {
        shader->type = 0;
    }

    void ShaderType01SetUp(Shader* shader)
    {
        shader->type = 0x1;
    }

    void ShaderType02SetUp(Shader* shader)
    {
        shader->type = 0x2;
    }

    void ShaderType04SetUp(Shader* shader)
    {
        shader->type = 0x4;
    }

    // The classes of types 3 and 5 to 9, which the factory never makes
    void ShaderType03SetUp(Shader* shader) RETAIL(func_001D94C0);
    void ShaderType05SetUp(Shader* shader) RETAIL(func_001D9580);
    void ShaderType06SetUp(Shader* shader) RETAIL(func_001D9700);
    void ShaderType07SetUp(Shader* shader) RETAIL(func_001D97C0);
    void ShaderType08SetUp(Shader* shader) RETAIL(func_001D9880);
    void ShaderType09SetUp(Shader* shader) RETAIL(func_001D9940);

    void ShaderType03SetUp(Shader* shader)
    {
        shader->type = 0x3;
    }

    void ShaderType05SetUp(Shader* shader)
    {
        shader->type = 0x5;
    }

    void ShaderType06SetUp(Shader* shader)
    {
        shader->type = 0x6;
    }

    void ShaderType07SetUp(Shader* shader)
    {
        shader->type = 0x7;
    }

    void ShaderType08SetUp(Shader* shader)
    {
        shader->type = 0x8;
    }

    void ShaderType09SetUp(Shader* shader)
    {
        shader->type = 0x9;
    }

    void ShaderType0ASetUp(Shader* shader)
    {
        shader->type = 0xa;
    }

    void ShaderType0BSetUp(Shader* shader)
    {
        shader->type = 0xb;
    }

    void ShaderType0CSetUp(Shader* shader)
    {
        shader->type = 0xc;
    }

    void ShaderType0DSetUp(Shader* shader)
    {
        shader->type = 0xd;
    }

    void ShaderType0ESetUp(Shader* shader)
    {
        shader->type = 0xe;
    }

    void ShaderType0FSetUp(Shader* shader)
    {
        shader->type = 0xf;
    }

    void ShaderType10SetUp(ScreenCopyShader* shader)
    {
        shader->type = 0x10;
    }

    void ShaderType11SetUp(ScreenCopyShader* shader)
    {
        shader->type = 0x11;
    }

    void ShaderType12SetUp(Shader* shader)
    {
        shader->type = 0x12;
    }

    void ShaderType13SetUp(Shader* shader)
    {
        shader->type = 0x13;
    }

    void ShaderType14SetUp(Shader* shader)
    {
        shader->type = 0x14;
    }

    void ShaderType15SetUp(Shader* shader)
    {
        shader->type = 0x15;
    }

    void ShaderType16SetUp(Shader* shader)
    {
        shader->type = 0x16;
    }

    void ShaderType17SetUp(ClothShader* shader)
    {
        shader->type = 0x17;
    }

    void ShaderType18SetUp(ScreenCopyShader* shader)
    {
        shader->type = 0x18;
    }

    void ShaderType19SetUp(Shader* shader)
    {
        shader->type = 0x19;
    }

    void ShaderType1ASetUp(ClothShader* shader)
    {
        shader->type = 0x1a;
    }

    void ShaderType1BSetUp(Shader* shader)
    {
        shader->type = 0x1b;
    }

    void ShaderType1CSetUp(WaveShader* shader)
    {
        shader->type = 0x1c;
    }

    void ShaderType1DSetUp(Shader* shader)
    {
        shader->type = 0x1d;
    }

    void ShaderType1ESetUp(Shader* shader)
    {
        shader->type = 0x1e;
    }

    void ShaderType1FSetUp(Shader* shader)
    {
        shader->type = 0x1f;
    }

    void ShaderType20SetUp(Shader* shader)
    {
        shader->type = 0x20;
    }

    // The types that answer yes to the models' questions
    u32 ShaderType0BNeedsEye(const Shader* shader) RETAIL(FUN_001d9ac0);
    u32 ShaderType0CNeedsEye(const Shader* shader) RETAIL(FUN_001d9b78);
    u32 ShaderType0CSlot10(const Shader* shader) RETAIL(FUN_001d9b80);
    u32 ShaderType0FNeedsEye(const Shader* shader) RETAIL(FUN_001d9c48);
    u32 ShaderType0FSlot10(const Shader* shader) RETAIL(FUN_001d9c50);
    u32 ShaderType10NeedsEye(const Shader* shader) RETAIL(FUN_001d9d18);
    u32 ShaderType10Slot10(const Shader* shader) RETAIL(FUN_001d9d20);
    u32 ShaderType11NeedsEye(const Shader* shader) RETAIL(FUN_001d9df0);
    u32 ShaderType11Slot10(const Shader* shader) RETAIL(FUN_001d9df8);
    u32 ShaderType16NeedsEye(const Shader* shader) RETAIL(FUN_001da0e8);
    u32 ShaderType16Slot10(const Shader* shader) RETAIL(FUN_001da0f0);
    u32 ShaderType18NeedsEye(const Shader* shader) RETAIL(FUN_001da1f0);
    u32 ShaderType1BNeedsEye(const Shader* shader) RETAIL(FUN_001da3b0);
    u32 ShaderType1BMakesBillboard(const Shader* shader) RETAIL(FUN_001da3b8);

    u32 ShaderType0BNeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType0CNeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType0CSlot10(const Shader*)
    {
        return 1;
    }

    u32 ShaderType0FNeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType0FSlot10(const Shader*)
    {
        return 1;
    }

    u32 ShaderType10NeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType10Slot10(const Shader*)
    {
        return 1;
    }

    u32 ShaderType11NeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType11Slot10(const Shader*)
    {
        return 1;
    }

    u32 ShaderType16NeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType16Slot10(const Shader*)
    {
        return 1;
    }

    u32 ShaderType18NeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType1BNeedsEye(const Shader*)
    {
        return 1;
    }

    u32 ShaderType1BMakesBillboard(const Shader*)
    {
        return 1;
    }

    // The readers
    void ShaderType00Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x0);
    void ShaderType01Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x1_);
    void ShaderType02Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x2_);
    void ShaderType04Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x4);
    void ShaderType0ARead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xA);
    void ShaderType0BRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xB);
    void ShaderType0CRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xC);
    void ShaderType0DRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xD);
    void ShaderType0ERead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xE);
    void ShaderType0FRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0xF);
    void ShaderType10Read(ScreenCopyShader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x10);
    void ShaderType11Read(ScreenCopyShader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x11);
    void ShaderType12Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x12);
    void ShaderType13Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x13_);
    void ShaderType14Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x14);
    void ShaderType15Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x15);
    void ShaderType16Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x16);
    void ShaderType17Read(ClothShader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x17);
    void ShaderType18Read(ScreenCopyShader* shader, Stream* stream) RETAIL(FUN_001dc740);
    void ShaderType19Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x19);
    void ShaderType1ARead(ClothShader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x1A);
    void ShaderType1BRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x1B);
    void ShaderType1CRead(WaveShader* shader, Stream* stream) RETAIL(FUN_001db2e8);
    void ShaderType1DRead(Shader* shader, Stream* stream) RETAIL(FUN_001dc548);
    void ShaderType1ERead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x1E);
    void ShaderType1FRead(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x1F);
    void ShaderType20Read(Shader* shader, Stream* stream) RETAIL(ReadPrecompShaderType_0x20);

    void ShaderType00Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType01Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType02Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType04Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType03Read(Shader* shader, Stream* stream) RETAIL(FUN_001db460);
    void ShaderType05Read(Shader* shader, Stream* stream) RETAIL(FUN_001dd030);
    void ShaderType06Read(Shader* shader, Stream* stream) RETAIL(FUN_001dcb38);
    void ShaderType07Read(Shader* shader, Stream* stream) RETAIL(FUN_001dcc50);
    void ShaderType08Read(Shader* shader, Stream* stream) RETAIL(FUN_001dcd68);
    void ShaderType09Read(Shader* shader, Stream* stream) RETAIL(FUN_001dce80);

    void ShaderType03Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType05Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType06Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType07Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType08Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType09Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0ARead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0BRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0CRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0DRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0ERead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType0FRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType10Read(ScreenCopyShader* shader, Stream* stream)
    {
        stream->ReadF32(&shader->cornerValue);
        ReadShader(shader, stream);
    }

    void ShaderType11Read(ScreenCopyShader* shader, Stream* stream)
    {
        stream->ReadF32(&shader->cornerValue);
        ReadShader(shader, stream);
    }

    void ShaderType12Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    // Its GS settings made twice: the first ones stay where they were made
    void ShaderType13Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
        CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    }

    void ShaderType14Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType15Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType16Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType17Read(ClothShader* shader, Stream* stream)
    {
        s32 mode;
        stream->ReadS32(&mode);
        stream->ReadF32(&shader->speed);
        stream->ReadF32(&shader->amplitudes[0]);
        shader->mode = mode;
        ReadShader(shader, stream);
    }

    void ShaderType18Read(ScreenCopyShader* shader, Stream* stream)
    {
        stream->ReadF32(&shader->cornerValue);
        ReadShader(shader, stream);
    }

    void ShaderType19Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType1ARead(ClothShader* shader, Stream* stream)
    {
        s32 mode;
        stream->ReadS32(&mode);
        stream->ReadF32(&shader->speed);
        stream->ReadF32(&shader->amplitudes[0]);
        stream->ReadF32(&shader->amplitudes[1]);
        stream->ReadF32(&shader->amplitudes[2]);
        shader->mode = mode;
        ReadShader(shader, stream);
    }

    void ShaderType1BRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType1CRead(WaveShader* shader, Stream* stream)
    {
        stream->ReadF32(&shader->speed);
        stream->ReadF32(&shader->amplitude);
        ReadShader(shader, stream);
    }

    void ShaderType1DRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType1ERead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType1FRead(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    void ShaderType20Read(Shader* shader, Stream* stream)
    {
        ReadShader(shader, stream);
    }

    // Sameness (function 15) and its opposite (16)
    u32 ShaderType00Same(Shader* shader, Shader* other) RETAIL(FUN_001db748);
    u32 ShaderType00Different(Shader* shader, Shader* other) RETAIL(FUN_001db768);
    u32 ShaderType01Same(Shader* shader, Shader* other) RETAIL(FUN_001dc3c8);
    u32 ShaderType01Different(Shader* shader, Shader* other) RETAIL(FUN_001dc3e8);
    u32 ShaderType02Same(Shader* shader, Shader* other) RETAIL(FUN_001db5e0);
    u32 ShaderType02Different(Shader* shader, Shader* other) RETAIL(FUN_001db600);
    u32 ShaderType04Same(Shader* shader, Shader* other) RETAIL(FUN_001dd3c8);
    u32 ShaderType04Different(Shader* shader, Shader* other) RETAIL(FUN_001dd3e8);
    u32 ShaderType0ESame(Shader* shader, Shader* other) RETAIL(FUN_001dc2e0);
    u32 ShaderType0EDifferent(Shader* shader, Shader* other) RETAIL(FUN_001dc300);
    u32 ShaderType10Same(ScreenCopyShader* shader, ScreenCopyShader* other) RETAIL(FUN_001dc660);
    u32 ShaderType10Different(Shader* shader, Shader* other) RETAIL(FUN_001dc698);
    u32 ShaderType11Same(ScreenCopyShader* shader, ScreenCopyShader* other) RETAIL(FUN_001dd2e8);
    u32 ShaderType11Different(Shader* shader, Shader* other) RETAIL(FUN_001dd320);
    u32 ShaderType12Same(Shader* shader, Shader* other) RETAIL(FUN_001db910);
    u32 ShaderType12Different(Shader* shader, Shader* other) RETAIL(FUN_001db930);
    u32 ShaderType13Same(Shader* shader, Shader* other) RETAIL(FUN_001dade0);
    u32 ShaderType13Different(Shader* shader, Shader* other) RETAIL(FUN_001dae00);
    u32 ShaderType14Same(Shader* shader, Shader* other) RETAIL(FUN_001dac18);
    u32 ShaderType14Different(Shader* shader, Shader* other) RETAIL(FUN_001dac38);
    u32 ShaderType15Same(Shader* shader, Shader* other) RETAIL(FUN_001db6c8);
    u32 ShaderType15Different(Shader* shader, Shader* other) RETAIL(FUN_001db6e8);
    u32 ShaderType16Same(Shader* shader, Shader* other) RETAIL(FUN_001dbfe0);
    u32 ShaderType16Different(Shader* shader, Shader* other) RETAIL(FUN_001dc000);
    u32 ShaderType17Same(ClothShader* shader, ClothShader* other) RETAIL(FUN_001dbd60);
    u32 ShaderType17Different(Shader* shader, Shader* other) RETAIL(FUN_001dbdc8);
    u32 ShaderType18Same(ScreenCopyShader* shader, ScreenCopyShader* other) RETAIL(FUN_001dc790);
    u32 ShaderType18Different(Shader* shader, Shader* other) RETAIL(FUN_001dc7c8);
    u32 ShaderType19Same(Shader* shader, Shader* other) RETAIL(FUN_001dbef8);
    u32 ShaderType19Different(Shader* shader, Shader* other) RETAIL(FUN_001dbf18);
    u32 ShaderType1ASame(ClothShader* shader, ClothShader* other) RETAIL(FUN_001dbab0);
    u32 ShaderType1ADifferent(Shader* shader, Shader* other) RETAIL(FUN_001dbb48);
    u32 ShaderType1BSame(Shader* shader, Shader* other) RETAIL(FUN_001dc1b0);
    u32 ShaderType1BDifferent(Shader* shader, Shader* other) RETAIL(FUN_001dc1d0);
    u32 ShaderType1CSame(WaveShader* shader, WaveShader* other) RETAIL(FUN_001db268);
    u32 ShaderType1CDifferent(Shader* shader, Shader* other) RETAIL(FUN_001db2b8);
    u32 ShaderType1DSame(Shader* shader, Shader* other) RETAIL(FUN_001dc4f8);
    u32 ShaderType1DDifferent(Shader* shader, Shader* other) RETAIL(FUN_001dc518);
    u32 ShaderType1ESame(Shader* shader, Shader* other) RETAIL(FUN_001dc0c8);
    u32 ShaderType1EDifferent(Shader* shader, Shader* other) RETAIL(FUN_001dc0e8);
    u32 ShaderType1FSame(Shader* shader, Shader* other) RETAIL(FUN_001db4f8);
    u32 ShaderType1FDifferent(Shader* shader, Shader* other) RETAIL(FUN_001db518);
    u32 ShaderType20Same(Shader* shader, Shader* other) RETAIL(FUN_001dd180);
    u32 ShaderType20Different(Shader* shader, Shader* other) RETAIL(FUN_001dd1a0);

    u32 ShaderType00Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType00Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType01Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType01Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType02Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType02Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType04Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType04Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    // Types 5 to 9 are never the same as another (type 3 has neither function)
    u32 ShaderType05Same(Shader* shader, Shader* other) RETAIL(FUN_001dcff8);
    u32 ShaderType05Different(Shader* shader, Shader* other) RETAIL(FUN_001dd000);
    u32 ShaderType06Same(Shader* shader, Shader* other) RETAIL(FUN_001dcb00);
    u32 ShaderType06Different(Shader* shader, Shader* other) RETAIL(FUN_001dcb08);
    u32 ShaderType07Same(Shader* shader, Shader* other) RETAIL(FUN_001dcc18);
    u32 ShaderType07Different(Shader* shader, Shader* other) RETAIL(FUN_001dcc20);
    u32 ShaderType08Same(Shader* shader, Shader* other) RETAIL(FUN_001dcd30);
    u32 ShaderType08Different(Shader* shader, Shader* other) RETAIL(FUN_001dcd38);
    u32 ShaderType09Same(Shader* shader, Shader* other) RETAIL(FUN_001dce48);
    u32 ShaderType09Different(Shader* shader, Shader* other) RETAIL(FUN_001dce50);

    u32 ShaderType05Same(Shader*, Shader*)
    {
        return 0;
    }

    u32 ShaderType05Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType06Same(Shader*, Shader*)
    {
        return 0;
    }

    u32 ShaderType06Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType07Same(Shader*, Shader*)
    {
        return 0;
    }

    u32 ShaderType07Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType08Same(Shader*, Shader*)
    {
        return 0;
    }

    u32 ShaderType08Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType09Same(Shader*, Shader*)
    {
        return 0;
    }

    u32 ShaderType09Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType0ESame(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType0EDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType10Same(ScreenCopyShader* shader, ScreenCopyShader* other)
    {
        if (shader->cornerValue != other->cornerValue)
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType10Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType11Same(ScreenCopyShader* shader, ScreenCopyShader* other)
    {
        if (shader->cornerValue != other->cornerValue)
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType11Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType12Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType12Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType13Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType13Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType14Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType14Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType15Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType15Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType16Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType16Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType17Same(ClothShader* shader, ClothShader* other)
    {
        if (shader->mode != other->mode || shader->speed != other->speed || shader->amplitudes[0] != other->amplitudes[0])
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType17Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType18Same(ScreenCopyShader* shader, ScreenCopyShader* other)
    {
        if (shader->cornerValue != other->cornerValue)
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType18Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType19Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType19Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1ASame(ClothShader* shader, ClothShader* other)
    {
        if (shader->mode != other->mode || shader->speed != other->speed || shader->amplitudes[0] != other->amplitudes[0] ||
            shader->amplitudes[1] != other->amplitudes[1] || shader->amplitudes[2] != other->amplitudes[2])
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1ADifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1BSame(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1BDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1CSame(WaveShader* shader, WaveShader* other)
    {
        if (shader->speed != other->speed || shader->amplitude != other->amplitude)
        {
            return 0;
        }

        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1CDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1DSame(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1DDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1ESame(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1EDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType1FSame(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType1FDifferent(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    u32 ShaderType20Same(Shader* shader, Shader* other)
    {
        return SameShaders(shader, other) != 0;
    }

    u32 ShaderType20Different(Shader* shader, Shader* other)
    {
        return Opposite(Same(shader, other, ShaderTypeSameSlot));
    }

    // Function 11 of the types with fields of their own: their 15 (type 0x11's calls its own 11 again, forever: nothing asks it)
    u32 ShaderType10SameAs(Shader* shader, Shader* other) RETAIL(FUN_001dc630);
    u32 ShaderType11SameAs(Shader* shader, Shader* other) RETAIL(FUN_001dd2b8);
    u32 ShaderType17SameAs(Shader* shader, Shader* other) RETAIL(FUN_001dbd30);
    u32 ShaderType1ASameAs(Shader* shader, Shader* other) RETAIL(FUN_001dba80);
    u32 ShaderType1CSameAs(Shader* shader, Shader* other) RETAIL(FUN_001db238);

    u32 ShaderType10SameAs(Shader* shader, Shader* other)
    {
        return Same(shader, other, ShaderTypeSameSlot) != 0;
    }

    u32 ShaderType11SameAs(Shader* shader, Shader* other)
    {
        return Same(shader, other, ShaderSameSlot) != 0;
    }

    u32 ShaderType17SameAs(Shader* shader, Shader* other)
    {
        return Same(shader, other, ShaderTypeSameSlot) != 0;
    }

    u32 ShaderType1ASameAs(Shader* shader, Shader* other)
    {
        return Same(shader, other, ShaderTypeSameSlot) != 0;
    }

    u32 ShaderType1CSameAs(Shader* shader, Shader* other)
    {
        return Same(shader, other, ShaderTypeSameSlot) != 0;
    }

    // Whether any of a material's shaders blends
    u32 MaterialBlends(const Material* material) RETAIL(FUN_001c0b50);

    u32 MaterialBlends(const Material* material)
    {
        u32 blends = 0;
        for (u32 index = 0; index < material->shaderCount; index++)
        {
            blends |= material->shaders[index]->settings.blends;
        }

        return blends;
    }
}

namespace
{
template <typename T>
T* MadeShader(u32 size, const GccVTableEntry* vtable)
{
    auto* shader = static_cast<T*>(MemoryAllocate(size));
    ShaderConstruct(shader);
    shader->vtable = vtable;
    return shader;
}

// A shader of a type the materials have, set up (none for the others: reading it then goes through address 0x6C)
Shader* MakeShader(s32 type)
{
    switch (type)
    {
    case 0x1:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType01VTable);
        ShaderType01SetUp(shader);
        return shader;
    }
    case 0x2:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType02VTable);
        ShaderType02SetUp(shader);
        return shader;
    }
    case 0x4:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType04VTable);
        ShaderType04SetUp(shader);
        return shader;
    }
    case 0xa:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType0AVTable);
        ShaderType0ASetUp(shader);
        return shader;
    }
    case 0xb:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType0BVTable);
        ShaderType0BSetUp(shader);
        return shader;
    }
    case 0xc:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType0CVTable);
        ShaderType0CSetUp(shader);
        return shader;
    }
    case 0xd:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType0DVTable);
        ShaderType0DSetUp(shader);
        return shader;
    }
    case 0xf:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType0FVTable);
        ShaderType0FSetUp(shader);
        return shader;
    }
    case 0x10:
    {
        auto* shader = MadeShader<ScreenCopyShader>(ScreenCopyShaderSize, g_ShaderType10VTable);
        ShaderType10SetUp(shader);
        return shader;
    }
    case 0x11:
    {
        auto* shader = MadeShader<ScreenCopyShader>(ScreenCopyShaderSize, g_ShaderType11VTable);
        ShaderType11SetUp(shader);
        return shader;
    }
    case 0x12:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType12VTable);
        ShaderType12SetUp(shader);
        return shader;
    }
    case 0x13:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType13VTable);
        ShaderType13SetUp(shader);
        return shader;
    }
    case 0x14:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType14VTable);
        ShaderType14SetUp(shader);
        return shader;
    }
    case 0x15:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType15VTable);
        ShaderType15SetUp(shader);
        return shader;
    }
    case 0x16:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType16VTable);
        ShaderType16SetUp(shader);
        return shader;
    }
    case 0x17:
    {
        auto* shader = MadeShader<ClothShader>(ClothShaderSize, g_ShaderType17VTable);
        shader->speed = ClothSpeed;
        shader->amplitudes[0] = ClothAmplitude;
        shader->mode = ClothSines;
        ShaderType17SetUp(shader);
        return shader;
    }
    case 0x19:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType19VTable);
        ShaderType19SetUp(shader);
        return shader;
    }
    case 0x1a:
    {
        auto* shader = MadeShader<ClothShader>(ClothShader2Size, g_ShaderType1AVTable);
        shader->amplitudes[2] = ClothAmplitude;
        shader->speed = ClothSpeed;
        shader->mode = ClothSines;
        shader->amplitudes[0] = ClothAmplitude;
        shader->amplitudes[1] = ClothAmplitude;
        ShaderType1ASetUp(shader);
        return shader;
    }
    case 0x1b:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType1BVTable);
        ShaderType1BSetUp(shader);
        return shader;
    }
    case 0x1e:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType1EVTable);
        ShaderType1ESetUp(shader);
        return shader;
    }
    case 0x1f:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType1FVTable);
        ShaderType1FSetUp(shader);
        return shader;
    }
    case 0x20:
    {
        auto* shader = MadeShader<Shader>(sizeof(Shader), g_ShaderType20VTable);
        ShaderType20SetUp(shader);
        return shader;
    }
    default:
        return nullptr;
    }
}
}

extern "C"
{
    // A material from its file: its programs' key, its bucket, a name (read and dropped), and its shaders. Shaders past the fourth
    // go over the count (which the loop reads again)
    void ReadMaterialShaders(Material* material, Stream* stream) RETAIL(ReadMaterialShader);

    void ReadMaterialShaders(Material* material, Stream* stream)
    {
        stream->ReadS64(reinterpret_cast<s64*>(&material->activatedShaders));
        s32 bucket;
        stream->ReadS32(&bucket);
        material->bucket = bucket;
        s32 nameLength;
        stream->ReadS32(&nameLength);
        if (nameLength != 0)
        {
            void* name = MemoryAllocate2(nameLength);
            stream->Read(name, nameLength, 1);
            MemoryDeallocate2_(name);
        }

        stream->ReadS32(reinterpret_cast<s32*>(&material->shaderCount));
        if (material->shaderCount != 0)
        {
            u32 index = 0;
            do
            {
                s32 type;
                stream->ReadS32(&type);
                Shader* shader = MakeShader(type);
                CallVirtual<void>(shader, VTableOf(shader), ShaderReadSlot, stream);
                material->shaders[index] = shader;
                index++;
            } while (index < material->shaderCount);
        }

        if (MaterialBlends(material) != 0)
        {
            material->call = CallSharedGifTag;
        }
    }
}
