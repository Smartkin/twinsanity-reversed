#include "game/vehicles.h"

#include "game/agents.h"
#include "game/attachments.h"
#include "game/camerarig.h"
#include "game/chunkdata.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"

// The Rollerbrawl: Crash and Cortex fighting as one rolling ball, a sphere body the stick pushes along. Left still on the
// ground it stops (both stand up out of it) until the stick or a push moves it again. Hard knocks make it wobble, sticky ground
// packs snow on it (a bigger and heavier ball under the snow shell) that other ground wears off and lava melts, and it leaves a
// skid trail either side

EABI_EXPORT(FUN_00150a40, &RollerbrawlVehicle::PlaceSnowShell);
EABI_EXPORT(FUN_001513e0, &RollerbrawlVehicle::Wobble);
EABI_EXPORT(FUN_001516c8, &RollerbrawlVehicle::RollFrame);
EABI_EXPORT(FUN_00151990, &RollerbrawlVehicle::StoppedFrame);
EABI_EXPORT(FUN_00151c98, &RollerbrawlVehicle::SquashedFrame);
EABI_EXPORT(FUN_00152d58, &RollerbrawlVehicle::Frame);
EABI_EXPORT(FUN_00160be8, &RollerbrawlVehicle::GrowSnow);
EABI_EXPORT(FUN_00160ce8, &RollerbrawlVehicle::WearSnow);

namespace
{
// The character's nodes: its object node, its body and its attachments
constexpr u32 ObjectNodeKind = 1;
constexpr u32 BodyNodeKind = 5;
constexpr u32 AttachmentsKind = 6;

// The agent's bump, the object node's whether it takes packets and its collision
constexpr u32 BumpedSlot = 8;
constexpr u32 TakesPacketsSlot = 15;
constexpr u32 CollidedSlot = 28;

// The script events both characters get: a hard fall (falling fast, or squashed for 3 seconds), the ball stopping (they stand
// up out of it) and rolling again
constexpr u32 EventFell = 11;
constexpr u32 EventStopped = 0x4C;
constexpr u32 EventRolling = 0x4D;

// Surfaces: sticky ground (collision mask bit 10) packs snow on, a contact of lava's (contact kind bit 3) melts it
constexpr u32 StickySurface = 0x400;
constexpr u32 LavaContact = 0x8;

// The surfaces' bit its casts and its body's collision cache take (solid to the probes), and the kinds of nodes (a bit each) of the
// instances it hits (kind 4's hulls, characters, crates, creatures, generic objects and pay gates) and of those that hold it up
// (projectiles too)
constexpr u32 SolidToProbes = 0x10;
constexpr u32 HitNodeKinds = 0x5B010;
constexpr u32 GroundNodeKinds = 0x15B010;

constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 NoHit = Rounded(1e30);

// The ball: its radius without snow (the snow past 0.4 adds to it), the box its mass is spread over, gravity (times the mass)
constexpr f32 BallRadius = Rounded(0.8);
constexpr f32 SnowWithoutGrowth = Rounded(0.4);
constexpr f32 BoxSize = Rounded(1.1);
constexpr f32 Gravity = -35.0f;

// Snow: the most, the mass a unit adds, the shell's scale without any, what a unit of rolling packs on (sticky ground) and
// wears off (other ground), how fast lava melts it and the least time a lava contact melts it for
constexpr f32 MostSnow = 3.0f;
constexpr f32 SnowMass = Rounded(0.55);
constexpr f32 ShellScale = Rounded(0.6);
constexpr f32 SnowGrowth = Rounded(0.01);
constexpr f32 SnowWear = Rounded(0.02);
constexpr f32 MeltRate = 0.5f;
constexpr f32 MeltTime = Rounded(0.6);

// Rolling: falling faster than this is a hard fall; it stops after 0.35 seconds with no snow, barely moving (its speed and spin
// squared under 0.4) and the stick under 0.1 (retail's 0.1 squared)
constexpr f32 FallSpeed = -120.0f;
constexpr f32 StillMotion = Rounded(0.4);
constexpr f32 StillStick = 0x1.47ae16p-7f;
constexpr f32 StillTime = Rounded(0.35);

// Stopped: it turns upright over 0.5 seconds, and rolls again after 8 seconds, or once the stick passes 0.2 (retail's 0.2
// squared) or its speed and spin squared pass 0.8
constexpr f32 MostStopTime = 8.0f;
constexpr f32 MovingStick = 0x1.47ae16p-5f;
constexpr f32 MovingMotion = Rounded(0.8);

// The stick's push: in the air along it (8 times the mass); on the ground down at a point 2 along it (its length to the fourth
// times 10, up to half again as hard against the motion)
constexpr f32 AirPush = 8.0f;
constexpr f32 GroundPush = 10.0f;
constexpr f32 SpinFriction = 0.5f;
constexpr f32 RollFriction = Rounded(0.07);

// Drags past a speed and a spin of 0.09
constexpr f32 FreeSpeed = Rounded(0.09);
constexpr f32 DragShare = Rounded(-0.005);
constexpr f32 SpinDragShare = Rounded(-0.0025);

// The wobble: a change of velocity past 15 starts one of its sixtieth (at most 0.5), 21 radians a second, fading by 1.5 a second
constexpr f32 KnockSpeed = 15.0f;
constexpr f32 WobblePerKnock = Rounded(1.0 / 60.0);
constexpr f32 MostWobble = 0.5f;
constexpr f32 WobbleSpeed = 21.0f;
constexpr f32 WobbleFade = 1.5f;
constexpr f32 ShakeFalloff = Rounded(0.01);

// Squashed: flattened to a tenth over 0.135 seconds (a third of the time times 20), the hard fall after 3 seconds
constexpr f32 Third = Rounded(1.0 / 3.0);
constexpr f32 SquashSpeed = 20.0f;
constexpr f32 Flattest = Rounded(0.1);
constexpr f32 Bulge = Rounded(-1.0 / 0.9);
constexpr f32 SquashTime = 3.0f;

// The exit matrix sits at the ball's bottom raised 0.05
constexpr f32 ExitRaise = Rounded(0.05);

// The skid marks: laid moving faster than 0.1 across, half the radius out either side of the ball's bottom (0.866 of the radius
// down), 0.16 deep
constexpr f32 SkidSpeed = Rounded(0.1);
constexpr f32 SkidDrop = Rounded(0.866);
constexpr f32 SkidDepth = Rounded(0.16);

// What's under the ball: a ray 0.1 down from 0.05 above its bottom
constexpr f32 GroundRayStart = Rounded(0.05);
constexpr f32 GroundRayLength = Rounded(-0.1);

constexpr u16 MostHits = 20;
constexpr u16 MostGroundHits = 8;

SphereBody* BodyOf(CharacterAgent* agent)
{
    return static_cast<SphereBody*>(GetGameNode(&agent->instance->nodes, BodyNodeKind));
}

const Vector4* PositionOf(const SphereBody* body)
{
    return RowOf(&body->matrix, 3);
}

ObjectNode* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
}

// A query of the awake instances with some flags (retail leaves the bits nothing reads as the stack had them)
void StartQuery(InstanceRayHit* query, void** results, u16 most, u32 wanted)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = NoHit;
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = wanted;
    query->unwantedFlags = ReferencedObject::FlagAsleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// The object node's AgentRef2 (an asleep one forgotten unless the node keeps it)
InstanceContext* AwakeAgentRef2(ObjectNode* node)
{
    InstanceContext* other = node->agentRef2;
    if (other != nullptr && (other->flags & ReferencedObject::FlagAsleep) != 0
        && (node->flags & ObjectNodeBase::FlagKeepsAgentRef2) == 0)
    {
        node->agentRef2 = nullptr;
    }

    return node->agentRef2;
}

// The snow set (at most MostSnow): the ball's radius (the snow past 0.4 added) and the shell's scale (the snow and 0.6) follow
// it, the body's mass and size are made again. The mass takes the value as it was given: uncapped the frame the snow passes the
// most (retail)
void SetSnow(RollerbrawlVehicle* vehicle, f32 value)
{
    vehicle->snow = value;
    SphereBody* body = BodyOf(vehicle->agent);
    if (MostSnow < vehicle->snow)
    {
        vehicle->snow = MostSnow;
    }

    f32 growth = vehicle->snow - SnowWithoutGrowth;
    if (growth < 0.0f)
    {
        growth = 0.0f;
    }

    vehicle->radius = growth + BallRadius;
    vehicle->snowScale = vehicle->snow - Rounded(0.2) + Rounded(0.8);
    body->SetMassAndSize(value * SnowMass + 1.0f, BoxSize, BoxSize, BoxSize);
    body->ellipsoid = 0;
    body->radius = vehicle->radius;
}

// No snow: the start's radius, shell scale and mass
void ClearSnow(RollerbrawlVehicle* vehicle)
{
    SphereBody* body = BodyOf(vehicle->agent);
    vehicle->snow = 0.0f;
    vehicle->radius = BallRadius;
    vehicle->snowScale = ShellScale;
    body->SetMassAndSize(1.0f, BoxSize, BoxSize, BoxSize);
    body->ellipsoid = 0;
    body->radius = vehicle->radius;
}

// Gravity's force added to the body's (its x and z the zeros added)
void AddGravity(SphereBody* body)
{
    Vector4 force = {0.0f, body->mass * Gravity, 0.0f, 1.0f};
    body->force.x = body->force.x + force.x;
    body->force.y = body->force.y + force.y;
    body->force.z = body->force.z + force.z;
}

// A matrix moved by an offset (x, y and z)
void MoveMatrix(Matrix4x4* matrix, const Vector4* offset)
{
    Vector4* position = RowOf(matrix, 3);
    position->x = position->x + offset->x;
    position->y = position->y + offset->y;
    position->z = position->z + offset->z;
}

bool Contains(InstanceContext* const* instances, s32 count, const InstanceContext* instance)
{
    for (s32 i = 0; i < count; i++)
    {
        if (instances[i] == instance)
        {
            return true;
        }
    }

    return false;
}
}

void RollerbrawlVehicle::PlaceSnowShell(f32 seconds)
{
    SphereBody* body = BodyOf(agent);
    ObjectNode* node = ObjectNodeOf(agent->instance);
    if (node != nullptr && CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) != 0)
    {
        InstanceContext* shell = AwakeAgentRef2(node);
        if (shell != nullptr)
        {
            shell->flags &= ~ReferencedObject::FlagSphereContact;
            if (0.0f < snow)
            {
                Matrix4x4 matrix = body->matrix;
                f32 scale = snowScale;
                for (s32 row = 0; row < 3; row++)
                {
                    matrix.m[row][0] = matrix.m[row][0] * scale;
                    matrix.m[row][1] = matrix.m[row][1] * scale;
                    matrix.m[row][2] = matrix.m[row][2] * scale;
                }

                shell->flags |= ReferencedObject::FlagVisible;
                if (SetPlaceMatrix(shell->place, &matrix) != 0)
                {
                    QueueObject(shell);
                }

                ObjectPlace* place = shell->place;
                place->SyncPosition();
                if (place->MoveTo(RowOf(&matrix, 3)))
                {
                    QueueObject(shell);
                }
            }
            else
            {
                shell->flags &= ~ReferencedObject::FlagVisible;
            }
        }
    }

    if (0.0f < meltTime)
    {
        meltTime = meltTime - seconds;
        if (meltTime < 0.0f)
        {
            meltTime = 0.0f;
        }
    }
}

void RollerbrawlVehicle::StickPush(u32 touching)
{
    CharacterAgent* character = agent;
    SphereBody* body = BodyOf(character);
    Vector4 stick = {character->buttons.moveX, 0.0f, character->buttons.moveZ, 1.0f};
    if (touching == 0)
    {
        Vector4 push = {stick.x * AirPush, stick.y, stick.z * AirPush, 1.0f};
        f32 mass = body->mass;
        body->force.x = body->force.x + push.x * mass;
        body->force.y = body->force.y + push.y * mass;
        body->force.z = body->force.z + push.z * mass;
        return;
    }

    // Down at the point 2 along the stick, harder the more the stick goes against the motion
    Vector4 point = *PositionOf(body);
    Vector4 reach = {stick.x + stick.x, stick.y, stick.z + stick.z, 1.0f};
    f32 length = __builtin_sqrtf(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);
    point.x = point.x + reach.x;
    point.y = point.y + reach.y;
    point.z = point.z + reach.z;
    f32 lengthSquared = length * length;
    Vector4 force = {0.0f, -(lengthSquared * lengthSquared * GroundPush), 0.0f, 1.0f};
    Vector4 heading = body->velocity;
    heading.y = 0.0f;
    f32 inverse = InverseLength(&heading, LengthEpsilon);
    heading.x = heading.x * inverse;
    heading.y = heading.y * inverse;
    heading.z = heading.z * inverse;
    f32 along = stick.x * heading.x + stick.y * heading.y + stick.z * heading.z;
    f32 against = (1.0f - along) * 0.5f;
    if (against < 0.0f)
    {
        against = 0.0f;
    }

    if (1.0f < against)
    {
        against = 1.0f;
    }

    f32 strength = against * 0.5f + 1.0f;
    force.x = force.x * strength;
    force.y = force.y * strength;
    force.z = force.z * strength;

    // The roll friction: fast, little going with the motion (none past 8) and much against it; slow, little going with it, more
    // with the stick let go or against it; still, more the less the stick goes
    f32 speed = __builtin_sqrtf(body->velocity.x * body->velocity.x + body->velocity.z * body->velocity.z);
    if (1.0f < along)
    {
        along = 1.0f;
    }
    else if (along < -1.0f)
    {
        along = -1.0f;
    }

    f32 roll;
    if (4.0f < speed)
    {
        if (0.0f < along || length < Rounded(0.1))
        {
            roll = 8.0f < speed ? 0.0f : 0.5f;
        }
        else
        {
            f32 alongSquared = along * along;
            f32 alongFourth = alongSquared * alongSquared;
            roll = alongFourth * alongFourth * 18.0f + 1.0f;
        }
    }
    else if (Rounded(0.001) < speed)
    {
        if (Rounded(0.01) < along)
        {
            roll = 1.0f;
        }
        else if (length < Rounded(0.1))
        {
            roll = 5.0f;
        }
        else
        {
            roll = along * (along * 10.0f) + 1.0f;
        }
    }
    else
    {
        roll = Rounded(1.2) / (length + Rounded(0.2));
    }

    body->SetSpinAndRollFriction(SpinFriction, roll * RollFriction);
    body->AddForce(&force, &point);
}

void RollerbrawlVehicle::Drag()
{
    SphereBody* body = BodyOf(agent);
    Vector4 velocity = body->velocity;
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    f32 excess = 0.0f;
    if (!(speed < FreeSpeed))
    {
        excess = speed - FreeSpeed;
    }

    f32 share = excess * DragShare;
    f32 mass = body->mass;
    body->force.x = body->force.x + velocity.x * share * mass;
    body->force.y = body->force.y + velocity.y * share * mass;
    body->force.z = body->force.z + velocity.z * share * mass;
}

void RollerbrawlVehicle::SpinDrag()
{
    SphereBody* body = BodyOf(agent);
    Vector4 spin = body->angularVelocity;
    f32 speed = __builtin_sqrtf(spin.x * spin.x + spin.y * spin.y + spin.z * spin.z);
    f32 excess = 0.0f;
    if (!(speed < FreeSpeed))
    {
        excess = speed - FreeSpeed;
    }

    f32 share = excess * SpinDragShare;
    body->torque.x = body->torque.x + spin.x * share;
    body->torque.y = body->torque.y + spin.y * share;
    body->torque.z = body->torque.z + spin.z * share;
}

void RollerbrawlVehicle::Wobble(f32 seconds)
{
    SphereBody* body = BodyOf(agent);
    InitIdentityMatrix(&squash);
    Vector4 change = {body->velocity.x - lastVelocity.x, body->velocity.y - lastVelocity.y, body->velocity.z - lastVelocity.z,
                      1.0f};
    f32 knock = __builtin_sqrtf(change.x * change.x + change.y * change.y + change.z * change.z);
    lastVelocity = body->velocity;
    if (KnockSpeed < knock && wobble < knock)
    {
        // A wobble along the change (in the body's space), the camera shaken
        wobble = knock * WobblePerKnock;
        if (MostWobble < wobble)
        {
            wobble = MostWobble;
        }
        else if (wobble < -MostWobble)
        {
            wobble = -MostWobble;
        }

        wobbleAxis = change;
        f32 inverse = InverseLength(&wobbleAxis, LengthEpsilon);
        wobbleAxis.x = wobbleAxis.x * inverse;
        wobbleAxis.y = wobbleAxis.y * inverse;
        wobbleAxis.z = wobbleAxis.z * inverse;
        VuRotateVector(&body->inverseMatrix, &wobbleAxis, &wobbleAxis);
        f32 strength = __builtin_fabsf(wobble);
        PushCameraShake(&g_CameraShake, PositionOf(body), strength + strength, ShakeFalloff);
        wobblePhase = 0.0f;
    }

    if (wobble == 0.0f)
    {
        return;
    }

    // The squash: the identity less the amplitude times the phase's cosine along the axis
    s32 angle;
    AngleFrom(&angle, wobblePhase, AngleRadians);
    f32 squeeze = wobble * CosOfAngle(&angle);
    wobblePhase = wobblePhase + seconds * WobbleSpeed;
    wobble = wobble * (1.0f - seconds * WobbleFade);
    const f32* axis = &wobbleAxis.x;
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 column = 0; column < 3; column++)
        {
            squash.m[row][column] = squash.m[row][column] - squeeze * axis[row] * axis[column];
        }
    }

    squashOffset = g_DefaultBox.min;
    squashOffset.w = 1.0f;
}

void RollerbrawlVehicle::RollFrame(f32 seconds)
{
    SphereBody* body = BodyOf(agent);
    u32 touching = (body->bodyFlags & RigidBody::FlagTouchedBody) != 0 || (body->bodyFlags & RigidBody::FlagTouched) != 0;
    AddGravity(body);
    StickPush(touching);
    Drag();
    SpinDrag();
    Wobble(seconds);
    if (body->velocity.y < FallSpeed)
    {
        RunAgentEvent(agent, EventFell, reinterpret_cast<u32>(agent->instance), 0, 0);
    }

    // Left still long enough without snow, on the ground, it stops
    f32 stickX = agent->buttons.moveX;
    f32 stickZ = agent->buttons.moveZ;
    const Vector4& velocity = body->velocity;
    const Vector4& spin = body->angularVelocity;
    f32 flatSpeed = velocity.x * velocity.x + velocity.z * velocity.z;
    f32 spinSpeed = spin.x * spin.x + spin.y * spin.y + spin.z * spin.z;
    if (flatSpeed + spinSpeed < StillMotion && stickX * stickX + stickZ * stickZ < StillStick)
    {
        timer = timer + seconds;
        if (StillTime < timer)
        {
            if (snow == 0.0f && OnGround() != 0)
            {
                Stop();
            }
            else
            {
                timer = 0.0f;
            }
        }
    }
    else
    {
        timer = 0.0f;
    }

    Vehicle::Frame(seconds);
}

void RollerbrawlVehicle::StoppedFrame(f32 seconds)
{
    CharacterAgent* character = agent;
    SphereBody* body = BodyOf(character);
    Vector4 stick = {character->buttons.moveX, 0.0f, character->buttons.moveZ, 1.0f};
    // Held where it stopped
    body->SetPosition(&stopPosition);
    Vector4 gravity = {0.0f, Gravity, 0.0f, 1.0f};
    f32 mass = body->mass;
    Vector4 force = {gravity.x * mass, gravity.y * mass, gravity.z * mass, 1.0f};
    body->force.x = body->force.x + force.x;
    body->force.y = body->force.y + force.y;
    body->force.z = body->force.z + force.z;
    Drag();
    SpinDrag();
    Wobble(seconds);
    const Vector4& velocity = body->velocity;
    const Vector4& spin = body->angularVelocity;
    f32 motion = (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
                 + (spin.x * spin.x + spin.y * spin.y + spin.z * spin.z);

    // Turned from its rotation then to the upright one (smoothstep over half a second)
    f32 t = timer + timer;
    if (1.0f < t)
    {
        t = 1.0f;
    }

    Vector4 rotation;
    SlerpRotations(t * (t * 3.0f) - (t + t) * t * t, &rotation, &stopRotation, &uprightRotation);
    Matrix4x4 matrix;
    MatrixFromRotation(&matrix, &rotation);
    *RowOf(&matrix, 3) = *PositionOf(body);
    body->SetMatrix(&matrix);
    // Placed twice (retail drops the first)
    Vehicle::Place();
    Vehicle::Frame(seconds);
    timer = timer + seconds;
    if (MostStopTime < timer || MovingStick < stick.x * stick.x + stick.y * stick.y + stick.z * stick.z
        || MovingMotion < motion)
    {
        Roll();
    }
}

// Nothing makes the state (retail)
void RollerbrawlVehicle::SquashedFrame(f32 seconds)
{
    SphereBody* body = BodyOf(agent);
    AddGravity(body);
    wobbleAxis = {0.0f, 1.0f, 0.0f, 1.0f};
    f32 flat = 1.0f - timer * Third * SquashSpeed;
    if (flat < Flattest)
    {
        flat = Flattest;
    }

    // Flattened along y, spread across it
    f32 spread = (flat - Flattest) * Bulge + 2.0f;
    InitIdentityMatrix(&squash);
    f32 scale = flat / spread - 1.0f;
    const f32* axis = &wobbleAxis.x;
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 column = 0; column < 3; column++)
        {
            squash.m[row][column] = (squash.m[row][column] + scale * axis[row] * axis[column]) * spread;
        }
    }

    // The axis turned into the world (retail drops it)
    Vector4 worldAxis;
    VuRotateVector(&body->matrix, &wobbleAxis, &worldAxis);
    squashOffset = g_DefaultBox.min;
    squashOffset.w = 1.0f;
    // Placed twice (retail drops the first)
    Vehicle::Place();
    Vehicle::Frame(seconds);
    timer = timer + seconds;
    if (SquashTime < timer)
    {
        RunAgentEvent(agent, EventFell, reinterpret_cast<u32>(agent->instance), 0, 0);
    }
}

void RollerbrawlVehicle::Stop()
{
    InstanceContext* instance = agent->instance;
    InstanceContext* partnerInstance = partner->instance;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    stopPosition = *RowOf(&place->matrix, 3);
    GetRotationVec(&stopRotation, &place->matrix);

    // The rotation that stands the character's place upright, and a random turn about y (360 taken as radians: still a
    // random yaw)
    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 worldUp = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 placeUp;
    VuRotateVector(&place->matrix, &up, &placeUp);
    f32 inverse = InverseLength(&placeUp, LengthEpsilon);
    placeUp.x = placeUp.x * inverse;
    placeUp.y = placeUp.y * inverse;
    placeUp.z = placeUp.z * inverse;
    Vector4 axis = {placeUp.y * worldUp.z - placeUp.z * worldUp.y, placeUp.z * worldUp.x - placeUp.x * worldUp.z,
                    placeUp.x * worldUp.y - placeUp.y * worldUp.x, 1.0f};
    if (Rounded(5e-5) < axis.x * axis.x + axis.y * axis.y + axis.z * axis.z)
    {
        inverse = InverseLength(&axis, LengthEpsilon);
        f32 cosine = placeUp.x * worldUp.x + placeUp.y * worldUp.y + placeUp.z * worldUp.z;
        axis.x = axis.x * inverse;
        axis.y = axis.y * inverse;
        axis.z = axis.z * inverse;
        s32 tilt;
        AngleOfCosine(cosine, &tilt);
        s32 angle = tilt;
        Vector4 upright;
        RotationAboutAxis(&upright, &axis, &angle, 0);
        uprightRotation = upright;
        MultiplyRotations(&uprightRotation, &uprightRotation, &stopRotation);
        AngleFrom(&angle, RandomBelowFloat(360.0f), AngleRadians);
        Vector4 yaw;
        RotationFromYaw(&yaw, &angle);
        MultiplyRotations(&uprightRotation, &uprightRotation, &yaw);
    }
    else
    {
        uprightRotation = stopRotation;
    }

    ObjectNode* node = ObjectNodeOf(instance);
    ObjectNode* partnerNode = ObjectNodeOf(partnerInstance);
    u8 countdown = static_cast<u8>(RandomBelow(0xFF));
    node->unknown154 = countdown;
    partnerNode->unknown154 = countdown;
    RunAgentEvent(agent, EventStopped, 0, 0, 0);
    RunAgentEvent(partner, EventStopped, 0, 0, 0);
    wobble = 0.0f;
    state = StateStopped;
    timer = 0.0f;
    meltTime = 0.0f;
}

u32 RollerbrawlVehicle::OnGround()
{
    void* results[MostGroundHits];
    InstanceRayHit query;
    StartQuery(&query, results, MostGroundHits, ReferencedObject::FlagSphereContact);
    SphereBody* body = BodyOf(agent);
    SkipInQuery(&query, agent->instance);
    query.skipped[1] = partner->instance;
    Vector4 from = *PositionOf(body);
    from.y = from.y - radius + GroundRayStart;
    Vector4 way = {0.0f, GroundRayLength, 0.0f, 1.0f};
    return agent->LineOfSight(&from, &way, SolidToProbes, &query, GroundNodeKinds) != 0;
}

void RollerbrawlVehicle::LaySkidMarks(u32 touching)
{
    u32 laid = 0;
    if (touching != 0)
    {
        SphereBody* body = BodyOf(agent);
        Vector4 bottom = *PositionOf(body);
        bottom.y = bottom.y - radius * SkidDrop;
        Vector4 heading = body->velocity;
        heading.y = 0.0f;
        if (SkidSpeed < __builtin_sqrtf(heading.x * heading.x + heading.z * heading.z))
        {
            f32 inverse = InverseLength(&heading, LengthEpsilon);
            heading.x = heading.x * inverse;
            heading.y = heading.y * inverse;
            heading.z = heading.z * inverse;
            Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
            laid = 1;
            Vector4 side = {heading.y * up.z - heading.z * up.y, heading.z * up.x - heading.x * up.z,
                            heading.x * up.y - heading.y * up.x, 1.0f};
            inverse = InverseLength(&side, LengthEpsilon);
            side.x = side.x * inverse;
            side.y = side.y * inverse;
            side.z = side.z * inverse;
            f32 offset = radius * 0.5f;
            Vector4 otherSide = side;
            otherSide.x = -otherSide.x;
            otherSide.y = -otherSide.y;
            otherSide.z = -otherSide.z;
            // (retail passes LaySkidMark 26 and -26 after the cache, which it doesn't read)
            LaySkidMark(offset, SkidDepth, &leftMarks, &bottom, &side, body->Cache());
            LaySkidMark(offset, SkidDepth, &rightMarks, &bottom, &otherSide, body->Cache());
        }
    }

    if (laid == 0)
    {
        IdleSkidMarks(&leftMarks);
        IdleSkidMarks(&rightMarks);
    }
}

void RollerbrawlVehicle::HitInstances()
{
    constexpr s32 MostLeftOut = 20;

    s32 leftOutCount = 0;
    SphereBody* body = BodyOf(agent);
    Box box = {*PositionOf(body), *PositionOf(body)};
    GrowBox(radius, &box);
    void* results[MostHits];
    InstanceRayHit query;
    StartQuery(&query, results, MostHits, ReferencedObject::FlagTriggerSignals);
    InstanceContext* instance = agent->instance;
    ChunkData* chunk = instance->chunk;
    SkipInQuery(&query, instance);
    QueryChunkInstances(chunk, &box, HitNodeKinds, &query);

    // Left out: what either character has attached, and the other character (more than 19 attached overflow the list: retail)
    InstanceContext* leftOut[MostLeftOut];
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&agent->instance->nodes, AttachmentsKind));
    if (attachments != nullptr && attachments->path != nullptr)
    {
        leftOutCount = AttachedInstances(attachments->path, leftOut, MostLeftOut - 1);
    }

    s32 otherCount = 0;
    attachments = static_cast<AttachmentsNode*>(GetGameNode(&other->instance->nodes, AttachmentsKind));
    if (attachments != nullptr && attachments->path != nullptr)
    {
        otherCount = AttachedInstances(attachments->path, &leftOut[leftOutCount], MostLeftOut - 1 - leftOutCount);
    }

    leftOutCount += otherCount;
    leftOut[leftOutCount++] = other->instance;
    for (s32 i = 0; i < query.count; i++)
    {
        auto* hit = static_cast<InstanceContext*>(query.results[i]);
        if ((hit->flags & ReferencedObject::FlagSphereContact) != 0 || Contains(leftOut, leftOutCount, hit))
        {
            continue;
        }

        // Every hull the ball is in: the hull's surface's message to the character, its bump, and the object node's collision
        // when its instance has a physics body
        ObjectCollision* collision = &hit->collision;
        s32 hulls = GetHullCount(collision);
        for (s32 index = 0; index < hulls; index++)
        {
            CollisionHull* hull;
            Matrix4x4 matrix;
            GetInstanceHull(collision, index, &hull, &matrix);
            Vector4 centre = *PositionOf(body);
            VuInvertRigidInPlace(&matrix);
            VuTransformPoint(&matrix, &centre, &centre);
            Vector4 push;
            if (SphereInHull(radius, hull, &centre, &push) == 0)
            {
                continue;
            }

            agent->SendSurfaceMessage(&g_CollisionSurfaces.surfaces[HullSurfaceIndex(collision, static_cast<u8>(index))]);
            Vector4 back = {-body->velocity.x, -body->velocity.y, -body->velocity.z, 1.0f};
            CallVirtual<void>(agent, agent->vtable, BumpedSlot, hit, &body->velocity, &back);
            // (retail looks up the hit instance's object node and drops it)
            ObjectNodeOf(hit);
            ObjectNode* node = ObjectNodeOf(agent->instance);
            if ((agent->instance->flags & ReferencedObject::FlagPhysicsBody) != 0)
            {
                back = {-body->velocity.x, -body->velocity.y, -body->velocity.z, 1.0f};
                CallVirtual<u32>(node, node->vtable, CollidedSlot, hit, PositionOf(body), &back);
            }
        }
    }
}

RollerbrawlVehicle* RollerbrawlVehicle::Construct(RollerbrawlVehicle* vehicle, CharacterAgent* agent, CharacterAgent* partner)
{
    vehicle->agent = agent;
    vehicle->bits = 0;
    vehicle->partner = partner;
    vehicle->vtable = g_RollerbrawlVehicleVTable;
    vehicle->other = partner;
    vehicle->bits = (vehicle->bits | BitDrives) & ~BitHeld;
    ConstructSkidMarks(&vehicle->leftMarks);
    ConstructSkidMarks(&vehicle->rightMarks);
    vehicle->Start();
    return vehicle;
}

void RollerbrawlVehicle::Start()
{
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    state = StateRolling;
    heading = *RowOf(&place->matrix, 2);
    radius = BallRadius;
    unknown110 = 0;
    ClearSkidMarks(&leftMarks);
    ClearSkidMarks(&rightMarks);
    wobbleAxis = g_DefaultBox.min;
    wobbleAxis.w = 1.0f;
    wobble = 0.0f;
    InitIdentityMatrix(&squash);
    squashOffset = g_DefaultBox.min;
    squashOffset.w = 1.0f;
    wobblePhase = 0.0f;
    timer = 0.0f;
    meltTime = 0.0f;
    Vehicle::Start();
    ClearSnow(this);

    // The ball: bouncy, rubbing, its own callbacks and no drag; the other character's sphere contact off while it's in the ball
    SphereBody* body = BodyOf(agent);
    body->SetRestitution(Rounded(0.15));
    body->SetSoftness(12.0f);
    body->SetFriction(1.5f);
    body->SetSpinAndRollFriction(SpinFriction, RollFriction);
    body->bits &= ~DynamicBody::BitPlacesInstance;
    body->contactArgument = this;
    body->touchArgument = this;
    body->touchCallback = TouchCallback;
    body->contactCallback = ContactCallback;
    body->substeps = 3;
    body->SetCacheMask(SolidToProbes);
    body->lengthDrag = 0.0f;
    body->drag = 0.0f;
    other->instance->flags &= ~ReferencedObject::FlagSphereContact;
    lastVelocity = body->velocity;
    ClearSnow(this);
    onSticky = 0;
}

void RollerbrawlVehicle::Frame(f32 seconds)
{
    SphereBody* body = BodyOf(agent);
    u32 touching = (body->bodyFlags & RigidBody::FlagTouchedBody) != 0 || (body->bodyFlags & RigidBody::FlagTouched) != 0;
    if (state == StateRolling || state == StateStopped)
    {
        if (state == StateRolling)
        {
            RollFrame(seconds);
        }
        else
        {
            StoppedFrame(seconds);
        }

        // Snow packed on rolling over sticky ground and worn off over other ground, melted while lava's melt lasts
        SphereBody* moved = BodyOf(agent);
        if (touching != 0)
        {
            const Vector4& velocity = moved->velocity;
            f32 travelled
                = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) * seconds;
            if (onSticky != 0)
            {
                GrowSnow(travelled * SnowGrowth);
            }
            else
            {
                WearSnow(travelled * SnowWear);
            }
        }

        if (0.0f < meltTime)
        {
            WearSnow(seconds * MeltRate);
        }

        onSticky = 0;
    }
    else if (state == StateSquashed)
    {
        SquashedFrame(seconds);
    }

    PlaceSnowShell(seconds);
    LaySkidMarks((body->bodyFlags & RigidBody::FlagTouched) != 0);
    HitInstances();
}

u32 RollerbrawlVehicle::Place()
{
    SphereBody* body = BodyOf(agent);

    // Both characters at the body's matrix with the wobble's squash
    otherMatrix = body->matrix;
    PreMultiply(&otherMatrix, &squash);
    MoveMatrix(&otherMatrix, &squashOffset);
    agentMatrix = body->matrix;
    PreMultiply(&agentMatrix, &squash);
    MoveMatrix(&agentMatrix, &squashOffset);

    // The exit upright on the heading (flattened) at the ball's bottom
    Vector4* side = RowOf(&exitMatrix, 0);
    Vector4* up = RowOf(&exitMatrix, 1);
    Vector4* forward = RowOf(&exitMatrix, 2);
    Vector4* bottom = RowOf(&exitMatrix, 3);
    *bottom = *PositionOf(body);
    *forward = heading;
    bottom->y = bottom->y + (ExitRaise - radius);
    forward->y = 0.0f;
    f32 inverse = InverseLength(forward, LengthEpsilon);
    forward->x = forward->x * inverse;
    forward->y = forward->y * inverse;
    forward->z = forward->z * inverse;
    *up = {0.0f, 1.0f, 0.0f, 0.0f};
    // Up times forward (retail takes 1 times z less 0 times y as z itself)
    side->x = forward->z;
    side->y = up->z * forward->x - up->x * forward->z;
    side->z = up->x * forward->y - up->y * forward->x;
    inverse = InverseLength(side, LengthEpsilon);
    side->x = side->x * inverse;
    side->y = side->y * inverse;
    side->z = side->z * inverse;
    forward->w = 0.0f;
    side->w = 0.0f;
    return 1;
}

u32 RollerbrawlVehicle::Kind()
{
    return KindRollerbrawl;
}

void RollerbrawlVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-1.0f, -1.0f, -1.0f, 1.0f};
    *max = {1.0f, 1.0f, 1.0f, 1.0f};
}

u32 RollerbrawlVehicle::HeadFollowsCamera()
{
    return 0;
}

void RollerbrawlVehicle::Roll()
{
    RunAgentEvent(agent, EventRolling, 0, 0, 0);
    RunAgentEvent(partner, EventRolling, 0, 0, 0);
    timer = 0.0f;
    state = StateRolling;
}

void RollerbrawlVehicle::GrowSnow(f32 amount)
{
    snow = snow + amount;
    SetSnow(this, snow);
}

void RollerbrawlVehicle::WearSnow(f32 amount)
{
    snow = snow - amount;
    if (snow < 0.0f)
    {
        snow = 0.0f;
    }

    SetSnow(this, snow);
}

void RollerbrawlVehicle::Destroy(u32 destroyFlags)
{
    vtable = g_RollerbrawlVehicleVTable;
    if (other != nullptr)
    {
        other->instance->flags |= ReferencedObject::FlagSphereContact;
    }

    DestroySkidMarks(&rightMarks, DestroyOnly);
    DestroySkidMarks(&leftMarks, DestroyOnly);
    Vehicle::Destroy(destroyFlags);
}

void RollerbrawlVehicle::SetVelocity(const Vector4* velocity)
{
    BodyOf(agent)->SetVelocity(velocity);
}

u32 RollerbrawlVehicle::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &lastVelocity, 0);
    return 1;
}

void RollerbrawlVehicle::ContactCallback(const CollisionHit* triangle, void* vehicle)
{
    static_cast<RollerbrawlVehicle*>(vehicle)->ApplyStickySurface(triangle);
}

void RollerbrawlVehicle::ApplyStickySurface(const CollisionHit* triangle)
{
    CharacterAgent* character = agent;
    if ((GetTriangleSurface(triangle)->collisionMask & StickySurface) != 0)
    {
        onSticky = 1;
    }

    // Lava melts the snow instead of hurting while there's snow
    if ((GetTriangleSurface(triangle)->contact.word & LavaContact) != 0 && 0.0f < snow)
    {
        if (meltTime < MeltTime)
        {
            meltTime = MeltTime;
        }
    }
    else
    {
        character->SendSurfaceMessage(GetTriangleSurface(triangle));
    }

    if (0.0f < meltTime)
    {
        onSticky = 0;
    }
}

void RollerbrawlVehicle::TouchCallback(InstanceContext* other, u32 hull, void* vehicle)
{
    static_cast<RollerbrawlVehicle*>(vehicle)->TouchedInstance(other, hull);
}

void RollerbrawlVehicle::TouchedInstance(InstanceContext*, u32)
{
}

void RollerbrawlVehicle::Draw()
{
    DrawSkidMarks(&leftMarks);
    DrawSkidMarks(&rightMarks);
}

void RollerbrawlVehicle::SplashSphere(Vector4* centre, f32* radius)
{
    *centre = *PositionOf(BodyOf(agent));
    *radius = this->radius;
}
