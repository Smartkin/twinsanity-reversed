#include "memorycard.h"

#include <libmc.h>

// PS2SDK's libmc with XMCSERV's protocol, which the disc's MCSERV speaks. Its function numbers and results are the interface's
static_assert(sizeof(Platform::MemoryCard::DirectoryEntry) == sizeof(sceMcTblGetDir));

s32 Platform::MemoryCard::Initialise()
{
    static bool initialised;
    if (initialised)
    {
        return 0;
    }

    s32 result = mcInit(MC_TYPE_XMC);
    initialised = result >= 0;
    return result;
}

s32 Platform::MemoryCard::GetInfo(s32 port, s32 slot, s32* type, s32* freeClusters, s32* format)
{
    return mcGetInfo(port, slot, type, freeClusters, format);
}

s32 Platform::MemoryCard::Open(s32 port, s32 slot, const char* name, s32 flags)
{
    return mcOpen(port, slot, name, flags);
}

s32 Platform::MemoryCard::Close(s32 file)
{
    return mcClose(file);
}

s32 Platform::MemoryCard::Read(s32 file, void* buffer, s32 size)
{
    return mcRead(file, buffer, size);
}

s32 Platform::MemoryCard::Write(s32 file, const void* buffer, s32 size)
{
    return mcWrite(file, buffer, size);
}

s32 Platform::MemoryCard::MakeDirectory(s32 port, s32 slot, const char* name)
{
    return mcMkDir(port, slot, name);
}

s32 Platform::MemoryCard::ChangeDirectory(s32 port, s32 slot, const char* directory, char* currentDirectory)
{
    // Sony's libmc only hands the directory back when it's asked for, PS2SDK's always copies it (mcStoreDir, once the call has
    // finished): without somewhere to go it went to address 0
    static char unwanted[1024];
    return mcChdir(port, slot, directory, currentDirectory != nullptr ? currentDirectory : unwanted);
}

s32 Platform::MemoryCard::GetDirectory(s32 port, s32 slot, const char* name, u32 mode, s32 maxEntries, DirectoryEntry* table)
{
    return mcGetDir(port, slot, name, mode, maxEntries, reinterpret_cast<sceMcTblGetDir*>(table));
}

s32 Platform::MemoryCard::Delete(s32 port, s32 slot, const char* name)
{
    return mcDelete(port, slot, name);
}

s32 Platform::MemoryCard::Format(s32 port, s32 slot)
{
    return mcFormat(port, slot);
}

s32 Platform::MemoryCard::Sync(SyncMode mode, s32* function, s32* result)
{
    return mcSync(mode, function, result);
}
