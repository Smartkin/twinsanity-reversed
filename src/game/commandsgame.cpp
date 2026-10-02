#include "game/commands.h"

#include "game/camerarig.h"
#include "game/clock.h"
#include "game/controllers.h"
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
    // A sound group faded out over a time (the float first; still asm)
    void FadeSoundGroup(f32 time, u32 group) RETAIL_N32(FUN_001e5a20);
    // The video controller's movie started at the clock's time, the queued one started, and the one playing stopped (still asm)
    void StartScriptMovie(VideoController* controller, TimeClock* clock) RETAIL(FUN_0029aa10);
    void StartQueuedMovie(VideoController* controller, TimeClock* clock) RETAIL(FUN_0029e8e0);
    void StopScriptMovie(VideoController* controller) RETAIL(FUN_0029ea38);
    // The playable character's fall reset, and ammunition added to its counter (still asm)
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

EABI_IMPORT(FUN_001e5a20, FadeSoundGroup);

void EndContextMusicCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FadeSoundGroup(time, MusicGroup);
}

void FadeSoundGroupCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FadeSoundGroup(value, group.raw & GroupMask);
}

void VideoControllerUpdateCommand::Execute(TimeClock* clock, BehaviourRunner*, BehaviourLevel*)
{
    StartScriptMovie(G_VideoController, clock);
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

extern "C"
{
    // A movie queued for an object's instance, and a movie queued (from the clock's time, played at once with the flag; still
    // asm)
    void QueueObjectMovie(VideoController* controller, GameObject* object, InstanceContext* instance, s32 value)
        RETAIL(FUN_0029e5f8);
    void QueueMovie(VideoController* controller, s32 movie, TimeClock* clock, u32 now) RETAIL(FUN_0029e790);
    // The object of a node's instance (the node it takes its object from)
    GameObject* ObjectOfNode(GameNode* node) RETAIL(GetGameObjectAddress_FromInstance_);
    // A voice's pitch and volume set (the float first; still asm)
    void SetChannelPitch(f32 pitch, u32 voice) RETAIL_N32(FUN_001e5950);
    void SetChannelVolume(f32 volume, u32 voice) RETAIL_N32(FUN_001e58f8);
}

EABI_IMPORT(FUN_001e5950, SetChannelPitch);
EABI_IMPORT(FUN_001e58f8, SetChannelVolume);

namespace
{
constexpr u32 TakesPacketsSlot = 15;
}

// The object the node takes its object from when there's such a node
void QueueObjectVideoCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    auto* node = static_cast<ObjectNode*>(runner->agentNode);
    InstanceContext* instance = node->owner;
    GameObject* object = node->sourceNode != nullptr ? ObjectOfNode(node->sourceNode) : node->object;
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
        SetChannelPitch(pitch, voice);
    }

    if ((set.raw & 2) != 0)
    {
        SetChannelVolume(*reinterpret_cast<const f32*>(&volume), voice);
    }
}

extern "C"
{
    // A colour made of three channels, alpha 1 (the floats after it; still asm)
    void MakeColour(u32* colour, f32 red, f32 green, f32 blue) RETAIL_N32(FUN_0011bea0);
    // The bottom texts by their index (nothing sets it: the commands read a text at the index's address)
    extern const char* const* g_BottomTexts RETAIL(D_00309B14);
}

EABI_IMPORT(FUN_0011bea0, MakeColour);

namespace
{
constexpr u16 NoModel = 0xFFFF;
constexpr u32 IconSlot = 2;

GameObject* ObjectOf(ObjectNode* node)
{
    return node->sourceNode != nullptr ? ObjectOfNode(node->sourceNode) : node->object;
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
