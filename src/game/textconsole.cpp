#include "game/memory.h"
#include "game/string.h"
#include "gcc2.h"

// The disk module's console of nine lines of text, which nothing makes (an abstract class: retail's vtable D_002FC948, after the
// lines and the count: 1 the destructor, 2 a text added, 4 cleared, 6 scrolled up; 3 and 5 abstract): the lines, each starting
// with a prompt, and how many are used. A text goes onto the first unused line, and once all nine are used they're scrolled up
struct TextConsole
{
    static constexpr u32 LineCount = 9;

    String lines[LineCount];
    u8 count;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00203f28);
    // The line the text went onto
    u8 Add(const char* text) RETAIL(FUN_00203fb0);
    void Clear() RETAIL(FUN_00204020);
    void Scroll(u32 dropLast) RETAIL(FUN_00204090);
    // The lines moved up one (the first one dropped), then the last line used emptied and one line less used when asked, else the
    // ninth emptied and eight used. With nine used the last used one is past the lines (no caller asks with nine)
    void ScrollLines(u32 dropLast) RETAIL(FUN_002040b0);
};
CHECK_OFFSET(TextConsole, count, 0x6C);
CHECK_OFFSET(TextConsole, vtable, 0x70);

extern "C"
{
    extern const GccVTableEntry g_TextConsoleVTable[] RETAIL(D_002FC948);
    // "> "
    extern const char g_ConsolePrompt[] RETAIL(D_0030A078);
}

namespace
{
constexpr u32 ScrollSlot = 6;
}

void TextConsole::Destroy(u32 destroyFlags)
{
    vtable = g_TextConsoleVTable;
    // GCC 2.9x destroys the lines only when the object isn't null
    if (this != nullptr)
    {
        for (s32 index = LineCount - 1; index >= 0; index--)
        {
            StringDestroy(&lines[index]);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u8 TextConsole::Add(const char* text)
{
    StringAppend(&lines[count], text);
    count++;
    if (count == LineCount)
    {
        CallVirtual<void>(this, vtable, ScrollSlot, 0u);
    }

    return static_cast<u8>(count - 1);
}

void TextConsole::Clear()
{
    for (u8 index = 0; index < LineCount; index++)
    {
        StringAssign(&lines[index], g_ConsolePrompt);
    }

    count = 0;
}

void TextConsole::Scroll(u32 dropLast)
{
    ScrollLines(dropLast);
}

void TextConsole::ScrollLines(u32 dropLast)
{
    for (u8 index = 1; index < LineCount; index++)
    {
        StringAssign(&lines[index - 1], lines[index].string);
    }

    if (dropLast != 0)
    {
        StringAssign(&lines[count], g_ConsolePrompt);
        count--;
        return;
    }

    count = LineCount - 1;
    StringAssign(&lines[LineCount - 1], g_ConsolePrompt);
}
