#include "game/agents.h"

#include "game/animation.h"
#include "game/characters.h"
#include "game/collision.h"
#include "game/events.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/hull.h"
#include "game/instances.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectcollision.h"
#include "game/objectnode.h"
#include "game/physics.h"
#include "game/place.h"
#include "game/properties.h"
#include "game/reference.h"
#include "game/vehicles.h"

#include <cstddef>
#include <cstdint>

// The playable character's moves through the collision solver (alone and tied to the other character) and their parts: the
// probes keeping its limbs out of walls, riding a moving hull, what it stands on, the agents it mustn't be pushed out of, the
// touches and surfaces' messages, the crushes, Nina's wall clinging

EABI_EXPORT(FUN_00135438, &CharacterAgent::ChangeHeight);
EABI_EXPORT(FUN_001365e8, &CharacterAgent::CheckLinkedCrush);
EABI_EXPORT(FUN_00138b00, &CharacterAgent::Probe);
EABI_EXPORT(FUN_00139678, &CharacterAgent::Solve);
EABI_EXPORT(FUN_0013a470, &CharacterAgent::SolveLinked);

// An instance's attachments (its kind 6 node): how many instances are linked to it (bits 0-4) and the instances
struct AttachmentsNode
{
    u8 unknown00[0x18];
    u32 count;
    u8 unknown1C[4];
    InstanceContext* linked[16];
};

namespace
{
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;
constexpr f32 NoHit = Rounded(1e30);
// 65536ths of a turn in radians
constexpr f32 RadiansPerUnit = 0x1.921fb6p-14f;

// The kinds of the nodes it uses besides the model and the follow camera: the movement node, the object node, the attachments
constexpr u32 MovementNodeKind = 0;
constexpr u32 ObjectNodeKind = 1;
constexpr u32 AttachmentsKind = 6;
// The agents' functions told of a contact message and of an instance touched
constexpr u32 ContactSlot = 9;
constexpr u32 TouchedSlot = 19;

// Its first integer property: which character it is
constexpr u32 CharacterProperty = 0;
constexpr s32 Cortex = 1;
constexpr s32 Nina = 3;
// Its second float property: its gravity
constexpr u32 GravityProperty = 1;
// Crash, whom Cortex's solver leaves out (the game's progress keeps the characters' instances by the character)
constexpr u32 Crash = 0;

// The kinds of nodes (a bit each) whose instances the solver collides with: kind 12 (the playable characters') left out while
// it's thrown, kind 20 added for the two tied together
constexpr u32 SolidNodeKinds = 0x5B010;
constexpr u32 ThrownNodeKinds = 0x5A010;
constexpr u32 LinkedNodeKinds = 0x15B010;
// A crush: 4 alone, 2 tied, hurt from 8; its contact message (the kinds of hit and the damage)
constexpr s32 CrushStep = 4;
constexpr s32 LinkedCrushStep = 2;
constexpr s32 CrushedAt = 8;
constexpr u32 CrushHitKinds = 0xC00;
constexpr u8 CrushDamage = 100;

// The contact of the hull it rides, left out of the slide's end test (SlideStep's bit 3); the marks of contacts the move touched
// and of those it was pushed out of (or not, when it would have been)
constexpr u32 RiddenMark = 0x8;
constexpr u32 TouchMarks = Contact::Touched | Contact::StoodOn | Contact::Ground;
constexpr u32 PushMarks = Contact::PushedOut | Contact::PushRefused;
// The surfaces' bits: contact messages sent to the player (bit 9), soft ground (bit 11: footprints, skid marks, the walls Nina
// clings to), solid to the player's probes (bit 4)
constexpr u32 SendsMessageToPlayer = 0x200;
constexpr u32 SoftSurface = 0x800;
constexpr u32 SolidToProbes = 0x10;
// An instance's flags: its collision on (its agent's StateCollisionActive), it moves (what stands on it rides along)
constexpr u32 CollisionOnFlag = ReferencedObject::FlagSphereContact;
constexpr u32 MovingFlag = 0x4000;
// The part's bit 55: no ground ahead of it
constexpr u64 NoGroundAhead = u64{1} << 55;
// The jump's bits Nina's wall clinging clears (the part's bits 33, 34, 39 and 40)
constexpr u64 ClingClearedBits = u64{0x186} << 32;
// The water an object node is in (its surface ID), none
constexpr s32 NoWater = -1;
// The vehicle the wall clinging is
constexpr u32 WallClingKind = 7;

ReferencedObject* ObjectOf(const Reference* handle)
{
    return handle != nullptr ? handle->object : nullptr;
}

// An object's collision and the OGI its hulls are of, as the retail code reads them, also when there's no object (the words
// from 0x10 then)
ObjectCollision* CollisionOf(const ReferencedObject* object)
{
    return reinterpret_cast<ObjectCollision*>(reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, collision));
}

u32 HullStamp(const ReferencedObject* object)
{
    return reinterpret_cast<std::uintptr_t>(CollisionOf(object)->ogi);
}

// Whether an exit point's animation has a place (the word at 0x44 read when there's no animation)
bool IsPlaced(const ExitPointAnimation* exit)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(exit) + offsetof(ExitPointAnimation, place);
    return *reinterpret_cast<void* const*>(address) != nullptr;
}

u32 AttackKind(const AgentPart* part)
{
    return static_cast<u8>(static_cast<const BasicAgentPart*>(part)->bits);
}

// The attack kinds the solver looks at: body slams, spins, being thrown
bool IsSlam(u32 kind)
{
    return kind == 7 || kind == 11;
}

bool IsSpin(u32 kind)
{
    return kind == 6 || kind == 10;
}

bool IsThrown(u32 kind)
{
    return kind == 13 || kind == 14;
}

bool IsCrouching(const AgentPart* part)
{
    return (static_cast<const CharacterPart*>(part)->moveBits & CharacterPart::Crouching) != 0;
}

bool IsOnGround(const AgentPart* part)
{
    return (static_cast<const CreaturePart*>(part)->flags & CreaturePart::FlagOnGround) != 0;
}

u32 StandingOf(CharacterAgent* agent)
{
    return agent->state >> CharacterAgent::StandingShift & CharacterAgent::StandingMask;
}

void SetStanding(CharacterAgent* agent, u32 standing)
{
    constexpr u64 Mask = u64{CharacterAgent::StandingMask} << CharacterAgent::StandingShift;
    agent->StateBits() = (agent->StateBits() & ~Mask) | u64{standing} << CharacterAgent::StandingShift;
}

// The 0x34 bytes the game copies of a hit (the struct is padded to 0x40)
void CopyHit(CollisionHit* to, const CollisionHit* from)
{
    to->vertices[0] = from->vertices[0];
    to->vertices[1] = from->vertices[1];
    to->vertices[2] = from->vertices[2];
    to->surface = from->surface;
    to->unknown32 = from->unknown32;
}

bool SendsMessage(const CollisionSurface* surface)
{
    return (surface->collisionMask & SendsMessageToPlayer) != 0;
}

void TouchedVirtual(CharacterAgent* agent, InstanceContext* other, const Vector4* normal)
{
    CallVirtual<void>(agent, agent->vtable, TouchedSlot, other, normal);
}

f32 LengthSquared(const Vector4& vector)
{
    return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

// The game controller in the title or watching a cutscene (no touches then)
bool Watching()
{
    u32 state = G_GameController_00309914->states >> GameController::CurrentShift & GameController::StateMask;
    return state == GameController::StateWatching || state == GameController::StateTitle;
}

// The hull the solver moves: the crouched one while it crouches (when it can) or body slams
const CollisionHull* SolverHull(CharacterAgent* agent)
{
    AgentPart* part = agent->part;
    bool crouched = false;
    if (agent->crouch != nullptr && (agent->crouch->bits & CrouchController::BitCanCrouch) != 0)
    {
        crouched = IsCrouching(part);
    }

    if (!crouched && !IsSlam(AttackKind(part)))
    {
        return &agent->body->standingHull;
    }

    return &agent->body->crouchHull;
}

// The instance's place moved by a vector unless it's none (each coordinate within 5e-05), the instance queued when it moved
bool MoveInstanceBy(InstanceContext* instance, const Vector4* move)
{
    constexpr f32 Still = Rounded(5e-05);
    if (__builtin_fabsf(move->x) <= Still && __builtin_fabsf(move->y) <= Still && __builtin_fabsf(move->z) <= Still)
    {
        return false;
    }

    ObjectPlace* place = instance->place;
    place->SyncPosition();
    place->bits = (place->bits | ObjectPlace::BitMoved) & ~u64{ObjectPlace::BitMatrixMoved};
    place->position.x += move->x;
    place->position.y += move->y;
    place->position.z += move->z;
    QueueObject(instance);
    return true;
}

// The push out of what it went into capped at the body's radius (its squared length too), and the motion's part against it
// taken away
void CapPush(CharacterAgent* agent, Vector4* push, f32* pushSquared)
{
    f32 radius = IsCrouching(agent->Part()) ? agent->body->crouchRadius : agent->body->radius;
    f32 radiusSquared = radius * radius;
    if (radiusSquared < *pushSquared)
    {
        f32 inverse = InverseLength(push, LengthEpsilon);
        *pushSquared = radiusSquared;
        push->x = push->x * inverse * radius;
        push->y = push->y * inverse * radius;
        push->z = push->z * inverse * radius;
    }
}

void RemoveMotionAgainst(Vector4* motion, Vector4* away)
{
    f32 inverse = InverseLength(away, LengthEpsilon);
    away->x = away->x * inverse;
    away->y = away->y * inverse;
    away->z = away->z * inverse;
    f32 along = motion->x * away->x + motion->y * away->y + motion->z * away->z;
    if (along < 0.0f)
    {
        Vector4 back = {away->x * along, away->y * along, away->z * along, 1.0f};
        motion->x = motion->x - back.x;
        motion->y = motion->y - back.y;
        motion->z = motion->z - back.z;
    }
}

// The probe's push from a wall the ray from the character's middle (at the probe's height) to the probe point goes into: back
// along the wall's normal by how far the point is in, for walls (normals within 45 degrees of level)
void ProbeWall(CharacterAgent* agent, const Vector4* start, const Vector4* end, Vector4* hitPoint, CollisionHit* triangle,
               Vector4* push)
{
    constexpr f32 Wall = Rounded(0.707);
    if (TriangleListRayCast(&agent->cache, start, end, nullptr, hitPoint, triangle) == 0)
    {
        return;
    }

    Vector4 normal;
    TriangleNormal(triangle, &normal);
    normal.w = 1.0f;
    Vector4 ray = {end->x - start->x, end->y - start->y, end->z - start->z, 1.0f};
    if (ray.x * normal.x + ray.y * normal.y + ray.z * normal.z < 0.0f)
    {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
    }

    if (!(-Wall < normal.y) || !(normal.y < Wall))
    {
        return;
    }

    Vector4 in = {hitPoint->x - end->x, hitPoint->y - end->y, hitPoint->z - end->z, 1.0f};
    f32 depth = -__builtin_sqrtf(LengthSquared(in));
    push->x = normal.x * depth;
    push->y = normal.y * depth;
    push->z = normal.z * depth;
    push->w = 1.0f;
}

// The probe's push from an instance the ray hits (not one attached to it or the body it pushes, and one that stops the move it
// attacks with): the instance told, kept with the part's smoothed push
void ProbeInstance(CharacterAgent* agent, const Vector4* start, const Vector4* end, Vector4* hitPoint, CollisionHit* triangle,
                   Vector4* push)
{
    constexpr f32 Wall = Rounded(0.707);
    constexpr u16 MostFound = 20;
    void* found[MostFound];
    InstanceRayHit query;
    query.results = found;
    query.count = 0;
    query.most = MostFound;
    query.distance = NoHit;
    query.bits = InstanceRayHit::BitAllWanted;
    query.wantedFlags = CollisionOnFlag;
    query.unwantedFlags = ReferencedObject::FlagAsleep;
    query.skipped[0] = nullptr;
    query.skipped[1] = nullptr;
    query.instance = nullptr;
    // The cast fills the face it hit through its last argument
    if (SegmentHitsInstances(agent->instance->chunk, start, end, &query, ThrownNodeKinds, nullptr, hitPoint,
                             reinterpret_cast<std::uintptr_t>(triangle)) == 0)
    {
        return;
    }

    auto* hit = static_cast<InstanceContext*>(query.instance);
    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&agent->instance->nodes, AttachmentsKind));
    bool skipped = hit == ObjectOf(agent->pushedBody);
    if (attachments != nullptr)
    {
        u32 count = attachments->count & 0x1F;
        for (u32 index = 0; index < count; index++)
        {
            if (attachments->linked[index] == hit)
            {
                skipped = true;
            }
        }
    }

    if (skipped)
    {
        return;
    }

    u32 mode = agent->MoveModeOf();
    AgentNode* node = AgentNodeOf(hit);
    if (mode != CharacterAgent::MoveNone && node != nullptr && (node->agent->properties->state & 1u << mode) == 0)
    {
        return;
    }

    Vector4 normal;
    TriangleNormal(triangle, &normal);
    normal.w = 1.0f;
    if (!(-Wall < normal.y) || !(normal.y < Wall))
    {
        return;
    }

    Vector4 in = {hitPoint->x - end->x, hitPoint->y - end->y, hitPoint->z - end->z, 1.0f};
    f32 depth = __builtin_sqrtf(LengthSquared(in));
    push->x += normal.x * depth;
    push->y += normal.y * depth;
    push->z += normal.z * depth;
    TouchedVirtual(agent, hit, &g_DefaultBox.min);
    AssignReference(&agent->probedInstance, hit);
    agent->probePush = *static_cast<CharacterPart*>(agent->part)->SmoothPush();
}
}

u32 CharacterAgent::PlaceAt(const Vector4* position)
{
    Vector4 at = *position;
    at.y -= heightOffset;
    InstanceContext* self = instance;
    ObjectPlace* place = self->place;
    place->SyncPosition();
    if (!place->MoveTo(&at))
    {
        return 0;
    }

    QueueObject(self);
    return 1;
}

u32 CharacterAgent::ChangeHeight(f32 change)
{
    f32 height = heightOffset + change;
    Vector4 move = {0.0f, heightOffset - height, 0.0f, 1.0f};
    u32 moved = MoveInstanceBy(instance, &move);
    heightOffset = height;
    return moved;
}

void CharacterAgent::MarkUnpushable(ContactSet* contacts, u32 mode)
{
    if (mode == MoveNone)
    {
        return;
    }

    u32 modeBit = 1u << mode;
    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        ::Contact* contact = &contacts->solid.contacts[index];
        if ((contact->kind & ::Contact::KindHull) == 0)
        {
            continue;
        }

        InstanceContext* other = contact->instance;
        AgentNode* node = AgentNodeOf(other);
        if (node == nullptr)
        {
            continue;
        }

        // An agent that stops the move is pushed out of, unless it's a character being thrown
        if ((node->agent->properties->state & modeBit) != 0)
        {
            CharacterAgent* character = CharacterAgentOf(other);
            if (character == nullptr)
            {
                continue;
            }

            if (!IsThrown(AttackKind(character->part)))
            {
                continue;
            }
        }

        contact->kind |= ::Contact::NoPush;
    }
}

u32 CharacterAgent::RideMove(Vector4* move, f32* turn)
{
    if (StandingOf(this) != StandingRidden)
    {
        rideTurn = 0.0f;
        return 0;
    }

    ReferencedObject* ridden = ObjectOf(standingOn);
    if (standingStamp != HullStamp(ridden))
    {
        return 0;
    }

    CollisionHull* hull;
    Matrix4x4 matrix;
    GetInstanceHull(CollisionOf(ridden), standingHull, &hull, &matrix);
    Vector4 point;
    VuTransformPoint(&matrix, &ridePoint, &point);
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 position = place->position;
    position.y += heightOffset;
    Vector4 moved = {point.x - position.x, point.y - position.y, point.z - position.z, 1.0f};
    *move = moved;
    Matrix4x4 turned;
    VuMultiplyMatrices(&rideMatrix, &matrix, &turned);
    s32 angle;
    AngleOfSine(turned.m[0][2], &angle);
    *turn = static_cast<f32>(-angle) * RadiansPerUnit;
    rideTurn = *turn;
    return 1;
}

void CharacterAgent::TellTouched(ContactSet* contacts)
{
    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        ::Contact* contact = &contacts->solid.contacts[index];
        u32 kind = contact->kind;
        if ((kind & ::Contact::KindHull) == 0)
        {
            continue;
        }

        InstanceContext* other = contact->instance;
        bool told = false;
        if ((kind & ::Contact::StoodOn) != 0)
        {
            Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
            TouchedVirtual(this, other, &up);
            told = true;
        }

        if ((kind & ::Contact::Ground) != 0)
        {
            Vector4 down = {0.0f, -1.0f, 0.0f, 1.0f};
            TouchedVirtual(this, other, &down);
            told = true;
        }

        if (told)
        {
            continue;
        }

        if ((kind & ::Contact::Touched) != 0)
        {
            TouchedVirtual(this, other, &g_DefaultBox.min);
        }

        if ((kind & ::Contact::PushRefused) != 0)
        {
            TouchedVirtual(this, other, &g_DefaultBox.min);
        }

        if ((kind & ::Contact::PushedOut) != 0)
        {
            TouchedVirtual(this, other, &g_DefaultBox.min);
        }
    }
}

void CharacterAgent::SendSurfaceMessages(ContactSet* contacts)
{
    CollisionSurface* standing = StandingSurface();
    if (standing != nullptr && SendsMessage(standing))
    {
        SendSurfaceMessage(standing);
    }

    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        ::Contact* contact = &contacts->solid.contacts[index];
        u32 kind = contact->kind;
        CollisionSurface* surface;
        if ((kind & ::Contact::KindTriangle) != 0)
        {
            if ((kind & TouchMarks) == 0)
            {
                continue;
            }

            surface = GetTriangleSurface(&contacts->solid.triangles[index]);
        }
        else
        {
            if ((kind & (TouchMarks | ::Contact::PushedOut)) == 0)
            {
                continue;
            }

            u16 hull = HullSurfaceIndex(CollisionOf(contact->instance), static_cast<u8>(contact->hullIndex));
            surface = &g_CollisionSurfaces.surfaces[hull];
        }

        if (surface != nullptr && SendsMessage(surface))
        {
            SendSurfaceMessage(surface);
        }
    }
}

void CharacterAgent::TouchOthers(ContactSet* contacts)
{
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 middle = place->position;
    middle.y += heightOffset;
    auto* node = static_cast<ObjectNode*>(GetGameNode(&instance->nodes, ObjectNodeKind));
    bool inWater = false;
    for (s32 index = 0; index < contacts->others.count; index++)
    {
        ::Contact* contact = &contacts->others.contacts[index];
        InstanceContext* other = contact->instance;
        if ((PointInsidePlanes(0.0f, &contact->space, &middle, &contact->outside) & 0xFF) == 0)
        {
            continue;
        }

        if ((contact->kind & ::Contact::KindTriangle) == 0)
        {
            TouchedVirtual(this, other, &g_DefaultBox.min);
            continue;
        }

        CollisionHit* triangle = &contacts->others.triangles[index];
        CollisionSurface* surface = GetTriangleSurface(triangle);
        if (surface == nullptr)
        {
            continue;
        }

        inWater = true;
        node->TouchedWater(triangle, &middle);
        if (SendsMessage(surface))
        {
            SendSurfaceMessage(surface);
        }
    }

    if (!inWater)
    {
        node->unknown134 = NoWater;
    }
}

void CharacterAgent::ClingToWall(ContactSet* contacts)
{
    constexpr f32 Reach = Rounded(0.4);
    constexpr f32 Facing = Rounded(0.707);
    constexpr f32 Upright = Rounded(0.01);
    constexpr f32 Above = Rounded(0.1);
    constexpr f32 GroundBelow = 2.0f;
    constexpr f32 SlideSpeed = -15.0f;
    if (IsOnGround(part))
    {
        return;
    }

    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    const Vector4* position = RowOf(&place->matrix, 3);
    place = instance->place;
    RotateAndTranslate(place);
    Vector4 ahead = {place->matrix.m[2][0] * Reach, place->matrix.m[2][1] * Reach, place->matrix.m[2][2] * Reach, 1.0f};
    Vector4 probe = {position->x + ahead.x, position->y + ahead.y, position->z + ahead.z, 1.0f};
    // A soft triangle the point just ahead of it is in, that it faces (within 45 degrees), upright, and no ground within 2
    // below it: it slides down the wall (every such wall makes the vehicle again)
    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        ::Contact* contact = &contacts->solid.contacts[index];
        if ((contact->kind & ::Contact::KindTriangle) == 0)
        {
            continue;
        }

        CollisionHit* triangle = &contacts->solid.triangles[index];
        if ((GetTriangleSurface(triangle)->collisionMask & SoftSurface) == 0)
        {
            continue;
        }

        s32 outside = -1;
        if (PointInsidePlanes(0.0f, &contact->space, &probe, &outside) == 0)
        {
            continue;
        }

        Vector4 normal;
        TriangleNormal(triangle, &normal);
        Vector4 wall = {normal.x, normal.y, normal.z, 1.0f};
        place = instance->place;
        RotateAndTranslate(place);
        if (!(Facing < wall.x * place->matrix.m[2][0] + wall.y * place->matrix.m[2][1] + wall.z * place->matrix.m[2][2]))
        {
            continue;
        }

        f32 inverse = InverseLength(&wall, LengthEpsilon);
        wall.x = wall.x * inverse;
        wall.y = wall.y * inverse;
        wall.z = wall.z * inverse;
        if (!(__builtin_fabsf(wall.y) < Upright))
        {
            continue;
        }

        place = instance->place;
        RotateAndTranslate(place);
        Vector4 top = *RowOf(&place->matrix, 3);
        Vector4 bottom = top;
        top.y += Above;
        bottom.y -= GroundBelow;
        if (GetCollisionCheck(instance->chunk, &top, &bottom, SolidToProbes, nullptr, nullptr, nullptr) != 0)
        {
            continue;
        }

        f32 gravity = properties->GetFloat(GravityProperty);
        auto* character = static_cast<CharacterPart*>(part);
        character->Bits() &= ~ClingClearedBits;
        character->Gravity() = gravity;
        Vector4 slide = {wall.x * SlideSpeed, wall.y * SlideSpeed, wall.z * SlideSpeed, 1.0f};
        velocity = slide;
        SetVehicle(WallClingKind, nullptr, 0);
        static_cast<WallClingVehicle*>(vehicle)->wallNormal = wall;
    }
}

u32 CharacterAgent::CrushedAgents(ContactSet* contacts)
{
    // Agents whose properties' state has bit 11 aren't crushed; the others get attack kind 8 (the slide's)
    constexpr u32 NotCrushed = 0x800;
    constexpr u32 CrushAttack = 8;
    u32 none = 1;
    for (s32 index = 0; index < contacts->solid.count; index++)
    {
        ::Contact* contact = &contacts->solid.contacts[index];
        if ((contact->kind & ::Contact::KindHull) == 0 || (contact->kind & PushMarks) == 0)
        {
            continue;
        }

        InstanceContext* other = contact->instance;
        AgentNode* node = AgentNodeOf(other);
        if (node == nullptr || (node->agent->properties->state & NotCrushed) != 0)
        {
            continue;
        }

        u32 kinds = KindOf(other);
        none = 0;
        QueueAttack(other, CrushAttack, instance, kinds);
    }

    return none;
}

void CharacterAgent::CheckLinkedCrush(f32 moved, ContactSet* contacts, const Vector4* from)
{
    constexpr f32 Moving = Rounded(0.2);
    constexpr f32 Short = Rounded(0.2);
    constexpr f32 Far = Rounded(0.4);
    if (!(Moving < moved))
    {
        return;
    }

    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 went = place->position;
    went.x = went.x - from->x;
    went.y = went.y + heightOffset - from->y;
    went.z = went.z - from->z;
    // The leader went less than (squared) a fifth of its second's push, or the push was big: what the leader was pushed out of
    // is crushed, or the second when there was nothing
    if (!(LengthSquared(went) < moved * Short) && !(Far < moved))
    {
        return;
    }

    if (CrushedAgents(contacts) != 0)
    {
        link->Second()->Crushed();
    }
}

void CharacterAgent::Crushed()
{
    crushCount += Linked() != 0 ? LinkedCrushStep : CrushStep;
    if (crushCount < CrushedAt || Invincible() != 0)
    {
        return;
    }

    ContactMessage message;
    ContactMessage::Construct(&message);
    message.word |= CrushHitKinds;
    ObjectPlace* place = instance->place;
    RotateAndTranslate(place);
    message.byte = CrushDamage;
    message.point = *RowOf(&place->matrix, 3);
    CallVirtual<void>(this, vtable, ContactSlot, &message, instance, 1u);
}

void CharacterAgent::KeepStanding(s32 contact, const Vector4* point, const Vector4* move, const Vector4* normal,
                                  ContactSet* contacts)
{
    constexpr s32 NoContact = -1;
    constexpr f32 Rising = Rounded(0.001);
    constexpr f32 Unit = Rounded(0.1);
    if (contact != NoContact)
    {
        ::Contact* ground = &contacts->solid.contacts[contact];
        if ((ground->kind & ::Contact::KindHull) != 0)
        {
            AssignReference(&standingOn, ground->instance);
            standingHull = ground->hullIndex;
            standingStamp = HullStamp(ObjectOf(standingOn));
            u32 standing = StandingHull;
            // A moving instance's hull is ridden while its matrix isn't scaled (its axes' lengths within 0.1 of 1)
            if ((contacts->solid.contacts[contact].instance->flags & MovingFlag) != 0)
            {
                CollisionHull* hull;
                GetInstanceHull(CollisionOf(ObjectOf(standingOn)), standingHull, &hull, &rideMatrix);
                standing = StandingRidden;
                for (u32 row = 0; row < 3; row++)
                {
                    if (Unit < __builtin_fabsf(__builtin_sqrtf(LengthSquared(*RowOf(&rideMatrix, row))) - 1.0f))
                    {
                        standing = StandingHull;
                        break;
                    }
                }
            }

            SetStanding(this, standing);
            if (standing == StandingRidden)
            {
                VuInvertRigidInPlace(&rideMatrix);
                VuTransformPoint(&rideMatrix, point, &ridePoint);
            }
        }
        else
        {
            SetStanding(this, StandingGround);
            AssignReference(&standingOn, nullptr);
            standingHull = NoContact;
            standingStamp = 0;
            CopyHit(&groundHit, &contacts->solid.triangles[contact]);
        }

        groundNormal = *normal;
        if (move->y < Rising)
        {
            static_cast<CreaturePart*>(part)->flags |= CreaturePart::FlagOnGround;
        }
        else
        {
            static_cast<CreaturePart*>(part)->flags &= ~CreaturePart::FlagOnGround;
        }
    }
    else
    {
        static_cast<CreaturePart*>(part)->flags &= ~CreaturePart::FlagOnGround;
    }

    if (IsOnGround(part))
    {
        return;
    }

    SetStanding(this, StandingNothing);
    AssignReference(&standingOn, nullptr);
    standingStamp = 0;
    standingHull = NoContact;
}

void CharacterAgent::Probe(f32 seconds, const Vector4* move, Vector4* out)
{
    constexpr u32 SquashedProbe = 3;
    constexpr f32 Squashing = Rounded(0.01);
    constexpr f32 SquashPerPush = 5.0f;
    constexpr f32 Pushing = Rounded(5e-05);
    constexpr f32 PushPerSecond = 5.0f;
    AssignReference(&probedInstance, nullptr);
    // No probes while its motion drives it (Controlled: its controls node's bit 0) or when it's no character
    if (Controlled() == 0)
    {
        auto* model = static_cast<ModelNode*>(GetGameNode(&instance->nodes, ModelNode::NodeKind));
        OgiAnimator* animator = model->animator;
        ObjectPlace* place = instance->place;
        place->SyncPosition();
        Vector4 middle = place->position;
        middle.y += heightOffset;
        CharacterBody* shape = body;
        const f32* weights = shape->probeWeights;
        const s32* exitPoints = shape->probeExitPoints;
        const Vector4* offsets = shape->probeOffsets;
        s32 count = shape->probeCount;
        // (Retail keeps the hit and its face from one probe to the next)
        Vector4 hitPoint;
        CollisionHit triangle;
        for (s32 index = 0; index < count; index++)
        {
            // The probe's exit point (the model's animation of it; the word at 0x44 is read without one) and its offset there
            SizedArray<ExitPointAnimation*>* exits = animator->exitPoints;
            ExitPointAnimation* exit = exits != nullptr ? exits->data[static_cast<u8>(exitPoints[index])] : nullptr;
            Vector4 point;
            if (IsPlaced(exit))
            {
                ExitPointAnimation* placed = UpdateExitPointMatrix(exit);
                point = *RowOf(&placed->matrix, 3);
                Vector4 offset = offsets[index];
                VuRotateVector(&placed->matrix, &offset, &offset);
                point.x += offset.x;
                point.y += offset.y;
                point.z += offset.z;
            }
            else
            {
                point = middle;
            }

            Vector4 probe = point;
            probe.x = probe.x - middle.x;
            probe.y = probe.y - middle.y;
            probe.z = probe.z - middle.z;
            Vector4 push = {0.0f, 0.0f, 0.0f, 1.0f};
            // Probes above its feet that are out of its hull: a ray from its middle at the probe's height to the probe point
            if (0.0f < probe.y && !IsPointInsideHull(&body->standingHull, &probe))
            {
                Vector4 start = middle;
                Vector4 end = point;
                start.y = end.y;
                ProbeWall(this, &start, &end, &hitPoint, &triangle, &push);
                ProbeInstance(this, &start, &end, &hitPoint, &triangle, &push);
            }

            f32 weight = weights[index];
            Vector4 weighted = {push.x * weight, push.y * weight, push.z * weight, 1.0f};
            auto* character = static_cast<CharacterPart*>(part);
            character->push.x += weighted.x;
            character->push.y += weighted.y;
            character->push.z += weighted.z;
            // The fourth probe's push squashes it
            if (index == SquashedProbe && proceduralJoints != nullptr)
            {
                f32 length = __builtin_sqrtf(LengthSquared(push));
                if (Squashing < length)
                {
                    proceduralJoints->Squash(1.0f - length * SquashPerPush);
                }
            }
        }
    }

    // The move slid along the probes' smoothed push (flat), pushed by it
    Vector4 smoothed = *static_cast<CharacterPart*>(part)->SmoothPush();
    smoothed.y = 0.0f;
    if (!(Pushing < smoothed.x * smoothed.x + smoothed.z * smoothed.z))
    {
        *out = *move;
        return;
    }

    f32 scale = seconds * PushPerSecond;
    *out = *move;
    smoothed.z = smoothed.z * scale;
    smoothed.x = smoothed.x * scale;
    Vector4 away = smoothed;
    RemoveMotionAgainst(out, &away);
    out->x += smoothed.x;
    out->y += smoothed.y;
    out->z += smoothed.z;
}

void CharacterAgent::Solve(f32 lift, f32 seconds, const Vector4* move, const f32* turn, f32* turnOut, u32 mode)
{
    constexpr f32 MostLift = Rounded(0.1);
    constexpr f32 RideMoving = Rounded(0.0001);
    constexpr f32 PushScale = Rounded(1.001);
    constexpr f32 Pushed = 1.0f;
    constexpr f32 Crushing = 0.25f;
    constexpr f32 Rising = Rounded(0.1);
    constexpr f32 Bounced = Rounded(0.0001);
    constexpr f32 BounceRestitution = 0.5f;
    constexpr f32 Pushing = Rounded(0.2);
    constexpr f32 Stuck = Rounded(0.2);
    ContactSet* contacts = BeginContacts();
    auto* character = static_cast<CharacterPart*>(part);
    u32 onGround = character->flags >> 2 & 1;
    ChunkData* chunk = instance->chunk;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 from = place->position;
    from.y += heightOffset;
    Vector4 motion;
    Probe(seconds, move, &motion);
    Vector4 rideMove;
    f32 rideTurnNow;
    if (RideMove(&rideMove, &rideTurnNow) != 0)
    {
        motion.x += rideMove.x;
        motion.y += rideMove.y;
        motion.z += rideMove.z;
        *turnOut = *turn + rideTurnNow;
    }
    else
    {
        rideMove = g_DefaultBox.min;
        *turnOut = *turn;
        rideMove.w = 1.0f;
    }

    const CollisionHull* hull = SolverHull(this);
    if (GatherTriangleContacts(contacts, chunk, &from, &motion, hull) != 0)
    {
        auto* follow = static_cast<FollowNode*>(GetGameNode(&instance->nodes, Node16));
        if (follow != nullptr)
        {
            follow->timer = 1.0f;
        }
    }

    // Itself, Cortex's partner and what's attached to it left out (16 attachments at most)
    InstanceContext* skipped[2 + 31];
    s32 skippedCount = 0;
    skipped[skippedCount++] = instance;
    if (properties->GetInt(CharacterProperty) == Cortex)
    {
        skipped[skippedCount++] = static_cast<InstanceContext*>(ObjectOf(G_GameController_00309914->progress.characters[Crash]));
    }

    auto* attachments = static_cast<AttachmentsNode*>(GetGameNode(&instance->nodes, AttachmentsKind));
    if (attachments != nullptr)
    {
        for (u32 index = 0; index < (attachments->count & 0x1F); index++)
        {
            skipped[skippedCount++] = attachments->linked[index];
        }
    }

    u32 kinds = IsThrown(AttackKind(character)) ? ThrownNodeKinds : SolidNodeKinds;
    GatherInstanceContacts(contacts, chunk, &from, &motion, &kinds, skipped, skippedCount, hull);
    MarkUnpushable(contacts, mode);
    if (ObjectOf(standingOn) != nullptr && StandingOf(this) == StandingRidden &&
        RideMoving < __builtin_sqrtf(LengthSquared(rideMove)))
    {
        MarkInstanceContact(contacts, static_cast<InstanceContext*>(ObjectOf(standingOn)), standingHull, RiddenMark);
    }

    // In the air, lifted by its height offset's change where there's room
    if (MostLift < lift)
    {
        lift = MostLift;
    }

    if (lift < -MostLift)
    {
        lift = -MostLift;
    }

    if (onGround == 0)
    {
        Vector4 liftBy = {0.0f, lift, 0.0f, 1.0f};
        Vector4 lifted = {from.x + liftBy.x, from.y + liftBy.y, from.z + liftBy.z, 1.0f};
        if (PointInsideSolidContact(0.0f, contacts, &lifted, ::Contact::SphereBit) == 0)
        {
            from.y += lift;
            heightOffset += lift;
        }
    }

    // Pushed out of what it's in: a deep push that went nowhere crushes (what it was pushed out of, else itself). The push is
    // capped at its radius
    Vector4 moved = {from.x + rideMove.x, from.y + rideMove.y, from.z + rideMove.z, 1.0f};
    Vector4 push;
    f32 pushed;
    Depenetrate(contacts, &moved, &push, 1, 0, 1, &pushed);
    push.x *= PushScale;
    push.y *= PushScale;
    push.z *= PushScale;
    f32 pushSquared = LengthSquared(push);
    // (Retail takes the squared length from the distance pushed in all; SolveLinked's swing takes the length)
    if (Pushed < pushed && Crushing < pushed - pushSquared && CrushedAgents(contacts) != 0)
    {
        Crushed();
    }

    CapPush(this, &push, &pushSquared);
    Vector4 start;
    if (pushSquared == 0.0f)
    {
        start = from;
    }
    else
    {
        Vector4 away = push;
        RemoveMotionAgainst(&motion, &away);
        Vector4 pushNormal;
        MoveCharacter(1.0f, contacts, &from, &push, &start, &pushNormal, 0);
    }

    Vector4 position = {from.x + rideMove.x, from.y + rideMove.y, from.z + rideMove.z, 1.0f};
    f32 height = IsCrouching(Part()) ? body->crouchHeight : body->height;
    f32 halfWidth = IsCrouching(Part()) ? body->crouchRadius : body->radius;
    Vector4 spheresPush;
    PushOutOfSpheres(height, halfWidth, contacts, &position, move, &spheresPush);
    motion.x += spheresPush.x;
    motion.y += spheresPush.y;
    motion.z += spheresPush.z;
    Vector4 normal = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 end;
    s32 ground = MoveCharacter(1.0f, contacts, &start, &motion, &end, &normal, onGround);
    PlaceAt(&end);
    KeepStanding(ground, &end, move, &normal, contacts);

    // A rise short of half of the motion halves the vertical speed
    Vector4 rose = {end.x - from.x, end.y - from.y, end.z - from.z, 1.0f};
    Vector4 step = rose;
    if (Rising < motion.y && rose.y < motion.y * 0.5f)
    {
        velocity.y *= 0.5f;
    }

    if (StepUp(contacts, &end, &motion, &step, onGround) != 0)
    {
        MoveInstanceBy(instance, &step);
    }

    // A push of the walk's bounced off what the move went into
    if ((walk->bits & WalkController::StateMask) == WalkController::StatePushed)
    {
        Vector4 bounce = {step.x - motion.x, step.y - motion.y, step.z - motion.z, 1.0f};
        if (Bounced < LengthSquared(bounce))
        {
            f32 inverse = InverseLength(&bounce, LengthEpsilon);
            bounce.x = bounce.x * inverse;
            bounce.y = bounce.y * inverse;
            bounce.z = bounce.z * inverse;
            walk->BouncePush(BounceRestitution, &bounce);
        }
    }

    TellTouched(contacts);
    if ((instance->flags & CollisionOnFlag) != 0 && !Watching())
    {
        SendSurfaceMessages(contacts);
        TouchOthers(contacts);
    }

    PushBodies(seconds, contacts);
    // Ground ahead of it on the ground (its facing as the velocity), unless it crouches, slams or spins
    u64 noGround = 0;
    if (IsOnGround(part) && (character->moveBits & CharacterPart::Crouching) == 0)
    {
        u32 kind = AttackKind(character);
        if (!IsSlam(kind) && !IsSpin(kind))
        {
            ObjectPlace* facing = instance->place;
            RotateAndTranslate(facing);
            noGround = GroundAhead(contacts, reinterpret_cast<const PhysicsBody*>(facing)) == 0;
        }
    }

    character->Bits() = (character->Bits() & ~NoGroundAhead) | noGround << 55;
    // Crushed when a big push moved it less than a fifth of it
    if (Pushing < pushSquared)
    {
        ObjectPlace* now = instance->place;
        now->SyncPosition();
        Vector4 went = now->position;
        went.y += heightOffset;
        went.x = went.x - from.x;
        went.y = went.y - from.y;
        went.z = went.z - from.z;
        if (LengthSquared(went) < pushSquared * Stuck && CrushedAgents(contacts) != 0)
        {
            Crushed();
        }
    }

    if (properties->GetInt(CharacterProperty) == Nina)
    {
        ClingToWall(contacts);
    }

    EndContacts();
}

void CharacterAgent::SolveLinked(f32 seconds, const Vector4* move, const f32* turn, f32* turnOut, u32 mode, u32 steps)
{
    constexpr Vector4 Reach = {0.5f, 2.0f, 0.5f, 1.0f};
    constexpr f32 Margin = Rounded(0.1);
    constexpr f32 PushScale = Rounded(1.001);
    constexpr f32 Pushed = 1.0f;
    constexpr f32 Crushing = 0.25f;
    constexpr f32 Leaning = Rounded(5e-05);
    constexpr f32 SwingLength = Rounded(1.3);
    constexpr f32 CastAbove = 0.5f;
    constexpr f32 CastBelow = Rounded(0.05);
    constexpr f32 SwingGravity = 120.0f;
    constexpr f32 AboveGround = Rounded(0.001);
    constexpr f32 Lowest = Rounded(0.45);
    constexpr f32 Highest = 0.5f;
    constexpr f32 Flat = Rounded(0.99);
    constexpr f32 Pull = 15.0f;
    auto* character = static_cast<CharacterPart*>(part);
    u32 onGround = character->flags >> 2 & 1;
    CharacterAgent* second = link->Second();
    f32 stepCount = static_cast<f32>(static_cast<s32>(steps));
    f32 share = 1.0f / stepCount;
    CharacterLink* secondLink = second->link;
    Vector4* swing = &secondLink->swingPosition;
    Vector4* swingVelocity = &secondLink->swingVelocity;
    seconds = seconds / stepCount;
    Vector4 stepMove = {share * move->x, share * move->y, share * move->z, 1.0f};
    f32 stepTurn = *turn / stepCount;
    ObjectPlace* place = instance->place;
    place->SyncPosition();
    Vector4 from = place->position;
    from.y += heightOffset;
    ContactSet* contacts = BeginContacts();
    const CollisionHull* hull = SolverHull(this);
    ChunkData* chunk = instance->chunk;

    // The triangles in a box around both (as far as the move goes and 0.1 more), the instances around their middle
    Box box;
    box.min = from;
    box.min.x = box.min.x - Reach.x;
    box.min.y = box.min.y - Reach.y;
    box.min.z = box.min.z - Reach.z;
    box.max = from;
    box.max.x = box.max.x + Reach.x;
    box.max.y = box.max.y + Reach.y;
    box.max.z = box.max.z + Reach.z;
    Vector4 low = *swing;
    low.x = low.x - Reach.x;
    low.y = low.y - Reach.y;
    low.z = low.z - Reach.z;
    Vector4 high = *swing;
    high.x = high.x + Reach.x;
    high.y = high.y + Reach.y;
    high.z = high.z + Reach.z;
    GrowBoxByPoint(&box, &low);
    GrowBoxByPoint(&box, &high);
    GrowBox(__builtin_sqrtf(LengthSquared(*move)) + Margin, &box);
    GatherBoxTriangleContacts(contacts, chunk, &box, hull);
    InstanceContext* skipped[2] = {instance, second->instance};
    u32 kinds = LinkedNodeKinds;
    Vector4 both = {from.x + swing->x, from.y + swing->y, from.z + swing->z, 1.0f};
    Vector4 middle = {both.x * 0.5f, both.y * 0.5f, both.z * 0.5f, 1.0f};
    Vector4 around = {1.0f, 1.0f, 1.0f, 1.0f};
    GatherInstanceContacts(contacts, chunk, &middle, &around, &kinds, skipped, 2, hull);
    for (s32 left = static_cast<s32>(steps); left > 0; left--)
    {
        ClearContactMarks(contacts);
        ObjectPlace* now = instance->place;
        now->SyncPosition();
        from = now->position;
        from.y += heightOffset;
        ObjectPlace* turned = instance->place;
        RotateAndTranslate(turned);
        Vector4 facing = *RowOf(&turned->matrix, 2);
        Vector4 started = from;

        // The second pushed out of what its swing went into (a deep push that went nowhere crushes), flat
        Vector4 swingPush = g_DefaultBox.min;
        swingPush.w = 1.0f;
        if (PointInsideSolidContact(0.0f, contacts, swing, 0) != 0)
        {
            f32 pushed;
            Depenetrate(contacts, swing, &swingPush, 1, 1, 0, &pushed);
            f32 length = __builtin_sqrtf(LengthSquared(swingPush));
            if (Pushed < pushed && Crushing < pushed - length && CrushedAgents(contacts) != 0)
            {
                Crushed();
            }

            if (length != 0.0f)
            {
                swingPush.y = 0.0f;
            }
        }

        Vector4 motion;
        Probe(seconds, &stepMove, &motion);
        Vector4 rideMove;
        f32 rideTurnNow;
        if (RideMove(&rideMove, &rideTurnNow) != 0)
        {
            motion.x += rideMove.x;
            motion.y += rideMove.y;
            motion.z += rideMove.z;
            *turnOut = stepTurn + rideTurnNow;
        }
        else
        {
            rideMove = g_DefaultBox.min;
            rideMove.w = 1.0f;
            *turnOut = stepTurn;
        }

        // Where the two are kept takes over the motion
        if (Leaning < __builtin_sqrtf(LengthSquared(linkedPoint)))
        {
            motion = linkedPoint;
        }

        motion.x += swingPush.x;
        motion.y += swingPush.y;
        motion.z += swingPush.z;

        // Turned about y by the turn
        s32 yaw;
        AngleFrom(&yaw, *turnOut, AngleRadians);
        InstanceContext* self = instance;
        ObjectPlace* turning = self->place;
        if (yaw != 0)
        {
            turning->SyncRotation();
            turning->bits = (turning->bits | ObjectPlace::BitTurned) & ~u64{ObjectPlace::BitMatrixTurned};
            s32 angle = yaw;
            Vector4 rotation;
            RotationFromYaw(&rotation, &angle);
            MultiplyRotations(&rotation, &rotation, &turning->rotation);
            turning->rotation.x = rotation.x;
            turning->rotation.y = rotation.y;
            turning->rotation.z = rotation.z;
            turning->rotation.w = rotation.w;
            QueueObject(self);
        }

        MarkUnpushable(contacts, mode);
        if (ObjectOf(standingOn) != nullptr && StandingOf(this) == StandingRidden)
        {
            MarkInstanceContact(contacts, static_cast<InstanceContext*>(ObjectOf(standingOn)), standingHull, RiddenMark);
        }

        // Retail's one vector for the place pushed out, the push's direction and the move's ground normal: a move that finds no
        // ground and falls back to contact 0 (MoveCharacter stuck) leaves it, and KeepStanding keeps it as the ground's normal
        Vector4 groundNormal = {from.x + rideMove.x, from.y + rideMove.y, from.z + rideMove.z, 1.0f};
        Vector4 push;
        f32 pushed;
        Depenetrate(contacts, &groundNormal, &push, 1, 0, 1, &pushed);
        push.x *= PushScale;
        push.y *= PushScale;
        push.z *= PushScale;
        f32 pushSquared = LengthSquared(push);
        if (Pushed < pushed && Crushing < pushed - pushSquared && CrushedAgents(contacts) != 0)
        {
            Crushed();
        }

        CapPush(this, &push, &pushSquared);
        Vector4 start;
        if (pushSquared == 0.0f)
        {
            start = from;
        }
        else
        {
            groundNormal = push;
            RemoveMotionAgainst(&motion, &groundNormal);
            Vector4 pushNormal;
            MoveCharacter(1.0f, contacts, &from, &push, &start, &pushNormal, 0);
        }

        Vector4 end;
        s32 ground = MoveCharacter(1.0f, contacts, &start, &motion, &end, &groundNormal, onGround);
        PlaceAt(&end);
        KeepStanding(ground, &end, move, &groundNormal, contacts);
        Vector4 rose = {end.x - from.x, end.y - from.y, end.z - from.z, 1.0f};
        Vector4 step = rose;
        if (StepUp(contacts, &end, &motion, &step, onGround) != 0)
        {
            MoveInstanceBy(instance, &step);
        }

        TellTouched(contacts);
        if ((instance->flags & CollisionOnFlag) != 0 && !Watching())
        {
            SendSurfaceMessages(contacts);
            TouchOthers(contacts);
        }

        PushBodies(seconds, contacts);

        // The second's swing: kept behind the leader (1.3 back along its facing) at the height of what's under it, falling
        // otherwise, between 0.45 below where the push took the leader and 0.5 above (where the two are kept then pulled
        // toward the ground's slope, or along the facing on level ground)
        secondLink->swingLength = SwingLength;
        Vector4 behind = {facing.x * SwingLength, facing.y * SwingLength, facing.z * SwingLength, 1.0f};
        Vector4 anchor = {started.x - behind.x, started.y - behind.y, started.z - behind.z, 1.0f};
        // Retail's cast leaves the point and the normal as they were when a sphere is pushed down (it returns 0): no point,
        // the anchor as the normal
        Vector4 hit = {0.0f, 0.0f, 0.0f, 1.0f};
        Vector4 slope = {anchor.x - hit.x, anchor.y - hit.y, anchor.z - hit.z, 1.0f};
        Vector4 target = slope;
        InstanceContext* under = nullptr;
        f32 drop = CastDown(CastAbove, CastBelow, contacts, swing, &hit, &slope, nullptr, &under);
        Vector4 nextSwing;
        nextSwing.x = target.x;
        nextSwing.z = target.z;
        nextSwing.y = swing->y + swingVelocity->y * seconds;
        Vector4 nextVelocity = *swingVelocity;
        if (drop == NoHit)
        {
            nextVelocity.y = nextVelocity.y - seconds * SwingGravity;
        }
        else
        {
            nextSwing.y = hit.y + AboveGround;
            nextVelocity.y = 0.0f;
            if (under != nullptr)
            {
                void* movement = GetGameNode(&under->nodes, MovementNodeKind);
                Vector4 delta = {0.0f, 0.0f, 0.0f, 1.0f};
                if (movement != nullptr)
                {
                    MovementVelocity(static_cast<MovementNode*>(movement), &delta);
                }

                nextVelocity.y = delta.y;
            }
        }

        f32 lowest = start.y - Lowest;
        if (nextSwing.y < lowest)
        {
            nextSwing.y = lowest;
            nextVelocity.y = swingVelocity->y;
        }

        linkedPoint = g_DefaultBox.min;
        linkedPoint.w = 1.0f;
        f32 highest = start.y + Highest;
        if (highest < nextSwing.y)
        {
            if (0.0f < nextVelocity.y)
            {
                nextVelocity.y = swingVelocity->y;
            }

            if (drop == NoHit)
            {
                nextSwing.y = highest;
            }
            else
            {
                ObjectPlace* leader = instance->place;
                RotateAndTranslate(leader);
                Matrix4x4 matrix = leader->matrix;
                if (velocity.y < swingVelocity->y)
                {
                    velocity.y = swingVelocity->y;
                }

                linkedPoint.x = slope.x;
                linkedPoint.y = slope.y;
                linkedPoint.z = slope.z;
                linkedPoint.w = 1.0f;
                if (linkedPoint.y < Flat)
                {
                    linkedPoint.y = linkedPoint.y - 1.0f / linkedPoint.y;
                }
                else
                {
                    linkedPoint = *RowOf(&matrix, 2);
                }

                f32 pull = seconds * Pull;
                linkedPoint.x = linkedPoint.x * pull;
                nextSwing.y = start.y + Highest;
                linkedPoint.z = linkedPoint.z * pull;
                linkedPoint.y = linkedPoint.y * pull;
            }
        }

        if (onGround != 0)
        {
            stepMove.y = 0.0f;
        }

        nextSwing.w = 1.0f;
        CheckLinkedCrush(__builtin_sqrtf(LengthSquared(swingPush)), contacts, &from);
        secondLink->swingPosition = nextSwing;
        secondLink->swingVelocity = nextVelocity;
    }

    character->Bits() &= ~NoGroundAhead;
    EndContacts();
}

Vector4* CharacterPart::SmoothPush()
{
    constexpr f32 Small = Rounded(0.1);
    Vector4 average;
    average.x = smoothedPush.x * 0.5f + push.x * 0.5f;
    average.y = smoothedPush.y * 0.5f + push.y * 0.5f;
    average.z = smoothedPush.z * 0.5f + push.z * 0.5f;
    average.w = 1.0f;
    smoothedPush = average;
    if (__builtin_sqrtf(LengthSquared(smoothedPush)) < Small)
    {
        smoothedPush = g_DefaultBox.min;
        smoothedPush.w = 1.0f;
    }

    return &smoothedPush;
}
