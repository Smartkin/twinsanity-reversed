#include "renderer.h"

#include "platform/graphics.h"

#include <bit>
#include <libgs.h>

namespace
{
// RGBAQ's Q of 1.0f (the colour's RGBA bytes ORed in)
const u64 QOne = std::bit_cast<u64>(GS_RGBAQ{.q = 1.0f});

// The frame's corner along a side of the frame's size in the GS's coordinates (1/16 pixels, the screen's middle at 2048), the
// frame's offset added
f32 FrameCorner(s32 size, s32 offset)
{
    return static_cast<f32>((static_cast<s32>(GsScreenMiddleFloat - static_cast<f32>(size >> 1)) + offset) << GsSubpixelShift);
}

u64* PairAt(u8* packet)
{
    return reinterpret_cast<u64*>(packet);
}

u8* WritePair(u8* packet, u64 value, u64 address)
{
    PairAt(packet)[0] = value;
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
        u32 registers = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSlot);
        auto* at = reinterpret_cast<u32*>(packet);
        at[0] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | ReferenceTag;
        at[1] = registers;
        at[2] = 0;
        at[3] = CallVirtual<u32>(shader, shader->vtable, ShaderRegistersSizeSlot, 0u) | VifDirect;
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
            packet = WriteSharedCall(packet, CallRenderTarget);
            Shader* shader = material != nullptr ? material->shaders[0] : nullptr;
            if (shader->settings.textured != 0)
            {
                packet = UploadShaderTexture(material, packet, 0, material->bucket);
                packet = WriteShaderTextureRegisters(material, packet, 0);
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
        gif[0] = AddressDataTag(count, GS_PRIM_TRI_STRIP);
        gif[1] = GifAddressData;
        g_2DGifTag = gif + 2;
        EndPacket(g_FrameBuckets.buckets[g_2DMaterial->bucket], end);
    }

    u8* Write2DColour(u8* packet, u32 colour)
    {
        return WritePair(packet, static_cast<u64>(colour) | QOne, GsRgbaq);
    }

    u8* Write2DVertex(u8* packet, f32 x, f32 y, u32 z, u32 withoutKick)
    {
        // The asm ORs the integers sign-extended: a negative x or y sets the bits above it
        u64 value = static_cast<u64>(static_cast<s64>(static_cast<s32>(x))) |
                    static_cast<u64>(static_cast<s64>(static_cast<s32>(y))) << GsXyzYShift |
                    static_cast<u64>(z) << GsXyzDepthShift;
        return WritePair(packet, value, withoutKick != 0 ? GsXyz3 : GsXyz2);
    }

    u8* Write2DTexCoord(u8* packet, f32 s, f32 t)
    {
        g_2DUsesSt = 1;
        return WritePair(packet, std::bit_cast<u64>(GS_ST{.s = s, .t = t}), GsSt);
    }
}

// The UI's 2D drawers' shapes (their vertexes' order and the strips' first two that don't draw are the drawers')
void Platform::Graphics::DrawSprite(Material* material, u32 colour, const Rectangle& place, const Rectangle& texture)
{
    const RenderTargetDescription* target = g_RenderTarget;
    f32 width = static_cast<f32>(g_ScreenWidth);
    f32 height = static_cast<f32>(g_ScreenHeight);
    f32 left = place.x * width * GsSubpixels + FrameCorner(target->displayWidth, target->offsetX);
    f32 top = place.y * height * GsSubpixels + FrameCorner(target->displayHeight, target->offsetY);
    f32 right = left + place.width * width * GsSubpixels;
    f32 bottom = top + place.height * height * GsSubpixels;
    f32 textureRight = texture.x + texture.width;
    f32 textureBottom = texture.y + texture.height;
    u8* packet = Begin2D(material);
    packet = Write2DColour(packet, colour);
    packet = Write2DTexCoord(packet, texture.x, texture.y);
    packet = Write2DVertex(packet, left, top, GsFrontDepth, 1);
    packet = Write2DTexCoord(packet, texture.x, textureBottom);
    packet = Write2DVertex(packet, left, bottom, GsFrontDepth, 1);
    packet = Write2DTexCoord(packet, textureRight, texture.y);
    packet = Write2DVertex(packet, right, top, GsFrontDepth, 0);
    packet = Write2DTexCoord(packet, textureRight, textureBottom);
    packet = Write2DVertex(packet, right, bottom, GsFrontDepth, 0);
    End2D(packet);
}

void Platform::Graphics::DrawTurnedSprite(Material* material, u32 colour, const Vector2* corners, const Rectangle& texture)
{
    const RenderTargetDescription* target = g_RenderTarget;
    f32 width = static_cast<f32>(g_ScreenWidth) * GsSubpixels;
    f32 height = static_cast<f32>(g_ScreenHeight) * GsSubpixels;
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
        packet = Write2DVertex(packet, x, y, GsFrontDepth, corner < 2);
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
        f32 x = places[vertex].x * GsSubpixels + left;
        f32 y = places[vertex].y * GsSubpixels + top;
        packet = Write2DColour(packet, colours[vertex]);
        packet = Write2DVertex(packet, x, y, GsFrontDepth, vertex < 2);
    }

    End2D(packet);
}
