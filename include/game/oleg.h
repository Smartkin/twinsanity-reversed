#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/bindings.h"
#include "game/math.h"
#include "game/menus.h"
#include "game/particles2d.h"
#include "game/progress.h"
#include "game/reference.h"
#include "game/savedevice.h"
#include "game/shapes.h"
#include "game/sound.h"
#include "game/string.h"
#include "game/widgeteffects.h"
#include "game/widgets.h"

class Font;
struct ChunkManager;
struct GamePad;

// A cycling scale's values' count, and whether it's running
union CyclingScaleFlags
{
    u16 value;
    struct
    {
        u16 count : 15;
        u16 running : 1;
    };
};
CHECK_SIZE(CyclingScaleFlags, 2);

// A scale going round values (the titles' breathing), a cycle each period (clock units) from when it last began (0 before its first
// step), the scale between the values the cycle is at; with a limit it stops after that many cycles at its last value. Its scale is
// where a widget's scaler is read
struct CyclingScale
{
    CyclingScaleFlags flags;
    u8 limited;
    u8 cyclesLeft;
    s32 time;
    s32 period;
    f32* values;
    f32 scale;

    // Whether it's still running
    u32 Step(TimeClock* clock) RETAIL(FUN_0017a038);
};

struct SaveManager;

// The character the player plays (game/player.h)
struct PlayerCharacter;

// The HUD's health bar (D_002F5438), of OLEG's sprites: a body as long as the progress's bar length, then a piece for each point
// of health with gaps between them up to the most it can have, an end cap when it's full
class HudBarWidget : public AnimatedWidget
{
public:
    Sprite* sprites;
    GameProgress* progress;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00179598);
    void Draw(Renderer* renderer) RETAIL(FUN_00167270);
};
CHECK_SIZE(HudBarWidget, 0x90);

// A slider (D_002F53B8): a track across its place, a knob a fraction of the way along it and the track's two end caps (none
// at first), the knob and the caps squares of its height
class SliderWidget : public AnimatedWidget
{
public:
    f32 value;
    Sprite* track;
    Sprite* knob;
    Sprite* rightEnd;
    Sprite* leftEnd;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001795b8);
    void Draw(Renderer* renderer) RETAIL(FUN_00167530);
};
CHECK_SIZE(SliderWidget, 0x9C);

struct GameController;

// A level on a world's levels page (D_002F50A0): its title (question marks until it's open) and its six gem slots; selected,
// the title pulses and the gems found sparkle (with OLEG's first six emitters), and it bobs a little
class LevelWidget : public ItemWidget
{
public:
    GameController* controller;
    // How far it bobs (fractions of the screen)
    f32 bob;

    static LevelWidget* Construct(LevelWidget* widget, f32 anchor, MenuPage* page, MenuItem* item, GameController* controller)
        RETAIL_N32(FUN_0017a378);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00179e20);
    // The frame begun: its bob and its found gems' sparkles stepped while it's selected (no bob otherwise)
    void BeginFrame() RETAIL(FUN_00171a28);
    void Draw(Renderer* renderer) RETAIL(FUN_001716d8);
};
CHECK_SIZE(LevelWidget, 0x98);

// What a save slot shows of its save's progress: the area it was saved in, the character, the crystals, the lives and one, how
// much of the game is done (percent)
union SavedProgress
{
    u32 value;
    struct
    {
        u32 area : 6;
        u32 character : 4;
        u32 crystals : 6;
        u32 lives : 7;
        u32 done : 7;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(SavedProgress, 4);

// What a save slot shows of its save (the save manager's): the folder's kind of summary (its date, both of whose saved bits are set
// while the slot holds a save), then the progress and the time played (clock units)
struct SaveSummary
{
    FolderSummary folder;
    SavedProgress progress;
    s32 time;

    // Its vtable (BanksHeader_methods, 0x14 bytes in) over the folder summary's (game/savemanager.cpp): 1 the destructor (the
    // folder summary's), 5 and 6 the progress and the time read and written before the folder summary's part
    void Destroy(u32 destroyFlags) RETAIL(FUN_00179ae0);
    void Read(Stream* stream) RETAIL(ReadBanksHeader);
    void Write(Stream* stream) RETAIL(WriteBanksHeader);
};
CHECK_OFFSET(SaveSummary, progress, 0x18);
CHECK_SIZE(SaveSummary, 0x20);

// A save slot on the save manager's slots page (D_002F5180): its save's level picture and name, the time played (hours and
// minutes), how much is done, the character's head with the lives and the crystals, the picture pulsing while it's selected; an
// empty slot says so (dimmer when it isn't shown to the player)
class SaveSlotWidget : public ItemWidget
{
public:
    GameController* controller;
    SaveSummary* summary;

    static SaveSlotWidget* Construct(SaveSlotWidget* widget, f32 anchor, MenuPage* page, MenuItem* item,
                                     GameController* controller, SaveSummary* summary) RETAIL_N32(FUN_00179db0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00179d90);
    void Draw(Renderer* renderer) RETAIL(FUN_001688a0);
};
CHECK_SIZE(SaveSlotWidget, 0x98);

class OLEG;
class Stream;

// Reads a picture's file for OLEG (its vtable D_002F5390)
class PictureReader : public SectionReader
{
public:
    u32 picture;
    OLEG* oleg;

    void Destroy(u32 flags) RETAIL(FUN_001795d8);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_00179e40);
    // Nothing when the file isn't there (the empty function other readers' vtables share)
    void Missing(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_00179358);
};
CHECK_SIZE(PictureReader, 0xC);

// A picture OLEG reads from a file: whether it's being read, the picture read and the one wanted (each picture has a number of its
// own for none)
union OlegPictureState
{
    u32 value;
    struct
    {
        u32 reading : 1;
        u32 read : 8;
        u32 wanted : 8;
        u32 unused17 : 15;
    };
};
CHECK_SIZE(OlegPictureState, 4);

// The gem whose colour the pickup's sparkle plays in
union PickupGem
{
    u16 value;
    struct
    {
        u16 gem : 4;
        u16 unused4 : 12;
    };
};
CHECK_SIZE(PickupGem, 2);

// OLEG: the in-game UI manager (the retail RendererRelatedInGameController, its vtable the retail one after the widget controller's
// members: 1 the destructor, 2 every widget hidden, 3 and 6 the widgets' 11 and 13, 4 the update, 5 the draw). The HUD, the pause
// menu and the front end's menus as widgets in its 64 slots (a member of the game controller, at 0x5E0)
class OLEG : public WidgetController
{
public:
    // Its screens (WidgetController::screens: the widget slots each shows)
    enum Screen : u32
    {
        // A cutscene's bars and its text, the scripts' fader, the bottom text on its backdrop, the dimmer behind menus
        ScreenCutscene = 0,
        ScreenFader = 1,
        ScreenBottomText = 2,
        ScreenDimmer = 3,
        // The buttons' hints alone
        ScreenBackHint = 4,
        ScreenCancelHint = 5,
        ScreenSelectHint = 6,
        ScreenPagesLeftHint = 7,
        ScreenPagesRightHint = 8,
        // The menus
        ScreenOptions = 9,
        ScreenScreenPosition = 10,
        ScreenMainMenu = 11,
        ScreenPauseMenu = 12,
        ScreenDisableAutosave = 13,
        ScreenQuit = 14,
        // The pause screens of a missing controller, a disc error and the notices (the fourth's shows the first's widgets)
        ScreenNoController = 15,
        ScreenDiscError = 16,
        ScreenAutosaveOff = 17,
        ScreenAutosaveOn = 18,
        ScreenAutosaveFailed = 19,
        ScreenFourthNotice = 20,
        // The four worlds' levels pages, the extras, a gallery's picture and the game over
        ScreenLevels = 21,
        ScreenExtras = 25,
        ScreenGallery = 26,
        ScreenGameOver = 27,
        // The save code's message, its choices and the save slots
        ScreenSaveMessage = 28,
        ScreenSaveChoices = 29,
        ScreenSaveSlots = 30,
        // The autosave's icon, and the HUD: the wumpa fruit and lives, the timed play's count and time, the health bar and lives,
        // the slider, the lives, the wumpa fruit, a pickup, the ammo
        ScreenAutosaving = 31,
        ScreenHud = 32,
        ScreenTime = 33,
        ScreenHealth = 34,
        ScreenSlider = 35,
        ScreenLives = 36,
        ScreenWumpa = 37,
        ScreenPickup = 38,
        ScreenAmmo = 39,
        // The black screen, the screen picture, and the screen picture with the loading screen's logo and text
        ScreenBlack = 40,
        ScreenPicture = 41,
        ScreenLoading = 42,
        // Every widget but the cutscene's bars, the fader and the bottom text's backdrop, and every widget but the pause menu
        ScreenAllButOverlays = 43,
        ScreenAllButPauseMenu = 44,
        Screens = 45,
    };

    // The pictures it reads from files: the Crash title (into its second sprite), the level's title (the area's, into the third)
    // and the tiles' (the screen picture's)
    enum Picture : u32
    {
        PictureCrashTitle = 0,
        PictureLevelTitle = 1,
        PictureTiles = 2,
        Pictures = 3,
    };

    // What the pictures read are without a picture: the Crash title's only picture is 0, the level's title's are the areas' (25)
    static constexpr u32 NoCrashTitle = 1;
    static constexpr u32 NoLevelTitle = 25;

    // The tiles' pictures: the legal screen, the loading screens, the game over screens (Cortex, Crash, Crash and Cortex,
    // Mecha-Bandicoot, Nina), the two named by its picture name (a gallery's), the credits, and none
    enum TilesPicture : u32
    {
        TilesLegal = 0,
        TilesLoading = 1,
        TilesGameOver = 4,
        TilesNamed = 9,
        TilesCredits = 11,
        TilesNone = 12,
    };

    // Its sprites: the flat material's square, the pictures' (the Crash title, the level's title), the HUD's icons (SetHudIcon's
    // slots, the first the health bar's body), then StartUp\Icons.psm's: the characters' heads, the empty gem slot, the six gems,
    // the wumpa fruit, the crystal a pickup shows and the crystal, the icons of the ammo, the autosave and the clock, the health
    // bar's pieces, the locked level's title and the levels' titles
    enum Sprites : u32
    {
        SpriteFlat = 0,
        SpriteCrashTitle = 1,
        SpriteLevelTitle = 2,
        SpriteHudIcons = 3,
        SpriteIcons = 6,
        SpriteHeads = 6,
        SpriteEmptyGem = 12,
        SpriteGems = 13,
        SpriteWumpa = 19,
        SpritePickupCrystal = 20,
        SpriteCrystal = 21,
        SpriteAmmo = 22,
        SpriteAutosave = 23,
        SpriteClock = 24,
        SpriteBarStart = 25,
        SpriteBarGap = 26,
        SpriteBarPiece = 27,
        SpriteBarEnd = 28,
        SpriteLockedLevel = 29,
        SpriteLevelTitles = 30,
        SpriteCount = 46,
    };

    // The HUD's icons (SetHudIcon's slots, sprites from SpriteHudIcons): a boss's (the health bar's body), whack-a-worm's (the
    // timed play's count's) and the vehicle gauge's left end
    static constexpr u32 HudIconBoss = 0;
    static constexpr u32 HudIconWhackaworm = 1;
    static constexpr u32 HudIconGauge = 2;
    static constexpr u32 HudIcons = 3;

    // The sprite the pickup's icon shows
    u8 pickupSprite;
    // Wumpa fruit to add to the count, one at a time
    u8 wumpaToAdd;
    PickupGem pickupGem;
    OlegPictureState pictures[Pictures];
    // How many saves show the saving screens (the first stops the chunks)
    u32 savingShown;
    // Clock units until the next wumpa fruit is added (0.15 seconds at the start)
    s32 wumpaDelay;
    CurveScaleEffect pulse;
    SlideEffect slide;
    SpinEffect spin;
    RockEffect rock;
    GameProgress* progress;
    Font* font;
    // The file of the tiles' pictures named by it (a gallery's)
    String pictureName;
    ButtonBindings bindings;
    MenuInput input;
    MenuDrawer drawers[9];
    MenuSounds sounds;
    // Its menus' pages (its start-up makes them)
    MenuPage* pages[18];
    // A renderer material of its own (the platform's, in storage of its own)
    alignas(8) u8 material[0x70];
    // The particles' shader (the platform's), and their sprite
    void* particleShader;
    Sprite particleSprite;
    // The particles' curves
    Vector4Curve particleColours;
    Vector2Curve particleSizes;
    FloatCurve particleTurns[2];
    Emitter2D emitters[8];
    SparkleEffect sparkle;
    // The materials of the HUD's icons, none for the flat material
    struct MaterialResource* hudIcons[HudIcons];
    Sprite sprites[SpriteCount];
    Sprite tiles[8];
    TextLine textLine;
    // A cutscene's bars (rectangles of the flat sprite, sliding in from the screen's top and bottom), the scripts' fader, the
    // bottom text's backdrop and the bottom text (the text line's label), the dimmer behind the menus
    SpriteWidget letterboxTop;
    SpriteWidget letterboxBottom;
    SpriteWidget fader;
    SpriteWidget bottomTextBackdrop;
    StringLabel bottomText;
    SpriteWidget dimmer;
    // The eight rings round the middle behind the menus, and the panel behind their items
    RingWidget menuRings;
    RingWidget menuPanel;
    Label backHint;
    Label cancelHint;
    Label selectHint;
    Label pagesLeftHint;
    Label pagesRightHint;
    MenuWidget optionsMenu;
    RingWidget optionsPanel;
    MenuWidget screenPositionMenu;
    Label screenPositionHint;
    MenuWidget mainMenu;
    SpriteWidget titleLogo;
    MenuWidget pauseMenu;
    MenuWidget disableAutosaveMenu;
    Label disableAutosaveTitle;
    MenuWidget quitMenu;
    Label quitTitle;
    Label noControllerText;
    Label discErrorTitle;
    Label discErrorText;
    Label autosaveOffTitle;
    Label autosaveOffText;
    MenuWidget autosaveOffMenu;
    Label autosaveOnTitle;
    Label autosaveOnText;
    SpriteWidget autosaveOnIcon;
    MenuWidget autosaveOnMenu;
    Label autosaveFailedText;
    MenuWidget autosaveFailedMenu;
    // The fourth notice (pause reason 7, which nothing asks for): the autosave off notice's texts, and its menu
    Label fourthNoticeTitle;
    Label fourthNoticeText;
    MenuWidget fourthNoticeMenu;
    RingWidget wumpaRing;
    RingWidget livesRing;
    RingWidget crystalRing;
    RingWidget completionRing;
    RingWidget gemArc;
    SpriteWidget levelPicture;
    StringLabel completionText;
    StringLabel wumpaText;
    SpriteWidget wumpaSprite;
    StringLabel livesText;
    SpriteWidget livesSprite;
    StringLabel crystalText;
    SpriteWidget crystalSprite;
    SpriteWidget* gemSprites[6];
    MenuWidget levelsMenus[4];
    Label levelName;
    RingWidget levelNamePanel;
    MenuWidget extrasMenu;
    RingWidget extrasPanel;
    SpriteWidget extrasLogo;
    Label nextHint;
    MenuWidget gameOverMenu;
    // The save code's message, the message over its choices, and the save slots' title
    StringLabel saveMessage;
    StringLabel saveChoicesMessage;
    MenuWidget saveChoicesMenu;
    StringLabel saveSlotsTitle;
    RingWidget saveSlotsPanel;
    MenuWidget saveSlotsMenu;
    Label autosavingText;
    SpriteWidget autosavingIcon;
    // The timed play's count (whack-a-worm's: what's counted of the total, with the HUD's second icon) and time left
    StringLabel timedCountText;
    SpriteWidget timedCountIcon;
    StringLabel timeText;
    SpriteWidget clockIcon;
    HudBarWidget healthBar;
    // The slider of the vehicle of kind 6, between the HUD's third icon and Crash's head
    SliderWidget slider;
    StringLabel hudWumpaText;
    SpriteWidget hudWumpaSprite;
    StringLabel hudLivesText;
    SpriteWidget hudLivesSprite;
    // The extra life's head, the pickup's icon, the gun's ammo
    SpriteWidget extraLifeHead;
    SpriteWidget pickupIcon;
    StringLabel ammoText;
    SpriteWidget ammoIcon;
    // A black screen (behind the game over's picture too), the tiles' picture
    SpriteWidget blackout;
    TiledPicture screenPicture;
    SpriteWidget loadingLogo;
    Label loadingText;

    static OLEG* Construct(OLEG* oleg, Font* font, GameProgress* progress) RETAIL(FUN_0016d938);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0016f318);
    // Its menus' pages made and given to its menus with the pad (the pause page is handed the pad and the first font, which it
    // doesn't use), the save manager's screens, texts and pages, the menu input's buttons, the drawers' styles in the menu font
    // and the menus' sounds of the front end's
    void StartUp(PadButtons* pad, Font* font, Font* menuFont, SoundTable* frontEnd) RETAIL(FUN_0016fef0);
    // In the slot of the controller's HideAll, hiding nothing: its text line emptied, no wumpa fruit to add, and the fourth to
    // sixth sprites drawn flat again
    void Reset() RETAIL(FUN_00179f80);
    // A frame: the titles' breathing stepped; paused, the wumpa count and the extra life's head go; otherwise wumpa fruit to add
    // are added one at a time (sooner the more there are), each pulsing the count, a hundred making a life; then the HUD's values,
    // the widgets and the text line (its text copied into the bottom text)
    void Update(TimeClock* clock, GamePad* pad, PlayerCharacter* character) RETAIL(FUN_00171308);
    // A pickup's effect on a screen: the lives count rocks, the pickup's icon sparkles in the gem's colour (the crystal's for the
    // crystal)
    void PlayPickupEffect(u32 screen) RETAIL(FUN_0016ce38);
    // The first widget a screen shows (none for a screen without any)
    Widget* ScreenWidget(u32 screen)
    {
        s32 index = IndexOf(screens[screen]);
        return index < 0 ? nullptr : widgets[index];
    }

    // The picture a picture wants read (LoadPicture reads it)
    void WantPicture(u32 picture, u32 wanted)
    {
        pictures[picture].wanted = wanted;
    }

    // The first of its menus that's shown (but the game over's and the save manager's), none without one
    MenuWidget* ShownMenu() RETAIL(FUN_001715a0);
    // Its material for the 2D particles and its particle sprite, and the emitters' sprites
    void SetUpParticles() RETAIL(FUN_0016d0d0);
    // The HUD's values from the game's progress and the character
    void UpdateHud(PlayerCharacter* character) RETAIL(FUN_00169320);
    // The HUD's icon of a slot (sprites 3 to 5: 0 the boss's, 1 whack-a-worm's) shows an OGI's (its first rigid model's first
    // material; 0xFFFF the flat material; nothing changes for an ID without an OGI)
    void SetHudIcon(u32 slot, const u16* modelId) RETAIL(FUN_00171008);
    // Lives added (0 to 100), the count shown and rocked; celebrated, the extra life's head (the character's) appears sliding and
    // spinning and its sound plays
    void AddLives(s32 lives, u32 celebrated) RETAIL(FUN_00171138);
    // The widgets, then its line of text
    void Draw(Renderer* renderer) RETAIL(FUN_0017a1d8);
    // A picture's sprites let go of what they drew, and the picture made none
    void ReleasePicture(u32 picture) RETAIL(FUN_00170bd8);
    // The picture wanted read from its file unless it's the one read (with the readers queued, read now when asked): the Crash
    // title, the level's title, or the legal screen, a loading screen, a game over screen or the credits (9 and 10: the file
    // its picture name names)
    void LoadPicture(u32 picture, u32 now) RETAIL(FUN_00170d30);
    // A picture's file read into its sprites, no longer being read
    void ReadPicture(u32 picture, Stream* stream) RETAIL(FUN_00179ea0);

    // Its set-up, a part at a time
    // Which widgets each of its 45 screens shows (the controller's screens)
    void SetUpScreens() RETAIL(FUN_00169d20);
    // The UI's flat material made, the first six sprites drawn with it
    void SetUpFlatSprites() RETAIL(FUN_0016cc08);
    // The timed play's count and time with their icons
    void SetUpTimeHud() RETAIL(FUN_0016a0f8);
    // A cutscene's bars, the fader, and the bottom text and its backdrop
    void SetUpOverlays() RETAIL(FUN_0016a278);
    // The extras' menu, its panel, the logo and the next hint
    void SetUpExtras() RETAIL(FUN_0016a518);
    // The main menu and the title's logo
    void SetUpMainMenu() RETAIL(FUN_0016a740);
    // The loading screen's logo and text, the screen picture and the black screen
    void SetUpLoadingScreen() RETAIL(FUN_0016a890);
    // The menu rings: eight round the middle behind the menus, the breathing scale's values, the curves; the hints, the menu panel
    // and the dimmer
    void SetUpMenuRings() RETAIL(FUN_0016aa58);
    // The levels menus and the level's name on its panel
    void SetUpLevelsMenus() RETAIL(FUN_0016b718);
    // The options menu and its panel
    void SetUpOptionsMenu() RETAIL(FUN_0016b938);
    // The pause menu, its pages, and the progress round it: the completion, the gems on an arc, the level's picture, the wumpa
    // fruit, lives and crystals
    void SetUpPauseMenu() RETAIL(FUN_0016ba38);
    // The save code's messages, its choices menu and the save slots' menu on their panel
    void SetUpSaveScreens() RETAIL(FUN_0016c9f8);
};
CHECK_OFFSET(OLEG, pictures, 0x314);
CHECK_OFFSET(OLEG, pages, 0x760);
CHECK_OFFSET(OLEG, pulse, 0x328);
CHECK_OFFSET(OLEG, progress, 0x390);
CHECK_OFFSET(OLEG, sounds, 0x6F8);
CHECK_OFFSET(OLEG, material, 0x7A8);
CHECK_OFFSET(OLEG, particleSprite, 0x81C);
CHECK_OFFSET(OLEG, particleColours, 0x83C);
CHECK_OFFSET(OLEG, emitters, 0x85C);
CHECK_OFFSET(OLEG, sparkle, 0xAC0);
CHECK_OFFSET(OLEG, sprites, 0xB4C);
CHECK_OFFSET(OLEG, textLine, 0x120C);
CHECK_OFFSET(OLEG, letterboxTop, 0x1240);
CHECK_OFFSET(OLEG, fader, 0x13B8);
CHECK_OFFSET(OLEG, menuRings, 0x1688);
CHECK_OFFSET(OLEG, optionsMenu, 0x1A94);
CHECK_OFFSET(OLEG, autosaveOnIcon, 0x2660);
CHECK_OFFSET(OLEG, fourthNoticeTitle, 0x2908);
CHECK_OFFSET(OLEG, wumpaRing, 0x2ADC);
CHECK_OFFSET(OLEG, gemSprites, 0x3320);
CHECK_OFFSET(OLEG, levelsMenus, 0x3338);
CHECK_OFFSET(OLEG, saveMessage, 0x3A4C);
CHECK_OFFSET(OLEG, autosavingIcon, 0x3EA0);
CHECK_OFFSET(OLEG, timedCountText, 0x3F5C);
CHECK_OFFSET(OLEG, healthBar, 0x420C);
CHECK_OFFSET(OLEG, slider, 0x429C);
CHECK_OFFSET(OLEG, extraLifeHead, 0x45E8);
CHECK_OFFSET(OLEG, blackout, 0x48B8);
CHECK_OFFSET(OLEG, screenPicture, 0x4974);
CHECK_OFFSET(OLEG, loadingText, 0x4ABC);

// The ring widgets' shapes: a panel's four rings (the disc AddPanelRings makes), every ring a segment of 32 steps round (but
// the menu rings' and the gem arc's)
constexpr u32 PanelRings = 4;
constexpr u32 RingSegments = 1;
constexpr u32 RingSteps = 32;
// The game texts (the code's text file's lines) of the confirmations' titles, which their screens and their pages show: the
// quit's and the disable autosave's
constexpr u32 QuitTitleText = 0x45;
constexpr u32 DisableAutosaveTitleText = 0x60;

extern "C"
{
    extern const GccVTableEntry g_OlegVTable[] RETAIL(RendererRelatedInGameController_Methods);
    extern const GccVTableEntry g_PulseEffectVTable[] RETAIL(D_002F5698);
    extern const GccVTableEntry g_SlideEffectVTable[] RETAIL(D_002F55F0);
    extern const GccVTableEntry g_SpinEffectVTable[] RETAIL(D_002F55B8);
    extern const GccVTableEntry g_RockEffectVTable[] RETAIL(D_002F5628);
    extern const GccVTableEntry g_SparkleEffectVTable[] RETAIL(D_002F5660);
    extern const GccVTableEntry g_RadialEmitter2DVTable[] RETAIL(D_002F6D60);
    extern const GccVTableEntry g_HudBarWidgetVTable[] RETAIL(D_002F5438);
    extern const GccVTableEntry g_SliderWidgetVTable[] RETAIL(D_002F53B8);
    // -0, a constant of OLEG's file's
    extern f32 g_MinusZero RETAIL(D_00309A44);
    // OLEG's file's places (its static initialiser fills them): where the screens' titles and the menus are
    extern Vector2 g_OlegTitlePlace RETAIL(D_0030A628);
    extern Vector2 g_OlegMenuPlace RETAIL(D_0030A630);
    // The menus' items' place and size (the lists' drawers'), and the notices' titles' and texts' places and sizes
    extern Vector2 g_OlegListItemsPlace RETAIL(D_0030A638);
    extern Vector2 g_OlegListItemsScale RETAIL(D_0030A640);
    extern Vector2 g_OlegNoticeTitlePlace RETAIL(D_0030A648);
    extern Vector2 g_OlegNoticeTitleScale RETAIL(D_0030A650);
    extern Vector2 g_OlegNoticeTextPlace RETAIL(D_0030A658);
    extern Vector2 g_OlegNoticeTextScale RETAIL(D_0030A660);
    extern Vector2 g_OlegShadowOffset RETAIL(D_0030A610);
    extern u32 g_OlegShadowColour RETAIL(D_0030A618);
    // The cutscene's bars' colour
    extern u32 g_OlegLetterboxColour RETAIL(D_0030A678);
    // The picture's colour shown and hidden
    extern u32 g_OlegPictureHiddenColour RETAIL(D_0030A700);
    extern u32 g_OlegPictureColour RETAIL(D_0030A708);
    // The rings' colours: the panels' borders and their faded edge, the panels' blue and its clear edge, the gem arc's
    extern u32 g_OlegRingBorderColour RETAIL(D_0030A748);
    extern u32 g_OlegRingEdgeColour RETAIL(D_0030A750);
    extern u32 g_OlegPanelColour RETAIL(D_0030A758);
    extern u32 g_OlegPanelClearColour RETAIL(D_0030A760);
    extern u32 g_OlegGemArcTrimColour RETAIL(D_0030A768);
    extern u32 g_OlegGemArcColour RETAIL(D_0030A770);
    extern u32 g_OlegGemArcClearColour RETAIL(D_0030A778);
    // The panels' inner disc's colours
    extern Vector4Curve g_OlegPanelColours RETAIL(D_0030A798);
    // The angles of the menu rings (15 degrees, the middle ones 25) and the gems' arc (turned 21 degrees, from -36 round 122
    // degrees, its last ring from -35 round 120)
    extern s32 g_OlegMenuRingsTurn RETAIL(D_0030A710);
    extern s32 g_OlegGemArcTurn RETAIL(D_0030A718);
    extern s32 g_OlegMenuMiddleRingsTurn RETAIL(D_0030A720);
    extern s32 g_OlegGemArcLastStart RETAIL(D_0030A728);
    extern s32 g_OlegGemArcLastSpan RETAIL(D_0030A730);
    extern s32 g_OlegGemArcStart RETAIL(D_0030A738);
    extern s32 g_OlegGemArcSpan RETAIL(D_0030A740);
    // Curves of OLEG's file's (its static initialiser makes them, the menu rings' set-up fills them): the gem arc's edges shrinking
    // and growing along it, and a glint passing round the outer menu ring and the completion's disc
    extern Vector2Curve g_OlegShrinkingShape RETAIL(D_0030A780);
    extern Vector2Curve g_OlegGrowingShape RETAIL(D_0030A788);
    extern Vector4Curve g_OlegGlintColours RETAIL(D_0030A790);
    // The titles' breathing scale
    extern CyclingScale g_BreathingScale RETAIL(D_0030BF10);
    // The pictures' files: the titles' folder and the language folder, the levels' titles (by the level), the legal screen's,
    // the loading screens', the game over screens' and the credits' (by the picture), the Crash title, the files' extension
    extern const char g_TitlesFolder[] RETAIL(D_002F4938);
    extern const char g_LanguageFolder[] RETAIL(D_002F4950);
    extern const char* const g_LevelTitles[25] RETAIL(D_002E7060);
    extern const char* const g_ScreenPictures[12] RETAIL(D_002E70C8);
    extern const char g_CrashTitle[] RETAIL(D_00309A48);
    extern const char* g_PictureExtension RETAIL(D_003099A0);
    extern const GccVTableEntry g_PictureReaderVTable[] RETAIL(D_002F5390);
    extern const GccVTableEntry g_LevelWidgetVTable[] RETAIL(D_002F50A0);
    extern const GccVTableEntry g_SaveSlotWidgetVTable[] RETAIL(D_002F5180);
    // The level of each of the game's 25 areas (the levels' titles' order; -1 none)
    extern const s32 g_AreaLevels[25] RETAIL(D_002E6FD8);
    // The story's share of the game done (percent) at each area it gets to
    extern const u8 g_StoryDone[32] RETAIL(D_002E7040);
    // The level widget the gems sparkle for, and where their bob is
    extern LevelWidget* g_SparklingLevel RETAIL(D_0030A60C);
    extern f32 g_LevelBobPhase RETAIL(D_0030A608);
    // The pickup sparkle's colours: the six gems' (blue, clear, green, purple, red, yellow) and the crystal's (the static
    // initialiser's green, red once the first pickup set the gems'), the guard of the gems' array and whether they're set
    extern Vector4 g_GemColours[6] RETAIL(D_0030BE10);
    extern Vector4 g_CrystalColour RETAIL(D_0030BE70);
    extern u32 g_GemColoursGuard RETAIL(D_0030A600);
    extern u8 g_PickupColoursSet RETAIL(D_0030A604);
    // The corners of a cutscene's bars (the rectangles from the first to the second) shown and hidden: the top bar's, the bottom
    // bar's
    extern Vector2 g_OlegTopBarFrom RETAIL(D_0030A680);
    extern Vector2 g_OlegTopBarTo RETAIL(D_0030A688);
    extern Vector2 g_OlegTopBarHiddenFrom RETAIL(D_0030A690);
    extern Vector2 g_OlegTopBarHiddenTo RETAIL(D_0030A698);
    extern Vector2 g_OlegBottomBarFrom RETAIL(D_0030A6A0);
    extern Vector2 g_OlegBottomBarTo RETAIL(D_0030A6A8);
    extern Vector2 g_OlegBottomBarHiddenFrom RETAIL(D_0030A6B0);
    extern Vector2 g_OlegBottomBarHiddenTo RETAIL(D_0030A6B8);
    // The bottom text's backdrop's colour shown and hidden, the text's, the backdrop's corners and the text's place and size
    extern u32 g_OlegBackdropColour RETAIL(D_0030A6C0);
    extern u32 g_OlegBackdropHiddenColour RETAIL(D_0030A6C8);
    extern u32 g_OlegBottomTextColour RETAIL(D_0030A6D0);
    extern u32 g_OlegBottomTextHiddenColour RETAIL(D_0030A6D8);
    extern Vector2 g_OlegBackdropTo RETAIL(D_0030A6E0);
    extern Vector2 g_OlegBackdropFrom RETAIL(D_0030A6E8);
    extern Vector2 g_OlegBottomTextPlace RETAIL(D_0030A6F0);
    extern Vector2 g_OlegBottomTextScale RETAIL(D_0030A6F8);
    // A ring widget made a bordered disc of the radii (four rings of the flat material: one coloured by a curve, three a little
    // bigger round it), with the UI's drop shadow
    void AddPanelRings(const Vector2* radii, RingWidget* widget) RETAIL(FUN_00169f78);
    // The pulse effect (a curve scale of 1, 1.2, 1.3 and 1)
    CurveScaleEffect* PulseEffectConstruct(CurveScaleEffect* effect) RETAIL(FUN_00167160);

    // OLEG's pages, their parent page given or none: the screen's position (moved by the directions), the
    // extras, the main menu ("crash twinsanity"), the game over, the pause menu (handed what it doesn't use), the confirmations of
    // disabling the autosave and of quitting, an autosave notice's (continuing to the resume page, or a link that leads nowhere
    // when it's told to), a world's levels (its four, each with a ring and a widget of its own chained after the menu's), the
    // options, and the save manager's choices and save slots
    MenuPage* SaveChoicesPageConstruct(void* memory, SaveManager* manager) RETAIL(FUN_002a2258);
    MenuPage* SaveSlotsPageConstruct(void* memory, SaveManager* manager, MenuWidget* menu) RETAIL(FUN_00164840);
}
