#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/attachments.h"
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
#include "game/objects.h"
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
    // The header's constants the start-up sets, which nothing reads: up (0, 1, 0, 1), the UI's shadow offset and colour, 0 and 45
    // degrees (65536ths); and the thin pyramid's hull (game/projectiles.cpp)
    extern Vector4 g_CommandsUp RETAIL(D_0030AE90);
    extern f32 g_CommandsShadowY RETAIL(D_0030A4AC);
    extern f32 g_CommandsShadowX RETAIL(D_0030A4A8);
    extern u32 g_CommandsShadowColour RETAIL(D_0030A4B0);
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
    // The mode (1 or 2), the designators, the offset given, the second designator past the receivers, the keyword 0x1B
    union Bits
    {
        u32 value;
        struct
        {
            u32 mode : 4;
            u32 first : 8;
            u32 second : 8;
            u32 unused20 : 1;
            u32 offsetGiven : 1;
            u32 secondNotReceiver : 1;
            u32 keyword1B : 1;
            u32 unused24 : 8;
        };
    };

    u32 unused0C;
    Vector4 offset;
    u32 unused20;
    f32 value;
    u32 unused28;
    Bits bits;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0011d7a8);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(FUN_0010d740);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_0011f650);
    u32 Size() RETAIL(FUN_0011d7c8);
};
CHECK_OFFSET(UnusedOffsetCommand, offset, 0x10);
CHECK_OFFSET(UnusedOffsetCommand, bits, 0x2C);
CHECK_SIZE(UnusedOffsetCommand, 0x30);

// A command class nothing makes (retail's vtable 0x40 bytes past vt_Cmd636_AddAmmo): AddAmmo's code, the amount going to the
// player's gun's second count instead (Gun::AddSecondCount)
class AddToGunSecondCountCommand : public ScriptCommand
{
public:
    s32 amount;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00129c90);
    void ParseTokens(const ScriptTokenList* tokens) RETAIL(FUN_00129d40);
    void Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel* level) RETAIL(FUN_00129cb0);
    void ExecuteOn(GameNode* node) RETAIL(FUN_00129dd0);
    u32 Size() RETAIL(FUN_00129ce0);
};
CHECK_SIZE(AddToGunSecondCountCommand, 0x10);

namespace
{
// A skate controller's sound of none
constexpr u32 NoSkateSound = 0xFFFF;
// A gun's second count (game/characters.h's Gun::AddSecondCount)
constexpr u32 AmountToken = 0x200;
// The board's sounds by slot: sliding (on water or ice, on wood), grinding, leaning and landing (on metal, on wood)
enum BoardSound : u32
{
    SoundSlide = 0,
    SoundSlideSlippery = 1,
    SoundSlideWood = 2,
    SoundGrind = 3,
    SoundLean = 4,
    SoundLeanMetal = 5,
    SoundLeanWood = 6,
    SoundLand = 7,
    SoundLandMetal = 8,
    SoundLandWood = 9,
};
// The instances the queries take, by the kinds of nodes they have: the playable characters and the other agents a hit or a throw
// reaches
constexpr u32 CharacterKinds = 1u << NodeCharacter;
constexpr u32 ObjectKinds = 1u << NodeCrate | 1u << NodeCreature | 1u << NodeGenericObject;

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
void StartQuery(InstanceQuery* query, void** results, u16 most, u32 wanted, u32 unwanted)
{
    query->results = results;
    query->count = 0;
    query->most = most;
    query->distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query->bits.value = InstanceQueryBits::AllWanted;
    query->wantedFlags = wanted;
    query->unwantedFlags = unwanted;
    query->skipped[0] = nullptr;
    query->skipped[1] = nullptr;
    query->instance = nullptr;
}

// A counter of an agent's (its bytes 0x18 on)
u8* CounterOf(Agent* agent, u32 counter)
{
    return reinterpret_cast<u8*>(agent) + offsetof(Agent, counters) + counter;
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

// A setting of two bits applied: on sets the bits, off clears them, 0 and 3 leave them
template <typename Set>
void ApplySetting(u32 setting, Set set)
{
    if (setting == SetAttacksTakenCommand::SettingOn)
    {
        set(1);
    }
    else if (setting == SetAttacksTakenCommand::SettingOff)
    {
        set(0);
    }
}

// The bits of a mask set or cleared
void SetBit(u32* word, u32 mask, u32 on)
{
    *word = on != 0 ? *word | mask : *word & ~mask;
}

// The byte of an object node that keeps the instance sound it plays (0xFF none)
u8& InstanceSoundOf(ObjectNode* node)
{
    return node->playingSound;
}

// The game controller's chooser of a crate's contents (nothing reads it)
void* CrateChooser()
{
    return reinterpret_cast<u8*>(G_GameController) + 0x5D4;
}
}

void ApplyVelocity(const ApplyVelocityToSelfCommand* command, InstanceContext* target, InstanceContext* source)
{
    PropertyHolder* properties = AgentNodeOf(target)->agent->properties;
    Agent* launched = AgentNodeOf(source)->agent;
    f32 gravity = command->gravity.FloatWith(properties);
    f32 x = command->pointX.FloatWith(properties);
    f32 y = command->pointY.FloatWith(properties);
    f32 z = command->pointZ.FloatWith(properties);
    Vector4 velocity;
    if (__builtin_fabsf(x) <= Epsilon && __builtin_fabsf(y) <= Epsilon && __builtin_fabsf(z) <= Epsilon)
    {
        velocity.x = command->velocityX.FloatWith(properties);
        velocity.y = command->velocityY.FloatWith(properties);
        velocity.z = command->velocityZ.FloatWith(properties);
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

    CallVirtual<void>(launched, launched->vtable, Agent::LaunchSlot, gravity, &velocity, target);
}

u32 ApplyVelocityToSelfCommand::ParseToken(const ScriptToken* token)
{
    switch (token->tag)
    {
    case 0x6A:
        ParseTaggedValueRecord(token, &gravity);
        return 1;
    case 0x49:
        ParseTaggedValueRecord(token, &velocityX);
        given.unused0 = 1;
        return 1;
    case 0x4A:
        ParseTaggedValueRecord(token, &velocityY);
        given.unused0 = 1;
        return 1;
    case 0x4B:
        ParseTaggedValueRecord(token, &velocityZ);
        given.unused0 = 1;
        return 1;
    case 0x4C:
        ParseTaggedValueRecord(token, &pointX);
        given.unused1 = 1;
        return 1;
    case 0x4D:
        ParseTaggedValueRecord(token, &pointY);
        given.unused1 = 1;
        return 1;
    case 0x4E:
        ParseTaggedValueRecord(token, &pointZ);
        given.unused1 = 1;
        return 1;
    case TagNone:
        if (token->value != 0xAC)
        {
            return 0;
        }

        given.castsRay = 1;
        return 1;
    default:
        return 0;
    }
}

u32 ApplyVelocityCommand::ParseToken(const ScriptToken* token)
{
    switch (token->tag)
    {
    case 6:
        receiver = static_cast<s32>(token->value);
        return 1;
    case 0x46:
        ParseTaggedValueRecord(token, &unused2C);
        given.rayValues = 1;
        return 1;
    case 0x47:
        ParseTaggedValueRecord(token, &unused30);
        given.rayValues = 1;
        return 1;
    case 0x48:
        ParseTaggedValueRecord(token, &unused34);
        given.rayValues = 1;
        return 1;
    default:
        return reinterpret_cast<ApplyVelocityToSelfCommand*>(this)->ParseToken(token);
    }
}

// Without the ray the velocity goes to the receiver's instance; with it, to every instance a ray from the agent's instance to the
// point away hits (none when the ray starts inside one)
void ApplyVelocityCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 MostHit = 0x20;
    constexpr u32 HitKinds = CharacterKinds | 1u << NodeCrate | 1u << NodeCreature | 1u << NodeProjectile;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    PropertyHolder* properties = PropertiesOf(node);
    // ApplyVelocityToSelf's values lead its own (retail's base class)
    auto* base = reinterpret_cast<const ApplyVelocityToSelfCommand*>(this);
    if (!given.rayValues && !given.castsRay)
    {
        InstanceContext* other = runner->InstanceOf(receiver);
        if (other != nullptr)
        {
            ApplyVelocity(base, instance, other);
        }

        return;
    }

    ChunkData* chunk = instance->chunk;
    void* results[MostHit];
    InstanceQuery query;
    StartQuery(&query, results, MostHit, 0, ReferencedObjectFlags::Asleep);
    Vector4 point = PositionOf(instance);
    Vector4 segment[2];
    segment[0] = point;
    f32 x = pointX.FloatWith(properties);
    f32 y = pointY.FloatWith(properties);
    f32 z = pointZ.FloatWith(properties);
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

// Settings of the attacks that reach the agent: the spin's, the body slam's (and the tied characters'), walking into it and hitting
// it from below (and bit 14 of its part)
void SetAttacksTakenCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* part = static_cast<BasicAgentPart*>(NodeOf(runner)->agent->part);
    Settings given = settings;
    ApplySetting(given.spin, [part](u32 on) { part->bits.hitBySpin = on; });
    ApplySetting(given.slam, [part](u32 on) { part->bits.hitBySlamOrTied = on; });
    ApplySetting(given.walkInto, [part](u32 on) {
        part->bits.hitByWalkInto = on;
        part->bits.unused14 = on;
        part->bits.hitFromBelow = on;
    });
}

// The first contents come out as many as the count's range says, spread around above the crate and staggered by 0.05 seconds; the
// second contents instead when there are no first ones
void CreateCrateContents(const CreateCrateContentsCommand* command, Agent*, InstanceContext* instance, BehaviourRunner*)
{
    constexpr f32 Above = 0.5f;
    constexpr f32 Spread = Rounded(0.8);
    constexpr f32 Drop = Rounded(-0.15);
    constexpr f32 Stagger = Rounded(0.05);
    ChunkEntry* chunk = ChunkOfInstance(G_ChunkManager, instance);
    u16 first = command->contents.first;
    u16 second = command->contents.second;
    InstanceFactory* factory = g_InstanceFactory;
    void* chooser = CrateChooser();
    factory->SetInstanceProperties();
    factory->ClearUnused1();
    factory->SetGivesIds();
    factory->ClearGivesFlagSlots();
    factory->creationFlags = 0;
    Vector4 position = PositionOf(instance);
    s32 angles[3] = {0, 0, 0};
    if (second != NoObjectId && (first == NoObjectId || CrateGivesSecondContents(chooser) != 0))
    {
        position.y = position.y + Above;
        CreateInstance(factory, chunk, second & ResourceIndexMask, &position, angles);
        return;
    }

    if (first == NoObjectId)
    {
        return;
    }

    s32 count = CrateContentsCount(chooser, command->count.least, command->count.most);
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
        InstanceContext* made = CreateInstance(factory, chunk, first & ResourceIndexMask, &spot, angles);
        auto* node = static_cast<GameNode*>(GetGameNode(&made->nodes, NodeObject));
        velocity.x = velocity.x + velocity.x;
        velocity.y = velocity.y + velocity.y;
        velocity.z = velocity.z + velocity.z;
        CallVirtual<void>(node, node->vtable, ObjectNode::LaunchSlot, -1.0f, &velocity);
        static_cast<PickupObjectNode*>(node)->waitStart -= static_cast<s32>(delay * g_ClockUnitsPerSecond);
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
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        switch (token->tag)
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
            bits.first = token->value;
            break;
        case 7:
            bits.second = token->value;
            if (FirstDesignator <= bits.second)
            {
                bits.secondNotReceiver = 1;
            }

            break;
        case 0x38:
            value = token->Float();
            break;
        case TagNone:
            if (token->type != TokenKeyword)
            {
                break;
            }

            if (token->value == 3)
            {
                bits.mode = 1;
            }
            else if (token->value == 4)
            {
                bits.mode = 2;
            }
            else if (token->value == 0x1B)
            {
                bits.keyword1B = 1;
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
        bits.offsetGiven = 1;
    }
}

void UnusedOffsetCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 UnusedOffsetCommand::Size()
{
    return sizeof(UnusedOffsetCommand);
}

// A counter (the game's, or the agent's own) moved by how fast the player closes in: the squared distance from the agent's instance
// to the player less that to where the player's velocity takes it in a second (80 or more: up by it, faster the higher the
// counter is; less: down by what it lacks of 80; moving away: down by it), kept within 0 and 255
void CountPlayerApproachCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Low = 128.0f;
    constexpr f32 Middle = 192.0f;
    constexpr f32 Fast = 80.0f;
    constexpr f32 Slowly = Rounded(0.0125);
    constexpr f32 Faster = Rounded(0.02);
    constexpr f32 Fastest = Rounded(0.04);
    constexpr f32 Most = 255.0f;
    u32 index = counter.counter;
    f32 count;
    if (counter.agentCounter)
    {
        count = static_cast<f32>(*CounterOf(NodeOf(runner)->agent, index));
    }
    else
    {
        count = static_cast<f32>(GameCounter(G_ChunkManager, index));
    }

    f32 closing = 0.0f;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* player = progress->Instance(progress->play.character);
    InstanceContext* instance = NodeOf(runner)->owner;
    if (player != nullptr)
    {
        auto* character = static_cast<CharacterAgent*>(AgentOfKind(player, NodeCharacter));
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
    if (count <= Low)
    {
        gain = Slowly;
        loss = Slowly;
    }
    else if (count <= Middle)
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
        count = count + closing * gain;
    }
    else if (0.0f <= closing)
    {
        count = count - (Fast - closing) * loss;
    }
    else
    {
        count = count + closing * Slowly;
    }

    count = ClampFloat(count, 0.0f, Most);
    if (counter.agentCounter)
    {
        *CounterOf(NodeOf(runner)->agent, index) = static_cast<u8>(static_cast<s32>(count));
    }
    else
    {
        SetGameCounter(G_ChunkManager, index, static_cast<s32>(count) & 0xFF);
    }
}

// The instances awake and taking triggers' signals in each of the agent's instance's hulls (the playable characters, the others or
// both) get a contact message of the kinds of hit and 1 damage, or are sent the trigger message instead
void HitInstancesInBoxesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 MostHit = 20;
    constexpr u8 Damage = 1;
    InstanceContext* instance = NodeOf(runner)->owner;
    s32 hulls = GetHullCount(&instance->collision);
    void* results[MostHit];
    InstanceQuery query;
    StartQuery(&query, results, MostHit, ReferencedObjectFlags::ReceivesTriggerSignals, ReferencedObjectFlags::Asleep);
    u32 kinds;
    if (flags.onlyCharacters)
    {
        kinds = CharacterKinds;
    }
    else if (flags.skipsCharacters)
    {
        kinds = ObjectKinds;
    }
    else
    {
        kinds = CharacterKinds | ObjectKinds;
    }

    ContactMessage contact;
    ContactMessage::Construct(&contact);
    contact.hitKinds = hitKinds;
    contact.damage = Damage;
    InstanceContext* sender = NodeOf(runner)->owner;
    Reference* handle = sender != nullptr ? AddReference(sender) : nullptr;
    GameEvent* trigger = GameEvent::Construct(static_cast<GameEvent*>(MemoryAllocate(sizeof(GameEvent))),
                                              static_cast<u16>(message.id), &handle, ObjectNodeKinds);
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
            if (flags.sendsMessage)
            {
                Reference* eventHandle = trigger != nullptr ? AddEventReference(trigger) : nullptr;
                QueueEvent(hit, &eventHandle);
                sent = true;
            }
            else
            {
                Agent* agent = static_cast<ObjectNodeBase*>(GetGameNode(&hit->nodes, NodeObject))->agent;
                CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &contact, instance, 1);
            }
        }

        query.bits.unused0 = 0;
        query.count = 0;
        query.distance = Infinite;
        query.instance = nullptr;
    }

    if (!sent && trigger != nullptr)
    {
        CallVirtual<void>(trigger, trigger->vtable, GameEvent::DestroySlot, DestroyAndFree);
    }
}

namespace
{
// The slot of the sound of sliding on a surface (by its ID)
u32 SlideSlot(const CollisionSurface* surface)
{
    switch (surface->surfaceId)
    {
    case SurfaceWood:
        return SoundSlideWood;
    case SurfaceWater:
    case SurfaceIce:
        return SoundSlideSlippery;
    default:
        return SoundSlide;
    }
}
}

// The sound of sliding on the surface (or of grinding a rail) stopped when the surface changed or the grinding started, and played
// or kept playing on the board character's node at a volume and a pitch that grow with the speed
void SetCharacterSurface(SkateController* skate, CollisionSurface* surface, const Vector4* velocity)
{
    constexpr f32 VolumePerSpeed = Rounded(0.01);
    constexpr f32 Volume = Rounded(0.1);
    constexpr f32 PitchPerSpeed = Rounded(0.06);
    constexpr f32 Pitch = Rounded(0.4);
    constexpr f32 GrindingVolume = 0.75f;
    constexpr f32 LeastPitch = 0.5f;
    constexpr f32 MostPitch = 3.0f;
    constexpr f32 LeastVolume = Rounded(0.05);
    constexpr f32 MostVolume = 1.5f;
    CollisionSurface* last = skate->surface;
    skate->surface = surface;
    if (surface != last || (skate->flags.grinding && !skate->flags.wasGrinding))
    {
        ObjectNode* node = skate->node;
        CallVirtual<void>(node, node->vtable, ObjectNode::StopSoundSlot);
    }

    bool grinding = skate->flags.grinding;
    u32 slot;
    if (grinding)
    {
        slot = SoundGrind;
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

    pitch = ClampFloat(pitch, LeastPitch, MostPitch);
    volume = ClampFloat(volume, LeastVolume, MostVolume);
    u16 id = skate->sounds[slot];
    if (id == NoSoundId)
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
    constexpr f32 LeastPitch = 0.5f;
    constexpr f32 MostPitch = 2.0f;
    constexpr f32 MostVolume = Rounded(1.3);
    u32 slot = NoSkateSound;
    if (skate->surface != nullptr)
    {
        switch (skate->surface->surfaceId)
        {
        case SurfaceSlippyMetal:
        case SurfaceMetal:
            slot = SoundLandMetal;
            break;
        case SurfaceWood:
            slot = SoundLandWood;
            break;
        default:
            slot = SoundLand;
            break;
        }
    }

    f32 pitch = ClampFloat(fall * PitchPerFall + 1.0f, LeastPitch, MostPitch);
    f32 volume = ClampFloat(fall * VolumePerFall + Volume, Volume, MostVolume);
    if (slot == NoSkateSound)
    {
        return;
    }

    u16 id = skate->sounds[slot];
    if (id == NoSoundId)
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
    constexpr f32 LeastPitch = 0.5f;
    constexpr f32 MostPitch = 2.0f;
    constexpr f32 LeastVolume = Rounded(0.2);
    constexpr f32 MostVolume = 2.0f;
    u32 slot = NoSkateSound;
    if (skate->surface != nullptr)
    {
        switch (skate->surface->surfaceId)
        {
        case SurfaceSlippyMetal:
        case SurfaceMetal:
            slot = SoundLeanMetal;
            break;
        case SurfaceWood:
            slot = SoundLeanWood;
            break;
        default:
            slot = SoundLean;
            break;
        }
    }

    f32 across = __builtin_fabsf(lean);
    f32 pitch = ClampFloat(across * PitchPerLean + Pitch, LeastPitch, MostPitch);
    f32 volume = ClampFloat(across * VolumePerLean + Volume, LeastVolume, MostVolume);
    PlayCharacterSound(skate, slot, pitch, volume);
}

void PlayCharacterSound(SkateController* skate, u32 slot, f32 pitch, f32 volume)
{
    slot &= 0xFFFF;
    if (slot == NoSkateSound)
    {
        return;
    }

    u16 id = skate->sounds[slot];
    if (id == NoSoundId)
    {
        return;
    }

    s32 kind = ListenerVoiceKind();
    InstanceContext* instance = skate->node->owner;
    Vector4 position = PositionOf(instance);
    PlaySoundByIdAt(volume, pitch, id, 0, instance->chunk, &position, kind, -1);
}

// The player's character (the agent's own, else the player's) given the settings given: the inputs it may use (locked when not),
// all of them (when not, locked unless its controls' node is driven by its motion), its part's resting flag, its being
// vulnerable; then its controls' node driven by its motion or not, its state's boxOnly
void SetPlayerInputCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = NodeOf(runner)->owner;
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (node == nullptr)
    {
        // Retail bug: neither the player nor its character node (nor its controls' node below) is checked for none
        instance = PlayerInstance();
        node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    }

    auto* character = static_cast<CharacterAgent*>(node->agent);
    u32* locked = &character->buttons.locked.value;
    for (u32 setting = 0; setting < SettingCount; setting++)
    {
        u32 bit = (1u << setting) & 0xFFFF;
        if ((settings.given & bit) == 0)
        {
            continue;
        }

        u32 on = (settings.values & bit) != 0;
        if (setting < SettingAllInputs)
        {
            SetBit(locked, bit, on ^ 1);
        }
        else if (setting == SettingAllInputs)
        {
            if (on != 0)
            {
                *locked = 0;
            }
            else if (!controls.motionDriven)
            {
                *locked = CharacterButtons::LockAll;
            }
        }
        else if (setting == SettingResting)
        {
            auto* part = static_cast<CharacterPart*>(character->part);
            part->flags.resting = on;
        }
        else
        {
            auto* part = static_cast<CharacterPart*>(character->part);
            part->bits.invulnerable = on ^ 1;
        }
    }

    auto* controlsNode = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    controlsNode->bits.motionDriven = controls.motionDriven;
    character->state.boxOnly = controls.boxOnly;
}

// A playable character's instance in another chunk than the player's is put half a unit in front of the load wall of its chunk's
// link to the player's chunk (when that chunk's RM2 is loaded)
void WarpToChunkLinkTowardsPlayerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 InFrontOfWall = 0.5f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    if (GetGameNode(&instance->nodes, NodeCharacter) == nullptr)
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
    if (link == nullptr || link->flags.linkedRm2Loaded == 0)
    {
        return;
    }

    LoadWall* wall = link->loadWall;
    Vector4 normal = wall->plane;
    Vector4 middle;
    WallMiddle(wall->corners, &middle);
    Vector4 position;
    position.x = normal.x * InFrontOfWall + middle.x;
    position.y = normal.y * InFrontOfWall + middle.y;
    position.z = normal.z * InFrontOfWall + middle.z;
    position.w = 1.0f;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&position))
    {
        QueueObject(instance);
    }

    node->frameStart = position;
}

// The agent's instance made a checkpoint for the progress's pairing and characters: the respawn checkpoint (the saved one let go
// unless it's this instance) or the saved one (the respawn one let go and forgotten unless it's this instance), saving the game
// when it's set
void SetPlayerRespawnPositionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    GameController* controller = G_GameController;
    GameProgress* progress = &controller->progress;
    ChunkManager* chunks = controller->chunkManager;
    InstanceContext* instance = NodeOf(runner)->owner;
    u32 pairing = progress->play.pairing;
    bool saves = respawn.saves != 0;
    Checkpoint* checkpoint;
    if (saves)
    {
        progress->checkpoints[GameProgress::CheckpointRespawn]->Release(1, chunks, instance);
        checkpoint = progress->checkpoints[GameProgress::CheckpointSaved];
    }
    else
    {
        progress->checkpoints[GameProgress::CheckpointSaved]->Release(0, chunks, instance);
        checkpoint = progress->checkpoints[GameProgress::CheckpointRespawn];
    }

    // The pairings of one character and of two (5 as well); the others set nothing (the hoverboard's with its controls among them)
    bool set = false;
    GameProgress* now = &G_GameController->progress;
    switch (pairing)
    {
    case PairingAlone:
    case PairingHoverboard:
        set = checkpoint->Set(pairing, chunks, instance, now->Instance(now->play.character), nullptr);
        break;
    case PairingHumiliskate:
    case PairingRollerbrawl:
    case PairingTied:
    case Pairing5:
    {
        InstanceContext* character = now->Instance(now->play.character);
        InstanceContext* second = now->Instance(now->play.second);
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

// The agent's character tied to the character of its focus instance (awake), the focus's leading when the command says so
void LinkToFocusCharacterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    auto* own = static_cast<AgentNode*>(GetGameNode(&node->owner->nodes, NodeCharacter));
    if (own == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(own->agent);
    auto* other = static_cast<AgentNode*>(GetGameNode(&focus->nodes, NodeCharacter));
    if (other == nullptr)
    {
        return;
    }

    auto* focusCharacter = static_cast<CharacterAgent*>(other->agent);
    if (link.focusLeads)
    {
        focusCharacter->Link(character);
    }
    else
    {
        character->Link(focusCharacter);
    }
}

// A counter (the game's, or the agent's own) up by 1 when the player's velocity goes across the way from it to the agent's instance
// (to its side: to the left, or to the right) faster than 5, down by 1 below 1, kept within 0 and 255 when it's the agent's own
void CountPlayerCirclingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Fast = 5.0f;
    constexpr f32 Slow = 1.0f;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* player = progress->Instance(progress->play.character);
    InstanceContext* instance = NodeOf(runner)->owner;
    if (player == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(AgentOfKind(player, NodeCharacter));
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
    if (counter.toTheRight)
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
    else if (across < Slow)
    {
        change = -1;
    }

    if (counter.agentCounter)
    {
        u8* agentCounter = CounterOf(NodeOf(runner)->agent, counter.counter);
        s32 total = *agentCounter + change;
        if (total > Agent::CounterMax)
        {
            *agentCounter = Agent::CounterMax;
        }
        else if (total >= 0)
        {
            *agentCounter = static_cast<u8>(total);
        }
        else
        {
            *agentCounter = 0;
        }
    }
    else
    {
        AddToGameCounter(G_ChunkManager, counter.counter, change);
    }
}

void MakeCharactersIdleCommand::MakeIdle(InstanceContext* instance, BehaviourLevel*)
{
    if (instance == nullptr)
    {
        return;
    }

    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    if (node != nullptr)
    {
        RunAgentEvent(node->agent, EventIdle, reinterpret_cast<u32>(instance), 1, 0);
    }
}

// The idle behaviour started on the characters: every one the progress has (not CharacterNone's), Crash's, Cortex's and the
// Mecha-Bandicoot's, else the agent's own
void MakeCharactersIdleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel* level)
{
    if (characters.every)
    {
        MakeIdle(G_GameController->progress.Instance(CharacterCrash), level);
        MakeIdle(G_GameController->progress.Instance(CharacterCortex), level);
        MakeIdle(G_GameController->progress.Instance(CharacterTallCrash), level);
        MakeIdle(G_GameController->progress.Instance(CharacterNina), level);
        MakeIdle(G_GameController->progress.Instance(CharacterMecha), level);
        return;
    }

    if (!characters.crash && !characters.cortex && !characters.mecha)
    {
        MakeIdle(NodeOf(runner)->owner, level);
        return;
    }

    if (characters.crash)
    {
        MakeIdle(G_GameController->progress.Instance(CharacterCrash), level);
    }

    if (characters.cortex)
    {
        MakeIdle(G_GameController->progress.Instance(CharacterCortex), level);
    }

    if (characters.mecha)
    {
        MakeIdle(G_GameController->progress.Instance(CharacterMecha), level);
    }
}

// The player switched to a character, else to the character of an instance (a designator's, else one the agent's attachments
// link) when its agent is a playable character's
void SwitchCharacterCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    if (character != static_cast<s32>(GameProgress::NoCharacter))
    {
        G_GameController->SwitchCharacter(character, 1, 1);
        return;
    }

    InstanceContext* switched = nullptr;
    u32 designator = target.designator;
    if (designator != DesignatesNone)
    {
        GameNode* node = runner->agentNode;
        switched = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, designator);
    }
    else if (target.byLinked)
    {
        auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&NodeOf(runner)->owner->nodes, NodeAttachments));
        if (attachments != nullptr)
        {
            switched = attachments->linked[target.linked];
        }
    }

    if (switched == nullptr)
    {
        return;
    }

    auto* node = static_cast<ObjectNodeBase*>(GetGameNode(&switched->nodes, NodeObject));
    if (node == nullptr)
    {
        return;
    }

    // Retail bug: the agent is checked for none only after its vtable is called
    Agent* agent = node->agent;
    if (CallVirtual<u32>(agent, agent->vtable, Agent::IsCharacterSlot) == 0)
    {
        return;
    }

    agent = node->agent;
    if (agent == nullptr)
    {
        return;
    }

    G_GameController->SwitchCharacter(agent->properties->GetInt(CharacterKindProperty), 1, 1);
}

// The agent's settings given: its instance awake (asleep when not, its node's parts let go first), its instance's flags, its
// creature part's snapping to the ground, its part's bits
void SetAgentCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    for (u32 setting = 0; setting < SettingCount; setting++)
    {
        u32 bit = (1u << setting) & 0xFFFF;
        if ((settings.given & bit) == 0)
        {
            continue;
        }

        u32 on = (settings.values & bit) != 0;
        switch (setting)
        {
        case SettingAwake:
            if (on != 0)
            {
                CallVirtual<u32>(instance, instance->vtable, InstanceContext::WakeSlot);
            }
            else
            {
                // Retail leaves the clock it was given in the argument register the slot reads
                CallVirtual<void>(node, node->vtable, ObjectNode::ReleasePartsUnlessUnloadingSlot, clock);
                CallVirtual<u32>(instance, instance->vtable, InstanceContext::SleepSlot);
            }

            break;
        case SettingVisible:
            instance->flags.visible = on;
            break;
        case SettingCollision:
            instance->flags.collisionActive = on;
            break;
        case SettingTriggerSignals:
            instance->flags.receivesTriggerSignals = on;
            break;
        case SettingShadow:
            instance->flags.shadowActive = on;
            break;
        case SettingSnapsToGround:
        {
            auto* creature = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCreature));
            if (creature == nullptr)
            {
                break;
            }

            auto* part = static_cast<CreaturePart*>(creature->agent->part);
            part->flags.snapsToGround = on;
            break;
        }
        case SettingCanDamageCharacter:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits.canDamageCharacter = on;
            break;
        }
        case SettingVulnerable:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits.invulnerable = on ^ 1;
            break;
        }
        case SettingBulletsBounceBack:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits.bulletsBounceBack = on;
            break;
        }
        case SettingTargettable:
        {
            auto* part = static_cast<BasicAgentPart*>(AgentNodeOf(instance)->agent->part);
            part->bits.targettable = on;
            break;
        }
        default:
            break;
        }
    }
}

// The area play is in set (or the tagged value's), the last area open raised to it (areas past 24 none)
void SetPlayAreaCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr s32 Areas = 25;
    GameProgress* progress = &G_GameController->progress;
    s32 played = area;
    if (played == FromValue)
    {
        played = areaValue.IntWith(PropertiesOf(NodeOf(runner)));
    }

    // Retail bug: only areas past 24 are refused, the others below -1 are written as their low 5 bits
    if (played >= Areas)
    {
        return;
    }

    progress->play.area = static_cast<u32>(played);
    if (static_cast<s32>(progress->play.open) < played)
    {
        progress->play.open = static_cast<u32>(played);
    }
}

// The crates of the stack above the agent's instance (awake ones a ray 0.1 to 3.9 up hits) within half a unit of the lowest one's
// height start falling (their flag 9 set), and the instances around it are told
void TriggerBalancedCrateFallingCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 MostFound = 0x20;
    // The crates found have their collision active, and are neither asleep nor attached to an agent
    constexpr u32 Wanted = ReferencedObjectFlags::CollisionActive;
    constexpr u32 Unwanted = ReferencedObjectFlags::Asleep | ReferencedObjectFlags::Attached;
    constexpr f32 Start = Rounded(0.1);
    constexpr f32 Reach = Rounded(3.8);
    constexpr f32 Margin = 0.5f;
    ObjectNode* node = NodeOf(runner);
    InstanceContext* instance = node->owner;
    ChunkData* chunk = instance->chunk;
    void* results[MostFound];
    InstanceQuery query;
    StartQuery(&query, results, MostFound, Wanted, Unwanted);
    f32 lowest = Infinite;
    Vector4 point = PositionOf(instance);
    point.y = point.y + Start;
    Vector4 segment[2];
    segment[0] = point;
    point.y = point.y + Reach;
    segment[1] = point;
    SkipInQuery(&query, instance);
    f32 share = ChunkInstancesRayCast(chunk, segment, 1u << NodeCrate, &query, 0);
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
            auto* agent = static_cast<CrateAgent*>(AgentOfKind(crate, NodeCrate));
            if (PositionOf(crate).y - point.y < lowest)
            {
                agent->StartFalling();
                crate->flags.inDrawnCell = 1;
            }
        }
    }

    NotifyInstancesWithin(0.0f, node);
}

namespace
{
// The characters of two of the progress's characters, when both have one
bool CharactersOf(CharacterPairArgument pair, CharacterAgent** first, CharacterAgent** second)
{
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* firstInstance = progress->Instance(pair.first);
    InstanceContext* secondInstance = progress->Instance(pair.second);
    if (firstInstance == nullptr || secondInstance == nullptr)
    {
        return false;
    }

    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&firstInstance->nodes, NodeCharacter));
    auto* secondNode = static_cast<AgentNode*>(GetGameNode(&secondInstance->nodes, NodeCharacter));
    if (firstNode == nullptr || secondNode == nullptr)
    {
        return false;
    }

    *first = static_cast<CharacterAgent*>(firstNode->agent);
    *second = static_cast<CharacterAgent*>(secondNode->agent);
    return true;
}

// The progress's pairing and its characters set
void SetPairing(u32 pairing, CharacterAgent* first, CharacterAgent* second)
{
    GameProgress* progress = &G_GameController->progress;
    progress->play.pairing = pairing;
    progress->play.character = static_cast<u32>(first->properties->GetInt(CharacterKindProperty));
    progress->play.second = static_cast<u32>(second->properties->GetInt(CharacterKindProperty));
}
}

// Two of the progress's characters on the Rollerbrawl, the first driving
void SetVehicleRollerbrawlCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    CharacterAgent* first;
    CharacterAgent* second;
    if (!CharactersOf(characters, &first, &second))
    {
        return;
    }

    first->SetVehicle(Vehicle::KindRollerbrawl, second, 0);
    SetPairing(PairingRollerbrawl, first, second);
}

// Two of the progress's characters on the Humiliskate, the first skating on the second, with its top and crouched speeds
void SetVehicleHumiliskateCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* firstInstance = progress->Instance(characters.first);
    InstanceContext* secondInstance = progress->Instance(characters.second);
    PropertyHolder* properties = PropertiesOf(NodeOf(runner));
    f32 top = topSpeed.FloatWith(properties);
    f32 crouched = crouchedSpeed.FloatWith(properties);
    // Retail bug: unlike the Rollerbrawl's, the characters' instances aren't checked for none
    auto* firstNode = static_cast<AgentNode*>(GetGameNode(&firstInstance->nodes, NodeCharacter));
    auto* secondNode = static_cast<AgentNode*>(GetGameNode(&secondInstance->nodes, NodeCharacter));
    if (firstNode == nullptr || secondNode == nullptr)
    {
        return;
    }

    auto* first = static_cast<CharacterAgent*>(firstNode->agent);
    auto* second = static_cast<CharacterAgent*>(secondNode->agent);
    first->SetVehicle(Vehicle::KindHumiliskate, second, 0);
    SetPairing(PairingHumiliskate, first, second);
    auto* board = static_cast<HumiliskateVehicle*>(first->vehicle);
    board->crouchedSpeed = crouched;
    board->topSpeed = top;
}

// The character of a receiver's instance on the hoverboard that is the agent's instance (with the hoverboard's controls or not:
// pairing 7 or 6), alone
void SetVehicleHoverboardCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* board = NodeOf(runner)->owner;
    InstanceContext* riding = runner->InstanceOf(rider.receiver);
    if (riding == nullptr || board == nullptr)
    {
        return;
    }

    auto* riderNode = static_cast<AgentNode*>(GetGameNode(&riding->nodes, NodeCharacter));
    AgentNode* boardNode = AgentNodeOf(board);
    if (riderNode == nullptr || boardNode == nullptr)
    {
        return;
    }

    auto* character = static_cast<CharacterAgent*>(riderNode->agent);
    GameProgress* progress = &G_GameController->progress;
    character->SetVehicle(Vehicle::KindHoverboard, boardNode->agent, rider.boardControls);
    progress->play.pairing = rider.boardControls ? PairingHoverboardControls : PairingHoverboard;
    u32 played = static_cast<u32>(character->properties->GetInt(CharacterKindProperty));
    PlayState play = progress->play;
    play.character = played;
    play.second = GameProgress::NoCharacter;
    progress->play = play;
}

void CreateDamageCommand::ParseToken(const ScriptToken* token)
{
    switch (token->tag)
    {
    case TagSize:
        ParseTaggedValueRecord(token, &reach);
        break;
    case TagCylinderHeight:
        shape.kind = ShapeCylinder;
        ParseTaggedValueRecord(token, &height);
        break;
    case TagDamageHull:
        if (token->value == KeywordFirstHull)
        {
            shape.hull = 0;
        }

        break;
    default:
        ParseBaseToken(token);
        break;
    }
}

void CreateDamageCommand::ParseBaseToken(const ScriptToken* token)
{
    switch (token->tag)
    {
    case TagXShift:
        flags.offsetGiven = 1;
        offsetX = token->Float();
        break;
    case TagYShift:
        flags.offsetGiven = 1;
        offsetY = token->Float();
        break;
    case TagZShift:
        flags.offsetGiven = 1;
        offsetZ = token->Float();
        break;
    case TagExitPoint:
        flags.joint = token->value;
        break;
    case TagHitPoints:
        ParseTaggedValueRecord(token, &damage);
        break;
    case TagRepelForce:
        flags.givesMessageW = 1;
        ParseTaggedValueRecord(token, &messageW);
        break;
    case TagInstantDeath:
        flags.instantDeath = token->value == 0;
        break;
    case TagNone:
        switch (token->value)
        {
        case KeywordNearestOnly:
            flags.nearestOnly = 1;
            break;
        case KeywordNoCharacters:
            flags.skipsCharacters = 1;
            break;
        case KeywordOnlyCharacters:
            flags.onlyCharacters = 1;
            break;
        case KeywordHitExplosion:
            hitKinds |= HitExplosion;
            break;
        case KeywordHitFallingThrough:
            hitKinds |= HitFallingThrough;
            break;
        case KeywordHitBurning:
            hitKinds |= HitBurning;
            break;
        case KeywordHitIceteroid:
            hitKinds |= HitIceteroid;
            break;
        case KeywordHitProjectile:
            hitKinds |= HitProjectile;
            break;
        case KeywordHitKind6:
            hitKinds |= HitKind6;
            break;
        case KeywordHitElectric:
            hitKinds |= HitElectric;
            break;
        case KeywordHitKind8:
            hitKinds |= HitKind8;
            break;
        case KeywordHitGeneric:
            hitKinds |= HitGeneric;
            break;
        case KeywordHitCrush:
            hitKinds |= HitCrush;
            break;
        case KeywordHitKind12:
            hitKinds |= HitKind12;
            break;
        case KeywordHitKind13:
            hitKinds |= HitKind13;
            break;
        case KeywordHitKind14:
            hitKinds |= HitKind14;
            break;
        case KeywordHitBite:
            hitKinds |= HitBite;
            break;
        case KeywordHitSpin:
            hitKinds |= HitSpin;
            break;
        case KeywordHitKick:
            hitKinds |= HitKick;
            break;
        case KeywordHitKind18:
            hitKinds |= HitKind18;
            break;
        case KeywordHitHeavy:
            hitKinds |= HitHeavy;
            break;
        case KeywordHitKind21:
            hitKinds |= HitKind21;
            break;
        case KeywordHitKind22:
            hitKinds |= HitKind22;
            break;
        case KeywordHitSinking:
            hitKinds |= HitSinking;
            break;
        case KeywordHitKneeDrop:
            hitKinds |= HitKneeDrop;
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

void AddToGunSecondCountCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddToGunSecondCountCommand::ParseTokens(const ScriptTokenList* tokens)
{
    ScriptTokenReader reader;
    ScriptTokenReader::Construct(&reader, tokens);
    reader.First();
    while (!reader.AtEnd())
    {
        const ScriptToken* token = reader.Current();
        if (token->tag == AmountToken)
        {
            amount = static_cast<s32>(token->value);
        }

        reader.Next();
    }
}

void AddToGunSecondCountCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

void AddToGunSecondCountCommand::ExecuteOn(GameNode*)
{
    Gun* gun = reinterpret_cast<CharacterAgent*>(g_PlayerCharacter)->gun;
    if (gun != nullptr)
    {
        gun->AddSecondCount(amount);
    }
}

u32 AddToGunSecondCountCommand::Size()
{
    return sizeof(AddToGunSecondCountCommand);
}

f32 RidesRollerbrawl(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
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
            auto* driverNode = static_cast<AgentNode*>(GetGameNode(&vehicle->other->instance->nodes, NodeCharacter));
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
    if (priority != DefaultInitPriority || initialize == 0)
    {
        return;
    }

    g_CommandsUp.w = 1.0f;
    g_CommandsUp.x = 0.0f;
    g_CommandsShadowY = UiShadowOffset;
    g_CommandsZero = 0;
    g_CommandsUp.y = 1.0f;
    g_CommandsUp.z = 0.0f;
    g_CommandsShadowX = UiShadowOffset;
    ColourSet(&g_CommandsShadowColour, 0.0f, 0.0f, 0.0f, UiShadowAlpha);
    AngleFrom(&g_CommandsAngle45, QuarterPi, AngleRadians);
    // (48 objects of a header with nothing to construct follow in retail: an empty loop)
    HullConstruct(&g_PyramidHull);
}

void ConstructCharacterCommandsModule()
{
    InitCharacterCommandsModule(1, DefaultInitPriority);
}
