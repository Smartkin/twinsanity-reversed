#pragma once

#include "common.h"
#include "game/controllers.h"

class MenuPage;
struct GameController;
class OLEG;
class StringLabel;

// The save manager (the retail UnkStruct_0x5C0, 0x5C0 bytes, that G_UnkStruct_5C0 points to; still asm): the save code updated
// and drawn every frame, it saves and loads through OLEG's screens (the ones it hides and shows), its texts (their strings its messages) and its
// two pages
struct SaveManager : SaveCode
{
    // Made for the game controller (still asm)
    static SaveManager* Construct(SaveManager* manager, GameController* controller) RETAIL(FUN_00169028);

    // The save code's operation running (0 none)
    u32 Step() const
    {
        return bits & OperationMask;
    }

    OLEG* oleg;
    s32 screens[4];
    u8 unknown30[0x24];
    StringLabel* messages[3];
    MenuPage* choicesPage;
    MenuPage* slotsPage;
    u8 unknown68[0x5C0 - 0x68];
};
CHECK_SIZE(SaveManager, 0x5C0);
CHECK_OFFSET(SaveManager, screens, 0x20);
CHECK_OFFSET(SaveManager, messages, 0x54);
CHECK_OFFSET(SaveManager, slotsPage, 0x64);

extern "C"
{
    // The save code's messages' texts (numbers of the game's texts), -1 for the save code's own (English) string
    extern s32 g_SaveMessageTexts[38] RETAIL(D_003D1D88);

    // An operation of the save code asked for, with a time (clock units) it waits for (still asm)
    void SaveManagerRequest(SaveManager* manager, u32 operation, s32 time) RETAIL(FUN_002a1a48);
}
