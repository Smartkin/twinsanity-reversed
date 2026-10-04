#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/ik.h"
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
    extern const GccVTableEntry g_CharacterLinkVTable[] RETAIL(D_002F3610);
    // The arms' IK chain: its five links' lengths (the leader's upper arm and forearm, between the hands, the second's forearm
    // and upper arm) and their angles (radians)
    extern const f32 g_ArmChainLengths[5] RETAIL(D_002F31E8);
    extern const f32 g_ArmChainAngles[5] RETAIL(D_002F3200);
}

EABI_EXPORT(FUN_0014de38, &CharacterLink::Frame);
EABI_EXPORT(FUN_0015f968, &CharacterLink::SetStretch);

namespace
{
constexpr u32 AgentNodeKind = 0xC;
constexpr u32 IkChainSize = 0x90;
constexpr u32 IkSolverOffset = 0x30;
constexpr u32 ArmChainLinks = 5;
constexpr u32 StateBits = CharacterLink::StateMask;
constexpr u32 GaitBits = CharacterLink::GaitMask << CharacterLink::GaitShift;
constexpr u32 HoldBits = CharacterLink::HoldMask << CharacterLink::HoldShift;
// The second's gaits (its link's bits 4-7)
constexpr u32 GaitNone = 0;
constexpr u32 GaitIdle = 1;
constexpr u32 GaitWalking = 2;
constexpr u32 GaitRunning = 3;
constexpr u32 GaitShuffling = 4;
// The part's attack kinds of the spin (its two)
constexpr u32 AttackSpin = 6;
constexpr u32 AttackSpin2 = 10;
// The characters (their first int property): Crash's hand point differs
constexpr s32 Crash = 0;

// The joints the link poses: the spine, and the arm holding the other's hand (the leader's right, the second's left)
constexpr u32 SpineFirst = 2;
constexpr u32 SpineCount = 3;
constexpr u32 LeaderShoulder = 0x11;
constexpr u32 LeaderElbow = 0x12;
constexpr u32 LeaderHand = 0x13;
constexpr u32 SecondShoulder = 0xD;
constexpr u32 SecondElbow = 0xE;
constexpr u32 SecondHand = 0xF;
constexpr u32 PosedJoints[] = {2, 3, 4, LeaderShoulder, LeaderElbow, LeaderHand, SecondShoulder, SecondElbow, SecondHand};

// A shoulder not seen yet (its x)
constexpr f32 Unseen = 1e10f;
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// The stretch: how far the second's swing length is past 1.1, full at 1.45
constexpr f32 StretchStart = Rounded(1.1);
constexpr f32 StretchScale = 0x1.6db6dcp+1f;
constexpr f32 BlendInPerSecond = 10.0f;
// The slam: over after a second, then a quarter second before the next; the spine leans fully after a quarter second and
// back from 0.75, the arms are posed again from 0.75, and the part of it MidSlam tells LinkedHits of
constexpr f32 SlamSeconds = 1.0f;
constexpr f32 SlamCooldown = 0.25f;
constexpr f32 SlamLeanIn = 0.25f;
constexpr f32 SlamLeanOut = 0.75f;
constexpr f32 MidSlamFrom = Rounded(0.2);
constexpr f32 MidSlamTo = Rounded(0.99);
// The hand's length along the forearm (the leader's, the second's), and its point in the hand's space
constexpr f32 LeaderHandLength = Rounded(0.3);
constexpr f32 SecondHandLength = Rounded(0.256);
constexpr Vector4 CrashHandPoint = {Rounded(0.2), Rounded(0.11), Rounded(-0.05), 1.0f};
constexpr Vector4 OtherHandPoint = {Rounded(-0.1), Rounded(-0.05), Rounded(-0.05), 1.0f};
// The second's gait: walking past 2 a second, running past 4 (back under 3), idle under 0.5 (0.2 shuffling), shuffling while
// its motion is off the leader's facing (a cosine under 0.6; back over 0.8)
constexpr f32 GaitWalkSpeed = 2.0f;
constexpr f32 GaitRunSpeed = 4.0f;
constexpr f32 GaitRunEndSpeed = 3.0f;
constexpr f32 GaitStopSpeed = 0.5f;
constexpr f32 GaitShuffleSpeed = Rounded(0.2);
constexpr f32 GaitShuffleCosine = Rounded(0.6);
constexpr f32 GaitShuffleEndCosine = Rounded(0.8);

// A character's link as retail reads it, also when there's no character (the word at 0xB0 then)
CharacterLink* LinkOf(const CharacterAgent* agent)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(agent) + offsetof(CharacterAgent, link);
    return *reinterpret_cast<CharacterLink* const*>(address);
}

ReferencedObject* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? handle->object : nullptr;
}

// An object's flags and place as retail reads them, also when there's no object (the words at 4 and 8 then)
u32& FlagsOf(ReferencedObject* object)
{
    return *reinterpret_cast<u32*>(reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, flags));
}

ObjectPlace* PlaceOf(const ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// The character agent of a handle's instance (none without one)
CharacterAgent* CharacterOf(const Reference* handle)
{
    auto* instance = static_cast<InstanceContext*>(ObjectOf(handle));
    if (instance == nullptr)
    {
        return nullptr;
    }

    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&instance->nodes, AgentNodeKind))->agent);
}

u8 AttackKind(AgentPart* part)
{
    return *reinterpret_cast<u8*>(&static_cast<BasicAgentPart*>(part)->bits);
}

void SetState(CharacterLink* link, u32 state)
{
    link->bits = (link->bits & ~StateBits) | state;
}

void SetGait(CharacterLink* link, u32 gait)
{
    link->bits = (link->bits & ~GaitBits) | gait << CharacterLink::GaitShift;
}

u32 StateOf(const CharacterLink* link)
{
    return link->bits & StateBits;
}

void Normalize(Vector4* vector)
{
    f32 inverse = InverseLength(vector, LengthEpsilon);
    vector->x *= inverse;
    vector->y *= inverse;
    vector->z *= inverse;
}

void Negate(Vector4* row)
{
    row->x = -row->x;
    row->y = -row->y;
    row->z = -row->z;
}

// An arm joint's rotation along its segment: RowsAlong's rows, Crash's second and third pointing the other way, the other
// characters' first and third
void ArmRows(Matrix4x4* rows, const Vector4* segment, s32 character)
{
    RowsAlong(RowOf(rows, 0), segment, 0, 1, 2, 0);
    Negate(RowOf(rows, character == Crash ? 1 : 0));
    Negate(RowOf(rows, 2));
}

// The second's swing length: the second's link's (the leader's link asks the second for it), the link's own on the second's
f32 SwingLengthOf(CharacterLink* link)
{
    CharacterAgent* second = link->Second();
    return second != nullptr ? LinkOf(second)->swingLength : link->swingLength;
}

void RunEvent(CharacterAgent* character, u32 event)
{
    RunAgentEvent(character, event, 0, 0, 0);
}
} // namespace

void RowsAlong(Vector4* rows, const Vector4* direction, u32 along, u32 second, u32 across, u32 axis)
{
    Vector4* first = &rows[along];
    *first = *direction;
    Normalize(first);
    Vector4* next = &rows[second];
    next->x = 0.0f;
    next->y = 0.0f;
    next->z = 0.0f;
    next->w = 1.0f;
    reinterpret_cast<f32*>(next)[axis] = 1.0f;
    Vector4* third = &rows[across];
    third->x = first->y * next->z - first->z * next->y;
    third->y = first->z * next->x - first->x * next->z;
    third->z = first->x * next->y - first->y * next->x;
    third->w = 1.0f;
    Normalize(third);
    next->x = third->y * first->z - third->z * first->y;
    next->y = third->z * first->x - third->x * first->z;
    next->z = third->x * first->y - third->y * first->x;
    next->w = 1.0f;
    Normalize(next);
    rows[2].w = 0.0f;
    rows[0].w = 0.0f;
    rows[1].w = 0.0f;
}

CharacterLink* CharacterLink::Construct(CharacterLink* link, CharacterAgent* character, CharacterAgent* other, u32 leader)
{
    link->vtable = g_CharacterLinkVTable;
    link->character = character;
    link->second = nullptr;
    link->leader = nullptr;
    link->ikChain = nullptr;
    link->blendIn = 0.0f;
    if (leader != 0)
    {
        AssignReference(&link->second, other->instance);
    }
    else
    {
        AssignReference(&link->leader, other->instance);
    }

    link->bits = StateTied | ((leader & 1) != 0 ? BitLeader : 0);
    link->SetStretch(1.0f);
    if ((link->bits & BitLeader) != 0)
    {
        // The second isn't taken for a projectile while tied (the flag put back by the destructor)
        InstanceContext* instance = other->instance;
        u32 wasProjectile = (instance->flags & ReferencedObject::FlagProjectile) != 0 ? BitSecondWasProjectile : 0;
        link->bits = (link->bits & ~BitSecondWasProjectile) | wasProjectile;
        instance->flags &= ~ReferencedObject::FlagProjectile;
    }
    else
    {
        ObjectPlace* place = character->instance->place;
        RotateAndTranslate(place);
        link->swingPosition = *RowOf(&place->matrix, 3);
        link->swingVelocity.z = 0.0f;
        link->swingVelocity.y = 0.0f;
        link->swingVelocity.x = 0.0f;
        link->swingVelocity.w = 1.0f;
        link->swingLength = 1.0f;
    }

    link->slamSeconds = 0.0f;
    link->slamCooldown = 0.0f;
    link->shoulder.x = Unseen;
    link->elbow = g_DefaultBox.min;
    link->elbow.w = 1.0f;
    link->hand = g_DefaultBox.min;
    link->hand.w = 1.0f;
    link->handPoint = g_DefaultBox.min;
    link->handPoint.w = 1.0f;
    return link;
}

void CharacterLink::Destroy(u32 destroyFlags)
{
    vtable = g_CharacterLinkVTable;
    DetachFromModelAnimator(this, character->instance);
    if ((bits & BitLeader) != 0)
    {
        ReferencedObject* object = ObjectOf(second);
        if (object != nullptr)
        {
            if ((bits & BitSecondWasProjectile) != 0)
            {
                object->flags |= ReferencedObject::FlagProjectile;
            }
            else
            {
                object->flags &= ~ReferencedObject::FlagProjectile;
            }
        }
    }

    if (ikChain != nullptr)
    {
        DestroyIkChain(ikChain, 3);
    }

    RemoveReference(&leader);
    RemoveReference(&second);
    vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CharacterLink::Attach(OgiAnimator* animator)
{
    for (u32 joint : PosedJoints)
    {
        AddJointCallback(animator, joint, this);
    }

    SetState(this, StateTied);
}

void CharacterLink::Detach(OgiAnimator* animator)
{
    for (u32 joint : PosedJoints)
    {
        RemoveJointCallback(animator, joint, this);
    }

    SetState(this, StateDetached);
}

void CharacterLink::SetStretch(f32 stretch)
{
    this->stretch = stretch;
}

f32 CharacterLink::SpineLean()
{
    return stretch * 0.25f;
}

f32 CharacterLink::ThreeQuartersStretch()
{
    return stretch * 0.25f * 3.0f;
}

CharacterAgent* CharacterLink::Leader()
{
    return CharacterOf(leader);
}

CharacterAgent* CharacterLink::Second()
{
    return CharacterOf(second);
}

u32 CharacterLink::MidSlam()
{
    CharacterAgent* second = Second();
    CharacterAgent* leader = Leader();
    f32 seconds;
    if (second != nullptr)
    {
        seconds = slamSeconds;
    }
    else
    {
        CharacterLink* leaderLink = LinkOf(leader);
        if (leaderLink == nullptr)
        {
            return 0;
        }

        seconds = leaderLink->slamSeconds;
    }

    if (StateOf(this) != StateSlamming)
    {
        return 0;
    }

    return MidSlamFrom < seconds && seconds < MidSlamTo;
}

f32 CharacterLink::SlamLeanWeight()
{
    CharacterAgent* second = Second();
    CharacterAgent* leader = Leader();
    // Unlike MidSlam, a leader without a link isn't checked for (its slam seconds are read at 0x1A4 then)
    f32 seconds = second == nullptr ? LinkOf(leader)->slamSeconds : slamSeconds;
    if (StateOf(this) != StateSlamming)
    {
        return 1.0f;
    }

    if (seconds < SlamLeanIn)
    {
        return 1.0f - seconds * 4.0f;
    }

    if (seconds < SlamLeanOut)
    {
        return 0.0f;
    }

    return seconds * 4.0f - 3.0f;
}

u32 CharacterLink::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    if ((bits & BitLeader) != 0)
    {
        TransformThroughLink(link, &secondMatrix, 1);
        return static_cast<InstanceContext*>(ObjectOf(second))->ChangeChunk(link) != nullptr;
    }

    TransformVectorThroughLink(link, &swingPosition, 1);
    TransformVectorThroughLink(link, &swingVelocity, 0);
    return 1;
}

u32 CharacterLink::StartSlam(const s32* time)
{
    u32 kind = AttackKind(character->part);
    if (kind == AttackSpin || kind == AttackSpin2)
    {
        return 0;
    }

    CharacterAgent* second = Second();
    SetState(this, StateSlamming);
    SetState(LinkOf(second), StateSlamming);
    character->instance->flags |= ReferencedObject::FlagVisible;
    FlagsOf(ObjectOf(this->second)) |= ReferencedObject::FlagVisible;
    RunEvent(character, EventBodySlam);
    RunEvent(second, EventBodySlam);
    slamStart = *time;
    return 1;
}

void CharacterLink::Frame(f32 circle, TimeClock* clock)
{
    CharacterAgent* second = Second();
    auto* part = static_cast<CharacterPart*>(character->part);
    f32 stretch = (SwingLengthOf(this) - StretchStart) * StretchScale;
    if (1.0f < stretch)
    {
        stretch = 1.0f;
    }

    if (stretch < 0.0f)
    {
        stretch = 0.0f;
    }

    SetStretch(stretch);
    f32 seconds = static_cast<s32>(clock->advance) * g_SecondsPerClockUnit;
    blendIn = blendIn + seconds * BlendInPerSecond;
    if (1.0f < blendIn)
    {
        blendIn = 1.0f;
    }

    // The leader's: circle slams them while tied, on the ground and after the cooldown
    if (second != nullptr)
    {
        s32 time = clock->time;
        if (StateOf(this) == StateTied && (part->moveBits & CharacterPart::Jumping) == 0 && circle != 0.0f
            && slamCooldown == 0.0f)
        {
            StartSlam(&time);
        }

        if (StateOf(this) == StateSlamming)
        {
            slamSeconds = time * g_SecondsPerClockUnit - slamStart * g_SecondsPerClockUnit;
            if (SlamSeconds < slamSeconds)
            {
                SetState(this, StateTied);
                SetState(LinkOf(second), StateTied);
                SetGait(LinkOf(second), GaitNone);
                slamCooldown = SlamCooldown;
            }
        }
    }

    if (slamCooldown <= seconds)
    {
        slamCooldown = 0.0f;
    }
    else
    {
        slamCooldown = slamCooldown - seconds;
    }
}

void CharacterLink::SecondGait(const Vector4* velocity, const Vector4* facing)
{
    // Retail asks for the leader and drops it
    Leader();
    f32 speed = __builtin_sqrtf(velocity->x * velocity->x + velocity->z * velocity->z);
    Vector4 moving = *velocity;
    Normalize(&moving);
    Vector4 ahead = *facing;
    Normalize(&ahead);
    f32 agreement = moving.x * ahead.x + moving.y * ahead.y + moving.z * ahead.z;
    switch (bits >> GaitShift & GaitMask)
    {
    case GaitNone:
        RunEvent(character, EventIdle);
        SetGait(this, GaitIdle);
        break;
    case GaitIdle:
        if (GaitWalkSpeed < speed)
        {
            SetGait(this, GaitWalking);
            RunEvent(character, EventWalk);
        }
        else if (agreement < GaitShuffleCosine && GaitShuffleSpeed < speed)
        {
            SetGait(this, GaitShuffling);
            RunEvent(character, EventShuffleFeet);
        }
        else
        {
            RunEvent(character, EventIdle);
        }

        break;
    case GaitWalking:
        if (agreement < GaitShuffleCosine && GaitShuffleSpeed < speed)
        {
            SetGait(this, GaitShuffling);
            RunEvent(character, EventShuffleFeet);
        }
        else if (GaitRunSpeed < speed)
        {
            SetGait(this, GaitRunning);
            RunEvent(character, EventRun);
        }
        else if (speed < GaitStopSpeed)
        {
            SetGait(this, GaitIdle);
            RunEvent(character, EventIdle);
        }
        else
        {
            RunEvent(character, EventWalk);
        }

        break;
    case GaitRunning:
        if (speed < GaitRunEndSpeed)
        {
            SetGait(this, GaitWalking);
            RunEvent(character, EventWalk);
        }
        else
        {
            RunEvent(character, EventRun);
        }

        break;
    case GaitShuffling:
        if (GaitShuffleEndCosine < agreement || GaitWalkSpeed < speed)
        {
            SetGait(this, GaitWalking);
            RunEvent(character, EventWalk);
        }
        else if (speed < GaitShuffleSpeed)
        {
            SetGait(this, GaitIdle);
            RunEvent(character, EventIdle);
        }
        else
        {
            RunEvent(character, EventShuffleFeet);
        }

        break;
    default:
        break;
    }
}

s32 CharacterLink::SolveChain(const Vector4* start, const Vector4* end)
{
    if (ikChain == nullptr)
    {
        f32 lengths[ArmChainLinks];
        f32 angles[ArmChainLinks];
        for (u32 i = 0; i < ArmChainLinks; i++)
        {
            lengths[i] = g_ArmChainLengths[i];
            angles[i] = g_ArmChainAngles[i];
        }

        void* chain = MemoryAllocate(IkChainSize);
        ConstructIkSolver(static_cast<u8*>(chain) + IkSolverOffset);
        ikChain = chain;
        SetIkLinks(chain, ArmChainLinks, lengths, angles);
        // Each link's angle limits: -72 to -18 degrees, -144 to -36, 0 to 135, 72 to 108 and 9 to 81
        f32 limits[ArmChainLinks][2] = {
            {-0x1.41b2f8p+0f, -0x1.41b2f8p-2f}, {-0x1.41b2f8p+1f, -0x1.41b2f8p-1f}, {0.0f, 0x1.2d97c8p+1f},
            {0x1.41b2f8p+0f, 0x1.e28c76p+0f},   {0x1.41b2f8p-3f, 0x1.69e956p+0f},
        };
        for (u32 i = 0; i < ArmChainLinks; i++)
        {
            SetIkLimits(ikChain, i + 1, &limits[i][0], &limits[i][1]);
        }
    }

    SetIkEnds(ikChain, start, end);
    return SolveIk(ikChain, chain, 2, LengthEpsilon);
}

void CharacterLink::SolveArms()
{
    CharacterAgent* second = Second();
    if ((bits & HoldBits) == 0 || (LinkOf(second)->bits & HoldBits) == 0)
    {
        return;
    }

    // The second's shoulder in the leader's model space, the chain solved from the leader's shoulder to it
    ObjectPlace* place = character->instance->place;
    RotateAndTranslate(place);
    Matrix4x4 own = place->matrix;
    ObjectPlace* secondPlace = second->instance->place;
    RotateAndTranslate(secondPlace);
    Matrix4x4 other = secondPlace->matrix;
    Matrix4x4 ownInverse;
    Matrix4x4 otherInverse;
    VuInvertRigid(&ownInverse, &own);
    VuInvertRigid(&otherInverse, &other);
    Vector4 ownShoulder;
    Vector4 otherShoulder;
    VuTransformPoint(&own, &shoulder, &ownShoulder);
    VuTransformPoint(&other, &LinkOf(second)->shoulder, &otherShoulder);
    Vector4 between = {otherShoulder.x - ownShoulder.x, otherShoulder.y - ownShoulder.y, otherShoulder.z - ownShoulder.z, 1.0f};
    Vector4 turned = between;
    Vector4 end;
    VuRotateVector(&ownInverse, &turned, &end);
    Vector4 start = {0.0f, 0.0f, 0.0f, 1.0f};
    SolveChain(&start, &end);
    bits = (bits & ~HoldBits) | HoldHands << HoldShift;
    reach = __builtin_sqrtf(end.x * end.x + end.y * end.y + end.z * end.z);
    CharacterLink* secondLink = LinkOf(second);
    secondLink->bits = (secondLink->bits & ~HoldBits) | HoldHands << HoldShift;
    // The chain's second half moved to end at the second's shoulder: the arms' segments
    Vector4 shift = {end.x - chain[4].x, end.y - chain[4].y, end.z - chain[4].z, 1.0f};
    upperArm = chain[0];
    forearm = {chain[1].x - chain[0].x, chain[1].y - chain[0].y, chain[1].z - chain[0].z, 1.0f};
    secondLink = LinkOf(second);
    for (u32 i = 2; i < 4; i++)
    {
        chain[i].x = chain[i].x + shift.x;
        chain[i].y = chain[i].y + shift.y;
        chain[i].z = chain[i].z + shift.z;
    }

    secondLink->upperArm = {chain[3].x - end.x, chain[3].y - end.y, chain[3].z - end.z, 1.0f};
    secondLink->forearm = {chain[2].x - chain[3].x, chain[2].y - chain[3].y, chain[2].z - chain[3].z, 1.0f};
    for (Vector4& point : chain)
    {
        point.x = point.x + shoulder.x;
        point.y = point.y + shoulder.y;
        point.z = point.z + shoulder.z;
    }

    elbow = chain[0];
    hand = chain[1];
    // The second's elbow and hand in its own model space
    Vector4 world;
    VuTransformPoint(&own, &chain[3], &world);
    VuTransformPoint(&otherInverse, &world, &secondLink->elbow);
    VuTransformPoint(&own, &chain[2], &world);
    VuTransformPoint(&otherInverse, &world, &secondLink->hand);
    ObjectPlace* kept = PlaceOf(ObjectOf(this->second));
    RotateAndTranslate(kept);
    secondMatrix = kept->matrix;
}

u32 CharacterLink::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    CharacterAgent* leader = Leader();
    CharacterAgent* second = Second();
    f32 slam = slamSeconds;
    if (second == nullptr)
    {
        CharacterLink* leaderLink = LinkOf(leader);
        if (leaderLink == nullptr)
        {
            return 0;
        }

        slam = leaderLink->slamSeconds;
    }

    JointAnimation* animation = animator->animation;
    u32 hold = bits >> HoldShift & HoldMask;
    u32 joint = animation->joint->id;
    u32 shoulderJoint = second != nullptr ? LeaderShoulder : SecondShoulder;
    u32 elbowJoint = second != nullptr ? LeaderElbow : SecondElbow;
    u32 handJoint = second != nullptr ? LeaderHand : SecondHand;
    CreateJointTransform(animator, animation->parent, 1, matrix);
    Matrix4x4 posed;
    if (joint - SpineFirst < SpineCount)
    {
        // The spine leans with the stretch while holding hands, eased out and back during the slam
        if (hold != HoldHands)
        {
            return 0;
        }

        Matrix4x4 turned = *matrix;
        RowOf(&turned, 3)->x = 0.0f;
        RowOf(&turned, 3)->y = 0.0f;
        RowOf(&turned, 3)->z = 0.0f;
        RowOf(&turned, 3)->w = 1.0f;
        f32 weight = SlamLeanWeight();
        s32 angle;
        AngleFrom(&angle, weight * SpineLean(), AngleRadians);
        Matrix4x4 lean;
        MatrixAboutY(&lean, &angle);
        VuMultiplyMatrices(&lean, &turned, &posed);
        *RowOf(&posed, 3) = *RowOf(matrix, 3);
    }
    else if (joint == shoulderJoint)
    {
        if (StateOf(this) == StateSlamming)
        {
            return 0;
        }

        f32 seen = shoulder.x;
        Vector4 now = *RowOf(matrix, 3);
        shoulderNow = now;
        if (seen == Unseen)
        {
            shoulder = now;
        }

        if (hold == 0)
        {
            bits = (bits & ~HoldBits) | HoldSeen << HoldShift;
        }

        if (hold != HoldHands)
        {
            return 0;
        }

        ArmRows(&posed, &upperArm, character->properties->GetInt(0));
        const Vector4* at = RowOf(matrix, 3);
        shoulderShift = {at->x - shoulder.x, at->y - shoulder.y, at->z - shoulder.z, 1.0f};
        *RowOf(&posed, 3) = {shoulder.x + shoulderShift.x, shoulder.y + shoulderShift.y, shoulder.z + shoulderShift.z, 1.0f};
    }
    else if (joint == elbowJoint)
    {
        if (StateOf(this) == StateSlamming || hold != HoldHands)
        {
            return 0;
        }

        ArmRows(&posed, &forearm, character->properties->GetInt(0));
        *RowOf(&posed, 3) = {elbow.x + shoulderShift.x, elbow.y + shoulderShift.y, elbow.z + shoulderShift.z, 1.0f};
    }
    else if (joint == handJoint)
    {
        Vector4 along = {hand.x - elbow.x, hand.y - elbow.y, hand.z - elbow.z, 1.0f};
        Normalize(&along);
        f32 length = second != nullptr ? LeaderHandLength : SecondHandLength;
        along.z = along.z * length;
        along.x = along.x * length;
        along.y = along.y * length;
        if (hold == HoldHands)
        {
            // The hand at the forearm's end, its point taken into the world
            Matrix4x4 rows;
            ArmRows(&rows, &forearm, character->properties->GetInt(0));
            Vector4 point = {elbow.x + along.x, elbow.y + along.y, elbow.z + along.z, 1.0f};
            *RowOf(&rows, 3) = {point.x + shoulderShift.x, point.y + shoulderShift.y, point.z + shoulderShift.z, 1.0f};
            handPoint = character->properties->GetInt(0) == Crash ? CrashHandPoint : OtherHandPoint;
            if (StateOf(this) != StateSlamming)
            {
                posed = rows;
                VuTransformPoint(&rows, &handPoint, &handPoint);
                ObjectPlace* place = character->instance->place;
                RotateAndTranslate(place);
                VuTransformPoint(&place->matrix, &handPoint, &handPoint);
            }
            else
            {
                VuTransformPoint(matrix, &handPoint, &handPoint);
                ObjectPlace* place = character->instance->place;
                RotateAndTranslate(place);
                VuTransformPoint(&place->matrix, &handPoint, &handPoint);
                posed = *matrix;
            }
        }

        // The leader's IK runs once its hand is made
        if (second != nullptr)
        {
            shoulder = shoulderNow;
            SolveArms();
        }
    }
    else
    {
        return 0;
    }

    // Posed while holding hands; during the slam only the spine, and the arm from 0.75 seconds in
    bool replace = false;
    if (hold == HoldHands)
    {
        if (StateOf(this) != StateSlamming || joint - SpineFirst < SpineCount)
        {
            replace = true;
        }
        else if (SlamLeanOut <= slam && (joint == shoulderJoint || joint == elbowJoint || joint == handJoint))
        {
            replace = true;
        }
    }

    if (!replace)
    {
        return 0;
    }

    if (blendIn < 1.0f)
    {
        for (u32 row = 0; row < 4; row++)
        {
            const Vector4* to = RowOf(&posed, row);
            Vector4* from = RowOf(matrix, row);
            f32 x = to->x - from->x;
            f32 y = to->y - from->y;
            f32 z = to->z - from->z;
            f32 share = blendIn;
            from->w = 1.0f;
            from->x = from->x + x * share;
            from->y = from->y + y * share;
            from->z = from->z + z * share;
        }
    }
    else
    {
        *matrix = posed;
    }

    RowOf(matrix, 0)->w = 0.0f;
    RowOf(matrix, 3)->w = 1.0f;
    RowOf(matrix, 1)->w = 0.0f;
    RowOf(matrix, 2)->w = 0.0f;
    return 1;
}
