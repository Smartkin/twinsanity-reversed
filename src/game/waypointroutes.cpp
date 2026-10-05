#include "game/objectnode.h"

#include "game/array.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/navigation.h"
#include "game/place.h"

// An object node's waypoints copied from another's (a spawned instance's from its spawner's), and the position of a step of the
// route the waypoints follow, tuned by the commands that go there

// RouteStepPosition has more arguments than n32 passes in registers: the asm hands it four integers in $a0-$a3 and five floats in
// $f12-$f16, the C++ function takes the floats in $f16-$f19 and the last one on the stack
asm(R"(
    .pushsection .text.FUN_0020b6e8, "ax", @progbits
    .globl FUN_0020b6e8
    .type FUN_0020b6e8, @function
    .set push
    .set noreorder
FUN_0020b6e8:
    addiu $sp, $sp, -0x10
    sd $ra, 0x8($sp)
    swc1 $f16, 0x0($sp)
    mov.s $f19, $f15
    mov.s $f18, $f14
    mov.s $f17, $f13
    jal FUN_0020b6e8_n32
    mov.s $f16, $f12
    ld $ra, 0x8($sp)
    jr $ra
    addiu $sp, $sp, 0x10
    .set pop
    .size FUN_0020b6e8, . - FUN_0020b6e8
    .popsection
)");

namespace
{
// How high the lift raises the point per unit
constexpr f32 LiftHeight = 4.5f;

// The vector's x, y and z times a number
void Scale(Vector4* vector, f32 scale)
{
    vector->x = vector->x * scale;
    vector->y = vector->y * scale;
    vector->z = vector->z * scale;
}

void Add(Vector4* vector, const Vector4* other)
{
    vector->x = vector->x + other->x;
    vector->y = vector->y + other->y;
    vector->z = vector->z + other->z;
}

// The way from a point to another made a unit one (its length 0 under the epsilon: it stays as it is)
void Normalize(Vector4* vector)
{
    Scale(vector, InverseLength(vector, LengthEpsilon));
}

Vector4 Difference(const Vector4* to, const Vector4* from)
{
    Vector4 difference = *to;
    difference.x = to->x - from->x;
    difference.y = to->y - from->y;
    difference.z = to->z - from->z;
    return difference;
}

// a × b
Vector4 Cross(const Vector4* a, const Vector4* b)
{
    return {a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x, 1.0f};
}

Vector4 Scaled(const Vector4* vector, f32 scale)
{
    return {vector->x * scale, vector->y * scale, vector->z * scale, 1.0f};
}
}

void CopyKeys(Waypoints* waypoints, const Waypoints* other)
{
    waypoints->firstKey = other->firstKey;
    waypoints->lastKey = other->lastKey;
    waypoints->positions.count = 0;
    waypoints->keyCount = other->keyCount;
    if (waypoints->keyCount != 0)
    {
        s32 index = 0;
        do
        {
            // (The other's key, none past its count)
            LayoutPosition* key = index < other->keyCount ? other->positions.data[index] : nullptr;
            waypoints->positions.Append(key);
            index++;
        } while (index < waypoints->keyCount);
    }

    waypoints->flags.wrapped = other->flags.wrapped;
    waypoints->flags.stopped = other->flags.stopped;
    waypoints->flags.backwards = other->flags.backwards;

    waypoints->key = other->key;
}

void CopyPaths(Waypoints* waypoints, const Waypoints* other)
{
    waypoints->pathCount = other->pathCount;
    waypoints->paths.count = 0;
    if (waypoints->pathCount != 0)
    {
        s32 index = 0;
        do
        {
            waypoints->paths.Append(other->paths.data[index]);
            index++;
        } while (index < waypoints->pathCount);
    }

    waypoints->pathDirection = other->pathDirection;
    waypoints->pathParameter = other->pathParameter;
}

// The position of the route's current step (or of the one before it), moved within the room the step's radius leaves the node's
// roll radius: into the corner (along the middle of the ways back to the step before it and to the instance), to a random side,
// toward the instance, across the way to it and up, each by its share of the room (the lift by 4.5 units), then brought back
// within the room across when it went further. Without a route the position is left as it was
void RouteStepPosition(Waypoints* waypoints, Vector4* position, ObjectNode* node, u32 current, f32 corner, f32 toward,
                       f32 scatter, f32 sideways, f32 lift)
{
    Route* route = waypoints->route;
    if (route == nullptr)
    {
        return;
    }

    u32 index = current != 0 || waypoints->routeIndex == 0 ? waypoints->routeIndex : waypoints->routeIndex - 1;
    AiPosition* step = route->PositionAt(index);
    Vector4 point = step->position;
    point.w = 1.0f;
    f32 rollRadius = node->rollRadius;
    ObjectPlace* place = node->owner->place;
    f32 room = step->position.w - rollRadius;
    place->SyncPosition();
    Vector4 here = place->position;
    bool moved = false;
    if (corner != 0.0f && static_cast<s32>(index) - 1 >= 0)
    {
        Vector4 back = waypoints->route->PositionAt(index - 1)->position;
        back.w = 1.0f;
        back = Difference(&back, &point);
        Normalize(&back);
        Vector4 out = Difference(&here, &point);
        Normalize(&out);
        Vector4 middle = back;
        Add(&middle, &out);
        Scale(&middle, 0.5f);
        f32 inverse = InverseLength(&middle, LengthEpsilon);
        f32 distance = room * corner;
        Vector4 offset = Scaled(&middle, inverse);
        Scale(&offset, distance);
        Add(&point, &offset);
        moved = true;
    }

    if (scatter != 0.0f)
    {
        f32 x = RandomSignedTimes(1.0f);
        f32 z = RandomSignedTimes(1.0f);
        Vector4 side = {x, 0.0f, z, 1.0f};
        f32 inverse = InverseLength(&side, LengthEpsilon);
        f32 distance = room * scatter;
        Vector4 offset = Scaled(&side, inverse);
        Scale(&offset, distance);
        Add(&point, &offset);
        moved = true;
    }

    if (sideways > 0.0f || lift > 0.0f || toward > 0.0f)
    {
        Vector4 way = Difference(&here, &point);
        way.w = 1.0f;
        f32 inverse = InverseLength(&way, LengthEpsilon);
        way.x = way.x * inverse;
        // A retail bug: the way's y is the point's height (not its own y times the inverse), so a push toward the instance moves
        // the point up by its height times the room and the push
        way.y = point.y;
        way.z = way.z * inverse;
        moved = true;
        Vector4 push = Scaled(&way, room);
        Vector4 move = Scaled(&push, toward);
        const Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
        Vector4 across = Cross(&way, &up);
        across = Scaled(&across, room);
        across = Scaled(&across, sideways);
        Add(&move, &across);
        Vector4 raise = Scaled(&up, LiftHeight);
        raise = Scaled(&raise, lift);
        Add(&move, &raise);
        Add(&point, &move);
    }

    if (moved)
    {
        Vector4 at = step->position;
        at.w = 1.0f;
        f32 dx = at.x - point.x;
        f32 dz = at.z - point.z;
        if (room * room < dx * dx + dz * dz)
        {
            point = Difference(&point, &at);
            point.w = 1.0f;
            Normalize(&point);
            Scale(&point, room);
            Add(&point, &at);
        }
    }

    *position = point;
}
