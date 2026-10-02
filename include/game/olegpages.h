#pragma once

#include "common.h"
#include "game/menus.h"
#include "game/string.h"

class Font;

// OLEG's menus' pages and their items (their vtables the retail ones)

// A page of OLEG's (D_002F4610): a menu page for the first player, its parent given
class OlegPage : public MenuPage
{
public:
    static OlegPage* Construct(OlegPage* page, const char* name, MenuPage* parent) RETAIL(FUN_00166998);
    static OlegPage* ConstructTitled(OlegPage* page, u32 title, MenuPage* parent) RETAIL(FUN_001669e0);
    void Destroy(u32 flags) RETAIL(FUN_00166a28);
};
CHECK_SIZE(OlegPage, 0x50);

class MainMenuPage;

// The main menu's new game page (D_002F4250, its title "new game"): entering it asks the save code for a new game's operation
class NewGamePage : public OlegPage
{
public:
    MainMenuPage* owner;

    static NewGamePage* Construct(NewGamePage* page, MainMenuPage* owner) RETAIL(FUN_00166150);
    void Destroy(u32 flags) RETAIL(FUN_00165f10);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00166130);
};
CHECK_SIZE(NewGamePage, 0x54);

// The main menu's and the pause menu's load game page (D_002F42A0, "load game"): entering it asks the save code for a load
class LoadGamePage : public OlegPage
{
public:
    OlegPage* owner;

    static LoadGamePage* Construct(LoadGamePage* page, OlegPage* owner) RETAIL(FUN_001660e0);
    void Destroy(u32 flags) RETAIL(FUN_00165ef0);
    void Entered(u32 player, u32 mode) RETAIL(FUN_001660c0);
};
CHECK_SIZE(LoadGamePage, 0x54);

// An item doing one of the game controller's actions when it's activated (D_002F4188), then leading where a link leads (nowhere)
struct ActionItem : LinkItem
{
    enum Action : u32
    {
        // Back to the pause menu
        ActionPauseMenu = 0,
        // OLEG's screens: the options, the screen's position, quitting
        ActionOptions = 1,
        ActionScreenPosition = 2,
        ActionQuit = 3,
        // Back to the title
        ActionTitle = 4,
        // The screen asking to switch the autosave off
        ActionDisableAutosave = 5,
        // The saving stopped, a load from the card, a save from the pause menu
        ActionStopSaving = 6,
        ActionLoad = 7,
        ActionSave = 8,
    };

    u32 action;

    static ActionItem* Construct(ActionItem* item, u32 text, u32 id, u32 action) RETAIL(FUN_00166310);
    void Destroy(u32 flags) RETAIL(FUN_00165f90);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(FUN_001636d0);
};
CHECK_SIZE(ActionItem, 0x18);

// The main menu (D_002F4200, "crash twinsanity"; going back leaves the menu): a new game, loading a game, and the options
class MainMenuPage : public OlegPage
{
public:
    NewGamePage newGame;
    LoadGamePage loadGame;

    static MainMenuPage* Construct(MainMenuPage* page) RETAIL(FUN_00165fb8);
    void Destroy(u32 flags) RETAIL(FUN_00165f30);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00165fb0);
};
CHECK_SIZE(MainMenuPage, 0xF8);

// The screen position page (D_002F3E40, untitled): the directions held move the screen (left and right 0.015 of it a frame, up and
// down 0.03), going back shows the options again; a "continue" item leads back to its parent
class ScreenPositionPage : public OlegPage
{
public:
    static ScreenPositionPage* Construct(ScreenPositionPage* page, MenuPage* parent) RETAIL(FUN_00166920);
    void Destroy(u32 flags) RETAIL(FUN_001668f8);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00164238);
    u32 Back(u32 player) RETAIL(FUN_001641a8);
};
CHECK_SIZE(ScreenPositionPage, 0x50);

// A page class no code makes (its vtable after the screen position page's): told it's entered and left, doing nothing
class UnusedPage : public OlegPage
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001668c0);
    void Entered(u32 player, u32 mode) RETAIL(FUN_001668e8);
    void Left(u32 player, u32 mode) RETAIL(FUN_001668f0);
};

// An item restarting the game a way when it's activated (D_002F4020, a choice item without choices): bit 0 of its word whether it
// restarts, bits 1-7 the way into the game. It leads nowhere
struct RestartItem : ChoiceItem
{
    u32 restart;

    static RestartItem* Construct(RestartItem* item, u32 text, u32 id, u32 entry) RETAIL(FUN_00166840);
    void Destroy(u32 flags) RETAIL(FUN_00166360);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(FUN_00166808);
};
CHECK_SIZE(RestartItem, 0x54);

// The game over page (D_002F4750, "game over"): continue (restarting from the save's chunk and place) or quit the game (back to
// the title), continuing selected first
class GameOverPage : public OlegPage
{
public:
    static GameOverPage* Construct(GameOverPage* page) RETAIL(FUN_00166380);
    void Destroy(u32 flags) RETAIL(FUN_00167100);
};
CHECK_SIZE(GameOverPage, 0x50);

// The disable autosave page (D_002F4098, "disable autosave"): yes stops the saving, no goes back (selected first); going back
// shows the pause menu again
class DisableAutosavePage : public OlegPage
{
public:
    static DisableAutosavePage* Construct(DisableAutosavePage* page, MenuPage* parent) RETAIL(FUN_00166248);
    void Destroy(u32 flags) RETAIL(FUN_00166220);
    u32 Back(u32 player) RETAIL(FUN_00163640);
};
CHECK_SIZE(DisableAutosavePage, 0x50);

// The quit page (D_002F3F30, "quit game?"): yes goes back to the title, no goes back (selected first); going back shows the pause
// menu again
class QuitPage : public OlegPage
{
public:
    static QuitPage* Construct(QuitPage* page, MenuPage* parent) RETAIL(FUN_00166728);
    void Destroy(u32 flags) RETAIL(FUN_00166538);
    u32 Back(u32 player) RETAIL(FUN_00164118);
};
CHECK_SIZE(QuitPage, 0x50);

// A notice's page (D_002F4700, untitled): "continue" back to the pause menu, or (none asked for) leaving the menu
class NoticePage : public OlegPage
{
public:
    static NoticePage* Construct(NoticePage* page, u32 toPauseMenu, MenuPage* parent) RETAIL(FUN_00166670);
    void Destroy(u32 flags) RETAIL(FUN_00167120);
};
CHECK_SIZE(NoticePage, 0x50);

// Two more page classes no code makes (their vtables after the disable autosave and quit pages'), doing nothing
class UnusedPage2 : public OlegPage
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001661e8);
    void Entered(u32 player, u32 mode) RETAIL(func_00166210);
    void Left(u32 player, u32 mode) RETAIL(func_00166218);
};

class UnusedPage3 : public OlegPage
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00166508);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00166530);
};

// The options' game options page (D_002F3DF0, "game options"): vibration (the pad controller's, shown when entered, set every
// frame) and back
class GameOptionsPage : public OlegPage
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00166a50);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00166a78);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00166ac0);
    void Left(u32 player, u32 mode) RETAIL(FUN_00166b28);
};
CHECK_SIZE(GameOptionsPage, 0x50);

// The options' graphic options page (D_002F3DA0, "graphic options"): centring the screen (the screen position), widescreen (the
// TV's, shown when entered, set every frame) and back
class GraphicsOptionsPage : public OlegPage
{
public:
    u32 unknown50;

    void Destroy(u32 flags) RETAIL(FUN_00166b30);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00166ba0);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00166b58);
};
CHECK_SIZE(GraphicsOptionsPage, 0x54);

// The options' sound options page (D_002F3D50, "sound options"): the effects' and the music's volumes (0 to 10: the volume
// groups' levels), the output type (mono, stereo, Dolby Pro Logic II: the music's) and back. While it's shown the selected
// volume plays as the effects' (both volumes and the output type set when it's left)
class SoundOptionsPage : public OlegPage
{
public:
    static SoundOptionsPage* Construct(SoundOptionsPage* page, MenuPage* parent) RETAIL(FUN_001646c8);
    void Destroy(u32 flags) RETAIL(FUN_00166be8);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00166c98);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_00166db8);
    void Left(u32 player, u32 mode) RETAIL(FUN_00166ec8);
};
CHECK_SIZE(SoundOptionsPage, 0x50);

// The options (D_002F3D00, "options"): the graphic, sound and game options and back; going back shows the main menu (in the
// main menu) or the pause menu again
class OptionsPage : public OlegPage
{
public:
    GameOptionsPage game;
    GraphicsOptionsPage graphics;
    SoundOptionsPage sound;

    static OptionsPage* Construct(OptionsPage* page, MenuPage* parent) RETAIL(FUN_00164418);
    void Destroy(u32 flags) RETAIL(FUN_00166c10);
    u32 Back(u32 player) RETAIL(FUN_00164348);
};
CHECK_SIZE(OptionsPage, 0x144);

// The pause menu's save game page (D_002F4138, "save game"): entering it asks the save code for a save
class SaveGamePage : public OlegPage
{
public:
    OlegPage* owner;

    void Destroy(u32 flags) RETAIL(FUN_001661a0);
    void Entered(u32 player, u32 mode) RETAIL(FUN_001661c0);
};
CHECK_SIZE(SaveGamePage, 0x54);

// The pause menu (D_002F3EE0, "game paused"; going back leaves the menu): options, saving and loading a game, disabling the
// autosave (shown only while an autosave waits or saves), quitting the game and resuming (selected first)
class PausePage : public OlegPage
{
public:
    SaveGamePage saveGame;
    LoadGamePage loadGame;
    QuitPage quit;

    // The pad and the font are left unused
    static PausePage* Construct(PausePage* page, PadButtons* pad, Font* font) RETAIL(FUN_00163ee8);
    void Destroy(u32 flags) RETAIL(FUN_00166558);
    void Frame(u32 player, MenuInput* input, MenuSounds* sounds) RETAIL(FUN_001665c0);
};
CHECK_SIZE(PausePage, 0x148);

// An item showing a gallery of pictures when it's activated (D_002F42F0), half a second after: its first and last pictures and their
// name (the picture's number after it). It leads nowhere
struct GalleryItem : LinkItem
{
    u8 first;
    u8 last;
    u8 unknown16[2];
    String name;

    void Destroy(u32 flags) RETAIL(FUN_001653c8);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(func_00165408);
};
CHECK_SIZE(GalleryItem, 0x24);

// An item playing a movie of the game's when it's activated (D_002F4598), half a second after. It leads nowhere
struct MovieItem : LinkItem
{
    u32 movie;

    static MovieItem* Construct(MovieItem* item, u32 text, u32 id, u32 movie) RETAIL(FUN_00165370);
    void Destroy(u32 flags) RETAIL(FUN_001650b8);
    MenuPage* Activate(u32 player, MenuPage* page) RETAIL(func_00165330);
};
CHECK_SIZE(MovieItem, 0x18);

// The extras of a gem's colour (blue, clear, green, purple, red, yellow): sixteen items, each there once its level's gem of the
// colour is found, and back. Each gem's page is a class of its own (its vtable and functions apart)
class GemExtrasPage : public OlegPage
{
public:
    u32 gem;

    // Every item there whose level has the page's gem
    void ShowFound();
};
CHECK_SIZE(GemExtrasPage, 0x54);

// Blue (D_002F4548): the bosses' galleries, a picture each
class BossesPage : public GemExtrasPage
{
public:
    static BossesPage* Construct(BossesPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_001654f8);
    void Destroy(u32 flags) RETAIL(FUN_001650d8);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165468);
};

// Purple (D_002F44F8): the concept art's galleries, two pictures each
class ConceptPage : public GemExtrasPage
{
public:
    static ConceptPage* Construct(ConceptPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_001656a8);
    void Destroy(u32 flags) RETAIL(FUN_001650f8);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165618);
};

// Green (D_002F44A8): the enemies' galleries, two pictures each
class EnemiesPage : public GemExtrasPage
{
public:
    static EnemiesPage* Construct(EnemiesPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_00165868);
    void Destroy(u32 flags) RETAIL(FUN_00165118);
    void Entered(u32 player, u32 mode) RETAIL(FUN_001657d8);
};

// Clear (D_002F4458): the movies (the bonus movies and the story's), the last three only once the story is done
class MoviesPage : public GemExtrasPage
{
public:
    static MoviesPage* Construct(MoviesPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_00165a58);
    void Destroy(u32 flags) RETAIL(FUN_00165138);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165998);
};

// Red (D_002F4408): the levels' storyboards
class StoryboardsPage : public GemExtrasPage
{
public:
    static StoryboardsPage* Construct(StoryboardsPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_00165bf0);
    void Destroy(u32 flags) RETAIL(FUN_00165158);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165b60);
};

// Yellow (D_002F43B8): the unseen galleries, three pictures each
class UnseenPage : public GemExtrasPage
{
public:
    static UnseenPage* Construct(UnseenPage* page, u32 gem, MenuPage* parent) RETAIL(FUN_00165dc0);
    void Destroy(u32 flags) RETAIL(FUN_00165178);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165d30);
};

// The extras (D_002F4368, "extras"; going back leaves the menu): the gem extras (each there once a gem of its colour is found),
// the complete game's movie ("100%": there once the game's done, "??????????" before) and resume
class ExtrasPage : public OlegPage
{
public:
    BossesPage bosses;
    ConceptPage conceptArt;
    EnemiesPage enemies;
    MoviesPage movies;
    StoryboardsPage storyboards;
    UnseenPage unseen;

    static ExtrasPage* Construct(ExtrasPage* page) RETAIL(FUN_001633c8);
    void Destroy(u32 flags) RETAIL(FUN_00165198);
    void Entered(u32 player, u32 mode) RETAIL(FUN_00165228);
};
CHECK_SIZE(ExtrasPage, 0x248);

class LevelWidget;
class MenuWidget;
class RingWidget;

// A world's levels page (D_002F3FD0, untitled; going back leaves the menu): its four levels (named items: the level widgets show
// them) in two staggered columns, each a disc (a ring widget) behind a level widget, sliding in from their side. The rings and
// level widgets are chained after the world's menu widget
class LevelsPage : public OlegPage
{
public:
    static constexpr u32 Slots = 5;

    LevelWidget* levels[Slots];
    RingWidget* rings[Slots];

    static LevelsPage* Construct(LevelsPage* page, u32 world, MenuWidget* menu) RETAIL(FUN_001638c0);
    void Destroy(u32 flags) RETAIL(FUN_00166458);
};
CHECK_SIZE(LevelsPage, 0x78);

extern "C"
{
    extern const GccVTableEntry g_OlegPageVTable[] RETAIL(D_002F4610);
    extern const GccVTableEntry g_LevelsPageVTable[] RETAIL(D_002F3FD0);
    extern const GccVTableEntry g_GalleryItemVTable[] RETAIL(D_002F42F0);
    extern const GccVTableEntry g_MovieItemVTable[] RETAIL(D_002F4598);
    extern const GccVTableEntry g_BossesPageVTable[] RETAIL(D_002F4548);
    extern const GccVTableEntry g_ConceptPageVTable[] RETAIL(D_002F44F8);
    extern const GccVTableEntry g_EnemiesPageVTable[] RETAIL(D_002F44A8);
    extern const GccVTableEntry g_MoviesPageVTable[] RETAIL(D_002F4458);
    extern const GccVTableEntry g_StoryboardsPageVTable[] RETAIL(D_002F4408);
    extern const GccVTableEntry g_UnseenPageVTable[] RETAIL(D_002F43B8);
    extern const GccVTableEntry g_ExtrasPageVTable[] RETAIL(D_002F4368);
    extern const GccVTableEntry g_PausePageVTable[] RETAIL(D_002F3EE0);
    extern const GccVTableEntry g_SaveGamePageVTable[] RETAIL(D_002F4138);
    extern const GccVTableEntry g_OptionsPageVTable[] RETAIL(D_002F3D00);
    extern const GccVTableEntry g_GameOptionsPageVTable[] RETAIL(D_002F3DF0);
    extern const GccVTableEntry g_GraphicsOptionsPageVTable[] RETAIL(D_002F3DA0);
    extern const GccVTableEntry g_SoundOptionsPageVTable[] RETAIL(D_002F3D50);
    extern const GccVTableEntry g_RestartItemVTable[] RETAIL(D_002F4020);
    extern const GccVTableEntry g_GameOverPageVTable[] RETAIL(D_002F4750);
    extern const GccVTableEntry g_DisableAutosavePageVTable[] RETAIL(D_002F4098);
    extern const GccVTableEntry g_QuitPageVTable[] RETAIL(D_002F3F30);
    extern const GccVTableEntry g_NoticePageVTable[] RETAIL(D_002F4700);
    extern const GccVTableEntry g_ScreenPositionPageVTable[] RETAIL(D_002F3E40);
    extern const GccVTableEntry g_MainMenuPageVTable[] RETAIL(D_002F4200);
    extern const GccVTableEntry g_NewGamePageVTable[] RETAIL(D_002F4250);
    extern const GccVTableEntry g_LoadGamePageVTable[] RETAIL(D_002F42A0);
    extern const GccVTableEntry g_ActionItemVTable[] RETAIL(D_002F4188);
}
