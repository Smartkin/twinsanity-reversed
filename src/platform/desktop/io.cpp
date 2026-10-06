#include "platform/io.h"

// The desktop's side of Platform::Io: there's no I/O processor
namespace Platform::Io
{
void Initialise()
{
}

void Restart(const char* image)
{
}

s32 LoadDriver(const char* path)
{
    return 0;
}

void InitialiseHeap()
{
}

void* AllocateHeap(s32 size)
{
    return nullptr;
}

s32 FreeHeap(void* address)
{
    return 0;
}

}
