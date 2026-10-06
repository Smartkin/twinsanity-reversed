#include "platform/stream.h"

// The desktop's side of Platform::Stream: nothing read yet
namespace Platform::Stream
{
s32 Initialise(s32 channels, void* workMemory)
{
    return 0;
}

s32 Update()
{
    return 0;
}

State GetState()
{
    return {};
}

void AttachBuffer(s32 channel, u32 size, u32 soundAddress, u32 soundSize)
{
}

void DetachBuffer(s32 channel)
{
}

u32 FreeBufferMemory()
{
    return 0;
}

s32 OpenFile(const char* path)
{
    return 0;
}

u32 FileSize()
{
    return 0;
}

void CloseFile(s32 file)
{
}

void Read(s32 channel, s32 file, u32 offset, u32 size, void* destination)
{
}

void ReadSoundBank(s32 channel, u32 bank, s32 file, u32 offset, u32 size)
{
}

bool IsReading(s32 channel)
{
    return false;
}

s32 Wait(s32 channel)
{
    return 0;
}

}
