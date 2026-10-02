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

s32 Platform::Disc::WaitReady()
{
    return sceCdDiskReady(0);
}
