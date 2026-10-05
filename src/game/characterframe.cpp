#include "game/agents.h"

#include "game/animation.h"
#include "game/camerarig.h"
#include "game/characters.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/particles.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/vehicles.h"

#include <cstddef>
#include <cstdint>

extern "C"
{
    // A pickup's object node put in a state (pickups.cpp's PickupObjectNode::SetState)
    void SetPickupState(ObjectNode* node, TimeClock* clock, u32 state) RETAIL(FUN_00108e68);
}

EABI_EXPORT(FUN_00136800, &CharacterAgent::LiftOntoGround);

namespace
{
// The pickups' code model kind
constexpr u32 PickupCodeModel = 0x11;
// A pickup's state flying to its focus, and the event the other instances near the player run
constexpr u32 PickupFliesToFocus = 6;
constexpr u32 NearPlayerEvent = 1;

// The character's hull that kicks (its hits are spins)
constexpr s32 KickHull = 7;
// The most hulls the character's own are gathered for, and the most instances the queries find
constexpr s32 HullsMost = 24;
constexpr u16 MostHits = 0xB4;
constexpr u16 MostGround = 0x40;
constexpr u16 MostHullHits = 0x100;
// The kinds of nodes the attacks look for (projectiles too, besides what damage reaches) and the touches (the pickups)
constexpr u32 AttackHitKinds = DamageableNodeKinds | 1u << NodeProjectile;
constexpr u32 PickupKinds = 1u << NodePickup;
// The contact message a splash into water sends (its hit kinds and damage)
constexpr u32 SplashKinds = HitBurning;
constexpr u8 SplashDamage = 1;

static_assert(offsetof(CharacterAgent, triggers) == 0x110);
static_assert(offsetof(CharacterAgent, cache) == 0x2B0);
static_assert(offsetof(CharacterBody, standingHull) == 0x14);
static_assert(offsetof(LookController, freeLookX) == 0x34);
static_assert(offsetof(FollowNode, camera) + offsetof(FollowCamera, lensRig) == 0x750);
static_assert(offsetof(ObjectNode, agentRef2) == 0x11C);

// The attack kind (the part's bits' low byte, which retail reads and writes as a byte)
u8& AttackKind(AgentPart* part)
{
    return *reinterpret_cast<u8*>(&static_cast<BasicAgentPart*>(part)->bits);
}

// An attack kind while a move goes on: the kind it starts from turns into the move's, and back once the move is over; other
// kinds are kept
u32 KindWhile(u32 kind, bool active, u32 from, u32 to)
{
    if (active)
    {
        return kind == from ? to : kind;
    }

    return kind == to ? from : kind;
}

bool IsSpinning(u32 kind)
{
    return kind == AttackSpin || kind == AttackSpinVariant;
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// A query of a chunk's instances into the results: none of the asleep ones, all the wanted flags needed
void StartQuery(InstanceQuery* query, void** results, u16 most, u32 wanted)
{
    query->results = results;
    query->most = most;
    query->count = 0;
    query->distance = NoHitDistance;
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = wanted;
    query->unwantedFlags = ReferencedObjectFlags::Asleep;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// The middle of a box (its w the lowest corner's)
Vector4 MiddleOf(const Box* box)
{
    Vector4 middle = box->min;
    middle.x = (middle.x + box->max.x) * 0.5f;
    middle.y = (middle.y + box->max.y) * 0.5f;
    middle.z = (middle.z + box->max.z) * 0.5f;
    return middle;
}

f32 Length(const Vector4& vector)
{
    return __builtin_sqrtf(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

void Scale(Vector4* vector, f32 scale)
{
    vector->x = vector->x * scale;
    vector->y = vector->y * scale;
    vector->z = vector->z * scale;
}

// The sum and a vector scaled (w 1)
Vector4 Sum(const Vector4& a, const Vector4& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, 1.0f};
}

Vector4 Scaled(const Vector4& vector, f32 scale)
{
    return {vector.x * scale, vector.y * scale, vector.z * scale, 1.0f};
}

Vector4 Cross(const Vector4& a, const Vector4& b, f32 w)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, w};
}

// The object node's AgentRef2 (an asleep one forgotten unless the node keeps it)
InstanceContext* AwakeAgentRef2(ObjectNode* node)
{
    InstanceContext* other = node->agentRef2;
    if (other != nullptr && other->flags.asleep && !node->flags.keepsAgentRef2)
    {
        node->agentRef2 = nullptr;
    }

    return node->agentRef2;
}

// An instance moved to a position (its place's position taken from its matrix first), queued to be stepped when it moved
void MoveInstance(InstanceContext* instance, const Vector4* position)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(position))
    {
        QueueObject(instance);
    }
}

// The second of the tied characters keeps the instance its object node's AgentRef2 names (the pair's marker) between the two
// hands: there, its matrix turned by the angle between its up axis and the way from one hand to the other (about the axis square
// to both), its axes made unit long and square again
void PlaceLinkMarker(CharacterAgent* character)
{
    constexpr f32 FarHand = 1000.0f;
    constexpr f32 Turned = Epsilon;

    auto* node = static_cast<ObjectNode*>(GetGameNode(NodesOf(character->instance), NodeObject));
    if (node == nullptr || CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        return;
    }

    InstanceContext* marker = AwakeAgentRef2(node);
    if (marker == nullptr)
    {
        return;
    }

    Vector4 own;
    Vector4 other;
    if (character->HandPoints(&own, &other) == 0 || !(Length(own) < FarHand) || !(Length(other) < FarHand))
    {
        return;
    }

    if (IsSpinning(AttackKind(character->link->Leader()->part)))
    {
        return;
    }

    ObjectPlace* place = marker->place;
    RotateAndTranslate(place);
    Matrix4x4 matrix = place->matrix;
    Vector4 direction = {other.x - own.x, other.y - own.y, other.z - own.z, 1.0f};
    Vector4 up = *RowOf(&matrix, 1);
    Scale(&direction, InverseLength(&direction, LengthEpsilon));
    Vector4 axis = Cross(up, direction, 1.0f);
    Scale(&up, InverseLength(&up, LengthEpsilon));
    if (axis.x * axis.x + axis.y * axis.y + axis.z * axis.z > Turned)
    {
        f32 inverse = InverseLength(&axis, LengthEpsilon);
        f32 cosine = up.x * direction.x + up.y * direction.y + up.z * direction.z;
        Scale(&axis, inverse);
        s32 angle;
        AngleOfCosine(cosine, &angle);
        Matrix4x4 turn;
        AxisAngleMatrix(&turn, &axis, &angle, 0);
        VuMultiplyMatrices(&matrix, &turn, &matrix);
    }

    Vector4 middle = Sum(own, other);
    Vector4* position = RowOf(&matrix, 3);
    position->x = middle.x * 0.5f;
    position->y = middle.y * 0.5f;
    position->z = middle.z * 0.5f;
    position->w = 1.0f;
    MoveInstance(marker, position);
    Vector4* side = RowOf(&matrix, 0);
    Vector4* top = RowOf(&matrix, 1);
    Vector4* front = RowOf(&matrix, 2);
    Scale(side, InverseLength(side, LengthEpsilon));
    Scale(top, InverseLength(top, LengthEpsilon));
    *front = Cross(*side, *top, 0.0f);
    *side = Cross(*top, *front, 0.0f);
    top->w = 0.0f;
    if (SetPlaceMatrix(marker->place, &matrix) != 0)
    {
        QueueObject(marker);
    }

    marker->flags.collisionActive = 0;
}
}

void CharacterAgent::CrouchFrame(TimeClock* clock, CharacterPart* part)
{
    if (crouch == nullptr || Linked() != 0)
    {
        part->moveBits.crouching = 0;
        part->moveBits.crawling = 0;
        return;
    }

    f32 stick = __builtin_sqrtf(buttons.moveZ * buttons.moveZ + buttons.moveX * buttons.moveX);
    u32 kind = AttackKind(part);
    crouch->Frame(buttons.circle, stick, buttons.turn, clock);
    u32 state = crouch->bits.state;
    kind = KindWhile(kind, state == CrouchController::StateSliding || state == CrouchController::StateSlideEnd, AttackWalkInto,
                     AttackSlide);
    kind = KindWhile(kind, crouch->bits.slideVariantKind != 0, AttackSlide, AttackSlideVariant);
    part->moveBits.crouching = crouch->bits.state != CrouchController::StateStanding;
    u32 crawling = crouch->bits.state == CrouchController::StateCrawling;
    AttackKind(part) = kind;
    part->moveBits.crawling = crawling;
}

void CharacterAgent::JumpFrame(TimeClock* clock, CharacterPart* part)
{
    if (jump == nullptr)
    {
        // Retail leaves the slide jump as it was
        f32 gravity = properties->GetFloat(JumpController::PropGravity);
        part->moveBits.jumping = 0;
        part->moveBits.doubleJump = 0;
        part->moveBits.flyingKick = 0;
        part->moveBits.unusedJump = 0;
        part->gravity = gravity;
        return;
    }

    u32 kind = AttackKind(part);
    u32 mayJump = crouch != nullptr ? crouch->bits.mayJump : 1;
    jump->Frame(buttons.cross, buttons.circle, clock, mayJump);
    u32 state = jump->bits.state;
    kind = KindWhile(kind, state == JumpController::StateKneeDropHang || state == JumpController::StateKneeDrop, AttackWalkInto,
                     AttackSlam);
    kind = KindWhile(kind, jump->bits.slamVariantKind != 0, AttackSlam, AttackSlamVariant);
    part->moveBits.jumping = jump->bits.state != JumpController::StateGrounded;
    state = jump->bits.state;
    bool doubleJump = state == JumpController::StateRisingDouble || state == JumpController::StateKneeDropHang
                      || state == JumpController::StateKneeDrop || state == JumpController::StateFallingDouble;
    part->moveBits.doubleJump = doubleJump;
    state = jump->bits.state;
    bool slideJump = state == JumpController::StateRisingSlide || state == JumpController::StateFallingSlide;
    part->moveBits.slideJump = slideJump;
    state = jump->bits.state;
    part->moveBits.flyingKick = state == JumpController::StateFlyingKick || state == JumpController::StateFallingKick;
    state = jump->bits.state;
    AttackKind(part) = kind;
    part->moveBits.unusedJump = state == JumpController::StateRisingUnused || state == JumpController::StateFallingUnused;
}

void CharacterAgent::SpinFrame(TimeClock* clock, CharacterPart* part)
{
    if (spin == nullptr)
    {
        part->moveBits.spinning = 0;
        return;
    }

    f32 stick = __builtin_sqrtf(buttons.moveZ * buttons.moveZ + buttons.moveX * buttons.moveX);
    u32 kind = AttackKind(part);
    // A character with a gun spins with circle (the second float is never read)
    if (gun == nullptr)
    {
        spin->Frame(buttons.square, buttons.circle, stick, clock);
    }
    else
    {
        spin->Frame(buttons.circle, 0.0f, stick, clock);
    }

    u32 state = spin->bits.state;
    AttackKind(part) =
        KindWhile(kind, state == SpinController::StateSpinningTied || state == SpinController::StateSpinning, AttackWalkInto,
                  AttackSpin);
}

void CharacterAgent::LinkFrame(TimeClock* clock, CharacterPart* part)
{
    constexpr f32 CircleAfter = 0.5f;
    constexpr f32 GroundProbe = 1.0f;
    // Thrown from a spin: put a step ahead (0.4 up; it must fit half a step ahead, 0.2 up), launched forward; from a jump: put
    // above (2.25 up; it must fit at 1.125 up), launched up
    constexpr Vector4 SpinThrowRaise = {0.0f, Rounded(0.4), 0.0f, 1.0f};
    constexpr Vector4 SpinThrowCheckRaise = {0.0f, Rounded(0.2), 0.0f, 1.0f};
    constexpr Vector4 SpinThrowLift = {0.0f, 10.5f, 0.0f, 1.0f};
    constexpr f32 SpinThrowSpeed = 33.0f;
    constexpr f32 SpinThrowStrength = 25.0f;
    constexpr Vector4 JumpThrowRaise = {0.0f, 2.25f, 0.0f, 1.0f};
    constexpr Vector4 JumpThrowCheckRaise = {0.0f, 1.125f, 0.0f, 1.0f};
    constexpr Vector4 JumpThrowLift = {0.0f, 16.0f, 0.0f, 1.0f};
    constexpr f32 JumpThrowSpeed = 19.0f;
    constexpr f32 JumpThrowStrength = 40.0f;

    if (link == nullptr)
    {
        // Thrown kinds stay while the walk is pushed (retail reads the walk without checking it's there)
        u32 kind = AttackKind(part);
        if ((kind == AttackThrownFromSpin || kind == AttackThrownFromJump) && walk->bits.state != WalkController::StatePushed)
        {
            AttackKind(part) = AttackWalkInto;
        }

        return;
    }

    f32 circle = 0.0f;
    bool spinThrow = false;
    bool jumpThrow = false;
    if (part->moveBits.linkedSecond != 0)
    {
        PlaceLinkMarker(this);
    }

    CharacterMoveBits bits = part->moveBits;
    if (bits.linkedFirst != 0)
    {
        f32 tied = static_cast<s32>(clock->time - linkTime) * g_SecondsPerClockUnit;
        circle = tied > CircleAfter ? buttons.circle : 0.0f;

        if (circle != 0.0f)
        {
            if (bits.jumping != 0)
            {
                jumpThrow = true;
            }
            else if (IsSpinning(AttackKind(part)))
            {
                spinThrow = true;
            }
            else
            {
                // The height above the ground, which retail hands the link's frame in $f13 and the frame never reads
                Vector4 position;
                Position(&position);
                Vector4 below = position;
                below.y = below.y - GroundProbe;
                Vector4 ground;
                GetCollisionCheck(instance->chunk, &position, &below, SurfaceFlags::SolidToPlayerProbes, nullptr, &ground,
                                  nullptr);
            }
        }
    }

    link->Frame(circle, clock);
    bool slamming = link->bits.state == CharacterLink::StateSlamming;
    AttackKind(part) = KindWhile(AttackKind(this->part), slamming, AttackWalkInto, AttackTied);
    CharacterAgent* second = link->Second();
    if (second == nullptr || (!jumpThrow && !spinThrow))
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    const Vector4* forward = RowOf(&place->matrix, 2);
    const Vector4* at = RowOf(&place->matrix, 3);
    Vector4 high;
    Vector4 low;
    Vector4 push;
    f32 strength;
    u32 event;
    if (spinThrow)
    {
        high = Sum(Sum(*at, *forward), SpinThrowRaise);
        low = Sum(Sum(*at, Scaled(*forward, 0.5f)), SpinThrowCheckRaise);
        push = Sum(Scaled(*forward, SpinThrowSpeed), SpinThrowLift);
        strength = SpinThrowStrength;
        event = EventThrownFromSpin;
    }
    else
    {
        high = Sum(*at, JumpThrowRaise);
        low = Sum(*at, JumpThrowCheckRaise);
        push = Sum(Scaled(*forward, JumpThrowSpeed), JumpThrowLift);
        strength = JumpThrowStrength;
        event = EventThrownFromJump;
    }

    walk->BeIdle();
    if (second->FitsAt(FitStanding, 0, nullptr, &high) == 0 || second->FitsAt(FitStanding, 0, nullptr, &low) == 0)
    {
        return;
    }

    Unlink();
    MoveInstance(second->instance, &high);
    ObjectPlace* secondPlace = second->instance->place;
    RotateAndTranslate(secondPlace);
    Matrix4x4 toSecond = secondPlace->matrix;
    VuInvertRigidInPlace(&toSecond);
    VuRotateVector(&toSecond, &push, &push);
    CallVirtual<void>(second, second->vtable, Agent::LaunchSlot, strength, &push, instance);
    static_cast<CharacterPart*>(second->part)->flags.onGround = 0;
    second->state.standing = StandingNothing;
    if (second->standingOn != nullptr && second->standingOn->object != nullptr)
    {
        RemoveReference(&second->standingOn);
        second->standingOn = nullptr;
    }

    second->standingStamp = 0;
    second->standingHull = -1;
    RunAgentEvent(this, event, 0, 0, 0);
    RunAgentEvent(second, event, reinterpret_cast<u32>(instance), 0, 0);
    u32 kind = KindWhile(AttackKind(second->part), spinThrow, AttackWalkInto, AttackThrownFromSpin);
    AttackKind(second->part) = KindWhile(kind, jumpThrow, AttackWalkInto, AttackThrownFromJump);
    static_cast<CharacterPart*>(second->part)->bits.invulnerable = 1;
}

void CharacterAgent::WalkFrame(TimeClock* clock, CharacterPart* part)
{
    if (walk == nullptr)
    {
        part->moveBits.walking = 0;
        part->moveBits.running = 0;
        part->moveBits.strafing = 0;
        return;
    }

    Vector4 stick;
    f32 strafe;
    f32 turn;
    if (state.dead != 0)
    {
        stick = g_DefaultBox.min;
        stick.w = 1.0f;
        turn = 0.0f;
        strafe = 0.0f;
    }
    else
    {
        // The shoulder buttons strafe while the character stands
        strafe = 0.0f;
        if (walk->Strafes() != 0
            && (crouch == nullptr || crouch->bits.state == CrouchController::StateStanding))
        {
            strafe = buttons.shoulders;
        }

        stick = {buttons.moveZ, 0.0f, buttons.moveX, 1.0f};
        turn = buttons.turn;
    }

    walk->Frame(strafe, turn, clock, &stick);
    part->moveBits.walking = walk->bits.state == WalkController::StateWalking;
    part->moveBits.running = walk->bits.state == WalkController::StateRunning;
    part->moveBits.strafing = walk->bits.strafeHeld != 0;
}

void CharacterAgent::LookFrame(TimeClock* clock)
{
    if (look == nullptr)
    {
        return;
    }

    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    Vector4 point = {0.0f, 0.0f, 0.0f, 1.0f};
    u32 hasPoint = 0;
    HeadTracking* tracking = node->headTracking;
    if (tracking != nullptr && !tracking->flags.ignoredByLook)
    {
        InstanceContext* target =
            tracking->target != nullptr ? static_cast<InstanceContext*>(tracking->target->object) : nullptr;
        if (target != nullptr)
        {
            // A character's head, else the middle of the instance's box
            CharacterAgent* character = CharacterAgentOf(target);
            if (character != nullptr)
            {
                auto* model = static_cast<ModelNode*>(GetGameNode(&character->instance->nodes, NodeModel));
                SizedArray<ExitPointAnimation*>* exitPoints = model->animator->exitPoints;
                ExitPointAnimation* head = exitPoints != nullptr ? exitPoints->data[ExitPointHead] : nullptr;
                hasPoint = 1;
                point = *RowOf(&UpdateExitPointMatrix(head)->matrix, 3);
            }
            else
            {
                point = MiddleOf(target->CollisionBox());
                hasPoint = 1;
            }
        }
    }

    look->SetLookPoint(&point, hasPoint);
    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, NodeFollow));
    auto* character = static_cast<CharacterPart*>(part);
    f32 lookX = 0.0f;
    f32 lookY = 0.0f;
    if (follow != nullptr)
    {
        // The joints look along the camera's stick but while crouching, body slamming or riding what says no
        bool follows = character->moveBits.crouching == 0;
        u32 kind = AttackKind(character);
        if (kind == AttackSlam || kind == AttackSlamVariant)
        {
            follows = false;
        }

        if (vehicle != nullptr && vehicle->HeadFollowsCameraVirtual() == 0)
        {
            follows = false;
        }

        CameraRig* rig = follow->camera.lensRig;
        if (follows && rig != nullptr)
        {
            CallVirtual<void>(rig, rig->vtable, PadCameraRig::LookStickSlot, &lookX, &lookY);
        }
    }

    look->freeLookX = lookX;
    look->freeLookY = lookY;
    look->Frame(clock);
}

void CharacterAgent::LinkedHits()
{
    // A box at the hand the second of the tied characters slams with
    constexpr Vector4 HandMin = {Rounded(-0.4), Rounded(-0.65), Rounded(-0.4), 1.0f};
    constexpr Vector4 HandMax = {Rounded(0.4), 0.0f, Rounded(0.8), 1.0f};

    if (static_cast<CharacterPart*>(part)->moveBits.linkedSecond == 0 || link->MidSlam() == 0)
    {
        return;
    }

    void* results[MostHits];
    InstanceQuery query;
    StartQuery(&query, results, MostHits, 0);
    ChunkData* chunk = instance->chunk;
    OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel))->animator;
    ExitPointAnimation* hand = animator->exitPoints != nullptr ? animator->exitPoints->data[ExitPointHand] : nullptr;
    ExitPointAnimation* placed = UpdateExitPointMatrix(hand);
    if (g_BoxHullCache == nullptr)
    {
        g_BoxHullCache = ConstructBoxHullCache(static_cast<BoxHullCache*>(MemoryAllocate(sizeof(BoxHullCache))));
    }

    Vector4 min = HandMin;
    Vector4 max = HandMax;
    CollisionHull* hull = BoxHullOf(g_BoxHullCache, &min, &max);
    if (ChunkInstancesInHull(chunk, hull, &placed->matrix, DamageableNodeKinds, &query, 0) != 0)
    {
        TouchQuery(&query);
    }
}

void CharacterAgent::AttackHits()
{
    auto* character = static_cast<CharacterPart*>(part);
    if (!IsSpinning(AttackKind(character)) && AttackKind(character) != AttackThrownFromSpin)
    {
        return;
    }

    void* results[MostHits];
    InstanceQuery query;
    StartQuery(&query, results, MostHits, 0);
    ObjectPlace* place = instance->place;
    ChunkData* chunk = instance->chunk;
    RotateAndTranslate(place);
    CharacterMoveBits bits = character->moveBits;
    if (bits.linkedFirst != 0)
    {
        if (ChunkInstancesInHull(chunk, linkedAttackHull, &place->matrix, AttackHitKinds, &query, 0) != 0)
        {
            TouchQuery(&query);
        }

        return;
    }

    if (bits.linkedSecond != 0)
    {
        return;
    }

    s32 attacked = ChunkInstancesInHull(chunk, attackHull, &place->matrix, AttackHitKinds, &query, 0);
    s32 touched = ChunkInstancesInHull(chunk, &body->standingHull, &place->matrix, AttackHitKinds, &query, 0);
    if (attacked > 0 || touched > 0)
    {
        TouchQuery(&query);
    }
}

void CharacterAgent::TouchHits(TimeClock* clock)
{
    constexpr f32 TouchRadius = 1.0f;
    constexpr f32 AttractRadius = 1.5f;

    Vector4 sphere = MiddleOf(instance->CollisionBox());
    u32 kind = AttackKind(part);
    void* results[MostHits];
    InstanceQuery query;
    if (!IsSpinning(kind))
    {
        StartQuery(&query, results, MostHits, 0);
        sphere.w = TouchRadius;
        if (ChunkInstancesInSphere(instance->chunk, &sphere, PickupKinds, &query, 1) != 0)
        {
            TouchQuery(&query);
        }
    }

    if (IsPlayer() == 0)
    {
        return;
    }

    StartQuery(&query, results, MostHits, 0);
    sphere.w = AttractRadius;
    if (ChunkInstancesInSphere(instance->chunk, &sphere, PickupKinds, &query, 1) != 0)
    {
        AttractPickups(clock, &query);
    }
}

void CharacterAgent::Splash()
{
    Vector4 centre;
    f32 radius;
    if (vehicle != nullptr)
    {
        vehicle->SplashSphereVirtual(&centre, &radius);
    }
    else
    {
        Position(&centre);
        centre.y = centre.y + CharacterSplashRaise;
        radius = CharacterSplashRadius;
    }

    f32 point[3] = {centre.x, centre.y, centre.z};
    if (TestParticleCollision(point, radius) < 0)
    {
        return;
    }

    ObjectPlace* place = instance->place;
    place->SyncPosition();
    ContactMessage message;
    message.point = place->position;
    message.hitKinds = SplashKinds;
    message.damage = SplashDamage;
    message.point.w = 0.0f;
    CallVirtual<void>(this, vtable, ContactSlot, &message, instance, 1u);
}

void CharacterAgent::TellTrigger(FollowCamera* camera, InstanceContext* trigger, u32 entered)
{
    auto* node = static_cast<MessageTriggerNode*>(GetGameNode(NodesOf(trigger), NodeMessageTrigger));
    auto* cameraNode = static_cast<CameraNode*>(GetGameNode(NodesOf(trigger), NodeCameraTrigger));
    if (node != nullptr)
    {
        // Only the triggers that aren't polled are checked by the characters inside
        if (node->bits.neverPolled == 0)
        {
            return;
        }

        if (node->messageBits.anyCharacter == 0 && instance != PlayerInstance())
        {
            return;
        }

        node->CheckWith(entered != 0 ? instance : nullptr);
        return;
    }

    if (cameraNode == nullptr || entered == 0 || cameraNode->bits.neverPolled == 0)
    {
        return;
    }

    OfferCamera(camera, cameraNode, cameraNode->bits.kind, this);
}

void CharacterAgent::CheckTriggers()
{
    auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, NodeFollow));
    u32 played = g_AgentsGameController->progress.play.character;
    s32 character = properties->GetInt(CharacterKindProperty);
    // A character of no kind only checks them while it's the one played
    if (follow == nullptr || (character == CharacterNone && played != CharacterNone))
    {
        return;
    }

    // Both sets are sorted by the objects' addresses, so they're walked side by side: a trigger in both is told it's entered
    // again, one only in the old set that it's left, one only in the new set that it's entered
    ReferenceSet now;
    GatherTriggerInstances(&now, instance->chunk, g_TriggerNodeKinds, instance);
    FollowCamera* camera = &follow->camera;
    HandleWalk fresh = {g_HandleWalkVTable, 0, &now};
    HandleWalk old = {g_HandleWalkVTable, 0, &triggers};
    TakeChosenCamera(camera);
    while (!old.AtEnd() && !fresh.AtEnd())
    {
        auto* was = static_cast<InstanceContext*>(old.Object());
        auto* is = static_cast<InstanceContext*>(fresh.Object());
        if (was == nullptr)
        {
            old.Step();
        }
        else if (is == nullptr)
        {
            // Retail goes round again without stepping, for good (the new set never has an empty handle)
        }
        else if (was == is)
        {
            TellTrigger(camera, is, 1);
            old.Step();
            fresh.Step();
        }
        else if (reinterpret_cast<u32>(was) < reinterpret_cast<u32>(is))
        {
            TellTrigger(camera, was, 0);
            old.Step();
        }
        else
        {
            TellTrigger(camera, is, 1);
            fresh.Step();
        }
    }

    while (!old.AtEnd())
    {
        auto* was = static_cast<InstanceContext*>(old.Object());
        if (was != nullptr)
        {
            TellTrigger(camera, was, 0);
        }

        old.Step();
    }

    while (!fresh.AtEnd())
    {
        TellTrigger(camera, static_cast<InstanceContext*>(fresh.Object()), 1);
        fresh.Step();
    }

    triggers.Assign(&now);
    fresh.vtable = g_HandleWalkBaseVTable;
    old.vtable = g_HandleWalkBaseVTable;
    now.Destroy(DestroyOnly);
}

void CharacterAgent::HurtFrame(TimeClock* clock)
{
    constexpr f32 HurtSeconds = Rounded(0.3);
    constexpr f32 BlinkSeconds = Rounded(0.2);
    constexpr f32 BlinksPerSecond = 5.0f;
    constexpr f32 ShownShare = Rounded(0.8);
    constexpr u32 SteadyHitPoints = 3;

    u32 mode = ModeHurt;
    if (state.mode != ModeHurt)
    {
        modeStart = clock->time;
        state.previousMode = state.mode;
        state.mode = ModeHurt;
    }

    // Hurt after invincibility: it blinks (unless it has 3 hit points or more) until the mode's time is over
    if (state.previousMode != ModeInvincible)
    {
        return;
    }

    auto* character = static_cast<CharacterPart*>(part);
    s32 elapsed = clock->time - modeStart;
    if (character->moveBits.hurt != 0 && elapsed >= static_cast<s32>(g_ClockUnitsPerSecond * HurtSeconds))
    {
        character->moveBits.hurt = 0;
    }

    if (elapsed >= modeTicks)
    {
        instance->flags.visible = 1;
        mode = ModeNone;
        character->bits.invulnerable = 0;
    }
    else
    {
        u32 hitPoints = character->flags.hitPoints;
        if (hitPoints < SteadyHitPoints)
        {
            s32 period = static_cast<s32>(g_ClockUnitsPerSecond * BlinkSeconds);
            s32 into = elapsed;
            while (into >= period)
            {
                into -= period;
            }

            if (static_cast<f32>(into) * g_SecondsPerClockUnit * BlinksPerSecond <= ShownShare)
            {
                instance->flags.visible = 1;
            }
            else
            {
                instance->flags.visible = 0;
            }
        }
        else
        {
            instance->flags.visible = 1;
        }
    }

    if (mode != ModeHurt)
    {
        state.mode = mode;
    }
}

void CharacterAgent::FindGroundPoint()
{
    constexpr Vector4 Cast = {0.0f, -8.0f, 0.0f, 1.0f};
    constexpr f32 CastRaise = 0.25f;
    constexpr f32 AboveGround = Rounded(0.02);

    void* results[MostGround];
    InstanceQuery query;
    StartQuery(&query, results, MostGround, ReferencedObjectFlags::CollisionActive);
    Vector4 way = Cast;
    SkipInQuery(&query, instance);
    Position(&groundPoint);
    groundPoint.y = groundPoint.y + CastRaise;
    u32 found = LineOfSight(&groundPoint, &way, SurfaceFlags::SolidToPlayerProbes, &query, SolidOrProjectileNodeKinds);
    groundPoint.x = groundPoint.x + way.x;
    state.groundFound = found;
    groundPoint.y = groundPoint.y + way.y + AboveGround;
    groundPoint.z = groundPoint.z + way.z;
}

void CharacterAgent::LiftOntoGround(f32 height, f32 reach)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    position.y = position.y + heightOffset;
    InstanceContext* leftOut = instance;
    Vector4 from = position;
    from.y = from.y + height;
    Vector4 found;
    if (CastHullDown(height + reach, instance->chunk, &body->standingHull, &from, SurfaceFlags::SolidToPlayerProbes,
                     SolidOrProjectileNodeKinds, &found, &leftOut, 1)
        == 0)
    {
        static_cast<CharacterPart*>(part)->flags.onGround = 0;
        return;
    }

    heightOffset = 0.0f;
    MoveInstance(instance, &found);
    static_cast<CharacterPart*>(part)->flags.onGround = 1;
}

void CharacterAgent::RefreshCache()
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    // The box around its place the cache holds the collision of: so far across, above and below (the Mecha-Bandicoot's bigger)
    constexpr f32 CacheAcross = 2.0f;
    constexpr f32 CacheAbove = 3.0f;
    constexpr f32 CacheBelow = 1.0f;
    constexpr f32 MechaCacheAcross = 12.0f;
    constexpr f32 MechaCacheAbove = 13.0f;
    constexpr f32 MechaCacheBelow = 3.0f;

    Box box = {*RowOf(&place->matrix, 3), *RowOf(&place->matrix, 3)};
    f32 across;
    f32 above;
    f32 below;
    if (properties->GetInt(CharacterKindProperty) == CharacterMecha)
    {
        across = MechaCacheAcross;
        above = MechaCacheAbove;
        below = MechaCacheBelow;
    }
    else
    {
        across = CacheAcross;
        above = CacheAbove;
        below = CacheBelow;
    }

    box.min.x = box.min.x - across;
    box.min.y = box.min.y - below;
    box.min.z = box.min.z - across;
    box.max.x = box.max.x + across;
    box.max.y = box.max.y + above;
    box.max.z = box.max.z + across;
    RefreshCollisionCache(&cache, &box);
}

// Each of the instances its box touches against each of its own hulls: the hulls whose boxes and shapes meet attack the
// instance (retail has room for 24 hulls of its own and never checks)
void CharacterAgent::HullHits()
{
    void* results[MostHullHits];
    InstanceQuery query;
    StartQuery(&query, results, MostHullHits, ReferencedObjectFlags::CollisionActive);
    Box box = *instance->CollisionBox();
    SkipInQuery(&query, instance);
    s32 count = QueryChunkInstances(instance->chunk, &box, SolidNodeKinds, &query);
    if (count > 0)
    {
        CollisionHull* hulls[HullsMost];
        Matrix4x4 matrices[HullsMost];
        Box boxes[HullsMost];
        ObjectCollision* collision = &instance->collision;
        for (s32 index = 0; index < GetHullCount(collision); index++)
        {
            GetInstanceHull(collision, index, &hulls[index], &matrices[index]);
            GetHullBoundingBox(hulls[index], &matrices[index], &boxes[index]);
        }

        for (s32 index = 0; index < count; index++)
        {
            auto* other = static_cast<InstanceContext*>(query.results[index]);
            auto* otherCollision = reinterpret_cast<ObjectCollision*>(reinterpret_cast<std::uintptr_t>(other)
                                                                       + offsetof(InstanceContext, collision));
            for (s32 hullIndex = 0; hullIndex < GetHullCount(otherCollision); hullIndex++)
            {
                CollisionHull* otherHull;
                Matrix4x4 otherMatrix;
                Box otherBox;
                GetInstanceHull(otherCollision, hullIndex, &otherHull, &otherMatrix);
                GetHullBoundingBox(otherHull, &otherMatrix, &otherBox);
                if (BoxesOverlap(&otherBox, &box) == 0)
                {
                    continue;
                }

                for (s32 own = 0; own < GetHullCount(collision); own++)
                {
                    if (BoxesOverlap(&otherBox, &boxes[own]) == 0
                        || HullsTouch(otherHull, &otherMatrix, hulls[own], &matrices[own]) == 0)
                    {
                        continue;
                    }

                    // QueueAttack's steps, the node kinds worked out after the event's memory
                    u32 kind = own == KickHull ? AttackSpin : AttackWalkInto;
                    Reference* attacker = instance != nullptr ? AddReference(instance) : nullptr;
                    void* memory = MemoryAllocate(sizeof(AttackEvent));
                    u32 kinds = KindOf(other);
                    AttackEvent* event = AttackEvent::Construct(static_cast<AttackEvent*>(memory), kind, &attacker, kinds);
                    Reference* handle = event != nullptr ? AddEventReference(event) : nullptr;
                    QueueEvent(other, &handle);
                }
            }
        }
    }

    if (gun != nullptr)
    {
        Vector4 aim;
        u32 aims = gun->AimPoint(&aim) != 0;
        look->SetAimPoint(&aim, aims);
    }
}

void CharacterAgent::Frame(TimeClock* clock)
{
    constexpr f32 TopSpeed = 60.0f;

    // The frame after its state was applied it's lifted onto the ground under it
    if (state.previousMode == ModeApplied && properties->GetInt(CharacterKindProperty) != CharacterNone && body != nullptr)
    {
        LiftOntoGround(CharacterLiftHeight, CharacterLiftReach);
    }

    if (crushCount > 0)
    {
        crushCount--;
    }

    RefreshCache();
    auto* character = static_cast<CharacterPart*>(part);
    u32 onGround = character->flags.onGround != 0;
    bool frozen = character->bits.frozen != 0;
    if (properties->GetInt(CharacterKindProperty) == CharacterMecha)
    {
        HullHits();
    }

    f32 seconds = static_cast<s32>(clock->advance) * g_SecondsPerClockUnit;
    CharacterState bits = state;
    if (bits.dead != 0)
    {
        // Dead: moved (unless drowned) or ridden, its joints and its look relaxing
        if (vehicle == nullptr)
        {
            if ((contact.hitKinds & HitWater) == 0)
            {
                Move(seconds, character, onGround);
            }
        }
        else
        {
            vehicle->FrameVirtual(seconds);
        }

        if (proceduralJoints != nullptr)
        {
            proceduralJoints->Frame(clock);
        }

        if (look != nullptr)
        {
            look->Relax(clock);
        }

        HurtFrame(clock);
        if (vehicle == nullptr)
        {
            CreatureAgent::Frame(clock);
            LinkFrame(clock, character);
        }

        buttons.moveX = 0.0f;
        buttons.turn = 0.0f;
        buttons.moveZ = 0.0f;
        character->ClearSpeedRequests();
        character->ClearTurnRequest();
        character->push = g_DefaultBox.min;
        character->push.w = 1.0f;
        CheckTriggers();
        return;
    }

    if (bits.boxOnly != 0)
    {
        MakeBoxOfExitPoints();
        CheckTriggers();
        HurtFrame(clock);
        return;
    }

    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    bool scripted = node->flags.moves;
    if (frozen || !instance->flags.collisionActive || scripted)
    {
        if (proceduralJoints != nullptr)
        {
            proceduralJoints->Frame(clock);
        }

        HurtFrame(clock);
        CheckTriggers();
        return;
    }

    character->ClearTurnRequest();
    if (claw == nullptr)
    {
        character->moveBits.clawing = 0;
    }
    else
    {
        claw->Frame(buttons.circle, buttons.cross, clock);
        // Not clawing while ready, coming back (never entered) and after a swipe
        u32 clawState = claw->bits.state;
        bool clawing = clawState != ClawController::StateReady && clawState != ClawController::StateReturning
                       && clawState != ClawController::StateAfterSwipe;
        character->moveBits.clawing = clawing;
    }

    if (gun == nullptr)
    {
        character->moveBits.shooting = 0;
    }
    else
    {
        // No shooting during the radial blast, and no charging while it's blasting or strafing
        u32 jumpState = jump != nullptr ? jump->bits.state : JumpController::StateGrounded;
        bool blasting = jumpState >= JumpController::StateBlastHang && jumpState <= JumpController::StateBlastFalling;
        f32 square = blasting ? 0.0f : buttons.square;
        bool strafing = walk != nullptr && walk->bits.strafeHeld != 0;
        gun->Frame(square, blasting || strafing ? 0.0f : 1.0f, clock);
        character->moveBits.shooting = gun->bits.state != Gun::StatePutAway;
    }

    WalkFrame(clock, character);
    JumpFrame(clock, character);
    CrouchFrame(clock, character);
    SpinFrame(clock, character);
    if (vehicle == nullptr)
    {
        Move(seconds, character, onGround);
    }
    else
    {
        vehicle->FrameVirtual(static_cast<s32>(clock->advance) * g_SecondsPerClockUnit);
    }

    if (proceduralJoints != nullptr)
    {
        proceduralJoints->Frame(clock);
    }

    LookFrame(clock);
    LinkedHits();
    AttackHits();
    TouchHits(clock);
    Splash();
    HurtFrame(clock);
    if (vehicle == nullptr)
    {
        CreatureAgent::Frame(clock);
        LinkFrame(clock, character);
    }

    FindGroundPoint();
    buttons.moveX = 0.0f;
    buttons.turn = 0.0f;
    buttons.moveZ = 0.0f;
    character->ClearSpeedRequests();
    character->push = g_DefaultBox.min;
    character->push.w = 1.0f;
    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z > TopSpeed * TopSpeed)
    {
        f32 inverse = InverseLength(&velocity, LengthEpsilon);
        Scale(&velocity, inverse);
        Scale(&velocity, TopSpeed);
    }

    CheckTriggers();
}

u32 CharacterAgent::IsPlayer()
{
    return instance == PlayerInstance();
}

void CharacterAgent::AttractPickups(TimeClock* clock, const InstanceQuery* query)
{
    for (u32 index = 0; index < query->count; index++)
    {
        auto* found = static_cast<InstanceContext*>(query->results[index]);
        auto* node = static_cast<ObjectNode*>(GetGameNode(NodesOf(found), NodeObject));
        if (CallVirtual<u32>(node, node->vtable, ObjectNode::CodeModelKindSlot) != PickupCodeModel)
        {
            RunAgentEvent(node->agent, NearPlayerEvent, 0, 0, 0);
            continue;
        }

        // A pickup flies to the character
        node->focusInstance = instance;
        if (instance != nullptr)
        {
            node->flags.focusInstance = 1;
        }

        node->flags.focusPosition = 0;
        SetPickupState(node, clock, PickupFliesToFocus);
    }
}

void CharacterAgent::TouchedNothing(InstanceContext* other)
{
    Vector4 normal = {0.0f, 0.0f, 0.0f, 1.0f};
    CallVirtual<void>(this, vtable, TouchedSlot, other, &normal);
}

void CharacterAgent::TouchQuery(const InstanceQuery* query)
{
    Vector4 normal = {0.0f, 0.0f, 0.0f, 1.0f};
    for (u32 index = 0; index < query->count; index++)
    {
        CallVirtual<void>(this, vtable, TouchedSlot, static_cast<InstanceContext*>(query->results[index]), &normal);
    }
}
