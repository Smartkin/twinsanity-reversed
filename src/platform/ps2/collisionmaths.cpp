#include "game/collision.h"
#include "game/physics.h"

// The collision's and the physics probes' VU0 half (macro mode): retail's sequences as they are, handing their results on in VU0's
// registers where the retail ones do (the edge tests' second half and the support ranges take what the first left there) and
// leaving the EE's 128 bit temporaries as retail's do

// The ray's ends against the triangle's plane (its normal the edges' cross product), into the first two values; the six edge
// tests' cross products and differences left in vf16-vf18, vf21-vf23 and vf26-vf28 for FinishTriangleEdgeTests
void StartTriangleEdgeTests(const Vector4* start, const Vector4* end, const CollisionHit* triangle, f32* values)
{
    asm volatile("lqc2 $vf4, 0x0(%0)\n\t"
                 "lqc2 $vf5, 0x0(%1)\n\t"
                 "lqc2 $vf1, 0x0(%2)\n\t"
                 "lqc2 $vf2, 0x10(%2)\n\t"
                 "lqc2 $vf3, 0x20(%2)\n\t"
                 "vsub.xyz $vf7, $vf5, $vf4\n\t"
                 "vsub.xyz $vf10, $vf2, $vf1\n\t"
                 "vsub.xyz $vf11, $vf3, $vf1\n\t"
                 "vsub.xyz $vf15, $vf2, $vf1\n\t"
                 "vsub.xyz $vf20, $vf3, $vf2\n\t"
                 "vsub.xyz $vf25, $vf1, $vf3\n\t"
                 "vopmula.xyz $ACC, $vf10, $vf11\n\t"
                 "vopmsub.xyz $vf11, $vf11, $vf10\n\t"
                 "vopmula.xyz $ACC, $vf7, $vf15\n\t"
                 "vopmsub.xyz $vf16, $vf15, $vf7\n\t"
                 "vsub.xyz $vf12, $vf4, $vf1\n\t"
                 "vsub.xyz $vf13, $vf5, $vf1\n\t"
                 "vopmula.xyz $ACC, $vf7, $vf20\n\t"
                 "vopmsub.xyz $vf21, $vf20, $vf7\n\t"
                 "vmul.xyz $vf12, $vf12, $vf11\n\t"
                 "vmul.xyz $vf13, $vf13, $vf11\n\t"
                 "vopmula.xyz $ACC, $vf7, $vf25\n\t"
                 "vopmsub.xyz $vf26, $vf25, $vf7\n\t"
                 "vaddy.x $vf12, $vf12, $vf12y\n\t"
                 "vaddy.x $vf13, $vf13, $vf13y\n\t"
                 "vsub.xyz $vf17, $vf1, $vf4\n\t"
                 "vsub.xyz $vf18, $vf3, $vf4\n\t"
                 "vaddz.x $vf12, $vf12, $vf12z\n\t"
                 "vaddz.x $vf13, $vf13, $vf13z\n\t"
                 "vsub.xyz $vf22, $vf2, $vf4\n\t"
                 "vsub.xyz $vf23, $vf1, $vf4\n\t"
                 "vsub.xyz $vf27, $vf3, $vf4\n\t"
                 "qmfc2.ni $8, $vf12\n\t"
                 "vsub.xyz $vf28, $vf2, $vf4\n\t"
                 "qmfc2.ni $9, $vf13\n\t"
                 "vmul.xyz $vf17, $vf17, $vf16\n\t"
                 "sw $8, 0x0(%3)\n\t"
                 "vmul.xyz $vf18, $vf18, $vf16\n\t"
                 "sw $9, 0x4(%3)"
                 :
                 : "r"(start), "r"(end), "r"(triangle), "r"(values)
                 : "$8", "$9", "memory");
}

// The edge tests' dot products: each edge's sides of the ray's two ends, into the six values
void FinishTriangleEdgeTests(f32* values)
{
    asm volatile("vmul.xyz $vf22, $vf22, $vf21\n\t"
                 "vmul.xyz $vf23, $vf23, $vf21\n\t"
                 "vmul.xyz $vf27, $vf27, $vf26\n\t"
                 "vmul.xyz $vf28, $vf28, $vf26\n\t"
                 "vaddy.x $vf17, $vf17, $vf17y\n\t"
                 "vaddy.x $vf18, $vf18, $vf18y\n\t"
                 "vaddy.x $vf22, $vf22, $vf22y\n\t"
                 "vaddy.x $vf23, $vf23, $vf23y\n\t"
                 "vaddy.x $vf27, $vf27, $vf27y\n\t"
                 "vaddy.x $vf28, $vf28, $vf28y\n\t"
                 "vaddz.x $vf17, $vf17, $vf17z\n\t"
                 "vaddz.x $vf18, $vf18, $vf18z\n\t"
                 "vaddz.x $vf22, $vf22, $vf22z\n\t"
                 "vaddz.x $vf23, $vf23, $vf23z\n\t"
                 "vaddz.x $vf27, $vf27, $vf27z\n\t"
                 "vaddz.x $vf28, $vf28, $vf28z\n\t"
                 "qmfc2.ni $8, $vf17\n\t"
                 "qmfc2.ni $9, $vf18\n\t"
                 "qmfc2.ni $10, $vf22\n\t"
                 "qmfc2.ni $11, $vf23\n\t"
                 "qmfc2.ni $12, $vf27\n\t"
                 "qmfc2.ni $13, $vf28\n\t"
                 "sw $8, 0x0(%0)\n\t"
                 "sw $9, 0x4(%0)\n\t"
                 "sw $10, 0x8(%0)\n\t"
                 "sw $11, 0xC(%0)\n\t"
                 "sw $12, 0x10(%0)\n\t"
                 "sw $13, 0x14(%0)"
                 :
                 : "r"(values)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "memory");
}

// Every vertex of both hulls through its matrix into moved (the first's, then the other's), then every vertex of the first less
// every vertex of the other into differences (the w of each the first matrix's third row's: vf3's left from it). The vertexes are
// read one past each list's end. Counts of 0 run on until the count wraps around
void MakeVertexDifferences(const VertexDifferences* request)
{
    asm volatile(".set push\n\t"
                 ".set noreorder\n\t"
                 "lw $8, 0x0(%0)\n\t"
                 "lw $9, 0x4(%0)\n\t"
                 "lw $10, 0x8(%0)\n\t"
                 "lw $11, 0xC(%0)\n\t"
                 "lw $12, 0x10(%0)\n\t"
                 "lw $13, 0x14(%0)\n\t"
                 "lw $14, 0x18(%0)\n\t"
                 "lw $15, 0x1C(%0)\n\t"
                 "lqc2 $vf1, 0x0($8)\n\t"
                 "lqc2 $vf2, 0x10($8)\n\t"
                 "lqc2 $vf3, 0x20($8)\n\t"
                 "lqc2 $vf4, 0x30($8)\n\t"
                 "lqc2 $vf5, 0x0($11)\n\t"
                 "lqc2 $vf6, 0x10($11)\n\t"
                 "lqc2 $vf7, 0x20($11)\n\t"
                 "lqc2 $vf8, 0x30($11)\n\t"
                 "lqc2 $vf10, 0x0($9)\n"
                 "1:\n\t"
                 "vmulax.xyzw $ACC, $vf1, $vf10x\n\t"
                 "vmadday.xyzw $ACC, $vf2, $vf10y\n\t"
                 "vmaddaz.xyzw $ACC, $vf3, $vf10z\n\t"
                 "vmaddw.xyzw $vf11, $vf4, $vf10w\n\t"
                 "addi $10, $10, -0x1\n\t"
                 "addi $15, $15, 0x10\n\t"
                 "lqc2 $vf10, 0x10($9)\n\t"
                 "sqc2 $vf11, -0x10($15)\n\t"
                 "bnez $10, 1b\n\t"
                 "addi $9, $9, 0x10\n\t"
                 "lqc2 $vf10, 0x0($12)\n\t"
                 "daddu $8, $15, $0\n"
                 "2:\n\t"
                 "vmulax.xyzw $ACC, $vf5, $vf10x\n\t"
                 "vmadday.xyzw $ACC, $vf6, $vf10y\n\t"
                 "vmaddaz.xyzw $ACC, $vf7, $vf10z\n\t"
                 "vmaddw.xyzw $vf11, $vf8, $vf10w\n\t"
                 "addi $13, $13, -0x1\n\t"
                 "addi $15, $15, 0x10\n\t"
                 "lqc2 $vf10, 0x10($12)\n\t"
                 "sqc2 $vf11, -0x10($15)\n\t"
                 "bnez $13, 2b\n\t"
                 "addi $12, $12, 0x10\n\t"
                 "lw $9, 0x1C(%0)\n\t"
                 "lw $10, 0x8(%0)\n"
                 "3:\n\t"
                 "lqc2 $vf1, 0x0($9)\n\t"
                 "daddu $12, $8, $0\n\t"
                 "lw $13, 0x14(%0)\n"
                 "4:\n\t"
                 "lqc2 $vf2, 0x0($12)\n\t"
                 "vsub.xyz $vf3, $vf1, $vf2\n\t"
                 "addi $13, $13, -0x1\n\t"
                 "addi $12, $12, 0x10\n\t"
                 "sqc2 $vf3, 0x0($14)\n\t"
                 "bnez $13, 4b\n\t"
                 "addi $14, $14, 0x10\n\t"
                 "addi $10, $10, -0x1\n\t"
                 "addi $9, $9, 0x10\n\t"
                 "bnez $10, 3b\n\t"
                 "nop\n\t"
                 ".set pop"
                 :
                 : "r"(request)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "memory");
}

// Groups of four points (x, y, z, w each) made their xs, ys and zs (three quadwords), in place
void GroupPoints(Vector4* points, s32 groups)
{
    asm volatile(".set push\n\t"
                 ".set noreorder\n\t"
                 "daddu $8, %0, $0\n\t"
                 "daddu $9, %1, $0\n\t"
                 "daddu $25, %0, $0\n"
                 "1:\n\t"
                 "lq $10, 0x0($8)\n\t"
                 "lq $11, 0x10($8)\n\t"
                 "lq $12, 0x20($8)\n\t"
                 "lq $13, 0x30($8)\n\t"
                 "pextlw $14, $11, $10\n\t"
                 "pextuw $15, $11, $10\n\t"
                 "pextlw $11, $13, $12\n\t"
                 "pextuw $24, $13, $12\n\t"
                 "pcpyld $10, $11, $14\n\t"
                 "pcpyud $11, $14, $11\n\t"
                 "pcpyld $12, $24, $15\n\t"
                 "sq $10, 0x0($25)\n\t"
                 "sq $11, 0x10($25)\n\t"
                 "sq $12, 0x20($25)\n\t"
                 "addi $9, $9, -0x1\n\t"
                 "addi $8, $8, 0x40\n\t"
                 "bnez $9, 1b\n\t"
                 "addi $25, $25, 0x30\n\t"
                 ".set pop"
                 :
                 : "r"(points), "r"(groups)
                 : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "memory");
}

// The least and the most of the grouped points' dot products with the axis (from 2^65 and -2^65), into the range. The groups are
// read one past the last
void GroupsRange(const Vector4* groups, s32 count, const Vector4* axis, f32* range)
{
    asm volatile(".set push\n\t"
                 ".set noreorder\n\t"
                 "daddu $8, %0, $0\n\t"
                 "daddu $9, %1, $0\n\t"
                 "lui $10, 0x6000\n\t"
                 "qmtc2.ni $10, $vf6\n\t"
                 "vsub.yzw $vf6, $vf6, $vf6\n\t"
                 "vaddx.yzw $vf6, $vf6, $vf6x\n\t"
                 "vsub.xyzw $vf7, $vf7, $vf7\n\t"
                 "vsub.xyzw $vf7, $vf7, $vf6\n\t"
                 "lqc2 $vf1, 0x0(%2)\n\t"
                 "lqc2 $vf2, 0x0($8)\n\t"
                 "lqc2 $vf3, 0x10($8)\n\t"
                 "lqc2 $vf4, 0x20($8)\n"
                 "1:\n\t"
                 "vmulax.xyzw $ACC, $vf2, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf3, $vf1y\n\t"
                 "vmaddz.xyzw $vf5, $vf4, $vf1z\n\t"
                 "addi $9, $9, -0x1\n\t"
                 "addi $8, $8, 0x30\n\t"
                 "lqc2 $vf2, 0x0($8)\n\t"
                 "lqc2 $vf3, 0x10($8)\n\t"
                 "lqc2 $vf4, 0x20($8)\n\t"
                 "vmini.xyzw $vf6, $vf6, $vf5\n\t"
                 "bnez $9, 1b\n\t"
                 "vmax.xyzw $vf7, $vf7, $vf5\n\t"
                 "vminiy.x $vf8, $vf6, $vf6y\n\t"
                 "vminiw.z $vf9, $vf6, $vf6w\n\t"
                 "vminiz.x $vf10, $vf8, $vf9z\n\t"
                 "qmfc2.ni $8, $vf10\n\t"
                 "vmaxy.x $vf11, $vf7, $vf7y\n\t"
                 "vmaxw.z $vf12, $vf7, $vf7w\n\t"
                 "vmaxz.x $vf13, $vf11, $vf12z\n\t"
                 "qmfc2.ni $9, $vf13\n\t"
                 "sw $8, 0x0(%3)\n\t"
                 "sw $9, 0x4(%3)\n\t"
                 ".set pop"
                 :
                 : "r"(groups), "r"(count), "r"(axis), "r"(range)
                 : "$8", "$9", "$10", "memory");
}

// The same of 16 groups (64 points), two at a time
void SixteenGroupsRange(const Vector4* groups, const Vector4* axis, f32* range)
{
    asm volatile(".set push\n\t"
                 ".set noreorder\n\t"
                 "lui $10, 0x6000\n\t"
                 "qmtc2.ni $10, $vf30\n\t"
                 "vsub.yzw $vf30, $vf30, $vf30\n\t"
                 "vaddx.yzw $vf30, $vf30, $vf30x\n\t"
                 "vsub.xyzw $vf31, $vf31, $vf31\n\t"
                 "vsub.xyzw $vf31, $vf31, $vf30\n\t"
                 "lqc2 $vf1, 0x0(%1)\n\t"
                 "daddu $8, %0, $0\n\t"
                 "addiu $9, $0, 0x8\n\t"
                 "lqc2 $vf2, 0x0($8)\n\t"
                 "lqc2 $vf3, 0x10($8)\n\t"
                 "lqc2 $vf4, 0x20($8)\n\t"
                 "lqc2 $vf5, 0x30($8)\n\t"
                 "lqc2 $vf6, 0x40($8)\n\t"
                 "lqc2 $vf7, 0x50($8)\n"
                 "1:\n\t"
                 "vmulax.xyzw $ACC, $vf2, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf3, $vf1y\n\t"
                 "vmaddz.xyzw $vf28, $vf4, $vf1z\n\t"
                 "vmulax.xyzw $ACC, $vf5, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf6, $vf1y\n\t"
                 "vmaddz.xyzw $vf29, $vf7, $vf1z\n\t"
                 "addi $8, $8, 0x60\n\t"
                 "vmini.xyzw $vf30, $vf30, $vf28\n\t"
                 "addi $9, $9, -0x1\n\t"
                 "vmax.xyzw $vf31, $vf31, $vf28\n\t"
                 "lqc2 $vf2, 0x0($8)\n\t"
                 "lqc2 $vf3, 0x10($8)\n\t"
                 "lqc2 $vf4, 0x20($8)\n\t"
                 "vmini.xyzw $vf30, $vf30, $vf29\n\t"
                 "lqc2 $vf5, 0x30($8)\n\t"
                 "lqc2 $vf6, 0x40($8)\n\t"
                 "lqc2 $vf7, 0x50($8)\n\t"
                 "bnez $9, 1b\n\t"
                 "vmax.xyzw $vf31, $vf31, $vf29\n\t"
                 "vminiy.x $vf8, $vf30, $vf30y\n\t"
                 "vminiw.z $vf9, $vf30, $vf30w\n\t"
                 "vminiz.x $vf10, $vf8, $vf9z\n\t"
                 "qmfc2.ni $8, $vf10\n\t"
                 "vmaxy.x $vf11, $vf31, $vf31y\n\t"
                 "vmaxw.z $vf12, $vf31, $vf31w\n\t"
                 "vmaxz.x $vf13, $vf11, $vf12z\n\t"
                 "qmfc2.ni $9, $vf13\n\t"
                 "sw $8, 0x0(%2)\n\t"
                 "sw $9, 0x4(%2)\n\t"
                 ".set pop"
                 :
                 : "r"(groups), "r"(axis), "r"(range)
                 : "$8", "$9", "$10", "memory");
}

// The triangle's three vertexes less the box hull's eight (24 differences) grouped four at a time into vf2-vf19 for
// TriangleHullSupportRange (the last group's first difference made in vf1)
void LoadTriangleHullSupport(const CollisionHit* triangle, const Vector4* hullVertices)
{
    // One group: four differences in vf16-vf19 (the first in the register given) made the xs, ys and zs of three registers
#define GROUP(first, xs, ys, zs)                                                                                                   \
    "qmfc2.ni $10, " first "\n\t"                                                                                                 \
    "qmfc2.ni $11, $vf17\n\t"                                                                                                     \
    "qmfc2.ni $12, $vf18\n\t"                                                                                                     \
    "qmfc2.ni $13, $vf19\n\t"                                                                                                     \
    "pextlw $14, $11, $10\n\t"                                                                                                    \
    "pextuw $15, $11, $10\n\t"                                                                                                    \
    "pextlw $11, $13, $12\n\t"                                                                                                    \
    "pextuw $24, $13, $12\n\t"                                                                                                    \
    "pcpyld $10, $11, $14\n\t"                                                                                                    \
    "pcpyud $11, $14, $11\n\t"                                                                                                    \
    "pcpyld $12, $24, $15\n\t"                                                                                                    \
    "qmtc2.ni $10, " xs "\n\t"                                                                                                    \
    "qmtc2.ni $11, " ys "\n\t"                                                                                                    \
    "qmtc2.ni $12, " zs "\n\t"
    asm volatile("lqc2 $vf20, 0x0(%0)\n\t"
                 "lqc2 $vf21, 0x10(%0)\n\t"
                 "lqc2 $vf22, 0x20(%0)\n\t"
                 "lqc2 $vf23, 0x0(%1)\n\t"
                 "lqc2 $vf24, 0x10(%1)\n\t"
                 "lqc2 $vf25, 0x20(%1)\n\t"
                 "lqc2 $vf26, 0x30(%1)\n\t"
                 "lqc2 $vf27, 0x40(%1)\n\t"
                 "lqc2 $vf28, 0x50(%1)\n\t"
                 "lqc2 $vf29, 0x60(%1)\n\t"
                 "lqc2 $vf30, 0x70(%1)\n\t"
                 "vsub.xyz $vf16, $vf20, $vf23\n\t"
                 "vsub.xyz $vf17, $vf20, $vf24\n\t"
                 "vsub.xyz $vf18, $vf20, $vf25\n\t"
                 "vsub.xyz $vf19, $vf20, $vf26\n\t"
                 GROUP("$vf16", "$vf2", "$vf3", "$vf4")
                 "vsub.xyz $vf16, $vf20, $vf27\n\t"
                 "vsub.xyz $vf17, $vf20, $vf28\n\t"
                 "vsub.xyz $vf18, $vf20, $vf29\n\t"
                 "vsub.xyz $vf19, $vf20, $vf30\n\t"
                 GROUP("$vf16", "$vf5", "$vf6", "$vf7")
                 "vsub.xyz $vf16, $vf21, $vf23\n\t"
                 "vsub.xyz $vf17, $vf21, $vf24\n\t"
                 "vsub.xyz $vf18, $vf21, $vf25\n\t"
                 "vsub.xyz $vf19, $vf21, $vf26\n\t"
                 GROUP("$vf16", "$vf8", "$vf9", "$vf10")
                 "vsub.xyz $vf16, $vf21, $vf27\n\t"
                 "vsub.xyz $vf17, $vf21, $vf28\n\t"
                 "vsub.xyz $vf18, $vf21, $vf29\n\t"
                 "vsub.xyz $vf19, $vf21, $vf30\n\t"
                 GROUP("$vf16", "$vf11", "$vf12", "$vf13")
                 "vsub.xyz $vf16, $vf22, $vf23\n\t"
                 "vsub.xyz $vf17, $vf22, $vf24\n\t"
                 "vsub.xyz $vf18, $vf22, $vf25\n\t"
                 "vsub.xyz $vf19, $vf22, $vf26\n\t"
                 GROUP("$vf16", "$vf14", "$vf15", "$vf16")
                 "vsub.xyz $vf1, $vf22, $vf27\n\t"
                 "vsub.xyz $vf17, $vf22, $vf28\n\t"
                 "vsub.xyz $vf18, $vf22, $vf29\n\t"
                 "vsub.xyz $vf19, $vf22, $vf30\n\t"
                 GROUP("$vf1", "$vf17", "$vf18", "$vf19")
                 :
                 : "r"(triangle), "r"(hullVertices)
                 : "$10", "$11", "$12", "$13", "$14", "$15", "$24", "memory");
#undef GROUP
}

// The least and the most of the 24 differences' dot products with the axis, into the range
void TriangleHullSupportRange(const Vector4* axis, f32* range)
{
    asm volatile("lqc2 $vf1, 0x0(%0)\n\t"
                 "vmulax.xyzw $ACC, $vf2, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf3, $vf1y\n\t"
                 "vmaddz.xyzw $vf31, $vf4, $vf1z\n\t"
                 "vmulax.xyzw $ACC, $vf5, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf6, $vf1y\n\t"
                 "vmaddz.xyzw $vf20, $vf7, $vf1z\n\t"
                 "vmulax.xyzw $ACC, $vf8, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf9, $vf1y\n\t"
                 "vmaddz.xyzw $vf30, $vf10, $vf1z\n\t"
                 "vmini.xyzw $vf21, $vf31, $vf20\n\t"
                 "vmax.xyzw $vf20, $vf31, $vf20\n\t"
                 "vmulax.xyzw $ACC, $vf11, $vf1x\n\t"
                 "vmadday.xyzw $ACC, $vf12, $vf1y\n\t"
                 "vmini.xyzw $vf22, $vf21, $vf30\n\t"
                 "vmaddz.xyzw $vf31, $vf13, $vf1z\n\t"
                 "vmulax.xyzw $ACC, $vf14, $vf1x\n\t"
                 "vmax.xyzw $vf21, $vf20, $vf30\n\t"
                 "vmadday.xyzw $ACC, $vf15, $vf1y\n\t"
                 "vmini.xyzw $vf20, $vf22, $vf31\n\t"
                 "vmaddz.xyzw $vf30, $vf16, $vf1z\n\t"
                 "vmulax.xyzw $ACC, $vf17, $vf1x\n\t"
                 "vmax.xyzw $vf21, $vf21, $vf31\n\t"
                 "vmadday.xyzw $ACC, $vf18, $vf1y\n\t"
                 "vmini.xyzw $vf31, $vf20, $vf30\n\t"
                 "vmaddz.xyzw $vf1, $vf19, $vf1z\n\t"
                 "vmax.xyzw $vf20, $vf21, $vf30\n\t"
                 "vmini.xyzw $vf31, $vf31, $vf1\n\t"
                 "vmax.xyzw $vf1, $vf20, $vf1\n\t"
                 "vminiy.x $vf31, $vf31, $vf31y\n\t"
                 "vminiw.z $vf31, $vf31, $vf31w\n\t"
                 "vmaxy.x $vf1, $vf1, $vf1y\n\t"
                 "vmaxw.z $vf1, $vf1, $vf1w\n\t"
                 "vminiz.x $vf30, $vf31, $vf31z\n\t"
                 "vmaxz.x $vf31, $vf1, $vf1z\n\t"
                 "qmfc2.ni $8, $vf30\n\t"
                 "qmfc2.ni $9, $vf31\n\t"
                 "sw $8, 0x0(%1)\n\t"
                 "sw $9, 0x4(%1)"
                 :
                 : "r"(axis), "r"(range)
                 : "$8", "$9", "memory");
}
