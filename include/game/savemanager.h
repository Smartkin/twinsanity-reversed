#pragma once

#include "common.h"
#include "game/controllers.h"
#include "game/language.h"
#include "game/oleg.h"
#include "game/savedevice.h"
#include "game/string.h"

class MenuPage;
class Stream;
struct GameController;
struct SaveController;
struct BankFile;
class OLEG;
class StringLabel;

// The save code's operations (its bits' operation and asked; SaveManagerRequest asks for one): the card checked at the start (done
// when it's unformatted or has room for the save, else the screen of no room), the card checked for (done while it's there: the
// game controller asks every quarter of a second while it autosaves), a save to a slot the player chooses, which its screens offer
// to skip (a new game's) or not (the pause menu's), with the card formatted and the save made first when the player asks, a load,
// and a save to the slot of the results (the checkpoints' autosave; every file when the save was just made: the results' fresh)
enum SaveCodeOperation : u32
{
    SaveOperationNone = 0,
    SaveOperationCheckRoom = 1,
    SaveOperationCheckInserted = 2,
    SaveOperationNewGameSave = 3,
    SaveOperationPauseSave = 4,
    SaveOperationLoad = 5,
    SaveOperationAutosave = 6,
};

// The save code's screens (its bits' screen; their messages g_ScreenMessages gives): none (the message of the device's operation
// running), no card at the start, no room at the start, the save made?, no card, unformatted, format?, a card with a save wanted,
// a card with room wanted, overwrite?, cancel the save?, the slots to save to and to load from, and the failures of formatting,
// saving and loading
enum SaveCodeScreen : u32
{
    SaveScreenMessage = 0,
    SaveScreenNoCard = 1,
    SaveScreenNoRoom = 2,
    SaveScreenCreate = 3,
    SaveScreenInsertCard = 4,
    SaveScreenUnformatted = 5,
    SaveScreenConfirmFormat = 6,
    SaveScreenInsertSave = 7,
    SaveScreenInsertRoom = 8,
    SaveScreenOverwrite = 9,
    SaveScreenCancelSave = 10,
    SaveScreenSaveSlots = 11,
    SaveScreenLoadSlots = 12,
    SaveScreenFormatFailed = 13,
    SaveScreenSaveFailed = 14,
    SaveScreenLoadFailed = 15,
    SaveScreenCount = 16,
};

// The save code's messages the screens' choices name (g_SaveMessages' English, g_SaveMessageTexts' game texts): a slot without
// a save, "Cancel", "Format", "Continue", "Continue wihout saving", "Create save", "Retry", "Yes" and "No"
enum SaveMessageId : s32
{
    SaveMessageEmpty = 0x1D,
    SaveMessageCancel = 0x1E,
    SaveMessageFormat = 0x1F,
    SaveMessageContinue = 0x20,
    SaveMessageLeave = 0x21,
    SaveMessageCreate = 0x22,
    SaveMessageRetry = 0x23,
    SaveMessageYes = 0x24,
    SaveMessageNo = 0x25,
    SaveMessageCount = 38,
};

// The save manager's mode of a screen of choices (SaveChoicesPage::ShowItems, -1 none): the screens of choices in their order,
// the save slots' left out, and one with only "Cancel" no screen has
enum SaveChoicesMode : s32
{
    ChoicesHidden = -1,
    ChoicesNoCard = 0,
    ChoicesNoRoom = 1,
    ChoicesCreate = 2,
    ChoicesInsertCard = 3,
    ChoicesUnformatted = 4,
    ChoicesConfirmFormat = 5,
    ChoicesInsertSave = 6,
    ChoicesInsertRoom = 7,
    ChoicesOverwrite = 8,
    ChoicesCancelSave = 9,
    ChoicesFormatFailed = 10,
    ChoicesSaveFailed = 11,
    ChoicesLoadFailed = 12,
    ChoicesCancelOnly = 13,
};

// The save manager's mode of the save slots' screens (SaveCodePage::ShowSlotItems, -1 none): loading, saving
enum SaveSlotsMode : s32
{
    SlotsPageHidden = -1,
    SlotsPageLoad = 0,
    SlotsPageSave = 1,
};

// The save manager (the retail UnkStruct_0x5C0, 0x5C0 bytes, that g_SaveManager points to): the save code updated and drawn every
// frame, it saves and loads through OLEG's screens (the ones it hides and shows), its texts (their strings its messages) and its
// two pages
struct SaveManager : SaveCode
{
    // Made for the game controller, and its vtable's slot 7 (D_002F5120: 8 its update, 9 its drawing)
    static SaveManager* Construct(SaveManager* manager, GameController* controller) RETAIL(FUN_00169028);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00169238);
    // Its vtable's slot 10, which nothing calls: nothing
    void Nothing10() RETAIL(FUN_00179e18);

    // Its base's (D_00306470, the save code on OLEG's screens) functions of its vtable: an operation asked of the device with its
    // message on the message screen, the choices screen and the save slots screen shown (the screen returned), the base's
    // destructor and drawing (nothing: OLEG draws the screens)
    u32 Ask(u32 operation, u32 file) RETAIL(FUN_0029fe18);
    u32 ShowChoices(u32 screen) RETAIL(FUN_0029ff50);
    u32 ShowSlots(u32 screen) RETAIL(FUN_002a00e8);
    void DestroyScreens(u32 destroyFlags) RETAIL(FUN_002a7950);
    void Draw(Renderer* renderer) RETAIL(FUN_002a7a90);

    // The save code's operation running (SaveOperationNone none)
    u32 RunningOperation() const
    {
        return bits.operation;
    }

    OLEG* oleg;
    // OLEG's screens it switches between (OLEG::StartUp's): every widget hidden (before another is shown), its message, its
    // choices and its save slots
    s32 screens[4];
    // The texts of the message, the choices' title and the save slots' title (its constructor makes them, the manager's
    // destructor lets them go), and their labels on OLEG's screens
    String strings[3];
    StringLabel* messages[3];
    MenuPage* choicesPage;
    MenuPage* slotsPage;
    // The save code's parts: the icon.sys file ("Crash~Twinsanity", "Crash.ico"), the save's icon files (icon.sys and the icon,
    // StartUp\Crash.ico), the card folder's file ("BESLES-52568", its summaries the four banks') and the memory card (its files the
    // four banks)
    IconSysFile iconSys;
    SaveIconFiles iconFiles;
    FolderFile folder;
    u8 unused4A4[0x4C0 - 0x4A4];
    SaveDevice memoryCard;
    // Its four bank files
    BankFile** banks;
    u8 unused584[0x3C];
};
CHECK_SIZE(SaveManager, 0x5C0);
CHECK_OFFSET(SaveManager, screens, 0x20);
CHECK_OFFSET(SaveManager, messages, 0x54);
CHECK_OFFSET(SaveManager, slotsPage, 0x64);
CHECK_OFFSET(SaveManager, iconSys, 0x68);
CHECK_OFFSET(SaveManager, iconFiles, 0x464);
CHECK_OFFSET(SaveManager, folder, 0x470);
CHECK_OFFSET(SaveManager, memoryCard, 0x4C0);
CHECK_OFFSET(SaveManager, banks, 0x580);

// A save slot's file on the card (retail's vtable D_002F5200 over the save code's file D_00306698, 0x50 bytes): "Bank<n>.bin"
// (0xF400 bytes), its summary and the game controller. Its vtable: 2 before a save, 3 and 4 (FUN_00179b58, FUN_00179b78), 5 the
// destructor (FUN_00179b10), 6-9 the save code's file's
struct BankFile : SaveFile
{
    SaveSummary summary;
    GameController* controller;

    // Before a save: the save controller's options, volumes and screen position taken from the game, the summary made from them
    void GatherSettings() RETAIL(FUN_001687a8);
    // The save controller's data read from the file and written into it
    void ReadData(Stream* stream) RETAIL(FUN_00179b58);
    void WriteData(Stream* stream) RETAIL(FUN_00179b78);
    // The summary destroyed (its folder summary's part), then the save code's file
    void Destroy(u32 destroyFlags) RETAIL(FUN_00179b10);
};
CHECK_OFFSET(BankFile, vtable, 0x28);
CHECK_OFFSET(BankFile, summary, 0x2C);
CHECK_SIZE(BankFile, 0x50);

extern "C"
{
    // The save code's messages' texts (numbers of the game's texts), -1 for the save code's own (English) string
    extern s32 g_SaveMessageTexts[SaveMessageCount] RETAIL(D_003D1D88);
    extern const char* const g_SaveMessages[] RETAIL(D_002E8010);
    // The messages of the device's operations (the waits' the outcomes: "Format successful!", "Save successful!", "Load
    // successful!", "Save cancelled!") and of the save code's screens
    extern const s32 g_OperationMessages[] RETAIL(D_002E80A8);
    extern const s32 g_ScreenMessages[] RETAIL(D_002E80E0);

    // An operation of the save code asked for, with a time (clock units) the device waits before it asks how a request went
    void SaveManagerRequest(SaveManager* manager, u32 operation, s32 time) RETAIL(FUN_002a1a48);
    // A bank's summary (oleg.h's SaveSummary) made (the folder's kind of summary, no save, no time), and made from the save
    // controller's settings: the area, the character, the lives, the crystals, how much is done and the time played, its date
    // taken again
    void ConstructBankSummary(void* summary) RETAIL(FUN_00179ba0);
    void MakeBankSummary(void* summary, SaveController* settings) RETAIL(FUN_00179be8);
    // The save code on OLEG's screens made with its name, OLEG and its device (its screens none yet), and the save code's
    // destructor
    void ConstructSaveCode(SaveCode* code, const char* name, OLEG* oleg, void* device) RETAIL(FUN_002a79d0);
    void DestroySaveCode(SaveCode* code, u32 destroyFlags) RETAIL(FUN_002a7f50);
}

// A message of the save code's: the game text it's given, or its own
inline const char* SaveMessage(s32 message)
{
    s32 text = g_SaveMessageTexts[message];
    if (text != -1)
    {
        return GameText(text);
    }

    return g_SaveMessages[message];
}
