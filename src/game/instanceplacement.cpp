#include "game/instances.h"

#include "game/chunkdata.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/scenery.h"
#include "game/string.h"

// Where an instance is kept to be put back (its rotation, position and chunk): copied, taken from an instance and applied to one.
// A character keeps such places in the chunks it was put in (InstancePlaces), the ones in chunks still loaded

namespace
{
// The way into the game an instance put back at its first place leaves its chunk for
constexpr u32 PlacesWay = 1;
// ChunkMakeGlobal moving the instance whatever state its chunk is in
constexpr u32 NoStateCheck = 0;

void TakeRotation(InstancePlacement* placement, InstanceContext* instance)
{
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    placement->rotation = place->rotation;
}
}

InstancePlacement* CopyInstancePlacement(InstancePlacement* placement, const InstancePlacement* other)
{
    placement->rotation = other->rotation;
    placement->position = other->position;
    placement->flags = other->flags;
    placement->chunk.string = nullptr;
    placement->chunk.length = 0;
    placement->chunk.capacity = 0;
    StringAssign(&placement->chunk, other->chunk.string);
    return placement;
}

InstancePlacement* InstancePlacement::Assign(const InstancePlacement* other)
{
    rotation = other->rotation;
    position = other->position;
    flags = other->flags;
    StringAssign(&chunk, other->chunk.string);
    return this;
}

void InstancePlacement::Take(InstanceContext* instance, u32 stays)
{
    ChunkData* chunkData = instance->chunk;
    TakeRotation(this, instance);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    position = place->position;
    if (chunkData != nullptr)
    {
        StringAssign(&chunk, chunkData->path.string);
    }

    flags.stays = stays;
}

void InstancePlacement::TakeInChunk(InstanceContext* instance, ChunkData* chunkData, u32 stays)
{
    StringAssign(&chunk, chunkData->path.string);
    TakeRotation(this, instance);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    position = place->position;
    flags.stays = stays;
}

void InstancePlacement::Apply(InstanceContext* instance)
{
    ChunkData* chunkData = FindChunkData(GetChunkList(), &chunk);
    if (chunkData == nullptr)
    {
        if (flags.stays == 0)
        {
            CallVirtual<u32>(instance, instance->vtable, InstanceContext::ReleaseSlot);
        }

        return;
    }

    ObjectPlace* place = instance->place;
    place->SyncRotation();
    if (place->TurnTo(&rotation))
    {
        QueueObject(instance);
    }

    place = instance->place;
    place->SyncPosition();
    if (place->MoveTo(&position))
    {
        QueueObject(instance);
    }

    MoveToChunk(chunkData, instance);
}

namespace
{
ObjectNodeBase* ObjectNodeOf(InstanceContext* instance)
{
    return static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
}

// A place kept, moved down over the ones dropped before it
void KeepPlace(InstancePlaces* places, u32 index, u32* kept)
{
    if (*kept < index)
    {
        places->places[*kept].Assign(&places->places[index]);
    }

    (*kept)++;
}
}

InstancePlaces* ConstructInstancePlaces(InstancePlaces* places, InstanceContext* instance, ChunkData* chunk)
{
    places->count = 0;
    places->lastChunk.string = nullptr;
    places->lastChunk.length = 0;
    places->lastChunk.capacity = 0;
    StringAssign(&places->lastChunk, chunk->path.string);
    for (InstancePlacement& placement : places->places)
    {
        InstancePlacement::Construct(&placement, nullptr);
    }

    places->places[places->count++].TakeInChunk(instance, chunk, 0);
    return places;
}

InstancePlaces* MakeInstancePlaces(InstanceContext* instance, ChunkData* chunk)
{
    if (instance->places == nullptr)
    {
        auto* places = static_cast<InstancePlaces*>(MemoryAllocate(sizeof(InstancePlaces)));
        instance->places = ConstructInstancePlaces(places, instance, chunk);
    }

    return instance->places;
}

u32 KeepLoadedPlaces(InstancePlaces* places)
{
    u32 kept = 0;
    for (u32 index = 0; index < places->count; index++)
    {
        ChunkData* chunk = FindChunkData(GetChunkList(), &places->places[index].chunk);
        if (chunk == nullptr)
        {
            continue;
        }

        u32 state = chunk->flags.state;
        if (state == ChunkReleasing || state == ChunkReleased)
        {
            continue;
        }

        KeepPlace(places, index, &kept);
    }

    places->count = kept;
    return kept;
}

u32 DropPlacesInChunk(InstancePlaces* places, ChunkData* chunk)
{
    u32 kept = 0;
    for (u32 index = 0; index < places->count; index++)
    {
        if (FindChunkData(GetChunkList(), &places->places[index].chunk) == chunk)
        {
            continue;
        }

        KeepPlace(places, index, &kept);
    }

    places->count = kept;
    return kept;
}

// Retail bug: nothing keeps the count within the 8 places, a ninth place in another loaded chunk would be written past them
u32 AddInstancePlace(InstancePlaces* places, InstanceContext* instance, ChunkData* chunk)
{
    KeepLoadedPlaces(places);
    DropPlacesInChunk(places, chunk);
    places->places[places->count++].TakeInChunk(instance, chunk, 0);
    return 1;
}

u32 ReleasePlaces(InstancePlaces* places, InstanceContext* instance)
{
    if (KeepLoadedPlaces(places) == 0)
    {
        return 1;
    }

    ChunkData* chunk = instance->chunk;
    if (chunk != nullptr && DropPlacesInChunk(places, chunk) == 0)
    {
        return 1;
    }

    ObjectNodeBase* node = ObjectNodeOf(instance);
    node->information.Assign(&places->places[0]);
    ClearComebackPlacement(node);
    if (chunk == nullptr)
    {
        CallVirtual<u32>(instance, instance->vtable, InstanceContext::SleepSlot);
        MakeGlobal(PlacesWay, instance);
        return 0;
    }

    ChunkMakeGlobal(chunk, NoStateCheck, PlacesWay, instance);
    StringAssign(&places->lastChunk, node->information.chunk.string);
    return 0;
}

void DismissPlaces(InstancePlaces* places, InstanceContext* instance)
{
    KeepLoadedPlaces(places);
    CallVirtual<u32>(instance, instance->vtable, InstanceContext::SleepSlot);
}

u32 PlacePlacesInChunk(InstancePlaces* places, InstanceContext* instance, ChunkData* chunk)
{
    if (KeepLoadedPlaces(places) == 0)
    {
        return 0;
    }

    InstancePlacement* found = nullptr;
    for (u32 index = 0; index < places->count; index++)
    {
        if (FindChunkData(GetChunkList(), &places->places[index].chunk) == chunk)
        {
            found = &places->places[index];
        }
    }

    if (found == nullptr)
    {
        return 0;
    }

    ObjectNodeBase* node = ObjectNodeOf(instance);
    ChunkData* current = instance->chunk;
    node->information.Assign(found);
    ClearComebackPlacement(node);
    if (current != chunk)
    {
        ChunkMakeGlobal(current, NoStateCheck, PlacesWay, instance);
    }
    else
    {
        node->information.Take(instance, 0);
    }

    StringAssign(&places->lastChunk, node->information.chunk.string);
    return 1;
}
