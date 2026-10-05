#include "game/graphicstables.h"

#include "game/memory.h"
#include "game/stream.h"
#include "platform/graphics.h"

extern "C"
{
    // Each kind's vtables: its own, the class with the name's and the base's
    extern const GccVTableEntry g_TextureTableVTable[] RETAIL(D_002F9148);
    extern const GccVTableEntry g_TextureTableNamedVTable[] RETAIL(D_002F9198);
    extern const GccVTableEntry g_TextureTableBaseVTable[] RETAIL(D_002F9710);
    extern const GccVTableEntry g_MaterialTableVTable[] RETAIL(D_002F9508);
    extern const GccVTableEntry g_MaterialTableNamedVTable[] RETAIL(D_002F9558);
    extern const GccVTableEntry g_MaterialTableBaseVTable[] RETAIL(D_002F98F0);
    extern const GccVTableEntry g_ModelTableVTable[] RETAIL(D_002F9468);
    extern const GccVTableEntry g_ModelTableNamedVTable[] RETAIL(D_002F94B8);
    extern const GccVTableEntry g_ModelTableBaseVTable[] RETAIL(D_002F98A0);
    extern const GccVTableEntry g_RigidModelTableVTable[] RETAIL(D_002F93C8);
    extern const GccVTableEntry g_RigidModelTableNamedVTable[] RETAIL(D_002F9418);
    extern const GccVTableEntry g_RigidModelTableBaseVTable[] RETAIL(D_002F9850);
    extern const GccVTableEntry g_SkinTableVTable[] RETAIL(D_002F9288);
    extern const GccVTableEntry g_SkinTableNamedVTable[] RETAIL(D_002F92D8);
    extern const GccVTableEntry g_SkinTableBaseVTable[] RETAIL(D_002F97B0);
    extern const GccVTableEntry g_BlendSkinTableVTable[] RETAIL(D_002F9328);
    extern const GccVTableEntry g_BlendSkinTableNamedVTable[] RETAIL(D_002F9378);
    extern const GccVTableEntry g_BlendSkinTableBaseVTable[] RETAIL(D_002F9800);
    extern const GccVTableEntry g_MeshTableVTable[] RETAIL(D_002F95A8);
    extern const GccVTableEntry g_MeshTableNamedVTable[] RETAIL(MeshTable_methods);
    extern const GccVTableEntry g_MeshTableBaseVTable[] RETAIL(D_002F9940);
    extern const GccVTableEntry g_LodTableVTable[] RETAIL(D_002F91E8);
    extern const GccVTableEntry g_LodTableNamedVTable[] RETAIL(D_002F9238);
    extern const GccVTableEntry g_LodTableBaseVTable[] RETAIL(D_002F9760);
    extern const GccVTableEntry g_SkyTableVTable[] RETAIL(D_002F90F8);
    extern const GccVTableEntry g_SkyTableNamedVTable[] RETAIL(D_002F9648);
    extern const GccVTableEntry g_SkyTableBaseVTable[] RETAIL(D_002F9698);

    // Each kind's vtables of its readers: the kind's reader's and its resources' section readers'
    extern const GccVTableEntry g_TextureReaderVTable[] RETAIL(TextureItem_Methods);
    extern const GccVTableEntry g_TextureSectionReaderVTable[] RETAIL(D_002F69C0);
    extern const GccVTableEntry g_MaterialReaderVTable[] RETAIL(MaterialItem_Methods);
    extern const GccVTableEntry g_MaterialSectionReaderVTable[] RETAIL(MaterialSectionReader_Methods);
    extern const GccVTableEntry g_ModelReaderVTable[] RETAIL(ModelsItem_Methods);
    extern const GccVTableEntry g_ModelSectionReaderVTable[] RETAIL(ModelSectionReader_Methods);
    extern const GccVTableEntry g_RigidModelReaderVTable[] RETAIL(RigidModelItem_Methods);
    extern const GccVTableEntry g_RigidModelSectionReaderVTable[] RETAIL(RigidModelSectionReader_Methods);
    extern const GccVTableEntry g_SkinReaderVTable[] RETAIL(SkinItem_Methods);
    extern const GccVTableEntry g_SkinSectionReaderVTable[] RETAIL(SkinSectionReader_Methods);
    extern const GccVTableEntry g_BlendSkinReaderVTable[] RETAIL(BlendSkinItem_Methods);
    extern const GccVTableEntry g_BlendSkinSectionReaderVTable[] RETAIL(BlendSkinSectionReader_Methods);
    extern const GccVTableEntry g_MeshReaderVTable[] RETAIL(RigidModel2Item_Methods);
    extern const GccVTableEntry g_MeshSectionReaderVTable[] RETAIL(RigidModel2SectionReader_Methods);
    extern const GccVTableEntry g_LodReaderVTable[] RETAIL(LodItem_Methods);
    extern const GccVTableEntry g_LodSectionReaderVTable[] RETAIL(LodSectionReader_Methods);
    extern const GccVTableEntry g_SkyReaderVTable[] RETAIL(SkydomeItem_Methods);
    extern const GccVTableEntry g_SkySectionReaderVTable[] RETAIL(SkydomeSectionReader_Methods);
    // The graphics item's, its subsections' section reader's, and the section readers' base's
    extern const GccVTableEntry g_GraphicsItemVTable[] RETAIL(GraphicsSectionItem_Methods);
    extern const GccVTableEntry g_GraphicsSubsectionReaderVTable[] RETAIL(GraphicsSectionReader_Methods);
    extern const GccVTableEntry g_SectionReaderVTable[] RETAIL(SectionReaderInterface_Methods);


    // Each kind's table functions under their retail names: the ones the readers call, and the vtables' (3 to 8 and the
    // destructors)
    TextureTable::Entry* TextureFind(TextureTable* table, u32 id) RETAIL(FUN_001a2750);
    s32 TextureInsert(TextureTable* table, GameTexture* const* texture, u32 id) RETAIL(FUN_001a1468);
    void TextureInsertAt(TextureTable* table, s32 index, GameTexture* const* texture, u32 id) RETAIL(FUN_001a5bb8);
    void TextureReleaseAll(TextureTable* table) RETAIL(FUN_001a5a50);
    s32 TextureAddReference(TextureTable* table, const u32* id, GameTexture* texture) RETAIL(FUN_001c5f80);
    GameTexture* TextureAcquire(TextureTable* table, const u32* id, bool* created) RETAIL(FUN_001c2408);
    void TextureRelease(TextureTable* table, const u32* id) RETAIL(FUN_001c25a8);
    void TextureReleaseItem(TextureTable* table, GameTexture* texture) RETAIL(FUN_001c4e70);
    bool TextureExists(TextureTable* table, const u32* id) RETAIL(TextureExists_);
    GameTexture* TextureGet(TextureTable* table, const u32* id) RETAIL(GetTexture);
    void TextureDestroy(TextureTable* table, u32 flags) RETAIL(FUN_001c4b98);
    void TextureDestroyNamed(TextureTable* table, u32 flags) RETAIL(FUN_001c4af0);
    void TextureDestroyBase(TextureTable* table, u32 flags) RETAIL(FUN_001c7328);

    MaterialTable::Entry* MaterialFind(MaterialTable* table, u32 id) RETAIL(FUN_001a2810);
    s32 MaterialInsert(MaterialTable* table, MaterialResource* const* material, u32 id) RETAIL(FUN_001a15e8);
    void MaterialInsertAt(MaterialTable* table, s32 index, MaterialResource* const* material, u32 id) RETAIL(FUN_001a5db0);
    void MaterialReleaseAll(MaterialTable* table) RETAIL(FUN_001a4f10);
    s32 MaterialAddReference(MaterialTable* table, const u32* id, MaterialResource* material) RETAIL(FUN_001c6af0);
    MaterialResource* MaterialAcquire(MaterialTable* table, const u32* id, bool* created) RETAIL(FUN_001c52b8);
    void MaterialRelease(MaterialTable* table, const u32* id) RETAIL(FUN_001c3a58);
    void MaterialReleaseItem(MaterialTable* table, MaterialResource* material) RETAIL(FUN_001c4c50);
    bool MaterialExists(MaterialTable* table, const u32* id) RETAIL(FUN_001c6b40);
    MaterialResource* MaterialGet(MaterialTable* table, const u32* id) RETAIL(FUN_001c6c00);
    void MaterialDestroy(MaterialTable* table, u32 flags) RETAIL(FUN_001c4358);
    void MaterialDestroyNamed(MaterialTable* table, u32 flags) RETAIL(FUN_001c42b0);
    void MaterialDestroyBase(MaterialTable* table, u32 flags) RETAIL(FUN_001c6f68);

    ModelTable::Entry* ModelFind(ModelTable* table, u32 id) RETAIL(FUN_001c74a0);
    s32 ModelInsert(ModelTable* table, RigidModelData* const* model, u32 id) RETAIL(FUN_001a38b8);
    void ModelInsertAt(ModelTable* table, s32 index, RigidModelData* const* model, u32 id) RETAIL(FUN_001a36c0);
    void ModelReleaseAll(ModelTable* table) RETAIL(FUN_001a5078);
    s32 ModelAddReference(ModelTable* table, const u32* id, RigidModelData* model) RETAIL(FUN_001c6908);
    RigidModelData* ModelAcquire(ModelTable* table, const u32* id, bool* created) RETAIL(FUN_001c53c8);
    void ModelRelease(ModelTable* table, const u32* id) RETAIL(FUN_001c3800);
    void ModelReleaseItem(ModelTable* table, RigidModelData* model) RETAIL(FUN_001c5040);
    bool ModelExists(ModelTable* table, const u32* id) RETAIL(FUN_001c6958);
    RigidModelData* ModelGet(ModelTable* table, const u32* id) RETAIL(FUN_001c6a18);
    void ModelDestroy(ModelTable* table, u32 flags) RETAIL(FUN_001c44b8);
    void ModelDestroyNamed(ModelTable* table, u32 flags) RETAIL(FUN_001c4410);
    void ModelDestroyBase(ModelTable* table, u32 flags) RETAIL(FUN_001c7008);

    s32 RigidModelInsert(RigidModelTable* table, RigidModel* const* model, u32 id) RETAIL(FUN_001a3c30);
    void RigidModelInsertAt(RigidModelTable* table, s32 index, RigidModel* const* model, u32 id) RETAIL(FUN_001a3a38);
    void RigidModelReleaseAll(RigidModelTable* table) RETAIL(FUN_001a51e0);
    s32 RigidModelAddReference(RigidModelTable* table, const u32* id, RigidModel* model) RETAIL(FUN_001c6720);
    RigidModel* RigidModelAcquire(RigidModelTable* table, const u32* id, bool* created) RETAIL(FUN_001c3408);
    void RigidModelRelease(RigidModelTable* table, const u32* id) RETAIL(FUN_001c35a8);
    void RigidModelReleaseItem(RigidModelTable* table, RigidModel* model) RETAIL(FUN_001c54d8);
    bool RigidModelExists(RigidModelTable* table, const u32* id) RETAIL(FUN_001c6770);
    RigidModel* RigidModelGet(RigidModelTable* table, const u32* id) RETAIL(FUN_001c6830);
    void RigidModelDestroy(RigidModelTable* table, u32 flags) RETAIL(FUN_001c4618);
    void RigidModelDestroyNamed(RigidModelTable* table, u32 flags) RETAIL(FUN_001c4570);
    void RigidModelDestroyBase(RigidModelTable* table, u32 flags) RETAIL(FUN_001c70a8);

    s32 SkinInsert(SkinTable* table, Skin* const* skin, u32 id) RETAIL(FUN_001a3fa8);
    void SkinInsertAt(SkinTable* table, s32 index, Skin* const* skin, u32 id) RETAIL(FUN_001a3db0);
    void SkinReleaseAll(SkinTable* table) RETAIL(FUN_001a54b0);
    s32 SkinAddReference(SkinTable* table, const u32* id, Skin* skin) RETAIL(FUN_001c6350);
    Skin* SkinAcquire(SkinTable* table, const u32* id, bool* created) RETAIL(FUN_001c2c08);
    void SkinRelease(SkinTable* table, const u32* id) RETAIL(FUN_001c2da8);
    void SkinReleaseItem(SkinTable* table, Skin* skin) RETAIL(FUN_001c5918);
    bool SkinExists(SkinTable* table, const u32* id) RETAIL(FUN_001c63a0);
    Skin* SkinGet(SkinTable* table, const u32* id) RETAIL(FUN_001c6460);
    void SkinDestroy(SkinTable* table, u32 flags) RETAIL(FUN_001c48d8);
    void SkinDestroyNamed(SkinTable* table, u32 flags) RETAIL(FUN_001c4830);
    void SkinDestroyBase(SkinTable* table, u32 flags) RETAIL(FUN_001c71e8);

    s32 BlendSkinInsert(BlendSkinTable* table, BlendSkin* const* blendSkin, u32 id) RETAIL(FUN_001a4320);
    void BlendSkinInsertAt(BlendSkinTable* table, s32 index, BlendSkin* const* blendSkin, u32 id) RETAIL(FUN_001a4128);
    void BlendSkinReleaseAll(BlendSkinTable* table) RETAIL(FUN_001a5348);
    s32 BlendSkinAddReference(BlendSkinTable* table, const u32* id, BlendSkin* blendSkin) RETAIL(FUN_001c6538);
    BlendSkin* BlendSkinAcquire(BlendSkinTable* table, const u32* id, bool* created) RETAIL(FUN_001c3008);
    void BlendSkinRelease(BlendSkinTable* table, const u32* id) RETAIL(FUN_001c31a8);
    void BlendSkinReleaseItem(BlendSkinTable* table, BlendSkin* blendSkin) RETAIL(FUN_001c56e0);
    bool BlendSkinExists(BlendSkinTable* table, const u32* id) RETAIL(FUN_001c6588);
    BlendSkin* BlendSkinGet(BlendSkinTable* table, const u32* id) RETAIL(FUN_001c6648);
    void BlendSkinDestroy(BlendSkinTable* table, u32 flags) RETAIL(FUN_001c4778);
    void BlendSkinDestroyNamed(BlendSkinTable* table, u32 flags) RETAIL(FUN_001c46d0);
    void BlendSkinDestroyBase(BlendSkinTable* table, u32 flags) RETAIL(FUN_001c7148);

    MeshTable::Entry* MeshFind(MeshTable* table, u32 id) RETAIL(FUN_001c73d8);
    s32 MeshInsert(MeshTable* table, RigidModel* const* mesh, u32 id) RETAIL(FUN_001a4a10);
    void MeshInsertAt(MeshTable* table, s32 index, RigidModel* const* mesh, u32 id) RETAIL(FUN_001a4818);
    void MeshReleaseAll(MeshTable* table) RETAIL(FUN_001a5780);
    s32 MeshAddReference(MeshTable* table, const u32* id, RigidModel* mesh) RETAIL(FUN_001c6cd8);
    RigidModel* MeshAcquire(MeshTable* table, const u32* id, bool* created) RETAIL(FUN_001c41b0);
    void MeshRelease(MeshTable* table, const u32* id) RETAIL(FUN_001c3cb0);
    void MeshReleaseItem(MeshTable* table, RigidModel* mesh) RETAIL(FUN_001c5ab0);
    bool MeshExists(MeshTable* table, const u32* id) RETAIL(FUN_001c6d28);
    RigidModel* MeshGet(MeshTable* table, const u32* id) RETAIL(FUN_001c6de8);
    void MeshDestroy(MeshTable* table, u32 flags) RETAIL(FUN_001c40f8);
    void MeshDestroyNamed(MeshTable* table, u32 flags) RETAIL(FUN_001c4050);
    void MeshDestroyBase(MeshTable* table, u32 flags) RETAIL(FUN_001c6ec0);

    s32 LodInsert(LodTable* table, Lod* const* lod, u32 id) RETAIL(FUN_001a4d88);
    void LodInsertAt(LodTable* table, s32 index, Lod* const* lod, u32 id) RETAIL(FUN_001a4b90);
    void LodReleaseAll(LodTable* table) RETAIL(FUN_001a58e8);
    s32 LodAddReference(LodTable* table, const u32* id, Lod* lod) RETAIL(FUN_001c6168);
    Lod* LodAcquire(LodTable* table, const u32* id, bool* created) RETAIL(FUN_001c2808);
    void LodRelease(LodTable* table, const u32* id) RETAIL(FUN_001c29a8);
    void LodReleaseItem(LodTable* table, Lod* lod) RETAIL(FUN_001c5cb8);
    bool LodExists(LodTable* table, const u32* id) RETAIL(FUN_001c61b8);
    Lod* LodGet(LodTable* table, const u32* id) RETAIL(FUN_001c6278);
    void LodDestroy(LodTable* table, u32 flags) RETAIL(FUN_001c4a38);
    void LodDestroyNamed(LodTable* table, u32 flags) RETAIL(FUN_001c4990);
    void LodDestroyBase(LodTable* table, u32 flags) RETAIL(FUN_001c7288);

    s32 SkyInsert(SkyTable* table, Sky* const* sky, u32 id) RETAIL(FUN_001a4698);
    void SkyInsertAt(SkyTable* table, s32 index, Sky* const* sky, u32 id) RETAIL(FUN_001a44a0);
    void SkyReleaseAll(SkyTable* table) RETAIL(FUN_001a5618);
    s32 SkyAddReference(SkyTable* table, const u32* id, Sky* sky) RETAIL(FUN_001c2220);
    Sky* SkyAcquire(SkyTable* table, const u32* id, bool* created) RETAIL(FUN_001bfa28);
    void SkyRelease(SkyTable* table, const u32* id) RETAIL(FUN_001bfbd8);
    void SkyReleaseItem(SkyTable* table, Sky* sky) RETAIL(FUN_001c0480);
    bool SkyExists(SkyTable* table, const u32* id) RETAIL(FUN_001c2270);
    Sky* SkyGet(SkyTable* table, const u32* id) RETAIL(FUN_001c2330);
    void SkyDestroy(SkyTable* table, u32 flags) RETAIL(FUN_001c02e8);
    void SkyDestroyNamed(SkyTable* table, u32 flags) RETAIL(FUN_001c3f08);
    void SkyDestroyBase(SkyTable* table, u32 flags) RETAIL(FUN_001c3fb0);

    // Each kind's reader's functions and its section readers' under their retail names
    void TextureReaderDestroy(GraphicsKindReader<TextureKind>* reader, u32 flags) RETAIL(FUN_001a1bb8);
    u32 TextureReaderCount(GraphicsKindReader<TextureKind>* reader) RETAIL(GetSubSectionsAmount_001A33D0);
    u32 TextureReaderSectionType(GraphicsKindReader<TextureKind>* reader) RETAIL(FUN_001a33e0);
    bool TextureReaderCanRead(GraphicsKindReader<TextureKind>* reader, u32 type) RETAIL(FUN_001a33e8);
    SectionReader* TextureReaderGetReader(GraphicsKindReader<TextureKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetTextureItemSectionReader);
    void TextureReaderClear(GraphicsKindReader<TextureKind>* reader) RETAIL(FUN_001a3528);
    void TextureSectionReaderDestroy(GraphicsResourceReader<TextureKind>* reader, u32 flags) RETAIL(FUN_001a6240);
    void TextureSectionReaderRead(GraphicsResourceReader<TextureKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadTexture);

    void MaterialReaderDestroy(GraphicsKindReader<MaterialKind>* reader, u32 flags) RETAIL(FUN_001a1ae8);
    u32 MaterialReaderCount(GraphicsKindReader<MaterialKind>* reader) RETAIL(GetSubSectionAmount);
    u32 MaterialReaderSectionType(GraphicsKindReader<MaterialKind>* reader) RETAIL(FUN_001a3558);
    bool MaterialReaderCanRead(GraphicsKindReader<MaterialKind>* reader, u32 type) RETAIL(FUN_001a3560);
    SectionReader* MaterialReaderGetReader(GraphicsKindReader<MaterialKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetMaterialsItemSectionReader);
    void MaterialReaderClear(GraphicsKindReader<MaterialKind>* reader) RETAIL(FUN_001a36a0);
    void MaterialSectionReaderDestroy(GraphicsResourceReader<MaterialKind>* reader, u32 flags) RETAIL(FUN_001a6270);
    void MaterialSectionReaderRead(GraphicsResourceReader<MaterialKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadMaterial);

    void ModelReaderDestroy(GraphicsKindReader<ModelKind>* reader, u32 flags) RETAIL(FUN_001a1c88);
    u32 ModelReaderCount(GraphicsKindReader<ModelKind>* reader) RETAIL(GetMaterialsItemsAmount);
    u32 ModelReaderSectionType(GraphicsKindReader<ModelKind>* reader) RETAIL(FUN_001a3268);
    bool ModelReaderCanRead(GraphicsKindReader<ModelKind>* reader, u32 type) RETAIL(FUN_001a3270);
    SectionReader* ModelReaderGetReader(GraphicsKindReader<ModelKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetGameModelReader);
    void ModelReaderClear(GraphicsKindReader<ModelKind>* reader) RETAIL(FUN_001a33b0);
    void ModelSectionReaderDestroy(GraphicsResourceReader<ModelKind>* reader, u32 flags) RETAIL(FUN_001a6210);
    void ModelSectionReaderRead(GraphicsResourceReader<ModelKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadGameModel);

    void RigidModelReaderDestroy(GraphicsKindReader<RigidModelKind>* reader, u32 flags) RETAIL(FUN_001a1d58);
    u32 RigidModelReaderCount(GraphicsKindReader<RigidModelKind>* reader) RETAIL(GetRigidModelsAmount);
    u32 RigidModelReaderSectionType(GraphicsKindReader<RigidModelKind>* reader) RETAIL(FUN_001a30f0);
    bool RigidModelReaderCanRead(GraphicsKindReader<RigidModelKind>* reader, u32 type) RETAIL(FUN_001a30f8);
    SectionReader* RigidModelReaderGetReader(GraphicsKindReader<RigidModelKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetRigidModelsReader);
    void RigidModelReaderClear(GraphicsKindReader<RigidModelKind>* reader) RETAIL(FUN_001a3238);
    void RigidModelSectionReaderDestroy(GraphicsResourceReader<RigidModelKind>* reader, u32 flags) RETAIL(FUN_001a61e0);
    void RigidModelSectionReaderRead(GraphicsResourceReader<RigidModelKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadRigidModel);

    void SkinReaderDestroy(GraphicsKindReader<SkinKind>* reader, u32 flags) RETAIL(FUN_001a1e28);
    u32 SkinReaderCount(GraphicsKindReader<SkinKind>* reader) RETAIL(GetSkinsAmount);
    u32 SkinReaderSectionType(GraphicsKindReader<SkinKind>* reader) RETAIL(FUN_001a2f78);
    bool SkinReaderCanRead(GraphicsKindReader<SkinKind>* reader, u32 type) RETAIL(FUN_001a2f80);
    SectionReader* SkinReaderGetReader(GraphicsKindReader<SkinKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetSkinsSubSectionReader);
    void SkinReaderClear(GraphicsKindReader<SkinKind>* reader) RETAIL(FUN_001a30c0);
    void SkinSectionReaderDestroy(GraphicsResourceReader<SkinKind>* reader, u32 flags) RETAIL(FUN_001a61b0);
    void SkinSectionReaderRead(GraphicsResourceReader<SkinKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadGameSkin);

    void BlendSkinReaderDestroy(GraphicsKindReader<BlendSkinKind>* reader, u32 flags) RETAIL(FUN_001a1ef8);
    u32 BlendSkinReaderCount(GraphicsKindReader<BlendSkinKind>* reader) RETAIL(GetBlendSkinsAmount);
    u32 BlendSkinReaderSectionType(GraphicsKindReader<BlendSkinKind>* reader) RETAIL(FUN_001a2e00);
    bool BlendSkinReaderCanRead(GraphicsKindReader<BlendSkinKind>* reader, u32 type) RETAIL(FUN_001a2e08);
    SectionReader* BlendSkinReaderGetReader(GraphicsKindReader<BlendSkinKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetBlendSkinSectionLoader);
    void BlendSkinReaderClear(GraphicsKindReader<BlendSkinKind>* reader) RETAIL(FUN_001a2f48);
    void BlendSkinSectionReaderDestroy(GraphicsResourceReader<BlendSkinKind>* reader, u32 flags) RETAIL(FUN_001a6180);
    void BlendSkinSectionReaderRead(GraphicsResourceReader<BlendSkinKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadBlendSkin);

    void MeshReaderDestroy(GraphicsKindReader<MeshKind>* reader, u32 flags) RETAIL(FUN_001a2098);
    u32 MeshReaderCount(GraphicsKindReader<MeshKind>* reader) RETAIL(GetRigidModels2Amount);
    u32 MeshReaderSectionType(GraphicsKindReader<MeshKind>* reader) RETAIL(FUN_001a2b10);
    bool MeshReaderCanRead(GraphicsKindReader<MeshKind>* reader, u32 type) RETAIL(FUN_001a2b18);
    SectionReader* MeshReaderGetReader(GraphicsKindReader<MeshKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetRigidModels2SectionReader);
    void MeshReaderClear(GraphicsKindReader<MeshKind>* reader) RETAIL(FUN_001a2c58);
    void MeshSectionReaderDestroy(GraphicsResourceReader<MeshKind>* reader, u32 flags) RETAIL(FUN_001a6120);
    void MeshSectionReaderRead(GraphicsResourceReader<MeshKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadRigidModel2);

    void LodReaderDestroy(GraphicsKindReader<LodKind>* reader, u32 flags) RETAIL(FUN_001a2168);
    u32 LodReaderCount(GraphicsKindReader<LodKind>* reader) RETAIL(GetLodsAmount);
    u32 LodReaderSectionType(GraphicsKindReader<LodKind>* reader) RETAIL(FUN_001a2998);
    bool LodReaderCanRead(GraphicsKindReader<LodKind>* reader, u32 type) RETAIL(FUN_001a29a0);
    SectionReader* LodReaderGetReader(GraphicsKindReader<LodKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetLodsSectionReader);
    void LodReaderClear(GraphicsKindReader<LodKind>* reader) RETAIL(FUN_001a2ae0);
    void LodSectionReaderDestroy(GraphicsResourceReader<LodKind>* reader, u32 flags) RETAIL(FUN_001a60f0);
    void LodSectionReaderRead(GraphicsResourceReader<LodKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadLOD);

    void SkyReaderDestroy(GraphicsKindReader<SkyKind>* reader, u32 flags) RETAIL(FUN_001a1fc8);
    u32 SkyReaderCount(GraphicsKindReader<SkyKind>* reader) RETAIL(GetSkydomesAmount);
    u32 SkyReaderSectionType(GraphicsKindReader<SkyKind>* reader) RETAIL(FUN_001a2c88);
    bool SkyReaderCanRead(GraphicsKindReader<SkyKind>* reader, u32 type) RETAIL(FUN_001a2c90);
    SectionReader* SkyReaderGetReader(GraphicsKindReader<SkyKind>* reader, s32 index, ItemHeader* header, s32* size) RETAIL(GetSkydomeSectionReader);
    void SkyReaderClear(GraphicsKindReader<SkyKind>* reader) RETAIL(FUN_001a2dd0);
    void SkySectionReaderDestroy(GraphicsResourceReader<SkyKind>* reader, u32 flags) RETAIL(FUN_001a6150);
    void SkySectionReaderRead(GraphicsResourceReader<SkyKind>* reader, u8* data, u32 size, ReaderStack* readers) RETAIL(LoadSkydome);

    // The vtables' first functions: a sky made is read from its own file (the table's folder, a backslash and the ID in decimal)
    // on the first stream, the others have nothing to do
    void SkyMade(SkyTable* table, const u32* id, Sky* sky) RETAIL(FUN_001baa50);
    void TextureMade(TextureTable* table, const u32* id, GameTexture* texture) RETAIL(FUN_001c5f78);
    void TextureBaseMade(TextureTable* table, const u32* id, GameTexture* texture) RETAIL(FUN_001c7560);
    void MaterialMade(MaterialTable* table, const u32* id, MaterialResource* material) RETAIL(FUN_001c6ae8);
    void MaterialBaseMade(MaterialTable* table, const u32* id, MaterialResource* material) RETAIL(FUN_001c73c8);
    void ModelMade(ModelTable* table, const u32* id, RigidModelData* model) RETAIL(FUN_001c6900);
    void ModelBaseMade(ModelTable* table, const u32* id, RigidModelData* model) RETAIL(FUN_001c73d0);
    void RigidModelMade(RigidModelTable* table, const u32* id, RigidModel* model) RETAIL(FUN_001c6718);
    void RigidModelBaseMade(RigidModelTable* table, const u32* id, RigidModel* model) RETAIL(FUN_001c7580);
    void SkinMade(SkinTable* table, const u32* id, Skin* skin) RETAIL(FUN_001c6348);
    void SkinBaseMade(SkinTable* table, const u32* id, Skin* skin) RETAIL(FUN_001c7570);
    void BlendSkinMade(BlendSkinTable* table, const u32* id, BlendSkin* blendSkin) RETAIL(FUN_001c6530);
    void BlendSkinBaseMade(BlendSkinTable* table, const u32* id, BlendSkin* blendSkin) RETAIL(FUN_001c7578);
    void MeshMade(MeshTable* table, const u32* id, RigidModel* mesh) RETAIL(FUN_001c6cd0);
    void MeshBaseMade(MeshTable* table, const u32* id, RigidModel* mesh) RETAIL(FUN_001c6f60);
    void LodMade(LodTable* table, const u32* id, Lod* lod) RETAIL(FUN_001c6160);
    void LodBaseMade(LodTable* table, const u32* id, Lod* lod) RETAIL(FUN_001c7568);
    void SkyNamedMade(SkyTable* table, const u32* id, Sky* sky) RETAIL(FUN_001c6eb8);
    void SkyBaseMade(SkyTable* table, const u32* id, Sky* sky) RETAIL(FUN_001c5f70);
}

namespace
{
// How much room the tables grow by
constexpr u16 Growth = 0x40;

// How each kind's resources are made, deleted and read (the platform's), and its vtables
template <typename Kind>
struct Traits;

template <>
struct Traits<TextureKind>
{
    static GameTexture* Make(u32 id)
    {
        return Platform::Graphics::NewTexture(id);
    }
    static void Delete(GameTexture* texture)
    {
        Platform::Graphics::DeleteTexture(texture);
    }
    static void Read(GameTexture* texture, Stream* stream)
    {
        Platform::Graphics::ReadTexture(texture, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_TextureTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_TextureTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_TextureTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_TextureReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_TextureSectionReaderVTable;
};

template <>
struct Traits<MaterialKind>
{
    static MaterialResource* Make(u32 id)
    {
        return Platform::Graphics::NewMaterial(id);
    }
    static void Delete(MaterialResource* material)
    {
        Platform::Graphics::DeleteMaterial(material);
    }
    static void Read(MaterialResource* material, Stream* stream)
    {
        Platform::Graphics::ReadMaterial(material, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_MaterialTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_MaterialTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_MaterialTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_MaterialReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_MaterialSectionReaderVTable;
};

template <>
struct Traits<ModelKind>
{
    static RigidModelData* Make(u32 id)
    {
        return Platform::Graphics::NewModel(id);
    }
    static void Delete(RigidModelData* model)
    {
        Platform::Graphics::DeleteModel(model);
    }
    static void Read(RigidModelData* model, Stream* stream)
    {
        Platform::Graphics::ReadModel(model, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_ModelTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_ModelTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_ModelTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_ModelReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_ModelSectionReaderVTable;
};

template <>
struct Traits<RigidModelKind>
{
    static RigidModel* Make(u32 id)
    {
        return Platform::Graphics::NewRigidModel(id);
    }
    static void Delete(RigidModel* model)
    {
        Platform::Graphics::DeleteRigidModel(model);
    }
    static void Read(RigidModel* model, Stream* stream)
    {
        Platform::Graphics::ReadRigidModel(model, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_RigidModelTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_RigidModelTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_RigidModelTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_RigidModelReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_RigidModelSectionReaderVTable;
};

template <>
struct Traits<SkinKind>
{
    static Skin* Make(u32 id)
    {
        return Platform::Graphics::NewSkin(id);
    }
    static void Delete(Skin* skin)
    {
        Platform::Graphics::DeleteSkin(skin);
    }
    static void Read(Skin* skin, Stream* stream)
    {
        Platform::Graphics::ReadSkin(skin, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_SkinTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_SkinTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_SkinTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_SkinReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_SkinSectionReaderVTable;
};

template <>
struct Traits<BlendSkinKind>
{
    static BlendSkin* Make(u32 id)
    {
        return Platform::Graphics::NewBlendSkin(id);
    }
    static void Delete(BlendSkin* skin)
    {
        Platform::Graphics::DeleteBlendSkin(skin);
    }
    static void Read(BlendSkin* skin, Stream* stream)
    {
        Platform::Graphics::ReadBlendSkin(skin, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_BlendSkinTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_BlendSkinTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_BlendSkinTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_BlendSkinReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_BlendSkinSectionReaderVTable;
};

template <>
struct Traits<MeshKind>
{
    static RigidModel* Make(u32 id)
    {
        return Platform::Graphics::NewMesh(id);
    }
    static void Delete(RigidModel* mesh)
    {
        Platform::Graphics::DeleteMesh(mesh);
    }
    static void Read(RigidModel* mesh, Stream* stream)
    {
        Platform::Graphics::ReadMesh(mesh, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_MeshTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_MeshTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_MeshTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_MeshReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_MeshSectionReaderVTable;
};

template <>
struct Traits<LodKind>
{
    static Lod* Make(u32 id)
    {
        return Platform::Graphics::NewLod(id);
    }
    static void Delete(Lod* lod)
    {
        Platform::Graphics::DeleteLod(lod);
    }
    static void Read(Lod* lod, Stream* stream)
    {
        Platform::Graphics::ReadLod(lod, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_LodTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_LodTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_LodTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_LodReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_LodSectionReaderVTable;
};

template <>
struct Traits<SkyKind>
{
    static Sky* Make(u32 id)
    {
        return Platform::Graphics::NewSky(id);
    }
    static void Delete(Sky* sky)
    {
        Platform::Graphics::DeleteSky(sky);
    }
    static void Read(Sky* sky, Stream* stream)
    {
        Platform::Graphics::ReadSky(sky, stream);
    }
    static constexpr const GccVTableEntry* VTable = g_SkyTableVTable;
    static constexpr const GccVTableEntry* NamedVTable = g_SkyTableNamedVTable;
    static constexpr const GccVTableEntry* BaseVTable = g_SkyTableBaseVTable;
    static constexpr const GccVTableEntry* ReaderVTable = g_SkyReaderVTable;
    static constexpr const GccVTableEntry* SectionReaderVTable = g_SkySectionReaderVTable;
};

// A reference let go of through the table, or the resource deleted when it was made outside the tables
template <typename Kind>
void ReleaseResource(GraphicsTable<Kind>* table, typename Kind::Item* item)
{
    if (HeaderOf(item)->id != NoResourceId)
    {
        table->ReleaseItem(item);
        return;
    }

    if (item != nullptr)
    {
        Traits<Kind>::Delete(item);
    }
}
}

template <typename Kind>
s32 GraphicsTable<Kind>::Search(u32 id) const
{
    s32 high = count;
    s32 low = 0;
    s32 middle = count >> 1;
    s32 last = -1;
    u32 entryId = entries[middle].id;
    while (entryId != id)
    {
        if (entryId < id)
        {
            high = middle;
        }
        else
        {
            low = middle;
        }

        if (last == middle)
        {
            break;
        }

        last = middle;
        middle = (high - low) / 2 + low;
        entryId = entries[middle].id;
    }

    return middle;
}

template <typename Kind>
typename GraphicsTable<Kind>::Entry* GraphicsTable<Kind>::Find(u32 id)
{
    if (count == 0)
    {
        return nullptr;
    }

    Entry* entry = &entries[Search(id)];
    return entry->id == id ? entry : nullptr;
}

template <typename Kind>
s32 GraphicsTable<Kind>::Insert(Item* const* item, u32 id)
{
    if (count == 0)
    {
        count = 1;
        if (entries == nullptr)
        {
            capacity = growth;
            entries = NewArray<Entry>(capacity);
        }

        entries[0].item = *item;
        entries[0].id = id;
        return 0;
    }

    s32 index = Search(id);
    Entry& entry = entries[index];
    if (entry.id == id)
    {
        entry.item = *item;
        return 1;
    }

    InsertAt(entry.id < id ? index : index + 1, item, id);
    return 0;
}

// Without the loops made into calls of memmove, which the game doesn't have
template <typename Kind>
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void GraphicsTable<Kind>::InsertAt(s32 index, Item* const* item,
                                                                                               u32 id)
{
    if (count < capacity)
    {
        for (s32 at = count - 1; at >= index; at--)
        {
            entries[at + 1] = entries[at];
        }

        entries[index].item = *item;
        entries[index].id = id;
        count++;
        return;
    }

    capacity += growth;
    Entry* grown = NewArray<Entry>(capacity);
    for (s32 at = 0; at < index; at++)
    {
        grown[at] = entries[at];
    }

    grown[index].item = *item;
    grown[index].id = id;
    for (s32 at = index; at < count; at++)
    {
        grown[at + 1] = entries[at];
    }

    if (entries != nullptr)
    {
        DeleteArray(entries);
    }

    entries = grown;
    count++;
}

template <typename Kind>
void GraphicsTable<Kind>::ReleaseAll()
{
    for (u32 index = 0; index < count; index++)
    {
        // Cleared before the check for none (through address 0)
        Item* item = entries[index].item;
        ResourceHeader* header = HeaderOf(item);
        header->bits.references = 0;
        header->bits.unused16 = 0;
        header->bits.kept = 0;
        if (item != nullptr)
        {
            Traits<Kind>::Delete(item);
        }
    }

    if (entries != nullptr)
    {
        DeleteArray(entries);
    }

    entries = nullptr;
    count = 0;
    capacity = 0;
    if (queue != nullptr)
    {
        while (queue->DeleteFirst([](void* item) { Traits<Kind>::Delete(static_cast<Item*>(item)); }))
        {
        }
    }
}

template <typename Kind>
s32 GraphicsTable<Kind>::AddReference(const u32* id, Item* item)
{
    TakeReference(item);
    return Insert(&item, *id);
}

template <typename Kind>
typename GraphicsTable<Kind>::Item* GraphicsTable<Kind>::Acquire(const u32* id, bool* created)
{
    Entry* entry = Find(*id);
    Item* item = entry != nullptr ? entry->item : nullptr;
    if (created != nullptr)
    {
        *created = item == nullptr;
    }

    if (item != nullptr)
    {
        TakeReference(item);
        return item;
    }

    item = Traits<Kind>::Make(*id);
    TakeReference(item);
    CallVirtual<void>(this, vtable, MadeSlot, id, item);
    Insert(&item, *id);
    return item;
}

template <typename Kind>
bool GraphicsTable<Kind>::DropReference(Item* item)
{
    ResourceHeader* header = HeaderOf(item);
    u16 references = --header->bits.references;
    return references != 0 || header->bits.kept != 0;
}

// Without the loop made into a call of memmove, which the game doesn't have
template <typename Kind>
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void GraphicsTable<Kind>::Forget(Item* item, u32 id)
{
    if (count != 0)
    {
        s32 index = Search(id);
        if (entries[index].id == id)
        {
            count--;
            for (s32 at = index; at < count; at++)
            {
                entries[at] = entries[at + 1];
            }
        }
    }

    if (queue != nullptr)
    {
        queue->Push(item);
        return;
    }

    if (item != nullptr)
    {
        Traits<Kind>::Delete(item);
    }
}

// An ID the table hasn't got lets go of a reference through address 0
template <typename Kind>
void GraphicsTable<Kind>::Release(const u32* id)
{
    Entry* entry = Find(*id);
    Item* item = entry != nullptr ? entry->item : nullptr;
    if (DropReference(item))
    {
        return;
    }

    Forget(item, *id);
}

template <typename Kind>
void GraphicsTable<Kind>::ReleaseItem(Item* item)
{
    if (DropReference(item))
    {
        return;
    }

    Forget(item, HeaderOf(item)->id);
}

template <typename Kind>
bool GraphicsTable<Kind>::Exists(const u32* id)
{
    if (count == 0)
    {
        return false;
    }

    return entries[Search(*id)].id == *id;
}

template <typename Kind>
typename GraphicsTable<Kind>::Item* GraphicsTable<Kind>::Get(const u32* id)
{
    Entry* entry = Find(*id);
    return entry != nullptr ? entry->item : nullptr;
}

template <typename Kind>
void GraphicsTable<Kind>::Destroy(u32 flags)
{
    vtable = Traits<Kind>::VTable;
    DestroyNamed(flags);
}

template <typename Kind>
void GraphicsTable<Kind>::DestroyNamed(u32 flags)
{
    StringDestroy(&name);
    DestroyBase(flags);
}

// The array is freed again after ReleaseAll freed it (it's none by then)
template <typename Kind>
void GraphicsTable<Kind>::DestroyBase(u32 flags)
{
    vtable = Traits<Kind>::BaseVTable;
    ReleaseAll();
    if (entries != nullptr)
    {
        DeleteArray(entries);
    }

    entries = nullptr;
    count = 0;
    capacity = 0;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

template <typename Kind>
void GraphicsTable<Kind>::Construct(const char* folder)
{
    String text;
    StringConstruct(&text, folder);
    entries = nullptr;
    vtable = Traits<Kind>::NamedVTable;
    count = 0;
    capacity = 0;
    growth = Growth;
    pending = nullptr;
    queue = nullptr;
    name.string = nullptr;
    name.length = 0;
    name.capacity = 0;
    StringAssign(&name, text.string);
    StringDestroy(&text);
    vtable = Traits<Kind>::VTable;
}

template <typename Kind>
void GraphicsTable<Kind>::ReleasePending()
{
    if (pending == nullptr)
    {
        return;
    }

    for (u32 index = 0; index < pending->count; index++)
    {
        Release(&pending->ids[index]);
    }

    pending = nullptr;
}

template <typename Kind>
void GraphicsKindReader<Kind>::Destroy(u32 flags)
{
    Unload();
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

template <typename Kind>
void GraphicsKindReader<Kind>::Unload()
{
    vtable = Traits<Kind>::ReaderVTable;
    table->ReleasePending();
    vtable = g_ItemInterfaceVTable;
}

template <typename Kind>
u32 GraphicsKindReader<Kind>::Count()
{
    return table->count;
}

// The resource the reading takes a reference of has its ID in the pending list
template <typename Kind>
SectionReader* GraphicsKindReader<Kind>::GetReader(s32, ItemHeader* header, s32*)
{
    using Item = typename Kind::Item;
    u32 id = header->id;
    Item* item = table->Get(&id);
    if (item != nullptr)
    {
        PendingIds* ids = table->pending;
        if (ids != nullptr)
        {
            ids->ids[ids->count] = HeaderOf(item)->id;
            ids->count++;
            TakeReference(item);
        }

        return nullptr;
    }

    item = Traits<Kind>::Make(id);
    PendingIds* ids = table->pending;
    if (ids != nullptr)
    {
        ids->ids[ids->count] = HeaderOf(item)->id;
        ids->count++;
        TakeReference(item);
    }

    auto* reader = static_cast<GraphicsResourceReader<Kind>*>(MemoryAllocate(sizeof(GraphicsResourceReader<Kind>)));
    reader->vtable = Traits<Kind>::SectionReaderVTable;
    reader->item = item;
    reader->table = table;
    return reader;
}

template <typename Kind>
void GraphicsKindReader<Kind>::Clear()
{
    table->ReleaseAll();
}

template <typename Kind>
void GraphicsResourceReader<Kind>::Destroy(u32 flags)
{
    vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

template <typename Kind>
void GraphicsResourceReader<Kind>::Read(u8* data, u32 size, ReaderStack*)
{
    MemoryStream stream;
    MemoryStream::Construct(&stream, data, size, 0, MemoryStream::FileAlignment);
    Traits<Kind>::Read(item, &stream);
    typename Kind::Item* read = item;
    table->Insert(&read, HeaderOf(read)->id);
    stream.Destroy(DestroyOnly);
}

template struct GraphicsTable<TextureKind>;
template struct GraphicsTable<MaterialKind>;
template struct GraphicsTable<ModelKind>;
template struct GraphicsTable<RigidModelKind>;
template struct GraphicsTable<SkinKind>;
template struct GraphicsTable<BlendSkinKind>;
template struct GraphicsTable<MeshKind>;
template struct GraphicsTable<LodKind>;
template struct GraphicsTable<SkyKind>;
template struct GraphicsKindReader<TextureKind>;
template struct GraphicsKindReader<MaterialKind>;
template struct GraphicsKindReader<ModelKind>;
template struct GraphicsKindReader<RigidModelKind>;
template struct GraphicsKindReader<SkinKind>;
template struct GraphicsKindReader<BlendSkinKind>;
template struct GraphicsKindReader<MeshKind>;
template struct GraphicsKindReader<LodKind>;
template struct GraphicsKindReader<SkyKind>;
template struct GraphicsResourceReader<TextureKind>;
template struct GraphicsResourceReader<MaterialKind>;
template struct GraphicsResourceReader<ModelKind>;
template struct GraphicsResourceReader<RigidModelKind>;
template struct GraphicsResourceReader<SkinKind>;
template struct GraphicsResourceReader<BlendSkinKind>;
template struct GraphicsResourceReader<MeshKind>;
template struct GraphicsResourceReader<LodKind>;
template struct GraphicsResourceReader<SkyKind>;

void InitGraphicsResourceTables(s32 initialise, s32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    // The meshes' table has the rigid models' name
    g_SkyTable.Construct("SkyDomes\\");
    g_MaterialTable.Construct("Materials\\");
    g_TextureTable.Construct("Textures\\");
    g_RigidModelTable.Construct("RigidModels\\");
    g_ModelTable.Construct("Models\\");
    g_BlendSkinTable.Construct("BlendSkins\\");
    g_SkinTable.Construct("Skins\\");
    g_MeshTable.Construct("RigidModels\\");
    g_LodTable.Construct("LODs\\");
}

void GraphicsTablesStaticInit()
{
    InitGraphicsResourceTables(1, DefaultInitPriority);
}

void ReleaseGraphicsResources(GraphicsItem*)
{
    g_SkyTable.ReleaseAll();
    g_LodTable.ReleaseAll();
    g_MeshTable.ReleaseAll();
    g_BlendSkinTable.ReleaseAll();
    g_SkinTable.ReleaseAll();
    g_RigidModelTable.ReleaseAll();
    g_ModelTable.ReleaseAll();
    g_MaterialTable.ReleaseAll();
    g_TextureTable.ReleaseAll();
}

GraphicsItem* InitGraphicsItem(GraphicsItem* item)
{
    item->vtable = g_GraphicsItemVTable;
    item->materials.vtable = g_MaterialReaderVTable;
    item->materials.table = &g_MaterialTable;
    item->textures.vtable = g_TextureReaderVTable;
    item->textures.table = &g_TextureTable;
    item->models.vtable = g_ModelReaderVTable;
    item->models.table = &g_ModelTable;
    item->rigidModels.vtable = g_RigidModelReaderVTable;
    item->rigidModels.table = &g_RigidModelTable;
    item->skins.vtable = g_SkinReaderVTable;
    item->skins.table = &g_SkinTable;
    item->blendSkins.vtable = g_BlendSkinReaderVTable;
    item->blendSkins.table = &g_BlendSkinTable;
    item->skies.vtable = g_SkyReaderVTable;
    item->skies.table = &g_SkyTable;
    item->meshes.vtable = g_MeshReaderVTable;
    item->meshes.table = &g_MeshTable;
    item->lods.vtable = g_LodReaderVTable;
    item->lods.table = &g_LodTable;
    for (PendingIds*& ids : item->pending)
    {
        ids = nullptr;
    }

    return item;
}

void UnloadGraphics(GraphicsItem* item, u32 flags)
{
    item->vtable = g_GraphicsItemVTable;
    FinishGraphicsReading(item);
    item->lods.Unload();
    item->meshes.Unload();
    item->skies.Unload();
    item->blendSkins.Unload();
    item->skins.Unload();
    item->rigidModels.Unload();
    item->models.Unload();
    item->textures.Unload();
    item->materials.Unload();
    item->vtable = g_ItemInterfaceVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(item);
    }
}

u32 GraphicsItemCount(GraphicsItem*)
{
    return GraphicsItem::SubsectionCount;
}

u32 GraphicsItemSectionType(GraphicsItem*)
{
    return DefaultSectionType;
}

bool GraphicsItemCanRead(GraphicsItem*, u32 type)
{
    return type == DefaultSectionType;
}

SectionReader* GetGraphicsSectionReader(GraphicsItem* item, s32, ItemHeader* header, s32* size)
{
    if (*size == 0)
    {
        return nullptr;
    }

    u32 kind = header->id;
    u32 start = header->offset;
    // A subsection is read as far as its header, its table is queued
    if (kind < GraphicsItem::SubsectionCount)
    {
        *size = sizeof(SectionHeader);
    }

    auto* reader = static_cast<GraphicsSubsectionReader*>(MemoryAllocate(sizeof(GraphicsSubsectionReader)));
    reader->kind = kind;
    reader->vtable = g_GraphicsSubsectionReaderVTable;
    reader->start = start;
    reader->item = item;
    return reader;
}

void GraphicsItemSetCount(GraphicsItem* item, u32)
{
    StartGraphicsReading(item);
}

void StartGraphicsReading(GraphicsItem* item)
{
    for (PendingIds*& ids : item->pending)
    {
        ids = static_cast<PendingIds*>(MemoryAllocate(sizeof(PendingIds)));
        ids->count = 0;
    }

    g_MaterialTable.pending = item->pending[GraphicsItem::MaterialSubsection];
    g_TextureTable.pending = item->pending[GraphicsItem::TextureSubsection];
    g_ModelTable.pending = item->pending[GraphicsItem::ModelSubsection];
    g_RigidModelTable.pending = item->pending[GraphicsItem::RigidModelSubsection];
    g_SkinTable.pending = item->pending[GraphicsItem::SkinSubsection];
    g_BlendSkinTable.pending = item->pending[GraphicsItem::BlendSkinSubsection];
    g_SkyTable.pending = item->pending[GraphicsItem::SkySubsection];
    g_MeshTable.pending = item->pending[GraphicsItem::MeshSubsection];
    g_LodTable.pending = item->pending[GraphicsItem::LodSubsection];
}

// The lists are freed whether they were made or not
void FinishGraphicsReading(GraphicsItem* item)
{
    g_MaterialTable.ReleasePending();
    g_TextureTable.ReleasePending();
    g_ModelTable.ReleasePending();
    g_RigidModelTable.ReleasePending();
    g_SkinTable.ReleasePending();
    g_BlendSkinTable.ReleasePending();
    g_SkyTable.ReleasePending();
    g_MeshTable.ReleasePending();
    g_LodTable.ReleasePending();
    for (PendingIds*& ids : item->pending)
    {
        MemoryDeallocate2_(ids);
        ids = nullptr;
    }
}

void ForgetGraphicsReading(GraphicsItem*)
{
    g_MaterialTable.pending = nullptr;
    g_TextureTable.pending = nullptr;
    g_ModelTable.pending = nullptr;
    g_RigidModelTable.pending = nullptr;
    g_SkinTable.pending = nullptr;
    g_BlendSkinTable.pending = nullptr;
    g_SkyTable.pending = nullptr;
    g_MeshTable.pending = nullptr;
    g_LodTable.pending = nullptr;
}

void DestroyGraphicsSubsectionReader(GraphicsSubsectionReader* reader, u32 flags)
{
    reader->vtable = g_SectionReaderVTable;
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(reader);
    }
}

void LoadGraphicsSection(GraphicsSubsectionReader* reader, u8*, u32, ReaderStack*)
{
    LoadGraphicsSubSection(reader->item, reader->kind, reader->start);
}

void LoadGraphicsSubSection(GraphicsItem* item, u32 kind, u32 start)
{
    switch (kind)
    {
    case GraphicsItem::TextureSubsection:
        AddSectionToLoadQueue(&item->textures, start);
        break;
    case GraphicsItem::MaterialSubsection:
        AddSectionToLoadQueue(&item->materials, start);
        break;
    case GraphicsItem::ModelSubsection:
        AddSectionToLoadQueue(&item->models, start);
        break;
    case GraphicsItem::RigidModelSubsection:
        AddSectionToLoadQueue(&item->rigidModels, start);
        break;
    case GraphicsItem::SkinSubsection:
        AddSectionToLoadQueue(&item->skins, start);
        break;
    case GraphicsItem::BlendSkinSubsection:
        AddSectionToLoadQueue(&item->blendSkins, start);
        break;
    case GraphicsItem::MeshSubsection:
        AddSectionToLoadQueue(&item->meshes, start);
        break;
    case GraphicsItem::LodSubsection:
        AddSectionToLoadQueue(&item->lods, start);
        break;
    case GraphicsItem::SkySubsection:
        AddSectionToLoadQueue(&item->skies, start);
        break;
    }
}

void ReleaseTexture(GameTexture* texture)
{
    ReleaseResource(&g_TextureTable, texture);
}

void ReleaseMaterial(MaterialResource* material)
{
    ReleaseResource(&g_MaterialTable, material);
}

void ReleaseModel(RigidModelData* model)
{
    ReleaseResource(&g_ModelTable, model);
}

void ReleaseRigidModel(RigidModel* model)
{
    ReleaseResource(&g_RigidModelTable, model);
}

void ReleaseSkin(Skin* skin)
{
    ReleaseResource(&g_SkinTable, skin);
}

void ReleaseBlendSkin(BlendSkin* blendSkin)
{
    ReleaseResource(&g_BlendSkinTable, blendSkin);
}

void ReleaseMesh(RigidModel* mesh)
{
    ReleaseResource(&g_MeshTable, mesh);
}

void ReleaseLod(Lod* lod)
{
    ReleaseResource(&g_LodTable, lod);
}

void ReleaseSky(Sky* sky)
{
    ReleaseResource(&g_SkyTable, sky);
}

TextureTable::Entry* TextureFind(TextureTable* table, u32 id)
{
    return table->Find(id);
}

s32 TextureInsert(TextureTable* table, GameTexture* const* texture, u32 id)
{
    return table->Insert(texture, id);
}

void TextureInsertAt(TextureTable* table, s32 index, GameTexture* const* texture, u32 id)
{
    table->InsertAt(index, texture, id);
}

void TextureReleaseAll(TextureTable* table)
{
    table->ReleaseAll();
}

s32 TextureAddReference(TextureTable* table, const u32* id, GameTexture* texture)
{
    return table->AddReference(id, texture);
}

GameTexture* TextureAcquire(TextureTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void TextureRelease(TextureTable* table, const u32* id)
{
    table->Release(id);
}

void TextureReleaseItem(TextureTable* table, GameTexture* texture)
{
    table->ReleaseItem(texture);
}

bool TextureExists(TextureTable* table, const u32* id)
{
    return table->Exists(id);
}

GameTexture* TextureGet(TextureTable* table, const u32* id)
{
    return table->Get(id);
}

void TextureDestroy(TextureTable* table, u32 flags)
{
    table->Destroy(flags);
}

void TextureDestroyNamed(TextureTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void TextureDestroyBase(TextureTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

MaterialTable::Entry* MaterialFind(MaterialTable* table, u32 id)
{
    return table->Find(id);
}

s32 MaterialInsert(MaterialTable* table, MaterialResource* const* material, u32 id)
{
    return table->Insert(material, id);
}

void MaterialInsertAt(MaterialTable* table, s32 index, MaterialResource* const* material, u32 id)
{
    table->InsertAt(index, material, id);
}

void MaterialReleaseAll(MaterialTable* table)
{
    table->ReleaseAll();
}

s32 MaterialAddReference(MaterialTable* table, const u32* id, MaterialResource* material)
{
    return table->AddReference(id, material);
}

MaterialResource* MaterialAcquire(MaterialTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void MaterialRelease(MaterialTable* table, const u32* id)
{
    table->Release(id);
}

void MaterialReleaseItem(MaterialTable* table, MaterialResource* material)
{
    table->ReleaseItem(material);
}

bool MaterialExists(MaterialTable* table, const u32* id)
{
    return table->Exists(id);
}

MaterialResource* MaterialGet(MaterialTable* table, const u32* id)
{
    return table->Get(id);
}

void MaterialDestroy(MaterialTable* table, u32 flags)
{
    table->Destroy(flags);
}

void MaterialDestroyNamed(MaterialTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void MaterialDestroyBase(MaterialTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

ModelTable::Entry* ModelFind(ModelTable* table, u32 id)
{
    return table->Find(id);
}

s32 ModelInsert(ModelTable* table, RigidModelData* const* model, u32 id)
{
    return table->Insert(model, id);
}

void ModelInsertAt(ModelTable* table, s32 index, RigidModelData* const* model, u32 id)
{
    table->InsertAt(index, model, id);
}

void ModelReleaseAll(ModelTable* table)
{
    table->ReleaseAll();
}

s32 ModelAddReference(ModelTable* table, const u32* id, RigidModelData* model)
{
    return table->AddReference(id, model);
}

RigidModelData* ModelAcquire(ModelTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void ModelRelease(ModelTable* table, const u32* id)
{
    table->Release(id);
}

void ModelReleaseItem(ModelTable* table, RigidModelData* model)
{
    table->ReleaseItem(model);
}

bool ModelExists(ModelTable* table, const u32* id)
{
    return table->Exists(id);
}

RigidModelData* ModelGet(ModelTable* table, const u32* id)
{
    return table->Get(id);
}

void ModelDestroy(ModelTable* table, u32 flags)
{
    table->Destroy(flags);
}

void ModelDestroyNamed(ModelTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void ModelDestroyBase(ModelTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

s32 RigidModelInsert(RigidModelTable* table, RigidModel* const* model, u32 id)
{
    return table->Insert(model, id);
}

void RigidModelInsertAt(RigidModelTable* table, s32 index, RigidModel* const* model, u32 id)
{
    table->InsertAt(index, model, id);
}

void RigidModelReleaseAll(RigidModelTable* table)
{
    table->ReleaseAll();
}

s32 RigidModelAddReference(RigidModelTable* table, const u32* id, RigidModel* model)
{
    return table->AddReference(id, model);
}

RigidModel* RigidModelAcquire(RigidModelTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void RigidModelRelease(RigidModelTable* table, const u32* id)
{
    table->Release(id);
}

void RigidModelReleaseItem(RigidModelTable* table, RigidModel* model)
{
    table->ReleaseItem(model);
}

bool RigidModelExists(RigidModelTable* table, const u32* id)
{
    return table->Exists(id);
}

RigidModel* RigidModelGet(RigidModelTable* table, const u32* id)
{
    return table->Get(id);
}

void RigidModelDestroy(RigidModelTable* table, u32 flags)
{
    table->Destroy(flags);
}

void RigidModelDestroyNamed(RigidModelTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void RigidModelDestroyBase(RigidModelTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

s32 SkinInsert(SkinTable* table, Skin* const* skin, u32 id)
{
    return table->Insert(skin, id);
}

void SkinInsertAt(SkinTable* table, s32 index, Skin* const* skin, u32 id)
{
    table->InsertAt(index, skin, id);
}

void SkinReleaseAll(SkinTable* table)
{
    table->ReleaseAll();
}

s32 SkinAddReference(SkinTable* table, const u32* id, Skin* skin)
{
    return table->AddReference(id, skin);
}

Skin* SkinAcquire(SkinTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void SkinRelease(SkinTable* table, const u32* id)
{
    table->Release(id);
}

void SkinReleaseItem(SkinTable* table, Skin* skin)
{
    table->ReleaseItem(skin);
}

bool SkinExists(SkinTable* table, const u32* id)
{
    return table->Exists(id);
}

Skin* SkinGet(SkinTable* table, const u32* id)
{
    return table->Get(id);
}

void SkinDestroy(SkinTable* table, u32 flags)
{
    table->Destroy(flags);
}

void SkinDestroyNamed(SkinTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void SkinDestroyBase(SkinTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

s32 BlendSkinInsert(BlendSkinTable* table, BlendSkin* const* blendSkin, u32 id)
{
    return table->Insert(blendSkin, id);
}

void BlendSkinInsertAt(BlendSkinTable* table, s32 index, BlendSkin* const* blendSkin, u32 id)
{
    table->InsertAt(index, blendSkin, id);
}

void BlendSkinReleaseAll(BlendSkinTable* table)
{
    table->ReleaseAll();
}

s32 BlendSkinAddReference(BlendSkinTable* table, const u32* id, BlendSkin* blendSkin)
{
    return table->AddReference(id, blendSkin);
}

BlendSkin* BlendSkinAcquire(BlendSkinTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void BlendSkinRelease(BlendSkinTable* table, const u32* id)
{
    table->Release(id);
}

void BlendSkinReleaseItem(BlendSkinTable* table, BlendSkin* blendSkin)
{
    table->ReleaseItem(blendSkin);
}

bool BlendSkinExists(BlendSkinTable* table, const u32* id)
{
    return table->Exists(id);
}

BlendSkin* BlendSkinGet(BlendSkinTable* table, const u32* id)
{
    return table->Get(id);
}

void BlendSkinDestroy(BlendSkinTable* table, u32 flags)
{
    table->Destroy(flags);
}

void BlendSkinDestroyNamed(BlendSkinTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void BlendSkinDestroyBase(BlendSkinTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

MeshTable::Entry* MeshFind(MeshTable* table, u32 id)
{
    return table->Find(id);
}

s32 MeshInsert(MeshTable* table, RigidModel* const* mesh, u32 id)
{
    return table->Insert(mesh, id);
}

void MeshInsertAt(MeshTable* table, s32 index, RigidModel* const* mesh, u32 id)
{
    table->InsertAt(index, mesh, id);
}

void MeshReleaseAll(MeshTable* table)
{
    table->ReleaseAll();
}

s32 MeshAddReference(MeshTable* table, const u32* id, RigidModel* mesh)
{
    return table->AddReference(id, mesh);
}

RigidModel* MeshAcquire(MeshTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void MeshRelease(MeshTable* table, const u32* id)
{
    table->Release(id);
}

void MeshReleaseItem(MeshTable* table, RigidModel* mesh)
{
    table->ReleaseItem(mesh);
}

bool MeshExists(MeshTable* table, const u32* id)
{
    return table->Exists(id);
}

RigidModel* MeshGet(MeshTable* table, const u32* id)
{
    return table->Get(id);
}

void MeshDestroy(MeshTable* table, u32 flags)
{
    table->Destroy(flags);
}

void MeshDestroyNamed(MeshTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void MeshDestroyBase(MeshTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

s32 LodInsert(LodTable* table, Lod* const* lod, u32 id)
{
    return table->Insert(lod, id);
}

void LodInsertAt(LodTable* table, s32 index, Lod* const* lod, u32 id)
{
    table->InsertAt(index, lod, id);
}

void LodReleaseAll(LodTable* table)
{
    table->ReleaseAll();
}

s32 LodAddReference(LodTable* table, const u32* id, Lod* lod)
{
    return table->AddReference(id, lod);
}

Lod* LodAcquire(LodTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void LodRelease(LodTable* table, const u32* id)
{
    table->Release(id);
}

void LodReleaseItem(LodTable* table, Lod* lod)
{
    table->ReleaseItem(lod);
}

bool LodExists(LodTable* table, const u32* id)
{
    return table->Exists(id);
}

Lod* LodGet(LodTable* table, const u32* id)
{
    return table->Get(id);
}

void LodDestroy(LodTable* table, u32 flags)
{
    table->Destroy(flags);
}

void LodDestroyNamed(LodTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void LodDestroyBase(LodTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

s32 SkyInsert(SkyTable* table, Sky* const* sky, u32 id)
{
    return table->Insert(sky, id);
}

void SkyInsertAt(SkyTable* table, s32 index, Sky* const* sky, u32 id)
{
    table->InsertAt(index, sky, id);
}

void SkyReleaseAll(SkyTable* table)
{
    table->ReleaseAll();
}

s32 SkyAddReference(SkyTable* table, const u32* id, Sky* sky)
{
    return table->AddReference(id, sky);
}

Sky* SkyAcquire(SkyTable* table, const u32* id, bool* created)
{
    return table->Acquire(id, created);
}

void SkyRelease(SkyTable* table, const u32* id)
{
    table->Release(id);
}

void SkyReleaseItem(SkyTable* table, Sky* sky)
{
    table->ReleaseItem(sky);
}

bool SkyExists(SkyTable* table, const u32* id)
{
    return table->Exists(id);
}

Sky* SkyGet(SkyTable* table, const u32* id)
{
    return table->Get(id);
}

void SkyDestroy(SkyTable* table, u32 flags)
{
    table->Destroy(flags);
}

void SkyDestroyNamed(SkyTable* table, u32 flags)
{
    table->DestroyNamed(flags);
}

void SkyDestroyBase(SkyTable* table, u32 flags)
{
    table->DestroyBase(flags);
}

void TextureMade(TextureTable*, const u32*, GameTexture*)
{
}

void TextureBaseMade(TextureTable*, const u32*, GameTexture*)
{
}

void MaterialMade(MaterialTable*, const u32*, MaterialResource*)
{
}

void MaterialBaseMade(MaterialTable*, const u32*, MaterialResource*)
{
}

void ModelMade(ModelTable*, const u32*, RigidModelData*)
{
}

void ModelBaseMade(ModelTable*, const u32*, RigidModelData*)
{
}

void RigidModelMade(RigidModelTable*, const u32*, RigidModel*)
{
}

void RigidModelBaseMade(RigidModelTable*, const u32*, RigidModel*)
{
}

void SkinMade(SkinTable*, const u32*, Skin*)
{
}

void SkinBaseMade(SkinTable*, const u32*, Skin*)
{
}

void BlendSkinMade(BlendSkinTable*, const u32*, BlendSkin*)
{
}

void BlendSkinBaseMade(BlendSkinTable*, const u32*, BlendSkin*)
{
}

void MeshMade(MeshTable*, const u32*, RigidModel*)
{
}

void MeshBaseMade(MeshTable*, const u32*, RigidModel*)
{
}

void LodMade(LodTable*, const u32*, Lod*)
{
}

void LodBaseMade(LodTable*, const u32*, Lod*)
{
}

void SkyMade(SkyTable* table, const u32* id, Sky* sky)
{
    String path;
    path.string = nullptr;
    path.length = 0;
    path.capacity = 0;
    StringAssign(&path, table->name.string);
    StringAppend(&path, "\\");
    String number;
    StringConstructNumber(&number, *id);
    StringAppend(&path, number.string);
    StringDestroy(&number);
    GameReadersStorage* storage = g_ReadersStorages[MainReaders];
    auto* reader = static_cast<GraphicsResourceReader<SkyKind>*>(MemoryAllocate(sizeof(GraphicsResourceReader<SkyKind>)));
    reader->item = sky;
    reader->vtable = g_SkySectionReaderVTable;
    reader->table = table;
    auto* file = static_cast<SubItemsReader*>(MemoryAllocate(sizeof(SubItemsReader)));
    file = SubItemsReader::ConstructFile(file, path.string, reader,
                                         SubItemsReaderOptions::ClosesFile | SubItemsReaderOptions::OnDisk);
    AddItemReaderToReaderStorage(storage, file, QueueBack);
    StringDestroy(&path);
}

void SkyNamedMade(SkyTable*, const u32*, Sky*)
{
}

void SkyBaseMade(SkyTable*, const u32*, Sky*)
{
}

void TextureReaderDestroy(GraphicsKindReader<TextureKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 TextureReaderCount(GraphicsKindReader<TextureKind>* reader)
{
    return reader->Count();
}

u32 TextureReaderSectionType(GraphicsKindReader<TextureKind>*)
{
    return GraphicsKindSectionType;
}

bool TextureReaderCanRead(GraphicsKindReader<TextureKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* TextureReaderGetReader(GraphicsKindReader<TextureKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void TextureReaderClear(GraphicsKindReader<TextureKind>* reader)
{
    reader->Clear();
}

void TextureSectionReaderDestroy(GraphicsResourceReader<TextureKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void TextureSectionReaderRead(GraphicsResourceReader<TextureKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void MaterialReaderDestroy(GraphicsKindReader<MaterialKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 MaterialReaderCount(GraphicsKindReader<MaterialKind>* reader)
{
    return reader->Count();
}

u32 MaterialReaderSectionType(GraphicsKindReader<MaterialKind>*)
{
    return GraphicsKindSectionType;
}

bool MaterialReaderCanRead(GraphicsKindReader<MaterialKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* MaterialReaderGetReader(GraphicsKindReader<MaterialKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void MaterialReaderClear(GraphicsKindReader<MaterialKind>* reader)
{
    reader->Clear();
}

void MaterialSectionReaderDestroy(GraphicsResourceReader<MaterialKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void MaterialSectionReaderRead(GraphicsResourceReader<MaterialKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void ModelReaderDestroy(GraphicsKindReader<ModelKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 ModelReaderCount(GraphicsKindReader<ModelKind>* reader)
{
    return reader->Count();
}

u32 ModelReaderSectionType(GraphicsKindReader<ModelKind>*)
{
    return GraphicsKindSectionType;
}

bool ModelReaderCanRead(GraphicsKindReader<ModelKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* ModelReaderGetReader(GraphicsKindReader<ModelKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void ModelReaderClear(GraphicsKindReader<ModelKind>* reader)
{
    reader->Clear();
}

void ModelSectionReaderDestroy(GraphicsResourceReader<ModelKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void ModelSectionReaderRead(GraphicsResourceReader<ModelKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void RigidModelReaderDestroy(GraphicsKindReader<RigidModelKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 RigidModelReaderCount(GraphicsKindReader<RigidModelKind>* reader)
{
    return reader->Count();
}

u32 RigidModelReaderSectionType(GraphicsKindReader<RigidModelKind>*)
{
    return GraphicsKindSectionType;
}

bool RigidModelReaderCanRead(GraphicsKindReader<RigidModelKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* RigidModelReaderGetReader(GraphicsKindReader<RigidModelKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void RigidModelReaderClear(GraphicsKindReader<RigidModelKind>* reader)
{
    reader->Clear();
}

void RigidModelSectionReaderDestroy(GraphicsResourceReader<RigidModelKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void RigidModelSectionReaderRead(GraphicsResourceReader<RigidModelKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void SkinReaderDestroy(GraphicsKindReader<SkinKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 SkinReaderCount(GraphicsKindReader<SkinKind>* reader)
{
    return reader->Count();
}

u32 SkinReaderSectionType(GraphicsKindReader<SkinKind>*)
{
    return GraphicsKindSectionType;
}

bool SkinReaderCanRead(GraphicsKindReader<SkinKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* SkinReaderGetReader(GraphicsKindReader<SkinKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void SkinReaderClear(GraphicsKindReader<SkinKind>* reader)
{
    reader->Clear();
}

void SkinSectionReaderDestroy(GraphicsResourceReader<SkinKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void SkinSectionReaderRead(GraphicsResourceReader<SkinKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void BlendSkinReaderDestroy(GraphicsKindReader<BlendSkinKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 BlendSkinReaderCount(GraphicsKindReader<BlendSkinKind>* reader)
{
    return reader->Count();
}

u32 BlendSkinReaderSectionType(GraphicsKindReader<BlendSkinKind>*)
{
    return GraphicsKindSectionType;
}

bool BlendSkinReaderCanRead(GraphicsKindReader<BlendSkinKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* BlendSkinReaderGetReader(GraphicsKindReader<BlendSkinKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void BlendSkinReaderClear(GraphicsKindReader<BlendSkinKind>* reader)
{
    reader->Clear();
}

void BlendSkinSectionReaderDestroy(GraphicsResourceReader<BlendSkinKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void BlendSkinSectionReaderRead(GraphicsResourceReader<BlendSkinKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void MeshReaderDestroy(GraphicsKindReader<MeshKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 MeshReaderCount(GraphicsKindReader<MeshKind>* reader)
{
    return reader->Count();
}

u32 MeshReaderSectionType(GraphicsKindReader<MeshKind>*)
{
    return GraphicsKindSectionType;
}

bool MeshReaderCanRead(GraphicsKindReader<MeshKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* MeshReaderGetReader(GraphicsKindReader<MeshKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void MeshReaderClear(GraphicsKindReader<MeshKind>* reader)
{
    reader->Clear();
}

void MeshSectionReaderDestroy(GraphicsResourceReader<MeshKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void MeshSectionReaderRead(GraphicsResourceReader<MeshKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void LodReaderDestroy(GraphicsKindReader<LodKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 LodReaderCount(GraphicsKindReader<LodKind>* reader)
{
    return reader->Count();
}

u32 LodReaderSectionType(GraphicsKindReader<LodKind>*)
{
    return GraphicsKindSectionType;
}

bool LodReaderCanRead(GraphicsKindReader<LodKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* LodReaderGetReader(GraphicsKindReader<LodKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void LodReaderClear(GraphicsKindReader<LodKind>* reader)
{
    reader->Clear();
}

void LodSectionReaderDestroy(GraphicsResourceReader<LodKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void LodSectionReaderRead(GraphicsResourceReader<LodKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}

void SkyReaderDestroy(GraphicsKindReader<SkyKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

u32 SkyReaderCount(GraphicsKindReader<SkyKind>* reader)
{
    return reader->Count();
}

u32 SkyReaderSectionType(GraphicsKindReader<SkyKind>*)
{
    return GraphicsKindSectionType;
}

bool SkyReaderCanRead(GraphicsKindReader<SkyKind>*, u32 type)
{
    return type == GraphicsKindSectionType;
}

SectionReader* SkyReaderGetReader(GraphicsKindReader<SkyKind>* reader, s32 index, ItemHeader* header, s32* size)
{
    return reader->GetReader(index, header, size);
}

void SkyReaderClear(GraphicsKindReader<SkyKind>* reader)
{
    reader->Clear();
}

void SkySectionReaderDestroy(GraphicsResourceReader<SkyKind>* reader, u32 flags)
{
    reader->Destroy(flags);
}

void SkySectionReaderRead(GraphicsResourceReader<SkyKind>* reader, u8* data, u32 size, ReaderStack* readers)
{
    reader->Read(data, size, readers);
}
