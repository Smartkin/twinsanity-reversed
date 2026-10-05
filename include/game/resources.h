#pragma once

#include "common.h"
#include "gcc2.h"

// A resource's first word: its references, and whether it stays when they run out
union ResourceBits
{
    u32 value;
    struct
    {
        u32 references : 16;
        // Cleared with kept, never set or read
        u32 unused16 : 1;
        // Kept when its references run out: a reader sets it when it reads a resource its table already has, taking a
        // reference clears it
        u32 kept : 1;
        u32 unused18 : 14;
    };
};
CHECK_SIZE(ResourceBits, 4);

// The first 8 bytes of every resource the tables share: its bits, and its ID (none for a resource made outside the tables, which
// is deleted rather than released)
struct ResourceHeader
{
    ResourceBits bits;
    u32 id;
};

constexpr u32 NoResourceId = 0xFFFFFFFF;

// A resource's ID as the tables, slots and lists keep it (an object's, a model's, an animation's, a script's, a sound's: the
// resource header's ID's low half): the low 15 bits are the resource's place in its table; every bit set is none (UndefinedId in
// code of any kind's IDs, else each kind's: NoObjectId, NoModelId, NoAnimationId, NoScriptId, NoSoundId)
constexpr u16 ResourceIndexMask = 0x7FFF;
// An ID that's undefined (a resource's in a slot or a list, an element's of a layout): every bit set
constexpr u16 UndefinedId = 0xFFFF;

inline ResourceHeader* HeaderOf(void* resource)
{
    return static_cast<ResourceHeader*>(resource);
}

// A resource's header: no references, not kept, no ID
inline void ConstructResourceHeader(void* resource)
{
    ResourceHeader* header = HeaderOf(resource);
    header->bits.references = 0;
    header->bits.unused16 = 0;
    header->bits.kept = 0;
    header->id = NoResourceId;
}

// A resource's header made with its ID
inline void ConstructResourceHeader(void* resource, u32 id)
{
    ResourceHeader* header = HeaderOf(resource);
    header->bits.references = 0;
    header->bits.unused16 = 0;
    header->bits.kept = 0;
    header->id = id;
}

// A reference taken: the resource isn't kept any more
inline void TakeReference(void* resource)
{
    ResourceHeader* header = HeaderOf(resource);
    header->bits.kept = 0;
    header->bits.references++;
}

// The resources a table let go of, waiting to be deleted (0xC00 bytes): a ring of 0x2FF that nothing checks has room.
// ResourcesStep deletes the oldest of one queue a step, a graphics table that lets every resource go deletes all of its own
struct DeletionQueue
{
    static constexpr u32 Size = 0x2FF;

    u16 head;
    u16 count;
    void* items[Size];

    void Push(void* item)
    {
        u32 at = head + count;
        if (at >= Size)
        {
            at -= Size;
        }

        items[at] = item;
        count++;
    }

    // The oldest deleted (none deletes nothing). Whether there was one
    template <typename Delete>
    bool DeleteFirst(Delete deleteItem)
    {
        if (count == 0)
        {
            return false;
        }

        void* item = items[head];
        if (item != nullptr)
        {
            deleteItem(item);
        }

        count--;
        head++;
        if (head >= Size)
        {
            head -= Size;
        }

        return true;
    }
};
CHECK_SIZE(DeletionQueue, 0xC00);

// A table's first word
union ResourceTableBits
{
    u32 value;
    struct
    {
        u32 capacity : 14;
        u32 unused14 : 1;
        // Set in every table but the objects', never read
        u32 unused15 : 1;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(ResourceTableBits, 4);

// A table of the game's resources of a kind (0x18 bytes; vtable at 0x14: 1 the destructor, 2 every resource let go, 3 and 4 (a
// count of resources added) nothing but in the scripts' and sounds'): its capacity, the resources by their IDs
// (ResourceIndexMask's bits), the order they were added in, and its deletion queue
struct ResourceTable
{
    ResourceTableBits bits;
    void** items;
    u16* order;
    u32 unused0C;
    DeletionQueue* queue;
    const GccVTableEntry* vtable;
};
CHECK_SIZE(ResourceTable, 0x18);

// The game's resources (0x44 bytes, vtable at 0x40): how many languages have voices, the tables of the game objects, the
// scripts, the animations, the models (OGIs), the code models, the sounds and each language's voices, and each table's deletion
// queue
struct GameResources
{
    enum Slot : u32
    {
        DestroySlot = 1,
        SetUpCodeModelsSlot = 2,
    };

    u32 languageCount;
    // The tools' path of the levels' agents ("Import\LevelAgents.axp"), never read
    const void* unused04;
    ResourceTable* objects;
    ResourceTable* scripts;
    ResourceTable* animations;
    ResourceTable* models;
    ResourceTable* codeModels;
    ResourceTable* sounds;
    ResourceTable** voices;
    DeletionQueue* objectQueue;
    DeletionQueue* scriptQueue;
    DeletionQueue* animationQueue;
    DeletionQueue* modelQueue;
    DeletionQueue* codeModelQueue;
    DeletionQueue* soundQueue;
    DeletionQueue** voiceQueues;
    const GccVTableEntry* vtable;

    // The base made for the languages: the graphics tables given their static deletion queues, no tables of its own yet
    static GameResources* ConstructBase(GameResources* resources, u32 languageCount, const void* agentsPath) RETAIL(InitResources);
    // Every table made (the game's capacities), and the game's resources it is
    static GameResources* Construct(GameResources* resources) RETAIL(InitializeAllGameResources);
    // Each table made with a capacity, its deletion queue with it (a table of each language for the voices). Each returns the
    // table (the voices: the array of them)
    ResourceTable* MakeObjectTable(u32 capacity) RETAIL(InitObjectTable);
    ResourceTable* MakeScriptTable(u32 capacity) RETAIL(InitScriptTable);
    ResourceTable* MakeAnimationTable(u32 capacity) RETAIL(InitAnimationTable);
    ResourceTable* MakeCodeModelTable(u32 capacity) RETAIL(InitCodeModelTable);
    ResourceTable* MakeModelTable(u32 capacity) RETAIL(InitOgiTable);
    ResourceTable* MakeSoundTable(u32 capacity) RETAIL(InitSoundTable);
    ResourceTable** MakeVoiceTables(u32 capacity) RETAIL(InitVoiceTables);
    // The base's destructor (its vtable's slot 1): every table destroyed, everything waiting in the deletion queues deleted, the
    // queues freed
    void DestroyBase(u32 destroyFlags) RETAIL(FUN_00264c48);
    // The resources of each object of a chunk's list (a count, then the IDs) taken and let go of (game/objects.h's
    // LoadObjectResources and ReleaseObjectResources)
    void TakeObjects(const u32* objects) RETAIL(FUN_00266180);
    void ReleaseObjects(const u32* objects) RETAIL(FUN_00266260);
    // The game's (its vtable GameResources_Methods): 1 the destructor (the base's), 2 the code models' slots set up again (the
    // custom pickups' and projectiles' cleared first when asked, then each of the 200 code models a pickup's or a projectile's
    // set up), 3 nothing
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017a720);
    void SetUpCodeModels(u32 clear) RETAIL(FUN_00171c20);
    void Nothing3() RETAIL(FUN_0017a748);
};

extern "C"
{
    // Every resource of every table let go of (their vtables' function 2)
    void ReleaseCodeResources(GameResources* resources) RETAIL(FUN_00265920);
    // The scripts' table (the game's resources')
    extern ResourceTable* g_ScriptTable RETAIL(G_ScriptTable);
    // A resource's ID copied (the ID's copy constructor, out of line): the copy
    u16* CopyResourceId(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
}
CHECK_OFFSET(GameResources, voiceQueues, 0x3C);
CHECK_SIZE(GameResources, 0x44);
