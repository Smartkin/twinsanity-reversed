#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/chunkdata.h"
#include "game/chunkfiles.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/place.h"
#include "game/readers.h"
#include "game/string.h"

class Stream;
struct AiNavigation;
struct AiPosition;
struct GameResources;
template <typename T>
struct PointerArray;
struct ChunkLoader;
struct ChunkManager;
struct InstanceContext;
// A chunk's store of persistent flags (its vtable at its start, the base's UnkChunkInterface_methods: 1 its words to read, 2 its
// words, 3 how many words, 4 how many flags, 5 the destructor, 6 read from a stream, 7 written; the game's two stores are
// game/gamechunkmanager.cpp's)
struct PersistentFlags
{
    enum Slot : u32
    {
        WordsToReadSlot = 1,
        WordsSlot = 2,
        WordCountSlot = 3,
        CountSlot = 4,
        DestroySlot = 5,
        ReadSlot = 6,
        WriteSlot = 7,
    };

    const GccVTableEntry* vtable;

    const u32* WordsToRead()
    {
        return CallVirtual<const u32*>(this, vtable, WordsToReadSlot);
    }

    u32* Words()
    {
        return CallVirtual<u32*>(this, vtable, WordsSlot);
    }

    u32 WordCount()
    {
        return CallVirtual<u32>(this, vtable, WordCountSlot);
    }

    u32 Count()
    {
        return CallVirtual<u32>(this, vtable, CountSlot);
    }

    void Destroy(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, destroyFlags);
    }

    void Read(Stream* stream)
    {
        CallVirtual<void>(this, vtable, ReadSlot, stream);
    }

    void Write(Stream* stream)
    {
        CallVirtual<void>(this, vtable, WriteSlot, stream);
    }

    // The base's: made, destroyed, every flag cleared (through the vtable)
    static PersistentFlags* ConstructBase(PersistentFlags* flags) RETAIL(FUN_002691b0);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_002691c8);
    void Clear() RETAIL(FUN_002691f8);
};
struct ReferencedObject;
struct Reference;
struct TimeClock;

// A reference to a chunk loader, like the objects' (game/reference.h): the last owning one destroys the loader
struct LoaderReference
{
    ChunkLoader* loader;
    ReferenceBits bits;
};

// What a chunk's file loader asks whether its file is wanted: the RM2's wants it within one keep link of the focus chunk (the
// loader's keep depth), the SM2's within one link of any kind (its depth)
class WantPolicy
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        WantsSlot = 2,
        DoesNotWantSlot = 3,
    };

    const GccVTableEntry* vtable;

    // The base's destructor (D_002FC2E0, whose other functions are abstract)
    void BaseDestroy(u32 flags) RETAIL(FUN_001f6880);

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, flags);
    }

    bool Wants(ChunkLoader* loader)
    {
        return CallVirtual<u32>(this, vtable, WantsSlot, loader) != 0;
    }

    bool DoesNotWant(ChunkLoader* loader)
    {
        return CallVirtual<u32>(this, vtable, DoesNotWantSlot, loader) != 0;
    }
};

class Sm2WantPolicy : public WantPolicy
{
public:
    void Destroy(u32 flags) RETAIL(FUN_001f5800);
    bool Wants(ChunkLoader* loader) RETAIL(FUN_001f5838);
    bool DoesNotWant(ChunkLoader* loader) RETAIL(FUN_001f5848);
    // Wanted at the first depth past the wanted ones (2: one link further), which nothing asks
    bool JustPastWanted(ChunkLoader* loader) RETAIL(FUN_001f5860);
};

class Rm2WantPolicy : public WantPolicy
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00269440);
    bool Wants(ChunkLoader* loader) RETAIL(FUN_00269478);
    bool DoesNotWant(ChunkLoader* loader) RETAIL(FUN_00269490);
    bool JustPastWanted(ChunkLoader* loader) RETAIL(FUN_002694a8);
};

// The loader of a chunk's RM2 or SM2: a state machine (StepStates) that waits a second before loading a wanted file, waits 4
// seconds (0.1 when no link reached the chunk this frame) before unloading an unwanted one, and goes back when it's wanted again.
// Its vtable follows 0x14 bytes of members
class ChunkLoaderBase
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        StartLoadingSlot = 2,
        ContinueLoadingSlot = 3,
        StartUnloadingSlot = 4,
        ContinueUnloadingSlot = 5,
        HoldsSlot = 6,
        StepSlot = 7,
        IsWantedSlot = 8,
        IsUnwantedSlot = 9,
    };

    enum State : s32
    {
        None = 0,
        Queued = 1,
        WaitingToLoad = 2,
        Loading = 3,
        Loaded = 4,
        WaitingToUnload = 5,
        Unloading = 6,
        Unloaded = 7,
    };

    ChunkLoader* loader;
    // The loaders are made for any number of files, the game's have one: these are its
    WantPolicy* policy;
    // The clock's ticks when it got into its state, and the ticks it waits
    s32 stateTime;
    s32 delay;
    s32 state;
    const GccVTableEntry* vtable;

    void Destroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, DestroySlot, flags);
    }

    // Start loading or unloading the file (the index of the policy): false when it can't yet. Continuing returns false when done
    bool StartLoading(s32 index, bool now)
    {
        return CallVirtual<u32>(this, vtable, StartLoadingSlot, index, now) != 0;
    }

    bool ContinueLoading(s32 index, bool now)
    {
        return CallVirtual<u32>(this, vtable, ContinueLoadingSlot, index, now) != 0;
    }

    bool StartUnloading(s32 index, bool now)
    {
        return CallVirtual<u32>(this, vtable, StartUnloadingSlot, index, now) != 0;
    }

    bool ContinueUnloading(s32 index, bool now)
    {
        return CallVirtual<u32>(this, vtable, ContinueUnloadingSlot, index, now) != 0;
    }

    // Whether the object is in the loader's chunk
    bool Holds(ReferencedObject* object)
    {
        return CallVirtual<u32>(this, vtable, HoldsSlot, object) != 0;
    }

    // Moves through the states. Returns whether it's busy (neither queued nor unloaded)
    bool Step(bool now)
    {
        return (CallVirtual<u32>(this, vtable, StepSlot, now) & 0xFF) != 0;
    }

    bool IsWanted()
    {
        return CallVirtual<u32>(this, vtable, IsWantedSlot) != 0;
    }

    bool IsUnwanted()
    {
        return CallVirtual<u32>(this, vtable, IsUnwantedSlot) != 0;
    }

    // The base's versions
    static ChunkLoaderBase* Construct(ChunkLoaderBase* base, ChunkLoader* loader) RETAIL(FUN_002ad1b0);
    void DestroyBase(u32 flags) RETAIL(FUN_002ad248);
    bool StepStates(bool now) RETAIL(FUN_002aa8a0);
    bool PolicyWants() RETAIL(FUN_002ad2a0);
    bool PolicyDoesNotWant() RETAIL(FUN_002ad318);

    // In the state, or with any the waiting state next to it too (waiting to load after queued and before loading, waiting to
    // unload after loaded)
    bool IsQueued(bool any) RETAIL(FUN_002ad390);
    bool IsLoading(bool any) RETAIL(FUN_002ad3b0);
    bool IsLoaded(bool any) RETAIL(FUN_002ad3d0);
};
CHECK_SIZE(ChunkLoaderBase, 0x18);

// What the chunk manager keeps of a chunk whose RM2 is loaded
struct ChunkEntry
{
    String path;
    u16 index;
    u16 unused0E;
    ChunkManager* manager;
    ChunkDataReference* data;
    // The object IDs the RM2 brought (a count, then the IDs)
    u32* objects;
    // The persistent flags of its instances: its own store, saved with the game, and the other one, reset with the game
    PersistentFlags* savedFlags;
    PersistentFlags* unsavedFlags;
    // The next persistent flag slot its instances that keep one get (their IDs in it)
    u16 nextFlagSlot;
    u16 unused26;
    // The AI navigation of its layouts' AI positions and paths, its layouts' positions (and how many the layouts made as the
    // chunk's own have) and its layouts' paths
    AiNavigation* navigation;
    PointerArray<void>* positions;
    u32 positionCount;
    PointerArray<void>* paths;

    // A position of a layout that isn't the chunk's own (they follow the chunk's own), nullptr when the chunk has none
    void* OtherLayoutPosition(u32 index) RETAIL(FUN_00268788);
    // The next persistent flag slot given out
    u16 NextFlagSlot() RETAIL(NextPersistentFlagSlot);
    // Unloaded first, its saved store of persistent flags destroyed, its data let go and its path
    void Destroy(u32 destroyFlags) RETAIL(FUN_00263fa8);
    // Every position and every path of its layouts destroyed, the list freed (none: nothing)
    void DestroyPositions() RETAIL(FUN_00263da8);
    void DestroyPaths() RETAIL(FUN_00263e98);
};
CHECK_OFFSET(ChunkEntry, savedFlags, 0x1C);
CHECK_OFFSET(ChunkEntry, navigation, 0x28);
CHECK_OFFSET(ChunkEntry, manager, 0x10);
CHECK_SIZE(ChunkEntry, 0x38);

// The chunk manager's flags (its constructor's, the game context's bits 6 and 7)
union ChunkManagerFlags
{
    u16 value;
    struct
    {
        // The default chunk's RM2 queued for reading ("RB" given: otherwise only what's queued already is read, the particles and
        // the shadows come from their own files)
        u16 queuesDefaultRm2 : 1;
        // The resources of the default chunk's objects not taken
        u16 takesNoObjects : 1;
        u16 unused2 : 14;
    };
};
CHECK_SIZE(ChunkManagerFlags, 2);

// The chunk manager (G_ChunkManager_). Its vtable (the base's D_00304208, the game's ChunkManager__Methods) follows 0x1FB8 bytes of
// members: 1 a chunk's entry (made when there's none), 2 a chunk unloaded, 3 the destructor
struct ChunkManager
{
    enum Slot : u32
    {
        AddChunkSlot = 1,
        RemoveChunkSlot = 2,
    };

    static constexpr u32 MaxChunks = 0x200;
    static constexpr u32 Counters = 1000;

    // How many chunks it has and the chunks
    u16 count;
    ChunkManagerFlags flags;
    ChunkEntry* entries[MaxChunks];
    // The default chunk's RM2 reader and the object IDs it brought (a count, then the IDs)
    Rm2Reader* defaultReader;
    u32* defaultObjects;
    // The game's resources the chunks' objects go into, and the path finder (retail's MiniBigBoi, 0x1FC0 bytes in),
    // which starts with every chunk's AI navigation by the chunk's index
    struct GameResources* resources;
    struct PathFinder* pathFinder;
    // The instances' IDs (g_InstanceIds)
    InstanceIds instanceIds;
    // The game's counters the scripts set, add to and test
    s32 counters[Counters];
    const GccVTableEntry* vtable;

    ChunkEntry* AddChunk(const char* path, ChunkData* chunk)
    {
        return CallVirtual<ChunkEntry*>(this, vtable, AddChunkSlot, path, chunk);
    }

    void RemoveChunk(ChunkEntry* entry)
    {
        CallVirtual<void>(this, vtable, RemoveChunkSlot, entry);
    }

    // The base made: no chunks, the instances' IDs made (g_InstanceIds), the two flags (each argument's low bit), and the
    // manager the game's (g_ChunkManager)
    static ChunkManager* Construct(ChunkManager* manager, struct GameResources* resources, u32 queuesDefaultRm2,
                                   u32 takesNoObjects) RETAIL(FUN_00268b50);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00268c10);
    // The counters cleared, the default chunk's RM2 reader made and read now (and its objects' resources taken unless the flags
    // say not to), then the resources told they were read
    void LoadDefault(const char* path) RETAIL(LoadDefault);
    // A chunk's entry made and added, given its saved store of persistent flags (the game's slot 1's; the fourth argument unused)
    ChunkEntry* NewEntry(const char* path, ChunkData* chunk, void* unused, PersistentFlags* store) RETAIL(FUN_00268948);
    // Its vtable's slot 2: the chunk unloaded, its entry kept. Returns 1
    u32 RemoveChunkEntry(ChunkEntry* entry) RETAIL(FUN_00268a78);
    void ClearCounters() RETAIL(FUN_00268da8);
    // Its vtable's slot 1: the chunk of the path given the data (made with no objects, flags, navigation, positions or paths
    // and added when there's none). The game's manager has its own (ChunkManager__Methods' GetChunkMeta, which makes the
    // chunks' stores of persistent flags too)
    ChunkEntry* AddChunkEntry(const char* path, ChunkData* chunk) RETAIL(FUN_00264248);
};
CHECK_OFFSET(ChunkManager, instanceIds, 0x814);
CHECK_OFFSET(ChunkManager, counters, 0x1018);
CHECK_OFFSET(ChunkManager, vtable, 0x1FB8);

extern "C"
{
    extern ChunkManager* g_ChunkManager RETAIL(G_ChunkManager_);
    // The game's chunk manager (game/gamechunkmanager.cpp, 0x3200 bytes) made: the base with the instance factory's resources and
    // its two flags, its own path finder (64 chunks' navigations); the path finder's and the instance factory's globals set
    void* ConstructChunkManager(void* manager, void* factory, u32 queuesDefaultRm2, u32 takesNoObjects) RETAIL(FUN_0017a508);
}

extern "C"
{
    // The chunk of a list of the chunk manager's (a count, then the chunks) whose data an instance's chunk is (nullptr for none)
    ChunkEntry* ChunkOfInstance(void* chunks, struct InstanceContext* instance) RETAIL(GetChunkMetaFromInstanceContext);
    // The chunk of the path, and of an index (nullptr for none)
    ChunkEntry* FindChunkEntry(ChunkManager* chunks, const char* path) RETAIL(FindChunkInfoByName);
    ChunkEntry* ChunkOfIndex(void* manager, u16 index) RETAIL(FUN_00268e60);
    // A chunk unloaded: its AI navigation, positions and paths, its unsaved store of persistent flags, its objects' resources
    // and its data let go
    void UnloadChunkEntry(ChunkEntry* chunk) RETAIL(FUN_002687e0);
    // A chunk's unsaved store of persistent flags set (the game's manager makes it)
    void SetUnsavedFlags(ChunkEntry* entry, PersistentFlags* flags) RETAIL(FUN_002688b0);
    // Every chunk destroyed, none left
    void UnloadAllChunks(ChunkManager* chunks) RETAIL(UnloadAllChunks_);
    // The AI position of a chunk nearest a point, with its index when asked; and the nearest with any of the required flags and
    // none of the ruled out ones (their low 16 bits). nullptr for a chunk without AI navigation
    AiPosition* NearestAiPosition(ChunkEntry* chunk, const Vector4* point, u16* index) RETAIL(FUN_002688d0);
    AiPosition* NearestFlaggedAiPosition(ChunkEntry* chunk, const Vector4* point, u16* index, u32 required, u32 ruledOut)
        RETAIL(FUN_00268910);
    // The chunks' instances reset for a way into the game, filtered by three words (the entry also the word every instance freed
    // gets)
    void ResetChunkInstances(ChunkManager* chunks, u32 entry, const u32* filter) RETAIL(FUN_00268fe0);
    // The chunks reset for a way into the game (the unsaved stores of their persistent flags cleared, the saved ones too when
    // their instances are dropped) and the game's counters cleared
    void ResetChunks(ChunkManager* chunks, u32 entry, u32 dropInstances) RETAIL(FUN_00269038);
    // The game's counters (the chunk manager's) set, added to and read
    void SetGameCounter(void* manager, u32 counter, s32 value) RETAIL(FUN_002690f8);
    void AddToGameCounter(void* manager, u32 counter, s32 value) RETAIL(FUN_00269108);
    s32 GameCounter(void* manager, u32 counter) RETAIL(FUN_00269130);
    // The chunk's instance of the ID (an awake one whose agent has it: nullptr for none, for the ID 0xFFFF and for a chunk without
    // data; the first 0x400 instances of the chunk are looked at)
    InstanceContext* FindChunkInstance(ChunkEntry* chunk, u16 id) RETAIL(FUN_00264120);
    // The chunks' persistent flags read (every chunk unloaded first, the chunks read added) and written: the count, then each
    // chunk's path and whether its saved store follows
    void ReadChunkStates(ChunkManager* chunks, Stream* stream) RETAIL(FUN_00264390);
    void WriteChunkStates(ChunkManager* chunks, Stream* stream) RETAIL(FUN_00268f08);
    // Every instance of the chunks' sceneries a filter matches (its kinds of nodes, the flags it has all of and none of): its
    // object node's parts let go, its slot 20 called and its runners stopped (the chunks unused)
    void StopFilteredObjectNodes(ChunkManager* chunks, const u32* filter) RETAIL(FUN_00264488);
    // A persistent flag set or cleared, toggled and read, when the store has it (0 when it hasn't)
    void SetPersistentFlag(PersistentFlags* flags, u32 index, u32 value) RETAIL(SetPersistentFlag);
    void TogglePersistentFlag(PersistentFlags* flags, u32 index) RETAIL(FUN_00269330);
    u32 GetPersistentFlag(PersistentFlags* flags, u32 index) RETAIL(GetPersistentFlag);
}

struct GameChunkLink;

class Sm2Loader : public ChunkLoaderBase
{
public:
    Sm2Reader* reader;
    ChunkDataReference* data;

    static Sm2Loader* Construct(Sm2Loader* loader, ChunkLoader* chunk) RETAIL(FUN_001f4560);
    void Destroy(u32 flags) RETAIL(FUN_001f45c0);
    // Makes the chunk's data and starts reading the SM2 into it
    bool StartLoading(s32 index, bool now) RETAIL(StartLoadingSM2);
    bool ContinueLoading(s32 index, bool now) RETAIL(FUN_001f5a18);
    bool StartUnloading(s32 index, bool now) RETAIL(FUN_001f5a70);
    bool ContinueUnloading(s32 index, bool now) RETAIL(FUN_001f4998);
    bool Holds(ReferencedObject* object) RETAIL(FUN_001f5aa8);
    // Gives the chunk's data the node kinds it steps by its depth, then steps the states
    bool Step(bool now) RETAIL(FUN_001f47e8);
    // Points the link at the linked chunk's data (hasLinkedData while there's some)
    void UpdateLink(GameChunkLink* link) RETAIL(FUN_001f4750);
    void Unlink(GameChunkLink* link) RETAIL(FUN_001f59d0);
};
CHECK_SIZE(Sm2Loader, 0x20);

class Rm2Loader : public ChunkLoaderBase
{
public:
    Rm2Reader* reader;
    ChunkEntry* entry;

    static Rm2Loader* Construct(Rm2Loader* loader, ChunkLoader* chunk) RETAIL(FUN_002694c0);
    void Destroy(u32 flags) RETAIL(FUN_00269520);
    // Once the SM2 made the chunk's data: the chunk manager's entry, then the RM2 read into it
    bool StartLoading(s32 index, bool now) RETAIL(FUN_002647e8);
    bool ContinueLoading(s32 index, bool now) RETAIL(FUN_00269600);
    bool StartUnloading(s32 index, bool now) RETAIL(FUN_002696f0);
    bool ContinueUnloading(s32 index, bool now) RETAIL(FUN_00269780);
    bool Holds(ReferencedObject* object) RETAIL(FUN_00269788);
    // The link's linkedRm2Loaded while the linked chunk's RM2 is loaded
    void UpdateLink(GameChunkLink* link) RETAIL(FUN_00269588);
    void Unlink(GameChunkLink* link) RETAIL(FUN_002695f8);
};
CHECK_SIZE(Rm2Loader, 0x20);

// A link hull's flags
union LinkHullFlags
{
    u32 value;
    struct
    {
        // Another hull follows
        u32 hasNext : 1;
        u32 unused1 : 31;
    };
};
CHECK_SIZE(LinkHullFlags, 4);

// A link's hulls: the player loads the linked chunk while inside one of them
struct LinkHullList
{
    // The item type the chunk links' builder makes it for
    static constexpr u32 TypeId = 0x1D02;

    LinkHullFlags flags;
    LinkHullList* next;
    CollisionHull hull;
};
CHECK_SIZE(LinkHullList, 0x28);

// A chunk link's type
union ChunkLinkType
{
    u32 value;
    struct
    {
        // Hulls follow the matrices
        u32 hasHulls : 1;
        // The linked chunk loads while there's no player yet (through a link with hulls)
        u32 loadsWithoutPlayer : 1;
        u32 unused2 : 30;
    };
};
CHECK_SIZE(ChunkLinkType, 4);

// A chunk's link to another: the chunk's path, its loader while it's wanted, its matrices and hulls
struct GameChunkLink
{
    ChunkLinkType type;
    String path;
    LoaderReference* loader;
    u8 unused14[0x20 - 0x14];
    ChunkLinkData data;
    LinkHullList* hulls;
    GameChunkLink* next;
    GameChunkLink* previous;
    u32 unusedCC;
};
CHECK_OFFSET(GameChunkLink, data, 0x20);
CHECK_SIZE(GameChunkLink, 0xD0);

struct ChunkLoadingManager;

// A chunk loader's bits
union ChunkLoaderBits
{
    u32 value;
    struct
    {
        // The depth of links it's wanted at for its scenery, and of keep links (ChunkLoader::NoDepth not at all)
        u32 depth : 4;
        u32 keepDepth : 4;
        // The manager's: its files are queued for reading; its RM2's objects' resources aren't taken nor its AI navigation made,
        // and its links are followed whatever their hulls and (LoadingStreamed) whether its files are wanted or not
        u32 queuesFiles : 1;
        u32 takesNoObjects : 1;
        // Every linked chunk loaded, and every linked chunk queued or loaded
        u32 linkedLoaded : 1;
        u32 linkedQueued : 1;
        u32 unused12 : 20;
    };
};
CHECK_SIZE(ChunkLoaderBits, 4);

// A chunk being loaded or loaded: its two files' loaders and its links
struct ChunkLoader
{
    // The depth of a chunk the links didn't reach: not wanted at all
    static constexpr u32 NoDepth = 0xF;
    // A file is wanted at the depths below it: the focus chunk's (0) and its links' (1)
    static constexpr u32 WantedDepths = 2;

    LoaderReference* self;
    ChunkLoaderBits bits;
    // The manager's frame it was last reached by the links in
    s32 frame;
    String path;
    ChunkLoadingManager* manager;
    TimeClock* clock;
    Rm2Loader* rm2;
    Sm2Loader* sm2;
    GameChunkLink* links;
    ChunkLoader* next;
    ChunkLoader* previous;

    // Destroys the links and the loaders
    void Destroy(u32 flags) RETAIL(FUN_002a9df0);
};
CHECK_SIZE(ChunkLoader, 0x34);
CHECK_OFFSET(ChunkLoader, next, 0x2C);

// How the chunks load (the loading manager's mode: the game context's bits 9-12 when its bit 8 is set, else LoadingStreamed).
// Only the last one unloads
enum ChunkLoadingMode : u32
{
    // The links aren't followed: only the chunks queued load, a second after they're wanted
    LoadingQueuedOnly = 0,
    // The chunks the links reach load at once, waited for when loading now: the ones queued already, or every one
    LoadingKnownAtOnce = 1,
    LoadingAllAtOnce = 2,
    // At once, not waited for
    LoadingAtOnce = 3,
    // A second after they're wanted
    LoadingDelayed = 4,
    // The game's: a second after they're wanted, unloaded 4 seconds after they aren't and forgotten
    LoadingStreamed = 5,
};

// The chunk loading manager's bits
union ChunkLoadingBits
{
    u32 value;
    struct
    {
        // The loaders of chunks under its path, and how many of them are loaded (counted every update)
        u32 loaderCount : 12;
        u32 loadedCount : 12;
        // ChunkLoadingMode
        u32 mode : 4;
        // Given to new loaders (the game context's bits 6, "RB", and 7, like the chunk manager's flags)
        u32 queuesFiles : 1;
        u32 takesNoObjects : 1;
        // Unloading everything: the focus isn't followed
        u32 unloading : 1;
        u32 unused31 : 1;
    };
};
CHECK_SIZE(ChunkLoadingBits, 4);

// The chunks loading and loaded, and the object the loading follows
struct ChunkLoadingManager
{
    ChunkLoadingBits bits;
    String path;
    s32 frame;
    TimeClock* clock;
    ChunkLoader* loaders;
    // The chunk the links are followed from
    ChunkLoader* focusLoader;
    Reference* focus;
};
CHECK_SIZE(ChunkLoadingManager, 0x24);

extern "C"
{
    ChunkLoadingManager* ConstructChunkLoadingManager(ChunkLoadingManager* manager, TimeClock* clock, u32 mode) RETAIL(FUN_002ad3f0);
    // The loader of the path (in either case), nullptr for none
    ChunkLoader* FindChunkLoader(ChunkLoadingManager* manager, String* path) RETAIL(FUN_002ab078);
    // The loader of the chunk the object is in
    ChunkLoader* FindChunkLoaderOf(ChunkLoadingManager* manager, ReferencedObject* object) RETAIL(FUN_002ad578);
    // The loading follows the object, unless everything's being unloaded
    void SetChunkLoadingFocus(ChunkLoadingManager* manager, ReferencedObject* object) RETAIL(FUN_002ad478);
    // The path's loader (made and queued when there's none) wanted at the depth for both files (depth 0 makes it the focus
    // chunk)
    ChunkLoader* QueueChunk(ChunkLoadingManager* manager, String* path, u32 depth) RETAIL(FUN_002aae10);
    // Follows the links from the focus chunk, steps every loader, forgets the idle ones (LoadingStreamed) and reads (all of it
    // now, or a step: read is what the readers did). Now, in the modes that wait, it goes on until every chunk is loaded. Returns
    // whether chunks are still loading and the readers did something
    bool UpdateChunkLoading(ChunkLoadingManager* manager, bool now, u8* read) RETAIL(FUN_002ab150);
    // Stops following the focus. Now, it also stops the sound, draws two empty frames and unloads every chunk
    void UnloadEverything(ChunkLoadingManager* manager, bool now, ChunkManager* chunks) RETAIL(FUN_002aac68);

    bool ChunkLoaderIsLoaded(ChunkLoader* loader, bool any) RETAIL(FUN_002ad130);
    // Steps the loader's files if it's under the manager's path. Returns whether they're busy
    bool ChunkLoaderStep(ChunkLoader* loader, s32 frame, bool now) RETAIL(FUN_002aa708);
    // Whether every linked chunk is loaded or, failing that, unwanted by one file's loader while the other file's loaded
    bool LinkedChunksLoaded(ChunkLoader* loader, bool any) RETAIL(FUN_002a9fc8);
    void ChunkLoaderRemoveLinks(ChunkLoader* loader) RETAIL(FUN_002ad070);
    // Follows the loader's links, loading the linked chunks at one more depth: through a link with hulls only while the focus
    // object is inside one (or, without a focus object, when the link says so)
    void LoadLinkedChunks(ChunkLoader* loader, s32 frame, u32 keepDepth, u32 depth);
    // Reads the chunk's links: a count, then each link
    void LoadChunkLinks(ChunkLoader* loader, Stream* reader);
    void ReadChunkLink(GameChunkLink* link, Stream* reader);
    void DestroyChunkLink(GameChunkLink* link, u32 flags) RETAIL(FUN_002a9c80);
    LoaderReference** AssignLoaderReference(LoaderReference** to, LoaderReference** from) RETAIL(FUN_002ace40);

    LinkHullList* InitLinkHullList(LinkHullList* list, Stream* reader);
    void FreeLinkHullList(LinkHullList* list, u32 flags);
    // The chunk links' items' builder (a vtable alone, D_003069D0, the game context's): a list of hulls (LinkHullList::TypeId,
    // empty; none for another type), and its destructor
    void* MakeChunkLinkItem(void* builder, u32 type) RETAIL(FUN_002add58);
    void DestroyChunkLinkItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_002add28);
    bool IsPositionInLinkHulls(LinkHullList* list, const Vector4* position);
    void ReadLinkHullList(LinkHullList* list, Stream* reader);

    // The intrusive lists (the last two arguments are GCC's pointers to the members, unused)
    void LinkListRemove(GameChunkLink* link, GameChunkLink** head, s32 previous, s32 next) RETAIL(FUN_002ae3e8);
    void LinkListPushFront(GameChunkLink* link, GameChunkLink** head, s32 previous, s32 next) RETAIL(FUN_002ae460);
    bool LoaderListContains(ChunkLoader* loader, ChunkLoader** head, s32 previous, s32 next) RETAIL(FUN_002ae4a0);
    void LoaderListPushFront(ChunkLoader* loader, ChunkLoader** head, s32 previous, s32 next) RETAIL(FUN_002ae4d8);
    void LoaderListRemove(ChunkLoader* loader, ChunkLoader** head, s32 previous, s32 next) RETAIL(FUN_002ae518);
}
