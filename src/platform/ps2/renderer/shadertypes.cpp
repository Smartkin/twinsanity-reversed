#include "renderer.h"

#include "abi.h"
#include "game/memory.h"
#include "game/shaderanimation.h"
#include "platform/math.h"

// The shader types' vtable functions other than their programs and packets (shaders.cpp) and GS settings (shadersettings.cpp):
// the destructor, the size and place of the GS settings, and type 0's frame update. Every type has the same destructor and the
// same functions in slots 7 and 8

extern "C"
{
    extern const GccVTableEntry g_ShaderBaseVTable[] RETAIL(PrecompiledShaderBaseInterface_Methods);
}

namespace
{
// Back to the base class, its GS settings freed (once the frame's DMA is done with them), its texture and animation released
void DestroyShader(Shader* shader, u32 flags)
{
    shader->vtable = g_ShaderBaseVTable;
    FreeDeferred(GetHeapManager(), reinterpret_cast<void*>(shader->registers));
    if (shader->texture != nullptr)
    {
        ReleaseTexture(shader->texture);
    }

    if (shader->animation != nullptr)
    {
        DestroyShaderAnimation(shader->animation, DestroyAndFree);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(shader);
    }
}

// The GS settings' size in quadwords, and with a nonzero argument two more (the materials ask with 0)
u32 RegistersSize(const Shader* shader, u32 withMore)
{
    return withMore != 0 ? shader->registerCount + 2 : shader->registerCount;
}
}

extern "C"
{
    void ShaderType00Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9360);
    void ShaderType01Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d91e0);
    void ShaderType02Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d92a0);
    void ShaderType04Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d95a8);
    void ShaderType0ADestroy(Shader* shader, u32 flags) RETAIL(FUN_001d9968);
    void ShaderType0BDestroy(Shader* shader, u32 flags) RETAIL(FUN_001d9a28);
    void ShaderType0CDestroy(Shader* shader, u32 flags) RETAIL(FUN_001d9ae0);
    void ShaderType0DDestroy(Shader* shader, u32 flags) RETAIL(FUN_001da660);
    void ShaderType0EDestroy(Shader* shader, u32 flags) RETAIL(FUN_001dc220);
    void ShaderType0FDestroy(Shader* shader, u32 flags) RETAIL(FUN_001d9bb0);
    void ShaderType10Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9c80);
    void ShaderType11Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9d58);
    void ShaderType12Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9e20);
    void ShaderType13Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9ed8);
    void ShaderType14Destroy(Shader* shader, u32 flags) RETAIL(FUN_001dab50);
    void ShaderType15Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9f90);
    void ShaderType16Destroy(Shader* shader, u32 flags) RETAIL(FUN_001da050);
    void ShaderType17Destroy(Shader* shader, u32 flags) RETAIL(FUN_001dbca8);
    void ShaderType18Destroy(Shader* shader, u32 flags) RETAIL(FUN_001da158);
    void ShaderType19Destroy(Shader* shader, u32 flags) RETAIL(FUN_001da220);
    void ShaderType1ADestroy(Shader* shader, u32 flags) RETAIL(FUN_001db9f8);
    void ShaderType1BDestroy(Shader* shader, u32 flags) RETAIL(FUN_001da318);
    void ShaderType1CDestroy(Shader* shader, u32 flags) RETAIL(FUN_001db010);
    void ShaderType1EDestroy(Shader* shader, u32 flags) RETAIL(FUN_001da420);
    void ShaderType1FDestroy(Shader* shader, u32 flags) RETAIL(FUN_001da4e0);
    void ShaderType20Destroy(Shader* shader, u32 flags) RETAIL(FUN_001da5a0);
    void ShaderBaseDestroy(Shader* shader, u32 flags) RETAIL(FUN_001da9e0);
    void UnusedShaderDestroy(Shader* shader, u32 flags) RETAIL(FUN_001dc438);

    void ShaderType00Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType01Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType02Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType04Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0ADestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0BDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0CDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0DDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0EDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType0FDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType10Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType11Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType12Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType13Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType14Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType15Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType16Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType17Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType18Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType19Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType1ADestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType1BDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType1CDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType1EDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType1FDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType20Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderBaseDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void UnusedShaderDestroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    // The classes of types 3 and 5 to 9, which the shader factory never makes
    void ShaderType03Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9420);
    void ShaderType05Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d94e8);
    void ShaderType06Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9668);
    void ShaderType07Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d9728);
    void ShaderType08Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d97e8);
    void ShaderType09Destroy(Shader* shader, u32 flags) RETAIL(FUN_001d98a8);

    void ShaderType03Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType05Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType06Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType07Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType08Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    void ShaderType09Destroy(Shader* shader, u32 flags)
    {
        DestroyShader(shader, flags);
    }

    u32 ShaderType00RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9408);
    u32 ShaderType01RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9288);
    u32 ShaderType02RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9348);
    u32 ShaderType04RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9650);
    u32 ShaderType0ARegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9a10);
    u32 ShaderType0CRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9b98);
    u32 ShaderType0DRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da6f8);
    u32 ShaderType0ERegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001dc2c8);
    u32 ShaderType0FRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9c68);
    u32 ShaderType10RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9d40);
    u32 ShaderType12RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9ec8);
    u32 ShaderType13RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9f80);
    u32 ShaderType14RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001dabf8);
    u32 ShaderType15RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da038);
    u32 ShaderType16RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da108);
    u32 ShaderType17RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da140);
    u32 ShaderType18RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da210);
    u32 ShaderType19RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da2c8);
    u32 ShaderType1ARegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da300);
    u32 ShaderType1BRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da3d0);
    u32 ShaderType1CRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da408);
    u32 ShaderType1ERegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da4c8);
    u32 ShaderType1FRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da588);
    u32 ShaderType20RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001da648);
    u32 UnusedShaderRegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001DC4E0);
    // Types 0xB and 0x11: never more
    u32 ShaderType0BRegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9ad8);
    u32 ShaderType11RegistersSize(const Shader* shader, u32 withMore) RETAIL(FUN_001d9e18);

    u32 ShaderType00RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType01RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType02RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType04RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0ARegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0CRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0DRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0ERegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0FRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType10RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType12RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType13RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType14RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType15RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType16RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType17RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType18RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType19RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType1ARegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType1BRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType1CRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType1ERegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType1FRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType20RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 UnusedShaderRegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType03RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D94D0);
    u32 ShaderType05RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D9590);
    u32 ShaderType06RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D9710);
    u32 ShaderType07RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D97D0);
    u32 ShaderType08RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D9890);
    u32 ShaderType09RegistersSize(const Shader* shader, u32 withMore) RETAIL(func_001D9950);

    u32 ShaderType03RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType05RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType06RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType07RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType08RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType09RegistersSize(const Shader* shader, u32 withMore)
    {
        return RegistersSize(shader, withMore);
    }

    u32 ShaderType0BRegistersSize(const Shader* shader, u32)
    {
        return shader->registerCount;
    }

    u32 ShaderType11RegistersSize(const Shader* shader, u32)
    {
        return shader->registerCount;
    }

    // Slot 7: where the GS settings are
    u32 ShaderRegisters(const Shader* shader) RETAIL(GetShaderSettings);
    // Slot 8 of most types: the shader doesn't need the eye and the model's matrix (ReadRigidModel and the skins ask)
    u32 ShaderNeedsEye(const Shader* shader) RETAIL(FUN_001d9070);
    // Type 0's frame update: nothing to update
    u32 ShaderType00Update(Shader* shader) RETAIL(FUN_001d93f8);

    u32 ShaderRegisters(const Shader* shader)
    {
        return shader->registers;
    }

    u32 ShaderNeedsEye(const Shader*)
    {
        return 0;
    }

    u32 ShaderType00Update(Shader*)
    {
        return 0;
    }
}

// The frame updates (slot 3, given the frame's seconds): the UV scroll and the animation, and the cloth and wave shaders' waves
namespace
{
// Five and ten turns in radians
constexpr f32 FiveTurns = 0x1.F6A7A4p+4f;
constexpr f32 TenTurns = 0x1.F6A7A4p+5f;
// Type 0x1A's second wave on V turns 2.3 times as fast (2 on the others)
constexpr f32 SecondWaveV = 0x1.266666p+1f;
// The animation's colour as the shader's: bytes, alpha 127 at most
constexpr f32 ColourScale = 256.0f;
constexpr f32 AlphaScale = 127.0f;

// A phase going round 0 to 1
f32 Wrapped(f32* phase, f32 speed, f32 seconds)
{
    f32 value = *phase + speed * seconds;
    *phase = value;
    s32 whole = TruncateToInt(value);
    if (value < static_cast<f32>(whole))
    {
        whole--;
    }

    *phase -= static_cast<f32>(whole);
    return *phase;
}

// The sine or cosine of the phase's turn (retail truncates the phase as well and drops it)
f32 Waved(f32* phase, f32 speed, f32 seconds, bool cosine)
{
    *phase += speed * seconds;
    s32 angle;
    AngleFrom(&angle, *phase * TwoPi, AngleRadians);
    return cosine ? CosOfAngle(&angle) : SinOfAngle(&angle);
}

f32 Turned(f32 phase, f32 step, f32 limit, f32 back)
{
    f32 value = phase + step;
    return limit <= value ? value - back : value;
}

Vector4* AllocateWaves()
{
    return reinterpret_cast<Vector4*>(AllocDmaTags(&g_FrameBuckets, ShaderWaveRows, sizeof(Vector4)));
}

// Their step: the speed in turns a second, or in mode 2 in radians
f32 ClothStep(const ClothShader* shader, f32 seconds)
{
    return shader->mode == ClothRadians ? shader->speed * seconds : shader->speed * (seconds * TwoPi);
}
}

extern "C"
{
    u32 UpdateShader(Shader* shader, f32 seconds) RETAIL_N32(UpdateShaderScrollAndColor);
    u32 ShaderType17Update(ClothShader* shader, f32 seconds) RETAIL_N32(FUN_001d2848);
    u32 ShaderType1AUpdate(ClothShader* shader, f32 seconds) RETAIL_N32(FUN_001d1cf0);
    u32 ShaderType1CUpdate(WaveShader* shader, f32 seconds) RETAIL_N32(FUN_001cf450);

    // The scrolls by their modes (U's cosine lands in V's place), then the animation moved on: the animated scrolls take its
    // offset, and with bit 60 the shader its colour
    u32 UpdateShader(Shader* shader, f32 seconds)
    {
        switch (shader->settings.uScroll)
        {
        case ScrollWrapped:
            shader->scroll[0] = Wrapped(&shader->scrollPhases[0], shader->scrollSpeeds[0], seconds);
            break;
        case ScrollSine:
            shader->scroll[0] = Waved(&shader->scrollPhases[0], shader->scrollSpeeds[0], seconds, false);
            break;
        case ScrollCosine:
            shader->scroll[1] = Waved(&shader->scrollPhases[0], shader->scrollSpeeds[0], seconds, true);
            break;
        }

        switch (shader->settings.vScroll)
        {
        case ScrollWrapped:
            shader->scroll[1] = Wrapped(&shader->scrollPhases[1], shader->scrollSpeeds[1], seconds);
            break;
        case ScrollSine:
            shader->scroll[1] = Waved(&shader->scrollPhases[1], shader->scrollSpeeds[1], seconds, false);
            break;
        case ScrollCosine:
            shader->scroll[1] = Waved(&shader->scrollPhases[1], shader->scrollSpeeds[1], seconds, true);
            break;
        }

        ShaderAnimation* animation = shader->animation;
        if (animation == nullptr)
        {
            return 0;
        }

        AdvanceShaderAnimation(animation, seconds);
        if (shader->settings.uScroll == ScrollAnimated)
        {
            shader->scroll[0] = animation->uvOffset.x;
        }

        if (shader->settings.vScroll == ScrollAnimated)
        {
            shader->scroll[1] = animation->uvOffset.y;
        }

        if (shader->settings.animatedColour)
        {
            shader->shaderColour[0] = animation->colour.x * ColourScale;
            shader->shaderColour[1] = animation->colour.y * ColourScale;
            shader->shaderColour[2] = animation->colour.z * ColourScale;
            shader->shaderColour[3] = animation->colour.w * AlphaScale;
            SetShaderColourByte(shader);
        }

        return 0;
    }

    // Only the red, the rest 0: what reaches the VU1 programs
    void SetShaderColourByte(Shader* shader)
    {
        shader->colour[3] = 0;
        shader->colour[1] = 0;
        shader->colour[2] = 0;
        shader->colour[0] = static_cast<u8>(static_cast<s32>(shader->shaderColour[0]));
    }

    // Modes 0 and 1: the phases turned on (wrapped at half a turn) and the waves the sines of the first two (mode 1 the cosines)
    // and the cosine of the third (VU0 makes them); from mode 2 on the phases move toward random places by the speed (the
    // seconds up to 1) and are the waves themselves
    u32 ShaderType17Update(ClothShader* shader, f32 seconds)
    {
        Vector4* waves = AllocateWaves();
        shader->waves = Address(waves);
        f32 share = Platform::Math::Min(seconds, 1.0f);
        f32 step = ClothStep(shader, seconds);
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            Vector4& phase = shader->phases[row];
            if (shader->mode < ClothRadians)
            {
                f32 x = Turned(phase.x, step, Pi, TwoPi);
                f32 y = Turned(phase.y, step, Pi, TwoPi);
                f32 sinCos[4];
                Platform::Math::SinCos(x, y, sinCos);
                f32 z = Turned(phase.z, step, Pi, TwoPi);
                phase.x = x;
                phase.y = y;
                phase.z = z;
                f32 third[4];
                Platform::Math::SinCos(z, z, third);
                waves[row].x = shader->mode == ClothSines ? sinCos[0] : sinCos[1];
                waves[row].y = shader->mode == ClothSines ? sinCos[2] : sinCos[3];
                waves[row].z = third[1];
                continue;
            }

            f32 x = phase.x;
            f32 y = phase.y;
            f32 z = phase.z;
            f32 towardX = RandomSignedTimes(1.0f);
            f32 towardY = RandomSignedTimes(1.0f);
            f32 towardZ = RandomSignedTimes(1.0f);
            phase.x = shader->speed * share * (towardX - x) + x;
            phase.y = shader->speed * share * (towardY - y) + y;
            phase.z = shader->speed * share * (towardZ - z) + z;
            waves[row].x = phase.x;
            waves[row].y = phase.y;
            waves[row].z = phase.z;
        }

        UpdateShader(shader, seconds);
        return 0;
    }

    // The phases turned on (wrapped at five turns), the waves their sines (cosines from mode 1 on); mode 2 also turns the second
    // phases and multiplies each wave by the cosine of its second phase twice as large (V's 2.3 times)
    u32 ShaderType1AUpdate(ClothShader* shader, f32 seconds)
    {
        Vector4* waves = AllocateWaves();
        shader->waves = Address(waves);
        f32 step = ClothStep(shader, seconds);
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            Vector4& phase = shader->phases[row];
            phase.x = Turned(phase.x, step, FiveTurns, TenTurns);
            phase.y = Turned(phase.y, step, FiveTurns, TenTurns);
            phase.z = Turned(phase.z, step, FiveTurns, TenTurns);
            s32 angles[3];
            AngleFrom(&angles[0], phase.x, AngleRadians);
            AngleFrom(&angles[1], phase.y, AngleRadians);
            AngleFrom(&angles[2], phase.z, AngleRadians);
            if (shader->mode == ClothSines)
            {
                waves[row].x = SinOfAngle(&angles[0]);
                waves[row].y = SinOfAngle(&angles[1]);
                waves[row].z = SinOfAngle(&angles[2]);
            }
            else
            {
                waves[row].x = CosOfAngle(&angles[0]);
                waves[row].y = CosOfAngle(&angles[1]);
                waves[row].z = CosOfAngle(&angles[2]);
            }

            if (shader->mode != ClothRadians)
            {
                continue;
            }

            Vector4& second = shader->secondPhases[row];
            second.x = Turned(second.x, step, FiveTurns, TenTurns);
            second.y = Turned(second.y, step, FiveTurns, TenTurns);
            second.z = Turned(second.z, step, FiveTurns, TenTurns);
            AngleFrom(&angles[0], second.x, AngleRadians);
            AngleFrom(&angles[1], second.y, AngleRadians);
            AngleFrom(&angles[2], second.z, AngleRadians);
            angles[0] = static_cast<s32>(static_cast<f32>(angles[0]) + static_cast<f32>(angles[0]));
            angles[1] = static_cast<s32>(static_cast<f32>(angles[1]) * SecondWaveV);
            angles[2] = static_cast<s32>(static_cast<f32>(angles[2]) + static_cast<f32>(angles[2]));
            waves[row].x *= CosOfAngle(&angles[0]);
            waves[row].y *= CosOfAngle(&angles[1]);
            waves[row].z *= CosOfAngle(&angles[2]);
        }

        UpdateShader(shader, seconds);
        return 0;
    }

    // The phases turned on (wrapped at five turns) and the waves their sines; no scroll
    u32 ShaderType1CUpdate(WaveShader* shader, f32 seconds)
    {
        Vector4* waves = AllocateWaves();
        shader->waves = Address(waves);
        f32 step = shader->speed * (seconds * TwoPi);
        for (u32 row = 0; row < ShaderWaveRows; row++)
        {
            Vector4& phase = shader->phases[row];
            phase.x = Turned(phase.x, step, FiveTurns, TenTurns);
            phase.y = Turned(phase.y, step, FiveTurns, TenTurns);
            phase.z = Turned(phase.z, step, FiveTurns, TenTurns);
            s32 angles[3];
            AngleFrom(&angles[0], phase.x, AngleRadians);
            AngleFrom(&angles[1], phase.y, AngleRadians);
            AngleFrom(&angles[2], phase.z, AngleRadians);
            waves[row].x = SinOfAngle(&angles[0]);
            waves[row].y = SinOfAngle(&angles[1]);
            waves[row].z = SinOfAngle(&angles[2]);
        }

        return 0;
    }
}


extern "C"
{
    // Types 5 to 8 call the base's update (type 9 has it in its vtable), type 3 has none
    u32 ShaderType03Update(const Shader* shader) RETAIL(FUN_001d94b8);
    u32 ShaderType05Update(Shader* shader, f32 seconds) RETAIL_N32(FUN_001dd050);
    u32 ShaderType06Update(Shader* shader, f32 seconds) RETAIL_N32(FUN_001dcb58);
    u32 ShaderType07Update(Shader* shader, f32 seconds) RETAIL_N32(FUN_001dcc70);
    u32 ShaderType08Update(Shader* shader, f32 seconds) RETAIL_N32(FUN_001dcd88);

    u32 ShaderType03Update(const Shader*)
    {
        return 0;
    }

    u32 ShaderType05Update(Shader* shader, f32 seconds)
    {
        return UpdateShader(shader, seconds);
    }

    u32 ShaderType06Update(Shader* shader, f32 seconds)
    {
        return UpdateShader(shader, seconds);
    }

    u32 ShaderType07Update(Shader* shader, f32 seconds)
    {
        return UpdateShader(shader, seconds);
    }

    u32 ShaderType08Update(Shader* shader, f32 seconds)
    {
        return UpdateShader(shader, seconds);
    }
}

EABI_EXPORT(FUN_001dd050, ShaderType05Update);
EABI_EXPORT(FUN_001dcb58, ShaderType06Update);
EABI_EXPORT(FUN_001dcc70, ShaderType07Update);
EABI_EXPORT(FUN_001dcd88, ShaderType08Update);
EABI_EXPORT(UpdateShaderScrollAndColor, UpdateShader);
EABI_EXPORT(FUN_001d2848, ShaderType17Update);
EABI_EXPORT(FUN_001d1cf0, ShaderType1AUpdate);
EABI_EXPORT(FUN_001cf450, ShaderType1CUpdate);
