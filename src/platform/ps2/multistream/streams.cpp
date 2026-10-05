#include "multistream.h"

#include "retail/libc.h"

#include <kernel.h>

// Playing streams and sounds, and the status the IOP reports on them
using namespace MultiStream;

namespace
{
// The highest loop count a sound takes
constexpr u32 MaxLoops = 0xFFFF;
// A stream's time is in frames of 1/60 s; the hours given are its low byte
constexpr u32 FramesPerSecond = 60;
constexpr u32 FramesPerMinute = 60 * FramesPerSecond;
constexpr u32 FramesPerHour = 60 * FramesPerMinute;
constexpr u32 HoursMask = 0xFF;

u32 ReadIopUpdates()
{
    return *static_cast<volatile u32*>(UNCACHED_SEG(&g_MsIopUpdates));
}

bool IsFree(s32 stream)
{
    return g_MsStreamStates[stream] == StreamOff || g_MsStreamStates[stream] == StreamStopRequested;
}
}

extern "C"
{
    s32 MsFirstFreeStream()
    {
        for (s32 stream = 0; stream < g_MsStreamCount; stream++)
        {
            if (IsFree(stream))
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
            if (IsFree(stream))
            {
                return stream;
            }
        }

        return -1;
    }

    s32 MsLoadFile(u32 mode, u8 stream, u32 file, u32 address)
    {
        MsLock();
        if (mode != LoadToEe && mode != LoadToEeLooping && mode != LoadToIop && mode != LoadToSpu)
        {
            MsUnlock();
            return -2;
        }

        if (address != 0)
        {
            if (mode == LoadToEe)
            {
                MsSetEeWriteAddress(address);
            }
            else if (mode == LoadToSpu)
            {
                MsSetSpuWriteAddress(address);
            }
            else if (mode == LoadToIop)
            {
                MsSetIopWriteAddress(address);
            }
        }

        s32 result = MsPlayStream(file, stream, 0, 0, 0, 0, mode, 0, 0);
        MsUnlock();
        return result;
    }

    s32 MsPlayStream(u32 file, s32 stream, u32 channelAndGroup, s32 left, s32 right, u16 pitch, u32 mode, u32 attack,
                     u32 release)
    {
        MsLock();
        if (stream == MsAnyStream)
        {
            stream = MsFirstFreeStream();
        }
        else if (stream == MsAnyDataStream || stream == MsAnyAudioStream)
        {
            u8 wanted = stream == MsAnyDataStream ? 1 : 0;
            s32 first = 0;
            do
            {
                stream = MsFreeStreamIn(first, g_MsStreamCount - 1);
                if (stream != -1 && g_MsStreamIsData[stream] == wanted)
                {
                    break;
                }

                // The last stream is taken when it's free, whatever its kind
                first = stream + 1;
            } while (first != g_MsStreamCount);
        }

        if (stream < 0 || stream >= g_MsStreamCount)
        {
            MsUnlock();
            return -1;
        }

        s32 group = static_cast<s32>(channelAndGroup & GroupMask);
        g_MsStreamStates[stream] = StreamPlayRequested;
        g_MsStreamPriorities[stream] = InitialPriority | PriorityKept;
        g_MsStreamActive[stream] = 0;
        u32 channel = channelAndGroup & ChannelMask;
        if (channel >= Channels)
        {
            MsUnlock();
            return -1;
        }

        // Music (ADPCM audio) marks its channel taken and scales its volumes by its group
        if (mode <= StreamOnce)
        {
            g_MsChannelStates[channel] = ChannelRequested;
            MsSetChannelGroup(static_cast<s32>(channel), group);
            MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
            left = g_MsLeftVolume;
            right = g_MsRightVolume;
        }

        g_MsChannelStreams[channel] = static_cast<u8>(stream);
        if (mode == LoadToEe)
        {
            g_MsStreamEeDataSizes[stream] = 0;
        }

        // The channel with the stream in the high byte, the envelope's rates with the release in the high byte
        Begin(OpPlayStream);
        Push32(file);
        Push(static_cast<u16>(channel + (stream << 8)));
        Push(static_cast<u16>(left));
        Push(static_cast<u16>(right));
        Push(pitch);
        Push(static_cast<u16>(mode));
        Push(static_cast<u16>(attack + (release << 8)));
        MsCommit();
        MsUnlock();
        return stream;
    }

    s32 MsPlaySound(u32 sound, u32 channelAndGroup, s32 left, s32 right, u16 pitch, u32 attack, u32 release, u32 loops)
    {
        MsLock();
        s32 group = static_cast<s32>(channelAndGroup & GroupMask);
        u32 channel = channelAndGroup & ChannelMask;
        if (channel >= Channels)
        {
            MsUnlock();
            return -1;
        }

        if (loops > MaxLoops)
        {
            MsUnlock();
            return -2;
        }

        g_MsChannelStates[channel] = ChannelRequested;
        g_MsChannelSounds[channel] = sound;
        MsSetChannelGroup(static_cast<s32>(channel), group);
        MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
        Begin(OpPlaySoundLoop);
        Push32(sound);
        Push(static_cast<u16>(channel));
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        Push(pitch);
        Push(static_cast<u16>(attack + (release << 8)));
        Push(static_cast<u16>(loops));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsOpenFile(s32 file, const char* path, u32 offset)
    {
        MsLock();
        if (file == -1)
        {
            file = MsNextFileId();
        }

        Begin(OpCreateFileInfo);
        if (g_MsBatchFailed != 1)
        {
            Push32(static_cast<u32>(file));
            Push32(offset);
            // The path follows as it is, its terminator in the last word
            char* text = reinterpret_cast<char*>(&g_MsCommandWords[g_MsCommandLength]);
            RetailLibc::StringCopy(text, path);
            s32 length = static_cast<s32>(RetailLibc::StringLength(path));
            text[length] = 0;
            g_MsCommandLength += (length + 2) / 2;
        }

        MsCommit();
        MsUnlock();
        return file;
    }

    void MsAddStreamChannel(s32 stream, u32 parent, u32 channelAndGroup, u16 track, s32 left, s32 right, u32 spuAddress)
    {
        s32 group = static_cast<s32>(channelAndGroup & GroupMask);
        u32 channel = channelAndGroup & ChannelMask;
        if (stream >= g_MsStreamCount || parent >= Streams || channel >= Channels)
        {
            return;
        }

        MsLock();
        g_MsStreamStates[stream] = StreamPlayRequested;
        g_MsChannelStates[channel] = ChannelRequested;
        MsSetChannelGroup(static_cast<s32>(channel), group);
        MsStoreChannelVolumes(static_cast<s32>(channel), static_cast<s16>(left), static_cast<s16>(right));
        // The stream in the high byte, its parent in the low
        Begin(OpSetStreamChild);
        Push(static_cast<u16>((stream << 8) | parent));
        Push(static_cast<u16>(channel));
        Push(track);
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        Push32(spuAddress);
        MsCommit();
        MsUnlock();
    }

    s32 MsGetStreamStatus(s32 stream, StreamStatus* status)
    {
        status->maxIopMemory = g_MsMaxIopMemory;
        status->discAccessStream = static_cast<u8>(g_MsDiscAccessStream);
        status->discAccess = static_cast<u8>(g_MsDiscAccess);
        status->discBusy = static_cast<u8>(g_MsDiscBusy);
        status->discError = g_MsDiscError;
        status->discNotReady = g_MsDiscNotReady;
        status->discInternalError = g_MsDiscInternalError;
        status->eeTransferSize = g_MsEeTransferSize;
        status->eeTransferCount = g_MsEeTransferCount;
        status->userTransferStatus = g_MsUserTransferStatus;
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        s8 state = g_MsStreamStates[stream];
        if (state == StreamOff || state == StreamStopRequested || state == StreamPlayRequested || state == StreamPlaySent)
        {
            status->state = state == StreamOff || state == StreamStopRequested ? StreamOff : StreamWaitingToPlay;
            status->pitch = 0;
            status->frames = 0;
            status->seconds = 0;
            status->minutes = 0;
            status->hours = 0;
        }
        else
        {
            status->state = StreamOn;
            status->pitch = g_MsStreamPitches[stream];
            u32 time = g_MsStreamTime[stream];
            status->frames = static_cast<u8>(time % FramesPerSecond);
            status->hours = time / FramesPerHour & HoursMask;
            status->minutes = static_cast<u8>(time / FramesPerMinute % 60);
            status->seconds = static_cast<u8>(time / FramesPerSecond % 60);
        }

        status->priority = g_MsStreamPriorities[stream] & ~PriorityKept;
        status->iopBuffer = g_MsStreamIopBuffers[stream];
        status->writeAddress = g_MsStreamWriteAddresses[stream];
        status->type = g_MsStreamTypes[stream];
        status->active = g_MsStreamActive[stream];
        status->channel = g_MsStreamChannels[stream];
        status->file = g_MsStreamFiles[stream];
        status->spuAddress = g_MsStreamSpuAddresses[stream];
        status->destinationAddress = g_MsStreamSpuAddresses[stream];
        status->playHalf = g_MsStreamPlayHalves[stream];
        status->envelope = g_MsStreamEnvelopes[stream];
        status->playOffset = g_MsStreamPlayOffsets[stream];
        status->eeDataSize = g_MsStreamEeDataSizes[stream];
        return 0;
    }

    s32 MsHandleDiscErrors()
    {
        if (g_MsDiscError == 1)
        {
            MsLock();
            g_MsLastDiscError = g_MsDiscError;
            MsCheckDiscError();
            while (MsSend(SendWait) < 0)
            {
            }

            if (g_MsDiscNotReady == 0)
            {
                MsRestartFromDiscError();
                MsSend(SendWait);
            }

            MsUnlock();
        }
        else
        {
            g_MsLastDiscError = 0;
        }

        g_MsLastDiscInternalError = g_MsDiscInternalError;
        if (g_MsDiscError == 1)
        {
            return -1;
        }

        if (g_MsDiscInternalError == 1)
        {
            return -2;
        }

        return static_cast<s32>(g_MsDiscErrorCode);
    }

    s32 MsWaitForStream(u32 waitStream, s32 kind)
    {
        WaitStream wait;
        wait.value = waitStream;
        s32 stream = static_cast<s32>(wait.stream);
        bool onlyTheStream = wait.onlyThisStream != 0;
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        while (MsIsBusy() == 1)
        {
        }

        if (kind == WaitCold)
        {
            MsLock();
            Begin(OpInitWait);
            Push32(reinterpret_cast<u32>(&g_MsIopUpdates));
            MsCommit();
            MsUnlock();
        }

        MsLock();
        Begin(OpGetStatus);
        Push(NoStreamAllowed);
        Push(0);
        MsCommit();
        g_MsStatusRequested = 1;
        MsUnlock();
        MsSend(SendWait);
        g_MsLastIopUpdate = ReadIopUpdates();
        StreamStatus status;
        MsGetStreamStatus(stream, &status);
        while (status.state != StreamOff)
        {
            // A call only once the IOP's thread has run again, not to keep it from working
            if (ReadIopUpdates() != g_MsLastIopUpdate)
            {
                g_MsLastIopUpdate = ReadIopUpdates();
                s32 request = MsGetLoadRequest();
                if (request != -1 && (!onlyTheStream || request == stream))
                {
                    // Another loading than MultiStream's own is the caller's
                    if (static_cast<u8>(g_MsIopLoadType) != LoadByModule)
                    {
                        if (kind == WaitCold)
                        {
                            MsCloseWaitUpdate();
                            MsSend(SendWait);
                        }

                        MsUnlock();
                        return request + 1;
                    }

                    // The stream that wants more data may load it
                    MsLock();
                    g_MsDiscBusy = 1;
                    g_MsLoadRequest = -1;
                    Begin(OpGetStatus);
                    Push(static_cast<u16>(request));
                    Push(0);
                    MsCommit();
                    g_MsStatusRequested = 1;
                    MsUnlock();
                }

                MsSend(SendWait);
                MsHandleDiscErrors();
            }

            MsGetStreamStatus(stream, &status);
        }

        if (kind == WaitCold)
        {
            MsCloseWaitUpdate();
            MsSend(SendWait);
        }

        MsUnlock();
        return 0;
    }

    u32 MsGetFileInfo(FileInfo* info)
    {
        info->sector = g_MsFileSector;
        info->counter = g_MsFileCounter;
        info->size = g_MsFileSize;
        info->source = g_MsFileSource;
        info->atWinMonOpen = g_MsAtWinMonOpen;
        info->atWinMonFile = g_MsAtWinMonFile;
        return static_cast<u8>(g_MsFileCounter);
    }

    s32 MsGetLoadRequest()
    {
        return g_MsLoadRequest;
    }

    s32 MsIsChannelFree(u32 channel)
    {
        if (channel >= Channels)
        {
            return -1;
        }

        return g_MsChannelStates[channel] == ChannelOff;
    }

    s32 MsCheckSoundId(u32 sound, u32 address)
    {
        if (g_MsMemoryBlocks == nullptr)
        {
            return -2;
        }

        for (s32 i = 0; i < g_MsMemoryBlockCount; i++)
        {
            MsMemoryBlock& block = g_MsMemoryBlocks[i];
            if (block.address == address)
            {
                block.sound = sound;
                block.hasSound = 1;
                return 0;
            }
        }

        return -1;
    }

    s32 MsInterleavedBufferSize(s32 trackSize, s32 files)
    {
        return trackSize * files * 2;
    }
}
