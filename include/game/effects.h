#pragma once

#include "common.h"

extern "C"
{
    // The screen's colour filter (the renderer's effect draws it): on (scripts switch it, and so does switching to a character
    // in a chunk), whether its two palettes are blended, the blend's amount, its palette and the second one (the chunks'
    // palettes)
    extern u8 g_ColourFilterOn RETAIL(D_00309C31);
    extern u8 g_ColourFilterBlends RETAIL(D_00309C32);
    extern f32 g_ColourFilterAmount RETAIL(D_00309C34);
    extern u32 g_ColourFilterPalette RETAIL(D_00309C38);
    extern u32 g_ColourFilterSecondPalette RETAIL(D_00309C3C);
}
