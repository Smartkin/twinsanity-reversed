#include "game/scripttokens.h"

#include "game/disk.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/properties.h"

extern "C"
{
    extern const GccVTableEntry g_ScriptTokenReaderVTable[] RETAIL(DiskDataReaderMethods);
    extern const GccVTableEntry g_ScriptTokenReaderBaseVTable[] RETAIL(D_002F0250);
    extern const GccVTableEntry g_OtherTokenReaderBaseVTable[] RETAIL(D_002FD050);
}

namespace
{
constexpr u8 PropertyRecord = 10;

void DestroyReader(ScriptTokenReader* reader, u32 destroyFlags, const GccVTableEntry* base)
{
    reader->vtable = base;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}
}

ScriptTokenReader* ScriptTokenReader::Construct(ScriptTokenReader* reader, const ScriptTokenList* list)
{
    reader->list = list;
    reader->vtable = g_ScriptTokenReaderVTable;
    reader->tokens = reinterpret_cast<const ScriptToken*>(DiskLoadedMemory(GetDiskManager(), &list->block));
    reader->First();
    return reader;
}

void ScriptTokenReader::Destroy(u32 destroyFlags)
{
    DestroyReader(this, destroyFlags, g_ScriptTokenReaderBaseVTable);
}

void ScriptTokenReader::First()
{
    index = 0;
}

bool ScriptTokenReader::AtEnd() const
{
    return !(index < list->count);
}

const ScriptToken* ScriptTokenReader::Current() const
{
    return &tokens[index];
}

void ScriptTokenReader::Next()
{
    index++;
}

void ScriptTokenReader::BaseDestroy(u32 destroyFlags)
{
    DestroyReader(this, destroyFlags, g_ScriptTokenReaderBaseVTable);
}

void ScriptTokenReader::OtherDestroy(u32 destroyFlags)
{
    DestroyReader(this, destroyFlags, g_OtherTokenReaderBaseVTable);
}

void ScriptTokenReader::OtherBaseDestroy(u32 destroyFlags)
{
    DestroyReader(this, destroyFlags, g_OtherTokenReaderBaseVTable);
}

void ScriptTokenReader::OtherFirst()
{
    index = 0;
}

bool ScriptTokenReader::OtherAtEnd() const
{
    return !(index < list->count);
}

const ScriptToken* ScriptTokenReader::OtherCurrent() const
{
    return &tokens[index];
}

void ScriptTokenReader::OtherNext()
{
    index++;
}

void ParseTaggedValueRecord(const ScriptToken* token, TaggedValue* value)
{
    constexpr u32 IntType = 0;
    constexpr u32 AngleType = 1;
    constexpr u32 FloatType = 2;
    constexpr u32 Degrees = 1;
    u32 type = (static_cast<u32>(value->raw) & TaggedValue::TypeMask) >> TaggedValue::TypeShift;
    if (token->type == PropertyRecord)
    {
        value->SetProperty(type, token->value);
        return;
    }

    if (type == AngleType)
    {
        s32 angle[4];
        AngleFrom(angle, token->Float(), Degrees);
        s32 turned = angle[0];
        value->SetAngle(&turned);
    }
    else if (type == IntType)
    {
        value->SetInt(static_cast<s32>(token->value));
    }
    else if (type == FloatType)
    {
        value->SetFloat(token->Float());
    }
}
