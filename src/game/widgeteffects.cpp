#include "game/widgeteffects.h"

#include "game/memory.h"
#include "game/renderer.h"
#include "platform/math.h"

namespace
{
// The rock's widest turn (a sixteenth of a turn, radians), the spin's four turns, the slide's distance (4:3 and 16:9)
constexpr f32 RockTurn = TwoPi / 16.0f;
constexpr f32 FourTurns = 4.0f * TwoPi;
constexpr f32 SlideNarrow = 0x1.70A3D8p-2f;
constexpr f32 SlideWide = 0x1.C28F5Cp-2f;
// The sparkle's emitter once reset
constexpr u32 SparkleTries = 4;
constexpr f32 SparkleLifetime = 0.5f;
constexpr f32 SparkleChance = 0x1.E66666p+0f;

// The cycle moved on by the seconds (its fraction from where the step started): past its end the time starts over (or keeps what's
// past the end) and a repeat is used up; with none left the next effect plays
WidgetEffect* StepCycle(WidgetEffect* effect, f32 seconds, bool keepsOverrun)
{
    f32 time = effect->time;
    effect->time = time + seconds;
    effect->fraction = time / effect->duration;
    if (1.0f <= effect->fraction)
    {
        effect->time = keepsOverrun ? effect->time - effect->duration : 0.0f;
        effect->repeats = effect->repeats - 1;
        if (effect->repeats <= 0)
        {
            return effect->next;
        }
    }

    return effect;
}

void DestroyEffect(WidgetEffect* effect, u32 flags)
{
    effect->vtable = g_WidgetEffectVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(effect);
    }
}

f32 SlideDistance()
{
    return g_WidescreenTv != 0 ? SlideWide : SlideNarrow;
}

void Turned(s32 angle, Matrix4x4* matrix)
{
    Matrix4x4 turn;
    MatrixRotationZ(&turn, &angle);
    VuMultiplyMatrices(&turn, matrix, matrix);
}
}

void WidgetEffect::Destroy(u32 flags)
{
    DestroyEffect(this, flags);
}

void WidgetEffect::Change(Matrix4x4*)
{
}

void WidgetEffect::DrawPlaced(const Matrix4x4*)
{
}

void WidgetEffect::Reset()
{
    repeats = 0;
    time = 0.0f;
    fraction = 0.0f;
}

void CurveScaleEffect::Destroy(u32 flags)
{
    DestroyEffect(this, flags);
}

WidgetEffect* CurveScaleEffect::Step(f32 seconds, const Vector2*, const Vector2*, u32 shown)
{
    if (shown == 0)
    {
        return this;
    }

    return StepCycle(this, seconds, false);
}

void CurveScaleEffect::Change(Matrix4x4* matrix)
{
    Vector2 scale;
    SampleVector2Curve(&curve, fraction, &scale);
    Matrix4x4 scaling;
    InitIdentityMatrix(&scaling);
    scaling.m[0][0] = scale.x;
    scaling.m[1][1] = scale.y;
    VuMultiplyMatrices(&scaling, matrix, matrix);
}

void SparkleEffect::Destroy(u32 flags)
{
    emitter.Emitter2D::Destroy(DestroyOnly);
    DestroyEffect(this, flags);
}

WidgetEffect* SparkleEffect::Step(f32 seconds, const Vector2* start, const Vector2* end, u32)
{
    WidgetEffect* after = StepCycle(this, seconds, false);
    s32 particles = Emitter2DStep(&emitter, seconds, start, end);
    if (after != nullptr)
    {
        return after;
    }

    // Over: no more particles, and done once they're gone
    emitter.tries = 0;
    return particles != 0 ? this : nullptr;
}

void SparkleEffect::DrawPlaced(const Matrix4x4* placed)
{
    emitter.RadialEmitter2D::Draw(placed);
}

void SparkleEffect::Reset()
{
    emitter.particles.Clear();
    emitter.tries = SparkleTries;
    emitter.lifetime = SparkleLifetime;
    emitter.chance = SparkleChance;
    repeats = 0;
    time = 0.0f;
    fraction = 0.0f;
}

void RockEffect::Destroy(u32 flags)
{
    DestroyEffect(this, flags);
}

WidgetEffect* RockEffect::Step(f32 seconds, const Vector2*, const Vector2*, u32)
{
    return StepCycle(this, seconds, true);
}

void RockEffect::Change(Matrix4x4* matrix)
{
    f32 sinCos[4];
    Platform::Math::SinCos(fraction * TwoPi, 0.0f, sinCos);
    s32 angle;
    AngleFrom(&angle, sinCos[0] * RockTurn, 0);
    Turned(angle, matrix);
}

void SlideEffect::Destroy(u32 flags)
{
    DestroyEffect(this, flags);
}

WidgetEffect* SlideEffect::Step(f32 seconds, const Vector2*, const Vector2*, u32 shown)
{
    if (shown == 0)
    {
        return this;
    }

    return StepCycle(this, seconds, false);
}

void SlideEffect::Change(Matrix4x4* matrix)
{
    matrix->m[3][0] = matrix->m[3][0] + SlideDistance() * fraction;
}

void SpinEffect::Destroy(u32 flags)
{
    DestroyEffect(this, flags);
}

WidgetEffect* SpinEffect::Step(f32 seconds, const Vector2*, const Vector2*, u32)
{
    return StepCycle(this, seconds, false);
}

void SpinEffect::Change(Matrix4x4* matrix)
{
    s32 angle;
    AngleFrom(&angle, fraction * FourTurns, 0);
    Matrix4x4 turn;
    MatrixRotationZ(&turn, &angle);
    matrix->m[3][0] = matrix->m[3][0] + SlideDistance();
    VuMultiplyMatrices(&turn, matrix, matrix);
}

EABI_EXPORT(FUN_00178f50, &CurveScaleEffect::Step);
EABI_EXPORT(FUN_00179080, &SparkleEffect::Step);
EABI_EXPORT(FUN_00179280, &RockEffect::Step);
EABI_EXPORT(FUN_00179368, &SlideEffect::Step);
EABI_EXPORT(FUN_00179410, &SpinEffect::Step);
