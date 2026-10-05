#include "platform/disc.h"

#include <libcdvd.h>

// PS2SDK's libxcdvd, for the CD/DVD server of the disc's IOPRP image
s32 Platform::Disc::Initialise()
{
    return sceCdInit(SCECdINIT);
}

s32 Platform::Disc::SetMedia(Media media)
{
    return sceCdMmode(media == Media::Dvd ? SCECdMmodeDvd : SCECdMmodeCd);
}

// Mode 0 waits until it's ready
Platform::Disc::Readiness Platform::Disc::WaitReady()
{
    return static_cast<Readiness>(sceCdDiskReady(0));
}
