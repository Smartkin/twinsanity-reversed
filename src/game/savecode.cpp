#include "game/controllers.h"

#include "game/language.h"
#include "game/memory.h"
#include "game/savedevice.h"
#include "game/savemanager.h"
#include "game/string.h"
#include "retail/libc.h"

// The save code: the state machine the game saves and loads through. It asks its device for operations, steps them every frame,
// and shows screens of choices (on OLEG's pages) the player answers when a card is missing, full or unformatted, when a save
// slot is to be chosen and when an operation failed

extern "C"
{
    extern const GccVTableEntry g_SaveCodeVTable[] RETAIL(D_00306570);
    // The marks of the save's size and its name in the messages
    extern const char g_SaveSizeMark[] RETAIL(D_0030A340);
    extern const char g_SaveNameMark[] RETAIL(D_0030A348);
}

namespace
{
// The save code's operations: the card checked at the start (done when it's unformatted or has room for the save, else the screen
// of no room), the card checked (a due save stays due), a save to a slot the player chooses, which its screens offer to skip (3)
// or not (4, the pause menu's), with the card formatted and the save made first when the player asks, a load, and a save to the
// slot of the results (of every file when the save was just made: bit 5 of the results)
enum Operation : u32
{
    OperationCheckRoom = 1,
    OperationCheck = 2,
    OperationSave = 3,
    OperationPauseSave = 4,
    OperationLoad = 5,
    OperationSaveSlot = 6,
};

// The device's operations (game/savedevice.cpp): the waits while there's a card (10 to 12) keep the message of what was done on
// the screen
enum DeviceOperation : u32
{
    DeviceCheck = 1,
    DeviceFormat = 2,
    DeviceMeasure = 3,
    DeviceCreate = 4,
    DeviceWriteFolder = 5,
    DeviceReadFolder = 6,
    DeviceFind = 7,
    DeviceWriteFile = 8,
    DeviceReadFile = 9,
    DeviceWaitFormatted = 10,
    DeviceWaitSaved = 11,
    DeviceWaitLoaded = 12,
    DeviceWait = 13,
};

// The screens (their messages D_002E80E0 gives): none (the message of the operation running), no card at the start, no room at the
// start, the save made?, no card, unformatted, format?, a card with a save wanted, a card with room wanted, overwrite?, cancel the
// save?, the slots to save to and to load from, and the failures of formatting, saving and loading
enum Screen : u32
{
    ScreenMessage = 0,
    ScreenNoCard = 1,
    ScreenNoRoom = 2,
    ScreenCreate = 3,
    ScreenInsertCard = 4,
    ScreenUnformatted = 5,
    ScreenConfirmFormat = 6,
    ScreenInsertSave = 7,
    ScreenInsertRoom = 8,
    ScreenOverwrite = 9,
    ScreenCancelSave = 10,
    ScreenSaveSlots = 11,
    ScreenLoadSlots = 12,
    ScreenFormatFailed = 13,
    ScreenSaveFailed = 14,
    ScreenLoadFailed = 15,
};

// The player's answers (bits 16-19 of the bits): the first choice (a slot on the slots' screens), back, the third choice
enum Answer : u32
{
    AnswerFirst = 1,
    AnswerBack = 2,
    AnswerThird = 3,
};

// The device's results (bits 16-19 of its flags)
constexpr u32 DeviceFailed = 1;
constexpr u32 SlotMask = 0xF;
constexpr u32 SaveMessageCount = 38;
constexpr u32 KilobyteShift = 10;
constexpr u32 KilobyteRound = 0x3FF;

enum Slots : u32
{
    AskSlot = 1,
    ShowChoicesSlot = 2,
    ShowSlotsSlot = 3,
    OperationDoneSlot = 4,
    WaitingSlot = 5,
    FinishSlot = 6,
};

u32 AskDevice(SaveCode* code, u32 operation, u32 file)
{
    return CallVirtual<u32>(code, code->vtable, AskSlot, operation, file);
}

u32 ShowChoicesScreen(SaveCode* code, u32 screen)
{
    return CallVirtual<u32>(code, code->vtable, ShowChoicesSlot, screen);
}

u32 ShowSlotsScreen(SaveCode* code, u32 screen)
{
    return CallVirtual<u32>(code, code->vtable, ShowSlotsSlot, screen);
}

void TellOperationDone(SaveCode* code, u32 operation)
{
    CallVirtual<void>(code, code->vtable, OperationDoneSlot, operation);
}

void TellWaiting(SaveCode* code, u32 screen)
{
    CallVirtual<void>(code, code->vtable, WaitingSlot, screen);
}

void EndOperation(SaveCode* code, u32 flagged, u32 saveDue)
{
    CallVirtual<void>(code, code->vtable, FinishSlot, flagged, saveDue);
}

// Whether the save fits on the card: in the free space, or where the save is (the same size, or with the free space)
bool SaveFits(SaveDevice* device)
{
    u32 free = device->FreeBytes();
    u32 needed = device->NeededBytes();
    if (device->HasSave() == 0)
    {
        return needed <= free;
    }

    u32 saved = device->SavedBytes();
    return saved == needed || needed <= free + saved;
}

// The slot written: every file after the save was just made, else the slot's file and the folder's
void WriteSlot(SaveCode* code, u32 slot)
{
    if ((code->results & SaveCode::Fresh) != 0)
    {
        code->device->flags = (code->device->flags & ~SaveDevice::WritesEverything) | SaveDevice::WritesEverything;
        AskDevice(code, DeviceWriteFolder, 0);
    }
    else
    {
        AskDevice(code, DeviceWriteFile, slot);
    }

    code->results = (code->results & ~SlotMask) | slot;
}

// The slot acted on (bits 24-27 of the bits)
u32 ActedSlot(const SaveCode* code)
{
    return reinterpret_cast<const u8*>(&code->bits)[3] & SlotMask;
}

void SetActedSlot(SaveCode* code, u32 slot)
{
    code->bits = (code->bits & ~(SlotMask << SaveCode::SlotShift)) | (slot & SlotMask) << SaveCode::SlotShift;
}
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) SaveCode* SaveCode::Construct(SaveCode* code, const char* name,
                                                                                            SaveDevice* device)
{
    code->vtable = g_SaveCodeVTable;
    StringConstruct(&code->name, name);
    code->device = device;
    RetailLibc::MemorySet(code, 0, 8);
    code->bits &= ~OperationMask;
    code->SetScreen(ScreenMessage);
    for (u32 message = 0; message < SaveMessageCount; message++)
    {
        g_SaveMessageTexts[message] = -1;
    }

    return code;
}

void SaveCode::OperationDone(u32 operation)
{
    switch (Operation())
    {
    case OperationCheckRoom:
        if (device->HasCard() == 0)
        {
            ShowChoicesScreen(this, ScreenNoCard);
        }
        else if (device->CardFormatted() == 0)
        {
            EndOperation(this, 1, 0);
        }
        else if (operation != DeviceMeasure)
        {
            AskDevice(this, DeviceMeasure, 0);
        }
        else if (SaveFits(device))
        {
            EndOperation(this, 1, 0);
        }
        else
        {
            ShowChoicesScreen(this, ScreenNoRoom);
        }

        return;
    case OperationCheck:
        EndOperation(this, 1, bits >> 28 & 1);
        return;
    case OperationSave:
    case OperationPauseSave:
        switch (operation)
        {
        case DeviceFormat:
            AskDevice(this, DeviceWaitFormatted, 0);
            return;
        case DeviceMeasure:
            if (!SaveFits(device))
            {
                ShowChoicesScreen(this, ScreenInsertRoom);
            }
            else if (device->HasSave() != 0)
            {
                results &= ~Fresh;
                AskDevice(this, DeviceReadFolder, 0);
            }
            else
            {
                ShowChoicesScreen(this, ScreenCreate);
            }

            return;
        case DeviceCreate:
            ShowSlotsScreen(this, ScreenSaveSlots);
            return;
        case DeviceWriteFolder:
        {
            u32 previous = results;
            if ((previous & Fresh) != 0)
            {
                results = previous & ~Fresh;
                AskDevice(this, DeviceWriteFile, previous & SlotMask);
                results = (results & ~SlotMask) | (previous & SlotMask);
            }
            else
            {
                ShowSlotsScreen(this, ScreenSaveSlots);
            }

            return;
        }
        case DeviceReadFolder:
            ShowSlotsScreen(this, ScreenSaveSlots);
            return;
        case DeviceWriteFile:
            AskDevice(this, DeviceWaitSaved, 0);
            return;
        case DeviceWaitFormatted:
            AskDevice(this, DeviceCreate, 0);
            return;
        case DeviceWaitSaved:
            EndOperation(this, 1, 1);
            return;
        case DeviceWait:
            EndOperation(this, 0, 0);
            return;
        default:
            return;
        }
    case OperationLoad:
        switch (operation)
        {
        case DeviceMeasure:
            if (device->HasSave() != 0)
            {
                results &= ~Fresh;
                AskDevice(this, DeviceReadFolder, 0);
            }
            else
            {
                ShowChoicesScreen(this, ScreenInsertSave);
            }

            return;
        case DeviceReadFolder:
            ShowSlotsScreen(this, ScreenLoadSlots);
            return;
        case DeviceReadFile:
            results = (results & ~SlotMask) | ActedSlot(this);
            AskDevice(this, DeviceWaitLoaded, 0);
            return;
        case DeviceWaitLoaded:
            EndOperation(this, 1, 1);
            return;
        default:
            return;
        }
    case OperationSaveSlot:
        if (operation == DeviceWriteFile)
        {
            EndOperation(this, 1, 1);
        }

        return;
    default:
        return;
    }
}

u32 SaveCode::Waiting(u32 screen)
{
    if (screen == ScreenMessage)
    {
        TellOperationDone(this, device->Running());
        return 1;
    }

    if (screen != ScreenInsertCard || device->HasCard() == 0)
    {
        return 0;
    }

    u32 operation = Operation();
    if (device->CardFormatted() != 0)
    {
        AskDevice(this, DeviceMeasure, 0);
    }
    else if (operation == OperationSave || operation == OperationPauseSave)
    {
        ShowChoicesScreen(this, ScreenUnformatted);
    }
    else if (operation == OperationLoad)
    {
        ShowChoicesScreen(this, ScreenInsertSave);
    }

    return 1;
}

void SaveCode::Finish(u32 flagged, u32 saveDue)
{
    results = (results & ~Flagged) | (flagged & 1) << 4;
    u32 operation = bits & OperationMask;
    bits = (((bits & ~(OperationMask << AskedShift)) | operation << AskedShift) & ~OperationMask & ~SaveDue) | (saveDue & 1) << 28;
}

void SaveCode::Frame(TimeClock* clock)
{
    u32 running = device->Running();
    if ((bits & AnswerMask) != 0)
    {
        TakeAnswer();
    }

    if (device->Step(clock) != 0)
    {
        return;
    }

    u32 operation = Operation();
    u32 state = device->State();
    if (running == 0)
    {
        running = device->Running();
    }

    if (state == 0)
    {
        u32 screen = Screen();
        if (operation >= OperationSave && operation <= OperationLoad)
        {
            if (device->HasCard() != 0)
            {
                // Retail asks whether the card is formatted on the format failure's screen and drops the answer
                if (screen == ScreenFormatFailed)
                {
                    device->CardFormatted();
                }
            }
            else if (running < DeviceWaitSaved || running > DeviceWait)
            {
                if (screen != ScreenCancelSave && screen != ScreenInsertCard &&
                    (screen < ScreenFormatFailed || screen > ScreenLoadFailed))
                {
                    screen = ShowChoicesScreen(this, ScreenInsertCard);
                }
            }
        }

        TellWaiting(this, screen);
        return;
    }

    if (state != DeviceFailed)
    {
        return;
    }

    switch (running)
    {
    case DeviceCheck:
        if (operation != OperationCheckRoom)
        {
            EndOperation(this, 0, 0);
        }
        else
        {
            ShowChoicesScreen(this, ScreenNoCard);
        }

        return;
    case DeviceFormat:
        ShowChoicesScreen(this, ScreenFormatFailed);
        return;
    case DeviceMeasure:
        if (operation == OperationLoad)
        {
            ShowChoicesScreen(this, device->HasCard() != 0 ? ScreenLoadFailed : ScreenInsertCard);
        }
        else if (operation == OperationSave || operation == OperationPauseSave)
        {
            ShowChoicesScreen(this, device->HasCard() != 0 ? ScreenSaveFailed : ScreenInsertCard);
        }
        else if (operation == OperationSaveSlot)
        {
            EndOperation(this, 0, 0);
        }

        return;
    case DeviceCreate:
    case DeviceWriteFolder:
    case DeviceWriteFile:
    case DeviceReadFile:
    case DeviceWaitFormatted:
    case DeviceFind:
        if (operation == OperationLoad)
        {
            ShowChoicesScreen(this, ScreenLoadFailed);
        }
        else if (operation == OperationSave || operation == OperationPauseSave)
        {
            ShowChoicesScreen(this, ScreenSaveFailed);
        }
        else if (operation == OperationSaveSlot)
        {
            EndOperation(this, 0, 0);
        }

        return;
    case DeviceReadFolder:
        if (operation == OperationLoad)
        {
            ShowChoicesScreen(this, ScreenLoadFailed);
        }
        else if (operation == OperationSave || operation == OperationPauseSave)
        {
            ShowSlotsScreen(this, ScreenSaveSlots);
        }
        else if (operation == OperationSaveSlot)
        {
            EndOperation(this, 0, 0);
        }

        results |= Fresh;
        return;
    default:
        return;
    }
}

void SaveCode::TakeAnswer()
{
    u32 screen = Screen();
    SetScreen(ScreenMessage);
    u32 operation = Operation();
    u32 answer = bits >> AnswerShift & 0xF;
    bits &= ~AnswerMask;
    u32 chosen = bits >> ChosenShift & SlotMask;
    if (answer == AnswerFirst)
    {
        switch (screen)
        {
        case ScreenNoCard:
        case ScreenNoRoom:
            AskDevice(this, DeviceCheck, 0);
            return;
        case ScreenCreate:
            AskDevice(this, DeviceCreate, 0);
            return;
        case ScreenUnformatted:
            ShowChoicesScreen(this, ScreenConfirmFormat);
            return;
        case ScreenConfirmFormat:
            AskDevice(this, DeviceFormat, 0);
            return;
        case ScreenOverwrite:
            WriteSlot(this, ActedSlot(this));
            return;
        case ScreenCancelSave:
            AskDevice(this, DeviceWait, 0);
            return;
        case ScreenSaveSlots:
        {
            FolderSummary* summary = static_cast<FolderFile*>(device->mainFile)->summaries[chosen];
            SetActedSlot(this, chosen);
            if (summary->HasSave())
            {
                ShowChoicesScreen(this, ScreenOverwrite);
                return;
            }

            GetSaveDate(&summary->date);
            WriteSlot(this, ActedSlot(this));
            return;
        }
        case ScreenLoadSlots:
            SetActedSlot(this, chosen);
            AskDevice(this, DeviceReadFile, chosen);
            return;
        case ScreenFormatFailed:
        case ScreenSaveFailed:
        case ScreenLoadFailed:
            if (device->HasCard() == 0)
            {
                ShowChoicesScreen(this, ScreenInsertCard);
            }
            else if (device->CardFormatted() != 0)
            {
                AskDevice(this, DeviceMeasure, 0);
            }
            else if (screen == ScreenSaveFailed)
            {
                ShowChoicesScreen(this, ScreenUnformatted);
            }
            else if (screen == ScreenFormatFailed)
            {
                ShowChoicesScreen(this, ScreenConfirmFormat);
            }
            else
            {
                ShowChoicesScreen(this, ScreenInsertSave);
            }

            return;
        default:
            return;
        }
    }

    if (answer == AnswerBack)
    {
        switch (screen)
        {
        case ScreenCreate:
        case ScreenInsertCard:
        case ScreenUnformatted:
        case ScreenInsertSave:
        case ScreenInsertRoom:
        case ScreenSaveSlots:
        case ScreenLoadSlots:
        case ScreenFormatFailed:
        case ScreenSaveFailed:
        case ScreenLoadFailed:
            if (operation == OperationSave || operation == OperationPauseSave)
            {
                ShowChoicesScreen(this, ScreenCancelSave);
            }
            else
            {
                EndOperation(this, 0, 0);
            }

            return;
        case ScreenConfirmFormat:
            ShowChoicesScreen(this, ScreenUnformatted);
            return;
        case ScreenOverwrite:
            ShowSlotsScreen(this, ScreenSaveSlots);
            return;
        case ScreenCancelSave:
            if (device->HasCard() == 0)
            {
                ShowChoicesScreen(this, ScreenInsertCard);
            }
            else if (device->CardFormatted() != 0)
            {
                AskDevice(this, DeviceMeasure, 0);
            }
            else if (operation == OperationSave || operation == OperationPauseSave)
            {
                ShowChoicesScreen(this, ScreenUnformatted);
            }
            else if (operation == OperationLoad)
            {
                ShowChoicesScreen(this, ScreenInsertSave);
            }

            return;
        default:
            return;
        }
    }

    bool thirdEnds = screen < ScreenOverwrite || (screen >= ScreenSaveSlots && screen <= ScreenLoadFailed);
    if (answer == AnswerThird && screen != ScreenMessage && thirdEnds)
    {
        EndOperation(this, 1, 0);
    }
}

const char* SaveCode::Message(s32 message)
{
    return SaveMessage(message);
}

void SaveCode::MessageText(s32 message, String* text)
{
    u32 kilobytes = (device->NeededBytes() + KilobyteRound) >> KilobyteShift;
    String size = {nullptr, 0, 0};
    String number;
    StringConstructNumber(&number, kilobytes);
    StringAppend(&size, number.string);
    StringDestroy(&number);
    StringAssign(text, Message(message));
    while (StringReplace(text, g_SaveSizeMark, size.string))
    {
    }

    while (StringReplace(text, g_SaveNameMark, name.string))
    {
    }

    StringDestroy(&size);
}

extern "C"
{
    void SaveManagerRequest(SaveManager* manager, u32 operation, s32 time)
    {
        s32 asked = static_cast<s32>(operation);
        manager->results &= ~SaveCode::Flagged;
        manager->bits = (manager->bits & 0xEFFFFF00) | (operation & SaveCode::OperationMask) |
                        (operation & SaveCode::OperationMask) << SaveCode::AskedShift;
        manager->device->wait = time;
        if (asked == 0)
        {
            return;
        }

        if (asked > 0 && asked < static_cast<s32>(OperationSave))
        {
            AskDevice(manager, DeviceCheck, 0);
            manager->SetScreen(ScreenMessage);
            return;
        }

        if (asked == static_cast<s32>(OperationSaveSlot))
        {
            WriteSlot(manager, manager->results & SlotMask);
            return;
        }

        if (manager->device->HasCard() == 0)
        {
            ShowChoicesScreen(manager, ScreenInsertCard);
        }
        else if (manager->device->CardFormatted() != 0)
        {
            AskDevice(manager, DeviceMeasure, 0);
        }
        else if (operation == OperationSave || operation == OperationPauseSave)
        {
            ShowChoicesScreen(manager, ScreenUnformatted);
        }
        else if (operation == OperationLoad)
        {
            ShowChoicesScreen(manager, ScreenInsertSave);
        }
    }

    void DestroySaveCode(SaveCode* code, u32 destroyFlags)
    {
        code->vtable = g_SaveCodeVTable;
        StringDestroy(&code->name);
        if ((destroyFlags & 1) != 0)
        {
            MemoryDeallocate2_(code);
        }
    }
}
