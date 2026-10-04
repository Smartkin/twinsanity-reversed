#pragma once

#include "common.h"
#include "platform/movie.h"

// The PS2's movie player. Sony's libmpeg demultiplexes the PSS file's sectors (read through the disc drive's stream) into a ring of
// video for the IPU and a ring of sound, and decodes the pictures with the IPU into a buffer of 32 bit pixels; an upload chain
// sends the buffer to the GS's memory from the vertical blank's interrupt, and the sound goes through a ring in the I/O processor's
// memory that the sound processor's block transfer plays in a loop. libmpeg and libipu are Sony's code in C++ (mpeg.h)

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
    u32 bitPosition;
    u32 control;
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

// The sound's header, the first 0x28 bytes of its stream ("SShd" then "SSbd")
struct MovieAudioHeader
{
    u32 id;
    u32 size;
    u32 type;
    u32 rate;
    s32 channels;
    s32 interleave;
    u32 unknown18;
    u32 unknown1C;
    u32 bodyId;
    u32 bodySize;
};
CHECK_SIZE(MovieAudioHeader, 0x28);

// The decoder: the file's sectors are read into readBuffer and demultiplexed into the video ring (the IPU's input, which the IPU's
// DMA takes from next on: read to next is being sent) and the sound's ring. Formats 0 and 1 are the IPU's own streams, which the
// IPU decodes without libmpeg; the movies are format 2, MPEG-2 through libmpeg
struct MovieDecoder
{
    // An upload: the DMA chain sending a picture buffer to the GS, a packet a macroblock (format 0) or a column of them
    struct Upload
    {
        u8* packet;
        u8 unknown04[0x1C];
    };

    Mpeg mpeg;
    u8* readBuffer;
    // What's left of the read buffer's data
    s32 pending;
    // Formats 0 and 1: the pictures in the file
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
    u32 unknownA0;
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
    // 0: a raw stream, 1: a PSS file
    u32 mode;
    u32 format;
    // 0 none, 1 PCM, 2 ADPCM, 3 data
    u32 audio;
    u32 audioChannel;
    // 0x100 and 0x101: 32 bit pixels, 0x102 and 0x103: 16 bit
    u32 outputFormat;
    u32 doubleBuffered;
    u32 ended;
};
CHECK_SIZE(MovieDecoder, 0x160);
CHECK_OFFSET(MovieDecoder, uploads, 0xA8);
CHECK_OFFSET(MovieDecoder, videoBase, 0xF0);
CHECK_OFFSET(MovieDecoder, audioHeader, 0x114);
CHECK_OFFSET(MovieDecoder, mode, 0x144);

// The player, from the movie controller's 0x30 (the offsets in the comments are the controller's)
struct Platform::Movie::Player
{
    // The decoder's settings
    u32 format;
    u32 audio;
    // The decoded pictures' uploads waiting for the vertical blank, and the one decoded last
    u8* queue[2];
    volatile u32 queued;
    u8* decoded;
    // The GS's memory the pictures go to (in blocks of 64 words), the Z buffer's: the retail player kept it at 0x1C
    u32 frameBase;
    // The decoder's mode, which the retail player kept at 0x2C
    u32 mode;
    u8 unknown50[0x30];
    // 0x80
    MovieDecoder decoder;
    // 0x1E0: 1 once the decoder opened
    s32 opened;
    // Pictures decoded since it opened, but the first two
    s32 frames;
    u8 unknown1E8[8];
    // 0x1F0: the scratchpad while the decoder has it (libmpeg works in it, and the main thread's stack is there), which is the
    // stack meanwhile: the stack pointer in this copy, and the one it was
    alignas(16) u8 scratchpad[0x4000];
    u32 stackPointer;
    u32 savedStackPointer;
    // The disc stream's buffer in the I/O processor's memory, and whether it's the game's
    void* discBuffer;
    u8 discBufferLent;
    u8 unknown41FD[3];
};
CHECK_SIZE(Platform::Movie::Player, Platform::Movie::PlayerSize);
CHECK_OFFSET(Platform::Movie::Player, decoder, 0x50);
CHECK_OFFSET(Platform::Movie::Player, scratchpad, 0x1C0);
CHECK_OFFSET(Platform::Movie::Player, discBuffer, 0x41C8);

using MpegCallback = s32 (*)(Mpeg* mpeg, void* data, void* user);

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
s32 Open(MovieDecoder* decoder, u32 mode, u32 format, u32 audio, u32 audioChannel, s32 (*seek)(const char* path),
         s32 (*read)(u8* buffer, u32 size, const char* path), const char* path, u32 doubleBuffered, u32 widthBlocks,
         u32 outputFormat);
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
