#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/agents.h"
#include "game/math.h"
#include "game/reference.h"

class CharacterLink;
class CharacterPart;
class Vehicle;

// The character a player plays (the retail player character): game/agents.h's CharacterAgent, whose fields name it
struct PlayerCharacter : CharacterAgent
{
};

extern "C"
{
    // The player: its instance (a reference), its character and its part (the scripts' conditions read the second copies)
    extern Reference* g_PlayerInstance RETAIL(G_UnkInstanceContextRefCounter);
    extern PlayerCharacter* g_PlayerCharacter RETAIL(G_UnkPlayableCharObjInstCxt);
    extern PlayerCharacter* g_PlayerCharacter2 RETAIL(G_UnkPlayableCharObjInstCxt2);
    extern CharacterPart* g_PlayerPart RETAIL(G_UnkCreationHelper);
    extern CharacterPart* g_PlayerPart2 RETAIL(G_UnkCreationHelper2);

    // The character given another as its vehicle, of a kind (CharacterAgent::SetVehicle: the hoverboard armed or not)
    void SetPlayerVehicle(PlayerCharacter* character, u32 kind, PlayerCharacter* other, u32 armed) RETAIL(SetPlayerVehicle);
    // Two characters tied together, each given the other, and untied (by the first)
    void LinkCharacters(PlayerCharacter* character, PlayerCharacter* other) RETAIL(FUN_00132ca8);
    void UnlinkCharacters(PlayerCharacter* character) RETAIL(FUN_00132dc0);
    // The leader, from the second's link (CharacterLink::Leader)
    PlayerCharacter* LinkedCharacter(CharacterLink* link) RETAIL(FUN_0015fdc0);
    // The character's own part of the overlay: the vehicle it rides draws it, its vtable's slot 13
    void DrawCharacterOverlay(PlayerCharacter* character) RETAIL(FUN_0013fe28);
    // The HUD's gauge of a wrestle (kind 6, vehicles.h's WrestleVehicle: only it has the fields read), 0 the creature pinning the
    // character to 1 the character pinning it
    f32 VehicleGauge(Vehicle* vehicle) RETAIL(FUN_00161198);
}
