#pragma once

#include "common.h"

namespace Platform::Disc
{
enum class Media : s32
{
    Cd = 1,
    Dvd = 2,
};

// What WaitReady finds (libcdvd's SCECdComplete and SCECdNotReady)
enum Readiness : s32
{
    Ready = 2,
    NotReady = 6,
};

// Returns 0 when it failed
s32 Initialise();
s32 SetMedia(Media media);
// Waits until the drive is ready
Readiness WaitReady();
}
