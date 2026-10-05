#include "game/vehicles.h"

#include "game/agents.h"
#include "game/collision.h"
#include "game/followcamera.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"

EABI_EXPORT(FUN_0015b4c8, &HoverboardVehicle::Frame);
EABI_EXPORT(FUN_0015a688, &HoverboardVehicle::Tilt);

namespace
{
// The board's body: a mass of 5 in its ellipsoid's box (twice as wide armed), how soft it is (armed 40), no friction but a
// little against spinning and rolling; it bounces 0.6 (armed 0.3)
constexpr f32 BoardMass = 5.0f;
constexpr f32 Softness = 0.5f;
constexpr f32 ArmedSoftness = 40.0f;
constexpr f32 SpinFriction = Rounded(0.002);
constexpr f32 Restitution = Rounded(0.6);
constexpr f32 ArmedRestitution = Rounded(0.3);
// The handling: the ellipsoid's radii, armed (1, 0.7, 1.6) or not (1.4, 0.5, 1.4)
constexpr f32 ArmedRadiusX = 1.0f;
constexpr f32 ArmedRadiusY = 0.7f;
constexpr f32 ArmedRadiusZ = Rounded(1.6);
constexpr f32 RadiusXZ = 1.4f;
constexpr f32 RadiusY = 0.5f;
// The follow camera's own rate: 5 at first, almost none while knocked, eased back
constexpr f32 StartCameraRate = 5.0f;
constexpr f32 KnockedCameraRate = 0.0001f;
constexpr f32 CameraRateKeep = Rounded(0.999);
constexpr f32 CameraRateGain = 0x1.47ae16p-7f;

// Knocks: fading 0.1 a second; a frame's impulses over 150 knock it (unarmed boards spin 5 times faster about y, half as fast
// about the others), armed boards are knocked 0.4 at the most; slot 5's pushes over 0.1 (their strength over the distance
// squared and 4 more, times 4)
constexpr f32 KnockFade = Rounded(0.1);
constexpr f32 HardHit = 150.0f;
constexpr f32 KnockSpin = 5.0f;
constexpr f32 KnockSpinOthers = 0.5f;
constexpr f32 ArmedMostKnocked = Rounded(0.4);
constexpr f32 KnockThreshold = Rounded(0.1);
constexpr f32 KnockScale = 4.0f;

// The drive: the stick rooted times 0.7, pulled toward the board's heading when it keeps it; a thrust of 32, armed boards' less
// the higher above the rest height they hover (by half at the rise height, over the 20.5 between them)
constexpr f32 StickScale = 0.7f;
constexpr f32 Thrust = 32.0f;
constexpr f32 InverseRiseRange = 0x1.8f9c18p-5f;
constexpr f32 RiseThrustLoss = 0.5f;
// The hover: probes 26 down from the board's four corners (half a unit from its middle) through the surfaces solid to objects
// and the solid objects (dynamic scenery, crates, creatures, generic objects and pay gates), a spring toward the hover height
// (3.5; 24 while it can rise and the shoulder or the stick pulled back is held)
// 20 times the error (armed 8.5, a tenth of it pulling down), at a point 0.3 of the way to the corner; a corner over nothing is
// pulled down 200
constexpr s32 MostProbed = 0x20;
constexpr s32 BoardCorners = 4;
constexpr f32 ProbeDepth = 26.0f;
constexpr f32 CornerOffset = 0.5f;
constexpr f32 RestHeight = 3.5f;
constexpr f32 RiseHeight = 24.0f;
constexpr f32 RiseShoulder = Rounded(0.1);
constexpr f32 RiseStick = Rounded(-0.8);
constexpr f32 Spring = 20.0f;
constexpr f32 ArmedSpring = 8.5f;
constexpr f32 ArmedDownShare = Rounded(0.1);
constexpr f32 LeverShare = Rounded(0.3);
constexpr f32 HoverForceShare = 0.25f;
constexpr f32 MissedError = -10.0f;
constexpr f32 MissedPush = -200.0f;
// The tilt: its y axis turned toward up leaned 0.2 along the drive and pitched by the height error (within 3, 0.07 a unit), 40
// times the turn (armed 45), twice as hard past a right angle; the turn toward the drive 14 times (armed 20) once it's driven;
// both times its mass
constexpr f32 TiltLean = Rounded(0.2);
constexpr f32 MostTiltError = 3.0f;
constexpr f32 TiltPitch = Rounded(-0.07);
constexpr f32 TiltStrength = -40.0f;
constexpr f32 ArmedTiltStrength = -45.0f;
constexpr f32 TurnEpsilon = Rounded(0.001);
constexpr f32 TurnStrength = 14.0f;
constexpr f32 ArmedTurnStrength = 20.0f;
// The drags in the board's space: by the speed squared (across 1.4, up 1.5, along 0.015), spins about y 4.2 (the others 2.6);
// times its mass
constexpr f32 DragX = Rounded(-1.4);
constexpr f32 DragY = -1.5f;
constexpr f32 DragZ = -0.015f;
constexpr f32 SpinDragY = -4.2f;
constexpr f32 SpinDragOthers = -2.6f;
// The character stands 0.76 down and 0.15 back along the board
constexpr f32 StandDown = 0.76f;
constexpr f32 StandBack = Rounded(0.15);
// The tow: circle pressed; the nearest body (an instance with a rigid body node) from 0 to 12 below it within a box 3 across
// (and 15 down); springs 2.6 long, 50 stiff and damped 5 between the two
constexpr f32 TowPress = Rounded(0.1);
constexpr s32 MostTowed = 0x20;
constexpr u32 TowedKinds = 1u << NodeRigidBody;
constexpr f32 TowReachAcross = 3.0f;
constexpr f32 TowReachDown = 15.0f;
constexpr f32 TowDepth = 12.0f;
constexpr f32 TowLength = 2.6f;
constexpr f32 TowStiffness = 50.0f;
constexpr f32 TowDamping = 5.0f;

DynamicBody* BodyOf(InstanceContext* instance)
{
    return static_cast<DynamicBody*>(GetGameNode(&instance->nodes, NodeRigidBody));
}

// The instance towed (none without a tow)
InstanceContext* TowedOf(const HoverboardVehicle* vehicle)
{
    return vehicle->towed != nullptr ? static_cast<InstanceContext*>(vehicle->towed->object) : nullptr;
}

// The tow let go
void LetGo(HoverboardVehicle* vehicle)
{
    if (TowedOf(vehicle) != nullptr)
    {
        RemoveReference(&vehicle->towed);
        vehicle->towed = nullptr;
    }
}

// The stick's part rooted and scaled (negative for none)
f32 RootedStick(f32 stick)
{
    f32 rooted = __builtin_sqrtf(__builtin_fabsf(stick)) * StickScale;
    return stick <= 0.0f ? -rooted : rooted;
}
}

HoverboardVehicle* HoverboardVehicle::Construct(HoverboardVehicle* vehicle, CharacterAgent* agent, Agent* board, u32 armed)
{
    vehicle->agent = agent;
    vehicle->bits.value = 0;
    vehicle->towed = nullptr;
    vehicle->other = board;
    vehicle->vtable = g_HoverboardVehicleVTable;
    vehicle->armed = static_cast<u8>(armed);
    vehicle->bits.drives = 1;
    vehicle->bits.held = 0;
    vehicle->hoverHeight = RestHeight;
    vehicle->Start();
    vehicle->cameraRate = StartCameraRate;
    return vehicle;
}

void HoverboardVehicle::Start()
{
    if (armed != 0)
    {
        SetArmedHandling();
    }
    else
    {
        SetUnarmedHandling();
    }

    knocked = 0.0f;
    LetGo(this);
    towReady = 0;
    Vehicle::Start();
    // The character's collision leaves the board out (retail doesn't check for a board)
    agent->instance->collision.leftOut = other->instance;
    auto* body = static_cast<SphereBody*>(BodyOf(agent->instance));
    if (armed != 0)
    {
        body->SetMassAndSize(BoardMass, radiusX + radiusX, radiusY, radiusZ);
        body->SetEllipsoid(radiusX, radiusY, radiusZ);
        body->SetSoftness(ArmedSoftness);
    }
    else
    {
        body->SetMassAndSize(BoardMass, radiusX, radiusY, radiusZ);
        body->SetEllipsoid(radiusX, radiusY, radiusZ);
        body->SetSoftness(Softness);
    }

    body->SetFriction(0.0f);
    body->SetSpinAndRollFriction(SpinFriction, SpinFriction);
    body->bits.placesInstance = 0;
}

// The board at the body's matrix, the character standing on it and the exit upright at the character, facing across the board
// (retail works out the rotations of both matrices and drops them)
u32 HoverboardVehicle::Place()
{
    DynamicBody* body = BodyOf(agent->instance);
    otherMatrix = body->matrix;
    agentMatrix = otherMatrix;
    Vector4* position = RowOf(&agentMatrix, 3);
    const Vector4* boardUp = RowOf(&agentMatrix, 1);
    position->x = position->x + boardUp->x * -StandDown;
    position->y = position->y + boardUp->y * -StandDown;
    position->z = position->z + boardUp->z * -StandDown;
    const Vector4* boardForward = RowOf(&agentMatrix, 2);
    position->x = position->x + boardForward->x * -StandBack;
    position->y = position->y + boardForward->y * -StandBack;
    position->z = position->z + boardForward->z * -StandBack;
    Vector4 rotation;
    GetRotationVec(&rotation, &agentMatrix);
    Vector4* side = RowOf(&exitMatrix, 0);
    Vector4* up = RowOf(&exitMatrix, 1);
    Vector4* forward = RowOf(&exitMatrix, 2);
    *up = {0.0f, 1.0f, 0.0f, 1.0f};
    // The board's forward axis across up (retail's x is the forward z negated, its zero terms dropped)
    side->x = -boardForward->z;
    side->y = boardForward->z * up->x - boardForward->x * up->z;
    side->z = boardForward->x * up->y - boardForward->y * up->x;
    side->w = 1.0f;
    forward->x = side->y * up->z - side->z * up->y;
    forward->y = side->z * up->x - side->x * up->z;
    forward->z = side->x * up->y - side->y * up->x;
    forward->w = 1.0f;
    *RowOf(&exitMatrix, 3) = *position;
    Vector4 exitRotation;
    GetRotationVec(&exitRotation, &exitMatrix);
    return 1;
}

void HoverboardVehicle::Destroy(u32 destroyFlags)
{
    vtable = g_HoverboardVehicleVTable;
    RemoveReference(&towed);
    Vehicle::Destroy(destroyFlags);
}

void HoverboardVehicle::Knock(const Vector4* push, u32, InstanceContext* source)
{
    f32 strength = push->w;
    if (!(KnockThreshold < strength) || source == nullptr)
    {
        return;
    }

    knocked = 1.0f;
    DynamicBody* body = BodyOf(agent->instance);
    Matrix4x4 toBody = body->matrix;
    Vector4 away = *RowOf(&toBody, 3);
    ObjectPlace* place = source->place;
    place->SyncPosition();
    Vector4 from = place->position;
    away.x = away.x - from.x;
    away.y = away.y - from.y;
    away.z = away.z - from.z;
    f32 impulse = strength * KnockScale / (away.x * away.x + away.y * away.y + away.z * away.z + KnockScale);
    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x = away.x * inverse * impulse;
    away.y = away.y * inverse * impulse;
    away.z = away.z * inverse * impulse;
    VuInvertRigidInPlace(&toBody);
    VuTransformPoint(&toBody, &from, &from);
    body->ApplyImpulse(&away, &from);
}

// Driven; towing (let go once the character is dead); knocked by hard hits and recovering; placed
void HoverboardVehicle::Frame(f32 seconds)
{
    DynamicBody* body = BodyOf(agent->instance);
    f32 impulses = body->impulseTotal;
    Drive();
    if (tows != 0)
    {
        if (agent->state.dead != 0)
        {
            LetGo(this);
        }
        else
        {
            Tow();
        }
    }

    knocked = knocked - seconds * KnockFade;
    if (knocked < 0.0f)
    {
        knocked = 0.0f;
    }

    if (HardHit < impulses)
    {
        if (knocked < 1.0f)
        {
            knocked = 1.0f;
        }

        if (armed == 0)
        {
            Vector4 spin = body->angularVelocity;
            spin.x = spin.x * KnockSpinOthers;
            spin.y = spin.y * KnockSpin;
            spin.z = spin.z * KnockSpinOthers;
            body->SetAngularVelocity(&spin);
        }
    }

    if (armed != 0 && ArmedMostKnocked < knocked)
    {
        knocked = ArmedMostKnocked;
    }

    if (armed != 0)
    {
        if (0.0f < knocked)
        {
            cameraRate = KnockedCameraRate;
        }
        else
        {
            cameraRate = cameraRate * CameraRateKeep + CameraRateGain;
        }

        auto* follow = static_cast<FollowNode*>(GetGameNode(&agent->instance->nodes, NodeFollow));
        if (follow != nullptr)
        {
            follow->camera.rig.ownPositioner.ownRate = cameraRate;
        }
    }

    body->SetRestitution(armed != 0 ? ArmedRestitution : Restitution);
    PlaceRiders(this);
}

u32 HoverboardVehicle::Kind()
{
    return KindHoverboard;
}

u32 HoverboardVehicle::KeepsGun()
{
    return armed;
}

void HoverboardVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-2.0f, -2.0f, -2.0f, 1.0f};
    *max = {2.0f, 2.0f, 2.0f, 1.0f};
}

void HoverboardVehicle::SetArmedHandling()
{
    canRise = 1;
    radiusZ = ArmedRadiusZ;
    radiusX = ArmedRadiusX;
    radiusY = ArmedRadiusY;
    keepsHeading = 1;
    tows = 0;
}

void HoverboardVehicle::SetUnarmedHandling()
{
    radiusZ = RadiusXZ;
    tows = 1;
    radiusY = RadiusY;
    keepsHeading = 0;
    canRise = 0;
    radiusX = RadiusXZ;
}

// The stick's direction (rooted) pulled toward the board's heading when it keeps it, a thrust along it; then the hover, the
// tilt, the turn and the drags
void HoverboardVehicle::Drive()
{
    DynamicBody* body = BodyOf(agent->instance);
    const CharacterButtons& buttons = agent->buttons;
    Vector4 drive;
    drive.y = 0.0f;
    drive.w = 1.0f;
    drive.x = RootedStick(buttons.moveX);
    drive.z = RootedStick(buttons.moveZ);
    if (keepsHeading != 0)
    {
        Vector4 heading = *RowOf(&body->matrix, 2);
        heading.y = 0.0f;
        f32 inverse = InverseLength(&heading, LengthEpsilon);
        heading.x = heading.x * inverse;
        heading.y = heading.y * inverse;
        heading.z = heading.z * inverse;
        // The further the drive is from the heading, the harder it's pulled (less while knocked)
        f32 calm = 1.0f - knocked;
        f32 apart = 1.0f - (heading.x * drive.x + heading.y * drive.y + heading.z * drive.z);
        drive.x = drive.x + heading.x * calm * apart;
        drive.y = drive.y + heading.y * calm * apart;
        drive.z = drive.z + heading.z * calm * apart;
        inverse = InverseLength(&drive, LengthEpsilon);
        drive.x = drive.x * inverse;
        drive.y = drive.y * inverse;
        drive.z = drive.z * inverse;
    }

    f32 thrust = Thrust;
    if (armed != 0)
    {
        thrust = (1.0f - (hoverHeight - RestHeight) * InverseRiseRange * RiseThrustLoss) * Thrust;
    }

    Vector4 force = {drive.x * thrust, drive.y * thrust, drive.z * thrust, 1.0f};
    body->force.x = body->force.x + force.x * body->mass;
    body->force.y = body->force.y + force.y * body->mass;
    body->force.z = body->force.z + force.z * body->mass;
    f32 heightError;
    Hover(&heightError);
    Tilt(heightError, &drive, &body->matrix);
    Turn(&drive, &body->matrix);
    Drag();
    SpinDrag();
}

// A probe down from each corner of the board (leaving out the character, the board and the towed body, which loses its sphere
// contact while they look) and a spring force toward the hover height there
void HoverboardVehicle::Hover(f32* heightError)
{
    InstanceContext* towedInstance = TowedOf(this);
    if (towedInstance != nullptr)
    {
        towedInstance->flags.collisionActive = 0;
    }

    void* results[MostProbed];
    // (bit 0 of the corner's index its side along x, bit 1 along z)
    for (s32 corner = 0; corner < BoardCorners; corner++)
    {
        Vector4 local = {(corner & 1) != 0 ? CornerOffset : -CornerOffset, 0.0f,
                         (corner & 2) != 0 ? CornerOffset : -CornerOffset, 1.0f};
        DynamicBody* body = BodyOf(agent->instance);
        Vector4 start;
        VuTransformPoint(&body->matrix, &local, &start);
        Vector4 end = start;
        end.y = end.y - ProbeDepth;
        InstanceQuery query;
        query.results = results;
        query.count = 0;
        query.most = MostProbed;
        query.distance = Infinite;
        query.bits.value = InstanceQueryBits::AllWanted;
        query.wantedFlags = ReferencedObjectFlags::CollisionActive;
        query.unwantedFlags = ReferencedObjectFlags::Asleep;
        query.skipped[0] = nullptr;
        query.instance = nullptr;
        query.skipped[1] = nullptr;
        SkipInQuery(&query, agent->instance);
        query.skipped[1] = other->instance;
        Vector4 hit;
        f32 push;
        if (SegmentHitsAnything(agent->instance->chunk, &start, &end, SurfaceFlags::SolidToObjects, &query, SolidObjectNodeKinds,
                                nullptr, &hit, nullptr)
            != 0)
        {
            f32 height = start.y - hit.y;
            if (height < 0.0f)
            {
                height = 0.0f;
            }

            hoverHeight = RestHeight;
            if (canRise != 0 && (RiseShoulder < agent->buttons.shoulders || agent->buttons.moveZ < RiseStick))
            {
                hoverHeight = RiseHeight;
            }

            f32 error = hoverHeight - height;
            *heightError = error;
            if (armed != 0)
            {
                push = error * ArmedSpring;
                if (push < 0.0f)
                {
                    push = push * ArmedDownShare;
                }
            }
            else
            {
                push = error * Spring;
            }

            if (0.0f < push)
            {
                push = push * (1.0f - knocked);
            }
        }
        else
        {
            *heightError = MissedError;
            push = MissedPush;
        }

        local.x = local.x * LeverShare;
        local.y = local.y * LeverShare;
        local.z = local.z * LeverShare;
        Vector4 force = {0.0f, push * HoverForceShare * body->mass, 0.0f, 1.0f};
        body->AddLocalForce(&force, &local);
    }

    towedInstance = TowedOf(this);
    if (towedInstance != nullptr)
    {
        towedInstance->flags.collisionActive = 1;
    }
}

void HoverboardVehicle::Tilt(f32 heightError, const Vector4* drive, const Matrix4x4* bodyMatrix)
{
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 want = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 lean = {drive->x * TiltLean, drive->y * TiltLean, drive->z * TiltLean, 1.0f};
    want.x = want.x + lean.x;
    want.y = want.y + lean.y;
    want.z = want.z + lean.z;
    f32 error = heightError;
    if (MostTiltError < error)
    {
        error = MostTiltError;
    }

    if (error < -MostTiltError)
    {
        error = -MostTiltError;
    }

    f32 pitch = error * TiltPitch;
    const Vector4* boardForward = RowOf(bodyMatrix, 2);
    lean = {boardForward->x * pitch, boardForward->y * pitch, boardForward->z * pitch, 1.0f};
    Vector4 up = *RowOf(bodyMatrix, 1);
    want.x = want.x + lean.x;
    want.y = want.y + lean.y;
    want.z = want.z + lean.z;
    f32 inverse = InverseLength(&want, LengthEpsilon);
    want.x = want.x * inverse;
    want.y = want.y * inverse;
    want.z = want.z * inverse;
    Vector4 axis = {up.y * want.z - up.z * want.y, up.z * want.x - up.x * want.z, up.x * want.y - up.y * want.x, 1.0f};
    f32 agreement = up.x * want.x + up.y * want.y + up.z * want.z;
    if (agreement < 0.0f)
    {
        inverse = InverseLength(&axis, LengthEpsilon);
        f32 apart = 1.0f - agreement;
        axis.x = axis.x * inverse * apart;
        axis.y = axis.y * inverse * apart;
        axis.z = axis.z * inverse * apart;
    }

    f32 calm = 1.0f - knocked;
    f32 strength = armed != 0 ? ArmedTiltStrength : TiltStrength;
    body->torque.x = body->torque.x + axis.x * strength * calm * BoardMass;
    body->torque.y = body->torque.y + axis.y * strength * calm * BoardMass;
    body->torque.z = body->torque.z + axis.z * strength * calm * BoardMass;
}

void HoverboardVehicle::Turn(const Vector4* drive, const Matrix4x4* bodyMatrix)
{
    DynamicBody* body = BodyOf(agent->instance);
    if (!(TurnEpsilon < __builtin_sqrtf(drive->x * drive->x + drive->y * drive->y + drive->z * drive->z)))
    {
        return;
    }

    Vector4 forward = *RowOf(bodyMatrix, 2);
    f32 inverse = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * inverse;
    forward.y = forward.y * inverse;
    forward.z = forward.z * inverse;
    Vector4 want = *drive;
    inverse = InverseLength(&want, LengthEpsilon);
    want.x = want.x * inverse;
    want.y = want.y * inverse;
    want.z = want.z * inverse;
    Vector4 axis = {want.y * forward.z - want.z * forward.y, want.z * forward.x - want.x * forward.z,
                    want.x * forward.y - want.y * forward.x, 1.0f};
    f32 calm = 1.0f - knocked;
    f32 strength = armed != 0 ? ArmedTurnStrength : TurnStrength;
    body->torque.x = body->torque.x + axis.x * strength * calm * BoardMass;
    body->torque.y = body->torque.y + axis.y * strength * calm * BoardMass;
    body->torque.z = body->torque.z + axis.z * strength * calm * BoardMass;
}

void HoverboardVehicle::Drag()
{
    f32 calm = 1.0f - knocked;
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 velocity = body->velocity;
    Vector4 local;
    VuRotateVector(&body->inverseMatrix, &velocity, &local);
    local.x = local.x * __builtin_fabsf(local.x) * DragX * calm;
    local.y = local.y * __builtin_fabsf(local.y) * DragY * calm;
    local.z = local.z * __builtin_fabsf(local.z) * DragZ * calm;
    VuRotateVector(&body->matrix, &local, &local);
    body->force.x = body->force.x + local.x * BoardMass;
    body->force.y = body->force.y + local.y * BoardMass;
    body->force.z = body->force.z + local.z * BoardMass;
}

void HoverboardVehicle::SpinDrag()
{
    f32 calm = 1.0f - knocked;
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 spin = body->angularVelocity;
    Vector4 local;
    VuRotateVector(&body->inverseMatrix, &spin, &local);
    local.y = local.y * SpinDragY * calm;
    local.z = local.z * SpinDragOthers;
    local.x = local.x * SpinDragOthers;
    VuRotateVector(&body->matrix, &local, &local);
    body->torque.x = body->torque.x + local.x * BoardMass;
    body->torque.y = body->torque.y + local.y * BoardMass;
    body->torque.z = body->torque.z + local.z * BoardMass;
}

// Circle: pressed again with nothing towed, the nearest body below the board is let back into the world and towed; towing,
// springs pull the two together once they're apart; pressed again, the tow is let go
void HoverboardVehicle::Tow()
{
    bool pressed = TowPress <= agent->buttons.circle;
    DynamicBody* towedBody = nullptr;
    InstanceContext* towedInstance = TowedOf(this);
    if (towedInstance != nullptr)
    {
        towedBody = BodyOf(towedInstance);
    }

    if (towedBody != nullptr)
    {
        Vector4 position = *RowOf(&BodyOf(agent->instance)->matrix, 3);
        Vector4 towedPosition = *RowOf(&towedBody->matrix, 3);
        f32 x = position.x - towedPosition.x;
        f32 y = position.y - towedPosition.y;
        f32 z = position.z - towedPosition.z;
        if (TowLength < __builtin_sqrtf(x * x + y * y + z * z))
        {
            Vector4 middle = {0.0f, 0.0f, 0.0f, 1.0f};
            BodyOf(agent->instance)->Spring(TowLength, TowStiffness, TowDamping, &towedPosition, &middle, 0);
            middle = {0.0f, 0.0f, 0.0f, 1.0f};
            towedBody->Spring(TowLength, TowStiffness, TowDamping, &position, &middle, 0);
        }
    }

    if (!pressed)
    {
        towReady = 1;
        return;
    }

    if (towReady == 0)
    {
        return;
    }

    if (towedBody != nullptr)
    {
        LetGo(this);
        towReady = 0;
        return;
    }

    void* results[MostTowed];
    InstanceQuery query;
    query.results = results;
    query.count = 0;
    query.most = MostTowed;
    query.distance = Infinite;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    f32 nearest = Infinite;
    InstanceContext* found = nullptr;
    query.bits.value = InstanceQueryBits::AllWanted;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, agent->instance);
    Vector4 position = *RowOf(&BodyOf(agent->instance)->matrix, 3);
    Box box;
    box.min = position;
    box.max = position;
    box.min.x = box.min.x - TowReachAcross;
    box.max.x = box.max.x + TowReachAcross;
    box.min.y = box.min.y - TowReachDown;
    box.min.z = box.min.z - TowReachAcross;
    box.max.z = box.max.z + TowReachAcross;
    u32 count = QueryChunkInstances(agent->instance->chunk, &box, TowedKinds, &query);
    for (u16 i = 0; i < count; i++)
    {
        auto* instance = static_cast<InstanceContext*>(results[i]);
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 at = place->position;
        f32 x = at.x - position.x;
        f32 z = at.z - position.z;
        f32 below = position.y - at.y;
        f32 distanceSquared = x * x + z * z;
        if (0.0f < below && below < TowDepth && distanceSquared < nearest)
        {
            nearest = distanceSquared;
            found = instance;
        }
    }

    if (found == nullptr)
    {
        return;
    }

    BodyOf(found)->ReleaseRide();
    AssignReference(&towed, found);
    towReady = 0;
}
