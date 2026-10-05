#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"

struct AiPosition;
struct ExitPointAnimation;
struct InstanceContext;
struct Reference;

// An attachment's bits: how it follows its holder (Attachment::Follow), what it is (Attachment::Kind), whether it keeps its
// matrix to the holder, the holder's exit point it's at (0xFF none), whether a joining instance has met its holder and whether
// the instance hangs from the holder
union AttachmentBits
{
    u64 value;
    struct
    {
        u64 follow : 4;
        u64 kind : 3;
        u64 keepsOffset : 1;
        u64 unused8 : 8;
        u64 exitPoint : 8;
        u64 joined : 1;
        u64 hangs : 1;
        u64 unused26 : 38;
    };
};
CHECK_SIZE(AttachmentBits, 8);

// An instance attached to another, its holder (0xC0 bytes, made with new; the holder's attachments node keeps them on a path): the
// matrix it keeps to the holder (its place in the space of the holder or of the holder's exit point) and that matrix as it was
// taken, a spring's end, the instance (and a reference to it), the AI position it's on, the spring's focus, the holder, the
// holder's exit point's animation and the focus's, the spring's stiffness, damping and length, and its bits
struct Attachment
{
    // How it follows (game/attachments.h's UpdateAttachment): placed at the holder's matrix, moved to its position (an AI
    // position too), hanging from there turned toward it, on a spring, keeping the holder where it is, joining the holder; none
    // past 6
    enum Follow : u32
    {
        FollowsPlace = 0,
        FollowsPosition = 1,
        FollowsHanging = 2,
        FollowsSpring = 4,
        FollowsPinned = 5,
        FollowsJoining = 6,
    };

    // What it is: an instance held, on an AI position, a spring
    enum Kind : u32
    {
        KindInstance = 0,
        KindPosition = 1,
        KindSpring = 2,
    };

    Matrix4x4 offset;
    Matrix4x4 takenOffset;
    Vector4 point;
    InstanceContext* instance;
    Reference* instanceReference;
    AiPosition* position;
    Reference* focus;
    InstanceContext* holder;
    ExitPointAnimation* exitPoint;
    ExitPointAnimation* focusExitPoint;
    f32 stiffness;
    f32 damping;
    f32 length;
    AttachmentBits bits;
};
CHECK_OFFSET(Attachment, point, 0x80);
CHECK_OFFSET(Attachment, instanceReference, 0x94);
CHECK_OFFSET(Attachment, holder, 0xA0);
CHECK_OFFSET(Attachment, bits, 0xB8);
CHECK_SIZE(Attachment, 0xC0);

extern "C"
{
    // Made for an instance a holder holds (no exit point, nothing kept), and for an AI position the holder is on; and its
    // destructor (an instance held stops hanging from anything first)
    Attachment* ConstructAttachment(Attachment* attachment, InstanceContext* holder, InstanceContext* instance) RETAIL(FUN_00192c80);
    Attachment* ConstructPositionAttachment(Attachment* attachment, InstanceContext* holder, AiPosition* position)
        RETAIL(FUN_00192db8);
    void DestroyAttachment(Attachment* attachment, u32 destroyFlags) RETAIL(FUN_00192e30);
    // The instance placed by a matrix in the holder's space (when given), pushed away from the holder's box's middle by a distance
    // (none for 0), and its matrix to the holder kept when asked; and the same at one of the holder's exit points (its index kept)
    void AttachToHolder(f32 push, Attachment* attachment, u32 keepOffset, Matrix4x4* matrix) RETAIL_N32(FUN_00192ef8);
    void AttachAtExitPoint(Attachment* attachment, u32 exitPoint, u32 keepOffset, Matrix4x4* matrix) RETAIL(FUN_00193328);
    // A spring from the instance (an offset from it as its matrix when given) to a point (on a focus's exit point when there's a
    // focus and an exit point, 0xFF none) of a stiffness and a length
    void SetAttachmentSpring(f32 stiffness, f32 length, Attachment* attachment, const Vector4* point, const Vector4* offset,
                             InstanceContext* focus, u32 focusExitPoint) RETAIL_N32(FUN_00193498);
    // The holder's exit point it's at (0xFF none, which leaves its animation as it was)
    void SetAttachmentExitPoint(Attachment* attachment, u32 exitPoint) RETAIL(FUN_00193630);
    // An instance made the one hanging from the holder (its collision leaving the holder out)
    void HangFromHolder(Attachment* attachment, InstanceContext* instance) RETAIL(FUN_001936a0);
}
