#pragma once

#include "common.h"
#include "gcc2.h"
#include "game/clock.h"
#include "game/math.h"
#include "game/graphicstables.h"
#include "platform/graphics.h"

// The UI's 2D shapes (their vtables the retail ones). A shape has a material (the platform's: its resource's when it has one), the
// material resource it holds a reference of (or none), whether it's drawn turned (a matrix places its corners rather than its
// edges) and its vtable. The vtable's functions: the destructor, the material set (the resource let go of), the material set by
// its resource's ID, the resource let go of, the draws (in the default colour, in a colour, placed by a matrix in the default
// colour, placed by a matrix in a colour) and the shape read from a file
class Shape2D
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        SetMaterialSlot = 2,
        SetMaterialByIdSlot = 3,
        ReleaseResourceSlot = 4,
        DrawSlot = 5,
        DrawColouredSlot = 6,
        DrawPlacedSlot = 7,
        DrawPlacedColouredSlot = 8,
        ReadSlot = 9,
    };

    Material* material;
    MaterialResource* resource;
    u8 turned;
    u8 unused09[3];
    const GccVTableEntry* vtable;

    void Destroy(u32 flags) RETAIL(FUN_001ab8a0);
    void SetMaterial(Material* material) RETAIL(FUN_001ab8f0);
    void SetMaterialById(u32 id) RETAIL(FUN_001ab938);
    void ReleaseResource() RETAIL(FUN_001ab9a8);
    // Its texture's and material's IDs read, the resources read too when the tables haven't got them, the material set by its ID
    void Read(class Stream* stream) RETAIL(FUN_001a7250);
};
CHECK_SIZE(Shape2D, 0x10);

// A textured rectangle showing an area of its texture (fractions of the texture): the unit square (y up) a matrix places in
// fractions of the screen, the whole frame without one. Turned, the square is taken around its middle and the matrix places its
// four corners
class Sprite : public Shape2D
{
public:
    Platform::Graphics::Rectangle texture;

    static Sprite* Construct(Sprite* sprite) RETAIL(FUN_001ac4b0);
    void Destroy(u32 flags) RETAIL(FUN_001abc58);
    // The material, showing the texture's whole area
    void SetMaterial(Material* material) RETAIL(FUN_001ac4e0);
    // The material, showing the texture's area between two corners
    void SetMaterial(Material* material, const Vector2* start, const Vector2* end) RETAIL(FUN_001ac540);
    void SetMaterialById(u32 id) RETAIL(FUN_001ac5b0);
    void Draw() RETAIL(FUN_001a8ce0);
    void Draw(u32 colour) RETAIL(FUN_001a8f30);
    void Draw(const Matrix4x4* matrix) RETAIL(FUN_001ac640);
    void Draw(const Matrix4x4* matrix, u32 colour) RETAIL(FUN_001a9170);
};
CHECK_SIZE(Sprite, 0x20);

// A strip's flags: whether it owns its arrays (nothing sets it)
union StripFlags
{
    u8 value;
    struct
    {
        u8 ownsArrays : 1;
        u8 unused1 : 7;
    };
};
CHECK_SIZE(StripFlags, 1);

// A triangle strip of coloured vertexes without a texture, its places in pixels from the frame's corner (moved by a matrix when
// it's drawn through one), its colours tinted by the colour it's drawn in (192 is the full red, green and blue, 128 the full
// alpha). It frees the arrays it owns. A strip has at most MostVertexes (its count is a byte)
class Strip : public Shape2D
{
public:
    static constexpr u32 MostVertexes = 0x100;

    u8 count;
    StripFlags flags;
    u8 unused12[2];
    Vector2* places;
    u32* colours;

    static Strip* Construct(Strip* strip) RETAIL(FUN_001ac6a8);
    void Destroy(u32 flags) RETAIL(FUN_001ac708);
    // The vertexes (the arrays stay the caller's), the arrays it owned freed
    void SetVertexes(u8 count, Vector2* places, u32* colours) RETAIL(FUN_001ac7b8);
    void TintColours(u32 colour, u32* tinted) RETAIL(FUN_001a9708);
    void Draw() RETAIL(FUN_001a98b0);
    void Draw(u32 colour) RETAIL(FUN_001a9a30);
    void Draw(const Matrix4x4* matrix) RETAIL(FUN_001a9bd8);
    void Draw(const Matrix4x4* matrix, u32 colour) RETAIL(FUN_001a9df0);
};
CHECK_SIZE(Strip, 0x1C);

// A ring: a strip between an outer and an inner edge, each an ellipse (its radii the scale given times the edge's scale, in
// fractions of the screen) turned by its rotation, from its start angle round its span in segments of steps (a segment's last
// point is the next one's first), shaped by the edge's curve (its radii multiplied by the curve's value at the point's fraction of
// the way) and pushed towards a square by its squareness. The edges are coloured by their curves (the colour given multiplied by
// the curve's value; its alpha only at the end) or in the ring's colour. Without its own inner edge, the inner edge is the
// previous ring's outer one, or the middle in the ring's middle colour. Its arrays hold an outer and an inner vertex for each point,
// and one more pair for the first again
class Ring : public Strip
{
public:
    u32 middleColour;
    u32 colour;
    Vector2 innerScale;
    Vector2 outerScale;
    Vector4Curve* innerColours;
    Vector4Curve* outerColours;
    Vector2Curve* innerShape;
    Vector2Curve* outerShape;
    s32 rotation;
    s32 startAngle;
    s32 span;
    f32 squareness;
    u32* vertexColours;
    Vector2* vertexPlaces;

    // A ring of the points (both edges' vertexes drawn)
    static Ring* Construct(Ring* ring, u32 points, Material* material) RETAIL(FUN_001a89f8);
    void Destroy(u32 flags) RETAIL(FUN_001ac2f0);
    // The vertexes for the ring's place and scale (fractions of the screen), drawn in a colour, inside the previous ring
    void Update(TimeClock* clock, u32 segments, u32 steps, u32 tint, const Vector2* place, const Vector2* scale,
                const Ring* previous) RETAIL(FUN_001ac358);
    // An edge's colours and places (in pixels)
    void BuildColours(u32 points, u32 tint, u32 inner) RETAIL(FUN_001a8500);
    void BuildEdge(const Vector2* place, const Vector2* scale, u32 segments, u32 steps, u32 inner) RETAIL(FUN_001a8190);
    // The inner edge taken from the previous ring, or made the middle
    void JoinInner(u32 points, const Vector2* place, u32 tint, const Ring* previous) RETAIL(FUN_001a8858);
    // The point a fraction t of the way round the line between the edges, for the place and scale (fractions of the screen)
    void PointAt(const Vector2* place, const Vector2* scale, Vector2* out, f32 t) RETAIL_N32(FUN_001a8b78);
};
CHECK_SIZE(Ring, 0x5C);

extern "C"
{
    extern const GccVTableEntry g_Shape2DVTable[] RETAIL(D_002F6F28);
    extern const GccVTableEntry g_SpriteVTable[] RETAIL(D_002F6ED0);
    extern const GccVTableEntry g_StripVTable[] RETAIL(D_002F6D08);
    extern const GccVTableEntry g_RingVTable[] RETAIL(D_002F6CB0);
}
