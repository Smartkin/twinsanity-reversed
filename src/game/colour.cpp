#include "game/colour.h"

#include "game/math.h"
#include "gcc2.h"

namespace
{
// The bytes set one at a time (the colour class's setters)
void SetBytes(u32* colour, f32 red, f32 green, f32 blue, f32 alpha)
{
    auto* rgba = reinterpret_cast<Rgba*>(colour);
    rgba->red = Colour::ColourByte(red);
    rgba->green = Colour::ColourByte(green);
    rgba->blue = Colour::ColourByte(blue);
    rgba->alpha = Colour::AlphaByte(alpha);
}
}

// The header's constants the start-up sets for this file, which nothing reads: up (0, 1, 0, 1), 0 and the UI's shadow offset
// (one block of 16 bytes, its second word left alone), the UI's shadow colour
struct ColourConstants
{
    u32 unused00;
    u32 unused04;
    f32 unused08;
    f32 unused0C;
};

extern "C"
{
    extern Vector4 g_ColourUp RETAIL(D_0030AE80);
    extern ColourConstants g_ColourConstants RETAIL(D_0030A480);
    extern u32 g_ColourShadowColour RETAIL(D_0030A490);
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
        Rgba start = {from};
        Rgba end = {to};
        f32 fromRed = Colour::ColourFraction(start.red);
        f32 fromGreen = Colour::ColourFraction(start.green);
        f32 fromBlue = Colour::ColourFraction(start.blue);
        f32 fromAlpha = Colour::AlphaFraction(start.alpha);
        f32 toRed = Colour::ColourFraction(end.red);
        f32 toGreen = Colour::ColourFraction(end.green);
        f32 toBlue = Colour::ColourFraction(end.blue);
        f32 toAlpha = Colour::AlphaFraction(end.alpha);
        f32 red = fromRed + (toRed - fromRed) * t;
        f32 green = fromGreen + (toGreen - fromGreen) * t;
        f32 blue = fromBlue + (toBlue - fromBlue) * t;
        f32 alpha = toAlpha * t + fromAlpha * (1.0f - t);
        SetBytes(colour, red, green, blue, alpha);
    }

    u32 ColourScale(u32* colour, f32 scale)
    {
        auto* rgba = reinterpret_cast<Rgba*>(colour);
        f32 red = scale * Colour::ColourFraction(rgba->red);
        f32 green = scale * Colour::ColourFraction(rgba->green);
        f32 blue = scale * Colour::ColourFraction(rgba->blue);
        rgba->red = Colour::ColourByte(red);
        rgba->green = Colour::ColourByte(green);
        rgba->blue = Colour::ColourByte(blue);
        return *colour;
    }

    u32 ColourTint(u32* colour, u32 tint)
    {
        Rgba tinting = {tint};
        const auto* own = reinterpret_cast<const Rgba*>(colour);
        f32 red = Colour::ColourFraction(tinting.red) * Colour::ColourFraction(own->red);
        f32 green = Colour::ColourFraction(tinting.green) * Colour::ColourFraction(own->green);
        f32 blue = Colour::ColourFraction(tinting.blue) * Colour::ColourFraction(own->blue);
        f32 alpha = Colour::AlphaFraction(tinting.alpha) * Colour::AlphaFraction(own->alpha);
        SetBytes(colour, red, green, blue, alpha);
        return *colour;
    }
}

extern "C"
{
    void ColourSetAlpha(u32* colour, f32 alpha)
    {
        reinterpret_cast<Rgba*>(colour)->alpha = Colour::AlphaByte(alpha);
    }

    void ColourSetRed(u32* colour, f32 red)
    {
        reinterpret_cast<Rgba*>(colour)->red = Colour::ColourByte(red);
    }

    void ColourSetGreen(u32* colour, f32 green)
    {
        reinterpret_cast<Rgba*>(colour)->green = Colour::ColourByte(green);
    }

    void ColourSetBlue(u32* colour, f32 blue)
    {
        reinterpret_cast<Rgba*>(colour)->blue = Colour::ColourByte(blue);
    }
}

void InitColourModule(s32 initialise, s32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_ColourUp.x = 0.0f;
    g_ColourUp.w = 1.0f;
    g_ColourConstants.unused0C = UiShadowOffset;
    g_ColourConstants.unused00 = 0;
    g_ColourUp.y = 1.0f;
    g_ColourUp.z = 0.0f;
    g_ColourConstants.unused08 = UiShadowOffset;
    ColourSet(&g_ColourShadowColour, 0.0f, 0.0f, 0.0f, UiShadowAlpha);
}

void ConstructColourModule()
{
    InitColourModule(1, DefaultInitPriority);
}

EABI_EXPORT(FUN_00101168, ColourSet);
EABI_EXPORT(FUN_00101678, ColourSetAlpha);
EABI_EXPORT(FUN_00101698, ColourSetRed);
EABI_EXPORT(FUN_001016b8, ColourSetGreen);
EABI_EXPORT(FUN_001016d8, ColourSetBlue);
EABI_EXPORT(FUN_001a0ee8, ColourScale);
EABI_EXPORT(FUN_0019ced8, ColourLerp);
