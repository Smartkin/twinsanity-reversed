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
void DestroyReader(ScriptTokenReader* reader, u32 destroyFlags, const GccVTableEntry* base)
{
    reader->vtable = base;
    if ((destroyFlags & FreeAfterDestroy) != 0)
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
    u32 type = value->type;
    if (token->type == TokenProperty)
    {
        value->SetProperty(type, token->value);
        return;
    }

    if (type == TaggedValue::TypeAngle)
    {
        s32 angle[4];
        AngleFrom(angle, token->Float(), AngleDegrees);
        s32 turned = angle[0];
        value->SetAngle(&turned);
    }
    else if (type == TaggedValue::TypeInt)
    {
        value->SetInt(static_cast<s32>(token->value));
    }
    else if (type == TaggedValue::TypeFloat)
    {
        value->SetFloat(token->Float());
    }
}
