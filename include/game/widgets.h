#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/clock.h"
#include "game/font.h"
#include "game/math.h"
#include "game/particles2d.h"
#include "game/stream.h"
#include "game/string.h"

struct Renderer;
class WidgetEffect;
// A scale going round values (game/oleg.h): what a widget's sizes and a menu's selected item are scaled by
struct CyclingScale;

// The anchor in the middle of the screen (the x of Widget::anchor a constructor takes)
constexpr f32 AnchorMiddle = 0.5f;

// A widget's state (Widget::State) and the state asked for once one is, whether it's drawn, whether it's fitted to a 16:9 TV
// (its places closing in on its anchor, its sizes shrinking) and the overlay layer its shapes are queued in
union WidgetFlags
{
    u32 value;
    struct
    {
        u32 state : 4;
        u32 nextState : 4;
        u32 stateAsked : 1;
        u32 invisible : 1;
        u32 widescreen : 1;
        u32 layer : 4;
        u32 unused15 : 17;
    };
};
CHECK_SIZE(WidgetFlags, 4);

// The UI's widgets (their vtables the retail ones). A widget is hidden, appears over its duration, is shown and disappears over
// its duration, in turn: it holds hidden or shown for its hold time when it has one, and a state asked for takes effect at its
// next update. Widgets are chained: what a widget is told is passed on to the next one after it. The vtable's functions: what
// happens when it becomes hidden, starts appearing, becomes shown and starts disappearing, then every update in each of those
// states (given the time since the state began), the destructor, hidden at its next update, the game's frame begun (before the
// update), the update, the frame ended (once it's drawn) and the draw
class Widget
{
public:
    enum Slot : u32
    {
        EnterHiddenSlot = 1,
        StartAppearingSlot = 2,
        EnterShownSlot = 3,
        StartDisappearingSlot = 4,
        WhileHiddenSlot = 5,
        WhileAppearingSlot = 6,
        WhileShownSlot = 7,
        WhileDisappearingSlot = 8,
        DestroySlot = 9,
        HideSlot = 10,
        BeginFrameSlot = 11,
        UpdateSlot = 12,
        EndFrameSlot = 13,
        DrawSlot = 14,
    };

    enum State : u32
    {
        StateHidden = 1,
        StateAppearing = 2,
        StateShown = 3,
        StateDisappearing = 4,
    };

    WidgetFlags flags;
    // The point its places close in on (x the constructor's, y the middle)
    Vector2 anchor;
    // When its state began (clock units), how long it appears and disappears, how long it holds (0: until it's told), how far it
    // has appeared or disappeared
    s32 start;
    s32 duration;
    s32 hold;
    f32 progress;
    Widget* next;
    // An effect it plays (the sprite widgets' 2D particles), and the scale its sizes are multiplied by
    WidgetEffect* effect;
    const CyclingScale* scaler;
    // Its drop shadow: the colour it's tinted with and its offset (fractions of the screen, none when both are about 0)
    u32 shadowColour;
    Vector2 shadowOffset;
    const GccVTableEntry* vtable;

    // Without an anchor: no fitting to the TV's shape
    static Widget* Construct(Widget* widget) RETAIL(FUN_00255690);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025a760);
    void Hide() RETAIL(FUN_0025a830);
    void BeginFrame() RETAIL(FUN_0025a858);
    void Update(TimeClock* clock) RETAIL(FUN_002559e0);
    void EndFrame() RETAIL(FUN_0025a8c8);
    void Draw(Renderer* renderer) RETAIL(FUN_0025a890);
    // A place made fit for the TV when the widget is (pixels of the frame when inPixels), and a size, multiplied by the scaler's
    // scale too
    void FitPlace(u32 inPixels, Vector2* place) RETAIL(FUN_0025a790);
    void FitSize(u32 inPixels, Vector2* size) RETAIL(FUN_0025a7c0);
    // Appears over the duration (shown at once without one: clock units) and holds for the hold, and so does the chain after it:
    // shown it starts again, disappearing it turns back over the time it has disappeared for. Disappearing is the other way round
    void Appear(s32 newDuration, s32 newHold) RETAIL(FUN_00255738);
    void Disappear(s32 newDuration, s32 newHold) RETAIL(FUN_00255898);
    // The effect it plays, and the chain after it
    void SetEffect(WidgetEffect* newEffect) RETAIL(FUN_0025a1a8);

    u32 State() const
    {
        return flags.state;
    }

    // Asks for a state, which begins at the next update
    void Ask(u32 state)
    {
        flags.nextState = state;
        flags.stateAsked = 1;
    }
};
CHECK_SIZE(Widget, 0x38);

// A widget whose colour, place and size go from their hidden values to their shown ones as it appears and back as it disappears
// (colours RGBA bytes, places fractions of the screen)
class AnimatedWidget : public Widget
{
public:
    u32 colour;
    u32 colourFrom;
    u32 hiddenColour;
    u32 shownColour;
    Vector2 place;
    Vector2 placeFrom;
    Vector2 hiddenPlace;
    Vector2 shownPlace;
    Vector2 scale;
    Vector2 scaleFrom;
    Vector2 hiddenScale;
    Vector2 shownScale;

    static AnimatedWidget* Construct(AnimatedWidget* widget, f32 anchor) RETAIL_N32(FUN_00256120);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025a1e8);
    void EnterHidden() RETAIL(FUN_0025ad38);
    void StartAppearing() RETAIL(FUN_0025ad68);
    void EnterShown() RETAIL(FUN_0025ad98);
    void StartDisappearing() RETAIL(FUN_0025adc8);
    void WhileHidden(s32 elapsed) RETAIL(FUN_0025adf8);
    void WhileAppearing(s32 elapsed) RETAIL(FUN_0025ae28);
    void WhileShown(s32 elapsed) RETAIL(FUN_0025aed8);
    void WhileDisappearing(s32 elapsed) RETAIL(FUN_0025af08);

    // The ways it appears: fading in where it's shown (its colour's alpha from 0), sliding in from its place moved by an offset,
    // fading in as it grows from nothing at a place
    void FadeIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale) RETAIL(FUN_0025afb8);
    void SlideIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale, const Vector2* offset) RETAIL(FUN_0025b020);
    void GrowIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale, const Vector2* from) RETAIL(FUN_0025b0e8);
    // A rectangle widget's (its place and scale its corners) round its middle, its width a size divided by 4:3: fading in as it
    // grows from a point, sliding in from its place moved by an offset
    void GrowRectangle(u32 newColour, const Vector2* middle, const Vector2* size, const Vector2* from) RETAIL(FUN_00257578);
    void FadeRectangle(u32 newColour, const Vector2* middle, const Vector2* size) RETAIL(FUN_002572c8);
    void SlideRectangle(u32 newColour, const Vector2* middle, const Vector2* size, const Vector2* offset) RETAIL(FUN_002573f0);
};
CHECK_SIZE(AnimatedWidget, 0x88);

class MenuPage;
struct MenuItem;

// An animated widget that draws nothing, following a menu item (D_00303268): what draws for it reads its animated values, and
// whether the item is the one selected on its page
class ItemWidget : public AnimatedWidget
{
public:
    MenuPage* page;
    MenuItem* item;

    static ItemWidget* Construct(ItemWidget* widget, f32 anchor, MenuPage* page, MenuItem* item) RETAIL_N32(FUN_0025aca0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025d058);
    // Whether the page's selection for the player is the item
    u32 IsSelected(u32 player) RETAIL(FUN_0025acf8);
};
CHECK_SIZE(ItemWidget, 0x90);

// A text of the game's text table drawn in a font, in an alignment
class Label : public AnimatedWidget
{
public:
    Font* font;
    TextAlignment alignment;
    u32 text;

    static Label* Construct(Label* label, f32 anchor, Font* font, u32 text, u32 alignment) RETAIL_N32(FUN_0025a9a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025a978);
    void Draw(Renderer* renderer) RETAIL(FUN_00255d08);
};
CHECK_SIZE(Label, 0x94);

// A text of its own drawn in a font, in an alignment
class StringLabel : public AnimatedWidget
{
public:
    Font* font;
    TextAlignment alignment;
    String text;

    static StringLabel* Construct(StringLabel* label, f32 anchor, Font* font, u32 alignment) RETAIL_N32(FUN_0025b670);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025b538);
    void Draw(Renderer* renderer) RETAIL(FUN_00257088);
};
CHECK_SIZE(StringLabel, 0x9C);

class StringLabel;

// A line of text drawn in a font at a place (fractions of the screen), centred: not a widget, the UI manager draws it itself, or
// copies it into a label of its own (then it isn't drawn). Not drawn while it's empty. Text with a duration (clock units) is timed
// from the first update it has text at, the duration and the start dropped once it's past (the text stays)
struct TextLine
{
    Font* font;
    TextAlignment alignment;
    String text;
    u32 colour;
    Vector2 place;
    Vector2 scale;
    s32 start;
    s32 duration;
    StringLabel* label;
};
CHECK_SIZE(TextLine, 0x34);

// The credits' count of lines and the first line still shown
union CreditsRollBits
{
    u32 value;
    struct
    {
        u32 count : 12;
        u32 first : 12;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(CreditsRollBits, 4);

// The credits rolling up the screen (Language\Credits\<language>.txt in the game's font): its lines' count and the first still
// shown; the font, the text file in memory, its lines, the speed (screens a second) and a scale of it, a line's height, the
// text's size and where the first line shown is (a fraction of the screen from its top)
struct CreditsRoll
{
    CreditsRollBits bits;
    Font* font;
    MemoryStream text;
    const char** lines;
    f32 speed;
    f32 speedScale;
    f32 lineHeight;
    f32 textSize;
    f32 top;

    // The file read, its lines found, the roll starting below the screen
    static CreditsRoll* Construct(CreditsRoll* roll, Font* font, const char* path) RETAIL(FUN_0025cc98);
    void Destroy(u32 flags) RETAIL(FUN_0025cd38);
    // The text split into its lines, the line breaks made ends (blank lines aren't lines: with blank lines the last ones are
    // found past the text's end)
    void SplitLines(char* text) RETAIL(FUN_00259778);
    // Rolled on for the clock's frame, the first line dropped once it's above the screen. Returns whether there's still a line
    // on the screen
    u32 Update(TimeClock* clock) RETAIL(FUN_0025cd98);
    // The lines from the first shown down to the screen's bottom, in the middle
    void Draw(Renderer* renderer) RETAIL(FUN_0025ce48);
};
CHECK_SIZE(CreditsRoll, 0x34);

class MenuPage;
struct MenuInput;
struct MenuSounds;
struct PadButtons;
struct MenuDrawer;

// How a menu widget's menu goes: the leave action leaves it (its resume page isn't drawn), and a page left (but the resume page)
// is where it starts again
union MenuWidgetFlags
{
    u32 value;
    struct
    {
        u32 leaves : 1;
        u32 remembersPage : 1;
        u32 unused2 : 30;
    };
};
CHECK_SIZE(MenuWidgetFlags, 4);

// A widget showing a menu: while it's shown it polls the input from its pad (cleared otherwise), steps the current page for
// player 0 (from its home page when it has none), goes where the page leads, and draws the page shown with its drawer; hidden,
// it leaves its page. The game's frame's beginning and end are passed on to its page
class MenuWidget : public AnimatedWidget
{
public:
    MenuWidgetFlags menuFlags;
    // The page it was given to start from, and the one it starts from
    MenuPage* firstPage;
    MenuPage* home;
    PadButtons* pad;
    MenuInput* input;
    MenuDrawer* drawer;
    MenuSounds* sounds;
    MenuPage* current;
    MenuPage* shown;

    static MenuWidget* Construct(MenuWidget* widget, f32 anchor, MenuInput* input, MenuDrawer* drawer, MenuSounds* sounds)
        RETAIL_N32(FUN_0025ab38);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025aaf8);
    void BeginFrame() RETAIL(FUN_0025abe0);
    void Update(TimeClock* clock) RETAIL(FUN_00255e50);
    void EndFrame() RETAIL(FUN_0025ac40);
    void Draw(Renderer* renderer) RETAIL(FUN_00256018);
};
CHECK_OFFSET(MenuWidget, firstPage, 0x8C);
CHECK_OFFSET(MenuWidget, current, 0xA4);
CHECK_SIZE(MenuWidget, 0xAC);

class Sprite;


// The animated widget's code again (another class of the game's) with other defaults: its place and scale are a rectangle's two
// corners in fractions of the screen, the whole screen by default; it lets go of its effect once it's hidden
class RectangleWidget : public AnimatedWidget
{
public:
    static RectangleWidget* Construct(RectangleWidget* widget, f32 anchor) RETAIL_N32(FUN_002571b0);
    static RectangleWidget* Construct(RectangleWidget* widget) RETAIL(FUN_0025b970);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025b3b0);
    void EnterHidden() RETAIL(FUN_0025b6e0);
    void StartAppearing() RETAIL(FUN_0025b710);
    void EnterShown() RETAIL(FUN_0025b740);
    void StartDisappearing() RETAIL(FUN_0025b770);
    void WhileHidden(s32 elapsed) RETAIL(FUN_0025b7a0);
    void WhileAppearing(s32 elapsed) RETAIL(FUN_0025b7e0);
    void WhileShown(s32 elapsed) RETAIL(FUN_0025b890);
    void WhileDisappearing(s32 elapsed) RETAIL(FUN_0025b8c0);
};

// A sprite stretched across the widget's rectangle (the middle of it when the sprite is turned), with a drop shadow one layer
// below it, its size pulsing, bobbing up and down and wobbling (turning to and fro, in radians) by sines of times that loop over
// their periods (each off while its period isn't above 0), and the particles of an emitter and of its effect
class SpriteWidget : public RectangleWidget
{
public:
    Sprite* sprite;
    Emitter2D* emitter;
    f32 pulseTime;
    f32 pulsePeriod;
    f32 pulseAmount;
    f32 bobTime;
    f32 bobPeriod;
    f32 bobBase;
    f32 bobAmount;
    f32 wobbleTime;
    f32 wobblePeriod;
    f32 wobbleBase;
    f32 wobbleAmount;

    static SpriteWidget* Construct(SpriteWidget* widget, f32 anchor, Sprite* sprite) RETAIL_N32(FUN_0025b4c0);
    static SpriteWidget* Construct(SpriteWidget* widget, Sprite* sprite) RETAIL(FUN_0025b440);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025b400);
    void Update(TimeClock* clock) RETAIL(FUN_00256e78);
    void Draw(Renderer* renderer) RETAIL(FUN_00256788);
};
CHECK_SIZE(SpriteWidget, 0xBC);

// A picture made of eight sprites in two rows of four, which cut the widget's place in eight
class TiledPicture : public AnimatedWidget
{
public:
    Sprite* tiles;

    static TiledPicture* Construct(TiledPicture* picture, f32 anchor, Sprite* tiles) RETAIL_N32(FUN_0025a250);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025a228);
    void Draw(Renderer* renderer) RETAIL(FUN_00255450);
};
CHECK_SIZE(TiledPicture, 0x8C);

class Ring;
class Shape2D;
struct Material;

// The UI's controllers' base (its vtable the retail one, after 0x308 bytes: 1 the destructor, 2 every widget hidden at its next
// update, 3 and 6 the game's frame begun and ended for every widget, 4 every widget updated, 5 every widget drawn): 64 widget
// slots that 64 bit masks pick, which the locked ones are kept out of, and the screens' masks (OLEG::Screen: the widgets each
// shows)
class WidgetController
{
public:
    static constexpr u32 Slots = 64;

    u64 locked;
    u64 screens[Slots];
    Widget* widgets[Slots];
    const GccVTableEntry* vtable;

    static WidgetController* Construct(WidgetController* controller) RETAIL(FUN_0025a4b8);
    void Destroy(u32 flags) RETAIL(FUN_0025a510);
    void SetWidget(u32 index, Widget* widget) RETAIL(FUN_0025a540);
    void HideAll() RETAIL(FUN_0025a550);
    void BeginFrame() RETAIL(FUN_0025a5b0);
    void Update(TimeClock* clock) RETAIL(FUN_0025a610);
    void Draw(Renderer* renderer) RETAIL(FUN_0025a680);
    void EndFrame() RETAIL(FUN_0025a6f0);
    // The widgets of the bits (but the locked ones) appear or disappear over the duration (clock units) and hold for the hold
    void Show(u64 bits, s32 duration, s32 hold) RETAIL(FUN_0025a378);
    void Hide(u64 bits, s32 duration, s32 hold) RETAIL(FUN_0025a418);
    // The slot of a mask's highest bit (-1: none)
    s32 IndexOf(u64 bits) RETAIL(FUN_0025a348);
    // The effect of the widget of the bit (there has to be one) and of the chain after it
    void SetEffect(u64 bit, WidgetEffect* effect) RETAIL(FUN_0025a2e8);
};
CHECK_SIZE(WidgetController, 0x310);

// A ring widget's rings stand apart: every one is inside nothing but its middle
union RingWidgetFlags
{
    u8 value;
    struct
    {
        u8 standApart : 1;
        u8 unused1 : 7;
    };
};
CHECK_SIZE(RingWidgetFlags, 1);

// A widget of rings (made by AddRing, its own), each inside the next unless they stand apart: the first inside the last ring of
// the widget it's inside, when it has one. They're drawn round its place, their radii its scale (fractions of the screen), in
// their own colours (the widget's colour tints them), with a drop shadow one layer below
class RingWidget : public AnimatedWidget
{
public:
    u8 count;
    u8 segments;
    u8 steps;
    RingWidgetFlags ringFlags;
    RingWidget* inside;
    Ring** rings;

    static RingWidget* Construct(RingWidget* widget, f32 anchor, u32 count, u32 segments, u32 steps, RingWidget* inside)
        RETAIL_N32(FUN_0025b190);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025b248);
    // A ring of the material at the index (each ring's points: its segments' steps and their ends)
    Ring* AddRing(u32 index, Material* material) RETAIL(FUN_0025b2f8);
    // The point a fraction t of the way round the first ring, shown (fractions of the screen)
    void PointAt(Vector2* out, f32 t) RETAIL_N32(FUN_0025b380);
    void Update(TimeClock* clock) RETAIL(FUN_00256378);
    void Draw(Renderer* renderer) RETAIL(FUN_00256518);
};
CHECK_SIZE(RingWidget, 0x94);

// A widget drawing a shape across its place and scale. Nothing makes one: only its vtable is left, after the ring widget's
class ShapeWidget : public AnimatedWidget
{
public:
    Shape2D* shape;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0025b158);
    void Draw(Renderer* renderer) RETAIL(FUN_00256238);
};
CHECK_SIZE(ShapeWidget, 0x8C);

// A pad moving an instance around (0xC bytes; its vtable after its members, the derived class's D_00302858 over the base's
// D_00302878: 1 the destructor, 2 its frame, the same in both). Nothing makes one: the bindings it reads a pad with, the instance
// (released with it)
struct PadInstanceMover
{
    struct ButtonBindings* bindings;
    struct InstanceContext* instance;
    const GccVTableEntry* vtable;

    // The derived class's destructor and the base's (the instance released)
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025cc00);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0025cc30);
    // Its frame (over some seconds, with a pad): the instance moved along its own x and y axes by the bindings' axes 9 and 10 (20
    // units a second) and along its z axis by how much more action 0 is pressed than action 1 (40 a second), then turned about the
    // world's x and y axes by axes 6 and 7 (radians a second); and the instance moved along an axis by an amount a second (not by
    // under 5e-05 on every axis)
    void Frame(f32 seconds, struct GamePad* pad) RETAIL_N32(FUN_00259420);
    void MoveAlong(const Vector4* axis, f32 amount, f32 seconds) RETAIL_N32(FUN_002592d8);
};
CHECK_SIZE(PadInstanceMover, 0xC);

extern "C"
{
    extern const GccVTableEntry g_WidgetVTable[] RETAIL(D_003031B8);
    extern const GccVTableEntry g_TiledPictureVTable[] RETAIL(D_003030B8);
    extern const GccVTableEntry g_AnimatedWidgetVTable[] RETAIL(D_00303138);
    extern const GccVTableEntry g_LabelVTable[] RETAIL(D_00302FF8);
    extern const GccVTableEntry g_RectangleWidgetVTable[] RETAIL(D_00302D08);
    extern const GccVTableEntry g_StringLabelVTable[] RETAIL(D_00302C08);
    extern const GccVTableEntry g_MenuWidgetVTable[] RETAIL(D_00302E88);
    extern const GccVTableEntry g_SpriteWidgetVTable[] RETAIL(D_00302C88);
    extern const GccVTableEntry g_RingWidgetVTable[] RETAIL(D_00302D88);
    extern const GccVTableEntry g_WidgetControllerVTable[] RETAIL(D_00303078);
    extern const GccVTableEntry g_ItemWidgetVTable[] RETAIL(D_00303268);
    // The defaults widgets start with: the colour, the place (the middle) and the scale (1); a rectangle widget's: the colour
    // and the corners (the whole screen)
    extern u32 g_WidgetColour RETAIL(D_0030A958);
    extern Vector2 g_WidgetPlace RETAIL(D_0030A960);
    extern Vector2 g_WidgetScale RETAIL(D_0030A968);
    extern u32 g_RectangleColour RETAIL(D_0030A970);
    extern Vector2 g_RectangleStart RETAIL(D_0030A978);
    extern Vector2 g_RectangleEnd RETAIL(D_0030A980);

    TextLine* TextLineConstruct(TextLine* line, Font* font) RETAIL(FUN_0025b580);
    void TextLineDraw(TextLine* line, Renderer* renderer) RETAIL(FUN_0025b5f8);
    void TextLineUpdate(TextLine* line, TimeClock* clock) RETAIL(FUN_00257018);
}
