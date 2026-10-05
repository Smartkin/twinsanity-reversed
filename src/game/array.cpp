#include "game/array.h"

#include "game/reference.h"

namespace
{
void DestroyIterator(ArrayIterator* iterator, const GccVTableEntry* base, u32 flags)
{
    iterator->vtable = base;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}

bool IsOutside(const ArrayIterator* iterator)
{
    return iterator->index < 0 || static_cast<u32>(iterator->index) >= iterator->array->count;
}
}

void ArrayIterator::Destroy(u32 flags)
{
    DestroyIterator(this, g_ArrayIteratorBaseVTable, flags);
}

void ArrayIterator::BaseDestroy(u32 flags)
{
    DestroyIterator(this, g_ArrayIteratorBaseVTable, flags);
}

void ArrayIterator::First()
{
    index = 0;
}

u32 ArrayIterator::IsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::Current()
{
    return &array->data[index];
}

void ArrayIterator::Next()
{
    index++;
}

void ArrayIterator::Previous()
{
    index--;
}

void ArrayIterator::Last()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::CurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::Assign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::DrawnItemsDestroy(u32 flags)
{
    DestroyIterator(this, g_DrawnItemsIteratorBaseVTable, flags);
}

void ArrayIterator::DrawnItemsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_DrawnItemsIteratorBaseVTable, flags);
}

void ArrayIterator::DrawnItemsFirst()
{
    index = 0;
}

u32 ArrayIterator::DrawnItemsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::DrawnItemsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::DrawnItemsNext()
{
    index++;
}

void ArrayIterator::DrawnItemsPrevious()
{
    index--;
}

void ArrayIterator::DrawnItemsLast()
{
    index = static_cast<s32>(array->count - 1);
}

ArrayIterator* ArrayIterator::DrawnItemsAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void ArrayIterator::FontsDestroy(u32 flags)
{
    DestroyIterator(this, g_FontsIteratorBaseVTable, flags);
}

void ArrayIterator::FontsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_FontsIteratorBaseVTable, flags);
}

void ArrayIterator::FontsFirst()
{
    index = 0;
}

u32 ArrayIterator::FontsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::FontsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::FontsNext()
{
    index++;
}

void ArrayIterator::FontsPrevious()
{
    index--;
}

void ArrayIterator::FontsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::FontsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::FontsAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void ArrayIterator::TextsDestroy(u32 flags)
{
    DestroyIterator(this, g_TextsIteratorBaseVTable, flags);
}

void ArrayIterator::TextsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_TextsIteratorBaseVTable, flags);
}

void ArrayIterator::TextsFirst()
{
    index = 0;
}

u32 ArrayIterator::TextsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::TextsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::TextsNext()
{
    index++;
}

void ArrayIterator::TextsPrevious()
{
    index--;
}

void ArrayIterator::TextsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::TextsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::TextsAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void ArrayIterator::QueuedDestroy(u32 flags)
{
    DestroyIterator(this, g_HandleWalkBaseVTable, flags);
}

void ArrayIterator::QueuedFirst()
{
    index = 0;
}

u32 ArrayIterator::QueuedIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::QueuedCurrent()
{
    return &array->data[index];
}

void ArrayIterator::QueuedNext()
{
    index++;
}

void ArrayIterator::QueuedPrevious()
{
    index--;
}

void ArrayIterator::QueuedLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::QueuedCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::QueuedAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void ArrayIterator::RendererFontsDestroy(u32 flags)
{
    DestroyIterator(this, g_RendererFontsIteratorBaseVTable, flags);
}

void ArrayIterator::RendererFontsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_RendererFontsIteratorBaseVTable, flags);
}

void ArrayIterator::RendererFontsFirst()
{
    index = 0;
}

u32 ArrayIterator::RendererFontsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::RendererFontsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::RendererFontsNext()
{
    index++;
}

void ArrayIterator::RendererFontsPrevious()
{
    index--;
}

void ArrayIterator::RendererFontsLast()
{
    index = static_cast<s32>(array->count - 1);
}

ArrayIterator* ArrayIterator::RendererFontsAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void ArrayIterator::RendererTextsDestroy(u32 flags)
{
    DestroyIterator(this, g_RendererTextsIteratorBaseVTable, flags);
}

void ArrayIterator::RendererTextsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_RendererTextsIteratorBaseVTable, flags);
}

void ArrayIterator::RendererTextsFirst()
{
    index = 0;
}

u32 ArrayIterator::RendererTextsIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::RendererTextsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::RendererTextsNext()
{
    index++;
}

void ArrayIterator::RendererTextsPrevious()
{
    index--;
}

void ArrayIterator::RendererTextsLast()
{
    index = static_cast<s32>(array->count - 1);
}

ArrayIterator* ArrayIterator::RendererTextsAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void SizedArrayIterator::Destroy(u32 flags)
{
    vtable = g_SizedArrayIteratorBaseVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SizedArrayIterator::BaseDestroy(u32 flags)
{
    vtable = g_SizedArrayIteratorBaseVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SizedArrayIterator::First()
{
    index = 0;
}

u32 SizedArrayIterator::IsDone()
{
    return index < 0 || !(static_cast<u32>(index) < static_cast<u32>(array->size)) ? 1 : 0;
}

void** SizedArrayIterator::Current()
{
    return &array->data[index];
}

void SizedArrayIterator::Next()
{
    index++;
}

void SizedArrayIterator::Previous()
{
    index--;
}

void SizedArrayIterator::Last()
{
    index = array->size - 1;
}

void** SizedArrayIterator::CurrentAgain()
{
    return &array->data[index];
}

SizedArrayIterator* SizedArrayIterator::Assign(const SizedArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}
