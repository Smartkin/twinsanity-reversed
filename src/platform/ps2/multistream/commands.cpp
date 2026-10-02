#include "multistream.h"

// The commands with nothing more to them than their words. Their names are the IOP side's where it's known, the opcode's
// otherwise
using namespace MultiStream;

namespace
{
// A channel's volume scaled by its group's (4.12 fixed point). Volumes past 0x3FFF are the SPU2's inverted phase, scaled from
// the other end
s32 ScaleVolume(s32 groupVolume, s32 volume)
{
    bool inverted = volume > 0x3FFF;
    if (inverted)
    {
        volume = 0x7FFF - volume;
    }

    s32 scaled = volume * groupVolume;
    if (scaled < 0)
    {
        scaled += 0xFFF;
    }

    scaled >>= 12;
    return inverted ? 0x7FFF - scaled : scaled;
}
}

extern "C"
{
    s32 MsGroupScale(s32 groupVolume, s32 volume)
    {
        return ScaleVolume(groupVolume, volume);
    }

    // Groups are numbered from 1, in the top half of the value
    void MsComputeChannelVolumes(s32 group, s32 channel)
    {
        s32 index = (group >> 16) - 1;
        g_MsLeftVolume = MsGroupScale(g_MsGroupVolumesLeft[index], g_MsChannelVolumesLeft[channel]);
        g_MsRightVolume = MsGroupScale(g_MsGroupVolumesRight[index], g_MsChannelVolumesRight[channel]);
    }

    void MsSetChannelGroup(s32 channel, s32 group)
    {
        g_MsChannelGroups[channel] = group;
    }

    void MsStoreChannelVolumes(s32 channel, s16 left, s16 right)
    {
        s32 group = g_MsChannelGroups[channel];
        g_MsChannelVolumesLeft[channel] = left;
        g_MsChannelVolumesRight[channel] = right;
        s32 leftVolume = left;
        s32 rightVolume = right;
        if (group != 0)
        {
            s32 index = (group >> 16) - 1;
            g_MsLeftVolume = MsGroupScale(g_MsGroupVolumesLeft[index], left);
            rightVolume = MsGroupScale(g_MsGroupVolumesRight[index], g_MsChannelVolumesRight[channel]);
            leftVolume = g_MsLeftVolume;
        }

        g_MsRightVolume = rightVolume;
        g_MsLeftVolume = leftVolume;
    }

    s32 MsSetGroupVolume(s32 group, u32 left, u32 right)
    {
        u32 index = static_cast<u32>(group) >> 16;
        if (index >= 5 || group == 0)
        {
            return -1;
        }

        if (left > 0x1000)
        {
            return -2;
        }

        if (right > 0x1000)
        {
            return -3;
        }

        g_MsGroupVolumesLeft[index - 1] = static_cast<s16>(left);
        g_MsGroupVolumesRight[index - 1] = static_cast<s16>(right);
        MsApplyGroupVolume(group);
        return 0;
    }

    // Sends the channels of the group that play their volumes again
    void MsApplyGroupVolume(s32 group)
    {
        for (s32 channel = 0; channel < static_cast<s32>(Streams); channel++)
        {
            if (g_MsChannelGroups[channel] != group || g_MsChannelStates[channel] == 0)
            {
                continue;
            }

            MsLock();
            MsComputeChannelVolumes(group, channel);
            Begin(2);
            Push(static_cast<u16>(channel));
            Push(static_cast<u16>(g_MsLeftVolume));
            Push(static_cast<u16>(g_MsRightVolume));
            MsCommit();
            MsUnlock();
        }
    }

    s32 MsSetChannelVolume(s16 channel, s16 left, s16 right)
    {
        if (static_cast<u16>(channel) >= Streams)
        {
            return -1;
        }

        MsStoreChannelVolumes(channel, left, right);
        MsLock();
        Begin(0x60);
        Push(static_cast<u16>(channel));
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsNextRequest()
    {
        s32 request = g_MsRequestCounter;
        g_MsRequestCounter++;
        if (g_MsRequestCounter == -1)
        {
            g_MsRequestCounter = 0;
        }

        return request;
    }

    // 48 kHz is 4096
    s32 MsPitchOfRate(s32 rate)
    {
        return (rate << 12) / 48000;
    }

    s32 MsStreamOnSlot(u32 slot)
    {
        if (slot >= Streams)
        {
            return -2;
        }

        return static_cast<s8>(g_MsSlotStreams[slot]);
    }

    s32 MsSetVoicePitch(u32 channel, u16 value)
    {
        if (channel >= Streams)
        {
            return -1;
        }

        MsLock();
        Begin(3);
        Push(static_cast<u16>(channel));
        Push(value);
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsKeyOff(u32 channel)
    {
        if (channel >= Streams)
        {
            return -1;
        }

        MsLock();
        Begin(4);
        Push(static_cast<u16>(channel));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsStopStream(s32 stream)
    {
        if (stream < 0)
        {
            return -1;
        }

        MsLock();
        if (stream >= 64)
        {
            u32 slot = static_cast<u32>(stream - 64);
            s32 playing = MsStreamOnSlot(slot);
            if (playing < 0)
            {
                MsUnlock();
                return -1;
            }

            g_MsSlotStreams[slot] = 0xFF;
            stream = playing;
        }

        g_MsStreamStates[stream] = 3;
        Begin(5);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsResetSound()
    {
        MsLock();
        Begin(7);
        MsCommit();
        MsUnlock();
    }

    void MsPauseAll()
    {
        MsLock();
        Begin(0xA);
        MsCommit();
        MsUnlock();
    }

    void MsResumeAll()
    {
        MsLock();
        Begin(0xB);
        MsCommit();
        MsUnlock();
    }

    void MsConfigure(u16 first, u16 second, u16 third)
    {
        MsLock();
        Begin(0xC);
        Push(first);
        Push(second);
        Push(third);
        Push(static_cast<u16>(g_MsVolume));
        MsCommit();
        D_0030A8B8 = 1;
        MsUnlock();
    }

    s32 MsSetStreamBuffer(s32 stream, u32 location, u32 size)
    {
        if (stream >= static_cast<s32>(Streams))
        {
            return -1;
        }

        MsLock();
        g_MsStreamBufferSizes[stream] = size;
        g_MsStreamBufferCapacities[stream] = size;
        g_MsStreamBufferUsed[stream] = size;
        if (location == 1)
        {
            location = 0;
            g_MsStreamBufferInEe[stream] = 1;
        }
        else
        {
            g_MsStreamBufferInEe[stream] = 0;
        }

        Begin(0xD);
        Push(static_cast<u16>(stream));
        Push32(location);
        Push32(size);
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsSetSoundDestination(u32 address)
    {
        MsLock();
        g_MsHoldStatus4 = 1;
        g_MsStatus4 = address;
        Begin(0xE);
        Push32(address);
        MsCommit();
        MsUnlock();
    }

    s32 MsReleaseStreamBuffer(u32 stream)
    {
        if (stream >= Streams)
        {
            return -1;
        }

        MsLock();
        g_MsStreamBufferSizes[stream] = 0;
        g_MsStreamBufferCapacities[stream] = 0;
        g_MsStreamBufferUsed[stream] = 0;
        g_MsStreamBufferInEe[stream] = 0;
        Begin(0xF);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsSetStreamCount(u32 count)
    {
        if (count >= Streams)
        {
            return -1;
        }

        MsLock();
        g_MsStreamCount = static_cast<s32>(count);
        Begin(0x10);
        Push(static_cast<u16>(count));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsSetEffect(u32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback)
    {
        if (core >= 2 || mode >= 10)
        {
            return -1;
        }

        MsLock();
        u32 other = 1 - core;
        if (g_MsEffectModes[other] == 0)
        {
            g_MsEffectAreaEnds[core] = g_MsEffectAreaEnd;
        }
        else if (g_MsEffectAreaEnds[other] == g_MsEffectAreaEnd)
        {
            g_MsEffectAreaEnds[core] = g_MsEffectOtherAreaEnd;
        }
        else
        {
            g_MsEffectAreaEnds[core] = g_MsEffectAreaEnd;
        }

        g_MsEffectSizes[core] = g_MsEffectSizesByMode[mode];
        g_MsEffectModes[core] = mode;
        Begin(0x13);
        Push(static_cast<u16>(core));
        Push(static_cast<u16>(mode));
        Push(depthLeft);
        Push(depthRight);
        Push(delay);
        Push(feedback);
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsClearEffect(s32 core)
    {
        MsLock();
        g_MsEffectModes[core] = 0;
        g_MsEffectSizes[core] = 0;
        Begin(0x14);
        Push(static_cast<u16>(core));
        MsCommit();
        MsUnlock();
    }

    void MsSetEffectVolume(u16 first, u16 second, u16 third)
    {
        MsLock();
        Begin(0x15);
        Push(first);
        Push(second);
        Push(third);
        MsCommit();
        MsUnlock();
    }

    void MsEffectSendOn(u16 value)
    {
        MsLock();
        Begin(0x18);
        Push(value);
        MsCommit();
        MsUnlock();
    }

    void MsEffectSendOff(u16 value)
    {
        MsLock();
        Begin(0x19);
        Push(value);
        MsCommit();
        MsUnlock();
    }

    s32 MsSetBankAddress(u32 bank, u32 address)
    {
        MsLock();
        Begin(0x1C);
        Push32(bank);
        Push32(address);
        MsCommit();
        MsUnlock();
        return MsFindBank(bank, address);
    }

    void MsCloseFile(u32 file)
    {
        MsLock();
        Begin(0x20);
        Push32(file);
        MsCommit();
        MsUnlock();
    }

    void MsReserveSounds(u16 value)
    {
        MsLock();
        Begin(0x25);
        Push(value);
        MsCommit();
        MsUnlock();
    }

    void MsFreeSound(u32 value)
    {
        MsLock();
        Begin(0x28);
        Push32(value);
        MsCommit();
        MsUnlock();
    }

    void MsReadFile(u32 file, u32 offset, u32 size)
    {
        MsLock();
        Begin(0x29);
        Push32(file);
        Push32(offset);
        Push32(size);
        MsCommit();
        MsUnlock();
    }

    void MsCommand2A(u16 value)
    {
        MsLock();
        Begin(0x2A);
        Push(value);
        MsCommit();
        MsUnlock();
    }

    void MsInterleaveStream(s32 stream, u16 value, u32 second)
    {
        if (stream >= g_MsStreamCount)
        {
            return;
        }

        MsLock();
        Begin(0x2B);
        Push(static_cast<u16>(stream));
        Push(value);
        Push32(second);
        MsCommit();
        MsUnlock();
    }

    void MsQueryFreeMemory()
    {
        MsLock();
        Begin(0x2D);
        MsCommit();
        MsUnlock();
    }

    s32 MsStartPrepared(s32 stream)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        Begin(0x2E);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsPrepareStream(s32 stream)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        Begin(0x2F);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsSetEeDestination(u32 address)
    {
        MsLock();
        g_MsHoldStatus56 = 1;
        Begin(0x31);
        g_MsStatus56 = address;
        Push32(address);
        MsCommit();
        MsUnlock();
    }

    void MsSetStatus57(u32 value)
    {
        MsLock();
        g_MsHoldStatus57 = 1;
        Begin(0x32);
        g_MsStatus57 = value;
        Push32(value);
        MsCommit();
        MsUnlock();
    }

    void MsRestartReading()
    {
        MsLock();
        Begin(0x35);
        MsCommit();
        MsUnlock();
    }

    void MsHoldReading()
    {
        MsLock();
        Begin(0x37);
        MsCommit();
        MsUnlock();
    }

    void MsLendTransfer()
    {
        MsLock();
        Begin(0x3B);
        MsCommit();
        MsUnlock();
    }

    void MsReclaimTransfer()
    {
        MsLock();
        Begin(0x3C);
        MsCommit();
        MsUnlock();
    }

    void MsStopProgress()
    {
        MsLock();
        Begin(0x49);
        Push(0);
        Push(0);
        MsCommit();
        MsUnlock();
    }

    s32 MsSetServerMode(u32 mode)
    {
        if (mode < 2 || mode == 7 || mode == 6)
        {
            if (g_MsServer.running != 1)
            {
                return -1;
            }

            Begin(0x51);
            g_MsServer.unknown18 = static_cast<s32>(mode);
            Push(1);
            Push(static_cast<u16>(mode));
            MsCommit();
            if (mode - 6 >= 2)
            {
                g_MsServer.mode = 4;
            }

            return 0;
        }

        if (mode - 2 < 3)
        {
            g_MsServer.mode = static_cast<s32>(mode);
            return 0;
        }

        return -1;
    }

    // The size goes up to the next 64 bytes, and has to fit the stream's buffer; sizes of 1 to 1023 aren't taken
    s32 MsSetStreamBufferSize(s32 stream, u32 size)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        if (size - 1 < 0x3FF)
        {
            return -2;
        }

        u32 aligned = (size + 0x3F) & ~0x3Fu;
        if (g_MsStreamBufferSizes[stream] == 0 || g_MsStreamBufferCapacities[stream] < aligned)
        {
            return -1;
        }

        MsLock();
        g_MsStreamBufferUsed[stream] = aligned;
        Begin(0x52);
        Push(static_cast<u16>(stream));
        Push32(aligned);
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsSetVoiceRelease(u32 voice, u32 parameter)
    {
        if (parameter >= 0x20)
        {
            return -1;
        }

        if (voice >= Streams)
        {
            return -2;
        }

        MsLock();
        Begin(0x5C);
        s32 index = static_cast<s32>(voice);
        Push(static_cast<u16>((index / 24) | (index % 24) << 1 | 0x400));
        Push(static_cast<u16>(parameter | 0x1FC0));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsCommand63(s32 stream, u32 value)
    {
        if (stream >= g_MsStreamCount || value == 0)
        {
            return -1;
        }

        MsLock();
        Begin(0x63);
        Push(static_cast<u16>(stream));
        Push32(value);
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsCommand103(u16 value)
    {
        MsLock();
        Begin(0x103);
        Push(value);
        MsCommit();
        MsUnlock();
    }

    void MsCommand104()
    {
        MsLock();
        Begin(0x104);
        MsCommit();
        MsUnlock();
    }
}
