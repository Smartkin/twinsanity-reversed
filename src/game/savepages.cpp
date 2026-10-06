#include "game/olegpages.h"

#include "game/memory.h"
#include "game/oleg.h"
#include "game/savedevice.h"
#include "game/savemanager.h"

// The save code's pages on OLEG's screens: the page of choices and the save slots' page (olegpages.cpp's SaveSlotsPageConstruct
// makes it over a save code page), and their items, which answer the save code's screen when they're activated

extern "C"
{
    // ""
    extern const char g_SaveCodePageName[] RETAIL(D_0030A288);
}

namespace
{
struct ChoiceItem
{
    s32 message;
    u32 answer;
};

// The choices page's items, by their ids: "Format", "Continue", "Continue wihout saving", "Create save", "Retry", "Cancel", "Yes",
// "No"
constexpr ChoiceItem ChoiceItems[] = {
    {SaveMessageFormat, SaveCodeItem::AnswerChoose},
    {SaveMessageContinue, SaveCodeItem::AnswerContinue},
    {SaveMessageLeave, SaveCodeItem::AnswerContinue},
    {SaveMessageCreate, SaveCodeItem::AnswerChoose},
    {SaveMessageRetry, SaveCodeItem::AnswerChoose},
    {SaveMessageCancel, SaveCodeItem::AnswerBack},
    {SaveMessageYes, SaveCodeItem::AnswerChoose},
    {SaveMessageNo, SaveCodeItem::AnswerBack},
};
constexpr u32 ChoiceItemCount = 8;

enum ChoiceItemId : u32
{
    FormatItem = 0,
    ContinueItem = 1,
    LeaveItem = 2,
    CreateItem = 3,
    RetryItem = 4,
    CancelItem = 5,
    YesItem = 6,
    NoItem = 7,
};

// The save code page's items: "Continue wihout saving" (id 0) and "Cancel" (id 1), after the save slots' items
constexpr u32 PageItemCount = 2;
constexpr u32 LeavePageItem = 0;
constexpr u32 CancelPageItem = 1;

// The page's item at an index (none out of range)
MenuItem* ItemAt(const MenuPage* page, s32 index)
{
    if (index < 0 || index >= static_cast<s32>(page->items.count))
    {
        return nullptr;
    }

    return page->items.data[index];
}
}

extern "C"
{
    MenuItem* ConstructSaveCodeItem(void* memory, u32 value, u32 id, u32 operation, SaveManager* manager)
    {
        auto* item = static_cast<SaveCodeItem*>(memory);
        LinkItem::ConstructNamed(item, g_SaveCodePageName, id, nullptr, MenuPlayers);
        item->manager = manager;
        item->answer = operation;
        item->message = static_cast<s32>(value);
        item->vtable = g_SaveCodeItemVTable;
        return item;
    }

    MenuPage* SaveChoicesPageConstruct(void* memory, SaveManager* manager)
    {
        auto* page = static_cast<SaveChoicesPage*>(memory);
        MenuPage::Construct(page, g_SaveCodePageName, MenuPlayers);
        page->manager = manager;
        page->vtable = g_SaveChoicesPageVTable;
        MenuItem* items[ChoiceItemCount];
        for (u32 id = 0; id < ChoiceItemCount; id++)
        {
            void* item = MemoryAllocate(sizeof(SaveCodeItem));
            items[id] = ConstructSaveCodeItem(item, ChoiceItems[id].message, id, ChoiceItems[id].answer, page->manager);
        }

        for (MenuItem* item : items)
        {
            page->Add(item);
        }

        return page;
    }
}

SaveCodePage* SaveCodePage::Construct(SaveCodePage* page, SaveManager* manager)
{
    MenuPage::Construct(page, g_SaveCodePageName, MenuPlayers);
    page->manager = manager;
    page->vtable = g_SaveCodePageVTable;
    return page;
}

void SaveCodePage::AddItems()
{
    void* memory = MemoryAllocate(sizeof(SaveCodeItem));
    MenuItem* leave = ConstructSaveCodeItem(memory, SaveMessageLeave, LeavePageItem, SaveCodeItem::AnswerContinue, manager);
    memory = MemoryAllocate(sizeof(SaveCodeItem));
    MenuItem* cancel = ConstructSaveCodeItem(memory, SaveMessageCancel, CancelPageItem, SaveCodeItem::AnswerBack, manager);
    Add(leave);
    Add(cancel);
}

void SaveCodePage::Destroy(u32 destroyFlags)
{
    MenuPage::Destroy(destroyFlags);
}

void SaveCodePage::ShowSlotItems(s32 mode, u32 saving)
{
    SaveDevice* device = manager->device;
    u32 slots = device->flags.fileCount;
    u8 shown[PageItemCount] = {0, 0};
    if (mode == SlotsPageLoad)
    {
        FolderFile* folder = device->folder;
        for (u32 slot = 0; slot < slots; slot++)
        {
            FolderSummary* summary = folder->summaries[slot];
            MenuItem* item = ItemAt(this, static_cast<s32>(slot));
            item->shown = summary->HasSave() ? EveryPlayer : 0;
        }

        shown[CancelPageItem] = EveryPlayer;
    }
    else if (mode == SlotsPageSave)
    {
        for (u32 slot = 0; slot < slots; slot++)
        {
            ItemAt(this, static_cast<s32>(slot))->shown = EveryPlayer;
        }

        shown[LeavePageItem] = saving != 0 ? EveryPlayer : 0;
        shown[CancelPageItem] = EveryPlayer;
    }

    for (u32 id = 0; id < PageItemCount; id++)
    {
        MenuItem* item = Find(id);
        item->enabled = shown[id];
        if (shown[id] != 0)
        {
            SetFirstItem(item->Id());
        }
    }
}

void SaveChoicesPage::Destroy(u32 destroyFlags)
{
    MenuPage::Destroy(destroyFlags);
}

KEEP_LOOPS void SaveChoicesPage::ShowItems(s32 mode, u32 saving)
{
    u8 shown[ChoiceItemCount] = {};
    u8 leave = saving != 0 ? EveryPlayer : 0;
    switch (mode)
    {
    case ChoicesNoCard:
    case ChoicesNoRoom:
        shown[RetryItem] = EveryPlayer;
        shown[ContinueItem] = EveryPlayer;
        break;
    case ChoicesCreate:
        shown[LeaveItem] = leave;
        shown[CancelItem] = EveryPlayer;
        shown[CreateItem] = EveryPlayer;
        break;
    case ChoicesUnformatted:
        shown[FormatItem] = EveryPlayer;
        shown[LeaveItem] = leave;
        shown[CancelItem] = EveryPlayer;
        break;
    case ChoicesInsertCard:
    case ChoicesInsertSave:
    case ChoicesInsertRoom:
        shown[LeaveItem] = leave;
        shown[CancelItem] = EveryPlayer;
        break;
    case ChoicesConfirmFormat:
    case ChoicesOverwrite:
    case ChoicesCancelSave:
        shown[NoItem] = EveryPlayer;
        shown[YesItem] = EveryPlayer;
        break;
    case ChoicesSaveFailed:
        shown[LeaveItem] = leave;
        shown[CancelItem] = EveryPlayer;
        shown[RetryItem] = EveryPlayer;
        break;
    case ChoicesFormatFailed:
    case ChoicesLoadFailed:
        shown[CancelItem] = EveryPlayer;
        shown[RetryItem] = EveryPlayer;
        break;
    case ChoicesCancelOnly:
        shown[CancelItem] = EveryPlayer;
        break;
    default:
        break;
    }

    for (u32 id = 0; id < ChoiceItemCount; id++)
    {
        MenuItem* item = Find(id);
        item->enabled = shown[id];
        if (shown[id] != 0)
        {
            Select(0, item->Id());
        }
    }
}

MenuPage* SaveCodeItem::Activate(u32, MenuPage*)
{
    switch (answer)
    {
    case AnswerChoose:
        manager->bits.answer = SaveCode::AnswerFirst;
        manager->bits.chosen = Id();
        break;
    case AnswerBack:
        manager->bits.answer = SaveCode::AnswerBack;
        break;
    case AnswerContinue:
        manager->bits.answer = SaveCode::AnswerThird;
        break;
    default:
        break;
    }

    return nullptr;
}

void SaveCodeItem::Destroy(u32 destroyFlags)
{
    LinkItem::Destroy(destroyFlags);
}

void SaveCodeItem::Entered(u32, u32)
{
    text = SaveMessage(message);
    bits.text = 0;
}
