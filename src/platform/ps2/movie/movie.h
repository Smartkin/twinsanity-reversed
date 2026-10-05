#pragma once

#include "common.h"
#include "platform/movie.h"

// The PS2's movie player. Sony's libmpeg demultiplexes the PSS file's sectors (read through the disc drive's stream) into a ring of
// video for the IPU and a ring of sound, and decodes the pictures with the IPU into a buffer of 32 bit pixels; an upload chain
// sends the buffer to the GS's memory from the vertical blank's interrupt, and the sound goes through a ring in the I/O processor's
// memory that the sound processor's block transfer plays in a loop. libmpeg and libipu are Sony's code in C++ (mpeg.h)

// A quadword's bytes (what DMA, the IPU's FIFOs and the GS's transfers count in)
constexpr u32 QuadwordBytes = 0x10;

// libmpeg's state (mpeg.h)
struct MpegSystem;

// libmpeg's decoder (sceMpeg): the picture's size and times, its own state behind sys
struct Mpeg
{
    s32 width;
    s32 height;
    s32 frameCount;
    s64 pts;
    s64 dts;
    u64 flags;
    s64 pts2nd;
    s64 dts2nd;
    u64 flags2nd;
    MpegSystem* sys;
};
CHECK_SIZE(Mpeg, 0x48);

// IPU_CTRL, the IPU's control (the EE manual's fields): the quadwords in its input and output FIFOs (IFC, OFC), the coded block
// pattern (CBP), an error code (ECD) or a start code (SCD) found, the picture's settings it decodes with (intra_dc_precision,
// alternate_scan, intra_vlc_format, q_scale_type, an MPEG-1 stream and picture_coding_type), RST (written: the IPU reset) and BUSY
union IpuControlRegister
{
    u32 value;
    struct
    {
        u32 inputQuadwords : 4;
        u32 outputQuadwords : 4;
        u32 codedBlockPattern : 6;
        u32 errorFound : 1;
        u32 startCodeFound : 1;
        u32 dcPrecision : 2;
        u32 unused18 : 2;
        u32 alternateScan : 1;
        u32 intraVlcFormat : 1;
        u32 quantiserScaleType : 1;
        u32 mpeg1 : 1;
        u32 pictureCodingType : 3;
        u32 unused27 : 3;
        u32 reset : 1;
        u32 busy : 1;
    };
};
CHECK_SIZE(IpuControlRegister, 4);

// IPU_BP, the IPU's place in its input: the bit position in what it reads (BP), the quadwords in the input FIFO (IFC) and the ones
// it took from the FIFO to read (FP, 0 to 2)
union IpuBitPositionRegister
{
    u32 value;
    struct
    {
        u32 bitPosition : 7;
        u32 unused7 : 1;
        u32 fifoQuadwords : 4;
        u32 unused12 : 4;
        u32 heldQuadwords : 2;
        u32 unused18 : 14;
    };
};
CHECK_SIZE(IpuBitPositionRegister, 4);

// libipu's copy of the IPU's DMA channels (sceIpuDmaEnv): the toIPU channel's, the fromIPU channel's and the IPU's bit position
// and control
struct IpuDmaEnvironment
{
    u32 toMadr;
    u32 toTadr;
    u32 toQwc;
    u32 toChcr;
    u32 fromMadr;
    u32 fromQwc;
    u32 fromChcr;
    IpuBitPositionRegister bitPosition;
    IpuControlRegister control;
};
CHECK_SIZE(IpuDmaEnvironment, 0x24);

// What libmpeg hands a stream's callback: the stream's packet, its header and data (sceMpegCbDataStr)
struct MpegStreamData
{
    u32 type;
    u8* header;
    u8* data;
    u32 length;
    s64 pts;
    s64 dts;
};
CHECK_SIZE(MpegStreamData, 0x20);

// The sound's header, the first 0x28 bytes of its stream ("SShd" then "SSbd"); the loop's start and end blocks after the
// interleave aren't read
struct MovieAudioHeader
{
    u32 id;
    u32 size;
    u32 type;
    u32 rate;
    s32 channels;
    s32 interleave;
    u32 unused18;
    u32 unused1C;
    u32 bodyId;
    u32 bodySize;
};
CHECK_SIZE(MovieAudioHeader, 0x28);

// MovieDecoder::container: a raw stream (all of it video) or a PSS file
enum MovieContainer : u32
{
    MovieRawStream = 0,
    MoviePss = 1,
};

// MovieDecoder::format: the IPU's own streams, which the IPU decodes without libmpeg (a picture's macroblocks row by row, or
// column by column), or MPEG-2 through libmpeg (whose pictures are in columns too)
enum MovieFormat : u32
{
    MovieIpuRows = 0,
    MovieIpuColumns = 1,
    MovieMpeg2 = 2,
};

// MovieDecoder::audio: the sound's stream
enum MovieAudioStream : u32
{
    MovieNoAudio = 0,
    MoviePcm = 1,
    MovieAdpcm = 2,
    MovieAudioData = 3,
};

// MovieDecoder::outputFormat: 32 bit pixels or 16 bit ones; the second of each also described the picture for a loader of images (a
// stub in retail)
enum MovieOutputFormat : u32
{
    MovieRgb32 = 0x100,
    MovieRgb32Image = 0x101,
    MovieRgb16 = 0x102,
    MovieRgb16Image = 0x103,
    MovieOutputFormatsEnd = 0x104,
};

// The decoder: the file's sectors are read into readBuffer and demultiplexed into the video ring (the IPU's input, which the IPU's
// DMA takes from next on: read to next is being sent) and the sound's ring. The IPU's own streams are decoded without libmpeg; the
// movies are MPEG-2 through libmpeg
struct MovieDecoder
{
    // An upload: the DMA chain sending a picture buffer to the GS, a packet a macroblock (MovieIpuRows) or a column of them
    struct Upload
    {
        u8* packet;
        u8 unused04[0x1C];
    };

    Mpeg mpeg;
    u8* readBuffer;
    // What's left of the read buffer's data
    s32 pending;
    // The IPU's streams: the pictures in the file
    s32 pictureCount;
    s32 decoded;
    u32 width;
    u32 height;
    // The bytes read (-1 at the end), and the seek to the start
    s32 (*read)(u8* buffer, u32 size, const char* path);
    s32 (*seek)(const char* path);
    const char* path;
    // The GS buffer's width in 64 pixels the uploads send to
    u32 widthBlocks;
    IpuDmaEnvironment ipuDma;
    // A picture buffer for even pictures and one for odd ones when double buffered
    u8* pictures[2];
    // The 16 bit output's intermediate macroblocks
    u8* raw8;
    u32 unusedA0;
    u8* work;
    Upload uploads[2];
    u32 uploadIds[2];
    u8* videoBase;
    s32 videoSize;
    u8* videoWrite;
    u8* videoRead;
    u8* videoNext;
    u8* audioBase;
    s32 audioSize;
    s32 audioRead;
    s32 audioCount;
    MovieAudioHeader audioHeader;
    // A sound packet didn't fit in the ring
    u32 audioFull;
    s32 audioHeaderBytes;
    MovieContainer container;
    MovieFormat format;
    MovieAudioStream audio;
    u32 audioChannel;
    MovieOutputFormat outputFormat;
    u32 doubleBuffered;
    u32 ended;
};
CHECK_SIZE(MovieDecoder, 0x160);
CHECK_OFFSET(MovieDecoder, uploads, 0xA8);
CHECK_OFFSET(MovieDecoder, videoBase, 0xF0);
CHECK_OFFSET(MovieDecoder, audioHeader, 0x114);
CHECK_OFFSET(MovieDecoder, container, 0x144);

// The decoded pictures' uploads waiting for the vertical blank
constexpr u32 PictureQueueSize = 2;

// The player, from the movie controller's 0x30 (the offsets in the comments and names are the controller's)
struct Platform::Movie::Player
{
    // The decoder's settings
    MovieFormat format;
    MovieAudioStream audio;
    // The decoded pictures' uploads waiting for the vertical blank, and the one decoded last
    u8* queue[PictureQueueSize];
    volatile u32 queued;
    u8* decoded;
    // The GS's memory the pictures go to (in blocks of 64 words), the Z buffer's: the retail player kept it at 0x1C
    u32 frameBase;
    // The decoder's container, which the retail player kept at 0x2C
    MovieContainer container;
    u8 unused50[0x30];
    // 0x80
    MovieDecoder decoder;
    // 0x1E0: 1 once the decoder opened
    s32 opened;
    // Pictures decoded since it opened, but the first two
    s32 frames;
    u8 unused1E8[8];
    // 0x1F0: the scratchpad while the decoder has it (libmpeg works in it, and the main thread's stack is there), which is the
    // stack meanwhile: the stack pointer in this copy, and the one it was
    alignas(16) u8 scratchpad[0x4000];
    u32 stackPointer;
    u32 savedStackPointer;
    // The disc stream's buffer in the I/O processor's memory, and whether it's the game's
    void* discBuffer;
    u8 discBufferLent;
    u8 unused41FD[3];
};
CHECK_SIZE(Platform::Movie::Player, Platform::Movie::PlayerSize);
CHECK_OFFSET(Platform::Movie::Player, decoder, 0x50);
CHECK_OFFSET(Platform::Movie::Player, scratchpad, 0x1C0);
CHECK_OFFSET(Platform::Movie::Player, discBuffer, 0x41C8);

using MpegCallback = s32 (*)(Mpeg* mpeg, void* callbackData, void* user);

// sceMpegCbType: what a callback is called for
enum MpegCallbackType : s32
{
    MpegCallbackError = 0,
    // The IPU needs more of the video
    MpegCallbackNoData = 1,
    MpegCallbackStopDma = 2,
    MpegCallbackRestartDma = 3,
    // While the IPU converts a picture
    MpegCallbackBackground = 4,
    MpegCallbackTimeStamp = 5,
    // A stream's packet (sceMpegAddStrCallback's)
    MpegCallbackStream = 6,
    MpegCallbackTypes = 7,
};

// sceMpegStrType: the streams sceMpegAddStrCallback takes (M2V, IPU, PCM, ADPCM, DATA, MPEG audio, AC-3, LPCM, DTS, SDDS), the
// indexes of their MpegStreamType
enum MpegStreamKind : s32
{
    MpegVideoStream = 0,
    MpegIpuStream = 1,
    MpegPcmStream = 2,
    MpegAdpcmStream = 3,
    MpegDataStream = 4,
    MpegAudioStream = 5,
    MpegAc3Stream = 6,
    MpegLpcmStream = 7,
    MpegDtsStream = 8,
    MpegSddsStream = 9,
    MpegStreamKinds = 10,
};

// sceIpuSync's modes: wait for the IPU, or return whether it's busy
enum IpuSyncMode : s32
{
    IpuSyncWait = 0,
    IpuSyncPoll = 1,
};

extern "C"
{
    // libipu
    void sceIpuInit();
    s32 sceIpuSync(s32 mode, u16 timeout);
    void sceIpuStopDMA(IpuDmaEnvironment* environment);
    void sceIpuRestartDMA(IpuDmaEnvironment* environment);

    // libmpeg
    s32 sceMpegInit();
    s32 sceMpegCreate(Mpeg* mpeg, u8* work, s32 size);
    s32 sceMpegDelete(Mpeg* mpeg) RETAIL(FUN_002b8498);
    s32 sceMpegReset(Mpeg* mpeg);
    s32 sceMpegIsEnd(Mpeg* mpeg) RETAIL(FUN_002b85c8);
    // The stream has no more data (Sony's sceMpegGetPictureAbort: the picture being decoded is given up, and the decoder needs
    // sceMpegReset before the next)
    void MpegStreamEnded(Mpeg* mpeg) RETAIL(FUN_002b8538);
    s32 sceMpegGetPicture(Mpeg* mpeg, u8* rgb32, s32 macroblocks);
    s32 sceMpegGetPictureRAW8(Mpeg* mpeg, u8* raw8, s32 macroblocks);
    // Returns the bytes it took
    s32 sceMpegDemuxPss(Mpeg* mpeg, u8* pss, s32 size);
    void* sceMpegAddCallback(Mpeg* mpeg, s32 type, MpegCallback callback, void* user);
    void* sceMpegAddStrCallback(Mpeg* mpeg, s32 type, s32 channel, MpegCallback callback, void* user);
}

// The decoder (decoder.cpp). One at a time: the functions work on the one opened last
namespace MovieDecoding
{
// sceIpuInit, sceMpegInit and the IPU's DMA channels' settings, once
void Initialise();
// The decoder's buffers come from memory nothing else uses meanwhile (the renderer's DMA buffers): an arena from begin on
void SetArena(u8* begin, u8* end);
s32 Open(MovieDecoder* decoder, MovieContainer container, MovieFormat format, MovieAudioStream audio, u32 audioChannel,
         s32 (*seek)(const char* path), s32 (*read)(u8* buffer, u32 size, const char* path), const char* path, u32 doubleBuffered,
         u32 widthBlocks, MovieOutputFormat outputFormat);
// Decodes the next picture and points its upload at the GS's memory (frameBase, in blocks): the upload goes to *upload. Returns
// whether there was one
s32 DecodeAndUpload(u8** upload, u32 frameBase);
// Hands the sound in the ring to consume, which returns how much it took
void FeedAudio(u32 (*consume)(u8* ring, u32 size, u32 read, u32 count, u32 user), u32 user);
bool Finished();
// Reads and demultiplexes the next sectors, without waiting for the disc
void ReadOn();
void Close();
// Starts a chain of the DMA channel at the tag
void StartChainDma(u32 channel, const void* tag);
}
