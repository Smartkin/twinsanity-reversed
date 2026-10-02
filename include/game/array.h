#pragma once

#include "common.h"
#include "game/memory.h"
#include "gcc2.h"

// The game's growable arrays of pointers (a template the game instantiates per type): a full one grows by its growth into a new
// array (new[]), the old one copied over and freed (delete[]). The game walks them with iterator objects whose vtables are the
// template's instances, which the C++ does with loops
template <typename T>
struct PointerArray
{
    T** data;
    u32 count;
    u32 capacity;
    u32 growth;

    void Append(T* item)
    {
        if (count == capacity)
        {
            u32 grown = count + growth;
            auto** larger = static_cast<T**>(MemoryAllocate2(grown * sizeof(T*)));
            for (u32 i = 0; i < count; i++)
            {
                larger[i] = data[i];
            }

            if (data != nullptr)
            {
                MemoryDeallocate_(data);
            }

            data = larger;
            capacity = grown;
        }

        data[count] = item;
        count++;
    }
};
CHECK_SIZE(PointerArray<void>, 0x10);

// The retail iterators over the pointer arrays (two instances of the template's: D_00302A10, its base D_00302A68, and D_00302988,
// its base D_003029D8, which has no second Current): the array and the index it's at, done outside the array
struct ArrayIterator
{
    const GccVTableEntry* vtable;
    PointerArray<void>* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_0025c078);
    void BaseDestroy(u32 flags) RETAIL(FUN_0025c048);
    void First() RETAIL(FUN_0025c0a8);
    u32 IsDone() RETAIL(FUN_0025c0b0);
    void** Current() RETAIL(FUN_0025c0f0);
    void Next() RETAIL(FUN_0025c0e0);
    void Previous() RETAIL(FUN_0025d000);
    void Last() RETAIL(FUN_0025d010);
    void** CurrentAgain() RETAIL(FUN_0025d028);
    ArrayIterator* Assign(const ArrayIterator* other) RETAIL(func_0025D040);

    void OtherDestroy(u32 flags) RETAIL(FUN_0025c138);
    void OtherBaseDestroy(u32 flags) RETAIL(FUN_0025c108);
    void OtherFirst() RETAIL(FUN_0025c168);
    u32 OtherIsDone() RETAIL(FUN_0025c170);
    void** OtherCurrent() RETAIL(FUN_0025c1b0);
    void OtherNext() RETAIL(FUN_0025c1a0);
    void OtherPrevious() RETAIL(FUN_0025cfc0);
    void OtherLast() RETAIL(FUN_0025cfd0);
    ArrayIterator* OtherAssign(const ArrayIterator* other) RETAIL(func_0025CFE8);
};
CHECK_SIZE(ArrayIterator, 0xC);

// The retail iterator over the arrays the game sizes once (the animators' tables: {items, count}; D_003058A8, its base
// D_00305900): the same as the pointer arrays'
struct SizedArrayIterator
{
    struct Array
    {
        void** data;
        s32 size;
    };

    const GccVTableEntry* vtable;
    Array* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_00298b40);
    void BaseDestroy(u32 flags) RETAIL(FUN_00298b10);
    void First() RETAIL(FUN_00298b70);
    u32 IsDone() RETAIL(FUN_00298b78);
    void** Current() RETAIL(FUN_00298bb8);
    void Next() RETAIL(FUN_00298ba8);
    void Previous() RETAIL(FUN_00299aa8);
    void Last() RETAIL(FUN_00299ab8);
    void** CurrentAgain() RETAIL(FUN_00299ad0);
    SizedArrayIterator* Assign(const SizedArrayIterator* other) RETAIL(FUN_00299ae8);
};
CHECK_SIZE(SizedArrayIterator, 0xC);

extern "C"
{
    extern const GccVTableEntry g_SizedArrayIteratorBaseVTable[] RETAIL(D_00305900);
    extern const GccVTableEntry g_ArrayIteratorBaseVTable[] RETAIL(D_00302A68);
    extern const GccVTableEntry g_OtherArrayIteratorBaseVTable[] RETAIL(D_003029D8);
}
