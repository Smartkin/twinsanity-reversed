#include "game/reference.h"

#include "game/memory.h"

Reference* AddReference(ReferencedObject* object)
{
    if (object->reference == nullptr)
    {
        auto* block = static_cast<Reference*>(MemoryAllocate(sizeof(Reference)));
        ReferenceBits leftover = block->bits;
        object->reference = block;
        block->object = object;
        leftover.count = 0;
        leftover.owns = 0;
        block->bits = leftover;
    }

    Reference* block = object->reference;
    block->bits.count++;
    return block;
}

extern "C"
{
    void RemoveReference(Reference** handle)
    {
        Reference* reference = *handle;
        if (reference == nullptr)
        {
            return;
        }

        ReferenceBits bits = reference->bits;
        bits.count--;
        reference->bits = bits;
        if (bits.count == 0 && bits.owns)
        {
            if (reference->object != nullptr)
            {
                reference->object->Destroy(DestroyAndFree);
            }

            reference->object = nullptr;
        }

        if (reference->bits.count != 0)
        {
            return;
        }

        reference = *handle;
        ReferencedObject* object = reference->object;
        if (object == nullptr)
        {
            if (reference != nullptr)
            {
                if (reference->bits.owns)
                {
                    reference->object = nullptr;
                }

                MemoryDeallocate2_(reference);
            }

            *handle = nullptr;
            return;
        }

        // The object's own block goes, with the object when the block owns it
        Reference* block = object->reference;
        if (block != nullptr)
        {
            if (block->bits.owns)
            {
                if (block->object != nullptr)
                {
                    block->object->Destroy(DestroyAndFree);
                }

                block->object = nullptr;
            }

            MemoryDeallocate2_(block);
            object->reference = nullptr;
        }

        *handle = nullptr;
    }
}
