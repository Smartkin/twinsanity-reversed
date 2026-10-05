#pragma once

#include "abi.h"
#include "common.h"
#include "game/attachment.h"
#include "game/instances.h"
#include "game/math.h"

struct AiPosition;
struct ChunkData;
struct ChunkLinkData;
struct TimeClock;

// An attachments path's bits: how many attachments hang on it
union AttachmentsPathBits
{
    u32 value;
    struct
    {
        u32 count : 5;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(AttachmentsPathBits, 4);

// The attachments hanging on an instance (0x44 bytes): 16 at most, and its bits
struct AttachmentsPath
{
    static constexpr u32 Most = 16;

    Attachment* entries[Most];
    AttachmentsPathBits bits;

    u32 Count() const
    {
        return bits.count;
    }
};
CHECK_SIZE(AttachmentsPath, 0x44);

// An attachments node's bits: the linked instances' count, bit 5 (set when its path is freed, cleared when it gets one, which
// nothing reads) and the linked instance the scripts' linked object commands are at (cleared by its step and its making)
union AttachmentsNodeBits
{
    u32 value;
    struct
    {
        u32 linkedCount : 5;
        // No path hangs the attachments (the scripts' condition reading it clears it)
        u32 noPath : 1;
        u32 unused6 : 1;
        u32 currentLinked : 5;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(AttachmentsNodeBits, 4);

// A linked instance's flags: it stays linked unless it's unlinked by force, and the mark an attach gives it (AttachFlags'
// marksLink: ReleaseLinkedInstances unlinks the marked ones)
union AttachmentLinkFlags
{
    u8 value;
    struct
    {
        u8 kept : 1;
        u8 marked : 1;
        u8 unused2 : 6;
    };

    enum Mask : u8
    {
        Kept = 0x1,
        Marked = 0x2,
    };
};
CHECK_SIZE(AttachmentLinkFlags, 1);

// An instance's attachments node (kind 6, 0x74 bytes, vtable D_002F6010: 2 the destructor, 4 whether its instance may change
// chunks, 7 its step, 8 its update, the rest the base's): its bits, the instances linked to its instance (16 at most) with their
// flags, and the path of what hangs on it; how far attaching pushes an instance from the middle of its holder's box (a motion
// block's stickiness)
struct AttachmentsNode : GameNode
{
    static constexpr u32 ClassId = 0x130A;
    static constexpr u32 MostLinked = 16;
    // The most the count field holds
    static constexpr u32 CountMask = 0x1F;

    AttachmentsNodeBits bits;
    f32 stickiness;
    InstanceContext* linked[MostLinked];
    AttachmentLinkFlags linkFlags[MostLinked];
    AttachmentsPath* path;

    u32 LinkedCount() const
    {
        return bits.linkedCount;
    }

    static AttachmentsNode* Construct(AttachmentsNode* node) RETAIL(FUN_00196030);
    // Everything unlinked and let go, then the base's destructor
    void Destroy(u32 destroyFlags) RETAIL(FUN_00196070);
    // Whether every linked instance went along into the chunk a link leads to (none goes before the linked chunk's RM2 is loaded)
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_001960b8);
    // Its instance starting again: everything let go of but what stays linked, its instance holding and held by nothing, the
    // scripts' current linked instance the first again, and the node itself taken off the instance once nothing stays linked
    void Step(TimeClock* clock, u32 way) RETAIL(FUN_00196168);
    // While the clock runs, what hangs on it follows (its instance holds nothing once its path is gone); the base's update after
    u32 Update(TimeClock* clock) RETAIL(FUN_00197548);
    // Its kind and its class (its vtable's functions 5 and 10)
    u32 GetKind() RETAIL(GetNodeIndex_0019A7F8);
    u32 GetClassId() RETAIL(FUN_0019a800);

    // Nothing linked, no path, no flags
    void Reset() RETAIL(FUN_00196208);
    // A linked instance unlinked by its index, the last one moved into its place (one that stays only by force): whether it was
    u32 UnlinkAt(u32 index, u32 force) RETAIL(FUN_00195ed8);
    // An instance let go: no longer left out of its holder's collisions, the empty path freed, unlinked (not by force): whether it
    // was
    u32 LetGo(InstanceContext* instance) RETAIL(FUN_00195f68);
    // The path freed once nothing hangs on it
    void FreeEmptyPath() RETAIL(FUN_00195fd8);
    // Whether no linked instance stays
    u32 NoneKept() RETAIL(FUN_001963b8);
    // An instance linked and an attachment of it hanging on the holder (no more than 16): the attachment (the last one made)
    Attachment* Add(InstanceContext* holder, InstanceContext* instance) RETAIL(FUN_001965d8);
    // The linked instances that may go unlinked
    void UnlinkUnkept() RETAIL(FUN_001970d8);
    // The first empty place of the linked instances unlinked (one that stays is passed over until as many places as there are
    // links were looked at)
    void UnlinkMissing() RETAIL(FUN_00197290);
    // A linked instance unlinked by its index (off the path too when there's one): the instance, none when it stays linked
    InstanceContext* UnlinkIndex(u32 index) RETAIL(FUN_00197440);
    // What hangs on it follows: whether its path is gone (an instance that lost its holder is unlinked)
    u32 FollowPath() RETAIL(FUN_001974e8);
};
CHECK_OFFSET(AttachmentsNode, bits, 0x18);
CHECK_OFFSET(AttachmentsNode, linked, 0x20);
CHECK_OFFSET(AttachmentsNode, linkFlags, 0x60);
CHECK_SIZE(AttachmentsNode, 0x74);

// How an instance is attached (HangOnExitPoint's and AttachInstance's flags): keeping its matrix to the holder, what hangs there
// taken off first (and launched), and its link marked
union AttachFlags
{
    u32 value;
    struct
    {
        u32 withOffset : 1;
        u32 replaces : 1;
        u32 launchesReplaced : 1;
        u32 marksLink : 1;
        u32 unused4 : 28;
    };

    enum Mask : u32
    {
        WithOffset = 0x1,
        Replaces = 0x2,
        LaunchesReplaced = 0x4,
        MarksLink = 0x8,
    };
};
CHECK_SIZE(AttachFlags, 4);

extern "C"
{
    extern const GccVTableEntry g_AttachmentsNodeVTable[] RETAIL(D_002F6010);
    // The attachment made last (SetLastAttachmentMode sets its mode)
    extern Attachment* g_LastAttachment RETAIL(D_00309B88);
    // The flag of the AI positions something is attached to
    extern u16 g_AttachedPositionFlag RETAIL(D_0030AA9C);

    // Where a spring's end is: the focus's exit point, the point in the focus's space, or the point
    void AttachmentAnchor(Attachment* attachment, Vector4* out) RETAIL(FUN_001937a8);
    // An attachment joining its instance and the holder (its matrix to the holder made from where they are now, their middles
    // meeting the strength's distance along the holder's z), the holder's body slowed down; and one keeping the holder where it is
    void HoldAttachmentOnSpring(Attachment* attachment, f32 strength, f32 stiffness) RETAIL_N32(FUN_00193848);
    void HoldAttachmentInPlace(Attachment* attachment, f32 strength, f32 stiffness) RETAIL_N32(FUN_001939e8);
    // The holder's velocity handed to the instance (its motion's last velocity kept as the start's)
    void HandOnVelocity(Attachment* attachment) RETAIL(FUN_00193960);
    // Its matrix to the holder made from where the instance (or the AI position) and the holder are (the holder's position alone
    // when asked)
    void MakeAttachmentMatrix(Attachment* attachment, u32 positionOnly) RETAIL(FUN_00193b18);
    // An attachment following its holder: whether its instance is still held
    u32 UpdateAttachment(Attachment* attachment) RETAIL(FUN_00193ca8);
    // The hanging and the spring's parts of it, with the holder's matrix
    u32 UpdateHanging(Attachment* attachment, const Matrix4x4* holder) RETAIL(FUN_001947d0);
    u32 UpdateSpring(Attachment* attachment, const Matrix4x4* holder) RETAIL(FUN_00194d98);

    // An attachments path made (empty) and destroyed (what hangs on it destroyed, the path freed when the flags say)
    AttachmentsPath* ConstructPath(AttachmentsPath* path) RETAIL(FUN_00195188);
    void DestroyPath(AttachmentsPath* path, u32 destroyFlags) RETAIL(FUN_001951a0);
    // An attachment added (not an instance on it already, nor a second AI position): whether it was. An instance taking packets
    // stops moving
    u32 AddToPath(AttachmentsPath* path, Attachment* attachment) RETAIL(FUN_001951e8);
    // An instance's index on the path (-1 none) and its attachment
    s32 PathIndexOf(void* path, InstanceContext* instance) RETAIL(FUN_001953a8);
    Attachment* AttachmentOn(AttachmentsPath* path, InstanceContext* instance) RETAIL(FUN_00195400);
    // An attachment taken off by its index (following its holder a last time when asked) and destroyed, and every one
    void RemoveFromPath(AttachmentsPath* path, u32 index, u32 update) RETAIL(FUN_00195450);
    void ClearPath(AttachmentsPath* path) RETAIL(FUN_00195510);
    // The instance hanging on the holder's place, or on an exit point (none when none hangs there)
    InstanceContext* UnslottedAttachment(void* path) RETAIL(FUN_001955a0);
    InstanceContext* SlottedAttachment(void* path, u32 slot) RETAIL(FUN_00195600);
    // The same taken off (following its holder a last time)
    InstanceContext* TakeUnslotted(AttachmentsPath* path) RETAIL(FUN_00195658);
    InstanceContext* TakeSlotted(AttachmentsPath* path, u32 slot) RETAIL(FUN_001956d8);
    // An instance's attachment taken off (following its holder a last time when asked): whether it was on the path
    u32 RemoveInstanceFromPath(AttachmentsPath* path, InstanceContext* instance, u32 unused, u32 update) RETAIL(FUN_00195758);
    // Everything follows its holder: the last instance whose holder let go of it (none)
    InstanceContext* UpdatePath(AttachmentsPath* path) RETAIL(FUN_001957d8);
    // The instances on the path (none for an attachment without one), how many (it doesn't read the limit callers pass)
    s32 AttachedInstances(AttachmentsPath* path, InstanceContext** out, s32 most) RETAIL(FUN_00195870);

    // The instance's attachments, made when it has none
    void* AttachmentsOf(InstanceContext* instance) RETAIL(FUN_001975b0);
    // An instance linked (unless it is; 16 at most), staying when asked: whether it is now; the index of a linked one (-1 none);
    // everything another node links linked too
    u32 LinkInstance(void* attachments, InstanceContext* instance, u32 flag) RETAIL(FUN_00196258);
    s32 IndexOfLinked(void* attachments, InstanceContext* instance) RETAIL(FUN_00195e98);
    void LinkAllOf(void* attachments, void* other) RETAIL(FUN_00196320);
    // An instance unlinked (and taken off the path, following its holder a last time when asked; by force when asked): whether it
    // was on the path. Everything unlinked and let go
    u32 UnlinkInstance(void* attachments, InstanceContext* instance, u32 unused, u32 update, u32 force) RETAIL(FUN_00196cb0);
    void UnlinkAll(void* attachments) RETAIL(FUN_00196e88);
    // The first linked instance unlinked (by force when asked): it, none when it stays
    InstanceContext* TakeFirstLinked(void* attachments, u32 force) RETAIL(FUN_00196e50);
    // The linked instances with an ID unlinked, put to sleep when asked (their object nodes' parts let go first)
    void UnlinkSpawned(void* attachments, u32 sleep) RETAIL(FUN_00197348);
    // An instance linked and hung on the holder's exit point (AttachFlags; placed by a matrix in the holder's space when there's
    // one, which the attach makes the world's in place despite the const its callers give it; linked already when forced), and
    // on the holder's place: whether it was
    u32 HangOnExitPoint(void* attachments, InstanceContext* holder, InstanceContext* instance, u32 exitPoint, u32 flags,
                        const Matrix4x4* offset, u32 force) RETAIL(FUN_00196408);
    u32 AttachInstance(void* attachments, InstanceContext* holder, InstanceContext* instance, u32 flags,
                       const Matrix4x4* offset) RETAIL(FUN_001969b0);
    // Every linked instance hung on the holder's place, or on an exit point (with their offsets when asked)
    void AttachLinkedAgents(void* attachments, InstanceContext* instance, u32 withOffset) RETAIL(FUN_00196b78);
    void AttachLinkedAgentsToSlot(void* attachments, InstanceContext* instance, u32 slot, u32 withOffset) RETAIL(FUN_00196c08);
    // A spring from the holder's joint (0xFF its place) and offset to a point (in the focus's space when there's one, at its
    // joint), its end's instance when there's one; its power, damping and rest length: whether it was attached. Its eleven
    // arguments are more than n32 passes in registers: the asm's go through a thunk of its own
    u32 AttachSpring(f32 power, f32 damping, f32 length, void* attachments, InstanceContext* instance, const Vector4* target,
                     u32 joint, const Vector4* offset, InstanceContext* end, InstanceContext* focus, u32 focusJoint)
        RETAIL_N32(FUN_001966b0);
    // An AI position attached to the holder (moved along with it, marked): whether it was
    u32 AttachToAiPosition(void* attachments, InstanceContext* instance, AiPosition* position) RETAIL(FUN_00196798);
    // An instance linked and held by another on a spring of a strength (unlinked from what held it before), and an instance held
    // where it is by its attachments: whether it was
    u32 HoldOnSpring(f32 strength, f32 stiffness, void* attachments, InstanceContext* holder, InstanceContext* held)
        RETAIL_N32(FUN_001968c0);
    u32 HoldInPlace(f32 strength, f32 stiffness, void* attachments, InstanceContext* held) RETAIL_N32(FUN_00196958);
    // What hangs on an exit point, or on the holder's place, taken off and let go of (left linked, only no longer left out of
    // its holder's collisions, when kept): it, none when nothing hangs there
    InstanceContext* DetachExitPoint(void* attachments, u32 exitPoint, u32 unused, u32 keep) RETAIL(FUN_00196d88);
    InstanceContext* DetachUnslotted(void* attachments) RETAIL(FUN_00196e00);
    // (ReleaseAttachmentsNode, FUN_00196ec8, is game/objectnode.h's)
    // Every spring let go of (its end's instance put to sleep)
    void DetachAllSprings(void* attachments) RETAIL(FUN_00197140);
    // The linked instances with any of the flags unlinked
    void ReleaseLinkedInstances(void* attachments, u32 flags) RETAIL(FUN_00197618);
    // How the last attachment made follows (bits 0-3 of its bits; the attachments unused), the last attachment forgotten
    void SetLastAttachmentMode(void* attachments, u32 mode) RETAIL(FUN_001974c0);
}
