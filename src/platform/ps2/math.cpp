#include "platform/math.h"

#include "game/math.h"

namespace Platform::Math
{
f32 Max(f32 first, f32 second)
{
    f32 larger;
    asm("max.s %0, %1, %2" : "=f"(larger) : "f"(first), "f"(second));
    return larger;
}

f32 Min(f32 first, f32 second)
{
    f32 smaller;
    asm("min.s %0, %1, %2" : "=f"(smaller) : "f"(first), "f"(second));
    return smaller;
}

// The angles go in vf20's x and z, the microprogram leaves their sines and cosines in vf31 (and changes vf01-vf05, ACC and I);
// the macro instructions after it wait for it to end
void SinCos(f32 first, f32 second, f32* out)
{
    alignas(16) f32 angles[4] = {first, 0.0f, second, 0.0f};
    alignas(16) f32 values[4];
    asm volatile("lqc2 $vf20, 0(%1)\n\t"
                 "vnop\n\t"
                 "vcallms 0xF0\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "sqc2 $vf31, 0(%0)"
                 :
                 : "r"(values), "r"(angles)
                 : "memory");
    for (u32 index = 0; index < 4; index++)
    {
        out[index] = values[index];
    }
}

// The animations' microprograms take their inputs in fixed registers and leave their results in vf01 (and vf02, vf03, vf04), using
// the registers from vf05 up as they go; the macro instructions after them (vnop, sqc2) wait for them to end
void SlerpRotations(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    alignas(16) f32 shares[4] = {share, share, share, 1.0f};
    asm volatile("lqc2 $vf1, 0(%1)\n\t"
                 "lqc2 $vf31, 0(%2)\n\t"
                 "lqc2 $vf29, 0(%3)\n\t"
                 "vcallms 0x1C0\n\t"
                 "vnop\n\t"
                 "sqc2 $vf1, 0(%0)"
                 :
                 : "r"(out), "r"(to), "r"(from), "r"(shares)
                 : "memory");
}

void EulerRotation(const Vector4* anglesNow, const Vector4* anglesNext, const Vector4* moveNow, const Vector4* moveNext,
                   Vector4* rotation, Vector4* move)
{
    asm volatile("lqc2 $vf29, 0(%2)\n\t"
                 "lqc2 $vf28, 0(%3)\n\t"
                 "lqc2 $vf31, 0(%4)\n\t"
                 "lqc2 $vf30, 0(%5)\n\t"
                 "vcallms 0x818\n\t"
                 "vnop\n\t"
                 "sqc2 $vf1, 0(%0)\n\t"
                 "sqc2 $vf2, 0(%1)"
                 :
                 : "r"(rotation), "r"(move), "r"(moveNow), "r"(moveNext), "r"(anglesNow), "r"(anglesNext)
                 : "memory");
}

// The point goes in vf01 (its w 1) and the view's address in vi01, the microprogram leaves the distance in vf30's x
f32 ViewDistance(s32 view, const f32* point)
{
    alignas(16) f32 place[4] = {point[0], point[1], point[2], 1.0f};
    alignas(16) f32 distance[4];
    asm volatile("lqc2 $vf1, 0(%1)\n\t"
                 "ctc2.ni %2, $vi1\n\t"
                 "vcallms 0xCB0\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "vnop\n\t"
                 "sqc2 $vf30, 0(%0)"
                 :
                 : "r"(distance), "r"(place), "r"(view)
                 : "memory");
    return distance[0];
}

// The box's extents go in vf02, the gravity's matrix in vf03-vf05, the place in vf06, the scale in vf07's x and the view's address
// in vi01; the microprograms leave the matrix in vf29-vf26, the distance in vf30's x and the clipping flags in vi02
bool ParticleBlockView(s32 view, bool keepsTranslation, const f32* extents, const Matrix4x4* gravity, const f32* place, f32 scale,
                       Matrix4x4* matrix, f32* distance)
{
    alignas(16) f32 box[4] = {extents[0], extents[1], extents[2], 1.0f};
    alignas(16) f32 at[4] = {place[0], place[1], place[2], 1.0f};
    alignas(16) f32 pull[4] = {scale, 0.0f, 0.0f, 1.0f};
    alignas(16) f32 far[4];
    u32 outside;
    if (keepsTranslation)
    {
        asm volatile("lqc2 $vf2, 0(%2)\n\t"
                     "lqc2 $vf3, 0(%3)\n\t"
                     "lqc2 $vf4, 0x10(%3)\n\t"
                     "lqc2 $vf5, 0x20(%3)\n\t"
                     "lqc2 $vf6, 0(%4)\n\t"
                     "ctc2.ni %5, $vi1\n\t"
                     "lqc2 $vf7, 0(%6)\n\t"
                     "vcallms 0xB90\n\t"
                     "vnop\n\t"
                     "sqc2 $vf30, 0(%7)\n\t"
                     "sqc2 $vf29, 0(%1)\n\t"
                     "sqc2 $vf28, 0x10(%1)\n\t"
                     "sqc2 $vf27, 0x20(%1)\n\t"
                     "sqc2 $vf26, 0x30(%1)\n\t"
                     "cfc2.ni %0, $vi2"
                     : "=r"(outside)
                     : "r"(matrix), "r"(box), "r"(gravity), "r"(at), "r"(view), "r"(pull), "r"(far)
                     : "memory");
    }
    else
    {
        asm volatile("lqc2 $vf2, 0(%2)\n\t"
                     "lqc2 $vf3, 0(%3)\n\t"
                     "lqc2 $vf4, 0x10(%3)\n\t"
                     "lqc2 $vf5, 0x20(%3)\n\t"
                     "lqc2 $vf6, 0(%4)\n\t"
                     "ctc2.ni %5, $vi1\n\t"
                     "lqc2 $vf7, 0(%6)\n\t"
                     "vcallms 0xA08\n\t"
                     "vnop\n\t"
                     "sqc2 $vf30, 0(%7)\n\t"
                     "sqc2 $vf29, 0(%1)\n\t"
                     "sqc2 $vf28, 0x10(%1)\n\t"
                     "sqc2 $vf27, 0x20(%1)\n\t"
                     "sqc2 $vf26, 0x30(%1)\n\t"
                     "cfc2.ni %0, $vi2"
                     : "=r"(outside)
                     : "r"(matrix), "r"(box), "r"(gravity), "r"(at), "r"(view), "r"(pull), "r"(far)
                     : "memory");
    }

    *distance = far[0];
    return outside != 0;
}

void TurnRotation(const Vector4* by, Vector4* rotation)
{
    asm volatile("lqc2 $vf1, 0(%0)\n\t"
                 "lqc2 $vf31, 0(%1)\n\t"
                 "vcallms 0xA70\n\t"
                 "vnop\n\t"
                 "sqc2 $vf1, 0(%0)"
                 :
                 : "r"(rotation), "r"(by)
                 : "memory");
}

void Lerp(const Vector4* from, const Vector4* to, f32 share, Vector4* out)
{
    alignas(16) f32 shares[4] = {0.0f, 0.0f, 0.0f, share};
    asm volatile("lqc2 $vf31, 0(%1)\n\t"
                 "lqc2 $vf30, 0(%2)\n\t"
                 "lqc2 $vf29, 0(%3)\n\t"
                 "vaddax.xyz $ACC, $vf30, $vf0x\n\t"
                 "vmsubaw.xyz $ACC, $vf30, $vf31w\n\t"
                 "vmaddw.xyz $vf29, $vf29, $vf31w\n\t"
                 "vmove.w $vf29, $vf0\n\t"
                 "sqc2 $vf29, 0(%0)"
                 :
                 : "r"(out), "r"(shares), "r"(from), "r"(to)
                 : "memory");
}

void JointMatrix(const Vector4* rotation, const Vector4* parentScale, const Vector4* scale, const Vector4* translation,
                 const Matrix4x4* parent, Matrix4x4* out)
{
    if (rotation != nullptr)
    {
        if (parentScale != nullptr)
        {
            asm volatile("lqc2 $vf29, 0(%0)" : : "r"(parentScale) : "memory");
        }
        else
        {
            asm volatile("vmaxw.xyzw $vf29, $vf0, $vf0w");
        }

        asm volatile("lqc2 $vf31, 0(%0)" : : "r"(rotation) : "memory");
        if (scale != nullptr)
        {
            asm volatile("lqc2 $vf30, 0(%0)\n\t"
                         "vcallms 0x668"
                         :
                         : "r"(scale)
                         : "memory");
        }
        else
        {
            asm volatile("vcallms 0x548");
        }
    }
    else
    {
        // The identity's rows
        asm volatile("vsub.xyzw $vf2, $vf2, $vf2\n\t"
                     "vsub.xyzw $vf1, $vf1, $vf1\n\t"
                     "vmr32.xyzw $vf3, $vf0\n\t"
                     "vaddw.y $vf2, $vf0, $vf0w\n\t"
                     "vaddw.x $vf1, $vf0, $vf0w");
    }

    if (translation != nullptr)
    {
        asm volatile("lqc2 $vf4, 0(%0)" : : "r"(translation) : "memory");
    }
    else
    {
        asm volatile("vmove.xyzw $vf4, $vf0");
    }

    if (parent != nullptr)
    {
        asm volatile("lqc2 $vf5, 0(%0)\n\t"
                     "lqc2 $vf6, 16(%0)\n\t"
                     "lqc2 $vf7, 32(%0)\n\t"
                     "lqc2 $vf8, 48(%0)\n\t"
                     "vnop\n\t"
                     "vcallms 0x790"
                     :
                     : "r"(parent)
                     : "memory");
    }

    asm volatile("vnop\n\t"
                 "sqc2 $vf1, 0(%0)\n\t"
                 "sqc2 $vf2, 16(%0)\n\t"
                 "sqc2 $vf3, 32(%0)\n\t"
                 "sqc2 $vf4, 48(%0)"
                 :
                 : "r"(out)
                 : "memory");
}

void MultiplyByParent(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out)
{
    asm volatile("vnop\n\t"
                 "lqc2 $vf1, 0(%0)\n\t"
                 "lqc2 $vf2, 16(%0)\n\t"
                 "lqc2 $vf3, 32(%0)\n\t"
                 "lqc2 $vf4, 48(%0)\n\t"
                 "lqc2 $vf5, 0(%1)\n\t"
                 "lqc2 $vf6, 16(%1)\n\t"
                 "lqc2 $vf7, 32(%1)\n\t"
                 "lqc2 $vf8, 48(%1)\n\t"
                 "vcallms 0x790\n\t"
                 "vnop\n\t"
                 "sqc2 $vf1, 0(%2)\n\t"
                 "sqc2 $vf2, 16(%2)\n\t"
                 "sqc2 $vf3, 32(%2)\n\t"
                 "sqc2 $vf4, 48(%2)"
                 :
                 : "r"(matrix), "r"(parent), "r"(out)
                 : "memory");
}

f32 DivideBySquareRoot(f32 value, f32 square)
{
    f32 result;
    asm("rsqrt.s %0, %1, %2" : "=f"(result) : "f"(value), "f"(square));
    return result;
}

void SetRay(const Vector4* start, const Vector4* end)
{
    asm volatile("lqc2 $vf4, 0(%0)\n\t"
                 "lqc2 $vf5, 0(%1)"
                 :
                 : "r"(start), "r"(end)
                 : "memory");
}

void StartRayTriangle(const Vector4* vertices, const Vector4* nearest)
{
    asm volatile("lqc2 $vf1, 0(%0)\n\t"
                 "lqc2 $vf2, 16(%0)\n\t"
                 "lqc2 $vf3, 32(%0)\n\t"
                 "lqc2 $vf6, 0(%1)\n\t"
                 "vcallms 0xAB8"
                 :
                 : "r"(vertices), "r"(nearest)
                 : "memory");
}

bool FinishRayTriangle(Vector4* hit)
{
    // The microprogram's flags when the triangle was hit nearer
    constexpr u32 Nearer = 0x900;
    u32 flags;
    asm volatile("vnop\n\t"
                 "cfc2.ni %0, $vi2"
                 : "=r"(flags)
                 :
                 : "memory");
    if (flags != Nearer)
    {
        return false;
    }

    asm volatile("sqc2 $vf31, 0(%0)" : : "r"(hit) : "memory");
    return true;
}
}
