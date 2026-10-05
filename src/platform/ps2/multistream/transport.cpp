#include "multistream.h"

#include <kernel.h>

// The batches sent to the IOP and its replies read, MultiStream's start-up, and the EE's server of the fast load
using namespace MultiStream;

namespace
{
// The module's RPC server (SOUND_DEV), the function the batches go to (SOUND_SETPARAMS) and the EE's own server
// (SOUND_FASTLOAD_RPC_DEVICE)
constexpr s32 ServerId = 0x12345;
constexpr u32 SetParametersFunction = 0;
constexpr s32 FastLoadServerId = 0x12344321;
// The loops MsInitialise waits for the server's binding each time it isn't there
constexpr s32 BindWaitLoops = 0x270E;
// The IOP's thread's priority (SOUND_THREAD_PRIORITY), and where the SPU2's memory that isn't the SPU2's own starts
// (SOUND_MemBaseAddress)
constexpr s32 IopThreadPriority = 40;
constexpr u32 SpuMemoryStart = 0x5010;
// The fast load's callback returns where the next transfer goes, its low bits a FastLoadMode
constexpr u32 FastLoadModeMask = 0xF;
}

extern "C"
{
    // SOUND_CopyIOPBuffer and SOUND_JP, the call's end
    void MsReadReply() RETAIL(FUN_001e26d8);
    void MsCallEnded(void*) RETAIL(fBufferReceiverFromUnkRPC_Call);
    // SOUND_FASTLOAD_RPC and SOUND_InitFastLoad_RPC
    void* MsFastLoadRpc(s32 function, void* data, s32 size) RETAIL(FUN_001e4380);
    void MsFastLoadMain(void* argument) RETAIL(FUN_001e8710);

    void MsLock()
    {
    }

    void MsUnlock()
    {
    }

    s32 MsIsBusy()
    {
        return g_MsBusy;
    }

    s32 MsInitialise()
    {
        g_MsLockThread = -1;
        MsLock();
        g_MsFastLoad.eeStatus = FastLoadOff;
        g_MsIopUpdates = 0;
        g_MsDspUpdates = 0;
        g_MsIopThreadPriority = IopThreadPriority;
        g_MsFastLoad.iopStatus = FastLoadOff;
        g_MsSpuWriteAddress = SpuMemoryStart;
        g_MsSleepCount = 0;
        g_MsUnusedWord = 0;
        g_MsWakeCount = 0;
        g_MsStreamDataInitialised = 0;
        g_MsFileIdCounter = 0;
        g_MsFileQueryCounter = 0;
        // Every stream, and every channel (as many)
        for (u32 i = 0; i < Streams; i++)
        {
            g_MsChannelVolumesLeft[i] = 0;
            g_MsChannelVolumesRight[i] = 0;
            g_MsChannelGroups[i] = 0;
            g_MsStreamStates[i] = StreamOff;
            g_MsStreamActive[i] = 0;
            g_MsStreamTypes[i] = AudioStream;
            g_MsChannelStates[i] = ChannelOff;
            g_MsChannelStreams[i] = NoStream;
        }

        for (u32 i = 0; i < Groups; i++)
        {
            g_MsGroupVolumesLeft[i] = Platform::Audio::FullGroupVolume;
            g_MsGroupVolumesRight[i] = Platform::Audio::FullGroupVolume;
        }

        sceSifInitRpc(0);
        do
        {
            // No server: nothing works without it
            if (sceSifBindRpc(&g_MsClient, ServerId, 0) < 0)
            {
                while (true)
                {
                }
            }

            for (s32 wait = BindWaitLoops; wait != -1; wait--)
            {
                asm volatile("");
            }
        } while (g_MsClient.server == nullptr);

        MsUnlock();
        return 0;
    }

    void MsCommit()
    {
        if (g_MsCommand == NoCommand)
        {
            return;
        }

        // The command's last word has to be in the batch
        s32 last = g_MsBatchLength + BatchHeaderWords + CommandHeaderWords + g_MsCommandLength - 1;
        if (last >= static_cast<s32>(BatchWords))
        {
            g_MsStatusRequested = 1;
            while (MsIsBusy() == 1)
            {
            }

            s32 sent = MsSend(SendWait);
            g_MsStatusRequested = 0;
            if (sent == -1)
            {
                g_MsBatchFailed = 1;
                return;
            }
        }

        u16* command = &g_MsBatch[g_MsBatchLength + BatchHeaderWords];
        command[0] = static_cast<u16>(g_MsCommand);
        command[1] = static_cast<u16>(g_MsCommandLength);
        g_MsBatchFailed = 0;
        for (s32 i = 0; i < g_MsCommandLength; i++)
        {
            command[CommandHeaderWords + i] = g_MsCommandWords[i];
        }

        g_MsBatchLength += CommandHeaderWords;
        g_MsBatchCommands++;
        g_MsCommand = NoCommand;
        g_MsBatchLength += g_MsCommandLength;
        g_MsCommandLength = 0;
    }

    s32 MsSend(u32 mode)
    {
        MsLock();
        if (mode == SendWait)
        {
            while (MsIsBusy() == 1)
            {
            }
        }
        else if (MsIsBusy() == 1)
        {
            MsUnlock();
            return -1;
        }

        if (sceSifCheckStatRpc(&g_MsClient) != 0)
        {
            MsUnlock();
            return -1;
        }

        if (g_MsStatusRequested == 0)
        {
            MsLock();
            Begin(OpGetStatus);
            Push(NoStreamAllowed);
            Push(0);
            MsCommit();
            g_MsStatusRequested = 1;
            MsUnlock();
        }

        // The header: the module's version, then the count of commands
        g_MsBatch[1] = static_cast<u16>(g_MsBatchCommands);
        g_MsBatch[0] = Version;
        g_MsStatusRequested = 0;
        for (s32 i = 0; i < g_MsBatchLength + BatchHeaderWords; i++)
        {
            g_MsSendBuffer[i] = g_MsBatch[i];
        }

        g_MsBusy = 1;
        if (g_MsDspChecksum == DspDataWaiting)
        {
            g_MsDspChecksum = DspDataSent;
        }

        sceSifCallRpc(&g_MsClient, SetParametersFunction, static_cast<s32>(mode), g_MsSendBuffer, TransferSize, g_MsReply,
                      TransferSize, MsCallEnded, nullptr);
        s32 sent = g_MsBatchLength;
        g_MsBatchLength = 0;
        g_MsBatchCommands = 0;
        MsUnlock();
        return sent;
    }

    s32 MsSendBatch(u32 mode)
    {
        return MsSend(mode);
    }

    void MsCallEnded(void*)
    {
        MsReadReply();
    }

    // Runs in the interrupt that ends the call
    void MsReadReply()
    {
        g_MsBusy = 0;
        if (g_MsDspChecksum == DspDataSent)
        {
            g_MsDspChecksum = DspIdle;
            g_MsDspCheckCount = 0;
        }

        const u32* reply = g_MsReply;
        // The count of records is the first word's low byte
        u32 recordCount = *reinterpret_cast<const u8*>(g_MsReply);
        g_MsDiscAccess = 0;
        const auto* records = reinterpret_cast<const StreamRecord*>(&reply[1]);
        const auto* status = reinterpret_cast<const ReplyStatus*>(&records[recordCount]);
        g_MsReplyCounter = status->counter;
        if (g_MsReplyCounter == g_MsLastReplyCounter)
        {
            return;
        }

        // A stream the reply leaves out is off, unless a play is on its way (for so many replies); every channel's stream is
        // forgotten (there are as many)
        g_MsLastReplyCounter = g_MsReplyCounter;
        for (u32 stream = 0; stream < Streams; stream++)
        {
            if (g_MsStreamStates[stream] == StreamPlaySent)
            {
                g_MsStreamCountdown[stream]--;
                if (g_MsStreamCountdown[stream] == 0)
                {
                    g_MsStreamStates[stream] = StreamOff;
                }
            }

            if (g_MsStreamStates[stream] == StreamOn)
            {
                g_MsStreamCountdown[stream] = PlaySentReplies;
                g_MsStreamStates[stream] = StreamOff;
            }
            else if (g_MsStreamStates[stream] == StreamPlayRequested)
            {
                g_MsStreamStates[stream] = StreamPlaySent;
            }

            g_MsChannelStreams[stream] = NoStream;
        }

        u32 discAccess = g_MsDiscAccess;
        u32 discAccessStream = g_MsDiscAccessStream;
        for (u32 i = 0; i < recordCount; i++)
        {
            const StreamRecord& record = records[i];
            RecordHeader header = record.header;
            u32 stream = header.stream;
            g_MsStreamStates[stream] = g_MsStreamStates[stream] == StreamStopRequested ? StreamStopPending : StreamOn;
            g_MsStreamPlayHalves[stream] = header.playHalf;
            u32 channel = header.channel;
            if (header.discAccess != 0)
            {
                discAccess = header.discAccess;
                discAccessStream = stream;
            }

            u8 kind = record.type.kind;
            g_MsStreamEnvelopes[stream] = static_cast<s16>(header.envelope);
            g_MsStreamSpuAddresses[stream] = record.spuAddress & SpuAddressMask;
            g_MsStreamFiles[stream] = record.file;
            g_MsStreamActive[stream] = record.type.active;
            g_MsStreamTypes[stream] = kind;
            // An audio stream is its channel's: the channel the last reply gave it
            if (kind == AudioStream)
            {
                g_MsChannelStreams[g_MsStreamChannels[stream]] = static_cast<u8>(stream);
            }

            g_MsStreamChannels[stream] = static_cast<u8>(channel);
            g_MsStreamWriteAddresses[stream] = record.writeAddress;
            g_MsStreamPlayOffsets[stream] = record.playOffset;
            g_MsStreamEeDataSizes[stream] = g_MsStreamStates[stream] == StreamOn ? record.eeDataSize : 0;
            g_MsStreamPitches[stream] = static_cast<s16>(record.pitchAndPriority.pitch);
            if ((g_MsStreamPriorities[stream] & PriorityKept) != 0)
            {
                g_MsStreamPriorities[stream] &= ~PriorityKept;
            }
            else
            {
                g_MsStreamPriorities[stream] = record.pitchAndPriority.priority;
            }

            g_MsStreamTime[stream] = record.time;
        }

        if (recordCount != 0)
        {
            g_MsDiscAccessStream = discAccessStream;
            g_MsDiscAccess = discAccess;
        }

        // A stop the IOP hadn't seen yet waits for the next reply, one it saw is done
        for (u32 stream = 0; stream < Streams; stream++)
        {
            if (g_MsStreamStates[stream] == StreamStopPending)
            {
                g_MsStreamStates[stream] = StreamStopRequested;
            }
            else if (g_MsStreamStates[stream] == StreamStopRequested)
            {
                g_MsStreamStates[stream] = StreamOff;
            }
        }

        g_MsLoadRequest = status->loadRequest;
        g_MsDiscBusy = status->discBusy;
        if (g_MsSpuWriteAddressSet == 0)
        {
            g_MsSpuWriteAddress = status->spuWriteAddress;
        }
        else
        {
            g_MsSpuWriteAddressSet = 0;
        }

        g_MsStereoNotStarted = status->stereoNotStarted;
        // A channel asked to play that the IOP reports off waits one more reply
        for (u32 word = 0; word < ChannelStateWords; word++)
        {
            for (u32 i = 0; i < ChannelsPerWord; i++)
            {
                u8* state = &g_MsChannelStates[word * ChannelsPerWord + i];
                u32 shift = (ChannelsPerWord - 1 - i) * ChannelStateBits;
                u32 value = (static_cast<s32>(status->channelStates[word]) >> shift) & ChannelStateMask;
                if (value != ChannelOff)
                {
                    *state = static_cast<u8>(value);
                }
                else
                {
                    *state = *state == ChannelRequested ? ChannelKeyedOn : ChannelOff;
                }
            }
        }

        g_MsIopDataAddress = status->iopDataAddress;
        g_MsIopDataSize = status->iopDataSize;
        g_MsIopDataCheck = status->iopDataCheck;
        for (u32 i = 0; i < ChainFiles; i++)
        {
            g_MsIopDataSeeks[i] = status->iopDataSeeks[i];
            g_MsIopDataFiles[i] = status->iopDataFiles[i];
            g_MsIopDataPlaySizes[i] = status->iopDataPlaySizes[i];
            g_MsIopDataSectors[i] = status->iopDataSectors[i];
        }

        FileQuery fileQuery = status->fileQuery;
        g_MsIopDataChainCount = status->iopDataChainCount;
        g_MsIopDataChainPosition = status->iopDataChainPosition;
        g_MsMaxIopMemory = status->maxIopMemory;
        g_MsFileCounter = static_cast<u32>(fileQuery.counter);
        g_MsFileSource = fileQuery.source;
        g_MsFileSize = status->fileSize;
        g_MsFileSector = status->fileSector;
        g_MsIopDataRemaining = status->iopDataRemaining;
        g_MsIopDataOffset = status->iopDataOffset;
        if (g_MsEeWriteAddressSet == 0)
        {
            g_MsEeWriteAddress = status->eeWriteAddress;
        }
        else
        {
            g_MsEeWriteAddressSet = 0;
        }

        if (g_MsIopWriteAddressSet == 0)
        {
            g_MsIopWriteAddress = status->iopWriteAddress;
        }
        else
        {
            g_MsIopWriteAddressSet = 0;
        }

        DiscErrors discErrors = status->discErrors;
        g_MsDiscError = discErrors.error;
        g_MsDiscInternalError = discErrors.internalError;
        g_MsDiscNotReady = discErrors.notReady;
        g_MsIopLoadType = status->iopLoadType;
        g_MsDtsStatus = status->dtsStatus;
        g_MsEeTransferSize = status->eeTransferSize;
        g_MsEeTransferCount = status->eeTransferCount;
        g_MsPcmIopAddress = status->pcmIopAddress;
        g_MsPcmSize = status->pcmSize;
        g_MsPcmAddress = status->pcmAddress;
        g_MsPcmStatus = status->pcmStatus;
        s32 buffers = status->iopBufferCount;
        u32 bufferCount = 0;
        if (buffers > 0)
        {
            for (s32 i = 0; i < buffers; i++)
            {
                g_MsStreamIopBuffers[i] = status->iopBuffers[i];
            }

            bufferCount = static_cast<u32>(buffers);
        }

        const auto* channels = reinterpret_cast<const ReplyChannels*>(&status->iopBuffers[bufferCount]);
        g_MsUserTransferStatus = channels->userTransferStatus;
        for (u32 i = 0; i < Channels; i++)
        {
            g_MsChannelNextAddresses[i] = channels->nextAddresses[i];
        }

        g_MsDebugScan[0] = channels->debugScan[0];
        g_MsDebugScan[1] = channels->debugScan[1];
        for (u32 i = 0; i < DspInfoWords; i++)
        {
            g_MsDspInfo[i] = channels->dspInfo[i];
        }

        g_MsDspBuffer = channels->dspBuffer;
        s32 transfers = channels->dspTransferCount;
        u32 transferCount = 0;
        g_MsDspTransferCount = static_cast<u32>(channels->dspTransferCount);
        const auto* replyTransfers = reinterpret_cast<const ReplyDspTransfer*>(channels + 1);
        if (transfers > 0)
        {
            for (s32 i = 0; i < transfers; i++)
            {
                g_MsDspTransfers[i].iopAddress = replyTransfers[i].iopAddress;
                g_MsDspTransfers[i].eeAddress = replyTransfers[i].eeAddress;
                g_MsDspTransfers[i].counter = replyTransfers[i].counter;
            }

            transferCount = static_cast<u32>(transfers);
        }

        const auto* end = reinterpret_cast<const ReplyEnd*>(&replyTransfers[transferCount]);
        g_MsDspStatus = static_cast<u8>(end->dspStatus);
        g_MsDiscErrorCode = end->discErrorCode;
        g_MsDiscErrorCodeIndex = &end->discErrorCode - reply;
        g_MsAtWinMonOpen = end->atWinMonOpen;
        g_MsAtWinMonFile = end->atWinMonFile;
    }

    // The IOP's calls (SOUND_FASTLOAD_RPC): FastLoadInvalidateCache before it writes the EE's memory (only in the mode
    // FastLoadInvalidateCacheOn, which the game never sets; the cache isn't written back first), the others after a transfer,
    // which the callback (nothing sets one) may stop or send elsewhere. The answer says whether the IOP may load on
    void* MsFastLoadRpc(s32, void* data, s32)
    {
        const auto* call = static_cast<const FastLoadCall*>(data);
        g_MsFastLoadCalled = 1;
        g_MsFastLoad.stream = call->stream;
        g_MsFastLoad.totalSize = call->totalSize;
        g_MsFastLoad.loadSize = call->loadSize;
        g_MsFastLoad.status = call->status;
        g_MsFastLoad.eeAddress = reinterpret_cast<u8*>(call->eeAddress);
        g_MsFastLoad.counter = call->counter;
        g_MsFastLoad.file = call->file;
        if (g_MsFastLoad.stream == FastLoadInvalidateCache)
        {
            InvalidDCache(g_MsFastLoad.eeAddress, g_MsFastLoad.eeAddress + g_MsFastLoad.loadSize - 1);
            return &g_MsFastLoadAnswer;
        }

        if (g_MsFastLoadCallback == nullptr)
        {
            g_MsFastLoadAnswer.nextEeAddress = 0;
        }
        else
        {
            g_MsFastLoadInCallback = 1;
            g_MsFastLoadAnswer.setsFileOffsetAndSize = 0;
            g_MsFastLoadAnswer.loadsNextFile = 0;
            u32 result = g_MsFastLoadCallback();
            g_MsFastLoadInCallback = 0;
            g_MsFastLoadAnswer.nextEeAddress = result & ~FastLoadModeMask;
            u32 mode = result & FastLoadModeMask;
            if (mode == FastLoadStop || mode == FastLoadContinue)
            {
                g_MsFastLoad.allowLoad = static_cast<s32>(mode);
            }
        }

        // A single transfer is the last while the IOP's status is below 2
        if (g_MsFastLoad.status < 2 && g_MsFastLoad.allowLoad == FastLoadSingle)
        {
            g_MsFastLoad.allowLoad = FastLoadStop;
        }

        g_MsFastLoadAnswer.allowLoad = g_MsFastLoad.allowLoad == FastLoadStop ? 0 : 1;
        return &g_MsFastLoadAnswer;
    }

    void MsFastLoadMain(void*)
    {
        sceSifSetRpcQueue(&g_MsFastLoadQueue, GetThreadId());
        sceSifRegisterRpc(&g_MsFastLoadServer, FastLoadServerId, MsFastLoadRpc, g_MsFastLoadArguments, nullptr, nullptr,
                          &g_MsFastLoadQueue);
        sceSifRpcLoop(&g_MsFastLoadQueue);
    }

    s32 MsInitFastLoad(s32 priority, void* stack, s32 stackSize)
    {
        ee_thread_t thread{};
        thread.func = reinterpret_cast<void*>(MsFastLoadMain);
        thread.stack = stack;
        thread.stack_size = stackSize;
        thread.gp_reg = &_gp;
        thread.initial_priority = priority;
        g_MsFastLoadStack = stack;
        g_MsFastLoad.eeStatus = FastLoadOff;
        g_MsFastLoadThread = CreateThread(&thread);
        if (g_MsFastLoadThread <= 0)
        {
            return -1;
        }

        g_MsFastLoad.eeStatus = FastLoadOn;
        StartThread(g_MsFastLoadThread, nullptr);
        g_MsFastLoad.iopStatus = FastLoadOff;
        Begin(OpFastLoad);
        Push(FastLoadBindRpc);
        Push(FastLoadOff);
        MsCommit();
        return 0;
    }
}
