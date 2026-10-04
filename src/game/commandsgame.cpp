#include "game/commands.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/camerarig.h"
#include "game/clock.h"
#include "game/controllers.h"
#include "game/cutscenereader.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/oleg.h"
#include "game/player.h"
#include "game/progress.h"
#include "game/string.h"
#include "game/widgets.h"

// The commands that ask the game's controllers: the game controller's requests, the video controller, the music, the player

extern "C"
{
    // The playable character's fall reset, and ammunition added to its counter
    void ResetCharacterFall(Agent* character) RETAIL(FUN_0013f790);
    void AddToCharacterCounter(CharacterCounter* counter, s32 amount) RETAIL(FUN_0015f010);
    // The empty string, and the flag command 624 sets
    extern const char g_EmptyText[] RETAIL(D_003098D8);
    extern s32 g_VarPercept629 RETAIL(D_003098E8);
}

namespace
{
// The music's sound group, which ending the context's music fades
constexpr u32 MusicGroup = 1;
constexpr u32 GroupMask = 0x7;
constexpr u32 CharacterNodeKind = 0xC;

s32 ClockUnits(f32 seconds)
{
    return static_cast<s32>(seconds * g_ClockUnitsPerSecond);
}
}


void EndContextMusicCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FadeOutMusicSlot(time, MusicGroup);
}

void FadeSoundGroupCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FadeOutMusicSlot(value, static_cast<s32>(group.raw & GroupMask));
}

void VideoControllerUpdateCommand::Execute(TimeClock* clock, BehaviourRunner*, BehaviourLevel*)
{
    StartReadCutscene(G_VideoController, clock);
}

void VideoControllerOp182Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    StopScriptMovie(G_VideoController);
}

void StartQueuedVideoCommand::Execute(TimeClock* clock, BehaviourRunner*, BehaviourLevel*)
{
    StartQueuedMovie(G_VideoController, clock);
}

void StopVideoCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_VideoController->Reset();
}

void ControllerRumbleCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    VibrateForShake(value1);
}

void ResetGameCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->RestartFromCheckpoint();
}

void UnlinkCharactersCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = runner->agentNode->owner;
    if (instance != nullptr && G_GameController != nullptr)
    {
        G_GameController->UnlinkCharacter(instance);
    }
}

// Half the seconds given
void CutsceneStartCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->StartCutscene(static_cast<s32>(value1 * 0.5f * g_ClockUnitsPerSecond));
}

void CutsceneEndCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->EndCutscene(static_cast<s32>(value1 * 0.5f * g_ClockUnitsPerSecond));
}

void ProgressWhackawormCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->progress.AddToCount(static_cast<s32>(value1));
}

void EndWhackawormCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->EndWhackaworm();
}

void ResetCharacterFallCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&runner->agentNode->owner->nodes, CharacterNodeKind));
    if (node != nullptr)
    {
        ResetCharacterFall(node->agent);
    }
}

void ClearBottomTextCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    StringAssign(&G_GameController->oleg.textLine.text, g_EmptyText);
}

void GameControllerOp612Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->EnableCharacters(0);
}

void DisablePlayerControlCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->DisablePlayerControl(0);
}

void ForceGameOverCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->ForceGameOver();
}

void ShowBottomTextCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->ShowBottomText(ClockUnits(value1));
}

void HideBottomTextCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->HideBottomText(ClockUnits(value1));
}

void EnableVarPercept629Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    g_VarPercept629 = 1;
}

void AddAmmoCommand::ExecuteOn(GameNode*)
{
    CharacterCounter* counter = g_PlayerCharacter->counter;
    if (counter != nullptr)
    {
        AddToCharacterCounter(counter, value1);
    }
}

void DamageBossCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->progress.AddHealth(static_cast<s32>(value1));
}

void ExitBossModeCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->ExitBossMode();
}

void PlayCreditsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    G_GameController->PlayCredits();
}

namespace
{
constexpr u32 TakesPacketsSlot = 15;
}

// The object the node takes its object from when there's such a node
void QueueObjectVideoCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    InstanceContext* instance = node->owner;
    GameObject* object = node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
    QueueObjectMovie(G_VideoController, object, instance, value);
}

void QueueVideoCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    s32 movie = movieId.IntWith(static_cast<ObjectNode*>(runner->agentNode)->PacketProperties());
    QueueMovie(G_VideoController, movie, clock, flags & 1);
}

// The node's voice's pitch (bit 0) and volume (bit 1; TT Lab's definitions have it as an integer)
void SetSoundParamsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    if (CallVirtual<u32>(node, node->vtable, TakesPacketsSlot) == 0)
    {
        return;
    }

    u8 voice = *(reinterpret_cast<u8*>(node) + 0x160);
    if ((set.raw & 1) != 0)
    {
        SetInstanceSoundPitch(pitch, static_cast<s32>(voice));
    }

    if ((set.raw & 2) != 0)
    {
        SetInstanceSoundVolume(*reinterpret_cast<const f32*>(&volume), static_cast<s32>(voice));
    }
}

extern "C"
{
    // A colour made of three channels, alpha 1 (the floats after it)
    void MakeColour(u32* colour, f32 red, f32 green, f32 blue) RETAIL_N32(FUN_0011bea0);
    // The bottom texts by their index (nothing sets it: the commands read a text at the index's address)
    extern const char* const* g_BottomTexts RETAIL(D_00309B14);
}


namespace
{
constexpr u16 NoModel = 0xFFFF;
constexpr u32 IconSlot = 2;

GameObject* ObjectOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? SourceObject(node->sourceNode) : node->object;
}

// The bottom text line: a text, a place, a colour, the scale 0.75 and how long it stays (seconds)
void ShowTextLine(const char* text, f32 x, f32 y, f32 red, f32 green, f32 blue, f32 seconds)
{
    constexpr f32 Scale = 0.75f;
    TextLine* line = &G_GameController->oleg.textLine;
    u32 colour;
    MakeColour(&colour, red, green, blue);
    StringAssign(&line->text, text);
    line->duration = ClockUnits(seconds);
    line->colour = colour;
    line->place.x = x;
    line->place.y = y;
    line->scale.x = Scale;
    line->scale.y = Scale;
}

// The model of an object's slot (none for slot -1)
u16 SlotModel(GameObject* object, s32 slot)
{
    u16 model;
    SetUndefinedId(&model);
    if (slot != -1)
    {
        u16 found;
        GetObjectModelId(&found, object, static_cast<u32>(slot));
        model = found;
    }

    return model;
}
}

// Lives given (taken only by the played character's own scripts)
void AddLivesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    GameController* controller = G_GameController;
    InstanceContext* owner = runner->agentNode->owner;
    InstanceContext* played = controller->progress.Instance(controller->progress.Field(GameProgress::CharacterShift));
    if (value1 > 0 || owner == played)
    {
        controller->oleg.AddLives(value1, 0);
    }
}

// The HUD's icon 2 the model of the object's slot (the object of the node it takes its object from when there's one)
void RequestOgiSlotCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    u16 model;
    GetObjectModelId(&model, ObjectOf(static_cast<ObjectNode*>(runner->agentNode)), slot & 0xFF);
    if (model != NoModel)
    {
        u16 icon = model;
        G_GameController->oleg.SetHudIcon(IconSlot, &icon);
    }
}

// The story raised to an area (-1: the second value's), never lowered
void SetGlobalProgression2Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    GameProgress* progress = &G_GameController->progress;
    s32 area = value1;
    if (area == -1)
    {
        area = value.IntWith(static_cast<ObjectNode*>(runner->agentNode)->PacketProperties());
    }

    if (static_cast<s32>(progress->bits >> GameProgress::StoryShift & GameProgress::AreaMask) < area)
    {
        progress->bits = (progress->bits & ~(GameProgress::AreaMask << GameProgress::StoryShift)) |
                         (static_cast<u32>(area) & GameProgress::AreaMask) << GameProgress::StoryShift;
    }
}

void DisplayBottomTextCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    ShowTextLine(g_BottomTexts[value1], x, y, value4, value5, value6, value7);
}

// The text of the instance's first integer property (to 10 the texts from 30, past it from 41)
void DisplayBottomTextInstanceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 LastLow = 10;
    u32 number = static_cast<u32>(static_cast<ObjectNode*>(runner->agentNode)->properties->GetInt(0));
    u32 index = number > LastLow ? number + 41 : number + 30;
    ShowTextLine(g_BottomTexts[index], x, y, value3, value4, value5, value6);
}

// The boss bar's length and health, its icon the model of the object's slot (the object of the node it takes its object from
// when there's one)
void EnableBossModeCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    GameObject* object = ObjectOf(node);
    s32 health = hitPoints.IntWith(node->PacketProperties());
    u16 icon = SlotModel(object, animSlots);
    G_GameController->EnableBossMode(value3, static_cast<u32>(health), &icon);
}

// Whack-a-worm's time (seconds) and count, its icon the model of the object's slot
void StartWhackawormCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    GameObject* object = ObjectOf(node);
    PropertyHolder* properties = node->PacketProperties();
    s32 total = hitPoints.IntWith(properties);
    s32 time = ClockUnits(value2.FloatWith(properties));
    u16 icon = SlotModel(object, animSlots);
    G_GameController->StartWhackaworm(time, static_cast<u32>(total), &icon);
}

namespace
{
// The HUD's gem and crystal widget (its mask) and the pickup effect played over it
constexpr u32 GemWidget = 0x26;
constexpr u32 GemSpriteBase = 13;
constexpr u32 CrystalSprite = 20;
constexpr f32 GemShowSeconds = Rounded(0.3);
constexpr f32 GemHoldSeconds = 1.5f;

// The word of the level play is in
u32* CurrentLevel(GameProgress* progress)
{
    return &progress->levels[g_AreaLevels[progress->Field(GameProgress::AreaShift) & GameProgress::AreaMask]];
}

void ShowGemWidget(OLEG* oleg)
{
    oleg->Show(oleg->masks[GemWidget], ClockUnits(GemShowSeconds), ClockUnits(GemHoldSeconds));
    oleg->PlayPickupEffect(GemWidget);
}
}

// The gem of the level play is in found, shown on the HUD in its colour
void AddGemCommand::ExecuteOn(GameNode*)
{
    GameController* controller = G_GameController;
    OLEG* oleg = &controller->oleg;
    MarkGem(reinterpret_cast<u8*>(CurrentLevel(&controller->progress)), static_cast<u32>(value1));
    s32 gem = value1;
    oleg->unknown310 = static_cast<u8>(gem + GemSpriteBase);
    oleg->unknown312 = static_cast<u16>((oleg->unknown312 & ~0xF) | (gem & 0xF));
    ShowGemWidget(oleg);
}

// The crystal of the level play is in found, shown on the HUD
void AddCrystalCommand::ExecuteOn(GameNode*)
{
    GameController* controller = G_GameController;
    OLEG* oleg = &controller->oleg;
    MarkCrystal(CurrentLevel(&controller->progress));
    oleg->unknown310 = CrystalSprite;
    ShowGemWidget(oleg);
}

// The played character's part gets a hit point, at most 3: a fourth makes it invincible
void CA_PickUpHealthCommand::ExecuteOn(GameNode*)
{
    constexpr u32 HitPointsShift = 6;
    constexpr u32 HitPointsMask = 0xFF;
    constexpr u32 MostHitPoints = 3;
    GameProgress* progress = &G_GameController->progress;
    InstanceContext* played = progress->Instance(progress->Field(GameProgress::CharacterShift));
    if (played == nullptr)
    {
        return;
    }

    auto* agent = static_cast<CharacterAgent*>(static_cast<AgentNode*>(GetGameNode(&played->nodes, CharacterNodeKind))->agent);
    auto* part = static_cast<BasicAgentPart*>(agent->part);
    u32 points = (part->bits >> HitPointsShift & HitPointsMask) + 1;
    if (points > MostHitPoints)
    {
        agent->StartInvincibility(1);
        points = MostHitPoints;
    }

    part->bits = (part->bits & ~(HitPointsMask << HitPointsShift)) | (points & HitPointsMask) << HitPointsShift;
}

// The wumpa fruit to add to the count; an instance with a crate's node (but no kind 14 node) loses as many of its crate's wumpa
// fruit (none left at least)
void CA_PickUpWumpaCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 WumpaShift = 2;
    constexpr u32 WumpaMask = 0xFF;
    NodeList* nodes = &node->owner->nodes;
    OLEG* oleg = &G_GameController->oleg;
    if (GetGameNode(nodes, 0xE) == nullptr)
    {
        auto* crate = static_cast<AgentNode*>(GetGameNode(nodes, 0xD));
        if (crate != nullptr)
        {
            auto* part = static_cast<BasicAgentPart*>(crate->agent->part);
            u32 wumpa = part->bits >> WumpaShift & WumpaMask;
            u32 left = wumpa < static_cast<u32>(value1) ? 0 : wumpa - value1;
            part->bits = (part->bits & ~(WumpaMask << WumpaShift)) | (left & WumpaMask) << WumpaShift;
        }
    }

    oleg->wumpaToAdd = static_cast<u8>(oleg->wumpaToAdd + value1);
}

namespace
{
// The custom pickups and projectiles the object scripts set up, in the slot their node's function 25 gives: a pickup's radius
// (and its square), two values and its flags; a projectile's speed, values and flags
struct CustomPickup
{
    f32 radius;
    f32 radiusSquared;
    f32 value2;
    f32 value3;
    u32 flags;
};

struct CustomProjectile
{
    u32 unknown00;
    f32 speed;
    u32 unknown08;
    s32 value5;
    s32 value3;
    f32 value4;
    f32 radius;
    u32 flags;
};

constexpr u32 CustomSlotSlot = 25;
constexpr u32 CustomSlots = 6;

u32 CustomSlotOf(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, CustomSlotSlot);
}
}

extern "C"
{
    extern CustomPickup* g_CustomPickups[CustomSlots] RETAIL(D_003D1E20);
    // The custom command packs' slots, the projectiles' after them
    extern CustomProjectile* g_CustomCommandsAndProjectiles[2 * CustomSlots] RETAIL(G_CodeModelCommand);
}

// The node's custom pickup: its radius and values, and the flags' low 11 bits
void CA_SetPickupCommand::ExecuteOn(GameNode* node)
{
    constexpr u32 FlagBits = 0x7FF;
    CustomPickup* pickup = g_CustomPickups[CustomSlotOf(node)];
    pickup->flags = (pickup->flags & ~FlagBits) | (hitPoints & FlagBits);
    pickup->radius = value1;
    pickup->radiusSquared = value1 * value1;
    pickup->value2 = value2;
    pickup->value3 = value3;
}

// The node's custom projectile: its speed, and what the flags give (its radius 0x20, two values 0x10, a value 0x40, bit 0x200 0x80)
void CA_SetProjectileCommand::ExecuteOn(GameNode* node)
{
    CustomProjectile* projectile = g_CustomCommandsAndProjectiles[CustomSlots + CustomSlotOf(node)];
    projectile->speed = speed;
    if ((hitPoints & 0x20) != 0)
    {
        projectile->radius = radius.FloatWith(static_cast<ObjectNode*>(node)->PacketProperties());
        projectile->flags |= 0x40;
    }

    if ((hitPoints & 0x10) != 0)
    {
        projectile->value3 = value3;
        projectile->value4 = value4;
        projectile->flags |= 0x100;
    }

    if ((hitPoints & 0x40) != 0)
    {
        projectile->value5 = value5;
        projectile->flags |= 0x80;
    }

    if ((hitPoints & 0x80) != 0)
    {
        projectile->flags |= 0x200;
    }
}
