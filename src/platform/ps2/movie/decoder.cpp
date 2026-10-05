#include "mpeg.h"

#include "retail/libc.h"

#include <bit>
#include <ee_regs.h>
#include <kernel.h>
#include <libgs.h>

// The decoder of Sony's movie samples the game built its player on: libmpeg for PSS files, the IPU's own command stream for its
// formats
namespace
{
// The DMA channels' registers by number (the retail CHCR_ARRAY: 5 to 7 are the SIF's, which it leaves out)
constexpr u32 ChannelRegisters[DmaChannels] = {A_EE_D0_CHCR, A_EE_D1_CHCR, A_EE_D2_CHCR, A_EE_D3_CHCR, A_EE_D4_CHCR, 0, 0, 0,
                                               A_EE_D8_CHCR, A_EE_D9_CHCR};

// D_SQWC: the quadwords an interleaved transfer skips (SQWC) and sends (TQWC) in turns
union SkipQuadwords
{
    u32 value;
    struct
    {
        u32 skip : 8;
        u32 unused8 : 8;
        u32 transfer : 8;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(SkipQuadwords, 4);

// The arena's and the decoder's buffers' alignment
constexpr u32 BufferAlignment = 0x40;
// The read buffer: the sectors read at once
constexpr u32 ReadBufferSize = 0x4000;
// The video ring, its first bytes the IPU's to start with
constexpr s32 VideoRingSize = 0x40000;
constexpr u32 VideoRingStart = 0x40;
// The sound's ring: PCM and data, ADPCM
constexpr s32 PcmRingSize = 0x19000;
constexpr s32 AdpcmRingSize = 0x8000;
// The blocks of the sound a full ring keeps whole: PCM's, the data's (ADPCM's are its header's interleave times its channels)
constexpr s32 PcmBlockBytes = 0x400;
constexpr s32 DataBlockBytes = 0x200;
// The IPU's input gets up to that many bytes of the ring at once, in quadwords
constexpr s32 IpuFeedBytes = 0x2000;
// The polls of the IPU waiting for data that WaitForIpu makes before feeding it
constexpr s32 FeedPolls = 1001;
// sceMpegCreate's work memory: the state, and the reference pictures' (three frames of 1.5 bytes a pixel) for MPEG-2
constexpr u32 MpegStateWorkSize = 0x2800;

// The IPU stream file's header
struct IpuFileHeader
{
    u32 id;
    u32 size;
    u16 width;
    u16 height;
    s32 pictureCount;
};
CHECK_SIZE(IpuFileHeader, 0x10);

// The first bytes of an IPU stream's picture: IPU_CTRL's picture settings (its bits 16 to 23) but for DTD, IDEC's reading of the
// DCT types, in place of a bit IPU_CTRL doesn't use
union IpuPictureFlags
{
    u8 value;
    struct
    {
        u8 dcPrecision : 2;
        u8 decodesDctType : 1;
        u8 unused3 : 1;
        u8 alternateScan : 1;
        u8 intraVlcFormat : 1;
        u8 quantiserScaleType : 1;
        u8 mpeg1 : 1;
    };
};
CHECK_SIZE(IpuPictureFlags, 1);

// An IPU stream's picture: its flags' byte, its macroblocks, the 32 bits of the start code that ends them
constexpr u32 PictureFlagsBits = 8;
constexpr u32 PictureEndBits = 32;
constexpr u32 StartCodeBytes = 4;

// A macroblock of 16 by 16 pixels, 32 bit or 16 bit
constexpr u32 MacroblockShift = 4;
constexpr u32 Rgb32MacroblockBytes = Rgb32MacroblockQuadwords * QuadwordBytes;
constexpr u32 Rgb16MacroblockBytes = Rgb16MacroblockQuadwords * QuadwordBytes;

MovieDecoder* g_Decoder;
// The arena's next byte (nothing checks its end)
u8* g_ArenaNext;
// The DMA channels' CHCR settings (the retail D_003C6EC0)
DmaChannelControl g_ChannelSettings[DmaChannels];

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

// The DMA controller's address of the memory (Sony's DmaAddr): the uncached segments' bits cleared, and the scratchpad's address
// (its bit 30 left then) moved to bit 31, the DMA's scratchpad bit
u32 DmaAddress(const void* memory)
{
    constexpr u32 ScratchpadBit = Scratchpad & ~UncachedAcceleratedSegment;
    u32 address = reinterpret_cast<u32>(memory) & ~UncachedAcceleratedSegment;
    if ((address & ~(PhysicalMask | UncachedAcceleratedSegment)) != 0)
    {
        address += ScratchpadBit;
    }

    return address;
}

// Through the uncached segment: what the DMA reads has to be in memory. The video ring's writes take the address's segment off
// first, the sound ring's and the upload chains' don't
u8* Uncached(u8* memory)
{
    return reinterpret_cast<u8*>((reinterpret_cast<u32>(memory) & PhysicalMask) | UncachedSegment);
}

u8* WithUncachedBit(u8* memory)
{
    return reinterpret_cast<u8*>(reinterpret_cast<u32>(memory) | UncachedSegment);
}

void SetChannelSettings(u32 channel, u32 fromMemory, u32 tagTransfer, u32 tagInterrupt, u32 interleave, u32 skipQuadwords,
                        u32 transferQuadwords)
{
    DmaChannelControl settings = {};
    settings.fromMemory = fromMemory != 0;
    settings.sendsTags = tagTransfer != 0;
    settings.tagInterrupts = tagInterrupt != 0;
    if (interleave != 0)
    {
        settings.mode = DmaInterleaveMode;
    }

    SkipQuadwords quadwords = {};
    quadwords.skip = skipQuadwords;
    quadwords.transfer = transferQuadwords;
    *R_EE_D_SQWC = quadwords.value;
    g_ChannelSettings[channel] = settings;
}

void StartNormalDma(u32 channel, const void* memory, u32 quadwords)
{
    *ChannelRegister(channel, MadrOffset) = DmaAddress(memory);
    *ChannelRegister(channel, QwcOffset) = quadwords;
    // The channel's interrupt status cleared
    *R_EE_D_STAT = 1u << channel;
    DmaChannelControl control = g_ChannelSettings[channel];
    control.started = 1;
    *ChannelRegister(channel, ChcrOffset) = control.value;
    asm volatile("sync" : : : "memory");
}

// The IPU's channels stopped (the DMA controller held meanwhile)
void StopIpuChannels(bool both)
{
    DI();
    *R_EE_D_ENABLEW = DmaSuspend;
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
    sceIpuSync(IpuSyncWait, 0);
    *IpuCommand = IpuClearInput;
    sceIpuSync(IpuSyncWait, 0);
}

s32 Demultiplex(bool wait);

// While the IPU works the decoder reads on
void WhileIpuBusy()
{
    Demultiplex(false);
}

// libmpeg's callback when the IPU needs data, and the IPU's waits': up to IpuFeedBytes of the video ring go to the IPU's input
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

    while (pending < IpuFeedBytes)
    {
        if (Demultiplex(true) == 0)
        {
            // What's left goes, rounded up to a quadword
            decoder = g_Decoder;
            pending += QuadwordBytes - 1;
            if (decoder->ended != 0)
            {
                if (decoder->format == MovieMpeg2)
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
    u32 size = static_cast<u32>(pending) & ~(QuadwordBytes - 1u);
    if (size > IpuFeedBytes)
    {
        size = IpuFeedBytes;
    }

    u8* from = decoder->videoRead;
    u8* end = decoder->videoBase + decoder->videoSize;
    if (end < from + size)
    {
        size = end - from;
    }

    StartNormalDma(IpuToChannel, from, size / QuadwordBytes);
    decoder = g_Decoder;
    u8* next = decoder->videoRead + size;
    decoder->videoNext = next;
    if (!(next < decoder->videoBase + decoder->videoSize))
    {
        decoder->videoNext = next - decoder->videoSize;
    }

    return 1;
}

// Waits until the IPU is done with its command (or found an error), feeding it when it waits for data no DMA brings. With output,
// until its output channel is done
void WaitForIpu(bool output)
{
    if (g_Decoder->ended != 0)
    {
        return;
    }

    while (true)
    {
        if (IpuControlRegister{*IpuControl}.errorFound)
        {
            return;
        }

        if (!IpuControlRegister{*IpuControl}.busy)
        {
            return;
        }

        if ((*R_EE_D4_CHCR & ChcrStart) == 0)
        {
            for (s32 tries = 0;; tries++)
            {
                if (!IpuControlRegister{*IpuControl}.busy)
                {
                    break;
                }

                if (tries >= FeedPolls)
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
    MpegStreamData* packet = static_cast<MpegStreamData*>(streamData);
    MovieDecoder* decoder = g_Decoder;
    if (decoder->format < MovieMpeg2 && decoder->container == MoviePss)
    {
        // The IPU stream's packets start with their sub-stream's bytes
        packet->data += SubStreamBytes;
        packet->length -= SubStreamBytes;
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

    u32 length = packet->length;
    if (static_cast<u32>(room) < length)
    {
        return 0;
    }

    s32 over = (decoder->videoWrite - decoder->videoBase) + static_cast<s32>(length) - decoder->videoSize;
    if (over > 0)
    {
        RetailLibc::MemoryCopy(Uncached(decoder->videoWrite), packet->data, length - over);
        RetailLibc::MemoryCopy(Uncached(g_Decoder->videoBase), packet->data + packet->length - over, over);
        g_Decoder->videoWrite = g_Decoder->videoBase + over;
    }
    else
    {
        RetailLibc::MemoryCopy(Uncached(decoder->videoWrite), packet->data, length);
        g_Decoder->videoWrite += packet->length;
    }

    return 1;
}

// libmpeg's callback for the sound: the header goes aside, the rest into the sound's ring. Returns 0 (and marks the ring full) when
// there isn't room
s32 StoreAudio(Mpeg*, void* streamData, void*)
{
    MpegStreamData* packet = static_cast<MpegStreamData*>(streamData);
    MovieDecoder* decoder = g_Decoder;
    packet->data += SubStreamBytes;
    packet->length -= SubStreamBytes;
    if (decoder->audio != MovieAudioData && decoder->audioHeaderBytes < static_cast<s32>(sizeof(MovieAudioHeader)))
    {
        u8* header = reinterpret_cast<u8*>(&decoder->audioHeader) + decoder->audioHeaderBytes;
        u32 missing = sizeof(MovieAudioHeader) - decoder->audioHeaderBytes;
        if (missing < packet->length)
        {
            RetailLibc::MemoryCopy(header, packet->data, missing);
            decoder = g_Decoder;
            packet->data = packet->data + sizeof(MovieAudioHeader) - decoder->audioHeaderBytes;
            packet->length = packet->length - sizeof(MovieAudioHeader) + decoder->audioHeaderBytes;
            decoder->audioHeaderBytes = sizeof(MovieAudioHeader);
        }
        else
        {
            RetailLibc::MemoryCopy(header, packet->data, packet->length);
            decoder = g_Decoder;
            decoder->audioHeaderBytes += packet->length;
            packet->length = 0;
        }
    }

    decoder = g_Decoder;
    u32 length = packet->length;
    if (static_cast<u32>(decoder->audioSize) < decoder->audioCount + length)
    {
        decoder->audioFull = 1;
        return 0;
    }

    s32 at = (decoder->audioRead + decoder->audioCount) % decoder->audioSize;
    s32 over = at + static_cast<s32>(length) - decoder->audioSize;
    if (over > 0)
    {
        RetailLibc::MemoryCopy(WithUncachedBit(decoder->audioBase + at), packet->data, length - over);
        RetailLibc::MemoryCopy(WithUncachedBit(g_Decoder->audioBase), packet->data + packet->length - over, over);
    }
    else
    {
        RetailLibc::MemoryCopy(WithUncachedBit(decoder->audioBase + at), packet->data, length);
    }

    g_Decoder->audioCount += packet->length;
    return 1;
}

// Reads the file's next sectors and demultiplexes them (a raw stream all goes to the video ring), waiting for the disc unless not
// told to. Returns the bytes used, 0 when none (the read buffer keeps what's left)
s32 Demultiplex(bool wait)
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->container == MovieRawStream)
    {
        if (decoder->pending == 0)
        {
            while (true)
            {
                s32 read = decoder->read(decoder->readBuffer, ReadBufferSize, decoder->path);
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
        MpegStreamData packet;
        packet.length = decoder->pending;
        packet.data = decoder->readBuffer;
        s32 used = StoreVideo(&decoder->mpeg, &packet, nullptr) != 0 ? packet.length : 0;
        g_Decoder->pending -= used;
        return used;
    }

    // What's left of the read buffer is at its end
    u8* sectors;
    if (decoder->pending != 0)
    {
        sectors = decoder->readBuffer - (decoder->pending - static_cast<s32>(ReadBufferSize));
    }
    else
    {
        while (true)
        {
            s32 read = decoder->read(decoder->readBuffer, ReadBufferSize, decoder->path);
            decoder = g_Decoder;
            decoder->pending = read;
            if (read < 0)
            {
                decoder->ended = 1;
                return 0;
            }

            if (read != 0)
            {
                sectors = decoder->readBuffer;
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
        s32 used = sceMpegDemuxPss(&decoder->mpeg, sectors, decoder->pending);
        if (used != 0 || !wait || g_Decoder->audioFull == 0)
        {
            g_Decoder->pending -= used;
            return used;
        }

        // The sound's ring was full: it keeps what isn't a whole block of it and the sectors are demultiplexed again
        decoder = g_Decoder;
        switch (decoder->audio)
        {
        case MoviePcm:
            decoder->audioCount %= PcmBlockBytes;
            break;
        case MovieAdpcm:
            decoder->audioCount %= decoder->audioHeader.interleave * decoder->audioHeader.channels;
            break;
        case MovieAudioData:
            decoder->audioCount %= DataBlockBytes;
            break;
        default:
            break;
        }
    }
}

s32 WaitIpuInBackground(Mpeg*, void*, void*)
{
    while (sceIpuSync(IpuSyncPoll, 0) != 0)
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
    if (decoder->format == MovieMpeg2)
    {
        sceMpegAddStrCallback(&decoder->mpeg, MpegVideoStream, 0, StoreVideo, nullptr);
    }
    else
    {
        sceMpegAddStrCallback(&decoder->mpeg, MpegIpuStream, 0, StoreVideo, nullptr);
    }

    decoder = g_Decoder;
    switch (decoder->audio)
    {
    case MoviePcm:
        sceMpegAddStrCallback(&decoder->mpeg, MpegPcmStream, channel, StoreAudio, nullptr);
        break;
    case MovieAdpcm:
        sceMpegAddStrCallback(&decoder->mpeg, MpegAdpcmStream, channel, StoreAudio, nullptr);
        break;
    case MovieAudioData:
        sceMpegAddStrCallback(&decoder->mpeg, MpegDataStream, channel, StoreAudio, nullptr);
        break;
    default:
        break;
    }
}

// The tags and GS registers of the upload chains, written through the uncached segment the way the retail code wrote them: a tag's
// count by halfword and its top byte (its ID, PCE and IRQ), its other bytes as they were
constexpr u32 TagTopByteShift = 24;

void WriteTag(volatile u8* at, u16 quadwords, u32 id)
{
    DmaTag tag = {};
    tag.id = id;
    *reinterpret_cast<volatile u16*>(at) = quadwords;
    at[3] = static_cast<u8>(tag.value >> TagTopByteShift);
}

// A REF tag's address, its second word
void WriteTagAddress(volatile u8* at, const u8* address)
{
    *reinterpret_cast<volatile u32*>(at + 4) = reinterpret_cast<u32>(address);
}

void WriteQuadword(volatile u8* at, u64 low, u64 high)
{
    reinterpret_cast<volatile u64*>(at)[0] = low;
    reinterpret_cast<volatile u64*>(at)[1] = high;
}

// A chain starts with a CNT of a GIF tag of 2 A+D writes, the upload's BITBLTBUF (first an address of no register, which the GS
// ignores) and TRXREG; a transfer is a CNT of a GIF tag of 2 A+D writes, TRXPOS, TRXDIR and a GIF tag of the image's quadwords,
// then a REF of them; a RET with nothing called ends the chain
constexpr u16 ChainHeadQuadwords = 3;
constexpr u16 TransferHeadQuadwords = 4;
constexpr u32 DestinationQuadword = 2;
const QWORD AddressData2Tag = std::bit_cast<QWORD>(GS_GIF_TAG{.nloop = 2, .nreg = 1, .reg = gif_rd_ad});
const u64 ImageTag = std::bit_cast<QWORD>(GS_GIF_TAG{.flg = GS_GIF_IMAGE}).lo;
const u64 EndOfPacket = std::bit_cast<QWORD>(GS_GIF_TAG{.eop = 1}).lo;
constexpr u64 GsNoRegister = 0x7F;
// TRXREG of a macroblock, and its width alone (a column's height ORed in); TRXDIR from the host to the GS's memory
const u64 MacroblockArea = std::bit_cast<u64>(GS_TRXREG{.trans_w = 16, .trans_h = 16});
const u64 MacroblockWidth = std::bit_cast<u64>(GS_TRXREG{.trans_w = 16});
const u64 HostToLocal = std::bit_cast<u64>(GS_TRXDIR{.trans_dir = 0});
// Where TRXREG's height, TRXPOS's destination and BITBLTBUF's destination base, width and format start
constexpr u32 TrxRegHeightShift = 32;
constexpr u32 TrxPosDestinationXShift = 32;
constexpr u32 TrxPosDestinationYShift = 48;
constexpr u32 BitBltDestinationBaseShift = 32;
constexpr u32 BitBltDestinationWidthShift = 48;
constexpr u32 BitBltDestinationFormatShift = 56;

// An upload chain's bytes: 6 quadwords a transfer and 8 more (its head and its end, and some), rounded down to 4 quadwords
u32 UploadBytes(u32 transfers)
{
    return (transfers * 6 + 8) / 4 * 4 * QuadwordBytes;
}

// The upload chain of a picture of macroblocks after each other: a transfer a macroblock
void BuildBlockUpload(u8* packet, u8* picture, u32 blocksWide, u32 blocksHigh, u32 pixelFormat)
{
    u32 quadwords = pixelFormat == GS_PIXMODE_32 ? Rgb32MacroblockQuadwords : Rgb16MacroblockQuadwords;
    SyncDCache(packet, packet + UploadBytes(blocksWide * blocksHigh) - 1);
    volatile u8* at = WithUncachedBit(packet);
    WriteTag(at, ChainHeadQuadwords, DMA_TAG_CNT);
    at += QuadwordBytes;
    WriteQuadword(at, AddressData2Tag.lo, AddressData2Tag.hi);
    at += QuadwordBytes;
    WriteQuadword(at, 0, GsNoRegister);
    at += QuadwordBytes;
    WriteQuadword(at, MacroblockArea, gs_g_trxreg);
    at += QuadwordBytes;
    for (u32 y = 0; y < blocksHigh; y++)
    {
        for (u32 x = 0; x < blocksWide; x++)
        {
            WriteTag(at, TransferHeadQuadwords, DMA_TAG_CNT);
            at += QuadwordBytes;
            WriteQuadword(at, AddressData2Tag.lo, AddressData2Tag.hi);
            at += QuadwordBytes;
            WriteQuadword(at,
                          static_cast<u64>(x << MacroblockShift) << TrxPosDestinationXShift |
                              static_cast<u64>(y << MacroblockShift) << TrxPosDestinationYShift,
                          gs_g_trxpos);
            at += QuadwordBytes;
            WriteQuadword(at, HostToLocal, gs_g_trxdir);
            at += QuadwordBytes;
            u64 last = x == blocksWide - 1 && y == blocksHigh - 1 ? EndOfPacket : 0;
            WriteQuadword(at, quadwords | last | ImageTag, 0);
            at += QuadwordBytes;
            WriteTagAddress(at, picture);
            WriteTag(at, quadwords, DMA_TAG_REF);
            picture += quadwords * QuadwordBytes;
            at += QuadwordBytes;
        }
    }

    WriteTag(at, 0, DMA_TAG_RET);
}

// The same of a picture of columns of macroblocks: a transfer a column
void BuildColumnUpload(u8* packet, u8* picture, u32 blocksWide, u32 blocksHigh, u32 pixelFormat)
{
    u32 quadwords = pixelFormat == GS_PIXMODE_32 ? blocksHigh * Rgb32MacroblockQuadwords : blocksHigh * Rgb16MacroblockQuadwords;
    SyncDCache(packet, packet + UploadBytes(blocksWide) - 1);
    volatile u8* at = WithUncachedBit(packet);
    WriteTag(at, ChainHeadQuadwords, DMA_TAG_CNT);
    at += QuadwordBytes;
    WriteQuadword(at, AddressData2Tag.lo, AddressData2Tag.hi);
    at += QuadwordBytes;
    WriteQuadword(at, 0, GsNoRegister);
    at += QuadwordBytes;
    WriteQuadword(at, static_cast<u64>(blocksHigh << MacroblockShift) << TrxRegHeightShift | MacroblockWidth, gs_g_trxreg);
    at += QuadwordBytes;
    for (u32 x = 0; x < blocksWide; x++)
    {
        WriteTag(at, TransferHeadQuadwords, DMA_TAG_CNT);
        at += QuadwordBytes;
        WriteQuadword(at, AddressData2Tag.lo, AddressData2Tag.hi);
        at += QuadwordBytes;
        WriteQuadword(at, static_cast<u64>(x << MacroblockShift) << TrxPosDestinationXShift, gs_g_trxpos);
        at += QuadwordBytes;
        WriteQuadword(at, HostToLocal, gs_g_trxdir);
        at += QuadwordBytes;
        u64 last = x == blocksWide - 1 ? EndOfPacket : 0;
        WriteQuadword(at, quadwords | last | ImageTag, 0);
        at += QuadwordBytes;
        WriteTagAddress(at, picture);
        WriteTag(at, quadwords, DMA_TAG_REF);
        picture += quadwords * QuadwordBytes;
        at += QuadwordBytes;
    }

    WriteTag(at, 0, DMA_TAG_RET);
}

// The upload's BITBLTBUF, in place of the address of no register: where in the GS's memory and its width and format
void SetUploadDestination(u8* packet, u32 base, u32 widthBlocks, u32 pixelFormat)
{
    volatile u64* registers = reinterpret_cast<volatile u64*>(WithUncachedBit(packet)) + DestinationQuadword * 2;
    registers[1] = gs_g_bitbltbuf;
    registers[0] = static_cast<u64>(base) << BitBltDestinationBaseShift |
                   static_cast<u64>(widthBlocks) << BitBltDestinationWidthShift |
                   static_cast<u64>(pixelFormat) << BitBltDestinationFormatShift;
}

// 16 bit pixels (the IPU converts libmpeg's macroblocks itself)
bool Rgb16Output(MovieOutputFormat format)
{
    return format - MovieRgb16 < MovieOutputFormatsEnd - MovieRgb16;
}

// Decodes the next picture into its buffer. Returns whether there was one
s32 DecodePicture()
{
    MovieDecoder* decoder = g_Decoder;
    u32 buffer = (decoder->decoded & 1) != 0 ? decoder->doubleBuffered != 0 : 0;
    u32 macroblocks = (decoder->width >> MacroblockShift) * (decoder->height >> MacroblockShift);
    sceIpuRestartDMA(&decoder->ipuDma);
    decoder = g_Decoder;
    if (decoder->format < MovieMpeg2)
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
    if (decoder->format == MovieMpeg2)
    {
        if (sceMpegIsEnd(&decoder->mpeg) != 0)
        {
            return 0;
        }

        decoder = g_Decoder;
        if (Rgb16Output(decoder->outputFormat))
        {
            if (sceMpegGetPictureRAW8(&decoder->mpeg, decoder->raw8, macroblocks) < 0)
            {
                return 0;
            }

            // The macroblocks converted to 16 bit pixels by the IPU, its DMA put back after
            IpuDmaEnvironment saved;
            sceIpuStopDMA(&saved);
            *R_EE_D4_QWC = macroblocks * MacroblockQuadwords;
            *R_EE_D4_MADR = reinterpret_cast<u32>(g_Decoder->raw8);
            *R_EE_D4_CHCR = ChcrStart | ChcrFromMemory;
            *R_EE_D3_QWC = macroblocks * Rgb16MacroblockQuadwords;
            *R_EE_D3_MADR = reinterpret_cast<u32>(g_Decoder->pictures[buffer]);
            *R_EE_D3_CHCR = ChcrStart;
            *IpuCommand = IpuClearInput;
            sceIpuSync(IpuSyncWait, 0);
            *IpuCommand = macroblocks | IpuConvert | IpuConvertRgb16 | IpuConvertDither;
            while (sceIpuSync(IpuSyncPoll, 0) != 0)
            {
                WhileIpuBusy();
            }

            sceIpuRestartDMA(&saved);
        }
        else if (sceMpegGetPicture(&decoder->mpeg, decoder->pictures[buffer],
                                   static_cast<s32>(decoder->width * decoder->height) >> (2 * MacroblockShift)) < 0)
        {
            return 0;
        }
    }
    else
    {
        u8* picture = decoder->pictures[buffer];
        MovieOutputFormat outputFormat = decoder->outputFormat;
        u32 pixelBits = 0;
        if (outputFormat >= MovieRgb32)
        {
            if (outputFormat < MovieRgb16)
            {
                pixelBits = 32;
            }
            else
            {
                pixelBits = outputFormat > MovieRgb16Image ? 0 : 16;
            }
        }

        // The picture's header: its flags in the first byte, then intra decoding of every macroblock
        *IpuCommand = IpuDecodeFixed;
        WaitForIpu(false);
        if (g_Decoder->ended != 0)
        {
            goto failed;
        }

        IpuPictureFlags flags = {static_cast<u8>(*IpuCommand >> 24)};
        *IpuCommand = IpuDecodeFixed | PictureFlagsBits;
        WaitForIpu(false);
        if (g_Decoder->ended != 0)
        {
            goto failed;
        }

        IpuPictureFlags settings = flags;
        settings.decodesDctType = 0;
        *IpuControl = u32{settings.value} << IpuControlDcPrecisionShift;
        IpuIntraDecodeCommand decode = {IpuIntraDecode};
        decode.quantiserScale = 1;
        decode.decodesDctType = flags.decodesDctType;
        decode.dither = 1;
        decode.rgb16 = pixelBits == 16;
        *IpuCommand = decode.value;
        for (u32 left = macroblocks; left != 0;)
        {
            u32 count = left <= ChunkMacroblocks ? left : ChunkMacroblocks;
            StartNormalDma(IpuFromChannel, picture,
                           pixelBits == 32 ? count * Rgb32MacroblockQuadwords : count * Rgb16MacroblockQuadwords);
            WaitForIpu(true);
            if (g_Decoder->ended != 0)
            {
                goto failed;
            }

            left -= count;
            // Retail bug: by 32 bit pixels' size, 16 bit ones too (pictures of more than 1023 macroblocks get gaps)
            picture += count * Rgb32MacroblockBytes;
        }

        *IpuCommand = IpuDecodeFixed | PictureEndBits;
        WaitForIpu(false);
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
s32 Start(bool create, MovieDecoder* decoder, MovieContainer container, MovieFormat format, MovieAudioStream audio,
          u32 audioChannel, s32 (*seek)(const char* path), s32 (*read)(u8* buffer, u32 size, const char* path), const char* path,
          u32 doubleBuffered, u32 widthBlocks, MovieOutputFormat outputFormat)
{
    // The first sceMpegCreate's work memory
    alignas(16) u8 work[MpegStateWorkSize];
    u32 pixelBits = 0;
    g_Decoder = decoder;
    StopIpuChannels(true);
    decoder->ended = 0;
    if (create)
    {
        FlushCache(0);
        decoder->readBuffer = ArenaAllocate(BufferAlignment, ReadBufferSize);
        if (decoder->readBuffer == nullptr)
        {
            return 0;
        }

        decoder->videoBase = ArenaAllocate(BufferAlignment, VideoRingSize);
        if (decoder->videoBase == nullptr)
        {
            return 0;
        }

        decoder->read = read;
        decoder->seek = seek;
        decoder->audioChannel = audioChannel;
        decoder->videoSize = VideoRingSize;
        decoder->widthBlocks = widthBlocks;
        decoder->container = container;
        decoder->path = path;
        decoder->format = format;
        decoder->audio = audio;
        decoder->outputFormat = outputFormat;
        decoder->doubleBuffered = doubleBuffered;
    }

    // The ring starts with VideoRingStart bytes going to the IPU
    decoder->pending = 0;
    decoder->videoRead = decoder->videoBase;
    decoder->videoNext = decoder->videoBase + VideoRingStart;
    decoder->videoWrite = decoder->videoBase + VideoRingStart;
    if (audio != MovieNoAudio)
    {
        if (create)
        {
            switch (decoder->audio)
            {
            case MoviePcm:
            case MovieAudioData:
                decoder->audioSize = PcmRingSize;
                break;
            case MovieAdpcm:
                decoder->audioSize = AdpcmRingSize;
                break;
            default:
                break;
            }

            decoder->audioBase = ArenaAllocate(BufferAlignment, decoder->audioSize);
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
    if (create && container == MoviePss)
    {
        sceMpegCreate(&decoder->mpeg, work, sizeof(work));
        AddStreamCallbacks(decoder->audioChannel);
    }

    if (Demultiplex(true) == 0)
    {
        return 0;
    }

    // The picture's size: the IPU file's header, or the MPEG sequence header's horizontal_size_value and vertical_size_value (12
    // bits each after its start code)
    bool ipuFormat = format < MovieMpeg2;
    if (ipuFormat)
    {
        if (create)
        {
            IpuFileHeader* header = reinterpret_cast<IpuFileHeader*>(decoder->videoNext);
            decoder->width = header->width;
            decoder->height = header->height;
            decoder->pictureCount = header->pictureCount;
        }

        decoder->videoNext += sizeof(IpuFileHeader);
    }
    else if (create)
    {
        u8* sizes = decoder->videoNext + StartCodeBytes;
        decoder->width = sizes[0] << 4 | sizes[1] >> 4;
        decoder->height = (sizes[1] & 0xF) << 8 | sizes[2];
    }

    if (format == MovieMpeg2 || container == MoviePss)
    {
        if (create)
        {
            u32 size = ipuFormat ? MpegStateWorkSize : (decoder->width * decoder->height * 9 >> 1) + MpegStateWorkSize;
            decoder->work = ArenaAllocate(BufferAlignment, size);
            if (decoder->work == nullptr)
            {
                return 0;
            }

            sceMpegCreate(&decoder->mpeg, decoder->work, size);
            if (format == MovieMpeg2)
            {
                sceMpegAddCallback(&decoder->mpeg, MpegCallbackNoData, FeedIpu, nullptr);
                sceMpegAddCallback(&decoder->mpeg, MpegCallbackBackground, WaitIpuInBackground, decoder);
            }

            sceMpegAddCallback(&decoder->mpeg, MpegCallbackError, IgnoreError, nullptr);
            if (container == MoviePss)
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
        if (outputFormat < MovieRgb32)
        {
            outputFormat = MovieRgb32;
            pixelBits = 32;
        }
        else if (outputFormat < MovieRgb16)
        {
            pixelBits = 32;
        }
        else if (outputFormat < MovieOutputFormatsEnd)
        {
            pixelBits = 16;
        }
        else
        {
            outputFormat = MovieRgb32;
            pixelBits = 32;
        }

        if (Rgb16Output(decoder->outputFormat) && decoder->format == MovieMpeg2)
        {
            u32 macroblocks = (decoder->width >> MacroblockShift) * (decoder->height >> MacroblockShift);
            decoder->raw8 = ArenaAllocate(BufferAlignment, macroblocks * MacroblockBytes);
            if (decoder->raw8 == nullptr)
            {
                return 0;
            }
        }
    }

    u32 pixelFormat = pixelBits == 16 ? GS_PIXMODE_16 : GS_PIXMODE_32;
    for (s32 i = 0; create && (doubleBuffered != 0 ? i < 2 : i <= 0); i++)
    {
        u32 blocksWide = decoder->width >> MacroblockShift;
        u32 blocksHigh = decoder->height >> MacroblockShift;
        u32 macroblocks = blocksWide * blocksHigh;
        u32 pictureBytes = pixelBits == 16 ? macroblocks * Rgb16MacroblockBytes : macroblocks * Rgb32MacroblockBytes;
        decoder->pictures[i] = ArenaAllocate(BufferAlignment, pictureBytes);
        if (decoder->pictures[i] == nullptr)
        {
            return 0;
        }

        if (decoder->format == MovieIpuRows)
        {
            decoder->uploads[i].packet = ArenaAllocate(BufferAlignment, UploadBytes(macroblocks));
            BuildBlockUpload(decoder->uploads[i].packet, decoder->pictures[i], blocksWide, blocksHigh, pixelFormat);
        }
        else
        {
            decoder->uploads[i].packet = ArenaAllocate(BufferAlignment, UploadBytes(blocksWide));
            BuildColumnUpload(decoder->uploads[i].packet, decoder->pictures[i], blocksWide, blocksHigh, pixelFormat);
        }

        // The retail code described the picture for a loader of images here, which was a stub returning 0
        if (outputFormat == MovieRgb32Image || outputFormat == MovieRgb16Image)
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

s32 MovieDecoding::Open(MovieDecoder* decoder, MovieContainer container, MovieFormat format, MovieAudioStream audio,
                        u32 audioChannel, s32 (*seek)(const char* path), s32 (*read)(u8* buffer, u32 size, const char* path),
                        const char* path, u32 doubleBuffered, u32 widthBlocks, MovieOutputFormat outputFormat)
{
    return Start(true, decoder, container, format, audio, audioChannel, seek, read, path, doubleBuffered, widthBlocks,
                 outputFormat);
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
    SetUploadDestination(decoder->uploads[buffer].packet, frameBase, decoder->widthBlocks,
                         decoder->outputFormat == MovieRgb16 ? GS_PIXMODE_16 : GS_PIXMODE_32);
    return 1;
}

void MovieDecoding::FeedAudio(u32 (*consume)(u8* ring, u32 size, u32 read, u32 count, u32 user), u32 user)
{
    MovieDecoder* decoder = g_Decoder;
    if (decoder->audio == MovieNoAudio)
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

    if (decoder->format < MovieMpeg2)
    {
        return decoder->decoded >= decoder->pictureCount;
    }

    if (decoder->format == MovieMpeg2)
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
    if (g_Decoder->format == MovieMpeg2)
    {
        sceMpegDelete(&g_Decoder->mpeg);
    }
}

// A chain from the tag: the channel's settings in chain mode
void MovieDecoding::StartChainDma(u32 channel, const void* tag)
{
    *ChannelRegister(channel, TadrOffset) = DmaAddress(tag);
    *ChannelRegister(channel, QwcOffset) = 0;
    *R_EE_D_STAT = 1u << channel;
    DmaChannelControl control = g_ChannelSettings[channel];
    control.mode = DmaChainMode;
    control.started = 1;
    *ChannelRegister(channel, ChcrOffset) = control.value;
    asm volatile("sync" : : : "memory");
}
