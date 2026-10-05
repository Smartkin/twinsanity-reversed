#pragma once

#include "common.h"

struct Matrix4x4;
struct Vector4;

// Maths the game's results depend on to the last bit, which the PS2 does on vector unit 0
namespace Platform::Math
{
// The sines and cosines of two angles in radians: sin first, cos first, sin second, cos second. The PS2 runs the game's VU0
// microprogram (0xF0 of the .vutext chain the start-up loads into VU0), which SinCosPolynomial does on the CPU: on the PS2 they
// differ in the last bit now and then (PCSX2 cuts the smaller operand of an EE FPU addition down to the bits the EE's adder
// keeps, VU0's additions keep all of them), and the game's pictures with them
void SinCos(f32 first, f32 second, f32* out);
// The EE FPU's MAX.S and MIN.S: the larger and the smaller of two floats compared as signed magnitudes (+0 above -0)
f32 Max(f32 first, f32 second);
f32 Min(f32 first, f32 second);

// The animations' joints, VU0's microprograms on the PS2 (their macro mode steps around them as well):
// a rotation a share of the way toward another (0x1C0: spherical, or straight when they're within 0.999 of each other, the shorter
// way round),
void SlerpRotations(const Vector4* from, const Vector4* to, f32 share, Vector4* out);
// a frame's rotation of its Euler angles (x, y, z, turned the shorter way round) the share between this frame's and the next's (in
// the next's w), and a translation the same share between two (0x818),
void EulerRotation(const Vector4* anglesNow, const Vector4* anglesNext, const Vector4* moveNow, const Vector4* moveNext,
                   Vector4* rotation, Vector4* move);
// a rotation turned by another (0xA70),
void TurnRotation(const Vector4* by, Vector4* rotation);
// a value the share of the way toward another as macro mode works it out (from + (to - from) · share, its w 1),
void Lerp(const Vector4* from, const Vector4* to, f32 share, Vector4* out);
// a joint's matrix: its rotation's (the identity without one), divided by its parent's scale and scaled by its own when it has
// them (0x548, 0x668), its translation (none: (0, 0, 0, 1)) as its last row, in its parent's space when there's a parent (0x790)
void JointMatrix(const Vector4* rotation, const Vector4* parentScale, const Vector4* scale, const Vector4* translation,
                 const Matrix4x4* parent, Matrix4x4* out);
// and a matrix put in its parent's space the way the last of those does it (0x790: times the parent's; a skin's joint's inverse
// bind pose by its matrix, an instance's place by its chunk's matrix); out may be either
void MultiplyByParent(const Matrix4x4* matrix, const Matrix4x4* parent, Matrix4x4* out);

// The distance between a point and the camera's place in a view loaded for the frame's particles
// (Platform::Graphics::LoadParticleView; VU0's microprogram 0xCB0 of the second set on the PS2, the view in its memory)
f32 ViewDistance(s32 view, const f32* point);

// A particle block's matrix for drawing in a view loaded for the frame's particles: the view's matrices with the block's gravity
// matrix and its place (pulled towards the camera by a scale), its distance from the camera, and whether a box of the extents
// around the place is out of the view (VU0's microprograms 0xA08 and, for emissions keeping their translation, 0xB90 of the second
// set on the PS2). Whether it's out of the view
bool ParticleBlockView(s32 view, bool keepsTranslation, const f32* extents, const Matrix4x4* gravity, const f32* place, f32 scale,
                       Matrix4x4* matrix, f32* distance);

// A value over a square root rounded as one division (the EE FPU's RSQRT.S, which retail's compiler made of a / sqrtf(b) in
// places: it rounds otherwise than a square root and a division)
f32 DivideBySquareRoot(f32 value, f32 square);

// Rays against triangles (the fast ray casts through the collision; VU0's microprogram 0xAB8 of its standard set on the PS2, the
// ray kept in its registers): the ray set, then a triangle (three vertexes) tested against the nearest hit so far (its w the
// distance along the ray), the test running while the caller goes on until the next Finish, which says whether the triangle was
// hit nearer and gives that hit (its w the distance)
void SetRay(const Vector4* start, const Vector4* end);
void StartRayTriangle(const Vector4* vertices, const Vector4* nearest);
bool FinishRayTriangle(Vector4* hit);

// The microprogram's steps: the cosine of a turn's fraction as a polynomial of degree 9, the sine as the cosine of the angle less
// a quarter turn
inline f32 CosinePolynomial(f32 radians)
{
    constexpr f32 MinusInverseTurn = -0x1.45F30Ep-3f;
    constexpr f32 Term1 = 0x1.921FB4p+2f;
    constexpr f32 Term3 = -0x1.4ABBC0p+5f;
    constexpr f32 Term5 = 0x1.4668AEp+6f;
    constexpr f32 Term7 = -0x1.324CC2p+6f;
    constexpr f32 Term9 = 0x1.3DAF6Ep+5f;
    f32 angle = __builtin_fabsf(radians);
    // The fraction of a turn: VU0 took the whole turns off by adding and taking away 1.5 * 2^23, which its rounding towards zero
    // makes the floor
    f32 turns = angle * MinusInverseTurn;
    f32 whole = -static_cast<f32>(static_cast<s32>(-turns));
    f32 fraction = whole - turns;
    // From 0.25 at no turn through 0 at a quarter to -0.25 at half a turn: the cosine is the sine of a turn of that
    f32 t = __builtin_fabsf(fraction - 0.5f) - 0.25f;
    f32 t2 = t * t;
    f32 t4 = t2 * t2;
    f32 t8 = t4 * t4;
    f32 sum = Term3 * t * t2;
    sum += Term7 * t * t2 * t4;
    sum += Term5 * t * t4;
    sum += t * Term1;
    return sum + Term9 * t * t8;
}

inline void SinCosPolynomial(f32 first, f32 second, f32* out)
{
    // The microprogram's π/2, a few bits below the float nearest it
    constexpr f32 ProgramHalfPi = 0x1.921FB0p+0f;
    out[0] = CosinePolynomial(first - ProgramHalfPi);
    out[1] = CosinePolynomial(first);
    out[2] = CosinePolynomial(second - ProgramHalfPi);
    out[3] = CosinePolynomial(second);
}
}
