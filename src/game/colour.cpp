#include "game/colour.h"

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

EABI_EXPORT(FUN_00101168, ColourSet);
EABI_EXPORT(FUN_001a0ee8, ColourScale);
EABI_EXPORT(FUN_0019ced8, ColourLerp);
