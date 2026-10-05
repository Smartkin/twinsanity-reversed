#include "game/savedevice.h"

#include "game/colour.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/renderer.h"
#include "game/stream.h"
#include "retail/libc.h"

// The files of a save: the base file (a stream over its buffer, with the date, the tag and the checksum), the icon copied from the
// disc, the card folder's file of the save slots' summaries and icon.sys, and the save's list of icon files

namespace
{
// A Shift-JIS code of a character, and the ASCII character it's of
struct SjisRange
{
    u16 sjis;
    u16 ascii;
};
}

extern "C"
{
    extern const GccVTableEntry g_FolderSummaryVTable[] RETAIL(D_003066F0);
    extern const GccVTableEntry g_SaveFileVTable[] RETAIL(D_00306698);
    extern const GccVTableEntry g_CopiedFileVTable[] RETAIL(D_00306418);
    extern const GccVTableEntry g_FolderFileVTable[] RETAIL(D_00306640);
    extern const GccVTableEntry g_IconSysFileVTable[] RETAIL(D_00306348);
    // A summary's text: " - ", ":0", ":", " " and "/"
    extern const char g_SummaryDash[] RETAIL(D_0030A2E8);
    extern const char g_SummaryColonZero[] RETAIL(D_0030A2F0);
    extern const char g_SummaryColon[] RETAIL(D_0030A2F8);
    extern const char g_SummarySpace[] RETAIL(D_0030A300);
    extern const char g_SummarySlash[] RETAIL(D_0030A308);
    // "icon.sys" and its magic "PS2D"
    extern const char g_IconSysName[] RETAIL(D_003061E8);
    extern const char g_IconSysMagic[] RETAIL(D_0030A390);
    // The Shift-JIS of the digits, the capitals and the small letters (by their first), and of the symbols from the space on (the
    // space to "/", ":" to "@", "[" to "`" and "{" to "~")
    extern const SjisRange g_SjisRanges[] RETAIL(D_003067F0);
    extern const u16 g_SjisSymbols[] RETAIL(D_00306800);
}

namespace
{
// A file with a tag ends with the tag and the checksum, which leave out the file's last 4 bytes
constexpr u32 TrailerSize = 8;
constexpr u32 UnsummedBytes = 4;
constexpr u8 FillByte = 0xA5;
// The title's Shift-JIS bytes cleared, and the bytes of a character
constexpr u32 IconTitleBytes = 0x40;
constexpr u32 SjisCharacterBytes = 2;
// The colours of icon.sys's background corners, ambient light and lights
constexpr ColourIndex CornerColours[] = {ColourRed, ColourMagenta, ColourYellow, ColourCyan};
constexpr ColourIndex AmbientColour = ColourGrey;
constexpr ColourIndex LightColours[] = {ColourRed, ColourGreen, ColourBlue};
// Each light along an axis
constexpr Vector4 LightDirections[] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
constexpr f32 IconOpacity = 0.5f;
// A symbol's index in g_SjisSymbols is its character less its group's offset and SymbolBase. The groups follow each other in the
// table (16 from the space, 7 from ":", 6 from "[", 4 from "{"), the base keeping the first group's offset above 0 (no symbol)
constexpr u32 SymbolBase = 0x1F;

constexpr u32 SymbolGroup(char first, u32 index)
{
    return static_cast<u32>(first) - index - SymbolBase;
}

constexpr u32 SpaceSymbols = SymbolGroup(' ', 0);
constexpr u32 ColonSymbols = SymbolGroup(':', 16);
constexpr u32 BracketSymbols = SymbolGroup('[', 23);
constexpr u32 BraceSymbols = SymbolGroup('{', 29);
// The ranges: digits, capitals, small letters. A character outside them and the symbols (below the space, past "~") is converted
// in the last range seen. Retail bug: before any, the range is 0x30, past the table (whatever follows it gives the code)
constexpr u32 DigitRange = 0;
constexpr u32 CapitalRange = 1;
constexpr u32 SmallRange = 2;
constexpr u32 UnsetRange = 0x30;

bool InRange(s32 character, char first, char last)
{
    return static_cast<u32>(character - first) <= static_cast<u32>(last - first);
}

void DropStream(SaveFile* file)
{
    Stream* stream = file->stream;
    if (stream != nullptr)
    {
        stream->Destroy(DestroyAndFree);
    }
}

// The base's members made (its vtable the base's)
void ConstructBase(SaveFile* file, const char* name)
{
    file->vtable = g_SaveFileVTable;
    StringConstruct(&file->name, name);
    ConstructSaveDate(&file->date);
}

void DestroyBase(SaveFile* file, u32 destroyFlags)
{
    file->vtable = g_SaveFileVTable;
    DropStream(file);
    StringDestroy(&file->name);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(file);
    }
}

// The checksum of a file's bytes but its last 4. Retail bug: every step adds its first byte, never the next ones, so the checksum
// is of the size and the first byte (the day of the date)
u32 Checksum(const u8* bytes, u32 size)
{
    u32 sum = size;
    u32 addShift = 24;
    u32 xorShift = 24;
    for (u32 i = 0; i < size - UnsummedBytes; i++)
    {
        u32 byte = bytes[0];
        sum += byte << (addShift & ShiftMask);
        sum ^= byte << (xorShift & ShiftMask);
        if (addShift >= 6)
        {
            addShift += 24;
        }

        if (xorShift >= 8)
        {
            xorShift += 24;
        }

        addShift -= 5;
        xorShift -= 7;
    }

    return sum;
}

void AppendNumber(String* text, u32 value)
{
    String number;
    StringConstructNumber(&number, value);
    StringAppend(text, number.string);
    StringDestroy(&number);
}

MemoryStream* NewStream()
{
    return static_cast<MemoryStream*>(MemoryAllocate(sizeof(MemoryStream)));
}
}

FolderSummary* FolderSummary::Construct(FolderSummary* summary)
{
    summary->vtable = g_FolderSummaryVTable;
    ConstructSaveDate(&summary->date);
    summary->name = {nullptr, 0, 0};
    return summary;
}

void FolderSummary::Destroy(u32 destroyFlags)
{
    vtable = g_FolderSummaryVTable;
    StringDestroy(&name);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void FolderSummary::Describe(String* text)
{
    u8 minute = date.minute;
    u8 second = date.second;
    StringAssign(text, name.string);
    StringAppend(text, g_SummaryDash);
    AppendNumber(text, date.hour);
    StringAppend(text, minute < 10 ? g_SummaryColonZero : g_SummaryColon);
    AppendNumber(text, minute);
    StringAppend(text, second < 10 ? g_SummaryColonZero : g_SummaryColon);
    AppendNumber(text, second);
    StringAppend(text, g_SummarySpace);
    AppendNumber(text, date.day);
    StringAppend(text, g_SummarySlash);
    AppendNumber(text, date.month);
    StringAppend(text, g_SummarySlash);
    AppendNumber(text, date.year);
}

void FolderSummary::Clear()
{
    RetailLibc::MemorySet(&date, 0, sizeof(SaveDate));
}

void FolderSummary::Refresh()
{
    if (HasSave())
    {
        GetSaveDate(&date);
    }
}

void FolderSummary::Read(Stream* stream)
{
    stream->Read(&date, sizeof(SaveDate), 1);
    StringRead(&name, stream);
}

void FolderSummary::Write(Stream* stream)
{
    stream->Write(&date, sizeof(SaveDate));
    StringWrite(&name, stream);
}

SaveFile* SaveFile::Construct(SaveFile* file, const char* name)
{
    ConstructBase(file, name);
    file->tag = 0;
    file->checksum = 0;
    file->stream = nullptr;
    file->size = 0;
    file->buffer = nullptr;
    return file;
}

MemoryStream* SaveFile::MakeStream()
{
    MemoryStream* made;
    u8* memory;
    if (buffer == nullptr)
    {
        MemoryStream* allocated = NewStream();
        made = MemoryStream::ConstructAllocated(allocated, size, 0, MemoryStream::FileAlignment);
        memory = made->begin;
    }
    else
    {
        MemoryStream* allocated = NewStream();
        made = MemoryStream::Construct(allocated, buffer, size, 0, MemoryStream::FileAlignment);
        memory = buffer;
    }

    RetailLibc::MemorySet(memory, FillByte, size);
    return made;
}

u32 SaveFile::BeginRead()
{
    DropStream(this);
    stream = CallVirtual<MemoryStream*>(this, vtable, MakeStreamSlot);
    return stream != nullptr;
}

u32 SaveFile::EndRead()
{
    u32 good = 1;
    if (tag != 0)
    {
        static_cast<Stream*>(stream)->Seek(size - TrailerSize);
        s32 storedTag;
        static_cast<Stream*>(stream)->ReadS32(&storedTag);
        static_cast<Stream*>(stream)->ReadS32(reinterpret_cast<s32*>(&checksum));
        good = 0;
        if (static_cast<u32>(storedTag) == tag)
        {
            good = checksum == 0 || Checksum(stream->begin, size) == checksum;
        }

        if (good != 0)
        {
            static_cast<Stream*>(stream)->Rewind();
            static_cast<Stream*>(stream)->Read(&date, sizeof(SaveDate), 1);
            CallVirtual<void>(this, vtable, ReadSlot, static_cast<Stream*>(stream));
        }
    }

    DropStream(this);
    stream = nullptr;
    return good;
}

u32 SaveFile::BeginWrite()
{
    DropStream(this);
    stream = CallVirtual<MemoryStream*>(this, vtable, MakeStreamSlot);
    CallVirtual<void>(this, vtable, GatherSlot);
    GetSaveDate(&date);
    if (tag != 0)
    {
        static_cast<Stream*>(stream)->Write(&date, sizeof(SaveDate));
        CallVirtual<void>(this, vtable, WriteSlot, static_cast<Stream*>(stream));
        checksum = Checksum(stream->begin, size);
        static_cast<Stream*>(stream)->Seek(size - TrailerSize);
        static_cast<Stream*>(stream)->WriteS32(static_cast<s32>(tag));
        static_cast<Stream*>(stream)->WriteS32(static_cast<s32>(checksum));
    }
    else
    {
        CallVirtual<void>(this, vtable, WriteSlot, static_cast<Stream*>(stream));
    }

    return stream != nullptr;
}

u32 SaveFile::EndWrite()
{
    DropStream(this);
    stream = nullptr;
    return 1;
}

void CopiedFile::Gather()
{
}

void CopiedFile::Read(Stream*)
{
}

void CopiedFile::Write(Stream*)
{
}

void CopiedFile::Destroy(u32 destroyFlags)
{
    DestroyBase(this, destroyFlags);
}

u32 CopiedFile::BeginRead()
{
    return 0;
}

u32 CopiedFile::EndRead()
{
    return 0;
}

u32 CopiedFile::BeginWrite()
{
    return 1;
}

u32 CopiedFile::EndWrite()
{
    return 1;
}

void FolderFile::Gather()
{
}

void FolderFile::Read(Stream* from)
{
    for (u32 i = 0; i < count; i++)
    {
        FolderSummary* summary = summaries[i];
        CallVirtual<void>(summary, summary->vtable, FolderSummary::ReadSlot, from);
    }
}

void FolderFile::Write(Stream* to)
{
    for (u32 i = 0; i < count; i++)
    {
        FolderSummary* summary = summaries[i];
        CallVirtual<void>(summary, summary->vtable, FolderSummary::WriteSlot, to);
    }
}

void FolderFile::ClearSummaries()
{
    for (u32 i = 0; i < count; i++)
    {
        FolderSummary* summary = summaries[i];
        CallVirtual<void>(summary, summary->vtable, FolderSummary::ClearSlot);
    }
}

MemoryStream* IconSysFile::MakeStream()
{
    return MemoryStream::Construct(NewStream(), &iconSys, sizeof(IconSys), 0, MemoryStream::FileAlignment);
}

void IconSysFile::Gather()
{
}

void IconSysFile::Read(Stream*)
{
}

void IconSysFile::Write(Stream* to)
{
    to->Write(&iconSys, sizeof(IconSys));
}

void IconSysFile::Destroy(u32 destroyFlags)
{
    StringDestroy(&title);
    DestroyBase(this, destroyFlags);
}

void IconSysFile::SetTitle(const char* text)
{
    u32 length = RetailLibc::StringLength(text);
    StringAssign(&title, text);
    iconSys.lineBreak = static_cast<u16>(length * SjisCharacterBytes);
    for (u32 i = 0; i < length; i++)
    {
        if (title.string[i] == '~')
        {
            title.string[i] = ' ';
            iconSys.lineBreak = static_cast<u16>(i * SjisCharacterBytes);
        }
    }

    IconTitleToSjis(iconSys.title, title.string);
}

void IconSysFile::SetViewIcon(const char* icon)
{
    RetailLibc::StringCopy(iconSys.viewIcon, icon);
}

void IconSysFile::SetCopyIcon(const char* icon)
{
    RetailLibc::StringCopy(iconSys.copyIcon, icon);
}

void IconSysFile::SetDeleteIcon(const char* icon)
{
    RetailLibc::StringCopy(iconSys.deleteIcon, icon);
}

void IconSysFile::SetTransparency(f32 opacity)
{
    iconSys.transparency = static_cast<s32>(opacity * 255.0f);
}

EABI_EXPORT(FUN_002a8b78, &IconSysFile::SetTransparency);

void IconSysFile::SetBackground(s32 corner, u32 colour)
{
    Rgba rgba = {colour};
    s32* to = iconSys.background[corner];
    to[0] = rgba.red;
    to[1] = rgba.green;
    to[2] = rgba.blue;
    to[3] = rgba.alpha;
}

void IconSysFile::SetAmbient(u32 colour)
{
    Rgba rgba = {colour};
    iconSys.ambient[0] = Colour::ColourFraction(rgba.red);
    iconSys.ambient[2] = Colour::ColourFraction(rgba.blue);
    iconSys.ambient[3] = Colour::AlphaFraction(rgba.alpha);
    iconSys.ambient[1] = Colour::ColourFraction(rgba.green);
}

void IconSysFile::SetLightColour(s32 light, u32 colour)
{
    Rgba rgba = {colour};
    f32* to = iconSys.lightColours[light];
    to[0] = Colour::ColourFraction(rgba.red);
    to[1] = Colour::ColourFraction(rgba.green);
    to[2] = Colour::ColourFraction(rgba.blue);
    to[3] = Colour::AlphaFraction(rgba.alpha);
}

extern "C"
{
    SaveFile* ConstructSaveFile(void* memory, const char* name, u32 tag, u32 size, void* buffer)
    {
        auto* file = static_cast<SaveFile*>(memory);
        ConstructBase(file, name);
        file->tag = tag;
        file->size = size;
        file->buffer = static_cast<u8*>(buffer);
        file->checksum = 0;
        file->stream = nullptr;
        return file;
    }

    void DestroySaveFile(void* file, u32 destroyFlags)
    {
        DestroyBase(static_cast<SaveFile*>(file), destroyFlags);
    }

    void* ConstructCopiedFile(void* memory, const char* path, const char* name)
    {
        auto* file = static_cast<CopiedFile*>(memory);
        SaveFile::Construct(file, name);
        file->vtable = g_CopiedFileVTable;
        MemoryStream* read = MemoryStream::ConstructFromFile(NewStream(), path, false);
        file->stream = read;
        file->size = static_cast<Stream*>(read)->Size();
        return file;
    }

    IconSysFile* ConstructIconSys(void* memory, const char* title, const char* icon)
    {
        auto* file = static_cast<IconSysFile*>(memory);
        ConstructBase(file, g_IconSysName);
        file->size = sizeof(IconSys);
        file->vtable = g_IconSysFileVTable;
        file->tag = 0;
        file->checksum = 0;
        file->stream = nullptr;
        file->buffer = nullptr;
        file->title = {nullptr, 0, 0};
        InitIconSys(&file->iconSys);
        file->SetTitle(title);
        file->SetViewIcon(icon);
        file->SetCopyIcon(icon);
        file->SetDeleteIcon(icon);
        file->SetTransparency(IconOpacity);
        for (s32 corner = 0; corner < 4; corner++)
        {
            u32 colour;
            GetColor(&colour, CornerColours[corner]);
            file->SetBackground(corner, colour);
        }

        u32 ambient;
        GetColor(&ambient, AmbientColour);
        file->SetAmbient(ambient);
        for (s32 light = 0; light < 3; light++)
        {
            u32 colour;
            GetColor(&colour, LightColours[light]);
            file->SetLightColour(light, colour);
            Vector4 direction = LightDirections[light];
            SetIconLightDirection(reinterpret_cast<u8*>(file), light, &direction);
        }

        return file;
    }

    SaveIconFiles* ConstructIconFiles(void* memory, u32 room, void* iconSys)
    {
        auto* files = static_cast<SaveIconFiles*>(memory);
        files->iconSys = static_cast<SaveFile*>(iconSys);
        // The count and the capacity
        RetailLibc::MemorySet(files, 0, offsetof(SaveIconFiles, iconSys));
        files->capacity = static_cast<u8>(room);
        files->files = static_cast<SaveFile**>(MemoryAllocate2(room * sizeof(SaveFile*)));
        return files;
    }

    void AddIconFile(void* memory, void* file)
    {
        auto* files = static_cast<SaveIconFiles*>(memory);
        u8 index = files->count;
        files->count = index + 1;
        files->files[index] = static_cast<SaveFile*>(file);
    }

    void DestroyIconFiles(void* memory, u32 destroyFlags)
    {
        auto* files = static_cast<SaveIconFiles*>(memory);
        for (u32 i = 0; i < files->count; i++)
        {
            SaveFile* file = files->files[i];
            if (file != nullptr)
            {
                CallVirtual<void>(file, file->vtable, SaveFile::DestroySlot, u32{DestroyAndFree});
            }
        }

        if (files->files != nullptr)
        {
            MemoryDeallocate_(files->files);
        }

        if ((destroyFlags & FreeAfterDestroy) != 0)
        {
            MemoryDeallocate2_(files);
        }
    }

    __attribute__((optimize("no-tree-loop-distribute-patterns"))) FolderFile* ConstructFolderFile(void* memory, const char* name,
                                                                                                   u32 files, u32 tag,
                                                                                                   u32 fileSize, void* buffer)
    {
        auto* folder = static_cast<FolderFile*>(memory);
        ConstructBase(folder, name);
        folder->tag = tag;
        folder->size = fileSize;
        folder->buffer = static_cast<u8*>(buffer);
        folder->checksum = 0;
        folder->stream = nullptr;
        folder->count = files;
        folder->vtable = g_FolderFileVTable;
        folder->summaries = static_cast<FolderSummary**>(MemoryAllocate2(files * sizeof(FolderSummary*)));
        for (u32 i = 0; i < files; i++)
        {
            folder->summaries[i] = nullptr;
        }

        return folder;
    }

    void DestroyFolderFile(void* memory, u32 destroyFlags)
    {
        auto* folder = static_cast<FolderFile*>(memory);
        folder->vtable = g_FolderFileVTable;
        for (u32 i = 0; i < folder->count; i++)
        {
            FolderSummary* summary = folder->summaries[i];
            if (summary != nullptr)
            {
                CallVirtual<void>(summary, summary->vtable, FolderSummary::DestroySlot, u32{DestroyAndFree});
            }
        }

        if (folder->summaries != nullptr)
        {
            MemoryDeallocate_(folder->summaries);
        }

        DestroyBase(folder, destroyFlags);
    }

    void InitIconSys(IconSys* iconSys)
    {
        char magic[5];
        __builtin_memcpy(magic, g_IconSysMagic, sizeof(magic));
        RetailLibc::StringCopy(iconSys->magic, magic);
        iconSys->reserved04 = 0;
        iconSys->reserved08 = 0;
        RetailLibc::MemorySet(iconSys->reserved1C4, 0, sizeof(iconSys->reserved1C4));
    }

    __attribute__((optimize("no-tree-loop-distribute-patterns"))) void IconTitleToSjis(u8* title, const char* text)
    {
        for (u32 i = 0; i < IconTitleBytes; i++)
        {
            title[i] = 0;
        }

        u32 range = UnsetRange;
        while (*text != '\0')
        {
            s32 character = *text++;
            u32 symbols = 0;
            if (InRange(character, ' ', '/'))
            {
                symbols = SpaceSymbols;
            }
            else if (InRange(character, '0', '9'))
            {
                range = DigitRange;
            }
            else if (InRange(character, ':', '@'))
            {
                symbols = ColonSymbols;
            }
            else if (InRange(character, 'A', 'Z'))
            {
                range = CapitalRange;
            }
            else if (InRange(character, '[', '`'))
            {
                symbols = BracketSymbols;
            }
            else if (InRange(character, 'a', 'z'))
            {
                range = SmallRange;
            }
            else
            {
                symbols = InRange(character, '{', '~') ? BraceSymbols : 0;
            }

            u32 code;
            if (symbols != 0)
            {
                code = g_SjisSymbols[character - static_cast<s32>(symbols + SymbolBase)];
            }
            else
            {
                code = g_SjisRanges[range].sjis + character - g_SjisRanges[range].ascii;
            }

            *title++ = static_cast<u8>(code >> 8);
            *title++ = static_cast<u8>(code);
        }
    }
}
