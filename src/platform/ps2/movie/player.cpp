#include "movie.h"

#include "../renderer/renderer.h"

#include "platform/io.h"
#include "retail/libc.h"

#include <kernel.h>
#include <libcdvd.h>
#include <sifdma.h>

extern "C"
{
    // Sony's libsdr (the sound processor's driver's calls) and libgraph's vertical blank handler, still in the asm
    s32 sceSdRemote(s32 arg, s32 command, ...);
    void* sceGsSyncVCallback(s32 (*handler)(s32 cause));

    // The renderer's DMA memory, from MemoryAllocate2(0x540000): the decoder takes what's after the chains' movie buffers (they're
    // at the start of the frames' buffers, which aren't used while a movie plays)
    extern u8* g_RendererDmaMemory RETAIL(G_DMA_ByteStream_Beg_);
    // The GS's memory: the Z buffer's page (the movie's pictures go there) and the frame buffer's
    extern u32 g_ZBufferPage RETAIL(D_0030AB00);
    extern u32 g_FrameBufferPage RETAIL(D_0030AAFC);
}

namespace
{
constexpr u32 MovieBucket = 27;
constexpr u32 GifChannel = 2;

// The disc stream's buffer (0x50 sectors in 5 banks, 64 byte aligned) and the sound's ring after it in the memory the game lends
constexpr u32 DiscBufferSize = 0x28040;
constexpr u32 SoundRingSize = 0x6000;

// libsdr's commands and the SPU2's: the block transfer of core 0 (its start, stop and where it plays), its input's volume
constexpr s32 SdBlockTransfer = 0x80E0;
constexpr s32 SdBlockTransferStatus = 0x8100;
constexpr s32 SdSetParameter = 0x8010;
constexpr s32 SdBlockLoopWrite = 0x13;
constexpr s32 SdBlockStop = 2;
constexpr s32 SdInputVolumeLeft = 0xF80;
constexpr s32 SdInputVolumeRight = 0x1080;

void (*g_Present)();
// The vertical blank's handler presented since WaitFrame started waiting, every second blank while it waits
volatile u8 g_Presented;
volatile u8 g_BlankToggle;
volatile u8 g_Waiting;

// The decoder's libraries are set up with the first movie
bool g_DecodingUnready = true;

// The movie file on the disc, and the stream's position in it
u32 g_FileLocation = 0xFFFFFFFF;
u32 g_FileSize;
u32 g_ReadPosition;
sceCdRMode g_ReadMode = {0, 0, 0, 0};

// The sound's ring in the I/O processor's memory: where the next bytes go, how many went in before the sound started, whether it's
// the game's memory
u32 g_SoundMode;
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
    if (g_SoundMode != 1)
    {
        return 0;
    }

    u8* to;
    s32 toSize;
    u8* toWrapped = nullptr;
    s32 toWrappedSize = 0;
    if (g_SoundStarted != 0)
    {
        u32 playing = (static_cast<u32>(sceSdRemote(1, SdBlockTransferStatus, 0)) & 0xFFFFFF) - reinterpret_cast<u32>(g_Ring);
        s32 room = static_cast<s32>(playing + g_RingSize - g_RingWrite - 0x400) % g_RingSize / 0x400 * 0x400;
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
        toSize = (g_RingSize - g_RingFilled) / 0x400 * 0x400;
        to = g_Ring + g_RingFilled;
    }

    u32 whole = count >> 10 << 10;
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
    if (g_SoundMode != 1)
    {
        return;
    }

    s32 value = static_cast<s32>(volume * 32767.0f);
    sceSdRemote(1, SdSetParameter, SdInputVolumeLeft, value);
    sceSdRemote(1, SdSetParameter, SdInputVolumeRight, value);
}

// The decoder's file: the disc stream from the file's start
s32 SeekStream(const char*)
{
    s32 result = sceCdStSeek(g_FileLocation);
    g_ReadPosition = 0;
    return result;
}

// Reads whole sectors (blocking), again after a read the drive had trouble with. Returns the bytes, -1 past the file's end
s32 ReadStream(u8* buffer, u32 size, const char*)
{
    u32 position = g_ReadPosition;
    if (position + size >= g_FileSize && position >= g_FileSize)
    {
        return -1;
    }

    u32 sectors = static_cast<s32>(size) / 0x800;
    u32 error = 0;
    s32 read = sceCdStRead(sectors, reinterpret_cast<u32*>(buffer), 0, &error) << 11;
    while (error != 0 || sceCdSync(0) != 0 || sceCdGetError() != 0)
    {
        while (sceCdDiskReady(0) != SCECdComplete)
        {
        }

        read = sceCdStRead(sectors, reinterpret_cast<u32*>(buffer), 0, &error) << 11;
    }

    g_ReadPosition += read;
    return read;
}

void WaitUntilDiscReady()
{
    while (sceCdDiskReady(1) != SCECdComplete)
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

    sceCdStInit(0x50, 5, reinterpret_cast<void*>((reinterpret_cast<u32>(player->discBuffer) + 0x3F) & ~0x3Fu));
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

    g_FileLocation = 0xFFFFFFFF;
    player->discBuffer = nullptr;
    while (sceCdSync(1) != 0)
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
    MovieDecoding::SetArena(g_DmaMovieNext, g_RendererDmaMemory + 0x540000);
    g_Ring = nullptr;
    g_SoundMode = player->audio;
    g_RingSize = player->audio == 1 ? SoundRingSize : 0;
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
    if (player->decoded == nullptr || player->queued >= 2)
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
    player->mode = 1;
    player->audio = 1;
    player->format = 2;
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
    player->frameBase = g_ZBufferPage << 5;
}

bool Platform::Movie::Open(Player* player, const char* file, u32 audioChannel, s32 width, void* lentMemory)
{
    player->mode = 1;
    player->audio = 1;
    player->format = 2;
    Prepare(player, lentMemory);
    OpenStream(player, file, lentMemory);
    // The decoder's read and seek go to the stream, which doesn't need the path
    player->opened = MovieDecoding::Open(&player->decoder, player->mode, player->format, player->audio, audioChannel, SeekStream,
                                         ReadStream, nullptr, 1, static_cast<u32>(width) >> 6, 0x100);
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
        bool full = g_SoundMode == 0 || (g_SoundMode == 1 && g_RingFilled >= g_RingSize);
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
    if (g_SoundMode != 1)
    {
        return;
    }

    sceSdRemote(1, SdBlockTransfer, 0, SdBlockLoopWrite, g_Ring, g_RingSize / 0x400 * 0x400, g_Ring);
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
    g_Waiting = 1;
    s32 reads = 0;
    while (g_Presented == 0)
    {
        if (player->queued < 2 && reads < 2)
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

// The picture's sprites: 32 pixel wide strips over the area, the texture the Z buffer's memory the pictures were sent to, through
// the GS's second context (its frame the renderer's frame buffer, with the alpha kept)
void Platform::Movie::Draw(Player* player, const Area& area, s32 width, s32 height, u32 skippedRows)
{
    u32 textureWidth = static_cast<u32>(width) >> 6;
    if ((width & 0x3F) != 0)
    {
        textureWidth++;
    }

    u32 x = static_cast<u32>(area.x) << 4;
    u32 y = static_cast<u32>(area.y) << 4;
    u32 areaWidth = static_cast<u32>(area.width) << 4;
    u32 areaHeight = static_cast<u32>(area.height) << 4;
    u32 strips = areaWidth >> 9;
    if (player->frames < 2)
    {
        return;
    }

    RenderBucket& bucket = g_FrameBuckets.buckets[MovieBucket];
    u8* packet = g_DmaChains[bucket.chain].next;
    bucket.last[1] = reinterpret_cast<u32>(packet);
    u32* tag = reinterpret_cast<u32*>(packet);
    u32 quadwords = strips * 4 + 10;
    // A DMA tag of the packet's quadwords, VIF1's FLUSHA and DIRECT of them
    tag[0] = quadwords | 0x10000000;
    tag[1] = 0;
    tag[2] = 0x13000000;
    tag[3] = quadwords | 0x50000000;
    u64* at = reinterpret_cast<u64*>(packet + 0x10);
    auto write = [&at](u64 low, u64 high)
    {
        at[0] = low;
        at[1] = high;
        at += 2;
    };

    // The GIF tag: sprites, the registers' writes (A+D) of the settings and every strip's corners
    write((strips * 4 + 9) | 0x8000 | 0x1003400000000000, 0xE);
    // PRMODECONT, FRAME_2, XYOFFSET_2, ZBUF_2 (not written), TEXFLUSH, TEX0_2, TEST_2 (depth greater or equal), PRIM (a textured
    // sprite of context 2, UV in texels), RGBAQ
    write(1, 0x1A);
    write(g_FrameBufferPage | 0x80000 | 0xFF00000000000000, 0x4D);
    write(0, 0x19);
    write(g_ZBufferPage | 0x30000000 | 0x100000000, 0x4F);
    write(0, 0x3F);
    write((g_ZBufferPage << 5) | static_cast<u64>(textureWidth) << 14 | 0x2000000EA8000000, 0x07);
    write(0x50000, 0x48);
    write(0x316, 0);
    write(0x3F80000080808080, 1);
    if (strips != 0)
    {
        u32 step = static_cast<u32>(width) / strips << 4;
        u64 top = static_cast<u64>((skippedRows << 4) + 8) << 16;
        u64 bottom = static_cast<u64>(((height - skippedRows) << 4) + 8) << 16;
        u64 depth = 0xFFFFFFFF00000000;
        u32 left = 8;
        for (u32 strip = 0; strip < strips; strip++)
        {
            u32 screenLeft = x + (strip << 9);
            write(left | top, 3);
            write(screenLeft | static_cast<u64>(y) << 16 | depth, 5);
            write((left + step) | bottom, 3);
            write((screenLeft + 0x200) | static_cast<u64>(y + areaHeight) << 16 | depth, 5);
            left += step;
        }
    }

    // The bucket's new last tag, a "next" tag after the packet
    DmaChain& chain = g_DmaChains[bucket.chain];
    chain.next = reinterpret_cast<u8*>(at);
    u8* next = g_DmaChains[bucket.chain].next;
    u32* nextTag = reinterpret_cast<u32*>(next);
    bucket.last = nextTag;
    nextTag[0] = 0x20000000;
    nextTag[1] = 0;
    nextTag[2] = 0;
    nextTag[3] = 0;
    g_DmaChains[bucket.chain].next = next + 0x10;
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
    sceSdRemote(1, SdBlockTransfer, 0, SdBlockStop, 0, 0);
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
