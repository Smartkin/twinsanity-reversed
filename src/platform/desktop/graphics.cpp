#include "platform/graphics.h"

#include "clock.h"
#include "window.h"

#include "game/context.h"
#include "game/font.h"
#include "game/particles.h"
#include "game/renderer.h"
#include "game/resources.h"

#include <algorithm>

// The desktop's side of Platform::Graphics: the window (window.cpp) showing every frame's clear color, nothing drawn yet, the
// frames paced like the display's
namespace
{
u8 g_ClearColor[3] = {};
}

namespace Platform::Graphics
{
void ResetDevices()
{
}

void ResetPath()
{
}

// The next of the display's 60 refreshes a second (IsPalDisplay's NTSC), counted from the last one waited for, and then the
// window's events: closing it ends Main's loop after the frame
void WaitVSync()
{
    constexpr u64 Refresh = 1000000000 / 60;
    static u64 next = DesktopClock::Now();
    next = std::max(next + Refresh, DesktopClock::Now());
    DesktopClock::SleepUntil(next);
    if (DesktopWindow::HandleEvents())
    {
        g_GameState = GameStateQuitting;
    }
}

bool IsPalDisplay()
{
    return false;
}

void WaitIdle()
{
}

void StartRenderer(s32 height, bool pal)
{
    DesktopWindow::Open();
}

void FinishRendererStart()
{
}

void MoveDisplay(const Vector2* offset)
{
}

void StepAnimations(const TimeClock* clock)
{
}

void SetUpRenderTarget(RenderTargetDescription* target)
{
}

void Present(const void* commands)
{
    WaitVSync();
    DesktopWindow::Show(g_ClearColor[0], g_ClearColor[1], g_ClearColor[2]);
}

void PresentFromInterrupt(const void* commands)
{
}

void WaitSent()
{
}

void ResetBuckets(bool movie)
{
}

void SubmitBuckets(bool fromInterrupt)
{
}

void* AllocFrameMemory(u32 count, u32 size)
{
    return nullptr;
}

void FinishFrame()
{
}

void FinishScene(bool effects)
{
}

void StartFrame(const FrameStart& frame)
{
    if (frame.clearColor)
    {
        g_ClearColor[0] = frame.red;
        g_ClearColor[1] = frame.green;
        g_ClearColor[2] = frame.blue;
    }
}

void UseHelperPrograms(u32 set, bool wait)
{
}

Material* MakeFlatMaterial()
{
    return nullptr;
}

Material* FlatMaterial()
{
    return nullptr;
}

void ConstructMaterial(Material* material)
{
}

void DestroyMaterial(Material* material)
{
}

void* MakeParticleMaterial(Material* material)
{
    return nullptr;
}

void MakeDistortionMaterial(Material* material)
{
}

void ChainParticleBlocks(u8* const* blocks, s32 count)
{
}

void EndParticleBlock(u8* block)
{
}

void InitParticleBlock(u8* block, bool hexagons)
{
}

void InitParticleRenderTable(u8* table)
{
}

void InitParticleGraphics()
{
}

void WriteParticleRenderTable(u8* table, const ParticleLook& look)
{
}

void DrawParticleBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time)
{
}

void DrawDistortionBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time, f32 distortionX, f32 distortionY)
{
}

void DrawDecals(DecalData* decals)
{
}

void DrawPlacedModel(RigidModel* model, ChunkView* view, const Matrix4x4* world)
{
}

RigidModel* LodModelAt(Lod* lod, u32 distance)
{
    return nullptr;
}

void SetFontPage(Material* material, u32 page)
{
}

void DrawSprite(Material* material, u32 colour, const Rectangle& place, const Rectangle& texture)
{
}

void DrawTurnedSprite(Material* material, u32 colour, const Vector2* corners, const Rectangle& texture)
{
}

void DrawStrip(Material* material, u32 count, const Vector2* places, const u32* colours)
{
}

void DrawRigidModel(RigidModel* model, const Matrix4x4* matrix, const ModelLights& lights, u32 mode)
{
}

void DrawSkin(Skin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix, const ModelLights& lights, u32 mode)
{
}

void DrawBlendSkin(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix, const ModelLights& lights, const f32* weights, const s32* shapes, s32 shapeCount, u32 mode)
{
}

void SetUpShadowPass()
{
}

void BeginShadows()
{
}

void EndShadows()
{
}

void DrawShadowMesh(RigidModel* mesh, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world)
{
}

void BeginScreenModel()
{
}

void SetScreenModelMaterial(Material* material)
{
}

void SetScreenModelColour(u32 colour)
{
}

void AddScreenModelVertex(const Vector4* place)
{
}

ScreenModel* EndScreenModel()
{
    return nullptr;
}

void DeleteScreenModel(ScreenModel* model)
{
}

void DrawScreenModel(ScreenModel* model, const Matrix4x4* toScreen, const Matrix4x4* toClip, const Vector4* clip)
{
}

Material* MakeSkidMaterial(bool subtracting)
{
    return nullptr;
}

void RestartAnimations(RigidModel* model)
{
}

void RestartAnimations(Skin* skin)
{
}

void RestartAnimations(BlendSkin* skin)
{
}

MaterialResource* FirstMaterial(const RigidModel* model)
{
    return nullptr;
}

GameTexture* NewTexture(u32 id)
{
    return nullptr;
}

void DeleteTexture(GameTexture* texture)
{
}

void ReadTexture(GameTexture* texture, Stream* stream)
{
}

MaterialResource* NewMaterial(u32 id)
{
    return nullptr;
}

void DeleteMaterial(MaterialResource* material)
{
}

void ReadMaterial(MaterialResource* material, Stream* stream)
{
}

RigidModelData* NewModel(u32 id)
{
    return nullptr;
}

void DeleteModel(RigidModelData* model)
{
}

void ReadModel(RigidModelData* model, Stream* stream)
{
}

RigidModel* NewRigidModel(u32 id)
{
    return nullptr;
}

void DeleteRigidModel(RigidModel* model)
{
}

void ReadRigidModel(RigidModel* model, Stream* stream)
{
}

Skin* NewSkin(u32 id)
{
    return nullptr;
}

void DeleteSkin(Skin* skin)
{
}

void ReadSkin(Skin* skin, Stream* stream)
{
}

BlendSkin* NewBlendSkin(u32 id)
{
    return nullptr;
}

void DeleteBlendSkin(BlendSkin* skin)
{
}

void ReadBlendSkin(BlendSkin* skin, Stream* stream)
{
}

RigidModel* NewMesh(u32 id)
{
    return nullptr;
}

void DeleteMesh(RigidModel* mesh)
{
}

void ReadMesh(RigidModel* mesh, Stream* stream)
{
}

Lod* NewLod(u32 id)
{
    return nullptr;
}

void DeleteLod(Lod* lod)
{
}

void ReadLod(Lod* lod, Stream* stream)
{
}

Sky* NewSky(u32 id)
{
    return nullptr;
}

void DeleteSky(Sky* sky)
{
}

void ReadSky(Sky* sky, Stream* stream)
{
}

void DrawChunkSky(Sky* sky, RenderView* view)
{
}

void LoadParticlePage(ParticlePage* page, const char* path, bool decals)
{
}

void ReadParticlePage(ParticlePage* page, Stream* stream, bool decals)
{
}

}

// A font's glyphs drawn (its vtable's slots, the PS2 side's renderer/text.cpp)
void Font::Begin()
{
}

void Font::Draw(const TextItem*, TextPackets*)
{
}

void Font::End(TextPackets*)
{
}

// The shadows' models and meshes, which the shadow code makes and reads itself
extern "C"
{
    RigidModelData* ModelConstruct(void* memory, u32 id) RETAIL(InitGameModel);
    void ModelRead(RigidModelData* model, Stream* stream) RETAIL(ReadGameModel);
    RigidModel* MeshConstruct(void* memory, u32 id) RETAIL(FUN_001c1e70);
    void MeshRead(RigidModel* mesh, Stream* stream) RETAIL(ReadRigidModel2);
}

RigidModelData* ModelConstruct(void* memory, u32 id)
{
    ConstructResourceHeader(memory, id);
    return static_cast<RigidModelData*>(memory);
}

void ModelRead(RigidModelData*, Stream*)
{
}

RigidModel* MeshConstruct(void* memory, u32 id)
{
    ConstructResourceHeader(memory, id);
    return static_cast<RigidModel*>(memory);
}

void MeshRead(RigidModel*, Stream*)
{
}

// The PS2 renderer's modules' static constructors, which the retail list of them (G_UnkFunTableSize's) names: the renderer's
// makes the game's statics it holds (no particle section's file, the screens' offsets none, the models' update rate), the VU
// programs' have nothing to make here
extern "C"
{
    void RendererStaticInit() RETAIL(FUN_001a6678);
    void InitVuProgramsModule() RETAIL(FUN_001dd4d0);
    void ConstructVu0ProgramsModule() RETAIL(FUN_002b2230);
}

void RendererStaticInit()
{
    g_ParticleSectionFile = -1;
    g_PalScreenOffset.x = 0.0f;
    g_PalScreenOffset.y = 0.0f;
    g_NtscScreenOffset.x = 0.0f;
    g_NtscScreenOffset.y = 0.0f;
    // The model nodes are never left without updates
    g_ModelUpdateRate.cutoff = UpdateRate::NoCutoff;
    g_ModelUpdateRate.slope = 1.0f;
    g_ModelUpdateRate.grace = 0;
}

void InitVuProgramsModule()
{
}

void ConstructVu0ProgramsModule()
{
}
