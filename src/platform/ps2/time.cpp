#include "platform/time.h"

#include <kernel.h>
#include <timer.h>

// Timer 0 counts 16 bits and interrupts when it overflows, which counts the overflows: the retail game's (SetupCPU_Timer0,
// GetCPU_Timer0_Count and its interrupt handler)
namespace
{
// A timer's mode register (Tn_MODE): its clock, its gate, whether it counts up to its compare value and back to 0, and
// interrupts when it reaches it or overflows; the flags are set when it did (writing 1 clears them)
union TimerMode
{
    u32 value;
    struct
    {
        u32 clock : 2;
        u32 gateEnabled : 1;
        u32 gateOnVblank : 1;
        u32 gateMode : 2;
        u32 zeroOnCompare : 1;
        u32 counting : 1;
        u32 interruptOnCompare : 1;
        u32 interruptOnOverflow : 1;
        u32 compareFlag : 1;
        u32 overflowFlag : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(TimerMode, 4);

constexpr u32 BusClockBy256 = 2;
constexpr u32 TimerCountMask = 0xFFFF;
constexpr u32 TimerCountBits = 16;

// The bus clock / 256, counting, interrupting on overflow, both flags cleared
u32 CountingMode()
{
    TimerMode mode;
    mode.value = 0;
    mode.clock = BusClockBy256;
    mode.counting = 1;
    mode.interruptOnOverflow = 1;
    mode.compareFlag = 1;
    mode.overflowFlag = 1;
    return mode.value;
}

volatile u64 g_Overflows;
// Ticks sets it and the interrupt clears it: Ticks reads again when an overflow came in between
volatile bool g_Reading;
u64 g_OverflowsSeen;
bool g_Initialised;

s32 OnOverflow(s32, void*, void*)
{
    g_Reading = false;
    TimerMode mode;
    mode.value = *T0_MODE;
    if (mode.overflowFlag != 0)
    {
        g_Overflows = g_Overflows + 1;
        *T0_MODE = CountingMode();
    }

    ExitHandler();
    return 1;
}
}

void Platform::Time::Initialise()
{
    if (g_Initialised)
    {
        return;
    }

    *T0_MODE = 0;
    *T0_COUNT = 0;
    *T0_MODE = CountingMode();
    g_Initialised = true;
    DI();
    DisableIntc(INTC_TIM0);
    AddIntcHandler2(INTC_TIM0, OnOverflow, 0, nullptr);
    EnableIntc(INTC_TIM0);
    EI();
}

// A count of 0 with no overflow counted since the last call is taken as the overflow its interrupt hasn't counted yet
u64 Platform::Time::Ticks()
{
    g_Reading = true;
    u32 count = *T0_COUNT;
    u64 overflows = g_Overflows;
    if (!g_Reading)
    {
        count = *T0_COUNT;
        overflows = g_Overflows;
    }
    else if (count == 0 && g_OverflowsSeen == overflows)
    {
        overflows++;
    }

    g_OverflowsSeen = g_Overflows;
    return overflows << TimerCountBits | (count & TimerCountMask);
}
