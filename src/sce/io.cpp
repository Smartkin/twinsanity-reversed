#include "platform/io.h"

// Sony's iopheap calls of the retail sound driver (its streams)
extern "C"
{
    void* sceSifAllocIopHeap(int size)
    {
        return Platform::Io::AllocateHeap(size);
    }

    int sceSifFreeIopHeap(void* address)
    {
        return Platform::Io::FreeHeap(address);
    }
}
