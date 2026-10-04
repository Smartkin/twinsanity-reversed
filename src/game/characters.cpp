#include "game/characters.h"

#include "game/agents.h"
#include "game/clock.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/objectnode.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/scripttokens.h"
#include "game/vehicles.h"

// The playable characters' code: their agents' controllers

namespace
{
// The nodes these functions use: the object node, the controls' node and the playable characters' agent node
constexpr u32 ObjectNodeKind = 1;
constexpr u32 ControlsNodeKind = 0xB;
constexpr u32 CharacterNodeKind = 0xC;
// The agents' vtable functions told contact messages and asked their velocity
constexpr u32 AgentContactSlot = 9;
constexpr u32 AgentVelocitySlot = 11;
}

u32 StartedWithin(CharacterController* controller, s32 ticks)
{
    TimeClock* clock = GetContextClock(controller->agent->instance);
    s32 elapsed = static_cast<s32>(clock->time - controller->startTime);
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
    return (static_cast<CharacterPart*>(part)->bits & CharacterPart::Invincible) != 0;
}

u32 CharacterAgent::Linked()
{
    u32 moveBits = static_cast<CharacterPart*>(part)->moveBits;
    return (moveBits & CharacterPart::LinkedFirst) != 0 || (moveBits & CharacterPart::LinkedSecond) != 0;
}

u32 CharacterAgent::HandPoints(Vector4* own, Vector4* other)
{
    u32 moveBits = static_cast<CharacterPart*>(part)->moveBits;
    if ((moveBits & CharacterPart::LinkedFirst) != 0)
    {
        if ((link->bits >> CharacterLink::HoldShift & CharacterLink::HoldMask) != CharacterLink::HoldHands)
        {
            return 0;
        }

        *own = link->handPoint;
        *other = link->Second()->link->handPoint;
        return 1;
    }

    if ((moveBits & CharacterPart::LinkedSecond) != 0)
    {
        return link->Leader()->HandPoints(own, other);
    }

    return 0;
}

u32 CharacterAgent::Controlled()
{
    constexpr s32 NoCharacter = 4;
    if (properties->GetInt(0) == NoCharacter)
    {
        return 1;
    }

    auto* controls = static_cast<ControlsNode*>(GetGameNode(&instance->nodes, ControlsNodeKind));
    if (controls == nullptr)
    {
        return 0;
    }

    return controls->bits & ControlsNode::BitMotionDriven;
}

CharacterAgent* CharacterAgentOf(InstanceContext* instance)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(&instance->nodes, CharacterNodeKind));
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

    CallVirtual<u32>(this, vtable, AgentVelocitySlot, velocity);
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
    constexpr u32 CrashToken = 0x232;
    constexpr u32 CortexToken = 0x233;
    constexpr u32 NinaToken = 0x234;
    constexpr u32 Character2Token = 0x235;
    constexpr u32 NoCharacter = 6;
    switch (token)
    {
    case CrashToken:
        return 0;
    case CortexToken:
        return 1;
    case NinaToken:
        return 3;
    case Character2Token:
        return 2;
    default:
        return NoCharacter;
    }
}

void CharacterAgent::SplashPoint(Vector4* point, f32* radius)
{
    constexpr f32 SplashRadius = Rounded(0.9);
    constexpr f32 SplashHeight = 1.0f;
    Position(point);
    *radius = SplashRadius;
    point->y = point->y + SplashHeight;
}

void CharacterAgent::SetFloorSurface(CollisionSurface* surface)
{
    constexpr s32 NoSurface = -1;
    floorSurface = surface;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
    node->surface = surface != nullptr ? surface->surfaceId : NoSurface;
    if ((static_cast<CharacterPart*>(part)->moveBits & CharacterPart::LinkedFirst) != 0)
    {
        link->Second()->SetFloorSurface(surface);
    }
}

u32 CharacterAgent::StandsOn(InstanceContext* other)
{
    u32 standing = state >> StandingShift & StandingMask;
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

    CallVirtual<void>(this, vtable, AgentContactSlot, &surface->contact, instance, 1u);
}
