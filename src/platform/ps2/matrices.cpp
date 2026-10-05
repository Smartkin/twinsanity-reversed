#include "game/collision.h"
#include "game/math.h"

// The maths library's VU0 half (macro mode, its multiply-adds' rounding): retail's sequences as they are, leaving VU0's registers
// (vf1-vf3, vf23-vf31, ACC and I) and the EE's 128 bit temporaries as retail's do

void VuRotateVector(const Matrix4x4* matrix, const Vector4* vector, Vector4* out)
{
    asm volatile("lqc2 $vf27, 0x0(%0)\n\t"
                 "lqc2 $vf28, 0x10(%0)\n\t"
                 "lqc2 $vf29, 0x20(%0)\n\t"
                 "lqc2 $vf31, 0x0(%1)\n\t"
                 "lqc2 $vf26, 0x0(%1)\n\t"
                 "vmulax.xyz $ACC, $vf27, $vf31x\n\t"
                 "vmadday.xyz $ACC, $vf28, $vf31y\n\t"
                 "vmaddz.xyz $vf26, $vf29, $vf31z\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "sqc2 $vf26, 0x0(%2)"
                 :
                 : "r"(matrix), "r"(vector), "r"(out)
                 : "memory");
}

void VuTransformPoint(const Matrix4x4* matrix, const Vector4* point, Vector4* out)
{
    asm volatile("vnop\n\t"
                 "lqc2 $vf27, 0x0(%0)\n\t"
                 "lqc2 $vf31, 0x0(%1)\n\t"
                 "lqc2 $vf28, 0x10(%0)\n\t"
                 "lqc2 $vf29, 0x20(%0)\n\t"
                 "lqc2 $vf30, 0x30(%0)\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf31x\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf31y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf31z\n\t"
                 "vmaddw.xyzw $vf26, $vf30, $vf0w\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "sqc2 $vf26, 0x0(%2)"
                 :
                 : "r"(matrix), "r"(point), "r"(out)
                 : "memory");
}

// The product of two quaternions: a's w times b's vector plus b's w times a's, plus their cross product; the w the product of the
// ws less the dot product
void MultiplyRotations(Vector4* out, const Vector4* a, const Vector4* b)
{
    asm volatile("lqc2 $vf31, 0x0(%1)\n\t"
                 "lqc2 $vf30, 0x0(%2)\n\t"
                 "vmul.xyzw $vf1, $vf31, $vf30\n\t"
                 "vopmula.xyz $ACC, $vf31, $vf30\n\t"
                 "vmaddaw.xyz $ACC, $vf31, $vf30w\n\t"
                 "vmaddaw.xyz $ACC, $vf30, $vf31w\n\t"
                 "vsubaz.w $ACC, $vf1, $vf1z\n\t"
                 "vmsubay.w $ACC, $vf0, $vf1y\n\t"
                 "vopmsub.xyz $vf29, $vf30, $vf31\n\t"
                 "vmsubx.w $vf29, $vf0, $vf1x\n\t"
                 "sqc2 $vf29, 0x0(%0)"
                 :
                 : "r"(out), "r"(a), "r"(b)
                 : "memory");
}

// A triangle's plane: the cross product of its edges from the first vertex made a unit vector, its w less how far along it the
// first vertex is; whether the cross product was longer than 5e-05
u32 PlaneThroughTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third)
{
    alignas(16) f32 length[4];
    asm volatile("lqc2 $vf8, 0x0(%2)\n\t"
                 "lqc2 $vf9, 0x0(%3)\n\t"
                 "lqc2 $vf10, 0x0(%4)\n\t"
                 "vsub.xyzw $vf9, $vf9, $vf8\n\t"
                 "vsub.xyzw $vf10, $vf10, $vf8\n\t"
                 "vopmula.xyz $ACC, $vf9, $vf10\n\t"
                 "vopmsub.xyz $vf10, $vf10, $vf9\n\t"
                 "vmul.xyz $vf11, $vf10, $vf10\n\t"
                 "vaddy.x $vf11, $vf11, $vf11y\n\t"
                 "vaddz.x $vf11, $vf11, $vf11z\n\t"
                 "vsqrt $Q, $vf11x\n\t"
                 "vwaitq\n\t"
                 "vaddq.x $vf11, $vf0, $Q\n\t"
                 "sqc2 $vf11, 0x0(%1)\n\t"
                 "vdiv $Q, $vf0w, $vf11x\n\t"
                 "vwaitq\n\t"
                 "vmulq.xyz $vf10, $vf10, $Q\n\t"
                 "vmul.xyz $vf9, $vf10, $vf8\n\t"
                 "vaddy.x $vf9, $vf9, $vf9y\n\t"
                 "vaddz.x $vf9, $vf9, $vf9z\n\t"
                 "vsub.w $vf10, $vf0, $vf0\n\t"
                 "vsubx.w $vf10, $vf10, $vf9x\n\t"
                 "sqc2 $vf10, 0x0(%0)"
                 :
                 : "r"(plane), "r"(length), "r"(first), "r"(second), "r"(third)
                 : "memory");
    return Epsilon < length[0];
}

void VuTransformByRows(const Matrix4x4* rows, const Vector4* point, Vector4* out)
{
    asm volatile("vnop\n\t"
                 "lqc2 $vf27, 0x0(%0)\n\t"
                 "lqc2 $vf31, 0x0(%1)\n\t"
                 "lqc2 $vf28, 0x10(%0)\n\t"
                 "lqc2 $vf29, 0x20(%0)\n\t"
                 "lqc2 $vf30, 0x30(%0)\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf31x\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf31y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf31z\n\t"
                 "vmaddw.xyzw $vf26, $vf30, $vf31w\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "sqc2 $vf26, 0x0(%2)"
                 :
                 : "r"(rows), "r"(point), "r"(out)
                 : "memory");
}

void InitIdentityMatrix(Matrix4x4* matrix)
{
    // vf0 is (0, 0, 0, 1): its rotation by a word gives the third row, a w of it added to a zero the others
    asm volatile("sqc2 $vf0, 0x30(%0)\n\t"
                 "vsub.xyzw $vf29, $vf29, $vf29\n\t"
                 "vsub.xyzw $vf28, $vf28, $vf28\n\t"
                 "vmr32.xyzw $vf30, $vf0\n\t"
                 "vaddw.y $vf29, $vf0, $vf0w\n\t"
                 "vaddw.x $vf28, $vf0, $vf0w\n\t"
                 "sqc2 $vf30, 0x20(%0)\n\t"
                 "sqc2 $vf29, 0x10(%0)\n\t"
                 "sqc2 $vf28, 0x0(%0)"
                 :
                 : "r"(matrix)
                 : "memory");
}

void VuMultiplyMatrices(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out)
{
    asm volatile("vnop\n\t"
                 "lqc2 $vf27, 0x0(%1)\n\t"
                 "lqc2 $vf23, 0x0(%0)\n\t"
                 "lqc2 $vf28, 0x10(%1)\n\t"
                 "lqc2 $vf29, 0x20(%1)\n\t"
                 "lqc2 $vf30, 0x30(%1)\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf23x\n\t"
                 "lqc2 $vf24, 0x10(%0)\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf23y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf23z\n\t"
                 "vmaddw.xyzw $vf23, $vf30, $vf23w\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf24x\n\t"
                 "lqc2 $vf25, 0x20(%0)\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf24y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf24z\n\t"
                 "vmaddw.xyzw $vf24, $vf30, $vf24w\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf25x\n\t"
                 "lqc2 $vf26, 0x30(%0)\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf25y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf25z\n\t"
                 "vmaddw.xyzw $vf25, $vf30, $vf25w\n\t"
                 "vmulax.xyzw $ACC, $vf27, $vf26x\n\t"
                 "vmadday.xyzw $ACC, $vf28, $vf26y\n\t"
                 "vmaddaz.xyzw $ACC, $vf29, $vf26z\n\t"
                 "vmaddw.xyzw $vf26, $vf30, $vf26w\n\t"
                 "sqc2 $vf23, 0x0(%2)\n\t"
                 "sqc2 $vf24, 0x10(%2)\n\t"
                 "sqc2 $vf25, 0x20(%2)\n\t"
                 "sqc2 $vf26, 0x30(%2)"
                 :
                 : "r"(a), "r"(b), "r"(out)
                 : "memory");
}

void MatrixFromRotation(Matrix4x4* matrix, const Vector4* rotation)
{
    // The rotation scaled by the square root of 2 (in I), so the products of its parts come out doubled; the rows' w 0
    constexpr u32 SquareRootOf2 = 0x3FB504F3;
    asm volatile("vnop\n\t"
                 "ctc2.ni %2, $vi21\n\t"
                 "lqc2 $vf1, 0x0(%1)\n\t"
                 "vmula.xyz $ACC, $vf1, $vf1\n\t"
                 "vmuli.xyzw $vf2, $vf1, $I\n\t"
                 "vmadd.xyz $vf28, $vf1, $vf1\n\t"
                 "vaddw.xyz $vf30, $vf0, $vf0w\n\t"
                 "vopmula.xyz $ACC, $vf2, $vf2\n\t"
                 "vmsubw.xyz $vf1, $vf2, $vf2w\n\t"
                 "vmaddw.xyz $vf2, $vf2, $vf2w\n\t"
                 "vmr32.w $vf28, $vf0\n\t"
                 "vaddaw.xyz $ACC, $vf0, $vf0w\n\t"
                 "vmr32.w $vf29, $vf0\n\t"
                 "vmsubax.yz $ACC, $vf30, $vf28x\n\t"
                 "vmr32.w $vf30, $vf0\n\t"
                 "vmsuby.z $vf30, $vf30, $vf28y\n\t"
                 "vmr32.y $vf3, $vf1\n\t"
                 "vmsubay.x $ACC, $vf30, $vf28y\n\t"
                 "vmr32.w $vf2, $vf2\n\t"
                 "vmsubz.y $vf29, $vf30, $vf28z\n\t"
                 "vmr32.y $vf28, $vf2\n\t"
                 "vmsubz.x $vf28, $vf30, $vf28z\n\t"
                 "vmr32.x $vf30, $vf2\n\t"
                 "vaddy.z $vf28, $vf0, $vf1y\n\t"
                 "vmr32.x $vf29, $vf3\n\t"
                 "vaddx.y $vf30, $vf0, $vf1x\n\t"
                 "vmr32.z $vf29, $vf2\n\t"
                 "vnop\n\t"
                 "sqc2 $vf28, 0x0(%0)\n\t"
                 "sqc2 $vf30, 0x20(%0)\n\t"
                 "sqc2 $vf29, 0x10(%0)"
                 :
                 : "r"(matrix), "r"(rotation), "r"(SquareRootOf2)
                 : "memory");
}

void VuTranspose(const Matrix4x4* matrix, Matrix4x4* out)
{
    asm volatile("lq $8, 0x0(%0)\n\t"
                 "lq $9, 0x10(%0)\n\t"
                 "lq $10, 0x20(%0)\n\t"
                 "lq $11, 0x30(%0)\n\t"
                 "pextlw $12, $9, $8\n\t"
                 "pextuw $13, $9, $8\n\t"
                 "pextlw $14, $11, $10\n\t"
                 "pextuw $15, $11, $10\n\t"
                 "pcpyld $8, $14, $12\n\t"
                 "pcpyud $9, $12, $14\n\t"
                 "pcpyld $10, $15, $13\n\t"
                 "pcpyud $11, $13, $15\n\t"
                 "sq $8, 0x0(%1)\n\t"
                 "sq $9, 0x10(%1)\n\t"
                 "sq $10, 0x20(%1)\n\t"
                 "sq $11, 0x30(%1)"
                 :
                 : "r"(matrix), "r"(out)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "memory");
}

void TransposeQuadwords(void* rows)
{
    asm volatile("lq $8, 0x0(%0)\n\t"
                 "lq $9, 0x10(%0)\n\t"
                 "lq $10, 0x20(%0)\n\t"
                 "lq $11, 0x30(%0)\n\t"
                 "pextlw $12, $9, $8\n\t"
                 "pextuw $13, $9, $8\n\t"
                 "pextlw $14, $11, $10\n\t"
                 "pextuw $15, $11, $10\n\t"
                 "pcpyld $8, $14, $12\n\t"
                 "pcpyud $9, $12, $14\n\t"
                 "pcpyld $10, $15, $13\n\t"
                 "pcpyud $11, $13, $15\n\t"
                 "sq $8, 0x0(%0)\n\t"
                 "sq $9, 0x10(%0)\n\t"
                 "sq $10, 0x20(%0)\n\t"
                 "sq $11, 0x30(%0)"
                 :
                 : "r"(rows)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "memory");
}

void VuTransposeRotation(const Matrix4x4* matrix, Matrix4x4* out)
{
    // The fourth row taken as vf0's (0, 0, 0, 1)
    asm volatile("lq $8, 0x0(%0)\n\t"
                 "lq $9, 0x10(%0)\n\t"
                 "lq $10, 0x20(%0)\n\t"
                 "qmfc2.ni $11, $vf0\n\t"
                 "pextlw $12, $9, $8\n\t"
                 "pextuw $13, $9, $8\n\t"
                 "pextlw $14, $11, $10\n\t"
                 "pextuw $15, $11, $10\n\t"
                 "pcpyld $8, $14, $12\n\t"
                 "pcpyud $9, $12, $14\n\t"
                 "pcpyld $10, $15, $13\n\t"
                 "pcpyud $11, $13, $15\n\t"
                 "sq $8, 0x0(%1)\n\t"
                 "sq $9, 0x10(%1)\n\t"
                 "sq $10, 0x20(%1)\n\t"
                 "sqc2 $vf0, 0x30(%1)"
                 :
                 : "r"(matrix), "r"(out)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "memory");
}

// The inverse of a rotation and a translation: the rotation transposed, the translation turned by it and negated
void VuInvertRigid(Matrix4x4* out, const Matrix4x4* matrix)
{
    asm volatile("vnop\n\t"
                 "vsubw.x $vf28, $vf0, $vf0w\n\t"
                 "lq $8, 0x0(%1)\n\t"
                 "lq $9, 0x10(%1)\n\t"
                 "lq $10, 0x20(%1)\n\t"
                 "lq $11, 0x30(%1)\n\t"
                 "qmtc2.ni $11, $vf31\n\t"
                 "vmulx.xyz $vf31, $vf31, $vf28x\n\t"
                 "pextlw $12, $9, $8\n\t"
                 "pextuw $13, $9, $8\n\t"
                 "pextlw $14, $11, $10\n\t"
                 "pextuw $15, $11, $10\n\t"
                 "pcpyld $8, $14, $12\n\t"
                 "pcpyud $9, $12, $14\n\t"
                 "pcpyld $10, $15, $13\n\t"
                 "vmove.w $vf31, $vf0\n\t"
                 "qmtc2.ni $8, $vf28\n\t"
                 "qmtc2.ni $9, $vf29\n\t"
                 "qmtc2.ni $10, $vf30\n\t"
                 "vmulax.xyz $ACC, $vf28, $vf31x\n\t"
                 "vmadday.xyz $ACC, $vf29, $vf31y\n\t"
                 "vmaddz.xyz $vf31, $vf30, $vf31z\n\t"
                 "vsub.w $vf28, $vf28, $vf28\n\t"
                 "vsub.w $vf29, $vf29, $vf29\n\t"
                 "vsub.w $vf30, $vf30, $vf30\n\t"
                 "sqc2 $vf31, 0x30(%0)\n\t"
                 "sqc2 $vf28, 0x0(%0)\n\t"
                 "sqc2 $vf29, 0x10(%0)\n\t"
                 "sqc2 $vf30, 0x20(%0)"
                 :
                 : "r"(out), "r"(matrix)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "memory");
}

void VuInvertRigidInPlace(Matrix4x4* matrix)
{
    VuInvertRigid(matrix, matrix);
}
