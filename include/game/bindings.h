#pragma once

#include "abi.h"
#include "common.h"

struct GamePad;
struct PadButtons;

// An action's buttons: up to four bit numbers of the pad's report (any of them does it), and maybe a modifier, another action whose
// buttons have to be held for it (or not held, without ModifierHeld)
struct ButtonBinding
{
    enum Flags : u32
    {
        CountMask = 0xF,
        HasModifier = 0x10,
        ModifierHeld = 0x20,
        ModifierShift = 6,
        ModifierMask = 0xFF,
    };

    u8 buttons[4];
    u32 flags;
};
CHECK_SIZE(ButtonBinding, 8);

// A binding of the second list's (an axis): buttons as an action's (the pad's axes), its dead zone and the scale past it
// (1 / (1 - deadZone))
struct AxisBinding
{
    ButtonBinding binding;
    f32 deadZone;
    f32 scale;
};
CHECK_SIZE(AxisBinding, 0x10);

// The game's button bindings (no vtable): a list of actions' bindings and a second list
struct ButtonBindings
{
    u8 actionCount;
    u8 axisCount;
    u8 unknown02[2];
    ButtonBinding* actions;
    AxisBinding* axes;

    static ButtonBindings* Construct(ButtonBindings* bindings, u32 axisCount, u32 actionCount) RETAIL(FUN_002b3730);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002b4290);
    // Every binding without buttons
    void Clear() RETAIL(FUN_002b42f8);
    // One more button for the action (which drops its modifier)
    void AddButton(u32 action, u32 button) RETAIL(SetDigitalBind);
    // Whether the action is pressed this frame (onPress) or held
    u32 Has(const PadButtons* pad, u32 action, u32 onPress) RETAIL(FUN_002b3868);
    // How hard the action's buttons are pressed: the first of them that's pressed at all
    f32 Pressure(GamePad* pad, u32 action) RETAIL(FUN_002b39a8);
    // An axis' value: the first of its pad axes past the dead zone, scaled (the modifier as Pressure's)
    f32 AxisValue(GamePad* pad, u32 axis) RETAIL(FUN_002b3b40);
    // A pad axis added to an axis' binding (which drops its modifier), with its dead zone
    void AddAxis(f32 deadZone, u32 axis, u32 padAxis) RETAIL_N32(SetAnalogBind);
};
CHECK_SIZE(ButtonBindings, 0xC);
