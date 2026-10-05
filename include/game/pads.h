#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"

// The controllers: a DualShock 2 per port and slot, set up to its analog, pressure sensitive and vibrating modes, read every
// frame into 24 buttons (the platform layer's pads are DualShock 2s, see platform/pads.h)

// The game's buttons: the pressure sensitive ones, the sticks' directions, and the others
enum PadButton : u32
{
    PadUp = 0,
    PadDown = 1,
    PadLeft = 2,
    PadRight = 3,
    PadTriangle = 4,
    PadCross = 5,
    PadCircle = 6,
    PadSquare = 7,
    PadLeftStickUp = 8,
    PadLeftStickDown = 9,
    PadLeftStickLeft = 10,
    PadLeftStickRight = 11,
    PadRightStickUp = 12,
    PadRightStickDown = 13,
    PadRightStickLeft = 14,
    PadRightStickRight = 15,
    PadL1 = 16,
    PadL2 = 17,
    PadL3 = 18,
    PadR1 = 19,
    PadR2 = 20,
    PadR3 = 21,
    PadStart = 22,
    PadSelect = 23,
    PadButtonCount = 24,
};

// The analog axes, -1 to 1: the sticks, and the directional buttons as a stick
enum PadAxis : u32
{
    PadAxisLeftX = 0,
    PadAxisLeftY = 1,
    PadAxisRightX = 2,
    PadAxisRightY = 3,
    PadAxisDirectionX = 4,
    PadAxisDirectionY = 5,
    PadAxisCount = 6,
};

// The report's buttons (pressed = 1)
union PadReportButtons
{
    u32 value;
    struct
    {
        u32 l2 : 1;
        u32 r2 : 1;
        u32 l1 : 1;
        u32 r1 : 1;
        u32 triangle : 1;
        u32 circle : 1;
        u32 cross : 1;
        u32 square : 1;
        u32 select : 1;
        u32 l3 : 1;
        u32 r3 : 1;
        u32 start : 1;
        u32 up : 1;
        u32 right : 1;
        u32 down : 1;
        u32 left : 1;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(PadReportButtons, 4);

// A pad's flags: the report's buttons released this frame (as PadReportButtons), its analog and pressure sensitive modes set up,
// read this frame, and dead zones of 32 for the sticks' raw values (nothing sets it)
union PadInformationFlags
{
    u64 value;
    struct
    {
        u64 released : 32;
        u64 analog : 1;
        u64 pressure : 1;
        u64 read : 1;
        u64 stickDeadZones : 1;
        u64 unused36 : 28;
    };
};
CHECK_SIZE(PadInformationFlags, 8);

// A pad's driver state and readings. Other code reads its fields as they are
struct GamePadInformation
{
    // The driver's 256 bytes, at the first 64 byte boundary
    u8 driverMemory[0x500];
    s32 port;
    s32 slot;
    // Where its setup is (UpdatePad)
    s32 setupState;
    s32 modeId;
    // The sticks, -128 to 127 (up is positive)
    s32 leftX;
    s32 leftY;
    s32 rightX;
    s32 rightY;
    // The sticks, scaled and past their dead zones
    f32 leftStickX;
    f32 leftStickY;
    f32 leftScaleX;
    f32 leftScaleY;
    f32 leftDeadZoneX;
    f32 leftDeadZoneY;
    f32 rightStickX;
    f32 rightStickY;
    f32 rightScaleX;
    f32 rightScaleY;
    f32 rightDeadZoneX;
    f32 rightDeadZoneY;
    // The motors: the small one on or off and the big one's strength, and which motor each actuator is
    u8 actuatorValues[6];
    u8 actuatorAlign[6];
    // The report's buttons, the last frame's, and the ones pressed since
    PadReportButtons buttons;
    PadReportButtons lastButtons;
    PadReportButtons pressed;
    PadInformationFlags flags;
    u32 unused570;
    // The driver's report: the mode at 1, the buttons at 2 and 3 (pressed = 0: Select, L3, R3, Start, Up, Right, Down, Left, then
    // L2, R2, L1, R1, Triangle, Circle, Cross, Square from the lowest bit), the sticks at 4-7 (right x, y, left x, y, 128 in the
    // middle), the pressures at 8-19 (right, left, up, down, triangle, circle, cross, square, L1, R1, L2, R2)
    u8 report[32];
    u8 unused594[0xDA0 - 0x594];
};
CHECK_SIZE(GamePadInformation, 0xDA0);
CHECK_OFFSET(GamePadInformation, port, 0x500);
CHECK_OFFSET(GamePadInformation, leftStickX, 0x520);
CHECK_OFFSET(GamePadInformation, actuatorValues, 0x550);
CHECK_OFFSET(GamePadInformation, flags, 0x568);
CHECK_OFFSET(GamePadInformation, report, 0x574);

// A controller's buttons down this frame and the last, by PadButton
struct PadButtons
{
    u32 now;
    u32 previous;
};

// The game's pad: its buttons and the pad's information
struct GamePad : PadButtons
{
    GamePadInformation* info;
};
CHECK_SIZE(GamePad, 0xC);

// A vibration request's bits: its pad's index + 1 (0 when there's none), to send to the pad, the small motor on and the big
// motor's strength
union VibrationBits
{
    u32 value;
    struct
    {
        u32 pad : 5;
        u32 pending : 1;
        u32 smallMotor : 1;
        u32 bigMotor : 8;
        u32 unused15 : 17;
    };
};
CHECK_SIZE(VibrationBits, 4);

// A request to vibrate a pad for a time
struct VibrationRequest
{
    VibrationBits bits;
    f32 seconds;
};
CHECK_SIZE(VibrationRequest, 8);

constexpr u32 MaxPads = 8;

// The frame loop's side of the pad controller: its vtable's
class PadControllerInterface
{
public:
    enum Slot : u32
    {
        UpdateSlot = 2,
        DestroySlot = 4,
        PauseSlot = 5,
        ResumeSlot = 6,
    };

    const GccVTableEntry* vtable;

    // Returns 0 when pad 0 is plugged in and a pad couldn't be read
    u32 Update(f32 seconds)
    {
        return CallVirtual<u32>(this, vtable, UpdateSlot, seconds);
    }

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, flags);
    }

    void Pause()
    {
        CallVirtual<void>(this, vtable, PauseSlot);
    }

    void Resume()
    {
        CallVirtual<void>(this, vtable, ResumeSlot);
    }
};

// The pad controller's flags: a bit per pad made, paused (the motors stopped), the vibration on
union PadControllerFlags
{
    u32 value;
    struct
    {
        u32 pads : 8;
        u32 paused : 1;
        u32 vibration : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(PadControllerFlags, 4);

class GamePadController : public PadControllerInterface
{
public:
    GamePad* pads[MaxPads];
    PadControllerFlags flags;

    // Waits 60 frames for pad 0 to be set up
    static GamePadController* Construct(void* memory, s32 padCount) RETAIL(FUN_002b2a10);

    void Nothing() RETAIL(FUN_002b3f00);
    u32 Update(f32 seconds) RETAIL_N32(FUN_002b2bc8);
    void Nothing2() RETAIL(FUN_002b3f28);
    void Destroy(u32 flags) RETAIL(FUN_002b3e60);
    // Stops the motors
    void Pause() RETAIL(FUN_002b3f98);
    void Resume() RETAIL(FUN_002b40c0);
    void DisableVibration() RETAIL(FUN_002b4028);
};
CHECK_SIZE(GamePadController, 0x28);

extern "C"
{
    extern VibrationRequest g_VibrationRequests[MaxPads] RETAIL(D_003C6F00);
    // The game's pad (the game controller's)
    extern GamePad* g_MainGamePad RETAIL(G_MainGamePad);

    GamePadInformation* CreateGamePadInfo(s32 port, s32 slot);
    void DestroyGamePadInfo(GamePadInformation* info) RETAIL(FUN_002b40d8);
    // Steps the pad's setup and reads it. Returns -1 when it was read, 0 when it wasn't
    s32 UpdatePad(GamePadInformation* info) RETAIL(FUN_002b2dd8);
    // Returns whether the small motor was asked for, or what the driver returns
    s32 SetPadMotors(GamePadInformation* info, s32 small, u8 big) RETAIL(FUN_002b4218);

    // How far the button is pressed, 0 to 1
    f32 GetButtonPressure(GamePad* pad, u8 button);
    f32 GetPadAxis(GamePad* pad, u32 axis) RETAIL(FUN_002b27d0);
    // Down, or pressed this frame
    bool GetButtonState(GamePad* pad, u32 button, bool pressedNow);
    bool IsPadRead(GamePad* pad) RETAIL(FUN_002b3e20);

    // Makes the request its pad's
    void RequestVibration(const VibrationRequest* request) RETAIL(FUN_002b3f50);
    // The module's statics (every pad's vibration request emptied) and its global constructor
    void InitialiseVibrationRequests(s32 initialise, s32 priority) RETAIL(FUN_002b3d50);
    void ConstructPadsModule() RETAIL(FUN_002b4448);
}
