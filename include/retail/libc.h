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

void* MemoryCopy(void* destination, const void* source, u32 size) asm("memcpy");
void* MemoryMove(void* destination, const void* source, u32 size) asm("memmove");
void* MemorySet(void* memory, s32 value, u32 size) asm("memset");
u32 StringLength(const char* text) asm("strlen");
char* StringCopy(char* destination, const char* source) asm("strcpy");
// strncpy
char* StringCopyCount(char* destination, const char* source, u32 count) asm("strncpy");
char* StringConcatenate(char* destination, const char* source) asm("strcat");
s32 StringCompare(const char* first, const char* second) asm("strcmp");
// sprintf
s32 Format(char* buffer, const char* format, ...) asm("sprintf");
// toupper and tolower by the game's casing table (newlib's ctype table, which any int indexes: the bytes before it for
// negative chars)
s32 ToUpper(s32 character) RETAIL(FUN_002c7020);
s32 ToLower(s32 character) RETAIL(ToLowerCase);
// qsort (old BSD's: its order of comparisons and of equal elements)
void QuickSort(void* items, u32 count, u32 size, s32 (*compare)(const void*, const void*)) RETAIL(FUN_002c7bc0);
}
