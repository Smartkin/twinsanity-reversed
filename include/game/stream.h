#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"

// The game's streams: files (File) and memory (MemoryStream) behind one interface of 33 virtual functions, whose vtables are the
// retail ones. Calls through a Stream go through the vtable; a derived class's methods of the same names are its versions of
// them (the vtables point at them), called directly
class Stream
{
public:
    const GccVTableEntry* vtable;

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, 1, flags);
    }

    // Returns how much it read. The last argument is 1 at every call
    s32 Read(void* buffer, u32 size, u32 unknown)
    {
        return CallVirtual<s32>(this, vtable, 2, buffer, size, unknown);
    }

    // Goes back over what was just read
    void PutBack(const void* buffer, u32 size)
    {
        CallVirtual<void>(this, vtable, 3, buffer, size);
    }

    s32 Write(const void* buffer, u32 size)
    {
        return CallVirtual<s32>(this, vtable, 4, buffer, size);
    }

    void Flush()
    {
        CallVirtual<void>(this, vtable, 5);
    }

    // 0 while it's usable
    s32 Error()
    {
        return CallVirtual<s32>(this, vtable, 6);
    }

    u32 Size()
    {
        return CallVirtual<u32>(this, vtable, 7);
    }

    u32 Tell()
    {
        return CallVirtual<u32>(this, vtable, 8);
    }

    bool AtEnd()
    {
        return CallVirtual<bool>(this, vtable, 9);
    }

    void Rewind()
    {
        CallVirtual<void>(this, vtable, 10);
    }

    void SeekToEnd()
    {
        CallVirtual<void>(this, vtable, 11);
    }

    void Skip(s32 offset)
    {
        CallVirtual<void>(this, vtable, 12, offset);
    }

    void Seek(u32 position)
    {
        CallVirtual<void>(this, vtable, 13, position);
    }

    // The values, in the vtable's order (GCC 2.9x's long is 64 bits)
    void ReadS8(s8* value)
    {
        CallVirtual<void>(this, vtable, 14, value);
    }

    void ReadS16(s16* value)
    {
        CallVirtual<void>(this, vtable, 15, value);
    }

    void ReadS64(s64* value)
    {
        CallVirtual<void>(this, vtable, 16, value);
    }

    void ReadS32(s32* value)
    {
        CallVirtual<void>(this, vtable, 17, value);
    }

    void ReadU8(u8* value)
    {
        CallVirtual<void>(this, vtable, 18, value);
    }

    void ReadU16(u16* value)
    {
        CallVirtual<void>(this, vtable, 19, value);
    }

    void ReadU64(u64* value)
    {
        CallVirtual<void>(this, vtable, 20, value);
    }

    void ReadU32(u32* value)
    {
        CallVirtual<void>(this, vtable, 21, value);
    }

    void ReadBool(bool* value)
    {
        CallVirtual<void>(this, vtable, 22, value);
    }

    void ReadF32(f32* value)
    {
        CallVirtual<void>(this, vtable, 23, value);
    }

    void WriteS8(s8 value)
    {
        CallVirtual<void>(this, vtable, 24, value);
    }

    void WriteS16(s16 value)
    {
        CallVirtual<void>(this, vtable, 25, value);
    }

    void WriteS64(s64 value)
    {
        CallVirtual<void>(this, vtable, 26, value);
    }

    void WriteS32(s32 value)
    {
        CallVirtual<void>(this, vtable, 27, value);
    }

    void WriteU8(u8 value)
    {
        CallVirtual<void>(this, vtable, 28, value);
    }

    void WriteU16(u16 value)
    {
        CallVirtual<void>(this, vtable, 29, value);
    }

    void WriteU64(u64 value)
    {
        CallVirtual<void>(this, vtable, 30, value);
    }

    void WriteU32(u32 value)
    {
        CallVirtual<void>(this, vtable, 31, value);
    }

    void WriteBool(bool value)
    {
        CallVirtual<void>(this, vtable, 32, value);
    }

    void WriteF32(f32 value)
    {
        CallVirtual<void>(this, vtable, 33, value);
    }

    // The abstract stream's destructor, its vtable's only function
    void DestroyStream(u32 flags) RETAIL(FUN_002b5ae0);

    // Copies the whole stream into the other one, 2 KB at a time, and goes back to where it was
    void CopyTo(Stream* destination) RETAIL(FUN_002b4668);
};
CHECK_SIZE(Stream, 4);

// A file of the game's disc, by the platform layer's descriptor. Destroying it closes it
class File : public Stream
{
public:
    enum Mode : s32
    {
        ModeCreate = 0,
        ModeRead = 1,
        ModeWrite = 2,
        ModeReadWrite = 3,
    };

    // Negative while it's closed
    s32 descriptor;

    static File* Construct(File* file) RETAIL(FUN_002b6ab8);

    void Destroy(u32 flags) RETAIL(FUN_002b6ae0);
    s32 Read(void* buffer, u32 size, u32 unknown) RETAIL(FUN_002b6b78);
    void PutBack(const void* buffer, u32 size) RETAIL(FUN_002b6c38);
    s32 Write(const void* buffer, u32 size) RETAIL(FUN_002b6c10);
    void Flush() RETAIL(FUN_002b6c40);
    s32 Error() RETAIL(FUN_002b6dd0);
    u32 Size() RETAIL(FUN_002b6c48);
    u32 Tell() RETAIL(FUN_002b6cb8);
    bool AtEnd() RETAIL(FUN_002b6ce0);
    void Rewind() RETAIL(FUN_002b6d40);
    void SeekToEnd() RETAIL(FUN_002b6d68);
    void Skip(s32 offset) RETAIL(FUN_002b6d90);
    void Seek(u32 position) RETAIL(FUN_002b6db0);
    void ReadS8(s8* value) RETAIL(FUN_002b5b10);
    void ReadS16(s16* value) RETAIL(FUN_002b5b40);
    void ReadS64(s64* value) RETAIL(FUN_002b5ba0);
    void ReadS32(s32* value) RETAIL(FUN_002b5b70);
    void ReadU8(u8* value) RETAIL(FUN_002b5bd0);
    void ReadU16(u16* value) RETAIL(FUN_002b5c00);
    void ReadU64(u64* value) RETAIL(FUN_002b5c60);
    void ReadU32(u32* value) RETAIL(FUN_002b5c30);
    void ReadBool(bool* value) RETAIL(FUN_002b5c90);
    void ReadF32(f32* value) RETAIL(FUN_002b5cc0);
    void WriteS8(s8 value) RETAIL(FUN_002b5cf0);
    void WriteS16(s16 value) RETAIL(FUN_002b5d28);
    void WriteS64(s64 value) RETAIL(FUN_002b5d98);
    void WriteS32(s32 value) RETAIL(FUN_002b5d60);
    void WriteU8(u8 value) RETAIL(FUN_002b5dd0);
    void WriteU16(u16 value) RETAIL(FUN_002b5e08);
    void WriteU64(u64 value) RETAIL(FUN_002b5e78);
    void WriteU32(u32 value) RETAIL(FUN_002b5e40);
    void WriteBool(bool value) RETAIL(FUN_002b5eb0);
    void WriteF32(f32 value) RETAIL_N32(FUN_002b5ee8);

    // Its own virtual functions (slots 34 to 36). Open takes a path on the disc and returns the descriptor
    s32 Open(const char* path, Mode mode) RETAIL(FUN_002b4ab0);
    void Close() RETAIL(FUN_002b6b38);
    // Read, but returns -1 when it read nothing of what it was asked for and endIsError
    s32 ReadChecked(void* buffer, u32 size, bool endIsError) RETAIL(FUN_002b6ba0);
};
CHECK_SIZE(File, 8);

// A stream over memory, which it may own (freeing it when it's destroyed) and grow when written past its end
class MemoryStream : public Stream
{
public:
    enum Flags : u16
    {
        FlagOwnsMemory = 0x1,
        FlagGrows = 0x2,
    };

    // What its memory is allocated at
    u16 alignment;
    u16 flags;
    u8* begin;
    u8* position;
    u32 size;

    // The flags' arguments are bits (the retail code takes bit 0)
    static MemoryStream* Construct(MemoryStream* stream, void* memory, u32 size, u32 ownsMemory, u16 alignment)
        RETAIL(CreateBinReader);
    // With memory of its own
    static MemoryStream* ConstructAllocated(MemoryStream* stream, u32 size, u32 grows, u16 alignment) RETAIL(FUN_002b6468);
    // With a file of the archive in memory of its own, a terminator after it when asked for
    static MemoryStream* ConstructFromFile(MemoryStream* stream, const char* path, bool terminate) RETAIL(FUN_002b63d8);
    bool LoadFile(const char* path, bool terminate) RETAIL(FUN_002b4538);

    void Destroy(u32 flags) RETAIL(DeleteBinaryReader);
    s32 Read(void* buffer, u32 size, u32 unknown) RETAIL(ReadBytesIntoMemory);
    void PutBack(const void* buffer, u32 size) RETAIL(MoveCurPositionBackwards);
    s32 Write(const void* buffer, u32 size) RETAIL(WriteBlock);
    void Flush() RETAIL(SyncMemory);
    s32 Error() RETAIL(BinReader_Get0);
    u32 Size() RETAIL(GetBlockSize);
    u32 Tell() RETAIL(GetReadAmount);
    bool AtEnd() RETAIL(GetEOF);
    void Rewind() RETAIL(ReturnToBeg);
    void SeekToEnd() RETAIL(SetEOF);
    void Skip(s32 offset) RETAIL(MovePositionBy);
    void Seek(u32 position) RETAIL(SetPosition);
    void ReadS8(s8* value) RETAIL(ReadByte);
    void ReadS16(s16* value) RETAIL(ReadShort);
    void ReadS64(s64* value) RETAIL(ReadLong);
    void ReadS32(s32* value) RETAIL(ReadInt);
    void ReadU8(u8* value) RETAIL(ReadChar_);
    void ReadU16(u16* value) RETAIL(ReadUShort_);
    void ReadU64(u64* value) RETAIL(ReadULong_);
    void ReadU32(u32* value) RETAIL(ReadUInt_);
    void ReadBool(bool* value) RETAIL(ReadAnotherOneByteSizedThing);
    void ReadF32(f32* value) RETAIL(ReadFloat);
    void WriteS8(s8 value) RETAIL(FUN_002b6180);
    void WriteS16(s16 value) RETAIL(FUN_002b61b8);
    void WriteS64(s64 value) RETAIL(FUN_002b6228);
    void WriteS32(s32 value) RETAIL(WriteInt);
    void WriteU8(u8 value) RETAIL(FUN_002b6260);
    void WriteU16(u16 value) RETAIL(FUN_002b6298);
    void WriteU64(u64 value) RETAIL(FUN_002b6308);
    void WriteU32(u32 value) RETAIL(FUN_002b62d0);
    void WriteBool(bool value) RETAIL(FUN_002b6340);
    void WriteF32(f32 value) RETAIL_N32(FUN_002b6378);
};
CHECK_SIZE(MemoryStream, 0x14);
CHECK_OFFSET(MemoryStream, flags, 6);

extern "C"
{
    extern const GccVTableEntry g_StreamVTable[] RETAIL(D_00307148);
    extern const GccVTableEntry g_FileVTable[] RETAIL(D_00307018);
    extern const GccVTableEntry g_MemoryStreamVTable[] RETAIL(BinaryReader_Methods);
}
