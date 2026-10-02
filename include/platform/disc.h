#pragma once

#include "common.h"

namespace Platform::Disc
{
enum class Media : s32
{
    Cd = 1,
    Dvd = 2,
};

// Returns 0 when it failed
s32 Initialise();
s32 SetMedia(Media media);
// Waits until the drive is ready. Returns 2 when it is, 6 when it isn't
s32 WaitReady();
}
