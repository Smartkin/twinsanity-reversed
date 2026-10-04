#include "game/chunkloading.h"

#include "game/instancefactory.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/stream.h"

// The game's chunk manager (retail's ChunkManager__Methods over the base's D_00304208, 0x3200 bytes: the base's 0x1FC0, then its
// path finder) and the chunks' stores of persistent flags it gives every chunk it adds: their own store, saved with the game
// (retail's UnkChunkInterfaceImp_Type1), and the other one (Type2), each 256 flags in eight words after their vtable

namespace
{
constexpr u32 FlagWords = 8;
constexpr u32 FlagCount = 0x100;
// How many chunks' navigations the path finder goes through
constexpr u16 PathFinderChunks = 0x40;
}

// The chunks' own store (saved with the game) and the other one (read and written as nothing)
class OwnFlagStore : public PersistentFlags
{
public:
    u32 words[FlagWords];

    static OwnFlagStore* Construct(OwnFlagStore* store) RETAIL(FUN_0017a5d0);
    const u32* WordsToRead() RETAIL(FUN_0017a3e0);
    u32* Words() RETAIL(FUN_0017a3e8);
    u32 WordCount() RETAIL(FUN_0017a3f0);
    u32 Count() RETAIL(FUN_0017a3f8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017a3c0);
    void Read(Stream* stream) RETAIL(FUN_0017a610);
    void Write(Stream* stream) RETAIL(FUN_0017a648);
};
CHECK_SIZE(OwnFlagStore, 0x24);

class OtherFlagStore : public PersistentFlags
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
CHECK_SIZE(OtherFlagStore, 0x24);

struct GameChunkManager : ChunkManager
{
    u32 unknown1FBC;
    GamePathFinder gamePathFinder;

    // Its vtable's functions: 1 the chunk of the path given the data (a new one, made with the chunk's own store of persistent
    // flags; either way given the other store when it has none), 3 the destructor
    ChunkEntry* AddChunk(const char* path, ChunkData* data) RETAIL(GetChunkMeta);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0017a580);
};
CHECK_OFFSET(GameChunkManager, gamePathFinder, 0x1FC0);
CHECK_SIZE(GameChunkManager, 0x3200);

extern "C"
{
    extern const GccVTableEntry g_GameChunkManagerVTable[] RETAIL(ChunkManager__Methods);
    extern const GccVTableEntry g_OwnFlagStoreVTable[] RETAIL(UnkChunkInterfaceImp_Type1_methods);
    extern const GccVTableEntry g_OtherFlagStoreVTable[] RETAIL(UnkChunkInterfaceImp_Type2_methods);

}

namespace
{
// The other store (its constructor is inline)
PersistentFlags* MakeOtherFlagStore()
{
    auto* store = static_cast<OtherFlagStore*>(MemoryAllocate(sizeof(OtherFlagStore)));
    PersistentFlags::ConstructBase(store);
    store->vtable = g_OtherFlagStoreVTable;
    store->Clear();
    return store;
}
}

OwnFlagStore* OwnFlagStore::Construct(OwnFlagStore* store)
{
    PersistentFlags::ConstructBase(store);
    store->vtable = g_OwnFlagStoreVTable;
    store->Clear();
    return store;
}

const u32* OwnFlagStore::WordsToRead()
{
    return words;
}

u32* OwnFlagStore::Words()
{
    return words;
}

u32 OwnFlagStore::WordCount()
{
    return FlagWords;
}

u32 OwnFlagStore::Count()
{
    return FlagCount;
}

void OwnFlagStore::Destroy(u32 destroyFlags)
{
    BaseDestroy(destroyFlags);
}

void OwnFlagStore::Read(Stream* stream)
{
    stream->Read(words, sizeof(words), 1);
}

void OwnFlagStore::Write(Stream* stream)
{
    stream->Write(words, sizeof(words));
}

const u32* OtherFlagStore::WordsToRead()
{
    return words;
}

u32* OtherFlagStore::Words()
{
    return words;
}

u32 OtherFlagStore::WordCount()
{
    return FlagWords;
}

u32 OtherFlagStore::Count()
{
    return FlagCount;
}

void OtherFlagStore::Destroy(u32 destroyFlags)
{
    vtable = g_OtherFlagStoreVTable;
    BaseDestroy(destroyFlags);
}

void OtherFlagStore::Read(Stream*)
{
}

void OtherFlagStore::Write(Stream*)
{
}

ChunkEntry* GameChunkManager::AddChunk(const char* path, ChunkData* data)
{
    ChunkEntry* entry = FindChunkEntry(this, path);
    if (entry == nullptr)
    {
        auto* own = OwnFlagStore::Construct(static_cast<OwnFlagStore*>(MemoryAllocate(sizeof(OwnFlagStore))));
        entry = NewEntry(path, data, this, own);
        SetOtherFlags(entry, MakeOtherFlagStore());
        return entry;
    }

    PersistentFlags* other = entry->otherFlags;
    AssignChunkData(reinterpret_cast<ChunkDataReference**>(&entry->data), data);
    if (other == nullptr)
    {
        SetOtherFlags(entry, MakeOtherFlagStore());
    }

    return entry;
}

void GameChunkManager::Destroy(u32 destroyFlags)
{
    vtable = g_GameChunkManagerVTable;
    gamePathFinder.Destroy(DestroyOnly);
    ChunkManager::Destroy(destroyFlags);
}

void* ConstructChunkManager(void* memory, void* factory, u32 flag16, u32 flag17)
{
    auto* manager = static_cast<GameChunkManager*>(memory);
    auto* instanceFactory = static_cast<InstanceFactory*>(factory);
    ChunkManager::Construct(manager, instanceFactory->resources, flag16, flag17);
    manager->vtable = g_GameChunkManagerVTable;
    GamePathFinder::Construct(&manager->gamePathFinder);
    manager->gamePathFinder.count = PathFinderChunks;
    manager->pathFinder = &manager->gamePathFinder;
    g_PathFinder = &manager->gamePathFinder;
    g_InstanceFactory = instanceFactory;
    return manager;
}
