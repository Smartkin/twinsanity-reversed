#include "game/resources.h"

#include "game/graphicstables.h"

#include "game/agentlab.h"
#include "game/animation.h"

#include "game/chunkfiles.h"
#include "game/controllers.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/sound.h"
#include "game/stream.h"
#include "game/string.h"
#include "platform/graphics.h"
#include "retail/libc.h"

extern "C"
{
    // The code kinds' readers' section readers, and each reader's vtable functions
    extern const GccVTableEntry g_ObjectSectionReaderVTable[] RETAIL(ObjectsSectionReader_Methods);
    void ObjectReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_00268438);
    u32 ObjectReaderCount(CodeKindReader* reader) RETAIL(GetSubItemsAmount_0026D168);
    u32 ObjectReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0026d1c0);
    u32 ObjectReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0026d1c8);
    void* ObjectReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetObjectSectionReader);
    void ObjectReaderClear(CodeKindReader* reader) RETAIL(FUN_0026d2a0);
    void ObjectReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0026d2d8);
    void ObjectReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0026d318);
    extern const GccVTableEntry g_BehaviourSectionReaderVTable[] RETAIL(ScriptSubItemReader_Methods);
    void BehaviourReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_00268468);
    u32 BehaviourReaderCount(CodeKindReader* reader) RETAIL(GetSubSectionAmount_0026CF80);
    u32 BehaviourReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0026cfd8);
    u32 BehaviourReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0026cfe0);
    void* BehaviourReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetScriptSectionItemReader);
    void BehaviourReaderClear(CodeKindReader* reader) RETAIL(FUN_0026d0b8);
    void BehaviourReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0026d0f0);
    void BehaviourReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0026d130);
    extern const GccVTableEntry g_AnimationSectionReaderVTable[] RETAIL(AnimationSectionReader_methods);
    void AnimationReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_00268498);
    u32 AnimationReaderCount(CodeKindReader* reader) RETAIL(GetSubItemsAmount_0026CD98);
    u32 AnimationReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0026cdf0);
    u32 AnimationReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0026cdf8);
    void* AnimationReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetAnimationItemReader);
    void AnimationReaderClear(CodeKindReader* reader) RETAIL(FUN_0026ced0);
    void AnimationReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0026cf08);
    void AnimationReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0026cf48);
    extern const GccVTableEntry g_ModelSectionReaderVTable[] RETAIL(OgiReader_Methods);
    void ModelReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_002684c8);
    u32 ModelReaderCount(CodeKindReader* reader) RETAIL(GetOgiSubItemsAmount);
    u32 ModelReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0026cc08);
    u32 ModelReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0026cc10);
    void* ModelReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetOgiItemReader);
    void ModelReaderClear(CodeKindReader* reader) RETAIL(FUN_0026cce8);
    void ModelReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0026cd20);
    void ModelReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0026cd60);
    extern const GccVTableEntry g_CodeModelSectionReaderVTable[] RETAIL(D_003048A8);
    void CodeModelReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_002684f8);
    u32 CodeModelReaderCount(CodeKindReader* reader) RETAIL(GetCodeModelSubItemsAmount);
    u32 CodeModelReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0026ca20);
    u32 CodeModelReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0026ca28);
    void* CodeModelReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetCodeModelSectionReader);
    void CodeModelReaderClear(CodeKindReader* reader) RETAIL(FUN_0026cb00);
    void CodeModelReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0026cb38);
    void CodeModelReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0026cb78);
    extern const GccVTableEntry g_SoundSectionReaderVTable[] RETAIL(SoundSectionReader_Methods);
    void SoundReaderDestroy(CodeKindReader* reader, u32 destroyFlags) RETAIL(FUN_00179538);
    u32 SoundReaderCount(CodeKindReader* reader) RETAIL(GetSubItemsAmount);
    u32 SoundReaderSlot3(CodeKindReader* reader) RETAIL(FUN_0017c4e8);
    u32 SoundReaderCanRead(CodeKindReader* reader, u32 type) RETAIL(FUN_0017c4f0);
    void* SoundReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetSoundItemSectionReader);
    void SoundReaderClear(CodeKindReader* reader) RETAIL(FUN_0017c5c8);
    void SoundReaderSetCount(CodeKindReader* reader, u32 count) RETAIL(FUN_0017c5f8);
    void SoundReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end) RETAIL(FUN_0017c630);
}

extern "C"
{
    // The tables' vtables, each kind's own and its base's
    extern const GccVTableEntry g_ObjectTableVTable[] RETAIL(D_00304708);
    extern const GccVTableEntry g_ObjectTableBaseVTable[] RETAIL(D_00304738);
    extern const GccVTableEntry g_ScriptTableVTable[] RETAIL(D_003046A8);
    extern const GccVTableEntry g_ScriptTableBaseVTable[] RETAIL(D_003046D8);
    extern const GccVTableEntry g_AnimationTableVTable[] RETAIL(D_00304648);
    extern const GccVTableEntry g_AnimationTableBaseVTable[] RETAIL(D_00304678);
    extern const GccVTableEntry g_ModelTableVTable[] RETAIL(D_003045E8);
    extern const GccVTableEntry g_ModelTableBaseVTable[] RETAIL(D_00304618);
    extern const GccVTableEntry g_CodeModelTableVTable[] RETAIL(D_00304588);
    extern const GccVTableEntry g_CodeModelTableBaseVTable[] RETAIL(D_003045B8);
    extern const GccVTableEntry g_SoundTableVTable[] RETAIL(GameObjectResourceTable_Methods);
    extern const GccVTableEntry g_SoundTableBaseVTable[] RETAIL(D_00304558);
    extern const GccVTableEntry g_ResourcesBaseVTable[] RETAIL(GameResourcesPrototype_Methods);
    extern const GccVTableEntry g_ResourcesVTable[] RETAIL(GameResources_Methods);
    // The game's languages
    extern const u8 g_ResourceLanguages[] RETAIL(D_002F4960);
    // The code section's item and its readers of each kind
    extern const GccVTableEntry g_CodeItemVTable[] RETAIL(CodeSectionItemReader_Methods);
    extern const GccVTableEntry g_ObjectReaderVTable[] RETAIL(ObjectsItemFunctions);
    extern const GccVTableEntry g_BehaviourReaderVTable[] RETAIL(BehavioursItemReader_Methods);
    extern const GccVTableEntry g_AnimationReaderVTable[] RETAIL(AnimationItemReader_methods);
    extern const GccVTableEntry g_ModelReaderVTable[] RETAIL(OgiItemReader_Methods);
    extern const GccVTableEntry g_CodeModelReaderVTable[] RETAIL(CodeModelItemReader_methods);
    extern const GccVTableEntry g_CodeSubsectionReaderVTable[] RETAIL(CodeSubSectionReader_Methods);
    // The code item's vtable slot 5: the reader of a subsection (none when the item's size is 0; the subsections it reads get
    // their header's size, the voices of other languages none)
    SectionReader* CodeItemGetReader(CodeItem* item, s32 index, ItemHeader* header, s32* size) RETAIL(FUN_00266540);
    // The graphics tables' static deletion queues
    extern DeletionQueue g_MaterialQueue RETAIL(D_003B6F20);
    extern DeletionQueue g_TextureQueue RETAIL(D_003B7B20);
    extern DeletionQueue g_ModelQueue RETAIL(D_003B8720);
    extern DeletionQueue g_RigidModelQueue RETAIL(D_003B9320);
    extern DeletionQueue g_SkinQueue RETAIL(D_003B9F20);
    extern DeletionQueue g_BlendSkinQueue RETAIL(D_003BAB20);
    extern DeletionQueue g_SkyQueue RETAIL(D_003BB720);
    extern DeletionQueue g_MeshQueue RETAIL(D_003BC320);
    extern DeletionQueue g_LodQueue RETAIL(D_003BCF20);
    // Zeroed with the queues and never read
    extern u32 g_ResourceTablesUnused RETAIL(D_0030A990);
    // The static initialisation of the queues (GCC 2.9x's: when initialise is 1 and the priority 0xFFFF), and its constructor
    void InitResourceQueues(s32 initialise, s32 priority) RETAIL(FUN_00267970);
    void ResourceQueuesStaticInit() RETAIL(FUN_0026eb60);

    // Every table's vtable functions (each kind's and its base's destructor, the clear, the two others)
    void ObjectTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267a40);
    void ObjectTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267ba0);
    void ObjectTableClear(ResourceTable* table) RETAIL(FUN_00267b18);
    void ScriptTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267c78);
    void ScriptTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00269850);
    void ScriptTableClear(ResourceTable* table) RETAIL(FUN_002697b8);
    void ScriptTableResolve(ResourceTable* table, u32 count) RETAIL(FUN_00269940);
    void AnimationTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267d60);
    void AnimationTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267ec0);
    void AnimationTableClear(ResourceTable* table) RETAIL(FUN_00267e38);
    void ModelTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00267f98);
    void ModelTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_002680f8);
    void ModelTableClear(ResourceTable* table) RETAIL(FUN_00268070);
    void CodeModelTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_002681d0);
    void CodeModelTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00268330);
    void CodeModelTableClear(ResourceTable* table) RETAIL(FUN_002682a8);
    void SoundTableDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00269bf0);
    void SoundTableBaseDestroy(ResourceTable* table, u32 destroyFlags) RETAIL(FUN_00269a50);
    void SoundTableClear(ResourceTable* table) RETAIL(FUN_00269cc8);
    void SoundTableBaseClear(ResourceTable* table) RETAIL(FUN_002699c8);
    // The vtables' functions that do nothing
    void ObjectTableNothing3() RETAIL(FUN_0026d2d0);
    void ObjectTableNothing4() RETAIL(FUN_0026d310);
    void ScriptTableNothing3() RETAIL(FUN_0026d0e8);
    void ScriptTableBaseNothing4() RETAIL(FUN_0026d128);
    void AnimationTableNothing3() RETAIL(FUN_0026cf00);
    void AnimationTableNothing4() RETAIL(FUN_0026cf40);
    void ModelTableNothing3() RETAIL(FUN_0026cd18);
    void ModelTableNothing4() RETAIL(FUN_0026cd58);
    void CodeModelTableNothing3() RETAIL(FUN_0026cb30);
    void CodeModelTableNothing4() RETAIL(FUN_0026cb70);
    void SoundTableNothing3() RETAIL(FUN_0026c9a8);
    void SoundTableBaseNothing4() RETAIL(FUN_0026c9b8);
    // The resources' base's vtable slots 2 and 3 do nothing
    void ResourcesBaseNothing2() RETAIL(FUN_002685c0);
    void ResourcesBaseNothing3() RETAIL(FUN_002685c8);
    // A sound table's slot 4: the samples of the sounds the section read (by the order they were read in) queued on the first
    // readers' storage, each at its offset past where the section ended
    void SoundTableQueueSamples(ResourceTable* table, s32 read, u32 count, u32 end) RETAIL(FUN_00269d50);
}

namespace
{
// A table's size
constexpr u32 TableSize = 0x18;
// A script's vtable is 0x18 bytes in: its destructor, and what a table's step 4 tells each script added (with the table)
constexpr u32 ScriptVTableOffset = 0x18;
constexpr u32 ScriptDestroySlot = 1;
constexpr u32 ScriptResolveSlot = 3;
// A table's vtable function that lets every resource go, and its destructor
constexpr u32 TableClearSlot = 2;
constexpr u32 TableDestroySlot = 1;

// A table made with its base's vtable and a capacity, every resource none
void ConstructTable(ResourceTable* table, const GccVTableEntry* base, u32 capacity, bool bit15)
{
    table->vtable = base;
    table->unknown0C = 0;
    table->queue = nullptr;
    RetailLibc::MemorySet(table, 0, 4);
    u32 bits = (table->bits & ~ResourceTable::CapacityMask) | (capacity & ResourceTable::CapacityMask);
    table->bits = bit15 ? bits | ResourceTable::Bit15 : bits & ~ResourceTable::Bit15;
    table->items = static_cast<void**>(MemoryAllocate2(capacity * sizeof(void*)));
    table->order = static_cast<u16*>(MemoryAllocate2(capacity * sizeof(u16)));
    for (u32 index = 0; index < capacity; index++)
    {
        table->items[index] = nullptr;
    }
}

ResourceTable* MakeTable(const GccVTableEntry* base, u32 capacity, bool bit15)
{
    auto* table = static_cast<ResourceTable*>(MemoryAllocate(TableSize));
    ConstructTable(table, base, capacity, bit15);
    return table;
}

// A table's deletion queue made
DeletionQueue* MakeQueue()
{
    auto* queue = static_cast<DeletionQueue*>(MemoryAllocate(sizeof(DeletionQueue)));
    queue->head = 0;
    queue->count = 0;
    return queue;
}

// Every resource let go of
template <typename Release>
void ReleaseAll(ResourceTable* table, Release release)
{
    for (u32 index = 0; index < table->Capacity(); index++)
    {
        void* item = table->items[index];
        if (item != nullptr)
        {
            release(item);
            table->items[index] = nullptr;
        }
    }
}

// A table's destructor: its base's vtable, every resource let go of, its arrays freed
template <typename Release>
void DestroyTable(ResourceTable* table, u32 destroyFlags, const GccVTableEntry* base, Release release)
{
    table->vtable = base;
    ReleaseAll(table, release);
    if (table->items != nullptr)
    {
        MemoryDeallocate_(table->items);
    }

    if (table->order != nullptr)
    {
        MemoryDeallocate_(table->order);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(table);
    }
}

void ReleaseObject(void* object)
{
    static_cast<GameObject*>(object)->Destroy(DestroyAndFree);
}

void ReleaseScript(void* script)
{
    auto* vtable = *reinterpret_cast<const GccVTableEntry**>(static_cast<u8*>(script) + ScriptVTableOffset);
    CallVirtual<void>(script, vtable, ScriptDestroySlot, u32{DestroyAndFree});
}

void ReleaseAnimation(void* animation)
{
    DestroyAnimation(static_cast<GameAnimation*>(animation), DestroyAndFree);
}

void ReleaseOgi(void* model)
{
    DestroyOgi(static_cast<GameOGI*>(model), DestroyAndFree);
}

void ReleaseCodeModel(void* codeModel)
{
    static_cast<CodeModel*>(codeModel)->Destroy(DestroyAndFree);
}

void ReleaseSound(void* sound)
{
    static_cast<GameSound*>(sound)->Destroy(DestroyAndFree);
}

// A table destroyed (none: nothing)
void DestroyResourceTable(ResourceTable* table)
{
    if (table != nullptr)
    {
        CallVirtual<void>(table, table->vtable, TableDestroySlot, u32{DestroyAndFree});
    }
}

// Everything waiting in a queue deleted, then the queue freed (none: nothing)
template <typename Release>
void EmptyQueue(DeletionQueue* queue, Release release)
{
    if (queue == nullptr)
    {
        return;
    }

    while (queue->DeleteFirst(release))
    {
    }

    MemoryDeallocate2_(queue);
}

// A queue's oldest deleted (none: nothing). Whether there was one
template <typename Release>
u32 DeleteOldest(DeletionQueue* queue, Release release)
{
    return queue != nullptr && queue->DeleteFirst(release);
}

// A graphics table's queue's oldest deleted by the platform. Whether there was one
template <typename Item>
u32 DeleteOldestGraphics(DeletionQueue* queue, void (*deleteItem)(Item*))
{
    return queue->DeleteFirst([deleteItem](void* item) { deleteItem(static_cast<Item*>(item)); });
}

// The game object of an ID (the ID 0xFFFF none)
GameObject* ObjectOf(GameResources* resources, u16 id)
{
    if (id == 0xFFFF)
    {
        return nullptr;
    }

    return static_cast<GameObject*>(resources->objects->items[id & 0x7FFF]);
}
}

namespace
{
// The sections the code kinds' readers read
constexpr u32 CodeKindSection = 1;
// The code section's subsections (game/chunkfiles.h's CodeSubsectionReader): kind 5 has none, the voices of the language
// played are read, and a subsection's reader reads its header
constexpr u32 CodeKindNone = 5;
constexpr u32 CodeKindVoices = 7;
constexpr u32 CodeKindCount = 13;
constexpr s32 CodeSubsectionHeaderSize = 0xC;
// The tables' vtable functions the readers pass on to: the clear, the count of resources to come, the count read
constexpr u32 TableSetCountSlot = 3;
constexpr u32 TableFinishSlot = 4;

void DestroyReader(CodeKindReader* reader, u32 destroyFlags)
{
    reader->vtable = g_ItemInterfaceVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}

// How many resources its table has
u32 CountResources(CodeKindReader* reader)
{
    ResourceTable* table = reader->table;
    if (table == nullptr)
    {
        return 0;
    }

    u32 count = 0;
    u32 capacity = table->Capacity();
    for (u32 index = 0; index < capacity; index++)
    {
        if (table->items[index] != nullptr)
        {
            count++;
        }
    }

    return count;
}

// A section reader for a resource its table hasn't got (a resource it has is marked kept again)
void* ResourceReaderFor(CodeKindReader* reader, s32 index, ItemHeader* header, const GccVTableEntry* vtable)
{
    ResourceTable* table = reader->table;
    if (table == nullptr)
    {
        return nullptr;
    }

    u32 id = header->id;
    u16 shortId = static_cast<u16>(id);
    void* existing = shortId != 0xFFFF ? table->items[shortId & 0x7FFF] : nullptr;
    if (existing == nullptr)
    {
        auto* made = static_cast<ResourceSectionReader*>(MemoryAllocate(sizeof(ResourceSectionReader)));
        made->id = id;
        made->vtable = vtable;
        made->index = index;
        made->table = reader->table;
        made->objects = reader->objects;
        return made;
    }

    u32& bits = HeaderOf(existing)->bits;
    if ((bits & ResourceHeader::Kept) == 0)
    {
        bits |= ResourceHeader::Kept;
    }

    return nullptr;
}

void ClearTable(CodeKindReader* reader)
{
    ResourceTable* table = reader->table;
    CallVirtual<void>(table, table->vtable, TableClearSlot);
}

void TableSetCount(CodeKindReader* reader, u32 count)
{
    ResourceTable* table = reader->table;
    if (table != nullptr)
    {
        CallVirtual<void>(table, table->vtable, TableSetCountSlot, count);
    }
}

void TableFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    ResourceTable* table = reader->table;
    if (table != nullptr)
    {
        CallVirtual<void>(table, table->vtable, TableFinishSlot, read, count, end);
    }
}
}

void InitResourceQueues(s32 initialise, s32 priority)
{
    if (priority != 0xFFFF || initialise == 0)
    {
        return;
    }

    DeletionQueue* const queues[] = {&g_MaterialQueue, &g_TextureQueue, &g_ModelQueue, &g_RigidModelQueue, &g_SkinQueue,
                                     &g_BlendSkinQueue, &g_SkyQueue, &g_MeshQueue, &g_LodQueue};
    for (DeletionQueue* queue : queues)
    {
        queue->head = 0;
        queue->count = 0;
    }

    g_ResourceTablesUnused = 0;
}

void ResourceQueuesStaticInit()
{
    InitResourceQueues(1, 0xFFFF);
}

GameResources* GameResources::ConstructBase(GameResources* resources, u32 languageCount, const void* languages)
{
    resources->vtable = g_ResourcesBaseVTable;
    resources->languageCount = languageCount;
    resources->languages = languages;
    g_SkinTable.queue = &g_SkinQueue;
    g_BlendSkinTable.queue = &g_BlendSkinQueue;
    g_SkyTable.queue = &g_SkyQueue;
    g_MeshTable.queue = &g_MeshQueue;
    g_LodTable.queue = &g_LodQueue;
    g_MaterialTable.queue = &g_MaterialQueue;
    g_TextureTable.queue = &g_TextureQueue;
    g_ModelTable.queue = &g_ModelQueue;
    g_RigidModelTable.queue = &g_RigidModelQueue;

    resources->objects = nullptr;
    resources->scripts = nullptr;
    resources->animations = nullptr;
    resources->models = nullptr;
    resources->codeModels = nullptr;
    resources->sounds = nullptr;
    resources->voices = nullptr;
    resources->objectQueue = nullptr;
    resources->scriptQueue = nullptr;
    resources->animationQueue = nullptr;
    resources->modelQueue = nullptr;
    resources->codeModelQueue = nullptr;
    resources->soundQueue = nullptr;
    resources->voiceQueues = nullptr;
    return resources;
}

GameResources* GameResources::Construct(GameResources* resources)
{
    ConstructBase(resources, 5, g_ResourceLanguages);
    resources->vtable = g_ResourcesVTable;
    resources->MakeObjectTable(1500);
    resources->MakeAnimationTable(2500);
    resources->MakeScriptTable(8000);
    resources->MakeCodeModelTable(200);
    resources->MakeModelTable(1500);
    resources->MakeSoundTable(2000);
    resources->MakeVoiceTables(2000);
    G_GameResourcesObjectPointer = resources;
    return resources;
}

ResourceTable* GameResources::MakeObjectTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_ObjectTableBaseVTable, capacity, false);
    objects = table;
    table->vtable = g_ObjectTableVTable;
    DeletionQueue* queue = MakeQueue();
    objectQueue = queue;
    objects->queue = queue;
    return objects;
}

ResourceTable* GameResources::MakeScriptTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_ScriptTableBaseVTable, capacity, true);
    table->vtable = g_ScriptTableVTable;
    // Retail allocates the order again, the first one let go of nowhere
    auto* order = static_cast<u16*>(MemoryAllocate2(capacity * sizeof(u16)));
    scripts = table;
    table->order = order;
    DeletionQueue* queue = MakeQueue();
    scriptQueue = queue;
    scripts->queue = queue;
    g_ScriptTable = scripts;
    return scripts;
}

ResourceTable* GameResources::MakeAnimationTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_AnimationTableBaseVTable, capacity, true);
    animations = table;
    table->vtable = g_AnimationTableVTable;
    DeletionQueue* queue = MakeQueue();
    animationQueue = queue;
    animations->queue = queue;
    return animations;
}

ResourceTable* GameResources::MakeCodeModelTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_CodeModelTableBaseVTable, capacity, true);
    codeModels = table;
    table->vtable = g_CodeModelTableVTable;
    DeletionQueue* queue = MakeQueue();
    codeModelQueue = queue;
    codeModels->queue = queue;
    return codeModels;
}

ResourceTable* GameResources::MakeModelTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_ModelTableBaseVTable, capacity, true);
    models = table;
    table->vtable = g_ModelTableVTable;
    DeletionQueue* queue = MakeQueue();
    modelQueue = queue;
    models->queue = queue;
    return models;
}

ResourceTable* GameResources::MakeSoundTable(u32 capacity)
{
    ResourceTable* table = MakeTable(g_SoundTableBaseVTable, capacity, true);
    table->vtable = g_SoundTableVTable;
    auto* order = static_cast<u16*>(MemoryAllocate2(capacity * sizeof(u16)));
    sounds = table;
    table->order = order;
    DeletionQueue* queue = MakeQueue();
    soundQueue = queue;
    sounds->queue = queue;
    return sounds;
}

ResourceTable** GameResources::MakeVoiceTables(u32 capacity)
{
    voices = static_cast<ResourceTable**>(MemoryAllocate2(languageCount * sizeof(ResourceTable*)));
    voiceQueues = static_cast<DeletionQueue**>(MemoryAllocate2(languageCount * sizeof(DeletionQueue*)));
    for (u32 language = 0; language < languageCount; language++)
    {
        ResourceTable* table = MakeTable(g_SoundTableBaseVTable, capacity, true);
        table->vtable = g_SoundTableVTable;
        table->order = static_cast<u16*>(MemoryAllocate2(capacity * sizeof(u16)));
        voices[language] = table;
        voiceQueues[language] = MakeQueue();
        voices[language]->queue = voiceQueues[language];
    }

    return voices;
}

// The ID 0xFFFF has no object: retail reads its references at address 0x24
void GameResources::TakeObjects(const u32* objects)
{
    const u16* ids = reinterpret_cast<const u16*>(objects + 1);
    for (u32 index = 0; index < objects[0]; index++)
    {
        u16 id = ids[index];
        LoadObjectResources(ObjectOf(this, id)->references, &id, this);
    }
}

void GameResources::ReleaseObjects(const u32* objects)
{
    const u16* ids = reinterpret_cast<const u16*>(objects + 1);
    for (u32 index = 0; index < objects[0]; index++)
    {
        u16 id = ids[index];
        ReleaseObjectResources(ObjectOf(this, id)->references, &id, this);
    }
}

CodeItem* CodeItem::Construct(CodeItem* item, GameResources* resources, u32* objects)
{
    item->vtable = g_CodeItemVTable;
    item->resources = resources;
    item->objects = {g_ObjectReaderVTable, resources->objects, objects};
    item->behaviours = {g_BehaviourReaderVTable, resources->scripts, nullptr};
    item->animations = {g_AnimationReaderVTable, resources->animations, nullptr};
    item->models = {g_ModelReaderVTable, resources->models, nullptr};
    item->codeModels = {g_CodeModelReaderVTable, resources->codeModels, nullptr};
    item->sounds = {g_SoundTableItemVTable, resources->sounds, nullptr};
    for (u32 language = 0; language < 6; language++)
    {
        ResourceTable** voices = resources->voices;
        ResourceTable* table = nullptr;
        if (voices != nullptr && language < resources->languageCount)
        {
            table = voices[language];
        }

        item->voices[language] = {g_SoundTableItemVTable, table, nullptr};
    }

    return item;
}

void CodeItem::Unload(u32 destroyFlags)
{
    vtable = g_ItemInterfaceVTable;
    CodeKindReader* const readers[] = {&voices[5], &voices[4], &voices[3], &voices[2], &voices[1], &voices[0], &sounds,
                                       &codeModels, &models, &animations, &behaviours, &objects};
    for (CodeKindReader* reader : readers)
    {
        reader->vtable = g_ItemInterfaceVTable;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

SectionReader* CodeItemGetReader(CodeItem* item, s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    u32 voices = CodeKindVoices + g_CurrentLanguage;
    u32 kind = header->id;
    u32 start = header->offset;
    if (kind < CodeKindCount)
    {
        if (kind >= CodeKindVoices)
        {
            *size = kind == voices ? CodeSubsectionHeaderSize : 0;
        }
        else if (kind != CodeKindNone)
        {
            *size = CodeSubsectionHeaderSize;
        }
    }

    if (*size == 0)
    {
        return nullptr;
    }

    auto* reader = static_cast<CodeSubsectionReader*>(MemoryAllocate(sizeof(CodeSubsectionReader)));
    reader->kind = kind;
    reader->vtable = g_CodeSubsectionReaderVTable;
    reader->start = start;
    reader->item = item;
    return reader;
}

s32 CodeItem::SectionCount()
{
    return CodeKindCount;
}

s32 CodeItem::Unknown3()
{
    return 1;
}

bool CodeItem::CanRead(u32 type)
{
    return type == CodeKindSection;
}

void CodeItem::ReleaseResources()
{
    UnloadPendingResources(resources);
}

void CodeItem::QueueSubsection(u32 kind, u32 start)
{
    CodeKindReader* reader;
    switch (kind)
    {
    case 0:
        reader = &objects;
        break;
    case 1:
        reader = &behaviours;
        break;
    case 2:
        reader = &animations;
        break;
    case 3:
        reader = &models;
        break;
    case 4:
        reader = &codeModels;
        break;
    case 6:
        reader = &sounds;
        break;
    default:
        if (kind < CodeKindVoices || kind >= CodeKindCount)
        {
            return;
        }

        reader = &voices[kind - CodeKindVoices];
        break;
    }

    AddSectionToLoadQueue(reinterpret_cast<ItemInterface*>(reader), start);
}

void CodeSubsectionReader::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CodeSubsectionReader::Read(u8*, u32, ReaderStack*)
{
    item->QueueSubsection(kind, start);
}

SoundTable* SoundTable::Construct(SoundTable* table, u32 count)
{
    auto* sounds = reinterpret_cast<ResourceTable*>(table);
    ConstructTable(sounds, g_SoundTableBaseVTable, count, true);
    sounds->vtable = g_SoundTableVTable;
    // Retail allocates the order again, the first one let go of nowhere
    sounds->order = static_cast<u16*>(MemoryAllocate2(count * sizeof(u16)));
    return table;
}

void SoundTableQueueSamples(ResourceTable* table, s32 read, u32, u32 end)
{
    GameReadersStorage* storage = g_ReadersStorages[0];
    for (u32 index = 0; index < static_cast<u32>(read); index++)
    {
        auto* sound = static_cast<GameSound*>(table->items[table->order[index]]);
        if (sound == nullptr || sound->size == 0)
        {
            continue;
        }

        s32 size = sound->size;
        u32 offset = end + sound->offset;
        auto* reader = static_cast<SoundBankReader*>(MemoryAllocate(sizeof(SoundBankReader)));
        reader = SoundBankReader::Construct(reader, reinterpret_cast<SoundBankEntry*>(sound), offset, size);
        AddItemReaderToReaderStorage(storage, reader, 0);
    }
}

void ResourcesBaseNothing2()
{
}

void ResourcesBaseNothing3()
{
}

void GameResources::DestroyBase(u32 destroyFlags)
{
    vtable = g_ResourcesBaseVTable;
    DestroyResourceTable(objects);
    DestroyResourceTable(scripts);
    DestroyResourceTable(animations);
    DestroyResourceTable(models);
    DestroyResourceTable(codeModels);
    DestroyResourceTable(sounds);
    EmptyQueue(objectQueue, ReleaseObject);
    EmptyQueue(scriptQueue, ReleaseScript);
    EmptyQueue(animationQueue, ReleaseAnimation);
    EmptyQueue(modelQueue, ReleaseOgi);
    EmptyQueue(codeModelQueue, ReleaseCodeModel);
    EmptyQueue(soundQueue, ReleaseSound);
    if (voices != nullptr)
    {
        for (u32 language = 0; language < languageCount; language++)
        {
            DestroyResourceTable(voices[language]);
        }

        MemoryDeallocate2_(voices);
    }

    if (voiceQueues != nullptr)
    {
        for (u32 language = 0; language < languageCount; language++)
        {
            EmptyQueue(voiceQueues[language], ReleaseSound);
        }

        MemoryDeallocate2_(voiceQueues);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 ResourcesStep(GameResources* resources)
{
    // The first of the tables' queues that has something deletes its oldest
    u32 deleted = DeleteOldest(resources->objectQueue, ReleaseObject);
    if (deleted != 0)
    {
        return deleted;
    }

    deleted = DeleteOldest(resources->scriptQueue, ReleaseScript);
    if (deleted != 0)
    {
        return deleted;
    }

    deleted = DeleteOldest(resources->animationQueue, ReleaseAnimation);
    if (deleted != 0)
    {
        return deleted;
    }

    deleted = DeleteOldest(resources->modelQueue, ReleaseOgi);
    if (deleted != 0)
    {
        return deleted;
    }

    deleted = DeleteOldest(resources->codeModelQueue, ReleaseCodeModel);
    if (deleted != 0)
    {
        return deleted;
    }

    deleted = DeleteOldest(resources->soundQueue, ReleaseSound);
    if (deleted != 0)
    {
        return deleted;
    }

    // Else every language's voices delete one (their queues are never none), else every graphics table's queue
    if (resources->voiceQueues != nullptr)
    {
        for (u32 language = 0; language < resources->languageCount; language++)
        {
            deleted |= resources->voiceQueues[language]->DeleteFirst(ReleaseSound);
        }
    }

    if (deleted != 0)
    {
        return deleted;
    }

    deleted |= DeleteOldestGraphics(&g_MaterialQueue, Platform::Graphics::DeleteMaterial);
    deleted |= DeleteOldestGraphics(&g_TextureQueue, Platform::Graphics::DeleteTexture);
    deleted |= DeleteOldestGraphics(&g_ModelQueue, Platform::Graphics::DeleteModel);
    deleted |= DeleteOldestGraphics(&g_RigidModelQueue, Platform::Graphics::DeleteRigidModel);
    deleted |= DeleteOldestGraphics(&g_SkinQueue, Platform::Graphics::DeleteSkin);
    deleted |= DeleteOldestGraphics(&g_BlendSkinQueue, Platform::Graphics::DeleteBlendSkin);
    deleted |= DeleteOldestGraphics(&g_SkyQueue, Platform::Graphics::DeleteSky);
    deleted |= DeleteOldestGraphics(&g_MeshQueue, Platform::Graphics::DeleteMesh);
    deleted |= DeleteOldestGraphics(&g_LodQueue, Platform::Graphics::DeleteLod);
    return deleted;
}

void UnloadPendingResources(GameResources* resources)
{
    ResourceTable* const tables[] = {resources->objects, resources->scripts, resources->animations, resources->models,
                                     resources->codeModels, resources->sounds};
    for (ResourceTable* table : tables)
    {
        CallVirtual<void>(table, table->vtable, TableClearSlot);
    }

    for (u32 language = 0; language < resources->languageCount; language++)
    {
        ResourceTable* table = resources->voices[language];
        CallVirtual<void>(table, table->vtable, TableClearSlot);
    }
}

void ObjectTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ObjectTableBaseVTable, ReleaseObject);
}

void ObjectTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ObjectTableBaseVTable, ReleaseObject);
}

void ObjectTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseObject);
}

void ScriptTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ScriptTableBaseVTable, ReleaseScript);
}

void ScriptTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ScriptTableBaseVTable, ReleaseScript);
}

void ScriptTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseScript);
}

void ScriptTableResolve(ResourceTable* table, u32 count)
{
    for (u32 added = 0; added < count; added++)
    {
        void* script = table->items[table->order[added]];
        if (script != nullptr)
        {
            auto* vtable = *reinterpret_cast<const GccVTableEntry**>(static_cast<u8*>(script) + ScriptVTableOffset);
            CallVirtual<void>(script, vtable, ScriptResolveSlot, table);
        }
    }
}

void AnimationTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_AnimationTableBaseVTable, ReleaseAnimation);
}

void AnimationTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_AnimationTableBaseVTable, ReleaseAnimation);
}

void AnimationTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseAnimation);
}

void ModelTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ModelTableBaseVTable, ReleaseOgi);
}

void ModelTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_ModelTableBaseVTable, ReleaseOgi);
}

void ModelTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseOgi);
}

void CodeModelTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_CodeModelTableBaseVTable, ReleaseCodeModel);
}

void CodeModelTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_CodeModelTableBaseVTable, ReleaseCodeModel);
}

void CodeModelTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseCodeModel);
}

void SoundTableDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_SoundTableBaseVTable, ReleaseSound);
}

void SoundTableBaseDestroy(ResourceTable* table, u32 destroyFlags)
{
    DestroyTable(table, destroyFlags, g_SoundTableBaseVTable, ReleaseSound);
}

void SoundTableClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseSound);
}

void SoundTableBaseClear(ResourceTable* table)
{
    ReleaseAll(table, ReleaseSound);
}

void ObjectTableNothing3()
{
}

void ObjectTableNothing4()
{
}

void ScriptTableNothing3()
{
}

void ScriptTableBaseNothing4()
{
}

void AnimationTableNothing3()
{
}

void AnimationTableNothing4()
{
}

void ModelTableNothing3()
{
}

void ModelTableNothing4()
{
}

void CodeModelTableNothing3()
{
}

void CodeModelTableNothing4()
{
}

void SoundTableNothing3()
{
}

void SoundTableBaseNothing4()
{
}

void ObjectReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 ObjectReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 ObjectReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 ObjectReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* ObjectReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_ObjectSectionReaderVTable);
}

void ObjectReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void ObjectReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void ObjectReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

void BehaviourReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 BehaviourReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 BehaviourReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 BehaviourReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* BehaviourReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_BehaviourSectionReaderVTable);
}

void BehaviourReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void BehaviourReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void BehaviourReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

void AnimationReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 AnimationReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 AnimationReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 AnimationReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* AnimationReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_AnimationSectionReaderVTable);
}

void AnimationReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void AnimationReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void AnimationReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

void ModelReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 ModelReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 ModelReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 ModelReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* ModelReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_ModelSectionReaderVTable);
}

void ModelReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void ModelReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void ModelReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

void CodeModelReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 CodeModelReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 CodeModelReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 CodeModelReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* CodeModelReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_CodeModelSectionReaderVTable);
}

void CodeModelReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void CodeModelReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void CodeModelReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

void SoundReaderDestroy(CodeKindReader* reader, u32 destroyFlags)
{
    DestroyReader(reader, destroyFlags);
}

u32 SoundReaderCount(CodeKindReader* reader)
{
    return CountResources(reader);
}

u32 SoundReaderSlot3(CodeKindReader*)
{
    return 1;
}

u32 SoundReaderCanRead(CodeKindReader*, u32 type)
{
    return type == CodeKindSection;
}

void* SoundReaderGetReader(CodeKindReader* reader, s32 index, ItemHeader* header, s32*)
{
    return ResourceReaderFor(reader, index, header, g_SoundSectionReaderVTable);
}

void SoundReaderClear(CodeKindReader* reader)
{
    ClearTable(reader);
}

void SoundReaderSetCount(CodeKindReader* reader, u32 count)
{
    TableSetCount(reader, count);
}

void SoundReaderFinish(CodeKindReader* reader, s32 read, u32 count, u32 end)
{
    TableFinish(reader, read, count, end);
}

extern "C"
{
    extern const GccVTableEntry g_SectionReaderInterfaceVTable[] RETAIL(SectionReaderInterface_Methods);

    // Each kind's section reader's functions: the destructor and the read
    void ObjectSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0026db28);
    void BehaviourSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0026daf8);
    void AnimationSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0026dac8);
    void ModelSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0026da98);
    void CodeModelSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0026da68);
    void SoundSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags) RETAIL(FUN_0017c8d0);
    void LoadObject(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadObject);
    void LoadScript(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadScript);
    void LoadAnimation(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadAnimation);
    void LoadOgi(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadOgi);
    void LoadCodeModel(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadCodeModel);
    void LoadSound(ResourceSectionReader* reader, u8* data, u32 size, void* readers) RETAIL(LoadSound);
}

namespace
{
// The loaders read their items from memory streams aligned like this
constexpr u16 ItemAlignment = 0x40;

// The resource read given the reader's ID and put in the table: at the low 15 bits of the ID, its index's place in the order of
// the resources added and, when the reader has the chunk's list of object IDs, at its end
void AddResource(ResourceSectionReader* reader, void* resource)
{
    static_cast<u32*>(resource)[1] = reader->id;
    u16 id = static_cast<u16>(reader->id) & 0x7FFF;
    ResourceTable* table = reader->table;
    auto* objects = static_cast<u32*>(reader->objects);
    table->items[id] = resource;
    table->order[reader->index] = id;
    if (objects != nullptr)
    {
        u32 count = objects[0];
        reinterpret_cast<u16*>(objects + 1)[count] = id;
        objects[0] = count + 1;
    }
}

void DestroySectionReader(ResourceSectionReader* reader, u32 destroyFlags)
{
    reader->vtable = g_SectionReaderInterfaceVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}
}

void ObjectSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void BehaviourSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void AnimationSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void ModelSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void CodeModelSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void SoundSectionReaderDestroy(ResourceSectionReader* reader, u32 destroyFlags)
{
    DestroySectionReader(reader, destroyFlags);
}

void LoadObject(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    GameObject* object = GameObject::Construct(static_cast<GameObject*>(MemoryAllocate(sizeof(GameObject))), &stream);
    AddResource(reader, object);
    stream.Destroy(DestroyOnly);
}

void LoadScript(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    ScriptResource* script;
    // Even IDs are the graphs' starters, odd ones the graphs
    if ((reader->id & 1) == 0)
    {
        auto* starter = static_cast<ScriptStarter*>(ScriptResource::Construct(static_cast<ScriptResource*>(MemoryAllocate(sizeof(ScriptStarter)))));
        starter->vtable = g_StarterVTable;
        starter->Read(&stream);
        script = starter;
    }
    else
    {
        auto* graph = static_cast<ScriptGraph*>(ScriptResource::Construct(static_cast<ScriptResource*>(MemoryAllocate(sizeof(ScriptGraph)))));
        graph->vtable = g_GraphVTable;
        graph->Read(&stream);
        script = graph;
    }

    AddResource(reader, script);
    stream.Destroy(DestroyOnly);
}

void LoadAnimation(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    // InitAnimation but for the bits, which the reader sets
    auto* animation = static_cast<GameAnimation*>(MemoryAllocate(sizeof(GameAnimation)));
    ConstructResourceHeader(animation);
    animation->blendShapes.diskHandle = -1;
    animation->main.diskHandle = -1;
    animation->main.sections = 0;
    animation->main.frames = 0;
    animation->blendShapes.sections = 0;
    animation->blendShapes.frames = 0;
    ReadAnimation(animation, &stream);
    AddResource(reader, animation);
    stream.Destroy(DestroyOnly);
}

void LoadOgi(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    // Only the header and the name of InitOGI, the reader sets the rest
    auto* model = static_cast<GameOGI*>(MemoryAllocate(sizeof(GameOGI)));
    ConstructResourceHeader(model);
    model->name.string = nullptr;
    model->name.capacity = 0;
    model->name.length = 0;
    ReadOgi(model, &stream);
    AddResource(reader, model);
    stream.Destroy(DestroyOnly);
}

void LoadCodeModel(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    auto* codeModel = static_cast<CodeModel*>(MemoryAllocate(sizeof(CodeModel)));
    ConstructResourceHeader(codeModel);
    codeModel->Read(&stream);
    AddResource(reader, codeModel);
    stream.Destroy(DestroyOnly);
}

void LoadSound(ResourceSectionReader* reader, u8* data, u32 size, void*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, ItemAlignment);
    GameSound* sound = GameSound::Construct(static_cast<GameSound*>(MemoryAllocate(sizeof(GameSound))), &stream);
    AddResource(reader, sound);
    stream.Destroy(DestroyOnly);
}

extern "C"
{
    // The custom pickups' and projectiles' slots cleared (their pickups, projectiles, packs and commands destroyed), and a code
    // model's slot set up (game/pickups.cpp, game/projectiles.cpp)
    void ClearCustomPickups() RETAIL(FUN_0010a4b0);
    void ClearCustomProjectiles() RETAIL(FUN_0010c240);
    void SetUpCustomPickup(CodeModel* model) RETAIL(FUN_0010a380);
    void SetUpCustomProjectile(CodeModel* model) RETAIL(FUN_0010c3b8);
}

void GameResources::Destroy(u32 destroyFlags)
{
    vtable = g_ResourcesVTable;
    DestroyBase(destroyFlags);
}

void GameResources::SetUpCodeModels(u32 clear)
{
    // The code models' table's capacity, and the kinds of code models with slots (their byte 8)
    constexpr u16 CodeModelIds = 200;
    constexpr u8 PickupModel = 0x11;
    constexpr u8 ProjectileModel = 0x12;
    if (clear != 0)
    {
        ClearCustomPickups();
        ClearCustomProjectiles();
    }

    for (u16 index = 0; index < CodeModelIds; index++)
    {
        u16 id;
        CopyResourceId(&id, &index);
        CodeModel* model = id != 0xFFFF ? static_cast<CodeModel*>(codeModels->items[id & 0x7FFF]) : nullptr;
        if (model == nullptr)
        {
            continue;
        }

        if (model->unknown08 == PickupModel)
        {
            SetUpCustomPickup(model);
        }
        else if (model->unknown08 == ProjectileModel)
        {
            SetUpCustomProjectile(model);
        }
    }
}

void GameResources::Nothing3()
{
}
