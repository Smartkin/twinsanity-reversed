#pragma once

#include "common.h"

struct Matrix4x4;
struct Vector4;

// The desktop's view frustum, which culling.cpp keeps (the planes VU0's memory has on the PS2) and decals.cpp takes too
namespace DesktopGraphics
{
// The far set's side planes and the near plane taken into a chunk's space
void ChunkViewPlanes(const Matrix4x4* matrix, Vector4* sides, Vector4* nearPlane);
}
