#include "game/rigidbody.h"

#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "platform/math.h"

namespace
{
// A rotation's length that's still one
constexpr f32 ShortRotation = 0x1.5798ecp-27f;
// A body's mass and the sides of its box when it's made, a sphere's sides and radius
constexpr f32 DefaultMass = 1.0f;
constexpr f32 DefaultSize = 1.5f;
constexpr f32 SphereSize = Rounded(1.3);
constexpr f32 SphereRadius = 0.5f;

Vector4* PositionOf(Matrix4x4* matrix)
{
    return reinterpret_cast<Vector4*>(matrix->m[3]);
}

Vector4* RowAt(Matrix4x4* matrix, u32 row)
{
    return reinterpret_cast<Vector4*>(matrix->m[row]);
}

void SetOrigin(Vector4* vector)
{
    *vector = g_DefaultBox.min;
    vector->w = 1.0f;
}
}

EABI_EXPORT(FUN_00289e88, &RigidBody::SetMassAndSize);
EABI_EXPORT(FUN_00289fc0, &RigidBody::Damp);
EABI_EXPORT(FUN_0028a160, &RigidBody::Step);
EABI_EXPORT(FUN_00291910, SetRotationLimit);
EABI_EXPORT(FUN_0028a9f0, &RigidBody::Spring);
EABI_EXPORT(FUN_0028ad48, &RigidBody::LevelSpring);
EABI_EXPORT(FUN_0028b228, &RigidBody::PointImpulse);
EABI_EXPORT(FUN_0028b3c0, &RigidBody::TwoBodyImpulse);
EABI_EXPORT(FUN_0028b690, &RigidBody::MovingPointImpulse);
EABI_EXPORT(FUN_0028ba78, &RigidBody::AngularFriction);
EABI_EXPORT(FUN_0028b878, VelocitiesBetween);
EABI_EXPORT(FUN_0028da10, &DynamicBody::StepDamping);
EABI_EXPORT(FUN_0028fb00, &HullBody::Float);
EABI_EXPORT(FUN_00291ea8, &RigidBody::Slow);
EABI_EXPORT(FUN_00291f38, &RigidBody::SlowAlongX);
EABI_EXPORT(FUN_00291fd0, &RigidBody::SlowAlongY);
EABI_EXPORT(FUN_00292068, &RigidBody::SlowAlongZ);
EABI_EXPORT(FUN_00292378, &DynamicBody::SetRestitution);
EABI_EXPORT(FUN_00292380, &DynamicBody::SetSoftness);
EABI_EXPORT(FUN_00292390, &DynamicBody::SetFriction);
EABI_EXPORT(FUN_00292398, &DynamicBody::SetSpinAndRollFriction);
EABI_EXPORT(FUN_00292ba0, &SphereBody::SetEllipsoid);
EABI_EXPORT(FUN_00290d38, EllipsoidValue);
EABI_EXPORT(FUN_002924e0, NegativeEllipsoidValue);

void ConstrainPosition(BodyConstraint* constraint, Vector4* position)
{
    BodyConstraintFlags flags = constraint->flags;
    if (flags.fixed != 0)
    {
        *position = constraint->fixedPosition;
        return;
    }

    if (flags.onLine != 0)
    {
        const Vector4& point = constraint->linePoint;
        const Vector4& direction = constraint->lineDirection;
        f32 x = position->x - point.x;
        f32 y = position->y - point.y;
        f32 z = position->z - point.z;
        f32 along = direction.x * x + direction.y * y + direction.z * z;
        Vector4 onLine;
        onLine.x = point.x + direction.x * along;
        onLine.y = point.y + direction.y * along;
        onLine.z = point.z + direction.z * along;
        onLine.w = 1.0f;
        *position = onLine;
        return;
    }

    if (flags.onPlane != 0)
    {
        const Vector4& plane = constraint->plane;
        f32 distance = plane.x * position->x + plane.y * position->y + plane.z * position->z + plane.w;
        position->x = position->x - plane.x * distance;
        position->y = position->y - plane.y * distance;
        position->z = position->z - plane.z * distance;
    }
}

void ConstrainVelocity(BodyConstraint* constraint, Vector4* velocity)
{
    BodyConstraintFlags flags = constraint->flags;
    if (flags.fixed != 0)
    {
        SetOrigin(velocity);
        return;
    }

    if (flags.onLine != 0)
    {
        const Vector4& direction = constraint->lineDirection;
        f32 along = direction.x * velocity->x + direction.y * velocity->y + direction.z * velocity->z;
        Vector4 alongLine;
        alongLine.x = direction.x * along;
        alongLine.y = direction.y * along;
        alongLine.z = direction.z * along;
        alongLine.w = 1.0f;
        *velocity = alongLine;
        return;
    }

    if (flags.onPlane != 0)
    {
        const Vector4& plane = constraint->plane;
        f32 distance = plane.x * velocity->x + plane.y * velocity->y + plane.z * velocity->z;
        velocity->x = velocity->x - plane.x * distance;
        velocity->y = velocity->y - plane.y * distance;
        velocity->z = velocity->z - plane.z * distance;
    }
}

void ConstrainRotation(BodyConstraint* constraint, Vector4* rotation)
{
    if (constraint->flags.hinge != 0)
    {
        // The nearest of the rotations s * a + t * b (a and b unit)
        const Vector4& a = constraint->hinge[0];
        const Vector4& b = constraint->hinge[1];
        f32 ab = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        f32 alongA = rotation->x * a.x + rotation->y * a.y + rotation->z * a.z + rotation->w * a.w;
        f32 t = (rotation->x * (a.x * ab - b.x) + rotation->y * (a.y * ab - b.y) + rotation->z * (a.z * ab - b.z) +
                 rotation->w * (a.w * ab - b.w)) /
                (ab * ab - 1.0f);
        f32 s = alongA - t * ab;
        Vector4 nearest;
        nearest.x = a.x * s + b.x * t;
        nearest.y = a.y * s + b.y * t;
        nearest.z = a.z * s + b.z * t;
        nearest.w = a.w * s + b.w * t;
        if (ShortRotation < nearest.x * nearest.x + nearest.y * nearest.y + nearest.z * nearest.z + nearest.w * nearest.w)
        {
            f32 inverse = InverseLength4(0.0f, InverseEpsilon, &nearest);
            nearest.x = nearest.x * inverse;
            nearest.y = nearest.y * inverse;
            nearest.z = nearest.z * inverse;
            nearest.w = nearest.w * inverse;
        }
        else
        {
            nearest = a;
        }

        rotation->x = nearest.x;
        rotation->y = nearest.y;
        rotation->z = nearest.z;
        rotation->w = nearest.w;
    }

    if (constraint->flags.limited != 0)
    {
        LimitRotation(constraint, rotation);
    }
}

void LimitRotation(BodyConstraint* constraint, Vector4* rotation)
{
    Vector4 given = *rotation;
    Vector4 center = constraint->limitCenter;
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, &center);
    // The turn from the center
    Vector4 turn;
    turn.w = center.w * inverse;
    f32 negated = -inverse;
    turn.z = center.z * negated;
    turn.y = center.y * negated;
    turn.x = center.x * negated;
    MultiplyRotations(&turn, &turn, &given);
    Vector4 axis;
    s32 angle;
    AxisAngleOfRotation(&turn, &axis, &angle, 0);
    f32 radians = static_cast<f32>(angle) * AngleToRadians;
    f32 limit = constraint->limit;
    if (radians < -limit)
    {
        radians = -limit;
    }
    else if (limit < radians)
    {
        radians = limit;
    }
    else
    {
        return;
    }

    s32 limited;
    AngleFrom(&limited, radians, AngleRadians);
    s32 kept = limited;
    RotationAboutAxis(&turn, &axis, &kept, 0);
    Vector4 result = constraint->limitCenter;
    MultiplyRotations(&result, &result, &turn);
    rotation->x = result.x;
    rotation->w = result.w;
    rotation->y = result.y;
    rotation->z = result.z;
}

void SetHinge(BodyConstraint* constraint, const Vector4* localAxis, const Vector4* axis)
{
    constraint->flags.hinge = 1;
    constraint->hingeLocalAxis = *localAxis;
    constraint->hingeAxis = *axis;
    const Vector4& from = constraint->hingeLocalAxis;
    const Vector4& to = constraint->hingeAxis;
    // The turn from the axis in the body's space to the one in the world's
    Vector4 normal;
    normal.x = from.y * to.z - from.z * to.y;
    normal.y = from.z * to.x - from.x * to.z;
    normal.z = from.x * to.y - from.y * to.x;
    normal.w = 1.0f;
    f32 sine = __builtin_sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    f32 cosine = from.x * to.x + from.y * to.y + from.z * to.z;
    HalfAngleSinCos(&sine, &cosine);
    f32 inverse = InverseLength(&normal, LengthEpsilon);
    normal.x = normal.x * inverse * sine;
    normal.y = normal.y * inverse * sine;
    normal.z = normal.z * inverse * sine;
    Vector4& turn = constraint->hinge[0];
    turn.w = cosine;
    turn.x = normal.x;
    turn.y = normal.y;
    turn.z = normal.z;
    // The turn times the axis in the body's space
    Vector4& turned = constraint->hinge[1];
    turned.x = (normal.y * from.z - normal.z * from.y) + from.x * cosine;
    turned.y = (normal.z * from.x - normal.x * from.z) + from.y * cosine;
    turned.z = (normal.x * from.y - normal.y * from.x) + from.z * cosine;
    turned.w = -(from.x * normal.x + from.y * normal.y + from.z * normal.z);
    f32 scale = InverseLength4(0.0f, InverseEpsilon, &turn);
    turn.x = turn.x * scale;
    turn.y = turn.y * scale;
    turn.z = turn.z * scale;
    turn.w = turn.w * scale;
    scale = InverseLength4(0.0f, InverseEpsilon, &turned);
    turned.x = turned.x * scale;
    turned.y = turned.y * scale;
    turned.z = turned.z * scale;
    turned.w = turned.w * scale;
    // Both inverted
    turn.x = -turn.x;
    turn.y = -turn.y;
    turn.z = -turn.z;
    turned.x = -turned.x;
    turned.y = -turned.y;
    turned.z = -turned.z;
}

void SetLocalHinge(BodyConstraint* constraint, const Vector4* localAxis)
{
    Vector4 axis;
    VuRotateVector(&constraint->body->matrix, localAxis, &axis);
    SetHinge(constraint, localAxis, &axis);
}

void SetRotationLimit(f32 limit, BodyConstraint* constraint)
{
    constraint->limit = limit;
    constraint->flags.limited = 1;
    const Vector4& rotation = constraint->body->rotation;
    constraint->limitCenter.x = rotation.x;
    constraint->limitCenter.y = rotation.y;
    constraint->limitCenter.z = rotation.z;
    constraint->limitCenter.w = rotation.w;
}

void FixHere(BodyConstraint* constraint)
{
    FixAt(constraint, PositionOf(&constraint->body->matrix));
}

void FixAt(BodyConstraint* constraint, const Vector4* position)
{
    constraint->flags.onLine = 0;
    constraint->flags.onPlane = 0;
    constraint->flags.fixed = 1;
    constraint->fixedPosition = *position;
}

void KeepOnLine(BodyConstraint* constraint, const Vector4* direction)
{
    constraint->flags.fixed = 0;
    constraint->flags.onPlane = 0;
    constraint->flags.onLine = 1;
    constraint->linePoint = *PositionOf(&constraint->body->matrix);
    constraint->lineDirection = *direction;
    f32 inverse = InverseLength(&constraint->lineDirection, LengthEpsilon);
    constraint->lineDirection.x = constraint->lineDirection.x * inverse;
    constraint->lineDirection.y = constraint->lineDirection.y * inverse;
    constraint->lineDirection.z = constraint->lineDirection.z * inverse;
}

void KeepOnPlaneThrough(BodyConstraint* constraint, const Vector4* normal)
{
    Vector4 plane;
    PlaneFromNormal(&plane, normal, PositionOf(&constraint->body->matrix));
    KeepOnPlane(constraint, &plane);
}

void KeepOnPlane(BodyConstraint* constraint, const Vector4* plane)
{
    constraint->flags.fixed = 0;
    constraint->flags.onLine = 0;
    constraint->flags.onPlane = 1;
    constraint->plane = *plane;
}

RigidBody* RigidBody::Construct(RigidBody* body)
{
    GameNode::Construct(body);
    body->constraint.flags.value = 0;
    body->vtable = g_RigidBodyVTable;
    body->constraint.body = body;
    SetOrigin(&body->inverseInertia);
    SetOrigin(&body->restInverseInertia);
    SetOrigin(PositionOf(&body->matrix));
    body->rotationRate = {0.0f, 0.0f, 0.0f, 0.0f};
    for (auto& row : body->worldInverseInertia.m)
    {
        for (f32& value : row)
        {
            value = 0.0f;
        }
    }

    SetOrigin(&body->recentTurn);
    body->substeps = 1;
    InitIdentityMatrix(&body->matrix);
    InitIdentityMatrix(&body->inverseMatrix);
    SetOrigin(&body->momentum);
    body->rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    SetOrigin(&body->angularMomentum);
    SetOrigin(&body->angularVelocity);
    SetOrigin(&body->velocity);
    body->lastVelocity = body->velocity;
    SetOrigin(&body->stepForce);
    SetOrigin(&body->stepTorque);
    body->waterLevel = -Infinite;
    // The most it moves and turns are set after its state is made: what the heap had then
    body->bodyFlags.value = 0;
    body->bodyFlags.velocityStale = 1;
    body->bodyFlags.rotationStale = 1;
    body->SetMassAndSize(DefaultMass, DefaultSize, DefaultSize, DefaultSize);
    body->maxSpin = Infinite;
    body->maxSpeed = Infinite;
    SetOrigin(&body->force);
    SetOrigin(&body->torque);
    body->knockScale = 1.0f;
    body->lastTouched = nullptr;
    body->ignoredInstance = nullptr;
    return body;
}

void RigidBody::Destroy(u32 destroyFlags)
{
    vtable = g_RigidBodyVTable;
    GameNode::Destroy(destroyFlags);
}

u32 RigidBody::CanChangeChunk()
{
    return 0;
}

u32 RigidBody::Kind()
{
    return NodeRigidBody;
}

u32 RigidBody::Update()
{
    return 0;
}

u32 RigidBody::GetClassId()
{
    return ClassId;
}

void RigidBody::UpdateState()
{
    if (bodyFlags.velocityStale != 0)
    {
        velocity = momentum;
        f32 scale = inverseMass;
        velocity.x = velocity.x * scale;
        velocity.y = velocity.y * scale;
        velocity.z = velocity.z * scale;
        if (maxSpeed * maxSpeed < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
        {
            f32 inverse = InverseLength(&velocity, LengthEpsilon);
            velocity.x = velocity.x * inverse;
            velocity.y = velocity.y * inverse;
            velocity.z = velocity.z * inverse;
            velocity.x = velocity.x * maxSpeed;
            velocity.y = velocity.y * maxSpeed;
            velocity.z = velocity.z * maxSpeed;
            Vector4 limited;
            limited.x = velocity.x * mass;
            limited.y = velocity.y * mass;
            limited.z = velocity.z * mass;
            limited.w = 1.0f;
            momentum = limited;
        }

        bodyFlags.velocityStale = 0;
    }

    if (bodyFlags.rotationStale == 0)
    {
        return;
    }

    ApplyConstraint();
    // The rotation's matrix (made the other way round), and its inverse
    Matrix4x4 turn;
    MatrixFromRotation(&turn, &rotation);
    SetOrigin(RowAt(&turn, 3));
    VuInvertRigidInPlace(&turn);
    *RowAt(&matrix, 0) = *RowAt(&turn, 0);
    *RowAt(&matrix, 1) = *RowAt(&turn, 1);
    *RowAt(&matrix, 2) = *RowAt(&turn, 2);
    inverseMatrix = matrix;
    VuInvertRigidInPlace(&inverseMatrix);
    Matrix4x4 scaled = inverseMatrix;
    for (u32 row = 0; row < 3; row++)
    {
        scaled.m[row][0] = scaled.m[row][0] * inverseInertia.x;
        scaled.m[row][1] = scaled.m[row][1] * inverseInertia.y;
        scaled.m[row][2] = scaled.m[row][2] * inverseInertia.z;
    }

    VuMultiplyMatrices(&scaled, &matrix, &worldInverseInertia);
    VuRotateVector(&worldInverseInertia, &angularMomentum, &angularVelocity);
    if (maxSpin * maxSpin <
        angularVelocity.x * angularVelocity.x + angularVelocity.y * angularVelocity.y + angularVelocity.z * angularVelocity.z)
    {
        f32 inverse = InverseLength(&angularVelocity, LengthEpsilon);
        angularVelocity.x = angularVelocity.x * inverse;
        angularVelocity.y = angularVelocity.y * inverse;
        angularVelocity.z = angularVelocity.z * inverse;
        angularVelocity.x = angularVelocity.x * maxSpin;
        angularVelocity.y = angularVelocity.y * maxSpin;
        angularVelocity.z = angularVelocity.z * maxSpin;
        // The world's inertia as the inverse of a rotation and a translation's
        Matrix4x4 inertiaMatrix = worldInverseInertia;
        VuInvertRigidInPlace(&inertiaMatrix);
        VuRotateVector(&inertiaMatrix, &angularVelocity, &angularMomentum);
    }

    // Half the angular velocity times the rotation
    const Vector4& q = rotation;
    const Vector4& w = angularVelocity;
    rotationRate.x = ((q.w * w.x - q.z * w.y) + q.y * w.z) * 0.5f;
    rotationRate.w = ((-q.x * w.x - q.y * w.y) - q.z * w.z) * 0.5f;
    rotationRate.z = ((-q.y * w.x + q.x * w.y) + q.w * w.z) * 0.5f;
    rotationRate.y = ((q.z * w.x + q.w * w.y) - q.x * w.z) * 0.5f;
    bodyFlags.rotationStale = 0;
}

void RigidBody::ApplyConstraint()
{
    ConstrainPosition(&constraint, PositionOf(&matrix));
    ConstrainVelocity(&constraint, &momentum);
    ConstrainVelocity(&constraint, &velocity);
    ConstrainRotation(&constraint, &rotation);
    if (constraint.flags.hinge != 0)
    {
        const Vector4& axis = constraint.hingeAxis;
        f32 along = angularMomentum.x * axis.x + angularMomentum.y * axis.y + angularMomentum.z * axis.z;
        Vector4 alongAxis;
        alongAxis.x = axis.x * along;
        alongAxis.y = axis.y * along;
        alongAxis.z = axis.z * along;
        alongAxis.w = 1.0f;
        angularMomentum = alongAxis;
    }

    if (constraint.flags.hinge != 0)
    {
        const Vector4& axis = constraint.hingeAxis;
        f32 along = angularVelocity.x * axis.x + angularVelocity.y * axis.y + angularVelocity.z * axis.z;
        Vector4 alongAxis;
        alongAxis.x = axis.x * along;
        alongAxis.y = axis.y * along;
        alongAxis.z = axis.z * along;
        alongAxis.w = 1.0f;
        angularVelocity = alongAxis;
    }
}

void RigidBody::SetMassAndSize(f32 newMass, f32 x, f32 y, f32 z)
{
    constexpr f32 Third = 0x1.555556p-2f;
    constexpr f32 Twelfth = 0x1.555556p-4f;
    f32 thirdY = y * Third;
    f32 thirdZ = z * Third;
    f32 thirdX = x * Third;
    // Every side at least a third of each other one
    f32 sizeX = thirdY;
    if (sizeX < thirdZ)
    {
        sizeX = thirdZ;
    }

    if (sizeX < x)
    {
        sizeX = x;
    }

    f32 sizeY = thirdX;
    if (sizeY < thirdZ)
    {
        sizeY = thirdZ;
    }

    if (sizeY < y)
    {
        sizeY = y;
    }

    f32 sizeZ = thirdX;
    if (sizeZ < thirdY)
    {
        sizeZ = thirdY;
    }

    if (sizeZ < z)
    {
        sizeZ = z;
    }

    f32 zz = sizeZ * sizeZ;
    f32 yy = sizeY * sizeY;
    mass = newMass;
    inverseMass = 1.0f / newMass;
    f32 xx = sizeX * sizeX;
    f32 share = newMass * Twelfth;
    inertia.x = share * (yy + zz);
    inertia.w = 1.0f;
    inertia.z = share * (xx + yy);
    inertia.y = share * (xx + zz);
    inverseInertia.x = 1.0f / inertia.x;
    inverseInertia.y = 1.0f / inertia.y;
    inverseInertia.z = 1.0f / inertia.z;
    inverseInertia.w = 1.0f;
    restInverseInertia = inverseInertia;
    UpdateState();
}

void RigidBody::WorldInertia(Matrix4x4* out)
{
    Matrix4x4 scaled = inverseMatrix;
    for (u32 row = 0; row < 3; row++)
    {
        f32 scale = (&inertia.x)[row];
        scaled.m[row][0] = scaled.m[row][0] * scale;
        scaled.m[row][1] = scaled.m[row][1] * scale;
        scaled.m[row][2] = scaled.m[row][2] * scale;
    }

    VuMultiplyMatrices(&scaled, &matrix, out);
}

void RigidBody::VelocityAt(const Vector4* point, Vector4* out)
{
    UpdateState();
    const Vector4* position = PositionOf(&matrix);
    f32 x = point->x - position->x;
    f32 y = point->y - position->y;
    f32 z = point->z - position->z;
    Vector4 result;
    result.x = velocity.x + (y * angularVelocity.z - z * angularVelocity.y);
    result.y = velocity.y + (z * angularVelocity.x - x * angularVelocity.z);
    result.z = velocity.z + (x * angularVelocity.y - y * angularVelocity.x);
    result.w = 1.0f;
    *out = result;
}

void RigidBody::SetAngularVelocity(const Vector4* value)
{
    angularVelocity = *value;
    Matrix4x4 inertiaMatrix;
    WorldInertia(&inertiaMatrix);
    VuRotateVector(&inertiaMatrix, &angularVelocity, &angularMomentum);
}

void RigidBody::Damp(f32 amount, f32 share)
{
    f32 cut = amount * share;
    f32 length = __builtin_sqrtf(momentum.x * momentum.x + momentum.y * momentum.y + momentum.z * momentum.z);
    if (!(__builtin_fabsf(length) <= Epsilon))
    {
        f32 left = length - cut;
        if (left < 0.0f)
        {
            left = 0.0f;
        }

        f32 scale = left / length;
        if (scale < 1.0f)
        {
            momentum.x = momentum.x * scale;
            momentum.y = momentum.y * scale;
            momentum.z = momentum.z * scale;
        }
    }

    length = __builtin_sqrtf(angularMomentum.x * angularMomentum.x + angularMomentum.y * angularMomentum.y +
                             angularMomentum.z * angularMomentum.z);
    if (__builtin_fabsf(length) <= Epsilon)
    {
        return;
    }

    f32 left = length - cut;
    if (left < 0.0f)
    {
        left = 0.0f;
    }

    f32 scale = left / length;
    if (scale < 1.0f)
    {
        angularMomentum.x = angularMomentum.x * scale;
        angularMomentum.y = angularMomentum.y * scale;
        angularMomentum.z = angularMomentum.z * scale;
    }

    // Retail marks the velocity out of date, not the angular one
    bodyFlags.velocityStale = 1;
}

void RigidBody::Step(f32 seconds)
{
    constexpr f32 MostMove = 0.5f;
    UpdateState();
    Vector4 move;
    move.x = velocity.x * seconds;
    move.y = velocity.y * seconds;
    move.z = velocity.z * seconds;
    move.w = 1.0f;
    if (MostMove < __builtin_sqrtf(move.x * move.x + move.y * move.y + move.z * move.z))
    {
        f32 inverse = InverseLength(&move, LengthEpsilon);
        move.x = move.x * inverse * MostMove;
        move.y = move.y * inverse * MostMove;
        move.z = move.z * inverse * MostMove;
    }

    Vector4* position = PositionOf(&matrix);
    position->x = position->x + move.x;
    position->y = position->y + move.y;
    position->z = position->z + move.z;
    momentum.x = momentum.x + force.x * seconds;
    momentum.y = momentum.y + force.y * seconds;
    momentum.z = momentum.z + force.z * seconds;
    angularMomentum.x = angularMomentum.x + torque.x * seconds;
    angularMomentum.y = angularMomentum.y + torque.y * seconds;
    angularMomentum.z = angularMomentum.z + torque.z * seconds;
    momentum.x = momentum.x + stepForce.x * seconds;
    momentum.y = momentum.y + stepForce.y * seconds;
    momentum.z = momentum.z + stepForce.z * seconds;
    angularMomentum.x = angularMomentum.x + stepTorque.x * seconds;
    angularMomentum.y = angularMomentum.y + stepTorque.y * seconds;
    angularMomentum.z = angularMomentum.z + stepTorque.z * seconds;
    rotation.x = rotation.x + rotationRate.x * seconds;
    rotation.y = rotation.y + rotationRate.y * seconds;
    rotation.z = rotation.z + rotationRate.z * seconds;
    rotation.w = rotation.w + rotationRate.w * seconds;
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, &rotation);
    rotation.x = rotation.x * inverse;
    rotation.y = rotation.y * inverse;
    rotation.z = rotation.z * inverse;
    rotation.w = rotation.w * inverse;
    bodyFlags.velocityStale = 1;
    bodyFlags.rotationStale = 1;
    inverseInertia = restInverseInertia;
    UpdateState();
    SetOrigin(&stepForce);
    SetOrigin(&stepTorque);
}

void RigidBody::ApplyImpulse(const Vector4* impulse, const Vector4* point)
{
    constexpr f32 OldShare = 0x1.666666p-1f;
    constexpr f32 NewShare = 0x1.333334p-2f;
    UpdateState();
    momentum.x = momentum.x + impulse->x;
    momentum.y = momentum.y + impulse->y;
    momentum.z = momentum.z + impulse->z;
    bodyFlags.velocityStale = 1;
    Vector4 local;
    VuRotateVector(&inverseMatrix, impulse, &local);
    Vector4 turn;
    turn.x = point->y * local.z - point->z * local.y;
    turn.y = point->z * local.x - point->x * local.z;
    turn.z = point->x * local.y - point->y * local.x;
    turn.w = 1.0f;
    Vector4 worldTurn;
    VuRotateVector(&matrix, &turn, &worldTurn);
    Vector4 recent;
    recent.x = recentTurn.x * OldShare + worldTurn.x * NewShare;
    recent.y = recentTurn.y * OldShare + worldTurn.y * NewShare;
    recent.z = recentTurn.z * OldShare + worldTurn.z * NewShare;
    recent.w = 1.0f;
    recentTurn = recent;
    angularMomentum.x = angularMomentum.x - worldTurn.x;
    angularMomentum.y = angularMomentum.y - worldTurn.y;
    angularMomentum.z = angularMomentum.z - worldTurn.z;
    bodyFlags.rotationStale = 1;
    impulseTotal = impulseTotal + __builtin_sqrtf(impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z);
}

namespace
{
// A force at a point added to a force and a torque (the force and the point taken into the body's space before its state is
// made again)
void AddForceTo(RigidBody* body, const Vector4* push, const Vector4* point, Vector4* force, Vector4* torque)
{
    Vector4 localPush;
    VuRotateVector(&body->inverseMatrix, push, &localPush);
    Vector4 localPoint;
    VuTransformPoint(&body->inverseMatrix, point, &localPoint);
    body->UpdateState();
    Vector4 worldPush;
    VuRotateVector(&body->matrix, &localPush, &worldPush);
    force->x = force->x + worldPush.x;
    force->y = force->y + worldPush.y;
    force->z = force->z + worldPush.z;
    Vector4 turn;
    turn.x = localPush.y * localPoint.z - localPush.z * localPoint.y;
    turn.y = localPush.z * localPoint.x - localPush.x * localPoint.z;
    turn.z = localPush.x * localPoint.y - localPush.y * localPoint.x;
    turn.w = 1.0f;
    Vector4 worldTurn;
    VuRotateVector(&body->matrix, &turn, &worldTurn);
    torque->x = torque->x + worldTurn.x;
    torque->y = torque->y + worldTurn.y;
    torque->z = torque->z + worldTurn.z;
}
}

void RigidBody::AddForce(const Vector4* push, const Vector4* point)
{
    AddForceTo(this, push, point, &force, &torque);
}

void RigidBody::AddStepForce(const Vector4* push, const Vector4* point)
{
    AddForceTo(this, push, point, &stepForce, &stepTorque);
}

namespace
{
// The velocity of a point of the body's own: its velocity and the point's arm (turned into the world) across its angular velocity
Vector4 LocalPointVelocity(RigidBody* body, const Vector4* localPoint)
{
    Vector4 arm;
    VuRotateVector(&body->matrix, localPoint, &arm);
    const Vector4& spin = body->angularVelocity;
    Vector4 velocity;
    velocity.x = body->velocity.x + (arm.y * spin.z - arm.z * spin.y);
    velocity.y = body->velocity.y + (arm.z * spin.x - arm.x * spin.z);
    velocity.z = body->velocity.z + (arm.x * spin.y - arm.y * spin.x);
    velocity.w = 1.0f;
    return velocity;
}


// The impulse of a contact that closes at a speed along its normal
f32 ContactImpulse(f32 restitution, f32 closing, f32 response)
{
    return -(restitution + 1.0f) * closing / response;
}

f32 SoftenedRestitution(f32 restitution, f32 softness, f32 closing)
{
    if (softness != 0.0f)
    {
        f32 squared = closing * closing;
        restitution = restitution * (squared / (squared + softness));
    }

    return restitution;
}
}

void RigidBody::Spring(f32 length, f32 stiffness, f32 damping, const Vector4* target, const Vector4* localPoint, u32 pullOnly)
{
    Vector4 point;
    VuTransformPoint(&matrix, localPoint, &point);
    Vector4 direction;
    direction.x = target->x - point.x;
    direction.y = target->y - point.y;
    direction.z = target->z - point.z;
    direction.w = 1.0f;
    f32 stretch = __builtin_sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z) - length;
    if (pullOnly != 0 && stretch < 0.0f)
    {
        return;
    }

    f32 inverse = InverseLength(&direction, LengthEpsilon);
    direction.x = direction.x * inverse;
    direction.y = direction.y * inverse;
    direction.z = direction.z * inverse;
    UpdateState();
    Vector4 pointVelocity = LocalPointVelocity(this, localPoint);
    f32 pull = stretch * stiffness -
               (pointVelocity.x * direction.x + pointVelocity.y * direction.y + pointVelocity.z * direction.z) * damping;
    Vector4 push;
    push.x = direction.x * pull;
    push.y = direction.y * pull;
    push.z = direction.z * pull;
    push.w = 1.0f;
    AddLocalForce(&push, localPoint);
}

void RigidBody::LevelSpring(f32 stiffness, f32 damping, f32 across, const Vector4* target, const Vector4* localPoint)
{
    Vector4 point;
    VuTransformPoint(&matrix, localPoint, &point);
    Vector4 flat = *target;
    flat.x = flat.x - point.x;
    flat.z = flat.z - point.z;
    f32 height = flat.y - point.y;
    flat.y = 0.0f;
    f32 distance = __builtin_sqrtf(flat.x * flat.x + flat.z * flat.z);
    // Multiplied out as retail does (the zeros' signs)
    f32 upX = Kept(0.0f);
    f32 upY = Kept(1.0f);
    f32 upZ = Kept(0.0f);
    f32 inverse = InverseLength(&flat, LengthEpsilon);
    flat.x = flat.x * inverse;
    flat.y = flat.y * inverse;
    flat.z = flat.z * inverse;
    UpdateState();
    Vector4 pointVelocity = LocalPointVelocity(this, localPoint);
    f32 lift = height * stiffness - pointVelocity.y * damping;
    f32 pull = distance * (stiffness * across) -
               (pointVelocity.x * flat.x + pointVelocity.y * flat.y + pointVelocity.z * flat.z) * damping * across;
    Vector4 push;
    push.x = upX * lift + flat.x * pull;
    push.y = upY * lift + flat.y * pull;
    push.z = upZ * lift + flat.z * pull;
    push.w = 1.0f;
    AddLocalForce(&push, localPoint);
}

f32 RigidBody::ImpulseResponse(const Vector4* localPoint, const Vector4* normal)
{
    Vector4 localNormal;
    VuRotateVector(&inverseMatrix, normal, &localNormal);
    Vector4 turn;
    turn.x = localNormal.y * localPoint->z - localNormal.z * localPoint->y;
    turn.y = localNormal.z * localPoint->x - localNormal.x * localPoint->z;
    turn.z = localNormal.x * localPoint->y - localNormal.y * localPoint->x;
    turn.w = 1.0f;
    Vector4 worldTurn;
    VuRotateVector(&matrix, &turn, &worldTurn);
    Vector4 spinChange;
    VuRotateVector(&worldInverseInertia, &worldTurn, &spinChange);
    Vector4 arm;
    VuRotateVector(&matrix, localPoint, &arm);
    f32 x = arm.y * spinChange.z - arm.z * spinChange.y;
    f32 y = arm.z * spinChange.x - arm.x * spinChange.z;
    f32 z = arm.x * spinChange.y - arm.y * spinChange.x;
    return x * normal->x + y * normal->y + z * normal->z + inverseMass;
}

f32 RigidBody::PointImpulse(f32 restitution, f32 softness, const Vector4* localPoint, const Vector4* normal)
{
    UpdateState();
    UpdateState();
    Vector4 pointVelocity = LocalPointVelocity(this, localPoint);
    f32 closing = pointVelocity.x * normal->x + pointVelocity.y * normal->y + pointVelocity.z * normal->z;
    if (0.0f < closing)
    {
        return 0.0f;
    }

    restitution = SoftenedRestitution(restitution, softness, closing);
    return ContactImpulse(restitution, closing, ImpulseResponse(localPoint, normal));
}

f32 RigidBody::TwoBodyImpulse(f32 restitution, f32 softness, const Vector4* localPoint, RigidBody* other,
                              const Vector4* otherLocalPoint, const Vector4* normal)
{
    UpdateState();
    UpdateState();
    Vector4 pointVelocity = LocalPointVelocity(this, localPoint);
    other->UpdateState();
    Vector4 otherVelocity = LocalPointVelocity(other, otherLocalPoint);
    Vector4 relative;
    relative.x = otherVelocity.x - pointVelocity.x;
    relative.y = otherVelocity.y - pointVelocity.y;
    relative.z = otherVelocity.z - pointVelocity.z;
    f32 closing = relative.x * normal->x + relative.y * normal->y + relative.z * normal->z;
    if (0.0f < closing)
    {
        return 0.0f;
    }

    restitution = SoftenedRestitution(restitution, softness, closing);
    f32 response = ImpulseResponse(localPoint, normal);
    response = response + other->ImpulseResponse(otherLocalPoint, normal);
    return ContactImpulse(restitution, closing, response);
}

f32 RigidBody::MovingPointImpulse(f32 restitution, f32 softness, const Vector4* localPoint, const Vector4* pointVelocity,
                                  const Vector4* normal)
{
    UpdateState();
    UpdateState();
    Vector4 velocity = LocalPointVelocity(this, localPoint);
    Vector4 relative;
    relative.x = pointVelocity->x - velocity.x;
    relative.y = pointVelocity->y - velocity.y;
    relative.z = pointVelocity->z - velocity.z;
    f32 closing = relative.x * normal->x + relative.y * normal->y + relative.z * normal->z;
    if (0.0f < closing)
    {
        return 0.0f;
    }

    restitution = SoftenedRestitution(restitution, softness, closing);
    return ContactImpulse(restitution, closing, ImpulseResponse(localPoint, normal));
}

void RigidBody::AngularFriction(f32 along, f32 across, const Vector4* impulse, const Vector4* target)
{
    Matrix4x4 inertiaMatrix;
    WorldInertia(&inertiaMatrix);
    Vector4 spinNow = angularVelocity;
    Vector4 difference;
    difference.x = target->x - spinNow.x;
    difference.y = target->y - spinNow.y;
    difference.z = target->z - spinNow.z;
    difference.w = 1.0f;
    Vector4 change;
    VuRotateVector(&inertiaMatrix, &difference, &change);
    Vector4 axis = *impulse;
    f32 inverse = InverseLength(&axis, LengthEpsilon);
    axis.x = axis.x * inverse;
    axis.y = axis.y * inverse;
    axis.z = axis.z * inverse;
    // The change about the impulse's direction, at most the impulse's length times the first share
    f32 aboutAxis = axis.x * change.x + axis.y * change.y + axis.z * change.z;
    Vector4 twist;
    twist.x = axis.x * aboutAxis;
    twist.y = axis.y * aboutAxis;
    twist.z = axis.z * aboutAxis;
    twist.w = 1.0f;
    f32 most = __builtin_sqrtf(impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z) * along;
    if (most < __builtin_sqrtf(twist.x * twist.x + twist.y * twist.y + twist.z * twist.z))
    {
        f32 scale = InverseLength(&twist, LengthEpsilon);
        twist.x = twist.x * scale * most;
        twist.y = twist.y * scale * most;
        twist.z = twist.z * scale * most;
    }

    // And the rest of it, at most the impulse's length times the second
    aboutAxis = axis.x * change.x + axis.y * change.y + axis.z * change.z;
    Vector4 roll;
    roll.x = change.x - axis.x * aboutAxis;
    roll.y = change.y - axis.y * aboutAxis;
    roll.z = change.z - axis.z * aboutAxis;
    roll.w = change.w;
    most = __builtin_sqrtf(impulse->x * impulse->x + impulse->y * impulse->y + impulse->z * impulse->z) * across;
    if (most < __builtin_sqrtf(roll.x * roll.x + roll.y * roll.y + roll.z * roll.z))
    {
        f32 scale = InverseLength(&roll, LengthEpsilon);
        roll.x = roll.x * scale * most;
        roll.y = roll.y * scale * most;
        roll.z = roll.z * scale * most;
    }

    angularMomentum.x = angularMomentum.x + twist.x;
    angularMomentum.y = angularMomentum.y + twist.y;
    angularMomentum.z = angularMomentum.z + twist.z;
    angularMomentum.x = angularMomentum.x + roll.x;
    angularMomentum.y = angularMomentum.y + roll.y;
    angularMomentum.z = angularMomentum.z + roll.z;
}

void VelocitiesBetween(f32 seconds, const Matrix4x4* before, const Matrix4x4* now, const Vector4* point, Vector4* velocity,
                       Vector4* angularVelocity)
{
    Matrix4x4 inverse = *now;
    VuInvertRigidInPlace(&inverse);
    Vector4 local;
    VuTransformPoint(&inverse, point, &local);
    Vector4 moved;
    VuTransformPoint(before, &local, &moved);
    f32 rate = 1.0f / seconds;
    Vector4 result;
    result.x = rate * (point->x - moved.x);
    result.y = rate * (point->y - moved.y);
    result.z = rate * (point->z - moved.z);
    result.w = 1.0f;
    *velocity = result;
    inverse = *before;
    VuInvertRigidInPlace(&inverse);
    Matrix4x4 relative;
    VuMultiplyMatrices(now, &inverse, &relative);
    Vector4 rotation;
    GetRotationVec(&rotation, &relative);
    Vector4 axis;
    s32 angle;
    AxisAngleOfRotation(&rotation, &axis, &angle, 0);
    f32 radians = static_cast<f32>(angle) * AngleToRadians;
    f32 backRate = 1.0f / -seconds;
    axis.z = axis.z * radians * backRate;
    axis.x = axis.x * radians * backRate;
    axis.y = axis.y * radians * backRate;
    *angularVelocity = axis;
}

DynamicBody* DynamicBody::Construct(DynamicBody* body)
{
    RigidBody::Construct(body);
    body->bits.placesInstance = 1;
    body->bits.noCollisions = 0;
    body->bits.tellsLanding = 1;
    body->bits.placesPositionOnly = 0;
    body->vtable = g_DynamicBodyVTable;
    body->instance = nullptr;
    body->centerOfMass = {0.0f, 0.0f, 0.0f, 1.0f};
    body->restitution = Rounded(0.3);
    body->softness = 4.0f;
    body->friction = 0.5f;
    body->rollFriction = Rounded(0.01);
    body->lengthDrag = Rounded(0.001);
    body->buoyancy = -1.0f;
    body->spinFriction = Rounded(0.01);
    body->drag = Rounded(0.001);
    body->unused32C = 1.0f;
    body->cache = nullptr;
    body->restingFrames = 0;
    body->collisionMask = 0;
    body->limitedPushes = {0.0f, 0.0f, 0.0f, 1.0f};
    body->pushes = {0.0f, 0.0f, 0.0f, 1.0f};
    body->pushedLength = 0.0f;
    body->contactCallback = nullptr;
    body->touchCallback = nullptr;
    SetOrigin(&body->unused200);
    return body;
}

void DynamicBody::Destroy(u32 destroyFlags)
{
    vtable = g_DynamicBodyVTable;
    RemovePhysicsBody(g_PhysicsWorld, this);
    if (cache != nullptr)
    {
        DestroyCollisionCache(cache, DestroyAndFree);
    }

    vtable = g_RigidBodyVTable;
    GameNode::Destroy(destroyFlags);
}

void DynamicBody::SetOwner(InstanceContext* owner)
{
    instance = owner;
    GameNode::SetOwner(owner);
}

void DynamicBody::Float()
{
}

void DynamicBody::SetCenterOfMass(const Vector4* center)
{
    Vector4 moved;
    moved.x = center->x - centerOfMass.x;
    moved.y = center->y - centerOfMass.y;
    moved.z = center->z - centerOfMass.z;
    moved.w = 1.0f;
    VuRotateVector(&matrix, &moved, &moved);
    Vector4* position = PositionOf(&matrix);
    position->x = position->x + moved.x;
    position->y = position->y + moved.y;
    position->z = position->z + moved.z;
    inverseMatrix = matrix;
    VuInvertRigidInPlace(&inverseMatrix);
    centerOfMass = *center;
}

void SphereBody::Destroy(u32 destroyFlags)
{
    DynamicBody::Destroy(destroyFlags);
}

namespace
{
// Each pair once, from the body first in memory, unless the other one is left out of the frames
bool CollidesFromHere(const DynamicBody* body, const DynamicBody* other)
{
    return other->bodyFlags.leftOut != 0 || !(other < body);
}
}

u32 SphereBody::Collide(DynamicBody* other)
{
    if (CallVirtual<u32>(other, other->vtable, IsSphereSlot) != 0)
    {
        if (!CollidesFromHere(this, other))
        {
            return 0;
        }

        auto* sphere = static_cast<SphereBody*>(other);
        if (ellipsoid != 0 || sphere->ellipsoid != 0)
        {
            return CollideEllipsoids(this, sphere);
        }

        return CollideSpheres(this, sphere);
    }

    if (!CollidesFromHere(this, other))
    {
        return 0;
    }

    return CollideSphereWithHulls(this, other);
}

u32 SphereBody::IsSphere()
{
    return 1;
}

u32 SphereBody::TouchesBox(const Box*)
{
    return 0;
}

void HullBody::Destroy(u32 destroyFlags)
{
    vtable = g_HullBodyVTable;
    if (hulls != nullptr)
    {
        CollisionHull* hull = hulls + ArrayCount(hulls);
        while (hull != hulls)
        {
            hull--;
            HullDestroy(hull, 0);
        }

        DeleteArray(hulls);
    }

    hulls = nullptr;
    DynamicBody::Destroy(destroyFlags);
}

u32 HullBody::Collide(DynamicBody* other)
{
    if (CallVirtual<u32>(other, other->vtable, IsSphereSlot) != 0)
    {
        if (!CollidesFromHere(this, other))
        {
            return 0;
        }

        return CollideSphereWithHulls(static_cast<SphereBody*>(other), this);
    }

    if (!CollidesFromHere(this, other))
    {
        return 0;
    }

    return CollideHullBodies(this, other);
}

u32 HullBody::IsSphere()
{
    return 0;
}

u32 HullBody::TouchesBox(const Box* box)
{
    return BoxesOverlap(instance->CollisionBox(), box);
}

void HullBody::SetCenterOfMass(const Vector4* center)
{
    DynamicBody::SetCenterOfMass(center);
    CopyHulls(&instance->collision);
}

PhysicsWorld* ConstructPhysicsWorld(PhysicsWorld* world)
{
    constexpr s32 Slots = PhysicsWorld::Slots;
    world->capacity = Slots;
    world->bodies = static_cast<DynamicBody**>(MemoryAllocate2(Slots * sizeof(DynamicBody*)));
    world->freeCapacity = Slots;
    world->freeSlots = static_cast<s16*>(MemoryAllocate2(Slots * sizeof(s16)));
    for (s32 slot = 0; slot < Slots; slot++)
    {
        world->bodies[slot] = nullptr;
        world->freeSlots[slot] = static_cast<s16>(slot);
    }

    world->used = 0;
    return world;
}

DynamicBody* AddPhysicsBody(PhysicsWorld* world, InstanceContext* instance, u32 sphere)
{
    // Taken by a body (a free slot's index is no slot's)
    constexpr s16 TakenSlot = -1;
    s16 slot = world->freeSlots[world->used];
    DynamicBody* body;
    if (sphere != 0)
    {
        auto* made = static_cast<SphereBody*>(MemoryAllocate(sizeof(SphereBody)));
        DynamicBody::Construct(made);
        made->vtable = g_SphereBodyVTable;
        made->SetMassAndSize(DefaultMass, SphereSize, SphereSize, SphereSize);
        made->scaleX = 1.0f;
        made->radius = SphereRadius;
        made->ellipsoid = 0;
        made->scaleZ = 1.0f;
        made->scaleY = 1.0f;
        body = made;
    }
    else
    {
        auto* made = static_cast<HullBody*>(MemoryAllocate(sizeof(HullBody)));
        DynamicBody::Construct(made);
        made->hulls = nullptr;
        made->hullCount = 0;
        made->vtable = g_HullBodyVTable;
        body = made;
    }

    RegisterNode(instance, AttachNode, body);
    body->slot = slot;
    world->bodies[slot] = body;
    world->freeSlots[world->used] = TakenSlot;
    world->used++;
    return body;
}

void RemovePhysicsBody(PhysicsWorld* world, DynamicBody* body)
{
    s16 slot = body->slot;
    if (world->bodies[slot] != body)
    {
        return;
    }

    if (body->owner->chunk == nullptr)
    {
        // Retail looks the object node up and does nothing with it
        GetGameNode(&body->owner->nodes, NodeObject);
    }
    else
    {
        RemoveNode(body->instance, body);
    }

    world->bodies[slot] = nullptr;
    world->used--;
    world->freeSlots[world->used] = slot;
}

u32 DynamicBody::SurfaceContact(const Vector4* localPoint, const Vector4* normal, CollisionSurface* surface, Vector4* impulseOut)
{
    constexpr f32 HardLanding = Rounded(1.8);
    constexpr f32 Sliding = Rounded(0.01);
    constexpr f32 ScrapeShare = Rounded(0.3);
    f32 most = __builtin_fminf(surface->friction, friction);
    f32 spin = spinFriction * surface->spinFriction;
    f32 roll = rollFriction * surface->rollFriction;
    f32 strength = PointImpulse(restitution * surface->restitution, softness, localPoint, normal);
    Vector4 impulse;
    impulse.x = normal->x * strength;
    impulse.y = normal->y * strength;
    impulse.z = normal->z * strength;
    impulse.w = 1.0f;
    ApplyImpulse(&impulse, localPoint);
    if (impulseOut != nullptr)
    {
        *impulseOut = impulse;
        impulseOut->x = -impulseOut->x;
        impulseOut->y = -impulseOut->y;
        impulseOut->z = -impulseOut->z;
    }

    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    ObjectRigidBody* rider = node->rigidBody;
    if (surface != nullptr && rider != nullptr && surface->flags.sendsContactMessageToObjects != 0)
    {
        Agent* agent = node->agent;
        CallVirtual<void>(agent, agent->vtable, Agent::ContactSlot, &surface->contact, owner, 0u);
        rider->state.touchedMessageSurface = 1;
    }

    // Its velocity before the impulse (its state isn't made again yet)
    Vector4 velocityBefore = velocity;
    if (bits.tellsLanding != 0)
    {
        Vector4 change = impulse;
        change.x = change.x * inverseMass;
        change.y = change.y * inverseMass;
        change.z = change.z * inverseMass;
        if (HardLanding < change.x * change.x + change.y * change.y + change.z * change.z)
        {
            Vector4 point = *localPoint;
            VuTransformPoint(&matrix, &point, &point);
            u32 told = CallVirtual<u32>(node, node->vtable, ObjectNode::LandedSlot, surface, &point, &change);
            bits.landed = told & 1;
        }
    }

    UpdateState();
    Vector4 pointVelocity = LocalPointVelocity(this, localPoint);
    Vector4 slide;
    slide.x = -pointVelocity.x;
    slide.y = -pointVelocity.y;
    slide.z = -pointVelocity.z;
    slide.w = pointVelocity.w;
    f32 along = slide.x * normal->x + slide.y * normal->y + slide.z * normal->z;
    slide.x = slide.x - normal->x * along;
    slide.y = slide.y - normal->y * along;
    slide.z = slide.z - normal->z * along;
    if (Sliding < __builtin_sqrtf(slide.x * slide.x + slide.y * slide.y + slide.z * slide.z))
    {
        f32 limit = strength * most;
        f32 inverse = InverseLength(&slide, LengthEpsilon);
        slide.x = slide.x * inverse;
        slide.y = slide.y * inverse;
        slide.z = slide.z * inverse;
        f32 rub = PointImpulse(0.0f, 0.0f, localPoint, &slide);
        Vector4 rubbing;
        rubbing.x = slide.x * rub;
        rubbing.y = slide.y * rub;
        rubbing.z = slide.z * rub;
        rubbing.w = 1.0f;
        if (limit < __builtin_sqrtf(rubbing.x * rubbing.x + rubbing.y * rubbing.y + rubbing.z * rubbing.z))
        {
            f32 scale = InverseLength(&rubbing, LengthEpsilon);
            rubbing.x = rubbing.x * scale * limit;
            rubbing.y = rubbing.y * scale * limit;
            rubbing.z = rubbing.z * scale * limit;
        }

        ApplyImpulse(&rubbing, localPoint);
    }

    Vector4 point = *localPoint;
    VuTransformPoint(&matrix, &point, &point);
    if (CallVirtual<u32>(this, vtable, IsSphereSlot) != 0)
    {
        Vector4 scraping;
        scraping.x = velocityBefore.x * ScrapeShare;
        scraping.y = velocityBefore.y * ScrapeShare;
        scraping.z = velocityBefore.z * ScrapeShare;
        scraping.w = 1.0f;
        u32 told = CallVirtual<u32>(node, node->vtable, ObjectNode::ScrapedSlot, surface, &point, &scraping);
        bits.scraped = told & 1;
    }
    else if (bits.landed == 0)
    {
        u32 told = CallVirtual<u32>(node, node->vtable, ObjectNode::LandedHardSlot, surface, &point, &velocityBefore);
        bits.landedHard = told & 1;
    }

    AngularFriction(spin, roll, &impulse, &g_DefaultBox.min);
    return 1;
}

u32 DynamicBody::BodyContact(const Vector4* point, const Vector4* normal, DynamicBody* other)
{
    f32 bounce = restitution * other->restitution;
    f32 most = __builtin_fminf(other->friction, friction);
    f32 soft = softness;
    Vector4 local;
    VuTransformPoint(&inverseMatrix, point, &local);
    Vector4 otherLocal;
    VuTransformPoint(&other->inverseMatrix, point, &otherLocal);
    f32 strength = TwoBodyImpulse(bounce, soft, &local, other, &otherLocal, normal);
    f32 negative = -strength;
    Vector4 impulse;
    impulse.x = normal->x * negative;
    impulse.y = normal->y * negative;
    impulse.z = normal->z * negative;
    impulse.w = 1.0f;
    ApplyImpulse(&impulse, &local);
    if (owner->flags.physicsBody)
    {
        auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
        CallVirtual<void>(node, node->vtable, ObjectNode::CollidedSlot, other->owner, point, &impulse);
    }

    impulse.x = -impulse.x;
    impulse.y = -impulse.y;
    impulse.z = -impulse.z;
    if (other->owner->flags.physicsBody)
    {
        // The other's node is looked up through its owner, this one's through its instance
        auto* node = static_cast<ObjectNode*>(GetGameNode(&other->owner->nodes, NodeObject));
        CallVirtual<void>(node, node->vtable, ObjectNode::CollidedSlot, owner, point, &impulse);
    }

    other->ApplyImpulse(&impulse, &otherLocal);
    f32 limit = strength * most;
    UpdateState();
    Vector4 velocityHere = LocalPointVelocity(this, &local);
    other->UpdateState();
    Vector4 velocityThere = LocalPointVelocity(other, &otherLocal);
    Vector4 slide;
    slide.x = velocityHere.x - velocityThere.x;
    slide.y = velocityHere.y - velocityThere.y;
    slide.z = velocityHere.z - velocityThere.z;
    slide.w = 1.0f;
    f32 along = slide.x * normal->x + slide.y * normal->y + slide.z * normal->z;
    slide.x = slide.x - normal->x * along;
    slide.y = slide.y - normal->y * along;
    slide.z = slide.z - normal->z * along;
    f32 inverse = InverseLength(&slide, LengthEpsilon);
    slide.x = slide.x * inverse;
    slide.y = slide.y * inverse;
    slide.z = slide.z * inverse;
    f32 rub = -TwoBodyImpulse(0.0f, 0.0f, &local, other, &otherLocal, &slide);
    Vector4 rubbing;
    rubbing.x = slide.x * rub;
    rubbing.y = slide.y * rub;
    rubbing.z = slide.z * rub;
    rubbing.w = 1.0f;
    if (limit < __builtin_sqrtf(rubbing.x * rubbing.x + rubbing.y * rubbing.y + rubbing.z * rubbing.z))
    {
        f32 scale = InverseLength(&rubbing, LengthEpsilon);
        rubbing.x = rubbing.x * scale * limit;
        rubbing.y = rubbing.y * scale * limit;
        rubbing.z = rubbing.z * scale * limit;
    }

    ApplyImpulse(&rubbing, &local);
    rubbing.x = -rubbing.x;
    rubbing.y = -rubbing.y;
    rubbing.z = -rubbing.z;
    other->ApplyImpulse(&rubbing, &otherLocal);
    return 1;
}

u32 DynamicBody::MovingPointContact(const Vector4* point, const Vector4* normal, const Vector4* pointVelocity,
                                    const Vector4* spinTarget, Vector4* impulseOut)
{
    f32 most = __builtin_fminf(1.0f, friction);
    f32 spin = __builtin_fminf(1.0f, spinFriction);
    f32 bounce = restitution;
    f32 roll = __builtin_fminf(1.0f, rollFriction);
    f32 soft = softness;
    Vector4 local;
    VuTransformPoint(&inverseMatrix, point, &local);
    f32 strength = MovingPointImpulse(bounce, soft, &local, pointVelocity, normal);
    f32 negative = -strength;
    Vector4 impulse;
    impulse.x = normal->x * negative;
    impulse.y = normal->y * negative;
    impulse.z = normal->z * negative;
    impulse.w = 1.0f;
    ApplyImpulse(&impulse, &local);
    if (impulseOut != nullptr)
    {
        *impulseOut = impulse;
        impulseOut->x = -impulseOut->x;
        impulseOut->y = -impulseOut->y;
        impulseOut->z = -impulseOut->z;
    }

    f32 limit = strength * most;
    UpdateState();
    Vector4 velocityHere = LocalPointVelocity(this, &local);
    Vector4 slide;
    slide.x = velocityHere.x - pointVelocity->x;
    slide.y = velocityHere.y - pointVelocity->y;
    slide.z = velocityHere.z - pointVelocity->z;
    slide.w = 1.0f;
    f32 along = slide.x * normal->x + slide.y * normal->y + slide.z * normal->z;
    slide.x = slide.x - normal->x * along;
    slide.y = slide.y - normal->y * along;
    slide.z = slide.z - normal->z * along;
    f32 inverse = InverseLength(&slide, LengthEpsilon);
    slide.x = slide.x * inverse;
    slide.y = slide.y * inverse;
    slide.z = slide.z * inverse;
    f32 rub = -MovingPointImpulse(0.0f, 0.0f, &local, pointVelocity, &slide);
    Vector4 rubbing;
    rubbing.x = slide.x * rub;
    rubbing.y = slide.y * rub;
    rubbing.z = slide.z * rub;
    rubbing.w = 1.0f;
    if (limit < __builtin_sqrtf(rubbing.x * rubbing.x + rubbing.y * rubbing.y + rubbing.z * rubbing.z))
    {
        f32 scale = InverseLength(&rubbing, LengthEpsilon);
        rubbing.x = rubbing.x * scale * limit;
        rubbing.y = rubbing.y * scale * limit;
        rubbing.z = rubbing.z * scale * limit;
    }

    ApplyImpulse(&rubbing, &local);
    // Rubbed by the sliding's impulse, where the other answers use the normal's
    AngularFriction(spin, roll, &rubbing, spinTarget);
    return 1;
}

u32 DynamicBody::InstanceContact(const Vector4* localPoint, const Vector4* normal, CollisionSurface* surface, InstanceContext* other)
{
    auto* otherNode = static_cast<ObjectNode*>(GetGameNode(&other->nodes, NodeObject));
    u32 answer = 1;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    MovementNode* movement = nullptr;
    if (other != nullptr)
    {
        movement = static_cast<MovementNode*>(GetGameNode(&other->nodes, NodeMovement));
    }

    if (movement != nullptr)
    {
        // A moving instance: the point of contact moves as the instance did since its last place
        Matrix4x4 before = *movement->PreviousMatrix();
        Matrix4x4 now = *movement->CurrentMatrix();
        f32 seconds = movement->Seconds();
        Vector4 point;
        VuTransformPoint(&matrix, localPoint, &point);
        Vector4 pointVelocity;
        Vector4 spinTarget;
        VelocitiesBetween(seconds, &before, &now, &point, &pointVelocity, &spinTarget);
        Vector4 away = *normal;
        away.x = -away.x;
        away.y = -away.y;
        away.z = -away.z;
        f32 bounce = restitution;
        f32 soft = softness;
        Vector4 local;
        VuTransformPoint(&inverseMatrix, &point, &local);
        f32 strength = -MovingPointImpulse(bounce, soft, &local, &pointVelocity, &away);
        Vector4 pushed;
        pushed.x = -(away.x * strength);
        pushed.y = -(away.y * strength);
        pushed.z = -(away.z * strength);
        pushed.w = 1.0f;
        if (otherNode != nullptr)
        {
            Vector4 knock = pushed;
            knock.x = knock.x * knockScale;
            knock.y = knock.y * knockScale;
            knock.z = knock.z * knockScale;
            answer = CallVirtual<u32>(otherNode, otherNode->vtable, ObjectNode::CollidedSlot, instance, &point, &knock);
        }

        if (node != nullptr)
        {
            CallVirtual<void>(node, node->vtable, ObjectNode::BumpedSlot, other, &velocity, &pushed);
            if (instance->flags.physicsBody)
            {
                Vector4 knock;
                knock.x = -pushed.x;
                knock.y = -pushed.y;
                knock.z = -pushed.z;
                knock.w = 1.0f;
                CallVirtual<void>(node, node->vtable, ObjectNode::CollidedSlot, other, &point, &knock);
            }
        }

        bits.tellsLanding = 0;
        if (answer == 0)
        {
            return 0;
        }

        return MovingPointContact(&point, &away, &pointVelocity, &spinTarget, nullptr);
    }

    // A still one: a surface
    f32 strength = PointImpulse(restitution, softness, localPoint, normal);
    Vector4 pushed;
    pushed.x = -(normal->x * strength);
    pushed.y = -(normal->y * strength);
    pushed.z = -(normal->z * strength);
    pushed.w = 1.0f;
    Vector4 point;
    bool pointMade = false;
    if (otherNode != nullptr)
    {
        VuTransformPoint(&matrix, localPoint, &point);
        pointMade = true;
        Vector4 knock = pushed;
        knock.x = knock.x * knockScale;
        knock.y = knock.y * knockScale;
        knock.z = knock.z * knockScale;
        answer = CallVirtual<u32>(otherNode, otherNode->vtable, ObjectNode::CollidedSlot, instance, &point, &knock);
    }

    if (node != nullptr)
    {
        CallVirtual<void>(node, node->vtable, ObjectNode::BumpedSlot, other, &velocity, &pushed);
        if (instance->flags.physicsBody)
        {
            if (!pointMade)
            {
                // Retail passes what its stack had there: the point it would have made stands in
                VuTransformPoint(&matrix, localPoint, &point);
            }

            Vector4 knock;
            knock.x = -pushed.x;
            knock.y = -pushed.y;
            knock.z = -pushed.z;
            knock.w = 1.0f;
            CallVirtual<void>(node, node->vtable, ObjectNode::CollidedSlot, other, &point, &knock);
        }
    }

    bits.tellsLanding = 0;
    if (answer == 0)
    {
        return 0;
    }

    return SurfaceContact(localPoint, normal, surface, nullptr);
}

namespace
{
// The inverse matrix made again from the matrix
void InvertMatrix(RigidBody* body)
{
    body->inverseMatrix = body->matrix;
    VuInvertRigidInPlace(&body->inverseMatrix);
}
}

namespace
{
// An instance put at a matrix: only its position (queued when that moved it), or the whole matrix (queued)
void PlaceInstanceAt(InstanceContext* instance, Matrix4x4* placed, bool positionOnly)
{
    if (!positionOnly)
    {
        if (SetPlaceMatrix(instance->place, placed) != 0)
        {
            QueueObject(instance);
        }

        return;
    }

    ObjectPlace* place = instance->place;
    if (place->bits.matrixMoved != 0)
    {
        place->position = *RowAt(&place->matrix, 3);
        place->MarkPositionSynced();
    }

    const Vector4* position = RowAt(placed, 3);
    if (!(position->x == place->position.x && position->y == place->position.y && position->z == place->position.z))
    {
        place->MarkMoved();
        place->position = *position;
        QueueObject(instance);
    }
}

// Its instance put where the matrix puts the centre of mass's opposite
void PlaceInstanceAtCenter(DynamicBody* body)
{
    Matrix4x4 placed = body->matrix;
    Vector4 center = body->centerOfMass;
    VuRotateVector(&placed, &center, &center);
    Vector4* position = RowAt(&placed, 3);
    position->x = position->x - center.x;
    position->y = position->y - center.y;
    position->z = position->z - center.z;
    PlaceInstanceAt(body->instance, &placed, body->bits.placesPositionOnly != 0);
}
}

void DynamicBody::MoveTo(InstanceContext* to, u32 withMotion)
{
    ObjectPlace* place = to->place;
    RotateAndTranslate(place);
    Matrix4x4 placed = place->matrix;
    Vector4 center = centerOfMass;
    VuRotateVector(&placed, &center, &center);
    Vector4* position = RowAt(&placed, 3);
    position->x = position->x + center.x;
    position->y = position->y + center.y;
    position->z = position->z + center.z;
    *PositionOf(&matrix) = *position;
    Matrix4x4 turn;
    TransposeMatrix(&placed, 3, &turn);
    GetRotationVec(&rotation, &turn);
    bodyFlags.velocityStale = 1;
    bodyFlags.rotationStale = 1;
    if (withMotion == 0)
    {
        return;
    }

    auto* movement = to != nullptr ? static_cast<MovementNode*>(GetGameNode(&to->nodes, NodeMovement)) : nullptr;
    if (movement == nullptr)
    {
        return;
    }

    Matrix4x4 before = *movement->PreviousMatrix();
    Matrix4x4 now = *movement->CurrentMatrix();
    f32 seconds = movement->Seconds();
    const Vector4* from = PositionOf(&before);
    const Vector4* at = PositionOf(&now);
    Vector4 moved;
    moved.x = at->x - from->x;
    moved.y = at->y - from->y;
    moved.z = at->z - from->z;
    moved.w = 1.0f;
    f32 rate = 1.0f / seconds;
    Vector4 speed = moved;
    speed.x = speed.x * rate;
    speed.y = speed.y * rate;
    speed.z = speed.z * rate;
    bodyFlags.velocityStale = 0;
    velocity = speed;
    Vector4 pushed;
    pushed.x = velocity.x * mass;
    pushed.y = velocity.y * mass;
    pushed.z = velocity.z * mass;
    pushed.w = 1.0f;
    momentum = pushed;
    UpdateState();
    Matrix4x4 inverse = before;
    VuInvertRigidInPlace(&inverse);
    Matrix4x4 relative;
    VuMultiplyMatrices(&inverse, &now, &relative);
    Vector4 turned;
    GetRotationVec(&turned, &relative);
    Vector4 axis;
    s32 angle;
    AxisAngleOfRotation(&turned, &axis, &angle, 0);
    f32 radians = static_cast<f32>(angle) * AngleToRadians;
    f32 backRate = 1.0f / -seconds;
    axis.z = axis.z * radians * backRate;
    axis.x = axis.x * radians * backRate;
    axis.y = axis.y * radians * backRate;
    SetAngularVelocity(&axis);
}

void DynamicBody::PlaceInstance()
{
    // The frames without a hard landing or a scrape its sound goes on for (a sphere's)
    constexpr u32 QuietFrames = 5;
    constexpr u32 SphereQuietFrames = 2;
    if (bits.placesInstance != 0)
    {
        PlaceInstanceAtCenter(this);
    }

    auto* node = static_cast<ObjectNode*>(GetGameNode(&owner->nodes, NodeObject));
    if (bits.landedHard != 0 || bits.scraped != 0)
    {
        restingFrames = 0;
        return;
    }

    restingFrames++;
    u32 most = CallVirtual<u32>(this, vtable, IsSphereSlot) != 0 ? SphereQuietFrames : QuietFrames;
    if (most < restingFrames)
    {
        CallVirtual<void>(node, node->vtable, ObjectNode::StopSoundSlot);
    }
}

void DynamicBody::StepDamping(f32 seconds)
{
    constexpr f32 Pushed = Rounded(0.01);
    // The drag's scale while a step force pushes a floating body (a sphere's)
    constexpr f32 PushedDragScale = 70.0f;
    constexpr f32 SphereDragScale = 4.0f;
    Damp(lengthDrag, seconds);
    f32 kept;
    if (buoyancy < 0.0f)
    {
        kept = 1.0f - drag * seconds * FramesPerSecond;
    }
    else
    {
        if (!(Pushed < stepForce.x * stepForce.x + stepForce.y * stepForce.y + stepForce.z * stepForce.z))
        {
            return;
        }

        f32 scale = CallVirtual<u32>(this, vtable, IsSphereSlot) != 0 ? SphereDragScale : PushedDragScale;
        kept = 1.0f - drag * scale * seconds * FramesPerSecond;
    }

    if (kept < 0.0f)
    {
        kept = 0.0f;
    }

    momentum.x = momentum.x * kept;
    momentum.y = momentum.y * kept;
    momentum.z = momentum.z * kept;
    angularMomentum.x = angularMomentum.x * kept;
    angularMomentum.y = angularMomentum.y * kept;
    angularMomentum.z = angularMomentum.z * kept;
    bodyFlags.velocityStale = 1;
}

void DynamicBody::PushOut(const Vector4* push, u32 limited)
{
    Vector4 half = *push;
    half.x = half.x * 0.5f;
    half.y = half.y * 0.5f;
    half.z = half.z * 0.5f;
    pushes.x = pushes.x + half.x;
    pushes.y = pushes.y + half.y;
    pushes.z = pushes.z + half.z;
    pushedLength = pushedLength + __builtin_sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
    if (limited != 0)
    {
        LimitedPush(&half);
        return;
    }

    Vector4* position = PositionOf(&matrix);
    position->x = position->x + half.x;
    position->y = position->y + half.y;
    position->z = position->z + half.z;
    InvertMatrix(this);
}

void DynamicBody::LimitedPush(const Vector4* push)
{
    constexpr f32 MostPushed = Rounded(0.2);
    f32 length = __builtin_sqrtf(push->x * push->x + push->y * push->y + push->z * push->z);
    limitedPushes.x = limitedPushes.x + push->x;
    limitedPushes.y = limitedPushes.y + push->y;
    limitedPushes.z = limitedPushes.z + push->z;
    f32 pushed = __builtin_sqrtf(limitedPushes.x * limitedPushes.x + limitedPushes.y * limitedPushes.y +
                                 limitedPushes.z * limitedPushes.z);
    Vector4 move;
    if (MostPushed < pushed)
    {
        f32 left = MostPushed - (pushed - length);
        if (!(0.0f < left))
        {
            return;
        }

        move = *push;
        f32 inverse = InverseLength(&move, LengthEpsilon);
        move.x = move.x * inverse * left;
        move.y = move.y * inverse * left;
        move.z = move.z * inverse * left;
    }
    else
    {
        move = *push;
    }

    Vector4* position = PositionOf(&matrix);
    position->x = position->x + move.x * 0.5f;
    position->y = position->y + move.y * 0.5f;
    position->z = position->z + move.z * 0.5f;
    InvertMatrix(this);
}

u32 DynamicBody::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    constexpr f32 Slowed = Rounded(0.8);
    if (link->flags.linkedRm2Loaded == 0)
    {
        *PositionOf(&matrix) = instance->box;
        InvertMatrix(this);
        Vector4 slowed = velocity;
        slowed.x = slowed.x * Slowed;
        slowed.y = slowed.y * Slowed;
        slowed.z = slowed.z * Slowed;
        velocity = slowed;
        Vector4 slowedMomentum;
        slowedMomentum.x = velocity.x * mass;
        slowedMomentum.y = velocity.y * mass;
        slowedMomentum.z = velocity.z * mass;
        slowedMomentum.w = 1.0f;
        bodyFlags.velocityStale = 0;
        momentum = slowedMomentum;
        UpdateState();
        Vector4 spin = angularVelocity;
        spin.x = spin.x * Slowed;
        spin.y = spin.y * Slowed;
        spin.z = spin.z * Slowed;
        SetAngularVelocity(&spin);
        return 0;
    }

    TransformVectorThroughLink(link, &momentum, 0);
    TransformVectorThroughLink(link, &angularMomentum, 0);
    Matrix4x4 turn;
    MatrixFromRotation(&turn, &rotation);
    SetOrigin(RowAt(&turn, 3));
    VuInvertRigidInPlace(&turn);
    TransformThroughLink(link, &turn, 1);
    VuInvertRigidInPlace(&turn);
    GetRotationVec(&rotation, &turn);
    Vector4 position = *PositionOf(&matrix);
    TransformVectorThroughLink(link, &position, 1);
    *PositionOf(&matrix) = position;
    InvertMatrix(this);
    bodyFlags.velocityStale = 1;
    bodyFlags.rotationStale = 1;
    return 1;
}

u32 SphereBody::CollideWithWorld()
{
    constexpr f32 BoxMargin = Rounded(0.01);
    constexpr f32 RisingDamping = Rounded(0.3);
    if (bits.noCollisions != 0)
    {
        return 0;
    }

    u32 inWater = 0;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&owner->nodes, NodeObject));
    CollisionCache* hits = Cache();
    UpdateState();
    // What SphereTouchesTriangle pushes (its w stays 1)
    Vector4 out = {radius, radius, radius, 1.0f};
    Vector4* position = PositionOf(&matrix);
    Box box;
    box.min = {position->x - out.x, position->y - out.y, position->z - out.z, 1.0f};
    f32 highest = 0.0f;
    box.max = {position->x + radius, position->y + radius, position->z + radius, 1.0f};
    GrowBox(BoxMargin, &box);
    RefreshCollisionCache(hits, &box);
    for (CollisionHit* hit = FirstCollisionHit(hits); hit != nullptr; hit = NextCollisionHit(hits))
    {
        // The triangle in the body's space (in the unit sphere's when it's an ellipsoid)
        CollisionHit triangle;
        triangle.vertices[0] = hit->vertices[0];
        triangle.vertices[1] = hit->vertices[1];
        triangle.vertices[2] = hit->vertices[2];
        triangle.surface = hit->surface;
        triangle.unused32 = hit->unused32;
        TransformCollisionHit(&triangle, &inverseMatrix);
        if (ellipsoid != 0)
        {
            Matrix4x4 unscale;
            InitIdentityMatrix(&unscale);
            unscale.m[0][0] = 1.0f / scaleX;
            unscale.m[1][1] = 1.0f / scaleY;
            unscale.m[2][2] = 1.0f / scaleZ;
            TransformCollisionHit(&triangle, &unscale);
        }

        Vector4 centre = {0.0f, 0.0f, 0.0f, 1.0f};
        if (SphereTouchesTriangle(radius, &triangle, &centre, &out) == 0)
        {
            continue;
        }

        if (GetTriangleSurface(&triangle)->flags.solidToObjects != 0)
        {
            // Where it touches, on the sphere toward the triangle
            Vector4 point = out;
            f32 inverse = InverseLength(&point, LengthEpsilon);
            f32 reach = -radius;
            point.x = point.x * inverse * reach;
            point.y = point.y * inverse * reach;
            point.z = point.z * inverse * reach;
            if (ellipsoid != 0)
            {
                out.x = out.x * scaleX;
                out.y = out.y * scaleY;
                out.z = out.z * scaleZ;
            }

            Vector4 push = out;
            VuRotateVector(&matrix, &out, &out);
            PushOut(&out, 0);
            Vector4 normal = push;
            inverse = InverseLength(&normal, LengthEpsilon);
            normal.x = normal.x * inverse;
            normal.y = normal.y * inverse;
            normal.z = normal.z * inverse;
            if (ellipsoid != 0)
            {
                normal.x = normal.x / scaleX;
                normal.y = normal.y / scaleY;
                normal.z = normal.z / scaleZ;
                point.x = point.x * scaleX;
                point.y = point.y * scaleY;
                point.z = point.z * scaleZ;
            }

            VuRotateVector(&matrix, &normal, &normal);
            inverse = InverseLength(&normal, LengthEpsilon);
            normal.x = normal.x * inverse;
            normal.y = normal.y * inverse;
            normal.z = normal.z * inverse;
            Damp(lengthDrag, 1.0f);
            bits.tellsLanding = 1;
            SurfaceContact(&point, &normal, GetTriangleSurface(&triangle), nullptr);
        }
        else
        {
            inWater = 1;
            node->TouchedWater(hit, position);
            if (0.0f < buoyancy)
            {
                // (Retail also works out where it touches, and leaves it)
                if (ellipsoid != 0)
                {
                    out.x = out.x * scaleX;
                    out.y = out.y * scaleY;
                    out.z = out.z * scaleZ;
                }

                VuRotateVector(&matrix, &out, &out);
                if (highest < out.y)
                {
                    highest = out.y;
                }
            }
        }

        bodyFlags.touchedWorld = 1;
        if (contactCallback != nullptr)
        {
            contactCallback(hit, contactArgument);
        }
    }

    if (inWater == 0)
    {
        node->waterSurface = ObjectNode::NoSurface;
    }
    else if (0.0f < highest)
    {
        Vector4 lift;
        lift.x = 0.0f;
        lift.y = highest * mass * buoyancy - velocity.y * RisingDamping;
        lift.z = 0.0f;
        lift.w = 1.0f;
        AddStepForce(&lift, position);
    }

    return 1;
}

namespace
{
// A sphere's pushes out of another body are shared all but a hair (0.50005 of the other's share), each pair told
constexpr f32 OtherShare = Rounded(0.50005);

// Both bodies told they touched each other (and their callbacks, each given the other's hull it touched)
void TellTouched(DynamicBody* body, DynamicBody* other, u32 hull, u32 otherHull)
{
    body->bodyFlags.touchedBody = 1;
    if (body->touchCallback != nullptr)
    {
        body->touchCallback(other->instance, hull, body->touchArgument);
    }

    other->bodyFlags.touchedBody = 1;
    if (other->touchCallback != nullptr)
    {
        other->touchCallback(body->instance, otherHull, other->touchArgument);
    }
}
}

u32 CollideSpheres(SphereBody* body, SphereBody* other)
{
    const Vector4* position = PositionOf(&body->matrix);
    const Vector4* otherPosition = PositionOf(&other->matrix);
    Vector4 apart;
    apart.x = position->x - otherPosition->x;
    apart.y = position->y - otherPosition->y;
    apart.z = position->z - otherPosition->z;
    f32 overlap = (body->radius + other->radius) - __builtin_sqrtf(apart.x * apart.x + apart.y * apart.y + apart.z * apart.z);
    if (!(0.0f < overlap))
    {
        return 0;
    }

    // Where they touch: the first's radius toward the second
    Vector4 contact;
    contact.x = position->x + (otherPosition->x - position->x) * body->radius;
    contact.y = position->y + (otherPosition->y - position->y) * body->radius;
    contact.z = position->z + (otherPosition->z - position->z) * body->radius;
    contact.w = 1.0f;
    Vector4 normal = apart;
    normal.w = 1.0f;
    f32 inverse = InverseLength(&normal, LengthEpsilon);
    normal.x = normal.x * inverse;
    normal.y = normal.y * inverse;
    normal.z = normal.z * inverse;
    Vector4 push;
    push.x = normal.x * overlap;
    push.y = normal.y * overlap;
    push.z = normal.z * overlap;
    push.w = 1.0f;
    Vector4 otherPush;
    otherPush.x = -(push.x * OtherShare);
    otherPush.y = -(push.y * OtherShare);
    otherPush.z = -(push.z * OtherShare);
    otherPush.w = 1.0f;
    body->LimitedPush(&push);
    other->LimitedPush(&otherPush);
    body->Damp(body->lengthDrag, 1.0f);
    other->Damp(other->lengthDrag, 1.0f);
    normal.x = -normal.x;
    normal.y = -normal.y;
    normal.z = -normal.z;
    body->BodyContact(&contact, &normal, other);
    TellTouched(body, other, 0, 0);
    return 1;
}

u32 CollideEllipsoids(SphereBody* body, SphereBody* other)
{
    Vector4 radii = {body->radius * body->scaleX, body->radius * body->scaleY, body->radius * body->scaleZ, 1.0f};
    Vector4 otherRadii = {other->radius * other->scaleX, other->radius * other->scaleY, other->radius * other->scaleZ, 1.0f};
    EllipsoidPair pair;
    Vector4 separation;
    Vector4 otherSeparation;
    if (EllipsoidsMeet(&pair, &separation, &otherSeparation, &body->matrix, &radii, &other->matrix, &otherRadii) == 0)
    {
        return 0;
    }

    Vector4* position = PositionOf(&body->matrix);
    Vector4* otherPosition = PositionOf(&other->matrix);
    Vector4 away;
    away.x = -((position->x + separation.x) - otherPosition->x);
    away.y = -((position->y + separation.y) - otherPosition->y);
    away.z = -((position->z + separation.z) - otherPosition->z);
    away.w = 1.0f;
    Vector4 otherPush;
    otherPush.x = -(away.x * OtherShare);
    otherPush.y = -(away.y * OtherShare);
    otherPush.z = -(away.z * OtherShare);
    otherPush.w = 1.0f;
    body->LimitedPush(&away);
    other->LimitedPush(&otherPush);
    // Where they touch: between their centres (once they're pushed)
    Vector4 contact;
    contact.x = (position->x + otherPosition->x) * 0.5f;
    contact.y = (position->y + otherPosition->y) * 0.5f;
    contact.z = (position->z + otherPosition->z) * 0.5f;
    contact.w = 1.0f;
    Vector4 normal = away;
    f32 inverse = InverseLength(&normal, LengthEpsilon);
    normal.x = normal.x * inverse;
    normal.y = normal.y * inverse;
    normal.z = normal.z * inverse;
    body->Damp(body->lengthDrag, 1.0f);
    other->Damp(other->lengthDrag, 1.0f);
    normal.x = -normal.x;
    normal.y = -normal.y;
    normal.z = -normal.z;
    body->BodyContact(&contact, &normal, other);
    TellTouched(body, other, 0, 0);
    return 1;
}

u32 CollideSphereWithHulls(SphereBody* sphere, DynamicBody* other)
{
    ObjectCollision* collision = &other->instance->collision;
    s32 count = GetHullCount(collision);
    u32 touched = 0;
    for (s32 index = 0; index < count; index++)
    {
        CollisionHull* hull;
        Matrix4x4 hullMatrix;
        GetInstanceHull(collision, index, &hull, &hullMatrix);
        // The hull goes with the other body (its own matrix isn't used)
        Vector4 push;
        Vector4 point;
        Vector4 normal;
        if (SphereTouchesHull(sphere, &sphere->matrix, &other->matrix, hull, &push, &point, &normal) == 0)
        {
            continue;
        }

        sphere->Damp(sphere->lengthDrag, 1.0f);
        other->Damp(other->lengthDrag, 1.0f);
        Vector4 otherPush;
        otherPush.x = -(push.x * OtherShare);
        otherPush.y = -(push.y * OtherShare);
        otherPush.z = -(push.z * OtherShare);
        otherPush.w = push.w;
        sphere->LimitedPush(&push);
        other->LimitedPush(&otherPush);
        VuTransformPoint(&sphere->matrix, &point, &point);
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
        sphere->BodyContact(&point, &normal, other);
        TellTouched(sphere, other, static_cast<u32>(index), 0);
        touched = 1;
    }

    return touched;
}

void SphereBody::CollideWithInstances()
{
    constexpr u16 MostInstances = 20;
    Box box;
    box.min = *PositionOf(&matrix);
    box.max = *PositionOf(&matrix);
    GrowBox(radius, &box);
    InstanceContext* results[MostInstances];
    InstanceQuery query;
    query.results = reinterpret_cast<void**>(results);
    query.most = MostInstances;
    query.count = 0;
    query.distance = Infinite;
    // Instances with their collision active, all the wanted flags, awake
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = ReferencedObjectFlags::CollisionActive;
    // Retail clears bit 0 and sets bit 1 of what its stack had (nothing reads the rest)
    query.bits.value = InstanceQueryBits::AllWanted;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, instance);
    QueryChunkInstances(instance->chunk, &box, collisionMask, &query);
    lastTouched = nullptr;
    for (s32 index = 0; index < query.count; index++)
    {
        InstanceContext* other = results[index];
        if (instance != nullptr && other->parent == instance)
        {
            continue;
        }

        if ((other->nodes.mask & (1u << NodeRigidBody)) != 0)
        {
            auto* body = other != nullptr ? static_cast<DynamicBody*>(GetGameNode(&other->nodes, NodeRigidBody)) : nullptr;
            CallVirtual<u32>(this, vtable, CollideSlot, body);
        }
        else
        {
            TouchHulls(other);
        }
    }
}

u32 SphereBody::TouchHulls(InstanceContext* other)
{
    ObjectCollision* collision = &other->collision;
    u8 count = static_cast<u8>(GetHullCount(collision));
    u32 touched = 0;
    for (u8 index = 0; index < count; index++)
    {
        CollisionHull* hull;
        Matrix4x4 hullMatrix;
        GetInstanceHull(collision, index, &hull, &hullMatrix);
        Vector4 push;
        Vector4 point;
        Vector4 normal;
        if (SphereTouchesHull(this, &matrix, &hullMatrix, hull, &push, &point, &normal) == 0)
        {
            continue;
        }

        if (other != ignoredInstance)
        {
            Damp(lengthDrag, 1.0f);
            if (InstanceContact(&point, &normal, GetHullSurface(collision, index), other) != 0)
            {
                PushOut(&push, 1);
            }
        }

        bodyFlags.touchedBody = 1;
        if (touchCallback != nullptr)
        {
            touchCallback(other, index, touchArgument);
        }

        touched = 1;
    }

    if (touched != 0)
    {
        lastTouched = other;
    }

    return touched;
}

u32 SphereTouchesHull(SphereBody* sphere, const Matrix4x4* matrix, const Matrix4x4* hullMatrix, const CollisionHull* hull,
                      Vector4* push, Vector4* point, Vector4* normal)
{
    Box box;
    box.min = *RowOf(matrix, 3);
    box.max = *RowOf(matrix, 3);
    GrowBox(sphere->radius, &box);
    Box hullBox;
    GetHullBoundingBox(hull, hullMatrix, &hullBox);
    if (BoxesOverlap(&box, &hullBox) == 0)
    {
        return 0;
    }

    Matrix4x4 intoHull = *hullMatrix;
    VuInvertRigidInPlace(&intoHull);
    if (sphere->ellipsoid != 0)
    {
        Vector4 radii = {sphere->radius * sphere->scaleX, sphere->radius * sphere->scaleY, sphere->radius * sphere->scaleZ, 1.0f};
        if (EllipsoidTouchesHull(hull, matrix, &radii, hullMatrix, push, normal) == 0)
        {
            return 0;
        }

        // Where it touches: its radius against the push, in its own space
        Matrix4x4 intoSphere = *matrix;
        VuInvertRigidInPlace(&intoSphere);
        VuRotateVector(&intoSphere, push, point);
        f32 inverse = InverseLength(point, LengthEpsilon);
        point->x = point->x * inverse;
        point->y = point->y * inverse;
        point->z = point->z * inverse;
        f32 reach = -sphere->radius;
        point->z = point->z * reach;
        point->x = point->x * reach;
        point->y = point->y * reach;
        return 1;
    }

    Vector4 centre;
    VuTransformPoint(&intoHull, RowOf(matrix, 3), &centre);
    // (Retail works out the hull's box here too, and leaves it)
    Box unused;
    HullBox(hull, &unused);
    Vector4 localPush;
    if (SphereInHull(sphere->radius, hull, &centre, &localPush) == 0)
    {
        return 0;
    }

    VuRotateVector(hullMatrix, &localPush, push);
    *normal = *push;
    f32 inverse = InverseLength(normal, LengthEpsilon);
    normal->x = normal->x * inverse;
    normal->y = normal->y * inverse;
    normal->z = normal->z * inverse;
    Matrix4x4 intoSphere = *matrix;
    VuInvertRigidInPlace(&intoSphere);
    VuRotateVector(&intoSphere, normal, point);
    f32 reach = -sphere->radius;
    point->x = point->x * reach;
    point->z = point->z * reach;
    point->y = point->y * reach;
    return 1;
}

void HullBody::CopyHulls(ObjectCollision* collision)
{
    if (hulls != nullptr)
    {
        CollisionHull* hull = hulls + ArrayCount(hulls);
        while (hull != hulls)
        {
            hull--;
            HullDestroy(hull, 0);
        }

        DeleteArray(hulls);
    }

    hulls = nullptr;
    s32 count = GetHullCount(collision);
    hullCount = count;
    CollisionHull* made = NewArray<CollisionHull>(count);
    CollisionHull* hull = made;
    for (s32 left = count - 1; left != -1; left--, hull++)
    {
        HullConstruct(hull);
    }

    hulls = made;
    for (s32 index = 0; index < hullCount; index++)
    {
        // A copy of the instance's hull under its matrix, the centre of mass taken off
        CollisionHull* copy = &hulls[index];
        Matrix4x4 identity;
        InitIdentityMatrix(&identity);
        CollisionHull* source;
        Matrix4x4 hullMatrix;
        GetHullAndMatrix(&instance->collision, &identity, index, &source, &hullMatrix);
        HullCopy(copy, source);
        Vector4 center = centerOfMass;
        VuRotateVector(&hullMatrix, &center, &center);
        Vector4* position = RowAt(&hullMatrix, 3);
        position->x = position->x - center.x;
        position->y = position->y - center.y;
        position->z = position->z - center.z;
        TransformHull(copy, &hullMatrix);
    }
}

u32 HullBody::CollideWithWorld()
{
    if (bits.noCollisions != 0)
    {
        return 0;
    }

    u32 inWater = 0;
    f32 deepest = 0.0f;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&owner->nodes, NodeObject));
    const Box* ownerBox = owner->CollisionBox();
    for (s32 index = 0; index < hullCount;)
    {
        CollisionCache* hits = Cache();
        CollisionHull* hull = &hulls[index];
        index++;
        Box box;
        GetHullBoundingBox(hull, &matrix, &box);
        UpdateState();
        RefreshCollisionCache(hits, &box);
        for (CollisionHit* hit = FirstCollisionHit(hits); hit != nullptr; hit = NextCollisionHit(hits))
        {
            Box triangleBox;
            TriangleBox(&triangleBox, &hit->vertices[0], &hit->vertices[1], &hit->vertices[2]);
            if (BoxesOverlap(&triangleBox, &box) == 0)
            {
                continue;
            }

            Vector4 plane;
            PlaneThroughTriangle(&plane, &hit->vertices[0], &hit->vertices[1], &hit->vertices[2]);
            if (HullSideOfPlane(hull, &matrix, &plane) != 0)
            {
                continue;
            }

            Vector4 push;
            Vector4 point;
            if (TriangleHullContact(hit, hull, &matrix, &push, &point) == 0)
            {
                continue;
            }

            if (GetTriangleSurface(hit)->flags.solidToObjects != 0)
            {
                Vector4 normal = push;
                f32 inverse = InverseLength(&normal, LengthEpsilon);
                normal.x = normal.x * inverse;
                normal.y = normal.y * inverse;
                normal.z = normal.z * inverse;
                PushOut(&push, 0);
                bits.tellsLanding = 1;
                VuTransformPoint(&inverseMatrix, &point, &point);
                SurfaceContact(&point, &normal, GetTriangleSurface(hit), nullptr);
                Damp(lengthDrag, 1.0f);
            }
            else
            {
                inWater = 1;
                node->TouchedWater(hit, &point);
                // Water without buoyancy isn't counted as touched (the sphere's is)
                if (!(0.0f < buoyancy))
                {
                    continue;
                }

                f32 depth = point.y - ownerBox->min.y;
                if (deepest < depth)
                {
                    waterLevel = point.y;
                    deepest = depth;
                }
            }

            bodyFlags.touchedWorld = 1;
            if (contactCallback != nullptr)
            {
                contactCallback(hit, contactArgument);
            }
        }
    }

    if (inWater == 0)
    {
        node->waterSurface = ObjectNode::NoSurface;
        // Still floating while its box is below the last level
        if (0.0f < buoyancy && ownerBox->max.y < waterLevel)
        {
            CallVirtual<void>(this, vtable, FloatSlot, waterLevel);
        }

        return 1;
    }

    if (0.0f < deepest)
    {
        CallVirtual<void>(this, vtable, FloatSlot, waterLevel);
    }

    return 1;
}

void HullBody::Float(f32 level)
{
    constexpr s32 Corners = 8;
    constexpr f32 Submerged = Rounded(0.1);
    constexpr f32 RisingDamping = Rounded(0.3);
    const Box* box = &owner->collision.ownBox;
    Vector4 corners[Corners];
    ObjectPlace* place = owner->place;
    RotateAndTranslate(place);
    BoxCorners(box, corners, &place->matrix);
    for (Vector4& corner : corners)
    {
        f32 depth = level - corner.y;
        if (!(Submerged < depth))
        {
            continue;
        }

        Vector4 cornerVelocity;
        VelocityAt(&corner, &cornerVelocity);
        Vector4 lift;
        lift.x = 0.0f;
        lift.y = depth * mass * buoyancy - cornerVelocity.y * RisingDamping;
        lift.z = 0.0f;
        lift.w = 1.0f;
        AddStepForce(&lift, &corner);
    }
}

void HullBody::CollideWithInstances()
{
    constexpr u16 MostInstances = 20;
    if (hulls == nullptr)
    {
        CopyHulls(&instance->collision);
    }

    if (hullCount <= 0)
    {
        return;
    }

    Box box;
    GetHullBoundingBox(&hulls[0], &matrix, &box);
    for (s32 index = 1; index < hullCount; index++)
    {
        Box hullBox;
        GetHullBoundingBox(&hulls[index], &matrix, &hullBox);
        MergeBox(&box, &hullBox);
    }

    InstanceContext* results[MostInstances];
    InstanceQuery query;
    query.most = MostInstances;
    query.results = reinterpret_cast<void**>(results);
    query.count = 0;
    query.distance = Infinite;
    // Any instance awake: all of no flags wanted (retail clears bit 0 and sets bit 1 of what its stack had)
    query.unwantedFlags = ReferencedObjectFlags::Asleep;
    query.wantedFlags = 0;
    query.bits.value = InstanceQueryBits::AllWanted;
    query.skipped[0] = nullptr;
    query.instance = nullptr;
    query.skipped[1] = nullptr;
    SkipInQuery(&query, instance);
    QueryChunkInstances(instance->chunk, &box, collisionMask, &query);
    lastTouched = nullptr;
    for (s32 index = 0; index < query.count; index++)
    {
        InstanceContext* other = results[index];
        // Its own children and what its collision is attached to
        if (instance != nullptr &&
            (other->parent == instance || other->collision.leftOut == instance))
        {
            continue;
        }

        if ((other->nodes.mask & (1u << NodeRigidBody)) != 0)
        {
            auto* body = other != nullptr ? static_cast<DynamicBody*>(GetGameNode(&other->nodes, NodeRigidBody)) : nullptr;
            CallVirtual<u32>(this, vtable, CollideSlot, body);
        }
        else if (other->flags.collisionActive)
        {
            TouchInstanceHulls(other);
        }
        else if (0.0f < buoyancy)
        {
            // A water volume: floated on at the top of its box
            CollisionSurface* surface = GetHullSurface(&other->collision, 0);
            if (surface != nullptr && surface->flags.solidToObjects == 0)
            {
                CallVirtual<void>(this, vtable, FloatSlot, other->CollisionBox()->max.y);
            }
        }
    }
}

u32 HullBody::TouchInstanceHulls(InstanceContext* other)
{
    ObjectCollision* collision = &other->collision;
    s32 otherCount = GetHullCount(collision);
    u32 touched = 0;
    for (s32 own = 0; own < hullCount; own++)
    {
        for (s32 index = 0; index < otherCount; index++)
        {
            PlaceInstanceAtCenter(this);
            CollisionHull* hull = &hulls[own];
            Matrix4x4 bodyMatrix = matrix;
            CollisionHull* otherHull;
            Matrix4x4 otherMatrix;
            GetInstanceHull(collision, index, &otherHull, &otherMatrix);
            Box box;
            GetHullBoundingBox(hull, &bodyMatrix, &box);
            Box otherBox;
            GetHullBoundingBox(otherHull, &otherMatrix, &otherBox);
            if (BoxesOverlap(&box, &otherBox) == 0)
            {
                continue;
            }

            Vector4 push;
            Vector4 point;
            if (HullsContact(hull, &bodyMatrix, otherHull, &otherMatrix, &push, &point) == 0)
            {
                continue;
            }

            Vector4 normal = push;
            f32 inverse = InverseLength(&normal, LengthEpsilon);
            normal.x = normal.x * inverse;
            normal.y = normal.y * inverse;
            normal.z = normal.z * inverse;
            push.x = -push.x;
            push.y = -push.y;
            push.z = -push.z;
            if (other != ignoredInstance)
            {
                Damp(lengthDrag, 1.0f);
                VuTransformPoint(&inverseMatrix, &point, &point);
                normal.x = -normal.x;
                normal.y = -normal.y;
                normal.z = -normal.z;
                if (InstanceContact(&point, &normal, GetHullSurface(collision, index & 0xFF), other) != 0)
                {
                    PushOut(&push, 1);
                }
            }

            bodyFlags.touchedBody = 1;
            if (touchCallback != nullptr)
            {
                touchCallback(other, index, touchArgument);
            }

            touched = 1;
        }

        if (touched != 0)
        {
            lastTouched = other;
        }
    }

    return 1;
}

u32 CollideHullBodies(HullBody* body, DynamicBody* other)
{
    auto* otherHulls = static_cast<HullBody*>(other);
    for (s32 own = 0; own < body->hullCount; own++)
    {
        for (s32 index = 0; index < otherHulls->hullCount; index++)
        {
            // Both instances put at their bodies' matrices as they are (the first one's flag the other way round)
            Matrix4x4 bodyMatrix = body->matrix;
            PlaceInstanceAt(body->instance, &bodyMatrix, body->bits.placesPositionOnly == 0);
            Matrix4x4 otherMatrix = other->matrix;
            PlaceInstanceAt(other->instance, &otherMatrix, other->bits.placesPositionOnly != 0);
            CollisionHull* hull = &body->hulls[own];
            CollisionHull* otherHull = &otherHulls->hulls[index];
            Box box;
            GetHullBoundingBox(hull, &bodyMatrix, &box);
            Box otherBox;
            GetHullBoundingBox(otherHull, &otherMatrix, &otherBox);
            if (BoxesOverlap(&box, &otherBox) == 0)
            {
                continue;
            }

            Vector4 push;
            Vector4 point;
            if (HullsContact(hull, &bodyMatrix, otherHull, &otherMatrix, &push, &point) == 0)
            {
                continue;
            }

            Vector4 normal = push;
            f32 inverse = InverseLength(&normal, LengthEpsilon);
            normal.x = normal.x * inverse;
            normal.y = normal.y * inverse;
            normal.z = normal.z * inverse;
            push.x = -push.x;
            push.y = -push.y;
            push.z = -push.z;
            body->Damp(body->lengthDrag, 1.0f);
            other->Damp(other->lengthDrag, 1.0f);
            Vector4 otherPush;
            otherPush.x = -(push.x * OtherShare);
            otherPush.y = -(push.y * OtherShare);
            otherPush.z = -(push.z * OtherShare);
            otherPush.w = push.w;
            body->LimitedPush(&push);
            other->LimitedPush(&otherPush);
            body->BodyContact(&point, &normal, other);
            TellTouched(body, other, index, own);
        }
    }

    return 1;
}

void StepPhysicsWorld(PhysicsWorld* world)
{
    constexpr s32 Slots = PhysicsWorld::Slots;
    DynamicBody* bodies[Slots];
    f32 seconds[Slots];
    s32 count = 0;
    for (s32 slot = 0; slot < Slots; slot++)
    {
        DynamicBody* body = world->bodies[slot];
        if (body == nullptr)
        {
            continue;
        }

        InstanceContext* instance = body->instance;
        if (instance->flags.asleep)
        {
            continue;
        }

        TimeClock* clock = GetContextClock(instance);
        if (clock->flags.running == 0)
        {
            continue;
        }

        body = world->bodies[slot];
        if (body->bodyFlags.leftOut != 0)
        {
            continue;
        }

        bodies[count] = body;
        seconds[count] = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
        count++;
    }

    for (s32 index = 0; index < count; index++)
    {
        DynamicBody* body = bodies[index];
        s32 substeps = body->substeps;
        body->bits.landed = 0;
        body->bits.landedHard = 0;
        body->bits.scraped = 0;
        body->bodyFlags.touchedWorld = 0;
        body->bodyFlags.touchedBody = 0;
        body->impulseTotal = 0.0f;
        f32 step = seconds[index] / static_cast<f32>(substeps);
        SetOrigin(&body->limitedPushes);
        SetOrigin(&body->pushes);
        body->pushedLength = 0.0f;
        for (s32 left = substeps; left > 0; left--)
        {
            body->Step(step);
            CallVirtual<void>(body, body->vtable, DynamicBody::CollideWithInstancesSlot);
            CallVirtual<void>(body, body->vtable, DynamicBody::CollideWithWorldSlot);
            body->lastVelocity = body->velocity;
        }
    }

    for (s32 index = 0; index < count; index++)
    {
        DynamicBody* body = bodies[index];
        body->StepDamping(seconds[index]);
        body->UpdateState();
        SetOrigin(&body->force);
        SetOrigin(&body->torque);
        body->PlaceInstance();
    }
}

void InitRigidBodyStatics(u32 initialise, u32 priority)
{
    if (priority == DefaultInitPriority && initialise != 0)
    {
        g_RigidBodyStatic = 0;
    }
}

void ConstructRigidBodyModule()
{
    InitRigidBodyStatics(1, DefaultInitPriority);
}

f32 InverseLengthKeepingSquare(const Vector4* vector, f32* lengthSquared)
{
    f32 squared = vector->x * vector->x + vector->y * vector->y + vector->z * vector->z;
    *lengthSquared = squared;
    if (LengthEpsilon < squared)
    {
        return 1.0f / Kept(__builtin_sqrtf(squared));
    }

    return 0.0f;
}

void RigidBody::StartRide()
{
    bodyFlags.leftOut = 1;
    f32 kept = momentum.w;
    momentum = g_DefaultBox.min;
    momentum.w = kept;
    kept = angularMomentum.w;
    angularMomentum = g_DefaultBox.min;
    angularMomentum.w = kept;
    lastTouched = nullptr;
}

void RigidBody::ReleaseRide()
{
    if (bodyFlags.leftOut == 0)
    {
        return;
    }

    bodyFlags.leftOut = 0;
    InstanceContext* instance = owner;
    if (instance->flags.physicsBody)
    {
        auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
        if (CallVirtual<u32>(node, node->vtable, ObjectNode::TakesPacketsSlot) != 0)
        {
            RestartTrajectory(node->trajectory);
        }
    }

    ClearForces();
}

void RigidBody::Stop()
{
    Vector4 none;
    none.x = 0.0f;
    none.y = 0.0f;
    none.z = 0.0f;
    none.w = 1.0f;
    SetAngularVelocity(&none);
    SetVelocity(&none);
}

void RigidBody::ClearForces()
{
    UpdateState();
    SetOrigin(&force);
    SetOrigin(&torque);
}

void RigidBody::SetMatrix(const Matrix4x4* placed)
{
    *PositionOf(&matrix) = *RowOf(placed, 3);
    Matrix4x4 turn;
    TransposeMatrix(placed, 3, &turn);
    GetRotationVec(&rotation, &turn);
    bodyFlags.velocityStale = 1;
    bodyFlags.rotationStale = 1;
}

void RigidBody::SetPosition(const Vector4* position)
{
    *PositionOf(&matrix) = *position;
    InvertMatrix(this);
}

void RigidBody::MoveBy(const Vector4* offset)
{
    Vector4* position = PositionOf(&matrix);
    position->x = position->x + offset->x;
    position->y = position->y + offset->y;
    position->z = position->z + offset->z;
    InvertMatrix(this);
}

void RigidBody::SetTurn(const Matrix4x4* turned)
{
    Matrix4x4 turn;
    TransposeMatrix(turned, 3, &turn);
    GetRotationVec(&rotation, &turn);
    bodyFlags.velocityStale = 1;
    bodyFlags.rotationStale = 1;
}

void RigidBody::SetVelocity(const Vector4* moving)
{
    velocity = *moving;
    Vector4 pushed;
    pushed.x = velocity.x * mass;
    pushed.y = velocity.y * mass;
    pushed.z = velocity.z * mass;
    pushed.w = 1.0f;
    bodyFlags.velocityStale = 0;
    momentum = pushed;
    UpdateState();
}

namespace
{
// What's left of a share for every 60th of a second of a time, none past all of it
f32 LeftAfter(f32 share, f32 seconds)
{
    f32 left = 1.0f - share * seconds * FramesPerSecond;
    if (left < 0.0f)
    {
        left = 0.0f;
    }

    return left;
}

void SlowAlongAxis(RigidBody* body, f32 share, f32 seconds, u32 axis)
{
    f32 left = LeftAfter(share, seconds);
    ObjectPlace* place = body->owner->place;
    RotateAndTranslate(place);
    Vector4 along = *RowOf(&place->matrix, axis);
    ScaleAlongAxis(left, &body->momentum, &along, 1);
    body->bodyFlags.velocityStale = 1;
}
}

void RigidBody::Slow(f32 share, f32 seconds)
{
    f32 left = LeftAfter(share, seconds);
    momentum.x = momentum.x * left;
    momentum.y = momentum.y * left;
    momentum.z = momentum.z * left;
    angularMomentum.x = angularMomentum.x * left;
    angularMomentum.y = angularMomentum.y * left;
    angularMomentum.z = angularMomentum.z * left;
    bodyFlags.velocityStale = 1;
}

void RigidBody::SlowAlongX(f32 share, f32 seconds)
{
    SlowAlongAxis(this, share, seconds, 0);
}

void RigidBody::SlowAlongY(f32 share, f32 seconds)
{
    SlowAlongAxis(this, share, seconds, 1);
}

void RigidBody::SlowAlongZ(f32 share, f32 seconds)
{
    SlowAlongAxis(this, share, seconds, 2);
}

void RigidBody::AddLocalForce(const Vector4* push, const Vector4* localPoint)
{
    Vector4 localPush;
    VuRotateVector(&inverseMatrix, push, &localPush);
    UpdateState();
    Vector4 worldPush;
    VuRotateVector(&matrix, &localPush, &worldPush);
    force.x = force.x + worldPush.x;
    force.y = force.y + worldPush.y;
    force.z = force.z + worldPush.z;
    Vector4 turn;
    turn.x = localPush.y * localPoint->z - localPush.z * localPoint->y;
    turn.y = localPush.z * localPoint->x - localPush.x * localPoint->z;
    turn.z = localPush.x * localPoint->y - localPush.y * localPoint->x;
    turn.w = 1.0f;
    Vector4 worldTurn;
    VuRotateVector(&matrix, &turn, &worldTurn);
    torque.x = torque.x + worldTurn.x;
    torque.y = torque.y + worldTurn.y;
    torque.z = torque.z + worldTurn.z;
}

CollisionCache* DynamicBody::Cache()
{
    constexpr u32 CacheMask = 0x50;
    if (cache == nullptr)
    {
        cache = ConstructCollisionCache(static_cast<CollisionCache*>(MemoryAllocate(sizeof(CollisionCache))), instance, CacheMask);
    }

    return cache;
}

void DynamicBody::SetRestitution(f32 value)
{
    restitution = value;
}

void DynamicBody::SetSoftness(f32 value)
{
    softness = value * value;
}

void DynamicBody::SetFriction(f32 value)
{
    friction = value;
}

void DynamicBody::SetSpinAndRollFriction(f32 spin, f32 roll)
{
    rollFriction = roll;
    spinFriction = spin;
}

void DynamicBody::SetInstancePosition(const Vector4* position)
{
    Vector4 moved = *position;
    moved.x = moved.x + centerOfMass.x;
    moved.y = moved.y + centerOfMass.y;
    moved.z = moved.z + centerOfMass.z;
    *PositionOf(&matrix) = moved;
    InvertMatrix(this);
}

void DynamicBody::SetCacheMask(u32 mask)
{
    Cache()->surfaceMask = mask;
}

void SphereBody::SetEllipsoid(f32 x, f32 y, f32 z)
{
    ellipsoid = 1;
    radius = x;
    if (x < y)
    {
        radius = y;
    }

    if (radius < z)
    {
        radius = z;
    }

    scaleZ = z / radius;
    scaleX = x / radius;
    scaleY = y / radius;
}

namespace
{
// The EABI entry of NegativeEllipsoidValue, which the minimum search calls
extern "C" void NegativeEllipsoidValueEntry() RETAIL(FUN_002924e0);

// A vector of rows of a matrix, each scaled by a value and how far a vector goes along it
void AddAlongRows(Vector4* sum, const Vector4* vector, const Matrix4x4* matrix, const f32* scales)
{
    for (u32 row = 0; row < 3; row++)
    {
        const Vector4* axis = RowOf(matrix, row);
        f32 along = scales[row] * (vector->x * axis->x + vector->y * axis->y + vector->z * axis->z);
        Vector4 part;
        part.x = axis->x * along;
        part.y = axis->y * along;
        part.z = axis->z * along;
        part.w = 1.0f;
        sum->x = sum->x + part.x;
        sum->y = sum->y + part.y;
        sum->z = sum->z + part.z;
    }
}
}

u32 EllipsoidsMeet(EllipsoidPair* pair, Vector4* separation, Vector4* other, const Matrix4x4* matrix, const Vector4* radii,
                   const Matrix4x4* otherMatrix, const Vector4* otherRadii)
{
    SetOrigin(separation);
    SetOrigin(other);
    const Vector4* position = RowOf(matrix, 3);
    const Vector4* otherPosition = RowOf(otherMatrix, 3);
    Vector4 between;
    between.x = otherPosition->x - position->x;
    between.y = otherPosition->y - position->y;
    between.z = otherPosition->z - position->z;
    between.w = 1.0f;
    pair->direction = between;
    if (radii != nullptr)
    {
        const f32* values = &radii->x;
        for (u32 axis = 0; axis < 3; axis++)
        {
            pair->radii[axis] = values[axis];
        }
    }

    if (otherRadii != nullptr)
    {
        const f32* values = &otherRadii->x;
        for (u32 axis = 0; axis < 3; axis++)
        {
            pair->otherRadii[axis] = values[axis];
        }
    }

    s32 largest;
    s32 middle;
    s32 smallest;
    SortThree(pair->radii, &largest, &middle, &smallest);
    s32 otherLargest;
    s32 otherMiddle;
    s32 otherSmallest;
    SortThree(pair->otherRadii, &otherLargest, &otherMiddle, &otherSmallest);
    const Vector4& apart = pair->direction;
    f32 reach = pair->radii[largest] + pair->otherRadii[otherLargest];
    if (!(apart.x * apart.x + apart.y * apart.y + apart.z * apart.z <= reach * reach))
    {
        return 0;
    }

    pair->inverseVolume = 1.0f;
    for (u32 axis = 0; axis < 3; axis++)
    {
        pair->squares[axis] = pair->radii[axis] * pair->radii[axis];
        f32 inverse = 1.0f / pair->radii[axis];
        f32 inverseSquare = inverse * inverse;
        pair->inverseRadii[axis] = inverse;
        pair->inverseSquares[axis] = inverseSquare;
        pair->inverseVolume = pair->inverseVolume * inverseSquare;
    }

    pair->otherVolume = 1.0f;
    for (u32 axis = 0; axis < 3; axis++)
    {
        pair->otherSquares[axis] = pair->otherRadii[axis] * pair->otherRadii[axis];
        f32 inverse = 1.0f / pair->otherRadii[axis];
        pair->otherInverseRadii[axis] = inverse;
        pair->otherInverseSquares[axis] = inverse * inverse;
        pair->otherVolume = pair->otherVolume * pair->otherSquares[axis];
    }

    EllipsoidProducts(pair);
    EllipsoidMatrices(pair, matrix, otherMatrix);
    // The direction into the first's unit sphere
    Vector4 direction = pair->direction;
    SetOrigin(&pair->direction);
    AddAlongRows(&pair->direction, &direction, matrix, pair->inverseRadii);
    f32 inverse = InverseLengthKeepingSquare(&pair->direction, &pair->distanceSquared);
    pair->direction.x = pair->direction.x * inverse;
    pair->direction.y = pair->direction.y * inverse;
    pair->direction.z = pair->direction.z * inverse;
    u32 noDirection = 0.0f < inverse ? 0 : 1;
    if (noDirection != 0)
    {
        // On top of each other: up, at a tiny distance
        pair->distanceSquared = __builtin_fminf(pair->otherSquares[otherSmallest], 1.0f) * LengthEpsilon;
        pair->direction = g_YAxis;
    }

    return (EllipsoidsApart(pair, separation, other, matrix, otherMatrix, noDirection) ^ 1) & 0xFF;
}

u32 EllipsoidsApart(EllipsoidPair* pair, Vector4* separation, Vector4* other, const Matrix4x4* matrix,
                    const Matrix4x4* otherMatrix, u32 noDirection)
{
    constexpr s32 Steps = 12;
    constexpr f32 FirstStep = 0x1.3b13b2p-4f;
    constexpr f32 Tolerance = Epsilon;
    f32 largest = -1.0f;
    f32 step = FirstStep;
    f32 along = 0.0f + step;
    // Retail leaves where the largest value was unset until a step finds one above -1
    f32 found = along;
    for (s32 index = 0; index < Steps; index++)
    {
        f32 value = EllipsoidValue(pair, along);
        if (largest < value)
        {
            largest = value;
            found = along;
            if (1.0f < largest * pair->distanceSquared)
            {
                return 1;
            }
        }
        else
        {
            along = along - step;
            step = step * -0.5f;
        }

        if (index + 1 < Steps)
        {
            along = along + step;
        }
    }

    MinimumSearch search{};
    search.steps = 16;
    search.tolerance = Tolerance;
    search.closeness = Tolerance;
    search.low = 0.0f;
    search.high = 1.0f;
    f32 value = -largest;
    FindMinimum(&search, pair, reinterpret_cast<const void*>(&NegativeEllipsoidValueEntry), &found, &value, 0);
    largest = -value;
    f32 scaled = largest * pair->distanceSquared;
    if (!(scaled <= 1.0f))
    {
        return 1;
    }

    f32 shrunk = scaled - scaled * Tolerance;
    if (noDirection != 0)
    {
        f32 inverse = Platform::Math::DivideBySquareRoot(1.0f, largest);
        Vector4 toward;
        toward.x = pair->direction.x * inverse;
        toward.y = pair->direction.y * inverse;
        toward.z = pair->direction.z * inverse;
        toward.w = 1.0f;
        *separation = toward;
        SetOrigin(separation);
        AddAlongRows(separation, &toward, matrix, pair->radii);
    }
    else
    {
        f32 inverse = Platform::Math::DivideBySquareRoot(1.0f, shrunk);
        const Vector4* position = RowOf(matrix, 3);
        const Vector4* otherPosition = RowOf(otherMatrix, 3);
        Vector4 toward;
        toward.x = (otherPosition->x - position->x) * inverse;
        toward.y = (otherPosition->y - position->y) * inverse;
        toward.z = (otherPosition->z - position->z) * inverse;
        toward.w = 1.0f;
        *separation = toward;
    }

    Vector4 point = pair->point;
    *other = point;
    SetOrigin(other);
    AddAlongRows(other, &point, matrix, pair->radii);
    f32 inverse = Platform::Math::DivideBySquareRoot(1.0f, largest);
    other->z = other->z * inverse;
    other->x = other->x * inverse;
    other->y = other->y * inverse;
    return 0;
}

f32 EllipsoidValue(EllipsoidPair* pair, f32 along)
{
    f32 rest = 1.0f - along;
    f32 both = rest * along;
    f32 alongSquared = along * along;
    f32 restSquared = rest * rest;
    pair->point = pair->direction;
    f32 value = alongSquared * pair->otherVolume * pair->inverseVolume + both * pair->otherSum + restSquared * pair->sum;
    pair->point.x = pair->point.x * restSquared;
    pair->point.y = pair->point.y * restSquared;
    pair->point.z = pair->point.z * restSquared;
    value = value * along + restSquared * rest;
    Vector4 moved;
    VuTransformPoint(&pair->second, &pair->direction, &moved);
    pair->point.x = pair->point.x + moved.x * alongSquared;
    pair->point.y = pair->point.y + moved.y * alongSquared;
    pair->point.z = pair->point.z + moved.z * alongSquared;
    VuTransformPoint(&pair->first, &pair->direction, &moved);
    f32 scale = rest / value;
    pair->point.x = pair->point.x - moved.x * both;
    pair->point.y = pair->point.y - moved.y * both;
    pair->point.z = pair->point.z - moved.z * both;
    pair->point.x = pair->point.x * scale;
    pair->point.y = pair->point.y * scale;
    pair->point.z = pair->point.z * scale;
    const Vector4& direction = pair->direction;
    return (pair->point.x * direction.x + pair->point.y * direction.y + pair->point.z * direction.z) * along;
}

f32 NegativeEllipsoidValue(EllipsoidPair* pair, f32 along)
{
    return -EllipsoidValue(pair, along);
}

void EllipsoidProducts(EllipsoidPair* pair)
{
    for (s32 axis = 0; axis < 3; axis++)
    {
        s32 next = NextAxis[axis];
        s32 after = NextAxis[next];
        pair->inverseProducts[axis] = pair->inverseRadii[next] * pair->inverseRadii[after];
        pair->otherProducts[axis] = pair->otherSquares[next] * pair->otherSquares[after];
    }
}

void EllipsoidMatrices(EllipsoidPair* pair, const Matrix4x4* matrix, const Matrix4x4* otherMatrix)
{
    pair->otherSum = 0.0f;
    pair->sum = 0.0f;
    for (u32 row = 0; row < 4; row++)
    {
        for (u32 column = 0; column < 4; column++)
        {
            pair->first.m[row][column] = 0.0f;
            pair->second.m[row][column] = 0.0f;
        }
    }

    pair->first.m[3][3] = 1.0f;
    pair->second.m[3][3] = 1.0f;
    for (u32 row = 0; row < 3; row++)
    {
        const Vector4* axis = RowOf(matrix, row);
        f32* cosines = &pair->cosines[row].x;
        for (u32 column = 0; column < 3; column++)
        {
            const Vector4* otherAxis = RowOf(otherMatrix, column);
            cosines[column] = axis->x * otherAxis->x + axis->y * otherAxis->y + axis->z * otherAxis->z;
        }
    }

    for (s32 row = 0; row < 3; row++)
    {
        const Vector4* axis = RowOf(matrix, row);
        const f32* cosines = &pair->cosines[row].x;
        for (s32 column = 0; column < 3; column++)
        {
            Matrix4x4 spread;
            OuterProduct(&spread, &axis->x, axis);
            f32 cosine = cosines[column];
            f32 weight = pair->otherSquares[column] * pair->inverseSquares[row] * (cosine * cosine);
            pair->sum = pair->sum + weight;
            Matrix4x4 part = spread;
            ScaleRotation(weight, &part);
            AddRotation(&pair->first, &part);
            f32 scaled = pair->inverseProducts[row] * cosine;
            f32 otherWeight = pair->otherProducts[column] * (scaled * scaled);
            pair->otherSum = pair->otherSum + otherWeight;
            part = spread;
            ScaleRotation(otherWeight, &part);
            AddRotation(&pair->second, &part);
            for (s32 earlier = 0; earlier < row; earlier++)
            {
                OuterProduct(&spread, &axis->x, RowOf(matrix, earlier));
                Matrix4x4 transposed = spread;
                TransposeInPlace(&transposed, 3);
                AddRotation(&spread, &transposed);
                f32 earlierCosine = (&pair->cosines[earlier].x)[column];
                f32 crossWeight = pair->otherRadii[column] * pair->inverseRadii[row] * pair->inverseRadii[earlier] * cosine *
                                  earlierCosine;
                transposed = spread;
                ScaleRotation(crossWeight, &transposed);
                AddRotation(&pair->first, &transposed);
                f32 otherCrossWeight = pair->otherProducts[column] * pair->inverseProducts[row] * pair->inverseProducts[earlier] *
                                       cosine * earlierCosine;
                transposed = spread;
                ScaleRotation(otherCrossWeight, &transposed);
                AddRotation(&pair->second, &transposed);
            }
        }
    }

    AddToDiagonal(-pair->sum, &pair->first, 0);
}
