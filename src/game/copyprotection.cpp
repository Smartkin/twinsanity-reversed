#include "game/copyprotection.h"

#include "game/camerarig.h"
#include "game/math.h"
#include "game/save.h"

#include <cstdint>

// The system module's statics, made at start-up, and the copy protection they arm

extern "C"
{
    // 45 degrees in 65536ths of a turn (the field of view the camera rigs default to is a copy of it), the copy protection's
    // countdown in frames and its frame counter
    extern s32 g_FortyFiveDegrees RETAIL(D_0030A7C8);
    extern s32 g_CopyrightCountdown RETAIL(CopyrightChecked_);
    extern s32 g_CopyProtectionFrame RETAIL(D_0030A7B8);
    // "(c) 2004 Vivendi Universal Games...", and the sum of its characters
    extern const char g_CopyrightNotice[] RETAIL(G_VVU_Games_String);
    extern const s32 g_CopyrightNoticeSum RETAIL(REQ_CHECKSUM);

    // GCC 2.9x's static initialisation of the module (when initialise is 1 and the priority 0xFFFF), and the static constructor
    // that runs it
    void InitSystemStatics(s32 initialise, s32 priority) RETAIL(FUN_001815e8);
    void SystemStaticInit() RETAIL(FUN_00182490);
}

namespace
{
// The countdown: 17100 frames, another 3286 for each minute of the clock past a multiple of 8 and one for each second
constexpr s32 ArmedFrames = 0x42CC;
constexpr s32 FramesPerMinute = 0xCD6;
// The instruction it breaks (bits 5, 7 and 19 cleared): 4 bytes before 64 for each of the counter's frames past this
constexpr std::uintptr_t BrokenCode = 0x158290;
constexpr u32 BrokenBits = 0xFFF7FF5F;
// The frame counter goes round at this, counting two a frame
constexpr s32 FrameCounterWrap = 0x20A6;
constexpr s32 NoiseThreshold = 0x58;
}

void InitSystemStatics(s32 initialise, s32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    AngleFrom(&g_FortyFiveDegrees, QuarterPi, AngleRadians);
    g_CopyrightCountdown = ArmedFrames;
    g_DefaultFov = g_FortyFiveDegrees;
    SaveDate now;
    ConstructSaveDate(&now);
    GetSaveDate(&now);
    g_CopyrightCountdown += (now.minute & 7) * FramesPerMinute + now.second;
    // Two arrays of ten things whose constructors do nothing come last (empty loops in retail)
}

void SystemStaticInit()
{
    InitSystemStatics(1, DefaultInitPriority);
}

void SimpleCopyrightChecksum()
{
    s32 sum = 0;
    for (const char* character = g_CopyrightNotice; *character != '\0'; character++)
    {
        sum += static_cast<s8>(*character);
    }

    if (sum == g_CopyrightNoticeSum)
    {
        g_CopyrightCountdown = 0;
    }
}

u32 CopyProtectionStep()
{
    s32 frame = ++g_CopyProtectionFrame;
    if (g_CopyrightCountdown != 0)
    {
        g_CopyrightCountdown--;
        if (g_CopyrightCountdown == 0)
        {
            // It counts down again from the frame's number, and the code is broken
            g_CopyrightCountdown = frame;
            std::uintptr_t instruction = BrokenCode + (static_cast<u32>(frame) << 6) - 4;
            *reinterpret_cast<volatile u32*>(instruction) &= BrokenBits;
        }
    }

    s32 index = g_CopyProtectionFrame++;
    if (g_CopyProtectionFrame >= FrameCounterWrap)
    {
        g_CopyProtectionFrame = 0;
    }

    // A word after the countdown, the frame counter's number of words on
    std::uintptr_t word = reinterpret_cast<uintptr_t>(&g_CopyrightCountdown) + (static_cast<u32>(index) << 2);
    return *reinterpret_cast<const s32*>(word) < NoiseThreshold;
}
