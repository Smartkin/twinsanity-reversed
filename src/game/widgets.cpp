#include "game/widgets.h"

#include "game/bindings.h"
#include "game/colour.h"
#include "game/controllers.h"
#include "game/instances.h"
#include "game/language.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/menus.h"
#include "game/overlay.h"
#include "game/place.h"
#include "game/reference.h"
#include "game/renderer.h"
#include "game/shapes.h"
#include "game/widgeteffects.h"

extern "C"
{
    extern const GccVTableEntry g_PadInstanceMoverVTable[] RETAIL(D_00302858);
    extern const GccVTableEntry g_PadInstanceMoverBaseVTable[] RETAIL(D_00302878);
    // The widgets' statics made (the defaults, the resume page), and the static constructor that runs it
    void InitWidgetStatics(s32 initialise, s32 priority) RETAIL(FUN_0025a018);
    void WidgetsStaticInit() RETAIL(FUN_0025d078);
    // The resume page's name
    extern const char g_ResumePageName[] RETAIL(D_00303238);
    // How far the widget is shown: 0 hidden, 1 shown, how far it appeared or is still to disappear (a drop shadow's share of its
    // offset)
    f32 ShownFraction(const Widget* widget) RETAIL(func_0025A900);
}

namespace
{
// The vtable's functions
constexpr u32 EnterHiddenSlot = 1;
constexpr u32 StartAppearingSlot = 2;
constexpr u32 EnterShownSlot = 3;
constexpr u32 StartDisappearingSlot = 4;
constexpr u32 WhileHiddenSlot = 5;
constexpr u32 WhileAppearingSlot = 6;
constexpr u32 WhileShownSlot = 7;
constexpr u32 WhileDisappearingSlot = 8;
constexpr u32 HideSlot = 10;
constexpr u32 Unknown11Slot = 11;
constexpr u32 UpdateSlot = 12;
constexpr u32 Unknown13Slot = 13;
constexpr u32 DrawSlot = 14;
// A shape's destructor
constexpr u32 ShapeDestroySlot = 1;

// Texts' alignment: centred both ways
constexpr u32 CentredText = 0x22;
constexpr s32 DefaultColourIndex = 0xF;

// The overlay layer a widget starts in
constexpr u32 DefaultLayer = 3;
// A tiled picture's rows and columns
constexpr u32 TileRows = 2;
constexpr u32 TileColumns = 4;

// How far into the appearing or disappearing the widget is: the time's fraction of the duration, cut down to whole clock units
// as if it were a time
// A sprite widget's sines: of half a turn (the pulse) and of a turn (the bob and the wobble) of a time's fraction of its period,
// the wobble's radians made an angle (65536ths of a turn), and the drop shadow's least offset
constexpr f32 HalfTurnRadians = 0x1.921FB6p+1f;
constexpr f32 TurnRadians = 0x1.921FB6p+2f;
constexpr f32 RadiansToAngle = 0x1.45F306p+13f;
constexpr f32 ShadowEpsilon = 0x1.A36E2Ep-15f;
// The emitter's and the effect's draw placed by the widget's turn and middle, the effect's step and its draw with the size
constexpr u32 EffectStepSlot = 2;
constexpr u32 EffectDrawSizedSlot = 3;
constexpr u32 PlacedDrawSlot = 4;

// The state a widget is in once what it was asked for takes effect
u32 StateToBe(const Widget* widget)
{
    u32 flags = widget->flags;
    if ((flags & Widget::StateAsked) != 0)
    {
        flags = flags >> Widget::NextStateShift;
    }

    return flags & Widget::StateMask;
}

// A widget's duration made the time it has been appearing or disappearing for, to turn back over
void TurnBack(Widget* widget, s32 duration)
{
    widget->duration = duration;
    f32 seconds = ClockUnitsToSeconds(&widget->duration);
    widget->duration = static_cast<s32>(seconds * widget->progress * g_ClockUnitsPerSecond);
}

// A rectangle's width is its size over this (4:3 as the game has it)
constexpr f32 RectangleAspect = 0x1.553F7Cp+0f;

// The colour faded out
u32 Transparent(u32 colour)
{
    return colour & 0x00FFFFFF;
}

// A rectangle's corners round its middle
void RectangleCorners(const Vector2* middle, const Vector2* size, Vector2* start, Vector2* end)
{
    Vector2 half;
    CopyVector2(&half, size);
    CopyVector2(start, middle);
    CopyVector2(end, middle);
    half.x = half.x * 0.5f;
    half.y = half.y * 0.5f;
    half.x = half.x / RectangleAspect;
    start->x = start->x - half.x;
    start->y = start->y - half.y;
    end->x = end->x + half.x;
    end->y = end->y + half.y;
}

bool HasShadow(const Widget* widget)
{
    return !(__builtin_fabsf(widget->shadowOffset.x) <= ShadowEpsilon && __builtin_fabsf(widget->shadowOffset.y) <= ShadowEpsilon);
}

f32 Sine(f32 radians)
{
    f32 sinCos[2];
    SinCosRadians(radians, sinCos);
    return sinCos[0];
}

// A sine's time moved on by the seconds, looping over the period while it's above 0
void Advance(f32& time, f32 period, f32 seconds)
{
    if (0.0f < period)
    {
        time = time + seconds;
        if (period <= time)
        {
            time = time - period;
        }
    }
}

// The matrix turned by a sprite widget's wobble (the turn first)
void Wobble(const SpriteWidget* widget, Matrix4x4* matrix)
{
    f32 radians = Sine(widget->wobbleTime / widget->wobblePeriod * TurnRadians) * widget->wobbleAmount + widget->wobbleBase;
    s32 angle = static_cast<s32>(radians * RadiansToAngle);
    Matrix4x4 turn;
    MatrixRotationZ(&turn, &angle);
    VuMultiplyMatrices(&turn, matrix, matrix);
}

bool Wobbles(const SpriteWidget* widget)
{
    return 0.0f < widget->wobbleBase || 0.0f < widget->wobbleAmount;
}

Vector2 Middle(const Vector2& start, const Vector2& end)
{
    return {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
}

// An animated widget made with an anchor: fitted to the TV's shape, the class's vtable and defaults
void ConstructAnchored(AnimatedWidget* widget, f32 anchor, const GccVTableEntry* vtable, u32 colour, const Vector2* place,
                       const Vector2* scale)
{
    widget->progress = 0.0f;
    widget->anchor.x = anchor;
    widget->next = nullptr;
    widget->effect = nullptr;
    widget->scaler = nullptr;
    widget->anchor.y = 0.5f;
    widget->vtable = g_WidgetVTable;
    ColourSet(&widget->shadowColour, widget->progress, widget->progress, widget->progress, 0.5f);
    widget->shadowOffset.x = 0.0f;
    widget->shadowOffset.y = 0.0f;
    widget->flags = 0;
    widget->flags = (((widget->flags | Widget::Widescreen) & ~Widget::LayerMask) | DefaultLayer << Widget::LayerShift) &
                    ~Widget::StateMask;
    widget->vtable = vtable;
    widget->start = 0;
    widget->duration = 0;
    widget->hold = 0;
    widget->hiddenColour = colour;
    widget->shownColour = colour;
    CopyVector2(&widget->hiddenPlace, place);
    CopyVector2(&widget->shownPlace, place);
    CopyVector2(&widget->hiddenScale, scale);
    CopyVector2(&widget->shownScale, scale);
}

// A sprite widget's own values
void ConstructSprite(SpriteWidget* widget, Sprite* sprite)
{
    widget->sprite = sprite;
    widget->vtable = g_SpriteWidgetVTable;
    widget->wobblePeriod = -1.0f;
    widget->emitter = nullptr;
    widget->pulseTime = 0.0f;
    widget->pulsePeriod = -1.0f;
    widget->pulseAmount = 0.0f;
    widget->bobTime = 0.0f;
    widget->bobPeriod = -1.0f;
    widget->bobBase = 0.0f;
    widget->bobAmount = 0.0f;
    widget->wobbleTime = 0.0f;
    widget->wobbleAmount = 0.0f;
}

f32 Progress(s32 elapsed, s32 duration)
{
    f32 seconds = static_cast<f32>(duration) * g_SecondsPerClockUnit;
    f32 fraction = static_cast<f32>(elapsed) * g_SecondsPerClockUnit / seconds;
    return static_cast<f32>(static_cast<s32>(fraction * g_ClockUnitsPerSecond)) * g_SecondsPerClockUnit;
}

Vector2 Lerp(const Vector2& from, const Vector2& to, f32 t)
{
    return {to.x * t + from.x * (1.0f - t), to.y * t + from.y * (1.0f - t)};
}

// The instance turned about one of the world's axes by an angle (not by none): whether it turned
template <void (*MakeTurn)(Vector4*, const s32*)>
bool TurnAboutWorld(InstanceContext* instance, s32 angle)
{
    ObjectPlace* place = instance->place;
    if (angle == 0)
    {
        return false;
    }

    place->SyncRotation();
    place->bits = (place->bits | ObjectPlace::BitTurned) & ~u64{ObjectPlace::BitMatrixTurned};
    Vector4 turn;
    MakeTurn(&turn, &angle);
    MultiplyRotations(&turn, &turn, &place->rotation);
    place->rotation = turn;
    return true;
}
}

f32 ShownFraction(const Widget* widget)
{
    switch (widget->State())
    {
    case Widget::StateAppearing:
        return widget->progress;
    case Widget::StateShown:
        return 1.0f;
    case Widget::StateDisappearing:
        return 1.0f - widget->progress;
    default:
        return 0.0f;
    }
}

void Widget::Destroy(u32 destroyFlags)
{
    vtable = g_WidgetVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void Widget::Hide()
{
    duration = 0;
    start = 0;
    hold = 0;
    Ask(StateHidden);
    progress = 0.0f;
}

void Widget::Unknown11()
{
    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, Unknown11Slot);
    }
}

void Widget::Update(TimeClock* clock)
{
    s32 now = static_cast<s32>(clock->time);
    if ((flags & StateAsked) != 0)
    {
        u32 state = (flags & NextStateMask) >> NextStateShift;
        flags = ((flags & ~StateMask) | state) & ~StateAsked;
        start = now;
        progress = 0.0f;
        switch (state)
        {
        case StateHidden:
            CallVirtual<void>(this, vtable, EnterHiddenSlot);
            break;
        case StateAppearing:
            CallVirtual<void>(this, vtable, StartAppearingSlot);
            break;
        case StateShown:
            CallVirtual<void>(this, vtable, EnterShownSlot);
            break;
        case StateDisappearing:
            CallVirtual<void>(this, vtable, StartDisappearingSlot);
            break;
        default:
            break;
        }
    }
    else
    {
        s32 elapsed = now - start;
        switch (State())
        {
        case StateHidden:
            if (hold != 0 && elapsed >= hold)
            {
                hold = 0;
                Ask(StateAppearing);
            }

            progress = 0.0f;
            CallVirtual<void>(this, vtable, WhileHiddenSlot, elapsed);
            break;
        case StateAppearing:
            if (elapsed < duration)
            {
                progress = Progress(elapsed, duration);
            }
            else
            {
                progress = 1.0f;
                Ask(StateShown);
            }

            CallVirtual<void>(this, vtable, WhileAppearingSlot, elapsed);
            break;
        case StateShown:
            if (hold != 0 && elapsed >= hold)
            {
                hold = 0;
                Ask(StateDisappearing);
            }

            progress = 0.0f;
            CallVirtual<void>(this, vtable, WhileShownSlot, elapsed);
            break;
        case StateDisappearing:
            if (elapsed < duration)
            {
                progress = Progress(elapsed, duration);
            }
            else
            {
                progress = 1.0f;
                Ask(StateHidden);
            }

            CallVirtual<void>(this, vtable, WhileDisappearingSlot, elapsed);
            break;
        default:
            break;
        }
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, UpdateSlot, clock);
    }
}

void Widget::Unknown13()
{
    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, Unknown13Slot);
    }
}

void Widget::Draw(Renderer* renderer)
{
    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

void Widget::FitPlace(u32 inPixels, Vector2* place)
{
    if ((flags & Widescreen) != 0)
    {
        FitPlaceToScreen(inPixels, &anchor, place);
    }
}

void Widget::FitSize(u32 inPixels, Vector2* size)
{
    if ((flags & Widescreen) != 0)
    {
        FitSizeToScreen(inPixels, size);
    }

    if (scaler != nullptr)
    {
        size->x = size->x * scaler->scale;
        size->y = size->y * scaler->scale;
    }
}

Widget* Widget::Construct(Widget* widget)
{
    widget->progress = 0.0f;
    widget->next = nullptr;
    widget->effect = nullptr;
    widget->scaler = nullptr;
    widget->anchor.x = 0.5f;
    widget->anchor.y = 0.5f;
    widget->vtable = g_WidgetVTable;
    ColourSet(&widget->shadowColour, widget->progress, widget->progress, widget->progress, 0.5f);
    widget->shadowOffset.x = 0.0f;
    widget->shadowOffset.y = 0.0f;
    widget->flags = 0;
    widget->flags = ((widget->flags & ~LayerMask) | DefaultLayer << LayerShift) & ~StateMask;
    widget->start = 0;
    widget->duration = 0;
    widget->hold = 0;
    return widget;
}

AnimatedWidget* AnimatedWidget::Construct(AnimatedWidget* widget, f32 anchor)
{
    ConstructAnchored(widget, anchor, g_AnimatedWidgetVTable, g_WidgetColour, &g_WidgetPlace, &g_WidgetScale);
    return widget;
}

void AnimatedWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void AnimatedWidget::EnterHidden()
{
    colour = hiddenColour;
    place = hiddenPlace;
    scale = hiddenScale;
}

void AnimatedWidget::StartAppearing()
{
    colourFrom = colour;
    placeFrom = place;
    scaleFrom = scale;
}

void AnimatedWidget::EnterShown()
{
    colour = shownColour;
    place = shownPlace;
    scale = shownScale;
}

void AnimatedWidget::StartDisappearing()
{
    colourFrom = colour;
    placeFrom = place;
    scaleFrom = scale;
}

void AnimatedWidget::WhileHidden(s32)
{
    EnterHidden();
}

void AnimatedWidget::WhileAppearing(s32)
{
    ColourLerp(progress, &colour, colourFrom, shownColour);
    f32 t = progress;
    place = Lerp(placeFrom, shownPlace, t);
    scale = Lerp(scaleFrom, shownScale, t);
}

void AnimatedWidget::WhileShown(s32)
{
    EnterShown();
}

void AnimatedWidget::WhileDisappearing(s32)
{
    ColourLerp(progress, &colour, colourFrom, hiddenColour);
    f32 t = progress;
    place = Lerp(placeFrom, hiddenPlace, t);
    scale = Lerp(scaleFrom, hiddenScale, t);
}

ItemWidget* ItemWidget::Construct(ItemWidget* widget, f32 anchor, MenuPage* page, MenuItem* item)
{
    AnimatedWidget::Construct(widget, anchor);
    widget->page = page;
    widget->item = item;
    widget->vtable = g_ItemWidgetVTable;
    return widget;
}

void ItemWidget::Destroy(u32 destroyFlags)
{
    AnimatedWidget::Destroy(destroyFlags);
}

u32 ItemWidget::IsSelected(u32 player)
{
    return page->ItemAgain(page->selections[player]) == item ? 1 : 0;
}

void Widget::SetEffect(WidgetEffect* newEffect)
{
    effect = newEffect;
    if (next != nullptr)
    {
        next->SetEffect(newEffect);
    }
}

RectangleWidget* RectangleWidget::Construct(RectangleWidget* widget, f32 anchor)
{
    ConstructAnchored(widget, anchor, g_RectangleWidgetVTable, g_RectangleColour, &g_RectangleStart, &g_RectangleEnd);
    return widget;
}

RectangleWidget* RectangleWidget::Construct(RectangleWidget* widget)
{
    Widget::Construct(widget);
    widget->vtable = g_RectangleWidgetVTable;
    widget->hiddenColour = g_RectangleColour;
    widget->shownColour = g_RectangleColour;
    CopyVector2(&widget->hiddenPlace, &g_RectangleStart);
    CopyVector2(&widget->shownPlace, &g_RectangleStart);
    CopyVector2(&widget->hiddenScale, &g_RectangleEnd);
    CopyVector2(&widget->shownScale, &g_RectangleEnd);
    return widget;
}

void RectangleWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void RectangleWidget::EnterHidden()
{
    AnimatedWidget::EnterHidden();
}

void RectangleWidget::StartAppearing()
{
    AnimatedWidget::StartAppearing();
}

void RectangleWidget::EnterShown()
{
    AnimatedWidget::EnterShown();
}

void RectangleWidget::StartDisappearing()
{
    AnimatedWidget::StartDisappearing();
}

void RectangleWidget::WhileHidden(s32)
{
    AnimatedWidget::EnterHidden();
    if (effect != nullptr)
    {
        effect = nullptr;
    }
}

void RectangleWidget::WhileAppearing(s32 elapsed)
{
    AnimatedWidget::WhileAppearing(elapsed);
}

void RectangleWidget::WhileShown(s32)
{
    AnimatedWidget::EnterShown();
}

void RectangleWidget::WhileDisappearing(s32 elapsed)
{
    AnimatedWidget::WhileDisappearing(elapsed);
}

SpriteWidget* SpriteWidget::Construct(SpriteWidget* widget, f32 anchor, Sprite* sprite)
{
    RectangleWidget::Construct(widget, anchor);
    // The wobble's base isn't set here, only by the other constructor
    ConstructSprite(widget, sprite);
    return widget;
}

SpriteWidget* SpriteWidget::Construct(SpriteWidget* widget, Sprite* sprite)
{
    RectangleWidget::Construct(widget);
    ConstructSprite(widget, sprite);
    widget->wobbleBase = 0.0f;
    return widget;
}

void SpriteWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void SpriteWidget::Update(TimeClock* clock)
{
    Widget::Update(clock);
    if ((flags & Invisible) != 0 || State() < StateAppearing)
    {
        return;
    }

    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    Advance(pulseTime, pulsePeriod, seconds);
    Advance(bobTime, bobPeriod, seconds);
    Advance(wobbleTime, wobblePeriod, seconds);
    if (emitter == nullptr && effect == nullptr)
    {
        return;
    }

    Vector2 start;
    Vector2 end;
    CopyVector2(&start, &place);
    CopyVector2(&end, &scale);
    if (emitter != nullptr)
    {
        Emitter2DStep(emitter, seconds, &start, &end);
    }

    if (effect != nullptr)
    {
        u32 shown = State() == StateShown ? 1 : 0;
        effect = CallVirtual<WidgetEffect*>(effect, effect->vtable, EffectStepSlot, seconds, &start, &end, shown);
    }
}

void SpriteWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 layer = (flags & LayerMask) >> LayerShift;
        u32 drawColour = colour;
        Vector2 start;
        Vector2 end;
        Vector2 size;
        CopyVector2(&start, &place);
        CopyVector2(&end, &scale);
        CopyVector2(&size, &end);
        size.x = size.x - start.x;
        size.y = size.y - start.y;
        size.x = size.x / sprite->texture.width;
        size.y = size.y / sprite->texture.height;
        if ((flags & Widescreen) != 0)
        {
            FitPlaceToScreen(0, &anchor, &start);
            if ((flags & Widescreen) != 0)
            {
                FitPlaceToScreen(0, &anchor, &end);
                if ((flags & Widescreen) != 0)
                {
                    FitSizeToScreen(0, &size);
                }
            }
        }

        if (scaler != nullptr)
        {
            size.x = size.x * scaler->scale;
            size.y = size.y * scaler->scale;
        }

        f32 pulse = 1.0f;
        if (0.0f < pulsePeriod)
        {
            pulse = Sine(pulseTime / pulsePeriod * HalfTurnRadians) * pulseAmount + 1.0f;
        }

        size.x = size.x * pulse;
        size.y = size.y * pulse;
        // A turned sprite is placed by its middle
        if (sprite->turned != 0)
        {
            start = Middle(start, end);
        }

        f32 bob = 0.0f;
        if (0.0f < bobBase || 0.0f < bobAmount)
        {
            bob = bobBase + Sine(bobTime / bobPeriod * TurnRadians) * bobAmount;
        }

        start.y = start.y + bob;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        matrix.m[0][0] = size.x;
        matrix.m[1][1] = size.y;
        matrix.m[3][0] = start.x;
        matrix.m[3][1] = start.y;
        if (Wobbles(this))
        {
            Wobble(this, &matrix);
        }

        if (effect != nullptr)
        {
            CallVirtual<void>(effect, effect->vtable, EffectDrawSizedSlot, &matrix);
        }

        // The drop shadow: the colour tinted, moved by its offset as far as the widget has appeared, a layer lower
        if (HasShadow(this))
        {
            f32 shown = ShownFraction(this);
            u32 tinted = colour;
            Vector2 offset;
            CopyVector2(&offset, &shadowOffset);
            ColourTint(&tinted, shadowColour);
            offset.x = offset.x * shown;
            offset.y = offset.y * shown;
            if ((flags & Widescreen) != 0)
            {
                FitSizeToScreen(0, &offset);
            }

            if (scaler != nullptr)
            {
                offset.x = offset.x * scaler->scale;
                offset.y = offset.y * scaler->scale;
            }

            matrix.m[3][0] = matrix.m[3][0] + offset.x;
            matrix.m[3][1] = matrix.m[3][1] + offset.y;
            renderer->colour = tinted;
            QueuePlacedShape(renderer, &matrix, sprite, layer - 1);
            matrix.m[3][0] = matrix.m[3][0] - offset.x;
            matrix.m[3][1] = matrix.m[3][1] - offset.y;
        }

        renderer->colour = drawColour;
        QueuePlacedShape(renderer, &matrix, sprite, layer);
        if (emitter != nullptr || effect != nullptr)
        {
            if (sprite->turned == 0)
            {
                start = Middle(start, end);
            }

            Matrix4x4 turn;
            InitIdentityMatrix(&turn);
            if (Wobbles(this))
            {
                Wobble(this, &turn);
            }

            turn.m[3][0] = start.x;
            turn.m[3][1] = start.y;
            if (emitter != nullptr)
            {
                CallVirtual<void>(emitter, emitter->vtable, PlacedDrawSlot, &turn);
            }

            if (effect != nullptr)
            {
                CallVirtual<void>(effect, effect->vtable, PlacedDrawSlot, &turn);
            }
        }
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

// The current page's vtable functions widget slots 11 and 13 pass on to (with 0)
constexpr u32 PageUnknown6Slot = 6;
constexpr u32 PageUnknown8Slot = 8;
// The input's poll
constexpr u32 InputPollSlot = 2;

MenuWidget* MenuWidget::Construct(MenuWidget* widget, f32 anchor, MenuInput* input, MenuDrawer* drawer, MenuSounds* sounds)
{
    AnimatedWidget::Construct(widget, anchor);
    widget->input = input;
    widget->vtable = g_MenuWidgetVTable;
    widget->drawer = drawer;
    widget->sounds = sounds;
    widget->firstPage = nullptr;
    widget->home = nullptr;
    widget->current = nullptr;
    widget->shown = nullptr;
    widget->menuFlags = 0;
    widget->Hide();
    return widget;
}

void MenuWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void MenuWidget::Unknown11()
{
    if (current != nullptr)
    {
        CallVirtual<void>(current, current->vtable, PageUnknown6Slot, 0u);
    }

    Widget::Unknown11();
}

void MenuWidget::Unknown13()
{
    if (current != nullptr)
    {
        CallVirtual<void>(current, current->vtable, PageUnknown8Slot, 0u);
    }

    Widget::Unknown13();
}

// The page left: remembered as the start when the widget does (but the resume page)
static void LeavePage(MenuWidget* widget, MenuPage* page)
{
    page->Leave();
    if ((widget->menuFlags & MenuWidget::RemembersPage) != 0 && widget->current != &g_ResumePage)
    {
        widget->home = widget->current;
    }
}

void MenuWidget::Update(TimeClock* clock)
{
    Widget::Update(clock);
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        if (State() == StateShown)
        {
            CallVirtual<void>(input, input->vtable, InputPollSlot, pad, menuFlags & Leaves);
        }
        else
        {
            input->Clear();
        }

        if (current == nullptr)
        {
            current = home;
            home->Enter(nullptr);
        }

        MenuPage* next = current->Step(input, sounds, 0);
        if (next != nullptr)
        {
            MenuPage* left = current;
            LeavePage(this, left);
            current = next;
            next->Enter(left);
        }
    }
    else if (current != nullptr)
    {
        LeavePage(this, current);
        current = nullptr;
    }

    u32 state = State();
    if (state == StateAppearing || state == StateShown)
    {
        bool leaving = (menuFlags & Leaves) != 0 && current == &g_ResumePage;
        if (!leaving)
        {
            shown = current;
            return;
        }
    }

    if (state < StateAppearing)
    {
        shown = nullptr;
    }
}

void MenuWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && shown != nullptr)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        FitPlace(0, &at);
        FitSize(0, &size);
        DrawMenuPage(drawer, shown, 0, drawColour, &at, &size, renderer);
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

StringLabel* StringLabel::Construct(StringLabel* label, f32 anchor, Font* font, u32 textFlags)
{
    AnimatedWidget::Construct(label, anchor);
    label->font = font;
    label->textFlags = textFlags;
    label->vtable = g_StringLabelVTable;
    label->text.string = nullptr;
    label->text.capacity = 0;
    label->text.length = 0;
    return label;
}

void StringLabel::Destroy(u32 destroyFlags)
{
    StringDestroy(&text);
    Widget::Destroy(destroyFlags);
}

void StringLabel::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        FitPlace(0, &at);
        FitSize(0, &size);
        renderer->font = font;
        renderer->textScale.x = size.x;
        renderer->textFlags = textFlags;
        renderer->textScale.y = size.y;
        renderer->colour = drawColour;
        QueueText(renderer, text.string, at.x, at.y);
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

extern "C"
{
    TextLine* TextLineConstruct(TextLine* line, Font* font)
    {
        line->font = font;
        line->flags = CentredText;
        line->text.string = nullptr;
        line->text.capacity = 0;
        line->text.length = 0;
        GetColor(&line->colour, DefaultColourIndex);
        line->place.y = 0.5f;
        line->scale.y = 1.0f;
        line->place.x = 0.5f;
        line->scale.x = 1.0f;
        line->label = nullptr;
        line->duration = 0;
        line->start = 0;
        return line;
    }

    void TextLineDraw(TextLine* line, Renderer* renderer)
    {
        if (line->text.length == 0 || line->label != nullptr)
        {
            return;
        }

        renderer->colour = line->colour;
        renderer->textScale.x = line->scale.x;
        renderer->textScale.y = line->scale.y;
        renderer->textFlags = line->flags;
        renderer->font = line->font;
        QueueText(renderer, line->text.string, line->place.x, line->place.y);
    }

    void TextLineUpdate(TextLine* line, TimeClock* clock)
    {
        if (line->text.length != 0)
        {
            if (line->start == 0)
            {
                line->start = clock->time;
            }
            else if (static_cast<s32>(clock->time - static_cast<u32>(line->start)) >= line->duration)
            {
                line->duration = 0;
                line->start = 0;
            }
        }

        if (line->label != nullptr)
        {
            StringAssign(&line->label->text, line->text.string);
        }
    }
}

Label* Label::Construct(Label* label, f32 anchor, Font* font, u32 text, u32 textFlags)
{
    AnimatedWidget::Construct(label, anchor);
    label->font = font;
    label->textFlags = textFlags;
    label->text = text;
    label->vtable = g_LabelVTable;
    return label;
}

void Label::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void Label::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        const char* string = GameText(text);
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        FitPlace(0, &at);
        FitSize(0, &size);

        renderer->font = font;
        renderer->colour = drawColour;
        renderer->textFlags = textFlags;
        renderer->textScale.x = size.x;
        renderer->textScale.y = size.y;
        QueueText(renderer, string, at.x, at.y);
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

TiledPicture* TiledPicture::Construct(TiledPicture* picture, f32 anchor, Sprite* tiles)
{
    AnimatedWidget::Construct(picture, anchor);
    picture->tiles = tiles;
    picture->vtable = g_TiledPictureVTable;
    return picture;
}

void TiledPicture::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void TiledPicture::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 layer = (flags & LayerMask) >> LayerShift;
        Sprite* tile = tiles;
        Vector2 corner;
        Vector2 size;
        CopyVector2(&corner, &place);
        CopyVector2(&size, &scale);
        FitPlace(0, &corner);
        FitSize(0, &size);
        // The place is the picture's middle: its corner and a tile's size
        f32 halfHeight = size.y * 0.5f;
        f32 halfWidth = size.x * 0.5f;
        corner.y = corner.y - halfHeight;
        size.y = halfHeight;
        size.x = size.x * 0.25f;
        corner.x = corner.x - halfWidth;
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        matrix.m[0][0] = size.x;
        matrix.m[1][1] = size.y;
        renderer->colour = colour;
        for (u32 row = 0; row < TileRows; row++)
        {
            for (u32 column = 0; column < TileColumns; column++)
            {
                matrix.m[3][0] = corner.x + size.x * static_cast<f32>(column);
                matrix.m[3][1] = corner.y + size.y * static_cast<f32>(row);
                QueuePlacedShape(renderer, &matrix, tile, layer);
                tile++;
            }
        }
    }

    Widget::Draw(renderer);
}

extern "C" f32 ClockUnitsToSeconds(const s32* units)
{
    return static_cast<f32>(*units) * g_SecondsPerClockUnit;
}

void Widget::Appear(s32 newDuration, s32 newHold)
{
    switch (StateToBe(this))
    {
    case 0:
        CallVirtual<void>(this, vtable, EnterHiddenSlot);
        [[fallthrough]];
    case StateHidden:
        duration = newDuration;
        hold = newHold;
        Ask(newDuration != 0 ? StateAppearing : StateShown);
        break;
    case StateAppearing:
        if (newDuration == 0)
        {
            Ask(StateShown);
        }

        break;
    case StateShown:
        duration = newDuration;
        hold = newHold;
        Ask(StateShown);
        break;
    case StateDisappearing:
        TurnBack(this, newDuration);
        hold = newHold;
        Ask(newDuration != 0 ? StateAppearing : StateShown);
        break;
    default:
        break;
    }

    if (next != nullptr)
    {
        next->Appear(newDuration, newHold);
    }
}

void Widget::Disappear(s32 newDuration, s32 newHold)
{
    switch (StateToBe(this))
    {
    case StateHidden:
        duration = newDuration;
        hold = newHold;
        Ask(StateHidden);
        break;
    case StateAppearing:
        TurnBack(this, newDuration);
        hold = newHold;
        Ask(newDuration != 0 ? StateDisappearing : StateHidden);
        break;
    case StateShown:
        duration = newDuration;
        hold = newHold;
        Ask(newDuration != 0 ? StateDisappearing : StateHidden);
        break;
    case StateDisappearing:
        if (newDuration == 0)
        {
            Ask(StateHidden);
        }

        break;
    default:
        break;
    }

    if (next != nullptr)
    {
        next->Disappear(newDuration, newHold);
    }
}

WidgetController* WidgetController::Construct(WidgetController* controller)
{
    controller->locked = 0;
    controller->vtable = g_WidgetControllerVTable;
    for (Widget*& widget : controller->widgets)
    {
        widget = nullptr;
    }

    for (u64& mask : controller->masks)
    {
        mask = 0;
    }

    return controller;
}

void WidgetController::Destroy(u32 flags)
{
    vtable = g_WidgetControllerVTable;
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void WidgetController::SetWidget(u32 index, Widget* widget)
{
    widgets[index] = widget;
}

void WidgetController::HideAll()
{
    for (Widget* widget : widgets)
    {
        if (widget != nullptr)
        {
            CallVirtual<void>(widget, widget->vtable, HideSlot);
        }
    }
}

void WidgetController::Unknown3()
{
    for (Widget* widget : widgets)
    {
        if (widget != nullptr)
        {
            CallVirtual<void>(widget, widget->vtable, Unknown11Slot);
        }
    }
}

void WidgetController::Update(TimeClock* clock)
{
    for (Widget* widget : widgets)
    {
        if (widget != nullptr)
        {
            CallVirtual<void>(widget, widget->vtable, UpdateSlot, clock);
        }
    }
}

void WidgetController::Draw(Renderer* renderer)
{
    for (Widget* widget : widgets)
    {
        if (widget != nullptr)
        {
            CallVirtual<void>(widget, widget->vtable, DrawSlot, renderer);
        }
    }
}

void WidgetController::Unknown6()
{
    for (Widget* widget : widgets)
    {
        if (widget != nullptr)
        {
            CallVirtual<void>(widget, widget->vtable, Unknown13Slot);
        }
    }
}

void WidgetController::Show(u64 bits, s32 duration, s32 hold)
{
    u64 picked = bits & ~locked;
    u64 bit = 1;
    for (Widget* widget : widgets)
    {
        if ((picked & bit) != 0 && widget != nullptr)
        {
            widget->Appear(duration, hold);
        }

        bit = bit << 1;
    }
}

void WidgetController::Hide(u64 bits, s32 duration, s32 hold)
{
    u64 picked = bits & ~locked;
    u64 bit = 1;
    for (Widget* widget : widgets)
    {
        if ((picked & bit) != 0 && widget != nullptr)
        {
            widget->Disappear(duration, hold);
        }

        bit = bit << 1;
    }
}

s32 WidgetController::IndexOf(u64 bits)
{
    s32 found = -1;
    u64 bit = 1;
    for (s32 index = 0; index < 64; index++)
    {
        if ((bits & bit) != 0)
        {
            found = index;
        }

        bit = bit << 1;
    }

    return found;
}

void WidgetController::SetEffect(u64 bit, WidgetEffect* effect)
{
    s32 index = IndexOf(bit);
    Widget* widget = index >= 0 ? widgets[index] : nullptr;
    widget->SetEffect(effect);
}

void AnimatedWidget::FadeIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale)
{
    hiddenColour = Transparent(newColour);
    shownColour = newColour;
    shownPlace = *newPlace;
    hiddenPlace = *newPlace;
    shownScale = *newScale;
    hiddenScale = *newScale;
}

void AnimatedWidget::SlideIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale, const Vector2* offset)
{
    Vector2 from;
    CopyVector2(&from, newPlace);
    from.x = from.x + offset->x;
    from.y = from.y + offset->y;
    hiddenColour = newColour;
    shownColour = newColour;
    shownPlace = *newPlace;
    hiddenPlace = from;
    shownScale = *newScale;
    hiddenScale = *newScale;
}

void AnimatedWidget::GrowIn(u32 newColour, const Vector2* newPlace, const Vector2* newScale, const Vector2* from)
{
    hiddenColour = Transparent(newColour);
    shownColour = newColour;
    shownPlace = *newPlace;
    hiddenPlace = *from;
    shownScale = *newScale;
    hiddenScale = {0.0f, 0.0f};
}

void AnimatedWidget::GrowRectangle(u32 newColour, const Vector2* middle, const Vector2* size, const Vector2* from)
{
    Vector2 start;
    Vector2 end;
    RectangleCorners(middle, size, &start, &end);
    hiddenColour = Transparent(newColour);
    shownColour = newColour;
    shownPlace = start;
    hiddenPlace = *from;
    shownScale = end;
    hiddenScale = *from;
}

void AnimatedWidget::FadeRectangle(u32 newColour, const Vector2* middle, const Vector2* size)
{
    Vector2 start;
    Vector2 end;
    RectangleCorners(middle, size, &start, &end);
    hiddenColour = Transparent(newColour);
    shownColour = newColour;
    shownPlace = start;
    hiddenPlace = start;
    shownScale = end;
    hiddenScale = end;
}

void AnimatedWidget::SlideRectangle(u32 newColour, const Vector2* middle, const Vector2* size, const Vector2* offset)
{
    Vector2 start;
    Vector2 end;
    RectangleCorners(middle, size, &start, &end);
    hiddenColour = newColour;
    shownColour = newColour;
    shownPlace = start;
    hiddenPlace = {start.x + offset->x, start.y + offset->y};
    shownScale = end;
    hiddenScale = {end.x + offset->x, end.y + offset->y};
}

RingWidget* RingWidget::Construct(RingWidget* widget, f32 anchor, u32 count, u32 segments, u32 steps, RingWidget* inside)
{
    AnimatedWidget::Construct(widget, anchor);
    widget->inside = inside;
    widget->vtable = g_RingWidgetVTable;
    widget->ringFlags = 0;
    widget->segments = static_cast<u8>(segments);
    widget->steps = static_cast<u8>(steps);
    widget->count = static_cast<u8>(count);
    widget->rings = static_cast<Ring**>(MemoryAllocate2(count * sizeof(Ring*)));
    for (u32 index = 0; index < count; index++)
    {
        widget->rings[index] = nullptr;
    }

    return widget;
}

void RingWidget::Destroy(u32 destroyFlags)
{
    vtable = g_RingWidgetVTable;
    for (u32 index = 0; index < count; index++)
    {
        Ring* ring = rings[index];
        if (ring != nullptr)
        {
            CallVirtual<void>(ring, ring->vtable, ShapeDestroySlot, 3u);
        }
    }

    if (rings != nullptr)
    {
        MemoryDeallocate_(rings);
    }

    Widget::Destroy(destroyFlags);
}

Ring* RingWidget::AddRing(u32 index, Material* material)
{
    u32 points = segments * (steps + 1);
    Ring** slot = &rings[index];
    *slot = Ring::Construct(static_cast<Ring*>(MemoryAllocate(sizeof(Ring))), points, material);
    return rings[index];
}

void RingWidget::PointAt(Vector2* out, f32 t)
{
    rings[0]->PointAt(&shownPlace, &shownScale, out, t);
}

void RingWidget::Update(TimeClock* clock)
{
    Widget::Update(clock);
    if ((flags & Invisible) != 0 || State() < StateAppearing)
    {
        return;
    }

    u32 tint = colour;
    Vector2 at;
    Vector2 size;
    CopyVector2(&at, &place);
    CopyVector2(&size, &scale);
    FitPlace(0, &at);
    if ((ringFlags & StandApart) != 0)
    {
        for (u32 index = 0; index < count; index++)
        {
            if (rings[index] != nullptr)
            {
                rings[index]->Update(clock, segments, steps, tint, &at, &size, nullptr);
            }
        }

        return;
    }

    const Ring* previous = nullptr;
    if (inside != nullptr)
    {
        for (u32 index = 0; index < inside->count; index++)
        {
            if (inside->rings[index] != nullptr)
            {
                previous = inside->rings[index];
            }
        }
    }

    for (u32 index = 0; index < count; index++)
    {
        Ring* ring = rings[index];
        if (ring != nullptr)
        {
            ring->Update(clock, segments, steps, tint, &at, &size, previous);
            previous = ring;
        }
    }
}

void RingWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 layer = (flags & LayerMask) >> LayerShift;
        bool shadow = HasShadow(this);
        u32 shadowTint = colour;
        // The shadow's offset in pixels, like the rings' places
        Matrix4x4 matrix;
        if (shadow)
        {
            f32 shown = ShownFraction(this);
            Vector2 offset;
            CopyVector2(&offset, &shadowOffset);
            ColourTint(&shadowTint, shadowColour);
            offset.x = offset.x * shown;
            offset.y = offset.y * shown;
            if ((flags & Widescreen) != 0)
            {
                FitSizeToScreen(1, &offset);
            }

            if (scaler != nullptr)
            {
                offset.x = offset.x * scaler->scale;
                offset.y = offset.y * scaler->scale;
            }

            InitIdentityMatrix(&matrix);
            matrix.m[3][0] = offset.x;
            matrix.m[3][1] = offset.y;
        }

        for (u32 index = 0; index < count; index++)
        {
            Ring* ring = rings[index];
            if (ring == nullptr)
            {
                continue;
            }

            if (shadow)
            {
                renderer->colour = shadowTint;
                QueuePlacedShape(renderer, &matrix, ring, layer - 1);
            }

            u32 own;
            GetColor(&own, DefaultColourIndex);
            renderer->colour = own;
            QueueShape(renderer, ring, layer);
        }
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

void ShapeWidget::Destroy(u32 destroyFlags)
{
    Widget::Destroy(destroyFlags);
}

void ShapeWidget::Draw(Renderer* renderer)
{
    if ((flags & Invisible) == 0 && State() >= StateAppearing)
    {
        u32 layer = (flags & LayerMask) >> LayerShift;
        u32 drawColour = colour;
        Vector2 at;
        Vector2 size;
        CopyVector2(&at, &place);
        CopyVector2(&size, &scale);
        FitPlace(0, &at);
        FitSize(0, &size);
        Matrix4x4 matrix;
        InitIdentityMatrix(&matrix);
        renderer->colour = drawColour;
        matrix.m[0][0] = size.x;
        matrix.m[1][1] = size.y;
        matrix.m[3][0] = at.x;
        matrix.m[3][1] = at.y;
        QueuePlacedShape(renderer, &matrix, shape, layer);
    }

    if (next != nullptr)
    {
        CallVirtual<void>(next, next->vtable, DrawSlot, renderer);
    }
}

CreditsRoll* CreditsRoll::Construct(CreditsRoll* roll, Font* font, const char* path)
{
    roll->font = font;
    MemoryStream::ConstructFromFile(&roll->text, path, true);
    roll->lines = nullptr;
    roll->speed = Rounded(0.1);
    roll->lineHeight = Rounded(0.05);
    roll->textSize = 0.75f;
    roll->top = 1.0f;
    roll->bits = 0;
    roll->speedScale = 1.0f;
    roll->SplitLines(reinterpret_cast<char*>(roll->text.begin));
    return roll;
}

void CreditsRoll::Destroy(u32 flags)
{
    if (lines != nullptr)
    {
        MemoryDeallocate_(lines);
    }

    lines = nullptr;
    text.Destroy(2);
    if ((flags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void CreditsRoll::SplitLines(char* string)
{
    bits &= ~(CountMask | CountMask << FirstShift);
    char* at = string;
    while (*at != '\0')
    {
        if ((at[0] == '\r' && at[1] == '\n') || (at[0] == '\n' && at[1] == '\r'))
        {
            at[0] = '\0';
            at[1] = '\0';
            at += 2;
        }
        else if (at[0] == '\n' || at[0] == '\r')
        {
            at[0] = '\0';
            at++;
        }
        else
        {
            at++;
            continue;
        }

        bits = (bits & ~CountMask) | (((bits & CountMask) + 1) & CountMask);
    }

    lines = static_cast<const char**>(MemoryAllocate2((bits & CountMask) * sizeof(const char*)));
    at = string;
    for (u32 line = 0; line < (bits & CountMask); line++)
    {
        lines[line] = at;
        while (*at++ != '\0')
        {
        }

        while (*at == '\0')
        {
            at++;
        }
    }
}

u32 CreditsRoll::Update(TimeClock* clock)
{
    f32 seconds = static_cast<f32>(static_cast<s32>(clock->advance)) * g_SecondsPerClockUnit;
    top -= seconds * (speed * speedScale);
    u32 first = bits >> FirstShift & CountMask;
    if (first < (bits & CountMask))
    {
        f32 below = top + lineHeight;
        if (below < 0.0f)
        {
            bits = (bits & ~(CountMask << FirstShift)) | ((first + 1) & CountMask) << FirstShift;
            top = below;
        }

        return 1;
    }

    return 0.0f < top ? 1 : 0;
}

void CreditsRoll::Draw(Renderer* renderer)
{
    renderer->textFlags = 3;
    u32 line = bits >> FirstShift & CountMask;
    f32 y = top;
    renderer->textScale.y = textSize;
    renderer->textScale.x = textSize;
    renderer->font = font;
    u32 colour;
    GetColor(&colour, 0xF);
    renderer->colour = colour;
    if (!(y < 1.0f))
    {
        return;
    }

    while (line < (bits & CountMask))
    {
        QueueText(renderer, lines[line], 0.5f, y);
        line++;
        y += lineHeight;
        if (!(y < 1.0f))
        {
            break;
        }
    }
}

EABI_EXPORT(FUN_00256120, AnimatedWidget::Construct);
EABI_EXPORT(FUN_0025b190, RingWidget::Construct);
EABI_EXPORT(FUN_0025aca0, ItemWidget::Construct);
EABI_EXPORT(FUN_0025b380, &RingWidget::PointAt);
EABI_EXPORT(FUN_002571b0, static_cast<RectangleWidget* (*)(RectangleWidget*, f32)>(RectangleWidget::Construct));
EABI_EXPORT(FUN_0025b4c0, static_cast<SpriteWidget* (*)(SpriteWidget*, f32, Sprite*)>(SpriteWidget::Construct));
EABI_EXPORT(FUN_0025a250, TiledPicture::Construct);
EABI_EXPORT(FUN_0025a9a0, Label::Construct);
EABI_EXPORT(FUN_0025b670, StringLabel::Construct);
EABI_EXPORT(FUN_0025ab38, MenuWidget::Construct);
EABI_EXPORT(FUN_00259420, &PadInstanceMover::Frame);
EABI_EXPORT(FUN_002592d8, &PadInstanceMover::MoveAlong);

void PadInstanceMover::Destroy(u32 destroyFlags)
{
    vtable = g_PadInstanceMoverVTable;
    BaseDestroy(destroyFlags);
}

void PadInstanceMover::BaseDestroy(u32 destroyFlags)
{
    // An instance's release (its slot 4), handed the flags the destructor got as the retail code leaves them
    constexpr u32 ReleaseSlot = 4;
    vtable = g_PadInstanceMoverBaseVTable;
    CallVirtual<u32>(instance, instance->vtable, ReleaseSlot, destroyFlags);
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(this);
    }
}

void WidgetsStaticInit()
{
    InitWidgetStatics(1, 0xFFFF);
}

void InitWidgetStatics(s32 initialise, s32 priority)
{
    constexpr s32 AllPriorities = 0xFFFF;
    constexpr u32 ResumePagePlayers = 1;
    if (priority != AllPriorities || initialise == 0)
    {
        return;
    }

    GetColor(&g_WidgetColour, DefaultColourIndex);
    g_WidgetPlace = {0.5f, 0.5f};
    g_WidgetScale = {1.0f, 1.0f};
    GetColor(&g_RectangleColour, DefaultColourIndex);
    g_RectangleStart = {0.0f, 0.0f};
    g_RectangleEnd = {1.0f, 1.0f};
    MenuPage::Construct(&g_ResumePage, g_ResumePageName, ResumePagePlayers);
}

void PadInstanceMover::Frame(f32 seconds, GamePad* pad)
{
    // The bindings' actions and axes it reads, and how far they move it a second
    constexpr u32 ForwardAction = 0;
    constexpr u32 BackAction = 1;
    constexpr u32 PitchAxis = 6;
    constexpr u32 YawAxis = 7;
    constexpr u32 SideAxis = 9;
    constexpr u32 UpAxis = 10;
    constexpr f32 AxisSpeed = 20.0f;
    constexpr f32 PressureSpeed = 40.0f;

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    f32 forward = bindings->Pressure(pad, ForwardAction);
    f32 along = (forward - bindings->Pressure(pad, BackAction)) * PressureSpeed;
    f32 side = bindings->AxisValue(pad, SideAxis) * AxisSpeed;
    f32 up = bindings->AxisValue(pad, UpAxis) * AxisSpeed;
    s32 pitch;
    AngleFrom(&pitch, bindings->AxisValue(pad, PitchAxis), AngleRadians);
    s32 yaw;
    AngleFrom(&yaw, bindings->AxisValue(pad, YawAxis), AngleRadians);
    Vector4 axis;
    if (side != 0.0f)
    {
        axis = *RowOf(&place->matrix, 0);
        MoveAlong(&axis, side, seconds);
    }

    if (up != 0.0f)
    {
        axis = *RowOf(&place->matrix, 1);
        MoveAlong(&axis, up, seconds);
    }

    if (along != 0.0f)
    {
        axis = *RowOf(&place->matrix, 2);
        MoveAlong(&axis, along, seconds);
    }

    s32 turn = pitch;
    s32 angle = *MultiplyAngle(&turn, seconds);
    InstanceContext* turned = instance;
    if (TurnAboutWorld<RotationFromPitch>(turned, angle))
    {
        QueueObject(turned);
    }

    turn = yaw;
    angle = *MultiplyAngle(&turn, seconds);
    turned = instance;
    if (TurnAboutWorld<RotationFromYaw>(turned, angle))
    {
        QueueObject(turned);
    }
}

void PadInstanceMover::MoveAlong(const Vector4* axis, f32 amount, f32 seconds)
{
    f32 scale = seconds * amount;
    Vector4 move = *axis;
    move.x = move.x * scale;
    move.y = move.y * scale;
    move.z = move.z * scale;
    InstanceContext* moved = instance;
    if (moved->place->MoveBy(&move))
    {
        QueueObject(moved);
    }
}
