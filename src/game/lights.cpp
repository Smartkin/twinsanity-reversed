#include "game/lights.h"

#include "game/chunkdata.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/scenery.h"
#include "game/stream.h"
#include "game/view.h"

namespace
{
// The box around the light's position, the extent each way
void BoxAround(Light* light, f32 scale)
{
    f32 extent = light->intensity * scale;
    f32 x = light->position.x;
    f32 y = light->position.y;
    f32 z = light->position.z;
    light->boundsMax.w = 1.0f;
    light->boundsMin.w = 1.0f;
    light->boundsMax.z = z + extent;
    light->boundsMax.x = x + extent;
    light->boundsMax.y = y + extent;
    light->boundsMin.x = x - extent;
    light->boundsMin.y = y - extent;
    light->boundsMin.z = z - extent;
}

// The way from the place to the light (its own position, or the one standing in) times the fall off, which it returns
f32 FallOff(const Light* light, const Vector4* at, Vector4* direction, const Vector4* position)
{
    const Vector4* from = position != nullptr ? position : &light->position;
    Vector4 towards;
    towards.x = from->x - at->x;
    towards.y = from->y - at->y;
    towards.z = from->z - at->z;
    towards.w = 1.0f;
    f32 constant = g_LightingConstants[FalloffConstant];
    f32 falloff = constant / (towards.x * towards.x + towards.y * towards.y + towards.z * towards.z + constant);
    towards.x = towards.x * falloff;
    towards.y = towards.y * falloff;
    towards.z = towards.z * falloff;
    *direction = towards;
    return falloff;
}

// The intensity attenuated: times the fall off as many times as the power (none for 0 or less)
f32 Attenuated(f32 intensity, s32 power, f32 falloff)
{
    for (; power > 0; power--)
    {
        intensity = intensity * falloff;
    }

    return intensity;
}

// The kind's values taken, then the base's through the vtable
template <typename T>
T* AssignLight(T* light, const T* other)
{
    CallVirtual<Light*>(static_cast<Light*>(light), light->vtable, Light::SlotAssignBase, static_cast<const Light*>(other));
    return light;
}
}

void Light::Destroy(u32 destroyFlags)
{
    vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Light::Enable()
{
    header.enabled = 1;
}

void Light::Disable()
{
    header.enabled = 0;
}

void Light::SetOwnKind()
{
    header.kind = KindNone;
}

void Light::SetKind(u32 kind)
{
    header.kind = kind;
}

u32 Light::GetKind() const
{
    return header.kind;
}

Light* Light::AssignBase(const Light* other)
{
    header = other->header;
    intensity = other->intensity;
    colour = other->colour;
    position = other->position;
    boundsMin = other->boundsMin;
    boundsMax = other->boundsMax;
    return this;
}

void Light::Read(Stream* stream)
{
    stream->Read(&header, sizeof(header), 1);
    stream->ReadF32(&intensity);
    stream->Read(&colour, sizeof(Vector4), 1);
    stream->Read(&position, sizeof(Vector4), 1);
    stream->Read(&boundsMin, sizeof(Vector4), 1);
    stream->Read(&boundsMax, sizeof(Vector4), 1);
}

void Light::Follow()
{
}

void AmbientLight::Destroy(u32 destroyFlags)
{
    vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void AmbientLight::SetOwnKind()
{
    header.kind = KindAmbient;
}

void AmbientLight::LightAt(const Vector4*, Vector4*, f32*, const Vector4*, const Vector4*) const
{
}

void AmbientLight::ComputeBounds()
{
    constexpr f32 Scale = 100000.0f;
    BoxAround(this, Scale);
}

AmbientLight* AmbientLight::Assign(const AmbientLight* other)
{
    return AssignLight(this, other);
}

void DirectionalLight::Destroy(u32 destroyFlags)
{
    vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void DirectionalLight::SetOwnKind()
{
    header.kind = KindDirectional;
}

void DirectionalLight::LightAt(const Vector4*, Vector4* out, f32* strength, const Vector4*, const Vector4* towards) const
{
    *out = towards != nullptr ? *towards : direction;
    *strength = intensity;
}

void DirectionalLight::ComputeBounds()
{
    constexpr f32 Scale = Rounded(99999.99);
    BoxAround(this, Scale);
}

void DirectionalLight::Read(Stream* stream)
{
    Light::Read(stream);
    stream->Read(&direction, sizeof(Vector4), 1);
    stream->ReadU16(reinterpret_cast<u16*>(&leftover));
}

DirectionalLight* DirectionalLight::Assign(const DirectionalLight* other)
{
    direction = other->direction;
    leftover = other->leftover;
    return AssignLight(this, other);
}

void PointLight::Destroy(u32 destroyFlags)
{
    vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PointLight::SetOwnKind()
{
    header.kind = KindPoint;
}

void PointLight::LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4*) const
{
    f32 falloff = FallOff(this, at, direction, position);
    *strength = Attenuated(intensity, attenuationPower, falloff);
}

void PointLight::ComputeBounds()
{
    constexpr f32 Scale = 100.0f;
    BoxAround(this, Scale);
}

void PointLight::Read(Stream* stream)
{
    Light::Read(stream);
    stream->ReadU16(reinterpret_cast<u16*>(&attenuationPower));
}

PointLight* PointLight::Assign(const PointLight* other)
{
    attenuationPower = other->attenuationPower;
    return AssignLight(this, other);
}

void SpotLight::Destroy(u32 destroyFlags)
{
    vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SpotLight::SetOwnKind()
{
    header.kind = KindSpot;
}

namespace
{
// A spot light's strength at a point from a position along an axis
void SpotLightFrom(const SpotLight* light, const Vector4* at, Vector4* out, f32* strength, const Vector4* position,
                   const Vector4* axis)
{
    constexpr u32 HighestBit = 0x80;
    f32 falloff = FallOff(light, at, out, position);
    f32 lit = Attenuated(light->intensity, light->attenuationPower, falloff);
    // How far into the cone: the (attenuated, not unit long) way to the light against its direction
    f32 cosine = out->x * -axis->x + out->y * -axis->y + out->z * -axis->z;
    if (cosine < light->outerConeCosine)
    {
        *strength = 0.0f;
        return;
    }

    if (cosine < light->innerConeCosine)
    {
        f32 share = (cosine - light->outerConeCosine) / (light->innerConeCosine - light->outerConeCosine);
        if (share < 0.0f)
        {
            share = 0.0f;
        }

        if (1.0f < share)
        {
            share = 1.0f;
        }

        lit = lit * share;
    }

    // The cosine to the power of the exponent's low byte, squaring from its top bit
    s32 exponent = static_cast<s16>(light->spotExponent);
    f32 power = 1.0f;
    for (s32 bit = HighestBit; bit != 0; bit >>= 1)
    {
        if ((exponent & bit) != 0)
        {
            f32 times = power * cosine;
            power = power * times;
        }
        else
        {
            power = power * power;
        }
    }

    *strength = lit * power;
}
}

void SpotLight::LightAt(const Vector4* at, Vector4* out, f32* strength, const Vector4* position, const Vector4*) const
{
    SpotLightFrom(this, at, out, strength, position, &direction);
}

void SpotLight::Read(Stream* stream)
{
    Light::Read(stream);
    stream->Read(&direction, sizeof(Vector4), 1);
    stream->ReadF32(&innerConeCosine);
    stream->ReadF32(&outerConeCosine);
    reinterpret_cast<TaggedValue*>(&coneAngle)->Read(stream);
    reinterpret_cast<TaggedValue*>(&falloffAngle)->Read(stream);
    stream->ReadU16(reinterpret_cast<u16*>(&attenuationPower));
    stream->ReadU16(&spotExponent);
}

SpotLight* SpotLight::Assign(const SpotLight* other)
{
    direction = other->direction;
    coneAngle = other->coneAngle;
    falloffAngle = other->falloffAngle;
    attenuationPower = other->attenuationPower;
    spotExponent = other->spotExponent;
    innerConeCosine = other->innerConeCosine;
    outerConeCosine = other->outerConeCosine;
    return AssignLight(this, other);
}

void InitLightingConstants(u32 initialise, u32 priority)
{
    static const f32 Values[LightingConstantCount] = {
        0.0f, 0.0f, 0.0f, 25.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 0.5f,
        0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f,
    };
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    for (u32 index = 0; index < LightingConstantCount; index++)
    {
        g_LightingConstants[index] = Values[index];
    }
}

ChunkLights* ConstructChunkLights(ChunkLights* lights, ChunkData* chunk)
{
    lights->chunk = chunk;
    lights->ambientLights = nullptr;
    lights->directionalLights = nullptr;
    lights->pointLights = nullptr;
    lights->spotLights = nullptr;
    lights->lightCount = 0;
    lights->ambientCount = 0;
    lights->directionalCount = 0;
    lights->pointCount = 0;
    lights->spotCount = 0;
    for (Light*& extra : lights->extraLights)
    {
        extra = nullptr;
    }

    for (Vector4& direction : lights->directions)
    {
        direction = g_DefaultBox.min;
        direction.w = 1.0f;
    }

    return lights;
}

namespace
{
// A list's lights destroyed last to first through their vtables, then the list freed
template <typename T>
void DestroyList(T* list)
{
    if (list == nullptr)
    {
        return;
    }

    T* light = list + ArrayCount(list);
    while (light != list)
    {
        light--;
        CallVirtual<void>(static_cast<Light*>(light), light->vtable, Light::SlotDestroy, 0u);
    }

    DeleteArray(list);
}

// A list of lights of a kind, made enabled and of its kind (none for a count of 0)
template <typename T>
T* MakeList(s32 count, const GccVTableEntry* vtable)
{
    if (count == 0)
    {
        return nullptr;
    }

    T* list = NewArray<T>(count);
    for (s32 index = 0; index < count; index++)
    {
        T* light = &list[index];
        light->header.value = 0;
        light->header.kind = Light::KindNone;
        light->vtable = vtable;
        light->header.enabled = 1;
        light->SetOwnKind();
    }

    return list;
}
}

void DestroyLights(ChunkLights* lights, u32 destroyFlags)
{
    DestroyList(lights->ambientLights);
    DestroyList(lights->directionalLights);
    DestroyList(lights->pointLights);
    DestroyList(lights->spotLights);
    for (Light* extra : lights->extraLights)
    {
        DestroyList(reinterpret_cast<AmbientLight*>(extra));
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(lights);
    }
}

void MakeChunkLights(ChunkLights* lights)
{
    lights->ambientLights = MakeList<AmbientLight>(lights->ambientCount, g_AmbientLightVTable);
    lights->directionalLights = MakeList<DirectionalLight>(lights->directionalCount, g_DirectionalLightVTable);
    lights->pointLights = MakeList<PointLight>(lights->pointCount, g_PointLightVTable);
    lights->spotLights = MakeList<SpotLight>(lights->spotCount, g_SpotLightVTable);
}

void ReadSceneryLights(ChunkLights* lights, Stream* stream)
{
    stream->Read(lights->references, sizeof(lights->references), 1);
    stream->ReadS32(&lights->lightCount);
    stream->ReadS32(&lights->ambientCount);
    stream->ReadS32(&lights->directionalCount);
    stream->ReadS32(&lights->pointCount);
    stream->ReadS32(&lights->spotCount);
    MakeChunkLights(lights);
    for (u32 index = 0; index < static_cast<u32>(lights->ambientCount); index++)
    {
        lights->ambientLights[index].ReadVirtual(stream);
    }

    for (u32 index = 0; index < static_cast<u32>(lights->directionalCount); index++)
    {
        lights->directionalLights[index].ReadVirtual(stream);
    }

    for (u32 index = 0; index < static_cast<u32>(lights->pointCount); index++)
    {
        lights->pointLights[index].ReadVirtual(stream);
    }

    for (u32 index = 0; index < static_cast<u32>(lights->spotCount); index++)
    {
        lights->spotLights[index].ReadVirtual(stream);
    }
}

void ClearGatheredLights(ChunkLights* lights)
{
    for (u32 index = 0; index < ChunkLights::Strongest; index++)
    {
        lights->strengths[index] = -1.0f;
        lights->colours[index] = g_DefaultBox.min;
    }

    lights->ambient = g_DefaultBox.min;
}

void FinishGatheredLights(ChunkLights* lights, const Matrix4x4* chunkMatrix, u32 ownLight)
{
    // The object's own light keeps its slot as it is but for its colour, halved by its strength
    u32 gathered = ownLight == 0 ? ChunkLights::Strongest : ChunkLights::OwnSlot;
    for (u32 index = 0; index < gathered && 0.0f <= lights->strengths[index]; index++)
    {
        VuRotateVector(chunkMatrix, &lights->directions[index], &lights->directions[index]);
        f32 half = lights->strengths[index] * 0.5f;
        lights->colours[index].x = lights->colours[index].x * half;
        lights->colours[index].z = lights->colours[index].z * half;
        lights->colours[index].y = lights->colours[index].y * half;
    }

    if (ownLight != 0)
    {
        f32 half = lights->strengths[ChunkLights::OwnSlot] * 0.5f;
        lights->colours[ChunkLights::OwnSlot].x = lights->colours[ChunkLights::OwnSlot].x * half;
        lights->colours[ChunkLights::OwnSlot].y = lights->colours[ChunkLights::OwnSlot].y * half;
        lights->colours[ChunkLights::OwnSlot].z = lights->colours[ChunkLights::OwnSlot].z * half;
    }

    lights->ambient.x = lights->ambient.x * 0.5f;
    lights->ambient.y = lights->ambient.y * 0.5f;
    lights->ambient.z = lights->ambient.z * 0.5f;
}

namespace
{
// A light's colour times a strength added to the ambient light, the product left in the scratch (the retail stack's)
void AddAmbient(ChunkLights* lights, const Light* light, f32 strength, Vector4* scratch)
{
    scratch->x = light->colour.x * strength;
    scratch->z = light->colour.z * strength;
    scratch->w = 1.0f;
    scratch->y = light->colour.y * strength;
    lights->ambient.x = lights->ambient.x + scratch->x;
    lights->ambient.y = lights->ambient.y + scratch->y;
    lights->ambient.z = lights->ambient.z + scratch->z;
}

// A light put among the strongest: before the first weaker one, the ones after it moved down (the last one dropped). Without the
// loop made into calls of memmove, which the game doesn't have
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void InsertStrongest(ChunkLights* lights, u32 slots, f32 strength, const Vector4* direction, const Light* light)
{
    for (u32 slot = 0; slot < slots; slot++)
    {
        if (!(lights->strengths[slot] < strength))
        {
            continue;
        }

        for (u32 moved = slots - 1; slot < moved; moved--)
        {
            lights->strengths[moved] = lights->strengths[moved - 1];
            lights->colours[moved] = lights->colours[moved - 1];
            lights->directions[moved] = lights->directions[moved - 1];
        }

        lights->strengths[slot] = strength;
        lights->directions[slot] = *direction;
        lights->colours[slot] = light->colour;
        return;
    }
}

Light* ReferencedLight(const ChunkLights* lights, const LightReference* reference)
{
    switch (reference->kind)
    {
    case Light::KindAmbient:
        return &lights->ambientLights[reference->index];
    case Light::KindDirectional:
        return &lights->directionalLights[reference->index];
    case Light::KindPoint:
        return &lights->pointLights[reference->index];
    case Light::KindSpot:
        return &lights->spotLights[reference->index];
    default:
        // The retail code then reads a vtable at 0x50
        return nullptr;
    }
}
}

void GatherStrongestLights(ChunkLights* lights, const Matrix4x4* chunkMatrix, Light* ownLight)
{
    // The retail stack's places, which some lights' LightAt leave as they were: the own light's position, then every direction
    // and ambient product of the scenery's lights; the own light's direction, then the ambient product of the 16 more
    Vector4 wayScratch;
    Vector4 directionScratch;
    Vector4 ownDirection;
    f32 ownStrength;
    f32 strength;
    f32 extraStrength;
    g_LightGathers++;
    u32 own = 0;
    ClearGatheredLights(lights);
    u32 slots = ChunkLights::OwnSlot;
    if (ownLight == nullptr)
    {
        slots = ChunkLights::Strongest;
    }
    else if (ownLight->Kind() == Light::KindAmbient)
    {
        AddAmbient(lights, ownLight, ownLight->intensity, &wayScratch);
    }
    else
    {
        // The own light is placed in the camera's space
        own = 1;
        Matrix4x4 fromCamera;
        VuInvertRigid(&fromCamera, &g_RenderView->toClip);
        wayScratch = ownLight->position;
        VuTransformPoint(&fromCamera, &wayScratch, &wayScratch);
        if (ownLight->Kind() == Light::KindDirectional)
        {
            directionScratch = static_cast<DirectionalLight*>(ownLight)->direction;
            VuRotateVector(&fromCamera, &directionScratch, &directionScratch);
            f32 scale = InverseLength(&directionScratch, LengthEpsilon);
            directionScratch.x = directionScratch.x * scale;
            directionScratch.y = directionScratch.y * scale;
            directionScratch.z = directionScratch.z * scale;
        }

        ownLight->LightAt(&lights->position, &ownDirection, &ownStrength, &wayScratch, &directionScratch);
        lights->strengths[ChunkLights::OwnSlot] = ownStrength;
        lights->directions[ChunkLights::OwnSlot] = ownDirection;
        lights->colours[ChunkLights::OwnSlot] = ownLight->colour;
    }

    // The scenery's root's light bits: a bit per reference
    const u8* mask = lights->chunk->scenery->lights;
    for (u32 group = 0; group < sizeof(SceneryCell::lights); group++, mask++)
    {
        if (*mask == 0)
        {
            continue;
        }

        for (u32 bit = 0; bit < 8; bit++)
        {
            if ((1u << bit & *mask) == 0)
            {
                continue;
            }

            Light* light = ReferencedLight(lights, &lights->references[group * 8 + bit]);
            if (light->Kind() == Light::KindAmbient)
            {
                AddAmbient(lights, light, light->intensity, &wayScratch);
                continue;
            }

            light->LightAt(&lights->position, &wayScratch, &strength, nullptr, nullptr);
            InsertStrongest(lights, slots, strength, &wayScratch, light);
        }
    }

    for (Light* light : lights->extraLights)
    {
        if (light == nullptr)
        {
            continue;
        }

        light->LightAt(&lights->position, &wayScratch, &extraStrength, nullptr, nullptr);
        if (light->Kind() == Light::KindAmbient)
        {
            AddAmbient(lights, light, extraStrength, &directionScratch);
            continue;
        }

        InsertStrongest(lights, slots, extraStrength, &wayScratch, light);
    }

    FinishGatheredLights(lights, chunkMatrix, own);
}

namespace
{
// The fall off at a point of a light at a position: 25 / (d² + 25)
f32 FallOffAt(const Vector4* from, const Vector4* at)
{
    Vector4 towards;
    towards.x = from->x - at->x;
    towards.y = from->y - at->y;
    towards.z = from->z - at->z;
    towards.w = 1.0f;
    f32 constant = g_LightingConstants[FalloffConstant];
    return constant / (towards.x * towards.x + towards.y * towards.y + towards.z * towards.z + constant);
}

void DestroyAttached(Light* light, u32 destroyFlags)
{
    light->vtable = g_LightVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(light);
    }
}

}

void AttachedAmbientLight::Destroy(u32 destroyFlags)
{
    DestroyAttached(this, destroyFlags);
}

void AttachedAmbientLight::SetOwnKind()
{
    header.kind = KindAttachedAmbient;
}

void AttachedAmbientLight::LightAt(const Vector4* at, Vector4*, f32* strength, const Vector4*, const Vector4*) const
{
    // The way out to the point is worked out with the fall off and dropped: an ambient light has no direction
    *strength = intensity * FallOffAt(&worldPosition, at);
}

void AttachedAmbientLight::ComputeBounds()
{
}

void AttachedAmbientLight::Follow()
{
    if (instance == nullptr)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuTransformPoint(&place->matrix, &position, &worldPosition);
}

u32 AttachedAmbientLight::Unused13()
{
    return 0;
}

void AttachedDirectionalLight::Destroy(u32 destroyFlags)
{
    DestroyAttached(this, destroyFlags);
}

void AttachedDirectionalLight::SetOwnKind()
{
    header.kind = KindAttachedDirectional;
}

void AttachedDirectionalLight::LightAt(const Vector4* at, Vector4* out, f32* strength, const Vector4*, const Vector4*) const
{
    *strength = intensity * FallOffAt(&worldPosition, at);
    *out = worldDirection;
}

void AttachedDirectionalLight::ComputeBounds()
{
}

void AttachedDirectionalLight::Follow()
{
    if (instance == nullptr)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuTransformPoint(&place->matrix, &position, &worldPosition);
    VuRotateVector(&place->matrix, &direction, &worldDirection);
}

u32 AttachedDirectionalLight::Unused13()
{
    return 0;
}

void AttachedPointLight::Destroy(u32 destroyFlags)
{
    DestroyAttached(this, destroyFlags);
}

void AttachedPointLight::SetOwnKind()
{
    header.kind = KindAttachedPoint;
}

void AttachedPointLight::LightAt(const Vector4* at, Vector4* out, f32* strength, const Vector4*, const Vector4*) const
{
    f32 falloff = FallOff(this, at, out, &worldPosition);
    *strength = Attenuated(intensity, attenuationPower, falloff);
}

void AttachedPointLight::ComputeBounds()
{
}

void AttachedPointLight::Follow()
{
    if (instance == nullptr)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuTransformPoint(&place->matrix, &position, &worldPosition);
}

u32 AttachedPointLight::Unused13()
{
    return 0;
}

void AttachedSpotLight::Destroy(u32 destroyFlags)
{
    DestroyAttached(this, destroyFlags);
}

void AttachedSpotLight::SetOwnKind()
{
    header.kind = KindAttachedSpot;
}

void AttachedSpotLight::LightAt(const Vector4* at, Vector4* out, f32* strength, const Vector4*, const Vector4*) const
{
    SpotLightFrom(this, at, out, strength, &worldPosition, &worldDirection);
}

void AttachedSpotLight::ComputeBounds()
{
}

void AttachedSpotLight::Follow()
{
    if (instance == nullptr)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    VuTransformPoint(&place->matrix, &position, &worldPosition);
    VuRotateVector(&place->matrix, &direction, &worldDirection);
}

u32 AttachedSpotLight::Unused13()
{
    return 0;
}

void ClearLightingUnused()
{
    g_LightingUnused = 0;
}

void ComputeSpotLightBounds(SpotLight* light)
{
    constexpr f32 Reach = 100.0f;
    f32 reach = light->intensity * Reach;
    s32 angle[4];
    AngleFrom(angle, static_cast<f32>(light->coneAngle) * AngleToRadians, AngleRadians);
    angle[0] = angle[0] + static_cast<s32>(static_cast<f32>(light->falloffAngle) * AngleToRadians * RadiansToAngle);
    f32 radius = reach * TanOfAngle(angle);
    // The cone's cross-section's two axes. Retail takes the first's perpendicular of an uninitialized stack vector: the direction
    // is what it means (the box is never read)
    Vector4 base = light->direction;
    Vector4 across;
    PerpendicularOf(&base, &across);
    Vector4 other;
    other.x = base.y * across.z - base.z * across.y;
    other.z = base.x * across.y - base.y * across.x;
    other.y = base.z * across.x - base.x * across.z;
    other.w = 1.0f;
    f32 inverse = InverseLength(&across, LengthEpsilon);
    across.x = across.x * inverse;
    across.y = across.y * inverse;
    across.z = across.z * inverse;
    inverse = InverseLength(&other, LengthEpsilon);
    other.x = other.x * inverse;
    other.y = other.y * inverse;
    other.z = other.z * inverse;
    Vector4 centre = {light->direction.x * reach, light->direction.y * reach, light->direction.z * reach, 1.0f};
    f32 extentX = radius * __builtin_sqrtf(across.x * across.x + other.x * other.x);
    f32 extentZ = radius * __builtin_sqrtf(across.z * across.z + other.z * other.z);
    f32 extentY = radius * __builtin_sqrtf(across.y * across.y + other.y * other.y);
    f32 low[4] = {centre.x - extentX, centre.y - extentY, centre.z - extentZ, 0.0f};
    f32 high[4] = {centre.x + extentX, centre.y + extentY, centre.z + extentZ, 0.0f};
    f32 lowest[4] = {};
    f32 highest[4] = {};
    for (s32 axis = 0; axis < 3; axis++)
    {
        if (low[axis] < lowest[axis])
        {
            lowest[axis] = low[axis];
        }

        if (highest[axis] < low[axis])
        {
            highest[axis] = low[axis];
        }

        if (high[axis] < lowest[axis])
        {
            lowest[axis] = high[axis];
        }

        if (highest[axis] < high[axis])
        {
            highest[axis] = high[axis];
        }
    }

    light->boundsMin = {lowest[0] + light->position.x, lowest[1] + light->position.y, lowest[2] + light->position.z, lowest[3]};
    light->boundsMax = {highest[0] + light->position.x, highest[1] + light->position.y, highest[2] + light->position.z,
                        highest[3]};
}
