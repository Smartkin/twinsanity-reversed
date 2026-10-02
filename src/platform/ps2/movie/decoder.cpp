#include "movie.h"

#include "retail/libc.h"

#include <ee_regs.h>
#include <kernel.h>

// The decoder of Sony's movie samples the game built its player on: libmpeg for PSS files, the IPU's own command stream for its
// formats 0 and 1
namespace
{
volatile u32* const IpuCommand = reinterpret_cast<volatile u32*>(A_EE_IPU_CMD);
volatile u32* const IpuControl = reinterpret_cast<volatile u32*>(A_EE_IPU_CTRL);

// The DMA channels' registers by number (the retail CHCR_ARRAY: 5 to 7 are the SIF's, which it leaves out)
constexpr u32 ChannelRegisters[10] = {0x10008000, 0x10009000, 0x1000A000, 0x1000B000, 0x1000B400, 0, 0, 0, 0x1000D000, 0x1000D400};
constexpr u32 IpuFromChannel = 3;
constexpr u32 IpuToChannel = 4;

constexpr u32 ChcrStart = 0x100;
constexpr u32 IpuControlReset = 0x40000000;
constexpr u32 IpuControlEndCode = 0x4000;
constexpr u32 ControlSuspend = 0x10000; // D_ENABLEW.CPND

// The IPU's commands: BCLR, FDEC (the bits it skips), IDEC and CSC
constexpr u32 IpuClearInput = 0x00000000;
constexpr u32 IpuReadBits = 0x40000000;
constexpr u32 IpuDecodeIntra = 0x14010000;
constexpr u32 IpuDecodeIntra16 = 0x1C010000;
constexpr u32 IpuConvert16 = 0x7C000000;

// libmpeg's callbacks and streams
constexpr s32 CallbackError = 0;
constexpr s32 CallbackNoData = 1;
constexpr s32 CallbackBackground = 4;
constexpr s32 StreamVideo = 0;
constexpr s32 StreamIpu = 1;
constexpr s32 StreamPcm = 2;
constexpr s32 StreamAdpcm = 3;
constexpr s32 StreamData = 4;

MovieDecoder* g_Decoder;
// The arena's next byte (nothing checks its end)
u8* g_ArenaNext;
// The DMA channels' CHCR settings (the retail D_003C6EC0)
u32 g_ChannelSettings[10];

u8* ArenaAllocate(u32 alignment, u32 size)
{
    u8* memory = reinterpret_cast<u8*>((reinterpret_cast<u32>(g_ArenaNext) + alignment - 1) & -alignment);
    g_ArenaNext = memory + size;
    return memory;
}

volatile u32* ChannelRegister(u32 channel, u32 offset)
{
    return reinterpret_cast<volatile u32*>(ChannelRegisters[channel] + offset);
}

// The DMA controller's address of the memory: the scratchpad's with bit 31 (Sony's DmaAddr)
u32 DmaAddress(const void* memory)
{
    u32 address = reinterpret_cast<u32>(memory) & 0xCFFFFFFF;
    if ((address & 0xC0000000) != 0)
    {
        address += 0x40000000;
    }

    return address;
}

// Through the uncached segment: what the DMA reads has to be in memory. The video ring's writes take the address's segment off
// first, the sound ring's and the upload chains' don't
u8* Uncached(u8* memory)
{
    return reinterpret_cast<u8*>((reinterpret_cast<u32>(memory) & 0x0FFFFFFF) | 0x20000000);
}

u8* UncachedSegment(u8* memory)
{
    return reinterpret_cast<u8*>(reinterpret_cast<u32>(memory) | 0x20000000);
}

void SetChannelSettings(u32 channel, u32 fromMemory, u32 tagTransfer, u32 tagInterrupt, u32 interleave, u32 skipQuadwords,
                        u32 transferQuadwords)
{
    u32 settings = fromMemory != 0 ? 1 : 0;
    if (tagTransfer != 0)
    {
        settings |= 0x40;
    }

    if (tagInterrupt != 0)
    {
        settings |= 0x80;
    }

    if (interleave != 0)
    {
        settings |= 0x8;
    }

    *R_EE_D_SQWC = skipQuadwords | transferQuadwords << 16;
    g_ChannelSettings[channel] = settings;
}

void StartNormalDma(u32 channel, const void* memory, u32 quadwords)
{
    *ChannelRegister(channel, 0x10) = DmaAddress(memory);
    *ChannelRegister(channel, 0x20) = quadwords;
    *R_EE_D_STAT = 1u << channel;
    *ChannelRegister(channel, 0) = g_ChannelSettings[channel] | ChcrStart;
    asm volatile("sync" : : : "memory");
}

// The IPU's channels stopped (the DMA controller held meanwhile)
void StopIpuChannels(bool both)
{
    DI();
    *R_EE_D_ENABLEW = ControlSuspend;
    if (both)
    {
        *R_EE_D3_CHCR = 0;
        *R_EE_D3_QWC = 0;
    }

    *R_EE_D4_CHCR = 0;
    *R_EE_D4_QWC = 0;
    *R_EE_D_ENABLEW = 0;
    asm volatile("sync" : : : "memory");
    EI();
}

void ResetIpu()
{
    *IpuControl = IpuControlReset;
    sceIpuSync(0, 0);
    *IpuCommand = IpuClearInput;
    sceIpuSync(0, 0);
}

s32 Demultiplex(bool wait);

// While the IPU works the decoder reads on
void WhileIpuBusy()
{
    Demultiplex(false);
}

// libmpeg's callback when the IPU needs data, and the IPU's waits': up to 0x2000 bytes of the video ring go to the IPU's input
// channel, read and demultiplexed first while there are less
s32 FeedIpu(Mpeg*, void*, void*)
{
    if ((*R_EE_D4_CHCR & ChcrStart) != 0)
    {
        return 1;
    }

    MovieDecoder* decoder = g_Decoder;
    decoder->videoRead = decoder->videoNext;
    s32 pending = decoder->videoWrite - decoder->videoNext;
    if (pending < 0)
    {
        pending += decoder->videoSize;
    }

    while (pending < 0x2000)
    {
        if (Demultiplex(true) == 0)
        {
            decoder = g_Decoder;
            pending += 0xF;
            if (decoder->ended != 0)
            {
                if (decoder->format == 2)
                {
                    MpegStreamEnded(&decoder->mpeg);
                }

                return 1;
            }

            break;
        }

        decoder = g_Decoder;
        pending = decoder->videoWrite - decoder->videoNext;
        if (pending < 0)
        {
            pending += decoder->videoSize;
        }
    }

    decoder = g_Decoder;
    u32 size = static_cast<u32>(pending) & ~0xFu;
    if (size >= 0x2001)
    {
        size = 0x2000;
    }

    u8* from = decoder->videoRead;
    u8* end = decoder->videoBase + decoder->videoSize;
    if (end < from + size)
    {
        size = end - from;
    }

    StartNormalDma(IpuToChannel, from, size >> 4);
    decoder = g_Decoder;
    u8* next = decoder->videoRead + size;
    decoder->videoNext = next;
    if (!(next < decoder->videoBase + decoder->videoSize))
    {
        decoder->videoNext = next - decoder->videoSize;
    }

    return 1;
}

// Waits until the IPU is done with its command (or found an end code), feeding it when it waits for data no DMA brings. With
// output, until its output channel is done
void WaitIpu(bool output)
{
    if (g_Decoder->ended != 0)
    {
        return;
    }

    while (true)
    {
        if ((*IpuControl & IpuControlEndCode) != 0)
        {
            return;
        }

        if (static_cast<s32>(*IpuControl) >= 0)
        {
            return;
        }

        if ((*R_EE_D4_CHCR & ChcrStart) == 0)
        {
            for (s32 tries = 0;; tries++)
            {
                if (static_cast<s32>(*IpuControl) >= 0)
                {
                    break;
                }

                if (tries >= 0x3E9)
                {
                    FeedIpu(nullptr, nullptr, nullptr);
                    break;
                }
            }
        }

        if (output && (*R_EE_D3_CHCR & ChcrStart) == 0)
        {
            return;
        }

        if (g_Decoder->ended != 0)
        {
            return;
        }
    }
}

// libmpeg's callback for the video: the packet goes into the video ring through the uncached segment, the IPU's DMA reads it.
// Returns 0 when there isn't room
s32 StoreVideo(Mpeg*, void* streamData, void*)
{
    MpegStreamData* data = static_cast<MpegStreamData*>(streamData);
    MovieDecoder* decoder = g_Decoder;
    if (decoder->format < 2 && decoder->mode == 1)
    {
        // The IPU stream's packets start with 4 bytes of their own
        data->data += 4;
        data->length -= 4;
    }

    decoder = g_Decoder;
    u8* read = decoder->videoRead;
    s32 room = read - decoder->videoWrite;
    if (room < 0)
    {
        room += decoder->videoSize;
    }

    if (room == 0 && read == decoder->videoNext)
    {
        room = decoder->videoSize;
    }

    u32 length = data->length;
    if (static_cast<u32>(room) < length)
    {
        return 0;
    }

    s32 over = (decoder->videoWrite - decoder->videoBase) + static_cast<s32>(length) - decoder->videoSize;
    if (over > 0)
    {
        RetailLibc::MemoryCopy(Uncached(decoder->videoWrite), data->data, length - over);
        RetailLibc::MemoryCopy(Uncached(g_Decoder->videoBase), data->data + data->length - over, over);
        g_Decoder->videoWrite = g_Decoder->videoBase + over;
    }
    else
    {
        RetailLibc::MemoryCopy(Uncached(decoder->videoWrite), data->data, length);
        g_Decoder->videoWrite += data->length;
    }

    return 1;
}

// libmpeg's callback for the sound: the header goes aside, the rest into the sound's ring. Returns 0 (and marks the ring full) when
// there isn't room
s32 StoreAudio(Mpeg*, void* streamData, void*)
{
    MpegStreamData* data = static_cast<MpegStreamData*>(streamData);
    MovieDecoder* decoder = g_Decoder;
    data->data += 4;
    data->length -= 4;
    if (decoder->audio != 3 && decoder->audioHeaderBytes < static_cast<s32>(sizeof(MovieAudioHeader)))
    {
        u8* header = reinterpret_cast<u8*>(&decoder->audioHeader) + decoder->audioHeaderBytes;
        u32 missing = sizeof(MovieAudioHeader) - decoder->audioHeaderBytes;
        if (missing < data->length)
        {
            RetailLibc::MemoryCopy(header, data->data, missing);
            decoder = g_Decoder;
            data->data = data->data + sizeof(MovieAudioHeader) - decoder->audioHeaderBytes;
            data->length = data->length - sizeof(MovieAudioHeader) + decoder->audioHeaderBytes;
            decoder->audioHeaderBytes = sizeof(MovieAudioHeader);
        }
        else
        {
            RetailLibc::MemoryCopy(header, data->data, data->length);
            decoder = g_Decoder;
            decoder->audioHeaderBytes += data->length;
            data->length = 0;
        }
    }

    decoder = g_Decoder;
    u32 length = data->length;
    if (static_cast<u32>(decoder->audioSize) < decoder->audioCount + length)
    {
        decoder->audioFull = 1;
        return 0;
    }

    s32 at = (decoder->audioRead + decoder->audioCount) % decoder->audioSize;
    s32 over = at + static_cast<s32>(length) - decoder->audioSize;
    if (over > 0)
    {
        RetailLibc::MemoryCopy(UncachedSegment(decoder->audioBase + at), data->data, length - over);
        RetailLibc::MemoryCopy(UncachedSegment(g_Decoder->audioBase), data->data + data->length - over, over);
    }
    else
    {
        RetailLibc::MemoryCopy(UncachedSegment(decoder->audioBase + at), data->data, length);
    }

    g_Decoder->audioCount += data->length;
    return 1;
}

// Reads the file's next sectors and demultiplexes them (a raw stream all goes to the video ring), waiting for the disc unless not
// told to. Returns the bytes used, 0 when none (the read buffer keeps what's left)
s32 Demultiplex(bool wait)
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->mode == 0)
    {
        if (decoder->pending == 0)
        {
            while (true)
            {
                s32 read = decoder->read(decoder->readBuffer, 0x4000, decoder->path);
                decoder = g_Decoder;
                decoder->pending = read;
                if (read < 0)
                {
                    decoder->ended = 1;
                    return 0;
                }

                if (read != 0)
                {
                    break;
                }

                if (!wait)
                {
                    return 0;
                }
            }
        }

        decoder = g_Decoder;
        MpegStreamData data;
        data.length = decoder->pending;
        data.data = decoder->readBuffer;
        s32 used = StoreVideo(&decoder->mpeg, &data, nullptr) != 0 ? data.length : 0;
        g_Decoder->pending -= used;
        return used;
    }

    u8* data;
    if (decoder->pending != 0)
    {
        data = decoder->readBuffer - (decoder->pending - 0x4000);
    }
    else
    {
        while (true)
        {
            s32 read = decoder->read(decoder->readBuffer, 0x4000, decoder->path);
            decoder = g_Decoder;
            decoder->pending = read;
            if (read < 0)
            {
                decoder->ended = 1;
                return 0;
            }

            if (read != 0)
            {
                data = decoder->readBuffer;
                break;
            }

            if (!wait)
            {
                return 0;
            }
        }
    }

    g_Decoder->audioFull = 0;
    while (true)
    {
        decoder = g_Decoder;
        s32 used = sceMpegDemuxPss(&decoder->mpeg, data, decoder->pending);
        if (used != 0 || !wait || g_Decoder->audioFull == 0)
        {
            g_Decoder->pending -= used;
            return used;
        }

        // The sound's ring was full: it keeps what isn't a whole block of it and the sectors are demultiplexed again
        decoder = g_Decoder;
        switch (decoder->audio)
        {
        case 1:
            decoder->audioCount %= 0x400;
            break;
        case 2:
            decoder->audioCount %= decoder->audioHeader.interleave * decoder->audioHeader.channels;
            break;
        case 3:
            decoder->audioCount %= 0x200;
            break;
        default:
            break;
        }
    }
}

s32 WaitIpuInBackground(Mpeg*, void*, void*)
{
    while (sceIpuSync(1, 0) != 0)
    {
        WhileIpuBusy();
    }

    return 1;
}

s32 IgnoreError(Mpeg*, void*, void*)
{
    return 1;
}

void AddStreamCallbacks(u32 channel)
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->format == 2)
    {
        sceMpegAddStrCallback(&decoder->mpeg, StreamVideo, 0, StoreVideo, nullptr);
    }
    else
    {
        sceMpegAddStrCallback(&decoder->mpeg, StreamIpu, 0, StoreVideo, nullptr);
    }

    decoder = g_Decoder;
    switch (decoder->audio)
    {
    case 1:
        sceMpegAddStrCallback(&decoder->mpeg, StreamPcm, channel, StoreAudio, nullptr);
        break;
    case 2:
        sceMpegAddStrCallback(&decoder->mpeg, StreamAdpcm, channel, StoreAudio, nullptr);
        break;
    case 3:
        sceMpegAddStrCallback(&decoder->mpeg, StreamData, channel, StoreAudio, nullptr);
        break;
    default:
        break;
    }
}

// The tags and GS registers of the upload chains, written through the uncached segment the way the retail code wrote them: a tag's
// count and ID by halfword and byte, its other bytes as they were
void WriteTag(volatile u8* tag, u16 quadwords, u8 id)
{
    *reinterpret_cast<volatile u16*>(tag) = quadwords;
    tag[3] = id;
}

void WriteQuadword(volatile u8* at, u64 low, u64 high)
{
    reinterpret_cast<volatile u64*>(at)[0] = low;
    reinterpret_cast<volatile u64*>(at)[1] = high;
}

constexpr u8 TagCount = 0x10;
constexpr u8 TagReference = 0x30;
constexpr u8 TagEnd = 0x60;
// A GIF tag of 2 registers' writes (A+D), a GIF tag of an image's quadwords
constexpr u64 GifAddressData2 = 0x1000000000000002;
constexpr u64 GifAddressDataRegisters = 0xE;
constexpr u64 GifImage = 0x0800000000000000;
constexpr u64 GifEndOfPacket = 0x8000;
// The GS's registers: an unused one where the upload's BITBLTBUF goes, TRXPOS, TRXREG, TRXDIR
constexpr u64 GsNothing = 0x7F;
constexpr u64 GsBitBltBuf = 0x50;
constexpr u64 GsTrxPos = 0x51;
constexpr u64 GsTrxReg = 0x52;
constexpr u64 GsTrxDir = 0x53;

// The upload chain of a picture of macroblocks after each other: a transfer a macroblock
void BuildBlockUpload(u8* packet, u8* picture, u32 blocksWide, u32 blocksHigh, u32 halfPixels)
{
    u32 quadwords = halfPixels == 0 ? 0x40 : 0x20;
    SyncDCache(packet, packet + ((blocksWide * (blocksHigh * 6) + 8) >> 2 << 6) - 1);
    volatile u8* at = UncachedSegment(packet);
    WriteTag(at, 3, TagCount);
    at += 0x10;
    WriteQuadword(at, GifAddressData2, GifAddressDataRegisters);
    at += 0x10;
    WriteQuadword(at, 0, GsNothing);
    at += 0x10;
    WriteQuadword(at, 0x1000000010, GsTrxReg);
    at += 0x10;
    for (u32 y = 0; y < blocksHigh; y++)
    {
        for (u32 x = 0; x < blocksWide; x++)
        {
            WriteTag(at, 4, TagCount);
            at += 0x10;
            WriteQuadword(at, GifAddressData2, GifAddressDataRegisters);
            at += 0x10;
            WriteQuadword(at, static_cast<u64>(x << 4) << 32 | static_cast<u64>(y << 4) << 48, GsTrxPos);
            at += 0x10;
            WriteQuadword(at, 0, GsTrxDir);
            at += 0x10;
            u64 last = x == blocksWide - 1 && y == blocksHigh - 1 ? GifEndOfPacket : 0;
            WriteQuadword(at, quadwords | last | GifImage, 0);
            at += 0x10;
            *reinterpret_cast<volatile u32*>(at + 4) = reinterpret_cast<u32>(picture);
            WriteTag(at, quadwords, TagReference);
            picture += quadwords << 4;
            at += 0x10;
        }
    }

    WriteTag(at, 0, TagEnd);
}

// The same of a picture of columns of macroblocks: a transfer a column
void BuildColumnUpload(u8* packet, u8* picture, u32 blocksWide, u32 blocksHigh, u32 halfPixels)
{
    u32 quadwords = halfPixels == 0 ? blocksHigh << 6 : blocksHigh << 5;
    SyncDCache(packet, packet + ((blocksWide * 6 + 8) >> 2 << 6) - 1);
    volatile u8* at = UncachedSegment(packet);
    WriteTag(at, 3, TagCount);
    at += 0x10;
    WriteQuadword(at, GifAddressData2, GifAddressDataRegisters);
    at += 0x10;
    WriteQuadword(at, 0, GsNothing);
    at += 0x10;
    WriteQuadword(at, static_cast<u64>(blocksHigh << 4) << 32 | 0x10, GsTrxReg);
    at += 0x10;
    for (u32 x = 0; x < blocksWide; x++)
    {
        WriteTag(at, 4, TagCount);
        at += 0x10;
        WriteQuadword(at, GifAddressData2, GifAddressDataRegisters);
        at += 0x10;
        WriteQuadword(at, static_cast<u64>(x << 4) << 32, GsTrxPos);
        at += 0x10;
        WriteQuadword(at, 0, GsTrxDir);
        at += 0x10;
        u64 last = x == blocksWide - 1 ? GifEndOfPacket : 0;
        WriteQuadword(at, quadwords | last | GifImage, 0);
        at += 0x10;
        *reinterpret_cast<volatile u32*>(at + 4) = reinterpret_cast<u32>(picture);
        WriteTag(at, quadwords, TagReference);
        picture += quadwords << 4;
        at += 0x10;
    }

    WriteTag(at, 0, TagEnd);
}

// The upload's BITBLTBUF, in place of the unused register: where in the GS's memory and its width and format
void SetUploadDestination(u8* packet, u32 base, u32 widthBlocks, u32 pixelFormat)
{
    volatile u64* registers = reinterpret_cast<volatile u64*>(UncachedSegment(packet));
    registers[5] = GsBitBltBuf;
    registers[4] = static_cast<u64>(base) << 32 | static_cast<u64>(widthBlocks) << 48 | static_cast<u64>(pixelFormat) << 56;
}

// Decodes the next picture into its buffer. Returns whether there was one
s32 DecodePicture()
{
    MovieDecoder* decoder = g_Decoder;
    u32 buffer = (decoder->decoded & 1) != 0 ? decoder->doubleBuffered != 0 : 0;
    u32 macroblocks = (decoder->width >> 4) * (decoder->height >> 4);
    sceIpuRestartDMA(&decoder->ipuDma);
    decoder = g_Decoder;
    if (decoder->format < 2)
    {
        if (decoder->decoded == 0)
        {
            ResetIpu();
        }

        decoder = g_Decoder;
        if (decoder->decoded >= decoder->pictureCount)
        {
            StopIpuChannels(false);
            return 0;
        }
    }

    decoder = g_Decoder;
    if (decoder->format == 2)
    {
        if (sceMpegIsEnd(&decoder->mpeg) != 0)
        {
            return 0;
        }

        decoder = g_Decoder;
        if (decoder->outputFormat - 0x102 < 2)
        {
            if (sceMpegGetPictureRAW8(&decoder->mpeg, decoder->raw8, macroblocks) < 0)
            {
                return 0;
            }

            // The macroblocks converted to 16 bit pixels by the IPU, its DMA put back after
            IpuDmaEnvironment saved;
            sceIpuStopDMA(&saved);
            *R_EE_D4_QWC = macroblocks * 0x18;
            *R_EE_D4_MADR = reinterpret_cast<u32>(g_Decoder->raw8);
            *R_EE_D4_CHCR = 0x101;
            *R_EE_D3_QWC = macroblocks << 5;
            *R_EE_D3_MADR = reinterpret_cast<u32>(g_Decoder->pictures[buffer]);
            *R_EE_D3_CHCR = 0x100;
            *IpuCommand = IpuClearInput;
            sceIpuSync(0, 0);
            *IpuCommand = macroblocks | IpuConvert16;
            while (sceIpuSync(1, 0) != 0)
            {
                WhileIpuBusy();
            }

            sceIpuRestartDMA(&saved);
        }
        else if (sceMpegGetPicture(&decoder->mpeg, decoder->pictures[buffer],
                                   static_cast<s32>(decoder->width * decoder->height) >> 8) < 0)
        {
            return 0;
        }
    }
    else
    {
        u8* picture = decoder->pictures[buffer];
        s32 outputFormat = static_cast<s32>(decoder->outputFormat);
        u32 pixelBits = 0;
        if (outputFormat >= 0x100)
        {
            if (outputFormat < 0x102)
            {
                pixelBits = 0x20;
            }
            else
            {
                pixelBits = outputFormat > 0x103 ? 0 : 0x10;
            }
        }

        // The picture's header: its flags in the first byte, then intra decoding of every macroblock
        *IpuCommand = IpuReadBits;
        WaitIpu(false);
        if (g_Decoder->ended != 0)
        {
            goto failed;
        }

        u32 flags = *IpuCommand >> 24;
        *IpuCommand = IpuReadBits | 8;
        WaitIpu(false);
        if (g_Decoder->ended != 0)
        {
            goto failed;
        }

        *IpuControl = (flags & 0xFB) << 16;
        *IpuCommand = (flags >> 2 & 1) << 24 | (pixelBits == 0x10 ? IpuDecodeIntra16 : IpuDecodeIntra);
        for (u32 left = macroblocks; left != 0;)
        {
            u32 count = left < 0x400 ? left : 0x3FF;
            StartNormalDma(IpuFromChannel, picture, pixelBits == 0x20 ? count << 6 : count << 5);
            WaitIpu(true);
            if (g_Decoder->ended != 0)
            {
                goto failed;
            }

            left -= count;
            picture += count << 10;
        }

        *IpuCommand = IpuReadBits | 0x20;
        WaitIpu(false);
        if (g_Decoder->ended != 0)
        {
            goto failed;
        }
    }

    sceIpuStopDMA(&g_Decoder->ipuDma);
    g_Decoder->decoded++;
    return 1;

failed:
    sceIpuStopDMA(&g_Decoder->ipuDma);
    ResetIpu();
    return 0;
}

// Opens the decoder (create) or starts it over: the buffers come from the arena, the first sectors are read to learn the
// picture's size
s32 Start(bool create, MovieDecoder* decoder, u32 mode, u32 format, u32 audio, u32 audioChannel, s32 (*seek)(const char* path),
          s32 (*read)(u8* buffer, u32 size, const char* path), const char* path, u32 doubleBuffered, u32 widthBlocks,
          u32 outputFormat)
{
    // The first sceMpegCreate's work memory
    alignas(16) u8 work[0x2800];
    u32 pixelBits = 0;
    g_Decoder = decoder;
    StopIpuChannels(true);
    decoder->ended = 0;
    if (create)
    {
        FlushCache(0);
        decoder->readBuffer = ArenaAllocate(0x40, 0x4000);
        if (decoder->readBuffer == nullptr)
        {
            return 0;
        }

        decoder->videoBase = ArenaAllocate(0x40, 0x40000);
        if (decoder->videoBase == nullptr)
        {
            return 0;
        }

        decoder->read = read;
        decoder->seek = seek;
        decoder->audioChannel = audioChannel;
        decoder->videoSize = 0x40000;
        decoder->widthBlocks = widthBlocks;
        decoder->mode = mode;
        decoder->path = path;
        decoder->format = format;
        decoder->audio = audio;
        decoder->outputFormat = outputFormat;
        decoder->doubleBuffered = doubleBuffered;
    }

    // The ring starts with 0x40 bytes going to the IPU
    decoder->pending = 0;
    decoder->videoRead = decoder->videoBase;
    decoder->videoNext = decoder->videoBase + 0x40;
    decoder->videoWrite = decoder->videoBase + 0x40;
    if (audio != 0)
    {
        if (create)
        {
            switch (decoder->audio)
            {
            case 1:
            case 3:
                decoder->audioSize = 0x19000;
                break;
            case 2:
                decoder->audioSize = 0x8000;
                break;
            default:
                break;
            }

            decoder->audioBase = ArenaAllocate(0x40, decoder->audioSize);
            if (decoder->audioBase == nullptr)
            {
                return 0;
            }
        }
        else
        {
            decoder->audioSize = 0;
        }

        decoder->audioRead = 0;
        decoder->audioCount = 0;
        decoder->audioHeaderBytes = 0;
    }

    if (decoder->seek != nullptr)
    {
        decoder->seek(path);
    }

    decoder->decoded = 0;
    if (create && mode == 1)
    {
        sceMpegCreate(&decoder->mpeg, work, sizeof(work));
        AddStreamCallbacks(decoder->audioChannel);
    }

    if (Demultiplex(true) == 0)
    {
        return 0;
    }

    // The picture's size: the IPU file's header, or the MPEG sequence header's
    bool ipuFormat = format < 2;
    if (ipuFormat)
    {
        if (create)
        {
            u8* header = decoder->videoNext;
            decoder->width = *reinterpret_cast<u16*>(header + 8);
            decoder->height = *reinterpret_cast<u16*>(header + 0xA);
            decoder->pictureCount = *reinterpret_cast<s32*>(header + 0xC);
        }

        decoder->videoNext += 0x10;
    }
    else if (create)
    {
        u8* header = decoder->videoNext;
        decoder->width = header[4] << 4 | header[5] >> 4;
        decoder->height = (header[5] & 0xF) << 8 | header[6];
    }

    if (format == 2 || mode == 1)
    {
        if (create)
        {
            u32 size = ipuFormat ? 0x2800 : (decoder->width * decoder->height * 9 >> 1) + 0x2800;
            decoder->work = ArenaAllocate(0x40, size);
            if (decoder->work == nullptr)
            {
                return 0;
            }

            sceMpegCreate(&decoder->mpeg, decoder->work, size);
            if (format == 2)
            {
                sceMpegAddCallback(&decoder->mpeg, CallbackNoData, FeedIpu, nullptr);
                sceMpegAddCallback(&decoder->mpeg, CallbackBackground, WaitIpuInBackground, decoder);
            }

            sceMpegAddCallback(&decoder->mpeg, CallbackError, IgnoreError, nullptr);
            if (mode == 1)
            {
                AddStreamCallbacks(decoder->audioChannel);
            }
        }
        else
        {
            sceMpegReset(&decoder->mpeg);
        }
    }

    if (create)
    {
        if (outputFormat < 0x100)
        {
            outputFormat = 0x100;
            pixelBits = 0x20;
        }
        else if (outputFormat < 0x102)
        {
            pixelBits = 0x20;
        }
        else if (outputFormat < 0x104)
        {
            pixelBits = 0x10;
        }
        else
        {
            outputFormat = 0x100;
            pixelBits = 0x20;
        }

        if (decoder->outputFormat - 0x102 < 2 && decoder->format == 2)
        {
            decoder->raw8 = ArenaAllocate(0x40, (decoder->width >> 4) * (decoder->height >> 4) * 0x180);
            if (decoder->raw8 == nullptr)
            {
                return 0;
            }
        }
    }

    u32 halfPixels = pixelBits == 0x10 ? 2 : 0;
    for (s32 i = 0; create && (doubleBuffered != 0 ? i < 2 : i <= 0); i++)
    {
        u32 blocksWide = decoder->width >> 4;
        u32 blocksHigh = decoder->height >> 4;
        u32 macroblocks = blocksWide * blocksHigh;
        decoder->pictures[i] = ArenaAllocate(0x40, pixelBits == 0x10 ? macroblocks << 9 : macroblocks << 10);
        if (decoder->pictures[i] == nullptr)
        {
            return 0;
        }

        if (decoder->format == 0)
        {
            decoder->uploads[i].packet = ArenaAllocate(0x40, (macroblocks * 6 + 8) >> 2 << 6);
            BuildBlockUpload(decoder->uploads[i].packet, decoder->pictures[i], blocksWide, blocksHigh, halfPixels);
        }
        else
        {
            decoder->uploads[i].packet = ArenaAllocate(0x40, (blocksWide * 6 + 8) >> 2 << 6);
            BuildColumnUpload(decoder->uploads[i].packet, decoder->pictures[i], blocksWide, blocksHigh, halfPixels);
        }

        // The retail code described the picture for a loader of images here, which was a stub returning 0
        if (outputFormat == 0x101 || outputFormat == 0x103)
        {
            decoder->uploadIds[i] = 0;
        }
    }

    sceIpuStopDMA(&decoder->ipuDma);
    return 1;
}
}

void MovieDecoding::Initialise()
{
    sceIpuInit();
    sceMpegInit();
    SetChannelSettings(IpuFromChannel, 0, 0, 0, 0, 0, 0);
    SetChannelSettings(IpuToChannel, 1, 0, 0, 0, 0, 0);
}

void MovieDecoding::SetArena(u8* begin, u8*)
{
    g_ArenaNext = begin;
}

s32 MovieDecoding::Open(MovieDecoder* decoder, u32 mode, u32 format, u32 audio, u32 audioChannel, s32 (*seek)(const char* path),
                        s32 (*read)(u8* buffer, u32 size, const char* path), const char* path, u32 doubleBuffered,
                        u32 widthBlocks, u32 outputFormat)
{
    return Start(true, decoder, mode, format, audio, audioChannel, seek, read, path, doubleBuffered, widthBlocks, outputFormat);
}

s32 MovieDecoding::DecodeAndUpload(u8** upload, u32 frameBase)
{
    if (DecodePicture() == 0)
    {
        return 0;
    }

    MovieDecoder* decoder = g_Decoder;
    u32 buffer = ((decoder->decoded - 1) & 1) != 0 ? decoder->doubleBuffered != 0 : 0;
    *upload = decoder->uploads[buffer].packet;
    decoder = g_Decoder;
    SetUploadDestination(decoder->uploads[buffer].packet, frameBase, decoder->widthBlocks, decoder->outputFormat == 0x102 ? 2 : 0);
    return 1;
}

void MovieDecoding::FeedAudio(u32 (*consume)(u8* ring, u32 size, u32 read, u32 count, u32 user), u32 user)
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->audio == 0)
    {
        return;
    }

    u32 used = consume(decoder->audioBase, decoder->audioSize, decoder->audioRead, decoder->audioCount, user);
    decoder = g_Decoder;
    decoder->audioCount -= used;
    decoder->audioRead = (decoder->audioRead + static_cast<s32>(used)) % decoder->audioSize;
}

bool MovieDecoding::Finished()
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->ended != 0)
    {
        return true;
    }

    if (decoder->format < 2)
    {
        return decoder->decoded >= decoder->pictureCount;
    }

    if (decoder->format == 2)
    {
        return sceMpegIsEnd(&decoder->mpeg) != 0;
    }

    return false;
}

void MovieDecoding::ReadOn()
{
    Demultiplex(false);
}

void MovieDecoding::Close()
{
    if (g_Decoder->format == 2)
    {
        sceMpegDelete(&g_Decoder->mpeg);
    }
}

// A chain from the tag: the channel's settings in chain mode
void MovieDecoding::StartChainDma(u32 channel, const void* tag)
{
    *ChannelRegister(channel, 0x30) = DmaAddress(tag);
    *ChannelRegister(channel, 0x20) = 0;
    *R_EE_D_STAT = 1u << channel;
    *ChannelRegister(channel, 0) = (g_ChannelSettings[channel] & ~0xCu) | 0x104;
    asm volatile("sync" : : : "memory");
}
