#include "game/clock.h"
#include "game/instances.h"
#include "game/place.h"

// The movement node's construction, destructor, update, kind and class, and velocity (its places' capture is in instances.cpp)

GameNode* ConstructMovementNode(void* memory)
{
    auto* node = static_cast<MovementNode*>(memory);
    GameNode::Construct(node);
    node->seconds = 0.0f;
    node->vtable = g_MovementNodeVTable;
    node->bits.carries = 1;
    node->bits.captured = 0;
    return node;
}

void MovementNode::Destroy(u32 destroyFlags)
{
    vtable = g_MovementNodeVTable;
    GameNode::Destroy(destroyFlags);
}

u32 MovementNode::Update(TimeClock* clock)
{
    if (clock->flags.running != 0 && bits.captured != 0)
    {
        previousMatrix = matrix;
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        matrix = place->matrix;
        seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    }
    else
    {
        Capture();
    }

    return GameNode::Update(clock);
}

u32 MovementNode::Kind()
{
    return NodeMovement;
}

u32 MovementNode::GetClassId()
{
    return ClassId;
}

void MovementVelocity(MovementNode* node, Vector4* velocity)
{
    if (node->bits.captured == 0)
    {
        node->Capture();
    }

    *velocity = *RowOf(&node->matrix, 3);
    const Vector4* before = RowOf(&node->previousMatrix, 3);
    f32 x = velocity->x - before->x;
    velocity->x = x;
    f32 y = velocity->y - before->y;
    velocity->y = y;
    f32 z = velocity->z - before->z;
    velocity->z = z;
    f32 inverse = 1.0f / node->seconds;
    velocity->x = x * inverse;
    velocity->y = y * inverse;
    velocity->z = z * inverse;
}
