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
#include "game/shapes.h"
#include "game/sound.h"
#include "game/string.h"
#include "game/widgeteffects.h"
#include "game/widgets.h"

class Font;
struct ChunkManager;
struct GamePad;

// A scale going round values (the titles' breathing), a cycle each period (clock units) from when it last began (0 before its first
// step), the scale between the values the cycle is at; with a limit it stops after that many cycles at its last value. Its scale is
// where a widget's scaler is read
struct CyclingScale
{
    enum Flags : u16
    {
        CountMask = 0x7FFF,
        Running = 0x8000,
    };

    u16 flags;
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

// The character the player plays (still unknown)
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
    void Unknown11() RETAIL(FUN_00171a28);
    void Draw(Renderer* renderer) RETAIL(FUN_001716d8);
};
CHECK_SIZE(LevelWidget, 0x98);

// What a save slot shows of its save (the save manager's)
struct SaveSummary
{
    enum Flags : u32
    {
        // Both set: the slot holds a save
        HasSave = 0x3000000,
    };

    enum Progress : u32
    {
        AreaMask = 0x3F,
        CharacterShift = 6,
        CharacterMask = 0xF,
        CrystalsShift = 10,
        CrystalsMask = 0x3F,
        LivesShift = 16,
        LivesMask = 0x7F,
        DoneShift = 23,
        DoneMask = 0x7F,
    };

    u32 unknown00;
    u32 flags;
    u8 unknown08[0x18 - 0x08];
    // Bits 0-5: the area it was saved in, 6-9: the character, 10-15: the crystals, 16-22: the lives and one, 23-29: how much of
    // the game is done (percent)
    u32 progress;
    // The time played (clock units)
    s32 time;

    // Its vtable (BanksHeader_methods, 0x14 bytes in) over game/savedevice.h's FolderSummary's (game/savemanager.cpp): 1 the
    // destructor (the folder summary's), 5 and 6 the progress and the time read and written before the folder summary's part
    void Destroy(u32 destroyFlags) RETAIL(FUN_00179ae0);
    void Read(Stream* stream) RETAIL(ReadBanksHeader);
    void Write(Stream* stream) RETAIL(WriteBanksHeader);
};

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

// OLEG: the in-game UI manager (the retail RendererRelatedInGameController, its vtable the retail one after the widget controller's
// members: 1 the destructor, 2 every widget hidden, 3 and 6 the widgets' 11 and 13, 4 the update, 5 the draw). The HUD, the pause
// menu and the front end's menus as widgets in its 64 slots (a member of the game controller, at 0x5E0). Its members
// are named by their place until what each shows is known
class OLEG : public WidgetController
{
public:
    // The sprite the middle sprite widget (sprite46A4) shows, a byte the reset clears, and bits 0-3 of 0x312 the gem colour the
    // sparkle effect plays in
    u8 unknown310;
    // Wumpa fruit to add to the count, one at a time
    u8 wumpaToAdd;
    u16 unknown312;
    // The pictures read from files into the second and third sprites and the tiles (the Crash title, the level's title, the legal
    // screens): bit 0 while one is being read, bits 1-8 the picture read, 9-16 the one wanted (1, 25 and 12 none)
    u32 pictures[3];
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
    String text;
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
    Sprite sprite81C;
    // The particles' curves
    Vector4Curve particleColours;
    Vector2Curve particleSizes;
    FloatCurve particleTurns[2];
    Emitter2D emitters[8];
    SparkleEffect sparkle;
    // The materials of the HUD's icons (SetHudIcon's slots: sprites 3 to 5), none for the flat material
    struct MaterialResource* hudIcons[3];
    Sprite sprites[46];
    Sprite tiles[8];
    TextLine textLine;
    SpriteWidget sprite1240;
    SpriteWidget sprite12FC;
    SpriteWidget sprite13B8;
    SpriteWidget sprite1474;
    StringLabel string1530;
    SpriteWidget sprite15CC;
    RingWidget ring1688;
    RingWidget ring171C;
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
    Label label2908;
    Label label299C;
    MenuWidget menu2A30;
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
    SpriteWidget extrasPicture;
    Label nextHint;
    MenuWidget gameOverMenu;
    StringLabel string3A4C;
    StringLabel string3AE8;
    MenuWidget saveChoicesMenu;
    StringLabel string3C30;
    RingWidget saveSlotsPanel;
    MenuWidget saveSlotsMenu;
    Label autosavingText;
    SpriteWidget autosavingIcon;
    StringLabel string3F5C;
    SpriteWidget sprite3FF8;
    StringLabel string40B4;
    SpriteWidget sprite4150;
    HudBarWidget bar420C;
    SliderWidget slider429C;
    StringLabel hudWumpaText;
    SpriteWidget hudWumpaSprite;
    StringLabel hudLivesText;
    SpriteWidget hudLivesSprite;
    SpriteWidget sprite45E8;
    SpriteWidget sprite46A4;
    StringLabel string4760;
    SpriteWidget sprite47FC;
    SpriteWidget sprite48B8;
    TiledPicture picture4974;
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
    // the widgets and the text line (its text copied into the subtitle)
    void Update(TimeClock* clock, GamePad* pad, PlayerCharacter* character) RETAIL(FUN_00171308);
    // A pickup's effect on a screen: the lives count (36) rocks, the pickup's sprite (38) sparkles in the gem's colour (the
    // crystal's for the crystal's sprite)
    void PlayPickupEffect(u32 screen) RETAIL(FUN_0016ce38);
    // The first widget a screen shows (none for a screen without any)
    Widget* ScreenWidget(u32 screen)
    {
        s32 index = IndexOf(masks[screen]);
        return index < 0 ? nullptr : widgets[index];
    }

    // The picture a picture wants read (LoadPicture reads it)
    void WantPicture(u32 picture, u32 wanted)
    {
        pictures[picture] = (pictures[picture] & ~(0xFFu << 9)) | (wanted & 0xFF) << 9;
    }

    // The first of its menus that's shown (but the game over's and the save manager's), none without one
    MenuWidget* ShownMenu() RETAIL(FUN_001715a0);
    // Its material for the 2D particles and its particle sprite, and the emitters' sprites
    void SetUpParticles() RETAIL(FUN_0016d0d0);
    // The HUD's values from the game's progress and the character
    void UpdateHud(PlayerCharacter* character) RETAIL(FUN_00169320);
    // The HUD's icon of a slot (sprites 3 to 5: 0 the boss's, 1 whack-a-worm's) shows an OGI's (its first rigid model's first
    // material; 0xFFFF the flat material; nothing changes for an ID without an OGI)
    void SetHudIcon(u32 slot, const u16* object) RETAIL(FUN_00171008);
    // Lives added (0 to 100), the count shown and rocked; celebrated, the extra life's head (sprite45E8: the character's) appears
    // sliding and spinning and its sound plays
    void AddLives(s32 lives, u32 celebrated) RETAIL(FUN_00171138);
    // The widgets, then its line of text
    void Draw(Renderer* renderer) RETAIL(FUN_0017a1d8);
    // A picture's sprites let go of what they drew, and the picture made none
    void ReleasePicture(u32 picture) RETAIL(FUN_00170bd8);
    // The picture wanted read from its file unless it's the one read (with the readers queued, read now when asked): the Crash
    // title, the level's title, or the legal screen, a loading screen, a game over screen or the credits (9 and 10: the file
    // its text names)
    void LoadPicture(u32 picture, u32 now) RETAIL(FUN_00170d30);
    // A picture's file read into its sprites, no longer being read
    void ReadPicture(u32 picture, Stream* stream) RETAIL(FUN_00179ea0);

    // Its set-up, a part at a time
    // Which widgets each of its 45 screens shows (the controller's masks)
    void SetUpScreens() RETAIL(FUN_00169d20);
    // The UI's flat material made, the first six sprites drawn with it
    void SetUpFlatSprites() RETAIL(FUN_0016cc08);
    void SetUp16A0F8() RETAIL(FUN_0016a0f8);
    // A ring widget made a bordered disc of the radii: a ring coloured by a curve and three a little bigger round it
    void SetUp16A278() RETAIL(FUN_0016a278);
    void SetUp16A518() RETAIL(FUN_0016a518);
    void SetUp16A740() RETAIL(FUN_0016a740);
    void SetUp16A890() RETAIL(FUN_0016a890);
    // The pause screen's rings: eight round its middle, the breathing scale's values, the curves
    void SetUpPauseRings() RETAIL(FUN_0016aa58);
    void SetUp16B718() RETAIL(FUN_0016b718);
    void SetUp16B938() RETAIL(FUN_0016b938);
    // The pause menu, its pages, and the progress round it: the completion, the gems on an arc, the level's picture, the wumpa
    // fruit, lives and crystals
    void SetUpPauseMenu() RETAIL(FUN_0016ba38);
    void SetUp16C9F8() RETAIL(FUN_0016c9f8);
};
CHECK_OFFSET(OLEG, pictures, 0x314);
CHECK_OFFSET(OLEG, pages, 0x760);
CHECK_OFFSET(OLEG, pulse, 0x328);
CHECK_OFFSET(OLEG, progress, 0x390);
CHECK_OFFSET(OLEG, sounds, 0x6F8);
CHECK_OFFSET(OLEG, material, 0x7A8);
CHECK_OFFSET(OLEG, particleColours, 0x83C);
CHECK_OFFSET(OLEG, emitters, 0x85C);
CHECK_OFFSET(OLEG, sparkle, 0xAC0);
CHECK_OFFSET(OLEG, sprites, 0xB4C);
CHECK_OFFSET(OLEG, textLine, 0x120C);
CHECK_OFFSET(OLEG, ring1688, 0x1688);
CHECK_OFFSET(OLEG, optionsMenu, 0x1A94);
CHECK_OFFSET(OLEG, autosaveOnIcon, 0x2660);
CHECK_OFFSET(OLEG, wumpaRing, 0x2ADC);
CHECK_OFFSET(OLEG, gemSprites, 0x3320);
CHECK_OFFSET(OLEG, levelsMenus, 0x3338);
CHECK_OFFSET(OLEG, autosavingIcon, 0x3EA0);
CHECK_OFFSET(OLEG, bar420C, 0x420C);
CHECK_OFFSET(OLEG, slider429C, 0x429C);
CHECK_OFFSET(OLEG, picture4974, 0x4974);
CHECK_OFFSET(OLEG, loadingText, 0x4ABC);

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
    // OLEG's file's places (its static initialiser fills them)
    extern Vector2 g_OlegPlace628 RETAIL(D_0030A628);
    extern Vector2 g_OlegPlace630 RETAIL(D_0030A630);
    // The menus' items' place and size (the lists' drawers')
    extern Vector2 g_OlegPlace638 RETAIL(D_0030A638);
    extern Vector2 g_OlegPlace640 RETAIL(D_0030A640);
    extern Vector2 g_OlegPlace648 RETAIL(D_0030A648);
    extern Vector2 g_OlegPlace650 RETAIL(D_0030A650);
    extern Vector2 g_OlegPlace658 RETAIL(D_0030A658);
    extern Vector2 g_OlegPlace660 RETAIL(D_0030A660);
    extern Vector2 g_OlegShadowOffset RETAIL(D_0030A610);
    extern u32 g_OlegShadowColour RETAIL(D_0030A618);
    extern u32 g_OlegColour678 RETAIL(D_0030A678);
    extern u32 g_OlegColour700 RETAIL(D_0030A700);
    extern u32 g_OlegColour708 RETAIL(D_0030A708);
    extern u32 g_OlegColour748 RETAIL(D_0030A748);
    extern u32 g_OlegColour750 RETAIL(D_0030A750);
    extern u32 g_OlegColour758 RETAIL(D_0030A758);
    extern u32 g_OlegColour760 RETAIL(D_0030A760);
    extern u32 g_OlegColour768 RETAIL(D_0030A768);
    extern u32 g_OlegColour770 RETAIL(D_0030A770);
    extern u32 g_OlegColour778 RETAIL(D_0030A778);
    extern Vector4Curve g_OlegColours798 RETAIL(D_0030A798);
    // The angles of the pause screen's rings and the gems' arc (15, 21 and 25 degrees, the arc from -36 round 122 degrees, its
    // last ring from -35 round 120)
    extern s32 g_OlegAngle710 RETAIL(D_0030A710);
    extern s32 g_OlegAngle718 RETAIL(D_0030A718);
    extern s32 g_OlegAngle720 RETAIL(D_0030A720);
    extern s32 g_OlegAngle728 RETAIL(D_0030A728);
    extern s32 g_OlegAngle730 RETAIL(D_0030A730);
    extern s32 g_OlegAngle738 RETAIL(D_0030A738);
    extern s32 g_OlegAngle740 RETAIL(D_0030A740);
    // Curves of OLEG's file's (its static initialiser makes them, the pause rings' set-up fills them)
    extern Vector2Curve g_OlegCurve780 RETAIL(D_0030A780);
    extern Vector2Curve g_OlegCurve788 RETAIL(D_0030A788);
    extern Vector4Curve g_OlegColours790 RETAIL(D_0030A790);
    // The titles' breathing scale
    extern CyclingScale g_OlegScaler RETAIL(D_0030BF10);
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
    extern Vector2 g_OlegPlace680 RETAIL(D_0030A680);
    extern Vector2 g_OlegPlace688 RETAIL(D_0030A688);
    extern Vector2 g_OlegPlace690 RETAIL(D_0030A690);
    extern Vector2 g_OlegPlace698 RETAIL(D_0030A698);
    extern Vector2 g_OlegPlace6A0 RETAIL(D_0030A6A0);
    extern Vector2 g_OlegPlace6A8 RETAIL(D_0030A6A8);
    extern Vector2 g_OlegPlace6B0 RETAIL(D_0030A6B0);
    extern Vector2 g_OlegPlace6B8 RETAIL(D_0030A6B8);
    extern u32 g_OlegColour6C0 RETAIL(D_0030A6C0);
    extern u32 g_OlegColour6C8 RETAIL(D_0030A6C8);
    extern u32 g_OlegColour6D0 RETAIL(D_0030A6D0);
    extern u32 g_OlegColour6D8 RETAIL(D_0030A6D8);
    extern Vector2 g_OlegPlace6E0 RETAIL(D_0030A6E0);
    extern Vector2 g_OlegPlace6E8 RETAIL(D_0030A6E8);
    extern Vector2 g_OlegPlace6F0 RETAIL(D_0030A6F0);
    extern Vector2 g_OlegPlace6F8 RETAIL(D_0030A6F8);
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
