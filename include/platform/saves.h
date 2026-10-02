#pragma once

#include "common.h"

// The game's saves, on the storage of a port and a slot (the PS2's memory cards). A save is a directory of files, named by the
// game's region and product codes and the save's name ("/BESLES-52568CRASH" on the PS2). One operation runs at a time: it starts
// when the storage is there and runs in the Updates after, the storage's info refreshed while none does. The operations keep their
// retail numbers, which the game's save code goes by
namespace Platform::Saves
{
enum class Operation : s32
{
    // What Update returns when the check found no memory card
    NoStorage = -1,
    None = 0,
    // The storage is being checked before the operation asked for
    Checking = 1,
    Format = 2,
    CreateSave = 4,
    MeasureSave = 7,
    FindFile = 8,
    Read = 10,
    Write = 11,
};

// How an operation went: Succeeded 1, 0 when it failed (Error then says why, Step where), -1 while it hasn't finished
struct Result
{
    s32 succeeded;
    s32 step;
    s32 error;
};

// What the storage is: Type 2 for a PS2 memory card (present), its free space in kilobytes, 1 when it's formatted
struct StorageInfo
{
    s32 type;
    s32 freeKilobytes;
    s32 formatted;
};

// A file's entry in its save's directory (libmc's, on the PS2): its times (a reserved byte, then second, minute, hour, day, month
// and the year in 16 bits) and size
struct FileEntry
{
    u8 created[8];
    u8 modified[8];
    u32 size;
    u16 attributes;
    u16 reserved0;
    u32 reserved1[2];
    char name[32];
};
CHECK_SIZE(FileEntry, 64);

constexpr s32 StorageMemoryCard = 2;
constexpr s32 ErrorNotEnoughSpace = -3;
constexpr s32 ErrorNoSave = -4;

// The game's region and product codes ("BE", "SLES-52568"). Returns false when the storage couldn't be set up
bool Initialise(const char* region, const char* product);

// Moves the operation on (the storage of the port and slot checked while none runs) and copies the storage's info to info (when
// it's given). Returns the operation running
Operation Update(s32 port, s32 slot, StorageInfo* info);
// How the operation last asked for went (they're all forgotten when one starts)
Result GetResult(Operation operation);

// Each starts an operation, false when one runs or there's no memory card (it then fails at once)
bool Format(s32 port, s32 slot);
// Makes the save's directory (it has to have the space for a directory)
bool CreateSave(s32 port, s32 slot, const char* save);
// The space the save takes, in kilobytes, to *kilobytes (-1 when there's no such save)
bool MeasureSave(s32 port, s32 slot, const char* save, s32* kilobytes);
// The file's size (-1 when there's no such file) and its directory entry, when given
bool FindFile(s32 port, s32 slot, const char* file, const char* save, s32* size, FileEntry* entry);
// Writes size bytes into the file of the save (which has to be there), made when it isn't
bool Write(s32 port, s32 slot, const char* save, const char* file, const void* data, s32 size);
bool Read(s32 port, s32 slot, const char* save, const char* file, void* buffer, s32 size);

// The kilobytes a file of size bytes takes, and the bytes a save of files files taking kilobytes needs, with its directory's
// entries
u32 FileKilobytes(u32 size);
u32 SaveBytes(u32 kilobytes, u32 files);
}
