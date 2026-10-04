#pragma once

#include "abi.h"
#include "common.h"
#include "game/array.h"
#include "gcc2.h"

struct ChunkEntry;
struct GameResources;
struct InstanceContext;
struct InstanceTemplate;

// The game's factory of instances (the game context's, retail's GameResourceManager, 0x34 bytes; its vtable 0x24 bytes in makes
// the nodes of an instance by its type): the flags the instances it makes get (0x2000: of a layout that isn't the
// chunk's own), its own flags (bits 0-4, set and cleared before it makes an instance; bits 5-12 a byte it gives the instances,
// 0xFF none), an ID, the game's resources and the instance templates the chunks' layouts read
struct InstanceFactory
{
    enum Flags : u32
    {
        Flag0 = 0x1,
        Flag1 = 0x2,
        Flag2 = 0x4,
        Flag3 = 0x8,
        Flag4 = 0x10,
    };

    // The flags of the instances of layouts that aren't the chunk's own
    static constexpr u32 NotChunkOwn = 0x2000;

    u32 creationFlags;
    // The flags the triggers and cameras it makes lose
    u32 clearedFlags;
    u32 flags;
    u16 id;
    u16 unknown0E;
    GameResources* resources;
    PointerArray<InstanceTemplate> templates;
    const GccVTableEntry* vtable;
    u8 unknown28[0x34 - 0x28];

    void SetFlag0() RETAIL(FUN_002627b0);
    void ClearFlag0() RETAIL(FUN_002627c0);
    void ClearFlag1() RETAIL(FUN_002627d8);
    void SetFlag2() RETAIL(FUN_002627f0);
    void SetFlag3() RETAIL(FUN_00262808);
    void ClearFlag3() RETAIL(FUN_00262818);
    void SetFlag4() RETAIL(FUN_00262830);
    void ClearFlag4() RETAIL(FUN_00262840);
};
CHECK_OFFSET(InstanceFactory, templates, 0x14);
CHECK_OFFSET(InstanceFactory, vtable, 0x24);
CHECK_SIZE(InstanceFactory, 0x34);

extern "C"
{
    extern InstanceFactory* g_InstanceFactory RETAIL(G_GameResourcesManager_);

    // The context of an object instance of a chunk's layout made: placed, given the nodes the factory makes for its model, its agent
    // (its object's type) and its type, its object's script pack run on its agent, put in the chunk (nullptr when the factory makes
    // none; the instance the factory has stand in for it instead, which it's released for)
    InstanceContext* CreateInstanceContext(InstanceFactory* factory, ChunkEntry* chunk, struct ObjectInstance* instance)
        RETAIL(CreateInstanceContext);
    // An instance of an object made at a position turned by angles (65536ths of a turn about x, y and z) with its object's
    // properties, as a layout's would be
    InstanceContext* CreateInstance(InstanceFactory* factory, ChunkEntry* chunk, u32 objectId, const struct Vector4* position,
                                    const s32* angles) RETAIL(CreateInstance);
    // The same, but given the properties of another instance's agent (its class's values and its extras), or its object's when
    // asked and it has some
    InstanceContext* CreateInstanceFrom(InstanceFactory* factory, ChunkEntry* chunk, InstanceContext* source, u32 objectId,
                                        const struct Vector4* position, const s32* angles, u32 useObjectProperties)
        RETAIL(FUN_0025ecd0);
    // The agent's node of an object instance's context made (its object's type) and the context made the instance its receiver
    // index stands for
    struct GameNode* MakeObjectNode(InstanceFactory* factory, ChunkEntry* chunk, struct ObjectInstance* instance,
                                    InstanceContext* context, struct GameNode* modelNode) RETAIL(FUN_0025dfa0);
    // The context of a trigger and of a camera made: the node the factory makes for it checking its box every interval, placed at
    // the trigger's rotation and position, its collision the trigger's box, the factory's flags set and cleared, in the chunk
    InstanceContext* CreateTriggerContext(InstanceFactory* factory, ChunkEntry* chunk, class MessageTrigger* trigger) RETAIL(FUN_0025e558);
    InstanceContext* CreateCameraContext(InstanceFactory* factory, ChunkEntry* chunk, class CameraTrigger* camera) RETAIL(FUN_0025e888);
}

// What a type node's agent is made from (on the stack): the instance, its object and its context
struct InstanceCreator
{
    struct ObjectInstance* instance;
    struct GameObject* object;
    InstanceContext* context;

    static InstanceCreator* Construct(InstanceCreator* creator, struct ObjectInstance* instance, GameResources* resources,
                                      InstanceContext* context) RETAIL(InitInstanceConstructor);
    void Destroy(u32 destroyFlags) RETAIL(DestroyInstanceConstructor);
};

extern "C"
{
    // The factory's vtable functions, the base's (retail's GameResourceManagerPrototype) and the game's (GameResourceManager):
    // 1 whether it makes the instance (both: always), 2 the instance's type node (the base: none; the game's: the agent of its
    // object's type, its class's property holder and their node), 3 its object node (the base: one with waypoints; the game's:
    // the kind its object's type has), 4 its model's node, 5 a trigger's node and 6 a camera's (the game's set the instances
    // they tell), 7 the factory's byte given to an instance's first integer, 8 an instance to stand in for a new one (the base:
    // none, its context's flags set; the game's keeps the playable characters) and 9 the destructor
    u32 BaseFactoryCanMake(InstanceFactory* factory) RETAIL(FUN_002623f8);
    struct GameNode* BaseFactoryTypeNode(InstanceFactory* factory) RETAIL(FUN_002624c8);
    struct GameNode* BaseFactoryObjectNode(InstanceFactory* factory, ChunkEntry* chunk) RETAIL(FUN_00262488);
    struct GameNode* MakeModelNode(InstanceFactory* factory, ChunkEntry* chunk, struct ObjectInstance* instance)
        RETAIL(GetOgiNodeFromInstance);
    struct TriggerNode* BaseFactoryTriggerNode(InstanceFactory* factory, ChunkEntry* chunk, class LayoutTrigger* trigger)
        RETAIL(FUN_002624d0);
    struct CameraNode* BaseFactoryCameraNode(InstanceFactory* factory, ChunkEntry* chunk, class CameraTrigger* camera)
        RETAIL(FUN_00262550);
    void GiveFactoryByte(InstanceFactory* factory, ChunkEntry* chunk, class PropertyHolder* holder) RETAIL(FUN_00262728);
    InstanceContext* BaseFactoryStandIn(InstanceFactory* factory, ChunkEntry* chunk, struct GameObject* object,
                                        struct ObjectInstance* instance, InstanceContext* context) RETAIL(FUN_002625a0);
    void BaseFactoryDestroy(InstanceFactory* factory, u32 destroyFlags) RETAIL(FUN_002626c8);
    u32 GameFactoryCanMake(InstanceFactory* factory) RETAIL(FUN_0013f180);
    struct GameNode* MakeTypeNode(InstanceFactory* factory, ChunkEntry* chunk, struct ObjectInstance* instance,
                                  InstanceContext* context, struct GameNode* objectNode) RETAIL(GetNodeBasedOnInstanceType);
    struct GameNode* MakeAgentObjectNode(InstanceFactory* factory, ChunkEntry* chunk, struct GameObject* object)
        RETAIL(GetInstanceNodeFromObject);
    struct TriggerNode* GameFactoryTriggerNode(InstanceFactory* factory, ChunkEntry* chunk, class LayoutTrigger* trigger,
                                               InstanceContext* context) RETAIL(GetTriggerNode);
    struct CameraNode* GameFactoryCameraNode(InstanceFactory* factory, ChunkEntry* chunk, class CameraTrigger* camera,
                                             InstanceContext* context) RETAIL(FUN_0013f1b8);
    void GameFactoryDestroy(InstanceFactory* factory, u32 destroyFlags) RETAIL(FUN_0013f250);
    // The game's stand-in: a playable character already in a chunk stands in for the new instance (none: it's kept, made the
    // character's with places, controls and follow nodes; other instances set up, then the base's stand-in)
    InstanceContext* GameFactoryStandIn(InstanceFactory* factory, ChunkEntry* chunk, struct GameObject* object,
                                        struct ObjectInstance* instance, InstanceContext* context) RETAIL(FUN_0012dee0);
    // The factories made: the game's (the resources handed on to the base's constructor) and the base's (no flags, no ID, room
    // for 10 templates, its byte none, its flag 4 set)
    InstanceFactory* ConstructGameFactory(InstanceFactory* factory, GameResources* resources) RETAIL(FUN_0013f218);
    InstanceFactory* ConstructBaseFactory(InstanceFactory* factory, GameResources* resources) RETAIL(FUN_00262648);
}

// The model nodes and the instances made outside the layouts (game/modelinstances.cpp)
extern "C"
{
    // A model's node: its OGI (none for the model 0xFFFF) with an object's camera joints and exit points (bits 6-11 and 0-5 of its
    // header's first word), or with neither
    struct GameNode* MakeObjectModelNode(struct GameObject* object) RETAIL(GetOgiNodeFromObject);
    struct ModelNode* MakePlainModelNode(u16 model) RETAIL(FUN_002599b0);
    // New instances in a chunk, at a matrix's position and turn (none: where they're made): a shown instance of a model (a
    // cutscene's), and the camera's (its box 0.1 either way, a lens node of the default field of view)
    InstanceContext* MakeModelInstance(struct ChunkData* chunk, u16 model, const struct Matrix4x4* matrix) RETAIL(FUN_00259a48);
    InstanceContext* MakeCameraInstance(struct ChunkData* chunk, const struct Matrix4x4* matrix) RETAIL(FUN_00259ce0);
}
