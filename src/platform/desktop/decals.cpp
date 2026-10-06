#include "platform/graphics.h"

#include "frustum.h"
#include "game/decals.h"
#include "game/math.h"
#include "vu0.h"

#include <cstring>

// The desktop's decals: each decal's look over its life worked out every frame (VU0's decal set on the PS2)
// On the PS2 the view and a type's variants go into VU0's memory (renderer/particles.cpp) and a microprogram ages each decal:
// 0x1C0, or 0x360 for decals placed in another chunk than the camera's. Here the memory is this file's, the programs' operations
// done in their order
namespace
{
// VU0's memory as the programs address it, in quadwords (an address past its end wraps round): the view from 0 (the far set's
// four side planes, then the near plane four times, each four transposed: a plane a field), the matrix of the chunk the decals
// are in from 8 when it's another, and the type's variants from 0x10
constexpr u32 MemoryQuadwords = 0x100;
constexpr u32 SidePlanesAddress = 0;
constexpr u32 NearPlaneAddress = 4;
constexpr u32 OtherChunkAddress = 8;
constexpr u32 VariantsAddress = 0x10;
// A variant's quadwords: its four colour keys (alpha in w), then its four size keys (the width across in x, the length along in
// z). The life left times the second size key's w is the share of the life still to go
constexpr u32 VariantQuadwords = sizeof(DecalVariant) / sizeof(Vector4);
constexpr u32 SizeKeys = 4;
constexpr u32 LifeScale = SizeKeys + 1;
// The keys are a third of the life apart
constexpr f32 KeySpans = 3.0f;
// The frame's half words are 32768 a unit (ITOF15), the corners' offsets 4096 (FTOI12)
constexpr f32 FrameUnit = 0x1p-15f;
constexpr f32 OffsetUnit = 0x1p12f;
// A quadword's fields, and their sign bits in VU0's MAC flags as the programs take them (FMAND with 0xF0): x's the highest, w's
// the lowest
constexpr u32 Fields = 4;
constexpr u32 FirstFieldSign = 0x80;
constexpr s32 MostInt = 0x7FFFFFFF;

alignas(16) Vector4 g_Memory[MemoryQuadwords];
// The program LoadDecalView picked (vi27): the decals' places taken through the other chunk's matrix
bool g_InOtherChunk;

const Vector4& Quadword(u32 address)
{
    return g_Memory[address % MemoryQuadwords];
}

f32& FieldOf(Vector4& vector, u32 field)
{
    return (&vector.x)[field];
}

f32 FieldOf(const Vector4& vector, u32 field)
{
    return (&vector.x)[field];
}

Vector4 Scaled(const Vector4& vector, f32 scale)
{
    return {vector.x * scale, vector.y * scale, vector.z * scale, vector.w * scale};
}

Vector4 Sum(const Vector4& a, const Vector4& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

Vector4 LessBy(const Vector4& vector, f32 value)
{
    return {vector.x - value, vector.y - value, vector.z - value, vector.w - value};
}

// The share of the way from a key to the next: from less from · share, plus to · share
f32 Between(f32 from, f32 to, f32 share)
{
    return from - from * share + to * share;
}

Vector4 Between(const Vector4& from, const Vector4& to, f32 share)
{
    return {Between(from.x, to.x, share), Between(from.y, to.y, share), Between(from.z, to.z, share),
            Between(from.w, to.w, share)};
}

// A direction of the decal's frame (ITOF15 of its four words)
Vector4 FrameDirection(const s32* words)
{
    return {static_cast<f32>(words[0]) * FrameUnit, static_cast<f32>(words[1]) * FrameUnit,
            static_cast<f32>(words[2]) * FrameUnit, static_cast<f32>(words[3]) * FrameUnit};
}

// A place through four transposed planes from an address: ((the offsets + x · the normals' x) + y · their y) + z · their z
Vector4 PlaneDistances(const Vector4& place, u32 address)
{
    Vector4 distance = Sum(Quadword(address + 3), Scaled(Quadword(address), place.x));
    distance = Sum(distance, Scaled(Quadword(address + 1), place.y));
    return Sum(distance, Scaled(Quadword(address + 2), place.z));
}

// A place through the other chunk's matrix: ((x · its first row + y · its second) + z · its third) + its fourth
Vector4 ThroughOtherChunk(const Vector4& place)
{
    Vector4 moved = Sum(Scaled(Quadword(OtherChunkAddress), place.x), Scaled(Quadword(OtherChunkAddress + 1), place.y));
    moved = Sum(Sum(moved, Scaled(Quadword(OtherChunkAddress + 2), place.z)), Quadword(OtherChunkAddress + 3));
    return {moved.x, moved.y, moved.z, place.w};
}

// The fields' sign bits (a zero's too), as the programs read the MAC flags
u32 SignBits(const Vector4& value)
{
    u32 bits = 0;
    for (u32 field = 0; field < Fields; field++)
    {
        if (__builtin_signbitf(FieldOf(value, field)) != 0)
        {
            bits |= FirstFieldSign >> field;
        }
    }

    return bits;
}

// VU0's FTOI0: towards zero, held at an int's ends past them (where VU0 also puts the infinities and NaNs it doesn't have)
s32 FloatToInt(f32 value)
{
    if (__builtin_isnan(value) || !(__builtin_fabsf(value) < 0x1p31f))
    {
        return __builtin_signbitf(value) != 0 ? -MostInt - 1 : MostInt;
    }

    return static_cast<s32>(value);
}
}

namespace Platform::Graphics
{
void LoadDecalView(const Matrix4x4* chunkMatrices, const Matrix4x4* fromChunk)
{
    Vector4 sides[Fields];
    Vector4 nearPlane;
    DesktopGraphics::ChunkViewPlanes(chunkMatrices, sides, &nearPlane);
    for (u32 axis = 0; axis < Fields; axis++)
    {
        Vector4& sidesAxis = g_Memory[SidePlanesAddress + axis];
        for (u32 side = 0; side < Fields; side++)
        {
            FieldOf(sidesAxis, side) = FieldOf(sides[side], axis);
        }

        f32 nearAxis = FieldOf(nearPlane, axis);
        g_Memory[NearPlaneAddress + axis] = {nearAxis, nearAxis, nearAxis, nearAxis};
    }

    g_InOtherChunk = fromChunk != nullptr;
    if (fromChunk == nullptr)
    {
        return;
    }

    for (u32 row = 0; row < 4; row++)
    {
        g_Memory[OtherChunkAddress + row] = *RowOf(fromChunk, row);
    }
}

// Its count kept within its variants: the PS2's transfer of more reads what follows them
void LoadDecalType(DecalType* type)
{
    s32 count = type->variantCount;
    if (count < 0)
    {
        count = 0;
    }
    else if (count > static_cast<s32>(DecalType::MostVariants))
    {
        count = DecalType::MostVariants;
    }

    std::memcpy(&g_Memory[VariantsAddress], type->variants, static_cast<u32>(count) * sizeof(DecalVariant));
}

// The share of its life gone picks the keys either side of it and the share of the way between them, which give its colour and
// size. Its frame's directions scaled by the size give two corners' offsets (12 bits of fraction), and its reach (the distance to
// a corner) the planes it reaches behind, a sign bit each (the near plane's in every field): the first offsets' fourth half word
void AgeDecal(const f32* place, const s32* frame, s32 variant, DecalLook* look)
{
    Vector4 at = {place[0], place[1], place[2], 1.0f};
    if (g_InOtherChunk)
    {
        at = ThroughOtherChunk(at);
    }

    u32 address = static_cast<u32>(variant) * VariantQuadwords + VariantsAddress;
    f32 thirds = (1.0f - place[3] * Quadword(address + LifeScale).w) * KeySpans;
    s32 key = FloatToInt(thirds);
    f32 share = thirds - static_cast<f32>(key);
    address += static_cast<u32>(key);
    const Vector4& size = Quadword(address + SizeKeys);
    const Vector4& nextSize = Quadword(address + SizeKeys + 1);
    f32 width = Between(size.x, nextSize.x, share);
    f32 length = Between(size.z, nextSize.z, share);
    Vector4 colour = Between(Quadword(address), Quadword(address + 1), share);
    Vector4 across = Scaled(FrameDirection(&frame[0]), width);
    Vector4 along = Scaled(FrameDirection(&frame[4]), length);
    Vector4 first = Sum(Scaled(across, -0.5f), Scaled(along, 0.5f));
    Vector4 second = Sum(Scaled(across, 0.5f), Scaled(along, 0.5f));
    f32 halfWidth = width * 0.5f;
    f32 halfLength = length * 0.5f;
    f32 reach = Vu0::SquareRoot(halfWidth * halfWidth + halfLength * halfLength);
    u32 behind = SignBits(LessBy(PlaneDistances(at, SidePlanesAddress), reach)) |
                 SignBits(LessBy(PlaneDistances(at, NearPlaneAddress), reach));
    look->place[0] = at.x;
    look->place[1] = at.y;
    look->place[2] = at.z;
    look->place[3] = g_InOtherChunk ? reach : 1.0f;
    for (u32 field = 0; field < Fields; field++)
    {
        look->colour[field] = static_cast<u8>(FloatToInt(FieldOf(colour, field)));
        look->moreSizes[field] = static_cast<s16>(FloatToInt(FieldOf(second, field) * OffsetUnit));
    }

    for (u32 field = 0; field < 3; field++)
    {
        look->sizes[field] = static_cast<s16>(FloatToInt(FieldOf(first, field) * OffsetUnit));
    }

    look->sizes[3] = static_cast<s16>(behind);
}
}
