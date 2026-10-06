#pragma once

#include "common.h"

// What the PS2's vector units and FPU give where IEEE's floats don't: a zero divisor gives the largest float (signed as the
// quotient would be) in place of an infinity or a NaN, and a square root is the root of the number's magnitude
namespace Vu0
{
inline constexpr f32 LargestFloat = __builtin_bit_cast(f32, 0x7F7FFFFFu);

inline f32 Divide(f32 numerator, f32 denominator)
{
    if (denominator == 0.0f)
    {
        return __builtin_signbit(numerator) != __builtin_signbit(denominator) ? -LargestFloat : LargestFloat;
    }

    return numerator / denominator;
}

inline f32 SquareRoot(f32 value)
{
    return __builtin_sqrtf(__builtin_fabsf(value));
}
}
