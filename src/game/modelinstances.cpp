#include "game/instancefactory.h"

#include "game/camerarig.h"
#include "game/chunkdata.h"
#include "game/controllers.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/resources.h"

// The model nodes made of a model's ID (an object's first model, a cutscene's models), and the instances made outside the
// layouts: a cutscene's models and the camera's

extern "C"
{
    // A halfword copied (the retail code copies a model's ID through it)
    u16* CopyHalfword(u16* to, const u16* from) RETAIL(MoveShortFromS2toS1);
    // The camera instances' hull (made the first time)
    extern CollisionHull* g_CameraHull RETAIL(D_0030A13C);
}

namespace
{
constexpr u16 NoModel = 0xFFFF;
constexpr u16 ModelIdMask = 0x7FFF;
// An object's header's first word: its exit points (bits 0-5) and camera joints (bits 6-11)
constexpr u32 JointsMask = 0x3F;
constexpr u32 CameraJointsShift = 6;
// A node registered attached to its instance
constexpr u32 AttachToInstance = 1;
// How far a camera instance's box goes either way
constexpr f32 CameraBoxReach = 0x1.99999Ap-4f;

// The OGI of a model's ID (none for 0xFFFF)
GameOGI* ModelOgi(u16 model)
{
    ResourceTable* models = G_GameResourcesObjectPointer->models;
    u16 id;
    CopyHalfword(&id, &model);
    return id != NoModel ? static_cast<GameOGI*>(models->items[id & ModelIdMask]) : nullptr;
}

ModelNode* NewModelNode()
{
    return ModelNode::Construct(static_cast<ModelNode*>(MemoryAllocate(sizeof(ModelNode))));
}

// A new instance at a matrix's position and turn
void PlaceInstance(InstanceContext* instance, const Matrix4x4* matrix)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(RowOf(matrix, 3)))
    {
        QueueObject(instance);
    }

    place = instance->place;
    place->SyncRotation();
    Vector4 rotation;
    GetRotationVec(&rotation, matrix);
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }
}

InstanceContext* NewInstance()
{
    return InstanceContext::Construct(static_cast<InstanceContext*>(MemoryAllocate(sizeof(InstanceContext))));
}
}

GameNode* MakeObjectModelNode(GameObject* object)
{
    u16 model;
    GetObjectModelId(&model, object, 0);
    GameOGI* ogi = ModelOgi(model);
    u32 header = object->header[0];
    ModelNode* node = NewModelNode();
    node->SetOgi(ogi, header >> CameraJointsShift & JointsMask, header & JointsMask);
    return node;
}

ModelNode* MakePlainModelNode(u16 model)
{
    GameOGI* ogi = ModelOgi(model);
    ModelNode* node = NewModelNode();
    node->SetOgi(ogi, 0, 0);
    return node;
}

InstanceContext* MakeModelInstance(ChunkData* chunk, u16 model, const Matrix4x4* matrix)
{
    InstanceContext* instance = NewInstance();
    if (matrix != nullptr)
    {
        PlaceInstance(instance, matrix);
    }

    ModelNode* node = MakePlainModelNode(model);
    instance->flags |= ReferencedObject::FlagVisible;
    RegisterNode(instance, AttachToInstance, node);
    node->AttachCollision();
    ChunkData::AddInstance(chunk, instance);
    return instance;
}

InstanceContext* MakeCameraInstance(ChunkData* chunk, const Matrix4x4* matrix)
{
    InstanceContext* instance = NewInstance();
    s32 fov = g_DefaultFov;
    if (matrix != nullptr)
    {
        PlaceInstance(instance, matrix);
    }

    Box box = {{-CameraBoxReach, -CameraBoxReach, -CameraBoxReach, 1.0f}, {CameraBoxReach, CameraBoxReach, CameraBoxReach, 1.0f}};
    if (g_CameraHull == nullptr)
    {
        g_CameraHull = HullConstruct(static_cast<CollisionHull*>(MemoryAllocate(sizeof(CollisionHull))));
        BuildBoxHull(g_CameraHull, &box.min, &box.max);
    }

    instance->collision.givenHull = g_CameraHull;
    instance->collision.ownBox = box;
    auto* lens = CameraLensNode::Construct(static_cast<CameraLensNode*>(MemoryAllocate(sizeof(CameraLensNode))));
    lens->bits |= CameraLensNode::BitProjectionChanged;
    lens->fov = fov;
    RegisterNode(instance, AttachToInstance, lens);
    ChunkData::AddInstance(chunk, instance);
    return instance;
}
