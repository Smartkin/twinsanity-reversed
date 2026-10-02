#pragma once

#include "common.h"

namespace Platform::Sound
{
// Connects to the sound processor's driver (the PS2's SDRDRV), which the sound code then drives
s32 InitialiseRemote();
}
