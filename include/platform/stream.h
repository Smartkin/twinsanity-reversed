#pragma once

#include "common.h"

// The disc's files read in the background, on channels: a channel reads one thing at a time, into memory or into the sound
// processor's. The game numbers the channels from 0 (the PS2's are the streams of the I/O processor's sound and stream module).
// Files are opened by their paths on the disc and stay open, by number, until they're closed
namespace Platform::Stream
{
enum class State : s32
{
    Stopped = 0,
    Running = 1,
    // The disc can't be read (its tray was opened): Update checks it until it can be again
    DiscError = 2,
    // The disc can be read again, reading starts over
    Recovering = 4,
};

constexpr s32 NoFile = -1;

// The memory Initialise needs, from the game's heap
constexpr u32 WorkMemorySize = 0x8000;
constexpr u32 WorkMemoryAlignment = 0x40;

// Starts reading with so many channels. Returns negative when it can't read
s32 Initialise(s32 channels, void* workMemory);
// Once a frame: sends what was asked for and follows the disc's state
s32 Update();
State GetState();

// The channel's buffer of size bytes (on the PS2's I/O processor). Music plays from soundAddress in the sound processor's
// memory, soundSize bytes of it (when not 0; the buffer's size otherwise); a channel reading files into memory gives 0 for both
void AttachBuffer(s32 channel, u32 size, u32 soundAddress, u32 soundSize);
void DetachBuffer(s32 channel);
// The memory left for buffers
u32 FreeBufferMemory();

// Returns the file's number. FileSize is its size until the next file is opened, 0 when there's no such file
s32 OpenFile(const char* path);
u32 FileSize();
void CloseFile(s32 file);

// Starts reading size bytes of the file from offset into memory
void Read(s32 channel, s32 file, u32 offset, u32 size, void* destination);
// Starts reading them into the sound processor's memory, as the samples of the sound bank
void ReadSoundBank(s32 channel, u32 bank, s32 file, u32 offset, u32 size);
bool IsReading(s32 channel);
// Waits until the channel has read what it started. Returns 0, or 1 more than a channel whose read the platform left to the
// caller (never on the PS2, which reads them all itself)
s32 Wait(s32 channel);
}
