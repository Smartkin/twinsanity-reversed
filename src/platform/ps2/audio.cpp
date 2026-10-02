#include "platform/audio.h"

#include "multistream/multistream.h"

// The SPU2 through MultiStream's commands. What the module does with them is from STREAM.IRX: op 3 sets SD_VP_PITCH, 4 keys
// the voice off, 0x5C sets SD_VP_ADSR2 (here to 0x1FC0 and the release rate), 0x18 and 0x19 switch the voice's effect send
// (SD_S_VMIXEL/R), 0x15 sets the core's SD_P_EVOLL/R, 7 starts libsd over, 0x25 and 0x28 keep and free the table of sounds,
// 0x3B and 0x3C take the transfer handler of DMA channel 1 away and put it back
using namespace MultiStream;

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
    MsLendTransfer();
    return MsSendBatch(0);
}

s32 Platform::Audio::ReclaimFromMovie()
{
    MsReclaimTransfer();
    return MsSendBatch(0);
}

void Platform::Audio::KeepAlive()
{
    MsCommand104();
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
    return MsSetVoicePitch(static_cast<u32>(voice), pitch);
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
    MsSetVoiceRelease(static_cast<u32>(voice), rate);
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
    MsCommand103(bank);
}

void Platform::Audio::ReserveSounds(u16 count)
{
    MsReserveSounds(count);
}

void Platform::Audio::ReleaseSound(u32 sound)
{
    MsFreeSound(sound);
}

s32 Platform::Audio::PlaySound(u32 sound, u32 voiceAndGroup, s16 left, s16 right, u16 unknown, u32 lowByte, u32 highByte,
                               u32 last)
{
    return MsPlaySound(sound, voiceAndGroup, left, right, unknown, lowByte, highByte, last);
}

s32 Platform::Audio::PitchOfRate(s32 rate)
{
    return MsPitchOfRate(rate);
}

u32 Platform::Audio::ChannelValue(s32 channel)
{
    MsSend(0);
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.value;
}

void Platform::Audio::ReadMusic(s32 file, u32 offset, u32 size)
{
    MsReadFile(static_cast<u32>(file), offset, size);
}

s32 Platform::Audio::StreamMusic(s32 file, s32 channel, u32 voiceAndGroup, u16 pitch, bool once)
{
    return MsPlayStream(static_cast<u32>(file), channel, voiceAndGroup, 0, 0, pitch, once ? 1 : 0, 0, 0);
}

void Platform::Audio::InterleaveMusic(s32 channel, u32 blockSize)
{
    MsInterleaveStream(channel, 0, blockSize);
}

void Platform::Audio::AddMusicChannel(s32 second, s32 channel, u32 voiceAndGroup, u32 location)
{
    MsAddStreamChannel(second, static_cast<u32>(channel), voiceAndGroup, 1, 0, 0, location);
}

void Platform::Audio::SetMusicValue(s32 channel, u32 value)
{
    MsCommand63(channel, value);
}

void Platform::Audio::PrepareMusic(s32 channel)
{
    MsPrepareStream(channel);
}

bool Platform::Audio::IsMusicReady(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.word3Top != 0;
}

s32 Platform::Audio::PlayMusic(s32 channel)
{
    return MsStartPrepared(channel);
}

bool Platform::Audio::IsMusicPlaying(s32 channel)
{
    StreamStatus status;
    MsGetStreamStatus(static_cast<u8>(channel), &status);
    return status.state != 0;
}

void Platform::Audio::StopMusic(s32 channel)
{
    MsStopStream(channel);
}
