#include "game/camerablender.h"

#include "game/clock.h"
#include "game/math.h"

EABI_EXPORT(FUN_00270d10, &AngleBlender::MoveToward);
EABI_EXPORT(FUN_00271070, &AngleBlender::Push);
EABI_EXPORT(FUN_00271520, &DistanceBlender::Step);

namespace
{
constexpr f32 NoInput = Rounded(5e-5);
// 65536ths of a turn to radians and back
constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr f32 RadiansToAngle = 0x1.45f306p+13f;

f32 StepSeconds(const TimeClock* clock)
{
    return static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
}

// Where a value is between two ends (half way when they're the same)
f32 ShareBetween(s32 value, s32 low, s32 high)
{
    s32 range = high - low;
    f32 share = 0.5f;
    if (range != 0)
    {
        share = static_cast<f32>(value - low) / static_cast<f32>(range);
    }

    return share;
}

// A distance's move toward two ends at its speed for a time, up to the low one when it's above it by over 0.1, else down to the
// high one when it's below it by over 0.1 (straight there when it jumps): whether it moves
u32 MoveDistance(DistanceBlender* blender, f32 toLow, f32 toHigh, f32 seconds, u32 jump)
{
    constexpr f32 Near = Rounded(0.1);
    if (Near < toLow)
    {
        if (jump != 0)
        {
            blender->delta = toLow;
            return 1;
        }

        f32 move = blender->speed * seconds;
        blender->delta = move;
        if (toLow < move)
        {
            blender->delta = toLow;
        }

        return 1;
    }

    if (!(toHigh < -Near))
    {
        blender->delta = 0.0f;
        return 0;
    }

    if (jump != 0)
    {
        blender->delta = toHigh;
        return 1;
    }

    f32 move = -blender->speed * seconds;
    blender->delta = move;
    if (move < toHigh)
    {
        blender->delta = toHigh;
    }

    return 1;
}
}

AngleBlender* AngleBlender::Construct(AngleBlender* blender, const s32* value)
{
    constexpr f32 Speed = 90.0f;
    blender->initial = *value;
    AngleFrom(&blender->speed, Speed, AngleDegrees);
    AngleFrom(&blender->inputSpeed, Speed, AngleDegrees);
    blender->low = *value;
    blender->bits &= ~BitSineSpeed;
    blender->high = *value;
    blender->secondLow = *value;
    blender->secondHigh = *value;
    blender->Reset();
    return blender;
}

void AngleBlender::Reset()
{
    current = initial;
    previousSpeed = speed;
    delta = 0;
    share = ShareBetween(initial, low, high);
    bits = (bits & BitHolds) | (bits & BitSineSpeed);
    holdStart = 0;
    start = current;
    rateScale = 0.0f;
    input = 0.0f;
}

void AngleBlender::KeepWithin(const s32* toLow, const s32* toHigh)
{
    s32 next = current + delta;
    bits = (bits & ~BitAtLow) | (!(*toLow < next) ? BitAtLow : 0);
    bits = (bits & ~BitAtHigh) | (!(next < *toHigh) ? BitAtHigh : 0);
    if ((bits & BitAtLow) != 0)
    {
        if (*toLow <= current)
        {
            delta = *toLow - current;
        }
    }
    else if ((bits & BitAtHigh) != 0)
    {
        if (current <= *toHigh)
        {
            delta = *toHigh - current;
        }
    }

    if (*toLow == *toHigh)
    {
        share = 0.5f;
    }
    else if ((bits & BitAtLow) != 0)
    {
        share = 0.0f;
    }
    else if ((bits & BitAtHigh) != 0)
    {
        share = 1.0f;
    }
    else
    {
        share = ShareBetween(next, *toLow, *toHigh);
    }
}

u32 AngleBlender::MoveToward(f32 seconds, const s32* toLow, const s32* toHigh, u32 jump)
{
    u32 moving = 1;
    s32 deadZone = g_CameraAngleDeadZone;
    s32 gap = WrapAngle(*toLow - current);
    if (deadZone < gap)
    {
        if (jump != 0)
        {
            delta = gap;
        }
        else
        {
            delta = static_cast<s32>(static_cast<f32>(speed) * seconds);
            if ((bits & (BitSineSpeed | BitAtOwnSpeed)) == BitSineSpeed)
            {
                delta = static_cast<s32>(static_cast<f32>(delta) * __builtin_fabsf(SinOfAngle(&gap)));
            }

            if (gap < delta)
            {
                delta = gap;
            }
        }
    }
    else
    {
        gap = WrapAngle(*toHigh - current);
        if (!(gap < -deadZone))
        {
            delta = 0;
            moving = 0;
        }
        else if (jump != 0)
        {
            delta = gap;
        }
        else
        {
            delta = static_cast<s32>(static_cast<f32>(-speed) * seconds);
            if ((bits & (BitSineSpeed | BitAtOwnSpeed)) == BitSineSpeed)
            {
                delta = static_cast<s32>(static_cast<f32>(delta) * __builtin_fabsf(SinOfAngle(&gap)));
            }

            if (delta < gap)
            {
                delta = gap;
            }
        }
    }

    share = ShareBetween(current + delta, low, high);
    return moving;
}

u32 AngleBlender::Push(f32 seconds, const s32* rate, u32 clamp)
{
    if ((bits & BitEnabled) == 0)
    {
        return bits >> 4 & 1;
    }

    delta = static_cast<s32>(static_cast<f32>(*rate) * seconds);
    if (clamp != 0)
    {
        s32 toLow = low;
        s32 toHigh = high;
        if ((bits & BitSecondRange) != 0)
        {
            toLow = secondLow;
            toHigh = secondHigh;
        }

        KeepWithin(&toLow, &toHigh);
    }

    bits |= BitPushed;
    return bits >> 4 & 1;
}

u32 AngleBlender::Step(TimeClock* clock, const s32* value, u32 clamp, u32 jump)
{
    f32 seconds = StepSeconds(clock);
    if ((bits & BitEnabled) == 0)
    {
        return 0;
    }

    if ((bits & BitPushed) != 0)
    {
        bits = bits & ~BitMoving & ~BitAtOwnSpeed;
        return 0;
    }

    if ((bits & BitSecondRange) != 0)
    {
        s32 toLow = secondLow;
        s32 toHigh = secondHigh;
        u32 moving = MoveToward(seconds, &toLow, &toHigh, jump);
        bits = (bits & ~BitMoving) | (moving & 1) << 1;
        if ((bits & BitMoving) != 0)
        {
            bits |= BitAtOwnSpeed;
            return 1;
        }

        // Within it: moved by its input
        s32 rate = inputSpeed;
        rate = *MultiplyAngle(&rate, input);
        s32 move = *MultiplyAngle(&rate, seconds);
        bits &= ~BitAtOwnSpeed;
        delta = move;
        return 1;
    }

    if ((bits & BitBlendedGoal) != 0)
    {
        f32 lowRadians = static_cast<f32>(low) * AngleToRadians;
        f32 highRadians = static_cast<f32>(high) * AngleToRadians;
        s32 goal = static_cast<s32>((highRadians * goalShare + lowRadians * (1.0f - goalShare)) * RadiansToAngle);
        s32 toLow = goal;
        s32 toHigh = goal;
        u32 moving = MoveToward(seconds, &toLow, &toHigh, jump);
        bits = (bits & ~BitMoving) | (moving & 1) << 1 | BitAtOwnSpeed;
        return 1;
    }

    f32 step = seconds;
    u32 scaled = 0.0f < rateScale;
    bits = (bits & ~BitMoving) | scaled << 1;
    if ((bits & BitMoving) != 0)
    {
        bits |= BitAtOwnSpeed;
        step = seconds * rateScale;
    }
    else if ((bits & BitHolds) != 0 && !(static_cast<s32>(clock->time - holdStart) < holdTicks))
    {
        bits = (bits | BitMoving) & ~BitAtOwnSpeed;
    }

    if ((bits & BitMoving) != 0)
    {
        s32 toLow = *value;
        s32 toHigh = *value;
        MoveToward(step, &toLow, &toHigh, jump);
        return 1;
    }

    if (__builtin_fabsf(input) <= NoInput)
    {
        return 0;
    }

    bits = bits & ~BitMoving & ~BitAtOwnSpeed;
    s32 rate = static_cast<s32>(static_cast<f32>(inputSpeed) * input);
    return Push(seconds, &rate, clamp);
}

void AngleBlender::TimeToward(const s32* ticks, const s32* toLow, const s32* toHigh)
{
    s32 towardHigh = WrapAngle(*toHigh - current);
    s32 towardLow = WrapAngle(*toLow - current);
    s32 nearer = towardLow < towardHigh ? towardLow : towardHigh;
    previousSpeed = speed;
    if (*ticks == 0)
    {
        speed = nearer;
    }
    else
    {
        speed = *DivideAngle(&nearer, static_cast<f32>(*ticks) * g_SecondsPerClockUnit);
    }

    if (speed < 0)
    {
        speed = -speed;
    }
}

u32 DistanceBlender::Step(f32 value, TimeClock* clock, u32 clamp, u32 jump)
{
    f32 seconds = StepSeconds(clock);
    if ((bits & AngleBlender::BitEnabled) == 0)
    {
        return 0;
    }

    if ((bits & AngleBlender::BitPushed) != 0)
    {
        bits = bits & ~AngleBlender::BitMoving & ~AngleBlender::BitAtOwnSpeed;
        return 0;
    }

    if ((bits & AngleBlender::BitSecondRange) != 0)
    {
        u32 moving = MoveDistance(this, secondLow - current, secondHigh - current, seconds, jump);
        bits = (bits & ~AngleBlender::BitMoving) | moving << 1;
        if ((bits & AngleBlender::BitMoving) != 0)
        {
            bits |= AngleBlender::BitAtOwnSpeed;
            return 1;
        }

        bits &= ~AngleBlender::BitAtOwnSpeed;
        delta = inputSpeed * input * seconds;
        return 1;
    }

    if ((bits & AngleBlender::BitBlendedGoal) != 0)
    {
        f32 goal = high * goalShare + low * (1.0f - goalShare);
        u32 moving = MoveDistance(this, goal - current, goal - current, seconds, jump);
        bits = (bits & ~AngleBlender::BitMoving) | moving << 1 | AngleBlender::BitAtOwnSpeed;
        return 1;
    }

    f32 step = seconds;
    u32 scaled = 0.0f < rateScale;
    bits = (bits & ~AngleBlender::BitMoving) | scaled << 1;
    if ((bits & AngleBlender::BitMoving) != 0)
    {
        bits |= AngleBlender::BitAtOwnSpeed;
        step = seconds * rateScale;
    }
    else if ((bits & AngleBlender::BitHolds) != 0 && !(static_cast<s32>(clock->time - holdStart) < holdTicks))
    {
        bits = (bits | AngleBlender::BitMoving) & ~AngleBlender::BitAtOwnSpeed;
    }

    if ((bits & AngleBlender::BitMoving) != 0)
    {
        MoveDistance(this, value - current, value - current, step, jump);
        return 1;
    }

    if (__builtin_fabsf(input) <= NoInput)
    {
        return 0;
    }

    bits = bits & ~AngleBlender::BitMoving & ~AngleBlender::BitAtOwnSpeed;
    if ((bits & AngleBlender::BitEnabled) == 0)
    {
        return bits >> 4 & 1;
    }

    delta = inputSpeed * input * seconds;
    if (clamp != 0)
    {
        f32 toLow = low;
        f32 toHigh = high;
        if ((bits & AngleBlender::BitSecondRange) != 0)
        {
            toLow = secondLow;
            toHigh = secondHigh;
        }

        KeepWithin(toLow, toHigh);
    }

    bits |= AngleBlender::BitPushed;
    return bits >> 4 & 1;
}

void DistanceBlender::KeepWithin(f32 toLow, f32 toHigh)
{
    f32 next = current + delta;
    bits = (bits & ~AngleBlender::BitAtLow) | (next <= toLow ? AngleBlender::BitAtLow : 0);
    bits = (bits & ~AngleBlender::BitAtHigh) | (toHigh <= next ? AngleBlender::BitAtHigh : 0);
    if ((bits & AngleBlender::BitAtLow) != 0)
    {
        if (toLow <= current)
        {
            delta = toLow - current;
        }
    }
    else if ((bits & AngleBlender::BitAtHigh) != 0)
    {
        if (current <= toHigh)
        {
            delta = toHigh - current;
        }
    }

    if (toLow == toHigh)
    {
        share = 0.5f;
    }
    else if ((bits & AngleBlender::BitAtLow) != 0)
    {
        share = 0.0f;
    }
    else if ((bits & AngleBlender::BitAtHigh) != 0)
    {
        share = 1.0f;
    }
    else
    {
        share = (next - toLow) / (toHigh - toLow);
    }
}

void DistanceBlender::Reset()
{
    constexpr f32 NoRange = Rounded(5e-5);
    constexpr f32 Far = 0x1.93e594p+99f;
    f32 range = high - low;
    delta = 0.0f;
    current = initial;
    share = __builtin_fabsf(range) <= NoRange ? Far : (initial - low) / range;
    previousSpeed = speed;
    bits &= AngleBlender::BitHolds;
    start = current;
    input = 0.0f;
    unknown48 = 0.0f;
    rateScale = 0.0f;
}
