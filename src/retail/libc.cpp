#include "common.h"
#include "retail/libc.h"

// The C library is the platform's where it behaves like the game's own (Sony's newlib: sprintf, the string functions). Here the
// game's functions it does otherwise, Sony's newlib as it was (malloc.cpp has its heap, libm.cpp its expf) and the PS2's
// memmove; the platform's side has the rest (on the PS2 its console output and exit, src/platform/ps2/libc.cpp)
namespace
{
using RetailLibc::AtExitBlock;
using RetailLibc::AtExitBlockSize;
using RetailLibc::CasingLowerCase;
using RetailLibc::CasingUpperCase;

// How far a lower case letter is from its capital
constexpr s32 CaseDistance = 'a' - 'A';

// rand's linear congruential generator, and its results' bits (RAND_MAX)
constexpr u32 RandomMultiplier = 1103515245;
constexpr u32 RandomIncrement = 12345;
constexpr u32 RandomMax = 0x7FFFFFFF;

// qsort sorts fewer elements than this by insertion, takes its pivot from three elements when there are more, and from three
// medians of three past the other
constexpr u32 InsertionSortBelow = 7;
constexpr u32 MedianOfThreeAbove = 7;
constexpr u32 NintherAbove = 40;

}

extern "C"
{
    // _ctype_ + 1: the casing of every character, indexed by the character (EOF at -1)
    extern const u8 g_CasingTable[] RETAIL(CasingTable);

    // __errno: errno is the reentrancy block's first word
    s32* ErrorNumber() RETAIL(FUN_002c7040);
    s32 CompareStrings(const char* first, const char* second, u32 count) RETAIL(CompareStrings);
    s32 GetRand();
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
        u64* firstLong = reinterpret_cast<u64*>(first);
        u64* secondLong = reinterpret_cast<u64*>(second);
        s64 count = static_cast<u32>(size) / sizeof(u64);
        do
        {
            u64 kept = *firstLong;
            *firstLong++ = *secondLong;
            *secondLong++ = kept;
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

}

s32* ErrorNumber()
{
    return &g_Impure->errorNumber;
}

s32 RetailLibc::ToUpper(s32 character)
{
    return (Casing(character) & CasingLowerCase) != 0 ? character - CaseDistance : character;
}

s32 RetailLibc::ToLower(s32 character)
{
    return (Casing(character) & CasingUpperCase) != 0 ? character + CaseDistance : character;
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
    u32 next = g_Impure->randomNext * RandomMultiplier + RandomIncrement;
    g_Impure->randomNext = next;
    return static_cast<s32>(next & RandomMax);
}

int RetailLibc::AtExit(void (*function)())
{
    AtExitBlock* block = g_Impure->atExit;
    if (block == nullptr)
    {
        block = &g_Impure->atExit0;
        g_Impure->atExit = block;
    }

    if (block->count >= AtExitBlockSize)
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
    *reinterpret_cast<void (**)()>(reinterpret_cast<u32>(block->functions) + sizeof(block->functions[0]) * index) = function;
    block->count = index + 1;
    return 0;
}

// qsort: Bentley and McIlroy's (4.4BSD's), insertion sort when a partition swapped nothing, recursing on the left part and
// iterating on the right one
void RetailLibc::QuickSort(void* items, u32 count, u32 size, Compare compare)
{
    u8* base = static_cast<u8*>(items);
    for (;;)
    {
        s32 kind = (reinterpret_cast<u32>(base) % sizeof(u64) != 0 || size % sizeof(u64) != 0) ? SwapBytes
                   : size == sizeof(u64)                                                       ? SwapLong
                                                                                               : SwapLongs;
        bool swapped = false;
        if (count < InsertionSortBelow)
        {
            InsertionSort(base, count, size, compare, kind);
            return;
        }

        u8* middle = base + (count / 2) * size;
        if (count > MedianOfThreeAbove)
        {
            u8* low = base;
            u8* high = base + (count - 1) * size;
            if (count > NintherAbove)
            {
                s32 step = static_cast<s32>((count / 8) * size);
                low = MedianOfThree(low, low + step, low + 2 * step, compare);
                middle = MedianOfThree(middle - step, middle, middle + step, compare);
                high = MedianOfThree(high - 2 * step, high - step, high, compare);
            }

            middle = MedianOfThree(low, middle, high, compare);
        }

        Swap(base, middle, size, kind);
        u8* lessEnd = base + size;
        u8* left = lessEnd;
        u8* right = base + (count - 1) * size;
        u8* greaterStart = right;
        for (;;)
        {
            s32 order;
            while (left <= right && (order = compare(left, base)) <= 0)
            {
                if (order == 0)
                {
                    swapped = true;
                    Swap(lessEnd, left, size, kind);
                    lessEnd += size;
                }

                left += size;
            }

            while (left <= right && (order = compare(right, base)) >= 0)
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
            InsertionSort(base, count, size, compare, kind);
            return;
        }

        // The equal elements at both ends go to the middle: min() of an int and of a size_t's difference compares unsigned
        u8* end = base + count * size;
        s32 moved = lessEnd - base < left - lessEnd ? lessEnd - base : left - lessEnd;
        if (moved > 0)
        {
            SwapRange(base, left - moved, moved, kind);
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
            QuickSort(base, static_cast<u32>(part) / size, size, compare);
        }

        part = greaterStart - right;
        if (static_cast<u32>(part) <= size)
        {
            return;
        }

        base = end - part;
        count = static_cast<u32>(part) / size;
    }
}

#if defined(_EE)
extern "C"
{
    // Sony's memmove: an overlap from behind copied backwards a byte at a time, anything else forwards by memcpy (PS2SDK's is
    // the game's own code: quadwords when both ends are 16 byte aligned). newlib's small memmove copies bytes, and the disk
    // manager's compaction moves megabytes. Elsewhere the host's memmove does the same
    KEEP_LOOPS void* memmove(void* destination, const void* source, size_t size)
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
}
#endif
