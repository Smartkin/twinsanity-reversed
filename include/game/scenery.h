#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/archive.h"
#include "game/math.h"
#include "game/pools.h"

class Stream;
struct ChunkData;
struct ChunkList;
struct ChunkLinkData;
struct InstanceContext;
struct InstanceQuery;
struct ObjectPlace;
struct RenderView;
struct RigidModel;
struct Lod;
struct BoundingVolume;
struct CollisionHull;
struct ReaderStack;
struct ObjectCollision;

// What a chunk's things are drawn and culled through (0x110 bytes, its vtable 0x104 bytes in): the renderer view's matrix to the
// clip space, the matrices VU0's culling worked out for the last model (to the clip space when it's partly out, to the screen),
// the view's clip vector, where the camera is (in the chunk), the level of detail's distance of the last test, the view, the last
// tests' outcome (Visibility) and the chunk's matrices. Its vtable's functions: 1 the destructor, 2 a box tested, 3 a box under a
// matrix tested, 4 a point tested, 5 a matrix taken into the chunk's space, 6 whether it's a linked chunk's, 7 and 8 1 and 0
// (nothing calls them), 9 whether a direction faces the camera's way, 10 the view loaded into VU0, 11 a model's matrix loaded,
// 12 a scenery cell's box tested
struct alignas(16) ChunkView
{
    // What a test found (a scenery cell's visibility is also how its children are drawn and collected: all of them as they are
    // when it's wholly in view, each tested when it's partly)
    enum Visibility : u32
    {
        OutOfView = 0,
        InView = 1,
        PartlyInView = 2,
    };

    // Its vtable's slots the scenery calls
    enum Slot : u32
    {
        SlotTestBox = 2,
        SlotTestBoxAt = 3,
        SlotLoad = 10,
        SlotLoadModel = 11,
        SlotTestCell = 12,
    };

    // The chunk's matrices (ChunkData's matrix, toScreen, drawMatrix and drawInverse)
    enum MatrixIndex : u32
    {
        CameraMatrix = 0,
        ToScreenMatrix = 1,
        DrawMatrix = 2,
        DrawInverseMatrix = 3,
    };

    Matrix4x4 toClip;
    // The first row is also what the box test scales the corners by for its second clip test
    Matrix4x4 clipped;
    Matrix4x4 toScreen;
    Vector4 clip;
    Vector4 origin;
    Vector4 camera;
    s32 distance;
    RenderView* view;
    u32 visibility;
    u32 lastVisibility;
    // The chunk's matrices: the camera's place in it, its view to the screen, its draw matrix and that one's inverse
    Matrix4x4* matrices;
    const GccVTableEntry* vtable;

    // The renderer's view, or one given (the clip vector the view's, 0.2, 0.2, 1, 1 without one)
    static ChunkView* Construct(ChunkView* view) RETAIL(FUN_001f62f0);
    static ChunkView* ConstructFor(ChunkView* view, RenderView* renderView) RETAIL(FUN_001f6330);
    void Destroy(u32 flags) RETAIL(FUN_001f6368);
    void UseRendererView() RETAIL(FUN_001f6398);
    void Reset() RETAIL(FUN_001f63b0);
    void TakeViewMatrix() RETAIL(FUN_001f66a8);

    void TestBox(const Box* box) RETAIL(FUN_001f65e0);
    void TestBoxAt(const Box* box, const Matrix4x4* matrix) RETAIL(FUN_001f6640);
    void TestPoint(const Vector4* point) RETAIL(FUN_001f5520);
    const Matrix4x4* ChunkMatrix(const Matrix4x4* matrix) RETAIL(FUN_001f5888);
    u32 IsLinked() RETAIL(FUN_001f5890);
    u32 Unused7() RETAIL(FUN_001f58a0);
    u32 Unused8() RETAIL(FUN_001f58a8);
    u32 Faces(const Vector4* direction) RETAIL(FUN_001f58b0);
    void Load() RETAIL(FUN_001f55e0);
    void LoadModel(const Matrix4x4* model) RETAIL(FUN_001f6700);
    void TestCell(const struct SceneryCell* cell) RETAIL(FUN_001f6720);

    // The outcome of VU0's last test: the cell's (its visibility), a mesh's (its distance as well, and the matrices to draw it)
    // and the drawn model's (the matrices and the distance)
    u32 CellResult() RETAIL(FUN_001f6740);
    u32 MeshResult() RETAIL(FUN_001f67b8);
    u32 InstanceResult() RETAIL(FUN_001f6838);
    void DrawnModelResult() RETAIL(FUN_001f66d8);
    // A box under a matrix tested by VU0
    void TestBoxOnVu0(const Box* box, const Matrix4x4* matrix) RETAIL(FUN_001f6778);

    void VirtualTestBox(const Box* box)
    {
        CallVirtual<void>(this, vtable, SlotTestBox, box);
    }

    void VirtualLoad()
    {
        CallVirtual<void>(this, vtable, SlotLoad);
    }

    void VirtualLoadModel(const Matrix4x4* model)
    {
        CallVirtual<void>(this, vtable, SlotLoadModel, model);
    }

    void VirtualTestCell(const struct SceneryCell* cell)
    {
        CallVirtual<void>(this, vtable, SlotTestCell, cell);
    }
};
CHECK_OFFSET(ChunkView, clip, 0xC0);
CHECK_OFFSET(ChunkView, distance, 0xF0);
CHECK_OFFSET(ChunkView, vtable, 0x104);
CHECK_SIZE(ChunkView, 0x110);

// A linked chunk's view (0x150 bytes): the link's matrix taking things into the first chunk's space. Its own vtable's functions: 1
// the destructor, 2 and 3 the boxes, 4 the point and 5 a matrix taken through the link, 6 1, 13 whether a direction taken through
// it faces the camera's way
struct LinkedChunkView : ChunkView
{
    Matrix4x4 link;

    static LinkedChunkView* Construct(LinkedChunkView* view, const Matrix4x4* chunkMatrix, const Matrix4x4* objectMatrix)
        RETAIL(FUN_001f5748);
    void Destroy(u32 flags) RETAIL(FUN_001f6430);
    void TestBox(const Box* box) RETAIL(FUN_001f64a0);
    void TestBoxAt(const Box* box, const Matrix4x4* matrix) RETAIL(FUN_001f64c0);
    void TestPoint(const Vector4* point) RETAIL(FUN_001f6510);
    Matrix4x4* ChunkMatrix(const Matrix4x4* matrix) RETAIL(FUN_001f6460);
    u32 IsLinked() RETAIL(FUN_001f6498);
    u32 Faces(const Vector4* direction) RETAIL(FUN_001f6548);
};
CHECK_SIZE(LinkedChunkView, 0x150);

// The meshes and levels of detail of a scenery cell (0x10 bytes): how many of each, a box per item (the minimum's w the radius
// around its middle), the items (the meshes first) and a matrix per item
struct SceneryMeshes
{
    // Its type in the SM2 (a cell without meshes has NoTypeId)
    static constexpr s32 TypeId = 0x1613;
    static constexpr s32 NoTypeId = 3;

    u16 meshCount;
    u16 lodCount;
    Box* boxes;
    void** items;
    Matrix4x4* matrices;

    static SceneryMeshes* Construct(SceneryMeshes* meshes, s16 meshCount, s16 lodCount) RETAIL(FUN_001f3320);
    static SceneryMeshes* ConstructRead(SceneryMeshes* meshes, Stream* stream) RETAIL(FUN_001f33d8);
    void Destroy(u32 flags) RETAIL(FUN_001f3408);
    void Read(Stream* stream) RETAIL(FUN_001f34a8);
    // The counts, boxes and IDs read, the items taken from the meshes' and the LODs' tables
    void ReadItems(Stream* stream) RETAIL(FUN_001e9f48);
    // Every item let go of
    void Release() RETAIL(FUN_001efd08);
    void SetMesh(RigidModel* mesh, s32 index) RETAIL(FUN_001efcd0);
    void SetLod(Lod* lod, s32 index) RETAIL(FUN_001efce8);
    void SetMatrix(const ObjectPlace* place, s32 index) RETAIL(FUN_001f3478);
    void SetBox(f32 extra, const Box* box, s32 index) RETAIL_N32(FUN_001e9e90);
    // Drawn without culling (the view's outcome taken as in view) and culled a box at a time: how many items were
    u32 Draw(ChunkView* view) RETAIL(RenderDynamicScenery_);
    u32 DrawCulled(ChunkView* view) RETAIL(FUN_001ee8c0);
};
CHECK_SIZE(SceneryMeshes, 0x10);

// The instances a scenery collection gathers (0x1C bytes): two pools of instances and two of cells (the wholly inside ones
// first), the flags an instance has all of and none of to be gathered, and a word of the caller's (0) nothing reads
struct InstanceCollector
{
    enum Pool : u32
    {
        WhollyInsidePool = 0,
        PartlyInsidePool = 1,
        PoolCount = 2,
    };

    // A walk's mask of both pools
    static constexpr u32 BothPools = 1 << WhollyInsidePool | 1 << PartlyInsidePool;

    ItemPools<InstanceContext*> instances;
    ItemPools<SceneryCell*> cells;
    u32 wantedFlags;
    u32 unwantedFlags;
    u32 unused18;
};
CHECK_SIZE(InstanceCollector, 0x1C);

// A cell of a chunk's scenery (0x50 bytes; TT Lab's scenery types, an octree whose cells hold what's in them): its box, the
// instances in it (dynamic scenery instances, ReferencedObjectFlags::dynamicScenery, on a list of their own; linked through
// their previous and next links, which link a sleeping instance into its chunk's list), its parent, its meshes, its chunk and
// its lights' bits (a bit per ChunkLights reference). Its vtable's functions (the base's; the tree's add what children need):
//  1 destructor, 2 release (the instances let go of now, or queued with the cell's destruction), 3 drawn, 4 its instances
//  collected, 5 whether it's a leaf, 6 its cells with instances collected, 7 the cell a box goes in, 8 a child taken out, 9 its
//  chunk set, 10 collected against a volume, 11 its contents drawn, 12 culled, 13 its meshes let go of (or released), 14 to 16
//  the root's collection and the cells of an instance and a box (nothing and none in the others), 17 0.0 (nothing calls it), 18
//  its depth (-1 a leaf), 19 its instances added to a query, 20 whether it's empty, 21 whether it holds instances, 22 and 23 its
//  instances put to sleep and released by a filter (InstanceFilterWord), 24 its type, 25 read, 26 an instance added at a path, 27
//  a mesh or LOD set at a path, 28 the cell at a path, 29 its parent, 30 a child made, 31 a light's bit set, 32-34 its children.
//  A cell drawn or collected is given its parent's visibility (ChunkView::Visibility)
struct alignas(16) SceneryCell
{
    static constexpr u32 TypeId = 0x1612;

    // Its vtable's slots the cells and the chunks call
    enum Slot : u32
    {
        SlotDestroy = 1,
        SlotRelease = 2,
        SlotRender = 3,
        SlotCollectInstances = 4,
        SlotCollectCells = 6,
        SlotFindCell = 7,
        SlotSetChunk = 9,
        SlotCollectVisible = 10,
        SlotDrawContents = 11,
        SlotDrawContentsCulled = 12,
        SlotReleaseMeshes = 13,
        SlotCollect = 14,
        SlotCellOf = 15,
        SlotHasInstances = 21,
        SlotSleepInstances = 22,
        SlotReleaseInstances = 23,
        SlotRead = 25,
        SlotAddInstanceAt = 26,
        SlotSetItemAt = 27,
        SlotCellAt = 28,
        SlotMakeChild = 30,
        SlotSetLight = 31,
        SlotChildCount = 32,
        SlotChildren = 33,
        SlotOtherChildren = 34,
    };

    Vector4 min;
    Vector4 max;
    InstanceContext* instances;
    InstanceContext* dynamicInstances;
    SceneryCell* parent;
    SceneryMeshes* meshes;
    ChunkData* chunk;
    u8 lights[0x10];
    const GccVTableEntry* vtable;

    // The cell's base made (no box)
    void ConstructBase();
    void Destroy(u32 flags) RETAIL(FUN_001ef660);
    void Release(u32 instances, u32 queue) RETAIL(FUN_001efb40);
    u32 Render(s32 visibility, ChunkView* view, ChunkData* chunk) RETAIL(FUN_001ef920);
    void CollectInstances(s32 visibility, InstanceCollector* collector, u32 kinds, ChunkView* view) RETAIL(FUN_001ef730);
    void CollectCells(InstanceCollector* collector) RETAIL(FUN_001ef6d8);
    SceneryCell* FindCell() RETAIL(FUN_001ef2b8);
    void SetChunk(ChunkData* chunk) RETAIL(FUN_001ef2c0);
    u32 CollectVisible(BoundingVolume* volume, InstanceCollector* collector) RETAIL(FUN_001ef2e0);
    u32 DrawContents(ChunkView* view) RETAIL(FUN_001ef7e0);
    u32 DrawContentsCulled(ChunkView* view) RETAIL(FUN_001ef880);
    void ReleaseMeshes(u32 keep) RETAIL(FUN_001efae8);
    u32 Collect() RETAIL(FUN_001ef300);
    u32 CellOf() RETAIL(FUN_001ef308);
    u32 CellOfBox() RETAIL(FUN_001ef310);
    f32 Unused17() RETAIL(FUN_001ef318);
    s32 Depth() RETAIL(FUN_001ef328);
    u16 QueryInstances(InstanceQuery* query) RETAIL(FUN_001ef5f0);
    u32 HasInstances() RETAIL(FUN_001ef338);
    void SleepInstances(const u32* filter) RETAIL(FUN_001e9d60);
    void ReleaseInstances(const u32* filter) RETAIL(FUN_001e9df8);
    u32 Type() RETAIL(FUN_001ef348);
    void Read(Stream* stream) RETAIL(ReadSceneryBase);
    SceneryCell* AddInstanceAt(const s16* path, InstanceContext* instance) RETAIL(FUN_001ef9d8);
    SceneryCell* SetItemAt(const s16* path, RigidModel* mesh, Lod* lod, u32 unused4, s32 index) RETAIL(FUN_001efa08);
    SceneryCell* CellAt() RETAIL(FUN_001efae0);
    SceneryCell* Parent() RETAIL(FUN_001ef350);
    void SetLight() RETAIL(FUN_001ef358);

    // The instance on its list
    void AddInstance(InstanceContext* instance) RETAIL(FUN_001efc88);
    void SetChunkField(ChunkData* chunk) RETAIL(FUN_001ef330);
    u32 CollectVisibleCells(BoundingVolume* volume, InstanceCollector* collector) RETAIL(FUN_001efa60);

    void VirtualDestroy(u32 flags)
    {
        CallVirtual<void>(this, vtable, SlotDestroy, flags);
    }

    void VirtualRelease(u32 instances, u32 queue)
    {
        CallVirtual<void>(this, vtable, SlotRelease, instances, queue);
    }

    u32 VirtualRender(s32 visibility, ChunkView* view, ChunkData* chunk)
    {
        return CallVirtual<u32>(this, vtable, SlotRender, visibility, view, chunk);
    }
};
CHECK_OFFSET(SceneryCell, instances, 0x20);
CHECK_OFFSET(SceneryCell, lights, 0x34);
CHECK_OFFSET(SceneryCell, vtable, 0x44);
CHECK_SIZE(SceneryCell, 0x50);

// A leaf (0x50 bytes, the SM2's type 0x1605)
struct SceneryLeaf : SceneryCell
{
    static constexpr u32 TypeId = 0x1605;

    static SceneryLeaf* Construct(SceneryLeaf* leaf) RETAIL(FUN_001eff30);
    void Destroy(u32 flags) RETAIL(FUN_001effb8);
    u32 IsLeaf() RETAIL(FUN_001efdd0);
    void RemoveChild() RETAIL(FUN_001eff18);
    u32 DrawContents(ChunkView* view) RETAIL(FUN_001efdd8);
    u32 DrawContentsCulled(ChunkView* view) RETAIL(FUN_001efe78);
    u32 IsEmpty() RETAIL(FUN_001f0030);
    u32 Type() RETAIL(FUN_001efdc8);
    SceneryCell* SetItemAt(const s16* path, RigidModel* mesh, Lod* lod, u32 unused4, s32 index, u32 unused6, u32 set)
        RETAIL(FUN_001f0058);
    SceneryCell* MakeChild() RETAIL(FUN_001eff20);
};

// A cell with children (0x60 bytes, the tools' type 0x1612): its depth below it
struct SceneryTree : SceneryCell
{
    s32 depth;

    // Without a box, and with the box around a middle and a half size, a parent and a depth
    static SceneryTree* Construct(SceneryTree* tree) RETAIL(FUN_001f3658);
    static SceneryTree* ConstructBox(SceneryTree* tree, const Vector4* middle, const Vector4* halfSize, SceneryCell* parent,
                                     s32 depth) RETAIL(FUN_001f3540);
    void Destroy(u32 flags) RETAIL(FUN_001f3750);
    u32 Render(s32 visibility, ChunkView* view, ChunkData* chunk) RETAIL(RenderScenery_);
    void CollectInstances(s32 visibility, InstanceCollector* collector, u32 kinds, ChunkView* view) RETAIL(FUN_001eee88);
    u32 IsLeaf() RETAIL(FUN_001f0688);
    void CollectCells(InstanceCollector* collector) RETAIL(FUN_001f37c8);
    void SetChunk(ChunkData* chunk) RETAIL(FUN_001f39a8);
    void CollectVisible(BoundingVolume* volume, InstanceCollector* collector) RETAIL(FUN_001f3890);
    void ReleaseMeshes(u32 keep) RETAIL(FUN_001f3e00);
    s32 Depth() RETAIL(FUN_001f0690);
    u32 HasInstances() RETAIL(FUN_001f3a50);
    SceneryCell* AddInstanceAt(const s16* path, InstanceContext* instance, u32 unused) RETAIL(FUN_001f3b18);
    SceneryCell* SetItemAt(const s16* path, RigidModel* mesh, Lod* lod, u32 unused4, s32 index, u32 unused6, u32 set)
        RETAIL(FUN_001f3c20);
    SceneryCell* CellAt(const s16* path, u32 make) RETAIL(FUN_001f3d30);
    void SetLight(u32 bit) RETAIL(FUN_001f3ed8);
    s32 ChildCount() RETAIL(FUN_001f0670);
    SceneryCell** Children() RETAIL(FUN_001f0678);
    SceneryCell** OtherChildren() RETAIL(FUN_001f0680);
    // Every child cleared
    void ClearChildren() RETAIL(FUN_001f36d0);

    s32 VirtualChildCount()
    {
        return CallVirtual<s32>(this, vtable, SlotChildCount);
    }

    SceneryCell** VirtualChildren()
    {
        return CallVirtual<SceneryCell**>(this, vtable, SlotChildren);
    }

    SceneryCell** VirtualOtherChildren()
    {
        return CallVirtual<SceneryCell**>(this, vtable, SlotOtherChildren);
    }
};
CHECK_OFFSET(SceneryTree, depth, 0x50);
CHECK_SIZE(SceneryTree, 0x60);

// A node of the octree (0x80 bytes, the SM2's type 0x1600): its eight children, one per octant of its box (bit 0 of the octant x,
// bit 1 y, bit 2 z, a set bit the lower half)
struct SceneryNode : SceneryTree
{
    static constexpr u32 TypeId = 0x1600;
    static constexpr u32 Octants = 8;

    // (Aligned so GCC doesn't put them in the tree's tail padding, which GCC 2.9x left alone)
    alignas(16) SceneryCell* children[Octants];

    static SceneryNode* Construct(SceneryNode* node) RETAIL(FUN_001f1028);
    void Destroy(u32 flags) RETAIL(FUN_001f1068);
    void Release(u32 instances, u32 queue) RETAIL(FUN_001ec250);
    // The child holding the box nearest its middle, down to a depth
    SceneryCell* FindCell(const Vector4* min, const Vector4* max, s32 depth) RETAIL(FUN_001ebbf8);
    void RemoveChild(SceneryCell* child) RETAIL(FUN_001f1140);
    u32 IsEmpty() RETAIL(FUN_001f10f8);
    void SleepInstances(const u32* filter) RETAIL(FUN_001f11b0);
    void ReleaseInstances(const u32* filter) RETAIL(FUN_001f1230);
    u32 Type() RETAIL(FUN_001f06b0);
    void Read(Stream* stream) RETAIL(ReadScenery_Type_0x1600);
    // The child of an octant made (a leaf at the depths below 2) with the octant of the box
    SceneryCell* MakeChild(u32 octant, s32 depth) RETAIL(FUN_001ebe00);
    // The octant's offset from the middle (a quarter of the size, signed by its bits; level when the node has no 8 children)
    void OctantOffset(f32* offset, u32 octant) RETAIL(FUN_001eeac0);
    s32 ChildCount() RETAIL(GetChildrenAmount);
    SceneryCell** Children() RETAIL(GetSceneryChildren);
    SceneryCell** OtherChildren() RETAIL(FUN_001f06a8);
};
CHECK_OFFSET(SceneryNode, children, 0x60);
CHECK_SIZE(SceneryNode, 0x80);

// The scenery's root (0x90 bytes, the SM2's type 0x160A): the tree's depth (the node search gets it and ignores it)
struct SceneryRoot : SceneryNode
{
    static constexpr u32 TypeId = 0x160A;

    u32 treeDepth;

    void Destroy(u32 flags) RETAIL(FUN_001f12c8);
    void Collect(u32 kinds, InstanceCollector* collector) RETAIL(FUN_001f13b8);
    // The cell an instance's box (or its hull's under a matrix) goes in, and a box's (none when the root doesn't hold it)
    SceneryCell* CellOf(InstanceContext* instance, const Matrix4x4* matrix) RETAIL(FUN_001ec3a0);
    SceneryCell* CellOfBox(const Vector4* min, const Vector4* max) RETAIL(FUN_001f1358);
    u32 Type() RETAIL(FUN_001f12b0);
    void Read(Stream* stream) RETAIL(ReadSceneryRoot);
};
CHECK_OFFSET(SceneryRoot, treeDepth, 0x80);
CHECK_SIZE(SceneryRoot, 0x90);

// Reads a cell (SceneryTypeSection_Reader: its stream and the cell)
class SceneryCellReader : public SectionReader
{
public:
    Stream* stream;
    SceneryCell* cell;

    void Destroy(u32 flags) RETAIL(FUN_001ef220);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(LoadSceneryType);
};

// Releases a cell later (D_002FBE60: the cell, its release's two flags)
class SceneryReleaseReader : public SectionReader
{
public:
    SceneryCell* cell;
    u8 instances;
    u8 queue;

    void Destroy(u32 flags) RETAIL(FUN_001ef250);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_001efc18);
};

// Destroys a cell later (D_002FBE38)
class SceneryDestroyReader : public SectionReader
{
public:
    SceneryCell* cell;

    void Destroy(u32 flags) RETAIL(FUN_001ef280);
    void Read(u8* data, u32 size, ReaderStack* readers) RETAIL(FUN_001efc50);
};

// A filter of instances (three words): the kinds of nodes (a bit per NodeKind) one has any of, the flags (ReferencedObjectFlags)
// it has all of and the flags it has none of
enum InstanceFilterWord : u32
{
    FilterKinds = 0,
    FilterWantedFlags = 1,
    FilterUnwantedFlags = 2,
};

extern "C"
{
    extern const GccVTableEntry g_SceneryCellVTable[] RETAIL(SceneryBase_Methods);
    extern const GccVTableEntry g_SceneryLeafVTable[] RETAIL(SceneryLeaf_Methods);
    extern const GccVTableEntry g_SceneryTreeVTable[] RETAIL(SceneryType_0x1612_Methods);
    extern const GccVTableEntry g_SceneryNodeVTable[] RETAIL(SceneryNode_Methods);
    extern const GccVTableEntry g_SceneryRootVTable[] RETAIL(SceneryRoot_Methods);
    extern const GccVTableEntry g_ChunkViewVTable[] RETAIL(ChunkRenderingRelated__Methods);
    extern const GccVTableEntry g_LinkedChunkViewVTable[] RETAIL(ChunkRenderingRelated__Type0_Methods);
    // The direction the far scenery faces from (a direction faces the camera's way when it's along it)
    extern Vector4 g_CullDirection RETAIL(D_003B49D0);

    // The SM2's scenery read into the chunk's data: its bits, name, fog colour, root type, a byte, the sky, the lights and the
    // root (its cells queued for the readers)
    void ReadScenery(ChunkData* chunk, Stream* stream) RETAIL(ReadScenery);
    // A list of instances linked by their cell links: one put in front, one taken out, all of them released
    void CellListAdd(InstanceContext** list, InstanceContext* instance) RETAIL(FUN_001f03a0);
    void CellListRemove(InstanceContext** list, InstanceContext* instance) RETAIL(FUN_001f03d0);
    void CellListRelease(InstanceContext** list) RETAIL(FUN_001f0400);
    InstanceContext** CellListConstruct(InstanceContext** list) RETAIL(FUN_001f0348);
    void CellListDestroy(InstanceContext** list, u32 flags) RETAIL(FUN_001f0358);

    // A cell's instances drawn through a view, wholly in view or culled a box at a time (the frame's lists of instances to draw
    // get the ones that draw): how many
    u32 DrawCellInstances(InstanceContext** list, ChunkView* view) RETAIL(FUN_001ea530);
    u32 DrawCellInstancesCulled(InstanceContext** list, ChunkView* view) RETAIL(FUN_001ea720);
    // A cell's instances with a node of the kinds collected, all of them or tested against the view first
    void CollectCellInstancesAll(SceneryCell* cell, InstanceCollector* collector, u32 kinds) RETAIL(FUN_001e9a60);
    void CollectCellInstancesInView(SceneryCell* cell, InstanceCollector* collector, u32 kinds, ChunkView* view) RETAIL(FUN_001e9b08);
    // A collector made with two empty pools of each, the flags to match and the caller's word; destroyed with GCC 2.9x's
    // flags; its pools made empty; whether it gathered any instance
    InstanceCollector* InstanceCollectorConstruct(InstanceCollector* collector, u32 wantedFlags, u32 unwantedFlags,
                                                  u32 unused) RETAIL(FUN_001f2670);
    void InstanceCollectorDestroy(InstanceCollector* collector, u32 flags) RETAIL(FUN_001ee290);
    void InstanceCollectorClear(InstanceCollector* collector) RETAIL(FUN_001f2780);
    u32 InstanceCollectorFound(const InstanceCollector* collector) RETAIL(FUN_001f2f10);
    // The instances of the cells a collector gathered (both pools) that match its flags and have a node of the kinds, in its
    // first instance pool: whether it has any
    u32 CollectCellInstances(InstanceCollector* collector, u32 kinds) RETAIL(FUN_001ee390);
    // Every chunk's scenery collected: whether any instance was
    u32 CollectChunksInstances(ChunkList* list, u32 kinds, InstanceCollector* collector) RETAIL(FUN_001f1750);
    // Every chunk of the list made global where a filter matches (InstanceFilterWord), the current one first, by a way into the
    // game (game/progress.h's)
    void ChunkListMakeGlobalWhere(ChunkList* list, u32 way, const u32* filter) RETAIL(FUN_001f17c8);

    // The chunks' collision cells: the level a box goes in, the cell of an instance's place, the cells a volume overlaps (the
    // last cell after them; how many)
    s32 CellLevel(const Box* box) RETAIL(FUN_001e93a0);
    s32 InstanceCell(InstanceContext* instance) RETAIL(FUN_001e9418);
    s32 CellsOfVolume(s32* cells, const BoundingVolume* volume) RETAIL(FUN_001e94e0);
    // A cell list's instances the query takes: in a sphere (or overlapping its box), in a vertical cylinder, touching a hull
    // (or its place inside it), hit by a segment (an instance's hulls cast at, the query's nearest)
    u16 CellListInSphere(InstanceQuery* query, InstanceContext* first, const Vector4* sphere, u32 kinds, u32 boxTest)
        RETAIL(FUN_001ea9d0);
    u16 CellListInCylinder(f32 height, InstanceQuery* query, InstanceContext* first, const Vector4* base, u32 kinds)
        RETAIL_N32(FUN_001eac90);
    u16 CellListInHull(InstanceQuery* query, InstanceContext* first, BoundingVolume* volume, const CollisionHull* hull,
                       const Matrix4x4* matrix, u32 kinds, u32 points) RETAIL(FUN_001eae50);
    f32 InstanceRayCast(InstanceQuery* query, const Box* box, const Vector4* segment, InstanceContext* instance, void* hit)
        RETAIL(FUN_001eb0b0);
    f32 CellListRayCast(InstanceQuery* query, InstanceContext* first, const Vector4* segment, u32 kinds, void* hit)
        RETAIL(FUN_001f04c8);
    // A chunk's instances with a node of the kinds (a bit per NodeKind) in any cell
    s32 QueryChunkInstancesOfKinds(ChunkData* chunk, u32 kinds, InstanceQuery* query) RETAIL(FUN_001f1e40);
    // The levels' first cells worked out at the start (GCC 2.9x's static initialisation), and its constructor
    void InitCellBases(s32 initialise, s32 priority) RETAIL(FUN_001ef018);
    void CellsStaticInit() RETAIL(FUN_001f4540);
    // Where a box is against a cell's box (CellContainment), axis by axis, by how far their ranges reach together: apart (within
    // the margin's share of the box), inside (within the margin of all of it), partly. The span the retail code measures is the
    // ranges' union, so any box overlapping the cell is inside
    u32 CellHoldsBox(f32 margin, SceneryCell* cell, const Vector4* min, const Vector4* max) RETAIL_N32(FUN_001fa4b0);

    // An awake instance's collision put in the scenery cell holding its box and its collision cell, moved to another scenery
    // cell (the cell's chunk), and taken out of a chunk (whether the chunk let it go)
    u32 SortIntoCells(ObjectCollision* collision, SceneryCell* root, InstanceContext** cells, InstanceContext* instance)
        RETAIL(FUN_001eb7b0);
    ChunkData* MoveToCell(ObjectCollision* collision, InstanceContext* instance, SceneryCell* cell) RETAIL(FUN_001f0ba8);
    u32 ChunkRemoveInstance(ChunkData* chunk, InstanceContext* instance) RETAIL(FUN_001ed838);
    // The instance moved to the instances of no chunk (put to sleep first when it's awake), and those of a filter, not while the
    // chunk is being released when the state is checked; the way into the game is what they're given (game/progress.h's)
    u32 ChunkMakeGlobal(ChunkData* chunk, u32 checkState, u32 way, InstanceContext* instance) RETAIL(FUN_001f22d0);
    u32 ChunkMakeGlobalWhere(ChunkData* chunk, u32 checkState, u32 way, const u32* filter) RETAIL(FUN_001f2380);
    // Whether a link takes an instance (through its wall, or into the linked scenery's cells)
    u32 LinkTakesInstance(const ChunkLinkData* link, InstanceContext* instance) RETAIL(FUN_001ea2d8);
    // The scenery told its chunk once it's read
    void ChunkSceneryRead(ChunkData* chunk) RETAIL(FUN_001f22a0);

    // Six planes moved by a matrix, one of them moved into another, one turned to face a point, one made through three corners
    void TransformPlanes(Vector4* planes, const Matrix4x4* matrix) RETAIL(FUN_002015a8);
    void TransformPlaneOf(const Vector4* planes, s32 index, const Matrix4x4* matrix, Vector4* out) RETAIL(FUN_00201600);
    void FacePlaneTowards(Vector4* planes, s32 index, const Vector4* point) RETAIL(FUN_00201628);
    void PlaneOfCorners(Vector4* planes, s32 index, const Vector4* corners) RETAIL(FUN_00201698);

    // A chunk drawn: its view loaded from the camera's place (through a portal's planes for a linked chunk), a linked chunk drawn
    // through its link, the links' walls tested, the chunk's scenery and its links' chunks drawn, the shadows cast in it and its
    // links' chunks drawn, the camera's chunk's scene and shadows drawn, the instances whose drawing waited
    void SetUpChunkView(Matrix4x4* matrices, ChunkView* view, const ObjectPlace* place) RETAIL(FUN_001e8f08);
    void SetUpLinkedChunkView(Matrix4x4* matrices, ChunkView* view, const ObjectPlace* place, const Vector4* portal)
        RETAIL(FUN_001e90a0);
    void DrawLinkedChunk(ChunkLinkData* link, ChunkView* view, const ObjectPlace* place, s32 depth) RETAIL(FUN_001ea3a8);
    void LinksVisibility(ChunkData* chunk, ChunkView* view) RETAIL(FUN_001ed188);
    void DrawChunk(ChunkData* chunk, ChunkView* view, const ObjectPlace* place, s32 depth, const Vector4* portal)
        RETAIL(FUN_001ed2e8);
    void DrawChunkShadows(ChunkData* chunk, s32 depth, const Matrix4x4* toCamera, const Matrix4x4* matrix) RETAIL(FUN_001ed468);
    void DrawScene(ChunkData* chunk, const ObjectPlace* place, RenderView* view) RETAIL(FUN_001f1968);
    void DrawShadows(ChunkData* chunk, const Matrix4x4* toCamera, const Matrix4x4* matrix) RETAIL(FUN_001f1a10);
    void DrawDeferredInstances(ChunkView* view) RETAIL(FUN_002016c8);
}
