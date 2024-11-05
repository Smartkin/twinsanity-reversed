// _vcallms(0x1C0) vcallms 0x38

typedef usVector4 = unsigned Vector4;

// First 4 bits set if calc results were 0
// Second set of 4 bits set if calc results were negative
// Third set of 4 bits set if calc results caused an underflow
// Fourth set of 4 bits set if calc results caused an overflow
usVector4 MAC_FLAGS = 0;

// bindRotation vf01
// endRotation vf31
// factor vf29
void AnimationJointRotate(Vector4 bindRotation, Vector4 endRotation, Vector4 factor)
{
	Vector4 resultVec = endRot * bindRotation // vf09
	unsigned int resultWasNegative = 0x80
	int I = -1
	resultVec.x += resultVec.y
	resultVec.x += resultVec.z
	resultVec.x += resultVec.w
	
	Vector4 resultVecCopy = resultVec // vf10
	resultWasNegative &= MAC_FLAGS
	if (resultWasNegative != 0) {
		resultVec.x *= I
		resultVecCopy *= I
	}
	
	I = 0.999
	Vector4 unkRotVec; // vf11
	unkRotVec.x = resultVec.x - I
	resultWasNegative = 0x80
	zeroVec.x = abs(resultVecCopy.x)
	
	resultWasNegative &= MAX_FLAGS
	if (resultWasNegative == 0) {
		unkRotVec.x = factor.x
		factor.z = 1.0
		unkRotVec.z = factor.z - factor.x
		accumulator = endRot * unkRotVec.z
		OUT_VEC = accumulator + resultVecCopy * unkRotVec.x
		return;
	}
	
	resultVec.z = 1.0
	I = PI / 2
	unkRotVec.x = resultVec.x * I
	I = PI / 2
	unkRotVec.x -= I
	I = -1 / (2 * PI) // 1 radian to degree
	Vector4 unkVec; // vf12
	unkVec.x = abs(unkRotVec.x)
	unkRotVec.x = 1.0
	accumulator.x = unkVec.x * I;
	
	// What the hell is this???
	I = 1.25829e7
	accumulator.x -= unkRotVec.x * I
	accumulator.x += unkRotVec.x * I
	
	I = -1 / (2 * PI)
	accumulator.x += unkVec.x * I
	I = 0.5
	unkRotVec.x = accumulator.x - unkRotVec.x * I
	I = 0.25
	unkRotVec.x = abs(unkRotVec.x)
	unkRotVec.x -= I
	
	Vector4 unkVec2; // vf15
	unkVec2.x = unkRotVec.x * unkRotVec.x
	I = -(4 + 1/PI + 23*PI)
	Vector4 unkVec3; // vf16
	unkVec3.x = unkRotVec.x * I
	I = -(4/3 * PI * PI * PI)
	Vector4 unkVec4; // vf18
	unkVec4.x = unkRotVec.x * I
	I = 81.6022
	unkVec.x = unkRotVec.x * I
	Vector4 unkVec5; // vf13
	unkVec5.x = unkVec2.x * unkVec2.x
	unkVec3.x *= unkVec2.x
	accumulator.x = unkVec4.x * unkVec2.x
	I = 39.7107
	unkVec2.x = unkRotVec.x * I
	unkVec4.x = unkVec5.x * unkVec5.x
	accumulator.x += unkVec3.x * unkVec5.x
	accumulator.x += unkVec.x * unkVec5.x
	I = 2 * PI
	accumulator.x += unkRotVec.x * I
	unkVec.x = accumulator.x + unkVec2.x * unkVec4.x
	
	I = -PI / 2
	unkRotVec.x = resultVec.x * I
	resultVec.x = unkVec.x * I
	unkRotVec.x -= I
	resultVec.x -= I
	unkRotVec.x += unkRotVec.x
	resultVec.x = unkRotVec.x - resultVec.x
	
	factor.z = resultVec.z - factor.x
	I = PI / 2
	unkRotVec.x = resultVec.x - I
	I = PI / 2
	resultVec.xz = factor.xz * resultVec.x
	factor.x = abs(unkRotVec.x)
	unkVec.x = 1.0
	unkRotVec.xz = resultVec.xz * I
	I = -1 / (2 * PI) // 1 radian to degree
	accumulator.x = factor.x * I
	
	I = 1.25829e7
	accumulator.x -= unkVec.x * I
	accumulator.x += unkVec.x * I
	
	I = -1 / (2 * PI) // 1 radian to degree
	accumulator.x -= factor.x * I
	I = 0.5
	factor.x = accumulator.x - unkVec.x * I
	
	unkRotVec.xz = abs(unkRotVec.xz)
	resultVec.xz = Vector2(1.0, 1.0)
	
	factor.x = abs(factor.x)
	I = -1 / (2 * PI) // 1 radian to degree
	accumulator.xz = unkRotVec.xz * I
	
	I = 1.25829e7
	accumulator.xz -= resultVec.xz * I
	accumulator.xz += resultVec.xz * I
	
	I = 0.25
	factor.x -= I
	I = -1 / (2 * PI) // 1 radian to degree
	accumulator.xz -= unkRotVec.xz * I
	I = 0.5
	resultVec.xz *= I
	unkVec.x = factor.x * factor.x
	resultVec.xz = abs(resultVec.xz)
	
	unkVec5.x = unkVec.x * unkVec.x
	I = 0.25
	resultVec.xz *= I
	I = -(4 + 1/PI + 23*PI)
	unkVec4.x = resultVec.x * I
	I = -(4/3 * PI * PI * PI)
	unkVec3.x = resultVec.x * I
	I = 81.6022
	unkVec2.x = resultVec.x * I
	
	unkRotVec.xz = resultVec.xz * resultVec.xz
	unkVec4.x *= unkVec.x
	
	I = 39.7107
	Vector4 unkVec6; // vf14
	unkVec6.x = factor.x * I
	accumulator.x = unkVec3.x * unkVec.x
	unkVec.xz = unkRotVec.xz * unkRotVec.xz
	accumulator.x += unkVec4.x
	
	I = -(4 + 1/PI + 23*PI)
	unkVec4.xz = resultVec.xz * I
	I = -(4/3 * PI * PI * PI)
	unkVec3.xz = resultVec.xz * I
	
	Vector4 unkVec7; // vf17
	unkVec7.x = unkVec5.x * unkVec5.x
	accumulator.x += unkVec2.x * unkVec5.x
	
	Vector4 unkVec8; // vf19
	unkVec8.xz = unkVec4.xz * unkRotVec.xz
	I = 81.6022
	unkVec4.xz = resultVec.xz * I
	I = 39.7107
	unkVec2.xz = resultVec.xz * I
	I = 2 * PI
	accumulator.x += factor.x * I
	factor.x = accumulator.x + unkVec6.x * unkVec7.x
	
	unkVec5.xz = unkVec.xz * unkVec.xz
	accumulator.xz = unkVec3.xz * unkRotVec.xz
	accumulator.xz += unkVec8.xz * unkVec.xz
	Q = 1 / factor.x
	accumulator.xz += unkVec4.xz * unkVec.xz
	I = 2 * PI
	accumulator.xz += resultVec.xz * I
	resultVec.xz = accumulator.xz + unkVec2.xz * unkVec5.xz
	unkRotVec.xz = resultVec.xz * Q
	
	accumulator = endRot * unkRotVec.z
	OUT_VEC = accumulator + resultVecCopy * unkRotVec.x
}





























