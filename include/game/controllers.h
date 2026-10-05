#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/array.h"
#include "game/clock.h"
#include "game/cutscenes.h"
#include "game/font.h"
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
    u32 unused6C;

    // Made for the controller (nothing set but its clear colour, the colour table's 8th, and its packet), a copy made of another
    // (its frame worked out, the platform's Platform::Graphics::SetUpRenderTarget) and destroyed
    static RenderTargetDescription* Construct(RenderTargetDescription* description, struct GameRendererController* controller)
        RETAIL(FUN_001a2458);
    static RenderTargetDescription* Copy(RenderTargetDescription* description, const RenderTargetDescription* other)
        RETAIL(FUN_001a24b0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001a2528);
};
CHECK_SIZE(RenderTargetDescription, 0x70);

class Font;
struct FontTexts;
struct QueuedShape;
struct GameRendererController;
struct RenderView;

// A renderer's flags: it draws (its overlay too), it clears its frame's colour and its depth when it has no sky to draw
union RendererFlags
{
    u32 value;
    struct
    {
        u32 draws : 1;
        u32 unused1 : 3;
        u32 clearsColour : 1;
        u32 clearsDepth : 1;
        u32 unused6 : 26;
    };

    // The bits' masks (a renderer starts with all three)
    enum Mask : u32
    {
        Draws = 0x1,
        ClearsColour = 0x10,
        ClearsDepth = 0x20,
    };
};
CHECK_SIZE(RendererFlags, 4);

// The renderer made at start-up (G_Renderer_): its controller, the view it draws the scene with, its render target, its flags,
// and the UI's 2D overlay queued for the frame (game/overlay.h): the colour shapes and texts are queued in (white leaves a
// shape's own colours), the texts queued per font, the font, scale and alignment texts are queued with, and the shapes queued
// in each of six layers (the first and the last of each)
struct Renderer
{
    static constexpr u32 OverlayLayers = 6;

    GameRendererController* controller;
    RenderView* view;
    RenderTargetDescription* target;
    RendererFlags flags;
    u32 colour;
    u32 unused14;
    PointerArray<FontTexts> texts;
    Font* font;
    Vector2 textScale;
    TextAlignment textAlignment;
    QueuedShape* layers[OverlayLayers];
    QueuedShape* layerEnds[OverlayLayers];

    // Made for a controller with its own copy of a render target: drawing, clearing its frame, nothing queued, texts in white
    // at a scale of 1 at the top left
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
    enum Slot : u32
    {
        BeginFrameSlot = 3,
        UpdateSlot = 4,
        BeforeDrawingSlot = 5,
        AfterDrawingSlot = 6,
        DrawRendererScenesSlot = 8,
        AfterGameRenderSlot = 9,
        FinishSceneSlot = 10,
        SetScreenOffsetSlot = 12,
        RenderSlot = 13,
        PresentSlot = 14,
        CreateRendererSlot = 18,
    };

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
    u32 None2() RETAIL(FUN_001a05c0);
    void ClearFrames() RETAIL(FUN_0019d250);
    void StepAnimations(TimeClock* clock) RETAIL(FUN_001a05c8);
    void CountCameraRenderers() RETAIL(FUN_0019d3f0);
    void DrawScenes() RETAIL(FUN_0019d578);
    void DrawOverlays() RETAIL(FUN_0019d6e0);
    void FinishSceneWithEffects() RETAIL(FUN_001a0608);
    void PresentOverlay() RETAIL(FUN_001a1270);
    void BaseSetScreenOffset(const Vector2* offset) RETAIL(FUN_0019d840);
    void MoveScreen(const Vector2* offset) RETAIL(FUN_001a06d0);
    Renderer* MakeRenderer(RenderTargetDescription* description, u32 unused) RETAIL(FUN_001a03f0);
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
        CallVirtual<void>(this, vtable, BeginFrameSlot);
    }

    void Update(TimeClock* clocks)
    {
        CallVirtual<void>(this, vtable, UpdateSlot, clocks);
    }

    void EndFrame()
    {
        CallVirtual<void>(this, vtable, BeforeDrawingSlot);
    }

    void AfterEndFrame()
    {
        CallVirtual<void>(this, vtable, AfterDrawingSlot);
    }

    void DrawRendererScenes()
    {
        CallVirtual<void>(this, vtable, DrawRendererScenesSlot);
    }

    void AfterGameRender()
    {
        CallVirtual<void>(this, vtable, AfterGameRenderSlot);
    }

    // The scene's frame finished (the movies' end does it without the screen effects)
    void FinishScene(u32 effects)
    {
        CallVirtual<void>(this, vtable, FinishSceneSlot, effects);
    }

    // The screen moved to an offset (the display's position follows it)
    void SetScreenOffset(const Vector2* offset)
    {
        CallVirtual<void>(this, vtable, SetScreenOffsetSlot, offset);
    }

    // Sends the frame's render buckets, waits for the vertical blank and starts the next frame's
    void Render()
    {
        CallVirtual<void>(this, vtable, RenderSlot);
    }

    // The same, the buckets sent unless told not to
    void Present(bool withoutSending)
    {
        CallVirtual<void>(this, vtable, PresentSlot, withoutSending);
    }

    // Its versions of those (game/renderer.cpp)
    void PresentFrame() RETAIL(Render);
    void PresentFrameUnlessHeld(bool held) RETAIL(FUN_001a0540);
    void EndScene(u32 effects) RETAIL(FUN_001a0638);

    // (The game passes 1, which the game's controller ignores)
    Renderer* CreateRenderer(RenderTargetDescription* description, u32 unused)
    {
        return CallVirtual<Renderer*>(this, vtable, CreateRendererSlot, description, unused);
    }
};
CHECK_OFFSET(GameRendererController, vtable, 0x1C);

// The cutscenes' player's bits: the tracks are placed in the origin's space, the next part was read, the first part was read (the
// cutscene can start), it waits for the music, it was stopped by a script (a part read then is let go of), the cutscene can be
// skipped, it's only music (it plays until the music ends) and the music was asked for (it starts once it's prepared)
union VideoControllerBits
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 relative : 1;
        u32 unused2 : 5;
        u32 nextPartRead : 1;
        u32 firstPartRead : 1;
        u32 waitingForMusic : 1;
        u32 stopped : 1;
        u32 skippable : 1;
        u32 musicOnly : 1;
        u32 musicAsked : 1;
        u32 unused14 : 18;
    };

    // The bits' masks: what the constructor clears (it sets relative)
    enum Mask : u32
    {
        Relative = 0x2,
        ConstructorCleared = 0x3FFF,
    };
};
CHECK_SIZE(VideoControllerBits, 4);

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

    static constexpr u32 ModelCount = 15;
    static constexpr u32 TrackCount = 32;
    // The frames waited from which the disc error shows
    static constexpr u32 DiscErrorFrames = 13;

    void* resourceManager;
    VideoControllerBits bits;
    u32 state;
    // The state it was paused in
    u32 pausedState;
    u32 waitedFrames;
    struct GameObject* object;
    InstanceContext* instance;
    u32 unused1C;
    Matrix4x4 origin;
    struct ChunkData* chunk;
    TimeClock* clock;
    // The hint shown while a cutscene plays and the disc error's title
    String skipHint;
    String discErrorTitle;
    u16 models[ModelCount];
    InstanceContext* modelInstances[ModelCount];
    u32 unusedDC;
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
    u8 unused152C[0x15B0 - 0x152C];

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

// The save code's bits: the operation running (0 none), the one asked for, the screen shown, the player's answer to it
// (SaveCode::Answer) with the save slot chosen by the first choice, the save slot the operation saves to or loads from, a save due
// once the game controller has handled a failure (set by the save code, cleared by a request) and that save under way (the game
// controller's)
union SaveCodeBits
{
    u32 value;
    struct
    {
        u32 operation : 4;
        u32 asked : 4;
        u32 screen : 8;
        u32 answer : 4;
        u32 chosen : 4;
        u32 targetSlot : 4;
        u32 saveDue : 1;
        u32 savingDue : 1;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(SaveCodeBits, 4);

// The save code's results: the save slot, set by the last operation (the game controller saves again after a save with it, and
// goes ahead with a load or a new game with it), and the card has no save of the game yet (a slot's save writes every file)
union SaveCodeResults
{
    u32 value;
    struct
    {
        u32 slot : 4;
        u32 flagged : 1;
        u32 fresh : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(SaveCodeResults, 4);

// The save code (the memory card's state machine; the save manager derives from it, g_SaveManager): its bits, its results, its
// name, its device and its vtable: 1 an operation asked of the device (with its message), 2 and 3 a screen of choices and of save
// slots shown, 4 the device's operation done, 5 the screen's frame while the device waits, 6 its operation finished (the results'
// flagged, whether a save is due), 7 the destructor, 8 the update with the global clock, 9 the drawing
struct SaveCode
{
    enum Slot : u32
    {
        AskSlot = 1,
        ShowChoicesSlot = 2,
        ShowSlotsSlot = 3,
        OperationDoneSlot = 4,
        WaitingSlot = 5,
        FinishSlot = 6,
        UpdateSlot = 8,
        RenderSlot = 9,
    };

    // The player's answers to a screen: none, the first choice (on the slots' screens a slot, the bits' chosen), back or no, the
    // third choice
    enum Answer : u32
    {
        AnswerNone = 0,
        AnswerFirst = 1,
        AnswerBack = 2,
        AnswerThird = 3,
    };

    SaveCodeBits bits;
    SaveCodeResults results;
    String name;
    // Its device (game/savedevice.h, the memory card): its flags' fileCount, the files besides the icons, are its save slots
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

    void Update(TimeClock* clock)
    {
        CallVirtual<void>(this, vtable, UpdateSlot, clock);
    }

    void Render(Renderer* renderer)
    {
        CallVirtual<void>(this, vtable, RenderSlot, renderer);
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
    extern SaveCode* g_SaveManager RETAIL(G_UnkStruct_5C0);
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
