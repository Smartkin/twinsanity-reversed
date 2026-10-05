#include "movie.h"

#include "../renderer/renderer.h"
#include "../renderer/gsvalues.h"

#include "platform/io.h"
#include "retail/libc.h"

#include <bit>
#include <kernel.h>
#include <libcdvd.h>
#include <libgs.h>
#include <sifdma.h>

extern "C"
{
    // libsdr's calls of the sound processor's driver (sdr.cpp) and libgraph's vertical blank callback (graphics.cpp)
    s32 sceSdRemote(s32 wait, s32 command, ...);
    void* sceGsSyncVCallback(s32 (*handler)(s32 cause));
}

namespace
{
// The disc stream's buffer: 0x50 sectors in 5 banks, 64 byte aligned, and the sound's ring after it in the memory the game lends
constexpr u32 SectorSize = 0x800;
constexpr u32 SectorShift = 11;
constexpr u32 StreamSectors = 0x50;
constexpr u32 StreamBanks = 5;
constexpr u32 DiscBufferAlignment = 0x40;
constexpr u32 DiscBufferSize = StreamSectors * SectorSize + DiscBufferAlignment;
constexpr u32 SoundRingSize = 0x6000;
// No file streamed: no location on the disc
constexpr u32 NoFileLocation = 0xFFFFFFFF;
// libcdvd's modes of its waits: wait, or return the state
constexpr s32 CdWait = 0;
constexpr s32 CdPoll = 1;

// libsdr's calls wait for the driver's result. Its commands and the SPU2's: the block transfer of core 0 (its start, stop and where
// it plays), its input's volume (at most 0x7FFF)
constexpr s32 SdWait = 1;
constexpr s32 SdCore0 = 0;
constexpr s32 SdBlockTransfer = 0x80E0;
constexpr s32 SdBlockTransferStatus = 0x8100;
constexpr s32 SdSetParameter = 0x8010;
constexpr s32 SdBlockLoopWrite = 0x13;
constexpr s32 SdBlockStop = 2;
constexpr s32 SdInputVolumeLeft = 0xF80;
constexpr s32 SdInputVolumeRight = 0x1080;
constexpr f32 SdMaximumVolume = 32767.0f;
// The sound goes to the ring a KB at a time
constexpr s32 SoundBlockBytes = 0x400;

// The block transfer's status: where in the sound processor's memory it plays, and the half of the buffer
union BlockTransferStatus
{
    u32 value;
    struct
    {
        u32 address : 24;
        u32 half : 8;
    };
};
CHECK_SIZE(BlockTransferStatus, 4);

// The GS's buffers' widths are in 64 pixels
constexpr u32 BufferWidthUnit = 1u << GsWidthShift;

// The picture's packet: a DMA tag, the GIF tag and its A+D writes, 9 of the settings and 4 a strip (its corners' UV and XYZ2). The
// GS's coordinates are in sixteenths of a pixel (half a texel in from the edges), its depth in front of everything
constexpr u32 SettingWrites = 9;
constexpr u32 StripWrites = 4;
constexpr u32 StripWidth = 32 << GsSubpixelShift;
const QWORD SpritesTag =
    std::bit_cast<QWORD>(GS_GIF_TAG{.eop = 1, .pre = 1, .prim = GS_PRIM_SPRITE, .flg = GS_GIF_PACKED, .nreg = 1, .reg = gif_rd_ad});
// The settings: the primitives' attributes from PRIM; the frame 512 pixels wide with its alpha kept (its page ORed in); the depth
// buffer of 32 bits (Z32 where GS_SET_ZBUF puts the format, past PSM's 4 bits: the GS reads 0) not written (its page ORed in); a
// texture of 1024 by 1024 with its alpha, as it is (its address and width ORed in); depth greater or equal. gsvalues.h's
// textured sprite (UV in texels) in white draws it, the texture's colours as they are
const u64 MovieAttributes = std::bit_cast<u64>(GS_PRMODECONT{.control = 1});
const u64 MovieFrame = std::bit_cast<u64>(GS_FRAME{.fb_width = 8, .draw_mask = 0xFF000000});
const u64 MovieDepth = DepthNotWritten | ZbufZ32Format;
const u64 MovieTexture = std::bit_cast<u64>(
    GS_TEX0{.tex_width = 10, .tex_height = 10, .tex_cc = 1, .tex_funtion = GS_TEX_DECAL, .clut_loadmode = 1});
const u64 MovieTest = std::bit_cast<u64>(GS_TEST{.ztest_enable = 1, .ztest_method = GS_ZBUFF_GEQUAL});

void (*g_Present)();
// The vertical blank's handler presented since WaitFrame started waiting, every second blank while it waits
volatile u8 g_Presented;
volatile u8 g_BlankToggle;
volatile u8 g_Waiting;

// The decoder's libraries are set up with the first movie
bool g_DecodingUnready = true;

// The movie file on the disc, and the stream's position in it
u32 g_FileLocation = NoFileLocation;
u32 g_FileSize;
u32 g_ReadPosition;
sceCdRMode g_ReadMode = {0, 0, 0, 0};

// The sound's ring in the I/O processor's memory: where the next bytes go, how many went in before the sound started, whether it's
// the game's memory
MovieAudioStream g_SoundMode;
u32 g_SoundStarted;
s32 g_RingWrite;
s32 g_RingFilled;
bool g_RingLent;
u8* g_Ring;
s32 g_RingSize;

s32 OnVerticalBlank(s32)
{
    if (g_Waiting != 0 && g_BlankToggle != 0)
    {
        g_Present();
        g_Presented = 1;
        g_BlankToggle = 0;
    }
    else
    {
        g_BlankToggle = 1;
    }

    ExitHandler();
    return 1;
}

// Sends size bytes to the I/O processor's memory (SIF DMA), waiting for it when told to
void SendToIop(u8* to, u8* from, s32 size, bool wait)
{
    if (size <= 0)
    {
        return;
    }

    SifDmaTransfer_t transfer = {from, to, size, 0};
    s32 id;
    do
    {
        id = sceSifSetDma(&transfer, 1);
    } while (id == 0);

    while (wait && sceSifDmaStat(id) >= 0)
    {
    }
}

// Copies a ring's part (two pieces when it wraps) into another's room (two pieces as well), as much as fits. Returns the bytes
s32 CopyToIop(u8* to, s32 toSize, u8* toWrapped, s32 toWrappedSize, u8* from, s32 fromSize, u8* fromWrapped, s32 fromWrappedSize)
{
    s32 room = toSize + toWrappedSize;
    s32 total = fromSize + fromWrappedSize;
    if (room < total)
    {
        s32 excess = total - room;
        if (excess < fromWrappedSize)
        {
            fromWrappedSize -= excess;
        }
        else
        {
            fromSize -= excess - fromWrappedSize;
            fromWrappedSize = 0;
        }
    }

    if (fromSize >= toSize)
    {
        SendToIop(to, from, toSize, false);
        SendToIop(toWrapped, from + toSize, fromSize - toSize, false);
        SendToIop(toWrapped + fromSize - toSize, fromWrapped, fromWrappedSize, false);
    }
    else if (fromWrappedSize >= toSize - fromSize)
    {
        SendToIop(to, from, fromSize, false);
        SendToIop(to + fromSize, fromWrapped, toSize - fromSize, false);
        SendToIop(toWrapped, fromWrapped + toSize - fromSize, fromWrappedSize - (toSize - fromSize), false);
    }
    else
    {
        SendToIop(to, from, fromSize, false);
        SendToIop(to + fromSize, fromWrapped, fromWrappedSize, false);
    }

    return fromSize + fromWrappedSize;
}

// The decoder's sound goes to the ring the sound processor plays, a KB at a time: up to where it plays once it started, else until
// the ring is full. Returns the bytes taken
u32 FeedSound(u8* ring, u32 size, u32 read, u32 count, u32)
{
    if (g_SoundMode != MoviePcm)
    {
        return 0;
    }

    u8* to;
    s32 toSize;
    u8* toWrapped = nullptr;
    s32 toWrappedSize = 0;
    if (g_SoundStarted != 0)
    {
        BlockTransferStatus status = {static_cast<u32>(sceSdRemote(SdWait, SdBlockTransferStatus, 0))};
        u32 playing = status.address - reinterpret_cast<u32>(g_Ring);
        s32 room = static_cast<s32>(playing + g_RingSize - g_RingWrite - SoundBlockBytes) % g_RingSize / SoundBlockBytes *
                   SoundBlockBytes;
        toSize = g_RingSize - g_RingWrite;
        to = g_Ring + g_RingWrite;
        if (toSize < room)
        {
            toWrapped = g_Ring;
            toWrappedSize = room - toSize;
        }
        else
        {
            toSize = room;
        }
    }
    else
    {
        toSize = (g_RingSize - g_RingFilled) / SoundBlockBytes * SoundBlockBytes;
        to = g_Ring + g_RingFilled;
    }

    u32 whole = count / SoundBlockBytes * SoundBlockBytes;
    s32 fromSize;
    s32 fromWrappedSize;
    if (size < read + whole)
    {
        fromSize = size - read;
        fromWrappedSize = whole - fromSize;
    }
    else
    {
        fromSize = whole;
        fromWrappedSize = 0;
    }

    s32 sent = CopyToIop(to, toSize, toWrapped, toWrappedSize, ring + read, fromSize, ring, fromWrappedSize);
    g_RingWrite = (g_RingWrite + sent) % g_RingSize;
    g_RingFilled += sent;
    return sent;
}

// The sound processor's input volume (0 to 1)
void SetSoundVolume(f32 volume)
{
    if (g_SoundMode != MoviePcm)
    {
        return;
    }

    s32 value = static_cast<s32>(volume * SdMaximumVolume);
    sceSdRemote(SdWait, SdSetParameter, SdInputVolumeLeft, value);
    sceSdRemote(SdWait, SdSetParameter, SdInputVolumeRight, value);
}

// The decoder's file: the disc stream from the file's start
s32 SeekStream(const char*)
{
    s32 result = sceCdStSeek(g_FileLocation);
    g_ReadPosition = 0;
    return result;
}

// Reads the whole sectors the stream has (without waiting for more), again after a read the drive had trouble with. Returns the
// bytes, -1 past the file's end
s32 ReadStream(u8* buffer, u32 size, const char*)
{
    u32 position = g_ReadPosition;
    if (position + size >= g_FileSize && position >= g_FileSize)
    {
        return -1;
    }

    u32 sectors = static_cast<s32>(size) / static_cast<s32>(SectorSize);
    u32 error = 0;
    s32 read = sceCdStRead(sectors, reinterpret_cast<u32*>(buffer), STMNBLK, &error) << SectorShift;
    while (error != 0 || sceCdSync(CdWait) != 0 || sceCdGetError() != 0)
    {
        while (sceCdDiskReady(CdWait) != SCECdComplete)
        {
        }

        read = sceCdStRead(sectors, reinterpret_cast<u32*>(buffer), STMNBLK, &error) << SectorShift;
    }

    g_ReadPosition += read;
    return read;
}

void WaitUntilDiscReady()
{
    while (sceCdDiskReady(CdPoll) != SCECdComplete)
    {
    }
}

// Starts streaming the file, through the memory the game lent or a buffer of its own in the I/O processor's memory. Nothing when
// there's no such file
void OpenStream(Platform::Movie::Player* player, const char* file, void* lentMemory)
{
    while (sceCdBreak() == 0)
    {
    }

    WaitUntilDiscReady();
    sceCdlFILE found;
    if (sceCdSearchFile(&found, file) == 0)
    {
        return;
    }

    g_FileSize = found.size;
    g_FileLocation = found.lsn;
    player->discBuffer = lentMemory;
    player->discBufferLent = 1;
    if (player->discBuffer == nullptr)
    {
        player->discBuffer = Platform::Io::AllocateHeap(DiscBufferSize);
        player->discBufferLent = 0;
    }

    sceCdStInit(StreamSectors, StreamBanks,
                reinterpret_cast<void*>((reinterpret_cast<u32>(player->discBuffer) + DiscBufferAlignment - 1) &
                                        ~(DiscBufferAlignment - 1)));
    WaitUntilDiscReady();
    sceCdStStart(g_FileLocation, &g_ReadMode);
}

void CloseStream(Platform::Movie::Player* player)
{
    WaitUntilDiscReady();
    sceCdStStop();
    WaitUntilDiscReady();
    if (player->discBuffer != nullptr && player->discBufferLent == 0)
    {
        Platform::Io::FreeHeap(player->discBuffer);
    }

    g_FileLocation = NoFileLocation;
    player->discBuffer = nullptr;
    while (sceCdSync(CdPoll) != 0)
    {
    }
}

// The decoder's libraries (once), its memory and the sound's ring
void Prepare(Platform::Movie::Player* player, void* lentMemory)
{
    if (g_DecodingUnready)
    {
        MovieDecoding::Initialise();
        g_DecodingUnready = false;
    }

    player->frames = 0;
    // The decoder takes the renderer's DMA memory after the chains' movie buffers (they're at the start of the frames' buffers,
    // which aren't used while a movie plays)
    MovieDecoding::SetArena(g_DmaMovieNext, g_RendererDmaMemory + RendererDmaMemorySize);
    g_Ring = nullptr;
    g_SoundMode = player->audio;
    g_RingSize = player->audio == MoviePcm ? SoundRingSize : 0;
    if (g_RingSize != 0)
    {
        g_Ring = static_cast<u8*>(lentMemory);
        g_RingLent = true;
        if (g_Ring == nullptr)
        {
            g_Ring = static_cast<u8*>(Platform::Io::AllocateHeap(g_RingSize));
            g_RingLent = false;
        }
        else
        {
            g_Ring += DiscBufferSize;
        }
    }

    g_SoundStarted = 0;
    g_RingFilled = 0;
    g_RingWrite = 0;
}

// The picture decoded last waits for the vertical blank when fewer than two do. Returns whether it went in
bool QueueDecoded(Platform::Movie::Player* player)
{
    if (player->decoded == nullptr || player->queued >= PictureQueueSize)
    {
        return false;
    }

    DI();
    player->queue[player->queued] = player->decoded;
    player->queued = player->queued + 1;
    EI();
    return true;
}

s32 DecodeNext(void* player)
{
    Platform::Movie::Player* state = static_cast<Platform::Movie::Player*>(player);
    return MovieDecoding::DecodeAndUpload(&state->decoded, state->frameBase);
}
}

// Calls function(argument) with the scratchpad copied into copy and the stack moved into the copy, the scratchpad put back after:
// libmpeg works in the scratchpad, where the main thread's stack is. stackPointers gets the stack pointer in the copy and the one
// it was
extern "C" s32 CallWithScratchpadCopied(u8* copy, u32* stackPointers, s32 (*function)(void*), void* argument);

asm(R"(
    .section .text.CallWithScratchpadCopied, "ax", @progbits
    .globl CallWithScratchpadCopied
    .type CallWithScratchpadCopied, @function
    .ent CallWithScratchpadCopied
    .set noreorder
CallWithScratchpadCopied:
    addiu $sp, $sp, -0x30
    sd $31, 0x00($sp)
    sd $16, 0x08($sp)
    sd $17, 0x10($sp)
    sd $18, 0x18($sp)
    sd $19, 0x20($sp)
    move $16, $4
    move $17, $5
    move $18, $6
    move $19, $7
    sw $sp, 4($17)
    move $4, $16
    lui $5, 0x7000
    jal CopyBytesIntoMemory
    li $6, 0x4000
    jal DIntr
    nop
    lw $3, 4($17)
    li $2, 0x90000000
    addu $3, $3, $2
    addu $3, $16, $3
    sw $3, 0($17)
    move $sp, $3
    jal EIntr
    nop
    sync.p
    jalr $18
    move $4, $19
    move $19, $2
    jal FlushCache
    move $4, $0
    lui $4, 0x7000
    move $5, $16
    jal CopyBytesIntoMemory
    li $6, 0x4000
    jal DIntr
    nop
    lw $sp, 4($17)
    jal EIntr
    nop
    move $2, $19
    ld $31, 0x00($sp)
    ld $16, 0x08($sp)
    ld $17, 0x10($sp)
    ld $18, 0x18($sp)
    ld $19, 0x20($sp)
    jr $31
    addiu $sp, $sp, 0x30
    .set reorder
    .end CallWithScratchpadCopied
    .size CallWithScratchpadCopied, . - CallWithScratchpadCopied
)");

namespace
{
s32 DecodeOnCopiedStack(Platform::Movie::Player* player)
{
    return CallWithScratchpadCopied(player->scratchpad, &player->stackPointer, DecodeNext, player);
}
}

void Platform::Movie::Construct(Player* player)
{
    player->queue[0] = nullptr;
    player->queue[1] = nullptr;
    player->queued = 0;
    player->decoded = nullptr;
    player->opened = 0;
    player->frames = 0;
    player->discBuffer = nullptr;
    player->container = MoviePss;
    player->audio = MoviePcm;
    player->format = MovieMpeg2;
}

void Platform::Movie::BeginPresenting(Player* player, void (*present)())
{
    g_Present = present;
    DI();
    DisableIntc(INTC_VBLANK_S);
    sceGsSyncVCallback(OnVerticalBlank);
    EnableIntc(INTC_VBLANK_S);
    EI();
    player->queued = 0;
    player->frameBase = g_DepthBufferPage << GsPageBlocksShift;
}

bool Platform::Movie::Open(Player* player, const char* file, u32 audioChannel, s32 width, void* lentMemory)
{
    player->container = MoviePss;
    player->audio = MoviePcm;
    player->format = MovieMpeg2;
    Prepare(player, lentMemory);
    OpenStream(player, file, lentMemory);
    // The decoder's read and seek go to the stream, which doesn't need the path
    player->opened = MovieDecoding::Open(&player->decoder, player->container, player->format, player->audio, audioChannel,
                                         SeekStream, ReadStream, nullptr, 1, static_cast<u32>(width) / BufferWidthUnit, MovieRgb32);
    if (player->opened != 1)
    {
        return false;
    }

    // The first two pictures, and the sound's ring filled
    DecodeOnCopiedStack(player);
    QueueDecoded(player);
    DecodeOnCopiedStack(player);
    while (!MovieDecoding::Finished())
    {
        bool full = g_SoundMode == MovieNoAudio || (g_SoundMode == MoviePcm && g_RingFilled >= g_RingSize);
        if (full)
        {
            break;
        }

        MovieDecoding::ReadOn();
        MovieDecoding::FeedAudio(FeedSound, 0);
    }

    return true;
}

// The sound processor plays the ring in a loop
void Platform::Movie::StartSound(Player*, f32 volume)
{
    if (g_SoundMode != MoviePcm)
    {
        return;
    }

    sceSdRemote(SdWait, SdBlockTransfer, SdCore0, SdBlockLoopWrite, g_Ring, g_RingSize / SoundBlockBytes * SoundBlockBytes,
                g_Ring);
    SetSoundVolume(volume);
    g_SoundStarted = 1;
}

bool Platform::Movie::Step(Player* player)
{
    if (MovieDecoding::Finished())
    {
        return false;
    }

    if (QueueDecoded(player))
    {
        DecodeOnCopiedStack(player);
        MovieDecoding::FeedAudio(FeedSound, 0);
        player->frames++;
    }

    return true;
}

void Platform::Movie::WaitFrame(Player* player)
{
    // The stream read twice at most meanwhile
    constexpr s32 ReadsWhileWaiting = 2;
    g_Waiting = 1;
    s32 reads = 0;
    while (g_Presented == 0)
    {
        if (player->queued < PictureQueueSize && reads < ReadsWhileWaiting)
        {
            reads++;
            MovieDecoding::ReadOn();
        }
    }

    g_Presented = 0;
    g_Waiting = 0;
}

void Platform::Movie::QueuePicture(Player* player)
{
    if (player->queued == 0)
    {
        return;
    }

    MovieDecoding::StartChainDma(GifChannel, player->queue[0]);
    u8* next = player->queue[1];
    player->queued = player->queued - 1;
    player->queue[1] = nullptr;
    player->queue[0] = next;
}

// The picture's sprites: 32 pixel wide strips over the area, the texture the depth buffer's memory the pictures were sent to,
// through the GS's second context (its frame the renderer's frame buffer, with the alpha kept)
void Platform::Movie::Draw(Player* player, const Area& area, s32 width, s32 height, u32 skippedRows)
{
    u32 textureWidth = static_cast<u32>(width) / BufferWidthUnit;
    if (static_cast<u32>(width) % BufferWidthUnit != 0)
    {
        textureWidth++;
    }

    u32 x = static_cast<u32>(area.x) << GsSubpixelShift;
    u32 y = static_cast<u32>(area.y) << GsSubpixelShift;
    u32 areaWidth = static_cast<u32>(area.width) << GsSubpixelShift;
    u32 areaHeight = static_cast<u32>(area.height) << GsSubpixelShift;
    u32 strips = areaWidth / StripWidth;
    if (player->frames < 2)
    {
        return;
    }

    RenderBucket& bucket = g_FrameBuckets.buckets[BucketMovies];
    u8* packet = g_DmaChains[bucket.chain].next;
    bucket.last[1] = reinterpret_cast<u32>(packet);
    u32* tag = reinterpret_cast<u32*>(packet);
    u32 quadwords = strips * StripWrites + SettingWrites + 1;
    // A DMA tag of the packet's quadwords, VIF1's FLUSHA and DIRECT of them
    tag[0] = quadwords | CountTag;
    tag[1] = 0;
    tag[2] = VifFlushA;
    tag[3] = quadwords | VifDirect;
    u64* at = reinterpret_cast<u64*>(packet + QuadwordBytes);
    auto write = [&at](u64 low, u64 high)
    {
        at[0] = low;
        at[1] = high;
        at += 2;
    };

    // The GIF tag: sprites, the registers' writes (A+D) of the settings and every strip's corners
    write((strips * StripWrites + SettingWrites) | SpritesTag.lo, SpritesTag.hi);
    write(MovieAttributes, gs_g_prmodecont);
    write(g_FrameBufferPage | MovieFrame, gs_g_frame_2);
    write(0, gs_g_xyoffset_2);
    write(g_DepthBufferPage | MovieDepth, gs_g_zbuf_2);
    write(0, gs_g_texflush);
    write((g_DepthBufferPage << GsPageBlocksShift) | static_cast<u64>(textureWidth) << GsTextureWidthShift | MovieTexture,
          gs_g_tex0_2);
    write(MovieTest, gs_g_test_2);
    write(TexturedSprite, gs_g_prim);
    write(White, gs_g_rgbaq);
    if (strips != 0)
    {
        u32 step = static_cast<u32>(width) / strips << GsSubpixelShift;
        u64 top = static_cast<u64>((skippedRows << GsSubpixelShift) + GsHalfTexel) << GsUvVShift;
        u64 bottom = static_cast<u64>(((height - skippedRows) << GsSubpixelShift) + GsHalfTexel) << GsUvVShift;
        u64 depth = u64{GsFrontDepth} << GsXyzDepthShift;
        u32 left = GsHalfTexel;
        for (u32 strip = 0; strip < strips; strip++)
        {
            u32 screenLeft = x + strip * StripWidth;
            write(left | top, gs_g_uv);
            write(screenLeft | static_cast<u64>(y) << GsXyzYShift | depth, gs_g_xyz2);
            write((left + step) | bottom, gs_g_uv);
            write((screenLeft + StripWidth) | static_cast<u64>(y + areaHeight) << GsXyzYShift | depth, gs_g_xyz2);
            left += step;
        }
    }

    // The bucket's new last tag, a "next" tag after the packet
    DmaChain& chain = g_DmaChains[bucket.chain];
    chain.next = reinterpret_cast<u8*>(at);
    u8* next = g_DmaChains[bucket.chain].next;
    u32* nextTag = reinterpret_cast<u32*>(next);
    bucket.last = nextTag;
    nextTag[0] = NextTag;
    nextTag[1] = 0;
    nextTag[2] = 0;
    nextTag[3] = 0;
    g_DmaChains[bucket.chain].next = next + QuadwordBytes;
}

// The vertical blank's handler goes, the disc stream stops, the sound and its ring go and the decoder closes
void Platform::Movie::Close(Player* player)
{
    DI();
    DisableIntc(INTC_VBLANK_S);
    sceGsSyncVCallback(nullptr);
    EI();
    CloseStream(player);
    SetSoundVolume(0.0f);
    sceSdRemote(SdWait, SdBlockTransfer, SdCore0, SdBlockStop, 0, 0);
    if (g_Ring != nullptr && !g_RingLent)
    {
        Platform::Io::FreeHeap(g_Ring);
    }

    g_Ring = nullptr;
    g_RingSize = 0;
    if (player->opened == 1)
    {
        MovieDecoding::Close();
    }
}
