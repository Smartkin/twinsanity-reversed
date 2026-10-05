#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "game/reference.h"

struct CameraLensNode;
struct RenderTargetDescription;

// What the scene being drawn is seen with (the game controller's view, 0x170 bytes, a render target's): the projection (camera to
// clip space), the world to camera matrix (the camera's place inverted), the world to clip space one (the VU1 programs clip by it),
// the world to screen one and the camera to screen one (the render target's screen matrix after them), a vector VU1 clips by (its
// x and y 1 over the guard band), the camera's forward direction in the world, the camera (a reference: its lens is its node of
// kind 9) and 1 over the aspect the projection was made for
struct RenderView
{
    Matrix4x4 projection;
    Matrix4x4 toCamera;
    Matrix4x4 toClip;
    Matrix4x4 toScreen;
    Matrix4x4 cameraToScreen;
    Vector4 clip;
    Vector4 forward;
    Reference* cameraObject;
    f32 inverseAspect;
    u8 unused168[0x170 - 0x168];

    // Looking down the Z axis (its projection and world to clip matrices the identity, the clip vector the guard band's), with no
    // camera
    static RenderView* Construct(RenderView* view) RETAIL(FUN_0026ef30);
    // The projection made for a lens (its field of view, near and far planes; the aspect the view keeps), the lens told
    void SetProjection(CameraLensNode* lens) RETAIL(FUN_0026efa8);
    // Taken from its camera for a screen's aspect: the world to camera matrix and the forward direction, and the projection made
    // again when the lens asks (or, as retail has it, when the aspect times the lens's pixel aspect is the one over it it kept)
    void Update(f32 aspect) RETAIL_N32(FUN_0026f0c8);
    // The matrices that follow from the projection and the world to camera one, for a render target
    void MakeMatrices(const RenderTargetDescription* target) RETAIL(FUN_0027b640);
};
CHECK_OFFSET(RenderView, clip, 0x140);
CHECK_OFFSET(RenderView, cameraObject, 0x160);
CHECK_SIZE(RenderView, 0x170);

extern "C"
{
    // The scene's view and its render target
    extern RenderView* g_RenderView RETAIL(G_RendRel);
    extern RenderTargetDescription* g_RenderTarget RETAIL(G_FontRendererRel);
    // The instance of the camera the renderer's view looks through (none: nullptr)
    struct InstanceContext* CameraInstance() RETAIL(FUN_002098a0);
    // How far past the screen's edges, in screens, the VU1 programs let things be before they clip them (5 either way)
    extern f32 g_GuardBandX RETAIL(D_0030A1D4);
    extern f32 g_GuardBandY RETAIL(D_0030A1D8);
}
