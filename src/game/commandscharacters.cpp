#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/characters.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/colour.h"
#include "game/events.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/instancefactory.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/scripttokens.h"
#include "game/sound.h"
#include "game/vehicles.h"

#include <cstddef>
#include <cstdint>

// The commands that work on the playable characters and the agents around them (velocities thrown, the agents' and the player's
// settings, counters, hits on the instances in an instance's hulls, respawn points, links, vehicles, crates), the damage commands'
// parsers of a token, two command classes nothing makes, a character's surface sounds, and this translation unit's start-up

extern "C"
{
    // The velocity commands' work (ApplyVelocity's for each instance its ray hits, ApplyVelocityToSelf's for its own): the
    // velocity the source's agent is launched with in the target's space, the target's properties giving the values. A crate's
    // contents made (CreateCrateContents')
    void ApplyVelocity(const ApplyVelocityToSelfCommand* command, InstanceContext* target, InstanceContext* source)
        RETAIL(FUN_0010c570);
    void CreateCrateContents(const CreateCrateContentsCommand* command, Agent* crate, InstanceContext* instance,
                             BehaviourRunner* runner) RETAIL(FUN_0010d358);
    // The Humiliskate's sounds (its board's node controller, game/nodecontrollers.h) told the surface it skates on and the
    // velocity, a landing's fall, a lean (how far across), and a sound of a slot played where the board character is. Its sounds by
    // slot: 0 to 2 sliding on surfaces, 3 grinding, 4 to 6 leaning, 7 to 9 landing
    void SetCharacterSurface(SkateController* skate, CollisionSurface* surface, const Vector4* velocity);
    void PlayCharacterSurfaceSound(SkateController* skate, f32 fall) RETAIL_N32(PlayCharacterSurfaceSound);
    void PlayCharacterSurfaceSound2(SkateController* skate, f32 lean) RETAIL_N32(PlayCharacterSurfaceSound2);
    void PlayCharacterSound(SkateController* skate, u32 slot, f32 pitch, f32 volume) RETAIL_N32(FUN_001221c8);
    // Whether an instance's character rides a Rollerbrawl, as its driver or a passenger (1 or 0; the conditions 594 and 627)
    f32 RidesRollerbrawl(InstanceContext* instance) RETAIL(FUN_00128c58);

    // This translation unit's start-up: its static initialisation (initialize 1, priority 0xFFFF) and its entry in the static
    // constructors' table
    void InitCharacterCommandsModule(u32 initialize, u32 priority) RETAIL(FUN_0011bdb8);
    void ConstructCharacterCommandsModule() RETAIL(FUN_001230c8);
    // The header's constants the start-up sets, which nothing reads: up (0, 1, 0, 1), 0.01 twice, a black of half alpha, 0 and 45
    // degrees (65536ths); and the thin pyramid's hull (game/projectiles.cpp)
    extern Vector4 g_CommandsUp RETAIL(D_0030AE90);
    extern f32 g_CommandsSmall RETAIL(D_0030A4AC);
    extern f32 g_CommandsSmall2 RETAIL(D_0030A4A8);
    extern u32 g_CommandsShade RETAIL(D_0030A4B0);
    extern u32 g_CommandsZero RETAIL(D_0030A4A0);
    extern s32 g_CommandsAngle45 RETAIL(D_0030A4B8);
    extern CollisionHull g_PyramidHull RETAIL(PyramidHull);

    // The chunk manager, the game's counters (0x1018 bytes into it) read, set and added to
    extern void* G_ChunkManager;
}

EABI_EXPORT(PlayCharacterSurfaceSound, PlayCharacterSurfaceSound);
EABI_EXPORT(PlayCharacterSurfaceSound2, PlayCharacterSurfaceSound2);
EABI_EXPORT(FUN_001221c8, PlayCharacterSound);

// A command class nothing makes (retail's vtable 0x40 bytes past vt_Cmd528_ReduceHitPoints, 0x30 bytes): an offset, a value,
// two designators and a mode its parser reads, and an execution that does nothing
class UnusedOffsetCommand : public ScriptCommand
{
public:
    // Its bits: the mode (bits 0-3: 1 or 2), the designators (4-11 and 12-19), the offset given (21), the second designator
    // past the receivers (22), the keyword 0x1B (23)
    enum Bits : u32
    {
        ModeMask = 0xF,
        FirstShift = 4,
        SecondShift = 12,
        DesignatorMask = 0xFF,
        OffsetGiven = 0x200000,
        SecondNotReceiver = 0x400000,
        Keyword1B = 0x800000,
    };

    u32 unknown0C;
    Vector4 offset;
    u32 unknown20;
    f32 value;
    u32 unknown28;
    u32 bits;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0011d7a8);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(FUN_0010d740);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_0011f650);
    u32 Size() RETAIL(FUN_0011d7c8);
};
CHECK_OFFSET(UnusedOffsetCommand, offset, 0x10);
CHECK_OFFSET(UnusedOffsetCommand, bits, 0x2C);
CHECK_SIZE(UnusedOffsetCommand, 0x30);

// A command class nothing makes (retail's vtable 0x40 bytes past vt_Cmd636_AddAmmo): AddAmmo's code, the amount going to the
// player's gun's bits 9-12 instead (Gun::AddValue9)
class AddToGunValueCommand : public ScriptCommand
{
public:
    s32 amount;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00129c90);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(FUN_00129d40);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_00129cb0);
    void ExecuteOn(GameNode* node) RETAIL(FUN_00129dd0);
    u32 Size() RETAIL(FUN_00129ce0);
};
CHECK_SIZE(AddToGunValueCommand, 0x10);

namespace
{
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
constexpr u32 CharacterNodeKind = 0xC;
constexpr u32 CrateNodeKind = 0xD;
constexpr u32 CreatureNodeKind = 0xF;
// The agents' vtable functions: a contact message, whether it's a playable character, a launch
constexpr u32 AgentContactSlot = 9;
constexpr u32 AgentIsCharacterSlot = 12;
constexpr u32 AgentLaunchSlot = 21;
// The instances' vtable functions woken and put to sleep
constexpr u32 WakeSlot = 2;
constexpr u32 SleepSlot = 3;
// The object nodes' vtable functions: 11 (before its instance is put to sleep), a velocity set (the float first), a designator's
// instance, its sound stopped
constexpr u32 NodeSlot11 = 11;
constexpr u32 SetVelocitySlot = 26;
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 StopSoundSlot = 44;
// A command's execution on a node, and an event's destructor
constexpr u32 ExecuteOnSlot = 4;
constexpr u32 EventDestroySlot = 1;
constexpr u8 NoDesignator = 0xFF;
constexpr u32 NoSlot = 0xFFFF;
constexpr u16 NoSound = 0xFFFF;
constexpr u8 NoInstanceSound = 0xFF;
constexpr f32 Far = Rounded(1e30);
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// A gun's bits 9-12 (game/characters.h's Gun::AddValue9)
constexpr u32 AmountToken = 0x200;
// The agents' counters a byte each (their bytes 0x18 on), at most 255
constexpr s32 CounterMax = 0xFF;
// A counter command's word: the counter (bits 0-15) and whether it's the agent's own (bit 16) rather than the game's
constexpr u32 CounterMask = 0xFFFF;
constexpr u32 OwnCounter = 0x10000;
constexpr u32 AllPriorities = 0xFFFF;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// The place and the chunk of an instance that may be none (retail reads the words at addresses 8 and 0xA0 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

ChunkData* RetailChunkOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, chunk);
    return *reinterpret_cast<ChunkData* const*>(address);
}

// The agent of an instance's node of a kind (no check that it has one)
Agent* AgentOfKind(InstanceContext* instance, u32 kind)
{
    return static_cast<AgentNode*>(GetGameNode(&instance->nodes, kind))->agent;
}

// The properties the agent's arguments read: its source node's when it has one
PropertyHolder* PropertiesOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? GetPropsHolderFromInstanceNode(node->sourceNode) : node->properties;
}

// A query of up to so many instances with the flags (all of them) and without the others
void StartQuery(InstanceRayHit* query, void** results, u16 most, u32 wanted, u32 unwanted)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Far;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits = InstanceRayHit::BitAllWanted;
    query->wantedFlags = wanted;
    query->unwantedFlags = unwanted;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// A counter of an agent's (its bytes 0x18 on)
u8* CounterOf(Agent* agent, u32 counter)
{
    return reinterpret_cast<u8*>(agent) + offsetof(Agent, unknown18) + counter;
}

// The velocity the player's character moves with: its vehicle's, else its own
void CharacterVelocity(CharacterAgent* character, Vector4* velocity)
{
    Vehicle* vehicle = character->vehicle;
    if (vehicle != nullptr)
    {
        vehicle->VelocityVirtual(velocity);
    }
    else
    {
        *velocity = character->velocity;
    }
}

// The position of an instance's place
Vector4 PositionOf(InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    return place->position;
}

// A setting of two bits for a mask: 1 sets it, 2 clears it, 0 and 3 leave it
void ApplySetting(u32* bits, u32 setting, u32 mask)
{
    if (setting == 1)
    {
        *bits |= mask;
    }
    else if (setting == 2)
    {
        *bits &= ~mask;
    }
}

// A bit of a word set to a value (0 or 1)
u32 WithBit(u32 word, u32 mask, u32 shift, u32 on)
{
    return (word & ~mask) | on << shift;
}

// The bits of a mask set or cleared
void SetBit(u32* word, u32 mask, u32 on)
{
    *word = on != 0 ? *word | mask : *word & ~mask;
}

// A field of the progress's bits (4 bits) written
void SetProgressField(GameProgress* progress, u32 shift, u32 value)
{
    progress->bits = (progress->bits & ~(GameProgress::FieldMask << shift)) | (value & GameProgress::FieldMask) << shift;
}

// An instance's attachments node (kind 6): the instances it links (16 at most)
struct AttachmentsNode
{
    u8 unknown00[0x20];
    InstanceContext* linked[16];
};

// The wait start (clock units) of a pickup's node (game/pickups.cpp's PickupObjectNode, 0xC0 bytes in)
s32& PickupWaitStart(GameNode* node)
{
    return *reinterpret_cast<s32*>(reinterpret_cast<u8*>(node) + 0xC0);
}

// The byte of an object node that keeps the instance sound it plays (0xFF none)
u8& InstanceSoundOf(ObjectNode* node)
{
    return node->unknown155[0x158 - offsetof(ObjectNode, unknown155)];
}

// The game controller's chooser of a crate's contents (nothing reads it)
void* CrateChooser()
{
    return reinterpret_cast<u8*>(G_GameController) + 0x5D4;
}
}

void ApplyVelocity(const ApplyVelocityToSelfCommand* command, InstanceContext* target, InstanceContext* source)
{
    constexpr f32 Epsilon = 0x1.a36e2ep-15f;
    PropertyHolder* properties = AgentNodeOf(target)->agent->properties;
    Agent* launched = AgentNodeOf(source)->agent;
    f32 gravity = command->radius.FloatWith(properties);
    f32 x = command->value5.FloatWith(properties);
    f32 y = command->value6.FloatWith(properties);
    f32 z = command->value7.FloatWith(properties);
    Vector4 velocity;
    if (__builtin_fabsf(x) <= Epsilon && __builtin_fabsf(y) <= Epsilon && __builtin_fabsf(z) <= Epsilon)
    {
        velocity.x = command->velX.FloatWith(properties);
        velocity.y = command->velY.FloatWith(properties);
        velocity.z = command->velZ.FloatWith(properties);
        velocity.w = 1.0f;
    }
    else
    {
        // Thrown to a point x, y, z away: up fast enough to rise by y under the gravity, across it in the time that takes. Retail
        // bug: a point level with it (y 0) divides x and z by a time of 0, and one below gets the R5900's root of the negative's size
        f32 up = __builtin_sqrtf((gravity + gravity) * y);
        f32 time = up / gravity;
        velocity.y = up;
        velocity.z = z / time;
        velocity.x = x / time;
        velocity.w = 1.0f;
    }

    CallVirtual<void>(launched, launched->vtable, AgentLaunchSlot, gravity, &velocity, target);
}

u32 ApplyVelocityToSelfCommand::ParseToken(const ScriptToken* token)
{
    constexpr u32 VelocityGiven = 0x1;
    constexpr u32 PointGiven = 0x2;
    constexpr u32 Keyword = 0x8;
    switch (token->kind)
    {
    case 0x6A:
        ParseTaggedValueRecord(token, &radius);
        return 1;
    case 0x49:
        ParseTaggedValueRecord(token, &velX);
        value8.raw |= VelocityGiven;
        return 1;
    case 0x4A:
        ParseTaggedValueRecord(token, &velY);
        value8.raw |= VelocityGiven;
        return 1;
    case 0x4B:
        ParseTaggedValueRecord(token, &velZ);
        value8.raw |= VelocityGiven;
        return 1;
    case 0x4C:
        ParseTaggedValueRecord(token, &value5);
        value8.raw |= PointGiven;
        return 1;
    case 0x4D:
        ParseTaggedValueRecord(token, &value6);
        value8.raw |= PointGiven;
        return 1;
    case 0x4E:
        ParseTaggedValueRecord(token, &value7);
        value8.raw |= PointGiven;
        return 1;
    case 0xFFFF:
        if (token->value != 0xAC)
        {
            return 0;
        }

        value8.raw |= Keyword;
        return 1;
    default:
        return 0;
    }
}

u32 ApplyVelocityCommand::ParseToken(const ScriptToken* token)
{
    constexpr u32 ValuesGiven = 0x4;
    switch (token->kind)
    {
    case 6:
        target = static_cast<s32>(token->value);
        return 1;
    case 0x46:
        ParseTaggedValueRecord(token, &value9);
        value8.raw |= ValuesGiven;
        return 1;
    case 0x47:
        ParseTaggedValueRecord(token, &value10);
        value8.raw |= ValuesGiven;
        return 1;
    case 0x48:
        ParseTaggedValueRecord(token, &value11);
        value8.raw |= ValuesGiven;
        return 1;
    default:
        return reinterpret_cast<ApplyVelocityToSelfCommand*>(this)->ParseToken(token);
    }
}

// Without bits 2 and 3 of value8 the velocity goes to the target's instance; with them, to every instance a ray from the agent's
// instance to the point (velX2, velY2, velZ2) away hits (none when the ray starts inside one)
void ApplyVelocityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 CastsRay = 0x4 | 0x8;
    constexpr u16 MostHit = 0x20;
    constexpr u32 HitKinds = 0x10B000;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    PropertyHolder* properties = PropertiesOf(node);
    // ApplyVelocityToSelf's values lead its own (retail's base class)
    auto* base = reinterpret_cast<const ApplyVelocityToSelfCommand*>(this);
    if ((value8.raw & CastsRay) == 0)
    {
        InstanceContext* other = runner->InstanceOf(target);
        if (other != nullptr)
        {
            ApplyVelocity(base, instance, other);
        }

        return;
    }

    ChunkData* chunk = instance->chunk;
    void* results[MostHit];
    InstanceRayHit query;
    StartQuery(&query, results, MostHit, 0, ReferencedObject::FlagAsleep);
    Vector4 point = PositionOf(instance);
    Vector4 segment[2];
    segment[0] = point;
    f32 x = velX2.FloatWith(properties);
    f32 y = velY2.FloatWith(properties);
    f32 z = velZ2.FloatWith(properties);
    point.x = point.x + x;
    point.y = point.y + y;
    point.z = point.z + z;
    segment[1] = point;
    SkipInQuery(&query, instance);
    if (ChunkInstancesRayCast(chunk, segment, HitKinds, &query, 0) == 0.0f)
    {
        return;
    }

    for (u32 i = 0; i < query.count; i++)
    {
        ApplyVelocity(base, instance, static_cast<InstanceContext*>(results[i]));
    }
}

// Settings of the attacks that reach the agent (a setting of two bits each: 1 on, 2 off): bits 0-1 the spin's (kind 6), 4-5 the
// body slam's (7 or 9), 10-11 walking into it and landing on it (3 and 5, and bit 14 of its part)
void SetObjectFlags587Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 WalkedIntoMask = BasicAgentPart::HitByKind3 | 0x4000 | BasicAgentPart::HitByKind5;
    auto* part = static_cast<BasicAgentPart*>(NodeOf(runner)->agent->part);
    u32 settings = static_cast<u32>(flags.raw);
    ApplySetting(&part->bits, settings & 3, BasicAgentPart::HitByKind6);
    ApplySetting(&part->bits, settings >> 4 & 3, BasicAgentPart::HitByKind7Or9);
    ApplySetting(&part->bits, settings >> 10 & 3, WalkedIntoMask);
}

// The first contents (the low half of value1, 0xFFFF none) come out as many as value2's range says (its bits 0-3 and 4-7), spread
// around above the crate and staggered by 0.05 seconds; the second contents (the high half) instead when there are no first ones
void CreateCrateContents(const CreateCrateContentsCommand* command, Agent*, InstanceContext* instance, BehaviourRunner*)
{
    constexpr u16 NoObject = 0xFFFF;
    constexpr u32 ObjectMask = 0x7FFF;
    constexpr f32 Above = 0.5f;
    constexpr f32 Spread = Rounded(0.8);
    constexpr f32 Drop = Rounded(-0.15);
    constexpr f32 Stagger = Rounded(0.05);
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    u16 first = static_cast<u16>(command->value1);
    u16 second = static_cast<u16>(command->value1 >> 16);
    InstanceFactory* factory = g_InstanceFactory;
    void* chooser = CrateChooser();
    factory->SetFlag2();
    factory->ClearFlag1();
    factory->SetFlag0();
    factory->ClearFlag3();
    factory->creationFlags = 0;
    Vector4 position = PositionOf(instance);
    s32 angles[3] = {0, 0, 0};
    if (second != NoObject && (first == NoObject || CrateGivesSecondContents(chooser) != 0))
    {
        position.y = position.y + Above;
        CreateInstance(factory, chunk, second & ObjectMask, &position, angles);
        return;
    }

    if (first == NoObject)
    {
        return;
    }

    s32 count = CrateContentsCount(chooser, command->value2 & 0xF, command->value2 >> 4 & 0xF);
    f32 delay = 0.0f;
    for (s32 i = 0; i < count; i++)
    {
        Vector4 spot = position;
        spot.y = spot.y + Above;
        delay = delay + Stagger;
        Vector4 velocity;
        velocity.x = RandomSignedTimes(Spread);
        velocity.z = RandomSignedTimes(Spread);
        velocity.y = Drop;
        velocity.w = 1.0f;
        spot.x = spot.x + velocity.x;
        spot.y = spot.y + velocity.y;
        spot.z = spot.z + velocity.z;
        InstanceContext* made = CreateInstance(factory, chunk, first & ObjectMask, &spot, angles);
        auto* node = static_cast<GameNode*>(GetGameNode(&made->nodes, ObjectNodeKind));
        velocity.x = velocity.x + velocity.x;
        velocity.y = velocity.y + velocity.y;
        velocity.z = velocity.z + velocity.z;
        CallVirtual<void>(node, node->vtable, SetVelocitySlot, -1.0f, &velocity);
        PickupWaitStart(node) -= static_cast<s32>(delay * g_ClockUnitsPerSecond);
    }
}

void UnusedOffsetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

// Kinds 0 to 2 the offset (its bit when any isn't 0), 0x38 the value, 6 and 7 the designators (a byte each), the keywords (type 4)
// 3 and 4 the mode, 0x1B its bit
void UnusedOffsetCommand::ParseTokens(const ScriptTokenList* tokens)
{
    constexpr u8 KeywordType = 4;
    constexpr u32 FirstReserved = 0xDE;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->kind)
        {
        case 0:
            x = token->Float();
            break;
        case 1:
            y = token->Float();
            break;
        case 2:
            z = token->Float();
            break;
        case 6:
            bits = (bits & ~(DesignatorMask << FirstShift)) | (token->value & DesignatorMask) << FirstShift;
            break;
        case 7:
            bits = (bits & ~(DesignatorMask << SecondShift)) | (token->value & DesignatorMask) << SecondShift;
            if (FirstReserved <= (bits >> SecondShift & DesignatorMask))
            {
                bits |= SecondNotReceiver;
            }

            break;
        case 0x38:
            value = token->Float();
            break;
        case 0xFFFF:
            if (token->type != KeywordType)
            {
                break;
            }

            if (token->value == 3)
            {
                bits = (bits & ~ModeMask) | 1;
            }
            else if (token->value == 4)
            {
                bits = (bits & ~ModeMask) | 2;
            }
            else if (token->value == 0x1B)
            {
                bits |= Keyword1B;
            }

            break;
        default:
            break;
        }

        reader.Next();
    }

    if (x != 0.0f || y != 0.0f || z != 0.0f)
    {
        offset.x = x;
        offset.w = 1.0f;
        offset.y = y;
        offset.z = z;
        bits |= OffsetGiven;
    }
}

void UnusedOffsetCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 UnusedOffsetCommand::Size()
{
    return sizeof(UnusedOffsetCommand);
}

// A counter (the game's, or the agent's own: CounterMask, OwnCounter) moved by how fast the player closes in: the squared distance
// from the agent's instance to the player less that to where the player's velocity takes it in a second (80 or more: up by it,
// faster the higher the counter is; less: down by what it lacks of 80; moving away: down by it), kept within 0 and 255
void CounterPositionOp579Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Low = 128.0f;
    constexpr f32 Middle = 192.0f;
    constexpr f32 Fast = 80.0f;
    constexpr f32 Slowly = Rounded(0.0125);
    constexpr f32 Faster = Rounded(0.02);
    constexpr f32 Fastest = Rounded(0.04);
    u32 index = counter & CounterMask;
    f32 value;
    if ((counter & OwnCounter) != 0)
    {
        value = static_cast<f32>(*CounterOf(NodeOf(runner)->agent, index));
    }
    else
    {
        value = static_cast<f32>(GameCounter(G_ChunkManager, index));
    }

    f32 closing = 0.0f;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* player = progress->Instance(progress->Field(GameProgress::CharacterShift));
    InstanceContext* instance = NodeOf(runner)->owner;
    if (player != nullptr)
    {
        auto* character = static_cast<CharacterAgent*>(AgentOfKind(player, CharacterNodeKind));
        Vector4 velocity;
        CharacterVelocity(character, &velocity);
        Vector4 from = PositionOf(player);
        Vector4 own = PositionOf(instance);
        Vector4 ahead;
        ahead.x = from.x + velocity.x;
        ahead.y = from.y + velocity.y;
        ahead.z = from.z + velocity.z;
        f32 nowX = from.x - own.x;
        f32 nowY = from.y - own.y;
        f32 nowZ = from.z - own.z;
        f32 thenX = ahead.x - own.x;
        f32 thenY = ahead.y - own.y;
        f32 thenZ = ahead.z - own.z;
        closing = (nowX * nowX + nowY * nowY + nowZ * nowZ) - (thenX * thenX + thenY * thenY + thenZ * thenZ);
    }

    f32 gain;
    f32 loss;
    if (value <= Low)
    {
        gain = Slowly;
        loss = Slowly;
    }
    else if (value <= Middle)
    {
        gain = Faster;
        loss = Slowly;
    }
    else
    {
        gain = Fastest;
        loss = Faster;
    }

    if (Fast < closing)
    {
        value = value + closing * gain;
    }
    else if (0.0f <= closing)
    {
        value = value - (Fast - closing) * loss;
    }
    else
    {
        value = value + closing * Slowly;
    }

    value = ClampFloat(value, 0.0f, 255.0f);
    if ((counter & OwnCounter) != 0)
    {
        *CounterOf(NodeOf(runner)->agent, index) = static_cast<u8>(static_cast<s32>(value));
    }
    else
    {
        SetGameCounter(G_ChunkManager, index, static_cast<s32>(value) & 0xFF);
    }
}

// value1's bit 0: the instances searched for are those of 0x1000 (with bit 1) or 0x1A000 (else 0x1B000) awake and taking
// triggers' signals in each of the agent's instance's hulls, which get a contact message of the kinds what commands.h has as the
// radius gives and 1 damage; with bit 2 they're sent the trigger message (event) instead
void HitInstancesInBoxesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 MostHit = 20;
    constexpr u32 SearchesAll = 0x1;
    constexpr u32 SearchesCharacters = 0x2;
    constexpr u32 SendsMessage = 0x4;
    constexpr u32 EventKinds = 0x2;
    constexpr u8 Damage = 1;
    InstanceContext* instance = NodeOf(runner)->owner;
    s32 hulls = GetHullCount(&instance->collision);
    void* results[MostHit];
    InstanceRayHit query;
    StartQuery(&query, results, MostHit, ReferencedObject::FlagTriggerSignals, ReferencedObject::FlagAsleep);
    u32 kinds;
    if ((flags & SearchesCharacters) != 0)
    {
        kinds = 0x1000;
    }
    else if ((flags & SearchesAll) != 0)
    {
        kinds = 0x1A000;
    }
    else
    {
        kinds = 0x1B000;
    }

    ContactMessage message;
    ContactMessage::Construct(&message);
    message.word = __builtin_bit_cast(u32, radius);
    message.byte = Damage;
    InstanceContext* sender = NodeOf(runner)->owner;
    Reference* handle = sender != nullptr ? AddReference(sender) : nullptr;
    GameEvent* trigger = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))),
                                              static_cast<u16>(event), &handle, EventKinds);
    bool sent = false;
    for (s32 hull = 0; hull < hulls; hull++)
    {
        CollisionHull* shape;
        Matrix4x4 matrix;
        GetInstanceHull(&instance->collision, hull, &shape, &matrix);
        ChunkInstancesInHull(instance->chunk, shape, &matrix, kinds, &query, 0);
        for (s32 i = 0; i < query.count; i++)
        {
            auto* hit = static_cast<InstanceContext*>(results[i]);
            if ((flags & SendsMessage) != 0)
            {
                Reference* eventHandle = trigger != nullptr ? AddEventReference(trigger) : nullptr;
                QueueEvent(hit, &eventHandle);
                sent = true;
            }
            else
            {
                Agent* agent = static_cast<ObjectNodeBase*>(GetGameNode(&hit->nodes, ObjectNodeKind))->agent;
                CallVirtual<void>(agent, agent->vtable, AgentContactSlot, &message, instance, 1);
            }
        }

        query.bits &= ~InstanceRayHit::BitFull;
        query.count = 0;
        query.distance = Far;
        query.instance = nullptr;
    }

    if (!sent && trigger != nullptr)
    {
        CallVirtual<void>(trigger, trigger->vtable, EventDestroySlot, DestroyAndFree);
    }
}

namespace
{
// The slot of the sound of sliding on a surface (by its ID)
u32 SlideSlot(const CollisionSurface* surface)
{
    switch (surface->surfaceId)
    {
    case 8:
        return 2;
    case 12:
    case 17:
        return 1;
    default:
        return 0;
    }
}
}

// The sound of sliding on the surface (or of grinding a rail) stopped when the surface changed or the grinding started, and played
// or kept playing on the board character's node at a volume and a pitch that grow with the speed
void SetCharacterSurface(SkateController* skate, CollisionSurface* surface, const Vector4* velocity)
{
    constexpr u32 GrindingSlot = 3;
    constexpr f32 VolumePerSpeed = Rounded(0.01);
    constexpr f32 Volume = Rounded(0.1);
    constexpr f32 PitchPerSpeed = Rounded(0.06);
    constexpr f32 Pitch = Rounded(0.4);
    constexpr f32 GrindingVolume = 0.75f;
    CollisionSurface* last = skate->surface;
    skate->surface = surface;
    if (surface != last
        || (skate->flags & (SkateController::FlagGrinding | SkateController::FlagWasGrinding)) == SkateController::FlagGrinding)
    {
        ObjectNode* node = skate->node;
        CallVirtual<void>(node, node->vtable, StopSoundSlot);
    }

    bool grinding = (skate->flags & SkateController::FlagGrinding) != 0;
    u32 slot;
    if (grinding)
    {
        slot = GrindingSlot;
    }
    else if (skate->surface != nullptr)
    {
        slot = SlideSlot(skate->surface);
    }
    else
    {
        return;
    }

    f32 speed = __builtin_sqrtf(velocity->x * velocity->x + velocity->y * velocity->y + velocity->z * velocity->z);
    f32 volume = speed * VolumePerSpeed + Volume;
    f32 pitch = speed * PitchPerSpeed + Pitch;
    if (grinding)
    {
        pitch = pitch - Pitch;
        volume = volume + GrindingVolume;
    }

    pitch = ClampFloat(pitch, 0.5f, 3.0f);
    volume = ClampFloat(volume, Rounded(0.05), 1.5f);
    u16 id = skate->sounds[slot];
    if (id == NoSound)
    {
        return;
    }

    ObjectNode* node = skate->node;
    node->owner->place->SyncPosition();
    u8& playing = InstanceSoundOf(node);
    if (playing == NoInstanceSound)
    {
        s32 kind = ListenerVoiceKind();
        playing = static_cast<u8>(PlayInstanceSoundById(volume, pitch, id, 0, node->owner, kind, 0));
    }
    else
    {
        SetInstanceSoundPitch(pitch, playing);
        SetInstanceSoundVolume(volume, playing);
    }
}

// A landing's sound, by the surface the board is on (none without one), its volume and its pitch growing with the fall
void PlayCharacterSurfaceSound(SkateController* skate, f32 fall)
{
    constexpr f32 PitchPerFall = Rounded(0.05);
    constexpr f32 VolumePerFall = Rounded(0.045);
    constexpr f32 Volume = Rounded(0.2);
    u32 slot = NoSlot;
    if (skate->surface != nullptr)
    {
        switch (skate->surface->surfaceId)
        {
        case 7:
        case 9:
            slot = 8;
            break;
        case 8:
            slot = 9;
            break;
        default:
            slot = 7;
            break;
        }
    }

    f32 pitch = ClampFloat(fall * PitchPerFall + 1.0f, 0.5f, 2.0f);
    f32 volume = ClampFloat(fall * VolumePerFall + Volume, Volume, Rounded(1.3));
    if (slot == NoSlot)
    {
        return;
    }

    u16 id = skate->sounds[slot];
    if (id == NoSound)
    {
        return;
    }

    s32 kind = ListenerVoiceKind();
    InstanceContext* instance = skate->node->owner;
    Vector4 position = PositionOf(instance);
    PlaySoundByIdAt(volume, pitch, id, 0, instance->chunk, &position, kind, -1);
}

// A lean's sound, by the surface the board is on (none without one), its volume and its pitch growing with how far across
void PlayCharacterSurfaceSound2(SkateController* skate, f32 lean)
{
    constexpr f32 PitchPerLean = 0.75f;
    constexpr f32 Pitch = Rounded(0.8);
    constexpr f32 VolumePerLean = 0.25f;
    constexpr f32 Volume = Rounded(0.3);
    u32 slot = NoSlot;
    if (skate->surface != nullptr)
    {
        switch (skate->surface->surfaceId)
        {
        case 7:
        case 9:
            slot = 5;
            break;
        case 8:
            slot = 6;
            break;
        default:
            slot = 4;
            break;
        }
    }

    f32 across = __builtin_fabsf(lean);
    f32 pitch = ClampFloat(across * PitchPerLean + Pitch, 0.5f, 2.0f);
    f32 volume = ClampFloat(across * VolumePerLean + Volume, Rounded(0.2), 2.0f);
    PlayCharacterSound(skate, slot, pitch, volume);
}

void PlayCharacterSound(SkateController* skate, u32 slot, f32 pitch, f32 volume)
{
    slot &= 0xFFFF;
    if (slot == NoSlot)
    {
        return;
    }

    u16 id = skate->sounds[slot];
    if (id == NoSound)
    {
        return;
    }

    s32 kind = ListenerVoiceKind();
    InstanceContext* instance = skate->node->owner;
    Vector4 position = PositionOf(instance);
    PlaySoundByIdAt(volume, pitch, id, 0, instance->chunk, &position, kind, -1);
}

// The player's character (the agent's own, else the player's) given settings: inputFlags' low half says which of the 9 are given,
// its high half their values. 0-5 the turn, the moves and the buttons it may use (locked when not), 6 all of them (when not,
// locked unless unknown2's bit 0), 7 its part's resting flag, 8 its being invincible (when not). unknown2's bit 0 makes its
// controls' node driven by its motion, bit 1 its state's bit 15
void SetPlayerInputCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Settings = 9;
    constexpr u32 LockSettings = 6;
    constexpr u32 AllInputs = 6;
    constexpr u32 Resting = 7;
    constexpr u32 MotionDriven = 0x1;
    constexpr u32 BoxOnly = 0x2;
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (node == nullptr)
    {
        // Retail bug: neither the player nor its character node (nor its controls' node below) is checked for none
        instance = PlayerInstance();
        node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    }

    auto* character = static_cast<CharacterAgent*>(node->agent);
    u32* locked = &character->buttons.locked;
    for (u32 setting = 0; setting < Settings; setting++)
    {
        u32 bit = (1u << setting) & 0xFFFF;
        if ((inputFlags & bit) == 0)
        {
            continue;
        }

        u32 on = (inputFlags >> 16 & bit) != 0;
        if (setting < LockSettings)
        {
            SetBit(locked, bit, on ^ 1);
        }
        else if (setting == AllInputs)
        {
            if (on != 0)
            {
                *locked = 0;
            }
            else if ((unknown2 & MotionDriven) == 0)
            {
                *locked = CharacterButtons::LockAll;
            }
        }
        else if (setting == Resting)
        {
            auto* part = static_cast<CharacterPart*>(character->part);
            part->flags = WithBit(part->flags, CreaturePart::FlagResting, 1, on);
        }
        else
        {
            auto* part = static_cast<CharacterPart*>(character->part);
            part->bits = WithBit(part->bits, CharacterPart::Invincible, 10, on ^ 1);
        }
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    SetBit(&controls->bits, ControlsNode::BitMotionDriven, (unknown2 & MotionDriven) != 0);
    SetBit(&character->state, CharacterAgent::StateBoxOnly, (unknown2 & BoxOnly) != 0);
}

// A playable character's instance in another chunk than the player's is put half a unit in front of the load wall of its chunk's
// link to the player's chunk (when that chunk's RM2 is loaded)
void WarpToChunkLinkTowardsPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if (GetGameNode(&instance->nodes, CharacterNodeKind) == nullptr)
    {
        return;
    }

    InstanceContext* player = PlayerInstance();
    ObjectPlace* playerPlace = RetailPlaceOf(player);
    ChunkData* playerChunk = RetailChunkOf(player);
    playerPlace->SyncPosition();
    ChunkData* chunk = instance->chunk;
    if (chunk == playerChunk)
    {
        return;
    }

    ChunkLinkData* link = FindLinkTo(chunk, playerChunk);
    if (link == nullptr || (link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
    {
        return;
    }

    LoadWall* wall = link->loadWall;
    Vector4 normal = wall->plane;
    Vector4 middle;
    WallMiddle(wall->corners, &middle);
    Vector4 position;
    position.x = normal.x * 0.5f + middle.x;
    position.y = normal.y * 0.5f + middle.y;
    position.z = normal.z * 0.5f + middle.z;
    position.w = 1.0f;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&position))
    {
        QueueObject(instance);
    }

    node->unknownB0 = position;
}

// The agent's instance made a checkpoint for the progress's pairing and characters: the start's checkpoint (the last one let go
// unless it's this instance) or, with value1's second byte, the last one (the start's let go and forgotten unless it's this
// instance), saving the game when it's set
void SetPlayerRespawnPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Start = 0;
    constexpr u32 Last = 2;
    GameController* controller = G_GameController;
    GameProgress* progress = &controller->progress;
    ChunkManager* chunks = controller->chunkManager;
    InstanceContext* instance = NodeOf(runner)->owner;
    u32 pairing = progress->Field(GameProgress::PairingShift);
    bool saves = (value1 >> 8 & 0xFF) != 0;
    Checkpoint* checkpoint;
    if (saves)
    {
        progress->checkpoints[Start]->Release(1, chunks, instance);
        checkpoint = progress->checkpoints[Last];
    }
    else
    {
        progress->checkpoints[Last]->Release(0, chunks, instance);
        checkpoint = progress->checkpoints[Start];
    }

    // Pairings 1 and 6 have one character, 2 to 5 two; the others set nothing (the hoverboard's pairing 7 among them)
    bool set = false;
    GameProgress* now = &G_GameController->progress;
    switch (pairing)
    {
    case 1:
    case 6:
        set = checkpoint->Set(pairing, chunks, instance, now->Instance(now->Field(GameProgress::CharacterShift)), nullptr);
        break;
    case 2:
    case 3:
    case 4:
    case 5:
    {
        InstanceContext* character = now->Instance(now->Field(GameProgress::CharacterShift));
        InstanceContext* second = now->Instance(now->Field(GameProgress::SecondShift));
        set = checkpoint->Set(pairing, chunks, instance, character, second);
        break;
    }
    default:
        break;
    }

    if (saves && set)
    {
        G_GameController->Autosave(checkpoint);
    }
}

// The agent's character tied to the character of its focus instance (awake): the focus's leading with value1's bit 0
void LinkToFocusCharacterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 FocusLeads = 0x1;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    auto* own = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, CharacterNodeKind));
    if (own == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(own->agent);
    auto* other = static_cast<AgentNode*>(GetGameNode(&focus->nodes, CharacterNodeKind));
    if (other == nullptr)
    {
        return;
    }

    auto* focusCharacter = static_cast<CharacterAgent*>(other->agent);
    if ((value1 & FocusLeads) != 0)
    {
        focusCharacter->Link(character);
    }
    else
    {
        character->Link(focusCharacter);
    }
}

// A counter (value's CounterMask, OwnCounter) up by 1 when the player's velocity goes across the way from it to the agent's
// instance (to its side: to the left, or to the right with bit 17) faster than 5, down by 1 below 1, kept within 0 and 255 when it's
// the agent's own
void CharacterOp578Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 ToTheRight = 0x20000;
    constexpr f32 Fast = 5.0f;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* player = progress->Instance(progress->Field(GameProgress::CharacterShift));
    InstanceContext* instance = NodeOf(runner)->owner;
    if (player == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(AgentOfKind(player, CharacterNodeKind));
    Vector4 velocity;
    CharacterVelocity(character, &velocity);
    Vector4 from = PositionOf(player);
    Vector4 way = PositionOf(instance);
    way.x = way.x - from.x;
    way.z = way.z - from.z;
    way.y = 0.0f;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    const Vector4* up = &g_YAxis;
    Vector4 side;
    if ((value & ToTheRight) != 0)
    {
        side.y = way.z * up->x - way.x * up->z;
        side.z = way.x * up->y - way.y * up->x;
        side.x = way.y * up->z - way.z * up->y;
    }
    else
    {
        side.y = up->z * way.x - up->x * way.z;
        side.z = up->x * way.y - up->y * way.x;
        side.x = up->y * way.z - up->z * way.y;
    }

    f32 across = side.x * velocity.x + side.y * velocity.y + side.z * velocity.z;
    s32 change = 0;
    if (Fast < across)
    {
        change = 1;
    }
    else if (across < 1.0f)
    {
        change = -1;
    }

    if ((value & OwnCounter) != 0)
    {
        u8* counter = CounterOf(NodeOf(runner)->agent, value & CounterMask);
        s32 total = *counter + change;
        if (total > CounterMax)
        {
            *counter = CounterMax;
        }
        else if (total >= 0)
        {
            *counter = static_cast<u8>(total);
        }
        else
        {
            *counter = 0;
        }
    }
    else
    {
        AddToGameCounter(G_ChunkManager, value & CounterMask, change);
    }
}

void TriggerCharacterEvent12Command::TriggerOn(InstanceContext* instance, BehaviourLevel*)
{
    constexpr u32 Event12 = 12;
    if (instance == nullptr)
    {
        return;
    }

    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (node != nullptr)
    {
        RunAgentEvent(node->agent, Event12, reinterpret_cast<u32>(instance), 1, 0);
    }
}

// Event 12 run on the characters: every one the progress has (bit 0; not character 4's), Crash's (bit 1), Cortex's (bit 2) and
// Mecha-Bandicoot's (bit 3), else the agent's own
void TriggerCharacterEvent12Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    constexpr s32 Every = 0x1;
    constexpr s32 Crash = 0x2;
    constexpr s32 Cortex = 0x4;
    constexpr s32 Mecha = 0x8;
    if ((characters & Every) != 0)
    {
        TriggerOn(G_GameController->progress.Instance(0), level);
        TriggerOn(G_GameController->progress.Instance(1), level);
        TriggerOn(G_GameController->progress.Instance(2), level);
        TriggerOn(G_GameController->progress.Instance(3), level);
        TriggerOn(G_GameController->progress.Instance(5), level);
        return;
    }

    if ((characters & (Crash | Cortex | Mecha)) == 0)
    {
        TriggerOn(NodeOf(runner)->owner, level);
        return;
    }

    if ((characters & Crash) != 0)
    {
        TriggerOn(G_GameController->progress.Instance(0), level);
    }

    if ((characters & Cortex) != 0)
    {
        TriggerOn(G_GameController->progress.Instance(1), level);
    }

    if ((characters & Mecha) != 0)
    {
        TriggerOn(G_GameController->progress.Instance(5), level);
    }
}

// The player switched to a character (value2, 6 none), else to the character of an instance: a designator's (value1's bits 5-12,
// 0xFF none) or, with bit 4, an instance the agent's attachments link (bits 0-3), when its agent is a playable character's
void SwitchCharacterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 DesignatorShift = 5;
    constexpr u32 ByLinked = 0x10;
    constexpr u32 LinkedMask = 0xF;
    if (value2 != static_cast<s32>(GameProgress::NoCharacter))
    {
        G_GameController->SwitchCharacter(value2, 1, 1);
        return;
    }

    InstanceContext* target = nullptr;
    u32 designator = value1 >> DesignatorShift & 0xFF;
    if (designator != NoDesignator)
    {
        GameNode* node = runner->agentNode;
        target = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, designator);
    }
    else if ((value1 & ByLinked) != 0)
    {
        auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, AttachmentsKind));
        if (attachments != nullptr)
        {
            target = attachments->linked[value1 & LinkedMask];
        }
    }

    if (target == nullptr)
    {
        return;
    }

    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&target->nodes, ObjectNodeKind));
    if (node == nullptr)
    {
        return;
    }

    // Retail bug: the agent is checked for none only after its vtable is called
    Agent* agent = node->agent;
    if (CallVirtual<u32>(agent, agent->vtable, AgentIsCharacterSlot) == 0)
    {
        return;
    }

    agent = node->agent;
    if (agent == nullptr)
    {
        return;
    }

    G_GameController->SwitchCharacter(agent->properties->GetInt(0), 1, 1);
}

// The agent's settings: agentFlags' low half says which of the 10 are given, its high half their values. 0 its instance awake
// (asleep when not, its node's slot 11 told first), 1-4 its instance's flags (visible, the sphere contact, triggers' signals, the
// shadow), 5 its creature part's snapping to the ground, 6-9 its part's bits (it may damage the character, it may always (when
// not), bullets bounce back, it's targettable)
void SetAgentCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 Settings = 10;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    for (u32 setting = 0; setting < Settings; setting++)
    {
        u32 bit = (1u << setting) & 0xFFFF;
        if ((agentFlags & bit) == 0)
        {
            continue;
        }

        u32 on = (agentFlags >> 16 & bit) != 0;
        switch (setting)
        {
        case 0:
            if (on != 0)
            {
                CallVirtual<u32>(instance, instance->vtable, WakeSlot);
            }
            else
            {
                CallVirtual<void>(node, node->vtable, NodeSlot11);
                CallVirtual<u32>(instance, instance->vtable, SleepSlot);
            }

            break;
        case 1:
            SetBit(&instance->flags, ReferencedObject::FlagVisible, on);
            break;
        case 2:
            SetBit(&instance->flags, ReferencedObject::FlagSphereContact, on);
            break;
        case 3:
            SetBit(&instance->flags, ReferencedObject::FlagTriggerSignals, on);
            break;
        case 4:
            SetBit(&instance->flags, ReferencedObject::FlagShadow, on);
            break;
        case 5:
        {
            auto* creature = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CreatureNodeKind));
            if (creature == nullptr)
            {
                break;
            }

            auto* part = static_cast<CreaturePart*>(creature->agent->part);
            SetBit(&part->flags, CreaturePart::FlagSnapsToGround, on);
            break;
        }
        case 6:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits = WithBit(part->bits, BasicAgentPart::CanDamageCharacter, 8, on);
            break;
        }
        case 7:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits = WithBit(part->bits, BasicAgentPart::CanAlwaysDamageCharacter, 10, on ^ 1);
            break;
        }
        case 8:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits = WithBit(part->bits, BasicAgentPart::BulletsBounceBack, 12, on);
            break;
        }
        case 9:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits = WithBit(part->bits, BasicAgentPart::Targettable, 9, on);
            break;
        }
        default:
            break;
        }
    }
}

// The area play is in set (value1, -1: the tagged value's), the last area open raised to it (areas past 24 none)
void SetGlobalProgressionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr s32 FromValue = -1;
    constexpr s32 Areas = 25;
    GameProgress* progress = &G_GameController->progress;
    s32 area = static_cast<s32>(value1);
    if (area == FromValue)
    {
        area = value.IntWith(PropertiesOf(NodeOf(runner)));
    }

    // Retail bug: only areas past 24 are refused, the others below -1 are written as their low 5 bits
    if (area >= Areas)
    {
        return;
    }

    progress->bits = (progress->bits & ~(GameProgress::AreaMask << GameProgress::AreaShift))
                     | (static_cast<u32>(area) & GameProgress::AreaMask) << GameProgress::AreaShift;
    if (static_cast<s32>(progress->bits >> GameProgress::OpenShift & GameProgress::AreaMask) < area)
    {
        progress->bits = (progress->bits & ~(GameProgress::AreaMask << GameProgress::OpenShift))
                         | (static_cast<u32>(area) & GameProgress::AreaMask) << GameProgress::OpenShift;
    }
}

// The crates of the stack above the agent's instance (awake ones a ray 0.1 to 3.9 up hits) within half a unit of the lowest one's
// height start falling (their flag 9 set), and the instances around it are told
void TriggerBalancedCrateFallingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 MostFound = 0x20;
    constexpr u32 CrateKinds = 0x2000;
    constexpr u32 Wanted = 0x10;
    constexpr u32 Unwanted = 0x41;
    constexpr f32 Start = Rounded(0.1);
    constexpr f32 Reach = Rounded(3.8);
    constexpr f32 Margin = 0.5f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    ChunkData* chunk = instance->chunk;
    void* results[MostFound];
    InstanceRayHit query;
    StartQuery(&query, results, MostFound, Wanted, Unwanted);
    f32 lowest = Far;
    Vector4 point = PositionOf(instance);
    point.y = point.y + Start;
    Vector4 segment[2];
    segment[0] = point;
    point.y = point.y + Reach;
    segment[1] = point;
    SkipInQuery(&query, instance);
    f32 share = ChunkInstancesRayCast(chunk, segment, CrateKinds, &query, 0);
    if (0.0f <= share && share <= 1.0f)
    {
        u32 count = query.count;
        for (u32 i = 0; i < count; i++)
        {
            f32 height = PositionOf(static_cast<InstanceContext*>(results[i])).y - point.y;
            if (height < lowest)
            {
                lowest = height;
            }
        }

        lowest = lowest + Margin;
        for (u32 i = 0; i < count; i++)
        {
            auto* crate = static_cast<InstanceContext*>(results[i]);
            auto* agent = static_cast<CrateAgent*>(AgentOfKind(crate, CrateNodeKind));
            if (PositionOf(crate).y - point.y < lowest)
            {
                agent->StartFalling();
                crate->flags |= ReferencedObject::FlagInDrawnCell;
            }
        }
    }

    NotifyInstancesWithin(0.0f, node);
}

namespace
{
// The characters of two of the progress's characters (value1's first and second bytes), when both have one
bool CharactersOf(u32 value, CharacterAgent** first, CharacterAgent** second)
{
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* firstInstance = progress->Instance(value & 0xFF);
    InstanceContext* secondInstance = progress->Instance(value >> 8 & 0xFF);
    if (firstInstance == nullptr || secondInstance == nullptr)
    {
        return false;
    }

    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&firstInstance->nodes, CharacterNodeKind));
    auto* secondNode = static_cast<AgentNode*>(GetGameNode(&secondInstance->nodes, CharacterNodeKind));
    if (firstNode == nullptr || secondNode == nullptr)
    {
        return false;
    }

    *first = static_cast<CharacterAgent*>(firstNode->agent);
    *second = static_cast<CharacterAgent*>(secondNode->agent);
    return true;
}

// The progress's pairing and its characters (by their first integer properties) set
void SetPairing(u32 pairing, CharacterAgent* first, CharacterAgent* second)
{
    GameProgress* progress = &G_GameController->progress;
    SetProgressField(progress, GameProgress::PairingShift, pairing);
    SetProgressField(progress, GameProgress::CharacterShift, static_cast<u32>(first->properties->GetInt(0)));
    SetProgressField(progress, GameProgress::SecondShift, static_cast<u32>(second->properties->GetInt(0)));
}
}

// Two of the progress's characters (value1's bytes) on the Rollerbrawl, the first driving
void SetVehicleRollerbrawlCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    constexpr u32 RollerbrawlPairing = 3;
    CharacterAgent* first;
    CharacterAgent* second;
    if (!CharactersOf(value1, &first, &second))
    {
        return;
    }

    first->SetVehicle(Vehicle::KindRollerbrawl, second, 0);
    SetPairing(RollerbrawlPairing, first, second);
}

// Two of the progress's characters (value1's bytes) on the Humiliskate, the first skating on the second, its top speed value2 and
// its crouched speed value3
void SetVehicleHumiliskateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 HumiliskatePairing = 2;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* firstInstance = progress->Instance(value1 & 0xFF);
    InstanceContext* secondInstance = progress->Instance(value1 >> 8 & 0xFF);
    PropertyHolder* properties = PropertiesOf(NodeOf(runner));
    f32 topSpeed = value2.FloatWith(properties);
    f32 crouchedSpeed = value3.FloatWith(properties);
    // Retail bug: unlike the Rollerbrawl's, the characters' instances aren't checked for none
    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&firstInstance->nodes, CharacterNodeKind));
    auto* secondNode = static_cast<AgentNode*>(GetGameNode(&secondInstance->nodes, CharacterNodeKind));
    if (firstNode == nullptr || secondNode == nullptr)
    {
        return;
    }

    auto* first = static_cast<CharacterAgent*>(firstNode->agent);
    auto* second = static_cast<CharacterAgent*>(secondNode->agent);
    first->SetVehicle(Vehicle::KindHumiliskate, second, 0);
    SetPairing(HumiliskatePairing, first, second);
    auto* board = static_cast<HumiliskateVehicle*>(first->vehicle);
    board->crouchedSpeed = crouchedSpeed;
    board->topSpeed = topSpeed;
}

// The character of a designator's instance (value1's low byte) on the hoverboard that is the agent's instance (value1's bit 8 the
// hoverboard's controls, and pairing 7 rather than 6), alone
void SetVehicleHoverboardCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OwnControls = 0x100;
    InstanceContext* board = NodeOf(runner)->owner;
    InstanceContext* rider = runner->InstanceOf(value1 & 0xFF);
    if (rider == nullptr || board == nullptr)
    {
        return;
    }

    auto* riderNode = static_cast<AgentNode*>(GetGameNode(&rider->nodes, CharacterNodeKind));
    AgentNode* boardNode = AgentNodeOf(board);
    if (riderNode == nullptr || boardNode == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(riderNode->agent);
    GameProgress* progress = &G_GameController->progress;
    character->SetVehicle(Vehicle::KindHoverboard, boardNode->agent, value1 >> 8 & 1);
    SetProgressField(progress, GameProgress::PairingShift, (value1 & OwnControls) != 0 ? 7 : 6);
    u32 played = static_cast<u32>(character->properties->GetInt(0));
    progress->bits = (progress->bits & ~(GameProgress::FieldMask << GameProgress::CharacterShift)
                      & ~(GameProgress::FieldMask << GameProgress::SecondShift))
                     | (played & GameProgress::FieldMask) << GameProgress::CharacterShift
                     | GameProgress::NoCharacter << GameProgress::SecondShift;
}

void CreateDamageCommand::ParseToken(const ScriptToken* token)
{
    constexpr u32 ShapeMask = 0xF;
    constexpr u32 Cylinder = 3;
    constexpr u32 HullMask = 0xF0;
    switch (token->kind)
    {
    case 0xCD:
        ParseTaggedValueRecord(token, &reach);
        break;
    case 0xA9:
        shape = (shape & ~ShapeMask) | Cylinder;
        ParseTaggedValueRecord(token, &height);
        break;
    case 0x236:
        if (token->value == 0x216)
        {
            shape &= ~HullMask;
        }

        break;
    default:
        ParseBaseToken(token);
        break;
    }
}

void CreateDamageCommand::ParseBaseToken(const ScriptToken* token)
{
    constexpr u32 JointMask = 0xFF;
    switch (token->kind)
    {
    case 0x82:
        flags |= Offset;
        x = token->Float();
        break;
    case 0x83:
        flags |= Offset;
        y = token->Float();
        break;
    case 0x84:
        flags |= Offset;
        z = token->Float();
        break;
    case 0x12:
        flags = (flags & ~(JointMask << JointShift)) | (token->value & JointMask) << JointShift;
        break;
    case 0x204:
        ParseTaggedValueRecord(token, &damage);
        break;
    case 0x94:
        flags |= GivesW;
        ParseTaggedValueRecord(token, &messageW);
        break;
    case 0x217:
        flags = (flags & ~Bit10Kind) | static_cast<u32>(token->value == 0) << 1;
        break;
    case 0xFFFF:
        switch (token->value)
        {
        case 0xDF:
            flags |= NearestOnly;
            break;
        case 0x206:
            flags |= SearchKinds5E;
            break;
        case 0x245:
            flags |= SearchKinds1000;
            break;
        case 0x21B:
            hitKinds |= 0x2;
            break;
        case 0x21C:
            hitKinds |= 0x4;
            break;
        case 0x21D:
            hitKinds |= 0x8;
            break;
        case 0x21E:
            hitKinds |= 0x10;
            break;
        case 0x21F:
            hitKinds |= 0x20;
            break;
        case 0x220:
            hitKinds |= 0x40;
            break;
        case 0x221:
            hitKinds |= 0x80;
            break;
        case 0x222:
            hitKinds |= 0x100;
            break;
        case 0x223:
            hitKinds |= 0x400;
            break;
        case 0x224:
            hitKinds |= 0x800;
            break;
        case 0x225:
            hitKinds |= 0x1000;
            break;
        case 0x226:
            hitKinds |= 0x2000;
            break;
        case 0x227:
            hitKinds |= 0x4000;
            break;
        case 0x228:
            hitKinds |= 0x8000;
            break;
        case 0x229:
            hitKinds |= 0x10000;
            break;
        case 0x22A:
            hitKinds |= 0x20000;
            break;
        case 0x22B:
            hitKinds |= 0x40000;
            break;
        case 0x22C:
            hitKinds |= 0x80000;
            break;
        case 0x22D:
            hitKinds |= 0x200000;
            break;
        case 0x22E:
            hitKinds |= 0x400000;
            break;
        case 0x22F:
            hitKinds |= 0x800000;
            break;
        case 0x236:
            hitKinds |= 0x1000000;
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }
}

void CreateDamageCommand::BaseDestroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

// The base ends where CreateDamage's own values start
u32 CreateDamageCommand::BaseSize()
{
    return offsetof(CreateDamageCommand, shape);
}

void AddToGunValueCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddToGunValueCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->kind == AmountToken)
        {
            amount = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void AddToGunValueCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

void AddToGunValueCommand::ExecuteOn(GameNode*)
{
    Gun* gun = reinterpret_cast<CharacterAgent*>(g_PlayerCharacter)->gun;
    if (gun != nullptr)
    {
        gun->AddValue9(amount);
    }
}

u32 AddToGunValueCommand::Size()
{
    return sizeof(AddToGunValueCommand);
}

f32 RidesRollerbrawl(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
    if (node == nullptr)
    {
        return 0.0f;
    }

    bool rides = false;
    Vehicle* vehicle = static_cast<CharacterAgent*>(node->agent)->vehicle;
    if (vehicle != nullptr)
    {
        if (vehicle->Kind() == Vehicle::KindRollerbrawl)
        {
            rides = true;
        }
        else if (vehicle->Kind() == Vehicle::KindPassenger)
        {
            auto* driverNode = static_cast<AgentNode*>(GetGameNode(&vehicle->other->instance->nodes, CharacterNodeKind));
            if (driverNode != nullptr)
            {
                Vehicle* driven = static_cast<CharacterAgent*>(driverNode->agent)->vehicle;
                if (driven != nullptr)
                {
                    rides = driven->Kind() == Vehicle::KindRollerbrawl;
                }
            }
        }
    }

    return rides ? 1.0f : 0.0f;
}

void InitCharacterCommandsModule(u32 initialize, u32 priority)
{
    constexpr f32 Small = Rounded(0.01);
    if (priority != AllPriorities || initialize == 0)
    {
        return;
    }

    g_CommandsUp.w = 1.0f;
    g_CommandsUp.x = 0.0f;
    g_CommandsSmall = Small;
    g_CommandsZero = 0;
    g_CommandsUp.y = 1.0f;
    g_CommandsUp.z = 0.0f;
    g_CommandsSmall2 = Small;
    ColourSet(&g_CommandsShade, 0.0f, 0.0f, 0.0f, 0.5f);
    AngleFrom(&g_CommandsAngle45, 0x1.921fb6p-1f, AngleRadians);
    // (48 objects of a header with nothing to construct follow in retail: an empty loop)
    HullConstruct(&g_PyramidHull);
}

void ConstructCharacterCommandsModule()
{
    InitCharacterCommandsModule(1, AllPriorities);
}
