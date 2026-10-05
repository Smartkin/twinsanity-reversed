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
constexpr u32 ArmChainLinks = 5;
// The points the arms' chain is solved into: the leader's elbow and hand, the second's hand and elbow, and the end (the
// second's shoulder); and the solver's steps
enum ArmChainPoint : u32
{
    ChainLeaderElbow = 0,
    ChainLeaderHand = 1,
    ChainSecondHand = 2,
    ChainSecondElbow = 3,
    ChainEnd = 4,
};
constexpr u32 ArmChainSteps = 2;

// The joints the link poses: the spine, and the arm holding the other's hand (the leader's right, the second's left)
constexpr u32 SpineFirst = 2;
constexpr u32 SpineCount = 3;
constexpr u32 LeaderShoulder = 0x11;
constexpr u32 LeaderElbow = 0x12;
constexpr u32 LeaderHand = 0x13;
constexpr u32 SecondShoulder = 0xD;
constexpr u32 SecondElbow = 0xE;
constexpr u32 SecondHand = 0xF;
constexpr u32 PosedJoints[] = {SpineFirst, SpineFirst + 1, SpineFirst + 2, LeaderShoulder, LeaderElbow, LeaderHand,
                               SecondShoulder, SecondElbow, SecondHand};

// A shoulder not seen yet (its x)
constexpr f32 Unseen = 1e10f;
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
ReferencedObjectFlags& FlagsOf(ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, flags);
    return *reinterpret_cast<ReferencedObjectFlags*>(address);
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

    return static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter))->agent);
}

u32 AttackKind(AgentPart* part)
{
    return static_cast<BasicAgentPart*>(part)->bits.attackKind;
}

s32 CharacterKindOf(const CharacterAgent* character)
{
    return character->properties->GetInt(CharacterKindProperty);
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
    Negate(RowOf(rows, character == CharacterCrash ? 1 : 0));
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

    link->bits.value = 0;
    link->bits.state = StateTied;
    link->bits.leader = leader & 1;
    link->SetStretch(1.0f);
    if (link->bits.leader != 0)
    {
        // The second isn't taken for a projectile while tied (the flag put back by the destructor)
        InstanceContext* instance = other->instance;
        link->bits.secondWasProjectile = instance->flags.movesBetweenChunks;
        instance->flags.movesBetweenChunks = 0;
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
    if (bits.leader != 0)
    {
        ReferencedObject* object = ObjectOf(second);
        if (object != nullptr)
        {
            if (bits.secondWasProjectile != 0)
            {
                object->flags.movesBetweenChunks = 1;
            }
            else
            {
                object->flags.movesBetweenChunks = 0;
            }
        }
    }

    if (ikChain != nullptr)
    {
        DestroyIkChain(ikChain, DestroyAndFree);
    }

    RemoveReference(&leader);
    RemoveReference(&second);
    vtable = g_JointHookVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
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

    bits.state = StateTied;
}

void CharacterLink::Detach(OgiAnimator* animator)
{
    for (u32 joint : PosedJoints)
    {
        RemoveJointCallback(animator, joint, this);
    }

    bits.state = StateDetached;
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

    if (bits.state != StateSlamming)
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
    if (bits.state != StateSlamming)
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
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    if (bits.leader != 0)
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
    if (kind == AttackSpin || kind == AttackSpinVariant)
    {
        return 0;
    }

    CharacterAgent* second = Second();
    bits.state = StateSlamming;
    LinkOf(second)->bits.state = StateSlamming;
    character->instance->flags.visible = 1;
    FlagsOf(ObjectOf(this->second)).visible = 1;
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
        if (bits.state == StateTied && part->moveBits.jumping == 0 && circle != 0.0f && slamCooldown == 0.0f)
        {
            StartSlam(&time);
        }

        if (bits.state == StateSlamming)
        {
            slamSeconds = time * g_SecondsPerClockUnit - slamStart * g_SecondsPerClockUnit;
            if (SlamSeconds < slamSeconds)
            {
                bits.state = StateTied;
                LinkOf(second)->bits.state = StateTied;
                LinkOf(second)->bits.gait = GaitNone;
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
    switch (bits.gait)
    {
    case GaitNone:
        RunEvent(character, EventIdle);
        bits.gait = GaitIdle;
        break;
    case GaitIdle:
        if (GaitWalkSpeed < speed)
        {
            bits.gait = GaitWalking;
            RunEvent(character, EventWalk);
        }
        else if (agreement < GaitShuffleCosine && GaitShuffleSpeed < speed)
        {
            bits.gait = GaitShuffling;
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
            bits.gait = GaitShuffling;
            RunEvent(character, EventShuffleFeet);
        }
        else if (GaitRunSpeed < speed)
        {
            bits.gait = GaitRunning;
            RunEvent(character, EventRun);
        }
        else if (speed < GaitStopSpeed)
        {
            bits.gait = GaitIdle;
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
            bits.gait = GaitWalking;
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
            bits.gait = GaitWalking;
            RunEvent(character, EventWalk);
        }
        else if (speed < GaitShuffleSpeed)
        {
            bits.gait = GaitIdle;
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

        auto* made = static_cast<IkChain*>(MemoryAllocate(sizeof(IkChain)));
        ConstructIkSolver(&made->solver);
        ikChain = made;
        SetIkLinks(made, ArmChainLinks, lengths, angles);
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
    return SolveIk(ikChain, chain, ArmChainSteps, LengthEpsilon);
}

void CharacterLink::SolveArms()
{
    CharacterAgent* second = Second();
    if (bits.hold == HoldNone || LinkOf(second)->bits.hold == HoldNone)
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
    bits.hold = HoldHands;
    reach = __builtin_sqrtf(end.x * end.x + end.y * end.y + end.z * end.z);
    CharacterLink* secondLink = LinkOf(second);
    secondLink->bits.hold = HoldHands;
    // The chain's second half moved to end at the second's shoulder: the arms' segments
    const Vector4& chainEnd = chain[ChainEnd];
    Vector4 shift = {end.x - chainEnd.x, end.y - chainEnd.y, end.z - chainEnd.z, 1.0f};
    const Vector4& leaderElbow = chain[ChainLeaderElbow];
    const Vector4& leaderHand = chain[ChainLeaderHand];
    upperArm = leaderElbow;
    forearm = {leaderHand.x - leaderElbow.x, leaderHand.y - leaderElbow.y, leaderHand.z - leaderElbow.z, 1.0f};
    secondLink = LinkOf(second);
    for (u32 i = ChainSecondHand; i <= ChainSecondElbow; i++)
    {
        chain[i].x = chain[i].x + shift.x;
        chain[i].y = chain[i].y + shift.y;
        chain[i].z = chain[i].z + shift.z;
    }

    const Vector4& secondHand = chain[ChainSecondHand];
    const Vector4& secondElbow = chain[ChainSecondElbow];
    secondLink->upperArm = {secondElbow.x - end.x, secondElbow.y - end.y, secondElbow.z - end.z, 1.0f};
    secondLink->forearm = {secondHand.x - secondElbow.x, secondHand.y - secondElbow.y, secondHand.z - secondElbow.z, 1.0f};
    for (Vector4& point : chain)
    {
        point.x = point.x + shoulder.x;
        point.y = point.y + shoulder.y;
        point.z = point.z + shoulder.z;
    }

    elbow = leaderElbow;
    hand = leaderHand;
    // The second's elbow and hand in its own model space
    Vector4 world;
    VuTransformPoint(&own, &secondElbow, &world);
    VuTransformPoint(&otherInverse, &world, &secondLink->elbow);
    VuTransformPoint(&own, &secondHand, &world);
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
    u32 hold = bits.hold;
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
        if (bits.state == StateSlamming)
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

        if (hold == HoldNone)
        {
            bits.hold = HoldSeen;
        }

        if (hold != HoldHands)
        {
            return 0;
        }

        ArmRows(&posed, &upperArm, CharacterKindOf(character));
        const Vector4* at = RowOf(matrix, 3);
        shoulderShift = {at->x - shoulder.x, at->y - shoulder.y, at->z - shoulder.z, 1.0f};
        *RowOf(&posed, 3) = {shoulder.x + shoulderShift.x, shoulder.y + shoulderShift.y, shoulder.z + shoulderShift.z, 1.0f};
    }
    else if (joint == elbowJoint)
    {
        if (bits.state == StateSlamming || hold != HoldHands)
        {
            return 0;
        }

        ArmRows(&posed, &forearm, CharacterKindOf(character));
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
            ArmRows(&rows, &forearm, CharacterKindOf(character));
            Vector4 point = {elbow.x + along.x, elbow.y + along.y, elbow.z + along.z, 1.0f};
            *RowOf(&rows, 3) = {point.x + shoulderShift.x, point.y + shoulderShift.y, point.z + shoulderShift.z, 1.0f};
            handPoint = CharacterKindOf(character) == CharacterCrash ? CrashHandPoint : OtherHandPoint;
            if (bits.state != StateSlamming)
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
        if (bits.state != StateSlamming || joint - SpineFirst < SpineCount)
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
