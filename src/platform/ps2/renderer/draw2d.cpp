#include "renderer.h"

#include "platform/graphics.h"

namespace
{
// The GS registers the 2D vertexes set: RGBAQ, ST, XYZ2 (a vertex that draws) and XYZ3 (one that doesn't)
constexpr u64 Rgbaq = 0x01;
constexpr u64 St = 0x02;
constexpr u64 Xyz2 = 0x05;
constexpr u64 Xyz3 = 0x0D;
// RGBAQ's Q of 1.0f
constexpr u64 QOne = 0x3F800000ull << 32;
// The primitives' GIF tag: as many A+D pairs as were written, the end of the packet, PRIM preset to a triangle strip
constexpr u64 TriangleStripTag = 0x8012ull << 45 | 0x8000;
// A shader's header bit: it has a texture
constexpr u64 Textured = 1ull << 27;
// The 2D vertexes' depth: in front of everything
constexpr u32 FrontDepth = 0xFFFFFFFF;

// The frame's corner along a side of the frame's size in the GS's coordinates (1/16 pixels, the screen's middle at 2048), the
// frame's offset added
f32 FrameCorner(s32 size, s32 offset)
{
    return static_cast<f32>((static_cast<s32>(2048.0f - static_cast<f32>(size >> 1)) + offset) << 4);
}

u64* PairAt(u8* packet)
{
    return reinterpret_cast<u64*>(packet);
}

u8* WritePair(u8* packet, u64 data, u64 address)
{
    PairAt(packet)[0] = data;
    PairAt(packet)[1] = address;
    g_2DPairCount++;
    return packet + 0x10;
}
}

extern "C"
{
    u8* WriteShaderRegisters(const Material* material, u8* packet, u32)
    {
        Shader* shader = material != nullptr ? material->shaders[0] : nullptr;
        u32 registers = CallVirtual<u32>(shader, shader->vtable, 7);
        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CallVirtual<u32>(shader, shader->vtable, 4, 0u) | ReferenceTag;
        at[1] = registers;
        at[2] = 0;
        at[3] = CallVirtual<u32>(shader, shader->vtable, 4, 0u) | VifDirect;
        return packet + 0x10;
    }

    u8* Begin2D(Material* material)
    {
        RenderBucket& bucket = g_FrameBuckets.buckets[material->bucket];
        auto* packet = reinterpret_cast<u8*>(BeginPacket(bucket));
        // The bucket's last 2D material set up the drawing already: the CALL of the render target's packet, the texture uploaded
        // and its registers (a textured shader's), the shader's registers
        if (bucket.last2DMaterial != material)
        {
            bucket.last2DMaterial = material;
            packet = FUN_001a0dd0(packet, 1);
            Shader* shader = material != nullptr ? material->shaders[0] : nullptr;
            if ((*reinterpret_cast<const u64*>(shader) & Textured) != 0)
            {
                packet = FUN_001c0ab8(material, packet, 0, material->bucket);
                packet = FUN_001c0b00(material, packet, 0);
            }

            packet = WriteShaderRegisters(material, packet, 0);
        }

        g_2DTag = reinterpret_cast<u32*>(packet);
        g_2DMaterial = material;
        g_2DGifTag = reinterpret_cast<u64*>(packet + 0x10);
        g_2DUsesSt = 0;
        g_2DPairCount = 0;
        return packet + 0x20;
    }

    void End2D(u8* end)
    {
        u32 count = g_2DPairCount;
        u32* tag = g_2DTag;
        tag[0] = (count + 1) | CountTag;
        tag[1] = 0;
        tag[2] = VifFlushA;
        tag[3] = (count + 1) | VifDirect;
        u64* gif = g_2DGifTag;
        gif[0] = static_cast<u64>(count | 0x8000) | TriangleStripTag;
        gif[1] = 0xE;
        g_2DGifTag = gif + 2;
        EndPacket(g_FrameBuckets.buckets[g_2DMaterial->bucket], end);
    }

    u8* Write2DColour(u8* packet, u32 colour)
    {
        return WritePair(packet, static_cast<u64>(colour) | QOne, Rgbaq);
    }

    u8* Write2DVertex(u8* packet, f32 x, f32 y, u32 z, u32 withoutKick)
    {
        // The asm ORs the integers sign-extended: a negative x or y sets the bits above it
        u64 value = static_cast<u64>(static_cast<s64>(static_cast<s32>(x))) |
                    static_cast<u64>(static_cast<s64>(static_cast<s32>(y))) << 16 | static_cast<u64>(z) << 32;
        return WritePair(packet, value, withoutKick != 0 ? Xyz3 : Xyz2);
    }

    u8* Write2DTexCoord(u8* packet, f32 s, f32 t)
    {
        g_2DUsesSt = 1;
        u64 value = __builtin_bit_cast(u32, s) | static_cast<u64>(__builtin_bit_cast(u32, t)) << 32;
        return WritePair(packet, value, St);
    }
}

// The UI's 2D drawers' shapes (their vertexes' order and the strips' first two that don't draw are the drawers')
void Platform::Graphics::DrawSprite(Material* material, u32 colour, const Rectangle& place, const Rectangle& texture)
{
    const RenderTargetDescription* target = g_RenderTarget;
    f32 width = static_cast<f32>(g_ScreenWidth);
    f32 height = static_cast<f32>(g_ScreenHeight);
    f32 left = place.x * width * 16.0f + FrameCorner(target->displayWidth, target->offsetX);
    f32 top = place.y * height * 16.0f + FrameCorner(target->displayHeight, target->offsetY);
    f32 right = left + place.width * width * 16.0f;
    f32 bottom = top + place.height * height * 16.0f;
    f32 textureRight = texture.x + texture.width;
    f32 textureBottom = texture.y + texture.height;
    u8* packet = Begin2D(material);
    packet = Write2DColour(packet, colour);
    packet = Write2DTexCoord(packet, texture.x, texture.y);
    packet = Write2DVertex(packet, left, top, FrontDepth, 1);
    packet = Write2DTexCoord(packet, texture.x, textureBottom);
    packet = Write2DVertex(packet, left, bottom, FrontDepth, 1);
    packet = Write2DTexCoord(packet, textureRight, texture.y);
    packet = Write2DVertex(packet, right, top, FrontDepth, 0);
    packet = Write2DTexCoord(packet, textureRight, textureBottom);
    packet = Write2DVertex(packet, right, bottom, FrontDepth, 0);
    End2D(packet);
}

void Platform::Graphics::DrawTurnedSprite(Material* material, u32 colour, const Vector2* corners, const Rectangle& texture)
{
    const RenderTargetDescription* target = g_RenderTarget;
    f32 width = static_cast<f32>(g_ScreenWidth) * 16.0f;
    f32 height = static_cast<f32>(g_ScreenHeight) * 16.0f;
    f32 left = FrameCorner(target->displayWidth, 0);
    f32 top = FrameCorner(target->displayHeight, 0);
    const Vector2 coordinates[4] = {
        {texture.x, texture.y},
        {texture.x, texture.y + texture.height},
        {texture.x + texture.width, texture.y},
        {texture.x + texture.width, texture.y + texture.height},
    };
    u8* packet = Begin2D(material);
    packet = Write2DColour(packet, colour);
    for (u32 corner = 0; corner < 4; corner++)
    {
        f32 x = corners[corner].x * width + left;
        f32 y = corners[corner].y * height + top;
        packet = Write2DTexCoord(packet, coordinates[corner].x, coordinates[corner].y);
        packet = Write2DVertex(packet, x, y, FrontDepth, corner < 2);
    }

    End2D(packet);
}

void Platform::Graphics::DrawStrip(Material* material, u32 count, const Vector2* places, const u32* colours)
{
    const RenderTargetDescription* target = g_RenderTarget;
    f32 left = FrameCorner(target->displayWidth, target->offsetX);
    f32 top = FrameCorner(target->displayHeight, target->offsetY);
    u8* packet = Begin2D(material);
    for (u32 vertex = 0; vertex < count; vertex++)
    {
        f32 x = places[vertex].x * 16.0f + left;
        f32 y = places[vertex].y * 16.0f + top;
        packet = Write2DColour(packet, colours[vertex]);
        packet = Write2DVertex(packet, x, y, FrontDepth, vertex < 2);
    }

    End2D(packet);
}
