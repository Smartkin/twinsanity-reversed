#pragma once

#include "abi.h"
#include "common.h"

// The game's colours: RGBA bytes, which its colour class reads and writes as fractions (red, green and blue are 1 at 192, alpha
// at 128; a fraction is truncated into its byte)
union Rgba
{
    u32 value;
    struct
    {
        u32 red : 8;
        u32 green : 8;
        u32 blue : 8;
        u32 alpha : 8;
    };
};
CHECK_SIZE(Rgba, 4);

// The indexes of the game's colour table (renderer.h's g_Colours, which GetColor reads), named by their colours: eight colours
// without alpha, the same eight with alpha 1 (red, green and blue at 192), seven greys from light to dark (their bytes 184,
// 160, 120, 96, 72, 48 and 24) and black without alpha again (unused)
enum ColourIndex : s32
{
    ColourTransparentBlack = 0,
    ColourTransparentRed = 1,
    ColourTransparentGreen = 2,
    ColourTransparentBlue = 3,
    ColourTransparentYellow = 4,
    ColourTransparentCyan = 5,
    ColourTransparentMagenta = 6,
    ColourTransparentWhite = 7,
    ColourBlack = 8,
    ColourRed = 9,
    ColourGreen = 0xA,
    ColourBlue = 0xB,
    ColourYellow = 0xC,
    ColourCyan = 0xD,
    ColourMagenta = 0xE,
    ColourWhite = 0xF,
    ColourLightestGrey = 0x10,
    ColourLightGrey = 0x11,
    ColourLightMidGrey = 0x12,
    ColourGrey = 0x13,
    ColourDarkMidGrey = 0x14,
    ColourDarkGrey = 0x15,
    ColourDarkestGrey = 0x16,
    ColourLastTransparentBlack = 0x17,
};

// The UI's shadows (OLEG's widgets'): offset a hundredth of the screen across and down, black at half alpha. The game's header
// gave every file including it copies of them (and of an up vector, a 0 and 45 degrees), which the start-ups set and only OLEG
// reads
constexpr f32 UiShadowOffset = Rounded(0.01);
constexpr f32 UiShadowAlpha = 0.5f;

namespace Colour
{
constexpr f32 ColourFull = 192.0f;
constexpr f32 AlphaFull = 128.0f;
// 1/192 and 1/128 as the game has them
constexpr f32 ColourScale = 0x1.555556p-8f;
constexpr f32 AlphaScale = 0.0078125f;

// A red, green or blue byte as a fraction, and an alpha byte
inline f32 ColourFraction(u8 byte)
{
    return static_cast<f32>(byte) * ColourScale;
}

inline f32 AlphaFraction(u8 alpha)
{
    return static_cast<f32>(alpha) * AlphaScale;
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
