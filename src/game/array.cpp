#include "game/array.h"

namespace
{
void DestroyIterator(ArrayIterator* iterator, const GccVTableEntry* base, u32 flags)
{
    iterator->vtable = base;
    if ((flags & 1) != 0)
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

void ArrayIterator::OtherDestroy(u32 flags)
{
    DestroyIterator(this, g_OtherArrayIteratorBaseVTable, flags);
}

void ArrayIterator::OtherBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_OtherArrayIteratorBaseVTable, flags);
}

void ArrayIterator::OtherFirst()
{
    index = 0;
}

u32 ArrayIterator::OtherIsDone()
{
    return IsOutside(this) ? 1 : 0;
}

void** ArrayIterator::OtherCurrent()
{
    return &array->data[index];
}

void ArrayIterator::OtherNext()
{
    index++;
}

void ArrayIterator::OtherPrevious()
{
    index--;
}

void ArrayIterator::OtherLast()
{
    index = static_cast<s32>(array->count - 1);
}

ArrayIterator* ArrayIterator::OtherAssign(const ArrayIterator* other)
{
    return Assign(other);
}

void SizedArrayIterator::Destroy(u32 flags)
{
    vtable = g_SizedArrayIteratorBaseVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SizedArrayIterator::BaseDestroy(u32 flags)
{
    vtable = g_SizedArrayIteratorBaseVTable;
    if ((flags & 1) != 0)
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
