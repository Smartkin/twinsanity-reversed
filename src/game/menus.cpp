#include "game/menus.h"

#include "game/bindings.h"
#include "game/colour.h"
#include "game/controllers.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/overlay.h"
#include "game/renderer.h"
#include "game/sound.h"
#include "game/string.h"
#include "game/widgets.h"
#include "retail/libc.h"

namespace
{
// The page's vtable functions: entered and left; an item's (its vtable after 0x10 bytes) the same
constexpr u32 PageEnteredSlot = 1;
constexpr u32 PageLeftSlot = 3;
constexpr u32 ItemEnteredSlot = 3;
constexpr u32 ItemLeftSlot = 4;

// The flags a page starts with: bits 24-27 set (still unknown), bit 12 too (wrapping), the first item 31, entered afresh, bit 31
// set
constexpr u32 InitialSet = 0x1000000 | 0x2000000 | 0x4000000 | 0x8001000;
constexpr u32 InitialFirstItemClear = 0xFF03FFFF;
constexpr u32 InitialFirstItem = 0x7C0000;
constexpr u32 InitialModeClear = 0x8FFFFFFF;
constexpr u32 InitialMode = 0x10000000;
constexpr u32 InitialHigh = 0x80000000;

// The page's vtable functions: a frame of it (for a player, with the input and the sounds), back (for a player: returns whether
// it took it)
constexpr u32 PageFrameSlot = 2;
constexpr u32 PageBackSlot = 5;
// An item's frame (for a player, on its page, with the input and the sounds): returns where it leads
constexpr u32 ItemFrameSlot = 5;
// The menus' sounds: nothing to go up to and up, nothing to go down to and down, the back
constexpr u32 SoundNotUp = 4;
constexpr u32 SoundUp = 5;
constexpr u32 SoundNotDown = 2;
constexpr u32 SoundDown = 3;
constexpr u32 SoundBack = 11;
constexpr u32 ModeBack = 2;

// The nearest page of the ring a link starts (going on by the same link) that shows the player something, none round to the page
MenuPage* NextShowing(MenuPage* page, u32 link, u32 player)
{
    MenuPage* next = page->links[link];
    if (next == nullptr || next == page)
    {
        return nullptr;
    }

    while (next->ShownCount(player) <= 0)
    {
        next = next->links[link];
        if (next == nullptr || next == page)
        {
            return nullptr;
        }
    }

    return next;
}

// A page gone to by a link: on the link's item, or else on the selection's place (the last item when there aren't that many)
void EnterSideways(MenuPage* page, MenuPage* to, u32 link, u32 player)
{
    s32 index = to->IndexOf(static_cast<u32>(page->linkItems[link]));
    g_NextPage = to;
    if (index == -1)
    {
        u32 selection = page->selections[player];
        s32 count = static_cast<s32>(to->items.count);
        to->selections[player] = static_cast<u8>(static_cast<s32>(selection) < count ? selection : count - 1);
        return;
    }

    to->selections[player] = static_cast<u8>(index);
}

// A sound played as the menus play theirs: their volumes and pitch, the voice kind and last
constexpr f32 OwnVolume = -1.0f;
constexpr f32 OwnPitch = -1.0f;
constexpr s32 MenuVoiceKind = 0;
constexpr s32 MenuSoundLast = -1;

// An item's text and value: the vtable function writing the value's text (for a player; returns whether it has one), and the
// most lines a page draws
constexpr u32 ItemValueTextSlot = 7;
constexpr u32 MaxLines = 64;
constexpr f32 TitleEpsilon = 0x1.A36E2Ep-15f;

// Where the items go up from the place: centred on it, or ending at it
bool CentresItems(u32 flags)
{
    return flags == 0x22 || flags == 0x30 || flags == 0x60;
}

bool EndsItems(u32 flags)
{
    return flags == 0x06 || flags == 0x14 || flags == 0x44;
}

// The fraction of the way from the few-items values to the many-items ones
f32 ManyItems(const MenuDrawer* drawer, u32 count)
{
    u32 start = drawer->windowStart;
    u32 size = drawer->windowSize;
    if (start == 0 || size == 0)
    {
        return 1.0f;
    }

    if (start >= count)
    {
        return 0.0f;
    }

    if (count >= size)
    {
        return 1.0f;
    }

    return static_cast<f32>(count - start) / static_cast<f32>(size - start);
}

// An item's vtable functions: activated, its value set and got as an int
constexpr u32 ItemActivateSlot = 1;
constexpr u32 ItemSetIntSlot = 8;
constexpr u32 ItemGetIntSlot = 9;
// The sounds: nothing to select, selected and leading on, selected; a value not stepped down, stepped down, not stepped up, stepped
// up
constexpr u32 SoundNothing = 0;
constexpr u32 SoundSelected = 1;
constexpr u32 SoundSelectedOn = 10;
constexpr u32 SoundValueNotDown = 8;
constexpr u32 SoundValueDown = 9;
constexpr u32 SoundValueNotUp = 6;
constexpr u32 SoundValueUp = 7;
// A choice's text before it's named
constexpr s32 NoText = -1;
constexpr u32 PageItemsGrowth = 10;

// The players' bits and the id's fields an item starts with
void SetItemBits(MenuItem* item, u32 text, u32 id, u32 players, bool withText)
{
    u32 bits = item->id;
    bits = (bits & ~MenuItem::IdMask) | (id & MenuItem::IdMask);
    bits = (bits & ~(MenuItem::PlayersMask << MenuItem::PlayersShift)) | (players & MenuItem::PlayersMask) << MenuItem::PlayersShift;
    bits |= MenuItem::StepsOnPress;
    bits = bits & ~(MenuItem::TextMask << MenuItem::TextShift);
    if (withText)
    {
        bits |= (text & MenuItem::TextMask) << MenuItem::TextShift;
    }

    u8 all = static_cast<u8>((1 << players) - 1);
    item->id = bits;
    item->shown = all;
    item->enabled = all;
}

// A page's arrays and fields made for the players
void ConstructPage(MenuPage* page, u32 players)
{
    page->vtable = g_MenuPageVTable;
    page->unknown08 = players;
    page->unknown04 = players != 0 ? static_cast<u8*>(MemoryAllocate2(players)) : nullptr;
    page->unknown10 = players;
    page->selections = players != 0 ? static_cast<u8*>(MemoryAllocate2(players)) : nullptr;
}

void ConstructPageItems(MenuPage* page)
{
    page->items.growth = PageItemsGrowth;
    page->items.count = 0;
    page->items.capacity = PageItemsGrowth;
    page->items.data = static_cast<MenuItem**>(MemoryAllocate2(PageItemsGrowth * sizeof(MenuItem*)));
    page->Initialise();
}

// A value item's choice texts made, none named
void ConstructChoices(ChoiceItem* item, u32 choices)
{
    item->usesStrings = 0;
    item->strings = nullptr;
    item->vtable = g_ChoiceItemVTable;
    item->choices = choices;
    item->texts = choices != 0 ? static_cast<s32*>(MemoryAllocate2(choices * sizeof(s32))) : nullptr;
    for (u32 choice = 0; choice < item->choices; choice++)
    {
        item->texts[choice] = NoText;
    }
}

// A bit of an action's byte set or cleared
void SetActionBit(u8* bits, u32 bit, u32 on)
{
    if (on != 0)
    {
        *bits = static_cast<u8>(*bits | 1 << bit);
    }
    else
    {
        *bits = static_cast<u8>(*bits & ~(1 << bit));
    }
}
}

MenuInput* MenuInput::Construct(MenuInput* input, ButtonBindings* bindings)
{
    input->bindings = bindings;
    input->vtable = g_MenuInputVTable;
    input->Clear();
    return input;
}

void MenuInput::Destroy(u32 destroyFlags)
{
    vtable = g_MenuInputVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void MenuInput::Poll(const PadButtons* pad, u32 withLeave)
{
    u32 edge = 1;
    for (s32 pass = 1; pass >= 0; pass--)
    {
        u32 select = bindings->Has(pad, ActionSelect, edge);
        u32 backPressed = bindings->Has(pad, ActionBack, edge);
        u32 up = bindings->Has(pad, ActionUp, edge);
        u32 down = bindings->Has(pad, ActionDown, edge);
        u32 left = bindings->Has(pad, ActionLeft, edge);
        u32 right = bindings->Has(pad, ActionRight, edge);
        u32 leave = withLeave != 0 ? bindings->Has(pad, ActionLeave, edge) : 0;
        u8* bits = edge != 0 ? &pressed : &held;
        SetActionBit(bits, ActionSelect, select);
        SetActionBit(bits, ActionBack, backPressed);
        SetActionBit(bits, ActionUp, up);
        SetActionBit(bits, ActionDown, down);
        SetActionBit(bits, ActionLeft, left);
        SetActionBit(bits, ActionRight, right);
        SetActionBit(bits, ActionLeave, leave);
        edge ^= 1;
    }
}

void MenuInput::Nothing()
{
}

void MenuInput::Clear()
{
    held = 0;
    pressed = 0;
}

u32 MenuInput::Has(u32 action, u32 pressedNow)
{
    u8 bits = pressedNow != 0 ? pressed : held;
    return (bits & static_cast<u8>(1 << action)) != 0 ? 1 : 0;
}

MenuSounds* MenuSounds::Construct(MenuSounds* menuSounds)
{
    for (u32 i = 0; i < Count; i++)
    {
        menuSounds->sounds[i] = nullptr;
        menuSounds->groups[i] = 3;
    }

    return menuSounds;
}

void MenuSounds::Play(u32 index)
{
    if (sounds[index] != nullptr)
    {
        PlaySound(OwnVolume, OwnPitch, sounds[index], groups[index], MenuVoiceKind, MenuSoundLast);
    }
}

void MenuPage::Initialise()
{
    u32 value = flags | InitialSet;
    value = (value & InitialFirstItemClear) | InitialFirstItem;
    value = (value & InitialModeClear) | InitialMode | InitialHigh;
    parent = nullptr;
    flags = value;
    for (u32 player = 0; player < Players(); player++)
    {
        selections[player] = 0;
        unknown04[player] = 0;
    }

    for (u32 link = 0; link < 4; link++)
    {
        links[link] = nullptr;
        linkItems[link] = -1;
    }
}

void MenuPage::Destroy(u32 destroyFlags)
{
    vtable = g_MenuPageVTable;
    if (items.data != nullptr)
    {
        MemoryDeallocate_(items.data);
    }

    if (selections != nullptr)
    {
        MemoryDeallocate_(selections);
    }

    if (unknown04 != nullptr)
    {
        MemoryDeallocate_(unknown04);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

MenuItem* MenuPage::Item(s32 index)
{
    if (index >= 0 && index < static_cast<s32>(items.count))
    {
        return items.data[index];
    }

    return nullptr;
}

MenuItem* MenuPage::ItemAgain(s32 index)
{
    return Item(index);
}

u32 MenuPage::SelectPrevious(u32 player)
{
    s32 count = static_cast<s32>(items.count);
    if (count <= 0)
    {
        return 0;
    }

    s32 start = selections[player];
    s32 index = start;
    while (true)
    {
        if (index != 0)
        {
            index--;
        }
        else
        {
            if ((flags & Wraps) == 0)
            {
                return 0;
            }

            index = count - 1;
        }

        if (index == start)
        {
            return 0;
        }

        MenuItem* item = Item(index);
        if (item->IsShownTo(player) && item->CanSelect(player))
        {
            selections[player] = static_cast<u8>(index);
            return 1;
        }
    }
}

u32 MenuPage::SelectNext(u32 player)
{
    s32 count = static_cast<s32>(items.count);
    if (count <= 0)
    {
        return 0;
    }

    s32 start = selections[player];
    s32 index = start + 1;
    while (true)
    {
        if (index >= count)
        {
            if ((flags & Wraps) == 0)
            {
                return 0;
            }

            index = 0;
        }

        if (index == start)
        {
            return 0;
        }

        MenuItem* item = Item(index);
        if (item->IsShownTo(player) && item->CanSelect(player))
        {
            selections[player] = static_cast<u8>(index);
            return 1;
        }

        index++;
    }
}

void MenuPage::SkipHidden(u32 player)
{
    s32 count = static_cast<s32>(items.count);
    MenuItem* item = Item(selections[player]);
    if (count <= 0)
    {
        return;
    }

    for (s32 tried = 0; !item->IsShownTo(player);)
    {
        selections[player] = static_cast<u8>((selections[player] + 1) % count);
        item = Item(selections[player]);
        tried++;
        if (tried >= count)
        {
            return;
        }
    }
}

s32 MenuPage::IndexOf(u32 id)
{
    for (u32 i = 0; i < items.count; i++)
    {
        if (items.data[i]->Id() == id)
        {
            return static_cast<s32>(i);
        }
    }

    return -1;
}

MenuItem* MenuPage::Find(u32 id)
{
    for (u32 i = 0; i < items.count; i++)
    {
        if (items.data[i]->Id() == id)
        {
            return items.data[i];
        }
    }

    return nullptr;
}

s32 MenuPage::ShownCount(u32 player)
{
    s32 shown = 0;
    for (u32 i = 0; i < items.count; i++)
    {
        shown += items.data[i]->IsShownTo(player) ? 1 : 0;
    }

    return shown;
}

void MenuPage::Enter(MenuPage* from)
{
    u32 count = items.count;
    u32 mode = from != nullptr ? from->Mode() : 1;
    flags = (flags & ~(ModeMask << ModeShift)) | (mode & ModeMask) << ModeShift;
    mode = Mode();
    for (u32 player = 0; player < Players(); player++)
    {
        CallVirtual<void>(this, vtable, PageEnteredSlot, player, mode);
        if (mode == 1)
        {
            u32 first = (flags >> FirstItemShift) & FirstItemMask;
            if (first < count)
            {
                selections[player] = static_cast<u8>(first);
            }
        }

        for (u32 tried = 0; tried < count; tried++)
        {
            u32 index = (selections[player] + tried) % count;
            MenuItem* item = Item(static_cast<s32>(index));
            if (item->IsShownTo(player) && item->CanSelect(player))
            {
                selections[player] = static_cast<u8>(index);
                break;
            }
        }

        for (u32 i = 0; i < count; i++)
        {
            MenuItem* item = Item(static_cast<s32>(i));
            CallVirtual<void>(item, item->vtable, ItemEnteredSlot, player, mode);
        }
    }
}

void MenuPage::Leave()
{
    s32 count = static_cast<s32>(items.count);
    u32 mode = Mode();
    for (s32 player = 0; player < static_cast<s32>(Players()); player++)
    {
        CallVirtual<void>(this, vtable, PageLeftSlot, static_cast<u32>(player), mode);
        for (s32 i = 0; i < count; i++)
        {
            MenuItem* item = Item(i);
            CallVirtual<void>(item, item->vtable, ItemLeftSlot, static_cast<u32>(player), mode);
        }
    }
}

MenuPage* MenuPage::Step(MenuInput* input, MenuSounds* sounds, u32 player)
{
    if (g_NextPage == nullptr)
    {
        if ((flags & TakesLeave) != 0 && input->Has(MenuInput::ActionLeave, 1) != 0)
        {
            g_NextPage = &g_ResumePage;
        }
        else
        {
            if ((flags & AsksSelect) != 0)
            {
                input->Has(MenuInput::ActionSelect, 1);
            }

            u32 back = (flags & TakesBack) != 0 ? input->Has(MenuInput::ActionBack, 1) : 0;
            u32 up = 0;
            u32 down = 0;
            if ((flags & TakesUpDown) != 0)
            {
                up = input->Has(MenuInput::ActionUp, 1);
                down = input->Has(MenuInput::ActionDown, 1);
            }

            SkipHidden(player);
            if (up != 0)
            {
                MenuPage* above = links[LinkUp];
                u32 moved = 1;
                if (above != nullptr && selections[player] == 0)
                {
                    s32 index = above->IndexOf(static_cast<u32>(linkItems[LinkUp]));
                    g_NextPage = above;
                    above->selections[player] = static_cast<u8>(index == -1 ? static_cast<s32>(above->items.count) - 1 : index);
                }
                else
                {
                    moved = SelectPrevious(player);
                }

                if (sounds != nullptr)
                {
                    sounds->Play(moved != 0 ? SoundUp : SoundNotUp);
                }
            }
            else if (down != 0)
            {
                MenuPage* below = links[LinkDown];
                u32 moved = 1;
                // The retail game compares the selection with the page below's last item rather than its own
                if (below != nullptr && selections[player] == below->items.count - 1)
                {
                    s32 index = below->IndexOf(static_cast<u32>(linkItems[LinkDown]));
                    g_NextPage = below;
                    below->selections[player] = static_cast<u8>(index == -1 ? 0 : index);
                }
                else
                {
                    moved = SelectNext(player);
                }

                if (sounds != nullptr)
                {
                    sounds->Play(moved != 0 ? SoundDown : SoundNotDown);
                }
            }

            MenuItem* item = Item(selections[player]);
            u32 left = 0;
            u32 right = 0;
            if ((flags & TakesLeftRight) != 0 && up == 0 && down == 0)
            {
                left = input->Has(MenuInput::ActionLeft, 0);
                right = input->Has(MenuInput::ActionRight, 0);
            }

            MenuPage* leftPage = NextShowing(this, LinkLeft, player);
            MenuPage* rightPage = NextShowing(this, LinkRight, player);
            if (left != 0 && leftPage != nullptr)
            {
                EnterSideways(this, leftPage, LinkLeft, player);
            }
            else if (right != 0 && rightPage != nullptr)
            {
                EnterSideways(this, rightPage, LinkRight, player);
            }
            else if (back != 0)
            {
                u32 tookIt = CallVirtual<u32>(this, vtable, PageBackSlot, player);
                flags = (flags & ~(ModeMask << ModeShift)) | ModeBack << ModeShift;
                g_NextPage = parent;
                if (sounds != nullptr && (tookIt != 0 || (parent != nullptr && parent != this)))
                {
                    sounds->Play(SoundBack);
                }
            }
            else if (item != nullptr)
            {
                MenuPage* next = CallVirtual<MenuPage*>(item, item->vtable, ItemFrameSlot, player, this, input, sounds);
                g_NextPage = next;
                if (next != nullptr && next != this)
                {
                    u32 mode = next != parent ? 1 : ModeBack;
                    flags = (flags & ~(ModeMask << ModeShift)) | mode << ModeShift;
                    if (sounds != nullptr && Mode() == ModeBack)
                    {
                        sounds->Play(SoundBack);
                    }
                }
            }

            CallVirtual<void>(this, vtable, PageFrameSlot, player, input, sounds);
        }
    }

    MenuPage* next = g_NextPage;
    g_NextPage = nullptr;
    return next;
}

extern "C"
{
    void MenuDrawerStyle(MenuDrawer* drawer, u32 style, const Vector2* scale, const u32* colours, Renderer* renderer)
    {
        switch (style)
        {
        case MenuDrawer::StyleTitle:
            renderer->colour = colours[MenuDrawer::StyleTitle];
            renderer->font = drawer->fonts[MenuDrawer::StyleTitle];
            renderer->textScale.y = scale->y;
            renderer->textScale.x = scale->x;
            renderer->textFlags = drawer->titleFlags;
            break;
        case MenuDrawer::StyleSelected:
        {
            u32 colour = colours[MenuDrawer::StyleSelected];
            Vector2 size;
            CopyVector2(&size, scale);
            if (drawer->colourPulse != nullptr)
            {
                ColourScale(&colour, drawer->colourPulse->value);
            }

            if (drawer->sizePulse != nullptr)
            {
                size.x = size.x * drawer->sizePulse->value;
                size.y = size.y * drawer->sizePulse->value;
            }

            renderer->colour = colour;
            renderer->textScale.x = size.x;
            renderer->font = drawer->fonts[MenuDrawer::StyleSelected];
            renderer->textScale.y = size.y;
            renderer->textFlags = drawer->itemFlags;
            break;
        }
        case MenuDrawer::StyleShown:
        case MenuDrawer::StyleHidden:
            renderer->colour = colours[style];
            renderer->font = drawer->fonts[style];
            renderer->textScale.y = scale->y;
            renderer->textScale.x = scale->x;
            renderer->textFlags = drawer->itemFlags;
            break;
        default:
            break;
        }
    }

    void DrawMenuPage(MenuDrawer* drawer, MenuPage* page, u32 player, u32 colour, const Vector2* at, const Vector2* size,
                      Renderer* renderer)
    {
        Vector2 titleScale;
        CopyVector2(&titleScale, &drawer->titleScale);
        MenuItem* selected = page->ItemAgain(page->selections[player]);
        f32 red = Colour::ColourFraction(colour, 0);
        f32 alpha = Colour::AlphaFraction(colour);
        f32 blue = Colour::ColourFraction(colour, 2);
        f32 green = Colour::ColourFraction(colour, 1);
        const char* name = page->name;
        u32 colours[4];
        String title = {nullptr, 0, 0};
        for (u32 style = 0; style < 4; style++)
        {
            u32 own = drawer->colours[style];
            auto* bytes = reinterpret_cast<u8*>(&colours[style]);
            bytes[0] = Colour::ColourByte(Colour::ColourFraction(own, 0) * red);
            bytes[1] = Colour::ColourByte(Colour::ColourFraction(own, 1) * green);
            bytes[2] = Colour::ColourByte(Colour::ColourFraction(own, 2) * blue);
            bytes[3] = Colour::AlphaByte(Colour::AlphaFraction(own) * alpha);
        }

        titleScale.x = titleScale.x * size->x;
        titleScale.y = titleScale.y * size->y;
        if (!(__builtin_fabsf(titleScale.x) <= TitleEpsilon && __builtin_fabsf(titleScale.y) <= TitleEpsilon))
        {
            Vector2 place;
            CopyVector2(&place, &drawer->titlePlace);
            place.x = place.x * size->x + at->x;
            place.y = place.y * size->y + at->y;
            if (name == nullptr)
            {
                name = GameText(page->flags & 0x3FF);
            }

            StringAssign(&title, name);
            MenuDrawerStyle(drawer, MenuDrawer::StyleTitle, &titleScale, colours, renderer);
            QueueText(renderer, title.string, place.x, place.y);
        }

        if (selected != nullptr)
        {
            String lines[MaxLines];
            for (String& line : lines)
            {
                line = {nullptr, 0, 0};
            }

            char value[0x40];
            MenuItem* drawn[MaxLines];
            u32 count = 0;
            u32 selectedLine = 0;
            for (u32 i = 0; i < page->items.count; i++)
            {
                MenuItem* item = page->items.data[i];
                String* line = &lines[count];
                const char* text = item->text;
                if (text == nullptr)
                {
                    text = GameText((item->id >> 12) & 0xFFF);
                }

                StringAssign(line, text);
                if (CallVirtual<u32>(item, item->vtable, ItemValueTextSlot, player, value) != 0)
                {
                    StringAppend(line, g_MenuValueSeparator);
                    StringAppend(line, value);
                }

                if (item == selected)
                {
                    selectedLine = count;
                }

                if (line->length != 0 && item->CanSelect(player))
                {
                    drawn[count] = item;
                    count++;
                }
            }

            if (count != 0)
            {
                Vector2 cursor = {0.0f, 0.0f};
                Vector2 itemsPlace;
                CopyVector2(&itemsPlace, &drawer->itemsPlace);
                Vector2 places[MaxLines];
                f32 many = ManyItems(drawer, count);
                f32 few = 1.0f - many;
                Vector2 itemScale;
                itemScale.x = (drawer->manyItemsScale.x * many + drawer->fewItemsScale.x * few) * size->x;
                itemScale.y = (drawer->manyItemsScale.y * many + drawer->fewItemsScale.y * few) * size->y;
                f32 spacing = (drawer->manyItemsSpacing * many + drawer->fewItemsSpacing * few) * size->y;
                for (u32 line = 0; line < count; line++)
                {
                    places[line] = cursor;
                    cursor.y = cursor.y + spacing;
                }

                // The lines drawn: every one, or the window's around the selection
                u32 first;
                u32 last;
                u32 window = drawer->windowSize;
                if (window == 0 || window >= count)
                {
                    last = count - 1;
                    first = 0;
                }
                else
                {
                    u32 half = window >> 1;
                    first = half < selectedLine ? selectedLine - half : 0;
                    cursor.y = static_cast<f32>(static_cast<s32>(window)) * spacing;
                    if (first + window >= count)
                    {
                        first = count - window;
                    }

                    last = first - 1 + window;
                }

                itemsPlace.x = itemsPlace.x * size->x + at->x;
                itemsPlace.y = itemsPlace.y * size->y + at->y;
                if (CentresItems(drawer->itemFlags))
                {
                    itemsPlace.y = itemsPlace.y - (cursor.y - spacing) * 0.5f;
                }
                else if (EndsItems(drawer->itemFlags))
                {
                    itemsPlace.y = itemsPlace.y - cursor.y;
                }

                for (u32 line = first; line <= last; line++)
                {
                    Vector2 place;
                    CopyVector2(&place, &places[line]);
                    MenuItem* item = drawn[line];
                    u32 style = item == selected       ? MenuDrawer::StyleSelected
                                : item->IsShownTo(player) ? MenuDrawer::StyleShown
                                                          : MenuDrawer::StyleHidden;
                    MenuDrawerStyle(drawer, style, &itemScale, colours, renderer);
                    place.x = place.x + itemsPlace.x;
                    place.y = place.y + itemsPlace.y;
                    QueueText(renderer, lines[line].string, place.x, place.y);
                }
            }

            for (u32 line = MaxLines; line-- != 0;)
            {
                StringDestroy(&lines[line]);
            }
        }

        StringDestroy(&title);
    }
}

MenuItem* MenuItem::Construct(MenuItem* item, u32 text, u32 id, u32 players)
{
    item->vtable = g_MenuItemVTable;
    SetItemBits(item, text, id, players, true);
    item->target = nullptr;
    item->text = nullptr;
    return item;
}

MenuPage* MenuItem::Activate(u32, MenuPage*)
{
    return target;
}

void MenuItem::Destroy(u32 destroyFlags)
{
    vtable = g_MenuItemVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void MenuItem::Entered(u32, u32)
{
}

void MenuItem::Left(u32, u32)
{
}

MenuPage* MenuItem::Frame(u32 player, MenuPage* page, MenuInput* input, MenuSounds* sounds)
{
    if ((input->pressed & 1 << MenuInput::ActionSelect) == 0)
    {
        return nullptr;
    }

    bool isShown = IsShownTo(player);
    MenuPage* next = nullptr;
    if (isShown)
    {
        next = CallVirtual<MenuPage*>(this, vtable, ItemActivateSlot, player, page);
    }

    if (sounds != nullptr)
    {
        sounds->Play(!isShown ? SoundNothing : next != nullptr ? SoundSelectedOn : SoundSelected);
    }

    return next;
}

u32 MenuItem::SetFlag(u32 player, u32 flag)
{
    return CallVirtual<u32>(this, vtable, ItemSetIntSlot, player, flag);
}

u32 MenuItem::GetFlag(u32 player, u8* flag)
{
    s32 value;
    if (CallVirtual<u32>(this, vtable, ItemGetIntSlot, player, &value) == 0)
    {
        return 0;
    }

    if (value == 0)
    {
        *flag = 0;
        return 1;
    }

    if (value != 1)
    {
        return 0;
    }

    *flag = 1;
    return 1;
}

LinkItem* LinkItem::Construct(LinkItem* item, u32 text, u32 id, MenuPage* target, u32 players)
{
    SetItemBits(item, text, id, players, true);
    item->vtable = g_LinkItemVTable;
    item->target = target;
    item->text = nullptr;
    return item;
}

LinkItem* LinkItem::ConstructNamed(LinkItem* item, const char* text, u32 id, MenuPage* target, u32 players)
{
    SetItemBits(item, 0, id, players, false);
    item->text = text;
    item->vtable = g_LinkItemVTable;
    item->target = target;
    return item;
}

void LinkItem::Destroy(u32 destroyFlags)
{
    MenuItem::Destroy(destroyFlags);
}

MenuPage* LinkItem::Unknown6()
{
    return target;
}

u32 LinkItem::ValueText(u32, char* text)
{
    text[0] = '\0';
    return 0;
}

u32 LinkItem::SetInt(u32, s32)
{
    return 0;
}

u32 LinkItem::GetInt(u32, s32* value)
{
    *value = -1;
    return 0;
}

u32 LinkItem::SetUnsigned(u32, u32)
{
    return 0;
}

u32 LinkItem::GetUnsigned(u32, u32* value)
{
    *value = 0xFFFFFFFF;
    return 0;
}

u32 LinkItem::SetFloat(u32, f32)
{
    return 0;
}

u32 LinkItem::GetFloat(u32, f32* value)
{
    *value = Rounded(1e30);
    return 0;
}

void MenuPage::Entered(u32, u32)
{
}

void MenuPage::Frame(u32, MenuInput*, MenuSounds*)
{
}

void MenuPage::Left(u32, u32)
{
}

u32 MenuPage::Back(u32)
{
    return 0;
}

void MenuPage::Unknown6(u32)
{
}

void MenuPage::Unknown7(u32)
{
}

void MenuPage::Unknown8(u32)
{
}

MenuDrawer* MenuDrawer::Construct(MenuDrawer* drawer, Font* font)
{
    constexpr s32 TitleColour = 0xF;
    constexpr s32 SelectedColour = 0xF;
    constexpr s32 ShownColour = 0x13;
    constexpr s32 HiddenColour = 0x15;
    drawer->colourPulse = nullptr;
    drawer->sizePulse = nullptr;
    drawer->windowStart = 0;
    drawer->windowSize = 0;
    drawer->titleFlags = 6;
    drawer->itemFlags = 3;
    drawer->titlePlace = {0.0f, 0.0f};
    drawer->titleScale = {1.0f, 1.0f};
    drawer->itemsPlace = {0.0f, Rounded(0.1)};
    drawer->fewItemsScale = {0.5f, 0.5f};
    drawer->fewItemsSpacing = Rounded(0.05);
    drawer->manyItemsScale = {0.5f, 0.5f};
    drawer->manyItemsSpacing = Rounded(0.05);
    const s32 colours[4] = {TitleColour, SelectedColour, ShownColour, HiddenColour};
    for (u32 style = 0; style < 4; style++)
    {
        u32 colour;
        GetColor(&colour, colours[style]);
        drawer->fonts[style] = font;
        drawer->colours[style] = colour;
    }

    return drawer;
}

BackItem* BackItem::Construct(BackItem* item, u32 text, u32 id, MenuPage* target, u32 players)
{
    LinkItem::Construct(item, text, id, target, players);
    item->vtable = g_BackItemVTable;
    return item;
}

MenuPage* BackItem::Activate(u32 player, MenuPage* page)
{
    CallVirtual<u32>(page, page->vtable, PageBackSlot, player);
    return MenuItem::Activate(player, page);
}

void BackItem::Destroy(u32 destroyFlags)
{
    vtable = g_BackItemVTable;
    LinkItem::Destroy(destroyFlags);
}

ValueItem* ValueItem::Construct(ValueItem* item, u32 text, u32 id, s32 minimum, s32 maximum, u32 wraps, s32 value, u32 players)
{
    MenuItem::Construct(item, text, id, players);
    item->cycles = 0;
    item->vtable = g_ValueItemVTable;
    item->SetRange(minimum, maximum, wraps, value);
    return item;
}

void ValueItem::SetRange(s32 newMinimum, s32 newMaximum, u32 newWraps, s32 value)
{
    minimum = newMinimum;
    maximum = newMaximum;
    wraps = static_cast<u8>(newWraps);
    for (s32 player = 0; player < static_cast<s32>((id >> PlayersShift) & PlayersMask); player++)
    {
        values[player] = value;
    }
}

MenuPage* ValueItem::Activate(u32 player, MenuPage*)
{
    if (target != nullptr)
    {
        return target;
    }

    if (cycles != 0)
    {
        s32 value = values[player];
        if (value < maximum)
        {
            value++;
        }
        else if (wraps != 0)
        {
            value = minimum;
        }
        else
        {
            return nullptr;
        }

        CallVirtual<u32>(this, vtable, ItemSetIntSlot, player, value);
    }

    return nullptr;
}

void ValueItem::Destroy(u32 destroyFlags)
{
    vtable = g_ValueItemVTable;
    MenuItem::Destroy(destroyFlags);
}

MenuPage* ValueItem::Frame(u32 player, MenuPage* page, MenuInput* input, MenuSounds* sounds)
{
    u32 onPress = (id >> 28) & 1;
    u32 left = input->Has(MenuInput::ActionLeft, onPress);
    u32 right = input->Has(MenuInput::ActionRight, onPress);
    if (left != 0)
    {
        s32 value = values[player];
        u32 stepped = 0;
        // Above 0 it always wraps down to the maximum
        if (value > 0 || wraps != 0)
        {
            CallVirtual<u32>(this, vtable, ItemSetIntSlot, player, minimum < value ? value - 1 : maximum);
            stepped = 1;
        }

        if (sounds != nullptr)
        {
            sounds->Play(stepped != 0 ? SoundValueDown : SoundValueNotDown);
        }

        return nullptr;
    }

    if (right != 0)
    {
        s32 value = values[player];
        u32 stepped = 0;
        if (value < maximum)
        {
            CallVirtual<u32>(this, vtable, ItemSetIntSlot, player, value + 1);
            stepped = 1;
        }
        else if (wraps != 0)
        {
            CallVirtual<u32>(this, vtable, ItemSetIntSlot, player, minimum);
            stepped = 1;
        }

        if (sounds != nullptr)
        {
            sounds->Play(stepped != 0 ? SoundValueUp : SoundValueNotUp);
        }

        return nullptr;
    }

    return MenuItem::Frame(player, page, input, sounds);
}

MenuPage* ValueItem::Unknown6()
{
    return nullptr;
}

u32 ValueItem::ValueText(u32 player, char* text)
{
    RetailLibc::Format(text, g_ValueFormat, values[player]);
    return 1;
}

u32 ValueItem::SetInt(u32 player, s32 value)
{
    values[player] = value;
    return 1;
}

u32 ValueItem::GetInt(u32 player, s32* value)
{
    *value = values[player];
    return 1;
}

u32 ValueItem::SetUnsigned(u32 player, u32 value)
{
    values[player] = static_cast<s32>(value);
    return 1;
}

u32 ValueItem::GetUnsigned(u32 player, u32* value)
{
    *value = static_cast<u32>(values[player]);
    return 1;
}

u32 ValueItem::SetFloat(u32 player, f32 value)
{
    values[player] = static_cast<s32>(value);
    return 1;
}

u32 ValueItem::GetFloat(u32 player, f32* value)
{
    *value = static_cast<f32>(values[player]);
    return 1;
}

ChoiceItem* ChoiceItem::Construct(ChoiceItem* item, u32 text, u32 id, u32 choices, u32 wraps, s32 value, u32 players)
{
    MenuItem::Construct(item, text, id, players);
    item->cycles = 0;
    item->vtable = g_ValueItemVTable;
    item->SetRange(0, static_cast<s32>(choices) - 1, wraps, value);
    ConstructChoices(item, choices);
    return item;
}

void ChoiceItem::SetChoiceText(u32 choice, s32 text)
{
    texts[choice] = text;
}

void ChoiceItem::Destroy(u32 destroyFlags)
{
    vtable = g_ChoiceItemVTable;
    if (texts != nullptr)
    {
        MemoryDeallocate_(texts);
    }

    if (strings != nullptr)
    {
        MemoryDeallocate_(strings);
    }

    vtable = g_ValueItemVTable;
    MenuItem::Destroy(destroyFlags);
}

u32 ChoiceItem::ValueText(u32 player, char* text)
{
    if (maximum < minimum)
    {
        return 0;
    }

    s32 value = values[player];
    const char* name = usesStrings != 0 ? strings[value] : GameText(texts[value]);
    RetailLibc::StringCopy(text, name);
    return 1;
}

ToggleItem* ToggleItem::Construct(ToggleItem* item, u32 text, u32 id, s32 offText, s32 onText, u32 wraps, s32 value, u32 players)
{
    MenuItem::Construct(item, text, id, players);
    item->cycles = 0;
    item->vtable = g_ValueItemVTable;
    item->SetRange(0, 1, wraps, value);
    ConstructChoices(item, 2);
    item->vtable = g_ToggleItemVTable;
    item->cycles = 1;
    item->texts[0] = offText;
    item->texts[1] = onText;
    return item;
}

void ToggleItem::Destroy(u32 destroyFlags)
{
    ChoiceItem::Destroy(destroyFlags);
}

MenuPage* MenuPage::Construct(MenuPage* page, const char* name, u32 players)
{
    ConstructPage(page, players);
    page->name = name;
    page->flags = (page->flags & ~(PlayersMask << PlayersShift)) | (players & PlayersMask) << PlayersShift;
    ConstructPageItems(page);
    return page;
}

MenuPage* MenuPage::ConstructTitled(MenuPage* page, u32 title, u32 players)
{
    ConstructPage(page, players);
    page->name = nullptr;
    page->flags = (page->flags & ~(PlayersMask << PlayersShift | 0x3FFu)) | (title & 0x3FF) | (players & PlayersMask) << PlayersShift;
    ConstructPageItems(page);
    return page;
}

u32 MenuPage::Add(MenuItem* item)
{
    items.Append(item);
    return 1;
}

void MenuPage::SetFirstItem(u32 id)
{
    s32 index = IndexOf(id);
    if (index >= 0)
    {
        flags = (flags & ~(FirstItemMask << FirstItemShift)) | (static_cast<u32>(index) & FirstItemMask) << FirstItemShift;
    }
}

void MenuPage::Select(u32 player, u32 id)
{
    s32 index = IndexOf(id);
    if (index >= 0)
    {
        selections[player] = static_cast<u8>(index);
    }
}

EABI_EXPORT(FUN_0025ca48, &LinkItem::SetFloat);
EABI_EXPORT(func_0025BCD0, &ValueItem::SetFloat);
