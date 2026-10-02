#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/math.h"
#include "game/reference.h"

// What controls a character (a vehicle while it rides one, still asm): its vtable at 0xD4 (8 its kind: 6 the slider's)
struct CharacterControl
{
    // Bit 2 the characters' flag commands 650 and 651 set and clear
    u64 bits;
    u8 unknown08[0xD4 - 8];
    const GccVTableEntry* vtable;

    u32 Kind()
    {
        return CallVirtual<u32>(this, vtable, 8);
    }
};

// What the HUD's counter at the bottom right counts of a character (still asm): the count in bits 13-19 of its second double
// word
struct CharacterCounter
{
    u64 unknown00;
    u64 bits;

    u32 Count() const
    {
        return static_cast<u32>(bits >> 13) & 0x7F;
    }
};

// A character's data (still asm): bit 54 of its bits is set in the second of two characters tied together
struct CharacterData
{
    enum Bits : u64
    {
        BitLinkedSecond = 1ull << 54,
    };

    u8 unknown00[0x18];
    u64 bits;
};

// The character a player plays (the retail player character, still asm): its data, what the HUD's counter counts of it, its link
// to another character (two tied together), what controls it and what it's told to do (a vector at 0xE0)
struct PlayerCharacter
{
    u8 unknown00[0x10];
    CharacterData* data;
    u8 unknown14[0xA0 - 0x14];
    // Its attack's state in the low 5 bits of the first word (12 to 14 a downward blast; still asm)
    const u32* attack;
    CharacterCounter* counter;
    u8 unknownA8[0xB0 - 0xA8];
    void* link;
    u8 unknownB4[0xB8 - 0xB4];
    CharacterControl* control;
    u8 unknownBC[0xE0 - 0xBC];
    Vector4 input;
};
CHECK_OFFSET(PlayerCharacter, attack, 0xA0);
CHECK_OFFSET(PlayerCharacter, counter, 0xA4);
CHECK_OFFSET(PlayerCharacter, link, 0xB0);
CHECK_OFFSET(PlayerCharacter, control, 0xB8);
CHECK_OFFSET(PlayerCharacter, input, 0xE0);

extern "C"
{
    // The player: its instance (a reference), its character (the scripts' conditions read the second copy) and the characters'
    // data
    extern Reference* g_PlayerInstance RETAIL(G_UnkInstanceContextRefCounter);
    extern PlayerCharacter* g_PlayerCharacter RETAIL(G_UnkPlayableCharObjInstCxt);
    extern PlayerCharacter* g_PlayerCharacter2 RETAIL(G_UnkPlayableCharObjInstCxt2);
    extern void* g_PlayerCharacterData RETAIL(G_UnkCreationHelper);
    extern void* g_PlayerCharacterData2 RETAIL(G_UnkCreationHelper2);

    // The character given another as its vehicle, of a kind (still asm)
    void SetPlayerVehicle(PlayerCharacter* character, u32 kind, PlayerCharacter* other, u32 unknown) RETAIL(SetPlayerVehicle);
    // Two characters tied together, each given the other, and untied (by the first; still asm)
    void LinkCharacters(PlayerCharacter* character, PlayerCharacter* other) RETAIL(FUN_00132ca8);
    void UnlinkCharacters(PlayerCharacter* character) RETAIL(FUN_00132dc0);
    // The other character of a link (still asm)
    PlayerCharacter* LinkedCharacter(void* link) RETAIL(FUN_0015fdc0);
    // The character's own part of the overlay: what controls it (at 0xB8) draws it, its vtable's slot 13 (still asm)
    void DrawCharacterOverlay(PlayerCharacter* character) RETAIL(FUN_0013fe28);
    // A vehicle's gauge, 0 to 1 (by its kind: its speed; still asm)
    f32 VehicleGauge(CharacterControl* vehicle) RETAIL(FUN_00161198);
}
