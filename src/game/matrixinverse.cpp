#include "game/math.h"

// A matrix's inverse through its cofactors, and the minors they're made of

namespace
{
constexpr u32 MatrixElements = 16;

// An element's index: its row times 4 plus its column
constexpr u32 ElementOf(u32 row, u32 column)
{
    return row * 4 + column;
}
}

f32 MatrixMinor(const Matrix4x4* matrix, u32 element)
{
    if (element >= MatrixElements)
    {
        return 0.0f;
    }

    // What's left without the element's row and column, its rows as vectors (w 1): its determinant is their triple product
    u32 row = element / 4;
    u32 column = element % 4;
    u32 columns[3];
    u32 kept = 0;
    for (u32 index = 0; index < 4; index++)
    {
        if (index != column)
        {
            columns[kept++] = index;
        }
    }

    Vector4 rows[3];
    kept = 0;
    for (u32 index = 0; index < 4; index++)
    {
        if (index == row)
        {
            continue;
        }

        rows[kept].x = matrix->m[index][columns[0]];
        rows[kept].y = matrix->m[index][columns[1]];
        rows[kept].z = matrix->m[index][columns[2]];
        rows[kept].w = 1.0f;
        kept++;
    }

    return TripleProduct(&rows[0], &rows[1], &rows[2], nullptr);
}

void InvertMatrix(const Matrix4x4* matrix, s32 size, Matrix4x4* out)
{
    // The determinant along the last column (the rotation's alone for size 3): its elements' minors by row
    f32 minor33 = MatrixMinor(matrix, ElementOf(3, 3));
    f32 determinant = minor33;
    f32 minor03 = 0.0f;
    f32 minor13 = 0.0f;
    f32 minor23 = 0.0f;
    if (size != 3)
    {
        minor03 = MatrixMinor(matrix, ElementOf(0, 3));
        minor13 = MatrixMinor(matrix, ElementOf(1, 3));
        minor23 = MatrixMinor(matrix, ElementOf(2, 3));
        determinant =
            matrix->m[0][3] * -minor03 + matrix->m[1][3] * minor13 + matrix->m[2][3] * -minor23 + matrix->m[3][3] * minor33;
    }

    if (determinant == 0.0f)
    {
        return;
    }

    // The cofactors transposed over the determinant: each row's elements set to its inverse and then multiplied by their
    // cofactors one at a time (retail works in out as it goes, so a matrix inverted into itself comes out wrong)
    f32 inverse = 1.0f / determinant;
    for (u32 row = 0; row < 3; row++)
    {
        out->m[row][0] = inverse;
        out->m[row][1] = inverse;
        out->m[row][2] = inverse;
        for (u32 column = 0; column < 3; column++)
        {
            f32 minor = MatrixMinor(matrix, ElementOf(column, row));
            out->m[row][column] = out->m[row][column] * ((row + column) % 2 != 0 ? -minor : minor);
        }
    }

    if (size == 3)
    {
        out->m[0][3] = 0.0f;
        out->m[1][3] = 0.0f;
        out->m[2][3] = 0.0f;
        out->m[3][0] = 0.0f;
        out->m[3][1] = 0.0f;
        out->m[3][2] = 0.0f;
        out->m[3][3] = 1.0f;
        return;
    }

    for (u32 row = 0; row < 3; row++)
    {
        out->m[row][3] = inverse;
    }

    for (u32 row = 0; row < 3; row++)
    {
        f32 minor = MatrixMinor(matrix, ElementOf(3, row));
        out->m[row][3] = out->m[row][3] * (row % 2 == 0 ? -minor : minor);
    }

    out->m[3][0] = inverse * -minor03;
    out->m[3][1] = inverse * minor13;
    out->m[3][2] = inverse * -minor23;
    out->m[3][3] = inverse * minor33;
}
