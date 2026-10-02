#include "game/archive.h"

#include "game/memory.h"
#include "game/readers.h"
#include "game/stream.h"
#include "retail/libc.h"

// The archives: a BH file's table of the files in its BD file
extern "C"
{
    // The C library's character classes (bit 0 upper case, 1 lower case)
    extern const u8 CasingTable[];
}

namespace
{
constexpr const char* HostPrefix = "HOST0:";

void ConstructStrings(String* strings, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        strings[i].string = nullptr;
        strings[i].length = 0;
        strings[i].capacity = 0;
    }
}

void UpperCase(String* string)
{
    for (s32 i = 0; i < string->length; i++)
    {
        char* character = string->string + i;
        s8 value = *character;
        if ((CasingTable[value] & 2) != 0)
        {
            *character = static_cast<char>(value - 0x20);
        }
    }
}

// The binary search of the table (sorted by StringLess the other way round): the index it ends on
s32 SearchIndex(Archive* archive, String* path)
{
    String probe;
    probe.string = nullptr;
    probe.length = 0;
    probe.capacity = 0;
    s32 high = archive->count;
    s32 low = 0;
    s32 previous = -1;
    s32 found;
    while (true)
    {
        s32 middle = (high - low) / 2 + low;
        StringAssign(&probe, archive->files[middle].path.string);
        if (!StringNotEqual(&probe, path->string))
        {
            found = middle;
            break;
        }

        if (StringLess(&probe, path))
        {
            high = middle;
        }
        else
        {
            low = middle;
        }

        if (previous == middle)
        {
            found = previous;
            break;
        }

        previous = middle;
    }

    StringDestroy(&probe);
    return found;
}

void SetFile(ArchiveFile* file, const ArchiveEntry* entry, const char* path)
{
    String copy;
    copy.string = nullptr;
    copy.length = 0;
    copy.capacity = 0;
    StringAssign(&copy, path);
    file->entry = *entry;
    StringAssign(&file->path, copy.string);
    StringDestroy(&copy);
}
}

extern "C"
{
    void ArchiveLoad(Archive* archive, const char* path, s32 readNow, GameReadersStorage* storage)
    {
        String tablePath;
        String dataPath;
        StringConstruct(&tablePath, path);
        StringConstruct(&dataPath, path);
        StringAppend(&tablePath, ".BH");
        StringAppend(&dataPath, ".BD");
        auto* tableReader = static_cast<ArchiveSectionReader*>(MemoryAllocate(sizeof(ArchiveSectionReader)));
        String copy;
        StringConstruct(&copy, path);
        tableReader->archive = archive;
        tableReader->vtable = g_ArchiveSectionReaderVTable;
        tableReader->path.string = nullptr;
        tableReader->path.length = 0;
        tableReader->path.capacity = 0;
        StringAssign(&tableReader->path, copy.string);
        StringDestroy(&copy);
        GenericItemReader* table = SubItemsReader::ConstructFile(static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader))),
                                                                 tablePath.string, tableReader, 5);
        if (table != nullptr)
        {
            AddItemReaderToReaderStorage(storage, table, 0);
        }

        GenericItemReader* data = BdReader::Construct(static_cast<BdReader*>(MemoryAllocate(sizeof(BdReader))), &dataPath, nullptr, 0, 1);
        if (data != nullptr)
        {
            AddItemReaderToReaderStorage(storage, data, 0);
        }

        if (readNow != 0)
        {
            LoadQueuedSectionsIntoMemory_();
        }

        StringDestroy(&dataPath);
        StringDestroy(&tablePath);
    }

    ArchiveEntry* ArchiveFind(Archive* archive, const char* path)
    {
        String upper;
        StringConstruct(&upper, path);
        UpperCase(&upper);
        StringRemove(&upper, HostPrefix);
        String key;
        key.string = nullptr;
        key.length = 0;
        key.capacity = 0;
        StringAssign(&key, upper.string);
        ArchiveFile* found = ArchiveSearch(archive, &key);
        archive->lastFound = found;
        StringConstruct(&key, path);
        StringListAdd(&archive->lookups, &key);
        StringDestroy(&upper);
        // The entry is the file's start
        return reinterpret_cast<ArchiveEntry*>(found);
    }

    ArchiveFile* ArchiveSearch(Archive* archive, String* path)
    {
        ArchiveFile* found = nullptr;
        if (archive->count != 0)
        {
            s32 index = SearchIndex(archive, path);
            if (!StringNotEqual(&archive->files[index].path, path->string))
            {
                found = &archive->files[index];
            }
        }

        StringDestroy(path);
        return found;
    }

    s32 ArchiveAddFile(Archive* archive, const ArchiveEntry* entry, String* path)
    {
        s32 there = 0;
        if (archive->count == 0)
        {
            archive->count = 1;
            if (archive->files == nullptr)
            {
                archive->capacity = archive->growth;
                ArchiveFile* files = NewArray<ArchiveFile>(archive->capacity);
                for (u32 i = 0; i < archive->capacity; i++)
                {
                    ConstructStrings(&files[i].path, 1);
                }

                archive->files = files;
            }

            SetFile(&archive->files[0], entry, path->string);
        }
        else
        {
            s32 index = SearchIndex(archive, path);
            if (!StringNotEqual(&archive->files[index].path, path->string))
            {
                there = 1;
                SetFile(&archive->files[index], entry, path->string);
            }
            else
            {
                s32 at = StringLess(&archive->files[index].path, path) ? index : index + 1;
                String copy;
                copy.string = nullptr;
                copy.length = 0;
                copy.capacity = 0;
                StringAssign(&copy, path->string);
                ArchiveInsertFile(archive, at, entry, &copy);
            }
        }

        StringDestroy(path);
        return there;
    }

    void ArchiveInsertFile(Archive* archive, s32 index, const ArchiveEntry* entry, String* path)
    {
        if (archive->count < archive->capacity)
        {
            // Everything from the index on moves up one
            for (s32 i = archive->count - 1; i >= index; i--)
            {
                archive->files[i + 1].entry = archive->files[i].entry;
                StringAssign(&archive->files[i + 1].path, archive->files[i].path.string);
            }

            SetFile(&archive->files[index], entry, path->string);
        }
        else
        {
            archive->capacity = static_cast<u16>(archive->capacity + archive->growth);
            u32 capacity = archive->capacity;
            ArchiveFile* files = NewArray<ArchiveFile>(capacity);
            for (u32 i = 0; i < capacity; i++)
            {
                ConstructStrings(&files[i].path, 1);
            }

            for (s32 i = 0; i < index; i++)
            {
                files[i].entry = archive->files[i].entry;
                StringAssign(&files[i].path, archive->files[i].path.string);
            }

            SetFile(&files[index], entry, path->string);
            for (s32 i = index; i < archive->count; i++)
            {
                files[i + 1].entry = archive->files[i].entry;
                StringAssign(&files[i + 1].path, archive->files[i].path.string);
            }

            if (archive->files != nullptr)
            {
                for (ArchiveFile* file = archive->files + ArrayCount(archive->files); file != archive->files;)
                {
                    file--;
                    StringDestroy(&file->path);
                }

                DeleteArray(archive->files);
            }

            archive->files = files;
        }

        archive->count++;
        StringDestroy(path);
    }

    s32 StringListAdd(StringList* list, String* string)
    {
        if (list->count == list->capacity)
        {
            u32 capacity = list->count + list->growth;
            String* items = NewArray<String>(capacity);
            ConstructStrings(items, capacity);
            for (u32 i = 0; i < list->count; i++)
            {
                StringAssign(&items[i], list->items[i].string);
            }

            if (list->items != nullptr)
            {
                for (String* item = list->items + ArrayCount(list->items); item != list->items;)
                {
                    item--;
                    StringDestroy(item);
                }

                DeleteArray(list->items);
            }

            list->items = items;
            list->capacity = capacity;
        }

        u32 index = list->count++;
        StringAssign(&list->items[index], string->string);
        s32 added = static_cast<s32>(list->count) - 1;
        StringDestroy(string);
        return added;
    }
}

void ArchiveSectionReader::Destroy(u32 flags)
{
    vtable = g_ArchiveSectionReaderVTable;
    StringDestroy(&path);
    vtable = g_SectionReaderVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ArchiveSectionReader::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream table;
    MemoryStream::Construct(&table, data, size, 1, 0x40);
    Stream* reader = &table;
    s32 header = 0;
    reader->ReadS32(&header);
    while (reader->Tell() < size)
    {
        s32 length;
        reader->ReadS32(&length);
        char name[0x400];
        RetailLibc::MemoryCopy(name, table.position, static_cast<u32>(length));
        table.position += length;
        String filePath;
        filePath.string = nullptr;
        filePath.length = length;
        filePath.capacity = 0;
        StringReserve(&filePath, length);
        RetailLibc::StringCopyCount(filePath.string, name, static_cast<u32>(length));
        filePath.string[filePath.length] = 0;
        UpperCase(&filePath);
        ArchiveEntry entry;
        reader->ReadS32(reinterpret_cast<s32*>(&entry.start));
        reader->ReadS32(reinterpret_cast<s32*>(&entry.size));
        String key;
        key.string = nullptr;
        key.length = 0;
        key.capacity = 0;
        StringAssign(&key, filePath.string);
        ArchiveAddFile(archive, &entry, &key);
        StringDestroy(&filePath);
    }

    StringAssign(&archive->path, path.string);
    // The table's memory is the stream's: it's freed with it
    table.Destroy(0);
}
