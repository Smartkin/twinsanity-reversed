#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/string.h"

// The builder of the game objects' kinds of items (the game context's, vtable D_00303628 over the items' builders' base
// BuilderBaseFunctions; 4 bytes): 1 the destructor, 2 an empty item of a type made
struct ObjectItemBuilder
{
    // The item types it makes: game objects, property lists, the named items of 0xA0 bytes, code models, the named items of
    // 0x28 bytes, the named items of 0x50 bytes and resource references; and the named items' base's type
    enum Type : u32
    {
        TypeGameObject = 0x1B02,
        TypePropertyList = 0x1B04,
        TypeNamedA0 = 0x1B0B,
        TypeCodeModel = 0x1B13,
        TypeNamed28 = 0x1B14,
        TypeNamed50 = 0x1B16,
        TypeResourceReferences = 0x1B18,
        TypeNamed = 0x1B0E,
    };

    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00261c98);
    // An item of a type made empty (none for another type)
    void* Make(u32 type) RETAIL(FUN_0025d098);
};

// The base of named items the builder makes (0x20 bytes, type 0x1B0E, vtable D_00303648 at 0x1C: 1 the destructor, 2 its type):
// its name, a byte and a word made 0, an ID (-1 none) and a byte made 1 (nothing reads the bytes and the word)
struct NamedItem
{
    String name;
    u8 unused0C;
    u8 unused0D;
    u16 unused0E;
    u32 unused10;
    s32 id;
    u8 unused18;
    u8 unused19[3];
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_00263ae0);
    u32 ItemType() RETAIL(FUN_00263b30);
};
CHECK_OFFSET(NamedItem, id, 0x14);
CHECK_OFFSET(NamedItem, vtable, 0x1C);
CHECK_SIZE(NamedItem, 0x20);

// The named items of type 0x1B0B (0xA0 bytes, vtable D_003035F0) and 0x1B16 (0x50 bytes, vtable D_003035D0, another ID (-1 none)
// at 0x40): nothing of their own but what the builder sets, their destructors the base's
struct NamedItemA0 : NamedItem
{
    u8 unused20[0xA0 - 0x20];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00261cd0);
    u32 ItemType() RETAIL(FUN_00261d20);
};
CHECK_SIZE(NamedItemA0, 0xA0);

struct NamedItem50 : NamedItem
{
    u8 unused20[0x40 - 0x20];
    s32 otherId;
    u8 unused44[0x50 - 0x44];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00261d28);
    u32 ItemType() RETAIL(FUN_00261d78);
};
CHECK_OFFSET(NamedItem50, otherId, 0x40);
CHECK_SIZE(NamedItem50, 0x50);

// The named items of type 0x1B14 (0x28 bytes, vtable D_00304BC8): four halfwords made 0, their destructor the base's
struct NamedItem28 : NamedItem
{
    u16 unused20;
    u16 unused22;
    u16 unused24;
    u16 unused26;

    static NamedItem28* Construct(NamedItem28* item) RETAIL(FUN_0026ed60);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0026eed0);
    u32 ItemType() RETAIL(FUN_0026ef20);
};
CHECK_OFFSET(NamedItem28, unused20, 0x20);
CHECK_SIZE(NamedItem28, 0x28);
