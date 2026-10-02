#include "platform/memory.h"

#include "debug.h"
#include "retail/libc.h"

#include <kernel.h>

namespace
{
constexpr u32 MemorySize = 0x2000000;
// What the retail game left the C library's heap before its pools (0x3DDA00, 10 KB past its executable's end): the pools come
// from it too, with their malloc headers
constexpr u32 HeapReserve = 0x2800;
// Test runs (debug.h's test time) start the pools here, past any build's end, so the heap's addresses don't change with the
// build: what the game goes through in the order of its addresses goes in the same order
constexpr u32 TestPoolStart = 0x400000;
}

extern "C" char _end[];

namespace
{
u32 PoolStart()
{
    return g_DebugFixedTime != 0 ? TestPoolStart : reinterpret_cast<u32>(_end);
}
}

u32 Platform::Memory::PoolSpace()
{
    return MemorySize - (PoolStart() + HeapReserve);
}

void* Platform::Memory::AllocatePool(u32 size)
{
    static bool moved;
    if (g_DebugFixedTime != 0 && !moved)
    {
        moved = true;
        u32 now = reinterpret_cast<u32>(RetailLibc::Sbrk(0));
        if (now < TestPoolStart)
        {
            RetailLibc::Sbrk(static_cast<s32>(TestPoolStart - now));
        }
    }

    return RetailLibc::Malloc(size);
}

void Platform::Memory::Synchronise()
{
    asm volatile("sync" : : : "memory");
}

void Platform::Memory::BeforeDeviceWrite(void* memory, u32 size)
{
    u8* begin = static_cast<u8*>(memory);
    InvalidDCache(begin, begin + size - 1);
}

void Platform::Memory::WriteBackCache()
{
    FlushCache(0);
}
