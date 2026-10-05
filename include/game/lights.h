#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"

class Stream;
struct ChunkData;

// A light's header (TT Lab's LightType and Enabled): its kind (Light::Kind) and whether it's enabled, which nothing reads
union LightHeader
{
    u32 value;
    struct
    {
        u32 kind : 8;
        u32 enabled : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(LightHeader, 4);

// A light of a chunk's scenery (TT Lab's Light: verified in the PAL executable, its notes there), its vtable 0x50 bytes in: its
// header, its intensity (which multiplies its colour), its colour, position and the box around it the game works out again when
// it's read (never read either). Its vtable's functions: 1 the destructor, 2 and 3 enabled and disabled, 4 its own kind set, 5 a
// kind set, 6 its kind, 7 another's base values taken, 8 what it comes to at a place (the direction toward it, a row, and its
// strength there; a place and a direction can stand in for its own), 9 its box worked out, 10 read, 11 its place taken from what
// it follows (nothing for the scenery's lights), 12 another's values taken (its kind's and the base's)
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
        // The lights that follow an instance
        KindAttachedAmbient = 5,
        KindAttachedDirectional = 6,
        KindAttachedPoint = 7,
        KindAttachedSpot = 8,
    };

    // Its vtable's slots the lights call
    enum Slot : u32
    {
        SlotDestroy = 1,
        SlotKind = 6,
        SlotAssignBase = 7,
        SlotLightAt = 8,
        SlotRead = 10,
    };

    LightHeader header;
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
    void Follow() RETAIL(FUN_001cb4c0);

    u32 Kind() const
    {
        return CallVirtual<u32>(this, vtable, SlotKind);
    }

    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
    {
        CallVirtual<void>(this, vtable, SlotLightAt, at, direction, strength, position, towards);
    }

    void ReadVirtual(Stream* stream)
    {
        CallVirtual<void>(this, vtable, SlotRead, stream);
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

// The lights that follow an instance (kinds 5 to 8, which chunks never have): an ambient light glowing out from where it is and a
// directional light, both falling off with the distance by 25 / (d² + 25) once, a point light and a spot light. Their vtables'
// slot 11 takes their position (and direction) into the world through their instance's place; 9 (their bounds) does nothing, 13
// says no (nothing calls it)
struct AttachedAmbientLight : AmbientLight
{
    // After the base's whole size (GCC 2.9x didn't put members into a base's tail padding)
    alignas(16) struct InstanceContext* instance;
    Vector4 worldPosition;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb7a0);
    void SetOwnKind() RETAIL(func_001CB7D0);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(func_001CBA40);
    void ComputeBounds() RETAIL(FUN_001cb7e0);
    void Follow() RETAIL(FUN_001cb9f0);
    u32 Unused13() RETAIL(FUN_001cb7e8);
};
CHECK_OFFSET(AttachedAmbientLight, instance, 0x60);
CHECK_OFFSET(AttachedAmbientLight, worldPosition, 0x70);
CHECK_SIZE(AttachedAmbientLight, 0x80);

struct AttachedDirectionalLight : DirectionalLight
{
    alignas(16) struct InstanceContext* instance;
    Vector4 worldPosition;
    Vector4 worldDirection;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb7f8);
    void SetOwnKind() RETAIL(func_001CB828);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(func_001CBB50);
    void ComputeBounds() RETAIL(FUN_001cb838);
    void Follow() RETAIL(FUN_001cbaf0);
    u32 Unused13() RETAIL(FUN_001cb840);
};
CHECK_OFFSET(AttachedDirectionalLight, instance, 0x80);
CHECK_OFFSET(AttachedDirectionalLight, worldPosition, 0x90);
CHECK_SIZE(AttachedDirectionalLight, 0xB0);

struct AttachedPointLight : PointLight
{
    alignas(16) struct InstanceContext* instance;
    Vector4 worldPosition;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb748);
    void SetOwnKind() RETAIL(func_001CB778);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(func_001C88D0);
    void ComputeBounds() RETAIL(FUN_001cb788);
    void Follow() RETAIL(FUN_001cbbf0);
    u32 Unused13() RETAIL(FUN_001cb790);
};
CHECK_OFFSET(AttachedPointLight, instance, 0x70);
CHECK_OFFSET(AttachedPointLight, worldPosition, 0x80);
CHECK_SIZE(AttachedPointLight, 0x90);

struct AttachedSpotLight : SpotLight
{
    alignas(16) struct InstanceContext* instance;
    Vector4 worldPosition;
    Vector4 worldDirection;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001cb850);
    void SetOwnKind() RETAIL(FUN_001cb880);
    void LightAt(const Vector4* at, Vector4* direction, f32* strength, const Vector4* position, const Vector4* towards) const
        RETAIL(FUN_001c89a8);
    void ComputeBounds() RETAIL(FUN_001cb890);
    void Follow() RETAIL(FUN_001cbc40);
    u32 Unused13() RETAIL(FUN_001cb898);
};
CHECK_OFFSET(AttachedSpotLight, instance, 0x90);
CHECK_OFFSET(AttachedSpotLight, worldPosition, 0xA0);
CHECK_SIZE(AttachedSpotLight, 0xC0);

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
    // The strongest lights kept, the last one the object's own light's slot when it has one
    static constexpr u32 Strongest = 3;
    static constexpr u32 OwnSlot = 2;

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
    u8 unused428[0x430 - 0x428];
    Vector4 position;
    Vector4 colours[Strongest];
    Vector4 directions[Strongest];
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

// Where the lighting's constants keep the point and spot lights' fall off (K in K / (d² + K)) and the way shadows are cast from an
// instance (a Vector4, times the shadow's strength)
enum LightingConstant : u32
{
    FalloffConstant = 3,
    ShadowDirection = 0x20,
    LightingConstantCount = 36,
};

extern "C"
{
    extern const GccVTableEntry g_LightVTable[] RETAIL(LightBase_Methods);
    extern const GccVTableEntry g_AmbientLightVTable[] RETAIL(AmbientLight_Methods);
    extern const GccVTableEntry g_DirectionalLightVTable[] RETAIL(DirectionalLight_Methods);
    extern const GccVTableEntry g_PointLightVTable[] RETAIL(PointLight_Methods);
    extern const GccVTableEntry g_SpotLightVTable[] RETAIL(NegativeLight_Methods);
    // The lighting's constants (a static constructor sets them, LightingConstant): 25 the point and spot lights' fall off by,
    // then colours and directions
    extern f32 g_LightingConstants[LightingConstantCount] RETAIL(LightingConstants);
    // How many objects had their lights gathered (only counted)
    extern s32 g_LightGathers RETAIL(D_00309D84);
    void InitLightingConstants(u32 initialise, u32 priority) RETAIL(InitLightingConstants);
    // A byte the renderer clears every frame, which nothing reads
    extern u8 g_LightingUnused RETAIL(D_00309D88);
    void ClearLightingUnused() RETAIL(FUN_001cb8b0);

    // The spot light's box: the cone's at its reach (its intensity times 100), the light's position in it. Nothing reads it
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
