#include "game/oleg.h"

#include "game/animation.h"
#include "game/characters.h"
#include "game/clock.h"
#include "game/colour.h"
#include "game/context.h"
#include "game/gamecontroller.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/olegpages.h"
#include "game/overlay.h"
#include "game/pads.h"
#include "game/player.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/resources.h"
#include "game/savemanager.h"
#include "game/stream.h"
#include "game/vehicles.h"
#include "platform/graphics.h"
#include "platform/math.h"

namespace
{
// Memory for an object of a type
template <typename T> T* Allocate()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}

// Anchors: near the left and near the right, a fifth of the way across
constexpr f32 AnchorLeft = Rounded(0.05);
constexpr f32 AnchorRight = Rounded(0.95);
constexpr f32 AnchorFifth = Rounded(0.2);
// The overlay's layers (drawn in their order, widgets in 3 by default): the dimmer, the fader and the black screen behind
// everything, the cutscene's bars, the bottom text's backdrop and the menu rings over them, the screen picture, the logos on top
constexpr u32 ShadeLayer = 1;
constexpr u32 BackgroundLayer = 2;
constexpr u32 PictureLayer = 4;
constexpr u32 LogoLayer = 5;
// The pulse's curve
constexpr u32 PulsePoints = 4;
// The lists' item spacing (a twentieth), and how many items they show at a time
constexpr f32 ListSpacing = Rounded(0.05);
constexpr u32 ListWindow = 7;
// A swaying sprite's sway: π/20 either way about a quarter turn
constexpr f32 SwayAngle = 0x1.41B2F8p-3f;

// The game's texts its labels show: the hints (select, back, the pages either way, cancel, next), the controller to insert, the
// disc error and the disc to replace, the autosave notices' (enabled, disabled, how to enable it again, the warning about the
// autosave's icon, failed), the screen position's hint, the autosaving's and the loading screen's
// texts; a level's name (by the level), an unknown level's, the save slots' levels' names (by the level) and an empty slot's
constexpr u32 SelectHintText = 0x1C;
constexpr u32 BackHintText = 0x1D;
constexpr u32 PagesLeftHintText = 0x1E;
constexpr u32 PagesRightHintText = 0x1F;
constexpr u32 CancelHintText = 0xBB;
constexpr u32 NextHintText = 0xB2;
constexpr u32 InsertControllerText = 0x20;
constexpr u32 DiscErrorTitleText = 0x21;
constexpr u32 ReplaceDiscText = 0x22;
constexpr u32 AutosaveEnabledText = 0x23;
constexpr u32 AutosaveDisabledText = 0x24;
constexpr u32 EnableAutosaveText = 0x25;
constexpr u32 AutosaveWarningText = 0x42;
constexpr u32 AutosaveFailedText = 0xB3;
constexpr u32 ScreenPositionHintText = 0x26;
constexpr u32 AutosavingText = 0x43;
constexpr u32 LoadingText = 0x57;
constexpr u32 LevelNameTexts = 0x47;
constexpr u32 UnknownLevelText = 0x58;
constexpr u32 SlotLevelNameTexts = 0xBC;
constexpr u32 EmptySlotText = 0x3C;

// The widgets' slots (the controller's), in the order of OLEG's members: a slot shows the widgets chained after its widget too
enum Slot : u32
{
    SlotLetterbox = 0,
    SlotFader = 1,
    SlotBottomTextBackdrop = 2,
    SlotBottomText = 3,
    SlotDimmer = 4,
    SlotMenuRings = 5,
    SlotMenuPanel = 6,
    SlotBackHint = 7,
    SlotCancelHint = 8,
    SlotSelectHint = 9,
    SlotPagesLeftHint = 10,
    SlotPagesRightHint = 11,
    SlotOptionsMenu = 12,
    SlotScreenPositionMenu = 13,
    SlotMainMenu = 14,
    SlotTitleLogo = 15,
    SlotPauseMenu = 16,
    SlotDisableAutosaveMenu = 17,
    SlotQuitMenu = 18,
    SlotPauseProgress = 19,
    SlotNoController = 20,
    SlotDiscError = 21,
    SlotAutosaveOff = 22,
    SlotAutosaveOn = 23,
    SlotAutosaveFailed = 24,
    SlotFourthNotice = 25,
    SlotLevelsMenus = 26,
    SlotLevelName = 30,
    SlotExtrasMenu = 31,
    SlotExtrasLogo = 32,
    SlotGameOverMenu = 33,
    SlotSaveMessage = 34,
    SlotSaveChoices = 35,
    SlotSaveSlots = 36,
    SlotAutosaving = 37,
    SlotTimedCount = 38,
    SlotHealthBar = 39,
    SlotSlider = 40,
    SlotHudWumpa = 41,
    SlotHudLives = 42,
    SlotPickup = 43,
    SlotAmmo = 44,
    SlotBlackout = 45,
    SlotScreenPicture = 46,
    SlotLoadingLogo = 47,
};

constexpr u64 SlotBit(u32 slot)
{
    return u64{1} << slot;
}

// What the menus' screens show with them: the dimmer and the menu rings, the menu panel, the hints of back and select and of the
// pages either way
constexpr u64 MenuBackground = SlotBit(SlotDimmer) | SlotBit(SlotMenuRings);
constexpr u64 MenuWithPanel = MenuBackground | SlotBit(SlotMenuPanel);
constexpr u64 BackAndSelect = SlotBit(SlotBackHint) | SlotBit(SlotSelectHint);
constexpr u64 PagesHints = SlotBit(SlotPagesLeftHint) | SlotBit(SlotPagesRightHint);

// The widget slots each screen shows
constexpr u64 ScreenWidgets[OLEG::Screens] = {
    SlotBit(SlotLetterbox) | SlotBit(SlotBottomText),
    SlotBit(SlotFader),
    SlotBit(SlotBottomTextBackdrop) | SlotBit(SlotBottomText),
    SlotBit(SlotDimmer),
    SlotBit(SlotBackHint),
    SlotBit(SlotCancelHint),
    SlotBit(SlotSelectHint),
    SlotBit(SlotPagesLeftHint),
    SlotBit(SlotPagesRightHint),
    MenuBackground | BackAndSelect | SlotBit(SlotOptionsMenu),
    MenuWithPanel | BackAndSelect | SlotBit(SlotScreenPositionMenu),
    MenuWithPanel | BackAndSelect | SlotBit(SlotMainMenu) | SlotBit(SlotTitleLogo),
    MenuWithPanel | BackAndSelect | PagesHints | SlotBit(SlotPauseMenu) | SlotBit(SlotPauseProgress),
    MenuWithPanel | BackAndSelect | PagesHints | SlotBit(SlotDisableAutosaveMenu),
    MenuWithPanel | BackAndSelect | PagesHints | SlotBit(SlotQuitMenu),
    MenuBackground | SlotBit(SlotNoController),
    MenuBackground | SlotBit(SlotDiscError),
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotAutosaveOff),
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotAutosaveOn),
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotAutosaveFailed),
    // The fourth notice's screen shows the first notice's widgets
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotAutosaveOff),
    MenuBackground | SlotBit(SlotBackHint) | PagesHints | SlotBit(SlotLevelsMenus) | SlotBit(SlotLevelName),
    MenuBackground | SlotBit(SlotBackHint) | PagesHints | SlotBit(SlotLevelsMenus + 1) | SlotBit(SlotLevelName),
    MenuBackground | SlotBit(SlotBackHint) | PagesHints | SlotBit(SlotLevelsMenus + 2) | SlotBit(SlotLevelName),
    MenuBackground | SlotBit(SlotBackHint) | PagesHints | SlotBit(SlotLevelsMenus + 3) | SlotBit(SlotLevelName),
    MenuBackground | BackAndSelect | PagesHints | SlotBit(SlotExtrasMenu),
    SlotBit(SlotBackHint) | SlotBit(SlotExtrasLogo) | SlotBit(SlotScreenPicture),
    SlotBit(SlotSelectHint) | SlotBit(SlotGameOverMenu) | SlotBit(SlotBlackout) | SlotBit(SlotScreenPicture),
    MenuBackground | SlotBit(SlotSaveMessage),
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotSaveChoices),
    MenuWithPanel | SlotBit(SlotSelectHint) | SlotBit(SlotSaveSlots),
    SlotBit(SlotAutosaving),
    SlotBit(SlotHudWumpa) | SlotBit(SlotHudLives),
    SlotBit(SlotTimedCount),
    SlotBit(SlotHealthBar) | SlotBit(SlotHudLives),
    SlotBit(SlotSlider),
    SlotBit(SlotHudLives),
    SlotBit(SlotHudWumpa),
    SlotBit(SlotPickup),
    SlotBit(SlotAmmo),
    SlotBit(SlotBlackout),
    SlotBit(SlotScreenPicture),
    SlotBit(SlotScreenPicture) | SlotBit(SlotLoadingLogo),
    ~(SlotBit(SlotLetterbox) | SlotBit(SlotFader) | SlotBit(SlotBottomTextBackdrop)),
    ~SlotBit(SlotPauseMenu),
};

// Its menus' pages: the screen's position, the extras, the main menu, the game over, the pause menu, the confirmations of
// disabling the autosave and of quitting, the four notices, the four worlds' levels, the options, the save code's choices and
// save slots
enum Page : u32
{
    PageScreenPosition = 0,
    PageExtras = 1,
    PageMainMenu = 2,
    PageGameOver = 3,
    PagePause = 4,
    PageDisableAutosave = 5,
    PageQuit = 6,
    PageNotices = 7,
    PageLevels = 11,
    PageOptions = 15,
    PageSaveChoices = 16,
    PageSaveSlots = 17,
    Pages = 18,
};

// Its menus' drawers: the screen position's, the extras', the other menus' (the main menu, the confirmations and the notices),
// the game over's, the pause menu's, the levels', the options', the save code's choices' and save slots'
enum Drawer : u32
{
    DrawerScreenPosition = 0,
    DrawerExtras = 1,
    DrawerMenus = 2,
    DrawerGameOver = 3,
    DrawerPause = 4,
    DrawerLevels = 5,
    DrawerOptions = 6,
    DrawerSaveChoices = 7,
    DrawerSaveSlots = 8,
};

// The ring widgets (but the panels'): the menu rings' eight, the completion's disc and border and the gem arc's four, the menu
// rings' steps round and the arc's
constexpr u32 MenuRings = 8;
constexpr u32 CompletionRings = 2;
constexpr u32 GemArcRings = 4;
constexpr u32 MenuRingSteps = 64;
constexpr u32 GemArcSteps = 24;
// The menus' sound slots the front end's move goes in (an item picked, the selection moved down and up, a value stepped up and
// down), and the front end's sounds: an item picked that leads on, the move, going back
constexpr u32 MoveSounds[] = {MenuSounds::SoundSelected, MenuSounds::SoundDown, MenuSounds::SoundUp, MenuSounds::SoundValueUp,
                              MenuSounds::SoundValueDown};
constexpr u32 FrontEndPicked = 0;
constexpr u32 FrontEndMove = 1;
constexpr u32 FrontEndBack = 2;

// The HUD: the emitters (the crystal's sparkle, the level widgets' first gem's too; the six gems'; Crash's lives'), the health
// bar's body
constexpr u32 CrystalEmitter = 0;
constexpr u32 FirstGemEmitter = 1;
constexpr u32 CrashLivesEmitter = 7;
constexpr u32 BarBody = OLEG::SpriteHudIcons + OLEG::HudIconBoss;
// The areas the four hubs are first entered in, whose titles are square (the areas going back to them, 5, 12, 19 and 24, show
// the same titles as wide as the levels')
constexpr u32 HubAreas[] = {0, 6, 13, 20};
// The pictures read from files: what each is without a picture, and the tiles' pictures named by the picture name
constexpr u32 NoPicture[OLEG::Pictures] = {OLEG::NoCrashTitle, OLEG::NoLevelTitle, OLEG::TilesNone};
constexpr u32 FirstNamedPicture = OLEG::TilesNamed;
constexpr u32 EndNamedPictures = OLEG::TilesCredits;

// The wumpa fruit: one added every 0.15 seconds (sooner the more there are), the count's pulse as long, the count shown in a
// tenth of a second for a second and a half (hidden as fast while paused), a hundred making a life. The lives (at most 100):
// their rock (0.45 seconds, 7 times), shown for a second and a half; the extra life's head appearing in half a second for a
// second, sliding and spinning for 0.75 seconds, its sound's volume (and the sound's pitch as it is)
constexpr f32 WumpaDelaySeconds = Rounded(0.15);
constexpr f32 CountShowSeconds = Rounded(0.1);
constexpr f32 CountHoldSeconds = 1.5f;
constexpr u32 WumpaPerLife = 100;
constexpr u32 MostLives = 100;
constexpr f32 RockSeconds = Rounded(0.45);
constexpr u32 RockRepeats = 7;
constexpr f32 HeadAppearSeconds = 0.5f;
constexpr f32 HeadMoveSeconds = 0.75f;
constexpr u32 ExtraLifeSound = 0x10A;
constexpr f32 ExtraLifeVolume = Rounded(0.2);
// The breathing scale: 30 values of 1 + 0.1 sin of a 60th of a turn more each, a cycle each half second
constexpr u16 BreathingValues = 30;
constexpr f32 BreathingStep = 0x1.ACEEA2p-4f;
constexpr f32 BreathingAmount = Rounded(0.1);
constexpr f32 BreathingSeconds = 0.5f;
// The particles' turns: a sixth of a turn
constexpr f32 SixthTurn = 0x1.0c1524p+0f;

// The health bar: its body's length (of the progress's bar length) and height, its pieces' width and height (fractions of its
// size); the slider's track's height
constexpr f32 BarLengthScale = Rounded(0.15);
constexpr f32 BarHeight = Rounded(0.2);
constexpr f32 BarPieceWidth = Rounded(0.03);
constexpr f32 BarPieceHeight = Rounded(0.04);
constexpr f32 SliderTrackHeight = Rounded(0.1);
// The level widgets: their bob (its phase's step a frame, wrapping at half a turn, and how far), the gems' spacing (a ninth of
// the widget), the emitters' step (a frame), the title's size and how far left of the middle it is (fractions of the widget)
constexpr f32 BobStep = 0x1.20E2C4p-4f;
constexpr f32 BobAmount = Rounded(0.005);
constexpr f32 GemSpacing = Rounded(1.0 / 9);
constexpr f32 FrameSeconds = Rounded(1.0 / 60);
constexpr f32 TitleWidth = Rounded(0.2);
constexpr f32 TitleHeight = Rounded(1.6);
constexpr f32 TitleOffset = Rounded(0.35);
// The save slots: a minute's share of a second
constexpr f32 MinutesPerSecond = Rounded(1.0 / 60);

// A widget effect made: the base's members, then its own vtable
void ConstructEffect(WidgetEffect* effect, const GccVTableEntry* vtable)
{
    effect->vtable = g_WidgetEffectVTable;
    effect->next = nullptr;
    effect->WidgetEffect::Reset();
    effect->vtable = vtable;
}

Material* OwnMaterial(OLEG* oleg)
{
    return reinterpret_cast<Material*>(oleg->material);
}

// A drawer's styles but the title's in a font: the selected item white, the others light and dark grey
void SetItemStyles(MenuDrawer* drawer, Font* font)
{
    const s32 colours[3] = {ColourWhite, ColourLightGrey, ColourDarkGrey};
    for (u32 style = MenuDrawer::StyleSelected; style <= MenuDrawer::StyleHidden; style++)
    {
        drawer->fonts[style] = font;
        GetColor(&drawer->colours[style], colours[style - MenuDrawer::StyleSelected]);
    }
}

// A list's drawer: no title, the items in the middle at the menus' place and size, a twentieth apart, the selected one pulsing
// with the titles' breathing
void ListStyle(MenuDrawer* drawer, Font* font)
{
    drawer->titlePlace = {0.0f, 0.0f};
    drawer->titleScale = {0.0f, 0.0f};
    drawer->itemAlignment = TextAlignment::Centred;
    drawer->itemsPlace = g_OlegListItemsPlace;
    drawer->manyItemsScale = g_OlegListItemsScale;
    drawer->manyItemsSpacing = ListSpacing;
    SetItemStyles(drawer, font);
    drawer->sizePulse = &g_BreathingScale;
}

// A string label's destructor, inline in OLEG's: its text, then the widget's
void DestroyStringLabel(StringLabel* label)
{
    StringDestroy(&label->text);
    label->Widget::Destroy(DestroyOnly);
}

u32 White()
{
    u32 colour;
    GetColor(&colour, ColourWhite);
    return colour;
}

// A widget's hidden and shown values set as they are
void SetPlaces(AnimatedWidget* widget, u32 hiddenColour, u32 shownColour, const Vector2* hiddenPlace, const Vector2* shownPlace,
               const Vector2* hiddenScale, const Vector2* shownScale)
{
    widget->hiddenColour = hiddenColour;
    widget->shownColour = shownColour;
    widget->shownPlace = *shownPlace;
    widget->hiddenPlace = *hiddenPlace;
    widget->shownScale = *shownScale;
    widget->hiddenScale = *hiddenScale;
}

void SetLayer(Widget* widget, u32 layer)
{
    widget->flags.layer = layer;
}

// A sprite widget's pulse (every 3 seconds, a tenth bigger) and its sway about a quarter turn (π/20 either way every 7 seconds)
void Swaying(SpriteWidget* widget)
{
    widget->pulseAmount = Rounded(0.1);
    widget->wobblePeriod = 7.0f;
    widget->wobbleBase = HalfPi;
    widget->wobbleAmount = SwayAngle;
    widget->pulsePeriod = 3.0f;
}

// The UI's drop shadow
void SetShadow(Widget* widget)
{
    widget->shadowColour = g_OlegShadowColour;
    widget->shadowOffset = g_OlegShadowOffset;
}

// A widget drawn or not
void SetVisible(Widget* widget, bool visible)
{
    widget->flags.invisible = visible ? 0 : 1;
}

// A number's decimal digits added to a string
void AppendNumber(String* string, u32 value)
{
    String number;
    StringConstructNumber(&number, value);
    StringAppend(string, number.string);
    StringDestroy(&number);
}

void SpritesTurned(Sprite* sprites, u32 first, u32 end)
{
    for (u32 sprite = first; sprite < end; sprite++)
    {
        sprites[sprite].turned = 1;
    }
}

// A level's crystal
u32 CrystalFound(const LevelProgress* level)
{
    return level->crystal;
}
}

extern "C" CurveScaleEffect* PulseEffectConstruct(CurveScaleEffect* effect)
{
    effect->next = nullptr;
    effect->vtable = g_WidgetEffectVTable;
    effect->WidgetEffect::Reset();
    effect->curve.count = PulsePoints;
    effect->vtable = g_PulseEffectVTable;
    effect->curve.points = static_cast<Vector2*>(MemoryAllocate2(PulsePoints * sizeof(Vector2)));
    effect->curve.points[0] = {1.0f, 1.0f};
    effect->curve.points[1] = {Rounded(1.2), Rounded(1.2)};
    effect->curve.points[2] = {Rounded(1.3), Rounded(1.3)};
    effect->curve.points[3] = {1.0f, 1.0f};
    return effect;
}

void OLEG::SetUpScreens()
{
    for (u32 screen = 0; screen < Screens; screen++)
    {
        screens[screen] = ScreenWidgets[screen];
    }
}

void OLEG::SetUpParticles()
{
    Material* own = OwnMaterial(this);
    particleShader = Platform::Graphics::MakeParticleMaterial(own);
    Vector2 start = {0.25f, 0.0f};
    Vector2 end = {0.5f, 0.25f};
    particleSprite.SetMaterial(own, &start, &end);

    // The particles grow to half as big again and shrink away, and brighten and fade
    const f32 sizes[8] = {0.5f, Rounded(0.6), Rounded(0.8), 1.25f, 1.5f, Rounded(1.1), Rounded(0.6), Rounded(0.1)};
    for (u32 point = 0; point < 8; point++)
    {
        particleSizes.points[point] = {sizes[point], sizes[point]};
    }

    const Vector4 colours[8] = {
        {Rounded(0.2), Rounded(0.2), Rounded(0.2), Rounded(0.1)}, {Rounded(0.6), Rounded(0.6), Rounded(0.6), 0.5f}, {Rounded(0.8), Rounded(0.8), Rounded(0.8), Rounded(0.9)}, {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}, {Rounded(0.8), Rounded(0.8), Rounded(0.8), Rounded(0.9)}, {Rounded(0.4), Rounded(0.4), Rounded(0.4), 0.5f}, {Rounded(0.1), Rounded(0.1), Rounded(0.1), Rounded(0.1)},
    };
    for (u32 point = 0; point < 8; point++)
    {
        particleColours.points[point] = colours[point];
    }

    // A sixth of a turn, and a whole one
    particleTurns[0].values[0] = 0.0f;
    particleTurns[0].values[1] = SixthTurn;
    particleTurns[1].values[0] = 0.0f;
    particleTurns[1].values[1] = TwoPi;

    // The crystal's emitter (6 particles), the gems' (4 each), Crash's lives' (2) and the sparkle effect's (128)
    Emitter2D& crystal = emitters[CrystalEmitter];
    crystal.SetSprite(&particleSprite, 6);
    crystal.chance = Rounded(0.03);
    crystal.lifetime = 0.5f;
    crystal.spread = {Rounded(0.65), 0.75f};
    crystal.size = {Rounded(0.05), Rounded(0.05)};
    crystal.turns = &particleTurns[0];
    crystal.colours = &particleColours;
    crystal.sizes = &particleSizes;
    for (u32 gem = FirstGemEmitter; gem < FirstGemEmitter + GameProgress::Gems; gem++)
    {
        Emitter2D& emitter = emitters[gem];
        emitter.SetSprite(&particleSprite, 4);
        emitter.lifetime = Rounded(0.45);
        emitter.chance = Rounded(0.023);
        emitter.size = {Rounded(0.05), Rounded(0.05)};
        emitter.spread = {0.5f, 0.5f};
        emitter.colours = &particleColours;
        emitter.sizes = &particleSizes;
        emitter.turns = &particleTurns[1];
    }

    Emitter2D& lives = emitters[CrashLivesEmitter];
    lives.SetSprite(&particleSprite, 2);
    lives.lifetime = Rounded(0.4);
    lives.chance = Rounded(0.001);
    lives.size = {Rounded(0.05), Rounded(0.05)};
    lives.spread = {Rounded(0.33), 0.25f};
    lives.turns = &particleTurns[1];
    lives.colours = &particleColours;
    lives.sizes = &particleSizes;
    lives.offset = {-Rounded(0.01), Rounded(0.04)};

    RadialEmitter2D& sparkles = sparkle.emitter;
    sparkles.SetSprite(&particleSprite, 128);
    sparkles.size = {Rounded(0.052), Rounded(0.052)};
    sparkles.spread = {0.5f, 0.5f};
    sparkles.turns = &particleTurns[1];
    sparkles.colours = &particleColours;
    sparkles.sizes = &particleSizes;
    sparkle.duration = Rounded(1.6);
    sparkles.colour = {0.0f, 1.0f, 0.0f, 1.0f};
}

void OLEG::SetUpFlatSprites()
{
    Material* material = Platform::Graphics::MakeFlatMaterial();
    for (u32 sprite = 0; sprite < SpriteIcons; sprite++)
    {
        CallVirtual<void>(&sprites[sprite], sprites[sprite].vtable, Shape2D::SetMaterialSlot, material);
    }
}

void OLEG::SetUpTimeHud()
{
    Vector2 place = {Rounded(0.15), Rounded(0.8)};
    Vector2 scale = {1.0f, 1.0f};
    timedCountText.FadeIn(White(), &place, &scale);
    place = {Rounded(0.1), Rounded(0.9)};
    scale = {Rounded(0.1), Rounded(0.1)};
    timedCountIcon.FadeRectangle(White(), &place, &scale);
    place = {Rounded(0.15), Rounded(0.9)};
    scale = {1.0f, 1.0f};
    timeText.FadeIn(White(), &place, &scale);
    place = {Rounded(0.1), Rounded(0.9)};
    scale = {Rounded(0.1), Rounded(0.1)};
    clockIcon.FadeRectangle(White(), &place, &scale);
    timedCountText.next = &timedCountIcon;
    timedCountIcon.next = &timeText;
    timeText.next = &clockIcon;
}

void OLEG::SetUpOverlays()
{
    SetPlaces(&letterboxTop, g_OlegLetterboxColour, g_OlegLetterboxColour, &g_OlegTopBarHiddenFrom, &g_OlegTopBarFrom,
              &g_OlegTopBarHiddenTo, &g_OlegTopBarTo);
    SetLayer(&letterboxTop, BackgroundLayer);
    SetPlaces(&letterboxBottom, g_OlegLetterboxColour, g_OlegLetterboxColour, &g_OlegBottomBarHiddenFrom, &g_OlegBottomBarFrom,
              &g_OlegBottomBarHiddenTo, &g_OlegBottomBarTo);
    SetLayer(&letterboxBottom, BackgroundLayer);
    u32 hidden;
    u32 shown;
    GetColor(&hidden, ColourTransparentBlack);
    GetColor(&shown, ColourBlack);
    fader.hiddenColour = hidden;
    fader.shownColour = shown;
    SetLayer(&fader, ShadeLayer);
    SetPlaces(&bottomTextBackdrop, g_OlegBackdropHiddenColour, g_OlegBackdropColour, &g_OlegBackdropFrom, &g_OlegBackdropFrom,
              &g_OlegBackdropTo, &g_OlegBackdropTo);
    SetLayer(&bottomTextBackdrop, BackgroundLayer);
    SetPlaces(&bottomText, g_OlegBottomTextHiddenColour, g_OlegBottomTextColour, &g_OlegBottomTextPlace, &g_OlegBottomTextPlace,
              &g_OlegBottomTextScale, &g_OlegBottomTextScale);
    letterboxTop.next = &letterboxBottom;
}

extern "C" void AddPanelRings(const Vector2* radii, RingWidget* widget)
{
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* inner = widget->AddRing(0, material);
    Ring* border1 = widget->AddRing(1, material);
    Ring* border2 = widget->AddRing(2, material);
    Ring* border3 = widget->AddRing(3, material);
    inner->outerScale = *radii;
    inner->outerColours = &g_OlegPanelColours;
    inner->middleColour = g_OlegPanelColour;
    border1->outerScale = {radii->x + Rounded(0.003), radii->y + Rounded(0.003)};
    border1->colour = g_OlegRingBorderColour;
    border2->outerScale = {radii->x + Rounded(0.01), radii->y + Rounded(0.01)};
    border2->colour = g_OlegRingBorderColour;
    border3->outerScale = {radii->x + Rounded(0.013), radii->y + Rounded(0.013)};
    border3->colour = g_OlegRingEdgeColour;
    SetShadow(widget);
}

void OLEG::SetUpExtras()
{
    Vector2 place = {0.5f, Rounded(0.1)};
    Vector2 scale = {1.0f, 1.0f};
    extrasMenu.FadeIn(White(), &place, &scale);
    extrasPanel.FadeIn(White(), &place, &scale);
    place = {Rounded(0.17), Rounded(0.13)};
    scale = {Rounded(0.24), Rounded(0.16)};
    extrasLogo.FadeRectangle(White(), &place, &scale);
    place = {Rounded(0.05), Rounded(0.95)};
    scale = {0.625f, 0.625f};
    nextHint.FadeIn(White(), &place, &scale);
    Vector2 radii = {Rounded(0.35), Rounded(0.05)};
    AddPanelRings(&radii, &extrasPanel);
    Swaying(&extrasLogo);
    SetLayer(&extrasLogo, LogoLayer);
    extrasMenu.next = &extrasPanel;
    extrasLogo.next = &nextHint;
}

void OLEG::SetUpMainMenu()
{
    Swaying(&titleLogo);
    SetLayer(&titleLogo, LogoLayer);
    SetShadow(&titleLogo);
    Vector2 scale = {1.0f, 1.0f};
    mainMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    Vector2 middle = {0.5f, Rounded(0.35)};
    Vector2 size = {Rounded(0.7), Rounded(0.45)};
    titleLogo.GrowRectangle(White(), &middle, &size, &g_OlegTitlePlace);
}

void OLEG::SetUpLoadingScreen()
{
    Swaying(&loadingLogo);
    SetLayer(&loadingLogo, LogoLayer);
    loadingText.scaler = &g_BreathingScale;
    screenPicture.shownColour = g_OlegPictureColour;
    screenPicture.hiddenColour = g_OlegPictureHiddenColour;
    SetLayer(&screenPicture, PictureLayer);
    u32 hidden;
    u32 shown;
    GetColor(&hidden, ColourTransparentBlack);
    GetColor(&shown, ColourBlack);
    blackout.hiddenColour = hidden;
    blackout.shownColour = shown;
    SetLayer(&blackout, ShadeLayer);
    Vector2 middle = {Rounded(0.2), Rounded(0.15)};
    Vector2 size = {Rounded(0.3), Rounded(0.2)};
    loadingLogo.FadeRectangle(White(), &middle, &size);
    Vector2 place = {0.5f, Rounded(0.9)};
    Vector2 scale = {0.75f, 0.75f};
    loadingText.FadeIn(White(), &place, &scale);
    loadingLogo.next = &loadingText;
}

void OLEG::SetUpLevelsMenus()
{
    Vector2 place = {0.5f, Rounded(0.05)};
    Vector2 scale = {1.0f, 1.0f};
    for (MenuWidget& menu : levelsMenus)
    {
        menu.FadeIn(White(), &place, &scale);
    }

    levelName.scaler = &g_BreathingScale;
    place = {0.5f, 0.75f};
    scale = {0.625f, 0.625f};
    Vector2 offset = {0.0f, 0.5f};
    levelName.SlideIn(White(), &place, &scale, &offset);
    scale = {1.0f, 1.0f};
    levelNamePanel.SlideIn(White(), &place, &scale, &offset);
    Vector2 radii = {Rounded(0.3), Rounded(0.05)};
    AddPanelRings(&radii, &levelNamePanel);
    levelName.next = &levelNamePanel;
}

void OLEG::SetUpOptionsMenu()
{
    Vector2 place = {0.5f, Rounded(0.1)};
    Vector2 scale = {1.0f, 1.0f};
    optionsMenu.FadeIn(White(), &place, &scale);
    optionsPanel.FadeIn(White(), &place, &scale);
    Vector2 radii = {Rounded(0.35), Rounded(0.05)};
    AddPanelRings(&radii, &optionsPanel);
    optionsMenu.next = &optionsPanel;
}

void OLEG::SetUpPauseMenu()
{
    Vector2 scale = {1.0f, 1.0f};
    pauseMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    disableAutosaveMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    quitMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    Vector2 place = {0.5f, 0.25f};
    Vector2 from = {0.5f, Rounded(0.3)};
    discErrorTitle.GrowIn(White(), &place, &scale, &from);
    disableAutosaveTitle.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    quitTitle.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    scale = {0.75f, 0.75f};
    noControllerText.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    scale = {Rounded(0.8), Rounded(0.8)};
    discErrorText.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    scale = {Rounded(0.85), Rounded(0.85)};
    autosaveOffTitle.GrowIn(White(), &place, &scale, &g_OlegTitlePlace);
    autosaveOffText.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegTitlePlace);
    scale = {1.0f, 1.0f};
    autosaveOffMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    autosaveOnTitle.GrowIn(White(), &g_OlegNoticeTitlePlace, &g_OlegNoticeTitleScale, &g_OlegTitlePlace);
    autosaveOnText.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegTitlePlace);
    Vector2 middle = {0.5f, Rounded(0.675)};
    Vector2 size = {Rounded(0.1), Rounded(0.075)};
    autosaveOnIcon.GrowRectangle(White(), &middle, &size, &g_OlegMenuPlace);
    autosaveOnMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    autosaveFailedText.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegTitlePlace);
    autosaveFailedMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    scale = {Rounded(0.85), Rounded(0.85)};
    fourthNoticeTitle.GrowIn(White(), &place, &scale, &g_OlegTitlePlace);
    fourthNoticeText.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegTitlePlace);
    scale = {1.0f, 1.0f};
    fourthNoticeMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    disableAutosaveMenu.next = &disableAutosaveTitle;
    quitMenu.next = &quitTitle;
    discErrorTitle.next = &discErrorText;
    autosaveOffTitle.next = &autosaveOffText;
    autosaveOffText.next = &autosaveOffMenu;
    autosaveOnTitle.next = &autosaveOnIcon;
    autosaveOnIcon.next = &autosaveOnText;
    autosaveOnText.next = &autosaveOnMenu;
    autosaveFailedText.next = &autosaveFailedMenu;
    fourthNoticeTitle.next = &fourthNoticeText;
    fourthNoticeText.next = &fourthNoticeMenu;

    // The completion's disc and its border, and the gems' arc: four rings about a third of the way round on the right
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* completion = completionRing.AddRing(0, material);
    Ring* completionBorder = completionRing.AddRing(1, material);
    Ring* arcs[GemArcRings];
    for (u32 ring = 0; ring < GemArcRings; ring++)
    {
        arcs[ring] = gemArc.AddRing(ring, material);
    }

    completion->outerColours = &g_OlegGlintColours;
    completion->middleColour = g_OlegPanelColour;
    completion->outerScale = {Rounded(0.075), Rounded(0.1)};
    completionBorder->colour = g_OlegPanelClearColour;
    completionBorder->outerScale = {Rounded(0.077), Rounded(0.102)};
    const u32 middleColours[GemArcRings] = {g_OlegGemArcColour, g_OlegGemArcClearColour, g_OlegGemArcColour,
                                            g_OlegGemArcTrimColour};
    const u32 colours[GemArcRings] = {g_OlegGemArcColour, g_OlegGemArcColour, g_OlegGemArcClearColour, g_OlegGemArcTrimColour};
    Vector2Curve* const innerShapes[GemArcRings] = {&g_OlegShrinkingShape, &g_OlegShrinkingShape, &g_OlegGrowingShape,
                                                    &g_OlegShrinkingShape};
    Vector2Curve* const outerShapes[GemArcRings] = {&g_OlegGrowingShape, &g_OlegShrinkingShape, &g_OlegGrowingShape,
                                                    &g_OlegGrowingShape};
    const Vector2 innerScales[GemArcRings] = {
        {Rounded(0.33), Rounded(0.308)},
        {Rounded(0.325), Rounded(0.303)},
        {Rounded(0.42), Rounded(0.392)},
        {Rounded(0.338), Rounded(0.315)},
    };
    const Vector2 outerScales[GemArcRings] = {
        {Rounded(0.42), Rounded(0.392)},
        {Rounded(0.33), Rounded(0.308)},
        {Rounded(0.425), Rounded(0.397)},
        {Rounded(0.413), Rounded(0.385)},
    };
    for (u32 ring = 0; ring < GemArcRings; ring++)
    {
        arcs[ring]->startAngle = ring < GemArcRings - 1 ? g_OlegGemArcStart : g_OlegGemArcLastStart;
        arcs[ring]->span = ring < GemArcRings - 1 ? g_OlegGemArcSpan : g_OlegGemArcLastSpan;
        arcs[ring]->middleColour = middleColours[ring];
        arcs[ring]->colour = colours[ring];
        arcs[ring]->innerShape = innerShapes[ring];
        arcs[ring]->outerShape = outerShapes[ring];
        arcs[ring]->innerScale = innerScales[ring];
        arcs[ring]->outerScale = outerScales[ring];
        arcs[ring]->rotation = g_OlegGemArcTurn;
    }

    gemArc.ringFlags.standApart = 1;
    const Vector2 radii = {Rounded(0.066), Rounded(0.088)};
    AddPanelRings(&radii, &wumpaRing);
    AddPanelRings(&radii, &livesRing);
    AddPanelRings(&radii, &crystalRing);

    place = {Rounded(0.71), 0x1.20C49Ap-1f};
    from = {0.5f, 0.5f};
    completionRing.GrowIn(White(), &place, &scale, &from);
    completionText.GrowIn(White(), &place, &scale, &from);
    place = {Rounded(0.53), 0x1.B020C6p-2f};
    Vector2 offset = {0.5f, 0.0f};
    gemArc.SlideIn(White(), &place, &scale, &offset);
    offset = {-0.5f, 0.0f};
    place = {Rounded(0.175), 0x1.B645A4p-3f};
    wumpaRing.SlideIn(White(), &place, &scale, &offset);
    place = {Rounded(0.16), 0x1.B645A4p-3f};
    wumpaText.SlideIn(White(), &place, &scale, &offset);
    middle = {Rounded(0.198), Rounded(0.199)};
    size = {Rounded(0.1), Rounded(0.13)};
    wumpaSprite.SlideRectangle(White(), &middle, &size, &offset);
    place = {Rounded(0.14), 0x1.C6A7EEp-2f};
    livesRing.SlideIn(White(), &place, &scale, &offset);
    place = {Rounded(0.108), 0x1.D0E55Ep-2f};
    livesText.SlideIn(White(), &place, &scale, &offset);
    middle = {Rounded(0.153), 0x1.B53F7Ep-2f};
    size = {Rounded(0.13), Rounded(0.13)};
    livesSprite.SlideRectangle(White(), &middle, &size, &offset);
    place = {0x1.CCCCCEp-3f, Rounded(0.664)};
    crystalRing.SlideIn(White(), &place, &scale, &offset);
    place = {Rounded(0.2), 0x1.547AE0p-1f};
    crystalText.SlideIn(White(), &place, &scale, &offset);
    middle = {Rounded(0.245), Rounded(0.664)};
    size = {0.125f, Rounded(0.28)};
    crystalSprite.SlideRectangle(White(), &middle, &size, &offset);
    wumpaSprite.pulseAmount = Rounded(0.1);
    wumpaSprite.pulsePeriod = Rounded(1.3);
    livesSprite.bobPeriod = Rounded(0.37);
    livesSprite.wobbleAmount = 0x1.921FB6p-3f;
    livesSprite.bobAmount = Rounded(0.0037);
    livesSprite.wobblePeriod = 1.0f;
    crystalSprite.bobPeriod = Rounded(0.7);
    crystalSprite.bobAmount = Rounded(0.005);

    // The gems in the middles of the arc's sixths
    for (s32 gem = 0; gem < static_cast<s32>(GameProgress::Gems); gem++)
    {
        gemArc.PointAt(&place, static_cast<f32>(gem * 2 + 1) * Rounded(1.0 / 12));
        size = {Rounded(0.1), Rounded(0.1)};
        offset = {0.5f, 0.0f};
        gemSprites[gem]->SlideRectangle(White(), &place, &size, &offset);
    }

    completionRing.next = &completionText;
    completionText.next = &gemArc;
    gemArc.next = &levelPicture;
    levelPicture.next = &wumpaRing;
    wumpaRing.next = &livesRing;
    livesRing.next = &crystalRing;
    crystalRing.next = &wumpaSprite;
    wumpaSprite.next = &wumpaText;
    wumpaText.next = &livesText;
    livesText.next = &livesSprite;
    livesSprite.next = &crystalText;
    crystalText.next = &crystalSprite;
    crystalSprite.next = gemSprites[0];
    for (u32 gem = 0; gem + 1 < GameProgress::Gems; gem++)
    {
        gemSprites[gem]->next = gemSprites[gem + 1];
    }
}

void OLEG::SetUpSaveScreens()
{
    saveMessage.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegNoticeTextPlace);
    saveChoicesMessage.GrowIn(White(), &g_OlegNoticeTextPlace, &g_OlegNoticeTextScale, &g_OlegNoticeTextPlace);
    Vector2 scale = {1.0f, 1.0f};
    saveChoicesMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    Vector2 place = {0.5f, Rounded(0.1)};
    Vector2 offset = {0.0f, -0.5f};
    saveSlotsTitle.SlideIn(White(), &place, &g_OlegNoticeTextScale, &offset);
    saveSlotsPanel.SlideIn(White(), &place, &scale, &offset);
    saveSlotsMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    Vector2 radii = {Rounded(0.45), Rounded(0.05)};
    AddPanelRings(&radii, &saveSlotsPanel);
    saveChoicesMessage.next = &saveChoicesMenu;
    saveSlotsTitle.next = &saveSlotsPanel;
    saveSlotsPanel.next = &saveSlotsMenu;
}

void OLEG::SetUpMenuRings()
{
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* rings[MenuRings];
    for (u32 ring = 0; ring < MenuRings; ring++)
    {
        rings[ring] = menuRings.AddRing(ring, material);
    }

    // The breathing scale: 1 + 0.1 sin of a 60th of a turn more each step, from the first
    for (u32 step = 0; step < BreathingValues; step++)
    {
        s32 angle;
        AngleFrom(&angle, static_cast<f32>(step) * BreathingStep, AngleRadians);
        f32 value = SinOfAngle(&angle) * BreathingAmount + 1.0f;
        g_BreathingScale.values[step] = value;
        if (step == 0)
        {
            g_BreathingScale.scale = value;
        }
    }

    s32 period = static_cast<s32>(g_ClockUnitsPerSecond * BreathingSeconds);
    if (!g_BreathingScale.flags.running)
    {
        g_BreathingScale.limited = 0;
        g_BreathingScale.cyclesLeft = 0;
        g_BreathingScale.period = period;
        g_BreathingScale.time = 0;
        g_BreathingScale.flags.running = 1;
    }

    const f32 grow[8] = {Rounded(0.96), Rounded(0.97), Rounded(0.98), Rounded(0.98),
                         Rounded(0.98), Rounded(0.99), 1.0f,          Rounded(1.02)};
    const f32 shrink[8] = {Rounded(1.03), Rounded(1.02), Rounded(1.02),  Rounded(1.02),
                           Rounded(1.01), Rounded(1.005), Rounded(0.988), Rounded(0.97)};
    for (u32 point = 0; point < 8; point++)
    {
        g_OlegGrowingShape.points[point] = {grow[point], grow[point]};
    }

    for (u32 point = 0; point < 8; point++)
    {
        g_OlegShrinkingShape.points[point] = {shrink[point], shrink[point]};
    }

    const Vector4 dim = {Rounded(0.1), Rounded(0.588), 1.0f, 1.0f};
    for (u32 point = 0; point < 16; point++)
    {
        g_OlegGlintColours.points[point] = dim;
    }

    g_OlegGlintColours.points[5] = {Rounded(0.3), Rounded(0.788), 1.0f, 1.0f};
    g_OlegGlintColours.points[6] = {Rounded(0.55), Rounded(0.925), 1.0f, 1.0f};
    g_OlegPanelColours.points[0] = dim;
    g_OlegPanelColours.points[1] = {Rounded(0.1), Rounded(0.6), 1.0f, 1.0f};
    g_OlegPanelColours.points[2] = {Rounded(0.4), Rounded(0.8), 1.0f, 1.0f};
    g_OlegPanelColours.points[3] = {Rounded(0.55), Rounded(0.925), 1.0f, 1.0f};
    g_OlegPanelColours.points[4] = {Rounded(0.1), Rounded(0.68), 1.0f, 1.0f};
    g_OlegPanelColours.points[5] = {Rounded(0.1), Rounded(0.4), 1.0f, 1.0f};
    g_OlegPanelColours.points[6] = {Rounded(0.1), Rounded(0.35), 1.0f, 1.0f};
    g_OlegPanelColours.points[7] = dim;

    Vector2 place = {Rounded(0.95), Rounded(0.95)};
    Vector2 scale = {0.625f, 0.625f};
    backHint.FadeIn(White(), &place, &scale);
    cancelHint.FadeIn(White(), &place, &scale);
    place = {Rounded(0.05), Rounded(0.95)};
    selectHint.FadeIn(White(), &place, &scale);
    place = {Rounded(0.05), Rounded(0.9)};
    scale = {0.75f, 0.75f};
    pagesLeftHint.FadeIn(White(), &place, &scale);
    place = {Rounded(0.95), Rounded(0.9)};
    pagesRightHint.FadeIn(White(), &place, &scale);
    scale = {1.0f, 1.0f};
    menuRings.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    menuPanel.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);

    // The inner disc and its border, the middle disc and its border (turned more), the outer ring with the glint and its borders
    const s32 rotations[MenuRings] = {g_OlegMenuRingsTurn, g_OlegMenuRingsTurn, g_OlegMenuMiddleRingsTurn,
                                      g_OlegMenuMiddleRingsTurn, g_OlegMenuRingsTurn, g_OlegMenuRingsTurn,
                                      g_OlegMenuRingsTurn, g_OlegMenuRingsTurn};
    for (u32 ring = 0; ring < MenuRings; ring++)
    {
        rings[ring]->rotation = rotations[ring];
    }

    rings[0]->middleColour = g_OlegPanelColour;
    rings[0]->colour = g_OlegPanelColour;
    rings[1]->colour = g_OlegRingBorderColour;
    rings[2]->colour = g_OlegRingBorderColour;
    rings[3]->colour = g_OlegPanelColour;
    rings[4]->outerColours = &g_OlegGlintColours;
    rings[5]->colour = g_OlegRingBorderColour;
    rings[6]->colour = g_OlegRingBorderColour;
    rings[7]->colour = g_OlegRingEdgeColour;
    const Vector2 radii[MenuRings] = {
        {Rounded(0.17), Rounded(0.15)},  {Rounded(0.173), Rounded(0.153)}, {Rounded(0.3), 0.25f},
        {Rounded(0.303), Rounded(0.253)}, {0.375f, Rounded(0.35)},          {Rounded(0.378), Rounded(0.353)},
        {Rounded(0.385), Rounded(0.36)},  {Rounded(0.388), Rounded(0.363)},
    };
    for (u32 ring = 0; ring < MenuRings; ring++)
    {
        rings[ring]->outerScale = radii[ring];
    }

    SetLayer(&menuRings, BackgroundLayer);
    Vector2 panel = {Rounded(0.12), Rounded(0.16)};
    AddPanelRings(&panel, &menuPanel);
    u32 hidden;
    u32 shown;
    ColourSet(&hidden, 0.0f, 0.0f, 0.0f, 0.0f);
    ColourSet(&shown, 0.0f, 0.0f, 0.0f, 0.75f);
    dimmer.hiddenColour = hidden;
    dimmer.shownColour = shown;
    SetLayer(&dimmer, ShadeLayer);
}

OLEG* OLEG::Construct(OLEG* oleg, Font* font, GameProgress* progress)
{
    WidgetController::Construct(oleg);
    oleg->savingShown = 0;
    oleg->vtable = g_OlegVTable;
    PulseEffectConstruct(&oleg->pulse);
    ConstructEffect(&oleg->slide, g_SlideEffectVTable);
    ConstructEffect(&oleg->spin, g_SpinEffectVTable);
    ConstructEffect(&oleg->rock, g_RockEffectVTable);
    oleg->progress = progress;
    oleg->font = font;
    oleg->pictureName.string = nullptr;
    oleg->pictureName.capacity = 0;
    oleg->pictureName.length = 0;
    ButtonBindings::Construct(&oleg->bindings, 0, MenuInput::Actions);
    MenuInput::Construct(&oleg->input, &oleg->bindings);
    for (MenuDrawer& drawer : oleg->drawers)
    {
        MenuDrawer::Construct(&drawer, font);
    }

    MenuSounds::Construct(&oleg->sounds);
    for (MenuPage*& page : oleg->pages)
    {
        page = nullptr;
    }

    Platform::Graphics::ConstructMaterial(OwnMaterial(oleg));
    oleg->particleShader = nullptr;
    Sprite::Construct(&oleg->particleSprite);
    oleg->particleColours.count = 8;
    oleg->particleColours.points = static_cast<Vector4*>(MemoryAllocate2(8 * sizeof(Vector4)));
    oleg->particleSizes.count = 8;
    oleg->particleSizes.points = static_cast<Vector2*>(MemoryAllocate2(8 * sizeof(Vector2)));
    for (FloatCurve& turns : oleg->particleTurns)
    {
        turns.count = 2;
        turns.values = static_cast<f32*>(MemoryAllocate2(2 * sizeof(f32)));
    }

    for (Emitter2D& emitter : oleg->emitters)
    {
        Emitter2D::Construct(&emitter);
    }

    ConstructEffect(&oleg->sparkle, g_SparkleEffectVTable);
    Emitter2D::Construct(&oleg->sparkle.emitter);
    oleg->sparkle.emitter.vtable = g_RadialEmitter2DVTable;
    for (Sprite& sprite : oleg->sprites)
    {
        Sprite::Construct(&sprite);
    }

    for (Sprite& tile : oleg->tiles)
    {
        Sprite::Construct(&tile);
    }

    Sprite* sprites = oleg->sprites;
    MenuInput* input = &oleg->input;
    MenuSounds* sounds = &oleg->sounds;
    MenuDrawer* drawers = oleg->drawers;
    TextLineConstruct(&oleg->textLine, font);
    SpriteWidget::Construct(&oleg->letterboxTop, &sprites[SpriteFlat]);
    SpriteWidget::Construct(&oleg->letterboxBottom, &sprites[SpriteFlat]);
    SpriteWidget::Construct(&oleg->fader, &sprites[SpriteFlat]);
    SpriteWidget::Construct(&oleg->bottomTextBackdrop, &sprites[SpriteFlat]);
    StringLabel::Construct(&oleg->bottomText, AnchorMiddle, font, TextAlignment::Centred);
    SpriteWidget::Construct(&oleg->dimmer, &sprites[SpriteFlat]);
    RingWidget::Construct(&oleg->menuRings, AnchorMiddle, MenuRings, RingSegments, MenuRingSteps, nullptr);
    RingWidget::Construct(&oleg->menuPanel, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    Label::Construct(&oleg->backHint, AnchorRight, font, BackHintText, TextAlignment::BottomRight);
    Label::Construct(&oleg->cancelHint, AnchorRight, font, CancelHintText, TextAlignment::BottomRight);
    Label::Construct(&oleg->selectHint, AnchorLeft, font, SelectHintText, TextAlignment::BottomLeft);
    Label::Construct(&oleg->pagesLeftHint, AnchorLeft, font, PagesLeftHintText, TextAlignment::BottomLeft);
    Label::Construct(&oleg->pagesRightHint, AnchorRight, font, PagesRightHintText, TextAlignment::BottomRight);
    MenuWidget::Construct(&oleg->optionsMenu, AnchorMiddle, input, &drawers[DrawerOptions], sounds);
    RingWidget::Construct(&oleg->optionsPanel, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    MenuWidget::Construct(&oleg->screenPositionMenu, AnchorMiddle, input, &drawers[DrawerScreenPosition], sounds);
    Label::Construct(&oleg->screenPositionHint, AnchorMiddle, font, ScreenPositionHintText, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->mainMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    SpriteWidget::Construct(&oleg->titleLogo, AnchorMiddle, &sprites[SpriteCrashTitle]);
    MenuWidget::Construct(&oleg->pauseMenu, AnchorMiddle, input, &drawers[DrawerPause], sounds);
    MenuWidget::Construct(&oleg->disableAutosaveMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    Label::Construct(&oleg->disableAutosaveTitle, AnchorMiddle, font, DisableAutosaveTitleText, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->quitMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    Label::Construct(&oleg->quitTitle, AnchorMiddle, font, QuitTitleText, TextAlignment::Centred);
    Label::Construct(&oleg->noControllerText, AnchorMiddle, font, InsertControllerText, TextAlignment::Centred);
    Label::Construct(&oleg->discErrorTitle, AnchorMiddle, font, DiscErrorTitleText, TextAlignment::Centred);
    Label::Construct(&oleg->discErrorText, AnchorMiddle, font, ReplaceDiscText, TextAlignment::Centred);
    Label::Construct(&oleg->autosaveOffTitle, AnchorMiddle, font, AutosaveDisabledText, TextAlignment::Centred);
    Label::Construct(&oleg->autosaveOffText, AnchorMiddle, font, EnableAutosaveText, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->autosaveOffMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    Label::Construct(&oleg->autosaveOnTitle, AnchorMiddle, font, AutosaveEnabledText, TextAlignment::Centred);
    Label::Construct(&oleg->autosaveOnText, AnchorMiddle, font, AutosaveWarningText, TextAlignment::Centred);
    SpriteWidget::Construct(&oleg->autosaveOnIcon, AnchorMiddle, &sprites[SpriteAutosave]);
    MenuWidget::Construct(&oleg->autosaveOnMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    Label::Construct(&oleg->autosaveFailedText, AnchorMiddle, font, AutosaveFailedText, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->autosaveFailedMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    Label::Construct(&oleg->fourthNoticeTitle, AnchorMiddle, font, AutosaveDisabledText, TextAlignment::Centred);
    Label::Construct(&oleg->fourthNoticeText, AnchorMiddle, font, EnableAutosaveText, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->fourthNoticeMenu, AnchorMiddle, input, &drawers[DrawerMenus], sounds);
    RingWidget::Construct(&oleg->wumpaRing, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    RingWidget::Construct(&oleg->livesRing, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    RingWidget::Construct(&oleg->crystalRing, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    RingWidget::Construct(&oleg->completionRing, AnchorMiddle, CompletionRings, RingSegments, RingSteps, nullptr);
    RingWidget::Construct(&oleg->gemArc, AnchorMiddle, GemArcRings, RingSegments, GemArcSteps, nullptr);
    SpriteWidget::Construct(&oleg->levelPicture, AnchorMiddle, &sprites[SpriteLevelTitle]);
    StringLabel::Construct(&oleg->completionText, AnchorMiddle, font, TextAlignment::Centred);
    StringLabel::Construct(&oleg->wumpaText, AnchorMiddle, font, TextAlignment::MiddleRight);
    SpriteWidget::Construct(&oleg->wumpaSprite, AnchorMiddle, &sprites[SpriteWumpa]);
    StringLabel::Construct(&oleg->livesText, AnchorMiddle, font, TextAlignment::MiddleRight);
    SpriteWidget::Construct(&oleg->livesSprite, AnchorMiddle, &sprites[SpriteHeads]);
    StringLabel::Construct(&oleg->crystalText, AnchorMiddle, font, TextAlignment::MiddleRight);
    SpriteWidget::Construct(&oleg->crystalSprite, AnchorMiddle, &sprites[SpriteCrystal]);
    for (MenuWidget& menu : oleg->levelsMenus)
    {
        MenuWidget::Construct(&menu, AnchorMiddle, input, &drawers[DrawerLevels], sounds);
    }

    Label::Construct(&oleg->levelName, AnchorMiddle, font, LevelNameTexts, TextAlignment::Centred);
    RingWidget::Construct(&oleg->levelNamePanel, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    MenuWidget::Construct(&oleg->extrasMenu, AnchorMiddle, input, &drawers[DrawerExtras], sounds);
    RingWidget::Construct(&oleg->extrasPanel, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    SpriteWidget::Construct(&oleg->extrasLogo, AnchorLeft, &sprites[SpriteCrashTitle]);
    Label::Construct(&oleg->nextHint, AnchorLeft, font, NextHintText, TextAlignment::BottomLeft);
    MenuWidget::Construct(&oleg->gameOverMenu, AnchorMiddle, input, &drawers[DrawerGameOver], sounds);
    StringLabel::Construct(&oleg->saveMessage, AnchorMiddle, font, TextAlignment::Centred);
    StringLabel::Construct(&oleg->saveChoicesMessage, AnchorMiddle, font, TextAlignment::Centred);
    MenuWidget::Construct(&oleg->saveChoicesMenu, AnchorMiddle, input, &drawers[DrawerSaveChoices], sounds);
    StringLabel::Construct(&oleg->saveSlotsTitle, AnchorMiddle, font, TextAlignment::Centred);
    RingWidget::Construct(&oleg->saveSlotsPanel, AnchorMiddle, PanelRings, RingSegments, RingSteps, nullptr);
    MenuWidget::Construct(&oleg->saveSlotsMenu, AnchorMiddle, input, &drawers[DrawerSaveSlots], sounds);
    Label::Construct(&oleg->autosavingText, AnchorMiddle, font, AutosavingText, TextAlignment::Centred);
    SpriteWidget::Construct(&oleg->autosavingIcon, AnchorMiddle, &sprites[SpriteAutosave]);
    StringLabel::Construct(&oleg->timedCountText, AnchorRight, font, TextAlignment::MiddleLeft);
    SpriteWidget::Construct(&oleg->timedCountIcon, AnchorLeft, &sprites[SpriteHudIcons + HudIconWhackaworm]);
    StringLabel::Construct(&oleg->timeText, AnchorRight, font, TextAlignment::MiddleLeft);
    SpriteWidget::Construct(&oleg->clockIcon, AnchorLeft, &sprites[SpriteClock]);
    GameProgress* state = oleg->progress;
    AnimatedWidget::Construct(&oleg->healthBar, AnchorLeft);
    oleg->healthBar.progress = state;
    oleg->healthBar.sprites = sprites;
    oleg->healthBar.vtable = g_HudBarWidgetVTable;
    AnimatedWidget::Construct(&oleg->slider, AnchorMiddle);
    oleg->slider.track = &sprites[SpriteEmptyGem];
    oleg->slider.value = 0.5f;
    oleg->slider.knob = &sprites[SpriteWumpa];
    oleg->slider.rightEnd = nullptr;
    oleg->slider.leftEnd = nullptr;
    oleg->slider.vtable = g_SliderWidgetVTable;
    StringLabel::Construct(&oleg->hudWumpaText, AnchorLeft, font, TextAlignment::BottomLeft);
    SpriteWidget::Construct(&oleg->hudWumpaSprite, AnchorLeft, &sprites[SpriteWumpa]);
    StringLabel::Construct(&oleg->hudLivesText, AnchorRight, font, TextAlignment::BottomRight);
    SpriteWidget::Construct(&oleg->hudLivesSprite, AnchorRight, &sprites[SpriteHeads]);
    SpriteWidget::Construct(&oleg->extraLifeHead, AnchorFifth, &sprites[SpriteHeads]);
    SpriteWidget::Construct(&oleg->pickupIcon, AnchorMiddle, &sprites[SpritePickupCrystal]);
    StringLabel::Construct(&oleg->ammoText, AnchorRight, font, TextAlignment::BottomRight);
    SpriteWidget::Construct(&oleg->ammoIcon, AnchorRight, &sprites[SpriteAmmo]);
    SpriteWidget::Construct(&oleg->blackout, &sprites[SpriteFlat]);
    TiledPicture::Construct(&oleg->screenPicture, AnchorMiddle, oleg->tiles);
    SpriteWidget::Construct(&oleg->loadingLogo, AnchorLeft, &sprites[SpriteCrashTitle]);
    Label::Construct(&oleg->loadingText, AnchorMiddle, font, LoadingText, TextAlignment::Centred);

    // The sprites drawn turned (placed by their middle): the heads, the levels' titles, the pictures' and a few icons
    SpritesTurned(sprites, SpriteHeads, SpriteEmptyGem);
    SpritesTurned(sprites, SpriteLevelTitles, SpriteCount);
    const u32 turned[] = {SpriteCrashTitle, SpriteLevelTitle,      SpriteWumpa,       SpritePickupCrystal,
                          SpriteCrystal,    SpriteAutosave,        SpriteLockedLevel};
    for (u32 sprite : turned)
    {
        sprites[sprite].turned = 1;
    }

    oleg->particleSprite.turned = 1;
    oleg->crystalSprite.emitter = &oleg->emitters[CrystalEmitter];
    oleg->pickupSprite = SpriteFlat;
    oleg->wumpaToAdd = 0;
    oleg->pickupGem.value = 0;
    oleg->wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * WumpaDelaySeconds);
    for (u32 picture = 0; picture < Pictures; picture++)
    {
        oleg->pictures[picture].value = 0;
        oleg->pictures[picture].read = NoPicture[picture];
        oleg->pictures[picture].wanted = NoPicture[picture];
    }
    oleg->pulse.duration = WumpaDelaySeconds;
    for (MaterialResource*& icon : oleg->hudIcons)
    {
        icon = nullptr;
    }

    for (u32 gem = 0; gem < GameProgress::Gems; gem++)
    {
        oleg->gemSprites[gem] = SpriteWidget::Construct(Allocate<SpriteWidget>(),
                                                        AnchorMiddle, &sprites[SpriteGems + gem]);
    }

    oleg->SetUpScreens();
    oleg->SetUpFlatSprites();
    Vector2 place;
    Vector2 scale;
    Vector2 offset;
    place = {Rounded(0.05), Rounded(0.05)};
    scale = {1.0f, 1.0f};
    offset = {-0.5f, g_MinusZero};
    oleg->healthBar.SlideIn(White(), &place, &scale, &offset);
    scale = {1.0f, 1.0f};
    oleg->screenPositionMenu.GrowIn(White(), &g_OlegMenuPlace, &scale, &g_OlegMenuPlace);
    scale = {0.625f, 0.625f};
    oleg->screenPositionHint.GrowIn(White(), &g_OlegTitlePlace, &scale, &g_OlegTitlePlace);
    oleg->screenPositionMenu.next = &oleg->screenPositionHint;
    oleg->SetUpTimeHud();
    oleg->SetUpOverlays();
    oleg->SetUpExtras();
    oleg->SetUpMainMenu();
    oleg->SetUpLoadingScreen();
    place = {0.5f, Rounded(0.1)};
    scale = {1.0f, 1.0f};
    oleg->gameOverMenu.FadeIn(White(), &place, &scale);
    oleg->SetUpMenuRings();
    oleg->SetUpLevelsMenus();
    oleg->SetUpOptionsMenu();
    oleg->SetUpPauseMenu();
    oleg->SetUpSaveScreens();
    place = {0.5f, Rounded(0.15)};
    scale = {0.5f, Rounded(0.1)};
    offset = {0.0f, -0.5f};
    oleg->slider.SlideIn(White(), &place, &scale, &offset);
    place = {0.5f, Rounded(0.4)};
    scale = {0.75f, 0.75f};
    oleg->autosavingText.FadeIn(White(), &place, &scale);
    place = {0.5f, Rounded(0.85)};
    scale = {Rounded(0.1), Rounded(0.1)};
    offset = {0.0f, 0.5f};
    oleg->autosavingIcon.SlideRectangle(White(), &place, &scale, &offset);
    oleg->autosavingIcon.next = &oleg->autosavingText;
    place = {Rounded(0.16), Rounded(0.19)};
    scale = {1.5f, 1.5f};
    offset = {-0.5f, 0.0f};
    oleg->hudWumpaText.SlideIn(White(), &place, &scale, &offset);
    place = {Rounded(0.1), 0x1.EB8520p-4f};
    scale = {Rounded(0.11), Rounded(0.15)};
    offset = {-0.5f, 0.0f};
    oleg->hudWumpaSprite.SlideRectangle(White(), &place, &scale, &offset);
    place = {Rounded(0.81), Rounded(0.19)};
    scale = {1.5f, 1.5f};
    offset = {0.5f, 0.0f};
    oleg->hudLivesText.SlideIn(White(), &place, &scale, &offset);
    place = {Rounded(0.88), 0x1.EB8520p-4f};
    scale = {Rounded(0.16), Rounded(0.15)};
    offset = {0.5f, 0.0f};
    oleg->hudLivesSprite.SlideRectangle(White(), &place, &scale, &offset);
    place = {0x1.999998p-1f, Rounded(0.95)};
    scale = {1.0f, 1.0f};
    oleg->ammoText.FadeIn(White(), &place, &scale);
    place = {0.875f, 0.875f};
    scale = {Rounded(0.15), Rounded(0.15)};
    oleg->ammoIcon.FadeRectangle(White(), &place, &scale);
    place = {0.5f, Rounded(0.2)};
    scale = {Rounded(0.2), Rounded(0.2)};
    offset = {0.5f, Rounded(0.2)};
    oleg->pickupIcon.GrowRectangle(White(), &place, &scale, &offset);
    place = {Rounded(0.34), Rounded(0.12)};
    scale = {Rounded(0.16), Rounded(0.15)};
    offset = {Rounded(0.34), Rounded(0.12)};
    oleg->extraLifeHead.GrowRectangle(White(), &place, &scale, &offset);
    oleg->hudWumpaText.next = &oleg->hudWumpaSprite;
    oleg->hudLivesText.next = &oleg->hudLivesSprite;
    oleg->ammoText.next = &oleg->ammoIcon;

    // The widgets of the slots
    Widget* const slots[] = {
        &oleg->letterboxTop,       &oleg->fader,               &oleg->bottomTextBackdrop, &oleg->bottomText,
        &oleg->dimmer,             &oleg->menuRings,           &oleg->menuPanel,          &oleg->backHint,
        &oleg->cancelHint,         &oleg->selectHint,          &oleg->pagesLeftHint,      &oleg->pagesRightHint,
        &oleg->optionsMenu,        &oleg->screenPositionMenu,  &oleg->mainMenu,           &oleg->titleLogo,
        &oleg->pauseMenu,          &oleg->disableAutosaveMenu, &oleg->quitMenu,           &oleg->completionRing,
        &oleg->noControllerText,   &oleg->discErrorTitle,      &oleg->autosaveOffTitle,   &oleg->autosaveOnTitle,
        &oleg->autosaveFailedText, &oleg->fourthNoticeTitle,   &oleg->levelsMenus[0],     &oleg->levelsMenus[1],
        &oleg->levelsMenus[2],     &oleg->levelsMenus[3],      &oleg->levelName,          &oleg->extrasMenu,
        &oleg->extrasLogo,         &oleg->gameOverMenu,        &oleg->saveMessage,        &oleg->saveChoicesMessage,
        &oleg->saveSlotsTitle,     &oleg->autosavingIcon,      &oleg->timedCountText,     &oleg->healthBar,
        &oleg->slider,             &oleg->hudWumpaText,        &oleg->hudLivesText,       &oleg->pickupIcon,
        &oleg->ammoText,           &oleg->blackout,            &oleg->screenPicture,      &oleg->loadingLogo,
    };
    // The slots in the order the game sets them: the dimmer to the title's logo but the screen position menu's, the cutscene's to
    // the bottom text's, the pause menu's to the fourth notice's, the screen position menu's, then the rest
    const u32 order[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 0, 1, 2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 13, 26, 27,
                         28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    for (u32 index : order)
    {
        oleg->SetWidget(index, slots[index]);
    }

    return oleg;
}

void OLEG::Destroy(u32 destroyFlags)
{
    vtable = g_OlegVTable;
    // The pages it deletes (the screen position's and the notices' are left)
    const u32 deleted[] = {PageExtras,     PageMainMenu,   PagePause,      PageDisableAutosave, PageQuit,
                           PageGameOver,   PageLevels,     PageLevels + 1, PageLevels + 2,      PageLevels + 3,
                           PageOptions,    PageSaveChoices, PageSaveSlots};
    for (u32 index : deleted)
    {
        MenuPage* page = pages[index];
        if (page != nullptr)
        {
            CallVirtual<void>(page, page->vtable, MenuPage::DestroySlot, u32{DestroyAndFree});
        }
    }

    for (u32 picture = 0; picture < Pictures; picture++)
    {
        ReleasePicture(picture);
    }

    for (SpriteWidget* gem : gemSprites)
    {
        if (gem != nullptr)
        {
            CallVirtual<void>(gem, gem->vtable, Widget::DestroySlot, u32{DestroyAndFree});
        }
    }

    // Its members the other way round, their destructors inline (a widget's the base one's)
    loadingText.Widget::Destroy(DestroyOnly);
    loadingLogo.Widget::Destroy(DestroyOnly);
    screenPicture.Widget::Destroy(DestroyOnly);
    blackout.Widget::Destroy(DestroyOnly);
    ammoIcon.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&ammoText);
    pickupIcon.Widget::Destroy(DestroyOnly);
    extraLifeHead.Widget::Destroy(DestroyOnly);
    hudLivesSprite.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&hudLivesText);
    hudWumpaSprite.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&hudWumpaText);
    slider.Widget::Destroy(DestroyOnly);
    healthBar.Widget::Destroy(DestroyOnly);
    clockIcon.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&timeText);
    timedCountIcon.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&timedCountText);
    autosavingIcon.Widget::Destroy(DestroyOnly);
    autosavingText.Widget::Destroy(DestroyOnly);
    saveSlotsMenu.Widget::Destroy(DestroyOnly);
    saveSlotsPanel.Destroy(DestroyOnly);
    DestroyStringLabel(&saveSlotsTitle);
    saveChoicesMenu.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&saveChoicesMessage);
    DestroyStringLabel(&saveMessage);
    gameOverMenu.Widget::Destroy(DestroyOnly);
    nextHint.Widget::Destroy(DestroyOnly);
    extrasLogo.Widget::Destroy(DestroyOnly);
    extrasPanel.Destroy(DestroyOnly);
    extrasMenu.Widget::Destroy(DestroyOnly);
    levelNamePanel.Destroy(DestroyOnly);
    levelName.Widget::Destroy(DestroyOnly);
    for (u32 world = 4; world > 0; world--)
    {
        levelsMenus[world - 1].Widget::Destroy(DestroyOnly);
    }

    crystalSprite.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&crystalText);
    livesSprite.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&livesText);
    wumpaSprite.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&wumpaText);
    DestroyStringLabel(&completionText);
    levelPicture.Widget::Destroy(DestroyOnly);
    gemArc.Destroy(DestroyOnly);
    completionRing.Destroy(DestroyOnly);
    crystalRing.Destroy(DestroyOnly);
    livesRing.Destroy(DestroyOnly);
    wumpaRing.Destroy(DestroyOnly);
    fourthNoticeMenu.Widget::Destroy(DestroyOnly);
    fourthNoticeText.Widget::Destroy(DestroyOnly);
    fourthNoticeTitle.Widget::Destroy(DestroyOnly);
    autosaveFailedMenu.Widget::Destroy(DestroyOnly);
    autosaveFailedText.Widget::Destroy(DestroyOnly);
    autosaveOnMenu.Widget::Destroy(DestroyOnly);
    autosaveOnIcon.Widget::Destroy(DestroyOnly);
    autosaveOnText.Widget::Destroy(DestroyOnly);
    autosaveOnTitle.Widget::Destroy(DestroyOnly);
    autosaveOffMenu.Widget::Destroy(DestroyOnly);
    autosaveOffText.Widget::Destroy(DestroyOnly);
    autosaveOffTitle.Widget::Destroy(DestroyOnly);
    discErrorText.Widget::Destroy(DestroyOnly);
    discErrorTitle.Widget::Destroy(DestroyOnly);
    noControllerText.Widget::Destroy(DestroyOnly);
    quitTitle.Widget::Destroy(DestroyOnly);
    quitMenu.Widget::Destroy(DestroyOnly);
    disableAutosaveTitle.Widget::Destroy(DestroyOnly);
    disableAutosaveMenu.Widget::Destroy(DestroyOnly);
    pauseMenu.Widget::Destroy(DestroyOnly);
    titleLogo.Widget::Destroy(DestroyOnly);
    mainMenu.Widget::Destroy(DestroyOnly);
    screenPositionHint.Widget::Destroy(DestroyOnly);
    screenPositionMenu.Widget::Destroy(DestroyOnly);
    optionsPanel.Destroy(DestroyOnly);
    optionsMenu.Widget::Destroy(DestroyOnly);
    pagesRightHint.Widget::Destroy(DestroyOnly);
    pagesLeftHint.Widget::Destroy(DestroyOnly);
    selectHint.Widget::Destroy(DestroyOnly);
    cancelHint.Widget::Destroy(DestroyOnly);
    backHint.Widget::Destroy(DestroyOnly);
    menuPanel.Destroy(DestroyOnly);
    menuRings.Destroy(DestroyOnly);
    dimmer.Widget::Destroy(DestroyOnly);
    DestroyStringLabel(&bottomText);
    bottomTextBackdrop.Widget::Destroy(DestroyOnly);
    fader.Widget::Destroy(DestroyOnly);
    letterboxBottom.Widget::Destroy(DestroyOnly);
    letterboxTop.Widget::Destroy(DestroyOnly);
    StringDestroy(&textLine.text);
    for (u32 tile = 8; tile > 0; tile--)
    {
        Sprite& sprite = tiles[tile - 1];
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::DestroySlot, u32{DestroyElement});
    }

    for (u32 index = SpriteCount; index > 0; index--)
    {
        Sprite& sprite = sprites[index - 1];
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::DestroySlot, u32{DestroyElement});
    }

    sparkle.emitter.Emitter2D::Destroy(DestroyOnly);
    sparkle.vtable = g_WidgetEffectVTable;
    for (u32 index = 8; index > 0; index--)
    {
        Emitter2D& emitter = emitters[index - 1];
        CallVirtual<void>(&emitter, emitter.vtable, Emitter2D::DestroySlot, u32{DestroyElement});
    }

    particleSprite.Shape2D::Destroy(DestroyOnly);
    Platform::Graphics::DestroyMaterial(OwnMaterial(this));
    input.Destroy(DestroyOnly);
    bindings.Destroy(DestroyOnly);
    StringDestroy(&pictureName);
    rock.vtable = g_WidgetEffectVTable;
    spin.vtable = g_WidgetEffectVTable;
    slide.vtable = g_WidgetEffectVTable;
    pulse.vtable = g_WidgetEffectVTable;
    WidgetController::Destroy(destroyFlags);
}

void OLEG::StartUp(PadButtons* pad, Font* font, Font* menuFont, SoundTable* frontEnd)
{
    auto* saves = static_cast<SaveManager*>(g_SaveManager);
    pages[PageScreenPosition] = ScreenPositionPage::Construct(Allocate<ScreenPositionPage>(), nullptr);
    pages[PageExtras] = ExtrasPage::Construct(Allocate<ExtrasPage>());
    pages[PageMainMenu] = MainMenuPage::Construct(Allocate<MainMenuPage>());
    pages[PageGameOver] = GameOverPage::Construct(Allocate<GameOverPage>());
    pages[PagePause] = PausePage::Construct(Allocate<PausePage>(), pad, font);
    pages[PageDisableAutosave] = DisableAutosavePage::Construct(Allocate<DisableAutosavePage>(), nullptr);
    pages[PageQuit] = QuitPage::Construct(Allocate<QuitPage>(), nullptr);
    // The notices' pages, the second one made first
    pages[PageNotices + 1] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[PageNotices] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[PageNotices + 2] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[PageNotices + 3] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    for (u32 world = 0; world < 4; world++)
    {
        pages[PageLevels + world] = LevelsPage::Construct(Allocate<LevelsPage>(), world, &levelsMenus[world]);
    }

    pages[PageOptions] = OptionsPage::Construct(Allocate<OptionsPage>(), nullptr);
    pages[PageSaveChoices] = SaveChoicesPageConstruct(MemoryAllocate(sizeof(SaveChoicesPage)), saves);
    pages[PageSaveSlots] = SaveSlotsPageConstruct(MemoryAllocate(sizeof(SaveSlotsPage)), saves, &saveSlotsMenu);

    // Each page's menu, and whether the leave action leaves it
    MenuWidget* const menus[Pages] = {
        &screenPositionMenu, &extrasMenu, &mainMenu, &gameOverMenu, &pauseMenu,
        &disableAutosaveMenu, &quitMenu, &autosaveOffMenu, &autosaveOnMenu, &autosaveFailedMenu,
        &fourthNoticeMenu, &levelsMenus[0], &levelsMenus[1], &levelsMenus[2], &levelsMenus[3],
        &optionsMenu, &saveChoicesMenu, &saveSlotsMenu,
    };
    const bool leaves[Pages] = {true, true, true, false, true, true, true, true, true,
                                true, true, true, true, true, true, true, false, false};
    for (u32 page = 0; page < Pages; page++)
    {
        MenuWidget* menu = menus[page];
        menu->home = pages[page];
        menu->firstPage = pages[page];
        menu->menuFlags.leaves = leaves[page];
        menu->pad = pad;
    }

    extrasMenu.menuFlags.remembersPage = 1;
    saves->messages[0] = &saveMessage;
    saves->messages[1] = &saveChoicesMessage;
    saves->messages[2] = &saveSlotsTitle;
    saves->choicesPage = pages[PageSaveChoices];
    saves->slotsPage = pages[PageSaveSlots];
    // The save code's screens: every widget hidden, its message, its choices and the save slots
    const s32 saveScreens[4] = {ScreenAllButOverlays, ScreenSaveMessage, ScreenSaveChoices, ScreenSaveSlots};
    for (u32 screen = 0; screen < 4; screen++)
    {
        saves->screens[screen] = saveScreens[screen];
    }

    bindings.AddButton(MenuInput::ActionSelect, PadCross);
    bindings.AddButton(MenuInput::ActionBack, PadTriangle);
    bindings.AddButton(MenuInput::ActionLeave, PadStart);
    bindings.AddButton(MenuInput::ActionUp, PadUp);
    bindings.AddButton(MenuInput::ActionUp, PadLeftStickUp);
    bindings.AddButton(MenuInput::ActionDown, PadDown);
    bindings.AddButton(MenuInput::ActionDown, PadLeftStickDown);
    bindings.AddButton(MenuInput::ActionLeft, PadLeft);
    bindings.AddButton(MenuInput::ActionLeft, PadLeftStickLeft);
    bindings.AddButton(MenuInput::ActionRight, PadRight);
    bindings.AddButton(MenuInput::ActionRight, PadLeftStickRight);

    // The drawers: lists (the screen's position, the main menu, the confirmations and notices, the pause menu, the save code's),
    // the extras' (a window of 9 to 17 items), the game over's (its title big), the levels' (big items, no pulse), the options'
    const u32 lists[] = {DrawerScreenPosition, DrawerMenus, DrawerPause, DrawerSaveChoices, DrawerSaveSlots};
    for (u32 index : lists)
    {
        ListStyle(&drawers[index], menuFont);
        if (index != DrawerScreenPosition)
        {
            drawers[index].windowSize = ListWindow;
        }
    }

    MenuDrawer& extras = drawers[DrawerExtras];
    extras.titlePlace = {0.0f, 0.0f};
    extras.titleScale = {0.75f, 0.75f};
    extras.titleAlignment = TextAlignment::Centred;
    extras.itemAlignment = TextAlignment::Centred;
    extras.windowStart = 9;
    extras.windowSize = 17;
    extras.itemsPlace = {0.0f, Rounded(0.364)};
    extras.fewItemsScale = {0.75f, 0.75f};
    extras.fewItemsSpacing = ListSpacing;
    extras.manyItemsScale = {0.5f, 0.5f};
    extras.manyItemsSpacing = Rounded(0.035);
    SetItemStyles(&extras, menuFont);
    extras.sizePulse = &g_BreathingScale;

    MenuDrawer& gameOver = drawers[DrawerGameOver];
    gameOver.titlePlace = {0.0f, 0.0f};
    gameOver.titleScale = {1.5f, 1.5f};
    gameOver.titleAlignment = TextAlignment::TopCentre;
    gameOver.itemAlignment = TextAlignment::TopCentre;
    gameOver.itemsPlace = {0.0f, Rounded(0.15)};
    gameOver.manyItemsScale = {0.75f, 0.75f};
    gameOver.manyItemsSpacing = ListSpacing;
    SetItemStyles(&gameOver, menuFont);
    gameOver.sizePulse = &g_BreathingScale;

    MenuDrawer& options = drawers[DrawerOptions];
    options.titlePlace = {0.0f, 0.0f};
    options.titleScale = {0.75f, 0.75f};
    options.titleAlignment = TextAlignment::Centred;
    options.itemAlignment = TextAlignment::Centred;
    options.itemsPlace = {0.0f, Rounded(0.314)};
    options.manyItemsScale = {0.625f, 0.75f};
    options.manyItemsSpacing = ListSpacing;
    SetItemStyles(&options, menuFont);
    options.sizePulse = &g_BreathingScale;

    MenuDrawer& levels = drawers[DrawerLevels];
    levels.titlePlace = {0.0f, 0.0f};
    levels.titleScale = {0.0f, 0.0f};
    levels.titleAlignment = TextAlignment::TopCentre;
    levels.itemAlignment = TextAlignment::TopCentre;
    levels.itemsPlace = {0.0f, 0.0f};
    levels.manyItemsScale = {1.0f, 1.0f};
    levels.manyItemsSpacing = Rounded(0.15);
    SetItemStyles(&levels, menuFont);

    // The front end's sounds: the first when an item is picked that leads on, the second when the selection or a value moves or
    // an item is picked, the third going back
    GameSound** frontEndSounds = frontEnd->sounds;
    for (u32 sound : MoveSounds)
    {
        sounds.sounds[sound] = frontEndSounds[FrontEndMove];
    }

    sounds.sounds[MenuSounds::SoundSelectedOn] = frontEndSounds[FrontEndPicked];
    sounds.sounds[MenuSounds::SoundBack] = frontEndSounds[FrontEndBack];
    sounds.sounds[MenuSounds::SoundUnused] = frontEndSounds[FrontEndBack];
}

void OLEG::LoadPicture(u32 picture, u32 now)
{
    u32 read = pictures[picture].read;
    u32 wanted = pictures[picture].wanted;
    if (read == wanted)
    {
        return;
    }

    GameReadersStorage* storage = g_ReadersStorages[FileReaders];
    String path;
    path.string = nullptr;
    path.length = 0;
    path.capacity = 0;
    const char separator[2] = {'\\', '\0'};
    if (picture == PictureCrashTitle)
    {
        StringAssign(&path, g_TitlesFolder);
        StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        StringAppend(&path, g_CrashTitle);
    }
    else if (picture == PictureLevelTitle)
    {
        StringAssign(&path, g_TitlesFolder);
        StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        StringAppend(&path, separator);
        StringAppend(&path, g_LevelTitles[wanted]);
    }
    else if (picture == PictureTiles)
    {
        if (wanted == TilesLegal)
        {
            StringAssign(&path, g_LanguageFolder);
            StringAppend(&path, g_ScreenPictures[TilesLegal]);
            StringAppend(&path, separator);
            StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        }
        else if (wanted >= FirstNamedPicture && wanted < EndNamedPictures)
        {
            StringAssign(&path, pictureName.string);
        }
        else
        {
            StringAssign(&path, g_LanguageFolder);
            StringAppend(&path, g_ScreenPictures[wanted]);
        }
    }

    StringAppend(&path, g_PictureExtension);
    ReleasePicture(picture);
    OlegPictureState& state = pictures[picture];
    state.read = state.wanted;
    state.reading = 1;
    auto* reader = Allocate<PictureReader>();
    reader->vtable = g_PictureReaderVTable;
    reader->picture = picture;
    reader->oleg = this;
    // The file read whole into the disk manager's memory, which goes once it's read
    SubItemsReader* file = SubItemsReader::ConstructFile(
        Allocate<SubItemsReader>(), path.string, reader,
        SubItemsReaderOptions::Unused0 | SubItemsReaderOptions::ClosesFile | SubItemsReaderOptions::OnDisk);
    AddItemReaderToReaderStorage(storage, file, QueueBack);
    if (now != 0)
    {
        LoadQueuedSectionsIntoMemory_();
    }

    StringDestroy(&path);
}

void OLEG::ReleasePicture(u32 picture)
{
    if (picture >= Pictures || pictures[picture].read == NoPicture[picture])
    {
        return;
    }

    if (picture == PictureTiles)
    {
        for (Sprite& tile : tiles)
        {
            CallVirtual<void>(&tile, tile.vtable, Shape2D::ReleaseResourceSlot);
        }
    }
    else
    {
        Sprite& sprite = sprites[SpriteCrashTitle + picture];
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::ReleaseResourceSlot);
    }

    pictures[picture].read = NoPicture[picture];
}

void OLEG::Reset()
{
    StringAssign(&textLine.text, "");
    wumpaToAdd = 0;
    wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * WumpaDelaySeconds);
    for (u32 sprite = SpriteHudIcons; sprite < SpriteIcons; sprite++)
    {
        CallVirtual<void>(&sprites[sprite], sprites[sprite].vtable, Shape2D::SetMaterialSlot, Platform::Graphics::FlatMaterial());
    }
}

void OLEG::Draw(Renderer* renderer)
{
    WidgetController::Draw(renderer);
    TextLineDraw(&textLine, renderer);
}

void OLEG::PlayPickupEffect(u32 screen)
{
    // The gems' array constructed, then the colours set the first time: the gems' (30, 33, 255), (255, 255, 255), (12, 107, 55),
    // (176, 0, 176), (215, 21, 27), (200, 200, 0) and the crystal's (234, 0, 0), as fractions of 255 a little above them
    if (g_GemColoursGuard == 0)
    {
        g_GemColoursGuard = 1;
    }

    if (g_PickupColoursSet == 0)
    {
        g_GemColours[0] = {0x1.E1E1E4p-4f, 0x1.09090Ap-3f, 1.0f, 1.0f};
        g_GemColours[1] = {1.0f, 1.0f, 1.0f, 1.0f};
        g_GemColours[2] = {0x1.818184p-5f, 0x1.ADADB0p-2f, 0x1.B9B9BCp-3f, 1.0f};
        g_GemColours[3] = {0x1.616162p-1f, 0.0f, 0x1.616162p-1f, 1.0f};
        g_GemColours[4] = {0x1.AFAFB2p-1f, 0x1.515152p-4f, 0x1.B1B1B4p-4f, 1.0f};
        g_GemColours[5] = {0x1.919194p-1f, 0x1.919194p-1f, 0.0f, 1.0f};
        g_CrystalColour = {0x1.D5D5D8p-1f, 0.0f, 0.0f, 1.0f};
        g_PickupColoursSet = 1;
    }

    if (screen == ScreenLives)
    {
        rock.WidgetEffect::Reset();
        rock.duration = RockSeconds;
        rock.repeats = RockRepeats;
        SetEffect(screens[ScreenLives], &rock);
    }
    else if (screen == ScreenPickup)
    {
        sparkle.SparkleEffect::Reset();
        sparkle.repeats = 1;
        sparkle.emitter.colour = pickupSprite == SpritePickupCrystal ? g_CrystalColour : g_GemColours[pickupGem.gem];
        SetEffect(screens[screen], &sparkle);
    }
}

void OLEG::UpdateHud(PlayerCharacter* character)
{
    Gun* gun = character != nullptr ? character->gun : nullptr;
    Vehicle* vehicle = character != nullptr ? character->vehicle : nullptr;
    bool riding = false;
    GameProgress* game = progress;
    u32 mode = game->play.mode;
    u32 lives = game->counts.lives;
    bool health = mode == PlayHealth;
    bool timed = mode == PlayTimed;
    if (vehicle != nullptr)
    {
        riding = vehicle->Kind() == Vehicle::KindWrestle;
    }

    String livesText = {};
    String wumpaText = {};
    String crystalsText = {};
    AppendNumber(&wumpaText, game->counts.wumpa);
    u32 crystals = 0;
    for (u32 level = 0; level < GameProgress::Levels; level++)
    {
        crystals += CrystalFound(&game->levels[level]);
    }

    AppendNumber(&crystalsText, crystals);

    // The level the levels menu shown has selected: its name, or unknown past the last area open
    MenuWidget* levels = nullptr;
    for (MenuWidget& menu : levelsMenus)
    {
        if (menu.State() >= Widget::StateAppearing)
        {
            levels = &menu;
            break;
        }
    }

    if (levels != nullptr)
    {
        MenuPage* page = levels->firstPage;
        u32 selected = page->ItemAgain(page->selections[0])->Id();
        u32 open = progress->play.open;
        levelName.text = open < selected ? UnknownLevelText : static_cast<u32>(g_AreaLevels[selected]) + LevelNameTexts;
    }

    if (timed)
    {
        s32 seconds = static_cast<s32>(static_cast<f32>(progress->timeLeft) * g_SecondsPerClockUnit);
        s32 minutes = seconds / 60;
        seconds -= minutes * 60;
        String counted = {};
        String time = {};
        String number;
        StringConstructNumber(&number, progress->counts.count);
        StringAssign(&counted, number.string);
        StringDestroy(&number);
        StringAppend(&counted, "/");
        String total;
        StringConstructNumber(&total, progress->counts.countTotal);
        StringAppend(&counted, total.string);
        StringDestroy(&total);
        StringConstructInteger(&number, minutes);
        StringAssign(&time, number.string);
        StringDestroy(&number);
        StringAppend(&time, seconds < 10 ? ":0" : ":");
        StringConstructInteger(&number, seconds);
        StringAppend(&time, number.string);
        StringDestroy(&number);
        StringAssign(&timedCountText.text, counted.string);
        StringAssign(&timeText.text, time.string);
        StringDestroy(&time);
        StringDestroy(&counted);
    }

    SetVisible(&timedCountText, timed);
    SetVisible(&timedCountIcon, timed && hudIcons[HudIconWhackaworm] != nullptr);
    SetVisible(&timeText, timed);
    SetVisible(&clockIcon, timed);
    if (!health)
    {
        StringAssign(&hudWumpaText.text, wumpaText.string);
    }

    SetVisible(&healthBar, health);
    SetVisible(&hudWumpaText, !health);
    if (riding)
    {
        f32 gauge = VehicleGauge(vehicle);
        slider.leftEnd = &sprites[SpriteHudIcons + HudIconGauge];
        slider.rightEnd = &sprites[SpriteHeads + u32{CharacterCrash}];
        slider.value = gauge - 0.5f;
    }

    SetVisible(&slider, riding);
    StringAssign(&this->wumpaText.text, wumpaText.string);
    StringAssign(&crystalText.text, crystalsText.string);
    pickupIcon.sprite = &sprites[pickupSprite];
    AppendNumber(&livesText, lives < 2 ? 0 : lives - 1);
    if (hudLivesSprite.State() >= Widget::StateAppearing)
    {
        StringAssign(&hudLivesText.text, livesText.string);
        hudLivesSprite.sprite = &sprites[SpriteHeads + progress->play.character];
    }

    if (livesSprite.State() >= Widget::StateAppearing)
    {
        StringAssign(&this->livesText.text, livesText.string);
        livesSprite.sprite = &sprites[SpriteHeads + progress->play.character];
        livesSprite.emitter =
            progress->play.character == CharacterCrash ? &emitters[CrashLivesEmitter] : nullptr;
    }

    if (gun != nullptr)
    {
        String count = {};
        AppendNumber(&count, gun->bits.ammo);
        StringAssign(&ammoText.text, count.string);
        StringDestroy(&count);
    }

    bool counted = gun != nullptr && !health;
    SetVisible(&ammoIcon, counted);
    SetVisible(&ammoText, counted);
    if (autosavingIcon.State() >= Widget::StateAppearing)
    {
        if (savingShown < 2)
        {
            SetVisible(&autosavingText, true);
            autosavingIcon.scaler = &g_BreathingScale;
        }
        else
        {
            SetVisible(&autosavingText, false);
            autosavingIcon.scaler = nullptr;
        }
    }

    // How much of the game is done: the story's share where it has got to, and a third of the gems found
    GameProgress* done = progress;
    u32 area = done->play.area;
    const LevelProgress* levelGems = &done->levels[g_AreaLevels[area]];
    String completion = {};
    u32 gems = 0;
    for (u32 gem = 0; gem < GameProgress::Gems; gem++)
    {
        u32 found = 0;
        for (u32 other = 0; other < GameProgress::Levels; other++)
        {
            if ((done->levels[other].gems & 1u << gem) != 0)
            {
                found++;
            }
        }

        gems += found;
    }

    String number;
    StringConstructNumber(&number, g_StoryDone[done->play.story] + gems / 3);
    StringAssign(&completion, number.string);
    StringDestroy(&number);
    StringAppend(&completion, "%");
    StringAssign(&completionText.text, completion.string);

    // The level's title once it's read (a hub's square), else it's asked for
    OlegPictureState title = pictures[PictureLevelTitle];
    if (title.reading == 0 && title.read == area)
    {
        bool hub = area == HubAreas[0] || area == HubAreas[1] || area == HubAreas[2] || area == HubAreas[3];
        Vector2 scale = {hub ? Rounded(0.4) : Rounded(0.6), Rounded(0.4)};
        Vector2 place = {Rounded(0.45), Rounded(0.364)};
        Vector2 from = {0.5f, 0.5f};
        levelPicture.GrowRectangle(White(), &place, &scale, &from);
        SetShadow(&levelPicture);
        SetVisible(&levelPicture, true);
        levelPicture.pulseAmount = Rounded(0.03);
        levelPicture.pulsePeriod = Rounded(0.6);
    }
    else
    {
        if (pictures[PictureLevelTitle].reading == 0)
        {
            pictures[PictureLevelTitle].wanted = area;
            LoadPicture(PictureLevelTitle, 0);
        }

        SetVisible(&levelPicture, false);
    }

    for (u32 gem = 0; gem < GameProgress::Gems; gem++)
    {
        bool found = (levelGems->gems & 1u << gem) != 0;
        gemSprites[gem]->sprite = found ? &sprites[SpriteGems + gem] : &sprites[SpriteEmptyGem];
        gemSprites[gem]->emitter = found ? &emitters[FirstGemEmitter + gem] : nullptr;
    }

    StringDestroy(&completion);
    StringDestroy(&crystalsText);
    StringDestroy(&wumpaText);
    StringDestroy(&livesText);
}

void OLEG::Update(TimeClock* clock, GamePad* pad, PlayerCharacter* character)
{
    g_BreathingScale.Step(clock);
    if (g_GameState == GameStatePaused)
    {
        s32 duration = static_cast<s32>(g_ClockUnitsPerSecond * CountShowSeconds);
        wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * WumpaDelaySeconds);
        Hide(screens[ScreenWumpa], duration, 0);
        extraLifeHead.Disappear(static_cast<s32>(g_ClockUnitsPerSecond * CountShowSeconds), 0);
    }
    else if (wumpaToAdd != 0)
    {
        if (static_cast<s32>(clock->advance) < wumpaDelay)
        {
            wumpaDelay -= clock->advance;
        }
        else
        {
            u32 wumpa = progress->counts.wumpa + 1;
            if (wumpa >= WumpaPerLife)
            {
                wumpa -= WumpaPerLife;
                AddLives(1, 1);
            }

            progress->counts.wumpa = wumpa;
            // The more there are to add, the sooner the next: 0.15 seconds times sin(1 / how many)
            f32 delay = WumpaDelaySeconds;
            if (wumpaToAdd >= 2)
            {
                f32 values[4];
                Platform::Math::SinCos(1.0f / static_cast<f32>(static_cast<s32>(wumpaToAdd)), 0.0f, values);
                delay = WumpaDelaySeconds * values[0];
            }

            wumpaToAdd--;
            pulse.duration = delay;
            wumpaDelay = static_cast<s32>(delay * g_ClockUnitsPerSecond);
            pulse.repeats++;
            Show(screens[ScreenWumpa], static_cast<s32>(g_ClockUnitsPerSecond * CountShowSeconds),
                 static_cast<s32>(g_ClockUnitsPerSecond * CountHoldSeconds));
            SetEffect(screens[ScreenWumpa], &pulse);
            if (wumpaToAdd == 0 && pulse.repeats >= 2)
            {
                pulse.repeats = 1;
            }
        }
    }

    UpdateHud(character);
    WidgetController::Update(clock);
    textLine.label = &bottomText;
    TextLineUpdate(&textLine, clock);
}

void OLEG::AddLives(s32 lives, u32 celebrated)
{
    ProgressCounts counts = progress->counts;
    s32 total = static_cast<s32>(counts.lives) + lives;
    counts.lives = 0;
    if (total >= static_cast<s32>(MostLives))
    {
        counts.lives = MostLives;
    }
    else if (total >= 0)
    {
        counts.lives = static_cast<u32>(total);
    }

    progress->counts = counts;
    Show(screens[ScreenLives], static_cast<s32>(g_ClockUnitsPerSecond),
         static_cast<s32>(g_ClockUnitsPerSecond * CountHoldSeconds));
    rock.WidgetEffect::Reset();
    rock.duration = RockSeconds;
    rock.repeats = RockRepeats;
    hudLivesSprite.SetEffect(&rock);
    if (celebrated == 0)
    {
        return;
    }

    extraLifeHead.sprite = &sprites[SpriteHeads + progress->play.character];
    extraLifeHead.Appear(static_cast<s32>(g_ClockUnitsPerSecond * HeadAppearSeconds), static_cast<s32>(g_ClockUnitsPerSecond));
    slide.WidgetEffect::Reset();
    slide.duration = HeadMoveSeconds;
    slide.repeats++;
    spin.WidgetEffect::Reset();
    spin.duration = HeadMoveSeconds;
    spin.repeats++;
    slide.next = &spin;
    extraLifeHead.SetEffect(&slide);
    PlaySoundById(ExtraLifeVolume, OwnScale, ExtraLifeSound, 0, 0, -1);
}

u32 CyclingScale::Step(TimeClock* clock)
{
    if (!flags.running)
    {
        return 0;
    }

    if (time == 0)
    {
        scale = values[0];
        time = clock->time;
        return 1;
    }

    s32 elapsed = static_cast<s32>(clock->time - static_cast<u32>(time));
    while (elapsed >= period)
    {
        if (limited != 0)
        {
            cyclesLeft--;
            if (cyclesLeft == 0)
            {
                CyclingScaleFlags last = flags;
                scale = values[last.count - 1];
                if (last.running)
                {
                    flags.running = 0;
                }

                return 0;
            }
        }

        elapsed -= period;
        time += period;
    }

    f32 fraction = (static_cast<f32>(elapsed) * g_SecondsPerClockUnit) / (static_cast<f32>(period) * g_SecondsPerClockUnit);
    if (fraction < 1.0f)
    {
        f32 at = static_cast<f32>(static_cast<s32>(flags.count - 1)) * fraction;
        s32 index = static_cast<s32>(at);
        f32 first = values[index];
        scale = first + (values[index + 1] - first) * (at - static_cast<f32>(index));
    }
    else
    {
        scale = values[flags.count - 1];
    }

    return 1;
}

void OLEG::ReadPicture(u32 picture, Stream* stream)
{
    pictures[picture].reading = 0;
    if (picture == PictureTiles)
    {
        for (Sprite& tile : tiles)
        {
            CallVirtual<void>(&tile, tile.vtable, Shape2D::ReadSlot, stream);
        }

        return;
    }

    if (picture < PictureTiles)
    {
        Sprite& sprite = sprites[SpriteCrashTitle + picture];
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::ReadSlot, stream);
    }
}

void PictureReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PictureReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
    oleg->ReadPicture(picture, &stream);
    stream.Destroy(DestroyOnly);
}

void PictureReader::Missing(u8*, u32, ReaderStack*)
{
}

void HudBarWidget::Draw(Renderer* renderer)
{
    u32 health = progress->counts.health;
    u32 most = progress->counts.mostHealth;
    if (flags.invisible == 0 && State() >= StateAppearing && health != 0)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        f32 length = progress->barLength * BarLengthScale;
        u32 layer = flags.layer;
        FitPlace(0, &at);
        FitSize(0, &size);
        renderer->colour = drawColour;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        f32 width = length * size.x;
        f32 height = size.y * BarHeight;
        f32 pieceWidth = size.x * BarPieceWidth;
        f32 pieceHeight = size.y * BarPieceHeight;
        matrix.m[3][0] = at.x;
        matrix.m[1][1] = height;
        matrix.m[3][1] = at.y;
        matrix.m[0][0] = width;
        QueuePlacedShape(renderer, &matrix, &sprites[BarBody], layer);
        matrix.m[0][0] = pieceWidth;
        matrix.m[1][1] = pieceHeight;
        matrix.m[3][0] = at.x + width;
        matrix.m[3][1] = at.y + pieceHeight;
        QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarStart], layer);
        matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
        QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarGap], layer);
        for (u32 piece = 2;; piece++)
        {
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            if (health < piece || piece >= most)
            {
                break;
            }

            QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarPiece], layer);
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarGap], layer);
        }

        if (health == most && most >= 2)
        {
            QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarPiece], layer);
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            QueuePlacedShape(renderer, &matrix, &sprites[OLEG::SpriteBarEnd], layer);
        }
    }

    Widget::Draw(renderer);
}

void SliderWidget::Draw(Renderer* renderer)
{
    if (flags.invisible == 0 && State() >= StateAppearing)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        u32 layer = flags.layer;
        FitPlace(0, &at);
        FitSize(0, &size);
        renderer->colour = drawColour;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        f32 trackHeight = size.y * SliderTrackHeight;
        matrix.m[0][0] = size.x;
        matrix.m[1][1] = trackHeight;
        matrix.m[3][0] = at.x - size.x * 0.5f;
        matrix.m[3][1] = at.y - trackHeight * 0.5f;
        QueuePlacedShape(renderer, &matrix, track, layer);
        matrix.m[0][0] = size.y;
        matrix.m[1][1] = size.y;
        matrix.m[3][0] = at.x + size.x * value;
        matrix.m[3][1] = at.y - size.y * 0.5f;
        QueuePlacedShape(renderer, &matrix, knob, layer);
        matrix.m[3][0] = at.x - (size.x + size.y) * 0.5f;
        matrix.m[3][1] = at.y - size.y * 0.5f;
        QueuePlacedShape(renderer, &matrix, leftEnd, layer);
        matrix.m[3][0] = at.x + (size.x - size.y) * 0.5f;
        matrix.m[3][1] = at.y - size.y * 0.5f;
        QueuePlacedShape(renderer, &matrix, rightEnd, layer);
    }

    Widget::Draw(renderer);
}

LevelWidget* LevelWidget::Construct(LevelWidget* widget, f32 anchor, MenuPage* page, MenuItem* item, GameController* controller)
{
    ItemWidget::Construct(widget, anchor, page, item);
    widget->controller = controller;
    widget->vtable = g_LevelWidgetVTable;
    return widget;
}

void LevelWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void LevelWidget::BeginFrame()
{
    bob = 0.0f;
    if (IsSelected(0) != 0)
    {
        OLEG* oleg = &g_OlegGameController->oleg;
        if (g_SparklingLevel != this)
        {
            g_SparklingLevel = this;
            g_LevelBobPhase = 0.0f;
            for (u32 gem = 0; gem < GameProgress::Gems; gem++)
            {
                oleg->emitters[gem].particles.Clear();
            }
        }

        f32 phase = g_LevelBobPhase;
        f32 values[4];
        Platform::Math::SinCos(phase, 0.0f, values);
        phase = phase + BobStep;
        bob = values[0] * BobAmount;
        g_LevelBobPhase = phase;
        if (phase >= Pi)
        {
            g_LevelBobPhase = phase - Pi;
        }

        const LevelProgress found = controller->progress.levels[g_AreaLevels[item->Id()]];
        Vector2 half;
        CopyVector2(&half, &scale);
        FitSize(0, &half);
        f32 spacing = half.x * GemSpacing;
        half = {half.x * 0.5f, half.y * 0.5f};
        for (u32 gem = 0; gem < GameProgress::Gems; gem++)
        {
            if ((found.gems & 1 << gem) != 0)
            {
                Vector2 start = {0.0f, half.y};
                Vector2 end = {spacing, 0.0f};
                Emitter2DStep(&oleg->emitters[gem], FrameSeconds, &start, &end);
            }
        }
    }

    Widget::BeginFrame();
}

void LevelWidget::Draw(Renderer* renderer)
{
    if (flags.invisible == 0 && State() >= StateAppearing)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        OLEG* oleg = &controller->oleg;
        u32 area = item->Id();
        s32 level = g_AreaLevels[area];
        u32 open = controller->progress.play.open;
        u32 title = area <= open ? OLEG::SpriteLevelTitles + level : OLEG::SpriteLockedLevel;
        const LevelProgress* found = &controller->progress.levels[level];
        u32 layer = flags.layer;
        FitPlace(0, &at);
        FitSize(0, &size);
        f32 spacing = size.x * GemSpacing;
        Vector2 titleSize;
        if (IsSelected(0) != 0)
        {
            titleSize = {size.x * g_BreathingScale.scale * TitleWidth, size.y * g_BreathingScale.scale * TitleHeight};
        }
        else
        {
            ColourScale(&drawColour, 0.5f);
            titleSize = {size.x * TitleWidth, size.y * TitleHeight};
        }

        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        matrix.m[0][0] = titleSize.x;
        matrix.m[1][1] = titleSize.y;
        matrix.m[3][0] = at.x - size.x * TitleOffset;
        matrix.m[3][1] = at.y;
        renderer->colour = drawColour;
        QueuePlacedShape(renderer, &matrix, &oleg->sprites[title], layer);
        matrix.m[0][0] = spacing;
        matrix.m[1][1] = size.y;
        matrix.m[3][1] = at.y - size.y * 0.5f;
        matrix.m[3][0] = at.x - (size.x - spacing * 5.0f) * 0.5f;
        Matrix4x4 placed;
        InitIdentityMatrix(&placed);
        for (u32 gem = 0; gem < GameProgress::Gems; gem++)
        {
            u32 sprite = (found->gems & 1 << gem) != 0 ? OLEG::SpriteGems + gem : OLEG::SpriteEmptyGem;
            QueuePlacedShape(renderer, &matrix, &oleg->sprites[sprite], layer);
            if (IsSelected(0) != 0 && sprite != OLEG::SpriteEmptyGem)
            {
                placed.m[3][0] = matrix.m[3][0] + spacing * 0.5f;
                placed.m[3][1] = at.y;
                Emitter2D& emitter = oleg->emitters[gem];
                CallVirtual<void>(&emitter, emitter.vtable, Emitter2D::DrawPlacedSlot, static_cast<const Matrix4x4*>(&placed));
            }

            matrix.m[3][0] = matrix.m[3][0] + spacing;
        }
    }

    Widget::Draw(renderer);
}

EABI_EXPORT(FUN_0017a378, LevelWidget::Construct);

SaveSlotWidget* SaveSlotWidget::Construct(SaveSlotWidget* widget, f32 anchor, MenuPage* page, MenuItem* item,
                                          GameController* controller, SaveSummary* summary)
{
    ItemWidget::Construct(widget, anchor, page, item);
    widget->controller = controller;
    widget->summary = summary;
    widget->vtable = g_SaveSlotWidgetVTable;
    return widget;
}

void SaveSlotWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void SaveSlotWidget::Draw(Renderer* renderer)
{
    if (flags.invisible != 0 || State() < StateAppearing)
    {
        Widget::Draw(renderer);
        return;
    }

    u32 drawColour = colour;
    Vector2 at;
    Vector2 size;
    CopyVector2(&at, &place);
    CopyVector2(&size, &scale);
    Font* font = &controller->font;
    u32 layer = flags.layer;
    FitPlace(0, &at);
    FitSize(0, &size);
    if (!summary->folder.HasSave())
    {
        const char* empty = GameText(EmptySlotText);
        if (IsSelected(0) != 0)
        {
            size = {size.x * g_BreathingScale.scale, size.y * g_BreathingScale.scale};
        }
        else
        {
            ColourScale(&drawColour, item->IsShownTo(0) ? 0.75f : 0.5f);
        }

        renderer->font = font;
        renderer->colour = drawColour;
        renderer->textScale = size;
        renderer->textAlignment.value = TextAlignment::Centred;
        QueueText(renderer, empty, at.x, at.y);
        Widget::Draw(renderer);
        return;
    }

    OLEG* oleg = &controller->oleg;
    SavedProgress saved = summary->progress;
    s32 level = g_AreaLevels[saved.area];
    u32 character = saved.character;
    // Crash's head without a character
    u32 head = character != GameProgress::NoCharacter ? OLEG::SpriteHeads + character : OLEG::SpriteHeads;
    s32 time = summary->time;
    String crystals;
    StringConstruct(&crystals, " ");
    String name;
    StringConstruct(&name, GameText(SlotLevelNameTexts + level));
    String lives;
    StringConstruct(&lives, "  ");
    Vector2 pictureSize;
    CopyVector2(&pictureSize, &size);
    Vector2 half;
    CopyVector2(&half, &size);
    String played = {nullptr, 0, 0};
    String done = {nullptr, 0, 0};
    if (IsSelected(0) != 0)
    {
        pictureSize = {pictureSize.x * Rounded(0.09) * g_BreathingScale.scale,
                       pictureSize.y * Rounded(0.12) * g_BreathingScale.scale};
    }
    else
    {
        ColourScale(&drawColour, 0.75f);
        pictureSize = {pictureSize.x * Rounded(0.09), pictureSize.y * Rounded(0.12)};
    }

    half = {half.x * 0.5f, half.y * 0.5f};
    renderer->colour = drawColour;
    renderer->font = font;
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    s32 minutes = static_cast<s32>(static_cast<f32>(time) * g_SecondsPerClockUnit * MinutesPerSecond);
    s32 hours = minutes / 60;
    minutes = minutes - hours * 60;
    String number;
    StringConstructNumber(&number, summary->progress.crystals);
    StringAppend(&crystals, number.string);
    StringDestroy(&number);
    StringConstructNumber(&number, summary->progress.lives - 1);
    StringAppend(&lives, number.string);
    StringDestroy(&number);
    // The name's words on lines of their own
    for (s32 letter = 0; letter < name.length; letter++)
    {
        if (name.string[letter] == ' ')
        {
            name.string[letter] = '~';
        }
    }

    StringConstructInteger(&number, hours);
    StringAppend(&played, number.string);
    StringDestroy(&number);
    StringAppend(&played, minutes < 10 ? ":0" : ":");
    StringConstructInteger(&number, minutes);
    StringAppend(&played, number.string);
    StringDestroy(&number);
    StringConstructNumber(&number, summary->progress.done);
    StringAppend(&done, number.string);
    StringDestroy(&number);
    StringAppend(&done, "%");

    matrix.m[0][0] = pictureSize.x;
    matrix.m[1][1] = pictureSize.y;
    matrix.m[3][1] = at.y - size.y * Rounded(0.04);
    matrix.m[3][0] = at.x - size.x * Rounded(0.13);
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[OLEG::SpriteLevelTitles + level], layer);
    renderer->textAlignment.value = TextAlignment::BottomCentre;
    renderer->textScale = half;
    QueueText(renderer, name.string, at.x, at.y);
    renderer->textAlignment.value = TextAlignment::BottomRight;
    renderer->textScale = {size.x * 0.5f, size.y * 0.5f};
    matrix.m[3][0] = at.x;
    matrix.m[3][1] = at.y + size.y * Rounded(0.05);
    QueueText(renderer, played.string, at.x, matrix.m[3][1]);
    matrix.m[3][0] = at.x + size.x * Rounded(0.1);
    QueueText(renderer, done.string, matrix.m[3][0], matrix.m[3][1]);
    matrix.m[0][0] = size.x * Rounded(0.075);
    matrix.m[1][1] = size.y * Rounded(0.075);
    matrix.m[3][1] = at.y - size.y * Rounded(0.05);
    matrix.m[3][0] = at.x + size.x * Rounded(0.15);
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[head], layer);
    renderer->textAlignment.value = TextAlignment::TopLeft;
    renderer->textScale = {size.x * 0.625f, size.y * 0.625f};
    QueueText(renderer, lives.string, matrix.m[3][0], matrix.m[3][1]);
    matrix.m[0][0] = size.x * Rounded(0.04);
    matrix.m[1][1] = size.y * Rounded(0.075);
    matrix.m[3][1] = at.y + size.y * Rounded(0.05);
    matrix.m[3][0] = at.x + size.x * 0.125f;
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[OLEG::SpriteCrystal], layer);
    renderer->textAlignment.value = TextAlignment::MiddleLeft;
    renderer->textScale = {size.x * 0.625f, size.y * 0.625f};
    QueueText(renderer, crystals.string, matrix.m[3][0], matrix.m[3][1]);
    StringDestroy(&done);
    StringDestroy(&played);
    StringDestroy(&lives);
    StringDestroy(&name);
    StringDestroy(&crystals);
    Widget::Draw(renderer);
}

EABI_EXPORT(FUN_00179db0, SaveSlotWidget::Construct);

void HudBarWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void SliderWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

MenuWidget* OLEG::ShownMenu()
{
    MenuWidget* const menus[] = {
        &screenPositionMenu, &extrasMenu, &mainMenu, &levelsMenus[0], &levelsMenus[1], &levelsMenus[2], &levelsMenus[3],
        &optionsMenu, &pauseMenu, &disableAutosaveMenu, &quitMenu, &autosaveOffMenu, &autosaveOnMenu, &autosaveFailedMenu,
        &fourthNoticeMenu,
    };
    for (MenuWidget* menu : menus)
    {
        if (menu->State() == Widget::StateShown)
        {
            return menu;
        }
    }

    return nullptr;
}

void OLEG::SetHudIcon(u32 slot, const u16* modelId)
{
    Sprite& sprite = sprites[SpriteHudIcons + slot];
    if (*modelId == NoModelId)
    {
        hudIcons[slot] = nullptr;
        CallVirtual<void>(&sprite, sprite.vtable, Shape2D::SetMaterialSlot, Platform::Graphics::FlatMaterial());
        return;
    }

    ResourceTable* models = G_GameResourcesObjectPointer->models;
    u16 id;
    CopyResourceId(&id, modelId);
    auto* ogi = id != NoModelId ? static_cast<GameOGI*>(models->items[id & ResourceIndexMask]) : nullptr;
    if (ogi == nullptr)
    {
        return;
    }

    RigidModel* model = ogi->rigidModels != nullptr && ogi->rigidModelCount != 0 ? ogi->rigidModels[0] : nullptr;
    MaterialResource* material = Platform::Graphics::FirstMaterial(model);
    hudIcons[slot] = material;
    CallVirtual<void>(&sprite, sprite.vtable, Shape2D::SetMaterialSlot, material->material);
}

extern "C"
{
    // OLEG's file's start-up: its static initialisation (initialise 1, priority 0xFFFF) and its entry in the static constructors'
    // table
    void InitOlegModule(u32 initialise, u32 priority) RETAIL(FUN_00178890);
    void ConstructOlegModule() RETAIL(FUN_0017ca00);
    // What the start-up sets that nothing reads: a word made 0, white and white without alpha, and 45 degrees (given as radians)
    extern u32 g_OlegUnused620 RETAIL(D_0030A620);
    extern u32 g_OlegUnusedWhite RETAIL(D_0030A668);
    extern u32 g_OlegUnusedClearWhite RETAIL(D_0030A670);
    extern s32 g_OlegUnusedAngle RETAIL(D_0030A7A0);
    // The chunks the game starts in and goes to after the credits (Levels\Earth\Hub\Beach, Levels\Ice\Hub\LabExt)
    extern const char g_StartChunkName[] RETAIL(D_002F5708);
    extern const char g_PostCreditsChunkName[] RETAIL(D_002F5720);
}

namespace
{
template <typename T> T* AllocatePoints(u32 count)
{
    return static_cast<T*>(MemoryAllocate2(count * sizeof(T)));
}

void SetPlace(Vector2* place, f32 x, f32 y)
{
    place->x = x;
    place->y = y;
}
}

void InitOlegModule(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_CrystalColour = {0.0f, 1.0f, 0.0f, 1.0f};
    SetPlace(&g_OlegShadowOffset, UiShadowOffset, UiShadowOffset);
    ColourSet(&g_OlegShadowColour, 0.0f, 0.0f, 0.0f, UiShadowAlpha);
    g_OlegUnused620 = 0;
    StringConstruct(&g_StartChunkPath, g_StartChunkName);
    StringConstruct(&g_PostCreditsChunkPath, g_PostCreditsChunkName);
    SetPlace(&g_OlegTitlePlace, 0.5f, Rounded(0.414));
    SetPlace(&g_OlegMenuPlace, 0.5f, Rounded(0.764));
    SetPlace(&g_OlegListItemsPlace, 0.0f, 0.0f);
    SetPlace(&g_OlegListItemsScale, Rounded(0.7), Rounded(0.7));
    SetPlace(&g_OlegNoticeTitlePlace, 0.5f, Rounded(0.15));
    SetPlace(&g_OlegNoticeTitleScale, 0.75f, 0.75f);
    SetPlace(&g_OlegNoticeTextPlace, 0.5f, Rounded(0.4));
    SetPlace(&g_OlegNoticeTextScale, 0.625f, 0.625f);
    ColourSet(&g_OlegUnusedWhite, 1.0f, 1.0f, 1.0f, 1.0f);
    ColourSet(&g_OlegUnusedClearWhite, 1.0f, 1.0f, 1.0f, 0.0f);
    GetColor(&g_OlegLetterboxColour, ColourBlack);
    SetPlace(&g_OlegTopBarFrom, 0.0f, 0.0f);
    SetPlace(&g_OlegTopBarTo, 1.0f, Rounded(0.15));
    SetPlace(&g_OlegTopBarHiddenFrom, 0.0f, Rounded(-0.15));
    SetPlace(&g_OlegTopBarHiddenTo, 1.0f, 0.0f);
    SetPlace(&g_OlegBottomBarFrom, 0.0f, Rounded(0.85));
    SetPlace(&g_OlegBottomBarTo, 1.0f, 1.0f);
    SetPlace(&g_OlegBottomBarHiddenFrom, 0.0f, 1.0f);
    SetPlace(&g_OlegBottomBarHiddenTo, 1.0f, Rounded(1.15));
    ColourSet(&g_OlegBackdropColour, 0.0f, 0.0f, 0.0f, 0.5f);
    ColourSet(&g_OlegBackdropHiddenColour, 0.0f, 0.0f, 0.0f, 0.0f);
    ColourSet(&g_OlegBottomTextColour, 1.0f, 1.0f, 1.0f, 1.0f);
    ColourSet(&g_OlegBottomTextHiddenColour, 1.0f, 1.0f, 1.0f, 0.0f);
    SetPlace(&g_OlegBackdropTo, 1.0f, 1.0f);
    SetPlace(&g_OlegBackdropFrom, 0.0f, Rounded(0.85));
    SetPlace(&g_OlegBottomTextPlace, 0.5f, Rounded(0.9));
    SetPlace(&g_OlegBottomTextScale, 0.5f, 0.5f);
    ColourSet(&g_OlegPictureHiddenColour, 1.0f, 1.0f, 1.0f, 0.0f);
    ColourSet(&g_OlegPictureColour, 1.0f, 1.0f, 1.0f, 1.0f);
    AngleFrom(&g_OlegMenuRingsTurn, 15.0f, AngleDegrees);
    AngleFrom(&g_OlegGemArcTurn, 21.0f, AngleDegrees);
    AngleFrom(&g_OlegMenuMiddleRingsTurn, 25.0f, AngleDegrees);
    AngleFrom(&g_OlegGemArcLastStart, -35.0f, AngleDegrees);
    AngleFrom(&g_OlegGemArcLastSpan, 120.0f, AngleDegrees);
    AngleFrom(&g_OlegGemArcStart, -36.0f, AngleDegrees);
    AngleFrom(&g_OlegGemArcSpan, 122.0f, AngleDegrees);
    Platform::Graphics::ConstructMaterial(Platform::Graphics::FlatMaterial());
    ColourSet(&g_OlegRingBorderColour, 1.0f, 1.0f, 1.0f, 1.0f);
    ColourSet(&g_OlegRingEdgeColour, 1.0f, 1.0f, 1.0f, 0.0f);
    ColourSet(&g_OlegPanelColour, Rounded(0.1), Rounded(0.588), 1.0f, 1.0f);
    ColourSet(&g_OlegPanelClearColour, Rounded(0.1), Rounded(0.588), 1.0f, 0.0f);
    ColourSet(&g_OlegGemArcTrimColour, Rounded(0.325), 1.0f, Rounded(0.9), 1.0f);
    ColourSet(&g_OlegGemArcColour, 1.0f, 1.0f, 1.0f, 1.0f);
    ColourSet(&g_OlegGemArcClearColour, 1.0f, 1.0f, 1.0f, 0.0f);
    // The titles' breathing: its values, not running
    g_BreathingScale.values = AllocatePoints<f32>(BreathingValues);
    g_BreathingScale.flags.count = BreathingValues;
    g_BreathingScale.flags.running = 0;
    g_OlegShrinkingShape.count = 8;
    g_OlegShrinkingShape.points = AllocatePoints<Vector2>(g_OlegShrinkingShape.count);
    g_OlegGrowingShape.count = 8;
    g_OlegGrowingShape.points = AllocatePoints<Vector2>(g_OlegGrowingShape.count);
    g_OlegGlintColours.count = 16;
    g_OlegGlintColours.points = AllocatePoints<Vector4>(g_OlegGlintColours.count);
    g_OlegPanelColours.count = 8;
    g_OlegPanelColours.points = AllocatePoints<Vector4>(g_OlegPanelColours.count);
    AngleFrom(&g_OlegUnusedAngle, QuarterPi, AngleRadians);
    SetPlace(&g_FontSize, Rounded(0.046), Rounded(0.085));
    SetPlace(&g_SmallFontSize, Rounded(0.029), Rounded(0.056));
}

void ConstructOlegModule()
{
    InitOlegModule(1, DefaultInitPriority);
}
