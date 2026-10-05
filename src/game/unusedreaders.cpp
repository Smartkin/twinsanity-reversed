#include "game/animation.h"
#include "game/archive.h"
#include "game/graphicstables.h"
#include "game/memory.h"
#include "game/readers.h"
#include "game/stream.h"

// Three section readers nothing makes: no code refers to their vtables (at 0x305678 and 0x305880 in D_00305668, at 0x305938 in
// D_00305900; their third function is the base's file missing). One reads nothing, one reads an OGI into the one it was made
// with, and one's reading destroys the readers of six graphics kinds it was made with

extern "C"
{
    // The six kinds' readers' vtables
    extern const GccVTableEntry g_TextureReaderVTable[] RETAIL(TextureItem_Methods);
    extern const GccVTableEntry g_MaterialReaderVTable[] RETAIL(MaterialItem_Methods);
    extern const GccVTableEntry g_ModelReaderVTable[] RETAIL(ModelsItem_Methods);
    extern const GccVTableEntry g_RigidModelReaderVTable[] RETAIL(RigidModelItem_Methods);
    extern const GccVTableEntry g_SkinReaderVTable[] RETAIL(SkinItem_Methods);
    extern const GccVTableEntry g_BlendSkinReaderVTable[] RETAIL(BlendSkinItem_Methods);
}

namespace
{
// A reader's references let go of: its table's pending IDs released through the table's vtable (GraphicsKindReader::Unload, which
// calls the table's own Release)
template <typename Kind>
void UnloadReader(GraphicsKindReader<Kind>* reader, const GccVTableEntry* vtable)
{
    reader->vtable = vtable;
    GraphicsTable<Kind>* table = reader->table;
    if (table->pending != nullptr)
    {
        for (u32 index = 0; index < table->pending->count; index++)
        {
            CallVirtual<void>(table, table->vtable, GraphicsTable<Kind>::ReleaseSlot,
                              static_cast<const u32*>(&table->pending->ids[index]));
        }

        table->pending = nullptr;
    }

    reader->vtable = g_ItemInterfaceVTable;
}
}

// The readers of six graphics kinds (0x34 bytes, nothing makes it): the graphics item's first six readers, past a word in place
// of its vtable
struct GraphicsReaders
{
    u32 unused00;
    GraphicsKindReader<MaterialKind> materials;
    GraphicsKindReader<TextureKind> textures;
    GraphicsKindReader<ModelKind> models;
    GraphicsKindReader<RigidModelKind> rigidModels;
    GraphicsKindReader<SkinKind> skins;
    GraphicsKindReader<BlendSkinKind> blendSkins;
};
CHECK_OFFSET(GraphicsReaders, blendSkins, 0x2C);
CHECK_SIZE(GraphicsReaders, 0x34);

class EmptySectionReader : public SectionReader
{
public:
    void Destroy(u32 flags) RETAIL(FUN_00299400);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002995d8);
};

class OgiSectionReader : public SectionReader
{
public:
    GameOGI* ogi;

    void Destroy(u32 flags) RETAIL(FUN_00299110);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_002993b8);
};

class GraphicsReadersSectionReader : public SectionReader
{
public:
    GraphicsReaders* graphics;

    void Destroy(u32 flags) RETAIL(FUN_00297d00);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_00299170);
};

extern "C"
{
    // The references the readers' reading took let go of (the blend skins' first, the materials' last)
    void DestroyGraphicsReaders(GraphicsReaders* graphics, u32 destroyFlags) RETAIL(FUN_00299cf8);
}

void EmptySectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void EmptySectionReader::Read(u8*, u32, ReaderStack*)
{
}

void OgiSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void OgiSectionReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
    ReadOgi(ogi, &stream);
    stream.Destroy(DestroyOnly);
}

void GraphicsReadersSectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void GraphicsReadersSectionReader::Read(u8*, u32, ReaderStack*)
{
    if (graphics != nullptr)
    {
        DestroyGraphicsReaders(graphics, DestroyAndFree);
    }
}

void DestroyGraphicsReaders(GraphicsReaders* graphics, u32 destroyFlags)
{
    UnloadReader(&graphics->blendSkins, g_BlendSkinReaderVTable);
    UnloadReader(&graphics->skins, g_SkinReaderVTable);
    UnloadReader(&graphics->rigidModels, g_RigidModelReaderVTable);
    UnloadReader(&graphics->models, g_ModelReaderVTable);
    UnloadReader(&graphics->textures, g_TextureReaderVTable);
    UnloadReader(&graphics->materials, g_MaterialReaderVTable);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(graphics);
    }
}
