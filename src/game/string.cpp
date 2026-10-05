#include "game/string.h"
#include "game/memory.h"
#include "game/stream.h"
#include "retail/libc.h"

namespace
{
// What the buffers grow by
constexpr u32 CapacityStep = 0x20;
// StringConstructFloat writes the numbers between these plainly, the others with an exponent, and 3 decimals
constexpr f32 LargestPlain = 1000000.0f;
constexpr f32 SmallestPlain = Rounded(0.0001);
constexpr f32 DecimalsScale = 1000.0f;

// What a string of that many characters and its terminator gets
s32 CapacityFor(s32 length)
{
    return static_cast<s32>((static_cast<u32>(length) + CapacityStep) & ~(CapacityStep - 1));
}

void AppendCharacter(String* string, char character)
{
    string->string[string->length] = character;
    string->length++;
}

void Terminate(String* string)
{
    string->string[string->length] = '\0';
}

// The retail constructors of numbers start with a buffer of one step
void ConstructForNumber(String* string)
{
    string->string = nullptr;
    string->length = 0;
    string->capacity = CapacityStep;
    string->string = static_cast<char*>(MemoryAllocate2(CapacityStep));
}

// "x10^" and the exponent
void AppendExponent(String* string, u32 exponent, bool negative)
{
    AppendCharacter(string, 'x');
    AppendCharacter(string, '1');
    AppendCharacter(string, '0');
    AppendCharacter(string, '^');
    if (negative)
    {
        AppendCharacter(string, '-');
    }

    u32 tens = exponent / 10;
    if (tens != 0)
    {
        StringAppendDigits(string, tens, 0);
    }

    AppendCharacter(string, static_cast<char>('0' + exponent - tens * 10));
}
}

EABI_EXPORT(FloatToString, StringConstructFloat);
EABI_EXPORT(FUN_00201ef8, StringAppendFloat);

extern "C"
{
    String* StringConstruct(String* string, const char* text)
    {
        string->string = nullptr;
        string->capacity = 0;
        StringInitialise(string, text);
        return string;
    }

    String* StringConstructInteger(String* string, s32 value)
    {
        ConstructForNumber(string);
        u32 magnitude = static_cast<u32>(value);
        if (value < 0)
        {
            magnitude = 0u - magnitude;
            AppendCharacter(string, '-');
        }

        u32 tens = magnitude / 10;
        if (tens != 0)
        {
            StringAppendDigits(string, tens, 0);
        }

        AppendCharacter(string, static_cast<char>('0' + magnitude - tens * 10));
        Terminate(string);
        return string;
    }

    String* StringConstructNumber(String* string, u32 value)
    {
        ConstructForNumber(string);
        if (value == 0)
        {
            AppendCharacter(string, '0');
        }
        else
        {
            u32 tens = value / 10;
            if (tens != 0)
            {
                StringAppendDigits(string, tens, 0);
            }

            AppendCharacter(string, static_cast<char>('0' + value - tens * 10));
        }

        Terminate(string);
        return string;
    }

    String* StringConstructFloat(String* string, f32 value)
    {
        ConstructForNumber(string);
        if (value < 0.0f)
        {
            AppendCharacter(string, '-');
            StringAppendFloat(string, -value);
        }
        else
        {
            StringAppendFloat(string, value);
        }

        Terminate(string);
        return string;
    }

    void StringInitialise(String* string, const char* text)
    {
        if (text == nullptr || *text == '\0')
        {
            if (string->string != nullptr)
            {
                MemoryDeallocate_(string->string);
            }

            string->length = 0;
            string->capacity = 0;
            string->string = nullptr;
            return;
        }

        u32 length = RetailLibc::StringLength(text);
        s32 capacity = CapacityFor(static_cast<s32>(length));
        string->length = static_cast<s32>(length);
        if (string->capacity < capacity)
        {
            string->capacity = capacity;
            string->string = static_cast<char*>(MemoryAllocate2(capacity));
        }

        RetailLibc::StringCopy(string->string, text);
    }

    void StringDestroy(String* string)
    {
        if (string->string != nullptr)
        {
            MemoryDeallocate_(string->string);
        }

        string->string = nullptr;
        string->capacity = 0;
    }

    bool StringReserve(String* string, s32 length)
    {
        s32 capacity = CapacityFor(length);
        if (string->capacity >= capacity)
        {
            return false;
        }

        string->capacity = capacity;
        string->string = static_cast<char*>(MemoryAllocate2(capacity));
        return true;
    }

    String* StringAssign(String* string, const char* text)
    {
        char* old = string->string;
        if (text == old)
        {
            return string;
        }

        u32 length = text != nullptr ? RetailLibc::StringLength(text) : 0;
        s32 capacity = CapacityFor(static_cast<s32>(length));
        string->length = static_cast<s32>(length);
        bool grew = string->capacity < capacity;
        if (grew)
        {
            string->capacity = capacity;
            string->string = static_cast<char*>(MemoryAllocate2(capacity));
        }

        if (text == nullptr)
        {
            string->string[0] = '\0';
        }
        else
        {
            RetailLibc::StringCopy(string->string, text);
        }

        if (grew && old != nullptr)
        {
            MemoryDeallocate_(old);
            string->capacity = 0;
        }

        return string;
    }

    void StringAssignPart(String* string, const char* text, s32 length)
    {
        String part;
        part.string = nullptr;
        part.capacity = 0;
        part.length = length;
        StringReserve(&part, length);
        RetailLibc::StringCopyCount(part.string, text, static_cast<u32>(length));
        part.string[part.length] = '\0';
        StringAssign(string, part.string);
        StringDestroy(&part);
    }

    String* StringAppend(String* string, const char* text)
    {
        s32 added = static_cast<s32>(RetailLibc::StringLength(text));
        if (added <= 0)
        {
            return string;
        }

        s32 length = string->length + added;
        char* old = string->string;
        s32 capacity = CapacityFor(length);
        string->length = length;
        bool grew = string->capacity < capacity;
        if (grew)
        {
            string->capacity = capacity;
            string->string = static_cast<char*>(MemoryAllocate2(capacity));
        }

        if (old == nullptr)
        {
            RetailLibc::StringCopy(string->string, text);
        }
        else
        {
            if (grew)
            {
                RetailLibc::StringCopy(string->string, old);
            }

            RetailLibc::StringConcatenate(string->string, text);
        }

        if (grew)
        {
            if (old != nullptr)
            {
                MemoryDeallocate_(old);
            }

            string->capacity = 0;
        }

        return string;
    }

    void StringPrepend(String* string, const char* text)
    {
        s32 added = static_cast<s32>(RetailLibc::StringLength(text));
        if (added <= 0)
        {
            return;
        }

        s32 length = string->length + added;
        s32 capacity = CapacityFor(length);
        char* old = string->string;
        if (string->capacity < capacity)
        {
            string->capacity = capacity;
            string->string = static_cast<char*>(MemoryAllocate2(capacity));
            RetailLibc::StringCopy(string->string, text);
            if (old != nullptr)
            {
                RetailLibc::StringConcatenate(string->string, old);
                MemoryDeallocate_(old);
                string->capacity = 0;
            }
        }
        else
        {
            // Moves the characters up from the last, then puts the text before them
            for (s32 i = 1; i <= string->length; i++)
            {
                string->string[length - i] = string->string[string->length - i];
            }

            for (s32 i = 0; i < added; i++)
            {
                string->string[i] = text[i];
            }
        }

        string->string[length] = '\0';
        string->length = length;
    }

    void StringAppendDigits(String* string, u32 value, s32 minimumDigits)
    {
        u32 tens = value / 10;
        if (tens != 0 || minimumDigits >= 2)
        {
            StringAppendDigits(string, tens, minimumDigits - 1);
        }

        AppendCharacter(string, static_cast<char>('0' + value - tens * 10));
    }

    // The retail constants: 0.1 is multiplied by, not divided by 10
    void StringAppendFloat(String* string, f32 value)
    {
        if (value == 0.0f)
        {
            AppendCharacter(string, '0');
            AppendCharacter(string, '.');
            AppendCharacter(string, '0');
            AppendCharacter(string, '0');
            AppendCharacter(string, '0');
            return;
        }

        if (value > LargestPlain)
        {
            u32 exponent = 0;
            while (value >= 10.0f)
            {
                value *= Rounded(0.1);
                exponent++;
            }

            StringAppendFloat(string, value);
            AppendExponent(string, exponent, false);
            return;
        }

        if (value < SmallestPlain)
        {
            u32 exponent = 0;
            while (value < 1.0f)
            {
                value *= 10.0f;
                exponent++;
            }

            StringAppendFloat(string, value);
            AppendExponent(string, exponent, true);
            return;
        }

        u32 whole = static_cast<u32>(static_cast<s32>(value));
        u32 thousandths = static_cast<u32>(static_cast<s32>((value - static_cast<f32>(static_cast<s32>(whole))) * DecimalsScale));
        u32 tens = whole / 10;
        if (tens != 0)
        {
            StringAppendDigits(string, tens, 0);
        }

        AppendCharacter(string, static_cast<char>('0' + whole - tens * 10));
        AppendCharacter(string, '.');
        u32 hundredths = thousandths / 10;
        StringAppendDigits(string, hundredths, 2);
        AppendCharacter(string, static_cast<char>('0' + thousandths - hundredths * 10));
    }

    void StringTruncate(String* string, s32 length)
    {
        if (length < string->length)
        {
            string->string[length] = '\0';
            string->length = length;
        }
    }

    void StringToBackslashes(String* string)
    {
        for (s32 i = 0; i < string->length; i++)
        {
            if (string->string[i] == '/')
            {
                string->string[i] = '\\';
            }
        }
    }

    s32 StringFind(const String* string, s32 start, const char* text)
    {
        s32 length = static_cast<s32>(RetailLibc::StringLength(text));
        String wanted;
        wanted.string = nullptr;
        wanted.capacity = 0;
        StringInitialise(&wanted, text);
        String part;
        part.string = nullptr;
        part.length = 0;
        part.capacity = 0;
        for (s32 at = start; at <= string->length - length; at++)
        {
            StringAssignPart(&part, string->string + at, length);
            if (!StringNotEqual(&part, wanted.string))
            {
                StringDestroy(&part);
                StringDestroy(&wanted);
                return at;
            }
        }

        StringDestroy(&part);
        StringDestroy(&wanted);
        return StringNotFound;
    }

    bool StringRemove(String* string, const char* text)
    {
        s32 at = StringFind(string, 0, text);
        if (at < 0)
        {
            return false;
        }

        s32 length = static_cast<s32>(RetailLibc::StringLength(text));
        // strncpy over the string itself, the terminator included
        RetailLibc::StringCopyCount(string->string + at, string->string + at + length,
                                    static_cast<u32>(string->length - (at + length) + 1));
        string->length -= length;
        string->string[string->length] = '\0';
        return true;
    }

    bool StringReplace(String* string, const char* text, const char* replacement)
    {
        s32 at = StringFind(string, 0, text);
        if (at < 0)
        {
            return false;
        }

        String rest;
        rest.string = nullptr;
        rest.capacity = 0;
        StringInitialise(&rest, string->string + at + RetailLibc::StringLength(text));
        StringTruncate(string, at);
        StringAppend(string, replacement);
        if (rest.length != 0)
        {
            StringAppend(string, rest.string);
        }

        StringDestroy(&rest);
        return true;
    }

    // No string is less than any other, and none is more than one
    bool StringLess(const String* first, const String* second)
    {
        if (first->string == second->string)
        {
            return false;
        }

        if (first->string == nullptr)
        {
            return true;
        }

        if (second->string == nullptr)
        {
            return false;
        }

        return RetailLibc::StringCompare(first->string, second->string) < 0;
    }

    // A string without a buffer is unequal to any text. With no text either, the retail code compares the two null pointers
    bool StringNotEqual(const String* string, const char* text)
    {
        if (string->string == nullptr)
        {
            if (text != nullptr)
            {
                return true;
            }
        }
        else if (text == nullptr)
        {
            return true;
        }

        return RetailLibc::StringCompare(string->string, text) != 0;
    }

    void StringRead(String* string, Stream* stream)
    {
        if (string->string != nullptr)
        {
            MemoryDeallocate_(string->string);
        }

        string->capacity = 0;
        string->string = nullptr;
        stream->ReadU32(reinterpret_cast<u32*>(&string->length));
        if (string->length <= 0)
        {
            return;
        }

        s32 capacity = CapacityFor(string->length);
        if (string->capacity < capacity)
        {
            string->capacity = capacity;
            string->string = static_cast<char*>(MemoryAllocate2(capacity));
        }

        stream->Read(string->string, static_cast<u32>(string->length), 1);
        string->string[string->length] = '\0';
    }

    void StringWrite(const String* string, Stream* stream)
    {
        stream->WriteU32(static_cast<u32>(string->length));
        if (string->length > 0)
        {
            stream->Write(string->string, static_cast<u32>(string->length));
        }
    }
}

extern "C" String* StringConstructCopy(String* string, const String* other)
{
    string->string = nullptr;
    string->length = 0;
    string->capacity = 0;
    StringAssign(string, other->string);
    return string;
}

extern "C" void StringDelete(String* string, u32 flags)
{
    StringDestroy(string);
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(string);
    }
}
