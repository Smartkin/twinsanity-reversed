#include "game/bindings.h"

#include "game/math.h"
#include "game/memory.h"
#include "game/menus.h"
#include "game/pads.h"

EABI_EXPORT(SetAnalogBind, &ButtonBindings::AddAxis);

namespace
{
// A binding as allocated: no buttons, no modifier (its count is left for Clear)
void ResetBinding(ButtonBinding* binding)
{
    binding->buttons[3] = 0;
    binding->buttons[2] = 0;
    binding->buttons[1] = 0;
    binding->buttons[0] = 0;
    binding->flags.modifierHeld = 0;
    binding->flags.hasModifier = 0;
    binding->flags.modifier = 0;
}

u32 Bit(u8 button)
{
    return 1u << (button & ShiftMask);
}

bool AnyHeld(const ButtonBinding& binding, u32 buttons)
{
    for (u32 button = 0; button < binding.flags.count; button++)
    {
        if ((buttons & Bit(binding.buttons[button])) != 0)
        {
            return true;
        }
    }

    return false;
}

const ButtonBinding& ModifierOf(const ButtonBindings* bindings, const ButtonBinding& binding)
{
    return bindings->actions[binding.flags.modifier];
}
}

ButtonBindings* ButtonBindings::Construct(ButtonBindings* bindings, u32 axisCount, u32 actionCount)
{
    ButtonBinding* newActions = nullptr;
    if (actionCount != 0)
    {
        newActions = static_cast<ButtonBinding*>(MemoryAllocate2(actionCount * sizeof(ButtonBinding)));
        for (u32 action = 0; action < actionCount; action++)
        {
            ResetBinding(&newActions[action]);
        }
    }

    bindings->actions = newActions;
    AxisBinding* newAxes = nullptr;
    if (axisCount != 0)
    {
        newAxes = static_cast<AxisBinding*>(MemoryAllocate2(axisCount * sizeof(AxisBinding)));
        for (u32 axis = 0; axis < axisCount; axis++)
        {
            ResetBinding(&newAxes[axis].binding);
        }
    }

    bindings->actionCount = static_cast<u8>(actionCount);
    bindings->axisCount = static_cast<u8>(axisCount);
    bindings->axes = newAxes;
    bindings->Clear();
    return bindings;
}

void ButtonBindings::Destroy(u32 destroyFlags)
{
    if (actions != nullptr)
    {
        MemoryDeallocate_(actions);
    }

    if (axes != nullptr)
    {
        MemoryDeallocate_(axes);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ButtonBindings::Clear()
{
    for (u32 action = 0; action < actionCount; action++)
    {
        actions[action].flags.count = 0;
    }

    for (u32 axis = 0; axis < axisCount; axis++)
    {
        axes[axis].binding.flags.count = 0;
    }
}

void ButtonBindings::AddButton(u32 action, u32 button)
{
    ButtonBinding& binding = actions[action];
    u32 count = binding.flags.count;
    binding.flags.hasModifier = 0;
    binding.flags.count = count + 1;
    binding.buttons[count] = static_cast<u8>(button);
}

u32 ButtonBindings::Has(const PadButtons* pad, u32 action, u32 onPress)
{
    const ButtonBinding& binding = actions[action];
    bool pressed = false;
    for (u32 button = 0; button < binding.flags.count; button++)
    {
        u32 bit = Bit(binding.buttons[button]);
        bool down = onPress != 0 ? (pad->previous & bit) == 0 && (pad->now & bit) != 0 : (pad->now & bit) != 0;
        if (down)
        {
            pressed = true;
            break;
        }
    }

    if (binding.flags.hasModifier == 0)
    {
        return pressed ? 1 : 0;
    }

    bool held = AnyHeld(ModifierOf(this, binding), pad->now);
    if (!pressed)
    {
        return 0;
    }

    return binding.flags.modifierHeld != 0 ? held : !held;
}

f32 ButtonBindings::Pressure(GamePad* pad, u32 action)
{
    const ButtonBinding& binding = actions[action];
    f32 value = 0.0f;
    for (u32 button = 0; button < binding.flags.count; button++)
    {
        f32 pressure = GetButtonPressure(pad, binding.buttons[button]);
        if (!(__builtin_fabsf(pressure) <= Epsilon) && __builtin_fabsf(value) <= Epsilon)
        {
            value = pressure;
        }
    }

    if (binding.flags.hasModifier == 0)
    {
        return value;
    }

    bool held = AnyHeld(ModifierOf(this, binding), pad->now);
    if (binding.flags.modifierHeld != 0)
    {
        return held ? value : 0.0f;
    }

    return held ? 0.0f : value;
}

f32 ButtonBindings::AxisValue(GamePad* pad, u32 axis)
{
    const AxisBinding& binding = axes[axis];
    f32 deadZone = binding.deadZone;
    f32 scale = binding.scale;
    f32 value = 0.0f;
    for (u32 index = 0; index < binding.binding.flags.count; index++)
    {
        f32 raw = GetPadAxis(pad, binding.binding.buttons[index]);
        f32 past = 0.0f;
        if (raw < 0.0f)
        {
            if (raw < -deadZone)
            {
                past = (raw + deadZone) * scale;
            }
        }
        else if (deadZone < raw)
        {
            past = (raw - deadZone) * scale;
        }

        if (!(__builtin_fabsf(past) <= Epsilon) && __builtin_fabsf(value) <= Epsilon)
        {
            value = past;
        }
    }

    ButtonBindingFlags flags = binding.binding.flags;
    if (flags.hasModifier == 0)
    {
        return value;
    }

    bool held = AnyHeld(ModifierOf(this, binding.binding), pad->now);
    if (flags.modifierHeld != 0)
    {
        return held ? value : 0.0f;
    }

    return held ? 0.0f : value;
}

void ButtonBindings::AddAxis(f32 deadZone, u32 axis, u32 padAxis)
{
    AxisBinding& binding = axes[axis];
    u32 count = binding.binding.flags.count;
    binding.binding.flags.hasModifier = 0;
    binding.binding.flags.count = count + 1;
    binding.binding.buttons[count] = static_cast<u8>(padAxis);
    binding.deadZone = deadZone;
    binding.scale = 1.0f / (1.0f - deadZone);
}
