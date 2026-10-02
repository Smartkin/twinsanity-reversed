#pragma once

#include "common.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/string.h"

struct ChunkLights;
struct GameOGI;
struct Light;
struct GamePad;
struct InstanceContext;
struct OgiAnimator;
struct PlayerCharacter;
struct TimeClock;

// The kinds of an instance's nodes (still asm): its model, the camera's lens, the controls (the pad they're read from), the
// player's, and the one kept at 0x16
enum NodeKind : u32
{
    NodeModel = 0x3,
    NodeCameraLens = 0x9,
    NodeControls = 0xB,
    NodePlayer = 0xC,
    Node16 = 0x16,
};

// The base of an instance's nodes (the retail GameNode, its vtable at 0x14): the instance it's in, its flags (bit 0: the next update
// keeps its time, bit 1: in its chunk's list of its kind), the time it was last updated at and the links of its chunk's list (or of
// the nodes to free). Its vtable's functions: 1 an event handled (the event's handle the
// callee's: the base lets it go), 2 the destructor, 3 given the instance it's in (kept only when it has none), 4 whether its
// instance may change chunks,
// 5 its kind (abstract), 6 its instance left its chunk, 7 a step with the instance's clock (the base takes the clock's time), 8
// the update (the base's keeps its time once when asked), 9 (nothing in the base), 10 and 14 abstract
class GameNode
{
public:
    enum Flags : u16
    {
        FlagKeepTime = 0x1,
        FlagListed = 0x2,
    };

    InstanceContext* owner;
    u16 flags;
    u16 unknown06;
    u32 time;
    GameNode* previous;
    GameNode* next;
    const GccVTableEntry* vtable;

    static GameNode* Construct(GameNode* node) RETAIL(InitGameNode_);
    void HandleEvent(Reference** event) RETAIL(FUN_00192588);
    void Destroy(u32 destroyFlags) RETAIL(DestroyNode_);
    void SetOwner(InstanceContext* instance) RETAIL(SetContextIfNone);
    // Whether it lets its instance move from a chunk into the one a link leads to (the base does)
    u32 CanChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_0019a328);
    void LeftChunk(u32 unknown) RETAIL(FUN_0019a330);
    void Step(TimeClock* clock, u32 unknown) RETAIL(FUN_0019a338);
    u32 Update(TimeClock* clock) RETAIL(UpdateNode);
    void Unknown9() RETAIL(FUN_0019a348);
};
CHECK_SIZE(GameNode, 0x18);

// An instance's nodes: one of each kind (24 kinds, the kind is the node's vtable function 5), a bit per kind it has
struct NodeList
{
    static constexpr u32 Kinds = 24;

    u32 mask;
    GameNode* nodes[Kinds];

    static NodeList* Construct(NodeList* list) RETAIL(FUN_00192608);
    // Every node destroyed and freed (the list freed too when the flags say)
    void Destroy(u32 destroyFlags) RETAIL(DestroyNodesList);
    // Every node told its instance left its chunk, and stepped
    void LeftChunk(u32 unknown) RETAIL(FUN_001926c8);
    void Step(TimeClock* clock, u32 unknown) RETAIL(FUN_00192738);
    // A node put in its kind's place (not when another one has it) and taken out (when there's one in it). Whether it was
    u32 Add(GameNode* node) RETAIL(AttachNodeToContext);
    u32 Remove(GameNode* node) RETAIL(FUN_00192830);
};
CHECK_SIZE(NodeList, 0x64);

// The events queued for an instance (still asm): a list of the events' handles
struct QueuedEvent
{
    Reference* event;
    QueuedEvent* previous;
    QueuedEvent* next;
};

// Where an instance is (still asm): its rotation and position, a flag, and the chunk it's in. A character's node of kind 1 has one
// for where it comes back to
struct alignas(16) InstancePlacement
{
    Vector4 rotation;
    Vector4 position;
    // In retail the low bits of a 64 bit word the chunk's string shares
    u32 flags;
    String chunk;

    // The chunk's path taken as its chunk when there's a chunk
    static InstancePlacement* Construct(InstancePlacement* placement, struct ChunkData* chunk) RETAIL(FUN_001958c8);
    InstancePlacement* Assign(const InstancePlacement* other) RETAIL(FUN_001959a8);
    // The instance's place, with the flag
    void Take(InstanceContext* instance, u32 flag) RETAIL(FUN_00195a08);
    // The instance put there (moved to the chunk; when the chunk isn't loaded, it's let go unless the flag is set)
    void Apply(InstanceContext* instance) RETAIL(FUN_00195c38);
};
CHECK_SIZE(InstancePlacement, 0x30);

// The places an instance keeps (0x190 bytes of the heap): a string and 8 places
struct InstancePlaces
{
    u32 unknown00;
    String name;
    InstancePlacement places[8];
};
CHECK_SIZE(InstancePlaces, 0x190);

// An iterator over a node list's nodes (the retail one, its vtable at 0: 1 the destructor, 2 the first node, 3 whether it's done,
// 4 the current node's place, 5 the next node): the kinds the list has, in order
struct NodeIterator
{
    const GccVTableEntry* vtable;
    NodeList* list;
    u32 index;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0019a658);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0019a628);
    void First() RETAIL(FUN_0019a688);
    u32 IsDone() RETAIL(HasNodeSpaceInList);
    GameNode** Current() RETAIL(FUN_0019a6f0);
    void Next() RETAIL(FUN_0019a708);
};

// A chunk's instances (0x68 bytes, the chunk data's): its sleeping instances' list, a list of the nodes of each kind (the ones that
// are stepped) and the kind being stepped (0xFF none), whose nodes taken out meanwhile are kept to be taken out after
struct ChunkInstances
{
    InstanceContext* sleeping;
    u8 steppingKind;
    u8 unknown05[3];
    GameNode* nodes[NodeList::Kinds];

    static ChunkInstances* Construct(ChunkInstances* chunkInstances) RETAIL(FUN_001991a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001991d8);
    // Whether no kind has nodes, and whether it has neither nodes nor sleeping instances
    u32 HasNoNodes() RETAIL(FUN_00199170);
    u32 IsEmpty() RETAIL(FUN_00199a70);
    // A node put in its kind's list (once) and taken out (later when its kind is being stepped: the node told, its vtable's
    // function 9, when it's taken out now). Whether it was
    u32 AddNode(GameNode* node) RETAIL(FUN_001993b0);
    u32 RemoveNode(GameNode* node) RETAIL(FUN_00199430);
    // An instance's nodes put in their lists, and taken out
    void AddNodes(InstanceContext* instance) RETAIL(FUN_00198ec0);
    void RemoveNodes(InstanceContext* instance) RETAIL(FUN_00199018);
    // An instance woken (out of the sleeping list when it's asleep, its nodes in) and put to sleep (into the list when it isn't
    // asleep, its nodes out); an instance added (into the list when it's asleep, its nodes in either way) and removed (out of the
    // list when it's asleep, its nodes out). Each returns 1
    u32 WakeInstance(InstanceContext* instance) RETAIL(FUN_00199200);
    u32 SleepInstance(InstanceContext* instance) RETAIL(FUN_00199260);
    u32 AddInstance(InstanceContext* instance) RETAIL(FUN_001992c0);
    u32 RemoveInstance(InstanceContext* instance) RETAIL(FUN_00199320);
    // The instance's nodes taken out and the instance put in the list of the instances to free
    void ReleaseInstance(InstanceContext* instance) RETAIL(FUN_00199380);
    // A sleeping instance moved to the instances of no chunk; the sleeping instances that have a node of the filter's first word's
    // kinds, the flags of its second and none of its third (everything to free freed first)
    void MakeGlobal(u32 unknown, InstanceContext* instance) RETAIL(FUN_00199508);
    void MakeGlobalWhere(u32 unknown, const u32* filter) RETAIL(FUN_00199558);
    // A frame of its instances' nodes, kind by kind (still asm)
    u32 Update(struct GameTimeController* clock, void* clocks, s32 detail) RETAIL(FUN_00199848);
    // Every sleeping instance released
    void Release() RETAIL(FUN_001997d0);
};
CHECK_SIZE(ChunkInstances, 0x68);

// The instances' IDs: an instance for each ID given, the count of them (an ID let go is given to the last instance)
struct InstanceIds
{
    struct Entry
    {
        InstanceContext* instance;
        u32 unknown04;
    };

    Entry entries[256];
    u16 count;

    static InstanceIds* Construct(InstanceIds* ids) RETAIL(FUN_00199bf0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00199c48);
    // The instance given the next ID (none past 256), and its ID let go
    void Add(InstanceContext* instance) RETAIL(FUN_00199ca0);
    void Remove(InstanceContext* instance) RETAIL(FUN_00199cd8);
};
CHECK_OFFSET(InstanceIds, count, 0x800);

// An instance's context (the game's objects' instances): a referenced object with its box (a corner of the default box, its W 1),
// its nodes, the events queued for it, the places it keeps (a block of 0x190 bytes: a string, then 8 places), the index of the
// clock it goes by (of its chunk's clocks, else the game's) and its ID (-1: none)
struct InstanceContext : ReferencedObject
{
    // (0xB4 to 0xC0 is the base's padding: its collision's boxes align it to 16 bytes)
    Vector4 box;
    // The instance it hangs from
    InstanceContext* parent;
    NodeList nodes;
    QueuedEvent* events;
    InstancePlaces* places;
    // The links of its chunk's list of instances (or of the instances of no chunk)
    InstanceContext* previous;
    InstanceContext* next;
    // The links of its scenery cell's list of instances
    InstanceContext* cellPrevious;
    InstanceContext* cellNext;
    // A stamp of when it was last seen (24 bits)
    u8 seen[3];
    u8 clockIndex;
    s32 id;

    // A new one (the referenced object's base made first)
    static InstanceContext* Construct(InstanceContext* instance) RETAIL(FUN_001979f0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00197ab8);
    // Its nodes told it left its chunk, and its chunk forgotten
    void LeaveChunk(u32 unknown) RETAIL(FUN_00197b88);
    // Its queued events let go, then its nodes stepped with its clock of the clocks
    void Step(TimeClock* clocks, u32 unknown) RETAIL(FUN_00197bb8);
    // Its vtable's functions: woken (put back in its chunk's lists), put to sleep (out of them; instances with an ID are
    // released instead), released (its places' instances first when it has places and no ID: when they don't let it, it isn't;
    // its ID let go, its chunk let go of it or it's freed, and put to sleep) and its step once it was queued (its place given its
    // box when it moved, else its box its place's position)
    u32 Wake() RETAIL(FUN_00197c58);
    u32 Sleep() RETAIL(FUN_00197c98);
    u32 Release() RETAIL(FUN_00197d00);
    void StepQueued() RETAIL(FUN_00197fa8);
    // Moved into the chunk a link of its chunk leads to when every node lets it: the chunk it's in after, none when it didn't move
    struct ChunkData* ChangeChunk(struct ChunkLinkData* link) RETAIL(FUN_00197de0);
    // The events queued for it let go
    void ClearEvents() RETAIL(FUN_00198328);
};
CHECK_OFFSET(InstanceContext, box, 0xC0);
CHECK_OFFSET(InstanceContext, nodes, 0xD4);
CHECK_OFFSET(InstanceContext, events, 0x138);
CHECK_OFFSET(InstanceContext, clockIndex, 0x153);
CHECK_SIZE(InstanceContext, 0x160);

// An instance's movement node (kind 0, still asm but its places): the seconds between its two places, its bits (1: they're
// captured for the frame) and its place's matrix a frame before and now (its update moves the one it had into the first)
struct MovementNode : GameNode
{
    enum Bits : u32
    {
        BitCaptured = 0x2,
    };

    f32 seconds;
    u32 bits;
    Matrix4x4 previousMatrix;
    Matrix4x4 matrix;

    // The matrices and the seconds, captured first when they aren't (both its instance's place now, a 60th of a second apart)
    Matrix4x4* PreviousMatrix() RETAIL(FUN_00192af0);
    Matrix4x4* CurrentMatrix() RETAIL(FUN_00192b38);
    f32 Seconds() RETAIL(FUN_00192b80);
    void Capture() RETAIL(CaptureMovementNodeTransform);
};
CHECK_OFFSET(MovementNode, previousMatrix, 0x20);
CHECK_OFFSET(MovementNode, matrix, 0x60);

// An instance's model node (kind 3, 0x30 bytes, still asm but its drawing): the OGI and the animator it's drawn with this frame
// (what its update left for the drawing, emptied once drawn) and what its lights are gathered with
struct ModelNode : GameNode
{
    u32 bits;
    GameOGI* unknown1C;
    GameOGI* drawnOgi;
    void* unknown24;
    OgiAnimator* drawnAnimator;
    Light* lighting;

    // Drawn with its instance's matrix in the world, lit by the strongest lights at it (gathered through the chunk's matrix):
    // with its animator's joints' matrices and blend shapes when it has one, else its OGI's rigid models at the matrix
    void Draw(const Matrix4x4* matrix, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode) RETAIL(FUN_001a1a00);
};
CHECK_SIZE(ModelNode, 0x30);

// The player's node: the character played
struct PlayerNode
{
    u8 unknown00[0x18];
    PlayerCharacter* character;
};

// The controls' node: the pad the character's controls are read from (none: the character isn't controlled)
struct ControlsNode
{
    u8 unknown00[0x18];
    GamePad* pad;
};

class AgentPart;
struct InstanceCreator;

// What an instance's agent node points at (retail's ObjectInstanceContext classes, game/agents.h: the character, the
// creature, ... the instance's script runs as): the instance, the behaviour starter its spawn runs, its object ID, its object,
// its properties (bit 6 of their state: it keeps a persistent flag of its chunk), the part of its type, its chunk's index and its
// ID in its chunk (the persistent flag slot it was given, which checkpoints find it by), the last contact message that told it
// something, and its vtable: 1 its instance's state flags applied (made again), 2 the destructor, 7 its node collided (the
// other, the point, the impulse), 9 a contact message (and its sender), 10 whether its instance may change chunks, 11 a position
// of its own (whether it has one), 14 the center of its collision box, 20 an event of type 0x1801 and its sender, 21 its
// velocity set, 22 its frame. The base's other functions do nothing (7 and 10 say yes, 12 no), 3 to 5 and 11 are abstract
class Agent
{
public:
    // Its instance's state flags (game/enums: the instance's or its object's properties' state)
    enum State : u32
    {
        StateDeactivated = 0x1,
        StateCollisionActive = 0x2,
        StateVisible = 0x4,
        StateShadowActive = 0x8,
        StateReceivesTriggerSignals = 0x100,
        StateCanDamageCharacter = 0x200,
        StateTargettable = 0x8000,
        StateCanAlwaysDamageCharacter = 0x10000,
        StateBulletsBounceBack = 0x20000,
    };

    InstanceContext* instance;
    u16 spawnScript;
    u16 objectId;
    struct GameObject* object;
    PropertyHolder* properties;
    AgentPart* part;
    u16 chunkIndex;
    u16 id;
    u8 unknown18[4];
    u32 unknown1C;
    ContactMessage contact;
    const GccVTableEntry* vtable;
    u8 unknown44[0x60 - 0x44];

    // Made for its creator's instance and object with its properties and part, a reference taken to its object and to every
    // resource the object lists; and those let go of
    static Agent* Construct(Agent* agent, InstanceCreator* creator, PropertyHolder* holder, AgentPart* part)
        RETAIL(FUN_002631f0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002632f0);
    void ClearUnknown18() RETAIL(FUN_00263410);
    void Nothing6() RETAIL(FUN_00262380);
    u32 Collided(void* other, const Vector4* point, const Vector4* impulse) RETAIL(FUN_00262388);
    void Nothing8() RETAIL(FUN_00262390);
    void Contact(const ContactMessage* message, InstanceContext* sender, u32 physical) RETAIL(FUN_00262398);
    u32 CanChangeChunk(struct ChunkData* from, struct ChunkLinkData* link) RETAIL(FUN_002623a0);
    u32 Slot12() RETAIL(FUN_002623a8);
    void Nothing13() RETAIL(FUN_002623b0);
    // The center of its instance's collision box (w the box's top corner's)
    void CollisionCenter(Vector4* center) RETAIL(FUN_00263630);
    void Nothing15() RETAIL(FUN_002623b8);
    void Nothing16() RETAIL(FUN_002623c0);
    void Nothing17() RETAIL(FUN_002623c8);
    void Nothing18() RETAIL(FUN_002623d0);
};
CHECK_OFFSET(Agent, properties, 0xC);
CHECK_OFFSET(Agent, id, 0x16);
CHECK_OFFSET(Agent, contact, 0x20);
CHECK_OFFSET(Agent, vtable, 0x40);
CHECK_SIZE(Agent, 0x60);

// The nodes of the kinds an agent's in (0xC to 0x14: the playable characters', crates', pickups', creatures', generic objects',
// grabbables', pay gates', graples' and projectiles' agents)
struct AgentNode : GameNode
{
    Agent* agent;
};
CHECK_SIZE(AgentNode, 0x1C);

extern "C"
{
    // The agent nodes made: the base's and each kind's (the projectiles' is the base's, given its vtable by the factory)
    AgentNode* ConstructAgentNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017a978);
    AgentNode* ConstructCharacterNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017ac08);
    AgentNode* ConstructCrateNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017ac50);
    AgentNode* ConstructPickupAgentNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017b140);
    AgentNode* ConstructCreatureNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017ad18);
    AgentNode* ConstructGenericObjectNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017aea8);
    AgentNode* ConstructGrabbableNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017ab90);
    AgentNode* ConstructPayGateNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017af70);
    AgentNode* ConstructGrapleNode(AgentNode* node, Agent* agent) RETAIL(FUN_0017ade0);
}

// A trigger's or a camera's node (the base of kinds 7 and 8, 0x170 bytes, still asm; kind 7's has 0x30 bytes more): the time
// between two checks of its box, the instances it tells (their count 0x19 bytes in)
struct TriggerNode : GameNode
{
    u8 unknown18;
    u8 instanceCount;
    // Bits 16-23 of the trigger's bits (2: a camera's node, 4: never polled)
    u8 nodeBits;
    u8 unknown1B;
    // The clock units between two checks of its box
    s32 checkTicks;
    // The kinds of nodes its events go to
    u32 eventKinds;
    u8 unknown24[0xE4 - 0x24];
    InstanceContext* instances[(0x170 - 0xE4) / 4];

    // Made for a trigger of a chunk (still asm), and destroyed (still asm)
    static TriggerNode* Construct(TriggerNode* node, struct ChunkData* chunk, class LayoutTrigger* trigger) RETAIL(FUN_001f6028);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001f60d0);
    void AddInstance(InstanceContext* instance) RETAIL(FUN_001f6298);
};
CHECK_OFFSET(TriggerNode, nodeBits, 0x1A);
CHECK_OFFSET(TriggerNode, instances, 0xE4);
CHECK_SIZE(TriggerNode, 0x170);

// A camera trigger's node (kind 8, 0x180 bytes, retail's vtable CameraNode_Methods: 14 something entered its box): the camera its
// trigger switches to (its own: its destructor destroys it) and the event it sends what enters (made the first time, kept by
// the references to it)
struct CameraNode : TriggerNode
{
    enum NodeBits : u8
    {
        NodeBitCamera = 0x4,
    };

    static constexpr u32 TypeId = 0x1C01;

    struct MainCamera* camera;
    struct CameraEvent* event;
    u8 unknown178[0x180 - 0x178];

    static CameraNode* Construct(CameraNode* node, struct ChunkData* chunk, class CameraTrigger* trigger) RETAIL(InitCameraNode);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0027e440);
    u32 Kind() RETAIL(FUN_0027b7e8);
    u32 Type() RETAIL(FUN_0027b7f0);
    void ForgetEvent() RETAIL(FUN_0027e3c8);
    // Something entered its box: the camera's event sent to it (for the node's instance) and to the instances it tells (for it)
    void Enter(InstanceContext* entering) RETAIL(FUN_0027aa80);
};
CHECK_OFFSET(CameraNode, event, 0x174);
CHECK_SIZE(CameraNode, 0x180);

// The node of kind 0x16: the instance it's in, an object it follows (while playing, the sounds are heard from it), its bits (bit 7:
// the scripts don't set its camera's target) and a camera of its own (still asm, 0x50 bytes in): the camera's target 0xE0 bytes
// in (a FollowCameraTarget, what it follows 0x74 bytes further: the character's instance) and its positioner 0x2F0 bytes in (a
// FollowCameraPositioner)
struct FollowNode
{
    InstanceContext* owner;
    u8 unknown04[0x20 - 0x4];
    Reference* object;
    u8 unknown24[0x30 - 0x24];
    u64 bits;
    u8 unknown38[0x50 - 0x38];
    u8 camera[0xE0];
    u8 target[0x74];
    Reference* cameraTarget;
    u8 unknown1A8[0x340 - 0x1A8];
    u8 positioner[0x400];
};
CHECK_OFFSET(FollowNode, bits, 0x30);
CHECK_OFFSET(FollowNode, target, 0x130);
CHECK_OFFSET(FollowNode, cameraTarget, 0x1A4);
CHECK_OFFSET(FollowNode, positioner, 0x340);

extern "C"
{
    extern const GccVTableEntry g_GameNodeVTable[] RETAIL(GameNode_Methods);
    extern const GccVTableEntry g_InstanceContextVTable[] RETAIL(InstanceContext_methods);
    extern const GccVTableEntry g_CameraNodeVTable[] RETAIL(CameraNode_Methods);

    // The node of a kind
    void* GetGameNode(NodeList* nodes, u32 kind) RETAIL(GetGameNode_);
    // The frame's instances to draw as the view test queued them (instances with a model to draw): wholly in view from the front
    // (as many as the count), crossing the view's sides from the back down (to the end, past the last), and how many in all
    extern InstanceContext* g_DrawnInstances[1024] RETAIL(G_InstanceContextCache_);
    extern s32 g_DrawnInViewCount RETAIL(G_InstanceContextListIndex);
    extern s32 g_DrawnClippedEnd RETAIL(G_InstanceContextListEndIndex);
    extern s32 g_DrawnInstanceCount RETAIL(D_0030AC9C);
    // The instance's model drawn: its place's matrix through the chunk's (VU0's microprogram), the lights' place its own, then
    // its model node
    void DrawInstanceModel(InstanceContext* instance, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode)
        RETAIL(FUN_001fe290);
    // The queued instances drawn (the VU0's standard programs loaded first), each through its chunk's matrix with its chunk's
    // lights: the ones wholly in view unclipped (mode 1), then the others clipped (mode 2)
    void DrawQueuedInstances() RETAIL(FUN_001fe358);
    // An event's handle let go (still asm)
    void ReleaseEvent(Reference** event) RETAIL(FUN_0011e890);
    // The instance's clock (of its chunk's clocks, else the game's)
    TimeClock* GetContextClock(InstanceContext* instance) RETAIL(GetContextClock);
    // The instances to free, the instances of no chunk (stepped by themselves, with a word given to every step), the nodes to free
    // and the nodes taken out of the kind being stepped
    extern InstanceContext* g_InstancesToFree RETAIL(D_00309B94);
    extern InstanceContext* g_GlobalInstances RETAIL(G_InstContext2);
    extern u32 g_GlobalStepWord RETAIL(D_00309B98);
    extern GameNode* g_NodesToFree RETAIL(g_GameNodeList);
    extern GameNode* g_NodesTakenOut[] RETAIL(D_003D3F40);
    extern u32 g_NodesTakenOutCount RETAIL(D_00309BA0);
    // The node kinds in the order they're stepped
    extern u8 g_StepKinds[] RETAIL(D_003D3F28);
    extern u32 g_StepKindCount RETAIL(G_SizeOfByteArrayAt_0x3D3F28_0x18);
    extern const GccVTableEntry g_NodeIteratorVTable[] RETAIL(NodeRelatedStruct_Methods);
    extern const GccVTableEntry g_NodeIteratorBaseVTable[] RETAIL(D_002F6140);
    // The place of a kind in the order they're stepped (-1 none)
    s32 StepIndexOf(u32 kind) RETAIL(FUN_00198b78);
    // The instance put in the list of the instances to free
    void FreeInstance(InstanceContext* instance) RETAIL(FUN_00199a20);
    // The instances with events queued (their handles; the count reset once they're handed out, the handles let go when they're
    // written over)
    extern ReferenceArray g_InstancesWithEvents RETAIL(G_UnkResourceTable);
    // One node to free destroyed, else one instance. Whether there was one
    u32 FreeOne() RETAIL(FUN_00198e38);
    // The instance out of its chunk (its nodes told) and in the list of the instances of no chunk
    void MakeGlobal(u32 unknown, InstanceContext* instance) RETAIL(FUN_00199630);
    // The queued objects told (their vtable's function 5), the queue emptied; the events handed out to their instances, or let
    // go
    void StepQueuedObjects() RETAIL(FUN_00198d50);
    void StepQueuedObjects2() RETAIL(FUN_00198e18);
    void DeliverEvents() RETAIL(FUN_00198bc0);
    void DropEvents() RETAIL(FUN_00198c88);
    // The instances of no chunk stepped (out of their list: put back by their nodes, or let go of when they're done, flag 2,
    // their vtable's function 4), then the events handed out
    void UpdateGlobalInstances() RETAIL(FUN_00199720);
    void UpdateInstances() RETAIL(FUN_00199820);
    // Everything let go: the events, the queued objects, then every node and instance to free (with the word every step gets)
    void FreeAll(u32 stepWord) RETAIL(FUN_00199670);
    // Every instance of no chunk let go (its vtable's function 4), then one thing freed
    void ReleaseGlobalInstances() RETAIL(FUN_001996b0);
    // An instance's queued events handed to its nodes of the kinds each goes to, and let go
    void HandOutEvents(InstanceContext* instance) RETAIL(FUN_00198380);
    // The chunk's side of an instance woken, put to sleep, released and moved in through a link (its place taken through the
    // link's object matrix, then the linked chunk's side) (still asm)
    void ChunkWakeInstance(struct ChunkData* chunk, InstanceContext* instance) RETAIL(FUN_001f2188);
    void ChunkSleepInstance(struct ChunkData* chunk, InstanceContext* instance) RETAIL(FUN_001edaf8);
    void ChunkReleaseInstance(struct ChunkData* chunk, InstanceContext* instance) RETAIL(FUN_001f2240);
    void MoveThroughLink(struct ChunkLinkData* link, InstanceContext* instance) RETAIL(FUN_001f0160);
    void LinkedChunkTakeInstance(struct ChunkLinkData* link, InstanceContext* instance) RETAIL(FUN_001f0130);
    // An instance's places' instances let go of when it's released: whether they let it be (still asm)
    u32 ReleasePlaces(InstancePlaces* places, InstanceContext* instance) RETAIL(FUN_00198938);
    // The instances' IDs (the game's table of them)
    extern struct InstanceIds* g_InstanceIds RETAIL(D_00309B8C);
    // The kinds' step order made (0 to 10), and a kind put in it before or after another (whether that one's there: else, in
    // retail, before the order's start)
    void InitStepKinds() RETAIL(FUN_00199aa0);
    u32 InsertStepKindBefore(u32 kind, u32 before) RETAIL(FUN_00199ae0);
    u32 InsertStepKindAfter(u32 kind, u32 after) RETAIL(FUN_00199b60);
    // Whether there was something to free
    u32 FreeOneAgain() RETAIL(FUN_00199a00);
    // Whether the instance has the other one among its parents (or the other one is none)
    u32 HasParent(InstanceContext* instance, InstanceContext* other) RETAIL(FUN_00198178);
    // The object's place set to a copy of one
    void SetObjectPlace(ReferencedObject* object, const ObjectPlace* place) RETAIL(FUN_001978b0);
    // The instance's chunk told of the instance when its flag 17 is set (still asm the chunk's part)
    void* ChunkNoticeInstance(InstanceContext* instance) RETAIL(FUN_00198138);
    void* ChunkNoticeInstance2(struct ChunkData* chunk, InstanceContext* instance) RETAIL(FUN_001ede20);
    // The object's place's matrix made from its position and rotation, unless its collision has a value at 0x1C (returned
    // instead; still asm the maths)
    void* UpdateObjectMatrix(ReferencedObject* object) RETAIL(FUN_001978e0);
    // The retail list template's copies: a node pushed onto a list's front, taken out of it
    void ListPushFront(void* node, void** head, u32 previous, u32 next) RETAIL(FUN_0019a088);
    void ListRemove(void* node, void** head, u32 previous, u32 next) RETAIL(FUN_0019a0c8);
    void InstanceListPushFront(void* node, void** head, u32 previous, u32 next) RETAIL(FUN_0019a140);
    void NodeListPushFront(void* node, void** head, u32 previous, u32 next) RETAIL(AddGameNodeToList);
    void NodeListRemove(void* node, void** head, u32 previous, u32 next) RETAIL(UnloadNode);
    // A node unregistered from its instance and queued to be destroyed (the game's list of nodes to free)
    u32 RemoveNode(InstanceContext* instance, void* node) RETAIL(FUN_00197c10);
    void FreeNode(void* node) RETAIL(FUN_00199a48);
    // An object's place set to none (still asm)
    u32 ResetObjectPlace(ObjectPlace* place) RETAIL(FUN_0019a370);
    // The follow node's camera reset (still asm)
    void ResetFollowCamera(void* camera) RETAIL(FUN_0015e4a8);
    // A node registered with its instance's chunk, after it's attached to the instance when asked; and the other way round
    // (still asm)
    u32 RegisterNode(InstanceContext* instance, u32 attach, void* node) RETAIL(FUN_00197918);
    u32 UnregisterNode(InstanceContext* instance, u32 detach, void* node) RETAIL(FUN_00197990);
    // An object put in a chunk (still asm)
    void MoveToChunk(struct ChunkData* chunk, ReferencedObject* object) RETAIL(FUN_001ed918);
    // The instance's agent node: the first it has of the kinds 0xD, 0xE, 0xF, 0xC, 0x10, 0x11, 0x14, 0x12 and 0x13
    AgentNode* AgentNodeOf(InstanceContext* instance) RETAIL(FUN_001721a8);
    // The same (a second copy in retail)
    AgentNode* AgentNodeOf2(InstanceContext* instance) RETAIL(FUN_00172280);
    // The agent's script told an event of its object, by the event's index (still asm)
    u32 RunAgentEvent(Agent* agent, u32 event, u32 unknown2, u32 unknown3, u32 unknown4) RETAIL(ExecuteEvent);
    // Where the node of kind 1 brings its instance back to: a copy of the placement (in memory of its own, kept), or back to its
    // own place (still asm)
    void SetComebackPlacement(void* node, const InstancePlacement* placement) RETAIL(FUN_0023d510);
    void ClearComebackPlacement(void* node) RETAIL(FUN_0023d4c0);
}
