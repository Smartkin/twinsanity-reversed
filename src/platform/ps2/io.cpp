#include "platform/io.h"
#include "stack.h"

#include <iopcontrol.h>
#include <iopheap.h>
#include <loadfile.h>
#include <sifrpc.h>

void Platform::Io::Initialise()
{
    SifInitRpc(0);
}

void Platform::Io::Restart(const char* image)
{
    while (!SifIopReboot(image))
    {
    }

    while (!SifIopSync())
    {
    }
}

// These send the IOP what they build on their stack
s32 Platform::Io::LoadDriver(const char* path)
{
    return OnMainMemoryStack([path] { return SifLoadModule(path, 0, nullptr); });
}

void Platform::Io::InitialiseHeap()
{
    SifInitIopHeap();
}

void* Platform::Io::AllocateHeap(s32 size)
{
    return OnMainMemoryStack([size] { return SifAllocIopHeap(size); });
}

s32 Platform::Io::FreeHeap(void* address)
{
    return OnMainMemoryStack([address] { return SifFreeIopHeap(address); });
}
