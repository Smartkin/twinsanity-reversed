#pragma once

#include "movie.h"
#include "../renderer/renderer.h"

#include <dma_tags.h>
#include <ee_regs.h>
#include <kernel.h>

// Sony's libmpeg and libipu (SDK 3.0.3), which the movie decoder is built on: a PSS file's packs are demultiplexed to the streams'
// callbacks, the video's pictures are decoded by the IPU into reference images in the work memory sceMpegCreate is given and put
// out into the caller's buffer, as 32 bit pixels the IPU converts them to (sceMpegGetPicture) or as the IPU's 8 bit macroblocks
// (sceMpegGetPictureRAW8). The pictures are decoded (mpegdecode.cpp) with the IPU reading the stream's bits (mpegbits.cpp) and
// headers (mpegheaders.cpp) and decoding the macroblocks' blocks, the motion compensation's predictions made on the CPU from the
// reference pictures' macroblocks fetched into the scratchpad (mpegmotion.cpp). The IPU's and the DMA's registers they drive are
// the movie decoder's (decoder.cpp) too

// What a callback gets: the type first (sceMpegCbData)
struct MpegCallbackData
{
    s32 type;
};

// sceMpegCbDataError
struct MpegErrorData
{
    s32 type;
    const char* message;
};

// sceMpegCbDataTimeStamp: the callback gives the picture whose header was read its PTS and DTS (-1 without)
struct MpegTimeStampData
{
    s32 type;
    s64 pts;
    s64 dts;
};
CHECK_OFFSET(MpegTimeStampData, pts, 0x8);
CHECK_SIZE(MpegTimeStampData, 0x18);

// A callback sceMpegAddCallback set: the function, its argument and the global pointer it was set with ($gp, put in place while
// it runs)
struct MpegCallbackEntry
{
    MpegCallback function;
    void* user;
    u32 globalPointer;
};
CHECK_SIZE(MpegCallbackEntry, 0xC);

// PES stream_ids: the private streams (a private stream 1's data starts with 4 bytes of its sub-stream) and the ones whose packets
// have none of the usual header fields
enum PesStreamId : u32
{
    ProgramStreamMap = 0xBC,
    PrivateStream1 = 0xBD,
    PaddingStream = 0xBE,
    PrivateStream2 = 0xBF,
    EcmStream = 0xF0,
    EmmStream = 0xF1,
    DsmccStream = 0xF2,
    H2221TypeEStream = 0xF8,
    ProgramStreamDirectory = 0xFF,
};

// A packet's stream as libmpeg matches it: a private stream's first 4 bytes (its sub-stream, 0 for the other streams) below the
// PES stream_id
union MpegStreamId
{
    u64 value;
    struct
    {
        u64 subStream : 32;
        u64 streamId : 8;
        u64 unused40 : 24;
    };
};
CHECK_SIZE(MpegStreamId, 8);

constexpr u32 PesStreamIdShift = 32;
// A sub-stream's first byte (Sony's own streams have 0xFF there, the DVD's the stream type and its channel)
constexpr u32 SubStreamTypeShift = 24;
constexpr u32 SubStreamBytes = 4;

// A stream's callback (sceMpegAddStrCallback): it gets the packets whose stream ID (MpegStreamId's value), masked, is id
struct MpegStreamCallback
{
    u64 id;
    u64 mask;
    MpegCallback function;
    void* user;
    u32 globalPointer;
    u32 unused1C;
};
CHECK_SIZE(MpegStreamCallback, 0x20);

// The stream types sceMpegAddStrCallback takes (by MpegStreamKind) and what they match: a stream ID and mask, the channel ORed in
// below the mask's top byte
struct MpegStreamType
{
    u64 id;
    u64 mask;
};
CHECK_SIZE(MpegStreamType, 0x10);

// The stream types' masks: the stream_id (the channel ORed into it: video, MPEG audio), with the sub-stream's first byte (the
// channel ORed into that byte: the DVD's AC-3, LPCM, DTS and SDDS) or with the whole sub-stream (the channel its last byte: Sony's
// IPU, PCM, ADPCM and DATA)
constexpr u64 MatchStreamId = 0xFF00000000ull;
constexpr u64 MatchSubStreamType = 0xFFFF000000ull;
constexpr u64 MatchSubStream = 0xFFFFFFFFFFull;

constexpr s32 MpegStreamCallbacks = 0x40;
// The private stream callback of every sub-stream no other callback takes, private stream 1's FF 00 00 00 (the PSS's DATA streams'
// catch-all)
constexpr u64 MpegAnyPrivateStream = u64{PrivateStream1} << PesStreamIdShift | 0xFFu << SubStreamTypeShift;

// The work memory past the state the reference pictures are allocated from (_sceMpegAlalc): a sequence's allocations start at
// the mark
struct MpegArena
{
    u8* base;
    s32 size;
    u8* next;
    u8* mark;
};
CHECK_SIZE(MpegArena, 0x10);

// A macroblock's pixels (16x16 Y, then 8x8 Cb and Cr: MacroblockBytes), the 16 bit values the IPU decodes them as, and a
// reference's macroblocks fetched for a prediction (the column of two its block starts in, then the column to its right)
constexpr s32 MacroblockPixels = 16 * 16;
constexpr u32 MacroblockBytes = 0x180;
constexpr u32 MacroblockQuadwords = MacroblockBytes >> 4;
constexpr u32 DecodedMacroblockBytes = 2 * MacroblockBytes;
constexpr s32 FetchedReferenceBytes = 4 * MacroblockBytes;
// A macroblock's prediction takes 4 references at most (dual prime's)
constexpr s32 MpegMaxReferences = 4;
// A macroblock converted to pixels: 64 quadwords of 32 bit pixels, 32 of 16 bit ones. The fromIPU channel takes 1023 macroblocks
// of them at most at once (its QWC is 16 bits)
constexpr u32 Rgb32MacroblockQuadwords = 0x40;
constexpr u32 Rgb16MacroblockQuadwords = 0x20;
constexpr s32 ChunkMacroblocks = 0x3FF;

// A reference picture: a frame or one of its fields, its macroblocks in columns from the left, each from the top
struct MpegImage
{
    u8* pixels;
    s32 width;
    s32 height;
    s32 widthMacroblocks;
    s32 heightMacroblocks;
    u32 unused14;
    s64 pts;
    s64 dts;
    // 1 while it holds a picture
    s32 holdsPicture;
    // The picture's header and extensions
    s32 pictureCodingType;
    s32 pictureStructure;
    s32 progressiveSequence;
    s32 progressiveFrame;
    s32 topFieldFirst;
    s32 repeatFirstField;
    s32 frameCentreHorizontalOffsets[3];
    s32 frameCentreVerticalOffsets[3];
    s32 displayHorizontalSize;
    s32 displayVerticalSize;
    u32 unused64;
};
CHECK_SIZE(MpegImage, 0x68);

// A block of a macroblock's prediction (its luma or its chroma) from a reference picture, as the prediction routines read it: the
// block's rows from the reference's macroblock it starts in and the one to its right, aligned by the column it starts at and
// averaged for half pixels, into the prediction as 16 bit pixels (rows of 32 bytes for the luma, 16 for the chroma: Cb, Cr 0x40
// bytes after it)
struct MpegPredictionBlock
{
    u8* prediction;
    // Where the block starts in its macroblock's row (luma 0-15, chroma 0-7)
    s32 column;
    // The rows made from the macroblocks the block starts in, then from the ones below them
    s32 rows;
    s32 rowsBelow;
    // From a row of the reference to the next: a macroblock's row (16 luma, 8 chroma), two of them for a field
    s32 stride;
    // The row the block starts in, in its macroblock and in the one to its right
    u8* left;
    u8* right;
};
CHECK_SIZE(MpegPredictionBlock, 0x1C);

// A prediction routine (hand-written MMI, called through the table jtbl_002E8238): 8 for the luma and 8 for the chroma, by
// average << 2 | half a pixel across << 1 | half a pixel down
using MpegPredictionRoutine = void (*)(const MpegPredictionBlock* block);

// A macroblock in the making: its blocks the IPU decodes, its prediction's references and where it goes. Two of them are used in
// turns (bufferIndex): a macroblock is finished (its prediction made and stored with the blocks) while the next is decoded
struct MpegMacroblockBuffers
{
    // Where the DMA fetches the references' macroblocks into, FetchedReferenceBytes each: the two macroblocks a block's column
    // starts in (one above the other, next to each other in a picture) and the two to their right (in the scratchpad; none in the
    // state's buffers, where the routines read the pictures themselves)
    u8* references;
    // The macroblock the IPU decodes into: its 384 values as 16 bit (an intra macroblock's pixels, or what a prediction adds to)
    u8* macroblocks;
    // A reference's macroblock its block starts in, and the one to its right: the DMA's sources
    u8* sources[MpegMaxReferences];
    u8* rightSources[MpegMaxReferences];
    MpegPredictionRoutine lumaRoutines[MpegMaxReferences];
    MpegPredictionRoutine chromaRoutines[MpegMaxReferences];
    MpegPredictionBlock luma[MpegMaxReferences];
    MpegPredictionBlock chroma[MpegMaxReferences];
    // Where the macroblock goes in the picture decoded
    u8* destination;
    s32 referenceCount;
    s32 intra;
    // A coded macroblock with no addresses skipped before it
    s32 unused134;
    // The references were fetched: the prediction is made
    s32 predicted;
    // The IPU decoded no blocks: the prediction alone is the macroblock
    s32 noResidual;
};
CHECK_OFFSET(MpegMacroblockBuffers, luma, 0x48);
CHECK_OFFSET(MpegMacroblockBuffers, chroma, 0xB8);
CHECK_OFFSET(MpegMacroblockBuffers, destination, 0x128);
CHECK_SIZE(MpegMacroblockBuffers, 0x140);

// A macroblock's motion vector or its predictor, in half pixels (MPEG-2's PMV[r][s]: the first or second vector, forward or
// backward)
struct MpegVector
{
    s32 horizontal;
    s32 vertical;
};

// A vector's component in half pixels, as a prediction takes it: whether it moves by half a pixel more than its whole pixels
union HalfPixels
{
    s32 value;
    struct
    {
        u32 half : 1;
        s32 whole : 31;
    };
};
CHECK_SIZE(HalfPixels, 4);

// Where the decoder's buffers are (bufferType): the scratchpad, or the state's own
enum MpegBufferType : s32
{
    MpegBuffersScratchpad = 0,
    MpegBuffersOwn = 1,
};

// picture_coding_type
enum MpegPictureCodingType : s32
{
    MpegPictureI = 1,
    MpegPictureP = 2,
    MpegPictureB = 3,
    MpegPictureD = 4,
};

// picture_structure
enum MpegPictureStructure : s32
{
    MpegTopField = 1,
    MpegBottomField = 2,
    MpegFrame = 3,
};

// frame_motion_type and field_motion_type: field prediction (one vector a field of a frame picture), frame prediction (16x8 in a
// field picture: a vector for each half), dual prime
enum MpegMotionType : s32
{
    MpegMotionField = 1,
    MpegMotionFrame = 2,
    MpegMotion16x8 = 2,
    MpegMotionDualPrime = 3,
};

// chroma_format: libmpeg decodes 4:2:0 only
constexpr s32 MpegChroma420 = 1;

// macroblock_type's flags, as the IPU's VDEC decodes them
union MacroblockType
{
    u32 value;
    struct
    {
        u32 intra : 1;
        u32 pattern : 1;
        u32 backward : 1;
        u32 forward : 1;
        u32 quantiser : 1;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(MacroblockType, 4);

// The video's start codes
constexpr u32 PictureStartCode = 0x100;
constexpr u32 FirstSliceStartCode = 0x101;
constexpr u32 SliceStartCodes = 0xAF;
constexpr u32 UserDataStartCode = 0x1B2;
constexpr u32 SequenceHeaderCode = 0x1B3;
constexpr u32 ExtensionStartCode = 0x1B5;
constexpr u32 SequenceEndCode = 0x1B7;
constexpr u32 GroupStartCode = 0x1B8;
// The 23 zero bits a start code starts with
constexpr s32 StartCodeZeros = 23;

// sceMpeg's flags of a picture (Mpeg::flags), by their first bit: picture_coding_type, picture_structure, then
// repeat_first_field, top_field_first, progressive_frame and progressive_sequence, the 4 that tell the fields the picture is shown
// for (the timing)
enum MpegFlagsShift : u32
{
    MpegFlagsCodingTypeShift = 0,
    MpegFlagsStructureShift = 3,
    MpegFlagsTimingShift = 5,
    MpegFlagsRepeatFirstFieldShift = 5,
    MpegFlagsTopFieldFirstShift = 6,
    MpegFlagsProgressiveFrameShift = 7,
    MpegFlagsProgressiveSequenceShift = 8,
};
constexpr u64 MpegFlagsTiming = 0xF;
constexpr u64 MpegFlagsTopFieldFirst = u64{1} << MpegFlagsTopFieldFirstShift;

// The reference pictures' places in MpegSystem's frames and fields: the past reference, the future one, the image the picture is
// decoded into (the future reference's, or the B pictures' own)
enum MpegReferenceSlot : s32
{
    MpegPast = 0,
    MpegFuture = 1,
    MpegCurrent = 2,
    MpegBImage = 3,
    MpegReferenceSlots = 4,
};

// MpegSystem's counts and limits of the I, P and B pictures (D pictures with the B ones) after an I picture
enum MpegPictureCount : s32
{
    MpegIPictures = 0,
    MpegPPictures = 1,
    MpegBPictures = 2,
    MpegPictureCounts = 3,
};

// What a slice's decoding ends with: the picture's end, the slice given up for a VLC error, an error that gives the picture up, the
// next start code, the decoding aborted. A slice's header read is 0 too
enum SliceResult : s32
{
    SliceEnded = 0,
    SliceGivenUp = 1,
    PictureGivenUp = 2,
    NextStartCodeFound = 3,
    DecodingAborted = 4,
};
constexpr s32 SliceHeaderRead = 0;

// NextHeader's results besides a picture_coding_type
constexpr s32 MpegSequenceEnded = 0;
constexpr s32 MpegHeaderAborted = -1;
// DecodeOrSkip's result for a field picture without its second field
constexpr s32 MpegSecondFieldMissing = -1;

// MpegSystem::outputState
enum MpegOutputState : s32
{
    MpegNothingDecoded = 0,
    MpegPictureDecoded = 1,
    MpegPicturePutOut = 2,
};

// MpegSystem::pendingPtsState: a PTS set, which the picture put out after the next one gets
enum MpegPendingPts : s32
{
    MpegNoPendingPts = 0,
    MpegPtsSet = 1,
    MpegPtsForNextPicture = 2,
};

// libmpeg's state (Mpeg::sys), in the work memory sceMpegCreate gets, 0x19A0 bytes before the arena's memory
struct MpegSystem
{
    // sceMpegIsEnd: the sequence end code came
    s32 ended;
    // Whether a decoded picture waits to be put out
    s32 pictureWaiting;
    MpegOutputState outputState;
    MpegCallbackEntry callbacks[MpegCallbackTypes];
    MpegStreamCallback* streamCallbacks;
    s32 streamCallbackCount;
    // The IPU's DMA while the default StopDma and RestartDma callbacks have it stopped
    IpuDmaEnvironment ipuDma;
    // Whether pictures without a PTS get one counted on from the last (nothing in the game sets it), by the step of a field
    // (times the fields shown, halved, its halves rounded up every other time)
    s32 countPts;
    s64 ptsStep;
    s32 lastPts;
    s32 ptsRounding;
    // The fields the picture put out last is shown for
    s64 fields;
    // How many of the I, P and B pictures (D pictures with the B ones) after an I picture are decoded (-1: all of them), the rest
    // skipped, and how many came
    s32 decodeLimits[MpegPictureCounts];
    s32 pictureCounts[MpegPictureCounts];
    // pictureNumber when the first picture was put out
    s32 firstOutputPicture;
    // sceMpegGetPicture: 32 bit pixels (the IPU's colour conversion), sceMpegGetPictureRAW8: the IPU's macroblocks
    s32 outputRgb32;
    // The picture put out last's display values (its picture display extension, the sequence display extension)
    s32 outputCentreHorizontalOffsets[3];
    s32 outputCentreVerticalOffsets[3];
    s32 outputDisplayWidth;
    s32 outputDisplayHeight;
    // The first picture's structure since the sequence header (its first picture coding extension's): MPEG-2's pictures of
    // another structure are passed over, and a frame's second field has the other parity
    s32 firstStructure;
    // The caller's buffer (uncached), its size in pixels (0: by output macroblocks instead, which sceMpegGetPicture uses) or in
    // macroblocks
    u8* output;
    s32 outputWidth;
    s32 outputHeight;
    s32 outputMacroblocks;
    // Makes B pictures lose their past reference like a broken link (nothing sets it)
    s32 forcedBrokenLink;
    // A PTS a picture put out gets (nothing in the game sets one)
    s64 pendingPts;
    MpegPendingPts pendingPtsState;
    u8* frameBuffers[3];
    MpegArena arena;
    // The pictures decoded
    s32 pictureNumber;
    // A macroblock went wrong (a VLC the IPU found no code of): the slice is given up
    s32 macroblockError;
    // Between a frame's first field and its second's decoding: a P field's references of the other parity are then the frame's
    // first field
    s32 secondFieldMissing;
    // The sequence header and its extensions
    s32 horizontalSize;
    s32 verticalSize;
    s32 widthMacroblocks;
    s32 heightMacroblocks;
    s32 bitRate;
    s32 vbvBufferSize;
    s32 progressiveSequence;
    s32 chromaFormat;
    s32 matrixCoefficients;
    s32 displayHorizontalSize;
    s32 displayVerticalSize;
    // The picture header and its extensions
    s32 pictureCodingType;
    s32 fullPelForwardVector;
    s32 forwardFCode;
    s32 fullPelBackwardVector;
    s32 backwardFCode;
    s32 fCodes[2][2];
    s32 pictureStructure;
    s32 topFieldFirst;
    s32 framePredFrameDct;
    s32 concealmentMotionVectors;
    s32 repeatFirstField;
    s32 progressiveFrame;
    s32 frameCentreHorizontalOffsets[3];
    s32 frameCentreVerticalOffsets[3];
    s32 closedGop;
    s32 brokenLink;
    s32 temporalReference;
    // The next BDEC's DC reset bit, and its quantiser scale
    s32 dcReset;
    s32 quantiserScale;
    // The reference pictures: the past one, the future one, the one decoded into and the B picture's, of the frames and their
    // top and bottom fields
    MpegImage* frames[MpegReferenceSlots];
    MpegImage* topFields[MpegReferenceSlots];
    MpegImage* bottomFields[MpegReferenceSlots];
    MpegImage images[9];
    MpegMacroblockBuffers buffers[2];
    s32 bufferIndex;
    // Zeroed for each picture
    u32 unused824;
    // Whether the bits read ahead from the IPU (top, topBits) are stale: the last command wasn't one that reads bits
    s32 bitsStale;
    // The last IPU command's code (its top 4 bits)
    u32 ipuCommand;
    // The prediction _copyAddRefImage adds a macroblock to
    u8* prediction;
    // Whether a picture was put out
    s32 pictureOutput;
    // The next picture's PTS and DTS
    s64 nextPts;
    s64 nextDts;
    u32 top;
    s32 topBits;
    s32 loadIntraQuantiserMatrix;
    s32 loadNonIntraQuantiserMatrix;
    // A sequence extension came: MPEG-2
    s32 mpeg2;
    s32 temporalReferenceBase;
    s32 temporalReferenceLast;
    s32 gopStarted;
    Mpeg* mpeg;
    u32 unused86C[3];
    // sceMpegGetPictureAbort: the decoding stops until sceMpegReset
    s32 aborted;
    MpegBufferType bufferType;
    // The buffers when they aren't in the scratchpad: two of macroblocks, and the prediction's
    u8 ownMacroblocks[2][DecodedMacroblockBytes];
    u8 ownPrediction[0xB20];
};
CHECK_OFFSET(MpegSystem, callbacks, 0xC);
CHECK_OFFSET(MpegSystem, ipuDma, 0x68);
CHECK_OFFSET(MpegSystem, ptsStep, 0x90);
CHECK_OFFSET(MpegSystem, fields, 0xA0);
CHECK_OFFSET(MpegSystem, decodeLimits, 0xA8);
CHECK_OFFSET(MpegSystem, pictureCounts, 0xB4);
CHECK_OFFSET(MpegSystem, firstStructure, 0xE8);
CHECK_OFFSET(MpegSystem, forcedBrokenLink, 0xFC);
CHECK_OFFSET(MpegSystem, outputRgb32, 0xC4);
CHECK_OFFSET(MpegSystem, output, 0xEC);
CHECK_OFFSET(MpegSystem, pendingPts, 0x100);
CHECK_OFFSET(MpegSystem, arena, 0x118);
CHECK_OFFSET(MpegSystem, pictureNumber, 0x128);
CHECK_OFFSET(MpegSystem, macroblockError, 0x12C);
CHECK_OFFSET(MpegSystem, horizontalSize, 0x134);
CHECK_OFFSET(MpegSystem, matrixCoefficients, 0x154);
CHECK_OFFSET(MpegSystem, pictureCodingType, 0x160);
CHECK_OFFSET(MpegSystem, fCodes, 0x174);
CHECK_OFFSET(MpegSystem, pictureStructure, 0x184);
CHECK_OFFSET(MpegSystem, concealmentMotionVectors, 0x190);
CHECK_OFFSET(MpegSystem, closedGop, 0x1B4);
CHECK_OFFSET(MpegSystem, temporalReference, 0x1BC);
CHECK_OFFSET(MpegSystem, frames, 0x1C8);
CHECK_OFFSET(MpegSystem, images, 0x1F8);
CHECK_OFFSET(MpegSystem, buffers, 0x5A0);
CHECK_OFFSET(MpegSystem, bufferIndex, 0x820);
CHECK_OFFSET(MpegSystem, bitsStale, 0x828);
CHECK_OFFSET(MpegSystem, prediction, 0x830);
CHECK_OFFSET(MpegSystem, nextPts, 0x838);
CHECK_OFFSET(MpegSystem, top, 0x848);
CHECK_OFFSET(MpegSystem, loadIntraQuantiserMatrix, 0x850);
CHECK_OFFSET(MpegSystem, mpeg2, 0x858);
CHECK_OFFSET(MpegSystem, gopStarted, 0x864);
CHECK_OFFSET(MpegSystem, mpeg, 0x868);
CHECK_OFFSET(MpegSystem, aborted, 0x878);
CHECK_OFFSET(MpegSystem, bufferType, 0x87C);
CHECK_OFFSET(MpegSystem, ownMacroblocks, 0x880);
CHECK_OFFSET(MpegSystem, ownPrediction, 0xE80);
CHECK_SIZE(MpegSystem, 0x19A0);

// The PSS demultiplexer's bit reader over the file's bytes (which can be a ring)
struct PssReader
{
    // The next bits, from the top: at least 57 of them
    u64 window;
    // Bits read from start on
    u64 position;
    u8* start;
    // The next byte to load into window
    u8* next;
    s32 loaded;
    u8* ringStart;
    u8* ringEnd;
    s32 ringSize;
};
CHECK_SIZE(PssReader, 0x28);

// A PES packet's header, as the demultiplexer hands it on
struct PesPacket
{
    MpegStreamId streamId;
    s32 length;
    s32 scramblingControl;
    s64 pts;
    s64 dts;
    // In bits from the reader's start
    s32 dataPosition;
    s32 dataLength;
    s32 position;
};
CHECK_SIZE(PesPacket, 0x30);

// A pack's header and the packet read last
struct PssPack
{
    u32 scrExtension;
    u32 scr;
    // Bit 32 of the SCR
    u32 scrHigh;
    s32 hasSystemHeader;
    u32 unused10[2];
    PesPacket packet;
};
CHECK_OFFSET(PssPack, packet, 0x18);

// The program stream's start codes
constexpr u32 PackStartCode = 0x1BA;
constexpr u32 SystemHeaderStartCode = 0x1BB;
constexpr u32 ProgramEndCode = 0x1B9;

// The IPU's registers
volatile u32* const IpuCommand = reinterpret_cast<volatile u32*>(A_EE_IPU_CMD);
volatile u32* const IpuControl = reinterpret_cast<volatile u32*>(A_EE_IPU_CTRL);
volatile u32* const IpuBitPosition = reinterpret_cast<volatile u32*>(A_EE_IPU_BP);

// IPU_CMD's commands, their code in the top 4 bits (IpuCodeMask): BCLR (the input FIFO cleared, the bits given skipped), IDEC (an
// intra picture of the IPU's own streams decoded and converted), BDEC (a macroblock's blocks decoded), VDEC (a VLC decoded by a
// table), FDEC (the bits given read), SETIQ (the intra or the non-intra quantiser matrix from the input), SETVQ (the VQ colour
// table), CSC (macroblocks of the input converted to pixels), SETTH (the thresholds of transparent pixels)
constexpr u32 IpuClearInput = 0x00000000;
constexpr u32 IpuIntraDecode = 0x10000000;
constexpr u32 IpuDecodeBlock = 0x20000000;
constexpr u32 IpuDecodeVariable = 0x30000000;
constexpr u32 IpuDecodeFixed = 0x40000000;
constexpr u32 IpuSetIntraMatrix = 0x50000000;
constexpr u32 IpuSetNonIntraMatrix = 0x58000000;
constexpr u32 IpuSetVqClut = 0x60000000;
constexpr u32 IpuConvert = 0x70000000;
constexpr u32 IpuSetThresholds = 0x90000000;
constexpr u32 IpuCodeMask = 0xF0000000;
// VDEC's tables: the macroblock address increment, the macroblock type, the motion code and the dual prime vector
constexpr u32 IpuAddressIncrementTable = 0x0;
constexpr u32 IpuMacroblockTypeTable = 0x4000000;
constexpr u32 IpuMotionCodeTable = 0x8000000;
constexpr u32 IpuDmVectorTable = 0xC000000;
// BDEC's fields: the quantiser scale, the DCT type, the DC reset and macroblock_intra (libmpeg ORs its values in unmasked)
constexpr s32 IpuBlockQuantiserShift = 16;
constexpr s32 IpuBlockDctTypeShift = 25;
constexpr s32 IpuBlockDcResetShift = 26;
constexpr s32 IpuBlockIntraShift = 27;
// CSC's: dithered (DTE), 16 bit pixels (OFM); the macroblocks' count below them
constexpr u32 IpuConvertDither = 0x4000000;
constexpr u32 IpuConvertRgb16 = 0x8000000;

// IDEC's command word: the bits skipped first (FB), the quantiser scale (QSC), the macroblocks' DCT types read (DTD), signed
// pixels (SGN), dithered (DTE), 16 bit pixels (OFM)
union IpuIntraDecodeCommand
{
    u32 value;
    struct
    {
        u32 skipBits : 6;
        u32 unused6 : 10;
        u32 quantiserScale : 5;
        u32 unused21 : 3;
        u32 decodesDctType : 1;
        u32 signedPixels : 1;
        u32 dither : 1;
        u32 rgb16 : 1;
        u32 code : 4;
    };
};
CHECK_SIZE(IpuIntraDecodeCommand, 4);

// IPU_CMD read and IPU_TOP: 32 bits (VDEC's or FDEC's result, the input's next bits) and BUSY while they're to come
union IpuDataRegister
{
    u64 value;
    struct
    {
        u64 data : 32;
        u64 unused32 : 31;
        u64 busy : 1;
    };
};
CHECK_SIZE(IpuDataRegister, 8);

// VDEC's result: the value the VLC decoded to and the code's length (all 0 when no code matched)
union IpuVlcResult
{
    u32 value;
    struct
    {
        s32 decoded : 16;
        u32 codeLength : 16;
    };
};
CHECK_SIZE(IpuVlcResult, 4);

// IPU_CTRL written whole: the IPU reset
constexpr u32 IpuControlReset = 0x40000000;
// IPU_CTRL's settings the picture coding extension gives (libmpeg ORs the stream's values in unmasked): intra_dc_precision,
// alternate_scan, intra_vlc_format and q_scale_type, and the picture_coding_type
constexpr u32 IpuControlDcPrecision = 0x30000;
constexpr s32 IpuControlDcPrecisionShift = 16;
constexpr u32 IpuControlAlternateScan = 0x100000;
constexpr s32 IpuControlAlternateScanShift = 20;
constexpr u32 IpuControlIntraVlc = 0x200000;
constexpr s32 IpuControlIntraVlcShift = 21;
constexpr u32 IpuControlQuantiserType = 0x400000;
constexpr s32 IpuControlQuantiserTypeShift = 22;
constexpr u32 IpuControlPictureType = 0x7000000;
constexpr s32 IpuControlPictureTypeShift = 24;

// CHCR's bits libmpeg writes and tests: the direction, the chain mode and STR
constexpr u32 ChcrFromMemory = 0x1;
constexpr u32 ChcrChain = 0x4;
constexpr u32 ChcrStart = 0x100;

// D_ENABLEW's suspension of every channel (CPND)
constexpr u32 DmaSuspend = 0x10000;

// The scratchpad's buffers: each macroblock buffer's references (MpegMaxReferences of FetchedReferenceBytes) and macroblock, then
// the prediction
constexpr u32 ScratchpadReferencesBytes = MpegMaxReferences * FetchedReferenceBytes;
constexpr u32 ScratchpadBufferBytes = ScratchpadReferencesBytes + DecodedMacroblockBytes;

inline void WaitIpu()
{
    while (IpuControlRegister{*IpuControl}.busy)
    {
    }
}

// The IPU still works on its command: busy, without an error found
inline bool IpuWorking()
{
    IpuControlRegister control = {*IpuControl};
    return control.busy && !control.errorFound;
}

// An IPU command, kept as the last (_sceMpegCscStoreRefImage's inlined copy of the decoder's macro): the bits read ahead are
// still good after the commands that read bits
inline void SetIpuCommand(MpegSystem* sys, u32 command)
{
    *IpuCommand = command;
    u32 code = command & IpuCodeMask;
    sys->ipuCommand = code;
    if (code == IpuDecodeBlock || code == IpuDecodeVariable || code == IpuDecodeFixed)
    {
        sys->bitsStale = 0;
    }
    else
    {
        sys->bitsStale = 1;
    }
}

inline u32 GlobalPointer()
{
    u32 globalPointer;
    asm volatile("move %0, $gp" : "=r"(globalPointer));
    return globalPointer;
}

// Calls a callback with the global pointer it was set with in $gp (the C++ doesn't use it, the asm and Sony's code do)
inline s32 CallWithGlobalPointer(u32 globalPointer, MpegCallback function, Mpeg* mpeg, void* callbackData, void* user)
{
    u32 saved;
    asm volatile("move %0, $gp\n\tmove $gp, %1" : "=&r"(saved) : "r"(globalPointer) : "memory");
    s32 result = function(mpeg, callbackData, user);
    asm volatile("move $gp, %0" : : "r"(saved) : "memory");
    return result;
}

extern "C"
{
    s32 sceMpegClearRefBuff(Mpeg* mpeg);
    s32 sceMpegDemuxPssRing(Mpeg* mpeg, u8* pss, s32 size, u8* ringStart, s32 ringSize);
}

// libmpeg's own functions (mpeg.cpp, mpegoutput.cpp, mpegdemux.cpp) by their retail names, which the asm calls
namespace Libmpeg
{
// The buffers' places by bufferType, the IPU set to MPEG-1 (until a sequence extension comes)
void SetBuffers(MpegSystem* sys) RETAIL(gcc2_compiled_);
// The IPU's and scratchpad's DMA stopped, the IPU reset
void StopDecoding(MpegSystem* sys) RETAIL(_clearEach);
s32 DispatchCallback(Mpeg* mpeg, MpegCallbackData* callbackData) RETAIL(_sceMpegDispatchMpegCallback);
// The NoData callback; returns 1 whatever it returns
s32 DispatchNoData(Mpeg* mpeg) RETAIL(_sceMpegDispatchMpegCbNodata);
void ArenaInitialise(MpegArena* arena, u8* base, s32 size) RETAIL(FUN_002b88b0);
void ArenaMark(MpegArena* arena) RETAIL(FUN_002b88c8);
// Back to the mark
void ArenaRewind(MpegArena* arena) RETAIL(FUN_002b88d8);
// Null (and an error) when the arena is too small
u8* Allocate(MpegSystem* sys, MpegArena* arena, s32 size, s32 alignment) RETAIL(_sceMpegAlalcAlloc);
// The StopDma and RestartDma callbacks sceMpegCreate sets: the IPU's DMA kept in the state's environment
s32 StopDma(Mpeg* mpeg, void* callbackData, void* user) RETAIL(FUN_002b8958);
s32 RestartDma(Mpeg* mpeg, void* callbackData, void* user) RETAIL(FUN_002b8980);
// A picture's macroblocks copied into the output through the scratchpad (RAW8)
void CopyMacroblocks(MpegSystem* sys, MpegImage* image) RETAIL(gcc2_compiled__002B89A8);
// A frame put out: its times and flags into the Mpeg, its picture into the output
void OutputImage(MpegSystem* sys, MpegImage* image) RETAIL(_dispRefImage);
// A frame of two field pictures put out (the one decoded first first in the times), into the output from image, whose fields
// are its rows in turn
void OutputFields(MpegSystem* sys, MpegImage* image, MpegImage* other) RETAIL(_dispRefImageField);
// The picture still waiting put out at the end. Returns 1 when there was one
s32 Flush(Mpeg* mpeg) RETAIL(_sceMpegFlush);
// The picture to show put out when one is waiting: the B picture just decoded, else the past reference (pictureNumber unused)
void OutputFrame(MpegSystem* sys, s32 pictureNumber, s32 waiting) RETAIL(_sceMpegOutputFrame);
// The IPU's colour conversion of more than 1023 macroblocks, 1023 at a time from the fromIPU channel's interrupt
void ConvertInChunks(MpegSystem* sys, u8* output, s32 macroblocks) RETAIL(gcc2_compiled__002B9468);
// A picture converted to 32 bit pixels into the output by the IPU
void ConvertImage(MpegSystem* sys, MpegImage* image) RETAIL(_sceMpegCscStoreRefImage);
// The DMA interrupt handlers of the fromIPU channel (the next chunk of ConvertInChunks) and the toIPU channel (the next 0xFFFF
// quadwords of the picture to convert)
s32 ConvertedChunk(s32 channel, void* argument, void* address) RETAIL(func_002B9B00);
s32 SentChunk(s32 channel, void* argument, void* address) RETAIL(func_002B9C48);
s32 SkipSystemHeader(PssReader* reader) RETAIL(_system_header);
s32 ReadPackHeader(PssReader* reader, PssPack* pack) RETAIL(_pack_header);
// Returns 0 on an error (a PES extension's pack header, which a program stream can't have)
s32 ReadPesPacket(MpegSystem* sys, PssReader* reader, PesPacket* packet) RETAIL(_PES_packet);
s32 PrintError(const char* message) RETAIL(gcc2_compiled__002BC878);
void StopIpuDma(MpegSystem* sys) RETAIL(_sceMpegStopIpuDma);
// The IPU's error code: the DMA stopped, the IPU reset
void BlockDecodeError(MpegSystem* sys) RETAIL(_sceMpegErrorBdec);
// To the Error callback, or printed without one
s32 Error(MpegSystem* sys, const char* message) RETAIL(_sceMpegError);
s32 ErrorValue(MpegSystem* sys, const char* format, s32 value) RETAIL(_sceMpegError1);

// The pictures' decoding (mpegdecode.cpp)
// The next picture into the output. Returns -1 on an error or at the end, 1 otherwise
s32 GetPicture(Mpeg* mpeg) RETAIL(_getpic);
// The picture whose header was read decoded unless its type's limit is reached (count of limit; -1 has none), else passed over.
// Returns whether it was decoded (0 when the decoding was aborted), MpegSecondFieldMissing for a field picture without its second
// field
s32 DecodeOrSkip(Mpeg* mpeg, s32 count, s32 limit) RETAIL(_decodeOrSkip);
s32 DecodeOrSkipFrame(Mpeg* mpeg, s32 count, s32 limit) RETAIL(_decodeOrSkipFrame);
s32 DecodeOrSkipFields(Mpeg* mpeg, s32 count, s32 limit) RETAIL(_decodeOrSkipField);
// The reference pictures moved on for the picture (secondField: a frame's second field), its values kept in the image it's
// decoded into. Returns whether its references hold pictures
s32 UpdateReferences(MpegSystem* sys, s32 secondField) RETAIL(_updateRefImage);
// Returns whether the picture was decoded
s32 DecodePicture(MpegSystem* sys) RETAIL(_decPicture);
// The picture's slices. Returns 1 when they were decoded, 0 when not, DecodingAborted when the decoding was aborted
s32 DecodePictureData(MpegSystem* sys) RETAIL(_sceMpegPictureData0);
// A slice of the picture (macroblocks of it). Returns a SliceResult
s32 DecodeSlice(MpegSystem* sys, s32 macroblocks) RETAIL(_slice0);
// The macroblock address increment (the macroblocks skipped, plus 1)
s32 MacroblockAddressIncrement(MpegSystem* sys) RETAIL(_sceMpegMbAddressIncrement);
// A coded macroblock's type, motion type, DCT type and vectors, its blocks started on the IPU. Returns 0 on an error
s32 DecodeMacroblock(MpegSystem* sys, s32* type, s32* motionType, s32* dctType, MpegVector predictors[2][2],
                     s32 fieldSelect[2][2], s32* dmVector) RETAIL(_decMB0);
// A skipped macroblock: the previous one's vectors (a P picture's zero), no blocks. Returns 0 in an I picture
s32 SkipMacroblock(MpegSystem* sys, MpegVector predictors[2][2], s32* motionType, s32 fieldSelect[2][2], s32* type)
    RETAIL(_skipMB0);
// A vector from its predictor (sizes of the residuals, from f_code); with dual prime its differential vector too; halveVertical:
// a field's vector in a frame picture, whose predictor is the frame's
void ReadMotionVector(MpegSystem* sys, MpegVector* vector, s32* dmVector, s32 horizontalSize, s32 verticalSize, s32 dualPrime,
                      s32 halveVertical, s32 fullPel) RETAIL(_motionVector);
// The macroblock's vectors of a direction (backward: 1) and their field selects
void ReadMotionVectors(MpegSystem* sys, MpegVector predictors[2][2], s32* dmVector, s32 fieldSelect[2][2], s32 backward,
                       s32 vectorCount, s32 frameVectors, s32 horizontalSize, s32 verticalSize, s32 dualPrime,
                       s32 halveVertical) RETAIL(_motionVectors);
// The fromIPU channel started into address (the scratchpad's through its DMA addresses)
void ReceiveFromIpu(u8* address, s32 size) RETAIL(receiveDataFromIPU);

// The motion compensation (mpegmotion.cpp)
// The macroblock's references gathered and fetched, or none for an intra one, its place in the picture. Returns 0 for a motion
// type it doesn't know
s32 MotionCompensate(MpegSystem* sys, s32 address, s32 increment, s32 type, s32 motionType, MpegVector predictors[2][2],
                     s32 fieldSelect[2][2], s32* dmVector) RETAIL(_motionComp0);
// The prediction's references of the macroblock at x, y (pixels)
void GetReferences(MpegSystem* sys, s32 x, s32 y, s32 type, s32 motionType, MpegVector predictors[2][2], s32 fieldSelect[2][2],
                   s32* dmVector) RETAIL(_getAllRefs);
// A reference of the prediction: height rows from image at x, y moved by the vector (half pixels), of its field's rows with field
// prediction (field: which), into the prediction from destinationRow on (destinationField's rows with field prediction),
// averaged with what's there or not
void GetReference(MpegSystem* sys, MpegImage* image, s32 field, s32 destinationField, s32 destinationRow, s32 height, s32 x,
                  s32 y, s32 vectorX, s32 vectorY, s32 fieldPrediction, s32 average) RETAIL(_getRef0);
// The dual prime vectors of the fields of the other parity (two of them in a frame picture) from the vector and its
// differential
void DualPrimeVectors(MpegSystem* sys, MpegVector* vectors, const s32* dmVector, s32 vectorX, s32 vectorY)
    RETAIL(gcc2_compiled__002BCA90);
// The references' macroblocks fetched into the scratchpad by the toSPR channel's chain
void FetchReferences(MpegSystem* sys) RETAIL(dmaRefImage);
// The macroblock of buffers[index] finished: its prediction made, its pixels stored into the picture
void StoreMacroblock(MpegSystem* sys, s32 index) RETAIL(FUN_002bd9f8);
// A macroblock's 16 bit values clamped to 0-255 into the picture, and the same of a prediction plus the residual
void StorePixels(u8* destination, const s16* values) RETAIL(FUN_002bf508);
void AddPrediction(u8* destination, const s16* prediction, const s16* residual) RETAIL(_copyAddRefImage);

// The bit reader (mpegbits.cpp): the IPU's FDEC reads the next bits (the command register then holds the 32 bits from the new
// position on), VDEC and BDEC move on by themselves; top keeps the 32 bits read ahead, topBits how many of them are good
// The IPU waited for, the NoData callback giving it more of the stream while a decoding command lacks bits
void WaitIpuIdle(MpegSystem* sys) RETAIL(_sceMpegWaitIpuIdle);
// The same waiting for the command register's result, which it returns
u64 WaitIpuResult(MpegSystem* sys) RETAIL(_sceMpegWaitIpuIdle64);
// WaitIpuIdle while the IPU is busy
void WaitIpuIdleIfBusy(MpegSystem* sys) RETAIL(_waitIpuIdle);
u32 NextBits(MpegSystem* sys, s32 count) RETAIL(_sceMpegNextBit);
u32 PeekBits(MpegSystem* sys, s32 count) RETAIL(_sceMpegPeepBit);
void SkipBits(MpegSystem* sys, s32 count) RETAIL(_sceMpegFlushBuf);
// SkipBits through the macroblocks' own stub
void SkipMacroblockBits(MpegSystem* sys, s32 count) RETAIL(_sceMpegFlushBuf_002BF350);

// The headers (mpegheaders.cpp)
// The next picture's header read (and the sequence's and GOP's before it). Returns its picture_coding_type, MpegSequenceEnded at
// the sequence's end, MpegHeaderAborted when aborted
s32 NextHeader(MpegSystem* sys) RETAIL(_sceMpegNextHeader);
// The slice's header (macroblocks unused): its first macroblock's address and the increment to it. Returns SliceHeaderRead,
// SliceGivenUp for an error in the address increment, PictureGivenUp for something not a slice
s32 ReadSliceHeader(MpegSystem* sys, s32 macroblocks, s32* address, s32* increment, MpegVector predictors[2][2])
    RETAIL(gcc2_compiled__002C04D8);
// The picture size the sequence header gave the Mpeg and the reference pictures, their memory allocated anew when it changed
void InitialiseSequence(Mpeg* mpeg) RETAIL(_initSeq);
void ReadSequenceHeader(MpegSystem* sys) RETAIL(_sequenceHeader);
void ReadGroupOfPicturesHeader(MpegSystem* sys) RETAIL(_groupOfPicturesHeader);
void ReadPictureHeader(MpegSystem* sys) RETAIL(_pictureHeader);
// The extensions and user data after a header, up to the next start code of something else
void ExtensionAndUserData(MpegSystem* sys) RETAIL(_extensionAndUserData);
// The extensions by their extension_start_code_identifier, through the table D_00307928
void ReadSequenceExtension(MpegSystem* sys) RETAIL(_sequenceExtension);
void ReadSequenceDisplayExtension(MpegSystem* sys) RETAIL(_sequenceDisplayExtension);
void ReadQuantMatrixExtension(MpegSystem* sys) RETAIL(_quantMatrixExtension);
void ReadCopyrightExtension(MpegSystem* sys) RETAIL(_copyrightExtension);
void ReadPictureDisplayExtension(MpegSystem* sys) RETAIL(_pictureDisplayExtension);
void ReadPictureCodingExtension(MpegSystem* sys) RETAIL(FUN_002c1070);
void ReadSequenceScalableExtension(MpegSystem* sys) RETAIL(func_002C1050);
void ReadPictureSpatialScalableExtension(MpegSystem* sys) RETAIL(func_002C1258);
void ReadPictureTemporalScalableExtension(MpegSystem* sys) RETAIL(FUN_002c1268);
void ReadUnknownExtension(MpegSystem* sys) RETAIL(FUN_002c1060);
// The headers' own copy of WaitIpuIdleIfBusy and stub of SkipBits
void WaitIpuIdleIfBusyForHeaders(MpegSystem* sys) RETAIL(_waitIpuIdle_002C1448);
void SkipHeaderBits(MpegSystem* sys, s32 count) RETAIL(_sceMpegFlushBuf_002C1480);
// The bits up to the next byte skipped, and a start code's 32
void SkipToByte(MpegSystem* sys) RETAIL(flushByteBoundary);
void SkipStartCode(MpegSystem* sys) RETAIL(FUN_002c15e0);

// The stream up to the next start code (its 0x000001) skipped
inline void NextStartCode(MpegSystem* sys)
{
    SkipToByte(sys);
    while (PeekBits(sys, 24) != 1 && sys->aborted == 0)
    {
        SkipHeaderBits(sys, 8);
    }
}
}

// libipu (ipu.cpp)
namespace Libipu
{
// The IPU channels' CHCR written with the DMA controller suspended
void SetFromIpuChcr(u32 chcr) RETAIL(setD3_CHCR);
void SetToIpuChcr(u32 chcr) RETAIL(setD4_CHCR);
// The same as SetToIpuChcr, sceIpuInit's own copy
void SetToIpuChcrForInit(u32 chcr) RETAIL(setD4_CHCR_002C1BE8);
}
