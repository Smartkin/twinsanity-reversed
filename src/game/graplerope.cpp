#include "game/characters.h"

#include "game/animation.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/springbody.h"

EABI_EXPORT(FUN_00162098, ConstructGrapleRope);

namespace
{
// The rope: 10 solver passes a step, a chain of one segment (a spring between two points)
constexpr u32 RopePasses = 10;
constexpr u32 RopeSegments = 1;
constexpr f32 RopeStiffness = 10000.0f;
constexpr f32 RopeDamping = 100.0f;
// The graple's joints: 0 at the anchor, turned toward 1 at the hook
constexpr u32 AnchorJoint = 0;
constexpr u32 HookJoint = 1;
constexpr u32 RopeJoints = 2;
// The ragdoll's joints, in the order its callbacks go on
constexpr u32 RagdollJoints[] = {0, 1, 5, 23, 15, 14, 13, 22, 19, 18, 17, 8, 7, 6, 11, 10, 9};
}

GrapleRope* ConstructGrapleRope(f32 mass, void* memory, InstanceContext* instance)
{
    auto* rope = static_cast<GrapleRope*>(memory);
    SpringSkeleton::Construct(rope, RopePasses, instance);
    rope->vtable = g_GrapleRopeVTable;
    SpringBody* body = &rope->body;
    rope->chain = ConstructSpringChain(MemoryAllocate(sizeof(SpringChain)), RopeSegments, body);
    AddSpringChain(body, rope->chain);
    SetPointsDrag(0.0f, body);
    SetChainSprings(RopeStiffness, RopeDamping, rope->chain);
    rope->chain->last->mass = mass;
    return rope;
}

void GrapleRope::Destroy(u32 destroyFlags)
{
    vtable = g_GrapleRopeVTable;
    DetachFromInstance();
    BaseDestroy(destroyFlags);
}

void GrapleRope::Attach(OgiAnimator* animator)
{
    for (u32 joint = 0; joint < RopeJoints; joint++)
    {
        AddJointCallback(animator, joint, this);
    }
}

void GrapleRope::Detach(OgiAnimator* animator)
{
    for (u32 joint = 0; joint < RopeJoints; joint++)
    {
        RemoveJointCallback(animator, joint, this);
    }
}

u32 GrapleRope::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    u32 joint = animator->animation->joint->id;
    u32 toward;
    if (joint == HookJoint)
    {
        toward = GameOGI::NoJoint;
    }
    else if (joint == AnchorJoint)
    {
        toward = HookJoint;
    }
    else
    {
        return 0;
    }

    PoseJointFromPoints(joint, toward, animator);
    return 1;
}

void GrapleRope::Step(TimeClock* clock, const Vector4* gravity, const Vector4* hook, const Vector4* anchor)
{
    if (anchor == nullptr)
    {
        PinChainStart(chain, hook);
        SimulateSpringBody(&body, clock, nullptr, gravity);
    }
    else
    {
        LayChain(chain, anchor, hook);
    }

    StepBlend(clock);
}

void Ragdoll::Destroy(u32 destroyFlags)
{
    vtable = g_RagdollVTable;
    DetachFromInstance();
    BaseDestroy(destroyFlags);
}

void Ragdoll::Attach(OgiAnimator* animator)
{
    for (u32 joint : RagdollJoints)
    {
        AddJointCallback(animator, joint, this);
    }
}

void Ragdoll::Detach(OgiAnimator* animator)
{
    for (u32 joint : RagdollJoints)
    {
        RemoveJointCallback(animator, joint, this);
    }
}

u32 Ragdoll::PoseJoint(JointAnimator* animator, Matrix4x4* matrix)
{
    // The joints at the body's ends are moved to their points, the others turned toward the next joint's
    u32 joint = animator->animation->joint->id;
    u32 toward;
    switch (joint)
    {
    case 0:
    case 8:
    case 11:
    case 22:
    case 23:
        toward = GameOGI::NoJoint;
        break;
    case 1:
        toward = 0;
        break;
    case 5:
        toward = 1;
        break;
    case 6:
        toward = 7;
        break;
    case 7:
        toward = 8;
        break;
    case 9:
        toward = 10;
        break;
    case 10:
        toward = 11;
        break;
    case 13:
        toward = 14;
        break;
    case 14:
        toward = 15;
        break;
    case 15:
        toward = 23;
        break;
    case 17:
        toward = 18;
        break;
    case 18:
        toward = 19;
        break;
    case 19:
        toward = 22;
        break;
    default:
        return 0;
    }

    PoseJointFromPoints(joint, toward, animator);
    return 1;
}
