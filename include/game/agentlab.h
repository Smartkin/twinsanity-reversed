#pragma once

#include "abi.h"
#include "common.h"
#include "game/string.h"
#include "gcc2.h"

class Stream;
struct ResourceTable;

// The behaviour scripts (AgentLab) as the game keeps them: script resources of two kinds, starters (the assigners that make the
// game run a graph on its own) and graphs (states of bodies of a condition and commands, and a jump to another state)

// A script resource's base (retail's GenericScriptInfo, 0x1C bytes; vtable 0x18 bytes in: 1 the destructor, 2 its script's ID, 3
// its references resolved, 4 the read, 5 the write, 6 its section's item type, 7 whether it's a starter): its resource header, its
// resource ID, its bits (its script's ID in the low half, 0xFFFF none; its priority in bits 16-23, 50 when made; bit 24 resolved)
// and a name
struct ScriptResource
{
    enum Bits : u32
    {
        IdMask = 0xFFFF,
        PriorityShift = 16,
        Resolved = 0x1000000,
    };

    u32 header;
    s32 resourceId;
    u32 bits;
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

// The way a starter's assigner calls its graph (4 bytes, retail's call convention, read whole)
struct CallConvention
{
    u32 bits;

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

// A starter (retail's HeaderScript, 0x3C bytes): its assigners (the count read as a word)
struct ScriptStarter : ScriptResource
{
    u8 assignerCount;
    u8 unknown1D[3];
    Assigner* assigners[7];

    static ScriptStarter* Construct(ScriptStarter* starter) RETAIL(FUN_002095e8);
    void Destroy(u32 destroyFlags) RETAIL(DestroyHeaderScript);
    // The assigners' graphs found among the scripts by their index (once)
    void Resolve(ResourceTable* scripts) RETAIL(LoadMajorScriptData);
    void Read(Stream* stream) RETAIL(ReadHeaderScript);
    u32 ItemType() RETAIL(FUN_00208890);
    u32 IsStarter() RETAIL(IsHeader);
};
CHECK_SIZE(ScriptStarter, 0x3C);

// A state's control packet (0xC bytes): the counts of its data (bytes 0 and 1: the slots and the floats, a halfword the version),
// its settings and its data: the floats, then a slot for each of its values (0xFF none, bit 7 an instance property's index, else a
// float's). The AgentLab tool's names of the values, by index: 0 Selector, 1 KeyIndex, 2 MoveSpeed, 3 TurnSpeed, 4-6 RawPos,
// 7-9 Pitch, Yaw, Roll, 10 Delay, 11 Duration, 12 TumbleData, 13 SpinData, 14 TwistData, 15 RandRange, 16 Power, 17 Damping,
// 18 AcDist, 19 DecDist, 20 Bounce, 21 SyncUnit, 22 JointIndex
struct ControlPacket
{
    // The settings (the tool's names): the space (bits 0-2), the motion (3-6), the continuous rotation (7-10), the acceleration
    // (11-12), then flags
    enum Settings : u32
    {
        SpaceMask = 0x7,
        MotionShift = 3,
        MotionMask = 0xF,
        ContinuousRotationShift = 7,
        AccelerationShift = 11,
        AccelerationMask = 0x3,
        Translates = 0x2000,
        Rotates = 0x4000,
        TranslationContinues = 0x8000,
        TracksDestination = 0x10000,
        InterpolatesAngles = 0x20000,
        YawFaces = 0x40000,
        PitchFaces = 0x80000,
        OrientsPredicts = 0x100000,
        KeyIsLocal = 0x400000,
        UsesRotator = 0x800000,
        UsesInterpolator = 0x1000000,
        UsesPhysics = 0x2000000,
        RotatesInWorldSpace = 0x4000000,
        AxesShift = 27,
        AxesMask = 0x7,
        Stalls = 0x80000000,
    };

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
        // 11 to 13 are chases as well, 13 sets bit 4 of the node's part 0x12C bytes in
        LastChase = 13,
    };

    static constexpr u32 NoSlot = 0xFF;
    static constexpr u32 PropertySlot = 0x80;

    u8 counts[4];
    u32 settings;
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

    u32 SlotOf(u32 index) const
    {
        if (index >= counts[0])
        {
            return NoSlot;
        }

        // Without data the slot is read at the index's address (the game's)
        u32 slots = data != nullptr ? reinterpret_cast<u32>(data) + counts[1] * 4 : 0;
        return *reinterpret_cast<const u8*>(slots + index);
    }

    u32 MotionKind() const
    {
        return settings >> MotionShift & MotionMask;
    }

    u32 Acceleration() const
    {
        return settings >> AccelerationShift & AccelerationMask;
    }
};
CHECK_SIZE(ControlPacket, 0xC);

// A command of a body or of an object's script pack (game/commands.h, made by the object builder by its ID): its bits (bits 0-23
// all set by the builder, bit 24 another follows), the next one, its vtable (1 the destructor, which destroys the commands after it
// too, 2 the development tools' parser of its arguments, 3 its execution, 4 its execution on a node, 5 its size, 6 its class's
// ID; the base's 2 and 4 do nothing, 3 and 5 are abstract), then its arguments
struct ScriptCommand
{
    enum Bits : u32
    {
        IdMask = 0xFFFFFF,
        HasNext = 0x1000000,
    };

    static constexpr u32 ClassId = 0x1806;

    u32 bits;
    ScriptCommand* next;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_002083a0);
    void ParseTokens() RETAIL(ScriptCommand_NoOp);
    void ExecuteOn() RETAIL(ScriptCommand_NoOp2);
    u32 GetClassId() RETAIL(ScriptCommand_GetClassId_0x1806);
};
CHECK_SIZE(ScriptCommand, 0xC);

// A condition of a body (game/conditions.h, made by the object builder by its ID): its word (its ID in the low half), its three
// floats and its vtable (1 the destructor, 2 its check (abstract), 3 its class's ID)
struct ScriptCondition
{
    static constexpr u32 ClassId = 0x1807;

    u32 bits;
    f32 values[3];
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00122f70);
    u32 GetClassId() RETAIL(FUN_0011e1d8);

    // What the script gives the check (bits 17-31 of its word; bit 16 inverts its result)
    u32 Parameter() const
    {
        return bits >> 17;
    }
};
CHECK_SIZE(ScriptCondition, 0x14);

struct GraphState;

// A body of a state (0x14 bytes): its bits (bits 0-7 the commands' count, bit 9 a condition, 10 a jump, 11 another body), its
// condition, the state it jumps to (its index until the graph is read), its commands, the next body
struct StateBody
{
    enum Bits : u32
    {
        CommandCountMask = 0xFF,
        HasCondition = 0x200,
        HasJump = 0x400,
        HasNext = 0x800,
    };

    u32 bits;
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

// A state of a graph (0x10 bytes): its bits (bits 0-4 its bodies, bit 14 a control packet, 15 another state), its control packet,
// its bodies, the next state
struct GraphState
{
    enum Bits : u32
    {
        BodyMask = 0x1F,
        HasPacket = 0x4000,
        HasNext = 0x8000,
    };

    u32 bits;
    ControlPacket* packet;
    StateBody* bodies;
    GraphState* next;

    // Made empty (its bits' high half 0xFFFF)
    static GraphState* Construct(GraphState* state) RETAIL(FUN_00208df0);
    // Read with the states after it (each put in the jump table), their bodies when asked
    void Read(Stream* stream, u32 readBodies) RETAIL(LoadScriptState);
    void ReadBodies(Stream* stream) RETAIL(LoadStateBodies);
    // Its bodies' jumps made the states of the jump table
    void ResolveJumps() RETAIL(LoadStateBodyAddresses);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00208e10);
};
CHECK_SIZE(GraphState, 0x10);

// A graph's data (0x18 bytes): its bits (its ID in the low half), its start state, its name and its states
struct GraphData
{
    u32 bits;
    GraphState* start;
    String name;
    GraphState* states;

    // Made empty: no name and states, its ID none and no start state (ClearHead)
    static GraphData* Construct(GraphData* data) RETAIL(FUN_00208b58);
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

// A script object of the builder's (game/commands.h, game/conditions.h): its header made (a command's bits all set and no next
// one, a condition's word its ID) with its class's vtable. The builder's constructors set the arguments too,
// which the reader always overwrites (only a few conditions keep values of their own)
template <typename T>
T* MakeCommand(T* command, const GccVTableEntry* vtable)
{
    command->bits = ScriptCommand::IdMask;
    command->next = nullptr;
    command->vtable = vtable;
    return command;
}

template <typename T>
T* MakeCondition(T* condition, u16 id, const GccVTableEntry* vtable)
{
    condition->bits = id;
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
    // The AgentLab items' builder's slot 2 (the game context's, vtable D_002FD2C0): an item of a class ID (0x1800 a graph's state,
    // 0x1801 a graph's data, 0x1803 a starter, 0x1804 a graph, 0x1805 a state's body, 0x1808 a control packet, 0x1809 a call
    // convention, 0x180F an AI position, 0x1810 an AI path; 0x180C, 0x180E and 0x1811 unknown items), none for another
    void* MakeAgentLabItem(void* builder, u32 classId) RETAIL(FUN_00209ef8);
    // The game context's constructor's settings 0x60 bytes before the states' jump table, which nothing reads
    void InitUnusedAgentLabSettings() RETAIL(InitSomeUnkownGlobals);
}
