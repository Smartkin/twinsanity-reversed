#include "game/commands.h"

#include "game/animation.h"
#include "game/instances.h"
#include "game/math.h"
#include "game/memory.h"
#include "game/nodecontrollers.h"
#include "game/objectnode.h"
#include "game/objects.h"
#include "game/properties.h"
#include "game/sound.h"

#include <bit>
#include <cstddef>
#include <cstdint>

// The commands that make and set up the object nodes' controllers (game/nodecontrollers.h)

extern "C"
{
    // The items' builders' base, and the script conditions' builder's destructor (vtable D_002EE368)
    extern const GccVTableEntry g_ItemBuilderBaseVTable[] RETAIL(BuilderBaseFunctions);
    void DestroyConditionBuilder(void* builder, u32 destroyFlags) RETAIL(FUN_0011e388);
}

namespace
{
ObjectNode* NodeOf(BehaviourRunner* runner)
{
    return static_cast<ObjectNode*>(runner->agentNode);
}

// The node's controller when it's of a kind
template <typename Controller>
Controller* ControllerOf(ObjectNode* node, u8 kind)
{
    NodeController* controller = node->controller;
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

// The controller of the command's kind made and given to the agent's node (not when the node has one the command keeps)
void CreateNodeControllerCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    if (controller.keepsExisting && node->controller != nullptr)
    {
        return;
    }

    u32 kind = controller.kind;
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

// The final boss's weapons (the node's JointAimController) set up by the mode: their joints hooked on the node's model, the
// weapons picked turned back to their animation's pose or left resting, raised to aim (taking the pose they're at), the target (a
// designator's instance, else its position), the weapons' scale and turn rate
void FinalBossWeaponsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    // A retail bug: the controller's kind isn't checked, nor whether there's one (then it writes from address 0x10 on)
    auto* aimer = reinterpret_cast<JointAimer*>(reinterpret_cast<std::uintptr_t>(node->controller)
                                                + offsetof(JointAimController, aimer));
    switch (mode)
    {
    case ModeHookJoints:
    {
        for (u32 index = 0; index < WeaponCount; index++)
        {
            aimer->joints[index].id = joints[index];
            aimer->joints[index].node = node;
        }

        aimer->node = node;
        auto* model = static_cast<ModelNode*>(GetGameNode(&node->owner->nodes, NodeModel));
        aimer->AttachVirtual(model->animator);
        break;
    }
    case ModeReturn:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons.value & 1u << index) != 0)
            {
                aimer->joints[index].returning = aimer->joints[index].resting ^ 1;
            }
        }

        break;
    case ModeRaise:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons.value & 1u << index) != 0)
            {
                AimedJoint& aimed = aimer->joints[index];
                u8 resting = aimed.resting;
                aimed.resting = 0;
                aimed.retakesMatrix = resting;
                aimed.returning = 0;
            }
        }

        break;
    case ModeTarget:
    {
        auto* instance = CallVirtual<InstanceContext*>(node, node->vtable, ObjectNode::GetDesignatorSlot, target.designator);
        if (instance != nullptr)
        {
            aimer->targetInstance = instance;
            aimer->target = JointAimer::TargetInstance;
            break;
        }

        Vector4 position;
        if (CallVirtual<u32>(node, node->vtable, ObjectNode::GetDesignatorPositionSlot, target.designator, &position) != 0)
        {
            aimer->target = JointAimer::TargetPosition;
            aimer->targetPosition = position;
        }

        break;
    }
    case ModeScaleAndRate:
        for (u32 index = 0; index < WeaponCount; index++)
        {
            if ((weapons.value & 1u << index) != 0)
            {
                aimer->joints[index].scale = scale;
                aimer->joints[index].rate = turnRate;
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
    // The mask's IDs: the invincibility's first trail, the unused trail, the boost trail
    enum MaskId : u32
    {
        InvincibleId = 0,
        UnusedTrailId = 1,
        BoostId = 2,
    };

    constexpr u16 InvincibleTrails = 3;
    constexpr u8 InvincibleExitPoint = 5;
    auto* mask = ControllerOf<MaskController>(NodeOf(runner), NodeController::KindMask);
    if (mask == nullptr)
    {
        return;
    }

    const u16* given = ids;
    for (u32 index = 0; index < count.ids; index++)
    {
        // A retail bug: the count is never reset, so IDs given again go past the three (into the boost trail's arguments from
        // the ninth on)
        u32 at = mask->flags.idCount;
        mask->flags.idCount = at + 1;
        reinterpret_cast<u16*>(&mask->ids)[at] = given[index];
    }

    SetTrailSystem(&mask->boostTrail, mask->ids[BoostId]);
    SetTrailSystem(&mask->unusedTrail, mask->ids[UnusedTrailId]);
    u16 first = mask->ids[InvincibleId];
    for (u16 index = 0; index < InvincibleTrails; index++)
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
    f32 x = offsetX.FloatWith(properties);
    f32 y = offsetY.FloatWith(properties);
    f32 z = offsetZ.FloatWith(properties);
    f32 pullValue = pull.FloatWith(properties);
    f32 turnRateValue = turnRate.FloatWith(properties);
    f32 unused = unusedValue.FloatWith(properties);
    f32 dropValue = drop.FloatWith(properties);
    spline->offset = {x, y, z, 1.0f};
    spline->pull = pullValue;
    spline->turnRate = turnRateValue;
    spline->unused28 = unused;
    spline->drop = dropValue;
}

// The skate controller's trails' particle systems (added to its IDs when it has none or the command says so, every trail given
// its ID) and its sounds (the node's own object's sounds of the slots given, added when it has none or the command says so)
void SetSkateControllerIdsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    ObjectNode* node = NodeOf(runner);
    auto* skate = ControllerOf<SkateController>(node, NodeController::KindSkate);
    if (skate == nullptr)
    {
        return;
    }

    if (counts.addsAgain || skate->counts.idCount == 0)
    {
        const u16* given = ids;
        for (u32 index = 0; index < counts.ids; index++)
        {
            // Retail bug: the count is never reset, so IDs added again go past the eight into the sounds
            u32 at = skate->counts.idCount;
            skate->counts.idCount = at + 1;
            reinterpret_cast<u16*>(&skate->ids)[at] = given[index];
        }

        for (u16 index = 0; index < SkateController::TrailCount; index++)
        {
            SetTrailSystem(&skate->trails[index], skate->ids[index]);
        }
    }

    if (!counts.addsAgain && skate->counts.soundCount != 0)
    {
        return;
    }

    GameObject* object = node->OwnObject();
    if (object == nullptr)
    {
        return;
    }

    const u16* slots = soundSlots;
    for (u32 index = 0; index < counts.sounds; index++)
    {
        if (slots[index] == NoSoundSlot)
        {
            continue;
        }

        u16 sound;
        GetObjectSoundId(&sound, object, slots[index]);
        if (sound == NoSoundId)
        {
            continue;
        }

        // Retail bug: past the thirteenth the sounds go into the padding before the trails
        u32 at = skate->counts.soundCount;
        skate->counts.soundCount = at + 1;
        reinterpret_cast<u16*>(&skate->sounds)[at] = sound & ResourceIndexMask;
    }
}
