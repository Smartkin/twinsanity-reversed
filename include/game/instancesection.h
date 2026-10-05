#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "game/layout.h"
#include "game/readers.h"

struct AiNavigation;
struct ChunkEntry;
struct GameResources;
struct InstanceContext;

// The contexts made of a layout's object instances while it's read (8 bytes): them by the instances' index, and how many there's
// room for
struct LayoutContexts
{
    InstanceContext** contexts;
    u32 capacity;
};
CHECK_SIZE(LayoutContexts, 0x8);

// A layout's instances' flags
union LayoutInstancesFlags
{
    u32 value;
    struct
    {
        // (The RM2 reader's bit 0: its graphics finish without registering)
        u32 unused0 : 1;
        // Its instances are the chunk's own (layouts 0-2 and 7)
        u32 chunkOwn : 1;
        // How many of its object instances got contexts
        u32 contextCount : 14;
        u32 unused16 : 16;
    };
};
CHECK_SIZE(LayoutInstancesFlags, 4);

// A layout's instances as an RM2's instance section has them (0x40 bytes): flags, the game's resources, the chunk, the list of
// each kind (pointer arrays: the instance templates, the object instances, the AI positions, the AI paths, the positions, the
// paths, the triggers, the cameras and the collision surfaces), the contexts of its object instances, and the chunk's AI
// navigation, positions and paths it adds its own to
struct LayoutInstances
{
    // The kinds' lists
    enum Kind : u32
    {
        KindTemplates,
        KindObjectInstances,
        KindAiPositions,
        KindAiPaths,
        KindPositions,
        KindPaths,
        KindTriggers,
        KindCameras,
        KindSurfaces,
        KindCount,
    };

    LayoutInstancesFlags flags;
    GameResources* resources;
    ChunkEntry* chunk;
    PointerArray<void>* kinds[KindCount];
    LayoutContexts* contexts;
    AiNavigation* navigation;
    PointerArray<void>* positions;
    PointerArray<void>* paths;

    static LayoutInstances* Construct(LayoutInstances* layout, u32 unregistered, u32 chunkOwn, GameResources* resources,
                                      ChunkEntry* chunk) RETAIL(FUN_0026a978);
    // Once every kind is read: the chunk's AI navigation linked, every object instance's context linked to the contexts of the
    // instances it names, and the contexts' table let go
    void Finish() RETAIL(FUN_00266ba0);
    // Once the chunk has what was read: the lists of what it keeps let go (the templates the factory has, the AI positions and
    // paths, the positions, the paths and the surfaces), the object instances (their contexts made), the triggers and the
    // cameras destroyed with their lists
    void ReleaseRead() RETAIL(FUN_00266d90);
    // Every element of its lists destroyed, then the lists freed
    void DestroyElements() RETAIL(FUN_002670f0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00266628);

    // Each kind's element registered as it's read (the index it gets in its list, the count of the kind): an instance template
    // goes to the factory's templates, an object instance gets a context (the factory makes it with the flags of the layout) linked
    // to its positions and paths, an AI position or path goes into the chunk's AI navigation (made for the first, its lists for
    // the count when they have none), a position or a path into the chunk's list (made for the count, the count of the positions
    // kept when the layout is the chunk's own), a trigger of kind 0 becomes a box of the chunk's data, another trigger or a
    // camera gets a context whose node tells the instances it names, a collision surface goes into the game's table
    void RegisterTemplate(u32 index, u32 count, InstanceTemplate* instanceTemplate) RETAIL(FUN_0026aae8);
    void RegisterObjectInstance(u32 index, u32 count, ObjectInstance* instance) RETAIL(FUN_002667e0);
    void RegisterAiPosition(u32 index, u32 count, struct AiPosition* position) RETAIL(FUN_0026abc0);
    void RegisterAiPath(u32 index, u32 count, struct AiPath* path) RETAIL(FUN_0026ac78);
    void RegisterPosition(u32 index, u32 count, LayoutPosition* position) RETAIL(FUN_0026ad30);
    void RegisterPath(u32 index, u32 count, LayoutPath* path) RETAIL(FUN_0026ae70);
    void RegisterTrigger(u32 index, u32 count, MessageTrigger* trigger) RETAIL(FUN_00266958);
    void RegisterCamera(u32 index, u32 count, CameraTrigger* camera) RETAIL(FUN_00266a90);
    void RegisterSurface(u32 index, u32 count, CollisionSurface* surface) RETAIL(FUN_0026af98);
};
CHECK_OFFSET(LayoutInstances, contexts, 0x30);
CHECK_SIZE(LayoutInstances, 0x40);

// An item of an instance section's kind (0x10 bytes, each kind's vtable: 1 the destructor, 2 the count, 3 (1), 4 whether it reads
// a section's type (1), 5 a section reader (none for an empty item), 6 every element let go and the list freed, 7 the count given
// (the list allocated for it), 8 nothing): the layout, its list and the count
struct InstanceKindItem : ItemInterface
{
    LayoutInstances* layout;
    PointerArray<void>** list;
    u32 count;
};
CHECK_SIZE(InstanceKindItem, 0x10);

// The section reader of an instance section's kind (0x10 bytes, each kind's vtable: 1 the destructor, 2 the element read from the
// section, registered with the layout and added to its list): the layout, its list and the count
struct InstanceKindReader
{
    const GccVTableEntry* vtable;
    LayoutInstances* layout;
    PointerArray<void>** list;
    u32 count;
};
CHECK_SIZE(InstanceKindReader, 0x10);

// The RM2's instance section item (0x98 bytes; vtable: 1 the destructor, 2 the count of kinds (9), 3 (1), 4 whether it reads a
// section's type (1), 5 a reader of a kind's section, 6 and 7 nothing, 8 the layout's instances made once every kind is read): the
// layout and an item of each kind
struct InstanceSectionItem : ItemInterface
{
    LayoutInstances* layout;
    InstanceKindItem kinds[LayoutInstances::KindCount];

    static InstanceSectionItem* Construct(InstanceSectionItem* item, LayoutInstances* layout) RETAIL(InitInstanceSectionItem);
    // A section of the layout queued for the item of its kind (nothing past the kinds): the sections' IDs go templates, AI
    // positions, AI paths, positions, paths, collision surfaces, object instances, triggers and cameras
    void QueueKind(u32 section, u32 start) RETAIL(LoadInstanceSectionSubSection);
};
CHECK_SIZE(InstanceSectionItem, 0x98);

// The section reader of a kind of an instance section (0x10 bytes; vtable: 1 the destructor, 2 the kind's section queued): the
// item, the section's ID and where it is
struct InstanceSectionReader
{
    const GccVTableEntry* vtable;
    InstanceSectionItem* item;
    u32 section;
    u32 offset;
};
CHECK_SIZE(InstanceSectionReader, 0x10);
