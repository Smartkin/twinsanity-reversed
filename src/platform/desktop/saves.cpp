#include "platform/saves.h"

// The desktop's side of Platform::Saves: no saves yet
namespace Platform::Saves
{
bool Initialise(const char* region, const char* product)
{
    return false;
}

Operation Update(s32 port, s32 slot, StorageInfo* info)
{
    return {};
}

Result GetResult(Operation operation)
{
    return {};
}

bool Format(s32 port, s32 slot)
{
    return false;
}

bool CreateSave(s32 port, s32 slot, const char* save)
{
    return false;
}

bool MeasureSave(s32 port, s32 slot, const char* save, s32* kilobytes)
{
    return false;
}

bool FindFile(s32 port, s32 slot, const char* file, const char* save, s32* size, FileEntry* entry)
{
    return false;
}

bool Write(s32 port, s32 slot, const char* save, const char* file, const void* data, s32 size)
{
    return false;
}

bool Read(s32 port, s32 slot, const char* save, const char* file, void* buffer, s32 size)
{
    return false;
}

u32 FileKilobytes(u32 size)
{
    return 0;
}

u32 SaveBytes(u32 kilobytes, u32 files)
{
    return 0;
}

}
