#pragma once

#include "abi.h"
#include "common.h"

// The game's colours: RGBA bytes, which its colour class reads and writes as fractions (red, green and blue are 1 at 192, alpha
// at 128; a fraction is truncated into its byte)
namespace Colour
{
constexpr f32 ColourFull = 192.0f;
constexpr f32 AlphaFull = 128.0f;
// 1/192 and 1/128 as the game has them
constexpr f32 ColourScale = 0x1.555556p-8f;
constexpr f32 AlphaScale = 0.0078125f;

inline u8 Byte(u32 colour, u32 index)
{
    return static_cast<u8>(colour >> (index * 8));
}

inline f32 ColourFraction(u32 colour, u32 index)
{
    return static_cast<f32>(Byte(colour, index)) * ColourScale;
}

inline f32 AlphaFraction(u32 colour)
{
    return static_cast<f32>(Byte(colour, 3)) * AlphaScale;
}

inline u8 ColourByte(f32 fraction)
{
    return static_cast<u8>(static_cast<s32>(fraction * ColourFull));
}

inline u8 AlphaByte(f32 fraction)
{
    return static_cast<u8>(static_cast<s32>(fraction * AlphaFull));
}
}

extern "C"
{
    // The colour from fractions
    void ColourSet(u32* colour, f32 red, f32 green, f32 blue, f32 alpha) RETAIL_N32(FUN_00101168);
    // The colour of the fraction t of the way from one colour to another (the alpha weighed the other way round)
    void ColourLerp(f32 t, u32* colour, u32 from, u32 to) RETAIL_N32(FUN_0019ced8);
    // The colour's red, green and blue times a scale (its alpha kept); returns it
    u32 ColourScale(u32* colour, f32 scale) RETAIL_N32(FUN_001a0ee8);
    // The colour tinted by another (their fractions multiplied); returns it
    u32 ColourTint(u32* colour, u32 tint) RETAIL(FUN_001a0f80);
    // One of its bytes set from a fraction
    void ColourSetAlpha(u32* colour, f32 alpha) RETAIL_N32(FUN_00101678);
    void ColourSetRed(u32* colour, f32 red) RETAIL_N32(FUN_00101698);
    void ColourSetGreen(u32* colour, f32 green) RETAIL_N32(FUN_001016b8);
    void ColourSetBlue(u32* colour, f32 blue) RETAIL_N32(FUN_001016d8);
}
