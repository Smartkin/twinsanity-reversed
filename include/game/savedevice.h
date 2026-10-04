#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/save.h"
#include "game/string.h"
#include "platform/saves.h"

class MemoryStream;
class Stream;
struct TimeClock;

// A save slot's summary in the card folder's file (oleg.h's SaveSummary is the bank's, with its progress after it): when it was
// saved, both bits of the date set while the slot holds a save, and the name of where. Its vtable (D_003066F0, the banks'
// BanksHeader_methods) follows 0x14 bytes of members: 1 the destructor, 2 its text, 3 cleared, 4 its date taken from the clock
// again while the slot holds a save, 5 and 6 read from and written to a stream
class FolderSummary
{
public:
    enum Slots : u32
    {
        DestroySlot = 1,
        DescribeSlot = 2,
        ClearSlot = 3,
        ReadSlot = 5,
        WriteSlot = 6,
    };

    static constexpr u8 SavedBits = 3;

    SaveDate date;
    String name;
    const GccVTableEntry* vtable;

    bool HasSave() const
    {
        return (date.bits & SavedBits) == SavedBits;
    }

    static FolderSummary* Construct(FolderSummary* summary) RETAIL(FUN_002a7bc8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002a7c10);
    // "<name> - h:mm:ss d/m/y"
    void Describe(String* text) RETAIL(FUN_002a0210);
    void Clear() RETAIL(FUN_002a7c70);
    void Refresh() RETAIL(FUN_002a7c90);
    // The date's 8 bytes, then the name
    void Read(Stream* stream) RETAIL(FUN_002a7cc0);
    void Write(Stream* stream) RETAIL(FUN_002a7d18);
};
CHECK_SIZE(FolderSummary, 0x18);

// A file of a save: its name, the date and checksum kept in its bytes, the stream it's read from or written to while that runs, its
// size and the buffer it's in (none: its stream's own memory). A file with a tag starts with the date and ends with the tag and the
// checksum. Its vtable (the base's D_00306698, where 2 to 4 are pure) follows 0x28 bytes of members: 1 its stream made, 2 its
// contents gathered before a write, 3 and 4 its contents read from and written to the stream, 5 the destructor, 6 a read begun,
// 7 a read checked and taken in, 8 a write begun (the stream filled), 9 the write over (the stream let go of)
class SaveFile
{
public:
    enum Slots : u32
    {
        MakeStreamSlot = 1,
        GatherSlot = 2,
        ReadSlot = 3,
        WriteSlot = 4,
        DestroySlot = 5,
        BeginReadSlot = 6,
        EndReadSlot = 7,
        BeginWriteSlot = 8,
        EndWriteSlot = 9,
    };

    String name;
    SaveDate date;
    // Stored 8 bytes before the end with the checksum after it (0: neither, nor the date)
    u32 tag;
    u32 checksum;
    MemoryStream* stream;
    u32 size;
    u8* buffer;
    const GccVTableEntry* vtable;

    // Named, no tag, size or buffer yet
    static SaveFile* Construct(SaveFile* file, const char* name) RETAIL(FUN_002a8298);
    // The base's functions of its vtable: a stream over the buffer (memory of its own without one), filled with 0xA5; a read begun
    // (a new stream) and checked (the tag and the checksum, then the date and the contents read: whether they were good); a write
    // begun (the date the clock's, the contents, the tag and the checksum written) and its stream let go of
    MemoryStream* MakeStream() RETAIL(FUN_002a82e8);
    u32 BeginRead() RETAIL(FUN_002a8478);
    u32 EndRead() RETAIL(FUN_002a25c0);
    u32 BeginWrite() RETAIL(FUN_002a2770);
    u32 EndWrite() RETAIL(FUN_002a84d8);
};
CHECK_SIZE(SaveFile, 0x2C);

// A file the save copies from the disc (the icon): read whole into its stream, written as it is and never read back (its vtable
// D_00306418)
class CopiedFile : public SaveFile
{
public:
    void Gather() RETAIL(FUN_002a7b10);
    void Read(Stream* stream) RETAIL(FUN_002a7b18);
    void Write(Stream* stream) RETAIL(FUN_002a7b20);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002a7a98);
    u32 BeginRead() RETAIL(FUN_002a7ba0);
    u32 EndRead() RETAIL(FUN_002a7ba8);
    u32 BeginWrite() RETAIL(FUN_002a7bb0);
    u32 EndWrite() RETAIL(FUN_002a7bb8);
};
CHECK_SIZE(CopiedFile, 0x2C);

// The card folder's file (named after the product code, its vtable D_00306640): the summaries of the save slots
class FolderFile : public SaveFile
{
public:
    u32 count;
    FolderSummary** summaries;

    void Gather() RETAIL(FUN_002a8520);
    void Read(Stream* stream) RETAIL(FUN_002a8528);
    void Write(Stream* stream) RETAIL(FUN_002a85a8);
    // Every summary cleared (before the folder is read or made)
    void ClearSummaries() RETAIL(FUN_002a87d8);
};
CHECK_OFFSET(FolderFile, summaries, 0x30);

// The PS2 browser's icon.sys (Sony's sceMcIconSys)
struct IconSys
{
    // "PS2D"
    char magic[4];
    u16 reserved04;
    // Where the title's second line starts (bytes)
    u16 lineBreak;
    u32 reserved08;
    // The background's transparency (0-255)
    s32 transparency;
    // The colours of the background's corners, a byte's value in each word
    s32 background[4][4];
    f32 lightDirections[3][4];
    f32 lightColours[3][4];
    f32 ambient[4];
    // The title in Shift-JIS
    u8 title[68];
    // The icon files shown in the list, copying and deleting
    char viewIcon[64];
    char copyIcon[64];
    char deleteIcon[64];
    u8 reserved1C4[0x200];
};
CHECK_SIZE(IconSys, 0x3C4);

// The save's icon.sys (its vtable D_00306348): its title ("~" breaks its line) and the file's bytes
class IconSysFile : public SaveFile
{
public:
    String title;
    IconSys iconSys;

    MemoryStream* MakeStream() RETAIL(FUN_002a89f0);
    void Gather() RETAIL(FUN_002a8a30);
    void Read(Stream* stream) RETAIL(FUN_002a8a38);
    void Write(Stream* stream) RETAIL(FUN_002a8a40);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002a8970);
    void SetTitle(const char* text) RETAIL(FUN_002a8a78);
    void SetViewIcon(const char* icon) RETAIL(FUN_002a8b18);
    void SetCopyIcon(const char* icon) RETAIL(FUN_002a8b38);
    void SetDeleteIcon(const char* icon) RETAIL(FUN_002a8b58);
    // Of 1
    void SetTransparency(f32 opacity) RETAIL_N32(FUN_002a8b78);
    // Colours as RGBA bytes: the background's corners, the ambient light and the lights' (128 the full colour of a light, 128 of
    // alpha the full alpha)
    void SetBackground(s32 corner, u32 colour) RETAIL(FUN_002a8b98);
    void SetAmbient(u32 colour) RETAIL(FUN_002a8be0);
    void SetLightColour(s32 light, u32 colour) RETAIL(FUN_002a8c68);
};
CHECK_OFFSET(IconSysFile, iconSys, 0x38);
CHECK_SIZE(IconSysFile, 0x3FC);

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
        // Bits 4-7: the file of the operations on one (8 and 9)
        FileShift = 4,
        // Bits 8-11: the operation running, 12-15: the one asked for (14 once it's taken)
        RunningShift = 8,
        AskedShift = 12,
        OperationMask = 0xF,
        AskTaken = 14,
        // Bits 16-19
        StateShift = 16,
        StateMask = 0xF0000,
        StateFailed = 0x10000,
        // Bits 20-25: the step the operation is at
        StepShift = 20,
        StepMask = 0x3F,
        // Bit 26: every file is written (the save was just made)
        WritesEverything = 0x4000000,
        // Bits 27-30: the request files the operation goes through
        RequestCountShift = 27,
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
    // When the step began and how long it waits before it asks how its request went (clock units)
    s32 stepStart;
    s32 wait;
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

    // The base class's: an operation asked for (its file for writes and reads of one; 0 when it can't be), the operation stepped
    // (whether it's still running) and the steps of the operations, the state set (returns 1) and the destructor
    u32 Ask(u32 asked, u32 file) RETAIL(FUN_002a08f8);
    u32 Step(TimeClock* clock) RETAIL(FUN_002a0aa8);
    u32 PollReads(u32 request, u32 next, TimeClock* clock) RETAIL(FUN_002a03a8);
    u32 PollWrites(u32 request, u32 next, TimeClock* clock) RETAIL(FUN_002a04f0);
    u32 PollFind(TimeClock* clock) RETAIL(FUN_002a0638);
    u32 StartWritingFiles() RETAIL(FUN_002a0750);
    u32 SetState(u32 state) RETAIL(FUN_002a7d78);
    void DestroyBase(u32 destroyFlags) RETAIL(FUN_002a7de8);

    // Its vtable's functions
    // The found file's time of change, into the file's date (taken from a clock)
    void FileDate(SaveFile* file) RETAIL(func_002A8D38);
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

    // The same called through the vtable (the base class's code and the save code do)
    u32 PollThrough(u32 request, u32 next, u32 asked)
    {
        return CallVirtual<u32>(this, vtable, 2, request, next, asked);
    }

    u32 RequestThrough(u32 request, u32 next)
    {
        return CallVirtual<u32>(this, vtable, 3, request, next);
    }

    u32 HasCard()
    {
        return CallVirtual<u32>(this, vtable, 7);
    }

    u32 CardFormatted()
    {
        return CallVirtual<u32>(this, vtable, 8);
    }

    u32 HasSave()
    {
        return CallVirtual<u32>(this, vtable, 9);
    }

    u32 SavedBytes()
    {
        return CallVirtual<u32>(this, vtable, 10);
    }

    u32 NeededBytes()
    {
        return CallVirtual<u32>(this, vtable, 12);
    }

    u32 FreeBytes()
    {
        return CallVirtual<u32>(this, vtable, 13);
    }

    u32 Running() const
    {
        return flags >> RunningShift & OperationMask;
    }

    u32 State() const
    {
        return flags >> StateShift & 0xF;
    }
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

    // A save's file: made with its name, its tag, its size and its buffer, and destroyed; icon.sys made with its title and icon's
    // name; a file read from a path of the disc and saved under another name
    SaveFile* ConstructSaveFile(void* file, const char* name, u32 tag, u32 size, void* buffer) RETAIL(FUN_002a8380);
    void DestroySaveFile(void* file, u32 destroyFlags) RETAIL(FUN_002a83f8);
    IconSysFile* ConstructIconSys(void* file, const char* title, const char* icon) RETAIL(FUN_002a2a78);
    void* ConstructCopiedFile(void* file, const char* path, const char* name) RETAIL(FUN_002a7b28);
    // The save's icon files: made with room for so many and icon.sys, given a file, destroyed (the files with them)
    SaveIconFiles* ConstructIconFiles(void* files, u32 room, void* iconSys) RETAIL(FUN_002a8848);
    void AddIconFile(void* files, void* file) RETAIL(FUN_002a8948);
    void DestroyIconFiles(void* files, u32 destroyFlags) RETAIL(FUN_002a8898);
    // The card folder's file (its name, how many save slots, its tag, its size, its buffer) and its destructor (the summaries with
    // it)
    FolderFile* ConstructFolderFile(void* folder, const char* name, u32 files, u32 tag, u32 fileSize, void* buffer)
        RETAIL(FUN_002a8630);
    void DestroyFolderFile(void* folder, u32 destroyFlags) RETAIL(FUN_002a86f0);
    // An icon.sys's header ("PS2D", the reserved fields cleared), and a text made its title (64 bytes cleared, then each character
    // in Shift-JIS)
    void InitIconSys(IconSys* iconSys) RETAIL(FUN_002a9840);
    void IconTitleToSjis(u8* title, const char* text) RETAIL(FUN_002a2920);
}
