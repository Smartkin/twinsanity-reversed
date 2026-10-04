#pragma once

#include "abi.h"
#include "common.h"

struct Matrix4x4;
struct Vector2;
struct Vector4;
struct Box;
struct ChunkView;
struct RenderView;
// The platform's material: its shaders, their textures and the render bucket it's drawn in
struct Material;
struct DecalData;
struct DecalType;
// The platform's models (the RM2's graphics): rigid models, skins (their vertexes on up to three joints each) and blend skins
// (skins with shapes blended into their vertexes)
struct RigidModel;
struct Skin;
struct BlendSkin;
// The scene's screen models: strips of coloured vertexes drawn after the scene's models (the skid marks)
struct ScreenModel;
// The graphics resources the chunks share by ID (their tables are the game's, game/graphicstables.h): textures, materials, models
// (a rigid model's submodels), the scenery's levels of detail and skies
struct GameTexture;
struct MaterialResource;
struct RigidModelData;
struct Lod;
struct Sky;
class Stream;
struct ParticlePage;
struct TimeClock;
struct RenderTargetDescription;

namespace Platform::Graphics
{
// Puts the graphics hardware (the vector units and their interfaces, the path to the GS and the DMA controller) in the state the
// renderer starts from
void ResetDevices();
// Resets the path to the GS again, which the renderer does when it sets up the display
void ResetPath();
// Waits for the next vertical blank
void WaitVSync();
// Whether the TV takes PAL (the video mode the renderer set the display up in at start-up; NTSC's pixels are narrower)
bool IsPalDisplay();
// Waits until the graphics hardware has drawn everything it was sent (with the processor's cache written back first)
void WaitIdle();

// The renderer started for frames of a height on a PAL TV (NTSC otherwise), up to its display: on the PS2 its DMA memory taken
// and the GS's display set up for a frame 512 pixels wide. Then the rest once the display has its place: on the PS2 the VU1
// programs, the DMA chains and their buckets, the texture slots, the instance blocks, the default materials, the alpha presets,
// the screen effects and the packet the materials call to draw on the whole screen
void StartRenderer(s32 height, bool pal);
void FinishRendererStart();
// The display moved on the TV by an offset between -1 and 1 each way (on the PS2 up to 256 of the display's units across and 32
// lines down)
void MoveDisplay(const Vector2* offset);
// The renderer's animations stepped by the game's clock (its materials' shaders and the particles' waves)
void StepAnimations(const TimeClock* clock);
// A render target's frame worked out: its matrix from the clip space to the drawing's coordinates (the GS's: 16ths of a pixel,
// the screen's middle at 2048, depths from 2^23 - 1 down to 0) and its corner there, and its packet setting the drawing up for
// it
void SetUpRenderTarget(RenderTargetDescription* target) RETAIL(FUN_0019f720);

// Shows the frame drawn and starts drawing the next: waits until the hardware is idle and for the vertical blank, sends the
// commands (nullptr: none, the screen keeps its frame) and sets the display up again (but on the first frame). The commands are
// the renderer's chain of the frame's render buckets (on the PS2 a DMA chain for the GIF)
void Present(const void* commands);
// The same from the vertical blank's interrupt (the movies present their pictures from it): the commands always sent
void PresentFromInterrupt(const void* commands);
// Waits until the commands sent have been taken and drawn
void WaitSent();

// The frame's 28 render buckets, drawn in their order: they start over empty (while a movie plays, small ones: the memory the
// frame's are in is the movie's then), or the frame's are sent to be drawn and the next frame's start (from the vertical blank's
// interrupt or not)
void ResetBuckets(bool movie);
void SubmitBuckets(bool fromInterrupt);
// Memory for count items of a size that lasts until the frame is drawn (a skin's joints' matrices; on the PS2, taken from the
// frame's DMA buffer, where the draws find it)
void* AllocFrameMemory(u32 count, u32 size);
// After a frame was submitted and its picture queued (on the PS2, the VU1 programs loaded for materials are forgotten)
void FinishFrame();
// The scene's drawing finished: the screen effects drawn when asked, then what the scene's models and materials gathered in the
// frame put into the buckets (on the PS2, the instance blocks closed, the materials' draws put after their set-ups, the texture
// slots let go of)
void FinishScene(bool effects);

// The frame's start, at the head of the first bucket: drawing set up for the frame (its corner in the drawing's coordinates, which
// have the screen's middle at 2048, its size and the screen's) and the frame cleared to the color when clearColor, its depth when
// clearDepth
struct FrameStart
{
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 screenWidth;
    s32 screenHeight;
    u8 red;
    u8 green;
    u8 blue;
    bool clearColor;
    bool clearDepth;
};
void StartFrame(const FrameStart& frame);
// The helper programs of the vector processor the game's maths runs on (the PS2's VU0 microcode sets: 1 the standard one, the
// sines and cosines among them), waiting until they're loaded when asked (else they're loaded while the game goes on). Nothing
// elsewhere
void UseHelperPrograms(u32 set, bool wait);

// The UI's flat material, made: untextured shapes in their own colours in the UI's render bucket; and that material
Material* MakeFlatMaterial();
Material* FlatMaterial();

// A material kept in storage of the game's (OLEG keeps one inside itself, MaterialStorage bytes) made without shaders, and
// destroyed (its shaders deleted)
constexpr u32 MaterialStorage = 0x70;
void ConstructMaterial(Material* material);
void DestroyMaterial(Material* material);
// The UI's 2D particles' material made in such storage: a shader of its own in the UI's render bucket, the flat material's kind,
// drawing the first particle page's picture blended (the flat material's shader set to white again on the way). Returns its
// shader, an opaque handle
void* MakeParticleMaterial(Material* material);
// The particles' distorting hexagons' material made in its storage: a shader of its own copying the frame first (the PS2's type
// 0x18) in the render bucket after the particles', blended, testing depth GREATER without writing it
void MakeDistortionMaterial(Material* material);
// The particles' blocks of an emitter made one chain to draw (each going on to the next, the last ending it), and a block made a
// chain of its own
void ChainParticleBlocks(u8* const* blocks, s32 count);
void EndParticleBlock(u8* block);
// The sizes of the particles' blocks of 32 particles and of 12 hexagons and of a system's render table, with what the platform puts
// around the particles and the table, which it makes once when they're allocated
constexpr u32 ParticleBlockBytes = 0x430;
constexpr u32 HexagonBlockBytes = 0x1B0;
constexpr u32 ParticleRenderTableBytes = 0x890;
void InitParticleBlock(u8* block, bool hexagons);
void InitParticleRenderTable(u8* table);
// What a system's particles look like over their life, for its render table: the gravity, the texture's rectangle (start and end
// corners in sixteenths of a pixel), the six corners of the distorting hexagons (their directions a quarter long, none for other
// systems), and at each of 64 steps of the life the quad's shape (two corners and the edge between them, the widths stretched
// by the screen's aspect) and its colour and alpha (128 leaves the texture as it is)
constexpr u32 ParticleRenderSteps = 64;
struct ParticleLook
{
    f32 gravity;
    s32 texture[4];
    bool hexagons;
    f32 hexagonCorners[12];
    struct Step
    {
        f32 shape[6];
        u8 colour[4];
    } steps[ParticleRenderSteps];
};
// The particles' graphics made at start-up: the effects' disk node none, the wave shader (the PS2's type 0x1C, speed 1, amplitude
// 1.5) and the distortion's material
void InitParticleGraphics();
// The look written into a render table (made by InitParticleRenderTable)
void WriteParticleRenderTable(u8* table, const ParticleLook& look);
// A block of particles drawn with its system's render table through a matrix (ParticleBlockView's) in a material at a time, and a
// block of distorting hexagons with the distortion's material and the system's distortion
void DrawParticleBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time);
void DrawDistortionBlock(u8* block, u8* table, const Matrix4x4* matrix, Material* material, f32 time, f32 distortionX, f32 distortionY);
// The decals' aging for the frame (the PS2's VU0 microprograms of its third set: 0x1C0, or 0x360 for decals placed in another chunk
// than the camera's, which takes their places there through that chunk's matrix): the camera's chunk's view (the matrices it
// starts with) and the matrix of the chunk the decals are in when it's another, then each type's variants, then each decal by a
// variant of the type: its place (w its life left in seconds) and frame (its normal's and direction's half words as words, 32768 a
// unit) give its place now, its colour (bytes) and two sets of four half words of its corners' offsets
void LoadDecalView(const Matrix4x4* chunkMatrices, const Matrix4x4* fromChunk);
void LoadDecalType(DecalType* type);
// The frame's decals drawn, each type's with its material (the lists emptied)
void DrawDecals(DecalData* decals);
struct DecalLook
{
    f32 place[4];
    u8 colour[4];
    s16 sizes[4];
    s16 moreSizes[4];
};
void AgeDecal(const f32* place, const s32* frame, s32 variant, DecalLook* look);
// The chunks' culling (on the PS2 VU0's microprograms 0x1C0, 0x2D0, 0x398 and 0x500 of its second set, the view in its memory, and
// its macro mode for the boxes' corners): the camera's frustum from its lens (in the camera's space: the planes the chunks are
// culled by and a set with the sides five times as far out); a chunk's planes loaded from the camera's place in it, or a linked
// chunk's through the planes of the link's portal (its load wall); the view's matrices, the camera and the link between the
// chunks; a model's matrix, a scenery cell's box (its two corners) or a box under a model's matrix tested. Its outcome: whether
// it's out of the view, whether it's partly out, the level of detail's distance, and the matrices to the clip space when it's
// partly out and to the screen
void SetViewFrustum(f32 near, f32 far, f32 aspect, const s32* fieldOfView) RETAIL_N32(FUN_001ef410);
void LoadChunkPlanes(const Matrix4x4* place);
void LoadPortalPlanes(const Matrix4x4* place, const Vector4* portal);
void LoadCullingView(const Matrix4x4* toClip, const Matrix4x4* toScreen, const Vector4* camera);
void CullLoadLink(const Matrix4x4* chunk, const Matrix4x4* object);
void CullLoadModel(const Matrix4x4* model);
void CullTestBox(const Vector4* corners);
void CullTestBoxAt(const Box* box, const Matrix4x4* model);
struct CullOutcome
{
    u32 outside;
    u32 clipped;
    s32 distance;
};
CullOutcome CullResult();
void CullMatrices(Matrix4x4* clipped, Matrix4x4* toScreen);
// A box's corners clipped through a view's matrix (and a model's), scaled as well by the row after it: every corner's outside
// bits taken together (the scaled ones in the low six) and their common ones (the unscaled ones in bits 6-11)
void ClipBox(const Matrix4x4* view, const Box* box, u32* flags) RETAIL(FUN_00201d0c);
// The camera's frustum's six planes in its space (near, the four sides, far)
const Vector4* ViewPlanes();
void ClipBoxAt(const Matrix4x4* view, const Box* box, u32* flags, const Matrix4x4* model) RETAIL(FUN_00201c68);
// A scenery mesh (or a dynamic scenery model) drawn through a chunk's view at a matrix (on the PS2 into a block of placed models
// with the matrices of the view's last culling test and how it's clipped), and a level of detail's mesh for a squared distance
// (none nearer than its nearest or past its farthest)
void DrawPlacedModel(RigidModel* model, ChunkView* view, const Matrix4x4* world);
RigidModel* LodModelAt(Lod* lod, u32 distance);
// A chunk's view for the frame's particles (the matrices it starts with: its camera's place in it among them) loaded as the
// frame's index'th, which their distances from the camera and their drawing go by. Returns it, an opaque handle (the PS2 also
// keeps it in the word 0x100 bytes past the matrices)
s32 LoadParticleView(const Matrix4x4* matrices, s32 index);

// A font page's material told which page of its font it draws (the text drawing picks its glyphs by it)
void SetFontPage(Material* material, u32 page);

// The UI's 2D drawing, over the frame in the material's render bucket with its first shader (and that shader's texture). Colours
// are RGBA bytes (128 leaves the texture's colour as it is). A rectangle is a corner and the size from it
struct Rectangle
{
    f32 x;
    f32 y;
    f32 width;
    f32 height;
};

// A textured rectangle, its place in fractions of the screen (the frame's corner 0, its size 1), the texture's area across it in
// fractions of the texture: the place's corner shows the area's corner, the opposite corners match
void DrawSprite(Material* material, u32 colour, const Rectangle& place, const Rectangle& texture);
// The same turned: the place's four corners in fractions of the screen from the screen's own corner (the retail game leaves the
// frame's offset out of these), showing the texture area's corner, the corner a height from it, the corner a width from it and
// the opposite one
void DrawTurnedSprite(Material* material, u32 colour, const Vector2* corners, const Rectangle& texture);
// A triangle strip of coloured vertexes without a texture, its places in pixels from the frame's corner
void DrawStrip(Material* material, u32 count, const Vector2* places, const u32* colours);

// What a model is lit by: the three strongest directional lights at it (their directions in the world, a row each, and their
// colours) and the ambient light
struct ModelLights
{
    const Vector4* directions;
    const Vector4* colours;
    const Vector4* ambient;
};

// The scene's models, their matrices in the world, drawn as the view test found them (mode 1: wholly in view, drawn without
// clipping; 2: crossing the view's sides). A skin's joints' matrices are each joint's inverse bind pose times its matrix in the
// model's space, in memory that lasts until the frame is drawn (AllocFrameMemory); a blend skin blends the shapes given in by
// their weights
void DrawRigidModel(RigidModel* model, const Matrix4x4* matrix, const ModelLights& lights, u32 mode);
void DrawSkin(Skin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix, const ModelLights& lights, u32 mode);
void DrawBlendSkin(BlendSkin* skin, const Matrix4x4* joints, u32 jointCount, const Matrix4x4* matrix, const ModelLights& lights,
                   const f32* weights, const s32* shapes, s32 shapeCount, u32 mode);
// The shadows' pass made ready when the default chunk is loaded: its texture slot and the packet uploading its palette (alpha
// from 255 down, the colour white)
void SetUpShadowPass();
// The shadows' pass begun (the frame's alpha and depth shrunk into the half-size buffer) and ended (the shadows drawn into it
// taken away from the screen)
void BeginShadows();
void EndShadows();
// A shadow's volume: a default mesh through the matrices to the shadows' half-size screen, to the camera and to the world
void DrawShadowMesh(RigidModel* mesh, const Matrix4x4* toScreen, const Matrix4x4* toCamera, const Matrix4x4* world);
// The scene's screen models (the vehicles' skid marks): a triangle strip of coloured vertexes in a material, built a vertex at a
// time. Begun (the default material, no vertexes), its material set (none keeps it), the colour of the vertexes that follow
// (RGBA bytes), a vertex (its x, y and z) and ended: the model. Deleted with what it holds, and drawn clipped to the view's sides
// through the matrices to the screen and to the clip space (the view's clip vector's)
void BeginScreenModel();
void SetScreenModelMaterial(Material* material);
void SetScreenModelColour(u32 colour);
void AddScreenModelVertex(const Vector4* place);
ScreenModel* EndScreenModel();
void DeleteScreenModel(ScreenModel* model);
void DrawScreenModel(ScreenModel* model, const Matrix4x4* toScreen, const Matrix4x4* toClip, const Vector4* clip);
// A skid marks' material made: no texture, its vertexes' colours added to the frame or taken away from it by their alpha where
// the frame's alpha passes the destination test, its depth not written, drawn with the opaque objects
Material* MakeSkidMaterial(bool subtracting);
// The animations of the shaders of a model's materials started again (as many of a rigid model's as both it and its model have
// submodels)
void RestartAnimations(RigidModel* model);
void RestartAnimations(Skin* skin);
void RestartAnimations(BlendSkin* skin);
// A rigid model's first material (nullptr when it has none)
MaterialResource* FirstMaterial(const RigidModel* model);

// The graphics resources, each kind made with its ID (no references, nothing read), deleted with what it holds (its references of
// other resources let go of) and read from a chunk's file (taking references of the resources it uses through their tables). The
// meshes are rigid models of the scenery's
GameTexture* NewTexture(u32 id);
void DeleteTexture(GameTexture* texture);
void ReadTexture(GameTexture* texture, Stream* stream);
MaterialResource* NewMaterial(u32 id);
void DeleteMaterial(MaterialResource* material);
void ReadMaterial(MaterialResource* material, Stream* stream);
RigidModelData* NewModel(u32 id);
void DeleteModel(RigidModelData* model);
void ReadModel(RigidModelData* model, Stream* stream);
RigidModel* NewRigidModel(u32 id);
void DeleteRigidModel(RigidModel* model);
void ReadRigidModel(RigidModel* model, Stream* stream);
Skin* NewSkin(u32 id);
void DeleteSkin(Skin* skin);
void ReadSkin(Skin* skin, Stream* stream);
BlendSkin* NewBlendSkin(u32 id);
void DeleteBlendSkin(BlendSkin* skin);
void ReadBlendSkin(BlendSkin* skin, Stream* stream);
RigidModel* NewMesh(u32 id);
void DeleteMesh(RigidModel* mesh);
void ReadMesh(RigidModel* mesh, Stream* stream);
Lod* NewLod(u32 id);
void DeleteLod(Lod* lod);
void ReadLod(Lod* lod, Stream* stream);
Sky* NewSky(u32 id);
void DeleteSky(Sky* sky);
void ReadSky(Sky* sky, Stream* stream);
// A chunk's sky drawn first in the frame instead of clearing it (on the PS2 into a half-size buffer copied over the screen)
void DrawChunkSky(Sky* sky, RenderView* view);

// A particle texture page (game/particles.h) loaded from its file, its texture and material read after their IDs unless their
// tables have them, or read from a chunk's stream, its material then taken from the material table; and its blend modes'
// materials made from the material's first shader (the decals' page's draw with STQ coordinates)
void LoadParticlePage(ParticlePage* page, const char* path, bool decals);
void ReadParticlePage(ParticlePage* page, Stream* stream, bool decals);
}
