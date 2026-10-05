#pragma once

#include "common.h"

#include <bit>
#include <libgs.h>

// GS register values several of the renderer's packets (and the movie player's) write, as the words libgs.h's structs make.
// Only .cpp files include this and libgs.h, never graphics.cpp: its gs_privileged.h has GS_SET_* macros that clash with
// libgs.h's. Values made with std::bit_cast are const, not constexpr: clang can't evaluate a bit_cast of bit-fields

// PRIM's sprites in the second context with UV coordinates: textured, textured and blended, untextured
const u64 TexturedSprite = std::bit_cast<u64>(GS_PRIM{.prim_type = GS_PRIM_SPRITE, .tme = 1, .fst = 1, .ctxt = 1});
const u64 BlendedSprite = std::bit_cast<u64>(GS_PRIM{.prim_type = GS_PRIM_SPRITE, .tme = 1, .abe = 1, .fst = 1, .ctxt = 1});
const u64 FlatSprite = std::bit_cast<u64>(GS_PRIM{.prim_type = GS_PRIM_SPRITE, .fst = 1, .ctxt = 1});
// TEST's depth test always passing, ZBUF's depth not written, FRAME's colour written without its alpha (FBMSK's top byte)
const u64 DepthAlways = std::bit_cast<u64>(GS_TEST{.ztest_enable = 1, .ztest_method = GS_ZBUFF_ALWAYS});
const u64 DepthNotWritten = std::bit_cast<u64>(GS_ZBUF{.update_mask = 1});
const u64 FrameColourOnly = std::bit_cast<u64>(GS_FRAME{.draw_mask = 0xFF000000});
// TEXA's alpha of 0x80 for 16 bit texels with their alpha bit set (TA1), SCISSOR's 1024 by 1024, TEX1's bilinear filtering,
// COLCLAMP clamping the colours
const u64 SecondAlpha = std::bit_cast<u64>(GS_TEXA{.alpha_1 = 0x80});
const u64 WholeScissor = std::bit_cast<u64>(GS_SCISSOR{.clip_x1 = 0x400, .clip_y1 = 0x400});
const u64 Bilinear = std::bit_cast<u64>(GS_TEX1{.mmag = GS_TEX_LINEAR, .mmin = GS_TEX_LINEAR});
const u64 ClampColours = std::bit_cast<u64>(GS_COLCLAMP{.clamp = 1});
// RGBAQ's 128 (the texture's colours as they are) at an alpha of 128 and of 0, both with a Q of 1.0f
const u64 White = std::bit_cast<u64>(GS_RGBAQ{.r = 0x80, .g = 0x80, .b = 0x80, .a = 0x80, .q = 1.0f});
const u64 GreyQOne = std::bit_cast<u64>(GS_RGBAQ{.r = 0x80, .g = 0x80, .b = 0x80, .q = 1.0f});
// TRXREG's 16 by 16 texels of a palette
const u64 PaletteSize = std::bit_cast<u64>(GS_TRXREG{.trans_w = 0x10, .trans_h = 0x10});
