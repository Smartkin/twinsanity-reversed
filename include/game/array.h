#pragma once

#include "common.h"
#include "game/memory.h"
#include "gcc2.h"

// The game's growable arrays of pointers (a template the game instantiates per type): a full one grows by its growth into a new
// array (new[]), the old one copied over and freed (delete[]). The game walks them with iterator objects whose vtables are the
// template's instances, which the C++ does with loops
template <typename T>
struct PointerArray
{
    T** data;
    u32 count;
    u32 capacity;
    u32 growth;

    void Append(T* item)
    {
        if (count == capacity)
        {
            u32 grown = count + growth;
            auto** larger = static_cast<T**>(MemoryAllocate2(grown * sizeof(T*)));
            for (u32 i = 0; i < count; i++)
            {
                larger[i] = data[i];
            }

            if (data != nullptr)
            {
                MemoryDeallocate_(data);
            }

            data = larger;
            capacity = grown;
        }

        data[count] = item;
        count++;
    }
};
CHECK_SIZE(PointerArray<void>, 0x10);

// The retail iterators over the pointer arrays (two instances of the template's over a menu page's items: D_00302A10, its base
// D_00302A68, the page's lookups', and DrawMenuPage's D_00302988, its base D_003029D8, which has no second Current): the array
// and the index it's at, done outside the array
struct ArrayIterator
{
    const GccVTableEntry* vtable;
    PointerArray<void>* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_0025c078);
    void BaseDestroy(u32 flags) RETAIL(FUN_0025c048);
    void First() RETAIL(FUN_0025c0a8);
    u32 IsDone() RETAIL(FUN_0025c0b0);
    void** Current() RETAIL(FUN_0025c0f0);
    void Next() RETAIL(FUN_0025c0e0);
    void Previous() RETAIL(FUN_0025d000);
    void Last() RETAIL(FUN_0025d010);
    void** CurrentAgain() RETAIL(FUN_0025d028);
    ArrayIterator* Assign(const ArrayIterator* other) RETAIL(func_0025D040);

    // The overlay's iterators over a renderer's text queue (its fonts' texts, D_002F6C20, its base D_002F6C78) and over a font's
    // texts (D_002F6B90, its base D_002F6BE8), which the C++ walks with loops
    void FontsDestroy(u32 flags) RETAIL(FUN_001ac968);
    void FontsBaseDestroy(u32 flags) RETAIL(FUN_001ac938);
    void FontsFirst() RETAIL(FUN_001ac998);
    u32 FontsIsDone() RETAIL(FUN_001ac9a0);
    void** FontsCurrent() RETAIL(FUN_001ac9e0);
    void FontsNext() RETAIL(FUN_001ac9d0);
    void FontsPrevious() RETAIL(FUN_001ad100);
    void FontsLast() RETAIL(FUN_001ad110);
    void** FontsCurrentAgain() RETAIL(FUN_001ad128);
    ArrayIterator* FontsAssign(const ArrayIterator* other) RETAIL(FUN_001ad140);
    void TextsDestroy(u32 flags) RETAIL(FUN_001acc80);
    void TextsBaseDestroy(u32 flags) RETAIL(FUN_001acc50);
    void TextsFirst() RETAIL(FUN_001accb0);
    u32 TextsIsDone() RETAIL(FUN_001accb8);
    void** TextsCurrent() RETAIL(FUN_001accf8);
    void TextsNext() RETAIL(FUN_001acce8);
    void TextsPrevious() RETAIL(FUN_001ad0a8);
    void TextsLast() RETAIL(FUN_001ad0b8);
    void** TextsCurrentAgain() RETAIL(FUN_001ad0d0);
    ArrayIterator* TextsAssign(const ArrayIterator* other) RETAIL(func_001AD0E8);

    void DrawnItemsDestroy(u32 flags) RETAIL(FUN_0025c138);
    void DrawnItemsBaseDestroy(u32 flags) RETAIL(FUN_0025c108);
    void DrawnItemsFirst() RETAIL(FUN_0025c168);
    u32 DrawnItemsIsDone() RETAIL(FUN_0025c170);
    void** DrawnItemsCurrent() RETAIL(FUN_0025c1b0);
    void DrawnItemsNext() RETAIL(FUN_0025c1a0);
    void DrawnItemsPrevious() RETAIL(FUN_0025cfc0);
    void DrawnItemsLast() RETAIL(FUN_0025cfd0);
    ArrayIterator* DrawnItemsAssign(const ArrayIterator* other) RETAIL(func_0025CFE8);

    // The instances module's iterator over the reference arrays (the queued objects, the instances with events: D_002F5F80, its
    // base the handle walk's D_002F2DE0), and the renderer's over a renderer's text queue (D_002F6398, its base D_002F63E8) and
    // over a font's texts (D_002F6288, its base D_002F62D8), which the C++ walks with loops
    void QueuedDestroy(u32 flags) RETAIL(FUN_0019aa48);
    void QueuedFirst() RETAIL(FUN_0019aa78);
    u32 QueuedIsDone() RETAIL(FUN_0019aa80);
    void** QueuedCurrent() RETAIL(FUN_0019aac0);
    void QueuedNext() RETAIL(FUN_0019aab0);
    void QueuedPrevious() RETAIL(FUN_0019ab10);
    void QueuedLast() RETAIL(FUN_0019ab20);
    void** QueuedCurrentAgain() RETAIL(FUN_0019ab38);
    ArrayIterator* QueuedAssign(const ArrayIterator* other) RETAIL(FUN_0019ab50);
    void RendererFontsDestroy(u32 flags) RETAIL(FUN_001a0a90);
    void RendererFontsBaseDestroy(u32 flags) RETAIL(FUN_001a0a60);
    void RendererFontsFirst() RETAIL(FUN_001a0ac0);
    u32 RendererFontsIsDone() RETAIL(FUN_001a0ac8);
    void** RendererFontsCurrent() RETAIL(FUN_001a0b08);
    void RendererFontsNext() RETAIL(FUN_001a0af8);
    void RendererFontsPrevious() RETAIL(FUN_001a2948);
    void RendererFontsLast() RETAIL(FUN_001a2958);
    ArrayIterator* RendererFontsAssign(const ArrayIterator* other) RETAIL(FUN_001a2970);
    void RendererTextsDestroy(u32 flags) RETAIL(FUN_001a1310);
    void RendererTextsBaseDestroy(u32 flags) RETAIL(FUN_001a12e0);
    void RendererTextsFirst() RETAIL(FUN_001a1340);
    u32 RendererTextsIsDone() RETAIL(FUN_001a1348);
    void** RendererTextsCurrent() RETAIL(FUN_001a1388);
    void RendererTextsNext() RETAIL(FUN_001a1378);
    void RendererTextsPrevious() RETAIL(FUN_001a28d0);
    void RendererTextsLast() RETAIL(FUN_001a28e0);
    ArrayIterator* RendererTextsAssign(const ArrayIterator* other) RETAIL(FUN_001a28f8);

    // The spring bodies' iterators (game/springbody.h) over a chain's springs (D_002F5E98, its base D_002F5EF0), a body's points
    // (D_002F5E08, its base D_002F5E60) and its chains (D_002F5D78, its base D_002F5DD0), which the C++ walks with loops
    void SpringsDestroy(u32 flags) RETAIL(FUN_001913e0);
    void SpringsBaseDestroy(u32 flags) RETAIL(FUN_001913b0);
    void SpringsFirst() RETAIL(FUN_00191410);
    u32 SpringsIsDone() RETAIL(FUN_00191418);
    void** SpringsCurrent() RETAIL(FUN_00191458);
    void SpringsNext() RETAIL(FUN_00191448);
    void SpringsPrevious() RETAIL(FUN_00191d08);
    void SpringsLast() RETAIL(FUN_00191d18);
    void** SpringsCurrentAgain() RETAIL(FUN_00191d30);
    ArrayIterator* SpringsAssign(const ArrayIterator* other) RETAIL(func_00191D48);
    void PointsDestroy(u32 flags) RETAIL(FUN_00191628);
    void PointsBaseDestroy(u32 flags) RETAIL(FUN_001915f8);
    void PointsFirst() RETAIL(FUN_00191658);
    u32 PointsIsDone() RETAIL(FUN_00191660);
    void** PointsCurrent() RETAIL(FUN_001916a0);
    void PointsNext() RETAIL(FUN_00191690);
    void PointsPrevious() RETAIL(FUN_00191cb0);
    void PointsLast() RETAIL(FUN_00191cc0);
    void** PointsCurrentAgain() RETAIL(FUN_00191cd8);
    ArrayIterator* PointsAssign(const ArrayIterator* other) RETAIL(func_00191CF0);
    void ChainsDestroy(u32 flags) RETAIL(FUN_001916f0);
    void ChainsBaseDestroy(u32 flags) RETAIL(FUN_001916c0);
    void ChainsFirst() RETAIL(FUN_00191720);
    u32 ChainsIsDone() RETAIL(FUN_00191728);
    void** ChainsCurrent() RETAIL(FUN_00191768);
    void ChainsNext() RETAIL(FUN_00191758);
    void ChainsPrevious() RETAIL(FUN_00191c58);
    void ChainsLast() RETAIL(FUN_00191c68);
    void** ChainsCurrentAgain() RETAIL(FUN_00191c80);
    ArrayIterator* ChainsAssign(const ArrayIterator* other) RETAIL(func_00191C98);

    // The layouts' lists' iterators (game/layoutiterators.cpp, game/instancesection.h): over the instance templates (D_00304A60,
    // its base D_00304AB8), the object instances (D_00303A60, D_00303AB8), the AI positions (D_00303948, D_003039A0), the AI paths
    // (D_003038B8, D_00303910), the positions (D_00304130, D_00304188), the paths (D_003040A0, D_003040F8), the triggers
    // (D_00303828, D_00303880), the cameras (D_00303798, D_003037F0) and the collision surfaces (D_00303708, D_00303760), which
    // the C++ walks with loops
    void TemplatesDestroy(u32 flags) RETAIL(FUN_0026d858);
    void TemplatesBaseDestroy(u32 flags) RETAIL(FUN_0026d828);
    void TemplatesFirst() RETAIL(FUN_0026d888);
    u32 TemplatesIsDone() RETAIL(FUN_0026ead0);
    void** TemplatesCurrent() RETAIL(FUN_0026d8a0);
    void TemplatesNext() RETAIL(FUN_0026d890);
    void TemplatesPrevious() RETAIL(FUN_0026eb00);
    void TemplatesLast() RETAIL(FUN_0026eb10);
    void** TemplatesCurrentAgain() RETAIL(FUN_0026eb28);
    ArrayIterator* TemplatesAssign(const ArrayIterator* other) RETAIL(func_0026EB40);
    void InstancesDestroy(u32 flags) RETAIL(FUN_0026aff0);
    void InstancesBaseDestroy(u32 flags) RETAIL(FUN_0026afc0);
    void InstancesFirst() RETAIL(FUN_0026b020);
    u32 InstancesIsDone() RETAIL(FUN_0026b028);
    void** InstancesCurrent() RETAIL(FUN_0026b068);
    void InstancesNext() RETAIL(FUN_0026b058);
    void InstancesPrevious() RETAIL(FUN_0026b890);
    void InstancesLast() RETAIL(FUN_0026b8a0);
    void** InstancesCurrentAgain() RETAIL(FUN_0026b8b8);
    ArrayIterator* InstancesAssign(const ArrayIterator* other) RETAIL(func_0026B8D0);
    void AiPositionsDestroy(u32 flags) RETAIL(FUN_0026b1f8);
    void AiPositionsBaseDestroy(u32 flags) RETAIL(FUN_0026b1c8);
    void AiPositionsFirst() RETAIL(FUN_0026b228);
    u32 AiPositionsIsDone() RETAIL(FUN_0026b800);
    void** AiPositionsCurrent() RETAIL(FUN_0026b240);
    void AiPositionsNext() RETAIL(FUN_0026b230);
    void AiPositionsPrevious() RETAIL(FUN_0026b830);
    void AiPositionsLast() RETAIL(FUN_0026b840);
    void** AiPositionsCurrentAgain() RETAIL(FUN_0026b858);
    ArrayIterator* AiPositionsAssign(const ArrayIterator* other) RETAIL(func_0026B870);
    void AiPathsDestroy(u32 flags) RETAIL(FUN_0026b288);
    void AiPathsBaseDestroy(u32 flags) RETAIL(FUN_0026b258);
    void AiPathsFirst() RETAIL(FUN_0026b2b8);
    u32 AiPathsIsDone() RETAIL(FUN_0026b778);
    void** AiPathsCurrent() RETAIL(FUN_0026b2d0);
    void AiPathsNext() RETAIL(FUN_0026b2c0);
    void AiPathsPrevious() RETAIL(FUN_0026b7a8);
    void AiPathsLast() RETAIL(FUN_0026b7b8);
    void** AiPathsCurrentAgain() RETAIL(FUN_0026b7d0);
    ArrayIterator* AiPathsAssign(const ArrayIterator* other) RETAIL(func_0026B7E8);
    void PositionsDestroy(u32 flags) RETAIL(FUN_00268638);
    void PositionsBaseDestroy(u32 flags) RETAIL(FUN_00268608);
    void PositionsFirst() RETAIL(FUN_00268668);
    u32 PositionsIsDone() RETAIL(FUN_00268670);
    void** PositionsCurrent() RETAIL(FUN_002686b0);
    void PositionsNext() RETAIL(FUN_002686a0);
    void PositionsPrevious() RETAIL(FUN_0026c930);
    void PositionsLast() RETAIL(FUN_0026c940);
    void** PositionsCurrentAgain() RETAIL(FUN_0026c958);
    ArrayIterator* PositionsAssign(const ArrayIterator* other) RETAIL(func_0026C970);
    void PathsDestroy(u32 flags) RETAIL(FUN_002686f8);
    void PathsBaseDestroy(u32 flags) RETAIL(FUN_002686c8);
    void PathsFirst() RETAIL(FUN_00268728);
    u32 PathsIsDone() RETAIL(FUN_00268730);
    void** PathsCurrent() RETAIL(FUN_00268770);
    void PathsNext() RETAIL(FUN_00268760);
    void PathsPrevious() RETAIL(FUN_0026c8d8);
    void PathsLast() RETAIL(FUN_0026c8e8);
    void** PathsCurrentAgain() RETAIL(FUN_0026c900);
    ArrayIterator* PathsAssign(const ArrayIterator* other) RETAIL(func_0026C918);
    void TriggersDestroy(u32 flags) RETAIL(FUN_0026b318);
    void TriggersBaseDestroy(u32 flags) RETAIL(FUN_0026b2e8);
    void TriggersFirst() RETAIL(FUN_0026b348);
    u32 TriggersIsDone() RETAIL(FUN_0026b6f0);
    void** TriggersCurrent() RETAIL(FUN_0026b360);
    void TriggersNext() RETAIL(FUN_0026b350);
    void TriggersPrevious() RETAIL(FUN_0026b720);
    void TriggersLast() RETAIL(FUN_0026b730);
    void** TriggersCurrentAgain() RETAIL(FUN_0026b748);
    ArrayIterator* TriggersAssign(const ArrayIterator* other) RETAIL(func_0026B760);
    void CamerasDestroy(u32 flags) RETAIL(FUN_0026b3a8);
    void CamerasBaseDestroy(u32 flags) RETAIL(FUN_0026b378);
    void CamerasFirst() RETAIL(FUN_0026b3d8);
    u32 CamerasIsDone() RETAIL(FUN_0026b668);
    void** CamerasCurrent() RETAIL(FUN_0026b3f0);
    void CamerasNext() RETAIL(FUN_0026b3e0);
    void CamerasPrevious() RETAIL(FUN_0026b698);
    void CamerasLast() RETAIL(FUN_0026b6a8);
    void** CamerasCurrentAgain() RETAIL(FUN_0026b6c0);
    ArrayIterator* CamerasAssign(const ArrayIterator* other) RETAIL(func_0026B6D8);
    void SurfacesDestroy(u32 flags) RETAIL(FUN_0026b438);
    void SurfacesBaseDestroy(u32 flags) RETAIL(FUN_0026b408);
    void SurfacesFirst() RETAIL(FUN_0026b468);
    u32 SurfacesIsDone() RETAIL(FUN_0026b5e0);
    void** SurfacesCurrent() RETAIL(FUN_0026b480);
    void SurfacesNext() RETAIL(FUN_0026b470);
    void SurfacesPrevious() RETAIL(FUN_0026b610);
    void SurfacesLast() RETAIL(FUN_0026b620);
    void** SurfacesCurrentAgain() RETAIL(FUN_0026b638);
    ArrayIterator* SurfacesAssign(const ArrayIterator* other) RETAIL(func_0026B650);
};
CHECK_SIZE(ArrayIterator, 0xC);

// The retail iterator over the arrays the game sizes once (the animators' tables: {items, count}; D_003058A8, its base
// D_00305900): the same as the pointer arrays'
struct SizedArrayIterator
{
    struct Array
    {
        void** data;
        s32 size;
    };

    const GccVTableEntry* vtable;
    Array* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_00298b40);
    void BaseDestroy(u32 flags) RETAIL(FUN_00298b10);
    void First() RETAIL(FUN_00298b70);
    u32 IsDone() RETAIL(FUN_00298b78);
    void** Current() RETAIL(FUN_00298bb8);
    void Next() RETAIL(FUN_00298ba8);
    void Previous() RETAIL(FUN_00299aa8);
    void Last() RETAIL(FUN_00299ab8);
    void** CurrentAgain() RETAIL(FUN_00299ad0);
    SizedArrayIterator* Assign(const SizedArrayIterator* other) RETAIL(FUN_00299ae8);
};
CHECK_SIZE(SizedArrayIterator, 0xC);

// The retail iterator over an array of 12 byte elements (D_00303B80, its base D_00303BD8; game/layoutiterators.cpp), which
// nothing in the game makes: the array (its elements and their count) and the index it's at, done outside the array
struct ElementArrayIterator
{
    struct Array
    {
        u8* data;
        u32 count;
    };

    static constexpr u32 ElementSize = 0xC;

    const GccVTableEntry* vtable;
    Array* array;
    s32 index;

    void Destroy(u32 flags) RETAIL(FUN_0026a778);
    void BaseDestroy(u32 flags) RETAIL(FUN_0026a748);
    void First() RETAIL(FUN_0026a7a8);
    u32 IsDone() RETAIL(FUN_0026a7b0);
    void* Current() RETAIL(FUN_0026a7f0);
    void Next() RETAIL(FUN_0026a7e0);
    void Previous() RETAIL(func_0026B940);
    void Last() RETAIL(func_0026B950);
    void* CurrentAgain() RETAIL(FUN_0026b968);
    ElementArrayIterator* Assign(const ElementArrayIterator* other) RETAIL(func_0026B988);
};
CHECK_SIZE(ElementArrayIterator, 0xC);

extern "C"
{
    extern const GccVTableEntry g_SizedArrayIteratorBaseVTable[] RETAIL(D_00305900);
    extern const GccVTableEntry g_ArrayIteratorBaseVTable[] RETAIL(D_00302A68);
    extern const GccVTableEntry g_DrawnItemsIteratorBaseVTable[] RETAIL(D_003029D8);
    extern const GccVTableEntry g_FontsIteratorBaseVTable[] RETAIL(D_002F6C78);
    extern const GccVTableEntry g_TextsIteratorBaseVTable[] RETAIL(D_002F6BE8);
    extern const GccVTableEntry g_RendererFontsIteratorBaseVTable[] RETAIL(D_002F63E8);
    extern const GccVTableEntry g_RendererTextsIteratorBaseVTable[] RETAIL(D_002F62D8);
}
