#include "common.h"
#include "retail/libc.h"

#include <kernel.h>

// The C library's heap's break on the PS2: from the program's end up to the end of the kernel's heap
namespace
{
// ENOMEM
constexpr s32 OutOfMemory = 12;
}

extern "C"
{
    // sbrk's break, the program's end to start with
    extern u8* g_HeapEnd RETAIL(D_002EABE4);
    // __errno: errno is the reentrancy block's first word
    s32* ErrorNumber() RETAIL(FUN_002c7040);
}

// The break moved past the kernel's end of the heap fails with ENOMEM
void* RetailLibc::Sbrk(s32 increment)
{
    s32 enabled = DIntr();
    u8* end = g_HeapEnd;
    u8* moved = end + increment;
    if (static_cast<u8*>(EndOfHeap()) < moved)
    {
        *ErrorNumber() = OutOfMemory;
        if (enabled != 0)
        {
            EIntr();
        }

        return reinterpret_cast<void*>(-1);
    }

    g_HeapEnd = moved;
    if (enabled != 0)
    {
        EIntr();
    }

    return end;
}
