#include "mpeg.h"

#include <stdio.h>

// libmpeg's pictures: decoded or passed over by type, their reference pictures moved on, their slices' macroblocks decoded by the
// IPU into the scratchpad while the motion compensation predicts them and the macroblock before is finished

extern "C"
{
    extern const char g_MpegOutputMisaligned[] RETAIL(D_00307410);
    extern const char g_MpegAbortedNeedsReset[] RETAIL(D_00307450);
    extern const char g_MpegAborted[] RETAIL(D_00307490);
    extern const char g_MpegOddFieldCount[] RETAIL(D_003074C8);
    extern const char g_MpegUnknownStructure[] RETAIL(D_003074E8);
    extern const char g_MpegSkipToNextPicture[] RETAIL(D_003076A0);
    extern const char g_MpegTooManyMacroblocks[] RETAIL(D_003076C0);
    extern const char g_MpegSkippedInIPicture[] RETAIL(D_003076E0);
    extern const char g_MpegInvalidMacroblockType[] RETAIL(D_00307710);
    extern const char g_MpegInvalidAddressIncrement[] RETAIL(D_00307730);
}

namespace Libmpeg
{
namespace
{
// The macroblock address increment's codes besides the increments: MPEG-1's stuffing, and the escape that adds 33 to the next
constexpr s32 AddressIncrementStuffing = 0x22;
constexpr s32 AddressIncrementEscape = 0x23;
constexpr s32 AddressIncrementEscaped = 0x21;
// MPEG-2's stuffing code (0000 0001 111), which the IPU's table doesn't have
constexpr u32 Mpeg2Stuffing = 0xF;
constexpr s32 Mpeg2StuffingBits = 11;
// The output's alignment the IPU's DMA needs
constexpr u32 OutputAlignment = 0x40;
// A word of the input, whose bits IPU_TOP holds
constexpr s32 WordBits = 32;

// The bits read ahead taken from the IPU's TOP: a word of them once it holds them, else what's left of the input's word
void ReadTop(MpegSystem* sys)
{
    IpuBitPositionRegister position = {*IpuBitPosition};
    IpuDataRegister top = {*R_EE_IPU_TOP};
    sys->top = top.data;
    sys->topBits = !top.busy ? WordBits : -position.bitPosition & (WordBits - 1);
}

// A VLC the IPU decodes with a table (no code matched: a macroblock error)
IpuVlcResult DecodeVariable(MpegSystem* sys, u32 table)
{
    WaitIpuIdleIfBusy(sys);
    SetIpuCommand(sys, IpuDecodeVariable | table);
    IpuVlcResult result = {static_cast<u32>(WaitIpuResult(sys))};
    ReadTop(sys);
    sys->macroblockError = result.value == 0;
    return result;
}

// Waits for the fromIPU channel to take what the IPU decoded, the toIPU channel given more of the stream while it's idle, and
// takes the bits read ahead from the IPU. Returns false when the decoding was aborted (the IPU's DMA then stopped) or the IPU
// found an error
bool FinishIpuOutput(MpegSystem* sys)
{
    if (*R_EE_D3_QWC != 0 && !IpuControlRegister{*IpuControl}.errorFound)
    {
        do
        {
            if (*R_EE_D4_QWC == 0 && (*R_EE_D4_CHCR & ChcrStart) == 0)
            {
                DispatchNoData(sys->mpeg);
            }

            if (sys->aborted != 0)
            {
                StopIpuDma(sys);
                return false;
            }
        } while (*R_EE_D3_QWC != 0 && !IpuControlRegister{*IpuControl}.errorFound);
    }

    ReadTop(sys);
    if (IpuControlRegister{*IpuControl}.errorFound)
    {
        BlockDecodeError(sys);
        return false;
    }

    return true;
}

// A vector's component from its predictor, motion code and residual (MPEG-2's 7.6.3.1), wrapped into the f_code's range (-16 f
// to 16 f - 1, f = 1 << r_size); MPEG-1's full pixel vectors count in whole pixels
s32 DecodeComponent(s32 predictor, s32 code, s32 residual, s32 residualSize, s32 fullPel)
{
    constexpr s32 VectorLimit = 16;
    s32 limit = VectorLimit << residualSize;
    s32 value = fullPel != 0 ? predictor >> 1 : predictor;
    if (code > 0)
    {
        value += ((code - 1) << residualSize) + residual + 1;
        if (value >= limit)
        {
            value -= limit * 2;
        }
    }
    else if (code < 0)
    {
        value -= ((-code - 1) << residualSize) + residual + 1;
        if (value < -limit)
        {
            value += limit * 2;
        }
    }

    return fullPel != 0 ? value << 1 : value;
}
}

s32 GetPicture(Mpeg* mpeg)
{
    MpegSystem* sys = mpeg->sys;
    sys->ended = 0;
    if ((reinterpret_cast<u32>(sys->output) & (OutputAlignment - 1)) != 0)
    {
        ErrorValue(sys, g_MpegOutputMisaligned, reinterpret_cast<s32>(sys->output));
        return -1;
    }

    if (sys->aborted != 0)
    {
        printf(g_MpegAbortedNeedsReset);
        return -1;
    }

    sys->pictureOutput = 0;
    s32 decoded = 0;
    s32 header = 1;
    do
    {
        // A field picture without its second field has the next picture's header read already
        if (decoded != MpegSecondFieldMissing)
        {
            do
            {
                header = NextHeader(sys);
                if (header < 0)
                {
                    return -1;
                }
            } while (header != 0 && sys->pictureStructure != sys->firstStructure && sys->mpeg2 != 0);
        }

        switch (header)
        {
        case MpegSequenceEnded:
            Flush(mpeg);
            sys->ended = 1;
            break;
        case MpegPictureI:
            sys->pictureCounts[MpegBPictures] = 0;
            sys->pictureCounts[MpegPPictures] = 0;
            sys->pictureCounts[MpegIPictures] = 0;
            decoded = DecodeOrSkip(mpeg, 0, sys->decodeLimits[MpegIPictures]);
            sys->pictureCounts[MpegIPictures]++;
            break;
        case MpegPictureP:
            decoded = DecodeOrSkip(mpeg, sys->pictureCounts[MpegPPictures], sys->decodeLimits[MpegPPictures]);
            sys->pictureCounts[MpegPPictures]++;
            break;
        case MpegPictureB:
        case MpegPictureD:
            decoded = DecodeOrSkip(mpeg, sys->pictureCounts[MpegBPictures], sys->decodeLimits[MpegBPictures]);
            sys->pictureCounts[MpegBPictures]++;
            break;
        }

        if (sys->aborted != 0)
        {
            printf(g_MpegAborted);
            return -1;
        }
    } while (sys->pictureOutput == 0 && sys->ended == 0);

    return 1;
}

s32 UpdateReferences(MpegSystem* sys, s32 secondField)
{
    s32 structure = sys->pictureStructure;
    s32 codingType = sys->pictureCodingType;
    s32 decodable = 0;
    if (codingType == MpegPictureB)
    {
        sys->frames[MpegCurrent] = sys->frames[MpegBImage];
        sys->topFields[MpegCurrent] = sys->topFields[MpegBImage];
        sys->bottomFields[MpegCurrent] = sys->bottomFields[MpegBImage];
        // With two frames' references (four fields') since the I picture, the B picture's are its GOP's
        s32 ownReferences = structure == MpegFrame ? 2 : 4;
        if (sys->pictureCounts[MpegIPictures] + sys->pictureCounts[MpegPPictures] >= ownReferences)
        {
            sys->forcedBrokenLink = 0;
            sys->brokenLink = 0;
            sys->closedGop = 0;
        }

        // An open GOP's B pictures after a broken link have no past reference
        if ((sys->forcedBrokenLink != 0 || sys->brokenLink != 0) && sys->closedGop == 0)
        {
            sys->frames[MpegPast]->holdsPicture = 0;
            sys->topFields[MpegPast]->holdsPicture = 0;
            sys->bottomFields[MpegPast]->holdsPicture = 0;
            structure = sys->pictureStructure;
        }

        sys->forcedBrokenLink = 0;
        sys->brokenLink = 0;
        if (structure == MpegFrame)
        {
            decodable =
                (sys->frames[MpegPast]->holdsPicture == 1 || sys->closedGop != 0) && sys->frames[MpegFuture]->holdsPicture == 1;
        }
        else
        {
            decodable = ((sys->topFields[MpegPast]->holdsPicture == 1 && sys->bottomFields[MpegPast]->holdsPicture == 1) ||
                         sys->closedGop != 0) &&
                        sys->topFields[MpegFuture]->holdsPicture == 1 && sys->bottomFields[MpegFuture]->holdsPicture == 1;
        }
    }
    else
    {
        // An I or P picture (a frame's first field) becomes the future reference, the future one the past one
        if (secondField == 0)
        {
            MpegImage* past = sys->frames[MpegPast];
            sys->frames[MpegPast] = sys->frames[MpegFuture];
            sys->frames[MpegFuture] = past;
            past = sys->topFields[MpegPast];
            sys->topFields[MpegPast] = sys->topFields[MpegFuture];
            sys->topFields[MpegFuture] = past;
            past = sys->bottomFields[MpegPast];
            sys->bottomFields[MpegPast] = sys->bottomFields[MpegFuture];
            sys->bottomFields[MpegFuture] = past;
        }

        sys->frames[MpegCurrent] = sys->frames[MpegFuture];
        sys->topFields[MpegCurrent] = sys->topFields[MpegFuture];
        sys->bottomFields[MpegCurrent] = sys->bottomFields[MpegFuture];
        if (structure == MpegFrame)
        {
            decodable = codingType != MpegPictureP || sys->frames[MpegPast]->holdsPicture == 1;
        }
        else
        {
            // A P picture's second field can take the first field of its frame for a reference
            MpegImage* firstField = structure == MpegTopField ? sys->bottomFields[MpegFuture] : sys->topFields[MpegFuture];
            decodable = codingType != MpegPictureP || (secondField != 0 && firstField->holdsPicture == 1) ||
                        (sys->topFields[MpegPast]->holdsPicture == 1 && sys->bottomFields[MpegPast]->holdsPicture == 1);
        }
    }

    // Retail bug: a picture_structure of 0 (reserved) has no image, its values go to address 0x18 on
    MpegImage* image = nullptr;
    if (structure == MpegBottomField)
    {
        image = sys->bottomFields[MpegCurrent];
    }
    else if (structure == MpegTopField)
    {
        image = sys->topFields[MpegCurrent];
    }
    else if (structure == MpegFrame)
    {
        image = sys->frames[MpegCurrent];
    }

    image->holdsPicture = 0;
    image->pts = sys->nextPts;
    image->pictureCodingType = sys->pictureCodingType;
    image->dts = sys->nextDts;
    image->pictureStructure = sys->pictureStructure;
    image->progressiveSequence = sys->progressiveSequence;
    image->progressiveFrame = sys->progressiveFrame;
    image->topFieldFirst = sys->topFieldFirst;
    image->repeatFirstField = sys->repeatFirstField;
    for (s32 i = 0; i < 3; i++)
    {
        image->frameCentreHorizontalOffsets[i] = sys->frameCentreHorizontalOffsets[i];
        image->frameCentreVerticalOffsets[i] = sys->frameCentreVerticalOffsets[i];
    }

    image->displayHorizontalSize = sys->displayHorizontalSize;
    image->displayVerticalSize = sys->displayVerticalSize;
    return decodable;
}

s32 DecodeOrSkipFrame(Mpeg* mpeg, s32 count, s32 limit)
{
    MpegSystem* sys = mpeg->sys;
    bool skipped = false;
    s32 decoded;
    if (limit == -1 || count < limit)
    {
        if (sys->outputState == MpegNothingDecoded)
        {
            mpeg->frameCount = 0;
            sys->outputState = MpegPictureDecoded;
        }

        decoded = UpdateReferences(sys, 0) != 0 && DecodePicture(sys) != 0;
    }
    else
    {
        decoded = UpdateReferences(sys, 0);
        DispatchNoData(mpeg);
        skipped = true;
    }

    if (sys->aborted != 0)
    {
        return 0;
    }

    OutputFrame(sys, sys->pictureNumber, sys->pictureWaiting);
    if (sys->pictureStructure != MpegFrame && !skipped)
    {
        sys->secondFieldMissing = sys->secondFieldMissing == 0;
    }

    mpeg->frameCount = sys->pictureNumber - sys->firstOutputPicture;
    if (sys->secondFieldMissing != 0)
    {
        return decoded;
    }

    sys->pictureNumber++;
    sys->pictureWaiting++;
    return decoded;
}

s32 DecodeOrSkipFields(Mpeg* mpeg, s32 count, s32 limit)
{
    MpegSystem* sys = mpeg->sys;
    sys->secondFieldMissing = 0;
    bool decode = limit == -1 || count < limit;
    if (sys->outputState == MpegNothingDecoded)
    {
        mpeg->frameCount = 0;
        sys->outputState = MpegPictureDecoded;
    }

    if (UpdateReferences(sys, 0) != 0 && decode)
    {
        DecodePicture(sys);
    }

    if (sys->aborted != 0)
    {
        return 0;
    }

    sys->secondFieldMissing = 1;
    if (NextHeader(sys) == MpegSequenceEnded)
    {
        Flush(mpeg);
        sys->ended = 1;
        return 0;
    }

    s32 secondStructure = sys->firstStructure == MpegTopField ? MpegBottomField : MpegTopField;
    if (sys->pictureStructure != secondStructure)
    {
        return MpegSecondFieldMissing;
    }

    s32 decoded = 0;
    if (UpdateReferences(sys, 1) != 0 && decode && DecodePicture(sys) != 0)
    {
        decoded = 1;
    }

    if (sys->aborted != 0)
    {
        return 0;
    }

    OutputFrame(sys, sys->pictureNumber, sys->pictureWaiting);
    sys->secondFieldMissing = 0;
    mpeg->frameCount = sys->pictureNumber - sys->firstOutputPicture;
    sys->pictureNumber++;
    sys->pictureWaiting++;
    if (!decode)
    {
        DispatchNoData(mpeg);
    }

    return decoded;
}

s32 DecodeOrSkip(Mpeg* mpeg, s32 count, s32 limit)
{
    if (mpeg->sys->pictureStructure == MpegFrame)
    {
        return DecodeOrSkipFrame(mpeg, count, limit);
    }

    return DecodeOrSkipFields(mpeg, count, limit);
}

s32 DecodePicture(MpegSystem* sys)
{
    if (sys->pictureStructure == MpegFrame && sys->secondFieldMissing != 0)
    {
        Error(sys, g_MpegOddFieldCount);
        sys->secondFieldMissing = 0;
    }

    MpegImage* image;
    s32 structure = sys->pictureStructure;
    if (structure == MpegBottomField)
    {
        image = sys->bottomFields[MpegCurrent];
    }
    else if (structure == MpegTopField)
    {
        image = sys->topFields[MpegCurrent];
    }
    else
    {
        image = sys->frames[MpegCurrent];
        if (structure != MpegFrame)
        {
            Error(sys, g_MpegUnknownStructure);
        }
    }

    s32 decoded = DecodePictureData(sys);
    if (decoded != 0)
    {
        image->holdsPicture = 1;
    }

    return decoded;
}

s32 DecodePictureData(MpegSystem* sys)
{
    sys->bufferIndex = 0;
    sys->unused824 = 0;
    s32 macroblocks = sys->widthMacroblocks * sys->heightMacroblocks;
    if (sys->pictureStructure != MpegFrame)
    {
        macroblocks >>= 1;
    }

    s32 result;
    do
    {
        result = DecodeSlice(sys, macroblocks);
    } while (result == SliceGivenUp || result == NextStartCodeFound);

    WaitIpuIdleIfBusy(sys);
    WaitIpuIdleIfBusy(sys);
    if (!FinishIpuOutput(sys))
    {
        if (sys->aborted != 0)
        {
            return DecodingAborted;
        }

        result = PictureGivenUp;
    }

    // The last macroblock's references fetched, it's finished
    while ((*R_EE_D9_CHCR & ChcrStart) != 0)
    {
    }

    if (result == SliceEnded)
    {
        StoreMacroblock(sys, sys->bufferIndex == 0);
    }

    if (result == SliceGivenUp || result == PictureGivenUp)
    {
        Error(sys, g_MpegSkipToNextPicture);
    }

    return result == SliceEnded;
}

s32 DecodeSlice(MpegSystem* sys, s32 macroblocks)
{
    s32 address = 0;
    s32 increment = 0;
    MpegVector predictors[2][2];
    s32 fieldSelect[2][2];
    s32 dmVector[2];
    s32 type;
    s32 motionType;
    s32 dctType;
    s32 result = ReadSliceHeader(sys, macroblocks, &address, &increment, predictors);
    if (result != 0)
    {
        return result;
    }

    sys->macroblockError = 0;
    while (address < macroblocks)
    {
        sys->buffers[sys->bufferIndex].noResidual = 0;
        WaitIpuIdleIfBusy(sys);
        if (!FinishIpuOutput(sys))
        {
            return sys->aborted != 0 ? DecodingAborted : PictureGivenUp;
        }

        if (increment == 0)
        {
            // A start code ends the slice
            if (PeekBits(sys, StartCodeZeros) == 0 || sys->macroblockError != 0)
            {
                sys->macroblockError = 0;
                return NextStartCodeFound;
            }

            increment = MacroblockAddressIncrement(sys);
            if (sys->macroblockError != 0)
            {
                sys->macroblockError = 0;
                return SliceGivenUp;
            }
        }

        if (address >= macroblocks)
        {
            Error(sys, g_MpegTooManyMacroblocks);
            return PictureGivenUp;
        }

        // The macroblocks an increment skips are motion compensated without blocks
        if (increment == 1)
        {
            if (DecodeMacroblock(sys, &type, &motionType, &dctType, predictors, fieldSelect, dmVector) == 0)
            {
                sys->macroblockError = 0;
                return SliceGivenUp;
            }
        }
        else if (SkipMacroblock(sys, predictors, &motionType, fieldSelect, &type) == 0)
        {
            sys->macroblockError = 0;
            return PictureGivenUp;
        }

        if (MotionCompensate(sys, address, increment, type, motionType, predictors, fieldSelect, dmVector) == 0)
        {
            sys->macroblockError = 0;
            return PictureGivenUp;
        }

        // The macroblock before is finished while the IPU decodes this one
        if (address != 0)
        {
            StoreMacroblock(sys, sys->bufferIndex ^ 1);
        }

        address++;
        sys->bufferIndex ^= 1;
        increment--;
    }

    return SliceEnded;
}

s32 DecodeMacroblock(MpegSystem* sys, s32* type, s32* motionType, s32* dctType, MpegVector predictors[2][2],
                     s32 fieldSelect[2][2], s32* dmVector)
{
    *IpuControl = (*IpuControl & ~IpuControlPictureType) | (sys->pictureCodingType << IpuControlPictureTypeShift);
    s32 code = DecodeVariable(sys, IpuMacroblockTypeTable).decoded;
    *type = code;
    if (code == 0)
    {
        Error(sys, g_MpegInvalidMacroblockType);
        sys->macroblockError = 1;
        return 0;
    }

    MacroblockType macroblock = {static_cast<u32>(code)};
    if (macroblock.forward || macroblock.backward)
    {
        if (sys->pictureStructure == MpegFrame && sys->framePredFrameDct != 0)
        {
            *motionType = MpegMotionFrame;
        }
        else
        {
            *motionType = NextBits(sys, 2);
        }
    }
    else if (macroblock.intra && sys->concealmentMotionVectors != 0)
    {
        *motionType = sys->pictureStructure == MpegFrame ? MpegMotionFrame : MpegMotionField;
    }

    // MPEG-2's motion_vector_count, mv_format (frame vectors or field ones) and dmv; a field vector in a frame picture has half
    // the frame's predictor
    s32 structure = sys->pictureStructure;
    s32 motion = *motionType;
    s32 vectorCount;
    s32 frameVectors;
    if (structure == MpegFrame)
    {
        vectorCount = motion == MpegMotionField ? 2 : 1;
        frameVectors = motion == MpegMotionFrame;
    }
    else
    {
        vectorCount = motion == MpegMotion16x8 ? 2 : 1;
        frameVectors = 0;
    }

    s32 dualPrime = motion == MpegMotionDualPrime;
    s32 halveVertical = frameVectors == 0 && structure == MpegFrame;
    if (structure == MpegFrame && sys->framePredFrameDct == 0 && (macroblock.intra || macroblock.pattern))
    {
        *dctType = NextBits(sys, 1);
    }
    else
    {
        *dctType = 0;
    }

    if (macroblock.quantiser)
    {
        sys->quantiserScale = NextBits(sys, 5);
    }

    if (macroblock.forward || (macroblock.intra && sys->concealmentMotionVectors != 0))
    {
        if (sys->mpeg2 != 0)
        {
            ReadMotionVectors(sys, predictors, dmVector, fieldSelect, 0, vectorCount, frameVectors, sys->fCodes[0][0] - 1,
                              sys->fCodes[0][1] - 1, dualPrime, halveVertical);
        }
        else
        {
            ReadMotionVector(sys, &predictors[0][0], dmVector, sys->forwardFCode - 1, sys->forwardFCode - 1, 0, 0,
                             sys->fullPelForwardVector);
        }
    }

    if (sys->macroblockError != 0)
    {
        return 0;
    }

    if (macroblock.backward)
    {
        if (sys->mpeg2 != 0)
        {
            ReadMotionVectors(sys, predictors, dmVector, fieldSelect, 1, vectorCount, frameVectors, sys->fCodes[1][0] - 1,
                              sys->fCodes[1][1] - 1, 0, halveVertical);
        }
        else
        {
            ReadMotionVector(sys, &predictors[0][1], dmVector, sys->backwardFCode - 1, sys->backwardFCode - 1, 0, 0,
                             sys->fullPelBackwardVector);
        }

        if (sys->macroblockError != 0)
        {
            return 0;
        }
    }

    // The marker bit after the concealment vectors
    if (macroblock.intra && sys->concealmentMotionVectors != 0)
    {
        SkipMacroblockBits(sys, 1);
    }

    if (macroblock.intra || macroblock.pattern)
    {
        ReceiveFromIpu(sys->buffers[sys->bufferIndex].macroblocks, DecodedMacroblockBytes);
        WaitIpuIdleIfBusy(sys);
        SetIpuCommand(sys, IpuDecodeBlock | (sys->quantiserScale << IpuBlockQuantiserShift) |
                               (macroblock.intra << IpuBlockIntraShift) | (*dctType << IpuBlockDctTypeShift) |
                               (sys->dcReset << IpuBlockDcResetShift));
    }
    else
    {
        sys->buffers[sys->bufferIndex].noResidual = 1;
    }

    sys->dcReset = 0;
    if (sys->macroblockError != 0)
    {
        return 0;
    }

    if (!macroblock.intra)
    {
        sys->dcReset = 1;
    }

    // An intra macroblock without concealment vectors resets the predictors
    if (macroblock.intra && sys->concealmentMotionVectors == 0)
    {
        predictors[0][0] = {};
        predictors[0][1] = {};
        predictors[1][0] = {};
        predictors[1][1] = {};
    }

    // A P picture's macroblock without vectors isn't moved
    if (sys->pictureCodingType == MpegPictureP && !macroblock.intra && !macroblock.forward)
    {
        predictors[0][0] = {};
        predictors[1][0] = {};
        if (sys->pictureStructure == MpegFrame)
        {
            *motionType = MpegMotionFrame;
        }
        else
        {
            *motionType = MpegMotionField;
            fieldSelect[0][0] = sys->pictureStructure == MpegBottomField;
        }
    }

    return 1;
}

s32 MacroblockAddressIncrement(MpegSystem* sys)
{
    s32 increment = 0;
    while (true)
    {
        s32 code = DecodeVariable(sys, IpuAddressIncrementTable).decoded;
        if (code == AddressIncrementStuffing)
        {
            continue;
        }

        if (code == AddressIncrementEscape)
        {
            increment += AddressIncrementEscaped;
            continue;
        }

        if (code != 0)
        {
            return increment + code;
        }

        u32 next = PeekBits(sys, Mpeg2StuffingBits);
        if (sys->mpeg2 == 0 || next != Mpeg2Stuffing)
        {
            ErrorValue(sys, g_MpegInvalidAddressIncrement, code);
            sys->macroblockError = 1;
            return 1;
        }

        SkipMacroblockBits(sys, Mpeg2StuffingBits);
    }
}

s32 SkipMacroblock(MpegSystem* sys, MpegVector predictors[2][2], s32* motionType, s32 fieldSelect[2][2], s32* type)
{
    sys->buffers[sys->bufferIndex].noResidual = 1;
    sys->dcReset = 1;
    // A P picture's skipped macroblocks aren't moved, a B picture's take the vectors before
    if (sys->pictureCodingType == MpegPictureP)
    {
        predictors[0][0] = {};
        predictors[1][0] = {};
    }

    if (sys->pictureStructure == MpegFrame)
    {
        *motionType = MpegMotionFrame;
    }
    else
    {
        *motionType = MpegMotionField;
        fieldSelect[0][0] = fieldSelect[0][1] = sys->pictureStructure == MpegBottomField;
    }

    s32 skipped = 1;
    if (sys->pictureCodingType == MpegPictureI)
    {
        Error(sys, g_MpegSkippedInIPicture);
        skipped = 0;
    }

    MacroblockType macroblock = {static_cast<u32>(*type)};
    macroblock.intra = 0;
    *type = macroblock.value;
    return skipped;
}

void ReadMotionVector(MpegSystem* sys, MpegVector* vector, s32* dmVector, s32 horizontalSize, s32 verticalSize, s32 dualPrime,
                      s32 halveVertical, s32 fullPel)
{
    s32 code = DecodeVariable(sys, IpuMotionCodeTable).decoded;
    s32 residual = horizontalSize != 0 && code != 0 ? NextBits(sys, horizontalSize) : 0;
    vector->horizontal = DecodeComponent(vector->horizontal, code, residual, horizontalSize, fullPel);
    if (dualPrime != 0)
    {
        dmVector[0] = DecodeVariable(sys, IpuDmVectorTable).decoded;
    }

    code = DecodeVariable(sys, IpuMotionCodeTable).decoded;
    residual = verticalSize != 0 && code != 0 ? NextBits(sys, verticalSize) : 0;
    if (halveVertical != 0)
    {
        vector->vertical >>= 1;
    }

    vector->vertical = DecodeComponent(vector->vertical, code, residual, verticalSize, fullPel);
    if (halveVertical != 0)
    {
        vector->vertical <<= 1;
    }

    if (dualPrime != 0)
    {
        dmVector[1] = DecodeVariable(sys, IpuDmVectorTable).decoded;
    }
}

void ReadMotionVectors(MpegSystem* sys, MpegVector predictors[2][2], s32* dmVector, s32 fieldSelect[2][2], s32 backward,
                       s32 vectorCount, s32 frameVectors, s32 horizontalSize, s32 verticalSize, s32 dualPrime,
                       s32 halveVertical)
{
    if (vectorCount == 1)
    {
        if (frameVectors == 0 && dualPrime == 0)
        {
            s32 field = NextBits(sys, 1);
            fieldSelect[1][backward] = field;
            fieldSelect[0][backward] = field;
        }

        ReadMotionVector(sys, &predictors[0][backward], dmVector, horizontalSize, verticalSize, dualPrime, halveVertical, 0);
        predictors[1][backward] = predictors[0][backward];
        return;
    }

    fieldSelect[0][backward] = NextBits(sys, 1);
    ReadMotionVector(sys, &predictors[0][backward], dmVector, horizontalSize, verticalSize, dualPrime, halveVertical, 0);
    fieldSelect[1][backward] = NextBits(sys, 1);
    ReadMotionVector(sys, &predictors[1][backward], dmVector, horizontalSize, verticalSize, dualPrime, halveVertical, 0);
}

void ReceiveFromIpu(u8* address, s32 size)
{
    s32 interrupts = DIntr();
    u32 dmaAddress = reinterpret_cast<u32>(address);
    if (dmaAddress >> SegmentShift == Scratchpad >> SegmentShift)
    {
        dmaAddress = (dmaAddress & PhysicalMask) | DmaScratchpad;
    }
    else
    {
        dmaAddress &= PhysicalMask;
    }

    *R_EE_D3_MADR = dmaAddress;
    *R_EE_D3_QWC = size >> 4;
    *R_EE_D3_CHCR = ChcrStart;
    if (interrupts != 0)
    {
        EIntr();
    }
}
}
