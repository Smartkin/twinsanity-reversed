#include "game/attachment.h"

#include "game/animation.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/reference.h"

// The attachments' entries: instances held by another, on an AI position or on a spring

EABI_EXPORT(FUN_00192ef8, AttachToHolder);
EABI_EXPORT(FUN_00193498, SetAttachmentSpring);

namespace
{
InstanceContext* AttachedInstance(const Attachment* attachment)
{
    Reference* reference = attachment->instanceReference;
    return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
}

// The animation of one of an instance's model's exit points (none when its animator has none)
ExitPointAnimation* ExitPointOf(InstanceContext* instance, u8 exitPoint)
{
    auto* node = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    SizedArray<ExitPointAnimation*>* exitPoints = node->animator->exitPoints;
    return exitPoints != nullptr ? exitPoints->data[exitPoint] : nullptr;
}

void SetFollow(Attachment* attachment, u32 follow, u32 kind)
{
    attachment->bits.follow = follow;
    attachment->bits.kind = kind;
}

// The instance's matrix to be kept: its place, made up to date
ObjectPlace* StartKeepingOffset(Attachment* attachment)
{
    attachment->bits.keepsOffset = 1;
    ObjectPlace* place = AttachedInstance(attachment)->place;
    RotateAndTranslate(place);
    return place;
}

// The instance's matrix in a space (its place's matrix times the space's inverse), kept as it's taken
void KeepOffset(Attachment* attachment, const ObjectPlace* place, const Matrix4x4* space)
{
    attachment->offset = *space;
    VuInvertRigidInPlace(&attachment->offset);
    PreMultiply(&attachment->offset, &place->matrix);
    attachment->takenOffset = attachment->offset;
}

// A box's middle: half its size past its lowest corner (its w the highest corner's)
Vector4 BoxMiddle(const Box* box)
{
    Vector4 middle = box->max;
    middle.x = (middle.x - box->min.x) * 0.5f + box->min.x;
    middle.y = (middle.y - box->min.y) * 0.5f + box->min.y;
    middle.z = (middle.z - box->min.z) * 0.5f + box->min.z;
    return middle;
}
}

Attachment* ConstructAttachment(Attachment* attachment, InstanceContext* holder, InstanceContext* instance)
{
    attachment->holder = holder;
    attachment->instanceReference = nullptr;
    attachment->focus = nullptr;
    AssignReference(&attachment->instanceReference, instance);
    SetFollow(attachment, Attachment::FollowsPlace, Attachment::KindInstance);
    attachment->bits.exitPoint = GameOGI::NoExitPoint;
    attachment->instance = instance;
    attachment->bits.keepsOffset = 0;
    attachment->bits.joined = 0;
    attachment->bits.hangs = 0;
    attachment->exitPoint = nullptr;
    attachment->focusExitPoint = nullptr;
    AssignReference(&attachment->focus, nullptr);
    attachment->damping = 0.0f;
    return attachment;
}

Attachment* ConstructPositionAttachment(Attachment* attachment, InstanceContext* holder, AiPosition* position)
{
    SetFollow(attachment, Attachment::FollowsPosition, Attachment::KindPosition);
    attachment->bits.exitPoint = GameOGI::NoExitPoint;
    attachment->holder = holder;
    attachment->position = position;
    attachment->bits.keepsOffset = 0;
    attachment->bits.joined = 0;
    attachment->bits.hangs = 0;
    attachment->instanceReference = nullptr;
    attachment->focus = nullptr;
    attachment->exitPoint = nullptr;
    return attachment;
}

void DestroyAttachment(Attachment* attachment, u32 destroyFlags)
{
    // An instance held stops hanging from the holder
    if (attachment->bits.kind == Attachment::KindInstance && AttachedInstance(attachment) != nullptr)
    {
        AttachedInstance(attachment)->flags.attached = 0;
        AttachedInstance(attachment)->parent = nullptr;
        AttachedInstance(attachment)->collision.leftOut = nullptr;
    }

    RemoveReference(&attachment->focus);
    RemoveReference(&attachment->instanceReference);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(attachment);
    }
}

void AttachToHolder(f32 push, Attachment* attachment, u32 keepOffset, Matrix4x4* matrix)
{
    if (AttachedInstance(attachment) == nullptr)
    {
        return;
    }

    ObjectPlace* holderPlace = attachment->holder->place;
    RotateAndTranslate(holderPlace);
    if (matrix != nullptr)
    {
        MultiplyInPlace(matrix, &holderPlace->matrix);
        InstanceContext* instance = AttachedInstance(attachment);
        if (SetPlaceMatrix(instance->place, matrix) != 0)
        {
            QueueObject(instance);
        }
    }

    if (push != 0.0f)
    {
        Vector4 holderMiddle = BoxMiddle(attachment->holder->CollisionBox());
        Vector4 middle = BoxMiddle(AttachedInstance(attachment)->CollisionBox());
        InstanceContext* instance = AttachedInstance(attachment);
        Vector4 away;
        away.x = middle.x - holderMiddle.x;
        away.y = middle.y - holderMiddle.y;
        away.z = middle.z - holderMiddle.z;
        away.w = 1.0f;
        f32 inverse = InverseLength(&away, LengthEpsilon);
        Vector4 move;
        move.x = away.x * inverse * push;
        move.y = away.y * inverse * push;
        move.z = away.z * inverse * push;
        move.w = 1.0f;
        if (instance->place->MoveBy(&move))
        {
            QueueObject(instance);
        }
    }

    if (keepOffset != 0)
    {
        ObjectPlace* place = StartKeepingOffset(attachment);
        KeepOffset(attachment, place, &holderPlace->matrix);
    }
}

void AttachAtExitPoint(Attachment* attachment, u32 exitPoint, u32 keepOffset, Matrix4x4* matrix)
{
    u8 index = static_cast<u8>(exitPoint);
    attachment->bits.exitPoint = index;
    attachment->exitPoint = ExitPointOf(attachment->holder, index);
    if (AttachedInstance(attachment) == nullptr)
    {
        return;
    }

    if (matrix != nullptr)
    {
        MultiplyInPlace(matrix, &UpdateExitPointMatrix(attachment->exitPoint)->matrix);
        InstanceContext* instance = AttachedInstance(attachment);
        if (SetPlaceMatrix(instance->place, matrix) != 0)
        {
            QueueObject(instance);
        }
    }

    if (keepOffset != 0)
    {
        ObjectPlace* place = StartKeepingOffset(attachment);
        KeepOffset(attachment, place, &UpdateExitPointMatrix(attachment->exitPoint)->matrix);
    }
}

void SetAttachmentSpring(f32 stiffness, f32 length, Attachment* attachment, const Vector4* point, const Vector4* offset,
                         InstanceContext* focus, u32 focusExitPoint)
{
    u8 exitPoint = static_cast<u8>(focusExitPoint);
    attachment->point = *point;
    AssignReference(&attachment->focus, focus);
    attachment->stiffness = stiffness;
    attachment->length = length;
    if (focus != nullptr && exitPoint != GameOGI::NoExitPoint)
    {
        attachment->focusExitPoint = ExitPointOf(focus, exitPoint);
    }

    if (offset != nullptr)
    {
        InitIdentityMatrix(&attachment->offset);
        attachment->bits.keepsOffset = 1;
        *RowOf(&attachment->offset, 3) = *offset;
    }

    SetFollow(attachment, Attachment::FollowsSpring, Attachment::KindSpring);
}

void SetAttachmentExitPoint(Attachment* attachment, u32 exitPoint)
{
    u8 index = static_cast<u8>(exitPoint);
    attachment->bits.exitPoint = index;
    if (index != GameOGI::NoExitPoint)
    {
        attachment->exitPoint = ExitPointOf(attachment->holder, index);
    }
}

void HangFromHolder(Attachment* attachment, InstanceContext* instance)
{
    AssignReference(&attachment->instanceReference, instance);
    attachment->bits.hangs = 1;
    instance->collision.leftOut = attachment->holder;
    instance->flags.attached = 1;
    instance->parent = attachment->holder;
    attachment->holder->flags.hasAttachment = 1;
}
