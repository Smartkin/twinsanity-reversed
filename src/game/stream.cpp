#include "game/stream.h"
#include "game/memory.h"
#include "game/string.h"
#include "platform/files.h"
#include "platform/memory.h"
#include "retail/libc.h"

namespace
{
constexpr u32 ScratchSize = 0x800;

// A memory stream's values are wherever they are: they're copied as unaligned
template <typename T>
void CopyUnaligned(void* destination, const void* source)
{
    typedef T __attribute__((aligned(1), may_alias)) Unaligned;
    *static_cast<Unaligned*>(destination) = *static_cast<const Unaligned*>(source);
}
}

extern "C"
{
    // What File::Open puts before every path. Nothing ever sets it
    extern String g_FilePathPrefix RETAIL(D_003C6F40);
    // 2 KB of scratch memory, CopyTo's and the decals' loader's
    extern u8 g_ScratchBuffer[ScratchSize] RETAIL(DecalUnusedInt);
    // The module's statics made (g_FilePathPrefix emptied), and the static constructor that runs it
    void InitStreamStatics(s32 initialise, s32 priority) RETAIL(FUN_002b5938);
    void StreamStaticInit() RETAIL(FUN_002b7850);
    // An item builder of the module's nothing makes (its vtable 0x18 bytes before the object builder's list iterator's): its
    // destructor (the base's), and its slot 3 making nothing (its slot 2 is abstract)
    void DestroyStreamItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_002b5a98);
    void* StreamItemBuilderMakesNothing() RETAIL(FUN_002b5ac8);
    // The items' builders' base (BuilderBaseFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
}

EABI_EXPORT(FUN_002b5ee8, &File::WriteF32);
EABI_EXPORT(FUN_002b6378, &MemoryStream::WriteF32);

void Stream::DestroyStream(u32 flags)
{
    vtable = g_StreamVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Stream::CopyTo(Stream* destination)
{
    u32 remaining = Size();
    u32 position = Tell();
    Rewind();
    while (!AtEnd() && remaining != 0)
    {
        u32 chunk = remaining <= ScratchSize ? remaining : ScratchSize;
        Read(g_ScratchBuffer, chunk, 1);
        remaining -= chunk;
        destination->Write(g_ScratchBuffer, chunk);
    }

    Seek(position);
}

File* File::Construct(File* file)
{
    file->descriptor = Closed;
    file->vtable = g_FileVTable;
    return file;
}

void File::Destroy(u32 flags)
{
    vtable = g_FileVTable;
    Close();
    vtable = g_StreamVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

s32 File::Read(void* buffer, u32 size, u32)
{
    return Platform::Files::Read(descriptor, buffer, static_cast<s32>(size));
}

void File::PutBack(const void*, u32)
{
}

s32 File::Write(const void* buffer, u32 size)
{
    return Platform::Files::Write(descriptor, buffer, static_cast<s32>(size));
}

void File::Flush()
{
}

s32 File::Error()
{
    return descriptor < 0 ? descriptor : 0;
}

u32 File::Size()
{
    u32 position = Stream::Tell();
    u32 size = static_cast<u32>(Platform::Files::Seek(descriptor, 0, Platform::Files::SeekEnd));
    Platform::Files::Seek(descriptor, static_cast<s32>(position), Platform::Files::SeekSet);
    return size;
}

u32 File::Tell()
{
    return static_cast<u32>(Platform::Files::Seek(descriptor, 0, Platform::Files::SeekCurrent));
}

bool File::AtEnd()
{
    u32 position = Stream::Tell();
    return position >= Stream::Size();
}

void File::Rewind()
{
    Platform::Files::Seek(descriptor, 0, Platform::Files::SeekSet);
}

void File::SeekToEnd()
{
    Platform::Files::Seek(descriptor, 0, Platform::Files::SeekEnd);
}

void File::Skip(s32 offset)
{
    Platform::Files::Seek(descriptor, offset, Platform::Files::SeekCurrent);
}

void File::Seek(u32 position)
{
    Platform::Files::Seek(descriptor, static_cast<s32>(position), Platform::Files::SeekSet);
}

void File::ReadS8(s8* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadS16(s16* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadS64(s64* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadS32(s32* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadU8(u8* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadU16(u16* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadU64(u64* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadU32(u32* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadBool(bool* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::ReadF32(f32* value)
{
    Stream::Read(value, sizeof(*value), 1);
}

void File::WriteS8(s8 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteS16(s16 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteS64(s64 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteS32(s32 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteU8(u8 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteU16(u16 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteU64(u64 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteU32(u32 value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteBool(bool value)
{
    Stream::Write(&value, sizeof(value));
}

void File::WriteF32(f32 value)
{
    Stream::Write(&value, sizeof(value));
}

// The retail game made the disc's path of the file itself (upper case, backslashes, "cdrom0:\" and ";1"): the platform layer
// does that now
s32 File::Open(const char* path, Mode mode)
{
    s32 flags = 0;
    switch (mode)
    {
    case ModeRead:
        flags = Platform::Files::OpenRead;
        break;
    case ModeCreate:
        flags = Platform::Files::OpenWrite | Platform::Files::OpenCreate | Platform::Files::OpenTruncate;
        break;
    case ModeWrite:
        flags = Platform::Files::OpenWrite | Platform::Files::OpenTruncate;
        break;
    case ModeReadWrite:
        flags = Platform::Files::OpenReadWrite;
        break;
    }

    String fullPath;
    fullPath.string = nullptr;
    fullPath.length = 0;
    fullPath.capacity = 0;
    StringAssign(&fullPath, g_FilePathPrefix.string);
    StringAppend(&fullPath, path);
    descriptor = Platform::Files::Open(fullPath.string, flags);
    StringDestroy(&fullPath);
    return descriptor;
}

void File::Close()
{
    if (descriptor >= 0)
    {
        Platform::Files::Close(descriptor);
    }

    descriptor = Closed;
}

s32 File::ReadChecked(void* buffer, u32 size, bool endIsError)
{
    s32 read = Platform::Files::Read(descriptor, buffer, static_cast<s32>(size));
    if (static_cast<u32>(read) < size && endIsError && read == 0)
    {
        return ReadNothing;
    }

    return read;
}

MemoryStream* MemoryStream::Construct(MemoryStream* stream, void* memory, u32 size, u32 ownsMemory, u16 alignment)
{
    stream->size = size;
    stream->position = static_cast<u8*>(memory);
    stream->begin = static_cast<u8*>(memory);
    stream->vtable = g_MemoryStreamVTable;
    MemoryStreamFlags flags{};
    flags.ownsMemory = ownsMemory;
    stream->flags = flags;
    stream->alignment = alignment;
    return stream;
}

MemoryStream* MemoryStream::ConstructAllocated(MemoryStream* stream, u32 size, u32 grows, u16 alignment)
{
    stream->vtable = g_MemoryStreamVTable;
    stream->begin = nullptr;
    stream->position = nullptr;
    stream->size = 0;
    MemoryStreamFlags flags{};
    flags.ownsMemory = 1;
    flags.grows = grows;
    stream->flags = flags;
    stream->alignment = alignment;
    u8* memory = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), size, stream->alignment));
    stream->begin = memory;
    stream->flags.ownsMemory = 1;
    stream->position = memory;
    stream->size = memory != nullptr ? size : 0;
    return stream;
}

MemoryStream* MemoryStream::ConstructFromFile(MemoryStream* stream, const char* path, bool terminate)
{
    stream->begin = nullptr;
    stream->position = nullptr;
    stream->size = 0;
    stream->vtable = g_MemoryStreamVTable;
    MemoryStreamFlags flags{};
    flags.ownsMemory = 1;
    stream->flags = flags;
    stream->alignment = FileAlignment;
    stream->LoadFile(path, terminate);
    return stream;
}

void MemoryStream::Destroy(u32 destroyFlags)
{
    vtable = g_MemoryStreamVTable;
    if (flags.ownsMemory)
    {
        FreeMemory(GetHeapManager(), begin);
        begin = nullptr;
        position = nullptr;
        size = 0;
    }

    vtable = g_StreamVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

s32 MemoryStream::Read(void* buffer, u32 count, u32)
{
    RetailLibc::MemoryCopy(buffer, position, count);
    position += count;
    return static_cast<s32>(count);
}

void MemoryStream::PutBack(const void*, u32 count)
{
    position -= count;
}

// A stream that grows doubles its size until the data fits (a stream of no size never does), keeping what's before the
// position. One that doesn't writes what fits
s32 MemoryStream::Write(const void* buffer, u32 count)
{
    u32 at = Stream::Tell();
    u32 room = size - at;
    if (room < count)
    {
        if (flags.grows)
        {
            u32 newSize = size * 2;
            while (newSize - at < count)
            {
                newSize *= 2;
            }

            u8* memory = static_cast<u8*>(MemoryAllocateAligned(GetHeapManager(), newSize, alignment));
            if (at != 0)
            {
                RetailLibc::MemoryCopy(memory, begin, at);
            }

            FreeMemory(GetHeapManager(), begin);
            size = newSize;
            position = memory + at;
            begin = memory;
        }
        else
        {
            count = room;
        }
    }

    RetailLibc::MemoryCopy(position, buffer, count);
    position += count;
    if (size < at + count)
    {
        size = at + count;
    }

    return static_cast<s32>(count);
}

void MemoryStream::Flush()
{
    Platform::Memory::Synchronise();
}

s32 MemoryStream::Error()
{
    return 0;
}

u32 MemoryStream::Size()
{
    return size;
}

u32 MemoryStream::Tell()
{
    return static_cast<u32>(position - begin);
}

bool MemoryStream::AtEnd()
{
    return Stream::Tell() >= size;
}

void MemoryStream::Rewind()
{
    position = begin;
}

void MemoryStream::SeekToEnd()
{
    position = begin + size;
}

void MemoryStream::Skip(s32 offset)
{
    position += offset;
}

void MemoryStream::Seek(u32 offset)
{
    position = begin + offset;
}

void MemoryStream::ReadS8(s8* value)
{
    *value = static_cast<s8>(*position);
    position += sizeof(*value);
}

void MemoryStream::ReadS16(s16* value)
{
    CopyUnaligned<s16>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadS64(s64* value)
{
    CopyUnaligned<s64>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadS32(s32* value)
{
    CopyUnaligned<s32>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadU8(u8* value)
{
    *value = *position;
    position += sizeof(*value);
}

void MemoryStream::ReadU16(u16* value)
{
    CopyUnaligned<u16>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadU64(u64* value)
{
    CopyUnaligned<u64>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadU32(u32* value)
{
    CopyUnaligned<u32>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadBool(bool* value)
{
    CopyUnaligned<u8>(value, position);
    position += sizeof(*value);
}

void MemoryStream::ReadF32(f32* value)
{
    CopyUnaligned<u32>(value, position);
    position += sizeof(*value);
}

void MemoryStream::WriteS8(s8 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteS16(s16 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteS64(s64 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteS32(s32 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteU8(u8 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteU16(u16 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteU64(u64 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteU32(u32 value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteBool(bool value)
{
    Stream::Write(&value, sizeof(value));
}

void MemoryStream::WriteF32(f32 value)
{
    Stream::Write(&value, sizeof(value));
}

void InitStreamStatics(s32 initialise, s32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_FilePathPrefix.string = nullptr;
    g_FilePathPrefix.capacity = 0;
    g_FilePathPrefix.length = 0;
}

void StreamStaticInit()
{
    InitStreamStatics(1, DefaultInitPriority);
}

void DestroyStreamItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void* StreamItemBuilderMakesNothing()
{
    return nullptr;
}
