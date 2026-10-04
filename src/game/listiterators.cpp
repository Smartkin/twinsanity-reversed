#include "game/list.h"

#include "game/memory.h"

// Three instances of the linked list iterators' template (game/list.h), which nothing makes: the disk and memory modules' and the
// object builder's (over its factories). Each the same code under its own vtable, and its base's destructor

extern "C"
{
    extern const GccVTableEntry g_DiskListIteratorBaseVTable[] RETAIL(D_002FC888);
    extern const GccVTableEntry g_OtherListIteratorBaseVTable[] RETAIL(D_002FC910);
    extern const GccVTableEntry g_BuilderListIteratorBaseVTable[] RETAIL(D_003072D8);
}

namespace
{
void DestroyIterator(LinkedListIterator* iterator, const GccVTableEntry* base, u32 destroyFlags)
{
    iterator->vtable = base;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}

LinkedListIterator* AssignIterator(LinkedListIterator* iterator, const LinkedListIterator* other)
{
    iterator->list = other->list;
    iterator->current = other->current;
    return iterator;
}
}

void LinkedListIterator::DiskDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_DiskListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::DiskBaseDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_DiskListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::DiskFirst()
{
    current = list->first;
}

u32 LinkedListIterator::DiskIsDone()
{
    return current == nullptr;
}

void* LinkedListIterator::DiskCurrent()
{
    return current->item;
}

void LinkedListIterator::DiskNext()
{
    current = current->next;
}

void LinkedListIterator::DiskPrevious()
{
    current = current->previous;
}

void LinkedListIterator::DiskLast()
{
    current = list->last;
}

LinkedListIterator* LinkedListIterator::DiskAssign(const LinkedListIterator* other)
{
    return AssignIterator(this, other);
}

void LinkedListIterator::OtherDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_OtherListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::OtherBaseDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_OtherListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::OtherFirst()
{
    current = list->first;
}

u32 LinkedListIterator::OtherIsDone()
{
    return current == nullptr;
}

void* LinkedListIterator::OtherCurrent()
{
    return current->item;
}

void LinkedListIterator::OtherNext()
{
    current = current->next;
}

void LinkedListIterator::OtherPrevious()
{
    current = current->previous;
}

void LinkedListIterator::OtherLast()
{
    current = list->last;
}

LinkedListIterator* LinkedListIterator::OtherAssign(const LinkedListIterator* other)
{
    return AssignIterator(this, other);
}

void LinkedListIterator::BuilderDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_BuilderListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::BuilderBaseDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_BuilderListIteratorBaseVTable, destroyFlags);
}

void LinkedListIterator::BuilderFirst()
{
    current = list->first;
}

u32 LinkedListIterator::BuilderIsDone()
{
    return current == nullptr;
}

void* LinkedListIterator::BuilderCurrent()
{
    return current->item;
}

void LinkedListIterator::BuilderNext()
{
    current = current->next;
}

void LinkedListIterator::BuilderPrevious()
{
    current = current->previous;
}

void LinkedListIterator::BuilderLast()
{
    current = list->last;
}

LinkedListIterator* LinkedListIterator::BuilderAssign(const LinkedListIterator* other)
{
    return AssignIterator(this, other);
}
