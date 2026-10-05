#include "game/attachments.h"

#include "game/animation.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/rigidbody.h"

// An instance's attachments (its node of kind 6): the instances linked to it, and the path of what's attached to it (instances,
// AI positions and springs' ends following it in their ways: game/attachment.h's attachments)

EABI_EXPORT(FUN_00193848, HoldAttachmentOnSpring);
EABI_EXPORT(FUN_001939e8, HoldAttachmentInPlace);
EABI_EXPORT(FUN_001968c0, HoldOnSpring);
EABI_EXPORT(FUN_00196958, HoldInPlace);

// AttachSpring has more arguments than n32 passes in registers: the asm hands it the eight integers in $a0-$a7 and the three
// floats in $f12-$f14, the C++ function takes the floats there, the first five integers in $a3-$a7 and the rest on the stack
asm(R"(
    .pushsection .text.FUN_001966b0, "ax", @progbits
    .globl FUN_001966b0
    .type FUN_001966b0, @function
    .set push
    .set noreorder
FUN_001966b0:
    addiu $sp, $sp, -0x30
    sd $ra, 0x20($sp)
    sd $9, 0x0($sp)
    sd $10, 0x8($sp)
    sd $11, 0x10($sp)
    move $11, $8
    move $10, $7
    move $9, $6
    move $8, $5
    jal FUN_001966b0_n32
    move $7, $4
    ld $ra, 0x20($sp)
    jr $ra
    addiu $sp, $sp, 0x30
    .set pop
    .size FUN_001966b0, . - FUN_001966b0
    .popsection
)");

namespace
{
// How far a joining attachment's instance and holder are from each other (squared) once they have met, and how much of the
// way to the holder's up its instance's up turns each time when it hangs
constexpr f32 JoinedDistanceSquared = Rounded(0.1);
constexpr f32 UpTurn = Rounded(0.1);

InstanceContext* AttachedInstance(const Attachment* attachment)
{
    Reference* reference = attachment->instanceReference;
    return reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
}

InstanceContext* FocusOf(const Attachment* attachment)
{
    return attachment->focus != nullptr ? static_cast<InstanceContext*>(attachment->focus->object) : nullptr;
}

GameNode* NodeOf(InstanceContext* instance, u32 kind)
{
    return static_cast<GameNode*>(GetGameNode(&instance->nodes, kind));
}

AttachmentsNode* AsNode(void* attachments)
{
    return static_cast<AttachmentsNode*>(attachments);
}

// A matrix set as an instance's place (queued to be stepped when it changed)
void PlaceAt(InstanceContext* instance, const Matrix4x4* matrix)
{
    if (SetPlaceMatrix(instance->place, matrix) != 0)
    {
        QueueObject(instance);
    }
}

// An instance moved to a position, moved by an offset (queued to be stepped when it did)
void MoveInstance(InstanceContext* instance, const Vector4* position)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(position))
    {
        QueueObject(instance);
    }
}

void MoveInstanceBy(InstanceContext* instance, const Vector4* move)
{
    if (instance->place->MoveBy(move))
    {
        QueueObject(instance);
    }
}

// The middle of an instance's collision box (its w the box's lowest corner's)
Vector4 MiddleOf(const InstanceContext* instance)
{
    Vector4 middle = instance->collision.box.min;
    const Vector4* max = &instance->collision.box.max;
    middle.x = (middle.x + max->x) * 0.5f;
    middle.y = (middle.y + max->y) * 0.5f;
    middle.z = (middle.z + max->z) * 0.5f;
    return middle;
}

// A new path for the node
void MakePath(AttachmentsNode* node)
{
    node->path = ConstructPath(static_cast<AttachmentsPath*>(MemoryAllocate(sizeof(AttachmentsPath))));
    node->bits.noPath = 0;
}

// An attachment put at the end of a path (16 at most: the callers check)
void Append(AttachmentsPath* path, Attachment* attachment)
{
    u32 count = path->Count();
    path->bits.count = count + 1;
    path->entries[count] = attachment;
}

// An instance taken off what it hung on, launched by its object node
void LaunchTaken(InstanceContext* taken)
{
    if (taken == nullptr)
    {
        return;
    }

    taken->parent = nullptr;
    taken->flags.attached = 0;
    GameNode* node = NodeOf(taken, NodeObject);
    if (node != nullptr)
    {
        // Retail bug: the velocity is what the stack held
        Vector4 velocity;
        CallVirtual<void>(node, node->vtable, ObjectNode::LaunchSlot, LaunchWithDefaultGravity, &velocity);
    }
}

// What a new attachment of an instance hanging on a holder leaves: the last attachment made, the instance held by the holder and
// left out of its collisions, its link marked when asked
void Attached(AttachmentsNode* node, Attachment* attachment, InstanceContext* holder, InstanceContext* instance, AttachFlags flags)
{
    AddToPath(node->path, attachment);
    node->bits.noPath = 0;
    g_LastAttachment = attachment;
    instance->collision.leftOut = holder;
    instance->flags.attached = 1;
    instance->parent = holder;
    if (flags.marksLink)
    {
        node->linkFlags[IndexOfLinked(node, instance)].marked = 1;
    }
}
}

void AttachmentAnchor(Attachment* attachment, Vector4* out)
{
    if (FocusOf(attachment) == nullptr)
    {
        *out = attachment->point;
        return;
    }

    if (attachment->focusExitPoint != nullptr)
    {
        *out = *RowOf(&UpdateExitPointMatrix(attachment->focusExitPoint)->matrix, 3);
        return;
    }

    ObjectPlace* place = FocusOf(attachment)->place;
    RotateAndTranslate(place);
    VuTransformPoint(&place->matrix, &attachment->point, out);
}

void HoldAttachmentOnSpring(Attachment* attachment, f32 strength, f32 stiffness)
{
    constexpr f32 SlowRate = Rounded(0.15);
    constexpr f32 SlowSeconds = Rounded(0.02);
    attachment->length = 0.0f;
    attachment->stiffness = stiffness;
    attachment->damping = strength;
    MakeAttachmentMatrix(attachment, 0);
    attachment->bits.follow = Attachment::FollowsJoining;
    AttachedInstance(attachment)->flags.attached = 1;
    AttachedInstance(attachment)->parent = attachment->holder;
    AttachedInstance(attachment)->collision.leftOut = attachment->holder;
    attachment->holder->collision.leftOut = AttachedInstance(attachment);
    attachment->bits.joined = 0;
    auto* body = static_cast<RigidBody*>(NodeOf(attachment->holder, NodeRigidBody));
    HandOnVelocity(attachment);
    if (body != nullptr)
    {
        body->Slow(SlowRate, SlowSeconds);
    }
}

void HandOnVelocity(Attachment* attachment)
{
    auto* body = static_cast<RigidBody*>(NodeOf(attachment->holder, NodeRigidBody));
    Vector4 velocity;
    if (body != nullptr)
    {
        velocity = body->velocity;
    }
    else
    {
        velocity = static_cast<ObjectNode*>(NodeOf(attachment->holder, NodeObject))->motion->velocity;
    }

    MotionState* motion = static_cast<ObjectNode*>(NodeOf(AttachedInstance(attachment), NodeObject))->motion;
    motion->startVelocity = motion->velocity;
    motion->velocity = velocity;
}

void HoldAttachmentInPlace(Attachment* attachment, f32 strength, f32 stiffness)
{
    attachment->length = 0.0f;
    attachment->stiffness = stiffness;
    attachment->damping = strength;
    InitIdentityMatrix(&attachment->offset);
    ObjectPlace* place = attachment->holder->place;
    place->SyncPosition();
    attachment->point = place->position;
    place = attachment->holder->place;
    place->SyncRotation();
    Vector4 rotation = place->rotation;
    MatrixFromRotation(&attachment->offset, &rotation);
    *RowOf(&attachment->offset, 3) = attachment->point;
    attachment->bits.follow = Attachment::FollowsPinned;
    attachment->bits.joined = 0;
}

void MakeAttachmentMatrix(Attachment* attachment, u32 positionOnly)
{
    // Only instances and AI positions get here: the matrix of anything else would be what the stack held
    Matrix4x4 target;
    u32 kind = attachment->bits.kind;
    if (kind == Attachment::KindInstance)
    {
        ObjectPlace* place = AttachedInstance(attachment)->place;
        RotateAndTranslate(place);
        target = place->matrix;
    }
    else if (kind == Attachment::KindPosition)
    {
        Vector4 position = attachment->position->position;
        position.w = 1.0f;
        InitIdentityMatrix(&target);
        *RowOf(&target, 3) = position;
    }

    if (positionOnly != 0)
    {
        InitIdentityMatrix(&attachment->offset);
        ObjectPlace* place = attachment->holder->place;
        place->SyncPosition();
        Vector4 position = place->position;
        *RowOf(&attachment->offset, 3) = position;
    }
    else
    {
        ObjectPlace* place = attachment->holder->place;
        RotateAndTranslate(place);
        attachment->offset = place->matrix;
    }

    VuInvertRigidInPlace(&attachment->offset);
    PreMultiply(&attachment->offset, &target);
}

u32 UpdateAttachment(Attachment* attachment)
{
    AttachmentBits bits = attachment->bits;
    if (bits.kind == Attachment::KindInstance || bits.hangs)
    {
        if (AttachedInstance(attachment) != nullptr && !AttachedInstance(attachment)->flags.attached)
        {
            return 0;
        }
    }

    const Matrix4x4* holder;
    if (attachment->exitPoint != nullptr)
    {
        holder = &UpdateExitPointMatrix(attachment->exitPoint)->matrix;
    }
    else
    {
        ObjectPlace* place = attachment->holder->place;
        RotateAndTranslate(place);
        holder = &place->matrix;
    }

    Matrix4x4 placed;
    switch (attachment->bits.follow)
    {
    case Attachment::FollowsPlace:
        if (attachment->bits.kind != Attachment::KindInstance)
        {
            return 1;
        }

        if (AttachedInstance(attachment) == nullptr)
        {
            return 0;
        }

        if (attachment->bits.keepsOffset)
        {
            VuMultiplyMatrices(&attachment->offset, holder, &placed);
            PlaceAt(AttachedInstance(attachment), &placed);
        }
        else
        {
            PlaceAt(AttachedInstance(attachment), holder);
        }

        HandOnVelocity(attachment);
        return 1;
    case Attachment::FollowsPosition:
        if (attachment->bits.kind == Attachment::KindInstance)
        {
            if (attachment->bits.keepsOffset)
            {
                VuMultiplyMatrices(&attachment->offset, holder, &placed);
                MoveInstance(AttachedInstance(attachment), RowOf(&placed, 3));
            }
            else
            {
                MoveInstance(AttachedInstance(attachment), RowOf(holder, 3));
            }

            HandOnVelocity(attachment);
        }
        else if (attachment->bits.kind == Attachment::KindPosition)
        {
            VuMultiplyMatrices(&attachment->offset, holder, &placed);
            Vector4* position = &attachment->position->position;
            position->x = placed.m[3][0];
            position->y = placed.m[3][1];
            position->z = placed.m[3][2];
        }

        return 1;
    case Attachment::FollowsHanging:
        UpdateHanging(attachment, holder);
        return 1;
    case Attachment::FollowsSpring:
        UpdateSpring(attachment, holder);
        return 1;
    case Attachment::FollowsPinned:
    {
        auto* body = static_cast<RigidBody*>(NodeOf(attachment->holder, NodeRigidBody));
        if (body != nullptr)
        {
            body->SetMatrix(&attachment->offset);
            return 1;
        }

        MoveInstance(attachment->holder, RowOf(&attachment->offset, 3));
        InstanceContext* pinned = attachment->holder;
        ObjectPlace* place = pinned->place;
        place->SyncRotation();
        Vector4 rotation;
        GetRotationVec(&rotation, &attachment->offset);
        if (place->TurnTo(&rotation))
        {
            QueueObject(pinned);
        }

        return 1;
    }
    case Attachment::FollowsJoining:
        if (!attachment->bits.joined)
        {
            Vector4 middle = MiddleOf(AttachedInstance(attachment));
            InstanceContext* holderInstance = attachment->holder;
            Vector4 meeting = MiddleOf(holderInstance);
            // The instance's middle meets the holder's at the distance along the holder's z
            Vector4 offset = {0.0f, 0.0f, attachment->damping, 1.0f};
            ObjectPlace* place = holderInstance->place;
            RotateAndTranslate(place);
            const Vector4* rowX = RowOf(&place->matrix, 0);
            const Vector4* rowY = RowOf(&place->matrix, 1);
            const Vector4* rowZ = RowOf(&place->matrix, 2);
            meeting.z = meeting.z + ((rowX->z * offset.x + rowY->z * offset.y) + rowZ->z * offset.z);
            meeting.x = meeting.x + ((rowX->x * offset.x + rowY->x * offset.y) + rowZ->x * offset.z);
            meeting.y = meeting.y + ((rowX->y * offset.x + rowY->y * offset.y) + rowZ->y * offset.z);
            Vector4 gap = meeting;
            gap.x = gap.x - middle.x;
            gap.y = gap.y - middle.y;
            gap.z = gap.z - middle.z;
            if (JoinedDistanceSquared < gap.x * gap.x + gap.y * gap.y + gap.z * gap.z)
            {
                // Each goes half the way
                gap.z = gap.z * 0.5f;
                gap.x = gap.x * 0.5f;
                gap.y = gap.y * 0.5f;
                MoveInstanceBy(AttachedInstance(attachment), &gap);
                gap.x = -gap.x;
                gap.y = -gap.y;
                gap.z = -gap.z;
                MoveInstanceBy(attachment->holder, &gap);
                MakeAttachmentMatrix(attachment, 0);
            }
            else
            {
                attachment->bits.joined = 1;
            }
        }

        VuMultiplyMatrices(&attachment->offset, holder, &placed);
        PlaceAt(AttachedInstance(attachment), &placed);
        HandOnVelocity(attachment);
        return 1;
    default:
        return 1;
    }
}

u32 UpdateHanging(Attachment* attachment, const Matrix4x4* holder)
{
    if (attachment->bits.kind != Attachment::KindInstance)
    {
        return 1;
    }

    ObjectPlace* place = AttachedInstance(attachment)->place;
    place->SyncPosition();
    Vector4 from = place->position;
    // The point it hangs from, and how far from it it hangs (its offset's length)
    Matrix4x4 placed;
    Vector4 to;
    f32 length;
    if (attachment->bits.keepsOffset)
    {
        VuMultiplyMatrices(&attachment->offset, holder, &placed);
        const Vector4* offset = RowOf(&attachment->offset, 3);
        length = __builtin_sqrtf(offset->x * offset->x + offset->y * offset->y + offset->z * offset->z);
        to = *RowOf(&placed, 3);
    }
    else
    {
        to = *RowOf(holder, 3);
        length = 0.0f;
    }

    Vector4 forward = to;
    forward.x = forward.x - from.x;
    forward.z = forward.z - from.z;
    forward.y = forward.y - from.y;
    f32 scale = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * scale;
    forward.y = forward.y * scale;
    forward.z = forward.z * scale;
    place = AttachedInstance(attachment)->place;
    RotateAndTranslate(place);
    Vector4 up = *RowOf(&place->matrix, 1);
    Vector4 holderUp = *RowOf(holder, 1);
    up.x = up.x + (holderUp.x - up.x) * UpTurn;
    up.y = up.y + (holderUp.y - up.y) * UpTurn;
    up.z = up.z + (holderUp.z - up.z) * UpTurn;
    up.w = 1.0f;
    scale = InverseLength(&up, LengthEpsilon);
    up.x = up.x * scale;
    up.y = up.y * scale;
    up.z = up.z * scale;
    Vector4 side;
    side.x = forward.y * up.z - forward.z * up.y;
    side.y = forward.z * up.x - forward.x * up.z;
    side.z = forward.x * up.y - forward.y * up.x;
    side.w = 1.0f;
    side.x = -side.x;
    side.y = -side.y;
    side.z = -side.z;
    up.x = forward.y * side.z - forward.z * side.y;
    up.y = forward.z * side.x - forward.x * side.z;
    up.z = forward.x * side.y - forward.y * side.x;
    up.w = 1.0f;
    Vector4 origin = {0.0f, 0.0f, 0.0f, 1.0f};
    MatrixFromRows(&placed, &side, &up, &forward, &origin);
    InstanceContext* instance = AttachedInstance(attachment);
    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, &placed);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    forward.x = forward.x * length;
    forward.y = forward.y * length;
    forward.z = forward.z * length;
    to.x = to.x - forward.x;
    to.y = to.y - forward.y;
    to.z = to.z - forward.z;
    MoveInstance(AttachedInstance(attachment), &to);
    return 1;
}

u32 UpdateSpring(Attachment* attachment, const Matrix4x4* holder)
{
    auto* node = static_cast<ObjectNode*>(NodeOf(attachment->holder, NodeObject));
    if (CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        return 1;
    }

    Vector4 end;
    AttachmentAnchor(attachment, &end);
    Matrix4x4 placed;
    Vector4 start;
    if (attachment->bits.keepsOffset)
    {
        VuMultiplyMatrices(&attachment->offset, holder, &placed);
        start = *RowOf(&placed, 3);
    }
    else
    {
        start = *RowOf(holder, 3);
    }

    // The spring pulls the holder's body at its start (the body's middle when it starts at the holder's place)
    if (node->rigidBody != nullptr && node->rigidBody->physicsBody != nullptr)
    {
        DynamicBody* body = node->rigidBody->physicsBody;
        Vector4 local;
        if (!attachment->bits.keepsOffset && attachment->exitPoint == nullptr)
        {
            local = {0.0f, 0.0f, 0.0f, 1.0f};
        }
        else
        {
            VuTransformPoint(&body->inverseMatrix, &start, &local);
        }

        body->Spring(attachment->length, attachment->stiffness, attachment->damping, &end, &local, 0);
    }

    if (!attachment->bits.hangs)
    {
        return 1;
    }

    // The end's instance stretched along its z from the start to the end, its x square to that and to the holder's x
    InstanceContext* instance = AttachedInstance(attachment);
    Vector4 along = end;
    f32 depth = instance->collision.ownBox.max.z - instance->collision.ownBox.min.z;
    along.x = along.x - start.x;
    along.y = along.y - start.y;
    along.z = along.z - start.z;
    f32 squared = along.x * along.x + along.y * along.y + along.z * along.z;
    Vector4 holderX = *RowOf(holder, 0);
    f32 length = __builtin_sqrtf(squared);
    f32 stretch = length / depth;
    f32 inverse = 1.0f / Kept(length);
    along.z = along.z * inverse;
    along.y = along.y * inverse;
    along.x = along.x * inverse;
    Vector4 y;
    y.x = along.y * holderX.z - along.z * holderX.y;
    y.y = along.z * holderX.x - along.x * holderX.z;
    y.z = along.x * holderX.y - along.y * holderX.x;
    y.w = 1.0f;
    if (__builtin_fabsf(__builtin_sqrtf(y.x * y.x + y.y * y.y + y.z * y.z)) <= Epsilon)
    {
        return 1;
    }

    f32 scale = InverseLength(&y, LengthEpsilon);
    y.x = y.x * scale;
    y.y = y.y * scale;
    y.z = y.z * scale;
    Vector4 x;
    x.x = along.y * y.z - along.z * y.y;
    x.y = along.z * y.x - along.x * y.z;
    x.z = along.x * y.y - along.y * y.x;
    x.w = 1.0f;
    along.x = along.x * stretch;
    along.y = along.y * stretch;
    along.z = along.z * stretch;
    Matrix4x4 matrix;
    InitIdentityMatrix(&matrix);
    MatrixFromAxes(&matrix, &x, &y, &along);
    *RowOf(&matrix, 3) = start;
    PlaceAt(AttachedInstance(attachment), &matrix);
    return 1;
}

AttachmentsPath* ConstructPath(AttachmentsPath* path)
{
    path->bits.count = 0;
    return path;
}

void DestroyPath(AttachmentsPath* path, u32 destroyFlags)
{
    ClearPath(path);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(path);
    }
}

u32 AddToPath(AttachmentsPath* path, Attachment* attachment)
{
    switch (attachment->bits.kind)
    {
    case Attachment::KindInstance:
    {
        InstanceContext* instance = AttachedInstance(attachment);
        if (instance != nullptr && PathIndexOf(path, instance) != -1)
        {
            return 0;
        }

        Append(path, attachment);
        if (instance != nullptr)
        {
            auto* node = static_cast<ObjectNode*>(NodeOf(instance, NodeObject));
            if (node != nullptr && CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0)
            {
                node->motion->Stop();
            }
        }

        return 1;
    }
    case Attachment::KindPosition:
        for (u8 index = 0; index < path->Count(); index++)
        {
            if (path->entries[index]->bits.kind == Attachment::KindPosition)
            {
                return 0;
            }
        }

        Append(path, attachment);
        return 1;
    case Attachment::KindSpring:
        Append(path, attachment);
        return 1;
    default:
        return 1;
    }
}

s32 PathIndexOf(void* path, InstanceContext* instance)
{
    auto* attachments = static_cast<AttachmentsPath*>(path);
    u32 count = attachments->Count();
    for (u32 index = 0; index < count; index++)
    {
        if (AttachedInstance(attachments->entries[index]) == instance)
        {
            return index;
        }
    }

    return -1;
}

Attachment* AttachmentOn(AttachmentsPath* path, InstanceContext* instance)
{
    u32 count = path->Count();
    for (u32 index = 0; index < count; index++)
    {
        if (AttachedInstance(path->entries[index]) == instance)
        {
            return path->entries[index];
        }
    }

    return nullptr;
}

// Without the loops made into calls of memmove and memset, which the game doesn't have and doesn't make
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void RemoveFromPath(AttachmentsPath* path, u32 index, u32 update)
{
    if (update != 0)
    {
        UpdateAttachment(path->entries[index]);
    }

    if (path->entries[index] != nullptr)
    {
        DestroyAttachment(path->entries[index], DestroyAndFree);
    }

    for (u32 next = index + 1; next < path->Count(); next++)
    {
        path->entries[next - 1] = path->entries[next];
    }

    path->bits.count = path->Count() - 1;
}

void ClearPath(AttachmentsPath* path)
{
    for (u32 index = 0; index < path->Count(); index++)
    {
        if (path->entries[index] != nullptr)
        {
            DestroyAttachment(path->entries[index], DestroyAndFree);
            path->entries[index] = nullptr;
        }
    }

    path->bits.count = 0;
}

InstanceContext* UnslottedAttachment(void* path)
{
    return SlottedAttachment(path, GameOGI::NoExitPoint);
}

InstanceContext* SlottedAttachment(void* path, u32 slot)
{
    auto* attachments = static_cast<AttachmentsPath*>(path);
    u32 count = attachments->Count();
    for (u32 index = 0; index < count; index++)
    {
        Attachment* attachment = attachments->entries[index];
        if (attachment->bits.exitPoint == static_cast<u8>(slot))
        {
            return AttachedInstance(attachment);
        }
    }

    return nullptr;
}

InstanceContext* TakeUnslotted(AttachmentsPath* path)
{
    return TakeSlotted(path, GameOGI::NoExitPoint);
}

InstanceContext* TakeSlotted(AttachmentsPath* path, u32 slot)
{
    for (u32 index = 0; index < path->Count(); index++)
    {
        Attachment* attachment = path->entries[index];
        if (attachment->bits.exitPoint == static_cast<u8>(slot))
        {
            InstanceContext* instance = AttachedInstance(attachment);
            RemoveFromPath(path, index, 1);
            return instance;
        }
    }

    return nullptr;
}

u32 RemoveInstanceFromPath(AttachmentsPath* path, InstanceContext* instance, u32, u32 update)
{
    s32 index = PathIndexOf(path, instance);
    if (index == -1)
    {
        return 0;
    }

    if (update != 0)
    {
        UpdateAttachment(path->entries[index]);
    }

    RemoveFromPath(path, index, update);
    return 1;
}

InstanceContext* UpdatePath(AttachmentsPath* path)
{
    InstanceContext* lost = nullptr;
    for (u32 index = 0; index < path->Count(); index++)
    {
        if (UpdateAttachment(path->entries[index]) == 0)
        {
            lost = AttachedInstance(path->entries[index]);
        }
    }

    return lost;
}

s32 AttachedInstances(AttachmentsPath* path, InstanceContext** out, s32)
{
    for (u32 index = 0; index < path->Count(); index++)
    {
        out[index] = AttachedInstance(path->entries[index]);
    }

    return path->Count();
}

s32 IndexOfLinked(void* attachments, InstanceContext* instance)
{
    AttachmentsNode* node = AsNode(attachments);
    u32 count = node->LinkedCount();
    for (u32 index = 0; index < count; index++)
    {
        if (node->linked[index] == instance)
        {
            return index;
        }
    }

    return -1;
}

u32 AttachmentsNode::UnlinkAt(u32 index, u32 force)
{
    if (force == 0 && linkFlags[index].kept)
    {
        return 0;
    }

    bits.linkedCount = LinkedCount() - 1;
    u32 count = LinkedCount();
    if (count != 0)
    {
        linked[index] = linked[count];
        linkFlags[index] = linkFlags[LinkedCount()];
    }

    return 1;
}

u32 AttachmentsNode::LetGo(InstanceContext* instance)
{
    if (instance == nullptr)
    {
        return 0;
    }

    instance->collision.leftOut = nullptr;
    FreeEmptyPath();
    s32 index = IndexOfLinked(this, instance);
    if (index == -1)
    {
        return 0;
    }

    return UnlinkAt(index, 0) != 0;
}

void AttachmentsNode::FreeEmptyPath()
{
    if (path == nullptr || path->Count() != 0)
    {
        return;
    }

    DestroyPath(path, DestroyAndFree);
    path = nullptr;
    bits.noPath = 1;
}

AttachmentsNode* AttachmentsNode::Construct(AttachmentsNode* node)
{
    GameNode::Construct(node);
    node->vtable = g_AttachmentsNodeVTable;
    node->Reset();
    return node;
}

void AttachmentsNode::Destroy(u32 destroyFlags)
{
    vtable = g_AttachmentsNodeVTable;
    UnlinkAll(this);
    GameNode::Destroy(destroyFlags);
}

u32 AttachmentsNode::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    u32 moved = 1;
    for (u32 index = 0; index < LinkedCount(); index++)
    {
        if (linked[index] != nullptr)
        {
            moved &= linked[index]->ChangeChunk(link) != nullptr;
        }
    }

    return moved;
}

void AttachmentsNode::Step(TimeClock*, u32)
{
    ReleaseAttachmentsNode(this, 0, 0, 1);
    UnlinkUnkept();
    owner->flags.hasAttachment = 0;
    owner->flags.attached = 0;
    owner->parent = nullptr;
    stickiness = 0.0f;
    bits.currentLinked = 0;
    if (NoneKept() != 0)
    {
        RemoveNode(owner, this);
    }
}

__attribute__((optimize("no-tree-loop-distribute-patterns"))) void AttachmentsNode::Reset()
{
    bits.linkedCount = 0;
    bits.noPath = 0;
    bits.currentLinked = 0;
    path = nullptr;
    stickiness = 0.0f;
    for (u8 index = 0; index < MostLinked; index++)
    {
        linkFlags[index].value = 0;
    }
}

u32 LinkInstance(void* attachments, InstanceContext* instance, u32 flag)
{
    AttachmentsNode* node = AsNode(attachments);
    s32 index = instance != nullptr ? IndexOfLinked(node, instance) : -1;
    if (index >= 0)
    {
        return 1;
    }

    u32 count = node->LinkedCount();
    if (count >= AttachmentsNode::MostLinked)
    {
        return 0;
    }

    if (flag != 0)
    {
        node->linkFlags[count].kept = 1;
    }
    else
    {
        node->linkFlags[count].kept = 0;
    }

    count = node->LinkedCount();
    node->bits.linkedCount = count + 1;
    node->linked[count] = instance;
    return 1;
}

void LinkAllOf(void* attachments, void* other)
{
    AttachmentsNode* from = AsNode(other);
    for (u32 index = 0; index < from->LinkedCount(); index++)
    {
        InstanceContext* instance = from->linked[index];
        AttachmentLinkFlags flags = from->linkFlags[index];
        if (instance != nullptr)
        {
            LinkInstance(attachments, instance, flags.kept);
        }
    }
}

u32 AttachmentsNode::NoneKept()
{
    u32 count = LinkedCount();
    for (u32 index = 0; index < count; index++)
    {
        if (linkFlags[index].kept)
        {
            return 0;
        }
    }

    return 1;
}

u32 HangOnExitPoint(void* attachments, InstanceContext* holder, InstanceContext* instance, u32 exitPoint, u32 flags,
                    const Matrix4x4* offset, u32 force)
{
    AttachmentsNode* node = AsNode(attachments);
    u8 slot = static_cast<u8>(exitPoint);
    AttachFlags attach = {flags};
    if (force == 0 && LinkInstance(node, instance, 0) == 0)
    {
        return 0;
    }

    if (node->path == nullptr)
    {
        MakePath(node);
    }
    else if (attach.replaces)
    {
        if (attach.launchesReplaced)
        {
            LaunchTaken(TakeSlotted(node->path, slot));
        }
        else
        {
            TakeSlotted(node->path, slot);
        }
    }

    if (node->path->Count() >= AttachmentsPath::Most)
    {
        return 0;
    }

    Attachment* attachment = ConstructAttachment(static_cast<Attachment*>(MemoryAllocate(sizeof(Attachment))), holder, instance);
    AttachAtExitPoint(attachment, slot, attach.withOffset, const_cast<Matrix4x4*>(offset));
    Attached(node, attachment, holder, instance, attach);
    return 1;
}

Attachment* AttachmentsNode::Add(InstanceContext* holder, InstanceContext* instance)
{
    if (LinkInstance(this, instance, 0) == 0)
    {
        return nullptr;
    }

    if (path == nullptr)
    {
        MakePath(this);
    }

    if (path->Count() >= AttachmentsPath::Most)
    {
        return nullptr;
    }

    Attachment* attachment = ConstructAttachment(static_cast<Attachment*>(MemoryAllocate(sizeof(Attachment))), holder, instance);
    AddToPath(path, attachment);
    bits.noPath = 0;
    g_LastAttachment = attachment;
    return attachment;
}

u32 AttachSpring(f32 power, f32 damping, f32 length, void* attachments, InstanceContext* instance, const Vector4* target,
                 u32 joint, const Vector4* offset, InstanceContext* end, InstanceContext* focus, u32 focusJoint)
{
    Attachment* attachment = AsNode(attachments)->Add(instance, end);
    if (attachment == nullptr)
    {
        return 0;
    }

    SetAttachmentExitPoint(attachment, static_cast<u8>(joint));
    SetAttachmentSpring(power, length, attachment, target, offset, focus, static_cast<u8>(focusJoint));
    attachment->damping = damping;
    if (end != nullptr)
    {
        HangFromHolder(attachment, end);
    }

    return 1;
}

u32 AttachToAiPosition(void* attachments, InstanceContext* instance, AiPosition* position)
{
    AttachmentsNode* node = AsNode(attachments);
    if (node->path == nullptr)
    {
        MakePath(node);
    }

    if (node->path->Count() >= AttachmentsPath::Most)
    {
        return 0;
    }

    Attachment* attachment =
        ConstructPositionAttachment(static_cast<Attachment*>(MemoryAllocate(sizeof(Attachment))), instance, position);
    attachment->bits.follow = Attachment::FollowsPosition;
    // Retail leaks the attachment when the path has an AI position already
    if (AddToPath(node->path, attachment) == 0)
    {
        return 0;
    }

    node->bits.noPath = 0;
    Vector4 point = attachment->position->position;
    point.w = 1.0f;
    attachment->point = point;
    attachment->position->flags.value |= g_AttachedPositionFlag;
    MakeAttachmentMatrix(attachment, 0);
    g_LastAttachment = attachment;
    return 1;
}

u32 HoldOnSpring(f32 strength, f32 stiffness, void* attachments, InstanceContext* holder, InstanceContext* held)
{
    Attachment* attachment = AsNode(attachments)->Add(holder, held);
    if (attachment == nullptr)
    {
        return 0;
    }

    if (held->parent != nullptr)
    {
        void* before = GetGameNode(&held->parent->nodes, NodeAttachments);
        if (before != nullptr)
        {
            UnlinkInstance(before, held, 0, 1, 0);
        }
    }

    HoldAttachmentOnSpring(attachment, strength, stiffness);
    return 1;
}

u32 HoldInPlace(f32 strength, f32 stiffness, void* attachments, InstanceContext* held)
{
    // Nothing hangs: the attachment keeps its holder where it is
    Attachment* attachment = AsNode(attachments)->Add(held, nullptr);
    if (attachment == nullptr)
    {
        return 0;
    }

    HoldAttachmentInPlace(attachment, strength, stiffness);
    return 1;
}

u32 AttachInstance(void* attachments, InstanceContext* holder, InstanceContext* instance, u32 flags, const Matrix4x4* offset)
{
    AttachmentsNode* node = AsNode(attachments);
    AttachFlags attach = {flags};
    if (LinkInstance(node, instance, 0) == 0)
    {
        return 0;
    }

    if (node->path == nullptr)
    {
        MakePath(node);
    }
    else if (attach.replaces)
    {
        if (attach.launchesReplaced)
        {
            LaunchTaken(TakeUnslotted(node->path));
        }
        else
        {
            TakeUnslotted(node->path);
        }
    }

    if (node->path->Count() >= AttachmentsPath::Most)
    {
        return 0;
    }

    Attachment* attachment = ConstructAttachment(static_cast<Attachment*>(MemoryAllocate(sizeof(Attachment))), holder, instance);
    AttachToHolder(node->stickiness, attachment, attach.withOffset, const_cast<Matrix4x4*>(offset));
    Attached(node, attachment, holder, instance, attach);
    return 1;
}

void AttachLinkedAgents(void* attachments, InstanceContext* instance, u32 withOffset)
{
    AttachmentsNode* node = AsNode(attachments);
    u32 flags = withOffset != 0 ? AttachFlags::WithOffset : 0;
    for (u32 index = 0; index < node->LinkedCount(); index++)
    {
        AttachInstance(node, instance, node->linked[index], flags, nullptr);
    }
}

void AttachLinkedAgentsToSlot(void* attachments, InstanceContext* instance, u32 slot, u32 withOffset)
{
    AttachmentsNode* node = AsNode(attachments);
    u8 exitPoint = static_cast<u8>(slot);
    u32 flags = withOffset != 0 ? AttachFlags::WithOffset : 0;
    for (u32 index = 0; index < node->LinkedCount(); index++)
    {
        HangOnExitPoint(node, instance, node->linked[index], exitPoint, flags, nullptr, 0);
    }
}

u32 UnlinkInstance(void* attachments, InstanceContext* instance, u32 unused, u32 update, u32 force)
{
    AttachmentsNode* node = AsNode(attachments);
    s32 index = IndexOfLinked(node, instance);
    if (index < 0)
    {
        return 0;
    }

    u32 detached = 0;
    if (node->path != nullptr && RemoveInstanceFromPath(node->path, instance, unused, update) != 0)
    {
        instance->collision.leftOut = nullptr;
        node->FreeEmptyPath();
        detached = 1;
    }

    node->UnlinkAt(index, force);
    return detached;
}

InstanceContext* DetachExitPoint(void* attachments, u32 exitPoint, u32, u32 keep)
{
    AttachmentsNode* node = AsNode(attachments);
    if (node->path == nullptr)
    {
        return nullptr;
    }

    InstanceContext* instance = TakeSlotted(node->path, static_cast<u8>(exitPoint));
    if (instance == nullptr)
    {
        return nullptr;
    }

    if (keep != 0)
    {
        instance->collision.leftOut = nullptr;
        return instance;
    }

    node->LetGo(instance);
    return instance;
}

InstanceContext* DetachUnslotted(void* attachments)
{
    AttachmentsNode* node = AsNode(attachments);
    if (node->path == nullptr)
    {
        return nullptr;
    }

    InstanceContext* instance = TakeUnslotted(node->path);
    node->LetGo(instance);
    return instance;
}

InstanceContext* TakeFirstLinked(void* attachments, u32 force)
{
    AttachmentsNode* node = AsNode(attachments);
    InstanceContext* first = node->linked[0];
    return node->UnlinkAt(0, force) != 0 ? first : nullptr;
}

void UnlinkAll(void* attachments)
{
    AttachmentsNode* node = AsNode(attachments);
    if (node->path != nullptr)
    {
        DestroyPath(node->path, DestroyAndFree);
    }

    node->Reset();
}

void ReleaseAttachmentsNode(void* attachments, u32 update, u32 remove, u32 clearsLeftOut)
{
    AttachmentsNode* node = AsNode(attachments);
    if (clearsLeftOut != 0)
    {
        node->owner->collision.leftOut = nullptr;
    }

    if (node->path != nullptr)
    {
        u32 index = 0;
        while (node->path != nullptr && index < node->path->Count())
        {
            Attachment* attachment = node->path->entries[static_cast<u8>(index)];
            InstanceContext* instance = nullptr;
            switch (attachment->bits.kind)
            {
            case Attachment::KindInstance:
                instance = AttachedInstance(attachment);
                if (instance == nullptr)
                {
                    instance = attachment->instance;
                }

                if (instance == nullptr)
                {
                    // Retail bug: the attachment stays, and the loop takes it again for ever
                    node->UnlinkMissing();
                    continue;
                }

                break;
            case Attachment::KindPosition:
            {
                // The AI position left where it hangs, no longer marked
                Vector4 point;
                AttachmentAnchor(attachment, &point);
                AiPosition* position = attachment->position;
                position->position.x = point.x;
                position->position.y = point.y;
                position->position.z = point.z;
                attachment->position->flags.value &= ~g_AttachedPositionFlag;
                RemoveFromPath(node->path, index, 0);
                node->UnlinkMissing();
                continue;
            }
            case Attachment::KindSpring:
                if (attachment->bits.hangs)
                {
                    instance = AttachedInstance(attachment);
                }

                if (instance == nullptr)
                {
                    node->UnlinkMissing();
                    index++;
                    continue;
                }

                break;
            default:
                RemoveFromPath(node->path, index, 1);
                node->UnlinkMissing();
                continue;
            }

            if (UnlinkInstance(node, instance, 0, update, 0) == 0)
            {
                index++;
            }
        }

        node->FreeEmptyPath();
    }

    if (remove != 0 && node->LinkedCount() == 0 && node->flags.listed != 0)
    {
        RemoveNode(node->owner, node);
    }
}

void AttachmentsNode::UnlinkUnkept()
{
    for (u32 index = 0; index < LinkedCount();)
    {
        if (UnlinkAt(index, 0) == 0)
        {
            index++;
        }
    }
}

void DetachAllSprings(void* attachments)
{
    AttachmentsNode* node = AsNode(attachments);
    if (node->path == nullptr)
    {
        return;
    }

    u32 index = 0;
    while (node->path != nullptr && index < node->path->Count())
    {
        Attachment* attachment = node->path->entries[static_cast<u8>(index)];
        InstanceContext* instance = AttachedInstance(attachment);
        if (attachment->bits.follow != Attachment::FollowsSpring)
        {
            // Retail bug: the loop takes an attachment that isn't a spring again for ever
            continue;
        }

        InstanceContext* end = attachment->bits.hangs ? AttachedInstance(attachment) : nullptr;
        if (end != nullptr)
        {
            GameNode* endNode = NodeOf(end, NodeObject);
            CallVirtual<void>(endNode, endNode->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
            CallVirtual<u32>(end, end->vtable, InstanceContext::SleepSlot);
            instance = end;
        }
        else if (instance == nullptr)
        {
            RemoveFromPath(node->path, index, 0);
            node->UnlinkMissing();
            continue;
        }

        if (UnlinkInstance(node, instance, 0, 0, 0) == 0)
        {
            index++;
        }
    }

    node->FreeEmptyPath();
}

void AttachmentsNode::UnlinkMissing()
{
    if (LinkedCount() == 0)
    {
        return;
    }

    u8 index = 0;
    u8 looked = 0;
    bool unlinked = false;
    do
    {
        if (linked[index] == nullptr)
        {
            if (UnlinkAt(index, 0) != 0)
            {
                unlinked = true;
            }
        }
        else
        {
            index++;
        }

        looked++;
    } while (index < LinkedCount() && looked < LinkedCount() && !unlinked);
}

void UnlinkSpawned(void* attachments, u32 sleep)
{
    AttachmentsNode* node = AsNode(attachments);
    u32 index = 0;
    while (index < node->LinkedCount())
    {
        InstanceContext* instance = node->linked[index];
        if (instance == nullptr)
        {
            // Retail bug: the loop takes an empty place again for ever
            continue;
        }

        if (instance->id == -1)
        {
            index++;
            continue;
        }

        node->UnlinkIndex(index);
        if (sleep == 0)
        {
            // Retail bug: the instance moved into the unlinked one's place is passed over
            index++;
            continue;
        }

        GameNode* objectNode = NodeOf(instance, NodeObject);
        if (objectNode != nullptr)
        {
            CallVirtual<void>(objectNode, objectNode->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot);
        }

        CallVirtual<u32>(instance, instance->vtable, InstanceContext::SleepSlot);
    }
}

InstanceContext* AttachmentsNode::UnlinkIndex(u32 index)
{
    InstanceContext* instance = linked[index];
    if (path != nullptr)
    {
        RemoveInstanceFromPath(path, instance, 0, 1);
        LetGo(instance);
        return instance;
    }

    return UnlinkAt(index, 0) != 0 ? instance : nullptr;
}

void SetLastAttachmentMode(void*, u32 mode)
{
    g_LastAttachment->bits.follow = mode;
    g_LastAttachment = nullptr;
}

u32 AttachmentsNode::FollowPath()
{
    if (path == nullptr)
    {
        return 1;
    }

    InstanceContext* lost = UpdatePath(path);
    if (lost == nullptr)
    {
        return 0;
    }

    UnlinkInstance(this, lost, 0, 0, 0);
    return path == nullptr;
}

u32 AttachmentsNode::Update(TimeClock* clock)
{
    if (clock->flags.running != 0 && FollowPath() != 0)
    {
        owner->flags.hasAttachment = 0;
    }

    return GameNode::Update(clock);
}

void* AttachmentsOf(InstanceContext* instance)
{
    void* node = GetGameNode(&instance->nodes, NodeAttachments);
    if (node != nullptr)
    {
        return node;
    }

    AttachmentsNode* made = AttachmentsNode::Construct(static_cast<AttachmentsNode*>(MemoryAllocate(sizeof(AttachmentsNode))));
    RegisterNode(instance, AttachNode, made);
    return made;
}

void ReleaseLinkedInstances(void* attachments, u32 flags)
{
    AttachmentsNode* node = AsNode(attachments);
    for (u8 index = 0; index < node->LinkedCount();)
    {
        if ((node->linkFlags[index].value & flags) != 0)
        {
            node->UnlinkIndex(index);
        }
        else
        {
            index++;
        }
    }
}

u32 AttachmentsNode::GetKind()
{
    return NodeAttachments;
}

u32 AttachmentsNode::GetClassId()
{
    return ClassId;
}
