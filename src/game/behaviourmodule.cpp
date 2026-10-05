#include "game/array.h"
#include "game/behaviours.h"
#include "game/events.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/resources.h"

// The rest of the behaviours' runtime module (the runners' motion, retail's 0x209xxx to 0x20FFA0): the events that start a
// behaviour on the node they reach and tell of a noise, the AgentLab items' builder's destructor, an iterator over pointer arrays
// nothing makes, and the module's statics

// An iterator over pointer arrays the module's template has, which no code makes (vtable D_002FD1B8, its base D_002FD210): the
// array and the index it's at, done outside the array
struct UnusedArrayIterator
{
    const GccVTableEntry* vtable;
    PointerArray<void>* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_0020f550);
    void BaseDestroy(u32 flags) RETAIL(FUN_0020f520);
    void First() RETAIL(FUN_0020f580);
    u32 IsDone() RETAIL(FUN_0020ff18);
    void** Current() RETAIL(FUN_0020f598);
    void Next() RETAIL(FUN_0020f588);
    void Previous() RETAIL(func_0020FF48);
    void Last() RETAIL(func_0020FF58);
    void** CurrentAgain() RETAIL(FUN_0020ff70);
    UnusedArrayIterator* Assign(const UnusedArrayIterator* other) RETAIL(func_0020FF88);
};
CHECK_SIZE(UnusedArrayIterator, 0xC);

extern "C"
{
    extern const GccVTableEntry g_UnusedArrayIteratorBaseVTable[] RETAIL(D_002FD210);
    // The items' builders' base, and the AgentLab items' builder's destructor (the game context's builder of class IDs 0x1800 to
    // 0x1811, vtable D_002FD2C0)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    void DestroyAgentLabItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0020ec30);
    // What a packet's end time is compared to first (only ever 0), and a place nothing reads
    extern s32 g_PacketTimeBase RETAIL(D_0030A918);
    extern ObjectPlace g_UnusedBehaviourPlace RETAIL(D_003B5A60);
    // The module's statics made (GCC 2.9x's initialisation function, called for every priority), and its global constructor
    void InitBehaviourStatics(u32 initialise, u32 priority) RETAIL(FUN_0020ebd8);
    void ConstructBehaviourModule() RETAIL(FUN_0020ffa0);
}

void ScriptEvent::Destroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScriptEvent::Apply(ObjectNodeBase* node, GameResources* resources)
{
    ResourceTable* scripts = resources->scripts;
    u16 index = starter;
    auto* found = index != NoScriptId ? static_cast<ScriptStarter*>(scripts->items[index & ResourceIndexMask]) : nullptr;
    if (found != nullptr)
    {
        CallVirtual<u32>(node, node->vtable, ObjectNode::StartBehaviourSlot, found, originator, u32{bits.forced},
                         u32{slot});
    }
}

void NoiseEvent::Destroy(u32 destroyFlags)
{
    vtable = g_GameEventVTable;
    RemoveReference(&argument);
    if (reference != nullptr)
    {
        reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void DestroyAgentLabItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void UnusedArrayIterator::Destroy(u32 flags)
{
    vtable = g_UnusedArrayIteratorBaseVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void UnusedArrayIterator::BaseDestroy(u32 flags)
{
    vtable = g_UnusedArrayIteratorBaseVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void UnusedArrayIterator::First()
{
    index = 0;
}

u32 UnusedArrayIterator::IsDone()
{
    return index < 0 || static_cast<u32>(index) >= array->count ? 1 : 0;
}

void** UnusedArrayIterator::Current()
{
    return &array->data[index];
}

void UnusedArrayIterator::Next()
{
    index++;
}

void UnusedArrayIterator::Previous()
{
    index--;
}

void UnusedArrayIterator::Last()
{
    index = static_cast<s32>(array->count - 1);
}

void** UnusedArrayIterator::CurrentAgain()
{
    return &array->data[index];
}

UnusedArrayIterator* UnusedArrayIterator::Assign(const UnusedArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

void InitBehaviourStatics(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_PacketTimeBase = 0;
    ConstructObjectPlace(&g_UnusedBehaviourPlace);
}

void ConstructBehaviourModule()
{
    InitBehaviourStatics(1, DefaultInitPriority);
}
