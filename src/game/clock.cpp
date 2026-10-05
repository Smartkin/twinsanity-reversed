#include "game/clock.h"

#include "debug.h"
#include "game/memory.h"
#include "platform/time.h"

namespace
{
// Clock units are the platform's ticks halved until there are at most this many a second
constexpr u32 MaxClockUnitsPerSecond = 9000;
// Ticks a frame may go past its time and still count as that frame
constexpr u64 FrameSlack = 0x4E;

// Test runs' time (g_DebugFixedTime): the ticks, a frame's worth more each frame, and the time checks a frame's work has left
constexpr u32 TestTimeChecks = 16;
u64 g_TestTicks;
u32 g_TestTimeChecks;

u64 Now()
{
    return g_DebugFixedTime != 0 ? g_TestTicks : Platform::Time::Ticks();
}
}

EABI_EXPORT(FUN_00191dd8, TimeClockAdvanceBy);
EABI_EXPORT(FUN_00192360, GameTimeHasTimeLeft);

extern "C"
{
    void TimeClockReset(TimeClock* clock)
    {
        clock->advance = 0;
        clock->flags.value = 0;
        clock->flags.restarts = 1;
        clock->time = 0;
    }

    bool TimeClockAdvance(TimeClock* clock, u32 units)
    {
        if (clock->flags.running == 0)
        {
            clock->advance = 0;
            return false;
        }

        if (clock->flags.restarts != 0)
        {
            clock->time = units;
            clock->advance = 0;
            clock->flags.restarts = 0;
        }
        else
        {
            clock->advance = units;
            clock->time += units;
        }

        return true;
    }

    bool TimeClockAdvanceBy(f32 speed, s32 frameTime, TimeClock* clock)
    {
        f32 seconds = g_FixedFrameSeconds;
        if (seconds <= 0.0f)
        {
            seconds = static_cast<f32>(frameTime) * g_SecondsPerClockUnit;
        }

        return TimeClockAdvance(clock, static_cast<u32>(static_cast<s32>(seconds * speed * g_ClockUnitsPerSecond)));
    }

    TimeClock* TimeClocksConstruct(TimeClock* clocks)
    {
        for (u32 i = 0; i < GameClockCount; i++)
        {
            TimeClockReset(&clocks[i]);
        }

        return clocks;
    }

    void TimeClocksDestroy(TimeClock* clocks, u32 flags)
    {
        if ((flags & 1) != 0)
        {
            MemoryDeallocate2_(clocks);
        }
    }

    void TimeClocksStart(TimeClock* clocks)
    {
        for (u32 i = 0; i < GameClockCount; i++)
        {
            clocks[i].flags.running = 1;
        }
    }

    void TimeClocksStop(TimeClock* clocks)
    {
        for (u32 i = 0; i < GameClockCount; i++)
        {
            clocks[i].flags.running = 0;
        }
    }

    void AdvanceClocks(TimeClock* clocks, GameTimeController* controller)
    {
        s32 frameTime = controller->frameTime;
        for (u32 i = 0; i < GameClockCount; i++)
        {
            TimeClockAdvanceBy(g_ClockSpeeds[i], frameTime, &clocks[i]);
        }
    }

    void ResetGlobalClock()
    {
        TimeClockReset(&g_GlobalClock);
        g_GlobalClock.flags.running = 1;
    }

    bool AdvanceGlobalClock(GameTimeController* controller)
    {
        return TimeClockAdvanceBy(1.0f, controller->frameTime, &g_GlobalClock);
    }

    void ResetClockSpeeds()
    {
        for (u32 i = 0; i < GameClockCount; i++)
        {
            g_ClockSpeeds[i] = 1.0f;
        }
    }

    GameTimeController* GameTimeConstructStamp(GameTimeController* controller)
    {
        controller->lastStamp = 0;
        return controller;
    }

    GameTimeController* GameTimeConstruct(GameTimeController* controller, s32 framesPerSecond)
    {
        GameTimeConstructStamp(controller);
        TimeClocksConstruct(controller->clocks);
        controller->framesPerSecond = framesPerSecond;
        controller->frameStart = 0;
        Platform::Time::Initialise();
        controller->ticksPerSecond = Platform::Time::TicksPerSecond;
        controller->frameSlack = FrameSlack;
        controller->shift = 0;
        s32 unitsPerSecond = static_cast<s32>(Platform::Time::TicksPerSecond);
        do
        {
            unitsPerSecond >>= 1;
            controller->shift++;
        } while (unitsPerSecond > static_cast<s32>(MaxClockUnitsPerSecond));

        g_ClockUnitsPerSecond = static_cast<f32>(unitsPerSecond);
        g_SecondsPerClockUnit = 1.0f / g_ClockUnitsPerSecond;
        ResetGlobalClock();
        ResetClockSpeeds();
        TimeClocksStart(controller->clocks);
        return controller;
    }

    void GameTimeTick(GameTimeController* controller)
    {
        if (g_DebugFixedTime != 0)
        {
            g_TestTicks += static_cast<u32>(controller->ticksPerSecond) / static_cast<u32>(controller->framesPerSecond);
            g_TestTimeChecks = TestTimeChecks;
        }

        s32 stamp = GameTimeStamp(controller);
        controller->frameStart = Now();
        GameTimeSetStamp(controller, stamp);
        AdvanceGlobalClock(controller);
        AdvanceClocks(controller->clocks, controller);
    }

    s32 GameTimeStamp(GameTimeController* controller)
    {
        return static_cast<s32>(Now() >> controller->shift);
    }

    // A stamp before the last one (the clock wrapped) leaves the frame's length as it was
    void GameTimeSetStamp(GameTimeController* controller, s32 stamp)
    {
        if (controller->lastStamp <= stamp)
        {
            controller->frameTime = stamp - controller->lastStamp;
        }

        controller->lastStamp = stamp;
    }

    void GameTimeFrameRendered(GameTimeController* controller)
    {
        GameTimeStamp(controller);
        CountRenderedFrame();
    }

    void CountRenderedFrame()
    {
        g_RenderedFrames++;
    }

    // Past the frame's time (and its slack), the time is taken within the frames since. The comparison is unsigned: in the
    // slack, the time left wraps around to plenty
    bool GameTimeHasTimeLeft(f32 seconds, GameTimeController* controller)
    {
        if (g_DebugFixedTime != 0)
        {
            if (g_TestTimeChecks == 0)
            {
                return false;
            }

            g_TestTimeChecks--;
            return true;
        }

        u32 ticksPerSecond = static_cast<u32>(controller->ticksPerSecond);
        s32 wanted = static_cast<s32>(seconds * static_cast<f32>(static_cast<s32>(ticksPerSecond)));
        u64 frameTicks = ticksPerSecond / static_cast<u32>(controller->framesPerSecond);
        u64 elapsed = Platform::Time::Ticks() - controller->frameStart;
        u64 limit = frameTicks + controller->frameSlack;
        while (limit < elapsed)
        {
            elapsed -= frameTicks;
        }

        return static_cast<u64>(static_cast<s64>(wanted)) < frameTicks - elapsed;
    }
}
