#include "mpeg.h"

#include <stdio.h>

// libmpeg's output of the decoded pictures: their times and flags into the Mpeg, their pixels into the caller's buffer

extern "C"
{
    extern const char g_MpegOutputTooSmall[] RETAIL(D_00307528);
    extern const char g_MpegSecondFieldMissing[] RETAIL(D_00307558);
    extern const char g_MpegConvertError[] RETAIL(D_00307578);
    // The fields a frame is shown for by its timing flags (repeat_first_field, top_field_first, progressive_frame,
    // progressive_sequence from bit 0): 0 for the combinations MPEG-2 doesn't allow
    extern const u32 g_MpegFieldsShown[MpegFlagsTiming + 1] RETAIL(D_002E8158);
}

namespace
{
// The IPU converts a picture of more than ChunkMacroblocks in chunks of them
constexpr u32 ChunkQuadwords = ChunkMacroblocks * Rgb32MacroblockQuadwords;
constexpr u32 ChunkBytes = ChunkQuadwords << 4;
// The toIPU channel sends 0xFFFF quadwords at a time
constexpr u32 SendQuadwords = 0xFFFF;

// ConvertInChunks's state, which the fromIPU channel's interrupt handler moves on
struct ConvertChunks
{
    volatile s32 converted;
    volatile s32 error;
    s32 macroblocks;
    u32 next;
    s32 chunks;
};

// The picture still to send to the IPU, which the toIPU channel's interrupt handler moves on
struct SendChunks
{
    u32 quadwords;
    u32 address;
};

u64 SignExtended(s32 value)
{
    return static_cast<u64>(static_cast<s64>(value));
}

// sceMpeg's flags of a picture (the values ORed in unmasked)
u64 FlagsOf(MpegImage* image)
{
    return SignExtended(image->progressiveSequence) << MpegFlagsProgressiveSequenceShift |
           SignExtended(image->pictureCodingType) << MpegFlagsCodingTypeShift |
           SignExtended(image->repeatFirstField) << MpegFlagsRepeatFirstFieldShift |
           SignExtended(image->topFieldFirst) << MpegFlagsTopFieldFirstShift |
           SignExtended(image->progressiveFrame) << MpegFlagsProgressiveFrameShift |
           SignExtended(image->pictureStructure) << MpegFlagsStructureShift;
}

// A picture without a PTS of its own gets the last one and its fields' time when the state counts them (the half fields of
// an odd step rounded up every other time). Always inlined: the second field's step (1 field) is no multiplication, as in
// retail
inline __attribute__((always_inline)) void CountPts(MpegSystem* sys, s64* pts, MpegImage* image, s32 fields)
{
    if (image->pts >= 0 || sys->lastPts < 0)
    {
        *pts = image->pts;
        return;
    }

    s32 roundUp = fields & sys->ptsRounding & static_cast<s32>(sys->ptsStep) & 1;
    s32 step = static_cast<s32>((sys->ptsStep * fields) >> 1);
    *pts = static_cast<s32>(static_cast<u32>(sys->lastPts) + static_cast<u32>(step + roundUp));
    if ((fields & sys->ptsStep & 1) != 0)
    {
        sys->ptsRounding++;
    }
}

void TakePendingPts(MpegSystem* sys, s64* pts)
{
    if (sys->pendingPtsState == MpegPtsForNextPicture && sys->pendingPts >= 0)
    {
        *pts = sys->pendingPts;
        sys->pendingPtsState = MpegNoPendingPts;
        sys->pendingPts = -1;
    }
}

// Whether the output takes the picture: by its size in pixels when the caller gave one, else in macroblocks
bool OutputTakes(MpegSystem* sys, MpegImage* image)
{
    if (sys->outputHeight != 0)
    {
        return sys->outputWidth >= image->width && sys->outputHeight >= image->height;
    }

    return sys->outputMacroblocks >= image->widthMacroblocks * image->heightMacroblocks;
}

void ReportOutputTooSmall(MpegSystem* sys, MpegImage* image)
{
    char message[0x100];
    snprintf(message, sizeof(message), g_MpegOutputTooSmall, image->width, image->height);
    Libmpeg::Error(sys, message);
}

void PutOut(MpegSystem* sys, MpegImage* image)
{
    if (sys->outputRgb32 != 0)
    {
        Libmpeg::ConvertImage(sys, image);
    }
    else
    {
        Libmpeg::CopyMacroblocks(sys, image);
    }

    if (sys->outputState != MpegPicturePutOut)
    {
        sys->outputState = MpegPicturePutOut;
        sys->firstOutputPicture = sys->pictureNumber;
    }

    sys->pictureOutput = 1;
}

// A transfer of one of the scratchpad's channels, started with interrupts off
void StartScratchpadDma(volatile u32* chcr, volatile u32* madr, volatile u32* qwc, volatile u32* sadr, u32 address,
                        u32 quadwords, u32 start)
{
    s32 interrupts = DIntr();
    *sadr = 0;
    *madr = address;
    *qwc = quadwords;
    *chcr = start;
    if (interrupts != 0)
    {
        EIntr();
    }
}

// All of a picture converted with one IPU command
void ConvertWhole(MpegSystem* sys, s32 macroblocks)
{
    u8* output = sys->output;
    WaitIpu();
    s32 interrupts = DIntr();
    *R_EE_D3_MADR = reinterpret_cast<u32>(output) & PhysicalMask;
    *R_EE_D3_QWC = macroblocks * Rgb32MacroblockQuadwords;
    *R_EE_D3_CHCR = ChcrStart;
    if (interrupts != 0)
    {
        EIntr();
    }

    SetIpuCommand(sys, macroblocks | IpuConvert);
    MpegCallbackData background = {MpegCallbackBackground};
    Libmpeg::DispatchCallback(sys->mpeg, &background);
    while ((*R_EE_D3_CHCR & ChcrStart) != 0)
    {
    }

    WaitIpu();
}
}

namespace Libmpeg
{
void CopyMacroblocks(MpegSystem* sys, MpegImage* image)
{
    u32 source = reinterpret_cast<u32>(image->pixels) & PhysicalMask;
    u32 output = reinterpret_cast<u32>(sys->output) & PhysicalMask;
    // A column's bytes, and the output's (its height's when given)
    s32 columnBytes;
    s32 outputColumnBytes;
    s32 passes;
    if (sys->pictureStructure == MpegFrame || sys->outputHeight == 0)
    {
        columnBytes = image->heightMacroblocks * MacroblockBytes;
        outputColumnBytes = sys->outputHeight != 0 ? (sys->outputHeight >> 4) * MacroblockBytes : columnBytes;
        passes = 1;
    }
    else
    {
        // A field picture into an output of a given height: each field's half of the column in turn
        outputColumnBytes = (sys->outputHeight >> 4) * (MacroblockBytes / 2);
        columnBytes = (image->heightMacroblocks >> 1) * MacroblockBytes;
        passes = 2;
    }

    s32 columnQuadwords = columnBytes >> 4;
    for (s32 pass = 0; pass < passes; pass++)
    {
        u32 destination = output;
        for (s32 column = 0; column < image->widthMacroblocks; column++)
        {
            StartScratchpadDma(R_EE_D9_CHCR, R_EE_D9_MADR, R_EE_D9_QWC, R_EE_D9_SADR, source, columnQuadwords,
                               ChcrStart | ChcrFromMemory);
            source += columnBytes;
            while ((*R_EE_D9_CHCR & ChcrStart) != 0)
            {
            }

            StartScratchpadDma(R_EE_D8_CHCR, R_EE_D8_MADR, R_EE_D8_QWC, R_EE_D8_SADR, destination, columnQuadwords, ChcrStart);
            destination += outputColumnBytes;
            while ((*R_EE_D8_CHCR & ChcrStart) != 0)
            {
            }

            while (*R_EE_D8_QWC != 0)
            {
            }
        }

        output += sys->outputMacroblocks * (MacroblockBytes / 2);
    }
}

void OutputImage(MpegSystem* sys, MpegImage* image)
{
    Mpeg* mpeg = sys->mpeg;
    if (sys->countPts != 0)
    {
        CountPts(sys, &mpeg->pts, image, static_cast<s32>(sys->fields));
    }
    else
    {
        mpeg->pts = image->pts;
    }

    TakePendingPts(sys, &mpeg->pts);
    mpeg->dts = image->dts;
    mpeg->flags = FlagsOf(image);
    sys->lastPts = static_cast<s32>(mpeg->pts);
    sys->fields = g_MpegFieldsShown[(mpeg->flags >> MpegFlagsTimingShift) & MpegFlagsTiming];
    sys->outputDisplayWidth = image->displayHorizontalSize;
    sys->outputDisplayHeight = image->displayVerticalSize;
    for (s32 i = 0; i < 3; i++)
    {
        sys->outputCentreHorizontalOffsets[i] = image->frameCentreHorizontalOffsets[i];
    }

    for (s32 i = 0; i < 3; i++)
    {
        sys->outputCentreVerticalOffsets[i] = image->frameCentreVerticalOffsets[i];
    }

    if (!OutputTakes(sys, image))
    {
        ReportOutputTooSmall(sys, image);
        return;
    }

    if (image->holdsPicture != 1)
    {
        return;
    }

    PutOut(sys, image);
}

void OutputFields(MpegSystem* sys, MpegImage* image, MpegImage* other)
{
    // The field decoded first gives the first times
    MpegImage* first;
    MpegImage* second;
    u64 topFieldFirst;
    if (sys->pictureStructure == MpegBottomField)
    {
        first = image;
        second = other;
        topFieldFirst = MpegFlagsTopFieldFirst;
    }
    else
    {
        first = other;
        second = image;
        topFieldFirst = 0;
    }

    Mpeg* mpeg = sys->mpeg;
    if (sys->countPts != 0)
    {
        CountPts(sys, &mpeg->pts, first, static_cast<s32>(sys->fields));
    }
    else
    {
        mpeg->pts = first->pts;
    }

    TakePendingPts(sys, &mpeg->pts);
    mpeg->dts = first->dts;
    mpeg->flags = FlagsOf(first);
    sys->fields = 1;
    sys->lastPts = static_cast<s32>(mpeg->pts);
    if (sys->countPts != 0)
    {
        CountPts(sys, &mpeg->pts2nd, second, 1);
    }
    else
    {
        mpeg->pts2nd = second->pts;
    }

    TakePendingPts(sys, &mpeg->pts2nd);
    mpeg->dts2nd = second->dts;
    mpeg->flags2nd = FlagsOf(second);
    sys->fields = 1;
    sys->lastPts = static_cast<s32>(mpeg->pts2nd);
    mpeg->flags |= topFieldFirst;
    sys->outputDisplayWidth = first->displayHorizontalSize;
    mpeg->flags2nd |= topFieldFirst;
    sys->outputDisplayHeight = first->displayVerticalSize;
    sys->outputCentreHorizontalOffsets[0] = first->frameCentreHorizontalOffsets[0];
    sys->outputCentreHorizontalOffsets[1] = second->frameCentreHorizontalOffsets[1];
    sys->outputCentreVerticalOffsets[0] = first->frameCentreVerticalOffsets[0];
    sys->outputCentreVerticalOffsets[1] = second->frameCentreVerticalOffsets[1];
    if (!OutputTakes(sys, image))
    {
        ReportOutputTooSmall(sys, image);
        return;
    }

    if (image->holdsPicture != 1 || other->holdsPicture != 1)
    {
        return;
    }

    // The fields' rows are the frame's in turn: image is put out as a picture of twice its height
    image->heightMacroblocks <<= 1;
    if (sys->outputRgb32 != 0)
    {
        ConvertImage(sys, image);
    }
    else
    {
        CopyMacroblocks(sys, image);
    }

    image->heightMacroblocks >>= 1;
    if (sys->outputState != MpegPicturePutOut)
    {
        sys->outputState = MpegPicturePutOut;
        sys->firstOutputPicture = sys->pictureNumber;
    }

    sys->pictureOutput = 1;
}

s32 Flush(Mpeg* mpeg)
{
    MpegSystem* sys = mpeg->sys;
    if (sys->pictureWaiting == 0 || sys->outputState == MpegNothingDecoded)
    {
        return 0;
    }

    if (sys->secondFieldMissing != 0)
    {
        Error(sys, g_MpegSecondFieldMissing);
    }
    else if (sys->pictureStructure == MpegFrame)
    {
        OutputImage(sys, sys->frames[MpegFuture]);
    }
    else
    {
        OutputFields(sys, sys->topFields[MpegFuture], sys->bottomFields[MpegFuture]);
    }

    sys->secondFieldMissing = 0;
    mpeg->frameCount = sys->pictureNumber - sys->firstOutputPicture;
    sys->pictureWaiting = 0;
    return 1;
}

void OutputFrame(MpegSystem* sys, s32, s32 waiting)
{
    if (waiting != 0)
    {
        bool bPicture = sys->pictureCodingType == MpegPictureB;
        if (sys->pictureStructure == MpegFrame)
        {
            OutputImage(sys, bPicture ? sys->frames[MpegBImage] : sys->frames[MpegPast]);
        }
        else if (bPicture)
        {
            OutputFields(sys, sys->topFields[MpegBImage], sys->bottomFields[MpegBImage]);
        }
        else
        {
            OutputFields(sys, sys->topFields[MpegPast], sys->bottomFields[MpegPast]);
        }
    }

    if (sys->pendingPtsState == MpegPtsSet)
    {
        sys->pendingPtsState = MpegPtsForNextPicture;
    }
}

void ConvertInChunks(MpegSystem* sys, u8* output, s32 macroblocks)
{
    ConvertChunks chunks;
    chunks.macroblocks = macroblocks;
    chunks.next = (reinterpret_cast<u32>(output) + ChunkBytes) & PhysicalMask;
    chunks.error = 0;
    chunks.converted = 0;
    chunks.chunks = macroblocks / ChunkMacroblocks + 1;
    WaitIpu();
    s32 handler = AddDmacHandler2(IpuFromChannel, ConvertedChunk, 0, &chunks);
    *R_EE_D_STAT = 1 << IpuFromChannel;
    EnableDmac(IpuFromChannel);
    s32 interrupts = DIntr();
    *R_EE_D3_MADR = reinterpret_cast<u32>(output) & PhysicalMask;
    *R_EE_D3_QWC = ChunkQuadwords;
    *R_EE_D3_CHCR = ChcrStart;
    if (interrupts != 0)
    {
        EIntr();
    }

    *IpuCommand = IpuConvert | ChunkMacroblocks;
    MpegCallbackData background = {MpegCallbackBackground};
    DispatchCallback(sys->mpeg, &background);
    // Retail bug: the handler's error is read before the wait for the chunks, so an error of the chunks converted meanwhile is
    // never reported
    s32 error = chunks.error;
    s32 count = chunks.chunks;
    while (chunks.converted < count)
    {
    }

    if (error != 0)
    {
        Error(sys, g_MpegConvertError);
    }

    WaitIpu();
    DisableDmac(IpuFromChannel);
    RemoveDmacHandler(IpuFromChannel, handler);
}

void ConvertImage(MpegSystem* sys, MpegImage* image)
{
    s32 macroblocks = image->widthMacroblocks * image->heightMacroblocks;
    MpegCallbackData dma = {MpegCallbackStopDma};
    DispatchCallback(sys->mpeg, &dma);
    if (IpuControlRegister{*IpuControl}.errorFound)
    {
        *IpuControl = IpuControlReset;
    }

    bool oneCommand = macroblocks <= ChunkMacroblocks;
    WaitIpu();
    SetIpuCommand(sys, IpuClearInput);
    WaitIpu();
    FlushCache(0);
    SendChunks send;
    send.quadwords = macroblocks * MacroblockQuadwords;
    send.address = reinterpret_cast<u32>(image->pixels) & PhysicalMask;
    if (send.quadwords > SendQuadwords)
    {
        s32 handler = AddDmacHandler2(IpuToChannel, SentChunk, 0, &send);
        *R_EE_D_STAT = 1 << IpuToChannel;
        EnableDmac(IpuToChannel);
        s32 interrupts = DIntr();
        *R_EE_D4_MADR = send.address;
        *R_EE_D4_QWC = SendQuadwords;
        *R_EE_D4_CHCR = ChcrStart | ChcrFromMemory;
        if (interrupts != 0)
        {
            EIntr();
        }

        send.address = (send.address + (SendQuadwords << 4)) & PhysicalMask;
        send.quadwords -= SendQuadwords;
        if (oneCommand)
        {
            ConvertWhole(sys, macroblocks);
        }
        else
        {
            ConvertInChunks(sys, sys->output, macroblocks);
        }

        DisableDmac(IpuToChannel);
        RemoveDmacHandler(IpuToChannel, handler);
    }
    else
    {
        s32 interrupts = DIntr();
        *R_EE_D4_MADR = reinterpret_cast<u32>(image->pixels) & PhysicalMask;
        *R_EE_D4_QWC = send.quadwords;
        *R_EE_D4_CHCR = ChcrStart | ChcrFromMemory;
        if (interrupts != 0)
        {
            EIntr();
        }

        send.quadwords = 0;
        if (oneCommand)
        {
            ConvertWhole(sys, macroblocks);
        }
        else
        {
            ConvertInChunks(sys, sys->output, macroblocks);
        }
    }

    dma.type = MpegCallbackRestartDma;
    DispatchCallback(sys->mpeg, &dma);
}

s32 ConvertedChunk(s32, void* argument, void*)
{
    ConvertChunks* chunks = static_cast<ConvertChunks*>(argument);
    *R_EE_D_STAT = 1 << IpuFromChannel;
    chunks->converted = chunks->converted + 1;
    if (*R_EE_D3_QWC != 0 || (*R_EE_D3_CHCR & ChcrStart) != 0)
    {
        chunks->error = 1;
        ExitHandler();
        return 0;
    }

    if (chunks->converted < chunks->chunks - 1)
    {
        *R_EE_D3_MADR = chunks->next;
        *R_EE_D3_QWC = ChunkQuadwords;
        *R_EE_D3_CHCR = ChcrStart;
        *IpuCommand = IpuConvert | ChunkMacroblocks;
        chunks->next = (chunks->next + ChunkBytes) & PhysicalMask;
    }
    else if (chunks->converted == chunks->chunks - 1)
    {
        // The last chunk, of what's left (none when the count is a multiple of 1023: then this interrupt is the last)
        chunks->macroblocks -= chunks->converted * ChunkMacroblocks;
        *R_EE_D3_MADR = chunks->next;
        *R_EE_D3_QWC = chunks->macroblocks * Rgb32MacroblockQuadwords;
        *R_EE_D3_CHCR = ChcrStart;
        *IpuCommand = chunks->macroblocks | IpuConvert;
    }

    ExitHandler();
    return 0;
}

s32 SentChunk(s32, void* argument, void*)
{
    SendChunks* send = static_cast<SendChunks*>(argument);
    *R_EE_D_STAT = 1 << IpuToChannel;
    if (send->quadwords == 0)
    {
        ExitHandler();
        return 1;
    }

    if (send->quadwords > SendQuadwords)
    {
        *R_EE_D4_MADR = send->address;
        *R_EE_D4_QWC = SendQuadwords;
        *R_EE_D4_CHCR = ChcrStart | ChcrFromMemory;
        send->address = (send->address + (SendQuadwords << 4)) & PhysicalMask;
        send->quadwords -= SendQuadwords;
    }
    else
    {
        *R_EE_D4_MADR = send->address;
        *R_EE_D4_QWC = send->quadwords;
        *R_EE_D4_CHCR = ChcrStart | ChcrFromMemory;
        send->quadwords = 0;
    }

    ExitHandler();
    return 0;
}
}
