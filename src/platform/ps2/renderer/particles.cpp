#include "renderer.h"

#include "abi.h"
#include "game/clock.h"
#include "game/graphicstables.h"
#include "game/memory.h"
#include "game/particles.h"
#include "game/stream.h"
#include "platform/graphics.h"

namespace
{
// Particles and decals go to VU1 at 0x4F: the matrix to the screen first (an UNPACK of four quadwords)
constexpr u32 MatrixUnpack = 0x6C04004F;
constexpr u32 RefsTag = 0x40000000;

// The writer's packet of a particle block: the matrix, the header's DMA tag and its two quadwords (the hexagons' distortion in
// the second), CALLs of the header's packet (the texture page's set-up) and of the particles
void WriteParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material)
{
    RenderBucket& writer = material->writer;
    auto* at = BeginPacket(writer);
    at[0] = CountTag | 4;
    at[1] = 0;
    at[2] = 0;
    at[3] = MatrixUnpack;
    *reinterpret_cast<Matrix4x4*>(at + 4) = *matrix;
    auto* quadwords = reinterpret_cast<Vector4*>(at + 20);
    quadwords[0] = *reinterpret_cast<const Vector4*>(header->tag);
    quadwords[1] = *reinterpret_cast<const Vector4*>(header->values);
    quadwords[2] = *reinterpret_cast<const Vector4*>(header->distortion);
    at[32] = CallTag;
    at[33] = Address(header->packet);
    at[34] = 0;
    at[35] = 0;
    at[36] = CallTag;
    at[37] = Address(block);
    at[38] = 0;
    at[39] = 0;
    EndPacket(writer, reinterpret_cast<u8*>(at + 40));
}
}

extern "C"
{
    void DrawParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale)
    {
        if (material->writer.first == nullptr)
        {
            StartMaterialWriter(material);
        }

        header->values[0] = 0.5f;
        header->values[3] = scale;
        WriteParticleBlock(block, header, matrix, material);
    }

    void DrawHexagonParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale,
                                  f32 distortionX, f32 distortionY)
    {
        if (material->writer.first == nullptr)
        {
            StartMaterialWriter(material);
        }

        header->values[0] = 0.5f;
        header->values[3] = scale;
        header->distortion[0] = distortionX * 0x1p-14f;
        header->distortion[1] = distortionY * -0x1p-12f;
        header->distortion[2] = 40.0f;
        *reinterpret_cast<u32*>(&header->distortion[3]) = 0;
        WriteParticleBlock(block, header, matrix, material);
    }

    void DrawDecals(DecalData* decals)
    {
        // No decals: no chunk they're in
        if (decals->count == 0)
        {
            decals->chunk = nullptr;
            return;
        }

        RenderView* view = g_RenderView;
        if (view == nullptr)
        {
            return;
        }

        Matrix4x4 toScreen = view->toScreen;
        for (u32 type = 0; type < 4; type++)
        {
            DecalBlock* decal = decals->drawLists[type];
            decals->drawLists[type] = nullptr;
            if (decal == nullptr)
            {
                continue;
            }

            Material* material = decals->page.materials[type];
            if (material->writer.first == nullptr)
            {
                StartMaterialWriter(material);
            }

            // The matrix, the UV packet by REFS (VU1 runs the program at 0 on it), and each block's 0x4C quadwords
            RenderBucket& writer = material->writer;
            u32* at = BeginPacket(writer);
            at[0] = CountTag | 4;
            at[1] = 0;
            at[2] = 0;
            at[3] = MatrixUnpack;
            *reinterpret_cast<Matrix4x4*>(at + 4) = toScreen;
            at += 20;
            u32 size = (decals->typeCount + 1) * 2 + 1;
            at[0] = size | RefsTag;
            at[1] = Address(decals->uvPacket);
            at[2] = 0;
            at[3] = size << 16 | 0x6C000053;
            at[4] = CountTag;
            at[5] = 0;
            at[6] = 0;
            at[7] = VifMscal;
            at += 8;
            for (; decal != nullptr; decal = decal->drawNext)
            {
                at[0] = RefsTag | 0x4C;
                at[1] = Address(decal->placesUnpack);
                at[2] = 0;
                at[3] = 0;
                at += 4;
            }

            EndPacket(writer, reinterpret_cast<u8*>(at));
        }
    }
}

// A packet of quadwords for VU0's memory as the asm makes it: how many there are, and room for them
struct Vu0Packet
{
    s32 count;
    s32 unknown04;
    s32 capacity;
    s32 unknown0C;
    u32 data[16][4];
};

extern "C"
{
    // A packet started: no quadwords
    Vu0Packet* StartVu0Packet(Vu0Packet* packet) RETAIL(FUN_001ba2f0);
    // A chunk's view for VU0's programs: four rows into a packet's quadwords and a fifth (still asm)
    u32* WriteChunkViewRows(u32* fifth, const Matrix4x4* chunkMatrices, u32* rows) RETAIL(FUN_001ef370);
    // Four quadwords transposed in place (still asm)
    void TransposeQuadwords(void* rows) RETAIL(FUN_0018ee88);
    // Quadwords copied into VU0's memory at an address by the VU0 programs' copier (still asm)
    void SendToVu0(u8* programs, const void* data, s32 quadwords, s32 address) RETAIL(FUN_002b2178);
    // A DMA channel's transfer started once its last is over, and whether VIF0's still is (still asm)
    void TransferDMA_Data(s32 channel, void* data, s32 unknown) RETAIL(TransferDMA_Data);
    s32 IsVIF0_TransferingFromMemory(s32 channel) RETAIL(IsVIF0_TransferingFromMemory);
    // The VU0 microcode sets
    extern u8 g_Vu0Programs[] RETAIL(G_UnkDmaRelated);
    // The disk node of the screen effects' buffers (none at start-up), the wave shader the particles' start-up makes and type 0x1C's
    // vtable
    extern s32 g_EffectsDiskNode RETAIL(D_0030A848);
    extern WaveShader g_ParticleWaveShader RETAIL(G_PrecompShader_0x1C_3323C0);
    extern const GccVTableEntry g_ShaderType1CVTable[] RETAIL(PrecompiledShader__Type_0x1C_Methods);
    extern Material g_DistortionMaterial RETAIL(D_00370758);
    // The view's packet made (the matrices for the particles) and sent into VU0's memory at the index's place, which it returns
    // and keeps in the word 0x100 bytes past the matrices (still asm)
    s32 UploadParticleView(const Matrix4x4* matrices, s32 index) RETAIL(FUN_001e91f8);
}

EABI_EXPORT(DrawParticleBlock, DrawParticleBlock);
EABI_EXPORT(DrawHexagonParticleBlock, DrawHexagonParticleBlock);

namespace Platform::Graphics
{
// A block's first quadword is its DMA tag: RET ends the chain, NEXT goes on at the address in its second word
void EndParticleBlock(u8* block)
{
    constexpr u32 TagKept = 0x8FFFFFFF;
    auto* tag = reinterpret_cast<u32*>(block);
    tag[1] = 0;
    tag[0] = (tag[0] & TagKept) | ReturnTag;
}

void ChainParticleBlocks(u8* const* blocks, s32 count)
{
    constexpr u32 TagKept = 0x8FFFFFFF;
    constexpr u32 AddressKept = 0x7FFFFFFF;
    EndParticleBlock(blocks[count - 1]);
    for (s32 block = count - 2; block >= 0; block--)
    {
        auto* tag = reinterpret_cast<u32*>(blocks[block]);
        tag[0] = (tag[0] & TagKept) | NextTag;
        tag[1] = Address(blocks[block + 1]) & AddressKept;
    }
}

// A block: a RET tag of its particles' quadwords with VIF1's UNPACK of them, the GIF tag of their draws and its registers, and
// after them an MSCNT starting the VU1 program (the hexagons' as the fourth word of the last quadword)
void InitParticleBlock(u8* block, bool hexagons)
{
    auto* words = reinterpret_cast<u32*>(block);
    auto* gifTag = reinterpret_cast<u64*>(block + 0x10);
    u32 footer = hexagons ? 0x1A0 : 0x420;
    auto* last = reinterpret_cast<u32*>(block + footer);
    last[0] = 0;
    last[1] = 0;
    last[2] = 0;
    last[3] = 0;
    words[0] = 0;
    words[1] = 0;
    if (hexagons)
    {
        *gifTag = 0xD002400000008006;
        words[0] = 0x6000001A;
        words[2] = 0;
        words[3] = 0x6C198000;
        words[6] = 0x52152512;
        words[7] = 0x52521;
        last[3] = VifMscnt;
        return;
    }

    *gifTag = 0x9002400000008008;
    words[0] = 0x60000042;
    words[2] = 0;
    words[3] = 0x6C418000;
    words[6] = 0x35353513;
    words[7] = 5;
    last[0] = VifMscnt;
}

// The table's data for VU1: the gravity in the first packet's second quadword, the texture's corners (UV in 12.4: start, end
// and the two mixed) or the hexagons' corners from 0x40, then each step's two quadwords from 0x70: the shape's six floats with
// the colour and alpha in the low bytes of the first four
void WriteParticleRenderTable(u8* table, const ParticleLook& look)
{
    constexpr u32 GravityAt = 0x14;
    constexpr u32 CornersAt = 0x40;
    constexpr u32 StepsAt = 0x70;
    constexpr u32 StepBytes = 0x20;
    *reinterpret_cast<f32*>(table + GravityAt) = look.gravity;
    auto* corners = reinterpret_cast<s32*>(table + CornersAt);
    corners[4] = look.texture[0];
    corners[0] = look.texture[0];
    corners[6] = look.texture[2];
    corners[2] = look.texture[2];
    corners[3] = look.texture[1];
    corners[1] = look.texture[1];
    corners[7] = look.texture[3];
    corners[5] = look.texture[3];
    for (u32 step = 0; step < ParticleRenderSteps; step++)
    {
        auto* entry = reinterpret_cast<f32*>(table + StepsAt + step * StepBytes);
        for (u32 value = 0; value < 6; value++)
        {
            entry[value] = look.steps[step].shape[value];
        }

        auto* bytes = reinterpret_cast<u8*>(entry);
        for (u32 channel = 0; channel < 4; channel++)
        {
            bytes[channel * 4] = look.steps[step].colour[channel];
        }
    }

    auto* hexagon = reinterpret_cast<f32*>(table + CornersAt);
    if (!look.hexagons)
    {
        hexagon[11] = 0.0f;
        hexagon[8] = 0.0f;
        hexagon[9] = 0.0f;
        hexagon[10] = 0.0f;
        return;
    }

    for (u32 value = 0; value < 12; value++)
    {
        hexagon[value] = look.hexagonCorners[value];
    }
}

void DrawParticleBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time)
{
    ::DrawParticleBlock(block, reinterpret_cast<ParticleHeader*>(table), matrix, material, time);
}

void DrawDistortionBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time, f32 distortionX, f32 distortionY)
{
    DrawHexagonParticleBlock(block, reinterpret_cast<ParticleHeader*>(table), matrix, material, time, distortionX, distortionY);
}

// The view's quadwords: the chunk's four rows and four copies of the fifth, each four transposed, then the other chunk's matrix when
// there is one; the program to run on each decal in vi27 (its address in instructions)
void LoadDecalView(const Matrix4x4* chunkMatrices, const Matrix4x4* fromChunk)
{
    constexpr u32 SameChunkProgram = 0x1C0;
    constexpr u32 OtherChunkProgram = 0x360;
    alignas(16) Vu0Packet packet;
    alignas(16) u32 fifth[4];
    StartVu0Packet(&packet);
    u32* rows = packet.data[packet.count];
    packet.count = packet.count + 8;
    u32* copies = rows + 16;
    WriteChunkViewRows(fifth, chunkMatrices, rows);
    for (u32 copy = 0; copy < 4; copy++)
    {
        for (u32 word = 0; word < 4; word++)
        {
            copies[copy * 4 + word] = fifth[word];
        }
    }

    TransposeQuadwords(rows);
    TransposeQuadwords(copies);
    u32 program = SameChunkProgram;
    if (fromChunk != nullptr)
    {
        program = OtherChunkProgram;
        const auto* from = reinterpret_cast<const u32*>(fromChunk);
        for (u32 word = 0; word < 16; word++)
        {
            packet.data[packet.count + word / 4][word % 4] = from[word];
        }

        packet.unknown04 = 0;
        packet.count = packet.count + 4;
    }

    SendToVu0(g_Vu0Programs, packet.data, packet.count, 0);
    u32 start = program >> 3;
    asm volatile("ctc2.ni %0, $vi27" : : "r"(start));
}

// The type's first quadword made the DMA tag and UNPACK that send its variants to VU0 from quadword 0x10 (written through the
// uncached mirror, which the DMA reads), the transfer waited for
void LoadDecalType(DecalType* type)
{
    constexpr u32 Uncached = 0x20000000;
    constexpr u32 EndTag = 0x70000000;
    constexpr u32 UnpackVariants = 0x6C000010;
    constexpr u32 VariantQuadwords = 8;
    s32 count = *reinterpret_cast<const s32*>(reinterpret_cast<u8*>(type) + 0x790);
    auto* tag = reinterpret_cast<volatile u32*>(Address(type) | Uncached);
    tag[0] = 0;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = 0;
    tag[3] = static_cast<u32>(count) * VariantQuadwords << 16 | UnpackVariants;
    tag[0] = static_cast<u32>(count) * VariantQuadwords | EndTag;
    TransferDMA_Data(0, type, 1);
    while (IsVIF0_TransferingFromMemory(0) != 0)
    {
    }
}

// The decal in vf01-vf03 and its variant's address in vi01, the program in vi27 leaves its place in vf28, its colour in vf29 and
// its offsets in vf31 and vf30 (12.4: the half words are their low ones)
void AgeDecal(const f32* place, const s32* frame, s32 variant, DecalLook* look)
{
    constexpr s32 VariantsAddress = 0x10;
    constexpr s32 VariantQuadwords = 8;
    alignas(16) f32 in[4] = {place[0], place[1], place[2], place[3]};
    alignas(16) s32 normal[4] = {frame[0], frame[1], frame[2], frame[3]};
    alignas(16) s32 direction[4] = {frame[4], frame[5], frame[6], frame[7]};
    alignas(16) f32 out[4];
    alignas(16) u32 colour[4];
    alignas(16) u32 sizes[4];
    alignas(16) u32 moreSizes[4];
    s32 address = variant * VariantQuadwords + VariantsAddress;
    asm volatile("lqc2 $vf1, 0(%4)\n\t"
                 "lqc2 $vf2, 0(%5)\n\t"
                 "lqc2 $vf3, 0(%6)\n\t"
                 "ctc2.ni %7, $vi1\n\t"
                 "vcallmsr $vi27\n\t"
                 "vnop\n\t"
                 "sqc2 $vf31, 0(%2)\n\t"
                 "sqc2 $vf30, 0(%3)\n\t"
                 "sqc2 $vf29, 0(%1)\n\t"
                 "sqc2 $vf28, 0(%0)"
                 :
                 : "r"(out), "r"(colour), "r"(sizes), "r"(moreSizes), "r"(in), "r"(normal), "r"(direction), "r"(address)
                 : "memory");
    for (u32 value = 0; value < 4; value++)
    {
        look->place[value] = out[value];
        look->colour[value] = static_cast<u8>(colour[value]);
        look->sizes[value] = static_cast<s16>(sizes[value]);
        look->moreSizes[value] = static_cast<s16>(moreSizes[value]);
    }
}

void InitParticleGraphics()
{
    constexpr f32 WaveSpeed = 1.0f;
    constexpr f32 WaveAmplitude = 1.5f;
    g_EffectsDiskNode = -1;
    ShaderConstruct(&g_ParticleWaveShader);
    g_ParticleWaveShader.vtable = g_ShaderType1CVTable;
    g_ParticleWaveShader.speed = WaveSpeed;
    g_ParticleWaveShader.amplitude = WaveAmplitude;
    ShaderType1CSetUp(&g_ParticleWaveShader);
    MaterialConstruct(&g_DistortionMaterial);
}

s32 LoadParticleView(const Matrix4x4* matrices, s32 index)
{
    return UploadParticleView(matrices, index);
}

// A render table: a CNT tag of its first two quadwords (VIF1's FLUSH and UNPACK), a RET of the steps that follow (the same), and
// after them a CNT tag with VIF1's MSCAL
void InitParticleRenderTable(u8* table)
{
    auto* words = reinterpret_cast<u32*>(table);
    words[0] = 0;
    words[1] = 0;
    words[12] = 0;
    words[13] = 0;
    words[0] = CountTag | 2;
    words[2] = VifFlush;
    words[3] = 0x6C020053;
    words[12] = ReturnTag | 0x84;
    words[15] = 0x6C830055;
    words[14] = VifFlush;
    auto* end = reinterpret_cast<u32*>(table + 0x870);
    end[0] = 0;
    end[1] = 0;
    end[2] = 0;
    end[3] = 0;
    end[0] = CountTag;
    end[2] = VifMscal;
}
}

extern "C"
{
    // A particle page read from a stream (its texture and material read after their IDs from the stream, else its material
    // taken from the material table), its modes' materials made; and one loaded from its file (from the stream)
    void ReadParticlePageData(ParticlePage* page, Stream* stream, u32 fromStream, u32 decals) RETAIL(FUN_0019d9d8);
    void LoadParticlePageFile(ParticlePage* page, const char* path, u32 decals) RETAIL(FUN_001a1768);
    // The particles' wave shader moved on by the clock's last advance
    void UpdateParticleWaves(const TimeClock* clock) RETAIL(FUN_001b9a30);
}

namespace
{
// The pages' materials go in the particles' render bucket, and every mode's shader drops what's under 5/128 alpha (ATST GEQUAL 5,
// kept in neither buffer). The page material's and the blended modes' test depth GEQUAL without writing it
constexpr u32 ParticleBucket = 0x16;
constexpr u64 AlphaGreaterOrEqual = 5;
constexpr u64 AlphaKept = 5;
constexpr u64 DepthGreaterOrEqual = 2;
// The blended modes' presets: Cs·As + Cd and Cd - Cs·As
constexpr u64 PresetAdd = 1;
constexpr u64 PresetSubtract = 2;
// Settings bits the files have that nothing uses
constexpr u32 SettingUnused23 = 23;
constexpr u32 SettingUnused57 = 57;
constexpr u32 ShaderSettingsSlot = 5;
constexpr u32 MaterialShaders = 4;

// A page's blend modes: additive, subtractive, the page material's own and cutout
enum PageMode : u32
{
    ModeAdd,
    ModeSubtract,
    ModeOwn,
    ModeCutout,
};

u64 With(u64 settings, u32 shift, u32 width, u64 value)
{
    u64 mask = ((1ull << width) - 1) << shift;
    return (settings & ~mask) | value << shift;
}

u64 WithPageAlphaTest(u64 settings)
{
    settings = With(settings, SettingAlphaTest, 1, 1);
    settings = With(settings, SettingAlphaMethod, 3, AlphaGreaterOrEqual);
    settings = With(settings, SettingAlphaReference, 8, AlphaKept);
    return With(settings, SettingAlphaFail, 2, 0);
}

// What a mode's shader leaves out: the unused bits, the destination test, the scrolls, Gouraud shading, fog and the second context
u64 WithoutExtras(u64 settings)
{
    settings = With(settings, SettingUnused57, 1, 0);
    settings = With(settings, SettingDestinationTest, 1, 0);
    settings = With(settings, SettingUScroll, 3, 0);
    settings = With(settings, SettingVScroll, 3, 0);
    settings = With(settings, SettingUnused23, 3, 0);
    settings = With(settings, SettingGouraud, 1, 0);
    settings = With(settings, SettingFog, 1, 0);
    return With(settings, SettingSecondContext, 1, 0);
}

// A mode's material in the particles' bucket with the page material's programs' key, no shaders yet
Material* MakeModeMaterial(ParticlePage* page, PageMode mode, u64 activated)
{
    Material* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(Platform::Graphics::MaterialStorage)));
    page->materials[mode] = material;
    material->bucket = ParticleBucket;
    material->shaderCount = 0;
    material->activatedShaders = activated;
    return material;
}

// A shader of type 0x12 (made with its type's set-up), or of type 0x13 with STQ coordinates for the decals
Shader* MakePageShader(bool decals)
{
    auto* shader = ShaderConstruct(static_cast<Shader*>(MemoryAllocate(sizeof(Shader))));
    if (!decals)
    {
        shader->vtable = g_ShaderType12VTable;
        ShaderType12SetUp(shader);
        shader->settings = With(shader->settings, SettingStq, 1, 0);
        return shader;
    }

    shader->vtable = g_ShaderType13VTable;
    ShaderType13SetUp(shader);
    shader->settings = With(shader->settings, SettingStq, 1, 1);
    return shader;
}

void AddShader(Material* material, Shader* shader)
{
    if (material->shaderCount < MaterialShaders)
    {
        material->shaders[material->shaderCount] = shader;
        material->shaderCount++;
    }
}

// A mode's shader drawn with the FBA in a grey that leaves the texture as it is
void StartModeShader(Shader* shader)
{
    shader->settings = With(shader->settings, SettingNoFba, 1, 0);
    shader->shaderColour[0] = 0.5f;
    shader->shaderColour[3] = 1.0f;
    shader->shaderColour[2] = 0.5f;
    shader->shaderColour[1] = 0.5f;
}

void SetUpBlendedShader(Shader* shader, u64 preset)
{
    StartModeShader(shader);
    u64 settings = shader->settings;
    settings = With(settings, SettingBlends, 1, 1);
    settings = With(settings, SettingNoDepthWrites, 1, 1);
    settings = WithPageAlphaTest(settings);
    settings = With(settings, SettingDepthTest, 2, DepthGreaterOrEqual);
    settings = WithoutExtras(settings);
    settings = With(settings, SettingOwnAlpha, 1, 0);
    shader->settings = With(settings, SettingPreset, 4, preset);
}

// The page shader's texture (a reference of it taken) and mip choice given to a mode's shader, drawn textured, its GS settings
// made
void FinishModeShader(Shader* shader, const Shader* pageShader)
{
    SetShaderTexture(shader, HeaderOf(pageShader->texture)->id);
    shader->lodK = pageShader->lodK;
    shader->settings = With(shader->settings, SettingTextured, 1, 1);
    shader->lodL = pageShader->lodL;
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
}
}

// The page material's first shader is mode 2's, made to blend and test like the others. Mode 1 makes a shader of type 0x12 it
// drops, and mode 3 sets mode 1's depth test (which it has) and keeps its own constructor's
void ReadParticlePageData(ParticlePage* page, Stream* stream, u32 fromStream, u32 decals)
{
    stream->ReadS32(reinterpret_cast<s32*>(&page->textureId));
    stream->ReadS32(reinterpret_cast<s32*>(&page->materialId));
    if (fromStream != 0)
    {
        TextureFromStream(page->textureId, stream);
        page->material = MaterialFromStream(page->materialId, stream);
    }
    else
    {
        page->material = g_MaterialTable.Acquire(&page->materialId, nullptr);
    }

    Material* own = page->material->material;
    page->materials[ModeOwn] = own;
    own->bucket = ParticleBucket;
    u64 activated = page->materials[ModeOwn]->activatedShaders;
    Shader* pageShader = page->materials[ModeOwn] != nullptr ? page->materials[ModeOwn]->shaders[0] : nullptr;
    u64 settings = pageShader->settings;
    settings = With(settings, SettingBlends, 1, 1);
    settings = With(settings, SettingNoDepthWrites, 1, 1);
    settings = WithPageAlphaTest(settings);
    pageShader->settings = With(settings, SettingDepthTest, 2, DepthGreaterOrEqual);
    CallVirtual<void>(pageShader, pageShader->vtable, ShaderSettingsSlot);

    Material* material = MakeModeMaterial(page, ModeAdd, activated);
    Shader* shader = MakePageShader(decals != 0);
    AddShader(material, shader);
    SetUpBlendedShader(shader, PresetAdd);
    FinishModeShader(shader, pageShader);

    material = MakeModeMaterial(page, ModeSubtract, activated);
    auto* dropped = ShaderConstruct(static_cast<Shader*>(MemoryAllocate(sizeof(Shader))));
    dropped->vtable = g_ShaderType12VTable;
    ShaderType12SetUp(dropped);
    Shader* subtractive = MakePageShader(decals != 0);
    AddShader(material, subtractive);
    SetUpBlendedShader(subtractive, PresetSubtract);
    FinishModeShader(subtractive, pageShader);

    material = MakeModeMaterial(page, ModeCutout, activated);
    shader = MakePageShader(decals != 0);
    AddShader(material, shader);
    StartModeShader(shader);
    settings = shader->settings;
    settings = With(settings, SettingBlends, 1, 0);
    settings = With(settings, SettingNoDepthWrites, 1, 0);
    shader->settings = WithPageAlphaTest(settings);
    subtractive->settings = With(subtractive->settings, SettingDepthTest, 2, DepthGreaterOrEqual);
    shader->settings = WithoutExtras(shader->settings);
    FinishModeShader(shader, pageShader);
}

void LoadParticlePageFile(ParticlePage* page, const char* path, u32 decals)
{
    MemoryStream stream;
    MemoryStream::ConstructFromFile(&stream, path, false);
    ReadParticlePageData(page, &stream, 1, decals);
    stream.Destroy(DestroyOnly);
}

void UpdateParticleWaves(const TimeClock* clock)
{
    ShaderType1CUpdate(&g_ParticleWaveShader, static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit);
}

Vu0Packet* StartVu0Packet(Vu0Packet* packet)
{
    packet->capacity = 16;
    packet->count = 0;
    packet->unknown04 = 0;
    return packet;
}

namespace Platform::Graphics
{
void LoadParticlePage(ParticlePage* page, const char* path, bool decals)
{
    LoadParticlePageFile(page, path, decals ? 1 : 0);
}

void ReadParticlePage(ParticlePage* page, Stream* stream, bool decals)
{
    ReadParticlePageData(page, stream, 0, decals ? 1 : 0);
}
}
