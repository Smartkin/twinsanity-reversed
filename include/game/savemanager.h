#pragma once

#include "common.h"
#include "game/controllers.h"
#include "game/language.h"
#include "game/string.h"

class MenuPage;
class Stream;
struct GameController;
struct SaveController;
struct BankFile;
class OLEG;
class StringLabel;

// The save manager (the retail UnkStruct_0x5C0, 0x5C0 bytes, that G_UnkStruct_5C0 points to): the save code updated
// and drawn every frame, it saves and loads through OLEG's screens (the ones it hides and shows), its texts (their strings its messages) and its
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

    // The save code's operation running (0 none)
    u32 Step() const
    {
        return bits & OperationMask;
    }

    OLEG* oleg;
    s32 screens[4];
    // The save code's strings (its constructor makes them, the manager's destructor lets them go)
    String strings[3];
    StringLabel* messages[3];
    MenuPage* choicesPage;
    MenuPage* slotsPage;
    // The save code's parts: the icon.sys file ("Crash~Twinsanity", "Crash.ico"), the save's file list (the icon and
    // StartUp\Crash.ico), the card folder ("BESLES-52568"; its four banks' summaries 0x30 in) and the card slots (the four bank
    // files 0x18 in)
    u8 icon[0x3FC];
    u8 files[0xC];
    u8 folder[0x50];
    u8 slots[0xC0];
    // Its four bank files
    BankFile** banks;
    u8 unknown584[0x3C];
};
CHECK_SIZE(SaveManager, 0x5C0);
CHECK_OFFSET(SaveManager, screens, 0x20);
CHECK_OFFSET(SaveManager, messages, 0x54);
CHECK_OFFSET(SaveManager, slotsPage, 0x64);
CHECK_OFFSET(SaveManager, files, 0x464);
CHECK_OFFSET(SaveManager, folder, 0x470);
CHECK_OFFSET(SaveManager, slots, 0x4C0);
CHECK_OFFSET(SaveManager, banks, 0x580);

// A save slot's file on the card (retail's vtable D_002F5200 at 0x28 over the save code's file D_00306698, 0x50 bytes):
// "Bank<n>.bin" (the save code's file's 0x28 bytes: its name, its size 0xF400 at 0x20, its buffer at 0x24), its summary (oleg.h's
// SaveSummary) and the game controller. Its vtable: 2 before a save, 3 and 4 (FUN_00179b58, FUN_00179b78), 5 the destructor
// (FUN_00179b10), 6-9 the save code's file's
struct BankFile
{
    u8 unknown00[0x28];
    const GccVTableEntry* vtable;
    u8 summary[0x20];
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
CHECK_SIZE(BankFile, 0x50);

extern "C"
{
    // The save code's messages' texts (numbers of the game's texts), -1 for the save code's own (English) string
    extern s32 g_SaveMessageTexts[38] RETAIL(D_003D1D88);
    extern const char* const g_SaveMessages[] RETAIL(D_002E8010);
    // The messages of the device's operations and of the save code's screens
    extern const s32 g_OperationMessages[] RETAIL(D_002E80A8);
    extern const s32 g_ScreenMessages[] RETAIL(D_002E80E0);

    // An operation of the save code asked for, with a time (clock units) the device waits before it asks how a request went
    void SaveManagerRequest(SaveManager* manager, u32 operation, s32 time) RETAIL(FUN_002a1a48);
    // A bank's summary (oleg.h's SaveSummary) made (the folder's kind of summary, no save, no time), and made from the save
    // controller's settings: the area, the character, the lives, the crystals, how much is done and the time played, its date
    // taken again
    void ConstructBankSummary(void* summary) RETAIL(FUN_00179ba0);
    void MakeBankSummary(void* summary, SaveController* settings) RETAIL(FUN_00179be8);
    // The save code on OLEG's screens made with its name, OLEG and its card slots (its screens none yet), and the save code's
    // destructor
    void ConstructSaveCode(SaveCode* code, const char* name, OLEG* oleg, void* slots) RETAIL(FUN_002a79d0);
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
