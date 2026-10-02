#include "game/animation.h"

#include "gcc2.h"
#include "game/chunkdata.h"
#include "game/chunkfiles.h"
#include "game/clock.h"
#include "game/disk.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/graphicstables.h"
#include "game/resources.h"
#include "retail/libc.h"
#include "game/stream.h"
#include "game/widgets.h"
#include "platform/graphics.h"
#include "platform/math.h"

EABI_EXPORT(ScaleJoint, ScaleJoint);
EABI_EXPORT(TranslateJoint, TranslateJoint);
EABI_EXPORT(DoAnimation, DoAnimation);
EABI_EXPORT(AnimateJoint, AnimateJoint);
EABI_EXPORT(FUN_002941b0, BlendShapesFrame);
EABI_EXPORT(FUN_00297f98, ConstructAnimationSettings);

namespace
{
// A track's turns: 16 bits of a turn shifted up to 65536ths, as radians
constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr s32 HalfTurn = 0x8000;
constexpr s32 Turn = 0x10000;

// Translations and scales in 4096ths
constexpr f32 TrackUnit = 0x1p-12f;

// A value moved a share of the way toward another
f32 Toward(f32 from, f32 to, f32 share)
{
    return from + (to - from) * share;
}

// Three channels' values of this frame and the next (static ones the same in both)
void ReadChannels(TrackReader* reader, f32* now, f32* next)
{
    for (u32 channel = 0; channel < 3; channel++)
    {
        if ((reader->statics & 1) == 0)
        {
            now[channel] = static_cast<f32>(*reader->current) * TrackUnit;
            next[channel] = static_cast<f32>(*reader->next) * TrackUnit;
            reader->current++;
            reader->next++;
        }
        else
        {
            f32 value = static_cast<f32>(*reader->staticValues) * TrackUnit;
            now[channel] = value;
            next[channel] = value;
            reader->staticValues++;
        }

        reader->statics >>= 1;
        reader->statics2 >>= 1;
    }
}

// The joint a share of the way toward its bind pose (at once from 1 on: the bind rotation, translation and the joints' scale)
void PoseAtBind(f32 share, JointAnimator* animator)
{
    JointStruct* joint = animator->joint;
    if (share < 1.0f)
    {
        Vector4 rotation;
        RotateJoint(share, animator, &joint->bindRotation, &rotation);
        ScaleJoint(share, animator, &g_JointScale);
        TranslateJoint(share, animator, &joint->bindPosition);
        animator->rotation = rotation;
        return;
    }

    animator->flags |= JointAnimator::HasRotation;
    animator->rotation.x = joint->bindRotation.x;
    animator->rotation.y = joint->bindRotation.y;
    animator->rotation.z = joint->bindRotation.z;
    animator->rotation.w = joint->bindRotation.w;
    animator->flags |= JointAnimator::HasScale;
    animator->flags |= JointAnimator::HasTranslation;
    animator->scale = g_JointScale;
    animator->translation = joint->bindPosition;
}
}

void RotateJoint(f32 share, JointAnimator* animator, const Vector4* to, Vector4* out)
{
    alignas(16) Vector4 from;
    if ((animator->flags & JointAnimator::HasRotation) != 0)
    {
        from = animator->rotation;
    }
    else
    {
        animator->flags |= JointAnimator::HasRotation;
        from = animator->joint->bindRotation;
    }

    Platform::Math::SlerpRotations(&from, to, share, out);
}

void DoAnimation(f32 weight, JointAnimator* animator, AnimationChain* chain)
{
    AnimationStatus* status = chain != nullptr ? chain->main : nullptr;
    if (status == nullptr)
    {
        PoseAtBind(weight, animator);
        return;
    }

    do
    {
        f32 share = status->weight * weight;
        AnimationStatus* next = status->next;
        if (status->main != nullptr)
        {
            AnimateJoint(share, status->frameShare, animator, status->main);
        }
        else if (status->blendShapes != nullptr || chain->next == nullptr)
        {
            PoseAtBind(share, animator);
        }
        else
        {
            DoAnimation(share, animator, chain->next);
        }

        status = next;
    } while (status != nullptr);
}

void AnimateJoint(f32 weight, f32 frameShare, JointAnimator* animator, const AnimationData* data)
{
    JointStruct* joint = animator->joint;
    const JointTrackSettings* settings = &data->settings[joint->index];
    TrackReader reader;
    reader.statics = settings->statics;
    reader.statics2 = (1 << (settings->flags >> JointTrackSettings::ChannelsShift & JointTrackSettings::ChannelsMask)) - 1;
    reader.staticValues = data->statics + settings->staticIndex;
    reader.current = data->current + settings->frameIndex;
    reader.next = data->next + settings->frameIndex;
    // Translation, the turns about x, y and z, scale: this frame's and the next's
    Vector4 moveNow = {};
    Vector4 moveNext = {};
    ReadChannels(&reader, &moveNow.x, &moveNext.x);
    Vector4 anglesNow = {};
    Vector4 anglesNext = {};
    for (u32 axis = 0; axis < 3; axis++)
    {
        ReadJointAngle(&reader, &(&anglesNow.x)[axis], &(&anglesNext.x)[axis]);
    }

    anglesNext.w = frameShare;
    Vector4 rotation;
    Vector4 move;
    Platform::Math::EulerRotation(&anglesNow, &anglesNext, &moveNow, &moveNext, &rotation, &move);
    Vector4 scaleNow = {};
    Vector4 scaleNext = {};
    ReadChannels(&reader, &scaleNow.x, &scaleNext.x);
    if ((settings->flags & JointTrackSettings::IndependentScaling) != 0)
    {
        animator->flags |= JointAnimator::ParentScale;
    }

    Vector4 scale;
    Platform::Math::Lerp(&scaleNow, &scaleNext, frameShare, &scale);
    if ((settings->flags & JointTrackSettings::AdditionalRotation) != 0)
    {
        Platform::Math::TurnRotation(&animator->joint->additionalRotation, &rotation);
    }

    if (weight < 1.0f)
    {
        Vector4 blended;
        RotateJoint(weight, animator, &rotation, &blended);
        ScaleJoint(weight, animator, &scale);
        TranslateJoint(weight, animator, &move);
        animator->rotation = blended;
        return;
    }

    animator->scale = scale;
    animator->translation = move;
    animator->flags |= JointAnimator::HasScale | JointAnimator::HasTranslation;
    animator->rotation.x = rotation.x;
    animator->rotation.y = rotation.y;
    animator->rotation.z = rotation.z;
    animator->rotation.w = rotation.w;
    animator->flags |= JointAnimator::HasRotation;
}

void ScaleJoint(f32 share, JointAnimator* animator, const Vector4* scale)
{
    if ((animator->flags & JointAnimator::HasScale) != 0)
    {
        animator->scale.x = Toward(animator->scale.x, scale->x, share);
        animator->scale.y = Toward(animator->scale.y, scale->y, share);
        animator->scale.w = 1.0f;
        animator->scale.z = Toward(animator->scale.z, scale->z, share);
        return;
    }

    animator->scale.x = Toward(g_JointScale.x, scale->x, share);
    animator->scale.y = Toward(g_JointScale.y, scale->y, share);
    animator->scale.w = 1.0f;
    animator->scale.z = Toward(g_JointScale.z, scale->z, share);
    animator->flags |= JointAnimator::HasScale;
}

void TranslateJoint(f32 share, JointAnimator* animator, const Vector4* translation)
{
    if ((animator->flags & JointAnimator::HasTranslation) != 0)
    {
        animator->translation.x = Toward(animator->translation.x, translation->x, share);
        animator->translation.y = Toward(animator->translation.y, translation->y, share);
        animator->translation.w = 1.0f;
        animator->translation.z = Toward(animator->translation.z, translation->z, share);
        return;
    }

    const Vector4* bind = &animator->joint->bindPosition;
    animator->translation.x = Toward(bind->x, translation->x, share);
    animator->translation.y = Toward(bind->y, translation->y, share);
    animator->translation.w = 1.0f;
    animator->translation.z = Toward(bind->z, translation->z, share);
    animator->flags |= JointAnimator::HasTranslation;
}

void ReadJointAngle(TrackReader* reader, f32* now, f32* next)
{
    if ((reader->statics & 1) == 0)
    {
        s32 current = *reader->current << 4;
        s32 coming = *reader->next << 4;
        s32 difference = current - coming;
        // This frame's turn taken round to the next's side
        if (HalfTurn < difference)
        {
            current = current + -Turn;
        }
        else if (difference < -HalfTurn)
        {
            current = current + Turn;
        }

        *now = static_cast<f32>(current) * AngleToRadians;
        *next = static_cast<f32>(coming) * AngleToRadians;
        reader->current++;
        reader->next++;
    }
    else
    {
        f32 value = static_cast<f32>(*reader->staticValues << 4) * AngleToRadians;
        *now = value;
        *next = value;
        reader->staticValues++;
    }

    reader->statics >>= 1;
    reader->statics2 >>= 1;
}

void ClearJointAnimator(JointAnimator* animator)
{
    animator->flags = 0;
}

void CreateJointTransform(JointAnimator* animator, JointAnimation* parent, u32 markDone, Matrix4x4* out)
{
    if ((animator->flags & JointAnimator::Done) != 0)
    {
        return;
    }

    const Vector4* rotation = nullptr;
    const Vector4* parentScale = nullptr;
    const Vector4* scale = nullptr;
    if ((animator->flags & JointAnimator::HasRotation) != 0)
    {
        rotation = &animator->rotation;
        if ((animator->flags & JointAnimator::ParentScale) != 0 && parent != nullptr)
        {
            parentScale = &parent->scale;
        }

        if ((animator->flags & JointAnimator::HasScale) != 0)
        {
            scale = &animator->scale;
        }
    }

    // The scale its children take as their parent's: its own, else its parent's, else 1
    JointAnimation* animation = animator->animation;
    if ((animator->flags & JointAnimator::HasScale) != 0)
    {
        animation->scale = animator->scale;
    }
    else if (parent == nullptr)
    {
        animation->scale = {1.0f, 1.0f, 1.0f, 1.0f};
    }
    else
    {
        animation->scale = parent->scale;
    }

    animation->bits |= JointAnimation::HasScale;
    u32 flags = animator->flags;
    const Vector4* translation = (flags & JointAnimator::HasTranslation) != 0 ? &animator->translation : nullptr;
    const Matrix4x4* parentMatrix = parent != nullptr && (flags & JointAnimator::NoParent) == 0 ? &parent->transform : nullptr;
    if (markDone != 0)
    {
        animator->flags = flags | JointAnimator::Done;
    }

    Platform::Math::JointMatrix(rotation, parentScale, scale, translation, parentMatrix, out);
}

void TransformJoints(JointAnimation* joint, AnimationChain* chain, JointAnimation* parent, JointMatrices* matrices)
{
    constexpr u32 CallbackSlot = 4;
    if (joint->chain != nullptr)
    {
        chain = joint->chain;
    }

    JointAnimator animator;
    animator.animation = joint;
    animator.joint = joint->joint;
    ClearJointAnimator(&animator);
    DoAnimation(1.0f, &animator, chain);
    Matrix4x4 matrix;
    for (JointCallback* callback = joint->callbacks; callback != nullptr; callback = callback->next)
    {
        void* object = callback->object;
        CallVirtual<void>(object, *static_cast<const GccVTableEntry* const*>(object), CallbackSlot, &animator, &matrix);
    }

    CreateJointTransform(&animator, parent, 1, &matrix);
    joint->transform = matrix;
    Matrix4x4* written = matrices->end;
    *written = joint->transform;
    matrices->end = written + 1;
    if ((joint->joint->detail >> 4) < matrices->detail)
    {
        return;
    }

    for (u32 child = 0; child < joint->ChildCount(); child++)
    {
        TransformJoints(joint->children[child], chain, joint, matrices);
    }
}

namespace
{
// A joint's animation of its own (its scale and matrix's values the heap's until it's worked out)
JointAnimation* NewJointAnimation(JointStruct* joint, JointAnimation* parent)
{
    auto* animation = static_cast<JointAnimation*>(MemoryAllocate(sizeof(JointAnimation)));
    animation->joint = joint;
    animation->callbacks = nullptr;
    animation->chain = nullptr;
    animation->parent = parent;
    InitIdentityMatrix(&animation->transform);
    return animation;
}
}

JointAnimation* GetJointAnimationFromParentJoint(JointAnimation* parent, JointStruct* joint)
{
    constexpr u32 MostChildren = 12;
    u32 count = parent->ChildCount();
    if (!(count < MostChildren))
    {
        return nullptr;
    }

    for (u32 child = 0; child < count; child++)
    {
        if (parent->children[child]->joint == joint)
        {
            return parent->children[child];
        }
    }

    JointAnimation* animation = NewJointAnimation(joint, parent);
    animation->bits = 0;
    parent->children[parent->ChildCount()] = animation;
    u32 grown = parent->ChildCount() + 1;
    parent->bits = (parent->bits & ~(JointAnimation::ChildrenMask << JointAnimation::ChildrenShift)) |
                   (grown & JointAnimation::ChildrenMask) << JointAnimation::ChildrenShift;
    return animation;
}

void SetJointAnimations(OgiAnimator* animator, u32 jointCount)
{
    constexpr u8 NoParent = 0xFF;
    for (u32 index = 0; index < jointCount; index++)
    {
        JointStruct* joint = &animator->ogi->joints[index];
        JointAnimation* animation;
        if (joint->parent == NoParent)
        {
            if (animator->root == nullptr)
            {
                JointAnimation* root = NewJointAnimation(joint, nullptr);
                animator->root = root;
                root->bits = 0;
            }

            animation = animator->root;
        }
        else
        {
            animation = GetJointAnimationFromParentJoint(animator->joints->data[joint->parent], joint);
        }

        animator->joints->data[index] = animation;
    }
}

void BlendShapesFrame(f32 weight, f32 frameShare, BlendShapeWeights* weights, const AnimationData* data)
{
    constexpr u32 RingSize = sizeof(g_BlendShapeRing);
    // The channels of the first joint's settings are the shapes
    const JointTrackSettings* settings = data->settings;
    TrackReader reader;
    reader.statics = settings->statics;
    u32 count = settings->flags >> JointTrackSettings::ChannelsShift & JointTrackSettings::ChannelsMask;
    reader.statics2 = (1 << (settings->flags >> JointTrackSettings::ChannelsShift & JointTrackSettings::ChannelsMask)) - 1;
    reader.staticValues = data->statics + settings->staticIndex;
    reader.current = data->current + settings->frameIndex;
    weights->current = weights->next;
    reader.next = data->next + settings->frameIndex;
    if (weights->count < count)
    {
        u8* end = reinterpret_cast<u8*>(g_BlendShapeRing) + RingSize;
        if (end < reinterpret_cast<u8*>(g_BlendShapeRingNext + count))
        {
            g_BlendShapeRingNext = reinterpret_cast<f32*>(end - RingSize);
        }

        weights->count = count;
        weights->room = g_BlendShapeRingNext;
        for (u32 shape = 0; shape < count; shape++)
        {
            weights->room[shape] = 0.0f;
        }

        g_BlendShapeRingNext = g_BlendShapeRingNext + count;
    }

    weights->index = 0;
    weights->next = weights->room;
    for (u32 shape = 0; shape < count; shape++)
    {
        f32 value;
        if ((reader.statics & 1) == 0)
        {
            f32 now = static_cast<f32>(*reader.current) * TrackUnit;
            f32 coming = static_cast<f32>(*reader.next) * TrackUnit;
            reader.current++;
            reader.next++;
            value = coming * frameShare + now * (1.0f - frameShare);
        }
        else
        {
            value = static_cast<f32>(*reader.staticValues) * TrackUnit;
            reader.staticValues++;
        }

        reader.statics >>= 1;
        reader.statics2 >>= 1;
        if (weights->current == nullptr)
        {
            value = value * weight;
        }
        else
        {
            value = value * weight + weights->current[weights->index] * (1.0f - weight);
        }

        weights->next[weights->index++] = value;
    }
}

void FacialAnimation(OgiAnimator* animator, BlendShapeWeights* weights)
{
    AnimationChain* chain = animator->root->chain;
    AnimationStatus* status = chain != nullptr ? chain->blendShapes : nullptr;
    while (status != nullptr)
    {
        AnimationStatus* next = status->next;
        f32 weight = status->weight;
        if (status->blendShapes != nullptr)
        {
            BlendShapesFrame(weight, status->frameShare, weights, status->blendShapes);
        }
        else
        {
            for (u32 shape = 0; shape < weights->count; shape++)
            {
                weights->index = shape;
                f32 value = weights->current != nullptr ? weights->current[weights->index] * (1.0f - weight) : 0.0f;
                weights->next[weights->index++] = value;
            }
        }

        status = next;
    }
}

void ProcessAnimationData(OgiAnimator* animator, JointMatrices* matrices, BlendShapeWeights* weights)
{
    TransformJoints(animator->root, nullptr, nullptr, matrices);
    FacialAnimation(animator, weights);
}

f32 CalculateProgress(const AnimationChain* chain)
{
    f32 longest = 0.0f;
    for (const AnimationStatus* status = chain->main; status != nullptr; status = status->next)
    {
        f32 left = static_cast<f32>(status->length) * g_SecondsPerClockUnit - static_cast<f32>(status->time) * g_SecondsPerClockUnit;
        f32 units = static_cast<f32>(static_cast<s32>(left * g_ClockUnitsPerSecond)) * g_SecondsPerClockUnit;
        if (longest < units)
        {
            longest = units;
        }
    }

    return 0.0f < longest ? longest : 0.0f;
}

void FindFrames(AnimationData* data, u32 frame, u32 next)
{
    AnimationDataInformation* information = data->information;
    u8* base = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
    data->settings = reinterpret_cast<const JointTrackSettings*>(base);
    u8* statics = base + (information->sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings);
    data->statics = reinterpret_cast<const s16*>(statics);
    u32 sections = information->sections;
    u32 perFrame = sections >> AnimationDataInformation::FrameValuesShift;
    u32 staticBytes = sections >> AnimationDataInformation::StaticBytesShift & AnimationDataInformation::StaticBytesMask;
    data->current = reinterpret_cast<const s16*>(statics + staticBytes + frame * perFrame * 2);
    sections = data->information->sections;
    perFrame = sections >> AnimationDataInformation::FrameValuesShift;
    staticBytes = sections >> AnimationDataInformation::StaticBytesShift & AnimationDataInformation::StaticBytesMask;
    data->next = reinterpret_cast<const s16*>(statics + staticBytes + next * perFrame * 2);
}

void SetAnimationData(AnimationStatus* status, u32 main)
{
    AnimationData* data = main != 0 ? status->main : status->blendShapes;
    if (data == nullptr)
    {
        return;
    }

    f32 through = static_cast<f32>(status->time) * g_SecondsPerClockUnit / (static_cast<f32>(status->length) * g_SecondsPerClockUnit);
    u32 frames = status->bits >> AnimationStatus::FramesShift & AnimationStatus::FramesMask;
    u32 last = frames - 1;
    f32 position;
    if ((status->bits & AnimationStatus::Loops) != 0)
    {
        position = static_cast<f32>(static_cast<s32>(frames)) * through;
    }
    else
    {
        position = static_cast<f32>(static_cast<s32>(last)) * through;
    }

    u32 frame = static_cast<u32>(static_cast<s32>(position));
    status->frameShare = position - static_cast<f32>(static_cast<s32>(frame));
    u32 next;
    if (frame == last)
    {
        next = (status->bits & AnimationStatus::Loops) != 0 ? 0 : frame;
    }
    else
    {
        next = frame + 1;
    }

    if ((status->bits & AnimationStatus::Reversed) != 0)
    {
        u32 reversed = last - next;
        status->frameShare = 1.0f - status->frameShare;
        next = last - frame;
        frame = reversed;
    }

    if (status->main != nullptr)
    {
        FindFrames(status->main, frame, next);
    }

    if (status->blendShapes != nullptr)
    {
        FindFrames(status->blendShapes, frame, next);
    }
}

void ProgressAnimation(AnimationStatus* status, s32 now, u32 main)
{
    u32 bits = status->bits;
    if ((bits & AnimationStatus::Ended) == 0)
    {
        if ((bits & AnimationStatus::Started) != 0)
        {
            s32 began = status->began;
            s32 length = status->length;
            s32 time = now - began;
            status->time = time;
            if (!(time < length))
            {
                if ((bits & AnimationStatus::Loops) != 0)
                {
                    s32 loops = static_cast<s32>(static_cast<f32>(time) * g_SecondsPerClockUnit /
                                                 (static_cast<f32>(length) * g_SecondsPerClockUnit));
                    s32 looped = length * loops;
                    status->began = began + looped;
                    status->time = time - looped;
                }
                else
                {
                    status->time = length;
                    status->bits = bits | AnimationStatus::Ended;
                }
            }
        }
        else
        {
            if ((bits & AnimationStatus::StartsPartWay) != 0)
            {
                f32 start = status->start;
                f32 seconds = ClockUnitsToSeconds(&status->length);
                status->began = now - static_cast<s32>(seconds * start * g_ClockUnitsPerSecond);
            }
            else
            {
                status->began = now;
            }

            bits = status->bits | AnimationStatus::Started;
            status->bits = bits;
            if ((bits & AnimationStatus::BlendsIn) != 0)
            {
                status->blendStart = now;
            }
        }
    }

    SetAnimationData(status, main);
    bits = status->bits;
    if ((bits & AnimationStatus::BlendsIn) == 0)
    {
        return;
    }

    s32 elapsed = now - status->blendStart;
    if (elapsed < status->blendTime)
    {
        status->weight = static_cast<f32>(elapsed) * g_SecondsPerClockUnit /
                         (static_cast<f32>(status->blendTime) * g_SecondsPerClockUnit);
    }
    else
    {
        status->bits = bits & ~AnimationStatus::BlendsIn;
        status->weight = 1.0f;
    }
}

void DestroyAnimationStatus(AnimationStatus* status, u32 destroyFlags)
{
    if (status->main != nullptr)
    {
        MemoryDeallocate2_(status->main);
    }

    if (status->blendShapes != nullptr)
    {
        MemoryDeallocate2_(status->blendShapes);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(status);
    }
}

void ClearAnimationStatuses(AnimationChain* chain, u32 main)
{
    AnimationStatus* status = main != 0 ? chain->main : chain->blendShapes;
    while (status != nullptr)
    {
        AnimationStatus* next = status->next;
        DestroyAnimationStatus(status, DestroyAndFree);
        status = next;
    }

    if (main != 0)
    {
        chain->main = nullptr;
    }
    else
    {
        chain->blendShapes = nullptr;
    }
}

void ContinueAnimationStatus(AnimationStatus* status, const AnimationStatus* from)
{
    if ((from->bits & AnimationStatus::Started) == 0)
    {
        return;
    }

    f32 share = static_cast<f32>(from->time) * g_SecondsPerClockUnit / (static_cast<f32>(from->length) * g_SecondsPerClockUnit);
    status->start = share;
    if ((status->bits & AnimationStatus::Reversed) != (from->bits & AnimationStatus::Reversed))
    {
        status->start = 1.0f - share;
    }

    status->bits = (status->bits & ~AnimationStatus::Started) | AnimationStatus::StartsPartWay;
}

u32 SameAnimations(const AnimationStatus* status, const AnimationStatus* other)
{
    if (status->main == nullptr ? other->main != nullptr
                                : other->main == nullptr || status->main->information != other->main->information)
    {
        return 0;
    }

    if (status->blendShapes == nullptr)
    {
        return other->blendShapes == nullptr ? 1 : 0;
    }

    return other->blendShapes != nullptr && status->blendShapes->information == other->blendShapes->information ? 1 : 0;
}

JointMatrices* ConstructJointMatrices(JointMatrices* matrices)
{
    matrices->frame = 0;
    matrices->start = nullptr;
    matrices->end = nullptr;
    *reinterpret_cast<u32*>(&matrices->detail) = 0;
    return matrices;
}

void DestroyJointMatrices(JointMatrices* matrices, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(matrices);
    }
}

void StartJointMatrices(JointMatrices* matrices, const GameOGI* ogi)
{
    u32 count = ogi->jointCount;
    *reinterpret_cast<u32*>(&matrices->detail) = 0;
    matrices->frame = 0;
    auto* room = static_cast<Matrix4x4*>(Platform::Graphics::AllocFrameMemory(count, sizeof(Matrix4x4)));
    matrices->end = room;
    matrices->frame = g_RenderedFrames;
    matrices->start = room;
}

BlendShapeWeights* ConstructBlendShapeWeights(BlendShapeWeights* weights)
{
    weights->count = 0;
    weights->index = 0;
    weights->room = nullptr;
    weights->current = nullptr;
    weights->next = nullptr;
    return weights;
}

void DestroyBlendShapeWeights(BlendShapeWeights* weights, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(weights);
    }
}

void ResetBlendShapeWeights(BlendShapeWeights* weights)
{
    weights->count = 0;
    weights->room = nullptr;
}

u32 ForgetJointMatrices(OgiAnimator* animator)
{
    animator->bits &= ~1u;
    return 0;
}

void ProgressAnimationChain(AnimationChain* chain, u32 main, s32 now)
{
    AnimationStatus* first = main != 0 ? chain->main : chain->blendShapes;
    for (AnimationStatus* status = first; status != nullptr;)
    {
        AnimationStatus* next = status->next;
        ProgressAnimation(status, now, main);
        status = next;
    }

    AnimationStatus* previous = nullptr;
    for (AnimationStatus* status = first; status != nullptr;)
    {
        AnimationStatus* next = status->next;
        bool gone;
        if (next == nullptr)
        {
            const AnimationData* data = main != 0 ? status->main : status->blendShapes;
            gone = (status->bits & AnimationStatus::BlendsIn) == 0 && data == nullptr;
        }
        else
        {
            gone = (next->bits & AnimationStatus::BlendsIn) == 0;
        }

        if (gone)
        {
            DestroyAnimationStatus(status, DestroyAndFree);
            if (previous != nullptr)
            {
                previous->next = next;
            }
            else if (main != 0)
            {
                chain->main = next;
            }
            else
            {
                chain->blendShapes = next;
            }
        }
        else
        {
            previous = status;
        }

        status = next;
    }
}

void ProgressJointChains(JointAnimation* joint, s32 now, u32 detail, AnimationChain* parentChain)
{
    u32 own = joint->joint->detail >> 4;
    AnimationChain* chain = joint->chain;
    if (chain != nullptr)
    {
        chain->next = parentChain;
        ProgressAnimationChain(chain, 1, now);
        ProgressAnimationChain(chain, 0, now);
        if (chain->main != nullptr || chain->blendShapes != nullptr)
        {
            parentChain = joint->chain;
        }
        else
        {
            AnimationChain* empty = joint->chain;
            if (empty != nullptr)
            {
                ClearAnimationStatuses(empty, 1);
                ClearAnimationStatuses(empty, 0);
                MemoryDeallocate2_(empty);
            }

            joint->chain = nullptr;
        }
    }

    if (own < detail)
    {
        return;
    }

    for (u32 child = 0; child < joint->ChildCount(); child++)
    {
        ProgressJointChains(joint->children[child], now, detail, parentChain);
    }
}

void ProgressAnimator(OgiAnimator* animator, s32 now, u32 detail)
{
    ProgressJointChains(animator->root, now, detail, nullptr);
}

f32 GetAnimationProgress(const OgiAnimator* animator, u32 joint)
{
    constexpr u8 Root = 0xFF;
    if ((joint & 0xFF) == Root)
    {
        AnimationChain* chain = animator->root->chain;
        return chain != nullptr ? CalculateProgress(chain) : 0.0f;
    }

    for (JointAnimation* animation = animator->cameraJoints->data[joint & 0xFF]; animation != nullptr;
         animation = animation->parent)
    {
        f32 left = animation->chain != nullptr ? CalculateProgress(animation->chain) : 0.0f;
        if (0.0f < left)
        {
            return left;
        }
    }

    return 0.0f;
}

void CopyJointMatrices(const JointAnimation* joint, JointMatrices* matrices)
{
    Matrix4x4* written = matrices->end;
    *written = joint->transform;
    matrices->end = written + 1;
    if ((joint->joint->detail >> 4) < matrices->detail)
    {
        return;
    }

    for (u32 child = 0; child < joint->ChildCount(); child++)
    {
        CopyJointMatrices(joint->children[child], matrices);
    }
}

void CopyAnimatorMatrices(const OgiAnimator* animator, JointMatrices* matrices)
{
    CopyJointMatrices(animator->root, matrices);
}

JointAnimation* FindChildJoint(JointAnimation* joint, u32 id, u32 deep)
{
    u32 wanted = id & 0xFF;
    JointAnimation* found = nullptr;
    for (u32 child = 0; child < joint->ChildCount(); child++)
    {
        JointAnimation* animation = joint->children[child];
        if (animation->joint->id == wanted)
        {
            found = animation;
        }
        else if (deep != 0)
        {
            found = FindChildJoint(animation, wanted, deep);
        }

        if (found != nullptr)
        {
            break;
        }
    }

    return found;
}

AnimationSettings* ConstructAnimationSettings(f32 speed, AnimationSettings* settings, GameAnimation* animation)
{
    settings->start = 0.0f;
    settings->animation = animation;
    settings->blendTime = 0;
    settings->bits = 0;
    f32 seconds;
    if (animation != nullptr)
    {
        u32 rate = animation->bits >> GameAnimation::RateShift & GameAnimation::RateMask;
        u32 frames = animation->bits >> GameAnimation::FramesShift & GameAnimation::FramesMask;
        seconds = static_cast<f32>(static_cast<s32>(frames)) / static_cast<f32>(static_cast<s32>(rate)) / speed;
    }
    else
    {
        seconds = settings->start;
    }

    settings->length = static_cast<s32>(seconds * g_ClockUnitsPerSecond);
    return settings;
}

void DestroyAnimationSettings(AnimationSettings* settings, u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(settings);
    }
}

AnimationStatus* ConstructAnimationStatus(AnimationStatus* status, GameAnimation* animation, s32 length, s32 blendTime)
{
    u32 blends = blendTime != 0 ? AnimationStatus::BlendsIn : 0;
    status->length = length;
    status->weight = 1.0f;
    status->blendTime = blendTime;
    status->next = nullptr;
    status->bits = blends;
    status->time = 0;
    status->began = 0;
    status->blendStart = 0;
    if (animation == nullptr)
    {
        status->main = nullptr;
        status->blendShapes = nullptr;
        status->bits = blends | (blends == 0 ? AnimationStatus::Ended : 0);
        return status;
    }

    status->bits = blends | (animation->bits >> GameAnimation::FramesShift & GameAnimation::FramesMask) << AnimationStatus::FramesShift;
    AnimationData* main = nullptr;
    if ((animation->bits & GameAnimation::HasMain) != 0)
    {
        main = static_cast<AnimationData*>(MemoryAllocate(sizeof(AnimationData)));
        main->information = &animation->main;
    }

    status->main = main;
    AnimationData* shapes = nullptr;
    if ((animation->bits & GameAnimation::HasBlendShapes) != 0)
    {
        shapes = static_cast<AnimationData*>(MemoryAllocate(sizeof(AnimationData)));
        shapes->information = &animation->blendShapes;
    }

    status->blendShapes = shapes;
    return status;
}

void PlayAnimationOnChain(AnimationChain* chain, u32 main, const AnimationSettings* settings, u32 queues)
{
    AnimationStatus* playing = main != 0 ? chain->main : chain->blendShapes;
    auto* status = ConstructAnimationStatus(static_cast<AnimationStatus*>(MemoryAllocate(sizeof(AnimationStatus))),
                                            settings->animation, settings->length, settings->blendTime);
    status->bits = (status->bits & ~AnimationStatus::Loops) | (settings->bits & AnimationSettings::Loops) << 2;
    status->bits = (status->bits & ~AnimationStatus::Reversed) | (settings->bits << 1 & AnimationStatus::Reversed);
    if ((settings->bits & AnimationSettings::StartsPartWay) != 0)
    {
        status->start = (settings->bits & AnimationSettings::StartsPartWay) != 0 ? settings->start : 0.0f;
        status->bits |= AnimationStatus::StartsPartWay;
    }

    if (queues == 0 || (settings->bits & AnimationSettings::Queued) == 0)
    {
        ClearAnimationStatuses(chain, main);
        if (main != 0)
        {
            chain->main = status;
        }
        else
        {
            chain->blendShapes = status;
        }

        return;
    }

    if (playing == nullptr)
    {
        if (main != 0)
        {
            chain->main = status;
        }
        else
        {
            chain->blendShapes = status;
        }

        return;
    }

    AnimationStatus* last = playing;
    while (last->next != nullptr)
    {
        last = last->next;
    }

    // The same animation played the same way again isn't queued
    u32 bits = settings->bits;
    if ((bits & AnimationSettings::QueuedAlways) == 0 &&
        ((last->bits & AnimationStatus::Loops) != 0) == ((settings->bits & AnimationSettings::Loops) != 0) &&
        ((last->bits & AnimationStatus::Reversed) != 0) == ((bits & AnimationSettings::Backward) != 0) && SameAnimations(last, status) != 0)
    {
        DestroyAnimationStatus(status, DestroyAndFree);
        return;
    }

    last->next = status;
    if ((settings->bits & AnimationSettings::Continues) != 0)
    {
        ContinueAnimationStatus(status, last);
    }
}

u32 GetJointIndexByID(const GameOGI* ogi, u32 id)
{
    u32 wanted = id & 0xFF;
    for (u32 index = 0; index < ogi->jointCount; index++)
    {
        if (ogi->joints[index].id == wanted)
        {
            return index & 0xFF;
        }
    }

    return 0xFF;
}

void DestroyJointTree(JointAnimation* joint, u32 destroyFlags)
{
    for (u32 child = 0; child < joint->ChildCount(); child++)
    {
        if (joint->children[child] != nullptr)
        {
            DestroyJointTree(joint->children[child], DestroyAndFree);
        }
    }

    AnimationChain* chain = joint->chain;
    if (chain != nullptr)
    {
        ClearAnimationStatuses(chain, 1);
        ClearAnimationStatuses(chain, 0);
        MemoryDeallocate2_(chain);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(joint);
    }
}

void ClearCameraJoints(OgiAnimator* animator)
{
    if (animator->cameraJoints == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < static_cast<u32>(animator->cameraJoints->size); index++)
    {
        animator->cameraJoints->data[index] = nullptr;
    }
}

void ClearJointCallbacks(OgiAnimator* animator)
{
    if (animator->callbacks == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < static_cast<u32>(animator->callbacks->size); index++)
    {
        animator->callbacks->data[index] = nullptr;
    }
}

void BindExitPoints(OgiAnimator* animator)
{
    constexpr u8 NoJoint = 0xFF;
    SizedArray<ExitPointAnimation*>* exitPoints = animator->exitPoints;
    if (exitPoints == nullptr)
    {
        return;
    }

    u32 count = exitPoints->size;
    u32 own = animator->ogi->exitPointCount;
    for (u32 index = 0; index < count; index++)
    {
        if (index < own)
        {
            OgiExitPoint* exitPoint = &animator->ogi->exitPoints[index];
            if (exitPoint->joint != NoJoint)
            {
                ExitPointAnimation* animation = animator->exitPoints->data[index];
                animation->exitPoint = exitPoint;
                animation->joint = animator->joints->data[exitPoint->joint];
            }
        }
        else
        {
            ExitPointAnimation* animation = animator->exitPoints->data[index];
            animation->joint = nullptr;
            animation->exitPoint = nullptr;
        }
    }
}

namespace
{
template <typename T>
SizedArray<T>* NewSizedArray(u32 size)
{
    auto* array = static_cast<SizedArray<T>*>(MemoryAllocate(sizeof(SizedArray<T>)));
    array->size = size;
    array->data = static_cast<T*>(MemoryAllocate2(size * sizeof(T)));
    return array;
}

// The fewer of the animator's camera joints and the OGI's
s32 CameraJointCount(const OgiAnimator* animator)
{
    s32 own = animator->ogi->cameraJointCount;
    s32 room = animator->cameraJoints->size;
    return own < room ? own : room;
}
}

void SetAnimatorOgi(OgiAnimator* animator, GameOGI* ogi, u32 cameraJoints, u32 exitPoints)
{
    constexpr u8 NoJoint = 0xFF;
    if (animator->ogi == ogi)
    {
        return;
    }

    if (animator->root != nullptr)
    {
        DestroyJointTree(animator->root, DestroyAndFree);
    }

    SizedArray<JointAnimation*>* joints = animator->joints;
    if (joints != nullptr)
    {
        if (joints->data != nullptr)
        {
            MemoryDeallocate_(joints->data);
        }

        MemoryDeallocate2_(joints);
    }

    animator->root = nullptr;
    animator->joints = nullptr;
    animator->ogi = ogi;
    if (ogi == nullptr)
    {
        return;
    }

    u32 count = ogi->jointCount;
    auto* table = static_cast<SizedArray<JointAnimation*>*>(MemoryAllocate(sizeof(SizedArray<JointAnimation*>)));
    table->size = count;
    table->data = count != 0 ? static_cast<JointAnimation**>(MemoryAllocate2(count * sizeof(JointAnimation*))) : nullptr;
    animator->joints = table;
    if (animator->exitPoints != nullptr)
    {
        for (u32 index = 0; index < static_cast<u32>(animator->exitPoints->size); index++)
        {
            ExitPointAnimation* animation = animator->exitPoints->data[index];
            animation->joint = nullptr;
            animation->exitPoint = nullptr;
        }
    }

    ClearCameraJoints(animator);
    if (animator->joints != nullptr)
    {
        for (u32 index = 0; index < static_cast<u32>(animator->joints->size); index++)
        {
            animator->joints->data[index] = nullptr;
        }
    }

    SetJointAnimations(animator, count);
    if (cameraJoints != 0)
    {
        if (animator->cameraJoints == nullptr)
        {
            animator->cameraJoints = NewSizedArray<JointAnimation*>(cameraJoints);
            ClearCameraJoints(animator);
        }

        if (animator->callbacks == nullptr)
        {
            animator->callbacks = NewSizedArray<JointCallback*>(cameraJoints);
            ClearJointCallbacks(animator);
        }
    }

    if (exitPoints != 0 && animator->exitPoints == nullptr)
    {
        animator->exitPoints = NewSizedArray<ExitPointAnimation*>(exitPoints);
        for (u32 index = 0; index < exitPoints; index++)
        {
            auto* animation = static_cast<ExitPointAnimation*>(MemoryAllocate(sizeof(ExitPointAnimation)));
            animation->bits = 0;
            ClearExitPointLinks(animation);
            InitIdentityMatrix(&animation->matrix);
            animation->bits |= ExitPointAnimation::Normalized;
            animator->exitPoints->data[index] = animation;
        }
    }

    BindExitPoints(animator);
    if (animator->cameraJoints != nullptr)
    {
        s32 cameraCount = CameraJointCount(animator);
        for (s32 id = 0; id < cameraCount; id++)
        {
            u32 index = GetJointIndexByID(animator->ogi, id & 0xFF);
            animator->cameraJoints->data[id] = index == NoJoint ? nullptr : animator->joints->data[index];
        }
    }

    animator->bits &= ~(OgiAnimator::CameraJointsMask << OgiAnimator::CameraJointsShift);
    if (animator->cameraJoints == nullptr)
    {
        return;
    }

    s32 cameraCount = CameraJointCount(animator);
    for (s32 id = 0; id < cameraCount; id++)
    {
        JointAnimation* joint = animator->cameraJoints->data[id];
        JointCallback* callbacks = animator->callbacks->data[id];
        if (joint != nullptr)
        {
            joint->callbacks = callbacks;
        }

        u32 bits = animator->bits;
        u32 counted = ((bits >> OgiAnimator::CameraJointsShift) + 1) & OgiAnimator::CameraJointsMask;
        animator->bits = (bits & ~(OgiAnimator::CameraJointsMask << OgiAnimator::CameraJointsShift)) | counted << OgiAnimator::CameraJointsShift;
    }
}

OgiAnimator* ConstructOgiAnimatorBase(OgiAnimator* animator, GameOGI* ogi, u32 cameraJoints, u32 exitPoints)
{
    animator->ogi = nullptr;
    animator->root = nullptr;
    animator->place = nullptr;
    animator->exitPoints = nullptr;
    animator->cameraJoints = nullptr;
    animator->joints = nullptr;
    animator->callbacks = nullptr;
    animator->bits = 0;
    animator->bits |= OgiAnimator::Animates;
    SetAnimatorOgi(animator, ogi, cameraJoints, exitPoints);
    return animator;
}

OgiAnimator* InitOgiAnimatorService(OgiAnimator* animator, GameOGI* ogi, u32 cameraJoints, u32 exitPoints)
{
    ConstructOgiAnimatorBase(animator, ogi, cameraJoints, exitPoints);
    ConstructJointMatrices(&animator->matrices);
    ConstructBlendShapeWeights(&animator->blendShapes);
    return animator;
}

void ReleaseAnimatorOgi(OgiAnimator* animator)
{
    SetAnimatorOgi(animator, nullptr, 0, 0);
}

void UpdateExitPoints(OgiAnimator* animator, u32 moved)
{
    SizedArray<ExitPointAnimation*>* exitPoints = animator->exitPoints;
    if (exitPoints == nullptr || animator->place == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < static_cast<u32>(exitPoints->size); index++)
    {
        ExitPointAnimation* animation = exitPoints->data[index];
        if (animation == nullptr)
        {
            continue;
        }

        animation->place = animator->place;
        animation->bits = ((animation->bits | ExitPointAnimation::InUse) & ~ExitPointAnimation::Moved) | (moved & 1) << 1;
    }
}

u32 AnimateOgi(OgiAnimator* animator, const GameOGI* ogi, const TimeClock* clock, u32 animate, u32 detail)
{
    ProgressAnimator(animator, clock->time, detail);
    if (animate != 0)
    {
        StartJointMatrices(&animator->matrices, ogi);
        ResetBlendShapeWeights(&animator->blendShapes);
        animator->matrices.detail = detail;
        ProcessAnimationData(animator, &animator->matrices, &animator->blendShapes);
    }

    UpdateExitPoints(animator, animate);
    animator->bits = (animator->bits & ~OgiAnimator::Animates) | (animate & 1);
    return animate != 0 ? 1 : 0;
}

u32 ReuseJointMatrices(OgiAnimator* animator, const GameOGI* ogi, u32 animate, u32 detail)
{
    if (animate != 0)
    {
        StartJointMatrices(&animator->matrices, ogi);
        ResetBlendShapeWeights(&animator->blendShapes);
        animator->matrices.detail = detail;
        CopyAnimatorMatrices(animator, &animator->matrices);
    }

    UpdateExitPoints(animator, 0);
    animator->bits = (animator->bits & ~OgiAnimator::Animates) | (animate & 1);
    return animate != 0 ? 1 : 0;
}

ExitPointAnimation* UpdateExitPointMatrix(ExitPointAnimation* animation)
{
    constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
    if ((animation->bits & (ExitPointAnimation::InUse | ExitPointAnimation::Moved)) == 0)
    {
        return animation;
    }

    auto* place = static_cast<ObjectPlace*>(animation->place);
    RotateAndTranslate(place);
    OgiExitPoint* exitPoint = animation->exitPoint;
    if (exitPoint == nullptr)
    {
        if ((animation->bits & ExitPointAnimation::InUse) != 0)
        {
            animation->matrix = place->matrix;
        }
    }
    else if (animation->joint == nullptr)
    {
        if ((animation->bits & ExitPointAnimation::InUse) != 0)
        {
            VuMultiplyMatrices(&exitPoint->matrix, &place->matrix, &animation->matrix);
        }
    }
    else
    {
        Matrix4x4 onJoint;
        VuMultiplyMatrices(&exitPoint->matrix, &animation->joint->transform, &onJoint);
        VuMultiplyMatrices(&onJoint, &place->matrix, &animation->matrix);
    }

    if ((animation->bits & ExitPointAnimation::Normalized) != 0)
    {
        for (u32 axis = 0; axis < 3; axis++)
        {
            auto* row = reinterpret_cast<Vector4*>(animation->matrix.m[axis]);
            f32 inverse = InverseLength(row, LengthEpsilon);
            row->x = row->x * inverse;
            row->y = row->y * inverse;
            row->z = row->z * inverse;
        }
    }

    animation->bits = animation->bits & ~ExitPointAnimation::InUse & ~ExitPointAnimation::Moved;
    return animation;
}

void ClearExitPointLinks(ExitPointAnimation* animation)
{
    animation->joint = nullptr;
    animation->exitPoint = nullptr;
}

namespace
{
constexpr u8 RootJoint = 0xFF;

// The joint an animator's animation calls name: its root, or a camera joint
JointAnimation* NamedJoint(const OgiAnimator* animator, u32 joint)
{
    return (joint & 0xFF) == RootJoint ? animator->root : animator->cameraJoints->data[joint & 0xFF];
}

// Settings played on a joint's chain (made the first time when they play an animation), its main and blend shapes' both
void PlayOnJoint(const OgiAnimator* animator, JointAnimation* joint, const AnimationSettings* settings)
{
    AnimationChain* chain = joint->chain;
    if (chain == nullptr)
    {
        if (settings->animation != nullptr)
        {
            chain = static_cast<AnimationChain*>(MemoryAllocate(sizeof(AnimationChain)));
            chain->main = nullptr;
            chain->blendShapes = nullptr;
            joint->chain = chain;
        }

        if (chain == nullptr)
        {
            return;
        }
    }

    u32 queues = animator->bits & 1;
    PlayAnimationOnChain(chain, 1, settings, queues);
    PlayAnimationOnChain(chain, 0, settings, queues);
}
}

void PlayOgiAnimation(OgiAnimator* animator, const AnimationSettings* settings, u32 joint)
{
    PlayOnJoint(animator, NamedJoint(animator, joint), settings);
}

void StopOgiAnimation(OgiAnimator* animator, s32 blendTime, u32 joint)
{
    AnimationSettings settings;
    settings.blendTime = blendTime;
    settings.start = 0.0f;
    settings.animation = nullptr;
    settings.length = 0;
    settings.bits = 0;
    PlayOnJoint(animator, NamedJoint(animator, joint), &settings);
}

u32 AddJointCallback(OgiAnimator* animator, u32 joint, void* object)
{
    u32 index = joint & 0xFF;
    JointCallback* previous = nullptr;
    for (JointCallback* callback = animator->callbacks->data[index]; callback != nullptr; callback = callback->next)
    {
        if (callback->object == object)
        {
            return 0;
        }

        previous = callback;
    }

    auto* added = static_cast<JointCallback*>(MemoryAllocate(sizeof(JointCallback)));
    added->object = object;
    added->next = nullptr;
    if (previous != nullptr)
    {
        previous->next = added;
        return 1;
    }

    JointAnimation* cameraJoint = animator->cameraJoints->data[index];
    animator->callbacks->data[index] = added;
    if (cameraJoint != nullptr)
    {
        cameraJoint->callbacks = added;
    }

    return 1;
}

void DestroyJointCallbacks(JointCallback* callback, u32 destroyFlags)
{
    if (callback->next != nullptr)
    {
        DestroyJointCallbacks(callback->next, DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(callback);
    }
}

void AddJointRotation(JointAnimator* animator, const Vector4* rotation)
{
    if ((animator->flags & JointAnimator::HasRotation) != 0)
    {
        Vector4 turned;
        MultiplyRotations(&turned, rotation, &animator->rotation);
        animator->rotation.x = turned.x;
        animator->rotation.y = turned.y;
        animator->rotation.z = turned.z;
        animator->rotation.w = turned.w;
        return;
    }

    animator->flags |= JointAnimator::HasRotation;
    animator->rotation.x = rotation->x;
    animator->rotation.y = rotation->y;
    animator->rotation.z = rotation->z;
    animator->rotation.w = rotation->w;
}

u32 CopyJointTransform(const OgiAnimator* animator, u32 index, Matrix4x4* out)
{
    JointAnimation* joint = animator->joints != nullptr ? animator->joints->data[index] : nullptr;
    if (joint == nullptr)
    {
        return 0;
    }

    *out = joint->transform;
    return 1;
}

namespace
{
// The animator of an instance's model node, when it has one
void* ModelAnimator(InstanceContext* instance)
{
    constexpr u32 ModelNodeKind = 3;
    auto* node = static_cast<u8*>(GetGameNode(&instance->nodes, ModelNodeKind));
    return node != nullptr ? *reinterpret_cast<void**>(node + 0x24) : nullptr;
}

void TellObject(void* object, u32 slot, void* animator)
{
    CallVirtual<void>(object, *static_cast<const GccVTableEntry* const*>(object), slot, animator);
}
}

void AttachToModelAnimator(void* object, InstanceContext* instance)
{
    constexpr u32 AttachSlot = 2;
    void* animator = ModelAnimator(instance);
    if (animator != nullptr)
    {
        TellObject(object, AttachSlot, animator);
    }
}

void DetachFromModelAnimator(void* object, InstanceContext* instance)
{
    constexpr u32 DetachSlot = 3;
    void* animator = ModelAnimator(instance);
    if (animator != nullptr)
    {
        TellObject(object, DetachSlot, animator);
    }
}

GameOGI* InitOGI(GameOGI* ogi)
{
    constexpr u32 ReadBits = 0x30000;
    *reinterpret_cast<u16*>(&ogi->bits) = 0;
    ogi->id = 0xFFFFFFFF;
    ogi->bits &= ~ReadBits;
    ogi->name.string = nullptr;
    ogi->name.capacity = 0;
    ogi->name.length = 0;
    ogi->rigidModelJoints = nullptr;
    ogi->jointMatrices = nullptr;
    ogi->rigidModelIds = nullptr;
    ogi->joints = nullptr;
    ogi->exitPoints = nullptr;
    ogi->skin = nullptr;
    ogi->blendSkin = nullptr;
    ogi->rigidModels = nullptr;
    ogi->hulls = nullptr;
    ogi->hullJoints = nullptr;
    auto* counts = reinterpret_cast<u32*>(&ogi->jointCount);
    for (u32 word = 0; word < 4; word++)
    {
        counts[word] = 0;
    }

    return ogi;
}

GameAnimation* InitAnimation(GameAnimation* animation)
{
    constexpr u32 ReadBits = 0x30000;
    *reinterpret_cast<u16*>(&animation->unknown00) = 0;
    animation->unknown00 &= ~ReadBits;
    animation->id = 0xFFFFFFFF;
    animation->blendShapes.diskHandle = -1;
    animation->main.diskHandle = -1;
    animation->main.sections = 0;
    animation->main.frames = 0;
    animation->blendShapes.sections = 0;
    animation->blendShapes.frames = 0;
    animation->bits = 0;
    return animation;
}

u32 AnimationDataSize(const AnimationDataInformation* information)
{
    if (information->frames == 0)
    {
        return 0;
    }

    u32 sections = information->sections;
    u32 perFrame = sections >> AnimationDataInformation::FrameValuesShift;
    return (sections & AnimationDataInformation::JointsMask) * sizeof(JointTrackSettings) +
           (sections >> AnimationDataInformation::StaticBytesShift & AnimationDataInformation::StaticBytesMask) +
           perFrame * information->frames * 2;
}

void ReadAnimationData(AnimationDataInformation* information, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&information->sections));
    stream->ReadS16(reinterpret_cast<s16*>(&information->frames));
    if (information->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &information->diskHandle);
    }

    u32 size = AnimationDataSize(information);
    if (size == 0)
    {
        return;
    }

    s32 handle;
    DiskAllocate(&handle, GetDiskManager(), size, false, 0);
    information->diskHandle = handle;
    stream->Read(DiskLoadedMemory(GetDiskManager(), &information->diskHandle), size, 1);
}

void ReadAnimation(GameAnimation* animation, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&animation->bits));
    ReadAnimationData(&animation->main, stream);
    ReadAnimationData(&animation->blendShapes, stream);
}

void ReadJoint(JointStruct* joint, Stream* stream)
{
    s32 words[5];
    for (s32& word : words)
    {
        stream->ReadS32(&word);
    }

    joint->id = words[0];
    joint->index = words[1];
    joint->parent = words[2];
    joint->detail = (words[3] & 0xF) | (words[4] & 0xF) << 4;
    stream->Read(&joint->bindPosition, sizeof(Vector4), 1);
    stream->Read(&joint->worldPosition, sizeof(Vector4), 1);
    stream->Read(&joint->bindRotation, sizeof(Vector4), 1);
    stream->Read(&joint->unusedRotation, sizeof(Vector4), 1);
    stream->Read(&joint->additionalRotation, sizeof(Vector4), 1);
}

void ReadOgi(GameOGI* ogi, Stream* stream)
{
    // What the older layout has that's skipped, and its hulls' surfaces ("Surf", then the surface)
    constexpr u32 SurfaceMark = 0x66727553;
    alignas(16) u8 older[0x100];
    if (g_Rm2Queued != 0)
    {
        stream->Read(&ogi->jointCount, 0x10, 1);
    }
    else
    {
        s32 counts[9];
        for (s32& count : counts)
        {
            stream->ReadS32(&count);
        }

        ogi->jointCount = counts[0];
        ogi->exitPointCount = counts[1];
        ogi->cameraJointCount = counts[2];
        ogi->unknown43[0] = counts[3];
        ogi->unknown43[1] = counts[4];
        ogi->rigidModelCount = counts[5];
        ogi->hasSkin = counts[6];
        ogi->hasBlendSkin = counts[7];
        ogi->hullCount = counts[8];
    }

    Box bounds;
    stream->Read(&bounds.min, sizeof(Vector4), 1);
    stream->Read(&bounds.max, sizeof(Vector4), 1);
    ogi->bounds = bounds;
    if (g_Rm2Queued == 0)
    {
        stream->Read(ogi->olderBytes, sizeof(ogi->olderBytes), 1);
    }

    if (ogi->jointCount == 0)
    {
        ogi->joints = nullptr;
    }
    else
    {
        ogi->joints = NewArray<JointStruct>(ogi->jointCount);
        for (u32 index = 0; index < ogi->jointCount; index++)
        {
            ReadJoint(&ogi->joints[index], stream);
        }
    }

    if (ogi->exitPointCount == 0)
    {
        ogi->exitPoints = nullptr;
    }
    else
    {
        ogi->exitPoints = NewArray<OgiExitPoint>(ogi->exitPointCount);
        for (u32 index = 0; index < ogi->exitPointCount; index++)
        {
            OgiExitPoint* point = &ogi->exitPoints[index];
            s32 joint;
            s32 id;
            stream->ReadS32(&joint);
            stream->ReadS32(&id);
            stream->Read(&point->matrix, sizeof(Matrix4x4), 1);
            point->joint = joint;
            point->id = id;
        }
    }

    if (ogi->rigidModelCount == 0)
    {
        ogi->rigidModelJoints = nullptr;
        ogi->rigidModelIds = nullptr;
        ogi->rigidModels = nullptr;
    }
    else
    {
        ogi->rigidModelJoints = static_cast<u8*>(MemoryAllocate2(ogi->rigidModelCount));
        ogi->rigidModelIds = static_cast<u32*>(MemoryAllocate2(ogi->rigidModelCount * sizeof(u32)));
        ogi->rigidModels = static_cast<RigidModel**>(MemoryAllocate2(ogi->rigidModelCount * sizeof(RigidModel*)));
        stream->Read(ogi->rigidModelJoints, ogi->rigidModelCount, 1);
        for (u32 index = 0; index < ogi->rigidModelCount; index++)
        {
            if (g_Rm2Queued == 0)
            {
                s32 size;
                stream->ReadS32(&size);
                stream->Read(older, size, 1);
            }

            stream->ReadS32(reinterpret_cast<s32*>(&ogi->rigidModelIds[index]));
            ogi->rigidModels[index] = g_RigidModelTable.Acquire(&ogi->rigidModelIds[index], nullptr);
        }
    }

    ogi->jointMatrices = static_cast<Matrix4x4*>(MemoryAllocate2(ogi->jointCount * sizeof(Matrix4x4)));
    for (u32 index = 0; index < ogi->jointCount; index++)
    {
        stream->Read(&ogi->jointMatrices[index], sizeof(Matrix4x4), 1);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&ogi->skinId));
    ogi->skin = ogi->hasSkin != 0 ? g_SkinTable.Acquire(&ogi->skinId, nullptr) : nullptr;
    stream->ReadS32(reinterpret_cast<s32*>(&ogi->blendSkinId));
    ogi->blendSkin = ogi->hasBlendSkin != 0 ? g_BlendSkinTable.Acquire(&ogi->blendSkinId, nullptr) : nullptr;
    if (ogi->hullCount == 0)
    {
        ogi->hullCount = 1;
        ogi->hulls = NewArray<CollisionHull>(1);
        HullConstruct(ogi->hulls);
        ogi->hullJoints = static_cast<u8*>(MemoryAllocate2(ogi->hullCount));
        ogi->hullJoints[0] = 0xFF;
        BuildBoxHull(ogi->hulls, &bounds.min, &bounds.max);
        return;
    }

    u32 hullCount = ogi->hullCount;
    CollisionHull* hulls = NewArray<CollisionHull>(hullCount);
    for (u32 index = 0; index < hullCount; index++)
    {
        HullConstruct(&hulls[index]);
    }

    ogi->hulls = hulls;
    ogi->hullJoints = static_cast<u8*>(MemoryAllocate2(ogi->hullCount));
    for (u32 index = 0; index < ogi->hullCount; index++)
    {
        ReadModelCollisionData(&ogi->hulls[index], stream);
    }

    stream->Read(ogi->hullJoints, ogi->hullCount, 1);
    if (g_Rm2Queued != 0)
    {
        return;
    }

    for (u32 index = 0; index < ogi->hullCount; index++)
    {
        stream->Read(older, 0x10, 1);
        if (*reinterpret_cast<u32*>(older) == SurfaceMark)
        {
            ogi->hulls[index].surface = *reinterpret_cast<u16*>(older + 4);
        }
    }
}

void DestroyAnimation(GameAnimation* animation, u32 destroyFlags)
{
    if (animation->blendShapes.diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &animation->blendShapes.diskHandle);
    }

    if (animation->main.diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &animation->main.diskHandle);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(animation);
    }
}

void DestroyOgi(GameOGI* ogi, u32 destroyFlags)
{
    if (ogi->joints != nullptr)
    {
        DeleteArray(ogi->joints);
    }

    if (ogi->exitPoints != nullptr)
    {
        DeleteArray(ogi->exitPoints);
    }

    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        ReleaseRigidModel(ogi->rigidModels[index]);
    }

    if (ogi->skin != nullptr)
    {
        ReleaseSkin(ogi->skin);
    }

    if (ogi->blendSkin != nullptr)
    {
        ReleaseBlendSkin(ogi->blendSkin);
    }

    if (ogi->rigidModels != nullptr)
    {
        MemoryDeallocate_(ogi->rigidModels);
    }

    if (ogi->rigidModelJoints != nullptr)
    {
        MemoryDeallocate_(ogi->rigidModelJoints);
    }

    if (ogi->hulls != nullptr)
    {
        // Last to first, like delete[]
        CollisionHull* hull = ogi->hulls + ArrayCount(ogi->hulls);
        while (hull != ogi->hulls)
        {
            hull--;
            HullDestroy(hull, 0);
        }

        DeleteArray(ogi->hulls);
    }

    if (ogi->hullJoints != nullptr)
    {
        MemoryDeallocate_(ogi->hullJoints);
    }

    if (ogi->rigidModelIds != nullptr)
    {
        MemoryDeallocate_(ogi->rigidModelIds);
    }

    if (ogi->jointMatrices != nullptr)
    {
        MemoryDeallocate_(ogi->jointMatrices);
    }

    StringDestroy(&ogi->name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(ogi);
    }
}

void DestroyOgiAnimator(OgiAnimator* animator, u32 destroyFlags)
{
    if (animator->root != nullptr)
    {
        DestroyJointTree(animator->root, DestroyAndFree);
    }

    if (animator->joints != nullptr)
    {
        if (animator->joints->data != nullptr)
        {
            MemoryDeallocate_(animator->joints->data);
        }

        MemoryDeallocate2_(animator->joints);
    }

    animator->root = nullptr;
    animator->joints = nullptr;
    if (animator->exitPoints != nullptr)
    {
        for (u32 index = 0; index < static_cast<u32>(animator->exitPoints->size); index++)
        {
            ExitPointAnimation* point = animator->exitPoints->data[index];
            if (point != nullptr)
            {
                MemoryDeallocate2_(point);
            }
        }

        if (animator->exitPoints->data != nullptr)
        {
            MemoryDeallocate_(animator->exitPoints->data);
        }

        MemoryDeallocate2_(animator->exitPoints);
        animator->exitPoints = nullptr;
    }

    if (animator->callbacks != nullptr)
    {
        for (u32 index = 0; index < static_cast<u32>(animator->callbacks->size); index++)
        {
            JointCallback* callback = animator->callbacks->data[index];
            if (callback != nullptr)
            {
                DestroyJointCallbacks(callback, DestroyAndFree);
            }
        }

        if (animator->callbacks->data != nullptr)
        {
            MemoryDeallocate_(animator->callbacks->data);
        }

        MemoryDeallocate2_(animator->callbacks);
        animator->callbacks = nullptr;
    }

    if (animator->cameraJoints != nullptr)
    {
        if (animator->cameraJoints->data != nullptr)
        {
            MemoryDeallocate_(animator->cameraJoints->data);
        }

        MemoryDeallocate2_(animator->cameraJoints);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(animator);
    }
}

void SetAnimatorPlace(OgiAnimator* animator, void* place)
{
    animator->place = place;
    SizedArray<ExitPointAnimation*>* exitPoints = animator->exitPoints;
    if (exitPoints == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < static_cast<u32>(exitPoints->size); index++)
    {
        ExitPointAnimation* point = exitPoints->data[index];
        if (point != nullptr)
        {
            point->place = animator->place;
            point->bits |= ExitPointAnimation::InUse | ExitPointAnimation::Moved;
        }
    }
}

u32 RemoveJointCallback(OgiAnimator* animator, u32 joint, void* object)
{
    u32 index = joint & 0xFF;
    JointCallback* previous = nullptr;
    for (JointCallback* callback = animator->callbacks->data[index]; callback != nullptr; callback = callback->next)
    {
        if (callback->object != object)
        {
            previous = callback;
            continue;
        }

        JointCallback* next = callback->next;
        if (previous != nullptr)
        {
            previous->next = next;
        }
        else
        {
            JointAnimation* cameraJoint = animator->cameraJoints->data[index];
            animator->callbacks->data[index] = next;
            if (cameraJoint != nullptr)
            {
                cameraJoint->callbacks = next;
            }
        }

        callback->next = nullptr;
        MemoryDeallocate2_(callback);
        return 1;
    }

    return 0;
}

u32 MoveAnimatorThroughLink(OgiAnimator* animator, ChunkData*, const ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return 0;
    }

    u32 moved = 1;
    SizedArray<ExitPointAnimation*>* exitPoints = animator->exitPoints;
    if (exitPoints == nullptr || animator->place == nullptr)
    {
        return moved;
    }

    for (u32 index = 0; index < static_cast<u32>(exitPoints->size); index++)
    {
        ExitPointAnimation* point = exitPoints->data[index];
        if (point == nullptr)
        {
            continue;
        }

        u32 movable = 0;
        if ((link->flags & ChunkLinkData::LinkedRm2Loaded) != 0)
        {
            TransformThroughLink(link, &point->matrix, 1);
            movable = 1;
        }

        moved &= movable;
    }

    return moved;
}

AnimationDataInformation* CopyAnimationData(AnimationDataInformation* information, const AnimationDataSource* source)
{
    if (information->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &information->diskHandle);
    }

    u32 layout = source->layout;
    u32 joints = layout & AnimationDataSource::JointsMask;
    u32 channels = layout >> AnimationDataSource::ChannelsShift & AnimationDataSource::ChannelsMask;
    u32 statics = layout >> AnimationDataSource::StaticsShift & AnimationDataSource::StaticsMask;
    information->frames = source->frames;
    information->sections = joints | channels << AnimationDataSource::ChannelsShift | statics << AnimationDataSource::StaticsShift |
                            (joints * channels - statics) << AnimationDataInformation::FrameValuesShift;
    if (information->diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &information->diskHandle);
    }

    u32 size = AnimationDataSize(information);
    if (size != 0)
    {
        s32 handle;
        DiskAllocate(&handle, GetDiskManager(), size, false, 0);
        information->diskHandle = handle;
    }

    size = AnimationDataSize(information);
    if (size != 0)
    {
        u8* copy = DiskLoadedMemory(GetDiskManager(), &information->diskHandle);
        RetailLibc::MemoryCopy(copy, DiskLoadedMemory(GetDiskManager(), &source->diskHandle), size);
    }

    return information;
}

GameAnimation* MakeAnimation(GameAnimation* animation, const AnimationDataSource* main, const AnimationDataSource* blendShapes)
{
    constexpr u32 ReadBits = 0x30000;
    *reinterpret_cast<u16*>(&animation->unknown00) = 0;
    animation->id = 0xFFFFFFFF;
    animation->main.diskHandle = -1;
    animation->main.sections = 0;
    animation->unknown00 &= ~ReadBits;
    animation->main.frames = 0;
    animation->blendShapes.diskHandle = -1;
    animation->blendShapes.sections = 0;
    animation->blendShapes.frames = 0;
    animation->bits = 0;
    if (main != nullptr)
    {
        animation->bits = GameAnimation::HasMain;
        CopyAnimationData(&animation->main, main);
    }

    if (blendShapes != nullptr)
    {
        animation->bits |= GameAnimation::HasBlendShapes;
        CopyAnimationData(&animation->blendShapes, blendShapes);
    }

    return animation;
}
