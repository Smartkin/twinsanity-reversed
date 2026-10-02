#include "platform/time.h"

#include <kernel.h>

// Timer 0 counts 16 bits and interrupts when it overflows, which counts the overflows: the retail game's (SetupCPU_Timer0,
// GetCPU_Timer0_Count and its interrupt handler)
namespace
{
volatile u32& TimerCount = *reinterpret_cast<volatile u32*>(0x10000000);
volatile u32& TimerMode = *reinterpret_cast<volatile u32*>(0x10000010);
// The bus clock / 256, counting, interrupting on overflow; writing the flags clears them
constexpr u32 Mode = 0xE82;
constexpr u32 OverflowFlag = 0x800;

volatile u64 g_Overflows;
// Ticks sets it and the interrupt clears it: Ticks reads again when an overflow came in between
volatile bool g_Reading;
u64 g_OverflowsSeen;
bool g_Initialised;

s32 OnOverflow(s32, void*, void*)
{
    g_Reading = false;
    if ((TimerMode & OverflowFlag) != 0)
    {
        g_Overflows = g_Overflows + 1;
        TimerMode = Mode;
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

    TimerMode = 0;
    TimerCount = 0;
    TimerMode = Mode;
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
    u32 count = TimerCount;
    u64 overflows = g_Overflows;
    if (!g_Reading)
    {
        count = TimerCount;
        overflows = g_Overflows;
    }
    else if (count == 0 && g_OverflowsSeen == overflows)
    {
        overflows++;
    }

    g_OverflowsSeen = g_Overflows;
    return overflows << 16 | (count & 0xFFFF);
}
