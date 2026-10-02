#include "game/place.h"

#include "platform/math.h"

u32 SetPlaceMatrix(ObjectPlace* place, const Matrix4x4* matrix)
{
    place->matrix = *matrix;
    place->bits = (place->bits & ~(ObjectPlace::BitMoved | ObjectPlace::BitTurned)) | ObjectPlace::BitMatrixMoved |
                  ObjectPlace::BitMatrixTurned;
    return 1;
}

ObjectPlace* ConstructObjectPlace(ObjectPlace* place)
{
    InitIdentityMatrix(&place->matrix);
    place->bits &= ~u64{ObjectPlace::BitMoved} & ~u64{ObjectPlace::BitTurned} & ~u64{ObjectPlace::BitMatrixMoved} &
                   ~u64{ObjectPlace::BitMatrixTurned};
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
    if ((place->bits & ObjectPlace::BitTurned) != 0)
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
        place->bits &= ~u64{ObjectPlace::BitTurned};
    }

    if ((place->bits & ObjectPlace::BitMoved) != 0)
    {
        *RowOf(&place->matrix, 3) = place->position;
        place->bits &= ~u64{ObjectPlace::BitMoved};
    }
}
