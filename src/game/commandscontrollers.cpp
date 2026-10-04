#include "game/commands.h"

#include "game/animation.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/properties.h"

#include <bit>
#include <cstddef>

// The commands that make and set up the object nodes' controllers (game/nodecontrollers.h)

extern "C"
{
    // The items' builders' base, and the script conditions' builder's destructor (vtable D_002EE368)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    void DestroyConditionBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0011e388);
}

namespace
{
// The object nodes' vtable functions: a designator's instance and position
constexpr u32 GetDesignatorSlot = 36;
constexpr u32 GetDesignatorPositionSlot = 37;

ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// The node's controller when it's of a kind
template <typename Controller>
Controller* ControllerOf(ObjectNode* node, u8 kind)
{
    auto* controller = reinterpret_cast<NodeController*>(node->unknown114);
    if (controller == nullptr || controller->kind != kind)
    {
        return nullptr;
    }

    return static_cast<Controller*>(controller);
}
}

void DestroyConditionBuilder(void* builder, u32 destroyFlags)
{
    *static_cast<const GccVTableEntry**>(builder) = g_ItemBuilderBaseVTable;
    if ((destroyFlags & 1) != 0)
    {
        MemoryDeallocate2_(builder);
    }
}

// The controller of the command's kind made and given to the agent's node (with the 0xFD token only when it has none)
void CreateNodeControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 OnlyWithoutOne = 0x100;
    ObjectNode* node = NodeOf(runner);
    if ((controller & OnlyWithoutOne) != 0 && node->unknown114 != 0)
    {
        return;
    }

    u32 kind = controller & 0xFF;
    NodeController* made;
    switch (kind)
    {
    case NodeController::KindJointAim:
        made = JointAimController::Construct(static_cast<JointAimController*>(MemoryAllocate(sizeof(JointAimController))), node,
                                             kind);
        break;
    case NodeController::KindMask:
        made = MaskController::Construct(static_cast<MaskController*>(MemoryAllocate(sizeof(MaskController))), node, kind);
        break;
    case NodeController::KindSpline:
        made = SplineController::Construct(static_cast<SplineController*>(MemoryAllocate(sizeof(SplineController))), node, kind);
        break;
    case NodeController::KindSkate:
        made = SkateController::Construct(static_cast<SkateController*>(MemoryAllocate(sizeof(SkateController))), node, kind);
        break;
    default:
        return;
    }

    SetNodeController(node, made);
}

// The final boss's weapons (the node's JointAimController) set up by the mode: 0 its three joints (the slots' bytes) hooked on the
// node's model, 1 the weapons (bits 0-2) turned back to their animation's pose or left resting, 2 raised to aim (taking the pose
// they're at), 3 the target (a designator's instance, else its position), 4 the weapons' scale and turn rate
void FinalBossInitWeaponsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 WeaponCount = 3;
    ObjectNode* node = NodeOf(runner);
    // A retail bug: the controller's kind isn't checked, nor whether there's one (then it writes from address 0x10 on)
    auto* aimer = reinterpret_cast<JointAimer*>(node->unknown114 + offsetof(JointAimController, aimer));
    switch (mode)
    {
    case 0:
    {
        for (u32 index = 0; index < WeaponCount; index++)
        {
            aimer->joints[index].id = static_cast<u8>(slots >> (8 * index));
            aimer->joints[index].node = node;
        }

        aimer->node = node;
        auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, ModelNode::NodeKind));
        aimer->AttachVirtual(model->animator);
        break;
    }
    case 1:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons & 1u << index) != 0)
            {
                aimer->joints[index].returning = aimer->joints[index].resting ^ 1;
            }
        }

        break;
    case 2:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons & 1u << index) != 0)
            {
                AimedJoint& aimed = aimer->joints[index];
                u8 resting = aimed.resting;
                aimed.resting = 0;
                aimed.retakesMatrix = resting;
                aimed.returning = 0;
            }
        }

        break;
    case 3:
    {
        auto* instance = CallVirtual<InstanceContext*>(node, node->vtable, GetDesignatorSlot, target & 0xFF);
        if (instance != nullptr)
        {
            aimer->targetInstance = instance;
            aimer->target = JointAimer::TargetInstance;
            break;
        }

        Vector4 position;
        if (CallVirtual<u32>(node, node->vtable, GetDesignatorPositionSlot, target & 0xFF, &position) != 0)
        {
            aimer->target = JointAimer::TargetPosition;
            aimer->targetPosition = position;
        }

        break;
    }
    case 4:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons & 1u << index) != 0)
            {
                aimer->joints[index].scale = std::bit_cast<f32>(value1);
                aimer->joints[index].rate = std::bit_cast<f32>(value2);
            }
        }

        break;
    default:
        break;
    }
}

// The command's IDs added to the mask controller's (from its count on), then its trails' particle systems: the boost trail the
// third ID, the unused one the second, the invincibility's three the first and the two after it (from exit point 5)
void SetMaskControllerIdsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u8 InvincibleExitPoint = 5;
    auto* mask = ControllerOf<MaskController>(NodeOf(runner), NodeController::KindMask);
    if (mask == nullptr)
    {
        return;
    }

    const u16* given = reinterpret_cast<const u16*>(&ids1);
    for (u32 index = 0; index < (count & 0xF); index++)
    {
        // A retail bug: the count is never reset, so IDs given again go past the three (into the boost trail's arguments from
        // the ninth on)
        u32 at = mask->flags >> MaskController::IdCountShift & MaskController::IdCountMask;
        mask->flags = (mask->flags & ~(MaskController::IdCountMask << MaskController::IdCountShift))
                      | ((at + 1) & MaskController::IdCountMask) << MaskController::IdCountShift;
        reinterpret_cast<u16*>(&mask->ids)[at] = given[index];
    }

    SetTrailSystem(&mask->boostTrail, mask->ids[2]);
    SetTrailSystem(&mask->unusedTrail, mask->ids[1]);
    u16 first = mask->ids[0];
    for (u16 index = 0; index < 3; index++)
    {
        SetTrailSystem(&mask->trails[index], static_cast<u16>(first + index));
        mask->trails[index].exitPoint = InvincibleExitPoint;
    }
}

// The spline controller's offset (x, y, z), pull, turn rate, unused value and drop, with the properties the agent's packets read
void SetSplineControllerValuesCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    auto* spline = ControllerOf<SplineController>(node, NodeController::KindSpline);
    if (spline == nullptr)
    {
        return;
    }

    PropertyHolder* properties = node->PacketProperties();
    f32 offsetX = x.FloatWith(properties);
    f32 offsetY = y.FloatWith(properties);
    f32 offsetZ = z.FloatWith(properties);
    f32 pull = value4.FloatWith(properties);
    f32 turnRate = value5.FloatWith(properties);
    f32 unknown28 = value6.FloatWith(properties);
    f32 drop = value7.FloatWith(properties);
    spline->offset = {offsetX, offsetY, offsetZ, 1.0f};
    spline->pull = pull;
    spline->turnRate = turnRate;
    spline->unknown28 = unknown28;
    spline->drop = drop;
}

// The skate controller's trails' particle systems (added to its IDs when it has none or the command says so, every trail given
// its ID) and its sounds (the node's own object's sounds of the slots given, added when it has none or the command says so)
void SetSkateControllerIdsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    constexpr u32 AddAgain = 0x200;
    constexpr u16 NoSound = 0xFFFF;
    constexpr u16 SoundIdMask = 0x7FFF;
    ObjectNode* node = NodeOf(runner);
    auto* skate = ControllerOf<SkateController>(node, NodeController::KindSkate);
    if (skate == nullptr)
    {
        return;
    }

    if ((counts & AddAgain) != 0 || (skate->counts & SkateController::IdCountMask) == 0)
    {
        const u16* given = reinterpret_cast<const u16*>(&ids1);
        for (u32 index = 0; index < (counts & 0xF); index++)
        {
            // Retail bug: the count is never reset, so IDs added again go past the eight into the sounds
            u32 at = skate->counts & SkateController::IdCountMask;
            skate->counts = (skate->counts & ~SkateController::IdCountMask) | ((at + 1) & SkateController::IdCountMask);
            reinterpret_cast<u16*>(&skate->ids)[at] = given[index];
        }

        for (u16 index = 0; index < SkateController::TrailCount; index++)
        {
            SetTrailSystem(&skate->trails[index], skate->ids[index]);
        }
    }

    if ((counts & AddAgain) == 0 && (skate->counts >> SkateController::SoundCountShift & SkateController::SoundCountMask) != 0)
    {
        return;
    }

    GameObject* object = node->OwnObject();
    if (object == nullptr)
    {
        return;
    }

    const u16* slots = reinterpret_cast<const u16*>(&sounds1);
    for (u32 index = 0; index < (counts >> 4 & 0x1F); index++)
    {
        if (slots[index] == NoSound)
        {
            continue;
        }

        u16 sound;
        GetObjectSoundId(&sound, object, slots[index]);
        if (sound == NoSound)
        {
            continue;
        }

        // Retail bug: past the thirteenth the sounds go into the padding before the trails
        u32 at = skate->counts >> SkateController::SoundCountShift & SkateController::SoundCountMask;
        skate->counts = (skate->counts & ~(SkateController::SoundCountMask << SkateController::SoundCountShift))
                        | ((at + 1) & SkateController::SoundCountMask) << SkateController::SoundCountShift;
        reinterpret_cast<u16*>(&skate->sounds)[at] = sound & SoundIdMask;
    }
}
