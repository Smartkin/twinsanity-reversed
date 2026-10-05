#include "game/objects.h"

#include "game/layout.h"
#include "game/memory.h"
#include "game/resources.h"
#include "game/sound.h"
#include "game/stream.h"

extern "C"
{
    // The module's static initialisation (its word nothing reads made 0), and the static constructor that runs it
    extern u32 g_ObjectsUnused RETAIL(D_0030A988);
    void InitObjectStatics(s32 initialise, s32 priority) RETAIL(FUN_00261c70);
    void ObjectsStaticInit() RETAIL(FUN_00263d88);
}

namespace
{
// A game object made empty has no subtype or type
constexpr u32 NoType = 0xFF;

// An array of IDs read: the old one let go, the count, the IDs (made undefined first)
void ReadIds(ObjectArray<u16>* array, Stream* stream)
{
    if (array->items != nullptr)
    {
        DeleteArray(array->items);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&array->count));
    u16* items = nullptr;
    if (array->count != 0)
    {
        items = NewArray<u16>(array->count);
        for (u32 index = 0; index < array->count; index++)
        {
            items[index] = UndefinedId;
        }
    }

    array->items = items;
    for (u32 index = 0; index < array->count; index++)
    {
        stream->ReadS16(reinterpret_cast<s16*>(&items[index]));
    }
}

// The members both constructors make
void ConstructMembers(GameObject* object)
{
    HeaderOf(object)->bits.references = 0;
    object->id = -1;
    HeaderOf(object)->bits.unused16 = 0;
    HeaderOf(object)->bits.kept = 0;
    object->name.string = nullptr;
    object->name.capacity = 0;
    object->name.length = 0;
    object->properties = nullptr;
    object->references = nullptr;
    ConstructScriptPack(&object->scripts);
    object->triggerBehaviours = {nullptr, 0};
    object->models = {nullptr, 0};
    object->animations = {nullptr, 0};
    object->behaviours = {nullptr, 0};
    object->objects = {nullptr, 0};
    object->sounds = {nullptr, 0};
    for (u32& word : object->header.words)
    {
        word = 0;
    }
}
}

GameObject* GameObject::Construct(GameObject* object, Stream* stream)
{
    ConstructMembers(object);
    object->Read(stream);
    return object;
}

GameObject* GameObject::ConstructEmpty(GameObject* object)
{
    ConstructMembers(object);
    object->header.subtype = NoType;
    object->header.type = NoType;
    return object;
}

void GameObject::Read(Stream* stream)
{
    stream->Read(&header, sizeof(header), 1);
    StringRead(&name, stream);
    if (triggerBehaviours.items != nullptr)
    {
        DeleteArray(triggerBehaviours.items);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&triggerBehaviours.count));
    triggerBehaviours.items = triggerBehaviours.count != 0 ? NewArray<TriggerBehaviour>(triggerBehaviours.count) : nullptr;
    for (u32 index = 0; index < triggerBehaviours.count; index++)
    {
        ReadObjectWord(&triggerBehaviours.items[index].value, stream);
    }

    ReadIds(&models, stream);
    ReadIds(&animations, stream);
    ReadIds(&behaviours, stream);
    ReadIds(&objects, stream);
    ReadIds(&sounds, stream);
    if (header.hasProperties != 0 && properties != nullptr)
    {
        CallVirtual<void>(properties, properties->vtable, PropertyList::DestroySlot, u32{DestroyAndFree});
    }

    PropertyList* list = nullptr;
    if (header.readsProperties != 0)
    {
        list = PropertyList::Construct(static_cast<PropertyList*>(MemoryAllocate(sizeof(PropertyList))), stream);
    }

    properties = list;
    ResourceReferences* read = nullptr;
    if (header.readsReferences != 0)
    {
        read = static_cast<ResourceReferences*>(MemoryAllocate(sizeof(ResourceReferences)));
        ResourceReferences::Construct(read, stream);
    }

    references = read;
    header.hasProperties = properties != nullptr ? 1 : 0;
    ReadScriptPack(&scripts, stream);
}

void GameObject::Destroy(u32 destroyFlags)
{
    if (header.hasProperties != 0 && properties != nullptr)
    {
        CallVirtual<void>(properties, properties->vtable, PropertyList::DestroySlot, u32{DestroyAndFree});
    }

    if (references != nullptr)
    {
        references->Destroy(DestroyAndFree);
    }

    ObjectArray<u16>* const idArrays[] = {&sounds, &objects, &behaviours, &animations, &models};
    for (ObjectArray<u16>* array : idArrays)
    {
        if (array->items != nullptr)
        {
            DeleteArray(array->items);
        }
    }

    if (triggerBehaviours.items != nullptr)
    {
        DeleteArray(triggerBehaviours.items);
    }

    DestroyScriptPack(&scripts, DestroyOnly);
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u16* GetObjectModelId(u16* id, const GameObject* object, u32 slot)
{
    *id = object->models.items[slot];
    return id;
}

u16* GetObjectAnimationId(u16* id, const GameObject* object, u32 slot)
{
    *id = object->animations.items[slot];
    return id;
}

u16* GetObjectBehaviourId(u16* id, const GameObject* object, u32 slot)
{
    *id = object->behaviours.items[slot];
    return id;
}

u16* GetObjectSoundId(u16* id, const GameObject* object, u32 slot)
{
    u16 sound = NoSoundId;
    if (slot < object->header.soundSlots)
    {
        sound = object->sounds.items[slot];
    }

    *id = sound;
    return id;
}

void ReadObjectWord(u32* word, Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(word));
}

const u32* GetObjectTriggerBehaviour(const GameObject* object, u32 index)
{
    return &object->triggerBehaviours.items[index].value;
}

void InitObjectStatics(s32 initialise, s32 priority)
{
    if (priority == DefaultInitPriority && initialise != 0)
    {
        g_ObjectsUnused = 0;
    }
}

void ObjectsStaticInit()
{
    InitObjectStatics(1, DefaultInitPriority);
}

void ScriptPack::Run(void* node)
{
    for (ScriptCommand* command = commands; command != nullptr; command = command->next)
    {
        CallVirtual<void>(command, command->vtable, ScriptCommand::ExecuteOnSlot, node);
    }
}
