#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/save.h"
#include "game/string.h"
#include "platform/saves.h"

class MemoryStream;

// A file of a save (still asm): its name, the stream holding its bytes and its size. Its vtable follows 0x28 bytes of members
struct SaveFile
{
    String name;
    u8 unknown0C[0x10];
    MemoryStream* stream;
    u32 size;
    u32 unknown24;
    const GccVTableEntry* vtable;
};
CHECK_SIZE(SaveFile, 0x2C);

// What the save code shows of a save, its date 0xC bytes in
struct SaveSummary
{
    u8 unknown00[0xC];
    SaveDate date;
};

// The icon files every save has (icon.sys and the icon), in a list
struct SaveIconFiles
{
    u8 count;
    u8 capacity;
    u8 unknown02[2];
    SaveFile* iconSys;
    SaveFile** files;
};

// What the save code reads and writes saves through, a save of files on the storage of a port and a slot. The save code asks for
// an operation and steps it with Poll, which says where to go next: what it was given once the operation succeeded, the state it
// is in while it runs, 1 (the state failed) when it failed. The retail game's base class is abstract (vtable D_003065C8, every
// function but the destructor pure), this one is its memory card device, now Platform::Saves'. Its vtable follows 0x20 bytes of
// members
class SaveDevice
{
public:
    enum Flags : u32
    {
        // Bits 0-3: the files the save has besides the icons
        FileCountMask = 0xF,
        // Bits 16-19
        StateMask = 0xF0000,
        StateFailed = 0x10000,
    };

    // The operations the save code asks for, of the file chosen by current for those on a file
    enum Request : u32
    {
        RequestFormat = 2,
        RequestMeasureSave = 3,
        RequestCreateSave = 4,
        RequestWrite = 5,
        RequestRead = 6,
        RequestFindFile = 7,
        RequestWriteAgain = 8,
        RequestReadAgain = 9,
    };

    u32 flags;
    // Bits 0-3: the file the requests on a file are about
    u32 current;
    u32 unknown08;
    u32 unknown0C;
    SaveIconFiles* icons;
    // The file named after the product code every save has besides the icons
    SaveFile* mainFile;
    // The save's files, and the ones the requests on a file choose from
    SaveFile** files;
    SaveFile** requestFiles;
    const GccVTableEntry* vtable;
    s32 port;
    s32 slot;
    // The region and product codes ("BESLES-52568"), and the save's name
    String product;
    String name;
    Platform::Saves::Operation operation;
    Platform::Saves::StorageInfo info;
    // The space the save takes (-1 when it isn't there), the size of the file found
    s32 saveKilobytes;
    s32 fileSize;
    u8 unknown5C[0x80 - 0x5C];
    Platform::Saves::FileEntry fileEntry;

    static SaveDevice* Construct(SaveDevice* device, u32 fileCount, SaveIconFiles* icons, SaveFile* mainFile, const char* name)
        RETAIL(FUN_002a2f78);

    // Its vtable's functions
    // The found file's time of change, into the summary's date (taken from a clock)
    void FileDate(SaveSummary* summary) RETAIL(func_002A8D38);
    u32 Poll(u32 request, u32 next, u32 asked) RETAIL(FUN_002a2ca8);
    // Asks for the request. Returns next, or 1 (the state failed) when it couldn't
    u32 Request(u32 request, u32 next) RETAIL(FUN_002a2dc0);
    void Update() RETAIL(FUN_002a8dd8);
    void Unknown5() RETAIL(FUN_002a8e10);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002a8e18);
    u32 IsMemoryCard() RETAIL(FUN_002a8d90);
    u32 IsFormatted() RETAIL(FUN_002a8da0);
    u32 SaveExists() RETAIL(FUN_002a8db0);
    u32 SaveSize() RETAIL(FUN_002a8dc0);
    s32 FileSize() RETAIL(func_002A8DD0);
    // The bytes a save of its files needs
    u32 SaveSpace() RETAIL(func_002A3110);
    u32 FreeSpace() RETAIL(FUN_002a8ea8);
};
CHECK_OFFSET(SaveDevice, vtable, 0x20);
CHECK_OFFSET(SaveDevice, operation, 0x44);
CHECK_OFFSET(SaveDevice, saveKilobytes, 0x54);
CHECK_SIZE(SaveDevice, 0xC0);

extern "C"
{
    // The icon.sys file's light directions (its sceMcIconSys 0x38 bytes in): the one function of its class in the file of the
    // device's FileDate
    void SetIconLightDirection(u8* iconSys, s32 light, const Vector4* direction) RETAIL(FUN_002a8d00);
}
