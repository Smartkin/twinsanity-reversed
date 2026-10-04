#pragma once

#include "common.h"
#include "game/agentlab.h"
#include "game/behaviours.h"
#include "game/properties.h"

class GameNode;
struct ScriptToken;
struct ScriptTokenList;
struct GameAnimation;
struct OgiAnimator;
struct TimeClock;

// The script commands the builder makes (generated from the retail builder and TT Lab's AgentLabDefsPS2.json, whose names
// they have): the base's bits, next command and vtable, then their arguments as a script has them, which the reader copies
// over the object whole (game/agentlab.h)

// 1, 541
class AddTrailCommand : public ScriptCommand
{
public:
    f32 lifetime;
    u32 flags;
    TaggedValue flags2;
    u32 unused4;
    u32 ids1;
    u32 ids2;
    u32 ids3;
    u32 ids4;
    u32 ids5;
    u32 ids6;
    f32 value11;
    f32 value12;
    f32 value13;
    u32 unused14;
    s32 value15;
    f32 value16;
    s32 value17;
    u32 unused18;
    f32 value19;
    f32 scale;
    f32 value21;
    f32 value22;
    f32 value23;
    f32 value24;
    f32 value25;
    s32 posX;
    s32 posY;
    s32 posZ;
    u32 posW;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd1_AddTrail_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd1_AddTrail_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd1_AddTrail_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd1_AddTrail_Dtor);
    u32 Size() RETAIL(Cmd1_AddTrail_GetSize);
};
CHECK_SIZE(AddTrailCommand, 0x80);

// 2, 542
class ClearTrailCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd2_ClearTrail_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd2_ClearTrail_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd2_ClearTrail_ExecuteOn);
    u32 Size() RETAIL(Cmd2_ClearTrail_GetSize);
};
CHECK_SIZE(ClearTrailCommand, 0xC);

// 3, 105
class PositionWarpCommand : public ScriptCommand
{
public:
    u32 targetAndSpace;
    f32 x;
    f32 y;
    f32 z;
    f32 w;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd3_PositionWarp_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd3_PositionWarp_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd3_PositionWarp_Execute);
    u32 Size() RETAIL(Cmd3_PositionWarp_GetSize);
};
CHECK_SIZE(PositionWarpCommand, 0x20);

// 4
class SetKeyCommand : public ScriptCommand
{
public:
    u32 keyAndFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd4_SetKey_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd4_SetKey_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd4_SetKey_Dtor);
    u32 Size() RETAIL(Cmd4_SetKey_GetSize);
};
CHECK_SIZE(SetKeyCommand, 0x10);

// 5
class NextKeyCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd5_NextKey_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd5_NextKey_Execute);
    u32 Size() RETAIL(Cmd5_NextKey_GetSize);
};
CHECK_SIZE(NextKeyCommand, 0xC);

// 7
class RestartPreviousCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd7_RestartPrevious_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd7_RestartPrevious_Execute);
    u32 Size() RETAIL(Cmd7_RestartPrevious_GetSize);
};
CHECK_SIZE(RestartPreviousCommand, 0xC);

// 8, 112
class SpawnResidentAgentCommand : public ScriptCommand
{
public:
    u32 effect;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    TaggedValue subtype;
    u32 objectAndKey;
    u32 flags;
    TaggedValue flags2;

    static SpawnResidentAgentCommand* Construct(SpawnResidentAgentCommand* command, u32 mode) RETAIL(FUN_00225540);
    void Destroy(u32 destroyFlags) RETAIL(SpawnResidentAgentCommand_dtor);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd8_SpawnResidentAgent_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd8_SpawnResidentAgent_Execute);
    u32 Size() RETAIL(Cmd8_SpawnResidentAgent_GetSize);
};
CHECK_SIZE(SpawnResidentAgentCommand, 0x30);

// 9, 543
class DoAnimationCommand : public ScriptCommand
{
public:
    s32 flags;
    TaggedValue blendTime;
    TaggedValue speed;
    TaggedValue speedRandom;
    f32 startPosition;
    s32 animSlots;

    static DoAnimationCommand* Construct(DoAnimationCommand* command) RETAIL(FUN_00221af8);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(ExecutePlayAnimationCommand);
    // The animation of its slot (one of the first count of its slots, picked at random) played on the node's instance's model
    // with the slot's OGI (the OGI set first, playing nothing when the slot has none), and the instance's shadow given its slot
    // when bit 21 says so
    void ExecuteOn(GameNode* node) RETAIL(Cmd9_DoAnimation_ExecuteOn);
    // An animation played on an animator as the flags say (none: what plays faded out over the blend time)
    void Play(PropertyHolder* properties, GameAnimation* animation, OgiAnimator* animator) RETAIL(SetInstanceAnimation_);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd9_DoAnimation_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd9_DoAnimation_Dtor);
    u32 Size() RETAIL(Cmd9_DoAnimation_GetSize);
};
CHECK_SIZE(DoAnimationCommand, 0x24);

// 10, 544
class DoParticleCommand : public ScriptCommand
{
public:
    u32 systemAndFlags;
    u32 flags2;
    u32 flags3;
    u32 unknown4;
    u32 unknown5;
    u32 posX;
    u32 posY;
    u32 posZ;
    u32 posW;

    // The bits of systemAndFlags: the system (an index into the particle systems' table), the emitter's value, a position taken
    // from a key (cleared once taken), from a frame (with bit 24), the exit point the frame is of (0x3F none); of flags2: the frame
    // turned with the instance, its axes mode, a position given (posX to posW), and a designator whose position it is, a surface's
    // mode and a key of the waypoints (0xFF each: none)
    enum Bits : u32
    {
        SystemMask = 0xFFFF,
        ValueShift = 16,
        ValueMask = 0x7F,
        FromKeyPending = 0x800000,
        FromFrame = 0x1800000,
        ExitPointShift = 25,
        ExitPointMask = 0x3F,
    };

    enum Flags2 : u32
    {
        Turned = 0x1,
        AxesShift = 1,
        AxesMask = 0xF,
        HasPosition = 0x20,
        DesignatorShift = 6,
        SurfaceShift = 14,
        KeyShift = 22,
        NoneByte = 0xFF,
    };

    // A system's emitter started on the node's instance: from a frame (its exit point's or its place's, turned by the axes mode,
    // moved by the position given) it then follows, else at the position (a key's, a designator's, the one given), or at the
    // node's contact for the surface mode with a decal there
    void ExecuteOn(GameNode* node) RETAIL(Cmd10_DoParticle_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd10_DoParticle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd10_DoParticle_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd10_DoParticle_Execute);
    u32 Size() RETAIL(Cmd10_DoParticle_GetSize);
};
CHECK_SIZE(DoParticleCommand, 0x30);

// 11, 545
class DoSoundCommand : public ScriptCommand
{
public:
    // Its flags: how many of its slots it picks from (bits 0-3), the group (8-10), played without a place (11; or followed with
    // the instance, 12 and 23), a random pitch (14), the volume's random part (15), a pitch (16), a volume (17), the camera shaken
    // (18), the contact kind of the node's surface it plays (19-22, 0xF: its slots), the sound kept in the node's tracked sound
    // slot (23)
    enum Flags : u32
    {
        SlotCountMask = 0xF,
        GroupShift = 8,
        GroupMask = 0x7,
        Unplaced = 0x800,
        Followed = 0x801000,
        RandomPitch = 0x4000,
        RandomVolume = 0x8000,
        HasPitch = 0x10000,
        HasVolume = 0x20000,
        Shakes = 0x40000,
        KindShift = 19,
        KindMask = 0x780000,
        Tracked = 0x800000,
    };

    u32 flags;
    u16 soundSlots[8];
    TaggedValue volume;
    f32 unused7;
    f32 pitch;
    f32 pitchRandom;
    f32 volumeRandom;
    u32 unknown11;
    // The camera's shake: its strength, or its strengths across and up, and how it falls off
    f32 shakeStrength;
    f32 shakeAcross;
    f32 shakeUp;
    f32 shakeFalloff;

    // A sound of its slots (one picked at random), or of the node's surface for a kind of contact, played as its flags say, and
    // the camera shaken
    void ExecuteOn(GameNode* node) RETAIL(Cmd11_DoSound_ExecuteOn);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd11_DoSound_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd11_DoSound_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd11_DoSound_Dtor);
    u32 Size() RETAIL(Cmd11_DoSound_GetSize);
};
CHECK_SIZE(DoSoundCommand, 0x48);

// 12
class SetWobbleCommand : public ScriptCommand
{
public:
    u32 amplitudeX;
    u32 amplitudeY;
    u32 amplitudeZ;
    f32 rateX;
    f32 rateY;
    f32 rateZ;
    u32 phaseX;
    u32 phaseY;
    u32 phaseZ;
    f32 value10;
    f32 value11;
    f32 value12;
    u32 keyAndObject;
    u32 unknown14;
    u32 unknown15;
    u32 unused16;
    u32 unused17;
    u32 unknown18;
    u32 unknown19;
    u32 unknown20;
    u32 unknown21;
    u32 unknown22;
    u32 unknown23;
    u32 unknown24;
    u32 unknown25;
    u32 unknown26;
    u32 flags27;
    u32 flags28;
    s32 unused29;
    u32 unused30;
    u32 unused31;
    u32 unused32;
    u32 unknown33;
    u32 unknown34;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd12_SetWobble_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd12_SetWobble_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd12_SetWobble_Dtor);
    u32 Size() RETAIL(Cmd12_SetWobble_GetSize);
};
CHECK_SIZE(SetWobbleCommand, 0x94);

// 13
class ClearWobbleCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd13_ClearWobble_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd13_ClearWobble_Execute);
    u32 Size() RETAIL(Cmd13_ClearWobble_GetSize);
};
CHECK_SIZE(ClearWobbleCommand, 0xC);

// 14
class NowMoveForwardsCommand : public ScriptCommand
{
public:
    TaggedValue distance;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd14_NowMoveForwards_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd14_NowMoveForwards_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd14_NowMoveForwards_Dtor);
    u32 Size() RETAIL(Cmd14_NowMoveForwards_GetSize);
};
CHECK_SIZE(NowMoveForwardsCommand, 0x10);

// 15
class NowMoveBackwardsCommand : public ScriptCommand
{
public:
    TaggedValue distance;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd15_NowMoveBackwards_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd15_NowMoveBackwards_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd15_NowMoveBackwards_Dtor);
    u32 Size() RETAIL(Cmd15_NowMoveBackwards_GetSize);
};
CHECK_SIZE(NowMoveBackwardsCommand, 0x10);

// 16
class NowStrafeLeftCommand : public ScriptCommand
{
public:
    TaggedValue distance;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd16_NowStrafeLeft_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd16_NowStrafeLeft_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd16_NowStrafeLeft_Dtor);
    u32 Size() RETAIL(Cmd16_NowStrafeLeft_GetSize);
};
CHECK_SIZE(NowStrafeLeftCommand, 0x10);

// 17
class NowStrafeRightCommand : public ScriptCommand
{
public:
    TaggedValue distance;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd17_NowStrafeRight_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd17_NowStrafeRight_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd17_NowStrafeRight_Dtor);
    u32 Size() RETAIL(Cmd17_NowStrafeRight_GetSize);
};
CHECK_SIZE(NowStrafeRightCommand, 0x10);

// 18, 19
class NowTurnLeftCommand : public ScriptCommand
{
public:
    TaggedValue angleValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd18_NowTurnLeft_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd18_NowTurnLeft_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd18_NowTurnLeft_Execute);
    u32 Size() RETAIL(Cmd18_NowTurnLeft_GetSize);
};
CHECK_SIZE(NowTurnLeftCommand, 0x10);

// 19's class (its vtable 0x40 bytes past vt_Cmd61_StopMoving's), which the builder never makes: it makes a NowTurnLeftCommand for
// 19 as well
class NowTurnRightCommand : public ScriptCommand
{
public:
    TaggedValue angleValue;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_00215fd0);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(func_00222B50);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002228e0);
    u32 Size() RETAIL(FUN_00222928);
};
CHECK_SIZE(NowTurnRightCommand, 0x10);

// 23, 24
class NowRotateJointCommand : public ScriptCommand
{
public:
    TaggedValue value1;

    void Destroy(u32 destroyFlags) RETAIL(Cmd23_NowRotateJoint_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd23_NowRotateJoint_Execute);
    u32 Size() RETAIL(Cmd23_NowRotateJoint_GetSize);
};
CHECK_SIZE(NowRotateJointCommand, 0x10);

// 27
class StoreCurrentSpaceCommand : public ScriptCommand
{
public:
    u32 targetAndSpace;

    static StoreCurrentSpaceCommand* Construct(StoreCurrentSpaceCommand* command) RETAIL(FUN_00221170);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd27_StoreCurrentSpace_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd27_StoreCurrentSpace_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd27_StoreCurrentSpace_Execute);
    u32 Size() RETAIL(Cmd27_StoreCurrentSpace_GetSize);
};
CHECK_SIZE(StoreCurrentSpaceCommand, 0x10);

// 28, 103
class SetFocusToKeyCommand : public ScriptCommand
{
public:
    u32 keyAndFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd28_SetFocusToKey_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd28_SetFocusToKey_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd28_SetFocusToKey_Execute);
    u32 Size() RETAIL(Cmd28_SetFocusToKey_GetSize);
};
CHECK_SIZE(SetFocusToKeyCommand, 0x10);

// 29
class RotationWarpCommand : public ScriptCommand
{
public:
    u32 targetAndSpace;
    f32 quatX;
    f32 quatY;
    f32 quatZ;
    f32 quatW;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd29_RotationWarp_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd29_RotationWarp_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd29_RotationWarp_Dtor);
    u32 Size() RETAIL(Cmd29_RotationWarp_GetSize);
};
CHECK_SIZE(RotationWarpCommand, 0x20);

// 31
class ClearThreatsCommand : public ScriptCommand
{
public:
    u32 designator;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd31_ClearThreats_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd31_ClearThreats_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd31_ClearThreats_Execute);
    u32 Size() RETAIL(Cmd31_ClearThreats_GetSize);
};
CHECK_SIZE(ClearThreatsCommand, 0x10);

// 33
class TriggerLinkedObjectsCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd33_TriggerLinkedObjects_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd33_TriggerLinkedObjects_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd33_TriggerLinkedObjects_Execute);
    u32 Size() RETAIL(Cmd33_TriggerLinkedObjects_GetSize);
};
CHECK_SIZE(TriggerLinkedObjectsCommand, 0x10);

// 34
class SetStateCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd34_SetState_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd34_SetState_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd34_SetState_Execute);
    u32 Size() RETAIL(Cmd34_SetState_GetSize);
};
CHECK_SIZE(SetStateCommand, 0x10);

// 35
class ToggleStateCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd35_ToggleState_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd35_ToggleState_Execute);
    u32 Size() RETAIL(Cmd35_ToggleState_GetSize);
};
CHECK_SIZE(ToggleStateCommand, 0xC);

// 36
class NextRouteNodeCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd36_NextRouteNode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd36_NextRouteNode_Execute);
    u32 Size() RETAIL(Cmd36_NextRouteNode_GetSize);
};
CHECK_SIZE(NextRouteNodeCommand, 0xC);

// 39
class DiscardRouteCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd39_DiscardRoute_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd39_DiscardRoute_Execute);
    u32 Size() RETAIL(Cmd39_DiscardRoute_GetSize);
};
CHECK_SIZE(DiscardRouteCommand, 0xC);

// 40
class SetLogicalRadiusCommand : public ScriptCommand
{
public:
    u32 value1;
    f32 value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd40_SetLogicalRadius_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd40_SetLogicalRadius_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd40_SetLogicalRadius_Execute);
    u32 Size() RETAIL(Cmd40_SetLogicalRadius_GetSize);
};
CHECK_SIZE(SetLogicalRadiusCommand, 0x14);

// 42
class SetBehaviourPriorityCommand : public ScriptCommand
{
public:
    u32 priorityValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd42_SetBehaviourPriority_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd42_SetBehaviourPriority_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd42_SetBehaviourPriority_Execute);
    u32 Size() RETAIL(Cmd42_SetBehaviourPriority_GetSize);
};
CHECK_SIZE(SetBehaviourPriorityCommand, 0x10);

// 44
class SetCollisionsCommand : public ScriptCommand
{
public:
    f32 unknown1;
    s32 value2;
    s32 value3;
    s32 value4;
    f32 one5;
    u32 flags;
    f32 height;
    f32 value8;
    f32 value9;
    f32 value10;
    f32 value11;
    f32 value12;
    s32 value13;
    f32 value14;
    f32 value15;
    TaggedValue radius;
    u32 unknown17;

    static SetCollisionsCommand* Construct(SetCollisionsCommand* command) RETAIL(FUN_00252ff0);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd44_SetCollisions_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd44_SetCollisions_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd44_SetCollisions_Execute);
    u32 Size() RETAIL(Cmd44_SetCollisions_GetSize);
};
CHECK_SIZE(SetCollisionsCommand, 0x50);

// 45, 90, 91, 104
class SetFocusToAgentCommand : public ScriptCommand
{
public:
    u32 targetAndSlot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd45_SetFocusToAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd45_SetFocusToAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd45_SetFocusToAgent_Execute);
    u32 Size() RETAIL(Cmd45_SetFocusToAgent_GetSize);
};
CHECK_SIZE(SetFocusToAgentCommand, 0x10);

// 47, 64, 94
class AttachFocusObjectCommand : public ScriptCommand
{
public:
    u32 flags;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    u32 rotX;
    u32 rotY;
    u32 rotZ;
    u32 unknown9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd47_AttachFocusObject_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd47_AttachFocusObject_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd47_AttachFocusObject_Dtor);
    u32 Size() RETAIL(Cmd47_AttachFocusObject_GetSize);
};
CHECK_SIZE(AttachFocusObjectCommand, 0x30);

// 48
class DropAttachedObjectCommand : public ScriptCommand
{
public:
    u32 message;
    u32 unused2;
    u32 unused3;
    u32 unused4;
    f32 unused5;
    u32 unused6;
    u32 unused7;
    u32 unused8;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd48_DropAttachedObject_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd48_DropAttachedObject_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd48_DropAttachedObject_Dtor);
    u32 Size() RETAIL(Cmd48_DropAttachedObject_GetSize);
};
CHECK_SIZE(DropAttachedObjectCommand, 0x30);

// 49
class ThrowAttachedObjectCommand : public ScriptCommand
{
public:
    u32 value1;
    f32 x;
    f32 y;
    f32 z;
    u32 unused5;
    TaggedValue radius;
    TaggedValue angleValue;
    f32 value8;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd49_ThrowAttachedObject_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd49_ThrowAttachedObject_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd49_ThrowAttachedObject_Dtor);
    u32 Size() RETAIL(Cmd49_ThrowAttachedObject_GetSize);
};
CHECK_SIZE(ThrowAttachedObjectCommand, 0x30);

// 50
class UnsupportOverFocusCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd50_UnsupportOverFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd50_UnsupportOverFocus_Execute);
    u32 Size() RETAIL(Cmd50_UnsupportOverFocus_GetSize);
};
CHECK_SIZE(UnsupportOverFocusCommand, 0xC);

// 51
class UnsupportAboveCommand : public ScriptCommand
{
public:
    u32 value1;
    f32 x;
    f32 y;
    f32 z;
    u32 value5;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd51_UnsupportAbove_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd51_UnsupportAbove_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd51_UnsupportAbove_Dtor);
    u32 Size() RETAIL(Cmd51_UnsupportAbove_GetSize);
};
CHECK_SIZE(UnsupportAboveCommand, 0x20);

// 52
class ClearFocusCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd52_ClearFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd52_ClearFocus_Execute);
    u32 Size() RETAIL(Cmd52_ClearFocus_GetSize);
};
CHECK_SIZE(ClearFocusCommand, 0xC);

// 53
class ClearCollisionsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd53_ClearCollisions_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd53_ClearCollisions_Execute);
    u32 Size() RETAIL(Cmd53_ClearCollisions_GetSize);
};
CHECK_SIZE(ClearCollisionsCommand, 0xC);

// 54, 65
class SendUserMessageCommand : public ScriptCommand
{
public:
    u32 messageTarget;
    u32 messageFlags;
    TaggedValue value1;
    TaggedValue value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd54_SendUserMessage_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd54_SendUserMessage_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd54_SendUserMessage_Dtor);
    u32 Size() RETAIL(Cmd54_SendUserMessage_GetSize);
};
CHECK_SIZE(SendUserMessageCommand, 0x1C);

// 55
class BroadcastUserMessageCommand : public ScriptCommand
{
public:
    u32 messageTarget;
    f32 radius;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    u32 unknown10;
    u32 unknown11;
    u32 unknown12;
    u32 unknown13;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd55_BroadcastUserMessage_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd55_BroadcastUserMessage_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd55_BroadcastUserMessage_Dtor);
    u32 Size() RETAIL(Cmd55_BroadcastUserMessage_GetSize);
};
CHECK_SIZE(BroadcastUserMessageCommand, 0x40);

// 56
class ClearAnimationCommand : public ScriptCommand
{
public:
    u32 value1;
    TaggedValue value2;

    static ClearAnimationCommand* Construct(ClearAnimationCommand* command) RETAIL(FUN_00221bd0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd56_ClearAnimation_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd56_ClearAnimation_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd56_ClearAnimation_Dtor);
    u32 Size() RETAIL(Cmd56_ClearAnimation_GetSize);
};
CHECK_SIZE(ClearAnimationCommand, 0x14);

// 57
class RequestAttachmentFocusCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd57_RequestAttachmentFocus_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd57_RequestAttachmentFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd57_RequestAttachmentFocus_Execute);
    u32 Size() RETAIL(Cmd57_RequestAttachmentFocus_GetSize);
};
CHECK_SIZE(RequestAttachmentFocusCommand, 0x10);

// 59, 99, 100, 106
class RequestMessengersFocusCommand : public ScriptCommand
{
public:
    u32 value1;

    void Destroy(u32 destroyFlags) RETAIL(Cmd59_RequestMessengersFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd59_RequestMessengersFocus_Execute);
    u32 Size() RETAIL(Cmd59_RequestMessengersFocus_GetSize);
};
CHECK_SIZE(RequestMessengersFocusCommand, 0x10);

// 60, 108
class SetFocusPositionCommand : public ScriptCommand
{
public:
    u32 flags;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    u32 targetFlags;
    u32 angleValue;
    f32 value8;
    f32 scale;
    f32 value10;
    f32 value11;
    f32 value12;
    f32 value13;
    s32 value14;
    s32 value15;
    s32 value16;
    u32 keyAndObject;
    s32 value18;
    u32 unknown19;
    u32 unknown20;
    u32 unknown21;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd60_SetFocusPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd60_SetFocusPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd60_SetFocusPosition_Execute);
    u32 Size() RETAIL(Cmd60_SetFocusPosition_GetSize);
};
CHECK_SIZE(SetFocusPositionCommand, 0x60);

// 61
class StopMovingCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd61_StopMoving_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd61_StopMoving_Execute);
    u32 Size() RETAIL(Cmd61_StopMoving_GetSize);
};
CHECK_SIZE(StopMovingCommand, 0xC);

// 62, 109
class AddNoiseToFocusPositionCommand : public ScriptCommand
{
public:
    u32 flags;
    TaggedValue amount;
    f32 factorA;
    f32 factorB;
    f32 factorC;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd62_AddNoiseToFocusPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd62_AddNoiseToFocusPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd62_AddNoiseToFocusPosition_Execute);
    u32 Size() RETAIL(Cmd62_AddNoiseToFocusPosition_GetSize);
};
CHECK_SIZE(AddNoiseToFocusPositionCommand, 0x20);

// 63, 531
class RestartDefaultBehaviourCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd63_RestartDefaultBehaviour_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd63_RestartDefaultBehaviour_Execute);
    u32 Size() RETAIL(Cmd63_RestartDefaultBehaviour_GetSize);
};
CHECK_SIZE(RestartDefaultBehaviourCommand, 0xC);

// 66
class ClearUserMessageCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd66_ClearUserMessage_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd66_ClearUserMessage_Execute);
    u32 Size() RETAIL(Cmd66_ClearUserMessage_GetSize);
};
CHECK_SIZE(ClearUserMessageCommand, 0xC);

// 67, 101, 102
class RequestMessSourceAsFocusCommand : public ScriptCommand
{
public:
    u32 slot;

    void Destroy(u32 destroyFlags) RETAIL(Cmd67_RequestMessSourceAsFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd67_RequestMessSourceAsFocus_Execute);
    u32 Size() RETAIL(Cmd67_RequestMessSourceAsFocus_GetSize);
};
CHECK_SIZE(RequestMessSourceAsFocusCommand, 0x10);

// 68
class SetCounterCommand : public ScriptCommand
{
public:
    u32 counterTarget;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd68_SetCounter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd68_SetCounter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd68_SetCounter_Execute);
    u32 Size() RETAIL(Cmd68_SetCounter_GetSize);
};
CHECK_SIZE(SetCounterCommand, 0x14);

// 69
class ModifyCounterCommand : public ScriptCommand
{
public:
    u32 counterTarget;
    TaggedValue delta;
    u32 unknown3;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd69_ModifyCounter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd69_ModifyCounter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd69_ModifyCounter_Execute);
    u32 Size() RETAIL(Cmd69_ModifyCounter_GetSize);
};
CHECK_SIZE(ModifyCounterCommand, 0x18);

// 70
class ContinueColliderMotionCommand : public ScriptCommand
{
public:
    TaggedValue value1;
    s32 velX;
    s32 velY;
    s32 velZ;
    u32 unused5;
    s32 value6;
    s32 value7;
    s32 value8;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd70_ContinueColliderMotion_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd70_ContinueColliderMotion_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd70_ContinueColliderMotion_Execute);
    u32 Size() RETAIL(Cmd70_ContinueColliderMotion_GetSize);
};
CHECK_SIZE(ContinueColliderMotionCommand, 0x30);

// 71
class RevertColliderMotionCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd71_RevertColliderMotion_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd71_RevertColliderMotion_Execute);
    u32 Size() RETAIL(Cmd71_RevertColliderMotion_GetSize);
};
CHECK_SIZE(RevertColliderMotionCommand, 0xC);

// 72
class ColliderLaunchNowCommand : public ScriptCommand
{
public:
    f32 unknown1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    u32 flags;
    u32 flags2;
    s32 value8;
    f32 value9;
    s32 value10;
    u32 value11;
    s32 value12;
    s32 value13;
    s32 value14;
    f32 power;
    u32 unknown16;
    u32 unknown17;

    static ColliderLaunchNowCommand* Construct(ColliderLaunchNowCommand* command) RETAIL(FUN_002532d8);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd72_ColliderLaunchNow_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd72_ColliderLaunchNow_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd72_ColliderLaunchNow_Execute);
    u32 Size() RETAIL(Cmd72_ColliderLaunchNow_GetSize);
};
CHECK_SIZE(ColliderLaunchNowCommand, 0x50);

// 74
class ForceAnimationUpdateCommand : public ScriptCommand
{
public:
    TaggedValue value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd74_ForceAnimationUpdate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd74_ForceAnimationUpdate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd74_ForceAnimationUpdate_Execute);
    u32 Size() RETAIL(Cmd74_ForceAnimationUpdate_GetSize);
};
CHECK_SIZE(ForceAnimationUpdateCommand, 0x10);

// 75
class DestroySpawnedAttachmentCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd75_DestroySpawnedAttachment_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd75_DestroySpawnedAttachment_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd75_DestroySpawnedAttachment_Dtor);
    u32 Size() RETAIL(Cmd75_DestroySpawnedAttachment_GetSize);
};
CHECK_SIZE(DestroySpawnedAttachmentCommand, 0x10);

// 76
class ApplyImpulseCommand : public ScriptCommand
{
public:
    u32 value1;
    f32 velX;
    f32 velY;
    f32 velZ;
    f32 value5;
    f32 value6;
    f32 value7;
    f32 value8;
    f32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd76_ApplyImpulse_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd76_ApplyImpulse_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd76_ApplyImpulse_Execute);
    u32 Size() RETAIL(Cmd76_ApplyImpulse_GetSize);
};
CHECK_SIZE(ApplyImpulseCommand, 0x30);

// 77
class RequestDetachCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd77_RequestDetach_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd77_RequestDetach_Execute);
    u32 Size() RETAIL(Cmd77_RequestDetach_GetSize);
};
CHECK_SIZE(RequestDetachCommand, 0xC);

// 78
class SetObjectCommand : public ScriptCommand
{
public:
    u32 flags;
    u32 flags2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd78_SetObject_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd78_SetObject_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(ExecuteCommand_0x4E);
    void ExecuteOn(GameNode* node) RETAIL(Cmd78_SetObject_ExecuteOn);
    u32 Size() RETAIL(GetScriptCommandSize);
};
CHECK_SIZE(SetObjectCommand, 0x14);

// 79
class KeepCommand : public ScriptCommand
{
public:
    TaggedValue flags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd79_Keep_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd79_Keep_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd79_Keep_Execute);
    u32 Size() RETAIL(Cmd79_Keep_GetSize);
};
CHECK_SIZE(KeepCommand, 0x10);

// 80
class AttachSpringCommand : public ScriptCommand
{
public:
    f32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 value5;
    f32 x;
    f32 y;
    f32 z;
    f32 value9;
    TaggedValue offsetX2;
    u32 value11;
    f32 value12;
    f32 value13;
    f32 value14;
    u32 unused15;
    u32 unused16;
    f32 unused17;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd80_AttachSpring_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd80_AttachSpring_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd80_AttachSpring_Dtor);
    u32 Size() RETAIL(Cmd80_AttachSpring_GetSize);
};
CHECK_SIZE(AttachSpringCommand, 0x50);

// 81
class DetachAllSpringsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd81_DetachAllSprings_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd81_DetachAllSprings_Execute);
    u32 Size() RETAIL(Cmd81_DetachAllSprings_GetSize);
};
CHECK_SIZE(DetachAllSpringsCommand, 0xC);

// 82, 83
class SetContactSpringyCommand : public ScriptCommand
{
public:
    f32 drag;
    s32 mode1;
    s32 hitType;
    s32 gravity;
    s32 friction;
    s32 rollingFriction;
    s32 bounce;
    s32 maxSpeed;
    f32 value8;
    f32 value9;
    f32 value10;
    u32 unused11;
    s32 offsetX;
    s32 offsetY;
    s32 offsetZ;
    s32 value15;
    s32 value16;
    f32 value17;
    s32 value18;
    u32 unused19;
    u32 unused20;
    u32 unused21;
    f32 one22;
    TaggedValue motionFlags;
    s32 value24;
    s32 value25;
    u32 motionType;
    u32 motionFlags2;
    u32 unused29;
    u32 unused30;
    u32 unused31;
    u32 id;
    u32 unused33;
    u32 unused34;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd82_SetContactSpringy_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd82_SetContactSpringy_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd82_SetContactSpringy_Dtor);
    u32 Size() RETAIL(Cmd82_SetContactSpringy_GetSize);
};
CHECK_SIZE(SetContactSpringyCommand, 0x94);

// 84
class ClearContactResponseCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd84_ClearContactResponse_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd84_ClearContactResponse_Execute);
    u32 Size() RETAIL(Cmd84_ClearContactResponse_GetSize);
};
CHECK_SIZE(ClearContactResponseCommand, 0xC);

// 85
class DestroyMeCommand : public ScriptCommand
{
public:
    u32 mode;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd85_DestroyMe_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd85_DestroyMe_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(ExecuteCommand_Destroy);
    u32 Size() RETAIL(Cmd85_DestroyMe_GetSize);
};
CHECK_SIZE(DestroyMeCommand, 0x10);

// 86
class SetSoundCommand : public ScriptCommand
{
public:
    u32 value1;
    f32 value2;
    f32 value3;
    f32 value4;
    u32 unused5;
    u32 unused6;
    u32 value7;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd86_SetSound_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd86_SetSound_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd86_SetSound_Execute);
    u32 Size() RETAIL(Cmd86_SetSound_GetSize);
};
CHECK_SIZE(SetSoundCommand, 0x28);

// 87
class AlterWobblePhaseCommand : public ScriptCommand
{
public:
    TaggedValue value1;
    TaggedValue value2;
    TaggedValue value3;
    TaggedValue value4;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd87_AlterWobblePhase_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd87_AlterWobblePhase_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd87_AlterWobblePhase_Dtor);
    u32 Size() RETAIL(Cmd87_AlterWobblePhase_GetSize);
};
CHECK_SIZE(AlterWobblePhaseCommand, 0x1C);

// 88
class BeginMusicCommand : public ScriptCommand
{
public:
    TaggedValue track;
    // Bits 12-14 the music slot (3: played where the instance is, heard to the range), bit 15 it loops
    u32 flags;
    f32 volume;
    f32 fadeTime;
    f32 range;

    // A track played in a music slot (the main slot's volume the track's), or by the instance where it is
    void ExecuteOn(GameNode* node) RETAIL(Cmd88_BeginMusic_ExecuteOn);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd88_BeginMusic_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd88_BeginMusic_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd88_BeginMusic_Dtor);
    u32 Size() RETAIL(Cmd88_BeginMusic_GetSize);
};
CHECK_SIZE(BeginMusicCommand, 0x20);

// 89
class EndContextMusicCommand : public ScriptCommand
{
public:
    f32 time;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd89_EndContextMusic_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd89_EndContextMusic_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd89_EndContextMusic_Execute);
    u32 Size() RETAIL(Cmd89_EndContextMusic_GetSize);
};
CHECK_SIZE(EndContextMusicCommand, 0x10);

// 92, 93, 585
class AddLivesCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd92_AddLives_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd92_AddLives_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd92_AddLives_Execute);
    u32 Size() RETAIL(Cmd92_AddLives_GetSize);
};
CHECK_SIZE(AddLivesCommand, 0x10);

// 95
class ReleaseAgentRef2Command : public ScriptCommand
{
public:
    u32 event;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd95_ReleaseAgentRef2_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd95_ReleaseAgentRef2_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd95_ReleaseAgentRef2_Dtor);
    u32 Size() RETAIL(Cmd95_ReleaseAgentRef2_GetSize);
};
CHECK_SIZE(ReleaseAgentRef2Command, 0x10);

// 96
class LaunchAgentRef2Command : public ScriptCommand
{
public:
    u32 unused;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    f32 motion0;
    s32 motion1;
    f32 motion2;
    f32 gravity;
    f32 motion4;
    s32 motion5;
    f32 angleX;
    s32 angleY;
    s32 angleZ;
    s32 motion9;
    s32 motion10;
    u32 motion11;
    s32 motionDesignator;
    s32 motion13;
    f32 motion14;
    s32 motion15;
    s32 motion16;
    f32 motion17;
    s32 motion18;
    u32 motion19;
    u32 motion20;
    u32 motion21;
    f32 motion22;
    TaggedValue motionFlags;
    s32 motionFlags2;
    s32 motion25;
    u32 motion26;
    f32 motion27;
    s32 motion28;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    u32 launch;
    TaggedValue launchFlags;
    TaggedValue timeOrDistance;
    f32 scaleFactor;
    f32 height;
    f32 value44;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd96_LaunchAgentRef2_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd96_LaunchAgentRef2_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd96_LaunchAgentRef2_Dtor);
    u32 Size() RETAIL(Cmd96_LaunchAgentRef2_GetSize);
};
CHECK_SIZE(LaunchAgentRef2Command, 0xC0);

// 97
class ClearAgentRef1Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd97_ClearAgentRef1_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd97_ClearAgentRef1_Execute);
    u32 Size() RETAIL(Cmd97_ClearAgentRef1_GetSize);
};
CHECK_SIZE(ClearAgentRef1Command, 0xC);

// 98
class ClearAgentRef2Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd98_ClearAgentRef2_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd98_ClearAgentRef2_Execute);
    u32 Size() RETAIL(Cmd98_ClearAgentRef2_GetSize);
};
CHECK_SIZE(ClearAgentRef2Command, 0xC);

// 110
class ClearFocusPositionCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd110_ClearFocusPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd110_ClearFocusPosition_Execute);
    u32 Size() RETAIL(Cmd110_ClearFocusPosition_GetSize);
};
CHECK_SIZE(ClearFocusPositionCommand, 0xC);

// 111
class CacheLinkedInstanceCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd111_CacheLinkedInstance_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd111_CacheLinkedInstance_Execute);
    u32 Size() RETAIL(Cmd111_CacheLinkedInstance_GetSize);
};
CHECK_SIZE(CacheLinkedInstanceCommand, 0xC);

// 113
// Which angles it sets (bits 0-2), then the angles (TT Lab's names are an argument off: rotX, rotY, rotZ, set)
class SetRotationComponentsCommand : public ScriptCommand
{
public:
    u32 components;
    f32 x;
    f32 y;
    f32 z;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd113_SetRotationComponents_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd113_SetRotationComponents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd113_SetRotationComponents_Execute);
    u32 Size() RETAIL(Cmd113_SetRotationComponents_GetSize);
};
CHECK_SIZE(SetRotationComponentsCommand, 0x1C);

// 114
class StopHeadTrackingCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd114_StopHeadTracking_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd114_StopHeadTracking_Execute);
    u32 Size() RETAIL(Cmd114_StopHeadTracking_GetSize);
};
CHECK_SIZE(StopHeadTrackingCommand, 0xC);

// 115
class StartHeadTrackingCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd115_StartHeadTracking_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd115_StartHeadTracking_Execute);
    u32 Size() RETAIL(Cmd115_StartHeadTracking_GetSize);
};
CHECK_SIZE(StartHeadTrackingCommand, 0xC);

// 116
class DestroyHeadTrackingCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd116_DestroyHeadTracking_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd116_DestroyHeadTracking_Execute);
    u32 Size() RETAIL(Cmd116_DestroyHeadTracking_GetSize);
};
CHECK_SIZE(DestroyHeadTrackingCommand, 0xC);

// 117
class MakeNoiseCommand : public ScriptCommand
{
public:
    f32 radius;
    f32 loudness;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd117_MakeNoise_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd117_MakeNoise_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd117_MakeNoise_Execute);
    u32 Size() RETAIL(Cmd117_MakeNoise_GetSize);
};
CHECK_SIZE(MakeNoiseCommand, 0x14);

// 118
class SetHeadTrackingTargetCommand : public ScriptCommand
{
public:
    u32 target;
    f32 weightValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd118_SetHeadTrackingTarget_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd118_SetHeadTrackingTarget_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd118_SetHeadTrackingTarget_Dtor);
    u32 Size() RETAIL(Cmd118_SetHeadTrackingTarget_GetSize);
};
CHECK_SIZE(SetHeadTrackingTargetCommand, 0x14);

// 119
class ClearNodeByte154Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd119_ClearNodeByte154_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd119_ClearNodeByte154_Execute);
    u32 Size() RETAIL(Cmd119_ClearNodeByte154_GetSize);
};
CHECK_SIZE(ClearNodeByte154Command, 0xC);

// 120
class PhysicsResetVelocityCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd120_PhysicsResetVelocity_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd120_PhysicsResetVelocity_Execute);
    u32 Size() RETAIL(Cmd120_PhysicsResetVelocity_GetSize);
};
CHECK_SIZE(PhysicsResetVelocityCommand, 0xC);

// 121
class SetFocusPositionBesidePlayerCommand : public ScriptCommand
{
public:
    f32 distance;
    u32 unused;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd121_SetFocusPositionBesidePlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd121_SetFocusPositionBesidePlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd121_SetFocusPositionBesidePlayer_Execute);
    u32 Size() RETAIL(Cmd121_SetFocusPositionBesidePlayer_GetSize);
};
CHECK_SIZE(SetFocusPositionBesidePlayerCommand, 0x14);

// 122
class SetFocusPositionToAgentCommand : public ScriptCommand
{
public:
    u32 targets;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd122_SetFocusPositionToAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd122_SetFocusPositionToAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd122_SetFocusPositionToAgent_Execute);
    u32 Size() RETAIL(Cmd122_SetFocusPositionToAgent_GetSize);
};
CHECK_SIZE(SetFocusPositionToAgentCommand, 0x10);

// 123
class LinkToNearestPointCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd123_LinkToNearestPoint_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd123_LinkToNearestPoint_Execute);
    u32 Size() RETAIL(Cmd123_LinkToNearestPoint_GetSize);
};
CHECK_SIZE(LinkToNearestPointCommand, 0xC);

// 124
class RunScriptSlotCommand : public ScriptCommand
{
public:
    u32 slotAndFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd124_RunScriptSlot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd124_RunScriptSlot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd124_RunScriptSlot_Execute);
    u32 Size() RETAIL(Cmd124_RunScriptSlot_GetSize);
};
CHECK_SIZE(RunScriptSlotCommand, 0x10);

// 125
class OffsetFocusPositionCommand : public ScriptCommand
{
public:
    u32 unused;
    f32 x;
    f32 y;
    f32 z;
    u32 w;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd125_OffsetFocusPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd125_OffsetFocusPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd125_OffsetFocusPosition_Execute);
    u32 Size() RETAIL(Cmd125_OffsetFocusPosition_GetSize);
};
CHECK_SIZE(OffsetFocusPositionCommand, 0x20);

// 126
class NextLinkedObjectCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd126_NextLinkedObject_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd126_NextLinkedObject_Execute);
    u32 Size() RETAIL(Cmd126_NextLinkedObject_GetSize);
};
CHECK_SIZE(NextLinkedObjectCommand, 0xC);

// 127
class PhysicsSetGravityCommand : public ScriptCommand
{
public:
    f32 gravity;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd127_PhysicsSetGravity_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd127_PhysicsSetGravity_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd127_PhysicsSetGravity_Execute);
    u32 Size() RETAIL(Cmd127_PhysicsSetGravity_GetSize);
};
CHECK_SIZE(PhysicsSetGravityCommand, 0x10);

// 128
class PhysicsBodyResetCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd128_PhysicsBodyReset_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd128_PhysicsBodyReset_Execute);
    u32 Size() RETAIL(Cmd128_PhysicsBodyReset_GetSize);
};
CHECK_SIZE(PhysicsBodyResetCommand, 0xC);

// 129
class PhysicsBodyActivateCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd129_PhysicsBodyActivate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd129_PhysicsBodyActivate_Execute);
    u32 Size() RETAIL(Cmd129_PhysicsBodyActivate_GetSize);
};
CHECK_SIZE(PhysicsBodyActivateCommand, 0xC);

// 130
class SetPhysicsSizesCommand : public ScriptCommand
{
public:
    f32 size;
    f32 size2;
    u32 mode1;
    TaggedValue mode2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd130_SetPhysicsSizes_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd130_SetPhysicsSizes_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd130_SetPhysicsSizes_Execute);
    u32 Size() RETAIL(Cmd130_SetPhysicsSizes_GetSize);
};
CHECK_SIZE(SetPhysicsSizesCommand, 0x1C);

// 131
class MagnetPullToFocusCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd131_MagnetPullToFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd131_MagnetPullToFocus_Execute);
    u32 Size() RETAIL(Cmd131_MagnetPullToFocus_GetSize);
};
CHECK_SIZE(MagnetPullToFocusCommand, 0xC);

// 132
class SetLinkedObjectIndexCommand : public ScriptCommand
{
public:
    s32 number;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd132_SetLinkedObjectIndex_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd132_SetLinkedObjectIndex_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd132_SetLinkedObjectIndex_Execute);
    u32 Size() RETAIL(Cmd132_SetLinkedObjectIndex_GetSize);
};
CHECK_SIZE(SetLinkedObjectIndexCommand, 0x10);

// 133
class ClearObjectContextTargetCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd133_ClearObjectContextTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd133_ClearObjectContextTarget_Execute);
    u32 Size() RETAIL(Cmd133_ClearObjectContextTarget_GetSize);
};
CHECK_SIZE(ClearObjectContextTargetCommand, 0xC);

// 134
class SetFocusToOriginatorCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd134_SetFocusToOriginator_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd134_SetFocusToOriginator_Execute);
    u32 Size() RETAIL(Cmd134_SetFocusToOriginator_GetSize);
};
CHECK_SIZE(SetFocusToOriginatorCommand, 0xC);

// 135
class DestroyPerceptionsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd135_DestroyPerceptions_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd135_DestroyPerceptions_Execute);
    u32 Size() RETAIL(Cmd135_DestroyPerceptions_GetSize);
};
CHECK_SIZE(DestroyPerceptionsCommand, 0xC);

// 136
class SetPerceptionWeightCommand : public ScriptCommand
{
public:
    TaggedValue slot;
    s32 weightValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd136_SetPerceptionWeight_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd136_SetPerceptionWeight_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd136_SetPerceptionWeight_Execute);
    u32 Size() RETAIL(Cmd136_SetPerceptionWeight_GetSize);
};
CHECK_SIZE(SetPerceptionWeightCommand, 0x14);

// 137
class AddPerceptionWeightCommand : public ScriptCommand
{
public:
    TaggedValue slot;
    s32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd137_AddPerceptionWeight_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd137_AddPerceptionWeight_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd137_AddPerceptionWeight_Execute);
    u32 Size() RETAIL(Cmd137_AddPerceptionWeight_GetSize);
};
CHECK_SIZE(AddPerceptionWeightCommand, 0x14);

// 138
class PushFromPerceptionCommand : public ScriptCommand
{
public:
    TaggedValue mode;
    f32 factor;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd138_PushFromPerception_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd138_PushFromPerception_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd138_PushFromPerception_Dtor);
    u32 Size() RETAIL(Cmd138_PushFromPerception_GetSize);
};
CHECK_SIZE(PushFromPerceptionCommand, 0x14);

// 139
class SetCharacterAnalogCommand : public ScriptCommand
{
public:
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd139_SetCharacterAnalog_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd139_SetCharacterAnalog_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd139_SetCharacterAnalog_Execute);
    u32 Size() RETAIL(Cmd139_SetCharacterAnalog_GetSize);
};
CHECK_SIZE(SetCharacterAnalogCommand, 0x10);

// 140
class AddCharacterAnalogCommand : public ScriptCommand
{
public:
    f32 delta;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd140_AddCharacterAnalog_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd140_AddCharacterAnalog_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd140_AddCharacterAnalog_Execute);
    u32 Size() RETAIL(Cmd140_AddCharacterAnalog_GetSize);
};
CHECK_SIZE(AddCharacterAnalogCommand, 0x10);

// 141
class PerceptionOp141Command : public ScriptCommand
{
public:
    TaggedValue slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd141_PerceptionOp141_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd141_PerceptionOp141_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd141_PerceptionOp141_Execute);
    u32 Size() RETAIL(Cmd141_PerceptionOp141_GetSize);
};
CHECK_SIZE(PerceptionOp141Command, 0x10);

// 142
class PerceptionOp142Command : public ScriptCommand
{
public:
    TaggedValue slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd142_PerceptionOp142_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd142_PerceptionOp142_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd142_PerceptionOp142_Execute);
    u32 Size() RETAIL(Cmd142_PerceptionOp142_GetSize);
};
CHECK_SIZE(PerceptionOp142Command, 0x10);

// 143
class DisableAllPerceptionsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd143_DisableAllPerceptions_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd143_DisableAllPerceptions_Execute);
    u32 Size() RETAIL(Cmd143_DisableAllPerceptions_GetSize);
};
CHECK_SIZE(DisableAllPerceptionsCommand, 0xC);

// 144
class EnableAllPerceptionsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd144_EnableAllPerceptions_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd144_EnableAllPerceptions_Execute);
    u32 Size() RETAIL(Cmd144_EnableAllPerceptions_GetSize);
};
CHECK_SIZE(EnableAllPerceptionsCommand, 0xC);

// 145
class SetParentExecutionValueCommand : public ScriptCommand
{
public:
    u32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd145_SetParentExecutionValue_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd145_SetParentExecutionValue_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd145_SetParentExecutionValue_Execute);
    u32 Size() RETAIL(Cmd145_SetParentExecutionValue_GetSize);
};
CHECK_SIZE(SetParentExecutionValueCommand, 0x10);

// 146, 154, 155
class SetFocusToLinkedObjectCommand : public ScriptCommand
{
public:
    u32 target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd146_SetFocusToLinkedObject_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd146_SetFocusToLinkedObject_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd146_SetFocusToLinkedObject_Execute);
    u32 Size() RETAIL(Cmd146_SetFocusToLinkedObject_GetSize);
};
CHECK_SIZE(SetFocusToLinkedObjectCommand, 0x10);

// 147
class PreviousKeyCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd147_PreviousKey_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd147_PreviousKey_Execute);
    u32 Size() RETAIL(Cmd147_PreviousKey_GetSize);
};
CHECK_SIZE(PreviousKeyCommand, 0xC);

// 148
class SetMotionFloatsCommand : public ScriptCommand
{
public:
    TaggedValue a;
    TaggedValue b;
    TaggedValue c;
    TaggedValue set;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd148_SetMotionFloats_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd148_SetMotionFloats_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd148_SetMotionFloats_Dtor);
    u32 Size() RETAIL(Cmd148_SetMotionFloats_GetSize);
};
CHECK_SIZE(SetMotionFloatsCommand, 0x1C);

// 149
class RotateWithLinkedCommand : public ScriptCommand
{
public:
    f32 degreesPerSecond;
    TaggedValue axes;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd149_RotateWithLinked_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd149_RotateWithLinked_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd149_RotateWithLinked_Dtor);
    u32 Size() RETAIL(Cmd149_RotateWithLinked_GetSize);
};
CHECK_SIZE(RotateWithLinkedCommand, 0x14);

// 150
class StrafeTowardsTargetCommand : public ScriptCommand
{
public:
    u32 target;
    f32 speed;
    f32 maxDistance;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd150_StrafeTowardsTarget_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd150_StrafeTowardsTarget_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd150_StrafeTowardsTarget_Dtor);
    u32 Size() RETAIL(Cmd150_StrafeTowardsTarget_GetSize);
};
CHECK_SIZE(StrafeTowardsTargetCommand, 0x18);

// 151
class NextKeyOfPath34Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd151_NextKeyOfPath34_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd151_NextKeyOfPath34_Execute);
    u32 Size() RETAIL(Cmd151_NextKeyOfPath34_GetSize);
};
CHECK_SIZE(NextKeyOfPath34Command, 0xC);

// 152
class AddMotionAnglesCommand : public ScriptCommand
{
public:
    TaggedValue x;
    TaggedValue y;
    TaggedValue z;
    TaggedValue set;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd152_AddMotionAngles_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd152_AddMotionAngles_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd152_AddMotionAngles_Dtor);
    u32 Size() RETAIL(Cmd152_AddMotionAngles_GetSize);
};
CHECK_SIZE(AddMotionAnglesCommand, 0x1C);

// 153
class MoveTowardsDesignatorCommand : public ScriptCommand
{
public:
    u32 target;
    s32 distance;
    f32 value2;
    s32 value3;
    s32 value4;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd153_MoveTowardsDesignator_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd153_MoveTowardsDesignator_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd153_MoveTowardsDesignator_Dtor);
    u32 Size() RETAIL(Cmd153_MoveTowardsDesignator_GetSize);
};
CHECK_SIZE(MoveTowardsDesignatorCommand, 0x20);

// 156
class SetNode150FieldsCommand : public ScriptCommand
{
public:
    u32 values;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd156_SetNode150Fields_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd156_SetNode150Fields_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd156_SetNode150Fields_Execute);
    u32 Size() RETAIL(Cmd156_SetNode150Fields_GetSize);
};
CHECK_SIZE(SetNode150FieldsCommand, 0x10);

// 157
class UnlinkTargetCommand : public ScriptCommand
{
public:
    TaggedValue target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd157_UnlinkTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd157_UnlinkTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd157_UnlinkTarget_Execute);
    u32 Size() RETAIL(Cmd157_UnlinkTarget_GetSize);
};
CHECK_SIZE(UnlinkTargetCommand, 0x10);

// 158
class AttachMotionBlockCommand : public ScriptCommand
{
public:
    s32 id;
    u32 block1;
    u32 block2;
    u32 block3;
    u32 block4;
    u32 block5;
    u32 block6;
    u32 block7;
    u32 block8;
    u32 block9;
    u32 block10;
    u32 block11;
    u32 block12;
    u32 block13;
    u32 block14;
    u32 block15;
    u32 block16;
    u32 block17;
    u32 block18;
    u32 block19;
    u32 block20;
    u32 block21;
    u32 block22;
    u32 block23;
    u32 block24;
    u32 block25;
    u32 block26;
    u32 block27;
    u32 block28;
    u32 block29;
    u32 block30;
    u32 block31;
    u32 block32;
    u32 block33;
    u32 block34;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd158_AttachMotionBlock_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd158_AttachMotionBlock_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd158_AttachMotionBlock_Dtor);
    u32 Size() RETAIL(Cmd158_AttachMotionBlock_GetSize);
};
CHECK_SIZE(AttachMotionBlockCommand, 0x98);

// 159
class ResetNode120Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd159_ResetNode120_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd159_ResetNode120_Execute);
    u32 Size() RETAIL(Cmd159_ResetNode120_GetSize);
};
CHECK_SIZE(ResetNode120Command, 0xC);

// 160
class ClearMotionBlockFlag16Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd160_ClearMotionBlockFlag16_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd160_ClearMotionBlockFlag16_Execute);
    u32 Size() RETAIL(Cmd160_ClearMotionBlockFlag16_GetSize);
};
CHECK_SIZE(ClearMotionBlockFlag16Command, 0xC);

// 161
class SetMotionBlockFlag16Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd161_SetMotionBlockFlag16_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd161_SetMotionBlockFlag16_Execute);
    u32 Size() RETAIL(Cmd161_SetMotionBlockFlag16_GetSize);
};
CHECK_SIZE(SetMotionBlockFlag16Command, 0xC);

// 162
class AddToFocusObjectByteCommand : public ScriptCommand
{
public:
    u32 index;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd162_AddToFocusObjectByte_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd162_AddToFocusObjectByte_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd162_AddToFocusObjectByte_Execute);
    u32 Size() RETAIL(Cmd162_AddToFocusObjectByte_GetSize);
};
CHECK_SIZE(AddToFocusObjectByteCommand, 0x14);

// 163
class UnlinkFromTargetCommand : public ScriptCommand
{
public:
    u32 target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd163_UnlinkFromTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd163_UnlinkFromTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd163_UnlinkFromTarget_Execute);
    u32 Size() RETAIL(Cmd163_UnlinkFromTarget_GetSize);
};
CHECK_SIZE(UnlinkFromTargetCommand, 0x10);

// 164
class AddToLinkedObjectsByteCommand : public ScriptCommand
{
public:
    u32 filter;
    u32 target;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd164_AddToLinkedObjectsByte_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd164_AddToLinkedObjectsByte_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd164_AddToLinkedObjectsByte_Dtor);
    u32 Size() RETAIL(Cmd164_AddToLinkedObjectsByte_GetSize);
};
CHECK_SIZE(AddToLinkedObjectsByteCommand, 0x18);

// 165
class ForceVolumeControllerCommand : public ScriptCommand
{
public:
    TaggedValue x;
    TaggedValue y;
    TaggedValue z;
    u32 unused4;
    u32 unused5;
    s32 value6;
    u32 value7;
    u32 value8;
    u32 value9;
    TaggedValue value10;
    u32 unused11;
    u32 unused12;
    u32 unused13;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd165_ForceVolumeController_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd165_ForceVolumeController_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd165_ForceVolumeController_Dtor);
    u32 Size() RETAIL(Cmd165_ForceVolumeController_GetSize);
};
CHECK_SIZE(ForceVolumeControllerCommand, 0x40);

// 166
class NotifyInstancesWithinCommand : public ScriptCommand
{
public:
    TaggedValue radius;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd166_NotifyInstancesWithin_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd166_NotifyInstancesWithin_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd166_NotifyInstancesWithin_Dtor);
    u32 Size() RETAIL(Cmd166_NotifyInstancesWithin_GetSize);
};
CHECK_SIZE(NotifyInstancesWithinCommand, 0x10);

// 167
class SetSurfaceCommand : public ScriptCommand
{
public:
    u32 surface;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd167_SetSurface_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd167_SetSurface_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd167_SetSurface_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd167_SetSurface_ExecuteOn);
    u32 Size() RETAIL(Cmd167_SetSurface_GetSize);
};
CHECK_SIZE(SetSurfaceCommand, 0x10);

// 168
class MoveInstancesInBoxCommand : public ScriptCommand
{
public:
    u32 unused;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 size;
    f32 value5;
    f32 value6;
    u32 flags;
    u32 unused2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd168_MoveInstancesInBox_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd168_MoveInstancesInBox_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd168_MoveInstancesInBox_Execute);
    u32 Size() RETAIL(Cmd168_MoveInstancesInBox_GetSize);
};
CHECK_SIZE(MoveInstancesInBoxCommand, 0x30);

// 169
class SetFocusPositionAlongCommand : public ScriptCommand
{
public:
    u32 targets;
    f32 distance;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd169_SetFocusPositionAlong_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd169_SetFocusPositionAlong_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd169_SetFocusPositionAlong_Execute);
    u32 Size() RETAIL(Cmd169_SetFocusPositionAlong_GetSize);
};
CHECK_SIZE(SetFocusPositionAlongCommand, 0x14);

// 170
class SetFocusObjectByteCommand : public ScriptCommand
{
public:
    u32 index;
    s32 source;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd170_SetFocusObjectByte_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd170_SetFocusObjectByte_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd170_SetFocusObjectByte_Execute);
    u32 Size() RETAIL(Cmd170_SetFocusObjectByte_GetSize);
};
CHECK_SIZE(SetFocusObjectByteCommand, 0x18);

// 171
class RunSlotBehaviourOnLinkedCommand : public ScriptCommand
{
public:
    u32 targetAndObject;
    u32 slotAndFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd171_RunSlotBehaviourOnLinked_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd171_RunSlotBehaviourOnLinked_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd171_RunSlotBehaviourOnLinked_Execute);
    u32 Size() RETAIL(Cmd171_RunSlotBehaviourOnLinked_GetSize);
};
CHECK_SIZE(RunSlotBehaviourOnLinkedCommand, 0x14);

// 172
class StopTargetBehaviourCommand : public ScriptCommand
{
public:
    u32 targetAndFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd172_StopTargetBehaviour_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd172_StopTargetBehaviour_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd172_StopTargetBehaviour_Execute);
    u32 Size() RETAIL(Cmd172_StopTargetBehaviour_GetSize);
};
CHECK_SIZE(StopTargetBehaviourCommand, 0x10);

// 173
class SetKeyPathByte43Command : public ScriptCommand
{
public:
    u32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd173_SetKeyPathByte43_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd173_SetKeyPathByte43_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd173_SetKeyPathByte43_Execute);
    u32 Size() RETAIL(Cmd173_SetKeyPathByte43_GetSize);
};
CHECK_SIZE(SetKeyPathByte43Command, 0x10);

// 174
class SetFocusToOwnerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd174_SetFocusToOwner_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd174_SetFocusToOwner_Execute);
    u32 Size() RETAIL(Cmd174_SetFocusToOwner_GetSize);
};
CHECK_SIZE(SetFocusToOwnerCommand, 0xC);

// 175
class SetAgentRef1ToOwnerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd175_SetAgentRef1ToOwner_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd175_SetAgentRef1ToOwner_Execute);
    u32 Size() RETAIL(Cmd175_SetAgentRef1ToOwner_GetSize);
};
CHECK_SIZE(SetAgentRef1ToOwnerCommand, 0xC);

// 176
class FadeSoundGroupCommand : public ScriptCommand
{
public:
    f32 value;
    TaggedValue group;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd176_FadeSoundGroup_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd176_FadeSoundGroup_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd176_FadeSoundGroup_Execute);
    u32 Size() RETAIL(Cmd176_FadeSoundGroup_GetSize);
};
CHECK_SIZE(FadeSoundGroupCommand, 0x14);

// 177, 179
class WarpAgentCommand : public ScriptCommand
{
public:
    u32 warp;
    u32 source;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd177_WarpAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd177_WarpAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd177_WarpAgent_Execute);
    u32 Size() RETAIL(Cmd177_WarpAgent_GetSize);
};
CHECK_SIZE(WarpAgentCommand, 0x30);

// 178
class RotateAgentCommand : public ScriptCommand
{
public:
    u32 rotate;
    u32 subject;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    f32 quatX;
    f32 quatY;
    f32 quatZ;
    f32 quatW;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd178_RotateAgent_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd178_RotateAgent_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd178_RotateAgent_Dtor);
    u32 Size() RETAIL(Cmd178_RotateAgent_GetSize);
};
CHECK_SIZE(RotateAgentCommand, 0x30);

// 180
class QueueObjectVideoCommand : public ScriptCommand
{
public:
    s32 value;
    TaggedValue value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd180_QueueObjectVideo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd180_QueueObjectVideo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd180_QueueObjectVideo_Execute);
    u32 Size() RETAIL(Cmd180_QueueObjectVideo_GetSize);
};
CHECK_SIZE(QueueObjectVideoCommand, 0x14);

// 181
class VideoControllerUpdateCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd181_VideoControllerUpdate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd181_VideoControllerUpdate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd181_VideoControllerUpdate_Execute);
    u32 Size() RETAIL(Cmd181_VideoControllerUpdate_GetSize);
};
CHECK_SIZE(VideoControllerUpdateCommand, 0xC);

// 182
class VideoControllerOp182Command : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd182_VideoControllerOp182_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd182_VideoControllerOp182_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd182_VideoControllerOp182_Execute);
    u32 Size() RETAIL(Cmd182_VideoControllerOp182_GetSize);
};
CHECK_SIZE(VideoControllerOp182Command, 0xC);

// 183
class SetTargetOwnerToSelfCommand : public ScriptCommand
{
public:
    TaggedValue target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd183_SetTargetOwnerToSelf_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd183_SetTargetOwnerToSelf_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd183_SetTargetOwnerToSelf_Execute);
    u32 Size() RETAIL(Cmd183_SetTargetOwnerToSelf_GetSize);
};
CHECK_SIZE(SetTargetOwnerToSelfCommand, 0x10);

// 184
class ResetTimerCommand : public ScriptCommand
{
public:
    u32 target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd184_ResetTimer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd184_ResetTimer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd184_ResetTimer_Execute);
    u32 Size() RETAIL(Cmd184_ResetTimer_GetSize);
};
CHECK_SIZE(ResetTimerCommand, 0x10);

// 185
class QueueVideoCommand : public ScriptCommand
{
public:
    TaggedValue movieId;
    u32 flags;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd185_QueueVideo_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd185_QueueVideo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd185_QueueVideo_Dtor);
    u32 Size() RETAIL(Cmd185_QueueVideo_GetSize);
};
CHECK_SIZE(QueueVideoCommand, 0x14);

// 186
class StartQueuedVideoCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd186_StartQueuedVideo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd186_StartQueuedVideo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd186_StartQueuedVideo_Execute);
    u32 Size() RETAIL(Cmd186_StartQueuedVideo_GetSize);
};
CHECK_SIZE(StartQueuedVideoCommand, 0xC);

// 187: a shadow slot (byte 0) of the node's instance given shapes with a distance and a strength (size and size2), cast as
// strongly as height
class SetShadowCommand : public ScriptCommand
{
public:
    u32 shadowSlot;
    TaggedValue size;
    TaggedValue height;
    TaggedValue size2;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd187_SetShadow_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd187_SetShadow_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd187_SetShadow_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd187_SetShadow_Dtor);
    u32 Size() RETAIL(Cmd187_SetShadow_GetSize);
};
CHECK_SIZE(SetShadowCommand, 0x1C);

// 188
class SetShadowCircleCommand : public ScriptCommand
{
public:
    u32 meshSlotAndMode;
    TaggedValue radius;
    TaggedValue radius2;
    TaggedValue offsetX;
    TaggedValue offsetY;
    TaggedValue offsetZ;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd188_SetShadowCircle_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd188_SetShadowCircle_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd188_SetShadowCircle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd188_SetShadowCircle_Dtor);
    u32 Size() RETAIL(Cmd188_SetShadowCircle_GetSize);
};
CHECK_SIZE(SetShadowCircleCommand, 0x24);

// 189
class SetShadowMeshCommand : public ScriptCommand
{
public:
    u32 meshSlotAndMode;
    TaggedValue size;
    TaggedValue offsetX;
    TaggedValue offsetY;
    TaggedValue offsetZ;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd189_SetShadowMesh_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd189_SetShadowMesh_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd189_SetShadowMesh_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd189_SetShadowMesh_Dtor);
    u32 Size() RETAIL(Cmd189_SetShadowMesh_GetSize);
};
CHECK_SIZE(SetShadowMeshCommand, 0x20);

// 190
class SetShadowRectangleCommand : public ScriptCommand
{
public:
    u32 slot;
    TaggedValue size;
    TaggedValue size3;
    TaggedValue offsetX;
    TaggedValue offsetY;
    TaggedValue offsetZ;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd190_SetShadowRectangle_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd190_SetShadowRectangle_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd190_SetShadowRectangle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd190_SetShadowRectangle_Dtor);
    u32 Size() RETAIL(Cmd190_SetShadowRectangle_GetSize);
};
CHECK_SIZE(SetShadowRectangleCommand, 0x24);

// 191
class ShadowToggleCommand : public ScriptCommand
{
public:
    u32 slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd191_ShadowToggle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd191_ShadowToggle_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd191_ShadowToggle_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd191_ShadowToggle_ExecuteOn);
    u32 Size() RETAIL(Cmd191_ShadowToggle_GetSize);
};
CHECK_SIZE(ShadowToggleCommand, 0x10);

// 192
class SetNode10SlotCommand : public ScriptCommand
{
public:
    u32 slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd192_SetNode10Slot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd192_SetNode10Slot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd192_SetNode10Slot_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd192_SetNode10Slot_ExecuteOn);
    u32 Size() RETAIL(Cmd192_SetNode10Slot_GetSize);
};
CHECK_SIZE(SetNode10SlotCommand, 0x10);

// 193
class LaunchAtTargetCommand : public ScriptCommand
{
public:
    u32 unused;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    TaggedValue target;
    f32 speedOrAngle;
    f32 value7;
    f32 value8;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd193_LaunchAtTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd193_LaunchAtTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd193_LaunchAtTarget_Execute);
    u32 Size() RETAIL(Cmd193_LaunchAtTarget_GetSize);
};
CHECK_SIZE(LaunchAtTargetCommand, 0x30);

// 194
class SetNodeBytes168Command : public ScriptCommand
{
public:
    u32 bytes;
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd194_SetNodeBytes168_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd194_SetNodeBytes168_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd194_SetNodeBytes168_Execute);
    u32 Size() RETAIL(Cmd194_SetNodeBytes168_GetSize);
};
CHECK_SIZE(SetNodeBytes168Command, 0x14);

// 195
class StopVideoCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd195_StopVideo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd195_StopVideo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd195_StopVideo_Execute);
    u32 Size() RETAIL(Cmd195_StopVideo_GetSize);
};
CHECK_SIZE(StopVideoCommand, 0xC);

// 196
class StopSoundCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd196_StopSound_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd196_StopSound_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd196_StopSound_Execute);
    u32 Size() RETAIL(Cmd196_StopSound_GetSize);
};
CHECK_SIZE(StopSoundCommand, 0xC);

// 197
class DUMMY_197Command : public ScriptCommand
{
public:
    u32 objectId;
    s32 value2;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    s32 offsetX;
    s32 offsetY;
    s32 offsetZ;
    u32 value9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd197_DUMMY_197_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd197_DUMMY_197_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd197_DUMMY_197_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd197_DUMMY_197_ExecuteOn);
    u32 Size() RETAIL(Cmd197_DUMMY_197_GetSize);
};
CHECK_SIZE(DUMMY_197Command, 0x30);

// 198
class SetCollisionBoxSizeCommand : public ScriptCommand
{
public:
    f32 x;
    f32 y;
    f32 z;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd198_SetCollisionBoxSize_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd198_SetCollisionBoxSize_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd198_SetCollisionBoxSize_Execute);
    u32 Size() RETAIL(Cmd198_SetCollisionBoxSize_GetSize);
};
CHECK_SIZE(SetCollisionBoxSizeCommand, 0x18);

// 199
class NextLinkedObjectInListCommand : public ScriptCommand
{
public:
    TaggedValue links1;
    u32 links2;
    u32 links3;
    u32 links4;
    u32 count;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd199_NextLinkedObjectInList_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd199_NextLinkedObjectInList_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd199_NextLinkedObjectInList_Execute);
    u32 Size() RETAIL(Cmd199_NextLinkedObjectInList_GetSize);
};
CHECK_SIZE(NextLinkedObjectInListCommand, 0x20);

// 200, 201
class ArrangeLinkedObjectsCommand : public ScriptCommand
{
public:
    f32 angleValue;
    f32 value;
    u32 flags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd200_ArrangeLinkedObjects_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd200_ArrangeLinkedObjects_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd200_ArrangeLinkedObjects_Dtor);
    u32 Size() RETAIL(Cmd200_ArrangeLinkedObjects_GetSize);
};
CHECK_SIZE(ArrangeLinkedObjectsCommand, 0x18);

// 202
class TriggerInstanceAtOwnBoxCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd202_TriggerInstanceAtOwnBox_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd202_TriggerInstanceAtOwnBox_Execute);
    u32 Size() RETAIL(Cmd202_TriggerInstanceAtOwnBox_GetSize);
};
CHECK_SIZE(TriggerInstanceAtOwnBoxCommand, 0xC);

// 203
class ScaleModelNodeCommand : public ScriptCommand
{
public:
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd203_ScaleModelNode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd203_ScaleModelNode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd203_ScaleModelNode_Execute);
    u32 Size() RETAIL(Cmd203_ScaleModelNode_GetSize);
};
CHECK_SIZE(ScaleModelNodeCommand, 0x10);

// 204
class SetFocusPositionOffsetCommand : public ScriptCommand
{
public:
    f32 x;
    f32 y;
    f32 z;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 distance;
    TaggedValue use;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd204_SetFocusPositionOffset_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd204_SetFocusPositionOffset_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd204_SetFocusPositionOffset_Execute);
    u32 Size() RETAIL(Cmd204_SetFocusPositionOffset_GetSize);
};
CHECK_SIZE(SetFocusPositionOffsetCommand, 0x2C);

// 205
class SetFocusPositionAtAngleCommand : public ScriptCommand
{
public:
    f32 distance;
    f32 angleValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd205_SetFocusPositionAtAngle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd205_SetFocusPositionAtAngle_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd205_SetFocusPositionAtAngle_Execute);
    u32 Size() RETAIL(Cmd205_SetFocusPositionAtAngle_GetSize);
};
CHECK_SIZE(SetFocusPositionAtAngleCommand, 0x14);

// 206
class SaveScriptStateCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd206_SaveScriptState_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd206_SaveScriptState_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd206_SaveScriptState_Execute);
    u32 Size() RETAIL(Cmd206_SaveScriptState_GetSize);
};
CHECK_SIZE(SaveScriptStateCommand, 0xC);

// 207
class ClearSavedScriptStateCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd207_ClearSavedScriptState_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd207_ClearSavedScriptState_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd207_ClearSavedScriptState_Execute);
    u32 Size() RETAIL(GetScriptActionSize);
};
CHECK_SIZE(ClearSavedScriptStateCommand, 0xC);

// 208
class SetNodeByte8cCommand : public ScriptCommand
{
public:
    TaggedValue value;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd208_SetNodeByte8c_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd208_SetNodeByte8c_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd208_SetNodeByte8c_Dtor);
    u32 Size() RETAIL(Cmd208_SetNodeByte8c_GetSize);
};
CHECK_SIZE(SetNodeByte8cCommand, 0x10);

// 209
class SetGlobalByte30a0e9Command : public ScriptCommand
{
public:
    TaggedValue value;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd209_SetGlobalByte30a0e9_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd209_SetGlobalByte30a0e9_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd209_SetGlobalByte30a0e9_Dtor);
    u32 Size() RETAIL(Cmd209_SetGlobalByte30a0e9_GetSize);
};
CHECK_SIZE(SetGlobalByte30a0e9Command, 0x10);

// 210
class TriggerInstancesInRangeCommand : public ScriptCommand
{
public:
    u32 event;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd210_TriggerInstancesInRange_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd210_TriggerInstancesInRange_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd210_TriggerInstancesInRange_Dtor);
    u32 Size() RETAIL(Cmd210_TriggerInstancesInRange_GetSize);
};
CHECK_SIZE(TriggerInstancesInRangeCommand, 0x10);

// 211
class MarkTimeCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd211_MarkTime_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd211_MarkTime_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd211_MarkTime_Execute);
    u32 Size() RETAIL(Cmd211_MarkTime_GetSize);
};
CHECK_SIZE(MarkTimeCommand, 0xC);

// 212
class ClearMarkedTimeCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd212_ClearMarkedTime_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd212_ClearMarkedTime_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd212_ClearMarkedTime_Execute);
    u32 Size() RETAIL(Cmd212_ClearMarkedTime_GetSize);
};
CHECK_SIZE(ClearMarkedTimeCommand, 0xC);

// 213
class KeyOfPath34Op213Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd213_KeyOfPath34Op213_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd213_KeyOfPath34Op213_Execute);
    u32 Size() RETAIL(Cmd213_KeyOfPath34Op213_GetSize);
};
CHECK_SIZE(KeyOfPath34Op213Command, 0xC);

// 214
class ControllerRumbleCommand : public ScriptCommand
{
public:
    f32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd214_ControllerRumble_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd214_ControllerRumble_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd214_ControllerRumble_Execute);
    u32 Size() RETAIL(Cmd214_ControllerRumble_GetSize);
};
CHECK_SIZE(ControllerRumbleCommand, 0x10);

// 215
class SetSoundParamsCommand : public ScriptCommand
{
public:
    TaggedValue set;
    f32 pitch;
    s32 volume;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd215_SetSoundParams_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd215_SetSoundParams_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd215_SetSoundParams_Execute);
    u32 Size() RETAIL(Cmd215_SetSoundParams_GetSize);
};
CHECK_SIZE(SetSoundParamsCommand, 0x18);

// 512
class CreateCrateContentsCommand : public ScriptCommand
{
public:
    u32 value1;
    u32 value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd512_CreateCrateContents_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd512_CreateCrateContents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd512_CreateCrateContents_Execute);
    u32 Size() RETAIL(Cmd512_CreateCrateContents_GetSize);
};
CHECK_SIZE(CreateCrateContentsCommand, 0x14);

// 513
class CA_PickUpWumpaCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd513_CA_PickUpWumpa_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd513_CA_PickUpWumpa_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd513_CA_PickUpWumpa_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd513_CA_PickUpWumpa_ExecuteOn);
    u32 Size() RETAIL(Cmd513_CA_PickUpWumpa_GetSize);
};
CHECK_SIZE(CA_PickUpWumpaCommand, 0x10);

// 514, 546
class CreateDamageCommand : public ScriptCommand
{
public:
    // Its flags: the contact message's w given (bit 0), its kinds with 0x400 (1), the offset turned with the instance (2), the
    // instances searched for (3: 0x5E000, 4: 0x1000, else 0x5F000), the joint the damage is at (5-12, 0xFF the place), only the
    // nearest instance hit (13); its shape's: 1 a sphere of the reach on every one (2 not), 3 a cylinder, a damage hull (bits 4-7,
    // 0xF none)
    enum Flags : u32
    {
        GivesW = 0x1,
        Bit10Kind = 0x2,
        Offset = 0x4,
        SearchKinds5E = 0x8,
        SearchKinds1000 = 0x10,
        JointShift = 5,
        NearestOnly = 0x2000,
    };

    u32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    u32 flags;
    u32 hitKinds;
    TaggedValue damage;
    TaggedValue messageW;
    u32 shape;
    TaggedValue reach;
    TaggedValue height;
    u32 unknown13;

    // A contact message of damage sent to the instances in a shape at the instance (or one of its joints)
    void ExecuteOn(GameNode* node) RETAIL(Cmd514_CreateDamage_ExecuteOn);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd514_CreateDamage_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd514_CreateDamage_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd514_CreateDamage_Dtor);
    u32 Size() RETAIL(Cmd514_CreateDamage_GetSize);
    // Its vtable's slot 7, a token: 0xCD the reach, 0xA9 the height (a cylinder), the keyword 0x216 after 0x236 (no damage hull), else
    // the base's (DamageOriginator's too): 0x82 to 0x84 the offset (bit 2), 0x12 the joint, 0x204 the damage, 0x94 the w (bit 0),
    // 0x217 bit 1 (its value 0), the keywords of the instances searched for, of only the nearest and of the kinds of hit
    void ParseToken(const ScriptToken* token) RETAIL(FUN_0011fa18);
    void ParseBaseToken(const ScriptToken* token) RETAIL(FUN_0010e088);
    // Its base's destructor and size (vtable D_002F0098, which nothing makes alone)
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0011c450);
    u32 BaseSize() RETAIL(FUN_0011c4a8);
};
CHECK_SIZE(CreateDamageCommand, 0x40);

// 515
class SetAgentCommand : public ScriptCommand
{
public:
    u32 agentFlags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd515_SetAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd515_SetAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd515_SetAgent_Execute);
    u32 Size() RETAIL(Cmd515_SetAgent_GetSize);
};
CHECK_SIZE(SetAgentCommand, 0x10);

// 516
class SetPlayerRespawnPositionCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd516_SetPlayerRespawnPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd516_SetPlayerRespawnPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd516_SetPlayerRespawnPosition_Execute);
    u32 Size() RETAIL(Cmd516_SetPlayerRespawnPosition_GetSize);
};
CHECK_SIZE(SetPlayerRespawnPositionCommand, 0x10);

// 517
class ResetGameCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd517_ResetGame_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd517_ResetGame_Execute);
    u32 Size() RETAIL(Cmd517_ResetGame_GetSize);
};
CHECK_SIZE(ResetGameCommand, 0xC);

// 518
class SetCrateCommand : public ScriptCommand
{
public:
    u32 unused1;
    u32 value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd518_SetCrate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd518_SetCrate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd518_SetCrate_Execute);
    u32 Size() RETAIL(Cmd518_SetCrate_GetSize);
};
CHECK_SIZE(SetCrateCommand, 0x14);

// 519
class TriggerBalancedCrateFallingCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd519_TriggerBalancedCrateFalling_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd519_TriggerBalancedCrateFalling_Execute);
    u32 Size() RETAIL(Cmd519_TriggerBalancedCrateFalling_GetSize);
};
CHECK_SIZE(TriggerBalancedCrateFallingCommand, 0xC);

// 520
class CA_PickUpHealthCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd520_CA_PickUpHealth_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd520_CA_PickUpHealth_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd520_CA_PickUpHealth_ExecuteOn);
    u32 Size() RETAIL(Cmd520_CA_PickUpHealth_GetSize);
};
CHECK_SIZE(CA_PickUpHealthCommand, 0xC);

// 521
class SetPlayerInputCommand : public ScriptCommand
{
public:
    u32 inputFlags;
    u32 unknown2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd521_SetPlayerInput_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd521_SetPlayerInput_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd521_SetPlayerInput_Execute);
    u32 Size() RETAIL(Cmd521_SetPlayerInput_GetSize);
};
CHECK_SIZE(SetPlayerInputCommand, 0x14);

// 522
class TriggerAllNitroCratesCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd522_TriggerAllNitroCrates_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd522_TriggerAllNitroCrates_Execute);
    u32 Size() RETAIL(Cmd522_TriggerAllNitroCrates_GetSize);
};
CHECK_SIZE(TriggerAllNitroCratesCommand, 0xC);

// 523
class ApplyVelocityCommand : public ScriptCommand
{
public:
    TaggedValue radius;
    TaggedValue velX;
    TaggedValue velY;
    TaggedValue velZ;
    TaggedValue velX2;
    TaggedValue velY2;
    TaggedValue velZ2;
    TaggedValue value8;
    TaggedValue value9;
    TaggedValue value10;
    TaggedValue value11;
    s32 target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd523_ApplyVelocity_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd523_ApplyVelocity_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd523_ApplyVelocity_Execute);
    // Its vtable's slot 7: a token of its own (6 the target, 0x46 to 0x48 value9 to value11, bit 2), else ApplyVelocityToSelf's
    u32 ParseToken(const ScriptToken* token) RETAIL(FUN_0011efb8);
    u32 Size() RETAIL(Cmd523_ApplyVelocity_GetSize);
};
CHECK_SIZE(ApplyVelocityCommand, 0x3C);

// 524
class SetKeyNearestPlayerCommand : public ScriptCommand
{
public:
    f32 value1;
    f32 value2;
    u32 value3;

    static SetKeyNearestPlayerCommand* Construct(SetKeyNearestPlayerCommand* command) RETAIL(FUN_001216d8);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd524_SetKeyNearestPlayer_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd524_SetKeyNearestPlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd524_SetKeyNearestPlayer_Dtor);
    u32 Size() RETAIL(Cmd524_SetKeyNearestPlayer_GetSize);
};
CHECK_SIZE(SetKeyNearestPlayerCommand, 0x18);

// 525
class RaycastFocusPositionCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 value5;
    f32 distance;
    s32 size;
    TaggedValue mode;
    u32 unused9;
    u32 target;
    u32 unused11;
    u32 unused12;
    u32 unused13;

    static RaycastFocusPositionCommand* Construct(RaycastFocusPositionCommand* command) RETAIL(FUN_00121130);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd525_RaycastFocusPosition_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd525_RaycastFocusPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd525_RaycastFocusPosition_Dtor);
    u32 Size() RETAIL(Cmd525_RaycastFocusPosition_GetSize);
};
CHECK_SIZE(RaycastFocusPositionCommand, 0x40);

// 526
class ApplyVelocityToSelfCommand : public ScriptCommand
{
public:
    TaggedValue radius;
    TaggedValue velX;
    TaggedValue velY;
    TaggedValue velZ;
    TaggedValue value5;
    TaggedValue value6;
    TaggedValue value7;
    TaggedValue value8;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd526_ApplyVelocityToSelf_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd526_ApplyVelocityToSelf_Dtor);
    u32 Size() RETAIL(Cmd526_ApplyVelocityToSelf_GetSize);
    // Its vtable's slot 7 (ApplyVelocity's falls back on it), a token: 0x6A the radius, 0x49 to 0x4B the velocity (value8 bit 0),
    // 0x4C to 0x4E value5 to value7 (bit 1), the keyword 0xAC (bit 3). Whether it took it
    u32 ParseToken(const ScriptToken* token) RETAIL(FUN_0010c758);
};
CHECK_SIZE(ApplyVelocityToSelfCommand, 0x2C);

// 527
class SetChiChiGrassCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd527_SetChiChiGrass_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd527_SetChiChiGrass_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd527_SetChiChiGrass_Execute);
    u32 Size() RETAIL(Cmd527_SetChiChiGrass_GetSize);
};
CHECK_SIZE(SetChiChiGrassCommand, 0x10);

// 528
class ReduceHitPointsCommand : public ScriptCommand
{
public:
    s32 hitPoints;

    static ReduceHitPointsCommand* Construct(ReduceHitPointsCommand* command) RETAIL(FUN_0011f658);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd528_ReduceHitPoints_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd528_ReduceHitPoints_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd528_ReduceHitPoints_Execute);
    u32 Size() RETAIL(Cmd528_ReduceHitPoints_GetSize);
};
CHECK_SIZE(ReduceHitPointsCommand, 0x10);

// 529
class SetHitPointsCommand : public ScriptCommand
{
public:
    u32 hitPoints;

    static SetHitPointsCommand* Construct(SetHitPointsCommand* command) RETAIL(FUN_0011f780);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd529_SetHitPoints_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd529_SetHitPoints_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd529_SetHitPoints_Execute);
    u32 Size() RETAIL(Cmd529_SetHitPoints_GetSize);
};
CHECK_SIZE(SetHitPointsCommand, 0x10);

// 530
class DUMMY_SetRayTestsCommand : public ScriptCommand
{
public:
    s32 value1;
    s32 value2;
    s32 value3;
    s32 value4;
    s32 value5;

    static DUMMY_SetRayTestsCommand* Construct(DUMMY_SetRayTestsCommand* command) RETAIL(FUN_0011f370);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd530_DUMMY_SetRayTests_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd530_DUMMY_SetRayTests_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd530_DUMMY_SetRayTests_Execute);
    u32 Size() RETAIL(Cmd530_DUMMY_SetRayTests_GetSize);
};
CHECK_SIZE(DUMMY_SetRayTestsCommand, 0x20);

// 532
class DUMMY_NowGoForwardCollidableCommand : public ScriptCommand
{
public:
    TaggedValue angleValue;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_Dtor);
    u32 Size() RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_GetSize);
};
CHECK_SIZE(DUMMY_NowGoForwardCollidableCommand, 0x10);

// 533
class NowGoBackCollidableCommand : public ScriptCommand
{
public:
    TaggedValue value1;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd533_NowGoBackCollidable_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd533_NowGoBackCollidable_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd533_NowGoBackCollidable_Dtor);
    u32 Size() RETAIL(Cmd533_NowGoBackCollidable_GetSize);
};
CHECK_SIZE(NowGoBackCollidableCommand, 0x10);

// 534
class SetGlobalProgressionCommand : public ScriptCommand
{
public:
    u32 value1;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd534_SetGlobalProgression_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd534_SetGlobalProgression_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd534_SetGlobalProgression_Execute);
    u32 Size() RETAIL(Cmd534_SetGlobalProgression_GetSize);
};
CHECK_SIZE(SetGlobalProgressionCommand, 0x14);

// 535
class AddCrystalCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd535_AddCrystal_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd535_AddCrystal_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd535_AddCrystal_ExecuteOn);
    u32 Size() RETAIL(Cmd535_AddCrystal_GetSize);
};
CHECK_SIZE(AddCrystalCommand, 0xC);

// 536
class DUMMY_536Command : public ScriptCommand
{
public:
    u32 unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd536_DUMMY_536_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd536_DUMMY_536_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd536_DUMMY_536_Execute);
    u32 Size() RETAIL(Cmd536_DUMMY_536_GetSize);
};
CHECK_SIZE(DUMMY_536Command, 0x10);

// 537
class AddGemCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd537_AddGem_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd537_AddGem_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd537_AddGem_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd537_AddGem_ExecuteOn);
    u32 Size() RETAIL(Cmd537_AddGem_GetSize);
};
CHECK_SIZE(AddGemCommand, 0x10);

// 538
class DUMMY_538Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd538_DUMMY_538_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd538_DUMMY_538_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd538_DUMMY_538_ExecuteOn);
    u32 Size() RETAIL(Cmd538_DUMMY_538_GetSize);
};
CHECK_SIZE(DUMMY_538Command, 0xC);

// 539
class CA_SetPickupCommand : public ScriptCommand
{
public:
    f32 value1;
    f32 value2;
    f32 value3;
    u32 hitPoints;

    static CA_SetPickupCommand* Construct(CA_SetPickupCommand* command) RETAIL(FUN_001292c0);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd539_CA_SetPickup_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd539_CA_SetPickup_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd539_CA_SetPickup_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd539_CA_SetPickup_ExecuteOn);
    u32 Size() RETAIL(Cmd539_CA_SetPickup_GetSize);
};
CHECK_SIZE(CA_SetPickupCommand, 0x1C);

// 540
class CA_SetProjectileCommand : public ScriptCommand
{
public:
    u32 hitPoints;
    f32 speed;
    s32 value3;
    f32 value4;
    s32 value5;
    TaggedValue radius;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd540_CA_SetProjectile_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd540_CA_SetProjectile_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd540_CA_SetProjectile_Dtor);
    void ExecuteOn(GameNode* node) RETAIL(Cmd540_CA_SetProjectile_ExecuteOn);
    u32 Size() RETAIL(Cmd540_CA_SetProjectile_GetSize);
};
CHECK_SIZE(CA_SetProjectileCommand, 0x24);

// 548
class ShootCommand : public ScriptCommand
{
public:
    // Bits 0-7 of shot: the exit point it's shot from (0xFF the instance's place); bit 11 the offset taken along the frame's
    // axes, 13 shot along the frame's z axis at the speed, 14 at AgentRef1, 15 a projectile bit
    enum Shot : u32
    {
        ExitPointMask = 0xFF,
        Offset = 0x800,
        HasSpeed = 0x2000,
        AtTarget = 0x4000,
        Bit15 = 0x8000,
    };

    u32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 unused5;
    // The object shot (bits 0-14) and the trigger message sent to it (bits 16-31, 0xFFFF none)
    u32 objectAndMessage;
    u32 shot;
    f32 speed;
    u32 unused9;

    // An instance of the object made from the frame of the instance's place or exit point, made a projectile of the instance's
    // (aimed at AgentRef1 when asked) unless it takes packets, and sent the trigger message
    void ExecuteOn(GameNode* node) RETAIL(Cmd548_Shoot_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd548_Shoot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd548_Shoot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd548_Shoot_Execute);
    u32 Size() RETAIL(Cmd548_Shoot_GetSize);
};
CHECK_SIZE(ShootCommand, 0x30);

// 549
class GetShortRouteCommand : public ScriptCommand
{
public:
    f32 unknown1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    u32 targetFlags;
    u32 flags7;
    TaggedValue value8;
    TaggedValue value9;
    TaggedValue value10;
    TaggedValue value11;
    TaggedValue value12;
    u32 keyAndObject;
    u32 unknown14;
    u32 unknown15;
    f32 value16;
    f32 value17;

    static GetShortRouteCommand* Construct(GetShortRouteCommand* command) RETAIL(FUN_0011ccd0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd549_GetShortRoute_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd549_GetShortRoute_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd549_GetShortRoute_Dtor);
    u32 Size() RETAIL(Cmd549_GetShortRoute_GetSize);
};
CHECK_SIZE(GetShortRouteCommand, 0x50);

// 550
class DUMMY_FuelPayGateCommand : public ScriptCommand
{
public:
    u32 unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd550_DUMMY_FuelPayGate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd550_DUMMY_FuelPayGate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd550_DUMMY_FuelPayGate_Execute);
    u32 Size() RETAIL(Cmd550_DUMMY_FuelPayGate_GetSize);
};
CHECK_SIZE(DUMMY_FuelPayGateCommand, 0x10);

// 551
class OpenAllLinkedFurnitureCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd551_OpenAllLinkedFurniture_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd551_OpenAllLinkedFurniture_Execute);
    u32 Size() RETAIL(Cmd551_OpenAllLinkedFurniture_GetSize);
};
CHECK_SIZE(OpenAllLinkedFurnitureCommand, 0xC);

// 552
class CloseAllLinkedFurnitureCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd552_CloseAllLinkedFurniture_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd552_CloseAllLinkedFurniture_Execute);
    u32 Size() RETAIL(Cmd552_CloseAllLinkedFurniture_GetSize);
};
CHECK_SIZE(CloseAllLinkedFurnitureCommand, 0xC);

// 553
class AttachAllLinkedAgentsCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd553_AttachAllLinkedAgents_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd553_AttachAllLinkedAgents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd553_AttachAllLinkedAgents_Execute);
    u32 Size() RETAIL(Cmd553_AttachAllLinkedAgents_GetSize);
};
CHECK_SIZE(AttachAllLinkedAgentsCommand, 0x10);

// 554
class DetachAllLinkedAgentsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd554_DetachAllLinkedAgents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd554_DetachAllLinkedAgents_Execute);
    u32 Size() RETAIL(Cmd554_DetachAllLinkedAgents_GetSize);
};
CHECK_SIZE(DetachAllLinkedAgentsCommand, 0xC);

// 555
class SetVehicleHumiliskateCommand : public ScriptCommand
{
public:
    u32 value1;
    TaggedValue value2;
    TaggedValue value3;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd555_SetVehicleHumiliskate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd555_SetVehicleHumiliskate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd555_SetVehicleHumiliskate_Execute);
    u32 Size() RETAIL(Cmd555_SetVehicleHumiliskate_GetSize);
};
CHECK_SIZE(SetVehicleHumiliskateCommand, 0x18);

// 556
class SetFocusToPlayerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd556_SetFocusToPlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd556_SetFocusToPlayer_Execute);
    u32 Size() RETAIL(Cmd556_SetFocusToPlayer_GetSize);
};
CHECK_SIZE(SetFocusToPlayerCommand, 0xC);

// 557, 564, 565
class RequestFocusCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    u32 ids6;
    u32 ids7;
    u32 ids8;
    u32 targetFlags;
    u32 flags10;
    s32 flags11;
    f32 radius;
    u32 unknown13;

    static RequestFocusCommand* Construct(RequestFocusCommand* command, u32 mode) RETAIL(FUN_0011d4a0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd557_RequestFocus_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd557_RequestFocus_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd557_RequestFocus_Dtor);
    u32 Size() RETAIL(Cmd557_RequestFocus_GetSize);
};
CHECK_SIZE(RequestFocusCommand, 0x40);

// 558
class SetFocusPropertiesCommand : public ScriptCommand
{
public:
    TaggedValue value1;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd558_SetFocusProperties_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd558_SetFocusProperties_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd558_SetFocusProperties_Dtor);
    u32 Size() RETAIL(Cmd558_SetFocusProperties_GetSize);
};
CHECK_SIZE(SetFocusPropertiesCommand, 0x10);

// 559
class SetCameraCommand : public ScriptCommand
{
public:
    TaggedValue value1;
    TaggedValue value2;
    s32 value3;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd559_SetCamera_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd559_SetCamera_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd559_SetCamera_Dtor);
    u32 Size() RETAIL(Cmd559_SetCamera_GetSize);
};
CHECK_SIZE(SetCameraCommand, 0x18);

// 560
class RestoreCameraDefaultsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd560_RestoreCameraDefaults_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd560_RestoreCameraDefaults_Execute);
    u32 Size() RETAIL(Cmd560_RestoreCameraDefaults_GetSize);
};
CHECK_SIZE(RestoreCameraDefaultsCommand, 0xC);

// 561
class LinkToFocusCharacterCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd561_LinkToFocusCharacter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd561_LinkToFocusCharacter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd561_LinkToFocusCharacter_Execute);
    u32 Size() RETAIL(Cmd561_LinkToFocusCharacter_GetSize);
};
CHECK_SIZE(LinkToFocusCharacterCommand, 0x10);

// 562
class UnlinkCharactersCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd562_UnlinkCharacters_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd562_UnlinkCharacters_Execute);
    u32 Size() RETAIL(Cmd562_UnlinkCharacters_GetSize);
};
CHECK_SIZE(UnlinkCharactersCommand, 0xC);

// 563
class DamageOriginatorCommand : public ScriptCommand
{
public:
    u32 unused1;
    u32 unused2;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    u32 unused6;
    u32 unused7;
    u32 unused8;
    u32 unused9;
    u32 contactWord;
    TaggedValue hitPoints;
    u32 unused12;
    u32 unused13;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd563_DamageOriginator_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd563_DamageOriginator_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd563_DamageOriginator_Dtor);
    u32 Size() RETAIL(Cmd563_DamageOriginator_GetSize);
};
CHECK_SIZE(DamageOriginatorCommand, 0x40);

// 566
class SetAgentRef1ToPlayerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd566_SetAgentRef1ToPlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd566_SetAgentRef1ToPlayer_Execute);
    u32 Size() RETAIL(Cmd566_SetAgentRef1ToPlayer_GetSize);
};
CHECK_SIZE(SetAgentRef1ToPlayerCommand, 0xC);

// 567
class SetFocusPositionToPlayerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd567_SetFocusPositionToPlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd567_SetFocusPositionToPlayer_Execute);
    u32 Size() RETAIL(Cmd567_SetFocusPositionToPlayer_GetSize);
};
CHECK_SIZE(SetFocusPositionToPlayerCommand, 0xC);

// 568
class DUMMY_568Command : public ScriptCommand
{
public:
    s32 distance;
    s32 value1;
    s32 value2;
    s32 value3;
    s32 value4;
    u32 shorts1;
    u32 shorts2;
    u32 shorts3;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd568_DUMMY_568_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd568_DUMMY_568_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd568_DUMMY_568_Execute);
    u32 Size() RETAIL(Cmd568_DUMMY_568_GetSize);
};
CHECK_SIZE(DUMMY_568Command, 0x2C);

// 569
class ExitVehicleModeCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd569_ExitVehicleMode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd569_ExitVehicleMode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd569_ExitVehicleMode_Execute);
    u32 Size() RETAIL(Cmd569_ExitVehicleMode_GetSize);
};
CHECK_SIZE(ExitVehicleModeCommand, 0x10);

// 570
class SetVehicleRollerbrawlCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd570_SetVehicleRollerbrawl_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd570_SetVehicleRollerbrawl_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd570_SetVehicleRollerbrawl_Execute);
    u32 Size() RETAIL(Cmd570_SetVehicleRollerbrawl_GetSize);
};
CHECK_SIZE(SetVehicleRollerbrawlCommand, 0x10);

// 571
class SetVehicleHoverboardCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd571_SetVehicleHoverboard_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd571_SetVehicleHoverboard_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd571_SetVehicleHoverboard_Execute);
    u32 Size() RETAIL(Cmd571_SetVehicleHoverboard_GetSize);
};
CHECK_SIZE(SetVehicleHoverboardCommand, 0x10);

// 572
class SetMotionCommand : public ScriptCommand
{
public:
    u32 flags;
    s32 range;
    s32 value2;
    s32 value3;
    s32 value4;
    u32 value5;
    u32 angleX;
    u32 angleY;
    u32 angleZ;
    u32 unused9;
    u32 unused10;
    u32 unused11;
    u32 startDesignator;
    u32 unused13;
    u32 unused14;
    u32 unused15;
    u32 unused16;
    u32 unused17;
    u32 unused18;
    u32 unused19;
    u32 unused20;
    u32 unused21;
    u32 unused22;
    u32 unused23;
    u32 unused24;
    u32 unused25;
    s32 motionType;
    s32 motionFlags2;
    s32 value28;
    u32 unused29;
    u32 unused30;
    u32 unused31;
    u32 unused32;
    u32 unused33;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd572_SetMotion_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd572_SetMotion_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd572_SetMotion_Dtor);
    u32 Size() RETAIL(Cmd572_SetMotion_GetSize);
};
CHECK_SIZE(SetMotionCommand, 0x94);

// 573
class SetNearestPointFlagsCommand : public ScriptCommand
{
public:
    TaggedValue flags;
    s32 range;
    s32 value;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd573_SetNearestPointFlags_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd573_SetNearestPointFlags_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd573_SetNearestPointFlags_Dtor);
    u32 Size() RETAIL(Cmd573_SetNearestPointFlags_GetSize);
};
CHECK_SIZE(SetNearestPointFlagsCommand, 0x18);

// 574
class CreateHeadTrackingCommand : public ScriptCommand
{
public:
    s32 value0;
    f32 range;
    f32 value2;
    s32 angle1;
    s32 angle2;
    s32 angle3;
    f32 dirX;
    f32 dirY;
    f32 dirZ;
    s32 value9;
    s32 value10;
    s32 value11;
    s32 value12;
    s32 value13;
    s32 value14;
    u32 animations;
    f32 speed;
    u32 flags;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd574_CreateHeadTracking_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd574_CreateHeadTracking_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd574_CreateHeadTracking_Dtor);
    u32 Size() RETAIL(Cmd574_CreateHeadTracking_GetSize);
};
CHECK_SIZE(CreateHeadTrackingCommand, 0x54);

// 575
class SetFocusPositionToNearestPointCommand : public ScriptCommand
{
public:
    u32 target;
    f32 unused;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd575_SetFocusPositionToNearestPoint_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd575_SetFocusPositionToNearestPoint_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd575_SetFocusPositionToNearestPoint_Dtor);
    u32 Size() RETAIL(Cmd575_SetFocusPositionToNearestPoint_GetSize);
};
CHECK_SIZE(SetFocusPositionToNearestPointCommand, 0x14);

// 576
class SetFocusToGameActorCommand : public ScriptCommand
{
public:
    u32 actorIndex;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd576_SetFocusToGameActor_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd576_SetFocusToGameActor_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd576_SetFocusToGameActor_Execute);
    u32 Size() RETAIL(Cmd576_SetFocusToGameActor_GetSize);
};
CHECK_SIZE(SetFocusToGameActorCommand, 0x10);

// 577
class BecomeStickyCommand : public ScriptCommand
{
public:
    s32 value1;
    f32 value2;
    u32 objectId;
    s32 message;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd577_BecomeSticky_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd577_BecomeSticky_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd577_BecomeSticky_Execute);
    u32 Size() RETAIL(Cmd577_BecomeSticky_GetSize);
};
CHECK_SIZE(BecomeStickyCommand, 0x1C);

// 578
class CharacterOp578Command : public ScriptCommand
{
public:
    u32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd578_CharacterOp578_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd578_CharacterOp578_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd578_CharacterOp578_Execute);
    u32 Size() RETAIL(Cmd578_CharacterOp578_GetSize);
};
CHECK_SIZE(CharacterOp578Command, 0x10);

// 579
class CounterPositionOp579Command : public ScriptCommand
{
public:
    u32 counter;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd579_CounterPositionOp579_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd579_CounterPositionOp579_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd579_CounterPositionOp579_Execute);
    u32 Size() RETAIL(Cmd579_CounterPositionOp579_GetSize);
};
CHECK_SIZE(CounterPositionOp579Command, 0x10);

// 580
class ApplyVelocityToHeldBodyCommand : public ScriptCommand
{
public:
    u32 unused;
    f32 x;
    f32 y;
    f32 z;
    f32 w;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd580_ApplyVelocityToHeldBody_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd580_ApplyVelocityToHeldBody_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd580_ApplyVelocityToHeldBody_Execute);
    u32 Size() RETAIL(Cmd580_ApplyVelocityToHeldBody_GetSize);
};
CHECK_SIZE(ApplyVelocityToHeldBodyCommand, 0x20);

// 581
class BecomeNormalCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd581_BecomeNormal_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd581_BecomeNormal_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd581_BecomeNormal_Execute);
    u32 Size() RETAIL(Cmd581_BecomeNormal_GetSize);
};
CHECK_SIZE(BecomeNormalCommand, 0x10);

// 582
class AddPerceptionCommand : public ScriptCommand
{
public:
    TaggedValue type;
    f32 value1;
    u32 unused2;
    u32 objectId;
    u32 unused4;
    u32 unused5;
    u32 unused6;
    u32 bytes;
    s32 range;
    u32 range2;
    s32 value10;
    f32 value11;
    s32 value12;
    f32 value13;
    s32 value14;
    f32 value15;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd582_AddPerception_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd582_AddPerception_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd582_AddPerception_Execute);
    u32 Size() RETAIL(Cmd582_AddPerception_GetSize);
};
CHECK_SIZE(AddPerceptionCommand, 0x4C);

// 583
class CutsceneCameraOp583Command : public ScriptCommand
{
public:
    f32 x;
    s32 y;
    TaggedValue value2;
    TaggedValue value3;
    f32 value4;
    f32 value5;
    f32 value6;
    TaggedValue value7;
    s32 value8;
    TaggedValue value9;
    s32 value10;
    TaggedValue value11;
    u32 unused12;
    TaggedValue value13;
    f32 value14;
    f32 value15;
    u32 unused16;
    u32 unused17;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd583_CutsceneCameraOp583_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd583_CutsceneCameraOp583_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd583_CutsceneCameraOp583_Dtor);
    u32 Size() RETAIL(Cmd583_CutsceneCameraOp583_GetSize);
};
CHECK_SIZE(CutsceneCameraOp583Command, 0x54);

// 584
class DUMMY_584Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd584_DUMMY_584_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd584_DUMMY_584_Execute);
    u32 Size() RETAIL(Cmd584_DUMMY_584_GetSize);
};
CHECK_SIZE(DUMMY_584Command, 0xC);

// 586
class DUMMY_586Command : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd586_DUMMY_586_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd586_DUMMY_586_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd586_DUMMY_586_Execute);
    u32 Size() RETAIL(Cmd586_DUMMY_586_GetSize);
};
CHECK_SIZE(DUMMY_586Command, 0x10);

// 587
class SetObjectFlags587Command : public ScriptCommand
{
public:
    TaggedValue flags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd587_SetObjectFlags587_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd587_SetObjectFlags587_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd587_SetObjectFlags587_Execute);
    u32 Size() RETAIL(Cmd587_SetObjectFlags587_GetSize);
};
CHECK_SIZE(SetObjectFlags587Command, 0x10);

// 588
class PlayerFaceTowardsCameraCommand : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd588_PlayerFaceTowardsCamera_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd588_PlayerFaceTowardsCamera_Dtor);
    u32 Size() RETAIL(Cmd588_PlayerFaceTowardsCamera_GetSize);
};
CHECK_SIZE(PlayerFaceTowardsCameraCommand, 0xC);

// 589
class CutsceneStartCommand : public ScriptCommand
{
public:
    f32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd589_CutsceneStart_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd589_CutsceneStart_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd589_CutsceneStart_Execute);
    u32 Size() RETAIL(Cmd589_CutsceneStart_GetSize);
};
CHECK_SIZE(CutsceneStartCommand, 0x10);

// 590
class CutsceneEndCommand : public ScriptCommand
{
public:
    f32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd590_CutsceneEnd_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd590_CutsceneEnd_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd590_CutsceneEnd_Execute);
    u32 Size() RETAIL(Cmd590_CutsceneEnd_GetSize);
};
CHECK_SIZE(CutsceneEndCommand, 0x10);

// 591
class CutsceneCameraMoveCommand : public ScriptCommand
{
public:
    TaggedValue flagsAndAngle;
    f32 offset1;
    f32 offset2;
    f32 offset3;
    f32 offset4;
    f32 offset5;
    f32 offset6;
    u32 unused8;
    u32 unused9;
    s32 value10;
    s32 value11;
    u32 value12;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd591_CutsceneCameraMove_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd591_CutsceneCameraMove_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd591_CutsceneCameraMove_Dtor);
    u32 Size() RETAIL(Cmd591_CutsceneCameraMove_GetSize);
};
CHECK_SIZE(CutsceneCameraMoveCommand, 0x3C);

// 592, 593
class CameraSaveParamsCommand : public ScriptCommand
{
public:
    TaggedValue value1;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd592_CameraSaveParams_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd592_CameraSaveParams_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd592_CameraSaveParams_Dtor);
    u32 Size() RETAIL(Cmd592_CameraSaveParams_GetSize);
};
CHECK_SIZE(CameraSaveParamsCommand, 0x10);

// 594
class ToggleCutsceneCameraCommand : public ScriptCommand
{
public:
    u32 modeFlags;
    f32 blendTime;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd594_ToggleCutsceneCamera_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd594_ToggleCutsceneCamera_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd594_ToggleCutsceneCamera_Dtor);
    u32 Size() RETAIL(Cmd594_ToggleCutsceneCamera_GetSize);
};
CHECK_SIZE(ToggleCutsceneCameraCommand, 0x14);

// 595
class CutsceneCameraTargetsCommand : public ScriptCommand
{
public:
    u32 targets;
    u32 flags;
    u32 keys;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd595_CutsceneCameraTargets_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd595_CutsceneCameraTargets_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd595_CutsceneCameraTargets_Dtor);
    u32 Size() RETAIL(Cmd595_CutsceneCameraTargets_GetSize);
};
CHECK_SIZE(CutsceneCameraTargetsCommand, 0x18);

// 596
class StartWhackawormCommand : public ScriptCommand
{
public:
    s32 animSlots;
    TaggedValue value2;
    TaggedValue hitPoints;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd596_StartWhackaworm_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd596_StartWhackaworm_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd596_StartWhackaworm_Dtor);
    u32 Size() RETAIL(Cmd596_StartWhackaworm_GetSize);
};
CHECK_SIZE(StartWhackawormCommand, 0x18);

// 597
class ProgressWhackawormCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd597_ProgressWhackaworm_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd597_ProgressWhackaworm_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd597_ProgressWhackaworm_Execute);
    u32 Size() RETAIL(Cmd597_ProgressWhackaworm_GetSize);
};
CHECK_SIZE(ProgressWhackawormCommand, 0x10);

// 598
class EndWhackawormCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd598_EndWhackaworm_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd598_EndWhackaworm_Execute);
    u32 Size() RETAIL(Cmd598_EndWhackaworm_GetSize);
};
CHECK_SIZE(EndWhackawormCommand, 0xC);

// 599
class ReleasePlayerHoldCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd599_ReleasePlayerHold_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd599_ReleasePlayerHold_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd599_ReleasePlayerHold_Execute);
    u32 Size() RETAIL(Cmd599_ReleasePlayerHold_GetSize);
};
CHECK_SIZE(ReleasePlayerHoldCommand, 0xC);

// 600
class WarpToChunkLinkTowardsPlayerCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd600_WarpToChunkLinkTowardsPlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd600_WarpToChunkLinkTowardsPlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd600_WarpToChunkLinkTowardsPlayer_Execute);
    u32 Size() RETAIL(Cmd600_WarpToChunkLinkTowardsPlayer_GetSize);
};
CHECK_SIZE(WarpToChunkLinkTowardsPlayerCommand, 0xC);

// 601
class SetVehicleWrestleCreatureCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd601_SetVehicleWrestleCreature_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd601_SetVehicleWrestleCreature_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd601_SetVehicleWrestleCreature_Execute);
    u32 Size() RETAIL(Cmd601_SetVehicleWrestleCreature_GetSize);
};
CHECK_SIZE(SetVehicleWrestleCreatureCommand, 0xC);

// 602
class FadeoutScreenCommand : public ScriptCommand
{
public:
    u32 flags;
    f32 duration;
    s32 unused3;
    f32 red;
    f32 green;
    f32 blue;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd602_FadeoutScreen_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd602_FadeoutScreen_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd602_FadeoutScreen_Dtor);
    u32 Size() RETAIL(Cmd602_FadeoutScreen_GetSize);
};
CHECK_SIZE(FadeoutScreenCommand, 0x24);

// 603
class DisplayBottomTextCommand : public ScriptCommand
{
public:
    s32 value1;
    f32 x;
    f32 y;
    f32 value4;
    f32 value5;
    f32 value6;
    f32 value7;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd603_DisplayBottomText_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd603_DisplayBottomText_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd603_DisplayBottomText_Execute);
    u32 Size() RETAIL(Cmd603_DisplayBottomText_GetSize);
};
CHECK_SIZE(DisplayBottomTextCommand, 0x28);

// 604
class ResetCharacterFallCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd604_ResetCharacterFall_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd604_ResetCharacterFall_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd604_ResetCharacterFall_Execute);
    u32 Size() RETAIL(Cmd604_ResetCharacterFall_GetSize);
};
CHECK_SIZE(ResetCharacterFallCommand, 0xC);

// 605
class DismissCharacterCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd605_DismissCharacter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd605_DismissCharacter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd605_DismissCharacter_Execute);
    u32 Size() RETAIL(Cmd605_DismissCharacter_GetSize);
};
CHECK_SIZE(DismissCharacterCommand, 0x10);

// 606
class CameraFocusObjectCommand : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd606_CameraFocusObject_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd606_CameraFocusObject_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd606_CameraFocusObject_Dtor);
    u32 Size() RETAIL(Cmd606_CameraFocusObject_GetSize);
};
CHECK_SIZE(CameraFocusObjectCommand, 0xC);

// 607
class CameraStopFocusObjectCommand : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd607_CameraStopFocusObject_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd607_CameraStopFocusObject_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd607_CameraStopFocusObject_Dtor);
    u32 Size() RETAIL(Cmd607_CameraStopFocusObject_GetSize);
};
CHECK_SIZE(CameraStopFocusObjectCommand, 0xC);

// 608
class ClearBottomTextCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd608_ClearBottomText_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd608_ClearBottomText_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd608_ClearBottomText_Execute);
    u32 Size() RETAIL(Cmd608_ClearBottomText_GetSize);
};
CHECK_SIZE(ClearBottomTextCommand, 0xC);

// 609
class DUMMY_609Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd609_DUMMY_609_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd609_DUMMY_609_Execute);
    u32 Size() RETAIL(Cmd609_DUMMY_609_GetSize);
};
CHECK_SIZE(DUMMY_609Command, 0xC);

// 610
class DUMMY_610Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd610_DUMMY_610_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd610_DUMMY_610_Execute);
    u32 Size() RETAIL(Cmd610_DUMMY_610_GetSize);
};
CHECK_SIZE(DUMMY_610Command, 0xC);

// 611
class SetCharacterHomeChunkCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd611_SetCharacterHomeChunk_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd611_SetCharacterHomeChunk_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd611_SetCharacterHomeChunk_Execute);
    u32 Size() RETAIL(Cmd611_SetCharacterHomeChunk_GetSize);
};
CHECK_SIZE(SetCharacterHomeChunkCommand, 0xC);

// 612
class GameControllerOp612Command : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd612_GameControllerOp612_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd612_GameControllerOp612_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd612_GameControllerOp612_Execute);
    u32 Size() RETAIL(Cmd612_GameControllerOp612_GetSize);
};
CHECK_SIZE(GameControllerOp612Command, 0xC);

// 613
class DisablePlayerControlCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd613_DisablePlayerControl_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd613_DisablePlayerControl_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd613_DisablePlayerControl_Execute);
    u32 Size() RETAIL(Cmd613_DisablePlayerControl_GetSize);
};
CHECK_SIZE(DisablePlayerControlCommand, 0xC);

// 614
class SetNode120FlagCommand : public ScriptCommand
{
public:
    u32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd614_SetNode120Flag_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd614_SetNode120Flag_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd614_SetNode120Flag_Execute);
    u32 Size() RETAIL(Cmd614_SetNode120Flag_GetSize);
};
CHECK_SIZE(SetNode120FlagCommand, 0x10);

// 615
class SetPlayerFlag57Command : public ScriptCommand
{
public:
    u32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd615_SetPlayerFlag57_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd615_SetPlayerFlag57_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd615_SetPlayerFlag57_Execute);
    u32 Size() RETAIL(Cmd615_SetPlayerFlag57_GetSize);
};
CHECK_SIZE(SetPlayerFlag57Command, 0x10);

// 616
class PlaceCharacterInChunkCommand : public ScriptCommand
{
public:
    s32 character;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd616_PlaceCharacterInChunk_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd616_PlaceCharacterInChunk_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd616_PlaceCharacterInChunk_Execute);
    u32 Size() RETAIL(Cmd616_PlaceCharacterInChunk_GetSize);
};
CHECK_SIZE(PlaceCharacterInChunkCommand, 0x10);

// 617
class HitInstancesInBoxesCommand : public ScriptCommand
{
public:
    u32 flags;
    f32 radius;
    u32 event;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd617_HitInstancesInBoxes_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd617_HitInstancesInBoxes_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd617_HitInstancesInBoxes_Execute);
    u32 Size() RETAIL(Cmd617_HitInstancesInBoxes_GetSize);
};
CHECK_SIZE(HitInstancesInBoxesCommand, 0x18);

// 618
class ForceGameOverCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd618_ForceGameOver_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd618_ForceGameOver_Execute);
    u32 Size() RETAIL(Cmd618_ForceGameOver_GetSize);
};
CHECK_SIZE(ForceGameOverCommand, 0xC);

// 619
class ShowBottomTextCommand : public ScriptCommand
{
public:
    f32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd619_ShowBottomText_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd619_ShowBottomText_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd619_ShowBottomText_Execute);
    u32 Size() RETAIL(Cmd619_ShowBottomText_GetSize);
};
CHECK_SIZE(ShowBottomTextCommand, 0x10);

// 620
class HideBottomTextCommand : public ScriptCommand
{
public:
    f32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd620_HideBottomText_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd620_HideBottomText_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd620_HideBottomText_Execute);
    u32 Size() RETAIL(Cmd620_HideBottomText_GetSize);
};
CHECK_SIZE(HideBottomTextCommand, 0x10);

// 621
class SetFocusToCameraTargetCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd621_SetFocusToCameraTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd621_SetFocusToCameraTarget_Execute);
    u32 Size() RETAIL(Cmd621_SetFocusToCameraTarget_GetSize);
};
CHECK_SIZE(SetFocusToCameraTargetCommand, 0xC);

// 622
class CharacterSoundProxyCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd622_CharacterSoundProxy_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd622_CharacterSoundProxy_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd622_CharacterSoundProxy_Execute);
    u32 Size() RETAIL(Cmd622_CharacterSoundProxy_GetSize);
};
CHECK_SIZE(CharacterSoundProxyCommand, 0x10);

// 623
class CameraNodeSetTargetCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd623_CameraNodeSetTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd623_CameraNodeSetTarget_Execute);
    u32 Size() RETAIL(Cmd623_CameraNodeSetTarget_GetSize);
};
CHECK_SIZE(CameraNodeSetTargetCommand, 0xC);

// 624
class EnableVarPercept629Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd624_EnableVarPercept629_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd624_EnableVarPercept629_Execute);
    u32 Size() RETAIL(Cmd624_EnableVarPercept629_GetSize);
};
CHECK_SIZE(EnableVarPercept629Command, 0xC);

// 625
class SwitchCharacterCommand : public ScriptCommand
{
public:
    u32 value1;
    s32 value2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd625_SwitchCharacter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd625_SwitchCharacter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd625_SwitchCharacter_Execute);
    u32 Size() RETAIL(Cmd625_SwitchCharacter_GetSize);
};
CHECK_SIZE(SwitchCharacterCommand, 0x14);

// 626
class CameraNodeEnableFlagsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd626_CameraNodeEnableFlags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd626_CameraNodeEnableFlags_Execute);
    u32 Size() RETAIL(Cmd626_CameraNodeEnableFlags_GetSize);
};
CHECK_SIZE(CameraNodeEnableFlagsCommand, 0xC);

// 627
class CameraNodeClearFlagsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd627_CameraNodeClearFlags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd627_CameraNodeClearFlags_Execute);
    u32 Size() RETAIL(Cmd627_CameraNodeClearFlags_GetSize);
};
CHECK_SIZE(CameraNodeClearFlagsCommand, 0xC);

// 628
class SetCameraNodeValueCommand : public ScriptCommand
{
public:
    TaggedValue mode;
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd628_SetCameraNodeValue_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd628_SetCameraNodeValue_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd628_SetCameraNodeValue_Execute);
    u32 Size() RETAIL(Cmd628_SetCameraNodeValue_GetSize);
};
CHECK_SIZE(SetCameraNodeValueCommand, 0x14);

// 629
class SetCameraNodeValuesCommand : public ScriptCommand
{
public:
    TaggedValue mode;
    f32 value1;
    f32 value2;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd629_SetCameraNodeValues_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd629_SetCameraNodeValues_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd629_SetCameraNodeValues_Dtor);
    u32 Size() RETAIL(Cmd629_SetCameraNodeValues_GetSize);
};
CHECK_SIZE(SetCameraNodeValuesCommand, 0x18);

// 630
class SetNode5FlagsCommand : public ScriptCommand
{
public:
    TaggedValue flags;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd630_SetNode5Flags_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd630_SetNode5Flags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd630_SetNode5Flags_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd630_SetNode5Flags_ExecuteOn);
    u32 Size() RETAIL(Cmd630_SetNode5Flags_GetSize);
};
CHECK_SIZE(SetNode5FlagsCommand, 0x10);

// 631
class SetPlayerVehicleValueCommand : public ScriptCommand
{
public:
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd631_SetPlayerVehicleValue_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd631_SetPlayerVehicleValue_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd631_SetPlayerVehicleValue_Execute);
    u32 Size() RETAIL(Cmd631_SetPlayerVehicleValue_GetSize);
};
CHECK_SIZE(SetPlayerVehicleValueCommand, 0x10);

// 632
class SetLinkedObjectNearestPlayerCommand : public ScriptCommand
{
public:
    u32 range;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_Dtor);
    u32 Size() RETAIL(Cmd632_SetLinkedObjectNearestPlayer_GetSize);
};
CHECK_SIZE(SetLinkedObjectNearestPlayerCommand, 0x10);

// 633
class SetPlayerModeCommand : public ScriptCommand
{
public:
    u32 value1;
    u32 value2;
    u32 value3;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd633_SetPlayerMode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd633_SetPlayerMode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd633_SetPlayerMode_Execute);
    u32 Size() RETAIL(Cmd633_SetPlayerMode_GetSize);
};
CHECK_SIZE(SetPlayerModeCommand, 0x18);

// 634
class PlayMovieCommand : public ScriptCommand
{
public:
    TaggedValue value;
    f32 value2;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd634_PlayMovie_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd634_PlayMovie_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd634_PlayMovie_Dtor);
    u32 Size() RETAIL(Cmd634_PlayMovie_GetSize);
};
CHECK_SIZE(PlayMovieCommand, 0x14);

// 636
class AddAmmoCommand : public ScriptCommand
{
public:
    s32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd636_AddAmmo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd636_AddAmmo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd636_AddAmmo_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd636_AddAmmo_ExecuteOn);
    u32 Size() RETAIL(Cmd636_AddAmmo_GetSize);
};
CHECK_SIZE(AddAmmoCommand, 0x10);

// 637
class LinkedObjectNearestPlayerOp637Command : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_Dtor);
    u32 Size() RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_GetSize);
};
CHECK_SIZE(LinkedObjectNearestPlayerOp637Command, 0xC);

// 638
class EnableBossModeCommand : public ScriptCommand
{
public:
    s32 animSlots;
    TaggedValue hitPoints;
    f32 value3;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd638_EnableBossMode_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd638_EnableBossMode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd638_EnableBossMode_Dtor);
    u32 Size() RETAIL(Cmd638_EnableBossMode_GetSize);
};
CHECK_SIZE(EnableBossModeCommand, 0x18);

// 639
class DamageBossCommand : public ScriptCommand
{
public:
    u32 value1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd639_DamageBoss_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd639_DamageBoss_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd639_DamageBoss_Execute);
    u32 Size() RETAIL(Cmd639_DamageBoss_GetSize);
};
CHECK_SIZE(DamageBossCommand, 0x10);

// 640
class ExitBossModeCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd640_ExitBossMode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd640_ExitBossMode_Execute);
    u32 Size() RETAIL(Cmd640_ExitBossMode_GetSize);
};
CHECK_SIZE(ExitBossModeCommand, 0xC);

// 641, 642, 643, 644, 653
class FinalBossInitWeaponsCommand : public ScriptCommand
{
public:
    u32 target;
    s32 mode;
    u32 weapons;
    s32 value1;
    s32 value2;
    u32 slots;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd641_FinalBossInitWeapons_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd641_FinalBossInitWeapons_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd641_FinalBossInitWeapons_Execute);
    u32 Size() RETAIL(Cmd641_FinalBossInitWeapons_GetSize);
};
CHECK_SIZE(FinalBossInitWeaponsCommand, 0x24);

// 645
class CreateNodeControllerCommand : public ScriptCommand
{
public:
    u32 controller;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd645_CreateNodeController_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd645_CreateNodeController_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd645_CreateNodeController_Execute);
    u32 Size() RETAIL(Cmd645_CreateNodeController_GetSize);
};
CHECK_SIZE(CreateNodeControllerCommand, 0x10);

// 646
class RequestOgiSlotCommand : public ScriptCommand
{
public:
    u32 slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd646_RequestOgiSlot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd646_RequestOgiSlot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd646_RequestOgiSlot_Execute);
    u32 Size() RETAIL(Cmd646_RequestOgiSlot_GetSize);
};
CHECK_SIZE(RequestOgiSlotCommand, 0x10);

// 647
class SetGlobalProgression2Command : public ScriptCommand
{
public:
    s32 value1;
    TaggedValue value;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd647_SetGlobalProgression2_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd647_SetGlobalProgression2_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd647_SetGlobalProgression2_Dtor);
    u32 Size() RETAIL(Cmd647_SetGlobalProgression2_GetSize);
};
CHECK_SIZE(SetGlobalProgression2Command, 0x14);

// 648
class SetNodeValue174Command : public ScriptCommand
{
public:
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd648_SetNodeValue174_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd648_SetNodeValue174_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd648_SetNodeValue174_Execute);
    u32 Size() RETAIL(Cmd648_SetNodeValue174_GetSize);
};
CHECK_SIZE(SetNodeValue174Command, 0x10);

// 649
class ClearNodeValue174Command : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd649_ClearNodeValue174_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd649_ClearNodeValue174_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd649_ClearNodeValue174_Execute);
    u32 Size() RETAIL(Cmd649_ClearNodeValue174_GetSize);
};
CHECK_SIZE(ClearNodeValue174Command, 0xC);

// 650
class SetCharacterFlag2Command : public ScriptCommand
{
public:
    s32 character;

    void Destroy(u32 destroyFlags) RETAIL(Cmd650_SetCharacterFlag2_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd650_SetCharacterFlag2_Execute);
    u32 Size() RETAIL(Cmd650_SetCharacterFlag2_GetSize);
};
CHECK_SIZE(SetCharacterFlag2Command, 0x10);

// 651
class ClearCharacterFlag2Command : public ScriptCommand
{
public:
    s32 character;

    void Destroy(u32 destroyFlags) RETAIL(Cmd651_ClearCharacterFlag2_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd651_ClearCharacterFlag2_Execute);
    u32 Size() RETAIL(Cmd651_ClearCharacterFlag2_GetSize);
};
CHECK_SIZE(ClearCharacterFlag2Command, 0x10);

// 652
class CameraTopdownModeCommand : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd652_CameraTopdownMode_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd652_CameraTopdownMode_Dtor);
    u32 Size() RETAIL(Cmd652_CameraTopdownMode_GetSize);
};
CHECK_SIZE(CameraTopdownModeCommand, 0xC);

// 654
class PlayCreditsCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd654_PlayCredits_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd654_PlayCredits_Execute);
    u32 Size() RETAIL(Cmd654_PlayCredits_GetSize);
};
CHECK_SIZE(PlayCreditsCommand, 0xC);

// 655
class SetMaskControllerIdsCommand : public ScriptCommand
{
public:
    u32 ids1;
    u32 ids2;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    u32 unused4;
    u32 count;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd655_SetMaskControllerIds_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd655_SetMaskControllerIds_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd655_SetMaskControllerIds_Execute);
    u32 Size() RETAIL(Cmd655_SetMaskControllerIds_GetSize);
};
CHECK_SIZE(SetMaskControllerIdsCommand, 0x28);

// 656
class ResetMaskControllerCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd656_ResetMaskController_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd656_ResetMaskController_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd656_ResetMaskController_Execute);
    u32 Size() RETAIL(Cmd656_ResetMaskController_GetSize);
};
CHECK_SIZE(ResetMaskControllerCommand, 0xC);

// 657
class DisplayBottomTextInstanceCommand : public ScriptCommand
{
public:
    f32 x;
    f32 y;
    f32 value3;
    f32 value4;
    f32 value5;
    f32 value6;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd657_DisplayBottomTextInstance_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd657_DisplayBottomTextInstance_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd657_DisplayBottomTextInstance_Execute);
    u32 Size() RETAIL(Cmd657_DisplayBottomTextInstance_GetSize);
};
CHECK_SIZE(DisplayBottomTextInstanceCommand, 0x24);

// 658
class SetSplineControllerValuesCommand : public ScriptCommand
{
public:
    TaggedValue x;
    TaggedValue y;
    TaggedValue z;
    TaggedValue value4;
    TaggedValue value5;
    TaggedValue value6;
    TaggedValue value7;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd658_SetSplineControllerValues_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd658_SetSplineControllerValues_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd658_SetSplineControllerValues_Execute);
    u32 Size() RETAIL(Cmd658_SetSplineControllerValues_GetSize);
};
CHECK_SIZE(SetSplineControllerValuesCommand, 0x28);

// 659
class TriggerCharacterEvent12Command : public ScriptCommand
{
public:
    s32 characters;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd659_TriggerCharacterEvent12_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd659_TriggerCharacterEvent12_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd659_TriggerCharacterEvent12_Execute);
    // Event 12 run on an instance's character (none without its character node)
    void TriggerOn(struct InstanceContext* instance, BehaviourLevel* level) RETAIL(FUN_00122c28);
    u32 Size() RETAIL(Cmd659_TriggerCharacterEvent12_GetSize);
};
CHECK_SIZE(TriggerCharacterEvent12Command, 0x10);

// 660
class ClearPlayerFlag14Command : public ScriptCommand
{
public:
    u32 unused;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd660_ClearPlayerFlag14_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd660_ClearPlayerFlag14_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd660_ClearPlayerFlag14_Execute);
    u32 Size() RETAIL(Cmd660_ClearPlayerFlag14_GetSize);
};
CHECK_SIZE(ClearPlayerFlag14Command, 0x10);

// 661
class SetSkateControllerIdsCommand : public ScriptCommand
{
public:
    u32 ids1;
    u32 ids2;
    u32 ids3;
    u32 ids4;
    u32 ids5;
    u32 ids6;
    u32 sounds1;
    u32 sounds2;
    u32 sounds3;
    u32 sounds4;
    u32 sounds5;
    u32 sounds6;
    u32 sounds7;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    u32 unused6;
    u32 unused7;
    u32 unused8;
    u32 unused9;
    u32 counts;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd661_SetSkateControllerIds_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd661_SetSkateControllerIds_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd661_SetSkateControllerIds_Execute);
    u32 Size() RETAIL(Cmd661_SetSkateControllerIds_GetSize);
};
CHECK_SIZE(SetSkateControllerIdsCommand, 0x68);

// 662
class ResetCameraCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd662_ResetCamera_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd662_ResetCamera_Execute);
    u32 Size() RETAIL(Cmd662_ResetCamera_GetSize);
};
CHECK_SIZE(ResetCameraCommand, 0xC);

extern "C"
{
    extern const GccVTableEntry g_AddTrailCommandVTable[] RETAIL(vt_Cmd1_AddTrail);
    extern const GccVTableEntry g_ClearTrailCommandVTable[] RETAIL(vt_Cmd2_ClearTrail);
    extern const GccVTableEntry g_PositionWarpCommandVTable[] RETAIL(vt_Cmd3_PositionWarp);
    extern const GccVTableEntry g_SetKeyCommandVTable[] RETAIL(vt_Cmd4_SetKey);
    extern const GccVTableEntry g_NextKeyCommandVTable[] RETAIL(vt_Cmd5_NextKey);
    extern const GccVTableEntry g_RestartPreviousCommandVTable[] RETAIL(VT_RestartPrevious);
    extern const GccVTableEntry g_SpawnResidentAgentCommandVTable[] RETAIL(SpawnResidentAgent_VTable);
    extern const GccVTableEntry g_DoAnimationCommandVTable[] RETAIL(PlayAnimationCommand_Methods);
    extern const GccVTableEntry g_DoParticleCommandVTable[] RETAIL(vt_Cmd10_DoParticle);
    extern const GccVTableEntry g_DoSoundCommandVTable[] RETAIL(vt_Cmd11_DoSound);
    extern const GccVTableEntry g_SetWobbleCommandVTable[] RETAIL(vt_Cmd12_SetWobble);
    extern const GccVTableEntry g_ClearWobbleCommandVTable[] RETAIL(vt_Cmd13_ClearWobble);
    extern const GccVTableEntry g_NowMoveForwardsCommandVTable[] RETAIL(vt_Cmd14_NowMoveForwards);
    extern const GccVTableEntry g_NowMoveBackwardsCommandVTable[] RETAIL(vt_Cmd15_NowMoveBackwards);
    extern const GccVTableEntry g_NowStrafeLeftCommandVTable[] RETAIL(vt_Cmd16_NowStrafeLeft);
    extern const GccVTableEntry g_NowStrafeRightCommandVTable[] RETAIL(vt_Cmd17_NowStrafeRight);
    extern const GccVTableEntry g_NowTurnLeftCommandVTable[] RETAIL(vt_Cmd18_NowTurnLeft);
    extern const GccVTableEntry g_NowRotateJointCommandVTable[] RETAIL(vt_Cmd23_NowRotateJoint);
    extern const GccVTableEntry g_StoreCurrentSpaceCommandVTable[] RETAIL(vt_Cmd27_StoreCurrentSpace);
    extern const GccVTableEntry g_SetFocusToKeyCommandVTable[] RETAIL(vt_Cmd28_SetFocusToKey);
    extern const GccVTableEntry g_RotationWarpCommandVTable[] RETAIL(vt_Cmd29_RotationWarp);
    extern const GccVTableEntry g_ClearThreatsCommandVTable[] RETAIL(vt_Cmd31_ClearThreats);
    extern const GccVTableEntry g_TriggerLinkedObjectsCommandVTable[] RETAIL(vt_Cmd33_TriggerLinkedObjects);
    extern const GccVTableEntry g_SetStateCommandVTable[] RETAIL(vt_Cmd34_SetState);
    extern const GccVTableEntry g_ToggleStateCommandVTable[] RETAIL(vt_Cmd35_ToggleState);
    extern const GccVTableEntry g_NextRouteNodeCommandVTable[] RETAIL(vt_Cmd36_NextRouteNode);
    extern const GccVTableEntry g_DiscardRouteCommandVTable[] RETAIL(vt_Cmd39_DiscardRoute);
    extern const GccVTableEntry g_SetLogicalRadiusCommandVTable[] RETAIL(vt_Cmd40_SetLogicalRadius);
    extern const GccVTableEntry g_SetBehaviourPriorityCommandVTable[] RETAIL(vt_Cmd42_SetBehaviourPriority);
    extern const GccVTableEntry g_SetCollisionsCommandVTable[] RETAIL(vt_Cmd44_SetCollisions);
    extern const GccVTableEntry g_SetFocusToAgentCommandVTable[] RETAIL(vt_Cmd45_SetFocusToAgent);
    extern const GccVTableEntry g_AttachFocusObjectCommandVTable[] RETAIL(vt_Cmd47_AttachFocusObject);
    extern const GccVTableEntry g_DropAttachedObjectCommandVTable[] RETAIL(vt_Cmd48_DropAttachedObject);
    extern const GccVTableEntry g_ThrowAttachedObjectCommandVTable[] RETAIL(vt_Cmd49_ThrowAttachedObject);
    extern const GccVTableEntry g_UnsupportOverFocusCommandVTable[] RETAIL(vt_Cmd50_UnsupportOverFocus);
    extern const GccVTableEntry g_UnsupportAboveCommandVTable[] RETAIL(vt_Cmd51_UnsupportAbove);
    extern const GccVTableEntry g_ClearFocusCommandVTable[] RETAIL(ClearFocusCommand_Methods);
    extern const GccVTableEntry g_ClearCollisionsCommandVTable[] RETAIL(vt_Cmd53_ClearCollisions);
    extern const GccVTableEntry g_SendUserMessageCommandVTable[] RETAIL(TriggerSignalCommand_Methods);
    extern const GccVTableEntry g_BroadcastUserMessageCommandVTable[] RETAIL(vt_Cmd55_BroadcastUserMessage);
    extern const GccVTableEntry g_ClearAnimationCommandVTable[] RETAIL(vt_Cmd56_ClearAnimation);
    extern const GccVTableEntry g_RequestAttachmentFocusCommandVTable[] RETAIL(vt_Cmd57_RequestAttachmentFocus);
    extern const GccVTableEntry g_RequestMessengersFocusCommandVTable[] RETAIL(vt_Cmd59_RequestMessengersFocus);
    extern const GccVTableEntry g_SetFocusPositionCommandVTable[] RETAIL(vt_Cmd60_SetFocusPosition);
    extern const GccVTableEntry g_StopMovingCommandVTable[] RETAIL(vt_Cmd61_StopMoving);
    extern const GccVTableEntry g_AddNoiseToFocusPositionCommandVTable[] RETAIL(vt_Cmd62_AddNoiseToFocusPosition);
    extern const GccVTableEntry g_RestartDefaultBehaviourCommandVTable[] RETAIL(vt_Cmd63_RestartDefaultBehaviour);
    extern const GccVTableEntry g_ClearUserMessageCommandVTable[] RETAIL(vt_Cmd66_ClearUserMessage);
    extern const GccVTableEntry g_RequestMessSourceAsFocusCommandVTable[] RETAIL(vt_Cmd67_RequestMessSourceAsFocus);
    extern const GccVTableEntry g_SetCounterCommandVTable[] RETAIL(vt_Cmd68_SetCounter);
    extern const GccVTableEntry g_ModifyCounterCommandVTable[] RETAIL(vt_Cmd69_ModifyCounter);
    extern const GccVTableEntry g_ContinueColliderMotionCommandVTable[] RETAIL(vt_Cmd70_ContinueColliderMotion);
    extern const GccVTableEntry g_RevertColliderMotionCommandVTable[] RETAIL(vt_Cmd71_RevertColliderMotion);
    extern const GccVTableEntry g_ColliderLaunchNowCommandVTable[] RETAIL(vt_Cmd72_ColliderLaunchNow);
    extern const GccVTableEntry g_ForceAnimationUpdateCommandVTable[] RETAIL(vt_Cmd74_ForceAnimationUpdate);
    extern const GccVTableEntry g_DestroySpawnedAttachmentCommandVTable[] RETAIL(vt_Cmd75_DestroySpawnedAttachment);
    extern const GccVTableEntry g_ApplyImpulseCommandVTable[] RETAIL(vt_Cmd76_ApplyImpulse);
    extern const GccVTableEntry g_RequestDetachCommandVTable[] RETAIL(vt_Cmd77_RequestDetach);
    extern const GccVTableEntry g_SetObjectCommandVTable[] RETAIL(vt_Cmd78_SetObject);
    extern const GccVTableEntry g_KeepCommandVTable[] RETAIL(vt_Cmd79_Keep);
    extern const GccVTableEntry g_AttachSpringCommandVTable[] RETAIL(vt_Cmd80_AttachSpring);
    extern const GccVTableEntry g_DetachAllSpringsCommandVTable[] RETAIL(vt_Cmd81_DetachAllSprings);
    extern const GccVTableEntry g_SetContactSpringyCommandVTable[] RETAIL(vt_Cmd82_SetContactSpringy);
    extern const GccVTableEntry g_ClearContactResponseCommandVTable[] RETAIL(vt_Cmd84_ClearContactResponse);
    extern const GccVTableEntry g_DestroyMeCommandVTable[] RETAIL(vt_Cmd85_DestroyMe);
    extern const GccVTableEntry g_SetSoundCommandVTable[] RETAIL(vt_Cmd86_SetSound);
    extern const GccVTableEntry g_AlterWobblePhaseCommandVTable[] RETAIL(vt_Cmd87_AlterWobblePhase);
    extern const GccVTableEntry g_BeginMusicCommandVTable[] RETAIL(vt_Cmd88_BeginMusic);
    extern const GccVTableEntry g_EndContextMusicCommandVTable[] RETAIL(vt_Cmd89_EndContextMusic);
    extern const GccVTableEntry g_AddLivesCommandVTable[] RETAIL(vt_Cmd92_AddLives);
    extern const GccVTableEntry g_ReleaseAgentRef2CommandVTable[] RETAIL(vt_Cmd95_Unknown);
    extern const GccVTableEntry g_LaunchAgentRef2CommandVTable[] RETAIL(vt_Cmd96_Unknown);
    extern const GccVTableEntry g_ClearAgentRef1CommandVTable[] RETAIL(vt_Cmd97_ClearAgentRef1);
    extern const GccVTableEntry g_ClearAgentRef2CommandVTable[] RETAIL(vt_Cmd98_ClearAgentRef2);
    extern const GccVTableEntry g_ClearFocusPositionCommandVTable[] RETAIL(vt_Cmd110_Unknown);
    extern const GccVTableEntry g_CacheLinkedInstanceCommandVTable[] RETAIL(vt_Cmd111_CacheLinkedInstance);
    extern const GccVTableEntry g_SetRotationComponentsCommandVTable[] RETAIL(vt_Cmd113_Unknown);
    extern const GccVTableEntry g_StopHeadTrackingCommandVTable[] RETAIL(vt_Cmd114_Unknown);
    extern const GccVTableEntry g_StartHeadTrackingCommandVTable[] RETAIL(vt_Cmd115_Unknown);
    extern const GccVTableEntry g_DestroyHeadTrackingCommandVTable[] RETAIL(vt_Cmd116_Unknown);
    extern const GccVTableEntry g_MakeNoiseCommandVTable[] RETAIL(vt_Cmd117_Unknown);
    extern const GccVTableEntry g_SetHeadTrackingTargetCommandVTable[] RETAIL(vt_Cmd118_Unknown);
    extern const GccVTableEntry g_ClearNodeByte154CommandVTable[] RETAIL(vt_Cmd119_Unknown);
    extern const GccVTableEntry g_PhysicsResetVelocityCommandVTable[] RETAIL(vt_Cmd120_PhysicsResetVelocity);
    extern const GccVTableEntry g_SetFocusPositionBesidePlayerCommandVTable[] RETAIL(vt_Cmd121_Unknown);
    extern const GccVTableEntry g_SetFocusPositionToAgentCommandVTable[] RETAIL(vt_Cmd122_Unknown);
    extern const GccVTableEntry g_LinkToNearestPointCommandVTable[] RETAIL(vt_Cmd123_Unknown);
    extern const GccVTableEntry g_RunScriptSlotCommandVTable[] RETAIL(vt_Cmd124_RunScriptSlot);
    extern const GccVTableEntry g_OffsetFocusPositionCommandVTable[] RETAIL(vt_Cmd125_Unknown);
    extern const GccVTableEntry g_NextLinkedObjectCommandVTable[] RETAIL(vt_Cmd126_Unknown);
    extern const GccVTableEntry g_PhysicsSetGravityCommandVTable[] RETAIL(vt_Cmd127_Unknown);
    extern const GccVTableEntry g_PhysicsBodyResetCommandVTable[] RETAIL(vt_Cmd128_Unknown);
    extern const GccVTableEntry g_PhysicsBodyActivateCommandVTable[] RETAIL(vt_Cmd129_Unknown);
    extern const GccVTableEntry g_SetPhysicsSizesCommandVTable[] RETAIL(vt_Cmd130_Unknown);
    extern const GccVTableEntry g_MagnetPullToFocusCommandVTable[] RETAIL(vt_Cmd131_Unknown);
    extern const GccVTableEntry g_SetLinkedObjectIndexCommandVTable[] RETAIL(vt_Cmd132_Unknown);
    extern const GccVTableEntry g_ClearObjectContextTargetCommandVTable[] RETAIL(vt_Cmd133_Unknown);
    extern const GccVTableEntry g_SetFocusToOriginatorCommandVTable[] RETAIL(vt_Cmd134_Unknown);
    extern const GccVTableEntry g_DestroyPerceptionsCommandVTable[] RETAIL(vt_Cmd135_Unknown);
    extern const GccVTableEntry g_SetPerceptionWeightCommandVTable[] RETAIL(vt_Cmd136_Unknown);
    extern const GccVTableEntry g_AddPerceptionWeightCommandVTable[] RETAIL(vt_Cmd137_Unknown);
    extern const GccVTableEntry g_PushFromPerceptionCommandVTable[] RETAIL(vt_Cmd138_Unknown);
    extern const GccVTableEntry g_SetCharacterAnalogCommandVTable[] RETAIL(vt_Cmd139_Unknown);
    extern const GccVTableEntry g_AddCharacterAnalogCommandVTable[] RETAIL(vt_Cmd140_Unknown);
    extern const GccVTableEntry g_PerceptionOp141CommandVTable[] RETAIL(vt_Cmd141_Unknown);
    extern const GccVTableEntry g_PerceptionOp142CommandVTable[] RETAIL(vt_Cmd142_Unknown);
    extern const GccVTableEntry g_DisableAllPerceptionsCommandVTable[] RETAIL(vt_Cmd143_Unknown);
    extern const GccVTableEntry g_EnableAllPerceptionsCommandVTable[] RETAIL(vt_Cmd144_Unknown);
    extern const GccVTableEntry g_SetParentExecutionValueCommandVTable[] RETAIL(vt_Cmd145_Unknown);
    extern const GccVTableEntry g_SetFocusToLinkedObjectCommandVTable[] RETAIL(vt_Cmd146_Unknown);
    extern const GccVTableEntry g_PreviousKeyCommandVTable[] RETAIL(vt_Cmd147_Unknown);
    extern const GccVTableEntry g_SetMotionFloatsCommandVTable[] RETAIL(vt_Cmd148_Unknown);
    extern const GccVTableEntry g_RotateWithLinkedCommandVTable[] RETAIL(vt_Cmd149_Unknown);
    extern const GccVTableEntry g_StrafeTowardsTargetCommandVTable[] RETAIL(vt_Cmd150_Unknown);
    extern const GccVTableEntry g_NextKeyOfPath34CommandVTable[] RETAIL(vt_Cmd151_Unknown);
    extern const GccVTableEntry g_AddMotionAnglesCommandVTable[] RETAIL(vt_Cmd152_Unknown);
    extern const GccVTableEntry g_MoveTowardsDesignatorCommandVTable[] RETAIL(vt_Cmd153_Unknown);
    extern const GccVTableEntry g_SetNode150FieldsCommandVTable[] RETAIL(vt_Cmd156_Unknown);
    extern const GccVTableEntry g_UnlinkTargetCommandVTable[] RETAIL(vt_Cmd157_Unknown);
    extern const GccVTableEntry g_AttachMotionBlockCommandVTable[] RETAIL(vt_Cmd158_Unknown);
    extern const GccVTableEntry g_ResetNode120CommandVTable[] RETAIL(vt_Cmd159_Unknown);
    extern const GccVTableEntry g_ClearMotionBlockFlag16CommandVTable[] RETAIL(vt_Cmd160_Unknown);
    extern const GccVTableEntry g_SetMotionBlockFlag16CommandVTable[] RETAIL(vt_Cmd161_Unknown);
    extern const GccVTableEntry g_AddToFocusObjectByteCommandVTable[] RETAIL(vt_Cmd162_Unknown);
    extern const GccVTableEntry g_UnlinkFromTargetCommandVTable[] RETAIL(vt_Cmd163_Unknown);
    extern const GccVTableEntry g_AddToLinkedObjectsByteCommandVTable[] RETAIL(vt_Cmd164_Unknown);
    extern const GccVTableEntry g_ForceVolumeControllerCommandVTable[] RETAIL(vt_Cmd165_ForceVolumeController);
    extern const GccVTableEntry g_NotifyInstancesWithinCommandVTable[] RETAIL(vt_Cmd166_Unknown);
    extern const GccVTableEntry g_SetSurfaceCommandVTable[] RETAIL(vt_Cmd167_SetSurface);
    extern const GccVTableEntry g_MoveInstancesInBoxCommandVTable[] RETAIL(vt_Cmd168_Unknown);
    extern const GccVTableEntry g_SetFocusPositionAlongCommandVTable[] RETAIL(vt_Cmd169_Unknown);
    extern const GccVTableEntry g_SetFocusObjectByteCommandVTable[] RETAIL(vt_Cmd170_Unknown);
    extern const GccVTableEntry g_RunSlotBehaviourOnLinkedCommandVTable[] RETAIL(vt_Cmd171_Unknown);
    extern const GccVTableEntry g_StopTargetBehaviourCommandVTable[] RETAIL(vt_Cmd172_Unknown);
    extern const GccVTableEntry g_SetKeyPathByte43CommandVTable[] RETAIL(vt_Cmd173_Unknown);
    extern const GccVTableEntry g_SetFocusToOwnerCommandVTable[] RETAIL(vt_Cmd174_Unknown);
    extern const GccVTableEntry g_SetAgentRef1ToOwnerCommandVTable[] RETAIL(vt_Cmd175_Unknown);
    extern const GccVTableEntry g_FadeSoundGroupCommandVTable[] RETAIL(vt_Cmd176_Unknown);
    extern const GccVTableEntry g_WarpAgentCommandVTable[] RETAIL(vt_Cmd177_Unknown);
    extern const GccVTableEntry g_RotateAgentCommandVTable[] RETAIL(vt_Cmd178_Unknown);
    extern const GccVTableEntry g_QueueObjectVideoCommandVTable[] RETAIL(vt_Cmd180_Unknown);
    extern const GccVTableEntry g_VideoControllerUpdateCommandVTable[] RETAIL(vt_Cmd181_Unknown);
    extern const GccVTableEntry g_VideoControllerOp182CommandVTable[] RETAIL(vt_Cmd182_Unknown);
    extern const GccVTableEntry g_SetTargetOwnerToSelfCommandVTable[] RETAIL(vt_Cmd183_Unknown);
    extern const GccVTableEntry g_ResetTimerCommandVTable[] RETAIL(vt_Cmd184_Unknown);
    extern const GccVTableEntry g_QueueVideoCommandVTable[] RETAIL(vt_Cmd185_Unknown);
    extern const GccVTableEntry g_StartQueuedVideoCommandVTable[] RETAIL(vt_Cmd186_Unknown);
    extern const GccVTableEntry g_SetShadowCommandVTable[] RETAIL(vt_Cmd187_SetShadow);
    extern const GccVTableEntry g_SetShadowCircleCommandVTable[] RETAIL(vt_Cmd188_SetShadowCircle);
    extern const GccVTableEntry g_SetShadowMeshCommandVTable[] RETAIL(vt_Cmd189_SetShadowMesh);
    extern const GccVTableEntry g_SetShadowRectangleCommandVTable[] RETAIL(vt_Cmd190_SetShadowRectangle);
    extern const GccVTableEntry g_ShadowToggleCommandVTable[] RETAIL(vt_Cmd191_ShadowToggle);
    extern const GccVTableEntry g_SetNode10SlotCommandVTable[] RETAIL(vt_Cmd192_Unknown);
    extern const GccVTableEntry g_LaunchAtTargetCommandVTable[] RETAIL(vt_Cmd193_Unknown);
    extern const GccVTableEntry g_SetNodeBytes168CommandVTable[] RETAIL(vt_Cmd194_Unknown);
    extern const GccVTableEntry g_StopVideoCommandVTable[] RETAIL(vt_Cmd195_Unknown);
    extern const GccVTableEntry g_StopSoundCommandVTable[] RETAIL(vt_Cmd196_Unknown);
    extern const GccVTableEntry g_DUMMY_197CommandVTable[] RETAIL(vt_Cmd197_DUMMY_197);
    extern const GccVTableEntry g_SetCollisionBoxSizeCommandVTable[] RETAIL(vt_Cmd198_Unknown);
    extern const GccVTableEntry g_NextLinkedObjectInListCommandVTable[] RETAIL(vt_Cmd199_Unknown);
    extern const GccVTableEntry g_ArrangeLinkedObjectsCommandVTable[] RETAIL(vt_Cmd200_Unknown);
    extern const GccVTableEntry g_TriggerInstanceAtOwnBoxCommandVTable[] RETAIL(vt_Cmd202_Unknown);
    extern const GccVTableEntry g_ScaleModelNodeCommandVTable[] RETAIL(vt_Cmd203_Unknown);
    extern const GccVTableEntry g_SetFocusPositionOffsetCommandVTable[] RETAIL(vt_Cmd204_Unknown);
    extern const GccVTableEntry g_SetFocusPositionAtAngleCommandVTable[] RETAIL(vt_Cmd205_Unknown);
    extern const GccVTableEntry g_SaveScriptStateCommandVTable[] RETAIL(vt_Cmd206_Unknown);
    extern const GccVTableEntry g_ClearSavedScriptStateCommandVTable[] RETAIL(vt_Cmd207_Unknown);
    extern const GccVTableEntry g_SetNodeByte8cCommandVTable[] RETAIL(vt_Cmd208_Unknown);
    extern const GccVTableEntry g_SetGlobalByte30a0e9CommandVTable[] RETAIL(vt_Cmd209_Unknown);
    extern const GccVTableEntry g_TriggerInstancesInRangeCommandVTable[] RETAIL(vt_Cmd210_Unknown);
    extern const GccVTableEntry g_MarkTimeCommandVTable[] RETAIL(vt_Cmd211_Unknown);
    extern const GccVTableEntry g_ClearMarkedTimeCommandVTable[] RETAIL(vt_Cmd212_Unknown);
    extern const GccVTableEntry g_KeyOfPath34Op213CommandVTable[] RETAIL(vt_Cmd213_Unknown);
    extern const GccVTableEntry g_ControllerRumbleCommandVTable[] RETAIL(vt_Cmd214_ControllerRumble);
    extern const GccVTableEntry g_SetSoundParamsCommandVTable[] RETAIL(vt_Cmd215_Unknown);
    extern const GccVTableEntry g_CreateCrateContentsCommandVTable[] RETAIL(CreateCrateContents_VTable);
    extern const GccVTableEntry g_CA_PickUpWumpaCommandVTable[] RETAIL(vt_Cmd513_CA_PickUpWumpa);
    extern const GccVTableEntry g_CreateDamageCommandVTable[] RETAIL(vt_Cmd514_CreateDamage);
    extern const GccVTableEntry g_SetAgentCommandVTable[] RETAIL(vt_Cmd515_SetAgent);
    extern const GccVTableEntry g_SetPlayerRespawnPositionCommandVTable[] RETAIL(VT_SetPlayerRespawnPosition);
    extern const GccVTableEntry g_ResetGameCommandVTable[] RETAIL(VT_ResetGame);
    extern const GccVTableEntry g_SetCrateCommandVTable[] RETAIL(vt_Cmd518_SetCrate);
    extern const GccVTableEntry g_TriggerBalancedCrateFallingCommandVTable[] RETAIL(vt_Cmd519_TriggerBalancedCrateFalling);
    extern const GccVTableEntry g_CA_PickUpHealthCommandVTable[] RETAIL(vt_Cmd520_CA_PickUpHealth);
    extern const GccVTableEntry g_SetPlayerInputCommandVTable[] RETAIL(vt_Cmd521_SetPlayerInput);
    extern const GccVTableEntry g_TriggerAllNitroCratesCommandVTable[] RETAIL(vt_Cmd522_TriggerAllNitroCrates);
    extern const GccVTableEntry g_ApplyVelocityCommandVTable[] RETAIL(vt_Cmd523_ApplyVelocity);
    extern const GccVTableEntry g_SetKeyNearestPlayerCommandVTable[] RETAIL(vt_Cmd524_SetKeyNearestPlayer);
    extern const GccVTableEntry g_RaycastFocusPositionCommandVTable[] RETAIL(vt_Cmd525_RaycastFocusPosition);
    extern const GccVTableEntry g_ApplyVelocityToSelfCommandVTable[] RETAIL(vt_Cmd526_ApplyVelocityToSelf);
    extern const GccVTableEntry g_SetChiChiGrassCommandVTable[] RETAIL(vt_Cmd527_SetChiChiGrass);
    extern const GccVTableEntry g_ReduceHitPointsCommandVTable[] RETAIL(vt_Cmd528_ReduceHitPoints);
    extern const GccVTableEntry g_SetHitPointsCommandVTable[] RETAIL(vt_Cmd529_SetHitPoints);
    extern const GccVTableEntry g_DUMMY_SetRayTestsCommandVTable[] RETAIL(vt_Cmd530_DUMMY_SetRayTests);
    extern const GccVTableEntry g_DUMMY_NowGoForwardCollidableCommandVTable[] RETAIL(vt_Cmd532_DUMMY_NowGoForwardCollidable);
    extern const GccVTableEntry g_NowGoBackCollidableCommandVTable[] RETAIL(vt_Cmd533_NowGoBackCollidable);
    extern const GccVTableEntry g_SetGlobalProgressionCommandVTable[] RETAIL(vt_Cmd534_SetGlobalProgression);
    extern const GccVTableEntry g_AddCrystalCommandVTable[] RETAIL(vt_Cmd535_AddCrystal);
    extern const GccVTableEntry g_DUMMY_536CommandVTable[] RETAIL(vt_Cmd536_DUMMY_536);
    extern const GccVTableEntry g_AddGemCommandVTable[] RETAIL(vt_Cmd537_AddGem);
    extern const GccVTableEntry g_DUMMY_538CommandVTable[] RETAIL(vt_Cmd538_DUMMY_538);
    extern const GccVTableEntry g_CA_SetPickupCommandVTable[] RETAIL(vt_Cmd539_CA_SetPickup);
    extern const GccVTableEntry g_CA_SetProjectileCommandVTable[] RETAIL(vt_Cmd540_CA_SetProjectile);
    extern const GccVTableEntry g_ShootCommandVTable[] RETAIL(vt_Cmd548_Shoot);
    extern const GccVTableEntry g_GetShortRouteCommandVTable[] RETAIL(vt_Cmd549_GetShortRoute);
    extern const GccVTableEntry g_DUMMY_FuelPayGateCommandVTable[] RETAIL(vt_Cmd550_DUMMY_FuelPayGate);
    extern const GccVTableEntry g_OpenAllLinkedFurnitureCommandVTable[] RETAIL(vt_Cmd551_OpenAllLinkedFurniture);
    extern const GccVTableEntry g_CloseAllLinkedFurnitureCommandVTable[] RETAIL(vt_Cmd552_CloseAllLinkedFurniture);
    extern const GccVTableEntry g_AttachAllLinkedAgentsCommandVTable[] RETAIL(vt_Cmd553_AttachAllLinkedAgents);
    extern const GccVTableEntry g_DetachAllLinkedAgentsCommandVTable[] RETAIL(vt_Cmd554_DetachAllLinkedAgents);
    extern const GccVTableEntry g_SetVehicleHumiliskateCommandVTable[] RETAIL(vt_Cmd555_SetVehicleHumiliskate);
    extern const GccVTableEntry g_SetFocusToPlayerCommandVTable[] RETAIL(vt_Cmd556_SetFocusToPlayer);
    extern const GccVTableEntry g_RequestFocusCommandVTable[] RETAIL(RequestFocusCommand_Methods);
    extern const GccVTableEntry g_SetFocusPropertiesCommandVTable[] RETAIL(vt_Cmd558_SetFocusProperties);
    extern const GccVTableEntry g_SetCameraCommandVTable[] RETAIL(vt_Cmd559_SetCamera);
    extern const GccVTableEntry g_RestoreCameraDefaultsCommandVTable[] RETAIL(vt_Cmd560_RestoreCameraDefaults);
    extern const GccVTableEntry g_LinkToFocusCharacterCommandVTable[] RETAIL(vt_Cmd561_LinkToFocusCharacter);
    extern const GccVTableEntry g_UnlinkCharactersCommandVTable[] RETAIL(vt_Cmd562_UnlinkCharacters);
    extern const GccVTableEntry g_DamageOriginatorCommandVTable[] RETAIL(vt_Cmd563_DamageOriginator);
    extern const GccVTableEntry g_SetAgentRef1ToPlayerCommandVTable[] RETAIL(vt_Cmd566_Unknown);
    extern const GccVTableEntry g_SetFocusPositionToPlayerCommandVTable[] RETAIL(vt_Cmd567_Unknown);
    extern const GccVTableEntry g_DUMMY_568CommandVTable[] RETAIL(vt_Cmd568_Unknown);
    extern const GccVTableEntry g_ExitVehicleModeCommandVTable[] RETAIL(vt_Cmd569_ExitVehicleMode);
    extern const GccVTableEntry g_SetVehicleRollerbrawlCommandVTable[] RETAIL(vt_Cmd570_SetVehicleRollerbrawl);
    extern const GccVTableEntry g_SetVehicleHoverboardCommandVTable[] RETAIL(vt_Cmd571_SetVehicleHoverboard);
    extern const GccVTableEntry g_SetMotionCommandVTable[] RETAIL(vt_Cmd572_Unknown);
    extern const GccVTableEntry g_SetNearestPointFlagsCommandVTable[] RETAIL(vt_Cmd573_Unknown);
    extern const GccVTableEntry g_CreateHeadTrackingCommandVTable[] RETAIL(vt_Cmd574_Unknown);
    extern const GccVTableEntry g_SetFocusPositionToNearestPointCommandVTable[] RETAIL(vt_Cmd575_Unknown);
    extern const GccVTableEntry g_SetFocusToGameActorCommandVTable[] RETAIL(vt_Cmd576_Unknown);
    extern const GccVTableEntry g_BecomeStickyCommandVTable[] RETAIL(vt_Cmd577_BecomeSticky);
    extern const GccVTableEntry g_CharacterOp578CommandVTable[] RETAIL(vt_Cmd578_Unknown);
    extern const GccVTableEntry g_CounterPositionOp579CommandVTable[] RETAIL(vt_Cmd579_Unknown);
    extern const GccVTableEntry g_ApplyVelocityToHeldBodyCommandVTable[] RETAIL(vt_Cmd580_Unknown);
    extern const GccVTableEntry g_BecomeNormalCommandVTable[] RETAIL(vt_Cmd581_BecomeNormal);
    extern const GccVTableEntry g_AddPerceptionCommandVTable[] RETAIL(vt_Cmd582_Unknown);
    extern const GccVTableEntry g_CutsceneCameraOp583CommandVTable[] RETAIL(vt_Cmd583_Unknown);
    extern const GccVTableEntry g_DUMMY_584CommandVTable[] RETAIL(vt_Cmd584_DUMMY_584);
    extern const GccVTableEntry g_DUMMY_586CommandVTable[] RETAIL(vt_Cmd586_DUMMY_586);
    extern const GccVTableEntry g_SetObjectFlags587CommandVTable[] RETAIL(vt_Cmd587_Unknown);
    extern const GccVTableEntry g_PlayerFaceTowardsCameraCommandVTable[] RETAIL(vt_Cmd588_PlayerFaceTowardsCamera);
    extern const GccVTableEntry g_CutsceneStartCommandVTable[] RETAIL(vt_Cmd589_CutsceneStart);
    extern const GccVTableEntry g_CutsceneEndCommandVTable[] RETAIL(vt_Cmd590_CutsceneEnd);
    extern const GccVTableEntry g_CutsceneCameraMoveCommandVTable[] RETAIL(vt_Cmd591_Unknown);
    extern const GccVTableEntry g_CameraSaveParamsCommandVTable[] RETAIL(vt_Cmd592_CameraSaveParams);
    extern const GccVTableEntry g_ToggleCutsceneCameraCommandVTable[] RETAIL(vt_Cmd594_ToggleCutsceneCamera);
    extern const GccVTableEntry g_CutsceneCameraTargetsCommandVTable[] RETAIL(vt_Cmd595_Unknown);
    extern const GccVTableEntry g_StartWhackawormCommandVTable[] RETAIL(vt_Cmd596_StartWhackaworm);
    extern const GccVTableEntry g_ProgressWhackawormCommandVTable[] RETAIL(vt_Cmd597_ProgressWhackaworm);
    extern const GccVTableEntry g_EndWhackawormCommandVTable[] RETAIL(vt_Cmd598_EndWhackaworm);
    extern const GccVTableEntry g_ReleasePlayerHoldCommandVTable[] RETAIL(vt_Cmd599_Unknown);
    extern const GccVTableEntry g_WarpToChunkLinkTowardsPlayerCommandVTable[] RETAIL(vt_Cmd600_Unknown);
    extern const GccVTableEntry g_SetVehicleWrestleCreatureCommandVTable[] RETAIL(vt_Cmd601_SetVehicleWrestleCreature);
    extern const GccVTableEntry g_FadeoutScreenCommandVTable[] RETAIL(vt_Cmd602_FadeoutScreen);
    extern const GccVTableEntry g_DisplayBottomTextCommandVTable[] RETAIL(vt_Cmd603_DisplayBottomText);
    extern const GccVTableEntry g_ResetCharacterFallCommandVTable[] RETAIL(vt_Cmd604_Unknown);
    extern const GccVTableEntry g_DismissCharacterCommandVTable[] RETAIL(vt_Cmd605_DismissCharacter);
    extern const GccVTableEntry g_CameraFocusObjectCommandVTable[] RETAIL(vt_Cmd606_CameraFocusObject);
    extern const GccVTableEntry g_CameraStopFocusObjectCommandVTable[] RETAIL(vt_Cmd607_CameraStopFocusObject);
    extern const GccVTableEntry g_ClearBottomTextCommandVTable[] RETAIL(vt_Cmd608_ClearBottomText);
    extern const GccVTableEntry g_DUMMY_609CommandVTable[] RETAIL(vt_Cmd609_DUMMY_609);
    extern const GccVTableEntry g_DUMMY_610CommandVTable[] RETAIL(vt_Cmd610_DUMMY_610);
    extern const GccVTableEntry g_SetCharacterHomeChunkCommandVTable[] RETAIL(vt_Cmd611_Unknown);
    extern const GccVTableEntry g_GameControllerOp612CommandVTable[] RETAIL(vt_Cmd612_Unknown);
    extern const GccVTableEntry g_DisablePlayerControlCommandVTable[] RETAIL(vt_Cmd613_DisablePlayerControl);
    extern const GccVTableEntry g_SetNode120FlagCommandVTable[] RETAIL(vt_Cmd614_Unknown);
    extern const GccVTableEntry g_SetPlayerFlag57CommandVTable[] RETAIL(vt_Cmd615_Unknown);
    extern const GccVTableEntry g_PlaceCharacterInChunkCommandVTable[] RETAIL(vt_Cmd616_Unknown);
    extern const GccVTableEntry g_HitInstancesInBoxesCommandVTable[] RETAIL(vt_Cmd617_Unknown);
    extern const GccVTableEntry g_ForceGameOverCommandVTable[] RETAIL(vt_Cmd618_ForceGameOver);
    extern const GccVTableEntry g_ShowBottomTextCommandVTable[] RETAIL(vt_Cmd619_ShowBottomText);
    extern const GccVTableEntry g_HideBottomTextCommandVTable[] RETAIL(vt_Cmd620_HideBottomText);
    extern const GccVTableEntry g_SetFocusToCameraTargetCommandVTable[] RETAIL(vt_Cmd621_Unknown);
    extern const GccVTableEntry g_CharacterSoundProxyCommandVTable[] RETAIL(D_002EEE88);
    extern const GccVTableEntry g_CameraNodeSetTargetCommandVTable[] RETAIL(vt_Cmd623_Unknown);
    extern const GccVTableEntry g_EnableVarPercept629CommandVTable[] RETAIL(vt_Cmd624_EnableVarPercept629);
    extern const GccVTableEntry g_SwitchCharacterCommandVTable[] RETAIL(vt_Cmd625_SwitchCharacter);
    extern const GccVTableEntry g_CameraNodeEnableFlagsCommandVTable[] RETAIL(vt_Cmd626_Unknown);
    extern const GccVTableEntry g_CameraNodeClearFlagsCommandVTable[] RETAIL(vt_Cmd627_Unknown);
    extern const GccVTableEntry g_SetCameraNodeValueCommandVTable[] RETAIL(vt_Cmd628_Unknown);
    extern const GccVTableEntry g_SetCameraNodeValuesCommandVTable[] RETAIL(vt_Cmd629_Unknown);
    extern const GccVTableEntry g_SetNode5FlagsCommandVTable[] RETAIL(vt_Cmd630_Unknown);
    extern const GccVTableEntry g_SetPlayerVehicleValueCommandVTable[] RETAIL(vt_Cmd631_Unknown);
    extern const GccVTableEntry g_SetLinkedObjectNearestPlayerCommandVTable[] RETAIL(vt_Cmd632_Unknown);
    extern const GccVTableEntry g_SetPlayerModeCommandVTable[] RETAIL(vt_Cmd633_SetPlayerMode);
    extern const GccVTableEntry g_PlayMovieCommandVTable[] RETAIL(vt_Cmd634_PlayMovie);
    extern const GccVTableEntry g_AddAmmoCommandVTable[] RETAIL(vt_Cmd636_AddAmmo);
    extern const GccVTableEntry g_LinkedObjectNearestPlayerOp637CommandVTable[] RETAIL(vt_Cmd637_Unknown);
    extern const GccVTableEntry g_EnableBossModeCommandVTable[] RETAIL(vt_Cmd638_EnableBossMode);
    extern const GccVTableEntry g_DamageBossCommandVTable[] RETAIL(vt_Cmd639_DamageBoss);
    extern const GccVTableEntry g_ExitBossModeCommandVTable[] RETAIL(vt_Cmd640_ExitBossMode);
    extern const GccVTableEntry g_FinalBossInitWeaponsCommandVTable[] RETAIL(vt_Cmd641_Unknown);
    extern const GccVTableEntry g_CreateNodeControllerCommandVTable[] RETAIL(vt_Cmd645_Unknown);
    extern const GccVTableEntry g_RequestOgiSlotCommandVTable[] RETAIL(vt_Cmd646_Unknown);
    extern const GccVTableEntry g_SetGlobalProgression2CommandVTable[] RETAIL(vt_Cmd647_SetGlobalProgression2);
    extern const GccVTableEntry g_SetNodeValue174CommandVTable[] RETAIL(vt_Cmd648_Unknown);
    extern const GccVTableEntry g_ClearNodeValue174CommandVTable[] RETAIL(vt_Cmd649_Unknown);
    extern const GccVTableEntry g_SetCharacterFlag2CommandVTable[] RETAIL(vt_Cmd650_Unknown);
    extern const GccVTableEntry g_ClearCharacterFlag2CommandVTable[] RETAIL(vt_Cmd651_Unknown);
    extern const GccVTableEntry g_CameraTopdownModeCommandVTable[] RETAIL(vt_Cmd652_CameraTopdownMode);
    extern const GccVTableEntry g_PlayCreditsCommandVTable[] RETAIL(vt_Cmd654_PlayCredits);
    extern const GccVTableEntry g_SetMaskControllerIdsCommandVTable[] RETAIL(vt_Cmd655_Unknown);
    extern const GccVTableEntry g_ResetMaskControllerCommandVTable[] RETAIL(vt_Cmd656_Unknown);
    extern const GccVTableEntry g_DisplayBottomTextInstanceCommandVTable[] RETAIL(vt_Cmd657_DisplayBottomTextInstance);
    extern const GccVTableEntry g_SetSplineControllerValuesCommandVTable[] RETAIL(vt_Cmd658_Unknown);
    extern const GccVTableEntry g_TriggerCharacterEvent12CommandVTable[] RETAIL(vt_Cmd659_Unknown);
    extern const GccVTableEntry g_ClearPlayerFlag14CommandVTable[] RETAIL(vt_Cmd660_Unknown);
    extern const GccVTableEntry g_SetSkateControllerIdsCommandVTable[] RETAIL(vt_Cmd661_Unknown);
    extern const GccVTableEntry g_ResetCameraCommandVTable[] RETAIL(vt_Cmd662_ResetCamera);
}

