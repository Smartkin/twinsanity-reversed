#include "game/agents.h"

#include "game/characters.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"

#include <cstddef>
#include <cstdint>

extern "C"
{
    // The up axis (0, 1, 0, 1), twice, as the controllers' start-up (InitCharacterControllerGlobals) sets it: the normal of the
    // crouch's checks for room to stand up and of the jump's for room to take off from a slide
    extern const Vector4 g_CrouchUp RETAIL(D_0030BB70);
    extern const Vector4 g_JumpUp RETAIL(D_0030BB20);
}

EABI_EXPORT(FUN_00143a58, &CrouchController::Slide);
EABI_EXPORT(FUN_00143d38, &CrouchController::SlideEnd);
EABI_EXPORT(FUN_00143fd0, &CrouchController::Frame);
EABI_EXPORT(FUN_0015ee48, &JumpController::QueueLaunch);
EABI_EXPORT(FUN_00149818, &JumpController::Frame);
EABI_EXPORT(FUN_0014e500, &SpinController::Sweep);
EABI_EXPORT(FUN_0014ebe0, &SpinController::Frame);

namespace
{
// The characters' sizes: most stand 1.8 high and crouch to 0.75, their radius 0.29 either way (the tall Crash stands 2 high),
// the Mecha-Bandicoot stands 9 high and crouches to 5.5, its radius 1.6, and an agent without a character 1 high, its radius 0.5
constexpr f32 StandingHeight = Rounded(1.8);
constexpr f32 CrouchHeight = 0.75f;
constexpr f32 BodyRadius = Rounded(0.29);
constexpr f32 TallCrashHeight = 2.0f;
constexpr f32 MechaHeight = 9.0f;
constexpr f32 MechaCrouchHeight = 5.5f;
constexpr f32 MechaRadius = Rounded(1.6);
constexpr f32 NoCharacterHeight = 1.0f;
constexpr f32 NoCharacterRadius = 0.5f;
// The most instances a ray of the spin finds (the solid objects, no characters, with collision on)
constexpr u16 MostSwept = 20;
// Presses and the ends of states count from 1 / EarlyDivisor of the clock's last advance early
constexpr s32 EarlyDivisor = 8;
// Seconds before circle crouches again once it stood up
constexpr f32 StandCooldown = Rounded(0.2);
// The speed scale an air attack asks for
constexpr f32 AirAttackScale = 3.0f;
// Within this of 0: none (a property's time, a speed)
constexpr f32 NoValue = Epsilon;

static_assert(offsetof(CharacterAgent, link) == 0xB0);
static_assert(offsetof(CharacterAgent, proceduralJoints) == 0xB4);
static_assert(offsetof(CharacterAgent, moveInput) == 0xE0);
static_assert(offsetof(CharacterAgent, standingOn) == 0x200);
static_assert(offsetof(InstanceContext, nodes) == 0xD4);
static_assert(offsetof(ProceduralJoints, elements) == 0xC);
static_assert(offsetof(ProceduralJoint, motion) == 0x10);

bool IsNone(f32 value)
{
    return __builtin_fabsf(value) <= NoValue;
}

// Seconds as clock units, cut down to whole ones
s32 TicksOf(f32 seconds)
{
    return static_cast<s32>(seconds * g_ClockUnitsPerSecond);
}

// The clock time a state of a length ends at, the sum taken in seconds
s32 EndOf(s32 start, s32 ticks)
{
    f32 startSeconds = static_cast<f32>(start) * g_SecondsPerClockUnit;
    f32 seconds = static_cast<f32>(ticks) * g_SecondsPerClockUnit;
    return TicksOf(startSeconds + seconds);
}

// A tagged property's word (the controllers' turn rates, 65536ths of a turn a second)
s32 TaggedProperty(PropertyHolder* properties, u32 index)
{
    TaggedValue value;
    PropertyHolder::GetTagged(&value, properties, index);
    return value.raw;
}

InstanceContext* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
}

// When a press came against a window from a start, the times an eighth of the clock's last advance early: before the start,
// within the window, after it
enum PressTiming : u32
{
    PressBefore = 0,
    PressWithinWindow = 1,
    PressAfter = 2,
};

u32 PressWithin(const TimeClock* clock, s32 pressTime, s32 start, s32 window)
{
    s32 early = static_cast<s32>(clock->advance) / EarlyDivisor;
    if (pressTime < start - early)
    {
        return PressBefore;
    }

    return pressTime < start + window - early ? PressWithinWindow : PressAfter;
}

// The rise's time to the top and the whole flight's back to the take-off height (float property 0xE given 0.95 of it in
// seconds)
void SolveFlight(JumpController* jump)
{
    constexpr f32 FlightShare = Rounded(0.95);

    f32 rise = jump->upSpeed / jump->gravity;
    PropertyHolder* properties = jump->agent->properties;
    f32 roots[2];
    s32 count = SolveQuadratic(roots, jump->gravity * -0.5f, jump->upSpeed, 0.0f);
    f32 flight = 0.0f;
    if (count == 1)
    {
        flight = roots[0];
    }
    else if (count == 2)
    {
        flight = __builtin_fmaxf(roots[1], roots[0]);
    }

    jump->stateTicks = TicksOf(rise);
    jump->flightTicks = TicksOf(flight);
    properties->SetFloat(JumpController::PropFlightSeconds, flight * FlightShare);
}

// The part on its way up: off the ground, the jump's gravity and upward speed, the push off what it stood on still to give, the
// agent's height in a jump
void TakeOff(JumpController* jump, CharacterPart* part)
{
    part->flags.onGround = 0;
    part->gravity = jump->gravity;
    part->RequestVertical(jump->upSpeed);
    jump->bits.jumped = 1;
    jump->agent->SetHeightState(HeightJumping);
    SolveFlight(jump);
}

// The link's second character's agent (none without a second: the second's own link)
CharacterAgent* SecondAgentOf(const CharacterLink* link)
{
    InstanceContext* second = ObjectOf(link->second);
    if (second == nullptr)
    {
        return nullptr;
    }

    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&second->nodes, NodeCharacter))->agent);
}

// An agent's procedural joints as retail reads them, also without an agent (the word at 0xB4 then)
ProceduralJoints* RetailJointsOf(const CharacterAgent* agent)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(agent) + offsetof(CharacterAgent, proceduralJoints);
    return *reinterpret_cast<ProceduralJoints* const*>(address);
}

// A ray hitting something pushes the character back along it by the push-back factor times what the ray had left past the hit
// (at least 0.3)
void PushBack(CharacterAgent* agent, const Vector4* way, const Vector4* end, const Vector4* hit, f32 pushBack)
{
    constexpr f32 LeastLeft = Rounded(0.3);

    f32 dx = hit->x - end->x;
    f32 dy = hit->y - end->y;
    f32 dz = hit->z - end->z;
    f32 left = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    f32 scale = pushBack * (left < LeastLeft ? LeastLeft : left);
    auto* part = static_cast<CharacterPart*>(agent->part);
    part->push.x += way->x * scale;
    part->push.y += way->y * scale;
    part->push.z += way->z * scale;
}

// A probe: its exit point, how much its contacts push and its offset on the exit point (none: the zero point)
void SetProbe(CharacterBody* body, s32 probe, s32 exitPoint, f32 weight)
{
    body->probeExitPoints[probe] = exitPoint;
    body->probeWeights[probe] = weight;
    body->probeOffsets[probe] = g_DefaultBox.min;
    body->probeOffsets[probe].w = 1.0f;
}

void SetProbe(CharacterBody* body, s32 probe, s32 exitPoint, f32 weight, f32 y, f32 z)
{
    body->probeExitPoints[probe] = exitPoint;
    body->probeWeights[probe] = weight;
    body->probeOffsets[probe] = {0.0f, y, z, 1.0f};
}

void SetSize(CharacterBody* body, f32 height, f32 radius, f32 crouchHeight, f32 crouchRadius)
{
    body->height = height;
    body->radius = radius;
    body->crouchHeight = crouchHeight;
    body->crouchRadius = crouchRadius;
}
}

CrouchController* CrouchController::Construct(CrouchController* crouch, CharacterAgent* agent)
{
    crouch->agent = agent;
    crouch->Reset();
    return crouch;
}

void CrouchController::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CrouchController::Reset()
{
    bits.value = 0;
    bits.mayJump = 1;
    if (!IsNone(agent->properties->GetFloat(PropCrouchSeconds)))
    {
        bits.canCrouch = 1;
    }

    slideFade = 1.0f;
}

void CrouchController::Stand(u32 circle, s32* next)
{
    // Running faster than this (squared, a second) slides
    constexpr f32 SlideSpeedSquared = 1.0f;

    auto* part = static_cast<CharacterPart*>(agent->part);
    if (circle == 0 || cooldown != 0.0f || agent->FitsAt(FitCrouching, 1, nullptr, nullptr) == 0)
    {
        return;
    }

    bool slide = false;
    PropertyHolder* properties = agent->properties;
    if (part->moveBits.running != 0)
    {
        Vector4 velocity;
        MovementVelocity(static_cast<MovementNode*>(GetGameNode(&agent->instance->nodes, NodeMovement)), &velocity);
        if (SlideSpeedSquared < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
        {
            f32 steer = properties->GetFloat(PropSlideSteerSeconds);
            f32 rest = properties->GetFloat(PropSlideRestSeconds);
            f32 end = properties->GetFloat(PropSlideEndSeconds);
            stateTicks = TicksOf(steer + rest - end);
            slide = stateTicks != 0;
        }
    }

    if (slide)
    {
        *next = StateSliding;
        slideSpeed = 1.0f;
        bits.slideVariantKind = 0;
        bits.mayJump = 0;
        slideFade = 1.0f;
        slideDirection = agent->moveInput;
        f32 inverse = InverseLength(&slideDirection, LengthEpsilon);
        slideDirection.x *= inverse;
        slideDirection.y *= inverse;
        slideDirection.z *= inverse;
        RunAgentEvent(agent, EventRunToKneeSlide, 0, 0, 0);
        return;
    }

    stateTicks = TicksOf(properties->GetFloat(PropCrouchSeconds));
    if (stateTicks != 0)
    {
        RunAgentEvent(agent, EventStandToCrouch, 0, 0, 0);
        *next = StateGoingDown;
    }
}

void CrouchController::Slide(f32 turn, s32 time, u32, s32* next)
{
    constexpr f32 SlowSpeed = Rounded(0.2);

    PropertyHolder* properties = agent->properties;
    s32 steerTicks = TicksOf(properties->GetFloat(PropSlideSteerSeconds));
    s32 elapsed = time - stateStart;
    bool steering = elapsed < steerTicks;
    bits.mayJump = !steering;
    s32 rate;
    f32 speed;
    if (slideSpeed < SlowSpeed)
    {
        // Too slow: it stands up. Retail bug: event 54 runs here and again when the slide's end is over (SlideEnd)
        RunAgentEvent(agent, EventKneeSlideToStand, 0, 0, 0);
        rate = 0;
        speed = 0.0f;
        stateTicks = TicksOf(properties->GetFloat(PropSlideToStandSeconds));
        *next = StateSlideEnd;
        cooldown = 0.0f;
        bits.mayJump = 1;
    }
    else
    {
        if (elapsed >= stateTicks)
        {
            stateTicks = TicksOf(properties->GetFloat(PropSlideEndSeconds));
            *next = StateSlideEnd;
        }

        bool still = IsNone(slideDirection.x) && IsNone(slideDirection.y) && IsNone(slideDirection.z);
        if (steering && !still)
        {
            // Steered: turning to the slide's direction over the seconds left of the steering
            f32 elapsedSeconds = static_cast<f32>(elapsed) * g_SecondsPerClockUnit;
            f32 secondsLeft = static_cast<f32>(steerTicks) * g_SecondsPerClockUnit - elapsedSeconds;
            ObjectPlace* place = agent->instance->place;
            RotateAndTranslate(place);
            s32 angle;
            SignedAngleAboutY(&angle, &slideDirection, RowOf(&place->matrix, 2));
            rate = static_cast<s32>(static_cast<f32>(angle) / secondsLeft);
            if (rate < 0)
            {
                rate = -rate;
            }
        }
        else
        {
            rate = TaggedProperty(properties, TaggedSlideTurn);
        }

        speed = properties->GetFloat(PropSlideSpeed);
    }

    s32 slideTurn = *MultiplyAngle(&rate, turn);
    auto* part = static_cast<CharacterPart*>(agent->part);
    slideSpeed *= part->moveShare;
    part->RequestForward(speed * slideFade);
    part->RequestSideways(0.0f);
    s32 fadedTurn = *MultiplyAngle(&slideTurn, slideFade);
    part->RequestTurn(&fadedTurn);
}

void CrouchController::SlideEnd(f32 turn, s32 time, u32 circle, s32* next)
{
    PropertyHolder* properties = agent->properties;
    s32 elapsed = time - stateStart;
    if (elapsed < stateTicks)
    {
        // Fading out over its time
        f32 elapsedSeconds = static_cast<f32>(elapsed) * g_SecondsPerClockUnit;
        f32 seconds = static_cast<f32>(stateTicks) * g_SecondsPerClockUnit;
        slideFade = 1.0f - elapsedSeconds / seconds;
        f32 speed = properties->GetFloat(PropSlideSpeed);
        s32 rate = TaggedProperty(properties, TaggedSlideTurn);
        s32 slideTurn = *MultiplyAngle(&rate, turn);
        auto* part = static_cast<CharacterPart*>(agent->part);
        slideSpeed *= part->moveShare;
        part->RequestForward(speed * slideFade);
        part->RequestSideways(0.0f);
        s32 fadedTurn = *MultiplyAngle(&slideTurn, slideFade);
        part->RequestTurn(&fadedTurn);
        return;
    }

    bits.slideVariantKind = 0;
    stateTicks = TicksOf(properties->GetFloat(PropSlideToCrouchSeconds));
    // Down into a crouch with circle held (when it has a time), or without room to stand up
    if ((circle != 0 && stateTicks != 0) || agent->FitsAt(FitStanding, 0, &g_CrouchUp, nullptr) == 0)
    {
        RunAgentEvent(agent, EventKneeSlideToCrouch, 0, 0, 0);
        *next = StateGoingDown;
    }
    else
    {
        RunAgentEvent(agent, EventKneeSlideToStand, 0, 0, 0);
        stateTicks = TicksOf(properties->GetFloat(PropSlideToStandSeconds));
        *next = StateGettingUp;
        cooldown = StandCooldown;
    }

    slideFade = 0.0f;
    auto* part = static_cast<CharacterPart*>(agent->part);
    slideSpeed *= part->moveShare;
    part->RequestForward(0.0f);
    part->RequestSideways(0.0f);
    s32 noTurn = 0;
    part->RequestTurn(&noTurn);
}

u32 CrouchController::Frame(f32 circle, f32 stick, f32 turn, TimeClock* clock)
{
    // The stick's size past which it crawls
    constexpr f32 CrawlStick = Rounded(0.6);

    s32 next = NoNextState;
    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 kind = part->bits.attackKind;
    u32 pressed = 0.0f < circle;
    s32 time = clock->time;
    u32 state = bits.state;
    if (part->moveBits.strafing != 0)
    {
        if (agent->FitsAt(FitStanding, 0, nullptr, nullptr) != 0)
        {
            next = StateStanding;
            cooldown = StandCooldown;
            bits.mayJump = 1;
        }
    }
    else if (part->moveBits.jumping != 0)
    {
        next = StateStanding;
        cooldown = StandCooldown;
    }
    else if (kind != AttackWalkInto && kind != AttackSlide && kind != AttackSlideVariant)
    {
        // Another move's attack: up when there's room, else crawling
        if (agent->FitsAt(FitStanding, 0, nullptr, nullptr) != 0)
        {
            next = StateStanding;
            cooldown = StandCooldown;
        }
        else
        {
            next = StateCrawling;
        }
    }
    else if (part->flags.onGround == 0 && state != StateSliding && state != StateSlideEnd)
    {
        if (agent->FitsAt(FitStanding, 0, nullptr, nullptr) != 0)
        {
            next = StateStanding;
            cooldown = StandCooldown;
        }
    }
    else
    {
        PropertyHolder* properties;
        switch (state)
        {
        case StateStanding:
            Stand(pressed, &next);
            break;
        case StateGoingDown:
            if (time >= EndOf(stateStart, stateTicks))
            {
                next = StateCrouched;
            }

            break;
        case StateCrouched:
            if (pressed == 0 && agent->FitsAt(FitStanding, 0, &g_CrouchUp, nullptr) != 0)
            {
                properties = agent->properties;
                RunAgentEvent(agent, EventCrouchToStand, 0, 0, 0);
                f32 seconds = properties->GetFloat(PropCrouchToStandSeconds);
                next = StateGettingUp;
                cooldown = StandCooldown;
                stateTicks = TicksOf(seconds);
            }
            else if (CrawlStick < stick && agent->FitsAt(FitCrawling, 1, &g_CrouchUp, nullptr) != 0)
            {
                RunAgentEvent(agent, EventCrouchToCrawl, 0, 0, 0);
                next = StateCrawling;
            }

            break;
        case StateCrawling:
            properties = agent->properties;
            if (pressed == 0 && agent->FitsAt(FitStanding, 0, &g_CrouchUp, nullptr) != 0)
            {
                RunAgentEvent(agent, EventCrawlToStand, 0, 0, 0);
                f32 seconds = properties->GetFloat(PropCrawlToStandSeconds);
                next = StateGettingUp;
                stateTicks = TicksOf(seconds);
            }
            else if (stick <= CrawlStick)
            {
                RunAgentEvent(agent, EventCrawlToCrouch, 0, 0, 0);
                next = StateCrouched;
            }

            break;
        case StateGettingUp:
            if (pressed != 0 && cooldown == 0.0f)
            {
                properties = agent->properties;
                RunAgentEvent(agent, EventStandToCrouch, 0, 0, 0);
                f32 seconds = properties->GetFloat(PropCrouchSeconds);
                next = StateGoingDown;
                stateTicks = TicksOf(seconds);
            }
            else if (time >= EndOf(stateStart, stateTicks))
            {
                // Standing when there's room, else crawling
                if (agent->FitsAt(FitStanding, 1, nullptr, nullptr) != 0)
                {
                    next = StateStanding;
                    cooldown = StandCooldown;
                }
                else
                {
                    next = StateCrawling;
                }
            }

            break;
        case StateSliding:
            Slide(turn, time, pressed, &next);
            break;
        case StateSlideEnd:
            SlideEnd(turn, time, pressed, &next);
            break;
        }
    }

    // Ducking (the part's bit from the last frame): crawling while the stick is pushed, else still
    if (part->moveBits.crouching != 0)
    {
        if (CrawlStick < stick)
        {
            PropertyHolder* properties = agent->properties;
            part->RequestForward(properties->GetFloat(PropCrawlSpeed));
            part->RequestSideways(0.0f);
            s32 rate = TaggedProperty(properties, TaggedCrawlTurn);
            s32 crawlTurn = *MultiplyAngle(&rate, turn);
            part->RequestTurn(&crawlTurn);
        }
        else
        {
            part->RequestForward(0.0f);
            part->RequestSideways(0.0f);
            s32 noTurn = 0;
            part->RequestTurn(&noTurn);
        }
    }

    cooldown -= static_cast<s32>(clock->advance) * g_SecondsPerClockUnit;
    if (cooldown < 0.0f)
    {
        cooldown = 0.0f;
    }

    if (next != NoNextState)
    {
        stateStart = time;
        bits.state = next;
    }

    return bits.state != StateStanding;
}

JumpController* JumpController::Construct(JumpController* jump, CharacterAgent* agent)
{
    jump->agent = agent;
    jump->Reset();
    return jump;
}

void JumpController::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void JumpController::Reset()
{
    PropertyHolder* properties = agent->properties;
    crossTime = 0;
    circleTime = 0;
    stateStart = 0;
    stateTicks = 0;
    gravity = properties->GetFloat(PropGravity);
    upSpeed = 0.0f;
    airSpeed = properties->GetFloat(PropAirSpeed);
    airTurn = TaggedProperty(properties, TaggedJumpTurn);
    // Falling, no next state
    bits.value = 0;
    bits.state = StateFalling;
    bits.next = StateNone;
    static_cast<CharacterPart*>(agent->part)->gravity = gravity;
}

void JumpController::QueueLaunch(f32 launchSpeed, f32 launchGravity, u32 event)
{
    gravity = launchGravity;
    upSpeed = launchSpeed;
    bits.launchQueued = 1;
    bits.launchEvent = event;
}

u32 JumpController::PressPhase(const TimeClock* clock, s32 pressTime, s32 lastTicks)
{
    return PressWithin(clock, pressTime, stateStart, stateTicks - lastTicks);
}

f32 JumpController::KindGravity(PropertyHolder* properties, u32 kind, u32 falling)
{
    if (falling != 0)
    {
        switch (kind)
        {
        case KindJump:
        case KindTied:
            return properties->GetFloat(PropJumpGravityDown);
        case KindDouble:
            return properties->GetFloat(PropDoubleGravityDown);
        case KindSlide:
        case KindUnused:
            return properties->GetFloat(PropSlideGravityDown);
        case KindLaunch:
            return properties->GetFloat(PropGravity);
        case KindFlyingKick:
            return properties->GetFloat(PropFlyingKickGravity);
        case KindRadialBlast:
            return properties->GetFloat(PropBlastGravity);
        default:
            return 0.0f;
        }
    }

    switch (kind)
    {
    case KindJump:
    case KindTied:
        return properties->GetFloat(PropJumpGravityUp);
    case KindDouble:
        return properties->GetFloat(PropDoubleGravityUp);
    case KindSlide:
    case KindUnused:
        return properties->GetFloat(PropSlideGravityUp);
    case KindLaunch:
        // The launch's own
        return gravity;
    case KindRadialBlast:
        return properties->GetFloat(PropBlastGravity);
    default:
        return 0.0f;
    }
}

u32 JumpController::KindEvent(u32 kind, u32 maxed)
{
    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 attack = part->bits.attackKind;
    if (attack == AttackSlide || attack == AttackSlideVariant)
    {
        return EventKneeSlideJump;
    }

    if (attack == AttackSpin || attack == AttackSpinVariant)
    {
        return EventNone;
    }

    CharacterMoveBits moveBits = part->moveBits;
    if (moveBits.running != 0)
    {
        if (moveBits.linkedFirst == 0)
        {
            return EventRunningJump;
        }

        RunAgentEvent(agent->link->Second(), EventLinkedRunningJump, 0, 0, 0);
        return EventLinkedRunningJump;
    }

    if (kind == KindDouble)
    {
        return maxed != 0 ? EventMaxedDoubleJump : EventDoubleJump;
    }

    if (kind == KindRadialBlast)
    {
        return EventRadialBlast;
    }

    if (moveBits.linkedFirst == 0)
    {
        return EventStandingJump;
    }

    RunAgentEvent(agent->link->Second(), EventLinkedStandingJump, 0, 0, 0);
    return EventLinkedStandingJump;
}

s32 JumpController::Fall(u32 kind, u32 tellFall)
{
    auto* part = static_cast<CharacterPart*>(agent->part);
    gravity = KindGravity(agent->properties, kind, 1);
    upSpeed = 0.0f;
    part->gravity = gravity;
    // StartFalling, with the agent in $a2 as retail leaves it
    CallVirtual<void>(agent, agent->vtable, CreatureAgent::StartFallingSlot, tellFall, agent);
    // What's left of the flight
    f32 flight = static_cast<f32>(flightTicks) * g_SecondsPerClockUnit;
    f32 rise = static_cast<f32>(stateTicks) * g_SecondsPerClockUnit;
    stateTicks = TicksOf(flight - rise);
    switch (kind)
    {
    case KindJump:
        return StateFalling;
    case KindTied:
        return StateFallingTied;
    case KindDouble:
        return StateFallingDouble;
    case KindSlide:
        return StateFallingSlide;
    case KindLaunch:
        return StateFallingLaunched;
    case KindUnused:
        return StateFallingUnused;
    case KindFlyingKick:
        return StateFallingKick;
    case KindRadialBlast:
        return StateBlastFalling;
    default:
        return NoNextState;
    }
}

s32 JumpController::Start(TimeClock* clock, u32 kind, u32 maxed, u32 midFall)
{
    // The tied jump's speed and share of the turn, and the share of its speed a double jump loses at the end of the fall
    constexpr f32 TiedSpeed = 10.0f;
    constexpr f32 TiedTurnShare = Rounded(0.2);
    constexpr f32 LateDoubleLoss = 0.5f;

    PropertyHolder* properties = agent->properties;
    u32 event = KindEvent(kind, maxed);
    s32 state;
    f32 speed;
    f32 air;
    s32 turn;
    bool launch = false;
    switch (kind)
    {
    case KindJump:
        turn = TaggedProperty(properties, TaggedJumpTurn);
        state = StateRising;
        speed = properties->GetFloat(PropJumpSpeed);
        air = properties->GetFloat(PropAirSpeed);
        break;
    case KindTied:
        speed = TiedSpeed;
        turn = TaggedProperty(properties, TaggedJumpTurn);
        state = StateRisingTied;
        turn = *MultiplyAngle(&turn, TiedTurnShare);
        air = properties->GetFloat(PropAirSpeed);
        break;
    case KindDouble:
        turn = TaggedProperty(properties, TaggedDoubleTurn);
        speed = properties->GetFloat(PropDoubleSpeed);
        air = properties->GetFloat(PropDoubleAirSpeed);
        if (midFall != 0)
        {
            // Weaker the later in the fall: half as strong once its time is over
            s32 elapsed = clock->time - stateStart;
            f32 share = 1.0f;
            if (elapsed < stateTicks)
            {
                f32 elapsedSeconds = static_cast<f32>(elapsed) * g_SecondsPerClockUnit;
                f32 seconds = static_cast<f32>(stateTicks) * g_SecondsPerClockUnit;
                share = elapsedSeconds / seconds;
            }

            speed *= 1.0f - share * LateDoubleLoss;
        }

        state = StateRisingDouble;
        break;
    case KindSlide:
        speed = properties->GetFloat(PropSlideSpeed);
        state = StateRisingSlide;
        air = properties->GetFloat(PropSlideAirSpeed);
        turn = TaggedProperty(properties, TaggedSlideTurn);
        break;
    case KindLaunch:
        turn = TaggedProperty(properties, TaggedJumpTurn);
        if (agent->Linked() != 0)
        {
            turn = *MultiplyAngle(&turn, TiedTurnShare);
        }

        speed = upSpeed;
        air = properties->GetFloat(PropAirSpeed);
        state = StateRisingLaunched;
        launch = true;
        break;
    case KindRadialBlast:
        turn = 0;
        air = 0.0f;
        speed = properties->GetFloat(PropBlastSpeed);
        state = StateBlastRising;
        break;
    default:
        return NoNextState;
    }

    if (IsNone(speed))
    {
        // No jump; a launch without speed falls at once
        if (midFall != 0 || !launch)
        {
            return NoNextState;
        }

        return Fall(kind, 0);
    }

    if (event == EventNone)
    {
        bits.mayDoubleJump = 0;
        bits.mayAttack = 0;
    }
    else
    {
        bool mayDouble = kind == KindJump || kind == KindLaunch;
        bool mayAttack = mayDouble || kind == KindDouble || kind == KindSlide;
        bits.mayDoubleJump = mayDouble;
        bits.mayAttack = mayAttack;
    }

    upSpeed = speed;
    airTurn = turn;
    airSpeed = air;
    gravity = KindGravity(properties, kind, 0);
    auto* part = static_cast<CharacterPart*>(agent->part);
    if (event != EventNone)
    {
        RunAgentEvent(agent, event, 0, 0, 0);
    }

    TakeOff(this, part);
    return state;
}

s32 JumpController::AirAttack(u32 kind)
{
    // What the radial blast costs of the gun's count
    constexpr u32 BlastCost = 5;

    bits.mayDoubleJump = 0;
    bits.mayAttack = 0;
    PropertyHolder* properties = agent->properties;
    if (kind == KindSlide)
    {
        return NoNextState;
    }

    // From a plain jump: Cortex's radial blast (a character with a gun and a blast) or Crash's flying kick (with a kick)
    Gun* gun = agent->gun;
    stateTicks = TicksOf(properties->GetFloat(PropBlastHangSeconds));
    if (kind == KindJump && gun != nullptr && stateTicks != 0)
    {
        if (gun->TakeAmmo(BlastCost) != 0)
        {
            RunAgentEvent(agent, EventRadialBlastHang, 0, 0, 0);
            return StateBlastHang;
        }

        RunAgentEvent(agent, EventFailedRadialBlast, 0, 0, 0);
        return NoNextState;
    }

    stateTicks = TicksOf(properties->GetFloat(PropFlyingKickSeconds));
    if (kind == KindJump && stateTicks != 0)
    {
        RunAgentEvent(agent, EventFlyingKick, 0, 0, 0);
        return StateFlyingKick;
    }

    stateTicks = TicksOf(properties->GetFloat(PropKneeDropHangSeconds));
    if (stateTicks == 0)
    {
        return NoNextState;
    }

    if (agent->FitsAt(FitKneeDrop, 1, nullptr, nullptr) != 0)
    {
        RunAgentEvent(agent, EventKneeDropHang, 0, 0, 0);
        return StateKneeDropHang;
    }

    bits.kneeDropWaits = 1;
    return NoNextState;
}

void JumpController::Grounded(TimeClock* clock, u32 mayJump, u32 cross, s32* next)
{
    // A jump is still allowed this long after leaving the ground
    constexpr f32 LateJumpSeconds = Rounded(0.11);

    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 kind = part->bits.attackKind;
    bool sliding = kind == AttackSlide || kind == AttackSlideVariant;
    bool canJump;
    if (part->flags.onGround != 0)
    {
        leftGroundTime = 0;
        if (agent->Linked() != 0)
        {
            // Tied: not while the link slams
            canJump = agent->link->bits.state != CharacterLink::StateSlamming;
        }
        else
        {
            // Not off another playable character
            InstanceContext* standing = ObjectOf(agent->standingOn);
            canJump = standing == nullptr || CharacterAgentOf(standing) == nullptr;
        }
    }
    else
    {
        s32 time = clock->time;
        if (leftGroundTime == 0)
        {
            leftGroundTime = time;
        }

        canJump = time - leftGroundTime < TicksOf(LateJumpSeconds);
    }

    if (mayJump == 0 || !canJump || cross == 0 || bits.crossHeld != 0)
    {
        return;
    }

    u32 fits = agent->FitsAt(FitSlideJump, 0, &g_JumpUp, nullptr);
    if (sliding && fits != 0)
    {
        *next = Start(clock, KindSlide, 0, 0);
        return;
    }

    // No jump while ducking without room
    if (part->moveBits.crouching != 0 && fits == 0)
    {
        return;
    }

    *next = Start(clock, agent->Linked() != 0 ? KindTied : KindJump, 0, 0);
}

void JumpController::Rise(TimeClock* clock, u32 kind, s32* next)
{
    // Cross pressed this close to the top makes the double jump the maxed one
    constexpr f32 MaxedSeconds = Rounded(0.15);

    PropertyHolder* properties = agent->properties;
    auto* part = static_cast<CharacterPart*>(agent->part);
    f32 doubleSpeed = properties->GetFloat(PropDoubleSpeed);
    s32 start = stateStart;
    u32 doublePhase = PressBefore;
    if (bits.mayDoubleJump != 0 && !IsNone(doubleSpeed))
    {
        doublePhase = PressPhase(clock, crossTime, TicksOf(MaxedSeconds));
    }

    u32 attackPhase = PressBefore;
    if (bits.mayAttack != 0)
    {
        s32 pressed = circleTime;
        s32 window = TicksOf(properties->GetFloat(PropAttackWindowUp));
        attackPhase = PressPhase(clock, pressed, window);
    }

    s32 end = start + stateTicks;
    if (attackPhase == PressBefore)
    {
        // Retail bug: the knee drop's wait for room is cleared here, before it's read below, so the knee drop never waits
        bits.kneeDropWaits = 0;
    }

    if (doublePhase != PressBefore)
    {
        *next = Start(clock, KindDouble, doublePhase == PressAfter, 0);
        return;
    }

    if (attackPhase != PressBefore || bits.kneeDropWaits != 0)
    {
        *next = AirAttack(kind);
        return;
    }

    // Falling once at the top
    if (TimeReached(clock, clock->time, end, EarlyDivisor) != 0)
    {
        *next = Fall(kind, 0);
        return;
    }

    part->gravity = gravity;
}

void JumpController::Hang(TimeClock* clock, u32 kind, u32, s32* next)
{
    PropertyHolder* properties = agent->properties;
    auto* part = static_cast<CharacterPart*>(agent->part);
    WalkController* walk = agent->walk;
    gravity = 0.0f;
    upSpeed = 0.0f;
    f32 push = 0.0f;
    if (static_cast<s32>(clock->time) - stateStart >= stateTicks)
    {
        if (kind == KindKneeDrop)
        {
            upSpeed = properties->GetFloat(PropKneeDropSpeed);
            gravity = properties->GetFloat(PropKneeDropGravity);
            RunAgentEvent(agent, EventKneeDrop, 0, 0, 0);
            part->gravity = gravity;
            *next = StateKneeDrop;
        }
        else if (kind == KindFlyingKick)
        {
            RunAgentEvent(agent, EventFlyingKickFall, 0, 0, 0);
            *next = Fall(KindFlyingKick, 0);
        }
        else if (kind == KindRadialBlast)
        {
            *next = Start(clock, KindRadialBlast, 0, 0);
        }
    }
    else if (kind == KindFlyingKick)
    {
        push = properties->GetFloat(PropFlyingKickSpeed);
    }

    // The walk isn't checked (every character with a jump has one)
    walk->PushAtSpeed(push, push, 0);
    part->gravity = gravity;
    part->RequestVertical(upSpeed);
    part->RequestForward(0.0f);
    part->RequestSideways(0.0f);
    part->RequestScale(AirAttackScale);
}

void JumpController::FallFrame(TimeClock* clock, u32 kind, s32* next)
{
    // Cross pressed this soon after the top starts a weaker double jump
    constexpr f32 LateDoubleSeconds = Rounded(0.4);

    auto* part = static_cast<CharacterPart*>(agent->part);
    if (part->flags.onGround != 0)
    {
        *next = StateGrounded;
        return;
    }

    PropertyHolder* properties = agent->properties;
    u32 doublePhase = PressBefore;
    if (bits.mayDoubleJump != 0)
    {
        doublePhase = PressWithin(clock, crossTime, stateStart, TicksOf(LateDoubleSeconds));
    }

    u32 attackPhase = PressBefore;
    if (bits.mayAttack != 0)
    {
        s32 pressed = circleTime;
        s32 window = TicksOf(properties->GetFloat(PropAttackWindowDown));
        attackPhase = PressWithin(clock, pressed, stateStart, window);
    }

    if (doublePhase == PressWithinWindow)
    {
        *next = Start(clock, KindDouble, 0, 1);
    }
    else if (attackPhase == PressWithinWindow)
    {
        *next = AirAttack(kind);
    }
    else
    {
        // Retail bug: the kind's falling gravity Fall gave lasts one frame, property 1 replaces it from here
        gravity = properties->GetFloat(PropGravity);
    }

    part->gravity = gravity;
}

void JumpController::Frame(f32 cross, f32 circle, TimeClock* clock, u32 mayJump)
{
    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 crossDown = 0.0f < cross;
    u32 circleDown = 0.0f < circle;
    s32 next = NoNextState;
    s32 time = clock->time;
    if (bits.crossHeld == 0 && crossDown != 0)
    {
        crossTime = time;
    }

    if (bits.circleHeld == 0 && circleDown != 0)
    {
        circleTime = time;
    }

    if (bits.next != StateNone)
    {
        bits.state = bits.next;
        bits.next = StateNone;
        stateStart = time;
    }

    // No double jump nor air attack while spinning or tied
    u32 kind = part->bits.attackKind;
    if (kind == AttackSpin || kind == AttackSpinVariant || agent->Linked() != 0)
    {
        bits.mayDoubleJump = 0;
        bits.mayAttack = 0;
    }

    if (bits.launchQueued != 0)
    {
        if (0.0f < upSpeed)
        {
            bits.mayDoubleJump = agent->Linked() == 0;
            bits.mayAttack = 0;
            u32 event = bits.launchEvent;
            if (event != EventNone)
            {
                RunAgentEvent(agent, event, 0, 0, 0);
            }

            TakeOff(this, part);
            next = StateRisingLaunched;
        }
        else
        {
            next = Fall(KindLaunch, 1);
        }

        bits.launchQueued = 0;
    }
    else if (part->moveBits.clawing != 0)
    {
        // Nina's claw keeps it on the ground
        next = StateGrounded;
    }
    else
    {
        switch (bits.state)
        {
        case StateGrounded:
            Grounded(clock, mayJump, crossDown, &next);
            break;
        case StateRising:
            Rise(clock, KindJump, &next);
            break;
        case StateRisingTied:
            Rise(clock, KindTied, &next);
            break;
        case StateRisingDouble:
            Rise(clock, KindDouble, &next);
            break;
        case StateRisingSlide:
            Rise(clock, KindSlide, &next);
            break;
        case StateRisingLaunched:
            Rise(clock, KindLaunch, &next);
            break;
        case StateKneeDropHang:
            Hang(clock, KindKneeDrop, circleDown, &next);
            break;
        case StateKneeDrop:
        {
            auto* dropping = static_cast<CharacterPart*>(agent->part);
            if (dropping->flags.onGround != 0)
            {
                PropertyHolder* properties = agent->properties;
                RunAgentEvent(agent, EventKneeDropLand, reinterpret_cast<u32>(agent->instance), 0, 0);
                gravity = properties->GetFloat(PropGravity);
                dropping->gravity = gravity;
                next = StateGrounded;
            }

            dropping->RequestForward(0.0f);
            dropping->RequestSideways(0.0f);
            dropping->RequestScale(AirAttackScale);
            break;
        }
        case StateRisingUnused:
            Rise(clock, KindUnused, &next);
            break;
        case StateFallingUnused:
            FallFrame(clock, KindUnused, &next);
            break;
        case StateFlyingKick:
            Hang(clock, KindFlyingKick, circleDown, &next);
            break;
        case StateFallingKick:
            FallFrame(clock, KindFlyingKick, &next);
            break;
        case StateBlastHang:
            Hang(clock, KindRadialBlast, circleDown, &next);
            break;
        case StateBlastRising:
            Rise(clock, KindRadialBlast, &next);
            break;
        case StateBlastFalling:
            FallFrame(clock, KindRadialBlast, &next);
            break;
        case StateFallingTied:
            FallFrame(clock, KindTied, &next);
            break;
        case StateFalling:
            FallFrame(clock, KindJump, &next);
            break;
        case StateFallingDouble:
            FallFrame(clock, KindDouble, &next);
            break;
        case StateFallingSlide:
            FallFrame(clock, KindSlide, &next);
            break;
        case StateFallingLaunched:
            FallFrame(clock, KindLaunch, &next);
            break;
        }
    }

    bits.crossHeld = crossDown != 0;
    bits.circleHeld = circleDown != 0;
    if (next != NoNextState)
    {
        bits.next = next;
    }
}

SpinController* SpinController::Construct(SpinController* spin, CharacterAgent* agent)
{
    spin->agent = agent;
    spin->Reset();
    return spin;
}

void SpinController::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SpinController::Reset()
{
    sweepAngle = 0;
    bits.value = 0;
    bits.next = StateNoNext;
}

void SpinController::Sweep(f32 reach, f32 pushBack)
{
    // The rays (four, a quarter turn (radians) apart), their turn a frame and a full turn (65536ths), their height above the
    // character's position
    constexpr s32 SweepRays = 4;
    constexpr s32 SweepStep = 0x21D1;
    constexpr f32 SweepHeight = 0x1.ee41b4p-1f;

    sweepAngle += SweepStep;
    if (FullTurnAngle < sweepAngle)
    {
        sweepAngle -= FullTurnAngle;
    }

    for (s32 ray = 0; ray < SweepRays; ray++)
    {
        s32 angle = sweepAngle;
        s32 rayAngle = *AddRadiansToAngle(&angle, static_cast<f32>(ray) * HalfPi);
        f32 cosine;
        f32 sine;
        CosSin16(&rayAngle, &cosine, &sine);
        Vector4 way = {cosine * reach, 0.0f, sine * reach, 1.0f};
        Vector4 start;
        agent->Position(&start);
        start.y += SweepHeight;
        Vector4 end = {start.x + way.x, start.y + way.y, start.z + way.z, 1.0f};
        Vector4 hit;
        if (GetCollisionCheck(agent->instance->chunk, &start, &end, SurfaceFlags::SolidToPlayerProbes, nullptr, &hit, nullptr)
            != 0)
        {
            PushBack(agent, &way, &end, &hit, pushBack);
        }

        void* results[MostSwept];
        InstanceQuery query;
        query.results = results;
        query.count = 0;
        query.most = MostSwept;
        query.distance = NoHitDistance;
        query.bits.value = InstanceQueryBits::AllWanted;
        query.wantedFlags = ReferencedObjectFlags::CollisionActive;
        query.unwantedFlags = ReferencedObjectFlags::Asleep;
        query.skipped[0] = nullptr;
        query.skipped[1] = nullptr;
        query.instance = nullptr;
        if (SegmentHitsInstances(agent->instance->chunk, &start, &end, &query, SolidObjectNodeKinds, nullptr, &hit, 0) == 0)
        {
            continue;
        }

        // Touched (without a normal), and pushed back by the solid ones
        auto* other = static_cast<InstanceContext*>(query.instance);
        agent->TouchedNothing(other);
        AgentNode* node = AgentNodeOf(other);
        if (node == nullptr || node->agent->properties->state.solidToSpin != 0)
        {
            PushBack(agent, &way, &end, &hit, pushBack);
        }
    }
}

u32 SpinController::Start()
{
    CharacterLink* link = agent->link;
    CharacterMoveBits moveBits = static_cast<CharacterPart*>(agent->part)->moveBits;
    // Not while ducking nor tied in a jump
    if (moveBits.crouching != 0 || (link != nullptr && moveBits.jumping != 0))
    {
        return StateNone;
    }

    PropertyHolder* properties = agent->properties;
    RunAgentEvent(agent, EventSpin, 0, 0, 0);
    stateTicks = TicksOf(properties->GetFloat(PropSpinSeconds));
    if (link == nullptr)
    {
        return StateSpinning;
    }

    // Retail bug: the second's own link has no second and runs its event on a null agent (the second never spins in play)
    RunAgentEvent(SecondAgentOf(link), EventSpin, 0, 0, 0);
    return StateSpinningTied;
}

u32 SpinController::End(u32)
{
    u32 state = bits.state;
    if (state != StateSpinningTied && state != StateSpinning)
    {
        return StateNone;
    }

    CharacterLink* link = agent->link;
    PropertyHolder* properties = agent->properties;
    if (link != nullptr)
    {
        // The second recovers too, its procedural joints put back at rest and its gait cleared. Retail bug: on the second's own
        // link the event runs on a null agent and the joints are read at address 0xB4 (the second never spins in play)
        CharacterAgent* second = SecondAgentOf(link);
        ProceduralJoints* joints = RetailJointsOf(second);
        RunAgentEvent(second, EventSpinRecovery, 0, 0, 0);
        for (s32 i = 0; i < joints->count; i++)
        {
            joints->elements[i].Settle(joints->agent);
        }

        CharacterAgent* tied = link->Second();
        CharacterLink* tiedLink = tied != nullptr ? tied->link : link;
        tiedLink->bits.gait = CharacterLink::GaitNone;
    }

    stateTicks = TicksOf(properties->GetFloat(PropRecoverySeconds));
    if (stateTicks == 0)
    {
        return StateNone;
    }

    RunAgentEvent(agent, EventSpinRecovery, 0, 0, 0);
    return StateRecovering;
}

void SpinController::Frame(f32 button, f32, f32 stick, TimeClock* clock)
{
    // The sweep's reach and push-back, tied and alone
    constexpr f32 TiedReach = Rounded(1.4);
    constexpr f32 TiedPushBack = -Rounded(0.8);
    constexpr f32 Reach = 1.0f;
    constexpr f32 PushBackFactor = -Rounded(1.3);

    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 kind = part->bits.attackKind;
    // Walking into things and its own attack kinds keep it going
    bool ownKind = kind == AttackWalkInto || kind == AttackSpin || kind == AttackSpinVariant;
    u32 pressed = 0.0f < button;
    s32 next = NoNextState;
    CharacterLink* link = agent->link;
    bool slamming = link != nullptr && link->bits.state == CharacterLink::StateSlamming;
    if (bits.next != StateNoNext)
    {
        bits.state = bits.next;
        stateStart = clock->time;
        bits.next = StateNoNext;
    }

    if (part->moveBits.slideJump != 0 || part->moveBits.clawing != 0 || slamming || !ownKind)
    {
        next = End(0);
    }
    else
    {
        switch (bits.state)
        {
        case StateNone:
            if (pressed != 0 && bits.buttonHeld == 0)
            {
                next = Start();
            }

            break;
        case StateSpinningTied:
        case StateSpinning:
        {
            bool tied = bits.state == StateSpinningTied;
            auto* spinning = static_cast<CharacterPart*>(agent->part);
            if (static_cast<s32>(clock->time) - stateStart >= stateTicks)
            {
                next = End(0);
            }

            if (0.0f < stick)
            {
                PropertyHolder* properties = agent->properties;
                spinning->RequestForward(properties->GetFloat(PropSpinSpeed));
                s32 rate = TaggedProperty(properties, TaggedSpinTurn);
                spinning->RequestTurn(&rate);
            }
            else
            {
                spinning->RequestForward(0.0f);
                s32 noTurn = 0;
                spinning->RequestTurn(&noTurn);
            }

            if (tied)
            {
                Sweep(TiedReach, TiedPushBack);
            }
            else
            {
                Sweep(Reach, PushBackFactor);
            }

            break;
        }
        case StateRecovering:
            if (static_cast<s32>(clock->time) - stateStart >= stateTicks)
            {
                next = StateNone;
            }

            break;
        }
    }

    bits.buttonHeld = pressed != 0;
    if (next != NoNextState)
    {
        bits.next = next;
    }
}

void ProceduralJoint::Settle(CharacterAgent* agent)
{
    ObjectPlace* place = agent->instance->place;
    RotateAndTranslate(place);
    Vector4 xAxis = {1.0f, 0.0f, 0.0f, 1.0f};
    Vector4 origin = {0.0f, 0.0f, 0.0f, 1.0f};
    VuRotateVector(&place->matrix, &xAxis, &motion);
    VuRotateVector(&place->matrix, &origin, &slowMotion);
}

CharacterBody* CharacterBody::Construct(CharacterBody* body, CharacterAgent* agent)
{
    body->agent = agent;
    HullConstruct(&body->standingHull);
    HullConstruct(&body->crouchHull);
    body->SetSizes();
    return body;
}

void CharacterBody::Destroy(u32 destroyFlags)
{
    HullDestroy(&crouchHull, DestroyOnly);
    HullDestroy(&standingHull, DestroyOnly);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CharacterBody::SetCrashSizes()
{
    SetSize(this, StandingHeight, BodyRadius, CrouchHeight, BodyRadius);
    probeCount = 6;
    SetProbe(this, 0, 7, 1.0f, 0.0f, Rounded(0.2));
    SetProbe(this, 1, 6, 1.0f, 0.0f, Rounded(0.2));
    SetProbe(this, 2, 0, 4.0f);
    SetProbe(this, 3, 1, Rounded(1.6), 0.0f, Rounded(0.13));
    SetProbe(this, 4, 4, 3.0f);
    SetProbe(this, 5, 3, 3.0f);
}

void CharacterBody::SetMechaSizes()
{
    SetSize(this, MechaHeight, MechaRadius, MechaCrouchHeight, MechaRadius);
    probeCount = 7;
    SetProbe(this, 0, 7, 2.0f, 0.0f, 3.0f);
    SetProbe(this, 1, 6, 2.0f, 0.0f, -3.0f);
    SetProbe(this, 2, 13, 2.0f);
    SetProbe(this, 3, 14, 4.0f, 0.0f, -2.0f);
    SetProbe(this, 4, 16, 4.0f, 0.0f, -2.0f);
    SetProbe(this, 5, 9, 4.0f, -1.0f, Rounded(3.2));
    SetProbe(this, 6, 10, 4.0f, -1.0f, Rounded(3.2));
}

void CharacterBody::SetSizes()
{
    switch (agent->properties->GetInt(CharacterKindProperty))
    {
    case CharacterCrash:
        SetCrashSizes();
        break;
    case CharacterCortex:
        SetSize(this, StandingHeight, BodyRadius, CrouchHeight, BodyRadius);
        probeCount = 3;
        SetProbe(this, 0, 4, 4.0f);
        SetProbe(this, 1, 3, 4.0f);
        SetProbe(this, 2, 1, 4.0f);
        break;
    case CharacterTallCrash:
        SetSize(this, TallCrashHeight, BodyRadius, CrouchHeight, BodyRadius);
        probeCount = 0;
        break;
    case CharacterNina:
        SetSize(this, StandingHeight, BodyRadius, CrouchHeight, BodyRadius);
        probeCount = 4;
        SetProbe(this, 0, 4, Rounded(0.7));
        SetProbe(this, 1, 3, Rounded(0.7));
        SetProbe(this, 2, 2, 1.0f);
        SetProbe(this, 3, 1, 1.0f, Rounded(0.35), 0.0f);
        break;
    case CharacterNone:
        SetSize(this, NoCharacterHeight, NoCharacterRadius, NoCharacterHeight, NoCharacterRadius);
        probeCount = 0;
        break;
    case CharacterMecha:
        SetMechaSizes();
        break;
    }

    // Boxes from the ground up, as wide as long
    Vector4 min = {-radius, 0.0f, -radius, 1.0f};
    Vector4 max = {radius, height, radius, 1.0f};
    BuildBoxHull(&standingHull, &min, &max);
    min = {-crouchRadius, 0.0f, -crouchRadius, 1.0f};
    max = {crouchRadius, crouchHeight, crouchRadius, 1.0f};
    BuildBoxHull(&crouchHull, &min, &max);
}
