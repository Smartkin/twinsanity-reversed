#include "game/objects.h"

#include "game/agentlab.h"
#include "game/animation.h"
#include "game/language.h"
#include "game/memory.h"
#include "game/resources.h"
#include "game/sound.h"
#include "game/stream.h"
#include "retail/libc.h"

extern "C"
{
    extern const GccVTableEntry g_ResourceIdIteratorVTable[] RETAIL(VirtualTable_303370);
    extern const GccVTableEntry g_ResourceIdIteratorBaseVTable[] RETAIL(VirtualTable_3033A8);
}

namespace
{
constexpr u16 NoId = 0xFFFF;
constexpr u16 IndexMask = 0x7FFF;

// A list made empty with new: every ID undefined, no count
ResourceIdList* NewIdList()
{
    auto* list = static_cast<ResourceIdList*>(MemoryAllocate(sizeof(ResourceIdList)));
    for (u16& id : list->ids)
    {
        id = NoId;
    }

    list->countOrMore = 0;
    return list;
}

// The list a full one goes on in (none)
ResourceIdList* MoreOf(const ResourceIdList* list)
{
    return list->countOrMore > ResourceIdList::BlockIds ? reinterpret_cast<ResourceIdList*>(list->countOrMore) : nullptr;
}

// Every ID of a list (none when it has none)
template <typename Visit>
void ForEachId(ResourceIdList* list, Visit visit)
{
    ResourceIdIterator iterator;
    iterator.vtable = g_ResourceIdIteratorVTable;
    iterator.list = list;
    for (iterator.First(); !iterator.IsDone(); iterator.Next())
    {
        visit(*iterator.Current());
    }

    iterator.vtable = g_ResourceIdIteratorBaseVTable;
}

// A reference taken to the resource of an ID: one made empty (given the ID's index) when its table has none yet
template <typename Make>
void TakeResource(ResourceTable* table, u16 id, Make make)
{
    if (id == NoId)
    {
        return;
    }

    u32 index = id & IndexMask;
    void* resource = table->items[index];
    if (resource == nullptr)
    {
        resource = make(index);
        HeaderOf(resource)->id = index;
    }

    TakeReference(resource);
    table->items[index] = resource;
}

// A reference to the resource of an ID let go of: without references and not kept, it leaves its table, into the table's
// deletion queue or deleted when the table has none
template <typename Delete>
void ReleaseResource(ResourceTable* table, u16 id, Delete deleteResource)
{
    if (id == NoId)
    {
        return;
    }

    u32 index = id & IndexMask;
    void* resource = table->items[index];
    if (resource == nullptr)
    {
        return;
    }

    if (--ReferencesOf(resource) != 0 || (HeaderOf(resource)->bits & ResourceHeader::Kept) != 0)
    {
        return;
    }

    if (table->queue != nullptr)
    {
        table->queue->Push(resource);
    }
    else
    {
        deleteResource(resource);
    }

    table->items[index] = nullptr;
}

// The voices of the language played (none past the languages)
ResourceTable* VoicesOf(GameResources* resources)
{
    if (resources->voices == nullptr || g_CurrentLanguage >= resources->languageCount)
    {
        return nullptr;
    }

    return resources->voices[g_CurrentLanguage];
}

// Sounds the game's table has are its own, the others the language's voices (no voices: retail reads the table at address 0)
ResourceTable* SoundTableOf(GameResources* resources, ResourceTable* voices, u16 id)
{
    ResourceTable* sounds = resources->sounds;
    return id != NoId && sounds->items[id & IndexMask] != nullptr ? sounds : voices;
}

void* EmptyObject(u32)
{
    return GameObject::ConstructEmpty(static_cast<GameObject*>(MemoryAllocate(sizeof(GameObject))));
}

void* EmptyModel(u32)
{
    return InitOGI(static_cast<GameOGI*>(MemoryAllocate(sizeof(GameOGI))));
}

void* EmptyAnimation(u32)
{
    return InitAnimation(static_cast<GameAnimation*>(MemoryAllocate(sizeof(GameAnimation))));
}

void* EmptyCodeModel(u32)
{
    return CodeModel::Construct(static_cast<CodeModel*>(MemoryAllocate(sizeof(CodeModel))));
}

// Odd IDs are graphs, even ones starters
void* EmptyScript(u32 index)
{
    if ((index & 1) == 0)
    {
        return ScriptStarter::Construct(static_cast<ScriptStarter*>(MemoryAllocate(sizeof(ScriptStarter))));
    }

    auto* graph = static_cast<ScriptGraph*>(MemoryAllocate(sizeof(ScriptGraph)));
    ScriptResource::Construct(graph);
    graph->vtable = g_GraphVTable;
    graph->data = nullptr;
    return graph;
}

void* EmptySound(u32)
{
    return GameSound::ConstructEmpty(static_cast<GameSound*>(MemoryAllocate(sizeof(GameSound))));
}

void FreeObject(void* object)
{
    static_cast<GameObject*>(object)->Destroy(DestroyAndFree);
}

void FreeModel(void* model)
{
    DestroyOgi(static_cast<GameOGI*>(model), DestroyAndFree);
}

void FreeAnimation(void* animation)
{
    DestroyAnimation(static_cast<GameAnimation*>(animation), DestroyAndFree);
}

void FreeCodeModel(void* model)
{
    static_cast<CodeModel*>(model)->Destroy(DestroyAndFree);
}

void FreeScript(void* script)
{
    auto* resource = static_cast<ScriptResource*>(script);
    CallVirtual<void>(resource, resource->vtable, 1, u32{DestroyAndFree});
}

void FreeSound(void* sound)
{
    static_cast<GameSound*>(sound)->Destroy(DestroyAndFree);
}
}

void ResourceIdIterator::Destroy(u32 destroyFlags)
{
    DestroyBase(destroyFlags);
}

void ResourceIdIterator::First()
{
    block = list->countOrMore != 0 ? list : nullptr;
    index = 0;
}

bool ResourceIdIterator::IsDone()
{
    return block == nullptr;
}

u16* ResourceIdIterator::Current()
{
    return &block->ids[index];
}

// A first word past 16 is the next block
void ResourceIdIterator::Next()
{
    index++;
    u32 countOrMore = block->countOrMore;
    if (countOrMore <= ResourceIdList::BlockIds)
    {
        if (index >= countOrMore)
        {
            index = 0;
            block = nullptr;
        }
    }
    else if (index >= ResourceIdList::BlockIds)
    {
        block = reinterpret_cast<ResourceIdList*>(countOrMore);
        index = 0;
    }
}

void ResourceIdIterator::DestroyBase(u32 destroyFlags)
{
    vtable = g_ResourceIdIteratorBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ResourceIdList::Read(Stream* stream)
{
    ResourceIdList* more = MoreOf(this);
    if (more != nullptr)
    {
        more->Destroy(DestroyAndFree);
    }

    countOrMore = 0;
    u32 count;
    stream->ReadS32(reinterpret_cast<s32*>(&count));
    for (u32 index = 0; index < count; index++)
    {
        u16 id;
        ReadResourceId(&id, stream);
        Add(&id);
    }
}

void ResourceIdList::Destroy(u32 destroyFlags)
{
    ResourceIdList* more = MoreOf(this);
    if (more != nullptr)
    {
        more->Destroy(DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ResourceIdList::Add(const u16* id)
{
    u32 count = countOrMore;
    if (count < BlockIds)
    {
        countOrMore = count + 1;
        ids[count] = *id;
        return;
    }

    if (count == BlockIds)
    {
        countOrMore = reinterpret_cast<u32>(NewIdList());
    }

    reinterpret_cast<ResourceIdList*>(countOrMore)->Add(id);
}

void ReadResourceId(u16* id, Stream* stream)
{
    stream->ReadS16(reinterpret_cast<s16*>(id));
}

void ResourceReferences::Construct(ResourceReferences* references, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&references->kinds));
    for (u32 kind = 0; kind < KindCount; kind++)
    {
        ResourceIdList* list = nullptr;
        if ((references->kinds & 1 << kind) != 0)
        {
            list = NewIdList();
            list->Read(stream);
        }

        references->lists[kind] = list;
    }
}

ResourceReferences* ResourceReferences::ConstructEmpty(ResourceReferences* references)
{
    references->kinds = 0;
    for (ResourceIdList*& list : references->lists)
    {
        list = nullptr;
    }

    return references;
}

void ResourceReferences::Destroy(u32 destroyFlags)
{
    for (ResourceIdList* list : lists)
    {
        if (list != nullptr)
        {
            list->Destroy(DestroyAndFree);
        }
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

CodeModel* CodeModel::Construct(CodeModel* model)
{
    ConstructResourceHeader(model);
    model->unknown09 = 0xFF;
    model->packCount = 0;
    model->unknown0B = 0;
    model->packs = nullptr;
    model->command = nullptr;
    model->packIds = nullptr;
    model->unknown08 = 0xFF;
    return model;
}

void CodeModel::Destroy(u32 destroyFlags)
{
    if (packs != nullptr)
    {
        // The packs destroyed last to first, as GCC 2.9x's delete[] does (flags 0)
        ScriptPack* pack = packs + ArrayCount(packs);
        while (pack != packs)
        {
            pack--;
            DestroyScriptPack(pack, 0);
        }

        DeleteArray(packs);
    }

    if (packIds != nullptr)
    {
        MemoryDeallocate_(packIds);
    }

    if (command != nullptr)
    {
        CallVirtual<void>(command, command->vtable, 1, u32{DestroyAndFree});
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CodeModel::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(&unknown08));
    u8 count = packCount;
    ScriptPack* made = NewArray<ScriptPack>(count);
    for (u32 index = 0; index < count; index++)
    {
        ConstructScriptPack(&made[index]);
    }

    packCount = count;
    packs = made;
    packIds = static_cast<u16*>(MemoryAllocate2(count * sizeof(u16)));
    for (u32 index = 0; index < packCount; index++)
    {
        ReadScriptPack(&packs[index], stream);
        stream->ReadS16(reinterpret_cast<s16*>(&packIds[index]));
    }

    command = ReadCommand(stream);
}

GameSound* GameSound::ConstructEmpty(GameSound* sound)
{
    ConstructResourceHeader(sound);
    sound->name.string = nullptr;
    sound->name.capacity = 0;
    sound->name.length = 0;
    sound->size = 0;
    sound->offset = 0;
    RetailLibc::MemorySet(&sound->flags, 0, 4);
    return sound;
}

void LoadObjectResources(ResourceReferences* references, const u16* objectId, GameResources* resources)
{
    ResourceTable* objects = resources->objects;
    ResourceTable* models = resources->models;
    ResourceTable* animations = resources->animations;
    ResourceTable* codeModels = resources->codeModels;
    ResourceTable* scripts = resources->scripts;
    ResourceTable* voices = VoicesOf(resources);
    if (references->lists[ResourceObjects] != nullptr && objects != nullptr)
    {
        ForEachId(references->lists[ResourceObjects], [&](u16 id) {
            if (id != *objectId)
            {
                TakeResource(objects, id, EmptyObject);
            }
        });
    }

    if (references->lists[ResourceModels] != nullptr && models != nullptr)
    {
        ForEachId(references->lists[ResourceModels], [&](u16 id) { TakeResource(models, id, EmptyModel); });
    }

    if (references->lists[ResourceAnimations] != nullptr && animations != nullptr)
    {
        ForEachId(references->lists[ResourceAnimations], [&](u16 id) { TakeResource(animations, id, EmptyAnimation); });
    }

    if (references->lists[ResourceCodeModels] != nullptr && codeModels != nullptr)
    {
        ForEachId(references->lists[ResourceCodeModels], [&](u16 id) { TakeResource(codeModels, id, EmptyCodeModel); });
    }

    if (references->lists[ResourceScripts] != nullptr && scripts != nullptr)
    {
        ForEachId(references->lists[ResourceScripts], [&](u16 id) { TakeResource(scripts, id, EmptyScript); });
    }

    if (references->lists[ResourceSounds] != nullptr && resources->sounds != nullptr)
    {
        ForEachId(references->lists[ResourceSounds],
                  [&](u16 id) { TakeResource(SoundTableOf(resources, voices, id), id, EmptySound); });
    }

    TakeResource(objects, *objectId, EmptyObject);
}

void ReleaseObjectResources(ResourceReferences* references, const u16* objectId, GameResources* resources)
{
    ResourceTable* objects = resources->objects;
    ResourceTable* models = resources->models;
    ResourceTable* animations = resources->animations;
    ResourceTable* codeModels = resources->codeModels;
    ResourceTable* scripts = resources->scripts;
    ResourceTable* voices = VoicesOf(resources);
    if (references->lists[ResourceSounds] != nullptr && resources->sounds != nullptr)
    {
        ForEachId(references->lists[ResourceSounds],
                  [&](u16 id) { ReleaseResource(SoundTableOf(resources, voices, id), id, FreeSound); });
    }

    if (references->lists[ResourceScripts] != nullptr && scripts != nullptr)
    {
        ForEachId(references->lists[ResourceScripts], [&](u16 id) { ReleaseResource(scripts, id, FreeScript); });
    }

    if (references->lists[ResourceCodeModels] != nullptr && codeModels != nullptr)
    {
        ForEachId(references->lists[ResourceCodeModels], [&](u16 id) { ReleaseResource(codeModels, id, FreeCodeModel); });
    }

    if (references->lists[ResourceAnimations] != nullptr && animations != nullptr)
    {
        ForEachId(references->lists[ResourceAnimations], [&](u16 id) { ReleaseResource(animations, id, FreeAnimation); });
    }

    if (references->lists[ResourceModels] != nullptr && models != nullptr)
    {
        ForEachId(references->lists[ResourceModels], [&](u16 id) { ReleaseResource(models, id, FreeModel); });
    }

    if (references->lists[ResourceObjects] != nullptr && objects != nullptr)
    {
        ForEachId(references->lists[ResourceObjects], [&](u16 id) {
            if (id != *objectId)
            {
                ReleaseResource(objects, id, FreeObject);
            }
        });
    }

    ReleaseResource(objects, *objectId, FreeObject);
}
