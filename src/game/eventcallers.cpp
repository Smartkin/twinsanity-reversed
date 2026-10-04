#include "game/characters.h"
#include "game/colour.h"
#include "game/conditions.h"
#include "game/events.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"

// The event callers' destructors, the items' and the scripts' builders' destructors, the joint hooks' and the node controllers'
// bases, the copies of a vector and of a half word the asm calls, and the conditions' checks' start-up

extern "C"
{
    // The items' builders' base, and the scripts' builder's destructor (vtable ScriptBuilderFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    void DestroyItemBuilderBase(void* builder, u32 destroyFlags) RETAIL(FUN_00122ec0);
    void DestroyScriptBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0011c220);
    // A vector's 16 bytes copied (one quadword), a matrix's row, a half word copied: the first argument back
    Vector4* CopyQuadword(Vector4* to, const Vector4* from) RETAIL(MovePositionFromPos2ToPos1);
    Vector4* MatrixRow(Matrix4x4* matrix, u32 row) RETAIL(GetPositionFromRTS);
    u16* CopyHalfword(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
    // The header's constants the conditions' checks' start-up sets, which nothing reads: up (0, 1, 0, 1), 0.01 twice, a black of
    // half alpha, 0 and 45 degrees (65536ths)
    extern Vector4 g_ConditionsUp RETAIL(D_0030BAC0);
    extern f32 g_ConditionsSmall RETAIL(D_0030A4C4);
    extern f32 g_ConditionsSmall2 RETAIL(D_0030A4C0);
    extern u32 g_ConditionsShade RETAIL(D_0030A4C8);
    extern u32 g_ConditionsZero RETAIL(D_0030A4D0);
    extern s32 g_ConditionsAngle45 RETAIL(D_0030A4D8);
}

namespace
{
// What the event caller's destructor does (every event class has it inline): back to the base's vtable, its argument let go, its
// reference block told it's gone
void EndEvent(GameEvent* event, u32 destroyFlags)
{
    event->vtable = g_GameEventVTable;
    RemoveReference(&event->argument);
    if (event->reference != nullptr)
    {
        event->reference->object = nullptr;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(event);
    }
}

void EndBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}
}

void GameEvent::Destroy(u32 destroyFlags)
{
    EndEvent(this, destroyFlags);
}

void GameEvent::ArgumentEventDestroy(u32 destroyFlags)
{
    EndEvent(this, destroyFlags);
}

void GameEvent::EventCallerDestroy(u32 destroyFlags)
{
    EndEvent(this, destroyFlags);
}

void GameEvent::ApplyNothing()
{
}

void DestroyItemBuilderBase(void* builder, u32 destroyFlags)
{
    EndBuilder(builder, destroyFlags);
}

void DestroyScriptBuilder(void* builder, u32 destroyFlags)
{
    EndBuilder(builder, destroyFlags);
}

void JointHook::Destroy(u32 destroyFlags)
{
    vtable = g_JointHookVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void NodeController::Destroy(u32 destroyFlags)
{
    vtable = g_NodeControllerVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void NodeController::Start()
{
}

void NodeController::Frame(TimeClock*)
{
}

void NodeController::Restart(u32)
{
}

void NodeController::Stop()
{
}

// One LQ and one SQ, which leave out an address's low 4 bits (a struct copy is LDs and SDs)
Vector4* CopyQuadword(Vector4* to, const Vector4* from)
{
    asm volatile("lq $8, 0(%1)\n\t"
                 "sq $8, 0(%0)"
                 :
                 : "r"(to), "r"(from)
                 : "$8", "memory");
    return to;
}

Vector4* MatrixRow(Matrix4x4* matrix, u32 row)
{
    return RowOf(matrix, row);
}

u16* CopyHalfword(u16* to, const u16* from)
{
    *to = *from;
    return to;
}

void InitConditionChecksModule(u32 initialize, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    constexpr f32 Small = Rounded(0.01);
    if (priority != AllPriorities || initialize == 0)
    {
        return;
    }

    g_ConditionsUp.w = 1.0f;
    g_ConditionsUp.x = 0.0f;
    g_ConditionsSmall = Small;
    g_ConditionsUp.y = 1.0f;
    g_ConditionsUp.z = 0.0f;
    g_ConditionsSmall2 = Small;
    ColourSet(&g_ConditionsShade, 0.0f, 0.0f, 0.0f, 0.5f);
    g_ConditionsZero = 0;
    AngleFrom(&g_ConditionsAngle45, 0x1.921fb6p-1f, AngleRadians);
}
