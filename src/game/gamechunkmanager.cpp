#include "game/chunkloading.h"

#include "game/instancefactory.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/stream.h"

// The game's chunk manager (retail's ChunkManager__Methods over the base's D_00304208, 0x3200 bytes: the base's 0x1FC0, then its
// path finder) and the chunks' stores of persistent flags it gives every chunk it adds: the store saved with the game (retail's
// UnkChunkInterfaceImp_Type1) and the one that isn't (Type2), each 256 flags in eight words after their vtable

namespace
{
constexpr u32 FlagWords = 8;
constexpr u32 FlagCount = 0x100;
// How many chunks' navigations the path finder goes through
constexpr u16 PathFinderChunks = 0x40;
}

// The store saved with the game, and the one that isn't (read and written as nothing)
class SavedFlagStore : public PersistentFlags
{
public:
    u32 words[FlagWords];

    static SavedFlagStore* Construct(SavedFlagStore* store) RETAIL(FUN_0017a5d0);
    const u32* WordsToRead() RETAIL(FUN_0017a3e0);
    u32* Words() RETAIL(FUN_0017a3e8);
    u32 WordCount() RETAIL(FUN_0017a3f0);
    u32 Count() RETAIL(FUN_0017a3f8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017a3c0);
    void Read(Stream* stream) RETAIL(FUN_0017a610);
    void Write(Stream* stream) RETAIL(FUN_0017a648);
};
CHECK_SIZE(SavedFlagStore, 0x24);

class UnsavedFlagStore : public PersistentFlags
{
public:
    u32 words[FlagWords];

    const u32* WordsToRead() RETAIL(FUN_0017c818);
    u32* Words() RETAIL(FUN_0017c820);
    u32 WordCount() RETAIL(FUN_0017c828);
    u32 Count() RETAIL(FUN_0017c830);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017c838);
    void Read(Stream* stream) RETAIL(FUN_0017c860);
    void Write(Stream* stream) RETAIL(FUN_0017c868);
};
CHECK_SIZE(UnsavedFlagStore, 0x24);

struct GameChunkManager : ChunkManager
{
    u32 unused1FBC;
    GamePathFinder gamePathFinder;

    // Its vtable's functions: 1 the chunk of the path given the data (a new one, made with a saved store of persistent flags;
    // either way given an unsaved store when it has none), 3 the destructor
    ChunkEntry* AddChunk(const char* path, ChunkData* chunk) RETAIL(GetChunkMeta);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017a580);
};
CHECK_OFFSET(GameChunkManager, gamePathFinder, 0x1FC0);
CHECK_SIZE(GameChunkManager, 0x3200);

extern "C"
{
    extern const GccVTableEntry g_GameChunkManagerVTable[] RETAIL(ChunkManager__Methods);
    extern const GccVTableEntry g_SavedFlagStoreVTable[] RETAIL(UnkChunkInterfaceImp_Type1_methods);
    extern const GccVTableEntry g_UnsavedFlagStoreVTable[] RETAIL(UnkChunkInterfaceImp_Type2_methods);

}

namespace
{
// The unsaved store (its constructor is inline)
PersistentFlags* MakeUnsavedFlagStore()
{
    auto* store = static_cast<UnsavedFlagStore*>(MemoryAllocate(sizeof(UnsavedFlagStore)));
    PersistentFlags::ConstructBase(store);
    store->vtable = g_UnsavedFlagStoreVTable;
    store->Clear();
    return store;
}
}

SavedFlagStore* SavedFlagStore::Construct(SavedFlagStore* store)
{
    PersistentFlags::ConstructBase(store);
    store->vtable = g_SavedFlagStoreVTable;
    store->Clear();
    return store;
}

const u32* SavedFlagStore::WordsToRead()
{
    return words;
}

u32* SavedFlagStore::Words()
{
    return words;
}

u32 SavedFlagStore::WordCount()
{
    return FlagWords;
}

u32 SavedFlagStore::Count()
{
    return FlagCount;
}

void SavedFlagStore::Destroy(u32 destroyFlags)
{
    BaseDestroy(destroyFlags);
}

void SavedFlagStore::Read(Stream* stream)
{
    stream->Read(words, sizeof(words), 1);
}

void SavedFlagStore::Write(Stream* stream)
{
    stream->Write(words, sizeof(words));
}

const u32* UnsavedFlagStore::WordsToRead()
{
    return words;
}

u32* UnsavedFlagStore::Words()
{
    return words;
}

u32 UnsavedFlagStore::WordCount()
{
    return FlagWords;
}

u32 UnsavedFlagStore::Count()
{
    return FlagCount;
}

void UnsavedFlagStore::Destroy(u32 destroyFlags)
{
    vtable = g_UnsavedFlagStoreVTable;
    BaseDestroy(destroyFlags);
}

void UnsavedFlagStore::Read(Stream*)
{
}

void UnsavedFlagStore::Write(Stream*)
{
}

ChunkEntry* GameChunkManager::AddChunk(const char* path, ChunkData* chunk)
{
    ChunkEntry* entry = FindChunkEntry(this, path);
    if (entry == nullptr)
    {
        auto* saved = SavedFlagStore::Construct(static_cast<SavedFlagStore*>(MemoryAllocate(sizeof(SavedFlagStore))));
        entry = NewEntry(path, chunk, this, saved);
        SetUnsavedFlags(entry, MakeUnsavedFlagStore());
        return entry;
    }

    PersistentFlags* unsaved = entry->unsavedFlags;
    AssignChunkData(&entry->data, chunk);
    if (unsaved == nullptr)
    {
        SetUnsavedFlags(entry, MakeUnsavedFlagStore());
    }

    return entry;
}

void GameChunkManager::Destroy(u32 destroyFlags)
{
    vtable = g_GameChunkManagerVTable;
    gamePathFinder.Destroy(DestroyOnly);
    ChunkManager::Destroy(destroyFlags);
}

void* ConstructChunkManager(void* memory, void* factory, u32 queuesDefaultRm2, u32 takesNoObjects)
{
    auto* manager = static_cast<GameChunkManager*>(memory);
    auto* instanceFactory = static_cast<InstanceFactory*>(factory);
    ChunkManager::Construct(manager, instanceFactory->resources, queuesDefaultRm2, takesNoObjects);
    manager->vtable = g_GameChunkManagerVTable;
    GamePathFinder::Construct(&manager->gamePathFinder);
    manager->gamePathFinder.count = PathFinderChunks;
    manager->pathFinder = &manager->gamePathFinder;
    g_PathFinder = &manager->gamePathFinder;
    g_InstanceFactory = instanceFactory;
    return manager;
}
