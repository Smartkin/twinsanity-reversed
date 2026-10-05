#include "gcc2.h"

#include "retail/libc.h"

extern "C"
{
    // GCC 2.9x's __CTOR_LIST__: the count of the static constructors (-1 for a list ending in a null entry), then the
    // constructors
    extern u32 g_StaticConstructors[] RETAIL(G_UnkFunTableSize);
    extern u32 g_StaticConstructorsRun RETAIL(D_003D1D6C);

    // GCC 2.9x's __do_global_ctors: the constructors from the last to the first
    void RunStaticConstructorList() RETAIL(FUN_002d6d20);
}

void RunStaticConstructorList()
{
    s32 count = static_cast<s32>(g_StaticConstructors[0]);
    if (count == -1)
    {
        count = 0;
        while (g_StaticConstructors[count + 1] != 0)
        {
            count++;
        }
    }

    for (s32 index = count; index != 0; index--)
    {
        reinterpret_cast<void (*)()>(g_StaticConstructors[index])();
    }
}

void RunStaticConstructors()
{
    if (g_StaticConstructorsRun != 0)
    {
        return;
    }

    g_StaticConstructorsRun = 1;
    RunStaticConstructorList();
}

// GCC 2.9x's runtime: libgcc2's terminate and pure virtual call, and frame.c's DWARF call frame interpreter (which nothing calls:
// frame.c's other functions aren't in the executable)
extern "C"
{
    // abort
    [[noreturn]] void Abort() RETAIL(FUN_002c79d8);

    // __terminate_func: the handler __terminate calls (set_terminate's), __default_terminate to start with
    extern void (*g_TerminateHandler)() RETAIL(D_002E7198);

    // __pure_virtual (every abstract function's entry in the vtables), with no message: __terminate
    void PureVirtualCalled() RETAIL(AbstractMethodCallException);
    // __default_terminate (abort) and __terminate (the handler called)
    [[noreturn]] void DefaultTerminate() RETAIL(FUN_0017ca50);
    void Terminate() RETAIL(FUN_0017ca60);
}

namespace
{
// frame.c's frame_state (long is 64 bits): the CFA and the exception's pointer, the CFA's offset from its register, the size of
// the arguments, every register's saved offset (or the register it's saved in) and how it's saved, the CFA's register and the
// return address's column. frame_state_internal adds the state DW_CFA_remember_state kept
constexpr u32 FrameRegisters = 112;

struct FrameState
{
    void* cfa;
    void* exceptionPointer;
    s64 cfaOffset;
    s64 argumentsSize;
    s64 savedAt[FrameRegisters];
    u16 cfaRegister;
    u16 returnAddressColumn;
    u8 saved[FrameRegisters];
};

struct FrameStateInternal
{
    FrameState state;
    FrameStateInternal* remembered;
};
CHECK_OFFSET(FrameStateInternal, state.cfaRegister, 0x398);
CHECK_OFFSET(FrameStateInternal, state.saved, 0x39C);
CHECK_OFFSET(FrameStateInternal, remembered, 0x410);
CHECK_SIZE(FrameStateInternal, 0x418);

// cie_info: the augmentation, the exception's pointer, the code's and the data's alignment factors and the return address's
// register
struct CieInfo
{
    const char* augmentation;
    void* exceptionPointer;
    s32 codeAlign;
    s32 dataAlign;
    u32 returnAddressRegister;
};

// How a register is saved (frame_state's saved[])
enum RegisterSaved : u8
{
    RegisterUnsaved = 0,
    RegisterSavedOffset = 1,
    RegisterSavedRegister = 2,
};

// The call frame instructions: three in the top two bits (the low six their operand), the rest whole
enum CallFrameInstruction : u32
{
    CfaAdvanceLoc = 0x40,
    CfaOffset = 0x80,
    CfaRestore = 0xC0,
    OperandMask = 0x3F,
    CfaNop = 0x0,
    CfaSetLoc = 0x1,
    CfaAdvanceLoc1 = 0x2,
    CfaAdvanceLoc2 = 0x3,
    CfaAdvanceLoc4 = 0x4,
    CfaOffsetExtended = 0x5,
    CfaRestoreExtended = 0x6,
    CfaUndefined = 0x7,
    CfaSameValue = 0x8,
    CfaRegister = 0x9,
    CfaRememberState = 0xA,
    CfaRestoreState = 0xB,
    CfaDefCfa = 0xC,
    CfaDefCfaRegister = 0xD,
    CfaDefCfaOffset = 0xE,
    CfaGnuWindowSave = 0x2D,
    CfaGnuArgsSize = 0x2E,
};

// DW_CFA_GNU_window_save's registers (SPARC's windows: 16 to 31)
constexpr u32 WindowFirst = 16;
constexpr u32 WindowEnd = 32;

// An LEB128 number's bytes: 7 bits of the number each, from the lowest, the top bit set on all but the last
constexpr u32 Leb128Bits = 7;
constexpr u32 Leb128Mask = 0x7F;
constexpr u32 Leb128More = 0x80;

u32 ReadUnaligned32(const u8* data)
{
    u32 value;
    __builtin_memcpy(&value, data, sizeof(value));
    return value;
}
}

extern "C"
{
    // frame.c's decode_uleb128 and execute_cfa_insn: an unsigned LEB128 number read (returns where it ends), and one call frame
    // instruction run on a frame's state (the address it's at moved on): returns where the next one starts
    const u8* DecodeUleb128(const u8* data, u32* value) RETAIL(FUN_0017cad0);
    const u8* ExecuteCfaInstruction(const u8* data, FrameStateInternal* state, const CieInfo* info, u8** address)
        RETAIL(func_0017CB20);
}

void PureVirtualCalled()
{
    Terminate();
}

void DefaultTerminate()
{
    Abort();
}

void Terminate()
{
    g_TerminateHandler();
}

const u8* DecodeUleb128(const u8* data, u32* value)
{
    u32 result = 0;
    u32 shift = 0;
    while (true)
    {
        u32 byte = *data++;
        result |= (byte & Leb128Mask) << (shift & ShiftMask);
        if ((byte & Leb128More) == 0)
        {
            break;
        }

        shift += Leb128Bits;
    }

    *value = result;
    return data;
}

// The function's code is 0x18 bytes past its label: three stack adjustments nothing reaches come before it
const u8* ExecuteCfaInstruction(const u8* data, FrameStateInternal* state, const CieInfo* info, u8** address)
{
    FrameState& frame = state->state;
    u32 instruction = *data++;
    u32 reg;
    u32 offset;
    // frame.c tests DW_CFA_advance_loc's bit first, so DW_CFA_restore (both bits) is taken for an advance and its own case is never
    // reached (a retail bug, in code nothing calls)
    if ((instruction & CfaAdvanceLoc) != 0)
    {
        *address += (instruction & OperandMask) * info->codeAlign;
        return data;
    }

    if ((instruction & CfaOffset) != 0)
    {
        reg = instruction & OperandMask;
        data = DecodeUleb128(data, &offset);
        frame.saved[reg] = RegisterSavedOffset;
        frame.savedAt[reg] = static_cast<s32>(offset * info->dataAlign);
        return data;
    }

    if ((instruction & CfaRestore) != 0)
    {
        reg = instruction & OperandMask;
        frame.saved[reg] = RegisterUnsaved;
        return data;
    }

    switch (instruction)
    {
    case CfaSetLoc:
        *address = reinterpret_cast<u8*>(ReadUnaligned32(data));
        data += sizeof(u32);
        break;
    case CfaAdvanceLoc1:
        *address += *data;
        data += sizeof(u8);
        break;
    case CfaAdvanceLoc2:
        *address += static_cast<u16>(data[0] | data[1] << 8);
        data += sizeof(u16);
        break;
    case CfaAdvanceLoc4:
        *address += ReadUnaligned32(data);
        data += sizeof(u32);
        break;
    case CfaOffsetExtended:
        data = DecodeUleb128(data, &reg);
        data = DecodeUleb128(data, &offset);
        frame.saved[reg] = RegisterSavedOffset;
        frame.savedAt[reg] = static_cast<s32>(offset * info->dataAlign);
        break;
    case CfaRestoreExtended:
        data = DecodeUleb128(data, &reg);
        frame.saved[reg] = RegisterUnsaved;
        break;
    case CfaNop:
    case CfaUndefined:
    case CfaSameValue:
        break;
    case CfaRegister:
    {
        u32 other;
        data = DecodeUleb128(data, &reg);
        data = DecodeUleb128(data, &other);
        frame.saved[reg] = RegisterSavedRegister;
        frame.savedAt[reg] = other;
        break;
    }
    case CfaRememberState:
    {
        auto* remembered = static_cast<FrameStateInternal*>(RetailLibc::Malloc(sizeof(FrameStateInternal)));
        *remembered = *state;
        state->remembered = remembered;
        break;
    }
    case CfaRestoreState:
    {
        FrameStateInternal* remembered = state->remembered;
        *state = *remembered;
        RetailLibc::Free(remembered);
        break;
    }
    case CfaDefCfa:
        data = DecodeUleb128(data, &reg);
        data = DecodeUleb128(data, &offset);
        frame.cfaRegister = reg;
        frame.cfaOffset = static_cast<s32>(offset);
        break;
    case CfaDefCfaRegister:
        data = DecodeUleb128(data, &reg);
        frame.cfaRegister = reg;
        break;
    case CfaDefCfaOffset:
        data = DecodeUleb128(data, &offset);
        frame.cfaOffset = static_cast<s32>(offset);
        break;
    case CfaGnuWindowSave:
        for (reg = WindowFirst; reg < WindowEnd; reg++)
        {
            frame.saved[reg] = RegisterSavedOffset;
            frame.savedAt[reg] = (reg - WindowFirst) * sizeof(void*);
        }

        break;
    case CfaGnuArgsSize:
        data = DecodeUleb128(data, &offset);
        frame.argumentsSize = static_cast<s32>(offset);
        break;
    default:
        Abort();
    }

    return data;
}
