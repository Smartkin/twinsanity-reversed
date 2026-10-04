#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/array.h"
#include "game/clock.h"
#include "game/cutscenes.h"
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
    struct GameRendererController* controller;
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

    // Made for the controller (nothing set but its clear colour, the colour table's 8th, and its packet), a copy made of another
    // (its frame worked out, the platform's Platform::Graphics::SetUpRenderTarget) and destroyed
    static RenderTargetDescription* Construct(RenderTargetDescription* description, struct GameRendererController* controller)
        RETAIL(FUN_001a2458);
    static RenderTargetDescription* Copy(RenderTargetDescription* description, const RenderTargetDescription* other)
        RETAIL(FUN_001a24b0);
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

    // Bits of its flags: it draws (its overlay too), it clears its frame's colour and its depth when it has no sky to draw
    enum Flags : u32
    {
        FlagDraws = 0x1,
        FlagClearColour = 0x10,
        FlagClearDepth = 0x20,
    };

    // Made for a controller with its own copy of a render target: drawing, clearing its frame, nothing queued, texts in the
    // colour table's 15th colour at a scale of 1 with flags 0x11
    static Renderer* Construct(Renderer* renderer, struct GameRendererController* controller,
                               const RenderTargetDescription* target) RETAIL(FUN_001a09b0);
};
CHECK_OFFSET(Renderer, layerEnds, 0x50);
CHECK_SIZE(Renderer, 0x68);

// The renderers a controller makes (game/rendererpool.cpp): a pool of the game's handing out slots by index, like the sound
// code's (game/sound.h): how many it has room for, how many more it grows by (10), how many are in use, the first free one,
// each slot's link (the next free one; -1 one in use), the renderers and its vtable (1 the destructor)
struct RendererPool
{
    s16 capacity;
    s16 growth;
    s16 used;
    s16 freeHead;
    s16* links;
    Renderer** items;
    const GccVTableEntry* vtable;
};
CHECK_SIZE(RendererPool, 0x14);

extern "C"
{
    // Made empty and destroyed (its arrays freed), grown by its growth (both made even first: the slots kept, the new ones free
    // and first), a free slot taken (the pool grown first when it has none) and a renderer put into one (its index returned), and
    // the slot of the first renderer in use (none when only the last slot has one)
    RendererPool* RendererPoolConstruct(RendererPool* pool) RETAIL(FUN_001a6040);
    void RendererPoolDestroy(RendererPool* pool, u32 destroyFlags) RETAIL(FUN_001a26e0);
    void RendererPoolGrow(RendererPool* pool) RETAIL(FUN_0019fd50);
    s32 RendererPoolTake(RendererPool* pool) RETAIL(FUN_001a6600);
    s32 RendererPoolAdd(RendererPool* pool, Renderer* const* renderer) RETAIL(FUN_001a5fa8);
    Renderer** RendererPoolFirst(RendererPool* pool) RETAIL(FUN_001a6078);
}

// A walk over a pool's renderers (retail's RendererUtil_Methods over its base D_002F6360, 0xC bytes, made on the stack): its
// vtable (1 the destructor, 2 back to the first renderer, 3 whether it's done, 4 the slot it's at, 5 on to the next renderer, 6
// assigned, 7 the slot's index, 8 how many renderers the pool has), the slot it's at, how many renderers it passed and the pool
struct RendererWalk
{
    const GccVTableEntry* vtable;
    s16 index;
    s16 passed;
    RendererPool* pool;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001a10e8);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_001a1118);
    void First() RETAIL(FUN_001a1148);
    u32 IsDone() RETAIL(FUN_001a11c0);
    Renderer** Current() RETAIL(FUN_001a1258);
    void Next() RETAIL(FUN_001a11d8);
    RendererWalk* Assign(const RendererWalk* other) RETAIL(FUN_001a2910);
    s32 Index() RETAIL(FUN_001a2930);
    s32 Count() RETAIL(FUN_001a2938);
};
CHECK_SIZE(RendererWalk, 0xC);

extern "C"
{
    extern const GccVTableEntry g_RendererWalkVTable[] RETAIL(RendererUtil_Methods);
    extern const GccVTableEntry g_RendererWalkBaseVTable[] RETAIL(D_002F6360);
}

// Its vtable follows 0x1C bytes of members. The base class (D_002F68D0) has the slots the game's controller doesn't replace
struct GameRendererController
{
    RendererPool renderers;
    // Where the screen is moved to (the options' screen position)
    Vector2 screenOffset;
    const GccVTableEntry* vtable;

    // The renderer started for frames of a size, the screen moved to the TV's default place (none)
    static GameRendererController* Construct(void* memory, s32 width, s32 height, bool pal) RETAIL(InitRenderer_);
    // The base made (no renderers) and destroyed (its renderers deleted with their targets and their texts' queues, then its
    // pool)
    static GameRendererController* ConstructBase(GameRendererController* controller) RETAIL(FUN_001a10b0);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0019d088);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001a0598);
    // The vtable's functions: 2 (0), 3 every drawing renderer to clear its frame's colour and depth (between 5 and 6), 4 the
    // platform's animations stepped by the game's clock, 7 the drawing renderers whose view has a camera counted (into a global
    // nothing reads), 8 every renderer's scene drawn, 9 the drawing renderers' overlays drawn (the base's) or the scene finished
    // with its screen effects (the game's), 11 a frame of the first renderer's overlay alone presented, 12 the screen moved (each
    // way between -1 and 1; the game's moves the display with it), 18 a renderer made for a render target (its own copy of it)
    u32 Unknown2() RETAIL(FUN_001a05c0);
    void ClearFrames() RETAIL(FUN_0019d250);
    void StepAnimations(TimeClock* clock) RETAIL(FUN_001a05c8);
    void CountCameraRenderers() RETAIL(FUN_0019d3f0);
    void DrawScenes() RETAIL(FUN_0019d578);
    void DrawOverlays() RETAIL(FUN_0019d6e0);
    void FinishSceneWithEffects() RETAIL(FUN_001a0608);
    void PresentOverlay() RETAIL(FUN_001a1270);
    void BaseSetScreenOffset(const Vector2* offset) RETAIL(FUN_0019d840);
    void MoveScreen(const Vector2* offset) RETAIL(FUN_001a06d0);
    Renderer* MakeRenderer(RenderTargetDescription* description, u32 unknown) RETAIL(FUN_001a03f0);
    // The ones that do nothing: the game's 5, 6, 15, 16, 17, 19, 20 and 21, the base's 10 and 14
    void Nothing5() RETAIL(FUN_001a00e0);
    void Nothing6() RETAIL(FUN_001a00e8);
    void Nothing15() RETAIL(FUN_001a06a0);
    void Nothing16() RETAIL(FUN_001a06a8);
    void Nothing17() RETAIL(FUN_001a06b0);
    void Nothing19() RETAIL(FUN_001a06b8);
    void Nothing20() RETAIL(FUN_001a06c0);
    void Nothing21() RETAIL(FUN_001a06c8);
    void BaseNothing10() RETAIL(FUN_001a00d0);
    void BaseNothing14() RETAIL(FUN_001a00d8);

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

    void DrawRendererScenes()
    {
        CallVirtual<void>(this, vtable, 8);
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
CHECK_OFFSET(GameRendererController, vtable, 0x1C);

// The cutscenes' player (the video controller, 0x15B0 bytes; the cutscenes and its tracks are game/cutscenes.h's): its resource
// manager, its bits (GCC 2.9x's bit fields of the 64 bit word it starts, past the resource manager), its state, the frames it
// waited (13 and on the disc error shows), the object (its sounds the cutscene's) and the instance a cutscene was queued for (its
// persistent flag says the cutscene was seen), the origin the tracks are placed in, the chunk the cutscene's own instances are
// made in, the clock it plays by, the skip hint and the disc error's title, the instances it was given for models (15 at most),
// the clock index the instances it makes go by, its tracks, the part playing and the one being read, the cutscene playing and the
// next part, and where it is: the clock's time it started at and now, the part, the frame, the frame of the part and the share of
// the way to the next frame
struct VideoController
{
    enum State : u32
    {
        StateIdle = 0,
        StateQueued = 1,
        StateReady = 2,
        StatePlaying = 3,
        // Its clock stopped
        StatePaused = 4,
        StateFinished = 5,
    };

    enum Bits : u32
    {
        // The tracks are placed in the origin's space
        BitRelative = 0x2,
        BitNextPartRead = 0x80,
        // The first part was read: the cutscene can start
        BitFirstPartRead = 0x100,
        BitWaitingForMusic = 0x200,
        // Stopped by a script: a part read then is let go of
        BitStopped = 0x400,
        BitSkippable = 0x800,
        // The cutscene is only music: it plays until the music ends
        BitMusicOnly = 0x1000,
        // The music was asked for: it starts once it's prepared
        BitMusicAsked = 0x2000,
        // What the constructor clears (it sets BitRelative)
        ConstructorMask = 0x3FFF,
    };

    static constexpr u32 ModelCount = 15;
    static constexpr u32 TrackCount = 32;
    // The frames waited from which the disc error shows
    static constexpr u32 DiscErrorFrames = 13;

    void* resourceManager;
    u32 bits;
    u32 state;
    // The state it was paused in
    u32 pausedState;
    u32 waitedFrames;
    struct GameObject* object;
    InstanceContext* instance;
    u32 unknown1C;
    Matrix4x4 origin;
    struct ChunkData* chunk;
    TimeClock* clock;
    // The hint shown while a cutscene plays and the disc error's title
    String skipHint;
    String discErrorTitle;
    u16 models[ModelCount];
    InstanceContext* modelInstances[ModelCount];
    u32 unknownDC;
    u16 modelCount;
    u8 clockIndex;
    PlayedCameraTrack cameraTrack;
    PlayedInstanceTrack instanceTracks[TrackCount];
    PlayedEmitterTrack emitterTracks[TrackCount];
    PlayedSoundTrack soundTracks[TrackCount];
    s32 currentPart;
    u32 readPart;
    Cutscene* cutscene;
    // A cutscene and an object (destroyed through its vtable) the stop lets go of, which nothing sets
    Cutscene* unused150C;
    Cutscene* nextPart;
    void* unused1514;
    u32 startTime;
    u32 now;
    u16 part;
    u16 frame;
    u16 partFrame;
    f32 frameShare;
    u8 unknown152C[0x15B0 - 0x152C];

    // Paused (the state it was in kept) and resumed in it, and a frame of the state it's in
    void Pause() RETAIL(FUN_0029eed8);
    void Resume() RETAIL(FUN_0029eef0);
    void Update() RETAIL(FUN_0029ec60);
    // The cutscene skipped to its end (when it can be skipped)
    void Skip() RETAIL(FUN_0029b658);
    // Reset for a new way into the game: what plays stopped, nothing queued, no part played or read
    void Reset() RETAIL(FUN_0029e5a0);
};
CHECK_OFFSET(VideoController, origin, 0x20);
CHECK_OFFSET(VideoController, skipHint, 0x68);
CHECK_OFFSET(VideoController, models, 0x80);
CHECK_OFFSET(VideoController, modelInstances, 0xA0);
CHECK_OFFSET(VideoController, modelCount, 0xE0);
CHECK_OFFSET(VideoController, cameraTrack, 0xE4);
CHECK_OFFSET(VideoController, instanceTracks, 0x100);
CHECK_OFFSET(VideoController, emitterTracks, 0x900);
CHECK_OFFSET(VideoController, soundTracks, 0xF00);
CHECK_OFFSET(VideoController, currentPart, 0x1500);
CHECK_OFFSET(VideoController, startTime, 0x1518);
CHECK_OFFSET(VideoController, frameShare, 0x1528);
CHECK_SIZE(VideoController, 0x15B0);

// The save code (the memory card's state machine; the save manager derives from it, G_UnkStruct_5C0): its bits (0-3 the operation
// running, 0 none; 4-7 the one asked for; 8-15 the screen shown; 16-19 the player's answer to it, 1 the first choice (20-23 the
// save slot chosen), 2 back or no, 3 the third; 24-27 the save slot acted on; 28 and 29 the game controller's), its results (0-3
// the save slot; bit 4 set by the last operation: the game controller saves again after a save with it, and goes ahead with a load
// or a new game with it; bit 5 the card has no save of the game yet: a slot's save writes every file), its name, its device and
// its vtable: 1 an operation asked of the device (with its message), 2 and 3 a screen of choices and of save slots shown, 4 the
// device's operation done, 5 the screen's frame while the device waits, 6 its operation finished (bit 4 of the results, whether a
// save is due), 7 the destructor, 8 the update with the global clock, 9 the drawing
struct SaveCode
{
    enum Bits : u32
    {
        OperationMask = 0xF,
        AskedShift = 4,
        ScreenShift = 8,
        AnswerShift = 16,
        AnswerMask = 0xF0000,
        ChosenShift = 20,
        SlotShift = 24,
        // Set by the save code, cleared by a request: a save is due once the game controller has handled a failure
        SaveDue = 0x10000000,
        // The game controller's: that save is under way
        SavingDue = 0x20000000,
    };

    enum Results : u32
    {
        SlotMask = 0xF,
        Flagged = 0x10,
        Fresh = 0x20,
    };

    u32 bits;
    u32 results;
    String name;
    // Its card slots (game/savedevice.h): bits 0-3 of their flags, the files besides the icons, are its save slots
    class SaveDevice* device;
    const GccVTableEntry* vtable;

    static SaveCode* Construct(SaveCode* code, const char* name, SaveDevice* device) RETAIL(FUN_002a7eb8);
    // The base's functions of its vtable: the device's operation done, the screen's frame while the device waits (whether it took
    // it), its operation finished, a frame of it (the player's answer taken, the device stepped, then on to what's next)
    void OperationDone(u32 operation) RETAIL(FUN_002a1428);
    u32 Waiting(u32 screen) RETAIL(FUN_002a1910);
    void Finish(u32 flagged, u32 saveDue) RETAIL(FUN_002a7e58);
    void Frame(TimeClock* clock) RETAIL(FUN_002a1c58);
    // The player's answer to the screen taken
    void TakeAnswer() RETAIL(FUN_002a0fd0);
    // A message's text (the game text g_SaveMessageTexts gives it, or the save code's own English), and its text with the save's
    // size in kilobytes for "(x)" and the save code's name for "(xxx)"
    const char* Message(s32 message) RETAIL(FUN_002a7fa8);
    void MessageText(s32 message, String* text) RETAIL(FUN_002a1fb0);

    u32 Operation() const
    {
        return bits & OperationMask;
    }

    // The screen is the bits' second byte
    u32 Screen() const
    {
        return reinterpret_cast<const u8*>(&bits)[1];
    }

    void SetScreen(u32 screen)
    {
        reinterpret_cast<u8*>(&bits)[1] = static_cast<u8>(screen);
    }

    void Update(TimeClock* clock)
    {
        CallVirtual<void>(this, vtable, 8, clock);
    }

    void Render(Renderer* renderer)
    {
        CallVirtual<void>(this, vtable, 9, renderer);
    }
};
CHECK_SIZE(SaveCode, 0x1C);

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
