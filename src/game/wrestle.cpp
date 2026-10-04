#include "game/vehicles.h"

#include "game/agents.h"
#include "game/camerarig.h"
#include "game/chunkdata.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/player.h"
#include "game/reference.h"
#include "game/rigidbody.h"

EABI_EXPORT(FUN_00154f60, &WrestleVehicle::Frame);
EABI_EXPORT(FUN_00153ac8, &WrestleVehicle::RollFrame);
EABI_EXPORT(FUN_00153770, &WrestleVehicle::Wobble);

namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 BodyNodeKind = 5;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;

// The script events run on the character: falling fast, pinning the creature and pinned by it
constexpr u32 EventFell = 0xB;
constexpr u32 EventPins = 0x67;
constexpr u32 EventPinned = 0x69;

// The ball: a sphere body of a unit mass in a 2.5 box, how it bounces and rubs, its steps and the triangles its cache keeps
constexpr f32 BallRadius = Rounded(0.8);
constexpr f32 BallMass = 1.0f;
constexpr f32 BallSize = 2.5f;
constexpr f32 BallRestitution = 0.35f;
constexpr f32 BallSoftness = 6.0f;
constexpr f32 BallFriction = 1.5f;
constexpr f32 BallSpinFriction = 0.5f;
constexpr f32 BallRollFriction = Rounded(0.07);
constexpr s32 BallSubsteps = 3;
constexpr u32 BallCacheMask = 0x10;
// The two sides of the ball (made units), in the body's space
constexpr f32 SideX = Rounded(1.1);
constexpr f32 SideZ = Rounded(0.4);
// The starting velocity's speed (which retail never gives it)
constexpr f32 StartSpeed = 32.0f;
// The exit stands on the ball's bottom, 0.05 above it
constexpr f32 ExitLift = Rounded(0.05);

// Rolling: gravity, the event of a fast fall
constexpr f32 RollGravity = 35.0f;
constexpr f32 FellSpeed = 40.0f;
// The character's push: in the air along the stick (8 times it), on the ground down at the point 2 along it (its length times
// 10, half again when it pushes against the motion)
constexpr f32 AirPush = 8.0f;
constexpr f32 GroundPush = 10.0f;
// The drags: speeds and spins under 0.09 have none, past it a share of the rest
constexpr f32 DragFreeSpeed = Rounded(0.09);
constexpr f32 DragShare = -0.005f;
constexpr f32 SpinDragShare = -0.0025f;
// The wobble: a change of velocity over 15 (and over the wobble) starts one (a 60th of it, within 0.5) and shakes the camera, one
// over 43 only turns its axis; it swings 21 radians a second and fades 1.5 of itself a second
constexpr f32 WobbleStart = 15.0f;
constexpr f32 WobbleAxisOnly = 43.0f;
constexpr f32 WobbleShare = Rounded(1.0 / 60.0);
constexpr f32 MostWobble = 0.5f;
constexpr f32 WobbleShakeFalloff = 0.01f;
constexpr f32 WobbleSwing = 21.0f;
constexpr f32 WobbleFade = 1.5f;

// Who's on top: the character's side's height over 0.68 (under -0.68 the creature's), kept while over 0.58; on top 1.7 seconds
// (the creature 1.5) pins, a pin lasts 4 and turns the body over 0.7
constexpr f32 OnTop = Rounded(0.68);
constexpr f32 StaysOnTop = 0.58f;
constexpr f32 CharacterPinTime = Rounded(1.7);
constexpr f32 CreaturePinTime = 1.5f;
constexpr f32 PinTime = 4.0f;
constexpr f32 InversePinTurnTime = Rounded(1.0 / 0.7);
// A pin's turn axis shorter than this (squared) turns nothing
constexpr f32 PinAxisEpsilon = 5e-05f;
// The pushes of a frame shorter than this (squared) don't count against each other
constexpr f32 PushEpsilon = 0.0001f;

// The creature: its pushes no longer than 2; heading home at 6 a second, done within 6 or spinning faster than 30, wandering
// past 9 heads home again; tactic 2 lasts 1.5 seconds
constexpr f32 MostPush = 2.0f;
constexpr f32 HomeSpeed = 6.0f;
constexpr f32 HomeRadius = 6.0f;
constexpr f32 HomeSpinLimit = 30.0f;
constexpr f32 WanderLimit = 9.0f;
constexpr f32 VelocityHomeTime = 1.5f;
// The struggle: a new push one time in 60 less its side's height squared times 40, of a length squared between the round's
// least and most; rounds 0 to 5, the last one pressing; pressing when the creature's side faces up more than 0.707
constexpr s32 StruggleOdds = 60;
constexpr f32 StruggleOddsScale = 40.0f;
constexpr f32 RestingLeast = 0.25f;
constexpr f32 RestingMost = 0.35f;
constexpr f32 StruggleLeast = 0.5f;
constexpr f32 StruggleMost = Rounded(0.8);
constexpr f32 LastRoundLeast = 0.75f;
constexpr f32 LastRoundMost = 1.05f;
constexpr s32 Rounds = 6;
constexpr s32 LastRound = 5;
constexpr f32 PressFacing = Rounded(0.707);
// Pressing down: toward the character's side laid flat (its length times 5 and 2 more), a quarter of the way from the velocity,
// no longer than 0.4 and 0.3 a second more up to 1.3; done after 2.4 seconds or once its own side faces down
constexpr f32 PressSpeed = 5.0f;
constexpr f32 PressSpeedBase = 2.0f;
constexpr f32 PressShare = 0.25f;
constexpr f32 PressPushBase = Rounded(0.4);
constexpr f32 PressPushGrowth = Rounded(0.3);
constexpr f32 MostPressPush = 1.3f;
constexpr f32 PressTime = Rounded(2.4);

// The gauge: rising over 1.7 seconds of the character on top, falling over 1.5 of the creature (retail's 1 / 1.7 rounded down)
constexpr f32 InverseCharacterPinTime = 0x1.2d2d2cp-1f;
constexpr f32 InverseCreaturePinTime = Rounded(1.0 / 1.5);

DynamicBody* BodyOf(InstanceContext* instance)
{
    return static_cast<DynamicBody*>(GetGameNode(&instance->nodes, BodyNodeKind));
}

// When the vehicle's Place says so, the character put at agentMatrix and the other at otherMatrix, each queued when its place
// changed (retail works out the rotation of agentMatrix first and drops it)
void PlaceRiders(Vehicle* vehicle)
{
    if (vehicle->Place() == 0)
    {
        return;
    }

    Vector4 rotation;
    GetRotationVec(&rotation, &vehicle->agentMatrix);
    InstanceContext* instance = vehicle->agent->instance;
    if (SetPlaceMatrix(instance->place, &vehicle->agentMatrix) != 0)
    {
        QueueObject(instance);
    }

    if (vehicle->other != nullptr)
    {
        InstanceContext* otherInstance = vehicle->other->instance;
        if (SetPlaceMatrix(otherInstance->place, &vehicle->otherMatrix) != 0)
        {
            QueueObject(otherInstance);
        }
    }
}

// A push cut to a length along itself when it's longer
void LimitPush(Vector4* push, f32 most)
{
    if (most < __builtin_sqrtf(push->x * push->x + push->y * push->y + push->z * push->z))
    {
        f32 inverse = InverseLength(push, LengthEpsilon);
        push->x = push->x * inverse * most;
        push->y = push->y * inverse * most;
        push->z = push->z * inverse * most;
    }
}

f32 Saturated(f32 value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (1.0f < value)
    {
        return 1.0f;
    }

    return value;
}
}

WrestleVehicle* WrestleVehicle::Construct(WrestleVehicle* vehicle, CharacterAgent* agent, Agent* creature)
{
    vehicle->agent = agent;
    vehicle->bits = 0;
    vehicle->other = creature;
    vehicle->vtable = g_WrestleVehicleVTable;
    vehicle->Bits() = (vehicle->Bits() | BitDrives) & ~u64{BitHeld};
    vehicle->Start();
    return vehicle;
}

void WrestleVehicle::Start()
{
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    state = StateRolling;
    tactic = TacticNone;
    heading = *RowOf(&place->matrix, 2);
    radius = BallRadius;
    unknown110 = 0;
    characterSide = {-SideX, 1.0f, SideZ, 1.0f};
    f32 inverse = InverseLength(&characterSide, LengthEpsilon);
    characterSide.x = characterSide.x * inverse;
    characterSide.y = characterSide.y * inverse;
    characterSide.z = characterSide.z * inverse;
    creatureSide = {SideX, -1.0f, -SideZ, 1.0f};
    inverse = InverseLength(&creatureSide, LengthEpsilon);
    creatureSide.x = creatureSide.x * inverse;
    creatureSide.y = creatureSide.y * inverse;
    creatureSide.z = creatureSide.z * inverse;
    Vehicle::Start();
    DynamicBody* body = BodyOf(agent->instance);
    auto* sphere = static_cast<SphereBody*>(BodyOf(agent->instance));
    radius = BallRadius;
    sphere->SetMassAndSize(BallMass, BallSize, BallSize, BallSize);
    sphere->ellipsoid = 0;
    sphere->radius = radius;
    body->SetRestitution(BallRestitution);
    body->SetSoftness(BallSoftness);
    body->SetFriction(BallFriction);
    body->SetSpinAndRollFriction(BallSpinFriction, BallRollFriction);
    *reinterpret_cast<u64*>(&body->bits) &= ~u64{DynamicBody::BitPlacesInstance};
    body->contactArgument = this;
    body->touchArgument = this;
    body->substeps = BallSubsteps;
    body->contactCallback = ContactCallback;
    body->touchCallback = TouchCallback;
    body->SetCacheMask(BallCacheMask);
    body->lengthDrag = 0.0f;
    body->drag = 0.0f;
    other->instance->flags &= ~ReferencedObject::FlagSphereContact;
    lastVelocity = body->velocity;
    wobbleAxis = g_DefaultBox.min;
    wobbleAxis.w = 1.0f;
    wobble = 0.0f;
    InitIdentityMatrix(&squash);
    squashOffset = g_DefaultBox.min;
    squashOffset.w = 1.0f;
    wobblePhase = 0.0f;
    home = *RowOf(&place->matrix, 3);
    stateTime = 0.0f;
    tacticTime = 0.0f;
    characterPush = g_DefaultBox.min;
    characterPush.w = 1.0f;
    creaturePush = g_DefaultBox.min;
    creaturePush.w = 1.0f;
    restTime = 0.0f;
    resting = 0;
    struggleX = 0.0f;
    struggleZ = 0.0f;
    round = 0;
}

// The character and the creature at the body's matrix squashed by the wobble, the exit upright on the heading at the ball's
// bottom
u32 WrestleVehicle::Place()
{
    DynamicBody* body = BodyOf(agent->instance);
    otherMatrix = body->matrix;
    PreMultiply(&otherMatrix, &squash);
    Vector4* otherPosition = RowOf(&otherMatrix, 3);
    otherPosition->x = otherPosition->x + squashOffset.x;
    otherPosition->y = otherPosition->y + squashOffset.y;
    otherPosition->z = otherPosition->z + squashOffset.z;
    agentMatrix = body->matrix;
    PreMultiply(&agentMatrix, &squash);
    Vector4* agentPosition = RowOf(&agentMatrix, 3);
    agentPosition->x = agentPosition->x + squashOffset.x;
    agentPosition->y = agentPosition->y + squashOffset.y;
    agentPosition->z = agentPosition->z + squashOffset.z;
    Vector4* exitPosition = RowOf(&exitMatrix, 3);
    *exitPosition = *RowOf(&body->matrix, 3);
    Vector4* side = RowOf(&exitMatrix, 0);
    Vector4* up = RowOf(&exitMatrix, 1);
    Vector4* forward = RowOf(&exitMatrix, 2);
    *forward = heading;
    exitPosition->y = exitPosition->y + (ExitLift - radius);
    forward->y = 0.0f;
    f32 inverse = InverseLength(forward, LengthEpsilon);
    forward->x = forward->x * inverse;
    forward->y = forward->y * inverse;
    forward->z = forward->z * inverse;
    *up = {0.0f, 1.0f, 0.0f, 0.0f};
    // The up axis across the forward one (retail's x is the forward z as it is, its zero terms dropped)
    side->x = forward->z;
    side->y = up->z * forward->x - up->x * forward->z;
    side->z = up->x * forward->y - up->y * forward->x;
    inverse = InverseLength(side, LengthEpsilon);
    side->x = side->x * inverse;
    side->y = side->y * inverse;
    side->z = side->z * inverse;
    side->w = 0.0f;
    forward->w = 0.0f;
    return 1;
}

void WrestleVehicle::Destroy(u32 destroyFlags)
{
    vtable = g_WrestleVehicleVTable;
    other->instance->flags |= ReferencedObject::FlagSphereContact;
    Vehicle::Destroy(destroyFlags);
}

// Only through links with bit 18, its last velocity turned through it
u32 WrestleVehicle::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &lastVelocity, 0);
    return 1;
}

// Rolling, or held while someone pins; the creature's tactic and who's on top stepped, how much the two pushes oppose shown by
// the character's trails, placed
void WrestleVehicle::Frame(f32 seconds)
{
    characterPush = g_DefaultBox.min;
    characterPush.w = 1.0f;
    creaturePush = g_DefaultBox.min;
    creaturePush.w = 1.0f;
    if (state == StateCharacterPins || state == StateCreaturePins)
    {
        PinnedFrame();
    }
    else
    {
        RollFrame(seconds);
    }

    StepTactic();
    StepState();
    stateTime = stateTime + seconds;
    tacticTime = tacticTime + seconds;
    f32 opposed = 0.0f;
    f32 characterSquared = characterPush.x * characterPush.x + characterPush.y * characterPush.y
                           + characterPush.z * characterPush.z;
    f32 creatureSquared = creaturePush.x * creaturePush.x + creaturePush.y * creaturePush.y + creaturePush.z * creaturePush.z;
    if (PushEpsilon < characterSquared && PushEpsilon < creatureSquared)
    {
        if (1.0f < characterSquared)
        {
            f32 inverse = InverseLength(&characterPush, LengthEpsilon);
            characterPush.x = characterPush.x * inverse;
            characterPush.y = characterPush.y * inverse;
            characterPush.z = characterPush.z * inverse;
        }

        if (1.0f < creatureSquared)
        {
            f32 inverse = InverseLength(&creaturePush, LengthEpsilon);
            creaturePush.x = creaturePush.x * inverse;
            creaturePush.y = creaturePush.y * inverse;
            creaturePush.z = creaturePush.z * inverse;
        }

        f32 along = characterPush.x * creaturePush.x + characterPush.y * creaturePush.y + characterPush.z * creaturePush.z;
        if (along <= 0.0f)
        {
            opposed = -along;
        }
    }

    // Retail doesn't check for the object node
    auto* node = static_cast<ObjectNode*>(GetGameNode(&agent->instance->nodes, ObjectNodeKind));
    ParticleTrails* trails = node->particleTrails;
    if (trails != nullptr)
    {
        trails->strength = opposed;
    }

    PlaceRiders(this);
}

u32 WrestleVehicle::Kind()
{
    return KindWrestle;
}

void WrestleVehicle::CollisionBox(Vector4* min, Vector4* max)
{
    *min = {-1.0f, -1.0f, -1.0f, 1.0f};
    *max = {1.0f, 1.0f, 1.0f, 1.0f};
}

// Retail stops the body and then hands ApplyImpulse the zero point as the impulse and the velocity (made 32 long) as the point:
// its arguments swapped, the ball always starts still
void WrestleVehicle::SetVelocity(const Vector4* velocity)
{
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 start = *velocity;
    f32 inverse = InverseLength(&start, LengthEpsilon);
    start.x = start.x * inverse * StartSpeed;
    start.y = start.y * inverse * StartSpeed;
    start.z = start.z * inverse * StartSpeed;
    body->SetVelocity(&g_DefaultBox.min);
    body->ApplyImpulse(&g_DefaultBox.min, &start);
}

u32 WrestleVehicle::HeadFollowsCamera()
{
    return 0;
}

u32 WrestleVehicle::HasBody()
{
    return 1;
}

// Gravity and the character's push, the drags and the wobble; a fast fall runs the character's event
void WrestleVehicle::RollFrame(f32 seconds)
{
    DynamicBody* body = BodyOf(agent->instance);
    u32 touching = (body->bodyFlags & (RigidBody::FlagTouchedBody | RigidBody::FlagTouched)) != 0;
    // Its weight (retail drops the zero axes' products)
    const Vector4 weight = {0.0f, body->mass * -RollGravity, 0.0f, 1.0f};
    body->force.x = body->force.x + weight.x;
    body->force.y = body->force.y + weight.y;
    body->force.z = body->force.z + weight.z;
    StickPush(touching);
    Drag();
    SpinDrag();
    Wobble(seconds);
    if (body->velocity.y < -FellSpeed)
    {
        RunAgentEvent(agent, EventFell, reinterpret_cast<u32>(agent->instance), 0, 0);
    }
}

// Held at the pin's position, turned from where the pin started to the pinned rotation (eased in and out); Place is called and
// its answer dropped (the frame places them again)
void WrestleVehicle::PinnedFrame()
{
    f32 share = stateTime * InversePinTurnTime;
    if (1.0f < share)
    {
        share = 1.0f;
    }

    f32 eased = share * (share * 3.0f) - (share + share) * share * share;
    Vector4 rotation;
    SlerpRotations(eased, &rotation, &pinFrom, &pinTo);
    Matrix4x4 matrix;
    MatrixFromRotation(&matrix, &rotation);
    *RowOf(&matrix, 3) = pinPosition;
    BodyOf(agent->instance)->SetMatrix(&matrix);
    Vehicle::Place();
}

// The character's side in the world says who's on top; on top long enough pins, a pin runs out
void WrestleVehicle::StepState()
{
    Vector4 side;
    VuRotateVector(&BodyOf(agent->instance)->matrix, &characterSide, &side);
    switch (state)
    {
    case StateRolling:
        if (OnTop < side.y)
        {
            stateTime = 0.0f;
            state = StateCharacterOnTop;
        }
        else if (side.y < -OnTop)
        {
            stateTime = 0.0f;
            state = StateCreatureOnTop;
        }

        break;

    case StateCharacterOnTop:
        if (side.y < StaysOnTop)
        {
            stateTime = 0.0f;
            state = StateRolling;
        }
        else if (CharacterPinTime < stateTime)
        {
            RunAgentEvent(agent, EventPins, 0, 0, 0);
            Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
            stateTime = 0.0f;
            state = StateCharacterPins;
            StartPin(&up);
        }

        break;

    case StateCreatureOnTop:
        if (-StaysOnTop < side.y)
        {
            stateTime = 0.0f;
            state = StateRolling;
        }
        else if (CreaturePinTime < stateTime)
        {
            RunAgentEvent(agent, EventPinned, 0, 0, 0);
            Vector4 up = {0.0f, -1.0f, 0.0f, 1.0f};
            stateTime = 0.0f;
            state = StateCreaturePins;
            StartPin(&up);
        }

        break;

    case StateCharacterPins:
    case StateCreaturePins:
        if (PinTime < stateTime)
        {
            stateTime = 0.0f;
            state = StateRolling;
        }

        break;
    }
}

void WrestleVehicle::StartPin(const Vector4* up)
{
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    pinPosition = *RowOf(&place->matrix, 3);
    GetRotationVec(&pinFrom, &place->matrix);
    Vector4 target = *up;
    Vector4 localUp = {0.0f, 1.0f, 0.0f, 1.0f};
    Vector4 currentUp;
    VuRotateVector(&place->matrix, &localUp, &currentUp);
    Vector4 axis = {currentUp.y * target.z - currentUp.z * target.y, currentUp.z * target.x - currentUp.x * target.z,
                    currentUp.x * target.y - currentUp.y * target.x, 1.0f};
    if (PinAxisEpsilon < axis.x * axis.x + axis.y * axis.y + axis.z * axis.z)
    {
        f32 inverse = InverseLength(&axis, LengthEpsilon);
        f32 cosine = currentUp.x * target.x + currentUp.y * target.y + currentUp.z * target.z;
        axis.x = axis.x * inverse;
        axis.z = axis.z * inverse;
        axis.y = axis.y * inverse;
        s32 angle;
        AngleOfCosine(cosine, &angle);
        Vector4 turn;
        RotationAboutAxis(&turn, &axis, &angle, 0);
        pinTo = turn;
        MultiplyRotations(&pinTo, &pinTo, &pinFrom);
    }
    else
    {
        pinTo = pinFrom;
    }
}

// The creature's tactic (none while someone pins): the last round presses
void WrestleVehicle::StepTactic()
{
    if (state == StateCharacterPins || state == StateCreaturePins)
    {
        return;
    }

    if (round == LastRound)
    {
        tactic = TacticPress;
    }

    switch (tactic)
    {
    case TacticNone:
        tactic = TacticStruggle;
        break;

    case TacticHeadHome:
        HeadHome();
        break;

    case TacticHeadHomeByVelocity:
    {
        // Retail heads along home less the body's velocity, where the other tactics go by its position (nothing sets tactic 2)
        const Vector4& velocity = BodyOf(agent->instance)->velocity;
        Vector4 push = {home.x - velocity.x, home.y - velocity.y, home.z - velocity.z, 1.0f};
        LimitPush(&push, MostPush);
        CreaturePush(&push);
        if (VelocityHomeTime < tacticTime)
        {
            tactic = TacticNone;
        }

        break;
    }

    case TacticStruggle:
        Struggle();
        break;

    case TacticPress:
        PressDown();
        break;
    }
}

// Toward home at a steady speed, done once there (or spinning too fast)
void WrestleVehicle::HeadHome()
{
    DynamicBody* body = BodyOf(agent->instance);
    const Vector4& spin = body->angularVelocity;
    if (HomeSpinLimit < __builtin_sqrtf(spin.x * spin.x + spin.y * spin.y + spin.z * spin.z))
    {
        tactic = TacticNone;
        return;
    }

    const Vector4* position = RowOf(&body->matrix, 3);
    Vector4 flat = {home.x - position->x, home.y - position->y, home.z - position->z, 1.0f};
    flat.y = 0.0f;
    Vector4 direction = flat;
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    const Vector4& velocity = body->velocity;
    Vector4 push = {direction.x * HomeSpeed - velocity.x, direction.y * HomeSpeed - velocity.y,
                    direction.z * HomeSpeed - velocity.z, 1.0f};
    LimitPush(&push, MostPush);
    CreaturePush(&push);
    if (__builtin_sqrtf(flat.x * flat.x + flat.y * flat.y + flat.z * flat.z) < HomeRadius)
    {
        tactic = TacticNone;
    }
}

// Random pushes for the round's time, then a rest; home when it wandered off, pressing when its own side is up
void WrestleVehicle::Struggle()
{
    Vector4 side;
    VuRotateVector(&BodyOf(agent->instance)->matrix, &characterSide, &side);
    s32 odds = StruggleOdds - static_cast<s32>(side.y * side.y * StruggleOddsScale);
    if (resting == 0 && RandomBelow(odds) == 0)
    {
        f32 least;
        f32 most;
        // Retail's range for a push while resting is never taken: it only picks one while it isn't
        if (resting != 0)
        {
            least = RestingLeast;
            most = RestingMost;
        }
        else if (round < LastRound)
        {
            least = StruggleLeast;
            most = StruggleMost;
        }
        else
        {
            least = LastRoundLeast;
            most = LastRoundMost;
        }

        f32 lengthSquared;
        do
        {
            struggleX = RandomSignedTimes(1.0f);
            struggleZ = RandomSignedTimes(1.0f);
            lengthSquared = struggleX * struggleX + struggleZ * struggleZ;
        } while (lengthSquared < least || most < lengthSquared);
    }

    if (resting != 0)
    {
        if (restTime < tacticTime)
        {
            resting = 0;
            tacticTime = 0.0f;
        }
    }
    else if (g_WrestleStruggleTimes[round] < tacticTime)
    {
        restTime = g_WrestleRestTimes[round];
        resting = 1;
        tacticTime = 0.0f;
        round = round + 1 < Rounds ? round + 1 : LastRound;
        struggleX = 0.0f;
        struggleZ = 0.0f;
    }

    Vector4 push = {struggleX, 0.0f, struggleZ, 1.0f};
    CreaturePush(&push);
    const Vector4* position = RowOf(&BodyOf(agent->instance)->matrix, 3);
    f32 x = home.x - position->x;
    f32 z = home.z - position->z;
    if (WanderLimit < __builtin_sqrtf(x * x + z * z))
    {
        tactic = TacticHeadHome;
        return;
    }

    Vector4 creatureUp;
    VuRotateVector(&BodyOf(agent->instance)->matrix, &creatureSide, &creatureUp);
    if (PressFacing < creatureUp.y)
    {
        tactic = TacticPress;
    }
}

// A push turning the character's side toward the ground, harder the longer it lasts; home when it wandered off, done after a
// while or once its own side faces down
void WrestleVehicle::PressDown()
{
    Vector4 side;
    VuRotateVector(&BodyOf(agent->instance)->matrix, &characterSide, &side);
    // Retail works out the angle between the side and down and drops it
    Vector4 down = {0.0f, -1.0f, 0.0f, 1.0f};
    s32 angle;
    AngleOfCosine(side.x * down.x + side.y * down.y + side.z * down.z, &angle);
    DynamicBody* body = BodyOf(agent->instance);
    f32 length = __builtin_sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
    Vector4 velocity = body->velocity;
    side.y = 0.0f;
    f32 inverse = InverseLength(&side, LengthEpsilon);
    f32 speed = length * PressSpeed + PressSpeedBase;
    side.x = side.x * inverse;
    side.y = side.y * inverse;
    side.z = side.z * inverse;
    f32 most = tacticTime * PressPushGrowth + PressPushBase;
    if (MostPressPush < most)
    {
        most = MostPressPush;
    }

    Vector4 push = {(side.x * speed - velocity.x) * PressShare, (side.y * speed - velocity.y) * PressShare,
                    (side.z * speed - velocity.z) * PressShare, 1.0f};
    LimitPush(&push, most);
    CreaturePush(&push);
    const Vector4* position = RowOf(&body->matrix, 3);
    f32 x = home.x - position->x;
    f32 z = home.z - position->z;
    if (WanderLimit < __builtin_sqrtf(x * x + z * z))
    {
        tactic = TacticHeadHome;
        return;
    }

    Vector4 creatureUp;
    VuRotateVector(&BodyOf(agent->instance)->matrix, &creatureSide, &creatureUp);
    if (PressTime < tacticTime)
    {
        tactic = TacticNone;
        return;
    }

    if (creatureUp.y < -PressFacing)
    {
        tactic = TacticNone;
    }
}

// The character's push: touching something, down at a point along the stick (harder against the motion), kept for the frame;
// in the air, a force along the stick
void WrestleVehicle::StickPush(u32 touching)
{
    DynamicBody* body = BodyOf(agent->instance);
    const CharacterButtons& buttons = agent->buttons;
    Vector4 stick = {buttons.moveX, 0.0f, buttons.moveZ, 1.0f};
    if (touching == 0)
    {
        Vector4 push = {stick.x * AirPush, stick.y * AirPush, stick.z * AirPush, 1.0f};
        Vector4 force = {push.x * body->mass, push.y * body->mass, push.z * body->mass, 1.0f};
        body->force.x = body->force.x + force.x;
        body->force.y = body->force.y + force.y;
        body->force.z = body->force.z + force.z;
        return;
    }

    Vector4 point = *RowOf(&body->matrix, 3);
    f32 length = __builtin_sqrtf(stick.x * stick.x + stick.y * stick.y + stick.z * stick.z);
    point.x = point.x + (stick.x + stick.x);
    point.y = point.y + (stick.y + stick.y);
    point.z = point.z + (stick.z + stick.z);
    Vector4 force = {0.0f, -(length * GroundPush), 0.0f, 1.0f};
    Vector4 moving = body->velocity;
    moving.y = 0.0f;
    f32 inverse = InverseLength(&moving, LengthEpsilon);
    moving.x = moving.x * inverse;
    moving.y = moving.y * inverse;
    moving.z = moving.z * inverse;
    characterPush = stick;
    f32 against = Saturated((1.0f - (stick.x * moving.x + stick.y * moving.y + stick.z * moving.z)) * 0.5f);
    f32 scale = against * 0.5f + 1.0f;
    force.x = force.x * scale;
    force.y = force.y * scale;
    force.z = force.z * scale;
    body->AddForce(&force, &point);
}

// The creature's push, kept for the frame: down at the point twice it from the body's middle, its length times 10
void WrestleVehicle::CreaturePush(const Vector4* push)
{
    DynamicBody* body = BodyOf(agent->instance);
    creaturePush = *push;
    Vector4 point = *RowOf(&body->matrix, 3);
    f32 length = __builtin_sqrtf(push->x * push->x + push->y * push->y + push->z * push->z);
    point.x = point.x + (push->x + push->x);
    point.y = point.y + (push->y + push->y);
    point.z = point.z + (push->z + push->z);
    Vector4 force = {0.0f, -(length * GroundPush), 0.0f, 1.0f};
    body->AddForce(&force, &point);
}

void WrestleVehicle::Drag()
{
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 velocity = body->velocity;
    f32 speed = __builtin_sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    f32 over = speed < DragFreeSpeed ? 0.0f : speed - DragFreeSpeed;
    f32 share = over * DragShare;
    body->force.x = body->force.x + velocity.x * share * body->mass;
    body->force.y = body->force.y + velocity.y * share * body->mass;
    body->force.z = body->force.z + velocity.z * share * body->mass;
}

void WrestleVehicle::SpinDrag()
{
    DynamicBody* body = BodyOf(agent->instance);
    Vector4 spin = body->angularVelocity;
    f32 speed = __builtin_sqrtf(spin.x * spin.x + spin.y * spin.y + spin.z * spin.z);
    f32 over = speed < DragFreeSpeed ? 0.0f : speed - DragFreeSpeed;
    f32 share = over * SpinDragShare;
    body->torque.x = body->torque.x + spin.x * share;
    body->torque.y = body->torque.y + spin.y * share;
    body->torque.z = body->torque.z + spin.z * share;
}

// A hard change of velocity starts a wobble about its direction (in the body's space) and shakes the camera; a wobbling ball is
// squashed along that axis by the wobble's swing
void WrestleVehicle::Wobble(f32 seconds)
{
    DynamicBody* body = BodyOf(agent->instance);
    InitIdentityMatrix(&squash);
    Vector4 change = {body->velocity.x - lastVelocity.x, body->velocity.y - lastVelocity.y, body->velocity.z - lastVelocity.z,
                      1.0f};
    f32 changeSquared = change.x * change.x + change.y * change.y + change.z * change.z;
    lastVelocity = body->velocity;
    f32 hit = __builtin_sqrtf(changeSquared);
    if (WobbleAxisOnly < hit)
    {
        wobbleAxis = change;
        f32 inverse = InverseLength(&wobbleAxis, LengthEpsilon);
        wobbleAxis.x = wobbleAxis.x * inverse;
        wobbleAxis.y = wobbleAxis.y * inverse;
        wobbleAxis.z = wobbleAxis.z * inverse;
        VuRotateVector(&body->inverseMatrix, &wobbleAxis, &wobbleAxis);
    }
    else if (WobbleStart < hit && wobble < hit)
    {
        wobble = hit * WobbleShare;
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
        PushCameraShake(&g_CameraShake, RowOf(&body->matrix, 3), strength + strength, WobbleShakeFalloff);
        wobblePhase = 0.0f;
    }

    if (wobble == 0.0f)
    {
        return;
    }

    s32 angle;
    AngleFrom(&angle, wobblePhase, AngleRadians);
    f32 squeeze = wobble * CosOfAngle(&angle);
    wobblePhase = wobblePhase + seconds * WobbleSwing;
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

void WrestleVehicle::TouchedSurface(const CollisionHit* triangle)
{
    CharacterAgent* character = agent;
    character->SendSurfaceMessage(GetTriangleSurface(triangle));
}

void WrestleVehicle::TouchedInstance(InstanceContext*, u32)
{
}

void WrestleVehicle::ContactCallback(const CollisionHit* triangle, void* vehicle)
{
    static_cast<WrestleVehicle*>(vehicle)->TouchedSurface(triangle);
}

void WrestleVehicle::TouchCallback(InstanceContext* other, u32 hull, void* vehicle)
{
    static_cast<WrestleVehicle*>(vehicle)->TouchedInstance(other, hull);
}

extern "C"
{
    // The wrestle's balance for the HUD's slider: even at 0.5, toward 1 while the character is on top and 1 while it pins, toward
    // 0 while the creature is on top and 0 while it pins (only a wrestle is asked)
    f32 VehicleGauge(CharacterControl* control)
    {
        const auto* vehicle = reinterpret_cast<const WrestleVehicle*>(control);
        switch (vehicle->state)
        {
        case WrestleVehicle::StateCharacterOnTop:
            return Saturated(vehicle->stateTime * InverseCharacterPinTime) * 0.5f + 0.5f;

        case WrestleVehicle::StateCreatureOnTop:
            return 0.5f - Saturated(vehicle->stateTime * InverseCreaturePinTime) * 0.5f;

        case WrestleVehicle::StateCharacterPins:
            return 1.0f;

        case WrestleVehicle::StateCreaturePins:
            return 0.0f;

        default:
            return 0.5f;
        }
    }
}
