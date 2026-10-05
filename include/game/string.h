#pragma once

#include "abi.h"
#include "common.h"

class Stream;

// The game's string. Its buffer grows in steps of 32 bytes, from the heap manager (MemoryAllocate2). The functions that make it
// grow set the capacity to 0 after freeing the old buffer (a retail bug: the next growth always allocates again), and
// StringReserve and StringInitialise don't free what was there: they're for strings being constructed
struct String
{
    char* string;
    s32 length;
    s32 capacity;
};
CHECK_SIZE(String, 0xC);

// StringFind's result when the text isn't there
constexpr s32 StringNotFound = -1;

extern "C"
{
    // Constructors: the struct is taken as it is and made up
    String* StringConstruct(String* string, const char* text) RETAIL(CopyToString);
    // The number in decimal, with a "-" when it's negative
    String* StringConstructInteger(String* string, s32 value) RETAIL(FUN_00202280);
    String* StringConstructNumber(String* string, u32 value) RETAIL(NumberToString);
    // The number with 3 decimals, or times a power of ten out of 0.0001 to 1000000 ("1.500x10^7", "2.000x10^-5")
    String* StringConstructFloat(String* string, f32 value) RETAIL_N32(FloatToString);
    // The constructor's body: no text or an empty one frees the buffer
    void StringInitialise(String* string, const char* text) RETAIL(FUN_00204b60);
    void StringDestroy(String* string) RETAIL(StringDelete);
    // A copy of another's text, and the destructor with GCC 2.9x's flags (FreeAfterDestroy frees the struct)
    String* StringConstructCopy(String* string, const String* other) RETAIL(FUN_0019ffd0);
    void StringDelete(String* string, u32 flags) RETAIL(FUN_001a0008);

    // Makes room for length characters and the terminator. Returns whether it allocated
    bool StringReserve(String* string, s32 length) RETAIL(NewString);
    String* StringAssign(String* string, const char* text) RETAIL(CreateStringFromChar);
    // The text's first length characters
    void StringAssignPart(String* string, const char* text, s32 length) RETAIL(FUN_00204918);
    String* StringAppend(String* string, const char* text) RETAIL(ConcatenateString);
    void StringPrepend(String* string, const char* text) RETAIL(ReverseStringConcatenation);
    // The number's decimal digits, at least minimumDigits of them, with no terminator
    void StringAppendDigits(String* string, u32 value, s32 minimumDigits) RETAIL(FUN_00204378);
    // StringConstructFloat's text for a value that isn't negative, with no terminator
    void StringAppendFloat(String* string, f32 value) RETAIL_N32(FUN_00201ef8);
    void StringTruncate(String* string, s32 length) RETAIL(FUN_002048e0);
    void StringToBackslashes(String* string) RETAIL(ForwardToBackSlash);

    // Where the text is from start on, StringNotFound when it isn't
    s32 StringFind(const String* string, s32 start, const char* text) RETAIL(FUN_00202360);
    // Remove or replace the text's first occurrence. Return whether there was one
    bool StringRemove(String* string, const char* text) RETAIL(FUN_002045c0);
    bool StringReplace(String* string, const char* text, const char* replacement) RETAIL(FUN_00204658);
    bool StringLess(const String* first, const String* second);
    bool StringNotEqual(const String* string, const char* text) RETAIL(StringsNotEqual);

    // The length (an unsigned 32 bit number), then the characters
    void StringRead(String* string, Stream* stream) RETAIL(GetGameResourceName_);
    void StringWrite(const String* string, Stream* stream) RETAIL(FUN_00204ab0);
}
