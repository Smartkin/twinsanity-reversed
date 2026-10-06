// The retail executable's .sdata from 0x309880 to 0x30A460, converted from the split
// once, in the retail order: the game's code may go from one object into the next, so they stay where the retail
// executable had them, each under its retail name
#include "common.h"
#include "gcc2.h"
#include "retaildata.h"

// The other data and functions it points at
extern u8 Ref_D_002EC400[] RETAIL(D_002EC400);
extern u8 Ref_D_002F4868[] RETAIL(D_002F4868);
extern u8 Ref_D_002F4978[] RETAIL(D_002F4978);
extern u8 Ref_D_002F4988[] RETAIL(D_002F4988);
extern u8 Ref_D_002F49A0[] RETAIL(D_002F49A0);
extern u8 Ref_D_002F49B0[] RETAIL(D_002F49B0);
extern u8 Ref_D_002F49D0[] RETAIL(D_002F49D0);
extern u8 Ref_D_002F49E8[] RETAIL(D_002F49E8);
extern u8 Ref_G_BLEND_SHAPE_FLOATS[] RETAIL(G_BLEND_SHAPE_FLOATS);

namespace RetailData
{
struct D_00309940_Fields
{
    u32 v0;
    u32 v1;
    const void* v2;
    const void* v3;
};

extern u32 D_00309880[1] RETAIL(D_00309880);
extern const void* D_00309884[1] RETAIL(D_00309884);
extern u32 G_GameController[1] RETAIL(G_GameController);
extern u32 G_GameController_0030988C[1] RETAIL(G_GameController_0030988C);
extern u32 G_GameController_00309890[1] RETAIL(G_GameController_00309890);
extern u32 D_00309894[1] RETAIL(D_00309894);
extern u8 RB_String[3] RETAIL(RB_String);
extern u8 D_0030989B[5] RETAIL(D_0030989B);
extern char BATCH_String[6] RETAIL(BATCH_String);
extern u16 D_003098A6[1] RETAIL(D_003098A6);
extern char D_003098A8[8] RETAIL(D_003098A8);
extern char D_003098B0[8] RETAIL(D_003098B0);
extern char D_003098B8[8] RETAIL(D_003098B8);
extern char D_003098C0[8] RETAIL(D_003098C0);
extern char D_003098C8[16] RETAIL(D_003098C8);
extern char D_003098D8[4] RETAIL(D_003098D8);
extern s32 D_003098DC[1] RETAIL(D_003098DC);
extern s32 D_003098E0[1] RETAIL(D_003098E0);
extern u32 D_003098E4[1] RETAIL(D_003098E4);
extern s32 D_003098E8[2] RETAIL(D_003098E8);
extern char D_003098F0[12] RETAIL(D_003098F0);
extern u32 G_UnkPlayableCharObjInstCxt[1] RETAIL(G_UnkPlayableCharObjInstCxt);
extern u32 G_UnkCreationHelper[1] RETAIL(G_UnkCreationHelper);
extern u32 G_UnkPlayableCharObjInstCxt2[1] RETAIL(G_UnkPlayableCharObjInstCxt2);
extern u32 G_UnkCreationHelper2[1] RETAIL(G_UnkCreationHelper2);
extern u32 D_0030990C[2] RETAIL(D_0030990C);
extern u32 G_GameController_00309914[1] RETAIL(G_GameController_00309914);
extern u32 D_00309918[3] RETAIL(D_00309918);
extern u32 D_00309924[1] RETAIL(D_00309924);
extern u32 D_00309928[1] RETAIL(D_00309928);
extern u32 D_0030992C[1] RETAIL(D_0030992C);
extern u32 D_00309930[2] RETAIL(D_00309930);
extern char D_00309938[8] RETAIL(D_00309938);
extern D_00309940_Fields D_00309940 RETAIL(D_00309940);
extern u32 G_GameController_00309950[1] RETAIL(G_GameController_00309950);
extern u32 D_00309954[1] RETAIL(D_00309954);
extern char D_00309958[8] RETAIL(D_00309958);
extern u32 D_00309960[2] RETAIL(D_00309960);
extern u32 D_00309968[2] RETAIL(D_00309968);
extern u32 D_00309970[2] RETAIL(D_00309970);
extern u32 D_00309978[2] RETAIL(D_00309978);
extern u32 D_00309980[2] RETAIL(D_00309980);
extern char D_00309988[8] RETAIL(D_00309988);
extern char D_00309990[8] RETAIL(D_00309990);
extern char D_00309998[8] RETAIL(D_00309998);
extern const void* D_003099A0[1] RETAIL(D_003099A0);
extern const void* D_003099A4[1] RETAIL(D_003099A4);
extern u32 G_PrecompShader_0xE_2[1] RETAIL(G_PrecompShader_0xE_2);
extern u32 D_003099AC[1] RETAIL(D_003099AC);
extern char D_003099B0[8] RETAIL(D_003099B0);
extern char D_003099B8[8] RETAIL(D_003099B8);
extern char D_003099C0[8] RETAIL(D_003099C0);
extern char D_003099C8[8] RETAIL(D_003099C8);
extern char D_003099D0[8] RETAIL(D_003099D0);
extern char D_003099D8[8] RETAIL(D_003099D8);
extern char D_003099E0[8] RETAIL(D_003099E0);
extern char D_003099E8[8] RETAIL(D_003099E8);
extern char D_003099F0[8] RETAIL(D_003099F0);
extern char D_003099F8[8] RETAIL(D_003099F8);
extern char D_00309A00[8] RETAIL(D_00309A00);
extern char D_00309A08[8] RETAIL(D_00309A08);
extern char D_00309A10[8] RETAIL(D_00309A10);
extern char D_00309A18[8] RETAIL(D_00309A18);
extern char D_00309A20[8] RETAIL(D_00309A20);
extern char D_00309A28[8] RETAIL(D_00309A28);
extern char D_00309A30[8] RETAIL(D_00309A30);
extern char D_00309A38[8] RETAIL(D_00309A38);
extern u32 D_00309A40[1] RETAIL(D_00309A40);
extern f32 D_00309A44[1] RETAIL(D_00309A44);
extern char D_00309A48[8] RETAIL(D_00309A48);
extern const void* D_00309A50[1] RETAIL(D_00309A50);
extern const void* D_00309A54[1] RETAIL(D_00309A54);
extern const void* D_00309A58[1] RETAIL(D_00309A58);
extern const void* D_00309A5C[3] RETAIL(D_00309A5C);
extern char D_00309A68[8] RETAIL(D_00309A68);
extern const void* D_00309A70[2] RETAIL(D_00309A70);
extern u32 D_00309A78[2] RETAIL(D_00309A78);
extern char D_00309A80[8] RETAIL(D_00309A80);
extern char D_00309A88[8] RETAIL(D_00309A88);
extern u32 D_00309A90[1] RETAIL(D_00309A90);
extern u32 D_00309A94[5] RETAIL(D_00309A94);
extern u8 D_00309AA8[9] RETAIL(D_00309AA8);
extern u8 D_00309AB1[1] RETAIL(D_00309AB1);
extern u16 D_00309AB2[1] RETAIL(D_00309AB2);
extern u32 g_GameContext[1] RETAIL(g_GameContext);
extern u32 G_GameState_[1] RETAIL(G_GameState_);
extern u32 G_GamePadController[1] RETAIL(G_GamePadController);
extern u32 G_GameMovieController[1] RETAIL(G_GameMovieController);
extern u32 G_UnkStruct_5C0[1] RETAIL(G_UnkStruct_5C0);
extern u32 G_GameRendererController[1] RETAIL(G_GameRendererController);
extern u32 G_GameClockController[1] RETAIL(G_GameClockController);
extern u32 G_GameResourcesObjectPointer[1] RETAIL(G_GameResourcesObjectPointer);
extern u32 UnkStruct_0x14_HasSomeDataPTRs[1] RETAIL(UnkStruct_0x14_HasSomeDataPTRs);
extern u32 G_ChunkLoadingManager_[1] RETAIL(G_ChunkLoadingManager_);
extern u32 G_VideoController[1] RETAIL(G_VideoController);
extern u32 G_Renderer_[1] RETAIL(G_Renderer_);
extern s32 D_00309AE4[1] RETAIL(D_00309AE4);
extern s32 D_00309AE8[1] RETAIL(D_00309AE8);
extern u32 D_00309AEC[1] RETAIL(D_00309AEC);
extern u32 GlobalLanguagesAmount[1] RETAIL(GlobalLanguagesAmount);
extern u32 CurrentLanguageIndex[1] RETAIL(CurrentLanguageIndex);
extern char G_unkLanguagesStruct[4] RETAIL(G_unkLanguagesStruct);
extern u32 D_00309AFC[1] RETAIL(D_00309AFC);
extern u32 D_00309B00[2] RETAIL(D_00309B00);
extern char D_00309B08[8] RETAIL(D_00309B08);
extern char D_00309B10[4] RETAIL(D_00309B10);
extern char D_00309B14[4] RETAIL(D_00309B14);
extern u32 D_00309B18[1] RETAIL(D_00309B18);
extern u32 D_00309B1C[1] RETAIL(D_00309B1C);
extern u32 D_00309B20[10] RETAIL(D_00309B20);
extern u32 D_00309B48[1] RETAIL(D_00309B48);
extern u32 D_00309B4C[6] RETAIL(D_00309B4C);
extern f32 G_GlobalClockSpeedScale[1] RETAIL(G_GlobalClockSpeedScale);
extern u32 D_00309B68[2] RETAIL(D_00309B68);
extern u32 G_CPU_T0_OVERFLOW_COUNTER[2] RETAIL(G_CPU_T0_OVERFLOW_COUNTER);
extern u8 G_Timer0_Set[1] RETAIL(G_Timer0_Set);
extern u8 D_00309B79[3] RETAIL(D_00309B79);
extern f32 G_TICKS_TO_TIME[1] RETAIL(G_TICKS_TO_TIME);
extern f32 G_TIME_TO_TICKS[1] RETAIL(G_TIME_TO_TICKS);
extern u32 D_00309B84[1] RETAIL(D_00309B84);
extern u32 D_00309B88[1] RETAIL(D_00309B88);
extern u32 D_00309B8C[1] RETAIL(D_00309B8C);
extern u32 g_GameNodeList[1] RETAIL(g_GameNodeList);
extern u32 D_00309B94[1] RETAIL(D_00309B94);
extern u32 D_00309B98[1] RETAIL(D_00309B98);
extern u32 G_InstContext2[1] RETAIL(G_InstContext2);
extern u32 D_00309BA0[1] RETAIL(D_00309BA0);
extern u32 G_SizeOfByteArrayAt_0x3D3F28_0x18[1] RETAIL(G_SizeOfByteArrayAt_0x3D3F28_0x18);
extern u32 D_00309BA8[3] RETAIL(D_00309BA8);
extern u32 D_00309BB4[1] RETAIL(D_00309BB4);
extern u32 D_00309BB8[1] RETAIL(D_00309BB8);
extern u32 G_ParticleBinReader[1] RETAIL(G_ParticleBinReader);
extern u32 D_00309BC0[1] RETAIL(D_00309BC0);
extern u32 G_DMA_ByteStream_Beg_[1] RETAIL(G_DMA_ByteStream_Beg_);
extern u32 G_DMA_ByteStream_CurPosition_[1] RETAIL(G_DMA_ByteStream_CurPosition_);
extern u32 D_00309BCC[1] RETAIL(D_00309BCC);
extern u32 D_00309BD0[1] RETAIL(D_00309BD0);
extern u32 D_00309BD4[1] RETAIL(D_00309BD4);
extern u8 G_IsPCRTC_Ready[1] RETAIL(G_IsPCRTC_Ready);
extern u8 D_00309BD9[3] RETAIL(D_00309BD9);
extern u32 G_FontRendererRel[1] RETAIL(G_FontRendererRel);
extern u32 G_RendRel[1] RETAIL(G_RendRel);
extern u32 G_UnkChunkData_[1] RETAIL(G_UnkChunkData_);
extern u32 D_00309BE8[1] RETAIL(D_00309BE8);
extern u32 D_00309BEC[1] RETAIL(D_00309BEC);
extern u32 D_00309BF0[1] RETAIL(D_00309BF0);
extern u32 D_00309BF4[1] RETAIL(D_00309BF4);
extern u32 G_0x320_ArrayOf_0x14_SizeStruct[1] RETAIL(G_0x320_ArrayOf_0x14_SizeStruct);
extern u32 D_00309BFC[11] RETAIL(D_00309BFC);
extern char D_00309C28[9] RETAIL(D_00309C28);
extern u8 D_00309C31[1] RETAIL(D_00309C31);
extern u8 D_00309C32[2] RETAIL(D_00309C32);
extern f32 D_00309C34[1] RETAIL(D_00309C34);
extern u32 D_00309C38[1] RETAIL(D_00309C38);
extern u32 D_00309C3C[3] RETAIL(D_00309C3C);
extern u32 D_00309C48[2] RETAIL(D_00309C48);
extern u8 D_00309C50[1] RETAIL(D_00309C50);
extern u8 D_00309C51[1] RETAIL(D_00309C51);
extern u8 D_00309C52[2] RETAIL(D_00309C52);
extern s32 G_ParticlesAmountLoaded_[1] RETAIL(G_ParticlesAmountLoaded_);
extern s32 D_00309C58[1] RETAIL(D_00309C58);
extern f32 ParticleTime[1] RETAIL(ParticleTime);
extern s32 ParticleFrameCounter[1] RETAIL(ParticleFrameCounter);
extern f32 ParticleFrameDelta[1] RETAIL(ParticleFrameDelta);
extern s32 D_00309C68[1] RETAIL(D_00309C68);
extern s32 D_00309C6C[1] RETAIL(D_00309C6C);
extern s32 D_00309C70[4] RETAIL(D_00309C70);
extern u32 ParticleRandomSeed[2] RETAIL(ParticleRandomSeed);
extern s32 D_00309C88[2] RETAIL(D_00309C88);
extern s32 D_00309C90[1] RETAIL(D_00309C90);
extern s32 D_00309C94[1] RETAIL(D_00309C94);
extern s32 G_ParticleInstancesAmount_[1] RETAIL(G_ParticleInstancesAmount_);
extern s32 D_00309C9C[3] RETAIL(D_00309C9C);
extern u32 D_00309CA8[2] RETAIL(D_00309CA8);
extern u32 D_00309CB0[1] RETAIL(D_00309CB0);
extern u32 D_00309CB4[1] RETAIL(D_00309CB4);
extern u32 D_00309CB8[1] RETAIL(D_00309CB8);
extern u32 D_00309CBC[1] RETAIL(D_00309CBC);
extern u32 D_00309CC0[1] RETAIL(D_00309CC0);
extern u32 D_00309CC4[1] RETAIL(D_00309CC4);
extern u32 D_00309CC8[1] RETAIL(D_00309CC8);
extern u32 D_00309CCC[4] RETAIL(D_00309CCC);
extern u32 D_00309CDC[2] RETAIL(D_00309CDC);
extern u32 D_00309CE4[1] RETAIL(D_00309CE4);
extern u32 D_00309CE8[1] RETAIL(D_00309CE8);
extern u32 D_00309CEC[1] RETAIL(D_00309CEC);
extern u32 D_00309CF0[1] RETAIL(D_00309CF0);
extern u32 D_00309CF4[1] RETAIL(D_00309CF4);
extern u32 D_00309CF8[1] RETAIL(D_00309CF8);
extern u32 G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN[1] RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN);
extern u32 G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END[1] RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END);
extern u32 G_CONST_D[1] RETAIL(G_CONST_D);
extern u32 D_00309D08[5] RETAIL(D_00309D08);
extern u32 D_00309D1C[2] RETAIL(D_00309D1C);
extern u32 D_00309D24[2] RETAIL(D_00309D24);
extern u32 D_00309D2C[4] RETAIL(D_00309D2C);
extern u32 D_00309D3C[5] RETAIL(D_00309D3C);
extern char D_00309D50[8] RETAIL(D_00309D50);
extern char D_00309D58[8] RETAIL(D_00309D58);
extern char D_00309D60[8] RETAIL(D_00309D60);
extern u32 D_00309D68[1] RETAIL(D_00309D68);
extern u32 D_00309D6C[1] RETAIL(D_00309D6C);
extern u32 D_00309D70[1] RETAIL(D_00309D70);
extern u32 D_00309D74[1] RETAIL(D_00309D74);
extern u32 D_00309D78[3] RETAIL(D_00309D78);
extern s32 D_00309D84[1] RETAIL(D_00309D84);
extern u32 D_00309D88[2] RETAIL(D_00309D88);
extern char D_00309D90[20] RETAIL(D_00309D90);
extern u32 D_00309DA4[1] RETAIL(D_00309DA4);
extern u32 D_00309DA8[1] RETAIL(D_00309DA8);
extern u32 D_00309DAC[1] RETAIL(D_00309DAC);
extern u32 D_00309DB0[1] RETAIL(D_00309DB0);
extern u32 D_00309DB4[1] RETAIL(D_00309DB4);
extern u32 D_00309DB8[1] RETAIL(D_00309DB8);
extern u32 D_00309DBC[1] RETAIL(D_00309DBC);
extern u32 D_00309DC0[1] RETAIL(D_00309DC0);
extern u32 D_00309DC4[1] RETAIL(D_00309DC4);
extern u32 D_00309DC8[1] RETAIL(D_00309DC8);
extern u32 D_00309DCC[3] RETAIL(D_00309DCC);
extern u32 D_00309DD8[1] RETAIL(D_00309DD8);
extern u32 D_00309DDC[1] RETAIL(D_00309DDC);
extern u32 D_00309DE0[1] RETAIL(D_00309DE0);
extern u32 D_00309DE4[1] RETAIL(D_00309DE4);
extern u32 D_00309DE8[1] RETAIL(D_00309DE8);
extern u32 D_00309DEC[1] RETAIL(D_00309DEC);
extern u32 D_00309DF0[1] RETAIL(D_00309DF0);
extern u32 D_00309DF4[1] RETAIL(D_00309DF4);
extern u32 D_00309DF8[1] RETAIL(D_00309DF8);
extern u32 D_00309DFC[1] RETAIL(D_00309DFC);
extern u32 D_00309E00[1] RETAIL(D_00309E00);
extern u32 D_00309E04[1] RETAIL(D_00309E04);
extern u32 D_00309E08[1] RETAIL(D_00309E08);
extern u32 D_00309E0C[1] RETAIL(D_00309E0C);
extern u32 G_MicroCode_7_Index[1] RETAIL(G_MicroCode_7_Index);
extern u32 G_MicroCode_8_Index[1] RETAIL(G_MicroCode_8_Index);
extern u32 G_MicroCode_9_Index[1] RETAIL(G_MicroCode_9_Index);
extern u32 D_00309E1C[1] RETAIL(D_00309E1C);
extern u32 G_MicroCode_6_Index[1] RETAIL(G_MicroCode_6_Index);
extern u32 D_00309E24[1] RETAIL(D_00309E24);
extern u32 D_00309E28[1] RETAIL(D_00309E28);
extern u32 D_00309E2C[1] RETAIL(D_00309E2C);
extern u32 D_00309E30[1] RETAIL(D_00309E30);
extern u32 D_00309E34[9] RETAIL(D_00309E34);
extern u32 G_UnkStructOfSize_0xe0[1] RETAIL(G_UnkStructOfSize_0xe0);
extern u32 D_00309E5C[1] RETAIL(D_00309E5C);
extern u32 D_00309E60[1] RETAIL(D_00309E60);
extern f32 D_00309E64[1] RETAIL(D_00309E64);
extern u32 D_00309E68[1] RETAIL(D_00309E68);
extern s32 D_00309E6C[1] RETAIL(D_00309E6C);
extern f32 D_00309E70[1] RETAIL(D_00309E70);
extern f32 D_00309E74[1] RETAIL(D_00309E74);
extern u32 D_00309E78[1] RETAIL(D_00309E78);
extern u32 D_00309E7C[1] RETAIL(D_00309E7C);
extern u32 D_00309E80[1] RETAIL(D_00309E80);
extern u32 D_00309E84[1] RETAIL(D_00309E84);
extern u32 D_00309E88[2] RETAIL(D_00309E88);
extern u32 D_00309E90[1] RETAIL(D_00309E90);
extern u32 D_00309E94[1] RETAIL(D_00309E94);
extern u32 D_00309E98[2] RETAIL(D_00309E98);
extern u32 D_00309EA0[6] RETAIL(D_00309EA0);
extern u32 D_00309EB8[1] RETAIL(D_00309EB8);
extern u32 D_00309EBC[1] RETAIL(D_00309EBC);
extern u32 D_00309EC0[2] RETAIL(D_00309EC0);
extern u32 D_00309EC8[1] RETAIL(D_00309EC8);
extern u32 D_00309ECC[1] RETAIL(D_00309ECC);
extern u32 D_00309ED0[1] RETAIL(D_00309ED0);
extern u32 D_00309ED4[1] RETAIL(D_00309ED4);
extern u32 D_00309ED8[1] RETAIL(D_00309ED8);
extern u32 D_00309EDC[1] RETAIL(D_00309EDC);
extern u32 D_00309EE0[1] RETAIL(D_00309EE0);
extern u32 D_00309EE4[1] RETAIL(D_00309EE4);
extern u32 D_00309EE8[1] RETAIL(D_00309EE8);
extern u32 D_00309EEC[1] RETAIL(D_00309EEC);
extern u32 D_00309EF0[1] RETAIL(D_00309EF0);
extern u32 D_00309EF4[1] RETAIL(D_00309EF4);
extern u32 D_00309EF8[2] RETAIL(D_00309EF8);
extern u32 D_00309F00[1] RETAIL(D_00309F00);
extern u32 D_00309F04[1] RETAIL(D_00309F04);
extern u32 D_00309F08[2] RETAIL(D_00309F08);
extern u32 D_00309F10[2] RETAIL(D_00309F10);
extern u32 D_00309F18[2] RETAIL(D_00309F18);
extern u8 D_00309F20[1] RETAIL(D_00309F20);
extern u8 D_00309F21[1] RETAIL(D_00309F21);
extern u8 D_00309F22[2] RETAIL(D_00309F22);
extern u32 D_00309F24[1] RETAIL(D_00309F24);
extern u32 D_00309F28[1] RETAIL(D_00309F28);
extern u32 D_00309F2C[1] RETAIL(D_00309F2C);
extern u32 D_00309F30[1] RETAIL(D_00309F30);
extern u32 D_00309F34[1] RETAIL(D_00309F34);
extern u32 D_00309F38[1] RETAIL(D_00309F38);
extern u32 D_00309F3C[1] RETAIL(D_00309F3C);
extern u32 D_00309F40[1] RETAIL(D_00309F40);
extern u32 D_00309F44[1] RETAIL(D_00309F44);
extern u32 D_00309F48[1] RETAIL(D_00309F48);
extern u32 D_00309F4C[1] RETAIL(D_00309F4C);
extern u32 D_00309F50[1] RETAIL(D_00309F50);
extern u32 D_00309F54[1] RETAIL(D_00309F54);
extern u32 D_00309F58[1] RETAIL(D_00309F58);
extern u32 D_00309F5C[1] RETAIL(D_00309F5C);
extern u32 D_00309F60[1] RETAIL(D_00309F60);
extern u32 D_00309F64[1] RETAIL(D_00309F64);
extern u32 D_00309F68[2] RETAIL(D_00309F68);
extern u32 D_00309F70[1] RETAIL(D_00309F70);
extern u32 D_00309F74[1] RETAIL(D_00309F74);
extern u8 D_00309F78[1] RETAIL(D_00309F78);
extern u8 D_00309F79[1] RETAIL(D_00309F79);
extern u8 D_00309F7A[6] RETAIL(D_00309F7A);
extern u32 D_00309F80[2] RETAIL(D_00309F80);
extern u32 D_00309F88[2] RETAIL(D_00309F88);
extern u32 UnkGlobalCounter[1] RETAIL(UnkGlobalCounter);
extern u32 D_00309F94[1] RETAIL(D_00309F94);
extern u32 D_00309F98[1] RETAIL(D_00309F98);
extern u32 D_00309F9C[1] RETAIL(D_00309F9C);
extern u32 D_00309FA0[1] RETAIL(D_00309FA0);
extern u32 D_00309FA4[1] RETAIL(D_00309FA4);
extern u32 D_00309FA8[1] RETAIL(D_00309FA8);
extern u32 D_00309FAC[6] RETAIL(D_00309FAC);
extern u32 D_00309FC4[1] RETAIL(D_00309FC4);
extern u8 D_00309FC8[1] RETAIL(D_00309FC8);
extern u8 D_00309FC9[3] RETAIL(D_00309FC9);
extern u32 D_00309FCC[2] RETAIL(D_00309FCC);
extern u32 D_00309FD4[1] RETAIL(D_00309FD4);
extern u32 D_00309FD8[2] RETAIL(D_00309FD8);
extern u32 D_00309FE0[1] RETAIL(D_00309FE0);
extern u32 D_00309FE4[1] RETAIL(D_00309FE4);
extern u8 D_00309FE8[10] RETAIL(D_00309FE8);
extern u8 D_00309FF2[1] RETAIL(D_00309FF2);
extern u8 D_00309FF3[5] RETAIL(D_00309FF3);
extern char D_00309FF8[16] RETAIL(D_00309FF8);
extern char D_0030A008[8] RETAIL(D_0030A008);
extern const void* D_0030A010[2] RETAIL(D_0030A010);
extern char D_0030A018[8] RETAIL(D_0030A018);
extern char D_0030A020[8] RETAIL(D_0030A020);
extern char D_0030A028[8] RETAIL(D_0030A028);
extern char D_0030A030[8] RETAIL(D_0030A030);
extern char D_0030A038[12] RETAIL(D_0030A038);
extern u8 HullBuilderPointCount[1] RETAIL(HullBuilderPointCount);
extern u8 D_0030A045[3] RETAIL(D_0030A045);
extern u32 HullBuilderCounts[4] RETAIL(HullBuilderCounts);
extern u32 D_0030A058[1] RETAIL(D_0030A058);
extern u32 D_0030A05C[1] RETAIL(D_0030A05C);
extern u32 G_DynamicSceneryClockIndex[1] RETAIL(G_DynamicSceneryClockIndex);
extern u32 D_0030A064[5] RETAIL(D_0030A064);
extern char D_0030A078[28] RETAIL(D_0030A078);
extern u32 D_0030A094[4] RETAIL(D_0030A094);
extern f32 D_0030A0A4[1] RETAIL(D_0030A0A4);
extern u32 G_ChunkManager[1] RETAIL(G_ChunkManager);
extern u32 G_StateDepth[1] RETAIL(G_StateDepth);
extern u32 D_0030A0B0[2] RETAIL(D_0030A0B0);
extern u32 G_ScriptStateBody_Ptr[1] RETAIL(G_ScriptStateBody_Ptr);
extern u32 G_ScriptCondition_Ptr[1] RETAIL(G_ScriptCondition_Ptr);
extern u32 G_ScriptTable[1] RETAIL(G_ScriptTable);
extern f32 D_0030A0C4[1] RETAIL(D_0030A0C4);
extern u32 G_ChunkManager_0030A0C8[1] RETAIL(G_ChunkManager_0030A0C8);
extern u8 D_0030A0CC[29] RETAIL(D_0030A0CC);
extern u8 D_0030A0E9[15] RETAIL(D_0030A0E9);
extern u32 D_0030A0F8[1] RETAIL(D_0030A0F8);
extern s32 D_0030A0FC[1] RETAIL(D_0030A0FC);
extern u32 G_CurrentScriptCall[1] RETAIL(G_CurrentScriptCall);
extern u32 D_0030A104[2] RETAIL(D_0030A104);
extern u32 D_0030A10C[1] RETAIL(D_0030A10C);
extern u32 G_MiniBigBoi[1] RETAIL(G_MiniBigBoi);
extern u32 D_0030A114[1] RETAIL(D_0030A114);
extern u32 D_0030A118[1] RETAIL(D_0030A118);
extern u32 D_0030A11C[1] RETAIL(D_0030A11C);
extern u32 D_0030A120[4] RETAIL(D_0030A120);
extern char D_0030A130[4] RETAIL(D_0030A130);
extern u32 D_0030A134[1] RETAIL(D_0030A134);
extern char D_0030A138[4] RETAIL(D_0030A138);
extern u32 D_0030A13C[4] RETAIL(D_0030A13C);
extern u32 G_GameResourcesManager_[1] RETAIL(G_GameResourcesManager_);
extern u32 G_ChunkManager_[1] RETAIL(G_ChunkManager_);
extern u32 D_0030A154[1] RETAIL(D_0030A154);
extern char D_0030A158[8] RETAIL(D_0030A158);
extern const void* D_0030A160[2] RETAIL(D_0030A160);
extern char D_0030A168[8] RETAIL(D_0030A168);
extern char D_0030A170[8] RETAIL(D_0030A170);
extern char D_0030A178[8] RETAIL(D_0030A178);
extern char D_0030A180[8] RETAIL(D_0030A180);
extern char D_0030A188[8] RETAIL(D_0030A188);
extern char D_0030A190[8] RETAIL(D_0030A190);
extern char D_0030A198[8] RETAIL(D_0030A198);
extern char D_0030A1A0[8] RETAIL(D_0030A1A0);
extern char D_0030A1A8[8] RETAIL(D_0030A1A8);
extern u32 D_0030A1B0[9] RETAIL(D_0030A1B0);
extern f32 D_0030A1D4[1] RETAIL(D_0030A1D4);
extern f32 D_0030A1D8[13] RETAIL(D_0030A1D8);
extern u8 D_0030A20C[5] RETAIL(D_0030A20C);
extern u8 D_0030A211[7] RETAIL(D_0030A211);
extern s32 D_0030A218[5] RETAIL(D_0030A218);
extern const void* G_BLEND_SHAPE_FLOATS_ALLOCATOR[1] RETAIL(G_BLEND_SHAPE_FLOATS_ALLOCATOR);
extern u32 D_0030A230[16] RETAIL(D_0030A230);
extern char D_0030A270[8] RETAIL(D_0030A270);
extern char D_0030A278[8] RETAIL(D_0030A278);
extern char D_0030A280[8] RETAIL(D_0030A280);
extern char D_0030A288[8] RETAIL(D_0030A288);
extern s32 D_0030A290[2] RETAIL(D_0030A290);
extern s32 D_0030A298[2] RETAIL(D_0030A298);
extern s32 D_0030A2A0[2] RETAIL(D_0030A2A0);
extern s32 D_0030A2A8[2] RETAIL(D_0030A2A8);
extern s32 D_0030A2B0[2] RETAIL(D_0030A2B0);
extern s32 D_0030A2B8[2] RETAIL(D_0030A2B8);
extern s32 D_0030A2C0[2] RETAIL(D_0030A2C0);
extern s32 D_0030A2C8[2] RETAIL(D_0030A2C8);
extern s32 D_0030A2D0[2] RETAIL(D_0030A2D0);
extern s32 D_0030A2D8[2] RETAIL(D_0030A2D8);
extern s32 D_0030A2E0[2] RETAIL(D_0030A2E0);
extern char D_0030A2E8[8] RETAIL(D_0030A2E8);
extern char D_0030A2F0[8] RETAIL(D_0030A2F0);
extern char D_0030A2F8[8] RETAIL(D_0030A2F8);
extern char D_0030A300[8] RETAIL(D_0030A300);
extern char D_0030A308[8] RETAIL(D_0030A308);
extern char D_0030A310[8] RETAIL(D_0030A310);
extern char D_0030A318[8] RETAIL(D_0030A318);
extern char D_0030A320[8] RETAIL(D_0030A320);
extern char D_0030A328[8] RETAIL(D_0030A328);
extern char D_0030A330[8] RETAIL(D_0030A330);
extern u32 D_0030A338[2] RETAIL(D_0030A338);
extern char D_0030A340[8] RETAIL(D_0030A340);
extern char D_0030A348[8] RETAIL(D_0030A348);
extern u32 D_0030A350[1] RETAIL(D_0030A350);
extern u32 D_0030A354[1] RETAIL(D_0030A354);
extern u32 D_0030A358[1] RETAIL(D_0030A358);
extern u32 D_0030A35C[1] RETAIL(D_0030A35C);
extern u32 D_0030A360[1] RETAIL(D_0030A360);
extern u32 D_0030A364[1] RETAIL(D_0030A364);
extern u32 D_0030A368[1] RETAIL(D_0030A368);
extern u32 D_0030A36C[1] RETAIL(D_0030A36C);
extern char D_0030A370[8] RETAIL(D_0030A370);
extern u32 D_0030A378[1] RETAIL(D_0030A378);
extern u32 D_0030A37C[1] RETAIL(D_0030A37C);
extern u32 D_0030A380[4] RETAIL(D_0030A380);
extern char D_0030A390[16] RETAIL(D_0030A390);
extern char D_0030A3A0[8] RETAIL(D_0030A3A0);
extern u32 D_0030A3A8[2] RETAIL(D_0030A3A8);
extern char D_0030A3B0[8] RETAIL(D_0030A3B0);
extern u32 D_0030A3B8[1] RETAIL(D_0030A3B8);
extern u32 D_0030A3BC[1] RETAIL(D_0030A3BC);
extern u8 D_0030A3C0[1] RETAIL(D_0030A3C0);
extern u8 D_0030A3C1[1] RETAIL(D_0030A3C1);
extern u8 D_0030A3C2[1] RETAIL(D_0030A3C2);
extern u8 D_0030A3C3[1] RETAIL(D_0030A3C3);
extern u32 G_PSS_FileLocation[1] RETAIL(G_PSS_FileLocation);
extern u32 G_PSS_FileSize[1] RETAIL(G_PSS_FileSize);
extern u32 D_0030A3CC[1] RETAIL(D_0030A3CC);
extern u32 D_0030A3D0[2] RETAIL(D_0030A3D0);
extern u32 D_0030A3D8[2] RETAIL(D_0030A3D8);
extern u32 D_0030A3E0[2] RETAIL(D_0030A3E0);
extern u32 D_0030A3E8[2] RETAIL(D_0030A3E8);
extern u32 D_0030A3F0[2] RETAIL(D_0030A3F0);
extern u32 D_0030A3F8[10] RETAIL(D_0030A3F8);
extern u8 D_0030A420[3] RETAIL(D_0030A420);
extern u8 D_0030A423[5] RETAIL(D_0030A423);
extern u32 G_GameReadersStorages[2] RETAIL(G_GameReadersStorages);
extern s32 D_0030A430[1] RETAIL(D_0030A430);
extern u32 D_0030A434[1] RETAIL(D_0030A434);
extern u32 D_0030A438[2] RETAIL(D_0030A438);
extern u8 G_archiveExtesion_Header[3] RETAIL(G_archiveExtesion_Header);
extern u8 D_0030A443[5] RETAIL(D_0030A443);
extern u8 G_archiveExtesion_Data[3] RETAIL(G_archiveExtesion_Data);
extern u8 D_0030A44B[5] RETAIL(D_0030A44B);
extern char D_0030A450[8] RETAIL(D_0030A450);
extern s32 D_0030A458[2] RETAIL(D_0030A458);

// 0x309880: nothing uses it
RETAIL_DATA(".sdata", 64) u32 D_00309880[1] RETAIL(D_00309880) = {
    0x0,
};
// 0x309884
RETAIL_DATA(".sdata", 4) const void* D_00309884[1] RETAIL(D_00309884) = {
    Ref_D_002EC400,
};
// 0x309888
RETAIL_DATA(".sdata", 8) u32 G_GameController[1] RETAIL(G_GameController) = {
    0x0,
};
// 0x30988C
RETAIL_DATA(".sdata", 4) u32 G_GameController_0030988C[1] RETAIL(G_GameController_0030988C) = {
    0x0,
};
// 0x309890
RETAIL_DATA(".sdata", 16) u32 G_GameController_00309890[1] RETAIL(G_GameController_00309890) = {
    0x0,
};
// 0x309894: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309894[1] RETAIL(D_00309894) = {
    0x0,
};
// 0x309898: nothing uses it
RETAIL_DATA(".sdata", 8) u8 RB_String[3] RETAIL(RB_String) = {
    0x52, 0x42, 0x00,
};
// 0x30989B: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030989B[5] RETAIL(D_0030989B) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x3098A0: nothing uses it
RETAIL_DATA(".sdata", 32) char BATCH_String[6] RETAIL(BATCH_String) = "BATCH";
// 0x3098A6: nothing uses it
RETAIL_DATA(".sdata", 2) u16 D_003098A6[1] RETAIL(D_003098A6) = {
    0x0,
};
// 0x3098A8
RETAIL_DATA(".sdata", 8) char D_003098A8[8] RETAIL(D_003098A8) = "English";
// 0x3098B0: nothing uses it
RETAIL_DATA(".sdata", 16) char D_003098B0[8] RETAIL(D_003098B0) = "French";
// 0x3098B8: nothing uses it
RETAIL_DATA(".sdata", 8) char D_003098B8[8] RETAIL(D_003098B8) = "German";
// 0x3098C0: nothing uses it
RETAIL_DATA(".sdata", 64) char D_003098C0[8] RETAIL(D_003098C0) = "Spanish";
// 0x3098C8: nothing uses it
RETAIL_DATA(".sdata", 8) char D_003098C8[16] RETAIL(D_003098C8) = "Italian";
// 0x3098D8
RETAIL_DATA(".sdata", 8) char D_003098D8[4] RETAIL(D_003098D8) = "";
// 0x3098DC
RETAIL_DATA(".sdata", 4) s32 D_003098DC[1] RETAIL(D_003098DC) = {
    0,
};
// 0x3098E0
RETAIL_DATA(".sdata", 32) s32 D_003098E0[1] RETAIL(D_003098E0) = {
    0,
};
// 0x3098E4
RETAIL_DATA(".sdata", 4) u32 D_003098E4[1] RETAIL(D_003098E4) = {
    0x0,
};
// 0x3098E8
RETAIL_DATA(".sdata", 8) s32 D_003098E8[2] RETAIL(D_003098E8) = {
    0, 1,
};
// 0x3098F0
RETAIL_DATA(".sdata", 16) char D_003098F0[12] RETAIL(D_003098F0) = "HP ";
// 0x3098FC
RETAIL_DATA(".sdata", 4) u32 G_UnkPlayableCharObjInstCxt[1] RETAIL(G_UnkPlayableCharObjInstCxt) = {
    0x0,
};
// 0x309900
RETAIL_DATA(".sdata", 64) u32 G_UnkCreationHelper[1] RETAIL(G_UnkCreationHelper) = {
    0x0,
};
// 0x309904
RETAIL_DATA(".sdata", 4) u32 G_UnkPlayableCharObjInstCxt2[1] RETAIL(G_UnkPlayableCharObjInstCxt2) = {
    0x0,
};
// 0x309908
RETAIL_DATA(".sdata", 8) u32 G_UnkCreationHelper2[1] RETAIL(G_UnkCreationHelper2) = {
    0x0,
};
// 0x30990C: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030990C[2] RETAIL(D_0030990C) = {
    0x0, 0x0,
};
// 0x309914
RETAIL_DATA(".sdata", 4) u32 G_GameController_00309914[1] RETAIL(G_GameController_00309914) = {
    0x0,
};
// 0x309918: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309918[3] RETAIL(D_00309918) = {
    0x0, 0x0, 0x0,
};
// 0x309924
RETAIL_DATA(".sdata", 4) u32 D_00309924[1] RETAIL(D_00309924) = {
    0x0,
};
// 0x309928
RETAIL_DATA(".sdata", 8) u32 D_00309928[1] RETAIL(D_00309928) = {
    0x0,
};
// 0x30992C
RETAIL_DATA(".sdata", 4) u32 D_0030992C[1] RETAIL(D_0030992C) = {
    0x0,
};
// 0x309930: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_00309930[2] RETAIL(D_00309930) = {
    0x0, 0x0,
};
// 0x309938: nothing uses it
RETAIL_DATA(".sdata", 8) char D_00309938[8] RETAIL(D_00309938) = "light";
// 0x309940: nothing uses it
RETAIL_DATA(".sdata", 64) D_00309940_Fields D_00309940 RETAIL(D_00309940) = {0x6B726164, 0x0, &D_00309938, &D_00309940};
// 0x309950
RETAIL_DATA(".sdata", 16) u32 G_GameController_00309950[1] RETAIL(G_GameController_00309950) = {
    0x0,
};
// 0x309954: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309954[1] RETAIL(D_00309954) = {
    0x0,
};
// 0x309958
RETAIL_DATA(".sdata", 8) char D_00309958[8] RETAIL(D_00309958) = "";
// 0x309960: nothing uses it
RETAIL_DATA(".sdata", 32) u32 D_00309960[2] RETAIL(D_00309960) = {
    0x20, 0x0,
};
// 0x309968: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309968[2] RETAIL(D_00309968) = {
    0x2020, 0x0,
};
// 0x309970: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_00309970[2] RETAIL(D_00309970) = {
    0x303A, 0x0,
};
// 0x309978: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309978[2] RETAIL(D_00309978) = {
    0x3A, 0x0,
};
// 0x309980: nothing uses it
RETAIL_DATA(".sdata", 64) u32 D_00309980[2] RETAIL(D_00309980) = {
    0x25, 0x0,
};
// 0x309988
RETAIL_DATA(".sdata", 8) char D_00309988[8] RETAIL(D_00309988) = "Bank";
// 0x309990
RETAIL_DATA(".sdata", 16) char D_00309990[8] RETAIL(D_00309990) = ".bin";
// 0x309998
RETAIL_DATA(".sdata", 8) char D_00309998[8] RETAIL(D_00309998) = ".psm";
// 0x3099A0
RETAIL_DATA(".sdata", 32) const void* D_003099A0[1] RETAIL(D_003099A0) = {
    &D_00309998,
};
// 0x3099A4: nothing uses it
RETAIL_DATA(".sdata", 4) const void* D_003099A4[1] RETAIL(D_003099A4) = {
    Ref_D_002F4868,
};
// 0x3099A8
RETAIL_DATA(".sdata", 8) u32 G_PrecompShader_0xE_2[1] RETAIL(G_PrecompShader_0xE_2) = {
    0x0,
};
// 0x3099AC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_003099AC[1] RETAIL(D_003099AC) = {
    0x0,
};
// 0x3099B0
RETAIL_DATA(".sdata", 16) char D_003099B0[8] RETAIL(D_003099B0) = "Hub01";
// 0x3099B8
RETAIL_DATA(".sdata", 8) char D_003099B8[8] RETAIL(D_003099B8) = "Level01";
// 0x3099C0
RETAIL_DATA(".sdata", 64) char D_003099C0[8] RETAIL(D_003099C0) = "Level02";
// 0x3099C8
RETAIL_DATA(".sdata", 8) char D_003099C8[8] RETAIL(D_003099C8) = "Level03";
// 0x3099D0
RETAIL_DATA(".sdata", 16) char D_003099D0[8] RETAIL(D_003099D0) = "Hub02";
// 0x3099D8
RETAIL_DATA(".sdata", 8) char D_003099D8[8] RETAIL(D_003099D8) = "Level04";
// 0x3099E0
RETAIL_DATA(".sdata", 32) char D_003099E0[8] RETAIL(D_003099E0) = "Level05";
// 0x3099E8
RETAIL_DATA(".sdata", 8) char D_003099E8[8] RETAIL(D_003099E8) = "Level06";
// 0x3099F0
RETAIL_DATA(".sdata", 16) char D_003099F0[8] RETAIL(D_003099F0) = "Hub03";
// 0x3099F8
RETAIL_DATA(".sdata", 8) char D_003099F8[8] RETAIL(D_003099F8) = "Level07";
// 0x309A00
RETAIL_DATA(".sdata", 64) char D_00309A00[8] RETAIL(D_00309A00) = "Level08";
// 0x309A08
RETAIL_DATA(".sdata", 8) char D_00309A08[8] RETAIL(D_00309A08) = "Level09";
// 0x309A10
RETAIL_DATA(".sdata", 16) char D_00309A10[8] RETAIL(D_00309A10) = "Level10";
// 0x309A18
RETAIL_DATA(".sdata", 8) char D_00309A18[8] RETAIL(D_00309A18) = "Hub04";
// 0x309A20
RETAIL_DATA(".sdata", 32) char D_00309A20[8] RETAIL(D_00309A20) = "Level11";
// 0x309A28
RETAIL_DATA(".sdata", 8) char D_00309A28[8] RETAIL(D_00309A28) = "Level12";
// 0x309A30
RETAIL_DATA(".sdata", 16) char D_00309A30[8] RETAIL(D_00309A30) = "Level13";
// 0x309A38
RETAIL_DATA(".sdata", 8) char D_00309A38[8] RETAIL(D_00309A38) = "Legal";
// 0x309A40: nothing uses it
RETAIL_DATA(".sdata", 64) u32 D_00309A40[1] RETAIL(D_00309A40) = {
    0x2F,
};
// 0x309A44
RETAIL_DATA(".sdata", 4) f32 D_00309A44[1] RETAIL(D_00309A44) = {
    -0.0f,
};
// 0x309A48
RETAIL_DATA(".sdata", 8) char D_00309A48[8] RETAIL(D_00309A48) = "\\Crash";
// 0x309A50: nothing uses it
RETAIL_DATA(".sdata", 16) const void* D_00309A50[1] RETAIL(D_00309A50) = {
    Ref_D_002F4978,
};
// 0x309A54: nothing uses it
RETAIL_DATA(".sdata", 4) const void* D_00309A54[1] RETAIL(D_00309A54) = {
    Ref_D_002F4988,
};
// 0x309A58: nothing uses it
RETAIL_DATA(".sdata", 8) const void* D_00309A58[1] RETAIL(D_00309A58) = {
    Ref_D_002F49A0,
};
// 0x309A5C: nothing uses it
RETAIL_DATA(".sdata", 4) const void* D_00309A5C[3] RETAIL(D_00309A5C) = {
    Ref_D_002F49B0, Ref_D_002F49D0, nullptr,
};
// 0x309A68: nothing uses it
RETAIL_DATA(".sdata", 8) char D_00309A68[8] RETAIL(D_00309A68) = "Code";
// 0x309A70: nothing uses it
RETAIL_DATA(".sdata", 16) const void* D_00309A70[2] RETAIL(D_00309A70) = {
    &D_00309A68, Ref_D_002F49E8,
};
// 0x309A78: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309A78[2] RETAIL(D_00309A78) = {
    0x5C, 0x0,
};
// 0x309A80: nothing uses it
RETAIL_DATA(".sdata", 64) char D_00309A80[8] RETAIL(D_00309A80) = ".txt";
// 0x309A88: nothing uses it
RETAIL_DATA(".sdata", 8) char D_00309A88[8] RETAIL(D_00309A88) = "Crash6\\";
// 0x309A90
RETAIL_DATA(".sdata", 16) u32 D_00309A90[1] RETAIL(D_00309A90) = {
    0x0,
};
// 0x309A94
RETAIL_DATA(".sdata", 4) u32 D_00309A94[5] RETAIL(D_00309A94) = {
    0x41980000, 0x80000000, 0x0, 0x0, 0x0,
};
// 0x309AA8: nothing uses it
RETAIL_DATA(".sdata", 8) u8 D_00309AA8[9] RETAIL(D_00309AA8) = {
    0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x309AB1
RETAIL_DATA(".sdata", 1) u8 D_00309AB1[1] RETAIL(D_00309AB1) = {
    0x00,
};
// 0x309AB2
RETAIL_DATA(".sdata", 2) u16 D_00309AB2[1] RETAIL(D_00309AB2) = {
    0x0,
};
// 0x309AB4
RETAIL_DATA(".sdata", 4) u32 g_GameContext[1] RETAIL(g_GameContext) = {
    0x0,
};
// 0x309AB8
RETAIL_DATA(".sdata", 8) u32 G_GameState_[1] RETAIL(G_GameState_) = {
    0x0,
};
// 0x309ABC
RETAIL_DATA(".sdata", 4) u32 G_GamePadController[1] RETAIL(G_GamePadController) = {
    0x0,
};
// 0x309AC0
RETAIL_DATA(".sdata", 64) u32 G_GameMovieController[1] RETAIL(G_GameMovieController) = {
    0x0,
};
// 0x309AC4
RETAIL_DATA(".sdata", 4) u32 G_UnkStruct_5C0[1] RETAIL(G_UnkStruct_5C0) = {
    0x0,
};
// 0x309AC8
RETAIL_DATA(".sdata", 8) u32 G_GameRendererController[1] RETAIL(G_GameRendererController) = {
    0x0,
};
// 0x309ACC
RETAIL_DATA(".sdata", 4) u32 G_GameClockController[1] RETAIL(G_GameClockController) = {
    0x0,
};
// 0x309AD0
RETAIL_DATA(".sdata", 16) u32 G_GameResourcesObjectPointer[1] RETAIL(G_GameResourcesObjectPointer) = {
    0x0,
};
// 0x309AD4
RETAIL_DATA(".sdata", 4) u32 UnkStruct_0x14_HasSomeDataPTRs[1] RETAIL(UnkStruct_0x14_HasSomeDataPTRs) = {
    0x0,
};
// 0x309AD8
RETAIL_DATA(".sdata", 8) u32 G_ChunkLoadingManager_[1] RETAIL(G_ChunkLoadingManager_) = {
    0x0,
};
// 0x309ADC
RETAIL_DATA(".sdata", 4) u32 G_VideoController[1] RETAIL(G_VideoController) = {
    0x0,
};
// 0x309AE0
RETAIL_DATA(".sdata", 32) u32 G_Renderer_[1] RETAIL(G_Renderer_) = {
    0x0,
};
// 0x309AE4
RETAIL_DATA(".sdata", 4) s32 D_00309AE4[1] RETAIL(D_00309AE4) = {
    512,
};
// 0x309AE8
RETAIL_DATA(".sdata", 8) s32 D_00309AE8[1] RETAIL(D_00309AE8) = {
    512,
};
// 0x309AEC
RETAIL_DATA(".sdata", 4) u32 D_00309AEC[1] RETAIL(D_00309AEC) = {
    0x1,
};
// 0x309AF0
RETAIL_DATA(".sdata", 16) u32 GlobalLanguagesAmount[1] RETAIL(GlobalLanguagesAmount) = {
    0x0,
};
// 0x309AF4
RETAIL_DATA(".sdata", 4) u32 CurrentLanguageIndex[1] RETAIL(CurrentLanguageIndex) = {
    0x0,
};
// 0x309AF8
RETAIL_DATA(".sdata", 8) char G_unkLanguagesStruct[4] RETAIL(G_unkLanguagesStruct) = "";
// 0x309AFC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309AFC[1] RETAIL(D_00309AFC) = {
    0x0,
};
// 0x309B00
RETAIL_DATA(".sdata", 64) u32 D_00309B00[2] RETAIL(D_00309B00) = {
    0x0, 0x0,
};
// 0x309B08
RETAIL_DATA(".sdata", 8) char D_00309B08[8] RETAIL(D_00309B08) = "";
// 0x309B10
RETAIL_DATA(".sdata", 16) char D_00309B10[4] RETAIL(D_00309B10) = "";
// 0x309B14
RETAIL_DATA(".sdata", 4) char D_00309B14[4] RETAIL(D_00309B14) = "";
// 0x309B18
RETAIL_DATA(".sdata", 8) u32 D_00309B18[1] RETAIL(D_00309B18) = {
    0x0,
};
// 0x309B1C
RETAIL_DATA(".sdata", 4) u32 D_00309B1C[1] RETAIL(D_00309B1C) = {
    0x0,
};
// 0x309B20
RETAIL_DATA(".sdata", 32) u32 D_00309B20[10] RETAIL(D_00309B20) = {
    0x0, 0x1000E000, 0x1000E010, 0x1000E020, 0x1000E030, 0x1000E040, 0x1000E050, 0x1000E060, 0x1000F520, 0x1000F590,
};
// 0x309B48
RETAIL_DATA(".sdata", 8) u32 D_00309B48[1] RETAIL(D_00309B48) = {
    0x0,
};
// 0x309B4C
RETAIL_DATA(".sdata", 4) u32 D_00309B4C[6] RETAIL(D_00309B4C) = {
    0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
};
// 0x309B64
RETAIL_DATA(".sdata", 4) f32 G_GlobalClockSpeedScale[1] RETAIL(G_GlobalClockSpeedScale) = {
    0.0f,
};
// 0x309B68
RETAIL_DATA(".sdata", 8) u32 D_00309B68[2] RETAIL(D_00309B68) = {
    0x0, 0x0,
};
// 0x309B70: nothing uses it
RETAIL_DATA(".sdata", 16) u32 G_CPU_T0_OVERFLOW_COUNTER[2] RETAIL(G_CPU_T0_OVERFLOW_COUNTER) = {
    0x0, 0x0,
};
// 0x309B78: nothing uses it
RETAIL_DATA(".sdata", 8) u8 G_Timer0_Set[1] RETAIL(G_Timer0_Set) = {
    0x00,
};
// 0x309B79: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_00309B79[3] RETAIL(D_00309B79) = {
    0x01, 0x00, 0x00,
};
// 0x309B7C
RETAIL_DATA(".sdata", 4) f32 G_TICKS_TO_TIME[1] RETAIL(G_TICKS_TO_TIME) = {
    0.0f,
};
// 0x309B80
RETAIL_DATA(".sdata", 64) f32 G_TIME_TO_TICKS[1] RETAIL(G_TIME_TO_TICKS) = {
    0.0f,
};
// 0x309B84: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309B84[1] RETAIL(D_00309B84) = {
    0x0,
};
// 0x309B88
RETAIL_DATA(".sdata", 8) u32 D_00309B88[1] RETAIL(D_00309B88) = {
    0x0,
};
// 0x309B8C
RETAIL_DATA(".sdata", 4) u32 D_00309B8C[1] RETAIL(D_00309B8C) = {
    0x0,
};
// 0x309B90
RETAIL_DATA(".sdata", 16) u32 g_GameNodeList[1] RETAIL(g_GameNodeList) = {
    0x0,
};
// 0x309B94
RETAIL_DATA(".sdata", 4) u32 D_00309B94[1] RETAIL(D_00309B94) = {
    0x0,
};
// 0x309B98
RETAIL_DATA(".sdata", 8) u32 D_00309B98[1] RETAIL(D_00309B98) = {
    0xFFFFFFFF,
};
// 0x309B9C
RETAIL_DATA(".sdata", 4) u32 G_InstContext2[1] RETAIL(G_InstContext2) = {
    0x0,
};
// 0x309BA0
RETAIL_DATA(".sdata", 32) u32 D_00309BA0[1] RETAIL(D_00309BA0) = {
    0x0,
};
// 0x309BA4
RETAIL_DATA(".sdata", 4) u32 G_SizeOfByteArrayAt_0x3D3F28_0x18[1] RETAIL(G_SizeOfByteArrayAt_0x3D3F28_0x18) = {
    0x0,
};
// 0x309BA8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309BA8[3] RETAIL(D_00309BA8) = {
    0x0, 0x0, 0x2020,
};
// 0x309BB4
RETAIL_DATA(".sdata", 4) u32 D_00309BB4[1] RETAIL(D_00309BB4) = {
    0x0,
};
// 0x309BB8
RETAIL_DATA(".sdata", 8) u32 D_00309BB8[1] RETAIL(D_00309BB8) = {
    0x0,
};
// 0x309BBC
RETAIL_DATA(".sdata", 4) u32 G_ParticleBinReader[1] RETAIL(G_ParticleBinReader) = {
    0x0,
};
// 0x309BC0
RETAIL_DATA(".sdata", 64) u32 D_00309BC0[1] RETAIL(D_00309BC0) = {
    0x1,
};
// 0x309BC4
RETAIL_DATA(".sdata", 4) u32 G_DMA_ByteStream_Beg_[1] RETAIL(G_DMA_ByteStream_Beg_) = {
    0x0,
};
// 0x309BC8
RETAIL_DATA(".sdata", 8) u32 G_DMA_ByteStream_CurPosition_[1] RETAIL(G_DMA_ByteStream_CurPosition_) = {
    0x0,
};
// 0x309BCC
RETAIL_DATA(".sdata", 4) u32 D_00309BCC[1] RETAIL(D_00309BCC) = {
    0x1,
};
// 0x309BD0
RETAIL_DATA(".sdata", 16) u32 D_00309BD0[1] RETAIL(D_00309BD0) = {
    0x2,
};
// 0x309BD4
RETAIL_DATA(".sdata", 4) u32 D_00309BD4[1] RETAIL(D_00309BD4) = {
    0x4,
};
// 0x309BD8: nothing uses it
RETAIL_DATA(".sdata", 8) u8 G_IsPCRTC_Ready[1] RETAIL(G_IsPCRTC_Ready) = {
    0x01,
};
// 0x309BD9: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_00309BD9[3] RETAIL(D_00309BD9) = {
    0x00, 0x00, 0x00,
};
// 0x309BDC
RETAIL_DATA(".sdata", 4) u32 G_FontRendererRel[1] RETAIL(G_FontRendererRel) = {
    0x0,
};
// 0x309BE0
RETAIL_DATA(".sdata", 32) u32 G_RendRel[1] RETAIL(G_RendRel) = {
    0x0,
};
// 0x309BE4
RETAIL_DATA(".sdata", 4) u32 G_UnkChunkData_[1] RETAIL(G_UnkChunkData_) = {
    0x0,
};
// 0x309BE8
RETAIL_DATA(".sdata", 8) u32 D_00309BE8[1] RETAIL(D_00309BE8) = {
    0x0,
};
// 0x309BEC
RETAIL_DATA(".sdata", 4) u32 D_00309BEC[1] RETAIL(D_00309BEC) = {
    0x0,
};
// 0x309BF0
RETAIL_DATA(".sdata", 16) u32 D_00309BF0[1] RETAIL(D_00309BF0) = {
    0x0,
};
// 0x309BF4
RETAIL_DATA(".sdata", 4) u32 D_00309BF4[1] RETAIL(D_00309BF4) = {
    0x0,
};
// 0x309BF8
RETAIL_DATA(".sdata", 8) u32 G_0x320_ArrayOf_0x14_SizeStruct[1] RETAIL(G_0x320_ArrayOf_0x14_SizeStruct) = {
    0x0,
};
// 0x309BFC
RETAIL_DATA(".sdata", 4) u32 D_00309BFC[11] RETAIL(D_00309BFC) = {
    0x0, 0x7269642E, 0x0, 0x6F65672E, 0x0, 0x5C, 0x0, 0x2E796B53, 0x746164, 0x0, 0x0,
};
// 0x309C28
RETAIL_DATA(".sdata", 8) char D_00309C28[9] RETAIL(D_00309C28) = ".psf";
// 0x309C31
RETAIL_DATA(".sdata", 1) u8 D_00309C31[1] RETAIL(D_00309C31) = {
    0x00,
};
// 0x309C32
RETAIL_DATA(".sdata", 2) u8 D_00309C32[2] RETAIL(D_00309C32) = {
    0x00, 0x00,
};
// 0x309C34
RETAIL_DATA(".sdata", 4) f32 D_00309C34[1] RETAIL(D_00309C34) = {
    1.0f,
};
// 0x309C38
RETAIL_DATA(".sdata", 8) u32 D_00309C38[1] RETAIL(D_00309C38) = {
    0x0,
};
// 0x309C3C
RETAIL_DATA(".sdata", 4) u32 D_00309C3C[3] RETAIL(D_00309C3C) = {
    0x0, 0x3F800000, 0x3FC00000,
};
// 0x309C48
RETAIL_DATA(".sdata", 8) u32 D_00309C48[2] RETAIL(D_00309C48) = {
    0x0, 0x0,
};
// 0x309C50
RETAIL_DATA(".sdata", 16) u8 D_00309C50[1] RETAIL(D_00309C50) = {
    0x00,
};
// 0x309C51
RETAIL_DATA(".sdata", 1) u8 D_00309C51[1] RETAIL(D_00309C51) = {
    0x00,
};
// 0x309C52
RETAIL_DATA(".sdata", 2) u8 D_00309C52[2] RETAIL(D_00309C52) = {
    0x00, 0x00,
};
// 0x309C54
RETAIL_DATA(".sdata", 4) s32 G_ParticlesAmountLoaded_[1] RETAIL(G_ParticlesAmountLoaded_) = {
    1,
};
// 0x309C58
RETAIL_DATA(".sdata", 8) s32 D_00309C58[1] RETAIL(D_00309C58) = {
    1,
};
// 0x309C5C
RETAIL_DATA(".sdata", 4) f32 ParticleTime[1] RETAIL(ParticleTime) = {
    0.0f,
};
// 0x309C60
RETAIL_DATA(".sdata", 32) s32 ParticleFrameCounter[1] RETAIL(ParticleFrameCounter) = {
    0,
};
// 0x309C64
RETAIL_DATA(".sdata", 4) f32 ParticleFrameDelta[1] RETAIL(ParticleFrameDelta) = {
    Rounded(0.016666668),
};
// 0x309C68
RETAIL_DATA(".sdata", 8) s32 D_00309C68[1] RETAIL(D_00309C68) = {
    256,
};
// 0x309C6C
RETAIL_DATA(".sdata", 4) s32 D_00309C6C[1] RETAIL(D_00309C6C) = {
    32,
};
// 0x309C70
RETAIL_DATA(".sdata", 16) s32 D_00309C70[4] RETAIL(D_00309C70) = {
    0, 0, 0, 0,
};
// 0x309C80
RETAIL_DATA(".sdata", 64) u32 ParticleRandomSeed[2] RETAIL(ParticleRandomSeed) = {
    0x5C0999, 0x0,
};
// 0x309C88
RETAIL_DATA(".sdata", 8) s32 D_00309C88[2] RETAIL(D_00309C88) = {
    4, 0,
};
// 0x309C90
RETAIL_DATA(".sdata", 16) s32 D_00309C90[1] RETAIL(D_00309C90) = {
    7,
};
// 0x309C94
RETAIL_DATA(".sdata", 4) s32 D_00309C94[1] RETAIL(D_00309C94) = {
    1,
};
// 0x309C98
RETAIL_DATA(".sdata", 8) s32 G_ParticleInstancesAmount_[1] RETAIL(G_ParticleInstancesAmount_) = {
    0,
};
// 0x309C9C
RETAIL_DATA(".sdata", 4) s32 D_00309C9C[3] RETAIL(D_00309C9C) = {
    0, 0, 0,
};
// 0x309CA8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_00309CA8[2] RETAIL(D_00309CA8) = {
    0x5C, 0x0,
};
// 0x309CB0
RETAIL_DATA(".sdata", 16) u32 D_00309CB0[1] RETAIL(D_00309CB0) = {
    0x1,
};
// 0x309CB4
RETAIL_DATA(".sdata", 4) u32 D_00309CB4[1] RETAIL(D_00309CB4) = {
    0x2,
};
// 0x309CB8
RETAIL_DATA(".sdata", 8) u32 D_00309CB8[1] RETAIL(D_00309CB8) = {
    0x3,
};
// 0x309CBC
RETAIL_DATA(".sdata", 4) u32 D_00309CBC[1] RETAIL(D_00309CBC) = {
    0x4,
};
// 0x309CC0
RETAIL_DATA(".sdata", 64) u32 D_00309CC0[1] RETAIL(D_00309CC0) = {
    0x5,
};
// 0x309CC4
RETAIL_DATA(".sdata", 4) u32 D_00309CC4[1] RETAIL(D_00309CC4) = {
    0x6,
};
// 0x309CC8
RETAIL_DATA(".sdata", 8) u32 D_00309CC8[1] RETAIL(D_00309CC8) = {
    0x7,
};
// 0x309CCC
RETAIL_DATA(".sdata", 4) u32 D_00309CCC[4] RETAIL(D_00309CCC) = {
    0x8, 0xA, 0x26, 0x27,
};
// 0x309CDC
RETAIL_DATA(".sdata", 4) u32 D_00309CDC[2] RETAIL(D_00309CDC) = {
    0x4F, 0xB3,
};
// 0x309CE4
RETAIL_DATA(".sdata", 4) u32 D_00309CE4[1] RETAIL(D_00309CE4) = {
    0x117,
};
// 0x309CE8
RETAIL_DATA(".sdata", 8) u32 D_00309CE8[1] RETAIL(D_00309CE8) = {
    0x17A,
};
// 0x309CEC
RETAIL_DATA(".sdata", 4) u32 D_00309CEC[1] RETAIL(D_00309CEC) = {
    0x1DD,
};
// 0x309CF0
RETAIL_DATA(".sdata", 16) u32 D_00309CF0[1] RETAIL(D_00309CF0) = {
    0x278,
};
// 0x309CF4
RETAIL_DATA(".sdata", 4) u32 D_00309CF4[1] RETAIL(D_00309CF4) = {
    0x313,
};
// 0x309CF8
RETAIL_DATA(".sdata", 8) u32 D_00309CF8[1] RETAIL(D_00309CF8) = {
    0x389,
};
// 0x309CFC
RETAIL_DATA(".sdata", 4) u32 G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN[1] RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_BEGIN) = {
    0x12B,
};
// 0x309D00
RETAIL_DATA(".sdata", 64) u32 G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END[1] RETAIL(G_BLEND_SKIN_ANIM_DATA_VU_ADDR_END) = {
    0x152,
};
// 0x309D04
RETAIL_DATA(".sdata", 4) u32 G_CONST_D[1] RETAIL(G_CONST_D) = {
    0xD,
};
// 0x309D08
RETAIL_DATA(".sdata", 8) u32 D_00309D08[5] RETAIL(D_00309D08) = {
    0xFFFFFFFF, 0x0, 0x4, 0x7, 0xA,
};
// 0x309D1C
RETAIL_DATA(".sdata", 4) u32 D_00309D1C[2] RETAIL(D_00309D1C) = {
    0xB, 0xF,
};
// 0x309D24
RETAIL_DATA(".sdata", 4) u32 D_00309D24[2] RETAIL(D_00309D24) = {
    0x10, 0x15,
};
// 0x309D2C
RETAIL_DATA(".sdata", 4) u32 D_00309D2C[4] RETAIL(D_00309D2C) = {
    0x4E, 0x0, 0x4, 0x8,
};
// 0x309D3C
RETAIL_DATA(".sdata", 4) u32 D_00309D3C[5] RETAIL(D_00309D3C) = {
    0x4E, 0x0, 0x0, 0x0, 0x0,
};
// 0x309D50: nothing uses it
RETAIL_DATA(".sdata", 16) char D_00309D50[8] RETAIL(D_00309D50) = "Models\\";
// 0x309D58: nothing uses it
RETAIL_DATA(".sdata", 8) char D_00309D58[8] RETAIL(D_00309D58) = "Skins\\";
// 0x309D60: nothing uses it
RETAIL_DATA(".sdata", 32) char D_00309D60[8] RETAIL(D_00309D60) = "LODs\\";
// 0x309D68
RETAIL_DATA(".sdata", 8) u32 D_00309D68[1] RETAIL(D_00309D68) = {
    0x0,
};
// 0x309D6C
RETAIL_DATA(".sdata", 4) u32 D_00309D6C[1] RETAIL(D_00309D6C) = {
    0x0,
};
// 0x309D70
RETAIL_DATA(".sdata", 16) u32 D_00309D70[1] RETAIL(D_00309D70) = {
    0x0,
};
// 0x309D74
RETAIL_DATA(".sdata", 4) u32 D_00309D74[1] RETAIL(D_00309D74) = {
    0x0,
};
// 0x309D78
RETAIL_DATA(".sdata", 8) u32 D_00309D78[3] RETAIL(D_00309D78) = {
    0x0, 0x0, 0x0,
};
// 0x309D84
RETAIL_DATA(".sdata", 4) s32 D_00309D84[1] RETAIL(D_00309D84) = {
    0,
};
// 0x309D88
RETAIL_DATA(".sdata", 8) u32 D_00309D88[2] RETAIL(D_00309D88) = {
    0x0, 0x0,
};
// 0x309D90
RETAIL_DATA(".sdata", 16) char D_00309D90[20] RETAIL(D_00309D90) = ".geom";
// 0x309DA4
RETAIL_DATA(".sdata", 4) u32 D_00309DA4[1] RETAIL(D_00309DA4) = {
    0x0,
};
// 0x309DA8
RETAIL_DATA(".sdata", 8) u32 D_00309DA8[1] RETAIL(D_00309DA8) = {
    0x1B,
};
// 0x309DAC
RETAIL_DATA(".sdata", 4) u32 D_00309DAC[1] RETAIL(D_00309DAC) = {
    0x14,
};
// 0x309DB0
RETAIL_DATA(".sdata", 16) u32 D_00309DB0[1] RETAIL(D_00309DB0) = {
    0x2A,
};
// 0x309DB4
RETAIL_DATA(".sdata", 4) u32 D_00309DB4[1] RETAIL(D_00309DB4) = {
    0x23,
};
// 0x309DB8
RETAIL_DATA(".sdata", 8) u32 D_00309DB8[1] RETAIL(D_00309DB8) = {
    0x0,
};
// 0x309DBC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309DBC[1] RETAIL(D_00309DBC) = {
    0x0,
};
// 0x309DC0
RETAIL_DATA(".sdata", 64) u32 D_00309DC0[1] RETAIL(D_00309DC0) = {
    0x15,
};
// 0x309DC4
RETAIL_DATA(".sdata", 4) u32 D_00309DC4[1] RETAIL(D_00309DC4) = {
    0x26,
};
// 0x309DC8
RETAIL_DATA(".sdata", 8) u32 D_00309DC8[1] RETAIL(D_00309DC8) = {
    0x10,
};
// 0x309DCC
RETAIL_DATA(".sdata", 4) u32 D_00309DCC[3] RETAIL(D_00309DCC) = {
    0x1C, 0x0, 0x0,
};
// 0x309DD8
RETAIL_DATA(".sdata", 8) u32 D_00309DD8[1] RETAIL(D_00309DD8) = {
    0x0,
};
// 0x309DDC
RETAIL_DATA(".sdata", 4) u32 D_00309DDC[1] RETAIL(D_00309DDC) = {
    0x1A,
};
// 0x309DE0
RETAIL_DATA(".sdata", 32) u32 D_00309DE0[1] RETAIL(D_00309DE0) = {
    0x21,
};
// 0x309DE4
RETAIL_DATA(".sdata", 4) u32 D_00309DE4[1] RETAIL(D_00309DE4) = {
    0x1E,
};
// 0x309DE8
RETAIL_DATA(".sdata", 8) u32 D_00309DE8[1] RETAIL(D_00309DE8) = {
    0x20,
};
// 0x309DEC
RETAIL_DATA(".sdata", 4) u32 D_00309DEC[1] RETAIL(D_00309DEC) = {
    0x1D,
};
// 0x309DF0
RETAIL_DATA(".sdata", 16) u32 D_00309DF0[1] RETAIL(D_00309DF0) = {
    0x25,
};
// 0x309DF4
RETAIL_DATA(".sdata", 4) u32 D_00309DF4[1] RETAIL(D_00309DF4) = {
    0x11,
};
// 0x309DF8
RETAIL_DATA(".sdata", 8) u32 D_00309DF8[1] RETAIL(D_00309DF8) = {
    0x16,
};
// 0x309DFC
RETAIL_DATA(".sdata", 4) u32 D_00309DFC[1] RETAIL(D_00309DFC) = {
    0x11,
};
// 0x309E00
RETAIL_DATA(".sdata", 64) u32 D_00309E00[1] RETAIL(D_00309E00) = {
    0x24,
};
// 0x309E04
RETAIL_DATA(".sdata", 4) u32 D_00309E04[1] RETAIL(D_00309E04) = {
    0x18,
};
// 0x309E08
RETAIL_DATA(".sdata", 8) u32 D_00309E08[1] RETAIL(D_00309E08) = {
    0x1F,
};
// 0x309E0C
RETAIL_DATA(".sdata", 4) u32 D_00309E0C[1] RETAIL(D_00309E0C) = {
    0x13,
};
// 0x309E10
RETAIL_DATA(".sdata", 16) u32 G_MicroCode_7_Index[1] RETAIL(G_MicroCode_7_Index) = {
    0x5,
};
// 0x309E14
RETAIL_DATA(".sdata", 4) u32 G_MicroCode_8_Index[1] RETAIL(G_MicroCode_8_Index) = {
    0x6,
};
// 0x309E18
RETAIL_DATA(".sdata", 8) u32 G_MicroCode_9_Index[1] RETAIL(G_MicroCode_9_Index) = {
    0x8,
};
// 0x309E1C
RETAIL_DATA(".sdata", 4) u32 D_00309E1C[1] RETAIL(D_00309E1C) = {
    0x7,
};
// 0x309E20
RETAIL_DATA(".sdata", 32) u32 G_MicroCode_6_Index[1] RETAIL(G_MicroCode_6_Index) = {
    0x3,
};
// 0x309E24
RETAIL_DATA(".sdata", 4) u32 D_00309E24[1] RETAIL(D_00309E24) = {
    0x17,
};
// 0x309E28
RETAIL_DATA(".sdata", 8) u32 D_00309E28[1] RETAIL(D_00309E28) = {
    0x27,
};
// 0x309E2C
RETAIL_DATA(".sdata", 4) u32 D_00309E2C[1] RETAIL(D_00309E2C) = {
    0x19,
};
// 0x309E30
RETAIL_DATA(".sdata", 16) u32 D_00309E30[1] RETAIL(D_00309E30) = {
    0xF,
};
// 0x309E34
RETAIL_DATA(".sdata", 4) u32 D_00309E34[9] RETAIL(D_00309E34) = {
    0x12, 0x0, 0x0, 0x6E756F73, 0x5C64, 0x5C, 0x0, 0x6E756F53, 0x5C64,
};
// 0x309E58
RETAIL_DATA(".sdata", 8) u32 G_UnkStructOfSize_0xe0[1] RETAIL(G_UnkStructOfSize_0xe0) = {
    0x0,
};
// 0x309E5C
RETAIL_DATA(".sdata", 4) u32 D_00309E5C[1] RETAIL(D_00309E5C) = {
    0x0,
};
// 0x309E60
RETAIL_DATA(".sdata", 32) u32 D_00309E60[1] RETAIL(D_00309E60) = {
    0x0,
};
// 0x309E64
RETAIL_DATA(".sdata", 4) f32 D_00309E64[1] RETAIL(D_00309E64) = {
    0.0f,
};
// 0x309E68
RETAIL_DATA(".sdata", 8) u32 D_00309E68[1] RETAIL(D_00309E68) = {
    0x1,
};
// 0x309E6C
RETAIL_DATA(".sdata", 4) s32 D_00309E6C[1] RETAIL(D_00309E6C) = {
    0,
};
// 0x309E70
RETAIL_DATA(".sdata", 16) f32 D_00309E70[1] RETAIL(D_00309E70) = {
    0.0f,
};
// 0x309E74
RETAIL_DATA(".sdata", 4) f32 D_00309E74[1] RETAIL(D_00309E74) = {
    1.0f,
};
// 0x309E78
RETAIL_DATA(".sdata", 8) u32 D_00309E78[1] RETAIL(D_00309E78) = {
    0x5010,
};
// 0x309E7C
RETAIL_DATA(".sdata", 4) u32 D_00309E7C[1] RETAIL(D_00309E7C) = {
    0x0,
};
// 0x309E80
RETAIL_DATA(".sdata", 64) u32 D_00309E80[1] RETAIL(D_00309E80) = {
    0x0,
};
// 0x309E84
RETAIL_DATA(".sdata", 4) u32 D_00309E84[1] RETAIL(D_00309E84) = {
    0x0,
};
// 0x309E88
RETAIL_DATA(".sdata", 8) u32 D_00309E88[2] RETAIL(D_00309E88) = {
    0x0, 0x0,
};
// 0x309E90
RETAIL_DATA(".sdata", 16) u32 D_00309E90[1] RETAIL(D_00309E90) = {
    0x0,
};
// 0x309E94
RETAIL_DATA(".sdata", 4) u32 D_00309E94[1] RETAIL(D_00309E94) = {
    0x0,
};
// 0x309E98
RETAIL_DATA(".sdata", 8) u32 D_00309E98[2] RETAIL(D_00309E98) = {
    0x0, 0x0,
};
// 0x309EA0
RETAIL_DATA(".sdata", 32) u32 D_00309EA0[6] RETAIL(D_00309EA0) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x309EB8
RETAIL_DATA(".sdata", 8) u32 D_00309EB8[1] RETAIL(D_00309EB8) = {
    0x0,
};
// 0x309EBC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_00309EBC[1] RETAIL(D_00309EBC) = {
    0x0,
};
// 0x309EC0
RETAIL_DATA(".sdata", 64) u32 D_00309EC0[2] RETAIL(D_00309EC0) = {
    0x28, 0x14,
};
// 0x309EC8
RETAIL_DATA(".sdata", 8) u32 D_00309EC8[1] RETAIL(D_00309EC8) = {
    0x0,
};
// 0x309ECC
RETAIL_DATA(".sdata", 4) u32 D_00309ECC[1] RETAIL(D_00309ECC) = {
    0x0,
};
// 0x309ED0
RETAIL_DATA(".sdata", 16) u32 D_00309ED0[1] RETAIL(D_00309ED0) = {
    0x5010,
};
// 0x309ED4
RETAIL_DATA(".sdata", 4) u32 D_00309ED4[1] RETAIL(D_00309ED4) = {
    0x0,
};
// 0x309ED8
RETAIL_DATA(".sdata", 8) u32 D_00309ED8[1] RETAIL(D_00309ED8) = {
    0x0,
};
// 0x309EDC
RETAIL_DATA(".sdata", 4) u32 D_00309EDC[1] RETAIL(D_00309EDC) = {
    0x0,
};
// 0x309EE0
RETAIL_DATA(".sdata", 32) u32 D_00309EE0[1] RETAIL(D_00309EE0) = {
    0x0,
};
// 0x309EE4
RETAIL_DATA(".sdata", 4) u32 D_00309EE4[1] RETAIL(D_00309EE4) = {
    0xFFFFFFFF,
};
// 0x309EE8
RETAIL_DATA(".sdata", 8) u32 D_00309EE8[1] RETAIL(D_00309EE8) = {
    0x0,
};
// 0x309EEC
RETAIL_DATA(".sdata", 4) u32 D_00309EEC[1] RETAIL(D_00309EEC) = {
    0x0,
};
// 0x309EF0
RETAIL_DATA(".sdata", 16) u32 D_00309EF0[1] RETAIL(D_00309EF0) = {
    0x0,
};
// 0x309EF4
RETAIL_DATA(".sdata", 4) u32 D_00309EF4[1] RETAIL(D_00309EF4) = {
    0x0,
};
// 0x309EF8
RETAIL_DATA(".sdata", 8) u32 D_00309EF8[2] RETAIL(D_00309EF8) = {
    0x0, 0x0,
};
// 0x309F00
RETAIL_DATA(".sdata", 64) u32 D_00309F00[1] RETAIL(D_00309F00) = {
    0x1FFFFF,
};
// 0x309F04
RETAIL_DATA(".sdata", 4) u32 D_00309F04[1] RETAIL(D_00309F04) = {
    0x1DFFFF,
};
// 0x309F08
RETAIL_DATA(".sdata", 8) u32 D_00309F08[2] RETAIL(D_00309F08) = {
    0x0, 0x0,
};
// 0x309F10
RETAIL_DATA(".sdata", 16) u32 D_00309F10[2] RETAIL(D_00309F10) = {
    0x0, 0x0,
};
// 0x309F18
RETAIL_DATA(".sdata", 8) u32 D_00309F18[2] RETAIL(D_00309F18) = {
    0x0, 0x0,
};
// 0x309F20
RETAIL_DATA(".sdata", 32) u8 D_00309F20[1] RETAIL(D_00309F20) = {
    0x00,
};
// 0x309F21
RETAIL_DATA(".sdata", 1) u8 D_00309F21[1] RETAIL(D_00309F21) = {
    0x00,
};
// 0x309F22
RETAIL_DATA(".sdata", 2) u8 D_00309F22[2] RETAIL(D_00309F22) = {
    0x00, 0x00,
};
// 0x309F24
RETAIL_DATA(".sdata", 4) u32 D_00309F24[1] RETAIL(D_00309F24) = {
    0x0,
};
// 0x309F28
RETAIL_DATA(".sdata", 8) u32 D_00309F28[1] RETAIL(D_00309F28) = {
    0x0,
};
// 0x309F2C
RETAIL_DATA(".sdata", 4) u32 D_00309F2C[1] RETAIL(D_00309F2C) = {
    0x0,
};
// 0x309F30
RETAIL_DATA(".sdata", 16) u32 D_00309F30[1] RETAIL(D_00309F30) = {
    0xFF,
};
// 0x309F34
RETAIL_DATA(".sdata", 4) u32 D_00309F34[1] RETAIL(D_00309F34) = {
    0xFFFFFFFF,
};
// 0x309F38
RETAIL_DATA(".sdata", 8) u32 D_00309F38[1] RETAIL(D_00309F38) = {
    0xFFFFFFFF,
};
// 0x309F3C
RETAIL_DATA(".sdata", 4) u32 D_00309F3C[1] RETAIL(D_00309F3C) = {
    0x0,
};
// 0x309F40
RETAIL_DATA(".sdata", 64) u32 D_00309F40[1] RETAIL(D_00309F40) = {
    0x0,
};
// 0x309F44
RETAIL_DATA(".sdata", 4) u32 D_00309F44[1] RETAIL(D_00309F44) = {
    0x0,
};
// 0x309F48
RETAIL_DATA(".sdata", 8) u32 D_00309F48[1] RETAIL(D_00309F48) = {
    0x0,
};
// 0x309F4C
RETAIL_DATA(".sdata", 4) u32 D_00309F4C[1] RETAIL(D_00309F4C) = {
    0x0,
};
// 0x309F50
RETAIL_DATA(".sdata", 16) u32 D_00309F50[1] RETAIL(D_00309F50) = {
    0x0,
};
// 0x309F54
RETAIL_DATA(".sdata", 4) u32 D_00309F54[1] RETAIL(D_00309F54) = {
    0x0,
};
// 0x309F58
RETAIL_DATA(".sdata", 8) u32 D_00309F58[1] RETAIL(D_00309F58) = {
    0x0,
};
// 0x309F5C
RETAIL_DATA(".sdata", 4) u32 D_00309F5C[1] RETAIL(D_00309F5C) = {
    0x0,
};
// 0x309F60
RETAIL_DATA(".sdata", 32) u32 D_00309F60[1] RETAIL(D_00309F60) = {
    0x0,
};
// 0x309F64
RETAIL_DATA(".sdata", 4) u32 D_00309F64[1] RETAIL(D_00309F64) = {
    0x2,
};
// 0x309F68
RETAIL_DATA(".sdata", 8) u32 D_00309F68[2] RETAIL(D_00309F68) = {
    0x0, 0x9999,
};
// 0x309F70
RETAIL_DATA(".sdata", 16) u32 D_00309F70[1] RETAIL(D_00309F70) = {
    0x0,
};
// 0x309F74
RETAIL_DATA(".sdata", 4) u32 D_00309F74[1] RETAIL(D_00309F74) = {
    0x1234,
};
// 0x309F78
RETAIL_DATA(".sdata", 8) u8 D_00309F78[1] RETAIL(D_00309F78) = {
    0x00,
};
// 0x309F79
RETAIL_DATA(".sdata", 1) u8 D_00309F79[1] RETAIL(D_00309F79) = {
    0x00,
};
// 0x309F7A
RETAIL_DATA(".sdata", 2) u8 D_00309F7A[6] RETAIL(D_00309F7A) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x309F80
RETAIL_DATA(".sdata", 64) u32 D_00309F80[2] RETAIL(D_00309F80) = {
    0x10001000, 0x10001000,
};
// 0x309F88
RETAIL_DATA(".sdata", 8) u32 D_00309F88[2] RETAIL(D_00309F88) = {
    0x10001000, 0x10001000,
};
// 0x309F90
RETAIL_DATA(".sdata", 16) u32 UnkGlobalCounter[1] RETAIL(UnkGlobalCounter) = {
    0x0,
};
// 0x309F94
RETAIL_DATA(".sdata", 4) u32 D_00309F94[1] RETAIL(D_00309F94) = {
    0xFFFFFFFF,
};
// 0x309F98
RETAIL_DATA(".sdata", 8) u32 D_00309F98[1] RETAIL(D_00309F98) = {
    0x0,
};
// 0x309F9C
RETAIL_DATA(".sdata", 4) u32 D_00309F9C[1] RETAIL(D_00309F9C) = {
    0x0,
};
// 0x309FA0
RETAIL_DATA(".sdata", 32) u32 D_00309FA0[1] RETAIL(D_00309FA0) = {
    0x0,
};
// 0x309FA4
RETAIL_DATA(".sdata", 4) u32 D_00309FA4[1] RETAIL(D_00309FA4) = {
    0x0,
};
// 0x309FA8
RETAIL_DATA(".sdata", 8) u32 D_00309FA8[1] RETAIL(D_00309FA8) = {
    0x0,
};
// 0x309FAC
RETAIL_DATA(".sdata", 4) u32 D_00309FAC[6] RETAIL(D_00309FAC) = {
    0x0, 0xFFFFFF, 0xFFFFFF, 0x0, 0x0, 0x0,
};
// 0x309FC4
RETAIL_DATA(".sdata", 4) u32 D_00309FC4[1] RETAIL(D_00309FC4) = {
    0xFFFFFFFF,
};
// 0x309FC8
RETAIL_DATA(".sdata", 8) u8 D_00309FC8[1] RETAIL(D_00309FC8) = {
    0x00,
};
// 0x309FC9
RETAIL_DATA(".sdata", 1) u8 D_00309FC9[3] RETAIL(D_00309FC9) = {
    0x00, 0x00, 0x00,
};
// 0x309FCC
RETAIL_DATA(".sdata", 4) u32 D_00309FCC[2] RETAIL(D_00309FCC) = {
    0x0, 0x0,
};
// 0x309FD4
RETAIL_DATA(".sdata", 4) u32 D_00309FD4[1] RETAIL(D_00309FD4) = {
    0x0,
};
// 0x309FD8
RETAIL_DATA(".sdata", 8) u32 D_00309FD8[2] RETAIL(D_00309FD8) = {
    0x0, 0x0,
};
// 0x309FE0
RETAIL_DATA(".sdata", 32) u32 D_00309FE0[1] RETAIL(D_00309FE0) = {
    0x0,
};
// 0x309FE4
RETAIL_DATA(".sdata", 4) u32 D_00309FE4[1] RETAIL(D_00309FE4) = {
    0x0,
};
// 0x309FE8
RETAIL_DATA(".sdata", 8) u8 D_00309FE8[10] RETAIL(D_00309FE8) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x309FF2
RETAIL_DATA(".sdata", 2) u8 D_00309FF2[1] RETAIL(D_00309FF2) = {
    0x01,
};
// 0x309FF3
RETAIL_DATA(".sdata", 1) u8 D_00309FF3[5] RETAIL(D_00309FF3) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x309FF8
RETAIL_DATA(".sdata", 8) char D_00309FF8[16] RETAIL(D_00309FF8) = "Arse";
// 0x30A008
RETAIL_DATA(".sdata", 8) char D_0030A008[8] RETAIL(D_0030A008) = ".sm2";
// 0x30A010
RETAIL_DATA(".sdata", 16) const void* D_0030A010[2] RETAIL(D_0030A010) = {
    &D_0030A008, nullptr,
};
// 0x30A018
RETAIL_DATA(".sdata", 8) char D_0030A018[8] RETAIL(D_0030A018) = ".sn";
// 0x30A020
RETAIL_DATA(".sdata", 32) char D_0030A020[8] RETAIL(D_0030A020) = ".lvl";
// 0x30A028
RETAIL_DATA(".sdata", 8) char D_0030A028[8] RETAIL(D_0030A028) = ".lgt";
// 0x30A030
RETAIL_DATA(".sdata", 16) char D_0030A030[8] RETAIL(D_0030A030) = ".sca";
// 0x30A038
RETAIL_DATA(".sdata", 8) char D_0030A038[12] RETAIL(D_0030A038) = ".lk";
// 0x30A044
RETAIL_DATA(".sdata", 4) u8 HullBuilderPointCount[1] RETAIL(HullBuilderPointCount) = {
    0x00,
};
// 0x30A045: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030A045[3] RETAIL(D_0030A045) = {
    0x00, 0x00, 0x00,
};
// 0x30A048
RETAIL_DATA(".sdata", 8) u32 HullBuilderCounts[4] RETAIL(HullBuilderCounts) = {
    0x0, 0x0, 0x0, 0x0,
};
// 0x30A058
RETAIL_DATA(".sdata", 8) u32 D_0030A058[1] RETAIL(D_0030A058) = {
    0x0,
};
// 0x30A05C
RETAIL_DATA(".sdata", 4) u32 D_0030A05C[1] RETAIL(D_0030A05C) = {
    0x0,
};
// 0x30A060
RETAIL_DATA(".sdata", 32) u32 G_DynamicSceneryClockIndex[1] RETAIL(G_DynamicSceneryClockIndex) = {
    0xFFFFFFFF,
};
// 0x30A064: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A064[5] RETAIL(D_0030A064) = {
    0x0, 0x0, 0x0, 0x10100, 0x0,
};
// 0x30A078
RETAIL_DATA(".sdata", 8) char D_0030A078[28] RETAIL(D_0030A078) = "> \000\000\000\000\000\000  \000\000\000\000\000\000 \000\000\000\000\000\000\000//";
// 0x30A094
RETAIL_DATA(".sdata", 4) u32 D_0030A094[4] RETAIL(D_0030A094) = {
    0x0, 0x0, 0x0, 0x0,
};
// 0x30A0A4
RETAIL_DATA(".sdata", 4) f32 D_0030A0A4[1] RETAIL(D_0030A0A4) = {
    Rounded(0.02),
};
// 0x30A0A8
RETAIL_DATA(".sdata", 8) u32 G_ChunkManager[1] RETAIL(G_ChunkManager) = {
    0x0,
};
// 0x30A0AC
RETAIL_DATA(".sdata", 4) u32 G_StateDepth[1] RETAIL(G_StateDepth) = {
    0x0,
};
// 0x30A0B0: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_0030A0B0[2] RETAIL(D_0030A0B0) = {
    0x0, 0x0,
};
// 0x30A0B8
RETAIL_DATA(".sdata", 8) u32 G_ScriptStateBody_Ptr[1] RETAIL(G_ScriptStateBody_Ptr) = {
    0x0,
};
// 0x30A0BC
RETAIL_DATA(".sdata", 4) u32 G_ScriptCondition_Ptr[1] RETAIL(G_ScriptCondition_Ptr) = {
    0x0,
};
// 0x30A0C0
RETAIL_DATA(".sdata", 64) u32 G_ScriptTable[1] RETAIL(G_ScriptTable) = {
    0x0,
};
// 0x30A0C4
RETAIL_DATA(".sdata", 4) f32 D_0030A0C4[1] RETAIL(D_0030A0C4) = {
    Rounded(0.02),
};
// 0x30A0C8
RETAIL_DATA(".sdata", 8) u32 G_ChunkManager_0030A0C8[1] RETAIL(G_ChunkManager_0030A0C8) = {
    0x0,
};
// 0x30A0CC: nothing uses it
RETAIL_DATA(".sdata", 4) u8 D_0030A0CC[29] RETAIL(D_0030A0CC) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A0E9
RETAIL_DATA(".sdata", 1) u8 D_0030A0E9[15] RETAIL(D_0030A0E9) = {
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x42,
};
// 0x30A0F8
RETAIL_DATA(".sdata", 8) u32 D_0030A0F8[1] RETAIL(D_0030A0F8) = {
    0x0,
};
// 0x30A0FC
RETAIL_DATA(".sdata", 4) s32 D_0030A0FC[1] RETAIL(D_0030A0FC) = {
    0,
};
// 0x30A100
RETAIL_DATA(".sdata", 64) u32 G_CurrentScriptCall[1] RETAIL(G_CurrentScriptCall) = {
    0x0,
};
// 0x30A104
RETAIL_DATA(".sdata", 4) u32 D_0030A104[2] RETAIL(D_0030A104) = {
    0x0, 0x0,
};
// 0x30A10C
RETAIL_DATA(".sdata", 4) u32 D_0030A10C[1] RETAIL(D_0030A10C) = {
    0x0,
};
// 0x30A110
RETAIL_DATA(".sdata", 16) u32 G_MiniBigBoi[1] RETAIL(G_MiniBigBoi) = {
    0x0,
};
// 0x30A114: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A114[1] RETAIL(D_0030A114) = {
    0x0,
};
// 0x30A118
RETAIL_DATA(".sdata", 8) u32 D_0030A118[1] RETAIL(D_0030A118) = {
    0xFFFFFFFF,
};
// 0x30A11C
RETAIL_DATA(".sdata", 4) u32 D_0030A11C[1] RETAIL(D_0030A11C) = {
    0xFFFFFFFF,
};
// 0x30A120
RETAIL_DATA(".sdata", 32) u32 D_0030A120[4] RETAIL(D_0030A120) = {
    0xFFFFFFFF, 0x0, 0x0, 0x0,
};
// 0x30A130
RETAIL_DATA(".sdata", 16) char D_0030A130[4] RETAIL(D_0030A130) = "%d";
// 0x30A134
RETAIL_DATA(".sdata", 4) u32 D_0030A134[1] RETAIL(D_0030A134) = {
    0x0,
};
// 0x30A138
RETAIL_DATA(".sdata", 8) char D_0030A138[4] RETAIL(D_0030A138) = " : ";
// 0x30A13C
RETAIL_DATA(".sdata", 4) u32 D_0030A13C[4] RETAIL(D_0030A13C) = {
    0x0, 0x0, 0x0, 0x0,
};
// 0x30A14C
RETAIL_DATA(".sdata", 4) u32 G_GameResourcesManager_[1] RETAIL(G_GameResourcesManager_) = {
    0x0,
};
// 0x30A150
RETAIL_DATA(".sdata", 16) u32 G_ChunkManager_[1] RETAIL(G_ChunkManager_) = {
    0x0,
};
// 0x30A154: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A154[1] RETAIL(D_0030A154) = {
    0x0,
};
// 0x30A158
RETAIL_DATA(".sdata", 8) char D_0030A158[8] RETAIL(D_0030A158) = ".rm2";
// 0x30A160
RETAIL_DATA(".sdata", 32) const void* D_0030A160[2] RETAIL(D_0030A160) = {
    &D_0030A158, nullptr,
};
// 0x30A168
RETAIL_DATA(".sdata", 8) char D_0030A168[8] RETAIL(D_0030A168) = ".ga2";
// 0x30A170
RETAIL_DATA(".sdata", 16) char D_0030A170[8] RETAIL(D_0030A170) = ".gc2";
// 0x30A178
RETAIL_DATA(".sdata", 8) char D_0030A178[8] RETAIL(D_0030A178) = ".ge2";
// 0x30A180
RETAIL_DATA(".sdata", 64) char D_0030A180[8] RETAIL(D_0030A180) = ".gw2";
// 0x30A188
RETAIL_DATA(".sdata", 8) char D_0030A188[8] RETAIL(D_0030A188) = ".ma2";
// 0x30A190
RETAIL_DATA(".sdata", 16) char D_0030A190[8] RETAIL(D_0030A190) = ".mc2";
// 0x30A198
RETAIL_DATA(".sdata", 8) char D_0030A198[8] RETAIL(D_0030A198) = ".me2";
// 0x30A1A0
RETAIL_DATA(".sdata", 32) char D_0030A1A0[8] RETAIL(D_0030A1A0) = ".ptl";
// 0x30A1A8
RETAIL_DATA(".sdata", 8) char D_0030A1A8[8] RETAIL(D_0030A1A8) = ".su2";
// 0x30A1B0
RETAIL_DATA(".sdata", 16) u32 D_0030A1B0[9] RETAIL(D_0030A1B0) = {
    0x6972742E, 0x0, 0x2020, 0x0, 0x0, 0x0, 0x1003, 0x0, 0x0,
};
// 0x30A1D4
RETAIL_DATA(".sdata", 4) f32 D_0030A1D4[1] RETAIL(D_0030A1D4) = {
    5.0f,
};
// 0x30A1D8
RETAIL_DATA(".sdata", 8) f32 D_0030A1D8[13] RETAIL(D_0030A1D8) = {
    5.0f, __builtin_bit_cast(f32, 0x00000100u), -5.0f, 1e+01f, Rounded(25.6), 0.0390625f, __builtin_bit_cast(f32, 0x00010000u),
    -5e+01f, 1e+02f, Rounded(655.36), Rounded(0.0015258789), 0.0f, 0.0f,
};
// 0x30A20C
RETAIL_DATA(".sdata", 4) u8 D_0030A20C[5] RETAIL(D_0030A20C) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A211
RETAIL_DATA(".sdata", 1) u8 D_0030A211[7] RETAIL(D_0030A211) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A218
RETAIL_DATA(".sdata", 8) s32 D_0030A218[5] RETAIL(D_0030A218) = {
    -1, -1, 0, 0, 0,
};
// 0x30A22C
RETAIL_DATA(".sdata", 4) const void* G_BLEND_SHAPE_FLOATS_ALLOCATOR[1] RETAIL(G_BLEND_SHAPE_FLOATS_ALLOCATOR) = {
    Ref_G_BLEND_SHAPE_FLOATS,
};
// 0x30A230
RETAIL_DATA(".sdata", 16) u32 D_0030A230[16] RETAIL(D_0030A230) = {
    0x0, 0x0, 0x7461642E, 0x0, 0x2E524944, 0x746164, 0x5C, 0x0, 0x7268632E, 0x0, 0x326E612E, 0x0, 0x0, 0x0, 0x2020, 0x0,
};
// 0x30A270
RETAIL_DATA(".sdata", 16) char D_0030A270[8] RETAIL(D_0030A270) = "%03d";
// 0x30A278
RETAIL_DATA(".sdata", 8) char D_0030A278[8] RETAIL(D_0030A278) = "_";
// 0x30A280
RETAIL_DATA(".sdata", 64) char D_0030A280[8] RETAIL(D_0030A280) = ".cts";
// 0x30A288
RETAIL_DATA(".sdata", 8) char D_0030A288[8] RETAIL(D_0030A288) = "";
// 0x30A290
RETAIL_DATA(".sdata", 16) s32 D_0030A290[2] RETAIL(D_0030A290) = {
    33, 35,
};
// 0x30A298
RETAIL_DATA(".sdata", 8) s32 D_0030A298[2] RETAIL(D_0030A298) = {
    36, 37,
};
// 0x30A2A0
RETAIL_DATA(".sdata", 32) s32 D_0030A2A0[2] RETAIL(D_0030A2A0) = {
    30, 0,
};
// 0x30A2A8
RETAIL_DATA(".sdata", 8) s32 D_0030A2A8[2] RETAIL(D_0030A2A8) = {
    36, 37,
};
// 0x30A2B0
RETAIL_DATA(".sdata", 16) s32 D_0030A2B0[2] RETAIL(D_0030A2B0) = {
    30, 0,
};
// 0x30A2B8
RETAIL_DATA(".sdata", 8) s32 D_0030A2B8[2] RETAIL(D_0030A2B8) = {
    30, 0,
};
// 0x30A2C0
RETAIL_DATA(".sdata", 64) s32 D_0030A2C0[2] RETAIL(D_0030A2C0) = {
    36, 37,
};
// 0x30A2C8
RETAIL_DATA(".sdata", 8) s32 D_0030A2C8[2] RETAIL(D_0030A2C8) = {
    36, 37,
};
// 0x30A2D0
RETAIL_DATA(".sdata", 16) s32 D_0030A2D0[2] RETAIL(D_0030A2D0) = {
    30, 0,
};
// 0x30A2D8
RETAIL_DATA(".sdata", 8) s32 D_0030A2D8[2] RETAIL(D_0030A2D8) = {
    30, 0,
};
// 0x30A2E0
RETAIL_DATA(".sdata", 32) s32 D_0030A2E0[2] RETAIL(D_0030A2E0) = {
    30, 0,
};
// 0x30A2E8
RETAIL_DATA(".sdata", 8) char D_0030A2E8[8] RETAIL(D_0030A2E8) = " - ";
// 0x30A2F0
RETAIL_DATA(".sdata", 16) char D_0030A2F0[8] RETAIL(D_0030A2F0) = ":0";
// 0x30A2F8
RETAIL_DATA(".sdata", 8) char D_0030A2F8[8] RETAIL(D_0030A2F8) = ":";
// 0x30A300
RETAIL_DATA(".sdata", 64) char D_0030A300[8] RETAIL(D_0030A300) = " ";
// 0x30A308
RETAIL_DATA(".sdata", 8) char D_0030A308[8] RETAIL(D_0030A308) = "/";
// 0x30A310
RETAIL_DATA(".sdata", 16) char D_0030A310[8] RETAIL(D_0030A310) = "Empty";
// 0x30A318
RETAIL_DATA(".sdata", 8) char D_0030A318[8] RETAIL(D_0030A318) = "Cancel";
// 0x30A320
RETAIL_DATA(".sdata", 32) char D_0030A320[8] RETAIL(D_0030A320) = "Format";
// 0x30A328
RETAIL_DATA(".sdata", 8) char D_0030A328[8] RETAIL(D_0030A328) = "Retry";
// 0x30A330
RETAIL_DATA(".sdata", 16) char D_0030A330[8] RETAIL(D_0030A330) = "Yes";
// 0x30A338
RETAIL_DATA(".sdata", 8) u32 D_0030A338[2] RETAIL(D_0030A338) = {
    0x6F4E, 0x0,
};
// 0x30A340
RETAIL_DATA(".sdata", 64) char D_0030A340[8] RETAIL(D_0030A340) = "(x)";
// 0x30A348
RETAIL_DATA(".sdata", 8) char D_0030A348[8] RETAIL(D_0030A348) = "(xxx)";
// 0x30A350: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_0030A350[1] RETAIL(D_0030A350) = {
    0x4542,
};
// 0x30A354: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A354[1] RETAIL(D_0030A354) = {
    0x0,
};
// 0x30A358: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A358[1] RETAIL(D_0030A358) = {
    0x0,
};
// 0x30A35C: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A35C[1] RETAIL(D_0030A35C) = {
    0x0,
};
// 0x30A360: nothing uses it
RETAIL_DATA(".sdata", 32) u32 D_0030A360[1] RETAIL(D_0030A360) = {
    0xFFFFFFFF,
};
// 0x30A364: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A364[1] RETAIL(D_0030A364) = {
    0x0,
};
// 0x30A368: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A368[1] RETAIL(D_0030A368) = {
    0x0,
};
// 0x30A36C: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A36C[1] RETAIL(D_0030A36C) = {
    0xFFFFFFFF,
};
// 0x30A370: nothing uses it
RETAIL_DATA(".sdata", 16) char D_0030A370[8] RETAIL(D_0030A370) = "/%s%s%s";
// 0x30A378: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A378[1] RETAIL(D_0030A378) = {
    0x0,
};
// 0x30A37C: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A37C[1] RETAIL(D_0030A37C) = {
    0x0,
};
// 0x30A380: nothing uses it
RETAIL_DATA(".sdata", 64) u32 D_0030A380[4] RETAIL(D_0030A380) = {
    0x0, 0x0, 0x73257325, 0x7325,
};
// 0x30A390
RETAIL_DATA(".sdata", 16) char D_0030A390[16] RETAIL(D_0030A390) = "PS2D";
// 0x30A3A0: nothing uses it
RETAIL_DATA(".sdata", 32) char D_0030A3A0[8] RETAIL(D_0030A3A0) = "Nothing";
// 0x30A3A8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A3A8[2] RETAIL(D_0030A3A8) = {
    0x686D, 0x0,
};
// 0x30A3B0: nothing uses it
RETAIL_DATA(".sdata", 16) char D_0030A3B0[8] RETAIL(D_0030A3B0) = "%s.mb";
// 0x30A3B8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A3B8[1] RETAIL(D_0030A3B8) = {
    0x0,
};
// 0x30A3BC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A3BC[1] RETAIL(D_0030A3BC) = {
    0x0,
};
// 0x30A3C0: nothing uses it
RETAIL_DATA(".sdata", 64) u8 D_0030A3C0[1] RETAIL(D_0030A3C0) = {
    0x00,
};
// 0x30A3C1: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030A3C1[1] RETAIL(D_0030A3C1) = {
    0x00,
};
// 0x30A3C2: nothing uses it
RETAIL_DATA(".sdata", 2) u8 D_0030A3C2[1] RETAIL(D_0030A3C2) = {
    0x00,
};
// 0x30A3C3: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030A3C3[1] RETAIL(D_0030A3C3) = {
    0x00,
};
// 0x30A3C4: nothing uses it
RETAIL_DATA(".sdata", 4) u32 G_PSS_FileLocation[1] RETAIL(G_PSS_FileLocation) = {
    0xFFFFFFFF,
};
// 0x30A3C8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 G_PSS_FileSize[1] RETAIL(G_PSS_FileSize) = {
    0x0,
};
// 0x30A3CC: nothing uses it
RETAIL_DATA(".sdata", 4) u32 D_0030A3CC[1] RETAIL(D_0030A3CC) = {
    0x0,
};
// 0x30A3D0: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_0030A3D0[2] RETAIL(D_0030A3D0) = {
    0x1, 0x0,
};
// 0x30A3D8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A3D8[2] RETAIL(D_0030A3D8) = {
    0x0, 0x0,
};
// 0x30A3E0: nothing uses it
RETAIL_DATA(".sdata", 32) u32 D_0030A3E0[2] RETAIL(D_0030A3E0) = {
    0x5C, 0x0,
};
// 0x30A3E8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A3E8[2] RETAIL(D_0030A3E8) = {
    0x2E, 0x0,
};
// 0x30A3F0: nothing uses it
RETAIL_DATA(".sdata", 16) u32 D_0030A3F0[2] RETAIL(D_0030A3F0) = {
    0x313B, 0x0,
};
// 0x30A3F8: nothing uses it
RETAIL_DATA(".sdata", 8) u32 D_0030A3F8[10] RETAIL(D_0030A3F8) = {
    0x737370, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2020, 0x0,
};
// 0x30A420: nothing uses it
RETAIL_DATA(".sdata", 32) u8 D_0030A420[3] RETAIL(D_0030A420) = {
    0x3B, 0x31, 0x00,
};
// 0x30A423
RETAIL_DATA(".sdata", 1) u8 D_0030A423[5] RETAIL(D_0030A423) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A428
RETAIL_DATA(".sdata", 8) u32 G_GameReadersStorages[2] RETAIL(G_GameReadersStorages) = {
    0x0, 0x0,
};
// 0x30A430
RETAIL_DATA(".sdata", 16) s32 D_0030A430[1] RETAIL(D_0030A430) = {
    0,
};
// 0x30A434
RETAIL_DATA(".sdata", 4) u32 D_0030A434[1] RETAIL(D_0030A434) = {
    0x0,
};
// 0x30A438
RETAIL_DATA(".sdata", 8) u32 D_0030A438[2] RETAIL(D_0030A438) = {
    0x0, 0x0,
};
// 0x30A440: nothing uses it
RETAIL_DATA(".sdata", 64) u8 G_archiveExtesion_Header[3] RETAIL(G_archiveExtesion_Header) = {
    0x2E, 0x42, 0x48,
};
// 0x30A443: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030A443[5] RETAIL(D_0030A443) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A448: nothing uses it
RETAIL_DATA(".sdata", 8) u8 G_archiveExtesion_Data[3] RETAIL(G_archiveExtesion_Data) = {
    0x2E, 0x42, 0x44,
};
// 0x30A44B: nothing uses it
RETAIL_DATA(".sdata", 1) u8 D_0030A44B[5] RETAIL(D_0030A44B) = {
    0x00, 0x00, 0x00, 0x00, 0x00,
};
// 0x30A450: nothing uses it
RETAIL_DATA(".sdata", 16) char D_0030A450[8] RETAIL(D_0030A450) = "HOST0:";
// 0x30A458
RETAIL_DATA(".sdata", 8) s32 D_0030A458[2] RETAIL(D_0030A458) = {
    1, 0,
};
} // namespace RetailData
