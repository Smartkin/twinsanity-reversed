#include "game/agents.h"

#include "game/animation.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/controls.h"
#include "game/cutscenereader.h"
#include "game/effects.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/vehicles.h"

#include <cstddef>
#include <cstdint>

extern "C"
{
    // An instance's decal of a kind (footprints) left at an exit point, offset in its space
    void AddInstanceDecal(u32 kind, InstanceContext* instance, ExitPointAnimation* at, const Vector4* offset)
        RETAIL(AddInstanceDecal);
    // An object node's slot animation played (the blend's seconds first; commandsfocus.cpp)
    void PlaySlotAnimation(f32 blendSeconds, ObjectNode* node, u32 slot, u32 loops) RETAIL_N32(FUN_002124c0);
    void CopyShort(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
}

EABI_EXPORT(FUN_00137010, &CharacterAgent::PushBack);
EABI_EXPORT(FUN_0013fad8, &CharacterAgent::PlayRideSound);
EABI_EXPORT(FUN_0013fb60, &CharacterAgent::Launch);

namespace
{

// The bits of an instance's node mask of the kinds the character's attacks go to: the agents' nodes (0xC to 0x14 but the
// graples') and the dynamic scenery's (4)
constexpr u32 CharacterNodes = 1u << 0xC;
constexpr u32 CrateNodes = 1u << 0xD;
constexpr u32 PickupNodes = 1u << 0xE;
constexpr u32 CreatureNodes = 1u << 0xF;
constexpr u32 GenericObjectNodes = 1u << 0x10;
constexpr u32 GrabbableNodes = 1u << 0x11;
constexpr u32 PayGateNodes = 1u << 0x12;
constexpr u32 ProjectileNodes = 1u << 0x14;
constexpr u32 DynamicSceneryNodes = 1u << 4;
constexpr u32 AttackedNodes[] = {CrateNodes,         PickupNodes,    ProjectileNodes, CreatureNodes,
                                 GenericObjectNodes, GrabbableNodes, PayGateNodes};

// The object Cortex's AgentRef2 is taken along through chunk links as
constexpr u16 CortexCarriedObject = 0x264;
// The footprints' offsets from the feet sideways (Crash's, Cortex's) and ahead
constexpr f32 CrashFootSide = Rounded(0.085);
constexpr f32 CortexFootSide = 0.06f;
constexpr f32 FootAhead = 0.12f;

constexpr f32 NoFallHeight = -100000.0f;
// A fall's drop that runs its event with a fall started, and without one
constexpr f32 FarDrop = 10.0f;
constexpr f32 LongDrop = 20.0f;
constexpr f32 FarFallRestSeconds = 3.0f;
constexpr f32 LandFromFarRestSeconds = 2.0f;
// A normal this close to straight up or down is taken as up or down
constexpr f32 Vertical = 0.9f;
// A knock-back: away from the hurter level, this far and this fast up, from where a quarter of its velocity puts it
constexpr f32 KnockDistance = 5.0f;
constexpr f32 KnockUpSpeed = 12.5f;
constexpr f32 KnockLead = 0.25f;
constexpr f32 LevelEpsilon = 1e-6f;
constexpr f32 PushEpsilon = 0.005f;
// The hit points a hit takes while it's invincible
constexpr u32 InvincibleDamage = 50;
// A vehicle starts at the character's velocity, at least this fast forward
constexpr f32 VehicleSpeed = 15.0f;
// The box hulls its attacks hit with: alone and tied to the other character
constexpr Vector4 AttackHullMin = {Rounded(-0.99), Rounded(0.05), Rounded(-0.99), 1.0f};
constexpr Vector4 AttackHullMax = {Rounded(0.99), 0.9f, Rounded(0.99), 1.0f};
constexpr Vector4 LinkedAttackHullMin = {-1.5f, 0.5f, -1.5f, 1.0f};
constexpr Vector4 LinkedAttackHullMax = {1.5f, 1.4f, 1.5f, 1.0f};
// After a set back: the slot animation blended in
constexpr f32 RestartBlendSeconds = Rounded(0.2);
constexpr u32 RestartAnimationSlot = 8;

template <typename T>
T* Allocate()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}

template <typename T>
T* Make(CharacterAgent* agent)
{
    return T::Construct(Allocate<T>(), agent);
}

CharacterPart* PartOf(const Agent* agent)
{
    return static_cast<CharacterPart*>(agent->part);
}

// The attack kind is the part's bits' low byte, which retail stores alone
void SetAttackKind(BasicAgentPart* part, u8 kind)
{
    *reinterpret_cast<u8*>(&part->bits) = kind;
}

// Of the kinds an attack reaches but the characters, the first an instance's node mask has (none: 0)
u32 AttackedKind(u32 mask)
{
    for (u32 kind : AttackedNodes)
    {
        if ((mask & kind) != 0)
        {
            return kind;
        }
    }

    return 0;
}

// An instance's object node pushed by another instance, without strength
void PushObjectNode(InstanceContext* instance, InstanceContext* by)
{
    auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, NodeObject));
    CallVirtual<void>(node, node->vtable, ObjectNode::PushSlot, by, 0.0f);
}

bool TakesPackets(ObjectNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

OgiAnimator* AnimatorOf(InstanceContext* instance)
{
    return static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel))->animator;
}

// An exit point of the instance's model made up to date (none without the animator, its exit points or that one)
ExitPointAnimation* UpdatedExitPoint(InstanceContext* instance, u32 index)
{
    OgiAnimator* animator = AnimatorOf(instance);
    if (animator == nullptr)
    {
        return nullptr;
    }

    ExitPointAnimation* exitPoint = animator->exitPoints != nullptr ? animator->exitPoints->data[index] : nullptr;
    return exitPoint != nullptr ? UpdateExitPointMatrix(exitPoint) : nullptr;
}

// The controls node's pad, through its address: retail clears it without a node too (the word at 0x18)
GamePad** PadOf(ControlsNode* controls)
{
    return reinterpret_cast<GamePad**>(reinterpret_cast<std::uintptr_t>(controls) + offsetof(ControlsNode, pad));
}

// Its fall forgotten: the part not falling and running no fall events, no fall start
void ClearFall(CharacterAgent* agent)
{
    CharacterPart* part = PartOf(agent);
    part->flags.falling = 0;
    part->moveBits.noFallEvents = 1;
    agent->fallStart = {0.0f, NoFallHeight, 0.0f, 1.0f};
}

// What a set back and a state applied make new: the buttons, the move, the cooldowns, the floor surface (the default one),
// what it stands on, the ground's normal, the velocity, the height state and offsets, the last position (the ground point),
// the linked point and the crushes
void ClearMotion(CharacterAgent* agent)
{
    CharacterButtons& buttons = agent->buttons;
    buttons.turn = 0.0f;
    buttons.moveZ = 0.0f;
    buttons.moveX = 0.0f;
    buttons.cross = 0.0f;
    buttons.square = 0.0f;
    buttons.circle = 0.0f;
    buttons.shoulders = 0.0f;
    buttons.locked.value = 0;
    agent->moveInput = g_DefaultBox.min;
    agent->moveInput.w = 1.0f;
    agent->kickCooldown = 0.0f;
    agent->slideHitCooldown = 0.0f;
    agent->SetFloorSurface(&g_CollisionSurfaces.surfaces[0]);
    agent->state.standing = CharacterAgent::StandingNothing;
    AssignReference(&agent->standingOn, nullptr);
    agent->standingStamp = 0;
    agent->standingHull = -1;
    agent->groundNormal = {0.0f, 1.0f, 0.0f, 1.0f};
    agent->velocity = g_DefaultBox.min;
    agent->velocity.w = 1.0f;
    agent->heightState = HeightOnGround;
    agent->heightOffset = 0.0f;
    agent->keptHeightOffset = 0.0f;
    agent->lastPosition = agent->groundPoint;
    agent->linkedPoint = g_DefaultBox.min;
    agent->linkedPoint.w = 1.0f;
    agent->crushCount = 0;
}

void KeepContact(Agent* agent, const ContactMessage* message)
{
    agent->contact.point = message->point;
    agent->contact.hitKinds = message->hitKinds;
    agent->contact.damage = message->damage;
}

void Die(CharacterAgent* agent, InstanceContext* sender)
{
    agent->buttons.locked.value = CharacterButtons::LockAll;
    agent->state.dead = 1;
    RunAgentEvent(agent, EventDied, reinterpret_cast<u32>(sender), 0, 0);
}

// Invincible for 2 seconds after a hurt, unless it already is (StartInvincibility's kind 0 inline, its start time left)
void StartHurtInvincibility(CharacterAgent* agent)
{
    CharacterPart* part = PartOf(agent);
    if (part->bits.invulnerable != 0)
    {
        return;
    }

    part->bits.invulnerable = 1;
    agent->state.mode = CharacterAgent::ModeInvincible;
    agent->modeTicks = static_cast<s32>(g_ClockUnitsPerSecond + g_ClockUnitsPerSecond);
}

// A target lock let go of (there's no destructor: the claw's and the gun's are destroyed inline)
void DestroyTargetLock(TargetLock* lock)
{
    HullDestroy(&lock->hull, DestroyOnly);
    RemoveReference(&lock->marker);
    RemoveReference(&lock->target);
}

void DestroyClaw(ClawController* claw)
{
    DestroyTargetLock(&claw->lock);
    RemoveReference(&claw->graple);
    MemoryDeallocate2_(claw);
}

// The gun's locks from the last
void DestroyGun(Gun* gun)
{
    for (TargetLock* lock = gun->locks + sizeof(gun->locks) / sizeof(*gun->locks); lock != gun->locks;)
    {
        lock--;
        DestroyTargetLock(lock);
    }

    RemoveReference(&gun->gunInstance);
    MemoryDeallocate2_(gun);
}

// A character off its vehicle at its exit matrix, its controls its own again, its controllers made again (the event unless the
// vehicle is being replaced) and its own box hull (the vehicle's box) let go
void StepOff(CharacterAgent* agent, const Matrix4x4* exit, u32 replaced, bool passenger)
{
    ObjectCollision* collision = &agent->instance->collision;
    auto* controls = static_cast<ControlsNode*>(GetGameNode(&agent->instance->nodes, NodeControls));
    if (controls != nullptr)
    {
        ReplaceControlsHandler(controls, nullptr);
    }

    if (passenger)
    {
        // Retail: outside the check above (a missing controls node gets the word at 0x18 cleared)
        *PadOf(controls) = nullptr;
    }

    InstanceContext* instance = agent->instance;
    if (SetPlaceMatrix(instance->place, exit) != 0)
    {
        QueueObject(instance);
    }

    agent->TearDown(0);
    agent->SetUp(0);
    if (replaced == 0)
    {
        RunAgentEvent(agent, EventVehicleLeft, 0, 0, 0);
    }

    ReleaseCollisionHull(collision);
}

// The box hulls the characters' attacks hit with, made by the shared cache (made the first time)
CollisionHull* SharedBoxHull(const Vector4* min, const Vector4* max)
{
    if (g_BoxHullCache == nullptr)
    {
        g_BoxHullCache = ConstructBoxHullCache(Allocate<BoxHullCache>());
    }

    return BoxHullOf(g_BoxHullCache, min, max);
}
}

void CharacterAgent::Reset()
{
    verticalBoost = 0.0f;
    state.previousMode = ModeNone;
    state.mode = ModeNone;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    state.groundFound = 0;
    state.noFallEvents = 0;
    groundPoint = place->position;
    triggers.Clear();
    if (jump != nullptr)
    {
        jump->Reset();
    }

    if (crouch != nullptr)
    {
        crouch->Reset();
    }

    if (spin != nullptr)
    {
        spin->Reset();
    }

    ClearMotion(this);
    AssignReference(&pushedBody, nullptr);
    if (properties->GetInt(CharacterKindProperty) != CharacterNone && body != nullptr)
    {
        LiftOntoGround(CharacterLiftHeight, CharacterLiftReach);
    }

    if (vehicle == nullptr)
    {
        auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
        PlaySlotAnimation(RestartBlendSeconds, node, RestartAnimationSlot, 1);
    }
}

void CharacterAgent::ApplyState(u32 unknown)
{
    CreatureAgent::ApplyState(unknown);
    state.previousMode = ModeApplied;
    state.mode = ModeNone;
    verticalBoost = 0.0f;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    state.groundFound = 0;
    state.noFallEvents = 0;
    homeChunk = nullptr;
    groundPoint = place->position;
    Remake();
    Vector4 min = AttackHullMin;
    Vector4 max = AttackHullMax;
    attackHull = SharedBoxHull(&min, &max);
    min = LinkedAttackHullMin;
    max = LinkedAttackHullMax;
    linkedAttackHull = SharedBoxHull(&min, &max);
    triggers.Clear();
    ClearMotion(this);
    ReleaseCollisionHull(&instance->collision);
    instance->flags.movesBetweenChunks = 1;
    AssignReference(&pushedBody, nullptr);
}

u32 CharacterAgent::KindOf(InstanceContext* other)
{
    if (other == instance)
    {
        return 0;
    }

    u32 mask = other->nodes.mask;
    if ((mask & CharacterNodes) != 0)
    {
        // The second of the two tied together is nothing to it
        if (link == nullptr)
        {
            return CharacterNodes;
        }

        CharacterAgent* second = link->Second();
        if (second == nullptr || second->instance != other)
        {
            return CharacterNodes;
        }

        return 0;
    }

    u32 kind = AttackedKind(mask);
    if (kind != 0)
    {
        return kind;
    }

    return (mask & DynamicSceneryNodes) != 0 ? DynamicSceneryNodes : 0;
}

u32 CharacterAgent::ContactAttack(const Vector4* normal)
{
    CharacterPart* character = PartOf(this);
    if (character->moveBits.linkedSecond != 0)
    {
        return AttackTied;
    }

    u32 kind = character->bits.attackKind;
    if (kind == AttackSlam || kind == AttackSlide || (kind >= AttackSpinVariant && kind <= AttackThrownFromJump))
    {
        return kind;
    }

    bool spinning = kind == AttackSpin;
    if (normal->y == -1.0f && velocity.y < -1.0f)
    {
        return spinning ? AttackLandOnSpinning : AttackLandOn;
    }

    if (normal->y == 1.0f && 0.0f <= velocity.y)
    {
        return spinning ? AttackFromBelowSpinning : AttackFromBelow;
    }

    return spinning ? AttackSpin : AttackWalkInto;
}

void CharacterAgent::Touched(InstanceContext* other, const Vector4* normal)
{
    u32 kinds = KindOf(other);
    if (kinds == 0)
    {
        return;
    }

    u32 kind = ContactAttack(normal);
    if (kind == AttackLandOnSpinning)
    {
        QueueAttack(other, AttackLandOn, instance, kinds);
        QueueAttack(other, AttackSpin, instance, kinds);
    }
    else if (kind == AttackFromBelowSpinning)
    {
        QueueAttack(other, AttackFromBelow, instance, kinds);
        QueueAttack(other, AttackSpin, instance, kinds);
    }
    else
    {
        QueueAttack(other, kind, instance, kinds);
    }

    // Another character walks into this one
    if (kinds == CharacterNodes)
    {
        QueueAttack(instance, AttackWalkInto, other, CharacterNodes);
    }

    if (other->flags.physicsBody)
    {
        PushObjectNode(other, instance);
    }
}

void CharacterAgent::SendAttack(InstanceContext* other, const Vector4* normal)
{
    u32 kinds = 0;
    if (other != instance)
    {
        u32 mask = other->nodes.mask;
        kinds = (mask & CharacterNodes) != 0 ? CharacterNodes : AttackedKind(mask);
    }

    if (kinds != 0)
    {
        u32 kind = AttackWalkInto;
        if (normal->y == -1.0f)
        {
            kind = AttackLandOn;
        }
        else if (normal->y == 1.0f)
        {
            kind = AttackFromBelow;
        }

        QueueAttack(other, kind, instance, kinds);
    }

    if (other->flags.physicsBody)
    {
        PushObjectNode(other, instance);
    }
}

void CharacterAgent::FallFrame()
{
    bool events = state.noFallEvents == 0;
    CharacterPart* character = PartOf(this);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    f32 drop = fallStart.y - place->position.y;
    if (events)
    {
        if (character->moveBits.falling != 0)
        {
            if (FarDrop <= drop)
            {
                RunAgentEvent(this, EventFellFar, reinterpret_cast<u32>(instance), 0, 0);
                character->moveBits.fellFar = 1;
                character->moveBits.falling = 0;
                if (look != nullptr)
                {
                    look->restSeconds = FarFallRestSeconds;
                }
            }
        }
        else if (character->moveBits.noFallEvents == 0 && LongDrop <= drop)
        {
            RunAgentEvent(this, EventLongDrop, reinterpret_cast<u32>(instance), 0, 0);
        }
    }

    CreatureAgent::FallFrame();
}

void CharacterAgent::Land(u32 tell)
{
    CharacterPart* character = PartOf(this);
    u32 kind = character->bits.attackKind;
    if (kind == AttackThrownFromJump || kind == AttackThrownFromSpin)
    {
        character->bits.invulnerable = 0;
        kind = character->bits.attackKind;
    }

    if (kind != AttackSpin && kind != AttackSpinVariant && tell != 0)
    {
        u32 event;
        if (kind == AttackSlam || kind == AttackSlamVariant)
        {
            event = EventKneeDropLand;
        }
        else if (character->moveBits.fellFar != 0)
        {
            event = EventLandFromFar;
            if (look != nullptr)
            {
                look->restSeconds = LandFromFarRestSeconds;
            }
        }
        else
        {
            event = character->flags.moving != 0 ? EventLandMoving : EventLand;
        }

        RunAgentEvent(this, event, reinterpret_cast<u32>(instance), 0, 0);
    }

    character->moveBits.noFallEvents = 0;
    CreatureAgent::Land(tell);
    state.noFallEvents = 0;
}

u32 CharacterAgent::Link(CharacterAgent* other)
{
    Unlink();
    link = CharacterLink::Construct(Allocate<CharacterLink>(), this, other, 1);
    other->link = CharacterLink::Construct(Allocate<CharacterLink>(), other, this, 0);
    if (link != nullptr)
    {
        AttachToModelAnimator(link, instance);
        PartOf(this)->moveBits.linkedFirst = 1;
        RunAgentEvent(this, EventLinked, 0, 0, 0);
    }

    if (other->link != nullptr)
    {
        AttachToModelAnimator(other->link, other->instance);
        PartOf(other)->moveBits.linkedSecond = 1;
        RunAgentEvent(other, EventLinked, 0, 0, 0);
    }

    linkTime = GetContextClock(instance)->time;
    return 1;
}

void CharacterAgent::Unlink()
{
    if (link == nullptr)
    {
        return;
    }

    CharacterPart* character = PartOf(this);
    CharacterAgent* second = link->Second();
    DetachFromModelAnimator(link, instance);
    if (link != nullptr)
    {
        link->DestroyVirtual(DestroyAndFree);
    }

    link = nullptr;
    character->moveBits.linkedFirst = 0;
    if (walk != nullptr)
    {
        walk->ResetCopy();
    }

    RunAgentEvent(this, EventUnlinked, 0, 0, 0);
    // Retail bug: the second isn't checked for none (gone with its instance: low memory read and written)
    if (second->link == nullptr)
    {
        return;
    }

    CharacterPart* secondPart = PartOf(second);
    DetachFromModelAnimator(second->link, second->instance);
    if (second->link != nullptr)
    {
        second->link->DestroyVirtual(DestroyAndFree);
    }

    second->link = nullptr;
    secondPart->moveBits.linkedSecond = 0;
    if (second->walk != nullptr)
    {
        second->walk->ResetCopy();
    }

    RunAgentEvent(second, EventUnlinked, 0, 0, 0);
}

u32 CharacterAgent::MoveModeOf()
{
    switch (PartOf(this)->bits.attackKind)
    {
    case AttackSlam:
    case AttackSlamVariant:
        return MoveSlam;
    case AttackSlide:
    case AttackSlideVariant:
        return MoveSlide;
    case AttackSpin:
    case AttackSpinVariant:
        return MoveSpin;
    case AttackTied:
        return MoveLinked;
    case AttackThrownFromSpin:
    case AttackThrownFromJump:
        return MoveThrown;
    default:
        return MoveNone;
    }
}

void CharacterAgent::SetUp(u32 made)
{
    s32 character = properties->GetInt(CharacterKindProperty);
    state.dead = 0;
    state.boxOnly = 0;
    linkTime = 0;
    u16 model;
    GetObjectModelId(&model, object, 0);
    SetVideoModelInstance(G_VideoController, model & ResourceIndexMask, instance);
    if (made != 0 && character != CharacterNone)
    {
        proceduralJoints = Make<ProceduralJoints>(this);
        look = Make<LookController>(this);
    }

    crouch = nullptr;
    claw = nullptr;
    jump = nullptr;
    link = nullptr;
    gun = nullptr;
    spin = nullptr;
    vehicle = nullptr;
    walk = nullptr;
    body = Make<CharacterBody>(this);
    switch (character)
    {
    case CharacterCrash:
    case CharacterTallCrash:
        crouch = Make<CrouchController>(this);
        jump = Make<JumpController>(this);
        spin = Make<SpinController>(this);
        walk = Make<WalkController>(this);
        break;
    case CharacterCortex:
        crouch = Make<CrouchController>(this);
        jump = Make<JumpController>(this);
        gun = Make<Gun>(this);
        walk = Make<WalkController>(this);
        break;
    case CharacterNina:
        claw = Make<ClawController>(this);
        jump = Make<JumpController>(this);
        spin = Make<SpinController>(this);
        walk = Make<WalkController>(this);
        break;
    case CharacterMecha:
        jump = Make<JumpController>(this);
        gun = Make<Gun>(this);
        spin = Make<SpinController>(this);
        walk = Make<WalkController>(this);
        break;
    default:
        break;
    }
}

void CharacterAgent::TearDown(u32 destroyed)
{
    if (crouch != nullptr)
    {
        crouch->Destroy(DestroyAndFree);
    }

    if (claw != nullptr)
    {
        DestroyClaw(claw);
    }

    if (jump != nullptr)
    {
        jump->Destroy(DestroyAndFree);
    }

    if (link != nullptr)
    {
        link->DestroyVirtual(DestroyAndFree);
    }

    if (gun != nullptr)
    {
        DestroyGun(gun);
    }

    if (spin != nullptr)
    {
        spin->Destroy(DestroyAndFree);
    }

    if (destroyed != 0 && vehicle != nullptr)
    {
        vehicle->ForgetOtherVirtual();
    }

    if (vehicle != nullptr)
    {
        vehicle->DestroyVirtual(DestroyAndFree);
    }

    if (walk != nullptr)
    {
        walk->Destroy(DestroyAndFree);
    }

    if (body != nullptr)
    {
        body->Destroy(DestroyAndFree);
    }

    crouch = nullptr;
    claw = nullptr;
    jump = nullptr;
    link = nullptr;
    gun = nullptr;
    spin = nullptr;
    vehicle = nullptr;
    walk = nullptr;
    body = nullptr;
    if (destroyed == 0)
    {
        return;
    }

    if (proceduralJoints != nullptr)
    {
        proceduralJoints->DestroyVirtual(DestroyAndFree);
    }

    if (look != nullptr)
    {
        look->DestroyVirtual(DestroyAndFree);
    }

    look = nullptr;
    proceduralJoints = nullptr;
}

void CharacterAgent::PushBack(f32 gravity, const Vector4* push, u32 event, InstanceContext* space)
{
    // A push in an instance's space is taken through the character's own place (every caller passes its own instance)
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Vector4 way;
    if (space == nullptr)
    {
        way = *push;
    }
    else
    {
        VuRotateVector(&place->matrix, push, &way);
    }

    Vector4 up = *RowOf(&place->matrix, 1);
    f32 upward = way.x * up.x + way.y * up.y + way.z * up.z;
    if (jump != nullptr)
    {
        jump->QueueLaunch(upward, gravity, event);
    }

    if (walk != nullptr)
    {
        Vector4 level = way;
        level.x -= up.x * upward;
        level.y -= up.y * upward;
        level.z -= up.z * upward;
        if (__builtin_fabsf(level.x) <= PushEpsilon && __builtin_fabsf(level.y) <= PushEpsilon &&
            __builtin_fabsf(level.z) <= PushEpsilon)
        {
            velocity = g_DefaultBox.min;
            velocity.w = 1.0f;
        }
        else
        {
            walk->PushAtVelocity(&level, &level, 0);
        }
    }

    if (vehicle != nullptr)
    {
        vehicle->PushVirtual(push, space);
    }
}

void CharacterAgent::KnockBack(InstanceContext* from)
{
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    CharacterPart* character = PartOf(this);
    f32 gravity = properties->GetFloat(JumpController::PropGravity);
    ObjectPlace* fromPlace = from->place;
    fromPlace->SyncPosition();
    Vector4 source = fromPlace->position;
    Vector4 away = *RowOf(&place->matrix, 3);
    Vector4 lead;
    if (vehicle == nullptr)
    {
        lead = {velocity.x * KnockLead, velocity.y * KnockLead, velocity.z * KnockLead, 1.0f};
    }
    else
    {
        Vector4 moving;
        CallVirtual<u32>(this, vtable, VelocitySlot, &moving);
        lead = {moving.x * KnockLead, moving.y * KnockLead, moving.z * KnockLead, 1.0f};
    }

    away.x = away.x - lead.x - source.x;
    away.z = away.z - lead.z - source.z;
    away.y = 0.0f;
    s32 angle;
    f32 square = away.x * away.x + away.z * away.z;
    if (square < LevelEpsilon)
    {
        AngleFrom(&angle, away.y, AngleRadians);
    }
    else
    {
        f32 scale = KnockDistance / Kept(__builtin_sqrtf(square));
        away.z = away.z * scale;
        away.x = away.x * scale;
        SignedAngleAboutY(&angle, RowOf(&place->matrix, 2), &away);
    }

    away.y = KnockUpSpeed;
    u32 event;
    if (angle < g_AgentAngleMinus135 || g_AgentAngle135 < angle)
    {
        event = EventKnockedBack;
    }
    else if (angle < g_AgentAngleMinus45)
    {
        event = EventKnockedNegative;
    }
    else
    {
        event = g_AgentAngle45 < angle ? EventKnockedPositive : EventKnockedForward;
    }

    character->moveBits.hurt = 1;
    PushBack(gravity, &away, event, nullptr);
}

void CharacterAgent::Contact(const ContactMessage* message, InstanceContext* sender, u32)
{
    u32 gameState = g_AgentsGameController->State();
    if (gameState == GameController::StateWatching || gameState == GameController::StateTitle)
    {
        return;
    }

    u32 damage = message->damage;
    // The driver takes the hit first and its vehicle's other agent with it (a passenger's vehicle lacks bit 1: its other agent is
    // the driver). Retail bug: the hoverboard's and the wrestle's other agents (the board, the creature) are taken for
    // characters too, and get the message, the hit points, and the death or the knock-back
    CharacterAgent* hit = this;
    CharacterAgent* passenger = nullptr;
    if (vehicle != nullptr)
    {
        auto* other = static_cast<CharacterAgent*>(vehicle->other);
        if (vehicle->bits.drives != 0)
        {
            passenger = other;
        }
        else
        {
            hit = other;
            passenger = this;
        }
    }

    CharacterPart* hitPart = PartOf(hit);
    u32 points = hitPart->flags.hitPoints;
    if (points == 0)
    {
        return;
    }

    if (hit->Invincible() != 0 && damage < InvincibleDamage)
    {
        return;
    }

    KeepContact(hit, message);
    u32 left = damage < points ? points - damage : 0;
    hitPart->flags.hitPoints = left;
    if (passenger != nullptr)
    {
        CharacterPart* passengerPart = PartOf(passenger);
        KeepContact(passenger, message);
        passengerPart->flags.hitPoints = left;
    }

    if (left == 0)
    {
        Die(hit, sender);
        if (passenger != nullptr)
        {
            Die(passenger, sender);
        }

        return;
    }

    if (left == points)
    {
        return;
    }

    hit->KnockBack(sender);
    StartHurtInvincibility(hit);
    if (passenger != nullptr)
    {
        passenger->KnockBack(sender);
        StartHurtInvincibility(passenger);
    }
}

void CharacterAgent::Bumped(InstanceContext* other, const Vector4* motion, const Vector4* normal)
{
    Vector4 unit = *normal;
    f32 inverse = InverseLength(&unit, LengthEpsilon);
    unit.x = unit.x * inverse;
    unit.y = unit.y * inverse;
    unit.z = unit.z * inverse;
    if (unit.y < -Vertical && motion->y < -1.0f)
    {
        Vector4 down = {0.0f, -1.0f, 0.0f, 1.0f};
        SendAttack(other, &down);
        return;
    }

    if (Vertical < unit.y)
    {
        Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
        SendAttack(other, &up);
        return;
    }

    SendAttack(other, &g_DefaultBox.min);
}

void CharacterAgent::LeaveFootprints(u32 kind)
{
    // (Retail copies a vector its callers pass after the kind and never reads the copy)
    Vector4 offset = {0.0f, 0.0f, 0.0f, 1.0f};
    if (properties->GetInt(CharacterKindProperty) == CharacterCrash)
    {
        offset.x = CrashFootSide;
        offset.z = FootAhead;
    }
    else if (properties->GetInt(CharacterKindProperty) == CharacterCortex)
    {
        offset.x = CortexFootSide;
        offset.z = FootAhead;
    }

    ExitPointAnimation* left = UpdatedExitPoint(instance, ExitPointLeftFoot);
    ExitPointAnimation* right = UpdatedExitPoint(instance, ExitPointRightFoot);
    if (left == nullptr || right == nullptr)
    {
        return;
    }

    AddInstanceDecal(kind, instance, right, &offset);
    offset.x = -offset.x;
    AddInstanceDecal(kind, instance, left, &offset);
}

u32 CharacterAgent::TakeVehicle(Vehicle* taken)
{
    buttons.locked.value = 0;
    SetAttackKind(PartOf(this), AttackWalkInto);
    vehicle = taken;
    if (crouch != nullptr)
    {
        crouch->Destroy(DestroyAndFree);
    }

    if (claw != nullptr)
    {
        DestroyClaw(claw);
    }

    if (jump != nullptr)
    {
        jump->Destroy(DestroyAndFree);
    }

    if (taken->KeepsGunVirtual() == 0)
    {
        if (gun != nullptr)
        {
            DestroyGun(gun);
        }

        gun = nullptr;
    }

    if (spin != nullptr)
    {
        spin->Destroy(DestroyAndFree);
    }

    if (walk != nullptr)
    {
        walk->Destroy(DestroyAndFree);
    }

    if (body != nullptr)
    {
        body->Destroy(DestroyAndFree);
    }

    crouch = nullptr;
    claw = nullptr;
    jump = nullptr;
    spin = nullptr;
    walk = nullptr;
    body = nullptr;
    RunAgentEvent(this, EventVehicleTaken, 0, 1, 0);
    // (The controls node isn't checked for none)
    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    controls->bits.motionDriven = 0;
    ClearFall(this);
    LetGoOfStanding();
    u32 moved = ChangeHeight(-heightOffset);
    keptHeightOffset = 0.0f;
    return moved;
}

void CharacterAgent::SetVehicle(u32 kind, Agent* other, u32 armed)
{
    Unlink();
    Vector4 speed;
    if (vehicle != nullptr)
    {
        vehicle->VelocityVirtual(&speed);
        LeaveVehicle(1);
        if (vehicle != nullptr)
        {
            vehicle->DestroyVirtual(DestroyAndFree);
        }
    }
    else
    {
        speed = velocity;
        if (__builtin_sqrtf(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z) < VehicleSpeed)
        {
            ObjectPlace* place = instance->place;
            RotateAndTranslate(place);
            speed = *RowOf(&place->matrix, 2);
            speed.x = speed.x * VehicleSpeed;
            speed.y = speed.y * VehicleSpeed;
            speed.z = speed.z * VehicleSpeed;
        }
    }

    auto* character = static_cast<CharacterAgent*>(other);
    Vehicle* made = nullptr;
    VehicleControls* controls = nullptr;
    switch (kind)
    {
    case Vehicle::KindRollerbrawl:
        made = RollerbrawlVehicle::Construct(Allocate<RollerbrawlVehicle>(), this, character);
        controls = VehicleControls::Construct(Allocate<VehicleControls>(), 0);
        break;
    case Vehicle::KindHumiliskate:
        made = HumiliskateVehicle::Construct(Allocate<HumiliskateVehicle>(), this, character);
        controls = VehicleControls::Construct(Allocate<VehicleControls>(), 0);
        break;
    case Vehicle::KindHoverboard:
        made = HoverboardVehicle::Construct(Allocate<HoverboardVehicle>(), this, other, armed);
        controls = VehicleControls::Construct(Allocate<VehicleControls>(), armed);
        break;
    case Vehicle::KindWrestle:
        made = WrestleVehicle::Construct(Allocate<WrestleVehicle>(), this, other);
        controls = VehicleControls::Construct(Allocate<VehicleControls>(), 0);
        break;
    case Vehicle::KindWallCling:
        made = WallClingVehicle::Construct(Allocate<WallClingVehicle>(), this);
        controls = VehicleControls::Construct(Allocate<VehicleControls>(), 0);
        break;
    default:
        break;
    }

    if (made != nullptr)
    {
        Vector4 min;
        Vector4 max;
        ObjectCollision* collision = &instance->collision;
        made->CollisionBox(&min, &max);
        SetCollisionBox(collision, &min, &max, 1, 0);
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 position = place->position;
        made->SetBodyPosition(&position);
        made->SetVelocityVirtual(&speed);
        TakeVehicle(made);
    }

    auto* ownControls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    if (ownControls != nullptr)
    {
        ReplaceControlsHandler(ownControls, controls);
    }

    // The other character rides along on the Rollerbrawl and the Humiliskate, without its controls
    PassengerVehicle* passenger = nullptr;
    if (kind == Vehicle::KindRollerbrawl || kind == Vehicle::KindHumiliskate)
    {
        passenger = PassengerVehicle::Construct(Allocate<PassengerVehicle>(), Vehicle::KindPassenger, character, this);
    }

    if (passenger != nullptr)
    {
        character->TakeVehicle(passenger);
    }

    if (other == nullptr)
    {
        return;
    }

    auto* otherControls = static_cast<ControlsNode*>(GetGameNode(&other->instance->nodes, NodeControls));
    if (otherControls != nullptr)
    {
        ReplaceControlsHandler(otherControls, nullptr);
        otherControls->bits.motionDriven = 0;
    }
}

void CharacterAgent::LeaveVehicle(u32 replaced)
{
    if (vehicle == nullptr)
    {
        return;
    }

    const Matrix4x4* exit = &vehicle->exitMatrix;
    u32 kind = vehicle->Kind();
    // The Rollerbrawl and the Humiliskate it drives put the other character off as well
    if (vehicle->bits.drives != 0 && (kind == Vehicle::KindRollerbrawl || kind == Vehicle::KindHumiliskate))
    {
        auto* passenger = static_cast<CharacterAgent*>(vehicle->other);
        if (passenger->vehicle != nullptr)
        {
            StepOff(passenger, exit, replaced, true);
        }
    }

    StepOff(this, exit, replaced, false);
}

u32 CharacterAgent::CanChangeChunk(ChunkData* from, ChunkLinkData* chunkLink)
{
    if (chunkLink->flags.linkedRm2Loaded == 0)
    {
        if (vehicle == nullptr)
        {
            RunAgentEvent(this, EventLandFromFar, reinterpret_cast<u32>(instance), 0, 0);
            return 0;
        }

        return vehicle->CanChangeChunkVirtual(from, chunkLink);
    }

    // The player's colour filter takes the linked chunk's palette
    InstanceContext* player = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    if (instance == player && chunkLink->linkedData != nullptr)
    {
        u32 palette = chunkLink->linkedData->colourFilterPalette;
        g_ColourFilterOn = 1;
        g_ColourFilterAmount = 1.0f;
        g_ColourFilterSecondPalette = palette;
        g_ColourFilterBlends = 0;
        g_ColourFilterPalette = palette;
    }

    OgiAnimator* animator = AnimatorOf(instance);
    TransformVectorThroughLink(chunkLink, &groundPoint, 1);
    TransformVectorThroughLink(chunkLink, &fallStart, 1);
    TransformVectorThroughLink(chunkLink, &linkedPoint, 0);
    MoveAnimatorThroughLink(animator, from, chunkLink);
    if (link != nullptr)
    {
        link->CanChangeChunk(from, chunkLink);
    }

    if (vehicle != nullptr)
    {
        vehicle->CanChangeChunkVirtual(from, chunkLink);
    }

    if (proceduralJoints != nullptr)
    {
        proceduralJoints->ChangeChunk(from, chunkLink);
    }

    if (properties->GetInt(CharacterKindProperty) == CharacterCortex)
    {
        // An asleep AgentRef2 is let go of unless the node keeps it; the object he carries goes through with him
        auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
        InstanceContext* carried = node->agentRef2;
        if (carried != nullptr && carried->flags.asleep &&
            !node->flags.keepsAgentRef2)
        {
            node->agentRef2 = nullptr;
        }

        carried = node->agentRef2;
        if (carried != nullptr)
        {
            auto* carriedNode = static_cast<ObjectNode*>(GetGameNode(&carried->nodes, NodeObject));
            if (carriedNode != nullptr && TakesPackets(carriedNode))
            {
                u16 id;
                CopyShort(&id, &carriedNode->agent->objectId);
                if ((id & ResourceIndexMask) == CortexCarriedObject)
                {
                    carried->ChangeChunk(chunkLink);
                }
            }
        }
    }

    return CreatureAgent::CanChangeChunk(from, chunkLink);
}

void CharacterAgent::Freeze()
{
    ClearFall(this);
    LetGoOfStanding();
    contact.hitKinds = 0;
    contact.damage = 0;
    contact.point = g_DefaultBox.min;
    instance->flags.collisionActive = 0;
    PartOf(this)->bits.frozen = 1;
    auto* node = static_cast<GameNode*>(GetGameNode(&instance->nodes, NodeObject));
    CallVirtual<void>(node, node->vtable, ObjectNode::ReleasePartsSlot);
    CallVirtual<void>(node, node->vtable, ObjectNode::DoNothingSlot);
    CallVirtual<void>(node, node->vtable, ObjectNode::StopRunnersSlot, 1u);
}

void CharacterAgent::Unfreeze()
{
    // Turned upright, facing where it faced (its up axis' way when it faced straight up or down)
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    Vector4 facing = *RowOf(&place->matrix, 2);
    facing.y = 0.0f;
    if (facing.x * facing.x + facing.z * facing.z < LevelEpsilon)
    {
        place = instance->place;
        RotateAndTranslate(place);
        facing = *RowOf(&place->matrix, 1);
        facing.y = 0.0f;
    }

    f32 inverse = InverseLength(&facing, LengthEpsilon);
    facing.x = facing.x * inverse;
    facing.y = facing.y * inverse;
    facing.z = facing.z * inverse;
    Matrix4x4 upright;
    MatrixFacing(&upright, &facing);
    InstanceContext* own = instance;
    ObjectPlace* ownPlace = own->place;
    ownPlace->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &upright);
    if (ownPlace->TurnTo(&rotation))
    {
        QueueObject(own);
    }

    PartOf(this)->bits.frozen = 0;
    instance->flags.collisionActive = 1;
}

void CharacterAgent::StartFalling(u32 tell)
{
    CharacterPart* character = PartOf(this);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    fallStart = place->position;
    if (tell != 0)
    {
        RunAgentEvent(this, EventShortFall, reinterpret_cast<u32>(instance), 0, 0);
    }

    character->moveBits.fellFar = 0;
    character->moveBits.falling = 1;
    CreatureAgent::StartFalling(tell);
}

void CharacterAgent::ResetFall()
{
    ClearFall(this);
}

void CharacterAgent::Remake()
{
    TearDown(1);
    SetUp(1);
}

void CharacterAgent::Recover()
{
    ClearFall(this);
    if (proceduralJoints != nullptr)
    {
        proceduralJoints->Still();
    }
}

void CharacterAgent::PlayRideSound(f32 strength)
{
    if (vehicle == nullptr || vehicle->other == nullptr)
    {
        return;
    }

    auto* node = static_cast<ObjectNode*>(GetGameNode(&vehicle->other->instance->nodes, NodeObject));
    if (node == nullptr || !TakesPackets(node) || node->contactSoundFirst == 0)
    {
        return;
    }

    PlayContactSound(strength, node);
}

void CharacterAgent::Launch(f32 strength, const Vector4* velocity, InstanceContext* space)
{
    PushBack(strength, velocity, EventStandingJump, space);
}
