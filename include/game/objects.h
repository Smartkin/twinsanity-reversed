#pragma once

#include "abi.h"
#include "common.h"
#include "game/agentlab.h"
#include "game/properties.h"
#include "game/resources.h"
#include "game/string.h"

class Stream;
struct ScriptPack;

// A list of resources' IDs of a kind (0x24 bytes): up to 16 kept in it, more in a list of its own the first word then points to
// (a first word past 16)
struct ResourceIdList
{
    static constexpr u32 BlockIds = 16;

    u32 countOrMore;
    u16 ids[BlockIds];

    // Read from a stream: the count, then each ID (what it had before let go)
    void Read(Stream* stream) RETAIL(FUN_00262c98);
    // The list it goes on in let go
    void Destroy(u32 destroyFlags) RETAIL(FUN_00262b08);
    // An ID added at the end (a full list goes on in a list of its own)
    void Add(const u16* id) RETAIL(FUN_00263b40);
};
CHECK_SIZE(ResourceIdList, 0x24);

// A walk over a list's IDs, block by block (vtable at 0: 1 the destructor, 2 to the first ID, 3 whether it's past the last, 4
// the current ID, 5 to the next; its base's are abstract): the list, the block it's in (none past the last) and the index in it
struct ResourceIdIterator
{
    const GccVTableEntry* vtable;
    ResourceIdList* list;
    ResourceIdList* block;
    u32 index;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00262bc8);
    void First() RETAIL(FUN_00262bf8);
    bool IsDone() RETAIL(FUN_00262c20);
    u16* Current() RETAIL(GetNextObjectElemIdAddress);
    void Next() RETAIL(FUN_00262c30);
    // Its base's destructor
    void DestroyBase(u32 destroyFlags) RETAIL(FUN_00262b98);
};
CHECK_SIZE(ResourceIdIterator, 0x10);

// The resources a game object names, by kind (0x20 bytes): a bit for each kind it has a list of
struct ResourceReferences
{
    static constexpr u32 KindCount = 7;

    u32 kinds;
    ResourceIdList* lists[KindCount];

    // Made and read from a stream: the kinds, then each kind's list (it returns its last list, not itself)
    static void Construct(ResourceReferences* references, Stream* stream) RETAIL(ReadResourceReferences);
    // Made with no kinds
    static ResourceReferences* ConstructEmpty(ResourceReferences* references) RETAIL(FUN_00262ae0);
    // Its lists let go
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025f640);
};
CHECK_SIZE(ResourceReferences, 0x20);

// The kinds of resources the references list, by their list's index (the sixth kind is never loaded or let go of)
enum ResourceKind : u32
{
    ResourceObjects,
    ResourceModels,
    ResourceAnimations,
    ResourceCodeModels,
    ResourceScripts,
    ResourceUnused,
    ResourceSounds,
};

// A code model (0x18 bytes, a resource of the game's tables): its resource header, its kind and the slot of the custom pickups or
// projectiles it sets up (both 0xFF when made), the count of its script packs, the packs (made with new[]), a command and the
// packs' IDs
struct CodeModel
{
    // The kinds that set up a custom slot (the others set up nothing)
    enum Kind : u8
    {
        KindPickup = 0x11,
        KindProjectile = 0x12,
        KindNone = 0xFF,
    };

    static constexpr u8 NoSlot = 0xFF;

    u32 bits;
    u32 id;
    u8 kind;
    u8 slot;
    u8 packCount;
    u8 unused0B;
    ScriptPack* packs;
    ScriptCommand* command;
    u16* packIds;

    // Made empty (no ID)
    static CodeModel* Construct(CodeModel* model) RETAIL(FUN_00263840);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00263898);
    // Its four bytes (its kind, slot, pack count and a byte nothing reads), its packs with their IDs and its command read (what
    // it had before is lost)
    void Read(Stream* stream) RETAIL(ReadCodeModel);
};
CHECK_SIZE(CodeModel, 0x18);

// A game object's script pack: a count, then its command list when there's one
struct ScriptPack
{
    u32 count;
    ScriptCommand* commands;

    // Every command executed on a node (an instance made of the object runs them on its object node as it's made)
    void Run(void* node) RETAIL(ExecuteGameObjectAppendCommands);
};

// An array of IDs or words as GCC 2.9x's new[] made it (a cookie of 16 bytes counting them before them), and its count
template <typename T>
struct ObjectArray
{
    T* items;
    u32 count;
};

// A game object's header (12 bytes, the RM2's): its model's exit points and react joints (OgiAnimator::reactJoints), its
// subtype (16 and 17 pickups with nodes of their own, 16's without properties) and type, whether it has properties, whether
// properties and resource references follow it in the RM2, then how many behaviour slots, trigger behaviours and sounds it has
// (and how many models and objects, which nothing reads)
union ObjectHeader
{
    u32 words[3];
    struct
    {
        u32 exitPoints : 6;
        u32 reactJoints : 6;
        u32 subtype : 8;
        u32 type : 8;
        u32 hasProperties : 1;
        u32 readsProperties : 1;
        u32 readsReferences : 1;
        u32 unused31 : 1;
        u8 unused4;
        u8 behaviourSlots;
        u8 unused6;
        u8 triggerBehaviourCount;
        u8 soundSlots;
        u8 unused9[3];
    };
};
CHECK_SIZE(ObjectHeader, 0xC);

// A trigger behaviour of an object (the AgentLab tool's TwinObjectTriggerBehaviour): the message it answers, its behaviour
// starter's ID and the runner it starts on (the Xbox version sets every bit above)
union TriggerBehaviour
{
    u32 value;
    struct
    {
        u32 message : 10;
        u32 starter : 14;
        u32 runner : 1;
        u32 unused25 : 7;
    };
};
CHECK_SIZE(TriggerBehaviour, 4);

// An object's ID of none (an empty slot, a node without an object of its own), which also stands for any object where the
// commands take an object's instances (every one when none is given)
constexpr u16 NoObjectId = 0xFFFF;
constexpr u16 AnyObjectId = 0xFFFF;

// A sound slot of an object of none (a script's or a trail's: nothing is played)
constexpr u16 NoSoundSlot = 0xFFFF;

// The game's objects (0x60 bytes, the RM2's code section's): its resource header (game/resources.h: its references, bits and
// ID), its header, its name, its properties, the resources it names, its script pack and its slots: its trigger behaviours, then
// the IDs of its models (OGIs), animations, scripts, objects and sounds
struct GameObject
{
    // The types of objects (their agents' classes)
    enum Type : u32
    {
        TypeCharacter,
        TypePickup,
        TypeCrate,
        TypeCreature,
        TypeGenericObject,
        TypeGrabbable,
        TypePayGate,
        TypeGraple,
        TypeProjectile,
        TypeCount,
    };

    u32 bits;
    s32 id;
    ObjectHeader header;
    String name;
    PropertyList* properties;
    ResourceReferences* references;
    ScriptPack scripts;
    ObjectArray<TriggerBehaviour> triggerBehaviours;
    ObjectArray<u16> models;
    ObjectArray<u16> animations;
    ObjectArray<u16> behaviours;
    ObjectArray<u16> objects;
    ObjectArray<u16> sounds;

    // Made and read from a stream, or made empty (no subtype or type: 0xFF)
    static GameObject* Construct(GameObject* object, Stream* stream) RETAIL(CreateGameObject);
    static GameObject* ConstructEmpty(GameObject* object) RETAIL(FUN_0025d1e0);
    // Its header, name, slots, properties, resource references and script pack read (what it had before let go)
    void Read(Stream* stream) RETAIL(ReadGameObject);
    // Its properties (when it has them), resource references, slots, script pack and name let go
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025d380);
};
CHECK_OFFSET(GameObject, header, 0x8);
CHECK_OFFSET(GameObject, name, 0x14);
CHECK_OFFSET(GameObject, scripts, 0x28);
CHECK_OFFSET(GameObject, sounds, 0x58);
CHECK_SIZE(GameObject, 0x60);

extern "C"
{
    // A slot's ID of a game object (returned through the first argument, as GCC 2.9x returns such a struct): a model's, an
    // animation's, a script's, a sound's (0xFFFF past the object's sounds)
    u16* GetObjectModelId(u16* id, const GameObject* object, u32 slot) RETAIL(GetOgiID_FromScriptSlots);
    u16* GetObjectAnimationId(u16* id, const GameObject* object, u32 slot) RETAIL(GetAnimID_FromObject);
    u16* GetObjectBehaviourId(u16* id, const GameObject* object, u32 slot) RETAIL(GetScriptId_FromScriptSlot);
    u16* GetObjectSoundId(u16* id, const GameObject* object, u32 slot) RETAIL(GetSoundID_FromObject);
    // A word of the object's first slots read, and an ID of a resource list
    void ReadObjectWord(u32* word, Stream* stream) RETAIL(FUN_00263a38);
    void ReadResourceId(u16* id, Stream* stream) RETAIL(FUN_00263ab0);
    // A trigger behaviour of the object (its word)
    const u32* GetObjectTriggerBehaviour(const GameObject* object, u32 index) RETAIL(GetTriggerReceiver);
    // A script pack made empty and read
    ScriptPack* ConstructScriptPack(ScriptPack* pack) RETAIL(InitGameObjectScriptAppend);
    void ReadScriptPack(ScriptPack* pack, Stream* stream) RETAIL(LoadScriptPack);
    void DestroyScriptPack(ScriptPack* pack, u32 destroyFlags) RETAIL(FUN_00251910);
    // A reference taken to every resource an object's references list, and to the object of the ID (whose own ID among the
    // objects listed is skipped): a resource not in its table yet is made empty there, not kept. Sounds the game's table has are
    // taken there, the others from the voices of the language. Let go of in the opposite order: a resource without references
    // and not kept leaves its table, into the table's deletion queue or deleted when it has none
    void LoadObjectResources(ResourceReferences* references, const u16* objectId, struct GameResources* resources)
        RETAIL(LoadObjectResources);
    void ReleaseObjectResources(ResourceReferences* references, const u16* objectId, struct GameResources* resources)
        RETAIL(FUN_00260310);
}
