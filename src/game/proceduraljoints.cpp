#include "game/characters.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/animation.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"

#include <cstddef>

EABI_EXPORT(FUN_0015f7a8, &ProceduralJoint::MakeSlopeTilt);
EABI_EXPORT(FUN_0014bdc0, &ProceduralJoint::Step);
EABI_EXPORT(FUN_0014add0, &ProceduralJoints::PlaceFeet);
EABI_EXPORT(FUN_0015f2c0, &ProceduralJoints::Squash);
EABI_EXPORT(FUN_0015f2f8, &ProceduralJoints::SetSquash);

// MakeDangling has more arguments than n32 passes in registers: the asm hands it this and six integers in $a0-$a6 and seven
// floats in $f12-$f18, the C++ function takes this and the floats in $a0 and $f13-$f19 and the integers on the stack
asm(R"(
    .pushsection .text.FUN_0015f610, "ax", @progbits
    .globl FUN_0015f610
    .type FUN_0015f610, @function
    .set push
    .set noreorder
FUN_0015f610:
    addiu $sp, $sp, -0x40
    sd $ra, 0x30($sp)
    sd $5, 0x0($sp)
    sd $6, 0x8($sp)
    sd $7, 0x10($sp)
    sd $8, 0x18($sp)
    sd $9, 0x20($sp)
    sd $10, 0x28($sp)
    mov.s $f19, $f18
    mov.s $f18, $f17
    mov.s $f17, $f16
    mov.s $f16, $f15
    mov.s $f15, $f14
    mov.s $f14, $f13
    jal FUN_0015f610_n32
    mov.s $f13, $f12
    ld $ra, 0x30($sp)
    jr $ra
    addiu $sp, $sp, 0x40
    .set pop
    .size FUN_0015f610, . - FUN_0015f610
    .popsection
)");

namespace
{
constexpr u32 ModelNodeKind = 3;
// The characters (the first int property): Crash, Cortex, a Crash 2 units high without probes, Nina, none and Mecha-Bandicoot
constexpr u32 CharacterProperty = 0;
constexpr s32 Crash = 0;
constexpr s32 Cortex = 1;
constexpr s32 TallCrash = 2;
constexpr s32 Nina = 3;
constexpr s32 MechaBandicoot = 5;
// The angles and the lag's components (0 x, 1 y, 2 z)
constexpr s32 AxisX = 0;
constexpr s32 AxisY = 1;
constexpr s32 AxisZ = 2;
// The legs' joints: the thigh, the shin and the foot of the leg on the instance's -x side (A) and of the +x side's (B)
constexpr s32 ThighA = 6;
constexpr s32 ShinA = 7;
constexpr s32 FootA = 8;
constexpr s32 ThighB = 9;
constexpr s32 ShinB = 10;
constexpr s32 FootB = 11;
constexpr s32 LegJoints[] = {ThighA, ThighB, ShinA, ShinB, FootA, FootB};
// Crash's joints tilted to the slope and squashed on landing
constexpr s32 SlopeTiltJoint = 5;
constexpr s32 SquashJoint = 28;
// The dangling joints' smoothing half-lives (seconds)
constexpr f32 DanglingHalfLife = Rounded(0.05);
constexpr f32 DanglingSlowHalfLife = Rounded(0.1);
constexpr f32 SlopeTiltSpeed = 1.3f;
// The landing squash: 20 times the drop is its speed, a spring back to 1 and its limits
constexpr f32 SquashDropSpeed = 20.0f;
constexpr f32 SquashStiffness = 300.0f;
constexpr f32 SquashDamping = 9.0f;
constexpr f32 SquashMost = 1.3f;
constexpr f32 SquashLeast = 0.5f;
// Foot placement: rays from half a unit above to half a unit below each foot (0.23 to either side, its toe 0.15 ahead) against
// the ground and the instances of node kinds 4, 13, 15, 16 and 18; a leg's stretch from its foot's height (within 0.3 either
// way), a foot's tilt from its heel's height above its toe's, eased at 5 and 14 a second
constexpr f32 NoHit = Rounded(1e30);
constexpr u16 MostFound = 32;
constexpr u32 FootNodeKinds = 0x5A010;
constexpr f32 RayReach = 0.5f;
constexpr f32 FootSide = Rounded(0.23);
constexpr f32 ToeAhead = Rounded(0.15);
constexpr f32 MostFootHeight = Rounded(0.3);
constexpr f32 StretchPerHeight = 0x1.aaaaaap-1f;
constexpr f32 StretchRate = 5.0f;
constexpr f32 TiltPerHeight = Rounded(6.4);
constexpr f32 MostTilt = 0.25f;
constexpr f32 TiltRate = 14.0f;
// The slope tilt turns only when the normal is this far from up
constexpr f32 LeastTilt = Rounded(0.001);
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// The part's move bit of the crouch's state 3 (crawling)
constexpr u32 MoveCrawling = 0x200;
constexpr Vector4 Up = {0.0f, 1.0f, 0.0f, 1.0f};

static_assert(offsetof(CharacterAgent, link) == 0xB0);
static_assert(offsetof(CharacterAgent, cache) == 0x2B0);

f32 SecondsOf(const TimeClock* clock)
{
    return static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
}

// A point along a way from another, its w 1
Vector4 Along(const Vector4* from, const Vector4* way, f32 scale)
{
    return {from->x + way->x * scale, from->y + way->y * scale, from->z + way->z * scale, 1.0f};
}

// A matrix's turn: its translation made (0, 0, 0, 1)
Matrix4x4 TurnOf(const Matrix4x4* matrix)
{
    Matrix4x4 turn = *matrix;
    Vector4* translation = RowOf(&turn, 3);
    translation->x = 0.0f;
    translation->y = 0.0f;
    translation->z = 0.0f;
    translation->w = 1.0f;
    return turn;
}

// The height of the ground or of an instance of the kinds within half a unit above or below a point, from the point (NoHit
// without one)
f32 GroundHeight(InstanceContext* instance, CollisionCache* cache, InstanceRayHit* query, const Vector4* point)
{
    Vector4 start = *point;
    Vector4 end = *point;
    end.y = end.y - RayReach;
    start.y = start.y + RayReach;
    query->bits &= ~InstanceRayHit::BitFull;
    query->count = 0;
    query->instance = nullptr;
    query->distance = NoHit;
    Vector4 hit;
    if (SegmentHitsTrianglesOrInstances(instance->chunk, cache, &start, &end, query, FootNodeKinds, nullptr, &hit, nullptr) == 0)
    {
        return NoHit;
    }

    return hit.y - point->y;
}

f32 ClampHeight(f32 height)
{
    if (MostFootHeight < height)
    {
        return MostFootHeight;
    }

    if (height < -MostFootHeight)
    {
        return -MostFootHeight;
    }

    return height;
}

// A foot's tilt: its heel's height above its toe's (none when a ray missed)
f32 FootTilt(f32 heel, f32 toe)
{
    if (heel == NoHit || toe == NoHit)
    {
        return 0.0f;
    }

    f32 tilt = (heel - toe) * TiltPerHeight;
    if (MostTilt < tilt)
    {
        return MostTilt;
    }

    if (tilt < -MostTilt)
    {
        return -MostTilt;
    }

    return tilt;
}

// An angle slot of a joint (0 x, 1 y, 2 z; any other the first)
s32* AngleSlot(ProceduralJoint* joint, s32 slot)
{
    if (slot == AxisY)
    {
        return &joint->angles[AxisY];
    }

    if (slot == AxisZ)
    {
        return &joint->angles[AxisZ];
    }

    return &joint->angles[AxisX];
}

// An angle eased toward a value (radians): what's kept of it, the rest the value's
void EaseAngle(s32* angle, f32 keep, f32 radians)
{
    s32 eased = *angle;
    MultiplyAngle(&eased, keep);
    AddRadiansToAngle(&eased, (1.0f - keep) * radians);
    *angle = eased;
}
}

ProceduralJoint* ProceduralJoint::Construct(ProceduralJoint* joint)
{
    joint->Reset();
    return joint;
}

void ProceduralJoint::Reset()
{
    kind = KindNone;
    anchor = g_DefaultBox.min;
    anchor.w = 1.0f;
    motion = g_DefaultBox.min;
    motion.w = 1.0f;
    slowMotion = g_DefaultBox.min;
    slowMotion.w = 1.0f;
    angles[AxisX] = 0;
    angles[AxisY] = 0;
    angles[AxisZ] = 0;
    normal = Up;
    joint = -1;
    started = 0;
    normalSpeed = 0.0f;
    anchorExitPoint = -1;
}

void ProceduralJoint::MakeDangling(f32 halfLife, f32 slowHalfLife, f32 gain, f32 secondGain, f32 unused, f32 limit,
                                   f32 secondLimit, s32 anchorExitPoint, s32 joint, s32 slot, s32 secondSlot, s32 axis,
                                   s32 secondAxis)
{
    Reset();
    kind = KindDangling;
    this->anchorExitPoint = anchorExitPoint;
    this->joint = joint;
    this->halfLife = halfLife;
    this->slowHalfLife = slowHalfLife;
    this->gain = gain;
    this->secondGain = secondGain;
    unused70 = unused;
    this->limit = limit;
    this->secondLimit = secondLimit;
    this->slot = slot;
    this->secondSlot = secondSlot;
    this->axis = axis;
    this->secondAxis = secondAxis;
}

void ProceduralJoint::MakeLeg(s32 joint)
{
    kind = KindLeg;
    this->joint = joint;
}

void ProceduralJoint::MakeSlopeTilt(f32 normalSpeed, s32 joint)
{
    this->normalSpeed = normalSpeed;
    kind = KindSlopeTilt;
    this->joint = joint;
}

void ProceduralJoint::MakeSquash(s32 joint)
{
    kind = KindSquash;
    this->joint = joint;
}

void ProceduralJoint::Still(CharacterAgent* unused)
{
    motion = g_DefaultBox.min;
    motion.w = 1.0f;
    slowMotion = g_DefaultBox.min;
    slowMotion.w = 1.0f;
}

u32 ProceduralJoint::ChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &anchor, 1);
    TransformVectorThroughLink(link, &motion, 0);
    TransformVectorThroughLink(link, &slowMotion, 0);
    TransformVectorThroughLink(link, &normal, 0);
    return 1;
}

void ProceduralJoint::Step(f32 seconds, CharacterAgent* agent)
{
    if (kind == KindDangling)
    {
        // The anchor's movement this frame smoothed twice: what's between the two, in the anchor's space, swings the joint
        InstanceContext* instance = agent->instance;
        OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNodeKind))->animator;
        Vector4 position;
        Matrix4x4 frame;
        if (anchorExitPoint == -1)
        {
            ObjectPlace* place = instance->place;
            place->SyncPosition();
            position = place->position;
            RotateAndTranslate(place);
            frame = place->matrix;
        }
        else
        {
            ExitPointAnimation* exitPoint =
                animator->exitPoints != nullptr ? animator->exitPoints->data[static_cast<u8>(anchorExitPoint)] : nullptr;
            const Matrix4x4* matrix = &UpdateExitPointMatrix(exitPoint)->matrix;
            frame = *matrix;
            position = *RowOf(matrix, 3);
        }

        if (started == 0)
        {
            started = 1;
            anchor = position;
        }

        Vector4 move = {position.x - anchor.x, position.y - anchor.y, position.z - anchor.z, 1.0f};
        anchor = position;
        f32 keep = HalfLifeShare(halfLife, seconds);
        f32 slowKeep = HalfLifeShare(slowHalfLife, seconds);
        f32 share = 1.0f - keep;
        motion = {motion.x * keep + move.x * share, motion.y * keep + move.y * share, motion.z * keep + move.z * share, 1.0f};
        f32 slowShare = 1.0f - slowKeep;
        slowMotion = {slowMotion.x * slowKeep + motion.x * slowShare, slowMotion.y * slowKeep + motion.y * slowShare,
                      slowMotion.z * slowKeep + motion.z * slowShare, 1.0f};
        Vector4 lag = {motion.x - slowMotion.x, motion.y - slowMotion.y, motion.z - slowMotion.z, 1.0f};
        VuInvertRigidInPlace(&frame);
        Vector4 local;
        VuRotateVector(&frame, &lag, &local);
        if (agent->link != nullptr)
        {
            // Tied to the other character: turned about y by the link's lean
            s32 lean;
            AngleFrom(&lean, -agent->link->ThreeQuartersStretch(), AngleRadians);
            Matrix4x4 turn;
            MatrixAboutY(&turn, &lean);
            VuRotateVector(&turn, &local, &local);
        }

        f32 first = -(&local.x)[axis] * gain;
        f32 second = (&local.x)[secondAxis] * secondGain;
        if (limit < first)
        {
            first = limit;
        }

        if (first < -limit)
        {
            first = -limit;
        }

        if (secondLimit < second)
        {
            second = secondLimit;
        }

        if (second < -secondLimit)
        {
            second = -secondLimit;
        }

        EaseAngle(AngleSlot(this, slot), keep, first);
        EaseAngle(AngleSlot(this, secondSlot), keep, second);
        *AngleSlot(this, 3 - secondSlot - slot) = 0;
    }
    else if (kind == KindSlopeTilt)
    {
        // Crouching or crawling the normal moves toward the ground's under the character at its speed, else it's up
        u32 moveBits = static_cast<CharacterPart*>(agent->part)->moveBits;
        if ((moveBits & MoveCrawling) == 0 && (moveBits & CharacterPart::Crouching) == 0)
        {
            normal = Up;
            return;
        }

        Vector4 ground;
        Vector4 target;
        if (agent->FootNormal(&ground) != 0)
        {
            target = ground;
        }
        else
        {
            target = Up;
        }

        Vector4 move = {target.x - normal.x, target.y - normal.y, target.z - normal.z, 1.0f};
        f32 most = normalSpeed * seconds;
        if (most < __builtin_sqrtf(move.x * move.x + move.y * move.y + move.z * move.z))
        {
            f32 inverse = InverseLength(&move, LengthEpsilon);
            move.x = move.x * inverse * most;
            move.y = move.y * inverse * most;
            move.z = move.z * inverse * most;
        }

        normal.x = normal.x + move.x;
        normal.y = normal.y + move.y;
        normal.z = normal.z + move.z;
    }
}

ProceduralJoints* ProceduralJoints::Construct(ProceduralJoints* joints, CharacterAgent* agent)
{
    joints->count = 0;
    joints->elements = nullptr;
    joints->attached = 0;
    joints->placesFeet = 0;
    joints->squashes = 0;
    joints->vtable = g_ProceduralJointsVTable;
    joints->agent = agent;
    joints->SetUp();
    return joints;
}

void ProceduralJoints::SetUp()
{
    switch (agent->properties->GetInt(CharacterProperty))
    {
    case Crash:
        MakeElements(12);
        elements[0].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -165.0f, -70.0f, Rounded(3.3), Rounded(2.1),
                                 Rounded(1.6), 1, 20, AxisZ, AxisX, AxisX, AxisZ);
        elements[1].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -165.0f, -70.0f, Rounded(3.3), Rounded(2.1),
                                 Rounded(1.6), 0, 21, AxisZ, AxisX, AxisX, AxisZ);
        elements[2].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -23.0f, 25.0f, Rounded(3.2), Rounded(1.1),
                                 Rounded(1.1), 9, 26, AxisY, AxisZ, AxisX, AxisY);
        elements[3].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -23.0f, -25.0f, Rounded(3.2), Rounded(1.1),
                                 Rounded(1.1), 10, 27, AxisY, AxisZ, AxisX, AxisY);
        for (s32 i = 0; i < 6; i++)
        {
            elements[4 + i].MakeLeg(LegJoints[i]);
        }

        elements[10].MakeSlopeTilt(SlopeTiltSpeed, SlopeTiltJoint);
        elements[11].MakeSquash(SquashJoint);
        squashes = 1;
        placesFeet = 1;
        break;
    case Cortex:
        MakeElements(8);
        for (s32 i = 0; i < 6; i++)
        {
            elements[i].MakeLeg(LegJoints[i]);
        }

        elements[6].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -23.0f, 45.0f, Rounded(5.2), 2.0f, Rounded(1.1), 9,
                                 26, AxisY, AxisZ, AxisX, AxisY);
        elements[7].MakeDangling(DanglingHalfLife, DanglingSlowHalfLife, -23.0f, -45.0f, Rounded(5.2), 2.0f, Rounded(1.1), 10,
                                 27, AxisY, AxisZ, AxisX, AxisY);
        placesFeet = 1;
        break;
    case TallCrash:
    case Nina:
        placesFeet = 1;
        MakeElements(6);
        for (s32 i = 0; i < 6; i++)
        {
            elements[i].MakeLeg(LegJoints[i]);
        }

        break;
    case MechaBandicoot:
        placesFeet = 0;
        break;
    }

    squash = 1.0f;
    squashSpeed = 0.0f;
    legStretchB = 1.0f;
    legStretchA = 1.0f;
    footTiltB = 0.0f;
    footTiltA = 0.0f;
}

void ProceduralJoints::MakeElements(s32 elementCount)
{
    if (elements != nullptr)
    {
        DeleteArray(elements);
        count = 0;
    }

    count = elementCount;
    ProceduralJoint* made = NewArray<ProceduralJoint>(elementCount);
    for (u32 i = 0; i < static_cast<u32>(elementCount); i++)
    {
        ProceduralJoint::Construct(&made[i]);
    }

    elements = made;
}

void ProceduralJoints::Destroy(u32 destroyFlags)
{
    vtable = g_ProceduralJointsVTable;
    if (attached != 0)
    {
        DetachFromModelAnimator(this, agent->instance);
    }

    if (elements != nullptr)
    {
        DeleteArray(elements);
    }

    vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ProceduralJoints::Attach(OgiAnimator* animator)
{
    for (s32 i = 0; i < count; i++)
    {
        AddJointCallback(animator, static_cast<u8>(elements[i].joint), this);
    }
}

void ProceduralJoints::Detach(OgiAnimator* animator)
{
    for (s32 i = 0; i < count; i++)
    {
        RemoveJointCallback(animator, static_cast<u8>(elements[i].joint), this);
    }
}

u32 ProceduralJoints::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    JointAnimation* animation = animator->animation;
    JointAnimation* parent = animation->parent;
    s32 joint = animation->joint->id;
    // Retail bug: past the last element when none has the joint (it only hooks its elements' joints, so it can't happen)
    s32 index = 0;
    while (index < count && elements[index].joint != joint)
    {
        index++;
    }

    ProceduralJoint* element = &elements[index];
    Matrix4x4 posedMatrix;
    switch (element->kind)
    {
    case ProceduralJoint::KindDangling:
    {
        s32 x = element->angles[AxisX];
        s32 y = element->angles[AxisY];
        s32 z = element->angles[AxisZ];
        Vector4 rotation;
        GetRotationXYZ(&rotation, &x, &y, &z);
        AddJointRotation(animator, &rotation);
        return 1;
    }
    case ProceduralJoint::KindLeg:
    {
        // Thighs stretched along y by their leg's stretch, feet by its inverse and sheared by their tilt
        CreateJointTransform(animator, parent, 1, matrix);
        Matrix4x4 turn = TurnOf(matrix);
        Matrix4x4 scale;
        InitIdentityMatrix(&scale);
        if (joint == ThighA)
        {
            scale.m[1][1] = legStretchA;
        }
        else if (joint == ThighB)
        {
            scale.m[1][1] = legStretchB;
        }

        if (joint == ShinA || joint == ShinB)
        {
            scale.m[1][1] = 1.0f;
        }

        if (joint == FootA)
        {
            scale.m[1][1] = 1.0f / legStretchA;
            scale.m[2][1] = -footTiltA;
        }
        else if (joint == FootB)
        {
            scale.m[1][1] = 1.0f / legStretchB;
            scale.m[2][1] = -footTiltB;
        }

        VuMultiplyMatrices(&turn, &scale, &posedMatrix);
        break;
    }
    case ProceduralJoint::KindSlopeTilt:
    {
        // Turned in the world from up to the normal
        const Vector4* normal = &element->normal;
        Vector4 axis;
        axis.x = normal->y * Up.z - normal->z * Up.y;
        axis.y = normal->z * Up.x - normal->x * Up.z;
        axis.z = normal->x * Up.y - normal->y * Up.x;
        axis.w = 1.0f;
        axis.x = -axis.x;
        axis.y = -axis.y;
        axis.z = -axis.z;
        bool tilted = LeastTilt < __builtin_sqrtf(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        Vector4 rotation;
        if (tilted)
        {
            s32 angle;
            AngleOfCosine(Up.x * normal->x + Up.y * normal->y + Up.z * normal->z, &angle);
            RotationAboutAxis(&rotation, &axis, &angle, 0);
        }

        CreateJointTransform(animator, parent, 1, matrix);
        Matrix4x4 turn = TurnOf(matrix);
        ObjectPlace* place = agent->instance->place;
        RotateAndTranslate(place);
        Matrix4x4 world = place->matrix;
        *RowOf(&world, 3) = g_DefaultBox.min;
        RowOf(&world, 3)->w = 1.0f;
        Matrix4x4 toLocal = world;
        VuInvertRigidInPlace(&toLocal);
        if (tilted)
        {
            Matrix4x4 tilt;
            MatrixFromRotation(&tilt, &rotation);
            *RowOf(&tilt, 3) = g_DefaultBox.min;
            RowOf(&tilt, 3)->w = 1.0f;
            PreMultiply(&tilt, &world);
            MultiplyInPlace(&tilt, &toLocal);
            VuMultiplyMatrices(&turn, &tilt, &posedMatrix);
        }
        else
        {
            posedMatrix = turn;
        }

        break;
    }
    case ProceduralJoint::KindSquash:
    {
        // Scaled along z by the squash (retail works out 2 minus the squash and compares it with 1, then leaves x and y at 1)
        CreateJointTransform(animator, parent, 1, matrix);
        Matrix4x4 turn = TurnOf(matrix);
        Matrix4x4 scale;
        InitIdentityMatrix(&scale);
        scale.m[2][2] = squash;
        scale.m[1][1] = 1.0f;
        scale.m[0][0] = 1.0f;
        VuMultiplyMatrices(&scale, &turn, &posedMatrix);
        break;
    }
    default:
        return 1;
    }

    *RowOf(&posedMatrix, 3) = *RowOf(matrix, 3);
    *matrix = posedMatrix;
    return 1;
}

void ProceduralJoints::Frame(TimeClock* clock)
{
    InstanceContext* instance = agent->instance;
    OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNodeKind))->animator;
    if (attached == 0)
    {
        AttachToModelAnimator(this, instance);
        attached = 1;
    }

    if (animator != nullptr && animator->callbacks != nullptr)
    {
        f32 seconds = SecondsOf(clock);
        for (s32 i = 0; i < count; i++)
        {
            elements[i].Step(seconds, agent);
        }
    }

    if (placesFeet != 0)
    {
        PlaceFeet(SecondsOf(clock));
    }

    if (squashes != 0)
    {
        // Crash's squash springs back to 1
        f32 seconds = SecondsOf(clock);
        f32 now = squash;
        f32 speed = squashSpeed;
        f32 pull = (now - 1.0f) * -SquashStiffness + speed * -SquashDamping;
        now = now + speed * seconds;
        squashSpeed = speed + pull * seconds;
        squash = now;
        if (SquashMost < now)
        {
            squash = SquashMost;
        }
        else if (now < SquashLeast)
        {
            squash = SquashLeast;
        }
    }
}

void ProceduralJoints::PlaceFeet(f32 seconds)
{
    InstanceContext* instance = agent->instance;
    CollisionCache* cache = &agent->cache;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    void* found[MostFound];
    InstanceRayHit query;
    query.results = found;
    query.most = MostFound;
    query.count = 0;
    query.distance = NoHit;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.bits = InstanceRayHit::BitAllWanted;
    query.wantedFlags = 0;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, instance);

    // Each leg stretched or bent by its foot's height
    const Vector4* side = RowOf(&place->matrix, 0);
    const Vector4* position = RowOf(&place->matrix, 3);
    Vector4 heelA = Along(position, side, -FootSide);
    Vector4 heelB = Along(position, side, FootSide);
    f32 heelHeightA = GroundHeight(instance, cache, &query, &heelA);
    f32 heelHeightB = GroundHeight(instance, cache, &query, &heelB);
    f32 stretchA = 1.0f;
    if (heelHeightA != NoHit)
    {
        stretchA = 1.0f - ClampHeight(heelHeightA) * StretchPerHeight;
    }

    f32 stretchB = 1.0f;
    if (heelHeightB != NoHit)
    {
        stretchB = 1.0f - ClampHeight(heelHeightB) * StretchPerHeight;
    }

    f32 ease = seconds * StretchRate;
    if (1.0f < ease)
    {
        ease = 1.0f;
    }

    legStretchB = (1.0f - ease) * legStretchB + ease * stretchB;
    legStretchA = (1.0f - ease) * legStretchA + ease * stretchA;

    // Each foot tilted by its heel's height above its toe's
    const Vector4* forward = RowOf(&place->matrix, 2);
    Vector4 sideA = Along(RowOf(&place->matrix, 3), RowOf(&place->matrix, 0), -FootSide);
    Vector4 toeA = Along(&sideA, forward, ToeAhead);
    Vector4 sideB = Along(RowOf(&place->matrix, 3), RowOf(&place->matrix, 0), FootSide);
    Vector4 toeB = Along(&sideB, forward, ToeAhead);
    f32 toeHeightA = GroundHeight(instance, cache, &query, &toeA);
    f32 toeHeightB = GroundHeight(instance, cache, &query, &toeB);
    ease = seconds * TiltRate;
    if (1.0f < ease)
    {
        ease = 1.0f;
    }

    footTiltA = (1.0f - ease) * footTiltA + ease * FootTilt(heelHeightA, toeHeightA);
    footTiltB = (1.0f - ease) * footTiltB + ease * FootTilt(heelHeightB, toeHeightB);
}

void ProceduralJoints::Squash(f32 scale)
{
    if (scale < squash)
    {
        squashSpeed = (scale - squash) * SquashDropSpeed;
        squash = scale;
    }
}

void ProceduralJoints::SetSquash(f32 scale)
{
    squash = scale;
    squashSpeed = 0.0f;
}

void ProceduralJoints::Still()
{
    for (s32 i = 0; i < count; i++)
    {
        elements[i].Still(agent);
    }
}

u32 ProceduralJoints::ChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    u32 all = 1;
    for (s32 i = 0; i < count; i++)
    {
        all &= elements[i].ChangeChunk(from, link) != 0;
    }

    return all;
}
