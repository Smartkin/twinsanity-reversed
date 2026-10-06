#pragma once

#include "common.h"

// The host's monotonic clock in nanoseconds and a wait until a time of it, from the system's own calls (time.cpp): the desktop
// side calls nothing libstdc++ compiled, whose calls are Windows' own on 32 bit Windows while the game's are System V's
namespace DesktopClock
{
u64 Now();
void SleepUntil(u64 time);
}
