#pragma once

#include "abi.h"
#include "common.h"
#include "game/animation.h"
#include "game/math.h"

class Stream;

// A shader animation's header: its frames, its frames a second and whether it has started (set the first time it's played)
union ShaderAnimationHeader
{
    // What two animations' headers have to agree in
    static constexpr u32 FramesAndRate = 0x1FFFFF;

    u32 value;
    struct
    {
        u32 frames : 16;
        u32 rate : 5;
        u32 started : 1;
        u32 unused22 : 10;
    };
};
CHECK_SIZE(ShaderAnimationHeader, 4);

// A shader's animation (0x40 bytes, TT Lab's TwinShaderAnimation): its header, its data (one joint's settings: six channels, the
// UV offset's U and V and the colour's R, G, B and A, in 4096ths), its frames' places in the data, the time its loop started at,
// the time it was played at, a loop's length, the time into the loop, the share between this frame and the next, and what it
// comes to: the UV offset and the colour
struct ShaderAnimation
{
    ShaderAnimationHeader header;
    AnimationDataInformation data;
    AnimationData* frames;
    s32 loopStart;
    s32 time;
    s32 loopLength;
    s32 loopTime;
    f32 frameShare;
    Vector2 uvOffset;
    Vector4 colour;
};
CHECK_OFFSET(ShaderAnimation, uvOffset, 0x28);
CHECK_OFFSET(ShaderAnimation, colour, 0x30);
CHECK_SIZE(ShaderAnimation, 0x40);

extern "C"
{
    // Without frames, a rate (the header's other bits as they were) or data
    ShaderAnimation* ConstructShaderAnimation(ShaderAnimation* animation) RETAIL(FUN_002995e0);
    void DestroyShaderAnimation(ShaderAnimation* animation, u32 destroyFlags) RETAIL(FUN_00299628);
    // Read from a shader's data: the header, the data, and a loop's length in clock units
    void ReadShaderAnimation(ShaderAnimation* animation, Stream* stream) RETAIL(FUN_00297060);
    // Played at a time (started there the first time): the time into its loop, the frames it's between and the six channels'
    // values (a static channel's value, else this frame's and the next's by the share)
    void AnimateShader(ShaderAnimation* animation, const s32* time);
    // Moved on by the seconds from where it is (from the time 0 when it hasn't started)
    void AdvanceShaderAnimation(ShaderAnimation* animation, f32 seconds) RETAIL_N32(FUN_002996b8);
    // Started again the next time it's played
    void RestartShaderAnimation(ShaderAnimation* animation) RETAIL(FUN_002996a0);
    // Whether two are the same: their headers and data, and the data their frames point at (an animation's own, so only an
    // animation without frames is the same as another)
    u32 SameShaderAnimations(const ShaderAnimation* animation, const ShaderAnimation* other) RETAIL(FUN_002978c0);
    bool DifferentShaderAnimations(const ShaderAnimation* animation, const ShaderAnimation* other) RETAIL(FUN_00299718);
}
