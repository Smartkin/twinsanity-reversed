#pragma once

#include "common.h"
#include "gcc2.h"

// The game's intrusive doubly linked lists: a head pointer and each node's links, named by member pointers (the retail functions
// took GCC 2.9x member pointers as arguments). Pushing onto a list that has nodes leaves the new node's previous link as it was:
// nodes come off lists with both links cleared
template <typename T, T* T::*Previous, T* T::*Next>
struct IntrusiveList
{
    static void PushFront(T* node, T** head)
    {
        if (*head == nullptr)
        {
            *head = node;
            node->*Previous = nullptr;
            node->*Next = nullptr;
            return;
        }

        (*head)->*Previous = node;
        node->*Next = *head;
        *head = node;
    }

    static void InsertAfter(T* node, T* after)
    {
        T* next = after->*Next;
        after->*Next = node;
        node->*Previous = after;
        node->*Next = next;
        if (next != nullptr)
        {
            next->*Previous = node;
        }
    }

    static void InsertBefore(T* node, T** head, T* before)
    {
        T* previous = before->*Previous;
        before->*Previous = node;
        node->*Next = before;
        node->*Previous = previous;
        if (previous == nullptr)
        {
            *head = node;
        }
        else
        {
            previous->*Next = node;
        }
    }

    static void Remove(T* node, T** head)
    {
        T* previous = node->*Previous;
        T* next = node->*Next;
        if (previous == nullptr)
        {
            *head = next;
        }
        else
        {
            previous->*Next = next;
        }

        if (next != nullptr)
        {
            next->*Previous = previous;
        }

        node->*Next = nullptr;
        node->*Previous = nullptr;
    }
};

// The retail list functions that take the links as GCC 2.9x member pointers (their offsets plus 1), as the game passes them
namespace RetailList
{
inline void*& Link(void* node, u32 member)
{
    return *reinterpret_cast<void**>(static_cast<u8*>(node) + member - 1);
}

inline void PushFront(void* node, void** head, u32 previous, u32 next)
{
    if (*head == nullptr)
    {
        *head = node;
        Link(node, previous) = nullptr;
        Link(node, next) = nullptr;
        return;
    }

    Link(*head, previous) = node;
    Link(node, next) = *head;
    *head = node;
}

inline void Remove(void* node, void** head, u32 previous, u32 next)
{
    void* before = Link(node, previous);
    void* after = Link(node, next);
    if (before == nullptr)
    {
        if (after == nullptr)
        {
            *head = nullptr;
        }
        else
        {
            Link(after, previous) = nullptr;
            *head = Link(node, next);
        }
    }
    else if (after == nullptr)
    {
        Link(before, next) = nullptr;
    }
    else
    {
        Link(before, next) = after;
        Link(after, previous) = Link(node, previous);
    }

    Link(node, next) = nullptr;
    Link(node, previous) = nullptr;
}
}

// The retail iterators over the game's doubly linked lists of items (a template it instantiates per type; nothing makes any): the
// list (its first and last nodes) and the node it's at (each node its previous and next node, then its item), done past either
// end. Their vtables: 1 the destructor, 2 to the first node, 3 whether it's done, 4 the current item, 5 to the next node, 6 to the
// previous one, 7 to the last one, 8 another's list and node taken; their bases' 2 to 5 are abstract. The scenery items'
// (D_002FC4B0, its base D_002FC500), two the disk and memory modules have (D_002FC838 over D_002FC888, and D_002FC8C0 over
// D_002FC910) and the object builder's over its factories (D_00307288 over D_003072D8, which the C++ walks with a loop)
struct LinkedListIterator
{
    // (The item, of the list's type, after the links)
    struct Node
    {
        Node* previous;
        Node* next;
        u8 item[];
    };

    struct List
    {
        Node* first;
        Node* last;
    };

    const GccVTableEntry* vtable;
    List* list;
    Node* current;

    void Destroy(u32 destroyFlags) RETAIL(FUN_001ff448);
    void First() RETAIL(FUN_001ff478);
    u32 IsDone() RETAIL(FUN_00201b28);
    void* Current() RETAIL(FUN_001ff498);
    void Next() RETAIL(FUN_001ff488);
    void Previous() RETAIL(func_00201B38);
    void Last() RETAIL(func_00201B48);
    LinkedListIterator* Assign(const LinkedListIterator* other) RETAIL(func_00201B58);

    void DiskDestroy(u32 destroyFlags) RETAIL(FUN_00204268);
    void DiskBaseDestroy(u32 destroyFlags) RETAIL(FUN_00204238);
    void DiskFirst() RETAIL(FUN_00204298);
    u32 DiskIsDone() RETAIL(FUN_002042a8);
    void* DiskCurrent() RETAIL(FUN_002042c8);
    void DiskNext() RETAIL(FUN_002042b8);
    void DiskPrevious() RETAIL(func_00205EF8);
    void DiskLast() RETAIL(func_00205F08);
    LinkedListIterator* DiskAssign(const LinkedListIterator* other) RETAIL(func_00205F18);

    void OtherDestroy(u32 destroyFlags) RETAIL(FUN_002041b0);
    void OtherBaseDestroy(u32 destroyFlags) RETAIL(FUN_00204180);
    void OtherFirst() RETAIL(FUN_002041e0);
    u32 OtherIsDone() RETAIL(FUN_002041f0);
    void* OtherCurrent() RETAIL(FUN_00204210);
    void OtherNext() RETAIL(FUN_00204200);
    void OtherPrevious() RETAIL(func_00205F30);
    void OtherLast() RETAIL(func_00205F40);
    LinkedListIterator* OtherAssign(const LinkedListIterator* other) RETAIL(func_00205F50);

    void BuilderDestroy(u32 destroyFlags) RETAIL(FUN_002b5998);
    void BuilderBaseDestroy(u32 destroyFlags) RETAIL(FUN_002b5968);
    void BuilderFirst() RETAIL(FUN_002b59c8);
    u32 BuilderIsDone() RETAIL(FUN_002b59d8);
    void* BuilderCurrent() RETAIL(FUN_002b59f8);
    void BuilderNext() RETAIL(FUN_002b59e8);
    void BuilderPrevious() RETAIL(FUN_002b7358);
    void BuilderLast() RETAIL(FUN_002b7368);
    LinkedListIterator* BuilderAssign(const LinkedListIterator* other) RETAIL(FUN_002b7378);
};
CHECK_SIZE(LinkedListIterator, 0xC);
