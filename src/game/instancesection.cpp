#include "game/instancesection.h"

#include "game/archive.h"
#include "game/chunkdata.h"
#include "game/chunkloading.h"
#include "game/instancefactory.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/objectnode.h"
#include "game/reference.h"
#include "game/resources.h"
#include "game/stream.h"

extern "C"
{
    extern const GccVTableEntry g_InstanceSectionItemVTable[] RETAIL(InstanceSectionItem_Methods);
    extern const GccVTableEntry g_InstanceSectionReaderVTable[] RETAIL(InstanceSectionReader_Methods);
    // The templates' list let go, every template first when asked
    void ClearTemplates(PointerArray<InstanceTemplate>** list, u32 destroyTemplates) RETAIL(FUN_0026b088);
    // The elements made and read from a stream: a position, a path, a trigger made (returned) and read, a camera read,
    // a collision surface read, and a surface's box made (the default box)
    // An object instance's context given the positions and the paths it names as waypoints (a layout that isn't the chunk's own
    // has its positions after the chunk's own; its paths are looked up like the chunk's own)
    void LinkInstancePositions(ObjectInstance* instance, ChunkEntry* chunk, InstanceContext* context, u32 notChunkOwn)
        RETAIL(FUN_0025f448);
    void LinkInstancePaths(ObjectInstance* instance, ChunkEntry* chunk, InstanceContext* context, u32 notChunkOwn) RETAIL(FUN_0025f558);

    // The section readers' functions: the destructors and the reads
    void InstanceSectionReaderDestroy(InstanceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_00269e38);
    void LoadInstanceSection(InstanceSectionReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadInstanceSection);
    void TemplateReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026da38);
    void ObjectInstanceReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026da08);
    void AiPositionReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d9d8);
    void AiPathReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d9a8);
    void PositionReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d978);
    void PathReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d948);
    void TriggerReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d918);
    void CameraReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d8e8);
    void SurfaceReaderDestroy(InstanceKindReader* reader, u32 destroyFlags) RETAIL(FUN_0026d8b8);
    void LoadInstanceTemplate(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadInstanceTemplate);
    void LoadInstance(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadInstance);
    void LoadAiPosition(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_0026e088);
    void LoadAiPath(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_0026e1f0);
    void LoadPosition(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_0026e358);
    void LoadPath(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadPath);
    void LoadTrigger(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadTrigger);
    void LoadCamera(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadCamera);
    void LoadCollisionSurface(InstanceKindReader* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadCollisionSurface);

    // The section item's vtable functions
    void InstanceSectionItemDestroy(InstanceSectionItem* item, u32 destroyFlags) RETAIL(FUN_0026a018);
    u32 InstanceSectionItemCount(InstanceSectionItem* item) RETAIL(GetInstanceSectionSubSectionsAmount);
    u32 InstanceSectionItemSlot3(InstanceSectionItem* item) RETAIL(FUN_0026b598);
    u32 InstanceSectionItemCanRead(InstanceSectionItem* item, u32 type) RETAIL(FUN_0026b5a0);
    InstanceSectionReader* InstanceSectionItemGetReader(InstanceSectionItem* item, s32 index, ItemHeader* header, s32* size)
        RETAIL(GetInstanceSectionReader);
    void InstanceSectionItemSlot6(InstanceSectionItem* item) RETAIL(FUN_0026b5b0);
    void InstanceSectionItemSlot7(InstanceSectionItem* item) RETAIL(FUN_0026b5b8);
    void InstanceSectionItemFinish(InstanceSectionItem* item) RETAIL(FUN_0026b5c0);

    // Each kind's item's vtable functions and its reader's vtable
    extern const GccVTableEntry g_TemplateItemVTable[] RETAIL(InstanceTemplateItem_Methods);
    extern const GccVTableEntry g_TemplateReaderVTable[] RETAIL(D_003048F8);
    void TemplateItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269e68);
    u32 TemplateItemCount(InstanceKindItem* item) RETAIL(FUN_0026c7c8);
    u32 TemplateItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026c7d0);
    u32 TemplateItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026c7d8);
    InstanceKindReader* TemplateItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026c7e8);
    void TemplateItemClear(InstanceKindItem* item) RETAIL(FUN_0026c848);
    void TemplateItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c868);
    extern const GccVTableEntry g_ObjectInstanceItemVTable[] RETAIL(ObjectInstanceItem_Methods);
    extern const GccVTableEntry g_ObjectInstanceReaderVTable[] RETAIL(ObjectInstanceSectionReader_Methods);
    void ObjectInstanceItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269e98);
    u32 ObjectInstanceItemCount(InstanceKindItem* item) RETAIL(GetObjectInstanceAmount);
    u32 ObjectInstanceItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026c610);
    u32 ObjectInstanceItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026c618);
    InstanceKindReader* ObjectInstanceItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(GetObjectInstanceSectionReader);
    void ObjectInstanceItemClear(InstanceKindItem* item) RETAIL(FUN_0026c688);
    void ObjectInstanceItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c760);
    extern const GccVTableEntry g_AiPositionItemVTable[] RETAIL(AiPositionSectionItemReader_Methods);
    extern const GccVTableEntry g_AiPositionReaderVTable[] RETAIL(D_00304948);
    void AiPositionItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269ec8);
    u32 AiPositionItemCount(InstanceKindItem* item) RETAIL(FUN_0026c428);
    u32 AiPositionItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026c430);
    u32 AiPositionItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026c438);
    InstanceKindReader* AiPositionItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026c448);
    void AiPositionItemClear(InstanceKindItem* item) RETAIL(FUN_0026c4a8);
    void AiPositionItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c5a0);
    extern const GccVTableEntry g_AiPathItemVTable[] RETAIL(AiPathSectionItemReader_Methods);
    extern const GccVTableEntry g_AiPathReaderVTable[] RETAIL(D_00304970);
    void AiPathItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269ef8);
    u32 AiPathItemCount(InstanceKindItem* item) RETAIL(FUN_0026c278);
    u32 AiPathItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026c280);
    u32 AiPathItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026c288);
    InstanceKindReader* AiPathItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026c298);
    void AiPathItemClear(InstanceKindItem* item) RETAIL(FUN_0026c2f8);
    void AiPathItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c3c0);
    extern const GccVTableEntry g_PositionItemVTable[] RETAIL(PositionSectionItemReader_Methods);
    extern const GccVTableEntry g_PositionReaderVTable[] RETAIL(D_00304998);
    void PositionItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269f28);
    u32 PositionItemCount(InstanceKindItem* item) RETAIL(FUN_0026c0b8);
    u32 PositionItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026c0c0);
    u32 PositionItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026c0c8);
    InstanceKindReader* PositionItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026c0d8);
    void PositionItemClear(InstanceKindItem* item) RETAIL(FUN_0026c138);
    void PositionItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c210);
    extern const GccVTableEntry g_PathItemVTable[] RETAIL(PathItem_Methods);
    extern const GccVTableEntry g_PathReaderVTable[] RETAIL(D_003049C0);
    void PathItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269f58);
    u32 PathItemCount(InstanceKindItem* item) RETAIL(FUN_0026bef0);
    u32 PathItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026bef8);
    u32 PathItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026bf00);
    InstanceKindReader* PathItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026bf10);
    void PathItemClear(InstanceKindItem* item) RETAIL(FUN_0026bf70);
    void PathItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026c050);
    extern const GccVTableEntry g_TriggerItemVTable[] RETAIL(TriggerItem_Methods);
    extern const GccVTableEntry g_TriggerReaderVTable[] RETAIL(TriggerSectionReader_Methods);
    void TriggerItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269f88);
    u32 TriggerItemCount(InstanceKindItem* item) RETAIL(GetTriggerAmount);
    u32 TriggerItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026bd30);
    u32 TriggerItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026bd38);
    InstanceKindReader* TriggerItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(GetTriggerSectionReader);
    void TriggerItemClear(InstanceKindItem* item) RETAIL(FUN_0026bda8);
    void TriggerItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026be88);
    extern const GccVTableEntry g_CameraItemVTable[] RETAIL(CameraSectionItem_Methods);
    extern const GccVTableEntry g_CameraReaderVTable[] RETAIL(CameraSectionReader_Methods);
    void CameraItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269fb8);
    u32 CameraItemCount(InstanceKindItem* item) RETAIL(GetInstanceSubSectionItemAmount);
    u32 CameraItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026bb68);
    u32 CameraItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026bb70);
    InstanceKindReader* CameraItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(GetCameraSectionReader);
    void CameraItemClear(InstanceKindItem* item) RETAIL(FUN_0026bbe0);
    void CameraItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026bcc0);
    extern const GccVTableEntry g_SurfaceItemVTable[] RETAIL(CollisionSurfaceItem_Methods);
    extern const GccVTableEntry g_SurfaceReaderVTable[] RETAIL(CollisionSurfaceSectionReader_Methods);
    void SurfaceItemDestroy(InstanceKindItem* item, u32 destroyFlags) RETAIL(FUN_00269fe8);
    u32 SurfaceItemCount(InstanceKindItem* item) RETAIL(FUN_0026b9a0);
    u32 SurfaceItemSlot3(InstanceKindItem* item) RETAIL(FUN_0026b9a8);
    u32 SurfaceItemCanRead(InstanceKindItem* item, u32 type) RETAIL(FUN_0026b9b0);
    InstanceKindReader* SurfaceItemGetReader(InstanceKindItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_0026b9c0);
    void SurfaceItemClear(InstanceKindItem* item) RETAIL(FUN_0026ba20);
    void SurfaceItemSetCount(InstanceKindItem* item, u32 count) RETAIL(FUN_0026baf8);
}

namespace
{
// The sections an instance section's items read
constexpr u32 InstanceKindSection = 1;
// The kinds of an instance section, and the size of a kind's section header
constexpr u32 Kinds = 9;
constexpr s32 KindHeaderSize = 0xC;
// Paths' vtables are 8 bytes in, triggers' and cameras' 0x50
constexpr u32 PathVTable = 0x8;
constexpr u32 TriggerVTable = 0x50;
// The node kinds of an object instance, a trigger and a camera
constexpr u32 ObjectNodeKind = 1;
constexpr u32 TriggerNodeKind = 7;
constexpr u32 CameraNodeKind = 8;
// A list made by a loader has room for this many, and grows by as many
constexpr u32 LoadedListGrowth = 10;
// The loaders read their element from a memory stream of the section aligned like this
constexpr u16 ElementAlignment = 1;

void DestroyKindItem(InstanceKindItem* item, u32 destroyFlags)
{
    item->vtable = g_ItemInterfaceVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(item);
    }
}

InstanceKindReader* MakeKindReader(InstanceKindItem* item, s32* size, const GccVTableEntry* vtable)
{
    if (*size == 0)
    {
        return nullptr;
    }

    auto* reader = static_cast<InstanceKindReader*>(MemoryAllocate(sizeof(InstanceKindReader)));
    reader->vtable = vtable;
    reader->layout = item->layout;
    reader->list = item->list;
    reader->count = item->count;
    return reader;
}

// Every element let go of (by the kind's destructor), then the list freed
template <typename Destroy>
void ClearList(InstanceKindItem* item, Destroy destroy)
{
    PointerArray<void>** list = item->list;
    PointerArray<void>* array = *list;
    if (array != nullptr)
    {
        for (s32 index = 0; index >= 0 && static_cast<u32>(index) < array->count; index++)
        {
            destroy(array->data[index]);
        }

        array = *list;
        if (array != nullptr)
        {
            if (array->data != nullptr)
            {
                MemoryDeallocate_(array->data);
            }

            MemoryDeallocate2_(array);
        }
    }

    *list = nullptr;
}

// The count given, and a list made for that many
void SetListCount(InstanceKindItem* item, u32 count)
{
    PointerArray<void>** list = item->list;
    item->count = count;
    if (count == 0)
    {
        return;
    }

    auto* array = static_cast<PointerArray<void>*>(MemoryAllocate(sizeof(PointerArray<void>)));
    array->count = 0;
    array->capacity = count;
    array->growth = count;
    array->data = static_cast<void**>(MemoryAllocate2(count * sizeof(void*)));
    *list = array;
}

void DestroyObjectInstance(void* instance)
{
    if (instance != nullptr)
    {
        static_cast<ObjectInstance*>(instance)->Destroy(DestroyAndFree);
    }
}

void DestroyAiPosition(void* element)
{
    auto* position = static_cast<AiPosition*>(element);
    if (position == nullptr)
    {
        return;
    }

    if (position->links != nullptr)
    {
        MemoryDeallocate_(position->links);
    }

    MemoryDeallocate2_(position);
}

void DestroyAiPath(void* path)
{
    MemoryDeallocate2_(path);
}

void DestroyPosition(void* position)
{
    if (position != nullptr)
    {
        static_cast<LayoutPosition*>(position)->Destroy(DestroyAndFree);
    }
}

void DestroyAt(void* object, u32 vtableOffset)
{
    auto* vtable = *reinterpret_cast<const GccVTableEntry**>(static_cast<u8*>(object) + vtableOffset);
    CallVirtual<void>(object, vtable, 1, u32{DestroyAndFree});
}

void DestroyPath(void* path)
{
    if (path != nullptr)
    {
        DestroyAt(path, PathVTable);
    }
}

void DestroyTrigger(void* trigger)
{
    if (trigger != nullptr)
    {
        DestroyAt(trigger, TriggerVTable);
    }
}

void DestroySurface(void* surface)
{
    if (surface != nullptr)
    {
        MemoryDeallocate2_(surface);
    }
}

void DestroyKindReader(InstanceKindReader* reader, u32 destroyFlags)
{
    reader->vtable = g_SectionReaderVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}

// The element read registered with the layout (its index the count of the kind's list so far) and added to the list, which is
// made with room for 10 when there's none
template <typename Element>
void AddElement(InstanceKindReader* reader, Element* element, void (LayoutInstances::*registerElement)(u32, u32, Element*))
{
    PointerArray<void>* list = *reader->list;
    (reader->layout->*registerElement)(list != nullptr ? list->count : 0, reader->count, element);
    if (*reader->list == nullptr)
    {
        auto* array = static_cast<PointerArray<void>*>(MemoryAllocate(sizeof(PointerArray<void>)));
        array->count = 0;
        array->growth = LoadedListGrowth;
        array->capacity = LoadedListGrowth;
        array->data = static_cast<void**>(MemoryAllocate2(LoadedListGrowth * sizeof(void*)));
        *reader->list = array;
    }

    (*reader->list)->Append(element);
}
}

InstanceSectionItem* InstanceSectionItem::Construct(InstanceSectionItem* item, LayoutInstances* layout)
{
    static const GccVTableEntry* const KindVTables[Kinds] = {g_TemplateItemVTable, g_ObjectInstanceItemVTable, g_AiPositionItemVTable, g_AiPathItemVTable, g_PositionItemVTable, g_PathItemVTable, g_TriggerItemVTable, g_CameraItemVTable, g_SurfaceItemVTable};
    item->vtable = g_InstanceSectionItemVTable;
    item->layout = layout;
    for (u32 kind = 0; kind < Kinds; kind++)
    {
        PointerArray<void>* list = layout->kinds[kind];
        InstanceKindItem& kindItem = item->kinds[kind];
        kindItem.vtable = KindVTables[kind];
        kindItem.layout = layout;
        kindItem.list = &layout->kinds[kind];
        kindItem.count = list != nullptr ? list->count : 0;
    }

    return item;
}

void InstanceSectionItemDestroy(InstanceSectionItem* item, u32 destroyFlags)
{
    item->vtable = g_ItemInterfaceVTable;
    for (s32 kind = Kinds - 1; kind >= 0; kind--)
    {
        item->kinds[kind].vtable = g_ItemInterfaceVTable;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(item);
    }
}

u32 InstanceSectionItemCount(InstanceSectionItem*)
{
    return Kinds;
}

u32 InstanceSectionItemSlot3(InstanceSectionItem*)
{
    return 1;
}

u32 InstanceSectionItemCanRead(InstanceSectionItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceSectionReader* InstanceSectionItemGetReader(InstanceSectionItem* item, s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    u32 section = header->id;
    u32 offset = header->offset;
    if (section < Kinds)
    {
        *size = KindHeaderSize;
    }

    auto* reader = static_cast<InstanceSectionReader*>(MemoryAllocate(sizeof(InstanceSectionReader)));
    reader->item = item;
    reader->vtable = g_InstanceSectionReaderVTable;
    reader->section = section;
    reader->offset = offset;
    return reader;
}

void InstanceSectionItemSlot6(InstanceSectionItem*)
{
}

void InstanceSectionItemSlot7(InstanceSectionItem*)
{
}

void InstanceSectionItemFinish(InstanceSectionItem* item)
{
    item->layout->Finish();
}

void TemplateItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 TemplateItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 TemplateItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 TemplateItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* TemplateItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_TemplateReaderVTable);
}

void TemplateItemClear(InstanceKindItem* item)
{
    ClearTemplates(reinterpret_cast<PointerArray<InstanceTemplate>**>(item->list), 1);
}

void TemplateItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void ObjectInstanceItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 ObjectInstanceItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 ObjectInstanceItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 ObjectInstanceItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* ObjectInstanceItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_ObjectInstanceReaderVTable);
}

void ObjectInstanceItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyObjectInstance);
}

void ObjectInstanceItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void AiPositionItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 AiPositionItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 AiPositionItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 AiPositionItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* AiPositionItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_AiPositionReaderVTable);
}

void AiPositionItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyAiPosition);
}

void AiPositionItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void AiPathItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 AiPathItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 AiPathItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 AiPathItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* AiPathItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_AiPathReaderVTable);
}

void AiPathItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyAiPath);
}

void AiPathItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void PositionItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 PositionItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 PositionItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 PositionItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* PositionItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_PositionReaderVTable);
}

void PositionItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyPosition);
}

void PositionItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void PathItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 PathItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 PathItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 PathItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* PathItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_PathReaderVTable);
}

void PathItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyPath);
}

void PathItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void TriggerItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 TriggerItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 TriggerItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 TriggerItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* TriggerItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_TriggerReaderVTable);
}

void TriggerItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyTrigger);
}

void TriggerItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void CameraItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 CameraItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 CameraItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 CameraItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* CameraItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_CameraReaderVTable);
}

void CameraItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroyTrigger);
}

void CameraItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void SurfaceItemDestroy(InstanceKindItem* item, u32 destroyFlags)
{
    DestroyKindItem(item, destroyFlags);
}

u32 SurfaceItemCount(InstanceKindItem* item)
{
    return item->count;
}

u32 SurfaceItemSlot3(InstanceKindItem*)
{
    return 1;
}

u32 SurfaceItemCanRead(InstanceKindItem*, u32 type)
{
    return type == InstanceKindSection;
}

InstanceKindReader* SurfaceItemGetReader(InstanceKindItem* item, s32, ItemHeader*, s32* size)
{
    return MakeKindReader(item, size, g_SurfaceReaderVTable);
}

void SurfaceItemClear(InstanceKindItem* item)
{
    ClearList(item, DestroySurface);
}

void SurfaceItemSetCount(InstanceKindItem* item, u32 count)
{
    SetListCount(item, count);
}

void InstanceSectionItem::QueueKind(u32 section, u32 start)
{
    // The RM2's sections of a layout: the instance templates, the AI positions, the AI paths, the positions, the paths, the
    // collision surfaces, the object instances, the triggers and the cameras
    static const u8 KindOfSection[Kinds] = {0, 2, 3, 4, 5, 8, 1, 6, 7};
    if (section < Kinds)
    {
        AddSectionToLoadQueue(&kinds[KindOfSection[section]], start);
    }
}

void InstanceSectionReaderDestroy(InstanceSectionReader* reader, u32 destroyFlags)
{
    reader->vtable = g_SectionReaderVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}

void LoadInstanceSection(InstanceSectionReader* reader, u8*, u32, ReaderStack*)
{
    reader->item->QueueKind(reader->section, reader->offset);
}

void TemplateReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void ObjectInstanceReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void AiPositionReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void AiPathReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void PositionReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void PathReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void TriggerReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void CameraReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void SurfaceReaderDestroy(InstanceKindReader* reader, u32 destroyFlags)
{
    DestroyKindReader(reader, destroyFlags);
}

void LoadInstanceTemplate(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* instanceTemplate = static_cast<InstanceTemplate*>(MemoryAllocate(sizeof(InstanceTemplate)));
    instanceTemplate = InstanceTemplate::Construct(instanceTemplate, &stream);
    AddElement(reader, instanceTemplate, &LayoutInstances::RegisterTemplate);
    stream.Destroy(DestroyOnly);
}

void LoadInstance(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    ObjectInstance* instance = ObjectInstance::Construct(static_cast<ObjectInstance*>(MemoryAllocate(sizeof(ObjectInstance))), &stream);
    AddElement(reader, instance, &LayoutInstances::RegisterObjectInstance);
    stream.Destroy(DestroyOnly);
}

void LoadAiPosition(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* position = static_cast<AiPosition*>(MemoryAllocate(sizeof(AiPosition)));
    position->Read(&stream);
    AddElement(reader, position, &LayoutInstances::RegisterAiPosition);
    stream.Destroy(DestroyOnly);
}

void LoadAiPath(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* path = static_cast<AiPath*>(MemoryAllocate(sizeof(AiPath)));
    path->Read(&stream);
    AddElement(reader, path, &LayoutInstances::RegisterAiPath);
    stream.Destroy(DestroyOnly);
}

void LoadPosition(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* position = static_cast<LayoutPosition*>(MemoryAllocate(sizeof(LayoutPosition)));
    position->Read(&stream);
    AddElement(reader, position, &LayoutInstances::RegisterPosition);
    stream.Destroy(DestroyOnly);
}

void LoadPath(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* path = static_cast<LayoutPath*>(MemoryAllocate(sizeof(LayoutPath)));
    path->unknown48 = -1;
    path->vtable = g_LayoutPathVTable;
    path->points = nullptr;
    path->Read(&stream);
    AddElement(reader, path, &LayoutInstances::RegisterPath);
    stream.Destroy(DestroyOnly);
}

void LoadTrigger(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* trigger = static_cast<MessageTrigger*>(MemoryAllocate(sizeof(MessageTrigger)));
    LayoutTrigger::Construct(trigger);
    trigger->vtable = g_MessageTriggerVTable;
    trigger->Read(&stream);
    AddElement(reader, trigger, &LayoutInstances::RegisterTrigger);
    stream.Destroy(DestroyOnly);
}

void LoadCamera(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* camera = static_cast<CameraTrigger*>(MemoryAllocate(sizeof(CameraTrigger)));
    LayoutTrigger::Construct(camera);
    camera->vtable = g_CameraTriggerVTable;
    camera->Read(&stream);
    AddElement(reader, camera, &LayoutInstances::RegisterCamera);
    stream.Destroy(DestroyOnly);
}

void LoadCollisionSurface(InstanceKindReader* reader, u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ElementAlignment);
    auto* surface = static_cast<CollisionSurface*>(MemoryAllocate(sizeof(CollisionSurface)));
    ConstructResourceHeader(surface);
    ContactMessage::Construct(&surface->contact);
    surface->Read(&stream);
    AddElement(reader, surface, &LayoutInstances::RegisterSurface);
    stream.Destroy(DestroyOnly);
}

namespace
{
// A list freed and forgotten, its elements kept (none: forgotten)
void FreeList(PointerArray<void>** list)
{
    PointerArray<void>* array = *list;
    if (array != nullptr)
    {
        if (array->data != nullptr)
        {
            MemoryDeallocate_(array->data);
        }

        MemoryDeallocate2_(array);
    }

    *list = nullptr;
}

// Every element of a list destroyed (none skipped), then the list freed and forgotten (no list: nothing)
template <typename Destroy>
void DestroyListElements(PointerArray<void>** list, Destroy destroy)
{
    PointerArray<void>* array = *list;
    if (array == nullptr)
    {
        *list = nullptr;
        return;
    }

    for (s32 index = 0; index >= 0 && static_cast<u32>(index) < array->count; index++)
    {
        destroy(array->data[index]);
    }

    FreeList(list);
}
}

void LayoutInstances::ReleaseRead()
{
    ClearTemplates(reinterpret_cast<PointerArray<InstanceTemplate>**>(&kinds[KindTemplates]), 0);
    FreeList(&kinds[KindAiPositions]);
    FreeList(&kinds[KindAiPaths]);
    FreeList(&kinds[KindPositions]);
    FreeList(&kinds[KindPaths]);
    FreeList(&kinds[KindSurfaces]);
    DestroyListElements(&kinds[KindObjectInstances], DestroyObjectInstance);
    DestroyListElements(&kinds[KindTriggers], DestroyTrigger);
    DestroyListElements(&kinds[KindCameras], DestroyTrigger);
}

void LayoutInstances::DestroyElements()
{
    ClearTemplates(reinterpret_cast<PointerArray<InstanceTemplate>**>(&kinds[KindTemplates]), 1);
    DestroyListElements(&kinds[KindObjectInstances], DestroyObjectInstance);
    DestroyListElements(&kinds[KindAiPositions], DestroyAiPosition);
    DestroyListElements(&kinds[KindAiPaths], DestroyAiPath);
    DestroyListElements(&kinds[KindPositions], DestroyPosition);
    DestroyListElements(&kinds[KindPaths], DestroyPath);
    DestroyListElements(&kinds[KindTriggers], DestroyTrigger);
    DestroyListElements(&kinds[KindCameras], DestroyTrigger);
    DestroyListElements(&kinds[KindSurfaces], DestroySurface);
}

void LayoutInstances::Destroy(u32 destroyFlags)
{
    DestroyElements();
    for (s32 kind = Kinds - 1; kind >= 0; kind--)
    {
        PointerArray<void>* list = kinds[kind];
        if (list != nullptr)
        {
            if (list->data != nullptr)
            {
                MemoryDeallocate_(list->data);
            }

            MemoryDeallocate2_(list);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

LayoutInstances* LayoutInstances::Construct(LayoutInstances* layout, u32 readerBit0, u32 chunkOwn, GameResources* resources, ChunkEntry* chunk)
{
    for (PointerArray<void>*& list : layout->kinds)
    {
        list = nullptr;
    }

    layout->contexts = nullptr;
    layout->navigation = nullptr;
    layout->positions = nullptr;
    layout->paths = nullptr;
    layout->resources = resources;
    layout->chunk = chunk;
    layout->flags = 0;
    layout->flags = (layout->flags & ~FlagReaderBit0) | (readerBit0 & 1);
    layout->flags = (layout->flags & ~FlagChunkOwn) | (chunkOwn & 1) << 1;
    return layout;
}

void LayoutInstances::Finish()
{
    PointerArray<void>* instances = kinds[1];
    if (navigation != nullptr)
    {
        navigation->Link(g_ChunkManager->pathFinder, chunk->index);
    }

    if (instances != nullptr)
    {
        for (u32 index = 0; index < instances->count; index++)
        {
            auto* instance = static_cast<ObjectInstance*>(instances->data[index]);
            IdArray& linked = instance->instances;
            if (linked.count == 0)
            {
                continue;
            }

            InstanceContext* context = contexts->contexts[index];
            if (context == nullptr)
            {
                continue;
            }

            Agent* agent = static_cast<ObjectNode*>(GetGameNode(&context->nodes, ObjectNodeKind))->agent;
            for (u32 link = 0; link < linked.count; link++)
            {
                LinkToAgent(agent, contexts->contexts[linked.data[link]], 1);
            }
        }
    }

    LayoutContexts* table = contexts;
    if (table != nullptr)
    {
        if (table->contexts != nullptr)
        {
            MemoryDeallocate_(table->contexts);
        }

        MemoryDeallocate2_(table);
    }

    contexts = nullptr;
}

void LayoutInstances::RegisterTemplate(u32, u32, InstanceTemplate* instanceTemplate)
{
    g_InstanceFactory->templates.Append(instanceTemplate);
}

void LayoutInstances::RegisterObjectInstance(u32, u32 count, ObjectInstance* instance)
{
    constexpr u32 CountBits = ContextCountMask << ContextCountShift;
    InstanceFactory* factory = g_InstanceFactory;
    factory->SetFlag2();
    factory->SetFlag3();
    factory->ClearFlag1();
    factory->ClearFlag0();
    factory->creationFlags = (flags & FlagChunkOwn) != 0 ? 0 : InstanceFactory::NotChunkOwn;
    InstanceContext* context = CreateInstanceContext(factory, chunk, instance);
    if (contexts == nullptr)
    {
        auto* table = static_cast<LayoutContexts*>(MemoryAllocate(sizeof(LayoutContexts)));
        table->capacity = count;
        table->contexts = count != 0 ? static_cast<InstanceContext**>(MemoryAllocate2(count * sizeof(InstanceContext*))) : nullptr;
        flags &= ~CountBits;
        contexts = table;
    }

    u32 made = flags >> ContextCountShift & ContextCountMask;
    contexts->contexts[made] = context;
    flags = (flags & ~CountBits) | ((made + 1) & ContextCountMask) << ContextCountShift;
    u32 notChunkOwn = (flags & FlagChunkOwn) == 0 ? 1 : 0;
    LinkInstancePositions(instance, chunk, context, notChunkOwn);
    LinkInstancePaths(instance, chunk, context, notChunkOwn);
    factory->creationFlags = 0;
}

void LayoutInstances::RegisterAiPosition(u32 index, u32 count, AiPosition* position)
{
    PathFinder* pathFinder = g_ChunkManager->pathFinder;
    if (pathFinder != nullptr)
    {
        if (navigation == nullptr)
        {
            ChunkEntry* entry = chunk;
            AiNavigation* made = AiNavigation::Construct(static_cast<AiNavigation*>(MemoryAllocate(sizeof(AiNavigation))), pathFinder);
            entry->navigation = made;
            navigation = made;
        }

        if (navigation->positionCount == 0)
        {
            navigation->SetPositionCount(count);
        }
    }

    navigation->SetPosition(index, position);
}

void LayoutInstances::RegisterAiPath(u32 index, u32 count, AiPath* path)
{
    PathFinder* pathFinder = g_ChunkManager->pathFinder;
    if (pathFinder != nullptr)
    {
        if (navigation == nullptr)
        {
            ChunkEntry* entry = chunk;
            AiNavigation* made = AiNavigation::Construct(static_cast<AiNavigation*>(MemoryAllocate(sizeof(AiNavigation))), pathFinder);
            entry->navigation = made;
            navigation = made;
        }

        if (navigation->pathCount == 0)
        {
            navigation->SetPathCount(count);
        }
    }

    navigation->SetPath(index, path);
}

namespace
{
// A list of a chunk's made for a count (room for the count, grown by as many)
PointerArray<void>* MakeChunkList(u32 count)
{
    auto* list = static_cast<PointerArray<void>*>(MemoryAllocate(sizeof(PointerArray<void>)));
    list->count = 0;
    list->capacity = count;
    list->growth = count;
    list->data = static_cast<void**>(MemoryAllocate2(count * sizeof(void*)));
    return list;
}
}

void LayoutInstances::RegisterPosition(u32, u32 count, LayoutPosition* position)
{
    if (positions == nullptr)
    {
        ChunkEntry* entry = chunk;
        if (entry->positions == nullptr)
        {
            entry->positions = MakeChunkList(count);
        }

        positions = entry->positions;
        if ((flags & FlagChunkOwn) != 0)
        {
            chunk->positionCount = count;
        }
    }

    positions->Append(position);
}

void LayoutInstances::RegisterPath(u32, u32 count, LayoutPath* path)
{
    if (paths == nullptr)
    {
        ChunkEntry* entry = chunk;
        if (entry->paths == nullptr)
        {
            entry->paths = MakeChunkList(count);
        }

        paths = entry->paths;
    }

    paths->Append(path);
}

void LayoutInstances::RegisterTrigger(u32, u32, MessageTrigger* trigger)
{
    if (trigger->Kind() == 0)
    {
        SoundBox* box = SoundBox::Construct(static_cast<SoundBox*>(MemoryAllocate(sizeof(SoundBox))), trigger);
        Reference* data = chunk->data;
        ChunkData::AddSoundBox(data != nullptr ? static_cast<ChunkData*>(static_cast<void*>(data->object)) : nullptr, box);
        return;
    }

    InstanceContext* context = CreateTriggerContext(g_InstanceFactory, chunk, trigger);
    auto* node = static_cast<TriggerNode*>(GetGameNode(&context->nodes, TriggerNodeKind));
    for (u32 index = 0; index < trigger->instances.count; index++)
    {
        node->AddInstance(contexts->contexts[trigger->instances.data[index]]);
    }
}

void LayoutInstances::RegisterCamera(u32, u32, CameraTrigger* camera)
{
    InstanceFactory* factory = g_InstanceFactory;
    factory->creationFlags = (flags & FlagChunkOwn) != 0 ? 0 : InstanceFactory::NotChunkOwn;
    InstanceContext* context = CreateCameraContext(factory, chunk, camera);
    auto* node = static_cast<TriggerNode*>(GetGameNode(&context->nodes, CameraNodeKind));
    for (u32 index = 0; index < camera->instances.count; index++)
    {
        node->AddInstance(contexts->contexts[camera->instances.data[index]]);
    }

    factory->creationFlags = 0;
}

void LayoutInstances::RegisterSurface(u32, u32, CollisionSurface* surface)
{
    g_CollisionSurfaces.Add(surface);
}

void ClearTemplates(PointerArray<InstanceTemplate>** list, u32 destroyTemplates)
{
    if (destroyTemplates != 0)
    {
        PointerArray<InstanceTemplate>* templates = *list;
        if (templates == nullptr)
        {
            *list = nullptr;
            return;
        }

        for (s32 index = 0; index >= 0 && static_cast<u32>(index) < templates->count; index++)
        {
            InstanceTemplate* instanceTemplate = templates->data[index];
            if (instanceTemplate == nullptr)
            {
                continue;
            }

            instanceTemplate->properties.Destroy(DestroyOnly);
            if (instanceTemplate->starters.data != nullptr)
            {
                DeleteArray(instanceTemplate->starters.data);
            }

            StringDestroy(&instanceTemplate->name);
            MemoryDeallocate2_(instanceTemplate);
        }
    }

    PointerArray<InstanceTemplate>* templates = *list;
    if (templates != nullptr)
    {
        if (templates->data != nullptr)
        {
            MemoryDeallocate_(templates->data);
        }

        MemoryDeallocate2_(templates);
    }

    *list = nullptr;
}

void LinkInstancePositions(ObjectInstance* instance, ChunkEntry* chunk, InstanceContext* context, u32 notChunkOwn)
{
    IdArray& ids = instance->positions;
    if (ids.count == 0)
    {
        return;
    }

    Waypoints* waypoints = static_cast<ObjectNode*>(GetGameNode(&context->nodes, ObjectNodeKind))->waypoints;
    for (u32 index = 0; index < ids.count; index++)
    {
        u16 id = ids.data[index];
        void* position;
        if (notChunkOwn != 0)
        {
            position = chunk->OtherLayoutPosition(id);
        }
        else
        {
            position = chunk->positions != nullptr ? chunk->positions->data[id] : nullptr;
        }

        waypoints->AddPosition(static_cast<LayoutPosition*>(position));
    }
}

void LinkInstancePaths(ObjectInstance* instance, ChunkEntry* chunk, InstanceContext* context, u32)
{
    IdArray& ids = instance->paths;
    if (ids.count == 0)
    {
        return;
    }

    Waypoints* waypoints = static_cast<ObjectNode*>(GetGameNode(&context->nodes, ObjectNodeKind))->waypoints;
    for (u32 index = 0; index < ids.count; index++)
    {
        u16 id = ids.data[index];
        waypoints->AddPath(static_cast<LayoutPath*>(chunk->paths != nullptr ? chunk->paths->data[id] : nullptr));
    }
}
