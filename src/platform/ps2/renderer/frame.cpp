#include "renderer.h"

#include "platform/graphics.h"

namespace
{
// GS registers, written by address and data pairs (A+D)
enum GsRegister : u64
{
    Prim = 0x00,
    Rgbaq = 0x01,
    Uv = 0x03,
    Xyzf2 = 0x04,
    Xyz2 = 0x05,
    Tex0Context2 = 0x07,
    Clamp2 = 0x09,
    Tex1Context2 = 0x15,
    XyOffset1 = 0x18,
    XyOffset2 = 0x19,
    PrModeCont = 0x1A,
    TexA = 0x3B,
    TexFlush = 0x3F,
    Scissor1 = 0x40,
    Scissor2 = 0x41,
    Dimx = 0x44,
    Dthe = 0x45,
    ColClamp = 0x46,
    Test2 = 0x48,
    Fba2 = 0x4B,
    Frame1 = 0x4C,
    Frame2 = 0x4D,
    ZBuf2 = 0x4F,
};

// The frame is cleared in strips this wide
constexpr s32 StripWidth = 32;
// The GIF tag: as many address and data pairs as its loops (the last packet's), PRIM preset to 4, one register: A+D
constexpr u64 GifTagEnd = 0x8000;
constexpr u64 GifTagFlags = 0x8012ull << 45;
constexpr u64 AddressAndData = 0xE;
// FRAME's FBMSK of every bit, ZBUF's ZMSK, the ZBUF's format (Z32, in its 6 bit code), TEXA's TA1 of 0x80, SCISSOR's 1024 by
// 1024, TEST's depth test always passing, PRIM's sprite in context 2 with UV coordinates, RGBAQ's Q of 1.0f
constexpr u64 FrameNoColor = 0xFFFFFFFFull << 32;
constexpr u64 ZBufNoDepth = 1ull << 32;
constexpr u64 ZBufFormat = 0x30000000;
constexpr u64 TexAlpha1 = 0x8000ull << 24;
constexpr u64 Scissor = 0x0400000004000000;
constexpr u64 TestDepthAlways = 0x30000;
constexpr u64 ClearSprite = 0x306;
constexpr u64 QOne = 0x3F800000ull << 32;
// The frame chain's head: the buffer shown 16 bits a pixel (FRAME's PSMCT16), DIMX's dither matrix, the frame drawn as a
// texture (TEX0's PSMCT24 of 1024 by 1024 texels, its colour), PRIM's textured sprite with UV coordinates in context 2, RGBAQ's
// grey of 128 with a Q of 1.0f, UV's half texel down, XYZF2's farthest depth
constexpr u64 ShownFrameFormat = 0x2000000;
constexpr u64 DitherMatrix = 0x6071243571603524;
constexpr u64 DrawnFrameTexture = 0x6A8100000;
constexpr u64 CopySprite = 0x316;
constexpr u64 GreyQOne = QOne | 0x808080;
constexpr u64 HalfTexelDown = 8 << 16;
constexpr u64 FarDepth = 0x7FFFFFull << 32;
// The shared packet's FRAME: alpha (FBMSK's top byte) not written
constexpr u64 FrameNoAlpha = 0xFFull << 56;
// The packets the materials call: a RET tag with 4 quadwords, then FLUSHA and DIRECT of 4 for VIF1
constexpr u32 CalledPacketTag = ReturnTag | 4;
constexpr u32 CalledPacketDirect = VifDirect | 4;
// A GIF tag of three address and data pairs
constexpr u64 ThreeRegisters = 3;

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

struct Writer
{
    u64* at;

    void Write(u64 data, u64 address)
    {
        at[0] = data;
        at[1] = address;
        at += 2;
    }
};

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
    Writer writer{reinterpret_cast<u64*>(packet + 0x10)};
    writer.Write(ThreeRegisters | GifTagEnd | GifTagFlags, AddressAndData);
    u64 frame = u64{width * height * g_DisplayPixelBytes >> 13} | u64{width >> 6} << 16;
    writer.Write(frame | frameMask, Frame1);
    u64 offsetX = u64{(0x800 - (width >> 1)) << 4};
    u64 offsetY = u64{(0x800 - (height >> 1)) << 4};
    writer.Write(offsetX | offsetY << 32, XyOffset1);
    writer.Write(scissor, Scissor1);
}
}

void WriteFrameHead()
{
    u32 width = g_DisplayWidth;
    u32 height = g_DisplayHeight;
    u32 strips = width >> 5;
    u32 pixels = width * height;
    auto* tag = reinterpret_cast<u32*>(g_FrameChain);
    // The GIF tag and its pairs: the registers set up, four for each strip and the dithering's end
    tag[0] = (strips * 4 + 0x14) | CountTag;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = 0;
    Writer packet{reinterpret_cast<u64*>(g_FrameChain + 0x10)};
    packet.Write(u64{strips * 4 + 0x13} | GifTagEnd | GifTagFlags, AddressAndData);
    packet.Write(0, TexFlush);
    packet.Write(1, PrModeCont);
    packet.Write(0, XyOffset2);
    packet.Write(u64{width >> 6} << 16 | ShownFrameFormat, Frame2);
    u64 depth = u64{pixels * (g_DisplayPixelBytes + g_DrawPixelBytes) >> 13} | ZBufFormat;
    packet.Write(depth | ZBufNoDepth, ZBuf2);
    packet.Write(0, Clamp2);
    packet.Write(1, ColClamp);
    packet.Write(1, Dthe);
    packet.Write(DitherMatrix, Dimx);
    packet.Write(TestDepthAlways, Test2);
    packet.Write(0, Fba2);
    packet.Write(TexAlpha1, TexA);
    packet.Write(Scissor, Scissor2);
    u64 texture = u64{pixels * g_DisplayPixelBytes >> 8} | u64{width >> 6} << 14;
    packet.Write(texture | DrawnFrameTexture, Tex0Context2);
    packet.Write(0, Tex1Context2);
    packet.Write(TestDepthAlways, Test2);
    packet.Write(CopySprite, Prim);
    packet.Write(GreyQOne, Rgbaq);
    // The strips copied half a texel down and right
    u64 bottom = u64{height << 4} << 16;
    u64 bottomTexel = u64{(height << 4) + 8} << 16;
    for (u32 strip = 0; strip < strips; strip++)
    {
        u32 left = strip * StripWidth * 16;
        u32 right = left + StripWidth * 16;
        packet.Write(u64{left + 8} | HalfTexelDown, Uv);
        packet.Write(u64{left} | FarDepth, Xyzf2);
        packet.Write(u64{right + 8} | bottomTexel, Uv);
        packet.Write(u64{right} | bottom | FarDepth, Xyzf2);
    }

    packet.Write(0, Dthe);
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
    u64 scissor = u64{g_DisplayWidth - 1} << 16 | Extended(static_cast<s32>(g_DisplayHeight - 1)) << 48;
    WriteCalledPacket(packet, FrameNoAlpha, scissor);
}

void Platform::Graphics::SetUpRenderTarget(RenderTargetDescription* target)
{
    constexpr f32 ScreenMiddle = 2048.0f;
    constexpr f32 Subpixels = 16.0f;
    constexpr f32 HalfSubpixels = 8.0f;
    constexpr f32 DepthRange = 8388607.0f;
    f32 left = ScreenMiddle - static_cast<f32>(target->displayWidth) * 0.5f + static_cast<f32>(target->offsetX);
    f32 top = ScreenMiddle - static_cast<f32>(target->displayHeight) * 0.5f + static_cast<f32>(target->offsetY);
    f32 height = static_cast<f32>(target->height);
    f32 width = static_cast<f32>(target->width);
    Matrix4x4* matrix = &target->clipToScreen;
    InitIdentityMatrix(matrix);
    matrix->m[3][0] = (left + width * 0.5f) * Subpixels;
    target->x = static_cast<s32>(left);
    matrix->m[1][1] = -height * HalfSubpixels;
    target->y = static_cast<s32>(top);
    matrix->m[0][0] = width * HalfSubpixels;
    matrix->m[3][2] = DepthRange;
    matrix->m[2][2] = -DepthRange;
    matrix->m[3][1] = (top + height * 0.5f) * Subpixels;

    u64 scissor = Extended(target->offsetX) | Extended(target->offsetX + target->width - 1) << 16 |
                  Extended(target->offsetY) << 32 | Extended(target->offsetY + target->height - 1) << 48;
    WriteCalledPacket(target->packet, 0, scissor);
}

// FUN_0019f958's packet
void Platform::Graphics::StartFrame(const FrameStart& frame)
{
    RenderBucket& bucket = g_FrameBuckets.buckets[0];
    DmaChain& chain = ChainOf(bucket);
    u8* start = chain.next;
    chain.start = start;
    BeginPacket(bucket);

    s32 strips = Divide(frame.width, 5);
    Writer packet{reinterpret_cast<u64*>(start + 0x10)};
    packet.Write(Zeroed(strips * 2 + 13) | GifTagEnd | GifTagFlags, AddressAndData);
    packet.Write(0, TexFlush);
    packet.Write(0, Dthe);
    packet.Write(1, PrModeCont);
    u64 offsetX = Extended((0x800 - (frame.screenWidth >> 1)) << 4);
    u64 offsetY = Zeroed((0x800 - (frame.screenHeight >> 1)) << 4);
    packet.Write(offsetX | offsetY << 32, XyOffset2);

    s32 pixels = frame.screenWidth * frame.screenHeight;
    u64 frameRegister = Extended(Divide(pixels, 12)) | static_cast<u64>(static_cast<s64>(frame.screenWidth) >> 6) << 16;
    packet.Write(frameRegister | (frame.clearColor ? 0 : FrameNoColor), Frame2);
    u64 depthRegister = Extended(Divide(pixels * 6, 13)) | ZBufFormat;
    packet.Write(depthRegister | (frame.clearDepth ? 0 : ZBufNoDepth), ZBuf2);
    packet.Write(1, ColClamp);
    packet.Write(0, Fba2);
    packet.Write(TexAlpha1, TexA);
    packet.Write(Scissor, Scissor2);
    packet.Write(TestDepthAlways, Test2);
    packet.Write(ClearSprite, Prim);
    packet.Write(frame.red | static_cast<u64>(frame.green) << 8 | static_cast<u64>(frame.blue) << 16 | QOne, Rgbaq);
    for (s32 strip = 0; strip < strips; strip++)
    {
        u64 top = Extended(frame.y << 4) << 16;
        u64 bottom = Extended((frame.y + frame.height) << 4) << 16;
        packet.Write(Zeroed((frame.x + strip * StripWidth) << 4) | top, Xyz2);
        packet.Write(Zeroed((frame.x + (strip + 1) * StripWidth) << 4) | bottom, Xyz2);
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
