#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/bindings.h"
#include "game/math.h"

struct AgentNode;
struct GamePad;
struct InstanceContext;
struct ObjectPlace;
struct TimeClock;

// What turns a pad into a playable character's input (the controls node's, kind 0xB, instances.h's ControlsNode): the bindings
// it reads the pad with and its vtable 0xC bytes in (retail's D_002F46D8: 1 the destructor, 2 set back, 3 a frame with the clock
// (never read), the pad and the instance). The node runs its own CharacterControls, or a vehicle's VehicleControls in its place
class ControlsHandler
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ResetSlot = 2,
        FrameSlot = 3,
    };

    enum Action : u32
    {
        ActionCross = 0,
        ActionSquare = 1,
        ActionCircle = 2,
        ActionL = 3,
        ActionR = 4,
        ActionL2 = 5,
        ActionStart = 6,
    };

    enum Axis : u32
    {
        AxisX = 0,
        AxisY = 1,
    };

    ButtonBindings bindings;
    const GccVTableEntry* vtable;

    // The axes and actions handed on to the bindings (2 and 7)
    static ControlsHandler* Construct(ControlsHandler* handler, u32 axisCount, u32 actionCount) RETAIL(FUN_00164e90);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00164f58);

    // The helpers (this never read): the stick (x, 0, y) turned by the chunk's view into the world and made square to the
    // character's up; the rotation turning the character's forward into a direction about its up; the turn and the move into the
    // character's buttons (the turn normalised in place); a move of a length toward a direction; the controls by a velocity (the
    // clock unread); the buttons read
    void StickInWorld(ObjectPlace* place, const Matrix4x4* view, const Vector4* stick, Vector4* out) RETAIL(FUN_00162830);
    void TurnToward(const Vector4* direction, const Vector4* up, const Vector4* forward, Vector4* turn) RETAIL(FUN_001629d0);
    void GiveTurn(Vector4* turn, const Vector4* move, AgentNode* node) RETAIL(FUN_00162c40);
    void Move(f32 length, InstanceContext* instance, const Vector4* direction) RETAIL_N32(FUN_00162da8);
    void MoveBy(TimeClock* clock, const Vector4* velocity, InstanceContext* instance) RETAIL(FUN_001630d8);
    void ReadButtons(GamePad* pad, InstanceContext* instance) RETAIL(FUN_00163270);

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void ResetVirtual()
    {
        CallVirtual<void>(this, vtable, ResetSlot);
    }

    void FrameVirtual(TimeClock* clock, GamePad* pad, InstanceContext* instance)
    {
        CallVirtual<void>(this, vtable, FrameSlot, clock, pad, instance);
    }
};
CHECK_OFFSET(ControlsHandler, vtable, 0xC);
CHECK_SIZE(ControlsHandler, 0x10);

// A character's (retail's D_002F4688): the controls node's own
class CharacterControls : public ControlsHandler
{
public:
    static CharacterControls* Construct(CharacterControls* handler) RETAIL(InitBindings);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00164e60);
    void Reset() RETAIL(FUN_00164e88);
    void Frame(TimeClock* clock, GamePad* pad, InstanceContext* instance) RETAIL(FUN_00162ed8);
};
CHECK_SIZE(CharacterControls, 0x10);

// A vehicle's (retail's D_002F46B0, SetPlayerVehicle's): the stick's y as cross instead of steering (the hoverboard's; ReadButtons
// writes cross's pressure over it right after, so it never shows)
class VehicleControls : public ControlsHandler
{
public:
    u8 stickIsCross;
    u8 unused11[3];

    static VehicleControls* Construct(VehicleControls* handler, u32 stickIsCross) RETAIL(InitVehicleButtonBindings);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00164d78);
    void Reset() RETAIL(FUN_00164da0);
    void Frame(TimeClock* clock, GamePad* pad, InstanceContext* instance) RETAIL(FUN_00162358);
};
CHECK_SIZE(VehicleControls, 0x14);

// A class nothing makes (its vtable at 0x2F4660, 0x50 bytes into D_002F4610's label, has no label of its own): the stick into the
// turn and the move
class StickControls : public ControlsHandler
{
public:
    void Destroy(u32 destroyFlags) RETAIL(FUN_00164fb0);
    void Reset() RETAIL(FUN_00165008);
    void Frame(TimeClock* clock, GamePad* pad, InstanceContext* instance) RETAIL(FUN_00165010);
};

extern "C"
{
    extern const GccVTableEntry g_ControlsHandlerVTable[] RETAIL(D_002F46D8);
    extern const GccVTableEntry g_CharacterControlsVTable[] RETAIL(D_002F4688);
    extern const GccVTableEntry g_VehicleControlsVTable[] RETAIL(D_002F46B0);
}
