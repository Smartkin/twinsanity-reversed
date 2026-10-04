#include "game/bindings.h"
#include "game/controllers.h"
#include "game/memory.h"
#include "game/overlay.h"
#include "game/renderer.h"
#include "game/savedevice.h"
#include "game/savemanager.h"

// A save code that reads a pad of its own: the screen it shows and, on the screens listing the card's saves, the choice selected.
// Its vtable is the second half of D_003064C8 (0x306518), which no code puts in an object: retail never makes one

extern "C"
{
    extern const GccVTableEntry g_SaveCodeVTable[] RETAIL(D_00306570);
    // The choices of the screens: the save code's messages ("Continue wihout saving" and "Retry"; "Yes" and "No"; "Cancel";
    // "Format", "Continue wihout saving" and "Cancel")
    extern const s32 g_RetryChoices[] RETAIL(D_0030A290);
    extern const s32 g_CreateChoices[] RETAIL(D_0030A298);
    extern const s32 g_InsertCardChoices[] RETAIL(D_0030A2A0);
    extern const s32 g_UnformattedChoices[] RETAIL(D_002E8000);
    extern const s32 g_FormatChoices[] RETAIL(D_0030A2A8);
    extern const s32 g_InsertSaveChoices[] RETAIL(D_0030A2B0);
    extern const s32 g_InsertRoomChoices[] RETAIL(D_0030A2B8);
    extern const s32 g_OverwriteChoices[] RETAIL(D_0030A2C0);
    extern const s32 g_CancelSaveChoices[] RETAIL(D_0030A2C8);
    extern const s32 g_FormatFailedChoices[] RETAIL(D_0030A2D0);
    extern const s32 g_SaveFailedChoices[] RETAIL(D_0030A2D8);
    extern const s32 g_LoadFailedChoices[] RETAIL(D_0030A2E0);
}

namespace
{
// The screens' texts: a large title in the middle, the choices below it a line apart, the selected one in the text colour
constexpr s32 TextColour = 0xF;
constexpr s32 UnselectedColour = 0x13;
constexpr u32 TextFlags = 0x22;
constexpr f32 TitleScale = 0.75f;
constexpr f32 ChoiceScale = 0.5f;
constexpr f32 TextX = 0.5f;
constexpr f32 TitleY = Rounded(0.4);
constexpr f32 ChoicesY = Rounded(0.7);
constexpr f32 ChoiceSpacing = Rounded(0.05);
// The save slots' choices: an empty slot's text, and the way back last
constexpr s32 EmptyMessage = 0x1D;
constexpr s32 CancelMessage = 0x1E;
// The screens: the message's, those of save slots, and one the pad code draws nothing of (cancel the save?)
constexpr u32 MessageScreen = 0;
constexpr u32 CancelSaveScreen = 10;
constexpr u32 Screens = 16;
// The operations whose messages it draws (the checks, the saves and the load) and the device's with a message
constexpr u32 OperationMessages = 10;
// The answers (bits 16-19 of the bits) and the slot chosen with the first (20-23)
constexpr u32 AnswerFirst = 1;
constexpr u32 AnswerBack = 2;
constexpr u32 AnswerThird = 3;
constexpr u32 AnswerMask = 0xF0000;
constexpr u32 ChosenMask = 0xF00000;
// The bindings' destructor without freeing them
constexpr u32 BindingsDestroyFlags = 2;
}

// The screen it shows (bits 8-15 of its bits), the choice selected and how many there are (the card's saves and the way back,
// last), its pad and its actions (0 the next choice, 1 the previous one, 2 the choice taken, 3 the way back)
class PadSaveCode : public SaveCode
{
public:
    enum ScreenBits : u32
    {
        ScreenShift = 8,
        ScreenMask = 0xFF00,
    };

    enum Action : u32
    {
        ActionNext = 0,
        ActionPrevious = 1,
        ActionTake = 2,
        ActionBack = 3,
    };

    // The screens listing the card's saves
    static constexpr s32 FirstSavesScreen = 11;
    static constexpr s32 LastSavesScreen = 12;
    // While loading only the saves that hold one (and the way back) can be chosen
    static constexpr u32 OperationLoad = 5;

    u16 selection;
    u16 choices;
    PadButtons* pad;
    ButtonBindings bindings;
    Font* font;

    // A screen shown: on the saves' screens, the first choice that can be chosen selected. Returns the screen
    s32 Show(s32 screen) RETAIL(func_0029F620);
    // The selection moved by the pad's next and previous (round the ends, over the choices that can't be chosen): 0 once a choice
    // was taken or the way back (then selected) was, else 1
    u32 Choose() RETAIL(FUN_0029f6d0);
    // Its vtable's functions: an operation asked of the device, a screen of choices shown (how many it has; Show sets the save
    // slots' screens'), a frame of the screen (the choice taken the screen's answer: whether it was), the destructor and the
    // drawing of the screen (the operation's message, or the screen's message and choices)
    u32 Ask(u32 operation, u32 file) RETAIL(FUN_002a7780);
    u32 ShowChoices(u32 screen) RETAIL(func_002A77A8);
    u32 Waiting(u32 screen) RETAIL(FUN_002a7820);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002a7718);
    void Draw(Renderer* renderer) RETAIL(FUN_002a78b0);
    // The choice selected made the screen's answer
    void Answer(u32 screen) RETAIL(FUN_0029f888);
    void DrawMessage(s32 operation, Renderer* renderer) RETAIL(FUN_0029f9e8);
    void DrawChoices(s32 screen, Renderer* renderer) RETAIL(FUN_0029fae8);

    bool CanChoose(u32 choice) const
    {
        if ((bits & OperationMask) != OperationLoad || choice + 1 == choices)
        {
            return true;
        }

        return static_cast<FolderFile*>(device->mainFile)->summaries[choice]->HasSave();
    }

    void SetAnswer(u32 answer)
    {
        bits = (bits & ~AnswerMask) | answer << AnswerShift;
    }

    // The first choice's answer, with the slot chosen
    void SetChosen(u32 slot)
    {
        bits = (((bits & ~AnswerMask) | AnswerFirst << AnswerShift) & ~ChosenMask) | (slot & 0xF) << ChosenShift;
    }
};
CHECK_OFFSET(PadSaveCode, selection, 0x1C);
CHECK_OFFSET(PadSaveCode, bindings, 0x24);
CHECK_OFFSET(PadSaveCode, font, 0x30);

s32 PadSaveCode::Show(s32 screen)
{
    bits = (bits & ~ScreenMask) | (static_cast<u32>(screen) << ScreenShift & ScreenMask);
    selection = 0;
    if (screen > LastSavesScreen || screen < FirstSavesScreen)
    {
        return screen;
    }

    choices = static_cast<FolderFile*>(device->mainFile)->count + 1;
    while (!CanChoose(selection))
    {
        selection++;
    }

    return screen;
}

u32 PadSaveCode::Choose()
{
    u32 last = static_cast<u32>(choices) - 1;
    if (bindings.Has(pad, ActionNext, 1) != 0)
    {
        do
        {
            selection = selection < last ? selection + 1 : 0;
        } while (!CanChoose(selection));
    }
    else if (bindings.Has(pad, ActionPrevious, 1) != 0)
    {
        do
        {
            selection = selection == 0 ? last : selection - 1;
        } while (!CanChoose(selection));
    }

    if (bindings.Has(pad, ActionTake, 1) != 0)
    {
        return 0;
    }

    if (bindings.Has(pad, ActionBack, 1) != 0)
    {
        selection = last;
        return 0;
    }

    return 1;
}

u32 PadSaveCode::Ask(u32 operation, u32 file)
{
    SetScreen(MessageScreen);
    return device->Ask(operation, file);
}

u32 PadSaveCode::ShowChoices(u32 screen)
{
    SetScreen(screen);
    selection = 0;
    switch (screen)
    {
    case 1:
    case 2:
    case 3:
    case 6:
    case 9:
    case 10:
        choices = 2;
        break;
    case 5:
        choices = 3;
        break;
    case 4:
    case 7:
    case 8:
    case 13:
    case 14:
    case 15:
        choices = 1;
        break;
    default:
        break;
    }

    return screen;
}

u32 PadSaveCode::Waiting(u32 screen)
{
    if (SaveCode::Waiting(screen) != 0)
    {
        return 1;
    }

    if (screen - 1 >= Screens - 1)
    {
        return 0;
    }

    if (Choose() != 0)
    {
        return 0;
    }

    Answer(screen);
    return 1;
}

void PadSaveCode::Destroy(u32 destroyFlags)
{
    bindings.Destroy(BindingsDestroyFlags);
    vtable = g_SaveCodeVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PadSaveCode::Draw(Renderer* renderer)
{
    if (device->State() != 0)
    {
        return;
    }

    u32 screen = Screen();
    if (screen == MessageScreen)
    {
        DrawMessage(static_cast<s32>(device->Running()), renderer);
    }
    else if (screen != CancelSaveScreen && screen < Screens)
    {
        DrawChoices(static_cast<s32>(screen), renderer);
    }
}

void PadSaveCode::Answer(u32 screen)
{
    switch (screen)
    {
    case 1:
    case 2:
        // "Continue wihout saving", "Retry"
        if (selection == 0)
        {
            SetAnswer(AnswerThird);
        }
        else if (selection == 1)
        {
            SetChosen(0);
        }

        return;
    case 3:
    case 6:
    case 9:
    case 10:
        // "Yes", "No"
        if (selection == 0)
        {
            SetChosen(0);
        }
        else if (selection == 1)
        {
            SetAnswer(AnswerBack);
        }

        return;
    case 5:
        // "Format", "Continue wihout saving", "Cancel"
        if (selection == 0)
        {
            SetChosen(0);
        }
        else if (selection == 1)
        {
            SetAnswer(AnswerThird);
        }
        else if (selection == 2)
        {
            SetAnswer(AnswerBack);
        }

        return;
    case 4:
    case 7:
    case 8:
    case 13:
    case 14:
    case 15:
        SetAnswer(AnswerBack);
        return;
    case FirstSavesScreen:
    case LastSavesScreen:
        if (selection + 1 == choices)
        {
            SetAnswer(AnswerBack);
        }
        else
        {
            SetChosen(selection);
        }

        return;
    default:
        return;
    }
}

void PadSaveCode::DrawMessage(s32 operation, Renderer* renderer)
{
    String text = {nullptr, 0, 0};
    switch (Operation())
    {
    case 1:
    case 3:
    case 4:
    case 5:
        if (operation != 0 && operation >= 0 && operation < static_cast<s32>(OperationMessages))
        {
            MessageText(g_OperationMessages[operation], &text);
        }

        break;
    default:
        break;
    }

    if (text.length != 0)
    {
        u32 colour;
        GetColor(&colour, TextColour);
        renderer->colour = colour;
        renderer->font = font;
        renderer->textScale.y = TitleScale;
        renderer->textFlags = TextFlags;
        renderer->textScale.x = TitleScale;
        QueueText(renderer, text.string, TextX, TitleY);
    }

    StringDestroy(&text);
}

void PadSaveCode::DrawChoices(s32 screen, Renderer* renderer)
{
    String title = {nullptr, 0, 0};
    u32 colour;
    GetColor(&colour, TextColour);
    renderer->colour = colour;
    renderer->textScale.y = TitleScale;
    renderer->font = font;
    renderer->textFlags = TextFlags;
    renderer->textScale.x = TitleScale;
    if (screen != 0 && screen >= 0 && screen < static_cast<s32>(Screens))
    {
        MessageText(g_ScreenMessages[screen], &title);
    }

    if (title.length != 0)
    {
        QueueText(renderer, title.string, TextX, TitleY);
    }

    for (u32 choice = 0; choice < choices; choice++)
    {
        u32 choiceColour;
        GetColor(&choiceColour, choice != selection ? UnselectedColour : TextColour);
        String text = {nullptr, 0, 0};
        const s32* messages = nullptr;
        switch (screen)
        {
        case 1:
        case 2:
            messages = g_RetryChoices;
            break;
        case 3:
            messages = g_CreateChoices;
            break;
        case 4:
            messages = g_InsertCardChoices;
            break;
        case 5:
            messages = g_UnformattedChoices;
            break;
        case 6:
            messages = g_FormatChoices;
            break;
        case 7:
            messages = g_InsertSaveChoices;
            break;
        case 8:
            messages = g_InsertRoomChoices;
            break;
        case 9:
            messages = g_OverwriteChoices;
            break;
        case 10:
            messages = g_CancelSaveChoices;
            break;
        case FirstSavesScreen:
        case LastSavesScreen:
            if (choice + 1 == choices)
            {
                MessageText(CancelMessage, &text);
            }
            else
            {
                FolderSummary* summary = static_cast<FolderFile*>(device->mainFile)->summaries[choice];
                if (summary->HasSave())
                {
                    CallVirtual<void>(summary, summary->vtable, FolderSummary::DescribeSlot, &text);
                }
                else
                {
                    MessageText(EmptyMessage, &text);
                }
            }

            break;
        case 13:
            messages = g_FormatFailedChoices;
            break;
        case 14:
            messages = g_SaveFailedChoices;
            break;
        case 15:
            messages = g_LoadFailedChoices;
            break;
        default:
            break;
        }

        if (messages != nullptr)
        {
            MessageText(messages[choice], &text);
        }

        renderer->colour = choiceColour;
        renderer->textScale.y = ChoiceScale;
        renderer->textScale.x = ChoiceScale;
        QueueText(renderer, text.string, TextX, static_cast<f32>(static_cast<s32>(choice)) * ChoiceSpacing + ChoicesY);
        StringDestroy(&text);
    }

    StringDestroy(&title);
}
