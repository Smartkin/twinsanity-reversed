#include "game/springbody.h"

#include "game/animation.h"
#include "game/characters.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/reference.h"

// The spring bodies (points joined by springs) and the SpringSkeletons posing joints by them, with the iterators over their arrays

EABI_EXPORT(FUN_00191520, ConstructSpring);
EABI_EXPORT(FUN_00191470, SetChainSprings);
EABI_EXPORT(FUN_00191780, SetPointsDrag);
EABI_EXPORT(FUN_0018fb18, MoveSpringPoint);
EABI_EXPORT(FUN_0018fe08, MoveSpringPoints);

namespace
{
constexpr f32 DefaultStiffness = 1000.0f;
// Springs whose ends are nearer pull nothing
constexpr f32 MinimumSpringLength = 0x1.0624dep-10f;
// A step longer than a 50th of a second takes its passes once for every whole 50th of a second in it
constexpr f32 LongStep = 0x1.47ae14p-6f;
constexpr f32 PassesPerSecond = 50.0f;
// The room the arrays are made with and grow by
constexpr u32 ArrayRoom = 10;

void DestroyIterator(ArrayIterator* iterator, const GccVTableEntry* base, u32 flags)
{
    iterator->vtable = base;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}

bool IsOutside(const ArrayIterator* iterator)
{
    return iterator->index < 0 || static_cast<u32>(iterator->index) >= iterator->array->count;
}

// A vector made none (the default box's corner, its w 1)
void Clear(Vector4* vector)
{
    *vector = g_DefaultBox.min;
    vector->w = 1.0f;
}

template <typename T>
void MakeArray(PointerArray<T>* array)
{
    array->count = 0;
    array->capacity = ArrayRoom;
    array->growth = ArrayRoom;
    array->data = static_cast<T**>(MemoryAllocate2(ArrayRoom * sizeof(T*)));
}

// A point of mass 1 at a position, stopped, added to a body
SpringPoint* AddPoint(SpringBody* body, const Vector4* position)
{
    auto* point = static_cast<SpringPoint*>(MemoryAllocate(sizeof(SpringPoint)));
    point->mass = 1.0f;
    point->position = *position;
    point->drag = 0.0f;
    point->pinned = 0;
    StopSpringPoint(point);
    body->points.Append(point);
    return point;
}

InstanceContext* InstanceOf(const SpringSkeleton* skeleton)
{
    Reference* reference = skeleton->instance;
    return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
}

// The animator of the skeleton's instance's model (none without an instance, a model node or an animator)
OgiAnimator* AnimatorOf(const SpringSkeleton* skeleton)
{
    if (InstanceOf(skeleton) == nullptr)
    {
        return nullptr;
    }

    auto* node = static_cast<ModelNode*>(GetGameNode(&InstanceOf(skeleton)->nodes, NodeModel));
    if (node == nullptr)
    {
        return nullptr;
    }

    return node->animator;
}

// A blend's share of its ticks gone by, and whether they all are
f32 BlendShare(const SpringSkeleton* skeleton, const TimeClock* clock, bool* done)
{
    s32 elapsed = static_cast<s32>(clock->time - static_cast<u32>(skeleton->blendStart));
    *done = !(elapsed < skeleton->blendTicks);
    if (*done)
    {
        return 1.0f;
    }

    return (static_cast<f32>(elapsed) * g_SecondsPerClockUnit) / (static_cast<f32>(skeleton->blendTicks) * g_SecondsPerClockUnit);
}
}

void ArrayIterator::SpringsDestroy(u32 flags)
{
    DestroyIterator(this, g_SpringsIteratorBaseVTable, flags);
}

void ArrayIterator::SpringsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_SpringsIteratorBaseVTable, flags);
}

void ArrayIterator::SpringsFirst()
{
    index = 0;
}

u32 ArrayIterator::SpringsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::SpringsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::SpringsNext()
{
    index++;
}

void ArrayIterator::SpringsPrevious()
{
    index--;
}

void ArrayIterator::SpringsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::SpringsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::SpringsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::PointsDestroy(u32 flags)
{
    DestroyIterator(this, g_PointsIteratorBaseVTable, flags);
}

void ArrayIterator::PointsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_PointsIteratorBaseVTable, flags);
}

void ArrayIterator::PointsFirst()
{
    index = 0;
}

u32 ArrayIterator::PointsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::PointsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::PointsNext()
{
    index++;
}

void ArrayIterator::PointsPrevious()
{
    index--;
}

void ArrayIterator::PointsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::PointsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::PointsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::ChainsDestroy(u32 flags)
{
    DestroyIterator(this, g_ChainsIteratorBaseVTable, flags);
}

void ArrayIterator::ChainsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_ChainsIteratorBaseVTable, flags);
}

void ArrayIterator::ChainsFirst()
{
    index = 0;
}

u32 ArrayIterator::ChainsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::ChainsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::ChainsNext()
{
    index++;
}

void ArrayIterator::ChainsPrevious()
{
    index--;
}

void ArrayIterator::ChainsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::ChainsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::ChainsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void StopSpringPoint(SpringPoint* point)
{
    Clear(&point->velocity);
    Clear(&point->force);
}

Spring* ConstructSpring(f32 restLength, Spring* spring, SpringPoint* start, SpringPoint* end)
{
    spring->start = start;
    spring->end = end;
    spring->restLength = restLength;
    spring->stiffness = DefaultStiffness;
    spring->damping = 0.0f;
    return spring;
}

void DestroySpring(Spring* spring, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(spring);
    }
}

void ApplySpring(Spring* spring)
{
    SpringPoint* end = spring->end;
    SpringPoint* start = spring->start;
    f32 x = end->position.x - start->position.x;
    f32 y = end->position.y - start->position.y;
    f32 z = end->position.z - start->position.z;
    f32 length = Kept(__builtin_sqrtf(x * x + y * y + z * z));
    if (!(MinimumSpringLength <= length))
    {
        return;
    }

    f32 inverse = 1.0f / length;
    x = x * inverse;
    y = y * inverse;
    z = z * inverse;
    f32 partX = end->velocity.x - start->velocity.x;
    f32 partY = end->velocity.y - start->velocity.y;
    f32 partZ = end->velocity.z - start->velocity.z;
    f32 parting = partX * x + partY * y + partZ * z;
    f32 force = spring->stiffness * (length - spring->restLength) + spring->damping * parting;
    x = x * force;
    y = y * force;
    z = z * force;
    end->force.x = end->force.x - x;
    end->force.y = end->force.y - y;
    end->force.z = end->force.z - z;
    start = spring->start;
    start->force.x = start->force.x + x;
    start->force.y = start->force.y + y;
    start->force.z = start->force.z + z;
}

void SpringChain::Destroy(u32 destroyFlags)
{
    vtable = g_SpringChainVTable;
    BaseDestroy(destroyFlags);
}

void SpringChain::BaseDestroy(u32 destroyFlags)
{
    vtable = g_SpringListVTable;
    for (u32 index = 0; index < springs.count; index++)
    {
        Spring* spring = springs.data[index];
        if (spring != nullptr)
        {
            DestroySpring(spring, DestroyAndFree);
        }
    }

    if (springs.data != nullptr)
    {
        MemoryDeallocate_(springs.data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SpringChain::ApplySprings()
{
    for (u32 index = 0; index < springs.count; index++)
    {
        ApplySpring(springs.data[index]);
    }
}

void SpringChain::Build(const Vector4* along, const Vector4* start, SpringBody* body)
{
    f32 length = __builtin_sqrtf(along->x * along->x + along->y * along->y + along->z * along->z);
    f32 restLength = length / static_cast<f32>(segments);
    Vector4 position = *start;
    Vector4 step = *along;
    first = AddPoint(body, &position);
    f32 share = 1.0f / static_cast<f32>(segments);
    step.x = step.x * share;
    step.y = step.y * share;
    step.z = step.z * share;
    position.x = position.x + step.x;
    position.y = position.y + step.y;
    position.z = position.z + step.z;
    SpringPoint* previous = first;
    for (u32 index = 1; index < segments; index++)
    {
        SpringPoint* point = AddPoint(body, &position);
        springs.Append(ConstructSpring(restLength, static_cast<Spring*>(MemoryAllocate(sizeof(Spring))), previous, point));
        previous = point;
        position.x = position.x + step.x;
        position.y = position.y + step.y;
        position.z = position.z + step.z;
    }

    last = AddPoint(body, &position);
    springs.Append(ConstructSpring(restLength, static_cast<Spring*>(MemoryAllocate(sizeof(Spring))), previous, last));
}

SpringChain* ConstructSpringChain(void* memory, u32 segments, SpringBody* body)
{
    auto* chain = static_cast<SpringChain*>(memory);
    chain->springs.count = 0;
    chain->springs.growth = ArrayRoom;
    chain->springs.capacity = ArrayRoom;
    chain->vtable = g_SpringListVTable;
    chain->springs.data = static_cast<Spring**>(MemoryAllocate2(ArrayRoom * sizeof(Spring*)));
    chain->segments = segments;
    chain->vtable = g_SpringChainVTable;
    Vector4 down = {0.0f, -1.0f, 0.0f, 1.0f};
    Vector4 origin = {0.0f, 0.0f, 0.0f, 1.0f};
    chain->Build(&down, &origin, body);
    return chain;
}

void SetChainSprings(f32 stiffness, f32 damping, SpringChain* chain)
{
    for (u32 index = 0; index < chain->springs.count; index++)
    {
        Spring* spring = chain->springs.data[index];
        spring->damping = damping;
        spring->stiffness = stiffness;
    }
}

void PinChainStart(SpringChain* chain, const Vector4* point)
{
    SpringPoint* first = chain->first;
    if (point == nullptr)
    {
        first->pinned = 0;
        return;
    }

    first->pinned = 1;
    first->position = *point;
    StopSpringPoint(first);
}

void LayChain(SpringChain* chain, const Vector4* start, const Vector4* end)
{
    Vector4 position = *start;
    Vector4 step = *end;
    step.x = step.x - position.x;
    step.y = step.y - position.y;
    step.z = step.z - position.z;
    f32 share = 1.0f / static_cast<f32>(chain->segments);
    step.x = step.x * share;
    step.y = step.y * share;
    step.z = step.z * share;
    f32 restLength = __builtin_sqrtf(step.x * step.x + step.y * step.y + step.z * step.z);
    for (u32 index = 0; index < chain->springs.count; index++)
    {
        Spring* spring = chain->springs.data[index];
        SpringPoint* point = spring->start;
        point->position = position;
        StopSpringPoint(point);
        spring->restLength = restLength;
        position.x = position.x + step.x;
        position.y = position.y + step.y;
        position.z = position.z + step.z;
    }

    chain->last->position = position;
    StopSpringPoint(chain->last);
}

void AddSpringChain(SpringBody* body, SpringChain* chain)
{
    body->chains.Append(chain);
}

void SetPointsDrag(f32 drag, SpringBody* body)
{
    for (u32 index = 0; index < body->points.count; index++)
    {
        body->points.data[index]->drag = drag;
    }
}

void StartSpringForces(SpringBody* body, const Vector4* force, const Vector4* gravity)
{
    for (u32 index = 0; index < body->points.count; index++)
    {
        SpringPoint* point = body->points.data[index];
        if (point->pinned != 0)
        {
            Clear(&point->force);
            continue;
        }

        Vector4 total;
        if (gravity == nullptr)
        {
            Clear(&total);
        }
        else
        {
            total = *gravity;
            total.x = total.x * point->mass;
            total.y = total.y * point->mass;
            total.z = total.z * point->mass;
        }

        if (force != nullptr)
        {
            total.x = total.x + force->x;
            total.y = total.y + force->y;
            total.z = total.z + force->z;
        }

        point->force = total;
    }
}

void MoveSpringPoint(f32 seconds, SpringPoint* point, const Vector4* fixedAxis)
{
    f32 share = seconds / point->mass;
    f32 x = point->force.x * share;
    f32 y = point->force.y * share;
    f32 z = point->force.z * share;
    f32 kept = 1.0f - point->drag * seconds;
    point->velocity.x = point->velocity.x + x;
    point->velocity.y = point->velocity.y + y;
    point->velocity.z = point->velocity.z + z;
    if (fixedAxis != nullptr)
    {
        RemoveComponentAlong(&point->velocity, fixedAxis, 0);
    }

    point->position.x = point->position.x + point->velocity.x * seconds;
    point->position.y = point->position.y + point->velocity.y * seconds;
    point->position.z = point->position.z + point->velocity.z * seconds;
    if (0.0f < kept)
    {
        point->velocity.x = point->velocity.x * kept;
        point->velocity.y = point->velocity.y * kept;
        point->velocity.z = point->velocity.z * kept;
    }
    else
    {
        Clear(&point->velocity);
    }
}

void MoveSpringPoints(f32 seconds, SpringBody* body)
{
    const Vector4* fixedAxis = &body->fixedAxis;
    if (__builtin_fabsf(fixedAxis->x) <= Epsilon && __builtin_fabsf(fixedAxis->y) <= Epsilon &&
        __builtin_fabsf(fixedAxis->z) <= Epsilon)
    {
        fixedAxis = nullptr;
    }

    for (u32 index = 0; index < body->points.count; index++)
    {
        SpringPoint* point = body->points.data[index];
        if (point->pinned == 0)
        {
            MoveSpringPoint(seconds, point, fixedAxis);
        }
    }
}

void SimulateSpringBody(SpringBody* body, TimeClock* clock, const Vector4* force, const Vector4* gravity)
{
    if (clock->flags.running == 0)
    {
        return;
    }

    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    u32 passes = body->passes;
    if (LongStep < seconds)
    {
        passes = passes * static_cast<u32>(static_cast<s32>(seconds * PassesPerSecond));
    }

    f32 step = seconds / static_cast<f32>(passes);
    for (u32 pass = 0; pass < passes; pass++)
    {
        StartSpringForces(body, force, gravity);
        for (u32 index = 0; index < body->chains.count; index++)
        {
            body->chains.data[index]->ApplySprings();
        }

        MoveSpringPoints(step, body);
    }
}

void DestroySpringBody(SpringBody* body, u32 destroyFlags)
{
    for (u32 index = 0; index < body->points.count; index++)
    {
        SpringPoint* point = body->points.data[index];
        if (point != nullptr)
        {
            MemoryDeallocate2_(point);
        }
    }

    for (u32 index = 0; index < body->chains.count; index++)
    {
        SpringChain* chain = body->chains.data[index];
        if (chain != nullptr)
        {
            CallVirtual<void>(chain, chain->vtable, SpringChain::DestroySlot, DestroyAndFree);
        }
    }

    if (body->chains.data != nullptr)
    {
        MemoryDeallocate_(body->chains.data);
    }

    if (body->points.data != nullptr)
    {
        MemoryDeallocate_(body->points.data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(body);
    }
}

SpringSkeleton* SpringSkeleton::Construct(SpringSkeleton* skeleton, u32 passes, InstanceContext* instance)
{
    skeleton->vtable = g_SpringSkeletonVTable;
    SpringBody* body = &skeleton->body;
    body->passes = passes;
    skeleton->blend = 0.0f;
    MakeArray(&body->points);
    MakeArray(&body->chains);
    Clear(&body->fixedAxis);
    skeleton->instance = instance != nullptr ? AddReference(instance) : nullptr;
    // Blended out, nothing asked for
    skeleton->flags.value = 0;
    skeleton->flags.requestedBlend = BlendNone;
    return skeleton;
}

void SpringSkeleton::BaseDestroy(u32 destroyFlags)
{
    vtable = g_SpringSkeletonVTable;
    RemoveReference(&instance);
    DestroySpringBody(&body, DestroyOnly);
    vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 SpringSkeleton::AttachToInstance()
{
    if (flags.attached != 0)
    {
        return 0;
    }

    OgiAnimator* animator = AnimatorOf(this);
    if (animator == nullptr)
    {
        return 0;
    }

    flags.attached = 1;
    AttachVirtual(animator);
    return 1;
}

u32 SpringSkeleton::DetachFromInstance()
{
    if (flags.attached == 0)
    {
        return 0;
    }

    OgiAnimator* animator = AnimatorOf(this);
    if (animator == nullptr)
    {
        return 0;
    }

    flags.attached = 0;
    DetachVirtual(animator);
    return 1;
}

void SpringSkeleton::StepBlend(TimeClock* clock)
{
    u32 requested = flags.requestedBlend;
    if (requested != BlendNone)
    {
        flags.blend = requested;
        blendStart = static_cast<s32>(clock->time);
        flags.requestedBlend = BlendNone;
    }

    bool done;
    switch (flags.blend)
    {
    case BlendedIn:
        blend = 1.0f;
        break;
    case BlendingIn:
        blend = BlendShare(this, clock, &done);
        if (done)
        {
            flags.requestedBlend = BlendedIn;
        }

        break;
    case BlendingOut:
    {
        f32 share = BlendShare(this, clock, &done);
        if (done)
        {
            flags.requestedBlend = BlendedOut;
        }

        blend = 1.0f - share;
        break;
    }
    default:
        break;
    }

    if (flags.blend == BlendedOut || InstanceOf(this) == nullptr)
    {
        return;
    }

    ObjectPlace* place = InstanceOf(this)->place;
    RotateAndTranslate(place);
    instanceMatrix = place->matrix;
    VuInvertRigid(&toInstance, &instanceMatrix);
    if (flags.turnsAxis != 0)
    {
        VuRotateVector(&toInstance, &axis, &instanceAxis);
        flags.axisTurned = 1;
    }
}

void SpringSkeleton::BlendIn(s32 ticks)
{
    if (AttachToInstance() == 0)
    {
        return;
    }

    if (ticks == 0)
    {
        flags.requestedBlend = BlendedIn;
        return;
    }

    blendTicks = ticks;
    blendStart = 0;
    flags.requestedBlend = BlendingIn;
}

void SpringSkeleton::PoseJointFromPoints(u32 joint, u32 toward, JointAnimator* animator)
{
    u8 jointId = static_cast<u8>(joint);
    u8 towardId = static_cast<u8>(toward);
    if (towardId != GameOGI::NoJoint)
    {
        TurnJointToward(jointId, towardId, animator);
        return;
    }

    if (flags.blend == BlendedOut)
    {
        return;
    }

    Vector4 place = body.points.data[jointId]->position;
    JointAnimation* parent = animator->animation->parent;
    VuTransformPoint(&toInstance, &place, &place);
    if (parent != nullptr)
    {
        Matrix4x4 fromParent;
        VuInvertRigid(&fromParent, &parent->transform);
        VuTransformPoint(&fromParent, &place, &place);
    }

    TranslateJoint(blend, animator, &place);
}

void SpringSkeleton::TurnJointToward(u32 joint, u32 toward, JointAnimator* animator)
{
    u8 jointId = static_cast<u8>(joint);
    u8 towardId = static_cast<u8>(toward);
    if (flags.blend == BlendedOut)
    {
        return;
    }

    JointAnimation* animation = animator->animation;
    JointAnimation* towardJoint = FindChildJoint(animation, towardId, 1);
    JointAnimation* parent = animation->parent;
    const OgiJoint* towardBind = towardJoint->joint;
    SpringPoint** points = body.points.data;
    SpringPoint* from = points[jointId];
    SpringPoint* to = points[towardId];
    // The points in the instance's space, and the way from one to the other
    Vector4 start;
    Vector4 way;
    VuTransformPoint(&toInstance, &from->position, &start);
    VuTransformPoint(&toInstance, &to->position, &way);
    // The way the joint's bind pose has to the other joint, in the same space
    Matrix4x4 jointMatrix;
    CreateJointTransform(animator, parent, 0, &jointMatrix);
    Vector4 bindWay;
    VuTransformPoint(&jointMatrix, &towardBind->bindPosition, &bindWay);
    const Vector4* jointPlace = RowOf(&jointMatrix, 3);
    bindWay.x = bindWay.x - jointPlace->x;
    bindWay.y = bindWay.y - jointPlace->y;
    bindWay.z = bindWay.z - jointPlace->z;
    way.x = way.x - start.x;
    way.y = way.y - start.y;
    way.z = way.z - start.z;
    if (flags.axisTurned != 0)
    {
        RemoveComponentAlong(&bindWay, &instanceAxis, 0);
        RemoveComponentAlong(&way, &instanceAxis, 0);
    }

    f32 inverse = InverseLength(&bindWay, LengthEpsilon);
    bindWay.x = bindWay.x * inverse;
    bindWay.y = bindWay.y * inverse;
    bindWay.z = bindWay.z * inverse;
    inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    s32 angle;
    AngleBetweenDirections(&angle, &bindWay, &way);
    s32 turn = static_cast<s32>(static_cast<f32>(angle) * blend);
    Vector4 rotation;
    if (turn == 0)
    {
        rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    else if (flags.axisTurned != 0)
    {
        angle = turn;
        RotationAboutAxis(&rotation, &instanceAxis, &angle, 0);
    }
    else
    {
        Vector4 about;
        about.x = bindWay.y * way.z - bindWay.z * way.y;
        about.z = bindWay.x * way.y - bindWay.y * way.x;
        about.y = bindWay.z * way.x - bindWay.x * way.z;
        about.w = 1.0f;
        angle = turn;
        RotationAboutAxis(&rotation, &about, &angle, 0);
    }

    if (parent != nullptr)
    {
        Matrix4x4 fromParent;
        VuInvertRigid(&fromParent, &parent->transform);
        VuTransformPoint(&fromParent, &start, &start);
    }

    AddJointRotation(animator, &rotation);
    TranslateJoint(blend, animator, &start);
}
