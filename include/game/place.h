#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"

struct ObjectPlace;

extern "C"
{
    // The rotation a matrix turns by, and a place turned by a rotation (after its own, made a unit one) unless it's
    // (within 5e-05) none: whether it turned
    void GetRotationVec(Vector4* rotation, const Matrix4x4* matrix) RETAIL(GetRotationVec_);
    u32 TurnPlace(ObjectPlace* place, const Vector4* rotation) RETAIL(Rotate);
    // A place turned about its own x, y or z axis by an angle unless it's 0: whether it turned
    u32 TurnPlaceAboutX(ObjectPlace* place, const s32* angle) RETAIL(FUN_0019af18);
    u32 TurnPlaceAboutY(ObjectPlace* place, const s32* angle) RETAIL(FUN_0019afd0);
    u32 TurnPlaceAboutZ(ObjectPlace* place, const s32* angle) RETAIL(FUN_0019b088);
    // A place moved along its own axes unless the offset is (within 5e-05) none: whether it moved
    u32 MovePlaceLocally(ObjectPlace* place, const Vector4* offset) RETAIL(FUN_0019abc0);
    // A place's position taken from its matrix once it moved (ObjectPlace::SyncPosition out of line), and a place set to none (at
    // the default box's corner, not turned) unless it's (within 5e-05) there: whether it moved
    void SyncPlacePosition(ObjectPlace* place) RETAIL(FUN_0019ab68);
    u32 ResetObjectPlace(ObjectPlace* place) RETAIL(FUN_0019a370);
    // A place's matrix set (it moved and turned, the position and rotation now out of date): whether it was (always)
    u32 SetPlaceMatrix(ObjectPlace* place, const Matrix4x4* matrix) RETAIL(FUN_0018b6d0);
    // A place made: at the origin, not turned, its matrix the identity
    ObjectPlace* ConstructObjectPlace(ObjectPlace* place) RETAIL(ConstructTransGeoPoint);
    // A place copied (retail's copy constructor and assignment): the place
    ObjectPlace* CopyObjectPlace(ObjectPlace* place, const ObjectPlace* other) RETAIL(FUN_0018b660);
    ObjectPlace* AssignObjectPlace(ObjectPlace* place, const ObjectPlace* other) RETAIL(FUN_0018b698);
    // The matrix made again from the rotation (normalized first) once it turned, and from the position once it moved
    void RotateAndTranslate(ObjectPlace* place) RETAIL(RotateAndTranslate);
}

// Which side of a place is newer: the position and the rotation changed, or the matrix moved (the position is its translation's)
// and turned (the rotation is worked out from it again)
union PlaceBits
{
    u64 value;
    struct
    {
        u64 moved : 1;
        u64 turned : 1;
        u64 matrixMoved : 1;
        u64 matrixTurned : 1;
        u64 unused4 : 60;
    };

    // The bits' masks, for what's tested and changed on one read of the word (as retail does: the changes of several at once)
    enum Mask : u64
    {
        Moved = 0x1,
        Turned = 0x2,
        MatrixMoved = 0x4,
        MatrixTurned = 0x8,
    };
};
CHECK_SIZE(PlaceBits, 8);

// An object's place (an instance's, 0x70 bytes): its matrix, its position and its rotation (a quaternion). Either side can change:
// the bits say which one is newer, the position and the rotation being worked out from the matrix when asked once it moved or
// turned, the matrix made again from them (RotateAndTranslate) once they changed
struct alignas(16) ObjectPlace
{
    Matrix4x4 matrix;
    Vector4 position;
    Vector4 rotation;
    PlaceBits bits;

    // The position newer than the matrix (which RotateAndTranslate makes again from it), the rotation newer, and the position
    // or the rotation taken from the matrix (the two in step)
    void MarkMoved()
    {
        bits.value = (bits.value | PlaceBits::Moved) & ~u64{PlaceBits::MatrixMoved};
    }

    void MarkTurned()
    {
        bits.value = (bits.value | PlaceBits::Turned) & ~u64{PlaceBits::MatrixTurned};
    }

    void MarkPositionSynced()
    {
        bits.value &= ~u64{PlaceBits::Moved} & ~u64{PlaceBits::MatrixMoved};
    }

    void MarkRotationSynced()
    {
        bits.value &= ~u64{PlaceBits::Turned} & ~u64{PlaceBits::MatrixTurned};
    }

    // The position taken from the matrix once it moved
    void SyncPosition()
    {
        if ((bits.value & PlaceBits::MatrixMoved) != 0)
        {
            position.x = matrix.m[3][0];
            position.w = matrix.m[3][3];
            position.y = matrix.m[3][1];
            position.z = matrix.m[3][2];
            MarkPositionSynced();
        }
    }

    // The rotation worked out from the matrix once it turned
    void SyncRotation()
    {
        if ((bits.value & PlaceBits::MatrixTurned) != 0)
        {
            GetRotationVec(&rotation, &matrix);
            MarkRotationSynced();
        }
    }

    // Moved to a position (x, y and z compared, all four copied): whether it moved
    bool MoveTo(const Vector4* to)
    {
        if (to->x == position.x && to->y == position.y && to->z == position.z)
        {
            return false;
        }

        MarkMoved();
        position = *to;
        return true;
    }

    // Turned to a rotation: whether it turned
    bool TurnTo(const Vector4* to)
    {
        if (to->x == rotation.x && to->y == rotation.y && to->z == rotation.z && to->w == rotation.w)
        {
            return false;
        }

        MarkTurned();
        rotation.x = to->x;
        rotation.y = to->y;
        rotation.z = to->z;
        rotation.w = to->w;
        return true;
    }

    // Moved by a vector (x, y and z) unless it's within 5e-05 of none on each axis: whether it moved
    bool MoveBy(const Vector4* move)
    {
        if (__builtin_fabsf(move->x) <= Epsilon && __builtin_fabsf(move->y) <= Epsilon && __builtin_fabsf(move->z) <= Epsilon)
        {
            return false;
        }

        SyncPosition();
        MarkMoved();
        position.x = position.x + move->x;
        position.y = position.y + move->y;
        position.z = position.z + move->z;
        return true;
    }
};
CHECK_OFFSET(ObjectPlace, bits, 0x60);
CHECK_SIZE(ObjectPlace, 0x70);
