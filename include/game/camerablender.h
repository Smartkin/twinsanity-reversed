#pragma once

#include "abi.h"
#include "common.h"

struct TimeClock;

// An angle's or a distance's blender's bits
union BlenderBits
{
    u32 value;
    struct
    {
        // It waits for its hold time before it goes toward a value given
        u32 holds : 1;
        // It moves toward its goal this step, at its speed as it is (not scaled by its sine)
        u32 moving : 1;
        u32 atOwnSpeed : 1;
        // It moves at all
        u32 enabled : 1;
        // It was pushed this step (its steps stop till that's cleared)
        u32 pushed : 1;
        // It's at its low or high end
        u32 atLow : 1;
        u32 atHigh : 1;
        // Its goal is its second range, or its range's ends blended by its goal share
        u32 secondRange : 1;
        u32 blendedGoal : 1;
        u32 unused9 : 1;
        // Its speed is scaled by the sine of how far it has to go
        u32 sineSpeed : 1;
        u32 unused11 : 21;
    };
};
CHECK_SIZE(BlenderBits, 4);

// A value the follow camera eases (0x50 bytes: its pitch, yaw and field of view are angles, 65536ths of a turn, its distance is
// units, the same layout with floats): every step works out how far it moves (its delta), toward its goal at its speed (per
// second) or by its input (its input speed times the input, per second), and where it stands between its low and high ends.
// The goal is its second range (it moves until it's within it), its range's ends blended by a share, or a value given: once
// its rate scale is above 0 (at its speed times it) or once a hold time is over. Angles go the shorter way round
struct AngleBlender
{
    BlenderBits bits;
    f32 rateScale;
    f32 input;
    s32 initial;
    f32 share;
    s32 current;
    s32 delta;
    s32 holdTicks;
    u32 holdStart;
    s32 speed;
    s32 inputSpeed;
    s32 previousSpeed;
    f32 goalShare;
    s32 low;
    s32 high;
    s32 secondLow;
    s32 secondHigh;
    s32 start;
    u32 unused48;
    u32 unused4C;

    // At a value (its ends there, speeds of 90 degrees a second), and set back to its initial value (of its bits only its hold
    // and its sine speed kept)
    static AngleBlender* Construct(AngleBlender* blender, const s32* value) RETAIL(FUN_00270f30);
    void Reset() RETAIL(FUN_00270fc8);
    // Its move kept within two ends (it stops at the end it would pass, when it's on the right side of it), and where it would be
    // between them
    void KeepWithin(const s32* low, const s32* high) RETAIL(FUN_00270bb8);
    // Its move toward two ends for a time (none within the dead zone of either; at once when it jumps): whether it moves
    u32 MoveToward(f32 seconds, const s32* toLow, const s32* toHigh, u32 jump) RETAIL_N32(FUN_00270d10);
    // Moved by a rate for a time (kept within its range when it's clamped), the push marked: whether it was (when it's enabled)
    u32 Push(f32 seconds, const s32* rate, u32 clamp) RETAIL_N32(FUN_00271070);
    // A step toward its goal (a value given when it's the goal): whether it moves
    u32 Step(TimeClock* clock, const s32* value, u32 clamp, u32 jump) RETAIL(FUN_00271128);
    // Its speed set to take a time (clock units, none: one step) to the nearer of two ends (the lower of the two ways), the old
    // one kept
    void TimeToward(const s32* ticks, const s32* low, const s32* high) RETAIL(FUN_00271430);
};
CHECK_SIZE(AngleBlender, 0x50);

// The follow camera's distance: an angle blender's layout with floats (no dead zone: it stops within 0.1 of its goal); its set
// back clears unused48 (and leaves the hold's start)
struct DistanceBlender
{
    BlenderBits bits;
    f32 rateScale;
    f32 input;
    f32 initial;
    f32 share;
    f32 current;
    f32 delta;
    s32 holdTicks;
    u32 holdStart;
    f32 speed;
    f32 inputSpeed;
    f32 previousSpeed;
    f32 goalShare;
    f32 low;
    f32 high;
    f32 secondLow;
    f32 secondHigh;
    f32 start;
    f32 unused48;
    u32 unused4C;

    void Reset() RETAIL(FUN_0027c2b0);
    // A step toward its goal (a value given when it's the goal): whether it moves
    u32 Step(f32 value, TimeClock* clock, u32 clamp, u32 jump) RETAIL_N32(FUN_00271520);
    // Its move kept within two ends (it stops at the end it would pass, when it's on the right side of it), and where it would be
    // between them (half way when they're the same)
    void KeepWithin(f32 toLow, f32 toHigh);
};
CHECK_SIZE(DistanceBlender, 0x50);

extern "C"
{
    // How near (65536ths of a turn) an angle blender gets to its goal before it stops (0.1 degrees, set at start-up)
    extern s32 g_CameraAngleDeadZone RETAIL(D_0030A9A8);
}
