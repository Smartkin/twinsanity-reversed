#pragma once

#include "common.h"

// The I/O processor the game's drivers run on (the PS2's IOP: the disc drive, the pads, the memory cards and the sound processor
// are behind it). Platforms without one do nothing here
namespace Platform::Io
{
void Initialise();
// Restarts it with an image of its modules (the disc's IOPRP) and waits until it's back. Initialise it again after
void Restart(const char* image);
// Loads a driver module (a disc path). Returns the module's ID, negative when it didn't load
s32 LoadDriver(const char* path);

// Its memory, for the sound driver's streams
void InitialiseHeap();
void* AllocateHeap(s32 size);
s32 FreeHeap(void* address);
}
