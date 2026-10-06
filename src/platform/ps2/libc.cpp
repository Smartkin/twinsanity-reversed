#include "common.h"
#include "retail/libc.h"

#include <kernel.h>
#include <stdarg.h>
#include <stdio.h>

// The C library's PS2 side: what the game printed went to the debugger's console (Sony's printf, here on PS2SDK's deci2), its
// exit, and what PS2SDK's libraries call that the toolchain's newlib doesn't have
namespace
{
// The TTY's line: a line longer than it goes out in parts, each with its character and the terminator
constexpr s32 TtyLineSize = 0x80;
constexpr s32 TtyLineFull = TtyLineSize - 2;

// What printf prints of a call, its terminator included
constexpr u32 PrintfSize = 0x100;
}

extern "C"
{
    // PS2SDK's deci2 output to the debugger's console (deci2.h has no C linkage)
    int kputs(char* text);

    // The retail crt0's exit: ends the program (with status 0, whatever it's given)
    [[noreturn]] void RetailExit(s32 status) RETAIL(FUN_001000e0);
    // The program's end (entry.cpp's)
    [[noreturn]] void ProgramExit(int status);
    // abort
    [[noreturn]] void Abort() RETAIL(FUN_002c79d8);
}

namespace
{
// The TTY's line, sent to the debugger's console (deci2's kputs) at a line's end or when it's full
char g_TtyLine[TtyLineSize];
s32 g_TtyLength;

void TtyPutCharacter(char character)
{
    if (g_TtyLength >= TtyLineFull)
    {
        g_TtyLine[g_TtyLength] = '\0';
        g_TtyLength = 0;
        kputs(g_TtyLine);
    }

    g_TtyLine[g_TtyLength++] = character;
    if (character == '\n')
    {
        g_TtyLine[g_TtyLength] = '\0';
        g_TtyLength = 0;
        kputs(g_TtyLine);
    }
}

void TtyPutText(const char* text)
{
    s32 enabled = DIntr();
    while (*text != '\0')
    {
        TtyPutCharacter(*text++);
    }

    if (enabled != 0)
    {
        EIntr();
    }
}
}

void Abort()
{
    RetailExit(1);
}

void RetailExit(s32)
{
    ProgramExit(0);
}

extern "C"
{
    // Sony's printf (scePrintf) wrote to the debugger's console a line at a time, with interrupts off. What a call prints past
    // 255 characters is cut (PS2SDK's messages and libmpeg's are a line)
    int printf(const char* format, ...)
    {
        char text[PrintfSize];
        va_list arguments;
        va_start(arguments, format);
        int length = vsnprintf(text, sizeof(text), format, arguments);
        va_end(arguments);
        TtyPutText(text);
        return length;
    }

    int puts(const char* text)
    {
        TtyPutText(text);
        TtyPutText("\n");
        return static_cast<int>(RetailLibc::StringLength(text)) + 1;
    }

    // BSD's, which newer PS2SDKs copy their paths and names with: what fits of the text, always ended, and the text's length
    size_t strlcpy(char* destination, const char* source, size_t size)
    {
        size_t length = RetailLibc::StringLength(source);
        if (size != 0)
        {
            size_t count = length < size ? length : size - 1;
            RetailLibc::MemoryCopy(destination, source, count);
            destination[count] = '\0';
        }

        return length;
    }
}
