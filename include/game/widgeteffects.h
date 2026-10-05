#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/particles2d.h"

// What a widget plays (D_002F56D0 and its subclasses, the vtable after 0x14 bytes: 1 the destructor, 2 a step by the seconds between
// the widget's two places, told whether the widget is shown, that returns what plays on (itself, the next or none), 3 a change to
// the matrix the widget draws with, 4 drawn placed by the widget's turn and middle, 5 started again): a cycle over its duration, how
// far it is (the fraction its last step started at) and how many times it's still to go round before the next plays
class WidgetEffect
{
public:
    f32 duration;
    f32 time;
    f32 fraction;
    s32 repeats;
    WidgetEffect* next;
    const GccVTableEntry* vtable;

    void Destroy(u32 flags) RETAIL(FUN_00178eb8);
    void Change(Matrix4x4* matrix) RETAIL(FUN_00178ee8);
    void DrawPlaced(const Matrix4x4* placed) RETAIL(FUN_00178ef0);
    void Reset() RETAIL(FUN_00178ef8);
};
CHECK_SIZE(WidgetEffect, 0x18);

// The widget scaled by a curve of the cycle's fraction, going round while it's shown (D_002F5698)
class CurveScaleEffect : public WidgetEffect
{
public:
    Vector2Curve curve;

    void Destroy(u32 flags) RETAIL(FUN_00178f20);
    WidgetEffect* Step(f32 seconds, const Vector2* start, const Vector2* end, u32 shown) RETAIL_N32(FUN_00178f50);
    void Change(Matrix4x4* matrix) RETAIL(FUN_00178fc0);
};
CHECK_SIZE(CurveScaleEffect, 0x20);

// Particles thrown out between the widget's places (a radial emitter's) while the cycle goes round, and until they're gone after it
// (D_002F5660)
class SparkleEffect : public WidgetEffect
{
public:
    u8 unused18[8];
    RadialEmitter2D emitter;

    void Destroy(u32 flags) RETAIL(FUN_00179018);
    WidgetEffect* Step(f32 seconds, const Vector2* start, const Vector2* end, u32 shown) RETAIL_N32(FUN_00179080);
    void DrawPlaced(const Matrix4x4* placed) RETAIL(FUN_00179190);
    // No particles, four tries a step that make one each (their chance is above 1), living half a second
    void Reset() RETAIL(FUN_00179128);
};
CHECK_SIZE(SparkleEffect, 0x80);

// The widget rocked to and fro by up to a sixteenth of a turn on a sine of the cycle, going round whether it's shown or not and
// keeping the time it went past the end (D_002F5628)
class RockEffect : public WidgetEffect
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001791b0);
    WidgetEffect* Step(f32 seconds, const Vector2* start, const Vector2* end, u32 shown) RETAIL_N32(FUN_00179280);
    void Change(Matrix4x4* matrix) RETAIL(FUN_001791e8);
};

// The widget slid right over the cycle (0.36 of the screen, 0.44 on a 16:9 TV), going round while it's shown (D_002F55F0)
class SlideEffect : public WidgetEffect
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001792e8);
    WidgetEffect* Step(f32 seconds, const Vector2* start, const Vector2* end, u32 shown) RETAIL_N32(FUN_00179368);
    void Change(Matrix4x4* matrix) RETAIL(FUN_001793d0);
};

// The widget spun round four times over the cycle, moved right as far as the slide goes (D_002F55B8)
class SpinEffect : public WidgetEffect
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00179320);
    WidgetEffect* Step(f32 seconds, const Vector2* start, const Vector2* end, u32 shown) RETAIL_N32(FUN_00179410);
    void Change(Matrix4x4* matrix) RETAIL(FUN_00179468);
};

extern "C"
{
    extern const GccVTableEntry g_WidgetEffectVTable[] RETAIL(D_002F56D0);
}
