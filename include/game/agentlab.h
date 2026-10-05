#pragma once

#include "abi.h"
#include "common.h"
#include "game/string.h"
#include "gcc2.h"

class Stream;
struct ResourceTable;

// The behaviour scripts (AgentLab) as the game keeps them: script resources of two kinds, starters (the assigners that make the
// game run a graph on its own) and graphs (states of bodies of a condition and commands, and a jump to another state)

// The class IDs of the AgentLab items (their item types, what the game context's AgentLab items' builder makes)
enum AgentLabClassId : u32
{
    GraphStateClassId = 0x1800,
    GraphDataClassId = 0x1801,
    ScriptClassId = 0x1802,
    StarterClassId = 0x1803,
    GraphClassId = 0x1804,
    StateBodyClassId = 0x1805,
    CommandClassId = 0x1806,
    ConditionClassId = 0x1807,
    ControlPacketClassId = 0x1808,
    CallConventionClassId = 0x1809,
    // Two words of -1, two words of 0 and 16 bytes left as the heap had them, which nothing reads
    UnusedNonesClassId = 0x180C,
    UnusedZerosClassId = 0x180E,
    AiPositionClassId = 0x180F,
    AiPathClassId = 0x1810,
    UnusedBlockClassId = 0x1811,
};

// A script's ID of none (a starter's, a graph's, a state's child behaviour)
constexpr u16 NoScriptId = 0xFFFF;

// A script resource's bits: its script's ID (NoScriptId none), its priority (50 when made) and whether its references are
// resolved
union ScriptResourceBits
{
    u32 value;
    struct
    {
        u32 id : 16;
        u32 priority : 8;
        u32 resolved : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(ScriptResourceBits, 4);

// A script resource's base (retail's GenericScriptInfo, 0x1C bytes; vtable 0x18 bytes in: 1 the destructor, 2 its script's ID, 3
// its references resolved, 4 the read, 5 the write, 6 its section's item type, 7 whether it's a starter): its resource header, its
// resource ID, its bits and a name
struct ScriptResource
{
    enum Slot : u32
    {
        DestroySlot = 1,
        IdSlot = 2,
        ResolveSlot = 3,
    };

    u32 header;
    s32 resourceId;
    ScriptResourceBits bits;
    String name;
    const GccVTableEntry* vtable;

    static ScriptResource* Construct(ScriptResource* script) RETAIL(InitGenericScriptInfo);
    void Destroy(u32 destroyFlags) RETAIL(DestroyGenericScriptInfo_);
    u16 Id() RETAIL(GetScriptId);
    void Resolve() RETAIL(SetFlagField);
    // Its bits read (not resolved yet), and written (every kind's)
    void Read(Stream* stream) RETAIL(FUN_00208d70);
    void Write(Stream* stream) RETAIL(FUN_00208dc0);
    u32 ItemType() RETAIL(FUN_00208878);
};
CHECK_OFFSET(ScriptResource, vtable, 0x18);
CHECK_SIZE(ScriptResource, 0x1C);

// Whose instance a starter's assigner runs its graph on (the AgentLab tool's names): the agent's own, one its links node keeps
// (by the argument), a global agent's (the instance registered under the argument), the player's, the originator's; none for
// the tool's other types
enum Assignee : u32
{
    AssignMe = 0,
    AssignLinkedObject = 2,
    AssignGlobalAgent = 3,
    AssignHumanPlayer = 4,
    AssignOriginator = 8,
    AssignNone = 0xF,
};

// A call convention's bits: who's assigned (Assignee), the tool's locality (3 anywhere), status (2 any state) and preference (5
// anyhow), which the game never reads, and the argument (the linked object's or global agent's index, 0xFFFF none)
union CallConventionBits
{
    u32 value;
    struct
    {
        u32 assignee : 4;
        u32 unused4 : 4;
        u32 unused8 : 4;
        u32 unused12 : 4;
        u32 argument : 16;
    };
};
CHECK_SIZE(CallConventionBits, 4);

// The way a starter's assigner calls its graph (4 bytes, retail's call convention, read whole)
struct CallConvention
{
    static constexpr u16 NoArgument = 0xFFFF;

    CallConventionBits bits;

    // The AgentLab tool's defaults: no type, anywhere, any state, anyhow, no argument
    void SetDefaults() RETAIL(FUN_00220760);
    void Read(Stream* stream) RETAIL(ReadAssignerConvention);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002207a0);
};

// An assigner of a starter (8 bytes): how it calls its graph, and the graph (its script's index plus 1 until resolved: the
// graph's data then, nullptr for none)
struct Assigner
{
    CallConvention* convention;
    union
    {
        s32 graphIndex;
        struct GraphData* graph;
    };

    void Read(Stream* stream) RETAIL(ReadStarterAssigner);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002097d8);
};
CHECK_SIZE(Assigner, 8);

// A starter (retail's HeaderScript, 0x3C bytes): its assigners (the count read and cleared as a word)
struct ScriptStarter : ScriptResource
{
    union
    {
        u32 assignerCountWord;
        struct
        {
            u8 assignerCount;
            u8 unused1D[3];
        };
    };
    Assigner* assigners[7];

    static ScriptStarter* Construct(ScriptStarter* starter) RETAIL(FUN_002095e8);
    void Destroy(u32 destroyFlags) RETAIL(DestroyHeaderScript);
    // The assigners' graphs found among the scripts by their index (once)
    void Resolve(ResourceTable* scripts) RETAIL(LoadMajorScriptData);
    void Read(Stream* stream) RETAIL(ReadHeaderScript);
    u32 ItemType() RETAIL(FUN_00208890);
    u32 IsStarter() RETAIL(IsHeader);
};
CHECK_OFFSET(ScriptStarter, assignerCount, 0x1C);
CHECK_SIZE(ScriptStarter, 0x3C);

// A control packet's settings (the AgentLab tool's names): where its target and offset are (ControlPacket::Space), its motion
// (ControlPacket::Motion), the acceleration's curve (ControlPacket::SmoothCurve), whether it translates and rotates the instance,
// tracks its destination (the target follows it), interpolates angles (a throw's end turns to the rotation's target), faces
// with its yaw, orients by its velocity, the natural axes it rolls along (ControlPacket::Axes), and whether it stalls (its delay
// and sync never end it). A projectile's motion reads bits 7 and 8 (the tool's continuous rotation, which nothing else reads):
// it lands where its flight's time ends instead of on the target, and isn't turned at the end. The tool's translation continues
// (bit 15), pitch faces (19), "valid data" (21, always set), key is local (22), uses rotator (23), uses interpolator (24), uses
// physics (25) and continuously rotates in world space (26) are never read
union ControlPacketSettings
{
    u32 value;
    struct
    {
        u32 space : 3;
        u32 motion : 4;
        u32 landsWhereItIs : 1;
        u32 skipsTurn : 1;
        u32 unused9 : 2;
        u32 acceleration : 2;
        u32 translates : 1;
        u32 rotates : 1;
        u32 unused15 : 1;
        u32 tracksDestination : 1;
        u32 interpolatesAngles : 1;
        u32 yawFaces : 1;
        u32 unused19 : 1;
        u32 orientsPredicts : 1;
        u32 unused21 : 6;
        u32 axes : 3;
        u32 unused30 : 1;
        u32 stalls : 1;
    };
};
CHECK_SIZE(ControlPacketSettings, 4);

// A state's control packet (0xC bytes): the counts of its data (read and cleared as a word: the slots, the floats and the tool's
// version, which nothing reads), its settings and its data: the floats, then a slot for each of its values (0xFF none, bit 7 an
// instance property's index, else a float's). The AgentLab tool's names of the values, by index: 0 Selector, 1 KeyIndex, 2
// MoveSpeed, 3 TurnSpeed, 4-6 RawPos, 7-9 Pitch, Yaw, Roll, 10 Delay, 11 Duration, 12 TumbleData, 13 SpinData, 14 TwistData, 15
// RandRange, 16 Power, 17 Damping, 18 AcDist, 19 DecDist, 20 Bounce, 21 SyncUnit, 22 JointIndex
struct ControlPacket
{
    // Where its target and offset are taken from (the tool's names; SetTranslationTarget in game/motion.cpp says where each is)
    enum Space : u32
    {
        WorldSpace = 0,
        InitialSpace = 1,
        CurrentSpace = 2,
        TargetSpace = 3,
        ParentSpace = 4,
        InitialPosition = 5,
        CurrentPosition = 6,
        StoredSpace = 7,
    };

    // The acceleration's curves the motion tells apart (the others are linear)
    static constexpr u32 SmoothCurve = 2;

    // The natural axes it rolls along (the tool's names)
    enum Axes : u32
    {
        NoNatural = 0,
        XNatural = 1,
        YNatural = 2,
        ZNatural = 3,
        AllNatural = 4,
    };

    // The values by the tool's names (the motion code reads 20 and 19 as the times its acceleration and deceleration take, and
    // 17 as the step a facing's yaw is rounded to)
    enum Value : u32
    {
        Selector = 0,
        KeyIndex = 1,
        MoveSpeed = 2,
        TurnSpeed = 3,
        RawPosX = 4,
        Pitch = 7,
        Delay = 10,
        Duration = 11,
        TumbleData = 12,
        RandRange = 15,
        Power = 16,
        Damping = 17,
        AcDist = 18,
        DecDist = 19,
        Bounce = 20,
        SyncUnit = 21,
        JointIndex = 22,
    };

    // The motions (the tool's names)
    enum Motion : u32
    {
        NoMotion = 0,
        ConstantVelocity = 1,
        Accelerated = 2,
        Spring = 3,
        Projectile = 4,
        LinearInterpolation = 5,
        SmoothPath = 6,
        FaceDestinationOnly = 7,
        Drive = 8,
        GroundChase = 9,
        AirChase = 10,
        // 11 to 13 are chases as well, 13 a climbing one (its body keeps to what it touches:
        // ObjectRigidBodyState::followsSurface)
        ClimbingChase = 13,
    };

    // A value's slot: the index of a float of its data or of an instance property; NoSlot when it isn't given
    union ValueSlot
    {
        u8 value;
        struct
        {
            u8 index : 7;
            u8 isProperty : 1;
        };
    };

    static constexpr u32 NoSlot = 0xFF;

    union
    {
        u32 countsWord;
        struct
        {
            u8 slotCount;
            u8 floatCount;
            u16 unused02;
        };
    };
    ControlPacketSettings settings;
    u8* data;

    static ControlPacket* Construct(ControlPacket* packet) RETAIL(FUN_00209150);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00209168);
    // Its counts, then its data (what it had freed first)
    void Read(Stream* stream) RETAIL(ReadScriptControlPacket);
    // Whether a value is given
    u32 HasByte(u32 index) RETAIL(FUN_002095a8);
    // A value's float or word (a property's slot taken as a float's): whether it's given
    u32 Float(u32 index, f32* value) RETAIL(FUN_00209280);
    u32 Word(u32 index, u32* value) RETAIL(FUN_002092e8);
    // The same (a second copy in retail)
    u32 Word2(u32 index, u32* value) RETAIL(FUN_00209350);
    // A value as a float, an integer or an angle (radians made 65536ths of a turn), the instance's properties read for a
    // property's slot: whether it's given
    u32 GetFloat(u32 index, class PropertyHolder* properties, f32* value) RETAIL(GetFloatFromByteIndexInScriptControlPacket);
    u32 GetInt(u32 index, class PropertyHolder* properties, s32* value) RETAIL(GetIntFromByteIndexInScriptSupport1);
    u32 GetAngle(u32 index, class PropertyHolder* properties, s32* value) RETAIL(GetFlagFromByteIndexInScriptSupport1);
    // Three values from an index (0 the ones not given; a vector's w 1): whether one of them isn't given. The first doesn't read
    // properties
    u32 GetRawAngles(u32 index, s32* angles) RETAIL(FUN_002071f0);
    u32 GetVector(u32 index, class PropertyHolder* properties, struct Vector4* vector) RETAIL(GetVector4FromByteIndexInScriptSupport1);
    u32 GetAngles(u32 index, class PropertyHolder* properties, s32* angles) RETAIL(FUN_00207530);

    ValueSlot SlotOf(u32 index) const
    {
        if (index >= slotCount)
        {
            return {NoSlot};
        }

        // Without data the slot is read at the index's address (the game's)
        u32 slots = data != nullptr ? reinterpret_cast<u32>(data) + floatCount * sizeof(f32) : 0;
        return {*reinterpret_cast<const u8*>(slots + index)};
    }

    const f32* Floats() const
    {
        return reinterpret_cast<const f32*>(data);
    }
};
CHECK_OFFSET(ControlPacket, settings, 0x4);
CHECK_SIZE(ControlPacket, 0xC);

// A command's bits: its ID (the builder leaves every bit set: NoId) and whether another command follows
union ScriptCommandBits
{
    u32 value;
    struct
    {
        u32 id : 24;
        u32 hasNext : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(ScriptCommandBits, 4);

// A command of a body or of an object's script pack (game/commands.h, made by the object builder by its ID): its bits, the next
// one, its vtable (1 the destructor, which destroys the commands after it too, 2 the development tools' parser of its
// arguments, 3 its execution, 4 its execution on a node, 5 its size, 6 its class's ID; the base's 2 and 4 do nothing, 3 and 5
// are abstract), then its arguments
struct ScriptCommand
{
    enum Slot : u32
    {
        DestroySlot = 1,
        ExecuteSlot = 3,
        ExecuteOnSlot = 4,
        SizeSlot = 5,
    };

    static constexpr u32 ClassId = CommandClassId;
    static constexpr u32 NoId = 0xFFFFFF;

    ScriptCommandBits bits;
    ScriptCommand* next;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_002083a0);
    void ParseTokens() RETAIL(ScriptCommand_NoOp);
    void ExecuteOn() RETAIL(ScriptCommand_NoOp2);
    u32 GetClassId() RETAIL(ScriptCommand_GetClassId_0x1806);
};
CHECK_SIZE(ScriptCommand, 0xC);

// A condition's word: its ID, whether its result passes below the threshold instead of above it, and the parameter the script
// gives its check
union ScriptConditionBits
{
    u32 value;
    struct
    {
        u32 id : 16;
        u32 inverted : 1;
        u32 parameter : 15;
    };
};
CHECK_SIZE(ScriptConditionBits, 4);

// A condition of a body (game/conditions.h, made by the object builder by its ID): its word, its three floats (the window in
// seconds the event conditions look back over, the threshold its result passes and the weight of how far it passes) and its
// vtable (1 the destructor, 2 its check (abstract), 3 its class's ID)
struct ScriptCondition
{
    enum Slot : u32
    {
        DestroySlot = 1,
        CheckSlot = 2,
    };

    static constexpr u32 ClassId = ConditionClassId;

    ScriptConditionBits bits;
    union
    {
        f32 values[3];
        struct
        {
            f32 window;
            f32 threshold;
            f32 weight;
        };
    };
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00122f70);
    u32 GetClassId() RETAIL(FUN_0011e1d8);

    u32 Parameter() const
    {
        return bits.parameter;
    }
};
CHECK_OFFSET(ScriptCondition, threshold, 0x8);
CHECK_SIZE(ScriptCondition, 0x14);

struct GraphState;

// A body's bits: its commands' count, whether its jump re-enters the state it's in when that's where it goes (restart), and
// whether it has a condition, a jump and another body after it
union StateBodyBits
{
    u32 value;
    struct
    {
        u32 commandCount : 8;
        u32 restarts : 1;
        u32 hasCondition : 1;
        u32 hasJump : 1;
        u32 hasNext : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(StateBodyBits, 4);

// A body of a state (0x14 bytes): its bits, its condition, the state it jumps to (its index until the graph is read), its
// commands, the next body
struct StateBody
{
    StateBodyBits bits;
    ScriptCondition* condition;
    union
    {
        s32 jumpIndex;
        GraphState* jump;
    };
    ScriptCommand* commands;
    StateBody* next;

    // Made empty
    static StateBody* Construct(StateBody* body) RETAIL(FUN_002088e0);
    void Read(Stream* stream) RETAIL(ReadStateBody);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00208900);
};
CHECK_SIZE(StateBody, 0x14);

// A state's bits: its bodies' count, whether its first body is the completion body (run when its control packet or child
// behaviour ends), whether it interrupts (its bodies are checked while its child behaviour runs), whether its child behaviour is a
// slot of the object's behaviours rather than a graph's ID, whether it has a control packet and another state after it, and its
// child behaviour (NoScriptId none). Bits 5-9 and 13 are the tools' and never read
union GraphStateBits
{
    u32 value;
    struct
    {
        u32 bodyCount : 5;
        u32 unused5 : 5;
        u32 hasCompletion : 1;
        u32 interrupting : 1;
        u32 childIsSlot : 1;
        u32 unused13 : 1;
        u32 hasPacket : 1;
        u32 hasNext : 1;
        u32 child : 16;
    };
};
CHECK_SIZE(GraphStateBits, 4);

// A state of a graph (0x10 bytes): its bits, its control packet, its bodies, the next state
struct GraphState
{
    GraphStateBits bits;
    ControlPacket* packet;
    StateBody* bodies;
    GraphState* next;

    // Made empty (no child behaviour)
    static GraphState* Construct(GraphState* state) RETAIL(FUN_00208df0);
    // Read with the states after it (each put in the jump table), their bodies when asked
    void Read(Stream* stream, u32 readBodies) RETAIL(LoadScriptState);
    void ReadBodies(Stream* stream) RETAIL(LoadStateBodies);
    // Its bodies' jumps made the states of the jump table
    void ResolveJumps() RETAIL(LoadStateBodyAddresses);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00208e10);
};
CHECK_SIZE(GraphState, 0x10);

// A graph's bits: its ID (NoScriptId none)
union GraphDataBits
{
    u32 value;
    struct
    {
        u32 id : 16;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(GraphDataBits, 4);

// A graph's data (0x18 bytes): its bits, its start state, its name and its states
struct GraphData
{
    GraphDataBits bits;
    GraphState* start;
    String name;
    GraphState* states;

    // Made empty: no name and states, its ID none and no start state (ClearHead)
    static GraphData* Construct(GraphData* graph) RETAIL(FUN_00208b58);
    void ClearHead() RETAIL(FUN_00208b98);
    // Its bits, name, states and their bodies (the start state the one of the index read)
    void Read(Stream* stream) RETAIL(ReadScriptData);
};
CHECK_SIZE(GraphData, 0x18);

// A graph (retail's MajorScript, 0x20 bytes): its data
struct ScriptGraph : ScriptResource
{
    GraphData* data;

    void Destroy(u32 destroyFlags) RETAIL(DestroyMajorScript);
    u16 Id() RETAIL(GetScriptId_002088C0);
    void Resolve() RETAIL(SetFlagField_00209C38);
    void Read(Stream* stream) RETAIL(ReadMainScript);
    u32 ItemType() RETAIL(FUN_002088d8);
    u32 IsStarter() RETAIL(IsHeader_002088D0);
};
CHECK_SIZE(ScriptGraph, 0x20);

// The builder of the items the game reads (16 bytes): a list of the factories that make them by their ID (the game context's item
// builders, the last one added first; each node the factory after its links), how many there are and how many were added. A
// factory's vtable's slot 2 makes the object of an ID (none for IDs it has none of); the scripts' commands and conditions only
// of their kind
struct ObjectBuilder
{
    // The kinds of objects it makes
    enum Kind : s32
    {
        CommandKind = -10,
        ConditionKind = -11,
    };

    struct Node
    {
        Node* previous;
        Node* next;
        void* factory;
    };

    Node* first;
    Node* last;
    u32 count;
    u32 added;

    static ObjectBuilder* Construct(ObjectBuilder* builder) RETAIL(FUN_00101408);
    // An object made by the first factory that makes the ID (kinds: -10 commands, -11 conditions)
    void* Build(u32 id, s32 kind) RETAIL(BuildObject);
    // A factory put in front of the others
    void Add(void* factory) RETAIL(FUN_002b5a08);
};
CHECK_SIZE(ObjectBuilder, 0x10);

// A script object of the builder's (game/commands.h, game/conditions.h): its header made (a command's ID none and no next one, a
// condition's word its ID) with its class's vtable. The builder's constructors set the arguments too, which the reader always
// overwrites (only a few conditions keep values of their own)
template <typename T>
T* MakeCommand(T* command, const GccVTableEntry* vtable)
{
    ScriptCommandBits made = {};
    made.id = ScriptCommand::NoId;
    command->bits = made;
    command->next = nullptr;
    command->vtable = vtable;
    return command;
}

template <typename T>
T* MakeCondition(T* condition, u16 id, const GccVTableEntry* vtable)
{
    ScriptConditionBits made = {};
    made.id = id;
    condition->bits = made;
    condition->vtable = vtable;
    return condition;
}

extern "C"
{
    extern const GccVTableEntry g_ScriptCommandVTable[] RETAIL(ScriptCommandInterface_Methods);
    extern const GccVTableEntry g_ScriptConditionVTable[] RETAIL(D_002F0358);
    // The factories of the builder's commands and conditions (their vtable's slot 2: the object of the ID when the kind is
    // theirs, none for IDs that have none)
    void* BuildScriptCommand(void* factory, u32 id, s32 kind) RETAIL(BuildScriptCommand);
    void* BuildScriptCondition(void* factory, s32 id, s32 kind) RETAIL(BuildScriptCondition);
    extern const GccVTableEntry g_ScriptVTable[] RETAIL(D_002FCEC8);
    extern const GccVTableEntry g_StarterVTable[] RETAIL(HeaderScript_methods);
    extern const GccVTableEntry g_GraphVTable[] RETAIL(MajorScript_methods);
    // The states of the graph being read by their index, and how many there are
    extern GraphState* g_StateJumpTable[512] RETAIL(G_ScriptStateJumpTable);
    extern u32 g_StateCount RETAIL(G_StateDepth);
    extern ObjectBuilder* g_ObjectBuilder RETAIL(G_ObjectBuilder_);

    // A command (with the ones after it) and a condition read
    ScriptCommand* ReadCommand(Stream* stream) RETAIL(ReadScriptCommand);
    ScriptCondition* ReadCondition(Stream* stream) RETAIL(ReadScriptCondition);
    // The AgentLab items' builder's slot 2 (the game context's, vtable D_002FD2C0): an item of a class ID (AgentLabClassId: not
    // the script resources' base, the commands' and the conditions'), none for another
    void* MakeAgentLabItem(void* builder, u32 classId) RETAIL(FUN_00209ef8);
    // The game context's constructor's settings 0x60 bytes before the states' jump table, which nothing reads
    void InitUnusedAgentLabSettings() RETAIL(InitSomeUnkownGlobals);
}
