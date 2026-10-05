#include "platform/files.h"
#include "stack.h"

// The game has its own C library, not newlib's POSIX layer over fileio that this guards
#define NEWLIB_PORT_AWARE
#include <fileio.h>

// PS2SDK's fileio (the IOP's FILEIO: its open flags and whences are the interface's). Its calls send the IOP what they build on
// their stack
void Platform::Files::Reset()
{
    fioExit();
}

// The disc's file system is ISO 9660's: names in upper case, backslashes, and the version after them
s32 Platform::Files::Open(const char* path, s32 flags)
{
    constexpr const char* Device = "cdrom0:\\";
    static constexpr char Version[] = ";1";
    constexpr u32 MaxPath = 256;
    char devicePath[MaxPath];
    u32 length = 0;
    for (const char* character = Device; *character != '\0'; character++)
    {
        devicePath[length++] = *character;
    }

    // Room is left for the version and the terminator
    for (const char* character = path; *character != '\0' && length < MaxPath - sizeof(Version); character++)
    {
        char converted = *character;
        if (converted >= 'a' && converted <= 'z')
        {
            converted = static_cast<char>(converted - 'a' + 'A');
        }
        else if (converted == '/')
        {
            converted = '\\';
        }

        devicePath[length++] = converted;
    }

    for (const char* character = Version; *character != '\0'; character++)
    {
        devicePath[length++] = *character;
    }

    devicePath[length] = '\0';
    return OnMainMemoryStack([&devicePath, flags] { return fioOpen(devicePath, flags); });
}

s32 Platform::Files::Close(s32 file)
{
    return OnMainMemoryStack([file] { return fioClose(file); });
}

s32 Platform::Files::Read(s32 file, void* buffer, s32 size)
{
    return OnMainMemoryStack([file, buffer, size] { return fioRead(file, buffer, size); });
}

s32 Platform::Files::Write(s32 file, const void* buffer, s32 size)
{
    return OnMainMemoryStack([file, buffer, size] { return fioWrite(file, buffer, size); });
}

s32 Platform::Files::Seek(s32 file, s32 offset, Whence whence)
{
    return OnMainMemoryStack([file, offset, whence] { return fioLseek(file, offset, whence); });
}
