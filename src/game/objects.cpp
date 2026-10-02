#include "game/objects.h"

#include "game/memory.h"
#include "game/stream.h"

namespace
{
constexpr u16 NoId = 0xFFFF;
// The header bits a game object made empty has (12-27)
constexpr u32 EmptyHeaderBits = 0xFF000 | 0xFF00000;
// The property list's destructor
constexpr u32 PropertyListDestroySlot = 1;

// The game object's flags its constructors clear
constexpr u32 FlagsCleared = 0x10000 | 0x20000;

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
            items[index] = NoId;
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
    *reinterpret_cast<u16*>(&object->flags) = 0;
    object->id = -1;
    object->flags &= ~FlagsCleared;
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
    for (u32& word : object->header)
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
    object->header[0] |= EmptyHeaderBits;
    return object;
}

void GameObject::Read(Stream* stream)
{
    stream->Read(header, sizeof(header), 1);
    StringRead(&name, stream);
    if (triggerBehaviours.items != nullptr)
    {
        DeleteArray(triggerBehaviours.items);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&triggerBehaviours.count));
    triggerBehaviours.items = triggerBehaviours.count != 0 ? NewArray<u32>(triggerBehaviours.count) : nullptr;
    for (u32 index = 0; index < triggerBehaviours.count; index++)
    {
        ReadObjectWord(&triggerBehaviours.items[index], stream);
    }

    ReadIds(&models, stream);
    ReadIds(&animations, stream);
    ReadIds(&behaviours, stream);
    ReadIds(&objects, stream);
    ReadIds(&sounds, stream);
    if ((header[0] & HeaderHasProperties) != 0 && properties != nullptr)
    {
        CallVirtual<void>(properties, properties->vtable, PropertyListDestroySlot, u32{DestroyAndFree});
    }

    PropertyList* list = nullptr;
    if ((header[0] & HeaderReadsProperties) != 0)
    {
        list = PropertyList::Construct(static_cast<PropertyList*>(MemoryAllocate(sizeof(PropertyList))), stream);
    }

    properties = list;
    ResourceReferences* read = nullptr;
    if ((header[0] & HeaderReadsReferences) != 0)
    {
        read = static_cast<ResourceReferences*>(MemoryAllocate(sizeof(ResourceReferences)));
        ResourceReferences::Construct(read, stream);
    }

    references = read;
    header[0] = (header[0] & ~HeaderHasProperties) | (properties != nullptr ? HeaderHasProperties : 0);
    ReadScriptPack(&scripts, stream);
}

void GameObject::Destroy(u32 destroyFlags)
{
    if ((header[0] & HeaderHasProperties) != 0 && properties != nullptr)
    {
        CallVirtual<void>(properties, properties->vtable, PropertyListDestroySlot, u32{DestroyAndFree});
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
    u16 sound = NoId;
    if (slot < static_cast<u8>(object->header[2]))
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

void ScriptPack::Run(void* agent)
{
    // Each command's vtable function 4
    for (ScriptCommand* command = commands; command != nullptr; command = command->next)
    {
        CallVirtual<void>(command, command->vtable, 4, agent);
    }
}
