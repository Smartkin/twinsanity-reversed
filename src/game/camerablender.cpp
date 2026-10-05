#include "game/camerablender.h"

#include "game/clock.h"
#include "game/math.h"

EABI_EXPORT(FUN_00270d10, &AngleBlender::MoveToward);
EABI_EXPORT(FUN_00271070, &AngleBlender::Push);
EABI_EXPORT(FUN_00271520, &DistanceBlender::Step);

namespace
{
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
    blender->bits.sineSpeed = 0;
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
    BlenderBits kept = {};
    kept.holds = bits.holds;
    kept.sineSpeed = bits.sineSpeed;
    bits = kept;
    holdStart = 0;
    start = current;
    rateScale = 0.0f;
    input = 0.0f;
}

void AngleBlender::KeepWithin(const s32* toLow, const s32* toHigh)
{
    s32 next = current + delta;
    bits.atLow = !(*toLow < next);
    bits.atHigh = !(next < *toHigh);
    if (bits.atLow != 0)
    {
        if (*toLow <= current)
        {
            delta = *toLow - current;
        }
    }
    else if (bits.atHigh != 0)
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
    else if (bits.atLow != 0)
    {
        share = 0.0f;
    }
    else if (bits.atHigh != 0)
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
            if (bits.sineSpeed != 0 && bits.atOwnSpeed == 0)
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
            if (bits.sineSpeed != 0 && bits.atOwnSpeed == 0)
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
    if (bits.enabled == 0)
    {
        return bits.pushed;
    }

    delta = static_cast<s32>(static_cast<f32>(*rate) * seconds);
    if (clamp != 0)
    {
        s32 toLow = low;
        s32 toHigh = high;
        if (bits.secondRange != 0)
        {
            toLow = secondLow;
            toHigh = secondHigh;
        }

        KeepWithin(&toLow, &toHigh);
    }

    bits.pushed = 1;
    return bits.pushed;
}

u32 AngleBlender::Step(TimeClock* clock, const s32* value, u32 clamp, u32 jump)
{
    f32 seconds = StepSeconds(clock);
    if (bits.enabled == 0)
    {
        return 0;
    }

    if (bits.pushed != 0)
    {
        bits.moving = 0;
        bits.atOwnSpeed = 0;
        return 0;
    }

    if (bits.secondRange != 0)
    {
        s32 toLow = secondLow;
        s32 toHigh = secondHigh;
        bits.moving = MoveToward(seconds, &toLow, &toHigh, jump);
        if (bits.moving != 0)
        {
            bits.atOwnSpeed = 1;
            return 1;
        }

        // Within it: moved by its input
        s32 rate = inputSpeed;
        rate = *MultiplyAngle(&rate, input);
        s32 move = *MultiplyAngle(&rate, seconds);
        bits.atOwnSpeed = 0;
        delta = move;
        return 1;
    }

    if (bits.blendedGoal != 0)
    {
        f32 lowRadians = static_cast<f32>(low) * AngleToRadians;
        f32 highRadians = static_cast<f32>(high) * AngleToRadians;
        s32 goal = static_cast<s32>((highRadians * goalShare + lowRadians * (1.0f - goalShare)) * RadiansToAngle);
        s32 toLow = goal;
        s32 toHigh = goal;
        bits.moving = MoveToward(seconds, &toLow, &toHigh, jump);
        bits.atOwnSpeed = 1;
        return 1;
    }

    f32 step = seconds;
    bits.moving = 0.0f < rateScale;
    if (bits.moving != 0)
    {
        bits.atOwnSpeed = 1;
        step = seconds * rateScale;
    }
    else if (bits.holds != 0 && !(static_cast<s32>(clock->time - holdStart) < holdTicks))
    {
        bits.moving = 1;
        bits.atOwnSpeed = 0;
    }

    if (bits.moving != 0)
    {
        s32 toLow = *value;
        s32 toHigh = *value;
        MoveToward(step, &toLow, &toHigh, jump);
        return 1;
    }

    if (__builtin_fabsf(input) <= Epsilon)
    {
        return 0;
    }

    bits.moving = 0;
    bits.atOwnSpeed = 0;
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
    if (bits.enabled == 0)
    {
        return 0;
    }

    if (bits.pushed != 0)
    {
        bits.moving = 0;
        bits.atOwnSpeed = 0;
        return 0;
    }

    if (bits.secondRange != 0)
    {
        bits.moving = MoveDistance(this, secondLow - current, secondHigh - current, seconds, jump);
        if (bits.moving != 0)
        {
            bits.atOwnSpeed = 1;
            return 1;
        }

        bits.atOwnSpeed = 0;
        delta = inputSpeed * input * seconds;
        return 1;
    }

    if (bits.blendedGoal != 0)
    {
        f32 goal = high * goalShare + low * (1.0f - goalShare);
        bits.moving = MoveDistance(this, goal - current, goal - current, seconds, jump);
        bits.atOwnSpeed = 1;
        return 1;
    }

    f32 step = seconds;
    bits.moving = 0.0f < rateScale;
    if (bits.moving != 0)
    {
        bits.atOwnSpeed = 1;
        step = seconds * rateScale;
    }
    else if (bits.holds != 0 && !(static_cast<s32>(clock->time - holdStart) < holdTicks))
    {
        bits.moving = 1;
        bits.atOwnSpeed = 0;
    }

    if (bits.moving != 0)
    {
        MoveDistance(this, value - current, value - current, step, jump);
        return 1;
    }

    if (__builtin_fabsf(input) <= Epsilon)
    {
        return 0;
    }

    bits.moving = 0;
    bits.atOwnSpeed = 0;
    if (bits.enabled == 0)
    {
        return bits.pushed;
    }

    delta = inputSpeed * input * seconds;
    if (clamp != 0)
    {
        f32 toLow = low;
        f32 toHigh = high;
        if (bits.secondRange != 0)
        {
            toLow = secondLow;
            toHigh = secondHigh;
        }

        KeepWithin(toLow, toHigh);
    }

    bits.pushed = 1;
    return bits.pushed;
}

void DistanceBlender::KeepWithin(f32 toLow, f32 toHigh)
{
    f32 next = current + delta;
    bits.atLow = next <= toLow;
    bits.atHigh = toHigh <= next;
    if (bits.atLow != 0)
    {
        if (toLow <= current)
        {
            delta = toLow - current;
        }
    }
    else if (bits.atHigh != 0)
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
    else if (bits.atLow != 0)
    {
        share = 0.0f;
    }
    else if (bits.atHigh != 0)
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
    constexpr f32 NoRange = Epsilon;
    f32 range = high - low;
    delta = 0.0f;
    current = initial;
    share = __builtin_fabsf(range) <= NoRange ? Infinite : (initial - low) / range;
    previousSpeed = speed;
    BlenderBits kept = {};
    kept.holds = bits.holds;
    bits = kept;
    start = current;
    input = 0.0f;
    unused48 = 0.0f;
    rateScale = 0.0f;
}
