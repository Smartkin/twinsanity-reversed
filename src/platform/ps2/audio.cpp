#include "platform/audio.h"

#include "multistream/multistream.h"

// The SPU2 through MultiStream's commands (multistream.h): the voices are its channels, the music's channels its streams. What
// the module does with them is from STREAM.IRX: the pitch is SD_VP_PITCH, the release SD_VP_ADSR2, the effect send
// SD_S_VMIXEL/R, the reverb's volume SD_P_EVOLL/R, and the reset starts libsd over
using namespace MultiStream;

namespace
{
// Interleaved music's tracks: the first channel's (it reads the file) and the second's
constexpr u16 FirstTrack = 0;
constexpr u16 SecondTrack = 1;
}

void Platform::Audio::Reset()
{
    MsResetSound();
}

void Platform::Audio::PauseAll()
{
    MsPauseAll();
}

void Platform::Audio::ResumeAll()
{
    MsResumeAll();
}

s32 Platform::Audio::LendToMovie()
{
    MsDisableSpuCallback();
    return MsSendBatch(SendWait);
}

s32 Platform::Audio::ReclaimFromMovie()
{
    MsEnableSpuCallback();
    return MsSendBatch(SendWait);
}

void Platform::Audio::CompactSoundMemory()
{
    MsCompactSoundMemory();
}

void Platform::Audio::SetGroupVolume(s32 group, u32 left, u32 right)
{
    MsSetGroupVolume(group, left, right);
}

s32 Platform::Audio::SetVoiceVolume(s32 voice, s16 left, s16 right)
{
    return MsSetChannelVolume(static_cast<s16>(voice), left, right);
}

s32 Platform::Audio::SetVoicePitch(s32 voice, u16 pitch)
{
    return MsSetChannelPitch(static_cast<u32>(voice), pitch);
}

void Platform::Audio::SetVoiceReverb(s32 voice, bool on)
{
    if (on)
    {
        MsEffectSendOn(static_cast<u16>(voice));
    }
    else
    {
        MsEffectSendOff(static_cast<u16>(voice));
    }
}

void Platform::Audio::ReleaseVoice(s32 voice, u32 rate)
{
    MsSetChannelRelease(static_cast<u32>(voice), rate);
    MsKeyOff(static_cast<u32>(voice));
}

s32 Platform::Audio::IsVoiceFree(s32 voice)
{
    return MsIsChannelFree(static_cast<u32>(voice));
}

void Platform::Audio::SetReverb(s32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback)
{
    MsSetEffect(static_cast<u32>(core), mode, depthLeft, depthRight, delay, feedback);
}

void Platform::Audio::ClearReverb(s32 core)
{
    MsClearEffect(core);
}

void Platform::Audio::SetReverbVolume(s32 core, s16 left, s16 right)
{
    MsSetEffectVolume(static_cast<u16>(core), static_cast<u16>(left), static_cast<u16>(right));
}

u32 Platform::Audio::ReserveSoundMemory(u32 size)
{
    u32 location = g_MsSoundBankAddress;
    g_MsSoundBankAddress = location + size;
    return location;
}

void Platform::Audio::SoundBankLoaded(u16 bank)
{
    MsSoundBankLoaded(bank);
}

void Platform::Audio::ReserveSounds(u16 count)
{
    MsReserveSounds(count);
}

void Platform::Audio::ReleaseSound(u32 sound)
{
    MsFreeSound(sound);
}

s32 Platform::Audio::PlaySound(u32 sound, u32 voiceAndGroup, s16 left, s16 right, u16 pitch, u32 attack, u32 release, u32 loops)
{
    return MsPlaySound(sound, voiceAndGroup, left, right, pitch, attack, release, loops);
}

s32 Platform::Audio::PitchOfRate(s32 rate)
{
    return MsPitchOfRate(rate);
}

u32 Platform::Audio::ChannelBufferAddress(s32 channel)
{
    MsSend(SendWait);
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.iopBuffer;
}

void Platform::Audio::ReadMusic(s32 file, u32 offset, u32 size)
{
    MsReadFile(static_cast<u32>(file), offset, size);
}

s32 Platform::Audio::StreamMusic(s32 file, s32 channel, u32 voiceAndGroup, u16 pitch, bool once)
{
    return MsPlayStream(static_cast<u32>(file), channel, voiceAndGroup, 0, 0, pitch, once ? StreamOnce : StreamLooping, 0, 0);
}

void Platform::Audio::InterleaveMusic(s32 channel, u32 blockSize)
{
    MsInterleaveStream(channel, FirstTrack, blockSize);
}

void Platform::Audio::AddMusicChannel(s32 child, s32 parent, u32 voiceAndGroup, u32 soundAddress)
{
    MsAddStreamChannel(child, static_cast<u32>(parent), voiceAndGroup, SecondTrack, 0, 0, soundAddress);
}

void Platform::Audio::SetMusicEnd(s32 channel, u32 end)
{
    MsSetMibEndOffset(channel, end);
}

// The stream preloads without starting, its data in when it's active
void Platform::Audio::PrepareMusic(s32 channel)
{
    MsDisableKeyOn(channel);
}

bool Platform::Audio::IsMusicReady(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.active != 0;
}

s32 Platform::Audio::PlayMusic(s32 channel)
{
    return MsAllowKeyOn(channel);
}

bool Platform::Audio::IsMusicPlaying(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.state != StreamOff;
}

void Platform::Audio::StopMusic(s32 channel)
{
    MsStopStream(channel);
}
