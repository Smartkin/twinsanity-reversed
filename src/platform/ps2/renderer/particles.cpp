#include "renderer.h"

#include "abi.h"
#include "game/clock.h"
#include "game/graphicstables.h"
#include "game/memory.h"
#include "game/particles.h"
#include "game/stream.h"
#include "platform/graphics.h"

#include <libgs.h>

namespace
{
// Where the particles' and the decals' data goes in VU1's memory: the matrix to the screen (four quadwords), then a render
// table's head's two quadwords (or the decals' UV packet), then the table's corners and steps
constexpr u32 VuMatrixAddress = 0x4F;
constexpr u32 VuHeadAddress = 0x53;
constexpr u32 VuStepsAddress = 0x55;
constexpr u32 MatrixQuadwords = 4;
constexpr u32 MatrixUnpack = VifUnpackTo(VifUnpackV4Count, VuMatrixAddress, MatrixQuadwords);
// The hexagons' distortion VU1 gets: the system's across and down (by these), and the depth they fade out to nothing at
constexpr f32 DistortionAcross = 0x1p-14f;
constexpr f32 DistortionDown = -0x1p-12f;
constexpr f32 DistortionFadeDepth = 40.0f;
// VU1's programs collapse the particles whose clip space W is below this
constexpr f32 ParticleNearClip = 0.5f;
// Where a decal type's variants go in VU0's memory, and their size
constexpr u32 Vu0VariantsAddress = 0x10;
constexpr u32 VariantQuadwords = sizeof(DecalVariant) / 0x10;

// A system's render table (Platform::Graphics::ParticleRenderTableBytes): its head, the texture's corners (UV in 12.4: start, end
// and the two mixed) or the hexagons' corners, each step's shape (six floats, the colour's and alpha's bytes over the low bytes
// of the first four) and a quadword of VIF1's codes after them (FLUSHE and MSCAL, what VU1 runs on them), which its head's RET
// tag sends with them
struct ParticleRenderTable
{
    struct Step
    {
        f32 shape[6];
        u32 unused18[2];
    };

    ParticleHeader head;
    s32 corners[12];
    Step steps[Platform::Graphics::ParticleRenderSteps];
    u32 end[4];
};
CHECK_OFFSET(ParticleRenderTable, steps, 0x70);
CHECK_OFFSET(ParticleRenderTable, end, 0x870);
static_assert(sizeof(ParticleRenderTable) <= Platform::Graphics::ParticleRenderTableBytes);

// The writer's packet of a particle block: the matrix, the table's head's DMA tag and its two quadwords (the hexagons'
// distortion in the second), CALLs of the head's RET tag (the table's corners and steps) and of the particles
void WriteParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material)
{
    RenderBucket& writer = material->writer;
    auto* at = BeginPacket(writer);
    at[0] = CountTag | MatrixQuadwords;
    at[1] = 0;
    at[2] = 0;
    at[3] = MatrixUnpack;
    *reinterpret_cast<Matrix4x4*>(at + 4) = *matrix;
    auto* quadwords = reinterpret_cast<Vector4*>(at + 20);
    quadwords[0] = *reinterpret_cast<const Vector4*>(header->tag);
    quadwords[1] = *reinterpret_cast<const Vector4*>(&header->nearClip);
    quadwords[2] = *reinterpret_cast<const Vector4*>(header->distortion);
    at[32] = CallTag;
    at[33] = Address(header->stepsTag);
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
    void DrawParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 time)
    {
        if (material->writer.first == nullptr)
        {
            StartMaterialWriter(material);
        }

        header->nearClip = ParticleNearClip;
        header->time = time;
        WriteParticleBlock(block, header, matrix, material);
    }

    void DrawHexagonParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 time,
                                  f32 distortionX, f32 distortionY)
    {
        if (material->writer.first == nullptr)
        {
            StartMaterialWriter(material);
        }

        header->nearClip = ParticleNearClip;
        header->time = time;
        header->distortion[0] = distortionX * DistortionAcross;
        header->distortion[1] = distortionY * DistortionDown;
        header->distortion[2] = DistortionFadeDepth;
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
        for (u32 key = 0; key < DecalData::KeyCount; key++)
        {
            DecalBlock* decal = decals->drawLists[key];
            decals->drawLists[key] = nullptr;
            if (decal == nullptr)
            {
                continue;
            }

            Material* material = decals->page.materials[key];
            if (material->writer.first == nullptr)
            {
                StartMaterialWriter(material);
            }

            // The matrix, the UV packet by REFS (its GIF tag's quadword and two rectangles for each type and one more; VU1 runs
            // the program at 0 on it), and each block's quadwords from its places' UNPACK on
            constexpr u32 BlockQuadwords = (sizeof(DecalBlock) - offsetof(DecalBlock, placesUnpack)) / 0x10;
            RenderBucket& writer = material->writer;
            u32* at = BeginPacket(writer);
            at[0] = CountTag | MatrixQuadwords;
            at[1] = 0;
            at[2] = 0;
            at[3] = MatrixUnpack;
            *reinterpret_cast<Matrix4x4*>(at + 4) = toScreen;
            at += 20;
            u32 size = (decals->typeCount + 1) * 2 + 1;
            at[0] = size | ReferenceStallTag;
            at[1] = Address(&decals->uvPacket);
            at[2] = 0;
            at[3] = VifUnpackTo(VifUnpackV4Count, VuHeadAddress, size);
            at[4] = CountTag;
            at[5] = 0;
            at[6] = 0;
            at[7] = VifMscal;
            at += 8;
            for (; decal != nullptr; decal = decal->drawNext)
            {
                at[0] = ReferenceStallTag | BlockQuadwords;
                at[1] = Address(decal->placesUnpack);
                at[2] = 0;
                at[3] = 0;
                at += 4;
            }

            EndPacket(writer, reinterpret_cast<u8*>(at));
        }
    }
}

// A packet of quadwords for VU0's memory as the asm makes it (BigVu0Packet's smaller kind): how many there are (a word after the
// count is written 0 with it), and room for them
struct Vu0Packet
{
    static constexpr s32 Capacity = 16;

    s32 count;
    s32 unused04;
    s32 capacity;
    s32 unused0C;
    u32 data[Capacity][4];
};

extern "C"
{
    // A packet started: no quadwords
    Vu0Packet* StartVu0Packet(Vu0Packet* packet) RETAIL(FUN_001ba2f0);
    // The disk node of the screen effects' buffers (none at start-up), the wave shader the particles' start-up makes and type
    // 0x1C's vtable
    extern s32 g_EffectsDiskNode RETAIL(D_0030A848);
    extern WaveShader g_ParticleWaveShader RETAIL(G_PrecompShader_0x1C_3323C0);
    extern const GccVTableEntry g_ShaderType1CVTable[] RETAIL(PrecompiledShader__Type_0x1C_Methods);
    extern Material g_DistortionMaterial RETAIL(D_00370758);
}

EABI_EXPORT(DrawParticleBlock, DrawParticleBlock);
EABI_EXPORT(DrawHexagonParticleBlock, DrawHexagonParticleBlock);

namespace Platform::Graphics
{
// A block's first quadword is its DMA tag (its ID replaced, the rest kept): RET ends the chain, NEXT goes on at the address in
// its second word (main memory's: SPR, bit 31, clear)
constexpr u32 TagId = 0x70000000;

void EndParticleBlock(u8* block)
{
    auto* tag = reinterpret_cast<u32*>(block);
    tag[1] = 0;
    tag[0] = (tag[0] & ~TagId) | ReturnTag;
}

void ChainParticleBlocks(u8* const* blocks, s32 count)
{
    EndParticleBlock(blocks[count - 1]);
    for (s32 block = count - 2; block >= 0; block--)
    {
        auto* tag = reinterpret_cast<u32*>(blocks[block]);
        tag[0] = (tag[0] & ~TagId) | NextTag;
        tag[1] = Address(blocks[block + 1]) & ~DmaScratchpad;
    }
}

// A block: a RET tag of its quadwords with VIF1's UNPACK of all but the last (to VU1's double buffer's place), the GIF tag of
// their draws (triangle strips, PRIM preset, ending the GIF's packet: 8 loops of a particle's four corners, UV, RGBAQ and XYZ2
// then UV and XYZ2 for the other three, or 6 loops of a hexagon's 13 registers) and its registers, the particles, and the last
// quadword's VIF1 codes: an MSCNT starting the VU1 program (the hexagons' in the fourth word)
void InitParticleBlock(u8* block, bool hexagons)
{
    constexpr u32 ParticleLoops = 8;
    constexpr u32 HexagonLoops = 6;
    constexpr u32 ParticleRegisterCount = 9;
    constexpr u32 HexagonRegisterCount = 13;
    constexpr u64 ParticleRegisters = GifDescriptors(GifUv, GifRgbaq, GifXyz2, GifUv, GifXyz2, GifUv, GifXyz2, GifUv, GifXyz2);
    constexpr u64 HexagonRegisters = GifDescriptors(GifSt, GifRgbaq, GifXyz2, GifSt, GifXyz2, GifRgbaq, GifSt, GifXyz2, GifRgbaq,
                                                    GifSt, GifXyz2, GifSt, GifXyz2);
    auto* words = reinterpret_cast<u32*>(block);
    auto* gifTag = reinterpret_cast<GifTag*>(block + 0x10);
    u32 quadwords = (hexagons ? HexagonBlockBytes : ParticleBlockBytes) / 0x10 - 1;
    auto* last = reinterpret_cast<u32*>(block + quadwords * 0x10);
    last[0] = 0;
    last[1] = 0;
    last[2] = 0;
    last[3] = 0;
    words[0] = 0;
    words[1] = 0;
    GifTag tag = {};
    tag.endOfPacket = 1;
    tag.setsPrim = 1;
    tag.prim = GS_PRIM_TRI_STRIP;
    if (hexagons)
    {
        tag.loops = HexagonLoops;
        tag.registerCount = HexagonRegisterCount;
        gifTag[0] = tag;
        words[0] = ReturnTag | quadwords;
        words[2] = 0;
        words[3] = VifUnpackTo(VifUnpackV4Count | VifTops, 0, quadwords - 1);
        *reinterpret_cast<u64*>(words + 6) = HexagonRegisters;
        last[3] = VifMscnt;
        return;
    }

    tag.loops = ParticleLoops;
    tag.registerCount = ParticleRegisterCount;
    gifTag[0] = tag;
    words[0] = ReturnTag | quadwords;
    words[2] = 0;
    words[3] = VifUnpackTo(VifUnpackV4Count | VifTops, 0, quadwords - 1);
    *reinterpret_cast<u64*>(words + 6) = ParticleRegisters;
    last[0] = VifMscnt;
}

// The table's data for VU1: the gravity in its head, the texture's corners or the hexagons' corners (none for other systems),
// then each step's shape with the colour and alpha
void WriteParticleRenderTable(u8* table, const ParticleLook& look)
{
    auto* render = reinterpret_cast<ParticleRenderTable*>(table);
    render->head.gravity = look.gravity;
    s32* corners = render->corners;
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
        f32* entry = render->steps[step].shape;
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

    auto* hexagon = reinterpret_cast<f32*>(render->corners);
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

// The view's quadwords: the chunk's four rows and four copies of the fifth, each four transposed, then the other chunk's matrix
// when there is one; the program to run on each decal in vi27 (its address in instructions)
void LoadDecalView(const Matrix4x4* chunkMatrices, const Matrix4x4* fromChunk)
{
    constexpr u32 SameChunkProgram = 0x1C0;
    constexpr u32 OtherChunkProgram = 0x360;
    constexpr u32 InstructionShift = 3;
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

        packet.unused04 = 0;
        packet.count = packet.count + 4;
    }

    SendToVu0(g_Vu0Programs, packet.data, packet.count, 0);
    u32 start = program >> InstructionShift;
    asm volatile("ctc2.ni %0, $vi27" : : "r"(start));
}

// The type's first quadword made the DMA tag and UNPACK that send its variants to VU0's memory (written through the uncached
// mirror, which the DMA reads), the transfer waited for
void LoadDecalType(DecalType* type)
{
    s32 count = type->variantCount;
    auto* tag = reinterpret_cast<volatile u32*>(Address(type) | UncachedSegment);
    tag[0] = 0;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = 0;
    tag[3] = VifUnpackTo(VifUnpackV4Count, Vu0VariantsAddress, static_cast<u32>(count) * VariantQuadwords);
    tag[0] = static_cast<u32>(count) * VariantQuadwords | EndTag;
    StartDmaChain(Vif0Channel, type, true);
    while (IsDmaChannelBusy(Vif0Channel))
    {
    }
}

void DrawDecals(DecalData* decals)
{
    ::DrawDecals(decals);
}

// The decal in vf01-vf03 and its variant's address in vi01, the program in vi27 leaves its place in vf28, its colour in vf29 and
// its offsets in vf31 and vf30 (12.4: the half words are their low ones)
void AgeDecal(const f32* place, const s32* frame, s32 variant, DecalLook* look)
{
    alignas(16) f32 in[4] = {place[0], place[1], place[2], place[3]};
    alignas(16) s32 normal[4] = {frame[0], frame[1], frame[2], frame[3]};
    alignas(16) s32 direction[4] = {frame[4], frame[5], frame[6], frame[7]};
    alignas(16) f32 out[4];
    alignas(16) u32 colour[4];
    alignas(16) u32 sizes[4];
    alignas(16) u32 moreSizes[4];
    auto address = static_cast<s32>(variant * VariantQuadwords + Vu0VariantsAddress);
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
    return UploadParticleView(const_cast<Matrix4x4*>(matrices), index);
}

// A render table: a CNT tag of its head's two quadwords (VIF1's FLUSH and UNPACK), a RET of the corners and steps that follow
// (the same) and of their VIF1 codes after them (FLUSHE, as the word of a CNT tag reads, and MSCAL)
void InitParticleRenderTable(u8* table)
{
    constexpr u32 HeadQuadwords = 2;
    constexpr u32 StepsQuadwords = (offsetof(ParticleRenderTable, end) - offsetof(ParticleRenderTable, corners)) / 0x10;
    auto* render = reinterpret_cast<ParticleRenderTable*>(table);
    u32* tag = render->head.tag;
    u32* stepsTag = render->head.stepsTag;
    tag[0] = 0;
    tag[1] = 0;
    stepsTag[0] = 0;
    stepsTag[1] = 0;
    tag[0] = CountTag | HeadQuadwords;
    tag[2] = VifFlush;
    tag[3] = VifUnpackTo(VifUnpackV4Count, VuHeadAddress, HeadQuadwords);
    stepsTag[0] = ReturnTag | (StepsQuadwords + 1);
    stepsTag[3] = VifUnpackTo(VifUnpackV4Count, VuStepsAddress, StepsQuadwords);
    stepsTag[2] = VifFlush;
    u32* end = render->end;
    end[0] = 0;
    end[1] = 0;
    end[2] = 0;
    end[3] = 0;
    end[0] = VifFlushE;
    end[2] = VifMscal;
}
}

extern "C"
{
    // A particle page read from a stream (its texture and material read after their IDs from the stream, else its material
    // taken from the material table), its modes' materials made; and one loaded from its file (from the stream)
    void ReadParticlePageData(ParticlePage* page, Stream* stream, u32 fromStream, u32 decals) RETAIL(FUN_0019d9d8);
    void LoadParticlePageFile(ParticlePage* page, const char* path, u32 decals) RETAIL(FUN_001a1768);
}

namespace
{
// Every mode's shader drops what's under 5/128 alpha (ATST GEQUAL 5, kept in neither buffer). The page material's and the blended
// modes' test depth GEQUAL without writing it
constexpr u64 AlphaThreshold = 5;

void SetPageAlphaTest(ShaderSettings& settings)
{
    settings.alphaTest = 1;
    settings.alphaMethod = GS_ALPHA_GEQUAL;
    settings.alphaReference = AlphaThreshold;
    settings.alphaFail = GS_ALPHA_NO_UPDATE;
}

// What a mode's shader leaves out: the unused bits, the destination test, the scrolls, Gouraud shading, fog and the second
// context
void LeaveOutExtras(ShaderSettings& settings)
{
    settings.unused57 = 0;
    settings.destinationTest = 0;
    settings.uScroll = ScrollNone;
    settings.vScroll = ScrollNone;
    settings.unused23 = 0;
    settings.gouraud = 0;
    settings.fog = 0;
    settings.secondContext = 0;
}

// A mode's material in the particles' bucket with the page material's programs' key, no shaders yet
Material* MakeModeMaterial(ParticlePage* page, ParticleBlendMode mode, u64 activated)
{
    Material* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(Platform::Graphics::MaterialStorage)));
    page->materials[mode] = material;
    material->bucket = BucketParticles;
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
        shader->settings.stq = 0;
        return shader;
    }

    shader->vtable = g_ShaderType13VTable;
    ShaderType13SetUp(shader);
    shader->settings.stq = 1;
    return shader;
}

void AddShader(Material* material, Shader* shader)
{
    if (material->shaderCount < MaxMaterialShaders)
    {
        material->shaders[material->shaderCount] = shader;
        material->shaderCount++;
    }
}

// A mode's shader drawn with the FBA in a grey that leaves the texture as it is
void StartModeShader(Shader* shader)
{
    shader->settings.noFba = 0;
    shader->shaderColour[0] = 0.5f;
    shader->shaderColour[3] = 1.0f;
    shader->shaderColour[2] = 0.5f;
    shader->shaderColour[1] = 0.5f;
}

void SetUpBlendedShader(Shader* shader, AlphaPreset preset)
{
    StartModeShader(shader);
    ShaderSettings& settings = shader->settings;
    settings.blends = 1;
    settings.noDepthWrites = 1;
    SetPageAlphaTest(settings);
    settings.depthTest = GS_ZBUFF_GEQUAL;
    LeaveOutExtras(settings);
    settings.ownAlpha = 0;
    settings.preset = preset;
}

// The page shader's texture (a reference of it taken) and mip choice given to a mode's shader, drawn textured, its GS settings
// made
void FinishModeShader(Shader* shader, const Shader* pageShader)
{
    SetShaderTexture(shader, HeaderOf(pageShader->texture)->id);
    shader->lodK = pageShader->lodK;
    shader->settings.textured = 1;
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
    page->materials[BlendPageMaterial] = own;
    own->bucket = BucketParticles;
    u64 activated = page->materials[BlendPageMaterial]->activatedShaders;
    Shader* pageShader = page->materials[BlendPageMaterial] != nullptr ? page->materials[BlendPageMaterial]->shaders[0] : nullptr;
    pageShader->settings.blends = 1;
    pageShader->settings.noDepthWrites = 1;
    SetPageAlphaTest(pageShader->settings);
    pageShader->settings.depthTest = GS_ZBUFF_GEQUAL;
    CallVirtual<void>(pageShader, pageShader->vtable, ShaderSettingsSlot);

    Material* material = MakeModeMaterial(page, BlendAdditive, activated);
    Shader* shader = MakePageShader(decals != 0);
    AddShader(material, shader);
    SetUpBlendedShader(shader, PresetAdd);
    FinishModeShader(shader, pageShader);

    material = MakeModeMaterial(page, BlendSubtractive, activated);
    auto* dropped = ShaderConstruct(static_cast<Shader*>(MemoryAllocate(sizeof(Shader))));
    dropped->vtable = g_ShaderType12VTable;
    ShaderType12SetUp(dropped);
    Shader* subtractive = MakePageShader(decals != 0);
    AddShader(material, subtractive);
    SetUpBlendedShader(subtractive, PresetSubtract);
    FinishModeShader(subtractive, pageShader);

    material = MakeModeMaterial(page, BlendCutout, activated);
    shader = MakePageShader(decals != 0);
    AddShader(material, shader);
    StartModeShader(shader);
    shader->settings.blends = 0;
    shader->settings.noDepthWrites = 0;
    SetPageAlphaTest(shader->settings);
    subtractive->settings.depthTest = GS_ZBUFF_GEQUAL;
    LeaveOutExtras(shader->settings);
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
    packet->capacity = Vu0Packet::Capacity;
    packet->count = 0;
    packet->unused04 = 0;
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
