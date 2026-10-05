#include "mpeg.h"

// libmpeg's motion compensation: a macroblock's prediction made from blocks of the reference pictures, their macroblocks fetched
// into the scratchpad by the toSPR channel and read by hand-written MMI routines into 16 bit pixels, then stored into the picture
// with the IPU's blocks added

extern "C"
{
    extern const char g_MpegInvalidMotionType0[] RETAIL(D_00307600);
    extern const char g_MpegInvalidMotionType1[] RETAIL(D_00307620);
    extern const char g_MpegInvalidMotionType2[] RETAIL(D_00307640);
    extern const char g_MpegMotionTypeIgnored[] RETAIL(D_00307660);
    extern const char g_MpegIntraWithoutBlocks[] RETAIL(D_00307688);
    // The prediction routines: the luma's 8, then the chroma's
    extern MpegPredictionRoutine g_MpegPredictionRoutines[16] RETAIL(jtbl_002E8238);
    // The toSPR channel's chain fetching the references' macroblocks: two tags a reference
    extern u64 g_MpegReferenceChain[0x40] RETAIL(D_003C6F80);
}

namespace Libmpeg
{
namespace
{
constexpr s32 ChromaRoutines = 8;
// A reference's macroblocks in the scratchpad: the column of two its block starts in, then the column to its right
constexpr u32 ColumnQuadwords = 2 * MacroblockQuadwords;
constexpr u32 ColumnBytes = ColumnQuadwords * 16;
// A macroblock's luma: 16 rows of 16 pixels (a byte each), then its chroma's Cb and Cr: 8 rows of 8 each
constexpr s32 LumaShift = 4;
constexpr s32 LumaRows = 1 << LumaShift;
constexpr s32 LumaRowBytes = 1 << LumaShift;
constexpr s32 LumaBytes = LumaRows * LumaRowBytes;
constexpr s32 ChromaShift = 3;
constexpr s32 ChromaRows = 1 << ChromaShift;
constexpr s32 ChromaRowBytes = 1 << ChromaShift;
// The prediction: the luma's rows of 16 bit pixels (32 bytes), then the chroma's (16 bytes)
constexpr s32 PredictionLumaRowShift = 5;
constexpr s32 PredictionChromaRowShift = 4;
constexpr s32 PredictionChroma = LumaRows << PredictionLumaRowShift;

// (vector * distance + (vector > 0)) / 2: a vector scaled to a field one or three away, rounded away from zero
s32 DualPrimeScaled(s32 vector, s32 distance)
{
    return (vector * distance + (vector > 0 ? 1 : 0)) >> 1;
}

// The rows of a block from the macroblock it starts in (rows at most), the rest from the one below: with a half pixel vertical
// vector one more row is read
void SplitRows(MpegPredictionBlock* block, s32 row, s32 rows, s32 macroblockRows, s32 halfDown, s32 fieldPrediction)
{
    if (row + (rows << fieldPrediction) <= macroblockRows - halfDown)
    {
        block->rows = rows;
        block->rowsBelow = 0;
        return;
    }

    s32 above = (macroblockRows >> fieldPrediction) - (row >> fieldPrediction) - halfDown;
    block->rows = above;
    block->rowsBelow = rows - above;
}

void WaitScratchpadDma()
{
    while ((*R_EE_D9_CHCR & ChcrStart) != 0)
    {
    }
}
}

void DualPrimeVectors(MpegSystem* sys, MpegVector* vectors, const s32* dmVector, s32 vectorX, s32 vectorY)
{
    // MPEG-2's 7.6.3.6: the vector scaled to the other parity's field, plus the differential, half a line up or down
    if (sys->pictureStructure == MpegFrame)
    {
        if (sys->topFieldFirst != 0)
        {
            vectors[0].horizontal = DualPrimeScaled(vectorX, 1) + dmVector[0];
            vectors[0].vertical = DualPrimeScaled(vectorY, 1) + dmVector[1] - 1;
            vectors[1].horizontal = DualPrimeScaled(vectorX, 3) + dmVector[0];
            vectors[1].vertical = DualPrimeScaled(vectorY, 3) + dmVector[1] + 1;
        }
        else
        {
            vectors[0].horizontal = DualPrimeScaled(vectorX, 3) + dmVector[0];
            vectors[0].vertical = DualPrimeScaled(vectorY, 3) + dmVector[1] - 1;
            vectors[1].horizontal = DualPrimeScaled(vectorX, 1) + dmVector[0];
            vectors[1].vertical = DualPrimeScaled(vectorY, 1) + dmVector[1] + 1;
        }

        return;
    }

    vectors[0].horizontal = DualPrimeScaled(vectorX, 1) + dmVector[0];
    s32 vertical = DualPrimeScaled(vectorY, 1) + dmVector[1];
    vectors[0].vertical = sys->pictureStructure == MpegTopField ? vertical - 1 : vertical + 1;
}

void GetReferences(MpegSystem* sys, s32 x, s32 y, s32 type, s32 motionType, MpegVector predictors[2][2], s32 fieldSelect[2][2],
                   s32* dmVector)
{
    sys->buffers[sys->bufferIndex].referenceCount = 0;
    MacroblockType macroblock = {static_cast<u32>(type)};
    // A backward prediction is averaged with the forward one
    s32 forward = 0;
    if (macroblock.forward || sys->pictureCodingType == MpegPictureP)
    {
        MpegVector* first = &predictors[0][0];
        MpegVector* second = &predictors[1][0];
        if (sys->pictureStructure == MpegFrame)
        {
            // A P picture's macroblock without vectors has frame prediction
            if (motionType == MpegMotionFrame || !macroblock.forward)
            {
                GetReference(sys, sys->frames[MpegPast], 0, 0, 0, 16, x, y, first->horizontal, first->vertical, 0, 0);
            }
            else if (motionType == MpegMotionField)
            {
                GetReference(sys, sys->frames[MpegPast], fieldSelect[0][0], 0, 0, 8, x, y, first->horizontal, first->vertical >> 1,
                             1, 0);
                GetReference(sys, sys->frames[MpegPast], fieldSelect[1][0], 1, 0, 8, x, y, second->horizontal,
                             second->vertical >> 1, 1, 0);
            }
            else if (motionType == MpegMotionDualPrime)
            {
                // Each field from the field of its parity averaged with the other's
                MpegVector vectors[2];
                DualPrimeVectors(sys, vectors, dmVector, first->horizontal, first->vertical >> 1);
                GetReference(sys, sys->frames[MpegPast], 0, 0, 0, 8, x, y, first->horizontal, first->vertical >> 1, 1, 0);
                GetReference(sys, sys->frames[MpegPast], 1, 0, 0, 8, x, y, vectors[0].horizontal, vectors[0].vertical, 1, 1);
                GetReference(sys, sys->frames[MpegPast], 1, 1, 0, 8, x, y, first->horizontal, first->vertical >> 1, 1, 0);
                GetReference(sys, sys->frames[MpegPast], 0, 1, 0, 8, x, y, vectors[1].horizontal, vectors[1].vertical, 1, 1);
            }
            else
            {
                ErrorValue(sys, g_MpegInvalidMotionType0, motionType);
            }
        }
        else
        {
            // A field picture's references are fields: the past frame's, and in a P picture's second field the frame's first
            // field when it's of the other parity
            s32 bottom = sys->pictureStructure == MpegBottomField;
            MpegImage* fields[2][2] = {{sys->topFields[MpegPast], sys->bottomFields[MpegPast]},
                                       {sys->topFields[MpegFuture], sys->bottomFields[MpegFuture]}};
            s32 sameFrame = 0;
            if (sys->pictureCodingType == MpegPictureP && sys->secondFieldMissing != 0)
            {
                sameFrame = (bottom ^ fieldSelect[0][0]) != 0;
            }

            if (motionType == MpegMotionField || !macroblock.forward)
            {
                GetReference(sys, fields[sameFrame][fieldSelect[0][0]], 0, 0, 0, 16, x, y, first->horizontal, first->vertical,
                             0, 0);
            }
            else if (motionType == MpegMotion16x8)
            {
                GetReference(sys, fields[sameFrame][fieldSelect[0][0]], 0, 0, 0, 8, x, y, first->horizontal, first->vertical, 0,
                             0);
                sameFrame = 0;
                if (sys->pictureCodingType == MpegPictureP && sys->secondFieldMissing != 0)
                {
                    sameFrame = (bottom ^ fieldSelect[1][0]) != 0;
                }

                GetReference(sys, fields[sameFrame][fieldSelect[1][0]], 0, 0, 8, 8, x, y, second->horizontal, second->vertical,
                             0, 0);
            }
            else if (motionType == MpegMotionDualPrime)
            {
                sameFrame = sys->secondFieldMissing != 0;
                MpegVector vectors[2];
                DualPrimeVectors(sys, vectors, dmVector, first->horizontal, first->vertical);
                GetReference(sys, fields[0][bottom], 0, 0, 0, 16, x, y, first->horizontal, first->vertical, 0, 0);
                GetReference(sys, fields[sameFrame][bottom == 0], 0, 0, 0, 16, x, y, vectors[0].horizontal, vectors[0].vertical,
                             0, 1);
            }
            else
            {
                ErrorValue(sys, g_MpegInvalidMotionType1, motionType);
            }
        }

        forward = 1;
    }

    if (!macroblock.backward)
    {
        return;
    }

    MpegVector* first = &predictors[0][1];
    MpegVector* second = &predictors[1][1];
    if (sys->pictureStructure == MpegFrame)
    {
        if (motionType == MpegMotionFrame)
        {
            GetReference(sys, sys->frames[MpegFuture], 0, 0, 0, 16, x, y, first->horizontal, first->vertical, 0, forward);
        }
        else
        {
            // Field prediction, and so is any other motion type (B pictures have no dual prime)
            GetReference(sys, sys->frames[MpegFuture], fieldSelect[0][1], 0, 0, 8, x, y, first->horizontal,
                         first->vertical >> 1, 1, forward);
            GetReference(sys, sys->frames[MpegFuture], fieldSelect[1][1], 1, 0, 8, x, y, second->horizontal,
                         second->vertical >> 1, 1, forward);
        }
    }
    else if (motionType == MpegMotionField)
    {
        GetReference(sys, fieldSelect[0][1] != 0 ? sys->bottomFields[MpegFuture] : sys->topFields[MpegFuture], 0, 0, 0, 16, x, y,
                     first->horizontal, first->vertical, 0, forward);
    }
    else if (motionType == MpegMotion16x8)
    {
        GetReference(sys, fieldSelect[0][1] != 0 ? sys->bottomFields[MpegFuture] : sys->topFields[MpegFuture], 0, 0, 0, 8, x, y,
                     first->horizontal, first->vertical, 0, forward);
        GetReference(sys, fieldSelect[1][1] != 0 ? sys->bottomFields[MpegFuture] : sys->topFields[MpegFuture], 0, 0, 8, 8, x, y,
                     second->horizontal, second->vertical, 0, forward);
    }
    else
    {
        ErrorValue(sys, g_MpegInvalidMotionType2, motionType);
    }
}

s32 MotionCompensate(MpegSystem* sys, s32 address, s32 increment, s32 type, s32 motionType, MpegVector predictors[2][2],
                     s32 fieldSelect[2][2], s32* dmVector)
{
    MacroblockType macroblock = {static_cast<u32>(type)};
    s32 intra = macroblock.intra;
    s32 row = address / sys->widthMacroblocks;
    s32 column = address % sys->widthMacroblocks;
    if (intra != 0)
    {
        WaitScratchpadDma();
        sys->buffers[sys->bufferIndex].predicted = 0;
    }
    else
    {
        if (static_cast<u32>(motionType - MpegMotionField) > MpegMotionDualPrime - MpegMotionField)
        {
            ErrorValue(sys, g_MpegMotionTypeIgnored, motionType);
            sys->macroblockError = 1;
            return 0;
        }

        GetReferences(sys, column << LumaShift, row << LumaShift, type, motionType, predictors, fieldSelect, dmVector);
        // The macroblock before's references are in the scratchpad until it's finished
        WaitScratchpadDma();
        FetchReferences(sys);
        sys->buffers[sys->bufferIndex].predicted = 1;
    }

    sys->buffers[sys->bufferIndex].unused134 = increment == 1 && macroblock.pattern;
    sys->buffers[sys->bufferIndex].intra = intra;
    MpegImage* image;
    if (sys->pictureStructure == MpegFrame)
    {
        image = sys->frames[MpegCurrent];
    }
    else
    {
        image = sys->pictureStructure == MpegBottomField ? sys->bottomFields[MpegCurrent] : sys->topFields[MpegCurrent];
    }

    sys->buffers[sys->bufferIndex].destination = image->pixels + (column * image->heightMacroblocks + row) * MacroblockBytes;
    return 1;
}

void GetReference(MpegSystem* sys, MpegImage* image, s32 field, s32 destinationField, s32 destinationRow, s32 height, s32 x,
                  s32 y, s32 vectorX, s32 vectorY, s32 fieldPrediction, s32 average)
{
    MpegMacroblockBuffers* buffer = &sys->buffers[sys->bufferIndex];
    s32 reference = buffer->referenceCount;
    u8* prediction = sys->prediction;
    MpegPredictionBlock* luma = &buffer->luma[reference];
    MpegPredictionBlock* chroma = &buffer->chroma[reference];

    // The luma's block, in pixels (a field's rows are every other one of the frame's)
    HalfPixels across = {vectorX};
    HalfPixels downwards = {vectorY};
    s32 lumaX = x + across.whole;
    s32 down = downwards.whole;
    if (fieldPrediction != 0)
    {
        down <<= 1;
    }

    s32 lumaY = y + down + destinationRow + field;
    s32 column = lumaX >> LumaShift;
    s32 row = lumaY >> LumaShift;
    s32 index = column * image->heightMacroblocks + row;
    u8* source = image->pixels + index * MacroblockBytes;
    u8* rightSource = image->pixels + (index + image->heightMacroblocks) * MacroblockBytes;
    s32 lumaRow = lumaY - (row << LumaShift);
    s32 halfAcross = across.half;
    s32 halfDown = downwards.half;
    luma->prediction = prediction + ((destinationField + destinationRow) << PredictionLumaRowShift);
    luma->column = lumaX - (column << LumaShift);
    buffer->sources[reference] = source;
    buffer->rightSources[reference] = rightSource;
    SplitRows(luma, lumaRow, height, LumaRows, halfDown, fieldPrediction);
    u8* fetched;
    if (sys->bufferType == MpegBuffersScratchpad)
    {
        fetched = buffer->references + reference * FetchedReferenceBytes;
        luma->left = fetched + lumaRow * LumaRowBytes;
        luma->right = fetched + lumaRow * LumaRowBytes + ColumnBytes;
    }
    else
    {
        fetched = source;
        luma->left = source + lumaRow * LumaRowBytes;
        luma->right = rightSource + lumaRow * LumaRowBytes;
    }

    // The chroma's, of half the size and half the vector (rounded towards zero)
    HalfPixels chromaAcross = {vectorX / 2};
    HalfPixels chromaDownwards = {vectorY / 2};
    luma->stride = LumaRowBytes << fieldPrediction;
    s32 chromaX = chromaAcross.whole + (x >> 1);
    s32 chromaDown = chromaDownwards.whole;
    if (fieldPrediction != 0)
    {
        chromaDown <<= 1;
    }

    s32 chromaY = chromaDown + (y >> 1) + (destinationRow >> 1) + field;
    s32 chromaColumn = chromaX >> ChromaShift;
    s32 chromaRow = chromaY >> ChromaShift;
    s32 chromaRowInBlock = chromaY - (chromaRow << ChromaShift);
    chroma->column = chromaX - (chromaColumn << ChromaShift);
    chroma->prediction =
        prediction + ((destinationField + (destinationRow >> 1)) << PredictionChromaRowShift) + PredictionChroma;
    s32 chromaHalfAcross = chromaAcross.half;
    s32 chromaHalfDown = chromaDownwards.half;
    SplitRows(chroma, chromaRowInBlock, height >> 1, ChromaRows, chromaHalfDown, fieldPrediction);
    // The chroma's macroblock among the four fetched (it can be right of the luma's)
    s32 offset = ((chromaColumn - column) * 2 + (chromaRow - row)) * MacroblockBytes;
    if (sys->bufferType == MpegBuffersScratchpad)
    {
        chroma->left = fetched + offset + chromaRowInBlock * ChromaRowBytes + LumaBytes;
        chroma->right = fetched + offset + chromaRowInBlock * ChromaRowBytes + ColumnBytes + LumaBytes;
    }
    else
    {
        u32 left = offset + chromaRowInBlock * ChromaRowBytes + LumaBytes;
        if (left <= ColumnBytes)
        {
            chroma->left = source + left;
        }
        else
        {
            chroma->left = rightSource + left - ColumnBytes;
        }

        chroma->right = rightSource + offset + chromaRowInBlock * ChromaRowBytes + LumaBytes;
    }

    chroma->stride = ChromaRowBytes << fieldPrediction;
    buffer->lumaRoutines[reference] = g_MpegPredictionRoutines[average << 2 | halfAcross << 1 | halfDown];
    sys->buffers[sys->bufferIndex].referenceCount++;
    buffer->chromaRoutines[reference] =
        g_MpegPredictionRoutines[ChromaRoutines + (average << 2 | chromaHalfAcross << 1 | chromaHalfDown)];
}

void StoreMacroblock(MpegSystem* sys, s32 index)
{
    MpegMacroblockBuffers* buffer = &sys->buffers[index];
    if (buffer->predicted != 0)
    {
        for (s32 reference = 0; reference < buffer->referenceCount; reference++)
        {
            buffer->lumaRoutines[reference](&buffer->luma[reference]);
            buffer->chromaRoutines[reference](&buffer->chroma[reference]);
        }
    }

    if (buffer->intra != 0 && buffer->noResidual != 0)
    {
        Error(sys, g_MpegIntraWithoutBlocks);
    }

    if (buffer->intra != 0)
    {
        StorePixels(buffer->destination, reinterpret_cast<s16*>(buffer->macroblocks));
    }
    else if (buffer->noResidual != 0)
    {
        StorePixels(buffer->destination, reinterpret_cast<s16*>(sys->prediction));
    }
    else
    {
        AddPrediction(buffer->destination, reinterpret_cast<s16*>(sys->prediction), reinterpret_cast<s16*>(buffer->macroblocks));
    }
}

void FetchReferences(MpegSystem* sys)
{
    if (sys->bufferType != MpegBuffersScratchpad)
    {
        return;
    }

    MpegMacroblockBuffers* buffer = &sys->buffers[sys->bufferIndex];
    s32 count = buffer->referenceCount;
    // The chain's written uncached, the DMA reads it from memory: for each reference the column of two macroblocks its block
    // starts in and the column to its right
    u64* tag = reinterpret_cast<u64*>((reinterpret_cast<u32>(g_MpegReferenceChain) & PhysicalMask) | UncachedSegment);
    for (s32 reference = 0; reference < count; reference++)
    {
        DmaTag column = {};
        column.quadwords = ColumnQuadwords;
        column.id = DMA_TAG_REF;
        column.address = reinterpret_cast<u32>(buffer->sources[reference]) & PhysicalMask;
        DmaTag rightColumn = {};
        rightColumn.quadwords = ColumnQuadwords;
        rightColumn.id = reference == count - 1 ? DMA_TAG_REFE : DMA_TAG_REF;
        rightColumn.address = reinterpret_cast<u32>(buffer->rightSources[reference]) & PhysicalMask;
        tag[0] = column.value;
        tag[2] = rightColumn.value;
        tag += 4;
    }

    s32 interrupts = DIntr();
    asm volatile("sync" : : : "memory");
    *R_EE_D9_SADR = reinterpret_cast<u32>(sys->buffers[sys->bufferIndex].references);
    *R_EE_D9_TADR = reinterpret_cast<u32>(g_MpegReferenceChain);
    *R_EE_D9_QWC = 0;
    *R_EE_D9_CHCR = ChcrFromMemory | ChcrChain | ChcrStart;
    if (interrupts != 0)
    {
        EIntr();
    }
}

// The prediction routines (the table's), hand-written MMI kept as retail's instructions: a block's rows read from the left and right
// macroblocks' rows a quadword at a time (luma) or a doubleword (chroma), shifted into place by the block's column (qfsrv), widened
// to 16 bit pixels; halves of pixels are (a + b + 1) / 2, quarters (a + b + c + d + 2) / 4, averages with the prediction
// (p + q + 1) / 2. They use the caller-saved registers and SA, and don't change a0
void PredictLuma(const MpegPredictionBlock* block) RETAIL(func_002BF748);
void PredictLumaHalfDown(const MpegPredictionBlock* block) RETAIL(func_002BF858);
void PredictLumaHalfAcross(const MpegPredictionBlock* block) RETAIL(func_002BF9E0);
void PredictLumaHalfBoth(const MpegPredictionBlock* block) RETAIL(func_002BFB48);
void AverageLuma(const MpegPredictionBlock* block) RETAIL(func_002BFD40);
void AverageLumaHalfDown(const MpegPredictionBlock* block) RETAIL(func_002BFE90);
void AverageLumaHalfAcross(const MpegPredictionBlock* block) RETAIL(func_002C0058);
void AverageLumaHalfBoth(const MpegPredictionBlock* block) RETAIL(func_002C0200);
void PredictChroma(const MpegPredictionBlock* block) RETAIL(func_002BF7C0);
void PredictChromaHalfDown(const MpegPredictionBlock* block) RETAIL(func_002BF910);
void PredictChromaHalfAcross(const MpegPredictionBlock* block) RETAIL(func_002BFA90);
void PredictChromaHalfBoth(const MpegPredictionBlock* block) RETAIL(func_002BFC40);
void AverageChroma(const MpegPredictionBlock* block) RETAIL(func_002BFDE0);
void AverageChromaHalfDown(const MpegPredictionBlock* block) RETAIL(func_002BFF70);
void AverageChromaHalfAcross(const MpegPredictionBlock* block) RETAIL(func_002C0130);
void AverageChromaHalfBoth(const MpegPredictionBlock* block) RETAIL(func_002C0320);

// The luma's rows as they are
void PredictLuma(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $a4, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "mtsab $a4, 0x0\n"
                 "sll $a7, $t0, 1\n"
                 "addiu $t3, $zero, -0x1\n"
                 ".L002BF76C_%=:\n"
                 "lq $a4, 0x0($a1)\n"
                 "lq $a5, 0x0($a2)\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $t0\n"
                 "addu $a2, $a2, $t0\n"
                 "qfsrv $a6, $a5, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "pextub $a5, $zero, $a6\n"
                 "sq $a4, 0x0($t2)\n"
                 "sq $a5, 0x10($t2)\n"
                 "bgtz $a3, .L002BF76C_%=\n"
                 "addu $t2, $t2, $a7\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $t3, $a3\n"
                 "bnez $a6, .L002BF76C_%=\n"
                 "daddu $t3, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each luma pixel the average of its row's and the next row's
void PredictLumaHalfDown(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t8, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "lq $a4, 0x0($a1)\n"
                 "sll $t0, $t8, 1\n"
                 "lq $a5, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a6, $a5, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "addiu $a7, $zero, -0x1\n"
                 "beqz $a3, .L002BF8EC_%=\n"
                 "pextub $a5, $zero, $a6\n"
                 ".L002BF89C_%=:\n"
                 "addu $a1, $a1, $t8\n"
                 "addu $a2, $a2, $t8\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "paddh $v0, $a4, $a6\n"
                 "paddh $v1, $a5, $t3\n"
                 "por $a4, $a6, $zero\n"
                 "por $a5, $t3, $zero\n"
                 "paddh $v0, $v0, $t9\n"
                 "paddh $v1, $v1, $t9\n"
                 "psrlh $v0, $v0, 1\n"
                 "psrlh $v1, $v1, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sq $v1, 0x10($t2)\n"
                 "bgtz $a3, .L002BF89C_%=\n"
                 "addu $t2, $t2, $t0\n"
                 ".L002BF8EC_%=:\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002BF89C_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each luma pixel the average of it and the next one across
void PredictLumaHalfAcross(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $a5, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $t8, $zero, 0x1\n"
                 "sll $a4, $a5, 1\n"
                 "addiu $a7, $zero, -0x1\n"
                 ".L002BFA0C_%=:\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "qfsrv $v1, $a6, $t3\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $v1, $v1, $v0\n"
                 "pextlb $v0, $zero, $v1\n"
                 "pextub $v1, $zero, $v1\n"
                 "paddh $a6, $a6, $v0\n"
                 "paddh $t3, $t3, $v1\n"
                 "paddh $v0, $a6, $t9\n"
                 "paddh $v1, $t3, $t9\n"
                 "psrlh $v0, $v0, 1\n"
                 "psrlh $v1, $v1, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sq $v1, 0x10($t2)\n"
                 "addu $a1, $a1, $a5\n"
                 "addu $a2, $a2, $a5\n"
                 "bgtz $a3, .L002BFA0C_%=\n"
                 "addu $t2, $t2, $a4\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $t0, $a7, $a3\n"
                 "bnez $t0, .L002BFA0C_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each luma pixel the average of four, across and down
void PredictLumaHalfBoth(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "psllh $t9, $t9, 1\n"
                 "addiu $t8, $zero, 0x1\n"
                 "lq $a4, 0x0($a1)\n"
                 "lq $a5, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a6, $a5, $a4\n"
                 "qfsrv $t3, $a4, $a5\n"
                 "pextlb $a4, $zero, $a6\n"
                 "pextub $a5, $zero, $a6\n"
                 "addiu $a7, $zero, -0x1\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $t3, $t3, $a6\n"
                 "pextlb $a6, $zero, $t3\n"
                 "pextub $t3, $zero, $t3\n"
                 "paddh $a4, $a4, $a6\n"
                 "beqz $a3, .L002BFC20_%=\n"
                 "paddh $a5, $a5, $t3\n"
                 ".L002BFBAC_%=:\n"
                 "addu $a1, $a1, $t0\n"
                 "addu $a2, $a2, $t0\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "qfsrv $v1, $a6, $t3\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $v1, $v1, $v0\n"
                 "pextlb $v0, $zero, $v1\n"
                 "pextub $v1, $zero, $v1\n"
                 "paddh $a6, $a6, $v0\n"
                 "paddh $t3, $t3, $v1\n"
                 "paddh $v0, $a4, $a6\n"
                 "paddh $v1, $a5, $t3\n"
                 "por $a4, $a6, $zero\n"
                 "por $a5, $t3, $zero\n"
                 "paddh $v0, $v0, $t9\n"
                 "paddh $v1, $v1, $t9\n"
                 "psrlh $v0, $v0, 2\n"
                 "psrlh $v1, $v1, 2\n"
                 "sq $v0, 0x0($t2)\n"
                 "sll $a6, $t0, 1\n"
                 "sq $v1, 0x10($t2)\n"
                 "bgtz $a3, .L002BFBAC_%=\n"
                 "addu $t2, $t2, $a6\n"
                 ".L002BFC20_%=:\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002BFBAC_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// The luma's rows averaged with the prediction
void AverageLuma(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $a5, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "sll $a4, $a5, 1\n"
                 "addiu $a7, $zero, -0x1\n"
                 "mtsab $t1, 0x0\n"
                 ".L002BFD64_%=:\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "pextlb $a6, $zero, $v0\n"
                 "pextub $t3, $zero, $v0\n"
                 "lq $v0, 0x0($t2)\n"
                 "lq $v1, 0x10($t2)\n"
                 "paddh $v0, $v0, $a6\n"
                 "paddh $v1, $v1, $t3\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "paddh $a6, $v0, $t9\n"
                 "psrlh $v0, $a6, 1\n"
                 "paddh $a6, $v1, $t9\n"
                 "psrlh $v1, $a6, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sq $v1, 0x10($t2)\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $a5\n"
                 "addu $t2, $t2, $a4\n"
                 "bgtz $a3, .L002BFD64_%=\n"
                 "addu $a2, $a2, $a5\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $t0, $a7, $a3\n"
                 "bnez $t0, .L002BFD64_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictLumaHalfDown, averaged with the prediction
void AverageLumaHalfDown(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "lq $a4, 0x0($a1)\n"
                 "lq $a5, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a6, $a5, $a4\n"
                 "sll $t8, $t0, 1\n"
                 "pextlb $a4, $zero, $a6\n"
                 "addiu $a7, $zero, -0x1\n"
                 "beqz $a3, .L002BFF4C_%=\n"
                 "pextub $a5, $zero, $a6\n"
                 ".L002BFED4_%=:\n"
                 "addu $a1, $a1, $t0\n"
                 "addu $a2, $a2, $t0\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "paddh $v0, $a4, $a6\n"
                 "paddh $v1, $a5, $t3\n"
                 "por $a4, $a6, $zero\n"
                 "por $a5, $t3, $zero\n"
                 "paddh $v0, $v0, $t9\n"
                 "paddh $v1, $v1, $t9\n"
                 "psrlh $v0, $v0, 1\n"
                 "psrlh $v1, $v1, 1\n"
                 "lq $a6, 0x0($t2)\n"
                 "lq $t3, 0x10($t2)\n"
                 "paddh $v0, $v0, $a6\n"
                 "paddh $v1, $v1, $t3\n"
                 "paddh $a6, $v0, $t9\n"
                 "psrlh $v0, $a6, 1\n"
                 "pcgth $a6, $v1, $zero\n"
                 "psrlh $a6, $a6, 15\n"
                 "paddh $a6, $v1, $a6\n"
                 "psrlh $v1, $a6, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sq $v1, 0x10($t2)\n"
                 "bgtz $a3, .L002BFED4_%=\n"
                 "addu $t2, $t2, $t8\n"
                 ".L002BFF4C_%=:\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002BFED4_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictLumaHalfAcross, averaged with the prediction
void AverageLumaHalfAcross(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $a5, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $t8, $zero, 0x1\n"
                 "sll $a4, $a5, 1\n"
                 "addiu $a7, $zero, -0x1\n"
                 ".L002C0084_%=:\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "qfsrv $v1, $a6, $t3\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $v1, $v1, $v0\n"
                 "pextlb $v0, $zero, $v1\n"
                 "pextub $v1, $zero, $v1\n"
                 "paddh $a6, $a6, $v0\n"
                 "paddh $t3, $t3, $v1\n"
                 "paddh $v0, $a6, $t9\n"
                 "paddh $v1, $t3, $t9\n"
                 "psrlh $v0, $v0, 1\n"
                 "psrlh $v1, $v1, 1\n"
                 "lq $a6, 0x0($t2)\n"
                 "lq $t3, 0x10($t2)\n"
                 "paddh $v0, $v0, $a6\n"
                 "paddh $v1, $v1, $t3\n"
                 "paddh $a6, $v0, $t9\n"
                 "psrlh $v0, $a6, 1\n"
                 "pcgth $a6, $v1, $zero\n"
                 "psrlh $a6, $a6, 15\n"
                 "paddh $a6, $v1, $a6\n"
                 "psrlh $v1, $a6, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sq $v1, 0x10($t2)\n"
                 "addu $a1, $a1, $a5\n"
                 "addu $a2, $a2, $a5\n"
                 "bgtz $a3, .L002C0084_%=\n"
                 "addu $t2, $t2, $a4\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $t0, $a7, $a3\n"
                 "bnez $t0, .L002C0084_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictLumaHalfBoth, averaged with the prediction
void AverageLumaHalfBoth(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t8, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "lw $a3, 0x8($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "psllh $t9, $t9, 1\n"
                 "addiu $t0, $zero, 0x1\n"
                 "lq $a4, 0x0($a1)\n"
                 "lq $a5, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a6, $a5, $a4\n"
                 "qfsrv $t3, $a4, $a5\n"
                 "pextlb $a4, $zero, $a6\n"
                 "pextub $a5, $zero, $a6\n"
                 "addiu $a7, $zero, -0x1\n"
                 "mtsab $t0, 0x0\n"
                 "qfsrv $t3, $t3, $a6\n"
                 "pextlb $a6, $zero, $t3\n"
                 "pextub $t3, $zero, $t3\n"
                 "paddh $a4, $a4, $a6\n"
                 "beqz $a3, .L002C0300_%=\n"
                 "paddh $a5, $a5, $t3\n"
                 ".L002C0264_%=:\n"
                 "addu $a1, $a1, $t8\n"
                 "addu $a2, $a2, $t8\n"
                 "lq $a6, 0x0($a1)\n"
                 "lq $t3, 0x0($a2)\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $v0, $t3, $a6\n"
                 "qfsrv $v1, $a6, $t3\n"
                 "pextlb $a6, $zero, $v0\n"
                 "addi $a3, $a3, -0x1\n"
                 "pextub $t3, $zero, $v0\n"
                 "mtsab $t0, 0x0\n"
                 "qfsrv $v1, $v1, $v0\n"
                 "pextlb $v0, $zero, $v1\n"
                 "pextub $v1, $zero, $v1\n"
                 "paddh $a6, $a6, $v0\n"
                 "paddh $t3, $t3, $v1\n"
                 "paddh $v0, $a4, $a6\n"
                 "paddh $v1, $a5, $t3\n"
                 "por $a4, $a6, $zero\n"
                 "por $a5, $t3, $zero\n"
                 "paddh $v0, $v0, $t9\n"
                 "paddh $v1, $v1, $t9\n"
                 "psrlh $v0, $v0, 2\n"
                 "psrlh $v1, $v1, 2\n"
                 "lq $a6, 0x0($t2)\n"
                 "lq $t3, 0x10($t2)\n"
                 "paddh $v0, $v0, $a6\n"
                 "paddh $v1, $v1, $t3\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "paddh $a6, $v0, $t9\n"
                 "psrlh $v0, $a6, 1\n"
                 "paddh $a6, $v1, $t9\n"
                 "psrlh $v1, $a6, 1\n"
                 "sq $v0, 0x0($t2)\n"
                 "sll $a6, $t8, 1\n"
                 "sq $v1, 0x10($t2)\n"
                 "bgtz $a3, .L002C0264_%=\n"
                 "addu $t2, $t2, $a6\n"
                 ".L002C0300_%=:\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x80\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002C0264_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// The chroma's rows (Cb, then Cr) as they are
void PredictChroma(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $a1, 0x0($a0)\n"
                 "lw $a2, 0x14($a0)\n"
                 "lw $a3, 0x18($a0)\n"
                 "lw $t1, 0x10($a0)\n"
                 "lw $t0, 0x4($a0)\n"
                 "sll $a7, $t1, 1\n"
                 "mtsab $t0, 0x0\n"
                 "addiu $t8, $zero, -0x1\n"
                 ".L002BF7E0_%=:\n"
                 "lw $t3, 0x8($a0)\n"
                 "addiu $t9, $zero, -0x1\n"
                 ".L002BF7E8_%=:\n"
                 "ld $a4, 0x0($a2)\n"
                 "ld $a5, 0x0($a3)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a5, $a4, $a4\n"
                 "pextlb $a4, $zero, $a5\n"
                 "sq $a4, 0x0($a1)\n"
                 "addi $t3, $t3, -0x1\n"
                 "addu $a2, $a2, $t1\n"
                 "addu $a1, $a1, $a7\n"
                 "bgtz $t3, .L002BF7E8_%=\n"
                 "addu $a3, $a3, $t1\n"
                 "addiu $a2, $a2, 0x140\n"
                 "addiu $a3, $a3, 0x140\n"
                 "lw $t3, 0xC($a0)\n"
                 "and $a6, $t9, $t3\n"
                 "bnez $a6, .L002BF7E8_%=\n"
                 "daddu $t9, $zero, $zero\n"
                 "lw $a1, 0x0($a0)\n"
                 "lw $a2, 0x14($a0)\n"
                 "lw $a3, 0x18($a0)\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a2, $a2, 0x40\n"
                 "addiu $a3, $a3, 0x40\n"
                 "bnez $t8, .L002BF7E0_%=\n"
                 "daddu $t8, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each chroma pixel the average of its row's and the next row's
void PredictChromaHalfDown(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $a7, $zero, 0x1\n"
                 "sll $t8, $t0, 1\n"
                 "mtsab $t1, 0x0\n"
                 ".L002BF938_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "ori $a7, $a7, 0x8000\n"
                 "beqz $a3, .L002BF994_%=\n"
                 "pextlb $t3, $zero, $a4\n"
                 ".L002BF958_%=:\n"
                 "addu $a1, $a1, $t0\n"
                 "addu $a2, $a2, $t0\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a6, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "paddh $a5, $a6, $t3\n"
                 "por $t3, $a6, $zero\n"
                 "paddh $a6, $a5, $t9\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002BF958_%=\n"
                 "addu $t2, $t2, $t8\n"
                 ".L002BF994_%=:\n"
                 "psrah $a6, $a7, 15\n"
                 "addiu $a1, $a1, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "addiu $a2, $a2, 0x140\n"
                 "and $a6, $a6, $a3\n"
                 "bnez $a6, .L002BF958_%=\n"
                 "andi $a7, $a7, 0x7FFF\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "andi $a6, $a7, 0x1\n"
                 "bnez $a6, .L002BF938_%=\n"
                 "andi $a7, $a7, 0xFFFE\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each chroma pixel the average of it and the next one across
void PredictChromaHalfAcross(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $v1, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $t8, $zero, 0x1\n"
                 "addiu $t0, $zero, -0x1\n"
                 "sll $v0, $v1, 1\n"
                 ".L002BFAB8_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "addiu $a7, $zero, -0x1\n"
                 ".L002BFAC0_%=:\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $v1\n"
                 "addu $a2, $a2, $v1\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "paddh $a6, $a5, $a4\n"
                 "paddh $a6, $a6, $t9\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002BFAC0_%=\n"
                 "addu $t2, $t2, $v0\n"
                 "addiu $a1, $a1, 0x140\n"
                 "addiu $a2, $a2, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002BFAC0_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "bnez $t0, .L002BFAB8_%=\n"
                 "daddu $t0, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// Each chroma pixel the average of four, across and down
void PredictChromaHalfBoth(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "psllh $t9, $t9, 1\n"
                 "addiu $t8, $zero, 0x1\n"
                 "addiu $a7, $zero, 0x1\n"
                 ".L002BFC68_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addu $a1, $a1, $t0\n"
                 "ori $a7, $a7, 0x8000\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "beqz $a3, .L002BFCF4_%=\n"
                 "paddh $t3, $a5, $a4\n"
                 ".L002BFCA0_%=:\n"
                 "addu $a2, $a2, $t0\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $t0\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "paddh $a6, $a5, $a4\n"
                 "paddh $a5, $a6, $t3\n"
                 "por $t3, $a6, $zero\n"
                 "paddh $a6, $a5, $t9\n"
                 "sll $a4, $t0, 1\n"
                 "psrlh $a6, $a6, 2\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002BFCA0_%=\n"
                 "addu $t2, $t2, $a4\n"
                 ".L002BFCF4_%=:\n"
                 "psrah $a6, $a7, 15\n"
                 "addiu $a1, $a1, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "addiu $a2, $a2, 0x140\n"
                 "and $a6, $a6, $a3\n"
                 "bnez $a6, .L002BFCA0_%=\n"
                 "andi $a7, $a7, 0x7FFF\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "andi $a6, $a7, 0x1\n"
                 "bnez $a6, .L002BFC68_%=\n"
                 "andi $a7, $a7, 0xFFFE\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// The chroma's rows averaged with the prediction
void AverageChroma(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $v1, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "addiu $t0, $zero, -0x1\n"
                 "sll $v0, $v1, 1\n"
                 "mtsab $t1, 0x0\n"
                 ".L002BFE00_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "addiu $a7, $zero, -0x1\n"
                 ".L002BFE08_%=:\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $v1\n"
                 "addu $a2, $a2, $v1\n"
                 "lq $a4, 0x0($t2)\n"
                 "paddh $a6, $a5, $a4\n"
                 "pcgth $a5, $a6, $zero\n"
                 "psrlh $a5, $a5, 15\n"
                 "paddh $a6, $a6, $a5\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002BFE08_%=\n"
                 "addu $t2, $t2, $v0\n"
                 "addiu $a1, $a1, 0x140\n"
                 "addiu $a2, $a2, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002BFE08_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "bnez $t0, .L002BFE00_%=\n"
                 "daddu $t0, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictChromaHalfDown, averaged with the prediction
void AverageChromaHalfDown(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $a7, $zero, 0x1\n"
                 "sll $t8, $t0, 1\n"
                 "mtsab $t1, 0x0\n"
                 ".L002BFF98_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "ori $a7, $a7, 0x8000\n"
                 "beqz $a3, .L002C000C_%=\n"
                 "pextlb $t3, $zero, $a4\n"
                 ".L002BFFB8_%=:\n"
                 "addu $a1, $a1, $t0\n"
                 "addu $a2, $a2, $t0\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a6, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "paddh $a5, $a6, $t3\n"
                 "por $t3, $a6, $zero\n"
                 "paddh $a6, $a5, $t9\n"
                 "psrlh $a6, $a6, 1\n"
                 "lq $a4, 0x0($t2)\n"
                 "paddh $a6, $a6, $a4\n"
                 "pcgth $a5, $a6, $zero\n"
                 "psrlh $a5, $a5, 15\n"
                 "paddh $a6, $a6, $a5\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002BFFB8_%=\n"
                 "addu $t2, $t2, $t8\n"
                 ".L002C000C_%=:\n"
                 "psrah $a6, $a7, 15\n"
                 "addiu $a1, $a1, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "addiu $a2, $a2, 0x140\n"
                 "and $a6, $a6, $a3\n"
                 "bnez $a6, .L002BFFB8_%=\n"
                 "andi $a7, $a7, 0x7FFF\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "andi $a6, $a7, 0x1\n"
                 "bnez $a6, .L002BFF98_%=\n"
                 "andi $a7, $a7, 0xFFFE\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictChromaHalfAcross, averaged with the prediction
void AverageChromaHalfAcross(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $v1, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "addiu $t8, $zero, 0x1\n"
                 "addiu $t0, $zero, -0x1\n"
                 "sll $v0, $v1, 1\n"
                 ".L002C0158_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "addiu $a7, $zero, -0x1\n"
                 ".L002C0160_%=:\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $v1\n"
                 "addu $a2, $a2, $v1\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "paddh $a6, $a5, $a4\n"
                 "paddh $a6, $a6, $t9\n"
                 "psrlh $a6, $a6, 1\n"
                 "lq $a4, 0x0($t2)\n"
                 "paddh $a6, $a6, $a4\n"
                 "pcgth $a5, $a6, $zero\n"
                 "psrlh $a5, $a5, 15\n"
                 "paddh $a6, $a6, $a5\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002C0160_%=\n"
                 "addu $t2, $t2, $v0\n"
                 "addiu $a1, $a1, 0x140\n"
                 "addiu $a2, $a2, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "and $a6, $a7, $a3\n"
                 "bnez $a6, .L002C0160_%=\n"
                 "daddu $a7, $zero, $zero\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "bnez $t0, .L002C0158_%=\n"
                 "daddu $t0, $zero, $zero\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// As PredictChromaHalfBoth, averaged with the prediction
void AverageChromaHalfBoth(const MpegPredictionBlock* block)
{
    register const MpegPredictionBlock* a0 asm("$4") = block;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "lw $t0, 0x10($a0)\n"
                 "lw $t1, 0x4($a0)\n"
                 "pnor $t9, $zero, $zero\n"
                 "psrlh $t9, $t9, 15\n"
                 "psllh $t9, $t9, 1\n"
                 "addiu $t8, $zero, 0x1\n"
                 "addiu $a7, $zero, 0x1\n"
                 ".L002C0348_%=:\n"
                 "lw $a3, 0x8($a0)\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addu $a1, $a1, $t0\n"
                 "ori $a7, $a7, 0x8000\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "beqz $a3, .L002C03EC_%=\n"
                 "paddh $t3, $a5, $a4\n"
                 ".L002C0380_%=:\n"
                 "addu $a2, $a2, $t0\n"
                 "ld $a4, 0x0($a1)\n"
                 "ld $a5, 0x0($a2)\n"
                 "pcpyld $a4, $a5, $a4\n"
                 "mtsab $t1, 0x0\n"
                 "qfsrv $a4, $a4, $a4\n"
                 "pextlb $a5, $zero, $a4\n"
                 "addi $a3, $a3, -0x1\n"
                 "addu $a1, $a1, $t0\n"
                 "mtsab $t8, 0x0\n"
                 "qfsrv $a6, $zero, $a4\n"
                 "pextlb $a4, $zero, $a6\n"
                 "paddh $a6, $a5, $a4\n"
                 "paddh $a5, $a6, $t3\n"
                 "por $t3, $a6, $zero\n"
                 "paddh $a6, $a5, $t9\n"
                 "psrlh $a6, $a6, 2\n"
                 "lq $a4, 0x0($t2)\n"
                 "paddh $a6, $a6, $a4\n"
                 "pcgth $a5, $a6, $zero\n"
                 "psrlh $a5, $a5, 15\n"
                 "paddh $a6, $a6, $a5\n"
                 "sll $a4, $t0, 1\n"
                 "psrlh $a6, $a6, 1\n"
                 "sq $a6, 0x0($t2)\n"
                 "bgtz $a3, .L002C0380_%=\n"
                 "addu $t2, $t2, $a4\n"
                 ".L002C03EC_%=:\n"
                 "psrah $a6, $a7, 15\n"
                 "addiu $a1, $a1, 0x140\n"
                 "lw $a3, 0xC($a0)\n"
                 "addiu $a2, $a2, 0x140\n"
                 "and $a6, $a6, $a3\n"
                 "bnez $a6, .L002C0380_%=\n"
                 "andi $a7, $a7, 0x7FFF\n"
                 "lw $t2, 0x0($a0)\n"
                 "lw $a1, 0x14($a0)\n"
                 "lw $a2, 0x18($a0)\n"
                 "addiu $t2, $t2, 0x80\n"
                 "addiu $a1, $a1, 0x40\n"
                 "addiu $a2, $a2, 0x40\n"
                 "andi $a6, $a7, 0x1\n"
                 "bnez $a6, .L002C0348_%=\n"
                 "andi $a7, $a7, 0xFFFE\n"
                 ".set pop"
                 :
                 : "r"(a0)
                 : "$2", "$3", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// The pixels' maximum, read with lq by StorePixels and AddPrediction (retail keeps it in .text after StorePixels' loop, which runs
// through it as harmless instructions into FUN_002bf5c0's return)
alignas(16) extern const u16 g_MpegPixelMaximum[8] RETAIL(D_002BF5B0) = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Six times 64 pixels: prediction plus residual (16 bit) clamped to 0-255 by pminh/pmaxh and packed (ppacb)
void AddPrediction(u8* destination, const s16* prediction, const s16* residual)
{
    register u8* a0 asm("$4") = destination;
    register const s16* a1 asm("$5") = prediction;
    register const s16* a2 asm("$6") = residual;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "addiu $v0, $zero, 0x18\n"
                 "lui $a6, %%hi(D_002BF5B0)\n"
                 "addiu $a6, $a6, %%lo(D_002BF5B0)\n"
                 "lq $v1, 0x0($a6)\n"
                 ".L002BF428_%=:\n"
                 "lq $a4, 0x0($a2)\n"
                 "addi $v0, $v0, -0x4\n"
                 "lq $a5, 0x10($a2)\n"
                 "lq $a6, 0x20($a2)\n"
                 "lq $a7, 0x30($a2)\n"
                 "lq $t0, 0x40($a2)\n"
                 "lq $t1, 0x50($a2)\n"
                 "lq $t2, 0x60($a2)\n"
                 "lq $t3, 0x70($a2)\n"
                 "lq $t9, 0x0($a1)\n"
                 "paddh $a4, $a4, $t9\n"
                 "lq $t9, 0x10($a1)\n"
                 "pminh $a4, $a4, $v1\n"
                 "pmaxh $a4, $a4, $zero\n"
                 "paddh $a5, $a5, $t9\n"
                 "lq $t9, 0x20($a1)\n"
                 "pminh $a5, $a5, $v1\n"
                 "pmaxh $a5, $a5, $zero\n"
                 "ppacb $a5, $a5, $a4\n"
                 "paddh $a6, $a6, $t9\n"
                 "pminh $a6, $a6, $v1\n"
                 "lq $t9, 0x30($a1)\n"
                 "pmaxh $a6, $a6, $zero\n"
                 "paddh $a7, $a7, $t9\n"
                 "lq $t9, 0x40($a1)\n"
                 "pminh $a7, $a7, $v1\n"
                 "pmaxh $a7, $a7, $zero\n"
                 "ppacb $a7, $a7, $a6\n"
                 "paddh $t0, $t0, $t9\n"
                 "lq $t9, 0x50($a1)\n"
                 "pminh $t0, $t0, $v1\n"
                 "pmaxh $t0, $t0, $zero\n"
                 "paddh $t1, $t1, $t9\n"
                 "lq $t9, 0x60($a1)\n"
                 "pminh $t1, $t1, $v1\n"
                 "pmaxh $t1, $t1, $zero\n"
                 "ppacb $t1, $t1, $t0\n"
                 "paddh $t2, $t2, $t9\n"
                 "lq $t9, 0x70($a1)\n"
                 "pminh $t2, $t2, $v1\n"
                 "pmaxh $t2, $t2, $zero\n"
                 "sq $a5, 0x0($a0)\n"
                 "paddh $t3, $t3, $t9\n"
                 "pminh $t3, $t3, $v1\n"
                 "pmaxh $t3, $t3, $zero\n"
                 "ppacb $t3, $t3, $t2\n"
                 "sq $a7, 0x10($a0)\n"
                 "sq $t1, 0x20($a0)\n"
                 "sq $t3, 0x30($a0)\n"
                 "addiu $a1, $a1, 0x80\n"
                 "addiu $a0, $a0, 0x40\n"
                 "bnez $v0, .L002BF428_%=\n"
                 "addiu $a2, $a2, 0x80\n"
                 ".set pop"
                 : "+r"(a0), "+r"(a1), "+r"(a2)
                 :
                 : "$2", "$3", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$25", "memory");
}

// The same without the residual
void StorePixels(u8* destination, const s16* values)
{
    register u8* a0 asm("$4") = destination;
    register const s16* a1 asm("$5") = values;
    asm volatile(".set push\n"
                 ".set noreorder\n"
                 "addiu $v0, $zero, 0x18\n"
                 "lui $a6, %%hi(D_002BF5B0)\n"
                 "addiu $a6, $a6, %%lo(D_002BF5B0)\n"
                 "lq $v1, 0x0($a6)\n"
                 ".L002BF518_%=:\n"
                 "lq $a4, 0x0($a1)\n"
                 "addi $v0, $v0, -0x4\n"
                 "daddu $t9, $a1, $zero\n"
                 "addiu $a1, $a1, 0x80\n"
                 "lq $a5, 0x10($t9)\n"
                 "lq $a6, 0x20($t9)\n"
                 "lq $a7, 0x30($t9)\n"
                 "lq $t0, 0x40($t9)\n"
                 "lq $t1, 0x50($t9)\n"
                 "lq $t2, 0x60($t9)\n"
                 "lq $t3, 0x70($t9)\n"
                 "pminh $a4, $a4, $v1\n"
                 "pmaxh $a4, $a4, $zero\n"
                 "pminh $a5, $a5, $v1\n"
                 "pmaxh $a5, $a5, $zero\n"
                 "ppacb $a5, $a5, $a4\n"
                 "pminh $a6, $a6, $v1\n"
                 "pmaxh $a6, $a6, $zero\n"
                 "pminh $a7, $a7, $v1\n"
                 "pmaxh $a7, $a7, $zero\n"
                 "ppacb $a7, $a7, $a6\n"
                 "pminh $t0, $t0, $v1\n"
                 "pmaxh $t0, $t0, $zero\n"
                 "pminh $t1, $t1, $v1\n"
                 "pmaxh $t1, $t1, $zero\n"
                 "ppacb $t1, $t1, $t0\n"
                 "pminh $t2, $t2, $v1\n"
                 "pmaxh $t2, $t2, $zero\n"
                 "pminh $t3, $t3, $v1\n"
                 "pmaxh $t3, $t3, $zero\n"
                 "ppacb $t3, $t3, $t2\n"
                 "sq $a5, 0x0($a0)\n"
                 "sq $a7, 0x10($a0)\n"
                 "sq $t1, 0x20($a0)\n"
                 "sq $t3, 0x30($a0)\n"
                 "bnez $v0, .L002BF518_%=\n"
                 "addiu $a0, $a0, 0x40\n"
                 "nop\n"
                 ".set pop"
                 : "+r"(a0), "+r"(a1)
                 :
                 : "$2", "$3", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$25", "memory");
}

// StorePixels' return: splat split the function at its mask, after which its loop ends in this jr $ra
void StorePixelsReturn() RETAIL(FUN_002bf5c0);

void StorePixelsReturn()
{
}
}
