#include "game/chunkfiles.h"

#include "game/graphicstables.h"
#include "game/resources.h"

#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/disk.h"
#include "game/memory.h"
#include "game/readers.h"
#include "game/reference.h"
#include "game/stream.h"
#include "retail/libc.h"

// The items reading a chunk's files: the SM2 into the chunk's data, the RM2 into the resources and the chunk's instances
extern "C"
{
    extern const GccVTableEntry g_Sm2ReaderVTable[] RETAIL(SM2Item_Methods);
    extern const GccVTableEntry g_Rm2ReaderVTable[] RETAIL(RM2_Item_Methods);
    extern const GccVTableEntry g_Sm2SectionReaderVTable[] RETAIL(SM2Reader_Methods);
    extern const GccVTableEntry g_Rm2SectionReaderVTable[] RETAIL(Rm2Reader_Methods);
    extern const GccVTableEntry g_ScenerySectionReaderVTable[] RETAIL(ScenerySectionReader_Methods);
    // ".sm2" and ".rm2"
    extern const char* g_Sm2Extension RETAIL(D_0030A010);
    extern const char* g_Rm2Extension RETAIL(D_0030A160);

    // Tells the chunk's scenery its data is read
    void ChunkSceneryRead(ChunkData* data) RETAIL(FUN_001f22a0);

    void Rm2ReaderDestroyInstances(Rm2Reader* reader) RETAIL(FUN_0026a4f0);

    void ReadScenery(ChunkData* data, MemoryStream* stream);
    void LoadDynamicScenery(void* scenery, MemoryStream* stream);
    void ReadRM2(Rm2Reader* reader, u32 id, u32 offset, MemoryStream* stream);
}

namespace
{
constexpr u32 FileSectionType = 1;
constexpr u32 Sm2Sections = 7;
constexpr u32 Rm2Sections = 12;

ChunkData* ChunkDataOf(Sm2Loader* loader)
{
    return loader->data != nullptr ? loader->data->data : nullptr;
}

template <typename Reader>
Reader* MakeSectionReader(const GccVTableEntry* vtable, ItemInterface* item, const ItemHeader* header)
{
    auto* reader = static_cast<Reader*>(MemoryAllocate(sizeof(Reader)));
    reader->id = header->id;
    reader->vtable = vtable;
    reader->offset = header->offset;
    reader->item = item;
    return reader;
}

void QueueFile(ItemInterface* item, const String& path, const char* extension)
{
    String file;
    file.string = nullptr;
    file.length = 0;
    file.capacity = 0;
    StringAssign(&file, path.string);
    StringAppend(&file, extension);
    AddResourcePackageToLoadQueue(item, file.string, 0);
    StringDestroy(&file);
}
}

Sm2Reader* Sm2Reader::Construct(Sm2Reader* reader, Sm2Loader* loader)
{
    reader->vtable = g_Sm2ReaderVTable;
    reader->path.string = nullptr;
    reader->path.length = 0;
    reader->path.capacity = 0;
    reader->loader = loader;
    reader->reading = 1;
    InitGraphicsItem(&reader->graphics);
    StringAssign(&reader->path, loader->loader->path.string);
    return reader;
}

void Sm2Reader::Destroy(u32 flags)
{
    UnloadGraphics(&graphics, 2);
    StringDestroy(&path);
    vtable = g_ItemInterfaceVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

s32 Sm2Reader::SectionCount()
{
    return Sm2Sections;
}

s32 Sm2Reader::Unknown3()
{
    return 1;
}

bool Sm2Reader::CanRead(u32 type)
{
    return type == FileSectionType;
}

SectionReader* Sm2Reader::GetReader(s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    // The graphics section is read as far as its header, the rest is queued
    if (header->id == 6)
    {
        *size = 0xC;
    }

    return MakeSectionReader<Sm2SectionReader>(g_Sm2SectionReaderVTable, this, header);
}

void Sm2Reader::Unload()
{
    ChunkData* data = ChunkDataOf(loader);
    if (data != nullptr)
    {
        u8* scenery = data->scenery;
        if (scenery != nullptr)
        {
            CallVirtual<void>(scenery, *reinterpret_cast<const GccVTableEntry**>(scenery + 0x44), 1, 3u);
        }

        data->scenery = nullptr;
    }

    ReleaseGraphicsResources(&graphics);
}

void Sm2Reader::Finish(s32, u32, u32)
{
    ChunkData* data = ChunkDataOf(loader);
    ChunkSceneryRead(data);
    ChunkListAdd(GetChunkList(), data);
    FinishGraphicsReading(&graphics);
    reading = 0;
}

void Sm2Reader::Queue(bool now, bool queue)
{
    if (queue)
    {
        QueueFile(this, path, g_Sm2Extension);
    }

    if (now)
    {
        LoadQueuedSectionsIntoMemory_();
    }
}

void Sm2SectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Sm2SectionReader::Read(u8* data, u32 size, ReaderStack*)
{
    auto* sm2 = static_cast<Sm2Reader*>(item);
    Sm2Loader* loader = sm2->loader;
    switch (id)
    {
    case 0:
    {
        auto* section = ScenerySectionReader::Construct(
            static_cast<ScenerySectionReader*>(MemoryAllocate(sizeof(ScenerySectionReader))), data, size);
        MemoryReader* reader = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), section,
                                                       nullptr, 0);
        ReadScenery(ChunkDataOf(loader), section->stream);
        AddItemReaderToReaderStorage(g_ReadersStorages[0], reader, 0);
        break;
    }
    case 4:
    {
        MemoryStream stream;
        MemoryStream::Construct(&stream, data, size, 0, 0x40);
        LoadDynamicScenery(ChunkDataDynamicScenery(ChunkDataOf(loader)), &stream);
        stream.Destroy(2);
        break;
    }
    case 5:
    {
        MemoryStream stream;
        MemoryStream::Construct(&stream, data, size, 0, 0x40);
        LoadChunkLinks(loader->loader, &stream);
        stream.Destroy(2);
        break;
    }
    case 6:
        AddSectionToLoadQueue(&sm2->graphics, offset);
        break;
    default:
        break;
    }
}

Rm2Reader* Rm2Reader::Construct(Rm2Reader* reader, ChunkEntry* entry)
{
    reader->path.string = nullptr;
    reader->vtable = g_Rm2ReaderVTable;
    reader->path.length = 0;
    reader->path.capacity = 0;
    StringAssign(&reader->path, entry->path.string);
    reader->resources = entry->manager->resources;
    reader->collision = nullptr;
    reader->objects = entry->objects;
    InitGraphicsItem(&reader->graphics);
    CodeItem::Construct(&reader->code, reader->resources, reader->objects);
    reader->entry = entry;
    auto* data = static_cast<ChunkData*>(entry->data != nullptr ? static_cast<void*>(entry->data->object) : nullptr);
    void* collision = data->collision;
    reader->bits = 0;
    reader->bits |= 2;
    reader->collision = ConstructCollisionHolder(static_cast<CollisionData**>(MemoryAllocate(sizeof(CollisionData*))),
                                                 static_cast<CollisionData*>(collision));
    for (u32 i = 0; i < 8; i++)
    {
        reader->unknown128[i] = nullptr;
        reader->unknown148[i] = nullptr;
    }

    return reader;
}

void Rm2Reader::Destroy(u32 flags)
{
    vtable = g_Rm2ReaderVTable;
    MemoryDeallocate2_(collision);
    Rm2ReaderDestroyInstances(this);
    code.Unload(DestroyOnly);
    UnloadGraphics(&graphics, 2);
    StringDestroy(&path);
    vtable = g_ItemInterfaceVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

s32 Rm2Reader::SectionCount()
{
    return Rm2Sections;
}

s32 Rm2Reader::Unknown3()
{
    return 1;
}

bool Rm2Reader::CanRead(u32 type)
{
    return type == FileSectionType;
}

SectionReader* Rm2Reader::GetReader(s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    // These sections are read as far as their headers, the rest is queued
    switch (header->id)
    {
    case 9:
        *size = 0x14;
        break;
    case 10:
    case 11:
        *size = 0xC;
        break;
    default:
        break;
    }

    return MakeSectionReader<Rm2SectionReader>(g_Rm2SectionReaderVTable, this, header);
}

void Rm2Reader::Unload()
{
    if (entry != nullptr)
    {
        Reference* data = entry->data;
        ReleaseChunkData(reinterpret_cast<ChunkData*>(data != nullptr ? data->object : nullptr), true, true, false);
    }

    UnloadPendingResources(resources);
    ReleaseGraphicsResources(&graphics);
}

void Rm2Reader::Finish(s32, u32, u32)
{
    CallVirtual<void>(resources, resources->vtable, 2, 0u);
    if ((bits & 1) != 0)
    {
        ForgetGraphicsReading(&graphics);
    }
    else
    {
        FinishGraphicsReading(&graphics);
    }

    bits &= ~2u;
}

void Rm2Reader::Queue(bool now, bool queue)
{
    if (queue)
    {
        g_Rm2Queued = 1;
        QueueFile(this, path, g_Rm2Extension);
    }

    if (now)
    {
        LoadQueuedSectionsIntoMemory_();
    }
}

void Rm2SectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Rm2SectionReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, 0x40);
    ReadRM2(static_cast<Rm2Reader*>(item), id, offset, &stream);
    stream.Destroy(2);
}

ScenerySectionReader* ScenerySectionReader::Construct(ScenerySectionReader* reader, const u8* data, u32 size)
{
    reader->node = -1;
    reader->vtable = g_ScenerySectionReaderVTable;
    s32 node;
    DiskAllocate(&node, GetDiskManager(), size, true, 0);
    reader->node = node;
    u8* copy = DiskMemory(GetDiskManager(), &reader->node);
    RetailLibc::MemoryCopy(copy, data, size);
    reader->stream = MemoryStream::Construct(static_cast<MemoryStream*>(MemoryAllocate(sizeof(MemoryStream))), copy, size, 0, 0x40);
    return reader;
}

void ScenerySectionReader::Destroy(u32 flags)
{
    vtable = g_ScenerySectionReaderVTable;
    if (node >= 0)
    {
        DiskRelease(GetDiskManager(), &node);
    }

    if (stream != nullptr)
    {
        stream->Destroy(3);
    }

    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScenerySectionReader::Read(u8*, u32, ReaderStack*)
{
}

void ScenerySectionReader::Missing(u8*, u32, ReaderStack*)
{
}
