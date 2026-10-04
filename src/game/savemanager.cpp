#include "game/savemanager.h"

#include "game/clock.h"
#include "game/gamecontroller.h"
#include "game/memory.h"
#include "game/olegpages.h"
#include "game/pads.h"
#include "game/progress.h"
#include "game/renderer.h"
#include "game/savedevice.h"
#include "game/sound.h"
#include "game/stream.h"
#include "game/string.h"
#include "game/widgets.h"
#include "retail/libc.h"

// The save manager, the save code on OLEG's screens it derives from and its bank files
extern "C"
{
    // The card slots (SaveDevice::Construct and Destroy): their files besides the icons, the icon files, the main file and the
    // save's name
    void ConstructCardSlots(void* slots, u32 fileCount, void* icons, void* mainFile, const char* name) RETAIL(FUN_002a2f78);
    void DestroyCardSlots(void* slots, u32 destroyFlags) RETAIL(FUN_002a8e18);

    extern const GccVTableEntry g_SaveManagerVTable[] RETAIL(D_002F5120);
    extern const GccVTableEntry g_SaveCodeVTable[] RETAIL(D_00306570);
    extern const GccVTableEntry g_SaveCodeScreensVTable[] RETAIL(D_00306470);
    extern const GccVTableEntry g_BankFileVTable[] RETAIL(D_002F5200);
    extern const GccVTableEntry g_BankSummaryVTable[] RETAIL(BanksHeader_methods);
    // "crash twinsanity", "Crash~Twinsanity", "Crash.ico", "BESLES-52568", "StartUp\Crash.ico", the save's name "", "Bank" and
    // ".bin"; the buffer the save's files are read into
    extern const char g_SaveCodeName[] RETAIL(D_002F4800);
    extern const char g_SaveTitle[] RETAIL(D_002F4818);
    extern const char g_IconName[] RETAIL(D_002F4830);
    extern const char g_ProductCode[] RETAIL(D_002F4840);
    extern const char g_IconPath[] RETAIL(D_002F4850);
    extern const char g_SaveName[] RETAIL(D_00309958);
    extern const char g_BankPrefix[] RETAIL(D_00309988);
    extern const char g_BankExtension[] RETAIL(D_00309990);
    extern u8 g_SaveBuffer[] RETAIL(D_0030BF28);
}

namespace
{
constexpr u32 Banks = 4;
constexpr u32 BankSize = 0xF400;
// The icon files' room, the folder's files and their size
constexpr u32 IconFilesRoom = 3;
constexpr u32 FolderFileSize = 0x800;
constexpr u32 CopiedFileSize = 0x2C;
// The bank files' destructor (their vtable's)
constexpr u32 BankDestroySlot = 5;
// The volume groups of the effects and the music
constexpr s32 EffectsGroup = 0;
constexpr s32 MusicGroup = 2;
// The pad controller's vibration bit made the options'
constexpr u32 VibrationShift = 6;
constexpr u32 WidescreenShift = 16;

// The folder's summaries of its files and the card slots' files
struct SaveFolderFiles
{
    u8 unknown00[0x30];
    void** summaries;
};

struct CardSlotFiles
{
    u8 unknown00[0x18];
    BankFile** files;
};

// The save code's strings the manager's destructor lets go of: the icon.sys file's and its own three
constexpr u32 IconSysString = 0x2C;
// The area play is in, the save controller's options' low bits (the progress's 16-20)
constexpr u32 OptionArea = 0x1F;

// A bank's summary is the folder's kind with the progress and the time played after it
FolderSummary* SummaryOf(void* summary)
{
    return static_cast<FolderSummary*>(summary);
}

// The save code's screens (OLEG's widgets by their slots): the one they replace, the message's, the choices' and the save slots'
constexpr u32 AwayWidget = 0;
constexpr u32 MessageWidget = 1;
constexpr u32 ChoicesWidget = 2;
constexpr u32 SlotsWidget = 3;
// The save code's operations (bits 0-3 of its bits): none and the check of the card (no message), the save its screens offer
// to skip
constexpr u32 OperationNone = 0;
constexpr u32 OperationCheck = 2;
constexpr u32 OperationSave = 3;
// The screens of save slots: to save to and to load from
constexpr u32 SaveSlotsScreen = 11;
constexpr u32 LoadSlotsScreen = 12;

// The screens appear and disappear over half a second
s32 HalfSecond()
{
    return static_cast<s32>(g_ClockUnitsPerSecond * 0.5f);
}

void SwitchScreen(SaveManager* manager, u32 widget)
{
    manager->oleg->Hide(manager->oleg->masks[manager->screens[AwayWidget]], HalfSecond(), 0);
    manager->oleg->Show(manager->oleg->masks[manager->screens[widget]], HalfSecond(), 0);
}

// The choices page's mode of a screen of choices (-1: none)
s32 ChoicesMode(u32 screen)
{
    if (screen >= 1 && screen <= 10)
    {
        return static_cast<s32>(screen) - 1;
    }

    if (screen >= 13 && screen <= 15)
    {
        return static_cast<s32>(screen) - 3;
    }

    return -1;
}
}

extern "C"
{
    void ConstructSaveCode(SaveCode* code, const char* name, OLEG* oleg, void* slots)
    {
        SaveCode::Construct(code, name, static_cast<SaveDevice*>(slots));
        auto* manager = static_cast<SaveManager*>(code);
        manager->oleg = oleg;
        manager->vtable = g_SaveCodeScreensVTable;
        for (String& string : manager->strings)
        {
            string = {nullptr, 0, 0};
        }

        for (StringLabel*& message : manager->messages)
        {
            message = nullptr;
        }

        manager->choicesPage = nullptr;
        manager->slotsPage = nullptr;
        for (s32& screen : manager->screens)
        {
            screen = 0;
        }

        manager->screens[AwayWidget] = -1;
    }
}

u32 SaveManager::Ask(u32 operation, u32 file)
{
    bool message = true;
    SetScreen(0);
    switch (bits & OperationMask)
    {
    case OperationNone:
    case OperationCheck:
        message = false;
        break;
    // The others but the save to a slot bring the message's screen up
    case 1:
    case 3:
    case 4:
    case 5:
        SwitchScreen(this, MessageWidget);
        break;
    default:
        break;
    }

    if (message)
    {
        MessageText(g_OperationMessages[operation], &strings[0]);
        StringAssign(&messages[0]->text, strings[0].string);
    }

    return device->Ask(operation, file);
}

u32 SaveManager::ShowChoices(u32 screen)
{
    SetScreen(screen);
    if (choicesPage != nullptr)
    {
        s32 mode = ChoicesMode(screen);
        MessageText(g_ScreenMessages[screen], &strings[1]);
        StringAssign(&messages[1]->text, strings[1].string);
        static_cast<SaveChoicesPage*>(choicesPage)->ShowItems(mode, (bits & OperationMask) == OperationSave);
    }

    SwitchScreen(this, ChoicesWidget);
    return screen;
}

u32 SaveManager::ShowSlots(u32 screen)
{
    SetScreen(screen);
    if (slotsPage != nullptr)
    {
        s32 mode = -1;
        if (screen == SaveSlotsScreen)
        {
            mode = 1;
        }
        else if (screen == LoadSlotsScreen)
        {
            mode = 0;
        }

        MessageText(g_ScreenMessages[screen], &strings[2]);
        StringAssign(&messages[2]->text, strings[2].string);
        static_cast<SaveCodePage*>(slotsPage)->ShowSlotItems(mode, (bits & OperationMask) == OperationSave);
    }

    SwitchScreen(this, SlotsWidget);
    return screen;
}

void SaveManager::DestroyScreens(u32 destroyFlags)
{
    StringDestroy(&strings[2]);
    StringDestroy(&strings[1]);
    StringDestroy(&strings[0]);
    vtable = g_SaveCodeVTable;
    StringDestroy(&name);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SaveManager::Draw(Renderer*)
{
}

SaveManager* SaveManager::Construct(SaveManager* manager, GameController* controller)
{
    ConstructSaveCode(manager, g_SaveCodeName, &controller->oleg, manager->slots);
    manager->vtable = g_SaveManagerVTable;
    ConstructIconSys(manager->icon, g_SaveTitle, g_IconName);
    ConstructIconFiles(manager->files, IconFilesRoom, manager->icon);
    ConstructFolderFile(manager->folder, g_ProductCode, Banks, 1, FolderFileSize, g_SaveBuffer);
    ConstructCardSlots(manager->slots, Banks, manager->files, manager->folder, g_SaveName);
    AddIconFile(manager->files, ConstructCopiedFile(MemoryAllocate(CopiedFileSize), g_IconPath, g_IconName));
    manager->banks = static_cast<BankFile**>(MemoryAllocate2(Banks * sizeof(BankFile*)));
    for (u32 index = 0; index < Banks; index++)
    {
        String name;
        StringConstruct(&name, g_BankPrefix);
        String number;
        StringConstructNumber(&number, index);
        StringAppend(&name, number.string);
        StringDestroy(&number);
        StringAppend(&name, g_BankExtension);
        auto* bank = static_cast<BankFile*>(MemoryAllocate(sizeof(BankFile)));
        ConstructSaveFile(bank, name.string, 1, BankSize, g_SaveBuffer);
        bank->vtable = g_BankFileVTable;
        ConstructBankSummary(bank->summary);
        bank->controller = controller;
        manager->banks[index] = bank;
        reinterpret_cast<SaveFolderFiles*>(manager->folder)->summaries[index] = bank->summary;
        reinterpret_cast<CardSlotFiles*>(manager->slots)->files[index] = bank;
        StringDestroy(&name);
    }

    return manager;
}

void SaveManager::Destroy(u32 destroyFlags)
{
    vtable = g_SaveManagerVTable;
    for (u32 index = 0; index < Banks; index++)
    {
        BankFile* bank = banks[index];
        if (bank != nullptr)
        {
            CallVirtual<void>(bank, bank->vtable, BankDestroySlot, u32{DestroyAndFree});
        }
    }

    MemoryDeallocate2_(banks);
    DestroyCardSlots(slots, DestroyOnly);
    DestroyFolderFile(folder, DestroyOnly);
    DestroyIconFiles(files, DestroyOnly);
    StringDestroy(reinterpret_cast<String*>(icon + IconSysString));
    DestroySaveFile(icon, DestroyOnly);
    StringDestroy(&strings[2]);
    StringDestroy(&strings[1]);
    StringDestroy(&strings[0]);
    DestroySaveCode(this, destroyFlags);
}

void BankFile::GatherSettings()
{
    SaveController* settings = &controller->saveController;
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    GameRendererController* renderer = G_GameRendererController;
    settings->options = (settings->options & ~SaveController::OptionVibration) |
                        (pads->flags << VibrationShift & SaveController::OptionVibration);
    settings->effectsVolume = GroupVolumeLevel(EffectsGroup);
    settings->musicVolume = GroupVolumeLevel(MusicGroup);
    settings->options = (settings->options & ~(SaveController::OptionMusicStereoMask << SaveController::OptionMusicStereoShift)) |
                        (g_MusicStereo & SaveController::OptionMusicStereoMask) << SaveController::OptionMusicStereoShift;
    settings->options = (settings->options & ~SaveController::OptionWidescreen) | (g_WidescreenTv & 1) << WidescreenShift;
    settings->screenOffset.x = renderer->screenOffset.x;
    settings->screenOffset.y = renderer->screenOffset.y;
    MakeBankSummary(summary, settings);
}

void BankFile::Destroy(u32 destroyFlags)
{
    SummaryOf(summary)->Destroy(DestroyOnly);
    DestroySaveFile(this, destroyFlags);
}

void BankFile::ReadData(Stream* stream)
{
    controller->saveController.Read(stream);
}

void BankFile::WriteData(Stream* stream)
{
    controller->saveController.Write(stream);
}

void SaveManager::Nothing10()
{
}

void ConstructBankSummary(void* memory)
{
    auto* summary = static_cast<SaveSummary*>(memory);
    FolderSummary* folder = SummaryOf(summary);
    FolderSummary::Construct(folder);
    folder->vtable = g_BankSummaryVTable;
    RetailLibc::MemorySet(&summary->progress, 0, sizeof(summary->progress));
    summary->time = 0;
}

void MakeBankSummary(void* memory, SaveController* settings)
{
    auto* summary = static_cast<SaveSummary*>(memory);
    u32 bits = settings->summary;
    u32 progress = summary->progress;
    progress = (progress & ~SaveSummary::AreaMask) | (settings->options & OptionArea);
    progress = (progress & ~(SaveSummary::CharacterMask << SaveSummary::CharacterShift)) |
               (bits >> SaveController::SummaryCharacterShift & SaveSummary::CharacterMask) << SaveSummary::CharacterShift;
    progress = (progress & ~(SaveSummary::LivesMask << SaveSummary::LivesShift)) |
               (bits & SaveController::SummaryLivesMask) << SaveSummary::LivesShift;
    progress = (progress & ~(SaveSummary::CrystalsMask << SaveSummary::CrystalsShift)) |
               (bits >> SaveController::SummaryCrystalsShift & SaveController::SummaryCrystalsMask) << SaveSummary::CrystalsShift;
    progress = (progress & ~(SaveSummary::DoneMask << SaveSummary::DoneShift)) |
               (bits >> SaveController::SummaryDoneShift & SaveController::SummaryDoneMask) << SaveSummary::DoneShift;
    summary->progress = progress;
    summary->time = settings->timePlayed;
    SummaryOf(summary)->Refresh();
}

void SaveSummary::Destroy(u32 destroyFlags)
{
    SummaryOf(this)->Destroy(destroyFlags);
}

void SaveSummary::Read(Stream* stream)
{
    stream->Read(&progress, sizeof(progress), 1);
    stream->ReadU32(reinterpret_cast<u32*>(&time));
    SummaryOf(this)->Read(stream);
}

void SaveSummary::Write(Stream* stream)
{
    stream->Write(&progress, sizeof(progress));
    stream->WriteU32(time);
    SummaryOf(this)->Write(stream);
}
