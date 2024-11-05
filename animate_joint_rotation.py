import numpy as np

def create_vec4():
    return np.array([0, 0, 0, 0], dtype=np.float32)

bindRotation = np.array(list(map(float, input("Enter bind rotation vector ").split())), dtype=np.float32)
endRotation = np.array(list(map(float, input("Enter end rotation vector ").split())), dtype=np.float32)
factor = float(input("Enter blending factor "))
factor = np.array([factor, factor, factor, 1.0], dtype=np.float32)
print("Factor vector ", end=" ")
print(factor)

resultVec = endRotation * bindRotation
print("Intermediate result vector ", end=" ")
print(resultVec)

I = -1.0
blendedResultVec = resultVec.copy()

resultVec[0] += resultVec[1]
resultVec[0] += resultVec[2]
resultVec[0] += resultVec[3]

if resultVec[0] < 0:
    resultVec[0] *= I
    blendedResultVec *= I

I = 0.999
unkRotVec = create_vec4()
unkRotVec[0] = resultVec[0] - I

if unkRotVec[0] >= 0:
    print("Using easy method for blending")
    unkRotVec[0] = factor[0]
    factor[2] = 1.0
    unkRotVec[2] = factor[2] - factor[0]
    completeResult = endRotation * unkRotVec[2] + blendedResultVec * unkRotVec[0]
    print(completeResult)
    exit(0)

print("Easy method failed, using complicated mess")

resultVec[2] = 1.0
I = np.pi / 2.0
unkRotVec[1] = resultVec[1] * I
unkRotVec[1] -= I
I = -1.0 / (2.0 * np.pi)

unkVec1 = create_vec4()
unkVec1[0] = np.abs(unkRotVec[0])
unkRotVec[0] = 1.0

accumulator = create_vec4()
accumulator[0] = unkVec1[0] * I

accumulator[0] += unkVec1[0] * I
I = 0.5
unkRotVec[0] = accumulator[0] - unkRotVec[0] * I
I = 0.25
unkRotVec[0] = np.abs(unkRotVec[0])
unkRotVec[0] -= I

unkVec2 = create_vec4()
unkVec2[0] = unkRotVec[0] * unkRotVec[0]

I = -(4 + 1/np.pi + 23*np.pi)
unkVec3 = create_vec4()
unkVec3[0] = unkRotVec[0] * I

I = -(4/3 * np.pi * np.pi * np.pi)
unkVec4 = create_vec4()
unkVec4[0] = unkRotVec[0] * I

I = 81.6022
unkVec1[0] = unkRotVec[0] * I

unkVec5 = create_vec4()
unkVec5[0] = unkVec2[0] * unkVec2[0]

unkVec3[0] *= unkVec2[0]
accumulator[0] = unkVec4[0] * unkVec2[0]

I = 39.7107
unkVec2[0] = unkRotVec[0] * I
unkVec4[0] = unkVec5[0] * unkVec5[0]
accumulator[0] += unkVec3[0] * unkVec5[0]
accumulator[0] += unkVec1[0] * unkVec5[0]

I = 2 * np.pi
accumulator[0] += unkRotVec[0] * I
unkVec1[0] = accumulator[0] + unkVec2[0] * unkVec4[0]

I = -np.pi / 2
unkRotVec[0] = resultVec[0] * I
resultVec[0] = unkVec1[0] * I
unkRotVec[0] -= I
resultVec[0] -= I
unkRotVec[0] += unkRotVec[0]
resultVec[0] = unkRotVec[0] - resultVec[0]

factor[2] = resultVec[2] - factor[0]
I = np.pi / 2
unkRotVec[0] = resultVec[0] - I
resultVec[[0, 2]] = factor[[0, 2]] * resultVec[0]

factor[0] = np.abs(unkRotVec[0])
unkVec1[0] = 1.0
unkRotVec[[0, 2]] = resultVec[[0, 2]] * I

I = -1 / (2 * np.pi)
accumulator[0] = factor[0] * I

I = 0.5
factor[0] = accumulator[0] - unkVec1[0] * I

unkRotVec[[0, 2]] = np.abs(unkRotVec[[0, 2]])
resultVec[[0, 2]] = np.array([1, 1], dtype=np.float32)

factor[0] = np.abs(factor[0])
I = -1 / (2 * np.pi)
accumulator[[0, 2]] = unkRotVec[[0, 2]] * I

I = 0.25
factor[0] -= I
I = -1 / (2 * np.pi)
accumulator[[0, 2]] -= unkRotVec[[0, 2]] * I
I = 0.5
resultVec[[0, 2]] *= I
unkVec1[0] = factor[0] * factor[0]
resultVec[[0, 2]] = np.abs(resultVec[[0, 2]])

unkVec5[0] = unkVec1[0] * unkVec1[0]
I = 0.25
resultVec[[0, 2]] *= I
I = -(4 + 1/np.pi + 23*np.pi)
unkVec4[0] = resultVec[0] * I
I = -(4/3 * np.pi * np.pi * np.pi)
unkVec3[0] = resultVec[0] * I
I = 81.6022
unkVec2[0] = resultVec[0] * I

unkRotVec[[0, 2]] = resultVec[[0, 2]] * resultVec[[0, 2]]
unkVec4[0] *= unkVec1[0]

I = 39.7107
unkVec6 = create_vec4()
unkVec6[0] = factor[0] * I
accumulator[0] = unkVec3[0] * unkVec1[0]
unkVec1[[0, 2]] = unkRotVec[[0, 2]] * unkRotVec[[0, 2]]
accumulator[0] += unkVec4[0]

I = -(4 + 1/np.pi + 23*np.pi)
unkVec4[[0, 2]] = resultVec[[0, 2]] * I
I = -(4/3 * np.pi * np.pi * np.pi)
unkVec3[[0, 2]] = resultVec[[0, 2]] * I

unkVec7 = create_vec4()
unkVec7[0] = unkVec5[0] * unkVec5[0]
accumulator[0] += unkVec2[0] * unkVec5[0]

unkVec8 = create_vec4()
unkVec8[[0, 2]] = unkVec4[[0, 2]] * unkRotVec[[0, 2]]
I = 81.6022
unkVec4[[0, 2]] = resultVec[[0, 2]] * I
I = 39.7107
unkVec2[[0, 2]] = resultVec[[0, 2]] * I
I = 2 * np.pi
accumulator[0] += factor[0] * I
factor[0] = accumulator[0] + unkVec6[0] * unkVec7[0]

unkVec5[[0, 2]] = unkVec1[[0, 2]] * unkVec1[[0, 2]]
accumulator[[0, 2]] = unkVec3[[0, 2]] * unkRotVec[[0, 2]]
accumulator[[0, 2]] += unkVec8[[0, 2]] * unkVec1[[0, 2]]
Q = 1 / factor[0]
accumulator[[0, 2]] += unkVec4[[0, 2]] * unkVec1[[0, 2]]
I = 2 * np.pi
accumulator[[0, 2]] += resultVec[[0, 2]] * I
resultVec[[0, 2]] = accumulator[[0, 2]] + unkVec2[[0, 2]] * unkVec5[[0, 2]]
unkRotVec[[0, 2]] = resultVec[[0, 2]] * Q

accumulator = endRotation * unkRotVec[2]
completeResult = accumulator + blendedResultVec * unkRotVec[0]
print(factor)
print(completeResult)

exit(0)