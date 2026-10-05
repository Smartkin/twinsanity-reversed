#include "game/shaderanimation.h"

#include "game/clock.h"
#include "game/disk.h"
#include "game/memory.h"
#include "game/stream.h"

EABI_EXPORT(FUN_002996b8, AdvanceShaderAnimation);

namespace
{
constexpr f32 TrackUnit = 0x1p-12f;
}

ShaderAnimation* ConstructShaderAnimation(ShaderAnimation* animation)
{
    animation->header.frames = 0;
    animation->data.diskHandle = -1;
    animation->header.rate = 0;
    animation->data.layout.value = 0;
    animation->data.frames = 0;
    animation->frames = nullptr;
    animation->loopStart = 0;
    animation->time = 0;
    animation->loopLength = 0;
    animation->frameShare = 0.0f;
    return animation;
}

void DestroyShaderAnimation(ShaderAnimation* animation, u32 destroyFlags)
{
    if (animation->frames != nullptr)
    {
        MemoryDeallocate2_(animation->frames);
    }

    if (animation->data.diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &animation->data.diskHandle);
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(animation);
    }
}

void ReadShaderAnimation(ShaderAnimation* animation, Stream* stream)
{
    stream->Read(&animation->header, sizeof(animation->header), 1);
    ReadAnimationData(&animation->data, stream);
    auto* frames = static_cast<AnimationData*>(MemoryAllocate(sizeof(AnimationData)));
    u32 count = animation->header.frames;
    animation->frames = frames;
    frames->information = &animation->data;
    u32 rate = animation->header.rate;
    animation->loopLength = static_cast<s32>(static_cast<f32>(count) / static_cast<f32>(rate) * g_ClockUnitsPerSecond);
}

void AnimateShader(ShaderAnimation* animation, const s32* time)
{
    constexpr u32 Channels = 6;
    if (animation->header.started == 0)
    {
        s32 now = *time;
        animation->header.started = 1;
        animation->time = now;
        animation->loopStart = now;
        animation->loopTime = 0;
    }
    else
    {
        s32 now = *time;
        s32 length = animation->loopLength;
        animation->time = now;
        animation->loopTime = now - animation->loopStart;
        // Retail never stops for a length of 0 or less
        while (!(animation->loopTime < length))
        {
            animation->loopTime -= length;
            animation->loopStart += length;
        }
    }

    u32 count = animation->header.frames;
    f32 through = static_cast<f32>(animation->loopTime) * g_SecondsPerClockUnit /
                  (static_cast<f32>(animation->loopLength) * g_SecondsPerClockUnit);
    f32 position = static_cast<f32>(static_cast<s32>(count)) * through;
    s32 frame = static_cast<s32>(position);
    s32 next = frame == static_cast<s32>(count) - 1 ? 0 : frame + 1;
    animation->frameShare = position - static_cast<f32>(frame);
    FindFrames(animation->frames, frame, next);

    // The one joint's channels, like a blend skin's shapes
    const AnimationData* data = animation->frames;
    f32 share = animation->frameShare;
    const JointTrackSettings* settings = data->settings;
    TrackReader reader;
    reader.statics = settings->statics;
    reader.unused02 = (1 << settings->flags.channels) - 1;
    reader.staticValues = data->statics + settings->staticIndex;
    reader.current = data->current + settings->frameIndex;
    reader.next = data->next + settings->frameIndex;
    f32* values[Channels] = {&animation->uvOffset.x, &animation->uvOffset.y, &animation->colour.x, &animation->colour.y,
                             &animation->colour.z, &animation->colour.w};
    for (f32* value : values)
    {
        if ((reader.statics & 1) == 0)
        {
            f32 coming = static_cast<f32>(*reader.next) * TrackUnit;
            f32 now = static_cast<f32>(*reader.current) * TrackUnit;
            reader.current++;
            reader.next++;
            *value = coming * share + now * (1.0f - share);
        }
        else
        {
            *value = static_cast<f32>(*reader.staticValues) * TrackUnit;
            reader.staticValues++;
        }

        reader.statics >>= 1;
        reader.unused02 >>= 1;
    }
}

void AdvanceShaderAnimation(ShaderAnimation* animation, f32 seconds)
{
    s32 time = 0;
    if (animation->header.started != 0)
    {
        time = animation->loopStart + animation->loopTime + static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    }

    AnimateShader(animation, &time);
}

void RestartShaderAnimation(ShaderAnimation* animation)
{
    animation->header.started = 0;
}

u32 SameShaderAnimations(const ShaderAnimation* animation, const ShaderAnimation* other)
{
    u32 same = 0;
    if ((animation->header.value & ShaderAnimationHeader::FramesAndRate) ==
        (other->header.value & ShaderAnimationHeader::FramesAndRate))
    {
        u32 size = AnimationDataSize(&animation->data);
        if (size == AnimationDataSize(&other->data))
        {
            const u8* bytes = DiskLoadedMemory(GetDiskManager(), &animation->data.diskHandle);
            const u8* otherBytes = DiskLoadedMemory(GetDiskManager(), &other->data.diskHandle);
            u32 differences = 0;
            for (u32 index = 0; index < size; index++)
            {
                if (bytes[index] != otherBytes[index])
                {
                    differences++;
                }
            }

            same = differences == 0;
        }
    }

    if (animation->frames != nullptr && animation->frames->information != other->frames->information)
    {
        same = 0;
    }

    return same;
}

bool DifferentShaderAnimations(const ShaderAnimation* animation, const ShaderAnimation* other)
{
    return SameShaderAnimations(animation, other) == 0;
}
