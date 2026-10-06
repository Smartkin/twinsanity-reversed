#include "common.h"
#include "game/math.h"

// expf as Sony's newlib had it: fdlibm's, on the R5900's floats (no infinities or NaNs, exponent 255 is a number), wrapped in the
// SVID/X/Open error handling of its time (the retail library is X/Open's: errno is ERANGE on an overflow or underflow, the
// result the double the error handling gives, as a float). The toolchain's newlib has neither the wrapper nor its results
namespace
{
constexpr f32 Bits(u32 bits)
{
    return __builtin_bit_cast(f32, bits);
}

constexpr f32 Half[2] = {Bits(0x3F000000), Bits(0xBF000000)};
constexpr f32 Ln2High[2] = {Bits(0x3F317180), Bits(0xBF317180)};
constexpr f32 Ln2Low[2] = {Bits(0x3717F7D1), Bits(0xB717F7D1)};
constexpr f32 Huge = Bits(0x7149F2CA);
constexpr f32 TwoToMinus100 = Bits(0x0D800000);
constexpr f32 InverseLn2 = Bits(0x3FB8AA3B);
constexpr f32 P1 = Bits(0x3E2AAAAB);
constexpr f32 P2 = Bits(0xBB360B61);
constexpr f32 P3 = Bits(0x388AB355);
constexpr f32 P4 = Bits(0xB5DDEA0E);
constexpr f32 P5 = Bits(0x3331BB4C);
// The largest and smallest arguments with a result
constexpr f32 OverflowThreshold = Bits(0x42B17180);
constexpr f32 UnderflowThreshold = Bits(0xC2CFF1B5);
// What retail's compiler folded huge * huge to (an IEEE infinity, the R5900's 2^128)
constexpr f32 HugeSquared = Bits(0x7F800000);
// HUGE_VAL and SVID's HUGE as floats
constexpr f32 HugeValue = Bits(0x7F800000);
constexpr f32 SvidHuge = Bits(0x7F7FFFFF);

// The bits of |x| (a float's magnitude): past which there's no result (|x| >= 88.72), of an infinity (exponent 255, which fdlibm
// takes for an infinity or a NaN, a number on the R5900), of 0.5 ln2 and 1.5 ln2 (the argument's reductions) and of 2^-28 (below
// it e^x is 1 + x)
constexpr u32 MagnitudeMask = 0x7FFFFFFF;
constexpr u32 LargeArgumentBits = 0x42B17217;
constexpr u32 InfinityBits = 0x7F800000;
constexpr u32 HalfLn2Bits = 0x3EB17218;
constexpr u32 ThreeHalvesLn2Bits = 0x3F851592;
constexpr u32 TinyArgumentBits = 0x31800000;
// k goes into the result's exponent, or k + 100 and the result times 2^-100 when k alone would leave the normal floats
constexpr s32 SmallestScale = -125;
constexpr s32 ScaleBias = 100;
constexpr u32 ExponentShift = 23;

// _LIB_VERSION's values
constexpr s32 IeeeLibrary = -1;
constexpr s32 SvidLibrary = 0;

// ERANGE
constexpr s32 RangeError = 34;

u32 BitsOf(f32 value)
{
    return __builtin_bit_cast(u32, value);
}
}

extern "C"
{
    // _LIB_VERSION: X/Open's
    extern const s32 g_MathLibraryVersion RETAIL(D_00307E80);

    s32* ErrorNumber() RETAIL(FUN_002c7040);

    f32 ExpFloat(f32 x) RETAIL(FUN_002c1e98);
    // __ieee754_expf
    f32 ExpFloatCore(f32 x) RETAIL(FUN_002c2010);
    // finitef: the exponent isn't 255
    s32 IsFiniteFloat(f32 x) RETAIL(FUN_002c2318);
}

// The difference's sign bit: set below the infinity's bits
s32 IsFiniteFloat(f32 x)
{
    return static_cast<s32>(((BitsOf(x) & MagnitudeMask) - InfinityBits) >> 31);
}

f32 ExpFloatCore(f32 x)
{
    FloatBits bits;
    bits.value = BitsOf(x);
    s32 negative = static_cast<s32>(bits.sign);
    u32 magnitude = bits.value & MagnitudeMask;
    if (magnitude > LargeArgumentBits)
    {
        if (magnitude > InfinityBits)
        {
            return x + x;
        }

        if (magnitude == InfinityBits)
        {
            return negative == 0 ? x : 0.0f;
        }

        if (x > OverflowThreshold)
        {
            return HugeSquared;
        }

        if (x < UnderflowThreshold)
        {
            return 0.0f;
        }
    }

    // The argument as k * ln2 + high - low, |high - low| <= ln2 / 2
    s32 k;
    f32 high = 0.0f;
    f32 low = 0.0f;
    if (magnitude > HalfLn2Bits)
    {
        if (magnitude < ThreeHalvesLn2Bits)
        {
            high = x - Ln2High[negative];
            low = Ln2Low[negative];
            k = 1 - negative - negative;
        }
        else
        {
            k = static_cast<s32>(InverseLn2 * x + Half[negative]);
            f32 t = static_cast<f32>(k);
            high = x - t * Ln2High[0];
            low = t * Ln2Low[0];
        }

        x = high - low;
    }
    else if (magnitude < TinyArgumentBits)
    {
        if (Huge + x > 1.0f)
        {
            return 1.0f + x;
        }

        k = 0;
    }
    else
    {
        k = 0;
    }

    f32 t = x * x;
    f32 c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    if (k == 0)
    {
        return 1.0f - ((x * c) / (c - 2.0f) - x);
    }

    f32 y = 1.0f - ((low - (x * c) / (2.0f - c)) - high);
    if (k >= SmallestScale)
    {
        return Bits(BitsOf(y) + (static_cast<u32>(k) << ExponentShift));
    }

    return Bits(BitsOf(y) + (static_cast<u32>(k + ScaleBias) << ExponentShift)) * TwoToMinus100;
}

// expf: matherr (newlib's default) never handles an error, so X/Open's errno is always ERANGE
f32 ExpFloat(f32 x)
{
    f32 result = ExpFloatCore(x);
    if (g_MathLibraryVersion == IeeeLibrary || IsFiniteFloat(x) == 0)
    {
        return result;
    }

    if (x > OverflowThreshold)
    {
        *ErrorNumber() = RangeError;
        return g_MathLibraryVersion == SvidLibrary ? SvidHuge : HugeValue;
    }

    if (x < UnderflowThreshold)
    {
        *ErrorNumber() = RangeError;
        return 0.0f;
    }

    return result;
}
