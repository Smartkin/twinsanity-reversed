#include "platform/files.h"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

// The desktop's side of Platform::Files: the disc's files in a folder ($TWINSANITY_DISC, "disc" by default), its paths matched
// whatever their case (the disc's names are upper case, the game asks in any)
namespace
{
// The longest host path and file name taken (the disc's are far shorter)
constexpr size_t PathSize = 4096;
constexpr size_t NameSize = 256;

// Appends a part of a disc path to a host folder's path: the folder's entry of that name in any case, the name as it is when
// there's none. False when it doesn't fit
bool AppendEntry(char* host, const char* part, size_t length)
{
    char name[NameSize];
    if (length >= NameSize)
    {
        return false;
    }

    std::memcpy(name, part, length);
    name[length] = '\0';
    const char* found = name;
    DIR* directory = opendir(host);
    if (directory != nullptr)
    {
        while (dirent* entry = readdir(directory))
        {
            if (strcasecmp(entry->d_name, name) == 0)
            {
                found = entry->d_name;
                break;
            }
        }
    }

    size_t hostLength = std::strlen(host);
    size_t foundLength = std::strlen(found);
    bool fits = hostLength + 1 + foundLength < PathSize;
    if (fits)
    {
        host[hostLength] = '/';
        std::memcpy(host + hostLength + 1, found, foundLength + 1);
    }

    if (directory != nullptr)
    {
        closedir(directory);
    }

    return fits;
}

// The host's path of a disc path (either slash), false when it doesn't fit
bool HostPath(const char* path, char* host)
{
    const char* folder = std::getenv("TWINSANITY_DISC");
    if (folder == nullptr || *folder == '\0')
    {
        folder = "disc";
    }

    size_t folderLength = std::strlen(folder);
    if (folderLength >= PathSize)
    {
        return false;
    }

    std::memcpy(host, folder, folderLength + 1);
    const char* part = path;
    for (const char* character = path;; character++)
    {
        if (*character != '\\' && *character != '/' && *character != '\0')
        {
            continue;
        }

        size_t length = static_cast<size_t>(character - part);
        if (length != 0 && !(length == 1 && *part == '.') && !AppendEntry(host, part, length))
        {
            return false;
        }

        if (*character == '\0')
        {
            return true;
        }

        part = character + 1;
    }
}
}

void Platform::Files::Reset()
{
}

s32 Platform::Files::Open(const char* path, s32 flags)
{
    char host[PathSize];
    if (!HostPath(path, host))
    {
        return -1;
    }

    int mode = (flags & OpenReadWrite) == OpenReadWrite ? O_RDWR : (flags & OpenWrite) != 0 ? O_WRONLY : O_RDONLY;
    if ((flags & OpenAppend) != 0)
    {
        mode |= O_APPEND;
    }

    if ((flags & OpenCreate) != 0)
    {
        mode |= O_CREAT;
    }

    if ((flags & OpenTruncate) != 0)
    {
        mode |= O_TRUNC;
    }

#if defined(O_BINARY)
    // Windows opens files as text otherwise
    mode |= O_BINARY;
#endif
    return open(host, mode, 0644);
}

s32 Platform::Files::Close(s32 file)
{
    return close(file);
}

s32 Platform::Files::Read(s32 file, void* buffer, s32 size)
{
    return static_cast<s32>(read(file, buffer, static_cast<size_t>(size)));
}

s32 Platform::Files::Write(s32 file, const void* buffer, s32 size)
{
    return static_cast<s32>(write(file, buffer, static_cast<size_t>(size)));
}

s32 Platform::Files::Seek(s32 file, s32 offset, Whence whence)
{
    int from = whence == SeekSet ? SEEK_SET : whence == SeekCurrent ? SEEK_CUR : SEEK_END;
    return static_cast<s32>(lseek(file, offset, from));
}
