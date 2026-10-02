#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"

struct ObjectPlace;

extern "C"
{
    // The rotation a matrix turns by, and a place turned by a rotation unless it's (within 5e-05) none: whether it turned (still
    // asm)
    void GetRotationVec(Vector4* rotation, const Matrix4x4* matrix) RETAIL(GetRotationVec_);
    u32 TurnPlace(ObjectPlace* place, const Vector4* rotation) RETAIL(Rotate);
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

// An object's place (an instance's, 0x70 bytes): its matrix, its position and its rotation (a quaternion). Either side can change:
// the bits say which one is newer, the position and the rotation being worked out from the matrix when asked once it moved or
// turned, the matrix made again from them (RotateAndTranslate) once they changed
struct alignas(16) ObjectPlace
{
    enum Bits : u64
    {
        // The position and the rotation changed; the matrix moved (the position is its translation's) and turned (the rotation
        // is worked out from it again)
        BitMoved = 0x1,
        BitTurned = 0x2,
        BitMatrixMoved = 0x4,
        BitMatrixTurned = 0x8,
    };

    Matrix4x4 matrix;
    Vector4 position;
    Vector4 rotation;
    u64 bits;

    // The position taken from the matrix once it moved
    void SyncPosition()
    {
        if ((bits & BitMatrixMoved) != 0)
        {
            position.x = matrix.m[3][0];
            position.w = matrix.m[3][3];
            position.y = matrix.m[3][1];
            position.z = matrix.m[3][2];
            bits &= ~u64{BitMoved} & ~u64{BitMatrixMoved};
        }
    }

    // The rotation worked out from the matrix once it turned
    void SyncRotation()
    {
        if ((bits & BitMatrixTurned) != 0)
        {
            GetRotationVec(&rotation, &matrix);
            bits &= ~u64{BitTurned} & ~u64{BitMatrixTurned};
        }
    }

    // Moved to a position (x, y and z compared, all four copied): whether it moved
    bool MoveTo(const Vector4* to)
    {
        if (to->x == position.x && to->y == position.y && to->z == position.z)
        {
            return false;
        }

        bits = (bits | BitMoved) & ~u64{BitMatrixMoved};
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

        bits = (bits | BitTurned) & ~u64{BitMatrixTurned};
        rotation.x = to->x;
        rotation.y = to->y;
        rotation.z = to->z;
        rotation.w = to->w;
        return true;
    }
};
CHECK_OFFSET(ObjectPlace, bits, 0x60);
CHECK_SIZE(ObjectPlace, 0x70);
