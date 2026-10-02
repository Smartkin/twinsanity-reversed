#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"

class Stream;
struct ChunkData;

// A light of a chunk's scenery (TT Lab's Light: verified in the PAL executable, its notes there), its vtable 0x50 bytes in: its
// header (the low byte its kind, bit 8 set when enabled, never read), its intensity (which multiplies its colour), its colour,
// position and the box around it the game works out again when it's read (never read either). Its vtable's functions: 1 the
// destructor, 2 and 3 enabled and disabled, 4 its own kind set, 5 a kind set, 6 its kind, 7 another's base values taken, 8 what
// it comes to at a place (the direction toward it, a row, and its strength there; a place and a direction can stand in for its
// own), 9 its box worked out, 10 read, 11 nothing, 12 another's values taken (its kind's and the base's)
struct Light
{
    enum Kind : u8
    {
        KindAmbient = 0,
        KindDirectional = 1,
        KindPoint = 2,
        KindSpot = 3,
        // The base's: none
        KindNone = 4,
    };

    enum Header : u32
    {
        KindMask = 0xFF,
        Enabled = 0x100,
    };

    u32 header;
    f32 intensity;
    Vector4 colour;
    Vector4 position;
    Vector4 boundsMin;
    Vector4 boundsMax;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb420);
    void Enable() RETAIL(FUN_001cb470);
    void Disable() RETAIL(FUN_001cb488);
    void SetOwnKind() RETAIL(FUN_001cb4a0);
    void SetKind(u32 kind) RETAIL(FUN_001cb4b0);
    u32 GetKind() const RETAIL(GetLightType);
    Light* AssignBase(const Light* other) RETAIL(FUN_001cb540);
    void Read(Stream* stream) RETAIL(ReadBaseLight);
    void Nothing() RETAIL(FUN_001cb4c0);

    u32 Kind() const
    {
        return CallVirtual<u32>(this, vtable, 6);
    }

    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
    {
        CallVirtual<void>(this, vtable, 8, at, direction, strength, position, towards);
    }

    void ReadVirtual(Stream* stream)
    {
        CallVirtual<void>(this, vtable, 10, stream);
    }
};
CHECK_OFFSET(Light, vtable, 0x50);

// Lights everything the same, added to the ambient light (0x60 bytes): its box 100000 times its intensity
struct AmbientLight : Light
{
    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb4c8);
    void SetOwnKind() RETAIL(FUN_001cb4f8);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(FUN_001cb500);
    void ComputeBounds() RETAIL(ComputeAmbientLightBounds);
    AmbientLight* Assign(const AmbientLight* other) RETAIL(FUN_001cb508);
};
CHECK_SIZE(AmbientLight, 0x60);

// Lights from one way wherever it is (0x80 bytes): its direction (a unit vector toward where it comes from), a leftover of the
// tools' (never read); its box 99999.99 times its intensity
struct DirectionalLight : Light
{
    Vector4 direction;
    s16 leftover;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb578);
    void SetOwnKind() RETAIL(FUN_001cb5b0);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(DirectionalLightAt);
    void ComputeBounds() RETAIL(ComputeDirectionalLightBounds);
    void Read(Stream* stream) RETAIL(ReadDirectionalLight);
    DirectionalLight* Assign(const DirectionalLight* other) RETAIL(FUN_001cb5c0);
};
CHECK_OFFSET(DirectionalLight, direction, 0x60);
CHECK_SIZE(DirectionalLight, 0x80);

// Lights from its position (0x70 bytes), falling off by 25 / (d² + 25) to the power of its attenuation; its box 100 times its
// intensity
struct PointLight : Light
{
    s16 attenuationPower;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb698);
    void SetOwnKind() RETAIL(FUN_001cb6d0);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(PointLightAt);
    void ComputeBounds() RETAIL(ComputePointLightBounds);
    void Read(Stream* stream) RETAIL(ReadPointLight);
    PointLight* Assign(const PointLight* other) RETAIL(FUN_001cb968);
};
CHECK_OFFSET(PointLight, attenuationPower, 0x60);
CHECK_SIZE(PointLight, 0x70);

// A point light shining along its direction (0x90 bytes, the tools' negative light): full within the inner cone's cosine, fading
// out to the outer's, the strength to the power of its spot exponent; the cone's angles (65536ths of a turn) only make its box
struct SpotLight : Light
{
    Vector4 direction;
    f32 innerConeCosine;
    f32 outerConeCosine;
    s32 coneAngle;
    s32 falloffAngle;
    s16 attenuationPower;
    u16 spotExponent;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb6e0);
    void SetOwnKind() RETAIL(FUN_001cb738);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(SpotLightAt);
    void Read(Stream* stream) RETAIL(ReadNegativeLight);
    SpotLight* Assign(const SpotLight* other) RETAIL(FUN_001cbca0);
};
CHECK_OFFSET(SpotLight, attenuationPower, 0x80);
CHECK_SIZE(SpotLight, 0x90);

// Which light a bit of the scenery's light masks is: the index in its kind's list and the kind
struct LightReference
{
    s32 index;
    s32 kind;
};

// A chunk's lights (0x500 bytes): its lists of each kind, the lights the scenery's 128 mask bits stand for and how many of each
// kind it has, and what the strongest come to at the object being drawn (GatherStrongestLights): its place, the three strongest
// lights' colours and directions (in the world, a row each), their strengths and the ambient light; and 16 more lights of the
// game's own
struct ChunkLights
{
    ChunkData* chunk;
    AmbientLight* ambientLights;
    DirectionalLight* directionalLights;
    PointLight* pointLights;
    SpotLight* spotLights;
    LightReference references[128];
    s32 lightCount;
    s32 ambientCount;
    s32 directionalCount;
    s32 pointCount;
    s32 spotCount;
    u8 unknown428[0x430 - 0x428];
    Vector4 position;
    Vector4 colours[3];
    Vector4 directions[3];
    f32 strengths[4];
    Vector4 ambient;
    Light* extraLights[16];
};
CHECK_OFFSET(ChunkLights, lightCount, 0x414);
CHECK_OFFSET(ChunkLights, colours, 0x440);
CHECK_OFFSET(ChunkLights, directions, 0x470);
CHECK_OFFSET(ChunkLights, strengths, 0x4A0);
CHECK_OFFSET(ChunkLights, ambient, 0x4B0);
CHECK_SIZE(ChunkLights, 0x500);

extern "C"
{
    extern const GccVTableEntry g_LightVTable[] RETAIL(LightBase_Methods);
    extern const GccVTableEntry g_AmbientLightVTable[] RETAIL(AmbientLight_Methods);
    extern const GccVTableEntry g_DirectionalLightVTable[] RETAIL(DirectionalLight_Methods);
    extern const GccVTableEntry g_PointLightVTable[] RETAIL(PointLight_Methods);
    extern const GccVTableEntry g_SpotLightVTable[] RETAIL(NegativeLight_Methods);
    // The lighting's constants (a static constructor sets them): 25 the point and spot lights' fall off by (the fourth), then
    // colours and directions
    extern f32 g_LightingConstants[36] RETAIL(LightingConstants);
    // How many objects had their lights gathered (only counted)
    extern s32 g_LightGathers RETAIL(D_00309D84);
    void InitLightingConstants(u32 initialise, u32 priority) RETAIL(FUN_001cb2d8);

    // The spot light's box: the cone's (still asm)
    void ComputeSpotLightBounds(SpotLight* light);

    // A chunk's lights without any yet, and destroyed (their lists and the 16 more, last to first)
    ChunkLights* ConstructChunkLights(ChunkLights* lights, ChunkData* chunk) RETAIL(CreateChunkLightController);
    void DestroyLights(ChunkLights* lights, u32 destroyFlags) RETAIL(FUN_001c78b0);
    // The lists made for the counts read, every light enabled and of its kind
    void MakeChunkLights(ChunkLights* lights) RETAIL(FUN_001c7b00);
    // From the SM2's scenery: the references, the counts and each kind's lights
    void ReadSceneryLights(ChunkLights* lights, Stream* stream);
    // What was gathered emptied: no strengths, no colours, no ambient light
    void ClearGatheredLights(ChunkLights* lights) RETAIL(FUN_001cb8b8);
    // What was gathered finished: the lights' directions turned through the chunk's matrix and their colours halved by their
    // strengths (those with a strength: all three, or two when the object's own light took the third), the ambient light halved
    void FinishGatheredLights(ChunkLights* lights, const Matrix4x4* chunkMatrix, u32 ownLight) RETAIL(FUN_001c7d50);
    // The strongest lights at the object being drawn gathered into the lights' slots for it (its place given before, the chunk's
    // matrix, the object's own light)
    void GatherStrongestLights(ChunkLights* lights, const Matrix4x4* chunkMatrix, Light* ownLight);
}
