#pragma once

#include "common.h"

// The C library: the toolchain's newlib and PS2SDK's libkernel where they behave like the game's own (Sony's newlib), by their
// own names, and the game's own functions where they behave otherwise (src/platform/ps2/libc.cpp, malloc.cpp, libm.cpp), by
// the retail names the asm calls
namespace RetailLibc
{
void* Malloc(u32 size) RETAIL(FUN_002c7a88);
void Free(void* memory) RETAIL(FUN_002c7ad8);
// The heap's break moved by increment bytes: returns where it was (-1 past the memory's end)
void* Sbrk(s32 increment) RETAIL(sbrk);
int AtExit(void (*function)()) RETAIL(FUN_002c79e8);

void* MemoryCopy(void* destination, const void* source, u32 size) RETAIL(memcpy);
void* MemoryMove(void* destination, const void* source, u32 size) RETAIL(memmove);
void* MemorySet(void* memory, s32 value, u32 size) RETAIL(memset);
u32 StringLength(const char* text) RETAIL(strlen);
char* StringCopy(char* destination, const char* source) RETAIL(strcpy);
// strncpy
char* StringCopyCount(char* destination, const char* source, u32 count) RETAIL(strncpy);
char* StringConcatenate(char* destination, const char* source) RETAIL(strcat);
s32 StringCompare(const char* first, const char* second) RETAIL(strcmp);
// sprintf
s32 Format(char* buffer, const char* format, ...) RETAIL(sprintf);
// toupper and tolower by the game's casing table (newlib's ctype table, which any int indexes: the bytes before it for
// negative chars)
s32 ToUpper(s32 character) RETAIL(FUN_002c7020);
s32 ToLower(s32 character) RETAIL(ToLowerCase);
// The casing table's bits (newlib's _U and _L): upper and lower case letters
constexpr u8 CasingUpperCase = 0x1;
constexpr u8 CasingLowerCase = 0x2;
// qsort (old BSD's: its order of comparisons and of equal elements)
void QuickSort(void* items, u32 count, u32 size, s32 (*compare)(const void*, const void*)) RETAIL(FUN_002c7bc0);

// atexit's block of functions (newlib's _ATEXIT_SIZE of them): the first is in the reentrancy block, more get allocated
constexpr s32 AtExitBlockSize = 32;

struct AtExitBlock
{
    AtExitBlock* next;
    s32 count;
    void (*functions[AtExitBlockSize])();
};
CHECK_SIZE(AtExitBlock, 0x88);

// Old newlib's struct _reent, the part the game's code uses: errno, rand's state and atexit's functions
struct Reent
{
    s32 errorNumber;
    u8 unused04[0x54];
    u32 randomNext;
    u8 unused5C[0xEC];
    AtExitBlock* atExit;
    AtExitBlock atExit0;
};
CHECK_OFFSET(Reent, randomNext, 0x58);
CHECK_OFFSET(Reent, atExit, 0x148);
CHECK_OFFSET(Reent, atExit0, 0x14C);
}

extern "C"
{
    // _impure_ptr: the C library's reentrancy block
    extern RetailLibc::Reent* g_Impure RETAIL(D_002EA3CC);
}
