#include "multistream.h"

// The commands with nothing more to them than their words, and the channels' volumes by their groups
using namespace MultiStream;

namespace
{
// libsd's voice attribute word (SOUND_SetParam's): the core, the voice and the parameter
union SdVoiceAttribute
{
    u16 value;
    struct
    {
        u16 core : 1;
        u16 voice : 5;
        u16 unused6 : 2;
        u16 parameter : 8;
    };
};
CHECK_SIZE(SdVoiceAttribute, 2);

// SD_VP_ADSR2's parameter: the release rate and its mode, the sustain's rate, direction and mode
constexpr u16 SdAdsr2 = 4;

union Adsr2
{
    u16 value;
    struct
    {
        u16 releaseRate : 5;
        u16 releaseExponential : 1;
        u16 sustainRate : 7;
        u16 unused13 : 1;
        u16 sustainDecreasing : 1;
        u16 sustainExponential : 1;
    };
};
CHECK_SIZE(Adsr2, 2);

// The release rate MsSetChannelRelease takes is below it; the sustain it sets is its slowest
constexpr u32 ReleaseRateLimit = 0x20;
constexpr u16 SlowestSustain = 0x7F;

// The SPU2's buffers are sized in blocks of 64 bytes, of 1 KB at least
constexpr u32 SpuBufferAlignment = 0x40;
constexpr u32 MinimumSpuBuffer = 0x400;
// The SPU2's reverb modes
constexpr s32 EffectModes = 10;
// A pitch of 0x1000 plays 48 kHz
constexpr s32 PitchShift = 12;
constexpr s32 PitchRate = 48000;

// A channel's volume scaled by its group's (4.12 fixed point). Volumes past MaxVolume are the SPU2's inverted phase, scaled
// from the other end
s32 ScaleVolume(s32 groupVolume, s32 volume)
{
    bool inverted = volume > Platform::Audio::MaxVolume;
    if (inverted)
    {
        volume = Platform::Audio::InvertedVolumeBase - volume;
    }

    s32 scaled = volume * groupVolume;
    if (scaled < 0)
    {
        scaled += (1 << GroupVolumeShift) - 1;
    }

    scaled >>= GroupVolumeShift;
    return inverted ? Platform::Audio::InvertedVolumeBase - scaled : scaled;
}

s32 GroupIndex(s32 group)
{
    return (group >> Platform::Audio::GroupShift) - 1;
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
        s32 index = GroupIndex(group);
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
            s32 index = GroupIndex(group);
            g_MsLeftVolume = MsGroupScale(g_MsGroupVolumesLeft[index], left);
            rightVolume = MsGroupScale(g_MsGroupVolumesRight[index], g_MsChannelVolumesRight[channel]);
            leftVolume = g_MsLeftVolume;
        }

        g_MsRightVolume = rightVolume;
        g_MsLeftVolume = leftVolume;
    }

    s32 MsSetGroupVolume(s32 group, u32 left, u32 right)
    {
        u32 index = static_cast<u32>(group) >> Platform::Audio::GroupShift;
        if (index > Groups || group == 0)
        {
            return -1;
        }

        if (left > Platform::Audio::FullGroupVolume)
        {
            return -2;
        }

        if (right > Platform::Audio::FullGroupVolume)
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
        for (s32 channel = 0; channel < static_cast<s32>(Channels); channel++)
        {
            if (g_MsChannelGroups[channel] != group || g_MsChannelStates[channel] == ChannelOff)
            {
                continue;
            }

            MsLock();
            MsComputeChannelVolumes(group, channel);
            Begin(OpSetVolume);
            Push(static_cast<u16>(channel));
            Push(static_cast<u16>(g_MsLeftVolume));
            Push(static_cast<u16>(g_MsRightVolume));
            MsCommit();
            MsUnlock();
        }
    }

    s32 MsSetChannelVolume(s16 channel, s16 left, s16 right)
    {
        if (static_cast<u16>(channel) >= Channels)
        {
            return -1;
        }

        MsStoreChannelVolumes(channel, left, right);
        MsLock();
        Begin(OpSetVolumeSmooth);
        Push(static_cast<u16>(channel));
        Push(static_cast<u16>(g_MsLeftVolume));
        Push(static_cast<u16>(g_MsRightVolume));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsNextFileId()
    {
        s32 file = g_MsFileIdCounter;
        g_MsFileIdCounter++;
        if (g_MsFileIdCounter == -1)
        {
            g_MsFileIdCounter = 0;
        }

        return file;
    }

    s32 MsPitchOfRate(s32 rate)
    {
        return (rate << PitchShift) / PitchRate;
    }

    s32 MsStreamOnChannel(u32 channel)
    {
        if (channel >= Channels)
        {
            return -2;
        }

        return static_cast<s8>(g_MsChannelStreams[channel]);
    }

    s32 MsSetChannelPitch(u32 channel, u16 pitch)
    {
        if (channel >= Channels)
        {
            return -1;
        }

        MsLock();
        Begin(OpSetPitch);
        Push(static_cast<u16>(channel));
        Push(pitch);
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsKeyOff(u32 channel)
    {
        if (channel >= Channels)
        {
            return -1;
        }

        MsLock();
        Begin(OpStopSound);
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
        if (stream >= ChannelStreams)
        {
            u32 channel = static_cast<u32>(stream - ChannelStreams);
            s32 playing = MsStreamOnChannel(channel);
            if (playing < 0)
            {
                MsUnlock();
                return -1;
            }

            g_MsChannelStreams[channel] = NoStream;
            stream = playing;
        }

        g_MsStreamStates[stream] = StreamStopRequested;
        Begin(OpStopStream);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsResetSound()
    {
        MsLock();
        Begin(OpInitSpu);
        MsCommit();
        MsUnlock();
    }

    void MsPauseAll()
    {
        MsLock();
        Begin(OpPause);
        MsCommit();
        MsUnlock();
    }

    void MsResumeAll()
    {
        MsLock();
        Begin(OpResume);
        MsCommit();
        MsUnlock();
    }

    void MsInitStreamData(u16 loadType, u16 maxFiles, u16 maxSounds)
    {
        MsLock();
        Begin(OpInitStreamData);
        Push(loadType);
        Push(maxFiles);
        Push(maxSounds);
        Push(static_cast<u16>(g_MsIopThreadPriority));
        MsCommit();
        g_MsStreamDataInitialised = 1;
        MsUnlock();
    }

    s32 MsAllocateStreamBuffer(s32 stream, u32 spuAddress, u32 size)
    {
        if (stream >= static_cast<s32>(Streams))
        {
            return -1;
        }

        MsLock();
        g_MsIopBufferSizes[stream] = size;
        g_MsIopBufferCurrentSizes[stream] = size;
        g_MsSpuBufferSizes[stream] = size;
        if (spuAddress == DataStream)
        {
            spuAddress = 0;
            g_MsStreamIsData[stream] = 1;
        }
        else
        {
            g_MsStreamIsData[stream] = 0;
        }

        Begin(OpAllocateStreamBuffer);
        Push(static_cast<u16>(stream));
        Push32(spuAddress);
        Push32(size);
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsSetSpuWriteAddress(u32 address)
    {
        MsLock();
        g_MsSpuWriteAddressSet = 1;
        g_MsSpuWriteAddress = address;
        Begin(OpSetSpuWriteAddress);
        Push32(address);
        MsCommit();
        MsUnlock();
    }

    s32 MsCloseStreamBuffer(u32 stream)
    {
        if (stream >= Streams)
        {
            return -1;
        }

        MsLock();
        g_MsIopBufferSizes[stream] = 0;
        g_MsIopBufferCurrentSizes[stream] = 0;
        g_MsSpuBufferSizes[stream] = 0;
        g_MsStreamIsData[stream] = 0;
        Begin(OpCloseStreamBuffer);
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
        Begin(OpSetMaxStreamLimit);
        Push(static_cast<u16>(count));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsSetEffect(u32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback)
    {
        if (core >= Platform::Audio::Cores || mode >= EffectModes)
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
        Begin(OpEnableEffect);
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
        Begin(OpDisableEffect);
        Push(static_cast<u16>(core));
        MsCommit();
        MsUnlock();
    }

    void MsSetEffectVolume(u16 core, u16 left, u16 right)
    {
        MsLock();
        Begin(OpSetEffectVolume);
        Push(core);
        Push(left);
        Push(right);
        MsCommit();
        MsUnlock();
    }

    void MsEffectSendOn(u16 channel)
    {
        MsLock();
        Begin(OpEffectChannelOn);
        Push(channel);
        MsCommit();
        MsUnlock();
    }

    void MsEffectSendOff(u16 channel)
    {
        MsLock();
        Begin(OpEffectChannelOff);
        Push(channel);
        MsCommit();
        MsUnlock();
    }

    s32 MsSetBankAddress(u32 bank, u32 address)
    {
        MsLock();
        Begin(OpPatchSoundBank);
        Push32(bank);
        Push32(address);
        MsCommit();
        MsUnlock();
        return MsCheckSoundId(bank, address);
    }

    void MsCloseFile(u32 file)
    {
        MsLock();
        Begin(OpFreeFileId);
        Push32(file);
        MsCommit();
        MsUnlock();
    }

    void MsReserveSounds(u16 count)
    {
        MsLock();
        Begin(OpAllocateSoundTable);
        Push(count);
        MsCommit();
        MsUnlock();
    }

    void MsFreeSound(u32 sound)
    {
        MsLock();
        Begin(OpFreeSound);
        Push32(sound);
        MsCommit();
        MsUnlock();
    }

    void MsReadFile(u32 file, u32 offset, u32 size)
    {
        MsLock();
        Begin(OpSetFileOffsetAndSize);
        Push32(file);
        Push32(offset);
        Push32(size);
        MsCommit();
        MsUnlock();
    }

    void MsInitDisc(u16 type)
    {
        MsLock();
        Begin(OpInitDisc);
        Push(type);
        MsCommit();
        MsUnlock();
    }

    void MsInterleaveStream(s32 stream, u16 track, u32 trackSize)
    {
        if (stream >= g_MsStreamCount)
        {
            return;
        }

        MsLock();
        Begin(OpSetStreamParent);
        Push(static_cast<u16>(stream));
        Push(track);
        Push32(trackSize);
        MsCommit();
        MsUnlock();
    }

    void MsQueryFreeMemory()
    {
        MsLock();
        Begin(OpGetMaxIopMemory);
        MsCommit();
        MsUnlock();
    }

    s32 MsAllowKeyOn(s32 stream)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        Begin(OpAllowKeyOn);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsDisableKeyOn(s32 stream)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        MsLock();
        Begin(OpDisableKeyOn);
        Push(static_cast<u16>(stream));
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsSetEeWriteAddress(u32 address)
    {
        MsLock();
        g_MsEeWriteAddressSet = 1;
        Begin(OpSetEeWriteAddress);
        g_MsEeWriteAddress = address;
        Push32(address);
        MsCommit();
        MsUnlock();
    }

    void MsSetIopWriteAddress(u32 address)
    {
        MsLock();
        g_MsIopWriteAddressSet = 1;
        Begin(OpSetIopWriteAddress);
        g_MsIopWriteAddress = address;
        Push32(address);
        MsCommit();
        MsUnlock();
    }

    void MsRestartFromDiscError()
    {
        MsLock();
        Begin(OpRestartFromDiscError);
        MsCommit();
        MsUnlock();
    }

    void MsCheckDiscError()
    {
        MsLock();
        Begin(OpCheckDiscError);
        MsCommit();
        MsUnlock();
    }

    void MsDisableSpuCallback()
    {
        MsLock();
        Begin(OpDisableSpuCallback);
        MsCommit();
        MsUnlock();
    }

    void MsEnableSpuCallback()
    {
        MsLock();
        Begin(OpEnableSpuCallback);
        MsCommit();
        MsUnlock();
    }

    void MsCloseWaitUpdate()
    {
        MsLock();
        Begin(OpInitWait);
        Push(0);
        Push(0);
        MsCommit();
        MsUnlock();
    }

    s32 MsSetFastLoadMode(u32 mode)
    {
        if (mode == FastLoadOff || mode == FastLoadOn || mode == FastLoadInvalidateCacheOn || mode == FastLoadInvalidateCacheOff)
        {
            if (g_MsFastLoad.eeStatus != FastLoadOn)
            {
                return -1;
            }

            Begin(OpFastLoad);
            g_MsFastLoad.iopStatus = static_cast<s32>(mode);
            Push(FastLoadOnOff);
            Push(static_cast<u16>(mode));
            MsCommit();
            if (mode != FastLoadInvalidateCacheOff && mode != FastLoadInvalidateCacheOn)
            {
                g_MsFastLoad.allowLoad = FastLoadContinue;
            }

            return 0;
        }

        if (mode >= FastLoadStop && mode <= FastLoadContinue)
        {
            g_MsFastLoad.allowLoad = static_cast<s32>(mode);
            return 0;
        }

        return -1;
    }

    // The size goes up to the next 64 bytes, and has to fit the stream's IOP buffer; sizes of 1 to 1023 aren't taken
    s32 MsResizeSpuBuffer(s32 stream, u32 size)
    {
        if (stream >= g_MsStreamCount)
        {
            return -1;
        }

        if (size != 0 && size < MinimumSpuBuffer)
        {
            return -2;
        }

        u32 aligned = (size + SpuBufferAlignment - 1) & ~(SpuBufferAlignment - 1);
        if (g_MsIopBufferSizes[stream] == 0 || g_MsIopBufferCurrentSizes[stream] < aligned)
        {
            return -1;
        }

        MsLock();
        g_MsSpuBufferSizes[stream] = aligned;
        Begin(OpResizeSpuBuffer);
        Push(static_cast<u16>(stream));
        Push32(aligned);
        MsCommit();
        MsUnlock();
        return 0;
    }

    // The channel's ADSR2: the release rate, and the slowest sustain
    s32 MsSetChannelRelease(u32 channel, u32 rate)
    {
        if (rate >= ReleaseRateLimit)
        {
            return -1;
        }

        if (channel >= Channels)
        {
            return -2;
        }

        MsLock();
        Begin(OpSetParameter);
        s32 index = static_cast<s32>(channel);
        SdVoiceAttribute attribute;
        attribute.value = 0;
        attribute.core = index / Platform::Audio::VoicesPerCore;
        attribute.voice = index % Platform::Audio::VoicesPerCore;
        attribute.parameter = SdAdsr2;
        Push(attribute.value);
        Adsr2 envelope;
        envelope.value = 0;
        envelope.releaseRate = rate;
        envelope.sustainRate = SlowestSustain;
        Push(envelope.value);
        MsCommit();
        MsUnlock();
        return 0;
    }

    s32 MsSetMibEndOffset(s32 stream, u32 offset)
    {
        if (stream >= g_MsStreamCount || offset == 0)
        {
            return -1;
        }

        MsLock();
        Begin(OpSetMibEnd);
        Push(static_cast<u16>(stream));
        Push32(offset);
        MsCommit();
        MsUnlock();
        return 0;
    }

    void MsSoundBankLoaded(u16 bank)
    {
        MsLock();
        Begin(OpSoundBankLoaded);
        Push(bank);
        MsCommit();
        MsUnlock();
    }

    void MsCompactSoundMemory()
    {
        MsLock();
        Begin(OpCompactSoundMemory);
        MsCommit();
        MsUnlock();
    }
}
