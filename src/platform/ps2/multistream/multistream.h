#pragma once

#include "common.h"

#include <sifrpc.h>

// SCEE's MultiStream, the EE side of the IOP's sound and streaming module (the disc's STREAM.IRX, "multi_streamer"). The EE
// batches commands for the IOP, sends them once a frame (or when the batch is full) to the module's RPC server 0x12345 and gets
// the module's status back in the reply, which it keeps for the queries. The module calls the EE back through a server of the
// EE's own (0x12344321). A command is an opcode and 16 bit words, staged one at a time and then committed to the batch
namespace MultiStream
{
// The stream channels the IOP reports on (the game uses 9)
constexpr u32 Streams = 48;
// The batch: a header of 2 words (the magic 0x48 and the count), then per command its opcode, its length and its words
constexpr u32 BatchWords = 0x400;
constexpr u32 TransferSize = 0x800;
constexpr s32 NoCommand = -1;

// The status the IOP sends back: a record of 9 words per stream that changed, then the module's state
struct StreamRecord
{
    // Bits 0-1 an event, 2-3 ..., 4-9 the stream, 10-15 its slot, 16-31 ...
    u32 header;
    u32 words[8];
};
static_assert(sizeof(StreamRecord) == 36);
}

extern "C"
{
    // The command being staged: its opcode (NoCommand when none) and its words
    extern s32 g_MsCommand RETAIL(D_00309F94);
    extern s32 g_MsCommandLength RETAIL(UnkGlobalCounter);
    extern u16 g_MsCommandWords[0x40C] RETAIL(D_003B2B68);
    // A full batch couldn't be sent: the words of the commands staged until the next one goes are dropped
    extern s16 g_MsBatchFailed RETAIL(D_00309F70);
    // Commit is sending a full batch, which gets no frame end
    extern s8 g_MsFlushing RETAIL(D_00309F48);
    // The batch being filled
    extern u16 g_MsBatch[MultiStream::BatchWords] RETAIL(D_003B1B40);
    // The batch's words after its header, and its commands
    extern s32 g_MsBatchLength RETAIL(D_00309E84);
    extern s32 g_MsBatchCommands RETAIL(D_00309F98);
    // A call is out: its reply clears it (in the interrupt that ends the call)
    extern volatile s32 g_MsBusy RETAIL(D_00309EA0);
    // 1 when a batch is to be sent, 2 while it is, 0 once its reply came
    extern s32 g_MsSyncState RETAIL(D_00309FD4);
    extern s32 D_00309FD8;
    extern SifRpcClientData_t g_MsClient RETAIL(D_003B2B40);
    extern u16 g_MsSendBuffer[MultiStream::TransferSize / sizeof(u16)] RETAIL(G_DataBuffer_UnkRPC_Call);
    extern u32 g_MsReply[MultiStream::TransferSize / sizeof(u32)] RETAIL(G_ReceiveBuffer_UnkRPC_Call);

    // The status of the last reply. The IOP counts its status, a reply with the same count is old
    extern u32 g_MsTick RETAIL(D_00309EE8);
    extern u32 g_MsLastTick RETAIL(D_00309F74);
    // The last stream event (bits 0-1 of a record) and its stream
    extern u32 g_MsLastEvent RETAIL(D_00309EDC);
    extern u32 g_MsLastEventStream RETAIL(D_00309EE0);

    // The streams' states on the EE side: 0 idle, 1 reported by the IOP, 2 stop asked for, 3 started, 4 started and reported,
    // 5 stopped (for 3 replies, g_MsStreamCountdown)
    extern s8 g_MsStreamStates[MultiStream::Streams] RETAIL(D_003B4080);
    extern u8 g_MsStreamCountdown[MultiStream::Streams] RETAIL(D_003B40B0);
    // The stream playing on each slot, 0xFF for none
    extern u8 g_MsSlotStreams[MultiStream::Streams] RETAIL(D_002E7D90);
    // The records' fields, by stream (the status queries hand them out)
    extern u8 g_MsStreamRecordBits[MultiStream::Streams] RETAIL(D_003B4170);
    extern s16 g_MsStreamRecordHigh[MultiStream::Streams] RETAIL(D_003B41A0);
    extern u32 g_MsStreamWord1[MultiStream::Streams] RETAIL(D_003B4200);
    extern u32 g_MsStreamWord2[MultiStream::Streams] RETAIL(D_003B3F00);
    extern u8 g_MsStreamWord3Top[MultiStream::Streams] RETAIL(D_003B4140);
    extern u8 g_MsStreamWord3Low[MultiStream::Streams] RETAIL(D_003B40E0);
    extern u8 g_MsStreamSlots[MultiStream::Streams] RETAIL(D_003B4110);
    extern u32 g_MsStreamWord4[MultiStream::Streams] RETAIL(D_003B4420);
    extern u32 g_MsStreamWord5[MultiStream::Streams] RETAIL(D_003B44E0);
    extern u32 g_MsStreamWord6[MultiStream::Streams] RETAIL(D_003B45A0);
    extern s16 g_MsStreamWord7Low[MultiStream::Streams] RETAIL(D_002E7948);
    // Bit 15 keeps the value the EE set through the next reply
    extern u16 g_MsStreamWord7High[MultiStream::Streams] RETAIL(D_002E79A8);
    // How long it has played, in 1/60 s
    extern u32 g_MsStreamTime[MultiStream::Streams] RETAIL(D_003B3FC0);
    // The channels' states, 2 bits each in the reply: 0 free, 3 then 4 on their way to free
    extern u8 g_MsChannelStates[MultiStream::Streams] RETAIL(D_002E7D60);
    // The sound each channel was last given (MsPlaySound)
    extern u32 g_MsChannelSounds[MultiStream::Streams] RETAIL(D_002E7A38);

    // The module's status after the records, named by their words' places after them when their meaning is unknown
    extern u32 g_MsStatus2 RETAIL(D_00309EE4);
    extern u32 g_MsStatus3 RETAIL(D_00309ECC);
    extern u32 g_MsStatus4 RETAIL(D_00309ED0);
    extern u32 g_MsStatus5 RETAIL(D_00309EC8);
    extern u32 g_MsStatus9 RETAIL(D_00309EF4);
    extern u32 g_MsStatus10 RETAIL(D_0030A8BC);
    extern u32 g_MsStatus11 RETAIL(D_00309EF8);
    extern u32 g_MsStatusTable12[9] RETAIL(D_003B42C0);
    extern u32 g_MsStatusTable21[9] RETAIL(D_003B42E8);
    extern u32 g_MsStatusTable30[9] RETAIL(D_003B4338);
    extern u32 g_MsStatusTable39[9] RETAIL(D_003B4310);
    extern u32 g_MsStatus48 RETAIL(D_0030A8C0);
    extern u32 g_MsStatus49 RETAIL(D_0030A8C4);
    extern u32 g_MsStatus50 RETAIL(D_00309F3C);
    // A file query's result (words 51 to 53 and the last two of the reply)
    extern u32 g_MsFileInfo51Low RETAIL(D_00309F2C);
    extern u32 g_MsFileInfo51High RETAIL(D_00309F30);
    extern u32 g_MsFileInfo52 RETAIL(D_00309F28);
    extern u32 g_MsFileInfo53 RETAIL(D_00309F24);
    extern u32 g_MsStatus54 RETAIL(D_0030A8C8);
    extern u32 g_MsStatus55 RETAIL(D_0030A8CC);
    extern u32 g_MsStatus56 RETAIL(D_00309ED8);
    extern u32 g_MsStatus57 RETAIL(D_00309ED4);
    // Set by the commands that set these values: the next reply leaves them
    extern u8 g_MsHoldStatus4 RETAIL(D_00309F78);
    extern u8 g_MsHoldStatus56 RETAIL(D_00309F79);
    extern u8 g_MsHoldStatus57 RETAIL(D_00309F7A);
    // Word 58's bits: 2 is the disc's error (the tray opened)
    extern u8 g_MsFlag0 RETAIL(D_00309F20);
    extern u8 g_MsFlag1 RETAIL(D_00309F21);
    extern u8 g_MsDiscError RETAIL(D_00309F22);
    extern u32 g_MsStatus59 RETAIL(D_00309F40);
    extern u32 g_MsStatus60 RETAIL(D_00309F54);
    extern u32 g_MsStatus61 RETAIL(D_00309F4C);
    extern u32 g_MsStatus62 RETAIL(D_00309F50);
    extern u32 g_MsStatus63 RETAIL(D_00309F58);
    extern u32 g_MsStatus64 RETAIL(D_00309F5C);
    extern u32 g_MsStatus65 RETAIL(D_00309F60);
    extern u32 g_MsStatus66 RETAIL(D_00309F64);
    // A list of the reply's own length (word 67 counts it)
    extern u32 g_MsStreamValues[MultiStream::Streams] RETAIL(D_003B4360);
    extern u32 g_MsStatusAfterValues RETAIL(D_00309F68);
    extern u32 g_MsChannelValues[MultiStream::Streams] RETAIL(D_002E7F40);
    extern u32 g_MsStatusB RETAIL(D_00309EB8);
    extern u32 g_MsStatusC RETAIL(D_00309EBC);
    extern u32 g_MsStatusTableD[8] RETAIL(D_002E7928);
    extern u32 g_MsStatusE RETAIL(D_00309EEC);
    // Another list, of 3 words per entry
    extern u32 g_MsEntryCount RETAIL(D_00309E88);
    extern u32 g_MsEntries[] RETAIL(D_003B3B80);
    extern u8 g_MsStatusF RETAIL(D_00309EF0);
    extern u32 g_MsStatusG RETAIL(D_00309E94);
    // The word of the reply g_MsStatusG came from
    extern s32 g_MsStatusGIndex RETAIL(D_00309E98);
    extern u32 g_MsFileInfoLast1 RETAIL(D_00309F34);
    extern u32 g_MsFileInfoLast2 RETAIL(D_00309F38);

    // MsInitialise's
    extern s32 D_00309FC4;
    extern u32 D_003B4680;
    extern u32 D_003B3E80;
    // The volume of the command 0xC
    extern s32 g_MsVolume RETAIL(D_00309EC0);
    extern s8 D_00309FC8;
    extern s32 D_00309FCC;
    extern s8 D_00309FC9;
    extern s32 D_0030A8B8;
    // A counter of requests (Increment_0x309FA8)
    extern s32 g_MsRequestCounter RETAIL(D_00309FA8);
    extern s8 D_00309FAC;
    // The volume pairs of the groups 1 to 4 (4.12 fixed point)
    extern s16 g_MsGroupVolumesLeft[4] RETAIL(D_00309F80);
    extern s16 g_MsGroupVolumesRight[4] RETAIL(D_00309F88);
    // Per channel: its group (ID << 16) and its volumes
    extern s32 g_MsChannelGroups[MultiStream::Streams] RETAIL(D_002E7E80);
    extern s16 g_MsChannelVolumesLeft[MultiStream::Streams] RETAIL(D_002E7DC0);
    extern s16 g_MsChannelVolumesRight[MultiStream::Streams] RETAIL(D_002E7E20);

    // The EE's server: the IOP's request, and what's answered
    struct MsServerState
    {
        s32 request;
        s32 argument1;
        s32 size;
        s32 argument3;
        // 2 or 4 from the callback
        s32 mode;
        s32 running;
        s32 unknown18;
        s32 unknown1C;
        u8* address;
        s32 argument5;
        s32 argument6;
    };
    extern MsServerState g_MsServer RETAIL(D_003B4700);
    struct MsServerAnswer
    {
        s32 unavailable;
        u32 value;
        s32 unknown08;
        s32 unknown0C;
        s32 unknown10;
        s32 unknown14;
        s32 unknown18;
    };
    extern MsServerAnswer g_MsServerAnswer RETAIL(D_003B47C0);
    extern SifRpcDataQueue_t g_MsServerQueue RETAIL(D_003B4730);
    extern SifRpcServerData_t g_MsServerData RETAIL(D_003B4748);
    extern u8 g_MsServerBuffer[] RETAIL(D_003B4800);
    // Set while the callback runs; the callback, which nothing sets
    extern s32 g_MsServerCallbackRunning RETAIL(D_00309FA0);
    extern u32 (*g_MsServerCallback)() RETAIL(D_00309F9C);
    extern s32 g_MsServerRequested RETAIL(D_00309FA4);
    extern s32 g_MsServerThread RETAIL(G_UnkThreadId);
    extern void* g_MsServerStack RETAIL(D_0030A8DC);

    // The streams the IOP plays (MsSetStreamCount)
    extern s32 g_MsStreamCount RETAIL(D_00309F44);
    // Per stream: its buffer on the IOP (allocated, capacity and in use) and whether it's the EE's memory
    extern u32 g_MsStreamBufferSizes[MultiStream::Streams] RETAIL(D_002E7AF8);
    extern u32 g_MsStreamBufferCapacities[MultiStream::Streams] RETAIL(D_002E7BB8);
    extern u32 g_MsStreamBufferUsed[MultiStream::Streams] RETAIL(D_002E7C78);
    extern u8 g_MsStreamBufferInEe[MultiStream::Streams] RETAIL(D_002E7A08);
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
    // The last change of the word the IOP moves on as it works (command 0x49 gives it its address)
    extern u32 g_MsProgress RETAIL(D_003B4680);
    extern u32 g_MsLastProgress RETAIL(D_00309E90);
    // MsCheckDisc's last flags
    extern u32 g_MsCheckedFlag0 RETAIL(D_00309FE4);
    extern u32 g_MsCheckedFlag1 RETAIL(D_00309FE0);
    // A table MsFindBank looks keys up in, which nothing sets
    struct MsEntry
    {
        u32 key;
        u32 unknown04;
        u32 unknown08;
        u32 value;
        u8 set;
    };
    extern MsEntry* g_MsEntryTable RETAIL(D_00309FE8);
    extern u16 g_MsEntryTableCount RETAIL(D_0030A8EC);

    void MsLock() RETAIL(UnknownDebugFunction_);
    void MsUnlock() RETAIL(UnkDebugFunction2);
    // Moves the staged command into the batch, sending the batch first when it's full
    void MsCommit() RETAIL(FUN_001e1b40);
    // Sends the batch with the frame's end appended: mode 0 waits for the call before and after, 1 gives up (-1) while one is
    // out. Returns the words sent
    s32 MsSend(u32 mode) RETAIL(FUN_001e30e8);
    s32 MsSendBatch(u32 mode) RETAIL(FUN_001e7860);
    s32 MsIsBusy() RETAIL(GetData_0x309EA0);
    s32 MsInitialise() RETAIL(FUN_001e1978);
    // The thread of the EE's own server, at the priority, on the stack
    s32 MsStartServer(s32 priority, void* stack, s32 stackSize) RETAIL(FUN_001e8780);

    s32 MsGroupScale(s32 groupVolume, s32 volume) RETAIL(FUN_001e8620);
    void MsComputeChannelVolumes(s32 group, s32 channel) RETAIL(FUN_001e84e0);
    void MsSetChannelGroup(s32 channel, s32 group) RETAIL(FUN_001e84c8);
    void MsStoreChannelVolumes(s32 channel, s16 left, s16 right) RETAIL(FUN_001e8560);
    s32 MsSetGroupVolume(s32 group, u32 left, u32 right) RETAIL(FUN_001e8448);
    void MsApplyGroupVolume(s32 group) RETAIL(FUN_001e3e18);
    s32 MsSetChannelVolume(s16 channel, s16 left, s16 right) RETAIL(FUN_001e4610);
    s32 MsNextRequest() RETAIL(Increment_0x309FA8);
    s32 MsPitchOfRate(s32 rate) RETAIL(FUN_001e7c10);
    s32 MsStreamOnSlot(u32 slot) RETAIL(FUN_001e7f18);

    s32 MsSetVoicePitch(u32 channel, u16 value) RETAIL(FUN_001e72e0);
    s32 MsKeyOff(u32 channel) RETAIL(FUN_001e74c0);
    // Streams from 64 up are slots
    s32 MsStopStream(s32 stream) RETAIL(FUN_001e73b8);
    void MsResetSound() RETAIL(FUN_001e75f0);
    void MsPauseAll() RETAIL(FUN_001e7560);
    void MsResumeAll() RETAIL(FUN_001e75a8);
    void MsConfigure(u16 first, u16 second, u16 third) RETAIL(FUN_001e2310);
    // A location of 1 is the EE's memory
    s32 MsSetStreamBuffer(s32 stream, u32 location, u32 size) RETAIL(FUN_001e2438);
    // Where a read into the sound processor's memory (MsTransfer's 0x7F) puts its bytes
    void MsSetSoundDestination(u32 address) RETAIL(FUN_001e77b8);
    s32 MsReleaseStreamBuffer(u32 stream) RETAIL(FUN_001e7638);
    s32 MsSetStreamCount(u32 count) RETAIL(FUN_001e7718);
    // The core's reverb: the SPU2's mode (below 10), depth, delay and feedback
    s32 MsSetEffect(u32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback) RETAIL(FUN_001e32d0);
    void MsClearEffect(s32 core) RETAIL(FUN_001e7880);
    void MsSetEffectVolume(u16 first, u16 second, u16 third) RETAIL(FUN_001e7920);
    void MsEffectSendOn(u16 value) RETAIL(FUN_001e7a20);
    void MsEffectSendOff(u16 value) RETAIL(FUN_001e7aa8);
    // The sound bank's samples are at the address of the sound processor's memory
    s32 MsSetBankAddress(u32 bank, u32 address) RETAIL(FUN_001e7b40);
    void MsCloseFile(u32 file) RETAIL(FUN_001e7c38);
    void MsReserveSounds(u16 value) RETAIL(FUN_001e7cd0);
    void MsFreeSound(u32 value) RETAIL(FUN_001e7d68);
    // What the next transfer of the file reads
    void MsReadFile(u32 file, u32 offset, u32 size) RETAIL(FUN_001e3670);
    void MsCommand2A(u16 value) RETAIL(FUN_001e7df8);
    void MsInterleaveStream(s32 stream, u16 value, u32 second) RETAIL(FUN_001e3760);
    // The next reply's status word 50 is the memory left for the streams' buffers
    void MsQueryFreeMemory() RETAIL(FUN_001e7f48);
    s32 MsStartPrepared(s32 stream) RETAIL(FUN_001e8080);
    s32 MsPrepareStream(s32 stream) RETAIL(FUN_001e8120);
    // Where a read into memory (MsTransfer's 0x7E) puts its bytes
    void MsSetEeDestination(u32 address) RETAIL(FUN_001e8228);
    void MsSetStatus57(u32 value) RETAIL(FUN_001e82c0);
    // What the EE sends while the disc can't be read and once it can again (named after when they're sent)
    void MsRestartReading() RETAIL(FUN_001e8358);
    void MsHoldReading() RETAIL(FUN_001e83a0);
    void MsLendTransfer() RETAIL(FUN_001e7e88);
    void MsReclaimTransfer() RETAIL(FUN_001e7ed0);
    // Command 0x49 without an address: the IOP stops moving the progress word on
    void MsStopProgress() RETAIL(FUN_001e8678);
    // Modes 0, 1, 6 and 7 go to the IOP (the EE's server has to run), 2 to 4 are the EE's own
    s32 MsSetServerMode(u32 mode) RETAIL(FUN_001e44f0);
    s32 MsSetStreamBufferSize(s32 stream, u32 size) RETAIL(FUN_001e25a0);
    // A voice's parameter, the voice as the SPU2's core and voice
    s32 MsSetVoiceRelease(u32 voice, u32 parameter) RETAIL(FUN_001e47c8);
    s32 MsCommand63(s32 stream, u32 value) RETAIL(FUN_001e7f90);
    void MsCommand103(u16 value) RETAIL(FUN_001e8a08);
    void MsCommand104() RETAIL(FUN_001e8a70);

    s32 MsFindBank(u32 value, u32 key) RETAIL(FUN_001e8968);

    // The stream selectors of MsPlayStream: the first free stream, of the EE's memory or of the IOP's
    constexpr s32 MsAnyStream = 0x80;
    constexpr s32 MsAnyEeStream = 0x81;
    constexpr s32 MsAnyIopStream = 0x82;
    // Plays a file on a stream (a selector or the stream) and a channel (its group in the top half). Modes below 2 set the
    // channel's volumes. Returns the stream, -1 when there's none. Looking for a stream of the EE's or the IOP's memory never
    // returns when no stream is free
    s32 MsPlayStream(u32 file, s32 stream, u32 channelAndGroup, s32 left, s32 right, u16 word, u32 mode, u32 lowByte,
                     u32 highByte) RETAIL(FUN_001e1c90);
    // Plays a sound (0x39) on a channel
    s32 MsPlaySound(u32 sound, u32 channelAndGroup, s32 left, s32 right, u16 word, u32 lowByte, u32 highByte, u32 last)
        RETAIL(FUN_001e2078);
    // Opens a file for a request (MsNextRequest's when -1). Returns the request
    s32 MsOpenFile(s32 request, const char* path, u32 value) RETAIL(FUN_001e3540);
    void MsAddStreamChannel(s32 stream, u32 slot, u32 channelAndGroup, u16 word, s32 left, s32 right, u32 value)
        RETAIL(FUN_001e3860);
    // The first stream that's idle, -1 when none is
    s32 MsFirstFreeStream() RETAIL(FUN_001e7230);
    // The modes of MsTransfer: a read into memory, into the sound processor's memory, ...
    constexpr u32 MsTransferToEe = 0x7E;
    constexpr u32 MsTransferToSound = 0x7F;
    constexpr u32 MsTransfer7D = 0x7D;
    constexpr u32 MsTransfer7B = 0x7B;
    // Transfers what MsReadFile set up for the file on the stream, to the destination of the mode. Returns the stream, -2 for
    // a mode that isn't one of these
    s32 MsTransfer(u32 mode, u8 stream, u32 file, u32 destination) RETAIL(FUN_001e7138);
    s32 MsFreeStreamIn(s32 first, s32 last) RETAIL(FUN_001e7280);
}

namespace MultiStream
{
// What MsGetStreamStatus hands out: the module's state, and the stream's
struct StreamStatus
{
    u32 status50;
    u8 flag0;
    u8 discError;
    u8 flag1;
    u8 lastEvent;
    u8 status3;
    u8 lastEventStream;
    u32 word4;
    u8 word3Low;
    // 0 idle, 1 playing, 6 stopping
    u8 state;
    u8 word3Top;
    u8 slot;
    u8 recordBits;
    s16 recordHigh;
    u32 word2;
    u32 word1;
    s32 word7Low;
    u32 word1Again;
    u32 word6;
    u32 status61;
    u32 status62;
    u32 word5;
    u32 value;
    u32 statusAfterValues;
    u16 word7High;
    // How long it has played
    u8 frames;
    u8 seconds;
    u8 minutes;
    u32 hours;
};
static_assert(sizeof(StreamStatus) == 0x4C);
}

extern "C"
{
    // -1 for a stream past the count
    s32 MsGetStreamStatus(s32 stream, MultiStream::StreamStatus* status) RETAIL(FUN_001e3a98);
    // Handles the disc's error the IOP reports: -1 while there's one, -2 for flag 1, else a status
    s32 MsCheckDisc() RETAIL(FUN_001e3d10);
    // Sends the batch and waits until the stream (the low byte) stops, handling the IOP's events on the way. With bits 8-15 set
    // only the stream's events count. Returns the event's stream + 1 when the IOP reports one, 0 when the stream stopped.
    // Keeping the progress on leaves the IOP moving the progress word on
    s32 MsWaitForStream(u32 streamAndMatch, s32 keepProgress) RETAIL(FUN_001e3f58);
    // The file query's result
    u32 MsGetFileInfo(u32 info[6]) RETAIL(FUN_001e81f0);
    // The stream of the IOP's last event, -1 for none
    s32 MsGetEventStream() RETAIL(FUN_001e8910);
    // 1 when free, 0 when not, -1 past the channels
    s32 MsIsChannelFree(u32 channel) RETAIL(FUN_001e88b8);
    s32 MsBufferSize(s32 first, s32 second) RETAIL(FUN_001e89e0);
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
