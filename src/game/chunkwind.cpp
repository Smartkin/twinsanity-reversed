#include "game/chunkdata.h"

#include "game/clock.h"
#include "game/math.h"
#include "game/memory.h"

// A chunk's wind (the chunk data's, which nothing makes in retail): its update and its destructor

namespace
{
// Its way swings once a minute (6 degrees a second) 30 degrees either side of 75, and its sway's phases, a 16th of a turn
// apart, turn 35 degrees a second
constexpr f32 SwingDegreesPerSecond = 6.0f;
constexpr f32 SwingDegrees = 30.0f;
constexpr f32 MiddleDegrees = 75.0f;
constexpr u32 Phases = 16;
constexpr f32 PhaseDegrees = 22.5f;
constexpr f32 SwayDegreesPerSecond = 35.0f;
}

void UpdateChunkWind(ChunkWind* wind, GameTimeController* time)
{
    const TimeClock* clock = &time->clocks[FirstClock];
    if (clock->flags.running == 0)
    {
        return;
    }

    f32 seconds = wind->seconds + static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    wind->seconds = seconds;
    s32 swing;
    AngleFrom(&swing, seconds * SwingDegreesPerSecond, AngleDegrees);
    s32 direction;
    AngleFrom(&direction, SinOfAngle(&swing) * SwingDegrees + MiddleDegrees, AngleDegrees);
    CosSin16(&direction, &wind->directionX, &wind->directionZ);
    for (u32 phase = 0; phase < Phases; phase++)
    {
        s32 angle;
        AngleFrom(&angle, wind->seconds * SwayDegreesPerSecond + static_cast<f32>(static_cast<s32>(phase)) * PhaseDegrees,
                  AngleDegrees);
        f32 sway = CosOfAngle(&angle);
        wind->sway[phase].x = wind->directionX * sway;
        wind->sway[phase].z = wind->directionZ * sway;
    }
}

void DestroyChunkWind(ChunkWind* wind, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(wind);
    }
}
