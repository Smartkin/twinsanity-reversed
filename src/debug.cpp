#include "debug.h"

#include "game/gamecontroller.h"

extern "C"
{
    volatile u32 g_DebugStep = 0;
    volatile s32 g_DebugValues[16] = {};

    __attribute__((section(".data"))) volatile u32 g_DebugFixedTime = 0;
    volatile u32 g_DebugFrame = 0;
    volatile u32 g_DebugFreezeFrame = 0;
    volatile u32 g_DebugStates[64] = {};
    volatile u32 g_DebugRandom[4096] = {};
    // The C library's reentrancy block: rand's state is at 0x58
    extern u8* D_002EA3CC;
    volatile DebugInput g_DebugInputs[16] = {};

}

namespace
{
// The game controller's states as two words (the state it's in and the next in the high one)
volatile u32* StateWords()
{
    return reinterpret_cast<volatile u32*>(&G_GameController->states);
}
}

void DebugFrameRendered()
{
    u32 frame = g_DebugFrame + 1;
    g_DebugFrame = frame;
    g_DebugRandom[frame % 4096] = *reinterpret_cast<volatile u32*>(D_002EA3CC + 0x58);
    if (G_GameController != nullptr)
    {
        // The current state is in bits 44-49 of the 64 bits at 8
        u32 state = StateWords()[1] >> 12 & 0x3F;
        static u32 logged;
        if (logged == 0 || (g_DebugStates[logged - 1] & 0xFF) != state)
        {
            if (logged < 64)
            {
                g_DebugStates[logged++] = frame << 8 | state;
            }
        }
    }

    for (volatile DebugInput& input : g_DebugInputs)
    {
        if (input.kind == DebugInput::State && input.frame == frame && G_GameController != nullptr)
        {
            auto* high = StateWords() + 1;
            *high = (*high & ~(0x3Fu << 18)) | (input.value & 0x3F) << 18;
        }
        else if (input.kind == DebugInput::Press)
        {
            if (input.frame == frame)
            {
                g_TestPadButtons = input.value;
            }
            else if (input.frame + input.length == frame)
            {
                g_TestPadButtons = 0;
            }
        }
    }

    while (g_DebugFreezeFrame == frame)
    {
    }
}
