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
// The icon files' room, and the folder's file's size
constexpr u32 IconFilesRoom = 3;
constexpr u32 FolderFileSize = 0x800;
// The tag the folder's and the banks' files end with (with their checksums)
constexpr u32 SaveTag = 1;

// The manager's OLEG screens (its screens' indexes): every widget hidden (before another is shown), the message's, the choices'
// and the save slots'
enum ManagerScreen : u32
{
    EveryWidgetScreen = 0,
    MessageScreen = 1,
    ChoicesScreen = 2,
    SlotsScreen = 3,
};

// Its labels and their texts (messages' and strings' indexes): the message, the choices' title and the save slots' title
enum ManagerLabel : u32
{
    MessageLabel = 0,
    ChoicesLabel = 1,
    SlotsLabel = 2,
};

// The screens appear and disappear over half a second
s32 HalfSecond()
{
    return static_cast<s32>(g_ClockUnitsPerSecond * 0.5f);
}

void SwitchScreen(SaveManager* manager, u32 screen)
{
    manager->oleg->Hide(manager->oleg->screens[manager->screens[EveryWidgetScreen]], HalfSecond(), 0);
    manager->oleg->Show(manager->oleg->screens[manager->screens[screen]], HalfSecond(), 0);
}

// The choices page's mode of a screen of choices
s32 ChoicesMode(u32 screen)
{
    if (screen >= SaveScreenNoCard && screen <= SaveScreenCancelSave)
    {
        return static_cast<s32>(screen - SaveScreenNoCard) + ChoicesNoCard;
    }

    if (screen >= SaveScreenFormatFailed && screen <= SaveScreenLoadFailed)
    {
        return static_cast<s32>(screen - SaveScreenFormatFailed) + ChoicesFormatFailed;
    }

    return ChoicesHidden;
}
}

extern "C"
{
    void ConstructSaveCode(SaveCode* code, const char* name, OLEG* oleg, void* device)
    {
        SaveCode::Construct(code, name, static_cast<SaveDevice*>(device));
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

        manager->screens[EveryWidgetScreen] = -1;
    }
}

u32 SaveManager::Ask(u32 operation, u32 file)
{
    bool message = true;
    bits.screen = SaveScreenMessage;
    switch (bits.operation)
    {
    case SaveOperationNone:
    case SaveOperationCheckInserted:
        message = false;
        break;
    // The others but the autosave bring the message's screen up
    case SaveOperationCheckRoom:
    case SaveOperationNewGameSave:
    case SaveOperationPauseSave:
    case SaveOperationLoad:
        SwitchScreen(this, MessageScreen);
        break;
    default:
        break;
    }

    if (message)
    {
        MessageText(g_OperationMessages[operation], &strings[MessageLabel]);
        StringAssign(&messages[MessageLabel]->text, strings[MessageLabel].string);
    }

    return device->Ask(operation, file);
}

u32 SaveManager::ShowChoices(u32 screen)
{
    bits.screen = screen;
    if (choicesPage != nullptr)
    {
        s32 mode = ChoicesMode(screen);
        MessageText(g_ScreenMessages[screen], &strings[ChoicesLabel]);
        StringAssign(&messages[ChoicesLabel]->text, strings[ChoicesLabel].string);
        static_cast<SaveChoicesPage*>(choicesPage)->ShowItems(mode, bits.operation == SaveOperationNewGameSave);
    }

    SwitchScreen(this, ChoicesScreen);
    return screen;
}

u32 SaveManager::ShowSlots(u32 screen)
{
    bits.screen = screen;
    if (slotsPage != nullptr)
    {
        s32 mode = SlotsPageHidden;
        if (screen == SaveScreenSaveSlots)
        {
            mode = SlotsPageSave;
        }
        else if (screen == SaveScreenLoadSlots)
        {
            mode = SlotsPageLoad;
        }

        MessageText(g_ScreenMessages[screen], &strings[SlotsLabel]);
        StringAssign(&messages[SlotsLabel]->text, strings[SlotsLabel].string);
        static_cast<SaveCodePage*>(slotsPage)->ShowSlotItems(mode, bits.operation == SaveOperationNewGameSave);
    }

    SwitchScreen(this, SlotsScreen);
    return screen;
}

void SaveManager::DestroyScreens(u32 destroyFlags)
{
    StringDestroy(&strings[SlotsLabel]);
    StringDestroy(&strings[ChoicesLabel]);
    StringDestroy(&strings[MessageLabel]);
    vtable = g_SaveCodeVTable;
    StringDestroy(&name);
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void SaveManager::Draw(Renderer*)
{
}

SaveManager* SaveManager::Construct(SaveManager* manager, GameController* controller)
{
    ConstructSaveCode(manager, g_SaveCodeName, &controller->oleg, &manager->memoryCard);
    manager->vtable = g_SaveManagerVTable;
    ConstructIconSys(&manager->iconSys, g_SaveTitle, g_IconName);
    ConstructIconFiles(&manager->iconFiles, IconFilesRoom, &manager->iconSys);
    ConstructFolderFile(&manager->folder, g_ProductCode, Banks, SaveTag, FolderFileSize, g_SaveBuffer);
    SaveDevice::Construct(&manager->memoryCard, Banks, &manager->iconFiles, &manager->folder, g_SaveName);
    AddIconFile(&manager->iconFiles, ConstructCopiedFile(MemoryAllocate(sizeof(CopiedFile)), g_IconPath, g_IconName));
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
        ConstructSaveFile(bank, name.string, SaveTag, BankSize, g_SaveBuffer);
        bank->vtable = g_BankFileVTable;
        ConstructBankSummary(&bank->summary);
        bank->controller = controller;
        manager->banks[index] = bank;
        manager->folder.summaries[index] = &bank->summary.folder;
        manager->memoryCard.files[index] = bank;
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
            CallVirtual<void>(bank, bank->vtable, SaveFile::DestroySlot, u32{DestroyAndFree});
        }
    }

    MemoryDeallocate2_(banks);
    memoryCard.Destroy(DestroyOnly);
    DestroyFolderFile(&folder, DestroyOnly);
    DestroyIconFiles(&iconFiles, DestroyOnly);
    // icon.sys's destructor inline
    StringDestroy(&iconSys.title);
    DestroySaveFile(&iconSys, DestroyOnly);
    StringDestroy(&strings[SlotsLabel]);
    StringDestroy(&strings[ChoicesLabel]);
    StringDestroy(&strings[MessageLabel]);
    DestroySaveCode(this, destroyFlags);
}

void BankFile::GatherSettings()
{
    SaveController* settings = &controller->saveController;
    auto* pads = static_cast<GamePadController*>(G_GamePadController);
    GameRendererController* renderer = G_GameRendererController;
    settings->options.vibration = pads->flags.vibration;
    settings->effectsVolume = GroupVolumeLevel(EffectsGroup);
    settings->musicVolume = GroupVolumeLevel(MusicGroup);
    settings->options.musicStereo = g_MusicStereo;
    settings->options.widescreen = g_WidescreenTv;
    settings->screenOffset.x = renderer->screenOffset.x;
    settings->screenOffset.y = renderer->screenOffset.y;
    MakeBankSummary(&summary, settings);
}

void BankFile::Destroy(u32 destroyFlags)
{
    summary.folder.Destroy(DestroyOnly);
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
    FolderSummary* folder = &summary->folder;
    FolderSummary::Construct(folder);
    folder->vtable = g_BankSummaryVTable;
    RetailLibc::MemorySet(&summary->progress, 0, sizeof(summary->progress));
    summary->time = 0;
}

void MakeBankSummary(void* memory, SaveController* settings)
{
    auto* summary = static_cast<SaveSummary*>(memory);
    SaveControllerSummary bits = settings->summary;
    SavedProgress progress = summary->progress;
    progress.area = settings->options.area;
    progress.character = bits.character;
    progress.lives = bits.lives;
    progress.crystals = bits.crystals;
    progress.done = bits.done;
    summary->progress = progress;
    summary->time = settings->timePlayed;
    summary->folder.Refresh();
}

void SaveSummary::Destroy(u32 destroyFlags)
{
    folder.Destroy(destroyFlags);
}

void SaveSummary::Read(Stream* stream)
{
    stream->Read(&progress, sizeof(progress), 1);
    stream->ReadU32(reinterpret_cast<u32*>(&time));
    folder.Read(stream);
}

void SaveSummary::Write(Stream* stream)
{
    stream->Write(&progress, sizeof(progress));
    stream->WriteU32(time);
    folder.Write(stream);
}
