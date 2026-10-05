#include "game/commands.h"

#include "game/clock.h"
#include "game/collision.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/rigidbody.h"

// The commands that work on an object node's rigid body (its collisions, launches, impulses and forces), and the rigid body's own
// functions: made and destroyed, its kinds of motion and of collisions set (its chunk's two lists of rigid bodies, its collision
// cache, its physics body made and let go of), active again and out of action, its values (gravity, restitution, drags, friction,
// mass, centre of mass, a turn) and its contacts, sized, pushed by an impulse or a force, and its magnet's pull

extern "C"
{
    // A rigid body active again, sized (the float first), its physics body made for a kind, pushed by a force at a point over the
    // time since its node's last update, and the force its magnet puts on what's at a position
    void ActivateRigidBody(ObjectRigidBody* body) RETAIL(FUN_00246950);
    void SetRigidBodySize(f32 size, ObjectRigidBody* body) RETAIL_N32(FUN_00246a88);
    void MakePhysicsBody(ObjectRigidBody* body, u32 kind) RETAIL(FUN_00247270);
    void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point) RETAIL(FUN_0024c508);
    void MagnetPull(ObjectRigidBody* body, const Vector4* position, Vector4* pull) RETAIL(FUN_0024c690);

    // A turn (radians, the float first) from the place its node keeps, its contacts forgotten (what it rides let go of, whether
    // it touched anything kept), and its kind of motion made none (out of its chunk's first list)
    void SetRigidBodyTurn(f32 radians, ObjectRigidBody* body) RETAIL_N32(FUN_002549b8);
    void ForgetRigidBodyContacts(ObjectRigidBody* body) RETAIL(FUN_00254a10);
    void StopRigidBodyMotion(ObjectRigidBody* body) RETAIL(FUN_00254428);

    // The node kinds whose instances the physics bodies collide with (the start-up sets them)
    extern u32 g_PhysicsBodyKinds RETAIL(D_0030A11C);
}

EABI_EXPORT(FUN_00246a88, SetRigidBodySize);
EABI_EXPORT(FUN_002541e0, SetRigidBodyGravity);
EABI_EXPORT(FUN_002542a0, SetRigidBodyRestitution);
EABI_EXPORT(FUN_00254340, SetRigidBodyFriction);
EABI_EXPORT(FUN_002542e0, SetRigidBodyDrag);
EABI_EXPORT(FUN_00254310, SetRigidBodyLengthDrag);
EABI_EXPORT(FUN_00254380, SetRigidBodySpinFriction);
EABI_EXPORT(FUN_002543a8, SetRigidBodyMass);
EABI_EXPORT(FUN_002549b8, SetRigidBodyTurn);

namespace
{
// The kinds of a rigid body past the hulls sized by its instance's own box (RigidBodyKind)
constexpr u32 BoxKindsEnd = BodyKindSimplex + 1;
// Made, its bits' low word is set (the bits nothing reads), bits 48 (a size given), 49 (it falls), 53 (touching anything) and 63
// left as the memory had them and the others cleared
constexpr u64 BitsSetWhenMade = 0xFFFFFFFF;
constexpr u64 BitsKeptWhenMade = 0x8023000000000000 | BitsSetWhenMade;
// Its state made: bits 0, 11-14 (the launch steps) and 19 set, 1, 2 and 26-31 left, the others cleared
constexpr u32 StateClearedWhenMade = 0x3F787F8;
constexpr u32 StateSetWhenMade = 0x87801;

// The surfaces whose triangles a rigid body's collision cache gathers (SurfaceFlags: solid to the player's probes and to objects)
constexpr u32 CacheSurfaces = SurfaceFlags::SolidToPlayerProbes | SurfaceFlags::SolidToObjects;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

bool TakesPackets(GameNode* node)
{
    return CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0;
}

u32 MotionKind(const ObjectRigidBody* body)
{
    return body->bits.motionKind;
}

u32 CollisionKind(const ObjectRigidBody* body)
{
    return body->bits.collisionKind;
}

void SetMotionKind(ObjectRigidBody* body, u32 kind)
{
    body->bits.motionKind = kind;
}

void SetCollisionKind(ObjectRigidBody* body, u32 kind)
{
    body->bits.collisionKind = kind;
}

// A body taken out of its chunk's first or second list (the last one moved into its place) and put in (no room past 255), the
// retail code's inlined TakeFirstRigidBody and co.
void TakeFromFirstList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u16 index = body->firstIndex;
    if (index == ObjectRigidBody::NoFirstIndex)
    {
        return;
    }

    bodies->firstCount--;
    ObjectRigidBody* last = bodies->first[bodies->firstCount];
    bodies->first[index] = last;
    last->firstIndex = index;
    ForgetFirstIndex(body);
}

void TakeFromSecondList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    u8 index = body->secondIndex;
    if (index == ObjectRigidBody::NoSecondIndex)
    {
        return;
    }

    bodies->secondCount--;
    ObjectRigidBody* last = bodies->second[bodies->secondCount];
    bodies->second[index] = last;
    last->secondIndex = index;
    ForgetSecondIndex(body);
}

void PutInFirstList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->firstCount >= ChunkRigidBodies::MostBodies)
    {
        return;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->firstIndex = bodies->firstCount;
    bodies->first[bodies->firstCount++] = body;
}

void PutInSecondList(ChunkRigidBodies* bodies, ObjectRigidBody* body)
{
    if (bodies->secondCount >= ChunkRigidBodies::MostBodies)
    {
        return;
    }

    if (body->chunkBodies == nullptr)
    {
        body->chunkBodies = bodies;
    }

    body->secondIndex = static_cast<u8>(bodies->secondCount);
    bodies->second[bodies->secondCount++] = body;
}

// The mass of a node's physics body: its motion block's when that's above 0, else the node's first float property
f32 BodyMass(ObjectNode* node)
{
    MotionBlock* block = node->motionBlock;
    if (block != nullptr && !(block->mass <= 0.0f))
    {
        return block->mass;
    }

    return node->PacketProperties()->GetFloat(0);
}

Vector4 SidesOf(const Box* box)
{
    return {box->max.x - box->min.x, box->max.y - box->min.y, box->max.z - box->min.z, 1.0f};
}

f32 LargestSide(const Box* box)
{
    Vector4 sides = SidesOf(box);
    f32 largest = sides.x;
    if (largest < sides.y)
    {
        largest = sides.y;
    }

    if (largest < sides.z)
    {
        largest = sides.z;
    }

    return largest;
}

// The motion's velocity replaced, the one it had kept as its start
void SetVelocity(MotionState* motion, const Vector4* velocity)
{
    motion->startVelocity = motion->velocity;
    motion->velocity = *velocity;
}

// A point of the world in a place's own space
Vector4 PointInPlace(ObjectPlace* place, const Vector4* point)
{
    Vector4 local = *point;
    RotateAndTranslate(place);
    Matrix4x4 inverse = place->matrix;
    VuInvertRigidInPlace(&inverse);
    VuTransformPoint(&inverse, &local, &local);
    return local;
}

// The seconds since a node's last update (none when it never was)
f32 SecondsSinceUpdate(const GameNode* node, const TimeClock* clock)
{
    if (node->time == 0)
    {
        return 0.0f;
    }

    return static_cast<f32>(static_cast<s32>(clock->time - node->time)) * g_SecondsPerClockUnit;
}

// Where a launch goes: the current route step's position (for its designator or DesignatesNextStep), moved by the step values,
// else the designated one
void LaunchTarget(ColliderLaunchBits launch, const Vector4* offset, f32 corner, f32 toward, f32 scatter, BehaviourRunner* runner,
                  Vector4* target)
{
    ObjectNode* node = NodeOf(runner);
    u32 designator = launch.designator;
    // The next step's designator takes the current step too
    if (designator == DesignatesCurrentStep || designator == DesignatesNextStep)
    {
        RouteStepPosition(node->waypoints, target, node, 1, corner, toward, scatter, 0.0f, 0.0f);
        return;
    }

    DesignatedPosition(target, launch.space, runner, launch.offsetGiven ? offset : nullptr, designator, launch.receiver, 0);
}
}

// Half the largest side of the instance's box as the radius (when asked), then the radius as the node's roll radius and its
// physics sphere's radius (an ellipsoid made a sphere)
void SetLogicalRadiusCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (flags.fromBox)
    {
        radius = LargestSide(node->owner->CollisionBox()) * 0.5f;
    }

    if (!TakesPackets(node))
    {
        return;
    }

    node->rollRadius = radius;
    auto* body = static_cast<GameNode*>(GetGameNode(&node->owner->nodes, NodeRigidBody));
    if (body == nullptr || CallVirtual<u32>(body, body->vtable, DynamicBody::IsSphereSlot) == 0)
    {
        return;
    }

    auto* sphere = static_cast<SphereBody*>(body);
    sphere->ellipsoid = 0;
    sphere->radius = radius;
}

// The node's roll radius; its rigid body (made the first time) given kinds of motion and of collisions, let go of when it's left
// with neither, and given the values of the settings
void SetCollisionsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    if (settings.rollRadiusGiven)
    {
        node->rollRadius = rollRadius;
    }

    if (settings.rollRadiusFromBox)
    {
        node->rollRadius = LargestSide(node->owner->CollisionBox()) * 0.5f;
    }
    else if (settings.rollRadiusFromHeight)
    {
        // Half the box's height, and the width scale the larger of its half width and half depth over it
        Vector4 sides = SidesOf(node->owner->CollisionBox());
        f32 half = sides.y * 0.5f;
        node->rollRadius = half;
        f32 inverse = 1.0f / half;
        widthScale = __builtin_fmaxf(sides.z * 0.5f, sides.x * 0.5f) * inverse;
    }

    // A body that's there keeps its kind of motion
    if (settings.motionKindGiven)
    {
        if (settings.motionKind == 0)
        {
            if (body != nullptr)
            {
                ListRigidBodyFirst(body, 0);
            }
        }
        else if (body == nullptr)
        {
            body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
            ListRigidBodyFirst(body, settings.motionKind);
            node->ReleaseRigidBody();
            node->rigidBody = body;
        }
    }

    if (settings.collisionKindGiven)
    {
        u32 kind = settings.collisionKind;
        if (kind == 0)
        {
            if (body == nullptr)
            {
                return;
            }

            ListRigidBodySecond(body, 0);
        }
        else
        {
            if (body == nullptr)
            {
                body = ConstructRigidBody(MemoryAllocate(sizeof(ObjectRigidBody)), node);
                node->ReleaseRigidBody();
                node->rigidBody = body;
            }

            ListRigidBodySecond(body, kind);
        }
    }

    if (body == nullptr)
    {
        return;
    }

    if (settings.pushedByVolumes)
    {
        body->state.pushedByVolumes = 1;
    }

    if (settings.gravityGiven)
    {
        SetRigidBodyGravity(gravity.FloatWith(node->PacketProperties()), body);
    }
    else
    {
        ClearRigidBodyGravity(body);
    }

    body->bits.immovable = settings.immovable;
    if (settings.steersItself)
    {
        body->bits.steersItself = 1;
    }

    body->state.unused2 = settings.unused25;
    body->state.unused3 = settings.unused26;
    body->widthScale = widthScale;
    body->impulseLength = impulseLength;
    if (settings.impulseCapped)
    {
        body->state.impulseCapped = 1;
    }
    else if (settings.impulseFixed)
    {
        body->state.impulseFixed = 1;
    }

    if ((body->bits.value & ObjectRigidBodyBits::KindsMask) == 0)
    {
        node->ReleaseRigidBody();
        return;
    }

    if (settings.dragGiven)
    {
        SetRigidBodyDrag(drag, body);
    }

    if (settings.lengthDragGiven)
    {
        SetRigidBodyLengthDrag(lengthDrag, body);
    }
    else if (lengthDrag < 0.0f)
    {
        body->state.tellsWaterTouches = 1;
    }

    if (settings.frictionGiven)
    {
        SetRigidBodyFriction(friction, body);
    }

    if (settings.restitutionGiven)
    {
        SetRigidBodyRestitution(restitution, body);
    }
    else if (settings.ownNormalRate)
    {
        body->restitution = restitution;
        body->bits.ownNormalRate = 1;
    }

    if (settings.sizeGiven)
    {
        SetRigidBodySize(size, body);
    }
    else if (settings.slopeLimited)
    {
        body->size = size;
        body->bits.slopeLimited = 1;
    }

    if (settings.spinFrictionGiven)
    {
        SetRigidBodySpinFriction(spinFriction, body);
    }

    if (settings.centerOfMassGiven)
    {
        SetRigidBodyCenterOfMass(body, reinterpret_cast<const Vector4*>(&centerOfMassX));
    }

    if (body->physicsBody == nullptr)
    {
        body->bits.launch = settings.launch;
    }

    body->bits.stopsAtRest = 0;
    body->state.launchSteps = ObjectRigidBodyState::MostSteps;
}

// The rigid body moving, its values (the drag, the friction, the restitution, its stopping once it rests, launched again) and the
// node's velocity given as the bits say
void ContinueColliderMotionCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    // Retail bug: without a rigid body it writes the bits at 0x88 (and the values' setters read through it)
    ObjectRigidBody* body = node->rigidBody;
    body->bits.moving = 1;
    if (flags.velocityGiven)
    {
        // Retail bug: the vector it turns into the world is its stack's, never the velocity given (unused2 to unused4), so the
        // node gets whatever was left there (none here)
        Vector4 velocity = {};
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(node->motion, &velocity);
    }

    if (flags.frictionGiven)
    {
        SetRigidBodyFriction(friction, body);
    }

    if (flags.dragGiven)
    {
        SetRigidBodyDrag(drag, body);
    }

    if (flags.restitutionGiven)
    {
        SetRigidBodyRestitution(restitution, body);
    }

    if (flags.stops)
    {
        body->bits.stopsAtRest = 1;
        body->state.launchSteps = 0;
    }
}

// The node's velocity: the one given (turned from the instance's space into the world), or the throw to the launch's target under
// the rigid body's gravity, over a height (the target's height above the node added when it's above, when asked) or in a time;
// then the rigid body's values given and its contacts forgotten
void ColliderLaunchNowCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    body->bits.moving = 1;
    MotionState* motion = node->motion;
    if (launch.velocityGiven)
    {
        Vector4 velocity = {x, y, z, w};
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(motion, &velocity);
    }
    else
    {
        ObjectPlace* place = node->owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        Vector4 target = g_DefaultBox.min;
        target.w = 1.0f;
        LaunchTarget(launch, reinterpret_cast<const Vector4*>(&x), stepCorner, stepToward, stepScatter, runner, &target);
        // Retail bug: with neither kind of throw the node gets the velocity its stack had (none here)
        Vector4 velocity = {};
        f32 gravity = body->gravity;
        if (launch.thrownOverHeight)
        {
            f32 height = heightOrTime;
            if (extras.addsRise)
            {
                f32 rise = target.y - position.y;
                if (0.0f < rise)
                {
                    height = rise + height;
                }
            }

            ThrowOverHeight(gravity, height, &position, &target, &velocity);
        }
        else if (launch.thrownInTime)
        {
            ThrowInTime(gravity, heightOrTime, &position, &target, &velocity);
        }

        SetVelocity(motion, &velocity);
    }

    if (launch.restitutionGiven)
    {
        SetRigidBodyRestitution(restitution, body);
    }

    if (launch.frictionGiven)
    {
        SetRigidBodyFriction(friction, body);
    }

    if (launch.dragGiven)
    {
        SetRigidBodyDrag(drag, body);
    }

    if (launch.turnGiven)
    {
        SetRigidBodyTurn(turn, body);
    }

    body->state.unused2 = extras.unused4;
    if (launch.stops)
    {
        body->bits.stopsAtRest = 1;
        body->state.launchSteps = 0;
    }

    ForgetRigidBodyContacts(body);
}

// ColliderLaunchNow's velocity (the designated target alone), then the physics body launched with it, spinning by spinX about the
// instance's x axis or else by spinY about its y axis. Without a rigid body retail reads its gravity at 0xA4
void LaunchAtTargetCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    ObjectRigidBody* body = node->rigidBody;
    MotionState* motion = node->motion;
    const auto* offset = reinterpret_cast<const Vector4*>(&offsetX);
    // Retail bug: with neither kind of throw the node and its physics body get the velocity the stack had (none here)
    Vector4 velocity = {};
    if (launch.velocityGiven)
    {
        velocity = *offset;
        ObjectPlace* place = node->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &velocity, &velocity);
        SetVelocity(motion, &velocity);
    }
    else
    {
        ObjectPlace* place = node->owner->place;
        place->SyncPosition();
        Vector4 position = place->position;
        Vector4 goal = g_DefaultBox.min;
        goal.w = 1.0f;
        DesignatedPosition(&goal, launch.space, runner, launch.offsetGiven ? offset : nullptr, launch.designator, launch.receiver,
                           0);
        f32 gravity = body->gravity;
        if (launch.thrownOverHeight)
        {
            f32 height = heightOrTime;
            if (launch.addsRise)
            {
                f32 rise = goal.y - position.y;
                if (0.0f < rise)
                {
                    height = rise + height;
                }
            }

            ThrowOverHeight(gravity, height, &position, &goal, &velocity);
        }
        else if (launch.thrownInTime)
        {
            ThrowInTime(gravity, heightOrTime, &position, &goal, &velocity);
        }

        SetVelocity(motion, &velocity);
    }

    LaunchPhysicsBody(spinY, spinX, node, &velocity);
}

// The designated instance's (else the originator's when asked, else the node's own) pushed: by the impulse given (turned from the
// node's instance's space, on its rigid body at its position or else on its physics body at its middle) or else by the spin given
// (added to its physics body's angular momentum)
void ApplyImpulseCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance;
    u8 designator = flags.designator;
    if (designator != DesignatesNone)
    {
        GameNode* agentNode = runner->agentNode;
        instance = CallVirtual<InstanceContext*>(agentNode, agentNode->vtable, ObjectNode::GetDesignatorSlot,
                                                 static_cast<u32>(designator));
    }
    else if (flags.fromOriginator)
    {
        instance = static_cast<InstanceContext*>(runner->originator);
    }
    else
    {
        instance = runner->agentNode->owner;
    }

    if (instance == nullptr)
    {
        return;
    }

    NodeList* nodes = &instance->nodes;
    if (GetGameNode(nodes, NodeObject) == nullptr)
    {
        return;
    }

    if (flags.impulseGiven)
    {
        Vector4 impulse = {impulseX, impulseY, impulseZ, impulseW};
        ObjectPlace* place = runner->agentNode->owner->place;
        RotateAndTranslate(place);
        VuRotateVector(&place->matrix, &impulse, &impulse);
        // Retail reads through the node when it doesn't take packets (none)
        ObjectNode* node = PacketNodeOf(instance);
        FollowOwnMotionBlock(node);
        ObjectRigidBody* body = node->rigidBody;
        if (body != nullptr)
        {
            ObjectPlace* bodyPlace = node->owner->place;
            bodyPlace->SyncPosition();
            Vector4 point = bodyPlace->position;
            PushRigidBody(body, &impulse, &point);
            return;
        }

        // Retail looks for the physics body a second time when there's none
        auto* physics = static_cast<RigidBody*>(GetGameNode(nodes, NodeRigidBody));
        if (physics == nullptr)
        {
            physics = static_cast<RigidBody*>(GetGameNode(nodes, NodeRigidBody));
            if (physics == nullptr)
            {
                return;
            }
        }

        Vector4 middle = {0.0f, 0.0f, 0.0f, 1.0f};
        physics->ApplyImpulse(&impulse, &middle);
        return;
    }

    if (!flags.spinGiven)
    {
        return;
    }

    auto* physics = static_cast<RigidBody*>(GetGameNode(nodes, NodeRigidBody));
    if (physics == nullptr)
    {
        return;
    }

    physics->angularMomentum.x = physics->angularMomentum.x + spinX;
    physics->angularMomentum.y = physics->angularMomentum.y + spinY;
    physics->angularMomentum.z = physics->angularMomentum.z + spinZ;
    physics->bodyFlags.velocityStale = 1;
}

// The pull of the awake focus's rigid body's magnet on the node's position, over the time since the node's last update, as a
// force on the node's rigid body at its middle; when the focus is attached to an instance whose node has a rigid body, half of it
// at the middle of that one's middle and the node's position, and the other half the other way on that body
void MagnetPullToFocusCommand::Execute(TimeClock* clock, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (node == nullptr)
    {
        return;
    }

    InstanceContext* focus = node->AwakeFocus();
    if (focus == nullptr)
    {
        return;
    }

    ObjectNode* focusNode = PacketNodeOf(focus);
    if (focusNode == nullptr)
    {
        return;
    }

    ObjectRigidBody* magnet = focusNode->RigidBody();
    ObjectPlace* place = node->owner->place;
    place->SyncPosition();
    Vector4 position = place->position;
    Vector4 pull = {0.0f, 0.0f, 0.0f, 1.0f};
    MagnetPull(magnet, &position, &pull);
    f32 seconds = SecondsSinceUpdate(node, clock);
    pull.x = pull.x * seconds;
    pull.y = pull.y * seconds;
    pull.z = pull.z * seconds;
    FollowOwnMotionBlock(node);
    ObjectRigidBody* body = node->rigidBody;
    if (body == nullptr)
    {
        return;
    }

    body->bits.moving = 1;
    body->bits.stopsAtRest = 1;
    body->state.launchSteps = 0;
    Vector4 point = node->middle;
    point.w = 1.0f;
    ObjectRigidBody* holderBody = nullptr;
    if (focus->flags.attached)
    {
        // Retail doesn't check that the parent has an object node
        auto* holder = static_cast<ObjectNode*>(GetGameNode(&focus->parent->nodes, NodeObject));
        if (TakesPackets(holder))
        {
            holderBody = holder->rigidBody;
            if (holderBody != nullptr)
            {
                point = holder->middle;
                point.w = 1.0f;
            }
        }
    }

    if (holderBody == nullptr)
    {
        ApplyRigidBodyForce(body, &pull, &point);
        return;
    }

    point.x = (point.x + position.x) * 0.5f;
    point.y = (point.y + position.y) * 0.5f;
    point.z = (point.z + position.z) * 0.5f;
    pull.x = pull.x * 0.5f;
    pull.y = pull.y * 0.5f;
    pull.z = pull.z * 0.5f;
    ApplyRigidBodyForce(body, &pull, &point);
    pull.x = -pull.x;
    pull.y = -pull.y;
    pull.z = -pull.z;
    ApplyRigidBodyForce(holderBody, &pull, &point);
}

// The physics bodies of the awake instances (with physics bodies, 64 at most) within a radius of a point (the instance's position
// plus the offset turned from its space) pushed away from it: by the push at their middle, or by distance by the push plus their
// distance times (edgePush - push) / radius, 3 units above their middle
void PushInstancesAwayCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u16 Most = 0x40;
    constexpr u32 QueryKinds = 1 << NodeRigidBody;
    constexpr f32 Above = 3.0f;
    InstanceContext* instance = runner->agentNode->owner;
    ChunkData* chunk = instance->chunk;
    void* results[Most];
    InstanceQuery query;
    query.results = results;
    query.count = 0;
    query.most = Most;
    query.distance = Infinite;
    // Retail keeps the stack's other bits (nothing reads them)
    query.bits.value = InstanceQueryBits::AllWanted;
    query.wantedFlags = 0;
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    Vector4 sphere = {offsetX, offsetY, offsetZ, radius};
    SkipInQuery(&query, instance);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 centre = place->position;
    if (!(__builtin_fabsf(sphere.x) <= Epsilon && __builtin_fabsf(sphere.y) <= Epsilon && __builtin_fabsf(sphere.z) <= Epsilon))
    {
        f32 x = sphere.x;
        f32 y = sphere.y;
        f32 z = sphere.z;
        place = instance->place;
        RotateAndTranslate(place);
        const Vector4* rowX = RowOf(&place->matrix, 0);
        const Vector4* rowY = RowOf(&place->matrix, 1);
        const Vector4* rowZ = RowOf(&place->matrix, 2);
        centre.z = centre.z + ((rowX->z * x + rowY->z * y) + rowZ->z * z);
        centre.x = centre.x + ((rowX->x * x + rowY->x * y) + rowZ->x * z);
        centre.y = centre.y + ((rowX->y * x + rowY->y * y) + rowZ->y * z);
    }

    sphere.x = centre.x;
    sphere.y = centre.y;
    sphere.z = centre.z;
    u32 found = ChunkInstancesInSphere(chunk, &sphere, QueryKinds, &query, 0);
    bool byDistance = flags.byDistance != 0;
    for (u16 index = 0; index < found; index++)
    {
        auto* other = static_cast<InstanceContext*>(results[index]);
        auto* physics = static_cast<RigidBody*>(GetGameNode(&other->nodes, NodeRigidBody));
        if (physics == nullptr)
        {
            continue;
        }

        ObjectPlace* otherPlace = other->place;
        otherPlace->SyncPosition();
        Vector4 away = otherPlace->position;
        away.x = away.x - centre.x;
        away.y = away.y - centre.y;
        away.z = away.z - centre.z;
        Vector4 impulse;
        Vector4 point;
        if (byDistance)
        {
            f32 perDistance = (edgePush - push) / radius;
            Vector4 way = away;
            f32 inverse = InverseLength(&way, LengthEpsilon);
            way.x = way.x * inverse;
            way.y = way.y * inverse;
            way.z = way.z * inverse;
            impulse = {way.x * push + away.x * perDistance, way.y * push + away.y * perDistance, way.z * push + away.z * perDistance,
                       1.0f};
            point = {0.0f, Above, 0.0f, 1.0f};
        }
        else
        {
            f32 inverse = InverseLength(&away, LengthEpsilon);
            impulse = {away.x * inverse * push, away.y * inverse * push, away.z * inverse * push, 1.0f};
            point = {0.0f, 0.0f, 0.0f, 1.0f};
        }

        physics->ApplyImpulse(&impulse, &point);
    }
}

ObjectRigidBody* ConstructRigidBody(void* memory, ObjectNode* node)
{
    auto* body = static_cast<ObjectRigidBody*>(memory);
    body->node = node;
    body->state.value = (body->state.value & ~StateClearedWhenMade) | StateSetWhenMade;
    body->bits.value = (body->bits.value & BitsKeptWhenMade) | BitsSetWhenMade;
    body->cache = nullptr;
    body->physicsBody = nullptr;
    body->drag = 1.0f;
    body->friction = 1.0f;
    body->restitution = 0.0f;
    body->impulseLength = 1.0f;
    body->unusedC4 = 0.0f;
    body->grip = 0.0f;
    body->widthScale = 1.0f;
    body->contactNormal = {0.0f, 1.0f, 0.0f, 1.0f};
    body->unusedCC = node->motion->velocity.y;
    body->secondIndex = ObjectRigidBody::NoSecondIndex;
    body->firstIndex = ObjectRigidBody::NoFirstIndex;
    body->chunkBodies = nullptr;
    body->object = nullptr;
    body->rideMovement = nullptr;
    body->sizes[1] = 0.0f;
    body->sizes[0] = 0.0f;
    return body;
}

// Back in action: its physics body made for a kind of motion of the physics, else it put back in the first list (even with no
// kind, and whether or not it's still in it)
void ActivateRigidBody(ObjectRigidBody* body)
{
    if (!body->state.outOfAction)
    {
        return;
    }

    ChunkRigidBodies* bodies = ChunkRigidBodiesOf(body->node->owner->chunk);
    body->state.outOfAction = 0;
    u32 kind = MotionKind(body);
    if (kind >= FirstPhysicsBodyKind)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, kind);
        }
    }
    else
    {
        PutInFirstList(bodies, body);
        body->bits.inFirstList = 1;
    }

    // Retail bug: it goes back into the second list only while it's still in it (a second entry then), so one put out of action
    // (DeactivateRigidBody takes it out) never does
    if (body->secondIndex != ObjectRigidBody::NoSecondIndex)
    {
        PutInSecondList(bodies, body);
    }
}

// The size kept, and the physics body's mass and size made again by its kind of collisions: a sphere's sides the size, those of
// hulls 10 and 11 the instance's own box times it (BodyMass's mass)
void SetRigidBodySize(f32 size, ObjectRigidBody* body)
{
    body->size = size;
    body->bits.sizeGiven = 1;
    if (body->physicsBody == nullptr)
    {
        return;
    }

    f32 mass = BodyMass(body->node);
    u32 kind = CollisionKind(body);
    if (kind == BodyKindSphere)
    {
        body->physicsBody->SetMassAndSize(mass, size, size, size);
    }
    else if (kind >= FirstPhysicsBodyKind && kind < BoxKindsEnd)
    {
        Vector4 sides = SidesOf(&body->node->owner->collision.ownBox);
        body->physicsBody->SetMassAndSize(mass, sides.x * size, sides.y * size, sides.z * size);
    }
}

// The kind of motion: none takes the body out of the first list (its physics body let go of unless its collisions' kind is of the
// physics), one of the physics makes the physics body (out of the first list), another puts it in the first list (the physics
// body let go of unless its collisions' kind is of the physics)
void ListRigidBodyFirst(ObjectRigidBody* body, u32 kind)
{
    if (kind == 0)
    {
        StopRigidBodyMotion(body);
        if (CollisionKind(body) < FirstPhysicsBodyKind)
        {
            ReleasePhysicsBody(body);
        }

        return;
    }

    ChunkRigidBodies* bodies = ChunkRigidBodiesOf(body->node->owner->chunk);
    SetMotionKind(body, kind);
    u32 set = MotionKind(body);
    if (set >= FirstPhysicsBodyKind)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, set);
        }

        TakeFromFirstList(bodies, body);
        body->bits.inFirstList = 0;
        return;
    }

    if (!body->bits.inFirstList)
    {
        PutInFirstList(bodies, body);
        body->bits.inFirstList = 1;
    }

    if (CollisionKind(body) < FirstPhysicsBodyKind && body->physicsBody != nullptr)
    {
        ReleasePhysicsBody(body);
    }
}

// The kind of collisions: none takes the body out of the second list (its collision cache destroyed, its physics body let go of
// unless its motion's kind is of the physics), another puts it in the second list and makes the physics body for one of the
// physics, a collision cache (once) for another (the physics body let go of unless its motion's kind is of the physics)
void ListRigidBodySecond(ObjectRigidBody* body, u32 kind)
{
    if (kind == 0)
    {
        StopRigidBodyCollisions(body);
        if (MotionKind(body) < FirstPhysicsBodyKind)
        {
            ReleasePhysicsBody(body);
        }

        return;
    }

    SetCollisionKind(body, kind);
    if (body->secondIndex == ObjectRigidBody::NoSecondIndex)
    {
        PutInSecondList(ChunkRigidBodiesOf(body->node->owner->chunk), body);
    }

    if (body->cache == nullptr && CollisionKind(body) < FirstPhysicsBodyKind)
    {
        CollisionCache* cache = ConstructCollisionCache(static_cast<CollisionCache*>(MemoryAllocate(sizeof(CollisionCache))),
                                                        body->node->owner, CacheSurfaces);
        body->cache = cache;
        cache->margin = 1.0f;
    }

    u32 set = CollisionKind(body);
    if (set >= FirstPhysicsBodyKind)
    {
        if (body->physicsBody == nullptr)
        {
            MakePhysicsBody(body, set);
        }

        return;
    }

    if (MotionKind(body) < FirstPhysicsBodyKind && body->physicsBody != nullptr)
    {
        ReleasePhysicsBody(body);
    }
}

// A sphere of the node's roll radius for 9 (sides of 1 and BodyMass's mass), a body of hulls for the others, placed where the
// instance is, with a restitution of 0.3, a friction of 0.8 and collisions with the instances that have nodes of the kinds of
// g_PhysicsBodyKinds, 4 and 5; it places the instance, which is told of its collisions
void MakePhysicsBody(ObjectRigidBody* body, u32 kind)
{
    constexpr u32 AlsoCollidesWith = 1 << NodeDynamicScenery | 1 << NodeRigidBody;
    constexpr f32 MadeRestitution = Rounded(0.3);
    constexpr f32 MadeFriction = Rounded(0.8);
    bool sphere = kind == BodyKindSphere;
    DynamicBody* physics = AddPhysicsBody(g_PhysicsWorld, body->node->owner, sphere);
    body->physicsBody = physics;
    physics->MoveTo(body->node->owner, 0);
    f32 mass = BodyMass(body->node);
    if (sphere)
    {
        body->physicsBody->SetMassAndSize(mass, 1.0f, 1.0f, 1.0f);
    }

    // Retail works out twice the sides of the instance's own box for hulls 10 and 11 and never uses them: they keep the mass and
    // size they were made with
    body->physicsBody->SetRestitution(MadeRestitution);
    body->physicsBody->SetFriction(MadeFriction);
    body->physicsBody->bits.placesInstance = 1;
    body->physicsBody->collisionMask = g_PhysicsBodyKinds | AlsoCollidesWith;
    if (sphere)
    {
        auto* ball = static_cast<SphereBody*>(body->physicsBody);
        ball->ellipsoid = 0;
        ball->radius = body->node->rollRadius;
    }

    body->bits.launch = 0;
    body->node->owner->flags.physicsBody = 1;
}

// By the kind of motion: 1-5 and 8 the node's velocity takes it, 9-11 the physics body (at the point in the instance's space)
void PushRigidBody(ObjectRigidBody* body, const Vector4* impulse, const Vector4* point)
{
    switch (MotionKind(body))
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
    {
        Vector4& velocity = body->node->motion->velocity;
        velocity.x = velocity.x + impulse->x;
        velocity.y = velocity.y + impulse->y;
        velocity.z = velocity.z + impulse->z;
        break;
    }
    case 9:
    case 10:
    case 11:
    {
        Vector4 local = PointInPlace(body->node->owner->place, point);
        body->physicsBody->ApplyImpulse(impulse, &local);
        break;
    }
    default:
        break;
    }
}

// As PushRigidBody, the node's velocity taking the force times the seconds since the node's last update and the physics body the
// force
void ApplyRigidBodyForce(ObjectRigidBody* body, const Vector4* force, const Vector4* point)
{
    switch (MotionKind(body))
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
    {
        Vector4 push = *force;
        TimeClock* clock = GetContextClock(body->node->owner);
        f32 seconds = SecondsSinceUpdate(body->node, clock);
        push.x = push.x * seconds;
        push.y = push.y * seconds;
        push.z = push.z * seconds;
        Vector4& velocity = body->node->motion->velocity;
        velocity.x = velocity.x + push.x;
        velocity.y = velocity.y + push.y;
        velocity.z = velocity.z + push.z;
        break;
    }
    case 9:
    case 10:
    case 11:
    {
        Vector4 local = PointInPlace(body->node->owner->place, point);
        body->physicsBody->AddLocalForce(force, &local);
        break;
    }
    default:
        break;
    }
}

// The second physics size along the magnet's way (from the body to the position, its z axis, others none) made a unit vector;
// none for its strengths 1 and 2
void MagnetPull(ObjectRigidBody* body, const Vector4* position, Vector4* pull)
{
    Vector4 none = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 way = {0.0f, 0.0f, 0.0f, 1.0f};
    ObjectPlace* place = body->node->owner->place;
    place->SyncPosition();
    Vector4 here = place->position;
    switch (body->state.magnetWay)
    {
    case MagnetFromBody:
        way = {position->x - here.x, position->y - here.y, position->z - here.z, 1.0f};
        break;
    case MagnetAlongZAxis:
        place = body->node->owner->place;
        RotateAndTranslate(place);
        way = *RowOf(&place->matrix, 2);
        break;
    default:
        break;
    }

    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    u32 strength = body->state.magnetStrength;
    if (strength == MagnetEven || strength == MagnetMode2)
    {
        *pull = none;
        return;
    }

    f32 size = body->sizes[1];
    *pull = {way.x * size, way.y * size, way.z * size, 1.0f};
}

void DestroyRigidBody(ObjectRigidBody* body, u32 destroyFlags)
{
    StopRigidBodyCollisions(body);
    StopRigidBodyMotion(body);
    ReleasePhysicsBody(body);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(body);
    }
}

// Out of action (once: back with ActivateRigidBody), out of its chunk's second list
void DeactivateRigidBody(ObjectRigidBody* body)
{
    if (body->state.outOfAction)
    {
        return;
    }

    body->state.outOfAction = 1;
    // Retail bug: it stays in the first list, only its index forgotten (a destroyed body is left in it, an activated one twice)
    body->firstIndex = ObjectRigidBody::NoFirstIndex;
    body->bits.inFirstList = 0;
    if (body->secondIndex != ObjectRigidBody::NoSecondIndex)
    {
        TakeFromSecondList(ChunkRigidBodiesOf(body->node->owner->chunk), body);
    }
}

void ForgetFirstIndex(ObjectRigidBody* body)
{
    body->bits.inFirstList = 0;
    body->firstIndex = ObjectRigidBody::NoFirstIndex;
    if (body->secondIndex == ObjectRigidBody::NoSecondIndex)
    {
        body->chunkBodies = nullptr;
    }
}

void ForgetSecondIndex(ObjectRigidBody* body)
{
    body->secondIndex = ObjectRigidBody::NoSecondIndex;
    if (body->firstIndex == ObjectRigidBody::NoFirstIndex)
    {
        body->chunkBodies = nullptr;
    }
}

void StopRigidBody(ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->Stop();
    }
}

void SetRigidBodyGravity(f32 gravity, ObjectRigidBody* body)
{
    body->gravity = gravity;
    if (gravity == 0.0f)
    {
        ClearRigidBodyGravity(body);
        return;
    }

    body->bits.falls = 1;
    body->node->flags.falls = 1;
}

void ClearRigidBodyGravity(ObjectRigidBody* body)
{
    body->bits.falls = 0;
    body->node->flags.falls = 0;
}

void SetRigidBodyRestitution(f32 value, ObjectRigidBody* body)
{
    body->restitution = value;
    body->bits.ownRestitution = 1;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetRestitution(value);
    }
}

void SetRigidBodyDrag(f32 value, ObjectRigidBody* body)
{
    body->drag = value;
    body->bits.dragged = 1;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->drag = value;
    }
}

void SetRigidBodyLengthDrag(f32 value, ObjectRigidBody* body)
{
    body->lengthDrag = value;
    body->bits.ownLengthDrag = 1;
    if (body->physicsBody != nullptr)
    {
        // Retail bug: the physics body's length drag is made the rigid body's drag, not its length drag
        body->physicsBody->lengthDrag = body->drag;
    }
}

void SetRigidBodyFriction(f32 value, ObjectRigidBody* body)
{
    body->friction = value;
    body->bits.gripped = 1;
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetFriction(value);
    }
}

void SetRigidBodySpinFriction(f32 value, ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        body->physicsBody->SetSpinAndRollFriction(value, value);
    }
}

// Its physics body (which the callers make first) given the mass, sized by the instance's own box
void SetRigidBodyMass(f32 mass, ObjectRigidBody* body)
{
    Vector4 sides = SidesOf(&body->node->owner->collision.ownBox);
    body->physicsBody->SetMassAndSize(mass, sides.x, sides.y, sides.z);
}

void StopRigidBodyMotion(ObjectRigidBody* body)
{
    if (body->firstIndex != ObjectRigidBody::NoFirstIndex)
    {
        ChunkData* chunk = body->node->owner->chunk;
        if (chunk != nullptr)
        {
            ChunkRigidBodies* bodies = ChunkRigidBodiesOf(chunk);
            if (bodies != nullptr)
            {
                TakeFromFirstList(bodies, body);
            }
        }
    }

    body->bits.inFirstList = 0;
    body->bits.motionKind = 0;
}

void StopRigidBodyCollisions(ObjectRigidBody* body)
{
    if (body->cache != nullptr)
    {
        DestroyCollisionCache(body->cache, DestroyAndFree);
        body->cache = nullptr;
    }

    if (body->secondIndex != ObjectRigidBody::NoSecondIndex)
    {
        ChunkData* chunk = body->node->owner->chunk;
        if (chunk != nullptr)
        {
            ChunkRigidBodies* bodies = ChunkRigidBodiesOf(chunk);
            if (bodies != nullptr)
            {
                TakeFromSecondList(bodies, body);
            }
        }
    }

    body->bits.collisionKind = 0;
}

void ReleasePhysicsBody(ObjectRigidBody* body)
{
    if (body->physicsBody != nullptr)
    {
        RemovePhysicsBody(g_PhysicsWorld, body->physicsBody);
        body->physicsBody = nullptr;
    }

    if (CollisionKind(body) >= FirstPhysicsBodyKind)
    {
        SetCollisionKind(body, BodyKindPlain);
    }

    if (MotionKind(body) >= FirstPhysicsBodyKind)
    {
        SetMotionKind(body, BodyKindPlain);
    }

    ObjectNode* node = body->node;
    if (node->motionBlock == nullptr)
    {
        node->owner->flags.physicsBody = 0;
    }
}

void RevertColliderMotion(ObjectRigidBody* body)
{
    body->bits.moving = 0;
    body->bits.stopsAtRest = 0;
}

void SetRigidBodyCenterOfMass(ObjectRigidBody* body, const Vector4* center)
{
    DynamicBody* physics = body->physicsBody;
    if (physics != nullptr)
    {
        CallVirtual<void>(physics, physics->vtable, DynamicBody::SetCenterOfMassSlot, center);
    }
}

void SetRigidBodyTurn(f32 radians, ObjectRigidBody* body)
{
    ObjectNode* node = body->node;
    SetStoredPlace(node, node->owner->place);
    body->unusedC4 = radians;
    body->untouchedTime = 0.0f;
    body->bits.turning = 1;
}

void ForgetRigidBodyContacts(ObjectRigidBody* body)
{
    body->state.touched = body->bits.touchingInstance || body->bits.touchingWorld;
    body->state.onGround = 0;
    body->state.againstWall = 0;
    body->object = nullptr;
    body->bits.touchingInstance = 0;
    body->bits.touchingWorld = 0;
    body->bits.touching = 0;
    body->rideMovement = nullptr;
    body->node->flags.riding = 0;
}
