#pragma once

#include "common.h"

// The sound processor: voices playing sounds out of its memory or music streamed into it, a reverb per core, volumes by group.
// The values are the PS2's SPU2's: 2 cores of 24 voices (a voice is core * 24 + its number), volumes of 14 bits (0x3FFF full,
// 0x4000 and up the other phase), pitches of 0x1000 for a sample's own rate, the SPU2's 10 reverb modes. Sounds and music are
// read on Platform::Stream's channels
namespace Platform::Audio
{
constexpr s32 Voices = 48;
constexpr s32 VoicesPerCore = 24;
constexpr s32 Cores = 2;
// A voice's group goes with its number in the top half of a word (voice | group << GroupShift)
constexpr u32 GroupShift = 16;
// A voice's full volume (past it the other phase: InvertedVolumeBase less the volume's size), and a group's volume of 1 (4.12
// fixed point)
constexpr s32 MaxVolume = 0x3FFF;
constexpr s32 InvertedVolumeBase = 0x7FFF;
constexpr s16 FullGroupVolume = 0x1000;

// Every voice and setting as when the sound processor started
void Reset();
void PauseAll();
void ResumeAll();
// A movie plays its own sound, the platform lends it what it needs (the PS2's sound DMA channel) and takes it back
s32 LendToMovie();
s32 ReclaimFromMovie();
// A step of moving the sounds in the sound processor's memory together, over the gaps released sounds left (the game's own
// addition to the PS2's sound module, which the game asks for every 36 frames)
void CompactSoundMemory();

// The volume groups 1 to 4 (4.12 fixed point, a group given as its number << GroupShift) the voices' volumes are scaled by
void SetGroupVolume(s32 group, u32 left, u32 right);

// Return 0, -1 for no such voice
s32 SetVoiceVolume(s32 voice, s16 left, s16 right);
s32 SetVoicePitch(s32 voice, u16 pitch);
void SetVoiceReverb(s32 voice, bool on);
// Lets the voice go: it fades out at the release rate (below 0x20)
void ReleaseVoice(s32 voice, u32 rate);
// -1 for no voice
s32 IsVoiceFree(s32 voice);

void SetReverb(s32 core, s32 mode, u16 depthLeft, u16 depthRight, u16 delay, u16 feedback);
void ClearReverb(s32 core);
void SetReverbVolume(s32 core, s16 left, s16 right);

// Takes size bytes of the sound processor's memory from where the sound banks' samples go. Returns where they are
u32 ReserveSoundMemory(u32 size);
// The sound bank's samples are all in the sound processor's memory
void SoundBankLoaded(u16 bank);
// The sound processor's memory keeps so many sounds (a table of them on the PS2's I/O processor)
void ReserveSounds(u16 count);
void ReleaseSound(u32 sound);
// Plays a sound of a bank on a voice (its group in the top half) at the pitch, with the envelope's attack and release rates,
// so many times (0 until it's let go). Returns 0, -1 for no such voice, -2 for more than 0xFFFF times
s32 PlaySound(u32 sound, u32 voiceAndGroup, s16 left, s16 right, u16 pitch, u32 attack, u32 release, u32 loops);

// Music: a channel streams a file's samples into a voice. Interleaved music has a second channel on a second voice
// The rate's pitch
s32 PitchOfRate(s32 rate);
// Where the channel's buffer is in the I/O processor's memory (the music keeps it)
u32 ChannelBufferAddress(s32 channel);
// Reads size bytes of the file from offset for the next music started
void ReadMusic(s32 file, u32 offset, u32 size);
// Starts streaming it on the channel into the voice, at the pitch, looping or not
s32 StreamMusic(s32 file, s32 channel, u32 voiceAndGroup, u16 pitch, bool once);
// The channel's music is interleaved in blocks of blockSize bytes, the second channel (the child, which reads nothing itself)
// plays the next of them on its voice from the sound processor's memory at soundAddress. Its end is where the music's data
// ends in the last block (the I/O processor looks for it otherwise)
void InterleaveMusic(s32 channel, u32 blockSize);
void AddMusicChannel(s32 child, s32 parent, u32 voiceAndGroup, u32 soundAddress);
void SetMusicEnd(s32 channel, u32 end);
// Gets the channel's music ready, then plays it
void PrepareMusic(s32 channel);
bool IsMusicReady(s32 channel);
s32 PlayMusic(s32 channel);
bool IsMusicPlaying(s32 channel);
void StopMusic(s32 channel);
}
