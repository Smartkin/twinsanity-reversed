#include "game/objects.h"

#include "game/memory.h"
#include "game/stream.h"

// A game object's script pack (game/objects.h): made empty, let go and read from the RM2

ScriptPack* ConstructScriptPack(ScriptPack* pack)
{
    pack->commands = nullptr;
    pack->count = 0;
    return pack;
}

void DestroyScriptPack(ScriptPack* pack, u32 destroyFlags)
{
    ScriptCommand* commands = pack->commands;
    if (commands != nullptr)
    {
        CallVirtual<void>(commands, commands->vtable, ScriptCommand::DestroySlot, u32{DestroyAndFree});
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(pack);
    }
}

void ReadScriptPack(ScriptPack* pack, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&pack->count));
    // Commands follow when the count's low byte isn't 0
    if (static_cast<u8>(pack->count) != 0)
    {
        pack->commands = ReadCommand(stream);
    }
}
