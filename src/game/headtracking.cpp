#include "game/objectnode.h"

#include "game/animation.h"
#include "game/clock.h"
#include "game/events.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/place.h"
#include "game/reference.h"

// An object node's head tracking: the joints of its head (or its instance, steered) turned toward what it looks at, which it
// takes from the scripts, the noises it hears and the target it remembers, until it lets go of it and turns back to rest

extern "C"
{
    extern const GccVTableEntry g_HeadTrackingVTable[] RETAIL(D_00300EB8);
}

namespace
{
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// 2π / 65536's inverse
constexpr f32 RadiansToAngle = 0x1.45f306p+13f;
constexpr u32 ObjectNodeKind = 1;
constexpr u32 PositionRow = 3;

// The instance a reference is to (none without one)
InstanceContext* TargetOf(const Reference* reference)
{
    return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
}

// A reference block counted once more (a handle copied)
void CountReference(Reference* reference)
{
    u32 count = ((reference->value & ReferenceBits::CountMask) + 1) & ReferenceBits::CountMask;
    reference->value = (reference->value & ~ReferenceBits::CountMask) | count;
}

bool SteersInstance(const HeadTracking* tracking)
{
    return (tracking->settings->bits & HeadTrackingSettings::SteersInstance) != 0;
}

f32 Dot(const Vector4* a, const Vector4* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// Where a target is looked at: its agent's centre of its collision box (its vtable's slot 14), else the middle of its box
void LookPosition(InstanceContext* target, Vector4* position)
{
    constexpr u32 CollisionCentreSlot = 14;
    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&target->nodes, ObjectNodeKind));
    if (node != nullptr)
    {
        Agent* agent = node->agent;
        CallVirtual<void>(agent, agent->vtable, CollisionCentreSlot, position);
        return;
    }

    const Box* box = target->CollisionBox();
    *position = box->max;
    position->x = (position->x - box->min.x) * 0.5f + box->min.x;
    position->y = (position->y - box->min.y) * 0.5f + box->min.y;
    position->z = (position->z - box->min.z) * 0.5f + box->min.z;
}
}

HeadTracking* HeadTracking::ConstructTurner(HeadTracking* tracking, ObjectNode* node)
{
    tracking->vtable = g_HeadTrackingVTable;
    tracking->node = node;
    tracking->direction = {0.0f, 0.0f, -1.0f, 1.0f};
    tracking->unknown90 = {0.0f, 0.0f, -1.0f, 1.0f};
    tracking->exitPoint = NoJoint;
    tracking->joint = NoJoint;
    tracking->secondJoint = NoJoint;
    tracking->damping = 1.0f;
    tracking->bits &= ~u64{BitLimits | BitReturning | BitHooked};
    ResetHeadTurns(tracking);
    return tracking;
}

HeadTracking* HeadTracking::Construct(HeadTracking* tracking, ObjectNode* node)
{
    ConstructTurner(tracking, node);
    tracking->target = nullptr;
    tracking->remembered = nullptr;
    tracking->flags &= ~u64{Flag0 | FlagStops | FlagAgentTurns | FlagIgnoredByLook | FlagHasTarget};
    tracking->last = nullptr;
    tracking->settings = nullptr;
    tracking->weight = 0.0f;
    tracking->time = 0;
    return tracking;
}

u32 HeadTracking::PoseJoint(JointAnimator* animator, Matrix4x4* jointMatrix)
{
    InstanceContext* instance = node->owner;
    if (exitPoint == NoJoint)
    {
        CreateJointTransform(animator, animator->animation->parent, 0, jointMatrix);
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        MultiplyReversed(jointMatrix, &place->matrix, &matrix);
    }
    else
    {
        OgiAnimator* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNode::NodeKind))->animator;
        SizedArray<ExitPointAnimation*>* exitPoints = model->exitPoints;
        matrix = UpdateExitPointMatrix(exitPoints != nullptr ? exitPoints->data[exitPoint] : nullptr)->matrix;
    }

    s32 x = pitchAngle;
    s32 y = yawAngle;
    Vector4 rotation;
    GetRotationXY(&rotation, &x, &y);
    AddJointRotation(animator, &rotation);
    return 1;
}

u32 TurnHead(HeadTracking* tracking, InstanceContext* instance, const Vector4* target)
{
    tracking->bits &= ~u64{HeadTracking::BitLimits};
    f32 elapsed = static_cast<f32>(static_cast<s32>(GetContextClock(instance)->advance)) * g_SecondsPerClockUnit;
    if ((tracking->bits & HeadTracking::BitReturning) != 0)
    {
        // A spring pulling the turns back to rest
        constexpr f32 Pull = 1000.0f;
        constexpr f32 Drag = 250.0f;
        f32 pull = tracking->stiffness * Pull;
        f32 drag = tracking->stiffness * (tracking->damping * Drag);
        tracking->pitchSpeed = tracking->pitchSpeed + (-tracking->pitch * pull - tracking->pitchSpeed * drag) * elapsed;
        tracking->yawSpeed = tracking->yawSpeed + (-tracking->yaw * pull - tracking->yawSpeed * drag) * elapsed;
    }
    else
    {
        // A spring turning the head's y axis down to the target and its x axis across to it
        constexpr f32 Pull = 2000.0f;
        constexpr f32 Drag = 166.0f;
        Vector4& direction = tracking->direction;
        direction = *target;
        const Vector4* head = RowOf(&tracking->matrix, PositionRow);
        direction.x = direction.x - head->x;
        direction.y = direction.y - head->y;
        direction.z = direction.z - head->z;
        f32 pull = tracking->stiffness * Pull;
        f32 drag = tracking->stiffness * (tracking->damping * Drag);
        f32 scale = InverseLength(&direction, LengthEpsilon);
        direction.x = direction.x * scale;
        direction.y = direction.y * scale;
        direction.z = direction.z * scale;
        f32 down = Dot(RowOf(&tracking->matrix, 1), &direction);
        f32 across = Dot(RowOf(&tracking->matrix, 0), &direction);
        tracking->pitchSpeed = tracking->pitchSpeed + (down * -pull - tracking->pitchSpeed * drag) * elapsed;
        tracking->yawSpeed = tracking->yawSpeed + (across * pull - tracking->yawSpeed * drag) * elapsed;
    }

    tracking->pitch = tracking->pitch + tracking->pitchSpeed * elapsed;
    tracking->yaw = tracking->yaw + tracking->yawSpeed * elapsed;
    if (tracking->pitch < tracking->minPitch)
    {
        tracking->pitch = tracking->minPitch;
        tracking->bits |= HeadTracking::BitPitchBelow | HeadTracking::BitLimited;
    }
    else if (tracking->maxPitch < tracking->pitch)
    {
        tracking->pitch = tracking->maxPitch;
        tracking->bits |= HeadTracking::BitPitchAbove | HeadTracking::BitLimited;
    }

    if (tracking->yaw < -tracking->maxYaw)
    {
        tracking->yaw = -tracking->maxYaw;
        tracking->bits |= HeadTracking::BitYawBelow | HeadTracking::BitLimited;
    }
    else if (tracking->maxYaw < tracking->yaw)
    {
        tracking->yaw = tracking->maxYaw;
        tracking->bits |= HeadTracking::BitYawAbove | HeadTracking::BitLimited;
    }

    tracking->rollAngle = 0;
    tracking->pitchAngle = static_cast<s32>(tracking->pitch * RadiansToAngle);
    tracking->yawAngle = static_cast<s32>(tracking->yaw * RadiansToAngle);
    return 0;
}

void SetUpHeadTracking(HeadTracking* tracking, const HeadTrackingSettings* settings, TimeClock* clock, ObjectNode* node)
{
    // The agent's vtable function that says whether it turns the head itself
    constexpr u32 AgentTurnsHeadSlot = 12;
    constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
    tracking->settings = settings;
    tracking->unknownC4 = RandomFromFloat(2.0f, 8.0f);
    if ((settings->bits & HeadTrackingSettings::SteersInstance) == 0)
    {
        tracking->joint = settings->bits & 0xFF;
        tracking->secondJoint = settings->bits >> 8 & 0xFF;
        tracking->stiffness = settings->stiffness;
        tracking->bits &= ~u64{HeadTracking::BitReturning};
        tracking->maxPitch = static_cast<f32>(settings->positivePitch) * AngleToRadians;
        tracking->maxYaw = static_cast<f32>(settings->yawLimit) * AngleToRadians;
        tracking->minPitch = -(static_cast<f32>(settings->negativePitch) * AngleToRadians);
        tracking->damping = settings->damping;
        tracking->exitPoint = settings->bits >> 16 & 0xFF;
    }

    tracking->time = clock->time;
    tracking->flags = (tracking->flags & ~u64{HeadTracking::FlagStops}) | HeadTracking::FlagHasTarget;
    Agent* agent = node->agent;
    u32 agentTurns = CallVirtual<u32>(agent, agent->vtable, AgentTurnsHeadSlot);
    tracking->flags = (tracking->flags & ~u64{HeadTracking::FlagAgentTurns}) | u64{agentTurns & 1} << 4;
    StartHeadTracking(tracking, node);
}

void StartHeadTracking(HeadTracking* tracking, ObjectNode* node)
{
    if (!SteersInstance(tracking) && (tracking->bits & HeadTracking::BitHooked) == 0)
    {
        if ((tracking->flags & HeadTracking::FlagAgentTurns) == 0)
        {
            OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, ModelNode::NodeKind))->animator;
            AddJointCallback(animator, tracking->joint, tracking);
            tracking->bits |= HeadTracking::BitHooked;
            if (tracking->secondJoint != HeadTracking::NoJoint)
            {
                AddJointCallback(animator, tracking->secondJoint, tracking);
            }
        }

        tracking->bits &= ~u64{HeadTracking::BitReturning};
    }

    tracking->flags =
        (tracking->flags | HeadTracking::FlagTracking) & ~u64{HeadTracking::FlagStops | HeadTracking::FlagIgnoredByLook};
}

void StepHeadTracking(HeadTracking* tracking, TimeClock* clock, ObjectNode* node)
{
    // It isn't stepped while its instance's seen stamp is past this less its settings' limit (when that's below 0xFF)
    constexpr u32 UnseenLimit = 0x640;
    constexpr u32 NoUnseenLimit = 0xFF;
    // How its weight fades each step, the least it looks at a target with, the turns it counts as back at rest
    constexpr f32 WeightFade = Rounded(0.97);
    constexpr f32 LeastWeight = Rounded(0.05);
    constexpr f32 AtRest = Rounded(0.005);
    constexpr f32 MostLean = 90.0f;
    InstanceContext* instance = node->owner;
    const HeadTrackingSettings* settings = tracking->settings;
    u32 seen = instance->seen[0] | instance->seen[1] << 8 | instance->seen[2] << 16;
    if (settings->unseenLimit < NoUnseenLimit && UnseenLimit - settings->unseenLimit < seen)
    {
        return;
    }

    bool steers = (settings->bits & HeadTrackingSettings::SteersInstance) != 0;
    bool agentTurns = (tracking->flags & HeadTracking::FlagAgentTurns) != 0;
    bool hooked = (tracking->bits & HeadTracking::BitHooked) != 0;
    if ((tracking->flags & HeadTracking::FlagTracking) == 0 || (!agentTurns && !steers && !hooked))
    {
        if (steers)
        {
            return;
        }

        InstanceContext* last = TargetOf(tracking->last);
        if (last != nullptr && (last->flags & ReferencedObject::FlagVisible) != 0)
        {
            HeadTrackingIdle(tracking, clock, instance);
        }

        return;
    }

    tracking->weight = tracking->weight * WeightFade;
    InstanceContext* target = TargetOf(tracking->target);
    InstanceContext* remembered = TargetOf(tracking->remembered);
    if (remembered != nullptr && tracking->weight < tracking->rememberedWeight)
    {
        // Back to the remembered target once what it looks at fades below it. Retail bug: the rest of this step still looks
        // at the one it had
        AssignReference(&tracking->target, remembered);
        tracking->weight = tracking->rememberedWeight;
        tracking->time = GetContextClock(remembered)->time;
    }

    Vector4 position;
    if (!steers && !agentTurns && (tracking->bits & HeadTracking::BitReturning) != 0)
    {
        if (!(__builtin_fabsf(tracking->pitch) < AtRest && __builtin_fabsf(tracking->yaw) < AtRest))
        {
            // Still turning back (the target goes unread)
            if ((tracking->bits & HeadTracking::BitHooked) != 0)
            {
                TurnHead(tracking, instance, &position);
            }

            return;
        }

        // Back at rest: its joints unhooked, and it stops when it was told to
        tracking->weight = 0.0f;
        LetGoOfHeadTracking(tracking, node);
        u64 flags = tracking->flags;
        tracking->flags = flags & ~u64{HeadTracking::FlagHasTarget};
        tracking->bits &= ~u64{HeadTracking::BitReturning};
        if ((flags & HeadTracking::FlagStops) != 0)
        {
            tracking->flags = flags & ~u64{HeadTracking::FlagHasTarget | HeadTracking::FlagTracking | HeadTracking::FlagStops};
        }

        return;
    }

    if (target != nullptr && !(tracking->weight < LeastWeight) && (target->flags & ReferencedObject::FlagVisible) != 0)
    {
        tracking->flags |= HeadTracking::FlagHasTarget;
        if ((tracking->flags & HeadTracking::FlagAgentTurns) != 0)
        {
            return;
        }

        LookPosition(target, &position);
        if (steers)
        {
            if ((settings->bits & HeadTrackingSettings::SteersFacing) != 0)
            {
                SteerBodyTowards(settings->stiffness, 0.0f, instance, &position, 1);
            }
            else
            {
                SteerTowards(settings->stiffness, 0.0f, MostLean, instance, &position);
            }

            return;
        }

        if ((tracking->bits & HeadTracking::BitHooked) != 0)
        {
            TurnHead(tracking, instance, &position);
        }

        return;
    }

    if (remembered != nullptr)
    {
        AssignReference(&tracking->target, remembered);
        AssignReference(&tracking->last, remembered);
        tracking->weight = tracking->rememberedWeight;
        tracking->time = GetContextClock(remembered)->time;
        tracking->flags |= HeadTracking::FlagHasTarget;
        return;
    }

    if ((tracking->flags & HeadTracking::FlagHasTarget) != 0)
    {
        LetGoOfHeadTarget(tracking, 0);
        tracking->flags &= ~u64{HeadTracking::FlagHasTarget};
        return;
    }

    HeadTrackingIdle(tracking, clock, instance);
}

u32 HeadTrackingIdle(HeadTracking*, TimeClock*, InstanceContext*)
{
    return 0;
}

void LetGoOfHeadTarget(HeadTracking* tracking, u32 stops)
{
    tracking->flags = (tracking->flags & ~u64{HeadTracking::FlagStops}) | u64{stops & 1} << 2;
    if (TargetOf(tracking->target) != nullptr)
    {
        RemoveReference(&tracking->target);
        tracking->target = nullptr;
    }

    if (!SteersInstance(tracking))
    {
        tracking->bits = (tracking->bits | HeadTracking::BitReturning) & ~u64{HeadTracking::Bit31};
    }
}

void HearNoise(HeadTracking* tracking, const NoiseEvent* noise, ObjectNode* node)
{
    if ((tracking->settings->bits & HeadTrackingSettings::IgnoresNoises) != 0 || (tracking->flags & HeadTracking::FlagStops) != 0)
    {
        return;
    }

    f32 loudness = noise->loudness;
    if (!(tracking->weight <= loudness))
    {
        return;
    }

    // The noise's maker taken as the target and the last one (the event's reference to it counted once more each)
    if (tracking->target != noise->argument)
    {
        RemoveReference(&tracking->target);
        tracking->target = noise->argument;
        if (tracking->target != nullptr)
        {
            CountReference(tracking->target);
        }
    }

    if (tracking->last != tracking->target)
    {
        RemoveReference(&tracking->last);
        tracking->last = tracking->target;
        if (tracking->last != nullptr)
        {
            CountReference(tracking->last);
        }
    }

    tracking->weight = loudness;
    tracking->time = GetContextClock(TargetOf(tracking->target))->time;
    if (!SteersInstance(tracking))
    {
        tracking->bits &= ~u64{HeadTracking::BitReturning};
    }

    u64 flags = (tracking->flags & ~u64{HeadTracking::FlagStops}) | HeadTracking::FlagHasTarget;
    tracking->flags = flags;
    if ((flags & HeadTracking::FlagTracking) == 0 || (tracking->bits & HeadTracking::BitHooked) == 0)
    {
        StartHeadTracking(tracking, node);
    }
}

EABI_EXPORT(FUN_00237db0, TrackHead);

void TrackHead(f32 weight, HeadTracking* tracking, InstanceContext* target, ObjectNode* node, u32 remembered)
{
    tracking->flags &= ~u64{HeadTracking::FlagStops};
    if (!SteersInstance(tracking))
    {
        tracking->bits &= ~u64{HeadTracking::BitReturning};
    }

    if (remembered != 0)
    {
        AssignReference(&tracking->remembered, target);
        tracking->rememberedWeight = weight;
    }

    AssignReference(&tracking->target, target);
    AssignReference(&tracking->last, target);
    tracking->weight = weight;
    tracking->time = GetContextClock(TargetOf(tracking->target))->time;
    tracking->flags |= HeadTracking::FlagHasTarget;
    StartHeadTracking(tracking, node);
}
