#include "game/shadows.h"

#include "game/animation.h"
#include "game/chunkdata.h"
#include "game/graphicstables.h"
#include "game/lights.h"
#include "game/memory.h"
#include "game/place.h"
#include "game/resources.h"
#include "game/stream.h"
#include "game/string.h"
#include "platform/graphics.h"
#include "retail/libc.h"

extern "C"
{
    extern const GccVTableEntry g_ShadowNodeVTable[] RETAIL(D_002F9B90);
    // The development tools' shadow files: the materials' and models' packages, the folder the models and meshes are read from,
    // the meshes' names by kind (none: no mesh) and their extension
    extern const char g_ShadowMaterialsPackage[] RETAIL(D_002F9A38);
    extern const char g_ShadowModelsPackage[] RETAIL(D_002F9A60);
    extern const char g_ShadowModelsFolder[] RETAIL(D_002F9A88);
    extern const char* const g_ShadowMeshNames[8] RETAIL(D_002E78F8);
    extern const char g_MeshExtension[] RETAIL(D_00309D90);
    // The C library's character classes (bit 1 lower case)
    extern const u8 CasingTable[];
    // The graphics tables' and their readers' functions of the kinds the files have
    ModelTable::Entry* ModelFind(ModelTable* table, u32 id) RETAIL(FUN_001c74a0);
    s32 ModelInsert(ModelTable* table, RigidModelData* const* model, u32 id) RETAIL(FUN_001a38b8);
    MeshTable::Entry* MeshFind(MeshTable* table, u32 id) RETAIL(FUN_001c73d8);
    s32 MeshInsert(MeshTable* table, RigidModel* const* mesh, u32 id) RETAIL(FUN_001a4a10);
    RigidModelData* ModelConstruct(void* memory, u32 id) RETAIL(InitGameModel);
    void ModelRead(RigidModelData* model, Stream* stream) RETAIL(ReadGameModel);
    RigidModel* MeshConstruct(void* memory, u32 id) RETAIL(FUN_001c1e70);
    void MeshRead(RigidModel* mesh, Stream* stream) RETAIL(ReadRigidModel2);
    void MaterialReaderDestroy(GraphicsKindReader<MaterialKind>* reader, u32 flags) RETAIL(FUN_001a1ae8);
    void ModelReaderDestroy(GraphicsKindReader<ModelKind>* reader, u32 flags) RETAIL(FUN_001a1c88);
    extern const GccVTableEntry g_MaterialReaderVTable[] RETAIL(MaterialItem_Methods);
    extern const GccVTableEntry g_ModelReaderVTable[] RETAIL(ModelsItem_Methods);

    // A model and a mesh of the tables read from the development tools' file of its name in the table's folder, unless the table
    // has its ID: a reference taken either way
    RigidModelData* LoadShadowModel(ModelTable* table, const String* name, u32 id) RETAIL(FUN_001cc790);
    RigidModel* LoadShadowMesh(MeshTable* table, const String* name, u32 id) RETAIL(FUN_001cc940);
}

namespace
{
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// How faint a shadow may be and still be cast
constexpr f32 Faintest = Rounded(5e-05);
// A capsule is cast while its axis is no closer to the way to the light than this cosine
constexpr f32 MostAlong = Rounded(0.996);
// The cross product of the up axis and the shadow's axis is used while its length squared is above it
constexpr f32 Crossing = Rounded(0.03);
constexpr u32 MostJoints = 0x40;
// The lighting constants' direction shadows are cast from (above), its fourth word w
constexpr u32 ShadowDirection = 0x20;

void DeleteSlot(ShadowSlot* slot)
{
    if (slot == nullptr)
    {
        return;
    }

    ShadowShapes* shapes = slot->shapes;
    if (shapes != nullptr)
    {
        shapes->Clear();
        MemoryDeallocate2_(shapes);
    }

    MemoryDeallocate2_(slot);
}

// The bits of a distance: it and its square
u32 DistanceBits(u32 bits, f32 distance)
{
    bits = (bits & ~ShadowShapes::DistanceMask) | (static_cast<u32>(static_cast<s32>(distance)) & ShadowShapes::DistanceMask);
    u32 near = bits & ShadowShapes::DistanceMask;
    return (bits & 0xC00003FF) | (near * near) << ShadowShapes::SquaredShift;
}

void ConstructShapes(ShadowShapes* shapes, f32 distance, f32 strength)
{
    shapes->strength = strength;
    RetailLibc::MemorySet(&shapes->bits, 0, sizeof(shapes->bits));
    shapes->bits = DistanceBits(shapes->bits, distance);
}

// The way from a point to where the shadow is cast from and how far it is; the unit way is worked out and dropped by the circles'
// and the plain shapes' casts
f32 WayTo(const Vector4* from, const Vector4* point, Vector4* way)
{
    *way = *from;
    way->x = way->x - point->x;
    way->y = way->y - point->y;
    way->z = way->z - point->z;
    f32 length = __builtin_sqrtf((way->x * way->x + way->y * way->y) + way->z * way->z);
    f32 inverse = 1.0f / length;
    way->x = way->x * inverse;
    way->y = way->y * inverse;
    way->z = way->z * inverse;
    return length;
}

void DeleteCircles(ShadowCircle* circle, u32 destroyFlags);
void DeleteCapsules(ShadowCapsule* capsule, u32 destroyFlags);
}

extern "C"
{
    // The lists of circles and capsules deleted, the rest of the list first
    void DeleteShadowCapsules(ShadowCapsule* capsule, u32 destroyFlags) RETAIL(FUN_001cc0a0);
    void DeleteShadowCircles(ShadowCircle* circle, u32 destroyFlags) RETAIL(FUN_001ccc38);
}

namespace
{
void DeleteCircles(ShadowCircle* circle, u32 destroyFlags)
{
    DeleteShadowCircles(circle, destroyFlags);
}

void DeleteCapsules(ShadowCapsule* capsule, u32 destroyFlags)
{
    DeleteShadowCapsules(capsule, destroyFlags);
}
}

void DeleteShadowCapsules(ShadowCapsule* capsule, u32 destroyFlags)
{
    if (capsule->next != nullptr)
    {
        DeleteShadowCapsules(capsule->next, DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(capsule);
    }
}

void DeleteShadowCircles(ShadowCircle* circle, u32 destroyFlags)
{
    if (circle->next != nullptr)
    {
        DeleteShadowCircles(circle->next, DestroyAndFree);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(circle);
    }
}

ShadowCircle* ShadowCircle::Construct(f32 radius, f32 height, ShadowCircle* circle, u8 kind, u8 joint)
{
    circle->next = nullptr;
    circle->radius = radius;
    circle->height = height;
    RetailLibc::MemorySet(&circle->kind, 0, 4);
    circle->kind = kind;
    circle->joint = joint;
    circle->offset = g_DefaultBox.min;
    circle->offset.w = 1.0f;
    return circle;
}

ShadowCapsule* ShadowCapsule::Construct(f32 radius, ShadowCapsule* capsule, u8 kind, u8 joint, u8 secondJoint)
{
    capsule->next = nullptr;
    capsule->radius = radius;
    RetailLibc::MemorySet(&capsule->kind, 0, 4);
    capsule->kind = kind;
    capsule->joint = joint;
    capsule->secondJoint = secondJoint;
    capsule->offset = g_DefaultBox.min;
    capsule->offset.w = 1.0f;
    return capsule;
}

ShadowPlain* ShadowPlain::Construct(f32 width, f32 depth, ShadowPlain* plain, u8 kind)
{
    plain->width = width;
    plain->depth = depth;
    RetailLibc::MemorySet(&plain->kind, 0, 4);
    plain->kind = kind;
    plain->offset = g_DefaultBox.min;
    plain->offset.w = 1.0f;
    return plain;
}

ShadowPlain* ShadowPlain::ConstructSquare(f32 size, ShadowPlain* plain, u8 kind)
{
    plain->depth = size;
    plain->width = size;
    RetailLibc::MemorySet(&plain->kind, 0, 4);
    plain->kind = kind;
    plain->offset = g_DefaultBox.min;
    plain->offset.w = 1.0f;
    return plain;
}

ShadowShapes* ShadowShapes::Construct(f32 distance, f32 strength, ShadowShapes* shapes)
{
    shapes->joints = 0;
    shapes->plain = nullptr;
    shapes->circles = nullptr;
    shapes->capsules = nullptr;
    shapes->next = nullptr;
    ConstructShapes(shapes, distance, strength);
    return shapes;
}

ShadowShapes* ShadowShapes::ConstructPlain(f32 distance, f32 strength, ShadowShapes* shapes, ShadowPlain* plain)
{
    shapes->plain = plain;
    shapes->joints = 0;
    shapes->circles = nullptr;
    shapes->capsules = nullptr;
    shapes->next = nullptr;
    ConstructShapes(shapes, distance, strength);
    return shapes;
}

void ShadowShapes::Clear()
{
    MemoryDeallocate2_(plain);
    if (circles != nullptr)
    {
        DeleteCircles(circles, DestroyAndFree);
    }

    if (capsules != nullptr)
    {
        DeleteCapsules(capsules, DestroyAndFree);
    }

    if (next != nullptr)
    {
        next->Clear();
        MemoryDeallocate2_(next);
    }

    joints = 0;
    plain = nullptr;
    circles = nullptr;
    capsules = nullptr;
    next = nullptr;
}

void ShadowShapes::SetPlain(ShadowPlain* shape)
{
    plain = shape;
}

void ShadowShapes::AddCircle(ShadowCircle* circle)
{
    u8 joint = circle->joint;
    circle->next = circles;
    circles = circle;
    joints |= static_cast<u64>(static_cast<s64>(1 << (joint & 0x1F)));
}

void ShadowShapes::AddCapsule(ShadowCapsule* capsule)
{
    u8 joint = capsule->joint;
    capsule->next = capsules;
    u8 secondJoint = capsule->secondJoint;
    capsules = capsule;
    joints = static_cast<u64>(static_cast<s64>(1 << (secondJoint & 0x1F))) | static_cast<u64>(static_cast<s64>(1 << (joint & 0x1F))) |
             joints;
}

ShadowSlot* ShadowSlot::Construct(f32 strength, ShadowSlot* slot, ShadowShapes* shapes)
{
    slot->strength = strength;
    slot->shapes = shapes;
    slot->reachSquared = shapes != nullptr ? shapes->bits >> ShadowShapes::SquaredShift & ShadowShapes::SquaredMask : 0;
    return slot;
}

ShadowNode* ShadowNode::Construct(ShadowNode* node)
{
    GameNode::Construct(node);
    node->vtable = g_ShadowNodeVTable;
    RetailLibc::MemorySet(&node->bits, 0, sizeof(node->bits));
    for (u32 index = 0; index < Slots; index++)
    {
        node->slots[index] = nullptr;
    }

    return node;
}

void ShadowNode::Destroy(u32 destroyFlags)
{
    vtable = g_ShadowNodeVTable;
    for (u32 index = 0; index < Slots; index++)
    {
        DeleteSlot(slots[index]);
    }

    GameNode::Destroy(destroyFlags);
}

u32 ShadowNode::Kind()
{
    return NodeKind;
}

u32 ShadowNode::GetClassId()
{
    return ClassId;
}

void ShadowNode::ClearSlot(u32 slot)
{
    DeleteSlot(slots[slot]);
    slots[slot] = nullptr;
}

void ShadowNode::SetSlot(u32 slot, ShadowSlot* shadow)
{
    DeleteSlot(slots[slot]);
    slots[slot] = shadow;
}

u32 ShadowNode::Update(TimeClock* clock)
{
    InstanceContext* instance = owner;
    u32 flags = instance->flags;
    if ((flags & ReferencedObject::FlagShadow) == 0 || (flags & ReferencedObject::FlagVisible) == 0 ||
        (flags & ReferencedObject::FlagInDrawnCell) == 0)
    {
        return GameNode::Update(clock);
    }

    ShadowSlot* slot = slots[bits & 0xFF];
    if (slot == nullptr)
    {
        return GameNode::Update(clock);
    }

    u32 stamp = instance->seen[0] | instance->seen[1] << 8 | instance->seen[2] << 16;
    if (!(stamp < slot->reachSquared))
    {
        return GameNode::Update(clock);
    }

    // The distance into the shapes' distances, the strength blended between the strengths on either side of it
    u32 distance = static_cast<u32>(static_cast<s32>(__builtin_sqrtf(static_cast<f32>(static_cast<s32>(stamp)))));
    f32 before = slot->strength;
    u32 near = 0;
    for (ShadowShapes* shapes = slot->shapes; shapes != nullptr; shapes = shapes->next)
    {
        u32 far = shapes->bits & ShadowShapes::DistanceMask;
        f32 strength = shapes->strength;
        if (distance < far)
        {
            f32 share = static_cast<f32>(static_cast<s32>(distance - near)) / static_cast<f32>(static_cast<s32>(far - near));
            f32 blended = strength * share + before * (1.0f - share);
            if (!(__builtin_fabsf(blended) <= Faintest))
            {
                CastShadow(blended, shapes, instance);
            }

            break;
        }

        near = far;
        before = strength;
    }

    return GameNode::Update(clock);
}

void CastShadow(f32 strength, ShadowShapes* shapes, InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    ChunkShadows* chunkShadows = instance->chunk->shadows;
    RotateAndTranslate(place);
    u64 joints = shapes->joints;
    Vector4 from = *reinterpret_cast<const Vector4*>(&g_LightingConstants[ShadowDirection]);
    from.x = from.x * strength;
    from.y = from.y * strength;
    from.z = from.z * strength;
    const Vector4* at = RowOf(&place->matrix, 3);
    from.x = from.x + at->x;
    from.y = from.y + at->y;
    from.z = from.z + at->z;
    if (joints != 0)
    {
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, NodeModel));
        OgiAnimator* animator = model->animator;
        if (animator != nullptr)
        {
            Matrix4x4 matrices[MostJoints];
            for (u32 joint = 0; joints != 0 && joint < MostJoints; joint++, joints >>= 1)
            {
                if ((joints & 1) == 0)
                {
                    continue;
                }

                SizedArray<JointAnimation*>* table = animator->cameraJoints;
                auto* jointMatrix = table != nullptr ? reinterpret_cast<const Matrix4x4*>(table->data[joint & 0xFF]) : nullptr;
                if (jointMatrix != nullptr)
                {
                    VuMultiplyMatrices(jointMatrix, &place->matrix, &matrices[joint]);
                }
                else
                {
                    matrices[joint] = place->matrix;
                }
            }

            if (shapes->circles != nullptr)
            {
                CastCircleShadows(shapes->circles, &from, matrices, chunkShadows);
            }

            if (shapes->capsules != nullptr)
            {
                CastCapsuleShadows(shapes->capsules, &from, matrices, chunkShadows);
            }
        }
    }

    if (shapes->plain != nullptr)
    {
        CastPlainShadow(shapes->plain, &from, &place->matrix, chunkShadows);
    }
}

void CastCircleShadows(ShadowCircle* circle, const Vector4* from, const Matrix4x4* joints, ChunkShadows* shadows)
{
    const Matrix4x4* joint = &joints[circle->joint];
    Vector4 way;
    f32 size[4];
    size[0] = circle->radius;
    size[2] = circle->height;
    size[3] = 1.0f;
    size[1] = WayTo(from, RowOf(joint, 3), &way);
    ShadowEntry* entry = shadows->Add(circle->kind, &circle->offset.x, size);
    if (entry != nullptr)
    {
        entry->frame = *joint;
    }

    if (circle->next != nullptr)
    {
        CastCircleShadows(circle->next, from, joints, shadows);
    }
}

void CastCapsuleShadows(ShadowCapsule* capsule, const Vector4* from, const Matrix4x4* joints, ChunkShadows* shadows)
{
    const Vector4* start = RowOf(&joints[capsule->joint], 3);
    const Vector4* end = RowOf(&joints[capsule->secondJoint], 3);
    Vector4 middle = *start;
    Vector4 axis = *end;
    middle.x = (middle.x + end->x) * 0.5f;
    middle.y = (middle.y + end->y) * 0.5f;
    middle.z = (middle.z + end->z) * 0.5f;
    Vector4 way = *from;
    way.x = way.x - middle.x;
    way.y = way.y - middle.y;
    way.z = way.z - middle.z;
    f32 length = __builtin_sqrtf((way.x * way.x + way.y * way.y) + way.z * way.z);
    f32 inverse = 1.0f / length;
    axis.x = axis.x - start->x;
    axis.y = axis.y - start->y;
    axis.z = axis.z - start->z;
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    f32 axisLength = __builtin_sqrtf((axis.x * axis.x + axis.y * axis.y) + axis.z * axis.z);
    if (__builtin_fabsf(axisLength) <= Faintest)
    {
        if (capsule->next != nullptr)
        {
            CastCapsuleShadows(capsule->next, from, joints, shadows);
        }

        return;
    }

    f32 axisInverse = 1.0f / axisLength;
    axis.x = axis.x * axisInverse;
    axis.y = axis.y * axisInverse;
    axis.z = axis.z * axisInverse;
    f32 along = (axis.x * way.x + axis.y * way.y) + axis.z * way.z;
    if (-MostAlong <= along && along <= MostAlong)
    {
        f32 size[4] = {capsule->radius, length, axisLength * 0.5f, 1.0f};
        ShadowEntry* entry = shadows->Add(capsule->kind, &capsule->offset.x, size);
        if (entry != nullptr)
        {
            // Across both the axis and the way, then across that and the axis, the axis, the middle
            Vector4* across = RowOf(&entry->frame, 0);
            across->x = axis.y * way.z - axis.z * way.y;
            across->y = axis.z * way.x - axis.x * way.z;
            across->z = axis.x * way.y - axis.y * way.x;
            across->w = 1.0f;
            Vector4* second = RowOf(&entry->frame, 1);
            second->x = across->y * axis.z - across->z * axis.y;
            second->y = across->z * axis.x - across->x * axis.z;
            second->z = across->x * axis.y - across->y * axis.x;
            second->w = 1.0f;
            *RowOf(&entry->frame, 2) = axis;
            *RowOf(&entry->frame, 3) = middle;
        }
    }

    if (capsule->next != nullptr)
    {
        CastCapsuleShadows(capsule->next, from, joints, shadows);
    }
}

void CastPlainShadow(ShadowPlain* plain, const Vector4* from, const Matrix4x4* place, ChunkShadows* shadows)
{
    Vector4 way;
    f32 size[4];
    size[0] = plain->width;
    size[2] = plain->depth;
    size[3] = 1.0f;
    size[1] = WayTo(from, RowOf(place, 3), &way);
    ShadowEntry* entry = shadows->Add(plain->kind, &plain->offset.x, size);
    if (entry != nullptr)
    {
        entry->frame = *place;
    }
}

ShadowEntry* ChunkShadows::Add(u8 kind, const f32* offset, const f32* size)
{
    u32 count = *currentCount;
    ShadowEntry* entry = &current[count];
    if (!(count < capacity))
    {
        return nullptr;
    }

    entry->kind = kind;
    entry->offset[0] = offset[0];
    entry->offset[1] = offset[1];
    entry->offset[2] = offset[2];
    entry->size[0] = size[0];
    entry->size[1] = size[1];
    entry->size[2] = size[2];
    (*currentCount)++;
    return entry;
}

ShadowEntry* ConstructShadowEntry(ShadowEntry* entry)
{
    RetailLibc::MemorySet(&entry->kind, 0, 4);
    return entry;
}

ChunkShadows* ChunkShadows::Construct(ChunkShadows* shadows, s32 capacity)
{
    shadows->firstCount = 0;
    shadows->secondCount = 0;
    shadows->capacity = capacity;
    auto* first = static_cast<ShadowEntry*>(MemoryAllocate2(capacity * sizeof(ShadowEntry)));
    for (u32 index = 0; index < static_cast<u32>(capacity); index++)
    {
        ConstructShadowEntry(&first[index]);
    }

    shadows->first = first;
    auto* second = static_cast<ShadowEntry*>(MemoryAllocate2(capacity * sizeof(ShadowEntry)));
    for (u32 index = 0; index < static_cast<u32>(capacity); index++)
    {
        ConstructShadowEntry(&second[index]);
    }

    shadows->second = second;
    shadows->currentCount = &shadows->firstCount;
    shadows->current = shadows->first;
    return shadows;
}

void ChunkShadows::Destroy(u32 destroyFlags)
{
    if (first != nullptr)
    {
        MemoryDeallocate_(first);
    }

    if (second != nullptr)
    {
        MemoryDeallocate_(second);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ChunkShadows::Swap()
{
    if (current == first)
    {
        currentCount = &secondCount;
        current = second;
        secondCount = 0;
        return;
    }

    current = first;
    currentCount = &firstCount;
    firstCount = 0;
}

void ChunkShadows::Draw(const Vector4* planes, const Matrix4x4*, const Matrix4x4* view)
{
    Matrix4x4 frame;
    InitIdentityMatrix(&frame);
    for (u32 index = 0; index < *currentCount; index++)
    {
        ShadowEntry* entry = &current[index];
        Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
        Vector4 axis = *RowOf(&entry->frame, 2);
        f32 inverse = InverseLength(&axis, LengthEpsilon);
        axis.x = axis.x * inverse;
        axis.y = axis.y * inverse;
        axis.z = axis.z * inverse;
        Vector4 side;
        side.x = up.y * axis.z - up.z * axis.y;
        side.y = up.z * axis.x - up.x * axis.z;
        side.z = up.x * axis.y - up.y * axis.x;
        side.w = 1.0f;
        if (Crossing < (side.x * side.x + side.y * side.y) + side.z * side.z)
        {
            inverse = InverseLength(&side, LengthEpsilon);
            side.x = side.x * inverse;
            side.y = side.y * inverse;
            side.z = side.z * inverse;
            axis.w = 1.0f;
            axis.x = side.y * up.z - side.z * up.y;
            axis.y = side.z * up.x - side.x * up.z;
            axis.z = side.x * up.y - side.y * up.x;
            inverse = InverseLength(&axis, LengthEpsilon);
            axis.x = axis.x * inverse;
            axis.y = axis.y * inverse;
            axis.z = axis.z * inverse;
        }
        else
        {
            axis = g_ZAxis;
            side = g_XAxis;
        }

        side.w = 0.0f;
        up.w = 0.0f;
        axis.w = 0.0f;
        *RowOf(&frame, 0) = side;
        *RowOf(&frame, 1) = up;
        *RowOf(&frame, 2) = axis;
        *RowOf(&frame, 3) = *RowOf(&entry->frame, 3);
        // Down from the shape's offset by its size, onto the collision: how far that is
        Vector4 start = g_DefaultBox.min;
        start.w = 1.0f;
        Vector4 down = {entry->size[0], entry->size[1], entry->size[2], 1.0f};
        down.x = -down.x;
        down.y = -down.y;
        down.z = -down.z;
        Vector4 segment[2];
        VuTransformPoint(&frame, &start, &segment[0]);
        VuTransformPoint(&frame, &down, &segment[1]);
        Vector4 clipped[2];
        if (ClipSegmentToPlanes(planes, segment, clipped) == 0)
        {
            continue;
        }

        const Vector4* point = &clipped[1];
        f32 dx = segment[0].x - point->x;
        f32 dy = segment[0].y - point->y;
        f32 dz = segment[0].z - point->z;
        entry->size[1] = __builtin_sqrtf((dx * dx + dy * dy) + dz * dz);
        Matrix4x4 world;
        VuMultiplyMatrices(&frame, view, &world);
        Matrix4x4 toScreen;
        VuMultiplyMatrices(&world, &g_ShadowToScreen, &toScreen);
        Matrix4x4 toCamera;
        VuMultiplyMatrices(&world, &g_ShadowToCamera, &toCamera);
        DrawShadowEntry(entry, &toScreen, &toCamera, &world);
    }

    if (current == first)
    {
        currentCount = &secondCount;
        current = second;
        secondCount = 0;
    }
    else
    {
        current = first;
        currentCount = &firstCount;
        firstCount = 0;
    }
}

void DrawShadowEntry(const ShadowEntry* entry, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
{
    Matrix4x4 scale;
    *RowOf(&scale, 0) = {entry->size[0], 0.0f, 0.0f, 0.0f};
    *RowOf(&scale, 1) = {0.0f, entry->size[1], 0.0f, 0.0f};
    *RowOf(&scale, 2) = {0.0f, 0.0f, entry->size[2], 0.0f};
    *RowOf(&scale, 3) = {entry->offset[0], entry->offset[1], entry->offset[2], 1.0f};
    RigidModel* mesh = g_ShadowMeshes[entry->kind];
    Matrix4x4 screen;
    MultiplyReversed(toScreen, &scale, &screen);
    Matrix4x4 camera;
    MultiplyReversed(toCamera, &scale, &camera);
    Matrix4x4 placed;
    MultiplyReversed(world, &scale, &placed);
    Platform::Graphics::DrawShadowMesh(mesh, &screen, &camera, &placed);
}

s32 ShadowShapeOfToken(u32 kind)
{
    switch (kind)
    {
    case 0xBB:
        return 0;
    case 0xBC:
        return 2;
    case 0xBD:
        return 1;
    case 0xBE:
        return 3;
    case 0x10D:
        return 4;
    case 0x10E:
        return 6;
    case 0x10F:
        return 5;
    case 0x110:
        return 7;
    default:
        return -1;
    }
}

void InitShadows(u32 fromFiles)
{
    Platform::Graphics::SetUpShadowPass();
    LoadShadowMeshes(fromFiles);
}

namespace
{
constexpr u32 ShadowMeshKinds = 8;
constexpr u32 ShadowModelIds = 0x67;
constexpr u32 AcquireSlot = 4;

// The table's folder, a backslash and the name, upper cased (by the signed character, as the retail code indexes the table)
void ShadowFilePath(String* path, const String* folder, const String* name)
{
    *path = {};
    StringAssign(path, folder->string);
    const char separator[2] = {'\\', '\0'};
    StringAppend(path, separator);
    if (name->length != 0)
    {
        StringAppend(path, name->string);
    }

    for (s32 index = 0; index < path->length; index++)
    {
        s8 character = path->string[index];
        if ((CasingTable[character] & 2) != 0)
        {
            path->string[index] = static_cast<char>(character - 0x20);
        }
    }
}
}

RigidModelData* LoadShadowModel(ModelTable* table, const String* name, u32 id)
{
    ModelTable::Entry* entry = ModelFind(table, id);
    RigidModelData* model = entry != nullptr ? entry->item : nullptr;
    String path;
    ShadowFilePath(&path, &table->name, name);
    if (model != nullptr)
    {
        TakeReference(model);
    }
    else
    {
        File file;
        File::Construct(&file);
        model = ModelConstruct(MemoryAllocate(0x1C), id);
        TakeReference(model);
        ModelInsert(table, &model, id);
        file.Open(path.string, File::ModeRead);
        ModelRead(model, &file);
        file.Destroy(2);
    }

    StringDestroy(&path);
    return model;
}

RigidModel* LoadShadowMesh(MeshTable* table, const String* name, u32 id)
{
    MeshTable::Entry* entry = MeshFind(table, id);
    RigidModel* mesh = entry != nullptr ? entry->item : nullptr;
    String path;
    ShadowFilePath(&path, &table->name, name);
    if (mesh != nullptr)
    {
        TakeReference(mesh);
    }
    else
    {
        File file;
        File::Construct(&file);
        mesh = MeshConstruct(MemoryAllocate(0x20), id);
        TakeReference(mesh);
        MeshInsert(table, &mesh, id);
        file.Open(path.string, File::ModeRead);
        MeshRead(mesh, &file);
        file.Destroy(2);
    }

    StringDestroy(&path);
    return mesh;
}

// From the files: their materials and models read, then each kind's model (ID 0x67 on) and mesh (by its kind) from the models'
// folder, the tables' folders put back after. Else the meshes of the default chunk's
void LoadShadowMeshes(u32 fromFiles)
{
    if (fromFiles == 0)
    {
        for (u32 kind = 0; kind < ShadowMeshKinds; kind++)
        {
            u32 id = kind;
            g_ShadowMeshes[kind] = CallVirtual<RigidModel*>(&g_MeshTable, g_MeshTable.vtable, AcquireSlot, &id, nullptr);
        }

        return;
    }

    GraphicsKindReader<MaterialKind> materials;
    materials.vtable = g_MaterialReaderVTable;
    materials.table = &g_MaterialTable;
    GraphicsKindReader<ModelKind> models;
    models.vtable = g_ModelReaderVTable;
    models.table = &g_ModelTable;
    String meshFolder = {};
    StringAssign(&meshFolder, g_MeshTable.name.string);
    String modelFolder = {};
    StringAssign(&modelFolder, g_ModelTable.name.string);
    AddResourcePackageToLoadQueue(&materials, g_ShadowMaterialsPackage, 0);
    AddResourcePackageToLoadQueue(&models, g_ShadowModelsPackage, 0);
    LoadQueuedSectionsIntoMemory_();
    String folder;
    StringConstruct(&folder, g_ShadowModelsFolder);
    StringAssign(&g_MeshTable.name, folder.string);
    StringDestroy(&folder);
    StringConstruct(&folder, g_ShadowModelsFolder);
    StringAssign(&g_ModelTable.name, folder.string);
    StringDestroy(&folder);
    for (u32 kind = 0; kind < ShadowMeshKinds; kind++)
    {
        const char* name = g_ShadowMeshNames[kind];
        if (name == nullptr)
        {
            g_ShadowMeshes[kind] = nullptr;
            continue;
        }

        String file;
        StringConstruct(&file, name);
        LoadShadowModel(&g_ModelTable, &file, kind + ShadowModelIds);
        StringAppend(&file, g_MeshExtension);
        g_ShadowMeshes[kind] = LoadShadowMesh(&g_MeshTable, &file, kind);
        StringDestroy(&file);
    }

    StringAssign(&g_MeshTable.name, meshFolder.string);
    StringAssign(&g_ModelTable.name, modelFolder.string);
    StringDestroy(&modelFolder);
    StringDestroy(&meshFolder);
    ModelReaderDestroy(&models, 0);
    MaterialReaderDestroy(&materials, 0);
}

void LightingConstantsStaticInit()
{
    InitLightingConstants(1, 0xFFFF);
}

EABI_EXPORT(FUN_001ccbb8, ShadowCircle::Construct);
EABI_EXPORT(FUN_001cc018, ShadowCapsule::Construct);
EABI_EXPORT(FUN_001cc158, ShadowPlain::Construct);
EABI_EXPORT(FUN_001cc0f0, ShadowPlain::ConstructSquare);
EABI_EXPORT(FUN_001cbde0, ShadowShapes::Construct);
EABI_EXPORT(FUN_001cbe80, ShadowShapes::ConstructPlain);
EABI_EXPORT(FUN_001cc2c8, ShadowSlot::Construct);
EABI_EXPORT(FUN_001c9130, CastShadow);
