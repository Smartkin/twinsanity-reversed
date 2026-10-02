#include "game/instancefactory.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/chunkloading.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/resources.h"

extern "C"
{
    // A model's node given its instance's collision (still asm)
    void AttachModelCollision(GameNode* node) RETAIL(FUN_001a1960);
    // The node keeping an instance's previous transform (0xA0 bytes, still asm)
    GameNode* ConstructMovementNode(void* node) RETAIL(InitMovementNode);
    // The instance a starter's receiver index stands for (still asm)
    void SetReceiverInstance(u32 index, InstanceContext* instance) RETAIL(FUN_00252af8);
    // A place's matrix made again from its position and rotation once they changed (still asm)
    void RotateAndTranslate(ObjectPlace* place) RETAIL(RotateAndTranslate);
}

namespace
{
// The factory's vtable functions: whether it makes instances, the node of the instance's type, the node of its agent (made from
// its object's type), its model's node, and an instance to stand in for a new one
constexpr u32 CanMakeSlot = 1;
constexpr u32 TypeNodeSlot = 2;
constexpr u32 ObjectNodeSlot = 3;
constexpr u32 ModelNodeSlot = 4;
constexpr u32 StandInSlot = 8;
// And the nodes of triggers and of cameras
constexpr u32 TriggerNodeSlot = 5;
constexpr u32 CameraNodeSlot = 6;
// An instance's vtable function releasing it
constexpr u32 ReleaseSlot = 4;
// The factory's byte given to a holder
constexpr u32 FactoryByteSlot = 7;
// Bit 5 of an instance's state: it gets a node keeping its previous transform
constexpr u32 StateTracksMovement = 0x20;
constexpr u32 MovementNodeSize = 0xA0;
// An instance's collision bounds 0x30 bytes into its collision
constexpr u32 CollisionBounds = 0x30;
constexpr u16 NoId = 0xFFFF;

GameObject* ObjectOf(InstanceFactory* factory, u16 id)
{
    if (id == NoId)
    {
        return nullptr;
    }

    return static_cast<GameObject*>(factory->resources->objects->items[id & 0x7FFF]);
}
}

void InstanceFactory::ClearFlag0()
{
    flags &= ~Flag0;
}

void InstanceFactory::ClearFlag1()
{
    flags &= ~Flag1;
}

void InstanceFactory::SetFlag2()
{
    flags |= Flag2;
}

void InstanceFactory::SetFlag3()
{
    flags |= Flag3;
}

void InstanceFactory::SetFlag4()
{
    flags |= Flag4;
}

GameNode* MakeObjectNode(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance, InstanceContext* context, GameNode*)
{
    GameObject* object = ObjectOf(factory, static_cast<u16>(instance->objectId));
    auto* node = CallVirtual<GameNode*>(factory, factory->vtable, ObjectNodeSlot, chunk, object, context);
    if (node != nullptr)
    {
        u16 receiver = static_cast<u16>(instance->refListIndex);
        if (receiver != NoId)
        {
            SetReceiverInstance(receiver & 0xFF, context);
        }
    }

    return node;
}

InstanceContext* CreateInstanceContext(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance)
{
    if (CallVirtual<u32>(factory, factory->vtable, CanMakeSlot, chunk, instance) == 0)
    {
        return nullptr;
    }

    auto* context = InstanceContext::Construct(static_cast<InstanceContext*>(MemoryAllocate(sizeof(InstanceContext))));
    if (context == nullptr)
    {
        return nullptr;
    }

    Reference* data = chunk->data;
    auto* chunkData = data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr;
    PropertyList* properties = instance->properties;
    GameObject* object = ObjectOf(factory, static_cast<u16>(instance->objectId));
    ObjectPlace* place = context->place;
    place->SyncPosition();
    if (place->MoveTo(&instance->position))
    {
        QueueObject(context);
    }

    place = context->place;
    place->SyncRotation();

    TaggedValue x = instance->rotation[0];
    TaggedValue y = instance->rotation[1];
    TaggedValue z = instance->rotation[2];
    Vector4 rotation;
    GetRotationXYZ(&rotation, &x.raw, &y.raw, &z.raw);
    if (place->TurnTo(&rotation))
    {
        QueueObject(context);
    }

    auto* modelNode = CallVirtual<GameNode*>(factory, factory->vtable, ModelNodeSlot, chunk, instance, context);
    if (modelNode != nullptr)
    {
        RegisterNode(context, 1, modelNode);
        AttachModelCollision(modelNode);
    }

    GameNode* objectNode = MakeObjectNode(factory, chunk, instance, context, modelNode);
    if (objectNode != nullptr)
    {
        RegisterNode(context, 1, objectNode);
    }

    auto* typeNode = CallVirtual<GameNode*>(factory, factory->vtable, TypeNodeSlot, chunk, instance, context, objectNode);
    if (typeNode != nullptr)
    {
        // The agent learns its chunk and its spawn's starter, and its ID when it keeps a persistent flag of a layout's instance
        Agent* agent = reinterpret_cast<ObjectNode*>(objectNode)->agent;
        PropertyHolder* agentProperties = agent->properties;
        agent->chunkIndex = chunk->index;
        agent->spawnScript = static_cast<u16>(instance->spawnScript);
        if ((factory->flags & InstanceFactory::Flag3) != 0 && (agentProperties->state & PropertyHolder::StatePersistentFlag) != 0)
        {
            agent->id = chunk->NextFlagSlot();
        }

        RegisterNode(context, 1, typeNode);
    }

    if ((properties->state & StateTracksMovement) != 0)
    {
        GameNode* movement = ConstructMovementNode(MemoryAllocate(MovementNodeSize));
        if (movement != nullptr)
        {
            RegisterNode(context, 1, movement);
        }
    }

    auto* standIn = CallVirtual<InstanceContext*>(factory, factory->vtable, StandInSlot, chunk, object, instance, context);
    if (standIn == nullptr)
    {
        reinterpret_cast<ScriptPack*>(reinterpret_cast<u8*>(object) + offsetof(GameObject, scripts))->Run(objectNode);
        ObjectPlace* placed = context->place;
        RotateAndTranslate(placed);
        RotateAndTranslate(placed);
        GetInstanceHullBounds(&context->collision, &placed->matrix, &context->collision.box);
        ChunkData::AddInstance(chunkData, context);
        return context;
    }

    ChunkData::AddInstance(chunkData, context);
    CallVirtual<u32>(context, context->vtable, ReleaseSlot);
    return standIn;
}

namespace
{
InstanceContext* MakeTriggerContext(InstanceFactory* factory, ChunkEntry* chunk, LayoutTrigger* trigger, u32 nodeSlot)
{
    Reference* data = chunk->data;
    auto* chunkData = data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr;
    auto* context = InstanceContext::Construct(static_cast<InstanceContext*>(MemoryAllocate(sizeof(InstanceContext))));
    auto* node = CallVirtual<TriggerNode*>(factory, factory->vtable, nodeSlot, chunk, trigger, context);
    node->checkTicks = static_cast<s32>(trigger->checkInterval * g_ClockUnitsPerSecond);
    ObjectPlace* place = context->place;
    place->SyncRotation();

    if (place->TurnTo(&trigger->rotation))
    {
        QueueObject(context);
    }

    place = context->place;
    place->SyncPosition();
    if (place->MoveTo(&trigger->position))
    {
        QueueObject(context);
    }

    Vector4 min = {-trigger->scale.x, -trigger->scale.y, -trigger->scale.z, 1.0f};
    SetCollisionBox(&context->collision, &min, &trigger->scale, 1, 1);
    context->flags = (context->flags | factory->creationFlags) & ~factory->clearedFlags;
    RegisterNode(context, 1, node);
    ChunkData::AddInstance(chunkData, context);
    return context;
}
}

InstanceContext* CreateTriggerContext(InstanceFactory* factory, ChunkEntry* chunk, MessageTrigger* trigger)
{
    return MakeTriggerContext(factory, chunk, trigger, TriggerNodeSlot);
}

InstanceContext* CreateCameraContext(InstanceFactory* factory, ChunkEntry* chunk, CameraTrigger* camera)
{
    return MakeTriggerContext(factory, chunk, camera, CameraNodeSlot);
}

extern "C"
{
    // The factories' vtables
    extern const GccVTableEntry g_BaseFactoryVTable[] RETAIL(GameResourceManagerPrototype_Methods);
    extern const GccVTableEntry g_GameFactoryVTable[] RETAIL(GameResourceManager_Methods);

    // The agent object nodes of pickups of subtypes 16 and 17 (0xD0 bytes) and of projectiles (0xF0 bytes), and a model's node
    // from its object (still asm)
    GameNode* ConstructPickupNode(void* node, ChunkEntry* chunk) RETAIL(InitInstanceNodeType1);
    GameNode* ConstructProjectileNode(void* node) RETAIL(InitInstanceNodeType8);
    GameNode* MakeObjectModelNode(GameObject* object) RETAIL(GetOgiNodeFromObject);
    // A message trigger's node (0x1A0 bytes, still asm)
    TriggerNode* ConstructMessageTriggerNode(void* node, ChunkData* chunk, LayoutTrigger* trigger) RETAIL(InitTriggerNode);

    extern const GccVTableEntry g_ProjectileNodeVTable[] RETAIL(UnkNode_0x14_Methods);
}

namespace
{
// The object types (their objects' header's bits 20-27) and subtypes (bits 12-19): pickups of subtypes 16 and 17 have nodes of
// their own, and 16's have no properties
enum ObjectType : u32
{
    TypeCharacter,
    TypePickup,
    TypeCrate,
    TypeCreature,
    TypeGenericObject,
    TypeGrabbable,
    TypePayGate,
    TypeGraple,
    TypeProjectile,
    TypeCount,
};

constexpr u32 TypeShift = 20;
constexpr u32 SubtypeShift = 12;
constexpr u32 PickupNodeSubtype = 0x10;
constexpr u32 PickupNodeSubtypeEnd = 0x12;
// The factory's flags: the agent's properties are the instance's (else its object's); every instance without an ID gets one; the
// factory's byte is in bits 5-12 (0xFF none)
constexpr u32 ByteShift = 5;
constexpr u32 NoByte = 0xFF;
// The clocks the instances step with: most use 1, characters and graples 2
constexpr u8 ObjectClock = 1;
constexpr u8 CharacterClock = 2;
// The object node's vtable function told of the agent
constexpr u32 AgentSlot = 12;
// The holders' vtable functions: how many integers they keep, and an integer's place to write
constexpr u32 HolderIntCountSlot = 11;
constexpr u32 HolderIntToWriteSlot = 6;
// The object node of kind 1 (an instance's agent's)
constexpr u32 AgentObjectNodeKind = 1;
// A context's flags the base's stand-in sets: always 9, 14 when the instance's properties' state has bit 4
constexpr u32 ContextBit9 = 0x200;
constexpr u32 ContextBit14 = 0x4000;
constexpr u32 PropertiesBit4 = 0x10;
constexpr u32 StandInIds = 0x1;
// A trigger node's bits (bits 16-23 of its first u64): 3 (still unknown) and 4 never polled; the message trigger node's flag 1
constexpr u8 TriggerNodeBit3 = 0x8;
constexpr u8 TriggerNodeNeverPolled = 0x10;
constexpr u32 TriggerHeaderNeverPolled = 0x1000;
constexpr u64 MessageNodeBit1 = 0x2;
// The activators a trigger's node tells: their bits made the node kinds of their agents (bits 12-21)
constexpr u32 OnlyCharacterActivator = 1;
constexpr u32 ActivatorKinds[][2] = {{0x1, 0x1000},   {0x4, 0x2000},    {0x8, 0x8000},    {0x2, 0x4000},    {0x10, 0x10000},
                                     {0x20, 0x20000}, {0x80, 0x80000}, {0x40, 0x40000}, {0x100, 0x100000}, {0x200, 0x200000}};
// The sizes of the factory's makings
constexpr u32 ObjectNodeSize = 0x180;
constexpr u32 PickupNodeSize = 0xD0;
constexpr u32 ProjectileNodeSize = 0xF0;
constexpr u32 MessageTriggerNodeSize = 0x1A0;
constexpr u32 AgentNodeSize = 0x1C;

// The node of a message trigger (0x1A0 bytes): a trigger node with 0x30 bytes of its own
struct MessageTriggerNode : TriggerNode
{
    u8 unknown170[0x10];
    u64 bits180;
    u8 unknown188[0x1A0 - 0x188];
};
CHECK_OFFSET(MessageTriggerNode, bits180, 0x180);

// An agent of a type made with its node: the holder of its class (the properties copied in), its part, the creator, the
// factory's byte given to the holder, the agent and its node
template <typename Holder, typename Part, typename AgentType>
AgentNode* MakeAgent(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance, InstanceContext* context,
                     InstanceCreator* creator, Holder* (*makeHolder)(Holder*, PropertyList*), PropertyList* list,
                     Part* (*makePart)(Part*), AgentType* (*makeAgent)(AgentType*, InstanceCreator*, PropertyHolder*, AgentPart*),
                     AgentNode* (*makeNode)(AgentNode*, Agent*))
{
    Holder* holder = makeHolder(static_cast<Holder*>(MemoryAllocate(sizeof(Holder))), list);
    Part* part = makePart(static_cast<Part*>(MemoryAllocate(sizeof(Part))));
    InstanceCreator::Construct(creator, instance, factory->resources, context);
    CallVirtual<void>(factory, factory->vtable, FactoryByteSlot, chunk, static_cast<PropertyHolder*>(holder));
    Agent* agent = makeAgent(static_cast<AgentType*>(MemoryAllocate(sizeof(AgentType))), creator, holder, part);
    return makeNode(static_cast<AgentNode*>(MemoryAllocate(AgentNodeSize)), agent);
}
}

InstanceCreator* InstanceCreator::Construct(InstanceCreator* creator, ObjectInstance* instance, GameResources* resources,
                                            InstanceContext* context)
{
    creator->context = context;
    creator->instance = instance;
    u16 id = static_cast<u16>(instance->objectId);
    creator->object = id != NoId ? static_cast<GameObject*>(resources->objects->items[id & 0x7FFF]) : nullptr;
    return creator;
}

void InstanceCreator::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 BaseFactoryCanMake(InstanceFactory*)
{
    return 1;
}

GameNode* BaseFactoryTypeNode(InstanceFactory*)
{
    return nullptr;
}

GameNode* BaseFactoryObjectNode(InstanceFactory*, ChunkEntry* chunk)
{
    return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(ObjectNodeSize)), chunk, 1, 1);
}

GameNode* MakeModelNode(InstanceFactory* factory, ChunkEntry*, ObjectInstance* instance)
{
    GameObject* object = ObjectOf(factory, static_cast<u16>(instance->objectId));
    if (object == nullptr)
    {
        return nullptr;
    }

    return MakeObjectModelNode(object);
}

// Never polled when its trigger's header says so
TriggerNode* BaseFactoryTriggerNode(InstanceFactory*, ChunkEntry* chunk, LayoutTrigger* trigger)
{
    Reference* data = chunk->data;
    auto* chunkData = data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr;
    TriggerNode* node = ConstructMessageTriggerNode(MemoryAllocate(MessageTriggerNodeSize), chunkData, trigger);
    if ((trigger->header & TriggerHeaderNeverPolled) != 0)
    {
        node->nodeBits |= TriggerNodeNeverPolled;
    }

    return node;
}

CameraNode* BaseFactoryCameraNode(InstanceFactory*, ChunkEntry* chunk, CameraTrigger* camera)
{
    Reference* data = chunk->data;
    auto* chunkData = data != nullptr ? reinterpret_cast<ChunkData*>(data->object) : nullptr;
    return CameraNode::Construct(static_cast<CameraNode*>(MemoryAllocate(sizeof(CameraNode))), chunkData, camera);
}

// In the holder's first integer, or the extras' when the class keeps none
void GiveFactoryByte(InstanceFactory* factory, ChunkEntry*, PropertyHolder* holder)
{
    u32 value = factory->flags >> ByteShift & 0xFF;
    if (value == NoByte)
    {
        return;
    }

    u32 count = CallVirtual<u32>(holder, holder->vtable, HolderIntCountSlot);
    if (count != 0)
    {
        *CallVirtual<s32*>(holder, holder->vtable, HolderIntToWriteSlot, 0u) = static_cast<s32>(value);
        return;
    }

    if (holder->extras != nullptr)
    {
        holder->extras->SetInt(-count, static_cast<s32>(value));
    }
}

InstanceContext* BaseFactoryStandIn(InstanceFactory* factory, ChunkEntry*, GameObject*, ObjectInstance* instance,
                                    InstanceContext* context)
{
    PropertyList* properties = instance->properties;
    context->flags |= ContextBit9;
    context->flags |= factory->creationFlags;
    context->flags &= ~factory->clearedFlags;
    if ((factory->flags & StandInIds) != 0 && context->id == -1)
    {
        g_InstanceIds->Add(context);
    }

    if ((properties->state & PropertiesBit4) != 0)
    {
        context->flags |= ContextBit14;
    }

    return nullptr;
}

void BaseFactoryDestroy(InstanceFactory* factory, u32 destroyFlags)
{
    factory->vtable = g_BaseFactoryVTable;
    if (factory->templates.data != nullptr)
    {
        MemoryDeallocate_(factory->templates.data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(factory);
    }
}

u32 GameFactoryCanMake(InstanceFactory*)
{
    return 1;
}

// The agent's properties are the instance's while the factory says so, else its object's. The projectiles' holder, part, agent
// and node get their vtables after their bases' constructors
GameNode* MakeTypeNode(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance, InstanceContext* context,
                       GameNode* objectNode)
{
    GameObject* object = ObjectOf(factory, static_cast<u16>(instance->objectId));
    u32 header = object->header[0];
    u32 type = header >> TypeShift & 0xFF;
    PropertyList* list = (factory->flags & InstanceFactory::Flag2) != 0 ? instance->properties : object->properties;
    InstanceCreator creator;
    AgentNode* node = nullptr;
    switch (type)
    {
    case TypeCrate:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCratePropertyHolder, list, CratePart::Construct,
                         CrateAgent::Construct, ConstructCrateNode);
        context->clockIndex = ObjectClock;
        context->flags |= ReferencedObject::FlagPhysicsBody;
        creator.Destroy(DestroyOnly);
        break;
    case TypeCharacter:
    {
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCharacterPropertyHolder, list,
                         CharacterPart::Construct, CharacterAgent::Construct, ConstructCharacterNode);
        auto* agentNode = static_cast<ObjectNodeBase*>(GetGameNode(&context->nodes, AgentObjectNodeKind));
        agentNode->flags |= ObjectNodeBase::FlagPinned;
        context->clockIndex = CharacterClock;
        creator.Destroy(DestroyOnly);
        break;
    }
    case TypePickup:
        if ((header >> SubtypeShift & 0xFF) == PickupNodeSubtype)
        {
            list = nullptr;
        }

        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructPickupPropertyHolder, list, PickupPart::Construct,
                         PickupAgent::Construct, ConstructPickupAgentNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypeCreature:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCreaturePropertyHolder, list,
                         CreaturePart::Construct, CreatureAgent::Construct, ConstructCreatureNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypeGenericObject:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGenericObjectPropertyHolder, list,
                         GenericObjectPart::Construct, GenericObjectAgent::Construct, ConstructGenericObjectNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypeGrabbable:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGrabbablePropertyHolder, list,
                         GrabbablePart::Construct, GrabbableAgent::Construct, ConstructGrabbableNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypePayGate:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructPayGatePropertyHolder, list,
                         PayGatePart::Construct, PayGateAgent::Construct, ConstructPayGateNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypeGraple:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGraplePropertyHolder, list, GraplePart::Construct,
                         GrapleAgent::Construct, ConstructGrapleNode);
        context->clockIndex = CharacterClock;
        creator.Destroy(DestroyOnly);
        break;
    case TypeProjectile:
    {
        auto* holder = static_cast<ProjectilePropertyHolder*>(MemoryAllocate(sizeof(ProjectilePropertyHolder)));
        PropertyHolder::Construct(holder);
        holder->vtable = g_ProjectileHolderVTable;
        if (list != nullptr)
        {
            holder->CopyFrom(list);
        }

        auto* part = static_cast<ProjectilePart*>(
            BasicAgentPart::Construct(static_cast<BasicAgentPart*>(MemoryAllocate(sizeof(ProjectilePart)))));
        part->vtable = g_ProjectilePartVTable;
        InstanceCreator::Construct(&creator, instance, factory->resources, context);
        CallVirtual<void>(factory, factory->vtable, FactoryByteSlot, chunk, static_cast<PropertyHolder*>(holder));
        auto* basic = static_cast<BasicAgent*>(MemoryAllocate(sizeof(BasicAgent)));
        Agent* agent = BasicAgent::Construct(basic, &creator, holder, part);
        agent->vtable = g_ProjectileAgentVTable;
        node = ConstructAgentNode(static_cast<AgentNode*>(MemoryAllocate(AgentNodeSize)), agent);
        context->clockIndex = ObjectClock;
        node->vtable = g_ProjectileNodeVTable;
        creator.Destroy(DestroyOnly);
        break;
    }
    }

    // A type without an agent tells the object node what's 0x18 bytes into nothing
    CallVirtual<void>(objectNode, objectNode->vtable, AgentSlot, node->agent);
    return node;
}

GameNode* MakeAgentObjectNode(InstanceFactory*, ChunkEntry* chunk, GameObject* object)
{
    u32 header = object->header[0];
    u32 subtype = header >> SubtypeShift & 0xFF;
    switch (header >> TypeShift & 0xFF)
    {
    case TypeCharacter:
    case TypeCreature:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(ObjectNodeSize)), chunk, 1, 1);
    case TypePickup:
        if (subtype >= PickupNodeSubtype && subtype < PickupNodeSubtypeEnd)
        {
            return ConstructPickupNode(MemoryAllocate(PickupNodeSize), chunk);
        }

        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(ObjectNodeSize)), chunk, 0, 0);
    case TypeGenericObject:
    case TypeGrabbable:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(ObjectNodeSize)), chunk, 1, 0);
    case TypeCrate:
    case TypePayGate:
    case TypeGraple:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(ObjectNodeSize)), chunk, 0, 0);
    case TypeProjectile:
        return ConstructProjectileNode(MemoryAllocate(ProjectileNodeSize));
    default:
        return nullptr;
    }
}

// A trigger the character alone activates is never polled unless its node's bit 3 is set (then it's told characters' events);
// one nothing activates is never polled, its node's flag 1 set; the others are told the events of their activators' agents
TriggerNode* GameFactoryTriggerNode(InstanceFactory* factory, ChunkEntry* chunk, LayoutTrigger* trigger, InstanceContext* context)
{
    auto* node = static_cast<MessageTriggerNode*>(BaseFactoryTriggerNode(factory, chunk, trigger));
    u32 activators = trigger->activators;
    if (activators == OnlyCharacterActivator && (node->nodeBits & TriggerNodeBit3) == 0)
    {
        node->nodeBits |= TriggerNodeNeverPolled;
        node->bits180 &= ~MessageNodeBit1;
    }
    else if (activators == 0)
    {
        if ((node->nodeBits & TriggerNodeBit3) == 0)
        {
            node->nodeBits |= TriggerNodeNeverPolled;
        }

        node->bits180 = (node->bits180 & ~MessageNodeBit1) | MessageNodeBit1;
    }
    else
    {
        u32 kinds = 0;
        for (const auto& [activator, kind] : ActivatorKinds)
        {
            if ((activators & activator) != 0)
            {
                kinds |= kind;
            }
        }

        node->eventKinds = kinds;
    }

    context->clockIndex = ObjectClock;
    return node;
}

// The camera is told its activators as they are, never polled when the character activates it
CameraNode* GameFactoryCameraNode(InstanceFactory* factory, ChunkEntry* chunk, CameraTrigger* camera, InstanceContext* context)
{
    CameraNode* node = BaseFactoryCameraNode(factory, chunk, camera);
    u32 activators = camera->activators;
    if ((activators & OnlyCharacterActivator) != 0)
    {
        node->nodeBits |= TriggerNodeNeverPolled;
    }

    node->eventKinds = activators;
    context->clockIndex = ObjectClock;
    return node;
}

void GameFactoryDestroy(InstanceFactory* factory, u32 destroyFlags)
{
    factory->vtable = g_GameFactoryVTable;
    BaseFactoryDestroy(factory, destroyFlags);
}
