#include "mpeg.h"

// libmpeg's video headers: the sequence's, the GOP's, the picture's and the slice's, their extensions, and the reference pictures'
// memory for the sequence's picture size

namespace
{
// extension_start_code_identifier's values libmpeg knows (the others read as 0)
constexpr u32 ExtensionIdentifiers = 11;
// MPEG's quantiser matrices: 64 values of a byte
constexpr u32 QuantiserMatrixBytes = 64;
}

extern "C"
{
    extern const char g_MpegSliceCodeOutOfRange[] RETAIL(D_00307768);
    extern const char g_MpegSliceError[] RETAIL(D_00307790);
    extern const char g_MpegVerticalSizeTooLarge[] RETAIL(D_003077B8);
    extern const char g_MpegChromaFormatNot420[] RETAIL(D_003077D0);
    extern const char g_MpegUnsupportedProfile[] RETAIL(D_003077F8);
    extern const char g_MpegChromaIntraMatrix[] RETAIL(D_00307818);
    extern const char g_MpegChromaNonIntraMatrix[] RETAIL(D_00307840);
    extern const char g_MpegSequenceScalableExtension[] RETAIL(D_00307870);
    extern const char g_MpegUnknownExtension[] RETAIL(D_003078A0);
    extern const char g_MpegSpatialScalableExtension[] RETAIL(D_003078B8);
    extern const char g_MpegTemporalScalableExtension[] RETAIL(D_003078F0);
    // The extensions' readers by extension_start_code_identifier
    extern void (*const g_MpegExtensionReaders[ExtensionIdentifiers])(MpegSystem* sys) RETAIL(D_00307928);
    // MPEG's default quantiser matrices, for sequences without their own
    extern u8 g_MpegDefaultIntraMatrix[QuantiserMatrixBytes] RETAIL(D_002E8280);
    extern u8 g_MpegDefaultNonIntraMatrix[QuantiserMatrixBytes] RETAIL(D_002E82C0);
}

namespace Libmpeg
{
namespace
{
// The profiles and levels libmpeg takes: Main profile at Main level, Simple profile at Main level, Main profile at High level
constexpr u32 MainProfileMainLevel = 0x48;
constexpr u32 SimpleProfileMainLevel = 0x58;
constexpr u32 MainProfileHighLevel = 0x44;
// MPEG-1's matrix coefficients (ITU-R BT.470-2 System B, G)
constexpr s32 Mpeg1MatrixCoefficients = 5;
// The largest vertical_size_value libmpeg takes without an error
constexpr s32 MaximumVerticalSize = 2800;
// A slice's start code's low byte, slice_vertical_position: its row of macroblocks, from 1
constexpr u32 SliceVerticalPosition = 0xFF;
// temporal_reference's range (10 bits)
constexpr s32 TemporalReferences = 0x400;
// The reference pictures' memory is 64 byte aligned
constexpr s32 FrameBufferAlignment = 0x40;

// The headers' values read at once, the first of them in the top bits. NextBits leaves the bits above the ones it reads 0, which
// the top value takes along

// The sequence header's horizontal_size_value, vertical_size_value, aspect_ratio_information and frame_rate_code
union SequenceSizes
{
    u32 value;
    struct
    {
        u32 frameRateCode : 4;
        u32 aspectRatio : 4;
        u32 verticalSize : 12;
        u32 horizontalSize : 12;
    };
};

// The sequence header's bit_rate_value, a marker bit, vbv_buffer_size_value and constrained_parameters_flag
union SequenceRates
{
    u32 value;
    struct
    {
        u32 constrainedParameters : 1;
        u32 vbvBufferSize : 10;
        u32 marker : 1;
        u32 bitRate : 20;
    };
};

// The sequence extension's profile_and_level_indication, progressive_sequence, chroma_format, horizontal_size_extension,
// vertical_size_extension, bit_rate_extension and a marker bit
union SequenceExtension
{
    u32 value;
    struct
    {
        u32 marker : 1;
        u32 bitRateExtension : 12;
        u32 verticalSizeExtension : 2;
        u32 horizontalSizeExtension : 2;
        u32 chromaFormat : 2;
        u32 progressiveSequence : 1;
        u32 profileAndLevel : 12;
    };
};

// The rest of it: vbv_buffer_size_extension, low_delay, frame_rate_extension_n and frame_rate_extension_d
union SequenceExtensionRates
{
    u32 value;
    struct
    {
        u32 frameRateExtensionD : 5;
        u32 frameRateExtensionN : 2;
        u32 lowDelay : 1;
        u32 vbvBufferSizeExtension : 24;
    };
};

// What the sequence extension extends the sequence header's sizes and rates by
constexpr s32 SizeExtensionShift = 12;
constexpr s32 BitRateExtensionShift = 18;
constexpr s32 VbvBufferSizeExtensionShift = 10;

// A quantiser matrix (SETIQ's intra or non-intra) the IPU loads from the stream
void ReadQuantiserMatrix(MpegSystem* sys, u32 command)
{
    WaitIpuIdleIfBusyForHeaders(sys);
    SetIpuCommand(sys, command);
    WaitIpuIdleIfBusyForHeaders(sys);
}

// A default quantiser matrix the IPU loads from memory: the stream's DMA stopped meanwhile, the IPU's input cleared and the matrix
// sent through the toIPU channel
void SendDefaultMatrix(MpegSystem* sys, u32 command, const u8* matrix)
{
    MpegCallbackData dma = {MpegCallbackStopDma};
    DispatchCallback(sys->mpeg, &dma);
    WaitIpuIdleIfBusyForHeaders(sys);
    *IpuCommand = IpuClearInput;
    WaitIpuIdleIfBusyForHeaders(sys);
    s32 interrupts = DIntr();
    *R_EE_D4_MADR = reinterpret_cast<u32>(matrix) & PhysicalMask;
    *R_EE_D4_QWC = QuantiserMatrixBytes >> 4;
    *R_EE_D4_CHCR = ChcrFromMemory | ChcrStart;
    if (interrupts != 0)
    {
        EIntr();
    }

    SetIpuCommand(sys, command);
    WaitIpuIdleIfBusyForHeaders(sys);
    dma.type = MpegCallbackRestartDma;
    DispatchCallback(sys->mpeg, &dma);
}
}

s32 ReadSliceHeader(MpegSystem* sys, s32, s32* address, s32* increment, MpegVector predictors[2][2])
{
    sys->macroblockError = 0;
    NextStartCode(sys);
    u32 code = PeekBits(sys, 32);
    if (code - FirstSliceStartCode >= SliceStartCodes)
    {
        ErrorValue(sys, g_MpegSliceCodeOutOfRange, code);
        return PictureGivenUp;
    }

    SkipStartCode(sys);
    sys->quantiserScale = NextBits(sys, 5);
    // The slice extension: intra_slice, 7 reserved bits and extra_information_slice's bytes
    if (NextBits(sys, 1) != 0)
    {
        NextBits(sys, 1);
        SkipHeaderBits(sys, 7);
        while (NextBits(sys, 1) != 0)
        {
            SkipHeaderBits(sys, 8);
        }
    }

    s32 first = MacroblockAddressIncrement(sys);
    *increment = first;
    if (sys->macroblockError != 0)
    {
        Error(sys, g_MpegSliceError);
        return SliceGivenUp;
    }

    *address = ((code & SliceVerticalPosition) - 1) * sys->widthMacroblocks + first - 1;
    *increment = 1;
    sys->dcReset = 1;
    predictors[0][0] = {};
    predictors[0][1] = {};
    predictors[1][0] = {};
    predictors[1][1] = {};
    return SliceHeaderRead;
}

void InitialiseSequence(Mpeg* mpeg)
{
    MpegSystem* sys = mpeg->sys;
    if (sys->mpeg2 == 0)
    {
        sys->pictureStructure = MpegFrame;
        sys->framePredFrameDct = 1;
        sys->matrixCoefficients = Mpeg1MatrixCoefficients;
        sys->progressiveSequence = 1;
        sys->chromaFormat = MpegChroma420;
        sys->progressiveFrame = 1;
    }

    sys->widthMacroblocks = (sys->horizontalSize + 15) >> 4;
    if (sys->mpeg2 != 0 && sys->progressiveSequence == 0)
    {
        // Interlaced: whole macroblocks in each field
        sys->heightMacroblocks = ((sys->verticalSize + 31) >> 5) << 1;
    }
    else
    {
        sys->heightMacroblocks = (sys->verticalSize + 15) >> 4;
    }

    s32 height = sys->heightMacroblocks << 4;
    s32 width = sys->widthMacroblocks << 4;
    if (width == mpeg->width && height == mpeg->height)
    {
        return;
    }

    mpeg->width = width;
    mpeg->height = height;
    u32 frameBytes = static_cast<u32>(width) * (static_cast<u32>(height) * MacroblockBytes) / MacroblockPixels;
    ArenaRewind(&sys->arena);
    for (s32 i = 0; i < 3; i++)
    {
        sys->frameBuffers[i] = Allocate(sys, &sys->arena, frameBytes, FrameBufferAlignment);
    }

    // The frames and their fields share a frame buffer, the bottom field's macroblocks after the top's. The pictures are written
    // uncached when the DMA fetches the references
    if (sys->bufferType == MpegBuffersScratchpad || sys->bufferType == MpegBuffersOwn)
    {
        u8* buffers[3];
        for (s32 i = 0; i < 3; i++)
        {
            buffers[i] = sys->frameBuffers[i];
            if (sys->bufferType == MpegBuffersScratchpad)
            {
                buffers[i] = reinterpret_cast<u8*>((reinterpret_cast<u32>(buffers[i]) & PhysicalMask) | UncachedSegment);
            }
        }

        s32 bottomField = mpeg->width * mpeg->height / (2 * MacroblockPixels) * MacroblockBytes;
        for (s32 i = 0; i < 3; i++)
        {
            sys->images[i].pixels = buffers[i];
            sys->images[3 + i].pixels = buffers[i];
            sys->images[6 + i].pixels = buffers[i] + bottomField;
        }
    }

    for (s32 i = 0; i < 9; i++)
    {
        MpegImage* image = &sys->images[i];
        image->width = width;
        image->widthMacroblocks = width >> 4;
        if (i < 3)
        {
            image->height = height;
            image->heightMacroblocks = height >> 4;
        }
        else
        {
            image->height = height / 2;
            image->heightMacroblocks = height / 2 >> 4;
        }
    }
}

void ReadSequenceHeader(MpegSystem* sys)
{
    sys->firstStructure = 0;
    SequenceSizes sizes = {NextBits(sys, 32)};
    sys->horizontalSize = sizes.horizontalSize;
    s32 verticalSize = sizes.verticalSize;
    sys->verticalSize = verticalSize;
    if (verticalSize > MaximumVerticalSize)
    {
        Error(sys, g_MpegVerticalSizeTooLarge);
    }

    SequenceRates rates = {NextBits(sys, 30)};
    sys->bitRate = rates.bitRate;
    sys->vbvBufferSize = rates.vbvBufferSize;
    s32 load = NextBits(sys, 1);
    sys->loadIntraQuantiserMatrix = load;
    if (load != 0)
    {
        ReadQuantiserMatrix(sys, IpuSetIntraMatrix);
    }
    else
    {
        SendDefaultMatrix(sys, IpuSetIntraMatrix, g_MpegDefaultIntraMatrix);
    }

    load = NextBits(sys, 1);
    sys->loadNonIntraQuantiserMatrix = load;
    if (load != 0)
    {
        ReadQuantiserMatrix(sys, IpuSetNonIntraMatrix);
    }
    else
    {
        SendDefaultMatrix(sys, IpuSetNonIntraMatrix, g_MpegDefaultNonIntraMatrix);
    }

    ExtensionAndUserData(sys);
    InitialiseSequence(sys->mpeg);
}

void ReadGroupOfPicturesHeader(MpegSystem* sys)
{
    sys->forcedBrokenLink = 0;
    sys->gopStarted = 1;
    sys->temporalReferenceBase = sys->temporalReferenceLast + 1;
    // The time code (drop_frame_flag, hours, minutes, a marker bit, seconds, pictures)
    NextBits(sys, 1);
    NextBits(sys, 5);
    NextBits(sys, 6);
    NextBits(sys, 1);
    NextBits(sys, 6);
    NextBits(sys, 6);
    sys->closedGop = NextBits(sys, 1);
    sys->brokenLink = NextBits(sys, 1);
    ExtensionAndUserData(sys);
}

void ReadPictureHeader(MpegSystem* sys)
{
    s32 temporalReference = NextBits(sys, 10);
    sys->pictureCodingType = NextBits(sys, 3);
    // vbv_delay
    NextBits(sys, 16);
    if (sys->pictureCodingType == MpegPictureP || sys->pictureCodingType == MpegPictureB)
    {
        sys->fullPelForwardVector = NextBits(sys, 1);
        sys->forwardFCode = NextBits(sys, 3);
    }

    if (sys->pictureCodingType == MpegPictureB)
    {
        sys->fullPelBackwardVector = NextBits(sys, 1);
        sys->backwardFCode = NextBits(sys, 3);
    }

    // extra_information_picture
    while (NextBits(sys, 1) != 0)
    {
        SkipHeaderBits(sys, 8);
    }

    ExtensionAndUserData(sys);
    bool wrapped = false;
    if (sys->pictureCodingType != MpegPictureB && temporalReference != 0)
    {
        // Retail bug: meant for a temporal reference that wrapped around 1024, this tests the 10 bit one for being negative, which
        // it never is (nothing reads the temporal references)
        if (temporalReference < 0)
        {
            wrapped = sys->gopStarted == 0;
        }

        sys->gopStarted = 0;
    }

    sys->temporalReference = sys->temporalReferenceBase + temporalReference;
    if (wrapped)
    {
        sys->temporalReference += TemporalReferences;
    }

    if (sys->temporalReferenceLast < sys->temporalReference)
    {
        sys->temporalReferenceLast = sys->temporalReference;
    }
}

void ReadSequenceExtension(MpegSystem* sys)
{
    sys->mpeg2 = 1;
    IpuControlRegister control = {*IpuControl};
    control.mpeg1 = 0;
    *IpuControl = control.value;
    SequenceExtension extension = {NextBits(sys, 28)};
    s32 bitRateExtension = extension.bitRateExtension;
    s32 chromaFormat = extension.chromaFormat;
    s32 verticalSizeExtension = extension.verticalSizeExtension;
    s32 horizontalSizeExtension = extension.horizontalSizeExtension;
    sys->chromaFormat = chromaFormat;
    if (chromaFormat != MpegChroma420)
    {
        Error(sys, g_MpegChromaFormatNot420);
    }

    sys->progressiveSequence = extension.progressiveSequence;
    u32 profileAndLevel = extension.profileAndLevel;
    s32 vbvBufferSizeExtension = SequenceExtensionRates{NextBits(sys, 16)}.vbvBufferSizeExtension;
    if (profileAndLevel != MainProfileMainLevel && profileAndLevel != SimpleProfileMainLevel &&
        profileAndLevel != MainProfileHighLevel)
    {
        Error(sys, g_MpegUnsupportedProfile);
    }

    // The sizes' low 12 bits are the sequence header's
    constexpr s32 SizeValueMask = (1 << SizeExtensionShift) - 1;
    sys->horizontalSize = (horizontalSizeExtension << SizeExtensionShift) | (sys->horizontalSize & SizeValueMask);
    sys->verticalSize = (verticalSizeExtension << SizeExtensionShift) | (sys->verticalSize & SizeValueMask);
    sys->bitRate += bitRateExtension << BitRateExtensionShift;
    sys->vbvBufferSize += vbvBufferSizeExtension << VbvBufferSizeExtensionShift;
}

void ReadQuantMatrixExtension(MpegSystem* sys)
{
    s32 load = NextBits(sys, 1);
    sys->loadIntraQuantiserMatrix = load;
    if (load != 0)
    {
        ReadQuantiserMatrix(sys, IpuSetIntraMatrix);
    }

    load = NextBits(sys, 1);
    sys->loadNonIntraQuantiserMatrix = load;
    if (load != 0)
    {
        ReadQuantiserMatrix(sys, IpuSetNonIntraMatrix);
    }

    // The 4:2:0 chroma has no matrices of its own
    if (NextBits(sys, 1) != 0)
    {
        Error(sys, g_MpegChromaIntraMatrix);
    }

    if (NextBits(sys, 1) != 0)
    {
        Error(sys, g_MpegChromaNonIntraMatrix);
    }
}

void ReadSequenceScalableExtension(MpegSystem* sys)
{
    Error(sys, g_MpegSequenceScalableExtension);
}

void ReadUnknownExtension(MpegSystem* sys)
{
    Error(sys, g_MpegUnknownExtension);
}

void ReadPictureCodingExtension(MpegSystem* sys)
{
    sys->fCodes[0][0] = NextBits(sys, 4);
    sys->fCodes[0][1] = NextBits(sys, 4);
    sys->fCodes[1][0] = NextBits(sys, 4);
    sys->fCodes[1][1] = NextBits(sys, 4);
    u32 dcPrecision = NextBits(sys, 2);
    *IpuControl = (*IpuControl & ~IpuControlDcPrecision) | (dcPrecision << IpuControlDcPrecisionShift);
    s32 structure = NextBits(sys, 2);
    sys->pictureStructure = structure;
    if (sys->firstStructure == 0)
    {
        sys->firstStructure = structure;
    }

    sys->topFieldFirst = NextBits(sys, 1);
    sys->framePredFrameDct = NextBits(sys, 1);
    sys->concealmentMotionVectors = NextBits(sys, 1);
    u32 quantiserType = NextBits(sys, 1);
    *IpuControl = (*IpuControl & ~IpuControlQuantiserType) | (quantiserType << IpuControlQuantiserTypeShift);
    u32 intraVlc = NextBits(sys, 1);
    *IpuControl = (*IpuControl & ~IpuControlIntraVlc) | (intraVlc << IpuControlIntraVlcShift);
    u32 alternateScan = NextBits(sys, 1);
    *IpuControl = (*IpuControl & ~IpuControlAlternateScan) | (alternateScan << IpuControlAlternateScanShift);
    sys->repeatFirstField = NextBits(sys, 1);
    // chroma_420_type
    NextBits(sys, 1);
    sys->progressiveFrame = NextBits(sys, 1);
    // composite_display_flag: v_axis, field_sequence, sub_carrier, burst_amplitude, sub_carrier_phase
    if (NextBits(sys, 1) != 0)
    {
        NextBits(sys, 1);
        NextBits(sys, 3);
        NextBits(sys, 1);
        NextBits(sys, 7);
        NextBits(sys, 8);
    }
}

void ReadPictureSpatialScalableExtension(MpegSystem* sys)
{
    Error(sys, g_MpegSpatialScalableExtension);
}

void ReadPictureTemporalScalableExtension(MpegSystem* sys)
{
    Error(sys, g_MpegTemporalScalableExtension);
}

void ExtensionAndUserData(MpegSystem* sys)
{
    NextStartCode(sys);
    while (true)
    {
        u32 code = PeekBits(sys, 32);
        if (code == ExtensionStartCode)
        {
            SkipStartCode(sys);
            u32 identifier = NextBits(sys, 4);
            if (identifier >= ExtensionIdentifiers)
            {
                identifier = 0;
            }

            g_MpegExtensionReaders[identifier](sys);
            NextStartCode(sys);
        }
        else if (code == UserDataStartCode)
        {
            SkipStartCode(sys);
            NextStartCode(sys);
        }
        else
        {
            return;
        }
    }
}

void WaitIpuIdleIfBusyForHeaders(MpegSystem* sys)
{
    if (IpuWorking())
    {
        WaitIpuIdle(sys);
    }
}

void SkipHeaderBits(MpegSystem* sys, s32 count)
{
    SkipBits(sys, count);
}

s32 NextHeader(MpegSystem* sys)
{
    while (sys->aborted == 0)
    {
        NextStartCode(sys);
        switch (NextBits(sys, 32))
        {
        case SequenceHeaderCode:
            ReadSequenceHeader(sys);
            break;
        case GroupStartCode:
            ReadGroupOfPicturesHeader(sys);
            break;
        case PictureStartCode:
        {
            ReadPictureHeader(sys);
            MpegTimeStampData timeStamp = {MpegCallbackTimeStamp, -1, -1};
            DispatchCallback(sys->mpeg, reinterpret_cast<MpegCallbackData*>(&timeStamp));
            sys->nextDts = timeStamp.dts;
            sys->nextPts = timeStamp.pts;
            return sys->pictureCodingType;
        }
        case SequenceEndCode:
            return MpegSequenceEnded;
        }
    }

    return MpegHeaderAborted;
}

void SkipStartCode(MpegSystem* sys)
{
    SkipHeaderBits(sys, 32);
}

void SkipToByte(MpegSystem* sys)
{
    WaitIpuIdleIfBusyForHeaders(sys);
    u32 bits = -IpuBitPositionRegister{*IpuBitPosition}.bitPosition & 7;
    if (bits != 0)
    {
        SkipHeaderBits(sys, bits);
    }
}

void ReadSequenceDisplayExtension(MpegSystem* sys)
{
    // video_format
    NextBits(sys, 3);
    // colour_description: colour_primaries, transfer_characteristics, matrix_coefficients
    if (NextBits(sys, 1) != 0)
    {
        NextBits(sys, 8);
        NextBits(sys, 8);
        sys->matrixCoefficients = NextBits(sys, 8);
    }

    sys->displayHorizontalSize = NextBits(sys, 14);
    NextBits(sys, 1);
    sys->displayVerticalSize = NextBits(sys, 14);
}

void ReadCopyrightExtension(MpegSystem* sys)
{
    // copyright_flag, copyright_identifier, original_or_copy, 7 reserved bits, a marker bit, copyright_number_1, a marker bit,
    // copyright_number_2, a marker bit, copyright_number_3
    NextBits(sys, 1);
    NextBits(sys, 8);
    NextBits(sys, 1);
    NextBits(sys, 7);
    NextBits(sys, 1);
    NextBits(sys, 20);
    NextBits(sys, 1);
    NextBits(sys, 22);
    NextBits(sys, 1);
    NextBits(sys, 22);
}

void ReadPictureDisplayExtension(MpegSystem* sys)
{
    // number_of_frame_centre_offsets: one a field shown
    s32 offsets;
    if (sys->progressiveSequence != 0)
    {
        if (sys->repeatFirstField == 0)
        {
            offsets = 1;
        }
        else
        {
            offsets = sys->topFieldFirst != 0 ? 3 : 2;
        }
    }
    else if (sys->pictureStructure != MpegFrame)
    {
        offsets = 1;
    }
    else
    {
        offsets = sys->repeatFirstField != 0 ? 3 : 2;
    }

    for (s32 i = 0; i < offsets; i++)
    {
        sys->frameCentreHorizontalOffsets[i] = NextBits(sys, 16);
        NextBits(sys, 1);
        sys->frameCentreVerticalOffsets[i] = NextBits(sys, 16);
        NextBits(sys, 1);
    }
}
}
