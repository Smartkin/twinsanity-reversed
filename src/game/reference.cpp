#include "game/reference.h"

#include "game/memory.h"

using namespace ReferenceBits;

Reference* AddReference(ReferencedObject* object)
{
    if (object->reference == nullptr)
    {
        auto* block = static_cast<Reference*>(MemoryAllocate(sizeof(Reference)));
        u32 leftover = block->value;
        object->reference = block;
        block->object = object;
        block->value = leftover & 0xFE000000;
    }

    Reference* block = object->reference;
    block->value = (block->value & ~CountMask) | (((block->value & CountMask) + 1) & CountMask);
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

        u32 value = reference->value;
        u32 count = ((value & CountMask) - 1) & CountMask;
        value = (value & ~CountMask) | count;
        reference->value = value;
        if (count == 0 && (value & Owns) != 0)
        {
            if (reference->object != nullptr)
            {
                reference->object->Destroy(3);
            }

            reference->object = nullptr;
        }

        if ((reference->value & CountMask) != 0)
        {
            return;
        }

        reference = *handle;
        ReferencedObject* object = reference->object;
        if (object == nullptr)
        {
            if (reference != nullptr)
            {
                if ((reference->value & Owns) != 0)
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
            if ((block->value & Owns) != 0)
            {
                if (block->object != nullptr)
                {
                    block->object->Destroy(3);
                }

                block->object = nullptr;
            }

            MemoryDeallocate2_(block);
            object->reference = nullptr;
        }

        *handle = nullptr;
    }
}
