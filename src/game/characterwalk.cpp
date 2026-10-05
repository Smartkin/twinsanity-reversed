#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"

EABI_EXPORT(FUN_0014efe0, &WalkController::NextState);
EABI_EXPORT(FUN_0014f330, &WalkController::AskTurn);
EABI_EXPORT(FUN_0014f778, &WalkController::Strafe);
EABI_EXPORT(FUN_0014fb10, &WalkController::Pushed);
EABI_EXPORT(FUN_0014fdf8, &WalkController::Frame);
EABI_EXPORT(FUN_00160420, &WalkController::PushAtSpeed);
EABI_EXPORT(FUN_001604b8, &WalkController::BouncePush);

namespace
{
// The stick's and the strafe's dead zone, the stick past which it walks (barely, when it's controlled) and runs, and how far
// the move's direction may be off the facing before it shuffles
constexpr f32 StickEpsilon = Epsilon;
constexpr f32 WalkStick = Rounded(0.3);
constexpr f32 ControlledWalkStick = Rounded(0.0001);
constexpr f32 RunStick = Rounded(0.96);
constexpr f32 ShuffleDegrees = 5.0f;
// The stick's turn (1 a half turn) is eased within 45 degrees and between 45 and 135
constexpr f32 HalfTurnDegrees = 180.0f;
constexpr f32 EaseNearDegrees = 45.0f;
constexpr f32 EaseFarDegrees = 135.0f;
constexpr f32 EaseNearLeast = Rounded(0.05);
constexpr f32 EaseNearMost = 0.5f;
constexpr f32 EaseFarLeast = 0.5f;
// Mecha-Bandicoot's strafe turns half a turn a second and ends a quarter second in
constexpr f32 MechaStrafeTurnDegrees = 180.0f;
constexpr f32 MechaStrafeSeconds = 0.25f;
// A push in the air is kept while the stick is within this
constexpr f32 PushedStick = Rounded(0.3);
// The tied slam: its speed (times the stick) and its turn rate (radians a second)
constexpr f32 SlamSpeed = 4.0f;
constexpr f32 SlamTurnRadians = 8.0f;

CharacterPart* PartOf(const CharacterAgent* agent)
{
    return static_cast<CharacterPart*>(agent->part);
}

s32 CharacterKindOf(const CharacterAgent* agent)
{
    return agent->properties->GetInt(CharacterKindProperty);
}

s32 Magnitude(s32 angle)
{
    return angle < 0 ? static_cast<s32>(0u - static_cast<u32>(angle)) : angle;
}

// The part asked to move at the speed along an angle from the facing (forward and sideways) and to turn
void AskAlong(WalkController* walk, s32 angle)
{
    CharacterPart* part = PartOf(walk->agent);
    f32 cosine;
    f32 sine;
    CosSin16(&angle, &cosine, &sine);
    f32 speed = walk->speed;
    part->RequestForward(speed * cosine);
    part->RequestSideways(speed * -sine);
    s32 turn = walk->turn;
    part->RequestTurn(&turn);
}

// The strafe's time is over: none but Mecha-Bandicoot's quarter second (its strafe ends on its own)
bool StrafeOver(const WalkController* walk, s32 elapsed)
{
    f32 seconds = CharacterKindOf(walk->agent) == CharacterMecha ? MechaStrafeSeconds : 0.0f;
    return elapsed >= static_cast<s32>(seconds * g_ClockUnitsPerSecond);
}

// The speed when it's controlled (the stick times its top speed), else the state's own
f32 SpeedFor(WalkController* walk, f32 stick, f32 own)
{
    if (walk->agent->Controlled() != 0)
    {
        return stick * walk->TopSpeed();
    }

    return own;
}

// Walking and running: the speed, the turn at the state's rate (a tagged property), along the move's direction for a second
// after it stood, then straight ahead
void Stride(WalkController* walk, f32 stick, f32 turn, TimeClock* clock, u32 speedProperty, u32 turnProperty)
{
    PropertyHolder* properties = walk->agent->properties;
    f32 own = properties->GetFloat(speedProperty);
    walk->speed = SpeedFor(walk, stick, own);
    TaggedValue rate;
    PropertyHolder::GetTagged(&rate, properties, turnProperty);
    walk->AskTurn(turn, clock, &rate.raw);
    walk->AskMove(static_cast<s32>(clock->time - walk->stoodTime) >= static_cast<s32>(g_ClockUnitsPerSecond));
}

void ResetWalk(WalkController* walk)
{
    // Retail clears the word with memset
    walk->bits.value = 0;
    walk->speed = 0.0f;
    walk->bits.state = WalkController::StateNone;
    walk->strafe = 0.0f;
    walk->airSpeed = 0.0f;
    walk->moveDirection = g_DefaultBox.min;
    walk->moveDirection.w = 1.0f;
    walk->faceDirection = g_DefaultBox.min;
    walk->faceDirection.w = 1.0f;
    walk->turn = 0;
    walk->airTurn = 0;
    walk->pushVelocity = g_DefaultBox.min;
    walk->pushVelocity.w = 1.0f;
}
} // namespace

WalkController* WalkController::Construct(WalkController* walk, CharacterAgent* agent)
{
    walk->agent = agent;
    walk->Reset();
    return walk;
}

void WalkController::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void WalkController::Reset()
{
    ResetWalk(this);
}

void WalkController::ResetCopy()
{
    ResetWalk(this);
}

f32 WalkController::TopSpeed()
{
    PropertyHolder* properties = agent->properties;
    f32 run = properties->GetFloat(PropRunSpeed);
    if (0.0f < run)
    {
        return run;
    }

    return properties->GetFloat(PropWalkSpeed);
}

u32 WalkController::Strafes()
{
    return !(__builtin_fabsf(agent->properties->GetFloat(PropStrafeSpeed)) <= StickEpsilon);
}

void WalkController::BeIdle()
{
    bits.next = StateIdle;
    bits.state = StateIdle;
    PartOf(agent)->bits.attackKind = AttackWalkInto;
}

void WalkController::PushAtSpeed(f32 from, f32 to, s32 ticks)
{
    bits.next = StatePushed;
    bits.pushVelocity = 0;
    bits.pushEases = ticks != 0;
    pushSpeedFrom = from;
    pushSpeedTo = to;
    pushTicks = ticks;
}

void WalkController::PushAtVelocity(const Vector4* from, const Vector4* to, s32 ticks)
{
    pushFrom = *from;
    bits.next = StatePushed;
    bits.pushVelocity = 1;
    pushTo = *to;
    pushTicks = ticks;
    bits.pushEases = ticks != 0;
}

u32 WalkController::BouncePush(f32 restitution, const Vector4* normal)
{
    f32 along = normal->x * pushVelocity.x + normal->y * pushVelocity.y + normal->z * pushVelocity.z;
    f32 scale = restitution + 1.0f;
    f32 x = normal->x * scale * -along;
    f32 y = normal->y * scale * -along;
    f32 z = normal->z * scale * -along;
    pushVelocity.x += x;
    pushVelocity.y += y;
    pushVelocity.z += z;
    return 0;
}

u32 WalkController::NextState(f32 stick, f32 strafe, TimeClock* clock)
{
    CharacterAgent* agent = this->agent;
    CharacterPart* part = PartOf(agent);
    if (part->bits.attackKind == AttackTied)
    {
        return bits.state == StateSlamming ? 0 : StateSlamming;
    }

    if (part->flags.onGround == 0)
    {
        u32 state = bits.state;
        if (state == StateInAir)
        {
            return 0;
        }

        if (state != StatePushed)
        {
            JumpController* jump = agent->jump;
            if (jump != nullptr)
            {
                airSpeed = jump->airSpeed;
                airTurn = jump->airTurn;
            }
        }

        return StateInAir;
    }

    if (part->moveBits.crouching != 0)
    {
        return bits.state == StateNone ? 0 : StateNone;
    }

    if (bits.strafeHeld == 0 && !(__builtin_fabsf(strafe) <= StickEpsilon) && Strafes() != 0)
    {
        if (bits.state == StateStrafing)
        {
            return 0;
        }

        RunAgentEvent(this->agent, strafe < 0.0f ? EventStrafeLeft : EventStrafeRight, 0, 0, 0);
        return StateStrafing;
    }

    f32 runSpeed = this->agent->properties->GetFloat(PropRunSpeed);
    f32 walkStick = this->agent->Controlled() != 0 ? ControlledWalkStick : WalkStick;
    if (RunStick < stick && 0.0f < runSpeed)
    {
        RunAgentEvent(this->agent, EventRun, 0, 0, 0);
        return StateRunning;
    }

    if (walkStick < stick)
    {
        RunAgentEvent(this->agent, EventWalk, 0, 0, 0);
        return StateWalking;
    }

    // Standing: the time kept, shuffling while the move's direction is off the facing
    s32 angle = 0;
    if (!(moveDirection.x * moveDirection.x + moveDirection.z * moveDirection.z < StickEpsilon))
    {
        ObjectPlace* place = this->agent->instance->place;
        RotateAndTranslate(place);
        SignedAngleAboutY(&angle, &moveDirection, RowOf(&place->matrix, 2));
    }

    s32 off = Magnitude(angle);
    stoodTime = clock->time;
    s32 most;
    AngleFrom(&most, ShuffleDegrees, AngleDegrees);
    if (most < off)
    {
        RunAgentEvent(this->agent, EventShuffleFeet, 0, 0, 0);
        return StateShuffling;
    }

    if (bits.state == StateIdle)
    {
        return 0;
    }

    RunAgentEvent(this->agent, EventIdle, 0, 0, 0);
    return StateIdle;
}

void WalkController::AskTurn(f32 turn, TimeClock* clock, const s32* rate)
{
    f32 seconds = static_cast<s32>(clock->advance) * g_SecondsPerClockUnit;
    if (__builtin_fabsf(faceDirection.x) <= StickEpsilon && __builtin_fabsf(faceDirection.y) <= StickEpsilon
        && __builtin_fabsf(faceDirection.z) <= StickEpsilon)
    {
        s32 near;
        s32 far;
        AngleFrom(&near, EaseNearDegrees, AngleDegrees);
        AngleFrom(&far, EaseFarDegrees, AngleDegrees);
        s32 perSecond = *rate;
        s32 wanted;
        AngleFrom(&wanted, turn * HalfTurnDegrees, AngleDegrees);
        s32 most = static_cast<s32>(static_cast<f32>(perSecond) * seconds);
        s32 least = static_cast<s32>(static_cast<f32>(perSecond) * -seconds);
        if (-near < wanted && wanted < near)
        {
            f32 share = static_cast<f32>(wanted) / static_cast<f32>(near);
            f32 squared = share * share;
            wanted = static_cast<s32>(static_cast<f32>(wanted) * (squared * EaseNearMost + (1.0f - squared) * EaseNearLeast));
        }
        else if (-far < wanted && wanted < far)
        {
            // Retail bug: the share is the turn's distance from +45 degrees for turns either way, so turns the other way are
            // scaled 1 to 1.5 instead of 0.5 to 1
            f32 share = __builtin_fabsf(static_cast<f32>(wanted - near) / static_cast<f32>(far - near));
            wanted = static_cast<s32>(static_cast<f32>(wanted) * (share + (1.0f - share) * EaseFarLeast));
        }

        if (most < wanted)
        {
            this->turn = most;
        }
        else if (wanted < least)
        {
            this->turn = least;
        }
        else
        {
            this->turn = wanted;
        }
    }
    else
    {
        ObjectPlace* place = agent->instance->place;
        RotateAndTranslate(place);
        s32 angle;
        SignedAngleAboutY(&angle, RowOf(&place->matrix, 2), &faceDirection);
        this->turn = angle;
    }

    this->turn = static_cast<s32>(static_cast<f32>(this->turn) / seconds);
}

void WalkController::AskMove(u32 straight)
{
    if (straight != 0)
    {
        CharacterPart* part = PartOf(agent);
        part->RequestForward(speed);
        part->RequestSideways(0.0f);
        s32 wanted = turn;
        part->RequestTurn(&wanted);
        return;
    }

    s32 angle = 0;
    if (!(moveDirection.x * moveDirection.x + moveDirection.z * moveDirection.z < StickEpsilon))
    {
        ObjectPlace* place = agent->instance->place;
        RotateAndTranslate(place);
        SignedAngleAboutY(&angle, &moveDirection, RowOf(&place->matrix, 2));
    }

    AskAlong(this, angle);
}

void WalkController::Strafe(f32, f32 strafe, TimeClock* clock, s32* next)
{
    bool held = !(__builtin_fabsf(strafe) <= StickEpsilon);
    PropertyHolder* properties = agent->properties;
    InstanceContext* instance = agent->instance;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    // It faces its first waypoint when it has one
    const Vector4* target = nullptr;
    Waypoints* waypoints = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject))->waypoints;
    if (waypoints != nullptr && waypoints->keyCount != 0)
    {
        target = &waypoints->positions.data[0]->position;
    }

    s32 elapsed = clock->time - stateStart;
    if (held)
    {
        this->strafe = strafe;
    }

    if (target != nullptr)
    {
        faceDirection = *target;
        faceDirection.w = 1.0f;
        const Vector4* position = RowOf(&place->matrix, 3);
        faceDirection.x -= position->x;
        faceDirection.z -= position->z;
        faceDirection.y = 0.0f;
        f32 inverse = InverseLength(&faceDirection, LengthEpsilon);
        faceDirection.x *= inverse;
        faceDirection.y *= inverse;
        faceDirection.z *= inverse;
    }
    else
    {
        faceDirection = *RowOf(&place->matrix, 2);
    }

    // Sideways: the facing across the up axis, the strafe's way
    const Vector4* up = RowOf(&place->matrix, 1);
    moveDirection.x = faceDirection.y * up->z - faceDirection.z * up->y;
    moveDirection.y = faceDirection.z * up->x - faceDirection.x * up->z;
    moveDirection.z = faceDirection.x * up->y - faceDirection.y * up->x;
    moveDirection.w = 1.0f;
    f32 side = -this->strafe;
    moveDirection.x *= side;
    moveDirection.y *= side;
    moveDirection.z *= side;
    f32 strafeSpeed = properties->GetFloat(PropStrafeSpeed);
    speed = agent->Controlled() != 0 ? TopSpeed() : strafeSpeed;
    // The rate goes unused: AskTurn turns to the face direction at once
    s32 rate;
    AngleFrom(&rate, CharacterKindOf(agent) == CharacterMecha ? MechaStrafeTurnDegrees : 0.0f, AngleDegrees);
    AskTurn(this->strafe, clock, &rate);
    AskMove(0);
    if ((!held && StrafeOver(this, elapsed)) || (CharacterKindOf(agent) == CharacterMecha && StrafeOver(this, elapsed)))
    {
        u32 state = 0;
        if (bits.state != StateIdle)
        {
            RunAgentEvent(agent, EventIdle, 0, 0, 0);
            state = StateIdle;
        }

        *next = state;
    }
}

void WalkController::Pushed(f32 stick, f32 strafe, TimeClock* clock, s32* next)
{
    CharacterAgent* agent = this->agent;
    CharacterPart* part = PartOf(agent);
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    s32 elapsed = clock->time - stateStart;
    bool over;
    if (bits.pushEases != 0 && elapsed < pushTicks)
    {
        f32 share = elapsed * g_SecondsPerClockUnit / (pushTicks * g_SecondsPerClockUnit);
        share = share * share;
        if (bits.pushVelocity != 0)
        {
            pushVelocity.x = pushFrom.x + (pushTo.x - pushFrom.x) * share;
            pushVelocity.y = pushFrom.y + (pushTo.y - pushFrom.y) * share;
            pushVelocity.w = 1.0f;
            pushVelocity.z = pushFrom.z + (pushTo.z - pushFrom.z) * share;
        }
        else
        {
            speed = pushSpeedTo * share + pushSpeedFrom * (1.0f - share);
        }

        over = false;
    }
    else
    {
        if (bits.pushVelocity != 0)
        {
            pushVelocity = pushTo;
        }
        else
        {
            speed = pushSpeedTo;
        }

        // Without a time it's kept while it's knocked back or in the air without the stick
        over = bits.pushEases != 0
               || (part->moveBits.hurt == 0
                   && (PushedStick < stick || part->flags.onGround != 0));
    }

    if (over)
    {
        *next = NextState(stick, strafe, clock);
    }

    s32 angle = 0;
    if (bits.pushVelocity != 0)
    {
        SignedAngleAboutY(&angle, &pushVelocity, RowOf(&place->matrix, 2));
        turn = 0;
        speed = __builtin_sqrtf(pushVelocity.x * pushVelocity.x + pushVelocity.y * pushVelocity.y
                                + pushVelocity.z * pushVelocity.z);
    }

    if (part->flags.onGround == 0)
    {
        airSpeed = speed;
    }

    AskAlong(this, angle);
}

u32 WalkController::Frame(f32 strafe, f32 turn, TimeClock* clock, const Vector4* stick)
{
    f32 stickSize = __builtin_sqrtf(stick->x * stick->x + stick->y * stick->y + stick->z * stick->z);
    s32 next = 0;
    u32 time = clock->time;
    moveDirection = agent->moveInput;
    faceDirection = g_DefaultBox.min;
    faceDirection.w = 1.0f;
    if (bits.next != 0)
    {
        bits.state = bits.next;
        stateStart = time;
        bits.next = 0;
    }

    switch (bits.state)
    {
    case StateNone:
        next = NextState(stickSize, strafe, clock);
        break;
    case StateIdle:
    case StateShuffling:
    {
        PropertyHolder* properties = agent->properties;
        speed = SpeedFor(this, stickSize, 0.0f);
        TaggedValue rate;
        PropertyHolder::GetTagged(&rate, properties, TaggedIdleTurn);
        AskTurn(turn, clock, &rate.raw);
        AskMove(1);
        next = NextState(stickSize, strafe, clock);
        break;
    }
    case StateWalking:
        Stride(this, stickSize, turn, clock, PropWalkSpeed, TaggedWalkTurn);
        next = NextState(stickSize, strafe, clock);
        break;
    case StateRunning:
        Stride(this, stickSize, turn, clock, PropRunSpeed, TaggedRunTurn);
        next = NextState(stickSize, strafe, clock);
        break;
    case StateStrafing:
        Strafe(stickSize, strafe, clock, &next);
        break;
    case StateInAir:
    {
        // The jump's air speed (times the stick but in a slide jump or a jump of kind 8, which go straight ahead) and air turn
        if (agent->jump != nullptr)
        {
            CharacterMoveBits moveBits = PartOf(agent)->moveBits;
            u32 straight = 0;
            if (moveBits.slideJump != 0 || moveBits.unusedJump != 0)
            {
                straight = 1;
            }

            f32 air = airSpeed;
            if (straight == 0)
            {
                air = air * stickSize;
            }

            speed = SpeedFor(this, stickSize, air);
            s32 rate = airTurn;
            AskTurn(turn, clock, &rate);
            AskMove(straight);
        }

        next = NextState(stickSize, strafe, clock);
        break;
    }
    case StateSlamming:
    {
        s32 rate;
        AngleFrom(&rate, SlamTurnRadians, AngleRadians);
        speed = SpeedFor(this, stickSize, stickSize * SlamSpeed);
        AskTurn(turn, clock, &rate);
        AskMove(0);
        next = NextState(stickSize, strafe, clock);
        break;
    }
    case StatePushed:
        Pushed(stickSize, strafe, clock, &next);
        break;
    default:
        break;
    }

    bits.strafeHeld = !(__builtin_fabsf(strafe) <= StickEpsilon);
    if (next != 0)
    {
        bits.next = next;
    }

    u32 state = bits.state;
    return state == StateWalking || state == StateRunning;
}
