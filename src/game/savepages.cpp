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
// "No" (the save code's messages)
constexpr ChoiceItem ChoiceItems[] = {
    {0x1F, SaveCodeItem::AnswerChoose}, {0x20, SaveCodeItem::AnswerThird}, {0x21, SaveCodeItem::AnswerThird},
    {0x22, SaveCodeItem::AnswerChoose}, {0x23, SaveCodeItem::AnswerChoose}, {0x1E, SaveCodeItem::AnswerBack},
    {0x24, SaveCodeItem::AnswerChoose}, {0x25, SaveCodeItem::AnswerBack},
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
constexpr s32 LeaveMessage = 0x21;
constexpr s32 CancelMessage = 0x1E;
constexpr u32 PageItemCount = 2;
constexpr u32 LeavePageItem = 0;
constexpr u32 CancelPageItem = 1;

constexpr u8 Shown = 0xFF;
// Player 1
constexpr u32 FirstPlayer = 1;
// Bits 16-19 of the save code's bits: the answer; 20-23: the slot chosen
constexpr u32 AnswerMask = 0xF0000;
constexpr u32 ChosenMask = 0xF00000;

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
        LinkItem::ConstructNamed(item, g_SaveCodePageName, id, nullptr, FirstPlayer);
        item->manager = manager;
        item->answer = operation;
        item->message = static_cast<s32>(value);
        item->vtable = g_SaveCodeItemVTable;
        return item;
    }

    MenuPage* SaveChoicesPageConstruct(void* memory, SaveManager* manager)
    {
        auto* page = static_cast<SaveChoicesPage*>(memory);
        MenuPage::Construct(page, g_SaveCodePageName, FirstPlayer);
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
    MenuPage::Construct(page, g_SaveCodePageName, FirstPlayer);
    page->manager = manager;
    page->vtable = g_SaveCodePageVTable;
    return page;
}

void SaveCodePage::AddItems()
{
    void* memory = MemoryAllocate(sizeof(SaveCodeItem));
    MenuItem* leave = ConstructSaveCodeItem(memory, LeaveMessage, LeavePageItem, SaveCodeItem::AnswerThird, manager);
    memory = MemoryAllocate(sizeof(SaveCodeItem));
    MenuItem* cancel = ConstructSaveCodeItem(memory, CancelMessage, CancelPageItem, SaveCodeItem::AnswerBack, manager);
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
    u32 slots = device->flags & SaveDevice::FileCountMask;
    u8 shown[PageItemCount] = {0, 0};
    if (mode == 0)
    {
        auto* folder = static_cast<FolderFile*>(device->mainFile);
        for (u32 slot = 0; slot < slots; slot++)
        {
            FolderSummary* summary = folder->summaries[slot];
            MenuItem* item = ItemAt(this, static_cast<s32>(slot));
            item->shown = summary->HasSave() ? Shown : 0;
        }

        shown[CancelPageItem] = Shown;
    }
    else if (mode == 1)
    {
        for (u32 slot = 0; slot < slots; slot++)
        {
            ItemAt(this, static_cast<s32>(slot))->shown = Shown;
        }

        shown[LeavePageItem] = saving != 0 ? Shown : 0;
        shown[CancelPageItem] = Shown;
    }

    for (u32 id = 0; id < PageItemCount; id++)
    {
        MenuItem* item = Find(id);
        item->enabled = shown[id];
        if (shown[id] != 0)
        {
            SetFirstItem(item->id & MenuItem::IdMask);
        }
    }
}

void SaveChoicesPage::Destroy(u32 destroyFlags)
{
    MenuPage::Destroy(destroyFlags);
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void SaveChoicesPage::ShowItems(s32 mode, u32 saving)
{
    u8 shown[ChoiceItemCount] = {};
    u8 leave = saving != 0 ? Shown : 0;
    switch (mode)
    {
    case 0:
    case 1:
        shown[RetryItem] = Shown;
        shown[ContinueItem] = Shown;
        break;
    case 2:
        shown[LeaveItem] = leave;
        shown[CancelItem] = Shown;
        shown[CreateItem] = Shown;
        break;
    case 4:
        shown[FormatItem] = Shown;
        shown[LeaveItem] = leave;
        shown[CancelItem] = Shown;
        break;
    case 3:
    case 6:
    case 7:
        shown[LeaveItem] = leave;
        shown[CancelItem] = Shown;
        break;
    case 5:
    case 8:
    case 9:
        shown[NoItem] = Shown;
        shown[YesItem] = Shown;
        break;
    case 11:
        shown[LeaveItem] = leave;
        shown[CancelItem] = Shown;
        shown[RetryItem] = Shown;
        break;
    case 10:
    case 12:
        shown[CancelItem] = Shown;
        shown[RetryItem] = Shown;
        break;
    case 13:
        shown[CancelItem] = Shown;
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
            Select(0, item->id & MenuItem::IdMask);
        }
    }
}

MenuPage* SaveCodeItem::Activate(u32, MenuPage*)
{
    switch (answer)
    {
    case AnswerChoose:
        manager->bits = (manager->bits & ~AnswerMask & ~ChosenMask) | 1 << 16 | (id & 0xF) << 20;
        break;
    case AnswerBack:
        manager->bits = (manager->bits & ~AnswerMask) | 2 << 16;
        break;
    case AnswerThird:
        manager->bits = (manager->bits & ~AnswerMask) | 3 << 16;
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
    id &= ~(MenuItem::TextMask << MenuItem::TextShift);
}
