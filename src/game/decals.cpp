#include "game/decals.h"

#include "game/chunkdata.h"
#include "game/memory.h"
#include "game/reference.h"
#include "game/stream.h"
#include "game/view.h"

#include "platform/graphics.h"

namespace
{
constexpr s32 BlockCount = 32;
constexpr s32 BlockDecals = 32;
constexpr s32 TypeCount = 16;
constexpr s16 NoBlock = -1;
constexpr s16 FreeKey = -1;
// A decal slot without a decal: its place's w and its first sizes' fourth half word
constexpr f32 NoDecal = -1.0f;
constexpr s16 NoSizes = 0xBA;
// The packet's VIF codes: UNPACKs of the places (V4-32), sizes (V4-16) and colours (V4-8) to their VU1 addresses, and the MSCNT
constexpr u32 PlacesUnpack = 0x6C208000;
constexpr u32 SizesUnpack = 0x6D408020;
constexpr u32 ColoursUnpack = 0x6E208060;
constexpr u32 StartProgram = 0x17000000;
// The UV packet's GIF tag (its registers and their count)
constexpr u64 UvGifTag = 0xC002400000008008;
constexpr u32 UvRegisters = 0x12512512;
constexpr u32 UvMoreRegisters = 0x5125;
// The two types the default data has without its section: the page's top and bottom halves (their UV rectangles)
constexpr u32 DefaultTypes = 2;
constexpr Vector4 DefaultTypeRectangles[DefaultTypes] = {{0.25f, 0.25f, 0.75f, 0.25f}, {0.25f, 0.75f, 0.75f, 0.75f}};
constexpr f32 FramesPerSecond = 60.0f;
constexpr s32 FlagBits = 0x3F;
constexpr s32 LifeShift = 6;
// The frames' directions' unit, and the lengths the frames take as none
constexpr f32 FrameUnit = 32768.0f;
constexpr f32 LengthEpsilon = 0x1.5798ECp-29f;
constexpr f32 ApartEnough = 0x1.0624DEp-10f;
constexpr f32 TooClose = 0x1.FAE148p-1f;
// Footfalls stand a little above the ground
constexpr f32 Lift = 0x1.70A3D8p-4f;
// A frame shorter than this takes nothing off the decals' lives
constexpr f32 NoTime = 0x1.A36E2Ep-14f;
constexpr f32 SecondsPerFrame = 0x1.111112p-6f;

// Its decals all none: no place, no sizes
void EmptyDecals(DecalBlock* block)
{
    for (s32 slot = 0; slot < BlockDecals; slot++)
    {
        block->places[slot].w = NoDecal;
        block->sizes[slot][3] = NoSizes;
    }
}

// The cross product of two vectors' x, y and z
void Cross(const Vector4& a, const Vector4& b, f32* out)
{
    out[0] = a.y * b.z - a.z * b.y;
    out[1] = a.z * b.x - a.x * b.z;
    out[2] = a.x * b.y - a.y * b.x;
}
}

extern "C"
{
    void InitDecalPool(DecalData* data)
    {
        for (s32 index = 0; index < BlockCount; index++)
        {
            DecalBlock* block = &data->blocks[index];
            block->count = 0;
            block->index = 0;
            block->key = FreeKey;
            block->next = nullptr;
            block->drawNext = nullptr;
            ClearDecalBlock(block);
            for (u32 word = 0; word < 4; word++)
            {
                block->placesUnpack[word] = 0;
                block->sizesUnpack[word] = 0;
                block->coloursUnpack[word] = 0;
                block->start[word] = 0;
            }

            block->start[3] = StartProgram;
            block->placesUnpack[3] = PlacesUnpack;
            block->sizesUnpack[3] = SizesUnpack;
            block->coloursUnpack[3] = ColoursUnpack;
        }

        data->count = 0;
        for (s32 type = 0; type < TypeCount; type++)
        {
            data->types[type] = nullptr;
            data->typeBlocks[type] = nullptr;
        }

        data->freeBlock = 0;
        for (DecalBlock*& list : data->drawLists)
        {
            list = nullptr;
        }

        for (s32 index = 0; index < BlockCount; index++)
        {
            data->nextFree[index] = static_cast<s16>(index + 1);
            data->blocks[index].index = static_cast<s16>(index);
        }

        data->nextFree[BlockCount - 1] = NoBlock;
        data->chunk = nullptr;
    }

    void ClearDecalBlock(DecalBlock* block)
    {
        block->count = 0;
        EmptyDecals(block);
    }

    DecalBlock* AllocDecalBlock(DecalData* data, s16 key, s16 type)
    {
        for (DecalBlock* block = data->typeBlocks[type]; block != nullptr; block = block->next)
        {
            if (block->count < BlockDecals && block->key == key)
            {
                return block;
            }
        }

        if (data->freeBlock == NoBlock)
        {
            return nullptr;
        }

        DecalBlock* block = &data->blocks[data->freeBlock];
        data->freeBlock = data->nextFree[data->freeBlock];
        block->key = key;
        block->next = data->typeBlocks[type];
        data->typeBlocks[type] = block;
        return block;
    }

    void FreeDecalBlock(DecalData* data, DecalBlock* block)
    {
        data->nextFree[block->index] = data->freeBlock;
        data->freeBlock = block->index;
    }

    void ResetDecalPool(DecalData* data)
    {
        for (s32 type = 0; type < TypeCount; type++)
        {
            if (data->types[type] != nullptr)
            {
                MemoryDeallocate2_(data->types[type]);
            }

            data->types[type] = nullptr;
            data->typeBlocks[type] = nullptr;
        }

        for (DecalBlock*& list : data->drawLists)
        {
            list = nullptr;
        }

        data->freeBlock = 0;
        for (s32 index = 0; index < BlockCount; index++)
        {
            DecalBlock* block = &data->blocks[index];
            data->nextFree[index] = static_cast<s16>(index + 1);
            block->count = 0;
            EmptyDecals(block);
            block->index = static_cast<s16>(index);
        }

        data->nextFree[BlockCount - 1] = NoBlock;
        data->chunk = nullptr;
        data->count = 0;
    }

    void ClearDecals(DecalData* data)
    {
        for (s32 type = 0; type < TypeCount; type++)
        {
            while (data->typeBlocks[type] != nullptr)
            {
                DecalBlock* block = data->typeBlocks[type];
                block->count = 0;
                EmptyDecals(block);
                data->typeBlocks[type] = block->next;
                data->nextFree[block->index] = data->freeBlock;
                data->freeBlock = block->index;
            }
        }

        data->chunk = nullptr;
        data->count = 0;
    }

    void ReadDecalData(DecalData* data, Stream* stream)
    {
        ResetDecalPool(data);
        stream->Read(&g_DecalUnusedInt, sizeof(g_DecalUnusedInt), 1);
        stream->Read(data->uvPacket, 0x420, 1);
        *reinterpret_cast<u64*>(data->uvPacket) = UvGifTag;
        reinterpret_cast<u32*>(data->uvPacket)[2] = UvRegisters;
        reinterpret_cast<u32*>(data->uvPacket)[3] = UvMoreRegisters;
        // The tools' pointers: a type follows for each that isn't null
        stream->Read(data->types, sizeof(data->types), 1);
        for (s32 type = 0; type < TypeCount; type++)
        {
            if (data->types[type] == nullptr)
            {
                continue;
            }

            auto* read = static_cast<DecalType*>(MemoryAllocate(sizeof(DecalType)));
            read->variantCount = 0;
            data->types[type] = read;
            stream->Read(read, sizeof(DecalType), 1);
        }
    }

    void OrthonormalizeDecalFrame(DecalDescriptor* decal)
    {
        Vector4& normal = decal->normal;
        Vector4& direction = decal->direction;
        f32 inverse = InverseLength(&normal, LengthEpsilon);
        normal.x = normal.x * inverse;
        normal.y = normal.y * inverse;
        normal.z = normal.z * inverse;
        inverse = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;
        f32 along = normal.x * direction.x + normal.y * direction.y + normal.z * direction.z;
        if (!(ApartEnough < along))
        {
            return;
        }

        Vector4 side;
        if (along < TooClose)
        {
            // Perpendicular to the normal in their plane: the normal's cross product with the normal and direction's
            Cross(normal, direction, &side.x);
            side.w = 1.0f;
            inverse = InverseLength(&side, LengthEpsilon);
            side.x = side.x * inverse;
            side.y = side.y * inverse;
            side.z = side.z * inverse;
            f32 turned[3];
            Cross(side, normal, turned);
            direction.x = turned[0];
            direction.y = turned[1];
            direction.w = 1.0f;
            direction.z = turned[2];
        }
        else
        {
            // Too close to tell: perpendicular to the normal and a random vector
            side.x = RandomSigned();
            side.y = RandomSigned();
            side.z = RandomSigned();
            f32 turned[3];
            Cross(normal, side, turned);
            direction.x = turned[0];
            direction.y = turned[1];
            direction.w = 1.0f;
            direction.z = turned[2];
        }

        inverse = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;
    }

    void SetDecalFrame(DecalDescriptor* decal, DecalBlock* block, s32 slot)
    {
        OrthonormalizeDecalFrame(decal);
        f32 normal[3];
        Cross(decal->normal, decal->direction, normal);
        DecalFrame& frame = block->frames[slot];
        frame.normal[0] = static_cast<s16>(static_cast<s32>(normal[0] * FrameUnit));
        frame.normal[1] = static_cast<s16>(static_cast<s32>(normal[1] * FrameUnit));
        frame.normal[2] = static_cast<s16>(static_cast<s32>(normal[2] * FrameUnit));
        frame.direction[2] = static_cast<s16>(static_cast<s32>(decal->direction.z * FrameUnit));
        frame.direction[0] = static_cast<s16>(static_cast<s32>(decal->direction.x * FrameUnit));
        frame.direction[1] = static_cast<s16>(static_cast<s32>(decal->direction.y * FrameUnit));
    }

    ChunkData* CameraChunk()
    {
        Reference* camera = g_RenderView->cameraObject;
        ReferencedObject* object = camera != nullptr ? camera->object : nullptr;
        return object != nullptr ? object->chunk : nullptr;
    }

    void AddDecal(DecalData* data, DecalDescriptor* decal)
    {
        ChunkData* camera = CameraChunk();
        if (data->chunk != nullptr && camera != data->chunk)
        {
            return;
        }

        // Into the camera's chunk's space
        if (decal->chunk != camera)
        {
            Matrix4x4 into = decal->chunk->drawMatrix;
            VuTransformPoint(&into, &decal->place, &decal->place);
            VuRotateVector(&into, &decal->normal, &decal->normal);
            VuRotateVector(&into, &decal->direction, &decal->direction);
        }

        DecalBlock* block = AllocDecalBlock(data, static_cast<s16>(decal->key), static_cast<s16>(decal->type));
        const DecalType* type = data->types[decal->type];
        if (block == nullptr)
        {
            return;
        }

        s32 slot = block->count;
        block->count = static_cast<s16>(block->count + 1);
        // Its life in frames and its flags in its place's w
        f32 life = type->variants[decal->variant].sizes[0][3];
        block->places[slot] = decal->place;
        s32 lifeAndFlags = static_cast<s32>(life * FramesPerSecond) << LifeShift | (decal->flags & FlagBits);
        reinterpret_cast<s32*>(&block->places[slot])[3] = lifeAndFlags;
        block->variants[slot] = static_cast<s8>(decal->variant);
        SetDecalFrame(decal, block, slot);
        data->count++;
    }

    void AddDecalFromDescriptor(const Matrix4x4* frame, ChunkData* chunk)
    {
        DecalDescriptor decal;
        decal.place = *reinterpret_cast<const Vector4*>(frame->m[3]);
        decal.place.y = decal.place.y + Lift;
        decal.chunk = chunk;
        decal.normal = *reinterpret_cast<const Vector4*>(frame->m[1]);
        decal.direction = *reinterpret_cast<const Vector4*>(frame->m[2]);
        decal.variant = 0;
        decal.type = 0;
        decal.flags = 0;
        decal.key = 0;
        AddDecal(&g_DecalData, &decal);
    }

    s32 UploadDecalCameraToVU0(DecalData* data)
    {
        ChunkData* camera = CameraChunk();
        if (camera == nullptr)
        {
            return 0;
        }

        ChunkData* from = data->chunk;
        Platform::Graphics::LoadDecalView(&camera->matrix, from != nullptr && from != camera ? &from->drawMatrix : nullptr);
        data->chunk = camera;
        return 1;
    }

    s32 UpdateDecalsVU0(DecalData* data, f32 delta)
    {
        if (data->count == 0)
        {
            return 0;
        }

        if (UploadDecalCameraToVU0(data) == 0)
        {
            return 0;
        }

        s32 lifeStep = NoTime < delta ? -1 : 0;
        for (s32 type = 0; type < TypeCount; type++)
        {
            if (data->typeBlocks[type] == nullptr)
            {
                continue;
            }

            Platform::Graphics::LoadDecalType(data->types[type]);
            DecalBlock* previous = nullptr;
            DecalBlock* block = data->typeBlocks[type];
            while (block != nullptr)
            {
                s32 decals = block->count;
                s32 kept = 0;
                s32 variant = block->variants[0];
                for (s32 slot = 0; slot < decals; slot++)
                {
                    DecalFrame frame = block->frames[slot];
                    s8 ownVariant = block->variants[slot];
                    f32 place[4] = {block->places[slot].x, block->places[slot].y, block->places[slot].z, 0.0f};
                    s32 lifeAndFlags = reinterpret_cast<const s32*>(&block->places[slot])[3];
                    s32 life = lifeAndFlags >> LifeShift;
                    s32 flags = lifeAndFlags & FlagBits;
                    place[3] = static_cast<f32>(life) * SecondsPerFrame;
                    s32 words[8] = {frame.normal[0],    frame.normal[1],    frame.normal[2],    frame.unused06,
                                    frame.direction[0], frame.direction[1], frame.direction[2], frame.unused0E};
                    Platform::Graphics::DecalLook look;
                    Platform::Graphics::AgeDecal(place, words, variant, &look);
                    // Into the next slot kept, its life a frame less (dropped once that's below 0)
                    block->frames[kept] = frame;
                    block->variants[kept] = ownVariant;
                    life = life + lifeStep;
                    Vector4& out = block->places[kept];
                    out.x = look.place[0];
                    out.y = look.place[1];
                    out.z = look.place[2];
                    reinterpret_cast<s32*>(&out)[3] = life << LifeShift | flags;
                    block->colours[kept] = static_cast<u32>(look.colour[3]) << 24 | static_cast<u32>(look.colour[2]) << 16 |
                                           static_cast<u32>(look.colour[1]) << 8 | look.colour[0];
                    for (u32 value = 0; value < 4; value++)
                    {
                        block->sizes[kept][value] = look.sizes[value];
                        block->moreSizes[kept][value] = look.moreSizes[value];
                    }

                    if (life < 0)
                    {
                        data->count--;
                    }
                    else
                    {
                        kept++;
                    }
                }

                block->count = static_cast<s16>(kept);
                for (s32 slot = kept; slot < decals; slot++)
                {
                    block->sizes[slot][3] = NoSizes;
                }

                DecalBlock* next = block->next;
                if (kept != 0)
                {
                    block->drawNext = data->drawLists[block->key];
                    data->drawLists[block->key] = block;
                    previous = block;
                    block = next;
                    continue;
                }

                if (previous == nullptr)
                {
                    data->typeBlocks[type] = next;
                }
                else
                {
                    previous->next = next;
                }

                FreeDecalBlock(data, block);
                block = next;
            }
        }

        return 1;
    }

    void ReadDecalPage(DecalData* data, Stream* stream)
    {
        Platform::Graphics::ReadParticlePage(&data->page, stream, true);
    }

    void LoadDecals(DecalData* data, const char* path)
    {
        Platform::Graphics::LoadParticlePage(&data->page, path, true);
        SetDefaultDecalTypes(data);
    }

    void SetDefaultDecalTypes(DecalData* data)
    {
        data->typeCount = DefaultTypes;
        *reinterpret_cast<u64*>(data->uvPacket) = UvGifTag;
        reinterpret_cast<u32*>(data->uvPacket)[2] = UvRegisters;
        reinterpret_cast<u32*>(data->uvPacket)[3] = UvMoreRegisters;
        auto* rectangles = reinterpret_cast<Vector4*>(data->uvPacket + sizeof(Vector4));
        for (u32 type = 0; type < DefaultTypes; type++)
        {
            rectangles[type] = DefaultTypeRectangles[type];
        }
    }
}

EABI_EXPORT(UpdateDecalsVU0, UpdateDecalsVU0);
