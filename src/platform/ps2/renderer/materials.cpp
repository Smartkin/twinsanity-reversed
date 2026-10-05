#include "renderer.h"

#include "game/memory.h"
#include "game/particles.h"

#include "gcc2.h"
#include "platform/graphics.h"

#include <libgs.h>

namespace
{
// A material's key of 1 never loads programs
constexpr u64 NoProgramsKey = 1;
// Shaders' programs from 15 on are loaded when a material needs them
constexpr s32 FirstLoadedShaderProgram = 0xF;
// The program whose entry the material's packets hand VU1 for the second parameter
constexpr u32 SecondParameterProgram = 11;

// The entries VU1 goes on to, by UNPACK at the counter's places (it counts them): the program's (and with it a place past it),
// and program 11's when clipped. The elements' fourth words aren't written: VU1 gets what the buffer had there
u8* WriteParameterEntries(u8* packet, u32* counter, u32 mode, u32 program, s16 offset)
{
    u32 count = mode == DrawClipped ? 2 : 1;
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | count;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count, *counter, count);
    at += 4;
    u32 place = ++*counter;
    u32 address = g_VuPrograms[program].address;
    at[0] = address;
    at[2] = place;
    at[1] = address + offset;
    at += 4;
    if (mode == DrawClipped)
    {
        place = ++*counter;
        at[0] = g_VuPrograms[SecondParameterProgram].address;
        at[2] = place;
        at[1] = 0;
        at += 4;
    }

    return reinterpret_cast<u8*>(at);
}

// Whether the material loads its shaders' programs: not when it's keyed like its bucket's last material (or keyed 1), which
// loaded them. They go into the bucket's region of VU1's micro memory, from its start
bool StartsPrograms(const Material* material, RenderBucket& bucket)
{
    u64 key = material->activatedShaders;
    if (key == NoProgramsKey || key == bucket.lastKey)
    {
        g_VuProgramBucket = material->bucket;
        return false;
    }

    bucket.lastKey = key;
    u32 start = bucket.programRegion == 1 ? g_VuProgramRegion1 : g_VuProgramRegion2;
    g_VuProgramNext = start;
    g_VuProgramTime++;
    g_VuProgramBucket = material->bucket;
    g_VuProgramStart = start;
    return true;
}

// The programs the shaders need beyond the ones always loaded
u8* LoadPrograms(const Material* material, u8* packet)
{
    for (u32 i = 0; i < material->shaderCount; i++)
    {
        Shader* shader = material->shaders[i];
        s32 program = CallVirtual<s32>(shader, shader->vtable, ShaderProgramSlot);
        if (program >= FirstLoadedShaderProgram)
        {
            packet = LoadProgramForMaterial(packet, static_cast<u32>(program));
        }
    }

    return packet;
}

// VU1's two buffers take turns: returns where the material's data starts in the one it takes
u32 TakeBuffer(RenderBucket& bucket)
{
    u32 counter = bucket.vuBuffer != 0 ? g_VuBuffer1 : g_VuBuffer2;
    bucket.vuBuffer = bucket.vuBuffer != 1;
    return counter;
}

// Each shader's texture uploaded and its packet
u8* WriteShaders(const Material* material, u8* packet, u32* counter)
{
    for (u32 i = 0; i < material->shaderCount; i++)
    {
        Shader* shader = material->shaders[i];
        if (shader->texture != nullptr)
        {
            packet = UploadTexture(&g_TextureUploadContext, packet, TextureOf(shader), material->bucket);
        }

        packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, counter, 1u);
    }

    return packet;
}

// EndProgram's entry at the counter (VIF1's code before the UNPACK given)
u8* WriteEndEntry(u8* packet, u32 counter, u32 vif)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | 1;
    at[1] = 0;
    at[2] = vif;
    at[3] = counter | VifUnpackV4;
    at[4] = g_VuPrograms[EndProgram].address;
    at[5] = 0;
    at[6] = 0;
    at[7] = 0;
    return packet + 0x20;
}

// The buffer VU1 takes next
u8* WriteNextBuffer(u8* packet, const RenderBucket& bucket)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag;
    at[1] = 0;
    at[2] = g_VuBufferPlace | VifUnpackS;
    at[3] = bucket.vuBuffer != 0 ? g_VuBuffer2 : g_VuBuffer1;
    return packet + 0x10;
}

// The CALL the material's drawing shares, unless the bucket's last material's was the same
u8* WriteCallUnlessRepeated(const Material* material, RenderBucket& bucket, u8* packet)
{
    if (bucket.lastCall == material->call)
    {
        return packet;
    }

    bucket.lastCall = material->call;
    return WriteMaterialCall(material, packet);
}

// The writer's draws follow the bucket's packet
void Splice(RenderBucket& bucket, const RenderBucket& writer)
{
    bucket.last[1] = Address(writer.first);
    bucket.last = writer.last;
}

// A single shader's set-up: its program loaded whatever it is (the material's of several shaders skip the ones always loaded),
// its packet, EndProgram's entry, its texture uploaded and its registers, its GS registers sent to the GIF directly, the CALL
u8* WriteSingleShader(const Material* material, RenderBucket& bucket, u8* packet)
{
    Shader* shader = material->shaders[0];
    if (StartsPrograms(material, bucket))
    {
        packet = LoadProgramForMaterial(packet, CallVirtual<u32>(shader, shader->vtable, ShaderProgramSlot));
    }

    u32 counter = TakeBuffer(bucket);
    packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, &counter, 0u);
    packet = WriteEndEntry(packet, counter, 0);
    if (shader->texture != nullptr)
    {
        Texture* texture = TextureOf(shader);
        packet = UploadTexture(&g_TextureUploadContext, packet, texture, material->bucket);
        packet = WriteTextureRegisters(&g_TextureUploadContext, packet, texture);
    }

    // A REF tag of the registers (VIF1's FLUSHA and DIRECT of them); the asm asks their size twice
    u32 registers = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSlot);
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | ReferenceTag;
    at[1] = registers;
    at[2] = VifFlushA;
    at[3] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | VifDirect;
    return WriteCallUnlessRepeated(material, bucket, packet + 0x10);
}
}

extern "C"
{
    u8* WriteSharedCall(u8* packet, u32 call)
    {
        u32 target;
        if (call == CallNone)
        {
            return packet;
        }

        if (call == CallSharedGifTag)
        {
            target = g_SharedGifPacket;
        }
        else if (call == CallRenderTarget)
        {
            target = Address(g_RenderTarget->packet);
        }
        else
        {
            return packet;
        }

        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CallTag;
        at[1] = target;
        at[2] = 0;
        at[3] = 0;
        return packet + 0x10;
    }

    u8* WriteMaterialCall(const Material* material, u8* packet)
    {
        return WriteSharedCall(packet, material->call);
    }

    u8* WriteMaterialEntries(u8* packet, u32* counter, u32 mode)
    {
        return WriteParameterEntries(packet, counter, mode, g_ParameterProgram, g_ParameterProgramOffset);
    }

    u8* WriteBlendSkinEntries(u8* packet, u32* counter, u32 mode)
    {
        return WriteParameterEntries(packet, counter, mode, g_BlendParameterProgram, g_BlendParameterProgramOffset);
    }

    u8* RenderMaterial(Material* material, u8* packet, u32 mode)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        if (StartsPrograms(material, bucket))
        {
            packet = LoadPrograms(material, packet);
        }

        u32 counter = TakeBuffer(bucket);
        packet = WriteMaterialCall(material, packet);
        packet = WriteMaterialEntries(packet, &counter, mode);
        packet = WriteShaders(material, packet, &counter);
        packet = WriteEndEntry(packet, counter, 0);
        return WriteNextBuffer(packet, bucket);
    }

    u8* RenderBlendSkinMaterial(Material* material, u8* packet, u32* counter, u32 mode)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        if (StartsPrograms(material, bucket))
        {
            packet = LoadPrograms(material, packet);
        }

        packet = WriteMaterialCall(material, packet);
        packet = WriteBlendSkinEntries(packet, counter, mode);
        // The asm uploads the first shader's texture for every shader
        for (u32 i = 0; i < material->shaderCount; i++)
        {
            if (material->shaders[0]->texture != nullptr)
            {
                packet = UploadTexture(&g_TextureUploadContext, packet, TextureOf(material->shaders[0]), material->bucket);
            }

            Shader* shader = material->shaders[i];
            packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, counter, 1u);
        }

        packet = WriteEndEntry(packet, *counter, 0);
        // VU1 takes its second buffer next, as always for blend skins
        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CountTag;
        at[1] = 0;
        at[2] = g_VuBufferPlace | VifUnpackS;
        at[3] = g_VuBuffer2;
        return packet + 0x10;
    }

    u8* RenderSkyMaterial(Material* material, u8* packet, u32)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        if (StartsPrograms(material, bucket))
        {
            packet = LoadPrograms(material, packet);
        }

        u32 counter = TakeBuffer(bucket);
        packet = WriteShaders(material, packet, &counter);
        packet = WriteEndEntry(packet, counter, 0);
        packet = WriteNextBuffer(packet, bucket);
        bucket.lastCall = CallNone;
        return packet;
    }

    void FlushMaterial(Material* material)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
        if (material->shaderCount < 2)
        {
            packet = WriteSingleShader(material, bucket, packet);
        }
        else
        {
            packet = WriteCallUnlessRepeated(material, bucket, packet);
            if (StartsPrograms(material, bucket))
            {
                packet = LoadPrograms(material, packet);
            }

            u32 counter = TakeBuffer(bucket);
            packet = WriteShaders(material, packet, &counter);
            packet = WriteEndEntry(packet, counter, VifFlushE);
        }

        packet = WriteNextBuffer(packet, bucket);
        EndPacket(bucket, packet);
        Splice(bucket, material->writer);
    }

    void StartMaterialDirectly(Material* material)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        material->drawnDirectly = 1;
        auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
        Shader* shader = material->shaders[0];
        if (StartsPrograms(material, bucket))
        {
            packet = LoadProgramForMaterial(packet, CallVirtual<u32>(shader, shader->vtable, ShaderProgramSlot));
        }

        u32 counter = TakeBuffer(bucket);
        packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, &counter, 0u);
        packet = WriteEndEntry(packet, counter, 0);
        packet = WriteNextBuffer(packet, bucket);
        EndPacket(bucket, packet);
        Splice(bucket, material->writer);
        // Nothing of it was a CALL: the bucket's next material makes its own
        bucket.lastCall = CallNone;
    }

    u32 FlushMaterials()
    {
        for (u32 i = 0; i < g_RenderedMaterialCount; i++)
        {
            Material* material = g_RenderedMaterials[i];
            if (material->drawnDirectly == 0)
            {
                FlushMaterial(material);
            }
        }

        return 1;
    }

    void ForgetMaterials()
    {
        for (u32 i = 0; i < g_RenderedMaterialCount; i++)
        {
            Material* material = g_RenderedMaterials[i];
            RenderBucket& writer = material->writer;
            writer.first = nullptr;
            writer.lastSlotAddress = 0;
            writer.vuBuffer = 0;
            writer.unused0D = 0;
            writer.programRegion = 0;
            writer.last = nullptr;
            writer.insertion = nullptr;
            writer.chain = 0;
            writer.lastKey = 0;
            writer.last2DMaterial = nullptr;
            writer.unused28 = 0;
            writer.lastJoints = 0;
            writer.lastCall = CallNone;
            material->drawnDirectly = 0;
            material->listed = 0;
            material->unused65 = 0;
        }

        g_RenderedMaterialCount = 0;
    }

    void ClearRenderedMaterials()
    {
        g_RenderedMaterialCount = 0;
    }

    void StartMaterialWriter(Material* material)
    {
        ListMaterial(material);
        StartWriter(&material->writer, material->bucket);
    }

    void StartMaterialWriterTwoTags(Material* material)
    {
        ListMaterial(material);
        StartWriterTwoTags(&material->writer, material->bucket);
    }
}

void Platform::Graphics::FinishScene(bool effects)
{
    if (effects)
    {
        DrawScreenEffects();
    }

    CloseInstanceBlocks();
    FlushMaterials();
    ForgetMaterials();
    FreeTextureSlots(&g_TextureUploadContext, 0);
}

extern "C" Material* MaterialConstruct(Material* material)
{
    material->shaderCount = 0;
    RenderBucketConstruct(&material->writer);
    material->call = CallRenderTarget;
    material->listed = 0;
    material->unused65 = 0;
    material->drawnDirectly = 0;
    return material;
}

extern "C" void MaterialDestroy(Material* material, u32 flags)
{
    for (u32 index = 0; index < material->shaderCount; index++)
    {
        Shader* shader = material->shaders[index];
        if (shader != nullptr)
        {
            CallVirtual<void>(shader, shader->vtable, ShaderDestroySlot, DestroyAndFree);
        }
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(material);
    }
}

// The UI's flat material: a shader of type 0xE (no texture, the vertexes' colours) of its own in the UI's bucket, made once by OLEG
extern "C"
{
    extern Material g_FlatMaterial RETAIL(G_PrecompShader_0xE);
    extern Shader* g_FlatShader RETAIL(G_PrecompShader_0xE_2);
    extern const GccVTableEntry g_FlatShaderVTable[] RETAIL(PrecompiledShader__Type__0xE_Methods);
    extern const GccVTableEntry g_DistortionShaderVTable[] RETAIL(D_002FA6C8);
}

namespace
{
constexpr u32 FlatShaderSize = 0x70;
// Its key: shader type 0xE's program bit
constexpr u64 FlatShaderKey = 2;

// The settings the UI's shapes draw with: blended with the mix preset without the alpha or destination tests, Gouraud shaded and
// untextured without fog or scrolls, in the first context with the FBA and no depth writes
void SetFlatSettings(ShaderSettings& settings)
{
    settings.blends = 1;
    settings.preset = PresetMix;
    settings.alphaTest = 0;
    settings.destinationTest = 0;
    settings.unused23 = 0;
    settings.gouraud = 1;
    settings.textured = 0;
    settings.fog = 0;
    settings.secondContext = 0;
    settings.uScroll = ScrollNone;
    settings.vScroll = ScrollNone;
    settings.noFba = 0;
    settings.unused57 = 0;
    settings.noDepthWrites = 1;
}
}

namespace Platform::Graphics
{
Material* MakeFlatMaterial()
{
    auto* shader = static_cast<Shader*>(MemoryAllocate(FlatShaderSize));
    ShaderConstruct(shader);
    shader->vtable = g_FlatShaderVTable;
    ShaderType0ESetUp(shader);
    for (f32& value : shader->shaderColour)
    {
        value = 1.0f;
    }

    ShaderSettings settings = shader->settings;
    g_FlatMaterial.shaderCount = 1;
    g_FlatMaterial.bucket = BucketUi;
    g_FlatMaterial.activatedShaders = FlatShaderKey;
    g_FlatMaterial.shaders[0] = shader;
    g_FlatShader = shader;
    SetFlatSettings(settings);
    shader->settings = settings;
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    return &g_FlatMaterial;
}

Material* FlatMaterial()
{
    return &g_FlatMaterial;
}

// The UI's 2D particles' shader: the flat settings, additive, textured with STQ coordinates
void* MakeParticleMaterial(Material* material)
{
    material->shaderCount = 0;
    material->bucket = BucketUi;
    auto* shader = static_cast<Shader*>(MemoryAllocate(FlatShaderSize));
    ShaderConstruct(shader);
    shader->vtable = g_FlatShaderVTable;
    ShaderType0ESetUp(shader);
    if (material->shaderCount < MaxMaterialShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    for (f32& value : g_FlatShader->shaderColour)
    {
        value = 1.0f;
    }

    SetFlatSettings(shader->settings);
    shader->settings.preset = PresetAdd;
    shader->settings.textured = 1;
    // The first page's texture: its additive mode's shader's
    Material* page = ParticlePageMaterial(0);
    Shader* pageShader = page != nullptr ? page->shaders[0] : nullptr;
    shader->settings.stq = 1;
    SetShaderTexture(shader, HeaderOf(pageShader->texture)->id);
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    return shader;
}

// The distortion's: shader type 0x18 in its bucket with its key, grey, blended with the mix preset, depth tested GREATER and not
// written, Gouraud, no scrolls; its LOD and linear filtering set after its GS settings are made (they don't get into them)
void MakeDistortionMaterial(Material* material)
{
    constexpr u64 DistortionKey = 2;
    constexpr u32 DistortionShaderSize = 0x80;
    // TEX1's L and K (-12.5 in K's 16ths)
    constexpr s16 DistortionLodL = 1;
    constexpr s16 DistortionLodK = -200;
    material->bucket = BucketDistortion;
    material->activatedShaders = DistortionKey;
    material->shaderCount = 0;
    auto* shader = static_cast<ScreenCopyShader*>(MemoryAllocate(DistortionShaderSize));
    ShaderConstruct(shader);
    shader->vtable = g_DistortionShaderVTable;
    ShaderType18SetUp(shader);
    if (material->shaderCount < MaxMaterialShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    shader->shaderColour[0] = 0.5f;
    shader->shaderColour[3] = 1.0f;
    shader->shaderColour[2] = 0.5f;
    shader->shaderColour[1] = 0.5f;
    ShaderSettings& settings = shader->settings;
    settings.blends = 1;
    settings.preset = PresetMix;
    settings.depthTest = GS_ZBUFF_GREATER;
    settings.gouraud = 1;
    settings.uScroll = ScrollNone;
    settings.vScroll = ScrollNone;
    settings.noDepthWrites = 1;
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    shader->settings.linear = 1;
    shader->lodL = DistortionLodL;
    shader->lodK = DistortionLodK;
}

// The skid marks' (InitFreedMemory makes both): shader type 1 (no texture, the vertexes' colours) in the global objects' bucket
// with its key, blended with the add or subtract preset where the destination alpha test passes (DATE, DATM 0), depth not written
Material* MakeSkidMaterial(bool subtracting)
{
    constexpr u64 SkidKey = 8;
    auto* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(MaterialStorage)));
    material->activatedShaders = SkidKey;
    material->bucket = BucketGlobalOpaque;
    auto* shader = static_cast<Shader*>(MemoryAllocate(sizeof(Shader)));
    ShaderConstruct(shader);
    shader->vtable = g_ShaderType01VTable;
    ShaderType01SetUp(shader);
    ShaderSettings& settings = shader->settings;
    settings.blends = 1;
    settings.ownAlpha = 0;
    settings.preset = subtracting ? PresetSubtract : PresetAdd;
    settings.noDepthWrites = 1;
    settings.destinationTest = 1;
    settings.destinationMode = 0;
    if (material->shaderCount < MaxMaterialShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    return material;
}

void ConstructMaterial(Material* material)
{
    MaterialConstruct(material);
}

void DestroyMaterial(Material* material)
{
    MaterialDestroy(material, DestroyOnly);
}
}

extern "C"
{
    void ShaderType00SetUp(Shader* shader) RETAIL(FUN_001d9400);
    extern const GccVTableEntry g_ShaderType00VTable[] RETAIL(PrecompiledShader__Type_0x0_Methods);
}

namespace
{
constexpr u64 DefaultKey = 8;

Material* MakeKeyedMaterial()
{
    auto* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(Platform::Graphics::MaterialStorage)));
    material->bucket = BucketOpaque;
    material->activatedShaders = DefaultKey;
    return material;
}

Shader* MakeShader(const GccVTableEntry* vtable, void (*setUp)(Shader*))
{
    auto* shader = static_cast<Shader*>(MemoryAllocate(sizeof(Shader)));
    ShaderConstruct(shader);
    shader->vtable = vtable;
    setUp(shader);
    return shader;
}

void AddShader(Material* material, Shader* shader)
{
    if (material->shaderCount < MaxMaterialShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }
}
}

// The type 0 shader's settings bits 23-25 (read from files and never used) are made 4 before its packets are, what the retail
// files have there
extern "C" void MakeDefaultMaterials()
{
    constexpr u64 RetailUnused23 = 4;
    g_UnusedDefaultMaterial = MakeKeyedMaterial();
    Shader* shader = MakeShader(g_ShaderType00VTable, ShaderType00SetUp);
    AddShader(g_UnusedDefaultMaterial, shader);
    shader->settings.unused23 = RetailUnused23;
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
    g_ScreenModelMaterial = MakeKeyedMaterial();
    shader = MakeShader(g_ShaderType01VTable, ShaderType01SetUp);
    AddShader(g_ScreenModelMaterial, shader);
    CallVirtual<void>(shader, shader->vtable, ShaderSettingsSlot);
}
