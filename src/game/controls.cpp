#include "game/controls.h"

#include "game/agents.h"
#include "game/chunkdata.h"
#include "game/followcamera.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/pads.h"
#include "game/place.h"

#include <cstdint>

EABI_EXPORT(FUN_00162da8, &ControlsHandler::Move);

namespace
{
constexpr u32 AxisCount = 2;
constexpr u32 ActionCount = 7;

CharacterAgent* CharacterOf(InstanceContext* instance)
{
    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter))->agent);
}

// Whether the follow camera's rig isn't the lens's (the pad then drives nothing): its bits read as retail does, through a null
// follow node too (the word at 0x30 then)
bool RigAway(InstanceContext* instance)
{
    void* follow = GetGameNode(&instance->nodes, NodeFollow);
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(follow) + offsetof(FollowNode, camera);
    return reinterpret_cast<const FollowCameraBits*>(address + offsetof(FollowCamera, bits))->rigAway != 0;
}

// An instance's place as the retail code reads it, also when there's no instance (the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

void ClearMove(CharacterAgent* character)
{
    Vector4 none = g_DefaultBox.min;
    none.w = 1.0f;
    character->moveInput = none;
}

bool IsNone(const Vector4* vector)
{
    return __builtin_fabsf(vector->x) <= Epsilon && __builtin_fabsf(vector->y) <= Epsilon &&
           __builtin_fabsf(vector->z) <= Epsilon;
}

Vector4 Cross(const Vector4* a, const Vector4* b)
{
    return {a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x, 1.0f};
}

// A rotation's inverse (InvertRotation inline)
void Invert(Vector4* rotation)
{
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, rotation);
    rotation->w = rotation->w * inverse;
    rotation->x = rotation->x * -inverse;
    rotation->y = rotation->y * -inverse;
    rotation->z = rotation->z * -inverse;
}

// The rotation of the frame of a direction (made unit) about an up axis: its rows the side (up x direction), the up and the
// direction
void FrameRotation(Vector4* rotation, const Vector4* direction, const Vector4* up)
{
    f32 inverse = InverseLength(direction, LengthEpsilon);
    Vector4 unit = {direction->x * inverse, direction->y * inverse, direction->z * inverse, 1.0f};
    Matrix4x4 frame;
    *RowOf(&frame, 0) = Cross(up, &unit);
    *RowOf(&frame, 1) = *up;
    *RowOf(&frame, 2) = unit;
    GetRotationVec(rotation, &frame);
}
}

ControlsHandler* ControlsHandler::Construct(ControlsHandler* handler, u32 axisCount, u32 actionCount)
{
    handler->vtable = g_ControlsHandlerVTable;
    ButtonBindings::Construct(&handler->bindings, axisCount, actionCount);
    handler->bindings.AddButton(ActionCross, PadCross);
    handler->bindings.AddButton(ActionSquare, PadSquare);
    handler->bindings.AddButton(ActionCircle, PadCircle);
    handler->bindings.AddButton(ActionL, PadL1);
    handler->bindings.AddButton(ActionL, PadL2);
    handler->bindings.AddButton(ActionR, PadR1);
    handler->bindings.AddButton(ActionR, PadR2);
    handler->bindings.AddButton(ActionL2, PadL2);
    handler->bindings.AddButton(ActionStart, PadStart);
    return handler;
}

void ControlsHandler::Destroy(u32 destroyFlags)
{
    vtable = g_ControlsHandlerVTable;
    bindings.Destroy(DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ControlsHandler::StickInWorld(ObjectPlace* place, const Matrix4x4* view, const Vector4* stick, Vector4* out)
{
    f32 length = __builtin_sqrtf(stick->x * stick->x + stick->y * stick->y + stick->z * stick->z);
    // Retail compares the view's up with the character's against 0.707 either way and does nothing with it
    Vector4 up = *RowOf(&place->matrix, 1);
    Vector4 world;
    VuRotateVector(view, stick, &world);
    f32 inverse = 1.0f / Kept(length);
    world.x = world.x * inverse;
    world.z = world.z * inverse;
    world.y = world.y * inverse;
    Vector4 side = Cross(&up, &world);
    Vector4 level = Cross(&side, &up);
    out->w = 1.0f;
    out->x = level.x * length;
    out->z = level.z * length;
    out->y = level.y * length;
}

void ControlsHandler::TurnToward(const Vector4* direction, const Vector4* up, const Vector4* forward, Vector4* turn)
{
    Vector4 toward;
    Vector4 from;
    FrameRotation(&toward, direction, up);
    FrameRotation(&from, forward, up);
    *turn = toward;
    Invert(&from);
    MultiplyRotations(turn, turn, &from);
}

void ControlsHandler::GiveTurn(Vector4* turn, const Vector4* move, AgentNode* node)
{
    f32 moveX = move->x;
    f32 moveZ = move->z;
    f32 length = Kept(__builtin_sqrtf(moveX * moveX + move->y * move->y + moveZ * moveZ));
    CharacterButtons& buttons = static_cast<CharacterAgent*>(node->agent)->buttons;
    Vector4 axis;
    s32 angle;
    AxisAngleOfRotation(turn, &axis, &angle, 0);
    angle = WrapAngle(angle);
    // The turn as a share of half a turn, signed by the way its axis points
    f32 share = static_cast<f32>(angle) * AngleToRadians * (axis.y < 0.0f ? -InversePi : InversePi);
    if (1.0f < length)
    {
        moveZ = moveZ / length;
        moveX = moveX / length;
    }

    buttons.turn = buttons.locked.turn != 0 ? 0.0f : share;
    buttons.moveZ = buttons.locked.moveZ != 0 ? 0.0f : moveZ;
    buttons.moveX = buttons.locked.moveX != 0 ? 0.0f : moveX;
}

void ControlsHandler::Move(f32 length, InstanceContext* instance, const Vector4* direction)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    auto* character = static_cast<CharacterAgent*>(node->agent);
    Vector4 turn;
    TurnToward(direction, RowOf(&place->matrix, 1), RowOf(&place->matrix, 2), &turn);
    Vector4 ahead = {0.0f, 0.0f, length, 1.0f};
    Vector4 move;
    // Retail bug: this first move is overwritten (the call only normalizes the turn)
    RotateByQuaternion(&turn, &ahead, &move, 0);
    Vector4 rotation;
    GetRotationVec(&rotation, &place->matrix);
    MultiplyRotations(&rotation, &rotation, &turn);
    RotateByQuaternion(&rotation, &ahead, &move, 0);
    GiveTurn(&turn, &move, node);
    character->moveInput = move;
}

void ControlsHandler::MoveBy(TimeClock*, const Vector4* velocity, InstanceContext* instance)
{
    if (IsNone(velocity))
    {
        ClearMove(CharacterOf(instance));
        return;
    }

    CharacterAgent* character = CharacterOf(instance);
    Vector4 direction = *velocity;
    f32 share = 1.0f / character->WalkTopSpeed();
    direction.x = direction.x * share;
    direction.y = direction.y * share;
    direction.z = direction.z * share;
    f32 length = __builtin_sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    Move(length, instance, &direction);
}

void ControlsHandler::ReadButtons(GamePad* pad, InstanceContext* instance)
{
    if (RigAway(instance))
    {
        return;
    }

    CharacterButtons& buttons = CharacterOf(instance)->buttons;
    f32 cross = bindings.Pressure(pad, ActionCross);
    f32 circle = bindings.Pressure(pad, ActionCircle);
    f32 square = bindings.Pressure(pad, ActionSquare);
    f32 left = bindings.Pressure(pad, ActionL);
    f32 right = bindings.Pressure(pad, ActionR);
    buttons.cross = buttons.locked.cross != 0 ? 0.0f : cross;
    buttons.square = buttons.locked.square != 0 ? 0.0f : square;
    buttons.circle = buttons.locked.circle != 0 ? 0.0f : circle;
    buttons.shoulders = buttons.locked.shoulders != 0 ? 0.0f : right - left;
}

CharacterControls* CharacterControls::Construct(CharacterControls* handler)
{
    ControlsHandler::Construct(handler, AxisCount, ActionCount);
    handler->vtable = g_CharacterControlsVTable;
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisX, PadAxisLeftX);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisX, PadAxisDirectionX);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisY, PadAxisLeftY);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisY, PadAxisDirectionY);
    handler->Reset();
    return handler;
}

void CharacterControls::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterControlsVTable;
    ControlsHandler::Destroy(destroyFlags);
}

void CharacterControls::Reset()
{
}

void CharacterControls::Frame(TimeClock*, GamePad* pad, InstanceContext* instance)
{
    if (RigAway(instance))
    {
        return;
    }

    Vector4 stick = {bindings.AxisValue(pad, AxisX), 0.0f, bindings.AxisValue(pad, AxisY), 1.0f};
    if (IsNone(&stick))
    {
        ClearMove(CharacterOf(instance));
        return;
    }

    ObjectPlace* place = instance->place;
    const Matrix4x4* view = &instance->chunk->matrix;
    RotateAndTranslate(place);
    f32 length = __builtin_sqrtf(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);
    Vector4 world;
    StickInWorld(place, view, &stick, &world);
    Move(length, instance, &world);
}

VehicleControls* VehicleControls::Construct(VehicleControls* handler, u32 stickIsCross)
{
    ControlsHandler::Construct(handler, AxisCount, ActionCount);
    handler->stickIsCross = stickIsCross;
    handler->vtable = g_VehicleControlsVTable;
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisX, PadAxisLeftX);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisX, PadAxisDirectionX);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisY, PadAxisLeftY);
    handler->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisY, PadAxisDirectionY);
    handler->Reset();
    return handler;
}

void VehicleControls::Destroy(u32 destroyFlags)
{
    vtable = g_VehicleControlsVTable;
    ControlsHandler::Destroy(destroyFlags);
}

void VehicleControls::Reset()
{
}

void VehicleControls::Frame(TimeClock*, GamePad* pad, InstanceContext* instance)
{
    f32 x = bindings.AxisValue(pad, AxisX);
    f32 y = bindings.AxisValue(pad, AxisY);
    CharacterButtons& buttons = CharacterOf(instance)->buttons;
    f32 cross = bindings.Pressure(pad, ActionCross);
    f32 circle = bindings.Pressure(pad, ActionCircle);
    f32 square = bindings.Pressure(pad, ActionSquare);
    if (stickIsCross != 0)
    {
        // Retail bug: the stick's y as cross (the hoverboard's) is written over by ReadButtons, which runs right after
        buttons.cross = buttons.locked.cross != 0 ? 0.0f : y;
        y = 0.0f;
    }
    else
    {
        buttons.cross = buttons.locked.cross != 0 ? 0.0f : cross;
    }

    buttons.square = buttons.locked.square != 0 ? 0.0f : square;
    buttons.circle = buttons.locked.circle != 0 ? 0.0f : circle;
    buttons.locked.cross = 0;
    buttons.locked.square = 0;
    buttons.locked.circle = 0;
    if (x == 0.0f && y == 0.0f)
    {
        // The move left as it was
        return;
    }

    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, NodeFollow));
    InstanceContext* camera =
        follow->cameraInstance != nullptr ? static_cast<InstanceContext*>(follow->cameraInstance->object) : nullptr;
    ObjectPlace* cameraPlace = RetailPlaceOf(camera);
    ObjectPlace* place = instance->place;
    RotateAndTranslate(cameraPlace);
    RotateAndTranslate(place);
    // Retail builds a frame from the camera's up and the character's forward that nothing uses: only its calls are left
    Vector4 forward = *RowOf(&place->matrix, 2);
    Matrix4x4 transposed;
    TransposeMatrix(&cameraPlace->matrix, 3, &transposed);
    VuRotateVector(&transposed, &forward, &forward);

    // The stick along the camera's level side and ahead
    Vector4 side = *RowOf(&cameraPlace->matrix, 0);
    Vector4 ahead = *RowOf(&cameraPlace->matrix, 2);
    side.y = 0.0f;
    ahead.y = 0.0f;
    f32 inverse = InverseLength(&side, LengthEpsilon);
    side.x = side.x * inverse;
    side.y = side.y * inverse;
    side.z = side.z * inverse;
    inverse = InverseLength(&ahead, LengthEpsilon);
    ahead.x = ahead.x * inverse;
    ahead.y = ahead.y * inverse;
    ahead.z = ahead.z * inverse;
    f32 moveX = side.x * x + ahead.x * y;
    f32 moveZ = side.z * x + ahead.z * y;
    f32 length = __builtin_sqrtf(moveX * moveX + moveZ * moveZ);
    if (1.0f < length)
    {
        f32 scale = 1.0f / Kept(length);
        moveZ = moveZ * scale;
        moveX = moveX * scale;
    }

    buttons.moveX = buttons.locked.moveX != 0 ? 0.0f : moveX;
    buttons.moveZ = buttons.locked.moveZ != 0 ? 0.0f : moveZ;
}

void StickControls::Destroy(u32 destroyFlags)
{
    vtable = g_ControlsHandlerVTable;
    bindings.Destroy(DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void StickControls::Reset()
{
}

void StickControls::Frame(TimeClock*, GamePad* pad, InstanceContext* instance)
{
    CharacterButtons& buttons = CharacterOf(instance)->buttons;
    f32 x = bindings.AxisValue(pad, AxisX);
    f32 y = bindings.AxisValue(pad, AxisY);
    buttons.turn = buttons.locked.turn != 0 ? 0.0f : x;
    buttons.moveZ = buttons.locked.moveZ != 0 ? 0.0f : y;
}
