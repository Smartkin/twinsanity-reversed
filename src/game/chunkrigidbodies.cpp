#include "game/objectnode.h"

#include "game/memory.h"

// A chunk's two lists of its rigid bodies (made the first time a body goes in): a body put in at the end (the lists taken when it
// has none) and taken out (the last one moved into its place), the lists destroyed and emptied, and their frame's collisions

namespace
{
// A body collides this frame while its node was updated or its instance is attached
bool Collides(const ObjectNode* node)
{
    return node->flags.updated || node->owner->flags.attached;
}

void ForgetFirstList(ObjectRigidBody* body)
{
    body->firstIndex = ObjectRigidBody::NoFirstIndex;
    body->bits.inFirstList = 0;
}
}

ChunkRigidBodies* ConstructChunkRigidBodies(void* memory)
{
    auto* bodies = static_cast<ChunkRigidBodies*>(memory);
    bodies->firstCount = 0;
    bodies->secondCount = 0;
    return bodies;
}

void DestroyChunkRigidBodies(ChunkRigidBodies* bodies, u32 destroyFlags)
{
    for (u32 index = 0; index < bodies->firstCount; index++)
    {
        ForgetFirstList(bodies->first[index]);
    }

    for (u32 index = 0; index < bodies->secondCount; index++)
    {
        bodies->second[index]->secondIndex = ObjectRigidBody::NoSecondIndex;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(bodies);
    }
}

u32 PutFirstRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->firstCount >= ChunkRigidBodies::MostBodies)
    {
        return 0;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->firstIndex = bodies->firstCount;
    bodies->first[bodies->firstCount++] = body;
    return 1;
}

u32 TakeFirstRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u16 index = body->firstIndex;
    if (index == ObjectRigidBody::NoFirstIndex)
    {
        return 0;
    }

    bodies->firstCount--;
    ObjectRigidBody* last = bodies->first[bodies->firstCount];
    bodies->first[index] = last;
    last->firstIndex = index;
    ForgetFirstIndex(body);
    return 1;
}

// The bodies' indexes forgotten (the second list's lists too once they're in neither) and both lists emptied
void ReleaseChunkRigidBodies(ChunkRigidBodies* bodies)
{
    for (s32 index = 0; index < bodies->firstCount; index++)
    {
        ForgetFirstList(bodies->first[index]);
    }

    for (s32 index = 0; index < bodies->secondCount; index++)
    {
        ForgetSecondIndex(bodies->second[index]);
    }

    bodies->secondCount = 0;
    bodies->firstCount = 0;
}

u32 PutSecondRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->secondCount >= ChunkRigidBodies::MostBodies)
    {
        return 0;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->secondIndex = static_cast<u8>(bodies->secondCount);
    bodies->second[bodies->secondCount++] = body;
    return 1;
}

u32 TakeSecondRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u8 index = body->secondIndex;
    if (index == ObjectRigidBody::NoSecondIndex)
    {
        return 0;
    }

    bodies->secondCount--;
    ObjectRigidBody* last = bodies->second[bodies->secondCount];
    bodies->second[index] = last;
    last->secondIndex = index;
    ForgetSecondIndex(body);
    return 1;
}

void CollideChunkRigidBodies(ChunkRigidBodies* bodies)
{
    for (u32 index = 0; index < bodies->firstCount; index++)
    {
        ObjectRigidBody* body = bodies->first[index];
        if (body == nullptr || body->bits.motionKind >= FirstPhysicsBodyKind || !Collides(body->node))
        {
            continue;
        }

        ObjectNode* node = body->node;
        Vector4 sphere = node->middle;
        sphere.w = node->rollRadius;
        CollideRigidBodyWithInstances(body, &sphere);
    }

    for (u32 index = 0; index < bodies->secondCount; index++)
    {
        ObjectRigidBody* body = bodies->second[index];
        if (body != nullptr && Collides(body->node))
        {
            CollideRigidBodyWithWorld(body);
        }
    }
}
