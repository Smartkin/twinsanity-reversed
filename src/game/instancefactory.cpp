#include "game/instancefactory.h"

#include "game/agentparts.h"
#include "game/agents.h"
#include "game/behaviours.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/chunkloading.h"
#include "game/controllers.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/pickups.h"
#include "game/place.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/resources.h"

#include <cstddef>
#include <cstdint>

extern "C"
{
    // A place's matrix made again from its position and rotation once they changed
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
// The factory's subtype given to a holder
constexpr u32 FactorySubtypeSlot = 7;
// An object instance's index in the starter's receivers of none (its -1)
constexpr u16 NoReceiverIndex = 0xFFFF;

// A chunk's data (none while its entry's reference has none)
ChunkData* DataOf(const ChunkEntry* chunk)
{
    ChunkDataReference* reference = chunk->data;
    return reference != nullptr ? reference->chunk : nullptr;
}

GameObject* ObjectOf(InstanceFactory* factory, u16 id)
{
    if (id == NoObjectId)
    {
        return nullptr;
    }

    return static_cast<GameObject*>(factory->resources->objects->items[id & ResourceIndexMask]);
}
}

void InstanceFactory::SetGivesIds()
{
    flags.givesIds = 1;
}

void InstanceFactory::ClearGivesIds()
{
    flags.givesIds = 0;
}

void InstanceFactory::ClearUnused1()
{
    flags.unused1 = 0;
}

void InstanceFactory::SetInstanceProperties()
{
    flags.instanceProperties = 1;
}

void InstanceFactory::SetGivesFlagSlots()
{
    flags.givesFlagSlots = 1;
}

void InstanceFactory::SetUnused4()
{
    flags.unused4 = 1;
}

GameNode* MakeObjectNode(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance, InstanceContext* context, GameNode*)
{
    GameObject* object = ObjectOf(factory, static_cast<u16>(instance->objectId));
    auto* node = CallVirtual<GameNode*>(factory, factory->vtable, ObjectNodeSlot, chunk, object, context);
    if (node != nullptr)
    {
        u16 receiver = static_cast<u16>(instance->refListIndex);
        if (receiver != NoReceiverIndex)
        {
            SetGlobalAgent(receiver & 0xFF, context);
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

    ChunkData* chunkData = DataOf(chunk);
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
        RegisterNode(context, AttachNode, modelNode);
        static_cast<ModelNode*>(modelNode)->AttachCollision();
    }

    GameNode* objectNode = MakeObjectNode(factory, chunk, instance, context, modelNode);
    if (objectNode != nullptr)
    {
        RegisterNode(context, AttachNode, objectNode);
    }

    auto* typeNode = CallVirtual<GameNode*>(factory, factory->vtable, TypeNodeSlot, chunk, instance, context, objectNode);
    if (typeNode != nullptr)
    {
        // The agent learns its chunk and its spawn's starter, and its ID when it keeps a persistent flag of a layout's instance
        Agent* agent = reinterpret_cast<ObjectNode*>(objectNode)->agent;
        PropertyHolder* agentProperties = agent->properties;
        agent->chunkIndex = chunk->index;
        agent->spawnScript = static_cast<u16>(instance->spawnScript);
        if (factory->flags.givesFlagSlots != 0 && agentProperties->state.persistentFlag != 0)
        {
            agent->id = chunk->NextFlagSlot();
        }

        RegisterNode(context, AttachNode, typeNode);
    }

    if (properties->state.tracksMovement != 0)
    {
        GameNode* movement = ConstructMovementNode(MemoryAllocate(sizeof(MovementNode)));
        if (movement != nullptr)
        {
            RegisterNode(context, AttachNode, movement);
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
    CallVirtual<u32>(context, context->vtable, InstanceContext::ReleaseSlot);
    return standIn;
}

namespace
{
InstanceContext* MakeTriggerContext(InstanceFactory* factory, ChunkEntry* chunk, LayoutTrigger* trigger, u32 nodeSlot)
{
    ChunkData* chunkData = DataOf(chunk);
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

    constexpr u32 MakeHull = 1;
    constexpr u32 OwnBoxToo = 1;
    Vector4 min = {-trigger->scale.x, -trigger->scale.y, -trigger->scale.z, 1.0f};
    SetCollisionBox(&context->collision, &min, &trigger->scale, MakeHull, OwnBoxToo);
    context->flags.value = (context->flags.value | factory->creationFlags) & ~factory->clearedFlags;
    RegisterNode(context, AttachNode, node);
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

    // The agent object nodes of pickups of subtypes 16 and 17 (0xD0 bytes) and of projectiles (0xF0 bytes)
    GameNode* ConstructPickupNode(void* node, ChunkEntry* chunk) RETAIL(InitInstanceNodeType1);
    GameNode* ConstructProjectileNode(void* node) RETAIL(InitInstanceNodeType8);

    extern const GccVTableEntry g_ProjectileNodeVTable[] RETAIL(UnkNode_0x14_Methods);
}

namespace
{
// The objects' subtypes: pickups of subtypes 16 and 17 have nodes of their own, and 16's have no properties
constexpr u32 PickupNodeSubtype = 0x10;
constexpr u32 PickupNodeSubtypeEnd = 0x12;
// The room the factory's list of templates starts with, and grows by
constexpr u32 TemplatesRoom = 10;
// The activators a trigger's node tells (a bit per object type) made the kinds of their agents' nodes (and type 9's, which no
// object is, kind 0x15's)
constexpr u32 OnlyCharacterActivator = 1u << GameObject::TypeCharacter;
constexpr u32 ActivatorKinds[][2] = {{1u << GameObject::TypeCharacter, 1u << NodeCharacter},
                                     {1u << GameObject::TypeCrate, 1u << NodeCrate},
                                     {1u << GameObject::TypeCreature, 1u << NodeCreature},
                                     {1u << GameObject::TypePickup, 1u << NodePickup},
                                     {1u << GameObject::TypeGenericObject, 1u << NodeGenericObject},
                                     {1u << GameObject::TypeGrabbable, 1u << NodeGrabbable},
                                     {1u << GameObject::TypeGraple, 1u << NodeGraple},
                                     {1u << GameObject::TypePayGate, 1u << NodePayGate},
                                     {1u << GameObject::TypeProjectile, 1u << NodeProjectile},
                                     {1u << GameObject::TypeCount, 1u << NodeUnusedObjectType}};
// ObjectNode::Construct's: whether the node has waypoints (and a word it doesn't read)
constexpr u32 HasWaypoints = 1;
constexpr u32 NoWaypoints = 0;
// The size of a projectile's node (game/projectiles.cpp's ProjectileObjectNode)
constexpr u32 ProjectileNodeSize = 0xF0;
// The instance's integer property giving its object and model nodes their near distance: 0 and 0xFF stand for 0 and any
// (0xFFFF), others are squared
constexpr u32 NearDistanceProperty = 1;
constexpr u32 NearDistanceNone = 0;
constexpr u32 NearDistanceAny = 0xFF;

static_assert(offsetof(ChunkData, clocks) == 0x154);
static_assert(offsetof(GameProgress, characters) == 0x80);
static_assert(offsetof(ObjectInstance, properties) == 0x4C);

// A pickup's object node (pickups.cpp's), as far as the stand-in sets it: the time its wait starts
struct PickupNodeTimes : GameNode
{
    u8 objectNode[0xC0 - sizeof(GameNode)];
    u32 waitStart;
};

// The clocks of a chunk's instances (the game's when it has none); retail reads them through no chunk data too (the word at
// 0x154)
TimeClock* ChunkClocks(const ChunkData* chunkData)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(chunkData) + offsetof(ChunkData, clocks);
    auto* clocks = *reinterpret_cast<TimeClock* const*>(address);
    return clocks != nullptr ? clocks : G_GameClockController->clocks;
}

// A character's handle among the progress's as retail indexes them: character 6 (none) is the first checkpoint's word
Reference** CharacterHandle(GameProgress* progress, s32 character)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(progress->characters);
    address += static_cast<u32>(character) * sizeof(Reference*);
    return reinterpret_cast<Reference**>(address);
}

// An agent of a type made with its node: the holder of its class (the properties copied in), its part, the creator, the
// factory's subtype given to the holder, the agent and its node
template <typename Holder, typename Part, typename AgentType>
AgentNode* MakeAgent(InstanceFactory* factory, ChunkEntry* chunk, ObjectInstance* instance, InstanceContext* context,
                     InstanceCreator* creator, Holder* (*makeHolder)(Holder*, PropertyList*), PropertyList* list,
                     Part* (*makePart)(Part*), AgentType* (*makeAgent)(AgentType*, InstanceCreator*, PropertyHolder*, AgentPart*),
                     AgentNode* (*makeNode)(AgentNode*, Agent*))
{
    Holder* holder = makeHolder(static_cast<Holder*>(MemoryAllocate(sizeof(Holder))), list);
    Part* part = makePart(static_cast<Part*>(MemoryAllocate(sizeof(Part))));
    InstanceCreator::Construct(creator, instance, factory->resources, context);
    CallVirtual<void>(factory, factory->vtable, FactorySubtypeSlot, chunk, static_cast<PropertyHolder*>(holder));
    Agent* agent = makeAgent(static_cast<AgentType*>(MemoryAllocate(sizeof(AgentType))), creator, holder, part);
    return makeNode(static_cast<AgentNode*>(MemoryAllocate(sizeof(AgentNode))), agent);
}
}

InstanceCreator* InstanceCreator::Construct(InstanceCreator* creator, ObjectInstance* instance, GameResources* resources,
                                            InstanceContext* context)
{
    creator->context = context;
    creator->instance = instance;
    u16 id = static_cast<u16>(instance->objectId);
    creator->object = id != NoObjectId ? static_cast<GameObject*>(resources->objects->items[id & ResourceIndexMask]) : nullptr;
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
    return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(sizeof(ObjectNode))), chunk, HasWaypoints, 1);
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
    ChunkData* chunkData = DataOf(chunk);
    TriggerNode* node = ConstructMessageTriggerNode(MemoryAllocate(sizeof(MessageTriggerNode)), chunkData, trigger);
    if (trigger->header.notPolled != 0)
    {
        node->bits.neverPolled = 1;
    }

    return node;
}

CameraNode* BaseFactoryCameraNode(InstanceFactory*, ChunkEntry* chunk, CameraTrigger* camera)
{
    ChunkData* chunkData = DataOf(chunk);
    return CameraNode::Construct(static_cast<CameraNode*>(MemoryAllocate(sizeof(CameraNode))), chunkData, camera);
}

// In the holder's first integer, or the extras' when the class keeps none
void GiveFactorySubtype(InstanceFactory* factory, ChunkEntry*, PropertyHolder* holder)
{
    u32 value = factory->flags.subtype;
    if (value == InstanceFactoryFlags::NoSubtype)
    {
        return;
    }

    u32 count = CallVirtual<u32>(holder, holder->vtable, PropertyHolder::IntCountSlot);
    if (count != 0)
    {
        *CallVirtual<s32*>(holder, holder->vtable, PropertyHolder::IntWriteSlot, 0u) = static_cast<s32>(value);
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
    // (As if it were in a scenery cell drawn this frame)
    context->flags.inDrawnCell = 1;
    context->flags.value |= factory->creationFlags;
    context->flags.value &= ~factory->clearedFlags;
    if (factory->flags.givesIds != 0 && context->id == -1)
    {
        g_InstanceIds->Add(context);
    }

    if (properties->state.carriesRiders != 0)
    {
        context->flags.carriesRiders = 1;
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
    ObjectHeader header = object->header;
    PropertyList* list = factory->flags.instanceProperties != 0 ? instance->properties : object->properties;
    InstanceCreator creator;
    AgentNode* node = nullptr;
    switch (header.type)
    {
    case GameObject::TypeCrate:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCratePropertyHolder, list, CratePart::Construct,
                         CrateAgent::Construct, ConstructCrateNode);
        context->clockIndex = ObjectClock;
        context->flags.physicsBody = 1;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeCharacter:
    {
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCharacterPropertyHolder, list,
                         CharacterPart::Construct, CharacterAgent::Construct, ConstructCharacterNode);
        auto* agentNode = static_cast<ObjectNodeBase*>(GetGameNode(&context->nodes, NodeObject));
        agentNode->flags.pinned = 1;
        context->clockIndex = CharacterClock;
        creator.Destroy(DestroyOnly);
        break;
    }
    case GameObject::TypePickup:
        if (header.subtype == PickupNodeSubtype)
        {
            list = nullptr;
        }

        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructPickupPropertyHolder, list, PickupPart::Construct,
                         PickupAgent::Construct, ConstructPickupAgentNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeCreature:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructCreaturePropertyHolder, list,
                         CreaturePart::Construct, CreatureAgent::Construct, ConstructCreatureNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeGenericObject:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGenericObjectPropertyHolder, list,
                         GenericObjectPart::Construct, GenericObjectAgent::Construct, ConstructGenericObjectNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeGrabbable:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGrabbablePropertyHolder, list,
                         GrabbablePart::Construct, GrabbableAgent::Construct, ConstructGrabbableNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypePayGate:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructPayGatePropertyHolder, list,
                         PayGatePart::Construct, PayGateAgent::Construct, ConstructPayGateNode);
        context->clockIndex = ObjectClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeGraple:
        node = MakeAgent(factory, chunk, instance, context, &creator, ConstructGraplePropertyHolder, list, GraplePart::Construct,
                         GrapleAgent::Construct, ConstructGrapleNode);
        context->clockIndex = CharacterClock;
        creator.Destroy(DestroyOnly);
        break;
    case GameObject::TypeProjectile:
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
        CallVirtual<void>(factory, factory->vtable, FactorySubtypeSlot, chunk, static_cast<PropertyHolder*>(holder));
        auto* basic = static_cast<BasicAgent*>(MemoryAllocate(sizeof(BasicAgent)));
        Agent* agent = BasicAgent::Construct(basic, &creator, holder, part);
        agent->vtable = g_ProjectileAgentVTable;
        node = ConstructAgentNode(static_cast<AgentNode*>(MemoryAllocate(sizeof(AgentNode))), agent);
        context->clockIndex = ObjectClock;
        node->vtable = g_ProjectileNodeVTable;
        creator.Destroy(DestroyOnly);
        break;
    }
    }

    // A type without an agent tells the object node what's 0x18 bytes into nothing
    CallVirtual<void>(objectNode, objectNode->vtable, ObjectNode::SetAgentSlot, node->agent);
    return node;
}

GameNode* MakeAgentObjectNode(InstanceFactory*, ChunkEntry* chunk, GameObject* object)
{
    ObjectHeader header = object->header;
    u32 subtype = header.subtype;
    switch (header.type)
    {
    case GameObject::TypeCharacter:
    case GameObject::TypeCreature:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(sizeof(ObjectNode))), chunk, HasWaypoints, 1);
    case GameObject::TypePickup:
        if (subtype >= PickupNodeSubtype && subtype < PickupNodeSubtypeEnd)
        {
            return ConstructPickupNode(MemoryAllocate(sizeof(PickupObjectNode)), chunk);
        }

        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(sizeof(ObjectNode))), chunk, NoWaypoints, 0);
    case GameObject::TypeGenericObject:
    case GameObject::TypeGrabbable:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(sizeof(ObjectNode))), chunk, HasWaypoints, 0);
    case GameObject::TypeCrate:
    case GameObject::TypePayGate:
    case GameObject::TypeGraple:
        return ObjectNode::Construct(static_cast<ObjectNode*>(MemoryAllocate(sizeof(ObjectNode))), chunk, NoWaypoints, 0);
    case GameObject::TypeProjectile:
        return ConstructProjectileNode(MemoryAllocate(ProjectileNodeSize));
    default:
        return nullptr;
    }
}

// A trigger the character alone activates is never polled unless it tells what leaves (the characters inside check it, the player
// only); one nothing activates is never polled the same way, any character checking it; the others are told the events of their
// activators' agents
TriggerNode* GameFactoryTriggerNode(InstanceFactory* factory, ChunkEntry* chunk, LayoutTrigger* trigger, InstanceContext* context)
{
    auto* node = static_cast<MessageTriggerNode*>(BaseFactoryTriggerNode(factory, chunk, trigger));
    u32 activators = trigger->activators;
    if (activators == OnlyCharacterActivator && node->bits.tellsExit == 0)
    {
        node->bits.neverPolled = 1;
        node->messageBits.anyCharacter = 0;
    }
    else if (activators == 0)
    {
        if (node->bits.tellsExit == 0)
        {
            node->bits.neverPolled = 1;
        }

        node->messageBits.anyCharacter = 1;
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
        node->bits.neverPolled = 1;
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

// A playable character's instance already in a chunk stands in for a new one of the same character (given a place for the new
// one's chunk) when it has places; otherwise the new one becomes the progress's character, and whichever of the two it is gets
// places, a controls' node and a follow node when it has none
InstanceContext* GameFactoryStandIn(InstanceFactory* factory, ChunkEntry* chunk, GameObject* object, ObjectInstance* instance,
                                    InstanceContext* context)
{
    auto* characterNode = static_cast<AgentNode*>(GetGameNode(&context->nodes, NodeCharacter));
    ChunkData* chunkData = DataOf(chunk);
    if (characterNode == nullptr)
    {
        u32 subtype = object->header.subtype;
        if (subtype >= PickupNodeSubtype && subtype < PickupNodeSubtypeEnd)
        {
            TimeClock* clock = &ChunkClocks(chunkData)[context->clockIndex];
            ObjectPlace* place = context->place;
            place->SyncPosition();
            Vector4 position = place->position;
            Matrix4x4 matrix;
            InitIdentityMatrix(&matrix);
            *RowOf(&matrix, 3) = position;
            SetCollisionMatrix(&context->collision, &matrix);
            auto* node = static_cast<PickupNodeTimes*>(GetGameNode(&context->nodes, NodeObject));
            node->waitStart = clock->time;
        }

        switch (object->header.type)
        {
        case GameObject::TypeCrate:
        case GameObject::TypeGenericObject:
        case GameObject::TypePayGate:
            context->collision.bits.stopsBodies = 1;
            break;
        default:
            break;
        }
    }
    else
    {
        auto* objectNode = static_cast<ObjectNodeBase*>(GetGameNode(&context->nodes, NodeObject));
        s32 character = characterNode->agent->properties->GetInt(0);
        GameProgress* progress = &g_AgentsGameController->progress;
        InstanceContext* kept = nullptr;
        if (character != static_cast<s32>(GameProgress::NoCharacter))
        {
            Reference* reference = *CharacterHandle(progress, character);
            kept = reference != nullptr ? static_cast<InstanceContext*>(reference->object) : nullptr;
        }

        ChunkData* keptChunk = kept != nullptr ? kept->chunk : nullptr;
        objectNode->information.flags.stays = 0;
        if (keptChunk == nullptr)
        {
            kept = context;
            AssignReference(CharacterHandle(progress, character), context);
        }

        if (kept->places != nullptr)
        {
            AddInstancePlace(kept->places, context, chunkData);
            return kept;
        }

        void* controls = GetGameNode(&kept->nodes, NodeControls);
        void* follow = GetGameNode(&kept->nodes, NodeFollow);
        MakeInstancePlaces(kept, chunkData);
        if (controls == nullptr)
        {
            RegisterNode(kept, AttachNode, ConstructControlsNode(MemoryAllocate(sizeof(ControlsNode))));
        }

        if (follow == nullptr)
        {
            RegisterNode(kept, AttachNode,
                         ConstructFollowNode(static_cast<FollowNode*>(MemoryAllocate(sizeof(FollowNode))), chunk));
        }
    }

    MarkThinAndCopySurfaces(&context->collision);
    auto* objectNode = static_cast<GameNode*>(GetGameNode(&context->nodes, NodeObject));
    auto* modelNode = static_cast<GameNode*>(GetGameNode(&context->nodes, NodeModel));
    u32 given = instance->properties->IntAt(NearDistanceProperty) & 0xFF;
    u16 distance = given == NearDistanceNone  ? 0
                   : given == NearDistanceAny ? GameNode::AnyNearDistance
                                              : static_cast<u16>(given * given);
    if (objectNode != nullptr)
    {
        objectNode->nearDistance = distance;
    }

    if (modelNode != nullptr)
    {
        modelNode->nearDistance = distance;
    }

    return BaseFactoryStandIn(factory, chunk, object, instance, context);
}

InstanceFactory* ConstructBaseFactory(InstanceFactory* factory, GameResources* resources)
{
    factory->vtable = g_BaseFactoryVTable;
    factory->creationFlags = 0;
    factory->clearedFlags = 0;
    factory->unused0C = UndefinedId;
    factory->resources = resources;
    factory->templates.growth = TemplatesRoom;
    factory->templates.capacity = TemplatesRoom;
    factory->templates.count = 0;
    factory->templates.data = static_cast<InstanceTemplate**>(MemoryAllocate2(TemplatesRoom * sizeof(InstanceTemplate*)));
    factory->flags.value = 0;
    factory->flags.subtype = InstanceFactoryFlags::NoSubtype;
    factory->SetUnused4();
    return factory;
}

InstanceFactory* ConstructGameFactory(InstanceFactory* factory, GameResources* resources)
{
    ConstructBaseFactory(factory, resources);
    factory->vtable = g_GameFactoryVTable;
    return factory;
}

void InstanceFactory::ClearGivesFlagSlots()
{
    flags.givesFlagSlots = 0;
}

void InstanceFactory::ClearUnused4()
{
    flags.unused4 = 0;
}

void ObjectInstance::UseProperties(PropertyList* list)
{
    if (ownsProperties != 0 && properties != nullptr)
    {
        CallVirtual<void>(properties, properties->vtable, PropertyList::DestroySlot, u32{DestroyAndFree});
    }

    properties = list;
    ownsProperties = 0;
}

InstanceContext* CreateInstance(InstanceFactory* factory, ChunkEntry* chunk, u32 objectId, const Vector4* position, const s32* angles)
{
    ResourceTable* objects = factory->resources->objects;
    u16 id = static_cast<u16>(objectId);
    auto* object = id != NoObjectId ? static_cast<GameObject*>(objects->items[id & ResourceIndexMask]) : nullptr;
    ObjectInstance instance;
    ObjectInstance::ConstructEmpty(&instance);
    instance.objectId = static_cast<s16>(objectId);
    instance.UseProperties(object->properties);
    instance.position = *position;
    instance.rotation[0].raw = static_cast<u32>(angles[0]);
    instance.rotation[1].raw = static_cast<u32>(angles[1]);
    instance.rotation[2].raw = static_cast<u32>(angles[2]);
    InstanceContext* context = CreateInstanceContext(factory, chunk, &instance);
    instance.Destroy(DestroyOnly);
    return context;
}

InstanceContext* CreateInstanceFrom(InstanceFactory* factory, ChunkEntry* chunk, InstanceContext* source, u32 objectId,
                                    const Vector4* position, const s32* angles, u32 useObjectProperties)
{
    GameObject* object = ObjectOf(factory, static_cast<u16>(objectId));
    PropertyList* objectProperties = object->properties;
    PropertyHolder* holder = static_cast<ObjectNodeBase*>(GetGameNode(&source->nodes, NodeObject))->PacketProperties();
    ObjectInstance instance;
    ObjectInstance::ConstructEmpty(&instance);
    instance.objectId = static_cast<s16>(objectId);
    InstanceContext* context;
    if (useObjectProperties != 0 && objectProperties != nullptr)
    {
        instance.UseProperties(objectProperties);
        instance.position = *position;
        instance.rotation[0].raw = static_cast<u32>(angles[0]);
        instance.rotation[1].raw = static_cast<u32>(angles[1]);
        instance.rotation[2].raw = static_cast<u32>(angles[2]);
        context = CreateInstanceContext(factory, chunk, &instance);
    }
    else
    {
        // A list of the source's values, its class's and its extras
        u32 taggedCount = holder->TaggedCount();
        u32 floatCount = holder->FloatCount();
        u32 intCount = holder->IntCount();
        u32 holderClass = CallVirtual<u32>(holder, holder->vtable, PropertyHolder::ClassSlot);
        PropertyExtras* extras = holder->extras;
        if (extras != nullptr)
        {
            intCount += extras->counts[IntProperties];
            taggedCount += extras->counts[TaggedProperties];
            floatCount += extras->counts[FloatProperties];
        }

        PropertyList list;
        PropertyList::Construct(&list, taggedCount, floatCount, intCount, holderClass);
        u8 tagged = list.counts[TaggedProperties];
        u8 floats = list.counts[FloatProperties];
        u8 ints = list.counts[IntProperties];
        for (u32 index = 0; index < tagged; index++)
        {
            TaggedValue value;
            PropertyHolder::GetTagged(&value, holder, index);
            list.SetTagged(index, &value);
        }

        for (u32 index = 0; index < floats; index++)
        {
            list.SetFloat(index, holder->GetFloat(index));
        }

        for (u32 index = 0; index < ints; index++)
        {
            list.SetInt(index, holder->GetInt(index));
        }

        list.state = holder->state;
        instance.UseProperties(&list);
        instance.position = *position;
        instance.rotation[0].raw = static_cast<u32>(angles[0]);
        instance.rotation[1].raw = static_cast<u32>(angles[1]);
        instance.rotation[2].raw = static_cast<u32>(angles[2]);
        context = CreateInstanceContext(factory, chunk, &instance);
        list.Destroy(DestroyOnly);
    }

    instance.Destroy(DestroyOnly);
    return context;
}
