#include "renderer.h"

#include "game/clock.h"
#include "game/disk.h"
#include "game/graphicstables.h"
#include "game/memory.h"
#include "game/shaderanimation.h"
#include "game/stream.h"
#include "platform/graphics.h"

extern "C"
{
    // A material's shaders read (shaderclasses.cpp)
    void ReadMaterialShaders(Material* material, Stream* stream) RETAIL(ReadMaterialShader);

    // Each kind's constructor, destructor (GCC 2.9x's, the flags' bit 0 frees it) and reader, under their retail names
    GameTexture* TextureConstruct(void* memory, u32 id) RETAIL(FUN_001c1180);
    void TextureDestroy(GameTexture* texture, u32 flags) RETAIL(FUN_001c1118);
    void TextureRead(GameTexture* texture, Stream* stream) RETAIL(ReadTexture);
    MaterialResource* MaterialResourceConstruct(void* memory, u32 id) RETAIL(FUN_001c0dc8);
    void MaterialResourceDestroy(MaterialResource* resource, u32 flags) RETAIL(FUN_001c0d18);
    void MaterialResourceRead(MaterialResource* resource, Stream* stream) RETAIL(ReadMaterial);
    RigidModelData* ModelConstruct(void* memory, u32 id) RETAIL(InitGameModel);
    void ModelDestroy(RigidModelData* model, u32 flags) RETAIL(FUN_001c1298);
    void ModelRead(RigidModelData* model, Stream* stream) RETAIL(ReadGameModel);
    RigidModel* RigidModelConstruct(void* memory, u32 id) RETAIL(FUN_001c1478);
    void RigidModelDestroy(RigidModel* model, u32 flags) RETAIL(FUN_001c13d8);
    void RigidModelRead(RigidModel* model, Stream* stream) RETAIL(ReadRigidModel);
    Skin* SkinConstruct(void* memory, u32 id) RETAIL(FUN_001c1c68);
    void SkinDestroy(Skin* skin, u32 flags) RETAIL(FUN_001be3e8);
    void SkinRead(Skin* skin, Stream* stream) RETAIL(ReadGameSkin);
    BlendSkin* BlendSkinConstruct(void* memory, u32 id) RETAIL(FUN_001c1668);
    void BlendSkinDestroy(BlendSkin* skin, u32 flags) RETAIL(FUN_001c16a8);
    void BlendSkinRead(BlendSkin* skin, Stream* stream) RETAIL(ReadBlendSkin);
    RigidModel* MeshConstruct(void* memory, u32 id) RETAIL(FUN_001c1e70);
    void MeshDestroy(RigidModel* mesh, u32 flags) RETAIL(FUN_001c1dd0);
    void MeshRead(RigidModel* mesh, Stream* stream) RETAIL(ReadRigidModel2);
    Lod* LodConstruct(void* memory, u32 id) RETAIL(FUN_001c2168);
    void LodDestroy(Lod* lod, u32 flags) RETAIL(FUN_001c2018);
    void LodRead(Lod* lod, Stream* stream) RETAIL(ReadLOD);
    Sky* SkyConstruct(void* memory, const u32* id) RETAIL(FUN_001c0440);
    void SkyDestroy(Sky* sky, u32 flags) RETAIL(FUN_001c03a8);
    void SkyRead(Sky* sky, Stream* stream) RETAIL(ReadSkydome);

    // A blend skin's submodels (their parts made and read with the shapes' count) and their parts
    BlendSubModel* BlendSubModelConstruct(BlendSubModel* subModel) RETAIL(FUN_001c1928);
    void BlendSubModelSetCount(BlendSubModel* subModel, u32 count) RETAIL(FUN_001c19e0);
    void BlendSubModelDestroy(BlendSubModel* subModel, u32 flags) RETAIL(FUN_001c1938);
    void ReadBlendParts(BlendSubModel* subModel, Stream* stream, u32 shapeCount) RETAIL(ReadBlendSkinSubBlendStorage);
    BlendPart* BlendPartConstruct(BlendPart* part) RETAIL(FUN_001c1b80);
    void BlendPartDestroy(BlendPart* part, u32 flags) RETAIL(FUN_001c1ba0);
    void ReadBlendPart(BlendPart* part, Stream* stream, u32 shapeCount) RETAIL(ReadBlendSkinSubBlend);
    // No block of instances
    InstanceBlockOwner* InstanceBlockOwnerConstruct(InstanceBlockOwner* owner) RETAIL(FUN_001a26c8);

    // The animations of the shaders of a model's materials started again (as many of a rigid model's as both it and its model
    // have submodels)
    void RigidModelRestartAnimations(RigidModel* model) RETAIL(FUN_001c1598);
    void BlendSkinRestartAnimations(BlendSkin* skin) RETAIL(FUN_001c1870);
    void SkinRestartAnimations(Skin* skin) RETAIL(FUN_001c1d18);
    // A material's shaders moved on by some seconds
    void AnimateMaterial(MaterialResource* resource, f32 seconds) RETAIL_N32(FUN_001c0e70);
}

namespace
{
// A texture's data in its file: the GS's view of it, 0x20 bytes of the tools' memory, then the image
constexpr u32 TextureHeaderSize = sizeof(Texture);
constexpr u32 TextureLeftoverSize = 0x20;
// The disk manager's handles are none until they're allocated
constexpr s32 NoHandle = -1;
// A blend part's shape factors
constexpr u32 ShapeFactorsSize = sizeof(BlendPart::shapeFactors);
// A LOD's distances are squared when it's read, or stored squared
constexpr s32 LodDistancesVersion = 0x1001;
constexpr s32 LodSquaresVersion = 0x1002;
constexpr u32 NoDistance = 0xFFFFFFFF;
// A blend skin's quadwords
constexpr u32 QuadwordShift = 4;

// The disk manager's handles of an array of GCC 2.9x's array new, each none
s32* NewHandles(u32 count)
{
    s32* handles = NewArray<s32>(count);
    for (u32 index = 0; index < count; index++)
    {
        handles[index] = NoHandle;
    }

    return handles;
}

// A packet of the stream's put in a block of the disk manager's for it
void ReadPacket(Stream* stream, s32* handle, u32 size)
{
    s32 allocated;
    DiskAllocate(&allocated, GetDiskManager(), size, false, DeferRelease);
    *handle = allocated;
    stream->Read(DiskLoadedMemory(GetDiskManager(), handle), size, 1);
}

// Whether any shader of the model's materials answers yes (every shader is asked)
bool AnyShader(const RigidModel* model, u32 slot)
{
    bool any = false;
    for (u32 index = 0; index < model->count; index++)
    {
        Material* material = model->materials[index]->material;
        bool answered = false;
        for (u32 shader = 0; shader < material->shaderCount; shader++)
        {
            Shader* asked = material->shaders[shader];
            answered |= CallVirtual<u32>(asked, asked->vtable, slot) != 0;
        }

        any |= answered;
    }

    return any;
}

// A rigid model's or mesh's data after its version: its materials, its model, and whether its shaders need the eye or make it a
// billboard
void ReadRigidModelData(RigidModel* model, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&model->count));
    model->materials = static_cast<MaterialResource**>(MemoryAllocate2(model->count * sizeof(MaterialResource*)));
    for (u32 index = 0; index < model->count; index++)
    {
        u32 id;
        stream->ReadS32(reinterpret_cast<s32*>(&id));
        model->materials[index] = g_MaterialTable.Acquire(&id, nullptr);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&model->modelId));
    model->model = g_ModelTable.Acquire(&model->modelId, nullptr);
    model->instances.block = nullptr;
    model->instances.needsEye = AnyShader(model, ShaderNeedsEyeSlot);
    model->instances.billboard = AnyShader(model, ShaderBillboardSlot);
}

void RestartAnimations(const Material* material)
{
    for (u32 index = 0; index < material->shaderCount; index++)
    {
        ShaderAnimation* animation = material->shaders[index]->animation;
        if (animation != nullptr)
        {
            RestartShaderAnimation(animation);
        }
    }
}

// The meshes of a LOD or sky taken from their table
void ReadMeshes(RigidModel** meshes, u32 count, Stream* stream)
{
    for (u32 index = 0; index < count; index++)
    {
        u32 id;
        stream->ReadS32(reinterpret_cast<s32*>(&id));
        meshes[index] = g_MeshTable.Acquire(&id, nullptr);
    }
}
}

GameTexture* TextureConstruct(void* memory, u32 id)
{
    auto* texture = static_cast<GameTexture*>(memory);
    ConstructResourceHeader(texture, id);
    texture->texture.transfer = NoHandle;
    texture->texture.unused02 = 2;
    texture->texture.unused5C = 0;
    texture->texture.slot = nullptr;
    texture->texture.secondContext = 0;
    return texture;
}

void TextureDestroy(GameTexture* texture, u32 flags)
{
    if (texture->texture.transfer >= 0)
    {
        DiskRelease(GetDiskManager(), &texture->texture.transfer);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(texture);
    }
}

// The header's last 12 bytes are the game's (they're read and set again)
void TextureRead(GameTexture* texture, Stream* stream)
{
    stream->ReadU32(&texture->size);
    stream->Read(&texture->texture, TextureHeaderSize, 1);
    texture->texture.secondContext = 0;
    texture->texture.slot = nullptr;
    u8 leftover[TextureLeftoverSize];
    stream->Read(leftover, TextureLeftoverSize, 1);
    ReadPacket(stream, &texture->texture.transfer, texture->size - (TextureHeaderSize + TextureLeftoverSize));
}

MaterialResource* MaterialResourceConstruct(void* memory, u32 id)
{
    auto* resource = static_cast<MaterialResource*>(memory);
    ConstructResourceHeader(resource, id);
    resource->material = nullptr;
    resource->unused0C = 0;
    return resource;
}

void MaterialResourceDestroy(MaterialResource* resource, u32 flags)
{
    if (resource->material != nullptr)
    {
        MaterialDestroy(resource->material, DestroyAndFree);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(resource);
    }
}

// The material made like MaterialConstruct but for its shader count, which its shaders' reading sets
void MaterialResourceRead(MaterialResource* resource, Stream* stream)
{
    auto* material = static_cast<Material*>(MemoryAllocate(Platform::Graphics::MaterialStorage));
    RenderBucketConstruct(&material->writer);
    material->call = CallRenderTarget;
    material->listed = 0;
    material->unused65 = 0;
    material->drawnDirectly = 0;
    ReadMaterialShaders(material, stream);
    resource->material = material;
}

RigidModelData* ModelConstruct(void* memory, u32 id)
{
    auto* model = static_cast<RigidModelData*>(memory);
    ConstructResourceHeader(model, id);
    model->subModelCount = 0;
    model->subModels = nullptr;
    return model;
}

void ModelDestroy(RigidModelData* model, u32 flags)
{
    for (u32 index = 0; index < model->subModelCount; index++)
    {
        DiskRelease(GetDiskManager(), &model->subModels[index]);
    }

    if (model->subModels != nullptr)
    {
        DeleteArray(model->subModels);
    }

    if (model->vertexCounts != nullptr)
    {
        MemoryDeallocate_(model->vertexCounts);
    }

    if (model->sizes != nullptr)
    {
        MemoryDeallocate_(model->sizes);
    }

    if (model->extraSizes != nullptr)
    {
        MemoryDeallocate_(model->extraSizes);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(model);
    }
}

// Each submodel's data after its packet is read and dropped
void ModelRead(RigidModelData* model, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&model->subModelCount));
    model->subModels = NewHandles(model->subModelCount);
    model->vertexCounts = static_cast<u32*>(MemoryAllocate2(model->subModelCount * sizeof(u32)));
    model->sizes = static_cast<u32*>(MemoryAllocate2(model->subModelCount * sizeof(u32)));
    model->extraSizes = static_cast<u32*>(MemoryAllocate2(model->subModelCount * sizeof(u32)));
    for (u32 index = 0; index < model->subModelCount; index++)
    {
        stream->ReadS32(reinterpret_cast<s32*>(&model->vertexCounts[index]));
        stream->ReadS32(reinterpret_cast<s32*>(&model->sizes[index]));
        ReadPacket(stream, &model->subModels[index], model->sizes[index]);
        stream->ReadS32(reinterpret_cast<s32*>(&model->extraSizes[index]));
        if (model->extraSizes[index] != 0)
        {
            void* extra = MemoryAllocate2(model->extraSizes[index]);
            stream->Read(extra, model->extraSizes[index], 1);
            if (extra != nullptr)
            {
                MemoryDeallocate_(extra);
            }
        }
    }
}

RigidModel* RigidModelConstruct(void* memory, u32 id)
{
    auto* model = static_cast<RigidModel*>(memory);
    ConstructResourceHeader(model, id);
    model->materials = nullptr;
    model->model = nullptr;
    InstanceBlockOwnerConstruct(&model->instances);
    model->count = 0;
    return model;
}

void RigidModelDestroy(RigidModel* model, u32 flags)
{
    for (u32 index = 0; index < model->count; index++)
    {
        ReleaseMaterial(model->materials[index]);
    }

    ReleaseModel(model->model);
    if (model->materials != nullptr)
    {
        MemoryDeallocate_(model->materials);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(model);
    }
}

// The version is read and dropped
void RigidModelRead(RigidModel* model, Stream* stream)
{
    s32 version = 0;
    stream->ReadS32(&version);
    ReadRigidModelData(model, stream);
}

Skin* SkinConstruct(void* memory, u32 id)
{
    auto* skin = static_cast<Skin*>(memory);
    ConstructResourceHeader(skin, id);
    skin->materials = nullptr;
    skin->vertexCounts = nullptr;
    skin->subModels = nullptr;
    skin->count = 0;
    skin->sizes = nullptr;
    return skin;
}

void SkinDestroy(Skin* skin, u32 flags)
{
    for (u32 index = 0; index < skin->count; index++)
    {
        ReleaseMaterial(skin->materials[index]);
        if (skin->subModels[index] >= 0)
        {
            DiskRelease(GetDiskManager(), &skin->subModels[index]);
        }
    }

    if (skin->materials != nullptr)
    {
        MemoryDeallocate_(skin->materials);
    }

    if (skin->vertexCounts != nullptr)
    {
        MemoryDeallocate_(skin->vertexCounts);
    }

    if (skin->subModels != nullptr)
    {
        DeleteArray(skin->subModels);
    }

    if (skin->sizes != nullptr)
    {
        MemoryDeallocate_(skin->sizes);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(skin);
    }
}

void SkinRead(Skin* skin, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&skin->count));
    skin->materials = static_cast<MaterialResource**>(MemoryAllocate2(skin->count * sizeof(MaterialResource*)));
    skin->subModels = NewHandles(skin->count);
    skin->sizes = static_cast<u32*>(MemoryAllocate2(skin->count * sizeof(u32)));
    skin->vertexCounts = static_cast<u32*>(MemoryAllocate2(skin->count * sizeof(u32)));
    for (u32 index = 0; index < skin->count; index++)
    {
        u32 id;
        stream->ReadS32(reinterpret_cast<s32*>(&id));
        skin->materials[index] = g_MaterialTable.Acquire(&id, nullptr);
        stream->ReadS32(reinterpret_cast<s32*>(&skin->sizes[index]));
        stream->ReadS32(reinterpret_cast<s32*>(&skin->vertexCounts[index]));
        skin->vertexTotal += skin->vertexCounts[index];
        ReadPacket(stream, &skin->subModels[index], skin->sizes[index]);
    }
}

BlendSkin* BlendSkinConstruct(void* memory, u32 id)
{
    auto* skin = static_cast<BlendSkin*>(memory);
    ConstructResourceHeader(skin, id);
    skin->materials = nullptr;
    skin->subModels = nullptr;
    skin->count = 0;
    return skin;
}

void BlendSkinDestroy(BlendSkin* skin, u32 flags)
{
    for (u32 index = 0; index < skin->count; index++)
    {
        ReleaseMaterial(skin->materials[index]);
        BlendSubModel* subModel = skin->subModels[index];
        if (subModel != nullptr)
        {
            BlendSubModelDestroy(subModel, DestroyAndFree);
        }
    }

    if (skin->materials != nullptr)
    {
        MemoryDeallocate_(skin->materials);
    }

    if (skin->subModels != nullptr)
    {
        MemoryDeallocate_(skin->subModels);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(skin);
    }
}

void BlendSkinRead(BlendSkin* skin, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&skin->count));
    stream->ReadS32(reinterpret_cast<s32*>(&skin->shapeCount));
    skin->materials = static_cast<MaterialResource**>(MemoryAllocate2(skin->count * sizeof(MaterialResource*)));
    skin->subModels = static_cast<BlendSubModel**>(MemoryAllocate2(skin->count * sizeof(BlendSubModel*)));
    for (u32 index = 0; index < skin->count; index++)
    {
        BlendSubModel* subModel = BlendSubModelConstruct(static_cast<BlendSubModel*>(MemoryAllocate(sizeof(BlendSubModel))));
        u32 partCount;
        stream->ReadS32(reinterpret_cast<s32*>(&partCount));
        u32 id;
        stream->ReadS32(reinterpret_cast<s32*>(&id));
        skin->materials[index] = g_MaterialTable.Acquire(&id, nullptr);
        BlendSubModelSetCount(subModel, partCount);
        ReadBlendParts(subModel, stream, skin->shapeCount);
        skin->subModels[index] = subModel;
    }
}

BlendSubModel* BlendSubModelConstruct(BlendSubModel* subModel)
{
    subModel->parts = nullptr;
    subModel->count = 0;
    return subModel;
}

void BlendSubModelSetCount(BlendSubModel* subModel, u32 count)
{
    subModel->count = count;
    subModel->parts = static_cast<BlendPart**>(MemoryAllocate2(count * sizeof(BlendPart*)));
}

// Without parts its array isn't freed
void BlendSubModelDestroy(BlendSubModel* subModel, u32 flags)
{
    if (subModel->count != 0)
    {
        for (u32 index = 0; index < subModel->count; index++)
        {
            BlendPart* part = subModel->parts[index];
            if (part != nullptr)
            {
                BlendPartDestroy(part, DestroyAndFree);
            }
        }

        if (subModel->parts != nullptr)
        {
            MemoryDeallocate_(subModel->parts);
        }
    }

    subModel->parts = nullptr;
    subModel->count = 0;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(subModel);
    }
}

void ReadBlendParts(BlendSubModel* subModel, Stream* stream, u32 shapeCount)
{
    for (u32 index = 0; index < subModel->count; index++)
    {
        subModel->parts[index] = BlendPartConstruct(static_cast<BlendPart*>(MemoryAllocate(sizeof(BlendPart))));
        ReadBlendPart(subModel->parts[index], stream, shapeCount);
    }
}

BlendPart* BlendPartConstruct(BlendPart* part)
{
    part->shapeAddresses = nullptr;
    part->shapeSizes = nullptr;
    part->shapeCounts = nullptr;
    part->packet = 0;
    part->shapeCount = 0;
    return part;
}

void BlendPartDestroy(BlendPart* part, u32 flags)
{
    if (part->packet != 0)
    {
        MemoryDeallocate_(reinterpret_cast<void*>(part->packet));
    }

    for (s32 shape = 0; shape < part->shapeCount; shape++)
    {
        MemoryDeallocate2_(reinterpret_cast<void*>(part->shapeAddresses[shape]));
    }

    if (part->shapeAddresses != nullptr)
    {
        MemoryDeallocate_(part->shapeAddresses);
    }

    if (part->shapeSizes != nullptr)
    {
        MemoryDeallocate_(part->shapeSizes);
    }

    if (part->shapeCounts != nullptr)
    {
        MemoryDeallocate_(part->shapeCounts);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(part);
    }
}

void ReadBlendPart(BlendPart* part, Stream* stream, u32 shapeCount)
{
    stream->ReadS32(reinterpret_cast<s32*>(&part->packetSize));
    stream->ReadS32(reinterpret_cast<s32*>(&part->unused20));
    part->packet = reinterpret_cast<u32>(MemoryAllocate2(part->packetSize));
    stream->Read(reinterpret_cast<void*>(part->packet), part->packetSize, 1);
    stream->Read(part->shapeFactors, ShapeFactorsSize, 1);
    part->shapeCount = static_cast<u8>(shapeCount);
    part->shapeAddresses = static_cast<u32*>(MemoryAllocate2(part->shapeCount * sizeof(u32)));
    part->shapeSizes = static_cast<u32*>(MemoryAllocate2(part->shapeCount * sizeof(u32)));
    part->shapeCounts = static_cast<u32*>(MemoryAllocate2(part->shapeCount * sizeof(u32)));
    for (u32 shape = 0; shape < part->shapeCount; shape++)
    {
        stream->ReadS32(reinterpret_cast<s32*>(&part->shapeSizes[shape]));
        stream->ReadS32(reinterpret_cast<s32*>(&part->shapeCounts[shape]));
        u32 size = part->shapeSizes[shape] << QuadwordShift;
        part->shapeAddresses[shape] = reinterpret_cast<u32>(MemoryAllocate2(size));
        stream->Read(reinterpret_cast<void*>(part->shapeAddresses[shape]), size, 1);
    }
}

RigidModel* MeshConstruct(void* memory, u32 id)
{
    return RigidModelConstruct(memory, id);
}

void MeshDestroy(RigidModel* mesh, u32 flags)
{
    RigidModelDestroy(mesh, flags);
}

void MeshRead(RigidModel* mesh, Stream* stream)
{
    s32 version;
    stream->ReadS32(&version);
    ReadRigidModelData(mesh, stream);
}

Lod* LodConstruct(void* memory, u32 id)
{
    auto* lod = static_cast<Lod*>(memory);
    ConstructResourceHeader(lod, id);
    lod->meshes = nullptr;
    lod->count = 0;
    return lod;
}

void LodDestroy(Lod* lod, u32 flags)
{
    for (s32 index = 0; index < lod->count; index++)
    {
        ReleaseMesh(lod->meshes[index]);
    }

    if (lod->meshes != nullptr)
    {
        MemoryDeallocate_(lod->meshes);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(lod);
    }
}

// The first format's distances are squared (the second's of no distance kept), a word after them read and dropped; the second's
// are stored squared. Other versions are read no further
void LodRead(Lod* lod, Stream* stream)
{
    s32 version;
    stream->ReadS32(&version);
    if (version == LodDistancesVersion)
    {
        u32 count;
        stream->ReadU32(&count);
        lod->count = static_cast<u8>(count);
        lod->meshes = static_cast<RigidModel**>(MemoryAllocate2(lod->count * sizeof(RigidModel*)));
        stream->ReadS32(reinterpret_cast<s32*>(&lod->nearest));
        stream->ReadS32(reinterpret_cast<s32*>(&lod->farthest));
        lod->nearest = lod->nearest * lod->nearest;
        if (lod->farthest != NoDistance)
        {
            lod->farthest = lod->farthest * lod->farthest;
        }

        for (u32 distance = 0; distance < Lod::MostSwitches; distance++)
        {
            u32& value = lod->switchDistances[distance];
            stream->ReadS32(reinterpret_cast<s32*>(&value));
            value = value * value;
        }

        s32 unused;
        stream->ReadS32(&unused);
        ReadMeshes(lod->meshes, lod->count, stream);
        return;
    }

    if (version == LodSquaresVersion)
    {
        stream->ReadS8(reinterpret_cast<s8*>(&lod->count));
        lod->meshes = static_cast<RigidModel**>(MemoryAllocate2(lod->count * sizeof(RigidModel*)));
        stream->ReadS32(reinterpret_cast<s32*>(&lod->nearest));
        stream->ReadS32(reinterpret_cast<s32*>(&lod->farthest));
        stream->Read(lod->switchDistances, sizeof(lod->switchDistances), 1);
        ReadMeshes(lod->meshes, lod->count, stream);
    }
}

Sky* SkyConstruct(void* memory, const u32* id)
{
    auto* sky = static_cast<Sky*>(memory);
    ConstructResourceHeader(sky, *id);
    sky->count = 0;
    sky->models = nullptr;
    return sky;
}

void SkyDestroy(Sky* sky, u32 flags)
{
    for (u32 index = 0; index < sky->count; index++)
    {
        ReleaseMesh(sky->models[index]);
    }

    if (sky->models != nullptr)
    {
        MemoryDeallocate_(sky->models);
    }

    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(sky);
    }
}

// A word before the meshes is read and dropped
void SkyRead(Sky* sky, Stream* stream)
{
    u32 unused;
    stream->ReadU32(&unused);
    stream->ReadS32(reinterpret_cast<s32*>(&sky->count));
    sky->models = static_cast<RigidModel**>(MemoryAllocate2(sky->count * sizeof(RigidModel*)));
    ReadMeshes(sky->models, sky->count, stream);
}

InstanceBlockOwner* InstanceBlockOwnerConstruct(InstanceBlockOwner* owner)
{
    owner->block = nullptr;
    owner->needsEye = 0;
    owner->billboard = 0;
    return owner;
}

void RigidModelRestartAnimations(RigidModel* model)
{
    u32 count = model->count;
    if (model->model->subModelCount < count)
    {
        count = model->model->subModelCount;
    }

    for (u32 index = 0; index < count; index++)
    {
        RestartAnimations(model->materials[index]->material);
    }
}

void BlendSkinRestartAnimations(BlendSkin* skin)
{
    for (u32 index = 0; index < skin->count; index++)
    {
        RestartAnimations(skin->materials[index]->material);
    }
}

void SkinRestartAnimations(Skin* skin)
{
    for (u32 index = 0; index < skin->count; index++)
    {
        RestartAnimations(skin->materials[index]->material);
    }
}

RigidModel* LodMeshAt(Lod* lod, u32 distance)
{
    if (distance < lod->nearest || lod->farthest < distance)
    {
        return nullptr;
    }

    RigidModel* mesh = lod->meshes[0];
    s32 last = lod->count - 1;
    if (last <= 0 || distance < lod->switchDistances[0])
    {
        return mesh;
    }

    mesh = lod->meshes[1];
    for (s32 index = 1; index < last && distance >= lod->switchDistances[index]; index++)
    {
        mesh = lod->meshes[index + 1];
    }

    return mesh;
}

void AnimateMaterials(const TimeClock* clock)
{
    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    for (u32 index = 0; index < g_MaterialTable.count; index++)
    {
        AnimateMaterial(g_MaterialTable.entries[index].item, seconds);
    }
}

void AnimateMaterial(MaterialResource* resource, f32 seconds)
{
    Material* material = resource->material;
    for (u32 index = 0; index < material->shaderCount; index++)
    {
        Shader* shader = material->shaders[index];
        CallVirtual<void>(shader, shader->vtable, ShaderUpdateSlot, seconds);
    }
}

EABI_EXPORT(FUN_001c0e70, AnimateMaterial);

namespace Platform::Graphics
{
void RestartAnimations(RigidModel* model)
{
    RigidModelRestartAnimations(model);
}

void RestartAnimations(Skin* skin)
{
    SkinRestartAnimations(skin);
}

void RestartAnimations(BlendSkin* skin)
{
    BlendSkinRestartAnimations(skin);
}

MaterialResource* FirstMaterial(const RigidModel* model)
{
    return model->materials != nullptr && model->count != 0 ? model->materials[0] : nullptr;
}

GameTexture* NewTexture(u32 id)
{
    return TextureConstruct(MemoryAllocate(sizeof(GameTexture)), id);
}

void DeleteTexture(GameTexture* texture)
{
    TextureDestroy(texture, DestroyAndFree);
}

void ReadTexture(GameTexture* texture, Stream* stream)
{
    TextureRead(texture, stream);
}

MaterialResource* NewMaterial(u32 id)
{
    return MaterialResourceConstruct(MemoryAllocate(sizeof(MaterialResource)), id);
}

void DeleteMaterial(MaterialResource* material)
{
    MaterialResourceDestroy(material, DestroyAndFree);
}

void ReadMaterial(MaterialResource* material, Stream* stream)
{
    MaterialResourceRead(material, stream);
}

RigidModelData* NewModel(u32 id)
{
    return ModelConstruct(MemoryAllocate(sizeof(RigidModelData)), id);
}

void DeleteModel(RigidModelData* model)
{
    ModelDestroy(model, DestroyAndFree);
}

void ReadModel(RigidModelData* model, Stream* stream)
{
    ModelRead(model, stream);
}

RigidModel* NewRigidModel(u32 id)
{
    return RigidModelConstruct(MemoryAllocate(sizeof(RigidModel)), id);
}

void DeleteRigidModel(RigidModel* model)
{
    RigidModelDestroy(model, DestroyAndFree);
}

void ReadRigidModel(RigidModel* model, Stream* stream)
{
    RigidModelRead(model, stream);
}

Skin* NewSkin(u32 id)
{
    return SkinConstruct(MemoryAllocate(sizeof(Skin)), id);
}

void DeleteSkin(Skin* skin)
{
    SkinDestroy(skin, DestroyAndFree);
}

void ReadSkin(Skin* skin, Stream* stream)
{
    SkinRead(skin, stream);
}

BlendSkin* NewBlendSkin(u32 id)
{
    return BlendSkinConstruct(MemoryAllocate(sizeof(BlendSkin)), id);
}

void DeleteBlendSkin(BlendSkin* skin)
{
    BlendSkinDestroy(skin, DestroyAndFree);
}

void ReadBlendSkin(BlendSkin* skin, Stream* stream)
{
    BlendSkinRead(skin, stream);
}

RigidModel* NewMesh(u32 id)
{
    return MeshConstruct(MemoryAllocate(sizeof(RigidModel)), id);
}

void DeleteMesh(RigidModel* mesh)
{
    MeshDestroy(mesh, DestroyAndFree);
}

void ReadMesh(RigidModel* mesh, Stream* stream)
{
    MeshRead(mesh, stream);
}

Lod* NewLod(u32 id)
{
    return LodConstruct(MemoryAllocate(sizeof(Lod)), id);
}

void DeleteLod(Lod* lod)
{
    LodDestroy(lod, DestroyAndFree);
}

void ReadLod(Lod* lod, Stream* stream)
{
    LodRead(lod, stream);
}

Sky* NewSky(u32 id)
{
    return SkyConstruct(MemoryAllocate(sizeof(Sky)), &id);
}

void DeleteSky(Sky* sky)
{
    SkyDestroy(sky, DestroyAndFree);
}

void ReadSky(Sky* sky, Stream* stream)
{
    SkyRead(sky, stream);
}
}
