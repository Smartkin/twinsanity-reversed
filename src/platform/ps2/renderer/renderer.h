#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/chunkdata.h"
#include "game/controllers.h"
#include "game/decals.h"
#include "game/math.h"
#include "game/place.h"
#include "game/renderer.h"
#include "game/graphicstables.h"
#include "game/view.h"

#include <cstddef>
#include <cstdint>

// The PS2 renderer's DMA: the channels it sends on, the chains of DMA packets the render buckets are written into, and the
// buckets. A chain has two buffers a frame apart (one is sent while the other is filled) and two small ones movies use, whose
// buffers' memory is the movie decoder's then. A bucket is a writer's place in its chain: each packet ends with a "next" tag the
// one after fills the address of, and linking the frame's buckets fills each one's last with the next one's first

// DMA tags (their ID in the first word: CNT sends the quadwords after it, NEXT goes on at its address, REF sends the ones at its
// address, REFS the same stalling on the stall address, CALL goes there until a RET, END stops) and VIF1's codes (in a tag's
// second half, or in what a CNT sends): STCYCL of 1 and 1 and of 4 and 1 (one quadword written of every four), OFFSET, BASE,
// MARK, FLUSHE, FLUSH, FLUSHA, MSCAL, MSCNT, MPG, DIRECT, and UNPACK of one S-32, V2-32, V3-32 or V4-32 element
// (VifUnpackV2Count, VifUnpackV4Count and VifUnpackV4Bytes, of V4-8 elements, take their count, NUM, at VifCountShift), to an
// address from VU1's double buffer's place (TOPS) with VifTops (FLG). The game ORs the counts and addresses into them unmasked (a
// value too wide for its field runs into the next), so they're built with ORs rather than fields
constexpr u32 CountTag = 0x10000000;
constexpr u32 NextTag = 0x20000000;
constexpr u32 ReferenceTag = 0x30000000;
constexpr u32 ReferenceStallTag = 0x40000000;
constexpr u32 CallTag = 0x50000000;
constexpr u32 ReturnTag = 0x60000000;
constexpr u32 EndTag = 0x70000000;
constexpr u32 VifCycle1 = 0x01000101;
constexpr u32 VifCycle4 = 0x01000104;
constexpr u32 VifOffset = 0x02000000;
constexpr u32 VifBase = 0x03000000;
constexpr u32 VifMark = 0x07000000;
constexpr u32 VifFlushE = 0x10000000;
constexpr u32 VifFlush = 0x11000000;
constexpr u32 VifFlushA = 0x13000000;
constexpr u32 VifMscal = 0x14000000;
constexpr u32 VifMscnt = 0x17000000;
constexpr u32 VifMpg = 0x4A000000;
constexpr u32 VifDirect = 0x50000000;
constexpr u32 VifUnpackS = 0x60010000;
constexpr u32 VifUnpackV2 = 0x64010000;
constexpr u32 VifUnpackV2Count = 0x64000000;
constexpr u32 VifUnpackV3 = 0x68010000;
constexpr u32 VifUnpackV4 = 0x6C010000;
constexpr u32 VifUnpackV4Count = 0x6C000000;
constexpr u32 VifUnpackV4Bytes = 0x6E000000;
constexpr u32 VifTops = 0x8000;
constexpr u32 VifCountShift = 16;

// An UNPACK (VifUnpackV2Count's, VifUnpackV4Count's or VifUnpackV4Bytes' kind) of count elements to VU1's address
constexpr u32 VifUnpackTo(u32 unpack, u32 address, u32 count)
{
    return address | count << VifCountShift | unpack;
}

inline u32 Address(const void* pointer)
{
    return reinterpret_cast<u32>(pointer);
}

// A GIF tag's first doubleword: its loops (NLOOP), whether it ends the GIF's packet (EOP), PRIM set to its prim (PRE), its
// format (FLG, GifFormat) and how many registers each loop writes (NREG, 0 for 16). The second doubleword names them, 4 bits
// each (GifDescriptor)
union GifTag
{
    u64 value;
    struct
    {
        u64 loops : 15;
        u64 endOfPacket : 1;
        u64 unused16 : 30;
        u64 setsPrim : 1;
        u64 prim : 11;
        u64 format : 2;
        u64 registerCount : 4;
    };
};
CHECK_SIZE(GifTag, 8);

enum GifFormat : u64
{
    GifPacked = 0,
    GifRegisterList = 1,
    GifImage = 2,
};

// What a GIF tag's loops write: the GS's registers of these numbers, or an address and its data (A+D)
enum GifDescriptor : u64
{
    GifRgbaq = 0x1,
    GifSt = 0x2,
    GifUv = 0x3,
    GifXyz2 = 0x5,
    GifAddressData = 0xE,
};

// A GIF tag's second doubleword: the registers its loops write, the first one's in the low four bits
template <typename... Descriptors>
constexpr u64 GifDescriptors(Descriptors... descriptors)
{
    u64 list = 0;
    u32 shift = 0;
    ((list |= static_cast<u64>(descriptors) << shift, shift += 4), ...);
    return list;
}

// EOP in a GIF tag's first word, also where the VU1 programs take a count of vertexes with it for the GIF tags they make
constexpr u32 GifEndOfPacket = 0x8000;
// A vertex's fourth word for the VU1 programs: the GS's ADC, the vertex draws no triangle
constexpr u32 VertexNoDraw = 0x8000;

// The GIF tag of a PACKED packet's loops of one A+D write each, ending the GIF's packet. The game ORs the count in whole
inline u64 AddressDataTag(u32 loops)
{
    GifTag tag;
    tag.value = loops;
    tag.endOfPacket = 1;
    tag.registerCount = 1;
    return tag.value;
}

// The same setting PRIM to a primitive (PRE) on the way, which the GS keeps until the A+D writes give it another
inline u64 AddressDataTag(u32 loops, u32 prim)
{
    GifTag tag;
    tag.value = AddressDataTag(loops);
    tag.setsPrim = 1;
    tag.prim = prim;
    return tag.value;
}

// The GIF tag of an IMAGE packet of quadwords (a transfer's pixels, after the A+D writes that set it up), ending the GIF's packet
inline u64 ImageDataTag(u32 quadwords)
{
    GifTag tag = {};
    tag.loops = quadwords;
    tag.endOfPacket = 1;
    tag.format = GifImage;
    return tag.value;
}

// The GS's registers (the A+D writes' addresses): the first context's, the second context's are one past them. XYZ3 sets a vertex
// without drawing (no kick)
enum GsRegister : u64
{
    GsPrim = 0x00,
    GsRgbaq = 0x01,
    GsSt = 0x02,
    GsUv = 0x03,
    GsXyzf2 = 0x04,
    GsXyz2 = 0x05,
    GsTex0 = 0x06,
    GsClamp = 0x08,
    GsXyz3 = 0x0D,
    GsTex1 = 0x14,
    GsXyOffset = 0x18,
    GsPrmodeCont = 0x1A,
    GsPrmode = 0x1B,
    GsMipTbp1 = 0x34,
    GsMipTbp2 = 0x36,
    GsTexa = 0x3B,
    GsTexFlush = 0x3F,
    GsScissor = 0x40,
    GsAlpha = 0x42,
    GsDimx = 0x44,
    GsDither = 0x45,
    GsColClamp = 0x46,
    GsTest = 0x47,
    GsFba = 0x4A,
    GsFrame = 0x4C,
    GsZbuf = 0x4E,
    GsBitBltBuf = 0x50,
    GsTrxPos = 0x51,
    GsTrxReg = 0x52,
    GsTrxDir = 0x53,
};

constexpr u64 SecondContext(GsRegister address)
{
    return address + 1;
}

// PRMODECONT: the primitives' attributes come from PRMODE, or from PRIM
constexpr u64 AttributesFromPrmode = 0;
constexpr u64 AttributesFromPrim = 1;

// An A+D write: a GS register's value and its address
struct GsWrite
{
    u64 value;
    u64 address;
};

// A packet's A+D writes, one after another
struct GsWriter
{
    GsWrite* at;

    void Write(u64 value, u64 address)
    {
        at->value = value;
        at->address = address;
        at++;
    }
};

extern "C"
{
    // The frame and depth buffers' pages (in 2048 words)
    extern u32 g_FrameBufferPage RETAIL(D_0030AAFC);
    extern u32 g_DepthBufferPage RETAIL(D_0030AB00);
}

// The depth buffer's format in ZBUF where GS_SET_ZBUF puts it: PSMZ32's 0x30 shifted past PSM's four bits (the GS reads 0 there,
// Z32 too). The game ORs the buffer's page in whole
constexpr u64 ZbufZ32Format = 0x30ull << 24;

// The GS's units: coordinates in 16ths of a pixel (or texel), the screen's middle at 2048 pixels; memory in pages of 2048 words
// (bytes shifted by GsPageShift) and blocks of 64 (GsBlockShift, 32 blocks a page); buffers' widths in 64 pixels
constexpr u32 GsSubpixelShift = 4;
constexpr u32 GsHalfTexel = 8;
constexpr s32 GsScreenMiddle = 0x800;
constexpr u32 GsPageShift = 13;
constexpr u32 GsBlockShift = 8;
constexpr u32 GsPageBlocksShift = 5;
constexpr u32 GsWidthShift = 6;
// The same as floats, for the matrices to the screen and the 2D places: the screen's middle, a pixel's 16ths and half of them,
// and half the depth's range (the matrices to the screen take -1 to 1 to 2^24 - 2 to 0)
constexpr f32 GsScreenMiddleFloat = 2048.0f;
constexpr f32 GsSubpixels = 16.0f;
constexpr f32 GsHalfSubpixels = 8.0f;
constexpr f32 GsDepthRange = 8388607.0f;
// A 32 bit depth in front of everything (the depth tests pass greater or equal)
constexpr u32 GsFrontDepth = 0xFFFFFFFF;

// The GS memory's buffers after the one shown (16 bits a pixel, at 0), by the bytes a pixel of the screen before them: the frame
// drawn (32 bit, at 2: its page is the pixels shifted by FrameBufferPagesShift, its block by FrameBufferBlocksShift), its depth
// (32 bit), and the half-size buffer the sky and the shadows draw into
constexpr s32 DepthBufferPixelOffset = 6;
constexpr s32 HalfSizeBufferPixelOffset = 10;
constexpr u32 FrameBufferPagesShift = 12;
constexpr u32 FrameBufferBlocksShift = 7;

// Where a 256 colour palette's entry goes in the GS's order (CSM1): bits 3 and 4 of its index swapped
inline u32 PaletteSlot(u32 index)
{
    return (index & 0xE7) | (index & 0x8) << 1 | (index & 0x10) >> 1;
}

// The registers' fields the game ORs its values into unmasked (a value too wide runs into the next field), by their first bits:
// FRAME's buffer width, TEX0's buffer width and palette address, XYOFFSET's y, SCISSOR's right, top and bottom, XYZ2's (and
// XYZF2's) y and depth, UV's v
constexpr u32 GsFrameWidthShift = 16;
constexpr u32 GsTextureWidthShift = 14;
constexpr u32 GsPaletteShift = 37;
constexpr u32 GsOffsetYShift = 32;
constexpr u32 GsScissorRightShift = 16;
constexpr u32 GsScissorTopShift = 32;
constexpr u32 GsScissorBottomShift = 48;
constexpr u32 GsXyzYShift = 16;
constexpr u32 GsXyzDepthShift = 32;
constexpr u32 GsUvVShift = 16;
// A sprite's UV half a texel in from the top (its U added)
constexpr u64 HalfTexelDown = u64{GsHalfTexel} << GsUvVShift;

// The DMA controller's channels by number (ten: VIF0, VIF1, the GIF, from and to the IPU, the SIF's three, from and to the
// scratchpad)
enum DmaChannelNumber : s32
{
    Vif0Channel = 0,
    Vif1Channel = 1,
    GifChannel = 2,
    IpuFromChannel = 3,
    IpuToChannel = 4,
    DmaChannels = 10,
};

// A channel's registers by their offset from its CHCR: CHCR, MADR, QWC and TADR
enum DmaRegisterOffset : u32
{
    ChcrOffset = 0x0,
    MadrOffset = 0x10,
    QwcOffset = 0x20,
    TadrOffset = 0x30,
};

// D_CHCR, a DMA channel's control: its direction (from memory), mode (DmaMode), address stack pointer, whether a chain's tags
// are sent along (TTE) and interrupt (TIE), STR (started, cleared when it's done) and the last tag's upper half
union DmaChannelControl
{
    u32 value;
    struct
    {
        u32 fromMemory : 1;
        u32 unused1 : 1;
        u32 mode : 2;
        u32 stackPointer : 2;
        u32 sendsTags : 1;
        u32 tagInterrupts : 1;
        u32 started : 1;
        u32 unused9 : 7;
        u32 tag : 16;
    };
};
CHECK_SIZE(DmaChannelControl, 4);

enum DmaMode : u32
{
    DmaNormalMode = 0,
    DmaChainMode = 1,
    DmaInterleaveMode = 2,
};

// A source chain's DMA tag (its first doubleword): the quadwords (QWC), PCE, the ID (DMA_TAG_REF, ...), IRQ, the address and
// whether it's in the scratchpad (SPR)
union DmaTag
{
    u64 value;
    struct
    {
        u64 quadwords : 16;
        u64 unused16 : 10;
        u64 priorityControl : 2;
        u64 id : 3;
        u64 interrupt : 1;
        u64 address : 31;
        u64 scratchpad : 1;
    };
};
CHECK_SIZE(DmaTag, 8);

// The memory's segments (an address's top 4 bits): the scratchpad's, the physical address below them, the uncached and the
// uncached accelerated segments; and a DMA address's bit of the scratchpad (SPR: the DMA controller takes the scratchpad's
// addresses with it set, main memory's physical ones)
constexpr u32 Scratchpad = 0x70000000;
constexpr u32 PhysicalMask = 0x0FFFFFFF;
constexpr u32 UncachedSegment = 0x20000000;
constexpr u32 UncachedAcceleratedSegment = 0x30000000;
constexpr u32 SegmentShift = 28;
constexpr u32 DmaScratchpad = 0x80000000;

// D_PCR, the DMA controller's priority control: the channels whose ends COP0's condition follows (CPC, a bit each), the channels
// priority control lets run (CDE, a bit each) and priority control on (PCE)
union DmaPriorityControl
{
    u32 value;
    struct
    {
        u32 conditionChannels : 10;
        u32 unused10 : 6;
        u32 enabledChannels : 10;
        u32 unused26 : 5;
        u32 priorityControl : 1;
    };
};
CHECK_SIZE(DmaPriorityControl, 4);

constexpr u32 AllDmaChannels = 0x3FF;

// D_PCR for a wait on channels' ends (their bits ORed in whole): COP0's condition follows them, every channel enabled
inline u32 WaitOnChannels(u32 channels)
{
    DmaPriorityControl control;
    control.value = channels;
    control.enabledChannels = AllDmaChannels;
    return control.value;
}

// The renderer's DMA channels 0-9 (SetDmaRegisterPointers, not the SIF's 5-7): their registers, their bit of D_STAT and whether
// a chain is being sent
struct RendererDmaChannel
{
    volatile u32* registers;
    u32 statusBit;
    u32 sending;
};

// A chain of DMA packets
struct DmaChain
{
    u8* buffers[2];
    u8* movieBuffers[2];
    // Where the next packet goes, where the buffer being filled starts, its size in quadwords and which buffer it is
    u8* next;
    u8* start;
    u32 capacity;
    u32 buffer;
    // 1 for the frame's buckets' chains, 2 and 8 for the two buckets of their own
    u32 kind;
    u32 unused24;
};
CHECK_SIZE(DmaChain, 0x28);

// A writer's place in its chain: its first tag and the last one, which the next packet's address goes into. Its materials take
// turns with VU1's two buffers and load their programs into one of two regions of VU1's micro memory (kept when the bucket starts
// over), and the key of the programs its last material loaded saves loading them again
struct RenderBucket
{
    u32* first;
    // A writer started with two tags takes packets in before its last: here
    u32* insertion;
    u32* last;
    u8 vuBuffer;
    u8 unused0D;
    u8 programRegion;
    u8 unused0F;
    s32 chain;
    u32 unused14;
    u64 lastKey;
    u32 unused20;
    // The material the bucket's last 2D drawing was set up for
    struct Material* last2DMaterial;
    u32 unused28;
    // The packet the bucket's last material called first (MaterialCall): the next one with the same skips its CALL
    u32 lastCall;
    // The joints' matrices the bucket's last skin sent VU1: the next skin with the same doesn't send them again
    u32 lastJoints;
    // The GS address of the ring's slot the bucket's last texture went into, which the next one doesn't take
    u32 lastSlotAddress;
};
CHECK_SIZE(RenderBucket, 0x38);

// The frame's render buckets, drawn in their order (TT Lab's RenderBuckets): the sky's (the frame starts in it), the opaque
// materials', the startup chunk's objects' and the skid marks', the screen effects', the blended materials' (6 to 19), the
// decals', the shadows', the particle pages', the distortion's, the UI's, the fonts' and the movies'. The first 5 are in the
// first chain, the next 16 in the second and the last 7 in the third
enum FrameBucket : u32
{
    BucketSky = 0,
    BucketOpaque = 2,
    BucketGlobalOpaque = 3,
    BucketScreenEffects = 4,
    BucketDecals = 20,
    BucketShadows = 21,
    BucketParticles = 22,
    BucketDistortion = 23,
    BucketUi = 24,
    BucketFonts = 26,
    BucketMovies = 27,
    FrameBucketCount = 28,
};

constexpr u32 SecondChainFirstBucket = 5;
constexpr u32 ThirdChainFirstBucket = 21;

// Buckets after 8 bytes nothing reads: the frame's 28 (drawn in their order) and two of their own
template <u32 Count>
struct RenderBucketSet
{
    u32 unused00[2];
    RenderBucket buckets[Count];
};
using FrameBuckets = RenderBucketSet<FrameBucketCount>;
using SingleBucket = RenderBucketSet<1>;
CHECK_SIZE(FrameBuckets, 0x628);

struct TextureSlot;

// A texture of the game's (an RM2's texture item) as the GS sees it: its size (TEX0's powers of two), how many mip levels it has
// (1 none), its pixel format (the upload's buffer is 64 pixels wide unless it's 32 bit), the format it's uploaded in, TEX0's
// colour component and function, where it and its mips go in its slot (in 64 words) and their buffer widths (in 64 pixels),
// where its palette goes, its transfer chain (a handle of the disk manager's: the GS's transfer registers and the image), the
// slot of GS memory it's in and whether its registers are the second context's. The rest is the tools' header nothing reads (TT
// Lab's texture leftovers: the signature, reserved bytes, the palette's buffer width and the sizes in blocks)
struct Texture
{
    u16 unused00;
    u16 unused02;
    u16 widthPower;
    u16 heightPower;
    u8 levels;
    u8 pixelFormat;
    u8 uploadFormat;
    u8 hasAlpha;
    u8 unused0C;
    u8 function;
    u8 unused0E[2];
    u32 levelOffsets[7];
    u32 levelWidths[7];
    u32 paletteOffset;
    u8 unused4C[0x54 - 0x4C];
    s32 transfer;
    TextureSlot* slot;
    u16 unused5C;
    u16 secondContext;
};
CHECK_OFFSET(Texture, paletteOffset, 0x48);
CHECK_OFFSET(Texture, secondContext, 0x5E);

// A slot of GS memory textures are uploaded to: its address (in 64 words), the buckets the texture was sent in since it came,
// when it was last used (a count of uses) and the texture
struct TextureSlot
{
    u32 gsAddress;
    u32 buckets;
    u32 used;
    Texture* owner;
};
CHECK_SIZE(TextureSlot, 0x10);

struct ShaderAnimation;

// A shader's settings (TT Lab's shader values): alpha blending (PRMODE's ABE) and its preset (AlphaPreset), the alpha test
// (TEST's ATE, ATST, AREF and AFAIL), the destination alpha test (DATE, DATM), the depth test (ZTST), Gouraud shading (IIP),
// texturing (TME, with TEX1 and CLAMP sent), STQ texture coordinates (FST cleared), fog (FGE), the GS's second context, the U and
// V scrolls' modes (ShaderScroll), an alpha formula of its own in place of the preset (A, B, C and D, then FIX), linear filtering
// (MMAG, and MMIN without mips), no FBA, anti-aliasing (AA1, only type 0 sends it), no depth writes (ZMSK), the colour from the
// animation and an animation read with the shader. Bits 23-25 and 57 are read from the files and only ever compared
union ShaderSettings
{
    u64 value;
    struct
    {
        u64 blends : 1;
        u64 preset : 4;
        u64 alphaTest : 1;
        u64 alphaMethod : 3;
        u64 alphaReference : 8;
        u64 alphaFail : 2;
        u64 destinationTest : 1;
        u64 destinationMode : 1;
        u64 depthTest : 2;
        u64 unused23 : 3;
        u64 gouraud : 1;
        u64 textured : 1;
        u64 stq : 1;
        u64 fog : 1;
        u64 secondContext : 1;
        u64 unused31 : 1;
        u64 uScroll : 3;
        u64 vScroll : 3;
        u64 ownAlpha : 1;
        u64 alphaA : 2;
        u64 alphaB : 2;
        u64 alphaC : 2;
        u64 alphaD : 2;
        u64 alphaFix : 8;
        u64 linear : 1;
        u64 noFba : 1;
        u64 unused57 : 1;
        u64 antiAliased : 1;
        u64 noDepthWrites : 1;
        u64 animatedColour : 1;
        u64 hasAnimation : 1;
        u64 unused62 : 2;
    };
};
CHECK_SIZE(ShaderSettings, 8);

// The alpha presets, ALPHA's values InitAlphaPresets makes: (Cs - Cd)·As + Cd, Cs·As + Cd, Cd - Cs·As, Cd·As + Cd, Cd - Cd·As
// and Cd·As (the rest of the table is zeros: the source as it is)
enum AlphaPreset : u64
{
    PresetMix = 0,
    PresetAdd = 1,
    PresetSubtract = 2,
    PresetBrighten = 3,
    PresetDarken = 4,
    PresetScale = 5,
};

// A scroll's modes: none, the animation's offset, a phase going round from 0 to 1, and the sine and cosine of the phase's turn
enum ShaderScroll : u32
{
    ScrollNone = 0,
    ScrollAnimated = 1,
    ScrollWrapped = 2,
    ScrollSine = 3,
    ScrollCosine = 4,
};

struct GameTexture;

// A material's shader (a type of the game's, its vtable 0x6C bytes in): its colour (bytes), its texture (a texture item), its UV
// scroll, and its GS registers (A+D pairs, the packet's GIF tag first) and how many quadwords they are
struct Shader
{
    ShaderSettings settings;
    u8 unused08[4];
    // A font material's first shader: the page of the font it draws
    u8 fontPage;
    u8 colour[4];
    u8 unused11[3];
    GameTexture* texture;
    // TEX1's mip level choice: K and L
    s16 lodK;
    s16 lodL;
    u8 unused1C[4];
    // Bytes the tools left (TT Lab's LeftoverVector), only compared when equal shaders are looked for
    f32 leftover[4];
    f32 shaderColour[4];
    // The U and V scrolls' phases and speeds (a turn a second, ShaderScroll)
    f32 scrollPhases[2];
    f32 scrollSpeeds[2];
    f32 scroll[2];
    u32 registers;
    u32 registerCount;
    // Its texture's ID (0 none) and its type
    u32 textureId;
    u32 type;
    // Its animation (UV scroll and colour tracks), if it has one
    ShaderAnimation* animation;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(Shader, lodL, 0x1A);
CHECK_OFFSET(Shader, leftover, 0x20);
CHECK_OFFSET(Shader, type, 0x64);
CHECK_OFFSET(Shader, animation, 0x68);
CHECK_OFFSET(Shader, registers, 0x58);
CHECK_SIZE(Shader, 0x70);

// The shaders' vtable functions: the destructor, the VU1 program the shader draws with, its frame update (given the frame's
// seconds), the size of its GS settings (in quadwords), their making, its packet, where its GS settings are, whether it needs the
// eye and the model's matrix, whether it makes its model a billboard, its sameness to another shader (the base's comparison) and
// its opposite, its set-up (its type number), its reader, and the type's own sameness and its opposite
enum ShaderSlot : u32
{
    ShaderDestroySlot = 1,
    ShaderProgramSlot = 2,
    ShaderUpdateSlot = 3,
    ShaderRegistersSizeSlot = 4,
    ShaderSettingsSlot = 5,
    ShaderPacketSlot = 6,
    ShaderRegistersSlot = 7,
    ShaderNeedsEyeSlot = 8,
    ShaderBillboardSlot = 9,
    ShaderSameSlot = 11,
    ShaderDifferentSlot = 12,
    ShaderSetUpSlot = 13,
    ShaderReadSlot = 14,
    ShaderTypeSameSlot = 15,
    ShaderTypeDifferentSlot = 16,
};

// The cloth and wave shaders' rows of three wave phases, and the waves VU1 gets (a quadword each, made every frame)
constexpr u32 ShaderWaveRows = 16;

// The cloth shaders' modes: waves that are the sines of their phases, their cosines, and from mode 2 on phases turned in radians,
// which type 0x17 moves toward random places and type 0x1A (mode 2 alone) gives a second set of
enum ClothMode : u32
{
    ClothSines = 0,
    ClothCosines = 1,
    ClothRadians = 2,
};

// The cloth shaders (types 0x17 and 0x1A): their rows of phases (type 0x1A's mode 2 has a second set), their waves, the mode
// (ClothMode), the speed and the amplitude (type 0x1A's one per axis)
struct ClothShader : Shader
{
    Vector4 phases[ShaderWaveRows];
    Vector4 secondPhases[ShaderWaveRows];
    u32 waves;
    u32 mode;
    f32 speed;
    f32 amplitudes[3];
};
CHECK_OFFSET(ClothShader, phases, 0x70);
CHECK_OFFSET(ClothShader, waves, 0x270);
CHECK_OFFSET(ClothShader, amplitudes, 0x27C);

extern "C"
{
    // The shaders' base constructor and a shader's texture set by its ID (none for 0)
    Shader* ShaderConstruct(Shader* shader) RETAIL(FUN_001cd070);
    void SetShaderTexture(Shader* shader, u32 id) RETAIL(FUN_001daaa8);
    // Type 0x1's, 0xE's, 0x12's, 0x13's, 0x18's and 0x1C's set-ups (their vtables' function 13: the type number), and types
    // 0x1's, 0x12's and 0x13's vtables (the skid marks' and the particle pages' shaders)
    void ShaderType01SetUp(Shader* shader) RETAIL(FUN_001d9278);
    extern const GccVTableEntry g_ShaderType01VTable[] RETAIL(PrecompiledShader__Type_0x1_Methods);
    void ShaderType0ESetUp(Shader* shader) RETAIL(FUN_001dc2b8);
    void ShaderType12SetUp(Shader* shader) RETAIL(FUN_001d9eb8);
    void ShaderType13SetUp(Shader* shader) RETAIL(FUN_001d9f70);
    extern const GccVTableEntry g_ShaderType12VTable[] RETAIL(PrecompiledShader__Type_0x12_Methods);
    extern const GccVTableEntry g_ShaderType13VTable[] RETAIL(PrecompiledShader__Type_0x13_Methods);
    void ShaderType18SetUp(struct ScreenCopyShader* shader) RETAIL(FUN_001da1f8);
    void ShaderType1CSetUp(struct WaveShader* shader) RETAIL(FUN_001da3f8);
    // Type 0x1C's frame update: its waves moved on by some seconds
    u32 ShaderType1CUpdate(struct WaveShader* shader, f32 seconds) RETAIL_N32(FUN_001cf450);
    // The colour bytes from the float colour: the red only (what reaches the VU1 programs)
    void SetShaderColourByte(Shader* shader) RETAIL(SetShaderColorByte);
}

// The screen copies (types 0x10, 0x11 and 0x18): a float VU1 gets with the screen's corner
struct ScreenCopyShader : Shader
{
    f32 cornerValue;
};

// Type 0x1C's shader: its rows of wave phases, its waves, their speed and what VU1 gets after the UV scroll (where the cloth
// shaders send their amplitude)
struct WaveShader : Shader
{
    Vector4 phases[ShaderWaveRows];
    u32 waves;
    f32 speed;
    f32 amplitude;
};
CHECK_OFFSET(WaveShader, waves, 0x170);
CHECK_OFFSET(WaveShader, amplitude, 0x178);

// How a model's packets are drawn: unclipped, or clipped by VU1 (its materials also hand VU1 program 11's entry)
enum DrawMode : u32
{
    DrawUnclipped = 1,
    DrawClipped = 2,
};

// The packet a material's drawing calls first: none, the render target's (the scene's set-up) or the shared GIF tag's
enum MaterialCall : u32
{
    CallNone = 0,
    CallRenderTarget = 1,
    CallSharedGifTag = 2,
};

constexpr u32 MaxMaterialShaders = 4;

// A material: its shaders, the key of the VU1 programs they draw with (a bit per shader type), the render bucket it's drawn in
// (FrameBucket), the writer of the frame's draws of it (in its bucket's chain, put into the bucket at the frame's end), the
// packet its drawing calls first (MaterialCall) and whether it's in the frame's list of materials drawn
struct Material
{
    Shader* shaders[MaxMaterialShaders];
    u32 shaderCount;
    u32 unused14;
    u64 activatedShaders;
    u32 bucket;
    u32 unused24;
    RenderBucket writer;
    u32 call;
    u8 listed;
    u8 unused65;
    // Skins drew it straight into its bucket this frame: its writer isn't put there
    u8 drawnDirectly;
};
CHECK_OFFSET(Material, call, 0x60);

struct InstanceBlock;

// A model's place for the block of its instances a frame draws: the block while it's open, whether its materials need the eye
// and the model's matrix (environment maps), and whether it's a billboard (a shader of its turns it to face the camera)
struct InstanceBlockOwner
{
    InstanceBlock* block;
    u8 needsEye;
    u8 billboard;
};

// A block of a frame's instances of one model, which the model's packets call: its owner (pointing back at it while it's open),
// where it is (a RET tag with VIF1's UNPACK of the instances, then the instances), how many instances it has, their kind
// (InstanceBlockKind, nothing reads it), the instances' size and the block's capacity (quadwords)
struct InstanceBlock
{
    InstanceBlockOwner* owner;
    u8* data;
    u16 count;
    u16 kind;
    u16 size;
    u16 capacity;
    u32 unused10;
};
CHECK_SIZE(InstanceBlock, 0x14);

enum InstanceBlockKind : u16
{
    RigidInstances = 5,
    PlacedInstances = 10,
};

// The graphics resources the chunks share (the game's tables', game/graphicstables.h), as the PS2 has them. Each starts with the
// tables' header. Their packets (in the disk manager) are let go of with DeferRelease: two rounds of releases later, as the DMA
// may still read them
constexpr u32 DeferRelease = 1;

// A texture: the size of its data in the file (the GS's header, 0x20 bytes of the tools' memory and the image), and the GS's view
// of it (its image in the disk manager)
struct GameTexture
{
    ResourceHeader header;
    u32 size;
    Texture texture;
};
CHECK_SIZE(GameTexture, 0x6C);

// The GS's view of a shader's texture
inline Texture* TextureOf(const Shader* shader)
{
    return &shader->texture->texture;
}

// A model (a rigid model's submodels, 0x1C bytes): how many submodels, each one's vertex count, packet size, the size of data
// the files have after the packet (read and dropped) and packet (a handle of the disk manager's)
struct RigidModelData
{
    ResourceHeader header;
    u32 subModelCount;
    u32* vertexCounts;
    u32* sizes;
    u32* extraSizes;
    s32* subModels;
};
CHECK_SIZE(RigidModelData, 0x1C);

// A rigid model as an OGI or the scenery (a mesh) draws it (0x20 bytes): its submodels' materials, the model, the block of its
// instances, how many submodels (materials) it has and the model's ID
struct RigidModel
{
    ResourceHeader header;
    MaterialResource** materials;
    RigidModelData* model;
    InstanceBlockOwner instances;
    u32 count;
    u32 modelId;
};
CHECK_OFFSET(RigidModel, count, 0x18);
CHECK_SIZE(RigidModel, 0x20);

// A skin as an OGI draws it (0x24 bytes): its submodels' materials, vertex counts, packets (handles of the disk manager's), how
// many, their packets' sizes, and the vertexes all of them have (added to what the memory held: its constructor doesn't clear it)
struct Skin
{
    ResourceHeader header;
    MaterialResource** materials;
    u32* vertexCounts;
    s32* subModels;
    u32 count;
    u32* sizes;
    u32 vertexTotal;
    u32 unused20;
};
CHECK_OFFSET(Skin, count, 0x14);
CHECK_SIZE(Skin, 0x24);

// A part of a blend skin's submodel (0x28 bytes): each shape's offsets of its vertexes (where they are, their size in quadwords and
// how many, bytes VU1 unpacks), the packet of the part's vertexes, how many shapes, three factors VU1 gets with each shape's
// weight (the first part's go with every part's), a word read with the packet's size that nothing uses, and the packet's size
struct BlendPart
{
    u32* shapeAddresses;
    u32* shapeSizes;
    u32* shapeCounts;
    u32 packet;
    u8 shapeCount;
    f32 shapeFactors[3];
    u32 unused20;
    u32 packetSize;
};
CHECK_OFFSET(BlendPart, shapeFactors, 0x14);
CHECK_SIZE(BlendPart, 0x28);

struct BlendSubModel
{
    BlendPart** parts;
    u32 count;
};

// A blend skin as an OGI draws it (0x18 bytes): its submodels' materials, the submodels, how many, and how many shapes they have
struct BlendSkin
{
    ResourceHeader header;
    MaterialResource** materials;
    BlendSubModel** subModels;
    u32 count;
    u32 shapeCount;
};
CHECK_OFFSET(BlendSkin, count, 0x10);
CHECK_SIZE(BlendSkin, 0x18);

// A level of detail of the scenery's (0x24 bytes): its meshes, how many, and the squares of its distances: the nearest and
// farthest it's drawn at (the farthest -1: no limit) and where each mesh gives way to the next
struct Lod
{
    static constexpr u32 MostSwitches = 3;

    ResourceHeader header;
    RigidModel** meshes;
    u8 count;
    u32 nearest;
    u32 farthest;
    u32 switchDistances[MostSwitches];
};
CHECK_OFFSET(Lod, nearest, 0x10);
CHECK_OFFSET(Lod, switchDistances, 0x18);
CHECK_SIZE(Lod, 0x24);

// What the scenery's and the dynamic scenery's models are drawn through as placed models: a chunk's view (game/scenery.h's
// ChunkView). The renderer reads its culling's last test's matrices to camera space and to the screen, the view's clip vector,
// the view it's drawn in, how (the test's visibility: DrawUnclipped wholly in view, DrawClipped partly), the chunk's matrices
// (the last, ChunkDrawInverse, takes the camera where a world matrix of the chunk applies) and its vtable (its ChunkMatrixSlot
// function hands a world matrix taken into the chunk's space)
struct PlacedObject
{
    enum ChunkMatrix : u32
    {
        ChunkDrawInverse = 3,
    };

    enum Slot : u32
    {
        ChunkMatrixSlot = 5,
    };

    Matrix4x4 toClip;
    Matrix4x4 toCamera;
    Matrix4x4 toScreen;
    Vector4 clip;
    Vector4 origin;
    Vector4 camera;
    s32 distance;
    RenderView* view;
    u32 mode;
    u32 lastVisibility;
    Matrix4x4* matrices;
    const GccVTableEntry* vtable;
};
CHECK_OFFSET(PlacedObject, clip, 0xC0);
CHECK_OFFSET(PlacedObject, view, 0xF4);
CHECK_OFFSET(PlacedObject, matrices, 0x100);
CHECK_OFFSET(PlacedObject, vtable, 0x104);

// A chunk's sky (0x50 bytes): its meshes, and the matrix of the half-size buffer's screen this frame
struct Sky
{
    ResourceHeader header;
    u32 count;
    RigidModel** models;
    Matrix4x4 toScreen;
};
CHECK_OFFSET(Sky, toScreen, 0x10);
CHECK_SIZE(Sky, 0x50);

// A particle system's render table's head (Platform::Graphics::ParticleRenderTableBytes, InitParticleRenderTable): a CNT tag
// sending its next two quadwords to VU1, those (the nearest a particle is drawn at, the system's gravity, a word nothing writes
// and the particles' clock in the first, the hexagons' distortion in the second), and a RET tag sending the rest of the table
// (the texture's or the hexagons' corners and the life's steps), which the packets drawing the system's blocks call
struct ParticleHeader
{
    u32 tag[4];
    f32 nearClip;
    f32 gravity;
    f32 unused18;
    f32 time;
    f32 distortion[4];
    u32 stepsTag[4];
};
CHECK_OFFSET(ParticleHeader, stepsTag, 0x30);

// A model of one material whose packet is drawn after the render target's (the screen models DrawSkidMarks draws): the
// material, the packet, the block of its instances
struct ScreenModel
{
    Material* material;
    u32 packet;
    InstanceBlockOwner instances;
};

// A VU1 program: its code, where it goes in VU1's micro memory (where it was put last, for one loaded when needed) and its size
// (in instructions), the material it was loaded for last (a count of materials, ProgramNotLoaded when it isn't loaded) and where
// it went for each bucket
struct VuProgram
{
    const u64* code;
    u32 address;
    u32 size;
    u32 loaded;
    // Loaded with the first bucket every frame (the registration's first programs), not when a material needs it
    u8 resident;
    u8 unused11;
    u16 bucketAddresses[FrameBucketCount];
    u16 unused4A;
};
CHECK_SIZE(VuProgram, 0x4C);

constexpr u32 ProgramNotLoaded = 0xFFFFFFFF;
constexpr u32 VuProgramCount = 43;
// VU1's program 2 ends at once (its first instruction has the E bit): the entry VU1 goes on to at a packet's end
constexpr u32 EndProgram = 2;
constexpr u32 RendererDmaMemorySize = 0x540000;

extern "C"
{
    extern RendererDmaChannel g_RendererDma[DmaChannels] RETAIL(G_DMA_N_CHANNELS);
    // Ten chains: the label's size (0x78) is splat's, the pad after it is the rest. The allocation checks for eleven
    extern DmaChain g_DmaChains[10] RETAIL(G_DmaChains_Array_);
    extern s32 g_DmaChainCount RETAIL(G_TotalDMA_Chains);
    extern u32 g_DmaQuadwordsAllocated RETAIL(G_DMA_ByteStream_TagsAllocatedAmt);
    // The two regions of the chains' buffers, and where the next movie buffers go (from the first region's start: while they're
    // used, the frames' buffers aren't)
    extern u8* g_DmaFirstBuffers RETAIL(G_DMA_ByteStream_Beg_2);
    extern u8* g_DmaSecondBuffers RETAIL(G_UnkDMA_0x187040_ByteStream_2);
    extern u8* g_DmaMovieNext RETAIL(G_UnkDMA_0x187040_ByteStream_1);
    extern u8 g_DmaReady RETAIL(D_00309B48);
    // The buckets fill the chains' movie buffers
    extern u8 g_MovieBuckets RETAIL(D_00309B4C);
    extern FrameBuckets g_FrameBuckets RETAIL(D_003239D8);

    // The renderer's DMA memory (RendererDmaMemorySize bytes taken at its start) and where its next part goes
    extern u8* g_RendererDmaMemory RETAIL(G_DMA_ByteStream_Beg_);
    extern u8* g_RendererDmaNext RETAIL(G_DMA_ByteStream_CurPosition_);
    // The two buckets of their own (the small one's chain of 100 quadwords, the large one's of 10000)
    extern SingleBucket g_SmallBucket RETAIL(D_003238F0);
    extern SingleBucket g_LargeBucket RETAIL(D_00324000);

    // The two regions of the chains' buffers out of the memory given; returns the memory after them
    u8* CarveDmaMemory(u8* memory) RETAIL(FUN_00181fa0);
    // Whether a DMA channel is sending (its CHCR's STR), and a chain sent on one once it's done with the one before (from the
    // scratchpad when it's there; its tags sent too when asked)
    bool IsDmaChannelBusy(s32 channel) RETAIL(IsVIF0_TransferingFromMemory);
    void StartDmaChain(s32 channel, const void* chain, bool sendTags) RETAIL(TransferDMA_Data);
    // The wait until the chain the renderer sent on a channel is sent (its D_STAT bit, through COP0's condition), and on VIF1's;
    // they return 1
    s32 WaitForDmaChannel(s32 channel) RETAIL(FinishDMATransfer);
    s32 WaitForVif1Dma() RETAIL(FinishDMATransferChannel1);
    // The frame's buckets' three chains (the first 5 buckets', the next 16', the last 7'), and the buckets started
    void InitialiseFrameBuckets(FrameBuckets* buckets) RETAIL(InitDMA_Manager);
    // A bucket of its own with a chain of 100 quadwords, of 10000
    void InitialiseSmallBucket(SingleBucket* bucket) RETAIL(FUN_0017e510);
    void InitialiseLargeBucket(SingleBucket* bucket) RETAIL(FUN_0017e3c8);
    // A writer emptied
    void RenderBucketConstruct(RenderBucket* bucket) RETAIL(FUN_001a0098);
    // A material without shaders (its writer empty, the packet it calls first 1), and destroyed: its shaders deleted
    Material* MaterialConstruct(Material* material) RETAIL(FUN_001c0838);
    void MaterialDestroy(Material* material, u32 flags) RETAIL(FUN_001c0880);
    // A writer of its own starts in the chain of a frame bucket, with one "next" tag or two
    void StartWriter(RenderBucket* writer, u32 bucket) RETAIL(FUN_00182280);
    void StartWriterTwoTags(RenderBucket* writer, u32 bucket) RETAIL(FUN_00182308);
    // Takes count times size bytes (rounded down to quadwords) and a quadword from the chain of the set's first bucket (every
    // caller hands it the frame's buckets); returns where they start
    u8* AllocDmaTags(FrameBuckets* buckets, u32 count, u32 size);
    // The buckets start over in the chains' other buffers, with the vector units' programs queued again
    void ResetRenderBuckets(FrameBuckets* buckets);
    // The buckets are linked into one chain that VIF1's channel is started on (from the vertical blank's interrupt or not), then
    // start over
    void LinkRenderBuckets(FrameBuckets* buckets, bool fromInterrupt);

    // The VU1 programs (the label's size is splat's, the pad after it is the rest), and where the data they share goes in VU1's
    // memory: VIF1's double buffers' base and offset, two registers' values, the entries of programs 9 to 13, the two GIF tag
    // templates
    extern VuProgram g_VuPrograms[VuProgramCount] RETAIL(G_Shader_VU_Programs);
    // The programs loaded when materials need them: a count of the materials that loaded programs (the programs loaded for the
    // current one have it), the bucket of the current material, where the next program goes
    extern u32 g_VuProgramTime RETAIL(D_0030AB70);
    extern u32 g_VuProgramBucket RETAIL(D_0030AB74);
    extern u32 g_VuProgramNext RETAIL(D_0030AB78);
    // The two regions of VU1's micro memory buckets load programs into, and the start of the one the current material loaded its
    // programs in (the shaders' packets go by it)
    extern u32 g_VuProgramRegion1 RETAIL(D_0030AB68);
    extern u32 g_VuProgramRegion2 RETAIL(D_0030AB6C);
    extern u32 g_VuProgramStart RETAIL(G_RegisteredVuInstructionsAmt);
    // VU1's two buffers of a material's data, and where the one it takes next goes
    extern u32 g_VuBuffer1 RETAIL(D_00309CE4);
    extern u32 g_VuBuffer2 RETAIL(D_00309CE8);
    extern u32 g_VuBufferPlace RETAIL(D_00309CB8);
    // The program whose entry (and a place past it) every material hands VU1
    extern u32 g_ParameterProgram RETAIL(G_MicroCode_6_Index);
    extern s16 g_ParameterProgramOffset RETAIL(D_002EC3B2);
    // The packets materials can call: the shared GIF tag's and the render target's
    extern u32 g_SharedGifPacket RETAIL(G_SomeGifTag);
    // What the scene being drawn is drawn into (set with the view)
    extern RenderTargetDescription* g_RenderTarget RETAIL(G_FontRendererRel);
    // What the texture uploads are handed by the materials (unused)
    extern u8 g_TextureUploadContext[0x18] RETAIL(D_0030A820);
    extern u32 g_VuBufferBase RETAIL(D_00309CEC);
    extern u32 g_VuBufferOffset RETAIL(D_00309CF0);
    extern u32 g_VuRegisterAddress RETAIL(D_00309CB0);
    extern u32 g_VuRegisterValue1 RETAIL(D_00309CF4);
    extern u32 g_VuRegisterValue2 RETAIL(D_00309CF8);
    extern u32 g_VuEntriesAddress1 RETAIL(D_00309CBC);
    extern u32 g_VuEntriesAddress2 RETAIL(D_00309CC0);
    extern u32 g_VuGifTemplateAddress1 RETAIL(D_00309CC4);
    extern u32 g_VuGifTemplateAddress2 RETAIL(D_00309CCC);

    // GS memory's texture slots: the ring of them, how many, the next one tried, the uses counted, the large textures' slot
    extern TextureSlot* g_TextureSlots RETAIL(G_TexRelArray);
    extern u32 g_TextureSlotCount RETAIL(G_TexRelArraySize);
    extern u32 g_TextureSlotNext RETAIL(G_TexRelArrayNextIndex);
    extern u32 g_TextureTime RETAIL(D_0030AB88);
    extern TextureSlot g_LargeTextureSlot RETAIL(G_UnkTexRel);

    // The slots made (the renderer's start)
    void MakeTextureSlots(void* unused) RETAIL(FUN_001bc8f0);
    // The texture into GS memory for the bucket: kept when it's in a slot already (sent again when the bucket hasn't had it
    // yet), the large textures' slot (192 pixels wide or more) or the ring's oldest slot otherwise. Writes the upload at packet,
    // returns where it ends (packet when nothing's sent, or no slot was free)
    u8* UploadTexture(void* context, u8* packet, Texture* texture, u32 bucket) RETAIL(FUN_001bc9e0);
    // The upload of the texture into the slot at packet; returns where it ends
    u8* WriteTextureUpload(void* context, u8* packet, Texture* texture, TextureSlot* slot) RETAIL(SetTextureDma_);
    // Every slot let go of (and the uses counted from 0 but when keepTime), one slot, the large textures', and the first slot
    // (its address returned: a buffer of GS memory to draw into)
    void FreeTextureSlots(void* context, u32 keepTime) RETAIL(FUN_001c0f08);
    void FreeTextureSlot(u32 index) RETAIL(FUN_001c0f88);
    void FreeLargeTextureSlot(void* unused) RETAIL(FUN_001c0fb8);
    u32 ClaimFirstTextureSlot() RETAIL(FUN_001c0fe0);

    // The VU1 programs queued into the first bucket, a program into a bucket, and the first bucket's packet of the data the
    // programs share
    void PutVUProgramsIntoDMAPipeline();
    void PutProgramIntoDMAPipeline(u32 program, u32 bucket);
    void QueueSharedProgramData() RETAIL(FUN_001bc650);
    // The programs loaded when needed are forgotten (every frame), one of them, a program loaded for the current material's
    // bucket unless it was for this material (the packet's end returned), and its upload to where it goes for the bucket
    void ForgetLoadedPrograms() RETAIL(FUN_001da718);
    void ForgetLoadedProgram(u32 program) RETAIL(FUN_001da758);
    u8* LoadProgramForMaterial(u8* packet, u32 program) RETAIL(FUN_001da880);
    u8* WriteProgramUpload(u8* packet, u32 program) RETAIL(FUN_001da918);

    // The material into its bucket's packet: the programs its shaders need loaded (unless the bucket's last material had them),
    // a CALL of what its drawing shares, the entries VU1 goes on to, each shader's texture uploaded and its packet. Returns where
    // the packet ends
    u8* RenderMaterial(Material* material, u8* packet, u32 mode);
    // A CALL of a packet the material's drawing shares (MaterialCall), the material's (anything with its call 0x60 bytes in),
    // and the entries VU1 goes on to by mode (the parameter program's)
    u8* WriteSharedCall(u8* packet, u32 call) RETAIL(FUN_001a0dd0);
    u8* WriteMaterialCall(const Material* material, u8* packet) RETAIL(FUN_001c09e0);
    u8* WriteMaterialEntries(u8* packet, u32* counter, u32 mode) RETAIL(FUN_001dcf40);
    // The frame's draws of the material into its bucket, after what they need set up (the programs, its shaders' packets, the
    // CALL); every listed material's (but the ones skins drew directly), the list forgotten (the writers too) and emptied
    void FlushMaterial(Material* material) RETAIL(FUN_001bab38);
    u32 FlushMaterials() RETAIL(FUN_001c0c20);
    void ForgetMaterials() RETAIL(FUN_001c0ba0);
    void ClearRenderedMaterials() RETAIL(InitShadersRenderedAmt);
    // The renderer's start: the VU1 programs registered, ALPHA's presets, the screen effects (their palettes and wave grid made)
    void InitVuPrograms() RETAIL(InitVU_Programs);
    void InitAlphaPresets() RETAIL(FUN_001daa68);
    void InitialiseScreenEffects() RETAIL(FUN_001afd88);
    // Every material's shaders moved on by the clock's last advance, and the particles' wave shader
    void AnimateMaterials(const TimeClock* clock) RETAIL(FUN_001c0c98);
    void UpdateParticleWaves(const TimeClock* clock) RETAIL(FUN_001b9a30);

    // The bytes of a pixel of the buffer shown (2) and of the one the frame is drawn in (4)
    extern u32 g_DisplayPixelBytes RETAIL(D_00309BD0);
    extern u32 g_DrawPixelBytes RETAIL(D_00309BD4);
    // The frame chain's head written (g_FrameChain: the GS set up for the frame in its second context and the frame drawn copied
    // into the buffer shown in strips of 32 pixels, dithered), and the packet the materials can call made (the drawing set up for
    // the whole screen in the first context, alpha not written)
    void WriteFrameHead() RETAIL(InitDefaultGifTags_);
    void MakeSharedGifPacket() RETAIL(FUN_0019bc10);
    // TEXFLUSH and the texture's TEX0 (and MIPTBP1 and 2 for its mips) sent to the GIF at packet, for its slot; returns where it
    // ends. The texture of the material's shader uploaded for the bucket, and its registers
    u8* WriteTextureRegisters(void* context, u8* packet, const Texture* texture) RETAIL(FUN_001bcbf8);
    // The same into VU1's memory at the counter (it counts them), with the GIF tag of them and the registers before them at the
    // tag's place; how many registers that is
    u8* WriteTextureRegistersToVu(void* context, u8* packet, const Texture* texture, u32* counter, u32 tagPlace)
        RETAIL(FUN_001bce30);
    u32 TextureRegisterCount(void* context, const Texture* texture) RETAIL(FUN_001c10f0);
    u8* UploadShaderTexture(const Material* material, u8* packet, u32 shader, u32 bucket) RETAIL(FUN_001c0ab8);
    u8* WriteShaderTextureRegisters(const Material* material, u8* packet, u32 shader) RETAIL(FUN_001c0b00);

    // The 2D drawing's packet in progress: its tag, its material, its GIF tag, whether its vertexes have ST coordinates and how
    // many registers it set (the screen models' builder keeps its packet, material and vertex count in them too)
    extern u32* g_2DTag RETAIL(D_0030AB20);
    extern Material* g_2DMaterial RETAIL(G_ShaderRel_3);
    extern u64* g_2DGifTag RETAIL(D_0030AB28);
    extern u8 g_2DUsesSt RETAIL(D_0030AB2C);
    extern u32 g_2DPairCount RETAIL(D_0030AB30);
    // A REF of the material's first shader's GS registers, sent to the GIF directly
    u8* WriteShaderRegisters(const Material* material, u8* packet, u32 unused) RETAIL(FUN_001c0a08);
    // 2D drawing with the material in its bucket (set up unless the bucket's last 2D drawing had it): returns where its
    // registers go, which the vertexes' functions write and count; its end finishes the packet's tags at end (the GIF tag of a
    // triangle strip)
    u8* Begin2D(Material* material) RETAIL(FUN_001a67f8);
    void End2D(u8* end) RETAIL(FUN_001a6920);
    // A vertex's colour (RGBA bytes), position (XYZ2, XYZ3 that doesn't draw) and texture coordinates
    u8* Write2DColour(u8* packet, u32 colour) RETAIL(FUN_001ab548);
    u8* Write2DVertex(u8* packet, f32 x, f32 y, u32 z, u32 withoutKick) RETAIL(FUN_001ab5b8);
    u8* Write2DTexCoord(u8* packet, f32 s, f32 t) RETAIL(FUN_001ab628);
    // The material listed among the frame's and its writer started, with one tag or two
    void StartMaterialWriter(Material* material) RETAIL(FUN_001c0920);
    void StartMaterialWriterTwoTags(Material* material) RETAIL(FUN_001c0980);
    // The two materials the renderer makes at its start (bucket 2, key 8): one of shader type 0 that nothing uses, and the screen
    // models' default, of shader type 1 (the vertexes' colours, no texture)
    extern Material* g_UnusedDefaultMaterial RETAIL(G_ShaderRel_1);
    extern Material* g_ScreenModelMaterial RETAIL(G_ShaderRel_2);
    void MakeDefaultMaterials() RETAIL(FUN_001a66a0);

    // What the skin being drawn is drawn with (the OGI's drawer sets them): its matrix, its inverse (what takes the camera into
    // its space), its lights' directions (a row each), their colours and the ambient light; the same of the blend skin
    extern const Matrix4x4* g_SkinMatrix RETAIL(G_InstTransformMat);
    extern const Matrix4x4* g_SkinInverse RETAIL(D_0030AB94);
    extern const Matrix4x4* g_SkinLights RETAIL(D_0030AB9C);
    extern const Vector4* g_SkinLightColours RETAIL(D_0030ABA0);
    extern const Vector4* g_SkinAmbient RETAIL(D_0030ABA4);
    extern const Matrix4x4* g_BlendSkinMatrix RETAIL(D_00309D68);
    extern const Matrix4x4* g_BlendSkinInverse RETAIL(D_00309D6C);
    extern const Matrix4x4* g_BlendSkinLights RETAIL(D_00309D70);
    extern const Vector4* g_BlendSkinLightColours RETAIL(D_00309D74);
    extern const Vector4* g_BlendSkinAmbient RETAIL(D_00309D78);
    // VU1's places of a skin's data: the joints' matrices, the skin's data in its buffer, and in that the clipped's (a skin's
    // eye's matrix too), the eye (before the data), a blend skin's inverse
    extern u32 g_VuJoints RETAIL(G_CONST_D);
    extern u32 g_VuSkinData RETAIL(D_00309D2C);
    extern u32 g_VuSkinClip RETAIL(D_00309D1C);
    extern u32 g_VuSkinEye RETAIL(D_00309D08);
    extern u32 g_VuBlendInverse RETAIL(D_00309D24);
    // The blend shapes' two buffers (the shapes take turns) and where VU1 learns them
    extern u32 g_VuBlendBuffer1 RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN);
    extern u32 g_VuBlendBuffer2 RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END);
    extern u32 g_VuBlendBuffersPlace RETAIL(D_00309CC8);
    // The program whose entry (and a place past it) the blend skins' materials hand VU1
    extern u32 g_BlendParameterProgram RETAIL(G_MicroCode_7_Index);
    extern s16 g_BlendParameterProgramOffset RETAIL(D_002EC3A2);
    // The resident programs a blend skin's parts call (6, 8 and 7): a part's vertexes without shapes, blended with the first
    // shape, and with each next shape (shader types 7 to 9 name them too)
    extern u32 g_BlendNoShapesProgram RETAIL(G_MicroCode_8_Index);
    extern u32 g_BlendFirstShapeProgram RETAIL(G_MicroCode_9_Index);
    extern u32 g_BlendNextShapeProgram RETAIL(D_00309E1C);

    // The skin drawn straight into its materials' buckets: the joints' matrices (unless the bucket has them), its data, its
    // material's set-up and each submodel's packet (DrawMode)
    void SetSkinDMA(Skin* skin, const Matrix4x4* joints, u32 jointCount, u32 mode);
    // A skin's data for shaders that need the eye: its matrix to camera space and the eye in its space
    u8* WriteSkinEyeData(const Skin* skin, u8* packet, const Matrix4x4* toCamera, u32 buffer) RETAIL(FUN_001be720);
    // The blend skin drawn like a skin (always in VU1's second buffer), its submodels' shapes blended by their weights
    void SetBlendSkinDMA(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const f32* weights, const s32* shapes,
                         const s32* shapeCount, u32 mode);
    // A blend skin's material set up: RenderMaterial with the blend program's entries, without a turn of VU1's buffers
    u8* RenderBlendSkinMaterial(Material* material, u8* packet, u32* counter, u32 mode) RETAIL(FUN_001bbc08);
    u8* WriteBlendSkinEntries(u8* packet, u32* counter, u32 mode) RETAIL(FUN_001dca48);
    // The submodel's parts: each one's vertexes, with the shapes given blended in
    u8* WriteBlendSubModel(const BlendSubModel* subModel, u8* packet, const f32* weights, const s32* shapes,
                           const s32* shapeCount) RETAIL(FUN_001c1ac0);
    u8* WriteBlendPart(const BlendPart* part, u8* packet, const f32* weights, const s32* shapes, const s32* shapeCount,
                       const f32* factors) RETAIL(CreateSubBlendDMA_Chain_);
    // The renderer's DMA channels set up: every channel's DMA on (D_PCR), the table of their address registers, and the
    // renderer's channels (not the SIF's 5 to 7) their registers, their tags sent too (CHCR's TTE), their bit, not sending
    void SetDmaRegisterPointers();
    // The wait until every channel's chain is sent (WaitForDmaChannel on each): 1
    s32 FinishDMATransferAll();
    // The screen effects the game's flags ask for
    void DrawScreenEffects() RETAIL(FUN_001b9a68);

    // The materials drawn this frame (the label's size is splat's, D_003D6074 is the rest), how many
    extern Material* g_RenderedMaterials[500] RETAIL(G_MaterialShaderArray_);
    extern u32 g_RenderedMaterialCount RETAIL(G_ShadersRendered_);

    // The instance blocks: 800 a frame, in two regions of memory a frame each (the frame's, and where the next block goes)
    extern InstanceBlock* g_InstanceBlocks RETAIL(G_0x320_ArrayOf_0x14_SizeStruct);
    extern u32 g_InstanceBlockCount RETAIL(D_00309BFC);
    extern u8* g_InstanceFirstRegion RETAIL(D_00309BE8);
    extern u8* g_InstanceSecondRegion RETAIL(D_00309BEC);
    extern u8* g_InstanceRegion RETAIL(D_00309BF0);
    extern u8* g_InstanceBlockNext RETAIL(D_00309BF4);
    // Where VU1 gets the instances, and where it's told that
    extern u32 g_VuInstances RETAIL(D_00309CDC);
    extern u32 g_VuInstancesPlace RETAIL(D_00309CB4);

    // The instance blocks' regions out of the memory given and their list made; returns the memory after them
    u8* InitialiseInstanceBlocks(u8* memory) RETAIL(FUN_001a0e38);
    // The open blocks closed (the frame's end), the other region taken
    u32 CloseInstanceBlocks() RETAIL(FUN_0019cdc8);
    // The rigid model's instance (its matrix, the lights) into its block, a new one when it has none open: returns the new
    // block for the model's packets to call, nullptr when it went into the open one (or there was no room)
    u8* SetRigidModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* matrix, u32 mode, const Vector4* lightDirections,
                               const Vector4* lightColours, const Vector4* ambient);
    // The rigid model drawn: its instance, and when that started a block, each submodel's packet with the block into its
    // material's writer (DrawMode)
    void SetRigidModelDma_(RigidModel* model, const Matrix4x4* matrix, const Vector4* lightDirections,
                           const Vector4* lightColours, const Vector4* ambient, u32 mode);
    // A placed model's instance into its block (a new one when it has none open), its matrices the caller's: returns the new
    // block, nullptr when it went into the open one or there was no room. A billboard (the owner's billboard) turns to face the
    // camera first; the eye comes from the object's world matrix (the one given without an object)
    u8* SetPlacedModelRenderDMA(InstanceBlockOwner* owner, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                                const Vector4* clip, const Matrix4x4* world, PlacedObject* object) RETAIL(FUN_0019c410);
    // The placed object's model drawn like a rigid model; a default mesh's (clipped, the material set up straight into its
    // bucket and the submodels put in before its writer's last tag)
    void SetPlacedModelDMA(RigidModel* model, PlacedObject* object, const Matrix4x4* world) RETAIL(FUN_001bef80);
    void SetDefaultMeshDMA(RigidModel* model, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
        RETAIL(FUN_001bf188);
    // The material's set-up straight into its bucket, its writer's draws after it (the first shader's packet and program)
    void StartMaterialDirectly(Material* material) RETAIL(FUN_001bb8e0);
    // A screen model drawn: its instance into its block, and when that started one, the material's writer (started with two
    // tags and put into its bucket at once) takes the CALLs of the render target's packet, the block and the model's packet
    void SetScreenModelDMA(ScreenModel* model, const Matrix4x4* toScreen, u32 mode, const Matrix4x4* toCamera,
                           const Vector4* clip) RETAIL(FUN_001a6a28);
    // The screen models' builder (screenmodels.cpp; the skid marks are its only models): started (no vertexes, the default
    // material), the material set (none: kept), the colour of the next vertexes (RGBA bytes), a vertex (VertexNoDraw in its
    // fourth word when it's flagged) and the model made of them (its packet ended, its instances' block made)
    void StartScreenModel() RETAIL(FUN_001ab680);
    void ScreenModelMaterial(Material* material) RETAIL(FUN_001ab7f0);
    void ScreenModelColour(u32 colour) RETAIL(FUN_001ab6e8);
    void ScreenModelVertex(const Vector4* place, u32 noDraw) RETAIL(FUN_001ab748);
    ScreenModel* FinishScreenModel() RETAIL(FUN_001ab800);

    // A block of particles drawn with the material (its writer's packet): the system's header with its scale (and a hexagon's
    // distortion) sent before them
    void DrawParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale)
        RETAIL_N32(DrawParticleBlock);
    void DrawHexagonParticleBlock(u8* block, ParticleHeader* header, const Matrix4x4* matrix, Material* material, f32 scale,
                                  f32 distortionX, f32 distortionY) RETAIL_N32(DrawHexagonParticleBlock);
    // The frame's decals of each type with the type's material, the lists emptied
    void DrawDecals(DecalData* decals);

    // The view's matrix to the screen made the half-size buffer's (the old one kept), and put back
    void UseHalfSizeScreen(RenderView* view, const RenderTargetDescription* target) RETAIL(FUN_0027b6c0);
    void RestoreViewScreenMatrix(RenderView* view) RETAIL(FUN_0027b730);
    // The shadows' half-size buffer made in their bucket (the frame's alpha and depth shrunk into it) and the drawing set up for
    // the shadows into it; the scenery is drawn into it next
    void StartShadows(void* shadows) RETAIL(FUN_001c97d8);
    // The shadows drawn into the half-size buffer taken away from the screen, a quarter of it (their bucket), the half-size
    // buffer cleared first
    void ApplyShadows(void* shadows) RETAIL(FUN_001ca250);

    // Where a sky model's matrices go in VU1's buffer
    extern u32 g_VuSkyData RETAIL(D_00309D3C);
    // The matrix from camera space to a buffer of half the target's size, the screen's middle at its middle
    void HalfSizeScreenMatrix(const RenderTargetDescription* target, Matrix4x4* matrix) RETAIL(FUN_001a2578);
    // The sky drawn first (instead of clearing the frame): the half-size buffer set up in the first bucket, each model's parts
    // around the camera into it (the sky's shaders take their matrices from the buffer's own place), and the buffer drawn over
    // the screen two pixels for one, with the depth written
    void DrawSky(Sky* sky, RenderView* view) RETAIL(FUN_001ba350);
    void StartSkyBuffer(Sky* sky) RETAIL(FUN_001ba488);
    void DrawSkyModel(RigidModel* model, RenderView* view, const Matrix4x4* toScreen, const Matrix4x4* toCamera)
        RETAIL(FUN_001bf3a0);
    void FinishSky(Sky* sky) RETAIL(FUN_001ba6c0);
    // A sky model's matrices into VU1's buffer
    u8* WriteSkyModelMatrices(const RigidModel* model, u8* packet, const Matrix4x4* toScreen, const Matrix4x4* toCamera,
                              u32 buffer) RETAIL(FUN_001c1f90);
    // A sky material's set-up: RenderMaterial without the CALL and the parameter entries (the bucket's next material makes its
    // own CALL)
    u8* RenderSkyMaterial(Material* material, u8* packet, u32 mode) RETAIL(FUN_001bb5e8);
}

inline DmaChain& ChainOf(const RenderBucket& bucket)
{
    return g_DmaChains[bucket.chain];
}

// A packet goes at the end of the writer's chain, which its last tag is pointed at
inline u32* BeginPacket(RenderBucket& writer)
{
    auto* at = reinterpret_cast<u32*>(ChainOf(writer).next);
    writer.last[1] = Address(at);
    return at;
}

// A packet put in before the writer's last tag (writers started with two), where the one before it points
inline u32* BeginInsertedPacket(RenderBucket& writer)
{
    auto* at = reinterpret_cast<u32*>(ChainOf(writer).next);
    writer.insertion[1] = Address(at);
    return at;
}

// The inserted packet ends at end: a "next" tag after it, to the last tag, takes the next one
inline void EndInsertedPacket(RenderBucket& writer, u8* end)
{
    DmaChain& chain = ChainOf(writer);
    chain.next = end;
    auto* next = reinterpret_cast<u32*>(chain.next);
    writer.insertion = next;
    next[0] = NextTag;
    next[1] = Address(writer.last);
    next[2] = 0;
    next[3] = 0;
    chain.next += 0x10;
}

// The packet ends at end: a "next" tag after it becomes the writer's last
inline void EndPacket(RenderBucket& writer, u8* end)
{
    DmaChain& chain = ChainOf(writer);
    chain.next = end;
    auto* next = reinterpret_cast<u32*>(chain.next);
    writer.last = next;
    next[0] = NextTag;
    next[1] = 0;
    next[2] = 0;
    next[3] = 0;
    chain.next += 0x10;
}

// The material in the frame's list (once): its draws go into its bucket at the frame's end
inline void ListMaterial(Material* material)
{
    if (material->listed == 0)
    {
        material->listed = 1;
        g_RenderedMaterials[g_RenderedMaterialCount++] = material;
    }
}

// The view's camera's place as the retail code reads it, also when the view has no camera (the word at address 8 then)
inline ObjectPlace* CameraPlace(const RenderView* view)
{
    Reference* reference = view->cameraObject;
    ReferencedObject* camera = reference != nullptr ? reference->object : nullptr;
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(camera) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// Where the view's camera is (its place's matrix made again first)
inline Vector4 CameraPosition(const RenderView* view)
{
    ObjectPlace* place = CameraPlace(view);
    RotateAndTranslate(place);
    return *reinterpret_cast<const Vector4*>(place->matrix.m[3]);
}

// A packet of up to 256 quadwords for VU0's memory (the culling's views): how many it has (a word after the count is written 0
// with it) and how many it can take
struct BigVu0Packet
{
    static constexpr s32 Capacity = 0x100;

    s32 count;
    s32 unused04;
    s32 capacity;
    s32 unused0C;
    u32 data[Capacity][4];
};

// Four planes as VU0 tests them: their normals' absolute values and their normals a row per axis, then their offsets
struct PlaneColumns
{
    f32 absoluteNormals[3][4];
    f32 normals[3][4];
    f32 offsets[4];
};
CHECK_SIZE(PlaneColumns, 0x70);

// A frustum's six planes: the near one, the four sides, the far one
enum FrustumPlane : u32
{
    NearPlane = 0,
    FirstSidePlane = 1,
    SidePlaneCount = 4,
    FarPlane = 5,
    FrustumPlaneCount = 6,
};

extern "C"
{
    // The camera's frustum's planes in its space, and the same with the sides five times as far out
    extern Vector4 g_ViewPlanes[FrustumPlaneCount] RETAIL(D_003B4900);
    extern Vector4 g_FarViewPlanes[FrustumPlaneCount] RETAIL(D_003B4960);
    extern u8 g_Vu0Programs[] RETAIL(G_UnkDmaRelated);
    // Quadwords copied into VU0's memory at an address by the VU0 programs' copier (vu0programs.cpp), and a set of programs loaded
    // (waited for when asked)
    void SendToVu0(u8* programs, const void* source, s32 quadwords, s32 address) RETAIL(FUN_002b2178);
    void SelectVu0Programs(u8* programs, u32 set, bool wait) RETAIL(FUN_002b20e0);
    BigVu0Packet* StartBigVu0Packet(BigVu0Packet* packet) RETAIL(FUN_001f3fa0);
    // A plane put in a column, the side planes of six (the second to the fifth) in the four, columns added to a packet, cleared
    void SetPlaneColumn(PlaneColumns* columns, const Vector4* plane, s32 column) RETAIL(FUN_001fdef0);
    void SetSidePlaneColumns(PlaneColumns* columns, const Vector4* planes) RETAIL(FUN_00201298);
    void AddPlaneColumns(const PlaneColumns* columns, BigVu0Packet* packet) RETAIL(FUN_00201300);
    void ClearPlaneColumns(PlaneColumns* columns) RETAIL(FUN_00201340);
    // A frustum's six planes through its eye and near corners, and the camera's from its field of view
    void FrustumPlanes(f32 depth, Vector4* planes, const Vector4* eye, const Vector4* topRight, const Vector4* bottomRight,
                       const Vector4* bottomLeft, const Vector4* topLeft) RETAIL_N32(FUN_00201480);
    void ViewFrustumPlanes(f32 near, f32 far, f32 aspect, f32 scale, Vector4* planes, const s32* fieldOfView)
        RETAIL_N32(FUN_00201368);
    // The view's planes taken into a space (the near and far ones apart), the far set's
    void ViewPlanesIn(const Matrix4x4* matrix, PlaneColumns* columns, Vector4* nearAndFar) RETAIL(FUN_001ef4a0);
    void FarViewPlanesIn(const Matrix4x4* matrix, PlaneColumns* columns) RETAIL(FUN_001ef550);
    // The far set's side planes into a space as rows and the near plane, for the decals' view
    u32* WriteChunkViewRows(u32* nearPlane, const Matrix4x4* matrix, u32* rows) RETAIL(FUN_001ef370);
    // The view's packet made (the matrices for the particles) and sent into VU0's memory at the index's place, which it returns
    // and keeps in the word 0x100 bytes past the matrices
    s32 UploadParticleView(Matrix4x4* matrices, s32 index) RETAIL(FUN_001e91f8);
    // The clipping of a box's corners loaded in VU0's registers
    void ClipCorners(u32* flags) RETAIL(FUN_00201d60);
    // A level of detail's mesh for a squared distance
    RigidModel* LodMeshAt(Lod* lod, u32 distance) RETAIL(FUN_001c21a0);
}
