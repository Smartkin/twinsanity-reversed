#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"

// The boxes the game works out on VU0 in macro mode (the retail functions are VU0 code): VU0 keeps the smallest and the largest x,
// y and z in two registers, and points moved by a matrix go through its multiply-adds, whose rounding the FPU's doesn't give

namespace
{
// The points' box so far: the first point in both registers
void StartBox(const Vector4* point)
{
    asm volatile("lqc2 $vf1, 0x0(%0)\n\t"
                 "lqc2 $vf2, 0x0(%0)"
                 :
                 : "r"(point)
                 : "memory");
}

void TakeIntoBox(const Vector4* point)
{
    asm volatile("lqc2 $vf3, 0x0(%0)\n\t"
                 "vmini.xyz $vf1, $vf1, $vf3\n\t"
                 "vmax.xyz $vf2, $vf2, $vf3"
                 :
                 : "r"(point)
                 : "memory");
}

void StoreBox(Box* box)
{
    asm volatile("sqc2 $vf1, 0x0(%0)\n\t"
                 "sqc2 $vf2, 0x10(%0)"
                 :
                 : "r"(box)
                 : "memory");
}
}

void GetBbox(const Vector4* points, s32 count, Box* box, const Matrix4x4* matrix)
{
    // The matrix's rows in vf04-vf07, the first point moved into both registers
    asm volatile("lqc2 $vf4, 0x0(%1)\n\t"
                 "lqc2 $vf5, 0x10(%1)\n\t"
                 "lqc2 $vf6, 0x20(%1)\n\t"
                 "lqc2 $vf7, 0x30(%1)\n\t"
                 "lqc2 $vf1, 0x0(%0)\n\t"
                 "lqc2 $vf2, 0x0(%0)\n\t"
                 "vmulax.xyzw $ACC, $vf4, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf5, $vf1y\n\t"
                 "vmaddaz.xyzw $ACC, $vf6, $vf1z\n\t"
                 "vmaddw.xyzw $vf1, $vf7, $vf1w\n\t"
                 "vmulax.xyzw $ACC, $vf4, $vf2x\n\t"
                 "vmadday.xyzw $ACC, $vf5, $vf2y\n\t"
                 "vmaddaz.xyzw $ACC, $vf6, $vf2z\n\t"
                 "vmaddw.xyzw $vf2, $vf7, $vf2w"
                 :
                 : "r"(points), "r"(matrix)
                 : "memory");
    for (s32 index = 1; index < count; index++)
    {
        asm volatile("lqc2 $vf3, 0x0(%0)\n\t"
                     "vmulax.xyzw $ACC, $vf4, $vf3x\n\t"
                     "vmadday.xyzw $ACC, $vf5, $vf3y\n\t"
                     "vmaddaz.xyzw $ACC, $vf6, $vf3z\n\t"
                     "vmaddw.xyzw $vf3, $vf7, $vf3w\n\t"
                     "vmini.xyz $vf1, $vf1, $vf3\n\t"
                     "vmax.xyz $vf2, $vf2, $vf3"
                     :
                     : "r"(&points[index])
                     : "memory");
    }

    StoreBox(box);
}

void PointsBox(const Vector4* points, s32 count, Box* box)
{
    StartBox(&points[0]);
    for (s32 index = 1; index < count; index++)
    {
        TakeIntoBox(&points[index]);
    }

    StoreBox(box);
}

void TriangleBounds(Box* box, const Vector4* first, const Vector4* second, const Vector4* third)
{
    // The corners' w stay what vf11 and vf12 had
    asm volatile("lqc2 $vf8, 0x0(%1)\n\t"
                 "lqc2 $vf9, 0x0(%2)\n\t"
                 "lqc2 $vf10, 0x0(%3)\n\t"
                 "vmini.xyz $vf11, $vf8, $vf9\n\t"
                 "vmax.xyz $vf12, $vf8, $vf9\n\t"
                 "vmini.xyz $vf11, $vf11, $vf10\n\t"
                 "vmax.xyz $vf12, $vf12, $vf10\n\t"
                 "sqc2 $vf11, 0x0(%0)\n\t"
                 "sqc2 $vf12, 0x10(%0)"
                 :
                 : "r"(box), "r"(first), "r"(second), "r"(third)
                 : "memory");
}
