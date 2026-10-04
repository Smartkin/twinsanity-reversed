#include "game/commands.h"

#include "game/agents.h"
#include "game/camerarig.h"
#include "game/cameras.h"
#include "game/clock.h"
#include "game/colour.h"
#include "game/followcamera.h"
#include "game/gamecontroller.h"
#include "game/layout.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/objectnode.h"
#include "game/oleg.h"
#include "game/place.h"
#include "game/progress.h"
#include "game/reference.h"
#include "game/renderer.h"
#include "game/vehicles.h"
#include "game/widgets.h"

#include <bit>
#include <cstdint>

// The commands that work the cameras: the played character's follow camera, the game's camera rig the cutscenes' commands
// place, the camera shown, and the screen's fade

extern "C"
{
    // What command 592 saves of the follow camera and puts back: its bits 0, 1, 2 and 5, its bit 4, and the rig its lens showed
    extern u8 g_SavedCameraBits[4] RETAIL(D_0030AAB4);
    extern u8 g_SavedCameraBit4 RETAIL(D_0030AAB8);
    extern CameraRig* g_SavedLensRig RETAIL(D_0030AAB0);
    // The height command 583 keeps its camera at, taken from the first command it runs
    extern f32 g_OrbitHeight RETAIL(D_0030A498);
    extern u32 g_OrbitHeightSet RETAIL(D_0030A49C);

    // The colour of the game's table at an index
    void GetColor(u32* colour, s32 index);
    // A colour made of three fractions, alpha 1
    void MakeColour(u32* colour, f32 red, f32 green, f32 blue) RETAIL_N32(FUN_0011bea0);

    // Command 591's parts: what it aims at and the distance from it, the scripted moves started, the field of view's angles, the
    // shot's shares, the framed object's height and size, the distance that frames it and the yaw it's seen from
    void MoveAim(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, Vector4* aim) RETAIL(FUN_00110878);
    f32 DistanceFromAim(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const Vector4* point)
        RETAIL(FUN_00110a60);
    void StartScriptedMoves(const CutsceneCameraMoveCommand* command, ScriptedCameraTarget* target,
                            ScriptedCameraPositioner* positioner, f32 targetSeconds, f32 cameraSeconds) RETAIL_N32(FUN_00110c90);
    void FramingAngles(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const s32* fov, s32* side, s32* half,
                       f32* ratio) RETAIL(FUN_00110e00);
    s32* FramingYaw(s32* yaw, const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const s32* angle, f32 distance,
                    f32 aimDistance) RETAIL_N32(FUN_00110fa8);
    void ShotShares(const CutsceneCameraMoveCommand* command, f32* height, f32* view) RETAIL(FUN_0011fd98);
    void FramedHeight(const CutsceneCameraMoveCommand* command, ReferencedObject* object, Vector4* point, f32 share)
        RETAIL_N32(FUN_0011fea8);
    f32 FramedSize(const CutsceneCameraMoveCommand* command, ReferencedObject* object, f32 share) RETAIL_N32(FUN_0011fef0);
    f32 FramingDistance(const CutsceneCameraMoveCommand* command, f32 size, f32 ratio, f32 viewShare, f32 viewSize)
        RETAIL_N32(FUN_0011ff40);
}

EABI_EXPORT(FUN_0011bea0, MakeColour);
EABI_EXPORT(FUN_00110c90, StartScriptedMoves);
EABI_EXPORT(FUN_00110fa8, FramingYaw);
EABI_EXPORT(FUN_0011fea8, FramedHeight);
EABI_EXPORT(FUN_0011fef0, FramedSize);
EABI_EXPORT(FUN_0011ff40, FramingDistance);

namespace
{
constexpr u32 ShowsFollowCamera = 0;
constexpr u32 ShowsGameRig = 3;
constexpr u32 ShowsCutsceneRig = 4;
// The main camera's flags taking its pitch's, distance's, yaw's and field of view's ends
constexpr u32 SetsPitch = 0x4;
constexpr u32 SetsDistance = 0x8;
constexpr u32 SetsYaw = 0x40;
constexpr u32 SetsFov = 0x80;
// The game rig's script bit that turns its framing's angles the other way
constexpr u32 RigMirrored = 0x8;
// The object nodes' vtable functions giving a designator's instance and position
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 GetDesignatorPositionSlot = 37;
// A vector's length below which it isn't normalized (InverseLength's)
constexpr f32 LengthEpsilon = 0x1.5798ecp-29f;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// An instance's nodes (no instance: read at 0xD4, retail's)
NodeList* NodesOf(InstanceContext* instance)
{
    return reinterpret_cast<NodeList*>(reinterpret_cast<std::uintptr_t>(instance) + offsetof(InstanceContext, nodes));
}

// An object's place (no object: the word at 8, retail's)
ObjectPlace* PlaceOf(const ReferencedObject* object)
{
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(object) + offsetof(ReferencedObject, place);
    return *reinterpret_cast<ObjectPlace* const*>(address);
}

ReferencedObject* ObjectOf(const Reference* reference)
{
    return reference != nullptr ? reference->object : nullptr;
}

InstanceContext* PlayedInstance()
{
    GameProgress* progress = &G_GameController->progress;
    return progress->Instance(progress->Field(GameProgress::CharacterShift));
}

FollowNode* PlayedFollowNode()
{
    return static_cast<FollowNode*>(GetGameNode(NodesOf(PlayedInstance()), Node16));
}

// A playable character's velocity: its vehicle's while it rides one, else its own
void CharacterVelocity(InstanceContext* character, Vector4* velocity)
{
    auto* node = static_cast<AgentNode*>(GetGameNode(NodesOf(character), NodePlayer));
    auto* agent = static_cast<CharacterAgent*>(node->agent);
    Vehicle* vehicle = agent->vehicle;
    if (vehicle != nullptr)
    {
        vehicle->VelocityVirtual(velocity);
        return;
    }

    *velocity = agent->velocity;
}

// The paths the cutscenes' commands move the game rig's scripted target and positioner along (its words 0xB4 and 0xB8)
LayoutPath*& TargetPathOf(GameCameraRig* rig)
{
    return *reinterpret_cast<LayoutPath**>(&rig->unknownB4);
}

LayoutPath*& CameraPathOf(GameCameraRig* rig)
{
    return *reinterpret_cast<LayoutPath**>(&rig->unknownB8);
}

s32 ClockUnits(f32 seconds)
{
    return static_cast<s32>(seconds * g_ClockUnitsPerSecond);
}

// A place of the cutscenes' commands: the object's (its position worked out from its matrix first), else the one given
Vector4 RigPlace(ReferencedObject* object, const Vector4* place)
{
    if (object == nullptr)
    {
        return *place;
    }

    ObjectPlace* objectPlace = object->place;
    objectPlace->SyncPosition();
    return objectPlace->position;
}

ReferencedObject* DesignatorOf(ObjectNode* node, u32 designator)
{
    return CallVirtual<ReferencedObject*>(node, node->vtable, GetDesignatorSlot, designator);
}

bool DesignatorPosition(ObjectNode* node, u32 designator, Vector4* position)
{
    return CallVirtual<u32>(node, node->vtable, GetDesignatorPositionSlot, designator, position) != 0;
}
}

// The follow camera's distance (bit 1, units) and pitch (bit 0, a tagged angle) given to its blenders
void SetCameraCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 GivesPitch = 0x1;
    constexpr u32 GivesDistance = 0x2;
    FollowCameraPositioner* positioner = &PlayedFollowNode()->camera.rig.ownPositioner;
    if ((value1.raw & GivesDistance) != 0)
    {
        positioner->distance.bits |= AngleBlender::BitHolds;
        positioner->distance.initial = std::bit_cast<f32>(value3);
    }

    if ((value1.raw & GivesPitch) != 0)
    {
        TaggedValue pitch;
        TaggedValue::AngleWith(&pitch, &value2, NodeOf(runner)->PacketProperties());
        positioner->pitch.bits |= AngleBlender::BitHolds;
        positioner->pitch.initial = pitch.raw;
    }
}

// The follow camera's target follows the agent's instance, and the played character again
void CameraFocusObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    InstanceContext* instance = runner->agentNode->owner;
    AssignReference(&PlayedFollowNode()->camera.rig.ownTarget.followed, instance);
}

void CameraStopFocusObjectCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    InstanceContext* played = PlayedInstance();
    auto* follow = static_cast<FollowNode*>(GetGameNode(NodesOf(played), Node16));
    AssignReference(&follow->camera.rig.ownTarget.followed, played);
}

// The agent's instance placed by the follow camera's instance: on a circle of the radius in its x-y plane at an angle that
// swings with the sine of the phase (60 degrees either way about -90), the phase moving on 3 degrees each time, and along its
// z axis at a height the played character's squared distance from the camera pushes up (nearer than the near distance) or
// down (past the far one), kept within the limit; turned to look back at the camera, its z axis the camera's
void CutsceneCameraOp583Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 ThirdOfPi = 0x1.0c1524p+0f;
    constexpr f32 MinusHalfPi = -0x1.921fb6p+0f;
    constexpr f32 PhaseStep = 0x1.aceea2p-5f;
    InstanceContext* played = PlayedInstance();
    auto* follow = static_cast<FollowNode*>(GetGameNode(NodesOf(played), Node16));
    ReferencedObject* cameraInstance = ObjectOf(follow->object);
    // The character's velocity, which nothing reads
    Vector4 unused;
    CharacterVelocity(played, &unused);
    ObjectPlace* playedPlace = PlaceOf(played);
    RotateAndTranslate(playedPlace);
    const f32* player = playedPlace->matrix.m[3];
    f32 playerX = player[0];
    f32 playerY = player[1];
    f32 playerZ = player[2];
    ObjectPlace* cameraPlace = PlaceOf(cameraInstance);
    RotateAndTranslate(cameraPlace);
    const f32* camera = cameraPlace->matrix.m[3];
    f32 dx = playerX - camera[0];
    f32 dy = playerY - camera[1];
    f32 dz = playerZ - camera[2];
    f32 distanceSquared = dx * dx + dy * dy + dz * dz;
    if (g_OrbitHeightSet == 0)
    {
        g_OrbitHeight = value15;
        g_OrbitHeightSet = 1;
    }

    f32 rate = std::bit_cast<f32>(value3);
    if (distanceSquared < value5)
    {
        g_OrbitHeight = g_OrbitHeight + (value5 - distanceSquared) * (1.0f / (value6 - value5)) * rate;
    }
    else if (value6 < distanceSquared)
    {
        g_OrbitHeight = g_OrbitHeight + (value6 - distanceSquared) * (1.0f / (value6 - value5)) * rate;
    }

    g_OrbitHeight = ClampFloat(g_OrbitHeight, -value4, value4);
    value15 = g_OrbitHeight;
    f32 phase = value14;
    s32 angle;
    AngleFrom(&angle, (phase + std::bit_cast<f32>(value7)) * std::bit_cast<f32>(value11), AngleRadians);
    f32 swing = SinOfAngle(&angle);
    s32 around;
    AngleFrom(&around, swing * ThirdOfPi + MinusHalfPi, AngleRadians);
    f32 cosine = CosOfAngle(&around);
    x = std::bit_cast<f32>(value13) * cosine;
    f32 sine = SinOfAngle(&around);
    f32 side = std::bit_cast<f32>(value13) * sine;
    Vector4 offset = {x, side, std::bit_cast<f32>(value2) + g_OrbitHeight, 1.0f};
    y = std::bit_cast<s32>(side);
    value14 = phase + PhaseStep;
    cameraPlace = PlaceOf(cameraInstance);
    RotateAndTranslate(cameraPlace);
    Matrix4x4 frame = cameraPlace->matrix;
    MoveAlongAxes(&frame, &offset);
    Vector4 forward = {-offset.x, -offset.y, -0.0f, offset.w};
    f32 inverse = InverseLength(&forward, LengthEpsilon);
    forward.x = forward.x * inverse;
    forward.y = forward.y * inverse;
    forward.z = forward.z * inverse;
    VuRotateVector(&frame, &forward, &forward);
    Vector4 up = *RowOf(&frame, 2);
    up.w = 1.0f;
    forward.x = -forward.x;
    forward.y = -forward.y;
    forward.z = -forward.z;
    Vector4 sideAxis;
    sideAxis.x = forward.y * up.z - forward.z * up.y;
    sideAxis.y = forward.z * up.x - forward.x * up.z;
    sideAxis.z = forward.x * up.y - forward.y * up.x;
    sideAxis.w = 1.0f;
    inverse = InverseLength(&sideAxis, LengthEpsilon);
    sideAxis.x = sideAxis.x * inverse;
    sideAxis.y = sideAxis.y * inverse;
    sideAxis.z = sideAxis.z * inverse;
    sideAxis.w = 0.0f;
    forward.w = 0.0f;
    *RowOf(&frame, 1) = forward;
    *RowOf(&frame, 0) = sideAxis;
    InstanceContext* instance = runner->agentNode->owner;
    if (SetPlaceMatrix(PlaceOf(instance), &frame) != 0)
    {
        QueueObject(instance);
    }
}

// The agent's instance turned to face the follow camera's instance across the ground (not when it's straight above or below),
// upright
void PlayerFaceTowardsCameraCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr f32 Epsilon = Rounded(5e-05);
    FollowNode* follow = PlayedFollowNode();
    ObjectPlace* cameraPlace = PlaceOf(ObjectOf(follow->object));
    InstanceContext* instance = runner->agentNode->owner;
    RotateAndTranslate(cameraPlace);
    Vector4 camera = *RowOf(&cameraPlace->matrix, 3);
    ObjectPlace* place = PlaceOf(instance);
    RotateAndTranslate(place);
    Vector4 position = *RowOf(&place->matrix, 3);
    Vector4 way;
    way.x = camera.x - position.x;
    way.z = camera.z - position.z;
    way.y = 0.0f;
    way.w = 1.0f;
    if (__builtin_fabsf(way.x) <= Epsilon && __builtin_fabsf(way.z) <= Epsilon)
    {
        return;
    }

    f32 inverse = InverseLength(&way, LengthEpsilon);
    way.x = way.x * inverse;
    way.y = way.y * inverse;
    way.z = way.z * inverse;
    Vector4 up = {0.0f, 1.0f, 0.0f, 0.0f};
    Vector4 side;
    side.x = way.y * up.z - way.z * up.y;
    side.y = way.z * up.x - way.x * up.z;
    side.z = way.x * up.y - way.y * up.x;
    side.w = 0.0f;
    way.w = 0.0f;
    position.w = 1.0f;
    Matrix4x4 matrix;
    *RowOf(&matrix, 0) = side;
    *RowOf(&matrix, 1) = up;
    *RowOf(&matrix, 2) = way;
    *RowOf(&matrix, 3) = position;
    instance = runner->agentNode->owner;
    if (SetPlaceMatrix(PlaceOf(instance), &matrix) != 0)
    {
        QueueObject(instance);
    }
}

// The cutscenes' camera moved: the game rig's scripted target and positioner moved over their times to new ends. With the
// paths the cutscenes' targets gave, a point along each (both along the one path: the camera's point and the target ahead
// of it along the path); without, the camera framing what it aims at (the shot's shares of the object's height and the
// view, at the field of view's distance), looking from the yaw and the pitch (degrees) the command gives
void CutsceneCameraMoveCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    // The command's flags: it keeps its field of view (else it takes the positioner's), what it frames (bits 9-11: 0 the
    // first place, 1 the second, 2 both)
    constexpr u32 KeepsFov = 0x200000;
    constexpr u32 AimShift = 9;
    constexpr u32 AimMask = 0x7;
    constexpr u32 AimsFirst = 0;
    constexpr u32 AimsSecond = 1;
    constexpr u32 AimsBoth = 2;
    constexpr f32 Aspect = Rounded(4.0 / 3.0);
    GameController* controller = G_GameController;
    GameCameraRig* rig = &controller->camera;
    ScriptedCameraTarget* target = &rig->ownTarget;
    ScriptedCameraPositioner* positioner = &rig->ownPositioner;
    s32 fov = positioner->fov;
    if ((flagsAndAngle.raw & KeepsFov) == 0)
    {
        value12 = static_cast<u32>(fov);
    }

    LayoutPath* targetPath = TargetPathOf(rig);
    LayoutPath* cameraPath = CameraPathOf(rig);
    target->path = targetPath;
    positioner->path = cameraPath;
    f32 targetAlong = std::bit_cast<f32>(value10);
    f32 cameraAlong = std::bit_cast<f32>(value11);
    if (targetPath != nullptr || cameraPath != nullptr)
    {
        StartScriptedMoves(this, target, positioner, offset3, offset4);
        // Retail bug: aims 3 to 7 leave the aim as the stack had it (here the origin)
        Vector4 aim = {0.0f, 0.0f, 0.0f, 1.0f};
        if (targetPath == nullptr)
        {
            MoveAim(this, rig, &aim);
            Vector4 place;
            PathPointAt(cameraAlong, cameraPath, &place);
            target->end = aim;
            positioner->end = place;
        }
        else if (cameraPath == nullptr)
        {
            MoveAim(this, rig, &aim);
            Vector4 point;
            PathPointAt(targetAlong, targetPath, &point);
            target->end = point;
            positioner->end = aim;
        }
        else if (targetPath == cameraPath)
        {
            Vector4 place;
            PathPointAt(targetAlong, cameraPath, &place);
            Vector4 direction;
            PathDirectionAt(targetAlong, cameraPath, &direction);
            Vector4 point = {place.x + direction.x, place.y + direction.y, place.z + direction.z, 1.0f};
            target->end = point;
            positioner->end = place;
        }
        else
        {
            Vector4 point;
            PathPointAt(targetAlong, targetPath, &point);
            Vector4 place;
            PathPointAt(cameraAlong, cameraPath, &place);
            target->end = point;
            positioner->end = place;
        }

        positioner->endFov = static_cast<s32>(value12);
        return;
    }

    f32 tangent = TanOfAngle(&fov);
    ReferencedObject* first = rig->firstObject;
    ReferencedObject* second = rig->secondObject;
    f32 viewSize = (tangent + tangent) / Aspect;
    Vector4 aim = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 height = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 back = {0.0f, 0.0f, 0.0f, 1.0f};
    f32 ratio = 0.0f;
    f32 heightShare = 0.0f;
    f32 viewShare = 0.0f;
    // Retail bug: the angles' modes 4 to 7 leave them as the stack had them (here 0)
    s32 side = 0;
    s32 half = 0;
    StartScriptedMoves(this, target, positioner, offset3, offset4);
    MoveAim(this, rig, &aim);
    FramingAngles(this, rig, &fov, &side, &half, &ratio);
    ShotShares(this, &heightShare, &viewShare);
    f32 size = 0.0f;
    u32 framed = flagsAndAngle.raw >> AimShift & AimMask;
    if (framed == AimsSecond)
    {
        FramedHeight(this, second, &height, heightShare);
        size = FramedSize(this, second, heightShare);
    }
    else if (framed == AimsFirst || framed == AimsBoth)
    {
        FramedHeight(this, first, &height, heightShare);
        size = FramedSize(this, first, heightShare);
    }

    f32 distance = FramingDistance(this, size, ratio, viewShare, viewSize) + offset2;
    f32 aimDistance = DistanceFromAim(this, rig, &aim);
    s32 yaw;
    FramingYaw(&yaw, this, rig, &half, distance, aimDistance);
    back.z = -distance;
    s32 pitch;
    AngleFrom(&pitch, offset1, AngleDegrees);
    s32 roll;
    AngleFrom(&roll, 0.0f, AngleRadians);
    Matrix4x4 turn;
    MatrixFromAngles(&turn, &pitch, &yaw, &roll);
    VuRotateVector(&turn, &back, &back);
    rig->MakeFrame();
    Matrix4x4 frame = rig->frame;
    VuRotateVector(&frame, &back, &back);
    Vector4 ahead = {-back.x, -back.y, -back.z, 1.0f};
    s32 turnBack = -side;
    Vector4 up = {0.0f, 1.0f, 0.0f, 1.0f};
    TurnAboutAxis(&ahead, &up, &turnBack, 1);
    Vector4 base = {aim.x + height.x, aim.y + height.y, aim.z + height.z, 1.0f};
    Vector4 place = {base.x + back.x, base.y + back.y, base.z + back.z, 1.0f};
    Vector4 point = {place.x + ahead.x, place.y + ahead.y, place.z + ahead.z, 1.0f};
    target->end = point;
    positioner->end = place;
    positioner->endFov = static_cast<s32>(value12);
}

// The places the cutscenes' commands use (the objects' when there are): the first (by bits 9-11: 0), the second (1), or half
// the way from the first to the second (2)
void MoveAim(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, Vector4* aim)
{
    ReferencedObject* firstObject = rig->firstObject;
    ReferencedObject* secondObject = rig->secondObject;
    Vector4 first = RigPlace(firstObject, &rig->firstPlace);
    Vector4 second = RigPlace(secondObject, &rig->secondPlace);
    switch (command->flagsAndAngle.raw >> 9 & 0x7)
    {
    case 0:
        *aim = first;
        break;
    case 1:
        *aim = second;
        break;
    case 2:
        // Retail bug: half the way from one place to the other, not the point between them (the first isn't added)
        aim->x = (second.x - first.x) * 0.5f;
        aim->y = (second.y - first.y) * 0.5f;
        aim->z = (second.z - first.z) * 0.5f;
        aim->w = 1.0f;
        break;
    default:
        break;
    }
}

// The distance from a point to what bits 12-14 name the same way (else the origin)
f32 DistanceFromAim(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const Vector4* point)
{
    Vector4 first = RigPlace(rig->firstObject, &rig->firstPlace);
    // Retail bug: without a second object the first place stands in for the second
    Vector4 second = RigPlace(rig->secondObject, &rig->firstPlace);
    Vector4 aim = g_DefaultBox.min;
    switch (command->flagsAndAngle.raw >> 12 & 0x7)
    {
    case 0:
        aim = first;
        break;
    case 1:
        aim = second;
        break;
    case 2:
        // Retail bug: half the way from one place to the other again
        aim.x = (second.x - first.x) * 0.5f;
        aim.y = (second.y - first.y) * 0.5f;
        aim.z = (second.z - first.z) * 0.5f;
        break;
    default:
        break;
    }

    f32 dx = point->x - aim.x;
    f32 dy = point->y - aim.y;
    f32 dz = point->z - aim.z;
    return __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
}

// The scripted target's and positioner's moves stopped and timed (seconds; the positioner eases half a second), the
// positioner's ease in (bit 16) and out (17), the curve of both (bits 18-20: 0 even, 1 smooth, else as they were) and its arc
// (15); the command's word 0x2C given to the positioner
void StartScriptedMoves(const CutsceneCameraMoveCommand* command, ScriptedCameraTarget* target,
                        ScriptedCameraPositioner* positioner, f32 targetSeconds, f32 cameraSeconds)
{
    constexpr u32 EasesInShift = 16;
    constexpr u32 EasesOutShift = 17;
    constexpr u32 CurveShift = 18;
    constexpr u32 ArcsShift = 15;
    positioner->Stop();
    target->Stop();
    f32 units = g_ClockUnitsPerSecond;
    target->moveTicks = static_cast<s32>(targetSeconds * units);
    positioner->easeTicks = static_cast<s32>(units * 0.5f);
    positioner->moveTicks = static_cast<s32>(cameraSeconds * units);
    positioner->unknown44 = command->unused9;
    u32 bits = static_cast<u32>(command->flagsAndAngle.raw);
    positioner->bits = (positioner->bits & ~ScriptedCameraPositioner::BitEasesIn) | (bits >> EasesInShift & 1);
    positioner->bits = (positioner->bits & ~ScriptedCameraPositioner::BitEasesOut) | (bits >> EasesOutShift & 1) << 1;
    switch (bits >> CurveShift & 0x7)
    {
    case 0:
        target->bits &= ~ScriptedCameraTarget::CurveMask;
        positioner->bits &= ~ScriptedCameraPositioner::CurveMask;
        break;
    case 1:
        target->bits = (target->bits & ~ScriptedCameraTarget::CurveMask)
                       | ScriptedCameraTarget::CurveSmooth << ScriptedCameraTarget::CurveShift;
        positioner->bits = (positioner->bits & ~ScriptedCameraPositioner::CurveMask)
                           | ScriptedCameraPositioner::CurveSmooth << ScriptedCameraPositioner::CurveShift;
        break;
    default:
        break;
    }

    positioner->bits = (positioner->bits & ~ScriptedCameraPositioner::BitArcs) | (bits >> ArcsShift & 1) << 5;
}

// The angles framing takes of the field of view (bits 6-8): 0 none (the ratio 1), 1 a sixth to the side and half of it, 2 a
// third and half, 3 half and two thirds (dividing by 1.5); the ratio is 1 over the side angle's cosine. The side angle is turned
// the other way when the rig is mirrored; other values leave the angles as they were
void FramingAngles(const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const s32* fov, s32* side, s32* half,
                   f32* ratio)
{
    static constexpr f32 Divisors[3][2] = {{6.0f, 2.0f}, {3.0f, 2.0f}, {2.0f, 1.5f}};
    u32 angles = command->flagsAndAngle.raw >> 6 & 0x7;
    if (angles == 0)
    {
        *side = 0;
        *half = 0;
        *ratio = 1.0f;
    }
    else if (angles <= 3)
    {
        s32 angle = *fov;
        *side = *DivideAngle(&angle, Divisors[angles - 1][0]);
        s32 other = *fov;
        *half = *DivideAngle(&other, Divisors[angles - 1][1]);
        *ratio = 1.0f / CosOfAngle(side);
    }

    f32 sign = (rig->scriptBits & RigMirrored) != 0 ? -1.0f : 1.0f;
    *side = static_cast<s32>(static_cast<f32>(*side) * sign);
}

// The yaw the camera frames from (mirrored with the rig): with bits 12-14 at 3 one of the shot's angles (bits 0-2) plus the
// command's (degrees); otherwise the angle whose sine is the distance's share of the angle's sine (over the aim's distance),
// the command's angle and the given one together, the other way round
s32* FramingYaw(s32* yaw, const CutsceneCameraMoveCommand* command, GameCameraRig* rig, const s32* angle, f32 distance,
                f32 aimDistance)
{
    constexpr u32 FixedMask = 0x7000;
    constexpr u32 Fixed = 0x3000;
    constexpr f32 UnitsPerDegree = Rounded(65536.0 / 360.0);
    u32 bits = static_cast<u32>(command->flagsAndAngle.raw);
    s32 value;
    if ((bits & FixedMask) != Fixed)
    {
        s32 aside;
        AngleOfSine(distance * SinOfAngle(angle) / aimDistance, &aside);
        s32 turn;
        AngleFrom(&turn, command->offset6, AngleDegrees);
        value = -(turn + aside + *angle);
    }
    else
    {
        f32 degrees;
        switch (bits & 0x7)
        {
        case 0:
            degrees = -145.0f;
            break;
        case 1:
            degrees = -15.0f;
            break;
        case 2:
            degrees = -180.0f;
            break;
        case 3:
            degrees = -90.0f;
            break;
        default:
            degrees = 0.0f;
            break;
        }

        value = static_cast<s32>((degrees + command->offset6) * UnitsPerDegree);
    }

    *yaw = (rig->scriptBits & RigMirrored) != 0 ? -value : value;
    return yaw;
}

// The shot (bits 3-5): the share of the framed object's height looked at and the view's share it fills
void ShotShares(const CutsceneCameraMoveCommand* command, f32* height, f32* view)
{
    static constexpr f32 Shares[8][2] = {
        {1.0f, Rounded(1.4)},
        {1.0f, Rounded(0.9)},
        {Rounded(0.9), 0.75f},
        {Rounded(0.8), Rounded(0.7)},
        {Rounded(0.6), Rounded(0.6)},
        {Rounded(0.4), 0.5f},
        {0.0f, Rounded(0.35)},
        {0.0f, Rounded(0.2)},
    };
    const f32* shares = Shares[command->flagsAndAngle.raw >> 3 & 0x7];
    *height = shares[0];
    *view = shares[1];
}

// The framed point's height above the object's base: half its own box's height and the share (plus the command's 0x20) of a
// quarter of it (without an object the command's 0x20 alone)
void FramedHeight(const CutsceneCameraMoveCommand* command, ReferencedObject* object, Vector4* point, f32 share)
{
    if (object == nullptr)
    {
        point->y = command->offset5;
        return;
    }

    const Box* box = &object->collision.ownBox;
    f32 height = box->max.y - box->min.y;
    point->y = height * 0.5f + (share + command->offset5) * 0.25f * height;
}

// The size framed: the object's own box's lowest corner's w less the share, and the share of a quarter of its height (1
// without an object)
f32 FramedSize(const CutsceneCameraMoveCommand*, ReferencedObject* object, f32 share)
{
    if (object == nullptr)
    {
        return 1.0f;
    }

    const Box* box = &object->collision.ownBox;
    return box->min.w * (1.0f - share) + (box->max.y - box->min.y) * 0.25f * share;
}

f32 FramingDistance(const CutsceneCameraMoveCommand*, f32 size, f32 ratio, f32 viewShare, f32 viewSize)
{
    return __builtin_fabsf(ratio * size / (viewShare * 0.5f * viewSize));
}

// What command 592 saves (mode 0) and puts back (1): the follow camera's bits 0, 1, 2, 4 and 5 and the rig its lens shows
void CameraSaveParamsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    auto* cameraInstance = static_cast<InstanceContext*>(ObjectOf(follow->object));
    FollowCamera* camera = &follow->camera;
    switch (value1.raw & 0x7)
    {
    case 0:
        g_SavedCameraBits[0] = camera->bits & FollowCamera::BitSmoothed;
        g_SavedCameraBits[1] = camera->bits >> 1 & 1;
        g_SavedCameraBits[2] = camera->bits >> 2 & 1;
        g_SavedCameraBits[3] = camera->bits >> 5 & 1;
        g_SavedCameraBit4 = camera->bits >> 4 & 1;
        g_SavedLensRig = camera->lensRig;
        break;
    case 1:
        camera->lensRig = g_SavedLensRig;
        SetFollowCameraSmoothed(camera, g_SavedCameraBits[0]);
        SetFollowCameraSteers(camera, g_SavedCameraBits[1]);
        SetFollowCameraIgnoresTriggers(camera, g_SavedCameraBits[2]);
        camera->bits = (camera->bits & ~FollowCamera::BitStepsRig) | (g_SavedCameraBits[3] & 1u) << 5;
        camera->bits = (camera->bits & ~FollowCamera::BitRigAway) | (g_SavedCameraBit4 & 1u) << 4;
        PutFollowCameraOnLens(camera, cameraInstance);
        break;
    default:
        break;
    }
}

// The camera shown (mode 0 the follow camera, 1 the game's rig, 2 the cutscenes' rig), blended to over the time when there's one
// (bit 4 sets it back, bits 5-7 the curve); the follow camera put where the game's rig has its camera and looks (bit 3)
void ToggleCutsceneCameraCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    constexpr u32 PlacesFollowCamera = 0x8;
    u32 flags = modeFlags;
    u32 reset = flags >> 4 & 1;
    u32 curve = flags >> 5 & 0x7;
    switch (flags & 0x7)
    {
    case 0:
    {
        if (0.0f < blendTime)
        {
            s32 ticks = ClockUnits(blendTime);
            BlendToCamera(G_GameController, ShowsFollowCamera, &ticks, reset, curve);
        }
        else
        {
            ShowCamera(G_GameController, ShowsFollowCamera, reset);
        }

        if ((modeFlags & PlacesFollowCamera) == 0)
        {
            return;
        }

        CameraRig* rig = &PlayedFollowNode()->camera.rig;
        GameCameraRig* gameRig = &G_GameController->camera;
        Vector4 position = gameRig->ownPositioner.position;
        Vector4 rotation = gameRig->ownPositioner.rotation;
        Vector4 point = gameRig->ownTarget.point;
        rig->positioner->position = position;
        rig->positioner->rotation = rotation;
        rig->target->point = point;
        static_cast<FollowCameraPositioner*>(rig->positioner)->PlaceBehind();
        break;
    }
    case 1:
        if (0.0f < blendTime)
        {
            s32 ticks = ClockUnits(blendTime);
            BlendToCamera(G_GameController, ShowsGameRig, &ticks, 0, curve);
        }
        else
        {
            ShowCamera(G_GameController, ShowsGameRig, 1);
        }

        break;
    case 2:
        ShowCamera(G_GameController, ShowsCutsceneRig, reset);
        break;
    default:
        break;
    }
}

// What the game rig's scripted target and positioner go by: the agent's waypoints' path of a key (0xFF none: the target's in
// byte 0x14, the positioner's in 0x15), else a designator's instance (bytes 0xC and 0xD) or position (0xE and 0xF; the
// place's bit set); bit 0 mirrors the framing, bit 1 asks for the frame
void CutsceneCameraTargetsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 NoKey = 0xFF;
    GameCameraRig* rig = &G_GameController->camera;
    rig->ResetScript();
    ObjectNode* node = NodeOf(runner);
    u8 firstKey = static_cast<u8>(keys);
    if (firstKey < NoKey)
    {
        TargetPathOf(rig) = node->waypoints->paths.data[firstKey];
    }
    else
    {
        ReferencedObject* object = DesignatorOf(node, targets & 0xFF);
        if (object != nullptr)
        {
            rig->ClearFirstPlace();
            rig->firstObject = object;
        }
        else
        {
            Vector4 place;
            if (DesignatorPosition(node, targets >> 16 & 0xFF, &place))
            {
                rig->firstObject = nullptr;
                rig->firstPlace = place;
                rig->scriptBits |= GameCameraRig::BitHasFirstPlace;
            }
        }
    }

    u8 secondKey = static_cast<u8>(keys >> 8);
    if (secondKey < NoKey)
    {
        CameraPathOf(rig) = node->waypoints->paths.data[secondKey];
    }
    else
    {
        ReferencedObject* object = DesignatorOf(node, targets >> 8 & 0xFF);
        if (object != nullptr)
        {
            rig->ClearSecondPlace();
            rig->secondObject = object;
        }
        else
        {
            Vector4 place;
            if (DesignatorPosition(node, targets >> 24, &place))
            {
                rig->secondObject = nullptr;
                rig->secondPlace = place;
                rig->scriptBits |= GameCameraRig::BitHasSecondPlace;
            }
        }
    }

    rig->scriptBits = (rig->scriptBits & ~RigMirrored) | (flags & 1) << 3;
    rig->scriptBits = (rig->scriptBits & ~GameCameraRig::BitFrameWanted) | (flags >> 1 & 1) << 4;
}

// The follow camera's own camera given both ends of one of its values: mode 0 the pitch, 2 the yaw (degrees), 3 the field of
// view (radians), 1 the distance
void SetCameraNodeValuesCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    MainCamera* camera = &follow->camera.rig.ownPositioner.camera;
    switch (mode.raw & 0x7)
    {
    case 0:
    {
        s32 start;
        AngleFrom(&start, value1, AngleDegrees);
        s32 end;
        AngleFrom(&end, value2, AngleDegrees);
        camera->flags |= SetsPitch;
        camera->pitchStart = static_cast<u32>(start);
        camera->pitchEnd = static_cast<u32>(end);
        break;
    }
    case 1:
        camera->flags |= SetsDistance;
        camera->distanceEnd = value2;
        camera->distanceStart = value1;
        break;
    case 2:
    {
        s32 start;
        AngleFrom(&start, value1, AngleDegrees);
        s32 end;
        AngleFrom(&end, value2, AngleDegrees);
        camera->flags |= SetsYaw;
        camera->yawStart = static_cast<u32>(start);
        camera->yawEnd = static_cast<u32>(end);
        break;
    }
    case 3:
    {
        s32 start;
        AngleFrom(&start, value1, AngleRadians);
        s32 end;
        AngleFrom(&end, value2, AngleRadians);
        camera->flags |= SetsFov;
        camera->fovStart = static_cast<u32>(start);
        camera->fovEnd = static_cast<u32>(end);
        break;
    }
    default:
        break;
    }
}

// The follow camera made to look down on the character from above: its own camera's second part a camera around a point at
// the character's height, at 60 degrees of field of view
void CameraTopdownModeCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    FollowNode* follow = PlayedFollowNode();
    if (follow == nullptr)
    {
        return;
    }

    FollowCameraPositioner* positioner = &follow->camera.rig.ownPositioner;
    auto* camera = static_cast<Camera1C0C*>(MemoryAllocate(sizeof(Camera1C0C)));
    CameraSubtype::Construct(camera);
    camera->unknown40[0] = 1;
    camera->vtable = g_Camera1C0CVTable;
    camera->radius = 11.0f;
    camera->farHeight = 16.0f;
    camera->farDistance = 20.0f;
    camera->centre = {Rounded(-0.07851), Rounded(1.25772), Rounded(-0.17564), 1.0f};
    camera->nearHeight = 16.0f;
    camera->extra = 0.0f;
    camera->nearDistance = 0.0f;
    s32 fov;
    AngleFrom(&fov, 60.0f, AngleDegrees);
    positioner->camera.flags |= SetsFov;
    positioner->camera.fovStart = static_cast<u32>(fov);
    positioner->camera.fovEnd = static_cast<u32>(fov);
    positioner->camera.second = camera;
    positioner->bits |= FollowCameraPositioner::BitOwnCamera;
    positioner->Clear();
    FollowCameraTarget* target = &follow->camera.rig.ownTarget;
    target->camera.flags = MainCamera::FlagSteers;
    target->bits |= FollowCameraTarget::BitOwnCamera;
    target->Clear();
}

// The screen's fade (OLEG's widgets of slot 1) hidden (mode 0) or shown (1) over the duration, its colours black and the
// command's (bit 3)
void FadeoutScreenCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
    constexpr u32 FadeSlot = 1;
    constexpr u32 SetsColour = 0x8;
    GameController* controller = G_GameController;
    OLEG* oleg = &controller->oleg;
    u32 colour;
    MakeColour(&colour, red, green, blue);
    switch (flags & 0x7)
    {
    case 0:
        oleg->Hide(oleg->masks[FadeSlot], ClockUnits(duration), 0);
        break;
    case 1:
        oleg->Show(oleg->masks[FadeSlot], ClockUnits(duration), 0);
        break;
    default:
        return;
    }

    if ((flags & SetsColour) != 0)
    {
        u32 black;
        GetColor(&black, 0);
        oleg->sprite13B8.shownColour = colour;
        oleg->sprite13B8.hiddenColour = black;
    }
}

void GetColor(u32* colour, s32 index)
{
    *colour = g_Colours[index];
}

void MakeColour(u32* colour, f32 red, f32 green, f32 blue)
{
    auto* bytes = reinterpret_cast<u8*>(colour);
    bytes[0] = Colour::ColourByte(red);
    bytes[1] = Colour::ColourByte(green);
    bytes[2] = Colour::ColourByte(blue);
    bytes[3] = Colour::AlphaByte(1.0f);
}
