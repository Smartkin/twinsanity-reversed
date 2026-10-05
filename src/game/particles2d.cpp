#include "game/particles2d.h"

#include "game/colour.h"
#include "game/memory.h"
#include "game/pools.h"
#include "game/renderer.h"
#include "game/shapes.h"
#include "platform/math.h"
#include "retail/libc.h"

namespace
{
// A radial particle's direction's scale (it's kept in 16 bit) and its speed per unit of that
constexpr f32 DirectionScale = 8192.0f;
constexpr f32 DirectionSpeed = 0x1.0p-15f;

// The 16 bit direction a radial particle keeps in its w
struct Direction
{
    s16 x;
    s16 y;
};

Direction& DirectionOf(Vector4& particle)
{
    return *reinterpret_cast<Direction*>(&particle.w);
}

// The walk the emitters' functions do with an iterator of their own on the stack
void First(SlotIterator& iterator)
{
    const SlotPool* pool = iterator.pool;
    iterator.slot = 0;
    iterator.passed = 0;
    if (pool->capacity - 1 <= 0 || pool->links[0] == PoolSlotUsed)
    {
        return;
    }

    do
    {
        iterator.slot = static_cast<s16>(iterator.slot + 1);
    } while (iterator.slot < pool->capacity - 1 && pool->links[iterator.slot] != PoolSlotUsed);
}

bool IsDone(const SlotIterator& iterator)
{
    return iterator.passed == iterator.pool->count;
}

void Next(SlotIterator& iterator)
{
    const SlotPool* pool = iterator.pool;
    if (!(iterator.passed < pool->count - 1))
    {
        iterator.passed = pool->count;
        return;
    }

    while (iterator.passed < pool->count)
    {
        iterator.slot = static_cast<s16>(iterator.slot + 1);
        if (pool->links[iterator.slot] == PoolSlotUsed)
        {
            iterator.passed = static_cast<s16>(iterator.passed + 1);
            return;
        }
    }
}

SlotIterator Walk(SlotPool* pool)
{
    SlotIterator iterator;
    iterator.vtable = nullptr;
    iterator.pool = pool;
    First(iterator);
    return iterator;
}

f32 SampleFloats(const FloatCurve* curve, f32 t)
{
    if (!(t < 1.0f))
    {
        return curve->values[curve->count - 1];
    }

    f32 place = static_cast<f32>(static_cast<s32>(curve->count - 1)) * t;
    s32 index = static_cast<s32>(place);
    f32 fraction = place - static_cast<f32>(index);
    f32 from = curve->values[index];
    return from + (curve->values[index + 1] - from) * fraction;
}

f32 ScreenAspect()
{
    return g_WidescreenTv != 0 ? WideAspect : NarrowAspect;
}

// A particle's matrix: its size (by the curve of its age), its turn, its place, then the widget's. The plain emitter widens its x by
// the TV's shape and goes without the curves it hasn't got, the radial one widens its y and needs them
void ParticleMatrix(const Emitter2D* emitter, bool radial, const Vector4* particle, const Matrix4x4* placed, f32 aspect,
                    Matrix4x4* matrix, f32* t)
{
    InitIdentityMatrix(matrix);
    Vector4 place;
    CopyVector4(&place, particle);
    Vector2 scale = {1.0f, 1.0f};
    *t = place.z / emitter->lifetime;
    if (radial || emitter->sizes != nullptr)
    {
        SampleVector2Curve(emitter->sizes, *t, &scale);
    }

    if (radial)
    {
        matrix->m[0][0] = emitter->size.x * scale.x;
        matrix->m[1][1] = emitter->size.y * aspect * scale.y;
    }
    else
    {
        matrix->m[0][0] = emitter->size.x * aspect * scale.x;
        matrix->m[1][1] = emitter->size.y * scale.y;
    }

    if (radial || emitter->turns != nullptr)
    {
        s32 angle;
        AngleFrom(&angle, SampleFloats(emitter->turns, *t), 0);
        Matrix4x4 turn;
        MatrixRotationZ(&turn, &angle);
        VuMultiplyMatrices(matrix, &turn, matrix);
    }

    place.z = 0.0f;
    place.w = 1.0f;
    *reinterpret_cast<Vector4*>(matrix->m[3]) = place;
    VuMultiplyMatrices(matrix, placed, matrix);
}

// Half the distance between the places, times the spread
Vector2 HalfSpread(const Emitter2D* emitter, const Vector2* start, const Vector2* end)
{
    Vector2 half;
    CopyVector2(&half, end);
    half.x = (half.x - start->x) * 0.5f * emitter->spread.x;
    half.y = (half.y - start->y) * 0.5f * emitter->spread.y;
    return half;
}
}

SlotPool* SlotPool::Construct(SlotPool* pool)
{
    pool->vtable = g_SlotPoolVTable;
    pool->growth = PoolGrowth;
    pool->firstFree = -1;
    pool->capacity = 0;
    pool->count = 0;
    pool->links = nullptr;
    pool->items = nullptr;
    return pool;
}

void SlotPool::Destroy(u32 flags)
{
    vtable = g_SlotPoolVTable;
    if (links != nullptr)
    {
        MemoryDeallocate_(links);
    }

    if (items != nullptr)
    {
        MemoryDeallocate_(items);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

s16 SlotPool::Allocate()
{
    if (!(count < capacity))
    {
        Grow();
        return Allocate();
    }

    s16 slot = firstFree;
    firstFree = links[slot];
    links[slot] = PoolSlotUsed;
    count = static_cast<s16>(count + 1);
    return slot;
}

void SlotPool::Free(s16 slot)
{
    links[slot] = firstFree;
    firstFree = slot;
    count = static_cast<s16>(count - 1);
}

void SlotPool::Reserve(u32 size)
{
    if (static_cast<u32>(static_cast<s32>(capacity)) < size)
    {
        s16 kept = growth;
        growth = static_cast<s16>(size - static_cast<u16>(capacity));
        Grow();
        growth = kept;
    }
}

void SlotPool::Clear()
{
    if (capacity <= 0 || count == 0)
    {
        return;
    }

    s32 slot = 0;
    for (; slot < capacity - 1; slot++)
    {
        links[slot] = static_cast<s16>(slot + 1);
    }

    links[slot] = PoolFreeListEnd;
    firstFree = 0;
    count = 0;
}

void SlotPool::Grow()
{
    if ((capacity & 1) != 0)
    {
        capacity = static_cast<s16>(capacity + 1);
    }

    if ((growth & 1) != 0)
    {
        growth = static_cast<s16>(growth + 1);
    }

    s32 grown = capacity + growth;
    auto* newItems = static_cast<Vector4*>(MemoryAllocate2(grown * sizeof(Vector4)));
    auto* newLinks = static_cast<s16*>(MemoryAllocate2((capacity + growth) * sizeof(s16)));
    if (capacity != 0)
    {
        Vector4* oldItems = items;
        items = newItems;
        for (s32 slot = 0; slot < capacity; slot++)
        {
            if (links[slot] == PoolSlotUsed)
            {
                newItems[slot] = oldItems[slot];
            }
        }

        // Every old slot used: it only grows when it's full (or reserves room before any is)
        RetailLibc::MemorySet(newLinks, -1, capacity * sizeof(s16));
        if (oldItems != nullptr)
        {
            MemoryDeallocate_(oldItems);
        }

        if (links != nullptr)
        {
            MemoryDeallocate_(links);
        }
    }

    s32 end = capacity + growth;
    for (s32 slot = capacity; slot < end; slot++)
    {
        newLinks[slot] = static_cast<s16>(slot + 1);
    }

    newLinks[end - 1] = PoolFreeListEnd;
    s16 oldCapacity = capacity;
    links = newLinks;
    items = newItems;
    capacity = static_cast<s16>(oldCapacity + growth);
    firstFree = oldCapacity;
}

void SlotIterator::Destroy(u32 flags)
{
    vtable = g_SlotIteratorBaseVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SlotIterator::BaseDestroy(u32 flags)
{
    Destroy(flags);
}

void SlotIterator::First()
{
    ::First(*this);
}

u32 SlotIterator::IsDone()
{
    return ::IsDone(*this) ? 1 : 0;
}

Vector4* SlotIterator::Current()
{
    return &pool->items[slot];
}

void SlotIterator::Next()
{
    ::Next(*this);
}

SlotIterator* SlotIterator::Assign(const SlotIterator* other)
{
    slot = other->slot;
    passed = other->passed;
    pool = other->pool;
    return this;
}

s32 SlotIterator::Slot()
{
    return slot;
}

s32 SlotIterator::Count()
{
    return pool->count;
}

void SlotIterator::OtherDestroy(u32 flags)
{
    vtable = g_OtherSlotIteratorBaseVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SlotIterator::OtherBaseDestroy(u32 flags)
{
    OtherDestroy(flags);
}

void SlotIterator::OtherFirst()
{
    ::First(*this);
}

u32 SlotIterator::OtherIsDone()
{
    return ::IsDone(*this) ? 1 : 0;
}

Vector4* SlotIterator::OtherCurrent()
{
    return &pool->items[slot];
}

void SlotIterator::OtherNext()
{
    ::Next(*this);
}

SlotIterator* SlotIterator::OtherAssign(const SlotIterator* other)
{
    return Assign(other);
}

s32 SlotIterator::OtherSlot()
{
    return slot;
}

s32 SlotIterator::OtherCount()
{
    return pool->count;
}

Emitter2D* Emitter2D::Construct(Emitter2D* emitter)
{
    emitter->vtable = g_Emitter2DVTable;
    SlotPool::Construct(&emitter->particles);
    emitter->tries = 1;
    emitter->sprite = nullptr;
    emitter->lifetime = 0.0f;
    emitter->chance = 0.0f;
    emitter->colours = nullptr;
    emitter->sizes = nullptr;
    emitter->offset = {0.0f, 0.0f};
    return emitter;
}

void Emitter2D::Destroy(u32 flags)
{
    vtable = g_Emitter2DVTable;
    particles.Destroy(DestroyOnly);
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Emitter2D::SetSprite(Shape2D* newSprite, u32 room)
{
    sprite = newSprite;
    particles.Reserve(room);
}

void Emitter2D::Spawn(const Vector2* start, const Vector2* end)
{
    if (!(GetRandFloat() < chance))
    {
        return;
    }

    f32 x = RandomSigned();
    f32 y = RandomSigned();
    Vector2 half = HalfSpread(this, start, end);
    x = x * half.x;
    y = y * half.y;
    s16 slot = particles.Allocate();
    Vector4& particle = particles.items[slot];
    particle.x = x + offset.x;
    particle.y = y + offset.y;
    particle.z = 0.0f;
}

void Emitter2D::Step(f32 seconds)
{
    if (particles.count == 0)
    {
        return;
    }

    SlotIterator iterator = Walk(&particles);
    while (!IsDone(iterator))
    {
        Vector4* particle = &particles.items[iterator.slot];
        particle->z = particle->z + seconds;
        if (lifetime < particle->z)
        {
            particles.Free(iterator.slot);
            iterator.passed = static_cast<s16>(iterator.passed - 1);
        }

        Next(iterator);
    }
}

void Emitter2D::Draw(const Matrix4x4* placed)
{
    if (particles.count == 0)
    {
        return;
    }

    f32 aspect = ScreenAspect();
    SlotIterator iterator = Walk(&particles);
    while (!IsDone(iterator))
    {
        Matrix4x4 matrix;
        f32 t;
        ParticleMatrix(this, false, &particles.items[iterator.slot], placed, aspect, &matrix, &t);
        if (colours != nullptr)
        {
            Vector4 tint;
            SampleVector4Curve(colours, t, &tint);
            u32 colour;
            ColourSet(&colour, tint.x, tint.y, tint.z, tint.w);
            CallVirtual<void>(sprite, sprite->vtable, Shape2D::DrawPlacedColouredSlot, &matrix, colour);
        }
        else
        {
            CallVirtual<void>(sprite, sprite->vtable, Shape2D::DrawPlacedSlot, &matrix);
        }

        Next(iterator);
    }
}

void RadialEmitter2D::Destroy(u32 flags)
{
    Emitter2D::Destroy(flags);
}

void RadialEmitter2D::Spawn(const Vector2* start, const Vector2* end)
{
    if (!(GetRandFloat() < chance))
    {
        return;
    }

    f32 sinCos[4];
    Platform::Math::SinCos(GetRandFloat() * TwoPi, 0.0f, sinCos);
    Vector2 half = HalfSpread(this, start, end);
    f32 radius = __builtin_sqrtf(half.x * half.x + half.y * half.y);
    f32 directionX = sinCos[1];
    f32 directionY = sinCos[0];
    s16 slot = particles.Allocate();
    Vector4& particle = particles.items[slot];
    particle.x = directionX * radius + offset.x;
    particle.y = directionY * radius + offset.y;
    particle.z = 0.0f;
    Direction& direction = DirectionOf(particle);
    direction.x = static_cast<s16>(static_cast<s32>(directionX * DirectionScale));
    direction.y = static_cast<s16>(static_cast<s32>(directionY * DirectionScale));
}

void RadialEmitter2D::Step(f32 seconds)
{
    if (particles.count == 0)
    {
        return;
    }

    SlotIterator iterator = Walk(&particles);
    while (!IsDone(iterator))
    {
        Vector4* particle = &particles.items[iterator.slot];
        particle->z = particle->z + seconds;
        if (lifetime < particle->z)
        {
            particles.Free(iterator.slot);
            iterator.passed = static_cast<s16>(iterator.passed - 1);
        }
        else
        {
            const Direction& direction = DirectionOf(*particle);
            particle->x = particle->x + static_cast<f32>(direction.x) * DirectionSpeed * seconds;
            particle->y = particle->y + static_cast<f32>(direction.y) * DirectionSpeed * seconds;
        }

        Next(iterator);
    }
}

void RadialEmitter2D::Draw(const Matrix4x4* placed)
{
    if (particles.count == 0)
    {
        return;
    }

    f32 aspect = ScreenAspect();
    SlotIterator iterator = Walk(&particles);
    while (!IsDone(iterator))
    {
        Matrix4x4 matrix;
        f32 t;
        ParticleMatrix(this, true, &particles.items[iterator.slot], placed, aspect, &matrix, &t);
        u32 drawColour;
        ColourSet(&drawColour, colour.x, colour.y, colour.z, colour.w);
        CallVirtual<void>(sprite, sprite->vtable, Shape2D::DrawPlacedColouredSlot, &matrix, drawColour);
        Next(iterator);
    }
}

extern "C" s32 Emitter2DStep(Emitter2D* emitter, f32 seconds, const Vector2* start, const Vector2* end)
{
    CallVirtual<void>(emitter, emitter->vtable, Emitter2D::StepSlot, seconds);
    for (u32 tries = emitter->tries; tries != 0; tries--)
    {
        if (emitter->particles.capacity != emitter->particles.count)
        {
            CallVirtual<void>(emitter, emitter->vtable, Emitter2D::SpawnSlot, start, end);
        }
    }

    return emitter->particles.count;
}

EABI_EXPORT(FUN_001a7528, &Emitter2D::Step);
EABI_EXPORT(FUN_001a7c20, &RadialEmitter2D::Step);
EABI_EXPORT(FUN_001abf08, Emitter2DStep);
