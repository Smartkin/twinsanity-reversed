#include "game/camerarig.h"

#include "game/cameras.h"
#include "game/chunkdata.h"
#include "game/clock.h"
#include "game/followcamera.h"
#include "game/layout.h"
#include "game/memory.h"
#include "game/pads.h"
#include "game/place.h"
#include "game/reference.h"
#include "platform/graphics.h"

EABI_EXPORT(FUN_0026fe40, PushCameraShake);
EABI_EXPORT(FUN_00270040, PushCameraShakeAxes);
EABI_EXPORT(func_0027E030, &CameraPointFollower::SetRate);
EABI_EXPORT(FUN_0027cd78, &CutscenePositioner::Take);
EABI_EXPORT(FUN_0027ca90, &ScriptedCameraPositioner::Take);

namespace
{
// The place of the shake's centre with its position up to date, none without a centre
ObjectPlace* CentrePlace(CameraShake* shake)
{
    ReferencedObject* centre = shake->centre != nullptr ? shake->centre->object : nullptr;
    if (centre == nullptr)
    {
        return nullptr;
    }

    ObjectPlace* place = centre->place;
    place->SyncPosition();
    return place;
}

// The way from a point to the shake's centre in the centre's space
Vector4 WayToCentre(CameraShake* shake, const ObjectPlace* place, const Vector4* from)
{
    Vector4 way = place->position;
    way.x = way.x - from->x;
    way.y = way.y - from->y;
    way.z = way.z - from->z;
    VuRotateVector(&shake->toCentre, &way, &way);
    return way;
}
}

CameraShake* ConstructCameraShake(CameraShake* shake)
{
    shake->centre = nullptr;
    shake->offset = g_DefaultBox.min;
    shake->offset.w = 1.0f;
    shake->previous = g_DefaultBox.min;
    shake->previous.w = 1.0f;
    shake->push = g_DefaultBox.min;
    shake->push.w = 1.0f;
    InitIdentityMatrix(&shake->toCentre);
    shake->limit = 1.0f;
    shake->damping = Rounded(0.1);
    shake->unused84 = 1.0f;
    AssignReference(&shake->centre, nullptr);
    return shake;
}

void PushCameraShake(CameraShake* shake, const Vector4* from, f32 strength, f32 falloff)
{
    constexpr f32 NoFalloff = Rounded(1e-4);
    constexpr f32 Jitter = Rounded(0.2);
    ObjectPlace* place = CentrePlace(shake);
    if (place == nullptr)
    {
        return;
    }

    Vector4 way = WayToCentre(shake, place, from);
    if (NoFalloff < falloff)
    {
        strength = strength / ((way.x * way.x + way.y * way.y + way.z * way.z) * falloff + 1.0f);
    }

    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    JitterVector(Jitter, 1.0f, 1.0f, 1.0f, &way);
    way.x = way.x * strength;
    way.y = way.y * strength;
    way.z = way.z * strength;
    shake->push.x = shake->push.x + way.x;
    shake->push.y = shake->push.y + way.y;
    shake->push.z = shake->push.z + way.z;
    VibrateForShake(strength);
}

void PushCameraShakeAxes(CameraShake* shake, const Vector4* from, f32 xStrength, f32 yStrength, f32 zStrength, f32 falloff)
{
    constexpr f32 NoFalloff = Rounded(1e-4);
    ObjectPlace* place = CentrePlace(shake);
    if (place == nullptr)
    {
        return;
    }

    Vector4 way = WayToCentre(shake, place, from);
    f32 strongest;
    if (NoFalloff < falloff)
    {
        f32 scale = 1.0f / ((way.x * way.x + way.y * way.y + way.z * way.z) * falloff + 1.0f);
        f32 z = zStrength * scale;
        f32 x = xStrength * scale;
        f32 y = yStrength * scale;
        way.x = 0.0f < way.x ? x : -x;
        way.y = 0.0f < way.y ? y : -y;
        way.z = 0.0f < way.z ? z : -z;
        strongest = x;
        if (strongest < y)
        {
            strongest = y;
        }

        if (strongest < z)
        {
            strongest = z;
        }
    }
    else
    {
        f32 inverse = InverseLength(&way, LengthEpsilon);
        way.x = way.x * inverse * xStrength;
        way.y = way.y * inverse * yStrength;
        way.z = way.z * inverse * zStrength;
        strongest = xStrength;
        if (strongest < yStrength)
        {
            strongest = yStrength;
        }

        if (strongest < zStrength)
        {
            strongest = zStrength;
        }
    }

    shake->push.x = shake->push.x + way.x;
    shake->push.y = shake->push.y + way.y;
    shake->push.z = shake->push.z + way.z;
    VibrateForShake(strongest);
}

void StepCameraShake(CameraShake* shake)
{
    ObjectPlace* place = CentrePlace(shake);
    if (place == nullptr)
    {
        return;
    }

    RotateAndTranslate(place);
    shake->toCentre = place->matrix;
    VuInvertRigidInPlace(&shake->toCentre);
    shake->offset.x = shake->offset.x + shake->push.x;
    shake->offset.y = shake->offset.y + shake->push.y;
    shake->offset.z = shake->offset.z + shake->push.z;
    shake->push = g_DefaultBox.min;
    shake->push.w = 1.0f;

    // On as far as it moved the step before
    Vector4 current = shake->offset;
    f32 x = current.x + (current.x - shake->previous.x);
    f32 y = current.y + (current.y - shake->previous.y);
    f32 z = current.z + (current.z - shake->previous.z);
    shake->offset.x = x;
    shake->offset.y = y;
    shake->previous = current;
    shake->offset.z = z;

    // Damped and kept under the limit
    f32 damping = shake->damping;
    x = x - x * damping;
    y = y - y * damping;
    z = z - z * damping;
    shake->offset.x = x;
    shake->offset.y = y;
    shake->offset.z = z;
    f32 limit = shake->limit;
    f32 length = __builtin_sqrtf(x * x + y * y + z * z);
    if (limit < length)
    {
        f32 scale = limit / length;
        shake->offset.z = z * scale;
        shake->offset.x = x * scale;
        shake->offset.y = y * scale;
    }
}

void CameraShakeOffset(const CameraShake* shake, Vector4* offset)
{
    *offset = shake->offset;
}

void VibrateForShake(f32 strength)
{
    constexpr u32 FirstPad = 1;
    constexpr f32 MotorPerStrength = 1024.0f;
    constexpr s32 Weakest = 20;
    constexpr s32 Strongest = 255;
    constexpr f32 Seconds = Rounded(0.4);
    s32 motor = static_cast<s32>(strength * MotorPerStrength);
    if (motor > Strongest)
    {
        motor = Strongest;
    }

    if (motor < Weakest)
    {
        motor = Weakest;
    }

    VibrationRequest request;
    request.bits.value = 0;
    request.bits.pad = FirstPad;
    request.bits.pending = 1;
    request.bits.bigMotor = motor;
    request.seconds = Seconds;
    RequestVibration(&request);
}

CameraPointFollower* CameraPointFollower::Construct(CameraPointFollower* follower)
{
    constexpr f32 OwnRate = 5.0f;
    follower->vtable = g_CameraPointFollowerVTable;
    follower->ownRate = OwnRate;
    follower->rate = OwnRate;
    CameraFollowerBits linear = {};
    linear.way = WayLinear;
    linear.ownWay = WayLinear;
    follower->bits = linear;
    follower->point = g_DefaultBox.min;
    follower->point.w = 1.0f;
    return follower;
}

void CameraPointFollower::Destroy(u32 destroyFlags)
{
    BaseDestroy(destroyFlags);
}

void CameraPointFollower::BaseDestroy(u32 destroyFlags)
{
    vtable = g_CameraFollowerBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CameraPointFollower::Take(const Vector4*, const Vector4* to)
{
    point = *to;
}

void CameraPointFollower::Reset()
{
    bits.way = bits.ownWay;
    if (bits.keepsRate == 0)
    {
        rate = ownRate;
    }
}

void CameraPointFollower::Step(TimeClock* clock, const Vector4*, Vector4* to)
{
    constexpr f32 MostKeptRate = 100.0f;
    if (clock->flags.running == 0)
    {
        return;
    }

    if (bits.keepsRate != 0 && rate < MostKeptRate)
    {
        rate = rate + 1.0f;
    }

    f32 share = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit * rate;
    if (1.0f <= share)
    {
        point = *to;
    }
    else
    {
        f32 step;
        switch (bits.way)
        {
        case WayLinear:
            step = share;
            break;
        case WayAtOnce:
            step = 1.0f;
            break;
        case WaySquared:
            step = share * share;
            break;
        case WaySquareRoot:
            step = __builtin_sqrtf(share);
            break;
        default:
            step = 0.0f;
            break;
        }

        point.x = point.x + (to->x - point.x) * step;
        point.y = point.y + (to->y - point.y) * step;
        point.w = 1.0f;
        point.z = point.z + (to->z - point.z) * step;
    }

    *to = point;
}

void CameraPointFollower::FollowSubtype(const CameraSubtype* subtype)
{
    if (subtype == nullptr || subtype->flags.follow == CameraSubtype::FollowOwnWay)
    {
        Reset();
        return;
    }

    if (subtype->flags.follow != CameraSubtype::FollowAtRate)
    {
        bits.way = WayAtOnce;
        return;
    }

    bits.way = WayLinear;
    if (bits.keepsRate == 0)
    {
        rate = subtype->rate;
    }
}

void CameraPointFollower::SetRate(f32 newRate)
{
    if (!(0.0f <= newRate))
    {
        bits.way = WayAtOnce;
        return;
    }

    bits.way = WayLinear;
    if (bits.keepsRate == 0)
    {
        rate = newRate;
    }
}

void CameraPointFollower::SetKeepsRate(u32 keeps)
{
    bits.keepsRate = keeps;
}

u32 CameraPointFollower::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    TransformVectorThroughLink(link, &point, 1);
    return 1;
}

void CameraTarget::Destroy(u32 destroyFlags)
{
    vtable = g_CameraTargetVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 CameraTarget::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    TransformRotationThroughLink(link, &rotation);
    TransformVectorThroughLink(link, &point, 1);
    return 1;
}

void CameraPositioner::Destroy(u32 destroyFlags)
{
    vtable = g_CameraPositionerVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

u32 CameraPositioner::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    TransformRotationThroughLink(link, &rotation);
    TransformVectorThroughLink(link, &position, 1);
    return 1;
}

u32 CameraPositioner::KeepsRigRotation()
{
    return 0;
}

CameraRig* CameraRig::Construct(CameraRig* rig)
{
    rig->trigger = nullptr;
    rig->targetFollower = nullptr;
    rig->cameraFollower = nullptr;
    rig->target = nullptr;
    rig->positioner = nullptr;
    rig->vtable = g_CameraRigVTable;
    rig->bits.value = 0;
    return rig;
}

void CameraRig::Destroy(u32 destroyFlags)
{
    vtable = g_CameraRigVTable;
    DropTargetFollower();
    DropCameraFollower();
    DropTarget();
    if (bits.ownsPositioner != 0 && positioner != nullptr)
    {
        positioner->DestroyVirtual(DestroyAndFree);
    }

    positioner = nullptr;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CameraRig::DropTargetFollower()
{
    if (bits.ownsTargetFollower != 0 && targetFollower != nullptr)
    {
        targetFollower->DestroyVirtual(DestroyAndFree);
    }

    targetFollower = nullptr;
}

void CameraRig::DropCameraFollower()
{
    if (bits.ownsCameraFollower != 0 && cameraFollower != nullptr)
    {
        cameraFollower->DestroyVirtual(DestroyAndFree);
    }

    cameraFollower = nullptr;
}

void CameraRig::DropTarget()
{
    if (bits.ownsTarget != 0 && target != nullptr)
    {
        target->DestroyVirtual(DestroyAndFree);
    }

    target = nullptr;
}

void CameraRig::Reset(InstanceContext* instance)
{
    if (target != nullptr)
    {
        target->ResetVirtual();
        if (targetFollower != nullptr)
        {
            targetFollower->TakeVirtual(&target->rotation, &target->point);
        }
    }

    if (positioner != nullptr)
    {
        positioner->ResetVirtual(instance, target);
        if (cameraFollower != nullptr)
        {
            cameraFollower->TakeVirtual(&positioner->rotation, &positioner->position);
        }
    }
}

void CameraRig::Step(TimeClock* clock)
{
    f32 value = 0.0f;
    if (target != nullptr)
    {
        target->StepVirtual(clock);
        if (bits.ignoresTrigger == 0)
        {
            value = target->ValueVirtual(trigger);
        }

        if (bits.smoothed == 0 || target->smoothed == 0)
        {
            if (targetFollower != nullptr)
            {
                targetFollower->TakeVirtual(&target->rotation, &target->point);
            }
        }
        else if (targetFollower != nullptr)
        {
            // The trigger's camera says how the point is followed, unless it sets the rate (as it set the way, it's always set
            // back to the follower's own)
            targetFollower->StepVirtual(clock, &target->rotation, &target->point);
            MainCamera* camera = nullptr;
            if (bits.ignoresTrigger == 0)
            {
                camera = trigger != nullptr ? trigger->camera : nullptr;
                targetFollower->FollowSubtypeVirtual(camera != nullptr ? camera->first : nullptr);
            }

            if (camera != nullptr && camera->flags.setsTargetFollowRate != 0)
            {
                targetFollower->SetRateVirtual(camera->targetFollowRate);
            }
            else
            {
                targetFollower->ResetVirtual();
            }
        }
    }

    if (positioner == nullptr)
    {
        return;
    }

    if (bits.ignoresTrigger == 0)
    {
        positioner->TakeVirtual(value, trigger, target);
    }

    positioner->StepVirtual(clock, target);
    if (bits.smoothed == 0 || positioner->smoothed == 0)
    {
        if (cameraFollower != nullptr)
        {
            cameraFollower->TakeVirtual(&positioner->rotation, &positioner->position);
        }

        return;
    }

    if (cameraFollower == nullptr)
    {
        return;
    }

    if (positioner->keepsRate != 0)
    {
        cameraFollower->SetKeepsRateVirtual(1);
    }
    else
    {
        cameraFollower->SetKeepsRateVirtual(0);
        cameraFollower->ResetVirtual();
    }

    cameraFollower->StepVirtual(clock, &positioner->rotation, &positioner->position);
    if (bits.ignoresTrigger != 0)
    {
        return;
    }

    MainCamera* camera = trigger != nullptr ? trigger->camera : nullptr;
    cameraFollower->FollowSubtypeVirtual(camera != nullptr ? camera->second : nullptr);
    if (camera != nullptr && camera->flags.setsPositionFollowRate != 0)
    {
        cameraFollower->SetRateVirtual(camera->positionFollowRate);
        return;
    }

    cameraFollower->ResetVirtual();
}

u32 CameraRig::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    u32 can = 1;
    if (targetFollower != nullptr)
    {
        can = targetFollower->CanChangeChunkVirtual(from, link) != 0;
    }

    if (cameraFollower != nullptr)
    {
        can &= cameraFollower->CanChangeChunkVirtual(from, link) != 0;
    }

    if (target != nullptr)
    {
        can &= target->CanChangeChunkVirtual(from, link) != 0;
    }

    if (positioner != nullptr)
    {
        can &= positioner->CanChangeChunkVirtual(from, link) != 0;
    }

    return can;
}

s32* CameraRigPlace(s32* fov, CameraRig* rig, const Matrix4x4* matrix, Vector4* rotation, Vector4* position)
{
    CameraPositioner* positioner = rig->positioner;
    s32 angle = positioner != nullptr ? positioner->fov : g_DefaultFov;
    if (positioner == nullptr)
    {
        GetRotationVec(rotation, matrix);
    }
    else
    {
        rotation->x = positioner->rotation.x;
        rotation->y = positioner->rotation.y;
        rotation->z = positioner->rotation.z;
        rotation->w = positioner->rotation.w;
    }

    if (positioner != nullptr)
    {
        *position = positioner->position;
    }
    else
    {
        *position = *RowOf(matrix, 3);
    }

    if (rig->target != nullptr)
    {
        // The shaken target is worked out and dropped
        Vector4 shaken = rig->target->point;
        StepCameraShake(&g_CameraShake);
        Vector4 offset;
        CameraShakeOffset(&g_CameraShake, &offset);
        shaken.x = shaken.x + offset.x;
        shaken.y = shaken.y + offset.y;
        shaken.z = shaken.z + offset.z;
        if (rig->positioner == nullptr || rig->positioner->KeepsRigRotationVirtual() != 0)
        {
            *rotation = rig->keptRotation;
        }

        rig->keptRotation = *rotation;
    }

    *fov = angle;
    return fov;
}

CameraLensNode* CameraLensNode::Construct(CameraLensNode* node)
{
    // 45 degrees in radians
    constexpr f32 Fov = QuarterPi;
    constexpr f32 NearPlane = Rounded(0.1);
    constexpr f32 FarPlane = 1500.0f;
    constexpr f32 NtscPixelAspect = Rounded(0.96);
    GameNode::Construct(node);
    node->rig = nullptr;
    node->next = nullptr;
    node->vtable = g_CameraLensNodeVTable;
    node->bits.value = 0;
    node->bits.projectionChanged = 1;
    AngleFrom(&node->fov, Fov, AngleRadians);
    node->nearPlane = NearPlane;
    node->farPlane = FarPlane;
    node->pixelAspect = Platform::Graphics::IsPalDisplay() ? 1.0f : NtscPixelAspect;
    node->blendStart = 0;
    node->blendTicks = 0;
    return node;
}

void CameraLensNode::Destroy(u32 destroyFlags)
{
    vtable = g_CameraLensNodeVTable;
    if (bits.ownsRig != 0)
    {
        if (rig != nullptr)
        {
            rig->DestroyVirtual(DestroyAndFree);
        }

        bits.ownsRig = 0;
    }

    rig = nullptr;
    if (bits.ownsNext != 0)
    {
        if (next != nullptr)
        {
            next->DestroyVirtual(DestroyAndFree);
        }

        bits.ownsNext = 0;
    }

    next = nullptr;
    GameNode::Destroy(destroyFlags);
}

u32 CameraLensNode::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if (link->flags.linkedRm2Loaded == 0)
    {
        return 0;
    }

    u32 can = 1;
    if (rig != nullptr)
    {
        can = rig->CanChangeChunkVirtual(from, link) != 0;
    }

    if (next != nullptr)
    {
        can &= next->CanChangeChunkVirtual(from, link) != 0;
    }

    return can;
}

u32 CameraLensNode::Kind()
{
    return NodeCameraLens;
}

u32 CameraLensNode::Type()
{
    return TypeId;
}

void CameraLensNode::Step(TimeClock* clock, u32)
{
    if (rig != nullptr)
    {
        rig->ResetVirtual(owner);
    }

    if (next != nullptr)
    {
        next->ResetVirtual(owner);
    }

    time = clock->time;
}

u32 CameraLensNode::Update(TimeClock* clock)
{
    if (clock->flags.running != 0)
    {
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        Matrix4x4 matrix = place->matrix;
        s32 angle;
        CameraLensBlend(&angle, this, clock, &matrix);
        bits.projectionChanged = 1;
        fov = angle;
        if (SetPlaceMatrix(owner->place, &matrix) != 0)
        {
            QueueObject(owner);
        }
    }

    return GameNode::Update(clock);
}

s32* CameraLensBlend(s32* fov, CameraLensNode* lens, TimeClock* clock, Matrix4x4* matrix)
{
    f32 share = 0.0f;
    s32 nextAngle = 0;
    Vector4 nextRotation;
    Vector4 nextPosition;
    if (lens->next != nullptr)
    {
        s32 elapsed = 0;
        if (lens->blendStart == 0)
        {
            lens->blendStart = clock->time;
        }
        else
        {
            elapsed = static_cast<s32>(clock->time - lens->blendStart);
        }

        if (elapsed >= lens->blendTicks)
        {
            // Done: the rig blended to takes over
            if (lens->bits.ownsRig != 0)
            {
                if (lens->rig != nullptr)
                {
                    lens->rig->DestroyVirtual(DestroyAndFree);
                }

                lens->bits.ownsRig = 0;
            }

            // (It goes on saying it owns the next rig, which it no longer has)
            lens->rig = lens->next;
            lens->next = nullptr;
            lens->bits.ownsRig = lens->bits.ownsNext;
            share = 1.0f;
            lens->blendStart = 0;
        }
        else
        {
            lens->next->StepVirtual(clock);
            CameraRigPlace(&nextAngle, lens->next, matrix, &nextRotation, &nextPosition);
            share = static_cast<f32>(elapsed) * g_SecondsPerClockUnit /
                    (static_cast<f32>(lens->blendTicks) * g_SecondsPerClockUnit);
            if (lens->bits.curve == CurveSmooth)
            {
                f32 squared = share * share;
                f32 cubed = share * squared;
                share = squared * 3.0f - (cubed + cubed);
            }
        }
    }

    lens->rig->StepVirtual(clock);
    s32 rigAngle;
    Vector4 rotation;
    Vector4 position;
    CameraRigPlace(&rigAngle, lens->rig, matrix, &rotation, &position);
    s32 angle = rigAngle;
    if (0.0f < share && share < 1.0f)
    {
        f32 rigRadians = static_cast<f32>(rigAngle) * AngleToRadians;
        f32 nextRadians = static_cast<f32>(nextAngle) * AngleToRadians;
        angle = static_cast<s32>((nextRadians * share + rigRadians * (1.0f - share)) * RadiansToAngle);
        Vector4 between;
        SlerpRotations(share, &between, &rotation, &nextRotation);
        Vector4 place;
        place.x = position.x + (nextPosition.x - position.x) * share;
        place.y = position.y + (nextPosition.y - position.y) * share;
        place.w = 1.0f;
        place.z = position.z + (nextPosition.z - position.z) * share;
        MatrixFromRotation(matrix, &between);
        *RowOf(matrix, 3) = place;
    }
    else
    {
        MatrixFromRotation(matrix, &rotation);
        *RowOf(matrix, 3) = position;
    }

    *fov = angle;
    return fov;
}

CameraRig* CameraLensNode::SetRig(CameraRig* to, u32 reset)
{
    if (rig != to)
    {
        if (bits.ownsRig != 0)
        {
            if (rig != nullptr)
            {
                rig->DestroyVirtual(DestroyAndFree);
            }

            bits.ownsRig = 0;
        }

        rig = to;
        bits.ownsRig = 0;
    }

    if (bits.ownsNext != 0)
    {
        if (next != nullptr)
        {
            next->DestroyVirtual(DestroyAndFree);
        }

        bits.ownsNext = 0;
    }

    blendStart = 0;
    next = nullptr;
    bits.ownsNext = 0;
    if (reset != 0 && rig != nullptr)
    {
        rig->ResetVirtual(owner);
    }

    return rig;
}

CameraRig* CameraLensNode::BlendTo(CameraRig* to, s32 ticks, u8 curve)
{
    if (rig == nullptr)
    {
        SetRig(to, 1);
        return next;
    }

    if (blendStart != 0)
    {
        // The blend going on ends at once
        if (bits.ownsRig != 0)
        {
            rig->DestroyVirtual(DestroyAndFree);
            bits.ownsRig = 0;
        }

        rig = next;
        bits.ownsRig = bits.ownsNext;
    }
    else if (rig == to)
    {
        return next;
    }

    bits.ownsNext = 0;
    bits.curve = curve;
    blendTicks = ticks;
    next = to;
    blendStart = 0;
    if (to != nullptr)
    {
        to->ResetVirtual(owner);
    }

    return next;
}

void CameraTarget::ConstructBase(CameraTarget* target)
{
    target->vtable = g_CameraTargetVTable;
    target->along = 0.0f;
    target->smoothed = 1;
    target->facesMovement = 0;
    target->rotation.z = 0.0f;
    target->rotation.y = 0.0f;
    target->rotation.x = 0.0f;
    target->rotation.w = 1.0f;
    target->point = g_DefaultBox.min;
    target->point.w = 1.0f;
}

void CameraPositioner::ConstructBase(CameraPositioner* positioner)
{
    positioner->vtable = g_CameraPositionerVTable;
    positioner->unused00 = 0;
    positioner->smoothed = 1;
    positioner->fov = g_DefaultFov;
    positioner->rotation.z = 0.0f;
    positioner->rotation.y = 0.0f;
    positioner->rotation.x = 0.0f;
    positioner->rotation.w = 1.0f;
    positioner->position = g_DefaultBox.min;
    positioner->position.w = 1.0f;
}

void CutscenePositioner::Destroy(u32 destroyFlags)
{
    vtable = g_CameraPositionerVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CutscenePositioner::Reset(InstanceContext*, CameraTarget*)
{
    Clear();
}

void CutscenePositioner::Clear()
{
}

void CutscenePositioner::Step(TimeClock*, CameraTarget*)
{
}

void CutscenePositioner::Take(f32, CameraNode*, CameraTarget*)
{
}

CutsceneCameraRig* CutsceneCameraRig::Construct(CutsceneCameraRig* rig)
{
    CameraRig::Construct(rig);
    rig->vtable = g_CutsceneCameraRigVTable;
    CameraPositioner::ConstructBase(&rig->ownPositioner);
    rig->ownPositioner.vtable = g_CutscenePositionerVTable;
    rig->Assemble();
    return rig;
}

void CutsceneCameraRig::Destroy(u32 destroyFlags)
{
    vtable = g_CutsceneCameraRigVTable;
    ownPositioner.vtable = g_CameraPositionerVTable;
    CameraRig::Destroy(destroyFlags);
}

void CutsceneCameraRig::Prepare(InstanceContext*)
{
    AssembleVirtual();
}

void CutsceneCameraRig::RestoreDefaults()
{
}

void CutsceneCameraRig::Assemble()
{
    positioner = &ownPositioner;
}

GameCameraRig* GameCameraRig::Construct(GameCameraRig* rig)
{
    CameraRig::Construct(rig);
    rig->vtable = g_GameCameraRigVTable;
    CameraPointFollower::Construct(&rig->ownTargetFollower);
    CameraPointFollower::Construct(&rig->ownCameraFollower);
    CameraTarget::ConstructBase(&rig->ownTarget);
    rig->ownTarget.vtable = g_ScriptedCameraTargetVTable;
    rig->ownTarget.Reset();
    ScriptedCameraPositioner::Construct(&rig->ownPositioner);
    rig->scriptBits.unused0 = 0;
    rig->scriptBits.unused2 = 1;
    rig->ResetScript();
    rig->Assemble();
    return rig;
}

void GameCameraRig::Destroy(u32 destroyFlags)
{
    vtable = g_GameCameraRigVTable;
    ownPositioner.vtable = g_CameraPositionerVTable;
    ownTarget.vtable = g_CameraTargetVTable;
    ownTargetFollower.vtable = g_CameraFollowerBaseVTable;
    ownCameraFollower.vtable = g_CameraFollowerBaseVTable;
    CameraRig::Destroy(destroyFlags);
}

void GameCameraRig::Prepare(InstanceContext*)
{
    constexpr f32 OwnRate = 6.0f;
    ownCameraFollower.ownRate = OwnRate;
    ownTargetFollower.bits.ownWay = CameraPointFollower::WaySquareRoot;
    ownCameraFollower.bits.ownWay = CameraPointFollower::WayLinear;
    ownTargetFollower.ownRate = OwnRate;
}

void GameCameraRig::RestoreDefaults()
{
}

void GameCameraRig::Assemble()
{
    target = &ownTarget;
    cameraFollower = &ownCameraFollower;
    positioner = &ownPositioner;
    targetFollower = &ownTargetFollower;
}

namespace
{
// A place's x, y and z cleared (its w kept)
void ClearPlace(Vector4* place)
{
    f32 w = place->w;
    *place = g_DefaultBox.min;
    place->w = w;
}
}

void GameCameraRig::ResetScript()
{
    secondObject = nullptr;
    scriptBits.mirrored = 0;
    scriptBits.frameWanted = 1;
    scriptBits.frameChanged = 1;
    scriptBits.hasFirstPlace = 0;
    scriptBits.hasSecondPlace = 0;
    scriptBits.worldUp = 1;
    firstObject = nullptr;
    ClearPlace(&firstPlace);
    ClearPlace(&secondPlace);
    cameraPath = nullptr;
    targetPath = nullptr;
}

void GameCameraRig::ClearFirstPlace()
{
    ClearPlace(&firstPlace);
    scriptBits.hasFirstPlace = 0;
}

void GameCameraRig::ClearSecondPlace()
{
    ClearPlace(&secondPlace);
    scriptBits.hasSecondPlace = 0;
}

namespace
{
constexpr f32 PathStart = Rounded(1e-4);

Vector4 OriginPoint()
{
    Vector4 point = g_DefaultBox.min;
    point.w = 1.0f;
    return point;
}

// A time taken to seconds and back (what retail's ticks lose on the way)
s32 RoundTripTime(u32 time)
{
    return static_cast<s32>(static_cast<f32>(static_cast<s32>(time)) * g_SecondsPerClockUnit * g_ClockUnitsPerSecond);
}

// The share of a time passed in a time (seconds cut down to whole clock units)
f32 TimeShare(s32* elapsed, s32 ticks)
{
    f32 total = static_cast<f32>(ticks) * g_SecondsPerClockUnit;
    return static_cast<f32>(static_cast<s32>(ClockUnitsToSeconds(elapsed) / total * g_ClockUnitsPerSecond)) *
           g_SecondsPerClockUnit;
}

// 3 s squared less 2 s cubed
f32 SmoothStep(f32 share)
{
    f32 squared = share * share;
    f32 cubed = share * squared;
    return squared * 3.0f - (cubed + cubed);
}
}

void ScriptedCameraTarget::Destroy(u32 destroyFlags)
{
    vtable = g_CameraTargetVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScriptedCameraTarget::Reset()
{
    bits.moving = 0;
    bits.curve = CurveEven;
    rotation.z = 0.0f;
    rotation.y = 0.0f;
    rotation.x = 0.0f;
    rotation.w = 1.0f;
    point = OriginPoint();
    start = OriginPoint();
    end = OriginPoint();
    path = nullptr;
    along = 0.0f;
}

f32 ScriptedCameraTarget::Value(CameraNode*)
{
    return 0.0f;
}

void ScriptedCameraTarget::Stop()
{
    bits.moving = 0;
    bits.curve = CurveEven;
    moveTicks = 0;
    end = point;
    moveStart = 0;
    start = point;
}

f32 ScriptedCameraTarget::MoveShare(TimeClock* clock)
{
    f32 share = 1.0f;
    if (moveTicks <= 0)
    {
        return share;
    }

    if (bits.moving == 0)
    {
        bits.moving = 1;
        moveStart = clock->time;
        return 0.0f;
    }

    s32 since = static_cast<s32>(clock->time - moveStart);
    if (!(since < moveTicks))
    {
        moveTicks = 0;
        bits.moving = 0;
        return share;
    }

    share = static_cast<f32>(since) * g_SecondsPerClockUnit / (static_cast<f32>(moveTicks) * g_SecondsPerClockUnit);
    if (bits.curve == CurveSmooth)
    {
        share = SmoothStep(share);
    }

    return share;
}

void ScriptedCameraTarget::Step(TimeClock* clock)
{
    f32 share = MoveShare(clock);
    if (!(share < 1.0f))
    {
        point = end;
        start = end;
        return;
    }

    if (path == nullptr)
    {
        point.x = start.x + (end.x - start.x) * share;
        point.y = start.y + (end.y - start.y) * share;
        point.w = 1.0f;
        point.z = start.z + (end.z - start.z) * share;
        return;
    }

    if (share <= PathStart)
    {
        PathPointAt(0.0f, path, &point);
    }
    else
    {
        Vector4 along;
        PathPointAt(share, path, &along);
        point.x = point.x + along.x;
        point.y = point.y + along.y;
        point.z = point.z + along.z;
        point.x = point.x * 0.5f;
        point.y = point.y * 0.5f;
        point.z = point.z * 0.5f;
    }

    Vector4 way;
    PathDirectionAt(share, path, &way);
    point.x = point.x + way.x;
    point.y = point.y + way.y;
    point.z = point.z + way.z;
}

ScriptedCameraPositioner* ScriptedCameraPositioner::Construct(ScriptedCameraPositioner* positioner)
{
    CameraPositioner::ConstructBase(positioner);
    positioner->vtable = g_ScriptedCameraPositionerVTable;
    positioner->Clear();
    return positioner;
}

void ScriptedCameraPositioner::Destroy(u32 destroyFlags)
{
    vtable = g_CameraPositionerVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void ScriptedCameraPositioner::Reset(InstanceContext*, CameraTarget*)
{
    Clear();
}

void ScriptedCameraPositioner::Take(f32, CameraNode*, CameraTarget*)
{
}

void ScriptedCameraPositioner::Clear()
{
    bits.easesIn = 0;
    bits.easesOut = 0;
    bits.curve = CurveEven;
    rotation.z = 0.0f;
    rotation.y = 0.0f;
    rotation.x = 0.0f;
    rotation.w = 1.0f;
    position = OriginPoint();
    // Retail clears the rotation again, its w too
    rotation.y = 0.0f;
    rotation.w = 0.0f;
    rotation.z = 0.0f;
    rotation.x = 0.0f;
    start = OriginPoint();
    end = OriginPoint();
    startFov = 0;
    bits.arcs = 0;
    endFov = 0;
    easeStart = 0;
    moveStart = 0;
    unused00 = 0;
    unused44 = 0;
    path = nullptr;
    fov = g_DefaultFov;
}

void ScriptedCameraPositioner::Stop()
{
    moveStart = 0;
    moveTicks = 0;
    f32 kept = start.w;
    start = g_DefaultBox.min;
    start.w = kept;
    bits.easesIn = 0;
    bits.easesOut = 0;
    bits.arcs = 0;
    bits.curve = CurveEven;
    fov = endFov;
    end = position;
}

f32 ScriptedCameraPositioner::LinearShare(const u32* now)
{
    s32 elapsed = static_cast<s32>(*now - static_cast<u32>(RoundTripTime(moveStart)));
    f32 share = TimeShare(&elapsed, moveTicks);
    if (bits.curve == CurveSmooth)
    {
        share = SmoothStep(share);
    }

    return share;
}

f32 ScriptedCameraPositioner::EaseShare(const u32* now)
{
    s32 elapsed = static_cast<s32>(*now - static_cast<u32>(RoundTripTime(easeStart)));
    f32 share = TimeShare(&elapsed, easeTicks);
    if (bits.curve == CurveSmooth)
    {
        share = SmoothStep(share);
        if (bits.easesOut != 0)
        {
            share = 1.0f - share;
        }
    }

    return share;
}

f32 ScriptedCameraPositioner::MoveShare(TimeClock* clock)
{
    f32 share = 1.0f;
    u32 now = clock->time;
    if (moveStart == 0)
    {
        moveStart = now;
        start = position;
        startFov = fov;
    }

    if (!(moveTicks < static_cast<s32>(now - static_cast<u32>(RoundTripTime(moveStart)))))
    {
        if (bits.easesIn != 0)
        {
            if (easeStart == 0)
            {
                easeStart = now;
            }

            f32 ease = EaseShare(&now);
            share = ease * TimeShare(&easeTicks, moveTicks);
            if (ease == 1.0f)
            {
                easeStart = 0;
                bits.easesIn = 0;
            }
        }
        else if (bits.easesOut != 0)
        {
            if (easeStart == 0)
            {
                easeStart = now;
            }

            f32 ease = EaseShare(&now);
            s32 steady = moveTicks - RoundTripTime(static_cast<u32>(easeTicks));
            share = TimeShare(&steady, moveTicks);
            share = share + ease * TimeShare(&easeTicks, moveTicks);
            if (ease == 1.0f)
            {
                easeStart = 0;
                bits.easesOut = 0;
            }
        }
        else
        {
            share = LinearShare(&now);
        }
    }

    if (share == 1.0f)
    {
        moveStart = 0;
    }

    return share;
}

void ScriptedCameraPositioner::LookAt(const Vector4* point, Vector4* turn, const Vector4* eye)
{
    Vector4 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    up.w = 1.0f;
    Matrix4x4 matrix;
    MatrixFromRotation(&matrix, turn);
    LookAtMatrix(&matrix, eye, point, &up);
    GetRotationVec(turn, &matrix);
}

void ScriptedCameraPositioner::Step(TimeClock* clock, CameraTarget* target)
{
    Vector4 point = target->point;
    if (position.x == end.x && position.y == end.y && position.z == end.z)
    {
        LookAt(&point, &rotation, &position);
        return;
    }

    if (moveTicks <= 0)
    {
        s32 angle = endFov;
        fov = angle;
        position = end;
        start = end;
        startFov = angle;
        LookAt(&point, &rotation, &position);
        return;
    }

    f32 share = MoveShare(clock);
    if (share <= 1.0f)
    {
        if (path == nullptr)
        {
            position.x = start.x + (end.x - start.x) * share;
            position.y = start.y + (end.y - start.y) * share;
            position.w = 1.0f;
            position.z = start.z + (end.z - start.z) * share;
        }
        else if (share <= PathStart)
        {
            PathPointAt(share, path, &position);
        }
        else
        {
            Vector4 along;
            PathPointAt(share, path, &along);
            position.x = position.x + along.x;
            position.y = position.y + along.y;
            position.z = position.z + along.z;
            position.x = position.x * 0.5f;
            position.y = position.y * 0.5f;
            position.z = position.z * 0.5f;
        }

        s32 from = startFov;
        fov = *AddRadiansToAngle(&from, share * (static_cast<f32>(endFov - startFov) * AngleToRadians));
        if (bits.arcs != 0)
        {
            f32 x = start.x - point.x;
            f32 y = start.y - point.y;
            f32 z = start.z - point.z;
            f32 rest = 1.0f - share;
            f32 startLength = __builtin_sqrtf(x * x + y * y + z * z);
            x = end.x - point.x;
            y = end.y - point.y;
            z = end.z - point.z;
            f32 endLength = __builtin_sqrtf(x * x + y * y + z * z);
            Vector4 way;
            way.x = position.x - point.x;
            way.y = position.y - point.y;
            way.z = position.z - point.z;
            way.w = 1.0f;
            f32 radius = rest * startLength + share * endLength;
            f32 inverse = InverseLength(&way, LengthEpsilon);
            way.x = way.x * inverse;
            way.y = way.y * inverse;
            way.z = way.z * inverse;
            position.x = point.x + way.x * radius;
            position.y = point.y + way.y * radius;
            position.z = point.z + way.z * radius;
            position.w = 1.0f;
        }
    }

    if (share == 1.0f)
    {
        moveTicks = 0;
    }

    LookAt(&point, &rotation, &position);
}

void GameCameraRig::MakeFrame()
{
    if (scriptBits.frameWanted == 0 && scriptBits.frameChanged == 0)
    {
        return;
    }

    if (firstObject != nullptr)
    {
        ObjectPlace* place = firstObject->place;
        place->SyncPosition();
        firstPlace = place->position;
        if (secondObject != nullptr)
        {
            ObjectPlace* other = secondObject->place;
            other->SyncPosition();
            secondPlace = other->position;
        }
        else if (scriptBits.hasSecondPlace == 0)
        {
            place = firstObject->place;
            RotateAndTranslate(place);
            Vector4 forward = *RowOf(&place->matrix, 2);
            Vector4 ahead;
            ahead.x = firstPlace.x + forward.x;
            ahead.y = firstPlace.y + forward.y;
            ahead.z = firstPlace.z + forward.z;
            ahead.w = 1.0f;
            secondPlace = ahead;
        }
    }
    else if (scriptBits.hasFirstPlace != 0 && secondObject != nullptr)
    {
        ObjectPlace* other = secondObject->place;
        other->SyncPosition();
        secondPlace = other->position;
    }

    Vector4 way;
    way.x = secondPlace.x - firstPlace.x;
    way.y = secondPlace.y - firstPlace.y;
    way.z = secondPlace.z - firstPlace.z;
    way.w = 1.0f;
    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    Vector4 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    up.w = 1.0f;
    Vector4 side = way;
    side.z = way.x * up.y - way.y * up.x;
    side.y = way.z * up.x - way.x * up.z;
    side.x = way.y * up.z - way.z * up.y;
    inverse = InverseLength(&side, LengthEpsilon);
    side.x = side.x * inverse;
    side.y = side.y * inverse;
    side.z = side.z * inverse;
    if (scriptBits.worldUp == 0)
    {
        up.x = side.x;
        up.y = side.y;
        up.z = side.z;
        f32 x = up.y * way.z - up.z * way.y;
        f32 y = up.z * way.x - up.x * way.z;
        f32 z = up.x * way.y - up.y * way.x;
        up.y = y;
        up.z = z;
        up.x = x;
        inverse = InverseLength(&up, LengthEpsilon);
        up.x = up.x * inverse;
        up.y = up.y * inverse;
        up.z = up.z * inverse;
    }

    side.w = 0.0f;
    up.w = 0.0f;
    way.w = 0.0f;
    Vector4 origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    origin.w = 1.0f;
    MatrixFromColumns(&frame, &side, &up, &way, &origin);
    scriptBits.frameChanged = 0;
}

void InitCameraModule(u32 initialise, u32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    ConstructCameraShake(&g_CameraShake);
    AngleFrom(&g_UnreadCameraAngle9A0, QuarterPi, AngleRadians);
    AngleFrom(&g_CameraAngleDeadZone, Rounded(0.1), AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9B0, 135.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9B8, -135.0f, AngleDegrees);
    AngleFrom(&g_CameraTiltRate, 90.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9C8, 10.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9D0, 0.0f, AngleDegrees);
    AngleFrom(&g_FollowCameraPitch, 15.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9E0, 720.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9E8, 90.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9F0, 5.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngle9F8, 360.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA00, 90.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA08, 5.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA10, 720.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA18, 180.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA20, 180.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA28, 180.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA30, 60.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA38, 120.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA40, -45.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA48, 75.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA50, 7.0f, AngleDegrees);
    g_UnreadCameraVector.w = 1.0f;
    g_UnreadCameraVector.x = 0.0f;
    g_UnreadCameraVector.y = 0.0f;
    g_UnreadCameraVector.z = -1.0f;
    AngleFrom(&g_UnreadCameraAngleA58, 180.0f, AngleDegrees);
    AngleFrom(&g_UnreadCameraAngleA60, 90.0f, AngleDegrees);
}

void ConstructCameraModule()
{
    InitCameraModule(1, DefaultInitPriority);
}
