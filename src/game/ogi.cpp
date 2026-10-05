#include "game/animation.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/context.h"
#include "game/instances.h"
#include "game/lights.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/place.h"
#include "platform/graphics.h"
#include "platform/math.h"
#include "retail/libc.h"

namespace
{
Platform::Graphics::ModelLights LightsAt(const ChunkLights* lights)
{
    return {lights->directions, lights->colours, &lights->ambient};
}

// The classes of the models' items: a model node, an OGI and an animation
constexpr u32 OgiClassId = 0x1407;
constexpr u32 AnimationClassId = 0x1702;
}

extern "C"
{
    // The items' builders' base (BuilderBaseFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);

    // The models' items' builder (a vtable alone, D_002F6268, the game context's): an item of a class made (none for another),
    // and its destructor
    void* MakeModelItem(void* builder, u32 classId) RETAIL(FUN_001a13d8);
    void DestroyModelItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_001a13a8);
}

void* MakeModelItem(void*, u32 classId)
{
    switch (classId)
    {
    case ModelNode::ClassId:
        return ModelNode::Construct(static_cast<ModelNode*>(MemoryAllocate(sizeof(ModelNode))));
    case AnimationClassId:
        return InitAnimation(static_cast<GameAnimation*>(MemoryAllocate(sizeof(GameAnimation))));
    case OgiClassId:
        return InitOGI(static_cast<GameOGI*>(MemoryAllocate(sizeof(GameOGI))));
    default:
        return nullptr;
    }
}

void DestroyModelItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void DrawOgi(GameOGI* ogi, const Matrix4x4* matrix, JointMatrices* joints, const BlendShapeWeights* blendShapes,
             const ChunkLights* lights, u32 mode)
{
    constexpr s32 MostShapes = 3;
    Matrix4x4* matrices = joints->start;
    if (matrices != nullptr && joints->frame + 1 < g_RenderedFrames)
    {
        matrices = nullptr;
    }

    if (matrices == nullptr)
    {
        return;
    }

    Platform::Graphics::ModelLights modelLights = LightsAt(lights);
    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        Matrix4x4 placed;
        VuMultiplyMatrices(&matrices[ogi->rigidModelJoints[index]], matrix, &placed);
        Platform::Graphics::DrawRigidModel(ogi->rigidModels[index], &placed, modelLights, mode);
    }

    if (ogi->hasSkin == 0 && ogi->hasBlendSkin == 0)
    {
        return;
    }

    for (u32 index = 0; index < ogi->jointCount; index++)
    {
        Platform::Math::MultiplyByParent(&ogi->jointMatrices[index], &matrices[index], &matrices[index]);
    }

    if (ogi->hasSkin != 0)
    {
        Platform::Graphics::DrawSkin(ogi->skin, matrices, ogi->jointCount, matrix, modelLights, mode);
    }

    if (ogi->hasBlendSkin == 0)
    {
        return;
    }

    s32 shapes[MostShapes] = {-1, -1, -1};
    f32 weights[MostShapes];
    s32 shapeCount = 0;
    for (u32 shape = 0; shape < blendShapes->count; shape++)
    {
        f32 weight = blendShapes->next[shape];
        if (weight != 0.0f && shapeCount < MostShapes)
        {
            shapes[shapeCount] = shape;
            weights[shapeCount] = weight;
            shapeCount++;
        }
    }

    Platform::Graphics::DrawBlendSkin(ogi->blendSkin, matrices, ogi->jointCount, matrix, modelLights, weights, shapes,
                                      shapeCount, mode);
}

void DrawOgiRigidModels(GameOGI* ogi, const Matrix4x4* matrix, const ChunkLights* lights, u32 mode)
{
    Platform::Graphics::ModelLights modelLights = LightsAt(lights);
    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        Platform::Graphics::DrawRigidModel(ogi->rigidModels[index], matrix, modelLights, mode);
    }
}

namespace
{
// The instance's seen stamp past the node's own (none: 0), past the update rate's grace: 0 within it
u32 StampsUnseen(const ModelNode* node)
{
    u32 seen = node->owner->seen;
    u32 since = node->nearDistance;
    if (since == GameNode::AnyNearDistance || !(since < seen))
    {
        return 0;
    }

    u32 gap = seen - since;
    u32 grace = g_ModelUpdateRate.grace;
    return grace < gap ? gap - grace : 0;
}
}

void RestartOgiAnimations(GameOGI* ogi)
{
    if (ogi->skin != nullptr)
    {
        Platform::Graphics::RestartAnimations(ogi->skin);
    }

    if (ogi->blendSkin != nullptr)
    {
        Platform::Graphics::RestartAnimations(ogi->blendSkin);
    }

    for (u32 index = 0; index < ogi->rigidModelCount; index++)
    {
        Platform::Graphics::RestartAnimations(ogi->rigidModels[index]);
    }
}

ModelNode* ModelNode::Construct(ModelNode* node)
{
    GameNode::Construct(node);
    node->ogi = nullptr;
    node->drawnOgi = nullptr;
    node->animator = nullptr;
    node->drawnAnimator = nullptr;
    node->lighting = nullptr;
    node->vtable = g_ModelNodeVTable;
    RetailLibc::MemorySet(&node->bits, 0, sizeof(node->bits));
    return node;
}

void ModelNode::Destroy(u32 destroyFlags)
{
    OgiAnimator* own = animator;
    vtable = g_ModelNodeVTable;
    if (own != nullptr)
    {
        DeleteOgiAnimator(own);
    }

    if (lighting != nullptr)
    {
        CallVirtual<void>(lighting, lighting->vtable, Light::SlotDestroy, u32{DestroyAndFree});
    }

    GameNode::Destroy(destroyFlags);
}

void ModelNode::SetOwner(InstanceContext* instance)
{
    if (animator != nullptr)
    {
        SetAnimatorPlace(animator, instance->place);
    }

    AttachCollision();
    GameNode::SetOwner(instance);
}

u32 ModelNode::Kind()
{
    return NodeModel;
}

void ModelNode::Step(TimeClock*, u32)
{
    if (animator != nullptr)
    {
        ReleaseAnimatorOgi(animator);
    }

    ogi = nullptr;
    drawnOgi = nullptr;
    drawnAnimator = nullptr;
    RetailLibc::MemorySet(&bits, 0, sizeof(bits));
}

u32 ModelNode::Update(TimeClock* clock)
{
    OgiAnimator* own = animator;
    drawnAnimator = own;
    drawnOgi = ogi;
    if (ogi == nullptr)
    {
        bits.matricesMade = 0;
        return GameNode::Update(clock);
    }

    InstanceContext* instance = owner;
    u32 inDrawnCell = instance->flags.inDrawnCell;
    u32 visible = instance->flags.visible;
    if (own == nullptr)
    {
        bits.unseen = StampsUnseen(this);
        // Drawn only while its instance is seen, in a drawn cell and not unseen for too long
        if (visible == 0 || !(bits.unseen < g_ModelUpdateRate.cutoff) || inDrawnCell == 0)
        {
            drawnOgi = nullptr;
        }

        return GameNode::Update(clock);
    }

    u8 detail = own->matrices.detail;
    bits.unseen = StampsUnseen(this);
    u32 unseen = bits.unseen;
    bool recent = unseen < g_ModelUpdateRate.cutoff;
    u32 drawn = 0;
    if (visible != 0 && recent)
    {
        drawn = inDrawnCell;
    }

    if (bits.ogiChanged != 0 || bits.alwaysAnimated != 0 || detail != 0)
    {
        u32 made = AnimateOgi(drawnAnimator, drawnOgi, clock, 1, 0);
        bits.matricesMade = bits.matricesMade | made;
        bits.ogiChanged = 0;
    }
    else if (!recent)
    {
        bits.matricesMade = ForgetJointMatrices(drawnAnimator);
    }
    else
    {
        // Every 2^n frames, n growing with the stamps it went unseen, staggered by the node's address (the bits keep 24 bits of the
        // stamps, never UpdateRate::NoCount)
        bool now = true;
        if (bits.matricesMade != 0 && unseen != 0)
        {
            u32 mask = UpdateRate::RarestMask;
            if (unseen != UpdateRate::NoCount)
            {
                s32 power = static_cast<s32>(g_ModelUpdateRate.slope * static_cast<f32>(unseen)) + 1;
                mask = (1u << (power & ShiftMask)) - 1;
            }

            now = ((g_RenderedFrames + (reinterpret_cast<u32>(this) >> 8)) & mask) == 0;
        }

        if (now)
        {
            u32 made = AnimateOgi(drawnAnimator, drawnOgi, clock, drawn, 0);
            bits.matricesMade = bits.matricesMade | made;
        }
        else
        {
            u32 made = ReuseJointMatrices(drawnAnimator, drawnOgi, drawn, 0);
            GameNode::flags.keepsTime = 1;
            bits.matricesMade = bits.matricesMade | made;
        }
    }

    if (drawn == 0)
    {
        drawnOgi = nullptr;
        return GameNode::Update(clock);
    }

    QueueObject(owner);
    return GameNode::Update(clock);
}

u32 ModelNode::GetClassId()
{
    return ClassId;
}

void ModelNode::SetOgi(GameOGI* newOgi, u32 reactJoints, u32 exitPoints)
{
    if (ogi != newOgi)
    {
        ogi = newOgi;
        bits.ogiChanged = 1;
        if (newOgi == nullptr || (newOgi->jointCount == 1 && newOgi->exitPointCount == 0))
        {
            OgiAnimator* own = animator;
            if (own != nullptr)
            {
                DeleteOgiAnimator(own);
                animator = nullptr;
            }
        }
        else if (animator == nullptr)
        {
            animator = InitOgiAnimatorService(static_cast<OgiAnimator*>(MemoryAllocate(sizeof(OgiAnimator))), ogi,
                                              reactJoints, exitPoints);
        }
        else
        {
            SetAnimatorOgi(animator, ogi, reactJoints, exitPoints);
        }

        AttachCollision();
    }

    if (newOgi != nullptr)
    {
        RestartOgiAnimations(newOgi);
    }
}

void ModelNode::AttachCollision()
{
    if (owner != nullptr)
    {
        SetCollisionOgi(&owner->collision, ogi);
    }
}

void ModelNode::SetSolid(u32 solid)
{
    bits.solid = solid;
    if (ogi != nullptr)
    {
        SetCollisionSolid(&owner->collision, bits.solid);
    }
}

void ModelNode::Draw(const Matrix4x4* matrix, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode)
{
    if (drawnOgi != nullptr)
    {
        GatherStrongestLights(lights, chunkMatrix, lighting);
        if (drawnAnimator == nullptr)
        {
            DrawOgiRigidModels(drawnOgi, matrix, lights, mode);
        }
        else
        {
            DrawOgi(drawnOgi, matrix, &drawnAnimator->matrices, &drawnAnimator->blendShapes, lights, mode);
        }

        drawnOgi = nullptr;
    }

    drawnAnimator = nullptr;
}

void DrawInstanceModel(InstanceContext* instance, const Matrix4x4* chunkMatrix, ChunkLights* lights, u32 mode)
{
    Matrix4x4 matrix = instance->place->matrix;
    Matrix4x4 placed;
    Platform::Math::MultiplyByParent(&matrix, chunkMatrix, &placed);
    auto* node = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
    if (lights != nullptr)
    {
        lights->position = *RowOf(&matrix, 3);
    }

    node->Draw(&placed, chunkMatrix, lights, mode);
}

void DrawQueuedInstances()
{
    constexpr u32 WhollyInView = 1;
    constexpr u32 Clipped = 2;
    // The clipped instances are queued from the array's end
    constexpr s32 LastSlot = sizeof(g_DrawnInstances) / sizeof(g_DrawnInstances[0]) - 1;
    if (g_DrawnInstanceCount == 0)
    {
        return;
    }

    Platform::Graphics::UseHelperPrograms(Platform::Graphics::StandardPrograms, true);
    // The chunk's matrix is kept while its instances follow each other (the first instance always has a chunk)
    ChunkData* chunk = nullptr;
    ChunkLights* lights = nullptr;
    Matrix4x4 chunkMatrix;
    auto draw = [&](InstanceContext* instance, u32 mode) {
        if (instance->chunk != chunk)
        {
            g_DrawnChunk = instance->chunk;
            chunk = instance->chunk;
            lights = chunk->lights;
            chunkMatrix = chunk->drawMatrix;
        }

        DrawInstanceModel(instance, &chunkMatrix, lights, mode);
    };

    for (s32 index = 0; index < g_DrawnInViewCount; index++)
    {
        draw(g_DrawnInstances[index], WhollyInView);
    }

    for (s32 index = LastSlot; g_DrawnClippedEnd < index; index--)
    {
        draw(g_DrawnInstances[index], Clipped);
    }
}
