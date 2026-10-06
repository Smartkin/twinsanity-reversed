#pragma once

#include <stddef.h>

#if defined(_EE)
#include <tamtypes.h>
#else
// PS2SDK's types (tamtypes.h) elsewhere: the game's 64 bit values are long long, pointers fit in 32 bits only on the PS2 and
// 32 bit platforms
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;
#endif

typedef float f32;

// GCC for the R5900 cuts decimal float literals down to single precision (it rounds towards zero, as the R5900's FPU does),
// where retail's compiler rounded them to the nearest: 0.1f comes out a bit below retail's 0.1. Rounded(0.1) is the float
// nearest to a double literal, as retail has it; literals single precision holds exactly (0.5f, 1.25f) don't need it
consteval f32 Rounded(double value)
{
    // A double's 52 bits of fraction rounded to a float's 23 (to even when it's halfway), its exponent biased by 127 in place
    // of 1023; the sign is the top bit of both, a carry out of the fraction goes into the exponent
    constexpr unsigned int DoubleFractionBits = 52;
    constexpr unsigned int FloatFractionBits = 23;
    constexpr unsigned int DroppedBits = DoubleFractionBits - FloatFractionBits;
    constexpr unsigned int DoubleExponentMask = 0x7FF;
    constexpr unsigned int DoubleBias = 1023;
    constexpr unsigned int FloatBias = 127;
    constexpr unsigned int SignBit = 0x80000000u;
    unsigned long long bits = __builtin_bit_cast(unsigned long long, value);
    unsigned int sign = static_cast<unsigned int>(bits >> 32) & SignBit;
    unsigned int exponent =
        static_cast<unsigned int>((bits >> DoubleFractionBits) & DoubleExponentMask) - DoubleBias + FloatBias;
    unsigned long long mantissa = bits & ((1ull << DoubleFractionBits) - 1);
    unsigned int kept = static_cast<unsigned int>(mantissa >> DroppedBits);
    unsigned long long rest = mantissa & ((1ull << DroppedBits) - 1);
    constexpr unsigned long long Half = 1ull << (DroppedBits - 1);
    if (rest > Half || (rest == Half && (kept & 1) != 0))
    {
        kept++;
    }

    return __builtin_bit_cast(f32, (sign | exponent << FloatFractionBits) + kept);
}

// What the R5900's variable shifts (sllv, srlv, srav) take of a shift: its low 5 bits. A shift the C++ can't keep below 32 is
// masked with it, as the retail code's wrap
inline constexpr u32 ShiftMask = 0x1F;

// A function or variable under the name the retail executable's symbols give it (the Ghidra project's), as the retail data's
// objects (src/data/) and the code naming them have it
#define RETAIL(name) asm(#name)

// GCC makes memset and memmove calls of some loops the retail compiler kept as loops: the functions with them keep them (another
// compiler may make the calls, which do the same)
#if defined(__clang__)
#define KEEP_LOOPS
#else
#define KEEP_LOOPS __attribute__((optimize("no-tree-loop-distribute-patterns")))
#endif

// Checks a struct against the size the game's code gives it. VS Code's C/C++ extension (__INTELLISENSE__) lays structs out
// for an x86 target, where 64 bit members are only 4 byte aligned: the compiler checks them
#ifdef __INTELLISENSE__
#define CHECK_SIZE(type, size)
#define CHECK_OFFSET(type, member, offset)
#else
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == (size), #type " must be " #size " bytes")
#define CHECK_OFFSET(type, member, offset) static_assert(offsetof(type, member) == (offset), #type "::" #member " must be at " #offset)
#endif
