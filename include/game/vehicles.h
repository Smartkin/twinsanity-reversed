#pragma once

#include "abi.h"
#include "common.h"
#include "gcc2.h"
#include "game/agents.h"
#include "game/collision.h"
#include "game/hull.h"
#include "game/math.h"
#include "game/place.h"
#include "game/reference.h"

class Agent;
class CharacterAgent;
struct ChunkData;
struct ChunkLinkData;
struct CollisionSurface;
struct InstanceContext;
// The platform's screen models (platform/graphics.h; the PS2's are 0x10 bytes, made by the renderer's builder)
struct ScreenModel;

// A trail of skid marks (no vtable, 0x1E0 bytes; the Rollerbrawl's and the Humiliskate's two each): the last point, the colours the
// next strip starts with, a ring of 100 marks (screen models), how many frames went by without a mark and the trail's length
struct alignas(16) SkidMarks
{
    static constexpr s32 MarkCount = 100;

    u8 active;
    u8 unused01[0xF];
    Vector4 lastPoint;
    // The last point lowered by the mark's depth (only written)
    Vector4 unused20;
    // The next strip's near top and low corners' colours (white, the alpha a slope's shade; the second 0x00FFFFFF after a strip).
    // Never set before the first strip (retail draws it with what the heap left)
    u32 edgeColour;
    u32 lowColour;
    ScreenModel* marks[MarkCount];
    s32 next;
    s32 idleFrames;
    f32 length;
    u8 unused1D4[0xC];
};
CHECK_OFFSET(SkidMarks, marks, 0x38);
CHECK_OFFSET(SkidMarks, length, 0x1D0);
CHECK_SIZE(SkidMarks, 0x1E0);

extern "C"
{
    // No marks, inactive, nothing laid (the colours left as they are)
    SkidMarks* ConstructSkidMarks(SkidMarks* marks) RETAIL(FUN_00161ac0);
    // Every mark freed (FreeSkidMark), the trail freed when the flags say
    void DestroySkidMarks(SkidMarks* marks, u32 destroyFlags) RETAIL(FUN_00161b08);
    // A mark (none skipped) put on the list of screen models freed after the next frame (gamecontroller.h's g_FreedBlocks and
    // g_FreedBlockCount; ReleaseFreedMemory frees them)
    void FreeSkidMark(ScreenModel* mark) RETAIL(FUN_00161b78);
    // Every mark freed (inline FreeSkidMark), inactive, no length
    void ClearSkidMarks(SkidMarks* marks) RETAIL(FUN_00161bd8);
    // A frame without a mark: the 4th in a row makes it inactive with no length
    void IdleSkidMarks(SkidMarks* marks) RETAIL(FUN_00161bb0);
    // A mark laid beside a point (the side's direction times the offset, jittered 0.02) on the triangles of a collision cache that
    // take marks (SurfaceFlags' soft), a strip the depth wide (jittered) between the last point and the new one
    void LaySkidMark(f32 offset, f32 depth, SkidMarks* marks, const Vector4* point, const Vector4* side, CollisionCache* cache)
        RETAIL_N32(FUN_0015bd20);
    // Its marks drawn through g_RenderView's screen and clip matrices (the screen one's depth moved 15% back)
    void DrawSkidMarks(SkidMarks* marks) RETAIL(FUN_0015c478);
    // A point dropped onto a triangle's plane (x and z kept)
    void DropOntoTriangle(const Vector4* point, const CollisionHit* triangle, Vector4* out) RETAIL(FUN_0015c550);
    // A segment clipped to a triangle seen from above: whether any of it is over it, the clipped ends
    u32 ClipToTriangle(const CollisionHit* triangle, const Vector4* from, const Vector4* to, Vector4* clippedFrom,
                       Vector4* clippedTo) RETAIL(FUN_0015c5d0);
}

// A vehicle's bits: its character drives it (it takes the hits first; kinds 1 and 3 let the passenger go with it), and it's held
// (commands 650 and 651, HoldVehicle and ReleaseVehicle; the Humiliskate stands still). Retail reads and writes them as the u64
// at 0, whose high half is the vehicle's agent
union VehicleBits
{
    u32 value;
    struct
    {
        u32 unused0 : 1;
        u32 drives : 1;
        u32 held : 1;
        u32 unused3 : 29;
    };
};
CHECK_SIZE(VehicleBits, 4);

// What a playable character rides (retail VehicleBase; the agent's vehicle at 0xB8),
// made by SetPlayerVehicle by kind: 1 the Rollerbrawl, 3 the Humiliskate, 5 the hoverboard, 6 the wrestle, 7 clinging to a wall,
// 8 the other character riding along on a 1 or a 3. Its character, the other agent it places (the other character, the driver of
// a passenger, the wrestled creature, the hoverboard object), where the character goes when it leaves, where it and the other are
// put each frame and, on the Humiliskate, its height above the ground. Abstract: slots 2 (Place), 8 (Kind) and 11 (CollisionBox)
// are the derived classes'. Slot 1 is Start (the constructors call their own), slot 3 the destructor
class Vehicle
{
public:
    // The kinds (2 and 4 none of them: conditions 595 and 597 test them)
    enum Kind : u32
    {
        KindRollerbrawl = 1,
        KindUnused2 = 2,
        KindHumiliskate = 3,
        KindUnused4 = 4,
        KindHoverboard = 5,
        KindWrestle = 6,
        KindWallCling = 7,
        KindPassenger = 8,
    };

    enum Slots : u32
    {
        SlotStart = 1,
        SlotPlace = 2,
        SlotDestroy = 3,
        SlotPush = 4,
        SlotKnock = 5,
        SlotCanChangeChunk = 6,
        SlotFrame = 7,
        SlotKind = 8,
        SlotVelocity = 9,
        SlotKeepsGun = 10,
        SlotCollisionBox = 11,
        SlotSetVelocity = 12,
        SlotDraw = 13,
        SlotHeadFollowsCamera = 14,
        SlotHasBody = 15,
        SlotSplashSphere = 16,
        SlotForgetOther = 17,
    };

    VehicleBits bits;
    CharacterAgent* agent;
    Agent* other;
    u8 unused0C[4];
    Matrix4x4 exitMatrix;
    Matrix4x4 agentMatrix;
    Matrix4x4 otherMatrix;
    f32 height;
    const GccVTableEntry* vtable;
    u8 unusedD8[8];

    // The abstract slots
    u32 Place()
    {
        return CallVirtual<u32>(this, vtable, SlotPlace);
    }

    u32 Kind()
    {
        return CallVirtual<u32>(this, vtable, SlotKind);
    }

    void CollisionBox(Vector4* min, Vector4* max)
    {
        CallVirtual<void>(this, vtable, SlotCollisionBox, min, max);
    }

    // The slots the base has a version of, called through the vtable
    void StartVirtual()
    {
        CallVirtual<void>(this, vtable, SlotStart);
    }

    void DestroyVirtual(u32 destroyFlags)
    {
        CallVirtual<void>(this, vtable, SlotDestroy, destroyFlags);
    }

    void PushVirtual(const Vector4* velocity, InstanceContext* instance)
    {
        CallVirtual<void>(this, vtable, SlotPush, velocity, instance);
    }

    void KnockVirtual(const Vector4* push, u32 unused, InstanceContext* source)
    {
        CallVirtual<void>(this, vtable, SlotKnock, push, unused, source);
    }

    u32 CanChangeChunkVirtual(ChunkData* from, ChunkLinkData* link)
    {
        return CallVirtual<u32>(this, vtable, SlotCanChangeChunk, from, link);
    }

    void FrameVirtual(f32 seconds)
    {
        CallVirtual<void>(this, vtable, SlotFrame, seconds);
    }

    u32 VelocityVirtual(Vector4* velocity)
    {
        return CallVirtual<u32>(this, vtable, SlotVelocity, velocity);
    }

    u32 KeepsGunVirtual()
    {
        return CallVirtual<u32>(this, vtable, SlotKeepsGun);
    }

    void SetVelocityVirtual(const Vector4* velocity)
    {
        CallVirtual<void>(this, vtable, SlotSetVelocity, velocity);
    }

    void DrawVirtual()
    {
        CallVirtual<void>(this, vtable, SlotDraw);
    }

    u32 HeadFollowsCameraVirtual()
    {
        return CallVirtual<u32>(this, vtable, SlotHeadFollowsCamera);
    }

    u32 HasBodyVirtual()
    {
        return CallVirtual<u32>(this, vtable, SlotHasBody);
    }

    void SplashSphereVirtual(Vector4* centre, f32* radius)
    {
        CallVirtual<void>(this, vtable, SlotSplashSphere, centre, radius);
    }

    void ForgetOtherVirtual()
    {
        CallVirtual<void>(this, vtable, SlotForgetOther);
    }

    // Slot 1: the character's place made up to date, the matrices the identity, the height 0, the character's body removed, a
    // sphere body made when HasBody (virtual) says so (not placing the instance, colliding with dynamic scenery, rigid bodies,
    // crates, creatures and generic objects), then Place (virtual)
    void Start() RETAIL(FUN_0014ef00);
    // Slot 3
    void Destroy(u32 destroyFlags) RETAIL(FUN_0015ff08);
    // Slot 4: a velocity turned by an instance's place (none: nothing) added to the body's
    void Push(const Vector4* velocity, InstanceContext* instance) RETAIL(FUN_001600d8);
    // Slot 5 (only the hoverboard's does something; nothing calls it)
    void Knock(const Vector4* push, u32 unused, InstanceContext* source) RETAIL(FUN_0015dee0);
    // Slot 6: only through links with bit 18; the other's instance moved through too when it's in the same chunk
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_0015ff78);
    // Slot 7 (the seconds, which the base's doesn't read): when Place says so, the character put at agentMatrix and the other at
    // otherMatrix (each queued when it moved)
    void Frame(f32 seconds) RETAIL_N32(FUN_0015ffe8);
    // Slot 9: the body's velocity (whether there's a body)
    u32 Velocity(Vector4* velocity) RETAIL(FUN_00160198);
    // Slot 10: whether the character keeps its gun (no)
    u32 KeepsGun() RETAIL(FUN_0015dee8);
    // Slot 12: the starting velocity (ignored)
    void SetVelocity(const Vector4* velocity) RETAIL(FUN_0015def0);
    // Slot 13: its part of the character's overlay (nothing)
    void Draw() RETAIL(FUN_0015def8);
    // Slot 14: whether the character's head tracking follows the camera (yes)
    u32 HeadFollowsCamera() RETAIL(FUN_0015df00);
    // Slot 15: whether Start makes a sphere body (yes)
    u32 HasBody() RETAIL(FUN_0015df08);
    // Slot 16: the sphere particles splash on: the character's position raised 1, radius 0.9 (CharacterAgent::SplashPoint)
    void SplashSphere(Vector4* centre, f32* radius) RETAIL(FUN_001601e8);
    // Slot 17: the other forgotten
    void ForgetOther() RETAIL(FUN_00160210);

    // The character's body (kind 5 node) taken out of the physics world, and put at a position
    void RemoveBody() RETAIL(FUN_0015fec8);
    void SetBodyPosition(const Vector4* position) RETAIL(FUN_00160090);
};
CHECK_OFFSET(Vehicle, other, 0x8);
CHECK_OFFSET(Vehicle, exitMatrix, 0x10);
CHECK_OFFSET(Vehicle, agentMatrix, 0x50);
CHECK_OFFSET(Vehicle, otherMatrix, 0x90);
CHECK_OFFSET(Vehicle, height, 0xD0);
CHECK_OFFSET(Vehicle, vtable, 0xD4);
CHECK_SIZE(Vehicle, 0xE0);

// When the vehicle's Place says so, the character put at agentMatrix and the other at otherMatrix, each queued when its place
// changed (retail works out the rotation of agentMatrix first and drops it)
inline void PlaceRiders(Vehicle* vehicle)
{
    if (vehicle->Place() == 0)
    {
        return;
    }

    Vector4 rotation;
    GetRotationVec(&rotation, &vehicle->agentMatrix);
    InstanceContext* instance = vehicle->agent->instance;
    if (SetPlaceMatrix(instance->place, &vehicle->agentMatrix) != 0)
    {
        QueueObject(instance);
    }

    if (vehicle->other != nullptr)
    {
        InstanceContext* otherInstance = vehicle->other->instance;
        if (SetPlaceMatrix(otherInstance->place, &vehicle->otherMatrix) != 0)
        {
            QueueObject(otherInstance);
        }
    }
}

// Kind 1: the two characters fighting as a rolling ball (vtable RollerbrawlVehicle_Methods, 0x5A0 bytes). Its state, the other
// character, the ball's radius, its snow and the shell's scale, a sticky touch this frame, the heading it started with, the wobble
// after hard knocks (squashing both characters), the state's time, the melting, where it stopped and its two skid trails
class RollerbrawlVehicle : public Vehicle
{
public:
    enum State : s32
    {
        StateRolling = 0,
        StateStopped = 1,
        // Nothing sets it
        StateSquashed = 2,
    };

    s32 state;
    CharacterAgent* partner;
    f32 radius;
    f32 snowScale;
    f32 snow;
    u8 onSticky;
    u8 unusedF5[0xB];
    Vector4 heading;
    // 0 when it starts, never read
    u32 unused110;
    u8 unused114[0xC];
    Vector4 lastVelocity;
    Vector4 wobbleAxis;
    f32 wobble;
    u8 unused144[0xC];
    Matrix4x4 squash;
    Vector4 squashOffset;
    f32 wobblePhase;
    // How long it's been still (rolling), stopped or squashed
    f32 stateTime;
    f32 meltTime;
    u8 unused1AC[4];
    Vector4 stopPosition;
    Vector4 stopRotation;
    Vector4 uprightRotation;
    SkidMarks leftMarks;
    SkidMarks rightMarks;

    // Made for the character and the other one: its trails made, then Start
    static RollerbrawlVehicle* Construct(RollerbrawlVehicle* vehicle, CharacterAgent* agent, CharacterAgent* partner)
        RETAIL(FUN_00152a60);
    void Start() RETAIL(FUN_00152ad8);
    u32 Place() RETAIL(FUN_00152f68);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00160e00);
    // Only through links with bit 18, its last velocity turned through
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00160ef0);
    void Frame(f32 seconds) RETAIL_N32(FUN_00152d58);
    // 1
    u32 Kind() RETAIL(FUN_0015e020);
    // -1 to 1
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(FUN_0015e028);
    // The body's velocity set
    void SetVelocity(const Vector4* velocity) RETAIL(FUN_00160eb0);
    // Both trails
    void Draw() RETAIL(FUN_001610a0);
    // No
    u32 HeadFollowsCamera() RETAIL(FUN_0015e060);
    // The body's position and the ball's radius
    void SplashSphere(Vector4* centre, f32* radius) RETAIL(FUN_001610d0);

    // The states' frames
    void RollFrame(f32 seconds) RETAIL_N32(FUN_001516c8);
    void StoppedFrame(f32 seconds) RETAIL_N32(FUN_00151990);
    void SquashedFrame(f32 seconds) RETAIL_N32(FUN_00151c98);
    // To state 1 (event 0x4C to both), back to state 0 (event 0x4D to both)
    void Stop() RETAIL(FUN_00151f70);
    void Roll() RETAIL(FUN_00160b80);
    // Whether there's ground just under the ball
    u32 OnGround() RETAIL(FUN_00152298);
    // The stick's push (touching: the body touched something this frame)
    void StickPush(u32 touching) RETAIL(FUN_00150d18);
    void Drag() RETAIL(FUN_001511e0);
    void SpinDrag() RETAIL(FUN_001512f8);
    // A hard change of velocity starts a wobble; the squash matrix made from it
    void Wobble(f32 seconds) RETAIL_N32(FUN_001513e0);
    void LaySkidMarks(u32 touching) RETAIL(FUN_001523d8);
    // The instances its sphere goes into told (surface message, Bumped, Collided)
    void HitInstances() RETAIL(FUN_00152660);
    // The snow changed by an amount, the body's mass and radius made again
    void GrowSnow(f32 amount) RETAIL_N32(FUN_00160be8);
    void WearSnow(f32 amount) RETAIL_N32(FUN_00160ce8);
    // The snow shell (the character's AgentRef2) placed or hidden, the melting run down
    void PlaceSnowShell(f32 seconds) RETAIL_N32(FUN_00150a40);
    // The body's contact (its retail name): sticky ground, lava melting the snow, the surface's message
    void ApplyStickySurface(const CollisionHit* triangle) RETAIL(ApplyStickySurface);
    // The body's touch: nothing
    void TouchedInstance(InstanceContext* other, u32 hull) RETAIL(FUN_00161098);
    // The body's contact and touch callbacks (DynamicBody's contactCallback and touchCallback), the vehicle their argument
    static void ContactCallback(const CollisionHit* triangle, void* vehicle) RETAIL(func_00160F38);
    static void TouchCallback(InstanceContext* other, u32 hull, void* vehicle) RETAIL(func_00161068);
};
CHECK_OFFSET(RollerbrawlVehicle, state, 0xE0);
CHECK_OFFSET(RollerbrawlVehicle, heading, 0x100);
CHECK_OFFSET(RollerbrawlVehicle, squash, 0x150);
CHECK_OFFSET(RollerbrawlVehicle, stopPosition, 0x1B0);
CHECK_OFFSET(RollerbrawlVehicle, leftMarks, 0x1E0);
CHECK_OFFSET(RollerbrawlVehicle, rightMarks, 0x3C0);
CHECK_SIZE(RollerbrawlVehicle, 0x5A0);

// Kind 3: the character skating on the other one as a board (vtable HumiliskateVehicle_Methods, 0x650 bytes; no body: it moves
// itself through its collision cache). Its shape and gravity, the board's place, velocity, ground and up, the ground and trick
// state, crouching and leaning, the suspension, its rail, the jump, the top speeds (command 555, SetVehicleHumiliskate, sets them)
// and two skid trails
class HumiliskateVehicle : public Vehicle
{
public:
    enum Trick : s32
    {
        TrickRiding = 0,
        TrickSpin = 1,
        TrickFlip = 2,
        TrickLanding = 3,
        TrickLanded = 4,
        TrickGrinding = 5,
        TrickOffRail = 6,
    };

    f32 radiusX;
    f32 radiusY;
    f32 radiusZ;
    f32 reach;
    f32 gravity;
    f32 riderHeight;
    u8 unusedF8[8];
    Matrix4x4 board;
    // 0 when it starts, never read
    u32 unused140;
    u8 unused144[0xC];
    Vector4 velocity;
    Vector4 lastVelocity;
    Vector4 groundNormal;
    Vector4 up;
    u8 onGround;
    u8 wasOnGround;
    u8 unused192[2];
    s32 trick;
    f32 trickTime;
    u8 crouched;
    u8 leanLeft;
    u8 leanRight;
    u8 backwards;
    f32 suspension;
    f32 suspensionSpeed;
    u8 unused1A8[8];
    CollisionCache cache;
    CollisionSurface* surface;
    f32 airTime;
    f32 spin;
    f32 flip;
    f32 spinTarget;
    f32 flipTarget;
    u8 jumped;
    u8 unused219[3];
    // The animation events last sent to the character and the board character (never read)
    u32 unused21C;
    u32 unused220;
    // 1 when it starts, never read
    f32 unused224;
    u8 grinding;
    u8 unused229[7];
    Vector4 railStart;
    Vector4 railEnd;
    // 0 when it starts, never read
    u32 unused250;
    u32 unused254;
    f32 railCooldown;
    // 0 when it starts, never read
    u32 unused25C;
    u8 touched;
    u8 unused261[3];
    f32 crouchedSpeed;
    f32 topSpeed;
    f32 jumpCooldown;
    f32 jumpReleased;
    u8 jumpedOffRail;
    u8 unused275[0xB];
    SkidMarks leftMarks;
    SkidMarks rightMarks;
    // The clock's time when it starts (never read)
    u32 unused640;
    u8 unused644[0xC];

    // Made for the character and the other one: its cache (the character's instance, mask 0x10) and trails made, then Start
    static HumiliskateVehicle* Construct(HumiliskateVehicle* vehicle, CharacterAgent* agent, CharacterAgent* other)
        RETAIL(FUN_00158ad8);
    void Start() RETAIL(FUN_00158b88);
    u32 Place() RETAIL(FUN_00159a28);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00161648);
    // Its own velocity pushed (turned by the instance's place when there's one)
    void Push(const Vector4* velocity, InstanceContext* instance) RETAIL(FUN_00161518);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_001616f8);
    void Frame(f32 seconds) RETAIL_N32(FUN_00159730);
    // 3
    u32 Kind() RETAIL(FUN_0015dfb0);
    // Its own velocity
    u32 Velocity(Vector4* velocity) RETAIL(FUN_0015dff0);
    // -1 to 1
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(FUN_0015dfb8);
    // Its velocity and last velocity
    void SetVelocity(const Vector4* velocity) RETAIL(FUN_0015e000);
    // Both trails
    void Draw() RETAIL(FUN_001617e8);
    // No
    u32 HasBody() RETAIL(FUN_0015e018);

    // A substep's parts
    void Jump(f32 seconds) RETAIL_N32(FUN_00155490);
    // Whether a jump tells the characters (yes; called with nothing set up)
    u32 JumpTells() RETAIL(FUN_00161508);
    void Steer(f32 seconds) RETAIL_N32(FUN_00155600);
    // Rails and the ground
    void Collide(f32 seconds) RETAIL_N32(FUN_00155978);
    // The rails among the cache's triangles: how many (starts and ends out; the used flags marked)
    s32 FindRails(CollisionHit** triangles, s32 count, u8* used, u32 usedSize, Vector4* starts, Vector4* ends) RETAIL(FUN_00158160);
    // Its cache's triangles listed: how many
    s32 CachedTriangles(CollisionHit** triangles) RETAIL(FUN_001615d8);
    // The ground's response to a normal (the friction first, then the substep)
    void GroundResponse(f32 friction, f32 seconds, const Vector4* normal) RETAIL_N32(FUN_00157210);
    // The board's y axis turned onto an up
    void AlignUp(const Vector4* up) RETAIL(FUN_00156d20);
    void LimitSpeed(f32 seconds) RETAIL_N32(FUN_00156fd8);
    void CollideInstances(f32 seconds) RETAIL_N32(FUN_00157f30);
    // An instance's hulls
    void CollideHulls(f32 seconds, InstanceContext* instance) RETAIL_N32(FUN_00157c98);
    // Whether an ellipsoid on a place touches a hull: the push, the contact point in the place's space (retail scales it by all
    // three radii) and the normal
    u32 TouchesHull(const Matrix4x4* place, const Vector4* radii, const Matrix4x4* hullMatrix, const CollisionHull* hull,
                    Vector4* push, Vector4* point, Vector4* normal) RETAIL(FUN_001578c0);
    // A hull touched: told, and pushed out when solid and the instance answers
    void TouchedHull(f32 seconds, const Vector4* point, const Vector4* push, const Vector4* normal, InstanceContext* instance,
                     u32 hull, u32 solid) RETAIL_N32(FUN_00157a88);
    void Suspension(f32 seconds) RETAIL_N32(FUN_00158db8);
    // In the air: the time the fall to the ground takes and its distance (on the ground 0, 0)
    void FallTime(f32* time, f32* height) RETAIL(FUN_001591e8);
    void Tricks(f32 seconds) RETAIL_N32(FUN_001593c0);
    // The animation events picked and sent to both characters (retail returns the last RunAgentEvent's)
    void SendEvents(f32 seconds) RETAIL_N32(FUN_00158ef8);
    void LaySkidMarks() RETAIL(FUN_001587c0);
};
CHECK_OFFSET(HumiliskateVehicle, board, 0x100);
CHECK_OFFSET(HumiliskateVehicle, velocity, 0x150);
CHECK_OFFSET(HumiliskateVehicle, onGround, 0x190);
CHECK_OFFSET(HumiliskateVehicle, crouched, 0x19C);
CHECK_OFFSET(HumiliskateVehicle, cache, 0x1B0);
CHECK_OFFSET(HumiliskateVehicle, surface, 0x200);
CHECK_OFFSET(HumiliskateVehicle, grinding, 0x228);
CHECK_OFFSET(HumiliskateVehicle, railStart, 0x230);
CHECK_OFFSET(HumiliskateVehicle, crouchedSpeed, 0x264);
CHECK_OFFSET(HumiliskateVehicle, jumpedOffRail, 0x274);
CHECK_OFFSET(HumiliskateVehicle, leftMarks, 0x280);
CHECK_OFFSET(HumiliskateVehicle, unused640, 0x640);
CHECK_SIZE(HumiliskateVehicle, 0x650);

// Kind 5: a hoverboard (vtable HoverboardVehicle_Methods, 0x110 bytes), the board object's agent its other. How knocked it is, the
// body it tows, armed (the character keeps its gun) and the handling that follows, its hover height, the follow camera's rate and
// its ellipsoid's radii
class HoverboardVehicle : public Vehicle
{
public:
    f32 knocked;
    Reference* towed;
    u8 towReady;
    u8 armed;
    u8 unusedEA[2];
    f32 hoverHeight;
    f32 cameraRate;
    u8 keepsHeading;
    u8 tows;
    u8 canRise;
    u8 unusedF7;
    f32 radiusX;
    f32 radiusY;
    f32 radiusZ;
    u8 unused104[0xC];

    // Made for the character, the board object's agent and armed (SetPlayerVehicle's 4th argument): Start, the camera rate 5
    static HoverboardVehicle* Construct(HoverboardVehicle* vehicle, CharacterAgent* agent, Agent* board, u32 armed)
        RETAIL(FUN_0015b2b0);
    void Start() RETAIL(FUN_0015b330);
    u32 Place() RETAIL(FUN_0015b7a0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001619c0);
    // Knocked and pushed away from the source by the push's strength (its w)
    void Knock(const Vector4* push, u32 unused, InstanceContext* source) RETAIL(FUN_0015ac20);
    void Frame(f32 seconds) RETAIL_N32(FUN_0015b4c8);
    // 5
    u32 Kind() RETAIL(FUN_00161970);
    // Armed
    u32 KeepsGun() RETAIL(FUN_00161968);
    // -2 to 2
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(FUN_00161978);

    // Armed: keeps its heading and can rise, radii 1, 0.7, 1.6
    void SetArmedHandling() RETAIL(FUN_00161a48);
    // Unarmed: tows, radii 1.4, 0.5, 1.4
    void SetUnarmedHandling() RETAIL(FUN_00161a88);
    void Drive() RETAIL(FUN_00159d28);
    // The four corners' springs: the last corner's height error out
    void Hover(f32* heightError) RETAIL(FUN_0015a2a8);
    // The height error first; the drive's direction and the body's matrix
    void Tilt(f32 heightError, const Vector4* drive, const Matrix4x4* bodyMatrix) RETAIL_N32(FUN_0015a688);
    void Turn(const Vector4* drive, const Matrix4x4* bodyMatrix) RETAIL(FUN_0015a9f0);
    void Drag() RETAIL(FUN_0015a030);
    void SpinDrag() RETAIL(FUN_0015a178);
    void Tow() RETAIL(FUN_0015adf8);
};
CHECK_OFFSET(HoverboardVehicle, knocked, 0xE0);
CHECK_OFFSET(HoverboardVehicle, armed, 0xE9);
CHECK_OFFSET(HoverboardVehicle, keepsHeading, 0xF4);
CHECK_OFFSET(HoverboardVehicle, radiusX, 0xF8);
CHECK_SIZE(HoverboardVehicle, 0x110);

// Kind 6: the character wrestling a creature in a rolling ball (vtable g_WrestleVehicleVTable, 0x250 bytes; the HUD's
// slider). Who's on top and the creature's tactic with their timers, the ball's radius, its heading, the wobble, home, each
// one's side of the ball, the pin, the two pushes of the frame, the struggle and its rounds
class WrestleVehicle : public Vehicle
{
public:
    enum State : s32
    {
        StateRolling = 0,
        StateCharacterOnTop = 1,
        StateCreatureOnTop = 2,
        StateCharacterPins = 3,
        StateCreaturePins = 4,
    };

    enum Tactic : s32
    {
        TacticNone = 0,
        TacticHeadHome = 1,
        TacticHeadHomeByVelocity = 2,
        TacticStruggle = 3,
        TacticPress = 4,
    };

    // The struggle's rounds (the last one presses)
    static constexpr s32 Rounds = 6;
    static constexpr s32 LastRound = Rounds - 1;

    s32 state;
    s32 tactic;
    f32 radius;
    u8 unusedEC[4];
    f32 stateTime;
    f32 tacticTime;
    u8 unusedF8[8];
    Vector4 heading;
    // 0 when it starts, never read
    u32 unused110;
    u8 unused114[0xC];
    Vector4 lastVelocity;
    Vector4 wobbleAxis;
    f32 wobble;
    u8 unused144[0xC];
    Vector4 home;
    Vector4 characterSide;
    Vector4 creatureSide;
    Matrix4x4 squash;
    Vector4 squashOffset;
    f32 wobblePhase;
    u8 unused1D4[0xC];
    Vector4 pinPosition;
    Vector4 pinFrom;
    Vector4 pinTo;
    Vector4 characterPush;
    Vector4 creaturePush;
    f32 struggleX;
    f32 struggleZ;
    u8 resting;
    u8 unused239[3];
    s32 round;
    f32 restTime;
    u8 unused244[0xC];

    // Made for the character and the creature's agent, then Start
    static WrestleVehicle* Construct(WrestleVehicle* vehicle, CharacterAgent* agent, Agent* creature) RETAIL(FUN_001549c8);
    void Start() RETAIL(FUN_00154a28);
    u32 Place() RETAIL(FUN_00155220);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001612d8);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00161420);
    void Frame(f32 seconds) RETAIL_N32(FUN_00154f60);
    // 6
    u32 Kind() RETAIL(FUN_00161130);
    // -1 to 1
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(func_00161138);
    // The body stopped; the impulse meant to start it has its arguments swapped (retail: no impulse)
    void SetVelocity(const Vector4* velocity) RETAIL(FUN_00161368);
    // No
    u32 HeadFollowsCamera() RETAIL(FUN_00161170);
    // Yes
    u32 HasBody() RETAIL(FUN_00161178);

    void RollFrame(f32 seconds) RETAIL_N32(FUN_00153ac8);
    // (The frame's seconds are passed and not read)
    void PinnedFrame() RETAIL(FUN_00153c00);
    void StepState() RETAIL(FUN_00153cd0);
    // A pin turning the character's up onto a direction
    void StartPin(const Vector4* up) RETAIL(FUN_00154d50);
    void StepTactic() RETAIL(FUN_00154800);
    void HeadHome() RETAIL(FUN_00153ed0);
    void Struggle() RETAIL(FUN_00154148);
    void PressDown() RETAIL(FUN_00154458);
    void StickPush(u32 touching) RETAIL(FUN_001531d8);
    void CreaturePush(const Vector4* push) RETAIL(FUN_00153468);
    void Drag() RETAIL(FUN_00153570);
    void SpinDrag() RETAIL(FUN_00153688);
    void Wobble(f32 seconds) RETAIL_N32(FUN_00153770);
    // The body's contact: the surface's message sent to the character
    void TouchedSurface(const CollisionHit* triangle) RETAIL(FUN_00161490);
    // The body's touch: nothing
    void TouchedInstance(InstanceContext* other, u32 hull) RETAIL(FUN_001614f8);
    static void ContactCallback(const CollisionHit* triangle, void* vehicle) RETAIL(func_00161468);
    static void TouchCallback(InstanceContext* other, u32 hull, void* vehicle) RETAIL(func_001614C8);
};
CHECK_OFFSET(WrestleVehicle, state, 0xE0);
CHECK_OFFSET(WrestleVehicle, stateTime, 0xF0);
CHECK_OFFSET(WrestleVehicle, home, 0x150);
CHECK_OFFSET(WrestleVehicle, squash, 0x180);
CHECK_OFFSET(WrestleVehicle, pinPosition, 0x1E0);
CHECK_OFFSET(WrestleVehicle, characterPush, 0x210);
CHECK_OFFSET(WrestleVehicle, round, 0x23C);
CHECK_SIZE(WrestleVehicle, 0x250);

// Kind 7: the character clinging to a wall and sliding down it (vtable g_WallClingVehicleVTable, 0x160 bytes; ClingToWall makes
// it). Its place, velocity, hull and the wall's direction
class WallClingVehicle : public Vehicle
{
public:
    Matrix4x4 matrix;
    Vector4 velocity;
    CollisionHull hull;
    Vector4 wallNormal;

    // Made for the character: its hull made, then Start
    static WallClingVehicle* Construct(WallClingVehicle* vehicle, CharacterAgent* agent) RETAIL(FUN_001606f8);
    void Start() RETAIL(FUN_001607f0);
    // Every matrix its own
    u32 Place() RETAIL(func_001609E0);
    void Destroy(u32 destroyFlags) RETAIL(FUN_00160768);
    // Its own velocity pushed (the instance isn't checked for none)
    void Push(const Vector4* velocity, InstanceContext* instance) RETAIL(FUN_00160af8);
    u32 CanChangeChunk(ChunkData* from, ChunkLinkData* link) RETAIL(FUN_00160a58);
    void Frame(f32 seconds) RETAIL_N32(FUN_001608d8);
    // 7
    u32 Kind() RETAIL(FUN_00160690);
    // Its own velocity
    u32 Velocity(Vector4* velocity) RETAIL(func_001606D0);
    // -1 to 1
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(func_00160698);
    // Ignored
    void SetVelocity(const Vector4* velocity) RETAIL(FUN_001606e0);
    // Nothing
    void Draw() RETAIL(FUN_00160b78);
    // No
    u32 HasBody() RETAIL(FUN_001606e8);

    void Move(f32 seconds) RETAIL_N32(FUN_001502c8);
    // Whether the cling ends (it landed, or the wall ended)
    u32 Collide(f32 seconds) RETAIL_N32(FUN_001504b0);
    // Its z axis turned along the wall's direction
    void FaceWall() RETAIL(FUN_00150880);
};
CHECK_OFFSET(WallClingVehicle, matrix, 0xE0);
CHECK_OFFSET(WallClingVehicle, velocity, 0x120);
CHECK_OFFSET(WallClingVehicle, hull, 0x130);
CHECK_OFFSET(WallClingVehicle, wallNormal, 0x150);
CHECK_SIZE(WallClingVehicle, 0x160);

// Kind 8: the other character riding along on a Rollerbrawl or a Humiliskate (vtable g_PassengerVehicleVTable, 0xF0 bytes; never
// started, the driver's vehicle places it). Its kind (8, as made)
class PassengerVehicle : public Vehicle
{
public:
    u32 kind;
    u8 unusedE4[0xC];

    // Made for a kind, the passenger and the driver
    static PassengerVehicle* Construct(PassengerVehicle* vehicle, u32 kind, CharacterAgent* agent, CharacterAgent* driver)
        RETAIL(FUN_00161870);
    // Nothing to place
    u32 Place() RETAIL(FUN_00161868);
    void Destroy(u32 destroyFlags) RETAIL(FUN_001618b0);
    // Ignored
    void Push(const Vector4* velocity, InstanceContext* instance) RETAIL(FUN_00161818);
    u32 Kind() RETAIL(FUN_00161820);
    // -1 to 1
    void CollisionBox(Vector4* min, Vector4* max) RETAIL(func_00161828);
    // No
    u32 HasBody() RETAIL(FUN_00161860);
    // The driver's (the SplashPoint of its instance's character agent)
    void SplashSphere(Vector4* centre, f32* radius) RETAIL(FUN_00161920);
};
CHECK_OFFSET(PassengerVehicle, kind, 0xE0);
CHECK_SIZE(PassengerVehicle, 0xF0);

extern "C"
{
    extern const GccVTableEntry g_VehicleVTable[] RETAIL(VehicleBase_Methods);
    extern const GccVTableEntry g_RollerbrawlVehicleVTable[] RETAIL(RollerbrawlVehicle_Methods);
    extern const GccVTableEntry g_HumiliskateVehicleVTable[] RETAIL(HumiliskateVehicle_Methods);
    extern const GccVTableEntry g_HoverboardVehicleVTable[] RETAIL(HoverboardVehicle_Methods);
    extern const GccVTableEntry g_WrestleVehicleVTable[] RETAIL(D_002F3420);
    extern const GccVTableEntry g_WallClingVehicleVTable[] RETAIL(D_002F34B8);
    extern const GccVTableEntry g_PassengerVehicleVTable[] RETAIL(D_002F3388);
    // The wrestle's rounds: how long the creature struggles and rests in each (WrestleVehicle::round indexes them)
    extern const f32 g_WrestleStruggleTimes[WrestleVehicle::Rounds] RETAIL(D_002F3908);
    extern const f32 g_WrestleRestTimes[WrestleVehicle::Rounds] RETAIL(D_002F3920);
}

// The HUD's gauge (the wrestle's balance) is player.h's VehicleGauge
