#include "game/chunkdata.h"

#include "game/clock.h"
#include "game/math.h"
#include "game/memory.h"

// A chunk's wind (the chunk data's, which nothing makes in retail): its update and its destructor

void UpdateChunkWind(ChunkWind* wind, GameTimeController* time)
{
    constexpr u32 Phases = 16;
    const TimeClock* clock = &time->clocks[0];
    if ((clock->flags & TimeClock::FlagRunning) == 0)
    {
        return;
    }

    f32 seconds = wind->seconds + static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    wind->seconds = seconds;
    s32 swing;
    AngleFrom(&swing, seconds * 6.0f, AngleDegrees);
    s32 direction;
    AngleFrom(&direction, SinOfAngle(&swing) * 30.0f + 75.0f, AngleDegrees);
    CosSin16(&direction, &wind->directionX, &wind->directionZ);
    for (u32 phase = 0; phase < Phases; phase++)
    {
        s32 angle;
        AngleFrom(&angle, wind->seconds * 35.0f + static_cast<f32>(static_cast<s32>(phase)) * 22.5f, AngleDegrees);
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
