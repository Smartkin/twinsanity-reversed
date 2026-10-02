#include "multistream.h"

#include "retail/libc.h"

// Playing streams and sounds, and the status the IOP reports on them
using namespace MultiStream;

namespace
{
// Every frame's end acknowledges no stream's event (0xFFFF)
constexpr s32 CommandAcknowledge = 9;
constexpr s32 CommandProgressAddress = 0x49;

u32 ReadProgress()
{
    // The IOP writes the word behind the cache's back
    return *reinterpret_cast<volatile u32*>(reinterpret_cast<u32>(&g_MsProgress) | 0x20000000);
}
}

extern "C"
{
    s32 MsFirstFreeStream()
    {
        for (s32 stream = 0; stream < g_MsStreamCount; stream++)
        {
            if (g_MsStreamStates[stream] == 0 || g_MsStreamStates[stream] == 3)
            {
                return stream;
            }
        }

        return -1;
    }

    s32 MsFreeStreamIn(s32 first, s32 last)
    {
        if (last >= g_MsStreamCount || last < first)
        {
            return -1;
        }

        for (s32 stream = first; stream <= last; stream++)
        {
            if (g_MsStreamStates[stream] == 0 || g_MsStreamStates[stream] == 3)
            {
                return stream;
            }
        }

        return -1;
    }

    s32 MsTransfer(u32 mode, u8 stream, u32 file, u32 destination)
    {
        MsLock();
        if (mode != MsTransferToEe && mode != MsTransfer7B && mode != MsTransfer7D && mode != MsTransferToSound)
        {
            MsUnlock();
            return -2;
        }

        if (destination != 0)
        {
            if (mode == MsTransferToEe)
            {
                MsSetEeDestination(destination);
            }
            else if (mode == MsTransferToSound)
            {
                MsSetSoundDestination(destination);
            }
            else if (mode == MsTransfer7D)
            {
                MsSetStatus57(destination);
            }
        }

        s32 result = MsPlayStream(file, stream, 0, 0, 0, 0, mode, 0, 0);
        MsUnlock();
        return result;
    }

    s32 MsPlayStream(u32 file, s32 stream, u32 channelAndGroup, s32 left, s32 right, u16 word, u32 mode, u32 lowByte,
                     u32 highByte)
    {
        MsLock();
        if (stream == MsAnyStream)
        {
            stream = MsFirstFreeStream();
        }
        else if (stream == MsAnyEeStream || stream == MsAnyIopStream)
        {
            u8 wanted = stream == MsAnyEeStream ? 1 : 0;
            s32 first = 0;
            do
            {
                stream = MsFreeStreamIn(first, g_MsStreamCount - 1);
                if (stream != -1 && g_MsStreamBufferInEe[stream] == wanted)
                {
                    break;
                }

                // The last stream is taken when it's free, whatever its memory
                first = stream + 1;
            } while (first != g_MsStreamCount);
        }

        if (stream < 0 || stream >= g_MsStreamCount)
        {
            MsUnlock();
            return -1;
        }

        s32 group = static_cast<s32>(channelAndGroup & 0xFFFF0000);
        g_MsStreamStates[stream] = 2;
        g_MsStreamWord7High[stream] = 0x8080;
        g_MsStreamWord3Top[stream] = 0;
        u32 channel = channelAndGroup & 0xFFFF;
        if (channel >= Streams)
        {
            MsUnlock();
            return -1;
        }

        if (mode < 2)
        {
            g_MsChannelStates[channel] = 3;
            MsSetChannelGroup(static_cast<s32>(channel), group);
            MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
            left = g_MsLeftVolume;
            right = g_MsRightVolume;
        }

        g_MsSlotStreams[channel] = static_cast<u8>(stream);
        if (mode == 0x7E)
        {
            g_MsStreamWord6[stream] = 0;
        }

        Begin(1);
        Push32(file);
        Push(static_cast<u16>(channel + (stream << 8)));
        Push(static_cast<u16>(left));
        Push(static_cast<u16>(right));
        Push(word);
        Push(static_cast<u16>(mode));
        Push(static_cast<u16>(lowByte + (highByte << 8)));
        MsCommit();
        MsUnlock();
        return stream;
    }

    s32 MsPlaySound(u32 sound, u32 channelAndGroup, s32 left, s32 right, u16 word, u32 lowByte, u32 highByte, u32 last)
    {
        MsLock();
        s32 group = static_cast<s32>(channelAndGroup & 0xFFFF0000);
        u32 channel = channelAndGroup & 0xFFFF;
        if (channel >= Streams)
        {
            MsUnlock();
            return -1;
        }

        if (last > 0xFFFF)
        {
            MsUnlock();
            return -2;
        }

        g_MsChannelStates[channel] = 3;
        g_MsChannelSounds[channel] = sound;
        MsSetChannelGroup(static_cast<s32>(channel), group);
        MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
        Begin(0x39);
        Push32(sound);
        Push(static_cast<u16>(channel));
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        Push(word);
        Push(static_cast<u16>(lowByte + (highByte << 8)));
        Push(static_cast<u16>(last));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsOpenFile(s32 request, const char* path, u32 value)
    {
        MsLock();
        if (request == -1)
        {
            request = MsNextRequest();
        }

        Begin(0x1F);
        if (g_MsBatchFailed != 1)
        {
            Push32(static_cast<u32>(request));
            Push32(value);
            // The path follows as it is, its terminator in the last word
            char* text = reinterpret_cast<char*>(&g_MsCommandWords[g_MsCommandLength]);
            RetailLibc::StringCopy(text, path);
            s32 length = static_cast<s32>(RetailLibc::StringLength(path));
            text[length] = 0;
            g_MsCommandLength += (length + 2) / 2;
        }

        MsCommit();
        MsUnlock();
        return request;
    }

    void MsAddStreamChannel(s32 stream, u32 slot, u32 channelAndGroup, u16 word, s32 left, s32 right, u32 value)
    {
        s32 group = static_cast<s32>(channelAndGroup & 0xFFFF0000);
        u32 channel = channelAndGroup & 0xFFFF;
        if (stream >= g_MsStreamCount || slot >= Streams || channel >= Streams)
        {
            return;
        }

        MsLock();
        g_MsStreamStates[stream] = 2;
        g_MsChannelStates[channel] = 3;
        MsSetChannelGroup(static_cast<s32>(channel), group);
        MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
        Begin(0x2C);
        Push(static_cast<u16>((stream << 8) | slot));
        Push(static_cast<u16>(channel));
        Push(word);
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        Push32(value);
        MsCommit();
        MsUnlock();
    }

    s32 MsGetStreamStatus(s32 stream, StreamStatus* status)
    {
        status->status50 = g_MsStatus50;
        status->lastEventStream = static_cast<u8>(g_MsLastEventStream);
        status->lastEvent = static_cast<u8>(g_MsLastEvent);
        status->status3 = static_cast<u8>(g_MsStatus3);
        status->flag0 = g_MsFlag0;
        status->discError = g_MsDiscError;
        status->flag1 = g_MsFlag1;
        status->status61 = g_MsStatus61;
        status->status62 = g_MsStatus62;
        status->statusAfterValues = g_MsStatusAfterValues;
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        s8 state = g_MsStreamStates[stream];
        if (state == 0 || state == 3 || state == 2 || state == 5)
        {
            status->state = state == 0 || state == 3 ? 0 : 6;
            status->word7Low = 0;
            status->frames = 0;
            status->seconds = 0;
            status->minutes = 0;
            status->hours = 0;
        }
        else
        {
            status->state = 1;
            status->word7Low = g_MsStreamWord7Low[stream];
            u32 time = g_MsStreamTime[stream];
            status->frames = static_cast<u8>(time % 60);
            status->hours = time / 216000 & 0xFF;
            status->minutes = static_cast<u8>(time / 3600 % 60);
            status->seconds = static_cast<u8>(time / 60 % 60);
        }

        status->word7High = g_MsStreamWord7High[stream] & 0x7FFF;
        status->value = g_MsStreamValues[stream];
        status->word4 = g_MsStreamWord4[stream];
        status->word3Low = g_MsStreamWord3Low[stream];
        status->word3Top = g_MsStreamWord3Top[stream];
        status->slot = g_MsStreamSlots[stream];
        status->word2 = g_MsStreamWord2[stream];
        status->word1 = g_MsStreamWord1[stream];
        status->word1Again = g_MsStreamWord1[stream];
        status->recordBits = g_MsStreamRecordBits[stream];
        status->recordHigh = g_MsStreamRecordHigh[stream];
        status->word5 = g_MsStreamWord5[stream];
        status->word6 = g_MsStreamWord6[stream];
        return 0;
    }

    s32 MsCheckDisc()
    {
        if (g_MsFlag0 == 1)
        {
            MsLock();
            g_MsCheckedFlag0 = g_MsFlag0;
            MsHoldReading();
            while (MsSend(0) < 0)
            {
            }

            if (g_MsDiscError == 0)
            {
                MsRestartReading();
                MsSend(0);
            }

            MsUnlock();
        }
        else
        {
            g_MsCheckedFlag0 = 0;
        }

        g_MsCheckedFlag1 = g_MsFlag1;
        if (g_MsFlag0 == 1)
        {
            return -1;
        }

        if (g_MsFlag1 == 1)
        {
            return -2;
        }

        return static_cast<s32>(g_MsStatusG);
    }

    s32 MsWaitForStream(u32 streamAndMatch, s32 keepProgress)
    {
        s32 stream = static_cast<s32>(streamAndMatch & 0xFF);
        bool onlyTheStream = (streamAndMatch & 0xFF00) != 0;
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        while (MsIsBusy() == 1)
        {
        }

        if (keepProgress == 0)
        {
            MsLock();
            Begin(CommandProgressAddress);
            Push32(reinterpret_cast<u32>(&g_MsProgress));
            MsCommit();
            MsUnlock();
        }

        MsLock();
        Begin(CommandAcknowledge);
        Push(0xFFFF);
        Push(0);
        MsCommit();
        g_MsFlushing = 1;
        MsUnlock();
        MsSend(0);
        g_MsLastProgress = ReadProgress();
        StreamStatus status;
        MsGetStreamStatus(stream, &status);
        while (status.state != 0)
        {
            if (ReadProgress() != g_MsLastProgress)
            {
                g_MsLastProgress = ReadProgress();
                s32 event = MsGetEventStream();
                if (event != -1 && (!onlyTheStream || event == stream))
                {
                    if (static_cast<u8>(g_MsStatus59) != '|')
                    {
                        if (keepProgress == 0)
                        {
                            MsStopProgress();
                            MsSend(0);
                        }

                        MsUnlock();
                        return event + 1;
                    }

                    // A '|' asks for the event to be acknowledged
                    MsLock();
                    g_MsStatus3 = 1;
                    g_MsStatus2 = static_cast<u32>(-1);
                    Begin(CommandAcknowledge);
                    Push(static_cast<u16>(event));
                    Push(0);
                    MsCommit();
                    g_MsFlushing = 1;
                    MsUnlock();
                }

                MsSend(0);
                MsCheckDisc();
            }

            MsGetStreamStatus(stream, &status);
        }

        if (keepProgress == 0)
        {
            MsStopProgress();
            MsSend(0);
        }

        MsUnlock();
        return 0;
    }

    u32 MsGetFileInfo(u32 info[6])
    {
        info[0] = g_MsFileInfo53;
        info[3] = g_MsFileInfo51High;
        info[1] = g_MsFileInfo52;
        info[2] = g_MsFileInfo51Low;
        info[4] = g_MsFileInfoLast1;
        info[5] = g_MsFileInfoLast2;
        return static_cast<u8>(g_MsFileInfo51High);
    }

    s32 MsGetEventStream()
    {
        return static_cast<s32>(g_MsStatus2);
    }

    s32 MsIsChannelFree(u32 channel)
    {
        if (channel >= Streams)
        {
            return -1;
        }

        return g_MsChannelStates[channel] == 0;
    }

    s32 MsFindBank(u32 value, u32 key)
    {
        if (g_MsEntryTable == nullptr)
        {
            return -2;
        }

        for (s32 i = 0; i < g_MsEntryTableCount; i++)
        {
            MsEntry& entry = g_MsEntryTable[i];
            if (entry.key == key)
            {
                entry.value = value;
                entry.set = 1;
                return 0;
            }
        }

        return -1;
    }

    s32 MsBufferSize(s32 first, s32 second)
    {
        return first * second * 2;
    }
}
