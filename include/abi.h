#pragma once

#include "common.h"

#include <type_traits>

// The retail code passes arguments the way GCC 2.9x's EABI does: integers and pointers in $a0-$a7 in their order, floats in
// $f12-$f19 in theirs. The C++ is n32, where every argument has a position and goes in that position's register of its kind:
// the two agree when a function takes no floats, or nothing but floats. A function taking both goes through a thunk that moves
// the registers, made from its C++ declaration, which gets the retail name with "_n32" (RETAIL_N32):
//
// - EABI_EXPORT(name, function): the C++ function, for the asm (and the retail vtables and function pointers) that call it by
//   its retail name;
// - EABI_IMPORT(name, function): an asm function, for the C++ that calls it through its declaration.
//
// Calls through the retail vtables and function pointers use the EABI's registers whatever is behind them (CallEabi).
//
// Only the PS2 has the retail convention: elsewhere every function is the C++ one under its retail name, and calls through the
// retail vtables and function pointers are plain calls.
#if defined(_EE)
#define RETAIL_N32(name) asm(#name "_n32")
#ifdef __INTELLISENSE__
// VS Code's C/C++ extension doesn't take asm made of constant expressions
#define EABI_EXPORT(name, function)
#define EABI_IMPORT(name, function)
#else
#define EABI_EXPORT(name, function) asm((Abi::Thunk<decltype(function)>(Abi::Direction::Export, #name)))
#define EABI_IMPORT(name, function) asm((Abi::Thunk<decltype(function)>(Abi::Direction::Import, #name)))
#endif
#else
#define RETAIL_N32(name) asm(#name)
#define EABI_EXPORT(name, function)
#define EABI_IMPORT(name, function)
#endif

namespace Abi
{
inline constexpr u32 ArgumentRegisters = 8;
// The registers of the first argument of each kind: $a0 ($4) and $f12
inline constexpr u32 FirstIntegerArgumentRegister = 4;
inline constexpr u32 FirstFloatArgumentRegister = 12;

enum class Direction
{
    // EABI to n32
    Export,
    // n32 to EABI
    Import,
};

template <typename T>
constexpr bool IsFloat = std::is_same_v<std::remove_cv_t<T>, f32>;

template <typename T>
consteval void CheckArgument()
{
    static_assert(IsFloat<T> || std::is_integral_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || std::is_reference_v<T>,
                  "only integers, pointers and floats are passed the same way by both conventions");
    static_assert(IsFloat<T> || sizeof(T) <= 8, "128 bit values aren't passed the same way");
}

template <typename Function>
struct Signature;

template <typename Result, typename... Args>
struct Signature<Result(Args...)>
{
    static constexpr u32 Count = sizeof...(Args);
    static constexpr bool Floats[Count + 1] = {IsFloat<Args>..., false};
};

// A pointer, which picks one of overloaded functions (static_cast to its type)
template <typename Result, typename... Args>
struct Signature<Result (*)(Args...)> : Signature<Result(Args...)>
{
};

// Methods take the object first
template <typename Class, typename Result, typename... Args>
struct Signature<Result (Class::*)(Args...)>
{
    static constexpr u32 Count = sizeof...(Args) + 1;
    static constexpr bool Floats[Count + 1] = {false, IsFloat<Args>..., false};
};

template <typename Class, typename Result, typename... Args>
struct Signature<Result (Class::*)(Args...) const> : Signature<Result (Class::*)(Args...)>
{
};

struct AsmText
{
    char text[2048]{};
    u32 length = 0;

    constexpr void Append(const char* string)
    {
        while (*string != '\0')
        {
            text[length++] = *string++;
        }
    }

    constexpr void Append(u32 number)
    {
        char digits[10]{};
        u32 count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + number % 10);
            number /= 10;
        } while (number != 0);

        while (count != 0)
        {
            text[length++] = digits[--count];
        }
    }

    constexpr const char* data() const
    {
        return text;
    }

    constexpr u32 size() const
    {
        return length;
    }
};

// The thunk's moves: an argument goes from its EABI register (its index among its kind) to its n32 one (its position) on the way
// in, back on the way out. An argument's n32 register is never below its EABI one, so exporting moves the last argument first
// and importing the first, and nothing is overwritten before it's moved
template <typename Function>
consteval AsmText Thunk(Direction direction, const char* name)
{
    using Info = Signature<Function>;
    static_assert(Info::Count <= ArgumentRegisters, "arguments on the stack need a thunk of their own");

    u32 eabiIndexes[ArgumentRegisters + 1]{};
    u32 integers = 0;
    u32 floats = 0;
    for (u32 i = 0; i < Info::Count; i++)
    {
        eabiIndexes[i] = Info::Floats[i] ? floats++ : integers++;
    }

    AsmText text;
    const char* symbol = name;
    const char* target = name;
    // The C++ function's symbol, RETAIL_N32's
    char n32Name[128]{};
    u32 length = 0;
    while (name[length] != '\0')
    {
        n32Name[length] = name[length];
        length++;
    }

    for (const char* suffix = "_n32"; *suffix != '\0'; suffix++)
    {
        n32Name[length++] = *suffix;
    }

    if (direction == Direction::Export)
    {
        target = n32Name;
    }
    else
    {
        symbol = n32Name;
    }

    text.Append("\t.pushsection .text.");
    text.Append(symbol);
    text.Append(", \"ax\", @progbits\n\t.globl ");
    text.Append(symbol);
    text.Append("\n\t.type ");
    text.Append(symbol);
    text.Append(", @function\n\t.set push\n\t.set noreorder\n");
    text.Append(symbol);
    text.Append(":\n");
    for (u32 step = 0; step < Info::Count; step++)
    {
        u32 position = direction == Direction::Export ? Info::Count - 1 - step : step;
        u32 eabi = eabiIndexes[position];
        if (eabi == position)
        {
            continue;
        }

        u32 from = direction == Direction::Export ? eabi : position;
        u32 to = direction == Direction::Export ? position : eabi;
        if (Info::Floats[position])
        {
            text.Append("\tmov.s $f");
            text.Append(FirstFloatArgumentRegister + to);
            text.Append(", $f");
            text.Append(FirstFloatArgumentRegister + from);
        }
        else
        {
            text.Append("\tmove $");
            text.Append(FirstIntegerArgumentRegister + to);
            text.Append(", $");
            text.Append(FirstIntegerArgumentRegister + from);
        }

        text.Append("\n");
    }

    text.Append("\tj ");
    text.Append(target);
    text.Append("\n\tnop\n\t.set pop\n\t.size ");
    text.Append(symbol);
    text.Append(", . - ");
    text.Append(symbol);
    text.Append("\n\t.popsection\n");
    return text;
}

// Whether a call with these arguments needs its registers moved
template <typename... Args>
constexpr bool IsMixed = (IsFloat<Args> || ...) && !(IsFloat<Args> && ...);

// Calls an EABI function through a pointer: CallEabiTrampoline (src/abi.cpp) loads the arguments' registers from these arrays.
// Integers go in sign extended, like every 32 bit value in a 64 bit register
extern "C" u64 CallEabiInteger(const void* function, const u64* integers, const f32* floats) asm("CallEabiTrampoline");
extern "C" f32 CallEabiFloat(const void* function, const u64* integers, const f32* floats) asm("CallEabiTrampoline");

template <typename T>
inline void Pack(T value, u64* integers, u32& integerCount, f32* floats, u32& floatCount)
{
    CheckArgument<T>();
    if constexpr (IsFloat<T>)
    {
        floats[floatCount++] = value;
    }
    else if constexpr (std::is_pointer_v<T>)
    {
        integers[integerCount++] = static_cast<u64>(static_cast<s64>(static_cast<s32>(reinterpret_cast<u32>(value))));
    }
    else if constexpr (sizeof(T) == 8)
    {
        integers[integerCount++] = static_cast<u64>(value);
    }
    else
    {
        integers[integerCount++] = static_cast<u64>(static_cast<s64>(static_cast<s32>(value)));
    }
}

template <typename Result, typename... Args>
inline Result CallEabi(const void* function, Args... args)
{
#if !defined(_EE)
    return reinterpret_cast<Result (*)(Args...)>(const_cast<void*>(function))(args...);
#else
    static_assert(sizeof...(Args) <= ArgumentRegisters, "arguments on the stack aren't passed");
    u64 integers[ArgumentRegisters]{};
    f32 floats[ArgumentRegisters]{};
    u32 integerCount = 0;
    u32 floatCount = 0;
    (Pack(args, integers, integerCount, floats, floatCount), ...);
    if constexpr (IsFloat<Result>)
    {
        return CallEabiFloat(function, integers, floats);
    }
    else if constexpr (std::is_void_v<Result>)
    {
        CallEabiInteger(function, integers, floats);
    }
    else if constexpr (std::is_pointer_v<Result>)
    {
        return reinterpret_cast<Result>(static_cast<u32>(CallEabiInteger(function, integers, floats)));
    }
    else
    {
        return static_cast<Result>(CallEabiInteger(function, integers, floats));
    }
#endif
}
}
