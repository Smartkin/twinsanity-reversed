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
// The instance a reference is to (none without one)
InstanceContext* TargetOf(const Reference* reference)
{
    return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
}

// A reference block counted once more (a handle copied)
void CountReference(Reference* reference)
{
    reference->bits.count++;
}

bool SteersInstance(const HeadTracking* tracking)
{
    return tracking->settings->bits.steering != 0;
}

f32 Dot(const Vector4* a, const Vector4* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// Where a target is looked at: its agent's centre of its collision box (its vtable's slot 14), else the middle of its box
void LookPosition(InstanceContext* target, Vector4* position)
{
    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&target->nodes, NodeObject));
    if (node != nullptr)
    {
        Agent* agent = node->agent;
        CallVirtual<void>(agent, agent->vtable, Agent::CollisionCenterSlot, position);
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
    tracking->unused90 = {0.0f, 0.0f, -1.0f, 1.0f};
    tracking->bits.exitPoint = GameOGI::NoExitPoint;
    tracking->bits.joint = GameOGI::NoJoint;
    tracking->bits.secondJoint = GameOGI::NoJoint;
    tracking->damping = 1.0f;
    tracking->bits.value &= ~HeadTrackingState::LimitsMask;
    tracking->bits.returning = 0;
    tracking->bits.hooked = 0;
    ResetHeadTurns(tracking);
    return tracking;
}

HeadTracking* HeadTracking::Construct(HeadTracking* tracking, ObjectNode* node)
{
    ConstructTurner(tracking, node);
    tracking->target = nullptr;
    tracking->remembered = nullptr;
    tracking->flags.unused0 = 0;
    tracking->flags.stops = 0;
    tracking->flags.agentTurns = 0;
    tracking->flags.ignoredByLook = 0;
    tracking->flags.hasTarget = 0;
    tracking->last = nullptr;
    tracking->settings = nullptr;
    tracking->weight = 0.0f;
    tracking->time = 0;
    return tracking;
}

u32 HeadTracking::PoseJoint(JointAnimator* animator, Matrix4x4* jointMatrix)
{
    InstanceContext* instance = node->owner;
    if (bits.exitPoint == GameOGI::NoExitPoint)
    {
        CreateJointTransform(animator, animator->animation->parent, 0, jointMatrix);
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        MultiplyReversed(jointMatrix, &place->matrix, &matrix);
    }
    else
    {
        OgiAnimator* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel))->animator;
        SizedArray<ExitPointAnimation*>* exitPoints = model->exitPoints;
        matrix = UpdateExitPointMatrix(exitPoints != nullptr ? exitPoints->data[bits.exitPoint] : nullptr)->matrix;
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
    tracking->bits.value &= ~HeadTrackingState::LimitsMask;
    f32 elapsed = static_cast<f32>(static_cast<s32>(GetContextClock(instance)->advance)) * g_SecondsPerClockUnit;
    if (tracking->bits.returning)
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
        tracking->bits.pitchBelow = 1;
        tracking->bits.limited = 1;
    }
    else if (tracking->maxPitch < tracking->pitch)
    {
        tracking->pitch = tracking->maxPitch;
        tracking->bits.pitchAbove = 1;
        tracking->bits.limited = 1;
    }

    if (tracking->yaw < -tracking->maxYaw)
    {
        tracking->yaw = -tracking->maxYaw;
        tracking->bits.yawBelow = 1;
        tracking->bits.limited = 1;
    }
    else if (tracking->maxYaw < tracking->yaw)
    {
        tracking->yaw = tracking->maxYaw;
        tracking->bits.yawAbove = 1;
        tracking->bits.limited = 1;
    }

    tracking->rollAngle = 0;
    tracking->pitchAngle = static_cast<s32>(tracking->pitch * RadiansToAngle);
    tracking->yawAngle = static_cast<s32>(tracking->yaw * RadiansToAngle);
    return 0;
}

void SetUpHeadTracking(HeadTracking* tracking, const HeadTrackingSettings* settings, TimeClock* clock, ObjectNode* node)
{
    tracking->settings = settings;
    tracking->unusedC4 = RandomFromFloat(2.0f, 8.0f);
    if (settings->bits.steering == 0)
    {
        tracking->bits.joint = settings->bits.joint;
        tracking->bits.secondJoint = settings->bits.secondJoint;
        tracking->stiffness = settings->stiffness;
        tracking->bits.returning = 0;
        tracking->maxPitch = static_cast<f32>(settings->positivePitch) * AngleToRadians;
        tracking->maxYaw = static_cast<f32>(settings->yawLimit) * AngleToRadians;
        tracking->minPitch = -(static_cast<f32>(settings->negativePitch) * AngleToRadians);
        tracking->damping = settings->damping;
        tracking->bits.exitPoint = settings->bits.exitPoint;
    }

    tracking->time = clock->time;
    tracking->flags.stops = 0;
    tracking->flags.hasTarget = 1;
    // The playable characters turn their heads themselves (their look)
    Agent* agent = node->agent;
    u32 agentTurns = CallVirtual<u32>(agent, agent->vtable, Agent::IsCharacterSlot);
    tracking->flags.agentTurns = agentTurns;
    StartHeadTracking(tracking, node);
}

void StartHeadTracking(HeadTracking* tracking, ObjectNode* node)
{
    if (!SteersInstance(tracking) && !tracking->bits.hooked)
    {
        if (!tracking->flags.agentTurns)
        {
            OgiAnimator* animator = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel))->animator;
            AddJointCallback(animator, tracking->bits.joint, tracking);
            tracking->bits.hooked = 1;
            if (tracking->bits.secondJoint != GameOGI::NoJoint)
            {
                AddJointCallback(animator, tracking->bits.secondJoint, tracking);
            }
        }

        tracking->bits.returning = 0;
    }

    tracking->flags.tracking = 1;
    tracking->flags.stops = 0;
    tracking->flags.ignoredByLook = 0;
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
    InstanceContext* instance = node->owner;
    const HeadTrackingSettings* settings = tracking->settings;
    u32 seen = instance->seen;
    if (settings->unseenLimit < NoUnseenLimit && UnseenLimit - settings->unseenLimit < seen)
    {
        return;
    }

    bool steers = settings->bits.steering != 0;
    bool agentTurns = tracking->flags.agentTurns;
    bool hooked = tracking->bits.hooked;
    if (!tracking->flags.tracking || (!agentTurns && !steers && !hooked))
    {
        if (steers)
        {
            return;
        }

        InstanceContext* last = TargetOf(tracking->last);
        if (last != nullptr && last->flags.visible)
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
    if (!steers && !agentTurns && tracking->bits.returning)
    {
        if (!(__builtin_fabsf(tracking->pitch) < AtRest && __builtin_fabsf(tracking->yaw) < AtRest))
        {
            // Still turning back (the target goes unread)
            if (tracking->bits.hooked)
            {
                TurnHead(tracking, instance, &position);
            }

            return;
        }

        // Back at rest: its joints unhooked, and it stops when it was told to
        tracking->weight = 0.0f;
        LetGoOfHeadTracking(tracking, node);
        tracking->flags.hasTarget = 0;
        tracking->bits.returning = 0;
        if (tracking->flags.stops)
        {
            tracking->flags.tracking = 0;
            tracking->flags.stops = 0;
        }

        return;
    }

    if (target != nullptr && !(tracking->weight < LeastWeight) && target->flags.visible)
    {
        tracking->flags.hasTarget = 1;
        if (tracking->flags.agentTurns)
        {
            return;
        }

        LookPosition(target, &position);
        if (steers)
        {
            if ((settings->bits.steering & HeadTrackingSettings::SteersFacing) != 0)
            {
                SteerBodyTowards(settings->stiffness, 0.0f, instance, &position, 1);
            }
            else
            {
                SteerTowards(settings->stiffness, 0.0f, SteerMostLean, instance, &position);
            }

            return;
        }

        if (tracking->bits.hooked)
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
        tracking->flags.hasTarget = 1;
        return;
    }

    if (tracking->flags.hasTarget)
    {
        LetGoOfHeadTarget(tracking, 0);
        tracking->flags.hasTarget = 0;
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
    tracking->flags.stops = stops;
    if (TargetOf(tracking->target) != nullptr)
    {
        RemoveReference(&tracking->target);
        tracking->target = nullptr;
    }

    if (!SteersInstance(tracking))
    {
        tracking->bits.returning = 1;
        tracking->bits.unused31 = 0;
    }
}

void HearNoise(HeadTracking* tracking, const NoiseEvent* noise, ObjectNode* node)
{
    if (tracking->settings->bits.ignoresNoises || tracking->flags.stops)
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
        tracking->bits.returning = 0;
    }

    tracking->flags.stops = 0;
    tracking->flags.hasTarget = 1;
    if (!tracking->flags.tracking || !tracking->bits.hooked)
    {
        StartHeadTracking(tracking, node);
    }
}

EABI_EXPORT(FUN_00237db0, TrackHead);

void TrackHead(f32 weight, HeadTracking* tracking, InstanceContext* target, ObjectNode* node, u32 remembered)
{
    tracking->flags.stops = 0;
    if (!SteersInstance(tracking))
    {
        tracking->bits.returning = 0;
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
    tracking->flags.hasTarget = 1;
    StartHeadTracking(tracking, node);
}
