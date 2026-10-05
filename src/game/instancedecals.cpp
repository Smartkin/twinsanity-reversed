#include "game/animation.h"
#include "game/collision.h"
#include "game/decals.h"
#include "game/instanceparticles.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/place.h"

// The decals instances leave on the ground: a footprint at an exit point, and the marks of an impact on a surface (with the
// surface's particles)

extern "C"
{
    void SpawnSurfaceImpactParticles(InstanceContext* instance, const CollisionSurface* surface, const Vector4* point)
        RETAIL(SpawnSurfaceImpactParticles);
    void AddInstanceDecal(u32 kind, InstanceContext* instance, ExitPointAnimation* at, const Vector4* offset)
        RETAIL(AddInstanceDecal);
}

namespace
{
// An emitter of a surface's particles starts with a value
constexpr u32 EmitterValue = 1;
// A footprint's decal: its variant, type, flags and key
constexpr s32 FootprintVariant = 1;
constexpr s32 FootprintType = 0;
constexpr s32 FootprintFlags = 1;
constexpr s32 FootprintKey = 1;
}

extern "C"
{
    // The marks of an impact on a surface around a point: six decals scattered up to 0.4 across the ground and, when the surface
    // has an impact particle system, an emitter of it at each of six points 0.9 around the point a sixth of a turn apart, 0.2
    // below it
    void SpawnSurfaceImpactParticles(InstanceContext* instance, const CollisionSurface* surface, const Vector4* point)
    {
        constexpr u32 Marks = 6;
        constexpr f32 Spread = Rounded(0.4);
        constexpr f32 Around = 0.9f;
        constexpr f32 Below = Rounded(0.2);
        constexpr f32 TurnDegrees = 60.0f;
        u32 system = GetSurfaceParticle(surface, ContactImpact);
        Vector4 offset = {Around, 0.0f, 0.0f, 1.0f};
        for (u32 mark = 0; mark < Marks; mark++)
        {
            Matrix4x4 frame;
            InitIdentityMatrix(&frame);
            Vector4* place = RowOf(&frame, 3);
            *place = *point;
            Vector4 jitter = {0.0f, 0.0f, 0.0f, 1.0f};
            JitterVector(Spread, 0.0f, 1.0f, 1.0f, &jitter);
            place->x = place->x + jitter.x;
            place->y = place->y + jitter.y;
            place->z = place->z + jitter.z;
            AddDecalFromDescriptor(&frame, instance->chunk);
            if (system == NoSurfaceEffect)
            {
                continue;
            }

            Vector4 at = offset;
            at.x = offset.x + point->x;
            at.y = offset.y + point->y - Below;
            at.z = offset.z + point->z;
            StartEmitterKeepingTranslation(instance, static_cast<s32>(system), EmitterValue, &at);
            s32 angle;
            AngleFrom(&angle, TurnDegrees, AngleDegrees);
            TurnAboutAxis(&offset, &g_YAxis, &angle, 1);
        }
    }

    // A footprint of an instance at an exit point (the kind isn't read): at the point's place moved by an offset turned with the
    // instance, at the instance's own height (0.09 above it), facing the point's z axis over the ground (the instance's when the
    // two are about 45 degrees apart or more), in the instance's chunk
    void AddInstanceDecal(u32, InstanceContext* instance, ExitPointAnimation* at, const Vector4* offset)
    {
        constexpr f32 SameWay = 0.7f;
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        const f32 (*m)[4] = place->matrix.m;
        Vector4 position = *RowOf(&at->matrix, 3);
        if (offset != nullptr)
        {
            position.x = position.x + (m[0][0] * offset->x + m[1][0] * offset->y + m[2][0] * offset->z);
            position.z = position.z + (m[0][2] * offset->x + m[1][2] * offset->y + m[2][2] * offset->z);
        }

        position.y = m[3][1] + FootprintLift;
        const Vector4& forward = *RowOf(&place->matrix, 2);
        Vector4 direction = *RowOf(&at->matrix, 2);
        if (direction.x * forward.x + direction.y * forward.y + direction.z * forward.z < SameWay)
        {
            direction = forward;
        }

        direction.y = 0.0f;
        f32 inverse = InverseLength(&direction, LengthEpsilon);
        DecalDescriptor decal;
        decal.place = position;
        decal.normal = {0.0f, 1.0f, 0.0f, 1.0f};
        decal.direction = {direction.x * inverse, direction.y * inverse, direction.z * inverse, 1.0f};
        decal.variant = FootprintVariant;
        decal.type = FootprintType;
        decal.flags = FootprintFlags;
        decal.key = FootprintKey;
        decal.chunk = instance->chunk;
        AddDecal(&g_DecalData, &decal);
    }
}
