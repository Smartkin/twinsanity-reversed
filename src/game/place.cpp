#include "game/place.h"

#include "game/memory.h"
#include "gcc2.h"
#include "platform/math.h"

extern "C"
{
    // The items' builders' base (BuilderBaseFunctions)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);

    // The maths module's items' builder (a vtable alone, D_002F5CC0, the game context's): a place or a point of a class (none for
    // another), and its destructor
    void* MakeMathsItem(void* builder, u32 classId) RETAIL(FUN_0018d318);
    void DestroyMathsItemBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0018d2e8);

    // The turn about x by an angle, and a quadword copied
    void RotationFromPitch(Vector4* rotation, const s32* pitch) RETAIL(FUN_0018dd20);
    Vector4* CopyQuadword(Vector4* to, const Vector4* from) RETAIL(MovePositionFromPos2ToPos1);
}

namespace
{
constexpr u32 PlaceClassId = 0x1507;
constexpr u32 PointClassId = 0x150D;

bool IsNoMove(const Vector4* move)
{
    return __builtin_fabsf(move->x) <= Epsilon && __builtin_fabsf(move->y) <= Epsilon && __builtin_fabsf(move->z) <= Epsilon;
}

// A rotation of no turn: x, y and z none and w 1 or -1
bool IsNoTurn(const Vector4* rotation)
{
    if (!IsNoMove(rotation))
    {
        return false;
    }

    return __builtin_fabsf(rotation->w - 1.0f) <= Epsilon || __builtin_fabsf(rotation->w + 1.0f) <= Epsilon;
}

// The rotation taken from the matrix when it turned, and the place turned by a rotation after its own
void StartTurn(ObjectPlace* place)
{
    place->SyncRotation();
    place->MarkTurned();
}

// The place turned about an axis by an angle (a copy of it handed on, as retail does)
template <void (*MakeTurn)(Vector4*, const s32*)>
u32 TurnPlaceAbout(ObjectPlace* place, const s32* angle)
{
    if (*angle == 0)
    {
        return 0;
    }

    StartTurn(place);
    s32 copy = *angle;
    Vector4 turn;
    MakeTurn(&turn, &copy);
    MultiplyRotations(&place->rotation, &place->rotation, &turn);
    return 1;
}
}

u32 SetPlaceMatrix(ObjectPlace* place, const Matrix4x4* matrix)
{
    place->matrix = *matrix;
    // The matrix newer than the position and the rotation
    place->bits.value = (place->bits.value & ~(PlaceBits::Moved | PlaceBits::Turned)) | PlaceBits::MatrixMoved |
                        PlaceBits::MatrixTurned;
    return 1;
}

ObjectPlace* ConstructObjectPlace(ObjectPlace* place)
{
    InitIdentityMatrix(&place->matrix);
    // The two sides in step
    place->bits.value &= ~u64{PlaceBits::Moved} & ~u64{PlaceBits::Turned} & ~u64{PlaceBits::MatrixMoved} &
                         ~u64{PlaceBits::MatrixTurned};
    place->position = g_DefaultBox.min;
    place->position.w = 1.0f;
    place->rotation.z = 0.0f;
    place->rotation.y = 0.0f;
    place->rotation.x = 0.0f;
    place->rotation.w = 1.0f;
    return place;
}

ObjectPlace* CopyObjectPlace(ObjectPlace* place, const ObjectPlace* other)
{
    *place = *other;
    return place;
}

ObjectPlace* AssignObjectPlace(ObjectPlace* place, const ObjectPlace* other)
{
    *place = *other;
    return place;
}

void RotateAndTranslate(ObjectPlace* place)
{
    if (place->bits.turned != 0)
    {
        Vector4* rotation = &place->rotation;
        f32 x = rotation->x;
        f32 lengthSquared = x * x + rotation->y * rotation->y + rotation->z * rotation->z + rotation->w * rotation->w;
        f32 inverse = Platform::Math::DivideBySquareRoot(1.0f, lengthSquared);
        rotation->x = x * inverse;
        rotation->y = rotation->y * inverse;
        rotation->z = rotation->z * inverse;
        rotation->w = rotation->w * inverse;
        MatrixFromRotation(&place->matrix, rotation);
        place->bits.turned = 0;
    }

    if (place->bits.moved != 0)
    {
        *RowOf(&place->matrix, 3) = place->position;
        place->bits.moved = 0;
    }
}

void* MakeMathsItem(void*, u32 classId)
{
    switch (classId)
    {
    case PlaceClassId:
        return ConstructObjectPlace(static_cast<ObjectPlace*>(MemoryAllocate(sizeof(ObjectPlace))));
    case PointClassId:
        // Left as the allocator gives it
        return MemoryAllocate(sizeof(Vector4));
    default:
        return nullptr;
    }
}

void DestroyMathsItemBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

void SyncPlacePosition(ObjectPlace* place)
{
    place->SyncPosition();
}

u32 ResetObjectPlace(ObjectPlace* place)
{
    SyncPlacePosition(place);
    if (IsNoMove(&place->position) && IsNoTurn(&place->rotation))
    {
        return 0;
    }

    // The position and the rotation newer than the matrix
    place->bits.value = (place->bits.value | PlaceBits::Moved | PlaceBits::Turned) & ~u64{PlaceBits::MatrixMoved} &
                        ~u64{PlaceBits::MatrixTurned};
    CopyQuadword(&place->position, &g_DefaultBox.min);
    place->position.w = 1.0f;
    place->rotation.z = 0.0f;
    place->rotation.y = 0.0f;
    place->rotation.x = 0.0f;
    place->rotation.w = 1.0f;
    return 1;
}

u32 MovePlaceLocally(ObjectPlace* place, const Vector4* offset)
{
    if (IsNoMove(offset))
    {
        return 0;
    }

    RotateAndTranslate(place);
    place->SyncPosition();
    place->MarkMoved();
    const Matrix4x4& matrix = place->matrix;
    Vector4& position = place->position;
    position.x = position.x + (matrix.m[0][0] * offset->x + matrix.m[1][0] * offset->y + matrix.m[2][0] * offset->z);
    position.y = position.y + (matrix.m[0][1] * offset->x + matrix.m[1][1] * offset->y + matrix.m[2][1] * offset->z);
    position.z = position.z + (matrix.m[0][2] * offset->x + matrix.m[1][2] * offset->y + matrix.m[2][2] * offset->z);
    return 1;
}

u32 TurnPlace(ObjectPlace* place, const Vector4* rotation)
{
    if (IsNoTurn(rotation))
    {
        return 0;
    }

    StartTurn(place);
    Vector4 turned;
    turned.x = rotation->x;
    turned.y = rotation->y;
    turned.z = rotation->z;
    turned.w = rotation->w;
    MultiplyRotations(&turned, &turned, &place->rotation);
    Vector4* own = &place->rotation;
    own->x = turned.x;
    own->y = turned.y;
    own->z = turned.z;
    own->w = turned.w;
    f32 inverse = InverseLength4(0.0f, InverseEpsilon, own);
    own->x = own->x * inverse;
    own->y = own->y * inverse;
    own->z = own->z * inverse;
    own->w = own->w * inverse;
    return 1;
}

u32 TurnPlaceAboutX(ObjectPlace* place, const s32* angle)
{
    return TurnPlaceAbout<RotationFromPitch>(place, angle);
}

u32 TurnPlaceAboutY(ObjectPlace* place, const s32* angle)
{
    return TurnPlaceAbout<RotationFromYaw>(place, angle);
}

u32 TurnPlaceAboutZ(ObjectPlace* place, const s32* angle)
{
    return TurnPlaceAbout<RotationFromRoll>(place, angle);
}
