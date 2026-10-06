#include "platform/pads.h"

#include "debug.h"

// The desktop's side of Platform::Pads: no controllers yet
namespace Platform::Pads
{
s32 Initialise()
{
    return 0;
}

s32 Open(s32 port, s32 slot, void* buffer)
{
    return 0;
}

s32 Close(s32 port, s32 slot)
{
    return 0;
}

s32 Read(s32 port, s32 slot, u8* data)
{
    return 0;
}

State GetState(s32 port, s32 slot)
{
    return {};
}

RequestState GetRequestState(s32 port, s32 slot)
{
    return {};
}

s32 InfoMode(s32 port, s32 slot, ModeInfo info, s32 index)
{
    return 0;
}

s32 SetMainMode(s32 port, s32 slot, MainMode mode, ModeLock lock)
{
    return 0;
}

s32 InfoActuator(s32 port, s32 slot, s32 actuator, s32 term)
{
    return 0;
}

s32 SetActuatorAlign(s32 port, s32 slot, const u8 align[6])
{
    return 0;
}

s32 SetActuatorDirect(s32 port, s32 slot, const u8 values[6])
{
    return 0;
}

s32 InfoPressureMode(s32 port, s32 slot)
{
    return 0;
}

s32 EnterPressureMode(s32 port, s32 slot)
{
    return 0;
}

}

volatile u32 g_TestPadButtons = 0;
