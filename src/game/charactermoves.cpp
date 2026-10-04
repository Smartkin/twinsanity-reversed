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
// The node kinds these functions use: the movement node, the playable characters' agent node
constexpr u32 MovementNodeKind = 0;
constexpr u32 CharacterNodeKind = 0xC;
// The agents' vtable function a fall starts with
constexpr u32 AgentStartFallingSlot = 24;
// The part's attack kinds (its low byte): walking into, spinning, sliding, each with its second kind
constexpr u32 AttackWalkInto = 3;
constexpr u32 AttackSpin = 6;
constexpr u32 AttackSpin2 = 10;
constexpr u32 AttackSlide = 8;
constexpr u32 AttackSlide2 = 12;
// The part's move bits agentparts.h doesn't name: the slide jump and the strafe held
constexpr u32 MoveSlideJump = 0x8;
constexpr u32 MoveStrafing = 0x1000;
// FitsAt's kinds: standing (the instances in the way told when there's a normal), crouching, crawling, taking off from a
// slide and the knee drop
constexpr u32 FitStanding = 0;
constexpr u32 FitCrouching = 3;
constexpr u32 FitCrawling = 4;
constexpr u32 FitSlideJump = 6;
constexpr u32 FitKneeDrop = 7;
// The characters (the first int property): Crash, Cortex, a Crash 2 units high without probes, Nina, none and Mecha-Bandicoot
constexpr u32 CharacterProperty = 0;
constexpr s32 Crash = 0;
constexpr s32 Cortex = 1;
constexpr s32 TallCrash = 2;
constexpr s32 Nina = 3;
constexpr s32 NoCharacter = 4;
constexpr s32 MechaBandicoot = 5;
// The instances' state flag that makes the spin's rays bounce off them (and the ones without an agent)
constexpr u32 SolidToSpin = 0x1000;
// The surfaces of the collision the spin's rays bounce off (and the instances' flag they take), the instances' node kinds
// they touch (no characters) and the most of them a ray finds
constexpr u32 SolidSurfaces = 0x10;
constexpr u32 SweptKinds = 0x5A010;
constexpr u16 MostSwept = 20;

// No next state
constexpr s32 NoState = -1;
// Within this of 0: none (a property's time, a speed)
constexpr f32 NoValue = Rounded(5e-05);
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 NoHitDistance = Rounded(1e30);

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

// When a press came against a window from a start, the times an eighth of the clock's last advance early: 0 before the start, 1
// within the window, 2 after it
u32 PressWithin(const TimeClock* clock, s32 pressTime, s32 start, s32 window)
{
    s32 early = static_cast<s32>(clock->advance) / 8;
    if (pressTime < start - early)
    {
        return 0;
    }

    return pressTime < start + window - early ? 1 : 2;
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
    part->flags &= ~CreaturePart::FlagOnGround;
    part->Gravity() = jump->gravity;
    part->RequestVertical(jump->upSpeed);
    jump->bits |= JumpController::Jumped;
    jump->agent->SetHeightState(1);
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

    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&second->nodes, CharacterNodeKind))->agent);
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
    bits = BitMayJump;
    if (!IsNone(agent->properties->GetFloat(PropCrouchSeconds)))
    {
        bits |= BitCanCrouch;
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
    if ((part->moveBits & CharacterPart::Running) != 0)
    {
        Vector4 velocity;
        MovementVelocity(static_cast<MovementNode*>(GetGameNode(&agent->instance->nodes, MovementNodeKind)), &velocity);
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
        bits &= ~(BitSlideKind12 | BitMayJump);
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
    bits = (bits & ~BitMayJump) | (steering ? 0 : BitMayJump);
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
        bits |= BitMayJump;
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
    constexpr f32 StandCooldown = Rounded(0.2);

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

    bits &= ~BitSlideKind12;
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
    // Seconds before circle crouches again once it stood up, and the stick's size past which it crawls
    constexpr f32 StandCooldown = Rounded(0.2);
    constexpr f32 CrawlStick = Rounded(0.6);

    s32 next = NoState;
    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 kind = part->bits & CharacterPart::AttackKindMask;
    u32 pressed = 0.0f < circle;
    s32 time = clock->time;
    u32 state = bits & StateMask;
    if ((part->moveBits & MoveStrafing) != 0)
    {
        if (agent->FitsAt(FitStanding, 0, nullptr, nullptr) != 0)
        {
            next = StateStanding;
            cooldown = StandCooldown;
            bits |= BitMayJump;
        }
    }
    else if ((part->moveBits & CharacterPart::Jumping) != 0)
    {
        next = StateStanding;
        cooldown = StandCooldown;
    }
    else if (kind != AttackWalkInto && kind != AttackSlide && kind != AttackSlide2)
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
    else if ((part->flags & CreaturePart::FlagOnGround) == 0 && state != StateSliding && state != StateSlideEnd)
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
    if ((part->moveBits & CharacterPart::Crouching) != 0)
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

    if (next != NoState)
    {
        stateStart = time;
        bits = (bits & ~StateMask) | (static_cast<u32>(next) & StateMask);
    }

    return (bits & StateMask) != StateStanding;
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
    bits = ResetBits;
    static_cast<CharacterPart*>(agent->part)->Gravity() = gravity;
}

void JumpController::QueueLaunch(f32 launchSpeed, f32 launchGravity, u32 event)
{
    gravity = launchGravity;
    upSpeed = launchSpeed;
    bits = ((bits | LaunchQueued) & ~(LaunchEventMask << LaunchEventShift)) | (event & LaunchEventMask) << LaunchEventShift;
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
        case KindUnused8:
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
    case KindUnused8:
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
    u32 attack = part->bits & CharacterPart::AttackKindMask;
    if (attack == AttackSlide || attack == AttackSlide2)
    {
        return EventKneeSlideJump;
    }

    if (attack == AttackSpin || attack == AttackSpin2)
    {
        return EventNone;
    }

    u32 moveBits = part->moveBits;
    if ((moveBits & CharacterPart::Running) != 0)
    {
        if ((moveBits & CharacterPart::LinkedFirst) == 0)
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

    if ((moveBits & CharacterPart::LinkedFirst) == 0)
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
    part->Gravity() = gravity;
    // StartFalling, with the agent in $a2 as retail leaves it
    CallVirtual<void>(agent, agent->vtable, AgentStartFallingSlot, tellFall, agent);
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
    case KindUnused8:
        return StateFallingKind8;
    case KindFlyingKick:
        return StateFallingKick;
    case KindRadialBlast:
        return StateBlastFalling;
    default:
        return NoState;
    }
}

s32 JumpController::Start(TimeClock* clock, u32 kind, u32 maxed, u32 midFall)
{
    // The tied jump's speed and share of the turn
    constexpr f32 TiedSpeed = 10.0f;
    constexpr f32 TiedTurnShare = Rounded(0.2);

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

            speed *= 1.0f - share * 0.5f;
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
        return NoState;
    }

    if (IsNone(speed))
    {
        // No jump; a launch without speed falls at once
        if (midFall != 0 || !launch)
        {
            return NoState;
        }

        return Fall(kind, 0);
    }

    if (event == EventNone)
    {
        bits &= ~(MayDoubleJump | MayAttack);
    }
    else
    {
        bool mayDouble = kind == KindJump || kind == KindLaunch;
        bool mayAttack = mayDouble || kind == KindDouble || kind == KindSlide;
        bits = (bits & ~MayDoubleJump) | (mayDouble ? MayDoubleJump : 0);
        bits = (bits & ~MayAttack) | (mayAttack ? MayAttack : 0);
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

    bits &= ~(MayDoubleJump | MayAttack);
    PropertyHolder* properties = agent->properties;
    if (kind == KindSlide)
    {
        return NoState;
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
        return NoState;
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
        return NoState;
    }

    if (agent->FitsAt(FitKneeDrop, 1, nullptr, nullptr) != 0)
    {
        RunAgentEvent(agent, EventKneeDropHang, 0, 0, 0);
        return StateKneeDropHang;
    }

    bits |= KneeDropWaits;
    return NoState;
}

void JumpController::Grounded(TimeClock* clock, u32 mayJump, u32 cross, s32* next)
{
    // A jump is still allowed this long after leaving the ground
    constexpr f32 LateJumpSeconds = Rounded(0.11);

    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 kind = part->bits & CharacterPart::AttackKindMask;
    bool sliding = kind == AttackSlide || kind == AttackSlide2;
    bool canJump;
    if ((part->flags & CreaturePart::FlagOnGround) != 0)
    {
        leftGroundTime = 0;
        if (agent->Linked() != 0)
        {
            // Tied: not while the link slams
            canJump = (agent->link->bits & CharacterLink::StateMask) != CharacterLink::StateSlamming;
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

    if (mayJump == 0 || !canJump || cross == 0 || (bits & CrossHeld) != 0)
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
    if ((part->moveBits & CharacterPart::Crouching) != 0 && fits == 0)
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
    u32 doublePhase = 0;
    if ((bits & MayDoubleJump) != 0 && !IsNone(doubleSpeed))
    {
        doublePhase = PressPhase(clock, crossTime, TicksOf(MaxedSeconds));
    }

    u32 attackPhase = 0;
    if ((bits & MayAttack) != 0)
    {
        s32 pressed = circleTime;
        s32 window = TicksOf(properties->GetFloat(PropAttackWindowUp));
        attackPhase = PressPhase(clock, pressed, window);
    }

    s32 end = start + stateTicks;
    if (attackPhase == 0)
    {
        // Retail bug: the knee drop's wait for room is cleared here, before it's read below, so the knee drop never waits
        bits &= ~KneeDropWaits;
    }

    if (doublePhase != 0)
    {
        *next = Start(clock, KindDouble, doublePhase == 2, 0);
        return;
    }

    if (attackPhase != 0 || (bits & KneeDropWaits) != 0)
    {
        *next = AirAttack(kind);
        return;
    }

    // Falling once at the top
    if (TimeReached(clock, clock->time, end, 8) != 0)
    {
        *next = Fall(kind, 0);
        return;
    }

    part->Gravity() = gravity;
}

void JumpController::Hang(TimeClock* clock, u32 kind, u32, s32* next)
{
    // The speed scale an air attack asks for
    constexpr f32 AirAttackScale = 3.0f;

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
            part->Gravity() = gravity;
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
    part->Gravity() = gravity;
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
    if ((part->flags & CreaturePart::FlagOnGround) != 0)
    {
        *next = StateGrounded;
        return;
    }

    PropertyHolder* properties = agent->properties;
    u32 doublePhase = 0;
    if ((bits & MayDoubleJump) != 0)
    {
        doublePhase = PressWithin(clock, crossTime, stateStart, TicksOf(LateDoubleSeconds));
    }

    u32 attackPhase = 0;
    if ((bits & MayAttack) != 0)
    {
        s32 pressed = circleTime;
        s32 window = TicksOf(properties->GetFloat(PropAttackWindowDown));
        attackPhase = PressWithin(clock, pressed, stateStart, window);
    }

    if (doublePhase == 1)
    {
        *next = Start(clock, KindDouble, 0, 1);
    }
    else if (attackPhase == 1)
    {
        *next = AirAttack(kind);
    }
    else
    {
        // Retail bug: the kind's falling gravity Fall gave lasts one frame, property 1 replaces it from here
        gravity = properties->GetFloat(PropGravity);
    }

    part->Gravity() = gravity;
}

void JumpController::Frame(f32 cross, f32 circle, TimeClock* clock, u32 mayJump)
{
    constexpr f32 AirAttackScale = 3.0f;

    auto* part = static_cast<CharacterPart*>(agent->part);
    u32 crossDown = 0.0f < cross;
    u32 circleDown = 0.0f < circle;
    s32 next = NoState;
    s32 time = clock->time;
    if ((bits & CrossHeld) == 0 && crossDown != 0)
    {
        crossTime = time;
    }

    if ((bits & CircleHeld) == 0 && circleDown != 0)
    {
        circleTime = time;
    }

    if ((bits >> NextShift & StateMask) != StateNone)
    {
        bits = (bits & ~StateMask) | (bits >> NextShift & StateMask);
        bits = (bits & ~(StateMask << NextShift)) | StateNone << NextShift;
        stateStart = time;
    }

    // No double jump nor air attack while spinning or tied
    u32 kind = part->bits & CharacterPart::AttackKindMask;
    if (kind == AttackSpin || kind == AttackSpin2 || agent->Linked() != 0)
    {
        bits &= ~(MayDoubleJump | MayAttack);
    }

    if ((bits & LaunchQueued) != 0)
    {
        if (0.0f < upSpeed)
        {
            bits = (bits & ~MayDoubleJump) | (agent->Linked() == 0 ? MayDoubleJump : 0);
            bits &= ~MayAttack;
            u32 event = bits >> LaunchEventShift & LaunchEventMask;
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

        bits &= ~LaunchQueued;
    }
    else if ((part->moveBits & CharacterPart::Clawing) != 0)
    {
        // Nina's claw keeps it on the ground
        next = StateGrounded;
    }
    else
    {
        switch (bits & StateMask)
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
            if ((dropping->flags & CreaturePart::FlagOnGround) != 0)
            {
                PropertyHolder* properties = agent->properties;
                RunAgentEvent(agent, EventKneeDropLand, reinterpret_cast<u32>(agent->instance), 0, 0);
                gravity = properties->GetFloat(PropGravity);
                dropping->Gravity() = gravity;
                next = StateGrounded;
            }

            dropping->RequestForward(0.0f);
            dropping->RequestSideways(0.0f);
            dropping->RequestScale(AirAttackScale);
            break;
        }
        case StateRisingKind8:
            Rise(clock, KindUnused8, &next);
            break;
        case StateFallingKind8:
            FallFrame(clock, KindUnused8, &next);
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

    bits = (bits & ~(CrossHeld | CircleHeld)) | (crossDown != 0 ? CrossHeld : 0) | (circleDown != 0 ? CircleHeld : 0);
    if (next != NoState)
    {
        bits = (bits & ~(StateMask << NextShift)) | (static_cast<u32>(next) & StateMask) << NextShift;
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
    bits = StateNoNext << NextShift;
}

void SpinController::Sweep(f32 reach, f32 pushBack)
{
    // The rays' turn a frame and a full turn (65536ths), a quarter turn (radians) between them, their height above the
    // character's position
    constexpr s32 SweepStep = 0x21D1;
    constexpr s32 FullTurn = 0x10000;
    constexpr f32 QuarterTurn = 0x1.921fb6p+0f;
    constexpr f32 SweepHeight = 0x1.ee41b4p-1f;

    sweepAngle += SweepStep;
    if (FullTurn < sweepAngle)
    {
        sweepAngle -= FullTurn;
    }

    for (s32 ray = 0; ray < 4; ray++)
    {
        s32 angle = sweepAngle;
        s32 rayAngle = *AddRadiansToAngle(&angle, static_cast<f32>(ray) * QuarterTurn);
        f32 cosine;
        f32 sine;
        CosSin16(&rayAngle, &cosine, &sine);
        Vector4 way = {cosine * reach, 0.0f, sine * reach, 1.0f};
        Vector4 start;
        agent->Position(&start);
        start.y += SweepHeight;
        Vector4 end = {start.x + way.x, start.y + way.y, start.z + way.z, 1.0f};
        Vector4 hit;
        if (GetCollisionCheck(agent->instance->chunk, &start, &end, SolidSurfaces, nullptr, &hit, nullptr) != 0)
        {
            PushBack(agent, &way, &end, &hit, pushBack);
        }

        void* results[MostSwept];
        InstanceRayHit query;
        query.results = results;
        query.count = 0;
        query.most = MostSwept;
        query.distance = NoHitDistance;
        query.bits = InstanceRayHit::BitAllWanted;
        query.wantedFlags = SolidSurfaces;
        query.unwantedFlags = ReferencedObject::FlagAsleep;
        query.skipped[0] = nullptr;
        query.skipped[1] = nullptr;
        query.instance = nullptr;
        if (SegmentHitsInstances(agent->instance->chunk, &start, &end, &query, SweptKinds, nullptr, &hit, 0) == 0)
        {
            continue;
        }

        // Touched (without a normal), and pushed back by the solid ones
        auto* other = static_cast<InstanceContext*>(query.instance);
        agent->TouchedNothing(other);
        AgentNode* node = AgentNodeOf(other);
        if (node == nullptr || (node->agent->properties->state & SolidToSpin) != 0)
        {
            PushBack(agent, &way, &end, &hit, pushBack);
        }
    }
}

u32 SpinController::Start()
{
    CharacterLink* link = agent->link;
    u32 moveBits = static_cast<CharacterPart*>(agent->part)->moveBits;
    // Not while ducking nor tied in a jump
    if ((moveBits & CharacterPart::Crouching) != 0 || (link != nullptr && (moveBits & CharacterPart::Jumping) != 0))
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
    u32 state = bits & StateMask;
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
        tiedLink->bits &= ~(CharacterLink::GaitMask << CharacterLink::GaitShift);
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
    u32 kind = part->bits & CharacterPart::AttackKindMask;
    // Walking into things and its own attack kinds keep it going
    bool ownKind = kind == AttackWalkInto || kind == AttackSpin || kind == AttackSpin2;
    u32 pressed = 0.0f < button;
    s32 next = NoState;
    CharacterLink* link = agent->link;
    bool slamming = link != nullptr && (link->bits & CharacterLink::StateMask) == CharacterLink::StateSlamming;
    if ((bits >> NextShift & StateMask) != StateNoNext)
    {
        bits = (bits & ~StateMask) | (bits >> NextShift & StateMask);
        stateStart = clock->time;
        bits = (bits & ~(StateMask << NextShift)) | StateNoNext << NextShift;
    }

    if ((part->moveBits & MoveSlideJump) != 0 || (part->moveBits & CharacterPart::Clawing) != 0 || slamming || !ownKind)
    {
        next = End(0);
    }
    else
    {
        switch (bits & StateMask)
        {
        case StateNone:
            if (pressed != 0 && (bits & ButtonHeld) == 0)
            {
                next = Start();
            }

            break;
        case StateSpinningTied:
        case StateSpinning:
        {
            bool tied = (bits & StateMask) == StateSpinningTied;
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

    bits = (bits & ~ButtonHeld) | (pressed != 0 ? ButtonHeld : 0);
    if (next != NoState)
    {
        bits = (bits & ~(StateMask << NextShift)) | (static_cast<u32>(next) & StateMask) << NextShift;
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
    SetSize(this, Rounded(1.8), Rounded(0.29), 0.75f, Rounded(0.29));
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
    SetSize(this, 9.0f, Rounded(1.6), 5.5f, Rounded(1.6));
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
    switch (agent->properties->GetInt(CharacterProperty))
    {
    case Crash:
        SetCrashSizes();
        break;
    case Cortex:
        SetSize(this, Rounded(1.8), Rounded(0.29), 0.75f, Rounded(0.29));
        probeCount = 3;
        SetProbe(this, 0, 4, 4.0f);
        SetProbe(this, 1, 3, 4.0f);
        SetProbe(this, 2, 1, 4.0f);
        break;
    case TallCrash:
        SetSize(this, 2.0f, Rounded(0.29), 0.75f, Rounded(0.29));
        probeCount = 0;
        break;
    case Nina:
        SetSize(this, Rounded(1.8), Rounded(0.29), 0.75f, Rounded(0.29));
        probeCount = 4;
        SetProbe(this, 0, 4, Rounded(0.7));
        SetProbe(this, 1, 3, Rounded(0.7));
        SetProbe(this, 2, 2, 1.0f);
        SetProbe(this, 3, 1, 1.0f, Rounded(0.35), 0.0f);
        break;
    case NoCharacter:
        SetSize(this, 1.0f, 0.5f, 1.0f, 0.5f);
        probeCount = 0;
        break;
    case MechaBandicoot:
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
