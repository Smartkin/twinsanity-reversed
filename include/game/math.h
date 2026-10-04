#pragma once

#include "abi.h"
#include "common.h"

// The game's maths library. Angles are 65536ths of a turn. Matrixes are four rows, the axes and then the translation, which
// vectors multiply from the left. The functions on vectors only touch x, y and z
struct alignas(16) Vector4
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};
CHECK_SIZE(Vector4, 0x10);

struct Vector2
{
    f32 x;
    f32 y;
};
CHECK_SIZE(Vector2, 8);

struct alignas(16) Matrix4x4
{
    f32 m[4][4];
};
CHECK_SIZE(Matrix4x4, 0x40);

extern "C"
{
    // The angle about an axis (0 x, 1 y, 2 z) that turns a direction toward another, in 65536ths of a turn (returned through the
    // first argument, as GCC 2.9x returns such a struct)
    s32* SignedAngleAbout(s32* angle, const Vector4* from, const Vector4* to, u32 axis) RETAIL(FUN_00188670);
    s32* SignedAngleAboutY(s32* angle, const Vector4* from, const Vector4* to) RETAIL(FUN_0018e560);
    // The squared distance from a point to the line through two points (to the first when they're the same: not to the segment
    // between them, despite the name), and to the segment between them; the point of a segment nearest a point, and the distance
    // squared to it; and the squared distance between two segments, with the shares of the way along them where they're nearest
    // (wrong in a few cases)
    f32 SegmentPointDistanceSquared(const Vector4* segment, const Vector4* point) RETAIL(FUN_00184ad0);
    f32 PointSegmentDistanceSquared(const Vector4* segment, const Vector4* point) RETAIL(FUN_00184c60);
    f32 NearestSegmentPoint(const Vector4* segment, const Vector4* point, Vector4* nearest) RETAIL(FUN_00184df8);
    f32 SegmentsDistanceSquared(const Vector4* first, const Vector4* second, f32* firstShare, f32* secondShare)
        RETAIL(FUN_00185088);
}

// The units AngleFrom takes a value in
enum AngleUnit : u32
{
    AngleRadians = 0,
    AngleDegrees = 1,
    AngleTurns = 2,
};

// A value GCC mustn't fold into what follows: 1 / sqrt(x) made one RSQRT.S rounds unlike the retail SQRT.S and DIV.S
inline f32 Kept(f32 value)
{
    asm("" : "+f"(value));
    return value;
}

// A row of a matrix (a place's axes and its position)
inline const Vector4* RowOf(const Matrix4x4* matrix, u32 row)
{
    return reinterpret_cast<const Vector4*>(matrix->m[row]);
}

inline Vector4* RowOf(Matrix4x4* matrix, u32 row)
{
    return reinterpret_cast<Vector4*>(matrix->m[row]);
}

// An angle within half a turn either way (the other way round when that's shorter)
inline s32 WrapAngle(s32 angle)
{
    return static_cast<s32>(((static_cast<u32>(angle) + 0x8000) & 0xFFFF) - 0x8000);
}

// Where PlaneSide finds a point
enum PlaneSideResult : u32
{
    InFrontOfPlane = 1,
    OnPlane = 2,
    BehindPlane = 3,
};

// A box along the axes: its lowest and its highest corner
struct Box
{
    Vector4 min;
    Vector4 max;

    // Whether a point is in it (its faces included)
    u32 Contains(const Vector4* point) const RETAIL(FUN_001f8f00);
};
CHECK_SIZE(Box, 0x20);

// Points spread evenly over [0, 1]: a value between two of them is on the line between them, from 1 on it's the last one
struct Vector2Curve
{
    u32 count;
    Vector2* points;
};

struct Vector4Curve
{
    u32 count;
    Vector4* points;
};

extern "C"
{
    extern const Matrix4x4 g_IdentityMatrix RETAIL(D_002EA0A0);
    // RandomNext's seed when it's given none
    extern s32 g_RandomSeed RETAIL(D_0030A458);

    void MatrixIdentity(Matrix4x4* matrix) RETAIL(FUN_002c6890);
    Vector2* CopyVector2(Vector2* out, const Vector2* vector) RETAIL(FUN_00164ca8);
    // Turn every row about an axis
    void MatrixRotateX(Matrix4x4* matrix, s32 angle);
    void MatrixRotateY(Matrix4x4* matrix, s32 angle);
    void MatrixRotateZ(Matrix4x4* matrix, s32 angle);

    // Numerical Recipes' ran0 (Park and Miller's minimal standard generator with its seed masked). A seed of 0 becomes 1.
    // Returns the next seed
    s32 RandomNext(s32* seed) RETAIL(FUN_002c6b58);
    // A linear congruential generator of 64 bits (Numerical Recipes' multiplier and increment): its low 23 bits as a float of
    // [0, 1)
    f32 RandomFloat01(u64* state);

    // The sine and the cosine of the angle
    void SinCos16(s32 angle, f32* out);
    // The same of radians (Platform::Math::SinCos: the PS2's VU0 microprogram 0xF0, vcallms 0xF0 in the asm)
    void SinCosRadians(f32 radians, f32* out);
    f32 Sin16(s32 angle);
    f32 Cos16(s32 angle);
    // The sine and the cosine of both: sin first, cos second, then the second's
    void SinCos16Pair(s32 first, s32 second, f32* out);

    // An angle of radians, of degrees (unit 1) or of turns (unit 2), cut down to whole 65536ths of a turn
    void AngleFrom(s32* angle, f32 value, u32 unit) RETAIL_N32(FUN_0011bf10);
    // An angle divided, multiplied, and given radians more, cut down to whole 65536ths of a turn: the angle
    s32* DivideAngle(s32* angle, f32 divisor) RETAIL_N32(FUN_0011bf78);
    s32* MultiplyAngle(s32* angle, f32 scale) RETAIL_N32(FUN_0015de00);
    s32* AddRadiansToAngle(s32* angle, f32 radians) RETAIL_N32(FUN_0015ddd0);

    // Rotations are quaternions (x, y, z, w; VU0's in part). One from three angles about x, y and z (turning about x, then y,
    // then z), and from two of them
    void GetRotationXYZ(Vector4* rotation, const s32* x, const s32* y, const s32* z) RETAIL(GetRotationXYZ);
    void GetRotationXY(Vector4* rotation, const s32* x, const s32* y) RETAIL(GetRotationXY);
    void GetRotationXZ(Vector4* rotation, const s32* x, const s32* z) RETAIL(GetRotationXZ);
    void GetRotationYZ(Vector4* rotation, const s32* y, const s32* z) RETAIL(GetRotationYZ);
    // a times b (VU0's)
    void MultiplyRotations(Vector4* out, const Vector4* a, const Vector4* b) RETAIL(RotateVecByVec);
    // The turn about x, about y and about z by an angle (none: the identity), and about an axis (made a unit one unless it is; the
    // half angle cut down to whole 65536ths of a turn, even none turning), VU0's sine and cosine
    void RotationFromPitch(Vector4* rotation, const s32* pitch) RETAIL(FUN_0018dd20);
    void RotationFromYaw(Vector4* rotation, const s32* yaw) RETAIL(FUN_0018ddb0);
    void RotationFromRoll(Vector4* rotation, const s32* roll) RETAIL(FUN_0018de40);
    void RotationAboutAxis(Vector4* rotation, const Vector4* axis, const s32* angle, u32 unitAxis) RETAIL(FUN_0018df28);
    // The axis a rotation turns about and the angle (whole 65536ths of a turn: twice the half angle cut down to them; the y axis
    // and none for no turn), the rotation normalized in place first unless it already is
    void AxisAngleOfRotation(Vector4* rotation, Vector4* axis, s32* angle, u32 normalized) RETAIL(FUN_001872e0);
    // The sine and the cosine of half an angle from the angle's (the sine's sign picks the half turn)
    void HalfAngleSinCos(f32* sine, f32* cosine) RETAIL(FUN_0018cc18);
    // A direction turned toward another by a share of the angle between them (VU0's sine and cosine; the other one itself when
    // they're parallel, its w the first's otherwise), and the two axes square to a direction (a side and an up)
    void TurnToward(f32 share, Vector4* out, const Vector4* from, const Vector4* to) RETAIL_N32(FUN_00188a90);
    void AxesAround(const Vector4* direction, Vector4* side, Vector4* up) RETAIL(FUN_00188160);
    // The angle about y from z to a direction's x and z
    void YawOfDirection(s32* yaw, const Vector4* direction) RETAIL(FUN_0018cfc0);
    // The angle between two rotations, both normalized in place first, and a vector turned by a rotation (normalized in place first
    // unless it is: none when it's too short)
    void AngleBetweenRotations(s32* angle, Vector4* a, Vector4* b) RETAIL(FUN_001876c8);
    void RotateByQuaternion(Vector4* rotation, const Vector4* vector, Vector4* out, u32 normalized) RETAIL(FUN_00187528);
    // A rotation's angles about x, y and z
    void AnglesOfRotation(const Vector4* rotation, s32* x, s32* y, s32* z) RETAIL(FUN_0018ded0);
    // A rotation made the turn about y by its yaw rounded to a multiple of a step (radians)
    void SnapToYaw(Vector4* rotation, f32 step) RETAIL_N32(FUN_00187c48);
    // The angle between two directions (65536ths of a turn, Abramowitz and Stegun's arc cosine), signed as their cross product's y
    // (its x when y is within 5e-05 of none, its z when both are; a half turn or none when it has no length): the angle
    s32* AngleBetweenDirections(s32* angle, const Vector4* from, const Vector4* to) RETAIL(FUN_00188358);
    // A vector turned about an axis (made a unit one unless it is; nothing when it's too short) by an angle's sine and cosine, and
    // by an angle (none: nothing)
    void TurnBySineCosine(f32 sine, f32 cosine, Vector4* vector, const Vector4* axis, u32 unitAxis) RETAIL_N32(FUN_00188950);
    void TurnAboutAxis(Vector4* vector, const Vector4* axis, const s32* angle, u32 unitAxis) RETAIL(FUN_0018e590);
    // The rotation a fraction of the way between two others, the shorter way round (along a straight line, made a unit one again,
    // when they're within 0.05 of each other), and whether two are the same turn (each part within a tolerance, either way round)
    void SlerpRotations(f32 t, Vector4* out, const Vector4* from, const Vector4* to) RETAIL_N32(FUN_00187890);
    u32 SameRotation(const Vector4* rotation, const Vector4* other, f32 tolerance) RETAIL_N32(FUN_00186930);
    // A matrix's rotation rows set from a rotation (VU0 code)
    void MatrixFromRotation(Matrix4x4* matrix, const Vector4* rotation) RETAIL(Rotate_);
    // A rotation matrix about an axis (made a unit one unless it is; none when it's too short) from the angle's sine and cosine, the
    // one turning a direction to another (a half turn about an axis square to the longer when they're opposite, none when they're
    // the same way), the same as a rotation, and a matrix whose rows are the axes looking from an eye to a target with an up
    // vector (y without one; its position row as retail has it: less the eye along the up and the side, the eye along the forward
    // axis)
    void MatrixAboutAxis(f32 sine, f32 cosine, Matrix4x4* matrix, const Vector4* axis, u32 unitAxis) RETAIL_N32(FUN_001857e8);
    // The same of an angle (none: the identity; VU0's sine and cosine)
    void AxisAngleMatrix(Matrix4x4* matrix, const Vector4* axis, const s32* angle, u32 unitAxis) RETAIL(FUN_0018d638);
    void MatrixBetween(Matrix4x4* matrix, const Vector4* from, const Vector4* to) RETAIL(FUN_00185c10);
    void RotationBetween(Vector4* rotation, const Vector4* from, const Vector4* to) RETAIL(FUN_00187d78);
    void LookAtMatrix(Matrix4x4* matrix, const Vector4* eye, const Vector4* target, const Vector4* up) RETAIL(FUN_00185df8);
    // A matrix whose third row is a direction (unit), the others made square to it with the second one up as far as it goes; a
    // matrix of unit axes around one (the axis that row, the next one square to it, the one after their cross product), and a
    // matrix looking along a direction with an up vector (the facing matrix of the direction in the up's frame, taken back)
    void MatrixFacing(Matrix4x4* matrix, const Vector4* direction) RETAIL(FUN_001861d0);
    void BasisAround(Matrix4x4* matrix, const Vector4* axis, s32 row) RETAIL(FUN_00186398);
    void LookAlong(Matrix4x4* matrix, const Vector4* direction, const Vector4* up) RETAIL(FUN_001860b0);
    // A value kept between two others
    f32 ClampFloat(f32 value, f32 low, f32 high) RETAIL(ClampFloat);
    // Nothing (the game context's constructor calls it)
    void UnkDebugFunction3();
    // The cosine and the sine of the angle, and each alone
    void CosSin16(const s32* angle, f32* cosine, f32* sine) RETAIL(FUN_0018c0a8);
    f32 SinOfAngle(const s32* angle) RETAIL(FUN_0018c050);
    f32 CosOfAngle(const s32* angle) RETAIL(FUN_0018c078);
    // The tangent (1e30 for a cosine of 0, signed as the sine)
    f32 TanOfAngle(const s32* angle) RETAIL(FUN_0018c0f8);
    // A matrix turning about z by the angle
    void MatrixRotationZ(Matrix4x4* matrix, const s32* angle) RETAIL(FUN_0018c428);
    // The first three rows' first three columns multiplied by the scale's x, y and z: the first row by x and so on (a scale
    // before the matrix), and each row's first column by x and so on (a scale after it, which scales the translation too)
    void MatrixScaleRows(Matrix4x4* matrix, const Vector4* scale) RETAIL(FUN_001844d8);
    void MatrixScaleColumns(Matrix4x4* matrix, const Vector4* scale) RETAIL(FUN_00184570);
    // The curve's value at t
    void SampleVector2Curve(const Vector2Curve* curve, f32 t, Vector2* out) RETAIL_N32(FUN_0017c668);
    void SampleVector4Curve(const Vector4Curve* curve, f32 t, Vector4* out) RETAIL_N32(FUN_001ac110);
    Vector4* CopyVector4(Vector4* out, const Vector4* vector) RETAIL(FUN_0018e080);

    // The C library's rand (its state in the library's), and the game's floats of it: [0, 1) and [-1, 1)
    s32 GetRand();
    f32 GetRandFloat();
    f32 RandomSigned() RETAIL(FUN_0018c880);
    // The same times a range
    f32 RandomSignedTimes(f32 range) RETAIL(GetRandFloat_);

    // The smallest power of two that's at least the value (1 for anything below 2)
    s32 NextPowerOfTwo(s32 value) RETAIL(FUN_002c6d70);

    void TransformPoint(Vector4* out, const Vector4* point, const Matrix4x4* matrix);
    // Without the translation
    void TransformVector(Vector4* out, const Vector4* vector, const Matrix4x4* matrix);
    void RotateVectorY(Vector4* out, const Vector4* vector, s32 angle);
    void RotateVectorZ(Vector4* out, const Vector4* vector, s32 angle);
    void ScaleVector(Vector4* out, const Vector4* vector, f32 scale) RETAIL_N32(FUN_002c6fb0);
    f32 SquaredDistance(const Vector4* a, const Vector4* b) RETAIL(FUN_0018f388);
    f32 DotProduct(const Vector4* a, const Vector4* b) RETAIL(FUN_001a0058);
    // 1 over the vector's length, 0 when its square isn't above epsilon
    f32 InverseLength(const Vector4* vector, f32 epsilon) RETAIL_N32(RSQRT);
    void SubtractVectors(Vector4* out, const Vector4* from, const Vector4* vector) RETAIL(FUN_002c6fd8);
    // 1 over a 4D vector's length (a rotation's), the fallback when its square isn't above the epsilon's
    f32 InverseLength4(f32 fallback, f32 epsilon, const Vector4* vector) RETAIL_N32(FUN_0011c028);
    // A plane through a point with a normal (the normal, its w the plane's offset), and a point taken onto a plane along its
    // normal by the distance of another (the game passes the same point), a matrix made of four rows
    Vector4* PlaneThrough(Vector4* plane, const Vector4* normal, const Vector4* point) RETAIL(FUN_0018d8c0);
    // A plane through a triangle's points: its normal the cross product of the edges from the first (a perpendicular of the first
    // edge, of the second, or the y axis when they're too short), its w the offset
    void PlaneFromTriangle(Vector4* plane, const Vector4* first, const Vector4* second, const Vector4* third);
    // Three times a vector square to another (the other itself when it's too short): whether it had a length
    u32 PerpendicularOf(const Vector4* vector, Vector4* out) RETAIL(FUN_0018e1d0);
    // Where a point is against a plane, an epsilon either side counting as on it
    u32 PlaneSide(f32 epsilon, const Vector4* plane, const Vector4* point) RETAIL_N32(FUN_0018d9d0);
    void ProjectOntoPlane(const Vector4* plane, const Vector4* point, Vector4* out) RETAIL(FUN_0018db90);
    // A plane moved by a rigid matrix: its normal turned, its offset less how far along it the matrix's position is
    void TransformPlane(const Vector4* plane, const Matrix4x4* matrix, Vector4* out) RETAIL(FUN_0018daa0);
    void MatrixFromRows(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z, const Vector4* w) RETAIL(FUN_0018c1c0);
    // The angle (65536ths of a turn) whose cosine, or sine, is a value (Abramowitz and Stegun's 4.4.45, the value kept within
    // -1 to 1): the angle
    s32* AngleOfCosine(f32 cosine, s32* angle) RETAIL_N32(FUN_0018bec8);
    s32* AngleOfSine(f32 sine, s32* angle) RETAIL_N32(ArcSin16);
    // A matrix of four columns
    void MatrixFromColumns(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z, const Vector4* w)
        RETAIL(FUN_0018c1e8);
    // A matrix turning about x and about y by an angle (none: the identity; VU0's sine and cosine), a vector turned by such a
    // matrix's x and z (its y and w kept), a matrix moved along its own axes, and a vector moved by a matrix's position (its w kept)
    void MatrixAboutX(Matrix4x4* matrix, const s32* angle) RETAIL(FUN_0018c278);
    void MatrixAboutY(Matrix4x4* matrix, const s32* angle) RETAIL(FUN_0018c350);
    // A rotation matrix of angles about x and y (the turn about x first; VU0's sine and cosine), about x and z, about y and z,
    // and of angles about x, y and z
    void MatrixFromPitchYaw(Matrix4x4* matrix, const s32* pitch, const s32* yaw) RETAIL(FUN_00183260);
    void MatrixFromPitchRoll(Matrix4x4* matrix, const s32* pitch, const s32* roll) RETAIL(FUN_001834d0);
    void MatrixFromYawRoll(Matrix4x4* matrix, const s32* yaw, const s32* roll) RETAIL(FUN_00183740);
    void MatrixFromAngles(Matrix4x4* matrix, const s32* x, const s32* y, const s32* z) RETAIL(FUN_001839b0);
    void TurnAboutY(const Matrix4x4* matrix, const Vector4* vector, Vector4* out) RETAIL(FUN_0018c650);
    void MoveAlongAxes(Matrix4x4* matrix, const Vector4* move) RETAIL(FUN_0018c6d8);
    void AddMatrixPosition(const Matrix4x4* matrix, const Vector4* vector, Vector4* out) RETAIL(FUN_0018c740);
    // Random numbers of the C library's rand: one, one below a count (0 below 2), one from a value below a count past it; a
    // float below a range, from a value below a range past it, and around a value within a range either way
    s32 RandomInt() RETAIL(FUN_0018c788);
    s32 RandomBelow(s32 count) RETAIL(GetRandom);
    s32 RandomFrom(s32 low, s32 count) RETAIL(GetRandRange);
    f32 RandomBelowFloat(f32 range) RETAIL(FUN_0018c910);
    f32 RandomFromFloat(f32 low, f32 range) RETAIL(FUN_0018c950);
    f32 RandomAround(f32 centre, f32 range) RETAIL(FUN_0018c9a0);
    // A quadratic's roots (a x squared + b x + c, the stable way: none when it has none): how many, and a value to a whole power
    // (0 for 0, even to the power 0)
    s32 SolveQuadratic(f32* roots, f32 a, f32 b, f32 c) RETAIL_N32(FUN_0018ca78);
    f32 PowerOf(f32 base, s32 exponent) RETAIL_N32(FUN_0018cb38);
    // A value when above 0, 0 below, and 0 times a scale at 0
    f32 PositivePart(f32 value, f32 scale) RETAIL(FUN_0018cbc8);
    // A pair made a unit vector unless its length squared is within a tolerance of 1 (squared; nothing when it's nearly 0), and
    // turned by a cosine and a sine first
    void NormalizePair(f32* x, f32* y, f32 tolerance) RETAIL_N32(FUN_0018ccd8);
    void TurnPair(f32* x, f32* y, f32 cosine, f32 sine, f32 tolerance) RETAIL_N32(FUN_0018ce00);
    // How far two ranges reach together when they overlap (from the lower low to the higher high; 0 when they don't)
    f32 OverlappingRangesSpan(f32 firstLow, f32 firstHigh, f32 secondLow, f32 secondHigh) RETAIL(FUN_0018cdb8);
    // The angle about x from z to a direction's y and z, and the signed angles between two directions about x and y: the angle
    void PitchOfDirection(s32* pitch, const Vector4* direction) RETAIL(FUN_0018d028);
    s32* SignedAngleAboutX(s32* angle, const Vector4* from, const Vector4* to) RETAIL(FUN_0018e530);
    // What's left after so many seconds of a half-life: 0.5 to the power of seconds over the half-life (the C library's expf of
    // the seconds times -ln 2 over the half-life)
    f32 HalfLifeShare(f32 halfLife, f32 seconds) RETAIL(FUN_0018d090);
    // The point at a parameter of a line (its two ends exactly at 0 and 1), the difference of its ends' dot products with a point,
    // and whether its length squared is at least a product (that length squared kept when asked)
    void LinePointAt(f32 along, const Vector4* line, Vector4* out) RETAIL_N32(FUN_0018d0d8);
    f32 LineDotDifference(const Vector4* line, const Vector4* point) RETAIL(FUN_0018d198);
    u32 LineAtLeast(const Vector4* line, f32* lengthSquared, f32 length, f32 scale) RETAIL_N32(FUN_0018d1f0);
    // A value cut down to a whole number, and a vector's length squared (x, y and z)
    s32 TruncateToInt(f32 value) RETAIL(FUN_0018b580);
    f32 LengthSquared(const Vector4* vector) RETAIL(FUN_0018b590);
    // The angle of a point from the x axis (65536ths of a turn, by Abramowitz and Stegun's 4.4.49 on half of it; a half turn
    // when it's on the negative x axis or at the origin): the angle
    s32* AngleOfPoint(s32* angle, f32 y, f32 x) RETAIL_N32(FUN_001830a8);
    // A matrix of three axes (rows, their w 0) at the origin, a matrix's position set (its w kept), a matrix multiplied by
    // another from the front (by times it), two multiplied the other way round (b times a), a matrix's angles about x, y and z
    // and a matrix turned by a rotation from the front
    void MatrixFromAxes(Matrix4x4* matrix, const Vector4* x, const Vector4* y, const Vector4* z) RETAIL(FUN_0018d3f0);
    void SetMatrixPosition(Matrix4x4* matrix, const Vector4* position) RETAIL(FillPosition);
    void PreMultiply(Matrix4x4* matrix, const Matrix4x4* by) RETAIL(FUN_0018d5b8);
    void MultiplyReversed(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out) RETAIL(FUN_0018d608);
    void EulerAnglesOfMatrix(const Matrix4x4* matrix, s32* x, s32* y, s32* z) RETAIL(FUN_00185910);
    void AnglesOfMatrix(const Matrix4x4* matrix, s32* angles) RETAIL(FUN_0018d6c8);
    void TurnMatrix(Matrix4x4* matrix, const Vector4* rotation) RETAIL(FUN_0018d7b0);
    // Where a point is against a plane exactly (in front 1, on it 2, behind 3), a plane with a normal (its w less how far along
    // it a point is, the normal's w kept), a plane moved by a rigid matrix in place, and a point taken onto a plane along its normal in place
    u32 PlaneSideOf(const Vector4* plane, const Vector4* point) RETAIL(FUN_0018d968);
    void PlaneAlongNormal(const Vector4* normal, const Vector4* point, Vector4* plane) RETAIL(FUN_0018da50);
    void TransformPlaneInPlace(Vector4* plane, const Matrix4x4* matrix) RETAIL(FUN_0018db18);
    void ProjectOntoPlaneInPlace(const Vector4* plane, Vector4* point) RETAIL(FUN_0018dc20);
    // A rotation from the sine and the cosine of half its angle about an axis (made a unit one unless it is; none for one too
    // short), a vector reflected across a plane through the origin and its part along a normal taken away (the normal a unit one
    // or not), the triple product of three vectors (its terms kept when asked), and whether two vectors are parallel within a
    // tolerance (their dot product kept when asked)
    void RotationFromAxisSine(f32 sine, f32 cosine, Vector4* rotation, const Vector4* axis, u32 unitAxis) RETAIL_N32(FUN_0018dfa8);
    void ReflectAcross(Vector4* vector, const Vector4* normal, u32 unitNormal) RETAIL(FUN_0018e0b0);
    void RemoveComponentAlong(Vector4* vector, const Vector4* normal, u32 unitNormal) RETAIL(FUN_0018e420);
    f32 TripleProduct(const Vector4* a, const Vector4* b, const Vector4* c, Vector4* terms) RETAIL(FUN_0018e288);
    u32 AreParallel(f32 tolerance, const Vector4* a, const Vector4* b, f32* dot) RETAIL_N32(FUN_0018e608);
    // A matrix's inverse through its cofactors (the size 3 one's rotation's, its translation none; nothing when it has none, and
    // not into the matrix itself), and the minor of an element (row times 4 plus column: the determinant of what's left without
    // its row and column; 0 past the last)
    void InvertMatrix(const Matrix4x4* matrix, s32 size, Matrix4x4* out) RETAIL(FUN_00183b60);
    f32 MatrixMinor(const Matrix4x4* matrix, u32 element) RETAIL(FUN_00183e48);
    // A vector moved by random amounts up to a spread times a scale along each axis: its x by the second scale's, its y by the
    // first's, its z by the third's
    void JitterVector(f32 spread, f32 yScale, f32 xScale, f32 zScale, Vector4* vector) RETAIL_N32(FUN_0023cfd8);
    // A vector's part along an axis scaled by a share (the axis taken as a unit one when said, else its part found through its
    // length squared; nothing when it has none along it)
    void ScaleAlongAxis(f32 share, Vector4* vector, const Vector4* axis, u32 unitAxis) RETAIL_N32(FUN_0018e310);
    // A matrix's rows made its columns: only its rotation's when the size is 3 (the rest the identity's), and in place (the
    // rotation's, or all of it when the size is 4)
    void TransposeMatrix(const Matrix4x4* matrix, s32 size, Matrix4x4* out) RETAIL(FUN_0018c588);
    void TransposeInPlace(Matrix4x4* matrix, s32 size) RETAIL(FUN_0018c510);
    // Three values' indexes from the largest down (retail's middle is the smallest's when the third value lies between the first
    // two)
    void SortThree(const f32* values, s32* largest, s32* middle, s32* smallest) RETAIL(FUN_0018cf08);
    // A rotation (the rows' x, y and z) of a column times a row (the rest the identity's), scaled, added to another, and a value
    // added to its diagonal (and the fourth element's when said)
    void OuterProduct(Matrix4x4* out, const f32* column, const Vector4* row) RETAIL(FUN_0018d728);
    void ScaleRotation(f32 scale, Matrix4x4* matrix) RETAIL_N32(FUN_0018d530);
    void AddRotation(Matrix4x4* matrix, const Matrix4x4* add) RETAIL(FUN_0018d490);
    void AddToDiagonal(f32 value, Matrix4x4* matrix, u32 all) RETAIL_N32(FUN_0018d4e8);

    // What the module's static constructor sets: the default box (from the origin (w 1) to 1, 1, 1 (w 0); its min is the zero
    // point everything starts at), the scale joints have without one of their own (1, 1, 1, 1) and the axes (w 1)
    extern Box g_DefaultBox RETAIL(G_DefaultBbox_);
    extern Vector4 g_JointScale RETAIL(GlobalJointScale);
    extern Vector4 g_XAxis RETAIL(D_00323880);
    extern Vector4 g_YAxis RETAIL(D_00323890);
    extern Vector4 g_ZAxis RETAIL(D_003238A0);
    void InitMathConstants(u32 initialise, u32 priority) RETAIL(FUN_0018b4c0);
    // The module's global constructor (in the static constructors' table): InitMathConstants(1, 0xFFFF)
    void MathConstantsConstructor() RETAIL(FUN_0018f7b8);

    // Brent's search for the smallest value of a function of one value (Numerical Recipes' brent): the most steps, an absolute
    // tolerance and a relative one (of the best value's size), the bracket it searches, the point that was second best before
    // the second best and the second best with their values, and the last point tried with its value. The function (an EABI
    // one, given the argument and the value) is searched from a start and its value there (known, or worked out), both the
    // minimum's when it's done: 0 when it converged, -1 when the steps ran out
    struct MinimumSearch
    {
        s32 steps;
        f32 tolerance;
        f32 closeness;
        f32 low;
        f32 high;
        f32 previous;
        f32 previousValue;
        f32 second;
        f32 secondValue;
        f32 tried;
        f32 triedValue;
    };
    CHECK_SIZE(MinimumSearch, 0x2C);
    s32 FindMinimum(MinimumSearch* search, void* argument, const void* function, f32* at, f32* value, u32 valueKnown)
        RETAIL(FUN_0018f3d0);

    // VU0's half of the library (macro mode, its rounding: the renderer's data has to come out of these). The rotation's rows
    // made its columns without the translation, every row made a column, the inverse of a rotation and a translation, a times
    // b, a point transformed (its w taken as 1) and a vector turned (its w kept)
    void InitIdentityMatrix(Matrix4x4* matrix);
    void VuTransposeRotation(const Matrix4x4* matrix, Matrix4x4* out) RETAIL(CopyMatrix_);
    void VuTranspose(const Matrix4x4* matrix, Matrix4x4* out) RETAIL(FUN_0018ee40);
    void VuInvertRigid(Matrix4x4* out, const Matrix4x4* matrix) RETAIL(FUN_0018ef18);
    void VuInvertRigidInPlace(Matrix4x4* matrix) RETAIL(FUN_0018ef98);
    void VuMultiplyMatrices(const Matrix4x4* a, const Matrix4x4* b, Matrix4x4* out) RETAIL(MultiplyMatrices);
    // A matrix multiplied by another in place (VuMultiplyMatrices into a copy)
    void MultiplyInPlace(Matrix4x4* matrix, const Matrix4x4* by) RETAIL(MultiplyMatByMat);
    void VuTransformPoint(const Matrix4x4* matrix, const Vector4* point, Vector4* out) RETAIL(MultiplyMatrixByVector);
    void VuRotateVector(const Matrix4x4* matrix, const Vector4* vector, Vector4* out) RETAIL(TransformVector_);
    // A point through a matrix's four rows (its own w taking the fourth's share), and four quadwords' words transposed in place
    // (a matrix's rows made its columns)
    void VuTransformByRows(const Matrix4x4* rows, const Vector4* point, Vector4* out) RETAIL(FUN_0018ecc0);
    void TransposeQuadwords(void* rows) RETAIL(FUN_0018ee88);
}
