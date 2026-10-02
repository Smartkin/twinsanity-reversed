#pragma once

#include "common.h"

// The memory the game's own allocators manage (the heap manager's and the disk manager's pools)
namespace Platform::Memory
{
// How much there is for the pools. The retail game sized its heap manager's pool to take what its executable and C library
// left of the console's memory after the disk manager's
u32 PoolSpace();
void* AllocatePool(u32 size);
// Waits until the writes so far are in memory, for hardware that reads it (the PS2's sync)
void Synchronise();
// The hardware is about to write the memory (DMA): what the processor's cache holds of it is dropped, it would be written back
// over what the hardware wrote (the PS2's InvalidDCache)
void BeforeDeviceWrite(void* memory, u32 size);
// Writes what the processor's cache holds back to memory, for hardware about to read it (the PS2's FlushCache(0))
void WriteBackCache();
}
