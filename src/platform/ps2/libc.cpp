#include "common.h"
#include "retail/libc.h"

#include <kernel.h>
#include <stdarg.h>
#include <stdio.h>

// The C library is the toolchain's newlib and PS2SDK's libkernel where they behave like the game's own (Sony's newlib: sprintf,
// the string functions; ps2sdk.txt gives the asm their retail names). Here the game's functions they do otherwise, Sony's
// newlib as it was (malloc.cpp has its heap, libm.cpp its expf), memmove, and what PS2SDK's libraries call that neither has
namespace
{
// atexit's block of functions: the first is in the reentrancy block, more get allocated
struct AtExitBlock
{
    AtExitBlock* next;
    s32 count;
    void (*functions[32])();
};
CHECK_SIZE(AtExitBlock, 0x88);

// Old newlib's struct _reent, the part the game's code uses
struct Reent
{
    s32 errorNumber;
    u8 unknown04[0x54];
    u32 randomNext;
    u8 unknown5C[0xEC];
    AtExitBlock* atExit;
    AtExitBlock atExit0;
};
CHECK_OFFSET(Reent, randomNext, 0x58);
CHECK_OFFSET(Reent, atExit, 0x148);
CHECK_OFFSET(Reent, atExit0, 0x14C);

constexpr u8 CtypeUpper = 0x1;
constexpr u8 CtypeLower = 0x2;
}

extern "C"
{
    // _impure_ptr: the C library's reentrancy block
    extern Reent* g_Impure RETAIL(D_002EA3CC);
    // _ctype_ + 1: the casing of every character, indexed by the character (EOF at -1)
    extern const u8 g_CasingTable[] RETAIL(CasingTable);

    // PS2SDK's deci2 output to the debugger's console (deci2.h has no C linkage)
    int kputs(char* text);

    // The retail crt0's exit: ends the program (with status 0, whatever it's given)
    [[noreturn]] void RetailExit(s32 status) RETAIL(FUN_001000e0);
    // The program's end (entry.cpp's)
    [[noreturn]] void ProgramExit(int status);

    // __errno: errno is the reentrancy block's first word
    s32* ErrorNumber() RETAIL(FUN_002c7040);
    s32 CompareStrings(const char* first, const char* second, u32 count) RETAIL(CompareStrings);
    s32 GetRand();
    // abort
    [[noreturn]] void Abort() RETAIL(FUN_002c79d8);
}

namespace
{
// The table's byte for any int: negative chars read the bytes before it (the rodata's text before _ctype_)
u8 Casing(s32 character)
{
    return *reinterpret_cast<const u8*>(reinterpret_cast<u32>(g_CasingTable) + character);
}

using Compare = s32 (*)(const void*, const void*);

// qsort's ways of swapping elements (SWAPINIT, long being 64 bits): one long, longs, or bytes when the array or the size
// isn't a multiple of a long
enum SwapKind : s32
{
    SwapLong = 0,
    SwapLongs = 1,
    SwapBytes = 2,
};

// swapfunc: the size in bytes, taken as unsigned for the count of longs
void SwapRange(u8* first, u8* second, s32 size, s32 kind)
{
    if (kind <= SwapLongs)
    {
        u64* a = reinterpret_cast<u64*>(first);
        u64* b = reinterpret_cast<u64*>(second);
        s64 count = static_cast<u32>(size) / sizeof(u64);
        do
        {
            u64 kept = *a;
            *a++ = *b;
            *b++ = kept;
        } while (--count > 0);
    }
    else
    {
        s64 count = static_cast<u32>(size);
        do
        {
            u8 kept = *first;
            *first++ = *second;
            *second++ = kept;
        } while (--count > 0);
    }
}

void Swap(u8* first, u8* second, u32 size, s32 kind)
{
    if (kind == SwapLong)
    {
        u64 kept = *reinterpret_cast<u64*>(first);
        *reinterpret_cast<u64*>(first) = *reinterpret_cast<u64*>(second);
        *reinterpret_cast<u64*>(second) = kept;
    }
    else
    {
        SwapRange(first, second, static_cast<s32>(size), kind);
    }
}

u8* MedianOfThree(u8* a, u8* b, u8* c, Compare compare)
{
    return compare(a, b) < 0 ? (compare(b, c) < 0 ? b : (compare(a, c) < 0 ? c : a))
                             : (compare(b, c) > 0 ? b : (compare(a, c) < 0 ? a : c));
}

void InsertionSort(u8* items, u32 count, u32 size, Compare compare, s32 kind)
{
    for (u8* placed = items + size; placed < items + count * size; placed += size)
    {
        for (u8* item = placed; item > items && compare(item - size, item) > 0; item -= size)
        {
            Swap(item, item - size, size, kind);
        }
    }
}

// The TTY's line, sent to the debugger's console (deci2's kputs) at a line's end or when it's full
char g_TtyLine[0x80];
s32 g_TtyLength;

void TtyPutCharacter(char character)
{
    if (g_TtyLength >= 0x7E)
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

s32* ErrorNumber()
{
    return &g_Impure->errorNumber;
}

s32 RetailLibc::ToUpper(s32 character)
{
    return (Casing(character) & CtypeLower) != 0 ? character - 0x20 : character;
}

s32 RetailLibc::ToLower(s32 character)
{
    return (Casing(character) & CtypeUpper) != 0 ? character + 0x20 : character;
}

// strncasecmp: the loop lowers the characters as signed chars, the result as unsigned ones
s32 CompareStrings(const char* first, const char* second, u32 count)
{
    if (count == 0)
    {
        return 0;
    }

    while (count-- != 0 && RetailLibc::ToLower(static_cast<s8>(*first)) == RetailLibc::ToLower(static_cast<s8>(*second)))
    {
        if (count == 0 || *first == '\0' || *second == '\0')
        {
            break;
        }

        first++;
        second++;
    }

    return RetailLibc::ToLower(static_cast<u8>(*first)) - RetailLibc::ToLower(static_cast<u8>(*second));
}

// rand: old newlib's 32 bit generator, its state in the reentrancy block
s32 GetRand()
{
    u32 next = g_Impure->randomNext * 1103515245 + 12345;
    g_Impure->randomNext = next;
    return static_cast<s32>(next & 0x7FFFFFFF);
}

void Abort()
{
    RetailExit(1);
}

void RetailExit(s32)
{
    ProgramExit(0);
}

int RetailLibc::AtExit(void (*function)())
{
    AtExitBlock* block = g_Impure->atExit;
    if (block == nullptr)
    {
        block = &g_Impure->atExit0;
        g_Impure->atExit = block;
    }

    if (block->count >= 32)
    {
        block = static_cast<AtExitBlock*>(Malloc(sizeof(AtExitBlock)));
        if (block == nullptr)
        {
            return -1;
        }

        block->count = 0;
        block->next = g_Impure->atExit;
        g_Impure->atExit = block;
    }

    // The function goes in before the count (a count of -1 puts it over the count)
    s32 index = block->count;
    *reinterpret_cast<void (**)()>(reinterpret_cast<u32>(block->functions) + 4 * index) = function;
    block->count = index + 1;
    return 0;
}

// qsort: Bentley and McIlroy's (4.4BSD's), insertion sort when a partition swapped nothing, recursing on the left part and
// iterating on the right one
void RetailLibc::QuickSort(void* items, u32 count, u32 size, Compare compare)
{
    u8* a = static_cast<u8*>(items);
    for (;;)
    {
        s32 kind = (reinterpret_cast<u32>(a) % sizeof(u64) != 0 || size % sizeof(u64) != 0) ? SwapBytes
                   : size == sizeof(u64)                                                 ? SwapLong
                                                                                         : SwapLongs;
        bool swapped = false;
        if (count < 7)
        {
            InsertionSort(a, count, size, compare, kind);
            return;
        }

        u8* middle = a + (count / 2) * size;
        if (count > 7)
        {
            u8* low = a;
            u8* high = a + (count - 1) * size;
            if (count > 40)
            {
                s32 step = static_cast<s32>((count / 8) * size);
                low = MedianOfThree(low, low + step, low + 2 * step, compare);
                middle = MedianOfThree(middle - step, middle, middle + step, compare);
                high = MedianOfThree(high - 2 * step, high - step, high, compare);
            }

            middle = MedianOfThree(low, middle, high, compare);
        }

        Swap(a, middle, size, kind);
        u8* lessEnd = a + size;
        u8* left = lessEnd;
        u8* right = a + (count - 1) * size;
        u8* greaterStart = right;
        for (;;)
        {
            s32 order;
            while (left <= right && (order = compare(left, a)) <= 0)
            {
                if (order == 0)
                {
                    swapped = true;
                    Swap(lessEnd, left, size, kind);
                    lessEnd += size;
                }

                left += size;
            }

            while (left <= right && (order = compare(right, a)) >= 0)
            {
                if (order == 0)
                {
                    swapped = true;
                    Swap(right, greaterStart, size, kind);
                    greaterStart -= size;
                }

                right -= size;
            }

            if (left > right)
            {
                break;
            }

            Swap(left, right, size, kind);
            swapped = true;
            left += size;
            right -= size;
        }

        if (!swapped)
        {
            InsertionSort(a, count, size, compare, kind);
            return;
        }

        // The equal elements at both ends go to the middle: min() of an int and of a size_t's difference compares unsigned
        u8* end = a + count * size;
        s32 moved = lessEnd - a < left - lessEnd ? lessEnd - a : left - lessEnd;
        if (moved > 0)
        {
            SwapRange(a, left - moved, moved, kind);
        }

        u32 equalHigh = greaterStart - right;
        u32 rest = static_cast<u32>(end - greaterStart) - size;
        moved = static_cast<s32>(equalHigh < rest ? equalHigh : rest);
        if (moved > 0)
        {
            SwapRange(left, end - moved, moved, kind);
        }

        s32 part = left - lessEnd;
        if (static_cast<u32>(part) > size)
        {
            QuickSort(a, static_cast<u32>(part) / size, size, compare);
        }

        part = greaterStart - right;
        if (static_cast<u32>(part) <= size)
        {
            return;
        }

        a = end - part;
        count = static_cast<u32>(part) / size;
    }
}

extern "C"
{
    // Sony's memmove: an overlap from behind copied backwards a byte at a time, anything else forwards by memcpy (PS2SDK's is
    // the game's own code: quadwords when both ends are 16 byte aligned). newlib's small memmove copies bytes, and the disk
    // manager's compaction moves megabytes
    __attribute__((optimize("no-tree-loop-distribute-patterns"))) void* memmove(void* destination, const void* source,
                                                                                size_t size)
    {
        u32 from = reinterpret_cast<u32>(source);
        u32 to = reinterpret_cast<u32>(destination);
        if (from < to && to < from + size)
        {
            u8* end = static_cast<u8*>(destination) + size;
            const u8* sourceEnd = static_cast<const u8*>(source) + size;
            while (size-- != 0)
            {
                *--end = *--sourceEnd;
            }

            return destination;
        }

        return RetailLibc::MemoryCopy(destination, source, size);
    }

    // Sony's printf (scePrintf) wrote to the debugger's console a line at a time, with interrupts off. What a call prints past
    // 255 characters is cut (PS2SDK's messages and libmpeg's are a line)
    int printf(const char* format, ...)
    {
        char text[0x100];
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
