#include "renderer.h"

#include "game/memory.h"
#include "game/particles.h"

#include "gcc2.h"
#include "platform/graphics.h"

namespace
{
// A material's key of 1 never loads programs
constexpr u64 NoProgramsKey = 1;
// Shaders' programs from 15 on are loaded when a material needs them
constexpr s32 FirstLoadedShaderProgram = 0xF;
// The shaders' vtable functions the materials use: the VU1 program the shader draws with, its packet
constexpr u32 ShaderProgramSlot = 2;
constexpr u32 ShaderPacketSlot = 6;
// The shaders' GS registers (a single shader's are sent to the GIF directly): their size in quadwords, where they are
constexpr u32 ShaderRegistersSizeSlot = 4;
constexpr u32 ShaderRegistersSlot = 7;
// Programs whose entries the material's packets hand VU1
constexpr u32 MaterialEndProgram = 2;
constexpr u32 SecondParameterProgram = 11;

// The entries VU1 goes on to, by UNPACK at the counter's places (it counts them): the program's (and with it a place past it),
// and program 11's in mode 2. The elements' fourth words aren't written: VU1 gets what the buffer had there
u8* WriteParameterEntries(u8* packet, u32* counter, u32 mode, u32 program, s16 offset)
{
    u32 count = mode == 2 ? 2 : 1;
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | count;
    at[1] = 0;
    at[2] = 0;
    at[3] = *counter | count << 16 | VifUnpackV4Count;
    at += 4;
    u32 place = ++*counter;
    u32 address = g_VuPrograms[program].address;
    at[0] = address;
    at[2] = place;
    at[1] = address + offset;
    at += 4;
    if (mode == 2)
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
            packet = FUN_001da880(packet, static_cast<u32>(program));
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
            packet = FUN_001bc9e0(&D_0030A820, packet, reinterpret_cast<Texture*>(shader->texture + 0xC), material->bucket);
        }

        packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, counter, 1u);
    }

    return packet;
}

// Program 2's entry at the counter (VIF1's code before the UNPACK given)
u8* WriteEndEntry(u8* packet, u32 counter, u32 vif)
{
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CountTag | 1;
    at[1] = 0;
    at[2] = vif;
    at[3] = counter | VifUnpackV4;
    at[4] = g_VuPrograms[MaterialEndProgram].address;
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
u8* WriteCall(const Material* material, RenderBucket& bucket, u8* packet)
{
    if (bucket.lastCall == material->call)
    {
        return packet;
    }

    bucket.lastCall = material->call;
    return FUN_001c09e0(material, packet);
}

// The writer's draws follow the bucket's packet
void Splice(RenderBucket& bucket, const RenderBucket& writer)
{
    bucket.last[1] = Address(writer.first);
    bucket.last = writer.last;
}

// A single shader's set-up: its program loaded whatever it is (the material's of several shaders skip the ones always loaded),
// its packet, program 2's entry, its texture uploaded and its registers, its GS registers sent to the GIF directly, the CALL
u8* WriteSingleShader(const Material* material, RenderBucket& bucket, u8* packet)
{
    Shader* shader = material->shaders[0];
    if (StartsPrograms(material, bucket))
    {
        packet = FUN_001da880(packet, CallVirtual<u32>(shader, shader->vtable, ShaderProgramSlot));
    }

    u32 counter = TakeBuffer(bucket);
    packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, &counter, 0u);
    packet = WriteEndEntry(packet, counter, 0);
    if (shader->texture != nullptr)
    {
        auto* texture = reinterpret_cast<Texture*>(shader->texture + 0xC);
        packet = FUN_001bc9e0(&D_0030A820, packet, texture, material->bucket);
        packet = WriteTextureRegisters(&D_0030A820, packet, texture);
    }

    // A REF tag of the registers (VIF1's FLUSHA and DIRECT of them); the asm asks their size twice
    u32 registers = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSlot);
    auto* at = reinterpret_cast<u32*>(packet);
    at[0] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | ReferenceTag;
    at[1] = registers;
    at[2] = VifFlushA;
    at[3] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | VifDirect;
    return WriteCall(material, bucket, packet + 0x10);
}
}

extern "C"
{
    // A CALL of a packet the material's drawing shares: none (0), the render target's (1), the shared GIF tag's (2)
    u8* FUN_001a0dd0(u8* packet, u32 call)
    {
        u32 target;
        if (call == 0)
        {
            return packet;
        }

        if (call == 2)
        {
            target = g_SharedGifPacket;
        }
        else if (call == 1)
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

    u8* FUN_001c09e0(const Material* material, u8* packet)
    {
        return FUN_001a0dd0(packet, material->call);
    }

    u8* FUN_001dcf40(u8* packet, u32* counter, u32 mode)
    {
        return WriteParameterEntries(packet, counter, mode, g_ParameterProgram, g_ParameterProgramOffset);
    }

    u8* FUN_001dca48(u8* packet, u32* counter, u32 mode)
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
        packet = FUN_001c09e0(material, packet);
        packet = FUN_001dcf40(packet, &counter, mode);
        packet = WriteShaders(material, packet, &counter);
        packet = WriteEndEntry(packet, counter, 0);
        return WriteNextBuffer(packet, bucket);
    }

    u8* FUN_001bbc08(Material* material, u8* packet, u32* counter, u32 mode)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        if (StartsPrograms(material, bucket))
        {
            packet = LoadPrograms(material, packet);
        }

        packet = FUN_001c09e0(material, packet);
        packet = FUN_001dca48(packet, counter, mode);
        // The asm uploads the first shader's texture for every shader
        for (u32 i = 0; i < material->shaderCount; i++)
        {
            u8* texture = material->shaders[0]->texture;
            if (texture != nullptr)
            {
                packet = FUN_001bc9e0(&D_0030A820, packet, reinterpret_cast<Texture*>(texture + 0xC), material->bucket);
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
        bucket.lastCall = 0;
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
            packet = WriteCall(material, bucket, packet);
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
            packet = FUN_001da880(packet, CallVirtual<u32>(shader, shader->vtable, ShaderProgramSlot));
        }

        u32 counter = TakeBuffer(bucket);
        packet = CallVirtual<u8*>(shader, shader->vtable, ShaderPacketSlot, packet, &counter, 0u);
        packet = WriteEndEntry(packet, counter, 0);
        packet = WriteNextBuffer(packet, bucket);
        EndPacket(bucket, packet);
        Splice(bucket, material->writer);
        // Nothing of it was a CALL: the bucket's next material makes its own
        bucket.lastCall = 0;
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
            writer.unknown34 = 0;
            writer.vuBuffer = 0;
            writer.unknown0D = 0;
            writer.programRegion = 0;
            writer.last = nullptr;
            writer.insertion = nullptr;
            writer.chain = 0;
            writer.lastKey = 0;
            writer.last2DMaterial = nullptr;
            writer.unknown28 = 0;
            writer.lastJoints = 0;
            writer.lastCall = 0;
            material->drawnDirectly = 0;
            material->listed = 0;
            material->unknown65 = 0;
        }

        g_RenderedMaterialCount = 0;
    }

    void InitShadersRenderedAmt()
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
        FUN_001b9a68();
    }

    CloseInstanceBlocks();
    FlushMaterials();
    ForgetMaterials();
    FUN_001c0f08(&D_0030A820, 0);
}

namespace
{
// The shaders' destructor
constexpr u32 ShaderDestroySlot = 1;
constexpr u32 DeleteShader = 3;
}

extern "C" Material* MaterialConstruct(Material* material)
{
    material->shaderCount = 0;
    RenderBucketConstruct(&material->writer);
    material->call = 1;
    material->listed = 0;
    material->unknown65 = 0;
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
            CallVirtual<void>(shader, shader->vtable, ShaderDestroySlot, DeleteShader);
        }
    }

    if ((flags & 1) != 0)
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
constexpr u32 UiBucket = 0x18;
// Its key: shader type 0xE's program bit
constexpr u64 FlatShaderKey = 2;
// The settings the UI's shapes draw with (bits 0-5, 19, 23-27, 29, 30, 32-37, 56, 57 and 59 replaced)
constexpr u64 FlatSettingsKept = 0xF4FFFFC09077FFC0;
constexpr u64 FlatSettings = 0x0800000004000001;
// The shader's vtable function that makes its packets
constexpr u32 ShaderPrepareSlot = 5;
// The UI's 2D particles' settings: the flat ones, blended (bits 1 and 27), and textured (bit 28)
constexpr u64 ParticleSettings = FlatSettings | 0x08000002;
constexpr u64 TexturedShader = 0x10000000;
constexpr u32 MaxShaders = 4;
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

    u64 settings = shader->settings;
    g_FlatMaterial.shaderCount = 1;
    g_FlatMaterial.bucket = UiBucket;
    g_FlatMaterial.activatedShaders = FlatShaderKey;
    g_FlatMaterial.shaders[0] = shader;
    g_FlatShader = shader;
    shader->settings = (settings & FlatSettingsKept) | FlatSettings;
    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
    return &g_FlatMaterial;
}

Material* FlatMaterial()
{
    return &g_FlatMaterial;
}

void* MakeParticleMaterial(Material* material)
{
    material->shaderCount = 0;
    material->bucket = UiBucket;
    auto* shader = static_cast<Shader*>(MemoryAllocate(FlatShaderSize));
    ShaderConstruct(shader);
    shader->vtable = g_FlatShaderVTable;
    ShaderType0ESetUp(shader);
    if (material->shaderCount < MaxShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    for (f32& value : g_FlatShader->shaderColour)
    {
        value = 1.0f;
    }

    shader->settings = (shader->settings & FlatSettingsKept) | ParticleSettings;
    // The first page's texture: its additive mode's shader's
    Material* page = ParticlePageMaterial(0);
    Shader* pageShader = page != nullptr ? page->shaders[0] : nullptr;
    shader->settings = (shader->settings & ~TexturedShader) | TexturedShader;
    SetShaderTexture(shader, HeaderOf(pageShader->texture)->id);
    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
    return shader;
}

// The distortion's: shader type 0x18 in bucket 23 with its key, grey, blended with preset 0, depth tested GREATER and not written,
// Gouraud, no scrolls; its LOD and linear filtering set after its GS settings are made (they don't get into them)
void MakeDistortionMaterial(Material* material)
{
    constexpr u32 DistortionBucket = 0x17;
    constexpr u64 DistortionKey = 2;
    constexpr u32 DistortionShaderSize = 0x80;
    constexpr u64 DistortionKept = 0xF7FFFFC0FB9FFFE0;
    constexpr u64 DistortionSettings = 0x800000004600001;
    material->bucket = DistortionBucket;
    material->activatedShaders = DistortionKey;
    material->shaderCount = 0;
    auto* shader = static_cast<ScreenCopyShader*>(MemoryAllocate(DistortionShaderSize));
    ShaderConstruct(shader);
    shader->vtable = g_DistortionShaderVTable;
    ShaderType18SetUp(shader);
    if (material->shaderCount < MaxShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    shader->shaderColour[0] = 0.5f;
    shader->shaderColour[3] = 1.0f;
    shader->shaderColour[2] = 0.5f;
    shader->shaderColour[1] = 0.5f;
    shader->settings = (shader->settings & DistortionKept) | DistortionSettings;
    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
    shader->settings = (shader->settings & ~(1ull << SettingLinear)) | 1ull << SettingLinear;
    shader->lodL = 1;
    shader->lodK = -200;
}

// The skid marks' (InitFreedMemory makes both): shader type 1 (no texture, the vertexes' colours) in bucket 3 with its key,
// blended with preset 1 (adding) or 2 (taking away) where the destination alpha test passes (DATE, DATM 0), depth not written
Material* MakeSkidMaterial(bool subtracting)
{
    constexpr u32 SkidBucket = 3;
    constexpr u64 SkidKey = 8;
    constexpr u64 AddingPreset = 1;
    constexpr u64 SubtractingPreset = 2;
    constexpr u64 PresetMask = 0xF;
    auto* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(MaterialStorage)));
    material->activatedShaders = SkidKey;
    material->bucket = SkidBucket;
    auto* shader = static_cast<Shader*>(MemoryAllocate(sizeof(Shader)));
    ShaderConstruct(shader);
    shader->vtable = g_ShaderType01VTable;
    ShaderType01SetUp(shader);
    u64 preset = subtracting ? SubtractingPreset : AddingPreset;
    u64 settings = shader->settings | 1ull << SettingBlends;
    settings &= ~(1ull << SettingOwnAlpha);
    settings = (settings & ~(PresetMask << SettingPreset)) | preset << SettingPreset;
    settings |= 1ull << SettingNoDepthWrites | 1ull << SettingDestinationTest;
    shader->settings = settings & ~(1ull << SettingDestinationMode);
    if (material->shaderCount < MaxShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }

    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
    return material;
}

void ConstructMaterial(Material* material)
{
    MaterialConstruct(material);
}

void DestroyMaterial(Material* material)
{
    MaterialDestroy(material, 2);
}
}

extern "C"
{
    void ShaderType00SetUp(Shader* shader) RETAIL(FUN_001d9400);
    extern const GccVTableEntry g_ShaderType00VTable[] RETAIL(PrecompiledShader__Type_0x0_Methods);
}

namespace
{
constexpr u32 DefaultBucket = 2;
constexpr u64 DefaultKey = 8;

Material* MakeKeyedMaterial()
{
    auto* material = MaterialConstruct(static_cast<Material*>(MemoryAllocate(Platform::Graphics::MaterialStorage)));
    material->bucket = DefaultBucket;
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
    if (material->shaderCount < MaxShaders)
    {
        material->shaders[material->shaderCount++] = shader;
    }
}
}

// The type 0 shader's settings bits 23-25 (read from files and never used) are made 4 before its packets are
extern "C" void MakeDefaultMaterials()
{
    constexpr u64 UnusedBits = 7ull << 23;
    constexpr u64 UnusedValue = 4ull << 23;
    g_UnusedDefaultMaterial = MakeKeyedMaterial();
    Shader* shader = MakeShader(g_ShaderType00VTable, ShaderType00SetUp);
    AddShader(g_UnusedDefaultMaterial, shader);
    shader->settings = (shader->settings & ~UnusedBits) | UnusedValue;
    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
    g_ScreenModelMaterial = MakeKeyedMaterial();
    shader = MakeShader(g_ShaderType01VTable, ShaderType01SetUp);
    AddShader(g_ScreenModelMaterial, shader);
    CallVirtual<void>(shader, shader->vtable, ShaderPrepareSlot);
}
