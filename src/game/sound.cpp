#include "game/sound.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/filestream.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/language.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/movie.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/resources.h"
#include "game/stream.h"
#include "platform/audio.h"
#include "retail/libc.h"

// The sound code's side of the sound processor: voices, reverbs, groups and music
namespace
{
constexpr f32 VoiceVolumeScale = 16383.0f;
constexpr f32 ReverbVolumeScale = 32767.0f;
constexpr f32 GroupVolumeScale = 4096.0f;
// A voice lets go at this release rate, 10 frames after it was faded out
constexpr u32 ReleaseRate = 0xD;
constexpr s32 ReleaseFrames = 10;
// A sound's parameter goes out with this value in its high byte
constexpr u32 SoundHighByte = 0xD;
constexpr u32 MusicBufferSize = 0x8000;
constexpr u32 MusicSecondBuffer = 0x4000;
constexpr s32 KeepAliveFrames = 36;

// Volumes below 0 are the other phase's
s32 Phase(s32 volume)
{
    return volume < 0 ? volume + 0x7FFF : volume;
}

s32 VoiceVolume(f32 volume)
{
    return Phase(static_cast<s32>(volume * VoiceVolumeScale));
}

u32 GroupOf(u32 bits)
{
    return SoundGroupOf(static_cast<s32>(bits >> 6 & 7));
}

u32 VoiceCount(const MusicPlayer* player)
{
    return (player->bits & MusicPlayer::Interleaved) != 0 ? 2 : 1;
}

void SetMusicState(MusicPlayer* player, u32 state)
{
    player->bits = (player->bits & ~MusicPlayer::StateMask) | state << 1;
}

u32 MusicState(const MusicPlayer* player)
{
    return player->bits >> 1 & 0x1F;
}

f32* GroupScales();

}

extern "C"
{
    // The music request the sound's update plays when one is pending
    extern const MusicRequest D_002E7918;

}

extern "C" u32 PlaySound(f32 volume, f32 pitchScale, GameSound* sound, s32 group, s32 voiceKind, s32 last)
{
    if ((sound->flags & 1) == 0)
    {
        return 0;
    }

    SoundVoice* voice = FindFreeVoice(voiceKind, 0);
    if (voice == nullptr)
    {
        return 0;
    }

    if (PlaySoundOnVoice(volume, pitchScale, voice, sound, SoundGroupOf(group), last) == -1)
    {
        ReleaseVoice(voice);
        return 0;
    }

    SetVoiceReverb(voice, voiceKind);
    return 1;
}

extern "C" void FadeSoundGroups(f32 time)
{
    g_GroupFadeTime = time;
    g_GroupFade = 1.0f;
}

extern "C" void SetMusicBank(const char* name)
{
    LoadSoundBank(&g_MusicBank, name);
    for (MusicPlayer& player : g_Music->players)
    {
        CreateMusicBuffers(&player, &g_MusicBank);
    }
}

extern "C" void SetVoiceBank(const char* name)
{
    LoadSoundBank(&g_VoiceBank, name);
}

extern "C" u32 PlaySoundById(f32 volume, f32 pitchScale, u32 id, s32 group, s32 voiceKind, s32 last)
{
    GameSound* sound = SoundById(static_cast<u16>(id));
    if (sound == nullptr)
    {
        return 0;
    }

    return PlaySound(volume, pitchScale, sound, group, voiceKind, last);
}

EABI_EXPORT(FUN_001e5d98, PlaySound);
EABI_EXPORT(FUN_001e5d10, PlaySoundById);
EABI_EXPORT(FUN_001e0fd0, FadeMusic);
EABI_EXPORT(FUN_001e6c20, FadeMusicOut);
EABI_EXPORT(FUN_001e6e30, FadeMusicTo);
EABI_EXPORT(FUN_001e60b0, SetReverbVolume);
EABI_EXPORT(FUN_001e6108, SetReverbDepth);
EABI_EXPORT(FUN_001e64f0, SetVoiceVolume);
EABI_EXPORT(FUN_001dfa38, PlaySoundOnVoice);
EABI_EXPORT(FUN_001dfbc0, PlaySoundAt);
EABI_EXPORT(FUN_001e6778, SetGroupVolume);
EABI_EXPORT(FUN_001e6fc0, PlayMusic);
EABI_EXPORT(FUN_001e6e78, SetMusicPitch);
EABI_EXPORT(FUN_001e14b8, SetMusicVolume);
EABI_EXPORT(FUN_001e1730, UpdateMusic);

namespace
{
f32* GroupScales()
{
    return reinterpret_cast<f32*>(&G_AlphaRegPresets[0xE]);
}

void ApplyGroupVolumes()
{
    for (u32 group = 0; group < 4; group++)
    {
        Platform::Audio::SetGroupVolume(static_cast<s32>(SoundGroupOf(static_cast<s32>(group))),
                                        static_cast<u16>(g_GroupVolumes[group].left),
                                        static_cast<u16>(g_GroupVolumes[group].right));
    }
}

void PlayOnVoice(SoundVoice* voice, GameSound* sound, u32 group, s32 last)
{
    Platform::Audio::PlaySound(sound->soundId, static_cast<u32>(VoiceNumber(voice)) | group, static_cast<s16>(voice->left),
                               static_cast<s16>(voice->right), static_cast<u16>(voice->pitch), sound->parameter1,
                               SoundHighByte, static_cast<u32>(last == -1 ? 1 : last));
    voice->frames = 0;
}
}

extern "C"
{
    s32 VoiceNumber(const SoundVoice* voice)
    {
        return static_cast<s32>((voice->bits >> 7 & 1) * Platform::Audio::VoicesPerCore + (voice->bits & SoundVoice::NumberMask));
    }

    u32 SoundGroupOf(s32 group)
    {
        switch (group)
        {
        case 1:
            return 0x20000;
        case 2:
            return 0x30000;
        case 3:
            return 0x40000;
        default:
            return 0x10000;
        }
    }

    void SetReverbVolume(f32 left, f32 right, SoundCore* core)
    {
        s32 rightVolume = static_cast<s32>(right * ReverbVolumeScale);
        s32 leftVolume = static_cast<s32>(left * ReverbVolumeScale);
        core->reverbVolumeRight = rightVolume;
        core->reverbVolumeLeft = leftVolume;
        Platform::Audio::SetReverbVolume(static_cast<u16>(core->reverbBits), static_cast<s16>(leftVolume),
                                         static_cast<s16>(rightVolume));
    }

    void SetReverbDepth(f32 left, f32 right, SoundCore* core)
    {
        core->reverbDepthRight = static_cast<s32>(right * ReverbVolumeScale);
        core->reverbDepthLeft = static_cast<s32>(left * ReverbVolumeScale);
    }

    void SetReverb(SoundCore* core, s32 mode, u32 delay, u32 feedback)
    {
        core->reverbMode = mode;
        u16 index = static_cast<u16>(core->reverbBits);
        if (mode == 0)
        {
            Platform::Audio::ClearReverb(index);
            return;
        }

        Platform::Audio::SetReverb(index, mode, static_cast<u16>(core->reverbDepthLeft), static_cast<u16>(core->reverbDepthRight),
                                   static_cast<u16>(delay), static_cast<u16>(feedback));
        core->unknown18 = 0;
        core->reverbBits = (core->reverbBits & 0xC000FFFF) | (delay & 0x7F) << 16 | (feedback & 0x7F) << 23;
    }

    void SetReverbFromSettings(SoundCore* core, const u8* settings)
    {
        // The SPU2's modes: the settings' 0 is its 9
        s32 mode;
        switch (settings[0])
        {
        case 0:
            mode = 9;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            mode = settings[0];
            break;
        default:
            mode = 0;
            break;
        }

        if (mode == 0)
        {
            return;
        }

        const f32* values = reinterpret_cast<const f32*>(settings + 4);
        s32 feedback = static_cast<s32>(values[1] * 127.0f);
        s32 depth = static_cast<s32>(values[2] * ReverbVolumeScale);
        s32 delay = static_cast<s32>(values[0] * 127.0f);
        core->reverbMode = mode;
        core->reverbDepthLeft = depth;
        core->reverbDepthRight = depth;
        Platform::Audio::SetReverb(static_cast<u16>(core->reverbBits), mode, static_cast<u16>(depth), static_cast<u16>(depth),
                                   static_cast<u16>(delay), static_cast<u16>(feedback));
        core->unknown18 = 0;
        core->reverbBits = (core->reverbBits & 0xC000FFFF) | (static_cast<u32>(delay) & 0x7F) << 16 |
                           (static_cast<u32>(feedback) & 0x7F) << 23;
    }

    void ClearReverbs()
    {
        Platform::Audio::ClearReverb(0);
        Platform::Audio::ClearReverb(1);
    }

    void RestoreReverbs()
    {
        for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
        {
            if (core->reverbMode != 0)
            {
                Platform::Audio::SetReverb(static_cast<u16>(core->reverbBits), core->reverbMode,
                                           static_cast<u16>(core->reverbDepthLeft), static_cast<u16>(core->reverbDepthRight),
                                           static_cast<u16>(core->reverbBits >> 16 & 0x7F),
                                           static_cast<u16>(core->reverbBits >> 23 & 0x7F));
            }
        }
    }

    u32 UpdateVoice(SoundVoice* voice)
    {
        u32 bits = voice->bits;
        s32 frames = voice->frames + 1;
        voice->frames = frames;
        s32 number = static_cast<s32>((bits >> 7 & 1) * Platform::Audio::VoicesPerCore + (bits & SoundVoice::NumberMask));
        if ((bits & SoundVoice::Released) != 0 && frames > ReleaseFrames)
        {
            Platform::Audio::ReleaseVoice(number, ReleaseRate);
            voice->bits &= ~SoundVoice::Released;
        }

        if (Platform::Audio::IsVoiceFree(number) != 0)
        {
            voice->bits &= ~(SoundVoice::Playing | SoundVoice::UseMask);
        }

        return voice->bits >> 6 & 1;
    }

    s32 SetVoicePitch(SoundVoice* voice, s32 pitch)
    {
        f32 scaled = g_PitchScale * static_cast<f32>(pitch);
        voice->pitch = pitch;
        return Platform::Audio::SetVoicePitch(VoiceNumber(voice), static_cast<u16>(static_cast<s32>(scaled)));
    }

    s32 SetVoiceVolume(f32 left, f32 right, SoundVoice* voice)
    {
        s32 rightVolume = VoiceVolume(right);
        s32 leftVolume = VoiceVolume(left);
        voice->left = leftVolume;
        voice->right = rightVolume;
        return Platform::Audio::SetVoiceVolume(VoiceNumber(voice), static_cast<s16>(leftVolume), static_cast<s16>(rightVolume));
    }

    void MuteVoice(SoundVoice* voice)
    {
        voice->bits &= ~SoundVoice::TakenByMusic;
        SetVoiceVolume(0.0f, 0.0f, voice);
        voice->bits |= SoundVoice::Released;
    }

    void SetVoiceReverb(SoundVoice* voice, s32 on)
    {
        u32 send = on != 0 ? 1 : 0;
        u32 bits = voice->bits;
        if ((bits >> 8 & 1) == send)
        {
            return;
        }

        bits = (bits & ~SoundVoice::ReverbSend) | send << 8;
        voice->bits = bits;
        Platform::Audio::SetVoiceReverb(VoiceNumber(voice), (bits & SoundVoice::ReverbSend) != 0);
    }

    s32 PlaySoundOnVoice(f32 volume, f32 pitchScale, SoundVoice* voice, GameSound* sound, u32 group, s32 last)
    {
        voice->volume = volume;
        voice->pitchScale = pitchScale;
        voice->bits &= ~SoundVoice::UseMask;
        f32 level = 0.0f < volume ? volume : 1.0f;
        s32 scaled = static_cast<s32>(level * VoiceVolumeScale);
        voice->right = scaled;
        voice->left = scaled;
        u32 basePitch = sound->basePitch;
        voice->pitch = static_cast<s32>(basePitch);
        if (0.0f < pitchScale)
        {
            voice->pitch = static_cast<s32>(static_cast<f32>(basePitch) * voice->pitchScale);
        }

        PlayOnVoice(voice, sound, group, last);
        voice->unknown1C = 0.0f;
        voice->bits |= SoundVoice::Playing;
        return VoiceNumber(voice);
    }

    s32 PlaySoundAt(f32 volume, f32 pitchScale, SoundVoice* voice, GameSound* sound, u32 group, const Vector4* position, s32 last)
    {
        voice->pitchScale = pitchScale;
        voice->volume = volume;
        voice->bits &= ~SoundVoice::UseMask;
        voice->unknown1C = 0.0f;
        f32 level = 1.0f;
        if (1.5f < volume)
        {
            level = 1.5f;
        }
        else if (voice->unknown1C < volume)
        {
            level = volume;
        }

        s32 pitch = sound->basePitch;
        if (0.0f < voice->pitchScale)
        {
            pitch = static_cast<s32>(static_cast<f32>(pitch) * voice->pitchScale);
        }

        f32 out[8];
        u32 heard = SoundAtPlace(level, g_SoundDistance, voice->unknown1C, pitch, position, out);
        if (heard != 0)
        {
            voice->unknown1C = out[4];
        }

        if ((heard & 0xFF) == 0)
        {
            return -1;
        }

        voice->pitch = static_cast<s32>(out[2]);
        voice->left = static_cast<s32>(out[0] * VoiceVolumeScale);
        voice->right = static_cast<s32>(out[1] * VoiceVolumeScale);
        voice->left = Phase(voice->left);
        voice->right = Phase(voice->right);
        PlayOnVoice(voice, sound, group, last);
        voice->bits |= SoundVoice::Playing;
        return VoiceNumber(voice);
    }

    void FreeSoundSamples(u32, u32 sound)
    {
        Platform::Audio::ReleaseSound(sound);
    }

    void PauseSound()
    {
        Platform::Audio::PauseAll();
    }

    void ResumeSound()
    {
        Platform::Audio::ResumeAll();
    }

    void ResetSound()
    {
        Platform::Audio::Reset();
        for (u32 group = 0; group < 4; group++)
        {
            GroupScales()[group] = 1.0f;
            Platform::Audio::SetGroupVolume(static_cast<s32>(SoundGroupOf(static_cast<s32>(group))),
                                            static_cast<u16>(g_GroupVolumes[group].left),
                                            static_cast<u16>(g_GroupVolumes[group].right));
        }
    }

    void SetGroupVolume(f32 left, f32 right, s32 group)
    {
        f32 rightVolume = right * GroupVolumeScale;
        f32 leftVolume = left * GroupVolumeScale;
        f32 scale = GroupScales()[group];
        g_GroupVolumes[group].right = static_cast<s16>(static_cast<s32>(rightVolume));
        g_GroupVolumes[group].left = static_cast<s16>(static_cast<s32>(leftVolume));
        Platform::Audio::SetGroupVolume(static_cast<s32>(SoundGroupOf(group)), static_cast<u32>(static_cast<s32>(leftVolume * scale)),
                                        static_cast<u32>(static_cast<s32>(rightVolume * scale)));
    }

    void StopAllSound()
    {
        for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
        {
            u32 bit = 1;
            for (u32 voice = 0; voice < Platform::Audio::VoicesPerCore; voice++)
            {
                if ((core->voicesInUse & bit) != 0)
                {
                    MuteVoice(&core->voices[voice]);
                }

                bit <<= 1;
            }

            SetReverb(core, 0, 0, 0);
        }

        for (u32 slot = 0; slot < 4; slot++)
        {
            MusicPlayer* player = g_Music->playing[slot];
            if (player == nullptr)
            {
                player = g_Music->fading[slot];
            }

            if (player == nullptr)
            {
                continue;
            }

            SetMusicState(player, MusicPlayer::Stopping);
            while (UpdateMusic(0.0f, player, false))
            {
            }

            g_Music->playing[slot] = nullptr;
            g_Music->fading[slot] = nullptr;
        }

        ClearInstanceSounds(&g_InstanceSounds);
        StopMusicEmitters();
        g_GroupFadeTime = 0.0f;
        ApplyGroupVolumes();
        for (u32 group = 0; group < 4; group++)
        {
            GroupScales()[group] = 1.0f;
            f32 level = GroupVolumeLevel(static_cast<s32>(group));
            SetGroupVolume(level, level, static_cast<s32>(group));
        }
    }

    u32 UpdateSound(s32 paused, TimeClock* clock)
    {
        f32 time = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
        if (g_MovieSound != 0)
        {
            return g_MovieSound;
        }

        for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
        {
            UpdateSoundCore(core);
        }

        if (paused == 0)
        {
            UpdateInstanceSounds();
            UpdateMusicEmitters();
        }

        UpdatePlayingSounds(time, paused != 0);
        if (0.0f < g_GroupFadeTime)
        {
            g_GroupFade = g_GroupFade - time / g_GroupFadeTime;
            if (g_GroupFade < 0.0f)
            {
                g_GroupFade = 0.0f;
            }

            for (u32 group = 0; group < 4; group++)
            {
                f32 left = static_cast<f32>(static_cast<s32>(static_cast<u16>(g_GroupVolumes[group].left))) * g_GroupFade;
                f32 right = static_cast<f32>(static_cast<s32>(static_cast<u16>(g_GroupVolumes[group].right))) * g_GroupFade;
                Platform::Audio::SetGroupVolume(static_cast<s32>(SoundGroupOf(static_cast<s32>(group))),
                                                static_cast<u32>(static_cast<s32>(left)), static_cast<u32>(static_cast<s32>(right)));
            }
        }

        UpdateCutsceneVolumes(time);
        g_SoundFrames++;
        if (g_SoundFrames >= KeepAliveFrames)
        {
            g_SoundFrames = 0;
            Platform::Audio::KeepAlive();
        }

        u32 result = 0;
        if (g_SoundRequestPending != 0)
        {
            result = static_cast<u32>(PlayMusicRequest(g_SoundRequest, &D_002E7918));
            g_SoundRequestPending = 0;
        }

        return result;
    }

    MusicSystem* ConstructMusic(MusicSystem* music)
    {
        for (s32 i = 0; i < 3; i++)
        {
            ConstructMusicPlayer(&music->players[i]);
        }

        for (u32 group = 0; group < 4; group++)
        {
            g_GroupVolumes[group].left = 0x1000;
            g_GroupVolumes[group].right = 0x1000;
        }

        ResetSound();
        Platform::Audio::ReserveSounds(0x100);
        for (u32 core = 0; core < 2; core++)
        {
            g_SoundCores[core].reverbBits = (g_SoundCores[core].reverbBits & 0xFFFF0000) | core;
        }

        g_SoundDistance = 60.0f;
        for (s32 i = 0; i < 3; i++)
        {
            music->players[i].location = Platform::Audio::ReserveSoundMemory(MusicBufferSize);
        }

        for (u32 slot = 0; slot < 4; slot++)
        {
            music->fading[slot] = nullptr;
            music->playing[slot] = nullptr;
            g_SoundSlots[slot] = 0;
        }

        return music;
    }

    void CreateMusicBuffers(MusicPlayer* player, SoundBankFiles* bank)
    {
        if (player->stream != nullptr)
        {
            return;
        }

        // Two of the music's blocks, double buffered
        const u32* header = reinterpret_cast<const u32*>(bank->header);
        u32 size = static_cast<u32>(static_cast<s32>(header[1]) * 2 * 2);
        if (size >= StreamFreeBufferMemory())
        {
            return;
        }

        player->stream = OpenFileStream(g_StreamSystem, size, player->location, MusicSecondBuffer);
        player->second = OpenFileStream(g_StreamSystem, 0, player->location + MusicSecondBuffer, MusicSecondBuffer);
        player->channelValue = Platform::Audio::ChannelValue(player->stream->channel);
        SetMusicState(player, MusicPlayer::Stopped);
    }

    void StartMusic(MusicPlayer* player, SoundBankFiles* bank, const u32* track)
    {
        u32 number = static_cast<u16>(track[0]);
        player->track = number;
        MusicTrack* entry = FindMusicTrack(bank, number);
        if (entry->interleaved == 0)
        {
            player->bits &= ~MusicPlayer::Interleaved;
        }
        else if (entry->interleaved == 1)
        {
            player->bits |= MusicPlayer::Interleaved;
        }

        u32 voices = VoiceCount(player);
        for (u32 i = 0; i < voices; i++)
        {
            player->voices[i] = FindFreeVoice(0, 0);
            if (player->voices[i] == nullptr)
            {
                player->voices[i] = FindFreeVoice(0, 1);
            }

            SoundVoice* voice = player->voices[i];
            voice->bits = (voice->bits & ~SoundVoice::TakenByMusic) | SoundVoice::TakenByMusic;
            if ((voice->bits & SoundVoice::ReverbSend) != 0)
            {
                voice->bits &= ~SoundVoice::ReverbSend;
                Platform::Audio::SetVoiceReverb(VoiceNumber(voice), false);
            }

            voice->bits = (voice->bits & ~SoundVoice::UseMask) | 0x400;
        }

        // The track: its number, group (bits 16-18), loop (bit 20) and start (bit 19), then its volumes and fade
        u32 bits = (player->bits & ~MusicPlayer::GroupMask) | (static_cast<u32>(reinterpret_cast<const u16*>(track)[1]) & 7) << 6;
        player->bits = bits;
        bits = (bits & ~MusicPlayer::Loops) | (track[0] >> 9 & MusicPlayer::Loops);
        player->bits = bits;
        bits &= ~MusicPlayer::StartsAtOnce;
        const f32* values = reinterpret_cast<const f32*>(track);
        player->targetLeft = values[1];
        player->targetRight = values[2];
        player->fadeTime = values[3];
        player->bits = bits | (track[0] >> 10 & MusicPlayer::StartsAtOnce);

        s32 file = bank->samples;
        Platform::Audio::ReadMusic(file, entry->offset, entry->size);
        s32 rate = Platform::Audio::PitchOfRate(static_cast<s32>(entry->rate));
        player->rate = static_cast<u32>(rate);
        u16 pitch = static_cast<u16>(static_cast<s32>(g_PitchScale * static_cast<f32>(rate)));
        s32 channel = player->stream->channel;
        Platform::Audio::StreamMusic(file, channel, static_cast<u32>(VoiceNumber(player->voices[0])) | GroupOf(player->bits), pitch,
                                     (player->bits & MusicPlayer::Loops) == 0);
        if ((player->bits & MusicPlayer::Interleaved) != 0)
        {
            Platform::Audio::InterleaveMusic(channel, reinterpret_cast<const u32*>(bank->header)[1]);
            Platform::Audio::AddMusicChannel(player->second->channel, channel,
                                             static_cast<u32>(VoiceNumber(player->voices[1])) | GroupOf(player->bits),
                                             player->location + MusicSecondBuffer);
            Platform::Audio::SetMusicValue(player->stream->channel, entry->value);
        }

        Platform::Audio::PrepareMusic(player->stream->channel);
        SetMusicState(player, MusicPlayer::Preparing);
    }

    s32 PlayMusic(f32 left, f32 right, f32 fadeTime, MusicPlayer* player)
    {
        player->left = 0.0f;
        player->fadeTime = fadeTime;
        f32 silent = player->left;
        player->right = 0.0f;
        player->targetLeft = left;
        player->targetRight = right;
        player->fromLeft = silent;
        player->fromRight = silent;
        if (fadeTime <= silent)
        {
            SetMusicVolume(left, right, player);
            SetMusicState(player, MusicPlayer::PlayingState);
        }
        else
        {
            SetMusicVolume(silent, silent, player);
            SetMusicState(player, MusicPlayer::FadingIn);
        }

        return Platform::Audio::PlayMusic(player->stream->channel);
    }

    void StopMusic(MusicPlayer* player)
    {
        u32 voices = VoiceCount(player);
        for (u32 i = 0; i < voices; i++)
        {
            if (player->voices[i] != nullptr)
            {
                player->voices[i]->bits &= ~SoundVoice::TakenByMusic;
            }
        }

        if (player->stream != nullptr)
        {
            Platform::Audio::StopMusic(player->stream->channel);
        }

        SetMusicState(player, MusicPlayer::Stopped);
    }

    s32 SetMusicPitch(f32 scale, MusicPlayer* player)
    {
        s32 pitch = static_cast<s32>(scale * static_cast<f32>(player->rate));
        f32 scaled = static_cast<f32>(pitch);
        u32 voices = VoiceCount(player);
        s32 result = 0;
        for (u32 i = 0; i < voices; i++)
        {
            SoundVoice* voice = player->voices[i];
            voice->pitch = pitch;
            result = Platform::Audio::SetVoicePitch(VoiceNumber(voice), static_cast<u16>(static_cast<s32>(g_PitchScale * scaled)));
        }

        return result;
    }

    s32 SetMusicVolume(f32 left, f32 right, MusicPlayer* player)
    {
        SoundVoice* first = player->voices[0];
        if ((player->bits & MusicPlayer::Interleaved) == 0 || g_MusicStereo == 0)
        {
            s32 rightVolume = static_cast<s32>(right * VoiceVolumeScale);
            s32 leftVolume = static_cast<s32>(left * VoiceVolumeScale);
            first->right = Phase(rightVolume);
            first->left = Phase(leftVolume);
            s32 result = Platform::Audio::SetVoiceVolume(VoiceNumber(first), static_cast<s16>(first->left),
                                                         static_cast<s16>(first->right));
            if ((player->bits & MusicPlayer::Interleaved) == 0)
            {
                return result;
            }

            // Both voices play both sides
            SoundVoice* second = player->voices[1];
            second->left = Phase(leftVolume);
            second->right = Phase(rightVolume);
            return Platform::Audio::SetVoiceVolume(VoiceNumber(second), static_cast<s16>(second->left),
                                                   static_cast<s16>(second->right));
        }

        // A side each
        first->right = 0;
        first->left = VoiceVolume(left);
        Platform::Audio::SetVoiceVolume(VoiceNumber(first), static_cast<s16>(first->left), 0);
        SoundVoice* second = player->voices[1];
        second->left = 0;
        second->right = VoiceVolume(right);
        return Platform::Audio::SetVoiceVolume(VoiceNumber(second), 0, static_cast<s16>(second->right));
    }

    bool UpdateMusic(f32 time, MusicPlayer* player, s32 paused)
    {
        switch (MusicState(player))
        {
        case MusicPlayer::Preparing:
        {
            u32 state = MusicPlayer::Preparing;
            if (Platform::Audio::IsMusicReady(player->stream->channel) && paused == 0)
            {
                if ((player->bits & MusicPlayer::StartsAtOnce) == 0)
                {
                    state = MusicPlayer::Ready;
                }
                else
                {
                    PlayMusic(player->targetLeft, player->targetRight, player->fadeTime, player);
                    state = MusicState(player);
                }
            }

            SetMusicState(player, state);
            break;
        }
        case MusicPlayer::FadingIn:
            SetMusicState(player, FadeMusic(time, player) != 0 ? MusicPlayer::PlayingState : MusicPlayer::FadingIn);
            break;
        case MusicPlayer::PlayingState:
            if (!Platform::Audio::IsMusicPlaying(player->stream->channel))
            {
                SetMusicState(player, MusicPlayer::Stopping);
                break;
            }

            if (player->left != player->targetLeft || player->right != player->targetRight)
            {
                FadeMusic(time, player);
            }

            break;
        case MusicPlayer::FadingOutState:
        {
            u32 state = MusicPlayer::FadingOutState;
            if (FadeMusic(time, player) != 0)
            {
                state = (player->bits & MusicPlayer::FadingOut) != 0 ? MusicPlayer::Stopping : MusicPlayer::Faded;
            }

            SetMusicState(player, state);
            break;
        }
        case MusicPlayer::Stopping:
            StopMusic(player);
            break;
        default:
            break;
        }

        return MusicState(player) != MusicPlayer::Stopped;
    }

    s32 MoviePlayer::LendSound()
    {
        g_MovieSound = 1;
        ClearReverbs();
        s32 result = Platform::Audio::LendToMovie();
        flags |= 0x80;
        return result;
    }

    void MoviePlayer::ReclaimSound()
    {
        Platform::Audio::ReclaimFromMovie();
        RestoreReverbs();
        g_MovieSound = 0;
        flags &= ~0x80u;
    }
}

void FadeToCutsceneVolumes()
{
    g_CutsceneVolumeMode = 1;
    g_CutsceneVolumeFade = 1.0f;
}

void FadeFromCutsceneVolumes()
{
    g_CutsceneVolumeMode = 3;
    g_CutsceneVolumeFade = 1.0f;
}

namespace
{
constexpr u32 VoicesPerCore = 24;
constexpr u32 UseShift = 9;
constexpr f32 NoScale = -1.0f;
// The frames a core keeps its reverb without a voice sending to it
constexpr u32 ReverbIdleFrames = 0x12D;
// A core without reverb counts as this many free voices when the chunk's reverb picks one
constexpr s32 ReverbFreeCore = 10000;
// The sound's listener

bool StealableVoice(const SoundVoice* voice)
{
    return (voice->bits >> UseShift & 0x1F) == 0;
}

SoundVoice* TakeVoice(SoundCore* core, s32 steal)
{
    u32 bit = 1;
    for (u32 index = 0; index < VoicesPerCore; index++, bit <<= 1)
    {
        SoundVoice* voice = nullptr;
        if ((core->voicesInUse & bit) == 0)
        {
            voice = &core->voices[index];
        }
        else if (steal != 0)
        {
            voice = &core->voices[index];
            if (!StealableVoice(voice))
            {
                voice = nullptr;
            }
            else
            {
                MuteVoice(voice);
            }
        }

        if (voice != nullptr)
        {
            u32 bits = (voice->bits & ~SoundVoice::NumberMask) | (index & SoundVoice::NumberMask);
            voice->bits = bits;
            bits = (bits & ~SoundVoice::SecondCore) | static_cast<u32>(static_cast<u16>(core->reverbBits) != 0) << 7;
            voice->bits = bits & ~SoundVoice::TakenByMusic;
            core->voicesInUse |= bit;
            return voice;
        }
    }

    return nullptr;
}

u32 FreeVoices(const SoundCore* core, s32 steal)
{
    u32 count = 0;
    u32 bit = 1;
    for (u32 index = 0; index < VoicesPerCore; index++, bit <<= 1)
    {
        if ((core->voicesInUse & bit) == 0)
        {
            count++;
        }
        else if (steal != 0 && StealableVoice(&core->voices[index]))
        {
            count++;
        }
    }

    return count;
}

// The reverb modes of the chunks' reverb types (0 is the sound processor's 9)
s32 ReverbModeOf(u8 type)
{
    switch (type)
    {
    case 0:
        return 9;
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
        return 4;
    case 5:
        return 5;
    case 6:
        return 6;
    default:
        return 0;
    }
}
}

// Whether the core is set to the reverb settings: their mode and depth (UseReverb has it inline)
extern "C" bool CoreHasReverb(const SoundCore* core, const ReverbSettings* settings) RETAIL(func_001E61E0);

bool CoreHasReverb(const SoundCore* core, const ReverbSettings* settings)
{
    const u8* bytes = reinterpret_cast<const u8*>(settings);
    return core->reverbMode == ReverbModeOf(bytes[0]) &&
           static_cast<s32>(settings->depth * ReverbVolumeScale) == core->reverbDepthLeft;
}

SoundVoice* FindFreeVoice(s32 reverbMode, s32 steal)
{
    if (reverbMode != 0)
    {
        for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
        {
            if (core->reverbMode == reverbMode)
            {
                SoundVoice* voice = TakeVoice(core, steal);
                if (voice != nullptr)
                {
                    return voice;
                }
            }
        }
    }

    SoundCore* plain = nullptr;
    for (s32 index = 0; index < 2; index++)
    {
        if (g_SoundCores[index].reverbMode == 0 && FreeVoices(&g_SoundCores[index], steal) != 0)
        {
            plain = &g_SoundCores[index];
            break;
        }
    }

    if (plain != nullptr)
    {
        return TakeVoice(plain, steal);
    }

    for (SoundCore* core = g_SoundCores; core < g_SoundCores + 2; core++)
    {
        if (FreeVoices(core, steal) != 0)
        {
            SoundVoice* voice = TakeVoice(core, steal);
            if (voice != nullptr)
            {
                return voice;
            }
        }
    }

    return nullptr;
}

void ReleaseVoice(SoundVoice* voice)
{
    u32 bits = voice->bits;
    u32 core = bits >> 7 & 1;
    voice->bits = bits & ~SoundVoice::UseMask;
    voice->volume = NoScale;
    voice->pitchScale = NoScale;
    g_SoundCores[core].voicesInUse &= ~(1u << (bits & SoundVoice::NumberMask));
}

SoundVoice* VoiceOfNumber(u32 number)
{
    if (number < VoicesPerCore)
    {
        return &g_SoundCores[0].voices[number];
    }

    return &g_SoundCores[1].voices[number - VoicesPerCore];
}

void UpdateSoundCore(SoundCore* core)
{
    bool reverbUsed = false;
    u32 bit = 1;
    for (u32 index = 0; index < VoicesPerCore; index++, bit <<= 1)
    {
        SoundVoice* voice = &core->voices[index];
        if ((core->voicesInUse & bit) == 0 || (voice->bits & SoundVoice::TakenByMusic) != 0)
        {
            continue;
        }

        u32 playing = UpdateVoice(voice);
        u32 bits = voice->bits;
        if (playing == 0)
        {
            voice->pitchScale = NoScale;
            voice->volume = NoScale;
            voice->bits = bits & ~SoundVoice::UseMask;
            core->voicesInUse &= ~(1u << (bits & SoundVoice::NumberMask));
        }
        else if ((bits & SoundVoice::ReverbSend) != 0)
        {
            reverbUsed = true;
        }
    }

    if (reverbUsed || core->reverbMode == 0)
    {
        core->unknown18 = 0;
    }
    else if (static_cast<u32>(core->unknown18) < ReverbIdleFrames)
    {
        core->unknown18++;
    }
    else
    {
        SetReverb(core, 0, 0, 0);
        core->unknown18 = 0;
    }
}

s32 UseReverb(const ReverbSettings* settings)
{
    const u8* bytes = reinterpret_cast<const u8*>(settings);
    s32 mode = ReverbModeOf(bytes[0]);
    for (s32 index = 0; index < 2; index++)
    {
        SoundCore* core = &g_SoundCores[index];
        if (core->reverbMode == ReverbModeOf(bytes[0]) &&
            static_cast<s32>(settings->depth * ReverbVolumeScale) == core->reverbDepthLeft)
        {
            return mode;
        }
    }

    s32 most = -1;
    s32 chosen = -1;
    for (s32 index = 0; index < 2; index++)
    {
        SoundCore* core = &g_SoundCores[index];
        s32 free = ReverbFreeCore;
        if (core->reverbMode != 0)
        {
            free = 0;
            for (u32 voice = 0; voice < VoicesPerCore; voice++)
            {
                if ((core->voicesInUse & 1u << voice) == 0)
                {
                    free++;
                }
            }
        }

        if (most < free)
        {
            most = free;
            chosen = index;
        }
    }

    SetReverbFromSettings(&g_SoundCores[chosen], bytes);
    return mode;
}

void SetSoundListener(ReferencedObject* object)
{
    AssignReference(&g_SoundListener, object);
}

s32 ListenerVoiceKind()
{
    auto* listener = static_cast<InstanceContext*>(g_SoundListener != nullptr ? g_SoundListener->object : nullptr);
    if (listener == nullptr)
    {
        return 0;
    }

    ChunkData* chunk = listener->chunk;
    const ReverbSettings* settings = &chunk->reverb;
    if (chunk->soundBoxCount != 0)
    {
        ObjectPlace* place = listener->place;
        place->SyncPosition();
        Vector4 position = place->position;
        if (chunk->InSoundBox(&position) != 0)
        {
            settings = &chunk->boxReverb;
        }
    }

    if (static_cast<s8>(settings->bits) == -1)
    {
        return 0;
    }

    return UseReverb(settings);
}

u32 SoundBox::Contains(const Vector4* point)
{
    f32 dy = point->y - position.y;
    if (!((point->x - position.x) * (point->x - position.x) + dy * dy + (point->z - position.z) * (point->z - position.z) <=
          radiusSquared))
    {
        return 0;
    }

    Vector4 local = *point;
    VuTransformPoint(&inverse, &local, &local);
    return reinterpret_cast<Box*>(this)->Contains(&local);
}

void UpdateCutsceneVolumes(f32 time)
{
    // The groups' volumes in cutscenes: the music's is left alone
    static constexpr f32 CutsceneLevels[4] = {0.25f, 0.25f, 0.25f, 1.0f};
    if (g_CutsceneVolumeMode == 0 || g_CutsceneVolumeMode == 2)
    {
        return;
    }

    if (0.0f < g_CutsceneVolumeFade)
    {
        g_CutsceneVolumeFade = g_CutsceneVolumeFade - time / g_CutsceneVolumeFade;
        if (g_CutsceneVolumeFade < 0.0f)
        {
            g_CutsceneVolumeFade = 0.0f;
        }
    }

    f32 share = g_CutsceneVolumeFade;
    if (g_CutsceneVolumeMode == 3)
    {
        share = 1.0f - share;
    }

    f32* scales = GroupScales();
    for (u32 group = 0; group < 4; group++)
    {
        f32 level = CutsceneLevels[group];
        scales[group] = level + (1.0f - level) * share;
        f32 volume = GroupVolumeLevel(static_cast<s32>(group));
        SetGroupVolume(volume, volume, static_cast<s32>(group));
    }

    if (g_CutsceneVolumeFade == 0.0f)
    {
        g_CutsceneVolumeMode = g_CutsceneVolumeMode == 1 ? 2 : 0;
    }
}

f32 GroupVolumeLevel(s32 group)
{
    constexpr f32 Half = 1.0f / (2.0f * GroupVolumeScale);
    return static_cast<f32>(static_cast<u16>(g_GroupVolumes[group].left)) * Half +
           static_cast<f32>(static_cast<u16>(g_GroupVolumes[group].right)) * Half;
}

void CreateMusic()
{
    g_Music = ConstructMusic(static_cast<MusicSystem*>(MemoryAllocate(sizeof(MusicSystem))));
}

void SetMusicStereo(u32 mode)
{
    g_MusicStereo = mode;
    for (MusicPlayer& player : g_Music->players)
    {
        if (MusicState(&player) != MusicPlayer::Stopped)
        {
            ApplyMusicVolume(&player);
        }
    }
}

void ReclaimMusicBuffers()
{
    for (MusicPlayer& player : g_Music->players)
    {
        LendMusicPlayer(&player, 0);
    }
}

void LendMusicPlayer(MusicPlayer* player, u32 lend)
{
    if (lend == 0)
    {
        if (MusicState(player) != MusicPlayer::LentToMovie)
        {
            return;
        }

        SetMusicState(player, MusicPlayer::Stopped);
        return;
    }

    if (MusicState(player) != MusicPlayer::Stopped)
    {
        StopMusic(player);
    }

    SetMusicState(player, MusicPlayer::LentToMovie);
}

s32 ApplyMusicVolume(MusicPlayer* player)
{
    return SetMusicVolume(player->left, player->right, player);
}

void FadeMusicOut(f32 time, MusicPlayer* player, u32 stops)
{
    constexpr f32 ShortestFade = Rounded(0.3);
    player->fadeTime = time;
    if (time < ShortestFade)
    {
        player->fadeTime = ShortestFade;
    }

    player->fromLeft = player->left;
    player->bits = (((player->bits & ~MusicPlayer::StateMask) | MusicPlayer::FadingOutState << 1) & ~MusicPlayer::FadingOut) |
                   (stops & 1) << 10;
    player->fromRight = player->right;
    player->targetLeft = 0.0f;
    player->targetRight = 0.0f;
}

void FadeMusicTo(f32 left, f32 right, f32 time, MusicPlayer* player)
{
    if (MusicState(player) == MusicPlayer::FadingOutState)
    {
        SetMusicState(player, MusicPlayer::PlayingState);
    }

    player->fadeTime = time;
    player->fromRight = player->right;
    player->targetLeft = left;
    player->targetRight = right;
    player->fromLeft = player->left;
}

MusicPlayer* ConstructMusicPlayer(MusicPlayer* player)
{
    player->channelValue = 0;
    player->voices[0] = nullptr;
    player->voices[1] = nullptr;
    player->bits &= ~0x3Fu;
    player->stream = nullptr;
    player->second = nullptr;
    return player;
}

u32 FadeMusic(f32 time, MusicPlayer* player)
{
    constexpr f32 Instant = Rounded(1e-05);
    u32 leftDone = 0;
    u32 rightDone = 0;
    if (player->fadeTime < Instant)
    {
        leftDone = 1;
        rightDone = 1;
        player->left = player->targetLeft;
        player->right = player->targetRight;
    }
    else
    {
        f32 step = time / player->fadeTime;
        f32 targetLeft = player->targetLeft;
        f32 left = player->left + (targetLeft - player->fromLeft) * step;
        player->left = left;
        player->right = player->right + (player->targetRight - player->fromRight) * step;
        if (player->fromLeft < targetLeft ? targetLeft <= left : left <= targetLeft)
        {
            player->left = targetLeft;
            leftDone = 1;
        }

        f32 targetRight = player->targetRight;
        f32 right = player->right;
        if (player->fromRight < targetRight ? targetRight <= right : right <= targetRight)
        {
            player->right = targetRight;
            rightDone = 1;
        }
    }

    SetMusicVolume(player->left, player->right, player);
    return leftDone != 0 ? rightDone : 0;
}

void ReadSoundParameters(u16* parameters, Stream* stream)
{
    for (u32 index = 0; index < 5; index++)
    {
        stream->ReadS16(reinterpret_cast<s16*>(&parameters[index]));
    }
}

f32 RemainingShare(f32 part, f32 whole, f32 total)
{
    return total - part / whole;
}

GameSound* SoundById(u16 id)
{
    GameSound* sound = nullptr;
    GameResources* resources = G_GameResourcesObjectPointer;
    ResourceTable* sounds = resources->sounds;
    ResourceTable* voices = resources->voices[g_CurrentLanguage];
    if (id < (sounds->bits & ResourceTable::CapacityMask))
    {
        u16 copy = id;
        if (copy != 0xFFFF)
        {
            sound = static_cast<GameSound*>(sounds->items[copy & 0x7FFF]);
        }

        if (sound == nullptr && copy != 0xFFFF)
        {
            sound = static_cast<GameSound*>(voices->items[copy & 0x7FFF]);
        }
    }

    return sound;
}

GameSound* GameSound::Construct(GameSound* sound, Stream* stream)
{
    ConstructResourceHeader(sound);
    sound->name.string = nullptr;
    sound->name.capacity = 0;
    sound->name.length = 0;
    RetailLibc::MemorySet(&sound->flags, 0, 4);
    sound->Read(stream);
    return sound;
}

void GameSound::Read(Stream* stream)
{
    u32 header;
    stream->ReadU32(&header);
    ReadSoundParameters(&basePitch, stream);
    stream->ReadS32(&size);
    stream->ReadS32(&offset);
}

void GameSound::Destroy(u32 destroyFlags)
{
    FreeSoundSamples(reinterpret_cast<u32>(&basePitch), soundId);
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}
