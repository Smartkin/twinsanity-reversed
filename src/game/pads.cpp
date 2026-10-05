#include "game/pads.h"
#include "game/memory.h"
#include "platform/graphics.h"
#include "platform/pads.h"
#include "retail/libc.h"

namespace
{
enum SetupState : s32
{
    SetupStart = 0,
    SetupCountModes = 1,
    SetupLockAnalog = 2,
    SetupWaitLock = 3,
    SetupActuators = 4,
    SetupWaitActuators = 5,
    SetupPressure = 6,
    SetupEnterPressure = 7,
    SetupWaitPressure = 8,
    SetupDone = 9,
};

// Frames to wait before looking for a pad again when none answers (for every pad), and the frames the controller waits for pad 0
// to be set up when it's made
constexpr s32 ReconnectDelay = 60;
constexpr s32 SetupFrames = 60;
// The information cleared when it's made: up to 12 bytes past the report
constexpr u32 ClearedInformation = 0x5A0;
// The driver's memory's alignment
constexpr u32 DriverAlignment = 64;
// The actuators: the small motor's and the big one's (their values the motors'), and none
constexpr u32 SmallMotor = 0;
constexpr u32 BigMotor = 1;
constexpr u8 NoActuator = 0xFF;
// The sticks' raw values' dead zone and range
constexpr s32 StickDeadZone = 0x20;
constexpr s32 StickRange = 0xFF;
// A button is down when it's pressed further
constexpr f32 ButtonDown = Rounded(0.05);
// The report's bytes (Platform::Pads::ReportByte: the buttons' high byte is GamePadInformation::buttons' high byte)
using namespace Platform::Pads;
// The report's sticks' middle
constexpr u8 StickRawMiddle = 0x80;
// The report's buttons with none pressed (each is 0 while it's pressed)
constexpr u32 ReportButtonsUp = 0xFFFF;

constexpr f32 PerPressure = Rounded(1.0 / 255.0);
constexpr f32 StickMiddle = 127.5f;
constexpr f32 PerStick = Rounded(1.0 / 127.5);

// The flags the setup sets cleared
void ClearSetUp(GamePadInformation* info)
{
    info->flags.analog = 0;
    info->flags.pressure = 0;
    info->flags.read = 0;
}

void ResetSetup(GamePadInformation* info)
{
    info->setupState = SetupStart;
    info->buttons.value = 0;
    ClearSetUp(info);
}

// A stick's raw value past a dead zone of 32, stretched back to the full range
s32 PastDeadZone(s32 value)
{
    if (value > 0)
    {
        return value < StickDeadZone ? 0 : (value - StickDeadZone) * StickRange / (StickRange - StickDeadZone);
    }

    return value < -(StickDeadZone - 1) ? (value + StickDeadZone) * StickRange / (StickRange - StickDeadZone) : 0;
}

// Scaled to the axis's scale plus its dead zone, then the dead zone taken off towards 0
f32 ScaleStick(s32 value, f32 scale, f32 deadZone)
{
    return static_cast<f32>(value) * (scale + deadZone) * (1.0f / 128.0f);
}

f32 TakeDeadZone(f32 value, f32 deadZone)
{
    if (value > 0.0f)
    {
        value -= deadZone;
        if (value < 0.0f)
        {
            value = 0.0f;
        }
    }

    if (value < 0.0f)
    {
        value += deadZone;
        if (value > 0.0f)
        {
            value = 0.0f;
        }
    }

    return value;
}

f32 Pressure(const GamePad* pad, u32 index)
{
    return static_cast<f32>(pad->info->report[index]) * PerPressure;
}

f32 Stick(const GamePad* pad, u32 index)
{
    return (static_cast<f32>(pad->info->report[index]) - StickMiddle) * PerStick;
}

// A stick's direction as a button: full past three quarters, from a quarter on doubled
f32 StickDirection(f32 value)
{
    if (value > 0.75f)
    {
        return 1.0f;
    }

    if (value > 0.25f)
    {
        f32 past = value - 0.25f;
        return past + past;
    }

    return 0.0f;
}

f32 Digital(u32 pressed)
{
    return pressed != 0 ? 1.0f : 0.0f;
}

// Two pressures as an axis: pressed at all is half way, full from a quarter's pressure on
f32 PressureAxis(f32 positive, f32 negative)
{
    if (positive > 0.0f)
    {
        return positive <= 0.25f ? positive + positive + 0.5f : 1.0f;
    }

    if (negative > 0.0f)
    {
        return negative <= 0.25f ? -0.5f - (negative + negative) : -1.0f;
    }

    return 0.0f;
}

// The buttons' pressures above ButtonDown, as the game's buttons
void ReadButtons(GamePad* pad)
{
    for (u32 button = 0; button < PadButtonCount; button++)
    {
        if (GetButtonPressure(pad, static_cast<u8>(button)) > ButtonDown)
        {
            pad->now |= 1u << button;
        }
    }
}
}

extern "C"
{
    // Frames until the pads are looked for again
    extern s32 g_PadReconnectDelay RETAIL(D_0030AA94);
    extern const GccVTableEntry g_GamePadControllerVTable[] RETAIL(GamePadController_Methods);
}

EABI_EXPORT(FUN_002b2bc8, &GamePadController::Update);

GamePadController* GamePadController::Construct(void* memory, s32 padCount)
{
    GamePadController* controller = static_cast<GamePadController*>(memory);
    controller->flags.pads = 0;
    controller->vtable = g_GamePadControllerVTable;
    controller->flags.paused = 0;
    controller->flags.vibration = 1;
    for (s32 i = 0; i < static_cast<s32>(MaxPads); i++)
    {
        if (i < padCount)
        {
            GamePad* pad = static_cast<GamePad*>(MemoryAllocate(sizeof(GamePad)));
            pad->now = 0;
            pad->previous = 0;
            GamePadInformation* info = CreateGamePadInfo(i & 1, i >> 1);
            controller->pads[i] = pad;
            pad->info = info;
            controller->flags.value |= 1u << i;
        }
        else
        {
            controller->pads[i] = nullptr;
        }

        VibrationBits& bits = g_VibrationRequests[i].bits;
        bits.pad = 0;
        bits.smallMotor = 0;
        bits.bigMotor = 0;
    }

    for (s32 frame = 0; frame < SetupFrames; frame++)
    {
        Platform::Graphics::WaitVSync();
        GamePad* pad = controller->pads[0];
        if (pad->info != nullptr)
        {
            pad->previous = pad->now;
            pad->now = 0;
            if (UpdatePad(pad->info) != 0)
            {
                ReadButtons(pad);
            }
        }
    }

    return controller;
}

void GamePadController::Nothing()
{
}

void GamePadController::Nothing2()
{
}

// Reads the pads and plays their vibration requests. A request's motors run until its time is up, then stop. The retail code
// checks pad 0's bit for every pad that couldn't be read
u32 GamePadController::Update(f32 seconds)
{
    u32 result = 1;
    for (s32 i = 0; i < static_cast<s32>(MaxPads); i++)
    {
        GamePad* pad = pads[i];
        if (pad == nullptr)
        {
            continue;
        }

        bool read = false;
        if (pad->info != nullptr)
        {
            pad->previous = pad->now;
            pad->now = 0;
            if (UpdatePad(pad->info) != 0)
            {
                ReadButtons(pad);
                read = true;
            }
        }

        if (!read)
        {
            if ((flags.pads & 1) != 0)
            {
                result = 0;
            }

            continue;
        }

        VibrationRequest* request = &g_VibrationRequests[i];
        if (request->bits.pad == 0 || flags.paused != 0)
        {
            continue;
        }

        if (request->seconds > 0.0f)
        {
            request->seconds -= seconds;
        }
        else
        {
            // Its time is up: the motors stopped
            request->bits.pad = 0;
            request->bits.smallMotor = 0;
            request->bits.bigMotor = 0;
            request->bits.pending = 1;
        }

        VibrationBits bits = request->bits;
        if (bits.pending != 0 && flags.vibration != 0)
        {
            SetPadMotors(pad->info, bits.smallMotor, bits.bigMotor);
            request->bits.pending = 0;
        }
    }

    return result;
}

void GamePadController::Destroy(u32 destroyFlags)
{
    vtable = g_GamePadControllerVTable;
    for (u32 i = 0; i < MaxPads; i++)
    {
        GamePad* pad = pads[i];
        if (pad != nullptr)
        {
            if (pad->info != nullptr)
            {
                DestroyGamePadInfo(pad->info);
            }

            MemoryDeallocate2_(pad);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GamePadController::Pause()
{
    for (u32 i = 0; i < MaxPads; i++)
    {
        if (pads[i] != nullptr && g_VibrationRequests[i].bits.pad != 0)
        {
            SetPadMotors(pads[i]->info, 0, 0);
        }
    }

    flags.paused = 1;
}

void GamePadController::Resume()
{
    flags.paused = 0;
}

void GamePadController::DisableVibration()
{
    for (u32 i = 0; i < MaxPads; i++)
    {
        if (pads[i] != nullptr && g_VibrationRequests[i].bits.pad != 0)
        {
            SetPadMotors(pads[i]->info, 0, 0);
        }
    }

    flags.vibration = 0;
}

extern "C"
{
    GamePadInformation* CreateGamePadInfo(s32 port, s32 slot)
    {
        GamePadInformation* info = static_cast<GamePadInformation*>(MemoryAllocate2(sizeof(GamePadInformation)));
        if (info == nullptr)
        {
            return nullptr;
        }

        RetailLibc::MemorySet(info, 0, ClearedInformation);
        info->port = port;
        info->slot = slot;
        info->rightScaleY = 1.0f;
        info->rightDeadZoneY = Rounded(0.1);
        info->setupState = SetupStart;
        info->leftScaleX = 1.0f;
        info->leftDeadZoneX = Rounded(0.1);
        info->leftScaleY = 1.0f;
        info->leftDeadZoneY = Rounded(0.1);
        info->rightScaleX = 1.0f;
        info->rightDeadZoneX = Rounded(0.1);
        for (u32 i = 0; i < sizeof(info->actuatorValues); i++)
        {
            info->actuatorValues[i] = 0;
            info->actuatorAlign[i] = NoActuator;
        }

        // The small motor's actuator 0, the big one's 1
        info->actuatorAlign[SmallMotor] = SmallMotor;
        info->actuatorAlign[BigMotor] = BigMotor;
        void* driverMemory =
            reinterpret_cast<void*>((reinterpret_cast<u32>(info->driverMemory) + DriverAlignment - 1) & ~(DriverAlignment - 1));
        if (Platform::Pads::Open(info->port, info->slot, driverMemory) == 0)
        {
            MemoryDeallocate2_(info);
            return nullptr;
        }

        return info;
    }

    void DestroyGamePadInfo(GamePadInformation* info)
    {
        Platform::Pads::Close(info->port, info->slot);
        MemoryDeallocate2_(info);
    }

    // The setup: find the mode; a digital pad gets its analog mode locked on; an analog one gets its motors and then its
    // pressure sensitive mode; then it's read every frame, until its mode changes or it's unplugged. Only a pad with both
    // (FlagAnalog and FlagPressure) is read
    s32 UpdatePad(GamePadInformation* info)
    {
        using namespace Platform::Pads;
        s32 state = 0;
        if (g_PadReconnectDelay > 0)
        {
            g_PadReconnectDelay--;
            ResetSetup(info);
        }
        else
        {
            state = GetState(info->port, info->slot);
            if (state < StateFindingCtp1 || state == StateError || state == StateFindingCtp1)
            {
                ResetSetup(info);
                state = 0;
                g_PadReconnectDelay = ReconnectDelay;
            }
        }

        if (state != 0)
        {
            switch (info->setupState)
            {
            case SetupStart:
                ClearSetUp(info);
                info->modeId = InfoMode(info->port, info->slot, InfoCurrentId, 0);
                if (info->modeId != 0)
                {
                    s32 extendedMode = InfoMode(info->port, info->slot, InfoCurrentExtendedId, 0);
                    if (extendedMode > 0)
                    {
                        info->modeId = extendedMode;
                    }

                    if (info->modeId == ModeIdDigital)
                    {
                        info->setupState = SetupCountModes;
                    }
                    else if (info->modeId == ModeIdAnalog)
                    {
                        info->setupState = SetupActuators;
                    }
                    else
                    {
                        info->setupState = SetupDone;
                    }
                }

                break;
            case SetupCountModes:
                if (InfoMode(info->port, info->slot, InfoModeTable, ModeTableLength) == 0)
                {
                    ResetSetup(info);
                }
                else
                {
                    info->setupState = SetupLockAnalog;
                }

                break;
            case SetupLockAnalog:
                if (SetMainMode(info->port, info->slot, MainModeAnalog, ModeLocked) == 1)
                {
                    info->setupState = SetupWaitLock;
                }

                break;
            case SetupWaitLock:
                if (GetRequestState(info->port, info->slot) == RequestFailed)
                {
                    info->setupState = SetupLockAnalog;
                }

                if (GetRequestState(info->port, info->slot) == RequestComplete)
                {
                    info->setupState = SetupStart;
                }

                break;
            case SetupActuators:
                if (InfoActuator(info->port, info->slot, CountActuators, 0) == 0)
                {
                    info->setupState = SetupDone;
                }
                else if (SetActuatorAlign(info->port, info->slot, info->actuatorAlign) != 0)
                {
                    info->setupState = SetupWaitActuators;
                }

                break;
            case SetupWaitActuators:
                if (GetRequestState(info->port, info->slot) == RequestFailed)
                {
                    info->setupState = SetupActuators;
                }

                if (GetRequestState(info->port, info->slot) == RequestComplete)
                {
                    info->setupState = SetupPressure;
                }

                break;
            case SetupPressure:
                info->flags.analog = 1;
                info->setupState = InfoPressureMode(info->port, info->slot) == 1 ? SetupEnterPressure : SetupDone;
                if (info->setupState == SetupDone)
                {
                    ResetSetup(info);
                }

                break;
            case SetupEnterPressure:
                if (EnterPressureMode(info->port, info->slot) == 1)
                {
                    info->setupState = SetupWaitPressure;
                }

                break;
            case SetupWaitPressure:
                if (GetRequestState(info->port, info->slot) == RequestFailed)
                {
                    info->setupState = SetupEnterPressure;
                }

                if (GetRequestState(info->port, info->slot) == RequestComplete)
                {
                    info->flags.pressure = 1;
                    info->setupState = SetupDone;
                }

                break;
            default:
            {
                if (info->flags.analog == 0 || info->flags.pressure == 0 || (state != StateStable && state != StateFindingCtp1))
                {
                    ResetSetup(info);
                    break;
                }

                u8 lastMode = info->report[ReportMode];
                if (Read(info->port, info->slot, info->report) <= 0)
                {
                    info->report[ReportButtonsHigh] = 0;
                    info->report[ReportButtonsLow] = 0;
                }

                u32 buttons =
                    (static_cast<u32>(info->report[ReportButtonsHigh]) << 8 | info->report[ReportButtonsLow]) ^ ReportButtonsUp;
                u32 changed = buttons ^ info->lastButtons.value;
                u32 released = info->lastButtons.value & changed;
                info->lastButtons.value = buttons;
                info->buttons.value = buttons;
                info->flags.released = released;
                info->pressed.value = buttons & changed;
                if (info->flags.analog != 0)
                {
                    info->leftY = StickRawMiddle - info->report[StickLeftY];
                    info->rightX = info->report[StickRightX] - StickRawMiddle;
                    info->rightY = StickRawMiddle - info->report[StickRightY];
                    info->leftX = info->report[StickLeftX] - StickRawMiddle;
                    if (info->flags.stickDeadZones != 0)
                    {
                        info->leftX = PastDeadZone(info->leftX);
                        info->leftY = PastDeadZone(info->leftY);
                        info->rightX = PastDeadZone(info->rightX);
                        info->rightY = PastDeadZone(info->rightY);
                    }

                    info->leftStickX = ScaleStick(info->leftX, info->leftScaleX, info->leftDeadZoneX);
                    info->leftStickY = ScaleStick(info->leftY, info->leftScaleY, info->leftDeadZoneY);
                    info->leftStickX = TakeDeadZone(info->leftStickX, info->leftDeadZoneX);
                    info->leftStickY = TakeDeadZone(info->leftStickY, info->leftDeadZoneY);
                    info->rightStickX = ScaleStick(info->rightX, info->rightScaleX, info->rightDeadZoneX);
                    info->rightStickY = ScaleStick(info->rightY, info->rightScaleY, info->rightDeadZoneY);
                    info->rightStickX = TakeDeadZone(info->rightStickX, info->rightDeadZoneX);
                    info->rightStickY = TakeDeadZone(info->rightStickY, info->rightDeadZoneY);
                }
                else
                {
                    info->report[StickLeftX] = StickRawMiddle;
                    info->leftStickX = 0.0f;
                    info->report[StickRightY] = StickRawMiddle;
                    info->report[StickRightX] = StickRawMiddle;
                    info->report[StickLeftY] = StickRawMiddle;
                    info->rightY = 0;
                    info->rightX = 0;
                    info->leftY = 0;
                    info->leftX = 0;
                    info->leftStickY = 0.0f;
                }

                info->flags.read = 1;
                // Its mode changed: set it up again
                if (lastMode != 0 && info->report[ReportMode] != lastMode)
                {
                    ResetSetup(info);
                }

                break;
            }
            }
        }

        if (info->flags.read == 0)
        {
            info->report[StickRightX] = StickRawMiddle;
            info->rightStickX = 0.0f;
            info->lastButtons.value = 0;
            info->flags.released = 0;
            info->pressed.value = 0;
            info->buttons.value = 0;
            info->rightY = 0;
            info->rightX = 0;
            info->leftY = 0;
            info->leftX = 0;
            info->leftStickY = 0.0f;
            info->leftStickX = 0.0f;
            info->rightStickY = 0.0f;
            info->report[StickLeftY] = StickRawMiddle;
            info->report[StickLeftX] = StickRawMiddle;
            info->report[StickRightY] = StickRawMiddle;
            for (u32 i = PressureRight; i <= PressureR2; i++)
            {
                info->report[i] = 0;
            }
        }

        return info->flags.read != 0 ? -1 : 0;
    }

    s32 SetPadMotors(GamePadInformation* info, s32 small, u8 big)
    {
        if (info->flags.read == 0)
        {
            return 0;
        }

        if (info->flags.analog == 0)
        {
            return small > 0;
        }

        info->actuatorValues[SmallMotor] = small > 0;
        info->actuatorValues[BigMotor] = big;
        return Platform::Pads::SetActuatorDirect(info->port, info->slot, info->actuatorValues);
    }

    f32 GetButtonPressure(GamePad* pad, u8 button)
    {
        switch (button)
        {
        case PadUp:
            return Pressure(pad, PressureUp);
        case PadDown:
            return Pressure(pad, PressureDown);
        case PadLeft:
            return Pressure(pad, PressureLeft);
        case PadRight:
            return Pressure(pad, PressureRight);
        case PadTriangle:
            return Pressure(pad, PressureTriangle);
        case PadCross:
            return Pressure(pad, PressureCross);
        case PadCircle:
            return Pressure(pad, PressureCircle);
        case PadSquare:
            return Pressure(pad, PressureSquare);
        case PadLeftStickUp:
            return StickDirection(-Stick(pad, StickLeftY));
        case PadLeftStickDown:
            return StickDirection(Stick(pad, StickLeftY));
        case PadLeftStickLeft:
            return StickDirection(-Stick(pad, StickLeftX));
        case PadLeftStickRight:
            return StickDirection(Stick(pad, StickLeftX));
        case PadRightStickUp:
            return StickDirection(-Stick(pad, StickRightY));
        case PadRightStickDown:
            return StickDirection(Stick(pad, StickRightY));
        case PadRightStickLeft:
            return StickDirection(-Stick(pad, StickRightX));
        case PadRightStickRight:
            return StickDirection(Stick(pad, StickRightX));
        case PadL1:
            return Pressure(pad, PressureL1);
        case PadL2:
            return Pressure(pad, PressureL2);
        case PadL3:
            return Digital(pad->info->buttons.l3);
        case PadR1:
            return Pressure(pad, PressureR1);
        case PadR2:
            return Pressure(pad, PressureR2);
        case PadR3:
            return Digital(pad->info->buttons.r3);
        case PadStart:
            return Digital(pad->info->buttons.start);
        case PadSelect:
            return Digital(pad->info->buttons.select);
        default:
            return 0.0f;
        }
    }

    f32 GetPadAxis(GamePad* pad, u32 axis)
    {
        switch (axis)
        {
        case PadAxisLeftX:
            return Stick(pad, StickLeftX);
        case PadAxisLeftY:
            return -Stick(pad, StickLeftY);
        case PadAxisRightX:
            return Stick(pad, StickRightX);
        case PadAxisRightY:
            return -Stick(pad, StickRightY);
        case PadAxisDirectionX:
            return PressureAxis(Pressure(pad, PressureRight), Pressure(pad, PressureLeft));
        case PadAxisDirectionY:
            return PressureAxis(Pressure(pad, PressureUp), Pressure(pad, PressureDown));
        default:
            return 0.0f;
        }
    }

    bool GetButtonState(GamePad* pad, u32 button, bool pressedNow)
    {
        u32 mask = 1u << (button & ShiftMask);
        if (pressedNow)
        {
            return (pad->previous & mask) == 0 && (pad->now & mask) != 0;
        }

        return (pad->now & mask) != 0;
    }

    bool IsPadRead(GamePad* pad)
    {
        if (pad->info == nullptr)
        {
            return false;
        }

        return pad->info->flags.read != 0;
    }

    void RequestVibration(const VibrationRequest* request)
    {
        VibrationRequest* slot = &g_VibrationRequests[request->bits.pad - 1];
        *slot = *request;
        slot->bits.pending = 1;
    }

    void InitialiseVibrationRequests(s32 initialise, s32 priority)
    {
        if (priority != DefaultInitPriority || initialise == 0)
        {
            return;
        }

        for (u32 i = 0; i < MaxPads; i++)
        {
            g_VibrationRequests[i].seconds = 0.0f;
            VibrationBits& bits = g_VibrationRequests[i].bits;
            bits.pad = 0;
            bits.pending = 0;
            bits.smallMotor = 0;
            bits.bigMotor = 0;
        }
    }

    void ConstructPadsModule()
    {
        InitialiseVibrationRequests(1, DefaultInitPriority);
    }
}
