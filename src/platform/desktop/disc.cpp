#include "platform/disc.h"

// The desktop's side of Platform::Disc: the disc's files are a folder, which is always there
namespace Platform::Disc
{
s32 Initialise()
{
    return 0;
}

s32 SetMedia(Media media)
{
    return 0;
}

Readiness WaitReady()
{
    return {};
}

}
