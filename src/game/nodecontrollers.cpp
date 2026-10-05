#include "game/nodecontrollers.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/attachments.h"
#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/particles.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/sound.h"
#include "game/string.h"
#include "game/vehicles.h"

#include <cstddef>
#include <cstdint>

// The controllers command 645 gives object nodes: the final boss's weapons, Aku Aku's mask, a path follower and the Humiliskate's
// trails and sounds

extern "C"
{
    void PlaySlotAnimation(f32 blendSeconds, ObjectNode* node, u32 slot, u32 loops) RETAIL_N32(FUN_002124c0);
    // The mask's states' names and the hit points' label
    extern const char g_MaskInactiveText[] RETAIL(D_002EDE28);
    extern const char g_MaskArrivingText[] RETAIL(D_002EDE38);
    extern const char g_MaskLeavingText[] RETAIL(D_002EDE48);
    extern const char g_MaskGotMaskText[] RETAIL(D_002EDE58);
    extern const char g_MaskGotBoostedMaskText[] RETAIL(D_002EDE68);
    extern const char g_MaskInvincibleText[] RETAIL(D_002EDE80);
    extern const char g_MaskInvalidText[] RETAIL(D_002EDE90);
    extern const char g_HitPointsText[] RETAIL(D_003098F0);
}


namespace
{
// The character's exit point the mask hangs on while it's invincible
constexpr u32 MaskExitPoint = 1;
// The player's hit points: one mask (it comes and follows) from 2, two (it shines) from 3, and it doesn't come for more than 4
constexpr u32 MaskHitPoints = 2;
constexpr u32 BoostedMaskHitPoints = 3;
constexpr u32 MostMaskHitPoints = 4;
// The mask's object's animation slots (on the character, shining, flying away or onto the character) and sound slots
enum MaskAnimation : u32
{
    AnimationMask = 0,
    AnimationBoosted = 1,
    AnimationFlying = 2,
};
enum MaskSound : u32
{
    SoundArrived = 0,
    SoundLeaving = 1,
    SoundBoosted = 2,
    SoundUnboosted = 3,
    SoundInvincible = 4,
    SoundInvincibilityOver = 6,
};
// It comes from 20 above and 3 behind the character in a quarter of a second (and leaves to there in a second), and flies onto
// it in an eighth
constexpr f32 AwayHeight = 20.0f;
constexpr f32 AwayBehind = 3.0f;
constexpr f32 ArrivingSpeed = 4.0f;
constexpr f32 FlyingOnSpeed = 8.0f;
// Where it follows the character: 0.7 to its side and 2 above it
constexpr Vector4 FollowOffset = {0.7f, 2.0f, 0.0f, 1.0f};
// How far along the exit point's z axis (and how far below) it flies to and hangs while invincible, and how much the character's
// joints are squashed then
constexpr f32 FlyToAlong = Rounded(0.15);
constexpr f32 HangAlong = Rounded(0.1);
constexpr f32 HangBelow = Rounded(0.2);
constexpr f32 InvincibleSquash = 0.5f;

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

CharacterAgent* PlayerAgent()
{
    return reinterpret_cast<CharacterAgent*>(g_PlayerCharacter2);
}

const CharacterPart* PlayerPart()
{
    return g_PlayerPart2;
}

u32 PlayerHitPoints()
{
    return PlayerPart()->flags.hitPoints;
}

bool PlayerInvincible()
{
    return PlayerPart()->bits.invulnerable != 0;
}

// The game's title or watching state, which the controllers sit out
bool GameWatched()
{
    u32 state = g_ConditionsGameController->State();
    return state == GameController::StateWatching || state == GameController::StateTitle;
}

// An instance's place, flags and nodes the way retail reaches them: a missing one reads the words at the start of memory
ObjectPlace* PlaceOf(const ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

ReferencedObjectFlags FlagsOf(const ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, flags);
    return *reinterpret_cast<const ReferencedObjectFlags*>(address);
}

NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

f32 SecondsOf(InstanceContext* instance)
{
    return static_cast<f32>(static_cast<s32>(GetContextClock(instance)->advance)) * g_SecondsPerClockUnit;
}

// The smooth step 3s² - 2s³, as retail works it out
f32 Ease(f32 s)
{
    return s * (s * 3.0f) - (s + s) * s * s;
}

void DestroyParticleTrails(ObjectNode* node)
{
    CallVirtual<void>(node, node->vtable, ObjectNode::DestroyParticleTrailsSlot);
}

// The matrix of the character's exit point 1 in the world (the character's place's without it)
const Matrix4x4* CharacterExitPoint(InstanceContext* character)
{
    auto* model = static_cast<ModelNode*>(GetGameNode(NodesOf(character), NodeModel));
    SizedArray<ExitPointAnimation*>* exitPoints = model->animator->exitPoints;
    ExitPointAnimation* exitPoint = exitPoints != nullptr ? exitPoints->data[MaskExitPoint] : nullptr;
    return &UpdateExitPointMatrix(exitPoint)->matrix;
}

// The rotation a matrix turns by, unit long
Vector4 UnitRotationOf(const Matrix4x4* matrix)
{
    Vector4 rotation;
    GetRotationVec(&rotation, matrix);
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, &rotation);
    rotation.x = rotation.x * inverse;
    rotation.y = rotation.y * inverse;
    rotation.z = rotation.z * inverse;
    rotation.w = rotation.w * inverse;
    return rotation;
}
}

// A joint's matrix (its pose's in the world) turned a share of the way from the one it's at toward the facing, or back toward the
// pose while it returns (resting once it's there), or left alone while it rests; the root joint's taken through its own matrix,
// then scaled. The place isn't read
void PoseAimedJoint(AimedJoint* aimed, Matrix4x4* matrix, const Matrix4x4* facing, const Matrix4x4* local, ObjectPlace*)
{
    constexpr u8 RootJointId = 0;
    constexpr f32 TurnsPerSecond = 20.0f;
    // How close to its pose a returning joint has to be to rest
    constexpr f32 RestingTolerance = 0.01f;
    if (aimed->resting != 0)
    {
        return;
    }

    if (aimed->retakesInverse != 0)
    {
        aimed->inverse = *local;
        VuInvertRigidInPlace(&aimed->inverse);
        aimed->retakesInverse = 0;
    }

    if (aimed->rate == 0.0f)
    {
        *matrix = aimed->matrix;
        return;
    }

    if (aimed->rate == 1.0f)
    {
        *matrix = *facing;
        aimed->matrix = *matrix;
        return;
    }

    if (aimed->retakesMatrix != 0)
    {
        aimed->matrix = *matrix;
        aimed->retakesMatrix = 0;
    }

    Matrix4x4 relative = aimed->inverse;
    MultiplyInPlace(&relative, local);
    Vector4 from = UnitRotationOf(&aimed->matrix);
    Vector4 to;
    if (aimed->returning != 0)
    {
        to = UnitRotationOf(matrix);
        if (SameRotation(&to, &from, RestingTolerance) != 0)
        {
            aimed->returning = 0;
            aimed->resting = 1;
        }
    }
    else
    {
        to = UnitRotationOf(facing);
    }

    f32 seconds = SecondsOf(aimed->node->owner);
    SlerpRotations(aimed->rate * TurnsPerSecond * seconds, &from, &from, &to);
    MatrixFromRotation(&aimed->matrix, &from);
    *matrix = aimed->matrix;
    if (aimed->id == RootJointId)
    {
        PreMultiply(matrix, &relative);
    }

    if (aimed->scale != 1.0f)
    {
        Vector4 scale = {aimed->scale, aimed->scale, aimed->scale, 1.0f};
        MatrixScaleColumns(matrix, &scale);
    }
}

void JointAimer::Destroy(u32 destroyFlags)
{
    vtable = g_JointHookVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void JointAimer::Attach(OgiAnimator* animator)
{
    for (AimedJoint& aimed : joints)
    {
        aimed.retakesMatrix = 1;
    }

    for (AimedJoint& aimed : joints)
    {
        AddJointCallback(animator, aimed.id, this);
    }
}

void JointAimer::Detach(OgiAnimator* animator)
{
    for (AimedJoint& aimed : joints)
    {
        RemoveJointCallback(animator, aimed.id, this);
    }
}

void JointAimer::DetachFromModel()
{
    if (node == nullptr)
    {
        return;
    }

    auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
    DetachVirtual(model->animator);
}

// The joint's matrix made (its animation's, marked done), turned toward the target in the world and taken back into the
// instance's space, keeping its own translation
u32 JointAimer::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    u8 id = animator->animation->joint->id;
    AimedJoint* aimed = nullptr;
    for (AimedJoint& candidate : joints)
    {
        if (candidate.id == id)
        {
            aimed = &candidate;
            break;
        }
    }

    if (aimed == nullptr)
    {
        return 0;
    }

    Matrix4x4 local;
    CreateJointTransform(animator, animator->animation->parent, 1, &local);
    Vector4 translation = *RowOf(&local, 3);
    Matrix4x4 world = local;
    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    Matrix4x4 toInstance = place->matrix;
    VuInvertRigidInPlace(&toInstance);
    MultiplyInPlace(&world, &place->matrix);
    Vector4 aim;
    if (target == TargetPosition)
    {
        aim = targetPosition;
    }
    else if (target == TargetInstance)
    {
        ObjectPlace* targetPlace = targetInstance->place;
        targetPlace->SyncPosition();
        aim = targetPlace->position;
    }
    else
    {
        return 0;
    }

    Vector4 direction = {aim.x - world.m[3][0], aim.y - world.m[3][1], aim.z - world.m[3][2], 1.0f};
    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    Matrix4x4 facing;
    MatrixFacing(&facing, &direction);
    // Retail takes the facing into the instance's space too and never uses it
    Matrix4x4 unused = facing;
    MultiplyInPlace(&unused, &toInstance);
    *matrix = world;
    PoseAimedJoint(aimed, matrix, &facing, &local, place);
    MultiplyInPlace(matrix, &toInstance);
    *RowOf(matrix, 3) = translation;
    return 1;
}

JointAimController* JointAimController::Construct(JointAimController* controller, ObjectNode* node, u32 kind)
{
    // The share of the way the joints turn (times 20 a second)
    constexpr f32 TurnRate = Rounded(0.1);
    controller->kind = static_cast<u8>(kind);
    controller->node = node;
    controller->vtable = g_JointAimControllerVTable;
    controller->aimer.vtable = g_JointAimerVTable;
    controller->aimer.node = nullptr;
    for (AimedJoint& aimed : controller->aimer.joints)
    {
        aimed.id = GameOGI::NoJoint;
        aimed.returning = 0;
        aimed.resting = 1;
        aimed.retakesMatrix = 0;
        aimed.retakesInverse = 1;
        aimed.scale = 1.0f;
        aimed.rate = TurnRate;
    }

    return controller;
}

void JointAimController::Destroy(u32 destroyFlags)
{
    aimer.vtable = g_JointHookVTable;
    vtable = g_NodeControllerVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void JointAimController::Start()
{
}

void JointAimController::Frame(TimeClock*)
{
}

// A retail bug: only a restart takes the joint hook off the model, stopping and destroying the controller leave it there (a
// controller given in its place leaves the animator posing joints through a freed hook)
void JointAimController::Restart(u32)
{
    aimer.DetachFromModel();
}

void JointAimController::Stop()
{
}

MaskController* MaskController::Construct(MaskController* mask, ObjectNode* node, u32 kind)
{
    mask->kind = static_cast<u8>(kind);
    mask->node = node;
    mask->vtable = g_MaskControllerVTable;
    ConstructTrailArguments(&mask->boostTrail);
    ConstructTrailArguments(&mask->unusedTrail);
    for (TrailArguments& trail : mask->trails)
    {
        ConstructTrailArguments(&trail);
    }

    mask->state = StateInactive;
    for (u16& id : mask->ids)
    {
        id = NoParticleSystem;
    }

    mask->flags.idCount = 0;
    mask->flags.hidden = 0;
    mask->flags.unused7 = 0;
    for (s32& slot : mask->trailSlots)
    {
        slot = -1;
    }

    return mask;
}

void MaskController::Destroy(u32 destroyFlags)
{
    for (s32 index = TrailCount - 1; index >= 0; index--)
    {
        DestroyTrailArguments(&trails[index], 0);
    }

    DestroyTrailArguments(&unusedTrail, DestroyOnly);
    DestroyTrailArguments(&boostTrail, DestroyOnly);
    vtable = g_NodeControllerVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void MaskController::Start()
{
    Hide();
    FindCharacter();
    state = StateInactive;
}

void MaskController::Frame(TimeClock*)
{
    if (GameWatched())
    {
        if (!flags.hidden)
        {
            flags.unused7 = FlagsOf(character).visible;
            Hide();
            flags.hidden = 1;
        }
    }
    else if (flags.hidden)
    {
        if (state != StateInactive)
        {
            node->owner->flags.visible = 1;
        }

        flags.hidden = 0;
        flags.unused7 = 0;
    }

    switch (state)
    {
    case StateInactive:
        StepInactive();
        break;
    case StateArriving:
        StepArriving();
        break;
    case StateLeaving:
        StepLeaving();
        break;
    case StateGotMask:
        StepGotMask();
        break;
    case StateGotBoostedMask:
        StepGotBoostedMask();
        break;
    case StateInvincible:
        StepInvincible();
        break;
    default:
        break;
    }
}

void MaskController::Restart(u32)
{
}

// Its instance unhung from the character and released (once)
void MaskController::Stop()
{
    InstanceContext* mask = node->owner;
    if (mask->flags.released)
    {
        return;
    }

    void* attachments = GetGameNode(NodesOf(character), NodeAttachments);
    if (attachments != nullptr)
    {
        UnlinkInstance(attachments, mask, 0, 1, 0);
    }

    mask = node->owner;
    CallVirtual<u32>(mask, mask->vtable, InstanceContext::ReleaseSlot);
}

void MaskController::Describe(String* text)
{
    switch (state)
    {
    case StateInactive:
        StringAssign(text, g_MaskInactiveText);
        break;
    case StateArriving:
        StringAssign(text, g_MaskArrivingText);
        break;
    case StateLeaving:
        StringAssign(text, g_MaskLeavingText);
        break;
    case StateGotMask:
        StringAssign(text, g_MaskGotMaskText);
        break;
    case StateGotBoostedMask:
        StringAssign(text, g_MaskGotBoostedMaskText);
        break;
    case StateInvincible:
        StringAssign(text, g_MaskInvincibleText);
        break;
    default:
        StringAssign(text, g_MaskInvalidText);
        break;
    }

    u32 hitPoints = PlayerHitPoints();
    StringAppend(text, g_HitPointsText);
    String number;
    StringConstructNumber(&number, hitPoints);
    StringAppend(text, number.string);
    StringDestroy(&number);
}

// The instance its instance's ID entry links (an instance without an ID reads the word at address 4)
void MaskController::FindCharacter()
{
    s32 id = node->owner->id;
    std::uintptr_t entry = id != -1 ? reinterpret_cast<std::uintptr_t>(&g_InstanceIds->entries[id]) : 0;
    character = *reinterpret_cast<InstanceContext* const*>(entry + offsetof(InstanceIds::Entry, spawner));
    auto* characterNode = static_cast<AgentNode*>(GetGameNode(NodesOf(character), NodeCharacter));
    switch (characterNode->agent->properties->GetInt(CharacterKindProperty))
    {
    case CharacterCortex:
    case CharacterNina:
    case CharacterMecha:
        flags.unused0 = 1;
        break;
    case CharacterNone:
        break;
    default:
        flags.unused0 = 0;
        break;
    }
}

void MaskController::Hide()
{
    DestroyParticleTrails(node);
    node->owner->flags.receivesTriggerSignals = 0;
    node->owner->flags.visible = 0;
}

void MaskController::StepInactive()
{
    if (character != PlayerInstance())
    {
        node->owner->flags.visible = 0;
        return;
    }

    u32 hitPoints = PlayerHitPoints();
    if (hitPoints >= MaskHitPoints && hitPoints <= MostMaskHitPoints)
    {
        Arrive();
    }
}

void MaskController::StepArriving()
{
    if (character != PlayerInstance())
    {
        Hide();
        state = StateInactive;
        return;
    }

    Vector4 target = offset;
    ObjectPlace* characterPlace = PlaceOf(character);
    RotateAndTranslate(characterPlace);
    VuTransformPoint(&characterPlace->matrix, &target, &target);
    InstanceContext* mask = node->owner;
    ObjectPlace* place = mask->place;
    place->SyncPosition();
    Vector4 position = place->position;
    progress = progress + SecondsOf(mask) * ArrivingSpeed;
    if (1.0f <= progress)
    {
        Arrived();
        return;
    }

    // Down along the way from where it starts, eased
    f32 along = distance * Ease(progress * 0.5f + 0.5f);
    Vector4 way = position;
    way.x = way.x - target.x;
    way.y = way.y - target.y;
    way.z = way.z - target.z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse * along + target.x;
    way.y = way.y * inverse * along + target.y;
    way.z = way.z * inverse * along + target.z;
    place = mask->place;
    place->SyncPosition();
    if (place->MoveTo(&way))
    {
        QueueObject(mask);
    }

    characterPlace = PlaceOf(character);
    characterPlace->SyncRotation();
    Vector4 rotation = characterPlace->rotation;
    place = mask->place;
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(mask);
    }
}

void MaskController::StepLeaving()
{
    if (character != PlayerInstance())
    {
        Hide();
        state = StateInactive;
        return;
    }

    Vector4 target = leavePoint;
    InstanceContext* mask = node->owner;
    ObjectPlace* place = mask->place;
    place->SyncPosition();
    Vector4 position = place->position;
    progress = progress + SecondsOf(mask);
    if (1.0f <= progress)
    {
        Hide();
        state = StateInactive;
        return;
    }

    f32 along = distance * (1.0f - Ease(progress * 0.5f));
    Vector4 way = position;
    way.x = way.x - target.x;
    way.y = way.y - target.y;
    way.z = way.z - target.z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse * along + target.x;
    way.y = way.y * inverse * along + target.y;
    way.z = way.z * inverse * along + target.z;
    place = mask->place;
    place->SyncPosition();
    if (place->MoveTo(&way))
    {
        QueueObject(mask);
    }

    ObjectPlace* characterPlace = PlaceOf(character);
    characterPlace->SyncRotation();
    Vector4 rotation = characterPlace->rotation;
    place = mask->place;
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(mask);
    }
}

void MaskController::StepGotMask()
{
    if (character != PlayerInstance())
    {
        Hide();
        state = StateInactive;
        return;
    }

    u32 hitPoints = PlayerHitPoints();
    if (hitPoints < MaskHitPoints)
    {
        Leave();
    }
    else if (hitPoints < BoostedMaskHitPoints)
    {
        Follow();
    }
    else
    {
        Boost();
    }
}

void MaskController::StepGotBoostedMask()
{
    if (character != PlayerInstance())
    {
        Hide();
        state = StateInactive;
        return;
    }

    if (PlayerHitPoints() < BoostedMaskHitPoints)
    {
        Unboost();
    }
    else if (PlayerInvincible())
    {
        BecomeInvincible();
    }
    else
    {
        Follow();
    }
}

// It flies to the character's exit point 1 (a little below it and along its axis) and hangs on it, its trails emitting from the
// character, visible while the character is; once on, the character's joints squashed
void MaskController::StepInvincible()
{
    if (character != PlayerInstance())
    {
        Hide();
        state = StateInactive;
        return;
    }

    if (PlayerHitPoints() < MaskHitPoints)
    {
        Leave();
        return;
    }

    if (!PlayerInvincible())
    {
        EndInvincibility();
        return;
    }

    exitPoint = *CharacterExitPoint(character);
    Vector4 shift = *RowOf(&exitPoint, 2);
    shift.x = shift.x * HangAlong;
    shift.y = shift.y * HangAlong - HangBelow;
    shift.z = shift.z * HangAlong;
    Vector4* at = RowOf(&exitPoint, 3);
    at->x = at->x + shift.x;
    at->y = at->y + shift.y;
    at->z = at->z + shift.z;
    InstanceContext* mask = node->owner;
    ObjectPlace* place = mask->place;
    place->SyncPosition();
    Vector4 position = place->position;
    auto* characterNode = static_cast<ObjectNode*>(GetGameNode(NodesOf(character), NodeObject));
    for (u32 index = 0; index < InvincibilityTrails; index++)
    {
        trailSlots[index] = UpdateTrail(&trails[index], characterNode, trailSlots[index]);
    }

    if (FlagsOf(character).visible)
    {
        mask->flags.visible = 1;
    }
    else
    {
        mask->flags.visible = 0;
    }

    if (flags.onCharacter)
    {
        ProceduralJoints* joints = PlayerAgent()->proceduralJoints;
        if (joints != nullptr)
        {
            joints->SetSquash(InvincibleSquash);
        }

        return;
    }

    progress = progress + SecondsOf(mask) * FlyingOnSpeed;
    f32 along = distance * Ease(progress * 0.5f + 0.5f);
    Vector4 way = position;
    way.x = way.x - at->x;
    way.y = way.y - at->y;
    way.z = way.z - at->z;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse * along + at->x;
    way.y = way.y * inverse * along + at->y;
    way.z = way.z * inverse * along + at->z;
    place = mask->place;
    place->SyncPosition();
    if (place->MoveTo(&way))
    {
        QueueObject(mask);
    }

    place = mask->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &exitPoint);
    if (place->TurnTo(&rotation))
    {
        QueueObject(mask);
    }

    if (1.0f <= progress)
    {
        place = mask->place;
        place->SyncPosition();
        if (place->MoveTo(at))
        {
            QueueObject(mask);
        }

        flags.onCharacter = 1;
        void* attachments = GetGameNode(NodesOf(character), NodeAttachments);
        HangOnExitPoint(attachments, character, mask, MaskExitPoint, 0, 0, 1);
    }
}

// It starts 20 above and 3 behind the character, 0.7 to its side and 2 above it when it gets there
void MaskController::Arrive()
{
    offset = FollowOffset;
    progress = 0.0f;
    ObjectPlace* characterPlace = PlaceOf(character);
    characterPlace->SyncPosition();
    Vector4 at = characterPlace->position;
    Vector4 from = at;
    from.y = from.y + AwayHeight;
    from.z = from.z + AwayBehind;
    InstanceContext* mask = node->owner;
    ObjectPlace* place = mask->place;
    place->SyncPosition();
    if (place->MoveTo(&from))
    {
        QueueObject(mask);
    }

    f32 x = from.x - at.x;
    f32 y = from.y - at.y;
    f32 z = from.z - at.z;
    distance = __builtin_sqrtf(x * x + y * y + z * z);
    node->owner->flags.visible = 1;
    state = StateArriving;
}

void MaskController::Arrived()
{
    state = StateGotMask;
    PlaySlotAnimation(0.0f, node, AnimationMask, 1);
    PlaySlotSound(node, SoundArrived, 0);
}

// Back up to 20 above and 3 behind where the character is
void MaskController::Leave()
{
    progress = 0.0f;
    ObjectPlace* characterPlace = PlaceOf(character);
    characterPlace->SyncPosition();
    Vector4 at = characterPlace->position;
    leavePoint = at;
    leavePoint.y = leavePoint.y + AwayHeight;
    leavePoint.z = leavePoint.z + AwayBehind;
    state = StateLeaving;
    f32 x = at.x - leavePoint.x;
    f32 y = at.y - leavePoint.y;
    f32 z = at.z - leavePoint.z;
    distance = __builtin_sqrtf(x * x + y * y + z * z);
    PlaySlotAnimation(0.0f, node, AnimationFlying, 0);
    PlaySlotSound(node, SoundLeaving, 0);
    DestroyParticleTrails(node);
}

void MaskController::Boost()
{
    state = StateGotBoostedMask;
    PlaySlotAnimation(0.0f, node, AnimationBoosted, 1);
    PlaySlotSound(node, SoundBoosted, 0);
    DestroyParticleTrails(node);
    CallVirtual<u32>(node, node->vtable, ObjectNode::AddParticleTrailSlot, &boostTrail);
}

void MaskController::Unboost()
{
    state = StateGotMask;
    PlaySlotAnimation(0.0f, node, AnimationMask, 1);
    PlaySlotSound(node, SoundUnboosted, 0);
    DestroyParticleTrails(node);
}

// The character's exit point 1 taken, a bit further along its axis, to fly to from where the mask is
void MaskController::BecomeInvincible()
{
    state = StateInvincible;
    DestroyParticleTrails(node);
    PlaySlotSound(node, SoundInvincible, 0);
    PlaySlotAnimation(0.0f, node, AnimationFlying, 1);
    flags.onCharacter = 0;
    progress = 0.0f;
    exitPoint = *CharacterExitPoint(character);
    Vector4 shift = *RowOf(&exitPoint, 2);
    shift.x = shift.x * FlyToAlong;
    shift.y = shift.y * FlyToAlong;
    shift.z = shift.z * FlyToAlong;
    Vector4* at = RowOf(&exitPoint, 3);
    at->x = at->x + shift.x;
    at->y = at->y + shift.y;
    at->z = at->z + shift.z;
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    f32 x = position.x - at->x;
    f32 y = position.y - at->y;
    f32 z = position.z - at->z;
    distance = __builtin_sqrtf(x * x + y * y + z * z);
}

// Off the character, its trails stopped and the character's and its own destroyed, its boost trail on again
void MaskController::EndInvincibility()
{
    state = StateGotBoostedMask;
    node->owner->flags.visible = 1;
    for (u32 index = 0; index < InvincibilityTrails; index++)
    {
        trailSlots[index] = StopTrail(&trails[index], trailSlots[index]);
    }

    DetachExitPoint(GetGameNode(NodesOf(character), NodeAttachments), MaskExitPoint, 0, 1);
    PlaySlotSound(node, SoundInvincibilityOver, 0);
    PlaySlotAnimation(0.0f, node, AnimationBoosted, 1);
    if (character != nullptr)
    {
        auto* characterNode = static_cast<ObjectNode*>(GetGameNode(&character->nodes, NodeObject));
        if (characterNode != nullptr)
        {
            DestroyParticleTrails(characterNode);
        }
    }

    DestroyParticleTrails(node);
    CallVirtual<u32>(node, node->vtable, ObjectNode::AddParticleTrailSlot, &boostTrail);
}

// Riding a vehicle, the character has it steered toward the middle of its box (offset) and moved four times the way there a
// second; else put at the offset in the character's space, turned as the character is
void MaskController::Follow()
{
    constexpr f32 SteerShare = 8.0f;
    constexpr f32 MoveShare = 4.0f;
    InstanceContext* mask = node->owner;
    f32 seconds = SecondsOf(mask);
    ObjectPlace* place = mask->place;
    place->SyncPosition();
    Vector4 position = place->position;
    Vector4 target;
    if (PlayerAgent()->vehicle != nullptr)
    {
        const Box* box = character->CollisionBox();
        target = box->max;
        target.x = (target.x - box->min.x) * 0.5f + box->min.x + offset.x;
        target.y = (target.y - box->min.y) * 0.5f + box->min.y + offset.y;
        target.z = (target.z - box->min.z) * 0.5f + box->min.z + offset.z;
        SteerTowards(seconds * SteerShare, 0.0f, SteerMostLean, mask, &target);
    }
    else
    {
        target = offset;
        ObjectPlace* characterPlace = PlaceOf(character);
        RotateAndTranslate(characterPlace);
        VuTransformPoint(&characterPlace->matrix, &target, &target);
        characterPlace = PlaceOf(character);
        characterPlace->SyncRotation();
        Vector4 rotation = characterPlace->rotation;
        place = mask->place;
        place->SyncRotation();
        if (place->TurnTo(&rotation))
        {
            QueueObject(mask);
        }
    }

    f32 share = seconds * MoveShare;
    Vector4 move = target;
    move.x = (target.x - position.x) * share;
    move.y = (target.y - position.y) * share;
    move.z = (target.z - position.z) * share;
    if (mask->place->MoveBy(&move))
    {
        QueueObject(mask);
    }
}

SplineController* SplineController::Construct(SplineController* spline, ObjectNode* node, u32 kind)
{
    spline->node = node;
    spline->kind = static_cast<u8>(kind);
    spline->vtable = g_SplineControllerVTable;
    spline->offset = g_DefaultBox.min;
    spline->offset.w = 1.0f;
    spline->turnRate = 1.0f;
    spline->pull = 1.0f;
    spline->drop = 0.0f;
    return spline;
}

void SplineController::Destroy(u32 destroyFlags)
{
    vtable = g_NodeControllerVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SplineController::Start()
{
}

// Its instance's place on its first path (kept as the waypoints' parameter and direction) and the player's: it's steered at 6
// ahead along the path (a third as fast unless its rigid body sets its vertical speed) and righted, and its speed is the motion's
// plus how far it's past the offset's z from the player along the path (times the pull, a second)
void SplineController::Frame(TimeClock*)
{
    constexpr f32 AheadDistance = 6.0f;
    // Not touching anything it turns at this share of its turn rate
    constexpr f32 UntouchedTurnShare = Rounded(0.3);
    Waypoints* waypoints = node->waypoints;
    MotionState* motion = node->motion;
    ObjectRigidBody* body = node->rigidBody;
    InstanceContext* instance = node->owner;
    LayoutPath* path = waypoints->paths.data[0];
    InstanceContext* player = PlayerInstance();
    f32 seconds = SecondsOf(instance);
    ObjectPlace* playerPlace = PlaceOf(player);
    playerPlace->SyncPosition();
    Vector4 playerPosition = playerPlace->position;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    Vector4 playerNearest;
    f32 playerAlong = NearestPointOnPath(path, &playerPosition, &playerNearest);
    Vector4 nearest;
    f32 along = NearestPointOnPath(path, &position, &nearest);
    // Retail clamps the player's place on the path and drops it, and makes the path's frame there only to move the player's
    // nearest point by the offset's x, y and z, which nothing reads
    ClampFloat(playerAlong, 0.0f, 1.0f);
    along = ClampFloat(along, 0.0f, 1.0f);
    Vector4 direction;
    PathDirectionAt(along, path, &direction);
    Matrix4x4 frame;
    MatrixFacing(&frame, &direction);
    waypoints->pathParameter = along;
    waypoints->pathDirection = direction;
    Vector4 heading = direction;
    heading.y = heading.y - drop;
    // Touching something (as of its last frame): the full turn and the whole velocity, else a third of the turn and the
    // velocity across
    bool touching = body->state.touched;
    f32 inverse = InverseLength(&heading, LengthEpsilon);
    heading.x = heading.x * inverse;
    heading.y = heading.y * inverse;
    heading.z = heading.z * inverse;
    Vector4 ahead = {nearest.x + heading.x * AheadDistance, nearest.y + heading.y * AheadDistance,
                     nearest.z + heading.z * AheadDistance, 1.0f};
    // (Retail passes 90 as a third float, which SteerBodyTowards doesn't take)
    if (touching)
    {
        SteerBodyTowards(turnRate * seconds, 0.0f, instance, &ahead, 1);
    }
    else
    {
        SteerBodyTowards(turnRate * (seconds * UntouchedTurnShare), 0.0f, instance, &ahead, 1);
    }

    place = instance->place;
    RotateAndTranslate(place);
    Vector4 up = *RowOf(&place->matrix, 1);
    RightRigidBody(seconds, body, &up);
    place = instance->place;
    RotateAndTranslate(place);
    f32 x = nearest.x - playerNearest.x;
    f32 y = nearest.y - playerNearest.y;
    f32 z = nearest.z - playerNearest.z;
    f32 apart = __builtin_sqrtf(x * x + y * y + z * z);
    Vector4 velocity = *RowOf(&place->matrix, 2);
    f32 speed = motion->speed + (apart - offset.z) * pull * seconds;
    velocity.x = velocity.x * speed;
    velocity.y = velocity.y * speed;
    velocity.z = velocity.z * speed;
    motion->startVelocity = motion->velocity;
    if (touching)
    {
        motion->velocity = velocity;
    }
    else
    {
        motion->velocity.x = velocity.x;
        motion->velocity.z = velocity.z;
    }

    Vector4 move = velocity;
    move.x = velocity.x * seconds;
    move.y = velocity.y * seconds;
    move.z = velocity.z * seconds;
    // Something holds its moves (it has a kind of motion or of collisions)
    if ((body->bits.value & ObjectRigidBodyBits::KindsMask) != 0)
    {
        HoldRigidBodyMove(body, &move);
    }

    if (instance->place->MoveBy(&move))
    {
        QueueObject(instance);
    }

    node->flags.unused4 = 1;
}

void SplineController::Restart(u32)
{
}

void SplineController::Stop()
{
}

SkateController* SkateController::Construct(SkateController* skate, ObjectNode* node, u32 kind)
{
    skate->kind = static_cast<u8>(kind);
    skate->node = node;
    skate->vtable = g_SkateControllerVTable;
    for (TrailArguments& trail : skate->trails)
    {
        ConstructTrailArguments(&trail);
    }

    for (u32 index = 0; index < TrailCount; index++)
    {
        skate->ids[index] = NoParticleSystem;
        skate->trailSlots[index] = -1;
    }

    for (u16& sound : skate->sounds)
    {
        sound = NoSoundId;
    }

    skate->counts.value = 0;
    skate->flags.paused = 0;
    return skate;
}

void SkateController::Destroy(u32 destroyFlags)
{
    for (s32 index = TrailCount - 1; index >= 0; index--)
    {
        DestroyTrailArguments(&trails[index], 0);
    }

    vtable = g_NodeControllerVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SkateController::Start()
{
    unused3E4 = 0;
    flags.grinding = 0;
    flags.wasGrinding = 0;
}

// Whether the player's Humiliskate grinds kept (and last frame's), unless the game is watched. Without a surface the node's sound
// is stopped and there's no grinding
void SkateController::Frame(TimeClock*)
{
    if (GameWatched())
    {
        if (!flags.paused)
        {
            flags.paused = 1;
        }
    }
    else if (flags.paused)
    {
        flags.paused = 0;
    }

    flags.wasGrinding = flags.grinding;
    if (!flags.paused)
    {
        Vehicle* vehicle = PlayerAgent()->vehicle;
        if (vehicle != nullptr && vehicle->Kind() == Vehicle::KindHumiliskate)
        {
            bool grinding = surface != nullptr && static_cast<HumiliskateVehicle*>(vehicle)->grinding != 0;
            flags.grinding = grinding;
        }
    }

    if (surface != nullptr)
    {
        return;
    }

    if (node->playingSound == NoInstanceSound)
    {
        return;
    }

    CallVirtual<void>(node, node->vtable, ObjectNode::StopSoundSlot);
    flags.grinding = 0;
    flags.wasGrinding = 0;
}

void SkateController::Restart(u32)
{
}

void SkateController::Stop()
{
}
