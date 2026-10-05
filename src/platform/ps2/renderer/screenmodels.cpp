#include "renderer.h"

#include "game/memory.h"

#include <libgs.h>

// The screen models' builder (the skid marks are its only models): a triangle strip of one material made into a packet VU1
// draws as the vertexes come. It keeps the packet and the material in the 2D drawing's globals, and puts the vertexes it holds
// into the packet every 38 (the packet grows by them) and when the model is made

namespace
{
constexpr u32 HeldVertexes = 38;
// The packet's first quadword: a RET tag (its count filled when the model is made) and VIF1's MARK of 0xC8
constexpr u32 PacketMark = VifMark | 0xC8;
// The strip's GIF tag (its vertex count in it): the end of the packet, PRIM preset to a triangle strip, three registers a vertex
// (ST, RGBAQ and XYZ2)
constexpr u32 StripVertexRegisters = 3;
constexpr u64 StripRegisters = GifDescriptors(GifSt, GifRgbaq, GifXyz2);
// Where VU1 gets them: the GIF tag, the count it draws, then every vertex's place, colour (integers), texture coordinates and
// colour (floats) in four quadwords
constexpr u32 TagPlace = 0;
constexpr u32 CountPlace = 1;
constexpr u32 PlacePlace = 3;
constexpr u32 ColourPlace = 4;
constexpr u32 CoordinatePlace = 5;
constexpr u32 FloatColourPlace = 6;

struct Quadword
{
    u32 words[4];
};

// A colour channel in the low byte of a word (the word's other bytes as the memory had them)
struct ChannelWord
{
    u8 value;
    u8 unused01[3];
};

struct VertexColour
{
    ChannelWord channels[4];
};

u32 WordOf(const f32& value)
{
    return *reinterpret_cast<const u32*>(&value);
}

u64 StripTag(u32 count)
{
    GifTag tag;
    tag.value = count;
    tag.endOfPacket = 1;
    tag.setsPrim = 1;
    tag.prim = GS_PRIM_TRI_STRIP;
    tag.registerCount = StripVertexRegisters;
    return tag.value;
}

// A quadword of VIF1's NOPs and an UNPACK of V4-32 elements to the place
u32* UnpackV4(u32* at, u32 count, u32 place)
{
    at[0] = 0;
    at[1] = 0;
    at[2] = 0;
    at[3] = VifUnpackTo(VifUnpackV4Count | VifTops, place, count);
    return at + 4;
}
}

extern "C"
{
    // The vertexes held: their places (the fourth word VertexNoDraw or 0), normals and texture coordinates (nothing gives the
    // builder any: their counts stay 0) and colours; how many places (in the 2D drawing's count of pairs), texture coordinates,
    // colours and normals, the colour given last and whether one was given since the builder started
    extern Vector4 g_ScreenModelPlaces[HeldVertexes] RETAIL(D_003D4ED0);
    extern Vector4 g_ScreenModelNormals[HeldVertexes] RETAIL(D_003D5130);
    extern VertexColour g_ScreenModelColours[HeldVertexes] RETAIL(D_003D5390);
    extern Vector4 g_ScreenModelCoordinates[HeldVertexes] RETAIL(D_003D55F0);
    extern u32 g_ScreenModelPlaceCount RETAIL(D_0030AB30);
    extern u32 g_ScreenModelCoordinateCount RETAIL(D_0030AB34);
    extern u32 g_ScreenModelColourCount RETAIL(D_0030AB38);
    extern u8 g_ScreenModelColoured RETAIL(D_0030AB3C);
    extern u32 g_ScreenModelColour RETAIL(D_0030AB48);
    extern u32 g_ScreenModelNormalCount RETAIL(D_0030AB4C);
    // The packet's size so far (quadwords) and its RET tag
    extern u32 g_ScreenModelSize RETAIL(D_0030AB40);
    extern u32* g_ScreenModelReturn RETAIL(D_0030AB44);

    InstanceBlockOwner* InstanceBlockOwnerConstruct(InstanceBlockOwner* owner) RETAIL(FUN_001a26c8);
    // The vertexes held put into the packet
    void FlushScreenModel() RETAIL(FUN_001a6b98);

    void StartScreenModel()
    {
        g_2DMaterial = g_ScreenModelMaterial;
        g_ScreenModelColour = 0;
        g_ScreenModelPlaceCount = 0;
        g_ScreenModelColourCount = 0;
        g_ScreenModelNormalCount = 0;
        g_ScreenModelCoordinateCount = 0;
        g_2DTag = nullptr;
        g_2DGifTag = nullptr;
        g_ScreenModelReturn = nullptr;
        g_ScreenModelSize = 0;
        g_2DUsesSt = 0;
        g_ScreenModelColoured = 0;
    }

    void ScreenModelMaterial(Material* material)
    {
        if (material != nullptr)
        {
            g_2DMaterial = material;
        }
    }

    // No room is checked for: the colours are only put into the packet with the places
    void ScreenModelColour(u32 colour)
    {
        VertexColour& held = g_ScreenModelColours[g_ScreenModelColourCount];
        held.channels[0].value = static_cast<u8>(colour);
        held.channels[1].value = static_cast<u8>(colour >> 8);
        held.channels[2].value = static_cast<u8>(colour >> 16);
        held.channels[3].value = static_cast<u8>(colour >> 24);
        g_ScreenModelColourCount++;
        g_ScreenModelColour = colour;
        g_ScreenModelColoured = 1;
    }

    void ScreenModelVertex(const Vector4* place, u32 noDraw)
    {
        if (g_ScreenModelPlaceCount + 1 > HeldVertexes)
        {
            FlushScreenModel();
        }

        Vector4& held = g_ScreenModelPlaces[g_ScreenModelPlaceCount];
        held.x = place->x;
        held.y = place->y;
        held.z = place->z;
        *reinterpret_cast<u32*>(&held.w) = noDraw != 0 ? VertexNoDraw : 0;
        g_ScreenModelPlaceCount++;
    }

    // The packet (made with a RET tag first, or grown by them) gets the strip's GIF tag, the strip's size (four quadwords a
    // vertex) and its tag's first word, each place, colour (integers, the place's fourth word added to the last two), texture
    // coordinates and colour (floats: none when no colour was given) and MSCAL. A vertex without a colour of its own takes the
    // colour given last
    void FlushScreenModel()
    {
        bool grows = g_2DTag != nullptr;
        u32 count = g_ScreenModelPlaceCount;
        u32 size = count * 2 + 6;
        if (!grows)
        {
            size++;
        }

        if (g_ScreenModelCoordinateCount != 0)
        {
            size += count + 1;
        }

        if (g_ScreenModelColourCount != 0 || g_ScreenModelColoured != 0)
        {
            size += count + 1;
        }

        u32* at;
        if (grows)
        {
            auto* packet = static_cast<Quadword*>(MemoryAllocate2((size + g_ScreenModelSize) * sizeof(Quadword)));
            const auto* old = reinterpret_cast<const Quadword*>(g_2DTag);
            for (u32 index = 0; index < g_ScreenModelSize; index++)
            {
                packet[index] = old[index];
            }

            at = packet[g_ScreenModelSize].words;
            if (g_2DTag != nullptr)
            {
                MemoryDeallocate_(g_2DTag);
            }

            g_ScreenModelReturn = packet->words;
            size += g_ScreenModelSize;
            g_2DTag = packet->words;
        }
        else
        {
            at = static_cast<u32*>(MemoryAllocate2(size * sizeof(Quadword)));
            g_2DTag = at;
            g_ScreenModelReturn = at;
            at[0] = ReturnTag;
            at[1] = 0;
            at[2] = PacketMark;
            at[3] = 0;
            at += 4;
        }

        count = g_ScreenModelPlaceCount;
        at = UnpackV4(at, 1, TagPlace);
        auto* tag = reinterpret_cast<u64*>(at);
        tag[0] = StripTag(count);
        tag[1] = StripRegisters;
        at += 4;
        at[0] = VifCycle1;
        at[1] = VifUnpackV2 | VifTops | CountPlace;
        at[2] = count << 2;
        at[3] = count | GifEndOfPacket;
        at += 4;
        at[0] = 0;
        at[1] = 0;
        at[2] = VifCycle4;
        at[3] = VifUnpackTo(VifUnpackV4Count | VifTops, PlacePlace, count);
        at += 4;
        for (u32 index = 0; index < count; index++)
        {
            const Vector4& place = g_ScreenModelPlaces[index];
            at[0] = WordOf(place.x);
            at[1] = WordOf(place.y);
            at[2] = WordOf(place.z);
            at += 4;
        }

        u32 colour = g_ScreenModelColour;
        at = UnpackV4(at, count, ColourPlace);
        for (u32 index = 0; index < count; index++)
        {
            // The normals go where the colours go next
            if (g_ScreenModelNormalCount != 0 && index < g_ScreenModelNormalCount)
            {
                auto* values = reinterpret_cast<f32*>(at);
                const Vector4& normal = g_ScreenModelNormals[index];
                values[0] = normal.x;
                values[1] = normal.y;
                values[2] = normal.z;
            }

            if (g_ScreenModelColourCount != 0 && index < g_ScreenModelColourCount)
            {
                const VertexColour& held = g_ScreenModelColours[index];
                for (u32 channel = 0; channel < 4; channel++)
                {
                    at[channel] = held.channels[channel].value;
                }
            }
            else
            {
                for (u32 channel = 0; channel < 4; channel++)
                {
                    at[channel] = colour >> (channel * 8) & 0xFF;
                }
            }

            at[2] |= WordOf(g_ScreenModelPlaces[index].w);
            at[3] |= WordOf(g_ScreenModelPlaces[index].w);
            at += 4;
        }

        // Room was made for as many as the places (nothing gives the builder any)
        u32 coordinates = g_ScreenModelCoordinateCount;
        if (coordinates != 0)
        {
            at = UnpackV4(at, coordinates, CoordinatePlace);
            for (u32 index = 0; index < coordinates; index++)
            {
                auto* values = reinterpret_cast<f32*>(at);
                const Vector4& coordinate = g_ScreenModelCoordinates[index];
                values[0] = coordinate.x;
                values[1] = coordinate.y;
                values[2] = coordinate.z;
                at += 4;
            }
        }

        u32 colours = g_ScreenModelColourCount;
        if (colours != 0 || g_ScreenModelColoured != 0)
        {
            count = g_ScreenModelPlaceCount;
            colour = g_ScreenModelColour;
            at = UnpackV4(at, count, FloatColourPlace);
            for (u32 index = 0; index < count; index++)
            {
                auto* values = reinterpret_cast<f32*>(at);
                for (u32 channel = 0; channel < 4; channel++)
                {
                    u32 value = index < colours ? *reinterpret_cast<const u32*>(&g_ScreenModelColours[index].channels[channel])
                                                : colour >> (channel * 8);
                    values[channel] = static_cast<f32>(value & 0xFF);
                }

                at += 4;
            }
        }

        at[0] = VifCycle1;
        at[1] = VifMscal;
        at[2] = VifFlushE;
        at[3] = 0;
        g_ScreenModelSize = size;
        g_ScreenModelPlaceCount = 0;
        g_ScreenModelColourCount = 0;
        g_ScreenModelNormalCount = 0;
        g_ScreenModelCoordinateCount = 0;
    }

    // The model's packet ends with what's still held, its RET tag counting its quadwords after it. Retail bug: a model of no
    // vertexes has no packet, and its RET tag goes to address 0
    ScreenModel* FinishScreenModel()
    {
        auto* model = static_cast<ScreenModel*>(MemoryAllocate(sizeof(ScreenModel)));
        InstanceBlockOwnerConstruct(&model->instances);
        model->material = nullptr;
        model->packet = 0;
        if (g_ScreenModelPlaceCount != 0)
        {
            FlushScreenModel();
        }

        *g_ScreenModelReturn = (g_ScreenModelSize - 1) | ReturnTag;
        model->material = g_2DMaterial;
        model->packet = Address(g_2DTag);
        model->instances.block = nullptr;
        return model;
    }
}
