#include "game/objectnode.h"

#include "game/memory.h"

// A chunk's two lists of its rigid bodies (made the first time a body goes in): a body put in at the end (the lists taken when it
// has none) and taken out (the last one moved into its place), the lists destroyed and emptied, and their frame's collisions

namespace
{
// The lists' room, and the indexes of a body in neither
constexpr u16 MostListed = 0xFF;
constexpr u16 NoFirstIndex = 0xFFFF;
constexpr u8 NoSecondIndex = 0xFF;
// The rigid body's bit 44: it's in the first list
constexpr u64 InFirstList = u64{1} << 44;
// The kind of a rigid body's motion (bits 32-35 of its 64 bits): from 9 on its physics body moves it
constexpr u32 MotionKindShift = 32;
constexpr u64 KindMask = 0xF;
constexpr u32 PhysicsKinds = 9;
// The object node's flag 3: it was updated this frame; the instance's flag 6: it's attached to its parent
constexpr u32 NodeUpdated = 0x8;
constexpr u32 AttachedFlag = 0x40;

// A body collides this frame while its node was updated or its instance is attached
bool Collides(const ObjectNode* node)
{
    return (node->flags & NodeUpdated) != 0 || (node->owner->flags & AttachedFlag) != 0;
}

void ForgetFirstList(ObjectRigidBody* body)
{
    body->firstIndex = NoFirstIndex;
    body->bits88 &= ~InFirstList;
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
    for (u32 i = 0; i < bodies->firstCount; i++)
    {
        ForgetFirstList(bodies->first[i]);
    }

    for (u32 i = 0; i < bodies->secondCount; i++)
    {
        bodies->second[i]->secondIndex = NoSecondIndex;
    }

    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(bodies);
    }
}

u32 PutFirstRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->firstCount >= MostListed)
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
    if (index == NoFirstIndex)
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
    for (s32 i = 0; i < bodies->firstCount; i++)
    {
        ForgetFirstList(bodies->first[i]);
    }

    for (s32 i = 0; i < bodies->secondCount; i++)
    {
        ForgetSecondIndex(bodies->second[i]);
    }

    bodies->secondCount = 0;
    bodies->firstCount = 0;
}

u32 PutSecondRigidBody(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->secondCount >= MostListed)
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
    if (index == NoSecondIndex)
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
        if (body == nullptr || (body->bits88 >> MotionKindShift & KindMask) >= PhysicsKinds || !Collides(body->node))
        {
            continue;
        }

        ObjectNode* node = body->node;
        Vector4 sphere = node->unknown20;
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
