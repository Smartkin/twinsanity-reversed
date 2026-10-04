#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"

struct MotionBlock;
struct TaggedValue;

// The development tools' arguments of a command, which each command's ParseTokens (its vtable's slot 2) reads into its fields:
// the retail game never parses any. A token is an argument's kind (the command's own numbers), its record type (10: a property
// reference) and its value (an int's or a float's bits)
struct ScriptToken
{
    u16 kind;
    u8 type;
    u8 unknown03;
    u32 value;

    f32 Float() const
    {
        return *reinterpret_cast<const f32*>(&value);
    }
};
CHECK_SIZE(ScriptToken, 8);

// A list of tokens: its disk manager block and how many it has
struct ScriptTokenList
{
    s32 block;
    u32 count;
};

// A walk over a token list (its vtable: 1 the destructor, 2 to the first, 3 whether it's past the last, 4 the token, 5 to the
// next; its base's are abstract), the list, its tokens in the disk manager's memory and the index. The same class again under
// another vtable has its own copies of the functions
struct ScriptTokenReader
{
    const GccVTableEntry* vtable;
    const ScriptTokenList* list;
    const ScriptToken* tokens;
    u32 index;

    static ScriptTokenReader* Construct(ScriptTokenReader* reader, const ScriptTokenList* list) RETAIL(DiskDataReader_Init);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00208338);
    void First() RETAIL(FUN_002087f8);
    bool AtEnd() const RETAIL(DiskDataReader_AtEnd);
    const ScriptToken* Current() const RETAIL(GetCurrentPosition);
    void Next() RETAIL(DiskDataReader_Next);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_0011f068);
    void OtherDestroy(u32 destroyFlags) RETAIL(FUN_00208308);
    void OtherBaseDestroy(u32 destroyFlags) RETAIL(FUN_00209e70);
    void OtherFirst() RETAIL(FUN_00208750);
    bool OtherAtEnd() const RETAIL(FUN_00208770);
    const ScriptToken* OtherCurrent() const RETAIL(FUN_00208758);
    void OtherNext() RETAIL(FUN_00208788);
};
CHECK_SIZE(ScriptTokenReader, 0x10);

extern "C"
{
    // A tagged value given a token's value as its type has it (a property reference whatever its type, an angle's degrees made
    // 65536ths of a turn)
    void ParseTaggedValueRecord(const ScriptToken* token, TaggedValue* value) RETAIL(ParseTaggedValueRecord);
}

// Helpers the commands' parsers share
extern "C"
{
    // A token's value as a two bit setting (1 for 0, 2 for anything else), and whether it's 0
    u32 TokenSetting(const ScriptToken* token) RETAIL(FUN_00208510);
    bool TokenIsZero(const ScriptToken* token) RETAIL(FUN_002084f0);
    // TokenIsZero again (the same code at another address)
    bool TokenIsZero2(const ScriptToken* token) RETAIL(FUN_00208500);
    // The designator a token names (a keyword's, kind 6's value, kind 7's less one) and the space a keyword names, the one given
    // when it names none
    u32 TokenDesignator(const ScriptToken* token, u32 designator) RETAIL(FUN_00208528);
    u32 TokenSpace(const ScriptToken* token, u32 space) RETAIL(FUN_00208550);
    // What they read: the designator or the space set when the token names one
    void ParseDesignatorToken(const ScriptToken* token, u32* designator) RETAIL(FUN_002066a0);
    void ParseSpaceToken(const ScriptToken* token, u32* space) RETAIL(FUN_002085c0);
    // A tagged value from a whole list of tokens: kind 0x38 makes it an angle, 0x39 a float, and gives it the token's value
    void ParseTaggedValueTokens(const ScriptTokenList* tokens, TaggedValue* value) RETAIL(ParseTaggedValueTokens);
    // Kinds 0, 1 and 2 set a vector's x, y and z
    void TokenVectorComponent(const ScriptToken* token, f32* vector) RETAIL(FUN_00208578);
    // The tokens of a motion block's arguments (the springy contacts' and the launches') read into it: a body's values, kind,
    // constraint, bits and centre of mass
    void ParseMotionBlockTokens(const ScriptTokenList* tokens, MotionBlock* block) RETAIL(FUN_00205fe8);
    // The character a keyword names (0x232 to 0x235), 6 for any other
    u32 TokenCharacter(u32 token) RETAIL(FUN_0013fe60);
    // The head tracking settings' angles (the command's arguments from value0 on) made from degrees, with angle1's sine in dirY,
    // minus angle2's in dirX and angle3's cosine in dirZ (which also sets bit 27 of the animations)
    void SetHeadTrackingAngle1(void* settings, f32 degrees) RETAIL_N32(FUN_0023ee18);
    void SetHeadTrackingAngle2(void* settings, f32 degrees) RETAIL_N32(FUN_0023edd0);
    void SetHeadTrackingAngle3(void* settings, f32 degrees) RETAIL_N32(FUN_0023ee60);
}
