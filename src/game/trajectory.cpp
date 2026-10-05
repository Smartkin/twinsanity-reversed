#include "game/objectnode.h"

#include "game/attachments.h"
#include "game/behaviours.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/navigation.h"
#include "game/place.h"
#include "game/player.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/rigidbody.h"

#include <cstddef>
#include <cstdint>

// An object node's trajectory controller, which follows a motion block: cycles about three axes move or turn the instance
// (stepped on their own, or in step with another instance's), a body of the node's own carries it (a body block's held by springs
// at its stored place and put back there once it rests stuck far from the player; a ball; one that grabs what it touches), or it
// looks among the AI positions around for the one that hides the instance best from the player

namespace
{
// The most links a cover position may have
constexpr u32 MostCoverLinks = 4;
// The most instances the query of its line of sight to the player takes
constexpr u16 MostSightInstances = 0x80;

// A body resting far from the player that long (frames, after the first second and a half) is put back
constexpr u32 StuckFrames = 48;
constexpr f32 StuckDelay = 1.5f;
constexpr f32 RestingSquared = Rounded(0.4);
// What a grabber that took hold of something is made: its size and drags
constexpr f32 GrabbedSize = 3.0f;
constexpr f32 GrabbedDrag = Rounded(0.03);
constexpr f32 GrabbedLengthDrag = Rounded(0.01);

bool IsAsleep(const InstanceContext* instance)
{
    return instance->flags.asleep;
}

// The player's instance (none without a player)
InstanceContext* PlayerInstance()
{
    return g_PlayerInstance != nullptr ? static_cast<InstanceContext*>(g_PlayerInstance->object) : nullptr;
}

// The place of an instance that may be none (retail reads the word at address 8 then)
ObjectPlace* RetailPlaceOf(const InstanceContext* instance)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(instance) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

// A place's position, worked out from its matrix first when it moved
Vector4 PositionOf(ObjectPlace* place)
{
    place->SyncPosition();
    return place->position;
}

// The vector of x, y and z along a matrix's rows added to a position
void AddAlongRows(Vector4* position, const Matrix4x4* matrix, const Vector4* by)
{
    position->x = position->x + (matrix->m[0][0] * by->x + matrix->m[1][0] * by->y + matrix->m[2][0] * by->z);
    position->z = position->z + (matrix->m[0][2] * by->x + matrix->m[1][2] * by->y + matrix->m[2][2] * by->z);
    position->y = position->y + (matrix->m[0][1] * by->x + matrix->m[1][1] * by->y + matrix->m[2][1] * by->z);
}

// An axis scaled by a length added to a position, or taken from it
void AddScaled(Vector4* position, const Vector4* axis, f32 length)
{
    f32 x = axis->x * length;
    f32 y = axis->y * length;
    f32 z = axis->z * length;
    position->x = position->x + x;
    position->y = position->y + y;
    position->z = position->z + z;
}

void SubtractScaled(Vector4* position, const Vector4* axis, f32 length)
{
    f32 x = axis->x * length;
    f32 y = axis->y * length;
    f32 z = axis->z * length;
    position->x = position->x - x;
    position->y = position->y - y;
    position->z = position->z - z;
}
}

extern "C"
{
    // The trajectory's parts only it uses: a body block's body made for the node (its rigid body made again, in its chunk's
    // lists, given the block's values; the stored place the instance's), its body turned toward the focus by a strength (the
    // torque across from the place's z axis to the focus), a cover search's step (the next position looked at: when the player
    // can't see a spot the instance's height above it, it's scored by how near the instance it is, how much further from the
    // player and how little the line from it is exposed; once all are looked at the best is the stored position: whether the
    // search is over), its cycles kept in step with another trajectory's (their angles the other's plus their phases), and a
    // ball's or a grabber's contacts (what it touches grabbed, else its body kept facing along its instance's z axis)
    void MakeTrajectoryBody(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_00238850);
    void TurnBodyTowardFocus(f32 strength, Trajectory* unused, DynamicBody* body, ObjectNode* node) RETAIL_N32(FUN_0023a390);
    u32 StepCoverSearch(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023a600);
    void FollowOtherCycles(Trajectory* trajectory, const Trajectory* other, f32* x, f32* y, f32* z) RETAIL(FUN_0023aaa8);
    void StepTrajectoryContact(Trajectory* trajectory, ObjectNode* node) RETAIL(FUN_0023bb70);

    // A rigid body's size (the float first; game/commandsphysics.cpp has it and its thunk)
    void SetRigidBodySize(f32 size, ObjectRigidBody* body) RETAIL_N32(FUN_00246a88);

    // A quadword copied
    Vector4* CopyQuadword(Vector4* to, const Vector4* from) RETAIL(MovePositionFromPos2ToPos1);
    // The node kinds whose instances' bodies keep their gravity (the start-up sets them)
    extern u32 g_GravityKinds RETAIL(D_0030A120);
}

EABI_EXPORT(FUN_0023a390, TurnBodyTowardFocus);

namespace
{
// The entry of an instance on the attachments' path of what it hangs from (none without a path)
Attachment* AttachmentOf(InstanceContext* parent, InstanceContext* instance)
{
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&parent->nodes, NodeAttachments));
    if (attachments->path == nullptr)
    {
        return nullptr;
    }

    return AttachmentOn(attachments->path, instance);
}

// The trajectory of an instance's object node when it takes packets (none otherwise)
Trajectory* TrajectoryOf(InstanceContext* instance)
{
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    if (CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) == 0)
    {
        return nullptr;
    }

    return node->trajectory;
}

// The node's trajectory controller let go of and destroyed
void ReleaseNodeTrajectory(ObjectNode* node)
{
    if (node->trajectory != nullptr)
    {
        LetGoOfTrajectory(node->trajectory, node);
        if (node->trajectory != nullptr)
        {
            DestroyTrajectory(node->trajectory, DestroyAndFree);
        }
    }

    node->trajectory = nullptr;
}

// A place turned about its x, y and z axes by the angles: whether any turn did
u32 TurnAboutAxes(ObjectPlace* place, const s32* angles)
{
    s32 angle = angles[0];
    u32 turned = TurnPlaceAboutX(place, &angle) != 0;
    angle = angles[1];
    turned |= TurnPlaceAboutY(place, &angle) != 0;
    angle = angles[2];
    turned |= TurnPlaceAboutZ(place, &angle) != 0;
    return turned;
}

// The rotation of angles of radians about x, y and z
void RotationOfRadians(Vector4* rotation, const f32* radians)
{
    s32 x;
    s32 y;
    s32 z;
    AngleFrom(&x, radians[0], AngleRadians);
    AngleFrom(&y, radians[1], AngleRadians);
    AngleFrom(&z, radians[2], AngleRadians);
    GetRotationXYZ(rotation, &x, &y, &z);
}

// A body block's body stuck: put back where the trajectory holds it (the stored place's position and rotation when its node's
// motion moves the stored place), its sound stopped, its node no longer moving the stored place and its physics body riding again
void PutBackStuckBody(Trajectory* trajectory, ObjectNode* node, InstanceContext* instance, DynamicBody* physics)
{
    if (trajectory->followed->motion.putsBackStuck)
    {
        if (node->flags.movesStoredPlace)
        {
            ObjectPlace* stored = node->storedPlace;
            if (stored != nullptr)
            {
                Vector4 position = PositionOf(stored);
                InstanceContext* owner = node->owner;
                ObjectPlace* place = owner->place;
                place->SyncPosition();
                if (place->MoveTo(&position))
                {
                    QueueObject(owner);
                }

                stored = node->storedPlace;
                if (stored != nullptr)
                {
                    stored->SyncRotation();
                    Vector4 rotation = stored->rotation;
                    owner = node->owner;
                    place = owner->place;
                    place->SyncRotation();
                    if (place->TurnTo(&rotation))
                    {
                        QueueObject(owner);
                    }
                }
            }
        }
        else
        {
            ObjectPlace* place = instance->place;
            place->SyncPosition();
            if (place->MoveTo(&trajectory->position))
            {
                QueueObject(instance);
            }

            place = instance->place;
            place->SyncRotation();
            if (place->TurnTo(&trajectory->rotation))
            {
                QueueObject(instance);
            }
        }
    }

    trajectory->bits.count = 0;
    CallVirtual<void>(node, node->vtable, ObjectNode::StopSoundSlot);
    node->flags.movesStoredPlace = 0;
    physics->StartRide();
}

// A body block's springs: four, at the corners of the body's reach across its stored place's x and z axes, pull it level toward
// the place (the axes kept)
void PullBodySprings(Trajectory* trajectory, ObjectNode* node, DynamicBody* physics)
{
    const MotionBlock* block = trajectory->followed;
    ObjectPlace* stored = node->storedPlace;
    f32 damping = block->springDamping;
    f32 stiffness = block->springStiffness;
    Vector4 corner = trajectory->reach;
    f32 depth = corner.z + corner.z;
    f32 width = corner.x + corner.x;
    RotateAndTranslate(stored);
    trajectory->axisX = *RowOf(&stored->matrix, 0);
    RotateAndTranslate(stored);
    trajectory->axisZ = *RowOf(&stored->matrix, 2);
    RotateAndTranslate(stored);
    trajectory->axisY = *RowOf(&stored->matrix, 1);
    Vector4 target = PositionOf(stored);
    AddScaled(&target, &trajectory->axisX, corner.x);
    AddScaled(&target, &trajectory->axisZ, corner.z);
    AddScaled(&target, &trajectory->axisY, corner.y);
    f32 across = trajectory->followed->springAcross;
    physics->LevelSpring(stiffness, damping, across, &target, &corner);
    corner.z = corner.z - depth;
    SubtractScaled(&target, &trajectory->axisZ, depth);
    physics->LevelSpring(stiffness, damping, across, &target, &corner);
    corner.x = corner.x - width;
    SubtractScaled(&target, &trajectory->axisX, width);
    physics->LevelSpring(stiffness, damping, across, &target, &corner);
    corner.z = corner.z + depth;
    AddScaled(&target, &trajectory->axisZ, depth);
    physics->LevelSpring(stiffness, damping, across, &target, &corner);
}

// The frame of a body's trajectory (kinds 1 to 3, their node's rigid body having a physics body): a body block's body put back
// once it rested, far from the player and untouched by the player, for 48 frames; else the motion's velocity the body's (the last
// one its start), the body turned toward the focus, a body block's springs pulled or a ball's or a grabber's contacts, and the
// body slowed along an axis
void StepBodyTrajectory(Trajectory* trajectory, TimeClock* clock, ObjectNode* node, u32 kind)
{
    ObjectRigidBody* body = node->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    DynamicBody* physics = body->physicsBody;
    if (physics == nullptr)
    {
        return;
    }

    InstanceContext* instance = node->owner;
    const MotionBlock* block = trajectory->followed;
    if (!block->body.neverPutBack && physics->bodyFlags.leftOut == 0
        && (trajectory->bits.madeBody || physics->bodyFlags.touchedWorld != 0 || physics->bodyFlags.touchedBody != 0))
    {
        InstanceContext* player = PlayerInstance();
        Vector4 position = PositionOf(instance->place);
        Vector4 seen = player != nullptr ? PositionOf(player->place) : position;
        f32 x = position.x - seen.x;
        f32 y = position.y - seen.y;
        f32 z = position.z - seen.z;
        f32 seconds = static_cast<f32>(static_cast<s32>(clock->time - trajectory->startTime)) * g_SecondsPerClockUnit;
        if (trajectory->farDistance < x * x + y * y + z * z && StuckDelay < seconds)
        {
            const Vector4& velocity = physics->lastVelocity;
            const Vector4& spin = physics->angularVelocity;
            if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < RestingSquared
                && spin.x * spin.x + spin.y * spin.y + spin.z * spin.z < RestingSquared)
            {
                trajectory->bits.count++;
                if (trajectory->bits.count > StuckFrames)
                {
                    PutBackStuckBody(trajectory, node, instance, physics);
                    return;
                }
            }
            else
            {
                trajectory->bits.count = 0;
            }
        }
    }

    MotionState* motion = trajectory->node->motion;
    motion->startVelocity = motion->velocity;
    motion->velocity = physics->velocity;
    if (0.0f < trajectory->followed->turnStrength)
    {
        TurnBodyTowardFocus(trajectory->followed->turnStrength, trajectory, physics, node);
    }

    if (kind == MotionBlock::KindBody)
    {
        PullBodySprings(trajectory, node, physics);
    }
    else if (kind == MotionBlock::KindBall || kind == MotionBlock::KindGrabber)
    {
        if (node->flags.seeksContact)
        {
            StepTrajectoryContact(trajectory, node);
        }
    }

    block = trajectory->followed;
    u32 axis = block->body.slowedAxis;
    if (axis != 0)
    {
        SlowBodyAlongAxis(block->slowing, trajectory, physics, axis);
    }
}

// Where the cycles move the instance to (the cycles' values along the axes of its space)
void SetCycleMove(Trajectory* trajectory, ObjectNode* node, InstanceContext* instance, const Vector4* by)
{
    switch (trajectory->followed->motion.space)
    {
    case SpaceStart:
    {
        CopyQuadword(&trajectory->move, &g_DefaultBox.min);
        trajectory->move.w = 1.0f;
        Matrix4x4 matrix;
        MatrixFromRotation(&matrix, &node->informationPointer->rotation);
        AddAlongRows(&trajectory->move, &matrix, by);
        return;
    }
    case SpaceOwn:
    {
        CopyQuadword(&trajectory->move, &g_DefaultBox.min);
        trajectory->move.w = 1.0f;
        ObjectPlace* place = instance->place;
        RotateAndTranslate(place);
        AddAlongRows(&trajectory->move, &place->matrix, by);
        return;
    }
    case SpaceTracked:
    {
        trajectory->move = trajectory->position;
        Matrix4x4 matrix;
        MatrixFacingFrom(instance, node->tracked, &matrix);
        AddAlongRows(&trajectory->move, &matrix, by);
        return;
    }
    case SpaceStored:
    {
        // The stored place's move is added to the last one rather than made anew
        ObjectPlace* stored = node->storedPlace;
        if (stored != nullptr)
        {
            RotateAndTranslate(stored);
            AddAlongRows(&trajectory->move, &stored->matrix, by);
            return;
        }

        break;
    }
    default:
        break;
    }

    trajectory->move = *by;
}
}

Trajectory* ConstructTrajectory(Trajectory* trajectory)
{
    constexpr f32 FirstRollRate = 4.0f;
    trajectory->rollRate = FirstRollRate;
    trajectory->cover = nullptr;
    trajectory->bits.unused16 = 0;
    trajectory->bits.madeBody = 0;
    trajectory->bits.cycleSource = OwnCycles;
    trajectory->bits.unused18 = 1;
    trajectory->unusedD4 = 0;
    trajectory->coverRoute = nullptr;
    trajectory->followed = nullptr;
    return trajectory;
}

void StartFollowing(Trajectory* trajectory, MotionBlock* block, TimeClock* clock, ObjectNode* node)
{
    if (trajectory->followed != nullptr)
    {
        if (trajectory->followed->flags.sticky)
        {
            block->flags.sticky = 1;
        }

        block->stickyKinds |= trajectory->followed->stickyKinds;
    }

    trajectory->followed = block;
    trajectory->bits.count = 0;
    trajectory->startTime = clock->time;
    switch (block->motion.kind)
    {
    case MotionBlock::KindCycles:
    {
        const MotionBlock* cycling = trajectory->followed;
        for (u32 axis = 0; axis < 3; axis++)
        {
            trajectory->motionFloats[axis] = cycling->cycleRanges[axis];
        }

        for (u32 axis = 0; axis < 3; axis++)
        {
            f32 phase = cycling->cyclePhases[axis];
            if (cycling->motion.StartsAtRandom(axis))
            {
                phase = RandomAround(0.0f, cycling->cyclePhases[axis]);
            }

            AngleFrom(&trajectory->cycles[axis], phase, AngleRadians);
        }

        for (u32 axis = 0; axis < 3; axis++)
        {
            trajectory->wobblePhases[axis] = trajectory->cycles[axis];
        }

        trajectory->bits.cycleSource = trajectory->followed->flags.cycleSource;
        trajectory->rollRate = 1.0f / node->rollRadius;
        break;
    }
    case MotionBlock::KindBody:
        trajectory->bits.madeBody = 1;
        MakeTrajectoryBody(trajectory, node);
        break;
    case MotionBlock::KindBall:
        StartBall(trajectory, node);
        break;
    case MotionBlock::KindGrabber:
        StartGrabber(trajectory, node);
        break;
    case MotionBlock::KindCover:
        StartCoverSearch(trajectory, node);
        break;
    default:
        break;
    }

    if (trajectory->followed->mover == MotionBlock::MoverOwnInstance)
    {
        InstanceContext* instance = node->owner;
        InstanceContext* parent = instance->parent;
        if (parent != nullptr)
        {
            // An instance missing from its parent's path is read at address 0 (retail)
            Attachment* entry = AttachmentOf(parent, instance);
            trajectory->position = *RowOf(&entry->offset, 3);
            GetRotationVec(&trajectory->rotation, &entry->offset);
        }
        else
        {
            trajectory->position = PositionOf(instance->place);
            ObjectPlace* place = instance->place;
            place->SyncRotation();
            trajectory->rotation = place->rotation;
        }
    }

    StepTrajectory(trajectory, clock, node);
}

void GatherCoverPositions(Trajectory* trajectory, ObjectNode* node)
{
    const MotionBlock* block = trajectory->followed;
    f32 height = block->halfHeight;
    f32 width = block->halfWidth;
    Box box = {{-width, -height, -width, 1.0f}, {width, height, width, 1.0f}};
    Vector4 position = PositionOf(node->owner->place);
    box.max.x = box.max.x + position.x;
    box.max.y = box.max.y + position.y;
    box.max.z = box.max.z + position.z;
    box.min.x = box.min.x + position.x;
    box.min.y = box.min.y + position.y;
    box.min.z = box.min.z + position.z;
    if (trajectory->coverRoute == nullptr)
    {
        trajectory->coverRoute = static_cast<Route*>(MemoryAllocate(sizeof(Route)));
        trajectory->coverRoute->count = 0;
    }

    trajectory->bits.count = g_PathFinder->CollectInside(&box, trajectory->coverRoute);
}

void MakeTrajectoryBody(Trajectory* trajectory, ObjectNode* node)
{
    InstanceContext* instance = node->owner;
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    trajectory->rotation = place->rotation;
    trajectory->reach = instance->collision.ownBox.max;
    const Vector4& reach = trajectory->reach;
    f32 length = Kept(__builtin_sqrtf(reach.x * reach.x + reach.y * reach.y + reach.z * reach.z));
    f32 twice = length + length;
    trajectory->farDistance = twice * twice;
    place = instance->place;
    RotateAndTranslate(place);
    trajectory->axisX = *RowOf(&place->matrix, 0);
    place = instance->place;
    RotateAndTranslate(place);
    trajectory->axisZ = *RowOf(&place->matrix, 2);
    place = instance->place;
    RotateAndTranslate(place);
    trajectory->axisY = *RowOf(&place->matrix, 1);

    // The body: one of kind 10 in the chunk's first list, and in the second unless it has no collisions
    constexpr u32 BodyKind = 10;
    node->ReleaseRigidBody();
    ObjectRigidBody* body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
    node->rigidBody = body;
    ListRigidBodyFirst(body, BodyKind);
    if (trajectory->followed->body.noCollisions)
    {
        StopRigidBodyCollisions(body);
        DynamicBody* physics = body->physicsBody;
        if (physics != nullptr)
        {
            physics->bits.noCollisions = 1;
        }
    }
    else
    {
        ListRigidBodySecond(body, BodyKind);
    }

    SetRigidBodyDrag(trajectory->followed->drag, body);
    SetRigidBodyMass(node->PacketProperties()->GetFloat(RigidBodyMassProperty), body);
    SetUpTrajectoryBody(trajectory, body);
    node->flags.movesStoredPlace = 1;
    place = node->owner->place;
    if (node->storedPlace != nullptr)
    {
        CopyObjectPlace(node->storedPlace, place);
    }
    else
    {
        node->storedPlace = AssignObjectPlace(static_cast<ObjectPlace*>(MemoryAllocate(sizeof(ObjectPlace))), place);
    }

    if ((node->owner->nodes.mask & g_GravityKinds) == 0)
    {
        ClearRigidBodyGravity(body);
    }
}

void SetUpTrajectoryBody(Trajectory* trajectory, ObjectRigidBody* body)
{
    DynamicBody* physics = body->physicsBody;
    if (trajectory->followed->drag != 0.0f)
    {
        SetRigidBodyDrag(trajectory->followed->drag, body);
    }

    if (trajectory->followed->gravity != 0.0f)
    {
        SetRigidBodyGravity(trajectory->followed->gravity, body);
    }

    if (trajectory->followed->size != 0.0f)
    {
        SetRigidBodySize(trajectory->followed->size, body);
    }

    if (trajectory->followed->friction != 0.0f)
    {
        SetRigidBodyFriction(trajectory->followed->friction, body);
    }

    if (trajectory->followed->restitution != 0.0f)
    {
        SetRigidBodyRestitution(trajectory->followed->restitution, body);
    }

    if (trajectory->followed->spinFriction != 0.0f)
    {
        SetRigidBodySpinFriction(trajectory->followed->spinFriction, body);
    }

    if (trajectory->followed->lengthDrag != 0.0f)
    {
        SetRigidBodyLengthDrag(trajectory->followed->lengthDrag, body);
    }

    const MotionBlock* block = trajectory->followed;
    if (block->flags.hasCenterOfMass)
    {
        Vector4 center = {block->centerOfMass[0], block->centerOfMass[1], block->centerOfMass[2], 1.0f};
        SetRigidBodyCenterOfMass(body, &center);
    }

    physics->knockScale = trajectory->followed->knockScale;
    u32 substeps = trajectory->followed->body.substeps;
    physics->substeps = static_cast<s32>(substeps);
    f32 buoyancy = -1.0f;
    if (trajectory->followed->body.floats)
    {
        buoyancy = trajectory->followed->springStiffness;
    }

    if (0.0f < buoyancy)
    {
        physics->buoyancy = buoyancy;
    }

    if (trajectory->followed->body.pushable20)
    {
        physics->bodyFlags.pushable |= 1;
    }

    if (trajectory->followed->body.pushable40)
    {
        physics->bodyFlags.pushable |= 2;
    }

    block = trajectory->followed;
    if (block->body.constraint != 0)
    {
        Vector4 direction = {block->constraint[0], block->constraint[1], block->constraint[2], block->constraint[3]};
        BodyConstraint* constraint = &body->physicsBody->constraint;
        ObjectPlace* place = trajectory->node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &direction, &direction);
        switch (trajectory->followed->body.constraint)
        {
        case MotionBlock::ConstraintFixed:
            FixHere(constraint);
            break;
        case MotionBlock::ConstraintLine:
            KeepOnLine(constraint, &direction);
            break;
        case MotionBlock::ConstraintPlane:
            KeepOnPlaneThrough(constraint, &direction);
            break;
        default:
            break;
        }
    }

    MotionBlockBody bits = trajectory->followed->body;
    if ((bits.value & MotionBlockBody::HingesMask) != 0)
    {
        BodyConstraint* constraint = &body->physicsBody->constraint;
        if (trajectory->followed->body.hingeX && bits.hingeY && bits.hingeZ)
        {
            // Hinged about all three axes: it doesn't turn at all
            physics->bits.placesPositionOnly = 1;
        }
        else
        {
            if (trajectory->followed->body.hingeX)
            {
                Vector4 axis = {1.0f, 0.0f, 0.0f, 1.0f};
                SetLocalHinge(constraint, &axis);
            }

            if (trajectory->followed->body.hingeY)
            {
                Vector4 axis = {0.0f, 1.0f, 0.0f, 1.0f};
                SetLocalHinge(constraint, &axis);
            }

            if (trajectory->followed->body.hingeZ)
            {
                Vector4 axis = {0.0f, 0.0f, 1.0f, 1.0f};
                SetLocalHinge(constraint, &axis);
            }

            if (trajectory->followed->body.limitsTurn)
            {
                SetRotationLimit(trajectory->followed->turnLimit, constraint);
            }
        }
    }

    if (trajectory->followed->body.noCollisions)
    {
        StopRigidBodyCollisions(body);
        if (physics != nullptr)
        {
            physics->bits.noCollisions = 1;
        }
    }
}

void StepTrajectory(Trajectory* trajectory, TimeClock* clock, ObjectNode* node)
{
    const MotionBlock* block = trajectory->followed;
    u32 kind = block->motion.kind;
    InstanceContext* instance = node->owner;
    if (kind == MotionBlock::KindCover)
    {
        if (block->search.kind == MotionBlock::SearchInBox && StepCoverSearch(trajectory, node) != 0)
        {
            ReleaseNodeTrajectory(node);
        }

        return;
    }

    if (kind != MotionBlock::KindCycles)
    {
        StepBodyTrajectory(trajectory, clock, node, kind);
        return;
    }

    f32 elapsed = 0.0f;
    if (node->time != 0)
    {
        elapsed = static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit;
    }

    f32 values[3] = {0.0f, 0.0f, 0.0f};
    u32 source = trajectory->bits.cycleSource;
    u8 mover = trajectory->followed->mover;
    switch (source)
    {
    case OwnCycles:
        if (trajectory->followed->flags.cyclesAboutX)
        {
            values[0] = StepCycleX(elapsed, trajectory->motionFloats[0], trajectory->followed, &trajectory->cycles[0]);
        }

        if (trajectory->followed->flags.cyclesAboutY)
        {
            values[1] = StepCycleY(elapsed, trajectory->motionFloats[1], trajectory->followed, &trajectory->cycles[1]);
        }

        if (trajectory->followed->flags.cyclesAboutZ)
        {
            values[2] = StepCycleZ(elapsed, trajectory->motionFloats[2], trajectory->followed, &trajectory->cycles[2]);
        }

        break;
    case FocusCycles:
    {
        InstanceContext* focus = node->AwakeFocus();
        if (focus != nullptr)
        {
            Trajectory* other = TrajectoryOf(focus);
            if (other != nullptr)
            {
                FollowOtherCycles(trajectory, other, &values[0], &values[1], &values[2]);
            }
        }

        break;
    }
    case AttachedCycles:
    {
        auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, NodeAttachments));
        if (attachments != nullptr && attachments->linked[0] != nullptr)
        {
            Trajectory* other = TrajectoryOf(attachments->linked[0]);
            if (other != nullptr)
            {
                FollowOtherCycles(trajectory, other, &values[0], &values[1], &values[2]);
            }
        }

        break;
    }
    default:
        break;
    }

    block = trajectory->followed;
    if (block->motion.turns)
    {
        u32 space = block->motion.space;
        if (mover == MotionBlock::MoverOwnInstance
            && (space == SpaceStart || space == SpaceOwn || space == SpaceStored))
        {
            trajectory->turnAngles[0] = static_cast<s32>(values[0] * RadiansToAngle);
            trajectory->turnAngles[1] = static_cast<s32>(values[1] * RadiansToAngle);
            trajectory->turnAngles[2] = static_cast<s32>(values[2] * RadiansToAngle);
        }
        else
        {
            RotationOfRadians(&trajectory->turn, values);
        }
    }
    else
    {
        Vector4 by = {values[0], values[1], values[2], 1.0f};
        if (mover == MotionBlock::MoverOwnInstance)
        {
            SetCycleMove(trajectory, node, instance, &by);
        }
        else
        {
            trajectory->move = by;
        }
    }

    // The amplitudes fade out (the trajectory let go of after the duration) or grow in over the duration
    block = trajectory->followed;
    if (block->motion.fades)
    {
        f32 seconds = static_cast<f32>(static_cast<s32>(clock->time - trajectory->startTime)) * g_SecondsPerClockUnit;
        if (block->duration < seconds)
        {
            ReleaseNodeTrajectory(node);
            return;
        }

        f32 faded = block->fadeRate * seconds;
        for (u32 axis = 0; axis < 3; axis++)
        {
            trajectory->motionFloats[axis] = block->cycleRanges[axis] - faded;
        }

        return;
    }

    if (!block->flags.growsIn)
    {
        return;
    }

    f32 seconds = static_cast<f32>(static_cast<s32>(clock->time - trajectory->startTime)) * g_SecondsPerClockUnit;
    f32 t = seconds / block->duration;
    f32 share = t * (t * 3.0f) - (t + t) * t * t;
    for (u32 axis = 0; axis < 3; axis++)
    {
        trajectory->motionFloats[axis] = block->cycleRanges[axis] * share;
    }
}

void RestartTrajectory(Trajectory* trajectory)
{
    trajectory->bits.count = 0;
    InstanceContext* instance = trajectory->node->owner;
    trajectory->startTime = GetContextClock(instance)->time;
    trajectory->position = PositionOf(instance->place);
    ObjectPlace* place = instance->place;
    place->SyncRotation();
    trajectory->rotation = place->rotation;
}

void TurnBodyTowardFocus(f32 strength, Trajectory*, DynamicBody* body, ObjectNode* node)
{
    InstanceContext* focus = nullptr;
    Vector4 target;
    if (node->flags.focusInstance)
    {
        focus = node->AwakeFocus();
        if (focus == nullptr)
        {
            return;
        }
    }
    else if (node->flags.focusPosition)
    {
        target = node->focusPosition;
    }
    else
    {
        return;
    }

    if (focus != nullptr)
    {
        target = PositionOf(focus->place);
    }

    f32 x = target.x - body->matrix.m[3][0];
    f32 y = target.y - body->matrix.m[3][1];
    f32 z = target.z - body->matrix.m[3][2];
    ObjectPlace* place = body->owner->place;
    RotateAndTranslate(place);
    Vector4 forward = *RowOf(&place->matrix, 2);
    body->torque.x = body->torque.x + (y * forward.z - z * forward.y) * strength;
    body->torque.y = body->torque.y + (z * forward.x - x * forward.z) * strength;
    body->torque.z = body->torque.z + (x * forward.y - y * forward.x) * strength;
}

u32 StepCoverSearch(Trajectory* trajectory, ObjectNode* node)
{
    if (trajectory->bits.count == 0)
    {
        if (trajectory->coverRoute != nullptr)
        {
            MemoryDeallocate2_(trajectory->coverRoute);
        }

        trajectory->coverRoute = nullptr;
        return 1;
    }

    trajectory->bits.count--;
    AiPosition* candidate = trajectory->coverRoute->PositionAt(static_cast<u8>(trajectory->bits.count));
    if (candidate->bits.linkCount < MostCoverLinks)
    {
        // Without a player its place, box and position are read at address 0 (retail)
        InstanceContext* player = PlayerInstance();
        Vector4 eye = PositionOf(RetailPlaceOf(player));
        InstanceContext* instance = node->owner;
        Vector4 spot = candidate->position;
        spot.w = 1.0f;
        f32 height = instance->collision.box.max.y - instance->collision.box.min.y;
        f32 playerHalf = (player->collision.box.max.y - player->collision.box.min.y) * 0.5f;
        spot.y = spot.y + height;
        eye.y = eye.y + playerHalf;
        Vector4 way = eye;
        way.x = way.x - spot.x;
        way.y = way.y - spot.y;
        way.z = way.z - spot.z;
        f32 length = Kept(__builtin_sqrtf(way.x * way.x + way.y * way.y + way.z * way.z));

        void* results[MostSightInstances];
        InstanceQuery query;
        query.results = results;
        query.count = 0;
        query.most = MostSightInstances;
        query.distance = NoHitDistance;
        // Retail leaves the bits nothing reads as the stack had them
        query.bits.value = InstanceQueryBits::AllWanted;
        query.wantedFlags = ReferencedObjectFlags::CollisionActive;
        query.unwantedFlags = ReferencedObjectFlags::Asleep;
        query.skipped[0] = nullptr;
        query.skipped[1] = nullptr;
        query.instance = nullptr;
        SkipInQuery(&query, instance);
        query.skipped[1] = player;
        f32 score = 0.0f;
        if (LineOfSight(instance->chunk, &spot, &way, SurfaceFlags::SolidToObjects, &query, g_CoverKinds) != 0)
        {
            constexpr f32 Nearness = 100.0f;
            Vector4 here = PositionOf(node->owner->place);
            f32 x = here.x - eye.x;
            f32 y = here.y - eye.y;
            f32 z = here.z - eye.z;
            f32 away = Kept(__builtin_sqrtf(x * x + y * y + z * z));
            x = spot.x - here.x;
            y = spot.y - here.y;
            z = spot.z - here.z;
            f32 near = x * x + y * y + z * z;
            spot.x = spot.x - eye.x;
            spot.y = spot.y - eye.y;
            spot.z = spot.z - eye.z;
            Vector4 normal = {here.x - spot.x, here.y - spot.y, here.z - spot.z, here.w};
            const MotionBlock* block = trajectory->followed;
            score = Nearness / near + (away / length - 1.0f) * block->distanceWeight;
            f32 scale = InverseLength(&normal, LengthEpsilon);
            normal.x = normal.x * scale;
            normal.y = normal.y * scale;
            normal.z = normal.z * scale;
            score = score + block->exposureWeight / (spot.x * normal.x + spot.y * normal.y + spot.z * normal.z);
        }

        if (trajectory->coverScore < score)
        {
            trajectory->coverScore = score;
            trajectory->cover = candidate;
        }
    }

    if (trajectory->bits.count != 0)
    {
        return 0;
    }

    if (trajectory->coverRoute != nullptr)
    {
        MemoryDeallocate2_(trajectory->coverRoute);
    }

    trajectory->coverRoute = nullptr;
    if (trajectory->cover == nullptr)
    {
        node->flags.searchEnded = 1;
        node->flags.noCover = 1;
        return 0;
    }

    Vector4 position = trajectory->cover->position;
    position.w = 1.0f;
    node->flags.storedPosition = 1;
    node->flags.searchEnded = 1;
    node->flags.foundCover = 1;
    node->storedPosition = position;
    return 1;
}

void FollowOtherCycles(Trajectory* trajectory, const Trajectory* other, f32* x, f32* y, f32* z)
{
    f32* values[3] = {x, y, z};
    for (u32 axis = 0; axis < 3; axis++)
    {
        const MotionBlock* block = trajectory->followed;
        if (!block->flags.CyclesAbout(axis))
        {
            continue;
        }

        f32 amplitude = trajectory->motionFloats[axis];
        trajectory->cycles[axis] = (other->cycles[axis] + trajectory->wobblePhases[axis]) & (FullTurnAngle - 1);
        f32 value;
        switch (block->motion.CycleOf(axis))
        {
        case MotionBlock::CycleSine:
            value = amplitude * SinOfAngle(&trajectory->cycles[axis]);
            break;
        case MotionBlock::CycleSquare:
        case MotionBlock::CycleSquareToo:
            value = amplitude;
            if ((static_cast<s32>(block->cycleRates[axis] * (static_cast<f32>(trajectory->cycles[axis]) * AngleToRadians)) & 1)
                != 0)
            {
                value = -value;
            }

            break;
        case MotionBlock::CycleRandom:
            value = RandomSignedTimes(amplitude);
            break;
        case MotionBlock::CycleAngle:
            value = static_cast<f32>(trajectory->cycles[axis]) * AngleToRadians;
            break;
        default:
            *values[axis] = 0.0f;
            continue;
        }

        u32 sign = block->motion.SignOf(axis);
        if (sign == MotionBlock::SignPositive)
        {
            value = __builtin_fabsf(value);
        }
        else if (sign == MotionBlock::SignNegative)
        {
            value = -__builtin_fabsf(value);
        }

        *values[axis] = value;
    }
}

void TrajectoryFrame(Trajectory* trajectory, ObjectNode* node)
{
    const MotionBlock* block = trajectory->followed;
    MotionBlockMotion bits = block->motion;
    InstanceContext* instance = node->owner;
    if (bits.kind != MotionBlock::KindCycles || block->mover != MotionBlock::MoverOwnInstance)
    {
        return;
    }

    if (bits.turns)
    {
        u32 turned;
        switch (bits.space)
        {
        case SpaceStart:
        {
            InstanceContext* parent = instance->parent;
            if (parent != nullptr)
            {
                // Its attachment turned from where it rests (an instance missing from its parent's path is turned at address 0,
                // as retail does), and the instance placed where that puts it
                Attachment* entry = AttachmentOf(parent, instance);
                entry->offset = entry->takenOffset;
                s32 x = trajectory->turnAngles[0];
                s32 y = trajectory->turnAngles[1];
                s32 z = trajectory->turnAngles[2];
                Vector4 rotation;
                GetRotationXYZ(&rotation, &x, &y, &z);
                TurnMatrix(&entry->offset, &rotation);
                ObjectPlace* parentPlace = parent->place;
                RotateAndTranslate(parentPlace);
                Matrix4x4 world;
                VuMultiplyMatrices(&entry->offset, &parentPlace->matrix, &world);
                ObjectPlace* place = instance->place;
                place->SyncPosition();
                if (place->MoveTo(RowOf(&world, 3)))
                {
                    QueueObject(instance);
                }

                place = instance->place;
                place->SyncRotation();
                Vector4 worldRotation;
                GetRotationVec(&worldRotation, &world);
                turned = place->TurnTo(&worldRotation);
                break;
            }

            Vector4 start = node->informationPointer->rotation;
            ObjectPlace* place = instance->place;
            place->SyncRotation();
            if (place->TurnTo(&start))
            {
                QueueObject(instance);
            }

            turned = TurnAboutAxes(instance->place, trajectory->turnAngles);
            break;
        }
        case SpaceOwn:
            turned = TurnAboutAxes(instance->place, trajectory->turnAngles);
            break;
        case SpaceStored:
        {
            ObjectPlace* stored = node->storedPlace;
            if (stored == nullptr)
            {
                turned = TurnPlace(instance->place, &trajectory->turn);
                break;
            }

            stored->SyncRotation();
            Vector4 rotation = stored->rotation;
            ObjectPlace* place = instance->place;
            place->SyncRotation();
            if (place->TurnTo(&rotation))
            {
                QueueObject(instance);
            }

            turned = TurnAboutAxes(instance->place, trajectory->turnAngles);
            break;
        }
        default:
            turned = TurnPlace(instance->place, &trajectory->turn);
            break;
        }

        if (turned != 0)
        {
            QueueObject(instance);
        }

        return;
    }

    if (instance->place->MoveBy(&trajectory->move))
    {
        QueueObject(instance);
    }

    block = trajectory->followed;
    if (block->motion.facesMove || block->motion.roll != 0)
    {
        Vector4 position = PositionOf(instance->place);
        Vector4 moved = position;
        moved.x = moved.x - trajectory->lastPosition.x;
        moved.y = moved.y - trajectory->lastPosition.y;
        moved.z = moved.z - trajectory->lastPosition.z;
        block = trajectory->followed;
        switch (block->motion.roll)
        {
        case MotionBlock::RollFaces:
        {
            // Facing the way it moved: its turn about y made the move's, the others kept
            s32 yaw;
            YawOfDirection(&yaw, &moved);
            ObjectPlace* place = instance->place;
            place->SyncRotation();
            place->MarkTurned();
            s32 pitch;
            s32 oldYaw;
            s32 roll;
            AnglesOfRotation(&place->rotation, &pitch, &oldYaw, &roll);
            GetRotationXYZ(&place->rotation, &pitch, &yaw, &roll);
            QueueObject(instance);
            break;
        }
        case MotionBlock::RollY:
            RollAlongY(trajectory->rollRate, node, &moved);
            break;
        case MotionBlock::RollX:
            RollAlongX(trajectory->rollRate, node, &moved);
            break;
        default:
            SpinAlongMove(trajectory->rollRate, block->spinDegrees, node, &moved,
                          block->flags.spinsBySize ? 1 : 0);
            break;
        }

        trajectory->lastPosition = position;
        return;
    }

    if (!block->flags.facesTracked)
    {
        return;
    }

    Vector4 target = PositionOf(node->tracked->place);
    SteerTowards(1.0f, 0.0f, SteerMostLean, instance, &target);
}

void HoldTrajectory(Trajectory* trajectory, ObjectNode* node)
{
    const MotionBlock* block = trajectory->followed;
    MotionBlockMotion bits = block->motion;
    if (bits.kind != MotionBlock::KindCycles || block->mover != MotionBlock::MoverOwnInstance)
    {
        return;
    }

    if (!bits.turns)
    {
        InstanceContext* instance = node->owner;
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        if (place->MoveTo(&trajectory->position))
        {
            QueueObject(instance);
        }

        return;
    }

    InstanceContext* instance = node->owner;
    if (instance->parent != nullptr)
    {
        // An instance missing from its parent's path has its matrix made at address 0 (retail)
        Attachment* entry = AttachmentOf(instance->parent, node->owner);
        MatrixFromRotation(&entry->offset, &trajectory->rotation);
        return;
    }

    ObjectPlace* place = instance->place;
    place->SyncRotation();
    if (place->TurnTo(&trajectory->rotation))
    {
        QueueObject(instance);
    }
}

void StepTrajectoryContact(Trajectory* trajectory, ObjectNode* node)
{
    ObjectRigidBody* body = node->rigidBody;
    DynamicBody* physics = body->physicsBody;
    RigidBodyFlags contacts = physics->bodyFlags;
    if (contacts.touchedBody != 0)
    {
        // Grabbing AgentRef1 when it touched it and holds, else what it touched
        node->flags.seeksContact = 0;
        InstanceContext* touched = physics->lastTouched;
        f32 strength = trajectory->followed->holdStrength;
        if (node->agentRef1 != nullptr && IsAsleep(node->agentRef1))
        {
            node->agentRef1 = nullptr;
        }

        if (!(0.0f < strength) || node->agentRef1 == nullptr || node->agentRef1 != touched)
        {
            if (touched == nullptr)
            {
                return;
            }

            strength = trajectory->followed->grabStrength;
            if (!(0.0f < strength))
            {
                return;
            }
        }

        GrabTouched(strength, trajectory, node, touched);
        SetRigidBodySize(GrabbedSize, body);
        SetRigidBodyDrag(GrabbedDrag, body);
        SetRigidBodyLengthDrag(GrabbedLengthDrag, body);
        return;
    }

    if (contacts.touchedWorld != 0)
    {
        ReferencedObject* leftOut = node->owner->collision.leftOut;
        if (leftOut != nullptr)
        {
            leftOut->collision.leftOut = nullptr;
        }

        f32 strength = trajectory->followed->grabStrength;
        if (!(0.0f < strength))
        {
            return;
        }

        node->flags.seeksContact = 0;
        HoldWithAttachments(strength, trajectory, node);
        return;
    }

    ObjectPlace* place = node->owner->place;
    RotateAndTranslate(place);
    Vector4 forward = *RowOf(&place->matrix, 2);
    f32 scale = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * scale;
    forward.y = forward.y * scale;
    forward.z = forward.z * scale;
    Matrix4x4 turn;
    MatrixFacing(&turn, &forward);
    physics->SetTurn(&turn);
}
