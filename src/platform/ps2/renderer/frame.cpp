#include "renderer.h"
#include "gsvalues.h"

#include "platform/graphics.h"

#include <bit>
#include <libgs.h>

namespace
{
// The frame is cleared, and copied into the buffer shown, in strips of 32 pixels
constexpr u32 StripShift = 5;
constexpr s32 StripWidth = 1 << StripShift;
// The registers' values the frame's set-ups write besides gsvalues.h's: FRAME writing none of the colour and DTHE dithering
const u64 FrameNoColour = std::bit_cast<u64>(GS_FRAME{.draw_mask = 0xFFFFFFFF});
const u64 Dither = std::bit_cast<u64>(GS_DTHE{.enable = 1});
// The frame chain's head: the buffer shown 16 bits a pixel (FRAME's PSMCT16), the frame drawn as a texture (TEX0's PSMCT24 of
// 1024 by 1024 texels, its colour) on a textured sprite in grey, XYZF2's farthest depth, UV's half texel down
const u64 ShownFrameFormat = std::bit_cast<u64>(GS_FRAME{.psm = GS_PIXMODE_16});
const u64 DrawnFrameTexture = std::bit_cast<u64>(GS_TEX0{.psm = GS_TEX_24, .tex_width = 10, .tex_height = 10, .tex_cc = 1});
const u64 FarDepth = std::bit_cast<u64>(GS_XYZF{.z = 0x7FFFFF});
// The packets the materials call: a RET tag with 4 quadwords, then FLUSHA and DIRECT of 4 for VIF1 (the GIF tag and its three A+D
// writes)
constexpr u32 CalledPacketWrites = 3;
constexpr u32 CalledPacketTag = ReturnTag | (CalledPacketWrites + 1);
constexpr u32 CalledPacketDirect = VifDirect | (CalledPacketWrites + 1);

// DIMX: the 4 by 4 dither matrix (Sony's default), a value of -4 to 3 in the low three bits of each nibble
constexpr u64 DitherMatrix()
{
    constexpr s32 Matrix[16] = {-4, 2, -3, 3, 0, -2, 1, -1, -3, 3, -4, 2, 1, -1, 0, -2};
    u64 value = 0;
    for (u32 index = 0; index < 16; index++)
    {
        value |= static_cast<u64>(Matrix[index] & 7) << (index * 4);
    }

    return value;
}
static_assert(DitherMatrix() == 0x6071243571603524);

// A value of the asm's 32 bit registers, as its 64 bit ORs see it
u64 Extended(s32 value)
{
    return static_cast<u64>(static_cast<s64>(value));
}

u64 Zeroed(s32 value)
{
    return static_cast<u32>(value);
}

// Divisions of the asm's: by a power of two, toward zero
s32 Divide(s32 value, u32 shift)
{
    return (value < 0 ? value + static_cast<s32>((1u << shift) - 1) : value) >> shift;
}

// A packet the materials call: drawing set up in context 1 for a frame of the display's size (in its drawn buffer, with FRAME's
// mask), clipped to a rectangle
void WriteCalledPacket(u8* packet, u64 frameMask, u64 scissor)
{
    u32 width = g_DisplayWidth;
    u32 height = g_DisplayHeight;
    auto* tag = reinterpret_cast<u32*>(packet);
    tag[0] = CalledPacketTag;
    tag[1] = 0;
    tag[2] = VifFlushA;
    tag[3] = CalledPacketDirect;
    GsWriter writer{reinterpret_cast<GsWrite*>(packet + 0x10)};
    writer.Write(AddressDataTag(CalledPacketWrites, GS_PRIM_TRI_STRIP), GifAddressData);
    u64 frame = u64{width * height * g_DisplayPixelBytes >> GsPageShift} | u64{width >> GsWidthShift} << GsFrameWidthShift;
    writer.Write(frame | frameMask, GsFrame);
    u64 offsetX = u64{(GsScreenMiddle - (width >> 1)) << GsSubpixelShift};
    u64 offsetY = u64{(GsScreenMiddle - (height >> 1)) << GsSubpixelShift};
    writer.Write(offsetX | offsetY << GsOffsetYShift, GsXyOffset);
    writer.Write(scissor, GsScissor);
}
}

void WriteFrameHead()
{
    // The GIF tag and its writes: the 19 settings (the dithering's end among them) and four for each strip
    constexpr u32 SettingWrites = 0x13;
    constexpr u32 StripWrites = 4;
    u32 width = g_DisplayWidth;
    u32 height = g_DisplayHeight;
    u32 strips = width >> StripShift;
    u32 pixels = width * height;
    auto* tag = reinterpret_cast<u32*>(g_FrameChain);
    tag[0] = (strips * StripWrites + SettingWrites + 1) | CountTag;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = 0;
    GsWriter packet{reinterpret_cast<GsWrite*>(g_FrameChain + 0x10)};
    packet.Write(AddressDataTag(strips * StripWrites + SettingWrites, GS_PRIM_TRI_STRIP), GifAddressData);
    packet.Write(0, GsTexFlush);
    packet.Write(AttributesFromPrim, GsPrmodeCont);
    packet.Write(0, SecondContext(GsXyOffset));
    packet.Write(u64{width >> GsWidthShift} << GsFrameWidthShift | ShownFrameFormat, SecondContext(GsFrame));
    u64 depth = u64{pixels * (g_DisplayPixelBytes + g_DrawPixelBytes) >> GsPageShift} | ZbufZ32Format;
    packet.Write(depth | DepthNotWritten, SecondContext(GsZbuf));
    packet.Write(0, SecondContext(GsClamp));
    packet.Write(ClampColours, GsColClamp);
    packet.Write(Dither, GsDither);
    packet.Write(DitherMatrix(), GsDimx);
    packet.Write(DepthAlways, SecondContext(GsTest));
    packet.Write(0, SecondContext(GsFba));
    packet.Write(SecondAlpha, GsTexa);
    packet.Write(WholeScissor, SecondContext(GsScissor));
    u64 texture = u64{pixels * g_DisplayPixelBytes >> GsBlockShift} | u64{width >> GsWidthShift} << GsTextureWidthShift;
    packet.Write(texture | DrawnFrameTexture, SecondContext(GsTex0));
    packet.Write(0, SecondContext(GsTex1));
    packet.Write(DepthAlways, SecondContext(GsTest));
    packet.Write(TexturedSprite, GsPrim);
    packet.Write(GreyQOne, GsRgbaq);
    // The strips copied half a texel down and right
    u64 bottom = u64{height << GsSubpixelShift} << GsXyzYShift;
    u64 bottomTexel = u64{(height << GsSubpixelShift) + GsHalfTexel} << GsUvVShift;
    for (u32 strip = 0; strip < strips; strip++)
    {
        u32 left = strip * StripWidth << GsSubpixelShift;
        u32 right = left + (StripWidth << GsSubpixelShift);
        packet.Write(u64{left + GsHalfTexel} | HalfTexelDown, GsUv);
        packet.Write(u64{left} | FarDepth, GsXyzf2);
        packet.Write(u64{right + GsHalfTexel} | bottomTexel, GsUv);
        packet.Write(u64{right} | bottom | FarDepth, GsXyzf2);
    }

    packet.Write(0, GsDither);
    auto* end = reinterpret_cast<u32*>(packet.at);
    end[0] = EndTag;
    end[3] = 0;
    end[1] = 0;
    end[2] = 0;
}

void MakeSharedGifPacket()
{
    constexpr u32 PacketSize = 0x60;
    auto* packet = static_cast<u8*>(MemoryAllocate2(PacketSize));
    g_SharedGifPacket = Address(packet);
    u64 scissor = u64{g_DisplayWidth - 1} << GsScissorRightShift |
                  Extended(static_cast<s32>(g_DisplayHeight - 1)) << GsScissorBottomShift;
    WriteCalledPacket(packet, FrameColourOnly, scissor);
}

void Platform::Graphics::SetUpRenderTarget(RenderTargetDescription* target)
{
    f32 left = GsScreenMiddleFloat - static_cast<f32>(target->displayWidth) * 0.5f + static_cast<f32>(target->offsetX);
    f32 top = GsScreenMiddleFloat - static_cast<f32>(target->displayHeight) * 0.5f + static_cast<f32>(target->offsetY);
    f32 height = static_cast<f32>(target->height);
    f32 width = static_cast<f32>(target->width);
    Matrix4x4* matrix = &target->clipToScreen;
    InitIdentityMatrix(matrix);
    matrix->m[3][0] = (left + width * 0.5f) * GsSubpixels;
    target->x = static_cast<s32>(left);
    matrix->m[1][1] = -height * GsHalfSubpixels;
    target->y = static_cast<s32>(top);
    matrix->m[0][0] = width * GsHalfSubpixels;
    matrix->m[3][2] = GsDepthRange;
    matrix->m[2][2] = -GsDepthRange;
    matrix->m[3][1] = (top + height * 0.5f) * GsSubpixels;

    u64 scissor = Extended(target->offsetX) | Extended(target->offsetX + target->width - 1) << GsScissorRightShift |
                  Extended(target->offsetY) << GsScissorTopShift |
                  Extended(target->offsetY + target->height - 1) << GsScissorBottomShift;
    WriteCalledPacket(target->packet, 0, scissor);
}

// FUN_0019f958's packet
void Platform::Graphics::StartFrame(const FrameStart& frame)
{
    // The GIF tag and its writes: 13 settings and two for each strip
    constexpr s32 SettingWrites = 13;
    constexpr s32 StripWrites = 2;
    RenderBucket& bucket = g_FrameBuckets.buckets[BucketSky];
    DmaChain& chain = ChainOf(bucket);
    u8* start = chain.next;
    chain.start = start;
    BeginPacket(bucket);

    s32 strips = Divide(frame.width, StripShift);
    GsWriter packet{reinterpret_cast<GsWrite*>(start + 0x10)};
    packet.Write(AddressDataTag(static_cast<u32>(strips * StripWrites + SettingWrites), GS_PRIM_TRI_STRIP), GifAddressData);
    packet.Write(0, GsTexFlush);
    packet.Write(0, GsDither);
    packet.Write(AttributesFromPrim, GsPrmodeCont);
    u64 offsetX = Extended((GsScreenMiddle - (frame.screenWidth >> 1)) << GsSubpixelShift);
    u64 offsetY = Zeroed((GsScreenMiddle - (frame.screenHeight >> 1)) << GsSubpixelShift);
    packet.Write(offsetX | offsetY << GsOffsetYShift, SecondContext(GsXyOffset));

    s32 pixels = frame.screenWidth * frame.screenHeight;
    u64 frameRegister = Extended(Divide(pixels, FrameBufferPagesShift)) |
                        static_cast<u64>(static_cast<s64>(frame.screenWidth) >> GsWidthShift) << GsFrameWidthShift;
    packet.Write(frameRegister | (frame.clearColor ? 0 : FrameNoColour), SecondContext(GsFrame));
    u64 depthRegister = Extended(Divide(pixels * DepthBufferPixelOffset, GsPageShift)) | ZbufZ32Format;
    packet.Write(depthRegister | (frame.clearDepth ? 0 : DepthNotWritten), SecondContext(GsZbuf));
    packet.Write(ClampColours, GsColClamp);
    packet.Write(0, SecondContext(GsFba));
    packet.Write(SecondAlpha, GsTexa);
    packet.Write(WholeScissor, SecondContext(GsScissor));
    packet.Write(DepthAlways, SecondContext(GsTest));
    packet.Write(FlatSprite, GsPrim);
    packet.Write(std::bit_cast<u64>(GS_RGBAQ{.r = frame.red, .g = frame.green, .b = frame.blue, .q = 1.0f}), GsRgbaq);
    for (s32 strip = 0; strip < strips; strip++)
    {
        u64 top = Extended(frame.y << GsSubpixelShift) << GsXyzYShift;
        u64 bottom = Extended((frame.y + frame.height) << GsSubpixelShift) << GsXyzYShift;
        packet.Write(Zeroed((frame.x + strip * StripWidth) << GsSubpixelShift) | top, GsXyz2);
        packet.Write(Zeroed((frame.x + (strip + 1) * StripWidth) << GsSubpixelShift) | bottom, GsXyz2);
    }

    // The tag sends the packet to the GIF, and a "next" tag goes after it
    auto* end = reinterpret_cast<u8*>(packet.at);
    auto* tag = reinterpret_cast<u32*>(start);
    u32 quadwords = static_cast<u32>((end - start) >> 4) - 1;
    tag[0] = quadwords | CountTag;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = quadwords | VifDirect;
    EndPacket(bucket, end);
}
