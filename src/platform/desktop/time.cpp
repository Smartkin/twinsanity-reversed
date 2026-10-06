#include "platform/time.h"

#include <chrono>

// The desktop's side of Platform::Time: the steady clock in the PS2 timer's ticks, from the program's start
namespace
{
std::chrono::steady_clock::time_point g_Start = std::chrono::steady_clock::now();
}

void Platform::Time::Initialise()
{
    g_Start = std::chrono::steady_clock::now();
}

u64 Platform::Time::Ticks()
{
    auto elapsed = std::chrono::steady_clock::now() - g_Start;
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) * TicksPerSecond /
           1000000000ull;
}
