#include "game/oleg.h"

#include "game/clock.h"
#include "game/colour.h"
#include "game/context.h"
#include "game/gamecontroller.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/olegpages.h"
#include "game/overlay.h"
#include "game/pads.h"
#include "game/player.h"
#include "game/readers.h"
#include "game/renderer.h"
#include "game/savemanager.h"
#include "game/stream.h"
#include "platform/graphics.h"
#include "platform/math.h"

namespace
{
// Memory for an object of a type
template <typename T> T* Allocate()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}

// Anchors: the middle, near the left and near the right
constexpr f32 AnchorMiddle = 0.5f;
constexpr f32 AnchorLeft = 0x1.99999Ap-5f;
constexpr f32 AnchorRight = 0x1.E66666p-1f;
constexpr f32 AnchorFifth = 0x1.99999Ap-3f;
// Texts' alignments
constexpr u32 Centred = 0x22;
constexpr u32 LeftAligned = 0x14;
constexpr u32 RightAligned = 0x44;
constexpr s32 WhiteColour = 0xF;
// The pulse's curve
constexpr u32 PulsePoints = 4;
// The sprites' vtable function that sets their material
constexpr u32 SpriteSetMaterialSlot = 2;
constexpr u32 FlatSprites = 6;

// The HUD: the play modes (the progress's bits 0-3: health, timed), the controls' kind of the slider, the character whose lives
// sprite sparkles (Crash: the eighth emitter), the levels' names (the area's levels' texts, unknown past the last area open), the
// gems' sprites (13 to 18, 12 none) and emitters (1 to 6)
constexpr u32 ModeMask = 0xF;
constexpr u32 ModeHealth = 1;
constexpr u32 ModeTimed = 2;
constexpr u32 SliderVehicle = 6;
constexpr u32 CrashCharacter = 0;
constexpr u32 CrashLivesEmitter = 7;
constexpr u32 UnknownLevelText = 0x58;
constexpr u32 LevelNameTexts = 0x47;
constexpr u32 FirstGemEmitter = 1;
constexpr u32 FirstIcon = 6;
constexpr u32 LevelPicture = 1;
// The pictures read from files: the one read and the one wanted (bits 1-8, 9-16) and what each is without a picture
constexpr u32 Pictures = 3;
constexpr u32 PictureShift = 1;
constexpr u32 WantedPictureShift = 9;
constexpr u32 PictureMask = 0xFF;
constexpr u32 PictureBeingRead = 1;
constexpr u32 NoPicture[Pictures] = {1, 25, 12};
// The screen pictures named by OLEG's text
constexpr u32 FirstNamedPicture = 9;
constexpr u32 EndNamedPictures = 11;
// The picture files' readers: in the second storage, the file read whole into memory that goes once it's read
constexpr u32 PictureReaders = 1;
constexpr u32 PictureFileFlags = 0xD;
// The sprites' function that lets go of what they drew, their destructor, and their reading from a stream
constexpr u32 ReleaseResourceSlot = 4;
constexpr u32 ShapeReadSlot = 9;
constexpr u32 ShapeDestroySlot = 1;
// The pages', the widgets' and the 2D emitters' destructors
constexpr u32 PageDestroySlot = 4;
constexpr u32 WidgetDestroySlot = 9;
constexpr u32 EmitterDestroySlot = 3;
constexpr u32 EmitterDrawSlot = 4;
// Destructor flags: an object deleted, a member destroyed, an array's element destroyed
constexpr u32 Delete = 3;
constexpr u32 Member = 2;
constexpr u32 Element = 0;

// The health bar's sprites: its body, its start, the gaps and pieces, its end
constexpr u32 BarBody = 3;
constexpr u32 BarStart = 25;
constexpr u32 BarGap = 26;
constexpr u32 BarPiece = 27;
constexpr u32 BarEnd = 28;

// The levels' sprites: the locked level's title, then the levels' titles; the empty gem slot and the gems
constexpr u32 LockedLevelTitle = 29;
constexpr u32 LevelTitles = 30;
constexpr u32 EmptyGemSlot = 12;
constexpr u32 GemSprites = 13;
constexpr u32 Gems = 6;

// The texts of the levels' names (by the level) and of an empty save slot; the crystal's sprite
constexpr u32 LevelNames = 0xBC;
constexpr u32 EmptySlotText = 0x3C;
constexpr u32 CrystalSpriteIndex = 21;

// The screens of the HUD's lives and wumpa counts
constexpr u32 LivesScreen = 36;
constexpr u32 WumpaScreen = 37;
// The pickup sparkle's screen, and the crystal's sprite
constexpr u32 PickupScreen = 38;
constexpr u32 CrystalSprite = 20;
// The characters' heads (the sprites from the seventh on), and the extra life's sound
constexpr u32 CharacterHeads = 6;
constexpr u32 ExtraLifeSound = 0x10A;

// The widget slots each screen shows
constexpr u64 ScreenWidgets[45] = {
    0x9,           0x2,           0xC,           0x10,          0x80,          0x100,         0x200,
    0x400,         0x800,         0x12B0,        0x22F0,        0xC2F0,        0x90EF0,       0x20EF0,
    0x40EF0,       0x100030,      0x200030,      0x400270,      0x800270,      0x1000270,     0x400270,
    0x44000CB0,    0x48000CB0,    0x50000CB0,    0x60000CB0,    0x80000EB0,    0x400100000080, 0x600200000200,
    0x400000030,   0x800000270,   0x1000000270,  0x2000000000,  0x60000000000, 0x4000000000,  0x48000000000,
    0x10000000000, 0x40000000000, 0x20000000000, 0x80000000000, 0x100000000000, 0x200000000000, 0x400000000000,
    0xC00000000000, 0xFFFFFFFFFFFFFFF8, 0xFFFFFFFFFFFEFFFF,
};

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

const MenuDrawer::Pulse* ScalerPulse()
{
    return reinterpret_cast<const MenuDrawer::Pulse*>(&g_OlegScaler);
}

// A drawer's styles but the title's in a font: the selected item in the game's colour 15, the others 17 and 21
void SetItemStyles(MenuDrawer* drawer, Font* font)
{
    const s32 colours[3] = {WhiteColour, 0x11, 0x15};
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
    drawer->itemFlags = Centred;
    drawer->itemsPlace = g_OlegPlace638;
    drawer->manyItemsScale = g_OlegPlace640;
    drawer->manyItemsSpacing = 0x1.99999Ap-5f;
    SetItemStyles(drawer, font);
    drawer->sizePulse = ScalerPulse();
}

// A string label's destructor, inline in OLEG's: its text, then the widget's
void DestroyStringLabel(StringLabel* label)
{
    StringDestroy(&label->text);
    label->Widget::Destroy(Member);
}

u32 White()
{
    u32 colour;
    GetColor(&colour, WhiteColour);
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
    widget->flags = (widget->flags & ~Widget::LayerMask) | layer << Widget::LayerShift;
}

// A sprite widget's pulse (every 3 seconds, a tenth bigger) and its sway about a quarter turn (π/20 either way every 7 seconds)
void Swaying(SpriteWidget* widget)
{
    widget->pulseAmount = 0x1.99999Ap-4f;
    widget->wobblePeriod = 7.0f;
    widget->wobbleBase = 0x1.921FB6p+0f;
    widget->wobbleAmount = 0x1.41B2F8p-3f;
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
    widget->flags = (widget->flags & ~Widget::Invisible) | (visible ? 0 : Widget::Invisible);
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
    effect->curve.points[1] = {0x1.333334p+0f, 0x1.333334p+0f};
    effect->curve.points[2] = {0x1.4CCCCCp+0f, 0x1.4CCCCCp+0f};
    effect->curve.points[3] = {1.0f, 1.0f};
    return effect;
}

void OLEG::SetUpScreens()
{
    for (u32 screen = 0; screen < 45; screen++)
    {
        masks[screen] = ScreenWidgets[screen];
    }
}

void OLEG::SetUpParticles()
{
    Material* own = OwnMaterial(this);
    particleShader = Platform::Graphics::MakeParticleMaterial(own);
    Vector2 start = {0.25f, 0.0f};
    Vector2 end = {0.5f, 0.25f};
    sprite81C.SetMaterial(own, &start, &end);

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
    particleTurns[0].values[1] = 0x1.0c1524p+0f;
    particleTurns[1].values[0] = 0.0f;
    particleTurns[1].values[1] = 0x1.921fb6p+2f;

    // The first emitter (6 particles), the gems' (4 each), the eighth (2: the lives' sparkle) and the sparkle effect's (128)
    Emitter2D& first = emitters[0];
    first.SetSprite(&sprite81C, 6);
    first.chance = Rounded(0.03);
    first.lifetime = 0.5f;
    first.spread = {Rounded(0.65), 0.75f};
    first.size = {Rounded(0.05), Rounded(0.05)};
    first.turns = &particleTurns[0];
    first.colours = &particleColours;
    first.sizes = &particleSizes;
    for (u32 gem = 1; gem <= 6; gem++)
    {
        Emitter2D& emitter = emitters[gem];
        emitter.SetSprite(&sprite81C, 4);
        emitter.lifetime = Rounded(0.45);
        emitter.chance = Rounded(0.023);
        emitter.size = {Rounded(0.05), Rounded(0.05)};
        emitter.spread = {0.5f, 0.5f};
        emitter.colours = &particleColours;
        emitter.sizes = &particleSizes;
        emitter.turns = &particleTurns[1];
    }

    Emitter2D& lives = emitters[7];
    lives.SetSprite(&sprite81C, 2);
    lives.lifetime = Rounded(0.4);
    lives.chance = Rounded(0.001);
    lives.size = {Rounded(0.05), Rounded(0.05)};
    lives.spread = {Rounded(0.33), 0.25f};
    lives.turns = &particleTurns[1];
    lives.colours = &particleColours;
    lives.sizes = &particleSizes;
    lives.offset = {-Rounded(0.01), Rounded(0.04)};

    RadialEmitter2D& sparkles = sparkle.emitter;
    sparkles.SetSprite(&sprite81C, 0x80);
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
    for (u32 sprite = 0; sprite < FlatSprites; sprite++)
    {
        CallVirtual<void>(&sprites[sprite], sprites[sprite].vtable, SpriteSetMaterialSlot, material);
    }
}

void OLEG::SetUp16A0F8()
{
    Vector2 place = {0x1.333334p-3f, 0x1.99999Ap-1f};
    Vector2 scale = {1.0f, 1.0f};
    string3F5C.FadeIn(White(), &place, &scale);
    place = {0x1.99999Ap-4f, 0x1.CCCCCCp-1f};
    scale = {0x1.99999Ap-4f, 0x1.99999Ap-4f};
    sprite3FF8.FadeRectangle(White(), &place, &scale);
    place = {0x1.333334p-3f, 0x1.CCCCCCp-1f};
    scale = {1.0f, 1.0f};
    string40B4.FadeIn(White(), &place, &scale);
    place = {0x1.99999Ap-4f, 0x1.CCCCCCp-1f};
    scale = {0x1.99999Ap-4f, 0x1.99999Ap-4f};
    sprite4150.FadeRectangle(White(), &place, &scale);
    string3F5C.next = &sprite3FF8;
    sprite3FF8.next = &string40B4;
    string40B4.next = &sprite4150;
}

void OLEG::SetUp16A278()
{
    SetPlaces(&sprite1240, g_OlegColour678, g_OlegColour678, &g_OlegPlace690, &g_OlegPlace680, &g_OlegPlace698, &g_OlegPlace688);
    SetLayer(&sprite1240, 2);
    SetPlaces(&sprite12FC, g_OlegColour678, g_OlegColour678, &g_OlegPlace6B0, &g_OlegPlace6A0, &g_OlegPlace6B8, &g_OlegPlace6A8);
    SetLayer(&sprite12FC, 2);
    u32 hidden;
    u32 shown;
    GetColor(&hidden, 0);
    GetColor(&shown, 8);
    sprite13B8.hiddenColour = hidden;
    sprite13B8.shownColour = shown;
    SetLayer(&sprite13B8, 1);
    SetPlaces(&sprite1474, g_OlegColour6C8, g_OlegColour6C0, &g_OlegPlace6E8, &g_OlegPlace6E8, &g_OlegPlace6E0, &g_OlegPlace6E0);
    SetLayer(&sprite1474, 2);
    SetPlaces(&string1530, g_OlegColour6D8, g_OlegColour6D0, &g_OlegPlace6F0, &g_OlegPlace6F0, &g_OlegPlace6F8, &g_OlegPlace6F8);
    sprite1240.next = &sprite12FC;
}

extern "C" void AddPanelRings(const Vector2* radii, RingWidget* widget)
{
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* inner = widget->AddRing(0, material);
    Ring* border1 = widget->AddRing(1, material);
    Ring* border2 = widget->AddRing(2, material);
    Ring* border3 = widget->AddRing(3, material);
    inner->outerScale = *radii;
    inner->outerColours = &g_OlegColours798;
    inner->middleColour = g_OlegColour758;
    border1->outerScale = {radii->x + 0x1.89374Cp-9f, radii->y + 0x1.89374Cp-9f};
    border1->colour = g_OlegColour748;
    border2->outerScale = {radii->x + 0x1.47AE14p-7f, radii->y + 0x1.47AE14p-7f};
    border2->colour = g_OlegColour748;
    border3->outerScale = {radii->x + 0x1.A9FBE8p-7f, radii->y + 0x1.A9FBE8p-7f};
    border3->colour = g_OlegColour750;
    SetShadow(widget);
}

void OLEG::SetUp16A518()
{
    Vector2 place = {0.5f, 0x1.99999Ap-4f};
    Vector2 scale = {1.0f, 1.0f};
    extrasMenu.FadeIn(White(), &place, &scale);
    extrasPanel.FadeIn(White(), &place, &scale);
    place = {0x1.5C28F6p-3f, 0x1.0A3D70p-3f};
    scale = {0x1.EB851Ep-3f, 0x1.47AE14p-3f};
    extrasPicture.FadeRectangle(White(), &place, &scale);
    place = {0x1.99999Ap-5f, 0x1.E66666p-1f};
    scale = {0.625f, 0.625f};
    nextHint.FadeIn(White(), &place, &scale);
    Vector2 radii = {0x1.666666p-2f, 0x1.99999Ap-5f};
    AddPanelRings(&radii, &extrasPanel);
    Swaying(&extrasPicture);
    SetLayer(&extrasPicture, 5);
    extrasMenu.next = &extrasPanel;
    extrasPicture.next = &nextHint;
}

void OLEG::SetUp16A740()
{
    Swaying(&titleLogo);
    SetLayer(&titleLogo, 5);
    SetShadow(&titleLogo);
    Vector2 scale = {1.0f, 1.0f};
    mainMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    Vector2 middle = {0.5f, 0x1.666666p-2f};
    Vector2 size = {0x1.666666p-1f, 0x1.CCCCCCp-2f};
    titleLogo.GrowRectangle(White(), &middle, &size, &g_OlegPlace628);
}

void OLEG::SetUp16A890()
{
    Swaying(&loadingLogo);
    SetLayer(&loadingLogo, 5);
    loadingText.scaler = reinterpret_cast<Widget::Scaler*>(&g_OlegScaler);
    picture4974.shownColour = g_OlegColour708;
    picture4974.hiddenColour = g_OlegColour700;
    SetLayer(&picture4974, 4);
    u32 hidden;
    u32 shown;
    GetColor(&hidden, 0);
    GetColor(&shown, 8);
    sprite48B8.hiddenColour = hidden;
    sprite48B8.shownColour = shown;
    SetLayer(&sprite48B8, 1);
    Vector2 middle = {0x1.99999Ap-3f, 0x1.333334p-3f};
    Vector2 size = {0x1.333334p-2f, 0x1.99999Ap-3f};
    loadingLogo.FadeRectangle(White(), &middle, &size);
    Vector2 place = {0.5f, 0x1.CCCCCCp-1f};
    Vector2 scale = {0.75f, 0.75f};
    loadingText.FadeIn(White(), &place, &scale);
    loadingLogo.next = &loadingText;
}

void OLEG::SetUp16B718()
{
    Vector2 place = {0.5f, 0x1.99999Ap-5f};
    Vector2 scale = {1.0f, 1.0f};
    for (MenuWidget& menu : levelsMenus)
    {
        menu.FadeIn(White(), &place, &scale);
    }

    levelName.scaler = reinterpret_cast<Widget::Scaler*>(&g_OlegScaler);
    place = {0.5f, 0.75f};
    scale = {0.625f, 0.625f};
    Vector2 offset = {0.0f, 0.5f};
    levelName.SlideIn(White(), &place, &scale, &offset);
    scale = {1.0f, 1.0f};
    levelNamePanel.SlideIn(White(), &place, &scale, &offset);
    Vector2 radii = {0x1.333334p-2f, 0x1.99999Ap-5f};
    AddPanelRings(&radii, &levelNamePanel);
    levelName.next = &levelNamePanel;
}

void OLEG::SetUp16B938()
{
    Vector2 place = {0.5f, 0x1.99999Ap-4f};
    Vector2 scale = {1.0f, 1.0f};
    optionsMenu.FadeIn(White(), &place, &scale);
    optionsPanel.FadeIn(White(), &place, &scale);
    Vector2 radii = {0x1.666666p-2f, 0x1.99999Ap-5f};
    AddPanelRings(&radii, &optionsPanel);
    optionsMenu.next = &optionsPanel;
}

void OLEG::SetUpPauseMenu()
{
    Vector2 scale = {1.0f, 1.0f};
    pauseMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    disableAutosaveMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    quitMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    Vector2 place = {0.5f, 0.25f};
    Vector2 from = {0.5f, 0x1.333334p-2f};
    discErrorTitle.GrowIn(White(), &place, &scale, &from);
    disableAutosaveTitle.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    quitTitle.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    scale = {0.75f, 0.75f};
    noControllerText.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    scale = {0x1.99999Ap-1f, 0x1.99999Ap-1f};
    discErrorText.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    scale = {0x1.B33334p-1f, 0x1.B33334p-1f};
    autosaveOffTitle.GrowIn(White(), &place, &scale, &g_OlegPlace628);
    autosaveOffText.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace628);
    scale = {1.0f, 1.0f};
    autosaveOffMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    autosaveOnTitle.GrowIn(White(), &g_OlegPlace648, &g_OlegPlace650, &g_OlegPlace628);
    autosaveOnText.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace628);
    Vector2 middle = {0.5f, 0x1.59999Ap-1f};
    Vector2 size = {0x1.99999Ap-4f, 0x1.333334p-4f};
    autosaveOnIcon.GrowRectangle(White(), &middle, &size, &g_OlegPlace630);
    autosaveOnMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    autosaveFailedText.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace628);
    autosaveFailedMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    scale = {0x1.B33334p-1f, 0x1.B33334p-1f};
    label2908.GrowIn(White(), &place, &scale, &g_OlegPlace628);
    label299C.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace628);
    scale = {1.0f, 1.0f};
    menu2A30.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    disableAutosaveMenu.next = &disableAutosaveTitle;
    quitMenu.next = &quitTitle;
    discErrorTitle.next = &discErrorText;
    autosaveOffTitle.next = &autosaveOffText;
    autosaveOffText.next = &autosaveOffMenu;
    autosaveOnTitle.next = &autosaveOnIcon;
    autosaveOnIcon.next = &autosaveOnText;
    autosaveOnText.next = &autosaveOnMenu;
    autosaveFailedText.next = &autosaveFailedMenu;
    label2908.next = &label299C;
    label299C.next = &menu2A30;

    // The completion's disc and its border, and the gems' arc: four rings about a third of the way round on the right
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* completion = completionRing.AddRing(0, material);
    Ring* completionBorder = completionRing.AddRing(1, material);
    Ring* arcs[4];
    for (u32 ring = 0; ring < 4; ring++)
    {
        arcs[ring] = gemArc.AddRing(ring, material);
    }

    completion->outerColours = &g_OlegColours790;
    completion->middleColour = g_OlegColour758;
    completion->outerScale = {0x1.333334p-4f, 0x1.99999Ap-4f};
    completionBorder->colour = g_OlegColour760;
    completionBorder->outerScale = {0x1.3B645Ap-4f, 0x1.A1CAC0p-4f};
    const u32 middleColours[4] = {g_OlegColour770, g_OlegColour778, g_OlegColour770, g_OlegColour768};
    const u32 colours[4] = {g_OlegColour770, g_OlegColour770, g_OlegColour778, g_OlegColour768};
    Vector2Curve* const innerShapes[4] = {&g_OlegCurve780, &g_OlegCurve780, &g_OlegCurve788, &g_OlegCurve780};
    Vector2Curve* const outerShapes[4] = {&g_OlegCurve788, &g_OlegCurve780, &g_OlegCurve788, &g_OlegCurve788};
    const Vector2 innerScales[4] = {
        {0x1.51EB86p-2f, 0x1.3B645Ap-2f},
        {0x1.4CCCCCp-2f, 0x1.3645A2p-2f},
        {0x1.AE147Ap-2f, 0x1.916872p-2f},
        {0x1.5A1CACp-2f, 0x1.428F5Cp-2f},
    };
    const Vector2 outerScales[4] = {
        {0x1.AE147Ap-2f, 0x1.916872p-2f},
        {0x1.51EB86p-2f, 0x1.3B645Ap-2f},
        {0x1.B33334p-2f, 0x1.96872Cp-2f},
        {0x1.A6E978p-2f, 0x1.8A3D70p-2f},
    };
    for (u32 ring = 0; ring < 4; ring++)
    {
        arcs[ring]->startAngle = ring < 3 ? g_OlegAngle738 : g_OlegAngle728;
        arcs[ring]->span = ring < 3 ? g_OlegAngle740 : g_OlegAngle730;
        arcs[ring]->middleColour = middleColours[ring];
        arcs[ring]->colour = colours[ring];
        arcs[ring]->innerShape = innerShapes[ring];
        arcs[ring]->outerShape = outerShapes[ring];
        arcs[ring]->innerScale = innerScales[ring];
        arcs[ring]->outerScale = outerScales[ring];
        arcs[ring]->rotation = g_OlegAngle718;
    }

    gemArc.ringFlags |= RingWidget::StandApart;
    const Vector2 radii = {0x1.0E5604p-4f, 0x1.6872B0p-4f};
    AddPanelRings(&radii, &wumpaRing);
    AddPanelRings(&radii, &livesRing);
    AddPanelRings(&radii, &crystalRing);

    place = {0x1.6B851Ep-1f, 0x1.20C49Ap-1f};
    from = {0.5f, 0.5f};
    completionRing.GrowIn(White(), &place, &scale, &from);
    completionText.GrowIn(White(), &place, &scale, &from);
    place = {0x1.0F5C28p-1f, 0x1.B020C6p-2f};
    Vector2 offset = {0.5f, 0.0f};
    gemArc.SlideIn(White(), &place, &scale, &offset);
    offset = {-0.5f, 0.0f};
    place = {0x1.666666p-3f, 0x1.B645A4p-3f};
    wumpaRing.SlideIn(White(), &place, &scale, &offset);
    place = {0x1.47AE14p-3f, 0x1.B645A4p-3f};
    wumpaText.SlideIn(White(), &place, &scale, &offset);
    middle = {0x1.958106p-3f, 0x1.978D50p-3f};
    size = {0x1.99999Ap-4f, 0x1.0A3D70p-3f};
    wumpaSprite.SlideRectangle(White(), &middle, &size, &offset);
    place = {0x1.1EB852p-3f, 0x1.C6A7EEp-2f};
    livesRing.SlideIn(White(), &place, &scale, &offset);
    place = {0x1.BA5E36p-4f, 0x1.D0E55Ep-2f};
    livesText.SlideIn(White(), &place, &scale, &offset);
    middle = {0x1.395810p-3f, 0x1.B53F7Ep-2f};
    size = {0x1.0A3D70p-3f, 0x1.0A3D70p-3f};
    livesSprite.SlideRectangle(White(), &middle, &size, &offset);
    place = {0x1.CCCCCEp-3f, 0x1.53F7CEp-1f};
    crystalRing.SlideIn(White(), &place, &scale, &offset);
    place = {0x1.99999Ap-3f, 0x1.547AE0p-1f};
    crystalText.SlideIn(White(), &place, &scale, &offset);
    middle = {0x1.F5C290p-3f, 0x1.53F7CEp-1f};
    size = {0.125f, 0x1.1EB852p-2f};
    crystalSprite.SlideRectangle(White(), &middle, &size, &offset);
    wumpaSprite.pulseAmount = 0x1.99999Ap-4f;
    wumpaSprite.pulsePeriod = 0x1.4CCCCCp+0f;
    livesSprite.bobPeriod = 0x1.7AE148p-2f;
    livesSprite.wobbleAmount = 0x1.921FB6p-3f;
    livesSprite.bobAmount = 0x1.E4F766p-9f;
    livesSprite.wobblePeriod = 1.0f;
    crystalSprite.bobPeriod = 0x1.666666p-1f;
    crystalSprite.bobAmount = 0x1.47AE14p-8f;

    // The gems in the middles of the arc's sixths
    for (s32 gem = 0; gem < 6; gem++)
    {
        gemArc.PointAt(&place, static_cast<f32>(gem * 2 + 1) * 0x1.555556p-4f);
        size = {0x1.99999Ap-4f, 0x1.99999Ap-4f};
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
    for (u32 gem = 0; gem + 1 < 6; gem++)
    {
        gemSprites[gem]->next = gemSprites[gem + 1];
    }
}

void OLEG::SetUp16C9F8()
{
    string3A4C.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace658);
    string3AE8.GrowIn(White(), &g_OlegPlace658, &g_OlegPlace660, &g_OlegPlace658);
    Vector2 scale = {1.0f, 1.0f};
    saveChoicesMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    Vector2 place = {0.5f, 0x1.99999Ap-4f};
    Vector2 offset = {0.0f, -0.5f};
    string3C30.SlideIn(White(), &place, &g_OlegPlace660, &offset);
    saveSlotsPanel.SlideIn(White(), &place, &scale, &offset);
    saveSlotsMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    Vector2 radii = {0x1.CCCCCCp-2f, 0x1.99999Ap-5f};
    AddPanelRings(&radii, &saveSlotsPanel);
    string3AE8.next = &saveChoicesMenu;
    string3C30.next = &saveSlotsPanel;
    saveSlotsPanel.next = &saveSlotsMenu;
}

void OLEG::SetUpPauseRings()
{
    Material* material = Platform::Graphics::FlatMaterial();
    Ring* rings[8];
    for (u32 ring = 0; ring < 8; ring++)
    {
        rings[ring] = ring1688.AddRing(ring, material);
    }

    // The breathing scale: 30 steps of 1 + 0.1 sin of a 60th of a turn more each, from the first
    for (u32 step = 0; step < 30; step++)
    {
        s32 angle;
        AngleFrom(&angle, static_cast<f32>(step) * 0x1.ACEEA2p-4f, 0);
        f32 value = SinOfAngle(&angle) * 0x1.99999Ap-4f + 1.0f;
        g_OlegScaler.values[step] = value;
        if (step == 0)
        {
            g_OlegScaler.scale = value;
        }
    }

    s32 period = static_cast<s32>(g_ClockUnitsPerSecond * 0.5f);
    if ((g_OlegScaler.flags & CyclingScale::Running) == 0)
    {
        g_OlegScaler.limited = 0;
        g_OlegScaler.cyclesLeft = 0;
        g_OlegScaler.period = period;
        g_OlegScaler.time = 0;
        g_OlegScaler.flags = g_OlegScaler.flags | CyclingScale::Running;
    }

    const f32 grow[8] = {0x1.EB851Ep-1f, 0x1.F0A3D8p-1f, 0x1.F5C290p-1f, 0x1.F5C290p-1f,
                         0x1.F5C290p-1f, 0x1.FAE148p-1f, 1.0f,          0x1.051EB8p+0f};
    const f32 shrink[8] = {0x1.07AE14p+0f, 0x1.051EB8p+0f, 0x1.051EB8p+0f, 0x1.051EB8p+0f,
                           0x1.028F5Cp+0f, 0x1.0147AEp+0f, 0x1.F9DB22p-1f, 0x1.F0A3D8p-1f};
    for (u32 point = 0; point < 8; point++)
    {
        g_OlegCurve788.points[point] = {grow[point], grow[point]};
    }

    for (u32 point = 0; point < 8; point++)
    {
        g_OlegCurve780.points[point] = {shrink[point], shrink[point]};
    }

    const Vector4 dim = {0x1.99999Ap-4f, 0x1.2D0E56p-1f, 1.0f, 1.0f};
    for (u32 point = 0; point < 16; point++)
    {
        g_OlegColours790.points[point] = dim;
    }

    g_OlegColours790.points[5] = {0x1.333334p-2f, 0x1.9374BCp-1f, 1.0f, 1.0f};
    g_OlegColours790.points[6] = {0x1.19999Ap-1f, 0x1.D9999Ap-1f, 1.0f, 1.0f};
    g_OlegColours798.points[0] = dim;
    g_OlegColours798.points[1] = {0x1.99999Ap-4f, 0x1.333334p-1f, 1.0f, 1.0f};
    g_OlegColours798.points[2] = {0x1.99999Ap-2f, 0x1.99999Ap-1f, 1.0f, 1.0f};
    g_OlegColours798.points[3] = {0x1.19999Ap-1f, 0x1.D9999Ap-1f, 1.0f, 1.0f};
    g_OlegColours798.points[4] = {0x1.99999Ap-4f, 0x1.5C28F6p-1f, 1.0f, 1.0f};
    g_OlegColours798.points[5] = {0x1.99999Ap-4f, 0x1.99999Ap-2f, 1.0f, 1.0f};
    g_OlegColours798.points[6] = {0x1.99999Ap-4f, 0x1.666666p-2f, 1.0f, 1.0f};
    g_OlegColours798.points[7] = dim;

    Vector2 place = {0x1.E66666p-1f, 0x1.E66666p-1f};
    Vector2 scale = {0.625f, 0.625f};
    backHint.FadeIn(White(), &place, &scale);
    cancelHint.FadeIn(White(), &place, &scale);
    place = {0x1.99999Ap-5f, 0x1.E66666p-1f};
    selectHint.FadeIn(White(), &place, &scale);
    place = {0x1.99999Ap-5f, 0x1.CCCCCCp-1f};
    scale = {0.75f, 0.75f};
    pagesLeftHint.FadeIn(White(), &place, &scale);
    place = {0x1.E66666p-1f, 0x1.CCCCCCp-1f};
    pagesRightHint.FadeIn(White(), &place, &scale);
    scale = {1.0f, 1.0f};
    ring1688.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    ring171C.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);

    const s32 rotations[8] = {g_OlegAngle710, g_OlegAngle710, g_OlegAngle720, g_OlegAngle720,
                              g_OlegAngle710, g_OlegAngle710, g_OlegAngle710, g_OlegAngle710};
    for (u32 ring = 0; ring < 8; ring++)
    {
        rings[ring]->rotation = rotations[ring];
    }

    rings[0]->middleColour = g_OlegColour758;
    rings[0]->colour = g_OlegColour758;
    rings[1]->colour = g_OlegColour748;
    rings[2]->colour = g_OlegColour748;
    rings[3]->colour = g_OlegColour758;
    rings[4]->outerColours = &g_OlegColours790;
    rings[5]->colour = g_OlegColour748;
    rings[6]->colour = g_OlegColour748;
    rings[7]->colour = g_OlegColour750;
    const Vector2 radii[8] = {
        {0x1.5C28F6p-3f, 0x1.333334p-3f}, {0x1.624DD2p-3f, 0x1.395810p-3f}, {0x1.333334p-2f, 0.25f},
        {0x1.3645A2p-2f, 0x1.03126Ep-2f}, {0.375f, 0x1.666666p-2f},          {0x1.83126Ep-2f, 0x1.6978D4p-2f},
        {0x1.8A3D70p-2f, 0x1.70A3D8p-2f}, {0x1.8D4FE0p-2f, 0x1.73B646p-2f},
    };
    for (u32 ring = 0; ring < 8; ring++)
    {
        rings[ring]->outerScale = radii[ring];
    }

    SetLayer(&ring1688, 2);
    Vector2 panel = {0x1.EB851Ep-4f, 0x1.47AE14p-3f};
    AddPanelRings(&panel, &ring171C);
    u32 hidden;
    u32 shown;
    ColourSet(&hidden, 0.0f, 0.0f, 0.0f, 0.0f);
    ColourSet(&shown, 0.0f, 0.0f, 0.0f, 0.75f);
    sprite15CC.hiddenColour = hidden;
    sprite15CC.shownColour = shown;
    SetLayer(&sprite15CC, 1);
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
    oleg->text.string = nullptr;
    oleg->text.capacity = 0;
    oleg->text.length = 0;
    ButtonBindings::Construct(&oleg->bindings, 0, 7);
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
    Sprite::Construct(&oleg->sprite81C);
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
    TextLineConstruct(&oleg->textLine, font);
    SpriteWidget::Construct(&oleg->sprite1240, &sprites[0]);
    SpriteWidget::Construct(&oleg->sprite12FC, &sprites[0]);
    SpriteWidget::Construct(&oleg->sprite13B8, &sprites[0]);
    SpriteWidget::Construct(&oleg->sprite1474, &sprites[0]);
    StringLabel::Construct(&oleg->string1530, AnchorMiddle, font, Centred);
    SpriteWidget::Construct(&oleg->sprite15CC, &sprites[0]);
    RingWidget::Construct(&oleg->ring1688, AnchorMiddle, 8, 1, 0x40, nullptr);
    RingWidget::Construct(&oleg->ring171C, AnchorMiddle, 4, 1, 0x20, nullptr);
    Label::Construct(&oleg->backHint, AnchorRight, font, 0x1D, RightAligned);
    Label::Construct(&oleg->cancelHint, AnchorRight, font, 0xBB, RightAligned);
    Label::Construct(&oleg->selectHint, AnchorLeft, font, 0x1C, LeftAligned);
    Label::Construct(&oleg->pagesLeftHint, AnchorLeft, font, 0x1E, LeftAligned);
    Label::Construct(&oleg->pagesRightHint, AnchorRight, font, 0x1F, RightAligned);
    MenuWidget::Construct(&oleg->optionsMenu, AnchorMiddle, input, &oleg->drawers[6], sounds);
    RingWidget::Construct(&oleg->optionsPanel, AnchorMiddle, 4, 1, 0x20, nullptr);
    MenuWidget::Construct(&oleg->screenPositionMenu, AnchorMiddle, input, &oleg->drawers[0], sounds);
    Label::Construct(&oleg->screenPositionHint, AnchorMiddle, font, 0x26, Centred);
    MenuWidget::Construct(&oleg->mainMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    SpriteWidget::Construct(&oleg->titleLogo, AnchorMiddle, &sprites[1]);
    MenuWidget::Construct(&oleg->pauseMenu, AnchorMiddle, input, &oleg->drawers[4], sounds);
    MenuWidget::Construct(&oleg->disableAutosaveMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    Label::Construct(&oleg->disableAutosaveTitle, AnchorMiddle, font, 0x60, Centred);
    MenuWidget::Construct(&oleg->quitMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    Label::Construct(&oleg->quitTitle, AnchorMiddle, font, 0x45, Centred);
    Label::Construct(&oleg->noControllerText, AnchorMiddle, font, 0x20, Centred);
    Label::Construct(&oleg->discErrorTitle, AnchorMiddle, font, 0x21, Centred);
    Label::Construct(&oleg->discErrorText, AnchorMiddle, font, 0x22, Centred);
    Label::Construct(&oleg->autosaveOffTitle, AnchorMiddle, font, 0x24, Centred);
    Label::Construct(&oleg->autosaveOffText, AnchorMiddle, font, 0x25, Centred);
    MenuWidget::Construct(&oleg->autosaveOffMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    Label::Construct(&oleg->autosaveOnTitle, AnchorMiddle, font, 0x23, Centred);
    Label::Construct(&oleg->autosaveOnText, AnchorMiddle, font, 0x42, Centred);
    SpriteWidget::Construct(&oleg->autosaveOnIcon, AnchorMiddle, &sprites[23]);
    MenuWidget::Construct(&oleg->autosaveOnMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    Label::Construct(&oleg->autosaveFailedText, AnchorMiddle, font, 0xB3, Centred);
    MenuWidget::Construct(&oleg->autosaveFailedMenu, AnchorMiddle, input, &oleg->drawers[2], sounds);
    Label::Construct(&oleg->label2908, AnchorMiddle, font, 0x24, Centred);
    Label::Construct(&oleg->label299C, AnchorMiddle, font, 0x25, Centred);
    MenuWidget::Construct(&oleg->menu2A30, AnchorMiddle, input, &oleg->drawers[2], sounds);
    RingWidget::Construct(&oleg->wumpaRing, AnchorMiddle, 4, 1, 0x20, nullptr);
    RingWidget::Construct(&oleg->livesRing, AnchorMiddle, 4, 1, 0x20, nullptr);
    RingWidget::Construct(&oleg->crystalRing, AnchorMiddle, 4, 1, 0x20, nullptr);
    RingWidget::Construct(&oleg->completionRing, AnchorMiddle, 2, 1, 0x20, nullptr);
    RingWidget::Construct(&oleg->gemArc, AnchorMiddle, 4, 1, 0x18, nullptr);
    SpriteWidget::Construct(&oleg->levelPicture, AnchorMiddle, &sprites[2]);
    StringLabel::Construct(&oleg->completionText, AnchorMiddle, font, Centred);
    StringLabel::Construct(&oleg->wumpaText, AnchorMiddle, font, 0x60);
    SpriteWidget::Construct(&oleg->wumpaSprite, AnchorMiddle, &sprites[19]);
    StringLabel::Construct(&oleg->livesText, AnchorMiddle, font, 0x60);
    SpriteWidget::Construct(&oleg->livesSprite, AnchorMiddle, &sprites[6]);
    StringLabel::Construct(&oleg->crystalText, AnchorMiddle, font, 0x60);
    SpriteWidget::Construct(&oleg->crystalSprite, AnchorMiddle, &sprites[21]);
    for (MenuWidget& menu : oleg->levelsMenus)
    {
        MenuWidget::Construct(&menu, AnchorMiddle, input, &oleg->drawers[5], sounds);
    }

    Label::Construct(&oleg->levelName, AnchorMiddle, font, 0x47, Centred);
    RingWidget::Construct(&oleg->levelNamePanel, AnchorMiddle, 4, 1, 0x20, nullptr);
    MenuWidget::Construct(&oleg->extrasMenu, AnchorMiddle, input, &oleg->drawers[1], sounds);
    RingWidget::Construct(&oleg->extrasPanel, AnchorMiddle, 4, 1, 0x20, nullptr);
    SpriteWidget::Construct(&oleg->extrasPicture, AnchorLeft, &sprites[1]);
    Label::Construct(&oleg->nextHint, AnchorLeft, font, 0xB2, LeftAligned);
    MenuWidget::Construct(&oleg->gameOverMenu, AnchorMiddle, input, &oleg->drawers[3], sounds);
    StringLabel::Construct(&oleg->string3A4C, AnchorMiddle, font, Centred);
    StringLabel::Construct(&oleg->string3AE8, AnchorMiddle, font, Centred);
    MenuWidget::Construct(&oleg->saveChoicesMenu, AnchorMiddle, input, &oleg->drawers[7], sounds);
    StringLabel::Construct(&oleg->string3C30, AnchorMiddle, font, Centred);
    RingWidget::Construct(&oleg->saveSlotsPanel, AnchorMiddle, 4, 1, 0x20, nullptr);
    MenuWidget::Construct(&oleg->saveSlotsMenu, AnchorMiddle, input, &oleg->drawers[8], sounds);
    Label::Construct(&oleg->autosavingText, AnchorMiddle, font, 0x43, Centred);
    SpriteWidget::Construct(&oleg->autosavingIcon, AnchorMiddle, &sprites[23]);
    StringLabel::Construct(&oleg->string3F5C, AnchorRight, font, 0x30);
    SpriteWidget::Construct(&oleg->sprite3FF8, AnchorLeft, &sprites[4]);
    StringLabel::Construct(&oleg->string40B4, AnchorRight, font, 0x30);
    SpriteWidget::Construct(&oleg->sprite4150, AnchorLeft, &sprites[24]);
    GameProgress* state = oleg->progress;
    AnimatedWidget::Construct(&oleg->bar420C, AnchorLeft);
    oleg->bar420C.progress = state;
    oleg->bar420C.sprites = sprites;
    oleg->bar420C.vtable = g_HudBarWidgetVTable;
    AnimatedWidget::Construct(&oleg->slider429C, AnchorMiddle);
    oleg->slider429C.track = &sprites[12];
    oleg->slider429C.value = 0.5f;
    oleg->slider429C.knob = &sprites[19];
    oleg->slider429C.rightEnd = nullptr;
    oleg->slider429C.leftEnd = nullptr;
    oleg->slider429C.vtable = g_SliderWidgetVTable;
    StringLabel::Construct(&oleg->hudWumpaText, AnchorLeft, font, LeftAligned);
    SpriteWidget::Construct(&oleg->hudWumpaSprite, AnchorLeft, &sprites[19]);
    StringLabel::Construct(&oleg->hudLivesText, AnchorRight, font, RightAligned);
    SpriteWidget::Construct(&oleg->hudLivesSprite, AnchorRight, &sprites[6]);
    SpriteWidget::Construct(&oleg->sprite45E8, AnchorFifth, &sprites[6]);
    SpriteWidget::Construct(&oleg->sprite46A4, AnchorMiddle, &sprites[20]);
    StringLabel::Construct(&oleg->string4760, AnchorRight, font, RightAligned);
    SpriteWidget::Construct(&oleg->sprite47FC, AnchorRight, &sprites[22]);
    SpriteWidget::Construct(&oleg->sprite48B8, &sprites[0]);
    TiledPicture::Construct(&oleg->picture4974, AnchorMiddle, oleg->tiles);
    SpriteWidget::Construct(&oleg->loadingLogo, AnchorLeft, &sprites[1]);
    Label::Construct(&oleg->loadingText, AnchorMiddle, font, 0x57, Centred);

    // The sprites drawn turned (placed by their middle)
    SpritesTurned(sprites, 6, 12);
    SpritesTurned(sprites, 30, 46);
    const u32 turned[] = {1, 2, 19, 20, 21, 23, 29};
    for (u32 sprite : turned)
    {
        sprites[sprite].turned = 1;
    }

    oleg->sprite81C.turned = 1;
    oleg->crystalSprite.emitter = &oleg->emitters[0];
    oleg->unknown310 = 0;
    oleg->wumpaToAdd = 0;
    oleg->unknown312 = 0;
    oleg->wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * 0x1.333334p-3f);
    for (u32 picture = 0; picture < Pictures; picture++)
    {
        oleg->pictures[picture] = NoPicture[picture] << PictureShift | NoPicture[picture] << WantedPictureShift;
    }
    oleg->pulse.duration = 0x1.333334p-3f;
    for (u32& value : oleg->unknownB40)
    {
        value = 0;
    }

    for (u32 gem = 0; gem < 6; gem++)
    {
        oleg->gemSprites[gem] = SpriteWidget::Construct(Allocate<SpriteWidget>(),
                                                        AnchorMiddle, &sprites[13 + gem]);
    }

    oleg->SetUpScreens();
    oleg->SetUpFlatSprites();
    Vector2 place;
    Vector2 scale;
    Vector2 offset;
    place = {0x1.99999Ap-5f, 0x1.99999Ap-5f};
    scale = {1.0f, 1.0f};
    offset = {-0.5f, g_MinusZero};
    oleg->bar420C.SlideIn(White(), &place, &scale, &offset);
    scale = {1.0f, 1.0f};
    oleg->screenPositionMenu.GrowIn(White(), &g_OlegPlace630, &scale, &g_OlegPlace630);
    scale = {0.625f, 0.625f};
    oleg->screenPositionHint.GrowIn(White(), &g_OlegPlace628, &scale, &g_OlegPlace628);
    oleg->screenPositionMenu.next = &oleg->screenPositionHint;
    oleg->SetUp16A0F8();
    oleg->SetUp16A278();
    oleg->SetUp16A518();
    oleg->SetUp16A740();
    oleg->SetUp16A890();
    place = {0.5f, 0x1.99999Ap-4f};
    scale = {1.0f, 1.0f};
    oleg->gameOverMenu.FadeIn(White(), &place, &scale);
    oleg->SetUpPauseRings();
    oleg->SetUp16B718();
    oleg->SetUp16B938();
    oleg->SetUpPauseMenu();
    oleg->SetUp16C9F8();
    place = {0.5f, 0x1.333334p-3f};
    scale = {0.5f, 0x1.99999Ap-4f};
    offset = {0.0f, -0.5f};
    oleg->slider429C.SlideIn(White(), &place, &scale, &offset);
    place = {0.5f, 0x1.99999Ap-2f};
    scale = {0.75f, 0.75f};
    oleg->autosavingText.FadeIn(White(), &place, &scale);
    place = {0.5f, 0x1.B33334p-1f};
    scale = {0x1.99999Ap-4f, 0x1.99999Ap-4f};
    offset = {0.0f, 0.5f};
    oleg->autosavingIcon.SlideRectangle(White(), &place, &scale, &offset);
    oleg->autosavingIcon.next = &oleg->autosavingText;
    place = {0x1.47AE14p-3f, 0x1.851EB8p-3f};
    scale = {1.5f, 1.5f};
    offset = {-0.5f, 0.0f};
    oleg->hudWumpaText.SlideIn(White(), &place, &scale, &offset);
    place = {0x1.99999Ap-4f, 0x1.EB8520p-4f};
    scale = {0x1.C28F5Cp-4f, 0x1.333334p-3f};
    offset = {-0.5f, 0.0f};
    oleg->hudWumpaSprite.SlideRectangle(White(), &place, &scale, &offset);
    place = {0x1.9EB852p-1f, 0x1.851EB8p-3f};
    scale = {1.5f, 1.5f};
    offset = {0.5f, 0.0f};
    oleg->hudLivesText.SlideIn(White(), &place, &scale, &offset);
    place = {0x1.C28F5Cp-1f, 0x1.EB8520p-4f};
    scale = {0x1.47AE14p-3f, 0x1.333334p-3f};
    offset = {0.5f, 0.0f};
    oleg->hudLivesSprite.SlideRectangle(White(), &place, &scale, &offset);
    place = {0x1.999998p-1f, 0x1.E66666p-1f};
    scale = {1.0f, 1.0f};
    oleg->string4760.FadeIn(White(), &place, &scale);
    place = {0.875f, 0.875f};
    scale = {0x1.333334p-3f, 0x1.333334p-3f};
    oleg->sprite47FC.FadeRectangle(White(), &place, &scale);
    place = {0.5f, 0x1.99999Ap-3f};
    scale = {0x1.99999Ap-3f, 0x1.99999Ap-3f};
    offset = {0.5f, 0x1.99999Ap-3f};
    oleg->sprite46A4.GrowRectangle(White(), &place, &scale, &offset);
    place = {0x1.5C28F6p-2f, 0x1.EB851Ep-4f};
    scale = {0x1.47AE14p-3f, 0x1.333334p-3f};
    offset = {0x1.5C28F6p-2f, 0x1.EB851Ep-4f};
    oleg->sprite45E8.GrowRectangle(White(), &place, &scale, &offset);
    oleg->hudWumpaText.next = &oleg->hudWumpaSprite;
    oleg->hudLivesText.next = &oleg->hudLivesSprite;
    oleg->string4760.next = &oleg->sprite47FC;

    // The widgets' slots
    Widget* const slots[] = {
        &oleg->sprite1240,          &oleg->sprite13B8,          &oleg->sprite1474,          &oleg->string1530,
        &oleg->sprite15CC,          &oleg->ring1688,            &oleg->ring171C,            &oleg->backHint,
        &oleg->cancelHint,          &oleg->selectHint,          &oleg->pagesLeftHint,       &oleg->pagesRightHint,
        &oleg->optionsMenu,         &oleg->screenPositionMenu,        &oleg->mainMenu,            &oleg->titleLogo,
        &oleg->pauseMenu,           &oleg->disableAutosaveMenu, &oleg->quitMenu,            &oleg->completionRing,
        &oleg->noControllerText,    &oleg->discErrorTitle,      &oleg->autosaveOffTitle,    &oleg->autosaveOnTitle,
        &oleg->autosaveFailedText,  &oleg->label2908,           &oleg->levelsMenus[0],      &oleg->levelsMenus[1],
        &oleg->levelsMenus[2],      &oleg->levelsMenus[3],      &oleg->levelName,           &oleg->extrasMenu,
        &oleg->extrasPicture,       &oleg->gameOverMenu,        &oleg->string3A4C,          &oleg->string3AE8,
        &oleg->string3C30,          &oleg->autosavingIcon,      &oleg->string3F5C,          &oleg->bar420C,
        &oleg->slider429C,          &oleg->hudWumpaText,        &oleg->hudLivesText,        &oleg->sprite46A4,
        &oleg->string4760,          &oleg->sprite48B8,          &oleg->picture4974,         &oleg->loadingLogo,
    };
    // In the order the game puts them in
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
    // The pages it deletes (the others are left)
    const u32 deleted[] = {1, 2, 4, 5, 6, 3, 11, 12, 13, 14, 15, 16, 17};
    for (u32 index : deleted)
    {
        MenuPage* page = pages[index];
        if (page != nullptr)
        {
            CallVirtual<void>(page, page->vtable, PageDestroySlot, Delete);
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
            CallVirtual<void>(gem, gem->vtable, WidgetDestroySlot, Delete);
        }
    }

    // Its members the other way round, their destructors inline (a widget's the base one's)
    loadingText.Widget::Destroy(Member);
    loadingLogo.Widget::Destroy(Member);
    picture4974.Widget::Destroy(Member);
    sprite48B8.Widget::Destroy(Member);
    sprite47FC.Widget::Destroy(Member);
    DestroyStringLabel(&string4760);
    sprite46A4.Widget::Destroy(Member);
    sprite45E8.Widget::Destroy(Member);
    hudLivesSprite.Widget::Destroy(Member);
    DestroyStringLabel(&hudLivesText);
    hudWumpaSprite.Widget::Destroy(Member);
    DestroyStringLabel(&hudWumpaText);
    slider429C.Widget::Destroy(Member);
    bar420C.Widget::Destroy(Member);
    sprite4150.Widget::Destroy(Member);
    DestroyStringLabel(&string40B4);
    sprite3FF8.Widget::Destroy(Member);
    DestroyStringLabel(&string3F5C);
    autosavingIcon.Widget::Destroy(Member);
    autosavingText.Widget::Destroy(Member);
    saveSlotsMenu.Widget::Destroy(Member);
    saveSlotsPanel.Destroy(Member);
    DestroyStringLabel(&string3C30);
    saveChoicesMenu.Widget::Destroy(Member);
    DestroyStringLabel(&string3AE8);
    DestroyStringLabel(&string3A4C);
    gameOverMenu.Widget::Destroy(Member);
    nextHint.Widget::Destroy(Member);
    extrasPicture.Widget::Destroy(Member);
    extrasPanel.Destroy(Member);
    extrasMenu.Widget::Destroy(Member);
    levelNamePanel.Destroy(Member);
    levelName.Widget::Destroy(Member);
    for (u32 world = 4; world > 0; world--)
    {
        levelsMenus[world - 1].Widget::Destroy(Member);
    }

    crystalSprite.Widget::Destroy(Member);
    DestroyStringLabel(&crystalText);
    livesSprite.Widget::Destroy(Member);
    DestroyStringLabel(&livesText);
    wumpaSprite.Widget::Destroy(Member);
    DestroyStringLabel(&wumpaText);
    DestroyStringLabel(&completionText);
    levelPicture.Widget::Destroy(Member);
    gemArc.Destroy(Member);
    completionRing.Destroy(Member);
    crystalRing.Destroy(Member);
    livesRing.Destroy(Member);
    wumpaRing.Destroy(Member);
    menu2A30.Widget::Destroy(Member);
    label299C.Widget::Destroy(Member);
    label2908.Widget::Destroy(Member);
    autosaveFailedMenu.Widget::Destroy(Member);
    autosaveFailedText.Widget::Destroy(Member);
    autosaveOnMenu.Widget::Destroy(Member);
    autosaveOnIcon.Widget::Destroy(Member);
    autosaveOnText.Widget::Destroy(Member);
    autosaveOnTitle.Widget::Destroy(Member);
    autosaveOffMenu.Widget::Destroy(Member);
    autosaveOffText.Widget::Destroy(Member);
    autosaveOffTitle.Widget::Destroy(Member);
    discErrorText.Widget::Destroy(Member);
    discErrorTitle.Widget::Destroy(Member);
    noControllerText.Widget::Destroy(Member);
    quitTitle.Widget::Destroy(Member);
    quitMenu.Widget::Destroy(Member);
    disableAutosaveTitle.Widget::Destroy(Member);
    disableAutosaveMenu.Widget::Destroy(Member);
    pauseMenu.Widget::Destroy(Member);
    titleLogo.Widget::Destroy(Member);
    mainMenu.Widget::Destroy(Member);
    screenPositionHint.Widget::Destroy(Member);
    screenPositionMenu.Widget::Destroy(Member);
    optionsPanel.Destroy(Member);
    optionsMenu.Widget::Destroy(Member);
    pagesRightHint.Widget::Destroy(Member);
    pagesLeftHint.Widget::Destroy(Member);
    selectHint.Widget::Destroy(Member);
    cancelHint.Widget::Destroy(Member);
    backHint.Widget::Destroy(Member);
    ring171C.Destroy(Member);
    ring1688.Destroy(Member);
    sprite15CC.Widget::Destroy(Member);
    DestroyStringLabel(&string1530);
    sprite1474.Widget::Destroy(Member);
    sprite13B8.Widget::Destroy(Member);
    sprite12FC.Widget::Destroy(Member);
    sprite1240.Widget::Destroy(Member);
    StringDestroy(&textLine.text);
    for (u32 tile = 8; tile > 0; tile--)
    {
        Sprite& sprite = tiles[tile - 1];
        CallVirtual<void>(&sprite, sprite.vtable, ShapeDestroySlot, Element);
    }

    for (u32 index = 46; index > 0; index--)
    {
        Sprite& sprite = sprites[index - 1];
        CallVirtual<void>(&sprite, sprite.vtable, ShapeDestroySlot, Element);
    }

    sparkle.emitter.Emitter2D::Destroy(Member);
    sparkle.vtable = g_WidgetEffectVTable;
    for (u32 index = 8; index > 0; index--)
    {
        Emitter2D& emitter = emitters[index - 1];
        CallVirtual<void>(&emitter, emitter.vtable, EmitterDestroySlot, Element);
    }

    sprite81C.Shape2D::Destroy(Member);
    Platform::Graphics::DestroyMaterial(OwnMaterial(this));
    input.Destroy(Member);
    bindings.Destroy(Member);
    StringDestroy(&text);
    rock.vtable = g_WidgetEffectVTable;
    spin.vtable = g_WidgetEffectVTable;
    slide.vtable = g_WidgetEffectVTable;
    pulse.vtable = g_WidgetEffectVTable;
    WidgetController::Destroy(destroyFlags);
}

void OLEG::StartUp(PadButtons* pad, Font* font, Font* menuFont, SoundTable* frontEnd)
{
    auto* saves = static_cast<SaveManager*>(G_UnkStruct_5C0);
    pages[0] = ScreenPositionPage::Construct(Allocate<ScreenPositionPage>(), nullptr);
    pages[1] = ExtrasPage::Construct(Allocate<ExtrasPage>());
    pages[2] = MainMenuPage::Construct(Allocate<MainMenuPage>());
    pages[3] = GameOverPage::Construct(Allocate<GameOverPage>());
    pages[4] = PausePage::Construct(Allocate<PausePage>(), pad, font);
    pages[5] = DisableAutosavePage::Construct(Allocate<DisableAutosavePage>(), nullptr);
    pages[6] = QuitPage::Construct(Allocate<QuitPage>(), nullptr);
    // The notices' pages, the second one made first
    pages[8] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[7] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[9] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    pages[10] = NoticePage::Construct(Allocate<NoticePage>(), 0, nullptr);
    for (u32 world = 0; world < 4; world++)
    {
        pages[11 + world] = LevelsPage::Construct(Allocate<LevelsPage>(), world, &levelsMenus[world]);
    }

    pages[15] = OptionsPage::Construct(Allocate<OptionsPage>(), nullptr);
    pages[16] = SaveChoicesPageConstruct(MemoryAllocate(0x54), saves);
    pages[17] = SaveSlotsPageConstruct(MemoryAllocate(0x60), saves, &saveSlotsMenu);

    // Each page's menu, and whether the leave action leaves it
    MenuWidget* const menus[18] = {
        &screenPositionMenu, &extrasMenu, &mainMenu, &gameOverMenu, &pauseMenu,
        &disableAutosaveMenu, &quitMenu, &autosaveOffMenu, &autosaveOnMenu, &autosaveFailedMenu,
        &menu2A30, &levelsMenus[0], &levelsMenus[1], &levelsMenus[2], &levelsMenus[3],
        &optionsMenu, &saveChoicesMenu, &saveSlotsMenu,
    };
    const bool leaves[18] = {true, true, true, false, true, true, true, true, true,
                             true, true, true, true, true, true, true, false, false};
    for (u32 page = 0; page < 18; page++)
    {
        MenuWidget* menu = menus[page];
        menu->home = pages[page];
        menu->firstPage = pages[page];
        menu->menuFlags = leaves[page] ? menu->menuFlags | MenuWidget::Leaves : menu->menuFlags & ~MenuWidget::Leaves;
        menu->pad = pad;
    }

    extrasMenu.menuFlags |= MenuWidget::RemembersPage;
    saves->messages[0] = &string3A4C;
    saves->messages[1] = &string3AE8;
    saves->messages[2] = &string3C30;
    saves->choicesPage = pages[16];
    saves->slotsPage = pages[17];
    const s32 saveScreens[4] = {43, 28, 29, 30};
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

    // The drawers: lists (the screen's position, the main menu and the confirmations, the pause menu, the save manager's), the
    // extras' (a window of 9 to 17 items), the game over's (its title big), the levels' (big items, no pulse) and the options'
    const u32 lists[] = {0, 2, 4, 7, 8};
    for (u32 index : lists)
    {
        ListStyle(&drawers[index], menuFont);
        if (index != 0)
        {
            drawers[index].windowSize = 7;
        }
    }

    MenuDrawer& extras = drawers[1];
    extras.titlePlace = {0.0f, 0.0f};
    extras.titleScale = {0.75f, 0.75f};
    extras.titleFlags = Centred;
    extras.itemFlags = Centred;
    extras.windowStart = 9;
    extras.windowSize = 0x11;
    extras.itemsPlace = {0.0f, 0x1.74BC6Ap-2f};
    extras.fewItemsScale = {0.75f, 0.75f};
    extras.fewItemsSpacing = 0x1.99999Ap-5f;
    extras.manyItemsScale = {0.5f, 0.5f};
    extras.manyItemsSpacing = 0x1.1EB852p-5f;
    SetItemStyles(&extras, menuFont);
    extras.sizePulse = ScalerPulse();

    MenuDrawer& gameOver = drawers[3];
    gameOver.titlePlace = {0.0f, 0.0f};
    gameOver.titleScale = {1.5f, 1.5f};
    gameOver.titleFlags = 3;
    gameOver.itemFlags = 3;
    gameOver.itemsPlace = {0.0f, 0x1.333334p-3f};
    gameOver.manyItemsScale = {0.75f, 0.75f};
    gameOver.manyItemsSpacing = 0x1.99999Ap-5f;
    SetItemStyles(&gameOver, menuFont);
    gameOver.sizePulse = ScalerPulse();

    MenuDrawer& options = drawers[6];
    options.titlePlace = {0.0f, 0.0f};
    options.titleScale = {0.75f, 0.75f};
    options.titleFlags = Centred;
    options.itemFlags = Centred;
    options.itemsPlace = {0.0f, 0x1.418938p-2f};
    options.manyItemsScale = {0.625f, 0.75f};
    options.manyItemsSpacing = 0x1.99999Ap-5f;
    SetItemStyles(&options, menuFont);
    options.sizePulse = ScalerPulse();

    MenuDrawer& levels = drawers[5];
    levels.titlePlace = {0.0f, 0.0f};
    levels.titleScale = {0.0f, 0.0f};
    levels.titleFlags = 3;
    levels.itemFlags = 3;
    levels.itemsPlace = {0.0f, 0.0f};
    levels.manyItemsScale = {1.0f, 1.0f};
    levels.manyItemsSpacing = 0x1.333334p-3f;
    SetItemStyles(&levels, menuFont);

    // The front end's sounds: the first when an item is switched on, the second when the selection or a value moves or an item is
    // picked, the third going back
    GameSound** frontEndSounds = frontEnd->sounds;
    const u32 moves[] = {1, 3, 5, 7, 9};
    for (u32 sound : moves)
    {
        sounds.sounds[sound] = frontEndSounds[1];
    }

    sounds.sounds[10] = frontEndSounds[0];
    sounds.sounds[11] = frontEndSounds[2];
    sounds.sounds[12] = frontEndSounds[2];
}

void OLEG::LoadPicture(u32 picture, u32 now)
{
    u32 read = pictures[picture] >> PictureShift & PictureMask;
    u32 wanted = pictures[picture] >> WantedPictureShift & PictureMask;
    if (read == wanted)
    {
        return;
    }

    GameReadersStorage* storage = g_ReadersStorages[PictureReaders];
    String path;
    path.string = nullptr;
    path.length = 0;
    path.capacity = 0;
    const char separator[2] = {'\\', '\0'};
    if (picture == 0)
    {
        StringAssign(&path, g_TitlesFolder);
        StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        StringAppend(&path, g_CrashTitle);
    }
    else if (picture == 1)
    {
        StringAssign(&path, g_TitlesFolder);
        StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        StringAppend(&path, separator);
        StringAppend(&path, g_LevelTitles[wanted]);
    }
    else if (picture == 2)
    {
        if (wanted == 0)
        {
            StringAssign(&path, g_LanguageFolder);
            StringAppend(&path, g_ScreenPictures[0]);
            StringAppend(&path, separator);
            StringAppend(&path, g_LanguageNames[g_CurrentLanguage]);
        }
        else if (wanted >= FirstNamedPicture && wanted < EndNamedPictures)
        {
            StringAssign(&path, text.string);
        }
        else
        {
            StringAssign(&path, g_LanguageFolder);
            StringAppend(&path, g_ScreenPictures[wanted]);
        }
    }

    StringAppend(&path, g_PictureExtension);
    ReleasePicture(picture);
    u32 bits = pictures[picture];
    pictures[picture] = (bits & ~(PictureMask << PictureShift)) | (bits >> 8 & PictureMask << PictureShift) | PictureBeingRead;
    auto* reader = Allocate<PictureReader>();
    reader->vtable = g_PictureReaderVTable;
    reader->picture = picture;
    reader->oleg = this;
    SubItemsReader* file = SubItemsReader::ConstructFile(Allocate<SubItemsReader>(),
                                                         path.string, reader, PictureFileFlags);
    AddItemReaderToReaderStorage(storage, file, 0);
    if (now != 0)
    {
        LoadQueuedSectionsIntoMemory_();
    }

    StringDestroy(&path);
}

void OLEG::ReleasePicture(u32 picture)
{
    if (picture >= Pictures || (pictures[picture] >> PictureShift & PictureMask) == NoPicture[picture])
    {
        return;
    }

    if (picture == 2)
    {
        for (Sprite& tile : tiles)
        {
            CallVirtual<void>(&tile, tile.vtable, ReleaseResourceSlot);
        }
    }
    else
    {
        Sprite& sprite = sprites[picture + 1];
        CallVirtual<void>(&sprite, sprite.vtable, ReleaseResourceSlot);
    }

    pictures[picture] = (pictures[picture] & ~(PictureMask << PictureShift)) | NoPicture[picture] << PictureShift;
}

void OLEG::Reset()
{
    StringAssign(&textLine.text, "");
    wumpaToAdd = 0;
    wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * 0x1.333334p-3f);
    for (u32 sprite = 3; sprite < FlatSprites; sprite++)
    {
        CallVirtual<void>(&sprites[sprite], sprites[sprite].vtable, SpriteSetMaterialSlot, Platform::Graphics::FlatMaterial());
    }
}

void OLEG::Draw(Renderer* renderer)
{
    WidgetController::Draw(renderer);
    TextLineDraw(&textLine, renderer);
}

void OLEG::PlayPickupEffect(u32 screen)
{
    // The gems' array constructed, then the colours set the first time
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

    if (screen == LivesScreen)
    {
        rock.WidgetEffect::Reset();
        rock.duration = 0x1.CCCCCCp-2f;
        rock.repeats = 7;
        SetEffect(masks[LivesScreen], &rock);
    }
    else if (screen == PickupScreen)
    {
        sparkle.SparkleEffect::Reset();
        sparkle.repeats = 1;
        sparkle.emitter.colour = unknown310 == CrystalSprite ? g_CrystalColour : g_GemColours[unknown312 & 0xF];
        SetEffect(masks[screen], &sparkle);
    }
}

void OLEG::UpdateHud(PlayerCharacter* character)
{
    CharacterCounter* counter = character != nullptr ? character->counter : nullptr;
    CharacterControl* control = character != nullptr ? character->control : nullptr;
    bool riding = false;
    GameProgress* game = progress;
    u32 mode = game->bits & ModeMask;
    u32 lives = game->counts >> GameProgress::LivesShift & GameProgress::LivesMask;
    bool health = mode == ModeHealth;
    bool timed = mode == ModeTimed;
    if (control != nullptr)
    {
        riding = control->Kind() == SliderVehicle;
    }

    String livesText = {};
    String wumpaText = {};
    String crystalsText = {};
    AppendNumber(&wumpaText, game->counts & GameProgress::WumpaMask);
    u32 crystals = 0;
    for (u32 level = 0; level < 16; level++)
    {
        crystals += reinterpret_cast<const u8*>(&game->levels[level])[1] & 1;
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
        u32 level = page->ItemAgain(page->selections[0])->Id();
        u32 area = progress->bits >> 26 & 0x1F;
        levelName.text = area < level ? UnknownLevelText : static_cast<u32>(g_AreaLevels[level]) + LevelNameTexts;
    }

    if (timed)
    {
        s32 seconds = static_cast<s32>(static_cast<f32>(progress->timeLeft) * g_SecondsPerClockUnit);
        s32 minutes = seconds / 60;
        seconds -= minutes * 60;
        String counted = {};
        String time = {};
        String number;
        StringConstructNumber(&number, progress->counts >> 26);
        StringAssign(&counted, number.string);
        StringDestroy(&number);
        StringAppend(&counted, "/");
        String total;
        StringConstructNumber(&total, progress->counts >> 20 & 0x3F);
        StringAppend(&counted, total.string);
        StringDestroy(&total);
        StringConstructInteger(&number, minutes);
        StringAssign(&time, number.string);
        StringDestroy(&number);
        StringAppend(&time, seconds < 10 ? ":0" : ":");
        StringConstructInteger(&number, seconds);
        StringAppend(&time, number.string);
        StringDestroy(&number);
        StringAssign(&string3F5C.text, counted.string);
        StringAssign(&string40B4.text, time.string);
        StringDestroy(&time);
        StringDestroy(&counted);
    }

    SetVisible(&string3F5C, timed);
    SetVisible(&sprite3FF8, timed && unknownB40[1] != 0);
    SetVisible(&string40B4, timed);
    SetVisible(&sprite4150, timed);
    if (!health)
    {
        StringAssign(&hudWumpaText.text, wumpaText.string);
    }

    SetVisible(&bar420C, health);
    SetVisible(&hudWumpaText, !health);
    if (riding)
    {
        f32 gauge = VehicleGauge(control);
        slider429C.leftEnd = &sprites[5];
        slider429C.rightEnd = &sprites[6];
        slider429C.value = gauge - 0.5f;
    }

    SetVisible(&slider429C, riding);
    StringAssign(&this->wumpaText.text, wumpaText.string);
    StringAssign(&crystalText.text, crystalsText.string);
    sprite46A4.sprite = &sprites[unknown310];
    AppendNumber(&livesText, lives < 2 ? 0 : lives - 1);
    if (hudLivesSprite.State() >= Widget::StateAppearing)
    {
        StringAssign(&hudLivesText.text, livesText.string);
        hudLivesSprite.sprite = &sprites[FirstIcon + (progress->bits >> 8 & 0xF)];
    }

    if (livesSprite.State() >= Widget::StateAppearing)
    {
        StringAssign(&this->livesText.text, livesText.string);
        livesSprite.sprite = &sprites[FirstIcon + (progress->bits >> 8 & 0xF)];
        livesSprite.emitter = (progress->bits >> 8 & 0xF) == CrashCharacter ? &emitters[CrashLivesEmitter] : nullptr;
    }

    if (counter != nullptr)
    {
        String count = {};
        AppendNumber(&count, counter->Count());
        StringAssign(&string4760.text, count.string);
        StringDestroy(&count);
    }

    bool counted = counter != nullptr && !health;
    SetVisible(&sprite47FC, counted);
    SetVisible(&string4760, counted);
    if (autosavingIcon.State() >= Widget::StateAppearing)
    {
        if (savingShown < 2)
        {
            SetVisible(&autosavingText, true);
            autosavingIcon.scaler = reinterpret_cast<Widget::Scaler*>(&g_OlegScaler);
        }
        else
        {
            SetVisible(&autosavingText, false);
            autosavingIcon.scaler = nullptr;
        }
    }

    // How much of the game is done: the story's share where it has got to, and a third of the gems found
    GameProgress* done = progress;
    u32 level = done->bits >> 16 & 0x1F;
    const u32* levelGems = &done->levels[g_AreaLevels[level]];
    String completion = {};
    u32 gems = 0;
    for (u32 gem = 0; gem < Gems; gem++)
    {
        u32 found = 0;
        for (u32 other = 0; other < 16; other++)
        {
            if ((static_cast<u8>(done->levels[other]) & 1u << gem) != 0)
            {
                found++;
            }
        }

        gems += found;
    }

    String number;
    StringConstructNumber(&number, g_StoryDone[done->bits >> 21 & 0x1F] + gems / 3);
    StringAssign(&completion, number.string);
    StringDestroy(&number);
    StringAppend(&completion, "%");
    StringAssign(&completionText.text, completion.string);

    // The level's picture once it's read (a hub's square), else it's asked for
    u32 picture = pictures[LevelPicture];
    if ((picture & PictureBeingRead) == 0 && (picture >> PictureShift & PictureMask) == level)
    {
        bool hub = level == 0 || level == 6 || level == 13 || level == 20;
        Vector2 scale = {hub ? Rounded(0.4) : Rounded(0.6), Rounded(0.4)};
        Vector2 place = {Rounded(0.45), 0x1.74BC6Ap-2f};
        Vector2 from = {0.5f, 0.5f};
        levelPicture.GrowRectangle(White(), &place, &scale, &from);
        SetShadow(&levelPicture);
        SetVisible(&levelPicture, true);
        levelPicture.pulseAmount = Rounded(0.03);
        levelPicture.pulsePeriod = Rounded(0.6);
    }
    else
    {
        if ((pictures[LevelPicture] & PictureBeingRead) == 0)
        {
            pictures[LevelPicture] = (pictures[LevelPicture] & ~(PictureMask << WantedPictureShift)) | level << WantedPictureShift;
            LoadPicture(LevelPicture, 0);
        }

        SetVisible(&levelPicture, false);
    }

    for (u32 gem = 0; gem < Gems; gem++)
    {
        bool found = (*reinterpret_cast<const u8*>(levelGems) & 1u << gem) != 0;
        gemSprites[gem]->sprite = found ? &sprites[GemSprites + gem] : &sprites[EmptyGemSlot];
        gemSprites[gem]->emitter = found ? &emitters[FirstGemEmitter + gem] : nullptr;
    }

    StringDestroy(&completion);
    StringDestroy(&crystalsText);
    StringDestroy(&wumpaText);
    StringDestroy(&livesText);
}

void OLEG::Update(TimeClock* clock, GamePad* pad, PlayerCharacter* character)
{
    g_OlegScaler.Step(clock);
    if (g_GameState == GameStatePaused)
    {
        s32 duration = static_cast<s32>(g_ClockUnitsPerSecond * 0x1.99999Ap-4f);
        wumpaDelay = static_cast<s32>(g_ClockUnitsPerSecond * 0x1.333334p-3f);
        Hide(masks[WumpaScreen], duration, 0);
        sprite45E8.Disappear(static_cast<s32>(g_ClockUnitsPerSecond * 0x1.99999Ap-4f), 0);
    }
    else if (wumpaToAdd != 0)
    {
        if (static_cast<s32>(clock->advance) < wumpaDelay)
        {
            wumpaDelay -= clock->advance;
        }
        else
        {
            u32 wumpa = (progress->counts & GameProgress::WumpaMask) + 1;
            if (wumpa >= 100)
            {
                wumpa -= 100;
                AddLives(1, 1);
            }

            progress->counts = (progress->counts & ~GameProgress::WumpaMask) | (wumpa & GameProgress::WumpaMask);
            // The more there are to add, the sooner the next: 0.15 seconds times sin(1 / how many)
            f32 delay = 0x1.333334p-3f;
            if (wumpaToAdd >= 2)
            {
                f32 values[4];
                Platform::Math::SinCos(1.0f / static_cast<f32>(static_cast<s32>(wumpaToAdd)), 0.0f, values);
                delay = 0x1.333334p-3f * values[0];
            }

            wumpaToAdd--;
            pulse.duration = delay;
            wumpaDelay = static_cast<s32>(delay * g_ClockUnitsPerSecond);
            pulse.repeats++;
            Show(masks[WumpaScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0x1.99999Ap-4f),
                 static_cast<s32>(g_ClockUnitsPerSecond * 1.5f));
            SetEffect(masks[WumpaScreen], &pulse);
            if (wumpaToAdd == 0 && pulse.repeats >= 2)
            {
                pulse.repeats = 1;
            }
        }
    }

    UpdateHud(character);
    WidgetController::Update(clock);
    textLine.label = &string1530;
    TextLineUpdate(&textLine, clock);
}

void OLEG::AddLives(s32 lives, u32 celebrated)
{
    u32 counts = progress->counts;
    s32 total = static_cast<s32>(counts >> GameProgress::LivesShift & GameProgress::LivesMask) + lives;
    counts &= ~(GameProgress::LivesMask << GameProgress::LivesShift);
    if (total >= 100)
    {
        counts |= 100 << GameProgress::LivesShift;
    }
    else if (total >= 0)
    {
        counts |= (total & GameProgress::LivesMask) << GameProgress::LivesShift;
    }

    progress->counts = counts;
    Show(masks[LivesScreen], static_cast<s32>(g_ClockUnitsPerSecond), static_cast<s32>(g_ClockUnitsPerSecond * 1.5f));
    rock.WidgetEffect::Reset();
    rock.duration = 0x1.CCCCCCp-2f;
    rock.repeats = 7;
    hudLivesSprite.SetEffect(&rock);
    if (celebrated == 0)
    {
        return;
    }

    sprite45E8.sprite = &sprites[CharacterHeads + (progress->bits >> 8 & 0xF)];
    sprite45E8.Appear(static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), static_cast<s32>(g_ClockUnitsPerSecond));
    slide.WidgetEffect::Reset();
    slide.duration = 0.75f;
    slide.repeats++;
    spin.WidgetEffect::Reset();
    spin.duration = 0.75f;
    spin.repeats++;
    slide.next = &spin;
    sprite45E8.SetEffect(&slide);
    PlaySoundById(0x1.99999Ap-3f, -1.0f, ExtraLifeSound, 0, 0, -1);
}

u32 CyclingScale::Step(TimeClock* clock)
{
    if ((flags & Running) == 0)
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
                u16 last = flags;
                scale = values[(last & CountMask) - 1];
                if ((last & Running) != 0)
                {
                    flags = last & ~Running;
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
        f32 at = static_cast<f32>(static_cast<s32>((flags & CountMask) - 1)) * fraction;
        s32 index = static_cast<s32>(at);
        f32 first = values[index];
        scale = first + (values[index + 1] - first) * (at - static_cast<f32>(index));
    }
    else
    {
        scale = values[(flags & CountMask) - 1];
    }

    return 1;
}

void OLEG::ReadPicture(u32 picture, Stream* stream)
{
    pictures[picture] &= ~PictureBeingRead;
    if (picture == 2)
    {
        for (Sprite& tile : tiles)
        {
            CallVirtual<void>(&tile, tile.vtable, ShapeReadSlot, stream);
        }

        return;
    }

    if (picture < 2)
    {
        Sprite& sprite = sprites[picture + 1];
        CallVirtual<void>(&sprite, sprite.vtable, ShapeReadSlot, stream);
    }
}

void PictureReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PictureReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, 0x40);
    oleg->ReadPicture(picture, &stream);
    stream.Destroy(2);
}

void PictureReader::Missing(u8*, u32, ReaderStack*)
{
}

void HudBarWidget::Draw(Renderer* renderer)
{
    u32 health = progress->counts >> GameProgress::HealthShift & GameProgress::HealthMask;
    u32 most = progress->counts >> GameProgress::MostHealthShift & GameProgress::HealthMask;
    if ((flags & Invisible) == 0 && State() >= StateAppearing && health != 0)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        f32 length = progress->barLength * 0x1.333334p-3f;
        u32 layer = (flags & LayerMask) >> LayerShift;
        FitPlace(0, &at);
        FitSize(0, &size);
        renderer->colour = drawColour;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        f32 width = length * size.x;
        f32 height = size.y * 0x1.99999Ap-3f;
        f32 pieceWidth = size.x * 0x1.EB851Ep-6f;
        f32 pieceHeight = size.y * 0x1.47AE14p-5f;
        matrix.m[3][0] = at.x;
        matrix.m[1][1] = height;
        matrix.m[3][1] = at.y;
        matrix.m[0][0] = width;
        QueuePlacedShape(renderer, &matrix, &sprites[BarBody], layer);
        matrix.m[0][0] = pieceWidth;
        matrix.m[1][1] = pieceHeight;
        matrix.m[3][0] = at.x + width;
        matrix.m[3][1] = at.y + pieceHeight;
        QueuePlacedShape(renderer, &matrix, &sprites[BarStart], layer);
        matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
        QueuePlacedShape(renderer, &matrix, &sprites[BarGap], layer);
        for (u32 piece = 2;; piece++)
        {
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            if (health < piece || piece >= most)
            {
                break;
            }

            QueuePlacedShape(renderer, &matrix, &sprites[BarPiece], layer);
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            QueuePlacedShape(renderer, &matrix, &sprites[BarGap], layer);
        }

        if (health == most && most >= 2)
        {
            QueuePlacedShape(renderer, &matrix, &sprites[BarPiece], layer);
            matrix.m[3][0] = matrix.m[3][0] + pieceWidth;
            QueuePlacedShape(renderer, &matrix, &sprites[BarEnd], layer);
        }
    }

    Widget::Draw(renderer);
}

void SliderWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        u32 layer = (flags & LayerMask) >> LayerShift;
        FitPlace(0, &at);
        FitSize(0, &size);
        renderer->colour = drawColour;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        f32 trackHeight = size.y * 0x1.99999Ap-4f;
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

void LevelWidget::Unknown11()
{
    bob = 0.0f;
    if (IsSelected(0) != 0)
    {
        OLEG* oleg = &G_GameController_00309950->oleg;
        if (g_SparklingLevel != this)
        {
            g_SparklingLevel = this;
            g_LevelBobPhase = 0.0f;
            for (u32 gem = 0; gem < Gems; gem++)
            {
                oleg->emitters[gem].particles.Clear();
            }
        }

        f32 phase = g_LevelBobPhase;
        f32 values[4];
        Platform::Math::SinCos(phase, 0.0f, values);
        phase = phase + 0x1.20E2C4p-4f;
        bob = values[0] * 0x1.47AE14p-8f;
        g_LevelBobPhase = phase;
        if (phase >= 0x1.921FB6p+1f)
        {
            g_LevelBobPhase = phase - 0x1.921FB6p+1f;
        }

        const u32 found = controller->progress.levels[g_AreaLevels[item->Id()]];
        Vector2 half;
        CopyVector2(&half, &scale);
        FitSize(0, &half);
        f32 spacing = half.x * 0x1.C71C72p-4f;
        half = {half.x * 0.5f, half.y * 0.5f};
        for (u32 gem = 0; gem < Gems; gem++)
        {
            if ((found & 1 << gem) != 0)
            {
                Vector2 start = {0.0f, half.y};
                Vector2 end = {spacing, 0.0f};
                Emitter2DStep(&oleg->emitters[gem], 0x1.111112p-6f, &start, &end);
            }
        }
    }

    Widget::Unknown11();
}

void LevelWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        OLEG* oleg = &controller->oleg;
        u32 area = item->Id();
        s32 level = g_AreaLevels[area];
        u32 open = controller->progress.bits >> 26 & 0x1F;
        u32 title = area <= open ? LevelTitles + level : LockedLevelTitle;
        const u32* found = &controller->progress.levels[level];
        u32 layer = (flags & LayerMask) >> LayerShift;
        FitPlace(0, &at);
        FitSize(0, &size);
        f32 spacing = size.x * 0x1.C71C72p-4f;
        Vector2 titleSize;
        if (IsSelected(0) != 0)
        {
            titleSize = {size.x * g_OlegScaler.scale * 0x1.99999Ap-3f, size.y * g_OlegScaler.scale * 0x1.99999Ap+0f};
        }
        else
        {
            ColourScale(&drawColour, 0.5f);
            titleSize = {size.x * 0x1.99999Ap-3f, size.y * 0x1.99999Ap+0f};
        }

        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        matrix.m[0][0] = titleSize.x;
        matrix.m[1][1] = titleSize.y;
        matrix.m[3][0] = at.x - size.x * 0x1.666666p-2f;
        matrix.m[3][1] = at.y;
        renderer->colour = drawColour;
        QueuePlacedShape(renderer, &matrix, &oleg->sprites[title], layer);
        matrix.m[0][0] = spacing;
        matrix.m[1][1] = size.y;
        matrix.m[3][1] = at.y - size.y * 0.5f;
        matrix.m[3][0] = at.x - (size.x - spacing * 5.0f) * 0.5f;
        Matrix4x4 placed;
        InitIdentityMatrix(&placed);
        for (u32 gem = 0; gem < Gems; gem++)
        {
            u32 sprite = (static_cast<u8>(*found) & 1 << gem) != 0 ? GemSprites + gem : EmptyGemSlot;
            QueuePlacedShape(renderer, &matrix, &oleg->sprites[sprite], layer);
            if (IsSelected(0) != 0 && sprite != EmptyGemSlot)
            {
                placed.m[3][0] = matrix.m[3][0] + spacing * 0.5f;
                placed.m[3][1] = at.y;
                Emitter2D& emitter = oleg->emitters[gem];
                CallVirtual<void>(&emitter, emitter.vtable, EmitterDrawSlot, static_cast<const Matrix4x4*>(&placed));
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
    if ((flags & Invisible) != 0 || State() < StateAppearing)
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
    u32 layer = (flags & LayerMask) >> LayerShift;
    FitPlace(0, &at);
    FitSize(0, &size);
    if ((summary->flags & SaveSummary::HasSave) != SaveSummary::HasSave)
    {
        const char* empty = GameText(EmptySlotText);
        if (IsSelected(0) != 0)
        {
            size = {size.x * g_OlegScaler.scale, size.y * g_OlegScaler.scale};
        }
        else
        {
            ColourScale(&drawColour, item->IsShownTo(0) ? 0.75f : 0.5f);
        }

        renderer->font = font;
        renderer->colour = drawColour;
        renderer->textScale = size;
        renderer->textFlags = Centred;
        QueueText(renderer, empty, at.x, at.y);
        Widget::Draw(renderer);
        return;
    }

    OLEG* oleg = &controller->oleg;
    u32 bits = summary->progress;
    s32 level = g_AreaLevels[bits & SaveSummary::AreaMask];
    u32 character = bits >> SaveSummary::CharacterShift & SaveSummary::CharacterMask;
    u32 head = character != 6 ? CharacterHeads + character : 6;
    s32 time = summary->time;
    String crystals;
    StringConstruct(&crystals, " ");
    String name;
    StringConstruct(&name, GameText(LevelNames + level));
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
        pictureSize = {pictureSize.x * 0x1.70A3D8p-4f * g_OlegScaler.scale, pictureSize.y * 0x1.EB851Ep-4f * g_OlegScaler.scale};
    }
    else
    {
        ColourScale(&drawColour, 0.75f);
        pictureSize = {pictureSize.x * 0x1.70A3D8p-4f, pictureSize.y * 0x1.EB851Ep-4f};
    }

    half = {half.x * 0.5f, half.y * 0.5f};
    renderer->colour = drawColour;
    renderer->font = font;
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    s32 minutes = static_cast<s32>(static_cast<f32>(time) * g_SecondsPerClockUnit * 0x1.111112p-6f);
    s32 hours = minutes / 60;
    minutes = minutes - hours * 60;
    String number;
    StringConstructNumber(&number, summary->progress >> SaveSummary::CrystalsShift & SaveSummary::CrystalsMask);
    StringAppend(&crystals, number.string);
    StringDestroy(&number);
    StringConstructNumber(&number, (summary->progress >> SaveSummary::LivesShift & SaveSummary::LivesMask) - 1);
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
    StringConstructNumber(&number, summary->progress >> SaveSummary::DoneShift & SaveSummary::DoneMask);
    StringAppend(&done, number.string);
    StringDestroy(&number);
    StringAppend(&done, "%");

    matrix.m[0][0] = pictureSize.x;
    matrix.m[1][1] = pictureSize.y;
    matrix.m[3][1] = at.y - size.y * 0x1.47AE14p-5f;
    matrix.m[3][0] = at.x - size.x * 0x1.0A3D70p-3f;
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[LevelTitles + level], layer);
    renderer->textFlags = 6;
    renderer->textScale = half;
    QueueText(renderer, name.string, at.x, at.y);
    renderer->textFlags = RightAligned;
    renderer->textScale = {size.x * 0.5f, size.y * 0.5f};
    matrix.m[3][0] = at.x;
    matrix.m[3][1] = at.y + size.y * 0x1.99999Ap-5f;
    QueueText(renderer, played.string, at.x, matrix.m[3][1]);
    matrix.m[3][0] = at.x + size.x * 0x1.99999Ap-4f;
    QueueText(renderer, done.string, matrix.m[3][0], matrix.m[3][1]);
    matrix.m[0][0] = size.x * 0x1.333334p-4f;
    matrix.m[1][1] = size.y * 0x1.333334p-4f;
    matrix.m[3][1] = at.y - size.y * 0x1.99999Ap-5f;
    matrix.m[3][0] = at.x + size.x * 0x1.333334p-3f;
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[head], layer);
    renderer->textFlags = 0x11;
    renderer->textScale = {size.x * 0.625f, size.y * 0.625f};
    QueueText(renderer, lives.string, matrix.m[3][0], matrix.m[3][1]);
    matrix.m[0][0] = size.x * 0x1.47AE14p-5f;
    matrix.m[1][1] = size.y * 0x1.333334p-4f;
    matrix.m[3][1] = at.y + size.y * 0x1.99999Ap-5f;
    matrix.m[3][0] = at.x + size.x * 0.125f;
    QueuePlacedShape(renderer, &matrix, &oleg->sprites[CrystalSpriteIndex], layer);
    renderer->textFlags = 0x30;
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
        &menu2A30,
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
