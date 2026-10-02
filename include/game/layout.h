#pragma once

#include "abi.h"
#include "common.h"
#include "game/math.h"
#include "game/properties.h"
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
    u8 unknown57[0x60 - 0x57];

    // Made empty (lists with room for 10, no IDs), and made then read from a stream
    static ObjectInstance* ConstructEmpty(ObjectInstance* instance) RETAIL(InitGameInstance_);
    static ObjectInstance* Construct(ObjectInstance* instance, Stream* stream) RETAIL(InitInstance);
    void Destroy(u32 destroyFlags) RETAIL(FUN_0025efd8);
    // Its place, rotation, lists, IDs and properties read (what it had before let go)
    void Read(Stream* stream) RETAIL(ReadInstance);
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
    u8 unknown12[2];
    IdArray starters;
    PropertyList properties;

    static InstanceTemplate* Construct(InstanceTemplate* instanceTemplate, Stream* stream) RETAIL(FUN_00261b68);
    void Read(Stream* stream) RETAIL(FUN_00263978);
};
CHECK_OFFSET(InstanceTemplate, starters, 0x14);
CHECK_OFFSET(InstanceTemplate, properties, 0x24);
CHECK_SIZE(InstanceTemplate, 0x48);

// A trigger of a layout (the base of the message triggers' and the cameras' classes, 0x60 bytes; vtable 0x50 bytes in: 1 the
// destructor, 2 the read, 3 its section's item type): its header (its low byte the kind: 0 makes it a plain box of its chunk the
// sound code tests the player against instead of a trigger), the objects that set it off (a mask), the seconds between two checks
// of its box, its box's rotation, place and size, and the instances of its layout it tells (by their index)
class LayoutTrigger
{
public:
    u32 header;
    u32 activators;
    f32 checkInterval;
    u32 unknown0C;
    Vector4 rotation;
    Vector4 position;
    Vector4 scale;
    IdArray instances;
    const GccVTableEntry* vtable;
    u8 unknown54[0x60 - 0x54];

    // Its kind (the header's low byte)
    u8 Kind() const
    {
        return static_cast<u8>(header);
    }

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
    s16 messages[4];
    u8 unknown68[0x70 - 0x68];

    void Destroy(u32 destroyFlags) RETAIL(FUN_00209c78);
    void Read(Stream* stream) RETAIL(ReadTrigger);
    u32 ItemType() RETAIL(FUN_00209c68);
};
CHECK_SIZE(MessageTrigger, 0x70);

// A camera's trigger (retail's GameCamera, 0x70 bytes): the camera it switches to (its destructor doesn't let it go)
class CameraTrigger : public LayoutTrigger
{
public:
    struct MainCamera* camera;
    u8 unknown64[0x70 - 0x64];

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
    u8 unknownB4[0xC0 - 0xB4];

    static SoundBox* Construct(SoundBox* box, const LayoutTrigger* trigger) RETAIL(FUN_001dd4f0);
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
// halves (every segment's arc length from the start, then 1 over the steps it takes)
class LayoutPath : public PointList
{
public:
    f32* lengths;
    f32* steps;
    u8 unknown14[0x48 - 0x14];
    s32 unknown48;
    u32 unknown4C;

    void Destroy(u32 destroyFlags) RETAIL(FUN_0018e758);
    void Read(Stream* stream) RETAIL(ReadPath);
    u32 ItemType() RETAIL(FUN_0018e750);
};
CHECK_OFFSET(LayoutPath, unknown48, 0x48);
CHECK_SIZE(LayoutPath, 0x50);

// What a collision surface does on contact, or a damage the scripts' commands deal (0x20 bytes, the tools' two vectors): a point
// (the default box's corner when made), a word and a byte
struct ContactMessage
{
    Vector4 point;
    u32 word;
    u8 byte;
    u8 unknown15[0x20 - 0x15];

    static ContactMessage* Construct(ContactMessage* message) RETAIL(FUN_0011c1e8);
};
CHECK_SIZE(ContactMessage, 0x20);

// A collision surface of the default chunk's layout 7 (0x90 bytes, a resource of the game): the objects that collide with it (a
// bit per ray cast, game/enums), the physics parameters (the tools' list of ten: five volume scales of the contact kinds, -1
// leaving the volume, then the friction (the seventh), the rest never read), two values made 1, the vector nothing reads, what a
// contact does, and the sounds and particles of each kind of contact (the particles index the default chunk's systems)
struct CollisionSurface
{
    u32 header;
    s32 id;
    u32 collisionMask;
    f32 physics5;
    f32 friction;
    f32 physics7;
    f32 unknown18;
    f32 unknown1C;
    f32 physics8;
    f32 physics9;
    u8 unknown28[0x30 - 0x28];
    Vector4 unusedVector;
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
    f32 volumeScales[5];
    u8 unknown88[0x90 - 0x88];

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
    // An ID made undefined (0xFFFF)
    void SetUndefinedId(u16* id) RETAIL(SetUndefinedID_);
    // A list of IDs read whose elements are made with new[]: its count, room and growth, then the count's IDs (what it had freed
    // first)
    void ReadNewedIds(IdArray* ids, Stream* stream) RETAIL(FUN_00263c18);
    extern const GccVTableEntry g_LayoutTriggerVTable[] RETAIL(D_00304BA0);
    extern const GccVTableEntry g_MessageTriggerVTable[] RETAIL(GameTrigger_Methods);
    extern const GccVTableEntry g_CameraTriggerVTable[] RETAIL(GameCamera_Methods);
    extern const GccVTableEntry g_PointListVTable[] RETAIL(D_002F5C90);
    extern const GccVTableEntry g_LayoutPathVTable[] RETAIL(D_002F5C28);
    // The point of a path nearest to a position: how far along the path it is (still asm)
    f32 NearestPointOnPath(LayoutPath* path, const Vector4* position, Vector4* nearest) RETAIL(FUN_0018e840);
    // A path's direction at how far along it (still asm)
    void PathDirectionAt(f32 along, LayoutPath* path, Vector4* direction) RETAIL_N32(FUN_00189550);
}
