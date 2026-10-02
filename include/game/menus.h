#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/array.h"
#include "game/math.h"

struct MenuInput;
struct MenuSounds;

class MenuPage;

// The UI's menus (their vtables the retail ones): pages of items, each player with a selected item on the page. An item has its
// vtable after 0x10 bytes; its functions: 1 activated (for a player, on a page: where it leads), 2 the destructor, 3 and 4 told
// when its page is entered and left (for a player, in a mode), 5 a frame of it while it's selected (for a player, on a page, with
// the input and the sounds: where it leads), 6 still unknown, 7 its value's text (for a player, into a buffer: whether it has one),
// then its value set and got for a player as an int (8, 9), unsigned (10, 11) and a float (12, 13)
struct MenuItem
{
    enum IdBits : u32
    {
        IdMask = 0xFFF,
        TextShift = 12,
        TextMask = 0xFFF,
        PlayersShift = 24,
        PlayersMask = 0xF,
        // Left and right step a value when pressed (held otherwise)
        StepsOnPress = 0x10000000,
    };

    // Its text (none: the text table's of bits 12-23 of id)
    const char* text;
    // Bits 0-11: what the item is (pages refer to their items by it); 12-23: its text in the text table; 24-27: the players
    u32 id;
    // A bit per player: the item is shown, and it can be selected
    u8 shown;
    u8 enabled;
    u8 unknown0A[2];
    // Where activating it leads
    MenuPage* target;
    const GccVTableEntry* vtable;

    static MenuItem* Construct(MenuItem* item, u32 text, u32 id, u32 players) RETAIL(FUN_0025c438);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(FUN_0025c428);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025c4b8);
    void Entered(u32 player, u32 mode) RETAIL(FUN_0025c4e8);
    void Left(u32 player, u32 mode) RETAIL(FUN_0025c590);
    // Select activates it (when it's shown to the player), with its sound
    MenuPage* Frame(u32 player, MenuPage* page, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_0025c4f0);
    // A flag as the value: set through the int setter, got from the int getter (whether it's 0 or 1)
    u32 SetFlag(u32 player, u32 flag) RETAIL(FUN_0025c598);
    u32 GetFlag(u32 player, u8* flag) RETAIL(FUN_0025c5c0);

    u32 Id() const
    {
        return id & 0xFFF;
    }

    // Its value set and got for a player as an int, through its vtable
    u32 SetIntValue(u32 player, s32 value)
    {
        return CallVirtual<u32>(this, vtable, 8, player, value);
    }

    u32 GetIntValue(u32 player, s32* value)
    {
        return CallVirtual<u32>(this, vtable, 9, player, value);
    }

    bool IsShownTo(u32 player) const
    {
        return ((shown >> player) & 1) != 0;
    }

    bool CanSelect(u32 player) const
    {
        return ((enabled >> player) & 1) != 0;
    }
};

// An item that leads to a page, with no value
struct LinkItem : MenuItem
{
    static LinkItem* Construct(LinkItem* item, u32 text, u32 id, MenuPage* target, u32 players) RETAIL(FUN_0025c948);
    static LinkItem* ConstructNamed(LinkItem* item, const char* text, u32 id, MenuPage* target, u32 players) RETAIL(FUN_0025c8d0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025c9c8);
    MenuPage* Unknown6() RETAIL(FUN_0025c9f8);
    u32 ValueText(u32 player, char* text) RETAIL(func_0025CA00);
    u32 SetInt(u32 player, s32 value) RETAIL(FUN_0025ca10);
    u32 GetInt(u32 player, s32* value) RETAIL(func_0025CA18);
    u32 SetUnsigned(u32 player, u32 value) RETAIL(func_0025CA28);
    u32 GetUnsigned(u32 player, u32* value) RETAIL(func_0025CA30);
    u32 SetFloat(u32 player, f32 value) RETAIL_N32(FUN_0025ca48);
    u32 GetFloat(u32 player, f32* value) RETAIL(func_0025CA50);
};

// A link that tells its page it goes back first (the page's back function)
struct BackItem : LinkItem
{
    static BackItem* Construct(BackItem* item, u32 text, u32 id, MenuPage* target, u32 players) RETAIL(FUN_0025ba50);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(FUN_0025b9e8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025ba88);
};

// An item with a number of each player's in a range, which left and right step (wrapping round when it wraps; stepping down from
// above 0 always does) and select steps up when it cycles. Its text is the number
struct ValueItem : MenuItem
{
    s32 values[8];
    s32 minimum;
    s32 maximum;
    u8 wraps;
    u8 cycles;
    u8 unknown3E[2];

    static ValueItem* Construct(ValueItem* item, u32 text, u32 id, s32 minimum, s32 maximum, u32 wraps, s32 value, u32 players)
        RETAIL(FUN_0025bb38);
    // The range, and every player's value
    void SetRange(s32 newMinimum, s32 newMaximum, u32 newWraps, s32 value) RETAIL(FUN_0025bbe8);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(FUN_0025bac0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025bbc0);
    MenuPage* Frame(u32 player, MenuPage* page, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_002576a8);
    MenuPage* Unknown6() RETAIL(FUN_0025bc30);
    u32 ValueText(u32 player, char* text) RETAIL(FUN_0025bc38);
    u32 SetInt(u32 player, s32 value) RETAIL(func_0025BC70);
    u32 GetInt(u32 player, s32* value) RETAIL(func_0025BC88);
    u32 SetUnsigned(u32 player, u32 value) RETAIL(func_0025BCA0);
    u32 GetUnsigned(u32 player, u32* value) RETAIL(func_0025BCB8);
    u32 SetFloat(u32 player, f32 value) RETAIL_N32(func_0025BCD0);
    u32 GetFloat(u32 player, f32* value) RETAIL(func_0025BCE8);
};
CHECK_SIZE(ValueItem, 0x40);

// A value item whose values are named: by the text table's texts, or by strings of its own
struct ChoiceItem : ValueItem
{
    const char** strings;
    u32 usesStrings;
    s32* texts;
    u32 choices;

    static ChoiceItem* Construct(ChoiceItem* item, u32 text, u32 id, u32 choices, u32 wraps, s32 value, u32 players)
        RETAIL(FUN_0025c6b0);
    void SetChoiceText(u32 choice, s32 text) RETAIL(FUN_0025c838);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025c7b8);
    u32 ValueText(u32 player, char* text) RETAIL(FUN_0025c850);
};
CHECK_SIZE(ChoiceItem, 0x50);

// A choice of two that select toggles
struct ToggleItem : ChoiceItem
{
    static ToggleItem* Construct(ToggleItem* item, u32 text, u32 id, s32 offText, s32 onText, u32 wraps, s32 value, u32 players)
        RETAIL(FUN_0025ca68);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025cb88);
};

// A page: its name, a byte of each player's (still unknown) and each player's selection, its flags, the pages next to it (with
// the item each comes back to) and its items. Its vtable follows 0x4C bytes of members; its functions: 1 and 3 are told when the
// page is entered and left (for a player, in a mode), 4 is the destructor
class MenuPage
{
public:
    enum Flags : u32
    {
        // Moving past the last item goes on at the first and the other way round
        Wraps = 0x1000,
        // Bits 13-17: the players
        PlayersShift = 13,
        PlayersMask = 0x1F,
        // Bits 18-23: the item selected when the page is entered afresh
        FirstItemShift = 18,
        FirstItemMask = 0x3F,
        // Bit 24: the select action asked for (and dropped); bit 25: back goes to the parent page; bit 26: up and down move the selection; bit 27: left and
        // right go to the pages next to it
        AsksSelect = 0x1000000,
        TakesBack = 0x2000000,
        TakesUpDown = 0x4000000,
        TakesLeftRight = 0x8000000,
        // Bits 28-30: how the page was entered (1 afresh, 2 back, else as the page it was entered from)
        ModeShift = 28,
        ModeMask = 7,
        // The leave action leaves the menu
        TakesLeave = 0x80000000,
    };

    // The pages left, right, above and below it
    enum Link : u32
    {
        LinkLeft = 0,
        LinkRight = 1,
        LinkUp = 2,
        LinkDown = 3,
    };

    const char* name;
    u8* unknown04;
    u32 unknown08;
    u8* selections;
    u32 unknown10;
    u32 flags;
    // Where going back goes
    MenuPage* parent;
    // The pages next to it (left and right: the nearest of the ring that shows the player anything) and the item each starts
    // on there (-1: the selection's place, or the end the page is entered at)
    MenuPage* links[4];
    s32 linkItems[4];
    PointerArray<MenuItem> items;
    const GccVTableEntry* vtable;

    // A page for the players, named or with the text table's title of an index (the low 10 bits of its flags)
    static MenuPage* Construct(MenuPage* page, const char* name, u32 players) RETAIL(FUN_0025bd08);
    static MenuPage* ConstructTitled(MenuPage* page, u32 title, u32 players) RETAIL(FUN_0025bde0);
    u32 Add(MenuItem* item) RETAIL(FUN_0025bf68);
    // The item of an id made the first selected, or selected for a player (no item of the id: nothing)
    void SetFirstItem(u32 id) RETAIL(FUN_0025c218);
    void Select(u32 player, u32 id) RETAIL(FUN_0025c1c8);

    u32 Players() const
    {
        return (flags >> PlayersShift) & PlayersMask;
    }

    u32 Mode() const
    {
        return (flags >> ModeShift) & ModeMask;
    }

    // The base page's vtable functions: told it's entered and left (for a player, in a mode), a frame of it (for a player, with the
    // input and the sounds), the back action (not taken: 0) and 6 to 8 (for a player, still unknown), all doing nothing
    void Entered(u32 player, u32 mode) RETAIL(FUN_0025aa30);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_0025aa38);
    void Left(u32 player, u32 mode) RETAIL(FUN_0025aa40);
    u32 Back(u32 player) RETAIL(FUN_0025aa68);
    void Unknown6(u32 player) RETAIL(FUN_0025aa70);
    void Unknown7(u32 player) RETAIL(FUN_0025c268);
    void Unknown8(u32 player) RETAIL(FUN_0025aa78);
    // Its links and selections cleared, its flags made the defaults
    void Initialise() RETAIL(FUN_002579a8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025bed8);
    // The item at an index (none out of range); the game has the same function twice
    MenuItem* Item(s32 index) RETAIL(FUN_0025aa90);
    MenuItem* ItemAgain(s32 index) RETAIL(FUN_0025aac0);
    // The player's selection moved to the previous or next item it can select. Returns whether it moved
    u32 SelectPrevious(u32 player) RETAIL(FUN_00257828);
    u32 SelectNext(u32 player) RETAIL(FUN_002578e8);
    // The player's selection moved on until it's on an item shown to them (at most once round)
    void SkipHidden(u32 player) RETAIL(FUN_00257a88);
    // The index of the item of an id, or -1; the item of an id, or none
    s32 IndexOf(u32 id) RETAIL(FUN_00257b68);
    MenuItem* Find(u32 id) RETAIL(FUN_00257c38);
    // How many items are shown to the player
    s32 ShownCount(u32 player) RETAIL(FUN_00257cf8);
    // Entered from a page (none: afresh): every player's selection put on an item they can select, the page and its items told
    void Enter(MenuPage* from) RETAIL(FUN_00257dc8);
    // Left: the page and its items told
    void Leave() RETAIL(FUN_0025c270);
    // A frame of the page for a player: the input moves the selection (up and down, to the page above or below at its ends), goes
    // to the next pages left and right that show the player anything, back to the parent page, or to where the selected item
    // leads. Returns the page to go to (the resume page leaves the menu), none to stay
    MenuPage* Step(MenuInput* input, MenuSounds* sounds, u32 player) RETAIL(FUN_00257fd0);
};
CHECK_SIZE(MenuPage, 0x50);

struct ButtonBindings;
struct GameSound;
struct PadButtons;

// What the menus take from a controller: seven actions of its button bindings, each pressed this frame (a bit of pressed) and
// held (a bit of held). Its vtable follows 8 bytes (2: poll, 3: nothing)
struct MenuInput
{
    enum Action : u32
    {
        // Picks the selected item (a page's flag bit 24 asks for it too, and drops it)
        ActionSelect = 0,
        // Goes back to the parent page
        ActionBack = 1,
        // Leaves the menu (a page whose flags' bit 31 is set)
        ActionLeave = 2,
        ActionUp = 3,
        ActionDown = 4,
        ActionLeft = 5,
        ActionRight = 6,
    };

    ButtonBindings* bindings;
    u8 pressed;
    u8 held;
    u8 unknown06[2];
    const GccVTableEntry* vtable;

    static MenuInput* Construct(MenuInput* input, ButtonBindings* bindings) RETAIL(FUN_0025c388);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025c3c0);
    // The actions read from the pad's buttons, the leave action only when asked
    void Poll(const PadButtons* pad, u32 withLeave) RETAIL(FUN_00258518);
    void Nothing() RETAIL(FUN_0025aaf0);
    void Clear() RETAIL(FUN_0025c3f0);
    // Whether the action was pressed this frame (or is held)
    u32 Has(u32 action, u32 pressedNow) RETAIL(FUN_0025c400);
};
CHECK_SIZE(MenuInput, 0xC);

// The sounds a menu plays (none in a slot without one), and the group each plays in (3 by default)
struct MenuSounds
{
    static constexpr u32 Count = 13;

    GameSound* sounds[Count];
    s32 groups[Count];

    static MenuSounds* Construct(MenuSounds* sounds) RETAIL(FUN_0025c630);
    void Play(u32 index) RETAIL(FUN_0025c668);
};
CHECK_SIZE(MenuSounds, 0x68);

class Font;
struct Renderer;

// How a menu's pages are drawn: the page's title and the lines of its items (an item's text, " : " and its value when it has
// one), each style (the title, the selected item, an item shown to the player and one that isn't) in its font and colour, the
// title in its alignment and the items in theirs. Places and sizes are fractions of the size the page is drawn at. With a window
// of lines the items are drawn at most that many at a time around the selection, and their scale and line spacing go from the
// few-items values to the many-items ones as the count goes from the window's start to its size; without one they're the
// many-items values. The selected item's colour and size pulse with the two scalers' scales
struct MenuDrawer
{
    enum Style : u32
    {
        StyleTitle = 0,
        StyleSelected = 1,
        StyleShown = 2,
        StyleHidden = 3,
    };

    u8 windowStart;
    u8 windowSize;
    u8 titleFlags;
    u8 itemFlags;
    Vector2 titlePlace;
    Vector2 titleScale;
    Vector2 itemsPlace;
    Vector2 fewItemsScale;
    f32 fewItemsSpacing;
    Vector2 manyItemsScale;
    f32 manyItemsSpacing;
    // Something whose value (0x10 bytes in) the selected item's colour or size is scaled by
    struct Pulse
    {
        u8 unknown00[0x10];
        f32 value;
    };

    Font* fonts[4];
    u32 colours[4];
    const Pulse* colourPulse;
    const Pulse* sizePulse;

    // Every style in the font: the title at the top, centred (alignment 6) and full size, the items below it (alignment 3) at half
    // size and spaced a twentieth apart, in the game's colours 15 (the title and the selected item), 19 and 21
    static MenuDrawer* Construct(MenuDrawer* drawer, Font* font) RETAIL(FUN_00258898);
};
CHECK_SIZE(MenuDrawer, 0x5C);

extern "C"
{
    // The renderer's text set to a style of the drawer's (the colours tinted already, the scale the size's)
    void MenuDrawerStyle(MenuDrawer* drawer, u32 style, const Vector2* scale, const u32* colours, Renderer* renderer)
        RETAIL(FUN_00258760);
    // A page drawn for a player, tinted by a colour, at a place and size (fractions of the screen)
    void DrawMenuPage(MenuDrawer* drawer, MenuPage* page, u32 player, u32 colour, const Vector2* at, const Vector2* size,
                      Renderer* renderer) RETAIL(FUN_002589f8);
    // What goes between an item's text and its value
    extern const char g_MenuValueSeparator[] RETAIL(D_0030A138);

    extern const GccVTableEntry g_MenuPageVTable[] RETAIL(D_00302F30);
    extern const GccVTableEntry g_MenuInputVTable[] RETAIL(D_00302F08);
    extern const GccVTableEntry g_MenuItemVTable[] RETAIL(D_00302F80);
    extern const GccVTableEntry g_LinkItemVTable[] RETAIL(D_00302B90);
    extern const GccVTableEntry g_BackItemVTable[] RETAIL(D_00302B18);
    extern const GccVTableEntry g_ValueItemVTable[] RETAIL(D_00302AA0);
    extern const GccVTableEntry g_ChoiceItemVTable[] RETAIL(D_00302910);
    extern const GccVTableEntry g_ToggleItemVTable[] RETAIL(D_00302898);
    // A value item's text: "%d"
    extern const char g_ValueFormat[] RETAIL(D_0030A130);
    // The page that leaves the menu ("DUMMY-RESUME"), and the page a step goes to
    extern MenuPage g_ResumePage RETAIL(D_003B6ED0);
    extern MenuPage* g_NextPage RETAIL(D_0030A134);

}
