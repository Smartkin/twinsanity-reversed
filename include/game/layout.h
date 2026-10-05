#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "game/properties.h"
#include "game/resources.h"
#include "game/string.h"
#include "gcc2.h"

class Stream;

// A growable array of IDs (an object instance's lists of the instances, positions and paths of its layout it names, a trigger's
// instances, a template's behaviour starters): like the pointer arrays, grown by its growth when full
struct IdArray
{
    u16* data;
    u32 count;
    u32 capacity;
    u32 growth;
};
CHECK_SIZE(IdArray, 0x10);

// The retail walk over an ID array (vtable D_003033E0 over its base D_00303430, whose functions but the destructor are abstract;
// 0xC bytes): the array and the index it's at, done outside the array. The C++ walks the arrays with loops
struct IdArrayIterator
{
    const GccVTableEntry* vtable;
    IdArray* array;
    s32 index;

    // Back to the base's vtable (both)
    void Destroy(u32 destroyFlags) RETAIL(FUN_00262a50);
    void BaseDestroy(u32 destroyFlags) RETAIL(FUN_00262a20);
    void First() RETAIL(FUN_00262a80);
    u32 IsDone() RETAIL(FUN_00262a88);
    u16* Current() RETAIL(FUN_00262ac8);
    void Next() RETAIL(FUN_00262ab8);
    void Previous() RETAIL(FUN_00263a68);
    void Last() RETAIL(FUN_00263a78);
    IdArrayIterator* Assign(const IdArrayIterator* other) RETAIL(FUN_00263a90);

    // The walk over a trigger's instances the layouts' readers use (D_00303AF0, its base D_00303B48; game/layoutiterators.cpp)
    void TriggerIdsDestroy(u32 destroyFlags) RETAIL(FUN_0026aa58);
    void TriggerIdsBaseDestroy(u32 destroyFlags) RETAIL(FUN_0026aa28);
    void TriggerIdsFirst() RETAIL(FUN_0026aa88);
    u32 TriggerIdsIsDone() RETAIL(FUN_0026aa90);
    u16* TriggerIdsCurrent() RETAIL(FUN_0026aad0);
    void TriggerIdsNext() RETAIL(FUN_0026aac0);
    void TriggerIdsPrevious() RETAIL(FUN_0026b8e8);
    void TriggerIdsLast() RETAIL(FUN_0026b8f8);
    u16* TriggerIdsCurrentAgain() RETAIL(FUN_0026b910);
    IdArrayIterator* TriggerIdsAssign(const IdArrayIterator* other) RETAIL(FUN_0026b928);
};
CHECK_SIZE(IdArrayIterator, 0xC);

// An object instance of a layout (0x60 bytes): where it is, its rotation (three angles), the instances, positions and paths of its
// layout it names (by their index in the layout's lists), its properties (its own when it says so), the object it's an instance of,
// its index in the starter's receivers (-1 none) and the behaviour starter its spawn runs
struct ObjectInstance
{
    Vector4 position;
    TaggedValue rotation[3];
    IdArray instances;
    IdArray positions;
    IdArray paths;
    PropertyList* properties;
    s16 objectId;
    s16 refListIndex;
    s16 spawnScript;
    u8 ownsProperties;
    u8 unused57[0x60 - 0x57];

    // Made empty (lists with room for 10, no IDs), and made then read from a stream
    static ObjectInstance* ConstructEmpty(ObjectInstance* instance) RETAIL(InitGameInstance_);
    static ObjectInstance* Construct(ObjectInstance* instance, Stream* stream) RETAIL(InitInstance);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025efd8);
    // Its place, rotation, lists, IDs and properties read (what it had before let go)
    void Read(Stream* stream) RETAIL(ReadInstance);
    // Given properties it doesn't own (its own destroyed first)
    void UseProperties(PropertyList* list) RETAIL(CopyInstProps_);
};
CHECK_OFFSET(ObjectInstance, instances, 0x1C);
CHECK_OFFSET(ObjectInstance, properties, 0x4C);
CHECK_OFFSET(ObjectInstance, ownsProperties, 0x56);
CHECK_SIZE(ObjectInstance, 0x60);

// An instance template of a layout (0x48 bytes; a table nothing looks up keeps them): its name, the header bytes the tools copied
// from its object (its exit points and react joints, its subtype and type), the object, its behaviour starters (made with new[], undefined
// IDs) and its properties
struct InstanceTemplate
{
    String name;
    s8 exitPoints;
    s8 reactJoints;
    s16 objectId;
    s8 objectSubType;
    s8 objectType;
    u8 unused12[2];
    IdArray starters;
    PropertyList properties;

    static InstanceTemplate* Construct(InstanceTemplate* instanceTemplate, Stream* stream) RETAIL(FUN_00261b68);
    void Read(Stream* stream) RETAIL(FUN_00263978);
};
CHECK_OFFSET(InstanceTemplate, starters, 0x14);
CHECK_OFFSET(InstanceTemplate, properties, 0x24);
CHECK_SIZE(InstanceTemplate, 0x48);

// A trigger's header (TT Lab's TriggerFlags past its kind)
union TriggerHeader
{
    u32 value;
    struct
    {
        // 0 makes the trigger a plain box of its chunk the sound code tests the player against instead (a camera trigger's is its
        // priority; 50 on most)
        u32 kind : 8;
        // Its messages sent: the second to what enters, the third to what enters or stays, the fourth to what leaves, the first to
        // what enters first since it was reset
        u32 onEnter : 1;
        u32 onStay : 1;
        u32 onExit : 1;
        u32 onEnterOnce : 1;
        // Its box is never checked (the characters inside check it)
        u32 notPolled : 1;
        u32 unused13 : 19;
    };
};
CHECK_SIZE(TriggerHeader, 4);

// A trigger of a layout (the base of the message triggers' and the cameras' classes, 0x60 bytes; vtable 0x50 bytes in: 1 the
// destructor, 2 the read, 3 its section's item type): its header, the objects that set it off (a bit per object type,
// GameObject::Type), the seconds between two checks of its box, its box's rotation, place and size, and the instances of its layout
// it tells (by their index)
class LayoutTrigger
{
public:
    TriggerHeader header;
    u32 activators;
    f32 checkInterval;
    u32 unused0C;
    Vector4 rotation;
    Vector4 position;
    Vector4 scale;
    IdArray instances;
    const GccVTableEntry* vtable;
    u8 unused54[0x60 - 0x54];

    // Made with no instances (room for 10)
    static LayoutTrigger* Construct(LayoutTrigger* trigger) RETAIL(InitTrigger);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0026ee58);
    void Read(Stream* stream) RETAIL(ReadBaseTrigger);
};
CHECK_OFFSET(LayoutTrigger, rotation, 0x10);
CHECK_OFFSET(LayoutTrigger, instances, 0x40);
CHECK_OFFSET(LayoutTrigger, vtable, 0x50);
CHECK_SIZE(LayoutTrigger, 0x60);

// A trigger that sends its instances messages (retail's GameTrigger, 0x70 bytes): the four messages' halfwords
class MessageTrigger : public LayoutTrigger
{
public:
    static constexpr u32 TypeId = 0x1813;

    s16 messages[4];
    u8 unused68[0x70 - 0x68];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00209c78);
    void Read(Stream* stream) RETAIL(ReadTrigger);
    u32 ItemType() RETAIL(FUN_00209c68);
};
CHECK_SIZE(MessageTrigger, 0x70);

// A camera's trigger (retail's GameCamera, 0x70 bytes): the camera it switches to (its destructor doesn't let it go)
class CameraTrigger : public LayoutTrigger
{
public:
    static constexpr u32 TypeId = 0x1C00;

    struct MainCamera* camera;
    u8 unused64[0x70 - 0x64];

    void Destroy(u32 destroyFlags) RETAIL(FUN_0027e218);
    void Read(Stream* stream) RETAIL(ReadCamera);
    u32 ItemType() RETAIL(FUN_0027e210);
};
CHECK_SIZE(CameraTrigger, 0x70);

// The plain box a trigger of kind 0 makes for its chunk (0xC0 bytes; the sound code tests the player against them): its corners
// in its own space (the trigger's size, negated for the first), its place, its matrix (the trigger's rotation and place) and that
// matrix's inverse, and the squared radius of a sphere holding it
struct SoundBox
{
    Vector4 min;
    Vector4 max;
    Vector4 position;
    Matrix4x4 matrix;
    Matrix4x4 inverse;
    f32 radiusSquared;
    u8 unusedB4[0xC0 - 0xB4];

    static SoundBox* Construct(SoundBox* box, const LayoutTrigger* trigger) RETAIL(FUN_001dd4f0);
    // Whether a point is in it: within its radius of its position, then inside its box through its inverse matrix
    u32 Contains(const Vector4* point) RETAIL(FUN_001e5370);
};
CHECK_OFFSET(SoundBox, inverse, 0x70);
CHECK_SIZE(SoundBox, 0xC0);

// A position of a layout (0x10 bytes)
struct LayoutPosition
{
    Vector4 position;

    void Read(Stream* stream) RETAIL(FUN_0023c760);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0023c738);
};
CHECK_SIZE(LayoutPosition, 0x10);

// A list of points (the base of the paths; vtable 8 bytes in: 1 the destructor, 2 every point transformed by a matrix, 3 the read,
// 4 its section's item type): their count and the points
class PointList
{
public:
    enum Slot : u32
    {
        DestroySlot = 1,
        ReadSlot = 3,
    };

    static constexpr u32 TypeId = 0x1511;

    s32 count;
    Vector4* points;
    const GccVTableEntry* vtable;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0018ea10);
    void Transform(const Matrix4x4* matrix) RETAIL(FUN_0018eb28);
    void Read(Stream* stream) RETAIL(ReadPathPoints);
    u32 ItemType() RETAIL(FUN_0018e6c8);
};
CHECK_SIZE(PointList, 0xC);

// A path of a layout (0x50 bytes; the cameras' path subtype has one too): its points and its parameters, a count of floats in two
// halves (every segment's arc length from the start, then 1 over the steps it takes), and where a nearest point search stands in
// the segment it searches: the point it searches for, the segment's point nearest it so far with its distance squared and its
// share of the segment, and the segment (-1 none yet)
class LayoutPath : public PointList
{
public:
    static constexpr u32 TypeId = 0x1512;

    f32* lengths;
    f32* steps;
    u8 unused14[0xC];
    Vector4 searchPoint;
    Vector4 nearest;
    f32 nearestDistance;
    f32 nearestShare;
    s32 searchSegment;
    u32 unused4C;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0018e758);
    void Read(Stream* stream) RETAIL(ReadPath);
    u32 ItemType() RETAIL(FUN_0018e750);
};
CHECK_OFFSET(LayoutPath, searchPoint, 0x20);
CHECK_OFFSET(LayoutPath, nearestDistance, 0x40);
CHECK_OFFSET(LayoutPath, searchSegment, 0x48);
CHECK_SIZE(LayoutPath, 0x50);

// What a collision surface does on contact, or a damage the scripts' commands deal (0x20 bytes, the tools' two vectors): a point
// (the default box's corner when made), the kinds of hit it is (bits the scripts' hit conditions test) and the damage
struct ContactMessage
{
    Vector4 point;
    u32 hitKinds;
    u8 damage;
    u8 unused15[0x20 - 0x15];

    static ContactMessage* Construct(ContactMessage* message) RETAIL(FUN_0011c1e8);
};
CHECK_SIZE(ContactMessage, 0x20);

// The kinds of hit of a contact message (ContactMessage::hitKinds, TT Lab's ContactKinds; named after what sends them, else
// after the AgentLab tool's keywords the damage commands add them by): explosions, the fall-through death surfaces, burning
// (particles, flames, lava; a character's splash into water too), the iceteroid, projectiles, kind 6, electric shocks, kind 8,
// the generic hit (instant death, what agents hit back with, a crush), a crush, kinds 12 to 14, a bite, the characters' spins and
// kicks, kind 18, heavy hits (Cortex's blast and laser, the mecha's rockets, the stomp kick), kinds 21 and 22, sinking (lava,
// drowning), the knee drop's landing and water. The hit conditions test them; nothing in retail sends kinds 18 and 22 (the
// scripts' CreateDamage could)
enum HitKind : u32
{
    HitExplosion = 1u << 1,
    HitFallingThrough = 1u << 2,
    HitBurning = 1u << 3,
    HitIceteroid = 1u << 4,
    HitProjectile = 1u << 5,
    HitKind6 = 1u << 6,
    HitElectric = 1u << 7,
    HitKind8 = 1u << 8,
    HitGeneric = 1u << 10,
    HitCrush = 1u << 11,
    HitKind12 = 1u << 12,
    HitKind13 = 1u << 13,
    HitKind14 = 1u << 14,
    HitBite = 1u << 15,
    HitSpin = 1u << 16,
    HitKick = 1u << 17,
    HitKind18 = 1u << 18,
    HitHeavy = 1u << 19,
    HitKind21 = 1u << 21,
    HitKind22 = 1u << 22,
    HitSinking = 1u << 23,
    HitKneeDrop = 1u << 24,
    HitWater = 1u << 25,
};

// A collision surface's flags (TT Lab's SurfaceCollisionFlags): a bit for the ray casts and box queries that take it as solid
// (their masks: the player's probes, the follow camera's, objects' and rigid bodies', the lines of sight), what touching it does
// and the ground it is
union SurfaceFlags
{
    // The masks of the casts and queries that take a surface as solid: the player's probes (the characters' casts, the agents'
    // and vehicles' collision caches), the follow camera, objects (and rigid bodies), the lines of sight, and the player
    enum Mask : u32
    {
        SolidToPlayerProbes = 0x10,
        BlocksCamera = 0x20,
        SolidToObjects = 0x40,
        BlocksLineOfSight = 0x80,
        SolidToPlayer = 0x100000,
    };

    u32 value;
    struct
    {
        u32 unused0 : 4;
        u32 solidToPlayerProbes : 1;
        u32 blocksCamera : 1;
        // (Water otherwise)
        u32 solidToObjects : 1;
        u32 blocksLineOfSight : 1;
        // A rigid body touching it hands its agent the surface's contact message, and so does the player standing on it
        u32 sendsContactMessageToObjects : 1;
        u32 sendsContactMessageToPlayer : 1;
        // The rollerbrawl's wheels stick to it
        u32 sticky : 1;
        // Soft ground: footprints and skid marks are left on it, the walls Nina clings to
        u32 soft : 1;
        // (Set on every surface)
        u32 unused12 : 8;
        u32 solidToPlayer : 1;
        u32 unused21 : 11;
    };
};
CHECK_SIZE(SurfaceFlags, 4);

// A collision surface's ID of none (a hull's without a surface of its own; the object nodes keep theirs as -1:
// ObjectNode::NoSurface)
constexpr u16 NoSurfaceId = 0xFFFF;

// The surfaces of the default chunk the code tells apart by their IDs (TT Lab's SURF_ names): slippy metal, wood, metal, sand
// (the agents told of landings on it), water and ice (the Humiliskate's board's sounds)
enum SurfaceId : u16
{
    SurfaceSlippyMetal = 7,
    SurfaceWood = 8,
    SurfaceMetal = 9,
    SurfaceSand = 10,
    SurfaceWater = 12,
    SurfaceIce = 17,
};

// A collision surface of the default chunk's layout 7 (0x90 bytes, a resource of the game): its flags, the physics parameters
// (the tools' list of ten: five volume scales of the contact kinds, -1 leaving the volume, then how much the characters' velocity
// changes on it a second at most (the sixth: their acceleration on it; a knee slide on it lasts 3.5 over it longer), the friction
// (the seventh), what rigid bodies' restitution is multiplied by (the eighth), how hard steep ground of it pulls the characters
// downhill (the ninth) and from which slope (the tenth: the ground's normal's y below it)), what rigid bodies' rolling and
// spinning friction are multiplied by (1: the RM2 has neither), the velocity it carries the characters along with, what a
// contact does, and the sounds and particles of each kind of contact (the particles index the default chunk's systems)
struct CollisionSurface
{
    // The tools' physics parameters, of which the volume scales (the impact's, the hard impact's, the scrape's, the steps' and
    // the landing's)
    static constexpr u32 PhysicsParameters = 10;
    static constexpr u32 VolumeScales = 5;

    u32 header;
    s32 id;
    SurfaceFlags flags;
    f32 acceleration;
    f32 friction;
    f32 restitution;
    f32 rollFriction;
    f32 spinFriction;
    f32 downhillPull;
    f32 steepNormalY;
    u8 unused28[0x30 - 0x28];
    Vector4 flow;
    ContactMessage contact;
    u16 surfaceId;
    u16 impactSound;
    u16 hardImpactSound;
    u16 scrapeSound;
    u16 stepSound1;
    u16 stepSound2;
    u16 landSound;
    u16 impactParticles;
    u16 hardImpactParticles;
    u16 stepParticles;
    f32 volumeScales[VolumeScales];
    u8 unused88[0x90 - 0x88];

    void Read(Stream* stream) RETAIL(ReadCollisionSurface);
};
CHECK_OFFSET(CollisionSurface, contact, 0x40);
CHECK_OFFSET(CollisionSurface, surfaceId, 0x60);
CHECK_OFFSET(CollisionSurface, volumeScales, 0x74);
CHECK_SIZE(CollisionSurface, 0x90);

// The game's collision surfaces (the default chunk's, copied from its layout as it's read): 128 and the count
struct SurfaceTable
{
    CollisionSurface surfaces[128];
    u32 count;

    // A surface's values copied into the next one (not its header, ID or what the game never reads)
    void Add(const CollisionSurface* surface) RETAIL(CopySurfaceIntoTable);
};
CHECK_OFFSET(SurfaceTable, count, 0x4800);

extern "C"
{
    // The game's surfaces (the count is the next symbol, StoredCollisionSurfaces)
    extern SurfaceTable g_CollisionSurfaces RETAIL(G_CollisionSurfaces);
    // An ID made undefined (UndefinedId)
    void SetUndefinedId(u16* id) RETAIL(SetUndefinedID_);
    // A list of IDs read whose elements are made with new[]: its count, room and growth, then the count's IDs (what it had freed
    // first)
    void ReadNewedIds(IdArray* ids, Stream* stream) RETAIL(FUN_00263c18);
    extern const GccVTableEntry g_LayoutTriggerVTable[] RETAIL(D_00304BA0);
    extern const GccVTableEntry g_MessageTriggerVTable[] RETAIL(GameTrigger_Methods);
    extern const GccVTableEntry g_CameraTriggerVTable[] RETAIL(GameCamera_Methods);
    extern const GccVTableEntry g_PointListVTable[] RETAIL(D_002F5C90);
    extern const GccVTableEntry g_LayoutPathVTable[] RETAIL(D_002F5C28);
    // The point of a path nearest to a position: how far along the path it is
    f32 NearestPointOnPath(LayoutPath* path, const Vector4* position, Vector4* nearest) RETAIL(FUN_0018e840);
    // A path's direction at how far along it
    void PathDirectionAt(f32 along, LayoutPath* path, Vector4* direction) RETAIL_N32(FUN_00189550);
    // A segment's cubic stepped on by its forward differences (each of the first three rows plus the next; the path unused), its
    // direction (a unit one, none when it has no length) and its tangent (the derivative of its uniform cubic B-spline, the w 1) a
    // share into a segment
    void PathDifferencesStep(const LayoutPath* path, Vector4* differences) RETAIL(FUN_0018e920);
    void PathDirectionIn(f32 into, const LayoutPath* path, Vector4* direction, s32 segment) RETAIL_N32(FUN_0018ea78);
    void PathTangentIn(f32 into, const LayoutPath* path, Vector4* tangent, s32 segment) RETAIL_N32(FUN_00189c18);
}
