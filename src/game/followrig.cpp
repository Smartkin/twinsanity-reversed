#include "game/followcamera.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/characters.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/pads.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/vehicles.h"

EABI_EXPORT(FUN_00142af8, &FollowCameraRig::RollerbrawlYaw);
EABI_EXPORT(FUN_0015e330, &FollowCameraRig::RollerbrawlFrame);

namespace
{
constexpr u32 AxisCount = 5;
constexpr u32 PressureCount = 3;
// What the followers' points move at (the share of the way a second)
constexpr f32 FollowRate = 6.0f;
// The camera's distance: made at, back to (between its ends), its speed (units a second) and Mecha-Bandicoot's
constexpr f32 StartDistance = 8.5f;
constexpr f32 DefaultDistance = 7.5f;
constexpr f32 NearestDistance = 2.0f;
constexpr f32 FarthestDistance = 13.0f;
constexpr f32 DistanceSpeed = 10.0f;
constexpr f32 MechaDistance = 12.0f;
// The probes' ends (first two up and down, the others across): around the target and around the camera, at the low pitch and
// at the high one
constexpr f32 LowTargetSides[4] = {0.5f, -0.5f, Rounded(0.8), Rounded(-0.8)};
constexpr f32 LowCameraSides[4] = {Rounded(0.42), Rounded(-0.42), Rounded(0.42), Rounded(-0.42)};
constexpr f32 HighTargetSides[4] = {1.0f, Rounded(-0.8), Rounded(0.8), Rounded(-0.8)};
constexpr f32 HighCameraSides[4] = {0.5f, -0.5f, 0.5f, -0.5f};
constexpr Vector4 TargetProbeOffset = {0.0f, 0.0f, -0.5f, 1.0f};
constexpr Vector4 CameraProbeOffset = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr f32 PositionerUnused1C8 = 10.0f;
// The Rollerbrawl: the ball's speed its yaw speed grows with (0.02 of the range for each unit a second), the snowball's scale the
// camera backs off and rises past, its distance then and the target box's height
constexpr f32 RollerbrawlTopSpeed = 50.0f;
constexpr f32 RollerbrawlSpeedShare = Rounded(0.02);
constexpr f32 SnowScaleBackingOff = Rounded(0.8);
constexpr f32 RollerbrawlDistance = 8.0f;
constexpr f32 RollerbrawlBoxHeight = 2.0f;
// The Humiliskate: the target box's height, the distance, the field of view and the pitch (degrees), the positioner's turn share
// and own rate
constexpr Vector4 HumiliskateBox = {0.0f, 1.0f, 0.0f, 1.0f};
constexpr f32 HumiliskateDistance = Rounded(2.3);
constexpr f32 HumiliskateFieldOfView = 80.0f;
constexpr f32 HumiliskatePitch = 20.0f;
constexpr f32 HumiliskateTurnShare = Rounded(0.3);
constexpr f32 HumiliskateOwnRate = 25.0f;
// What the character moves at (its flat speed squared) for the camera to count it as moving
constexpr f32 MovingSpeedSquared = 0.5f;
// The walk's yaw speed by the left stick's angle from straight ahead (65536ths of a turn): it grows from 1 to 90 degrees, grows
// on to 135 by half the tilting yaw speed more and falls from 1.3 times it to none at 179 (those spans in radians); and how much
// of the way to that goal it eases each frame
constexpr s32 OneDegree = 0xB6;
constexpr s32 ThreeEighthsTurn = 0x6000;
constexpr s32 AlmostHalfTurn = 0x7F49;
constexpr f32 EightyNineDegrees = 0x1.8da7e4p+0f;
constexpr f32 FortyFourDegrees = 0x1.893012p-1f;
constexpr f32 GrowingOnShare = 0.5f;
constexpr f32 FallingShare = Rounded(1.3);
constexpr f32 WalkYawEasing = Rounded(0.1);
// The walk's yaw speed's share in the air
constexpr f32 AirborneYawShare = 0.5f;
// How long the blenders hold (seconds)
constexpr f32 ShortHold = Rounded(0.1);
constexpr f32 TiltingHold = Rounded(0.2);

// The rig's own parts and the pad rig's bindings destroyed (the pad rig's destructor inline, as retail has it)
void DestroyRigParts(FollowCameraRig* rig)
{
    rig->vtable = g_FollowCameraRigVTable;
    rig->ownPositioner.Destroy(DestroyOnly);
    rig->ownTarget.Destroy(DestroyOnly);
    rig->ownCameraFollower.Destroy(DestroyOnly);
    rig->ownTargetFollower.Destroy(DestroyOnly);
    rig->vtable = g_PadCameraRigVTable;
    rig->bindings.Destroy(DestroyOnly);
}

void SetOwnWay(CameraPointFollower* follower, u32 way)
{
    follower->bits.ownWay = way;
}

void SetSteers(FollowCameraPositioner* positioner, u32 steers)
{
    positioner->bits.steers = steers;
}

CameraLensNode* LensOf(InstanceContext* camera)
{
    return static_cast<CameraLensNode*>(GetGameNode(&camera->nodes, NodeCameraLens));
}
}

void PadCameraRig::Destroy(u32 destroyFlags)
{
    vtable = g_PadCameraRigVTable;
    bindings.Destroy(DestroyOnly);
    CameraRig::Destroy(destroyFlags);
}

void PadCameraRig::LookStick(f32* x, f32* y)
{
    *x = 0.0f;
    *y = 0.0f;
}

FollowCameraRig* FollowCameraRig::Construct(FollowCameraRig* rig)
{
    CameraRig::Construct(rig);
    rig->vtable = g_PadCameraRigVTable;
    ButtonBindings::Construct(&rig->bindings, AxisCount, PressureCount);
    rig->vtable = g_FollowCameraRigVTable;
    CameraPointFollower::Construct(&rig->ownTargetFollower);
    CameraPointFollower::Construct(&rig->ownCameraFollower);
    FollowCameraTarget::Construct(&rig->ownTarget);
    s32 pitch = g_RigStartPitch;
    FollowCameraPositioner::Construct(&rig->ownPositioner, StartDistance, &pitch);
    AngleFrom(&rig->walkYawSpeed, 0.0f, AngleRadians);
    rig->bits.value = 0;
    rig->bits.distanceHolds = 1;
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisLookX, PadAxisRightX);
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisLookY, PadAxisRightY);
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisMoveY, PadAxisLeftY);
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisMoveY, PadAxisDirectionY);
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisMoveX, PadAxisLeftX);
    rig->bindings.AddAxis(ButtonBindings::AxisDeadZone, AxisMoveX, PadAxisDirectionX);
    rig->bindings.AddButton(PressureShoulders, PadR1);
    rig->bindings.AddButton(PressureShoulders, PadR2);
    rig->bindings.AddButton(PressureShoulders, PadL1);
    rig->bindings.AddButton(PressureShoulders, PadL2);
    rig->Assemble();
    rig->ClearInput();
    rig->SetTilts(1);
    return rig;
}

void FollowCameraRig::Destroy(u32 destroyFlags)
{
    DestroyRigParts(this);
    CameraRig::Destroy(destroyFlags);
}

void FollowCameraRig::Prepare(void*, InstanceContext*, InstanceContext* character)
{
    SetOwnWay(&ownTargetFollower, CameraPointFollower::WaySquareRoot);
    ownTargetFollower.ownRate = FollowRate;
    AssignReference(&ownTarget.followed, character);
    ownTarget.boxMin = g_RigTargetBoxLow;
    SetOwnWay(&ownCameraFollower, CameraPointFollower::WayLinear);
    ownCameraFollower.ownRate = FollowRate;
    ownTarget.boxMax = g_RigTargetBoxHigh;

    FollowCameraPositioner& positioner = ownPositioner;
    positioner.lowTargetOffset = TargetProbeOffset;
    positioner.highTargetOffset = TargetProbeOffset;
    positioner.lowCameraOffset = CameraProbeOffset;
    positioner.highCameraOffset = CameraProbeOffset;
    for (u32 side = 0; side < ProbeCount; side++)
    {
        positioner.lowTargetSides[side] = LowTargetSides[side];
        positioner.lowCameraSides[side] = LowCameraSides[side];
        positioner.highTargetSides[side] = HighTargetSides[side];
        positioner.highCameraSides[side] = HighCameraSides[side];
    }

    positioner.unused1C8 = PositionerUnused1C8;
    positioner.pitchPushRate = g_RigPitchPushRate;
    positioner.yawPushRate = g_RigYawPushRate;
    positioner.bits.distanceFollowsPitch = 1;
    s32 shortHold = static_cast<s32>(g_ClockUnitsPerSecond * ShortHold);
    positioner.pitch.inputSpeed = g_RigPitchInputSpeed;
    positioner.pitch.low = g_RigPitchLowest;
    positioner.pitch.high = g_RigPitchHighest;
    positioner.pitch.holdTicks = shortHold;
    positioner.pitch.speed = g_RigPitchSpeed;
    positioner.yaw.inputSpeed = g_RigYawInputSpeed;
    positioner.yaw.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond);
    positioner.yaw.speed = g_RigYawSpeed;
    positioner.distance.speed = DistanceSpeed;
    positioner.distance.low = NearestDistance;
    positioner.distance.high = FarthestDistance;
    positioner.distance.holdTicks = shortHold;
    positioner.distance.inputSpeed = DistanceSpeed;
    positioner.fieldOfView.holdTicks = 0;
    positioner.fieldOfView.speed = g_RigYawSpeed;
    FollowRide(CharacterAgentOf(character));
}

void FollowCameraRig::RestoreDefaults(CharacterAgent* character)
{
    if (character->properties->GetInt(CharacterKindProperty) == CharacterMecha)
    {
        SetMechaView();
        return;
    }

    FollowCameraPositioner& positioner = ownPositioner;
    positioner.distance.high = FarthestDistance;
    positioner.distance.initial = DefaultDistance;
    positioner.distance.low = NearestDistance;
    positioner.distance.bits.holds = bits.distanceHolds;
    positioner.pitch.bits.holds = bits.pitchHolds;
    positioner.yaw.bits.holds = bits.yawHolds;
    positioner.pitch.initial = g_RigStartPitch;
    SetTilts(bits.tilts);
    bits.vehicle = 0;
}

void FollowCameraRig::Assemble()
{
    targetFollower = &ownTargetFollower;
    cameraFollower = &ownCameraFollower;
    target = &ownTarget;
    positioner = &ownPositioner;
}

void FollowCameraRig::ClearInput()
{
    for (u32 axis = 0; axis < AxisCount; axis++)
    {
        axes[axis] = 0.0f;
    }

    for (u32 pressure = 0; pressure < PressureCount; pressure++)
    {
        pressures[pressure] = 0.0f;
    }
}

void FollowCameraRig::ReadPad(GamePad* pad)
{
    for (u32 axis = 0; axis < AxisCount; axis++)
    {
        axes[axis] = bindings.AxisValue(pad, axis);
    }

    pressures[PressurePitch] = bindings.Pressure(pad, PressurePitch);
    pressures[PressureShoulders] = bindings.Pressure(pad, PressureShoulders);
    pressures[PressureDistance] = bindings.Pressure(pad, PressureDistance);
}

void FollowCameraRig::Frame(TimeClock*, CharacterAgent* character)
{
    FollowCameraPositioner& positioner = ownPositioner;
    Vehicle* vehicle = character->vehicle;
    if (character->state.dead != 0)
    {
        positioner.bits.skipsViewCheck = 1;
    }

    // The target follows the vehicle's exit matrix (read as a place's matrix), the character on foot
    ownTarget.fixedPlace = vehicle != nullptr ? reinterpret_cast<ObjectPlace*>(&vehicle->exitMatrix) : nullptr;
    ownTarget.groundHeight = character->groundPoint.y;
    ownTarget.boxShare = positioner.pitch.share;
    positioner.probeFloor = character->groundPoint.y;
    if (positioner.bits.alwaysTakesValues == 0)
    {
        positioner.pitch.input = axes[AxisLookY];
        positioner.yaw.input = -axes[AxisLookX];
        positioner.distance.input = -axes[AxisZoom];
    }
    else
    {
        positioner.pitch.input = 0.0f;
        positioner.yaw.input = 0.0f;
        positioner.distance.input = 0.0f;
    }

    bool still = __builtin_fabsf(axes[AxisLookY]) <= Epsilon && __builtin_fabsf(axes[AxisLookX]) <= Epsilon &&
                 __builtin_fabsf(axes[AxisZoom]) <= Epsilon;
    if (still)
    {
        positioner.bits.stickTurning = 0;
        ownTarget.bits.unused10 = 0;
        bits.stickTurning = 0;
    }
    else
    {
        positioner.bits.stickTurning = 1;
        ownTarget.bits.unused10 = 1;
        bits.stickTurning = 1;
    }

    CheckMoving(character, still && positioner.bits.yawSpeedSet == 0);
    positioner.pitch.rateScale = pressures[PressurePitch];
    if (bits.moving == 0)
    {
        positioner.yaw.rateScale = pressures[PressureShoulders];
    }

    positioner.distance.rateScale = pressures[PressureDistance];
    FollowRide(character);
    s32 rideTurn;
    AngleFrom(&rideTurn, character->rideTurn, AngleRadians);
    positioner.yawExtra = rideTurn;
    IgnoreLinked(character);
}

void FollowCameraRig::LookStick(f32* x, f32* y)
{
    *x = axes[AxisLookX];
    *y = axes[AxisLookY];
}

void FollowCameraRig::SetTilts(u32 tilts)
{
    FollowCameraPositioner& positioner = ownPositioner;
    bits.tilts = tilts;
    positioner.SetTilts(tilts);
    positioner.fieldOfView.bits.secondRange = 0;
    positioner.pitch.bits.secondRange = 0;
    positioner.yaw.bits.secondRange = 0;
    positioner.distance.bits.secondRange = 0;
    ownTarget.ClearBox();
    // Retail writes the yaw's hold twice: the argument's bit, then its own yawHolds over it
    positioner.yaw.bits.sineSpeed = 0;
    positioner.yaw.bits.holds = tilts;
    positioner.yaw.bits.holds = bits.yawHolds;
    if (tilts != 0)
    {
        positioner.yaw.speed = g_RigTiltYawSpeed;
        positioner.yaw.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond * TiltingHold);
    }
    else
    {
        positioner.yaw.speed = g_RigYawSpeed;
        positioner.yaw.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond);
    }

    positioner.pitch.bits.holds = tilts;
    if (tilts != 0)
    {
        positioner.pitch.holdTicks = 0;
        positioner.pitch.speed = g_RigTiltPitchSpeed;
    }
    else
    {
        positioner.pitch.speed = g_RigPitchSpeed;
        positioner.pitch.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond * ShortHold);
    }

    if (tilts == 0)
    {
        return;
    }

    positioner.ResetTurnShare();
    positioner.ResetOwnRate();
    positioner.bits.noFacingTilt = 0;
    positioner.fieldOfView.high = g_DefaultFov;
    positioner.fieldOfView.low = g_DefaultFov;
    positioner.distance.high = FarthestDistance;
    positioner.distance.low = NearestDistance;
    positioner.pitch.high = g_RigPitchHighest;
    positioner.pitch.low = g_RigPitchLowest;
    ownTarget.boxMin = g_RigTargetBoxLow;
    ownTarget.boxMax = g_RigTargetBoxHigh;
}

void FollowCameraRig::FollowRide(CharacterAgent* character)
{
    // The vehicle's velocity when it rides one, else its own
    Vector4 velocity;
    if (character->MovingVelocity(&velocity) == 0)
    {
        if (bits.tilts != 0 && bits.vehicle != 0)
        {
            if (character->properties->GetInt(CharacterKindProperty) == CharacterMecha)
            {
                SetMechaView();
            }
            else
            {
                SetTilts(1);
                ownTarget.facesMovement = 0;
            }
        }

        UpdateHolds();
        StepWalkYaw(character);
        bits.vehicle = 0;
        ownTarget.velocity = velocity;
        return;
    }

    switch (character->vehicle->Kind())
    {
    case Vehicle::KindRollerbrawl:
        if (bits.vehicle != Vehicle::KindRollerbrawl)
        {
            SetRollerbrawlView();
        }

        UpdateHolds();
        RollerbrawlFrame(character,
                         __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z));
        break;
    case Vehicle::KindHumiliskate:
    {
        SetHumiliskateView();
        Vehicle* vehicle = character->vehicle;
        UpdateHolds();
        HumiliskateFrame(vehicle);
        break;
    }
    case Vehicle::KindHoverboard:
        if (bits.vehicle != Vehicle::KindHoverboard)
        {
            SetHoverboardView();
        }

        UpdateHolds();
        StepWalkYaw(character);
        HoverboardFrame(character);
        break;
    default:
        // Retail bug: the kind is kept in 3 bits, so a character riding along (kind 8) never matches it and is set tilting again
        // every frame
        if (bits.vehicle != character->vehicle->Kind() && bits.tilts != 0)
        {
            SetTilts(1);
        }

        StepWalkYaw(character);
        ownTarget.facesMovement = 0;
        break;
    }

    bits.vehicle = character->vehicle->Kind();
    ownTarget.velocity = velocity;
}

void FollowCameraRig::SetMechaView()
{
    FollowCameraPositioner& positioner = ownPositioner;
    positioner.pitch.high = g_MechaPitch;
    positioner.pitch.low = g_MechaPitch;
    positioner.distance.high = MechaDistance;
    positioner.distance.low = MechaDistance;
    positioner.fieldOfView.high = g_MechaFieldOfView;
    positioner.fieldOfView.low = g_MechaFieldOfView;
    ownTarget.boxMin = g_MechaTargetBox;
    ownTarget.boxMax = g_MechaTargetBox;
}

void FollowCameraRig::RollerbrawlYaw(CharacterAgent*, f32 speed)
{
    // Retail bug: past its top speed the ball's speed is taken as the most yaw speed in radians (3.49) instead of 50, so the
    // camera swings at about 37 degrees a second instead of 200
    if (RollerbrawlTopSpeed < speed)
    {
        speed = static_cast<f32>(g_RollerbrawlYawSpeedMost) * AngleToRadians;
    }

    s32 range = g_RollerbrawlYawSpeedRange;
    s32 extra = *MultiplyAngle(&range, speed * RollerbrawlSpeedShare);
    ownPositioner.yaw.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond * TiltingHold);
    ownPositioner.yaw.speed = extra + g_RollerbrawlYawSpeedLeast;
}

void FollowCameraRig::SetRollerbrawlView()
{
    FollowCameraPositioner& positioner = ownPositioner;
    ownTarget.facesMovement = 1;
    bits.tilts = 1;
    positioner.SetTilts(1);
    positioner.yaw.bits.sineSpeed = 1;
    positioner.yaw.bits.holds = 1;
    positioner.yaw.holdTicks = static_cast<s32>(g_ClockUnitsPerSecond * TiltingHold);
    positioner.yaw.speed = g_RigTiltYawSpeed;
    positioner.pitch.bits.holds = 1;
    positioner.pitch.holdTicks = 0;
    positioner.pitch.speed = g_RigTiltPitchSpeed;
    positioner.bits.noFacingTilt = 1;
    positioner.distance.bits.secondRange = 1;
    positioner.distance.secondHigh = RollerbrawlDistance;
    positioner.distance.secondLow = RollerbrawlDistance;
    positioner.ResetTurnShare();
}

void FollowCameraRig::SetHumiliskateView()
{
    FollowCameraPositioner& positioner = ownPositioner;
    ownTarget.facesMovement = 0;
    bits.tilts = 1;
    positioner.SetTilts(1);
    positioner.yaw.bits.sineSpeed = 1;
    positioner.yaw.bits.holds = 1;
    positioner.pitch.bits.holds = 1;
    positioner.yaw.speed = g_HumiliskateYawSpeed;
    positioner.yaw.holdTicks = 0;
    positioner.pitch.holdTicks = 0;
    positioner.pitch.speed = g_RigTiltPitchSpeed;
    positioner.bits.noFacingTilt = 1;
    positioner.turnShare = HumiliskateTurnShare;
    positioner.ownRate = HumiliskateOwnRate;
    s32 lowFieldOfView;
    s32 highFieldOfView;
    s32 lowPitch;
    s32 highPitch;
    AngleFrom(&lowFieldOfView, HumiliskateFieldOfView, AngleDegrees);
    AngleFrom(&highFieldOfView, HumiliskateFieldOfView, AngleDegrees);
    AngleFrom(&lowPitch, HumiliskatePitch, AngleDegrees);
    AngleFrom(&highPitch, HumiliskatePitch, AngleDegrees);
    ownTarget.boxMin = HumiliskateBox;
    ownTarget.boxMax = HumiliskateBox;
    positioner.distance.high = HumiliskateDistance;
    positioner.distance.low = HumiliskateDistance;
    positioner.pitch.high = highPitch;
    positioner.fieldOfView.low = lowFieldOfView;
    positioner.fieldOfView.high = highFieldOfView;
    positioner.pitch.low = lowPitch;
}

void FollowCameraRig::StepWalkYaw(CharacterAgent* character)
{
    FollowCameraPositioner& positioner = ownPositioner;
    if (positioner.bits.yawSpeedSet != 0)
    {
        return;
    }

    AngleBlender& yaw = positioner.yaw;
    if (!(__builtin_fabsf(yaw.rateScale) <= Epsilon))
    {
        // The shoulder buttons swing it behind the character
        yaw.speed = g_RigYawSpeed * 2;
        return;
    }

    Vector4 velocity;
    character->MovingVelocity(&velocity);
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    s32 goal;
    AngleFrom(&goal, 0.0f, AngleRadians);
    if (yaw.bits.secondRange != 0)
    {
        walkYawSpeed = goal;
        return;
    }

    f32 scale = static_cast<CreaturePart*>(character->part)->flags.onGround != 0 ? 1.0f : AirborneYawShare;
    f32 stickY = axes[AxisMoveY];
    f32 stickX = axes[AxisMoveX];
    if (stickY != 0.0f || stickX != 0.0f)
    {
        // The left stick's angle from straight ahead (either way)
        Vector4 stick = {stickX, 0.0f, stickY, 1.0f};
        s32 angle;
        YawOfDirection(&angle, &stick);
        angle = WrapAngle(angle);
        AngleFrom(&angle, __builtin_fabsf(static_cast<f32>(angle) * AngleToRadians), AngleRadians);
        if (angle >= OneDegree && angle < QuarterTurnAngle)
        {
            s32 past = angle - OneDegree;
            f32 share = static_cast<f32>(*DivideAngle(&past, EightyNineDegrees)) * AngleToRadians;
            goal = static_cast<s32>(static_cast<f32>(g_RigTiltYawSpeed) * share);
        }
        else if (angle >= QuarterTurnAngle && angle <= ThreeEighthsTurn)
        {
            s32 past = angle - QuarterTurnAngle;
            f32 share = static_cast<f32>(*DivideAngle(&past, QuarterPi)) * AngleToRadians;
            s32 half = g_RigTiltYawSpeed;
            goal = static_cast<s32>(static_cast<f32>(*MultiplyAngle(&half, GrowingOnShare)) * share) + g_RigTiltYawSpeed;
        }
        else if (angle > ThreeEighthsTurn && angle < AlmostHalfTurn)
        {
            s32 past = angle - ThreeEighthsTurn;
            f32 share = static_cast<f32>(*DivideAngle(&past, FortyFourDegrees)) * AngleToRadians;
            s32 peak = g_RigTiltYawSpeed;
            goal = static_cast<s32>(static_cast<f32>(*MultiplyAngle(&peak, FallingShare)) * (1.0f - share));
        }
    }

    s32 eased = static_cast<s32>(static_cast<f32>(goal - walkYawSpeed) * WalkYawEasing) + walkYawSpeed;
    s32 yawSpeed = eased;
    yawSpeed = *MultiplyAngle(&yawSpeed, speed);
    yaw.speed = *MultiplyAngle(&yawSpeed, scale);
    walkYawSpeed = eased;
}

void FollowCameraRig::UpdateHolds()
{
    FollowCameraPositioner& positioner = ownPositioner;
    if (bits.stickTurning != 0)
    {
        positioner.yaw.bits.holds = 0;
        positioner.pitch.bits.holds = 0;
        positioner.bits.stickTurned = 1;
        bits.stickTurned = 1;
    }
    else if (bits.moving != 0)
    {
        positioner.yaw.bits.holds = 1;
        positioner.pitch.bits.holds = 1;
        positioner.bits.stickTurned = 0;
        bits.stickTurned = 0;
    }
    else if (bits.stickTurned == 0)
    {
        positioner.pitch.bits.holds = 1;
        positioner.yaw.bits.holds = 0;
        positioner.bits.stickTurned = 0;
    }

    if (positioner.bits.unpushed == 0)
    {
        positioner.pitch.bits.holds = 0;
        positioner.yaw.bits.holds = 0;
    }
}

void FollowCameraRig::SetTarget(InstanceContext* instance)
{
    ownTarget.smoothed = 0;
    ownTarget.bits.heightUneased = 1;
    ownTarget.bits.cut = 1;
    ownTarget.bits.stepped = 0;
    AssignReference(&ownTarget.followed, instance);
    ownPositioner.state.cut = 1;
    ownPositioner.state.placed = 0;
    ownPositioner.bits.keepsHeight = 1;
    ownPositioner.smoothed = 0;
}

void FollowCameraRig::CheckMoving(CharacterAgent* character, u32)
{
    Vector4 velocity;
    character->MovingVelocity(&velocity);
    if (velocity.x * velocity.x + velocity.z * velocity.z < MovingSpeedSquared)
    {
        ownPositioner.bits.characterMoving = 0;
        bits.moving = 0;
    }
    else
    {
        ownPositioner.bits.characterMoving = 1;
        bits.moving = 1;
    }
}

void FollowCameraRig::RollerbrawlFrame(CharacterAgent* character, f32 speed)
{
    RollerbrawlYaw(character, speed);
    f32 excess = static_cast<RollerbrawlVehicle*>(character->vehicle)->snowScale - SnowScaleBackingOff;
    if (0.0f < excess)
    {
        ownPositioner.distance.bits.secondRange = 1;
        Vector4 min = {0.0f, excess + RollerbrawlBoxHeight, 0.0f, 1.0f};
        Vector4 max = min;
        ownPositioner.distance.secondHigh = excess + RollerbrawlDistance;
        ownPositioner.distance.secondLow = excess + RollerbrawlDistance;
        ownTarget.SetBox(&min, &max);
    }
    else
    {
        ownPositioner.distance.bits.secondRange = 0;
    }

    ownPositioner.bits.noFacingTilt = 1;
}

void FollowCameraRig::SetHoverboardView()
{
    ownTarget.facesMovement = 1;
    ownPositioner.bits.noFacingTilt = 1;
}

void FollowCameraRig::HoverboardFrame(CharacterAgent*)
{
}

void FollowCameraRig::HumiliskateFrame(Vehicle*)
{
    ownPositioner.bits.noFacingTilt = 1;
}

void FollowCameraRig::IgnoreLinked(CharacterAgent* character)
{
    if (character->link != nullptr)
    {
        ownPositioner.ignored = character->link->Second()->instance;
    }
    else
    {
        ownPositioner.ignored = nullptr;
    }
}

void FollowCameraRig::Restart()
{
    AssembleVirtual();
    ownTarget.Reset();
    ownPositioner.Restart();
    ownTargetFollower.Take(&ownTarget.rotation, &ownTarget.point);
    ownCameraFollower.Take(&ownPositioner.rotation, &ownPositioner.position);
}

FollowCamera* ConstructFollowCamera(FollowCamera* follow)
{
    FollowCameraRig::Construct(&follow->rig);
    follow->bits.value = 0;
    follow->bits.smoothed = 1;
    follow->bits.steers = 1;
    follow->bits.stepsRig = 1;
    return follow;
}

void DestroyFollowCamera(FollowCamera* follow, u32 destroyFlags)
{
    DestroyRigParts(&follow->rig);
    follow->rig.CameraRig::Destroy(DestroyOnly);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(follow);
    }
}

void RestartFollowCamera(FollowCamera* follow, void* controller, InstanceContext* camera, InstanceContext* character)
{
    CameraLensNode* lens = LensOf(camera);
    follow->bits.value &= FollowCameraBits::KeptByRestart;
    follow->current = nullptr;
    follow->pending = nullptr;
    follow->last = nullptr;
    follow->secondSlot = nullptr;
    if (character->chunk != nullptr)
    {
        MoveToChunk(character->chunk, camera);
    }

    follow->rig.Prepare(controller, camera, character);
    follow->lensRig = lens->SetRig(&follow->rig, 1);
}

void RestoreFollowCameraDefaults(FollowCamera* follow, CharacterAgent* character)
{
    follow->rig.RestoreDefaults(character);
    SetSteers(&follow->rig.ownPositioner, follow->bits.steers);
}

void ShowFollowCamera(FollowCamera* follow, InstanceContext* camera, InstanceContext* character, u32 reset)
{
    CameraLensNode* lens = LensOf(camera);
    if (follow->bits.rigAway == 0)
    {
        follow->lensRig = lens->SetRig(&follow->rig, reset);
    }

    auto* node = static_cast<AgentNode*>(GetGameNode(&character->nodes, NodeCharacter));
    static_cast<PadCameraRig*>(follow->lensRig)->RestoreDefaultsVirtual(static_cast<CharacterAgent*>(node->agent));
    TakeChosenCamera(follow);
    follow->lensRig->trigger = follow->current;
}

void PutFollowCameraOnLens(FollowCamera* follow, InstanceContext* camera)
{
    CameraLensNode* lens = LensOf(camera);
    if (follow->bits.rigAway == 0)
    {
        follow->lensRig = lens->SetRig(&follow->rig, 1);
    }
}

void SetFollowCameraSmoothed(FollowCamera* follow, u32 smoothed)
{
    follow->bits.smoothed = smoothed;
    CameraRig& rig = follow->rig;
    rig.bits.smoothed = smoothed;
}

void SetFollowCameraSteers(FollowCamera* follow, u32 steers)
{
    follow->bits.steers = steers;
    SetSteers(&follow->rig.ownPositioner, steers);
}

void SetFollowCameraIgnoresTriggers(FollowCamera* follow, u32 ignores)
{
    CameraRig& rig = follow->rig;
    rig.bits.ignoresTrigger = ignores;
    follow->bits.ignoresTriggers = ignores;
}

void TakeChosenCamera(FollowCamera* follow)
{
    follow->priority = 0;
    follow->current = follow->pending;
    follow->pending = nullptr;
}

void OfferCamera(FollowCamera* follow, CameraNode* trigger, u32 priority, CharacterAgent* character)
{
    if (follow->pending != nullptr && follow->priority >= priority &&
        (priority != follow->priority || follow->current != trigger))
    {
        return;
    }

    if (FollowCameraTakes(follow, trigger, character) != 0)
    {
        follow->priority = priority;
        follow->pending = trigger;
    }
}

void FollowCameraReadPad(FollowCamera* follow, GamePad* pad)
{
    follow->bits.switchBack = 0;
    static_cast<PadCameraRig*>(follow->lensRig)->ReadPadVirtual(pad);
}

u32 FollowCameraTakes(FollowCamera* follow, CameraNode* trigger, CharacterAgent* character)
{
    MainCamera* camera = trigger->camera;
    u32 takes = 1;
    if (camera->flags.needsRunningCamera != 0 && follow->last == nullptr)
    {
        takes = 0;
    }

    if (camera->switches.secondSlot != 0 && follow->bits.characterDied == 0)
    {
        takes = 0;
    }

    u8 group = camera->group;
    if (group != 0 && follow->last != nullptr)
    {
        u8 lastGroup = follow->last->camera->group;
        if (lastGroup != 0 && lastGroup != group)
        {
            takes = 0;
        }
    }

    // On foot a new camera needs the character on the ground or a restart, unless it says it doesn't
    if (takes != 0 && character->vehicle == nullptr && trigger != follow->last &&
        camera->flags.ignoresPlayerState == 0 &&
        static_cast<CreaturePart*>(character->part)->flags.onGround == 0 &&
        follow->rig.ownPositioner.bits.restarted == 0)
    {
        takes = 0;
    }

    if (takes != 0)
    {
        // A keyed camera (the second subtype) is taken until it finished (retail returns its finished byte xor 1)
        CameraSubtype* keyed = camera->second;
        if (keyed != nullptr && keyed->TypeVirtual() == CameraSubtype::TypeKeyed)
        {
            takes = static_cast<KeyedCamera*>(keyed)->finished ^ 1;
        }
    }

    if (takes == 0 || camera->switches.secondSlot == 0)
    {
        follow->secondSlot = nullptr;
        return takes;
    }

    follow->secondSlot = trigger;
    return takes;
}

void StepFollowCamera(FollowCamera* follow, TimeClock* clock, CharacterAgent* character, InstanceContext* camera)
{
    MainCamera* chosen = follow->pending != nullptr ? follow->pending->camera : nullptr;
    u32 allowsSwitchBack = chosen != nullptr ? chosen->flags.allowsSwitchBack != 0 : 1;
    if (follow->pending != follow->last)
    {
        follow->rig.bits.vehicle = 0;
    }

    if (character->state.dead != 0 && follow->secondSlot != nullptr)
    {
        follow->pending = follow->secondSlot;
        follow->bits.keepsTarget = 1;
    }

    // Nothing sets switchBack, so this switch back never runs
    if (allowsSwitchBack != 0 && follow->bits.switchBack != 0)
    {
        constexpr f32 SwitchBackSeconds = 0.5f;
        CameraLensNode* lens = LensOf(camera);
        follow->bits.rigAway ^= 1;
        follow->lensRig = lens->BlendTo(&follow->rig, static_cast<s32>(g_ClockUnitsPerSecond * SwitchBackSeconds), CurveEven);
        follow->bits.switchBack = 0;
    }

    if (follow->bits.rigAway != 0)
    {
        follow->pending = nullptr;
    }
    else
    {
        follow->lensRig->trigger = follow->pending;
    }

    follow->lensRig->bits.smoothed = follow->bits.smoothed;
    follow->lensRig->bits.ignoresTrigger = follow->bits.ignoresTriggers;
    if (follow->bits.stepsRig != 0)
    {
        static_cast<PadCameraRig*>(follow->lensRig)->FrameVirtual(clock, character);
    }

    if (follow->bits.characterDied == 0)
    {
        follow->bits.characterDied = character->state.dead;
    }

    follow->last = follow->pending;
}
