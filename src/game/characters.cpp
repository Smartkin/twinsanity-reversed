#include "game/characters.h"

#include "game/agents.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/progress.h"
#include "game/properties.h"
#include "game/scripttokens.h"
#include "game/vehicles.h"

// The playable characters' code: their agents' controllers

namespace
{
}

u32 Gun::ShotWithin(s32 ticks)
{
    TimeClock* clock = GetContextClock(agent->instance);
    s32 elapsed = static_cast<s32>(clock->time - shotTime);
    return ticks < elapsed ? 0 : 1;
}

void CharacterAgent::Position(Vector4* position)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    *position = place->position;
    position->y = position->y + heightOffset;
}

void CharacterAgent::PlacePosition(Vector4* position)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    *position = place->position;
}

f32 CharacterAgent::HeightOffset()
{
    return heightOffset;
}

u32 CharacterAgent::Invincible()
{
    return static_cast<CharacterPart*>(part)->bits.invulnerable != 0;
}

u32 CharacterAgent::Linked()
{
    CharacterMoveBits moveBits = static_cast<CharacterPart*>(part)->moveBits;
    return moveBits.linkedFirst != 0 || moveBits.linkedSecond != 0;
}

u32 CharacterAgent::HandPoints(Vector4* own, Vector4* other)
{
    CharacterMoveBits moveBits = static_cast<CharacterPart*>(part)->moveBits;
    if (moveBits.linkedFirst != 0)
    {
        if (link->bits.hold != CharacterLink::HoldHands)
        {
            return 0;
        }

        *own = link->handPoint;
        *other = link->Second()->link->handPoint;
        return 1;
    }

    if (moveBits.linkedSecond != 0)
    {
        return link->Leader()->HandPoints(own, other);
    }

    return 0;
}

u32 CharacterAgent::Controlled()
{
    if (properties->GetInt(CharacterKindProperty) == CharacterNone)
    {
        return 1;
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, NodeControls));
    if (controls == nullptr)
    {
        return 0;
    }

    return controls->bits.motionDriven;
}

CharacterAgent* CharacterAgentOf(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, NodeCharacter));
    return node != nullptr ? static_cast<CharacterAgent*>(node->agent) : nullptr;
}

f32 CharacterAgent::WalkTopSpeed()
{
    if (walk == nullptr)
    {
        return 1.0f;
    }

    return walk->TopSpeed();
}

u32 CharacterAgent::MovingVelocity(Vector4* velocity)
{
    if (vehicle != nullptr)
    {
        vehicle->VelocityVirtual(velocity);
        return 1;
    }

    CallVirtual<u32>(this, vtable, VelocitySlot, velocity);
    return 0;
}

void CharacterAgent::DrawOverlay()
{
    if (vehicle != nullptr)
    {
        vehicle->DrawVirtual();
    }
}

u32 TokenCharacter(u32 token)
{
    switch (token)
    {
    case KeywordCrash:
        return CharacterCrash;
    case KeywordCortex:
        return CharacterCortex;
    case KeywordNina:
        return CharacterNina;
    case KeywordTallCrash:
        return CharacterTallCrash;
    default:
        return GameProgress::NoCharacter;
    }
}

void CharacterAgent::SplashPoint(Vector4* point, f32* radius)
{
    Position(point);
    *radius = CharacterSplashRadius;
    point->y = point->y + CharacterSplashRaise;
}

void CharacterAgent::SetFloorSurface(CollisionSurface* surface)
{
    floorSurface = surface;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, NodeObject));
    node->surface = surface != nullptr ? surface->surfaceId : ObjectNode::NoSurface;
    if (static_cast<CharacterPart*>(part)->moveBits.linkedFirst != 0)
    {
        link->Second()->SetFloorSurface(surface);
    }
}

u32 CharacterAgent::StandsOn(InstanceContext* other)
{
    u32 standing = state.standing;
    if (standing != StandingHull && standing != StandingRidden)
    {
        return 0;
    }

    ReferencedObject* object = standingOn != nullptr ? standingOn->object : nullptr;
    return object == other;
}

void CharacterAgent::SendSurfaceMessage(CollisionSurface* surface)
{
    if (surface == nullptr)
    {
        return;
    }

    CallVirtual<void>(this, vtable, ContactSlot, &surface->contact, instance, 1u);
}
