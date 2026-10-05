#include "renderer.h"

#include "game/disk.h"
#include "game/memory.h"
#include "platform/graphics.h"

namespace
{
// 800 blocks a frame, in two regions of a million bytes that take turns. A block starts 128 bytes aligned with room for 100
// quadwords after its tag's
constexpr u32 BlockCount = 0x320;
constexpr u32 RegionSize = 1000000;
constexpr u32 BlockAlignment = 0x80;
constexpr u16 BlockCapacity = 100;
constexpr u32 BlockTagBytes = 0x10;
// A rigid model's block needs this much of the region, and closes when it has room for less than 22 quadwords more
constexpr u32 RigidBlockBytes = 0x6F0;
constexpr u32 RigidRoom = 0x16;

// An instance's flags: drawn clipped
constexpr u32 InstanceClipped = 0x8000;
// Its sizes (quadwords): the basic data, with what clipping needs, with the eye's too (the basic data's size tells VU1 where
// the rest starts)
constexpr u32 BasicSize = 0xD;
constexpr u32 ClippedSize = 0x12;
constexpr u32 EyeSize = 0x18;

// Placed models' blocks: they need this much of the region, and close when they have room for 14 quadwords more or less. Their
// instances' sizes: the basic data, clipped, with the eye
constexpr u32 PlacedBlockBytes = 0x650;
constexpr u32 PlacedRoom = 0xE;
constexpr u32 PlacedBasicSize = 5;
constexpr u32 PlacedClippedSize = 0xA;
constexpr u32 PlacedEyeSize = 0xB;
// A billboard looking at the camera from almost straight below or above takes an up a little tilted
constexpr f32 BillboardEpsilon = 0x1.5798ECp-29f;
constexpr Vector4 TiltedUp = {-0x1.9906CCp-5f, 0x1.FF5C28p-1f, 0.0f, 0.0f};

// A point 500 units above the model's origin, turned into its space with the eye
constexpr Vector4 Up = {0.0f, 500.0f, 0.0f, 1.0f};

// A rigid model's instance in its block: its flags, where the block's instances end in VU1's memory (the first instance's, when
// the block closes), the size of its basic data and its size (quadwords), its matrix to the screen, its lights' directions in
// its space (a column each, the rows' fourth words a stack's leftovers), their colours and the ambient light; then, clipped,
// its matrix to camera space and the view's clip vector, and when its materials need the eye, the eye, the point above it and
// its matrix
struct RigidInstance
{
    u32 flags;
    u32 end;
    u32 basicSize;
    u32 size;
    Matrix4x4 toScreen;
    Matrix4x4 lights;
    Vector4 lightColours[3];
    Vector4 ambient;
    Matrix4x4 toCamera;
    Vector4 clip;
    Vector4 eye;
    Vector4 up;
    Matrix4x4 matrix;
};
CHECK_SIZE(RigidInstance, EyeSize * 0x10);

// A placed model's instance in its block: its flags, where the block's instances end (the first's, when the block closes), the
// size of its basic data and its size (quadwords), its matrices to the screen and to camera space, the view's clip vector and the
// eye in its space
struct PlacedInstance
{
    u32 flags;
    u32 end;
    u32 basicSize;
    u32 size;
    Matrix4x4 toScreen;
    Matrix4x4 toCamera;
    Vector4 clip;
    Vector4 eye;
};
CHECK_SIZE(PlacedInstance, PlacedEyeSize * 0x10);

Vector4* Row(Matrix4x4& matrix, u32 row)
{
    return reinterpret_cast<Vector4*>(matrix.m[row]);
}

// The row's x, y and z (its w kept)
void SetRow(Matrix4x4& matrix, u32 row, const Vector4& vector)
{
    matrix.m[row][0] = vector.x;
    matrix.m[row][1] = vector.y;
    matrix.m[row][2] = vector.z;
}

// Its RET tag sends VIF1 an UNPACK of the instances (after a FLUSHE: VU1 is done with the last ones), and the first instance's
// second word tells VU1 where they end. The owner gets a new block for its next instance
void Close(InstanceBlock& block)
{
    auto* tag = reinterpret_cast<u32*>(block.data);
    tag[0] = block.size | ReturnTag;
    tag[1] = 0;
    tag[2] = VifFlushE;
    tag[3] = VifUnpackTo(VifUnpackV4Count, g_VuInstances, block.size);
    tag[5] = block.size + g_VuInstances;
    block.owner->block = nullptr;
    block.owner = nullptr;
}

// The eye in the model's space
void EyeIn(const Matrix4x4& inverse, Vector4* eye)
{
    *eye = *RowOf(&g_DrawnChunk->matrix, 3);
    VuTransformPoint(&inverse, eye, eye);
}

// The block's room for one more: the one open, a new one (nullptr when they're all taken or the region is full)
u8* NewBlock(InstanceBlockOwner* owner, u32 bytes, InstanceBlockKind kind)
{
    if (g_InstanceBlockCount >= BlockCount)
    {
        return nullptr;
    }

    u8* start = reinterpret_cast<u8*>((Address(g_InstanceBlockNext) + BlockAlignment - 1) & ~(BlockAlignment - 1));
    if (Address(g_InstanceRegion) + RegionSize < Address(start) + bytes)
    {
        return nullptr;
    }

    InstanceBlock* block = &g_InstanceBlocks[g_InstanceBlockCount];
    g_InstanceBlockNext = start;
    block->data = start;
    block->count = 1;
    block->kind = kind;
    block->capacity = BlockCapacity;
    block->owner = owner;
    block->size = 0;
    owner->block = block;
    return start;
}

// The new block is the frame's: the next one goes past its tag and capacity
u8* TakeBlock(InstanceBlock* block)
{
    g_InstanceBlockCount++;
    g_InstanceBlockNext = block->data + block->capacity * 0x10 + BlockTagBytes;
    return block->data;
}

// The world matrix taken into the object's chunk's space (its vtable's), or the one given without an object
Matrix4x4 WorldMatrix(const Matrix4x4* world, PlacedObject* object)
{
    if (object != nullptr)
    {
        return *CallVirtual<const Matrix4x4*>(object, object->vtable, PlacedObject::ChunkMatrixSlot, world);
    }

    return *world;
}

// A billboard's turn: its rows the side, up and back of the object facing the camera
void Billboard(const Matrix4x4* world, PlacedObject* object, Matrix4x4* out)
{
    const Matrix4x4* space = &object->matrices[PlacedObject::ChunkDrawInverse];
    Matrix4x4 inverse = *world;
    VuInvertRigidInPlace(&inverse);
    Vector4 camera = CameraPosition(object->view);
    VuTransformPoint(space, &camera, &camera);
    VuTransformPoint(&inverse, &camera, &camera);
    Vector4 direction = {-camera.x, -camera.y, -camera.z, 1.0f};
    Vector4 up = {0.0f, 1.0f, 0.0f, 0.0f};
    if (SquaredDistance(&up, &direction) <= DotProduct(&up, &direction) * BillboardEpsilon)
    {
        up = TiltedUp;
    }

    Vector4 side = {up.y * direction.z - up.z * direction.y, up.z * direction.x - up.x * direction.z,
                    up.x * direction.y - up.y * direction.x, 1.0f};
    f32 scale = InverseLength(&side, BillboardEpsilon);
    side.x *= scale;
    side.y *= scale;
    side.z *= scale;
    Vector4 back = {up.y * side.z - up.z * side.y, up.z * side.x - up.x * side.z, up.x * side.y - up.y * side.x, 1.0f};
    scale = InverseLength(&back, BillboardEpsilon);
    back.x *= scale;
    back.y *= scale;
    back.z *= scale;
    InitIdentityMatrix(out);
    SetRow(*out, 0, side);
    SetRow(*out, 1, up);
    SetRow(*out, 2, back);
}

// Each submodel's packet with the block into its material's writer: a CALL of the block (VIF1 waits for VU1 first), VU1 told
// where the instances are, a CALL of the submodel's packet. A default mesh's material is set up when its writer starts, which
// takes the packets in before its last tag
void WriteSubModels(RigidModel* model, u32 count, u8* block, bool inserted)
{
    for (u32 i = 0; i < count; i++)
    {
        DiskManager* disk = GetDiskManager();
        s32 handle = model->model->subModels[i];
        u8* packet = DiskLoadedMemory(disk, &handle);
        Material* material = model->materials[i]->material;
        RenderBucket& writer = material->writer;
        if (writer.first == nullptr)
        {
            if (inserted)
            {
                StartMaterialWriterTwoTags(material);
                StartMaterialDirectly(material);
            }
            else
            {
                StartMaterialWriter(material);
            }
        }

        u32* at = inserted ? BeginInsertedPacket(writer) : BeginPacket(writer);
        at[0] = CallTag;
        at[1] = Address(block);
        at[2] = VifFlushE;
        at[3] = 0;
        at[4] = CountTag;
        at[5] = 0;
        at[6] = g_VuInstancesPlace | VifUnpackS;
        at[7] = g_VuInstances;
        at[8] = CallTag;
        at[9] = Address(packet);
        at[10] = 0;
        at[11] = 0;
        if (inserted)
        {
            EndInsertedPacket(writer, reinterpret_cast<u8*>(at + 12));
        }
        else
        {
            EndPacket(writer, reinterpret_cast<u8*>(at + 12));
        }
    }
}

u32 DrawnSubModels(const RigidModel* model)
{
    return model->model->subModelCount < model->count ? model->model->subModelCount : model->count;
}
}

extern "C"
{
    u8* InitialiseInstanceBlocks(u8* memory)
    {
        if (g_InstanceBlocks != nullptr)
        {
            MemoryDeallocate2_(g_InstanceBlocks);
        }

        u8* first = reinterpret_cast<u8*>((Address(memory) + 0xF) & ~0xFu);
        g_InstanceFirstRegion = first;
        u8* second = reinterpret_cast<u8*>((Address(first + RegionSize) + 0xF) & ~0xFu);
        g_InstanceSecondRegion = second;
        g_InstanceBlockNext = g_InstanceFirstRegion;
        g_InstanceRegion = g_InstanceFirstRegion;
        g_InstanceBlocks = static_cast<InstanceBlock*>(MemoryAllocateAligned(GetHeapManager(), BlockCount * sizeof(InstanceBlock), 0x10));
        g_InstanceBlockCount = 0;
        for (u32 i = 0; i < BlockCount; i++)
        {
            g_InstanceBlocks[i].owner = nullptr;
        }

        return second + RegionSize;
    }

    u32 CloseInstanceBlocks()
    {
        for (u32 i = 0; i < g_InstanceBlockCount; i++)
        {
            InstanceBlock& block = g_InstanceBlocks[i];
            if (block.owner != nullptr)
            {
                Close(block);
            }
        }

        g_InstanceBlockCount = 0;
        u8* region = g_InstanceFirstRegion == g_InstanceRegion ? g_InstanceSecondRegion : g_InstanceFirstRegion;
        g_InstanceBlockNext = region;
        g_InstanceRegion = region;
        return 1;
    }

    u8* SetRigidModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* matrix, u32 mode, const Vector4* lightDirections,
                               const Vector4* lightColours, const Vector4* ambient)
    {
        RenderView* view = g_RenderView;
        Matrix4x4 rotation;
        VuTransposeRotation(matrix, &rotation);
        // The fourth row isn't written: the lights' rows get the stack's leftovers as their fourth words
        Matrix4x4 lights;
        VuRotateVector(&rotation, &lightDirections[0], Row(lights, 0));
        VuRotateVector(&rotation, &lightDirections[1], Row(lights, 1));
        VuRotateVector(&rotation, &lightDirections[2], Row(lights, 2));

        InstanceBlock* block = owner->block;
        bool opened = block == nullptr;
        u8* place;
        if (opened)
        {
            place = NewBlock(owner, RigidBlockBytes, RigidInstances);
            if (place == nullptr)
            {
                return nullptr;
            }

            block = owner->block;
        }
        else
        {
            place = block->data + block->size * 0x10;
        }

        auto* instance = reinterpret_cast<RigidInstance*>(place + BlockTagBytes);
        VuMultiplyMatrices(matrix, &view->toScreen, &instance->toScreen);
        VuTranspose(&lights, &instance->lights);
        instance->lightColours[0] = lightColours[0];
        instance->lightColours[1] = lightColours[1];
        instance->lightColours[2] = lightColours[2];
        instance->ambient = *ambient;
        u32 size;
        if (mode != DrawUnclipped)
        {
            instance->flags = InstanceClipped;
            instance->basicSize = BasicSize;
            VuMultiplyMatrices(matrix, &view->toClip, &instance->toCamera);
            instance->clip = view->clip;
            size = ClippedSize;
        }
        else
        {
            instance->flags = 0;
            size = BasicSize;
            if (owner->needsEye)
            {
                instance->basicSize = BasicSize;
            }
        }

        if (owner->needsEye)
        {
            Matrix4x4 inverse;
            VuInvertRigid(&inverse, matrix);
            EyeIn(inverse, &instance->eye);
            if (mode == DrawUnclipped)
            {
                VuMultiplyMatrices(matrix, &view->toClip, &instance->toCamera);
                instance->clip = view->clip;
            }

            // A new block's first instance turns the point by the inverse, the open block's by the rotation
            VuRotateVector(opened ? &inverse : &rotation, &Up, &instance->up);
            // The matrix goes where a new block's first instance has it: for an open block that's past the open blocks, where
            // the next new block goes (it writes over it)
            reinterpret_cast<RigidInstance*>(g_InstanceBlockNext + BlockTagBytes)->matrix = *matrix;
            size = EyeSize;
        }

        instance->size = size;
        block->size += size;
        if (opened)
        {
            return TakeBlock(block);
        }

        block->count++;
        if (block->capacity < block->size + RigidRoom)
        {
            Close(*block);
        }

        return nullptr;
    }

    void SetRigidModelDma_(RigidModel* model, const Matrix4x4* matrix, const Vector4* lightDirections,
                           const Vector4* lightColours, const Vector4* ambient, u32 mode)
    {
        u32 count = DrawnSubModels(model);
        u8* block = SetRigidModelRenderDMA(&model->instances, matrix, mode, lightDirections, lightColours, ambient);
        if (block != nullptr)
        {
            WriteSubModels(model, count, block, false);
        }
    }

    u8* SetPlacedModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                                const Vector4* clip, const Matrix4x4* world, PlacedObject* object)
    {
        RenderView* view = g_RenderView;
        Matrix4x4 turnedToScreen;
        Matrix4x4 turnedToCamera;
        if (owner->billboard != 0)
        {
            Matrix4x4 turn;
            Billboard(world, object, &turn);
            VuMultiplyMatrices(&turn, toScreen, &turnedToScreen);
            VuMultiplyMatrices(&turn, toCamera, &turnedToCamera);
            toScreen = &turnedToScreen;
            toCamera = &turnedToCamera;
        }

        InstanceBlock* block = owner->block;
        bool opened = block == nullptr;
        u8* place;
        if (opened)
        {
            place = NewBlock(owner, PlacedBlockBytes, PlacedInstances);
            if (place == nullptr)
            {
                return nullptr;
            }

            block = owner->block;
        }
        else
        {
            place = block->data + block->size * 0x10;
        }

        auto* instance = reinterpret_cast<PlacedInstance*>(place + BlockTagBytes);
        instance->toScreen = *toScreen;
        u32 size;
        if (mode != DrawUnclipped)
        {
            instance->flags = InstanceClipped;
            instance->basicSize = PlacedBasicSize;
            instance->toCamera = *toCamera;
            instance->clip = *clip;
            size = PlacedClippedSize;
            if (owner->needsEye)
            {
                Matrix4x4 inverse = WorldMatrix(world, object);
                VuInvertRigidInPlace(&inverse);
                instance->eye = CameraPosition(view);
                VuTransformPoint(&inverse, &instance->eye, &instance->eye);
                size = PlacedEyeSize;
            }
        }
        else
        {
            instance->flags = 0;
            size = PlacedBasicSize;
            if (owner->needsEye)
            {
                instance->basicSize = PlacedBasicSize;
                Matrix4x4 matrix = WorldMatrix(world, object);
                VuMultiplyMatrices(&matrix, &view->toClip, &instance->toCamera);
                instance->clip = view->clip;
                VuInvertRigidInPlace(&matrix);
                instance->eye = CameraPosition(view);
                VuTransformPoint(&matrix, &instance->eye, &instance->eye);
                size = PlacedEyeSize;
            }
        }

        instance->size = size;
        block->size += size;
        if (opened)
        {
            return TakeBlock(block);
        }

        block->count++;
        if (block->size + PlacedRoom >= block->capacity)
        {
            Close(*block);
        }

        return nullptr;
    }

    void SetPlacedModelDMA(RigidModel* model, PlacedObject* object, const Matrix4x4* world)
    {
        u32 count = DrawnSubModels(model);
        u8* block = SetPlacedModelRenderDMA(&model->instances, &object->toScreen, object->mode, &object->toCamera, &object->clip,
                                            world, object);
        if (block != nullptr)
        {
            WriteSubModels(model, count, block, false);
        }
    }

    void SetDefaultMeshDMA(RigidModel* model, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
    {
        u32 count = DrawnSubModels(model);
        u8* block =
            SetPlacedModelRenderDMA(&model->instances, toScreen, DrawClipped, toCamera, &g_RenderView->clip, world, nullptr);
        if (block != nullptr)
        {
            WriteSubModels(model, count, block, true);
        }
    }

    void SetScreenModelDMA(ScreenModel* model, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                           const Vector4* clip)
    {
        u8* block = SetPlacedModelRenderDMA(&model->instances, toScreen, mode, toCamera, clip, nullptr, nullptr);
        if (block == nullptr)
        {
            return;
        }

        Material* material = model->material;
        if (material->writer.first == nullptr)
        {
            StartMaterialWriterTwoTags(material);
            material->drawnDirectly = 1;
            FlushMaterial(material);
        }

        RenderBucket& writer = material->writer;
        auto* packet = WriteSharedCall(reinterpret_cast<u8*>(BeginInsertedPacket(writer)), CallRenderTarget);
        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CallTag;
        at[1] = Address(block);
        at[2] = VifFlushE;
        at[3] = 0;
        at[4] = CountTag;
        at[5] = 0;
        at[6] = g_VuInstancesPlace | VifUnpackS;
        at[7] = g_VuInstances;
        at[8] = CallTag;
        at[9] = model->packet;
        at[10] = 0;
        at[11] = 0;
        EndInsertedPacket(writer, reinterpret_cast<u8*>(at + 12));
    }
}

void Platform::Graphics::DrawRigidModel(RigidModel* model, const Matrix4x4* matrix, const ModelLights& lights, u32 mode)
{
    SetRigidModelDma_(model, matrix, lights.directions, lights.colours, lights.ambient, mode);
}

namespace Platform::Graphics
{
void DrawPlacedModel(RigidModel* model, ChunkView* view, const Matrix4x4* world)
{
    SetPlacedModelDMA(model, reinterpret_cast<PlacedObject*>(view), world);
}

RigidModel* LodModelAt(Lod* lod, u32 distance)
{
    return LodMeshAt(lod, distance);
}
}

namespace Platform::Graphics
{
void BeginScreenModel()
{
    StartScreenModel();
}

void SetScreenModelMaterial(Material* material)
{
    ScreenModelMaterial(material);
}

void SetScreenModelColour(u32 colour)
{
    ScreenModelColour(colour);
}

void AddScreenModelVertex(const Vector4* place)
{
    ScreenModelVertex(place, 0);
}

ScreenModel* EndScreenModel()
{
    return FinishScreenModel();
}

// Its packet's memory and then the model's
void DeleteScreenModel(ScreenModel* model)
{
    if (model->packet != 0)
    {
        MemoryDeallocate2_(reinterpret_cast<void*>(model->packet));
    }

    MemoryDeallocate2_(model);
}

void DrawScreenModel(ScreenModel* model, const Matrix4x4* toScreen, const Matrix4x4* toClip, const Vector4* clip)
{
    SetScreenModelDMA(model, toScreen, DrawClipped, toClip, clip);
}
}
