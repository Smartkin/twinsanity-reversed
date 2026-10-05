#pragma once

#include "common.h"

// The controllers, by port and slot (multitap), each reporting into a buffer of its own. The interface is a DualShock 2's, the
// PS2's libpad: its states, modes (4 digital, 7 analog), actuators, pressure sensitive mode and report (ReportByte), which
// the game's pad code goes by. Another platform presents its pads as DualShock 2s
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

// What InfoMode tells (libpad's PAD_MODE*): the current mode's ID, its extended ID, the current mode's index in the mode table,
// and the table's mode at the index (the table's length at ModeTableLength)
enum ModeInfo : s32
{
    InfoCurrentId = 1,
    InfoCurrentExtendedId = 2,
    InfoCurrentIndex = 3,
    InfoModeTable = 4,
};

constexpr s32 ModeTableLength = -1;

// The modes' IDs: a digital pad, an analog one (a DualShock's)
enum ModeId : s32
{
    ModeIdDigital = 4,
    ModeIdAnalog = 7,
};

// The main modes SetMainMode sets, and whether the pad's analog button may change it (libpad's PAD_MMODE_*)
enum MainMode : s32
{
    MainModeDigital = 0,
    MainModeAnalog = 1,
};

enum ModeLock : s32
{
    ModeUnlocked = 2,
    ModeLocked = 3,
};

// InfoActuator's actuator for how many there are
constexpr s32 CountActuators = -1;

// Its buffer: 256 bytes, 64 byte aligned
constexpr u32 BufferSize = 256;

// The report's bytes: the mode, the button word (pressed 0, its high byte first), the sticks and the buttons' pressures
enum ReportByte : u32
{
    ReportMode = 1,
    ReportButtonsHigh = 2,
    ReportButtonsLow = 3,
    StickRightX = 4,
    StickRightY = 5,
    StickLeftX = 6,
    StickLeftY = 7,
    PressureRight = 8,
    PressureLeft = 9,
    PressureUp = 10,
    PressureDown = 11,
    PressureTriangle = 12,
    PressureCircle = 13,
    PressureCross = 14,
    PressureSquare = 15,
    PressureL1 = 16,
    PressureR1 = 17,
    PressureL2 = 18,
    PressureR2 = 19,
};

s32 Initialise();
s32 Open(s32 port, s32 slot, void* buffer);
s32 Close(s32 port, s32 slot);
// The buttons and sticks (at most 32 bytes). Returns how many bytes
s32 Read(s32 port, s32 slot, u8* data);
State GetState(s32 port, s32 slot);
RequestState GetRequestState(s32 port, s32 slot);
// The controller's modes
s32 InfoMode(s32 port, s32 slot, ModeInfo info, s32 index);
s32 SetMainMode(s32 port, s32 slot, MainMode mode, ModeLock lock);
// The vibration motors
s32 InfoActuator(s32 port, s32 slot, s32 actuator, s32 term);
s32 SetActuatorAlign(s32 port, s32 slot, const u8 align[6]);
s32 SetActuatorDirect(s32 port, s32 slot, const u8 values[6]);
// Pressure sensitive buttons
s32 InfoPressureMode(s32 port, s32 slot);
s32 EnterPressureMode(s32 port, s32 slot);
}
