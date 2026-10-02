#include "renderer.h"

#include "platform/graphics.h"

namespace
{
// GS registers, written by address and data pairs (A+D)
enum GsRegister : u64
{
    Prim = 0x00,
    Rgbaq = 0x01,
    Xyz2 = 0x05,
    XyOffset2 = 0x19,
    PrModeCont = 0x1A,
    TexA = 0x3B,
    TexFlush = 0x3F,
    Scissor2 = 0x41,
    Dthe = 0x45,
    ColClamp = 0x46,
    Test2 = 0x48,
    Fba2 = 0x4B,
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
