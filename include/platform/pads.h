#pragma once

#include "common.h"

// The controllers, by port and slot (multitap), each reporting into a buffer of its own. The interface is a DualShock 2's, the
// PS2's libpad: its states, modes (4 digital, 7 analog), actuators, pressure sensitive mode and report (game/pads.h has its
// layout), which the game's pad code goes by. Another platform presents its pads as DualShock 2s
namespace Platform::Pads
{
enum State : s32
{
    StateDisconnected = 0,
    StateFindingPad = 1,
    StateFindingCtp1 = 2,
    StateExecutingRequest = 4,
    StateStable = 6,
    StateError = 7,
};

enum RequestState : s32
{
    RequestComplete = 0,
    RequestFailed = 1,
    RequestBusy = 2,
};

// Its buffer: 256 bytes, 64 byte aligned
constexpr u32 BufferSize = 256;

s32 Initialise();
s32 Open(s32 port, s32 slot, void* buffer);
s32 Close(s32 port, s32 slot);
// The buttons and sticks (at most 32 bytes). Returns how many bytes
s32 Read(s32 port, s32 slot, u8* data);
State GetState(s32 port, s32 slot);
RequestState GetRequestState(s32 port, s32 slot);
// The controller's modes (infoMode: 1 current ID, 2 current extended ID, 3 current mode's index, 4 the mode table)
s32 InfoMode(s32 port, s32 slot, s32 infoMode, s32 index);
s32 SetMainMode(s32 port, s32 slot, s32 mode, s32 lock);
// The vibration motors
s32 InfoActuator(s32 port, s32 slot, s32 actuator, s32 term);
s32 SetActuatorAlign(s32 port, s32 slot, const u8 align[6]);
s32 SetActuatorDirect(s32 port, s32 slot, const u8 values[6]);
// Pressure sensitive buttons
s32 InfoPressureMode(s32 port, s32 slot);
s32 EnterPressureMode(s32 port, s32 slot);
}
