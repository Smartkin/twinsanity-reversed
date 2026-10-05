#pragma once

#include "common.h"
#include "game/agentlab.h"
#include "game/behaviours.h"
#include "game/chunkdata.h"
#include "game/nodecontrollers.h"
#include "game/objectnode.h"
#include "game/pickups.h"
#include "game/properties.h"

class GameNode;
struct ScriptToken;
struct ScriptTokenList;
struct GameAnimation;
struct OgiAnimator;
struct TimeClock;

// The script commands the builder makes (generated from the retail builder and TT Lab's AgentLabDefsPS2.json, whose names
// they had, and named by what they do where their code shows otherwise): the base's bits, next command and vtable, then their
// arguments as a script has them, which the reader copies over the object whole (game/agentlab.h). Arguments nothing reads
// are unusedN (N the argument's number), what only the development tools' parsers (commandtokens*.cpp) write included

// An argument whose bit 0 switches something on (the commands name it by what it switches)
union SwitchArgument
{
    u32 value;
    struct
    {
        u32 on : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(SwitchArgument, 4);

// 1, 541: a particle trail the node leaves (its trails keep the command's trail arguments)
class AddTrailCommand : public ScriptCommand
{
public:
    f32 unused1;
    TrailArguments trail;

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

// A command's argument word that only names a designator (its low byte, game/objectnode.h's Designator)
union DesignatorArgument
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(DesignatorArgument, 4);

// A command's argument word that only names a DesignatorSlot
union DesignatorSlotArgument
{
    u32 value;
    struct
    {
        u32 slot : 3;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(DesignatorSlotArgument, 4);

// 3, 105
class PositionWarpCommand : public ScriptCommand
{
public:
    // Where the instance warps to: a receiver's position, else a designator's, else the space's (game/objectnode.h's
    // DesignatedPosition, the offset added when it's given; unused20 is handed to its unused argument), or the last contact's
    // point (moved by the offset in the instance's frame); turning toward it instead of moving there, its body too
    union Target
    {
        u32 value;
        struct
        {
            u32 receiver : 8;
            u32 designator : 8;
            u32 space : 4;
            u32 unused20 : 1;
            u32 offsetGiven : 1;
            u32 turns : 1;
            u32 turnsBody : 1;
            u32 toContact : 1;
            u32 unused25 : 7;
        };
    };

    Target target;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd3_PositionWarp_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd3_PositionWarp_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd3_PositionWarp_Execute);
    u32 Size() RETAIL(Cmd3_PositionWarp_GetSize);
};
CHECK_SIZE(PositionWarpCommand, 0x20);

// SetKey's key: its index (Waypoints::NoKey none), else a random one of a range (a different one from the current key with
// noRepeat), the one of the range nearest the instance or the last key (game/commandsmotion.cpp). The development tools' parser
// sets bit 24, which nothing reads
union KeyChoice
{
    u32 value;
    struct
    {
        u32 index : 8;
        u32 rangeStart : 8;
        u32 rangeEnd : 8;
        u32 unused24 : 1;
        u32 noRepeat : 1;
        u32 nearest : 1;
        u32 random : 1;
        u32 last : 1;
        u32 unused29 : 3;
    };
};
CHECK_SIZE(KeyChoice, 4);

// 4: the key the agent's waypoints are at
class SetKeyCommand : public ScriptCommand
{
public:
    KeyChoice key;

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

// SpawnResidentAgent's object (its ID's resource index, unless the flags take the agent's own or AgentRef2's), the designator
// whose position it's made at, and the distance within which its node's updates aren't thinned out (the square root of
// GameNode::nearDistance: 0 leaves the node's own, AnyDistance never thins them)
union SpawnedObject
{
    static constexpr u8 AnyDistance = 0xFF;

    u32 value;
    struct
    {
        u32 id : 16;
        u32 designator : 8;
        u32 nearDistance : 8;
    };
};
CHECK_SIZE(SpawnedObject, 4);

// SpawnResidentAgent's 64 bits (the retail code reads them whole; game/commandsmotion.cpp says what each does): the trigger message
// the instance is sent (NoMessage none), the exit point it's made at (0xFF none), the space it's placed in, then
// how it's made and placed and what it's given of the agent's
union SpawnFlags
{
    u64 value;
    struct
    {
        u64 message : 16;
        u64 exitPoint : 8;
        u64 space : 5;
        u64 unsignalled : 1;
        u64 offsetGiven : 1;
        u64 atAgent : 1;
        u64 linksToAgent : 1;
        u64 atFocusPosition : 1;
        u64 atFocus : 1;
        u64 turnedBySpace : 1;
        u64 ownObject : 1;
        u64 becomesFocus : 1;
        u64 becomesAgentRef2 : 1;
        u64 agentsId : 1;
        u64 agentRef2Object : 1;
        u64 linksSpawner : 1;
        u64 sharesAgentRef1 : 1;
        u64 sharesAgentRef2 : 1;
        u64 sharesFocus : 1;
        u64 sharesStoredPosition : 1;
        u64 sharesLinked : 1;
        u64 subtypeGiven : 1;
        u64 atStoredPosition : 1;
        u64 marksBusy : 1;
        u64 clearsBusy : 1;
        u64 sharesKeys : 1;
        u64 sharesPaths : 1;
        u64 unused53 : 11;
    };
};
CHECK_SIZE(SpawnFlags, 8);

// 8, 112: an instance made for the agent
class SpawnResidentAgentCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    TaggedValue subtype;
    SpawnedObject object;
    SpawnFlags flags;

    static SpawnResidentAgentCommand* Construct(SpawnResidentAgentCommand* command, u32 mode) RETAIL(FUN_00225540);
    void Destroy(u32 destroyFlags) RETAIL(SpawnResidentAgentCommand_dtor);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd8_SpawnResidentAgent_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd8_SpawnResidentAgent_Execute);
    u32 Size() RETAIL(Cmd8_SpawnResidentAgent_GetSize);
};
CHECK_SIZE(SpawnResidentAgentCommand, 0x30);

// DoAnimation's flags: how many of its slots it picks from, the joint whose chain plays it (0xFF the root), it loops, a blend
// time given, a speed given (else 1, or with speedIsDuration how long it lasts), a random part of the speed given, it starts part
// way (at startPosition), where the one before it is, backward, queued even after the same one, and the instance's shadow given
// a slot
union DoAnimationFlags
{
    u32 value;
    struct
    {
        u32 slotCount : 4;
        u32 joint : 8;
        u32 loops : 1;
        u32 blendTimeGiven : 1;
        u32 speedGiven : 1;
        u32 speedIsDuration : 1;
        u32 speedRandomGiven : 1;
        u32 startsPartWay : 1;
        u32 continues : 1;
        u32 backward : 1;
        u32 queuedAlways : 1;
        u32 shadowGiven : 1;
        u32 shadowSlot : 4;
        u32 unused26 : 6;
    };
};
CHECK_SIZE(DoAnimationFlags, 4);

// 9, 543
class DoAnimationCommand : public ScriptCommand
{
public:
    DoAnimationFlags flags;
    TaggedValue blendTime;
    TaggedValue speed;
    TaggedValue speedRandom;
    f32 startPosition;
    // The object's animation slots it picks from (a count past 4 reads past them, as retail does)
    u8 slots[4];

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

// What DoParticle plays: the system (an index into the particle systems' table), the emitter's value, it leaves from its
// instance's frame (unless a key or a designator's position is taken) or from an exit point's, and the exit point (0x3F none)
union ParticleEmission
{
    u32 value;
    struct
    {
        u32 system : 16;
        u32 emitterValue : 7;
        u32 fromFrame : 1;
        u32 fromExitPoint : 1;
        u32 exitPoint : 6;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(ParticleEmission, 4);

// Where DoParticle plays: the frame turned with the instance, its axes mode (game/instanceparticles.h's ParticleFrameAxes), a
// position given, a designator whose position it is (DesignatesNone none), the surface mode (NoSurfaceMode none) and a key of
// the waypoints (Waypoints::NoKey none)
union ParticlePlacement
{
    static constexpr u8 NoSurfaceMode = 0xFF;

    u32 value;
    struct
    {
        u32 turned : 1;
        u32 axes : 4;
        u32 hasPosition : 1;
        u32 designator : 8;
        u32 surfaceMode : 8;
        u32 key : 8;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(ParticlePlacement, 4);

// 10, 544
class DoParticleCommand : public ScriptCommand
{
public:
    ParticleEmission emission;
    ParticlePlacement placement;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    Vector4 position;

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

// DoSound's flags: how many of its slots it picks from, the sound's group, played without a place (or following the instance,
// with followed or tracked), a random part of the pitch and of the volume given, a pitch and a volume given, the camera shaken,
// the contact kind with the node's surface whose sound it plays (game/collision.h's GetSurfaceSound: 0 impact, 1 and 2 steps, 3
// land, 4 hard impact, 5 scrape; 0xF its slots), the sound kept in the node's tracked sound slot
union DoSoundFlags
{
    static constexpr u32 SlotsKind = 0xF;

    u32 value;
    struct
    {
        u32 slotCount : 4;
        u32 unused4 : 4;
        u32 group : 3;
        u32 unplaced : 1;
        u32 followed : 1;
        u32 unused13 : 1;
        u32 randomPitch : 1;
        u32 randomVolume : 1;
        u32 pitchGiven : 1;
        u32 volumeGiven : 1;
        u32 shakes : 1;
        u32 contactKind : 4;
        u32 tracked : 1;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(DoSoundFlags, 4);

// 11, 545
class DoSoundCommand : public ScriptCommand
{
public:
    DoSoundFlags flags;
    u16 soundSlots[8];
    TaggedValue volume;
    f32 unused7;
    f32 pitch;
    f32 pitchRandom;
    f32 volumeRandom;
    u32 unused11;
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

// 12: the node follows the motion block of its arguments (cycles moving or turning the instance)
class SetWobbleCommand : public ScriptCommand
{
public:
    MotionBlock block;

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
    // How far it goes a second
    TaggedValue speed;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd14_NowMoveForwards_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd14_NowMoveForwards_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd14_NowMoveForwards_Dtor);
    u32 Size() RETAIL(Cmd14_NowMoveForwards_GetSize);
};
CHECK_SIZE(NowMoveForwardsCommand, 0x10);

// 15: nothing (the instance hanging on its holder's place is looked up when it holds others, and dropped)
class NoOpNowMoveBackwardsCommand : public ScriptCommand
{
public:
    TaggedValue unused1;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd15_NowMoveBackwards_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd15_NowMoveBackwards_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd15_NowMoveBackwards_Dtor);
    u32 Size() RETAIL(Cmd15_NowMoveBackwards_GetSize);
};
CHECK_SIZE(NoOpNowMoveBackwardsCommand, 0x10);

// 16
class NowStrafeLeftCommand : public ScriptCommand
{
public:
    // How far it goes a second
    TaggedValue speed;

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
    // How far it goes a second
    TaggedValue speed;

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
    // An angle a second
    TaggedValue turnRate;

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
    TaggedValue turnRate;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_00215fd0);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(func_00222B50);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002228e0);
    u32 Size() RETAIL(FUN_00222928);
};
CHECK_SIZE(NowTurnRightCommand, 0x10);

// Which instance a command takes (its bits 0-2, a DesignatorSlot): the focus, AgentRef1, AgentRef2 (none for the others)
union LinkedTarget
{
    u32 value;
    struct
    {
        u32 kind : 3;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(LinkedTarget, 4);

// 23, 24: the focus, AgentRef1 or AgentRef2 linked to the instance (its attachments made when it has none)
class LinkTargetCommand : public ScriptCommand
{
public:
    LinkedTarget target;

    void Destroy(u32 destroyFlags) RETAIL(Cmd23_NowRotateJoint_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd23_NowRotateJoint_Execute);
    u32 Size() RETAIL(Cmd23_NowRotateJoint_GetSize);
};
CHECK_SIZE(LinkTargetCommand, 0x10);

// A command's target (game/objectnode.h's DesignatedPosition and SpaceOfRequest): a receiver of the starter (0xFF none), a
// designator (0xFF none), else a space, and a bit the target's search is handed and doesn't read; bit 21 says an offset is given
// (the commands that take one)
union TargetRequest
{
    u32 value;
    struct
    {
        u32 receiver : 8;
        u32 designator : 8;
        u32 space : 4;
        u32 unused20 : 1;
        u32 offsetGiven : 1;
        u32 unused22 : 10;
    };
};
CHECK_SIZE(TargetRequest, 4);

// 27: the node's stored place is the target's place (none when the request gives none; its space and bit 20 unread)
class StoreCurrentSpaceCommand : public ScriptCommand
{
public:
    TargetRequest request;

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
    // The key (its index, or the current or the next one: DesignatesCurrentKey, DesignatesNextKey) of the waypoints of the focus
    // instance, of AgentRef1 or else the agent's own, and what its position is given to (SlotFocus or SlotStoredPosition)
    union Target
    {
        u32 value;
        struct
        {
            u32 key : 8;
            u32 unused8 : 1;
            u32 slot : 3;
            u32 focusKeys : 1;
            u32 agentRef1Keys : 1;
            u32 unused14 : 18;
        };
    };

    Target target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd28_SetFocusToKey_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd28_SetFocusToKey_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd28_SetFocusToKey_Execute);
    u32 Size() RETAIL(Cmd28_SetFocusToKey_GetSize);
};
CHECK_SIZE(SetFocusToKeyCommand, 0x10);

// RotationWarp's and RotateAgent's word: the space the rotation is in (ControlPacket::Space: the world, the start, the instance's
// own place, the stored place; nothing in the others), whether a rotation is given (the instance's own place needs one), and the
// instance rolled level after. The development tools' parser writes a receiver (bits 0-7), a designator (8-15) and bit 20 as
// for PositionWarp, which nothing reads
union RotationWarpBits
{
    u32 value;
    struct
    {
        u32 unused0 : 8;
        u32 unused8 : 8;
        u32 space : 4;
        u32 unused20 : 1;
        u32 rotationGiven : 1;
        u32 levels : 1;
        u32 unused23 : 9;
    };
};
CHECK_SIZE(RotationWarpBits, 4);

// 29: the agent's instance turned to a rotation (a quaternion)
class RotationWarpCommand : public ScriptCommand
{
public:
    RotationWarpBits warp;
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

// 31: whether the same starter queued again restarts the running one
class SetRestartableCommand : public ScriptCommand
{
public:
    SwitchArgument restartable;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd31_ClearThreats_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd31_ClearThreats_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd31_ClearThreats_Execute);
    u32 Size() RETAIL(Cmd31_ClearThreats_GetSize);
};
CHECK_SIZE(SetRestartableCommand, 0x10);

// 33: every linked object's agent sent the trigger event, from the instance or from the runner's originator
class TriggerLinkedObjectsCommand : public ScriptCommand
{
public:
    SwitchArgument fromOriginator;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd33_TriggerLinkedObjects_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd33_TriggerLinkedObjects_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd33_TriggerLinkedObjects_Execute);
    u32 Size() RETAIL(Cmd33_TriggerLinkedObjects_GetSize);
};
CHECK_SIZE(TriggerLinkedObjectsCommand, 0x10);

// 34: the agent's persistent flag set (in its chunk's own store or the other one)
class SetStateCommand : public ScriptCommand
{
public:
    // What the flag is set to (the development tools' parser writes 1 for On)
    union State
    {
        u32 value;
        struct
        {
            u32 flag : 8;
            u32 unused8 : 24;
        };
    };

    State state;

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

// SetLogicalRadius's bits: the radius made half the largest side of the instance's box
union LogicalRadiusBits
{
    u32 value;
    struct
    {
        u32 fromBox : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(LogicalRadiusBits, 4);

// 40: the node's roll radius and its physics sphere's radius
class SetLogicalRadiusCommand : public ScriptCommand
{
public:
    LogicalRadiusBits flags;
    f32 radius;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd40_SetLogicalRadius_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd40_SetLogicalRadius_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd40_SetLogicalRadius_Execute);
    u32 Size() RETAIL(Cmd40_SetLogicalRadius_GetSize);
};
CHECK_SIZE(SetLogicalRadiusCommand, 0x14);

// SetBehaviourPriority's priority (past 100 the starter's own)
union PriorityArgument
{
    u32 value;
    struct
    {
        u32 priority : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(PriorityArgument, 4);

// 42: the starter's priority (past 100 the starter's own)
class SetBehaviourPriorityCommand : public ScriptCommand
{
public:
    PriorityArgument priority;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd42_SetBehaviourPriority_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd42_SetBehaviourPriority_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd42_SetBehaviourPriority_Execute);
    u32 Size() RETAIL(Cmd42_SetBehaviourPriority_GetSize);
};
CHECK_SIZE(SetBehaviourPriorityCommand, 0x10);

// SetCollisions' settings: the kinds of the rigid body's motion and of its collisions (ObjectRigidBodyBits) and whether each is
// given, the values given (the roll radius, gravity, drag, friction, restitution, size, centre of mass, spin friction and length
// drag), its launch, and what it sets of the rigid body's bits and state of the same names: it doesn't move for others, it steers
// itself, its size is the least slope it stands on and its restitution the rate its contact normal follows (given alone), its
// impulses capped or fixed at the impulse length, the volume controllers push it; the roll radius made half the largest side of
// the instance's box, or half its height (the width scale its half width or depth over that). Bits 25 and 26 go to state bits
// nothing reads
union CollisionSettings
{
    u32 value;
    struct
    {
        u32 motionKind : 4;
        u32 collisionKind : 4;
        u32 motionKindGiven : 1;
        u32 collisionKindGiven : 1;
        u32 rollRadiusGiven : 1;
        u32 gravityGiven : 1;
        u32 immovable : 1;
        u32 dragGiven : 1;
        u32 frictionGiven : 1;
        u32 restitutionGiven : 1;
        u32 sizeGiven : 1;
        u32 launch : 2;
        u32 centerOfMassGiven : 1;
        u32 steersItself : 1;
        u32 slopeLimited : 1;
        u32 ownNormalRate : 1;
        u32 spinFrictionGiven : 1;
        u32 lengthDragGiven : 1;
        u32 unused25 : 1;
        u32 unused26 : 1;
        u32 rollRadiusFromBox : 1;
        u32 rollRadiusFromHeight : 1;
        u32 impulseCapped : 1;
        u32 impulseFixed : 1;
        u32 pushedByVolumes : 1;
    };
};
CHECK_SIZE(CollisionSettings, 4);

// 44: the node's roll radius and its rigid body's kinds and values
class SetCollisionsCommand : public ScriptCommand
{
public:
    f32 unused1;
    f32 centerOfMassX;
    f32 centerOfMassY;
    f32 centerOfMassZ;
    f32 centerOfMassW;
    CollisionSettings settings;
    f32 rollRadius;
    f32 drag;
    f32 lengthDrag;
    f32 friction;
    f32 restitution;
    f32 size;
    f32 spinFriction;
    f32 widthScale;
    f32 impulseLength;
    TaggedValue gravity;
    u32 unused17;

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
    // The designator only this command answers: the instance the agent's own hangs from
    static constexpr u8 DesignatesParent = 0xDE;

    // The designator whose instance is given to a DesignatorSlot
    union Target
    {
        u32 value;
        struct
        {
            u32 designator : 8;
            u32 slot : 3;
            u32 unused11 : 21;
        };
    };

    Target target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd45_SetFocusToAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd45_SetFocusToAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd45_SetFocusToAgent_Execute);
    u32 Size() RETAIL(Cmd45_SetFocusToAgent_GetSize);
};
CHECK_SIZE(SetFocusToAgentCommand, 0x10);

// How AttachFocusObject attaches: the target it takes (a DesignatorSlot), the exit point (0xFF none), how the attached
// instance follows its holder (game/attachment.h's Attachment::Bits), an offset and angles given, a linked object's index (0xF:
// the target), the attached instance marked busy, and the instance attached to the focus instead. Bit 21 is only the
// development tools' parser's
union AttachFocusFlags
{
    static constexpr u32 NoLinked = 0xF;

    u32 value;
    struct
    {
        u32 target : 3;
        u32 exitPoint : 8;
        u32 follow : 4;
        u32 offsetGiven : 1;
        u32 anglesGiven : 1;
        u32 linked : 4;
        u32 unused21 : 1;
        u32 marksBusy : 1;
        u32 toFocus : 1;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(AttachFocusFlags, 4);

// 47, 64, 94: the target, or a linked object, attached to the instance (or the instance to the focus) at an exit point, with an
// offset and angles when given
class AttachFocusObjectCommand : public ScriptCommand
{
public:
    AttachFocusFlags flags;
    Vector4 offset;
    // 65536ths of a turn
    s32 angleX;
    s32 angleY;
    s32 angleZ;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd47_AttachFocusObject_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd47_AttachFocusObject_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd47_AttachFocusObject_Dtor);
    u32 Size() RETAIL(Cmd47_AttachFocusObject_GetSize);
};
CHECK_SIZE(AttachFocusObjectCommand, 0x30);

// What DropAttachedObject does: the message the dropped instance is sent (0xFFFF none), the exit point it's at (0xFF the one at
// none), how it's let go of, and its busy flag cleared
union DropRequest
{
    // Just let go of, launched with the velocity it had, made to fall, made to fall when it's a crate (launched otherwise)
    enum Mode : u32
    {
        LetGo = 0,
        Launched = 1,
        Falls = 2,
        FallsIfCrate = 3,
        Modes = 4,
    };

    u32 value;
    struct
    {
        u32 message : 16;
        u32 exitPoint : 8;
        u32 mode : 3;
        u32 unused27 : 2;
        u32 clearsBusy : 1;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(DropRequest, 4);

// 48: the instance at an exit point let go of (dropped, launched, made to fall) and sent a message
class DropAttachedObjectCommand : public ScriptCommand
{
public:
    DropRequest request;
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

// Where a throw goes: the exit point of the thrown instance (0xFF the one at none), the target (a space, a receiver of the
// starter and a designator: game/objectnode.h's DesignatedPosition), a bit the target's search is handed and doesn't read, an
// offset and a spread given, and the thrown instance's busy flag cleared
union ThrowRequest
{
    u32 value;
    struct
    {
        u32 exitPoint : 8;
        u32 space : 4;
        u32 receiver : 8;
        u32 designator : 8;
        u32 unused28 : 1;
        u32 offsetGiven : 1;
        u32 spreadGiven : 1;
        u32 clearsBusy : 1;
    };
};
CHECK_SIZE(ThrowRequest, 4);

// 49: the instance at an exit point let go of and thrown at a target at a speed under a gravity (30 when it's negative)
class ThrowAttachedObjectCommand : public ScriptCommand
{
public:
    ThrowRequest request;
    Vector4 offset;
    TaggedValue gravity;
    TaggedValue speed;
    // How far the target is moved at random
    f32 spread;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd49_ThrowAttachedObject_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd49_ThrowAttachedObject_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd49_ThrowAttachedObject_Dtor);
    u32 Size() RETAIL(Cmd49_ThrowAttachedObject_GetSize);
};
CHECK_SIZE(ThrowAttachedObjectCommand, 0x30);

// 50: the agent told it lost its support
class UnsupportOverFocusCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd50_UnsupportOverFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd50_UnsupportOverFocus_Execute);
    u32 Size() RETAIL(Cmd50_UnsupportOverFocus_GetSize);
};
CHECK_SIZE(UnsupportOverFocusCommand, 0xC);

// 51: the instance 4 units above the target made to fall
class UnsupportAboveCommand : public ScriptCommand
{
public:
    TargetRequest request;
    Vector4 offset;

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

// SendUserMessage's message and its recipient: a linked object's index, a designator (a receiver's index below 0xDE), or for an
// object's linked objects 0 when only the first gets it
union UserMessageTarget
{
    u32 value;
    struct
    {
        u32 message : 16;
        u32 recipient : 8;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(UserMessageTarget, 4);

// The object ID of none of the commands that pick linked objects by their object (SendUserMessage, RunSlotBehaviourOnLinked):
// retail takes 0xFF (the IDs' none, NoObjectId, finds nothing)
constexpr u16 NoLinkedObjectId = 0xFF;

// Which linked objects SendUserMessage sends to: those of an object (NoLinkedObjectId none), the one of an index (the current
// one, the last), every one, those a field of bits picks (a bit each, the field's index), only those on or off the attachments'
// path; and the linked objects aimed at unlinked
union UserMessageFlags
{
    u32 value;
    struct
    {
        u32 object : 16;
        u32 byIndex : 1;
        u32 everyLinked : 1;
        u32 currentLinked : 1;
        u32 unlinks : 1;
        u32 lastLinked : 1;
        u32 unused21 : 3;
        u32 byField : 1;
        u32 field : 5;
        u32 onlyOffPath : 1;
        u32 onlyOnPath : 1;
    };
};
CHECK_SIZE(UserMessageFlags, 4);

// 54, 65: a user message sent to linked objects or to a designator's instance
class SendUserMessageCommand : public ScriptCommand
{
public:
    UserMessageTarget target;
    UserMessageFlags flags;
    // The linked objects' field: the bits, and the width of a field
    TaggedValue fieldBits;
    TaggedValue fieldWidth;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd54_SendUserMessage_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd54_SendUserMessage_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd54_SendUserMessage_Dtor);
    u32 Size() RETAIL(Cmd54_SendUserMessage_GetSize);
};
CHECK_SIZE(SendUserMessageCommand, 0x1C);

// BroadcastUserMessage's message and the middle of its sphere: an offset given, a space, a receiver of the starter and a
// designator (game/objectnode.h's DesignatedPosition)
union BroadcastTarget
{
    u32 value;
    struct
    {
        u32 message : 11;
        u32 offsetGiven : 1;
        u32 space : 4;
        u32 receiver : 8;
        u32 designator : 8;
    };
};
CHECK_SIZE(BroadcastTarget, 4);

// 55: a user message sent to the awake instances in a sphere around a target
class BroadcastUserMessageCommand : public ScriptCommand
{
public:
    BroadcastTarget target;
    f32 radius;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    Vector4 offset;
    // Only the instances of this object get it (0xFFFF every one)
    union Object
    {
        u32 value;
        struct
        {
            u32 id : 16;
            u32 unused16 : 16;
        };
    };

    Object object;
    u32 unused11;
    u32 unused12;
    u32 unused13;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd55_BroadcastUserMessage_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd55_BroadcastUserMessage_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd55_BroadcastUserMessage_Dtor);
    u32 Size() RETAIL(Cmd55_BroadcastUserMessage_GetSize);
};
CHECK_SIZE(BroadcastUserMessageCommand, 0x40);

// 56: the model's animation of a joint stopped, blending out
class ClearAnimationCommand : public ScriptCommand
{
public:
    // The joint whose animation stops (0xFF the root)
    union Joint
    {
        u32 value;
        struct
        {
            u32 id : 8;
            u32 unused8 : 24;
        };
    };

    Joint joint;
    TaggedValue blendTime;

    static ClearAnimationCommand* Construct(ClearAnimationCommand* command) RETAIL(FUN_00221bd0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd56_ClearAnimation_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd56_ClearAnimation_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd56_ClearAnimation_Dtor);
    u32 Size() RETAIL(Cmd56_ClearAnimation_GetSize);
};
CHECK_SIZE(ClearAnimationCommand, 0x14);

// 57: the focus is the instance hanging on an exit point
class RequestAttachmentFocusCommand : public ScriptCommand
{
public:
    // The exit point the instance hangs on (0xFF: the one hanging on none)
    union Hanging
    {
        u32 value;
        struct
        {
            u32 exitPoint : 8;
            u32 unused8 : 24;
        };
    };

    Hanging hanging;

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
    DesignatorSlotArgument copied;

    void Destroy(u32 destroyFlags) RETAIL(Cmd59_RequestMessengersFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd59_RequestMessengersFocus_Execute);
    u32 Size() RETAIL(Cmd59_RequestMessengersFocus_GetSize);
};
CHECK_SIZE(RequestMessengersFocusCommand, 0x10);

// 60, 108
class SetFocusPositionCommand : public ScriptCommand
{
public:
    // Where the position is: a receiver's, else a designator's (the route's steps tuned by the route's values), else the space's
    // (game/objectnode.h's DesignatedPosition, the offset added when it's given; unused20 is handed to its unused argument); noise
    // added, the DesignatorSlot it's given to (SlotFocus or SlotStoredPosition), the axes of the instance's frame (its start's
    // with fromStart) it's kept to (bit 0 x, 1 y, 2 z) and the instance's move since its start added (with fromStart)
    union Target
    {
        u32 value;
        struct
        {
            u32 receiver : 8;
            u32 designator : 8;
            u32 space : 4;
            u32 unused20 : 1;
            u32 offsetGiven : 1;
            u32 noise : 1;
            u32 slot : 3;
            u32 keptAxes : 3;
            u32 fromStart : 1;
            u32 unused30 : 1;
            u32 addsMove : 1;
        };
    };

    // The joint's instance is the focus instance (else the agent's own); the position put straight ahead of the agent's instance as
    // far away (retail keeps these as the bits after the target's, a 64 bit word)
    union Options
    {
        u32 value;
        struct
        {
            u32 focusJoint : 1;
            u32 straightAhead : 1;
            u32 unused2 : 30;
        };
    };

    // The joint whose position it is (0xFF none)
    union Joint
    {
        u32 value;
        struct
        {
            u32 id : 8;
            u32 unused8 : 24;
        };
    };

    u32 unused0C;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    Target target;
    Options options;
    f32 noiseSpread;
    f32 noiseUpScale;
    // The route's step's (game/objectnode.h's RouteStepPosition)
    f32 routeCorner;
    f32 routeScatter;
    // How far along the perception's direction from the instance, else away from the player
    f32 alongPerception;
    f32 awayFromPlayer;
    f32 routeSideways;
    f32 routeLift;
    f32 routeToward;
    Joint joint;
    u32 unused50;
    u32 unused54;
    u32 unused58;
    u32 unused5C;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd60_SetFocusPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd60_SetFocusPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd60_SetFocusPosition_Execute);
    u32 Size() RETAIL(Cmd60_SetFocusPosition_GetSize);
};
CHECK_OFFSET(SetFocusPositionCommand, target, 0x20);
CHECK_OFFSET(SetFocusPositionCommand, joint, 0x4C);
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
    DesignatorSlotArgument moved;
    TaggedValue spread;
    f32 xScale;
    f32 yScale;
    f32 zScale;

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
    DesignatorSlotArgument given;

    void Destroy(u32 destroyFlags) RETAIL(Cmd67_RequestMessSourceAsFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd67_RequestMessSourceAsFocus_Execute);
    u32 Size() RETAIL(Cmd67_RequestMessSourceAsFocus_GetSize);
};
CHECK_SIZE(RequestMessSourceAsFocusCommand, 0x10);

// 68
class SetCounterCommand : public ScriptCommand
{
public:
    // The counter: the game's, else an agent's (a designator's, 0xFF the agent's own, or every linked object's), set to the value
    // or to a random number below it
    union Target
    {
        u32 value;
        struct
        {
            u32 counter : 16;
            u32 designator : 8;
            u32 agentCounter : 1;
            u32 randomBelow : 1;
            u32 everyLinked : 1;
            u32 unused27 : 5;
        };
    };

    Target target;
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
    // The counter: the game's, else an agent's (a designator's, 0xFF the agent's own, or the linked objects' of an object)
    union Target
    {
        u32 value;
        struct
        {
            u32 counter : 16;
            u32 designator : 8;
            u32 agentCounter : 1;
            u32 linkedObjects : 1;
            u32 unused26 : 6;
        };
    };

    // The object whose linked objects' counters change (0xFFFF any)
    union LinkedObject
    {
        u32 value;
        struct
        {
            u32 id : 16;
            u32 unused16 : 16;
        };
    };

    Target target;
    TaggedValue delta;
    LinkedObject linkedObject;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd69_ModifyCounter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd69_ModifyCounter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd69_ModifyCounter_Execute);
    u32 Size() RETAIL(Cmd69_ModifyCounter_GetSize);
};
CHECK_SIZE(ModifyCounterCommand, 0x18);

// ContinueColliderMotion's bits: a velocity given (the node's velocity is set, never to the one given: game/commandsphysics.cpp),
// the drag, friction and restitution given, and the rigid body stopping once it rests
union ColliderMotionBits
{
    u32 value;
    struct
    {
        u32 velocityGiven : 1;
        u32 dragGiven : 1;
        u32 frictionGiven : 1;
        u32 restitutionGiven : 1;
        u32 stops : 1;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(ColliderMotionBits, 4);

// 70: the rigid body moving again with its values
class ContinueColliderMotionCommand : public ScriptCommand
{
public:
    ColliderMotionBits flags;
    f32 unused2;
    f32 unused3;
    f32 unused4;
    u32 unused5;
    f32 drag;
    f32 friction;
    f32 restitution;
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

// ColliderLaunchNow's bits: the launch's target (the space it's in, a receiver's instance, a designator's or the route's step,
// whose position the step values move), the velocity given (else the throw to the target, in a time or over a height), the
// offset given, the drag, friction, restitution and turn given, and the rigid body stopping once it rests. The development
// tools' parser sets bit 26, which nothing reads
union ColliderLaunchBits
{
    u32 value;
    struct
    {
        u32 space : 4;
        u32 receiver : 8;
        u32 designator : 8;
        u32 velocityGiven : 1;
        u32 offsetGiven : 1;
        u32 dragGiven : 1;
        u32 frictionGiven : 1;
        u32 restitutionGiven : 1;
        u32 turnGiven : 1;
        u32 unused26 : 1;
        u32 stops : 1;
        u32 thrownInTime : 1;
        u32 thrownOverHeight : 1;
        u32 unused30 : 2;
    };
};
CHECK_SIZE(ColliderLaunchBits, 4);

// ColliderLaunchNow's second word: the target's height above the node added to the throw's. The development tools' parser marks
// the step values given in bits 0-2, which nothing reads, and bit 4 goes to a rigid body state bit nothing reads
union ColliderLaunchExtras
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 unused1 : 1;
        u32 unused2 : 1;
        u32 addsRise : 1;
        u32 unused4 : 1;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(ColliderLaunchExtras, 4);

// 72: the node's velocity (given or a throw to a target) and its rigid body's values
class ColliderLaunchNowCommand : public ScriptCommand
{
public:
    f32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    ColliderLaunchBits launch;
    ColliderLaunchExtras extras;
    f32 drag;
    f32 friction;
    f32 restitution;
    f32 turn;
    f32 stepCorner;
    f32 stepToward;
    f32 stepScatter;
    f32 heightOrTime;
    u32 unused16;
    u32 unused17;

    static ColliderLaunchNowCommand* Construct(ColliderLaunchNowCommand* command) RETAIL(FUN_002532d8);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd72_ColliderLaunchNow_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd72_ColliderLaunchNow_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd72_ColliderLaunchNow_Execute);
    u32 Size() RETAIL(Cmd72_ColliderLaunchNow_GetSize);
};
CHECK_SIZE(ColliderLaunchNowCommand, 0x50);

// How ForceAnimationUpdate animates the model: once (at its next update), always, or not any more (any other mode)
union AnimationUpdate
{
    enum Mode : u32
    {
        UpdateOnce = 1,
        UpdateAlways = 2,
    };

    u32 value;
    struct
    {
        u32 mode : 4;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(AnimationUpdate, 4);

// 74: the model node animated at its next update however long its instance went unseen, always animated, or not any more
class ForceAnimationUpdateCommand : public ScriptCommand
{
public:
    AnimationUpdate update;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd74_ForceAnimationUpdate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd74_ForceAnimationUpdate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd74_ForceAnimationUpdate_Execute);
    u32 Size() RETAIL(Cmd74_ForceAnimationUpdate_GetSize);
};
CHECK_SIZE(ForceAnimationUpdateCommand, 0x10);

// DestroySpawnedAttachment's attachment: the exit point it hangs at (GameOGI::NoExitPoint the one attached without one), or with
// none every linked object
union SpawnedAttachment
{
    u32 value;
    struct
    {
        u32 exitPoint : 8;
        u32 everyLinked : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(SpawnedAttachment, 4);

// 75: an instance a script made let go of by the agent's attachments and put to sleep
class DestroySpawnedAttachmentCommand : public ScriptCommand
{
public:
    SpawnedAttachment attachment;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd75_DestroySpawnedAttachment_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd75_DestroySpawnedAttachment_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd75_DestroySpawnedAttachment_Dtor);
    u32 Size() RETAIL(Cmd75_DestroySpawnedAttachment_GetSize);
};
CHECK_SIZE(DestroySpawnedAttachmentCommand, 0x10);

// ApplyImpulse's bits: the designator whose instance is pushed (0xFF none: the originator's when asked, else the agent's own),
// and whether an impulse or else a spin is given. The development tools' parser writes bits 8 and 9, which nothing reads
union ImpulseBits
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 unused8 : 1;
        u32 unused9 : 1;
        u32 fromOriginator : 1;
        u32 spinGiven : 1;
        u32 impulseGiven : 1;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(ImpulseBits, 4);

// 76: an instance pushed by an impulse (turned from the agent's instance's space) or spun (its physics body's angular momentum)
class ApplyImpulseCommand : public ScriptCommand
{
public:
    ImpulseBits flags;
    f32 impulseX;
    f32 impulseY;
    f32 impulseZ;
    f32 impulseW;
    f32 spinX;
    f32 spinY;
    f32 spinZ;
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

// SetObject's switches of the instance (two bits each: 1 on, 2 off, else left): busy, visible, its collision, asleep (1 woken, 2
// put to sleep), its physics body's flag 0x20 (game/objectnode.h's MotionBlockBody: pushable); then the squared near distances of
// its node and its model (a byte each, 0xFF none) and whether they're given
union ObjectSwitches
{
    static constexpr u8 NoDistance = 0xFF;

    u32 value;
    struct
    {
        u32 busy : 2;
        u32 visible : 2;
        u32 collision : 2;
        u32 asleep : 2;
        u32 unused8 : 2;
        u32 pushable20 : 2;
        u32 unused12 : 1;
        u32 modelDistance : 8;
        u32 nodeDistance : 8;
        u32 modelDistanceGiven : 1;
        u32 nodeDistanceGiven : 1;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(ObjectSwitches, 4);

// SetObject's other switches: its physics body's flag 0x40 (pushable), its movement node carrying what stands on it, its model
// solid
union ObjectBodySwitches
{
    u32 value;
    struct
    {
        u32 pushable40 : 2;
        u32 carries : 2;
        u32 solid : 2;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(ObjectBodySwitches, 4);

// 78: the instance's and its nodes' switches and near distances
class SetObjectCommand : public ScriptCommand
{
public:
    ObjectSwitches switches;
    ObjectBodySwitches bodySwitches;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd78_SetObject_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd78_SetObject_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(ExecuteCommand_0x4E);
    void ExecuteOn(GameNode* node) RETAIL(Cmd78_SetObject_ExecuteOn);
    u32 Size() RETAIL(GetScriptCommandSize);
};
CHECK_SIZE(SetObjectCommand, 0x14);

// What Keep makes a runner finishing leave the node: its particles, its trajectory and its perception; bits 2-4 go to node
// flags nothing reads
union KeepFlags
{
    u32 value;
    struct
    {
        u32 particles : 1;
        u32 trajectory : 1;
        u32 unused2 : 1;
        u32 unused3 : 1;
        u32 unused4 : 1;
        u32 perception : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(KeepFlags, 4);

// 79: what a runner finishing leaves the node
class KeepCommand : public ScriptCommand
{
public:
    KeepFlags keeps;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd79_Keep_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd79_Keep_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd79_Keep_Execute);
    u32 Size() RETAIL(Cmd79_Keep_GetSize);
};
CHECK_SIZE(KeepCommand, 0x10);

// How AttachSpring makes its spring: the space of its target (game/objectnode.h's DesignatedPosition), the object whose instance
// is made at its end (0xFFFF none), an offset given, its target in the focus's space, its rest length given, the instance at its
// end without collisions
union SpringFlags
{
    u32 value;
    struct
    {
        u32 space : 4;
        u32 object : 16;
        u32 offsetGiven : 1;
        u32 toFocus : 1;
        u32 lengthGiven : 1;
        u32 endUncollidable : 1;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(SpringFlags, 4);

// The joints of a spring's ends: the focus's its target is at, and the instance's (0xFF its place)
union SpringJoints
{
    u32 value;
    struct
    {
        u32 focusJoint : 8;
        u32 joint : 8;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(SpringJoints, 4);

// 80: a spring attached to the instance from its joint or its place to a target, with an instance made at its end when asked
class AttachSpringCommand : public ScriptCommand
{
public:
    f32 unused1;
    Vector4 offset;
    Vector4 target;
    SpringFlags flags;
    SpringJoints joints;
    f32 power;
    f32 damping;
    f32 length;
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

// 82, 83: the motion block of its arguments the node's own (pointing back at the node), its instance given a physics body
class SetContactSpringyCommand : public ScriptCommand
{
public:
    MotionBlock block;

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

// DestroyMe's mode (3 and up do nothing); the development tools' parser puts another one in bits 3-5, which nothing reads
union DestroyMeSettings
{
    static constexpr u32 Modes = 3;

    u32 value;
    struct
    {
        u32 mode : 3;
        u32 unused3 : 3;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(DestroyMeSettings, 4);

// 85: the node and its instance put to sleep (in modes 0 to 2)
class DestroyMeCommand : public ScriptCommand
{
public:
    DestroyMeSettings settings;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd85_DestroyMe_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd85_DestroyMe_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(ExecuteCommand_Destroy);
    u32 Size() RETAIL(Cmd85_DestroyMe_GetSize);
};
CHECK_SIZE(DestroyMeCommand, 0x10);

// 86: the chunk's reverb, or its box reverb
class SetReverbCommand : public ScriptCommand
{
public:
    ReverbSettings reverb;
    SwitchArgument boxReverb;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd86_SetSound_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd86_SetSound_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd86_SetSound_Execute);
    u32 Size() RETAIL(Cmd86_SetSound_GetSize);
};
CHECK_SIZE(SetReverbCommand, 0x28);

// Which of the trajectory controller's wobble phases (about x, y and z) a command sets
union WobbleAxes
{
    u32 value;
    struct
    {
        u32 x : 1;
        u32 y : 1;
        u32 z : 1;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(WobbleAxes, 4);

// 87: the trajectory controller's wobble phases set (degrees)
class AlterWobblePhaseCommand : public ScriptCommand
{
public:
    TaggedValue phaseX;
    TaggedValue phaseY;
    TaggedValue phaseZ;
    WobbleAxes axes;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd87_AlterWobblePhase_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd87_AlterWobblePhase_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd87_AlterWobblePhase_Dtor);
    u32 Size() RETAIL(Cmd87_AlterWobblePhase_GetSize);
};
CHECK_SIZE(AlterWobblePhaseCommand, 0x1C);

// BeginMusic's music slot (game/sound.h's PlayMusicRequest: 0 the main one, its volume the track's; 1 the context's; 2 in the
// effects' volume group; 3 a music emitter where the instance is, heard to the range; none past it) and whether it loops
union BeginMusicFlags
{
    static constexpr u32 EmitterSlot = 3;

    u32 value;
    struct
    {
        u32 unused0 : 12;
        u32 slot : 3;
        u32 loops : 1;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(BeginMusicFlags, 4);

// 88
class BeginMusicCommand : public ScriptCommand
{
public:
    TaggedValue track;
    BeginMusicFlags flags;
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

// 89: the context music's slot faded out
class EndContextMusicCommand : public ScriptCommand
{
public:
    f32 fadeSeconds;

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
    s32 lives;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd92_AddLives_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd92_AddLives_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd92_AddLives_Execute);
    u32 Size() RETAIL(Cmd92_AddLives_GetSize);
};
CHECK_SIZE(AddLivesCommand, 0x10);

// 95: AgentRef2 unlinked from the instance and sent a message
class ReleaseAgentRef2Command : public ScriptCommand
{
public:
    // The message AgentRef2 is sent (0xFFFF none); the development tools' parser sets bit 16 for MakeIdle, which nothing reads
    union Release
    {
        u32 value;
        struct
        {
            u32 message : 16;
            u32 unused16 : 1;
            u32 unused17 : 15;
        };
    };

    Release release;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd95_ReleaseAgentRef2_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd95_ReleaseAgentRef2_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd95_ReleaseAgentRef2_Dtor);
    u32 Size() RETAIL(Cmd95_ReleaseAgentRef2_GetSize);
};
CHECK_SIZE(ReleaseAgentRef2Command, 0x10);

// How LaunchAgentRef2 launches: an offset and a spread given, AgentRef2's busy flag cleared, AgentRef1 passed on to it (bit 0 is
// handed to the target's search, which doesn't read it)
union LaunchFlags
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 offsetGiven : 1;
        u32 spreadGiven : 1;
        u32 clearsBusy : 1;
        u32 unused4 : 2;
        u32 unused6 : 1;
        u32 passesAgentRef1 : 1;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(LaunchFlags, 4);

// 96: AgentRef2 let go of and launched at a target by the motion block of its arguments
class LaunchAgentRef2Command : public ScriptCommand
{
public:
    u32 unused1;
    Vector4 offset;
    MotionBlock block;
    // Its target (its exit point and the bits above the designator unread)
    ThrowRequest launch;
    LaunchFlags flags;
    TaggedValue speed;
    // How far the target is moved at random
    f32 spread;
    // How fast it spins about its x axis, else about its y axis
    f32 spinX;
    f32 spinY;

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

// The axes an argument picks: x, y and z
union AxisSelection
{
    static constexpr u32 All = 0x7;

    u32 value;
    struct
    {
        u32 x : 1;
        u32 y : 1;
        u32 z : 1;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(AxisSelection, 4);

// 113: the instance's rotation about its axes snapped to the nearest multiple of a step
class SnapRotationCommand : public ScriptCommand
{
public:
    AxisSelection axes;
    // Radians
    f32 stepX;
    f32 stepY;
    f32 stepZ;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd113_SetRotationComponents_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd113_SetRotationComponents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd113_SetRotationComponents_Execute);
    u32 Size() RETAIL(Cmd113_SetRotationComponents_GetSize);
};
CHECK_SIZE(SnapRotationCommand, 0x1C);

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

// SetHeadTrackingTarget's target: a starter receiver's instance (DesignatesNone none), else the focus instance, the player or
// AgentRef2; and whether the head tracking remembers it with its weight
union HeadTrackingTargetBits
{
    u32 value;
    struct
    {
        u32 receiver : 8;
        u32 player : 1;
        u32 agentRef2 : 1;
        u32 focus : 1;
        u32 remembers : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(HeadTrackingTargetBits, 4);

// 118: the head tracking's target given with a weight
class SetHeadTrackingTargetCommand : public ScriptCommand
{
public:
    HeadTrackingTargetBits target;
    f32 weight;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd118_SetHeadTrackingTarget_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd118_SetHeadTrackingTarget_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd118_SetHeadTrackingTarget_Dtor);
    u32 Size() RETAIL(Cmd118_SetHeadTrackingTarget_GetSize);
};
CHECK_SIZE(SetHeadTrackingTargetCommand, 0x14);

// 119: the node's knock countdown cleared
class ClearKnockCountdownCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd119_ClearNodeByte154_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd119_ClearNodeByte154_Execute);
    u32 Size() RETAIL(Cmd119_ClearNodeByte154_GetSize);
};
CHECK_SIZE(ClearKnockCountdownCommand, 0xC);

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
    u32 unused10;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd121_SetFocusPositionBesidePlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd121_SetFocusPositionBesidePlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd121_SetFocusPositionBesidePlayer_Execute);
    u32 Size() RETAIL(Cmd121_SetFocusPositionBesidePlayer_GetSize);
};
CHECK_SIZE(SetFocusPositionBesidePlayerCommand, 0x14);

// 122
class CopyDesignatorCommand : public ScriptCommand
{
public:
    // A designator of the agent whose designators the source is (DesignatesItself: the agent's own) and the agent's designator
    // given its instance, else its position
    union Designators
    {
        u32 value;
        struct
        {
            u32 source : 8;
            u32 destination : 8;
            u32 sourceAgent : 8;
            u32 unused24 : 8;
        };
    };

    Designators designators;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd122_SetFocusPositionToAgent_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd122_SetFocusPositionToAgent_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd122_SetFocusPositionToAgent_Execute);
    u32 Size() RETAIL(Cmd122_SetFocusPositionToAgent_GetSize);
};
CHECK_SIZE(CopyDesignatorCommand, 0x10);

// 123: the agent's instance attached to the AI position nearest it
class LinkToNearestPointCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd123_LinkToNearestPoint_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd123_LinkToNearestPoint_Execute);
    u32 Size() RETAIL(Cmd123_LinkToNearestPoint_GetSize);
};
CHECK_SIZE(LinkToNearestPointCommand, 0xC);

// What RunScriptSlot starts: the object's behaviour slot, and the node's runner it runs in
union BehaviourSlotRequest
{
    u32 value;
    struct
    {
        u32 slot : 16;
        u32 runner : 1;
        u32 unused17 : 15;
    };
};
CHECK_SIZE(BehaviourSlotRequest, 4);

// 124: the behaviour of the object's slot started on the node (forced) in one of its two runners
class RunScriptSlotCommand : public ScriptCommand
{
public:
    BehaviourSlotRequest request;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd124_RunScriptSlot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd124_RunScriptSlot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd124_RunScriptSlot_Execute);
    u32 Size() RETAIL(Cmd124_RunScriptSlot_GetSize);
};
CHECK_SIZE(RunScriptSlotCommand, 0x10);

// 125: the focus position moved
class OffsetFocusPositionCommand : public ScriptCommand
{
public:
    u32 unused1;
    Vector4 offset;

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

// 130: the rigid body's magnet: its pull and its modes (and its first size, which nothing reads)
class SetMagnetCommand : public ScriptCommand
{
public:
    f32 size;
    f32 magnetPull;
    // game/objectnode.h's ObjectRigidBodyState: the magnet's way, and its strength's mode
    u32 magnetWay;
    u32 magnetStrength;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd130_SetPhysicsSizes_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd130_SetPhysicsSizes_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd130_SetPhysicsSizes_Execute);
    u32 Size() RETAIL(Cmd130_SetPhysicsSizes_GetSize);
};
CHECK_SIZE(SetMagnetCommand, 0x1C);

// 131: the pull of the focus's rigid body's magnet on the agent's rigid body
class MagnetPullToFocusCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd131_MagnetPullToFocus_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd131_MagnetPullToFocus_Execute);
    u32 Size() RETAIL(Cmd131_MagnetPullToFocus_GetSize);
};
CHECK_SIZE(MagnetPullToFocusCommand, 0xC);

// 132: the linked object the linked object commands are at
class SetLinkedObjectIndexCommand : public ScriptCommand
{
public:
    // Counting from 1 (left as it is past the count)
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

// 136: a sense's weight
class SetPerceptionWeightCommand : public ScriptCommand
{
public:
    // Its low byte
    u32 sense;
    f32 weight;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd136_SetPerceptionWeight_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd136_SetPerceptionWeight_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd136_SetPerceptionWeight_Execute);
    u32 Size() RETAIL(Cmd136_SetPerceptionWeight_GetSize);
};
CHECK_SIZE(SetPerceptionWeightCommand, 0x14);

// 137: a weight added to a sense's
class AddPerceptionWeightCommand : public ScriptCommand
{
public:
    // Its low byte
    u32 sense;
    f32 weight;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd137_AddPerceptionWeight_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd137_AddPerceptionWeight_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd137_AddPerceptionWeight_Execute);
    u32 Size() RETAIL(Cmd137_AddPerceptionWeight_GetSize);
};
CHECK_SIZE(AddPerceptionWeightCommand, 0x14);

// 138: the agent's rigid body pushed along its perception's direction over the ground, for the time since its node's last update
class PushFromPerceptionCommand : public ScriptCommand
{
public:
    u32 perceptionSlot;
    f32 strength;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd138_PushFromPerception_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd138_PushFromPerception_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd138_PushFromPerception_Dtor);
    u32 Size() RETAIL(Cmd138_PushFromPerception_GetSize);
};
CHECK_SIZE(PushFromPerceptionCommand, 0x14);

// 139: how much the instance makes itself felt to the perceptions around (the scripts' character analog, kept between -1 and 1)
class SetPresenceCommand : public ScriptCommand
{
public:
    f32 presence;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd139_SetCharacterAnalog_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd139_SetCharacterAnalog_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd139_SetCharacterAnalog_Execute);
    u32 Size() RETAIL(Cmd139_SetCharacterAnalog_GetSize);
};
CHECK_SIZE(SetPresenceCommand, 0x10);

// 140: a change of how much the instance makes itself felt (kept between -1 and 1)
class AddPresenceCommand : public ScriptCommand
{
public:
    f32 delta;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd140_AddCharacterAnalog_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd140_AddCharacterAnalog_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd140_AddCharacterAnalog_Execute);
    u32 Size() RETAIL(Cmd140_AddCharacterAnalog_GetSize);
};
CHECK_SIZE(AddPresenceCommand, 0x10);

// 141: a sense turned off
class TurnSenseOffCommand : public ScriptCommand
{
public:
    // Its low byte
    u32 sense;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd141_PerceptionOp141_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd141_PerceptionOp141_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd141_PerceptionOp141_Execute);
    u32 Size() RETAIL(Cmd141_PerceptionOp141_GetSize);
};
CHECK_SIZE(TurnSenseOffCommand, 0x10);

// 142: a sense turned on
class TurnSenseOnCommand : public ScriptCommand
{
public:
    // Its low byte
    u32 sense;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd142_PerceptionOp142_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd142_PerceptionOp142_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd142_PerceptionOp142_Execute);
    u32 Size() RETAIL(Cmd142_PerceptionOp142_GetSize);
};
CHECK_SIZE(TurnSenseOnCommand, 0x10);

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

// 145: the message given to the behaviour level below (none on the first level)
class SetParentExecutionValueCommand : public ScriptCommand
{
public:
    union Message
    {
        u32 value;
        struct
        {
            u32 id : 16;
            u32 unused16 : 16;
        };
    };

    Message message;

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
    // A linked object (of the focus instance's attachments, AgentRef1's, else of the agent's instance's or of its source node's
    // instance's: the current one, the last or the one of the index) or else a receiver's or a designator's instance (the index),
    // given to a DesignatorSlot
    union Target
    {
        u32 value;
        struct
        {
            u32 index : 8;
            u32 linked : 1;
            u32 current : 1;
            u32 focusLinked : 1;
            u32 agentRef1Linked : 1;
            u32 slot : 3;
            u32 last : 1;
            u32 unused16 : 16;
        };
    };

    Target target;

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

// 148: the trajectory's cycles' amplitudes about the axes it picks
class SetCycleAmplitudesCommand : public ScriptCommand
{
public:
    TaggedValue amplitudeX;
    TaggedValue amplitudeY;
    TaggedValue amplitudeZ;
    AxisSelection axes;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd148_SetMotionFloats_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd148_SetMotionFloats_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd148_SetMotionFloats_Dtor);
    u32 Size() RETAIL(Cmd148_SetMotionFloats_GetSize);
};
CHECK_SIZE(SetCycleAmplitudesCommand, 0x1C);

// RotateWithLinked's axes: the instance's own it turns about (the first of x, y and z), and the instance's it hangs from that
// instance moves along (the first of x, y and z)
union TurnAlongAxes
{
    static constexpr u32 TurnsMask = 0x7;
    static constexpr u32 AlongMask = 0x38;

    u32 value;
    struct
    {
        u32 turnsX : 1;
        u32 turnsY : 1;
        u32 turnsZ : 1;
        u32 alongX : 1;
        u32 alongY : 1;
        u32 alongZ : 1;
        u32 unused6 : 26;
    };
};
CHECK_SIZE(TurnAlongAxes, 4);

// 149: the instance turned about one of its axes as the instance it hangs from moves along one of that one's
class RotateWithLinkedCommand : public ScriptCommand
{
public:
    f32 degreesPerSecond;
    TurnAlongAxes axes;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd149_RotateWithLinked_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd149_RotateWithLinked_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd149_RotateWithLinked_Dtor);
    u32 Size() RETAIL(Cmd149_RotateWithLinked_GetSize);
};
CHECK_SIZE(RotateWithLinkedCommand, 0x14);

// StrafeTowardsTarget's target: the designator it goes toward, the space whose x axis it moves along (ControlPacket::Space: the
// start's or its own, else the way to the target itself), and whether it turns toward the target
union StrafeTarget
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 space : 4;
        u32 faces : 1;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(StrafeTarget, 4);

// 150: the agent's instance moved along an axis toward a target
class StrafeTowardsTargetCommand : public ScriptCommand
{
public:
    StrafeTarget target;
    f32 speed;
    f32 maxDistance;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd150_StrafeTowardsTarget_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd150_StrafeTowardsTarget_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd150_StrafeTowardsTarget_Dtor);
    u32 Size() RETAIL(Cmd150_StrafeTowardsTarget_GetSize);
};
CHECK_SIZE(StrafeTowardsTargetCommand, 0x18);

// 151: the waypoints' route a step back (routes are followed down their steps)
class PreviousRouteNodeCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd151_NextKeyOfPath34_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd151_NextKeyOfPath34_Execute);
    u32 Size() RETAIL(Cmd151_NextKeyOfPath34_GetSize);
};
CHECK_SIZE(PreviousRouteNodeCommand, 0xC);

// 152: the trajectory controller's wobble phases turned (degrees)
class AddWobblePhaseCommand : public ScriptCommand
{
public:
    TaggedValue turnX;
    TaggedValue turnY;
    TaggedValue turnZ;
    WobbleAxes axes;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd152_AddMotionAngles_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd152_AddMotionAngles_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd152_AddMotionAngles_Dtor);
    u32 Size() RETAIL(Cmd152_AddMotionAngles_GetSize);
};
CHECK_SIZE(AddWobblePhaseCommand, 0x1C);

// 153: the agent's instance moved toward a designator's position at a speed
class MoveTowardsDesignatorCommand : public ScriptCommand
{
public:
    DesignatorArgument target;
    f32 unused2;
    f32 speed;
    f32 unused4;
    f32 unused5;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd153_MoveTowardsDesignator_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd153_MoveTowardsDesignator_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd153_MoveTowardsDesignator_Dtor);
    u32 Size() RETAIL(Cmd153_MoveTowardsDesignator_GetSize);
};
CHECK_SIZE(MoveTowardsDesignatorCommand, 0x20);

// The noise message SetNoiseMessage gives the node (0xFFFF keeps its own) and whether its noises are passed on (a message given
// sets it, kept in the command)
union NoiseSettings
{
    u32 value;
    struct
    {
        u32 message : 16;
        u32 passesNoises : 1;
        u32 unused17 : 15;
    };
};
CHECK_SIZE(NoiseSettings, 4);

// 156: the node's noise message (one given makes its noises passed on) and whether its noises are passed on to its instance
class SetNoiseMessageCommand : public ScriptCommand
{
public:
    NoiseSettings settings;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd156_SetNode150Fields_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd156_SetNode150Fields_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd156_SetNode150Fields_Execute);
    u32 Size() RETAIL(Cmd156_SetNode150Fields_GetSize);
};
CHECK_SIZE(SetNoiseMessageCommand, 0x10);

// Which linked object UnlinkTarget unlinks: every one, the current one, the one of the index, else the designator's instance when
// it's linked
union UnlinkRequest
{
    u32 value;
    struct
    {
        // A linked object's index or a designator
        u32 target : 8;
        u32 all : 1;
        u32 byIndex : 1;
        u32 current : 1;
        u32 unused11 : 21;
    };
};
CHECK_SIZE(UnlinkRequest, 4);

// 157: a linked object unlinked
class UnlinkTargetCommand : public ScriptCommand
{
public:
    UnlinkRequest request;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd157_UnlinkTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd157_UnlinkTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd157_UnlinkTarget_Execute);
    u32 Size() RETAIL(Cmd157_UnlinkTarget_GetSize);
};
CHECK_SIZE(UnlinkTargetCommand, 0x10);

// 158: the motion block of its arguments the node's own when it has none, its touch message given
class AttachMotionBlockCommand : public ScriptCommand
{
public:
    // The message what touches the block is sent (0 none)
    s32 touchMessage;
    MotionBlock block;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd158_AttachMotionBlock_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd158_AttachMotionBlock_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd158_AttachMotionBlock_Dtor);
    u32 Size() RETAIL(Cmd158_AttachMotionBlock_GetSize);
};
CHECK_SIZE(AttachMotionBlockCommand, 0x98);

// 159: the node's motion block's touch message forgotten
class ClearTouchMessageCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd159_ResetNode120_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd159_ResetNode120_Execute);
    u32 Size() RETAIL(Cmd159_ResetNode120_GetSize);
};
CHECK_SIZE(ClearTouchMessageCommand, 0xC);

// 160: what touches the node's motion block no longer sent its touch message
class StopTouchMessagesCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd160_ClearMotionBlockFlag16_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd160_ClearMotionBlockFlag16_Execute);
    u32 Size() RETAIL(Cmd160_ClearMotionBlockFlag16_GetSize);
};
CHECK_SIZE(StopTouchMessagesCommand, 0xC);

// 161: what touches the node's motion block sent its touch message
class SendTouchMessagesCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd161_SetMotionBlockFlag16_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd161_SetMotionBlockFlag16_Execute);
    u32 Size() RETAIL(Cmd161_SetMotionBlockFlag16_GetSize);
};
CHECK_SIZE(SendTouchMessagesCommand, 0xC);

// A command's argument word that only names an agent's counter (game/agents.h's Agent::counters)
union CounterArgument
{
    u32 value;
    struct
    {
        u32 counter : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(CounterArgument, 4);

// 162
class AddToFocusCounterCommand : public ScriptCommand
{
public:
    CounterArgument counter;
    TaggedValue amount;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd162_AddToFocusObjectByte_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd162_AddToFocusObjectByte_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd162_AddToFocusObjectByte_Execute);
    u32 Size() RETAIL(Cmd162_AddToFocusObjectByte_GetSize);
};
CHECK_SIZE(AddToFocusCounterCommand, 0x14);

// 163
class UnlinkFromTargetCommand : public ScriptCommand
{
public:
    // What lets go of the agent's instance: a designator's instance (0xFF none), else the agent's own linked objects: every one,
    // the current one or the one of the index; unlinking by force
    union Target
    {
        u32 value;
        struct
        {
            u32 designatorOrIndex : 8;
            u32 everyLinked : 1;
            u32 byIndex : 1;
            u32 current : 1;
            u32 force : 1;
            u32 unused12 : 20;
        };
    };

    Target target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd163_UnlinkFromTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd163_UnlinkFromTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd163_UnlinkFromTarget_Execute);
    u32 Size() RETAIL(Cmd163_UnlinkFromTarget_GetSize);
};
CHECK_SIZE(UnlinkFromTargetCommand, 0x10);

// AddToLinkedCounter's counter: its index (game/instances.h's Agent::counters), of the linked objects of an object
// (AnyObjectId: every one)
union LinkedCounter
{
    u32 value;
    struct
    {
        u32 object : 16;
        u32 index : 16;
    };
};
CHECK_SIZE(LinkedCounter, 4);

// Which of the agent's linked objects a command takes: the one of an index (NoIndex none), or every one, the current one, the
// first or the last (written into the index)
union LinkedObjectChoice
{
    static constexpr u8 NoIndex = 0xFF;

    u32 value;
    struct
    {
        u32 index : 8;
        u32 every : 1;
        u32 current : 1;
        u32 first : 1;
        u32 last : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(LinkedObjectChoice, 4);

// 164: a counter of the agent's linked objects changed by an amount
class AddToLinkedCounterCommand : public ScriptCommand
{
public:
    LinkedCounter counter;
    LinkedObjectChoice linked;
    TaggedValue amount;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd164_AddToLinkedObjectsByte_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd164_AddToLinkedObjectsByte_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd164_AddToLinkedObjectsByte_Dtor);
    u32 Size() RETAIL(Cmd164_AddToLinkedObjectsByte_GetSize);
};
CHECK_SIZE(AddToLinkedCounterCommand, 0x18);

// Whether ForceVolumeController turns the volume controller on or off (neither: left)
union VolumeSwitch
{
    u32 value;
    struct
    {
        u32 on : 1;
        u32 off : 1;
        u32 unused2 : 30;
    };
};
CHECK_SIZE(VolumeSwitch, 4);

// 165: a force given to the volume controller of an instance overlapping the instance's box, turned on or off
class ForceVolumeControllerCommand : public ScriptCommand
{
public:
    TaggedValue forceX;
    TaggedValue forceY;
    TaggedValue forceZ;
    u32 unused4;
    u32 unused5;
    // The force given (w 1), kept in the command
    Vector4 force;
    VolumeSwitch turns;
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

// What SetSurface sets: a collision surface ID, for every hull or for the hull of an index
union SurfaceRequest
{
    u32 value;
    struct
    {
        u32 surface : 16;
        u32 allHulls : 1;
        u32 hull : 8;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(SurfaceRequest, 4);

// 167: the surface of the instance's hulls, or of one of them
class SetSurfaceCommand : public ScriptCommand
{
public:
    SurfaceRequest request;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd167_SetSurface_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd167_SetSurface_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd167_SetSurface_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd167_SetSurface_ExecuteOn);
    u32 Size() RETAIL(Cmd167_SetSurface_GetSize);
};
CHECK_SIZE(SetSurfaceCommand, 0x10);

// PushInstancesAway's bits: the push grows from the middle's to the edge's with the distance (any of the low byte)
union PushAwayBits
{
    u32 value;
    struct
    {
        u32 byDistance : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(PushAwayBits, 4);

// 168: the physics bodies within a radius of a point by the agent's instance pushed away from it
class PushInstancesAwayCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 radius;
    f32 push;
    f32 edgePush;
    PushAwayBits flags;
    u32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd168_MoveInstancesInBox_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd168_MoveInstancesInBox_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd168_MoveInstancesInBox_Execute);
    u32 Size() RETAIL(Cmd168_MoveInstancesInBox_GetSize);
};
CHECK_SIZE(PushInstancesAwayCommand, 0x30);

// 169
class SetFocusPositionAlongCommand : public ScriptCommand
{
public:
    // OwnPositionMode: the agent's own position stands in for a start without one
    enum Mode : u32
    {
        OwnPositionMode = 2,
    };

    // The designator given the position, and the designators it's between
    union Designators
    {
        u32 value;
        struct
        {
            u32 destination : 8;
            u32 from : 8;
            u32 to : 8;
            u32 mode : 4;
            u32 unused28 : 4;
        };
    };

    Designators designators;
    f32 distance;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd169_SetFocusPositionAlong_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd169_SetFocusPositionAlong_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd169_SetFocusPositionAlong_Execute);
    u32 Size() RETAIL(Cmd169_SetFocusPositionAlong_GetSize);
};
CHECK_SIZE(SetFocusPositionAlongCommand, 0x14);

// 170
class SetFocusCounterCommand : public ScriptCommand
{
public:
    // The source's value: the value rather than a counter of the agent's
    static constexpr s32 FromValue = 0xFF;

    CounterArgument counter;
    s32 source;
    TaggedValue value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd170_SetFocusObjectByte_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd170_SetFocusObjectByte_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd170_SetFocusObjectByte_Execute);
    u32 Size() RETAIL(Cmd170_SetFocusObjectByte_GetSize);
};
CHECK_SIZE(SetFocusCounterCommand, 0x18);

// 171
class RunSlotBehaviourOnLinkedCommand : public ScriptCommand
{
public:
    // A designator's instance (0xFF none), else the linked objects of an object (NoLinkedObjectId none)
    union Target
    {
        u32 value;
        struct
        {
            u32 designator : 8;
            u32 unused8 : 8;
            u32 objectId : 16;
        };
    };

    // The object's behaviour slot run, in which runner slot, on every linked object; the node it's run on made to take its object
    // from the agent's node
    union Slot
    {
        u32 value;
        struct
        {
            u32 slot : 16;
            u32 runnerSlot : 1;
            u32 everyLinked : 1;
            u32 givesSource : 1;
            u32 unused19 : 13;
        };
    };

    Target target;
    Slot slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd171_RunSlotBehaviourOnLinked_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd171_RunSlotBehaviourOnLinked_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd171_RunSlotBehaviourOnLinked_Execute);
    u32 Size() RETAIL(Cmd171_RunSlotBehaviourOnLinked_GetSize);
};
CHECK_SIZE(RunSlotBehaviourOnLinkedCommand, 0x14);

// Whose object nodes StopTargetBehaviour gives back their own object: a designator's instance's (0xFF none), else every linked
// object's when asked. Bit 8 is only the development tools' parser's
union StopTargetRequest
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 unused8 : 1;
        u32 everyLinked : 1;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(StopTargetRequest, 4);

// 172: a designator's instance's object node made to take its object from itself again, or every linked object's
class StopTargetBehaviourCommand : public ScriptCommand
{
public:
    StopTargetRequest request;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd172_StopTargetBehaviour_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd172_StopTargetBehaviour_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd172_StopTargetBehaviour_Execute);
    u32 Size() RETAIL(Cmd172_StopTargetBehaviour_GetSize);
};
CHECK_SIZE(StopTargetBehaviourCommand, 0x10);

// 173: the path the node's waypoints are on
class SetPathIndexCommand : public ScriptCommand
{
public:
    union Path
    {
        u32 value;
        struct
        {
            u32 index : 8;
            u32 unused8 : 24;
        };
    };

    Path path;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd173_SetKeyPathByte43_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd173_SetKeyPathByte43_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd173_SetKeyPathByte43_Execute);
    u32 Size() RETAIL(Cmd173_SetKeyPathByte43_GetSize);
};
CHECK_SIZE(SetPathIndexCommand, 0x10);

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

// A music slot in an argument's bits 0-2 (game/sound.h's four: 0 the main music's, 1 the context music's and the cutscenes')
union MusicSlotArgument
{
    u32 value;
    struct
    {
        u32 index : 3;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(MusicSlotArgument, 4);

// 176: a music slot faded out
class FadeOutMusicSlotCommand : public ScriptCommand
{
public:
    f32 fadeSeconds;
    MusicSlotArgument slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd176_FadeSoundGroup_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd176_FadeSoundGroup_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd176_FadeSoundGroup_Execute);
    u32 Size() RETAIL(Cmd176_FadeSoundGroup_GetSize);
};
CHECK_SIZE(FadeOutMusicSlotCommand, 0x14);

// 177, 179
class WarpAgentCommand : public ScriptCommand
{
public:
    // Where an agent's instance (a designator's, 0xFF the agent's own) warps to like PositionWarp's target says
    union Warp
    {
        u32 value;
        struct
        {
            u32 receiver : 8;
            u32 designator : 8;
            u32 space : 4;
            u32 unused20 : 1;
            u32 offsetGiven : 1;
            u32 turns : 1;
            u32 turnsBody : 1;
            u32 agent : 8;
        };
    };

    Warp warp;
    // The designator whose position it warps to instead (0xFF none)
    DesignatorArgument source;
    u32 unused14;
    u32 unused18;
    u32 unused1C;
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

// RotateAgent's agent: the designator whose instance turns (0xFF the agent's own). The development tools' parser writes a second
// designator in byte 1, which nothing reads
union RotatedAgent
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 unused8 : 8;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(RotatedAgent, 4);

// 178: an agent's instance turned like RotationWarp turns the agent's own
class RotateAgentCommand : public ScriptCommand
{
public:
    RotationWarpBits warp;
    RotatedAgent agent;
    u32 unused3;
    u32 unused4;
    u32 unused5;
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
    s32 cutscene;
    f32 unused2;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd180_QueueObjectVideo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd180_QueueObjectVideo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd180_QueueObjectVideo_Execute);
    u32 Size() RETAIL(Cmd180_QueueObjectVideo_GetSize);
};
CHECK_SIZE(QueueObjectVideoCommand, 0x14);

// 181: the cutscene QueueObjectVideo queued started once its music is prepared
class StartObjectVideoCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd181_VideoControllerUpdate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd181_VideoControllerUpdate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd181_VideoControllerUpdate_Execute);
    u32 Size() RETAIL(Cmd181_VideoControllerUpdate_GetSize);
};
CHECK_SIZE(StartObjectVideoCommand, 0xC);

// 182: the cutscene stopped: what was read let go of while it waits to start (and what's read later), its music stopped
class CancelVideoCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd182_VideoControllerOp182_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd182_VideoControllerOp182_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd182_VideoControllerOp182_Execute);
    u32 Size() RETAIL(Cmd182_VideoControllerOp182_GetSize);
};
CHECK_SIZE(CancelVideoCommand, 0xC);

// 183: the designator's instance's object node takes its object from this node
class SetTargetOwnerToSelfCommand : public ScriptCommand
{
public:
    DesignatorArgument target;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd183_SetTargetOwnerToSelf_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd183_SetTargetOwnerToSelf_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd183_SetTargetOwnerToSelf_Execute);
    u32 Size() RETAIL(Cmd183_SetTargetOwnerToSelf_GetSize);
};
CHECK_SIZE(SetTargetOwnerToSelfCommand, 0x10);

// Whose object node RestoreOwnObject gives back its own object: the designator's instance's (0xFF none, whether it has one isn't
// checked), else the agent's own when asked
union OwnObjectRequest
{
    u32 value;
    struct
    {
        u32 designator : 8;
        u32 own : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(OwnObjectRequest, 4);

// 184: an object node made to take its object from itself again: the designator's instance's, or the agent's own
class RestoreOwnObjectCommand : public ScriptCommand
{
public:
    OwnObjectRequest request;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd184_ResetTimer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd184_ResetTimer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd184_ResetTimer_Execute);
    u32 Size() RETAIL(Cmd184_ResetTimer_GetSize);
};
CHECK_SIZE(RestoreOwnObjectCommand, 0x10);

union QueueVideoFlags
{
    u32 value;
    struct
    {
        u32 loops : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(QueueVideoFlags, 4);

// 185: a music-only cutscene queued (a track of the cutscenes' music slot)
class QueueVideoCommand : public ScriptCommand
{
public:
    TaggedValue track;
    QueueVideoFlags flags;

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

// A shadow command's slot of the node's instance's shadow node (ShadowNode::Slots)
union ShadowSlotArgument
{
    u32 value;
    struct
    {
        u32 slot : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(ShadowSlotArgument, 4);

// A shadow shape command's word: the slot whose shapes get it, the shape's kind (ShadowShapeOfToken's), its joint and a
// capsule's second joint (a circle reads no second joint, a plain shape neither joint)
union ShadowShapeArgument
{
    u32 value;
    struct
    {
        u32 slot : 8;
        u32 kind : 8;
        u32 joint : 8;
        u32 secondJoint : 8;
    };
};
CHECK_SIZE(ShadowShapeArgument, 4);

// 187: a shadow slot of the node's instance given new shapes, cast while the instance is within a distance of the camera, as
// strongly as nearStrength up close and farStrength at that distance
class SetShadowCommand : public ScriptCommand
{
public:
    ShadowSlotArgument slot;
    TaggedValue distance;
    TaggedValue nearStrength;
    TaggedValue farStrength;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd187_SetShadow_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd187_SetShadow_ExecuteOn);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd187_SetShadow_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd187_SetShadow_Dtor);
    u32 Size() RETAIL(Cmd187_SetShadow_GetSize);
};
CHECK_SIZE(SetShadowCommand, 0x1C);

// 188: a circle added to the shapes of a shadow slot
class SetShadowCircleCommand : public ScriptCommand
{
public:
    ShadowShapeArgument shape;
    TaggedValue radius;
    TaggedValue height;
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

// 189: a capsule added to the shapes of a shadow slot
class SetShadowMeshCommand : public ScriptCommand
{
public:
    ShadowShapeArgument shape;
    TaggedValue radius;
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

// 190: the plain shape (a rectangle) of a shadow slot's shapes set
class SetShadowRectangleCommand : public ScriptCommand
{
public:
    ShadowShapeArgument shape;
    TaggedValue width;
    TaggedValue depth;
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

// 191: the slot the instance's shadow node casts
class SetShadowSlotCommand : public ScriptCommand
{
public:
    ShadowSlotArgument slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd191_ShadowToggle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd191_ShadowToggle_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd191_ShadowToggle_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd191_ShadowToggle_ExecuteOn);
    u32 Size() RETAIL(Cmd191_ShadowToggle_GetSize);
};
CHECK_SIZE(SetShadowSlotCommand, 0x10);

// 192: a slot of the instance's shadow node made empty
class ClearShadowSlotCommand : public ScriptCommand
{
public:
    ShadowSlotArgument slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd192_SetNode10Slot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd192_SetNode10Slot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd192_SetNode10Slot_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd192_SetNode10Slot_ExecuteOn);
    u32 Size() RETAIL(Cmd192_SetNode10Slot_GetSize);
};
CHECK_SIZE(ClearShadowSlotCommand, 0x10);

// LaunchAtTarget's bits: the target (the space it's in, a receiver's instance or a designator's), the velocity given (else the
// throw to the target, in a time or over a height, the target's height above the node added when asked), the offset given
union LaunchAtTargetBits
{
    u32 value;
    struct
    {
        u32 space : 4;
        u32 receiver : 8;
        u32 designator : 8;
        u32 velocityGiven : 1;
        u32 offsetGiven : 1;
        u32 thrownInTime : 1;
        u32 thrownOverHeight : 1;
        u32 addsRise : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(LaunchAtTargetBits, 4);

// 193: the node's velocity (given or a throw to a target) given to its physics body too, spinning
class LaunchAtTargetCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    LaunchAtTargetBits launch;
    f32 heightOrTime;
    f32 spinX;
    f32 spinY;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd193_LaunchAtTarget_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd193_LaunchAtTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd193_LaunchAtTarget_Execute);
    u32 Size() RETAIL(Cmd193_LaunchAtTarget_GetSize);
};
CHECK_SIZE(LaunchAtTargetCommand, 0x30);

// SetContactSounds' slots of the node's contact sound (first and last, 0xFF none), and the node's contact sounds turned off
union ContactSoundSlots
{
    u32 value;
    struct
    {
        u32 first : 8;
        u32 last : 8;
        u32 off : 1;
        u32 unused17 : 15;
    };
};
CHECK_SIZE(ContactSoundSlots, 4);

// 194: the node's contact sound: its slots and the value it plays while it's negative
class SetContactSoundsCommand : public ScriptCommand
{
public:
    ContactSoundSlots slots;
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd194_SetNodeBytes168_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd194_SetNodeBytes168_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd194_SetNodeBytes168_Execute);
    u32 Size() RETAIL(Cmd194_SetNodeBytes168_GetSize);
};
CHECK_SIZE(SetContactSoundsCommand, 0x14);

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

// What the development tools' parser packs into NoOp197's first argument: an object, an exit point (bit 20 set with it) and
// whether an offset (the floats from unused6) was given
union NoOp197Bits
{
    u32 value;
    struct
    {
        u32 object : 16;
        u32 unused16 : 4;
        u32 hasExitPoint : 1;
        u32 exitPoint : 6;
        u32 hasOffset : 1;
        u32 unused28 : 4;
    };
};
CHECK_SIZE(NoOp197Bits, 4);

// 197: does nothing
class NoOp197Command : public ScriptCommand
{
public:
    NoOp197Bits unused1;
    f32 unused2;
    u32 unused3;
    u32 unused4;
    u32 unused5;
    f32 unused6;
    f32 unused7;
    f32 unused8;
    f32 unused9;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd197_DUMMY_197_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd197_DUMMY_197_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd197_DUMMY_197_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd197_DUMMY_197_ExecuteOn);
    u32 Size() RETAIL(Cmd197_DUMMY_197_GetSize);
};
CHECK_SIZE(NoOp197Command, 0x30);

// 198: the instance's collision a box of the sizes centred on it
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

// 199: the linked object the linked object commands are at made the next one of a list
class NextLinkedObjectInListCommand : public ScriptCommand
{
public:
    // How many linked objects' numbers it has
    union Count
    {
        u32 value;
        struct
        {
            u32 numbers : 8;
            u32 unused8 : 24;
        };
    };

    // The linked objects' numbers (counting from 1)
    u8 numbers[16];
    Count count;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd199_NextLinkedObjectInList_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd199_NextLinkedObjectInList_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd199_NextLinkedObjectInList_Execute);
    u32 Size() RETAIL(Cmd199_NextLinkedObjectInList_GetSize);
};
CHECK_SIZE(NextLinkedObjectInListCommand, 0x20);

// PickLinkedObjectNearPlayer's bits: what the object's node's slot 39 is told to give the linked object to (nothing sets it),
// the player's position turned about the agent's instance and led by the agent's velocity, the busy linked objects passed over
// and the one picked marked busy. The development tools' parser writes bytes at bits 4 and 12, which nothing reads
union LinkedPick
{
    u32 value;
    struct
    {
        u32 designator : 4;
        u32 unused4 : 8;
        u32 unused12 : 8;
        u32 turned : 1;
        u32 leads : 1;
        u32 passesBusy : 1;
        u32 marksBusy : 1;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(LinkedPick, 4);

// 200, 201: the linked object nearest a point by the player picked
class PickLinkedObjectNearPlayerCommand : public ScriptCommand
{
public:
    f32 degrees;
    f32 leadSeconds;
    LinkedPick pick;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd200_ArrangeLinkedObjects_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd200_ArrangeLinkedObjects_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd200_ArrangeLinkedObjects_Dtor);
    u32 Size() RETAIL(Cmd200_ArrangeLinkedObjects_GetSize);
};
CHECK_SIZE(PickLinkedObjectNearPlayerCommand, 0x18);

// 202: the first camera trigger overlapping the instance's box put to sleep
class TriggerInstanceAtOwnBoxCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd202_TriggerInstanceAtOwnBox_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd202_TriggerInstanceAtOwnBox_Execute);
    u32 Size() RETAIL(Cmd202_TriggerInstanceAtOwnBox_GetSize);
};
CHECK_SIZE(TriggerInstanceAtOwnBoxCommand, 0xC);

// 203: the mass of the instance's physics body (its size 1)
class SetBodyMassCommand : public ScriptCommand
{
public:
    f32 mass;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd203_ScaleModelNode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd203_ScaleModelNode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd203_ScaleModelNode_Execute);
    u32 Size() RETAIL(Cmd203_ScaleModelNode_GetSize);
};
CHECK_SIZE(SetBodyMassCommand, 0x10);

// 204
class SetFocusPositionOffsetCommand : public ScriptCommand
{
public:
    // The coordinates and the offsets used (a plain word, not a tagged value)
    union Uses
    {
        u32 value;
        struct
        {
            u32 x : 1;
            u32 y : 1;
            u32 z : 1;
            u32 offsetX : 1;
            u32 offsetY : 1;
            u32 offsetZ : 1;
            u32 unused6 : 26;
        };
    };

    f32 x;
    f32 y;
    f32 z;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 distance;
    Uses uses;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd204_SetFocusPositionOffset_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd204_SetFocusPositionOffset_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd204_SetFocusPositionOffset_Execute);
    u32 Size() RETAIL(Cmd204_SetFocusPositionOffset_GetSize);
};
CHECK_SIZE(SetFocusPositionOffsetCommand, 0x2C);

// 205
class SetStoredPositionAtAngleCommand : public ScriptCommand
{
public:
    f32 distance;
    // Degrees
    f32 angle;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd205_SetFocusPositionAtAngle_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd205_SetFocusPositionAtAngle_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd205_SetFocusPositionAtAngle_Execute);
    u32 Size() RETAIL(Cmd205_SetFocusPositionAtAngle_GetSize);
};
CHECK_SIZE(SetStoredPositionAtAngleCommand, 0x14);

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

// 208: the node's rank (what TriggerInstancesByRank compares)
class SetRankCommand : public ScriptCommand
{
public:
    TaggedValue rank;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd208_SetNodeByte8c_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd208_SetNodeByte8c_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd208_SetNodeByte8c_Dtor);
    u32 Size() RETAIL(Cmd208_SetNodeByte8c_GetSize);
};
CHECK_SIZE(SetRankCommand, 0x10);

// 209: the rank TriggerInstancesByRank compares with
class SetTriggerRankCommand : public ScriptCommand
{
public:
    TaggedValue rank;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd209_SetGlobalByte30a0e9_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd209_SetGlobalByte30a0e9_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd209_SetGlobalByte30a0e9_Dtor);
    u32 Size() RETAIL(Cmd209_SetGlobalByte30a0e9_GetSize);
};
CHECK_SIZE(SetTriggerRankCommand, 0x10);

// What TriggerInstancesByRank sends to whom: the message, and the instances' ranks it takes: the same as the node's (0), the
// trigger rank (1), below it (2) or not above it (3), none past 3
union RankMessage
{
    enum Mode : u32
    {
        SameRank = 0,
        TriggerRank = 1,
        BelowTriggerRank = 2,
        UpToTriggerRank = 3,
        Modes = 4,
    };

    u32 value;
    struct
    {
        u32 message : 16;
        u32 mode : 8;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(RankMessage, 4);

// 210: a message sent to the chunk's instances of a rank
class TriggerInstancesByRankCommand : public ScriptCommand
{
public:
    RankMessage message;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd210_TriggerInstancesInRange_ParseTokens);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd210_TriggerInstancesInRange_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd210_TriggerInstancesInRange_Dtor);
    u32 Size() RETAIL(Cmd210_TriggerInstancesInRange_GetSize);
};
CHECK_SIZE(TriggerInstancesByRankCommand, 0x10);

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

// 213: the waypoints' route started again
class RestartRouteCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd213_KeyOfPath34Op213_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd213_KeyOfPath34Op213_Execute);
    u32 Size() RETAIL(Cmd213_KeyOfPath34Op213_GetSize);
};
CHECK_SIZE(RestartRouteCommand, 0xC);

// 214
class ControllerRumbleCommand : public ScriptCommand
{
public:
    f32 strength;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd214_ControllerRumble_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd214_ControllerRumble_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd214_ControllerRumble_Execute);
    u32 Size() RETAIL(Cmd214_ControllerRumble_GetSize);
};
CHECK_SIZE(ControllerRumbleCommand, 0x10);

// Which of SetSoundParams' values it sets
union SoundParamsSet
{
    u32 value;
    struct
    {
        u32 pitch : 1;
        u32 volume : 1;
        u32 unused2 : 30;
    };
};
CHECK_SIZE(SoundParamsSet, 4);

// 215: the node's tracked sound's pitch scale and volume
class SetSoundParamsCommand : public ScriptCommand
{
public:
    SoundParamsSet sets;
    f32 pitch;
    f32 volume;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd215_SetSoundParams_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd215_SetSoundParams_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd215_SetSoundParams_Execute);
    u32 Size() RETAIL(Cmd215_SetSoundParams_GetSize);
};
CHECK_SIZE(SetSoundParamsCommand, 0x18);

// A crate's contents: the objects of its first and second contents (0xFFFF none)
union CrateContents
{
    u32 value;
    struct
    {
        u32 first : 16;
        u32 second : 16;
    };
};
CHECK_SIZE(CrateContents, 4);

// How many of a crate's first contents come out: from the least to the most
union CrateContentsRange
{
    u32 value;
    struct
    {
        u32 least : 4;
        u32 most : 4;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(CrateContentsRange, 4);

// 512: the crate's contents made
class CreateCrateContentsCommand : public ScriptCommand
{
public:
    CrateContents contents;
    CrateContentsRange count;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd512_CreateCrateContents_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd512_CreateCrateContents_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd512_CreateCrateContents_Execute);
    u32 Size() RETAIL(Cmd512_CreateCrateContents_GetSize);
};
CHECK_SIZE(CreateCrateContentsCommand, 0x14);

// 513
class PickUpWumpaCommand : public ScriptCommand
{
public:
    s32 wumpaFruit;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd513_CA_PickUpWumpa_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd513_CA_PickUpWumpa_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd513_CA_PickUpWumpa_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd513_CA_PickUpWumpa_ExecuteOn);
    u32 Size() RETAIL(Cmd513_CA_PickUpWumpa_GetSize);
};
CHECK_SIZE(PickUpWumpaCommand, 0x10);

// 514, 546
class CreateDamageCommand : public ScriptCommand
{
public:
    // The contact message's point's w given, its kinds with 0x400 (instant death), the offset (turned with the instance), the
    // instances searched for (but the playable characters, only them, else both), the joint the damage is at (0xFF the place),
    // only the nearest instance hit
    union Flags
    {
        u32 value;
        struct
        {
            u32 givesMessageW : 1;
            u32 instantDeath : 1;
            u32 offsetGiven : 1;
            u32 skipsCharacters : 1;
            u32 onlyCharacters : 1;
            u32 joint : 8;
            u32 nearestOnly : 1;
            u32 unused14 : 18;
        };
    };

    // The shape searched: a sphere of the reach (its instances on every one or not), a cylinder, else a damage hull (0xF none)
    union Shape
    {
        static constexpr u32 NoHull = 0xF;

        u32 value;
        struct
        {
            u32 kind : 4;
            u32 hull : 4;
            u32 unused8 : 24;
        };
    };

    enum ShapeKind : u32
    {
        ShapeSphereOnEvery = 1,
        ShapeSphere = 2,
        ShapeCylinder = 3,
    };

    u32 unused0C;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    u32 unused1C;
    Flags flags;
    u32 hitKinds;
    TaggedValue damage;
    TaggedValue messageW;
    Shape shape;
    TaggedValue reach;
    TaggedValue height;
    u32 unused3C;

    // A contact message of damage sent to the instances in a shape at the instance (or one of its joints)
    void ExecuteOn(GameNode* node) RETAIL(Cmd514_CreateDamage_ExecuteOn);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd514_CreateDamage_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd514_CreateDamage_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd514_CreateDamage_Dtor);
    u32 Size() RETAIL(Cmd514_CreateDamage_GetSize);
    // Its vtable's slot 7, a token: 0xCD the reach, 0xA9 the height (a cylinder), the keyword 0x216 after 0x236 (damage hull 0), else
    // the base's (DamageOriginator's too): 0x82 to 0x84 the offset, 0x12 the joint, 0x204 the damage, 0x94 the w, 0x217 the
    // instant death (its value 0), the keywords of the instances searched for, of only the nearest and of the kinds of hit
    void ParseToken(const ScriptToken* token) RETAIL(FUN_0011fa18);
    void ParseBaseToken(const ScriptToken* token) RETAIL(FUN_0010e088);
    // Its base's destructor and size (vtable D_002F0098, which nothing makes alone)
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0011c450);
    u32 BaseSize() RETAIL(FUN_0011c4a8);
};
CHECK_OFFSET(CreateDamageCommand, shape, 0x30);
CHECK_SIZE(CreateDamageCommand, 0x40);

// A command's word of settings: which are given (a bit each) and their values (the same bits)
union GivenSettings
{
    u32 value;
    struct
    {
        u32 given : 16;
        u32 values : 16;
    };
};
CHECK_SIZE(GivenSettings, 4);

// 515
class SetAgentCommand : public ScriptCommand
{
public:
    // The settings' bits: its instance awake, the instance's flags (visible, its collision active, triggers' signals, the shadow),
    // its creature part's snapping to the ground, its part's bits (it may damage the character, it may be hurt, bullets bounce
    // back, it's targettable)
    enum Setting : u32
    {
        SettingAwake = 0,
        SettingVisible = 1,
        SettingCollision = 2,
        SettingTriggerSignals = 3,
        SettingShadow = 4,
        SettingSnapsToGround = 5,
        SettingCanDamageCharacter = 6,
        SettingVulnerable = 7,
        SettingBulletsBounceBack = 8,
        SettingTargettable = 9,
        SettingCount = 10,
    };

    GivenSettings settings;

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
    // The last checkpoint (one that saves the game) rather than the start's
    union Respawn
    {
        u32 value;
        struct
        {
            u32 unused0 : 8;
            u32 saves : 8;
            u32 unused16 : 16;
        };
    };

    Respawn respawn;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd516_SetPlayerRespawnPosition_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd516_SetPlayerRespawnPosition_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd516_SetPlayerRespawnPosition_Execute);
    u32 Size() RETAIL(Cmd516_SetPlayerRespawnPosition_GetSize);
};
CHECK_SIZE(SetPlayerRespawnPositionCommand, 0x10);

// 517: play again from the last checkpoint (the game over without lives)
class RestartFromCheckpointCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd517_ResetGame_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd517_ResetGame_Execute);
    u32 Size() RETAIL(Cmd517_ResetGame_GetSize);
};
CHECK_SIZE(RestartFromCheckpointCommand, 0xC);

// 518: the wumpa fruit in a crate (none below 0: they're left as they are)
class SetCrateCommand : public ScriptCommand
{
public:
    u32 unused1;
    u32 wumpaFruit;

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
class PickUpHealthCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd520_CA_PickUpHealth_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd520_CA_PickUpHealth_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd520_CA_PickUpHealth_ExecuteOn);
    u32 Size() RETAIL(Cmd520_CA_PickUpHealth_GetSize);
};
CHECK_SIZE(PickUpHealthCommand, 0xC);

// 521
class SetPlayerInputCommand : public ScriptCommand
{
public:
    // The settings' bits: the inputs the character may use (agents.h's CharacterLocks, locked when not), all of them, its part's
    // resting flag, its being vulnerable
    enum Setting : u32
    {
        SettingTurn = 0,
        SettingMoveZ = 1,
        SettingMoveX = 2,
        SettingCross = 3,
        SettingSquare = 4,
        SettingCircle = 5,
        SettingAllInputs = 6,
        SettingResting = 7,
        SettingVulnerable = 8,
        SettingCount = 9,
    };

    // The character's controls' node driven by its motion (its buttons left as they were when they aren't all given back), its
    // state's boxOnly
    union Controls
    {
        u32 value;
        struct
        {
            u32 motionDriven : 1;
            u32 boxOnly : 1;
            u32 unused2 : 30;
        };
    };

    GivenSettings settings;
    Controls controls;

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

// What the velocity commands were given (a plain word, not a tagged value): the velocity, the point (nothing reads either), and
// what makes ApplyVelocity cast its ray: its unused values or the keyword asking for it
union ThrowGiven
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 unused1 : 1;
        u32 rayValues : 1;
        u32 castsRay : 1;
        u32 unused4 : 28;
    };
};
CHECK_SIZE(ThrowGiven, 4);

// 523: ApplyVelocityToSelf's values first (retail's derived class), the point also the ray's reach
class ApplyVelocityCommand : public ScriptCommand
{
public:
    TaggedValue gravity;
    TaggedValue velocityX;
    TaggedValue velocityY;
    TaggedValue velocityZ;
    TaggedValue pointX;
    TaggedValue pointY;
    TaggedValue pointZ;
    ThrowGiven given;
    TaggedValue unused2C;
    TaggedValue unused30;
    TaggedValue unused34;
    // Whose instance it goes to without the ray
    s32 receiver;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd523_ApplyVelocity_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd523_ApplyVelocity_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd523_ApplyVelocity_Execute);
    // Its vtable's slot 7: a token of its own (6 the receiver, 0x46 to 0x48 the unused values, rayValues), else
    // ApplyVelocityToSelf's
    u32 ParseToken(const ScriptToken* token) RETAIL(FUN_0011efb8);
    u32 Size() RETAIL(Cmd523_ApplyVelocity_GetSize);
};
CHECK_SIZE(ApplyVelocityCommand, 0x3C);

// SetKeyNearestPlayer's keys: the range of keys it picks from (inclusive) and whether it may pick the current one (any of the
// low byte)
union KeyRangeArgument
{
    u32 value;
    struct
    {
        u32 takesCurrent : 8;
        u32 first : 8;
        u32 last : 8;
        u32 unused24 : 8;
    };
};
CHECK_SIZE(KeyRangeArgument, 4);

// 524: the agent's key the one nearest where the played character goes (of those near it, the one nearest the agent)
class SetKeyNearestPlayerCommand : public ScriptCommand
{
public:
    f32 nearDistanceSquared;
    f32 leadSeconds;
    KeyRangeArgument keys;

    static SetKeyNearestPlayerCommand* Construct(SetKeyNearestPlayerCommand* command) RETAIL(FUN_001216d8);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd524_SetKeyNearestPlayer_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd524_SetKeyNearestPlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd524_SetKeyNearestPlayer_Dtor);
    u32 Size() RETAIL(Cmd524_SetKeyNearestPlayer_GetSize);
};
CHECK_SIZE(SetKeyNearestPlayerCommand, 0x18);

// 525: the focus position a ray from a designator's instance or position reaches (turned by the instance's place in the current
// and the target space: ControlPacket::Space), pulled back by a distance
class RaycastFocusPositionCommand : public ScriptCommand
{
public:
    u32 unused1;
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    f32 distance;
    s32 unused7;
    u32 space;
    u32 unused9;
    DesignatorArgument target;
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

// 526: the agent's instance launched at a velocity, or thrown to a point that far away, under a gravity
class ApplyVelocityToSelfCommand : public ScriptCommand
{
public:
    // The velocity the agent's instance is launched with, or (not 0) the point it's thrown to under the gravity
    TaggedValue gravity;
    TaggedValue velocityX;
    TaggedValue velocityY;
    TaggedValue velocityZ;
    TaggedValue pointX;
    TaggedValue pointY;
    TaggedValue pointZ;
    ThrowGiven given;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd526_ApplyVelocityToSelf_Execute);
    void Destroy(u32 destroyFlags) RETAIL(Cmd526_ApplyVelocityToSelf_Dtor);
    u32 Size() RETAIL(Cmd526_ApplyVelocityToSelf_GetSize);
    // Its vtable's slot 7 (ApplyVelocity's falls back on it), a token: 0x6A the gravity, 0x49 to 0x4B the velocity, 0x4C to 0x4E the
    // point, the keyword 0xAC (castsRay). Whether it took it
    u32 ParseToken(const ScriptToken* token) RETAIL(FUN_0010c758);
};
CHECK_SIZE(ApplyVelocityToSelfCommand, 0x2C);

// SetChiChiGrass's setting: given, and its value
union GrabbableSetting
{
    u32 value;
    struct
    {
        u32 given : 1;
        u32 unused1 : 15;
        u32 on : 1;
        u32 unused17 : 15;
    };
};
CHECK_SIZE(GrabbableSetting, 4);

// 527: a bit of the grabbable part that nothing reads
class SetChiChiGrassCommand : public ScriptCommand
{
public:
    GrabbableSetting setting;

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

// 530: does nothing
class NoOpSetRayTestsCommand : public ScriptCommand
{
public:
    s32 unused1;
    s32 unused2;
    s32 unused3;
    s32 unused4;
    s32 unused5;

    static NoOpSetRayTestsCommand* Construct(NoOpSetRayTestsCommand* command) RETAIL(FUN_0011f370);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd530_DUMMY_SetRayTests_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd530_DUMMY_SetRayTests_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd530_DUMMY_SetRayTests_Execute);
    u32 Size() RETAIL(Cmd530_DUMMY_SetRayTests_GetSize);
};
CHECK_SIZE(NoOpSetRayTestsCommand, 0x20);

// 532: does nothing
class NoOpNowGoForwardCollidableCommand : public ScriptCommand
{
public:
    TaggedValue unused1;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_Dtor);
    u32 Size() RETAIL(Cmd532_DUMMY_NowGoForwardCollidable_GetSize);
};
CHECK_SIZE(NoOpNowGoForwardCollidableCommand, 0x10);

// 533: the character pushed back by its node's velocity times a scale
class NowGoBackCollidableCommand : public ScriptCommand
{
public:
    TaggedValue scale;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd533_NowGoBackCollidable_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd533_NowGoBackCollidable_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd533_NowGoBackCollidable_Dtor);
    u32 Size() RETAIL(Cmd533_NowGoBackCollidable_GetSize);
};
CHECK_SIZE(NowGoBackCollidableCommand, 0x10);

// 534
class SetPlayAreaCommand : public ScriptCommand
{
public:
    // The area's own value: the tagged value's instead
    static constexpr s32 FromValue = -1;

    s32 area;
    TaggedValue areaValue;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd534_SetGlobalProgression_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd534_SetGlobalProgression_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd534_SetGlobalProgression_Execute);
    u32 Size() RETAIL(Cmd534_SetGlobalProgression_GetSize);
};
CHECK_SIZE(SetPlayAreaCommand, 0x14);

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

// 536: does nothing
class NoOp536Command : public ScriptCommand
{
public:
    u32 unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd536_DUMMY_536_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd536_DUMMY_536_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd536_DUMMY_536_Execute);
    u32 Size() RETAIL(Cmd536_DUMMY_536_GetSize);
};
CHECK_SIZE(NoOp536Command, 0x10);

// 537
class AddGemCommand : public ScriptCommand
{
public:
    s32 gem;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd537_AddGem_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd537_AddGem_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd537_AddGem_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd537_AddGem_ExecuteOn);
    u32 Size() RETAIL(Cmd537_AddGem_GetSize);
};
CHECK_SIZE(AddGemCommand, 0x10);

// 538: does nothing
class NoOp538Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd538_DUMMY_538_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd538_DUMMY_538_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd538_DUMMY_538_ExecuteOn);
    u32 Size() RETAIL(Cmd538_DUMMY_538_GetSize);
};
CHECK_SIZE(NoOp538Command, 0xC);

// 539: the custom pickup of the node's slot (game/pickups.cpp)
class SetCustomPickupCommand : public ScriptCommand
{
public:
    f32 radius;
    f32 pull;
    f32 fleeSpeed;
    CustomPickupFlags flags;

    static SetCustomPickupCommand* Construct(SetCustomPickupCommand* command) RETAIL(FUN_001292c0);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd539_CA_SetPickup_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd539_CA_SetPickup_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd539_CA_SetPickup_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd539_CA_SetPickup_ExecuteOn);
    u32 Size() RETAIL(Cmd539_CA_SetPickup_GetSize);
};
CHECK_SIZE(SetCustomPickupCommand, 0x1C);

// What SetCustomProjectile sets besides the speed: the projectile homes in (turning by turn, across times sideTurnScale), falls (by
// gravity), stops homing in after homingTime, is the player's shot. The development tools' parser puts hit points in bits 0-3,
// which nothing reads
union CustomProjectileSettings
{
    u32 value;
    struct
    {
        u32 unused0 : 4;
        u32 homes : 1;
        u32 falls : 1;
        u32 homesForATime : 1;
        u32 playersShot : 1;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(CustomProjectileSettings, 4);

// 540: the custom projectile of the node's slot (game/projectiles.cpp)
class SetCustomProjectileCommand : public ScriptCommand
{
public:
    CustomProjectileSettings settings;
    f32 speed;
    f32 turn;
    f32 sideTurnScale;
    f32 homingTime;
    TaggedValue gravity;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd540_CA_SetProjectile_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd540_CA_SetProjectile_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd540_CA_SetProjectile_Dtor);
    void ExecuteOn(GameNode* node) RETAIL(Cmd540_CA_SetProjectile_ExecuteOn);
    u32 Size() RETAIL(Cmd540_CA_SetProjectile_GetSize);
};
CHECK_SIZE(SetCustomProjectileCommand, 0x24);

// What Shoot makes: the object (bits 0-14 its ID) and the trigger message sent to it (0xFFFF none)
union ShotObject
{
    u32 value;
    struct
    {
        u32 id : 16;
        u32 message : 16;
    };
};
CHECK_SIZE(ShotObject, 4);

// How Shoot shoots: the exit point it's shot from (0xFF the instance's place), the offset taken along the frame's axes, shot
// along the frame's z axis at the speed, at AgentRef1, not bouncing off what's bullets bounce back. The development tools' parser
// puts an axes mode in bits 8-10 and sets bit 12, which nothing reads
union ShotSettings
{
    u32 value;
    struct
    {
        u32 exitPoint : 8;
        u32 unused8 : 3;
        u32 offsetGiven : 1;
        u32 unused12 : 1;
        u32 speedGiven : 1;
        u32 atAgentRef1 : 1;
        u32 noBounce : 1;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(ShotSettings, 4);

// 548
class ShootCommand : public ScriptCommand
{
public:
    u32 unused1;
    Vector4 offset;
    ShotObject object;
    ShotSettings shot;
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

// GetShortRoute's target and route: the end's receiver and designator (else the space's position), the paths a route takes (those
// needing jumps, long jumps (with jumps), high jumps (with jumps) and flights: AI path flags 2-5), the request's roll radius and
// weight (which nothing reads), the route kept near the path finder's focus or away from it, the offset given, the steps costing
// their distance alone or the positions' own costs too, and the space
union ShortRouteTarget
{
    u32 value;
    struct
    {
        u32 receiver : 8;
        u32 designator : 8;
        u32 unused16 : 1;
        u32 takesJumps : 1;
        u32 takesLongJumps : 1;
        u32 takesHighJumps : 1;
        u32 takesFlights : 1;
        u32 givesRollRadius : 1;
        u32 givesWeight : 1;
        u32 nearFocus : 1;
        u32 avoidsFocus : 1;
        u32 offsetGiven : 1;
        u32 distanceOnly : 1;
        u32 positionCosts : 1;
        u32 space : 4;
    };
};
CHECK_SIZE(ShortRouteTarget, 4);

// GetShortRoute's options: the start ahead along the agent's z axis, and the paths a route takes (those with AI path flags 6, 7
// and 8, and those with none of 5-8)
union ShortRouteOptions
{
    u32 value;
    struct
    {
        u32 startsAhead : 1;
        u32 takesPathFlag6 : 1;
        u32 takesPathFlag7 : 1;
        u32 takesPathFlag8 : 1;
        u32 takesPlainPaths : 1;
        u32 unused5 : 27;
    };
};
CHECK_SIZE(ShortRouteOptions, 4);

// GetShortRoute's AI position flags its start and end must have and mustn't have (game/navigation.h's AiPosition, none: any)
// over three words, and the request's kind byte (which nothing reads)
union RouteEndRequired
{
    u32 value;
    struct
    {
        u32 kind : 8;
        u32 unused8 : 8;
        u32 endRequired : 16;
    };
};
CHECK_SIZE(RouteEndRequired, 4);

union RoutePositionFlags
{
    u32 value;
    struct
    {
        u32 endRuledOut : 16;
        u32 startRequired : 16;
    };
};
CHECK_SIZE(RoutePositionFlags, 4);

union RouteStartRuledOut
{
    u32 value;
    struct
    {
        u32 startRuledOut : 16;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(RouteStartRuledOut, 4);

// 549: a route from the AI position nearest the agent to the one nearest a target
class GetShortRouteCommand : public ScriptCommand
{
public:
    f32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    ShortRouteTarget target;
    ShortRouteOptions options;
    TaggedValue avoidFocusWeight;
    TaggedValue nearFocusWeight;
    TaggedValue weight;
    TaggedValue positionCostWeight;
    TaggedValue ahead;
    RouteEndRequired endFlags;
    RoutePositionFlags positionFlags;
    RouteStartRuledOut startFlags;
    f32 startRange;
    f32 endRange;

    static GetShortRouteCommand* Construct(GetShortRouteCommand* command) RETAIL(FUN_0011ccd0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd549_GetShortRoute_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd549_GetShortRoute_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd549_GetShortRoute_Dtor);
    u32 Size() RETAIL(Cmd549_GetShortRoute_GetSize);
};
CHECK_SIZE(GetShortRouteCommand, 0x50);

// 550: does nothing
class NoOpFuelPayGateCommand : public ScriptCommand
{
public:
    u32 unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd550_DUMMY_FuelPayGate_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd550_DUMMY_FuelPayGate_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd550_DUMMY_FuelPayGate_Execute);
    u32 Size() RETAIL(Cmd550_DUMMY_FuelPayGate_GetSize);
};
CHECK_SIZE(NoOpFuelPayGateCommand, 0x10);

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

// Where AttachAllLinkedAgents attaches: an exit point given, and the exit point
union AttachAllSettings
{
    u32 value;
    struct
    {
        u32 exitPointGiven : 1;
        u32 exitPoint : 6;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(AttachAllSettings, 4);

// 553: every linked object attached to the instance, on an exit point when given
class AttachAllLinkedAgentsCommand : public ScriptCommand
{
public:
    AttachAllSettings settings;

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

// A command's two of the progress's characters (PlayableCharacter)
union CharacterPairArgument
{
    u32 value;
    struct
    {
        u32 first : 8;
        u32 second : 8;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(CharacterPairArgument, 4);

// 555
class SetVehicleHumiliskateCommand : public ScriptCommand
{
public:
    // The skater, then the character skated on
    CharacterPairArgument characters;
    TaggedValue topSpeed;
    TaggedValue crouchedSpeed;

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

// RequestFocus's target word: where the sphere it looks in is (the space, a receiver's instance (NoReceiver none), a designator's
// position), how many of the objects it asks for, the nearest one taken or a random one (else the first), the offset given, which
// instances (RequestFocusCommand::Attachment) and whether busy ones are wanted (RequestFocusCommand::Busy), the one found marked
// busy and made AgentRef2 too. Bit 21 is handed to DesignatedPosition, which ignores it
union RequestTarget
{
    static constexpr u32 NoReceiver = 0xF;

    u32 value;
    struct
    {
        u32 space : 4;
        u32 receiver : 4;
        u32 designator : 8;
        u32 objectCount : 3;
        u32 nearest : 1;
        u32 offsetGiven : 1;
        u32 unused21 : 1;
        u32 random : 1;
        u32 attachment : 4;
        u32 busy : 2;
        u32 marksBusy : 1;
        u32 alsoAgentRef2 : 1;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(RequestTarget, 4);

// RequestFocus's choice: what the instance found is given to (0 the focus, 1 AgentRef1, 2 AgentRef2), instances without the
// trigger signals flag taken too, how many objects of the hanging instances it asks for, the current one taken again, only
// visible ones
union RequestChoice
{
    u32 value;
    struct
    {
        u32 slot : 2;
        u32 ignoresSignals : 1;
        u32 hangingCount : 2;
        u32 keepsCurrent : 1;
        u32 visibleOnly : 1;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(RequestChoice, 4);

// 557, 564, 565: an instance of a sphere given to the focus or an agent reference
class RequestFocusCommand : public ScriptCommand
{
public:
    // Which of the instances found it takes: any, those holding something (1, 7) or holding nothing (2, 6, 8), those hanging from
    // something (3, and 5 with the hanging ones' objects) or from nothing (4)
    enum Attachment : u32
    {
        AnyAttachment = 0,
        Holding = 1,
        HoldingNothing = 2,
        Hanging = 3,
        HangingFromNothing = 4,
        HangingFromObjects = 5,
    };

    // Whether the busy instances are wanted or ruled out
    enum Busy : u32
    {
        AnyBusy = 0,
        OnlyBusy = 1,
        NoneBusy = 2,
    };

    // The hanging instances' objects' place among the objects
    static constexpr u32 HangingObjects = 4;

    u32 unused1;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 offsetW;
    // The IDs of the objects asked for (up to seven) and of the hanging ones' (up to three, from the fifth): a seventh, or a
    // third hanging one, is the target word's low half
    union
    {
        u16 objects[8];
        struct
        {
            u16 listedObjects[6];
            RequestTarget target;
        };
    };
    RequestChoice choice;
    // The kinds of nodes the instances have (a bit each)
    u32 kinds;
    f32 radius;
    u32 unused13;

    static RequestFocusCommand* Construct(RequestFocusCommand* command, u32 mode) RETAIL(FUN_0011d4a0);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd557_RequestFocus_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd557_RequestFocus_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd557_RequestFocus_Dtor);
    u32 Size() RETAIL(Cmd557_RequestFocus_GetSize);
};
CHECK_SIZE(RequestFocusCommand, 0x40);

// SetFocusProperties' switches of the focus instance (1 on, 2 off, 0 left): awake (on wakes it, off puts it to sleep), its flags
// and its part's damaging the character
union FocusProperties
{
    u32 value;
    struct
    {
        u32 awake : 2;
        u32 visible : 2;
        u32 collisionActive : 2;
        u32 receivesTriggerSignals : 2;
        u32 canDamageCharacter : 2;
        u32 unused10 : 22;
    };
};
CHECK_SIZE(FocusProperties, 4);

// 558: the focus instance's flags switched
class SetFocusPropertiesCommand : public ScriptCommand
{
public:
    FocusProperties properties;

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
    // The values given (a plain word, not a tagged value)
    union Given
    {
        u32 value;
        struct
        {
            u32 pitch : 1;
            u32 distance : 1;
            u32 unused2 : 30;
        };
    };

    // The follow camera's pitch (an angle) and distance
    Given given;
    TaggedValue pitch;
    f32 distance;

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
    union Link
    {
        u32 value;
        struct
        {
            u32 focusLeads : 1;
            u32 unused1 : 31;
        };
    };

    Link link;

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

// 563: a contact message of damage sent to the runner's originator
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
    // The contact message's kinds of hit and its damage
    u32 hitKinds;
    TaggedValue damage;
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

// 567: the node's stored position the player's
class SetFocusPositionToPlayerCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd567_SetFocusPositionToPlayer_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd567_SetFocusPositionToPlayer_Execute);
    u32 Size() RETAIL(Cmd567_SetFocusPositionToPlayer_GetSize);
};
CHECK_SIZE(SetFocusPositionToPlayerCommand, 0xC);

// An argument of two halfwords, which the development tools' parser fills one at a time
union HalfwordPair
{
    u32 value;
    struct
    {
        u32 low : 16;
        u32 high : 16;
    };
};
CHECK_SIZE(HalfwordPair, 4);

// 568: does nothing
class NoOp568Command : public ScriptCommand
{
public:
    f32 unused1;
    f32 unused2;
    f32 unused3;
    f32 unused4;
    f32 unused5;
    HalfwordPair unused6;
    HalfwordPair unused7;
    HalfwordPair unused8;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd568_DUMMY_568_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd568_DUMMY_568_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd568_DUMMY_568_Execute);
    u32 Size() RETAIL(Cmd568_DUMMY_568_GetSize);
};
CHECK_SIZE(NoOp568Command, 0x2C);

// 569: a character's vehicle left
class ExitVehicleModeCommand : public ScriptCommand
{
public:
    // The game's character number
    s32 character;

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
    // The driver, then the passenger
    CharacterPairArgument characters;

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
    // The receiver whose character rides, the hoverboard's own controls
    union Rider
    {
        u32 value;
        struct
        {
            u32 receiver : 8;
            u32 boardControls : 1;
            u32 unused9 : 23;
        };
    };

    Rider rider;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd571_SetVehicleHoverboard_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd571_SetVehicleHoverboard_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd571_SetVehicleHoverboard_Execute);
    u32 Size() RETAIL(Cmd571_SetVehicleHoverboard_GetSize);
};
CHECK_SIZE(SetVehicleHoverboardCommand, 0x10);

// 572: the node follows the motion block of its arguments (its cover search started again)
class SetMotionCommand : public ScriptCommand
{
public:
    MotionBlock block;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd572_SetMotion_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd572_SetMotion_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd572_SetMotion_Dtor);
    u32 Size() RETAIL(Cmd572_SetMotion_GetSize);
};
CHECK_SIZE(SetMotionCommand, 0x94);

// SetNearestPointFlags' switches of the AI position's flags: blocked (1 clears it, 2 sets it), airborne and always taken (1 on,
// 2 off). The development tools' parser sets bit 6 with the second and third arguments, which nothing reads
union NearestPointSwitches
{
    u32 value;
    struct
    {
        u32 blocked : 2;
        u32 airborne : 2;
        u32 alwaysTaken : 2;
        u32 unused6 : 1;
        u32 unused7 : 25;
    };
};
CHECK_SIZE(NearestPointSwitches, 4);

// 573: the flags of the AI position nearest the agent switched
class SetNearestPointFlagsCommand : public ScriptCommand
{
public:
    NearestPointSwitches switches;
    s32 unused2;
    s32 unused3;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd573_SetNearestPointFlags_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd573_SetNearestPointFlags_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd573_SetNearestPointFlags_Dtor);
    u32 Size() RETAIL(Cmd573_SetNearestPointFlags_GetSize);
};
CHECK_SIZE(SetNearestPointFlagsCommand, 0x18);

// 574: the node's head tracking made (it keeps the command's settings)
class CreateHeadTrackingCommand : public ScriptCommand
{
public:
    HeadTrackingSettings settings;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd574_CreateHeadTracking_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd574_CreateHeadTracking_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd574_CreateHeadTracking_Dtor);
    u32 Size() RETAIL(Cmd574_CreateHeadTracking_GetSize);
};
CHECK_SIZE(CreateHeadTrackingCommand, 0x54);

// SetFocusPositionToNearestPoint's receiver (DesignatesNone the player). The development tools' parser sets bit 8 with the
// second argument, which nothing reads
union NearestPointReceiver
{
    u32 value;
    struct
    {
        u32 receiver : 8;
        u32 unused8 : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(NearestPointReceiver, 4);

// 575: the focus position the AI position nearest a receiver's instance or the player
class SetFocusPositionToNearestPointCommand : public ScriptCommand
{
public:
    NearestPointReceiver target;
    f32 unused2;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd575_SetFocusPositionToNearestPoint_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd575_SetFocusPositionToNearestPoint_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd575_SetFocusPositionToNearestPoint_Dtor);
    u32 Size() RETAIL(Cmd575_SetFocusPositionToNearestPoint_GetSize);
};
CHECK_SIZE(SetFocusPositionToNearestPointCommand, 0x14);

// SetFocusToGameActor's character: the game's character number
union GameActorArgument
{
    u32 value;
    struct
    {
        u32 character : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(GameActorArgument, 4);

// 576: a character's instance the focus
class SetFocusToGameActorCommand : public ScriptCommand
{
public:
    GameActorArgument character;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd576_SetFocusToGameActor_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd576_SetFocusToGameActor_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd576_SetFocusToGameActor_Execute);
    u32 Size() RETAIL(Cmd576_SetFocusToGameActor_GetSize);
};
CHECK_SIZE(SetFocusToGameActorCommand, 0x10);

// BecomeSticky's object: the object (0 any) the motion block asks for
union StickyObjectArgument
{
    u32 value;
    struct
    {
        u32 object : 16;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(StickyObjectArgument, 4);

// 577: the node's motion block made sticky
class BecomeStickyCommand : public ScriptCommand
{
public:
    // The kinds of nodes it asks of an instance (added to the block's), how strongly it holds them, the object it asks for and
    // the message what sticks is sent
    s32 kinds;
    f32 strength;
    StickyObjectArgument object;
    s32 message;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd577_BecomeSticky_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd577_BecomeSticky_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd577_BecomeSticky_Execute);
    u32 Size() RETAIL(Cmd577_BecomeSticky_GetSize);
};
CHECK_SIZE(BecomeStickyCommand, 0x1C);

// 578
class CountPlayerCirclingCommand : public ScriptCommand
{
public:
    // The counter (the game's, else the agent's), the player going to the right of the way to the agent's instance (else to the
    // left)
    union Counter
    {
        u32 value;
        struct
        {
            u32 counter : 16;
            u32 agentCounter : 1;
            u32 toTheRight : 1;
            u32 unused18 : 14;
        };
    };

    Counter counter;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd578_CharacterOp578_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd578_CharacterOp578_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd578_CharacterOp578_Execute);
    u32 Size() RETAIL(Cmd578_CharacterOp578_GetSize);
};
CHECK_SIZE(CountPlayerCirclingCommand, 0x10);

// 579
class CountPlayerApproachCommand : public ScriptCommand
{
public:
    // The counter (the game's, else the agent's)
    union Counter
    {
        u32 value;
        struct
        {
            u32 counter : 16;
            u32 agentCounter : 1;
            u32 unused17 : 1;
            u32 unused18 : 14;
        };
    };

    Counter counter;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd579_CounterPositionOp579_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd579_CounterPositionOp579_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd579_CounterPositionOp579_Execute);
    u32 Size() RETAIL(Cmd579_CounterPositionOp579_GetSize);
};
CHECK_SIZE(CountPlayerApproachCommand, 0x10);

// 580: the body the character holds pushed
class ApplyVelocityToHeldBodyCommand : public ScriptCommand
{
public:
    u32 unused1;
    // Turned by the instance's place
    Vector4 impulse;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd580_ApplyVelocityToHeldBody_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd580_ApplyVelocityToHeldBody_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd580_ApplyVelocityToHeldBody_Execute);
    u32 Size() RETAIL(Cmd580_ApplyVelocityToHeldBody_GetSize);
};
CHECK_SIZE(ApplyVelocityToHeldBodyCommand, 0x20);

// 581: the node's motion block made normal, or kept sticky
class BecomeNormalCommand : public ScriptCommand
{
public:
    SwitchArgument staysSticky;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd581_BecomeNormal_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd581_BecomeNormal_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd581_BecomeNormal_Execute);
    u32 Size() RETAIL(Cmd581_BecomeNormal_GetSize);
};
CHECK_SIZE(BecomeNormalCommand, 0x10);

// 582: a sense added to the node's perception (the perception keeps the command's sense)
class AddPerceptionCommand : public ScriptCommand
{
public:
    PerceptionSense sense;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd582_AddPerception_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd582_AddPerception_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd582_AddPerception_Execute);
    u32 Size() RETAIL(Cmd582_AddPerception_GetSize);
};
CHECK_SIZE(AddPerceptionCommand, 0x4C);

// 583
class SwingAroundCameraCommand : public ScriptCommand
{
public:
    // Where its last run put the instance on the circle (nothing reads them)
    f32 circleX;
    f32 circleY;
    // How far along the camera's z axis, the rate the player's distance moves the height at and the height's limit, the squared
    // distances from the camera nearer and further than which the player moves it
    f32 depth;
    f32 heightRate;
    f32 heightLimit;
    f32 nearDistanceSquared;
    f32 farDistanceSquared;
    // The swing's phase offset (radians) and how fast the phase turns it
    f32 phaseOffset;
    u32 unused2C;
    f32 unused30;
    u32 unused34;
    f32 phaseScale;
    u32 unused3C;
    f32 radius;
    // The phase (moved on each run) and the height (the first command run sets everyone's, every run keeps it)
    f32 phase;
    f32 height;
    u32 unused4C;
    u32 unused50;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd583_CutsceneCameraOp583_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd583_CutsceneCameraOp583_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd583_CutsceneCameraOp583_Dtor);
    u32 Size() RETAIL(Cmd583_CutsceneCameraOp583_GetSize);
};
CHECK_OFFSET(SwingAroundCameraCommand, radius, 0x40);
CHECK_SIZE(SwingAroundCameraCommand, 0x54);

// 584: does nothing
class NoOp584Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd584_DUMMY_584_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd584_DUMMY_584_Execute);
    u32 Size() RETAIL(Cmd584_DUMMY_584_GetSize);
};
CHECK_SIZE(NoOp584Command, 0xC);

// 586: does nothing
class NoOp586Command : public ScriptCommand
{
public:
    HalfwordPair unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd586_DUMMY_586_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd586_DUMMY_586_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd586_DUMMY_586_Execute);
    u32 Size() RETAIL(Cmd586_DUMMY_586_GetSize);
};
CHECK_SIZE(NoOp586Command, 0x10);

// 587
class SetAttacksTakenCommand : public ScriptCommand
{
public:
    // A setting: on, off (0 and 3 leave it)
    enum Setting : u32
    {
        SettingOn = 1,
        SettingOff = 2,
    };

    // Whether the agent's part is hit by the spin, by the body slam (and the tied characters), by walking into it and from below (a
    // plain word, not a tagged value)
    union Settings
    {
        u32 value;
        struct
        {
            u32 spin : 2;
            u32 unused2 : 2;
            u32 slam : 2;
            u32 unused6 : 2;
            u32 unused8 : 2;
            u32 walkInto : 2;
            u32 unused12 : 20;
        };
    };

    Settings settings;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd587_SetObjectFlags587_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd587_SetObjectFlags587_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd587_SetObjectFlags587_Execute);
    u32 Size() RETAIL(Cmd587_SetObjectFlags587_GetSize);
};
CHECK_SIZE(SetAttacksTakenCommand, 0x10);

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
    f32 seconds;

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
    f32 seconds;

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
    // What the framing aims at and measures the distance from: the first place, the second, half the way from the first to the
    // second, and (the distance's) the shot's fixed yaw instead
    enum Aim : u32
    {
        AimFirst = 0,
        AimSecond = 1,
        AimBetween = 2,
        AimFixedYaw = 3,
    };

    // The moves' curve (others leave them as they were)
    enum Curve : u32
    {
        MoveEven = 0,
        MoveSmooth = 1,
    };

    // How it frames (a plain word, not a tagged value): the shot's fixed yaw, the shot (the shares of the object's height and of
    // the view), the angles of the field of view, what it aims at and measures the distance from, the positioner's arc, its ease in
    // and out and the moves' curve, the command's own field of view
    union Framing
    {
        u32 value;
        struct
        {
            u32 fixedYaw : 3;
            u32 shot : 3;
            u32 angles : 3;
            u32 aim : 3;
            u32 distanceFrom : 3;
            u32 arcs : 1;
            u32 easesIn : 1;
            u32 easesOut : 1;
            u32 curve : 3;
            u32 fovGiven : 1;
            u32 unused22 : 1;
            u32 unused23 : 1;
            u32 unused24 : 8;
        };
    };

    Framing framing;
    // Degrees
    f32 pitch;
    f32 extraDistance;
    f32 targetSeconds;
    f32 cameraSeconds;
    f32 extraHeightShare;
    // Degrees
    f32 yaw;
    u32 unused28;
    // Handed to the scripted positioner's unused44
    u32 unused2C;
    // Where along the paths the target and the camera go
    f32 targetAlong;
    f32 cameraAlong;
    // 65536ths of a turn
    s32 fov;

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
    enum Action : u32
    {
        ActionSave = 0,
        ActionRestore = 1,
    };

    // A plain word, not a tagged value
    union Mode
    {
        u32 value;
        struct
        {
            u32 action : 3;
            u32 unused3 : 29;
        };
    };

    Mode mode;

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
    enum Shown : u32
    {
        ShowsFollowCamera = 0,
        ShowsGameRig = 1,
        ShowsCutsceneRig = 2,
    };

    // The camera shown, the follow camera put where the game's rig is, set back to its start, the blend's curve (camerarig.h's
    // CameraCurve)
    union Mode
    {
        u32 value;
        struct
        {
            u32 shown : 3;
            u32 placesFollowCamera : 1;
            u32 resets : 1;
            u32 curve : 3;
            u32 unused8 : 24;
        };
    };

    Mode mode;
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
    // The designators whose instances are the first and the second place, else whose positions are
    union Designators
    {
        u32 value;
        struct
        {
            u32 first : 8;
            u32 second : 8;
            u32 firstPosition : 8;
            u32 secondPosition : 8;
        };
    };

    // The framing mirrored, the frame wanted (game/camerarig.h's GameCameraRig::scriptBits)
    union Flags
    {
        u32 value;
        struct
        {
            u32 mirrored : 1;
            u32 frameWanted : 1;
            u32 unused2 : 30;
        };
    };

    // The agent's waypoints' paths the scripted target and positioner go along (0xFF none)
    union Paths
    {
        u32 value;
        struct
        {
            u32 target : 8;
            u32 camera : 8;
            u32 unused16 : 16;
        };
    };

    Designators designators;
    Flags flags;
    Paths paths;

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
    s32 iconSlot;
    TaggedValue seconds;
    TaggedValue total;

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
    s32 countChange;

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
    enum Mode : u32
    {
        ModeHide = 0,
        ModeShow = 1,
    };

    // The fade hidden or shown, its colour given
    union Flags
    {
        u32 value;
        struct
        {
            u32 mode : 3;
            u32 setsColour : 1;
            u32 unused4 : 28;
        };
    };

    Flags flags;
    f32 duration;
    s32 unused14;
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
    s32 text;
    f32 x;
    f32 y;
    f32 red;
    f32 green;
    f32 blue;
    f32 seconds;

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

// 605: a character's places dismissed
class DismissCharacterCommand : public ScriptCommand
{
public:
    // The game's character number
    s32 character;

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

// 609: does nothing
class NoOp609Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd609_DUMMY_609_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd609_DUMMY_609_Execute);
    u32 Size() RETAIL(Cmd609_DUMMY_609_GetSize);
};
CHECK_SIZE(NoOp609Command, 0xC);

// 610: does nothing
class NoOp610Command : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd610_DUMMY_610_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd610_DUMMY_610_Execute);
    u32 Size() RETAIL(Cmd610_DUMMY_610_GetSize);
};
CHECK_SIZE(NoOp610Command, 0xC);

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

// 612: the characters given their roles again (DisablePlayerControl undone)
class EnablePlayerControlCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd612_GameControllerOp612_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd612_GameControllerOp612_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd612_GameControllerOp612_Execute);
    u32 Size() RETAIL(Cmd612_GameControllerOp612_GetSize);
};
CHECK_SIZE(EnablePlayerControlCommand, 0xC);

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

// 614: the node's motion block not sticky any more (what stuck to it kept when asked)
class StopStickingCommand : public ScriptCommand
{
public:
    SwitchArgument keepsStuck;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd614_SetNode120Flag_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd614_SetNode120Flag_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd614_SetNode120Flag_Execute);
    u32 Size() RETAIL(Cmd614_SetNode120Flag_GetSize);
};
CHECK_SIZE(StopStickingCommand, 0x10);

// 615: the player character's scripts' flag (the PlayerFlag57Clear condition tests it), cleared or set
class SetPlayerScriptFlagCommand : public ScriptCommand
{
public:
    SwitchArgument clears;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd615_SetPlayerFlag57_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd615_SetPlayerFlag57_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd615_SetPlayerFlag57_Execute);
    u32 Size() RETAIL(Cmd615_SetPlayerFlag57_GetSize);
};
CHECK_SIZE(SetPlayerScriptFlagCommand, 0x10);

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
    // The instances hit (but the playable characters, only them, else both), sent the trigger message instead of a contact message
    union Flags
    {
        u32 value;
        struct
        {
            u32 skipsCharacters : 1;
            u32 onlyCharacters : 1;
            u32 sendsMessage : 1;
            u32 unused3 : 29;
        };
    };

    union Message
    {
        u32 value;
        struct
        {
            u32 id : 16;
            u32 unused16 : 16;
        };
    };

    Flags flags;
    // The contact message's kinds of hit
    u32 hitKinds;
    Message message;

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
    f32 seconds;

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
    f32 seconds;

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

// A character a command picks: the game's character number, or the played one
union CharacterChoice
{
    u32 value;
    struct
    {
        u32 character : 8;
        u32 played : 1;
        u32 unused9 : 23;
    };
};
CHECK_SIZE(CharacterChoice, 4);

// 622: a character's object node given the agent's object as its sound's
class CharacterSoundProxyCommand : public ScriptCommand
{
public:
    CharacterChoice choice;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd622_CharacterSoundProxy_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd622_CharacterSoundProxy_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd622_CharacterSoundProxy_Execute);
    u32 Size() RETAIL(Cmd622_CharacterSoundProxy_GetSize);
};
CHECK_SIZE(CharacterSoundProxyCommand, 0x10);

// 623: the follow camera's target the instance
class SetFollowCameraTargetCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd623_CameraNodeSetTarget_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd623_CameraNodeSetTarget_Execute);
    u32 Size() RETAIL(Cmd623_CameraNodeSetTarget_GetSize);
};
CHECK_SIZE(SetFollowCameraTargetCommand, 0xC);

// 624: the flag condition 629 reads set (nothing clears it)
class SetScriptGlobalFlagCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd624_EnableVarPercept629_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd624_EnableVarPercept629_Execute);
    u32 Size() RETAIL(Cmd624_EnableVarPercept629_GetSize);
};
CHECK_SIZE(SetScriptGlobalFlagCommand, 0xC);

// 625
class SwitchCharacterCommand : public ScriptCommand
{
public:
    // The instance whose character is switched to: a designator's (0xFF none), else one the agent's attachments link
    union Target
    {
        u32 value;
        struct
        {
            u32 linked : 4;
            u32 byLinked : 1;
            u32 designator : 8;
            u32 unused13 : 19;
        };
    };

    Target target;
    // The character switched to (GameProgress::NoCharacter: the target's)
    s32 character;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd625_SwitchCharacter_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd625_SwitchCharacter_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd625_SwitchCharacter_Execute);
    u32 Size() RETAIL(Cmd625_SwitchCharacter_GetSize);
};
CHECK_SIZE(SwitchCharacterCommand, 0x14);

// 626: the follow camera's positioner and target take their own cameras
class UseOwnFollowCamerasCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd626_CameraNodeEnableFlags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd626_CameraNodeEnableFlags_Execute);
    u32 Size() RETAIL(Cmd626_CameraNodeEnableFlags_GetSize);
};
CHECK_SIZE(UseOwnFollowCamerasCommand, 0xC);

// 627: the follow camera back to the triggers' cameras
class UseTriggerCamerasCommand : public ScriptCommand
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cmd627_CameraNodeClearFlags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd627_CameraNodeClearFlags_Execute);
    u32 Size() RETAIL(Cmd627_CameraNodeClearFlags_GetSize);
};
CHECK_SIZE(UseTriggerCamerasCommand, 0xC);

// Which rate SetFollowCameraRate sets: the one its place is followed at, the yaw blender's speed (from radians a second)
union FollowCameraRate
{
    enum Kind : u32
    {
        PositionRate = 0,
        YawSpeed = 1,
    };

    u32 value;
    struct
    {
        u32 kind : 3;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(FollowCameraRate, 4);

// 628: a rate of the follow camera's positioner's own camera
class SetFollowCameraRateCommand : public ScriptCommand
{
public:
    FollowCameraRate which;
    f32 rate;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd628_SetCameraNodeValue_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd628_SetCameraNodeValue_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd628_SetCameraNodeValue_Execute);
    u32 Size() RETAIL(Cmd628_SetCameraNodeValue_GetSize);
};
CHECK_SIZE(SetFollowCameraRateCommand, 0x14);

// 629
class SetCameraNodeValuesCommand : public ScriptCommand
{
public:
    enum Value : u32
    {
        ValuePitch = 0,
        ValueDistance = 1,
        ValueYaw = 2,
        ValueFov = 3,
    };

    // Which value it sets (a plain word, not a tagged value)
    union Mode
    {
        u32 value;
        struct
        {
            u32 which : 3;
            u32 unused3 : 29;
        };
    };

    // The value's two ends: the angles in degrees, the field of view in radians
    Mode mode;
    f32 start;
    f32 end;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd629_SetCameraNodeValues_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd629_SetCameraNodeValues_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd629_SetCameraNodeValues_Dtor);
    u32 Size() RETAIL(Cmd629_SetCameraNodeValues_GetSize);
};
CHECK_SIZE(SetCameraNodeValuesCommand, 0x18);

// SwitchBodyFlags' switches (two bits each: 1 on, 2 off, else left) of the physics body's bits 13, 15, 16 and 12 (the tool's
// HitCrates, HitCreatures, HitFurniture and HitPlayer), which nothing reads
union BodyFlagSwitches
{
    u32 value;
    struct
    {
        u32 unused0 : 2;
        u32 unused2 : 2;
        u32 unused4 : 2;
        u32 unused6 : 2;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(BodyFlagSwitches, 4);

// 630: switches of the instance's physics body's flags
class SwitchBodyFlagsCommand : public ScriptCommand
{
public:
    BodyFlagSwitches switches;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd630_SetNode5Flags_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd630_SetNode5Flags_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd630_SetNode5Flags_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd630_SetNode5Flags_ExecuteOn);
    u32 Size() RETAIL(Cmd630_SetNode5Flags_GetSize);
};
CHECK_SIZE(SwitchBodyFlagsCommand, 0x10);

// 631: the played character's Humiliskate pushed
class PushPlayerVehicleCommand : public ScriptCommand
{
public:
    // Along z
    f32 push;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd631_SetPlayerVehicleValue_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd631_SetPlayerVehicleValue_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd631_SetPlayerVehicleValue_Execute);
    u32 Size() RETAIL(Cmd631_SetPlayerVehicleValue_GetSize);
};
CHECK_SIZE(PushPlayerVehicleCommand, 0x10);

// The range of linked objects SetLinkedObjectNearestPlayer looks at: one past the first's index and the end's (Whole: from the
// first, to the last)
union LinkedRange
{
    static constexpr u8 Whole = 0xFF;

    u32 value;
    struct
    {
        u32 first : 8;
        u32 end : 8;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(LinkedRange, 4);

// 632: AgentRef1 the linked object nearest the player that no other agent took
class SetLinkedObjectNearestPlayerCommand : public ScriptCommand
{
public:
    LinkedRange range;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd632_SetLinkedObjectNearestPlayer_Dtor);
    u32 Size() RETAIL(Cmd632_SetLinkedObjectNearestPlayer_GetSize);
};
CHECK_SIZE(SetLinkedObjectNearestPlayerCommand, 0x10);

// 633: the game's pairing, its played character and its second one
class SetPlayerModeCommand : public ScriptCommand
{
public:
    u32 pairing;
    u32 character;
    u32 second;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd633_SetPlayerMode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd633_SetPlayerMode_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd633_SetPlayerMode_Execute);
    u32 Size() RETAIL(Cmd633_SetPlayerMode_GetSize);
};
CHECK_SIZE(SetPlayerModeCommand, 0x18);

// 634: a movie played after a delay
class PlayMovieCommand : public ScriptCommand
{
public:
    TaggedValue movie;
    // Seconds
    f32 delay;

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
    s32 ammo;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd636_AddAmmo_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd636_AddAmmo_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd636_AddAmmo_Execute);
    void ExecuteOn(GameNode* node) RETAIL(Cmd636_AddAmmo_ExecuteOn);
    u32 Size() RETAIL(Cmd636_AddAmmo_GetSize);
};
CHECK_SIZE(AddAmmoCommand, 0x10);

// 637: the focus the linked object in the camera's view nearest the player
class SetFocusToLinkedObjectInViewCommand : public ScriptCommand
{
public:
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_Dtor);
    u32 Size() RETAIL(Cmd637_LinkedObjectNearestPlayerOp637_GetSize);
};
CHECK_SIZE(SetFocusToLinkedObjectInViewCommand, 0xC);

// 638
class EnableBossModeCommand : public ScriptCommand
{
public:
    s32 iconSlot;
    TaggedValue health;
    f32 barLength;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd638_EnableBossMode_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd638_EnableBossMode_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd638_EnableBossMode_Dtor);
    u32 Size() RETAIL(Cmd638_EnableBossMode_GetSize);
};
CHECK_SIZE(EnableBossModeCommand, 0x18);

// 639: the boss bar's health changed by an amount (below 0 a damage)
class DamageBossCommand : public ScriptCommand
{
public:
    s32 healthChange;

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

// 641, 642, 643, 644, 653: the final boss's three weapons (its node's JointAimController), by the mode the script gives: their
// joints hooked on the node's model, the weapons picked turned back to their animation's pose, raised to aim, the target, the
// weapons' scale and turn rate
class FinalBossWeaponsCommand : public ScriptCommand
{
public:
    static constexpr u32 WeaponCount = 3;

    enum Mode : s32
    {
        ModeHookJoints = 0,
        ModeReturn = 1,
        ModeRaise = 2,
        ModeTarget = 3,
        ModeScaleAndRate = 4,
    };

    // The weapons it works on, a bit each
    union Weapons
    {
        u32 value;
        struct
        {
            u32 first : 1;
            u32 second : 1;
            u32 third : 1;
            u32 unused3 : 29;
        };
    };

    DesignatorArgument target;
    s32 mode;
    Weapons weapons;
    f32 scale;
    f32 turnRate;
    // The weapons' joints' IDs
    u8 joints[WeaponCount];
    u8 unused23;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd641_FinalBossInitWeapons_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd641_FinalBossInitWeapons_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd641_FinalBossInitWeapons_Execute);
    u32 Size() RETAIL(Cmd641_FinalBossInitWeapons_GetSize);
};
CHECK_OFFSET(FinalBossWeaponsCommand, joints, 0x20);
CHECK_SIZE(FinalBossWeaponsCommand, 0x24);

// 645
class CreateNodeControllerCommand : public ScriptCommand
{
public:
    // The controller's kind (NodeController's), made only when the node has none with keepsExisting
    union Controller
    {
        u32 value;
        struct
        {
            u32 kind : 8;
            u32 keepsExisting : 1;
            u32 unused9 : 23;
        };
    };

    Controller controller;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd645_CreateNodeController_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd645_CreateNodeController_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd645_CreateNodeController_Execute);
    u32 Size() RETAIL(Cmd645_CreateNodeController_GetSize);
};
CHECK_SIZE(CreateNodeControllerCommand, 0x10);

// An object's model slot in an argument's low byte
union ModelSlotArgument
{
    u32 value;
    struct
    {
        u32 index : 8;
        u32 unused8 : 24;
    };
};
CHECK_SIZE(ModelSlotArgument, 4);

// 646: the vehicle gauge's left icon the model of an object's slot
class SetGaugeIconCommand : public ScriptCommand
{
public:
    ModelSlotArgument slot;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd646_RequestOgiSlot_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd646_RequestOgiSlot_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd646_RequestOgiSlot_Execute);
    u32 Size() RETAIL(Cmd646_RequestOgiSlot_GetSize);
};
CHECK_SIZE(SetGaugeIconCommand, 0x10);

// 647
class RaiseStoryAreaCommand : public ScriptCommand
{
public:
    s32 area;
    TaggedValue taggedArea;

    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd647_SetGlobalProgression2_Execute);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd647_SetGlobalProgression2_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd647_SetGlobalProgression2_Dtor);
    u32 Size() RETAIL(Cmd647_SetGlobalProgression2_GetSize);
};
CHECK_SIZE(RaiseStoryAreaCommand, 0x14);

// 648: the node's counted value (at most 1; a value where there was none counts the instance)
class SetCountedValueCommand : public ScriptCommand
{
public:
    f32 value;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd648_SetNodeValue174_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd648_SetNodeValue174_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd648_SetNodeValue174_Execute);
    u32 Size() RETAIL(Cmd648_SetNodeValue174_GetSize);
};
CHECK_SIZE(SetCountedValueCommand, 0x10);

// 649: the node's counted value cleared (the instance no longer counted)
class ClearCountedValueCommand : public ScriptCommand
{
public:
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd649_ClearNodeValue174_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd649_ClearNodeValue174_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd649_ClearNodeValue174_Execute);
    u32 Size() RETAIL(Cmd649_ClearNodeValue174_GetSize);
};
CHECK_SIZE(ClearCountedValueCommand, 0xC);

// 650: the vehicle a character rides held
class HoldVehicleCommand : public ScriptCommand
{
public:
    // The game's character number
    s32 character;

    void Destroy(u32 destroyFlags) RETAIL(Cmd650_SetCharacterFlag2_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd650_SetCharacterFlag2_Execute);
    u32 Size() RETAIL(Cmd650_SetCharacterFlag2_GetSize);
};
CHECK_SIZE(HoldVehicleCommand, 0x10);

// 651: the vehicle a character rides no longer held
class ReleaseVehicleCommand : public ScriptCommand
{
public:
    // The game's character number
    s32 character;

    void Destroy(u32 destroyFlags) RETAIL(Cmd651_ClearCharacterFlag2_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd651_ClearCharacterFlag2_Execute);
    u32 Size() RETAIL(Cmd651_ClearCharacterFlag2_GetSize);
};
CHECK_SIZE(ReleaseVehicleCommand, 0x10);

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
    static constexpr u32 MostIds = 12;

    union Count
    {
        u32 value;
        struct
        {
            u32 ids : 4;
            u32 unused4 : 28;
        };
    };

    // The mask controller's particle systems' IDs (3 used)
    u16 ids[MostIds];
    Count count;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd655_SetMaskControllerIds_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd655_SetMaskControllerIds_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd655_SetMaskControllerIds_Execute);
    u32 Size() RETAIL(Cmd655_SetMaskControllerIds_GetSize);
};
CHECK_OFFSET(SetMaskControllerIdsCommand, count, 0x24);
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
    f32 red;
    f32 green;
    f32 blue;
    f32 seconds;

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
    TaggedValue offsetX;
    TaggedValue offsetY;
    TaggedValue offsetZ;
    TaggedValue pull;
    TaggedValue turnRate;
    // The controller's unused28 (nothing reads it)
    TaggedValue unusedValue;
    TaggedValue drop;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd658_SetSplineControllerValues_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd658_SetSplineControllerValues_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd658_SetSplineControllerValues_Execute);
    u32 Size() RETAIL(Cmd658_SetSplineControllerValues_GetSize);
};
CHECK_SIZE(SetSplineControllerValuesCommand, 0x28);

// 659
class MakeCharactersIdleCommand : public ScriptCommand
{
public:
    // The characters whose idle behaviour starts: every one the progress has, Crash, Cortex, the Mecha-Bandicoot (none: the
    // agent's own)
    union Characters
    {
        u32 value;
        struct
        {
            u32 every : 1;
            u32 crash : 1;
            u32 cortex : 1;
            u32 mecha : 1;
            u32 unused4 : 28;
        };
    };

    Characters characters;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd659_TriggerCharacterEvent12_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd659_TriggerCharacterEvent12_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd659_TriggerCharacterEvent12_Execute);
    // The idle behaviour (characters.h's EventIdle) started on an instance's character (none without its character node)
    void MakeIdle(struct InstanceContext* instance, BehaviourLevel* level) RETAIL(FUN_00122c28);
    u32 Size() RETAIL(Cmd659_TriggerCharacterEvent12_GetSize);
};
CHECK_SIZE(MakeCharactersIdleCommand, 0x10);

// 660: the character (the instance's, else the player's) no longer dead
class ClearCharacterDeadCommand : public ScriptCommand
{
public:
    u32 unused1;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd660_ClearPlayerFlag14_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd660_ClearPlayerFlag14_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd660_ClearPlayerFlag14_Execute);
    u32 Size() RETAIL(Cmd660_ClearPlayerFlag14_GetSize);
};
CHECK_SIZE(ClearCharacterDeadCommand, 0x10);

// 661
class SetSkateControllerIdsCommand : public ScriptCommand
{
public:
    static constexpr u32 MostIds = 12;
    static constexpr u32 MostSounds = 32;

    // How many IDs and sounds it gives, and whether they're added when the controller has some already
    union Counts
    {
        u32 value;
        struct
        {
            u32 ids : 4;
            u32 sounds : 5;
            u32 addsAgain : 1;
            u32 unused10 : 22;
        };
    };

    // The trails' particle systems' IDs, and the sounds' slots of the node's object
    u16 ids[MostIds];
    u16 soundSlots[MostSounds];
    Counts counts;

    void ParseTokens(const ScriptTokenList* tokens) RETAIL(Cmd661_SetSkateControllerIds_ParseTokens);
    void Destroy(u32 destroyFlags) RETAIL(Cmd661_SetSkateControllerIds_Dtor);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(Cmd661_SetSkateControllerIds_Execute);
    u32 Size() RETAIL(Cmd661_SetSkateControllerIds_GetSize);
};
CHECK_OFFSET(SetSkateControllerIdsCommand, soundSlots, 0x24);
CHECK_OFFSET(SetSkateControllerIdsCommand, counts, 0x64);
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
    extern const GccVTableEntry g_NoOpNowMoveBackwardsCommandVTable[] RETAIL(vt_Cmd15_NowMoveBackwards);
    extern const GccVTableEntry g_NowStrafeLeftCommandVTable[] RETAIL(vt_Cmd16_NowStrafeLeft);
    extern const GccVTableEntry g_NowStrafeRightCommandVTable[] RETAIL(vt_Cmd17_NowStrafeRight);
    extern const GccVTableEntry g_NowTurnLeftCommandVTable[] RETAIL(vt_Cmd18_NowTurnLeft);
    extern const GccVTableEntry g_LinkTargetCommandVTable[] RETAIL(vt_Cmd23_NowRotateJoint);
    extern const GccVTableEntry g_StoreCurrentSpaceCommandVTable[] RETAIL(vt_Cmd27_StoreCurrentSpace);
    extern const GccVTableEntry g_SetFocusToKeyCommandVTable[] RETAIL(vt_Cmd28_SetFocusToKey);
    extern const GccVTableEntry g_RotationWarpCommandVTable[] RETAIL(vt_Cmd29_RotationWarp);
    extern const GccVTableEntry g_SetRestartableCommandVTable[] RETAIL(vt_Cmd31_ClearThreats);
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
    extern const GccVTableEntry g_SetReverbCommandVTable[] RETAIL(vt_Cmd86_SetSound);
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
    extern const GccVTableEntry g_SnapRotationCommandVTable[] RETAIL(vt_Cmd113_Unknown);
    extern const GccVTableEntry g_StopHeadTrackingCommandVTable[] RETAIL(vt_Cmd114_Unknown);
    extern const GccVTableEntry g_StartHeadTrackingCommandVTable[] RETAIL(vt_Cmd115_Unknown);
    extern const GccVTableEntry g_DestroyHeadTrackingCommandVTable[] RETAIL(vt_Cmd116_Unknown);
    extern const GccVTableEntry g_MakeNoiseCommandVTable[] RETAIL(vt_Cmd117_Unknown);
    extern const GccVTableEntry g_SetHeadTrackingTargetCommandVTable[] RETAIL(vt_Cmd118_Unknown);
    extern const GccVTableEntry g_ClearKnockCountdownCommandVTable[] RETAIL(vt_Cmd119_Unknown);
    extern const GccVTableEntry g_PhysicsResetVelocityCommandVTable[] RETAIL(vt_Cmd120_PhysicsResetVelocity);
    extern const GccVTableEntry g_SetFocusPositionBesidePlayerCommandVTable[] RETAIL(vt_Cmd121_Unknown);
    extern const GccVTableEntry g_CopyDesignatorCommandVTable[] RETAIL(vt_Cmd122_Unknown);
    extern const GccVTableEntry g_LinkToNearestPointCommandVTable[] RETAIL(vt_Cmd123_Unknown);
    extern const GccVTableEntry g_RunScriptSlotCommandVTable[] RETAIL(vt_Cmd124_RunScriptSlot);
    extern const GccVTableEntry g_OffsetFocusPositionCommandVTable[] RETAIL(vt_Cmd125_Unknown);
    extern const GccVTableEntry g_NextLinkedObjectCommandVTable[] RETAIL(vt_Cmd126_Unknown);
    extern const GccVTableEntry g_PhysicsSetGravityCommandVTable[] RETAIL(vt_Cmd127_Unknown);
    extern const GccVTableEntry g_PhysicsBodyResetCommandVTable[] RETAIL(vt_Cmd128_Unknown);
    extern const GccVTableEntry g_PhysicsBodyActivateCommandVTable[] RETAIL(vt_Cmd129_Unknown);
    extern const GccVTableEntry g_SetMagnetCommandVTable[] RETAIL(vt_Cmd130_Unknown);
    extern const GccVTableEntry g_MagnetPullToFocusCommandVTable[] RETAIL(vt_Cmd131_Unknown);
    extern const GccVTableEntry g_SetLinkedObjectIndexCommandVTable[] RETAIL(vt_Cmd132_Unknown);
    extern const GccVTableEntry g_ClearObjectContextTargetCommandVTable[] RETAIL(vt_Cmd133_Unknown);
    extern const GccVTableEntry g_SetFocusToOriginatorCommandVTable[] RETAIL(vt_Cmd134_Unknown);
    extern const GccVTableEntry g_DestroyPerceptionsCommandVTable[] RETAIL(vt_Cmd135_Unknown);
    extern const GccVTableEntry g_SetPerceptionWeightCommandVTable[] RETAIL(vt_Cmd136_Unknown);
    extern const GccVTableEntry g_AddPerceptionWeightCommandVTable[] RETAIL(vt_Cmd137_Unknown);
    extern const GccVTableEntry g_PushFromPerceptionCommandVTable[] RETAIL(vt_Cmd138_Unknown);
    extern const GccVTableEntry g_SetPresenceCommandVTable[] RETAIL(vt_Cmd139_Unknown);
    extern const GccVTableEntry g_AddPresenceCommandVTable[] RETAIL(vt_Cmd140_Unknown);
    extern const GccVTableEntry g_TurnSenseOffCommandVTable[] RETAIL(vt_Cmd141_Unknown);
    extern const GccVTableEntry g_TurnSenseOnCommandVTable[] RETAIL(vt_Cmd142_Unknown);
    extern const GccVTableEntry g_DisableAllPerceptionsCommandVTable[] RETAIL(vt_Cmd143_Unknown);
    extern const GccVTableEntry g_EnableAllPerceptionsCommandVTable[] RETAIL(vt_Cmd144_Unknown);
    extern const GccVTableEntry g_SetParentExecutionValueCommandVTable[] RETAIL(vt_Cmd145_Unknown);
    extern const GccVTableEntry g_SetFocusToLinkedObjectCommandVTable[] RETAIL(vt_Cmd146_Unknown);
    extern const GccVTableEntry g_PreviousKeyCommandVTable[] RETAIL(vt_Cmd147_Unknown);
    extern const GccVTableEntry g_SetCycleAmplitudesCommandVTable[] RETAIL(vt_Cmd148_Unknown);
    extern const GccVTableEntry g_RotateWithLinkedCommandVTable[] RETAIL(vt_Cmd149_Unknown);
    extern const GccVTableEntry g_StrafeTowardsTargetCommandVTable[] RETAIL(vt_Cmd150_Unknown);
    extern const GccVTableEntry g_PreviousRouteNodeCommandVTable[] RETAIL(vt_Cmd151_Unknown);
    extern const GccVTableEntry g_AddWobblePhaseCommandVTable[] RETAIL(vt_Cmd152_Unknown);
    extern const GccVTableEntry g_MoveTowardsDesignatorCommandVTable[] RETAIL(vt_Cmd153_Unknown);
    extern const GccVTableEntry g_SetNoiseMessageCommandVTable[] RETAIL(vt_Cmd156_Unknown);
    extern const GccVTableEntry g_UnlinkTargetCommandVTable[] RETAIL(vt_Cmd157_Unknown);
    extern const GccVTableEntry g_AttachMotionBlockCommandVTable[] RETAIL(vt_Cmd158_Unknown);
    extern const GccVTableEntry g_ClearTouchMessageCommandVTable[] RETAIL(vt_Cmd159_Unknown);
    extern const GccVTableEntry g_StopTouchMessagesCommandVTable[] RETAIL(vt_Cmd160_Unknown);
    extern const GccVTableEntry g_SendTouchMessagesCommandVTable[] RETAIL(vt_Cmd161_Unknown);
    extern const GccVTableEntry g_AddToFocusCounterCommandVTable[] RETAIL(vt_Cmd162_Unknown);
    extern const GccVTableEntry g_UnlinkFromTargetCommandVTable[] RETAIL(vt_Cmd163_Unknown);
    extern const GccVTableEntry g_AddToLinkedCounterCommandVTable[] RETAIL(vt_Cmd164_Unknown);
    extern const GccVTableEntry g_ForceVolumeControllerCommandVTable[] RETAIL(vt_Cmd165_ForceVolumeController);
    extern const GccVTableEntry g_NotifyInstancesWithinCommandVTable[] RETAIL(vt_Cmd166_Unknown);
    extern const GccVTableEntry g_SetSurfaceCommandVTable[] RETAIL(vt_Cmd167_SetSurface);
    extern const GccVTableEntry g_PushInstancesAwayCommandVTable[] RETAIL(vt_Cmd168_Unknown);
    extern const GccVTableEntry g_SetFocusPositionAlongCommandVTable[] RETAIL(vt_Cmd169_Unknown);
    extern const GccVTableEntry g_SetFocusCounterCommandVTable[] RETAIL(vt_Cmd170_Unknown);
    extern const GccVTableEntry g_RunSlotBehaviourOnLinkedCommandVTable[] RETAIL(vt_Cmd171_Unknown);
    extern const GccVTableEntry g_StopTargetBehaviourCommandVTable[] RETAIL(vt_Cmd172_Unknown);
    extern const GccVTableEntry g_SetPathIndexCommandVTable[] RETAIL(vt_Cmd173_Unknown);
    extern const GccVTableEntry g_SetFocusToOwnerCommandVTable[] RETAIL(vt_Cmd174_Unknown);
    extern const GccVTableEntry g_SetAgentRef1ToOwnerCommandVTable[] RETAIL(vt_Cmd175_Unknown);
    extern const GccVTableEntry g_FadeOutMusicSlotCommandVTable[] RETAIL(vt_Cmd176_Unknown);
    extern const GccVTableEntry g_WarpAgentCommandVTable[] RETAIL(vt_Cmd177_Unknown);
    extern const GccVTableEntry g_RotateAgentCommandVTable[] RETAIL(vt_Cmd178_Unknown);
    extern const GccVTableEntry g_QueueObjectVideoCommandVTable[] RETAIL(vt_Cmd180_Unknown);
    extern const GccVTableEntry g_StartObjectVideoCommandVTable[] RETAIL(vt_Cmd181_Unknown);
    extern const GccVTableEntry g_CancelVideoCommandVTable[] RETAIL(vt_Cmd182_Unknown);
    extern const GccVTableEntry g_SetTargetOwnerToSelfCommandVTable[] RETAIL(vt_Cmd183_Unknown);
    extern const GccVTableEntry g_RestoreOwnObjectCommandVTable[] RETAIL(vt_Cmd184_Unknown);
    extern const GccVTableEntry g_QueueVideoCommandVTable[] RETAIL(vt_Cmd185_Unknown);
    extern const GccVTableEntry g_StartQueuedVideoCommandVTable[] RETAIL(vt_Cmd186_Unknown);
    extern const GccVTableEntry g_SetShadowCommandVTable[] RETAIL(vt_Cmd187_SetShadow);
    extern const GccVTableEntry g_SetShadowCircleCommandVTable[] RETAIL(vt_Cmd188_SetShadowCircle);
    extern const GccVTableEntry g_SetShadowMeshCommandVTable[] RETAIL(vt_Cmd189_SetShadowMesh);
    extern const GccVTableEntry g_SetShadowRectangleCommandVTable[] RETAIL(vt_Cmd190_SetShadowRectangle);
    extern const GccVTableEntry g_SetShadowSlotCommandVTable[] RETAIL(vt_Cmd191_ShadowToggle);
    extern const GccVTableEntry g_ClearShadowSlotCommandVTable[] RETAIL(vt_Cmd192_Unknown);
    extern const GccVTableEntry g_LaunchAtTargetCommandVTable[] RETAIL(vt_Cmd193_Unknown);
    extern const GccVTableEntry g_SetContactSoundsCommandVTable[] RETAIL(vt_Cmd194_Unknown);
    extern const GccVTableEntry g_StopVideoCommandVTable[] RETAIL(vt_Cmd195_Unknown);
    extern const GccVTableEntry g_StopSoundCommandVTable[] RETAIL(vt_Cmd196_Unknown);
    extern const GccVTableEntry g_NoOp197CommandVTable[] RETAIL(vt_Cmd197_DUMMY_197);
    extern const GccVTableEntry g_SetCollisionBoxSizeCommandVTable[] RETAIL(vt_Cmd198_Unknown);
    extern const GccVTableEntry g_NextLinkedObjectInListCommandVTable[] RETAIL(vt_Cmd199_Unknown);
    extern const GccVTableEntry g_PickLinkedObjectNearPlayerCommandVTable[] RETAIL(vt_Cmd200_Unknown);
    extern const GccVTableEntry g_TriggerInstanceAtOwnBoxCommandVTable[] RETAIL(vt_Cmd202_Unknown);
    extern const GccVTableEntry g_SetBodyMassCommandVTable[] RETAIL(vt_Cmd203_Unknown);
    extern const GccVTableEntry g_SetFocusPositionOffsetCommandVTable[] RETAIL(vt_Cmd204_Unknown);
    extern const GccVTableEntry g_SetStoredPositionAtAngleCommandVTable[] RETAIL(vt_Cmd205_Unknown);
    extern const GccVTableEntry g_SaveScriptStateCommandVTable[] RETAIL(vt_Cmd206_Unknown);
    extern const GccVTableEntry g_ClearSavedScriptStateCommandVTable[] RETAIL(vt_Cmd207_Unknown);
    extern const GccVTableEntry g_SetRankCommandVTable[] RETAIL(vt_Cmd208_Unknown);
    extern const GccVTableEntry g_SetTriggerRankCommandVTable[] RETAIL(vt_Cmd209_Unknown);
    extern const GccVTableEntry g_TriggerInstancesByRankCommandVTable[] RETAIL(vt_Cmd210_Unknown);
    extern const GccVTableEntry g_MarkTimeCommandVTable[] RETAIL(vt_Cmd211_Unknown);
    extern const GccVTableEntry g_ClearMarkedTimeCommandVTable[] RETAIL(vt_Cmd212_Unknown);
    extern const GccVTableEntry g_RestartRouteCommandVTable[] RETAIL(vt_Cmd213_Unknown);
    extern const GccVTableEntry g_ControllerRumbleCommandVTable[] RETAIL(vt_Cmd214_ControllerRumble);
    extern const GccVTableEntry g_SetSoundParamsCommandVTable[] RETAIL(vt_Cmd215_Unknown);
    extern const GccVTableEntry g_CreateCrateContentsCommandVTable[] RETAIL(CreateCrateContents_VTable);
    extern const GccVTableEntry g_PickUpWumpaCommandVTable[] RETAIL(vt_Cmd513_CA_PickUpWumpa);
    extern const GccVTableEntry g_CreateDamageCommandVTable[] RETAIL(vt_Cmd514_CreateDamage);
    extern const GccVTableEntry g_SetAgentCommandVTable[] RETAIL(vt_Cmd515_SetAgent);
    extern const GccVTableEntry g_SetPlayerRespawnPositionCommandVTable[] RETAIL(VT_SetPlayerRespawnPosition);
    extern const GccVTableEntry g_RestartFromCheckpointCommandVTable[] RETAIL(VT_ResetGame);
    extern const GccVTableEntry g_SetCrateCommandVTable[] RETAIL(vt_Cmd518_SetCrate);
    extern const GccVTableEntry g_TriggerBalancedCrateFallingCommandVTable[] RETAIL(vt_Cmd519_TriggerBalancedCrateFalling);
    extern const GccVTableEntry g_PickUpHealthCommandVTable[] RETAIL(vt_Cmd520_CA_PickUpHealth);
    extern const GccVTableEntry g_SetPlayerInputCommandVTable[] RETAIL(vt_Cmd521_SetPlayerInput);
    extern const GccVTableEntry g_TriggerAllNitroCratesCommandVTable[] RETAIL(vt_Cmd522_TriggerAllNitroCrates);
    extern const GccVTableEntry g_ApplyVelocityCommandVTable[] RETAIL(vt_Cmd523_ApplyVelocity);
    extern const GccVTableEntry g_SetKeyNearestPlayerCommandVTable[] RETAIL(vt_Cmd524_SetKeyNearestPlayer);
    extern const GccVTableEntry g_RaycastFocusPositionCommandVTable[] RETAIL(vt_Cmd525_RaycastFocusPosition);
    extern const GccVTableEntry g_ApplyVelocityToSelfCommandVTable[] RETAIL(vt_Cmd526_ApplyVelocityToSelf);
    extern const GccVTableEntry g_SetChiChiGrassCommandVTable[] RETAIL(vt_Cmd527_SetChiChiGrass);
    extern const GccVTableEntry g_ReduceHitPointsCommandVTable[] RETAIL(vt_Cmd528_ReduceHitPoints);
    extern const GccVTableEntry g_SetHitPointsCommandVTable[] RETAIL(vt_Cmd529_SetHitPoints);
    extern const GccVTableEntry g_NoOpSetRayTestsCommandVTable[] RETAIL(vt_Cmd530_DUMMY_SetRayTests);
    extern const GccVTableEntry g_NoOpNowGoForwardCollidableCommandVTable[] RETAIL(vt_Cmd532_DUMMY_NowGoForwardCollidable);
    extern const GccVTableEntry g_NowGoBackCollidableCommandVTable[] RETAIL(vt_Cmd533_NowGoBackCollidable);
    extern const GccVTableEntry g_SetPlayAreaCommandVTable[] RETAIL(vt_Cmd534_SetGlobalProgression);
    extern const GccVTableEntry g_AddCrystalCommandVTable[] RETAIL(vt_Cmd535_AddCrystal);
    extern const GccVTableEntry g_NoOp536CommandVTable[] RETAIL(vt_Cmd536_DUMMY_536);
    extern const GccVTableEntry g_AddGemCommandVTable[] RETAIL(vt_Cmd537_AddGem);
    extern const GccVTableEntry g_NoOp538CommandVTable[] RETAIL(vt_Cmd538_DUMMY_538);
    extern const GccVTableEntry g_SetCustomPickupCommandVTable[] RETAIL(vt_Cmd539_CA_SetPickup);
    extern const GccVTableEntry g_SetCustomProjectileCommandVTable[] RETAIL(vt_Cmd540_CA_SetProjectile);
    extern const GccVTableEntry g_ShootCommandVTable[] RETAIL(vt_Cmd548_Shoot);
    extern const GccVTableEntry g_GetShortRouteCommandVTable[] RETAIL(vt_Cmd549_GetShortRoute);
    extern const GccVTableEntry g_NoOpFuelPayGateCommandVTable[] RETAIL(vt_Cmd550_DUMMY_FuelPayGate);
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
    extern const GccVTableEntry g_NoOp568CommandVTable[] RETAIL(vt_Cmd568_Unknown);
    extern const GccVTableEntry g_ExitVehicleModeCommandVTable[] RETAIL(vt_Cmd569_ExitVehicleMode);
    extern const GccVTableEntry g_SetVehicleRollerbrawlCommandVTable[] RETAIL(vt_Cmd570_SetVehicleRollerbrawl);
    extern const GccVTableEntry g_SetVehicleHoverboardCommandVTable[] RETAIL(vt_Cmd571_SetVehicleHoverboard);
    extern const GccVTableEntry g_SetMotionCommandVTable[] RETAIL(vt_Cmd572_Unknown);
    extern const GccVTableEntry g_SetNearestPointFlagsCommandVTable[] RETAIL(vt_Cmd573_Unknown);
    extern const GccVTableEntry g_CreateHeadTrackingCommandVTable[] RETAIL(vt_Cmd574_Unknown);
    extern const GccVTableEntry g_SetFocusPositionToNearestPointCommandVTable[] RETAIL(vt_Cmd575_Unknown);
    extern const GccVTableEntry g_SetFocusToGameActorCommandVTable[] RETAIL(vt_Cmd576_Unknown);
    extern const GccVTableEntry g_BecomeStickyCommandVTable[] RETAIL(vt_Cmd577_BecomeSticky);
    extern const GccVTableEntry g_CountPlayerCirclingCommandVTable[] RETAIL(vt_Cmd578_Unknown);
    extern const GccVTableEntry g_CountPlayerApproachCommandVTable[] RETAIL(vt_Cmd579_Unknown);
    extern const GccVTableEntry g_ApplyVelocityToHeldBodyCommandVTable[] RETAIL(vt_Cmd580_Unknown);
    extern const GccVTableEntry g_BecomeNormalCommandVTable[] RETAIL(vt_Cmd581_BecomeNormal);
    extern const GccVTableEntry g_AddPerceptionCommandVTable[] RETAIL(vt_Cmd582_Unknown);
    extern const GccVTableEntry g_SwingAroundCameraCommandVTable[] RETAIL(vt_Cmd583_Unknown);
    extern const GccVTableEntry g_NoOp584CommandVTable[] RETAIL(vt_Cmd584_DUMMY_584);
    extern const GccVTableEntry g_NoOp586CommandVTable[] RETAIL(vt_Cmd586_DUMMY_586);
    extern const GccVTableEntry g_SetAttacksTakenCommandVTable[] RETAIL(vt_Cmd587_Unknown);
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
    extern const GccVTableEntry g_NoOp609CommandVTable[] RETAIL(vt_Cmd609_DUMMY_609);
    extern const GccVTableEntry g_NoOp610CommandVTable[] RETAIL(vt_Cmd610_DUMMY_610);
    extern const GccVTableEntry g_SetCharacterHomeChunkCommandVTable[] RETAIL(vt_Cmd611_Unknown);
    extern const GccVTableEntry g_EnablePlayerControlCommandVTable[] RETAIL(vt_Cmd612_Unknown);
    extern const GccVTableEntry g_DisablePlayerControlCommandVTable[] RETAIL(vt_Cmd613_DisablePlayerControl);
    extern const GccVTableEntry g_StopStickingCommandVTable[] RETAIL(vt_Cmd614_Unknown);
    extern const GccVTableEntry g_SetPlayerScriptFlagCommandVTable[] RETAIL(vt_Cmd615_Unknown);
    extern const GccVTableEntry g_PlaceCharacterInChunkCommandVTable[] RETAIL(vt_Cmd616_Unknown);
    extern const GccVTableEntry g_HitInstancesInBoxesCommandVTable[] RETAIL(vt_Cmd617_Unknown);
    extern const GccVTableEntry g_ForceGameOverCommandVTable[] RETAIL(vt_Cmd618_ForceGameOver);
    extern const GccVTableEntry g_ShowBottomTextCommandVTable[] RETAIL(vt_Cmd619_ShowBottomText);
    extern const GccVTableEntry g_HideBottomTextCommandVTable[] RETAIL(vt_Cmd620_HideBottomText);
    extern const GccVTableEntry g_SetFocusToCameraTargetCommandVTable[] RETAIL(vt_Cmd621_Unknown);
    extern const GccVTableEntry g_CharacterSoundProxyCommandVTable[] RETAIL(D_002EEE88);
    extern const GccVTableEntry g_SetFollowCameraTargetCommandVTable[] RETAIL(vt_Cmd623_Unknown);
    extern const GccVTableEntry g_SetScriptGlobalFlagCommandVTable[] RETAIL(vt_Cmd624_EnableVarPercept629);
    extern const GccVTableEntry g_SwitchCharacterCommandVTable[] RETAIL(vt_Cmd625_SwitchCharacter);
    extern const GccVTableEntry g_UseOwnFollowCamerasCommandVTable[] RETAIL(vt_Cmd626_Unknown);
    extern const GccVTableEntry g_UseTriggerCamerasCommandVTable[] RETAIL(vt_Cmd627_Unknown);
    extern const GccVTableEntry g_SetFollowCameraRateCommandVTable[] RETAIL(vt_Cmd628_Unknown);
    extern const GccVTableEntry g_SetCameraNodeValuesCommandVTable[] RETAIL(vt_Cmd629_Unknown);
    extern const GccVTableEntry g_SwitchBodyFlagsCommandVTable[] RETAIL(vt_Cmd630_Unknown);
    extern const GccVTableEntry g_PushPlayerVehicleCommandVTable[] RETAIL(vt_Cmd631_Unknown);
    extern const GccVTableEntry g_SetLinkedObjectNearestPlayerCommandVTable[] RETAIL(vt_Cmd632_Unknown);
    extern const GccVTableEntry g_SetPlayerModeCommandVTable[] RETAIL(vt_Cmd633_SetPlayerMode);
    extern const GccVTableEntry g_PlayMovieCommandVTable[] RETAIL(vt_Cmd634_PlayMovie);
    extern const GccVTableEntry g_AddAmmoCommandVTable[] RETAIL(vt_Cmd636_AddAmmo);
    extern const GccVTableEntry g_SetFocusToLinkedObjectInViewCommandVTable[] RETAIL(vt_Cmd637_Unknown);
    extern const GccVTableEntry g_EnableBossModeCommandVTable[] RETAIL(vt_Cmd638_EnableBossMode);
    extern const GccVTableEntry g_DamageBossCommandVTable[] RETAIL(vt_Cmd639_DamageBoss);
    extern const GccVTableEntry g_ExitBossModeCommandVTable[] RETAIL(vt_Cmd640_ExitBossMode);
    extern const GccVTableEntry g_FinalBossWeaponsCommandVTable[] RETAIL(vt_Cmd641_Unknown);
    extern const GccVTableEntry g_CreateNodeControllerCommandVTable[] RETAIL(vt_Cmd645_Unknown);
    extern const GccVTableEntry g_SetGaugeIconCommandVTable[] RETAIL(vt_Cmd646_Unknown);
    extern const GccVTableEntry g_RaiseStoryAreaCommandVTable[] RETAIL(vt_Cmd647_SetGlobalProgression2);
    extern const GccVTableEntry g_SetCountedValueCommandVTable[] RETAIL(vt_Cmd648_Unknown);
    extern const GccVTableEntry g_ClearCountedValueCommandVTable[] RETAIL(vt_Cmd649_Unknown);
    extern const GccVTableEntry g_HoldVehicleCommandVTable[] RETAIL(vt_Cmd650_Unknown);
    extern const GccVTableEntry g_ReleaseVehicleCommandVTable[] RETAIL(vt_Cmd651_Unknown);
    extern const GccVTableEntry g_CameraTopdownModeCommandVTable[] RETAIL(vt_Cmd652_CameraTopdownMode);
    extern const GccVTableEntry g_PlayCreditsCommandVTable[] RETAIL(vt_Cmd654_PlayCredits);
    extern const GccVTableEntry g_SetMaskControllerIdsCommandVTable[] RETAIL(vt_Cmd655_Unknown);
    extern const GccVTableEntry g_ResetMaskControllerCommandVTable[] RETAIL(vt_Cmd656_Unknown);
    extern const GccVTableEntry g_DisplayBottomTextInstanceCommandVTable[] RETAIL(vt_Cmd657_DisplayBottomTextInstance);
    extern const GccVTableEntry g_SetSplineControllerValuesCommandVTable[] RETAIL(vt_Cmd658_Unknown);
    extern const GccVTableEntry g_MakeCharactersIdleCommandVTable[] RETAIL(vt_Cmd659_Unknown);
    extern const GccVTableEntry g_ClearCharacterDeadCommandVTable[] RETAIL(vt_Cmd660_Unknown);
    extern const GccVTableEntry g_SetSkateControllerIdsCommandVTable[] RETAIL(vt_Cmd661_Unknown);
    extern const GccVTableEntry g_ResetCameraCommandVTable[] RETAIL(vt_Cmd662_ResetCamera);
}

