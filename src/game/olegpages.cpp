#include "game/olegpages.h"

#include "game/clock.h"
#include "game/colour.h"
#include "game/controllers.h"
#include "game/gamecontroller.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/oleg.h"
#include "game/pads.h"
#include "game/renderer.h"
#include "game/savemanager.h"
#include "game/sound.h"
#include "game/widgets.h"

extern "C"
{
    // The header's constants the start-up sets for this file, which nothing reads: up (0, 1, 0, 1), 0.01 twice, a black of half
    // alpha and 45 degrees (65536ths); and its own: (0, 1, -1, 1), (0, 1, 0, 1), 0 and the colour table's 15 and 8
    extern Vector4 g_OlegPagesUp RETAIL(D_0030BDE0);
    extern f32 g_OlegPagesSmall RETAIL(D_0030A5D8);
    extern f32 g_OlegPagesSmall2 RETAIL(D_0030A5DC);
    extern u32 g_OlegPagesShade RETAIL(D_0030A5E0);
    extern s32 g_OlegPagesAngle45 RETAIL(D_0030A5E8);
    extern Vector4 g_OlegPagesUpBack RETAIL(D_0030BDF0);
    extern Vector4 g_OlegPagesUp2 RETAIL(D_0030BE00);
    extern u32 g_OlegPagesZero RETAIL(D_0030A5F0);
    extern u32 g_OlegPagesWhite RETAIL(D_0030A5F8);
    extern u32 g_OlegPagesColour8 RETAIL(D_0030A5FC);
}

namespace
{
// Memory for an object of a type
template <typename T> T* Allocate()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}

// The main menu's title and its items' texts
constexpr const char* MainMenuTitle = "crash twinsanity";
constexpr u32 NewGameText = 9;
constexpr u32 LoadGameText = 0xA;
constexpr u32 OptionsText = 0xB;
// Its items' IDs
constexpr u32 NewGameItem = 0;
constexpr u32 LoadGameItem = 2;
constexpr u32 OptionsItem = 0x1E;
// The pages are the first player's
constexpr u32 FirstPlayer = 1;
// OLEG's screens: every screen but the first three, the options, the screen's position, the disable autosave and the quit screens
constexpr u32 AllButFirstScreens = 0x2B;
constexpr u32 OptionsScreen = 9;
constexpr u32 ScreenPositionScreen = 10;
constexpr u32 DisableAutosaveScreen = 13;
constexpr u32 QuitScreen = 14;
// A destructor's flags: a member destroyed
constexpr u32 Member = 2;
// "continue", the ID of the items going back to a page's parent, and how far a frame moves the screen
constexpr u32 ContinueText = 5;
constexpr u32 ParentBackItem = 0x1D;
constexpr f32 ScreenStepX = Rounded(0.015);
constexpr f32 ScreenStepY = Rounded(0.03);

// The game over page: its title, "continue" (restarting from the save) and "quit game"
constexpr u32 GameOverTitle = 0x44;
constexpr u32 RestartItemId = 9;
constexpr u32 RestartEntry = 2;
constexpr u32 QuitGameText = 0x1A;
constexpr u32 QuitGameItem = 0xB;
// The confirmations: their titles, "yes" and "no" (its ID 0x1C), the quit's yes (its ID 0xB) and the disable autosave's (0xA)
constexpr u32 DisableAutosaveTitle = 0x60;
constexpr u32 QuitTitle = 0x45;
constexpr u32 YesText = 3;
constexpr u32 NoText = 2;
constexpr u32 NoItem = 0x1C;
constexpr u32 QuitYesItem = 0xB;
constexpr u32 DisableAutosaveYesItem = 0xA;
// The notices' "continue" (its ID 0xC)
constexpr u32 NoticeContinueItem = 0xC;
// The pause menu's and the main menu's screens
constexpr u32 PauseMenuScreen = 12;
constexpr u32 MainMenuScreen = 11;
// The options: the pages' titles, their links (IDs 6, 7, 8) and back (0x1C on the sub-pages)
constexpr u32 GraphicOptionsText = 0xD;
constexpr u32 SoundOptionsText = 0xE;
constexpr u32 GameOptionsText = 0xF;
constexpr u32 GraphicOptionsItem = 6;
constexpr u32 SoundOptionsItem = 7;
constexpr u32 GameOptionsItem = 8;
constexpr u32 BackText = 4;
constexpr u32 SubPageBackItem = 0x1C;
// Their items: vibration (no and yes), centring the screen, widescreen (off and on), the volumes (0 to 10), the output type and
// its choices (mono, stereo, Dolby Pro Logic II)
constexpr u32 VibrationText = 0x15;
constexpr u32 VibrationItem = 0x1A;
constexpr u32 CentreScreenText = 0x11;
constexpr u32 CentreScreenItem = 0x15;
constexpr u32 WidescreenText = 0x10;
constexpr u32 WidescreenItem = 0x16;
constexpr u32 OffText = 0;
constexpr u32 OnText = 1;
constexpr u32 EffectsVolumeText = 0x12;
constexpr u32 EffectsVolumeItem = 0x17;
constexpr u32 MusicVolumeText = 0x13;
constexpr u32 MusicVolumeItem = 0x18;
constexpr s32 MaxVolume = 10;
constexpr u32 OutputTypeText = 0x14;
constexpr u32 OutputTypeItem = 0x19;
constexpr u32 OutputTypes = 3;
constexpr u32 MonoText = 6;
// The pause menu: its title, its pages' (save game, load game), its items' texts and IDs
constexpr u32 PauseTitle = 0x18;
constexpr u32 SaveGameText = 0x19;
constexpr u32 SaveGameItem = 1;
constexpr u32 DisableAutosaveText = 0x16;
constexpr u32 DisableAutosaveItem = 0xA;
constexpr u32 ResumeText = 0x1B;
constexpr u32 ResumeItem = 0xC;
// The extras: their title, the gem extras' titles (the gem's after the first), links (IDs 0xE on) and items (16 each, their IDs
// their indexes), the complete game's movie item and its texts before and once the game's done, and resume's ID
constexpr u32 ExtrasTitle = 0x17;
constexpr u32 GemExtrasTitle = 0x59;
constexpr u32 GemExtrasLink = 0xE;
constexpr u32 GemExtrasItems = 16;
constexpr u32 Gems = 6;
constexpr u32 BlueGem = 0;
constexpr u32 ClearGem = 1;
constexpr u32 GreenGem = 2;
constexpr u32 PurpleGem = 3;
constexpr u32 RedGem = 4;
constexpr u32 YellowGem = 5;
constexpr u32 CompleteItem = 0x14;
constexpr u32 CompleteText = 0x61;
constexpr u32 NotCompleteText = 0x58;
constexpr u32 CompleteMovie = 18;
constexpr u32 ExtrasResumeItem = 0x1C;
// The galleries' first texts and their pictures' names
constexpr u32 BossesText = 0x62;
constexpr u32 ConceptText = 0x72;
constexpr u32 EnemiesText = 0x82;
constexpr u32 MoviesText = 0x92;
constexpr u32 UnseenText = 0xA2;
constexpr u32 StoryboardsText = 0x47;
constexpr const char* BossesName = "Extras\\Bosses\\Boss";
constexpr const char* ConceptName = "Extras\\Concept\\Concept";
constexpr const char* EnemiesName = "Extras\\Enemies\\Enemy";
constexpr const char* UnseenName = "Extras\\Unseen\\Unseen";
// The movies' movies: the bonus movies and the story's
constexpr u32 ExtrasMovies[GemExtrasItems] = {14, 15, 16, 17, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
// The movies needing the story done: from the 14th, once the story's area is 24
constexpr u32 FirstStoryEndMovie = 13;
constexpr u32 StoryDoneArea = 0x18;
// The levels' storyboards: their pictures' names and how many each has
struct Storyboard
{
    const char* name;
    u32 pictures;
};
constexpr Storyboard Storyboards[GemExtrasItems] = {
    {"Extras\\Storyboards\\02-Jungle\\Jungle", 8},           {"Extras\\Storyboards\\01-NSanity\\NSanity", 11},
    {"Extras\\Storyboards\\03-Cavern\\Cavern", 14},          {"Extras\\Storyboards\\04-Totem\\Totem", 15},
    {"Extras\\Storyboards\\06-IceClimb\\IceClimb", 17},      {"Extras\\Storyboards\\05-IceHub\\IceHub", 15},
    {"Extras\\Storyboards\\07-Slip\\Slip", 9},               {"Extras\\Storyboards\\08-HighSeas\\HighSeas", 14},
    {"Extras\\Storyboards\\09-Academy\\Academy", 8},         {"Extras\\Storyboards\\10-Boiler\\Boiler", 7},
    {"Extras\\Storyboards\\11-Classroom\\Classroom", 9},     {"Extras\\Storyboards\\12-Rooftop\\Rooftop", 7},
    {"Extras\\Storyboards\\13-Twinsanity\\Twinsanity", 5},   {"Extras\\Storyboards\\14-Rockslide\\Rockslide", 6},
    {"Extras\\Storyboards\\15-Pursuit\\Pursuit", 6},         {"Extras\\Storyboards\\16-AntAgony\\AntAgony", 10},
};
// The levels pages: each world's layout (the columns' places, the first row's height and the rows' spacing) and its levels
struct LevelsLayout
{
    f32 evenX;
    f32 oddX;
    f32 top;
    f32 spacing;
    u32 levels[4];
};
constexpr u32 Worlds = 4;
constexpr LevelsLayout LevelsLayouts[Worlds] = {
    {Rounded(0.6), Rounded(0.4), Rounded(0.15), Rounded(0.15), {0x00, 0x01, 0x03, 0x04}},
    {Rounded(0.4), Rounded(0.6), Rounded(0.15), Rounded(0.15), {0x06, 0x07, 0x09, 0x0A}},
    {Rounded(0.6), Rounded(0.4), Rounded(0.15), Rounded(0.15), {0x0D, 0x0F, 0x11, 0x12}},
    {Rounded(0.4), Rounded(0.6), Rounded(0.15), Rounded(0.15), {0x14, 0x15, 0x16, 0x17}},
};
// Past the last world (never asked for): the retail code's layout, with no levels (the retail code's are what its stack held)
constexpr LevelsLayout NoWorld = {Rounded(0.4), Rounded(0.6), Rounded(0.2), 0.125f, {}};
// The levels' rings: 4 rings of a segment of 32 steps, their radii; the colour table's white
constexpr u32 LevelRings = 4;
constexpr u32 LevelRingSegments = 1;
constexpr u32 LevelRingSteps = 0x20;
constexpr u32 WhiteColour = 0xF;
// The widgets' destructor (their vtable's), and a destructor's flags: an object deleted
constexpr u32 WidgetDestroySlot = 9;
constexpr u32 Delete = 3;
// The save slots page: a save code item per slot (IDs from 0x100), and a ring panel (0.2 by 0.075) behind each slot's widget, in
// two staggered columns (the even slots' sliding in from the left)
constexpr u32 SaveCodeItemSize = 0x20;
constexpr u32 SlotItemIds = 0x100;
constexpr f32 SlotEvenX = Rounded(0.3);
constexpr f32 SlotOddX = Rounded(0.7);
constexpr f32 SlotsTop = Rounded(0.26);
constexpr f32 SlotSpacing = Rounded(0.09);
constexpr Vector2 SlotPanelRadii = {Rounded(0.2), Rounded(0.075)};
// The volume groups: the effects' (0, 1 and 3) and the music's (2)
constexpr s32 MusicGroup = 2;

// Every other screen hidden and the screen shown, over half a second
void ShowScreen(OLEG& oleg, u32 screen)
{
    oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
    oleg.Show(oleg.masks[screen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
}

// A gallery item of a text and ID: its pictures (first to last) and their name
GalleryItem* MakeGalleryItem(u32 text, u32 id, const char* name, u32 first, u32 last)
{
    auto* item = Allocate<GalleryItem>();
    LinkItem::Construct(item, text, id, nullptr, FirstPlayer);
    item->vtable = g_GalleryItemVTable;
    StringConstruct(&item->name, name);
    item->first = 0;
    item->last = 0;
    item->unknown16[0] = 0;
    item->unknown16[1] = 0;
    item->first = static_cast<u8>(first);
    item->last = static_cast<u8>(last);
    return item;
}

// A gem extras page made: titled by its gem, its vtable, back (made first, added last)
BackItem* StartGemExtras(GemExtrasPage* page, u32 gem, MenuPage* parent, const GccVTableEntry* vtable)
{
    OlegPage::ConstructTitled(page, GemExtrasTitle + gem, parent);
    page->gem = gem;
    page->vtable = vtable;
    return BackItem::Construct(Allocate<BackItem>(), BackText, ParentBackItem, parent, FirstPlayer);
}

// The galleries of a gem extras page: each of so many pictures, numbered on from the last
void AddGalleries(GemExtrasPage* page, u32 text, const char* name, u32 pictures)
{
    u32 first = 1;
    u32 last = pictures;
    for (u32 item = 0; item < GemExtrasItems; item++)
    {
        page->Add(MakeGalleryItem(text + item, item, name, first, last));
        first += pictures;
        last += pictures;
    }
}

// A confirmation's yes and its no (back to the parent, selected first)
void AddYesNo(OlegPage* page, u32 yesId, u32 action, MenuPage* parent)
{
    ActionItem* yes = ActionItem::Construct(Allocate<ActionItem>(), YesText, yesId, action);
    BackItem* no = BackItem::Construct(Allocate<BackItem>(), NoText, NoItem, parent, FirstPlayer);
    page->Add(yes);
    page->Add(no);
    page->SetFirstItem(no->Id());
}
}

OlegPage* OlegPage::Construct(OlegPage* page, const char* name, MenuPage* parent)
{
    MenuPage::Construct(page, name, FirstPlayer);
    page->parent = parent;
    page->vtable = g_OlegPageVTable;
    return page;
}

OlegPage* OlegPage::ConstructTitled(OlegPage* page, u32 title, MenuPage* parent)
{
    MenuPage::ConstructTitled(page, title, FirstPlayer);
    page->parent = parent;
    page->vtable = g_OlegPageVTable;
    return page;
}

void OlegPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

NewGamePage* NewGamePage::Construct(NewGamePage* page, MainMenuPage* owner)
{
    OlegPage::ConstructTitled(page, NewGameText, nullptr);
    page->owner = owner;
    page->vtable = g_NewGamePageVTable;
    return page;
}

void NewGamePage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void NewGamePage::Entered(u32, u32)
{
    G_GameController_00309950->LoadForNewGame();
}

LoadGamePage* LoadGamePage::Construct(LoadGamePage* page, OlegPage* owner)
{
    OlegPage::ConstructTitled(page, LoadGameText, nullptr);
    page->owner = owner;
    page->vtable = g_LoadGamePageVTable;
    return page;
}

void LoadGamePage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void LoadGamePage::Entered(u32, u32)
{
    G_GameController_00309950->LoadSavedGame();
}

ActionItem* ActionItem::Construct(ActionItem* item, u32 text, u32 id, u32 action)
{
    LinkItem::Construct(item, text, id, nullptr, FirstPlayer);
    item->action = action;
    item->vtable = g_ActionItemVTable;
    return item;
}

void ActionItem::Destroy(u32 flags)
{
    LinkItem::Destroy(flags);
}

MenuPage* ActionItem::Activate(u32 player, MenuPage* page)
{
    GameController* controller = G_GameController_00309950;
    OLEG& oleg = controller->oleg;
    switch (action)
    {
    case ActionPauseMenu:
        controller->ReturnToPauseMenu();
        break;
    case ActionOptions:
        ShowScreen(oleg, OptionsScreen);
        break;
    case ActionScreenPosition:
        ShowScreen(oleg, ScreenPositionScreen);
        break;
    case ActionQuit:
        ShowScreen(oleg, QuitScreen);
        break;
    case ActionTitle:
        controller->QuitToTitle();
        break;
    case ActionDisableAutosave:
        ShowScreen(oleg, DisableAutosaveScreen);
        break;
    case ActionStopSaving:
        controller->StopSaving();
        break;
    case ActionLoad:
        controller->LoadSavedGame();
        break;
    case ActionSave:
        controller->SaveFromPause();
        break;
    default:
        break;
    }

    return MenuItem::Activate(player, page);
}

MainMenuPage* MainMenuPage::Construct(MainMenuPage* page)
{
    OlegPage::Construct(page, MainMenuTitle, &g_ResumePage);
    page->vtable = g_MainMenuPageVTable;
    NewGamePage::Construct(&page->newGame, page);
    LoadGamePage::Construct(&page->loadGame, page);
    LinkItem* newGame = LinkItem::Construct(Allocate<LinkItem>(), NewGameText, NewGameItem,
                                            &page->newGame, FirstPlayer);
    LinkItem* loadGame = LinkItem::Construct(Allocate<LinkItem>(), LoadGameText, LoadGameItem,
                                             &page->loadGame, FirstPlayer);
    ActionItem* options = ActionItem::Construct(Allocate<ActionItem>(), OptionsText,
                                                OptionsItem, ActionItem::ActionOptions);
    page->Add(newGame);
    page->Add(loadGame);
    page->Add(options);
    return page;
}

void MainMenuPage::Destroy(u32 flags)
{
    loadGame.OlegPage::Destroy(Member);
    newGame.OlegPage::Destroy(Member);
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void MainMenuPage::Frame(u32, MenuInput*, MenuSounds*)
{
}

ScreenPositionPage* ScreenPositionPage::Construct(ScreenPositionPage* page, MenuPage* parent)
{
    OlegPage::Construct(page, "", parent);
    page->vtable = g_ScreenPositionPageVTable;
    BackItem* item = BackItem::Construct(Allocate<BackItem>(), ContinueText, ParentBackItem,
                                         parent, FirstPlayer);
    page->Add(item);
    return page;
}

void ScreenPositionPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void ScreenPositionPage::Frame(u32, MenuInput* input, MenuSounds*)
{
    GameRendererController* renderer = G_GameRendererController;
    Vector2 offset;
    CopyVector2(&offset, &renderer->screenOffset);
    if (input->Has(MenuInput::ActionLeft, 0) != 0)
    {
        offset.x -= ScreenStepX;
    }
    else if (input->Has(MenuInput::ActionRight, 0) != 0)
    {
        offset.x += ScreenStepX;
    }

    if (input->Has(MenuInput::ActionUp, 0) != 0)
    {
        offset.y -= ScreenStepY;
    }
    else if (input->Has(MenuInput::ActionDown, 0) != 0)
    {
        offset.y += ScreenStepY;
    }

    renderer->SetScreenOffset(&offset);
}

u32 ScreenPositionPage::Back(u32)
{
    GameController* controller = G_GameController_00309950;
    OLEG& oleg = controller->oleg;
    oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
    oleg.Show(oleg.masks[OptionsScreen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
    return 1;
}

void UnusedPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void UnusedPage::Entered(u32, u32)
{
}

void UnusedPage::Left(u32, u32)
{
}

RestartItem* RestartItem::Construct(RestartItem* item, u32 text, u32 id, u32 entry)
{
    ChoiceItem::Construct(item, text, id, 0, 1, 0, FirstPlayer);
    item->vtable = g_RestartItemVTable;
    item->restart = 0;
    item->restart = ((item->restart | 1) & ~0xFEu) | (entry & 0x7F) << 1;
    return item;
}

void RestartItem::Destroy(u32 flags)
{
    ChoiceItem::Destroy(flags);
}

MenuPage* RestartItem::Activate(u32, MenuPage*)
{
    if ((restart & 1) != 0)
    {
        G_GameController_00309950->RequestRestart(restart >> 1 & 0x7F);
    }

    return nullptr;
}

GameOverPage* GameOverPage::Construct(GameOverPage* page)
{
    OlegPage::ConstructTitled(page, GameOverTitle, nullptr);
    page->vtable = g_GameOverPageVTable;
    RestartItem* restart = RestartItem::Construct(Allocate<RestartItem>(), ContinueText,
                                                  RestartItemId, RestartEntry);
    ActionItem* quit = ActionItem::Construct(Allocate<ActionItem>(), QuitGameText, QuitGameItem,
                                             ActionItem::ActionTitle);
    page->Add(restart);
    page->Add(quit);
    page->SetFirstItem(restart->Id());
    return page;
}

void GameOverPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

DisableAutosavePage* DisableAutosavePage::Construct(DisableAutosavePage* page, MenuPage* parent)
{
    OlegPage::ConstructTitled(page, DisableAutosaveTitle, parent);
    page->vtable = g_DisableAutosavePageVTable;
    AddYesNo(page, DisableAutosaveYesItem, ActionItem::ActionStopSaving, parent);
    return page;
}

void DisableAutosavePage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

u32 DisableAutosavePage::Back(u32)
{
    ShowScreen(G_GameController_00309950->oleg, PauseMenuScreen);
    return 1;
}

QuitPage* QuitPage::Construct(QuitPage* page, MenuPage* parent)
{
    OlegPage::ConstructTitled(page, QuitTitle, parent);
    page->vtable = g_QuitPageVTable;
    AddYesNo(page, QuitYesItem, ActionItem::ActionTitle, parent);
    return page;
}

void QuitPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

u32 QuitPage::Back(u32)
{
    ShowScreen(G_GameController_00309950->oleg, PauseMenuScreen);
    return 1;
}

NoticePage* NoticePage::Construct(NoticePage* page, u32 toPauseMenu, MenuPage* parent)
{
    OlegPage::Construct(page, "", parent);
    page->vtable = g_NoticePageVTable;
    MenuItem* item;
    if (toPauseMenu != 0)
    {
        item = ActionItem::Construct(Allocate<ActionItem>(), ContinueText, NoticeContinueItem,
                                     ActionItem::ActionPauseMenu);
    }
    else
    {
        item = BackItem::Construct(Allocate<BackItem>(), ContinueText, NoticeContinueItem,
                                   &g_ResumePage, FirstPlayer);
    }

    page->Add(item);
    return page;
}

void NoticePage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void UnusedPage2::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void UnusedPage2::Entered(u32, u32)
{
}

void UnusedPage2::Left(u32, u32)
{
}

void UnusedPage3::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void UnusedPage3::Frame(u32, MenuInput*, MenuSounds*)
{
}

void GameOptionsPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void GameOptionsPage::Entered(u32 player, u32)
{
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    Find(VibrationItem)->SetFlag(player, pads->flags >> 9 & 1);
}

void GameOptionsPage::Frame(u32 player, MenuInput*, MenuSounds*)
{
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    u8 vibration;
    Find(VibrationItem)->GetFlag(player, &vibration);
    if (vibration != 0)
    {
        pads->flags |= GamePadController::FlagVibration;
    }
    else
    {
        pads->DisableVibration();
    }
}

void GameOptionsPage::Left(u32, u32)
{
}

void GraphicsOptionsPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void GraphicsOptionsPage::Entered(u32 player, u32)
{
    Find(WidescreenItem)->SetFlag(player, g_WidescreenTv);
}

void GraphicsOptionsPage::Frame(u32 player, MenuInput*, MenuSounds*)
{
    u8 widescreen;
    Find(WidescreenItem)->GetFlag(player, &widescreen);
    g_WidescreenTv = widescreen;
}

SoundOptionsPage* SoundOptionsPage::Construct(SoundOptionsPage* page, MenuPage* parent)
{
    MenuPage::ConstructTitled(page, SoundOptionsText, FirstPlayer);
    page->parent = parent;
    page->vtable = g_SoundOptionsPageVTable;
    ValueItem* effects = ValueItem::Construct(Allocate<ValueItem>(), EffectsVolumeText, EffectsVolumeItem, 0, MaxVolume, 0, MaxVolume,
                                              FirstPlayer);
    ValueItem* music = ValueItem::Construct(Allocate<ValueItem>(), MusicVolumeText, MusicVolumeItem, 0, MaxVolume, 0, MaxVolume,
                                            FirstPlayer);
    ChoiceItem* output = ChoiceItem::Construct(Allocate<ChoiceItem>(), OutputTypeText, OutputTypeItem, OutputTypes, 1, 0, FirstPlayer);
    BackItem* back = BackItem::Construct(Allocate<BackItem>(), BackText, SubPageBackItem, parent, FirstPlayer);
    for (u32 choice = 0; choice < OutputTypes; choice++)
    {
        output->SetChoiceText(choice, static_cast<s32>(MonoText + choice));
    }

    page->Add(effects);
    page->Add(music);
    page->Add(output);
    page->Add(back);
    return page;
}

void SoundOptionsPage::Destroy(u32 flags)
{
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void SoundOptionsPage::Entered(u32 player, u32)
{
    MenuItem* effects = Find(EffectsVolumeItem);
    MenuItem* music = Find(MusicVolumeItem);
    MenuItem* output = Find(OutputTypeItem);
    f32 effectsLevel = GroupVolumeLevel(0);
    f32 musicLevel = GroupVolumeLevel(MusicGroup);
    u32 stereo = g_MusicStereo;
    effects->SetIntValue(player, static_cast<s32>(effectsLevel * 10.0f + 0.5f));
    music->SetIntValue(player, static_cast<s32>(musicLevel * 10.0f + 0.5f));
    output->SetIntValue(player, static_cast<s32>(stereo));
}

void SoundOptionsPage::Frame(u32 player, MenuInput*, MenuSounds*)
{
    MenuItem* effects = Find(EffectsVolumeItem);
    MenuItem* music = Find(MusicVolumeItem);
    s32 value;
    if (Item(selections[player])->Id() == MusicVolumeItem)
    {
        music->GetIntValue(player, &value);
    }
    else
    {
        effects->GetIntValue(player, &value);
    }

    f32 level = static_cast<f32>(value) * Rounded(0.1);
    SetGroupVolume(level, level, 0);
    SetGroupVolume(level, level, 1);
    SetGroupVolume(level, level, 3);
}

void SoundOptionsPage::Left(u32 player, u32)
{
    MenuItem* effects = Find(EffectsVolumeItem);
    MenuItem* music = Find(MusicVolumeItem);
    MenuItem* output = Find(OutputTypeItem);
    s32 values[3];
    effects->GetIntValue(player, &values[0]);
    music->GetIntValue(player, &values[1]);
    output->GetIntValue(player, &values[2]);
    f32 effectsLevel = static_cast<f32>(values[0]) * Rounded(0.1);
    f32 musicLevel = static_cast<f32>(values[1]) * Rounded(0.1);
    SetGroupVolume(effectsLevel, effectsLevel, 0);
    SetGroupVolume(effectsLevel, effectsLevel, 1);
    SetGroupVolume(effectsLevel, effectsLevel, 3);
    SetGroupVolume(musicLevel, musicLevel, MusicGroup);
    SetMusicStereo(static_cast<u32>(values[2]));
}

OptionsPage* OptionsPage::Construct(OptionsPage* page, MenuPage* parent)
{
    MenuPage::ConstructTitled(page, OptionsText, FirstPlayer);
    page->parent = parent;
    page->vtable = g_OptionsPageVTable;

    GameOptionsPage& game = page->game;
    MenuPage::ConstructTitled(&game, GameOptionsText, FirstPlayer);
    game.parent = page;
    game.vtable = g_GameOptionsPageVTable;
    ToggleItem* vibration = ToggleItem::Construct(Allocate<ToggleItem>(), VibrationText, VibrationItem, NoText, YesText, 1, 0, FirstPlayer);
    BackItem* gameBack = BackItem::Construct(Allocate<BackItem>(), BackText, SubPageBackItem, page, FirstPlayer);
    game.Add(vibration);
    game.Add(gameBack);

    GraphicsOptionsPage& graphics = page->graphics;
    MenuPage::ConstructTitled(&graphics, GraphicOptionsText, FirstPlayer);
    graphics.parent = page;
    graphics.vtable = g_GraphicsOptionsPageVTable;
    ActionItem* centre = ActionItem::Construct(Allocate<ActionItem>(), CentreScreenText, CentreScreenItem,
                                               ActionItem::ActionScreenPosition);
    ToggleItem* widescreen = ToggleItem::Construct(Allocate<ToggleItem>(), WidescreenText, WidescreenItem, OffText, OnText, 1, 0,
                                                   FirstPlayer);
    BackItem* graphicsBack = BackItem::Construct(Allocate<BackItem>(), BackText, SubPageBackItem, page, FirstPlayer);
    graphics.Add(centre);
    graphics.Add(widescreen);
    graphics.Add(graphicsBack);

    SoundOptionsPage::Construct(&page->sound, page);
    LinkItem* graphicsLink = LinkItem::Construct(Allocate<LinkItem>(), GraphicOptionsText, GraphicOptionsItem, &page->graphics,
                                                 FirstPlayer);
    LinkItem* soundLink = LinkItem::Construct(Allocate<LinkItem>(), SoundOptionsText, SoundOptionsItem, &page->sound, FirstPlayer);
    LinkItem* gameLink = LinkItem::Construct(Allocate<LinkItem>(), GameOptionsText, GameOptionsItem, &page->game, FirstPlayer);
    BackItem* back = BackItem::Construct(Allocate<BackItem>(), BackText, ParentBackItem, parent, FirstPlayer);
    page->Add(graphicsLink);
    page->Add(soundLink);
    page->Add(gameLink);
    page->Add(back);
    return page;
}

void OptionsPage::Destroy(u32 flags)
{
    sound.vtable = g_OlegPageVTable;
    sound.MenuPage::Destroy(Member);
    graphics.vtable = g_OlegPageVTable;
    graphics.MenuPage::Destroy(Member);
    game.vtable = g_OlegPageVTable;
    game.MenuPage::Destroy(Member);
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

u32 OptionsPage::Back(u32)
{
    GameController* controller = G_GameController_00309950;
    OLEG& oleg = controller->oleg;
    oleg.Hide(oleg.masks[AllButFirstScreens], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
    u32 screen = G_GameController_00309950->State() == GameController::StateMainMenu ? MainMenuScreen : PauseMenuScreen;
    oleg.Show(oleg.masks[screen], static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), 0);
    return 1;
}

void SaveGamePage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void SaveGamePage::Entered(u32, u32)
{
    G_GameController_00309950->SaveFromPause();
}

PausePage* PausePage::Construct(PausePage* page, PadButtons*, Font*)
{
    OlegPage::ConstructTitled(page, PauseTitle, &g_ResumePage);
    page->vtable = g_PausePageVTable;
    SaveGamePage& saveGame = page->saveGame;
    OlegPage::ConstructTitled(&saveGame, SaveGameText, nullptr);
    saveGame.owner = page;
    saveGame.vtable = g_SaveGamePageVTable;
    LoadGamePage& loadGame = page->loadGame;
    OlegPage::ConstructTitled(&loadGame, LoadGameText, nullptr);
    loadGame.owner = page;
    loadGame.vtable = g_LoadGamePageVTable;
    QuitPage::Construct(&page->quit, page);
    ActionItem* options = ActionItem::Construct(Allocate<ActionItem>(), OptionsText, OptionsItem, ActionItem::ActionOptions);
    LinkItem* save = LinkItem::Construct(Allocate<LinkItem>(), SaveGameText, SaveGameItem, &page->saveGame, FirstPlayer);
    LinkItem* load = LinkItem::Construct(Allocate<LinkItem>(), LoadGameText, LoadGameItem, &page->loadGame, FirstPlayer);
    ActionItem* disableAutosave = ActionItem::Construct(Allocate<ActionItem>(), DisableAutosaveText, DisableAutosaveItem,
                                                        ActionItem::ActionDisableAutosave);
    ActionItem* quit = ActionItem::Construct(Allocate<ActionItem>(), QuitGameText, QuitGameItem, ActionItem::ActionQuit);
    BackItem* resume = BackItem::Construct(Allocate<BackItem>(), ResumeText, ResumeItem, &g_ResumePage, FirstPlayer);
    page->Add(options);
    page->Add(save);
    page->Add(load);
    page->Add(disableAutosave);
    page->Add(quit);
    page->Add(resume);
    page->SetFirstItem(ResumeItem);
    return page;
}

void PausePage::Destroy(u32 flags)
{
    quit.OlegPage::Destroy(Member);
    loadGame.OlegPage::Destroy(Member);
    saveGame.OlegPage::Destroy(Member);
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void PausePage::Frame(u32, MenuInput*, MenuSounds*)
{
    // The disable autosave item is there while an autosave waits or saves, not while its screens show
    MenuItem* disableAutosave = Find(DisableAutosaveItem);
    u64 saving = G_GameController_00309950->states & u64{0xF} << 60;
    bool autosaving = saving == u64{1} << 60 || saving == u64{2} << 60 || saving == u64{3} << 60;
    bool shown = false;
    if (autosaving)
    {
        shown = (G_GameController_00309950->states & u64{0xF} << 60) != u64{3} << 60;
    }

    disableAutosave->shown = shown ? 0xFF : 0;
}

void GalleryItem::Destroy(u32 flags)
{
    StringDestroy(&name);
    LinkItem::Destroy(flags);
}

MenuPage* GalleryItem::Activate(u32, MenuPage*)
{
    G_GameController_00309950->RequestGallery(static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), name.string, first, last);
    return nullptr;
}

MovieItem* MovieItem::Construct(MovieItem* item, u32 text, u32 id, u32 movie)
{
    LinkItem::Construct(item, text, id, nullptr, FirstPlayer);
    item->movie = movie;
    item->vtable = g_MovieItemVTable;
    return item;
}

void MovieItem::Destroy(u32 flags)
{
    LinkItem::Destroy(flags);
}

MenuPage* MovieItem::Activate(u32, MenuPage*)
{
    G_GameController_00309950->RequestMovie(static_cast<s32>(g_ClockUnitsPerSecond * 0.5f), movie);
    return nullptr;
}

void GemExtrasPage::ShowFound()
{
    GameProgress& progress = G_GameController_00309950->progress;
    for (u32 item = 0; item < GemExtrasItems; item++)
    {
        MenuItem* found = Find(item);
        found->enabled = (static_cast<u8>(progress.levels[item]) & 1u << gem) != 0 ? 0xFF : 0;
    }
}

BossesPage* BossesPage::Construct(BossesPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_BossesPageVTable);
    AddGalleries(page, BossesText, BossesName, 1);
    page->Add(back);
    return page;
}

void BossesPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void BossesPage::Entered(u32, u32)
{
    ShowFound();
}

ConceptPage* ConceptPage::Construct(ConceptPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_ConceptPageVTable);
    AddGalleries(page, ConceptText, ConceptName, 2);
    page->Add(back);
    return page;
}

void ConceptPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void ConceptPage::Entered(u32, u32)
{
    ShowFound();
}

EnemiesPage* EnemiesPage::Construct(EnemiesPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_EnemiesPageVTable);
    AddGalleries(page, EnemiesText, EnemiesName, 2);
    page->Add(back);
    return page;
}

void EnemiesPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void EnemiesPage::Entered(u32, u32)
{
    ShowFound();
}

MoviesPage* MoviesPage::Construct(MoviesPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_MoviesPageVTable);
    for (u32 item = 0; item < GemExtrasItems; item++)
    {
        page->Add(MovieItem::Construct(Allocate<MovieItem>(), MoviesText + item, item, ExtrasMovies[item]));
    }

    page->Add(back);
    return page;
}

void MoviesPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void MoviesPage::Entered(u32, u32)
{
    GameProgress& progress = G_GameController_00309950->progress;
    for (u32 item = 0; item < GemExtrasItems; item++)
    {
        MenuItem* movie = Find(item);
        bool found = (static_cast<u8>(progress.levels[item]) & 1u << gem) != 0;
        if (item >= FirstStoryEndMovie)
        {
            found = found && (progress.bits >> 21 & 0x1F) >= StoryDoneArea;
        }

        movie->enabled = found ? 0xFF : 0;
    }
}

StoryboardsPage* StoryboardsPage::Construct(StoryboardsPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_StoryboardsPageVTable);
    for (u32 item = 0; item < GemExtrasItems; item++)
    {
        page->Add(MakeGalleryItem(StoryboardsText + item, item, Storyboards[item].name, 1, Storyboards[item].pictures));
    }

    page->Add(back);
    return page;
}

void StoryboardsPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void StoryboardsPage::Entered(u32, u32)
{
    ShowFound();
}

UnseenPage* UnseenPage::Construct(UnseenPage* page, u32 gem, MenuPage* parent)
{
    BackItem* back = StartGemExtras(page, gem, parent, g_UnseenPageVTable);
    AddGalleries(page, UnseenText, UnseenName, 3);
    page->Add(back);
    return page;
}

void UnseenPage::Destroy(u32 flags)
{
    OlegPage::Destroy(flags);
}

void UnseenPage::Entered(u32, u32)
{
    ShowFound();
}

ExtrasPage* ExtrasPage::Construct(ExtrasPage* page)
{
    OlegPage::ConstructTitled(page, ExtrasTitle, &g_ResumePage);
    page->vtable = g_ExtrasPageVTable;
    BossesPage::Construct(&page->bosses, BlueGem, page);
    ConceptPage::Construct(&page->conceptArt, PurpleGem, page);
    EnemiesPage::Construct(&page->enemies, GreenGem, page);
    MoviesPage::Construct(&page->movies, ClearGem, page);
    StoryboardsPage::Construct(&page->storyboards, RedGem, page);
    UnseenPage::Construct(&page->unseen, YellowGem, page);
    MovieItem* complete = MovieItem::Construct(Allocate<MovieItem>(), CompleteText, CompleteItem, CompleteMovie);
    BackItem* resume = BackItem::Construct(Allocate<BackItem>(), ResumeText, ExtrasResumeItem, &g_ResumePage, FirstPlayer);
    // Every link made before any is added (adding can grow the page's list)
    OlegPage* const gemPages[Gems] = {&page->bosses,     &page->movies,      &page->enemies,
                                      &page->conceptArt, &page->storyboards, &page->unseen};
    LinkItem* links[Gems];
    for (u32 gem = 0; gem < Gems; gem++)
    {
        links[gem] = LinkItem::Construct(Allocate<LinkItem>(), GemExtrasTitle + gem, GemExtrasLink + gem, gemPages[gem], FirstPlayer);
    }

    for (LinkItem* link : links)
    {
        page->Add(link);
    }

    page->Add(complete);
    page->Add(resume);
    return page;
}

void ExtrasPage::Destroy(u32 flags)
{
    unseen.OlegPage::Destroy(Member);
    storyboards.OlegPage::Destroy(Member);
    movies.OlegPage::Destroy(Member);
    enemies.OlegPage::Destroy(Member);
    conceptArt.OlegPage::Destroy(Member);
    bosses.OlegPage::Destroy(Member);
    vtable = g_OlegPageVTable;
    MenuPage::Destroy(flags);
}

void ExtrasPage::Entered(u32, u32)
{
    GameProgress& progress = G_GameController_00309950->progress;
    u32 done = progress.Done();
    MenuItem* complete = Find(CompleteItem);
    for (u32 gem = 0; gem < Gems; gem++)
    {
        MenuItem* link = Find(GemExtrasLink + gem);
        link->shown = progress.GemsFound(gem) != 0 ? 0xFF : 0;
    }

    if (done == 100)
    {
        complete->id = (complete->id & ~(MenuItem::TextMask << MenuItem::TextShift)) | CompleteText << MenuItem::TextShift;
        complete->shown = 0xFF;
    }
    else
    {
        complete->id = (complete->id & ~(MenuItem::TextMask << MenuItem::TextShift)) | NotCompleteText << MenuItem::TextShift;
        complete->shown = 0;
    }

    complete->text = nullptr;
}

LevelsPage* LevelsPage::Construct(LevelsPage* page, u32 world, MenuWidget* menu)
{
    OlegPage::Construct(page, "", &g_ResumePage);
    page->vtable = g_LevelsPageVTable;
    Widget* after = menu->next;
    const LevelsLayout& layout = world < Worlds ? LevelsLayouts[world] : NoWorld;
    MenuItem* items[Slots] = {};
    if (world < Worlds)
    {
        for (u32 level = 0; level < 4; level++)
        {
            items[level] = LinkItem::ConstructNamed(Allocate<LinkItem>(), "", layout.levels[level], nullptr, FirstPlayer);
        }
    }

    Widget* tail = menu;
    for (u32 slot = 0; slot < Slots; slot++)
    {
        MenuItem* item = items[slot];
        if (item == nullptr)
        {
            page->rings[slot] = nullptr;
            page->levels[slot] = nullptr;
            continue;
        }

        Vector2 place = {(slot & 1) == 0 ? layout.evenX : layout.oddX, layout.top + layout.spacing * static_cast<f32>(slot)};
        Vector2 scale = {1.0f, 1.0f};
        Vector2 levelScale = {Rounded(0.6), Rounded(0.1)};
        Vector2 offset = {place.x < 0.5f ? -1.0f : 1.0f, 0.0f};
        RingWidget* ring = RingWidget::Construct(Allocate<RingWidget>(), 0.5f, LevelRings, LevelRingSegments, LevelRingSteps, nullptr);
        page->rings[slot] = ring;
        LevelWidget* level = LevelWidget::Construct(Allocate<LevelWidget>(), 0.5f, page, item, G_GameController_00309950);
        page->levels[slot] = level;
        u32 colour;
        GetColor(&colour, WhiteColour);
        ring->SlideIn(colour, &place, &scale, &offset);
        GetColor(&colour, WhiteColour);
        level->SlideIn(colour, &place, &levelScale, &offset);
        Vector2 radii = {Rounded(0.3), Rounded(0.05)};
        AddPanelRings(&radii, ring);
        tail->next = ring;
        ring->next = level;
        tail = level;
        page->Add(item);
    }

    tail->next = after;
    return page;
}

void LevelsPage::Destroy(u32 flags)
{
    vtable = g_LevelsPageVTable;
    for (u32 slot = 0; slot < Slots; slot++)
    {
        if (levels[slot] != nullptr)
        {
            CallVirtual<void>(levels[slot], levels[slot]->vtable, WidgetDestroySlot, Delete);
        }

        if (rings[slot] != nullptr)
        {
            CallVirtual<void>(rings[slot], rings[slot]->vtable, WidgetDestroySlot, Delete);
        }
    }

    OlegPage::Destroy(flags);
}

MenuPage* SaveSlotsPageConstruct(void* memory, SaveManager* manager, MenuWidget* menu)
{
    auto* page = static_cast<SaveSlotsPage*>(memory);
    SaveCodePage::Construct(page, manager);
    GameController* controller = G_GameController_00309950;
    page->vtable = g_SaveSlotsPageVTable;
    Widget* after = menu->next;
    // The device's flags are its first word (savedevice.h's SaveSummary clashes with OLEG's, so its class can't be included)
    page->count = *reinterpret_cast<const u32*>(manager->device) & 0xF;
    page->widgets = static_cast<SaveSlotWidget**>(MemoryAllocate2(page->count * sizeof(SaveSlotWidget*)));
    page->rings = static_cast<RingWidget**>(MemoryAllocate2(page->count * sizeof(RingWidget*)));
    Widget* tail = menu;
    for (u32 slot = 0; slot < page->count; slot++)
    {
        auto* summary = reinterpret_cast<SaveSummary*>(manager->banks[slot]->summary);
        MenuItem* item = ConstructSaveCodeItem(MemoryAllocate(SaveCodeItemSize), 0, SlotItemIds + slot, 0, manager);
        bool even = (slot & 1) == 0;
        Vector2 place = {even ? SlotEvenX : SlotOddX, static_cast<f32>(slot) * SlotSpacing + SlotsTop};
        Vector2 scale = {1.0f, 1.0f};
        Vector2 offset = {even ? -1.0f : 1.0f, 0.0f};
        RingWidget* ring =
            RingWidget::Construct(Allocate<RingWidget>(), 0.5f, LevelRings, LevelRingSegments, LevelRingSteps, nullptr);
        page->rings[slot] = ring;
        SaveSlotWidget* widget = SaveSlotWidget::Construct(Allocate<SaveSlotWidget>(), 0.5f, page, item, controller, summary);
        page->widgets[slot] = widget;
        u32 colour;
        GetColor(&colour, WhiteColour);
        ring->SlideIn(colour, &place, &scale, &offset);
        GetColor(&colour, WhiteColour);
        widget->SlideIn(colour, &place, &scale, &offset);
        Vector2 radii = SlotPanelRadii;
        AddPanelRings(&radii, ring);
        tail->next = ring;
        ring->next = widget;
        tail = widget;
        page->Add(item);
    }

    tail->next = after;
    page->AddItems();
    return page;
}

void SaveSlotsPage::Destroy(u32 destroyFlags)
{
    vtable = g_SaveSlotsPageVTable;
    for (u32 slot = 0; slot < count; slot++)
    {
        if (widgets[slot] != nullptr)
        {
            CallVirtual<void>(widgets[slot], widgets[slot]->vtable, WidgetDestroySlot, Delete);
        }

        if (rings[slot] != nullptr)
        {
            CallVirtual<void>(rings[slot], rings[slot]->vtable, WidgetDestroySlot, Delete);
        }
    }

    if (widgets != nullptr)
    {
        MemoryDeallocate_(widgets);
    }

    if (rings != nullptr)
    {
        MemoryDeallocate_(rings);
    }

    // Past the save code page's destructor, straight to the menu page's
    MenuPage::Destroy(destroyFlags);
}

void InitOlegPagesModule(u32 initialize, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    constexpr f32 Small = Rounded(0.01);
    constexpr s32 Colour8 = 8;
    if (priority != AllPriorities || initialize == 0)
    {
        return;
    }

    g_OlegPagesUp.x = 0.0f;
    g_OlegPagesUp.w = 1.0f;
    g_OlegPagesSmall2 = Small;
    g_OlegPagesUp.y = 1.0f;
    g_OlegPagesUp.z = 0.0f;
    g_OlegPagesSmall = Small;
    ColourSet(&g_OlegPagesShade, 0.0f, 0.0f, 0.0f, 0.5f);
    AngleFrom(&g_OlegPagesAngle45, 0x1.921fb6p-1f, AngleRadians);
    g_OlegPagesUpBack.x = 0.0f;
    g_OlegPagesUp2.w = 1.0f;
    g_OlegPagesUpBack.z = -1.0f;
    g_OlegPagesUpBack.w = 1.0f;
    g_OlegPagesUp2.x = 0.0f;
    g_OlegPagesUp2.z = 0.0f;
    g_OlegPagesUpBack.y = 1.0f;
    g_OlegPagesUp2.y = 1.0f;
    g_OlegPagesZero = 0;
    GetColor(&g_OlegPagesWhite, WhiteColour);
    GetColor(&g_OlegPagesColour8, Colour8);
}

void ConstructOlegPagesModule()
{
    InitOlegPagesModule(1, 0xFFFF);
}
