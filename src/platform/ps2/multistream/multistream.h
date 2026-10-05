#pragma once

#include "common.h"
#include "platform/audio.h"

#include <sifrpc.h>

// SCEE's MultiStream 7.2 (its SDK's EE library, sound.c: the SOUND_ names in the comments), the EE side of the IOP's sound and
// streaming module (the disc's STREAM.IRX, "multi_streamer"). The EE batches commands for the IOP, sends them once a frame (or
// when the batch is full) to the module's RPC server 0x12345 and gets the module's status back in the reply, which it keeps for
// the queries. The module's own loading into the EE's memory ("fast load") calls the EE back through a server of the EE's own
// (0x12344321). A command is an opcode and 16 bit words, staged one at a time and then committed to the batch. A channel is one
// of the SPU2's 48 voices (core * 24 + voice), a stream one of the module's streams (the platform layer's channels)
namespace MultiStream
{
constexpr u32 Streams = 48;
constexpr u32 Channels = Platform::Audio::Voices;
// What a channel's stream is when none plays on it, and the status request's stream when none may load
constexpr u8 NoStream = 0xFF;
constexpr u16 NoStreamAllowed = 0xFFFF;
// The batch: a header of 2 words (the module's version and the count of commands), then per command its opcode, its length and
// its words
constexpr u32 BatchWords = 0x400;
constexpr s32 BatchHeaderWords = 2;
constexpr s32 CommandHeaderWords = 2;
// SOUND_MULTISTREAM_VERSION_ID, 7.2
constexpr u16 Version = 72;
// What's sent, and the reply
constexpr u32 TransferSize = 0x800;
constexpr s32 NoCommand = -1;

// How MsSend sends (sceSifCallRpc's mode): waiting for the call before and after, or giving up while one is out and not
// waiting for its end (SIF_RPC_M_NOWAIT)
enum SendMode : u32
{
    SendWait = 0,
    SendNoWait = 1,
};

// The commands (the SDK's SND_ names); the module's own additions start at 0x101
enum Opcode : s32
{
    OpPlayStream = 0x1,
    OpSetVolume = 0x2,
    OpSetPitch = 0x3,
    // SND_STOP_SFX: keys the channel off
    OpStopSound = 0x4,
    OpStopStream = 0x5,
    OpInitSpu = 0x7,
    // SND_GET_CD_STATUS: every batch's last command, the stream allowed to load (-1 for none) and a check value
    OpGetStatus = 0x9,
    OpPause = 0xA,
    OpResume = 0xB,
    OpInitStreamData = 0xC,
    OpAllocateStreamBuffer = 0xD,
    OpSetSpuWriteAddress = 0xE,
    OpCloseStreamBuffer = 0xF,
    OpSetMaxStreamLimit = 0x10,
    OpEnableEffect = 0x13,
    OpDisableEffect = 0x14,
    OpSetEffectVolume = 0x15,
    OpEffectChannelOn = 0x18,
    OpEffectChannelOff = 0x19,
    // SND_PATCH_SFX: where a sound bank's samples are
    OpPatchSoundBank = 0x1C,
    OpCreateFileInfo = 0x1F,
    OpFreeFileId = 0x20,
    // SND_ALLOCATE_SFX_RAM: the module's table of sounds
    OpAllocateSoundTable = 0x25,
    OpFreeSound = 0x28,
    OpSetFileOffsetAndSize = 0x29,
    OpInitDisc = 0x2A,
    // Interleaved streams: a parent reads the file, its children play the other parts
    OpSetStreamParent = 0x2B,
    OpSetStreamChild = 0x2C,
    OpGetMaxIopMemory = 0x2D,
    OpAllowKeyOn = 0x2E,
    OpDisableKeyOn = 0x2F,
    OpSetEeWriteAddress = 0x31,
    OpSetIopWriteAddress = 0x32,
    OpRestartFromDiscError = 0x35,
    OpCheckDiscError = 0x37,
    OpPlaySoundLoop = 0x39,
    // The SPU2's DMA transfer handler (DMA channel 1's) taken away and put back
    OpDisableSpuCallback = 0x3B,
    OpEnableSpuCallback = 0x3C,
    // SND_INIT_WAIT: the address the IOP's thread counts its runs at (0 for none)
    OpInitWait = 0x49,
    // SND_INTERNAL_FASTLOAD: a FastLoadCommand and its value
    OpFastLoad = 0x51,
    OpResizeSpuBuffer = 0x52,
    // SND_SET_PARAM: a voice's parameter (SdVoiceAttribute)
    OpSetParameter = 0x5C,
    OpSetVolumeSmooth = 0x60,
    // SND_SET_MIB_END: where an interleaved stream's data ends
    OpSetMibEnd = 0x63,
    // The game's: a sound bank's samples are all in the SPU2's memory, and a step of moving the sounds' memory together
    OpSoundBankLoaded = 0x103,
    OpCompactSoundMemory = 0x104,
};

// OpFastLoad's commands (SOUND_FASTLOAD_BIND_RPC, SOUND_FASTLOAD_ON_OFF) and the fast load's modes (SOUND_FASTLOAD_*): off, on,
// the loading stopped, allowed one more transfer or every one, the next file loaded (SOUND_SetFastLoadNext's), the EE's cache
// invalidated before the IOP writes the EE's memory or not
enum FastLoadCommand : u16
{
    FastLoadBindRpc = 0,
    FastLoadOnOff = 1,
};

enum FastLoadMode : s32
{
    FastLoadOff = 0,
    FastLoadOn = 1,
    FastLoadStop = 2,
    FastLoadSingle = 3,
    FastLoadContinue = 4,
    FastLoadNext = 5,
    FastLoadInvalidateCacheOff = 6,
    FastLoadInvalidateCacheOn = 7,
};

// What a stream plays or loads (MsPlayStream's mode, STREAM_*): ADPCM music looping or once, the EE's memory looping over the
// destination, MultiStream's own loading, the IOP's memory, the EE's memory, the SPU2's memory
enum StreamMode : u32
{
    StreamLooping = 0,
    StreamOnce = 1,
    LoadToEeLooping = 0x7B,
    LoadByModule = 0x7C,
    LoadToIop = 0x7D,
    LoadToEe = 0x7E,
    LoadToSpu = 0x7F,
};

// The EE's states of a stream (SOUND_STREAM_STATUS): off, reported playing by the IOP, a play or a stop asked for, a stop asked
// for while the IOP still reports it, a play sent (off once three replies haven't reported it), and what the status queries
// give while a play is on its way
enum StreamState : s8
{
    StreamOff = 0,
    StreamOn = 1,
    StreamPlayRequested = 2,
    StreamStopRequested = 3,
    StreamStopPending = 4,
    StreamPlaySent = 5,
    StreamWaitingToPlay = 6,
};

constexpr u8 PlaySentReplies = 3;

// The channels' states (SOUND_SPUKeyStatus): off, keyed on, used by a stream, a play asked for (until the IOP reports it) and
// waiting one more reply. The reply gives the IOP's, 2 bits a channel, 16 channels a word from the top bits down
enum ChannelState : u8
{
    ChannelOff = 0,
    ChannelOn = 1,
    ChannelStreaming = 2,
    ChannelRequested = 3,
    ChannelKeyedOn = 4,
};

constexpr u32 ChannelStateBits = 2;
constexpr u32 ChannelStateMask = 0x3;
constexpr u32 ChannelsPerWord = 16;
constexpr u32 ChannelStateWords = Channels / ChannelsPerWord;

// A channel's master volume group (1 to 4, 0 for none: SOUND_MASTER_VOLn) goes in the top half of a channel's word and stays
// there: the channels keep it as it is
constexpr u32 ChannelMask = 0xFFFF;
constexpr u32 GroupMask = 0xFFFF0000;
constexpr u32 Groups = 4;
// A group's volume's fraction bits (Platform::Audio::FullGroupVolume, SOUND_MASTER_VOL_FULL, is 1)
constexpr u32 GroupVolumeShift = 12;

// A stream's priority (SOUND_PRIORITY_INIT to start with); the EE's own value is kept through the next reply
constexpr u16 InitialPriority = 0x80;
constexpr u16 PriorityKept = 0x8000;

// A stream's record in the reply: the disc's access for it (0 none, 1 seeking, 2 reading), the half of its buffer playing, the
// stream, its channel and its envelope's value
union RecordHeader
{
    u32 value;
    struct
    {
        u32 discAccess : 2;
        u32 playHalf : 2;
        u32 stream : 6;
        u32 channel : 6;
        u32 envelope : 16;
    };
};
CHECK_SIZE(RecordHeader, 4);

// The stream's kind (AudioStream, else the LoadTo mode it loads by) and whether it's active (its data preloaded or its channel
// keyed on)
union RecordType
{
    u32 value;
    struct
    {
        u32 kind : 8;
        u32 unused8 : 23;
        u32 active : 1;
    };
};
CHECK_SIZE(RecordType, 4);
constexpr u8 AudioStream = 0;

// The channel's pitch and the stream's priority
union RecordPitch
{
    u32 value;
    struct
    {
        s32 pitch : 16;
        u32 priority : 16;
    };
};
CHECK_SIZE(RecordPitch, 4);

// The SPU2 address's bits
constexpr u32 SpuAddressMask = 0xFFFFFF;

// A stream's record: its header, where it plays in the SPU2's memory, its file, its kind, where it writes (in the SPU2's, the
// IOP's or the EE's memory), its offset in the file, how much it has loaded into the EE's memory, its pitch and priority and
// how long it has played (1/60 s)
struct StreamRecord
{
    RecordHeader header;
    u32 spuAddress;
    u32 file;
    RecordType type;
    u32 writeAddress;
    u32 playOffset;
    u32 eeDataSize;
    RecordPitch pitchAndPriority;
    u32 time;
};
static_assert(sizeof(StreamRecord) == 36);

// The reply's disc errors: an error stopped the loading (the streams paused), MultiStream isn't back from one, the disc isn't
// ready
union DiscErrors
{
    u32 value;
    struct
    {
        u32 error : 1;
        u32 internalError : 1;
        u32 notReady : 1;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(DiscErrors, 4);

// A file query's counter (a check that the result is the one asked for) and where the file is (host or disc)
union FileQuery
{
    u32 value;
    struct
    {
        u32 source : 8;
        s32 counter : 24;
    };
};
CHECK_SIZE(FileQuery, 4);

// An external loading's chain of files
constexpr u32 ChainFiles = 9;

// The reply: the count of records (in its first word's low byte) and the records, then the module's status. Its counter
// changes with every status, a stream that wants more data (-1 for none), whether the disc is busy, where the SPU2's next
// write goes, whether a stereo pair started (0 when it did), the channels' states, an external loading's request (where the
// data goes in the IOP's memory, its size, a check value, and the files of its chain: their seeks, IDs, sizes to play and
// sectors, the chain's count and its position), the largest IOP memory free, the file query's result (FileQuery, size and
// sector), what's left of the file and where in it, where the EE's and the IOP's next writes go, the disc's errors, how the
// IOP loads (a StreamMode), the DTS encoder's status, the last transfer to the EE and how many there were, the PCM playback's
// buffer, size, address and status, and the streams' IOP buffers
struct ReplyStatus
{
    u32 counter;
    s32 loadRequest;
    u32 discBusy;
    u32 spuWriteAddress;
    u32 stereoNotStarted;
    u32 channelStates[ChannelStateWords];
    u32 iopDataAddress;
    u32 iopDataSize;
    u32 iopDataCheck;
    u32 iopDataSeeks[ChainFiles];
    u32 iopDataFiles[ChainFiles];
    u32 iopDataPlaySizes[ChainFiles];
    u32 iopDataSectors[ChainFiles];
    u32 iopDataChainCount;
    u32 iopDataChainPosition;
    u32 maxIopMemory;
    FileQuery fileQuery;
    u32 fileSize;
    u32 fileSector;
    u32 iopDataRemaining;
    u32 iopDataOffset;
    u32 eeWriteAddress;
    u32 iopWriteAddress;
    DiscErrors discErrors;
    u32 iopLoadType;
    u32 dtsStatus;
    u32 eeTransferSize;
    u32 eeTransferCount;
    u32 pcmIopAddress;
    u32 pcmSize;
    u32 pcmAddress;
    u32 pcmStatus;
    s32 iopBufferCount;
    u32 iopBuffers[];
};
CHECK_OFFSET(ReplyStatus, channelStates, 5 * 4);
CHECK_OFFSET(ReplyStatus, maxIopMemory, 49 * 4);
CHECK_OFFSET(ReplyStatus, discErrors, 57 * 4);
CHECK_OFFSET(ReplyStatus, iopBufferCount, 66 * 4);

// The DSP effect's state in the reply
constexpr u32 DspInfoWords = 8;

// After the IOP buffers (and a word the EE skips): the user transfer's status, the channels' next addresses (their NAX), a word
// the EE skips, the IOP's two debug words, the DSP effect's state and buffer, and its transfers
struct ReplyChannels
{
    u32 unused00;
    u32 userTransferStatus;
    u32 nextAddresses[Channels];
    u32 unusedC8;
    u32 debugScan[2];
    u32 dspInfo[DspInfoWords];
    u32 dspBuffer;
    s32 dspTransferCount;
};
CHECK_OFFSET(ReplyChannels, debugScan, 51 * 4);
CHECK_OFFSET(ReplyChannels, dspTransferCount, 62 * 4);

// A transfer of the DSP effect's data, in the reply and as the EE keeps it
struct ReplyDspTransfer
{
    u32 counter;
    u32 iopAddress;
    u32 eeAddress;
};

struct DspTransfer
{
    u32 iopAddress;
    u32 eeAddress;
    u32 counter;
};

// After the DSP transfers: the DSP effect's status, the disc's error code (sceCdGetError's), and the ATWinMon debugger's file:
// whether it's open and its handle
struct ReplyEnd
{
    u32 dspStatus;
    u32 discErrorCode;
    s32 atWinMonOpen;
    s32 atWinMonFile;
};

// The DSP effect's handshake with the IOP (SOUND_DSP_CHECKSUM): idle (the reply came), its data waiting for a batch, the batch
// sent
enum DspHandshake : s32
{
    DspIdle = 0,
    DspDataWaiting = 1,
    DspDataSent = 2,
};

// What the fast load's IOP call sends (SOUND_FASTLOAD_RPC's packet)
struct FastLoadCall
{
    s32 stream;
    s32 totalSize;
    s32 loadSize;
    s32 status;
    u32 eeAddress;
    s32 counter;
    s32 file;
};
}

extern "C"
{
    // The command being staged: its opcode (NoCommand when none) and its words
    extern s32 g_MsCommand RETAIL(D_00309F94);
    extern s32 g_MsCommandLength RETAIL(UnkGlobalCounter);
    extern u16 g_MsCommandWords[MultiStream::BatchWords] RETAIL(D_003B2B68);
    // A full batch couldn't be sent: the words of the commands staged until the next one goes are dropped
    extern s16 g_MsBatchFailed RETAIL(D_00309F70);
    // The status request is in the batch already (MsSend adds one otherwise); Commit sets it while it sends a full batch, which
    // gets none
    extern s8 g_MsStatusRequested RETAIL(D_00309F48);
    // The batch being filled
    extern u16 g_MsBatch[MultiStream::BatchWords] RETAIL(D_003B1B40);
    // The batch's words after its header, and its commands
    extern s32 g_MsBatchLength RETAIL(D_00309E84);
    extern s32 g_MsBatchCommands RETAIL(D_00309F98);
    // A call is out: its reply clears it (in the interrupt that ends the call)
    extern volatile s32 g_MsBusy RETAIL(D_00309EA0);
    // The DSP effect's handshake with the IOP (DspHandshake), and its count of checks, cleared with it
    extern s32 g_MsDspChecksum RETAIL(D_00309FD4);
    extern s32 g_MsDspCheckCount RETAIL(D_00309FD8);
    extern SifRpcClientData_t g_MsClient RETAIL(D_003B2B40);
    extern u16 g_MsSendBuffer[MultiStream::TransferSize / sizeof(u16)] RETAIL(G_DataBuffer_UnkRPC_Call);
    extern u32 g_MsReply[MultiStream::TransferSize / sizeof(u32)] RETAIL(G_ReceiveBuffer_UnkRPC_Call);

    // The status of the last reply. The IOP counts its status, a reply with the same count is old
    extern u32 g_MsReplyCounter RETAIL(D_00309EE8);
    extern u32 g_MsLastReplyCounter RETAIL(D_00309F74);
    // The disc's access for a stream in the last reply (RecordHeader::discAccess) and the stream
    extern u32 g_MsDiscAccess RETAIL(D_00309EDC);
    extern u32 g_MsDiscAccessStream RETAIL(D_00309EE0);

    // The streams' states (StreamState), and the replies a play sent waits for its stream
    extern s8 g_MsStreamStates[MultiStream::Streams] RETAIL(D_003B4080);
    extern u8 g_MsStreamCountdown[MultiStream::Streams] RETAIL(D_003B40B0);
    // The stream playing on each channel, 0xFF for none
    extern u8 g_MsChannelStreams[MultiStream::Channels] RETAIL(D_002E7D90);
    // The records' fields, by stream (the status queries hand them out)
    extern u8 g_MsStreamPlayHalves[MultiStream::Streams] RETAIL(D_003B4170);
    extern s16 g_MsStreamEnvelopes[MultiStream::Streams] RETAIL(D_003B41A0);
    extern u32 g_MsStreamSpuAddresses[MultiStream::Streams] RETAIL(D_003B4200);
    extern u32 g_MsStreamFiles[MultiStream::Streams] RETAIL(D_003B3F00);
    extern u8 g_MsStreamActive[MultiStream::Streams] RETAIL(D_003B4140);
    extern u8 g_MsStreamTypes[MultiStream::Streams] RETAIL(D_003B40E0);
    extern u8 g_MsStreamChannels[MultiStream::Streams] RETAIL(D_003B4110);
    extern u32 g_MsStreamWriteAddresses[MultiStream::Streams] RETAIL(D_003B4420);
    extern u32 g_MsStreamPlayOffsets[MultiStream::Streams] RETAIL(D_003B44E0);
    extern u32 g_MsStreamEeDataSizes[MultiStream::Streams] RETAIL(D_003B45A0);
    extern s16 g_MsStreamPitches[MultiStream::Streams] RETAIL(D_002E7948);
    // PriorityKept keeps the value the EE set through the next reply
    extern u16 g_MsStreamPriorities[MultiStream::Streams] RETAIL(D_002E79A8);
    // How long it has played, in 1/60 s
    extern u32 g_MsStreamTime[MultiStream::Streams] RETAIL(D_003B3FC0);
    // The channels' states (ChannelState)
    extern u8 g_MsChannelStates[MultiStream::Channels] RETAIL(D_002E7D60);
    // The sound each channel was last given (MsPlaySound)
    extern u32 g_MsChannelSounds[MultiStream::Channels] RETAIL(D_002E7A38);

    // The module's status (ReplyStatus' fields)
    extern s32 g_MsLoadRequest RETAIL(D_00309EE4);
    extern u32 g_MsDiscBusy RETAIL(D_00309ECC);
    extern u32 g_MsSpuWriteAddress RETAIL(D_00309ED0);
    extern u32 g_MsStereoNotStarted RETAIL(D_00309EC8);
    extern u32 g_MsIopDataAddress RETAIL(D_00309EF4);
    extern u32 g_MsIopDataSize RETAIL(D_0030A8BC);
    extern u32 g_MsIopDataCheck RETAIL(D_00309EF8);
    extern u32 g_MsIopDataSeeks[MultiStream::ChainFiles] RETAIL(D_003B42C0);
    extern u32 g_MsIopDataFiles[MultiStream::ChainFiles] RETAIL(D_003B42E8);
    extern u32 g_MsIopDataPlaySizes[MultiStream::ChainFiles] RETAIL(D_003B4338);
    extern u32 g_MsIopDataSectors[MultiStream::ChainFiles] RETAIL(D_003B4310);
    extern u32 g_MsIopDataChainCount RETAIL(D_0030A8C0);
    extern u32 g_MsIopDataChainPosition RETAIL(D_0030A8C4);
    extern u32 g_MsMaxIopMemory RETAIL(D_00309F3C);
    // The file query's result (MsGetFileInfo)
    extern u32 g_MsFileSource RETAIL(D_00309F2C);
    extern u32 g_MsFileCounter RETAIL(D_00309F30);
    extern u32 g_MsFileSize RETAIL(D_00309F28);
    extern u32 g_MsFileSector RETAIL(D_00309F24);
    extern u32 g_MsIopDataRemaining RETAIL(D_0030A8C8);
    extern u32 g_MsIopDataOffset RETAIL(D_0030A8CC);
    extern u32 g_MsEeWriteAddress RETAIL(D_00309ED8);
    extern u32 g_MsIopWriteAddress RETAIL(D_00309ED4);
    // Set by the commands that set these addresses: the next reply leaves them
    extern u8 g_MsSpuWriteAddressSet RETAIL(D_00309F78);
    extern u8 g_MsEeWriteAddressSet RETAIL(D_00309F79);
    extern u8 g_MsIopWriteAddressSet RETAIL(D_00309F7A);
    // DiscErrors' bits
    extern u8 g_MsDiscError RETAIL(D_00309F20);
    extern u8 g_MsDiscInternalError RETAIL(D_00309F21);
    extern u8 g_MsDiscNotReady RETAIL(D_00309F22);
    extern u32 g_MsIopLoadType RETAIL(D_00309F40);
    extern u32 g_MsDtsStatus RETAIL(D_00309F54);
    extern u32 g_MsEeTransferSize RETAIL(D_00309F4C);
    extern u32 g_MsEeTransferCount RETAIL(D_00309F50);
    extern u32 g_MsPcmIopAddress RETAIL(D_00309F58);
    extern u32 g_MsPcmSize RETAIL(D_00309F5C);
    extern u32 g_MsPcmAddress RETAIL(D_00309F60);
    extern u32 g_MsPcmStatus RETAIL(D_00309F64);
    // The streams' buffers in the IOP's memory
    extern u32 g_MsStreamIopBuffers[MultiStream::Streams] RETAIL(D_003B4360);
    extern u32 g_MsUserTransferStatus RETAIL(D_00309F68);
    extern u32 g_MsChannelNextAddresses[MultiStream::Channels] RETAIL(D_002E7F40);
    extern u32 g_MsDebugScan[2] RETAIL(D_00309EB8);
    extern u32 g_MsDspInfo[MultiStream::DspInfoWords] RETAIL(D_002E7928);
    extern u32 g_MsDspBuffer RETAIL(D_00309EEC);
    extern u32 g_MsDspTransferCount RETAIL(D_00309E88);
    extern MultiStream::DspTransfer g_MsDspTransfers[] RETAIL(D_003B3B80);
    extern u8 g_MsDspStatus RETAIL(D_00309EF0);
    extern u32 g_MsDiscErrorCode RETAIL(D_00309E94);
    // The reply's word g_MsDiscErrorCode came from (SOUND_CD_ERRORCOUNT)
    extern s32 g_MsDiscErrorCodeIndex RETAIL(D_00309E98);
    extern s32 g_MsAtWinMonOpen RETAIL(D_00309F34);
    extern s32 g_MsAtWinMonFile RETAIL(D_00309F38);

    // The thread in MultiStream's calls (SOUND_MTSafe, -1 for none) and the counts of the threads put to sleep waiting for it
    // and woken: the game's MsLock and MsUnlock are empty, so only MsInitialise sets them
    extern s32 g_MsLockThread RETAIL(D_00309FC4);
    extern s8 g_MsSleepCount RETAIL(D_00309FC8);
    extern s8 g_MsWakeCount RETAIL(D_00309FC9);
    // A word MsInitialise clears that nothing reads
    extern s32 g_MsUnusedWord RETAIL(D_00309FCC);
    // The IOP's count of its thread's runs (OpInitWait), written behind the cache's back, and the last one seen
    extern u32 g_MsIopUpdates RETAIL(D_003B4680);
    extern u32 g_MsLastIopUpdate RETAIL(D_00309E90);
    // The DSP effect's count of the IOP's updates (SOUND_DSPUpdate)
    extern u32 g_MsDspUpdates RETAIL(D_003B3E80);
    // The priority of the IOP's thread, which OpInitStreamData hands it (SOUND_THREAD_PRIORITY)
    extern s32 g_MsIopThreadPriority RETAIL(D_00309EC0);
    // OpInitStreamData was sent
    extern s32 g_MsStreamDataInitialised RETAIL(D_0030A8B8);
    // The next file ID MsNextFileId hands out
    extern s32 g_MsFileIdCounter RETAIL(D_00309FA8);
    // The file queries' counter (SOUND_GetInfoCounter)
    extern s8 g_MsFileQueryCounter RETAIL(D_00309FAC);
    // The volume pairs of the groups 1 to 4 (4.12 fixed point)
    extern s16 g_MsGroupVolumesLeft[MultiStream::Groups] RETAIL(D_00309F80);
    extern s16 g_MsGroupVolumesRight[MultiStream::Groups] RETAIL(D_00309F88);
    // Per channel: its group (in the top half, as MultiStream takes it) and its volumes
    extern s32 g_MsChannelGroups[MultiStream::Channels] RETAIL(D_002E7E80);
    extern s16 g_MsChannelVolumesLeft[MultiStream::Channels] RETAIL(D_002E7DC0);
    extern s16 g_MsChannelVolumesRight[MultiStream::Channels] RETAIL(D_002E7E20);

    // The fast load (SOUND_FASTLOAD_INFO): what the IOP's last call said (its stream, the size loaded so far and last, its
    // status (0 loading, 1 done), where it wrote, its count and the file), whether loading goes on (FastLoadStop, Single or
    // Continue), whether the EE's server runs (FastLoadOn), the mode the IOP was given and the loading's state
    struct MsFastLoadInfo
    {
        s32 stream;
        s32 totalSize;
        s32 loadSize;
        s32 status;
        s32 allowLoad;
        s32 eeStatus;
        s32 iopStatus;
        s32 loadStatus;
        u8* eeAddress;
        s32 counter;
        s32 file;
    };
    extern MsFastLoadInfo g_MsFastLoad RETAIL(D_003B4700);
    // The answer to the IOP's call: whether it may load on, where its next transfer goes (0 for where it was going), and the
    // callback's requests (none in the game) of another offset and size of a file to load and of the next file to load
    struct MsFastLoadAnswer
    {
        s32 allowLoad;
        u32 nextEeAddress;
        s32 setsFileOffsetAndSize;
        s32 file;
        s32 offset;
        s32 size;
        s32 loadsNextFile;
    };
    extern MsFastLoadAnswer g_MsFastLoadAnswer RETAIL(D_003B47C0);
    extern SifRpcDataQueue_t g_MsFastLoadQueue RETAIL(D_003B4730);
    extern SifRpcServerData_t g_MsFastLoadServer RETAIL(D_003B4748);
    extern u8 g_MsFastLoadArguments[] RETAIL(D_003B4800);
    // Set while the callback runs; the callback, which nothing sets
    extern s32 g_MsFastLoadInCallback RETAIL(D_00309FA0);
    extern u32 (*g_MsFastLoadCallback)() RETAIL(D_00309F9C);
    // Set by every call of the IOP's (more data reached the EE)
    extern s32 g_MsFastLoadCalled RETAIL(D_00309FA4);
    extern s32 g_MsFastLoadThread RETAIL(G_UnkThreadId);
    extern void* g_MsFastLoadStack RETAIL(D_0030A8DC);

    // The streams the IOP plays (MsSetStreamCount)
    extern s32 g_MsStreamCount RETAIL(D_00309F44);
    // Per stream: its buffer in the IOP's memory (as allocated and as it is now), its SPU2 buffer's size, and whether it's a
    // data stream (MsAllocateStreamBuffer's DataStream)
    extern u32 g_MsIopBufferSizes[MultiStream::Streams] RETAIL(D_002E7AF8);
    extern u32 g_MsIopBufferCurrentSizes[MultiStream::Streams] RETAIL(D_002E7BB8);
    extern u32 g_MsSpuBufferSizes[MultiStream::Streams] RETAIL(D_002E7C78);
    extern u8 g_MsStreamIsData[MultiStream::Streams] RETAIL(D_002E7A08);
    // The last volumes worked out
    extern s32 g_MsLeftVolume RETAIL(D_0030A8D4);
    extern s32 g_MsRightVolume RETAIL(D_0030A8D8);
    // The SPU2 cores' reverb: its mode (0 off to 9, the SPU2's), the size of its work area in the SPU2's memory and where the
    // area ends. The areas end at one of two places, the cores' never overlapping
    extern s32 g_MsEffectModes[2] RETAIL(D_00309F08);
    extern u32 g_MsEffectSizes[2] RETAIL(D_00309F18);
    extern u32 g_MsEffectAreaEnds[2] RETAIL(D_00309F10);
    extern u32 g_MsEffectAreaEnd RETAIL(D_00309F00);
    extern u32 g_MsEffectOtherAreaEnd RETAIL(D_00309F04);
    extern u32 g_MsEffectSizesByMode[10] RETAIL(D_002E7D38);
    // Where sound banks' samples go in the sound processor's memory
    extern u32 g_MsSoundBankAddress RETAIL(D_00309E78);
    // MsHandleDiscErrors' last errors
    extern u32 g_MsLastDiscError RETAIL(D_00309FE4);
    extern u32 g_MsLastDiscInternalError RETAIL(D_00309FE0);
    // A table of the sound memory's blocks MsCheckSoundId gives their sounds' IDs, which nothing makes
    struct MsMemoryBlock
    {
        u32 address;
        u32 size;
        u32 next;
        u32 sound;
        u8 hasSound;
    };
    extern MsMemoryBlock* g_MsMemoryBlocks RETAIL(D_00309FE8);
    extern u16 g_MsMemoryBlockCount RETAIL(D_0030A8EC);

    // SOUND_MultiThreadSafeCheckStart and End, empty in the game
    void MsLock() RETAIL(UnknownDebugFunction_);
    void MsUnlock() RETAIL(UnkDebugFunction2);
    // Moves the staged command into the batch, sending the batch first when it's full
    void MsCommit() RETAIL(FUN_001e1b40);
    // Sends the batch with the status request appended, by a SendMode (-1 when it gave up). Returns the words sent
    s32 MsSend(u32 mode) RETAIL(FUN_001e30e8);
    s32 MsSendBatch(u32 mode) RETAIL(FUN_001e7860);
    s32 MsIsBusy() RETAIL(GetData_0x309EA0);
    s32 MsInitialise() RETAIL(FUN_001e1978);
    // The thread of the EE's own server, at the priority, on the stack
    s32 MsInitFastLoad(s32 priority, void* stack, s32 stackSize) RETAIL(FUN_001e8780);

    s32 MsGroupScale(s32 groupVolume, s32 volume) RETAIL(FUN_001e8620);
    void MsComputeChannelVolumes(s32 group, s32 channel) RETAIL(FUN_001e84e0);
    void MsSetChannelGroup(s32 channel, s32 group) RETAIL(FUN_001e84c8);
    void MsStoreChannelVolumes(s32 channel, s16 left, s16 right) RETAIL(FUN_001e8560);
    s32 MsSetGroupVolume(s32 group, u32 left, u32 right) RETAIL(FUN_001e8448);
    void MsApplyGroupVolume(s32 group) RETAIL(FUN_001e3e18);
    s32 MsSetChannelVolume(s16 channel, s16 left, s16 right) RETAIL(FUN_001e4610);
    s32 MsNextFileId() RETAIL(Increment_0x309FA8);
    s32 MsPitchOfRate(s32 rate) RETAIL(FUN_001e7c10);
    // The stream playing on the channel, -1 for none, -2 past the channels
    s32 MsStreamOnChannel(u32 channel) RETAIL(FUN_001e7f18);

    s32 MsSetChannelPitch(u32 channel, u16 pitch) RETAIL(FUN_001e72e0);
    s32 MsKeyOff(u32 channel) RETAIL(FUN_001e74c0);
    // From ChannelStreams up, the stream playing on the channel ChannelStreams below
    s32 MsStopStream(s32 stream) RETAIL(FUN_001e73b8);
    void MsResetSound() RETAIL(FUN_001e75f0);
    void MsPauseAll() RETAIL(FUN_001e7560);
    void MsResumeAll() RETAIL(FUN_001e75a8);
    // The module's loading (a LoadType) and the files and sounds it keeps room for
    void MsInitStreamData(u16 loadType, u16 maxFiles, u16 maxSounds) RETAIL(FUN_001e2310);
    // The stream's buffer of size bytes in the IOP's memory, an audio stream's SPU2 buffer at spuAddress (DataStream for a data
    // stream, which has none)
    s32 MsAllocateStreamBuffer(s32 stream, u32 spuAddress, u32 size) RETAIL(FUN_001e2438);
    // Where a load into the SPU2's memory (LoadToSpu) puts its bytes
    void MsSetSpuWriteAddress(u32 address) RETAIL(FUN_001e77b8);
    s32 MsCloseStreamBuffer(u32 stream) RETAIL(FUN_001e7638);
    s32 MsSetStreamCount(u32 count) RETAIL(FUN_001e7718);
    // The core's reverb: the SPU2's mode (below 10), depth, delay and feedback
    s32 MsSetEffect(u32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback) RETAIL(FUN_001e32d0);
    void MsClearEffect(s32 core) RETAIL(FUN_001e7880);
    void MsSetEffectVolume(u16 core, u16 left, u16 right) RETAIL(FUN_001e7920);
    void MsEffectSendOn(u16 channel) RETAIL(FUN_001e7a20);
    void MsEffectSendOff(u16 channel) RETAIL(FUN_001e7aa8);
    // The sound bank's samples are at the address of the sound processor's memory
    s32 MsSetBankAddress(u32 bank, u32 address) RETAIL(FUN_001e7b40);
    void MsCloseFile(u32 file) RETAIL(FUN_001e7c38);
    // The module's table of sounds: room for so many (SOUND_AllocateSpotFXMemory)
    void MsReserveSounds(u16 count) RETAIL(FUN_001e7cd0);
    void MsFreeSound(u32 sound) RETAIL(FUN_001e7d68);
    // What the next transfer of the file reads
    void MsReadFile(u32 file, u32 offset, u32 size) RETAIL(FUN_001e3670);
    // The drive's medium and spinning (DiscType)
    void MsInitDisc(u16 type) RETAIL(FUN_001e7df8);
    // The stream reads an interleaved file whose blocks are trackSize bytes, playing the track's (SOUND_SetStreamParent_Int)
    void MsInterleaveStream(s32 stream, u16 track, u32 trackSize) RETAIL(FUN_001e3760);
    // The next reply's status has the largest IOP memory free (the memory left for the streams' buffers)
    void MsQueryFreeMemory() RETAIL(FUN_001e7f48);
    // The stream keys its channel on once its buffers are full, or waits for MsAllowKeyOn
    s32 MsAllowKeyOn(s32 stream) RETAIL(FUN_001e8080);
    s32 MsDisableKeyOn(s32 stream) RETAIL(FUN_001e8120);
    // Where a load into the EE's or the IOP's memory (LoadToEe, LoadToIop) puts its bytes
    void MsSetEeWriteAddress(u32 address) RETAIL(FUN_001e8228);
    void MsSetIopWriteAddress(u32 address) RETAIL(FUN_001e82c0);
    // After a disc error: the IOP asked to check the disc again (the reply's DiscErrors), and to start over once it's ready
    void MsRestartFromDiscError() RETAIL(FUN_001e8358);
    void MsCheckDiscError() RETAIL(FUN_001e83a0);
    void MsDisableSpuCallback() RETAIL(FUN_001e7e88);
    void MsEnableSpuCallback() RETAIL(FUN_001e7ed0);
    // OpInitWait without an address: the IOP stops counting its runs
    void MsCloseWaitUpdate() RETAIL(FUN_001e8678);
    // FastLoadOff, FastLoadOn and the cache's modes go to the IOP (the EE's server has to run), FastLoadStop, Single and Continue
    // are the EE's own
    s32 MsSetFastLoadMode(u32 mode) RETAIL(FUN_001e44f0);
    // Of the stream's SPU2 buffer, size bytes are used (SOUND_ResizeSPUBuffer)
    s32 MsResizeSpuBuffer(s32 stream, u32 size) RETAIL(FUN_001e25a0);
    // The channel's release rate (below 0x20)
    s32 MsSetChannelRelease(u32 channel, u32 rate) RETAIL(FUN_001e47c8);
    // Where the interleaved stream's data ends in its last block (it's looked for otherwise)
    s32 MsSetMibEndOffset(s32 stream, u32 offset) RETAIL(FUN_001e7f90);
    void MsSoundBankLoaded(u16 bank) RETAIL(FUN_001e8a08);
    void MsCompactSoundMemory() RETAIL(FUN_001e8a70);

    // Gives the sound memory's block at the address the sound (SOUND_MemCheckForSFXID): 0, -1 for no such block, -2 without the
    // table
    s32 MsCheckSoundId(u32 sound, u32 address) RETAIL(FUN_001e8968);

    // The stream selectors of MsPlayStream (STREAM_FIND_FREE*): the first free stream, the first free data stream, the first
    // free audio stream
    constexpr s32 MsAnyStream = 0x80;
    constexpr s32 MsAnyDataStream = 0x81;
    constexpr s32 MsAnyAudioStream = 0x82;
    // Plays a file on a stream (a selector or the stream) and a channel (its group in the top half), loading it by the mode (a
    // StreamMode), at the pitch and the envelope's attack and release rates. Audio sets the channel's volumes. Returns the
    // stream, -1 when there's none. Looking for a data or an audio stream never returns when no stream is free
    s32 MsPlayStream(u32 file, s32 stream, u32 channelAndGroup, s32 left, s32 right, u16 pitch, u32 mode, u32 attack,
                     u32 release) RETAIL(FUN_001e1c90);
    // Plays a sound of a bank on a channel so many times (0 until it's keyed off: SOUND_PlaySFXLoop)
    s32 MsPlaySound(u32 sound, u32 channelAndGroup, s32 left, s32 right, u16 pitch, u32 attack, u32 release, u32 loops)
        RETAIL(FUN_001e2078);
    // Opens a file with an ID (MsNextFileId's when -1), reading from offset on. Returns the ID
    s32 MsOpenFile(s32 file, const char* path, u32 offset) RETAIL(FUN_001e3540);
    // The stream plays the parent's next block of its interleaved file on the channel, from the SPU2's memory at spuAddress
    // (SOUND_SetStreamChild_Int)
    void MsAddStreamChannel(s32 stream, u32 parent, u32 channelAndGroup, u16 track, s32 left, s32 right, u32 spuAddress)
        RETAIL(FUN_001e3860);
    // The first stream that's off, -1 when none is
    s32 MsFirstFreeStream() RETAIL(FUN_001e7230);
    // Loads what MsReadFile set up for the file on the stream into the memory of the mode (a LoadTo StreamMode), at the address
    // (where the last load ended when 0: SOUND_LoadFile). Returns the stream, -2 for a mode that isn't a load
    s32 MsLoadFile(u32 mode, u8 stream, u32 file, u32 address) RETAIL(FUN_001e7138);
    s32 MsFreeStreamIn(s32 first, s32 last) RETAIL(FUN_001e7280);
}

namespace MultiStream
{
// What MsGetStreamStatus hands out (SOUND_STREAM_INFO): the module's state, and the stream's (its state is StreamOff,
// StreamOn or StreamWaitingToPlay)
struct StreamStatus
{
    u32 maxIopMemory;
    u8 discError;
    u8 discNotReady;
    u8 discInternalError;
    u8 discAccess;
    u8 discBusy;
    u8 discAccessStream;
    u32 writeAddress;
    u8 type;
    u8 state;
    u8 active;
    u8 channel;
    u8 playHalf;
    s16 envelope;
    u32 file;
    u32 spuAddress;
    s32 pitch;
    u32 destinationAddress;
    u32 eeDataSize;
    u32 eeTransferSize;
    u32 eeTransferCount;
    u32 playOffset;
    u32 iopBuffer;
    u32 userTransferStatus;
    u16 priority;
    // How long it has played
    u8 frames;
    u8 seconds;
    u8 minutes;
    u32 hours;
};
static_assert(sizeof(StreamStatus) == 0x4C);

// What a file query gives (SOUND_FILE_INFO): the file's sector, size, source and the query's counter, and the ATWinMon
// debugger's file
struct FileInfo
{
    u32 sector;
    u32 size;
    u32 source;
    u32 counter;
    s32 atWinMonOpen;
    s32 atWinMonFile;
};
static_assert(sizeof(FileInfo) == 0x18);

// MsInitDisc's types (SOUND_CD, SOUND_DVD, SOUND_SET_CALLBACK, SOUND_CD_SPIN_*): the medium, only the callback set up (the drive
// was set up already), and the drive spinning the normal way or the streaming way (at a steady speed)
enum DiscType : u16
{
    DiscCd = 1,
    DiscDvd = 2,
    DiscCallbackOnly = 3,
    DiscSpinNormal = 4,
    DiscSpinStream = 5,
};

// MsInitStreamData's loading (SND_LOAD_*): the module's own, or the EE's, which the module hands what to load
enum LoadType : u16
{
    LoadInternal = 0,
    LoadExternal = 1,
};

// MsWaitForStream's wait: setting the IOP's count of its updates up and closing it (cold), or leaving that to the caller (hot)
enum WaitKind : s32
{
    WaitCold = 0,
    WaitHot = 1,
};

// MsWaitForStream's stream word: the stream, and whether only its load requests are let through (SOUND_CHAN_LOAD_ONLY)
union WaitStream
{
    u32 value;
    struct
    {
        u32 stream : 8;
        u32 onlyThisStream : 8;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(WaitStream, 4);

// MsAllocateStreamBuffer's address of a data stream (SOUND_DATA_STREAM)
constexpr u32 DataStream = 1;
// MsStopStream's streams from it are the channels' (SPU_CH)
constexpr s32 ChannelStreams = 0x40;
// The fast load's call before the IOP writes the EE's memory (SOUND_FASTLOAD_INVALID_DCACHE's stream)
constexpr s32 FastLoadInvalidateCache = 999;
}

extern "C"
{
    // -1 for a stream past the count
    s32 MsGetStreamStatus(s32 stream, MultiStream::StreamStatus* status) RETAIL(FUN_001e3a98);
    // Handles the disc's error the IOP reports (SOUND_HandleCDErrors): asks the IOP to check the disc, and to start over once
    // it's ready. Returns -1 while there's an error, -2 while MultiStream isn't back from one, else the disc's error code
    s32 MsHandleDiscErrors() RETAIL(FUN_001e3d10);
    // Sends the batch and waits until the stream (WaitStream) stops, handling the IOP's load requests on the way
    // (SOUND_WaitForFileToLoad). Returns the stream asking for a load + 1 when the load isn't MultiStream's own, else 0 once
    // the stream stopped
    s32 MsWaitForStream(u32 waitStream, s32 kind) RETAIL(FUN_001e3f58);
    // The file query's result, and its counter
    u32 MsGetFileInfo(MultiStream::FileInfo* info) RETAIL(FUN_001e81f0);
    // The stream that wants more data, -1 for none
    s32 MsGetLoadRequest() RETAIL(FUN_001e8910);
    // 1 when free, 0 when not, -1 past the channels
    s32 MsIsChannelFree(u32 channel) RETAIL(FUN_001e88b8);
    // The IOP buffer an interleaved file's stream needs: two blocks of every track (SOUND_ReturnMIBBufferSize)
    s32 MsInterleavedBufferSize(s32 trackSize, s32 files) RETAIL(FUN_001e89e0);
}

// Staging a command's words
namespace MultiStream
{
inline void Begin(s32 opcode)
{
    if (g_MsCommand == NoCommand)
    {
        g_MsCommandLength = 0;
        g_MsCommand = opcode;
    }
}

inline void Push(u16 word)
{
    if (g_MsBatchFailed != 1)
    {
        g_MsCommandWords[g_MsCommandLength++] = word;
    }
}

inline void Push32(u32 value)
{
    Push(static_cast<u16>(value >> 16));
    Push(static_cast<u16>(value));
}
}
