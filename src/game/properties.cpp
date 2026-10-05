#include "game/properties.h"

#include "game/math.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/stream.h"

namespace
{
// The classes the holders' vtable function 12 gives (the factory hands it to PropertyList's unused parameter, nothing else reads
// it): the playable characters', crates' and creatures' holders', and the others'
constexpr u32 CharacterCrateCreatureClass = 0x13;
constexpr u32 OtherHolderClass = 0x12;

// The extras' values (beyond the 0x20 bytes allocated when there are more of them, in retail)
u32* ValuesOf(PropertyExtras* extras)
{
    return reinterpret_cast<u32*>(extras->counts + sizeof(extras->counts));
}

// Where the extras of a kind start
u32 FirstOf(const PropertyExtras* extras, u32 kind)
{
    u32 first = 0;
    for (u32 before = 0; before < kind; before++)
    {
        first += extras->counts[before];
    }

    return first;
}
}

TaggedValue* TaggedValue::FromFloat(TaggedValue* value, u32 unit, f32 number)
{
    AngleFrom(&value->raw, number, unit);
    return value;
}

void TaggedValue::Read(Stream* stream)
{
    stream->ReadU32(reinterpret_cast<u32*>(&raw));
}

PropertyList* PropertyList::Construct(PropertyList* list, Stream* stream)
{
    list->vtable = g_PropertyListVTable;
    list->taggedCount = 0;
    list->tagged = nullptr;
    list->floatCount = 0;
    list->floats = nullptr;
    list->intCount = 0;
    list->ints = nullptr;
    list->Read(stream);
    return list;
}

PropertyList* PropertyList::Construct(PropertyList* list, u32 taggedCount, u32 floatCount, u32 intCount, u32)
{
    list->vtable = g_PropertyListVTable;
    list->state.value = 0;
    list->taggedCount = taggedCount;
    list->tagged = taggedCount != 0 ? static_cast<TaggedValue*>(MemoryAllocate2(taggedCount * sizeof(TaggedValue))) : nullptr;
    list->floatCount = floatCount;
    list->floats = floatCount != 0 ? static_cast<f32*>(MemoryAllocate2(floatCount * sizeof(f32))) : nullptr;
    list->intCount = intCount;
    list->ints = intCount != 0 ? static_cast<s32*>(MemoryAllocate2(intCount * sizeof(s32))) : nullptr;
    *reinterpret_cast<u32*>(list->counts) = 0;
    list->counts[TaggedProperties] = taggedCount;
    list->counts[FloatProperties] = floatCount;
    list->counts[IntProperties] = intCount;
    return list;
}

void PropertyList::Destroy(u32 destroyFlags)
{
    vtable = g_PropertyListVTable;
    if (ints != nullptr)
    {
        MemoryDeallocate_(ints);
    }

    if (floats != nullptr)
    {
        MemoryDeallocate_(floats);
    }

    if (tagged != nullptr)
    {
        MemoryDeallocate_(tagged);
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PropertyList::Read(Stream* stream)
{
    stream->ReadS32(reinterpret_cast<s32*>(counts));
    stream->ReadS32(reinterpret_cast<s32*>(&state));
    if (tagged != nullptr)
    {
        MemoryDeallocate_(tagged);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&taggedCount));
    tagged = taggedCount != 0 ? static_cast<TaggedValue*>(MemoryAllocate2(taggedCount * sizeof(TaggedValue))) : nullptr;
    for (u32 index = 0; index < taggedCount; index++)
    {
        stream->ReadU32(reinterpret_cast<u32*>(&tagged[index].raw));
    }

    if (floats != nullptr)
    {
        MemoryDeallocate_(floats);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&floatCount));
    floats = floatCount != 0 ? static_cast<f32*>(MemoryAllocate2(floatCount * sizeof(f32))) : nullptr;
    for (u32 index = 0; index < floatCount; index++)
    {
        stream->ReadF32(&floats[index]);
    }

    if (ints != nullptr)
    {
        MemoryDeallocate_(ints);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&intCount));
    ints = intCount != 0 ? static_cast<s32*>(MemoryAllocate2(intCount * sizeof(s32))) : nullptr;
    for (u32 index = 0; index < intCount; index++)
    {
        stream->ReadU32(reinterpret_cast<u32*>(&ints[index]));
    }
}

TaggedValue* PropertyList::TaggedAt(TaggedValue* value, const PropertyList* list, u32 index)
{
    *value = list->tagged[index];
    return value;
}

f32 PropertyList::FloatAt(u32 index)
{
    return floats[index];
}

s32 PropertyList::IntAt(u32 index)
{
    return ints[index];
}

void PropertyList::SetTagged(u32 index, const TaggedValue* value)
{
    tagged[index] = *value;
}

void PropertyList::SetFloat(u32 index, f32 value)
{
    floats[index] = value;
}

EABI_EXPORT(FUN_00263080, &PropertyList::SetFloat);

void PropertyList::SetInt(u32 index, s32 value)
{
    ints[index] = value;
}

PropertyExtras* PropertyExtras::Construct(PropertyExtras* extras, PropertyList* list, PropertyHolder* holder)
{
    u32 holderTagged = holder->TaggedCount();
    u32 holderFloats = holder->FloatCount();
    u32 holderInts = holder->IntCount();
    extras->counts[TaggedProperties] = static_cast<u8>(list->counts[TaggedProperties] - holderTagged);
    extras->counts[FloatProperties] = static_cast<u8>(list->counts[FloatProperties] - holderFloats);
    extras->counts[IntProperties] = static_cast<u8>(list->counts[IntProperties] - holderInts);
    for (u32 index = 0; index < extras->counts[TaggedProperties]; index++)
    {
        TaggedValue value = list->tagged[holderTagged + index];
        extras->SetTagged(index, &value);
    }

    for (u32 index = 0; index < extras->counts[FloatProperties]; index++)
    {
        extras->SetFloat(index, list->floats[holderFloats + index]);
    }

    for (u32 index = 0; index < extras->counts[IntProperties]; index++)
    {
        extras->SetInt(index, list->ints[holderInts + index]);
    }

    return extras;
}

void PropertyExtras::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PropertyExtras::SetTagged(u32 index, const TaggedValue* value)
{
    f32 radians = static_cast<f32>(value->raw) * AngleToRadians;
    ValuesOf(this)[index] = __builtin_bit_cast(u32, radians);
}

void PropertyExtras::SetFloat(u32 index, f32 value)
{
    ValuesOf(this)[FirstOf(this, FloatProperties) + index] = __builtin_bit_cast(u32, value);
}

EABI_EXPORT(AddExtraFloat, &PropertyExtras::SetFloat);

void PropertyExtras::SetInt(u32 index, s32 value)
{
    ValuesOf(this)[FirstOf(this, IntProperties) + index] = static_cast<u32>(value);
}

TaggedValue* PropertyExtras::TaggedAt(TaggedValue* value, PropertyExtras* extras, u32 index)
{
    TaggedValue made;
    TaggedValue::FromFloat(&made, AngleRadians, __builtin_bit_cast(f32, ValuesOf(extras)[index]));
    *value = made;
    return value;
}

f32 PropertyExtras::FloatAt(u32 index)
{
    return __builtin_bit_cast(f32, ValuesOf(this)[FirstOf(this, FloatProperties) + index]);
}

s32 PropertyExtras::IntAt(u32 index)
{
    return static_cast<s32>(ValuesOf(this)[FirstOf(this, IntProperties) + index]);
}

PropertyHolder* PropertyHolder::Construct(PropertyHolder* holder)
{
    holder->state.value = 0;
    holder->vtable = g_PropertyHolderVTable;
    holder->extras = nullptr;
    return holder;
}

void PropertyHolder::Destroy(u32 destroyFlags)
{
    vtable = g_PropertyHolderVTable;
    if (extras != nullptr)
    {
        extras->Destroy(DestroyAndFree);
    }

    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

TaggedValue* PropertyHolder::GetTagged(TaggedValue* value, PropertyHolder* holder, u32 index)
{
    u32 count = holder->TaggedCount();
    if (index < count)
    {
        *value = *CallVirtual<TaggedValue*>(holder, holder->vtable, TaggedReadSlot, index);
        return value;
    }

    if (holder->extras == nullptr)
    {
        return TaggedValue::FromFloat(value, AngleRadians, 0.0f);
    }

    PropertyExtras::TaggedAt(value, holder->extras, index - count);
    return value;
}

f32 PropertyHolder::GetFloat(u32 index)
{
    u32 count = FloatCount();
    if (index < count)
    {
        return *CallVirtual<f32*>(this, vtable, FloatReadSlot, index);
    }

    if (extras == nullptr)
    {
        return 0.0f;
    }

    return extras->FloatAt(index - count);
}

s32 PropertyHolder::GetInt(u32 index)
{
    u32 count = IntCount();
    if (index < count)
    {
        return *CallVirtual<s32*>(this, vtable, IntReadSlot, index);
    }

    if (extras == nullptr)
    {
        return 0;
    }

    return extras->IntAt(index - count);
}

void PropertyHolder::SetTagged(u32 index, const TaggedValue* value)
{
    u32 count = TaggedCount();
    if (index < count)
    {
        *CallVirtual<TaggedValue*>(this, vtable, TaggedWriteSlot, index) = *value;
        return;
    }

    if (extras != nullptr)
    {
        TaggedValue copy = *value;
        extras->SetTagged(index - count, &copy);
    }
}

void PropertyHolder::SetFloat(u32 index, f32 value)
{
    u32 count = FloatCount();
    if (index < count)
    {
        *CallVirtual<f32*>(this, vtable, FloatWriteSlot, index) = value;
        return;
    }

    if (extras != nullptr)
    {
        extras->SetFloat(index - count, value);
    }
}

EABI_EXPORT(StoreInstanceFloatInHolder, &PropertyHolder::SetFloat);
EABI_EXPORT(FUN_002098d8, &TaggedValue::MakeFloat);
EABI_EXPORT(SetTaggedFloatBits, &TaggedValue::SetFloat);

void PropertyHolder::SetInt(u32 index, s32 value)
{
    u32 count = IntCount();
    if (index < count)
    {
        *CallVirtual<s32*>(this, vtable, IntWriteSlot, index) = value;
        return;
    }

    if (extras != nullptr)
    {
        extras->SetInt(index - count, value);
    }
}

void PropertyHolder::CopyFrom(PropertyList* list)
{
    s32 listTagged = list->counts[TaggedProperties];
    s32 listFloats = list->counts[FloatProperties];
    s32 listInts = list->counts[IntProperties];
    s32 holderTagged = static_cast<s32>(TaggedCount());
    s32 holderFloats = static_cast<s32>(FloatCount());
    s32 holderInts = static_cast<s32>(IntCount());
    s32 taggedCopied = holderTagged < listTagged ? holderTagged : listTagged;
    s32 floatsCopied = holderFloats < listFloats ? holderFloats : listFloats;
    s32 intsCopied = holderInts < listInts ? holderInts : listInts;
    for (s32 index = 0; index < taggedCopied; index++)
    {
        TaggedValue value;
        PropertyList::TaggedAt(&value, list, static_cast<u32>(index));
        SetTagged(static_cast<u32>(index), &value);
    }

    for (s32 index = 0; index < floatsCopied; index++)
    {
        SetFloat(static_cast<u32>(index), list->FloatAt(static_cast<u32>(index)));
    }

    for (s32 index = 0; index < intsCopied; index++)
    {
        SetInt(static_cast<u32>(index), list->IntAt(static_cast<u32>(index)));
    }

    state = list->state;
    if (holderTagged < listTagged || holderFloats < listFloats || holderInts < listInts)
    {
        extras = PropertyExtras::Construct(static_cast<PropertyExtras*>(MemoryAllocate(sizeof(PropertyExtras))), list, this);
    }
}

extern "C"
{
    // The typed holders' base (its vtable's destructor the only function), and the holders' vtable functions under their retail
    // names: 1 to 3 the places of a tagged value, a float and an integer to read, 4 to 6 to write, 7 the destructor, 8 the type,
    // 9 to 11 the counts, 12 the class's 0x12 or 0x13
    extern const GccVTableEntry g_TypedPropertyHolderVTable[] RETAIL(D_002F2F70);
    void TypedHolderDestroy(PropertyHolder* holder, u32 flags) RETAIL(FUN_00141bd0);
    extern const GccVTableEntry g_CharacterHolderVTable[] RETAIL(InstancePropsHolderPlayableCharacter_Methods);
    TaggedValue* CharacterHolderTaggedPlace(CharacterPropertyHolder* holder, u32 index) RETAIL(FUN_0013e5c0);
    f32* CharacterHolderFloatPlace(CharacterPropertyHolder* holder, u32 index) RETAIL(FUN_0013e5d0);
    s32* CharacterHolderIntPlace(CharacterPropertyHolder* holder, u32 index) RETAIL(FUN_0013e5e0);
    TaggedValue* CharacterHolderTaggedToWrite(CharacterPropertyHolder* holder, u32 index) RETAIL(GetPlayableCharacterFlag);
    f32* CharacterHolderFloatToWrite(CharacterPropertyHolder* holder, u32 index) RETAIL(GetPlayableCharacterFloat);
    s32* CharacterHolderIntToWrite(CharacterPropertyHolder* holder, u32 index) RETAIL(GetPlayableCharacterInteger);
    void CharacterHolderDestroy(CharacterPropertyHolder* holder, u32 flags) RETAIL(FUN_00140048);
    u32 CharacterHolderType(CharacterPropertyHolder* holder) RETAIL(FUN_0013e620);
    u32 CharacterHolderTaggedCount(CharacterPropertyHolder* holder) RETAIL(FUN_0013e628);
    u32 CharacterHolderFloatCount(CharacterPropertyHolder* holder) RETAIL(FUN_0013e630);
    u32 CharacterHolderIntCount(CharacterPropertyHolder* holder) RETAIL(FUN_0013e638);
    u32 CharacterHolderClassId(CharacterPropertyHolder* holder) RETAIL(FUN_0013e640);

    extern const GccVTableEntry g_PickupHolderVTable[] RETAIL(InstancePropsHolderType1_Methods);
    TaggedValue* PickupHolderTaggedPlace(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013ee88);
    f32* PickupHolderFloatPlace(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013ee98);
    s32* PickupHolderIntPlace(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013eea8);
    TaggedValue* PickupHolderTaggedToWrite(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013eeb8);
    f32* PickupHolderFloatToWrite(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013eec8);
    s32* PickupHolderIntToWrite(PickupPropertyHolder* holder, u32 index) RETAIL(FUN_0013eed8);
    void PickupHolderDestroy(PickupPropertyHolder* holder, u32 flags) RETAIL(FUN_00141a88);
    u32 PickupHolderType(PickupPropertyHolder* holder) RETAIL(FUN_0013eee8);
    u32 PickupHolderTaggedCount(PickupPropertyHolder* holder) RETAIL(FUN_0013eef0);
    u32 PickupHolderFloatCount(PickupPropertyHolder* holder) RETAIL(FUN_0013eef8);
    u32 PickupHolderIntCount(PickupPropertyHolder* holder) RETAIL(FUN_0013ef00);
    u32 PickupHolderClassId(PickupPropertyHolder* holder) RETAIL(FUN_0013ef08);

    extern const GccVTableEntry g_CrateHolderVTable[] RETAIL(InstancePropsHolderType2_Methods);
    TaggedValue* CrateHolderTaggedPlace(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eae0);
    f32* CrateHolderFloatPlace(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eaf0);
    s32* CrateHolderIntPlace(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eb00);
    TaggedValue* CrateHolderTaggedToWrite(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eb10);
    f32* CrateHolderFloatToWrite(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eb20);
    s32* CrateHolderIntToWrite(CratePropertyHolder* holder, u32 index) RETAIL(FUN_0013eb30);
    void CrateHolderDestroy(CratePropertyHolder* holder, u32 flags) RETAIL(FUN_00140778);
    u32 CrateHolderType(CratePropertyHolder* holder) RETAIL(FUN_0013eb40);
    u32 CrateHolderTaggedCount(CratePropertyHolder* holder) RETAIL(FUN_0013eb48);
    u32 CrateHolderFloatCount(CratePropertyHolder* holder) RETAIL(FUN_0013eb50);
    u32 CrateHolderIntCount(CratePropertyHolder* holder) RETAIL(FUN_0013eb58);
    u32 CrateHolderClassId(CratePropertyHolder* holder) RETAIL(FUN_0013eb60);

    extern const GccVTableEntry g_CreatureHolderVTable[] RETAIL(InstancePropsHolderType3_Methods);
    TaggedValue* CreatureHolderTaggedPlace(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e538);
    f32* CreatureHolderFloatPlace(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e548);
    s32* CreatureHolderIntPlace(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e558);
    TaggedValue* CreatureHolderTaggedToWrite(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e568);
    f32* CreatureHolderFloatToWrite(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e578);
    s32* CreatureHolderIntToWrite(CreaturePropertyHolder* holder, u32 index) RETAIL(FUN_0013e588);
    void CreatureHolderDestroy(CreaturePropertyHolder* holder, u32 flags) RETAIL(FUN_00140d60);
    u32 CreatureHolderType(CreaturePropertyHolder* holder) RETAIL(FUN_0013e598);
    u32 CreatureHolderTaggedCount(CreaturePropertyHolder* holder) RETAIL(FUN_0013e5a0);
    u32 CreatureHolderFloatCount(CreaturePropertyHolder* holder) RETAIL(FUN_0013e5a8);
    u32 CreatureHolderIntCount(CreaturePropertyHolder* holder) RETAIL(FUN_0013e5b0);
    u32 CreatureHolderClassId(CreaturePropertyHolder* holder) RETAIL(FUN_0013e5b8);

    extern const GccVTableEntry g_GenericObjectHolderVTable[] RETAIL(InstancePropsHolderType4_Methods);
    TaggedValue* GenericObjectHolderTaggedPlace(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ecb8);
    f32* GenericObjectHolderFloatPlace(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ecc8);
    s32* GenericObjectHolderIntPlace(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ecd8);
    TaggedValue* GenericObjectHolderTaggedToWrite(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ece8);
    f32* GenericObjectHolderFloatToWrite(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ecf8);
    s32* GenericObjectHolderIntToWrite(GenericObjectPropertyHolder* holder, u32 index) RETAIL(FUN_0013ed08);
    void GenericObjectHolderDestroy(GenericObjectPropertyHolder* holder, u32 flags) RETAIL(FUN_00141158);
    u32 GenericObjectHolderType(GenericObjectPropertyHolder* holder) RETAIL(FUN_0013ed18);
    u32 GenericObjectHolderTaggedCount(GenericObjectPropertyHolder* holder) RETAIL(FUN_0013ed20);
    u32 GenericObjectHolderFloatCount(GenericObjectPropertyHolder* holder) RETAIL(FUN_0013ed28);
    u32 GenericObjectHolderIntCount(GenericObjectPropertyHolder* holder) RETAIL(FUN_0013ed30);
    u32 GenericObjectHolderClassId(GenericObjectPropertyHolder* holder) RETAIL(FUN_0013ed38);

    extern const GccVTableEntry g_GrabbableHolderVTable[] RETAIL(InstancePropsHolderType5_Methods);
    TaggedValue* GrabbableHolderTaggedPlace(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013e9b0);
    f32* GrabbableHolderFloatPlace(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013e9c0);
    s32* GrabbableHolderIntPlace(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013e9d0);
    TaggedValue* GrabbableHolderTaggedToWrite(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013e9e0);
    f32* GrabbableHolderFloatToWrite(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013e9f0);
    s32* GrabbableHolderIntToWrite(GrabbablePropertyHolder* holder, u32 index) RETAIL(FUN_0013ea00);
    void GrabbableHolderDestroy(GrabbablePropertyHolder* holder, u32 flags) RETAIL(FUN_001403f0);
    u32 GrabbableHolderType(GrabbablePropertyHolder* holder) RETAIL(FUN_0013ea10);
    u32 GrabbableHolderTaggedCount(GrabbablePropertyHolder* holder) RETAIL(FUN_0013ea18);
    u32 GrabbableHolderFloatCount(GrabbablePropertyHolder* holder) RETAIL(FUN_0013ea20);
    u32 GrabbableHolderIntCount(GrabbablePropertyHolder* holder) RETAIL(FUN_0013ea28);
    u32 GrabbableHolderClassId(GrabbablePropertyHolder* holder) RETAIL(FUN_0013ea30);

    extern const GccVTableEntry g_PayGateHolderVTable[] RETAIL(InstancePropsHolderType6_Methods);
    TaggedValue* PayGateHolderTaggedPlace(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013edd8);
    f32* PayGateHolderFloatPlace(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013ede8);
    s32* PayGateHolderIntPlace(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013edf8);
    TaggedValue* PayGateHolderTaggedToWrite(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013ee08);
    f32* PayGateHolderFloatToWrite(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013ee18);
    s32* PayGateHolderIntToWrite(PayGatePropertyHolder* holder, u32 index) RETAIL(FUN_0013ee28);
    void PayGateHolderDestroy(PayGatePropertyHolder* holder, u32 flags) RETAIL(FUN_00141888);
    u32 PayGateHolderType(PayGatePropertyHolder* holder) RETAIL(FUN_0013ee38);
    u32 PayGateHolderTaggedCount(PayGatePropertyHolder* holder) RETAIL(FUN_0013ee40);
    u32 PayGateHolderFloatCount(PayGatePropertyHolder* holder) RETAIL(FUN_0013ee48);
    u32 PayGateHolderIntCount(PayGatePropertyHolder* holder) RETAIL(FUN_0013ee50);
    u32 PayGateHolderClassId(PayGatePropertyHolder* holder) RETAIL(FUN_0013ee58);

    extern const GccVTableEntry g_GrapleHolderVTable[] RETAIL(InstancePropsHolderType7_Methods);
    TaggedValue* GrapleHolderTaggedPlace(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec28);
    f32* GrapleHolderFloatPlace(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec38);
    s32* GrapleHolderIntPlace(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec48);
    TaggedValue* GrapleHolderTaggedToWrite(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec58);
    f32* GrapleHolderFloatToWrite(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec68);
    s32* GrapleHolderIntToWrite(GraplePropertyHolder* holder, u32 index) RETAIL(FUN_0013ec78);
    void GrapleHolderDestroy(GraplePropertyHolder* holder, u32 flags) RETAIL(FUN_00140fb0);
    u32 GrapleHolderType(GraplePropertyHolder* holder) RETAIL(FUN_0013ec88);
    u32 GrapleHolderTaggedCount(GraplePropertyHolder* holder) RETAIL(FUN_0013ec90);
    u32 GrapleHolderFloatCount(GraplePropertyHolder* holder) RETAIL(FUN_0013ec98);
    u32 GrapleHolderIntCount(GraplePropertyHolder* holder) RETAIL(FUN_0013eca0);
    u32 GrapleHolderClassId(GraplePropertyHolder* holder) RETAIL(FUN_0013eca8);

    TaggedValue* ProjectileHolderTaggedPlace(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013efb0);
    f32* ProjectileHolderFloatPlace(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013efc0);
    s32* ProjectileHolderIntPlace(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013efd0);
    TaggedValue* ProjectileHolderTaggedToWrite(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013efe0);
    f32* ProjectileHolderFloatToWrite(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013eff0);
    s32* ProjectileHolderIntToWrite(ProjectilePropertyHolder* holder, u32 index) RETAIL(FUN_0013f000);
    void ProjectileHolderDestroy(ProjectilePropertyHolder* holder, u32 flags) RETAIL(FUN_0013f010);
    u32 ProjectileHolderType(ProjectilePropertyHolder* holder) RETAIL(FUN_0013f038);
    u32 ProjectileHolderTaggedCount(ProjectilePropertyHolder* holder) RETAIL(FUN_0013f040);
    u32 ProjectileHolderFloatCount(ProjectilePropertyHolder* holder) RETAIL(FUN_0013f048);
    u32 ProjectileHolderIntCount(ProjectilePropertyHolder* holder) RETAIL(FUN_0013f050);
    u32 ProjectileHolderClassId(ProjectilePropertyHolder* holder) RETAIL(FUN_0013f058);
}

namespace
{
// The typed holders' destructors are their base's
void DestroyTypedHolder(PropertyHolder* holder, u32 flags)
{
    holder->vtable = g_TypedPropertyHolderVTable;
    holder->Destroy(flags);
}

// A typed holder made with its vtable (the arrays' elements have nothing to make), the list's values copied in when there's one
template <typename Holder>
Holder* ConstructTyped(Holder* holder, PropertyList* list, const GccVTableEntry* vtable)
{
    PropertyHolder::Construct(holder);
    holder->vtable = vtable;
    if (list != nullptr)
    {
        holder->CopyFrom(list);
    }

    return holder;
}
}

void TypedHolderDestroy(PropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

// Made as type 3's first
CharacterPropertyHolder* ConstructCharacterPropertyHolder(CharacterPropertyHolder* holder, PropertyList* list)
{
    PropertyHolder::Construct(holder);
    holder->vtable = g_CreatureHolderVTable;
    holder->vtable = g_CharacterHolderVTable;
    if (list != nullptr)
    {
        holder->CopyFrom(list);
    }

    return holder;
}

PickupPropertyHolder* ConstructPickupPropertyHolder(PickupPropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_PickupHolderVTable);
}

CratePropertyHolder* ConstructCratePropertyHolder(CratePropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_CrateHolderVTable);
}

CreaturePropertyHolder* ConstructCreaturePropertyHolder(CreaturePropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_CreatureHolderVTable);
}

GenericObjectPropertyHolder* ConstructGenericObjectPropertyHolder(GenericObjectPropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_GenericObjectHolderVTable);
}

GrabbablePropertyHolder* ConstructGrabbablePropertyHolder(GrabbablePropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_GrabbableHolderVTable);
}

PayGatePropertyHolder* ConstructPayGatePropertyHolder(PayGatePropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_PayGateHolderVTable);
}

GraplePropertyHolder* ConstructGraplePropertyHolder(GraplePropertyHolder* holder, PropertyList* list)
{
    return ConstructTyped(holder, list, g_GrapleHolderVTable);
}

TaggedValue* CharacterHolderTaggedPlace(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterTagged + index;
}

f32* CharacterHolderFloatPlace(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterFloats + index;
}

s32* CharacterHolderIntPlace(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterInts + index;
}

TaggedValue* CharacterHolderTaggedToWrite(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterTagged + index;
}

f32* CharacterHolderFloatToWrite(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterFloats + index;
}

s32* CharacterHolderIntToWrite(CharacterPropertyHolder* holder, u32 index)
{
    return holder->characterInts + index;
}

void CharacterHolderDestroy(CharacterPropertyHolder* holder, u32 flags)
{
    holder->vtable = g_CharacterHolderVTable;
    CreatureHolderDestroy(holder, flags);
}

u32 CharacterHolderType(CharacterPropertyHolder*)
{
    return GameObject::TypeCharacter;
}

u32 CharacterHolderTaggedCount(CharacterPropertyHolder*)
{
    return CharacterPropertyHolder::KeptTagged;
}

u32 CharacterHolderFloatCount(CharacterPropertyHolder*)
{
    return CharacterPropertyHolder::KeptFloats;
}

u32 CharacterHolderIntCount(CharacterPropertyHolder*)
{
    return CharacterPropertyHolder::KeptInts;
}

u32 CharacterHolderClassId(CharacterPropertyHolder*)
{
    return CharacterCrateCreatureClass;
}

TaggedValue* PickupHolderTaggedPlace(PickupPropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* PickupHolderFloatPlace(PickupPropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* PickupHolderIntPlace(PickupPropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* PickupHolderTaggedToWrite(PickupPropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* PickupHolderFloatToWrite(PickupPropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* PickupHolderIntToWrite(PickupPropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void PickupHolderDestroy(PickupPropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 PickupHolderType(PickupPropertyHolder*)
{
    return GameObject::TypePickup;
}

u32 PickupHolderTaggedCount(PickupPropertyHolder*)
{
    return PickupPropertyHolder::KeptTagged;
}

u32 PickupHolderFloatCount(PickupPropertyHolder*)
{
    return PickupPropertyHolder::KeptFloats;
}

u32 PickupHolderIntCount(PickupPropertyHolder*)
{
    return PickupPropertyHolder::KeptInts;
}

u32 PickupHolderClassId(PickupPropertyHolder*)
{
    return OtherHolderClass;
}

TaggedValue* CrateHolderTaggedPlace(CratePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* CrateHolderFloatPlace(CratePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* CrateHolderIntPlace(CratePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* CrateHolderTaggedToWrite(CratePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* CrateHolderFloatToWrite(CratePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* CrateHolderIntToWrite(CratePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void CrateHolderDestroy(CratePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 CrateHolderType(CratePropertyHolder*)
{
    return GameObject::TypeCrate;
}

u32 CrateHolderTaggedCount(CratePropertyHolder*)
{
    return CratePropertyHolder::KeptTagged;
}

u32 CrateHolderFloatCount(CratePropertyHolder*)
{
    return CratePropertyHolder::KeptFloats;
}

u32 CrateHolderIntCount(CratePropertyHolder*)
{
    return CratePropertyHolder::KeptInts;
}

u32 CrateHolderClassId(CratePropertyHolder*)
{
    return CharacterCrateCreatureClass;
}

TaggedValue* CreatureHolderTaggedPlace(CreaturePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* CreatureHolderFloatPlace(CreaturePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* CreatureHolderIntPlace(CreaturePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* CreatureHolderTaggedToWrite(CreaturePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* CreatureHolderFloatToWrite(CreaturePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* CreatureHolderIntToWrite(CreaturePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void CreatureHolderDestroy(CreaturePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 CreatureHolderType(CreaturePropertyHolder*)
{
    return GameObject::TypeCreature;
}

u32 CreatureHolderTaggedCount(CreaturePropertyHolder*)
{
    return CreaturePropertyHolder::KeptTagged;
}

u32 CreatureHolderFloatCount(CreaturePropertyHolder*)
{
    return CreaturePropertyHolder::KeptFloats;
}

u32 CreatureHolderIntCount(CreaturePropertyHolder*)
{
    return CreaturePropertyHolder::KeptInts;
}

u32 CreatureHolderClassId(CreaturePropertyHolder*)
{
    return CharacterCrateCreatureClass;
}

TaggedValue* GenericObjectHolderTaggedPlace(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GenericObjectHolderFloatPlace(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GenericObjectHolderIntPlace(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* GenericObjectHolderTaggedToWrite(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GenericObjectHolderFloatToWrite(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GenericObjectHolderIntToWrite(GenericObjectPropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void GenericObjectHolderDestroy(GenericObjectPropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 GenericObjectHolderType(GenericObjectPropertyHolder*)
{
    return GameObject::TypeGenericObject;
}

u32 GenericObjectHolderTaggedCount(GenericObjectPropertyHolder*)
{
    return GenericObjectPropertyHolder::KeptTagged;
}

u32 GenericObjectHolderFloatCount(GenericObjectPropertyHolder*)
{
    return GenericObjectPropertyHolder::KeptFloats;
}

u32 GenericObjectHolderIntCount(GenericObjectPropertyHolder*)
{
    return GenericObjectPropertyHolder::KeptInts;
}

u32 GenericObjectHolderClassId(GenericObjectPropertyHolder*)
{
    return OtherHolderClass;
}

TaggedValue* GrabbableHolderTaggedPlace(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GrabbableHolderFloatPlace(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GrabbableHolderIntPlace(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* GrabbableHolderTaggedToWrite(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GrabbableHolderFloatToWrite(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GrabbableHolderIntToWrite(GrabbablePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void GrabbableHolderDestroy(GrabbablePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 GrabbableHolderType(GrabbablePropertyHolder*)
{
    return GameObject::TypeGrabbable;
}

u32 GrabbableHolderTaggedCount(GrabbablePropertyHolder*)
{
    return GrabbablePropertyHolder::KeptTagged;
}

u32 GrabbableHolderFloatCount(GrabbablePropertyHolder*)
{
    return GrabbablePropertyHolder::KeptFloats;
}

u32 GrabbableHolderIntCount(GrabbablePropertyHolder*)
{
    return GrabbablePropertyHolder::KeptInts;
}

u32 GrabbableHolderClassId(GrabbablePropertyHolder*)
{
    return OtherHolderClass;
}

TaggedValue* PayGateHolderTaggedPlace(PayGatePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* PayGateHolderFloatPlace(PayGatePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* PayGateHolderIntPlace(PayGatePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* PayGateHolderTaggedToWrite(PayGatePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* PayGateHolderFloatToWrite(PayGatePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* PayGateHolderIntToWrite(PayGatePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void PayGateHolderDestroy(PayGatePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 PayGateHolderType(PayGatePropertyHolder*)
{
    return GameObject::TypePayGate;
}

u32 PayGateHolderTaggedCount(PayGatePropertyHolder*)
{
    return PayGatePropertyHolder::KeptTagged;
}

u32 PayGateHolderFloatCount(PayGatePropertyHolder*)
{
    return PayGatePropertyHolder::KeptFloats;
}

u32 PayGateHolderIntCount(PayGatePropertyHolder*)
{
    return PayGatePropertyHolder::KeptInts;
}

u32 PayGateHolderClassId(PayGatePropertyHolder*)
{
    return OtherHolderClass;
}

TaggedValue* GrapleHolderTaggedPlace(GraplePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GrapleHolderFloatPlace(GraplePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GrapleHolderIntPlace(GraplePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* GrapleHolderTaggedToWrite(GraplePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* GrapleHolderFloatToWrite(GraplePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* GrapleHolderIntToWrite(GraplePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void GrapleHolderDestroy(GraplePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 GrapleHolderType(GraplePropertyHolder*)
{
    return GameObject::TypeGraple;
}

u32 GrapleHolderTaggedCount(GraplePropertyHolder*)
{
    return GraplePropertyHolder::KeptTagged;
}

u32 GrapleHolderFloatCount(GraplePropertyHolder*)
{
    return GraplePropertyHolder::KeptFloats;
}

u32 GrapleHolderIntCount(GraplePropertyHolder*)
{
    return GraplePropertyHolder::KeptInts;
}

u32 GrapleHolderClassId(GraplePropertyHolder*)
{
    return OtherHolderClass;
}

TaggedValue* ProjectileHolderTaggedPlace(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* ProjectileHolderFloatPlace(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* ProjectileHolderIntPlace(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

TaggedValue* ProjectileHolderTaggedToWrite(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->tagged + index;
}

f32* ProjectileHolderFloatToWrite(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->floats + index;
}

s32* ProjectileHolderIntToWrite(ProjectilePropertyHolder* holder, u32 index)
{
    return holder->ints + index;
}

void ProjectileHolderDestroy(ProjectilePropertyHolder* holder, u32 flags)
{
    DestroyTypedHolder(holder, flags);
}

u32 ProjectileHolderType(ProjectilePropertyHolder*)
{
    return GameObject::TypeProjectile;
}

u32 ProjectileHolderTaggedCount(ProjectilePropertyHolder*)
{
    return ProjectilePropertyHolder::KeptTagged;
}

u32 ProjectileHolderFloatCount(ProjectilePropertyHolder*)
{
    return ProjectilePropertyHolder::KeptFloats;
}

u32 ProjectileHolderIntCount(ProjectilePropertyHolder*)
{
    return ProjectilePropertyHolder::KeptInts;
}

u32 ProjectileHolderClassId(ProjectilePropertyHolder*)
{
    return OtherHolderClass;
}

s32 TaggedValue::IntWith(PropertyHolder* holder) const
{
    if (type != TypeInt)
    {
        return 0;
    }

    if (isProperty != 0)
    {
        return holder->GetInt(propertyIndex);
    }

    return number;
}

f32 TaggedValue::FloatWith(PropertyHolder* holder) const
{
    if (type != TypeFloat)
    {
        return 0.0f;
    }

    if (isProperty != 0)
    {
        return holder->GetFloat(propertyIndex);
    }

    return ValueBits();
}

TaggedValue* TaggedValue::AngleWith(TaggedValue* angle, const TaggedValue* value, PropertyHolder* holder)
{
    if (value->type != TypeAngle)
    {
        AngleFrom(&angle->raw, 0.0f, AngleRadians);
        return angle;
    }

    if (value->isProperty != 0)
    {
        PropertyHolder::GetTagged(angle, holder, value->propertyIndex);
        return angle;
    }

    AngleFrom(&angle->raw, value->ValueBits(), AngleRadians);
    return angle;
}

TaggedValue* TaggedValue::MakeFloat(TaggedValue* value, f32 number)
{
    value->raw = __builtin_bit_cast(s32, number);
    value->isProperty = 0;
    value->type = TypeFloat;
    return value;
}

TaggedValue* TaggedValue::MakeInt(TaggedValue* value, s32 number)
{
    value->raw = 0;
    value->number = number;
    return value;
}

void TaggedValue::SetAngle(const s32* angle)
{
    // Cleared before the angle is read, in retail's order
    isProperty = 0;
    SetValueBits(__builtin_bit_cast(s32, static_cast<f32>(*angle) * AngleToRadians));
}

void TaggedValue::SetFloat(f32 number)
{
    SetValueBits(__builtin_bit_cast(s32, number));
}

void TaggedValue::SetInt(s32 number)
{
    isProperty = 0;
    this->number = number;
}

void TaggedValue::SetProperty(u32, u32 index)
{
    isProperty = 1;
    propertyIndex = index;
}
