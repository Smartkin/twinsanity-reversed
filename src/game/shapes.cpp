#include "game/shapes.h"

#include "game/colour.h"
#include "game/memory.h"
#include "game/renderer.h"
#include "game/stream.h"

namespace
{
// The unit square's corners a sprite is drawn between (y up), and its corners around its middle a turned sprite's matrix places,
// in the texture area's order (its corner, the corner a height from it, the one a width from it, the opposite one)
constexpr Vector4 SquareStart = {0.0f, 1.0f, 0.0f, 1.0f};
constexpr Vector4 SquareEnd = {1.0f, 0.0f, 0.0f, 1.0f};
constexpr Vector4 TurnedCorners[4] = {
    {-0.5f, 0.5f, 0.0f, 1.0f},
    {-0.5f, -0.5f, 0.0f, 1.0f},
    {0.5f, 0.5f, 0.0f, 1.0f},
    {0.5f, -0.5f, 0.0f, 1.0f},
};

// A ring's squareness that leaves it an ellipse, its arrays' vertexes for each point
constexpr f32 RoundEpsilon = Epsilon;
constexpr u32 PointVertexes = 2;

// A colour curve's value at t: between two points, made with the game's vector maths, its w (the alpha) is 1
Vector4 SampleColours(const Vector4Curve* curve, f32 t)
{
    if (!(t < 1.0f))
    {
        return curve->points[curve->count - 1];
    }

    f32 place = static_cast<f32>(static_cast<s32>(curve->count - 1)) * t;
    s32 index = static_cast<s32>(place);
    f32 fraction = place - static_cast<f32>(index);
    const Vector4& from = curve->points[index];
    const Vector4& to = curve->points[index + 1];
    return {from.x + (to.x - from.x) * fraction, from.y + (to.y - from.y) * fraction, from.z + (to.z - from.z) * fraction,
            1.0f};
}

// A point of an edge pushed towards the square around the ellipse (y turned down the screen)
Vector4 Squared(f32 x, f32 y, f32 squareness)
{
    if (__builtin_fabsf(squareness) <= RoundEpsilon)
    {
        return {x, -y, 0.0f, 1.0f};
    }

    f32 keep = 1.0f - squareness;
    f32 keptX = x * keep;
    f32 keptY = y * -keep;
    return {keptX < 0.0f ? keptX - squareness : keptX + squareness, keptY < 0.0f ? keptY - squareness : keptY + squareness, 0.0f,
            1.0f};
}

u32 DefaultColour()
{
    u32 colour;
    GetColor(&colour, ColourWhite);
    return colour;
}

// The strip's arrays freed when it owns them, its count and flags cleared
void FreeArrays(Strip* strip)
{
    if (strip->flags.ownsArrays != 0)
    {
        if (strip->places != nullptr)
        {
            MemoryDeallocate_(strip->places);
        }

        if (strip->colours != nullptr)
        {
            MemoryDeallocate_(strip->colours);
        }
    }

    strip->count = 0;
    strip->flags.value = 0;
    strip->unused12[0] = 0;
    strip->unused12[1] = 0;
}

// The strip's places moved by the matrix
void PlaceVertexes(const Strip* strip, const Matrix4x4* matrix, Vector2* placed)
{
    for (u32 vertex = 0; vertex < strip->count; vertex++)
    {
        Vector4 point = {strip->places[vertex].x, strip->places[vertex].y, 0.0f, 1.0f};
        Vector4 moved;
        VuTransformPoint(matrix, &point, &moved);
        placed[vertex] = {moved.x, moved.y};
    }
}
}

void Shape2D::Destroy(u32 flags)
{
    vtable = g_Shape2DVTable;
    ReleaseResource();
    if ((flags & FreeAfterDestroy) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Shape2D::SetMaterial(Material* newMaterial)
{
    CallVirtual<void>(this, vtable, Shape2D::ReleaseResourceSlot);
    material = newMaterial;
}

void Shape2D::SetMaterialById(u32 id)
{
    MaterialResource* old = resource;
    resource = g_MaterialTable.Acquire(&id, nullptr);
    material = resource->material;
    if (old != nullptr)
    {
        ReleaseMaterial(old);
    }
}

void Shape2D::ReleaseResource()
{
    if (resource != nullptr)
    {
        ReleaseMaterial(resource);
        resource = nullptr;
    }
}

Sprite* Sprite::Construct(Sprite* sprite)
{
    sprite->material = nullptr;
    sprite->vtable = g_SpriteVTable;
    sprite->resource = nullptr;
    sprite->turned = 0;
    return sprite;
}

void Sprite::Destroy(u32 flags)
{
    Shape2D::Destroy(flags);
}

void Sprite::SetMaterial(Material* newMaterial)
{
    texture = {0.0f, 0.0f, 1.0f, 1.0f};
    CallVirtual<void>(this, vtable, Shape2D::ReleaseResourceSlot);
    material = newMaterial;
}

void Sprite::SetMaterial(Material* newMaterial, const Vector2* start, const Vector2* end)
{
    texture = {start->x, start->y, end->x - start->x, end->y - start->y};
    CallVirtual<void>(this, vtable, Shape2D::ReleaseResourceSlot);
    material = newMaterial;
}

void Sprite::SetMaterialById(u32 id)
{
    texture = {0.0f, 0.0f, 1.0f, 1.0f};
    Shape2D::SetMaterialById(id);
}

void Sprite::Draw()
{
    Draw(DefaultColour());
}

void Sprite::Draw(u32 colour)
{
    Platform::Graphics::Rectangle place = {SquareStart.x, SquareStart.y, SquareEnd.x - SquareStart.x, SquareEnd.y - SquareStart.y};
    Platform::Graphics::DrawSprite(material, colour, place, texture);
}

void Sprite::Draw(const Matrix4x4* matrix)
{
    u32 colour = DefaultColour();
    CallVirtual<void>(this, vtable, Shape2D::DrawPlacedColouredSlot, matrix, colour);
}

void Sprite::Draw(const Matrix4x4* matrix, u32 colour)
{
    if (turned == 0)
    {
        Vector4 start;
        Vector4 end;
        VuTransformPoint(matrix, &SquareStart, &start);
        VuTransformPoint(matrix, &SquareEnd, &end);
        Platform::Graphics::Rectangle place = {start.x, start.y, end.x - start.x, end.y - start.y};
        Platform::Graphics::DrawSprite(material, colour, place, texture);
        return;
    }

    Vector2 corners[4];
    for (u32 corner = 0; corner < 4; corner++)
    {
        Vector4 placed;
        VuTransformPoint(matrix, &TurnedCorners[corner], &placed);
        corners[corner] = {placed.x, placed.y};
    }

    Platform::Graphics::DrawTurnedSprite(material, colour, corners, texture);
}

void Shape2D::Read(Stream* stream)
{
    if (resource != nullptr)
    {
        ReleaseMaterial(resource);
    }

    s32 textureId;
    s32 materialId;
    stream->ReadS32(&textureId);
    stream->ReadS32(&materialId);
    GameTexture* texture = TextureFromStream(static_cast<u32>(textureId), stream);
    resource = MaterialFromStream(static_cast<u32>(materialId), stream);
    CallVirtual<void>(this, vtable, Shape2D::SetMaterialByIdSlot, static_cast<u32>(materialId));
    ReleaseTexture(texture);
}

Strip* Strip::Construct(Strip* strip)
{
    strip->material = nullptr;
    strip->resource = nullptr;
    strip->turned = 0;
    strip->vtable = g_StripVTable;
    strip->count = 0;
    strip->flags.value = 0;
    strip->unused12[0] = 0;
    strip->unused12[1] = 0;
    return strip;
}

void Strip::Destroy(u32 flags)
{
    vtable = g_StripVTable;
    FreeArrays(this);
    places = nullptr;
    colours = nullptr;
    Shape2D::Destroy(flags);
}

void Strip::SetVertexes(u8 newCount, Vector2* newPlaces, u32* newColours)
{
    FreeArrays(this);
    colours = newColours;
    count = newCount;
    places = newPlaces;
}

void Strip::TintColours(u32 colour, u32* tinted)
{
    Rgba tint = {colour};
    f32 red = Colour::ColourFraction(tint.red);
    f32 green = Colour::ColourFraction(tint.green);
    f32 blue = Colour::ColourFraction(tint.blue);
    f32 alpha = Colour::AlphaFraction(tint.alpha);
    for (u32 vertex = 0; vertex < count; vertex++)
    {
        Rgba own = {colours[vertex]};
        auto* out = reinterpret_cast<Rgba*>(&tinted[vertex]);
        out->red = Colour::ColourByte(red * Colour::ColourFraction(own.red));
        out->green = Colour::ColourByte(green * Colour::ColourFraction(own.green));
        out->blue = Colour::ColourByte(blue * Colour::ColourFraction(own.blue));
        out->alpha = Colour::AlphaByte(alpha * Colour::AlphaFraction(own.alpha));
    }
}

void Strip::Draw()
{
    Platform::Graphics::DrawStrip(material, count, places, colours);
}

void Strip::Draw(u32 colour)
{
    u32 tinted[MostVertexes];
    TintColours(colour, tinted);
    Platform::Graphics::DrawStrip(material, count, places, tinted);
}

void Strip::Draw(const Matrix4x4* matrix)
{
    Vector2 placed[MostVertexes];
    PlaceVertexes(this, matrix, placed);
    Platform::Graphics::DrawStrip(material, count, placed, colours);
}

void Strip::Draw(const Matrix4x4* matrix, u32 colour)
{
    u32 tinted[MostVertexes];
    Vector2 placed[MostVertexes];
    TintColours(colour, tinted);
    PlaceVertexes(this, matrix, placed);
    Platform::Graphics::DrawStrip(material, count, placed, tinted);
}

Ring* Ring::Construct(Ring* ring, u32 points, Material* material)
{
    Strip::Construct(ring);
    ring->vtable = g_RingVTable;
    GetColor(&ring->middleColour, ColourWhite);
    GetColor(&ring->colour, ColourWhite);
    ring->innerScale = {0.0f, 0.0f};
    ring->innerColours = nullptr;
    ring->outerColours = nullptr;
    ring->innerShape = nullptr;
    ring->outerShape = nullptr;
    ring->outerScale = {1.0f, 1.0f};
    AngleFrom(&ring->rotation, 0.0f, AngleRadians);
    AngleFrom(&ring->startAngle, 0.0f, AngleRadians);
    AngleFrom(&ring->span, TwoPi, AngleRadians);
    ring->squareness = 0.0f;
    u32 vertexes = (points + 1) * PointVertexes;
    ring->vertexPlaces = static_cast<Vector2*>(MemoryAllocate2(vertexes * sizeof(Vector2)));
    ring->vertexColours = static_cast<u32*>(MemoryAllocate2(vertexes * sizeof(u32)));
    CallVirtual<void>(ring, ring->vtable, Shape2D::ReleaseResourceSlot);
    ring->material = material;
    ring->SetVertexes(static_cast<u8>(points * PointVertexes), ring->vertexPlaces, ring->vertexColours);
    return ring;
}

void Ring::Destroy(u32 flags)
{
    vtable = g_RingVTable;
    if (vertexColours != nullptr)
    {
        MemoryDeallocate_(vertexColours);
    }

    if (vertexPlaces != nullptr)
    {
        MemoryDeallocate_(vertexPlaces);
    }

    Strip::Destroy(flags);
}

void Ring::Update(TimeClock*, u32 segments, u32 steps, u32 tint, const Vector2* place, const Vector2* scale, const Ring* previous)
{
    Vector2 outer;
    CopyVector2(&outer, scale);
    outer.x = outer.x * outerScale.x;
    outer.y = outer.y * outerScale.y;
    u32 points = segments * (steps + 1);
    BuildColours(points, tint, 0);
    BuildEdge(place, &outer, segments, steps, 0);
    if (innerColours != nullptr)
    {
        BuildColours(points, tint, 1);
    }

    if (innerShape != nullptr)
    {
        Vector2 inner;
        CopyVector2(&inner, scale);
        inner.x = inner.x * innerScale.x;
        inner.y = inner.y * innerScale.y;
        BuildEdge(place, &inner, segments, steps, 1);
    }

    JoinInner(points, place, tint, previous);
}

void Ring::BuildColours(u32 points, u32 tint, u32 inner)
{
    u32* first = inner != 0 ? vertexColours + 1 : vertexColours;
    const Vector4Curve* curve = inner != 0 ? innerColours : outerColours;
    u32* out = first;
    if (curve == nullptr)
    {
        u32 tinted = colour;
        ColourTint(&tinted, tint);
        for (u32 point = 0; point < points; point++)
        {
            *out = tinted;
            out += PointVertexes;
        }
    }
    else
    {
        Rgba tinting = {tint};
        f32 red = Colour::ColourFraction(tinting.red);
        f32 green = Colour::ColourFraction(tinting.green);
        f32 blue = Colour::ColourFraction(tinting.blue);
        f32 alpha = Colour::AlphaFraction(tinting.alpha);
        f32 step = 1.0f / static_cast<f32>(static_cast<s32>(points));
        f32 t = 0.0f;
        for (u32 point = 0; point < points; point++)
        {
            Vector4 value = SampleColours(curve, t);
            t = t + step;
            auto* vertexColour = reinterpret_cast<Rgba*>(out);
            vertexColour->red = Colour::ColourByte(value.x * red);
            vertexColour->green = Colour::ColourByte(value.y * green);
            vertexColour->blue = Colour::ColourByte(value.z * blue);
            vertexColour->alpha = Colour::AlphaByte(value.w * alpha);
            out += PointVertexes;
        }
    }

    *out = *first;
}

void Ring::BuildEdge(const Vector2* place, const Vector2* scale, u32 segments, u32 steps, u32 inner)
{
    u32 points = segments * steps;
    f32 step = 1.0f / static_cast<f32>(static_cast<s32>(points));
    Vector2* first = inner != 0 ? vertexPlaces + 1 : vertexPlaces;
    const Vector2Curve* curve = inner != 0 ? innerShape : outerShape;
    // The ellipse's radii scaled, turned, then made pixels (squeezed on a 16:9 TV) about the place in pixels
    Vector2 pixels = {1.0f, 1.0f};
    FitSizeToScreen(1, &pixels);
    Matrix4x4 matrix;
    MatrixRotationZ(&matrix, &rotation);
    Vector4 radii = {scale->x, scale->y, 1.0f, 1.0f};
    MatrixScaleRows(&matrix, &radii);
    Vector4 toPixels = {pixels.x, pixels.y, 1.0f, 1.0f};
    MatrixScaleColumns(&matrix, &toPixels);
    matrix.m[3][0] = static_cast<f32>(g_RendererWidth) * place->x;
    matrix.m[3][1] = static_cast<f32>(g_RendererHeight) * place->y;
    matrix.m[3][2] = 0.0f;
    matrix.m[3][3] = 1.0f;
    Vector2* out = first;
    for (u32 segment = 0; segment < segments; segment++)
    {
        f32 t = step * static_cast<f32>(static_cast<s32>(steps * segment));
        s32 segmentStart = span / static_cast<s32>(segments) * static_cast<s32>(segment) + startAngle;
        for (u32 point = 0; point <= steps; point++)
        {
            s32 angle = span * static_cast<s32>(point) / static_cast<s32>(points) + segmentStart;
            f32 cosine;
            f32 sine;
            CosSin16(&angle, &cosine, &sine);
            if (curve != nullptr)
            {
                Vector2 shape;
                SampleVector2Curve(curve, t, &shape);
                cosine = cosine * shape.x;
                sine = sine * shape.y;
            }

            Vector4 vertex = Squared(cosine, sine, squareness);
            VuTransformPoint(&matrix, &vertex, &vertex);
            t = t + step;
            out->x = vertex.x;
            out->y = vertex.y;
            out += PointVertexes;
        }
    }

    *out = *first;
}

void Ring::JoinInner(u32 points, const Vector2* place, u32 tint, const Ring* previous)
{
    u32 vertexes = points * PointVertexes;
    if (previous != nullptr)
    {
        for (u32 vertex = 1; vertex < vertexes; vertex += PointVertexes)
        {
            vertexColours[vertex] = previous->vertexColours[vertex - 1];
            vertexPlaces[vertex] = previous->vertexPlaces[vertex - 1];
        }

        return;
    }

    f32 width = static_cast<f32>(g_RendererWidth);
    f32 height = static_cast<f32>(g_RendererHeight);
    u32 tinted = middleColour;
    Vector2 middle;
    CopyVector2(&middle, place);
    ColourTint(&tinted, tint);
    middle.x = middle.x * width;
    middle.y = middle.y * height;
    if (innerShape == nullptr)
    {
        for (u32 vertex = 1; vertex < vertexes; vertex += PointVertexes)
        {
            vertexPlaces[vertex] = middle;
        }
    }

    if (innerColours == nullptr)
    {
        for (u32 vertex = 1; vertex < vertexes; vertex += PointVertexes)
        {
            vertexColours[vertex] = tinted;
        }
    }
}

void Ring::PointAt(const Vector2* place, const Vector2* scale, Vector2* out, f32 t)
{
    f32 middleY = (innerScale.y + outerScale.y) * 0.5f;
    f32 middleX = (innerScale.x + outerScale.x) * 0.5f;
    Matrix4x4 matrix;
    MatrixRotationZ(&matrix, &rotation);
    Vector4 radii = {scale->x, scale->y, 1.0f, 1.0f};
    MatrixScaleRows(&matrix, &radii);
    matrix.m[3][0] = place->x;
    matrix.m[3][1] = place->y;
    matrix.m[3][2] = 0.0f;
    matrix.m[3][3] = 1.0f;
    s32 angle = static_cast<s32>(static_cast<f32>(span) * t) + startAngle;
    f32 cosine;
    f32 sine;
    CosSin16(&angle, &cosine, &sine);
    Vector4 point = {cosine * middleX, sine * -middleY, 0.0f, 1.0f};
    VuTransformPoint(&matrix, &point, &point);
    out->x = point.x;
    out->y = point.y;
}

EABI_EXPORT(FUN_001a8b78, &Ring::PointAt);

extern "C"
{
    // The retail copies of the sprites' corners, which the C++ draws with the constants above: made by GCC 2.9x's static
    // initialisation (when initialise is 1 and the priority 0xFFFF), and the static constructor that runs it
    extern Vector4 g_SquareStart RETAIL(D_00324040);
    extern Vector4 g_SquareEnd RETAIL(D_00324050);
    extern Vector4 g_TurnedCorners[4] RETAIL(D_00324060);
    void InitSpriteCorners(s32 initialise, s32 priority) RETAIL(FUN_001ab420);
    void SpritesStaticInit() RETAIL(FUN_001ad368);
}

void InitSpriteCorners(s32 initialise, s32 priority)
{
    if (priority != DefaultInitPriority || initialise == 0)
    {
        return;
    }

    g_SquareStart = SquareStart;
    g_SquareEnd = SquareEnd;
    for (u32 corner = 0; corner < 4; corner++)
    {
        g_TurnedCorners[corner] = TurnedCorners[corner];
    }
}

void SpritesStaticInit()
{
    InitSpriteCorners(1, DefaultInitPriority);
}
