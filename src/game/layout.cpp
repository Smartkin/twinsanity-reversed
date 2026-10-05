#include "game/layout.h"

#include "game/cameras.h"
#include "game/instances.h"
#include "game/memory.h"
#include "game/stream.h"

extern "C"
{
    extern const GccVTableEntry g_IdArrayIteratorBaseVTable[] RETAIL(D_00303430);
}

namespace
{
// The lists an element is made with have room for this many, and grow by as many
constexpr u32 ListGrowth = 10;

void ConstructIds(IdArray* ids)
{
    ids->growth = ListGrowth;
    ids->capacity = ListGrowth;
    ids->count = 0;
    ids->data = static_cast<u16*>(MemoryAllocate2(ListGrowth * sizeof(u16)));
}

// An object instance's list read: its count, room and growth, then the count's IDs (what it had freed first)
void ReadIds(IdArray* ids, Stream* stream)
{
    if (ids->data != nullptr)
    {
        MemoryDeallocate_(ids->data);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&ids->count));
    stream->ReadS32(reinterpret_cast<s32*>(&ids->capacity));
    stream->ReadS32(reinterpret_cast<s32*>(&ids->growth));
    ids->data = ids->count != 0 ? static_cast<u16*>(MemoryAllocate2(ids->capacity * sizeof(u16))) : nullptr;
    for (u32 index = 0; index < ids->count; index++)
    {
        stream->ReadS16(reinterpret_cast<s16*>(&ids->data[index]));
    }
}

// A list of IDs made with new[] (each undefined)
u16* NewIds(u32 count)
{
    u16* ids = NewArray<u16>(count);
    for (u32 index = 0; index < count; index++)
    {
        SetUndefinedId(&ids[index]);
    }

    return ids;
}

void DestroyProperties(PropertyList* properties)
{
    CallVirtual<void>(properties, properties->vtable, PropertyList::DestroySlot, u32{DestroyAndFree});
}
}

void SetUndefinedId(u16* id)
{
    *id = UndefinedId;
}

void ReadNewedIds(IdArray* ids, Stream* stream)
{
    if (ids->data != nullptr)
    {
        DeleteArray(ids->data);
    }

    stream->ReadS32(reinterpret_cast<s32*>(&ids->count));
    stream->ReadS32(reinterpret_cast<s32*>(&ids->capacity));
    stream->ReadS32(reinterpret_cast<s32*>(&ids->growth));
    ids->data = ids->count != 0 ? NewIds(ids->capacity) : nullptr;
    for (u32 index = 0; index < ids->count; index++)
    {
        stream->ReadS16(reinterpret_cast<s16*>(&ids->data[index]));
    }
}

void IdArrayIterator::Destroy(u32 destroyFlags)
{
    BaseDestroy(destroyFlags);
}

void IdArrayIterator::BaseDestroy(u32 destroyFlags)
{
    vtable = g_IdArrayIteratorBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void IdArrayIterator::First()
{
    index = 0;
}

u32 IdArrayIterator::IsDone()
{
    return index < 0 || static_cast<u32>(index) >= array->count;
}

u16* IdArrayIterator::Current()
{
    return &array->data[index];
}

void IdArrayIterator::Next()
{
    index++;
}

void IdArrayIterator::Previous()
{
    index--;
}

void IdArrayIterator::Last()
{
    index = static_cast<s32>(array->count) - 1;
}

IdArrayIterator* IdArrayIterator::Assign(const IdArrayIterator* other)
{
    array = other->array;
    index = other->index;
    return this;
}

ObjectInstance* ObjectInstance::ConstructEmpty(ObjectInstance* instance)
{
    ConstructIds(&instance->instances);
    ConstructIds(&instance->positions);
    ConstructIds(&instance->paths);
    instance->properties = nullptr;
    SetUndefinedId(reinterpret_cast<u16*>(&instance->objectId));
    SetUndefinedId(reinterpret_cast<u16*>(&instance->refListIndex));
    SetUndefinedId(reinterpret_cast<u16*>(&instance->spawnScript));
    instance->ownsProperties = 0;
    return instance;
}

ObjectInstance* ObjectInstance::Construct(ObjectInstance* instance, Stream* stream)
{
    ConstructEmpty(instance);
    instance->Read(stream);
    return instance;
}

void ObjectInstance::Destroy(u32 destroyFlags)
{
    if (ownsProperties != 0 && properties != nullptr)
    {
        DestroyProperties(properties);
    }

    if (paths.data != nullptr)
    {
        MemoryDeallocate_(paths.data);
    }

    if (positions.data != nullptr)
    {
        MemoryDeallocate_(positions.data);
    }

    if (instances.data != nullptr)
    {
        MemoryDeallocate_(instances.data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ObjectInstance::Read(Stream* stream)
{
    stream->Read(&position, sizeof(position), 1);
    for (TaggedValue& angle : rotation)
    {
        angle.Read(stream);
    }

    ReadIds(&instances, stream);
    ReadIds(&positions, stream);
    ReadIds(&paths, stream);
    stream->ReadS16(&objectId);
    stream->ReadS16(&refListIndex);
    stream->ReadS16(&spawnScript);
    if (ownsProperties != 0 && properties != nullptr)
    {
        DestroyProperties(properties);
    }

    properties = PropertyList::Construct(static_cast<PropertyList*>(MemoryAllocate(sizeof(PropertyList))), stream);
    ownsProperties = 1;
}

InstanceTemplate* InstanceTemplate::Construct(InstanceTemplate* instanceTemplate, Stream* stream)
{
    instanceTemplate->name.string = nullptr;
    instanceTemplate->name.length = 0;
    instanceTemplate->name.capacity = 0;
    SetUndefinedId(reinterpret_cast<u16*>(&instanceTemplate->objectId));
    IdArray& starters = instanceTemplate->starters;
    starters.capacity = ListGrowth;
    starters.growth = ListGrowth;
    starters.count = 0;
    starters.data = NewIds(ListGrowth);
    PropertyList& properties = instanceTemplate->properties;
    properties.vtable = g_PropertyListVTable;
    properties.state.value = 0;
    properties.taggedCount = 0;
    properties.tagged = nullptr;
    properties.floatCount = 0;
    properties.floats = nullptr;
    properties.intCount = 0;
    properties.ints = nullptr;
    for (u8& count : properties.counts)
    {
        count = 0;
    }

    instanceTemplate->Read(stream);
    return instanceTemplate;
}

void InstanceTemplate::Read(Stream* stream)
{
    StringRead(&name, stream);
    stream->ReadS16(&objectId);
    stream->ReadS8(&objectSubType);
    stream->ReadS8(&objectType);
    ReadNewedIds(&starters, stream);
    stream->ReadS8(&exitPoints);
    stream->ReadS8(&reactJoints);
    properties.Read(stream);
}

LayoutTrigger* LayoutTrigger::Construct(LayoutTrigger* trigger)
{
    trigger->vtable = g_LayoutTriggerVTable;
    trigger->activators = 0;
    ConstructIds(&trigger->instances);
    trigger->header.value = 0;
    return trigger;
}

void LayoutTrigger::Destroy(u32 destroyFlags)
{
    vtable = g_LayoutTriggerVTable;
    if (instances.data != nullptr)
    {
        MemoryDeallocate_(instances.data);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void LayoutTrigger::Read(Stream* stream)
{
    stream->Read(&header, sizeof(header), 1);
    stream->ReadS32(reinterpret_cast<s32*>(&activators));
    stream->ReadF32(&checkInterval);
    stream->Read(&rotation, sizeof(rotation), 1);
    stream->Read(&position, sizeof(position), 1);
    stream->Read(&scale, sizeof(scale), 1);
    ReadIds(&instances, stream);
}

void MessageTrigger::Destroy(u32 destroyFlags)
{
    vtable = g_MessageTriggerVTable;
    LayoutTrigger::Destroy(destroyFlags);
}

void MessageTrigger::Read(Stream* stream)
{
    LayoutTrigger::Read(stream);
    for (s16& message : messages)
    {
        stream->ReadS16(&message);
    }
}

u32 MessageTrigger::ItemType()
{
    return TypeId;
}

void CameraTrigger::Destroy(u32 destroyFlags)
{
    vtable = g_CameraTriggerVTable;
    LayoutTrigger::Destroy(destroyFlags);
}

void CameraTrigger::Read(Stream* stream)
{
    LayoutTrigger::Read(stream);
    auto* made = static_cast<MainCamera*>(MemoryAllocate(sizeof(MainCamera)));
    made->Read(stream);
    camera = made;
}

u32 CameraTrigger::ItemType()
{
    return TypeId;
}

void LayoutPosition::Read(Stream* stream)
{
    stream->Read(&position, sizeof(position), 1);
}

void LayoutPosition::Destroy(u32 destroyFlags)
{
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PointList::Destroy(u32 destroyFlags)
{
    vtable = g_PointListVTable;
    if (points != nullptr)
    {
        MemoryDeallocate_(points);
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void PointList::Transform(const Matrix4x4* matrix)
{
    for (s32 index = 0; index < count; index++)
    {
        VuTransformPoint(matrix, &points[index], &points[index]);
    }
}

void PointList::Read(Stream* stream)
{
    stream->ReadU32(reinterpret_cast<u32*>(&count));
    auto* read = static_cast<Vector4*>(MemoryAllocate2(count * sizeof(Vector4)));
    points = read;
    stream->Read(read, count * sizeof(Vector4), 1);
}

u32 PointList::ItemType()
{
    return TypeId;
}

void LayoutPath::Destroy(u32 destroyFlags)
{
    vtable = g_LayoutPathVTable;
    if (lengths != nullptr)
    {
        MemoryDeallocate_(lengths);
    }

    PointList::Destroy(destroyFlags);
}

void LayoutPath::Read(Stream* stream)
{
    PointList::Read(stream);
    u32 parameters;
    stream->ReadU32(&parameters);
    lengths = static_cast<f32*>(MemoryAllocate2(parameters * 2 * sizeof(f32)));
    stream->Read(lengths, parameters * 2 * sizeof(f32), 1);
    steps = lengths + parameters;
}

u32 LayoutPath::ItemType()
{
    return TypeId;
}

ContactMessage* ContactMessage::Construct(ContactMessage* message)
{
    message->hitKinds = 0;
    message->damage = 0;
    message->point = g_DefaultBox.min;
    return message;
}

void CollisionSurface::Read(Stream* stream)
{
    // The ten physics parameters in the tools' order and the ID nothing keeps
    f32* const Physics[PhysicsParameters] = {&volumeScales[0], &volumeScales[1], &volumeScales[2], &volumeScales[3],
                                             &volumeScales[4], &acceleration,    &friction,        &restitution,
                                             &downhillPull,    &steepNormalY};
    u16* const Ids[10] = {&surfaceId, &stepSound1, &stepSound2, &impactParticles, &hardImpactParticles, &impactSound, &hardImpactSound,
                          &stepParticles, &landSound, &scrapeSound};
    spinFriction = 1.0f;
    rollFriction = 1.0f;
    stream->ReadS32(reinterpret_cast<s32*>(&flags));
    for (u16* id : Ids)
    {
        stream->ReadS16(reinterpret_cast<s16*>(id));
    }

    s16 unused;
    stream->ReadS16(&unused);
    for (f32* value : Physics)
    {
        stream->ReadF32(value);
    }

    stream->Read(&flow, sizeof(flow), 1);
    stream->Read(&contact, sizeof(contact), 1);
}

void SurfaceTable::Add(const CollisionSurface* surface)
{
    CollisionSurface& entry = surfaces[count];
    entry.flags = surface->flags;
    entry.acceleration = surface->acceleration;
    entry.friction = surface->friction;
    entry.restitution = surface->restitution;
    entry.downhillPull = surface->downhillPull;
    entry.steepNormalY = surface->steepNormalY;
    entry.flow = surface->flow;
    entry.contact.point = surface->contact.point;
    entry.contact.hitKinds = surface->contact.hitKinds;
    entry.contact.damage = surface->contact.damage;
    entry.surfaceId = surface->surfaceId;
    entry.impactSound = surface->impactSound;
    entry.hardImpactSound = surface->hardImpactSound;
    entry.scrapeSound = surface->scrapeSound;
    entry.stepSound1 = surface->stepSound1;
    entry.stepSound2 = surface->stepSound2;
    entry.landSound = surface->landSound;
    entry.impactParticles = surface->impactParticles;
    entry.hardImpactParticles = surface->hardImpactParticles;
    entry.stepParticles = surface->stepParticles;
    for (u32 index = 0; index < CollisionSurface::VolumeScales; index++)
    {
        entry.volumeScales[index] = surface->volumeScales[index];
    }

    entry.rollFriction = surface->rollFriction;
    entry.spinFriction = surface->spinFriction;
    count++;
}

SoundBox* SoundBox::Construct(SoundBox* box, const LayoutTrigger* trigger)
{
    Vector4 size = trigger->scale;
    box->min = size;
    box->max = size;
    box->min.x = -box->min.x;
    box->min.y = -box->min.y;
    box->min.z = -box->min.z;
    box->position = trigger->position;
    InitIdentityMatrix(&box->matrix);
    MatrixFromRotation(&box->matrix, &trigger->rotation);
    *reinterpret_cast<Vector4*>(box->matrix.m[3]) = box->position;
    box->inverse = box->matrix;
    VuInvertRigidInPlace(&box->inverse);
    f32 x = (box->min.x - box->max.x) * 0.5f;
    f32 y = (box->min.y - box->max.y) * 0.5f;
    f32 z = (box->min.z - box->max.z) * 0.5f;
    f32 radius = __builtin_sqrtf(x * x + y * y + z * z);
    box->radiusSquared = radius * radius;
    return box;
}
