#pragma once

#include "common.h"
#include "platform/files.h"

// The PS2's memory cards, by port and slot, for Platform::Saves and the start-up: libmc's interface, on PS2SDK's libmc. Every call
// but Initialise starts an operation Sync finishes, which gives its result
namespace Platform::MemoryCard
{
// An entry of a directory listing (a table of them is 64 byte aligned: the I/O processor's DMA writes it, and only whole
// quadwords)
struct alignas(64) DirectoryEntry
{
    u8 created[8];
    u8 modified[8];
    u32 fileSizeByte;
    u16 attributes;
    u16 reserved0;
    u32 reserved1[2];
    u8 name[32];
};
CHECK_SIZE(DirectoryEntry, 64);

enum SyncMode : s32
{
    SyncWait = 0,
    SyncNoWait = 1,
};

// What Sync returns: nothing was started, a call is running, one finished
enum SyncStatus : s32
{
    SyncNothing = -1,
    SyncRunning = 0,
    SyncFinished = 1,
};

// Returns 0 when it's ready. Starting it again does nothing
s32 Initialise();
s32 GetInfo(s32 port, s32 slot, s32* type, s32* freeClusters, s32* format);
// The flags: the I/O processor's file flags (Files::OpenFlags)
s32 Open(s32 port, s32 slot, const char* name, s32 flags);
s32 Close(s32 file);
s32 Read(s32 file, void* buffer, s32 size);
s32 Write(s32 file, const void* buffer, s32 size);
s32 MakeDirectory(s32 port, s32 slot, const char* name);
s32 ChangeDirectory(s32 port, s32 slot, const char* directory, char* currentDirectory);
// The table: 64 byte aligned, where the I/O processor can write
s32 GetDirectory(s32 port, s32 slot, const char* name, u32 mode, s32 maxEntries, DirectoryEntry* table);
s32 Delete(s32 port, s32 slot, const char* name);
s32 Format(s32 port, s32 slot);
// Returns a SyncStatus: when a call finished, function is what it was and result its result
s32 Sync(SyncMode mode, s32* function, s32* result);
}
