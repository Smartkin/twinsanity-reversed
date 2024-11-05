import numpy as np
import math as pmath

def create_vec4():
    return np.array([0, 0, 0, 0], dtype=np.float32)

def create_vec4_with_value(value):
    return np.array([value, value, value, value], dtype=np.float32)

parentJointScale = np.array(list(map(float, input("Enter parent joint scale ").split())), dtype=np.float32)
jointScale = np.array(list(map(float, input("Enter joint scale ").split())), dtype=np.float32)
jointRotation = np.array(list(map(float, input("Enter joint rotation ").split())), dtype=np.float32)

I = pmath.sqrt(2.0)
accumulator = create_vec4()
accumulator[[0, 1, 2]] = jointRotation[[0, 1, 2]] * jointRotation[[0, 1, 2]]

unkRot = jointRotation * I
unkRot2 = accumulator[[0, 1, 2]] * 2
unkVec = create_vec4_with_value(1.0)
jointRotation[[1, 2, 3]] = np.array([0, 0, 0], dtype=np.float32)
accumulator[0] = unkRot[1] * unkRot[2]
accumulator[1] = unkRot[2] * unkRot[0]
accumulator[2] = unkRot[0] * unkRot[1]

unkRot3 = create_vec4()
unkRot3[[0, 1, 2]] = accumulator[[0, 1, 2]]  -  unkRot[[0, 1, 2]]  * unkRot[3]
unkVec[3] = 0.0
unkRot4 = create_vec4()
unkRot4[[0, 1, 2]] = accumulator[[0, 1, 2]] + unkRot[[0, 1, 2]] * unkRot[3]
unkRot3[3] = 0
accumulator[[0, 1, 2]] = np.array([1.0, 1.0, 1.0], dtype=np.float32)
jointRotation[3] = 0

accumulator[[1, 2]] -= unkVec[[1, 2]] * unkRot2[0]
q = 1.0 / parentJointScale[1]
accumulator[0] -= unkVec[0] * unkRot2[1]
parentJointScale[1] = unkRot3[2]
unkRot[2] = unkRot3[0]

jointScale[3] = unkRot4[0]

unkRot3[2] = unkRot3[1]
unkRot[0] = unkRot4[1]
unkRot[2] = accumulator[2] - unkRot3[2] * unkRot2[1]
unkRot3[1] = unkRot4[2]
unkRot3[0] = accumulator[0] - unkVec[0] * unkRot2[2]
unkVec[0] = parentJointScale[1]
jointRotation[0] = q
unkVec[2] = jointScale[3]
unkVec[1] -= unkRot2[2]
q = 1 / parentJointScale[2]
parentJointScale = unkRot * jointScale[2]
unkRot2 = unkRot3 * jointScale[0]
jointScale[[0, 2, 3]] = np.array([0, 0, 0], dtype=np.float32)
unkRot = unkVec * jointScale[1]
jointScale[1] = q
unkVec[[0, 1, 3]] = np.array([0, 0, 0], dtype=np.float32)
unkVec[2] = q

unkRot3 = create_vec4()
accumulator = jointRotation * unkRot[0]
accumulator += jointScale * unkRot[1]
accumulator += unkVec * unkRot[2]
out_y_column = accumulator

accumulator = jointRotation * unkRot2[0]
accumulator += jointScale * unkRot2[1]
accumulator += unkVec * unkRot2[2]
out_x_column = accumulator

accumulator = jointRotation * parentJointScale[0]
accumulator += jointScale * parentJointScale[1]
accumulator += unkVec * parentJointScale[2]
out_z_column = accumulator

print(out_x_column)
print(out_y_column)
print(out_z_column)
