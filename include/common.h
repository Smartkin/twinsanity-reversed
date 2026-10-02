#pragma once

#include <stddef.h>
#include <tamtypes.h>

typedef float f32;

// GCC for the R5900 cuts decimal float literals down to single precision (it rounds towards zero, as the R5900's FPU does),
// where retail's compiler rounded them to the nearest: 0.1f comes out a bit below retail's 0.1. Rounded(0.1) is the float
// nearest to a double literal, as retail has it; literals single precision holds exactly (0.5f, 1.25f) don't need it
consteval f32 Rounded(double value)
{
    unsigned long long bits = __builtin_bit_cast(unsigned long long, value);
    unsigned int sign = static_cast<unsigned int>(bits >> 32) & 0x80000000u;
    unsigned int exponent = static_cast<unsigned int>((bits >> 52) & 0x7FF) - 1023 + 127;
    unsigned long long mantissa = bits & ((1ull << 52) - 1);
    unsigned int kept = static_cast<unsigned int>(mantissa >> 29);
    unsigned long long rest = mantissa & ((1ull << 29) - 1);
    constexpr unsigned long long Half = 1ull << 28;
    if (rest > Half || (rest == Half && (kept & 1) != 0))
    {
        kept++;
    }

    return __builtin_bit_cast(f32, (sign | exponent << 23) + kept);
}

// A function or variable the asm still defines, by the name the asm (and the Ghidra project) gives it
#define RETAIL(name) asm(#name)

// Checks a struct against the size the game's code gives it. VS Code's C/C++ extension (__INTELLISENSE__) lays structs out
// for an x86 target, where 64 bit members are only 4 byte aligned: the compiler checks them
#ifdef __INTELLISENSE__
#define CHECK_SIZE(type, size)
#define CHECK_OFFSET(type, member, offset)
#else
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == (size), #type " must be " #size " bytes")
#define CHECK_OFFSET(type, member, offset) static_assert(offsetof(type, member) == (offset), #type "::" #member " must be at " #offset)
#endif
