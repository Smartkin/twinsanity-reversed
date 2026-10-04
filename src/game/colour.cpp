#include "game/colour.h"

#include "game/math.h"

namespace
{
// The bytes set one at a time (the colour class's setters)
void SetBytes(u32* colour, f32 red, f32 green, f32 blue, f32 alpha)
{
    auto* bytes = reinterpret_cast<u8*>(colour);
    bytes[0] = Colour::ColourByte(red);
    bytes[1] = Colour::ColourByte(green);
    bytes[2] = Colour::ColourByte(blue);
    bytes[3] = Colour::AlphaByte(alpha);
}
}

// The header's constants the start-up sets for this file, which nothing reads: up (0, 1, 0, 1), 0 and 0.01 twice (one block of
// 16 bytes, its second word left alone), a black of half alpha
struct ColourConstants
{
    u32 zero;
    u32 unknown04;
    f32 small;
    f32 small2;
};

extern "C"
{
    extern Vector4 g_ColourUp RETAIL(D_0030AE80);
    extern ColourConstants g_ColourConstants RETAIL(D_0030A480);
    extern u32 g_ColourShade RETAIL(D_0030A490);
    // GCC 2.9x's initialisation function (for every priority) and the module's global constructor
    void InitColourModule(s32 initialise, s32 priority) RETAIL(FUN_001010e8);
    void ConstructColourModule() RETAIL(FUN_001016f8);
}

extern "C"
{
    void ColourSet(u32* colour, f32 red, f32 green, f32 blue, f32 alpha)
    {
        SetBytes(colour, red, green, blue, alpha);
    }

    void ColourLerp(f32 t, u32* colour, u32 from, u32 to)
    {
        f32 fromRed = Colour::ColourFraction(from, 0);
        f32 fromGreen = Colour::ColourFraction(from, 1);
        f32 fromBlue = Colour::ColourFraction(from, 2);
        f32 fromAlpha = Colour::AlphaFraction(from);
        f32 toRed = Colour::ColourFraction(to, 0);
        f32 toGreen = Colour::ColourFraction(to, 1);
        f32 toBlue = Colour::ColourFraction(to, 2);
        f32 toAlpha = Colour::AlphaFraction(to);
        f32 red = fromRed + (toRed - fromRed) * t;
        f32 green = fromGreen + (toGreen - fromGreen) * t;
        f32 blue = fromBlue + (toBlue - fromBlue) * t;
        f32 alpha = toAlpha * t + fromAlpha * (1.0f - t);
        SetBytes(colour, red, green, blue, alpha);
    }

    u32 ColourScale(u32* colour, f32 scale)
    {
        auto* bytes = reinterpret_cast<u8*>(colour);
        f32 red = scale * Colour::ColourFraction(*colour, 0);
        f32 green = scale * Colour::ColourFraction(*colour, 1);
        f32 blue = scale * Colour::ColourFraction(*colour, 2);
        bytes[0] = Colour::ColourByte(red);
        bytes[1] = Colour::ColourByte(green);
        bytes[2] = Colour::ColourByte(blue);
        return *colour;
    }

    u32 ColourTint(u32* colour, u32 tint)
    {
        f32 red = Colour::ColourFraction(tint, 0) * Colour::ColourFraction(*colour, 0);
        f32 green = Colour::ColourFraction(tint, 1) * Colour::ColourFraction(*colour, 1);
        f32 blue = Colour::ColourFraction(tint, 2) * Colour::ColourFraction(*colour, 2);
        f32 alpha = Colour::AlphaFraction(tint) * Colour::AlphaFraction(*colour);
        SetBytes(colour, red, green, blue, alpha);
        return *colour;
    }
}

extern "C"
{
    void ColourSetAlpha(u32* colour, f32 alpha)
    {
        reinterpret_cast<u8*>(colour)[3] = Colour::AlphaByte(alpha);
    }

    void ColourSetRed(u32* colour, f32 red)
    {
        reinterpret_cast<u8*>(colour)[0] = Colour::ColourByte(red);
    }

    void ColourSetGreen(u32* colour, f32 green)
    {
        reinterpret_cast<u8*>(colour)[1] = Colour::ColourByte(green);
    }

    void ColourSetBlue(u32* colour, f32 blue)
    {
        reinterpret_cast<u8*>(colour)[2] = Colour::ColourByte(blue);
    }
}

void InitColourModule(s32 initialise, s32 priority)
{
    constexpr s32 AllPriorities = 0xFFFF;
    constexpr f32 Small = Rounded(0.01);
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    g_ColourUp.x = 0.0f;
    g_ColourUp.w = 1.0f;
    g_ColourConstants.small2 = Small;
    g_ColourConstants.zero = 0;
    g_ColourUp.y = 1.0f;
    g_ColourUp.z = 0.0f;
    g_ColourConstants.small = Small;
    ColourSet(&g_ColourShade, 0.0f, 0.0f, 0.0f, 0.5f);
}

void ConstructColourModule()
{
    InitColourModule(1, 0xFFFF);
}

EABI_EXPORT(FUN_00101168, ColourSet);
EABI_EXPORT(FUN_00101678, ColourSetAlpha);
EABI_EXPORT(FUN_00101698, ColourSetRed);
EABI_EXPORT(FUN_001016b8, ColourSetGreen);
EABI_EXPORT(FUN_001016d8, ColourSetBlue);
EABI_EXPORT(FUN_001a0ee8, ColourScale);
EABI_EXPORT(FUN_0019ced8, ColourLerp);
