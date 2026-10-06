#include "platform/files.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <strings.h>
#include <unistd.h>

// The desktop's side of Platform::Files: the disc's files in a folder ($TWINSANITY_DISC, "disc" by default), its paths matched
// whatever their case (the disc's names are upper case, the game asks in any)
namespace
{
std::string DiscFolder()
{
    const char* folder = std::getenv("TWINSANITY_DISC");
    return folder != nullptr && *folder != '\0' ? folder : "disc";
}

// The folder's entry of a name in any case, the name as it is when there's none
std::string Entry(const std::string& folder, const std::string& name)
{
    DIR* directory = opendir(folder.c_str());
    if (directory == nullptr)
    {
        return name;
    }

    std::string found = name;
    while (dirent* entry = readdir(directory))
    {
        if (strcasecmp(entry->d_name, name.c_str()) == 0)
        {
            found = entry->d_name;
            break;
        }
    }

    closedir(directory);
    return found;
}

std::string HostPath(const char* path)
{
    std::string host = DiscFolder();
    std::string part;
    for (const char* character = path;; character++)
    {
        if (*character == '\\' || *character == '/' || *character == '\0')
        {
            if (!part.empty() && part != ".")
            {
                host = host + "/" + Entry(host, part);
            }

            part.clear();
            if (*character == '\0')
            {
                break;
            }

            continue;
        }

        part += *character;
    }

    return host;
}
}

void Platform::Files::Reset()
{
}

s32 Platform::Files::Open(const char* path, s32 flags)
{
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

    return open(HostPath(path).c_str(), mode, 0644);
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
