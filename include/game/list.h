#pragma once

#include "common.h"

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
