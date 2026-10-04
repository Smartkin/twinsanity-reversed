#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "retail/libc.h"

#include <cstddef>
#include <cstdint>

// Nina's claw: the graple instance tied to her by a spring is her claw, which she sends at what her target lock picks (a
// grabbable to hang from or to leap to) or swipes with. Each state has a step, and the frame takes the next state on

EABI_EXPORT(FUN_001467d0, &ClawController::Frame);

namespace
{
// A spring of an instance's attachments: the instance at its other end and its bits (4-6 its kind)
struct Spring
{
    static constexpr u32 KindMask = 0x70;

    u8 unknown00[0x94];
    Reference* other;
    u8 unknown98[0x20];
    u32 bits;
};

// The springs tying an instance to others: their count in bits 0-4
struct SpringSet
{
    static constexpr u32 CountMask = 0x1F;

    Spring* springs[16];
    u32 count;
};

// An instance's attachments (its kind 6 node): its springs
struct AttachmentsNode
{
    u8 unknown00[0x70];
    SpringSet* springs;
};

// The claw's states (bits 0-3 of its bits; the frame keeps the next one in bits 4-7)
enum ClawState : s32
{
    StateReady = 0,
    StateReaching = 1,
    StateFlying = 2,
    // The claw coming back to her hand: nothing enters it
    StateReturning = 3,
    StatePulled = 4,
    StateHanging = 5,
    StateLettingGo = 6,
    StateDropped = 7,
    StateJumpedOff = 8,
    StateLeaping = 9,
    StateWindingUp = 10,
    StateSwipingOut = 11,
    StateSwipingBack = 12,
    StateAfterSwipe = 13,
};
constexpr s32 NoNextState = -1;

// The nodes: her object node (its motion), her model node, her attachments, the grabbables' and the graples' agent nodes
constexpr u32 ObjectNodeKind = 1;
constexpr u32 ModelNodeKind = 3;
constexpr u32 AttachmentsKind = 6;
constexpr u32 GrabbableKind = 0x11;
constexpr u32 GrapleKind = 0x13;
// Her model's exit point the claw hangs from, and her float property of her gravity
constexpr u32 ClawHandExitPoint = 8;
constexpr u32 GravityProperty = 1;

// The part's attack kinds that keep her from grabbing: spinning, body slamming and sliding
constexpr u32 AttackSpin = 6;
constexpr u32 AttackSlam = 7;
constexpr u32 AttackSlide = 8;
constexpr u32 AttackSpin2 = 10;
constexpr u32 AttackSlam2 = 11;
constexpr u32 AttackSlide2 = 12;

// Her events: a grab, its target lost while reaching, the claw hitting a hook and a point, a swipe, the leap, hanging, dropped
// and jumped off the hook
constexpr u32 EventGrab = 0x37;
constexpr u32 EventTargetLost = 0x38;
constexpr u32 EventHookHit = 0x39;
constexpr u32 EventPointHit = 0x3A;
constexpr u32 EventSwipe = 0x3C;
constexpr u32 EventLeap = 0x3D;
constexpr u32 EventHang = 0x3E;
constexpr u32 EventDrop = 0x3F;
constexpr u32 EventJumpOff = 0x40;
// The graple's: the claw leaving her hand, hitting, the leap, back (hanging or retracted), a swipe going out, coming back, done
constexpr u32 GrapleLeaves = 0xB;
constexpr u32 GrapleHits = 0xC;
constexpr u32 GrapleLeap = 0xD;
constexpr u32 GrapleBack = 0xE;
constexpr u32 GrapleSwipeOut = 0xF;
constexpr u32 GrapleSwipeBack = 0x10;
constexpr u32 GrapleSwipeDone = 0x11;

// 65536ths of a turn in radians and back
constexpr f32 RadiansPerUnit = 0x1.921fb6p-14f;
constexpr f32 UnitsPerRadian = 0x1.45f306p+13f;

InstanceContext* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
}

CharacterPart* PartOf(CharacterAgent* agent)
{
    return static_cast<CharacterPart*>(agent->part);
}

u32 GrabOf(const ClawController* claw)
{
    return claw->bits >> ClawController::GrabShift & ClawController::GrabMask;
}

void SetGrab(ClawController* claw, u32 grab)
{
    claw->bits = (claw->bits & ~(ClawController::GrabMask << ClawController::GrabShift)) | grab << ClawController::GrabShift;
}

// The clock units of so many seconds (cut down to whole ones)
s32 ClockUnits(f32 seconds)
{
    return static_cast<s32>(g_ClockUnitsPerSecond * seconds);
}

bool TimeIsUp(const ClawController* claw, const TimeClock* clock)
{
    return static_cast<s32>(clock->time - claw->stateStart) >= claw->duration;
}

// How far into its time the state is (at least 0.01, 1 once the time is up): whether the time is up
bool StateShare(const ClawController* claw, const TimeClock* clock, f32* share)
{
    constexpr f32 LeastShare = Rounded(0.01);

    s32 elapsed = static_cast<s32>(clock->time - claw->stateStart);
    f32 fraction = static_cast<f32>(elapsed) * g_SecondsPerClockUnit / (static_cast<f32>(claw->duration) * g_SecondsPerClockUnit);
    if (elapsed >= claw->duration)
    {
        *share = 1.0f;
        return true;
    }

    *share = fraction < LeastShare ? LeastShare : fraction;
    return false;
}

// A point a share of the way from one point to another (w 1)
void Between(Vector4* out, const Vector4* from, const Vector4* to, f32 share)
{
    out->x = from->x + (to->x - from->x) * share;
    out->y = from->y + (to->y - from->y) * share;
    out->z = from->z + (to->z - from->z) * share;
    out->w = 1.0f;
}

// An angle (65536ths of a turn) a share of the way from one to another, through radians
s32 AngleBetween(s32 from, s32 to, f32 share)
{
    f32 radians = static_cast<f32>(to) * RadiansPerUnit * share + static_cast<f32>(from) * RadiansPerUnit * (1.0f - share);
    return static_cast<s32>(radians * UnitsPerRadian);
}

// An angle given a turn more or less to be within half a turn of another
s32 NearestTo(s32 angle, s32 other)
{
    s32 difference = static_cast<s32>(static_cast<u32>(angle) - static_cast<u32>(other));
    if (difference > 0x8000)
    {
        return static_cast<s32>(static_cast<u32>(angle) - 0x10000);
    }

    if (difference < -0x8000)
    {
        return static_cast<s32>(static_cast<u32>(angle) + 0x10000);
    }

    return angle;
}

// Whether an attack kind is a slam, a slide or a spin
bool IsAttacking(u32 kind)
{
    return kind == AttackSlam || kind == AttackSlam2 || kind == AttackSlide || kind == AttackSlide2 || kind == AttackSpin
           || kind == AttackSpin2;
}

// Whether a point is none (within 5e-05 of 0 on each axis)
bool IsNone(const Vector4& point)
{
    constexpr f32 Epsilon = Rounded(5e-05);
    return __builtin_fabsf(point.x) <= Epsilon && __builtin_fabsf(point.y) <= Epsilon && __builtin_fabsf(point.z) <= Epsilon;
}

// Her claw hand: her model's exit point 8 (none without exit points)
ExitPointAnimation* ClawHandOf(InstanceContext* instance)
{
    OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNodeKind))->animator;
    return animator->exitPoints != nullptr ? animator->exitPoints->data[ClawHandExitPoint] : nullptr;
}

// An instance's graple agent, read the way retail does without checking the instance has a graple node (the word at 0x18 then)
GrapleAgent* GrapleAgentOf(InstanceContext* instance)
{
    void* node = GetGameNode(&instance->nodes, GrapleKind);
    return *reinterpret_cast<GrapleAgent* const*>(reinterpret_cast<std::uintptr_t>(node) + offsetof(AgentNode, agent));
}

// The claw's graple told an event, when she has one
void TellGraple(ClawController* claw, u32 event)
{
    InstanceContext* graple = ObjectOf(claw->graple);
    if (graple != nullptr)
    {
        RunAgentEvent(GrapleAgentOf(graple), event, 0, 0, 0);
    }
}

// The graple taken from her attachments' springs: the instance at the other end of the last one of no kind
void TakeGraple(ClawController* claw, InstanceContext* instance)
{
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, AttachmentsKind));
    if (attachments == nullptr || attachments->springs == nullptr)
    {
        return;
    }

    // Indexed through a pointer: the count's 5 bits can say more than the 16 there's room for, and retail reads past them
    Spring* const* springs = attachments->springs->springs;
    u32 count = attachments->springs->count & SpringSet::CountMask;
    for (u32 index = 0; index < count; index++)
    {
        Spring* spring = springs[index];
        if (spring != nullptr && (spring->bits & Spring::KindMask) == 0)
        {
            AssignReference(&claw->graple, ObjectOf(spring->other));
        }
    }
}

// No speed of her own forward or sideways this frame
void HoldStill(CharacterPart* part)
{
    part->RequestForward(0.0f);
    part->RequestSideways(0.0f);
}

// No speed of her own at all this frame, and no gravity
void HoldInPlace(CharacterPart* part)
{
    HoldStill(part);
    part->RequestVertical(0.0f);
    part->Gravity() = 0.0f;
}

// Her own gravity (her float property) again
void TakeOwnGravity(CharacterAgent* agent)
{
    PartOf(agent)->Gravity() = agent->properties->GetFloat(GravityProperty);
}

// She jumps off the hook (its event told her instance): 0.1 seconds of state 8
void JumpOff(ClawController* claw, s32* next)
{
    RunAgentEvent(claw->agent, EventJumpOff, reinterpret_cast<u32>(claw->agent->instance), 0, 0);
    claw->duration = ClockUnits(Rounded(0.1));
    *next = StateJumpedOff;
}

// The grab point: the target's landing point (a grabbable's, from its waypoints), else its position and her turn of the grab
// offset; the default box's corner without a target
Vector4 GrabPointOf(InstanceContext* target, ObjectPlace* place)
{
    Vector4 grab;
    if (target == nullptr)
    {
        grab = g_DefaultBox.min;
        grab.w = 1.0f;
        return grab;
    }

    auto* grabbable = static_cast<AgentNode*>(GetGameNode(&target->nodes, GrabbableKind));
    const Vector4* landing = grabbable != nullptr ? GrabbableLandingPoint(grabbable, place) : nullptr;
    if (landing != nullptr)
    {
        grab = *landing;
        grab.w = 1.0f;
        return grab;
    }

    Vector4 offset;
    VuRotateVector(&place->matrix, &g_ClawGrabOffset, &offset);
    ObjectPlace* targetPlace = target->place;
    targetPlace->SyncPosition();
    grab = targetPlace->position;
    grab.x = grab.x + offset.x;
    grab.y = grab.y + offset.y;
    grab.z = grab.z + offset.z;
    return grab;
}

// An instance moved to a position (its place's position taken from its matrix first), queued when it moved
void MoveInstance(InstanceContext* instance, const Vector4* position)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(position))
    {
        QueueObject(instance);
    }
}

// An instance turned to angles about x, y and z (its rotation taken from its matrix first), queued when it turned
void TurnInstance(InstanceContext* instance, const s32* x, const s32* y, const s32* z)
{
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationXYZ(&rotation, x, y, z);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }
}
}

static_assert(offsetof(ClawController, grabYaw) == 0x18);
static_assert(offsetof(ClawController, claw) == 0x30);
static_assert(offsetof(ObjectNode, motion) == 0x128);
static_assert(offsetof(CharacterAgent, buttons) == 0xF0);

ClawController* ClawController::Construct(ClawController* claw, CharacterAgent* agent)
{
    claw->graple = nullptr;
    claw->agent = agent;
    TargetLock::Construct(&claw->lock);
    claw->Reset();
    claw->lock.offset = g_ClawLockOffset;
    claw->lock.SetFlatShape(10.0f, 1.0f, 7.0f);
    claw->lock.AddKind(GrabbableKind, 1);
    return claw;
}

void ClawController::Reset()
{
    RetailLibc::MemorySet(&bits, 0, sizeof(bits));
    bits = (bits & ~(StateMask | StateMask << NextShift | GrabMask << GrabShift)) | NoNext << NextShift;
    AssignReference(&graple, nullptr);
    stateStart = 0;
    duration = 0;
    circleTime = 0;
    lock.Reset();
}

void ClawController::AimPoints(Vector4* handOut, s32* facingYaw, Vector4* point, s32* grabYawOut, Vector4* grabPoint)
{
    constexpr f32 ReachAhead = 5.0f;

    InstanceContext* instance = agent->instance;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    const Matrix4x4* handMatrix = &UpdateExitPointMatrix(ClawHandOf(instance))->matrix;
    Vector4 handPoint;
    if (handOut != nullptr || facingYaw != nullptr)
    {
        handPoint = *RowOf(handMatrix, 3);
    }

    // The claw goes to its target's point (the middle of its box), without one 5 units ahead of her hand along its z axis
    const Matrix4x4* aimed = nullptr;
    Vector4 aim;
    if (facingYaw != nullptr || point != nullptr || grabYawOut != nullptr)
    {
        aimed = ObjectOf(lock.target) != nullptr ? &lock.targetMatrix : nullptr;
        if (aimed != nullptr)
        {
            aim = *RowOf(aimed, 3);
        }
        else
        {
            aim = *RowOf(handMatrix, 2);
            f32 scale = ReachAhead / Kept(__builtin_sqrtf(aim.x * aim.x + aim.y * aim.y + aim.z * aim.z));
            aim.x = aim.x * scale + handMatrix->m[3][0];
            aim.y = aim.y * scale + handMatrix->m[3][1];
            aim.z = aim.z * scale + handMatrix->m[3][2];
        }
    }

    if (facingYaw != nullptr)
    {
        s32 yaw;
        YawOfDirection(&yaw, RowOf(&place->matrix, 2));
        *facingYaw = yaw;
    }

    Vector4 grab;
    if (grabYawOut != nullptr || grabPoint != nullptr)
    {
        grab = GrabPointOf(ObjectOf(lock.target), place);
    }

    if (handOut != nullptr)
    {
        *handOut = handPoint;
    }

    if (point != nullptr)
    {
        *point = aim;
    }

    // Facing a hook along its -z, else from her hand toward the grab point (the claw's point without one)
    if (grabYawOut != nullptr)
    {
        if (GrabOf(this) != GrabHook)
        {
            Vector4 way = IsNone(grab) ? aim : grab;
            way.x = way.x - hand.x;
            way.y = way.y - hand.y;
            way.z = way.z - hand.z;
            s32 yaw;
            YawOfDirection(&yaw, &way);
            *grabYawOut = yaw;
        }
        else if (aimed != nullptr)
        {
            Vector4 back = *RowOf(aimed, 2);
            back.x = -back.x;
            back.y = -back.y;
            back.z = -back.z;
            s32 yaw;
            YawOfDirection(&yaw, &back);
            *grabYawOut = yaw;
        }
    }

    if (grabPoint != nullptr && !IsNone(grab))
    {
        *grabPoint = grab;
    }
}

void ClawController::PlaceGraple(TimeClock* clock, const Vector4* clawPoint, const Vector4* handPoint)
{
    if (ObjectOf(graple) == nullptr)
    {
        return;
    }

    InstanceContext* instance = agent->instance;
    ExitPointAnimation* clawHand = UpdateExitPointMatrix(ClawHandOf(instance));
    GrapleAgent* grapleAgent = GrapleAgentOf(ObjectOf(graple));
    if (handPoint != nullptr)
    {
        grapleAgent->SetEnds(handPoint, clawPoint);
    }

    // Pulled to the grab (unless she's tied to the other character), she's moved for her hand to be where it's asked; otherwise
    // the graple is turned like her hand
    u32 state = bits & StateMask;
    if (state >= StatePulled && state <= StateLettingGo && agent->Linked() == 0)
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 position = place->position;
        position.x = position.x - clawHand->matrix.m[3][0] + handPoint->x;
        position.y = position.y - clawHand->matrix.m[3][1] + handPoint->y;
        position.z = position.z - clawHand->matrix.m[3][2] + handPoint->z;
        MoveInstance(instance, &position);
        return;
    }

    InstanceContext* grapleInstance = ObjectOf(graple);
    ObjectPlace* place = grapleInstance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &clawHand->matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(grapleInstance);
    }
}

void ClawController::Ready(TimeClock* clock, u32 circle, s32* next)
{
    constexpr f32 ReachSeconds = Rounded(0.12);

    u32 kind = static_cast<BasicAgentPart*>(agent->part)->bits & BasicAgentPart::LowByteMask;
    if (circle == 0 || IsAttacking(kind))
    {
        SetGrab(this, GrabNone);
        return;
    }

    u32 event = EventNone;
    InstanceContext* aimed = ObjectOf(lock.target);
    auto* grabbable = aimed != nullptr ? static_cast<AgentNode*>(GetGameNode(&aimed->nodes, GrabbableKind)) : nullptr;
    if (grabbable != nullptr)
    {
        *next = StateReaching;
        duration = ClockUnits(ReachSeconds);
        if (IsHookGrabbable(grabbable) != 0)
        {
            SetGrab(this, GrabHook);
            event = EventGrab;
        }
        // Retail asks the grabbable again
        else if (IsHookGrabbable(grabbable) == 0)
        {
            SetGrab(this, GrabPoint);
            event = EventGrab;
        }
    }
    // Nothing to grab: a swipe once circle was pressed since the state began
    else if (circleTime >= stateStart)
    {
        SetGrab(this, GrabSwipe);
        duration = ClockUnits(ReachSeconds);
        *next = StateWindingUp;
        event = EventSwipe;
    }
    else
    {
        SetGrab(this, GrabNone);
    }

    RunAgentEvent(agent, event, 0, 0, 0);
}

void ClawController::Reach(TimeClock* clock, u32 circle, s32* next)
{
    CharacterPart* part = PartOf(agent);
    InstanceContext* aimed = ObjectOf(lock.target);
    if ((part->flags & CreaturePart::FlagOnGround) != 0)
    {
        HoldStill(part);
    }

    part->Gravity() = agent->properties->GetFloat(GravityProperty);
    if (aimed == nullptr)
    {
        RunAgentEvent(agent, EventTargetLost, 0, 0, 0);
        *next = StateReady;
        return;
    }

    if (TimeIsUp(this, clock))
    {
        TellGraple(this, GrapleLeaves);
        *next = StateFlying;
        duration = ClockUnits(Rounded(0.1));
        AimPoints(&hand, nullptr, nullptr, nullptr, nullptr);
        PlaceGraple(clock, &hand, &hand);
    }
}

void ClawController::Fly(TimeClock* clock, u32 circle, s32* next)
{
    constexpr f32 PullSeconds = Rounded(0.28);

    CharacterPart* part = PartOf(agent);
    if ((part->flags & CreaturePart::FlagOnGround) != 0)
    {
        HoldStill(part);
    }

    part->Gravity() = 0.0f;
    agent->buttons.locked = 0;
    AimPoints(&hand, &startYaw, &to, nullptr, nullptr);
    from = hand;
    f32 share;
    if (!StateShare(this, clock, &share))
    {
        Between(&claw, &from, &to, share);
        PlaceGraple(clock, &claw, &hand);
        return;
    }

    // The claw there: a hook or a point pulls her there (her object node's motion given the pull's velocity, the velocity it had
    // kept as the one it starts from) in 0.28 seconds; anything else would take a second with no next state
    MotionState* motion = static_cast<ObjectNode*>(GetGameNode(&agent->instance->nodes, ObjectNodeKind))->motion;
    claw = to;
    Vector4 pull = to;
    f32 seconds = 1.0f;
    u32 grab = GrabOf(this);
    if (grab == GrabHook || grab == GrabPoint)
    {
        RunAgentEvent(agent, grab == GrabHook ? EventHookHit : EventPointHit, 0, 0, 0);
        TellGraple(this, GrapleHits);
        seconds = PullSeconds;
        *next = StatePulled;
    }

    f32 inverse = 1.0f / seconds;
    pull.x = (pull.x - from.x) * inverse;
    pull.y = (pull.y - from.y) * inverse;
    pull.z = (pull.z - from.z) * inverse;
    motion->startVelocity = motion->velocity;
    motion->velocity = pull;
    duration = ClockUnits(seconds);
    PlaceGraple(clock, &claw, &hand);
}

void ClawController::Retract(TimeClock* clock, u32 circle, s32* next)
{
    TakeOwnGravity(agent);
    Vector4 handPoint;
    AimPoints(&handPoint, nullptr, nullptr, nullptr, nullptr);
    f32 share;
    if (StateShare(this, clock, &share))
    {
        TellGraple(this, GrapleBack);
        *next = StateReady;
        return;
    }

    Vector4 clawPoint;
    Between(&clawPoint, &claw, &handPoint, share);
    PlaceGraple(clock, &clawPoint, &handPoint);
}

void ClawController::Pull(TimeClock* clock, u32 circle, s32* next)
{
    CharacterPart* part = PartOf(agent);
    InstanceContext* instance = agent->instance;
    HoldInPlace(part);
    AimPoints(nullptr, nullptr, &to, &grabYaw, nullptr);
    grabYaw = NearestTo(grabYaw, startYaw);
    claw = to;
    f32 share;
    if (StateShare(this, clock, &share))
    {
        u32 grab = GrabOf(this);
        if (grab == GrabHook)
        {
            RunAgentEvent(agent, EventHang, 0, 0, 0);
            TellGraple(this, GrapleBack);
            *next = StateHanging;
        }
        else if (grab == GrabPoint)
        {
            duration = ClockUnits(0.5f);
            *next = StateLeaping;
        }
    }

    // Her hand drawn to the grab, her yaw turned from the one she faced to the grab's
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    s32 x;
    s32 y;
    s32 z;
    AnglesOfRotation(&place->rotation, &x, &y, &z);
    y = AngleBetween(startYaw, grabYaw, share);
    Between(&hand, &from, &to, share);
    TurnInstance(instance, &x, &y, &z);
    PlaceGraple(clock, &claw, &hand);
}

void ClawController::LetGo(TimeClock* clock, u32 cross, s32* next)
{
    if (cross != 0)
    {
        JumpOff(this, next);
        return;
    }

    InstanceContext* instance = agent->instance;
    TakeOwnGravity(agent);
    RunAgentEvent(agent, EventDrop, reinterpret_cast<u32>(instance), 0, 0);
    duration = ClockUnits(Rounded(0.2));
    *next = StateDropped;
}

void ClawController::Leap(TimeClock* clock, s32* next)
{
    // Up at 10 units a second, forward 1.1 times as fast as covers the flat way to the grab point in the time her gravity brings
    // her to its height (the later time, 0.01 seconds when it's never there)
    constexpr f32 UpSpeed = 10.0f;
    constexpr f32 NoTime = Rounded(0.01);
    constexpr f32 Faster = Rounded(1.1);

    if (clock->time != static_cast<u32>(stateStart))
    {
        if (TimeIsUp(this, clock))
        {
            *next = StateReady;
            lock.Drop();
        }

        return;
    }

    InstanceContext* instance = agent->instance;
    f32 gravity = agent->properties->GetFloat(GravityProperty);
    TellGraple(this, GrapleLeap);
    Vector4 way;
    AimPoints(nullptr, nullptr, nullptr, nullptr, &way);
    way.x = way.x - to.x;
    way.y = way.y - to.y;
    way.z = way.z - to.z;
    f32 times[2];
    f32 time;
    switch (SolveQuadratic(times, gravity * -0.5f, UpSpeed, -way.y))
    {
    case 1:
        time = times[0];
        break;
    case 2:
        time = times[0] > times[1] ? times[0] : times[1];
        break;
    default:
        time = NoTime;
        break;
    }

    Vector4 velocity = {0.0f, UpSpeed, __builtin_sqrtf(way.x * way.x + way.z * way.z) / time * Faster, 1.0f};
    agent->PushBack(gravity, &velocity, EventLeap, instance);
}

void ClawController::WindUp(TimeClock* clock, u32 circle, s32* next)
{
    InstanceContext* aimed = ObjectOf(lock.target);
    CharacterPart* part = PartOf(agent);
    HoldStill(part);
    part->Gravity() = agent->properties->GetFloat(GravityProperty);
    AimPoints(&hand, nullptr, nullptr, nullptr, nullptr);
    PlaceGraple(clock, &hand, &hand);
    f32 share;
    if (StateShare(this, clock, &share))
    {
        TellGraple(this, GrapleSwipeOut);
        *next = StateSwipingOut;
        duration = ClockUnits(Rounded(0.18));
    }

    if (aimed == nullptr)
    {
        return;
    }

    // She turns over the state toward the claw's point (as it was before this frame's)
    InstanceContext* instance = agent->instance;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Vector4 way = to;
    AimPoints(&hand, nullptr, &to, nullptr, nullptr);
    ObjectPlace* turned = instance->place;
    turned->SyncRotation();
    s32 x;
    s32 y;
    s32 z;
    AnglesOfRotation(&turned->rotation, &x, &y, &z);
    way.x = way.x - place->matrix.m[3][0];
    way.y = way.y - place->matrix.m[3][1];
    way.z = way.z - place->matrix.m[3][2];
    s32 facing = y;
    s32 yaw;
    YawOfDirection(&yaw, &way);
    y = AngleBetween(facing, NearestTo(yaw, facing), share);
    TurnInstance(instance, &x, &y, &z);
}

void ClawController::SwipeOut(TimeClock* clock, u32 circle, s32* next)
{
    CharacterPart* part = PartOf(agent);
    HoldStill(part);
    part->Gravity() = agent->properties->GetFloat(GravityProperty);
    f32 share;
    if (StateShare(this, clock, &share))
    {
        TellGraple(this, GrapleSwipeBack);
        *next = StateSwipingBack;
        duration = ClockUnits(Rounded(0.08));
    }

    AimPoints(&hand, nullptr, &to, nullptr, nullptr);
    Between(&claw, &hand, &to, share);
    PlaceGraple(clock, &claw, &hand);
}

void ClawController::SwipeBack(TimeClock* clock, u32 circle, s32* next)
{
    CharacterPart* part = PartOf(agent);
    HoldStill(part);
    part->Gravity() = agent->properties->GetFloat(GravityProperty);
    f32 share;
    if (StateShare(this, clock, &share))
    {
        TellGraple(this, GrapleSwipeDone);
        *next = StateAfterSwipe;
        duration = ClockUnits(Rounded(0.1));
    }

    AimPoints(&hand, nullptr, &to, nullptr, nullptr);
    Between(&claw, &to, &hand, share);
    PlaceGraple(clock, &claw, &hand);
}

void ClawController::Frame(f32 circle, f32 cross, TimeClock* clock)
{
    InstanceContext* instance = agent->instance;
    u32 circlePressed = 0.0f < circle;
    u32 crossPressed = 0.0f < cross;
    s32 next = NoNextState;
    u32 now = clock->time;
    if (ObjectOf(graple) == nullptr)
    {
        TakeGraple(this, instance);
    }

    if ((bits >> NextShift & StateMask) != NoNext)
    {
        bits = (bits & ~StateMask) | (bits >> NextShift & StateMask);
        stateStart = now;
        bits = (bits & ~(StateMask << NextShift)) | NoNext << NextShift;
    }

    // The target lock searched while she can grab or the swipe goes out, dropped once she lets go, kept otherwise
    switch (bits & StateMask)
    {
    case StateReady:
    case StateReaching:
    case StateSwipingOut:
        lock.Search(clock, instance, ObjectOf(graple));
        break;
    case StateFlying:
    case StateReturning:
    case StatePulled:
    case StateHanging:
    case StateLeaping:
    case StateWindingUp:
    case StateAfterSwipe:
        lock.Keep(clock, instance, ObjectOf(graple));
        break;
    case StateLettingGo:
    case StateDropped:
    case StateJumpedOff:
    case StateSwipingBack:
        lock.Drop();
        break;
    }

    switch (bits & StateMask)
    {
    case StateReady:
        Ready(clock, circlePressed, &next);
        break;
    case StateReaching:
        Reach(clock, circlePressed, &next);
        break;
    case StateFlying:
        Fly(clock, circlePressed, &next);
        break;
    case StateReturning:
        Retract(clock, circlePressed, &next);
        break;
    case StatePulled:
        Pull(clock, circlePressed, &next);
        break;
    case StateHanging:
        // Hanging still until cross jumps off or circle is pressed again
        if (crossPressed != 0)
        {
            JumpOff(this, &next);
        }
        else if (circlePressed != 0 && (bits & CircleHeld) == 0)
        {
            next = StateLettingGo;
        }
        else
        {
            HoldInPlace(PartOf(agent));
        }

        break;
    case StateLettingGo:
        LetGo(clock, crossPressed, &next);
        break;
    case StateJumpedOff:
        if (TimeIsUp(this, clock))
        {
            TakeOwnGravity(agent);
            next = StateReady;
        }

        break;
    case StateLeaping:
        Leap(clock, &next);
        break;
    case StateWindingUp:
        WindUp(clock, circlePressed, &next);
        break;
    case StateSwipingOut:
        SwipeOut(clock, circlePressed, &next);
        break;
    case StateSwipingBack:
        SwipeBack(clock, circlePressed, &next);
        break;
    case StateAfterSwipe:
        HoldStill(PartOf(agent));
        TakeOwnGravity(agent);
        [[fallthrough]];
    case StateDropped:
        if (TimeIsUp(this, clock))
        {
            next = StateReady;
        }

        break;
    }

    if (circlePressed != 0 && (bits & CircleHeld) == 0)
    {
        circleTime = now;
    }

    bits = (bits & ~CircleHeld) | (circlePressed != 0 ? CircleHeld : 0);
    bits = (bits & ~CrossHeld) | (0.0f < cross ? CrossHeld : 0);
    if (next != NoNextState)
    {
        bits = (bits & ~(StateMask << NextShift)) | (static_cast<u32>(next) & StateMask) << NextShift;
    }
}
