#include "multistream.h"

#include <kernel.h>

using namespace MultiStream;

namespace
{
constexpr s32 ServerId = 0x12345;
constexpr s32 CallbackServerId = 0x12344321;
constexpr u16 BatchMagic = 0x48;
// Every batch sent on its own ends with it, with the words 0xFFFF and 0
constexpr s32 CommandFrameEnd = 9;
// The command the EE's server starts with, with the words 0 and 0
constexpr s32 CommandServerStarted = 0x51;
constexpr s32 SendBufferWords = TransferSize / sizeof(u16);
// The IOP is about to read the EE's memory: the cache is written back
constexpr s32 RequestReadMemory = 999;
}

extern "C"
{
    void MsReadReply() RETAIL(FUN_001e26d8);
    void MsCallEnded(void*) RETAIL(fBufferReceiverFromUnkRPC_Call);
    void* MsServe(s32 function, void* data, s32 size) RETAIL(FUN_001e4380);
    void MsServerMain(void* argument) RETAIL(FUN_001e8710);

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
        D_00309FC4 = -1;
        MsLock();
        g_MsServer.running = 0;
        D_003B4680 = 0;
        D_003B3E80 = 0;
        g_MsVolume = 0x28;
        g_MsServer.unknown18 = 0;
        g_MsStatus4 = 0x5010;
        D_00309FC8 = 0;
        D_00309FCC = 0;
        D_00309FC9 = 0;
        D_0030A8B8 = 0;
        g_MsRequestCounter = 0;
        D_00309FAC = 0;
        for (u32 i = 0; i < Streams; i++)
        {
            g_MsChannelVolumesLeft[i] = 0;
            g_MsChannelVolumesRight[i] = 0;
            g_MsChannelGroups[i] = 0;
            g_MsStreamStates[i] = 0;
            g_MsStreamWord3Top[i] = 0;
            g_MsStreamWord3Low[i] = 0;
            g_MsChannelStates[i] = 0;
            g_MsSlotStreams[i] = 0xFF;
        }

        for (u32 i = 0; i < 4; i++)
        {
            g_MsGroupVolumesLeft[i] = 0x1000;
            g_MsGroupVolumesRight[i] = 0x1000;
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

            for (s32 wait = 0x270E; wait != -1; wait--)
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

        if (g_MsBatchLength + g_MsCommandLength + 3 >= static_cast<s32>(BatchWords))
        {
            g_MsFlushing = 1;
            while (MsIsBusy() == 1)
            {
            }

            s32 sent = MsSend(0);
            g_MsFlushing = 0;
            if (sent == -1)
            {
                g_MsBatchFailed = 1;
                return;
            }
        }

        g_MsBatch[g_MsBatchLength + 2] = static_cast<u16>(g_MsCommand);
        g_MsBatch[g_MsBatchLength + 3] = static_cast<u16>(g_MsCommandLength);
        g_MsBatchFailed = 0;
        for (s32 i = 0; i < g_MsCommandLength; i++)
        {
            g_MsBatch[g_MsBatchLength + 4 + i] = g_MsCommandWords[i];
        }

        g_MsBatchLength += 2;
        g_MsBatchCommands++;
        g_MsCommand = NoCommand;
        g_MsBatchLength += g_MsCommandLength;
        g_MsCommandLength = 0;
    }

    s32 MsSend(u32 mode)
    {
        MsLock();
        if (mode == 0)
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

        if (g_MsFlushing == 0)
        {
            MsLock();
            Begin(CommandFrameEnd);
            Push(0xFFFF);
            Push(0);
            MsCommit();
            g_MsFlushing = 1;
            MsUnlock();
        }

        g_MsBatch[1] = static_cast<u16>(g_MsBatchCommands);
        g_MsBatch[0] = BatchMagic;
        g_MsFlushing = 0;
        for (s32 i = 0; i < g_MsBatchLength + 2; i++)
        {
            g_MsSendBuffer[i] = g_MsBatch[i];
        }

        g_MsBusy = 1;
        if (g_MsSyncState == 1)
        {
            g_MsSyncState = 2;
        }

        sceSifCallRpc(&g_MsClient, 0, static_cast<s32>(mode), g_MsSendBuffer, TransferSize, g_MsReply, TransferSize, MsCallEnded,
                      nullptr);
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
        if (g_MsSyncState == 2)
        {
            g_MsSyncState = 0;
            D_00309FD8 = 0;
        }

        const u32* reply = g_MsReply;
        u32 records = *reinterpret_cast<const u8*>(g_MsReply);
        g_MsLastEvent = 0;
        u32 base = records * 9;
        g_MsTick = reply[base + 1];
        if (g_MsTick == g_MsLastTick)
        {
            return;
        }

        g_MsLastTick = g_MsTick;
        for (u32 stream = 0; stream < Streams; stream++)
        {
            if (g_MsStreamStates[stream] == 5)
            {
                g_MsStreamCountdown[stream]--;
                if (g_MsStreamCountdown[stream] == 0)
                {
                    g_MsStreamStates[stream] = 0;
                }
            }

            if (g_MsStreamStates[stream] == 1)
            {
                g_MsStreamCountdown[stream] = 3;
                g_MsStreamStates[stream] = 0;
            }
            else if (g_MsStreamStates[stream] == 2)
            {
                g_MsStreamStates[stream] = 5;
            }

            g_MsSlotStreams[stream] = 0xFF;
        }

        u32 lastEvent = g_MsLastEvent;
        u32 lastEventStream = g_MsLastEventStream;
        for (u32 i = 0; i < records; i++)
        {
            const StreamRecord* record = reinterpret_cast<const StreamRecord*>(&reply[1 + i * 9]);
            u32 header = record->header;
            u32 stream = (static_cast<s32>(header) >> 4) & 0x3F;
            g_MsStreamStates[stream] = g_MsStreamStates[stream] == 3 ? 4 : 1;
            g_MsStreamRecordBits[stream] = (static_cast<s32>(header) >> 2) & 0x3;
            u32 slot = (static_cast<s32>(header) >> 10) & 0x3F;
            if ((header & 0x3) != 0)
            {
                lastEvent = header & 0x3;
                lastEventStream = stream;
            }

            u8 word3Low = static_cast<u8>(record->words[2]);
            g_MsStreamRecordHigh[stream] = static_cast<s16>(static_cast<s32>(header) >> 16);
            g_MsStreamWord1[stream] = record->words[0] & 0xFFFFFF;
            g_MsStreamWord2[stream] = record->words[1];
            g_MsStreamWord3Top[stream] = static_cast<u8>(record->words[2] >> 31);
            g_MsStreamWord3Low[stream] = word3Low;
            // By the slot it had
            if (word3Low == 0)
            {
                g_MsSlotStreams[g_MsStreamSlots[stream]] = static_cast<u8>(stream);
            }

            g_MsStreamSlots[stream] = static_cast<u8>(slot);
            g_MsStreamWord4[stream] = record->words[3];
            g_MsStreamWord5[stream] = record->words[4];
            g_MsStreamWord6[stream] = g_MsStreamStates[stream] == 1 ? record->words[5] : 0;
            g_MsStreamWord7Low[stream] = static_cast<s16>(record->words[6]);
            if ((g_MsStreamWord7High[stream] & 0x8000) != 0)
            {
                g_MsStreamWord7High[stream] &= 0x7FFF;
            }
            else
            {
                g_MsStreamWord7High[stream] = static_cast<u16>(record->words[6] >> 16);
            }

            g_MsStreamTime[stream] = record->words[7];
        }

        if (records != 0)
        {
            g_MsLastEventStream = lastEventStream;
            g_MsLastEvent = lastEvent;
        }

        for (u32 stream = 0; stream < Streams; stream++)
        {
            if (g_MsStreamStates[stream] == 4)
            {
                g_MsStreamStates[stream] = 3;
            }
            else if (g_MsStreamStates[stream] == 3)
            {
                g_MsStreamStates[stream] = 0;
            }
        }

        g_MsStatus2 = reply[base + 2];
        g_MsStatus3 = reply[base + 3];
        if (g_MsHoldStatus4 == 0)
        {
            g_MsStatus4 = reply[base + 4];
        }
        else
        {
            g_MsHoldStatus4 = 0;
        }

        g_MsStatus5 = reply[base + 5];
        // 16 channels per word, from the top bits down
        for (u32 word = 0; word < 3; word++)
        {
            for (u32 i = 0; i < 16; i++)
            {
                u8* state = &g_MsChannelStates[word * 16 + i];
                u32 value = (static_cast<s32>(reply[base + 6 + word]) >> (30 - 2 * i)) & 0x3;
                if (value != 0)
                {
                    *state = static_cast<u8>(value);
                }
                else
                {
                    *state = *state == 3 ? 4 : 0;
                }
            }
        }

        g_MsStatus9 = reply[base + 9];
        g_MsStatus10 = reply[base + 10];
        g_MsStatus11 = reply[base + 11];
        for (u32 i = 0; i < 9; i++)
        {
            g_MsStatusTable12[i] = reply[base + 12 + i];
            g_MsStatusTable21[i] = reply[base + 21 + i];
            g_MsStatusTable30[i] = reply[base + 30 + i];
            g_MsStatusTable39[i] = reply[base + 39 + i];
        }

        u32 word51 = reply[base + 51];
        g_MsStatus48 = reply[base + 48];
        g_MsStatus49 = reply[base + 49];
        g_MsStatus50 = reply[base + 50];
        g_MsFileInfo51High = static_cast<u32>(static_cast<s32>(word51) >> 8);
        g_MsFileInfo51Low = word51 & 0xFF;
        g_MsFileInfo52 = reply[base + 52];
        g_MsFileInfo53 = reply[base + 53];
        g_MsStatus54 = reply[base + 54];
        g_MsStatus55 = reply[base + 55];
        if (g_MsHoldStatus56 == 0)
        {
            g_MsStatus56 = reply[base + 56];
        }
        else
        {
            g_MsHoldStatus56 = 0;
        }

        if (g_MsHoldStatus57 == 0)
        {
            g_MsStatus57 = reply[base + 57];
        }
        else
        {
            g_MsHoldStatus57 = 0;
        }

        u32 flags = reply[base + 58];
        g_MsFlag0 = static_cast<u8>(flags) & 1;
        g_MsFlag1 = (flags >> 1) & 1;
        g_MsDiscError = (flags >> 2) & 1;
        g_MsStatus59 = reply[base + 59];
        g_MsStatus60 = reply[base + 60];
        g_MsStatus61 = reply[base + 61];
        g_MsStatus62 = reply[base + 62];
        g_MsStatus63 = reply[base + 63];
        g_MsStatus64 = reply[base + 64];
        g_MsStatus65 = reply[base + 65];
        g_MsStatus66 = reply[base + 66];
        s32 values = static_cast<s32>(reply[base + 67]);
        u32 valueCount = 0;
        if (values > 0)
        {
            for (s32 i = 0; i < values; i++)
            {
                g_MsStreamValues[i] = reply[base + 68 + i];
            }

            valueCount = static_cast<u32>(values);
        }

        u32 at = base + 48 + valueCount;
        g_MsStatusAfterValues = reply[at + 21];
        for (u32 i = 0; i < Streams; i++)
        {
            g_MsChannelValues[i] = reply[at + 22 + i];
        }

        at += Streams;
        g_MsStatusB = reply[at + 23];
        g_MsStatusC = reply[at + 24];
        for (u32 i = 0; i < 8; i++)
        {
            g_MsStatusTableD[i] = reply[at + 25 + i];
        }

        g_MsStatusE = reply[at + 33];
        at += 34;
        s32 entries = static_cast<s32>(reply[at]);
        u32 entryCount = 0;
        g_MsEntryCount = reply[at];
        if (entries > 0)
        {
            for (s32 i = 0; i < entries; i++)
            {
                g_MsEntries[i * 3] = reply[at + 2 + i * 3];
                g_MsEntries[i * 3 + 1] = reply[at + 3 + i * 3];
                g_MsEntries[i * 3 + 2] = reply[at + 1 + i * 3];
            }

            entryCount = static_cast<u32>(entries);
        }

        at += 1 + entryCount * 3;
        g_MsStatusF = static_cast<u8>(reply[at]);
        g_MsStatusG = reply[at + 1];
        g_MsStatusGIndex = static_cast<s32>(at + 1);
        g_MsFileInfoLast1 = reply[at + 2];
        g_MsFileInfoLast2 = reply[at + 3];
    }

    // The IOP's requests: 999 before it reads the EE's memory, the rest go to the callback. The answer's first word is 0 when the
    // callback's mode is 2
    void* MsServe(s32, void* data, s32)
    {
        const s32* request = static_cast<const s32*>(data);
        g_MsServerRequested = 1;
        g_MsServer.request = request[0];
        g_MsServer.argument1 = request[1];
        g_MsServer.size = request[2];
        g_MsServer.argument3 = request[3];
        g_MsServer.address = reinterpret_cast<u8*>(request[4]);
        g_MsServer.argument5 = request[5];
        g_MsServer.argument6 = request[6];
        if (g_MsServer.request == RequestReadMemory)
        {
            SyncDCache(g_MsServer.address, g_MsServer.address + g_MsServer.size - 1);
            return &g_MsServerAnswer;
        }

        if (g_MsServerCallback == nullptr)
        {
            g_MsServerAnswer.value = 0;
        }
        else
        {
            g_MsServerCallbackRunning = 1;
            g_MsServerAnswer.unknown08 = 0;
            g_MsServerAnswer.unknown18 = 0;
            u32 result = g_MsServerCallback();
            g_MsServerCallbackRunning = 0;
            g_MsServerAnswer.value = result & ~0xFu;
            u32 mode = result & 0xF;
            if (mode == 2 || mode == 4)
            {
                g_MsServer.mode = static_cast<s32>(mode);
            }
        }

        if (g_MsServer.argument3 < 2 && g_MsServer.mode == 3)
        {
            g_MsServer.mode = 2;
        }

        g_MsServerAnswer.unavailable = g_MsServer.mode == 2 ? 0 : 1;
        return &g_MsServerAnswer;
    }

    void MsServerMain(void*)
    {
        sceSifSetRpcQueue(&g_MsServerQueue, GetThreadId());
        sceSifRegisterRpc(&g_MsServerData, CallbackServerId, MsServe, g_MsServerBuffer, nullptr, nullptr, &g_MsServerQueue);
        sceSifRpcLoop(&g_MsServerQueue);
    }

    s32 MsStartServer(s32 priority, void* stack, s32 stackSize)
    {
        ee_thread_t thread{};
        thread.func = reinterpret_cast<void*>(MsServerMain);
        thread.stack = stack;
        thread.stack_size = stackSize;
        thread.gp_reg = &_gp;
        thread.initial_priority = priority;
        g_MsServerStack = stack;
        g_MsServer.running = 0;
        g_MsServerThread = CreateThread(&thread);
        if (g_MsServerThread <= 0)
        {
            return -1;
        }

        g_MsServer.running = 1;
        StartThread(g_MsServerThread, nullptr);
        g_MsServer.unknown18 = 0;
        Begin(CommandServerStarted);
        Push(0);
        Push(0);
        MsCommit();
        return 0;
    }
}
