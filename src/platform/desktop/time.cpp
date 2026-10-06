#include "platform/time.h"

#include "clock.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#else
#include <cerrno>
#include <time.h>
#endif

// The desktop's side of Platform::Time: the host's monotonic clock in the PS2 timer's ticks, from the program's start
namespace
{
constexpr u64 NanosecondsPerSecond = 1000000000;

u64 g_Start = DesktopClock::Now();
}

u64 DesktopClock::Now()
{
#if defined(_WIN32)
    LARGE_INTEGER count;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    u64 ticks = static_cast<u64>(count.QuadPart);
    u64 perSecond = static_cast<u64>(frequency.QuadPart);
    return ticks / perSecond * NanosecondsPerSecond + ticks % perSecond * NanosecondsPerSecond / perSecond;
#else
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<u64>(now.tv_sec) * NanosecondsPerSecond + static_cast<u64>(now.tv_nsec);
#endif
}

void DesktopClock::SleepUntil(u64 time)
{
#if defined(_WIN32)
    // Sleep goes by the system timer's steps (a millisecond once asked for): the last one is waited out
    for (u64 now = Now(); now < time; now = Now())
    {
        u64 milliseconds = (time - now) / 1000000;
        Sleep(milliseconds > 1 ? static_cast<DWORD>(milliseconds - 1) : 0);
    }
#else
    timespec until;
    until.tv_sec = static_cast<time_t>(time / NanosecondsPerSecond);
    until.tv_nsec = static_cast<long>(time % NanosecondsPerSecond);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &until, nullptr) == EINTR)
    {
    }
#endif
}

void Platform::Time::Initialise()
{
#if defined(_WIN32)
    timeBeginPeriod(1);
#endif
    g_Start = DesktopClock::Now();
}

u64 Platform::Time::Ticks()
{
    u64 elapsed = DesktopClock::Now() - g_Start;
    u64 seconds = elapsed / NanosecondsPerSecond;
    return seconds * TicksPerSecond + (elapsed - seconds * NanosecondsPerSecond) * TicksPerSecond / NanosecondsPerSecond;
}
