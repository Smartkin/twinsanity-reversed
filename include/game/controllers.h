#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/array.h"
#include "game/clock.h"
#include "game/math.h"
#include "game/pads.h"
#include "game/string.h"

// The engine's singletons Main makes, and the ones the frames drive. Their constructors are GCC 2.9x ones: they take the memory
// and return it. Their virtual functions are named by what the frame loop uses them for

// What a renderer is made from (0x70 bytes), and what it draws into: the frame's size and its offset from the screen's middle,
// the color it's cleared to (red, green, blue, alpha bytes) and the screen's size. The renderer works out from them the matrix
// from clip space to the drawing's coordinates (16ths of a pixel, the screen's middle at 2048) and where the frame's corner is
// in them (FUN_0019f720)
struct RenderTargetDescription
{
    u32 unknown00;
    s32 offsetX;
    s32 offsetY;
    s32 width;
    s32 height;
    u32 clearColor;
    s32 displayWidth;
    s32 displayHeight;
    Matrix4x4 clipToScreen;
    s32 x;
    s32 y;
    // A packet the renderer keeps (0x60 bytes)
    u8* packet;
    u32 unknown6C;

    static RenderTargetDescription* Construct(RenderTargetDescription* description, struct GameRendererController* controller)
        RETAIL(FUN_001a2458);
    void Destroy(u32 flags) RETAIL(FUN_001a2528);
};
CHECK_SIZE(RenderTargetDescription, 0x70);

class Font;
struct FontTexts;
struct QueuedShape;
struct GameRendererController;
struct RenderView;

// The renderer made at start-up (G_Renderer_): its controller, the view it draws the scene with, its render target, its flags
// (bit 0: it draws the frame's overlay), and the UI's 2D overlay queued for the frame (game/overlay.h): the colour shapes and
// texts are queued in (the colour table's 15th leaves a shape's own colours), the texts queued per font, the font, scale and
// flags texts are queued with, and the shapes queued in each of six layers (the first and the last of each)
struct Renderer
{
    static constexpr u32 OverlayLayers = 6;

    GameRendererController* controller;
    RenderView* view;
    RenderTargetDescription* target;
    u32 flags;
    u32 colour;
    u32 unknown14;
    PointerArray<FontTexts> texts;
    Font* font;
    Vector2 textScale;
    u32 textFlags;
    QueuedShape* layers[OverlayLayers];
    QueuedShape* layerEnds[OverlayLayers];
};
CHECK_OFFSET(Renderer, layerEnds, 0x50);

// Its vtable follows 0x1C bytes of members
struct GameRendererController
{
    u8 unknown00[0x14];
    // Where the screen is moved to (the options' screen position)
    Vector2 screenOffset;
    const GccVTableEntry* vtable;

    static GameRendererController* Construct(void* memory, s32 width, s32 height, bool pal) RETAIL(InitRenderer_);

    void BeginFrame()
    {
        CallVirtual<void>(this, vtable, 3);
    }

    void Update(TimeClock* clocks)
    {
        CallVirtual<void>(this, vtable, 4, clocks);
    }

    void EndFrame()
    {
        CallVirtual<void>(this, vtable, 5);
    }

    void AfterEndFrame()
    {
        CallVirtual<void>(this, vtable, 6);
    }

    void AfterGameRender()
    {
        CallVirtual<void>(this, vtable, 9);
    }

    // The scene's frame finished (the movies' end does it without the screen effects)
    void FinishScene(u32 effects)
    {
        CallVirtual<void>(this, vtable, 10, effects);
    }

    // The screen moved to an offset (the display's position follows it)
    void SetScreenOffset(const Vector2* offset)
    {
        CallVirtual<void>(this, vtable, 12, offset);
    }

    // Sends the frame's render buckets, waits for the vertical blank and starts the next frame's
    void Render()
    {
        CallVirtual<void>(this, vtable, 13);
    }

    // The same, the buckets sent unless told not to
    void Present(bool withoutSending)
    {
        CallVirtual<void>(this, vtable, 14, withoutSending);
    }

    // Its versions of those (game/renderer.cpp)
    void PresentFrame() RETAIL(Render);
    void PresentFrameUnlessHeld(bool held) RETAIL(FUN_001a0540);
    void EndScene(u32 effects) RETAIL(FUN_001a0638);

    Renderer* CreateRenderer(RenderTargetDescription* description, u32 unknown)
    {
        return CallVirtual<Renderer*>(this, vtable, 18, description, unknown);
    }
};

// The in-game video player (cutscenes): its state 8 bytes in (3 playing)
struct VideoController
{
    enum State : u32
    {
        StatePlaying = 3,
    };

    u8 unknown00[0x8];
    u32 state;
    u8 unknown0C[0x68 - 0xC];
    // The hint shown while a cutscene plays and the disc error's title
    String skipHint;
    String discErrorTitle;
    u8 unknown80[0xF8 - 0x80];
    // The camera cutscenes move (the game controller's)
    void* camera;

    void Pause() RETAIL(FUN_0029eed8);
    void Resume() RETAIL(FUN_0029eef0);
    void Update() RETAIL(FUN_0029ec60);
    // The cutscene skipped to its end (when it can be skipped)
    void Skip() RETAIL(FUN_0029b658);
    // Reset for a new way into the game (still asm)
    void Reset() RETAIL(FUN_0029e5a0);
};
CHECK_OFFSET(VideoController, skipHint, 0x68);
CHECK_OFFSET(VideoController, camera, 0xF8);

// The save code (the memory card's state machine, still asm; the save manager derives from it, G_UnkStruct_5C0): its bits (0-3
// the operation running, 0 none; 4-7 the one asked for; 28 and 29 the game controller's), its results (bit 4 set by the last
// operation: the game controller saves again after a save with it, and goes ahead with a load or a new game with it) and its
// vtable (8 the update with the global clock, 9 the drawing)
struct SaveCode
{
    enum Bits : u32
    {
        OperationMask = 0xF,
        // Set by the save code, cleared by a request: a save is due once the game controller has handled a failure
        SaveDue = 0x10000000,
        // The game controller's: that save is under way
        SavingDue = 0x20000000,
    };

    enum Results : u32
    {
        Flagged = 0x10,
    };

    u32 bits;
    u32 results;
    u8 unknown08[0x18 - 0x8];
    const GccVTableEntry* vtable;

    void Update(TimeClock* clock)
    {
        CallVirtual<void>(this, vtable, 8, clock);
    }

    void Render(Renderer* renderer)
    {
        CallVirtual<void>(this, vtable, 9, renderer);
    }
};

class GameMovieController;
struct ChunkLoadingManager;
struct GameResources;

extern "C"
{
    extern GameRendererController* G_GameRendererController;
    extern GameTimeController* G_GameClockController;
    extern PadControllerInterface* G_GamePadController;
    extern GameMovieController* G_GameMovieController;
    extern VideoController* G_VideoController;
    extern SaveCode* G_UnkStruct_5C0;
    extern Renderer* G_Renderer_;
    extern ChunkLoadingManager* G_ChunkLoadingManager_;
    extern GameResources* G_GameResourcesObjectPointer;

    // The video mode Main starts the renderer and the clock with: 512x512, PAL (50 frames a second, NTSC's 60)
    extern s32 g_ScreenWidth RETAIL(D_00309AE4);
    extern s32 g_ScreenHeight RETAIL(D_00309AE8);
    extern bool g_Pal RETAIL(D_00309AEC);

    // Makes the music system (game/sound.h's g_Music)
    void CreateMusic() RETAIL(FUN_001e6880);

    u32 ResourcesStep(GameResources* resources) RETAIL(FUN_002651a0);
}
