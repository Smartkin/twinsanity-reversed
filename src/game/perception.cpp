#include "game/objectnode.h"

#include "game/agentparts.h"
#include "game/behaviours.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/objects.h"
#include "game/place.h"
#include "game/player.h"
#include "game/reference.h"

// What an object node's perception senses of the instances around it (its kind 0 sense)

namespace
{
// The query of the instances in range (those whose collision is active): how many it takes, the flag the search is handed, the
// distance of no hit
constexpr u16 MostInRange = 0x38;
constexpr u32 SearchFlag = 1;
// The instances noticed: those in range and the player
constexpr u32 MostNoticed = 60;
// The perception's slot an instance's attention is read from
constexpr u32 AttentionSlot = 2;

// A perception's sense (game/objectnode.h)
using Sense = PerceptionSense;

// How much an instance makes itself felt
f32 PresenceOf(const Agent* agent)
{
    return agent->part->presence;
}

// A place's position, worked out from its matrix first when it moved
Vector4 PositionOf(ObjectPlace* place)
{
    place->SyncPosition();
    return place->position;
}
}

u32 InstancesInSenseRange(const void* sense, ObjectNode* node, InstanceContext** found, u32 kinds)
{
    const auto* settings = static_cast<const Sense*>(sense);
    InstanceContext* instance = node->owner;
    ChunkData* chunk = instance->chunk;
    Vector4 sphere = {0.0f, 0.0f, 0.0f, 1.0f};
    DesignatedPosition(&sphere, ControlPacket::CurrentSpace, node->runners[0], nullptr, DesignatesNone, DesignatesNone, 0);
    sphere.w = settings->radius;
    InstanceQuery query;
    query.results = reinterpret_cast<void**>(found);
    query.count = 0;
    query.most = MostInRange;
    query.distance = NoHitDistance;
    // Retail keeps the stack's other bits (nothing reads them)
    query.bits.value = InstanceQueryBits::AllWanted;
    query.wantedFlags = ReferencedObjectFlags::CollisionActive;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    SkipInQuery(&query, instance);
    return ChunkInstancesInSphere(chunk, &sphere, kinds, &query, SearchFlag);
}

u32 NoticedInstances(const void* sense, ObjectNode* node, InstanceContext** noticed)
{
    const auto* settings = static_cast<const Sense*>(sense);
    InstanceContext* found[MostInRange];
    u32 count = InstancesInSenseRange(sense, node, found, settings->kinds);
    u32 noticedCount = 0;
    for (u32 index = 0; index < count; index++)
    {
        auto* other = static_cast<ObjectNodeBase*>(GetGameNode(&found[index]->nodes, NodeObject));
        if (SenseNoticesObject(sense, other->agent->objectId & ResourceIndexMask) != 0)
        {
            noticed[noticedCount++] = found[index];
        }
    }

    if (settings->bits.noticesPlayer)
    {
        noticed[noticedCount++] = g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
    }

    return noticedCount;
}

void SenseInstances(const void* sense, TimeClock*, ObjectNode* node, f32* level, Vector4* direction)
{
    const auto* settings = static_cast<const Sense*>(sense);
    *direction = g_DefaultBox.min;
    direction->w = 1.0f;
    f32 total = 0.0f;
    InstanceContext* noticed[MostNoticed];
    u32 count = NoticedInstances(sense, node, noticed);
    if (count != 0)
    {
        Vector4 position = PositionOf(node->owner->place);
        for (u32 index = 0; index < count; index++)
        {
            InstanceContext* instance = noticed[index];
            Vector4 other = PositionOf(instance->place);
            auto* otherNode = static_cast<ObjectNodeBase*>(GetGameNode(&instance->nodes, NodeObject));
            f32 dx = position.x - other.x;
            f32 dy = position.y - other.y;
            f32 dz = position.z - other.z;
            f32 squared = dx * dx + dy * dy + dz * dz;
            f32 presence = PresenceOf(otherNode->agent);
            f32 attention = InstanceAttention(sense, node, instance);
            presence = presence + settings->attentionWeight * attention;
            presence = presence * (1.0f - ClampFloat(squared / settings->falloff, settings->lowest, settings->highest));
            Vector4 away = {position.x - other.x, position.y - other.y, position.z - other.z, 1.0f};
            f32 scale = InverseLength(&away, LengthEpsilon);
            away.x = away.x * scale;
            away.y = away.y * scale;
            away.z = away.z * scale;
            total = total + presence / settings->divisor;
            direction->x = direction->x + away.x * presence;
            direction->y = direction->y + away.y * presence;
            direction->z = direction->z + away.z * presence;
        }

        total = ClampFloat(total, settings->lowest, settings->highest);
        f32 scale = InverseLength(direction, LengthEpsilon);
        direction->x = direction->x * scale;
        direction->y = direction->y * scale;
        direction->z = direction->z * scale;
    }

    if (*level <= total)
    {
        *level = total;
    }
    else
    {
        *level = ClampFloat(*level + settings->decay * PerceptionSense::DecayShare, settings->lowest, settings->highest);
    }
}

f32 InstanceAttention(const void*, ObjectNode* node, InstanceContext* instance)
{
    constexpr u32 FacingRow = 2;
    f32 attention = 0.0f;
    auto* other = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    if (other != nullptr && CallVirtual<u32>(other, other->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        other = nullptr;
    }

    if (other != nullptr && other->perception != nullptr && PerceptionValue(other->perception, AttentionSlot, &attention) != 0)
    {
        Vector4 toward = PositionOf(node->owner->place);
        ObjectPlace* place = instance->place;
        Vector4 position = PositionOf(place);
        RotateAndTranslate(place);
        Vector4 facing = *RowOf(&place->matrix, FacingRow);
        toward.x = toward.x - position.x;
        toward.y = toward.y - position.y;
        toward.z = toward.z - position.z;
        f32 scale = InverseLength(&toward, LengthEpsilon);
        toward.x = toward.x * scale;
        toward.y = toward.y * scale;
        toward.z = toward.z * scale;
        attention = attention * (facing.x * toward.x + facing.y * toward.y + facing.z * toward.z);
    }

    return attention + attention;
}
