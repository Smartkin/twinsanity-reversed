#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"

class Stream;

class PropertyHolder;

// A value the scripts give in one of three units (the AgentLab's tagged values): angles in 65536ths of a turn, made from degrees
// (kind 1), 16.16 fixed point numbers (kind 2) or angles from radians (any other kind). A command's argument of the kind is a word
// whose bits 1-2 are its type (0 an integer, 1 an angle, 2 a float) and bit 0 makes the rest (bits 3-31) an instance property's
// index; otherwise the rest is the value (an integer shifted up 3, the bits of a float or of an angle's radians, their low 3 bits
// the tag's)
struct TaggedValue
{
    enum Tag : s32
    {
        PropertyBit = 0x1,
        TypeMask = 0x6,
        TypeShift = 1,
        ValueShift = 3,
        ValueMask = ~0x7,
    };

    enum Type : u32
    {
        TypeInt,
        TypeAngle,
        TypeFloat,
    };

    s32 raw;

    // The game's AngleFrom (game/math.h)
    static TaggedValue* FromFloat(TaggedValue* value, u32 kind, f32 number);
    void Read(Stream* stream) RETAIL(FUN_0018c190);
    // Its destructor (the commands' destructors call it for each of theirs)
    void Destroy(u32 destroyFlags) RETAIL(DestroyObj_);
    // An argument's value with the properties its index would read: an integer, a float (other types 0) and an angle (made from
    // the radians; other types none), returned through the first argument as GCC 2.9x returns a struct
    s32 IntWith(PropertyHolder* holder) const RETAIL(GetTaggedInt);
    f32 FloatWith(PropertyHolder* holder) const RETAIL(GetTaggedFloat);
    static TaggedValue* AngleWith(TaggedValue* angle, const TaggedValue* value, PropertyHolder* holder) RETAIL(FUN_00209ab0);
    // An argument made a float or an integer (the development tools' parser), and its value set keeping its type: an angle from
    // 65536ths of a turn (the radians' bits, not an index), a float's bits, an integer, an instance property's index
    static TaggedValue* MakeFloat(TaggedValue* value, f32 number) RETAIL_N32(FUN_002098d8);
    static TaggedValue* MakeInt(TaggedValue* value, s32 number) RETAIL(FUN_00209900);
    void SetAngle(const s32* angle) RETAIL(SetTaggedAngle);
    void SetFloat(f32 number) RETAIL_N32(SetTaggedFloatBits);
    void SetInt(s32 number) RETAIL(SetTaggedInt);
    void SetProperty(u32 unused, u32 index) RETAIL(SetTaggedProperty);

    u32 TypeOf() const
    {
        return static_cast<u32>(raw & TypeMask) >> TypeShift;
    }
};
CHECK_SIZE(TaggedValue, 4);

// An instance's or an object's properties as an RM2 has them (vtable at 0x20: 1 the destructor): the counts a class's holder
// goes by (bytes: the tagged values', the floats', the integers'), the instance's state flags, and the three arrays
struct PropertyList
{
    u8 counts[4];
    u32 state;
    TaggedValue* tagged;
    u32 taggedCount;
    f32* floats;
    u32 floatCount;
    s32* ints;
    u32 intCount;
    const GccVTableEntry* vtable;

    // Made empty and read from the stream
    static PropertyList* Construct(PropertyList* list, Stream* stream) RETAIL(InitInstanceProperties);
    // Made with room for as many of each (no state; the holder's class the caller passes is unused)
    static PropertyList* Construct(PropertyList* list, u32 taggedCount, u32 floatCount, u32 intCount, u32 unused)
        RETAIL(FUN_00262e60);
    void Destroy(u32 destroyFlags) RETAIL(FreeInstancePropsList_);
    // Its counts and state, then each array's count and values (the arrays there were freed first)
    void Read(Stream* stream) RETAIL(ReadInstancePropertiesList);
    // The tagged value's returned through the first argument, as GCC 2.9x returns a struct: before the list
    static TaggedValue* TaggedAt(TaggedValue* value, const PropertyList* list, u32 index) RETAIL(GetFlagPropAtIndex_002630B0);
    f32 FloatAt(u32 index) RETAIL(GetFloatPropAtIndex_002630D0);
    s32 IntAt(u32 index) RETAIL(GetIntegerPropAtIndex);
    void SetTagged(u32 index, const TaggedValue* value) RETAIL(FUN_00263058);
    void SetFloat(u32 index, f32 value) RETAIL_N32(FUN_00263080);
    void SetInt(u32 index, s32 value) RETAIL(FUN_00263098);
};
CHECK_SIZE(PropertyList, 0x24);

class PropertyHolder;

// The values an instance has beyond the ones its class's holder keeps (0x20 bytes of the heap): the counts of each (bytes), then
// the tagged values (as radians), the floats and the integers
struct PropertyExtras
{
    u8 counts[4];
    u32 values[7];

    // The list's values the holder doesn't keep
    static PropertyExtras* Construct(PropertyExtras* extras, PropertyList* list, PropertyHolder* holder) RETAIL(FUN_002618b0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_002636c8);
    void SetTagged(u32 index, const TaggedValue* value) RETAIL(AddExtraFlag);
    void SetFloat(u32 index, f32 value) RETAIL_N32(AddExtraFloat);
    void SetInt(u32 index, s32 value) RETAIL(AddExtraInt);
    static TaggedValue* TaggedAt(TaggedValue* value, PropertyExtras* extras, u32 index) RETAIL(FUN_00263788);
    f32 FloatAt(u32 index) RETAIL(GetExtraFloatProp);
    s32 IntAt(u32 index) RETAIL(FUN_00263808);
};
CHECK_SIZE(PropertyExtras, 0x20);

// The properties an instance's class keeps (the retail holders, vtable at 8: 1 to 3 a tagged value's, a float's and an integer's
// place to read, 4 to 6 to write, 7 the destructor, 9 to 11 how many of each the class keeps): the instance's state flags first
// (bit 6 a persistent flag of its chunk, bit 7 kept in the chunk's own store), the values beyond the class's
class PropertyHolder
{
public:
    enum State : u32
    {
        StatePersistentFlag = 0x40,
        StateFlagInChunkStore = 0x80,
    };

    u32 state;
    PropertyExtras* extras;
    const GccVTableEntry* vtable;

    static PropertyHolder* Construct(PropertyHolder* holder) RETAIL(FUN_00261ea8);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00261ec8);
    // A value by its index: the class's while the index is within its count, the extras' past it (none: 0); the tagged value's
    // returned through the first argument, before the holder
    static TaggedValue* GetTagged(TaggedValue* value, PropertyHolder* holder, u32 index) RETAIL(GetFlagPropAtIndex);
    f32 GetFloat(u32 index) RETAIL(GetFloatPropAtIndex);
    s32 GetInt(u32 index) RETAIL(GetInstanceIntProp);
    void SetTagged(u32 index, const TaggedValue* value) RETAIL(StoreInstanceFlagInHolder);
    void SetFloat(u32 index, f32 value) RETAIL_N32(StoreInstanceFloatInHolder);
    void SetInt(u32 index, s32 value) RETAIL(StoreInstanceIntInHolder);
    // The list's values copied in (as many as both have), its state taken, and the list's values the class doesn't keep put
    // in extras of its own
    void CopyFrom(PropertyList* list) RETAIL(CopyInstanceProperties);

    u32 TaggedCount()
    {
        return CallVirtual<u32>(this, vtable, 9);
    }

    u32 FloatCount()
    {
        return CallVirtual<u32>(this, vtable, 10);
    }

    u32 IntCount()
    {
        return CallVirtual<u32>(this, vtable, 11);
    }
};
CHECK_SIZE(PropertyHolder, 0xC);

// The holders of the agents' classes, one per object type (vtable functions 8 to 12: the type, how many tagged values, floats and
// integers it keeps, and 0x12 or 0x13): the base's 0xC bytes, then its tagged values, floats and integers. The types: 0 the
// playable characters, 1 pickups, 2 crates, 3 creatures, 4 generic objects (the game's furniture), 5 grabbables, 6 pay gates, 7
// graples, 8 projectiles
template <u32 Tagged, u32 Floats, u32 Ints>
class TypedPropertyHolder : public PropertyHolder
{
public:
    TaggedValue tagged[Tagged];
    f32 floats[Floats];
    s32 ints[Ints];
};

// The playable characters' holder (type 0): the creatures' with arrays of its own after it
class CharacterPropertyHolder : public TypedPropertyHolder<1, 6, 3>
{
public:
    TaggedValue characterTagged[9];
    f32 characterFloats[0x38];
    s32 characterInts[3];
};
CHECK_OFFSET(CharacterPropertyHolder, characterTagged, 0x34);
CHECK_OFFSET(CharacterPropertyHolder, characterInts, 0x138);
CHECK_SIZE(CharacterPropertyHolder, 0x144);

using PickupPropertyHolder = TypedPropertyHolder<0, 1, 2>;
using CratePropertyHolder = TypedPropertyHolder<0, 3, 2>;
using CreaturePropertyHolder = TypedPropertyHolder<1, 6, 3>;
using GenericObjectPropertyHolder = TypedPropertyHolder<0, 1, 2>;
using GrabbablePropertyHolder = TypedPropertyHolder<1, 4, 2>;
using PayGatePropertyHolder = TypedPropertyHolder<0, 1, 3>;
using GraplePropertyHolder = TypedPropertyHolder<0, 0x12, 2>;
using ProjectilePropertyHolder = TypedPropertyHolder<0, 1, 2>;
CHECK_SIZE(GraplePropertyHolder, 0x5C);

extern "C"
{
    extern const GccVTableEntry g_PropertyListVTable[] RETAIL(D_00303610);
    extern const GccVTableEntry g_PropertyHolderVTable[] RETAIL(InstancePropsHolderBase_Methods);

    // The typed holders made (their class's values none, the list's copied in when there's one; type 8's is made inline by the
    // factory)
    CharacterPropertyHolder* ConstructCharacterPropertyHolder(CharacterPropertyHolder* holder, PropertyList* list)
        RETAIL(InitHolderType0);
    PickupPropertyHolder* ConstructPickupPropertyHolder(PickupPropertyHolder* holder, PropertyList* list) RETAIL(InitHolderType1);
    CratePropertyHolder* ConstructCratePropertyHolder(CratePropertyHolder* holder, PropertyList* list) RETAIL(InitHolderType2);
    CreaturePropertyHolder* ConstructCreaturePropertyHolder(CreaturePropertyHolder* holder, PropertyList* list)
        RETAIL(InitHolderType3);
    GenericObjectPropertyHolder* ConstructGenericObjectPropertyHolder(GenericObjectPropertyHolder* holder, PropertyList* list)
        RETAIL(InitHolderType4);
    GrabbablePropertyHolder* ConstructGrabbablePropertyHolder(GrabbablePropertyHolder* holder, PropertyList* list)
        RETAIL(InitHolderType5);
    PayGatePropertyHolder* ConstructPayGatePropertyHolder(PayGatePropertyHolder* holder, PropertyList* list)
        RETAIL(InitHolderType6);
    GraplePropertyHolder* ConstructGraplePropertyHolder(GraplePropertyHolder* holder, PropertyList* list) RETAIL(InitHolderType7);
    extern const GccVTableEntry g_ProjectileHolderVTable[] RETAIL(InstancePropsHolderType8_Methods);
}
