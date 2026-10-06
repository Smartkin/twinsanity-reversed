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
// Bytes rounded up to kilobytes
constexpr u32 KilobyteRound = (1u << SaveDevice::KilobyteShift) - 1;

u32 AskDevice(SaveCode* code, u32 operation, u32 file)
{
    return CallVirtual<u32>(code, code->vtable, SaveCode::AskSlot, operation, file);
}

u32 ShowChoicesScreen(SaveCode* code, u32 screen)
{
    return CallVirtual<u32>(code, code->vtable, SaveCode::ShowChoicesSlot, screen);
}

u32 ShowSlotsScreen(SaveCode* code, u32 screen)
{
    return CallVirtual<u32>(code, code->vtable, SaveCode::ShowSlotsSlot, screen);
}

void TellOperationDone(SaveCode* code, u32 operation)
{
    CallVirtual<void>(code, code->vtable, SaveCode::OperationDoneSlot, operation);
}

void TellWaiting(SaveCode* code, u32 screen)
{
    CallVirtual<void>(code, code->vtable, SaveCode::WaitingSlot, screen);
}

void EndOperation(SaveCode* code, u32 flagged, u32 saveDue)
{
    CallVirtual<void>(code, code->vtable, SaveCode::FinishSlot, flagged, saveDue);
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
    if (code->results.fresh != 0)
    {
        code->device->flags.writesEverything = 1;
        AskDevice(code, SaveDevice::OperationWriteFolder, 0);
    }
    else
    {
        AskDevice(code, SaveDevice::OperationWriteFile, slot);
    }

    code->results.slot = slot;
}
}

KEEP_LOOPS SaveCode* SaveCode::Construct(SaveCode* code, const char* name, SaveDevice* device)
{
    code->vtable = g_SaveCodeVTable;
    StringConstruct(&code->name, name);
    code->device = device;
    RetailLibc::MemorySet(code, 0, sizeof(SaveCodeBits) + sizeof(SaveCodeResults));
    code->bits.operation = SaveOperationNone;
    code->bits.screen = SaveScreenMessage;
    for (u32 message = 0; message < SaveMessageCount; message++)
    {
        g_SaveMessageTexts[message] = -1;
    }

    return code;
}

void SaveCode::OperationDone(u32 operation)
{
    switch (bits.operation)
    {
    case SaveOperationCheckRoom:
        if (device->HasCard() == 0)
        {
            ShowChoicesScreen(this, SaveScreenNoCard);
        }
        else if (device->CardFormatted() == 0)
        {
            EndOperation(this, 1, 0);
        }
        else if (operation != SaveDevice::OperationMeasure)
        {
            AskDevice(this, SaveDevice::OperationMeasure, 0);
        }
        else if (SaveFits(device))
        {
            EndOperation(this, 1, 0);
        }
        else
        {
            ShowChoicesScreen(this, SaveScreenNoRoom);
        }

        return;
    case SaveOperationCheckInserted:
        EndOperation(this, 1, bits.saveDue);
        return;
    case SaveOperationNewGameSave:
    case SaveOperationPauseSave:
        switch (operation)
        {
        case SaveDevice::OperationFormat:
            AskDevice(this, SaveDevice::OperationWaitFormatted, 0);
            return;
        case SaveDevice::OperationMeasure:
            if (!SaveFits(device))
            {
                ShowChoicesScreen(this, SaveScreenInsertRoom);
            }
            else if (device->HasSave() != 0)
            {
                results.fresh = 0;
                AskDevice(this, SaveDevice::OperationReadFolder, 0);
            }
            else
            {
                ShowChoicesScreen(this, SaveScreenCreate);
            }

            return;
        case SaveDevice::OperationCreate:
            ShowSlotsScreen(this, SaveScreenSaveSlots);
            return;
        case SaveDevice::OperationWriteFolder:
        {
            SaveCodeResults previous = results;
            if (previous.fresh != 0)
            {
                results.fresh = 0;
                AskDevice(this, SaveDevice::OperationWriteFile, previous.slot);
                results.slot = previous.slot;
            }
            else
            {
                ShowSlotsScreen(this, SaveScreenSaveSlots);
            }

            return;
        }
        case SaveDevice::OperationReadFolder:
            ShowSlotsScreen(this, SaveScreenSaveSlots);
            return;
        case SaveDevice::OperationWriteFile:
            AskDevice(this, SaveDevice::OperationWaitSaved, 0);
            return;
        case SaveDevice::OperationWaitFormatted:
            AskDevice(this, SaveDevice::OperationCreate, 0);
            return;
        case SaveDevice::OperationWaitSaved:
            EndOperation(this, 1, 1);
            return;
        case SaveDevice::OperationWaitCancelled:
            EndOperation(this, 0, 0);
            return;
        default:
            return;
        }
    case SaveOperationLoad:
        switch (operation)
        {
        case SaveDevice::OperationMeasure:
            if (device->HasSave() != 0)
            {
                results.fresh = 0;
                AskDevice(this, SaveDevice::OperationReadFolder, 0);
            }
            else
            {
                ShowChoicesScreen(this, SaveScreenInsertSave);
            }

            return;
        case SaveDevice::OperationReadFolder:
            ShowSlotsScreen(this, SaveScreenLoadSlots);
            return;
        case SaveDevice::OperationReadFile:
            results.slot = bits.targetSlot;
            AskDevice(this, SaveDevice::OperationWaitLoaded, 0);
            return;
        case SaveDevice::OperationWaitLoaded:
            EndOperation(this, 1, 1);
            return;
        default:
            return;
        }
    case SaveOperationAutosave:
        if (operation == SaveDevice::OperationWriteFile)
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
    if (screen == SaveScreenMessage)
    {
        TellOperationDone(this, device->flags.running);
        return 1;
    }

    if (screen != SaveScreenInsertCard || device->HasCard() == 0)
    {
        return 0;
    }

    u32 operation = bits.operation;
    if (device->CardFormatted() != 0)
    {
        AskDevice(this, SaveDevice::OperationMeasure, 0);
    }
    else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
    {
        ShowChoicesScreen(this, SaveScreenUnformatted);
    }
    else if (operation == SaveOperationLoad)
    {
        ShowChoicesScreen(this, SaveScreenInsertSave);
    }

    return 1;
}

void SaveCode::Finish(u32 flagged, u32 saveDue)
{
    results.flagged = flagged;
    bits.asked = bits.operation;
    bits.operation = SaveOperationNone;
    bits.saveDue = saveDue;
}

void SaveCode::Frame(TimeClock* clock)
{
    u32 running = device->flags.running;
    if (bits.answer != AnswerNone)
    {
        TakeAnswer();
    }

    if (device->Step(clock) != 0)
    {
        return;
    }

    u32 operation = bits.operation;
    u32 state = device->flags.state;
    if (running == SaveDevice::OperationNone)
    {
        running = device->flags.running;
    }

    if (state == SaveDevice::StateOk)
    {
        u32 screen = bits.screen;
        if (operation >= SaveOperationNewGameSave && operation <= SaveOperationLoad)
        {
            if (device->HasCard() != 0)
            {
                // Retail asks whether the card is formatted on the format failure's screen and drops the answer
                if (screen == SaveScreenFormatFailed)
                {
                    device->CardFormatted();
                }
            }
            else if (running < SaveDevice::OperationWaitSaved || running > SaveDevice::OperationWaitCancelled)
            {
                if (screen != SaveScreenCancelSave && screen != SaveScreenInsertCard &&
                    (screen < SaveScreenFormatFailed || screen > SaveScreenLoadFailed))
                {
                    screen = ShowChoicesScreen(this, SaveScreenInsertCard);
                }
            }
        }

        TellWaiting(this, screen);
        return;
    }

    if (state != SaveDevice::StateFailed)
    {
        return;
    }

    switch (running)
    {
    case SaveDevice::OperationCheck:
        if (operation != SaveOperationCheckRoom)
        {
            EndOperation(this, 0, 0);
        }
        else
        {
            ShowChoicesScreen(this, SaveScreenNoCard);
        }

        return;
    case SaveDevice::OperationFormat:
        ShowChoicesScreen(this, SaveScreenFormatFailed);
        return;
    case SaveDevice::OperationMeasure:
        if (operation == SaveOperationLoad)
        {
            ShowChoicesScreen(this, device->HasCard() != 0 ? SaveScreenLoadFailed : SaveScreenInsertCard);
        }
        else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
        {
            ShowChoicesScreen(this, device->HasCard() != 0 ? SaveScreenSaveFailed : SaveScreenInsertCard);
        }
        else if (operation == SaveOperationAutosave)
        {
            EndOperation(this, 0, 0);
        }

        return;
    case SaveDevice::OperationCreate:
    case SaveDevice::OperationWriteFolder:
    case SaveDevice::OperationWriteFile:
    case SaveDevice::OperationReadFile:
    case SaveDevice::OperationWaitFormatted:
    case SaveDevice::OperationFind:
        if (operation == SaveOperationLoad)
        {
            ShowChoicesScreen(this, SaveScreenLoadFailed);
        }
        else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
        {
            ShowChoicesScreen(this, SaveScreenSaveFailed);
        }
        else if (operation == SaveOperationAutosave)
        {
            EndOperation(this, 0, 0);
        }

        return;
    case SaveDevice::OperationReadFolder:
        if (operation == SaveOperationLoad)
        {
            ShowChoicesScreen(this, SaveScreenLoadFailed);
        }
        else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
        {
            ShowSlotsScreen(this, SaveScreenSaveSlots);
        }
        else if (operation == SaveOperationAutosave)
        {
            EndOperation(this, 0, 0);
        }

        results.fresh = 1;
        return;
    default:
        return;
    }
}

void SaveCode::TakeAnswer()
{
    u32 screen = bits.screen;
    bits.screen = SaveScreenMessage;
    u32 operation = bits.operation;
    u32 answer = bits.answer;
    bits.answer = AnswerNone;
    u32 chosen = bits.chosen;
    if (answer == AnswerFirst)
    {
        switch (screen)
        {
        case SaveScreenNoCard:
        case SaveScreenNoRoom:
            AskDevice(this, SaveDevice::OperationCheck, 0);
            return;
        case SaveScreenCreate:
            AskDevice(this, SaveDevice::OperationCreate, 0);
            return;
        case SaveScreenUnformatted:
            ShowChoicesScreen(this, SaveScreenConfirmFormat);
            return;
        case SaveScreenConfirmFormat:
            AskDevice(this, SaveDevice::OperationFormat, 0);
            return;
        case SaveScreenOverwrite:
            WriteSlot(this, bits.targetSlot);
            return;
        case SaveScreenCancelSave:
            AskDevice(this, SaveDevice::OperationWaitCancelled, 0);
            return;
        case SaveScreenSaveSlots:
        {
            FolderSummary* summary = device->folder->summaries[chosen];
            bits.targetSlot = chosen;
            if (summary->HasSave())
            {
                ShowChoicesScreen(this, SaveScreenOverwrite);
                return;
            }

            GetSaveDate(&summary->date);
            WriteSlot(this, bits.targetSlot);
            return;
        }
        case SaveScreenLoadSlots:
            bits.targetSlot = chosen;
            AskDevice(this, SaveDevice::OperationReadFile, chosen);
            return;
        case SaveScreenFormatFailed:
        case SaveScreenSaveFailed:
        case SaveScreenLoadFailed:
            if (device->HasCard() == 0)
            {
                ShowChoicesScreen(this, SaveScreenInsertCard);
            }
            else if (device->CardFormatted() != 0)
            {
                AskDevice(this, SaveDevice::OperationMeasure, 0);
            }
            else if (screen == SaveScreenSaveFailed)
            {
                ShowChoicesScreen(this, SaveScreenUnformatted);
            }
            else if (screen == SaveScreenFormatFailed)
            {
                ShowChoicesScreen(this, SaveScreenConfirmFormat);
            }
            else
            {
                ShowChoicesScreen(this, SaveScreenInsertSave);
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
        case SaveScreenCreate:
        case SaveScreenInsertCard:
        case SaveScreenUnformatted:
        case SaveScreenInsertSave:
        case SaveScreenInsertRoom:
        case SaveScreenSaveSlots:
        case SaveScreenLoadSlots:
        case SaveScreenFormatFailed:
        case SaveScreenSaveFailed:
        case SaveScreenLoadFailed:
            if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
            {
                ShowChoicesScreen(this, SaveScreenCancelSave);
            }
            else
            {
                EndOperation(this, 0, 0);
            }

            return;
        case SaveScreenConfirmFormat:
            ShowChoicesScreen(this, SaveScreenUnformatted);
            return;
        case SaveScreenOverwrite:
            ShowSlotsScreen(this, SaveScreenSaveSlots);
            return;
        case SaveScreenCancelSave:
            if (device->HasCard() == 0)
            {
                ShowChoicesScreen(this, SaveScreenInsertCard);
            }
            else if (device->CardFormatted() != 0)
            {
                AskDevice(this, SaveDevice::OperationMeasure, 0);
            }
            else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
            {
                ShowChoicesScreen(this, SaveScreenUnformatted);
            }
            else if (operation == SaveOperationLoad)
            {
                ShowChoicesScreen(this, SaveScreenInsertSave);
            }

            return;
        default:
            return;
        }
    }

    bool thirdEnds = screen < SaveScreenOverwrite || (screen >= SaveScreenSaveSlots && screen <= SaveScreenLoadFailed);
    if (answer == AnswerThird && screen != SaveScreenMessage && thirdEnds)
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
    u32 kilobytes = (device->NeededBytes() + KilobyteRound) >> SaveDevice::KilobyteShift;
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
        manager->results.flagged = 0;
        manager->bits.operation = operation;
        manager->bits.asked = operation;
        manager->bits.saveDue = 0;
        manager->device->wait = time;
        if (asked == static_cast<s32>(SaveOperationNone))
        {
            return;
        }

        // The checks
        if (asked > 0 && asked < static_cast<s32>(SaveOperationNewGameSave))
        {
            AskDevice(manager, SaveDevice::OperationCheck, 0);
            manager->bits.screen = SaveScreenMessage;
            return;
        }

        if (asked == static_cast<s32>(SaveOperationAutosave))
        {
            WriteSlot(manager, manager->results.slot);
            return;
        }

        if (manager->device->HasCard() == 0)
        {
            ShowChoicesScreen(manager, SaveScreenInsertCard);
        }
        else if (manager->device->CardFormatted() != 0)
        {
            AskDevice(manager, SaveDevice::OperationMeasure, 0);
        }
        else if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
        {
            ShowChoicesScreen(manager, SaveScreenUnformatted);
        }
        else if (operation == SaveOperationLoad)
        {
            ShowChoicesScreen(manager, SaveScreenInsertSave);
        }
    }

    void DestroySaveCode(SaveCode* code, u32 destroyFlags)
    {
        code->vtable = g_SaveCodeVTable;
        StringDestroy(&code->name);
        if ((destroyFlags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(code);
        }
    }
}
