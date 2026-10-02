#pragma once

#include "common.h"

// The game's files, by their paths on the disc ("Crash6\\Crash.bh", either slash, any case), and descriptors
namespace Platform::Files
{
enum OpenFlags : s32
{
    OpenRead = 0x1,
    OpenWrite = 0x2,
    OpenReadWrite = 0x3,
    OpenAppend = 0x100,
    OpenCreate = 0x200,
    OpenTruncate = 0x400,
};

enum Whence : s32
{
    SeekSet = 0,
    SeekCurrent = 1,
    SeekEnd = 2,
};

// Forgets what it knew of the file system (open again after the I/O processor restarted)
void Reset();
// Returns the descriptor, negative on failure
s32 Open(const char* path, s32 flags);
s32 Close(s32 file);
// Return the bytes read or written, negative on failure
s32 Read(s32 file, void* buffer, s32 size);
s32 Write(s32 file, const void* buffer, s32 size);
// Returns the new position
s32 Seek(s32 file, s32 offset, Whence whence);
}
