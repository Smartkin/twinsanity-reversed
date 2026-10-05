#pragma once

#include "common.h"

struct Renderer;
struct RenderTargetDescription;
struct RenderView;
struct Vector2;

// The TV's shapes as the game has them: 4:3 (the one the game's places were made for) and 16:9
constexpr f32 NarrowAspect = 0x1.555556p+0f;
constexpr f32 WideAspect = 0x1.C71C72p+0f;

// The renderer's frames: the render buckets (the PS2's DMA chains) linked into one chain the platform presents
extern "C"
{
    // The chain of the frame's buckets
    extern u8 g_FrameChain[] RETAIL(G_MainGifDmaTag);
    // The buffer the next frame is built in (0 or 1)
    extern u8 g_RenderBuffer RETAIL(D_00309BCC);
    // The present of the movies' vertical blank handler: the frame sent from the interrupt and the movie's next picture queued
    void PresentMovieFrame() RETAIL(FUN_001a0448);

    // The display's size in pixels, and whether the TV is 16:9 (the options' screen setting)
    extern u32 g_DisplayWidth RETAIL(G_HorizontalResolution);
    extern u32 g_DisplayHeight RETAIL(G_VerticalResolution);
    extern u8 g_WidescreenTv RETAIL(D_0030AAC4);
    // The renderer's frame size in pixels (InitRenderer_'s)
    extern s16 g_RendererWidth RETAIL(D_0030AB08);
    extern s16 g_RendererHeight RETAIL(D_0030AB0A);
    // Where the screen starts on a PAL and an NTSC TV (none: the static initialisation's)
    extern Vector2 g_PalScreenOffset RETAIL(D_0030A808);
    extern Vector2 g_NtscScreenOffset RETAIL(D_0030A810);

    // A place made fit for the TV's shape about an anchor (a 16:9 TV squeezes it towards the anchor), and a size; in pixels of
    // the frame when inPixels
    void FitPlaceToScreen(u32 inPixels, const Vector2* anchor, Vector2* place) RETAIL(FUN_001a0730);
    void FitSizeToScreen(u32 inPixels, Vector2* size) RETAIL(FUN_001a07c0);

    // The game's colour table (colour.h's ColourIndex), and a colour of it
    extern u32 g_Colours[] RETAIL(G_Colors);
    void GetColor(u32* color, s32 index);
    // The frame's start at the head of the first render bucket, for the target: drawing set up for it, and it cleared to its
    // color and depth (when they aren't 0; the asm hands in bits it doesn't mask)
    void SetUpFrame(RenderTargetDescription* target, u32 clearColor, u32 clearDepth) RETAIL(FUN_0019f958);
    // The frame's scene drawn with the renderer's view, then the overlay
    void DrawRendererScene(Renderer* renderer) RETAIL(FUN_0019ba10);
    // The view the renderer draws the scene with, its aspect the TV's (4:3, or 16:9 on a widescreen TV)
    void SetRendererView(Renderer* renderer, RenderView* view) RETAIL(FUN_001a0d70);
}
