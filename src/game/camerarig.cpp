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
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
// 65536ths of a turn to radians and back
constexpr f32 AngleToRadians = 0x1.921fb6p-14f;
constexpr f32 RadiansToAngle = 0x1.45f306p+13f;

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
    shake->unknown84 = 1.0f;
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
    constexpr s32 Weakest = 20;
    constexpr s32 Strongest = 255;
    s32 motor = static_cast<s32>(strength * 1024.0f);
    if (motor > Strongest)
    {
        motor = Strongest;
    }

    if (motor < Weakest)
    {
        motor = Weakest;
    }

    VibrationRequest request;
    request.bits = FirstPad | VibrationRequest::Pending | static_cast<u32>(motor) << VibrationRequest::BigMotorShift;
    request.seconds = Rounded(0.4);
    RequestVibration(&request);
}

CameraPointFollower* CameraPointFollower::Construct(CameraPointFollower* follower)
{
    constexpr f32 OwnRate = 5.0f;
    follower->vtable = g_CameraPointFollowerVTable;
    follower->ownRate = OwnRate;
    follower->rate = OwnRate;
    follower->bits = WayLinear | WayLinear << OwnWayShift;
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
    bits = (bits & ~WayMask) | (bits >> OwnWayShift & WayMask);
    if ((bits & BitKeepsRate) == 0)
    {
        rate = ownRate;
    }
}

void CameraPointFollower::Step(TimeClock* clock, const Vector4*, Vector4* to)
{
    constexpr f32 MostKeptRate = 100.0f;
    if ((clock->flags & TimeClock::FlagRunning) == 0)
    {
        return;
    }

    if ((bits & BitKeepsRate) != 0 && rate < MostKeptRate)
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
        switch (bits & WayMask)
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
    if (subtype == nullptr || (subtype->flags & CameraSubtype::FollowMask) == CameraSubtype::FollowOwnWay)
    {
        Reset();
        return;
    }

    if ((subtype->flags & CameraSubtype::FollowMask) != CameraSubtype::FollowAtRate)
    {
        bits = bits & ~WayMask;
        return;
    }

    bits = (bits & ~WayMask) | WayLinear;
    if ((bits & BitKeepsRate) == 0)
    {
        rate = subtype->rate;
    }
}

void CameraPointFollower::SetRate(f32 newRate)
{
    if (!(0.0f <= newRate))
    {
        bits = bits & ~WayMask;
        return;
    }

    bits = (bits & ~WayMask) | WayLinear;
    if ((bits & BitKeepsRate) == 0)
    {
        rate = newRate;
    }
}

void CameraPointFollower::SetKeepsRate(u32 keeps)
{
    bits = (bits & ~BitKeepsRate) | (keeps & 1) << 8;
}

u32 CameraPointFollower::CanChangeChunk(ChunkData*, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
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
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
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
    rig->bits = 0;
    return rig;
}

void CameraRig::Destroy(u32 destroyFlags)
{
    vtable = g_CameraRigVTable;
    DropTargetFollower();
    DropCameraFollower();
    DropTarget();
    if ((bits & BitOwnsPositioner) != 0 && positioner != nullptr)
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
    if ((bits & BitOwnsTargetFollower) != 0 && targetFollower != nullptr)
    {
        targetFollower->DestroyVirtual(DestroyAndFree);
    }

    targetFollower = nullptr;
}

void CameraRig::DropCameraFollower()
{
    if ((bits & BitOwnsCameraFollower) != 0 && cameraFollower != nullptr)
    {
        cameraFollower->DestroyVirtual(DestroyAndFree);
    }

    cameraFollower = nullptr;
}

void CameraRig::DropTarget()
{
    if ((bits & BitOwnsTarget) != 0 && target != nullptr)
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
        if ((bits & BitIgnoresTrigger) == 0)
        {
            value = target->ValueVirtual(trigger);
        }

        if ((bits & BitSmoothed) == 0 || target->smoothed == 0)
        {
            if (targetFollower != nullptr)
            {
                targetFollower->TakeVirtual(&target->rotation, &target->point);
            }
        }
        else if (targetFollower != nullptr)
        {
            // The trigger's camera says how the point is followed, unless its value is the rate (as it set the way, it's always
            // set back to the follower's own)
            targetFollower->StepVirtual(clock, &target->rotation, &target->point);
            MainCamera* camera = nullptr;
            if ((bits & BitIgnoresTrigger) == 0)
            {
                camera = trigger != nullptr ? trigger->camera : nullptr;
                targetFollower->FollowSubtypeVirtual(camera != nullptr ? camera->first : nullptr);
            }

            if (camera != nullptr && (camera->flags & MainCamera::FlagPassesFirstValue) != 0)
            {
                targetFollower->SetRateVirtual(camera->firstValue);
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

    if ((bits & BitIgnoresTrigger) == 0)
    {
        positioner->TakeVirtual(value, trigger, target);
    }

    positioner->StepVirtual(clock, target);
    if ((bits & BitSmoothed) == 0 || positioner->smoothed == 0)
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
    if ((bits & BitIgnoresTrigger) != 0)
    {
        return;
    }

    MainCamera* camera = trigger != nullptr ? trigger->camera : nullptr;
    cameraFollower->FollowSubtypeVirtual(camera != nullptr ? camera->second : nullptr);
    if (camera != nullptr && (camera->flags & MainCamera::FlagPassesSecondValue) != 0)
    {
        cameraFollower->SetRateVirtual(camera->secondValue);
        return;
    }

    cameraFollower->ResetVirtual();
}

u32 CameraRig::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
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
    constexpr f32 Fov = 0x1.921fb6p-1f;
    constexpr f32 NearPlane = Rounded(0.1);
    constexpr f32 FarPlane = 1500.0f;
    constexpr f32 NtscPixelAspect = Rounded(0.96);
    GameNode::Construct(node);
    node->rig = nullptr;
    node->next = nullptr;
    node->vtable = g_CameraLensNodeVTable;
    node->bits = BitProjectionChanged;
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
    if ((bits & BitOwnsRig) != 0)
    {
        if (rig != nullptr)
        {
            rig->DestroyVirtual(DestroyAndFree);
        }

        bits &= ~BitOwnsRig;
    }

    rig = nullptr;
    if ((bits & BitOwnsNext) != 0)
    {
        if (next != nullptr)
        {
            next->DestroyVirtual(DestroyAndFree);
        }

        bits &= ~BitOwnsNext;
    }

    next = nullptr;
    GameNode::Destroy(destroyFlags);
}

u32 CameraLensNode::CanChangeChunk(ChunkData* from, ChunkLinkData* link)
{
    if ((link->flags & ChunkLinkData::LinkedRm2Loaded) == 0)
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
    if ((clock->flags & TimeClock::FlagRunning) != 0)
    {
        ObjectPlace* place = owner->place;
        RotateAndTranslate(place);
        Matrix4x4 matrix = place->matrix;
        s32 angle;
        CameraLensBlend(&angle, this, clock, &matrix);
        bits |= BitProjectionChanged;
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
            if ((lens->bits & CameraLensNode::BitOwnsRig) != 0)
            {
                if (lens->rig != nullptr)
                {
                    lens->rig->DestroyVirtual(DestroyAndFree);
                }

                lens->bits &= ~CameraLensNode::BitOwnsRig;
            }

            u32 bits = lens->bits;
            lens->rig = lens->next;
            lens->next = nullptr;
            lens->bits = (bits & ~CameraLensNode::BitOwnsRig) | (bits >> 1 & CameraLensNode::BitOwnsRig);
            share = 1.0f;
            lens->blendStart = 0;
        }
        else
        {
            lens->next->StepVirtual(clock);
            CameraRigPlace(&nextAngle, lens->next, matrix, &nextRotation, &nextPosition);
            share = static_cast<f32>(elapsed) * g_SecondsPerClockUnit /
                    (static_cast<f32>(lens->blendTicks) * g_SecondsPerClockUnit);
            if ((lens->bits >> CameraLensNode::CurveShift & 7) == CameraLensNode::CurveEased)
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
        if ((bits & BitOwnsRig) != 0)
        {
            if (rig != nullptr)
            {
                rig->DestroyVirtual(DestroyAndFree);
            }

            bits &= ~BitOwnsRig;
        }

        rig = to;
        bits &= ~BitOwnsRig;
    }

    if ((bits & BitOwnsNext) != 0)
    {
        if (next != nullptr)
        {
            next->DestroyVirtual(DestroyAndFree);
        }

        bits &= ~BitOwnsNext;
    }

    blendStart = 0;
    next = nullptr;
    bits &= ~BitOwnsNext;
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
        if ((bits & BitOwnsRig) != 0)
        {
            rig->DestroyVirtual(DestroyAndFree);
            bits &= ~BitOwnsRig;
        }

        rig = next;
        bits = (bits & ~BitOwnsRig) | (bits >> 1 & BitOwnsRig);
    }
    else if (rig == to)
    {
        return next;
    }

    bits = (bits & ~BitOwnsNext & ~CurveMask) | (curve & 7) << CurveShift;
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
    positioner->unknown00 = 0;
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

void CutsceneCameraRig::Nothing()
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
    rig->scriptBits = (rig->scriptBits & ~3u) | 4;
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
    ownTargetFollower.bits = (ownTargetFollower.bits & ~CameraPointFollower::OwnWayMask) |
                             CameraPointFollower::WaySquareRoot << CameraPointFollower::OwnWayShift;
    ownCameraFollower.bits = (ownCameraFollower.bits & ~CameraPointFollower::OwnWayMask) |
                             CameraPointFollower::WayLinear << CameraPointFollower::OwnWayShift;
    ownTargetFollower.ownRate = OwnRate;
}

void GameCameraRig::Nothing()
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
    scriptBits = (scriptBits & ~0x8u) | BitFrameWanted | BitFrameChanged;
    scriptBits = (scriptBits & ~(BitHasFirstPlace | BitHasSecondPlace)) | BitWorldUp;
    firstObject = nullptr;
    ClearPlace(&firstPlace);
    ClearPlace(&secondPlace);
    unknownB8 = 0;
    unknownB4 = 0;
}

void GameCameraRig::ClearFirstPlace()
{
    ClearPlace(&firstPlace);
    scriptBits &= ~BitHasFirstPlace;
}

void GameCameraRig::ClearSecondPlace()
{
    ClearPlace(&secondPlace);
    scriptBits &= ~BitHasSecondPlace;
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
    bits &= ~(BitMoving | CurveMask);
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
    bits &= ~(BitMoving | CurveMask);
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

    if ((bits & BitMoving) == 0)
    {
        bits |= BitMoving;
        moveStart = clock->time;
        return 0.0f;
    }

    s32 since = static_cast<s32>(clock->time - moveStart);
    if (!(since < moveTicks))
    {
        moveTicks = 0;
        bits &= ~BitMoving;
        return share;
    }

    share = static_cast<f32>(since) * g_SecondsPerClockUnit / (static_cast<f32>(moveTicks) * g_SecondsPerClockUnit);
    if ((bits >> CurveShift & 7) == CurveSmooth)
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
    bits &= ~(BitEasesIn | BitEasesOut | CurveMask);
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
    bits &= ~BitArcs;
    endFov = 0;
    easeStart = 0;
    moveStart = 0;
    unknown00 = 0;
    unknown44 = 0;
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
    bits &= ~(BitEasesIn | BitEasesOut | BitArcs | CurveMask);
    fov = endFov;
    end = position;
}

f32 ScriptedCameraPositioner::LinearShare(const u32* now)
{
    s32 elapsed = static_cast<s32>(*now - static_cast<u32>(RoundTripTime(moveStart)));
    f32 share = TimeShare(&elapsed, moveTicks);
    if ((bits >> CurveShift & 7) == CurveSmooth)
    {
        share = SmoothStep(share);
    }

    return share;
}

f32 ScriptedCameraPositioner::EaseShare(const u32* now)
{
    s32 elapsed = static_cast<s32>(*now - static_cast<u32>(RoundTripTime(easeStart)));
    f32 share = TimeShare(&elapsed, easeTicks);
    if ((bits >> CurveShift & 7) == CurveSmooth)
    {
        share = SmoothStep(share);
        if ((bits & BitEasesOut) != 0)
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
        if ((bits & BitEasesIn) != 0)
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
                bits &= ~BitEasesIn;
            }
        }
        else if ((bits & BitEasesOut) != 0)
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
                bits &= ~BitEasesOut;
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
        if ((bits & BitArcs) != 0)
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
    if ((scriptBits & (BitFrameWanted | BitFrameChanged)) == 0)
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
        else if ((scriptBits & BitHasSecondPlace) == 0)
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
    else if ((scriptBits & BitHasFirstPlace) != 0 && secondObject != nullptr)
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
    if ((scriptBits & BitWorldUp) == 0)
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
    scriptBits &= ~BitFrameChanged;
}

void InitCameraModule(u32 initialise, u32 priority)
{
    constexpr u32 AllPriorities = 0xFFFF;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    ConstructCameraShake(&g_CameraShake);
    AngleFrom(&g_UnreadCameraAngle9A0, 0x1.921fb6p-1f, AngleRadians);
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
    InitCameraModule(1, 0xFFFF);
}
