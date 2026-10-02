#include "platform/pads.h"

#include "debug.h"

#include <libpad.h>

// PS2SDK's libpadx, for the disc's PADMAN. Its states and request states are the interface's
s32 Platform::Pads::Initialise()
{
    return padInit(0);
}

s32 Platform::Pads::Open(s32 port, s32 slot, void* buffer)
{
    return padPortOpen(port, slot, buffer);
}

s32 Platform::Pads::Close(s32 port, s32 slot)
{
    return padPortClose(port, slot);
}

// Buttons a test run holds on the first pad (tools/run_pcsx2.py's --press writes it through PCSX2's PINE, debug.cpp at a frame of
// --at-frame, nothing else does): the report's button word as the game reads it, pressed 1 (0x800 start, 0x100 select, 0x1000
// up, 0x2000 right, 0x4000 down, 0x8000 left, 0x10 triangle, 0x20 circle, 0x40 cross, 0x80 square, 0x1 L2, 0x2 R2, 0x4 L1, 0x8
// R1). The pressure sensitive ones are pressed fully, the game reads them by their pressure
volatile u32 g_TestPadButtons = 0;

namespace
{
// The report's pressure bytes of the low byte's buttons (L2, R2, L1, R1, triangle, circle, cross, square) and the directions (up,
// right, down, left)
constexpr u8 ButtonPressures[8] = {18, 19, 16, 17, 12, 13, 14, 15};
constexpr u8 DirectionPressures[4] = {10, 8, 11, 9};

void HoldTestButtons(u8* data)
{
    u32 held = g_TestPadButtons;
    // The report's buttons are 0 when pressed
    data[2] &= ~(held >> 8);
    data[3] &= ~held;
    for (u32 bit = 0; bit < 8; bit++)
    {
        if ((held & 1u << bit) != 0)
        {
            data[ButtonPressures[bit]] = 0xFF;
        }
    }

    for (u32 bit = 0; bit < 4; bit++)
    {
        if ((held & 0x1000u << bit) != 0)
        {
            data[DirectionPressures[bit]] = 0xFF;
        }
    }
}
}

s32 Platform::Pads::Read(s32 port, s32 slot, u8* data)
{
    s32 read = padRead(port, slot, reinterpret_cast<padButtonStatus*>(data));
    if (g_TestPadButtons != 0 && port == 0 && slot == 0 && read > 0)
    {
        HoldTestButtons(data);
    }

    return read;
}

Platform::Pads::State Platform::Pads::GetState(s32 port, s32 slot)
{
    return static_cast<State>(padGetState(port, slot));
}

Platform::Pads::RequestState Platform::Pads::GetRequestState(s32 port, s32 slot)
{
    return static_cast<RequestState>(padGetReqState(port, slot));
}

s32 Platform::Pads::InfoMode(s32 port, s32 slot, s32 infoMode, s32 index)
{
    return padInfoMode(port, slot, infoMode, index);
}

s32 Platform::Pads::SetMainMode(s32 port, s32 slot, s32 mode, s32 lock)
{
    return padSetMainMode(port, slot, mode, lock);
}

s32 Platform::Pads::InfoActuator(s32 port, s32 slot, s32 actuator, s32 term)
{
    return padInfoAct(port, slot, actuator, term);
}

s32 Platform::Pads::SetActuatorAlign(s32 port, s32 slot, const u8 align[6])
{
    return padSetActAlign(port, slot, reinterpret_cast<const char*>(align));
}

s32 Platform::Pads::SetActuatorDirect(s32 port, s32 slot, const u8 values[6])
{
    return padSetActDirect(port, slot, const_cast<char*>(reinterpret_cast<const char*>(values)));
}

s32 Platform::Pads::InfoPressureMode(s32 port, s32 slot)
{
    return padInfoPressMode(port, slot);
}

s32 Platform::Pads::EnterPressureMode(s32 port, s32 slot)
{
    return padEnterPressMode(port, slot);
}
