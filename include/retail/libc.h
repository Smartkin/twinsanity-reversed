#pragma once

#include "common.h"

// The game's C library, Sony's newlib, which the asm still has. Named apart from PS2SDK's
namespace RetailLibc
{
void* Malloc(u32 size) RETAIL(FUN_002c7a88);
// The heap's break moved by increment bytes: returns where it was (-1 past the memory's end)
void* Sbrk(s32 increment) RETAIL(sbrk);
int AtExit(void (*function)()) RETAIL(FUN_002c79e8);

void* MemoryCopy(void* destination, const void* source, u32 size) RETAIL(CopyBytesIntoMemory);
void* MemoryMove(void* destination, const void* source, u32 size) RETAIL(FUN_002c70fc);
void* MemorySet(void* memory, s32 value, u32 size) RETAIL(memset);
u32 StringLength(const char* text) RETAIL(GetStringLength);
char* StringCopy(char* destination, const char* source) RETAIL(CopyString);
// strncpy
char* StringCopyCount(char* destination, const char* source, u32 count) RETAIL(StringCopy);
char* StringConcatenate(char* destination, const char* source) RETAIL(DoConcat);
s32 StringCompare(const char* first, const char* second) RETAIL(StringDifference);
// sprintf
s32 Format(char* buffer, const char* format, ...) RETAIL(FormatString);
// toupper, by the casing table
s32 ToUpper(s32 character) RETAIL(FUN_002c7020);
}
