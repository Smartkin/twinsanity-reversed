#include "platform/memory.h"

#include "retail/libc.h"

// The desktop's side of Platform::Memory and of the C library's heap: the heap's break moves through an arena of the PS2's
// memory size, and the game's pools take all of it but what the retail game left the heap (the PS2's memory.cpp)
namespace
{
constexpr u32 ArenaSize = 0x2000000;
constexpr u32 HeapReserve = 0x2800;
// ENOMEM
constexpr s32 OutOfMemory = 12;

alignas(64) u8 g_Arena[ArenaSize];
}

extern "C"
{
    // sbrk's break: the program's end in the retail data, which the arena takes the place of
    extern u8* g_HeapEnd RETAIL(D_002EABE4);
    // __errno: errno is the reentrancy block's first word
    s32* ErrorNumber() RETAIL(FUN_002c7040);
}

void* RetailLibc::Sbrk(s32 increment)
{
    if (g_HeapEnd < g_Arena || g_HeapEnd > g_Arena + ArenaSize)
    {
        g_HeapEnd = g_Arena;
    }

    u8* end = g_HeapEnd;
    u8* moved = end + increment;
    if (moved < g_Arena || moved > g_Arena + ArenaSize)
    {
        *ErrorNumber() = OutOfMemory;
        return reinterpret_cast<void*>(-1);
    }

    g_HeapEnd = moved;
    return end;
}

u32 Platform::Memory::PoolSpace()
{
    return ArenaSize - HeapReserve;
}

void* Platform::Memory::AllocatePool(u32 size)
{
    return RetailLibc::Malloc(size);
}

void Platform::Memory::Synchronise()
{
}

void Platform::Memory::BeforeDeviceWrite(void*, u32)
{
}

void Platform::Memory::WriteBackCache()
{
}
