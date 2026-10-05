#include "platform/saves.h"

#include "memorycard.h"
#include "retail/libc.h"

// The retail game's memory card manager, on Platform::MemoryCard (libmc's interface): an operation is a function of steps, each
// starting a libmc call whose result an Update after finds through the non-blocking sync (some wait for theirs). Saves are
// directories "/<region><product><save>" of at most 20 entries. The retail manager had four more operations nothing starts
// (whether a save is there, how many files it has, a listing by pattern and a deletion), left out
namespace
{
using Platform::Saves::FileEntry;
using Platform::Saves::Operation;
using Platform::Saves::Result;
using Platform::Saves::StorageInfo;
namespace MemoryCard = Platform::MemoryCard;

// What the operation was given, made anew from the template when one ends (port and slot -1, the rest 0). The retail
// manager's other operations used the fields nothing uses
struct Context
{
    s32 port;
    s32 slot;
    u32 unused08;
    FileEntry* entry;
    u32 unused10;
    u32 unused14;
    s32* kilobytes;
    u32 unused1C;
    u32 unused20;
    void* data;
    s32* size;
    u32 unused2C;
    u32 unused30;
    u32 unused34;
    s32 dataSize;
    char save[32];
    char file[32];
    char unused7C[32];
};
CHECK_SIZE(Context, 0x9C);

// What libmc's non-blocking sync found (MemoryCard::SyncStatus): when a call finished, its function and result
struct Sync
{
    s32 function;
    s32 result;
    s32 status;
};

constexpr Context ContextTemplate = {-1, -1};
// A save's directory holds at most 20 entries, "." and ".." among them
constexpr s32 MaxEntries = 20;
constexpr s32 DotEntries = 2;
// The longest save and file names it takes are a character shorter
constexpr u32 SaveNameLimit = 10;
constexpr u32 FileNameLimit = 0x21;
// The memory card's clusters are a kilobyte: a directory takes two of them and one more for every two entries
constexpr u32 ClusterSize = 0x400;
constexpr u32 ClusterShift = 10;
constexpr s32 DirectoryClusters = 2;

// The writing's steps, each waiting for its call to finish: the save's directory looked for, its entries listed, the file
// opened, written and closed
enum WriteSteps : s32
{
    WriteFindingSave = 1,
    WriteListing = 2,
    WriteOpening = 3,
    WriteWriting = 4,
    WriteClosing = 5,
};

constexpr s32 NoNextOperation = -1;

bool g_Initialised;
Operation g_Operation = Operation::None;
// The operation the last one to end was (nothing reads it)
Operation g_LastOperation = Operation::None;
// The operation the check goes on to
s32 g_Next = NoNextOperation;
s32 g_Step;
s32 g_LastResult;
s32 g_File = -1;
char g_Region[8];
char g_Product[16];
StorageInfo g_Info = {-1, -1, -1};
Result g_Results[12];
Context g_Context = ContextTemplate;
// The listings GetDirectory writes, aligned as its tables have to be (a misaligned one got its entries 8 bytes early)
alignas(64) FileEntry g_Directory;
alignas(64) FileEntry g_Entries[MaxEntries];

void ForgetResults()
{
    for (Result& result : g_Results)
    {
        result = {-1, -1, 0};
    }
}

// The operation ends, how it went kept
void Finish(s32 succeeded, s32 step, s32 error)
{
    g_LastOperation = g_Operation;
    g_Results[static_cast<s32>(g_Operation)] = {succeeded, step, error};
    g_Next = NoNextOperation;
    g_Operation = Operation::None;
    g_Step = 0;
    g_Context = ContextTemplate;
}

// Starts the check of the port's and slot's memory card, which goes on to the operation: only when none runs and the card was
// there at the last check
bool Begin(s32 operation, s32 port, s32 slot)
{
    ForgetResults();
    if (g_Operation == Operation::None && g_Info.type == Platform::Saves::StorageMemoryCard)
    {
        g_Context.port = port;
        g_Context.slot = slot;
        g_Operation = Operation::Checking;
        g_Next = operation;
        g_Step = 0;
        return true;
    }

    g_LastOperation = g_Operation;
    g_Next = NoNextOperation;
    g_Operation = Operation::None;
    g_Step = 0;
    g_Context = ContextTemplate;
    return false;
}

// The save's directory, or the pattern of its entries
void SavePath(char* path, bool entries)
{
    RetailLibc::Format(path, entries ? "/%s%s%s/*" : "/%s%s%s", g_Region, g_Product, g_Context.save);
}

// A call that couldn't start, or a result that's an error
void Fail(s32 error)
{
    Finish(0, g_Step, error);
    g_LastResult = error;
}

void FormatStep(const Sync& sync)
{
    if (g_Step == 0)
    {
        s32 started = MemoryCard::Format(g_Context.port, g_Context.slot);
        if (started < 0)
        {
            Fail(started);
            return;
        }

        g_Step++;
    }
    else if (g_Step == 1 && sync.status == MemoryCard::SyncFinished)
    {
        if (sync.result < 0)
        {
            Finish(0, sync.status, sync.result);
            g_LastResult = sync.result;
        }
        else
        {
            Finish(sync.status, sync.status, 0);
        }
    }
}

// What mkdir returns isn't looked at, the sync's result is
void CreateSaveStep(const Sync& sync)
{
    if (g_Step == 0)
    {
        if (g_Info.freeKilobytes < DirectoryClusters)
        {
            Finish(0, 0, Platform::Saves::ErrorNotEnoughSpace);
            return;
        }

        char path[64];
        SavePath(path, false);
        MemoryCard::MakeDirectory(g_Context.port, g_Context.slot, path);
        g_Step++;
    }
    else if (g_Step == 1 && sync.status == MemoryCard::SyncFinished)
    {
        if (sync.result < 0)
        {
            Finish(0, sync.status, sync.result);
            g_LastResult = sync.result;
        }
        else
        {
            Finish(sync.status, sync.status, 0);
        }
    }
}

// Lists the save's entries. Returns whether the listing started
bool ListSave()
{
    char path[64];
    SavePath(path, true);
    s32 started = MemoryCard::GetDirectory(g_Context.port, g_Context.slot, path, 0, MaxEntries,
                                           reinterpret_cast<MemoryCard::DirectoryEntry*>(g_Entries));
    if (started < 0)
    {
        Fail(started);
        return false;
    }

    return true;
}

// The save's files in whole kilobytes, and half a kilobyte an entry ("." and ".." among them) and two for the directory. No such
// save measures -1, and the operation succeeds
void MeasureSaveStep(const Sync& sync)
{
    if (g_Step == 0)
    {
        if (ListSave())
        {
            g_Step++;
        }

        return;
    }

    if (g_Step != 1 || sync.status != MemoryCard::SyncFinished)
    {
        return;
    }

    s32 entries = sync.result;
    if (entries < 0)
    {
        *g_Context.kilobytes = -1;
        g_LastResult = entries;
    }
    else if (entries - DotEntries >= MaxEntries - 1)
    {
        Finish(0, sync.status, g_LastResult);
        return;
    }
    else
    {
        s32 kilobytes = 0;
        for (s32 i = 0; i < entries; i++)
        {
            kilobytes += (g_Entries[i].size + ClusterSize - 1) >> ClusterShift;
        }

        *g_Context.kilobytes = kilobytes + (entries - 1) / 2 + DirectoryClusters;
    }

    Finish(1, g_Step, 0);
}

// The file's size (-1 when it isn't there) and its entry
void FindFileStep(const Sync& sync)
{
    if (g_Step == 0)
    {
        if (g_Context.size != nullptr)
        {
            *g_Context.size = 0;
        }

        if (ListSave())
        {
            g_Step++;
        }

        return;
    }

    if (g_Step != 1 || sync.status != MemoryCard::SyncFinished)
    {
        return;
    }

    s32 entries = sync.result;
    if (entries < 0)
    {
        Finish(0, sync.status, entries);
        g_LastResult = entries;
        return;
    }

    if (entries - DotEntries >= MaxEntries - 1)
    {
        Finish(0, sync.status, g_LastResult);
        return;
    }

    bool found = false;
    for (s32 i = 0; i < entries && !found; i++)
    {
        found = RetailLibc::StringCompare(g_Context.file, g_Entries[i].name) == 0;
        if (found)
        {
            if (g_Context.size != nullptr)
            {
                *g_Context.size = g_Entries[i].size;
            }

            if (g_Context.entry != nullptr)
            {
                *g_Context.entry = g_Entries[i];
            }
        }
    }

    if (!found && g_Context.size != nullptr)
    {
        *g_Context.size = -1;
    }

    Finish(1, g_Step, 0);
}

// Into the save's directory, the file opened and read, waiting for each call but the read; the second step waits for the read.
// A failed open or read gives the descriptor as the error
void ReadStep(const Sync&)
{
    if (g_Step == 0)
    {
        char path[64];
        SavePath(path, false);
        s32 started = MemoryCard::ChangeDirectory(g_Context.port, g_Context.slot, path, nullptr);
        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_LastResult);
        if (started < 0 || g_LastResult != 0)
        {
            Finish(0, g_Step, g_LastResult);
            return;
        }

        started = MemoryCard::Open(g_Context.port, g_Context.slot, g_Context.file, Platform::Files::OpenRead);
        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_File);
        if (started < 0 || g_File < 0 || MemoryCard::Read(g_File, g_Context.data, g_Context.dataSize) < 0)
        {
            Fail(g_File);
            return;
        }

        g_Step++;
    }
    else if (g_Step == 1)
    {
        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_LastResult);
        if (g_LastResult < 0)
        {
            Finish(0, g_Step, g_LastResult);
            return;
        }

        MemoryCard::Close(g_File);
        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_LastResult);
        Finish(g_Step, g_Step, 0);
        g_File = -1;
    }
}

// The save's directory has to be there (with at most 20 entries), the file is made when it isn't: a step for each call
void WriteStep(const Sync& sync)
{
    if (g_Step == 0)
    {
        if (RetailLibc::StringLength(g_Context.save) >= SaveNameLimit ||
            RetailLibc::StringLength(g_Context.file) >= FileNameLimit)
        {
            Finish(0, g_Step, g_LastResult);
            return;
        }

        char path[64];
        SavePath(path, false);
        s32 started = MemoryCard::GetDirectory(g_Context.port, g_Context.slot, path, 0, 1,
                                               reinterpret_cast<MemoryCard::DirectoryEntry*>(&g_Directory));
        if (started < 0)
        {
            Fail(started);
            return;
        }

        g_Step = WriteFindingSave;
        return;
    }

    if (g_Step > WriteClosing || sync.status != MemoryCard::SyncFinished)
    {
        return;
    }

    if (sync.result < 0)
    {
        Fail(sync.result);
        return;
    }

    switch (g_Step)
    {
    case WriteFindingSave:
        if (sync.result == 0)
        {
            Fail(Platform::Saves::ErrorNoSave);
        }
        else if (ListSave())
        {
            g_Step = WriteListing;
        }

        break;
    case WriteListing:
    {
        if (sync.result - DotEntries >= MaxEntries - 1)
        {
            Finish(0, g_Step, g_LastResult);
            break;
        }

        char path[64];
        SavePath(path, false);
        s32 started = MemoryCard::ChangeDirectory(g_Context.port, g_Context.slot, path, nullptr);
        if (started < 0)
        {
            Fail(started);
            break;
        }

        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_LastResult);
        if (g_LastResult < 0)
        {
            Finish(0, g_Step, g_LastResult);
            break;
        }

        started = MemoryCard::Open(g_Context.port, g_Context.slot, g_Context.file,
                                   Platform::Files::OpenCreate | Platform::Files::OpenWrite);
        if (started < 0)
        {
            Fail(started);
            break;
        }

        g_Step = WriteOpening;
        break;
    }
    case WriteOpening:
    {
        g_File = sync.result;
        s32 started = MemoryCard::Write(g_File, g_Context.data, g_Context.dataSize);
        if (started < 0)
        {
            Fail(started);
            break;
        }

        g_Step = WriteWriting;
        break;
    }
    case WriteWriting:
        MemoryCard::Close(g_File);
        g_Step = WriteClosing;
        break;
    case WriteClosing:
        Finish(sync.status, g_Step, 0);
        g_File = -1;
        break;
    default:
        break;
    }
}

bool Start(Operation operation, s32 port, s32 slot)
{
    return Begin(static_cast<s32>(operation), port, slot);
}
}

bool Platform::Saves::Initialise(const char* region, const char* product)
{
    if (g_Initialised)
    {
        return true;
    }

    RetailLibc::StringCopy(g_Region, region);
    RetailLibc::StringCopy(g_Product, product);
    g_LastResult = MemoryCard::Initialise();
    bool ready = g_LastResult == 0;
    if (ready)
    {
        MemoryCard::Sync(MemoryCard::SyncWait, nullptr, &g_LastResult);
        g_Initialised = true;
    }

    ForgetResults();
    return ready;
}

Platform::Saves::Operation Platform::Saves::Update(s32 port, s32 slot, StorageInfo* info)
{
    Sync sync = {-1, -1, 0};
    sync.status = MemoryCard::Sync(MemoryCard::SyncNoWait, &sync.function, &sync.result);
    if (sync.status == MemoryCard::SyncFinished)
    {
        g_LastResult = sync.result;
    }

    switch (g_Operation)
    {
    case Operation::None:
        if (g_Next == NoNextOperation)
        {
            MemoryCard::GetInfo(port, slot, &g_Info.type, &g_Info.freeKilobytes, &g_Info.formatted);
        }

        break;
    case Operation::Checking:
        // Once the info asked for last is in: the operation goes on, or ends without a memory card
        if (sync.status != MemoryCard::SyncNothing)
        {
            break;
        }

        if (g_Info.type == StorageMemoryCard)
        {
            g_Operation = static_cast<Operation>(g_Next);
            g_Next = sync.status;
            break;
        }

        g_LastOperation = g_Operation;
        g_Next = NoNextOperation;
        g_Operation = Operation::None;
        g_Step = 0;
        g_Context = ContextTemplate;
        return Operation::NoStorage;
    case Operation::Format:
        FormatStep(sync);
        break;
    case Operation::CreateSave:
        CreateSaveStep(sync);
        break;
    case Operation::MeasureSave:
        MeasureSaveStep(sync);
        break;
    case Operation::FindFile:
        FindFileStep(sync);
        break;
    case Operation::Read:
        ReadStep(sync);
        break;
    case Operation::Write:
        WriteStep(sync);
        break;
    default:
        break;
    }

    if (info != nullptr)
    {
        *info = g_Info;
    }

    return g_Operation;
}

Platform::Saves::Result Platform::Saves::GetResult(Operation operation)
{
    return g_Results[static_cast<s32>(operation)];
}

bool Platform::Saves::Format(s32 port, s32 slot)
{
    return Start(Operation::Format, port, slot);
}

bool Platform::Saves::CreateSave(s32 port, s32 slot, const char* save)
{
    bool started = Start(Operation::CreateSave, port, slot);
    RetailLibc::StringCopy(g_Context.save, save);
    return started;
}

bool Platform::Saves::MeasureSave(s32 port, s32 slot, const char* save, s32* kilobytes)
{
    bool started = Start(Operation::MeasureSave, port, slot);
    g_Context.kilobytes = kilobytes;
    RetailLibc::StringCopy(g_Context.save, save);
    return started;
}

bool Platform::Saves::FindFile(s32 port, s32 slot, const char* file, const char* save, s32* size, FileEntry* entry)
{
    bool started = Start(Operation::FindFile, port, slot);
    g_Context.entry = entry;
    g_Context.size = size;
    RetailLibc::StringCopy(g_Context.save, save);
    RetailLibc::StringCopy(g_Context.file, file);
    return started;
}

bool Platform::Saves::Write(s32 port, s32 slot, const char* save, const char* file, const void* data, s32 size)
{
    bool started = Start(Operation::Write, port, slot);
    g_Context.data = const_cast<void*>(data);
    g_Context.dataSize = size;
    RetailLibc::StringCopy(g_Context.save, save);
    RetailLibc::StringCopy(g_Context.file, file);
    return started;
}

bool Platform::Saves::Read(s32 port, s32 slot, const char* save, const char* file, void* buffer, s32 size)
{
    bool started = Start(Operation::Read, port, slot);
    g_Context.data = buffer;
    g_Context.dataSize = size;
    RetailLibc::StringCopy(g_Context.save, save);
    RetailLibc::StringCopy(g_Context.file, file);
    return started;
}

u32 Platform::Saves::FileKilobytes(u32 size)
{
    return (size + ClusterSize - 1) >> ClusterShift;
}

u32 Platform::Saves::SaveBytes(u32 kilobytes, u32 files)
{
    return (kilobytes + (files + 1) / 2 + DirectoryClusters) << ClusterShift;
}
