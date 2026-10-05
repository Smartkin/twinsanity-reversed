#include "game/objectbuilder.h"

#include "game/memory.h"
#include "game/objects.h"
#include "game/properties.h"

extern "C"
{
    // The items' builders' base and the named items' vtables
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    extern const GccVTableEntry g_NamedItemVTable[] RETAIL(D_00303648);
    extern const GccVTableEntry g_NamedItemA0VTable[] RETAIL(D_003035F0);
    extern const GccVTableEntry g_NamedItem50VTable[] RETAIL(D_003035D0);
    extern const GccVTableEntry g_NamedItem28VTable[] RETAIL(D_00304BC8);
}

namespace
{
// What the named items' constructors set of the base (an empty name, no ID)
void ConstructNamedBase(NamedItem* item)
{
    item->name.string = nullptr;
    item->name.length = 0;
    item->name.capacity = 0;
    item->unused0D = 0;
    item->unused10 = 0;
}
}

void ObjectItemBuilder::Destroy(u32 destroyFlags)
{
    vtable = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void* ObjectItemBuilder::Make(u32 type)
{
    switch (type)
    {
    case TypeGameObject:
        return GameObject::ConstructEmpty(static_cast<GameObject*>(MemoryAllocate(sizeof(GameObject))));
    case TypeCodeModel:
        return CodeModel::Construct(static_cast<CodeModel*>(MemoryAllocate(sizeof(CodeModel))));
    case TypePropertyList:
        return PropertyList::Construct(static_cast<PropertyList*>(MemoryAllocate(sizeof(PropertyList))), 0, 0, 0, 0);
    case TypeResourceReferences:
        return ResourceReferences::ConstructEmpty(static_cast<ResourceReferences*>(MemoryAllocate(sizeof(ResourceReferences))));
    case TypeNamedA0:
    {
        auto* item = static_cast<NamedItemA0*>(MemoryAllocate(sizeof(NamedItemA0)));
        item->id = -1;
        item->unused18 = 1;
        item->vtable = g_NamedItemA0VTable;
        ConstructNamedBase(item);
        return item;
    }
    case TypeNamed28:
        return NamedItem28::Construct(static_cast<NamedItem28*>(MemoryAllocate(sizeof(NamedItem28))));
    case TypeNamed50:
    {
        auto* item = static_cast<NamedItem50*>(MemoryAllocate(sizeof(NamedItem50)));
        item->unused18 = 1;
        item->id = -1;
        item->vtable = g_NamedItem50VTable;
        item->otherId = -1;
        ConstructNamedBase(item);
        return item;
    }
    default:
        return nullptr;
    }
}

void NamedItem::Destroy(u32 destroyFlags)
{
    vtable = g_NamedItemVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 NamedItem::ItemType()
{
    return ObjectItemBuilder::TypeNamed;
}

void NamedItemA0::Destroy(u32 destroyFlags)
{
    NamedItem::Destroy(destroyFlags);
}

u32 NamedItemA0::ItemType()
{
    return ObjectItemBuilder::TypeNamedA0;
}

void NamedItem50::Destroy(u32 destroyFlags)
{
    NamedItem::Destroy(destroyFlags);
}

u32 NamedItem50::ItemType()
{
    return ObjectItemBuilder::TypeNamed50;
}

NamedItem28* NamedItem28::Construct(NamedItem28* item)
{
    item->id = -1;
    item->unused18 = 1;
    item->vtable = g_NamedItem28VTable;
    ConstructNamedBase(item);
    item->unused20 = 0;
    item->unused22 = 0;
    item->unused24 = 0;
    item->unused26 = 0;
    return item;
}

void NamedItem28::Destroy(u32 destroyFlags)
{
    NamedItem::Destroy(destroyFlags);
}

u32 NamedItem28::ItemType()
{
    return ObjectItemBuilder::TypeNamed28;
}
