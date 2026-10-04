#include "game/array.h"
#include "game/layout.h"
#include "game/memory.h"

// The retail iterators the layouts' readers walked their lists with (game/instancesection.h, game/chunkloading.h's chunk
// entries): over the pointer arrays of each kind, the IDs of a trigger's instances and an array of 12 byte elements. The C++
// walks them with loops, the vtables still hold these
extern "C"
{
    extern const GccVTableEntry g_TemplatesIteratorBaseVTable[] RETAIL(D_00304AB8);
    extern const GccVTableEntry g_InstancesIteratorBaseVTable[] RETAIL(D_00303AB8);
    extern const GccVTableEntry g_AiPositionsIteratorBaseVTable[] RETAIL(D_003039A0);
    extern const GccVTableEntry g_AiPathsIteratorBaseVTable[] RETAIL(D_00303910);
    extern const GccVTableEntry g_PositionsIteratorBaseVTable[] RETAIL(D_00304188);
    extern const GccVTableEntry g_PathsIteratorBaseVTable[] RETAIL(D_003040F8);
    extern const GccVTableEntry g_TriggersIteratorBaseVTable[] RETAIL(D_00303880);
    extern const GccVTableEntry g_CamerasIteratorBaseVTable[] RETAIL(D_003037F0);
    extern const GccVTableEntry g_SurfacesIteratorBaseVTable[] RETAIL(D_00303760);
    extern const GccVTableEntry g_TriggerIdsIteratorBaseVTable[] RETAIL(D_00303B48);
    extern const GccVTableEntry g_ElementIteratorBaseVTable[] RETAIL(D_00303BD8);
}

namespace
{
// Back to the base's vtable, freed when asked
template <typename Iterator>
void DestroyIterator(Iterator* iterator, const GccVTableEntry* base, u32 flags)
{
    iterator->vtable = base;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(iterator);
    }
}

bool IsOutside(s32 index, u32 count)
{
    return index < 0 || static_cast<u32>(index) >= count;
}
}

void ArrayIterator::TemplatesDestroy(u32 flags)
{
    DestroyIterator(this, g_TemplatesIteratorBaseVTable, flags);
}

void ArrayIterator::TemplatesBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_TemplatesIteratorBaseVTable, flags);
}

void ArrayIterator::TemplatesFirst()
{
    index = 0;
}

u32 ArrayIterator::TemplatesIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::TemplatesCurrent()
{
    return &array->data[index];
}

void ArrayIterator::TemplatesNext()
{
    index++;
}

void ArrayIterator::TemplatesPrevious()
{
    index--;
}

void ArrayIterator::TemplatesLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::TemplatesCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::TemplatesAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::InstancesDestroy(u32 flags)
{
    DestroyIterator(this, g_InstancesIteratorBaseVTable, flags);
}

void ArrayIterator::InstancesBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_InstancesIteratorBaseVTable, flags);
}

void ArrayIterator::InstancesFirst()
{
    index = 0;
}

u32 ArrayIterator::InstancesIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::InstancesCurrent()
{
    return &array->data[index];
}

void ArrayIterator::InstancesNext()
{
    index++;
}

void ArrayIterator::InstancesPrevious()
{
    index--;
}

void ArrayIterator::InstancesLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::InstancesCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::InstancesAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::AiPositionsDestroy(u32 flags)
{
    DestroyIterator(this, g_AiPositionsIteratorBaseVTable, flags);
}

void ArrayIterator::AiPositionsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_AiPositionsIteratorBaseVTable, flags);
}

void ArrayIterator::AiPositionsFirst()
{
    index = 0;
}

u32 ArrayIterator::AiPositionsIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::AiPositionsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::AiPositionsNext()
{
    index++;
}

void ArrayIterator::AiPositionsPrevious()
{
    index--;
}

void ArrayIterator::AiPositionsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::AiPositionsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::AiPositionsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::AiPathsDestroy(u32 flags)
{
    DestroyIterator(this, g_AiPathsIteratorBaseVTable, flags);
}

void ArrayIterator::AiPathsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_AiPathsIteratorBaseVTable, flags);
}

void ArrayIterator::AiPathsFirst()
{
    index = 0;
}

u32 ArrayIterator::AiPathsIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::AiPathsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::AiPathsNext()
{
    index++;
}

void ArrayIterator::AiPathsPrevious()
{
    index--;
}

void ArrayIterator::AiPathsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::AiPathsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::AiPathsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::PositionsDestroy(u32 flags)
{
    DestroyIterator(this, g_PositionsIteratorBaseVTable, flags);
}

void ArrayIterator::PositionsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_PositionsIteratorBaseVTable, flags);
}

void ArrayIterator::PositionsFirst()
{
    index = 0;
}

u32 ArrayIterator::PositionsIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::PositionsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::PositionsNext()
{
    index++;
}

void ArrayIterator::PositionsPrevious()
{
    index--;
}

void ArrayIterator::PositionsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::PositionsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::PositionsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::PathsDestroy(u32 flags)
{
    DestroyIterator(this, g_PathsIteratorBaseVTable, flags);
}

void ArrayIterator::PathsBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_PathsIteratorBaseVTable, flags);
}

void ArrayIterator::PathsFirst()
{
    index = 0;
}

u32 ArrayIterator::PathsIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::PathsCurrent()
{
    return &array->data[index];
}

void ArrayIterator::PathsNext()
{
    index++;
}

void ArrayIterator::PathsPrevious()
{
    index--;
}

void ArrayIterator::PathsLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::PathsCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::PathsAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::TriggersDestroy(u32 flags)
{
    DestroyIterator(this, g_TriggersIteratorBaseVTable, flags);
}

void ArrayIterator::TriggersBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_TriggersIteratorBaseVTable, flags);
}

void ArrayIterator::TriggersFirst()
{
    index = 0;
}

u32 ArrayIterator::TriggersIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::TriggersCurrent()
{
    return &array->data[index];
}

void ArrayIterator::TriggersNext()
{
    index++;
}

void ArrayIterator::TriggersPrevious()
{
    index--;
}

void ArrayIterator::TriggersLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::TriggersCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::TriggersAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::CamerasDestroy(u32 flags)
{
    DestroyIterator(this, g_CamerasIteratorBaseVTable, flags);
}

void ArrayIterator::CamerasBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_CamerasIteratorBaseVTable, flags);
}

void ArrayIterator::CamerasFirst()
{
    index = 0;
}

u32 ArrayIterator::CamerasIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::CamerasCurrent()
{
    return &array->data[index];
}

void ArrayIterator::CamerasNext()
{
    index++;
}

void ArrayIterator::CamerasPrevious()
{
    index--;
}

void ArrayIterator::CamerasLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::CamerasCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::CamerasAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ArrayIterator::SurfacesDestroy(u32 flags)
{
    DestroyIterator(this, g_SurfacesIteratorBaseVTable, flags);
}

void ArrayIterator::SurfacesBaseDestroy(u32 flags)
{
    DestroyIterator(this, g_SurfacesIteratorBaseVTable, flags);
}

void ArrayIterator::SurfacesFirst()
{
    index = 0;
}

u32 ArrayIterator::SurfacesIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void** ArrayIterator::SurfacesCurrent()
{
    return &array->data[index];
}

void ArrayIterator::SurfacesNext()
{
    index++;
}

void ArrayIterator::SurfacesPrevious()
{
    index--;
}

void ArrayIterator::SurfacesLast()
{
    index = static_cast<s32>(array->count - 1);
}

void** ArrayIterator::SurfacesCurrentAgain()
{
    return &array->data[index];
}

ArrayIterator* ArrayIterator::SurfacesAssign(const ArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void IdArrayIterator::TriggerIdsDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_TriggerIdsIteratorBaseVTable, destroyFlags);
}

void IdArrayIterator::TriggerIdsBaseDestroy(u32 destroyFlags)
{
    DestroyIterator(this, g_TriggerIdsIteratorBaseVTable, destroyFlags);
}

void IdArrayIterator::TriggerIdsFirst()
{
    index = 0;
}

u32 IdArrayIterator::TriggerIdsIsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

u16* IdArrayIterator::TriggerIdsCurrent()
{
    return &array->data[index];
}

void IdArrayIterator::TriggerIdsNext()
{
    index++;
}

void IdArrayIterator::TriggerIdsPrevious()
{
    index--;
}

void IdArrayIterator::TriggerIdsLast()
{
    index = static_cast<s32>(array->count - 1);
}

u16* IdArrayIterator::TriggerIdsCurrentAgain()
{
    return &array->data[index];
}

IdArrayIterator* IdArrayIterator::TriggerIdsAssign(const IdArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void ElementArrayIterator::Destroy(u32 flags)
{
    DestroyIterator(this, g_ElementIteratorBaseVTable, flags);
}

void ElementArrayIterator::BaseDestroy(u32 flags)
{
    DestroyIterator(this, g_ElementIteratorBaseVTable, flags);
}

void ElementArrayIterator::First()
{
    index = 0;
}

u32 ElementArrayIterator::IsDone()
{
    return IsOutside(index, array->count) ? 1 : 0;
}

void* ElementArrayIterator::Current()
{
    return &array->data[index * ElementSize];
}

void ElementArrayIterator::Next()
{
    index++;
}

void ElementArrayIterator::Previous()
{
    index--;
}

void ElementArrayIterator::Last()
{
    index = static_cast<s32>(array->count - 1);
}

void* ElementArrayIterator::CurrentAgain()
{
    return &array->data[index * ElementSize];
}

ElementArrayIterator* ElementArrayIterator::Assign(const ElementArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}
