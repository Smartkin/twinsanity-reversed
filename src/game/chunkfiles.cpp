#include "game/chunkfiles.h"
#include "game/dynamicscenery.h"

#include "game/graphicstables.h"
#include "game/resources.h"

#include "game/chunkloading.h"
#include "game/collision.h"
#include "game/instancesection.h"
#include "game/disk.h"
#include "game/memory.h"
#include "game/particles.h"
#include "game/readers.h"
#include "game/reference.h"
#include "game/scenery.h"
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
    // The extensions of the RM2's other files
    extern const char g_Ga2Extension[] RETAIL(D_0030A168);
    extern const char g_Gc2Extension[] RETAIL(D_0030A170);
    extern const char g_Ge2Extension[] RETAIL(D_0030A178);
    extern const char g_Gw2Extension[] RETAIL(D_0030A180);
    extern const char g_Ma2Extension[] RETAIL(D_0030A188);
    extern const char g_Mc2Extension[] RETAIL(D_0030A190);
    extern const char g_Me2Extension[] RETAIL(D_0030A198);
    extern const char g_PtlExtension[] RETAIL(D_0030A1A0);
    extern const char g_Su2Extension[] RETAIL(D_0030A1A8);
    extern const char g_TriExtension[] RETAIL(D_0030A1B0);
}

namespace
{
// The SM2's sections (1-3 not read)
enum Sm2Section : u32
{
    Sm2Scenery = 0,
    Sm2DynamicScenery = 4,
    Sm2Links = 5,
    Sm2Graphics = 6,
    Sm2Sections = 7,
};

// The RM2's sections past its layouts' instances (sections 0-7)
enum Rm2Section : u32
{
    Rm2Particles = Rm2Reader::Layouts,
    Rm2Collision = 9,
    Rm2Code = 10,
    Rm2Graphics = 11,
    Rm2Sections = 12,
};

// The layouts 0-2 and the last are the chunk's own, the others aren't
constexpr u32 FirstOtherLayout = 3;
constexpr u32 LastLayout = Rm2Reader::Layouts - 1;

ChunkData* ChunkDataOf(Sm2Loader* loader)
{
    return loader->data != nullptr ? loader->data->chunk : nullptr;
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
    UnloadGraphics(&graphics, DestroyOnly);
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

s32 Sm2Reader::SectionType()
{
    return DefaultSectionType;
}

bool Sm2Reader::CanRead(u32 type)
{
    return type == DefaultSectionType;
}

SectionReader* Sm2Reader::GetReader(s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    // The graphics section is read as far as its header, the rest is queued
    if (header->id == Sm2Graphics)
    {
        *size = sizeof(SectionHeader);
    }

    return MakeSectionReader<Sm2SectionReader>(g_Sm2SectionReaderVTable, this, header);
}

void Sm2Reader::Unload()
{
    ChunkData* chunk = ChunkDataOf(loader);
    if (chunk != nullptr)
    {
        SceneryCell* scenery = chunk->scenery;
        if (scenery != nullptr)
        {
            scenery->VirtualDestroy(DestroyAndFree);
        }

        chunk->scenery = nullptr;
    }

    ReleaseGraphicsResources(&graphics);
}

void Sm2Reader::Finish(s32, u32, u32)
{
    ChunkData* chunk = ChunkDataOf(loader);
    ChunkSceneryRead(chunk);
    ChunkListAdd(GetChunkList(), chunk);
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
    case Sm2Scenery:
    {
        auto* section = ScenerySectionReader::Construct(
            static_cast<ScenerySectionReader*>(MemoryAllocate(sizeof(ScenerySectionReader))), data, size);
        MemoryReader* reader = MemoryReader::Construct(static_cast<MemoryReader*>(MemoryAllocate(sizeof(MemoryReader))), section,
                                                       nullptr, 0);
        ReadScenery(ChunkDataOf(loader), section->stream);
        AddItemReaderToReaderStorage(g_ReadersStorages[MainReaders], reader, QueueBack);
        break;
    }
    case Sm2DynamicScenery:
    {
        MemoryStream stream;
        MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
        LoadDynamicScenery(ChunkDataDynamicScenery(ChunkDataOf(loader)), &stream);
        stream.Destroy(DestroyOnly);
        break;
    }
    case Sm2Links:
    {
        MemoryStream stream;
        MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
        LoadChunkLinks(loader->loader, &stream);
        stream.Destroy(DestroyOnly);
        break;
    }
    case Sm2Graphics:
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
    ChunkData* chunk = entry->data != nullptr ? entry->data->chunk : nullptr;
    CollisionData* collision = chunk->collision;
    RetailLibc::MemorySet(&reader->bits, 0, sizeof(reader->bits));
    reader->bits.reading = 1;
    reader->collision = ConstructCollisionHolder(static_cast<CollisionData**>(MemoryAllocate(sizeof(CollisionData*))), collision);
    for (u32 layout = 0; layout < Layouts; layout++)
    {
        reader->layouts[layout] = nullptr;
        reader->instanceItems[layout] = nullptr;
    }

    return reader;
}

void Rm2Reader::Destroy(u32 flags)
{
    vtable = g_Rm2ReaderVTable;
    MemoryDeallocate2_(collision);
    DestroyInstances();
    code.Unload(DestroyOnly);
    UnloadGraphics(&graphics, DestroyOnly);
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

s32 Rm2Reader::SectionType()
{
    return DefaultSectionType;
}

bool Rm2Reader::CanRead(u32 type)
{
    return type == DefaultSectionType;
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
    case Rm2Collision:
        *size = CollisionHeaderSize;
        break;
    case Rm2Code:
    case Rm2Graphics:
        *size = sizeof(SectionHeader);
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
        ChunkDataReference* reference = entry->data;
        ReleaseChunkData(reference != nullptr ? reference->chunk : nullptr, true, true, false);
    }

    ReleaseCodeResources(resources);
    ReleaseGraphicsResources(&graphics);
}

void Rm2Reader::Finish(s32, u32, u32)
{
    // The resources told they were read: the code models' slots set up again
    CallVirtual<void>(resources, resources->vtable, GameResources::SetUpCodeModelsSlot, 0u);
    if (bits.defaultChunk != 0)
    {
        ForgetGraphicsReading(&graphics);
    }
    else
    {
        FinishGraphicsReading(&graphics);
    }

    bits.reading = 0;
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

Rm2Reader* Rm2Reader::ConstructDefault(Rm2Reader* reader, const char* path, GameResources* resources, u32* objects)
{
    reader->vtable = g_Rm2ReaderVTable;
    StringConstruct(&reader->path, path);
    reader->resources = resources;
    reader->objects = objects;
    reader->collision = nullptr;
    InitGraphicsItem(&reader->graphics);
    CodeItem::Construct(&reader->code, resources, objects);
    reader->entry = nullptr;
    RetailLibc::MemorySet(&reader->bits, 0, sizeof(reader->bits));
    reader->bits.defaultChunk = 1;
    reader->bits.reading = 1;
    for (u32 layout = 0; layout < Layouts; layout++)
    {
        reader->layouts[layout] = nullptr;
        reader->instanceItems[layout] = nullptr;
    }

    return reader;
}

void Rm2Reader::DestroyInstances()
{
    for (u32 layout = 0; layout < Layouts; layout++)
    {
        if (layouts[layout] != nullptr)
        {
            layouts[layout]->ReleaseRead();
            if (layouts[layout] != nullptr)
            {
                layouts[layout]->Destroy(DestroyAndFree);
            }
        }

        if (instanceItems[layout] != nullptr)
        {
            instanceItems[layout]->Destroy(DestroyAndFree);
        }

        layouts[layout] = nullptr;
        instanceItems[layout] = nullptr;
    }
}

void Rm2Reader::ReadSection(u32 id, u32 offset, MemoryStream* stream)
{
    switch (id)
    {
    case Rm2Particles:
        ReadParticleData(this, stream);
        return;
    case Rm2Collision:
        QueueCollisionSection(collision, static_cast<s32>(offset));
        return;
    case Rm2Code:
        AddSectionToLoadQueue(&code, offset);
        return;
    case Rm2Graphics:
        AddSectionToLoadQueue(&graphics, offset);
        return;
    default:
        break;
    }

    // The other sections are the layouts' instances
    u32 chunkOwn = id < FirstOtherLayout || id == LastLayout ? 1 : 0;
    auto* layout = static_cast<LayoutInstances*>(MemoryAllocate(sizeof(LayoutInstances)));
    layout = LayoutInstances::Construct(layout, bits.defaultChunk, chunkOwn, resources, entry);
    auto* item = static_cast<InstanceSectionItem*>(MemoryAllocate(sizeof(InstanceSectionItem)));
    item = InstanceSectionItem::Construct(item, layout);
    layouts[id] = layout;
    instanceItems[id] = item;
    AddSectionToLoadQueue(item, offset);
}

void Rm2Reader::FilePath(u32 kind, String* file)
{
    StringAssign(file, path.string);
    switch (kind)
    {
    case FileMa2:
        StringAppend(file, g_Ma2Extension);
        break;
    case FileMc2:
        StringAppend(file, g_Mc2Extension);
        break;
    case FileMe2:
        StringAppend(file, g_Me2Extension);
        break;
    case FileGa2:
        StringAppend(file, g_Ga2Extension);
        break;
    case FileGc2:
        StringAppend(file, g_Gc2Extension);
        break;
    case FileGe2:
        StringAppend(file, g_Ge2Extension);
        break;
    case FileGw2:
        StringAppend(file, g_Gw2Extension);
        break;
    case FileSu2:
        StringAppend(file, g_Su2Extension);
        break;
    case FilePtl:
        StringAppend(file, g_PtlExtension);
        break;
    case FileTri:
        StringAppend(file, g_TriExtension);
        break;
    default:
        break;
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
    MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
    static_cast<Rm2Reader*>(item)->ReadSection(id, offset, &stream);
    stream.Destroy(DestroyOnly);
}

ScenerySectionReader* ScenerySectionReader::Construct(ScenerySectionReader* reader, const u8* data, u32 size)
{
    reader->diskHandle = NoDiskHandle;
    reader->vtable = g_ScenerySectionReaderVTable;
    s32 handle;
    DiskAllocate(&handle, GetDiskManager(), size, true, 0);
    reader->diskHandle = handle;
    u8* copy = DiskMemory(GetDiskManager(), &reader->diskHandle);
    RetailLibc::MemoryCopy(copy, data, size);
    reader->stream =
        MemoryStream::Construct(static_cast<MemoryStream*>(MemoryAllocate(sizeof(MemoryStream))), copy, size, 0,
                                MemoryStream::FileAlignment);
    return reader;
}

void ScenerySectionReader::Destroy(u32 flags)
{
    vtable = g_ScenerySectionReaderVTable;
    if (diskHandle >= 0)
    {
        DiskRelease(GetDiskManager(), &diskHandle);
    }

    if (stream != nullptr)
    {
        stream->Destroy(DestroyAndFree);
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
