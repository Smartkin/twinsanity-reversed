#include "game/agents.h"

#include "game/animation.h"
#include "game/characters.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"

#include <cstddef>
#include <cstdint>

EABI_EXPORT(FUN_0012e328, &CharacterAgent::MeasureVelocity);
EABI_EXPORT(FUN_00136168, &CharacterAgent::EaseVelocity);
EABI_EXPORT(FUN_00140830, &CharacterAgent::ApplyGravity);
EABI_EXPORT(FUN_0012fd08, &CharacterAgent::MoveBy);
EABI_EXPORT(FUN_00130268, &CharacterAgent::Move);
EABI_EXPORT(FUN_00134780, &CharacterAgent::PushBodies);

namespace
{
// The pushed body's spin when a spin kicks it, the kick's cooldown and the height its point is above the character's position
constexpr Vector4 KickSpin = {0.0f, -1.0f, 0.0f, 1.0f};
constexpr f32 KickSeconds = Rounded(0.8);
constexpr f32 KickHeight = Rounded(0.8);

static_assert(offsetof(CharacterLink, swingPosition) == 0x1F0);
static_assert(offsetof(CharacterAgent, heightState) == 0x270);
static_assert(offsetof(CharacterAgent, pushedBody) == 0x290);
static_assert(offsetof(CharacterAgent, probePush) == 0x2A0);
static_assert(offsetof(CharacterAgent, cache) == 0x2B0);
static_assert(offsetof(DynamicBody, instance) == 0x2E4);
static_assert(offsetof(SphereBody, radius) == 0x380);
static_assert(offsetof(RigidBody, angularMomentum) == 0x90);
static_assert(offsetof(RigidBody, velocity) == 0x150);

InstanceContext* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? static_cast<InstanceContext*>(handle->object) : nullptr;
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

DynamicBody* BodyOf(InstanceContext* instance)
{
    return static_cast<DynamicBody*>(GetGameNode(NodesOf(instance), NodeRigidBody));
}

OgiAnimator* AnimatorOf(InstanceContext* instance)
{
    return static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel))->animator;
}

// An animator's exit point (none without its array)
ExitPointAnimation* ExitPointOf(const OgiAnimator* animator, u32 index)
{
    return animator->exitPoints != nullptr ? animator->exitPoints->data[index] : nullptr;
}

// An exit point's place as retail reads it, also without an exit point (the word at 0x44)
void* PlaceOf(const ExitPointAnimation* exitPoint)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(exitPoint) + offsetof(ExitPointAnimation, place);
    return *reinterpret_cast<void* const*>(address);
}

bool IsSphere(DynamicBody* body)
{
    return CallVirtual<u32>(body, body->vtable, DynamicBody::IsSphereSlot) != 0;
}

f32 Length(const Vector4* vector)
{
    return __builtin_sqrtf(vector->x * vector->x + vector->y * vector->y + vector->z * vector->z);
}

void AddTo(Vector4* vector, const Vector4* add)
{
    vector->x += add->x;
    vector->y += add->y;
    vector->z += add->z;
}

bool Spinning(const CharacterPart* part)
{
    u32 kind = part->bits.attackKind;
    return kind == AttackSpin || kind == AttackSpinVariant;
}

bool Sliding(const CharacterPart* part)
{
    u32 kind = part->bits.attackKind;
    return kind == AttackSlide || kind == AttackSlideVariant;
}
}

void CharacterAgent::SetHeightState(s32 newState)
{
    if (heightState == HeightOnGround)
    {
        if (newState == HeightJumping)
        {
            ChangeHeight(-keptHeightOffset);
            keptHeightOffset = 0.0f;
            heightStateTime = 0.0f;
        }
    }
    else if (heightState == HeightJumping && newState == HeightOnGround)
    {
        keptHeightOffset = heightOffset;
    }

    heightState = newState;
}

void CharacterAgent::ApplyGravity(f32 seconds)
{
    auto* character = static_cast<CharacterPart*>(part);
    if (character->flags.onGround != 0 || character->flags.resting == 0)
    {
        return;
    }

    f32 pull = character->gravity * seconds;
    velocity.x += g_AgentDown.x * pull;
    velocity.y += g_AgentDown.y * pull;
    velocity.z += g_AgentDown.z * pull;
}

CollisionSurface* CharacterAgent::StandingSurface()
{
    u32 standing = state.standing;
    if (standing == StandingGround)
    {
        return GetTriangleSurface(&groundHit);
    }

    if (standing == StandingHull || standing == StandingRidden)
    {
        InstanceContext* other = ObjectOf(standingOn);
        if (other == nullptr)
        {
            state.standing = StandingNothing;
            return nullptr;
        }

        // The stamp is the OGI the instance's collision had: another one's hulls fall back to the default surface
        if (reinterpret_cast<std::uintptr_t>(other->collision.ogi) != standingStamp)
        {
            return &g_CollisionSurfaces.surfaces[0];
        }

        return &g_CollisionSurfaces.surfaces[HullSurfaceIndex(&other->collision, static_cast<u8>(standingHull))];
    }

    if (static_cast<CharacterPart*>(part)->flags.onGround != 0)
    {
        return &g_CollisionSurfaces.surfaces[0];
    }

    return nullptr;
}

void CharacterAgent::LetGoOfStanding()
{
    u32 standing = state.standing;
    if (standing != StandingHull && standing != StandingRidden)
    {
        return;
    }

    state.standing = StandingNothing;
    AssignReference(&standingOn, nullptr);
    standingStamp = 0;
    floorSurface = nullptr;
    static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject))->surface = -1;
    if (static_cast<CharacterPart*>(part)->moveBits.linkedFirst != 0)
    {
        link->Second()->SetFloorSurface(nullptr);
    }
}

void CharacterAgent::MeasureVelocity(f32 vertical, u32 measured)
{
    if (measured != 0)
    {
        auto* movement = static_cast<MovementNode*>(GetGameNode(&instance->nodes, NodeMovement));
        Vector4 last = lastPosition;
        Vector4 position;
        Position(&position);
        Vector4 moved = {position.x - last.x, position.y - last.y, position.z - last.z, 1.0f};
        f32 inverse = 1.0f / movement->Seconds();
        velocity.x = moved.x * inverse;
        velocity.z = moved.z * inverse;
        InstanceContext* standing = ObjectOf(standingOn);
        if (standing != nullptr && state.standing == StandingRidden)
        {
            // Retail fetches the ridden instance's move and does nothing with it
            auto* ridden = static_cast<MovementNode*>(GetGameNode(&standing->nodes, NodeMovement));
            if (ridden != nullptr)
            {
                MovementVelocity(ridden, &moved);
            }
        }

        // The boost is cleared before it's added: a measured velocity goes up by the value alone
        verticalBoost = 0.0f;
    }

    velocity.y = verticalBoost + vertical;
}

void CharacterAgent::EaseVelocity(f32 seconds, const Vector4* wanted)
{
    // In the air: 10 a second (and 40 by the last floor's friction), less above a speed of 30, none without a walk, unbounded
    // while the walk is pushed
    constexpr f32 AirRate = 10.0f;
    constexpr f32 FrictionRate = 40.0f;
    constexpr f32 FastSpeed = 30.0f;
    constexpr f32 FastRate = 1500.0f;
    constexpr f32 Unbounded = Infinite;
    constexpr f32 GroundFall = Rounded(-0.01);

    CollisionSurface* surface = StandingSurface();
    if (surface != nullptr)
    {
        floorSurface = surface;
        static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject))->surface = surface->surfaceId;
        if (static_cast<CharacterPart*>(part)->moveBits.linkedFirst != 0)
        {
            link->Second()->SetFloorSurface(surface);
        }

        AccelerateOnSurface(surface, seconds, &velocity, wanted, &groundNormal);
        velocity.y = GroundFall;
        return;
    }

    f32 rate = AirRate;
    if (floorSurface != nullptr)
    {
        rate = floorSurface->friction * FrictionRate + AirRate;
    }

    f32 speed = Length(&velocity);
    if (FastSpeed < speed)
    {
        f32 slower = FastRate / speed;
        if (slower < rate)
        {
            rate = slower;
        }
    }

    auto* character = static_cast<CharacterPart*>(part);
    if (walk == nullptr)
    {
        rate = 0.0f;
    }
    else if (walk->bits.state == WalkController::StatePushed)
    {
        rate = Unbounded;
    }
    else if (character->moveBits.scaleRequested != 0)
    {
        rate *= character->speedScale;
    }

    f32 most = seconds * rate;
    Vector4 change = {wanted->x - velocity.x, 0.0f, wanted->z - velocity.z, 1.0f};
    if (most * most < change.x * change.x + change.z * change.z)
    {
        f32 inverse = InverseLength(&change, LengthEpsilon);
        change.x = change.x * inverse * most;
        change.y = change.y * inverse * most;
        change.z = change.z * inverse * most;
    }

    velocity.x += change.x;
    velocity.z += change.z;
}

f32 CharacterAgent::ExitPointsHeight()
{
    // The lowest of the feet (counted 0.45 along their z axes), exit point 5 (where the invincibility's trails come from) and the
    // hand
    constexpr f32 Reach = 0.45f;
    constexpr u32 TrailExitPoint = 5;

    OgiAnimator* animator = AnimatorOf(instance);
    ExitPointAnimation* exitPoint = UpdateExitPointMatrix(ExitPointOf(animator, ExitPointLeftFoot));
    ExitPointAnimation* turned = UpdateExitPointMatrix(ExitPointOf(animator, ExitPointLeftFoot));
    f32 lowest = exitPoint->matrix.m[3][1] + turned->matrix.m[2][1] * Reach;
    exitPoint = UpdateExitPointMatrix(ExitPointOf(animator, ExitPointRightFoot));
    turned = UpdateExitPointMatrix(ExitPointOf(animator, ExitPointRightFoot));
    f32 height = exitPoint->matrix.m[3][1] + turned->matrix.m[2][1] * Reach;
    if (height < lowest)
    {
        lowest = height;
    }

    height = UpdateExitPointMatrix(ExitPointOf(animator, TrailExitPoint))->matrix.m[3][1];
    if (height < lowest)
    {
        lowest = height;
    }

    height = UpdateExitPointMatrix(ExitPointOf(animator, ExitPointHand))->matrix.m[3][1];
    if (height < lowest)
    {
        lowest = height;
    }

    Vector4 position;
    PlacePosition(&position);
    lowest -= position.y;
    return lowest - HeightOffset();
}

void CharacterAgent::MakeBoxOfExitPoints()
{
    // Its exit points 0 to 10 (none for the Mecha-Bandicoot)
    constexpr u32 BoxExitPoints = 11;

    if (properties->GetInt(CharacterKindProperty) == CharacterMecha)
    {
        return;
    }

    OgiAnimator* animator = AnimatorOf(instance);
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Matrix4x4 toLocal = place->matrix;
    VuInvertRigidInPlace(&toLocal);
    Box box;
    ResetBox(&box);
    bool found = false;
    for (u32 i = 0; i < BoxExitPoints; i++)
    {
        ExitPointAnimation* exitPoint = ExitPointOf(animator, i);
        if (PlaceOf(exitPoint) == nullptr)
        {
            continue;
        }

        ExitPointAnimation* updated = UpdateExitPointMatrix(exitPoint);
        Vector4 local;
        VuTransformPoint(&toLocal, RowOf(&updated->matrix, 3), &local);
        GrowBoxByPoint(&box, &local);
        found = true;
    }

    if (!found)
    {
        return;
    }

    place = instance->place;
    RotateAndTranslate(place);
    SetCollisionBox(&instance->collision, &box.min, &box.max, 1, 0);
    GetInstanceHullBounds(&instance->collision, &place->matrix, &instance->collision.box);
}

u32 CharacterAgent::FitsAt(u32 kind, u32 push, const Vector4* normal, const Vector4* position)
{
    constexpr s32 MostTouched = 16;

    // Crouching, crawling and the knee drop take the crouched hull, standing tells the instances touched
    bool crouched = kind == FitCrouching || kind == FitCrawling || kind == FitKneeDrop;
    Vector4 at;
    if (position != nullptr)
    {
        at = *position;
    }
    else
    {
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        at = place->position;
    }

    at.y += heightOffset;
    ReferencedObject* leftOut[3] = {instance, ObjectOf(standingOn), nullptr};
    s32 leftOutCount = 2;
    auto* character = static_cast<CharacterPart*>(part);
    if (character->moveBits.linkedFirst != 0)
    {
        leftOut[2] = link->Second()->instance;
        leftOutCount = 3;
    }
    else if (character->moveBits.linkedSecond != 0)
    {
        leftOut[2] = link->Leader()->instance;
        leftOutCount = 3;
    }

    InstanceContext* touched[MostTouched];
    s32 touchedCount;
    Vector4 away;
    u32 overlaps = HullOverlaps(instance->chunk, crouched ? &body->crouchHull : &body->standingHull, &at, MostTouched,
                                SolidOrProjectileNodeKinds, leftOut, leftOutCount, touched, MostTouched, &touchedCount,
                                push != 0 ? &away : nullptr);
    if (push != 0 && overlaps != 0)
    {
        AddTo(&character->push, &away);
    }

    if (normal != nullptr && kind == FitStanding)
    {
        for (s32 i = 0; i < touchedCount; i++)
        {
            CallVirtual<void>(this, vtable, TouchedSlot, touched[i], normal);
        }
    }

    return overlaps == 0;
}

u32 CharacterAgent::FootNormal(Vector4* normal)
{
    // Its four corners 0.3 across and along, cast 0.3 down
    constexpr f32 Spread = Rounded(0.3);
    constexpr f32 Down = -Rounded(0.3);

    *normal = g_DefaultBox.min;
    normal->w = 1.0f;
    f32 hits = 0.0f;
    for (s32 corner = 0; corner < 4; corner++)
    {
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        Vector4 origin = *RowOf(&place->matrix, 3);
        place = instance->place;
        RotateAndTranslate(place);
        f32 across = (corner & 1) != 0 ? Spread : -Spread;
        Vector4 side = {place->matrix.m[0][0] * across, place->matrix.m[0][1] * across, place->matrix.m[0][2] * across, 1.0f};
        Vector4 point = {origin.x + side.x, origin.y + side.y, origin.z + side.z, 1.0f};
        place = instance->place;
        RotateAndTranslate(place);
        f32 along = corner >= 2 ? Spread : -Spread;
        Vector4 ahead = {place->matrix.m[2][0] * along, place->matrix.m[2][1] * along, place->matrix.m[2][2] * along, 1.0f};
        Vector4 from = {point.x + ahead.x, point.y + ahead.y, point.z + ahead.z, 1.0f};
        place = instance->place;
        RotateAndTranslate(place);
        Vector4 drop = {place->matrix.m[1][0] * Down, place->matrix.m[1][1] * Down, place->matrix.m[1][2] * Down, 1.0f};
        Vector4 to = {from.x + drop.x, from.y + drop.y, from.z + drop.z, 1.0f};
        Vector4 hitPoint;
        CollisionHit triangle;
        if (TriangleListRayCast(&cache, &from, &to, nullptr, &hitPoint, &triangle) == 0)
        {
            continue;
        }

        Vector4 plane;
        PlaneThroughTriangle(&plane, &triangle.vertices[0], &triangle.vertices[1], &triangle.vertices[2]);
        Vector4 facing = {plane.x, plane.y, plane.z, 1.0f};
        if (0.0f < facing.y)
        {
            facing.x = -facing.x;
            facing.y = -facing.y;
            facing.z = -facing.z;
        }

        hits += 1.0f;
        AddTo(normal, &facing);
    }

    if (hits == 0.0f)
    {
        return 0;
    }

    // The average, turned up again
    f32 inverse = 1.0f / hits;
    normal->x = -(normal->x * inverse);
    normal->y = -(normal->y * inverse);
    normal->z = -(normal->z * inverse);
    return 1;
}

void CharacterAgent::MoveBy(f32 seconds, f32 turn, CharacterPart* part, const Vector4* move, f32* turnOut, u32 mode)
{
    // Rising in a jump (past its first 0.1 seconds, not thrown) slower than it should, its vertical speed drops by 150 a second
    constexpr f32 RiseStart = Rounded(0.1);
    constexpr f32 Rising = 0.01f;
    constexpr f32 RiseAsked = 1e-5f;
    constexpr f32 RiseShare = 0.9f;
    constexpr f32 RiseSlowing = 150.0f;
    // The share of the move it made: 1 for a move under 0.01 across, a drop of more than 0.05 or 0.707 of it made
    constexpr f32 LeastMove = 0.01f;
    constexpr f32 Dropping = Rounded(-0.05);
    constexpr f32 FullShare = Rounded(0.707);
    constexpr f32 ShareScale = Rounded(1.0 / 0.707);
    // A knee slide with circle held stays longer: the floor's 3.5 over its value, and 4 by the slope it goes down
    constexpr f32 SlideFloor = 3.5f;
    constexpr f32 LeastSlope = Rounded(0.001);
    constexpr f32 SlideSlope = -4.0f;
    // The steps the tied characters' move is solved in
    constexpr u32 LinkedSteps = 2;

    bool wasOnGround = part->flags.onGround != 0;
    u32 attack = part->bits.attackKind;
    bool rising = attack != AttackThrownFromSpin && attack != AttackThrownFromJump && heightState == HeightJumping
                  && RiseStart < heightStateTime;
    MakeBoxOfExitPoints();
    Vector4 before;
    PlacePosition(&before);
    Vector4 step = *move;
    if (1.0f < Length(&step))
    {
        f32 inverse = InverseLength(&step, LengthEpsilon);
        step.x *= inverse;
        step.y *= inverse;
        step.z *= inverse;
    }

    if (Linked() != 0)
    {
        SolveLinked(seconds, &step, &turn, turnOut, mode, LinkedSteps);
    }
    else
    {
        f32 lift = 0.0f;
        if (heightState == HeightJumping)
        {
            lift = ExitPointsHeight();
        }

        Solve(lift, seconds, &step, &turn, turnOut, mode);
    }

    Vector4 after;
    PlacePosition(&after);
    Vector4 moved = {after.x - before.x, after.y - before.y, after.z - before.z, 1.0f};
    if (rising && Rising < velocity.y && RiseAsked < step.y && moved.y / step.y < RiseShare)
    {
        velocity.y -= seconds * RiseSlowing;
    }

    if (part->flags.onGround != 0 && !wasOnGround)
    {
        SetHeightState(HeightOnGround);
    }

    if (heightState == HeightOnGround && keptHeightOffset != 0.0f)
    {
        // Back on the ground, the offset kept from the jump comes down to what its exit points allow, and never below 0
        f32 height = ExitPointsHeight();
        height += HeightOffset();
        f32 excess = keptHeightOffset - height;
        if (0.0f < excess)
        {
            ChangeHeight(-excess);
            keptHeightOffset = height;
        }

        if (heightOffset < 0.0f)
        {
            ChangeHeight(-heightOffset);
            keptHeightOffset = 0.0f;
        }
    }

    heightStateTime += seconds;
    f32 asked = __builtin_sqrtf(step.x * step.x + step.z * step.z);
    f32 made = __builtin_sqrtf(moved.x * moved.x + moved.z * moved.z);
    f32 share = 1.0f;
    if (!(asked < LeastMove) && !(moved.y < Dropping))
    {
        f32 ratio = made / asked;
        if (ratio < FullShare)
        {
            share = ratio * ShareScale;
        }
    }

    part->moveShare = share;
    if (crouch == nullptr)
    {
        return;
    }

    f32 slope = 0.0f;
    u32 crouchState = crouch->bits.state;
    if ((crouchState == CrouchController::StateSliding || crouchState == CrouchController::StateSlideEnd)
        && 0.0f < buttons.circle)
    {
        if (floorSurface != nullptr)
        {
            slope = SlideFloor / floorSurface->acceleration;
        }

        f32 across = __builtin_sqrtf(moved.x * moved.x + moved.z * moved.z);
        if (LeastSlope < across)
        {
            f32 down = moved.y / across;
            if (down < 0.0f)
            {
                slope += down * SlideSlope;
            }
        }
    }

    if (1.0f < slope)
    {
        slope = 1.0f;
    }

    crouch->stateTicks += static_cast<s32>(slope * seconds * g_ClockUnitsPerSecond);
}

void CharacterAgent::Move(f32 seconds, CharacterPart* part, u32 controlled)
{
    // The first of two tied characters turns at 0.65 of the stick's rate
    constexpr f32 LinkedTurn = 0.65f;

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Vector4 side = *RowOf(&place->matrix, 0);
    f32 vertical = part->wantedVertical;
    Vector4 forward = *RowOf(&place->matrix, 2);
    s32 turn = 0;
    if (part->moveBits.turnRequested != 0)
    {
        turn = static_cast<s32>(static_cast<f32>(part->wantedTurn) * seconds);
    }

    // The speeds asked for along its axes (none when they aren't); one past 1 sets the part's moving flag
    bool withinSpeeds = true;
    if (part->moveBits.forwardRequested != 0)
    {
        f32 speed = part->wantedForward;
        forward.x *= speed;
        forward.y *= speed;
        forward.z *= speed;
        if (!(__builtin_fabsf(speed) <= 1.0f))
        {
            withinSpeeds = false;
        }
    }
    else
    {
        forward = g_DefaultBox.min;
        forward.w = 1.0f;
    }

    if (part->moveBits.sidewaysRequested != 0)
    {
        f32 speed = part->wantedSideways;
        side.x *= speed;
        side.y *= speed;
        side.z *= speed;
        if (!(__builtin_fabsf(speed) <= 1.0f))
        {
            withinSpeeds = false;
        }
    }
    else
    {
        side = g_DefaultBox.min;
        side.w = 1.0f;
    }

    Vector4 wanted = {forward.x + side.x, forward.y + side.y, forward.z + side.z, 1.0f};
    EaseVelocity(seconds, &wanted);
    if (part->moveBits.verticalRequested != 0)
    {
        MeasureVelocity(vertical, controlled);
    }

    Position(&lastPosition);
    ApplyGravity(seconds);
    Vector4 step = velocity;
    step.x *= seconds;
    step.y *= seconds;
    step.z *= seconds;
    u32 mode = MoveModeOf();
    f32 turnOut;
    if (part->moveBits.linkedFirst != 0)
    {
        turn = static_cast<s32>(static_cast<f32>(turn) * LinkedTurn);
        MoveBy(seconds, static_cast<f32>(turn) * AngleToRadians, part, &step, &turnOut, mode);
    }
    else if (part->moveBits.linkedSecond != 0)
    {
        // The second of two tied characters is put where the link swings it, the leader's place first, and the link given the
        // velocity that took it there
        place = instance->place;
        place->SyncPosition();
        Vector4 before = place->position;
        CharacterAgent* leader = link->Leader();
        SetObjectPlace(instance, leader->instance->place);
        Vector4 swing = link->swingPosition;
        swing.w = 1.0f;
        place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&swing))
        {
            QueueObject(instance);
        }

        const Vector4* now = &link->swingPosition;
        Vector4 moved = {now->x - before.x, now->y - before.y, now->z - before.z, 1.0f};
        f32 inverse = 1.0f / seconds;
        Vector4 swingVelocity = {inverse * moved.x, inverse * moved.y, inverse * moved.z, 1.0f};
        ObjectPlace* leaderPlace = link->Leader()->instance->place;
        RotateAndTranslate(leaderPlace);
        link->SecondGait(&swingVelocity, RowOf(&leaderPlace->matrix, 2));
    }
    else
    {
        MoveBy(seconds, static_cast<f32>(turn) * AngleToRadians, part, &step, &turnOut, mode);
        if (part->moveBits.clawing == 0)
        {
            s32 angle;
            AngleFrom(&angle, turnOut, AngleRadians);
            InstanceContext* self = instance;
            place = self->place;
            if (angle != 0)
            {
                place->SyncRotation();
                place->MarkTurned();
                s32 yaw = angle;
                Vector4 rotation;
                RotationFromYaw(&rotation, &yaw);
                MultiplyRotations(&rotation, &rotation, &place->rotation);
                place->rotation = rotation;
                QueueObject(self);
            }
        }
    }

    part->flags.moving = !withinSpeeds;
}

void CharacterAgent::SlideIntoBody(DynamicBody* body)
{
    // Pushed away 4 (times 1.2) and up by 3.2 to 9 (times 1.2) as the slide fades
    constexpr f32 Away = 4.0f;
    constexpr f32 Strength = Rounded(1.2);
    constexpr f32 LeastLift = Rounded(3.2);
    constexpr f32 MoreLift = Rounded(5.8);

    Vector4 position;
    Position(&position);
    Vector4 local;
    VuTransformPoint(&body->inverseMatrix, &position, &local);
    Vector4 away = *RowOf(&body->matrix, 3);
    away.x -= position.x;
    away.y = 0.0f;
    away.z -= position.z;
    f32 inverse = InverseLength(&away, LengthEpsilon);
    away.x *= inverse;
    away.y *= inverse;
    away.z *= inverse;
    f32 fade = crouch->slideFade;
    if (fade < 0.0f)
    {
        fade = 0.0f;
    }

    if (1.0f < fade)
    {
        fade = 1.0f;
    }

    Vector4 impulse = {away.x * Away * Strength, away.y * Away * Strength, away.z * Away * Strength, 1.0f};
    body->ApplyImpulse(&impulse, &local);
    Vector4 lift = {0.0f, (fade * MoreLift + LeastLift) * Strength, 0.0f, 1.0f};
    body->ApplyImpulse(&lift, &g_DefaultBox.min);
    crouch->slideSpeed = 0.0f;
    if (body->instance == ObjectOf(pushedBody))
    {
        AssignReference(&pushedBody, nullptr);
    }
}

void CharacterAgent::WalkIntoBody(DynamicBody* body, const Vector4* point)
{
    constexpr f32 Strength = 3.5f;
    constexpr f32 LeastAcross = Rounded(0.001);
    // A spin kicks it (once every 0.8 seconds) and pushes 1.4 times harder
    constexpr f32 KickShare = 0.5f;
    constexpr f32 SpinStrength = 1.4f;

    if (__builtin_sqrtf(point->x * point->x + point->z * point->z) < __builtin_fabsf(point->y))
    {
        return;
    }

    DynamicBody* sphere = IsSphere(body) ? body : nullptr;
    Vector4 position;
    Position(&position);
    position.y += KickHeight;
    Vector4 local;
    VuTransformPoint(&body->inverseMatrix, &position, &local);
    auto* movement = static_cast<MovementNode*>(GetGameNode(&instance->nodes, NodeMovement));
    if (movement != nullptr)
    {
        // Retail works out its velocity from its movement node and does nothing with it
        const Matrix4x4* matrix = movement->CurrentMatrix();
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 moved = place->position;
        moved.x -= matrix->m[3][0];
        moved.y -= matrix->m[3][1];
        moved.z -= matrix->m[3][2];
        f32 inverse = 1.0f / movement->Seconds();
        moved.x *= inverse;
        moved.y *= inverse;
        moved.z *= inverse;
    }

    Vector4 slip = velocity;
    Vector4 bodyVelocity;
    body->VelocityAt(&position, &bodyVelocity);
    slip.x -= bodyVelocity.x;
    slip.y -= bodyVelocity.y;
    slip.z -= bodyVelocity.z;
    Vector4 push = slip;
    push.x *= Strength;
    push.y *= Strength;
    push.z *= Strength;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    if (slip.x * place->matrix.m[2][0] + slip.y * place->matrix.m[2][1] + slip.z * place->matrix.m[2][2] < 0.0f)
    {
        return;
    }

    if (sphere != nullptr)
    {
        // Retail bug: the push along the sphere's side (across the way it's walked into) is worked out and then left out, a
        // push of nothing is added instead
        Vector4 away = {position.x - body->matrix.m[3][0], position.y - body->matrix.m[3][1], position.z - body->matrix.m[3][2],
                        1.0f};
        Vector4 across = away;
        across.y = 0.0f;
        if (LeastAcross < across.x * across.x + across.z * across.z)
        {
            f32 inverse = InverseLength(&across, LengthEpsilon);
            Vector4 heading = velocity;
            across.x *= inverse;
            across.y *= inverse;
            across.z *= inverse;
            heading.y = 0.0f;
            if (LeastAcross < heading.x * heading.x + heading.z * heading.z)
            {
                inverse = InverseLength(&heading, LengthEpsilon);
                heading.x *= inverse;
                heading.y *= inverse;
                heading.z *= inverse;
                f32 along = across.x * heading.x + across.y * heading.y + across.z * heading.z;
                if (0.0f < along)
                {
                    across.x -= heading.x * along;
                    across.y -= heading.y * along;
                    across.z -= heading.z * along;
                    inverse = InverseLength(&across, LengthEpsilon);
                    across.x *= inverse;
                    across.y *= inverse;
                    across.z *= inverse;
                    constexpr Vector4 Sideways = {0.0f, 0.0f, 0.0f, 1.0f};
                    AddTo(&push, &Sideways);
                }
            }
        }
    }

    if (Spinning(static_cast<CharacterPart*>(part)))
    {
        if (kickCooldown == 0.0f && body->instance != ObjectOf(pushedBody))
        {
            AddTo(&body->angularMomentum, &KickSpin);
            Vector4 kick = push;
            kick.y = 0.0f;
            body->bodyFlags.velocityStale = 1;
            kick.x *= KickShare;
            kick.z *= KickShare;
            body->ApplyImpulse(&kick, &local);
            kickCooldown = KickSeconds;
        }

        push.x *= SpinStrength;
        push.y *= SpinStrength;
        push.z *= SpinStrength;
    }

    body->AddLocalForce(&push, &local);
}

void CharacterAgent::StandOnBody(DynamicBody* body)
{
    // Pressed down by 30 (a sphere only while the character doesn't rise 2 faster than it); a jump off another body kicks it
    // once, by 0.7 of the velocity across
    constexpr Vector4 Press = {0.0f, -30.0f, 0.0f, 1.0f};
    constexpr f32 Rebound = 2.0f;
    constexpr f32 JumpKick = 0.7f;

    bool sphere = IsSphere(body);
    Vector4 position;
    Position(&position);
    Vector4 local;
    VuTransformPoint(&body->inverseMatrix, &position, &local);
    if (sphere)
    {
        Vector4 pointVelocity;
        body->VelocityAt(&position, &pointVelocity);
        if (velocity.y - pointVelocity.y < Rebound)
        {
            Vector4 press = Press;
            body->AddForce(&press, &position);
        }

        return;
    }

    Vector4 press = Press;
    body->AddLocalForce(&press, &local);
    if (jump->bits.jumped != 0)
    {
        Vector4 kick = velocity;
        kick.x *= JumpKick;
        kick.z *= JumpKick;
        body->ApplyImpulse(&kick, &local);
        jump->bits.jumped = 0;
    }
}

void CharacterAgent::KickPushedBody()
{
    // 15 forward and 0.6 of the character's speed forward, raised by a tenth
    constexpr f32 Strength = 15.0f;
    constexpr f32 SpeedShare = Rounded(0.6);
    constexpr f32 Raise = Rounded(0.1);

    DynamicBody* body = BodyOf(ObjectOf(pushedBody));
    AddTo(&body->angularMomentum, &KickSpin);
    body->bodyFlags.velocityStale = 1;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Vector4 forward = *RowOf(&place->matrix, 2);
    f32 speed = forward.x * velocity.x + forward.y * velocity.y + forward.z * velocity.z;
    f32 raise = __builtin_sqrtf(forward.x * forward.x + forward.z * forward.z) * Raise;
    f32 strength = speed * SpeedShare + Strength;
    Vector4 impulse = {forward.x * strength, (forward.y + raise) * strength, forward.z * strength, forward.w};
    Vector4 position;
    Position(&position);
    position.y += KickHeight;
    Vector4 local;
    VuTransformPoint(&body->inverseMatrix, &position, &local);
    body->ApplyImpulse(&impulse, &local);
    kickCooldown = KickSeconds;
}

void CharacterAgent::KeepPushedBody()
{
    // Kept while one of the two moves, the character is on the ground with the body ahead and the stick doesn't point away
    // from it
    constexpr f32 Still = Rounded(0.2);
    constexpr f32 Reach = 4.0f;
    constexpr f32 LeastStick = Rounded(0.2);
    constexpr f32 Away = -0.35f;

    DynamicBody* body = BodyOf(ObjectOf(pushedBody));
    if (body != nullptr && !(Length(&body->velocity) < Still && Length(&velocity) < Still)
        && static_cast<CharacterPart*>(part)->flags.onGround != 0)
    {
        Vector4 position;
        Position(&position);
        Vector4 toBody = {body->matrix.m[3][0] - position.x, body->matrix.m[3][1] - position.y,
                          body->matrix.m[3][2] - position.z, 1.0f};
        f32 inverse = InverseLength(&toBody, LengthEpsilon);
        toBody.x *= inverse;
        toBody.y *= inverse;
        toBody.z *= inverse;
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        f32 ahead = toBody.x * place->matrix.m[2][0] + toBody.y * place->matrix.m[2][1] + toBody.z * place->matrix.m[2][2];
        // Retail bug: the reach is measured on the direction made unit long, which is always within it
        if (0.0f < ahead && Length(&toBody) < Reach)
        {
            Vector4 stick = {buttons.moveX, 0.0f, buttons.moveZ, 1.0f};
            Vector4 flat = toBody;
            flat.y = 0.0f;
            inverse = InverseLength(&flat, LengthEpsilon);
            f32 stickLength = Length(&stick);
            flat.x *= inverse;
            flat.y *= inverse;
            flat.z *= inverse;
            if (stickLength < LeastStick || Away < stick.x * flat.x + stick.y * flat.y + stick.z * flat.z)
            {
                return;
            }
        }
    }

    AssignReference(&pushedBody, nullptr);
}

void CharacterAgent::PickPushedBody(InstanceContext** touched, s32 count)
{
    // On the ground, with the stick pushed and the body ahead
    constexpr f32 LeastStick = Rounded(0.1);
    constexpr f32 Ahead = 0.5f;

    for (s32 i = 0; i < count; i++)
    {
        InstanceContext* other = touched[i];
        if (other == ObjectOf(standingOn))
        {
            continue;
        }

        DynamicBody* body = BodyOf(other);
        if (body->bodyFlags.pushable == 0
            || static_cast<CharacterPart*>(part)->flags.onGround == 0)
        {
            continue;
        }

        Vector4 position;
        Position(&position);
        Vector4 toBody = {body->matrix.m[3][0] - position.x, body->matrix.m[3][1] - position.y,
                          body->matrix.m[3][2] - position.z, 1.0f};
        Vector4 stick = {buttons.moveX, 0.0f, buttons.moveZ, 1.0f};
        Vector4 flat = toBody;
        flat.y = 0.0f;
        f32 inverse = InverseLength(&flat, LengthEpsilon);
        f32 stickLength = Length(&stick);
        flat.x *= inverse;
        flat.y *= inverse;
        flat.z *= inverse;
        if (!(LeastStick < stickLength))
        {
            continue;
        }

        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        if (!(Ahead < place->matrix.m[2][0] * flat.x + place->matrix.m[2][1] * flat.y + place->matrix.m[2][2] * flat.z))
        {
            continue;
        }

        AssignReference(&pushedBody, other);
        pushedDistance = Length(&toBody);
        BodyOf(ObjectOf(pushedBody))->ReleaseRide();
    }
}

void CharacterAgent::HoldPushedBody()
{
    // Held its distance ahead (crouched, its radius and 1.15) by a spring of 350 by the gap and 90 by the velocities' difference,
    // at most 80 across and less for a radius under 0.6; let go past 1.3
    constexpr f32 Radius = 0.5f;
    constexpr f32 CrouchedReach = 1.15f;
    constexpr f32 MostGap = 1.3f;
    constexpr f32 Stiffness = 350.0f;
    constexpr f32 Damping = 90.0f;
    constexpr f32 MostForce = 80.0f;
    constexpr f32 SmallRadius = Rounded(0.6);
    constexpr f32 SmallScale = Rounded(1.0 / 0.6);

    InstanceContext* pushed = ObjectOf(pushedBody);
    if (pushed == nullptr)
    {
        if (look != nullptr)
        {
            look->SetCarried(0.0f, 0.0f, 0.0f, 0);
        }

        return;
    }

    DynamicBody* body = BodyOf(pushed);
    if (body == nullptr)
    {
        AssignReference(&pushedBody, nullptr);
        return;
    }

    f32 radius = Radius;
    if (IsSphere(body))
    {
        radius = static_cast<SphereBody*>(body)->radius;
    }

    Vector4 target;
    Position(&target);
    ObjectPlace* place = instance->place;
    Vector4 ahead;
    if (static_cast<CharacterPart*>(part)->moveBits.crouching != 0)
    {
        f32 reach = radius + CrouchedReach;
        RotateAndTranslate(place);
        ahead = {place->matrix.m[2][0] * reach, place->matrix.m[2][1] * reach, place->matrix.m[2][2] * reach, 1.0f};
    }
    else
    {
        RotateAndTranslate(place);
        f32 reach = pushedDistance;
        ahead = {place->matrix.m[2][0] * reach, place->matrix.m[2][1] * reach, place->matrix.m[2][2] * reach, 1.0f};
    }

    AddTo(&target, &ahead);
    Vector4 gap = {target.x - body->matrix.m[3][0], target.y - body->matrix.m[3][1], target.z - body->matrix.m[3][2], 1.0f};
    if (MostGap < Length(&gap))
    {
        AssignReference(&pushedBody, nullptr);
        return;
    }

    Vector4 spring = {gap.x * Stiffness, gap.y * Stiffness, gap.z * Stiffness, 1.0f};
    Vector4 slip = {body->velocity.x - velocity.x, body->velocity.y - velocity.y, body->velocity.z - velocity.z, 1.0f};
    Vector4 damping = {slip.x * Damping, slip.y * Damping, slip.z * Damping, 1.0f};
    Vector4 force = {spring.x - damping.x, spring.y - damping.y, spring.z - damping.z, 1.0f};
    place = instance->place;
    RotateAndTranslate(place);
    f32 sideways = gap.x * place->matrix.m[0][0] + gap.y * place->matrix.m[0][1] + gap.z * place->matrix.m[0][2];
    place = instance->place;
    RotateAndTranslate(place);
    f32 forward = gap.x * place->matrix.m[2][0] + gap.y * place->matrix.m[2][1] + gap.z * place->matrix.m[2][2];
    force.y = 0.0f;
    if (MostForce < __builtin_sqrtf(force.x * force.x + force.z * force.z))
    {
        f32 inverse = InverseLength(&force, LengthEpsilon);
        force.x = force.x * inverse * MostForce;
        force.y = force.y * inverse * MostForce;
        force.z = force.z * inverse * MostForce;
    }

    if (radius < SmallRadius)
    {
        f32 scale = radius * SmallScale;
        force.x *= scale;
        force.y *= scale;
        force.z *= scale;
    }

    Vector4 center = {0.0f, 0.0f, 0.0f, 1.0f};
    body->AddLocalForce(&force, &center);
    if (look != nullptr)
    {
        look->SetCarried(sideways, forward, radius, 1);
    }
}

void CharacterAgent::PushBodies(f32 seconds, ContactSet* contacts)
{
    // The pushed body is held 0.1 further than where it's touched; out of touch, with the stick pushed, it's held a unit a
    // second closer. A slide knocks a body once a second. Room for 32 bodies touched (retail doesn't check)
    constexpr f32 HoldGap = Rounded(0.1);
    constexpr f32 LeastStick = Rounded(0.1);
    constexpr f32 SlideHitSeconds = 1.0f;
    constexpr s32 MostTouched = 32;

    f32 cooldown = slideHitCooldown - seconds;
    slideHitCooldown = cooldown < 0.0f ? 0.0f : cooldown;
    cooldown = kickCooldown - seconds;
    kickCooldown = cooldown < 0.0f ? 0.0f : cooldown;
    InstanceContext* touched[MostTouched];
    s32 touchedCount = 0;
    for (s32 i = 0; i < contacts->solid.count; i++)
    {
        const ::Contact* entry = &contacts->solid.contacts[i];
        ContactKind kind = entry->kind;
        if (kind.hull == 0)
        {
            continue;
        }

        InstanceContext* other = entry->instance;
        if (BodyOf(other) != nullptr && kind.pushShared != 0)
        {
            touched[touchedCount] = other;
            touchedCount++;
        }
    }

    auto* character = static_cast<CharacterPart*>(part);
    if (Linked() == 0)
    {
        if (Spinning(character) && ObjectOf(pushedBody) != nullptr && kickCooldown == 0.0f)
        {
            KickPushedBody();
        }

        if (!Spinning(character))
        {
            if (ObjectOf(pushedBody) != nullptr)
            {
                KeepPushedBody();
            }

            if (ObjectOf(pushedBody) == nullptr)
            {
                PickPushedBody(touched, touchedCount);
            }
        }
        else
        {
            AssignReference(&pushedBody, nullptr);
        }

        InstanceContext* pushed = ObjectOf(pushedBody);
        if (pushed != nullptr)
        {
            if (BodyOf(pushed) == nullptr)
            {
                AssignReference(&pushedBody, nullptr);
            }
            else
            {
                s32 i = 0;
                for (; i < touchedCount; i++)
                {
                    if (touched[i] == ObjectOf(pushedBody))
                    {
                        DynamicBody* body = BodyOf(ObjectOf(pushedBody));
                        Vector4 position;
                        Position(&position);
                        Vector4 away = {body->matrix.m[3][0] - position.x, body->matrix.m[3][1] - position.y,
                                        body->matrix.m[3][2] - position.z, 1.0f};
                        pushedDistance = Length(&away) + HoldGap;
                        break;
                    }
                }

                if (i == touchedCount)
                {
                    Vector4 stick = {buttons.moveX, 0.0f, buttons.moveZ, 1.0f};
                    if (LeastStick < Length(&stick))
                    {
                        pushedDistance -= seconds;
                    }
                }
            }
        }

        HoldPushedBody();
    }

    // The body marked below it is pressed while it falls, the ones it pushes against pushed (or, sliding on the ground into one
    // from the side, knocked away once a second)
    for (s32 i = 0; i < contacts->solid.count; i++)
    {
        ::Contact* entry = &contacts->solid.contacts[i];
        ContactKind kind = entry->kind;
        if (kind.hull == 0)
        {
            continue;
        }

        DynamicBody* body = BodyOf(entry->instance);
        if (body == nullptr)
        {
            continue;
        }

        if (kind.marked != 0 && velocity.y < 0.0f)
        {
            StandOnBody(body);
        }

        if (kind.pushShared == 0)
        {
            continue;
        }

        const Vector4* point = &entry->point;
        f32 across = __builtin_sqrtf(point->x * point->x + point->z * point->z);
        if (Sliding(character) && slideHitCooldown == 0.0f && character->flags.onGround != 0
            && __builtin_fabsf(point->y) < across + across)
        {
            SlideIntoBody(body);
            slideHitCooldown = SlideHitSeconds;
        }
        else
        {
            WalkIntoBody(body, point);
        }
    }

    InstanceContext* probed = ObjectOf(probedInstance);
    if (probed == nullptr)
    {
        return;
    }

    DynamicBody* body = BodyOf(probed);
    if (body != nullptr && body->instance != ObjectOf(pushedBody))
    {
        WalkIntoBody(body, &probePush);
    }
}
