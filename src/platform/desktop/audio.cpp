#include "platform/audio.h"

// The desktop's side of Platform::Audio: no sound yet
namespace Platform::Audio
{
void Reset()
{
}

void PauseAll()
{
}

void ResumeAll()
{
}

s32 LendToMovie()
{
    return 0;
}

s32 ReclaimFromMovie()
{
    return 0;
}

void CompactSoundMemory()
{
}

void SetGroupVolume(s32 group, u32 left, u32 right)
{
}

s32 SetVoiceVolume(s32 voice, s16 left, s16 right)
{
    return 0;
}

s32 SetVoicePitch(s32 voice, u16 pitch)
{
    return 0;
}

void SetVoiceReverb(s32 voice, bool on)
{
}

void ReleaseVoice(s32 voice, u32 rate)
{
}

s32 IsVoiceFree(s32 voice)
{
    return 0;
}

void SetReverb(s32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback)
{
}

void ClearReverb(s32 core)
{
}

void SetReverbVolume(s32 core, s16 left, s16 right)
{
}

u32 ReserveSoundMemory(u32 size)
{
    return 0;
}

void SoundBankLoaded(u16 bank)
{
}

void ReserveSounds(u16 count)
{
}

void ReleaseSound(u32 sound)
{
}

s32 PlaySound(u32 sound, u32 voiceAndGroup, s16 left, s16 right, u16 pitch, u32 attack, u32 release, u32 loops)
{
    return 0;
}

s32 PitchOfRate(s32 rate)
{
    return 0;
}

u32 ChannelBufferAddress(s32 channel)
{
    return 0;
}

void ReadMusic(s32 file, u32 offset, u32 size)
{
}

s32 StreamMusic(s32 file, s32 channel, u32 voiceAndGroup, u16 pitch, bool once)
{
    return 0;
}

void InterleaveMusic(s32 channel, u32 blockSize)
{
}

void AddMusicChannel(s32 child, s32 parent, u32 voiceAndGroup, u32 soundAddress)
{
}

void SetMusicEnd(s32 channel, u32 end)
{
}

void PrepareMusic(s32 channel)
{
}

bool IsMusicReady(s32 channel)
{
    return false;
}

s32 PlayMusic(s32 channel)
{
    return 0;
}

bool IsMusicPlaying(s32 channel)
{
    return false;
}

void StopMusic(s32 channel)
{
}

}
