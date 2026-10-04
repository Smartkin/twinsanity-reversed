#pragma once

#include "common.h"
#include "game/agentlab.h"
#include "game/behaviours.h"

class GameNode;

// The script conditions the builder makes (made once by a script from the retail builder and TT Lab's AgentLabDefsPS2.json,
// whose names they have, and edited by hand since): the base's word (the ID in its low half, a parameter from bit 17) and floats,
// which the reader sets, and its vtable, then what a few keep of their own (set by the builder by the ID). A check scores the
// agent's node for the level at the clock's time (src/game/conditionchecks.cpp and the conditions' own files)

// 0
class NextCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond0_Next_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckConditionAlways);
};
CHECK_SIZE(NextCondition, 0x14);

// 1
class IsCollidableCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond1_IsCollidable_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckConditionInstanceContextBitfieldRelated);
};
CHECK_SIZE(IsCollidableCondition, 0x14);

// 2
class ElseCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond2_Else_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckConditionDefault);
};
CHECK_SIZE(ElseCondition, 0x14);

// 3
class RandomCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond3_Random_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckConditionGetRandFloat);
};
CHECK_SIZE(RandomCondition, 0x14);

// 4
class IsVisibleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond4_IsVisible_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckConditionInstanceContextBitfieldFlag11);
};
CHECK_SIZE(IsVisibleCondition, 0x14);

// 5
class TimeInUnitCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond5_TimeInUnit_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckAnimationProgressCondition_);
};
CHECK_SIZE(TimeInUnitCondition, 0x14);

// 6
class IsInExternalScriptCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond6_IsInExternalScript_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond6_IsInExternalScript_Check);
};
CHECK_SIZE(IsInExternalScriptCondition, 0x14);

// 7
class AnimationFinishedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond7_AnimationFinished_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckAnimationEndedCondition);
};
CHECK_SIZE(AnimationFinishedCondition, 0x14);

// 8
class IsPathCompleteCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond8_IsPathComplete_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond8_IsPathComplete_Check);
};
CHECK_SIZE(IsPathCompleteCondition, 0x14);

// 9
class MeToInitPosSqrDistCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond9_MeToInitPosSqrDist_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckDistanceFromSpawnCondition);
};
CHECK_SIZE(MeToInitPosSqrDistCondition, 0x14);

// 10
class MeToFocusSqrDistCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond10_MeToFocusSqrDist_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckDistanceFromPlayerCondition);
};
CHECK_SIZE(MeToFocusSqrDistCondition, 0x14);

// 11
class CurrentKeyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond11_CurrentKey_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond11_CurrentKey_Check);
};
CHECK_SIZE(CurrentKeyCondition, 0x14);

// 12
class IsLoadZoneStateSetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond12_IsLoadZoneStateSet_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckPersistentFlagCondition);
};
CHECK_SIZE(IsLoadZoneStateSetCondition, 0x14);

// 13
class GetRouteCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond13_GetRoute_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond13_GetRoute_Check);
};
CHECK_SIZE(GetRouteCondition, 0x14);

// 14
class GotKeysCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond14_GotKeys_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond14_GotKeys_Check);
};
CHECK_SIZE(GotKeysCondition, 0x14);

// 20
class InsideEdgeStartNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond20_InsideEdgeStartNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond20_InsideEdgeStartNode_Check);
};
CHECK_SIZE(InsideEdgeStartNodeCondition, 0x14);

// 21
class InsideEdgeEndNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond21_InsideEdgeEndNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond21_InsideEdgeEndNode_Check);
};
CHECK_SIZE(InsideEdgeEndNodeCondition, 0x14);

// 22
class MeFacingFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond22_MeFacingFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond22_MeFacingFocus_Check);
};
CHECK_SIZE(MeFacingFocusCondition, 0x14);

// 23
class FocusFacingMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond23_FocusFacingMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond23_FocusFacingMe_Check);
};
CHECK_SIZE(FocusFacingMeCondition, 0x14);

// 24
class ClearLineOfSightToFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond24_ClearLineOfSightToFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond24_ClearLineOfSightToFocus_Check);
};
CHECK_SIZE(ClearLineOfSightToFocusCondition, 0x14);

// 25
class FocusAgentCanSeeMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond25_FocusAgentCanSeeMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond25_FocusAgentCanSeeMe_Check);
};
CHECK_SIZE(FocusAgentCanSeeMeCondition, 0x14);

// 26
class CanSeeFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond26_CanSeeFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond26_CanSeeFocus_Check);
};
CHECK_SIZE(CanSeeFocusCondition, 0x14);

// 27
class MeFacingRouteNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond27_MeFacingRouteNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond27_MeFacingRouteNode_Check);
};
CHECK_SIZE(MeFacingRouteNodeCondition, 0x14);

// 28
class ClearLineOfSightToRouteNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond28_ClearLineOfSightToRouteNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond28_ClearLineOfSightToRouteNode_Check);
};
CHECK_SIZE(ClearLineOfSightToRouteNodeCondition, 0x14);

// 29
class HeightAboveFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond29_HeightAboveFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond29_HeightAboveFocus_Check);
};
CHECK_SIZE(HeightAboveFocusCondition, 0x14);

// 35
class MeFacingCameraCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond35_MeFacingCamera_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond35_MeFacingCamera_Check);
};
CHECK_SIZE(MeFacingCameraCondition, 0x14);

// 36
class CameraFacingMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond36_CameraFacingMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond36_CameraFacingMe_Check);
};
CHECK_SIZE(CameraFacingMeCondition, 0x14);

// 37
class InCameraFrustrumCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond37_InCameraFrustrum_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond37_InCameraFrustrum_Check);
};
CHECK_SIZE(InCameraFrustrumCondition, 0x14);

// 38
class ClearLineOfSightToCameraCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond38_ClearLineOfSightToCamera_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond38_ClearLineOfSightToCamera_Check);
};
CHECK_SIZE(ClearLineOfSightToCameraCondition, 0x14);

// 39
class CameraCanSeeMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond39_CameraCanSeeMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond39_CameraCanSeeMe_Check);
};
CHECK_SIZE(CameraCanSeeMeCondition, 0x14);

// 40
class HeadLookingAtFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond40_HeadLookingAtFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond40_HeadLookingAtFocus_Check);
};
CHECK_SIZE(HeadLookingAtFocusCondition, 0x14);

// 41
class HeadCanSeeFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond41_HeadCanSeeFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond41_HeadCanSeeFocus_Check);
};
CHECK_SIZE(HeadCanSeeFocusCondition, 0x14);

// 42
class HeadLookingAtRouteNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond42_HeadLookingAtRouteNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond42_HeadLookingAtRouteNode_Check);
};
CHECK_SIZE(HeadLookingAtRouteNodeCondition, 0x14);

// 43
class FocusHeadLookingAtMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond43_FocusHeadLookingAtMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond43_FocusHeadLookingAtMe_Check);
};
CHECK_SIZE(FocusHeadLookingAtMeCondition, 0x14);

// 44
class FocusHeadCanSeeMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond44_FocusHeadCanSeeMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond44_FocusHeadCanSeeMe_Check);
};
CHECK_SIZE(FocusHeadCanSeeMeCondition, 0x14);

// 45
class GotAnyFocusCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond45_GotAnyFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond45_GotAnyFocus_Check);
};
CHECK_SIZE(GotAnyFocusCondition, 0x14);

// 46
class FocusActorEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond46_FocusActorEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond46_FocusActorEquals_Check);
};
CHECK_SIZE(FocusActorEqualsCondition, 0x14);

// 47
class ActorSubtypeEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond47_ActorSubtypeEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckInstanceTypeCondition);
};
CHECK_SIZE(ActorSubtypeEqualsCondition, 0x14);

// 48
class AttachedToAnAgentCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond48_AttachedToAnAgent_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond48_AttachedToAnAgent_Check);
};
CHECK_SIZE(AttachedToAnAgentCondition, 0x14);

// 49
class GotAttachedObjectCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond49_GotAttachedObject_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond49_GotAttachedObject_Check);
};
CHECK_SIZE(GotAttachedObjectCondition, 0x14);

// 50
class GotAnyUserMessageCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond50_GotAnyUserMessage_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond50_GotAnyUserMessage_Check);
};
CHECK_SIZE(GotAnyUserMessageCondition, 0x14);

// 51
class GotUserMessageEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond51_GotUserMessageEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond51_GotUserMessageEquals_Check);
};
CHECK_SIZE(GotUserMessageEqualsCondition, 0x14);

// 52
class CurrentKeyEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond52_CurrentKeyEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond52_CurrentKeyEquals_Check);
};
CHECK_SIZE(CurrentKeyEqualsCondition, 0x14);

// 53
class TouchingTerrainCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond53_TouchingTerrain_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond53_TouchingTerrain_Check);
};
CHECK_SIZE(TouchingTerrainCondition, 0x14);

// 54
class TouchingAnyAgentCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond54_TouchingAnyAgent_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond54_TouchingAnyAgent_Check);
};
CHECK_SIZE(TouchingAnyAgentCondition, 0x14);

// 55
class GotAttachmentOnExitCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond55_GotAttachmentOnExit_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond55_GotAttachmentOnExit_Check);
};
CHECK_SIZE(GotAttachmentOnExitCondition, 0x14);

// 56
class GotFocusObjectCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond56_GotFocusObject_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond56_GotFocusObject_Check);
};
CHECK_SIZE(GotFocusObjectCondition, 0x14);

// 57
class GotFocusPositionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond57_GotFocusPosition_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond57_GotFocusPosition_Check);
};
CHECK_SIZE(GotFocusPositionCondition, 0x14);

// 58
class GotAnimationTimeRemainingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond58_GotAnimationTimeRemaining_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond58_GotAnimationTimeRemaining_Check);
};
CHECK_SIZE(GotAnimationTimeRemainingCondition, 0x14);

// 59
class CounterValueCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond59_CounterValue_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond59_CounterValue_Check);
};
CHECK_SIZE(CounterValueCondition, 0x14);

// 60
class CounterValueEqualsThresholdCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond60_CounterValueEqualsThreshold_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond60_CounterValueEqualsThreshold_Check);
};
CHECK_SIZE(CounterValueEqualsThresholdCondition, 0x14);

// 61
class IsRestingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond61_IsResting_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond61_IsResting_Check);
};
CHECK_SIZE(IsRestingCondition, 0x14);

// 62
class SqrMoveSpeedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond62_SqrMoveSpeed_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond62_SqrMoveSpeed_Check);
};
CHECK_SIZE(SqrMoveSpeedCondition, 0x14);

// 63
class FocusHasAttachmentCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond63_FocusHasAttachment_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond63_FocusHasAttachment_Check);
};
CHECK_SIZE(FocusHasAttachmentCondition, 0x14);

// 64
class LostAllAttachmentsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond64_LostAllAttachments_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond64_LostAllAttachments_Check);
};
CHECK_SIZE(LostAllAttachmentsCondition, 0x14);

// 65
class IsBusyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond65_IsBusy_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond65_IsBusy_Check);
};
CHECK_SIZE(IsBusyCondition, 0x14);

// 66
class FocusIsBusyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond66_FocusIsBusy_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond66_FocusIsBusy_Check);
};
CHECK_SIZE(FocusIsBusyCondition, 0x14);

// 67
class SoftFlagSetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond67_SoftFlagSet_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(CheckInstanceFlagSet);
};
CHECK_SIZE(SoftFlagSetCondition, 0x14);

// 68, 69
class MeToCurrentKeySqrDistCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond68_MeToCurrentKeySqrDist_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond68_MeToCurrentKeySqrDist_Check);
};
CHECK_SIZE(MeToCurrentKeySqrDistCondition, 0x14);

// 70, 71
class SpeedTowardsNextKeyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond70_SpeedTowardsNextKey_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond70_SpeedTowardsNextKey_Check);
};
CHECK_SIZE(SpeedTowardsNextKeyCondition, 0x14);

// 72
class BoxAboveIsOverlappedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond72_BoxAboveIsOverlapped_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond72_BoxAboveIsOverlapped_Check);
};
CHECK_SIZE(BoxAboveIsOverlappedCondition, 0x14);

// 73
class GotLinkedObjectCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond73_GotLinkedObject_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond73_GotLinkedObject_Check);
};
CHECK_SIZE(GotLinkedObjectCondition, 0x14);

// 74
class XCycleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond74_XCycle_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond74_XCycle_Check);
};
CHECK_SIZE(XCycleCondition, 0x14);

// 75
class YCycleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond75_YCycle_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond75_YCycle_Check);
};
CHECK_SIZE(YCycleCondition, 0x14);

// 76
class ZCycleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond76_ZCycle_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond76_ZCycle_Check);
};
CHECK_SIZE(ZCycleCondition, 0x14);

// 77
class GotAgentRef1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond77_GotAgentRef1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond77_GotAgentRef1_Check);
};
CHECK_SIZE(GotAgentRef1Condition, 0x14);

// 78
class GotAgentRef2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond78_GotAgentRef2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond78_GotAgentRef2_Check);
};
CHECK_SIZE(GotAgentRef2Condition, 0x14);

// 79
class AgentRef1ActorEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond79_AgentRef1ActorEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond79_AgentRef1ActorEquals_Check);
};
CHECK_SIZE(AgentRef1ActorEqualsCondition, 0x14);

// 80
class AgentRef2ActorEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond80_AgentRef2ActorEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond80_AgentRef2ActorEquals_Check);
};
CHECK_SIZE(AgentRef2ActorEqualsCondition, 0x14);

// 81
class HasInstancePositionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond81_HasInstancePosition_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond81_HasInstancePosition_Check);
};
CHECK_SIZE(HasInstancePositionCondition, 0x14);

// 82
class HasFocusPositionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond82_HasFocusPosition_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond82_HasFocusPosition_Check);
};
CHECK_SIZE(HasFocusPositionCondition, 0x14);

// 83
class NodeFlag16Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond83_NodeFlag16_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond83_NodeFlag16_Check);
};
CHECK_SIZE(NodeFlag16Condition, 0x14);

// 84
class NodeFlag17Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond84_NodeFlag17_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond84_NodeFlag17_Check);
};
CHECK_SIZE(NodeFlag17Condition, 0x14);

// 85
class NodeFlag15Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond85_NodeFlag15_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond85_NodeFlag15_Check);
};
CHECK_SIZE(NodeFlag15Condition, 0x14);

// 86
class NodeByte154FractionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond86_NodeByte154Fraction_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond86_NodeByte154Fraction_Check);
};
CHECK_SIZE(NodeByte154FractionCondition, 0x14);

// 87
class PhysicsBodyFlag1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond87_PhysicsBodyFlag1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond87_PhysicsBodyFlag1_Check);
};
CHECK_SIZE(PhysicsBodyFlag1Condition, 0x14);

// 88, 123
class DistanceToTargetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond88_DistanceToTarget_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond88_DistanceToTarget_Check);
};
CHECK_SIZE(DistanceToTargetCondition, 0x14);

// 89
class FocusPositionDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond89_FocusPositionDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond89_FocusPositionDistanceSquared_Check);
};
CHECK_SIZE(FocusPositionDistanceSquaredCondition, 0x14);

// 90
class GroundBelowFocusPositionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond90_GroundBelowFocusPosition_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond90_GroundBelowFocusPosition_Check);
};
CHECK_SIZE(GroundBelowFocusPositionCondition, 0x14);

// 91
class ObjectInstanceByteAtCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond91_ObjectInstanceByteAt_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond91_ObjectInstanceByteAt_Check);
};
CHECK_SIZE(ObjectInstanceByteAtCondition, 0x14);

// 92
class InstanceSubtypeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond92_InstanceSubtype_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond92_InstanceSubtype_Check);
};
CHECK_SIZE(InstanceSubtypeCondition, 0x14);

// 93
class HeadTrackingFlag24Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond93_HeadTrackingFlag24_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond93_HeadTrackingFlag24_Check);
};
CHECK_SIZE(HeadTrackingFlag24Condition, 0x14);

// 94
class HeadTrackingFlag25Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond94_HeadTrackingFlag25_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond94_HeadTrackingFlag25_Check);
};
CHECK_SIZE(HeadTrackingFlag25Condition, 0x14);

// 95
class HeadTrackingFlag26Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond95_HeadTrackingFlag26_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond95_HeadTrackingFlag26_Check);
};
CHECK_SIZE(HeadTrackingFlag26Condition, 0x14);

// 96
class HeadTrackingFlag27Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond96_HeadTrackingFlag27_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond96_HeadTrackingFlag27_Check);
};
CHECK_SIZE(HeadTrackingFlag27Condition, 0x14);

// 97
class HeadTrackingFlag28Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond97_HeadTrackingFlag28_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond97_HeadTrackingFlag28_Check);
};
CHECK_SIZE(HeadTrackingFlag28Condition, 0x14);

// 102
class SubPathKeyRawCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond102_SubPathKeyRaw_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond102_SubPathKeyRaw_Check);
};
CHECK_SIZE(SubPathKeyRawCondition, 0x14);

// 103
class SubPathKeyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond103_SubPathKey_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond103_SubPathKey_Check);
};
CHECK_SIZE(SubPathKeyCondition, 0x14);

// 104
class SubPathKeyDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond104_SubPathKeyDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond104_SubPathKeyDistanceSquared_Check);
};
CHECK_SIZE(SubPathKeyDistanceSquaredCondition, 0x14);

// 105
class PhysicsCount8cCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond105_PhysicsCount8c_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond105_PhysicsCount8c_Check);
};
CHECK_SIZE(PhysicsCount8cCondition, 0x14);

// 106
class PhysicsHasContactsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond106_PhysicsHasContacts_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond106_PhysicsHasContacts_Check);
};
CHECK_SIZE(PhysicsHasContactsCondition, 0x14);

// 107
class SubPathPreviousKeyDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond107_SubPathPreviousKeyDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond107_SubPathPreviousKeyDistanceSquared_Check);
};
CHECK_SIZE(SubPathPreviousKeyDistanceSquaredCondition, 0x14);

// 108
class PhysicsHasGroundCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond108_PhysicsHasGround_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond108_PhysicsHasGround_Check);
};
CHECK_SIZE(PhysicsHasGroundCondition, 0x14);

// 109
class CurrentLinkIndexCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond109_CurrentLinkIndex_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond109_CurrentLinkIndex_Check);
};
CHECK_SIZE(CurrentLinkIndexCondition, 0x14);

// 110
class HeightAboveStartCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond110_HeightAboveStart_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond110_HeightAboveStart_Check);
};
CHECK_SIZE(HeightAboveStartCondition, 0x14);

// 111
class HasPerception0Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond111_HasPerception0_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond111_HasPerception0_Check);
};
CHECK_SIZE(HasPerception0Condition, 0x14);

// 112
class HasPerception2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond112_HasPerception2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond112_HasPerception2_Check);
};
CHECK_SIZE(HasPerception2Condition, 0x14);

// 113
class HasPerception1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond113_HasPerception1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond113_HasPerception1_Check);
};
CHECK_SIZE(HasPerception1Condition, 0x14);

// 114
class CharacterAnalogCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond114_CharacterAnalog_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond114_CharacterAnalog_Check);
};
CHECK_SIZE(CharacterAnalogCondition, 0x14);

// 115
class AlwaysZeroCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond115_AlwaysZero_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond115_AlwaysZero_Check);
};
CHECK_SIZE(AlwaysZeroCondition, 0x14);

// 116
class PhysicsBodyFlag5Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond116_PhysicsBodyFlag5_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond116_PhysicsBodyFlag5_Check);
};
CHECK_SIZE(PhysicsBodyFlag5Condition, 0x14);

// 117
class GotUserMessageOnceEqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond117_GotUserMessageOnceEquals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond117_GotUserMessageOnceEquals_Check);
};
CHECK_SIZE(GotUserMessageOnceEqualsCondition, 0x14);

// 118
class HasXLinksCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond118_HasXLinks_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond118_HasXLinks_Check);
};
CHECK_SIZE(HasXLinksCondition, 0x14);

// 119
class PositionXCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond119_PositionX_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond119_PositionX_Check);
};
CHECK_SIZE(PositionXCondition, 0x14);

// 120
class PositionYCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond120_PositionY_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond120_PositionY_Check);
};
CHECK_SIZE(PositionYCondition, 0x14);

// 121
class PositionZCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond121_PositionZ_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond121_PositionZ_Check);
};
CHECK_SIZE(PositionZCondition, 0x14);

// 122
class AgentRef1SpawnFlagCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond122_AgentRef1SpawnFlag_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond122_AgentRef1SpawnFlag_Check);
};
CHECK_SIZE(AgentRef1SpawnFlagCondition, 0x14);

// 124
class IsAttachedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond124_IsAttached_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond124_IsAttached_Check);
};
CHECK_SIZE(IsAttachedCondition, 0x14);

// 125
class PhysicsImpactCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond125_PhysicsImpact_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond125_PhysicsImpact_Check);
};
CHECK_SIZE(PhysicsImpactCondition, 0x14);

// 126
class FocusForwardDotCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond126_FocusForwardDot_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond126_FocusForwardDot_Check);
};
CHECK_SIZE(FocusForwardDotCondition, 0x14);

// 127
class FocusObjectProp0EqualsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond127_FocusObjectProp0Equals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond127_FocusObjectProp0Equals_Check);
};
CHECK_SIZE(FocusObjectProp0EqualsCondition, 0x14);

// 128, 129, 130, 131
class AngleToFocusCondition : public ScriptCondition
{
public:
    u32 unknown14;
    u32 unknown18;

    void Destroy(u32 destroyFlags) RETAIL(Cond128_AngleToFocus_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond128_AngleToFocus_Check);
};
CHECK_SIZE(AngleToFocusCondition, 0x1C);

// 132
class KeyPathOnLastKeyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond132_KeyPathOnLastKey_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond132_KeyPathOnLastKey_Check);
};
CHECK_SIZE(KeyPathOnLastKeyCondition, 0x14);

// 133
class ContextValue154SetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond133_ContextValue154Set_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond133_ContextValue154Set_Check);
};
CHECK_SIZE(ContextValue154SetCondition, 0x14);

// 134
class KeyPathProgressCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond134_KeyPathProgress_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond134_KeyPathProgress_Check);
};
CHECK_SIZE(KeyPathProgressCondition, 0x14);

// 135
class KeyPathByte42Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond135_KeyPathByte42_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond135_KeyPathByte42_Check);
};
CHECK_SIZE(KeyPathByte42Condition, 0x14);

// 136
class FocusVisibleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond136_FocusVisible_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond136_FocusVisible_Check);
};
CHECK_SIZE(FocusVisibleCondition, 0x14);

// 137
class KeyPathNumKeysCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond137_KeyPathNumKeys_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond137_KeyPathNumKeys_Check);
};
CHECK_SIZE(KeyPathNumKeysCondition, 0x14);

// 138
class FocusToAgentRef1DistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond138_FocusToAgentRef1DistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond138_FocusToAgentRef1DistanceSquared_Check);
};
CHECK_SIZE(FocusToAgentRef1DistanceSquaredCondition, 0x14);

// 139
class FocusDistanceFromStartSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond139_FocusDistanceFromStartSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond139_FocusDistanceFromStartSquared_Check);
};
CHECK_SIZE(FocusDistanceFromStartSquaredCondition, 0x14);

// 140
class AgentRef1HeightDifferenceCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond140_AgentRef1HeightDifference_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond140_AgentRef1HeightDifference_Check);
};
CHECK_SIZE(AgentRef1HeightDifferenceCondition, 0x14);

// 141
class FocusOffXAxisDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond141_FocusOffXAxisDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond141_FocusOffXAxisDistanceSquared_Check);
};
CHECK_SIZE(FocusOffXAxisDistanceSquaredCondition, 0x14);

// 142
class FocusHorizontalDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond142_FocusHorizontalDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond142_FocusHorizontalDistanceSquared_Check);
};
CHECK_SIZE(FocusHorizontalDistanceSquaredCondition, 0x14);

// 143
class FocusOffForwardAxisDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond143_FocusOffForwardAxisDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond143_FocusOffForwardAxisDistanceSquared_Check);
};
CHECK_SIZE(FocusOffForwardAxisDistanceSquaredCondition, 0x14);

// 144
class AgentRef1OffXAxisDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond144_AgentRef1OffXAxisDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond144_AgentRef1OffXAxisDistanceSquared_Check);
};
CHECK_SIZE(AgentRef1OffXAxisDistanceSquaredCondition, 0x14);

// 145
class AgentRef1HorizontalDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond145_AgentRef1HorizontalDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond145_AgentRef1HorizontalDistanceSquared_Check);
};
CHECK_SIZE(AgentRef1HorizontalDistanceSquaredCondition, 0x14);

// 146
class AgentRef1OffAxisDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond146_AgentRef1OffAxisDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond146_AgentRef1OffAxisDistanceSquared_Check);
};
CHECK_SIZE(AgentRef1OffAxisDistanceSquaredCondition, 0x14);

// 147
class PhysicsTouchingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond147_PhysicsTouching_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond147_PhysicsTouching_Check);
};
CHECK_SIZE(PhysicsTouchingCondition, 0x14);

// 148
class AgentRef1SideOffsetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond148_AgentRef1SideOffset_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond148_AgentRef1SideOffset_Check);
};
CHECK_SIZE(AgentRef1SideOffsetCondition, 0x14);

// 149
class AgentRef1VisibleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond149_AgentRef1Visible_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond149_AgentRef1Visible_Check);
};
CHECK_SIZE(AgentRef1VisibleCondition, 0x14);

// 150
class AgentRef1InViewConeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond150_AgentRef1InViewCone_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond150_AgentRef1InViewCone_Check);
};
CHECK_SIZE(AgentRef1InViewConeCondition, 0x14);

// 151
class FocusFlag10Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond151_FocusFlag10_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond151_FocusFlag10_Check);
};
CHECK_SIZE(FocusFlag10Condition, 0x14);

// 152
class IsInPlayerChunkCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond152_IsInPlayerChunk_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond152_IsInPlayerChunk_Check);
};
CHECK_SIZE(IsInPlayerChunkCondition, 0x14);

// 153
class HasScriptInSlotCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond153_HasScriptInSlot_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond153_HasScriptInSlot_Check);
};
CHECK_SIZE(HasScriptInSlotCondition, 0x14);

// 154
class CurrentKeyIsEvenCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond154_CurrentKeyIsEven_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond154_CurrentKeyIsEven_Check);
};
CHECK_SIZE(CurrentKeyIsEvenCondition, 0x14);

// 155
class FocusFromExitPointCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond155_FocusFromExitPoint_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond155_FocusFromExitPoint_Check);
};
CHECK_SIZE(FocusFromExitPointCondition, 0x14);

// 156
class VideoStateIs5Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond156_VideoStateIs5_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond156_VideoStateIs5_Check);
};
CHECK_SIZE(VideoStateIs5Condition, 0x14);

// 157
class UpVectorXCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond157_UpVectorX_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond157_UpVectorX_Check);
};
CHECK_SIZE(UpVectorXCondition, 0x14);

// 158
class IntProp0Bit0Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond158_IntProp0Bit0_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond158_IntProp0Bit0_Check);
};
CHECK_SIZE(IntProp0Bit0Condition, 0x14);

// 159
class VideoReadyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond159_VideoReady_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond159_VideoReady_Check);
};
CHECK_SIZE(VideoReadyCondition, 0x14);

// 160
class NearestPointEdgeDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond160_NearestPointEdgeDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond160_NearestPointEdgeDistanceSquared_Check);
};
CHECK_SIZE(NearestPointEdgeDistanceSquaredCondition, 0x14);

// 161
class FocusIsAgentRef1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond161_FocusIsAgentRef1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond161_FocusIsAgentRef1_Check);
};
CHECK_SIZE(FocusIsAgentRef1Condition, 0x14);

// 162
class PhysicsHasCollisionNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond162_PhysicsHasCollisionNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond162_PhysicsHasCollisionNode_Check);
};
CHECK_SIZE(PhysicsHasCollisionNodeCondition, 0x14);

// 163, 164, 165, 166
class FocusObjectByte0EqualsCondition : public ScriptCondition
{
public:
    u32 unknown14;

    void Destroy(u32 destroyFlags) RETAIL(Cond163_FocusObjectByte0Equals_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond163_FocusObjectByte0Equals_Check);
};
CHECK_SIZE(FocusObjectByte0EqualsCondition, 0x18);

// 167
class SplineDistanceToAgentRef1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond167_SplineDistanceToAgentRef1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond167_SplineDistanceToAgentRef1_Check);
};
CHECK_SIZE(SplineDistanceToAgentRef1Condition, 0x14);

// 168
class ObstacleAheadCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond168_ObstacleAhead_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond168_ObstacleAhead_Check);
};
CHECK_SIZE(ObstacleAheadCondition, 0x14);

// 169
class NodeValue174CountCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond169_NodeValue174Count_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond169_NodeValue174Count_Check);
};
CHECK_SIZE(NodeValue174CountCondition, 0x14);

// 170
class IsFullInstanceNodeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond170_IsFullInstanceNode_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond170_IsFullInstanceNode_Check);
};
CHECK_SIZE(IsFullInstanceNodeCondition, 0x14);

// 171
class NodeByte8cMinusGlobalCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond171_NodeByte8cMinusGlobal_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond171_NodeByte8cMinusGlobal_Check);
};
CHECK_SIZE(NodeByte8cMinusGlobalCondition, 0x14);

// 172
class TimeSinceMarkCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond172_TimeSinceMark_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond172_TimeSinceMark_Check);
};
CHECK_SIZE(TimeSinceMarkCondition, 0x14);

// 173
class AlwaysZero173Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond173_AlwaysZero173_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond173_AlwaysZero173_Check);
};
CHECK_SIZE(AlwaysZero173Condition, 0x14);

// 174
class AlwaysCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond174_Always_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond174_Always_Check);
};
CHECK_SIZE(AlwaysCondition, 0x14);

// 175
class NeverCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond175_Never_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond175_Never_Check);
};
CHECK_SIZE(NeverCondition, 0x14);

// 176
class ChunksLoadedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond176_ChunksLoaded_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond176_ChunksLoaded_Check);
};
CHECK_SIZE(ChunksLoadedCondition, 0x14);

// 177
class FocusInSameChunkCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond177_FocusInSameChunk_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond177_FocusInSameChunk_Check);
};
CHECK_SIZE(FocusInSameChunkCondition, 0x14);

// 512
class PlayerHitPointsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond512_PlayerHitPoints_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond512_PlayerHitPoints_Check);
};
CHECK_SIZE(PlayerHitPointsCondition, 0x14);

// 514
class PlayerIsCrouchingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond514_PlayerIsCrouching_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond514_PlayerIsCrouching_Check);
};
CHECK_SIZE(PlayerIsCrouchingCondition, 0x14);

// 515
class PlayerIsGroundedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond515_PlayerIsGrounded_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond515_PlayerIsGrounded_Check);
};
CHECK_SIZE(PlayerIsGroundedCondition, 0x14);

// 517
class MeToPlayerSqrDistCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond517_MeToPlayerSqrDist_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond517_MeToPlayerSqrDist_Check);
};
CHECK_SIZE(MeToPlayerSqrDistCondition, 0x14);

// 518
class AgentIsOnGroundCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond518_AgentIsOnGround_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond518_AgentIsOnGround_Check);
};
CHECK_SIZE(AgentIsOnGroundCondition, 0x14);

// 519
class CrateHasRedWumpaCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond519_CrateHasRedWumpa_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond519_CrateHasRedWumpa_Check);
};
CHECK_SIZE(CrateHasRedWumpaCondition, 0x14);

// 520
class AgentWasTouchedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond520_AgentWasTouched_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond520_AgentWasTouched_Check);
};
CHECK_SIZE(AgentWasTouchedCondition, 0x14);

// 521
class AgentWasSpunCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond521_AgentWasSpun_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond521_AgentWasSpun_Check);
};
CHECK_SIZE(AgentWasSpunCondition, 0x14);

// 522
class AgentWasKneeDroppedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond522_AgentWasKneeDropped_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond522_AgentWasKneeDropped_Check);
};
CHECK_SIZE(AgentWasKneeDroppedCondition, 0x14);

// 523
class AgentWasSlidCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond523_AgentWasSlid_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond523_AgentWasSlid_Check);
};
CHECK_SIZE(AgentWasSlidCondition, 0x14);

// 524
class AgentHitPointsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond524_AgentHitPoints_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond524_AgentHitPoints_Check);
};
CHECK_SIZE(AgentHitPointsCondition, 0x14);

// 525
class CanMoveForwardsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond525_CanMoveForwards_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond525_CanMoveForwards_Check);
};
CHECK_SIZE(CanMoveForwardsCondition, 0x14);

// 526
class CanMoveBackwardsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond526_CanMoveBackwards_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond526_CanMoveBackwards_Check);
};
CHECK_SIZE(CanMoveBackwardsCondition, 0x14);

// 527
class CanStrafeLeftCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond527_CanStrafeLeft_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond527_CanStrafeLeft_Check);
};
CHECK_SIZE(CanStrafeLeftCondition, 0x14);

// 528
class CanStrafeRightCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond528_CanStrafeRight_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond528_CanStrafeRight_Check);
};
CHECK_SIZE(CanStrafeRightCondition, 0x14);

// 529
class CanJumpForwardsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond529_CanJumpForwards_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond529_CanJumpForwards_Check);
};
CHECK_SIZE(CanJumpForwardsCondition, 0x14);

// 530
class CanFallCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond530_CanFall_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond530_CanFall_Check);
};
CHECK_SIZE(CanFallCondition, 0x14);

// 531
class WillHitLowWallCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond531_WillHitLowWall_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond531_WillHitLowWall_Check);
};
CHECK_SIZE(WillHitLowWallCondition, 0x14);

// 532
class WillHitWallCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond532_WillHitWall_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond532_WillHitWall_Check);
};
CHECK_SIZE(WillHitWallCondition, 0x14);

// 533
class WillRunOffCliffCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond533_WillRunOffCliff_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond533_WillRunOffCliff_Check);
};
CHECK_SIZE(WillRunOffCliffCondition, 0x14);

// 534
class AgentWasAttackedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond534_AgentWasAttacked_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond534_AgentWasAttacked_Check);
};
CHECK_SIZE(AgentWasAttackedCondition, 0x14);

// 535
class AgentWasJumpedOnCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond535_AgentWasJumpedOn_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond535_AgentWasJumpedOn_Check);
};
CHECK_SIZE(AgentWasJumpedOnCondition, 0x14);

// 536
class AgentWasWalkedIntoCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond536_AgentWasWalkedInto_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond536_AgentWasWalkedInto_Check);
};
CHECK_SIZE(AgentWasWalkedIntoCondition, 0x14);

// 537
class AgentWasHeadbuttedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond537_AgentWasHeadbutted_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond537_AgentWasHeadbutted_Check);
};
CHECK_SIZE(AgentWasHeadbuttedCondition, 0x14);

// 538
class PlayerToMyFocusSqrDistCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond538_PlayerToMyFocusSqrDist_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond538_PlayerToMyFocusSqrDist_Check);
};
CHECK_SIZE(PlayerToMyFocusSqrDistCondition, 0x14);

// 539
class WumpaNeededForPayGateCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond539_WumpaNeededForPayGate_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond539_WumpaNeededForPayGate_Check);
};
CHECK_SIZE(WumpaNeededForPayGateCondition, 0x14);

// 540
class MeFacingPlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond540_MeFacingPlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond540_MeFacingPlayer_Check);
};
CHECK_SIZE(MeFacingPlayerCondition, 0x14);

// 541
class PlayerFacingMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond541_PlayerFacingMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond541_PlayerFacingMe_Check);
};
CHECK_SIZE(PlayerFacingMeCondition, 0x14);

// 542
class ClearLineOfSightToPlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond542_ClearLineOfSightToPlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond542_ClearLineOfSightToPlayer_Check);
};
CHECK_SIZE(ClearLineOfSightToPlayerCondition, 0x14);

// 543
class CanSeePlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond543_CanSeePlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond543_CanSeePlayer_Check);
};
CHECK_SIZE(CanSeePlayerCondition, 0x14);

// 544
class PlayerCanSeeMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond544_PlayerCanSeeMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond544_PlayerCanSeeMe_Check);
};
CHECK_SIZE(PlayerCanSeeMeCondition, 0x14);

// 545
class NodeTrafficCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond545_NodeTraffic_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond545_NodeTraffic_Check);
};
CHECK_SIZE(NodeTrafficCondition, 0x14);

// 546
class NodeIsAirborneCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond546_NodeIsAirborne_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond546_NodeIsAirborne_Check);
};
CHECK_SIZE(NodeIsAirborneCondition, 0x14);

// 547
class EdgeNeedsJumpCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond547_EdgeNeedsJump_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond547_EdgeNeedsJump_Check);
};
CHECK_SIZE(EdgeNeedsJumpCondition, 0x14);

// 548
class EdgeNeedsFlyingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond548_EdgeNeedsFlying_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond548_EdgeNeedsFlying_Check);
};
CHECK_SIZE(EdgeNeedsFlyingCondition, 0x14);

// 550
class EdgeNeedsLongJumpCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond550_EdgeNeedsLongJump_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond550_EdgeNeedsLongJump_Check);
};
CHECK_SIZE(EdgeNeedsLongJumpCondition, 0x14);

// 551
class EdgeNeedsHighJumpCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond551_EdgeNeedsHighJump_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond551_EdgeNeedsHighJump_Check);
};
CHECK_SIZE(EdgeNeedsHighJumpCondition, 0x14);

// 552
class HeightAbovePlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond552_HeightAbovePlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond552_HeightAbovePlayer_Check);
};
CHECK_SIZE(HeightAbovePlayerCondition, 0x14);

// 553
class HeadLookingAtPlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond553_HeadLookingAtPlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond553_HeadLookingAtPlayer_Check);
};
CHECK_SIZE(HeadLookingAtPlayerCondition, 0x14);

// 554
class HeadCanSeePlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond554_HeadCanSeePlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond554_HeadCanSeePlayer_Check);
};
CHECK_SIZE(HeadCanSeePlayerCondition, 0x14);

// 555
class PlayerHeadLookingAtMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond555_PlayerHeadLookingAtMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond555_PlayerHeadLookingAtMe_Check);
};
CHECK_SIZE(PlayerHeadLookingAtMeCondition, 0x14);

// 556
class PlayerHeadCanSeeMeCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond556_PlayerHeadCanSeeMe_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond556_PlayerHeadCanSeeMe_Check);
};
CHECK_SIZE(PlayerHeadCanSeeMeCondition, 0x14);

// 557
class PlayerIsMovingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond557_PlayerIsMoving_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond557_PlayerIsMoving_Check);
};
CHECK_SIZE(PlayerIsMovingCondition, 0x14);

// 558
class PlayerIsWalkingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond558_PlayerIsWalking_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond558_PlayerIsWalking_Check);
};
CHECK_SIZE(PlayerIsWalkingCondition, 0x14);

// 559
class PlayerIsRunningCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond559_PlayerIsRunning_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond559_PlayerIsRunning_Check);
};
CHECK_SIZE(PlayerIsRunningCondition, 0x14);

// 560
class PlayerIsCrawlingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond560_PlayerIsCrawling_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond560_PlayerIsCrawling_Check);
};
CHECK_SIZE(PlayerIsCrawlingCondition, 0x14);

// 561
class PlayerIsFallingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond561_PlayerIsFalling_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond561_PlayerIsFalling_Check);
};
CHECK_SIZE(PlayerIsFallingCondition, 0x14);

// 562
class PlayerIsCoOpLinkedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond562_PlayerIsCoOpLinked_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond562_PlayerIsCoOpLinked_Check);
};
CHECK_SIZE(PlayerIsCoOpLinkedCondition, 0x14);

// 563
class PlayerHoldingMultiToolCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond563_PlayerHoldingMultiTool_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond563_PlayerHoldingMultiTool_Check);
};
CHECK_SIZE(PlayerHoldingMultiToolCondition, 0x14);

// 564
class PlayerIsSlammingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond564_PlayerIsSlamming_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond564_PlayerIsSlamming_Check);
};
CHECK_SIZE(PlayerIsSlammingCondition, 0x14);

// 565
class PlayerIsSpinningCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond565_PlayerIsSpinning_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond565_PlayerIsSpinning_Check);
};
CHECK_SIZE(PlayerIsSpinningCondition, 0x14);

// 566
class PlayerIsJumpingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond566_PlayerIsJumping_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond566_PlayerIsJumping_Check);
};
CHECK_SIZE(PlayerIsJumpingCondition, 0x14);

// 567
class HeadCanSeePlayerUnblockedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond567_HeadCanSeePlayerUnblocked_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond567_HeadCanSeePlayerUnblocked_Check);
};
CHECK_SIZE(HeadCanSeePlayerUnblockedCondition, 0x14);

// 568
class AmIHarmfulCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond568_AmIHarmful_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond568_AmIHarmful_Check);
};
CHECK_SIZE(AmIHarmfulCondition, 0x14);

// 569
class AttachedContextFlag8Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond569_AttachedContextFlag8_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond569_AttachedContextFlag8_Check);
};
CHECK_SIZE(AttachedContextFlag8Condition, 0x14);

// 570
class DUMMY_570Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond570_DUMMY_570_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond570_DUMMY_570_Check);
};
CHECK_SIZE(DUMMY_570Condition, 0x14);

// 571
class DUMMY_571Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond571_DUMMY_571_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond571_DUMMY_571_Check);
};
CHECK_SIZE(DUMMY_571Condition, 0x14);

// 572
class CutsceneSkippedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond572_CutsceneSkipped_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond572_CutsceneSkipped_Check);
};
CHECK_SIZE(CutsceneSkippedCondition, 0x14);

// 573
class IsCirclePressedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond573_ButtonPressure6_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond573_ButtonPressure6_Check);
};
CHECK_SIZE(IsCirclePressedCondition, 0x14);

// 574
class IsSquarePressedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond574_ButtonPressure7_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond574_ButtonPressure7_Check);
};
CHECK_SIZE(IsSquarePressedCondition, 0x14);

// 575
class IsTrianglePressedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond575_ButtonPressure_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond575_ButtonPressure_Check);
};
CHECK_SIZE(IsTrianglePressedCondition, 0x14);

// 576
class IsR1PressedCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond576_ButtonPressure19_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond576_ButtonPressure19_Check);
};
CHECK_SIZE(IsR1PressedCondition, 0x14);

// 577
class CharacterVehiclePointerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond577_CharacterVehiclePointer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond577_CharacterVehiclePointer_Check);
};
CHECK_SIZE(CharacterVehiclePointerCondition, 0x14);

// 578
class CharacterFlag23Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond578_CharacterFlag23_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond578_CharacterFlag23_Check);
};
CHECK_SIZE(CharacterFlag23Condition, 0x14);

// 579
class IsChargedShotCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond579_IsChargedShot_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond579_IsChargedShot_Check);
};
CHECK_SIZE(IsChargedShotCondition, 0x14);

// 580
class IsDownBlastCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond580_IsDownBlast_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond580_IsDownBlast_Check);
};
CHECK_SIZE(IsDownBlastCondition, 0x14);

// 581
class GlobalInstanceOp581Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond581_GlobalInstanceOp581_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond581_GlobalInstanceOp581_Check);
};
CHECK_SIZE(GlobalInstanceOp581Condition, 0x14);

// 582
class PlayerVisibleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond582_PlayerVisible_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond582_PlayerVisible_Check);
};
CHECK_SIZE(PlayerVisibleCondition, 0x14);

// 583
class SubPathPointFlag0Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond583_SubPathPointFlag0_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond583_SubPathPointFlag0_Check);
};
CHECK_SIZE(SubPathPointFlag0Condition, 0x14);

// 584
class PlayerVisible2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond584_PlayerVisible2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond584_PlayerVisible2_Check);
};
CHECK_SIZE(PlayerVisible2Condition, 0x14);

// 585
class PlayerVisible3Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond585_PlayerVisible3_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond585_PlayerVisible3_Check);
};
CHECK_SIZE(PlayerVisible3Condition, 0x14);

// 586
class CharacterFlag22Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond586_CharacterFlag22_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond586_CharacterFlag22_Check);
};
CHECK_SIZE(CharacterFlag22Condition, 0x14);

// 587
class IsPlayerCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond587_IsPlayer_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond587_IsPlayer_Check);
};
CHECK_SIZE(IsPlayerCondition, 0x14);

// 588
class ObjectContextFlag17Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond588_ObjectContextFlag17_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond588_ObjectContextFlag17_Check);
};
CHECK_SIZE(ObjectContextFlag17Condition, 0x14);

// 589
class HitByPunchCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond589_HitByPunch_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond589_HitByPunch_Check);
};
CHECK_SIZE(HitByPunchCondition, 0x14);

// 590
class HitByBodySlam2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond590_HitByBodySlam2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond590_HitByBodySlam2_Check);
};
CHECK_SIZE(HitByBodySlam2Condition, 0x14);

// 591
class HitBySpinHitboxCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond591_HitBySpinHitbox_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond591_HitBySpinHitbox_Check);
};
CHECK_SIZE(HitBySpinHitboxCondition, 0x14);

// 592
class HitByBodySlamHitboxCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond592_HitByBodySlamHitbox_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond592_HitByBodySlamHitbox_Check);
};
CHECK_SIZE(HitByBodySlamHitboxCondition, 0x14);

// 593
class CharacterHasVehicleCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond593_CharacterHasVehicle_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond593_CharacterHasVehicle_Check);
};
CHECK_SIZE(CharacterHasVehicleCondition, 0x14);

// 594
class IsVehicleRollerbrawlCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond594_IsVehicleRollerbrawl_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond594_IsVehicleRollerbrawl_Check);
};
CHECK_SIZE(IsVehicleRollerbrawlCondition, 0x14);

// 595
class VehicleTypeNot2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond595_VehicleTypeNot2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond595_VehicleTypeNot2_Check);
};
CHECK_SIZE(VehicleTypeNot2Condition, 0x14);

// 596
class IsVehicleHumiliskateCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond596_IsVehicleHumiliskate_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond596_IsVehicleHumiliskate_Check);
};
CHECK_SIZE(IsVehicleHumiliskateCondition, 0x14);

// 597
class VehicleTypeNot4Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond597_VehicleTypeNot4_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond597_VehicleTypeNot4_Check);
};
CHECK_SIZE(VehicleTypeNot4Condition, 0x14);

// 598
class IsVehicle3Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond598_IsVehicle3_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond598_IsVehicle3_Check);
};
CHECK_SIZE(IsVehicle3Condition, 0x14);

// 599
class SubPathPointFlag5Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond599_SubPathPointFlag5_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond599_SubPathPointFlag5_Check);
};
CHECK_SIZE(SubPathPointFlag5Condition, 0x14);

// 600
class SubPathPointFlag4Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond600_SubPathPointFlag4_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond600_SubPathPointFlag4_Check);
};
CHECK_SIZE(SubPathPointFlag4Condition, 0x14);

// 601
class SubPathPointFlag6Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond601_SubPathPointFlag6_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond601_SubPathPointFlag6_Check);
};
CHECK_SIZE(SubPathPointFlag6Condition, 0x14);

// 602
class PathSegmentFlag0Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond602_PathSegmentFlag0_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond602_PathSegmentFlag0_Check);
};
CHECK_SIZE(PathSegmentFlag0Condition, 0x14);

// 603
class SubPathPreviousPointFlag5Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond603_SubPathPreviousPointFlag5_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond603_SubPathPreviousPointFlag5_Check);
};
CHECK_SIZE(SubPathPreviousPointFlag5Condition, 0x14);

// 604
class SubPathPreviousPointFlag4Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond604_SubPathPreviousPointFlag4_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond604_SubPathPreviousPointFlag4_Check);
};
CHECK_SIZE(SubPathPreviousPointFlag4Condition, 0x14);

// 605
class SubPathPreviousPointFlag6Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond605_SubPathPreviousPointFlag6_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond605_SubPathPreviousPointFlag6_Check);
};
CHECK_SIZE(SubPathPreviousPointFlag6Condition, 0x14);

// 606
class SubPathPointFlag5bCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond606_SubPathPointFlag5b_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond606_SubPathPointFlag5b_Check);
};
CHECK_SIZE(SubPathPointFlag5bCondition, 0x14);

// 607
class SubPathPointFlag4bCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond607_SubPathPointFlag4b_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond607_SubPathPointFlag4b_Check);
};
CHECK_SIZE(SubPathPointFlag4bCondition, 0x14);

// 608
class SubPathPointFlag6bCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond608_SubPathPointFlag6b_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond608_SubPathPointFlag6b_Check);
};
CHECK_SIZE(SubPathPointFlag6bCondition, 0x14);

// 609
class PlayerVectorLengthDifferenceCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond609_PlayerVectorLengthDifference_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond609_PlayerVectorLengthDifference_Check);
};
CHECK_SIZE(PlayerVectorLengthDifferenceCondition, 0x14);

// 610
class HitByCortexBoltCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond610_HitByCortexBolt_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond610_HitByCortexBolt_Check);
};
CHECK_SIZE(HitByCortexBoltCondition, 0x14);

// 611
class ObjectContextFlag1Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond611_ObjectContextFlag1_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond611_ObjectContextFlag1_Check);
};
CHECK_SIZE(ObjectContextFlag1Condition, 0x14);

// 612
class SubPathPointFlag2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond612_SubPathPointFlag2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond612_SubPathPointFlag2_Check);
};
CHECK_SIZE(SubPathPointFlag2Condition, 0x14);

// 613
class SubPathPreviousPointFlag2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond613_SubPathPreviousPointFlag2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond613_SubPathPreviousPointFlag2_Check);
};
CHECK_SIZE(SubPathPreviousPointFlag2Condition, 0x14);

// 614
class SubPathPointFlag2bCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond614_SubPathPointFlag2b_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond614_SubPathPointFlag2b_Check);
};
CHECK_SIZE(SubPathPointFlag2bCondition, 0x14);

// 615
class SubPathPointFlags56Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond615_SubPathPointFlags56_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond615_SubPathPointFlags56_Check);
};
CHECK_SIZE(SubPathPointFlags56Condition, 0x14);

// 616
class FocusPositionToPlayerDistanceSquaredCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond616_FocusPositionToPlayerDistanceSquared_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond616_FocusPositionToPlayerDistanceSquared_Check);
};
CHECK_SIZE(FocusPositionToPlayerDistanceSquaredCondition, 0x14);

// 617
class IsPushingObjectCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond617_IsPushingObject_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond617_IsPushingObject_Check);
};
CHECK_SIZE(IsPushingObjectCondition, 0x14);

// 618
class PlayerSideOffsetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond618_PlayerSideOffset_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond618_PlayerSideOffset_Check);
};
CHECK_SIZE(PlayerSideOffsetCondition, 0x14);

// 619
class PlayerNearCurrentKeyCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond619_PlayerNearCurrentKey_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond619_PlayerNearCurrentKey_Check);
};
CHECK_SIZE(PlayerNearCurrentKeyCondition, 0x14);

// 620
class PlayerSplineVehicleValueCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond620_PlayerSplineVehicleValue_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond620_PlayerSplineVehicleValue_Check);
};
CHECK_SIZE(PlayerSplineVehicleValueCondition, 0x14);

// 621
class CharacterHasHomeChunkCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond621_CharacterHasHomeChunk_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond621_CharacterHasHomeChunk_Check);
};
CHECK_SIZE(CharacterHasHomeChunkCondition, 0x14);

// 622
class PlayerFlag57ClearCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond622_PlayerFlag57Clear_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond622_PlayerFlag57Clear_Check);
};
CHECK_SIZE(PlayerFlag57ClearCondition, 0x14);

// 623
class HasActorWeightCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond623_HasActorWeight_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond623_HasActorWeight_Check);
};
CHECK_SIZE(HasActorWeightCondition, 0x14);

// 624
class GameFlags44Is12Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond624_GameFlags44Is12_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond624_GameFlags44Is12_Check);
};
CHECK_SIZE(GameFlags44Is12Condition, 0x14);

// 625
class ObjectContextFlag25Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond625_ObjectContextFlag25_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond625_ObjectContextFlag25_Check);
};
CHECK_SIZE(ObjectContextFlag25Condition, 0x14);

// 626
class ObjectContextFlag2Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond626_ObjectContextFlag2_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond626_ObjectContextFlag2_Check);
};
CHECK_SIZE(ObjectContextFlag2Condition, 0x14);

// 627
class PlayerVehicle1ValueCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond627_PlayerVehicle1Value_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond627_PlayerVehicle1Value_Check);
};
CHECK_SIZE(PlayerVehicle1ValueCondition, 0x14);

// 628
class BothCharactersFlag14Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond628_BothCharactersFlag14_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond628_BothCharactersFlag14_Check);
};
CHECK_SIZE(BothCharactersFlag14Condition, 0x14);

// 629
class GlobalInt3098e8Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond629_GlobalInt3098e8_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond629_GlobalInt3098e8_Check);
};
CHECK_SIZE(GlobalInt3098e8Condition, 0x14);

// 630
class NodeValue134SetCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond630_NodeValue134Set_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond630_NodeValue134Set_Check);
};
CHECK_SIZE(NodeValue134SetCondition, 0x14);

// 631
class GameControllerField500HighCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond631_GameControllerField500High_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond631_GameControllerField500High_Check);
};
CHECK_SIZE(GameControllerField500HighCondition, 0x14);

// 632
class GameTimer57cCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond632_GameTimer57c_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond632_GameTimer57c_Check);
};
CHECK_SIZE(GameTimer57cCondition, 0x14);

// 633
class SecondCharacterGunStateCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond633_SecondCharacterGunState_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond633_SecondCharacterGunState_Check);
};
CHECK_SIZE(SecondCharacterGunStateCondition, 0x14);

// 634
class HasAmmoCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond634_HasAmmo_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond634_HasAmmo_Check);
};
CHECK_SIZE(HasAmmoCondition, 0x14);

// 635
class CameraForwardDistanceCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond635_CameraForwardDistance_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond635_CameraForwardDistance_Check);
};
CHECK_SIZE(CameraForwardDistanceCondition, 0x14);

// 636
class ObjectContextFlag19Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond636_ObjectContextFlag19_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond636_ObjectContextFlag19_Check);
};
CHECK_SIZE(ObjectContextFlag19Condition, 0x14);

// 637
class GameModeIs5Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond637_GameModeIs5_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond637_GameModeIs5_Check);
};
CHECK_SIZE(GameModeIs5Condition, 0x14);

// 638
class ObjectContextFlags3or22Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond638_ObjectContextFlags3or22_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond638_ObjectContextFlags3or22_Check);
};
CHECK_SIZE(ObjectContextFlags3or22Condition, 0x14);

// 639
class GlobalProgressionCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond639_GlobalProgression_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond639_GlobalProgression_Check);
};
CHECK_SIZE(GlobalProgressionCondition, 0x14);

// 640
class SecondCharacterVehicleValueCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond640_SecondCharacterVehicleValue_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond640_SecondCharacterVehicleValue_Check);
};
CHECK_SIZE(SecondCharacterVehicleValueCondition, 0x14);

// 641
class IsMoviePlayingCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond641_IsMoviePlaying_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond641_IsMoviePlaying_Check);
};
CHECK_SIZE(IsMoviePlayingCondition, 0x14);

// 642
class PlayerFlag14Condition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond642_PlayerFlag14_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond642_PlayerFlag14_Check);
};
CHECK_SIZE(PlayerFlag14Condition, 0x14);

// 643
class GameStateIsCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond643_GameStateIs_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond643_GameStateIs_Check);
};
CHECK_SIZE(GameStateIsCondition, 0x14);

// 644
class TriggeredByOtherCharacterCondition : public ScriptCondition
{
public:
    void Destroy(u32 destroyFlags) RETAIL(Cond644_TriggeredByOtherCharacter_Dtor);
    f32 Check(GameNode* node, BehaviourLevel* level, const u32* time) RETAIL(Cond644_TriggeredByOtherCharacter_Check);
};
CHECK_SIZE(TriggeredByOtherCharacterCondition, 0x14);

extern "C"
{
    extern const GccVTableEntry g_NextConditionVTable[] RETAIL(AlwaysCondition_Methods);
    extern const GccVTableEntry g_IsCollidableConditionVTable[] RETAIL(vt_Cond1_IsCollidable);
    extern const GccVTableEntry g_ElseConditionVTable[] RETAIL(DefaultCaseCondition_Methods);
    extern const GccVTableEntry g_RandomConditionVTable[] RETAIL(RandFloatCondition_Methods);
    extern const GccVTableEntry g_IsVisibleConditionVTable[] RETAIL(vt_Cond4_IsVisible);
    extern const GccVTableEntry g_TimeInUnitConditionVTable[] RETAIL(AnimationProgressCondition__Methods);
    extern const GccVTableEntry g_IsInExternalScriptConditionVTable[] RETAIL(vt_Cond6_IsInExternalScript);
    extern const GccVTableEntry g_AnimationFinishedConditionVTable[] RETAIL(AnimationEndedCondition__Methods);
    extern const GccVTableEntry g_IsPathCompleteConditionVTable[] RETAIL(vt_Cond8_IsPathComplete);
    extern const GccVTableEntry g_MeToInitPosSqrDistConditionVTable[] RETAIL(DistanceFromSpawnCondition_Methods);
    extern const GccVTableEntry g_MeToFocusSqrDistConditionVTable[] RETAIL(DistanceFromPlayerCondition_Methods);
    extern const GccVTableEntry g_CurrentKeyConditionVTable[] RETAIL(vt_Cond11_CurrentKey);
    extern const GccVTableEntry g_IsLoadZoneStateSetConditionVTable[] RETAIL(CheckPersistentFlagCondition_Methods);
    extern const GccVTableEntry g_GetRouteConditionVTable[] RETAIL(vt_Cond13_GetRoute);
    extern const GccVTableEntry g_GotKeysConditionVTable[] RETAIL(vt_Cond14_GotKeys);
    extern const GccVTableEntry g_InsideEdgeStartNodeConditionVTable[] RETAIL(vt_Cond20_InsideEdgeStartNode);
    extern const GccVTableEntry g_InsideEdgeEndNodeConditionVTable[] RETAIL(vt_Cond21_InsideEdgeEndNode);
    extern const GccVTableEntry g_MeFacingFocusConditionVTable[] RETAIL(vt_Cond22_MeFacingFocus);
    extern const GccVTableEntry g_FocusFacingMeConditionVTable[] RETAIL(vt_Cond23_FocusFacingMe);
    extern const GccVTableEntry g_ClearLineOfSightToFocusConditionVTable[] RETAIL(vt_Cond24_ClearLineOfSightToFocus);
    extern const GccVTableEntry g_FocusAgentCanSeeMeConditionVTable[] RETAIL(vt_Cond25_FocusAgentCanSeeMe);
    extern const GccVTableEntry g_CanSeeFocusConditionVTable[] RETAIL(vt_Cond26_CanSeeFocus);
    extern const GccVTableEntry g_MeFacingRouteNodeConditionVTable[] RETAIL(vt_Cond27_MeFacingRouteNode);
    extern const GccVTableEntry g_ClearLineOfSightToRouteNodeConditionVTable[] RETAIL(vt_Cond28_ClearLineOfSightToRouteNode);
    extern const GccVTableEntry g_HeightAboveFocusConditionVTable[] RETAIL(vt_Cond29_HeightAboveFocus);
    extern const GccVTableEntry g_MeFacingCameraConditionVTable[] RETAIL(vt_Cond35_MeFacingCamera);
    extern const GccVTableEntry g_CameraFacingMeConditionVTable[] RETAIL(vt_Cond36_CameraFacingMe);
    extern const GccVTableEntry g_InCameraFrustrumConditionVTable[] RETAIL(vt_Cond37_InCameraFrustrum);
    extern const GccVTableEntry g_ClearLineOfSightToCameraConditionVTable[] RETAIL(vt_Cond38_ClearLineOfSightToCamera);
    extern const GccVTableEntry g_CameraCanSeeMeConditionVTable[] RETAIL(vt_Cond39_CameraCanSeeMe);
    extern const GccVTableEntry g_HeadLookingAtFocusConditionVTable[] RETAIL(vt_Cond40_HeadLookingAtFocus);
    extern const GccVTableEntry g_HeadCanSeeFocusConditionVTable[] RETAIL(vt_Cond41_HeadCanSeeFocus);
    extern const GccVTableEntry g_HeadLookingAtRouteNodeConditionVTable[] RETAIL(vt_Cond42_HeadLookingAtRouteNode);
    extern const GccVTableEntry g_FocusHeadLookingAtMeConditionVTable[] RETAIL(vt_Cond43_FocusHeadLookingAtMe);
    extern const GccVTableEntry g_FocusHeadCanSeeMeConditionVTable[] RETAIL(vt_Cond44_FocusHeadCanSeeMe);
    extern const GccVTableEntry g_GotAnyFocusConditionVTable[] RETAIL(vt_Cond45_GotAnyFocus);
    extern const GccVTableEntry g_FocusActorEqualsConditionVTable[] RETAIL(vt_Cond46_FocusActorEquals);
    extern const GccVTableEntry g_ActorSubtypeEqualsConditionVTable[] RETAIL(InstanceTypeCondition_Methods);
    extern const GccVTableEntry g_AttachedToAnAgentConditionVTable[] RETAIL(vt_Cond48_AttachedToAnAgent);
    extern const GccVTableEntry g_GotAttachedObjectConditionVTable[] RETAIL(vt_Cond49_GotAttachedObject);
    extern const GccVTableEntry g_GotAnyUserMessageConditionVTable[] RETAIL(vt_Cond50_GotAnyUserMessage);
    extern const GccVTableEntry g_GotUserMessageEqualsConditionVTable[] RETAIL(vt_Cond51_GotUserMessageEquals);
    extern const GccVTableEntry g_CurrentKeyEqualsConditionVTable[] RETAIL(vt_Cond52_CurrentKeyEquals);
    extern const GccVTableEntry g_TouchingTerrainConditionVTable[] RETAIL(vt_Cond53_TouchingTerrain);
    extern const GccVTableEntry g_TouchingAnyAgentConditionVTable[] RETAIL(vt_Cond54_TouchingAnyAgent);
    extern const GccVTableEntry g_GotAttachmentOnExitConditionVTable[] RETAIL(vt_Cond55_GotAttachmentOnExit);
    extern const GccVTableEntry g_GotFocusObjectConditionVTable[] RETAIL(vt_Cond56_GotFocusObject);
    extern const GccVTableEntry g_GotFocusPositionConditionVTable[] RETAIL(vt_Cond57_GotFocusPosition);
    extern const GccVTableEntry g_GotAnimationTimeRemainingConditionVTable[] RETAIL(vt_Cond58_GotAnimationTimeRemaining);
    extern const GccVTableEntry g_CounterValueConditionVTable[] RETAIL(vt_Cond59_CounterValue);
    extern const GccVTableEntry g_CounterValueEqualsThresholdConditionVTable[] RETAIL(vt_Cond60_CounterValueEqualsThreshold);
    extern const GccVTableEntry g_IsRestingConditionVTable[] RETAIL(vt_Cond61_IsResting);
    extern const GccVTableEntry g_SqrMoveSpeedConditionVTable[] RETAIL(vt_Cond62_SqrMoveSpeed);
    extern const GccVTableEntry g_FocusHasAttachmentConditionVTable[] RETAIL(vt_Cond63_FocusHasAttachment);
    extern const GccVTableEntry g_LostAllAttachmentsConditionVTable[] RETAIL(vt_Cond64_LostAllAttachments);
    extern const GccVTableEntry g_IsBusyConditionVTable[] RETAIL(vt_Cond65_IsBusy);
    extern const GccVTableEntry g_FocusIsBusyConditionVTable[] RETAIL(vt_Cond66_FocusIsBusy);
    extern const GccVTableEntry g_SoftFlagSetConditionVTable[] RETAIL(InstanceFlagCondition_Methods);
    extern const GccVTableEntry g_MeToCurrentKeySqrDistConditionVTable[] RETAIL(vt_Cond68_MeToCurrentKeySqrDist);
    extern const GccVTableEntry g_SpeedTowardsNextKeyConditionVTable[] RETAIL(vt_Cond70_SpeedTowardsNextKey);
    extern const GccVTableEntry g_BoxAboveIsOverlappedConditionVTable[] RETAIL(vt_Cond72_BoxAboveIsOverlapped);
    extern const GccVTableEntry g_GotLinkedObjectConditionVTable[] RETAIL(vt_Cond73_GotLinkedObject);
    extern const GccVTableEntry g_XCycleConditionVTable[] RETAIL(vt_Cond74_XCycle);
    extern const GccVTableEntry g_YCycleConditionVTable[] RETAIL(vt_Cond75_YCycle);
    extern const GccVTableEntry g_ZCycleConditionVTable[] RETAIL(vt_Cond76_ZCycle);
    extern const GccVTableEntry g_GotAgentRef1ConditionVTable[] RETAIL(vt_Cond77_GotAgentRef1);
    extern const GccVTableEntry g_GotAgentRef2ConditionVTable[] RETAIL(vt_Cond78_GotAgentRef2);
    extern const GccVTableEntry g_AgentRef1ActorEqualsConditionVTable[] RETAIL(vt_Cond79_AgentRef1ActorEquals);
    extern const GccVTableEntry g_AgentRef2ActorEqualsConditionVTable[] RETAIL(vt_Cond80_AgentRef2ActorEquals);
    extern const GccVTableEntry g_HasInstancePositionConditionVTable[] RETAIL(vt_Cond81_Unknown);
    extern const GccVTableEntry g_HasFocusPositionConditionVTable[] RETAIL(vt_Cond82_Unknown);
    extern const GccVTableEntry g_NodeFlag16ConditionVTable[] RETAIL(vt_Cond83_Unknown);
    extern const GccVTableEntry g_NodeFlag17ConditionVTable[] RETAIL(vt_Cond84_Unknown);
    extern const GccVTableEntry g_NodeFlag15ConditionVTable[] RETAIL(vt_Cond85_Unknown);
    extern const GccVTableEntry g_NodeByte154FractionConditionVTable[] RETAIL(vt_Cond86_Unknown);
    extern const GccVTableEntry g_PhysicsBodyFlag1ConditionVTable[] RETAIL(vt_Cond87_Unknown);
    extern const GccVTableEntry g_DistanceToTargetConditionVTable[] RETAIL(vt_Cond88_DistanceToTarget);
    extern const GccVTableEntry g_FocusPositionDistanceSquaredConditionVTable[] RETAIL(vt_Cond89_Unknown);
    extern const GccVTableEntry g_GroundBelowFocusPositionConditionVTable[] RETAIL(vt_Cond90_Unknown);
    extern const GccVTableEntry g_ObjectInstanceByteAtConditionVTable[] RETAIL(vt_Cond91_ObjectInstanceByteAt);
    extern const GccVTableEntry g_InstanceSubtypeConditionVTable[] RETAIL(vt_Cond92_InstanceSubtype);
    extern const GccVTableEntry g_HeadTrackingFlag24ConditionVTable[] RETAIL(vt_Cond93_Unknown);
    extern const GccVTableEntry g_HeadTrackingFlag25ConditionVTable[] RETAIL(vt_Cond94_Unknown);
    extern const GccVTableEntry g_HeadTrackingFlag26ConditionVTable[] RETAIL(vt_Cond95_Unknown);
    extern const GccVTableEntry g_HeadTrackingFlag27ConditionVTable[] RETAIL(vt_Cond96_Unknown);
    extern const GccVTableEntry g_HeadTrackingFlag28ConditionVTable[] RETAIL(vt_Cond97_Unknown);
    extern const GccVTableEntry g_SubPathKeyRawConditionVTable[] RETAIL(vt_Cond102_Unknown);
    extern const GccVTableEntry g_SubPathKeyConditionVTable[] RETAIL(vt_Cond103_Unknown);
    extern const GccVTableEntry g_SubPathKeyDistanceSquaredConditionVTable[] RETAIL(vt_Cond104_Unknown);
    extern const GccVTableEntry g_PhysicsCount8cConditionVTable[] RETAIL(vt_Cond105_Unknown);
    extern const GccVTableEntry g_PhysicsHasContactsConditionVTable[] RETAIL(vt_Cond106_Unknown);
    extern const GccVTableEntry g_SubPathPreviousKeyDistanceSquaredConditionVTable[] RETAIL(vt_Cond107_Unknown);
    extern const GccVTableEntry g_PhysicsHasGroundConditionVTable[] RETAIL(vt_Cond108_Unknown);
    extern const GccVTableEntry g_CurrentLinkIndexConditionVTable[] RETAIL(vt_Cond109_Unknown);
    extern const GccVTableEntry g_HeightAboveStartConditionVTable[] RETAIL(vt_Cond110_Unknown);
    extern const GccVTableEntry g_HasPerception0ConditionVTable[] RETAIL(vt_Cond111_Unknown);
    extern const GccVTableEntry g_HasPerception2ConditionVTable[] RETAIL(vt_Cond112_Unknown);
    extern const GccVTableEntry g_HasPerception1ConditionVTable[] RETAIL(vt_Cond113_Unknown);
    extern const GccVTableEntry g_CharacterAnalogConditionVTable[] RETAIL(vt_Cond114_Unknown);
    extern const GccVTableEntry g_AlwaysZeroConditionVTable[] RETAIL(vt_Cond115_AlwaysZero);
    extern const GccVTableEntry g_PhysicsBodyFlag5ConditionVTable[] RETAIL(vt_Cond116_Unknown);
    extern const GccVTableEntry g_GotUserMessageOnceEqualsConditionVTable[] RETAIL(vt_Cond117_GotUserMessageOnceEquals);
    extern const GccVTableEntry g_HasXLinksConditionVTable[] RETAIL(vt_Cond118_HasXLinks);
    extern const GccVTableEntry g_PositionXConditionVTable[] RETAIL(vt_Cond119_Unknown);
    extern const GccVTableEntry g_PositionYConditionVTable[] RETAIL(vt_Cond120_Unknown);
    extern const GccVTableEntry g_PositionZConditionVTable[] RETAIL(vt_Cond121_Unknown);
    extern const GccVTableEntry g_AgentRef1SpawnFlagConditionVTable[] RETAIL(vt_Cond122_Unknown);
    extern const GccVTableEntry g_IsAttachedConditionVTable[] RETAIL(vt_Cond124_Unknown);
    extern const GccVTableEntry g_PhysicsImpactConditionVTable[] RETAIL(vt_Cond125_Unknown);
    extern const GccVTableEntry g_FocusForwardDotConditionVTable[] RETAIL(vt_Cond126_Unknown);
    extern const GccVTableEntry g_FocusObjectProp0EqualsConditionVTable[] RETAIL(vt_Cond127_Unknown);
    extern const GccVTableEntry g_AngleToFocusConditionVTable[] RETAIL(vt_Cond128_Unknown);
    extern const GccVTableEntry g_KeyPathOnLastKeyConditionVTable[] RETAIL(vt_Cond132_KeyPathOnLastKey);
    extern const GccVTableEntry g_ContextValue154SetConditionVTable[] RETAIL(vt_Cond133_Unknown);
    extern const GccVTableEntry g_KeyPathProgressConditionVTable[] RETAIL(vt_Cond134_Unknown);
    extern const GccVTableEntry g_KeyPathByte42ConditionVTable[] RETAIL(vt_Cond135_Unknown);
    extern const GccVTableEntry g_FocusVisibleConditionVTable[] RETAIL(vt_Cond136_Unknown);
    extern const GccVTableEntry g_KeyPathNumKeysConditionVTable[] RETAIL(vt_Cond137_KeyPathNumKeys);
    extern const GccVTableEntry g_FocusToAgentRef1DistanceSquaredConditionVTable[] RETAIL(vt_Cond138_Unknown);
    extern const GccVTableEntry g_FocusDistanceFromStartSquaredConditionVTable[] RETAIL(vt_Cond139_Unknown);
    extern const GccVTableEntry g_AgentRef1HeightDifferenceConditionVTable[] RETAIL(vt_Cond140_Unknown);
    extern const GccVTableEntry g_FocusOffXAxisDistanceSquaredConditionVTable[] RETAIL(vt_Cond141_Unknown);
    extern const GccVTableEntry g_FocusHorizontalDistanceSquaredConditionVTable[] RETAIL(vt_Cond142_Unknown);
    extern const GccVTableEntry g_FocusOffForwardAxisDistanceSquaredConditionVTable[] RETAIL(vt_Cond143_Unknown);
    extern const GccVTableEntry g_AgentRef1OffXAxisDistanceSquaredConditionVTable[] RETAIL(vt_Cond144_Unknown);
    extern const GccVTableEntry g_AgentRef1HorizontalDistanceSquaredConditionVTable[] RETAIL(vt_Cond145_Unknown);
    extern const GccVTableEntry g_AgentRef1OffAxisDistanceSquaredConditionVTable[] RETAIL(vt_Cond146_Unknown);
    extern const GccVTableEntry g_PhysicsTouchingConditionVTable[] RETAIL(vt_Cond147_Unknown);
    extern const GccVTableEntry g_AgentRef1SideOffsetConditionVTable[] RETAIL(vt_Cond148_Unknown);
    extern const GccVTableEntry g_AgentRef1VisibleConditionVTable[] RETAIL(vt_Cond149_Unknown);
    extern const GccVTableEntry g_AgentRef1InViewConeConditionVTable[] RETAIL(vt_Cond150_Unknown);
    extern const GccVTableEntry g_FocusFlag10ConditionVTable[] RETAIL(vt_Cond151_Unknown);
    extern const GccVTableEntry g_IsInPlayerChunkConditionVTable[] RETAIL(vt_Cond152_Unknown);
    extern const GccVTableEntry g_HasScriptInSlotConditionVTable[] RETAIL(vt_Cond153_Unknown);
    extern const GccVTableEntry g_CurrentKeyIsEvenConditionVTable[] RETAIL(vt_Cond154_Unknown);
    extern const GccVTableEntry g_FocusFromExitPointConditionVTable[] RETAIL(vt_Cond155_Unknown);
    extern const GccVTableEntry g_VideoStateIs5ConditionVTable[] RETAIL(vt_Cond156_Unknown);
    extern const GccVTableEntry g_UpVectorXConditionVTable[] RETAIL(vt_Cond157_Unknown);
    extern const GccVTableEntry g_IntProp0Bit0ConditionVTable[] RETAIL(vt_Cond158_Unknown);
    extern const GccVTableEntry g_VideoReadyConditionVTable[] RETAIL(vt_Cond159_Unknown);
    extern const GccVTableEntry g_NearestPointEdgeDistanceSquaredConditionVTable[] RETAIL(vt_Cond160_Unknown);
    extern const GccVTableEntry g_FocusIsAgentRef1ConditionVTable[] RETAIL(vt_Cond161_Unknown);
    extern const GccVTableEntry g_PhysicsHasCollisionNodeConditionVTable[] RETAIL(vt_Cond162_Unknown);
    extern const GccVTableEntry g_FocusObjectByte0EqualsConditionVTable[] RETAIL(vt_Cond163_Unknown);
    extern const GccVTableEntry g_SplineDistanceToAgentRef1ConditionVTable[] RETAIL(vt_Cond167_Unknown);
    extern const GccVTableEntry g_ObstacleAheadConditionVTable[] RETAIL(vt_Cond168_Unknown);
    extern const GccVTableEntry g_NodeValue174CountConditionVTable[] RETAIL(vt_Cond169_Unknown);
    extern const GccVTableEntry g_IsFullInstanceNodeConditionVTable[] RETAIL(vt_Cond170_Unknown);
    extern const GccVTableEntry g_NodeByte8cMinusGlobalConditionVTable[] RETAIL(vt_Cond171_Unknown);
    extern const GccVTableEntry g_TimeSinceMarkConditionVTable[] RETAIL(vt_Cond172_Unknown);
    extern const GccVTableEntry g_AlwaysZero173ConditionVTable[] RETAIL(vt_Cond173_Unknown);
    extern const GccVTableEntry g_AlwaysConditionVTable[] RETAIL(vt_Cond174_Unknown);
    extern const GccVTableEntry g_NeverConditionVTable[] RETAIL(vt_Cond175_Unknown);
    extern const GccVTableEntry g_ChunksLoadedConditionVTable[] RETAIL(vt_Cond176_ChunksLoaded);
    extern const GccVTableEntry g_FocusInSameChunkConditionVTable[] RETAIL(vt_Cond177_Unknown);
    extern const GccVTableEntry g_PlayerHitPointsConditionVTable[] RETAIL(vt_Cond512_PlayerHitPoints);
    extern const GccVTableEntry g_PlayerIsCrouchingConditionVTable[] RETAIL(vt_Cond514_PlayerIsCrouching);
    extern const GccVTableEntry g_PlayerIsGroundedConditionVTable[] RETAIL(vt_Cond515_PlayerIsGrounded);
    extern const GccVTableEntry g_MeToPlayerSqrDistConditionVTable[] RETAIL(vt_Cond517_MeToPlayerSqrDist);
    extern const GccVTableEntry g_AgentIsOnGroundConditionVTable[] RETAIL(vt_Cond518_AgentIsOnGround);
    extern const GccVTableEntry g_CrateHasRedWumpaConditionVTable[] RETAIL(vt_Cond519_CrateHasRedWumpa);
    extern const GccVTableEntry g_AgentWasTouchedConditionVTable[] RETAIL(vt_Cond520_AgentWasTouched);
    extern const GccVTableEntry g_AgentWasSpunConditionVTable[] RETAIL(vt_Cond521_AgentWasSpun);
    extern const GccVTableEntry g_AgentWasKneeDroppedConditionVTable[] RETAIL(vt_Cond522_AgentWasKneeDropped);
    extern const GccVTableEntry g_AgentWasSlidConditionVTable[] RETAIL(vt_Cond523_AgentWasSlid);
    extern const GccVTableEntry g_AgentHitPointsConditionVTable[] RETAIL(vt_Cond524_AgentHitPoints);
    extern const GccVTableEntry g_CanMoveForwardsConditionVTable[] RETAIL(vt_Cond525_CanMoveForwards);
    extern const GccVTableEntry g_CanMoveBackwardsConditionVTable[] RETAIL(vt_Cond526_CanMoveBackwards);
    extern const GccVTableEntry g_CanStrafeLeftConditionVTable[] RETAIL(vt_Cond527_CanStrafeLeft);
    extern const GccVTableEntry g_CanStrafeRightConditionVTable[] RETAIL(vt_Cond528_CanStrafeRight);
    extern const GccVTableEntry g_CanJumpForwardsConditionVTable[] RETAIL(vt_Cond529_CanJumpForwards);
    extern const GccVTableEntry g_CanFallConditionVTable[] RETAIL(vt_Cond530_CanFall);
    extern const GccVTableEntry g_WillHitLowWallConditionVTable[] RETAIL(vt_Cond531_WillHitLowWall);
    extern const GccVTableEntry g_WillHitWallConditionVTable[] RETAIL(vt_Cond532_WillHitWall);
    extern const GccVTableEntry g_WillRunOffCliffConditionVTable[] RETAIL(vt_Cond533_WillRunOffCliff);
    extern const GccVTableEntry g_AgentWasAttackedConditionVTable[] RETAIL(vt_Cond534_AgentWasAttacked);
    extern const GccVTableEntry g_AgentWasJumpedOnConditionVTable[] RETAIL(vt_Cond535_AgentWasJumpedOn);
    extern const GccVTableEntry g_AgentWasWalkedIntoConditionVTable[] RETAIL(vt_Cond536_AgentWasWalkedInto);
    extern const GccVTableEntry g_AgentWasHeadbuttedConditionVTable[] RETAIL(vt_Cond537_AgentWasHeadbutted);
    extern const GccVTableEntry g_PlayerToMyFocusSqrDistConditionVTable[] RETAIL(vt_Cond538_PlayerToMyFocusSqrDist);
    extern const GccVTableEntry g_WumpaNeededForPayGateConditionVTable[] RETAIL(vt_Cond539_WumpaNeededForPayGate);
    extern const GccVTableEntry g_MeFacingPlayerConditionVTable[] RETAIL(vt_Cond540_MeFacingPlayer);
    extern const GccVTableEntry g_PlayerFacingMeConditionVTable[] RETAIL(vt_Cond541_PlayerFacingMe);
    extern const GccVTableEntry g_ClearLineOfSightToPlayerConditionVTable[] RETAIL(vt_Cond542_ClearLineOfSightToPlayer);
    extern const GccVTableEntry g_CanSeePlayerConditionVTable[] RETAIL(vt_Cond543_CanSeePlayer);
    extern const GccVTableEntry g_PlayerCanSeeMeConditionVTable[] RETAIL(vt_Cond544_PlayerCanSeeMe);
    extern const GccVTableEntry g_NodeTrafficConditionVTable[] RETAIL(vt_Cond545_NodeTraffic);
    extern const GccVTableEntry g_NodeIsAirborneConditionVTable[] RETAIL(vt_Cond546_NodeIsAirborne);
    extern const GccVTableEntry g_EdgeNeedsJumpConditionVTable[] RETAIL(vt_Cond547_EdgeNeedsJump);
    extern const GccVTableEntry g_EdgeNeedsFlyingConditionVTable[] RETAIL(vt_Cond548_EdgeNeedsFlying);
    extern const GccVTableEntry g_EdgeNeedsLongJumpConditionVTable[] RETAIL(vt_Cond550_EdgeNeedsLongJump);
    extern const GccVTableEntry g_EdgeNeedsHighJumpConditionVTable[] RETAIL(vt_Cond551_EdgeNeedsHighJump);
    extern const GccVTableEntry g_HeightAbovePlayerConditionVTable[] RETAIL(vt_Cond552_HeightAbovePlayer);
    extern const GccVTableEntry g_HeadLookingAtPlayerConditionVTable[] RETAIL(vt_Cond553_HeadLookingAtPlayer);
    extern const GccVTableEntry g_HeadCanSeePlayerConditionVTable[] RETAIL(vt_Cond554_HeadCanSeePlayer);
    extern const GccVTableEntry g_PlayerHeadLookingAtMeConditionVTable[] RETAIL(vt_Cond555_PlayerHeadLookingAtMe);
    extern const GccVTableEntry g_PlayerHeadCanSeeMeConditionVTable[] RETAIL(vt_Cond556_PlayerHeadCanSeeMe);
    extern const GccVTableEntry g_PlayerIsMovingConditionVTable[] RETAIL(vt_Cond557_PlayerIsMoving);
    extern const GccVTableEntry g_PlayerIsWalkingConditionVTable[] RETAIL(vt_Cond558_PlayerIsWalking);
    extern const GccVTableEntry g_PlayerIsRunningConditionVTable[] RETAIL(vt_Cond559_PlayerIsRunning);
    extern const GccVTableEntry g_PlayerIsCrawlingConditionVTable[] RETAIL(vt_Cond560_PlayerIsCrawling);
    extern const GccVTableEntry g_PlayerIsFallingConditionVTable[] RETAIL(vt_Cond561_PlayerIsFalling);
    extern const GccVTableEntry g_PlayerIsCoOpLinkedConditionVTable[] RETAIL(vt_Cond562_PlayerIsCoOpLinked);
    extern const GccVTableEntry g_PlayerHoldingMultiToolConditionVTable[] RETAIL(vt_Cond563_PlayerHoldingMultiTool);
    extern const GccVTableEntry g_PlayerIsSlammingConditionVTable[] RETAIL(vt_Cond564_PlayerIsSlamming);
    extern const GccVTableEntry g_PlayerIsSpinningConditionVTable[] RETAIL(vt_Cond565_PlayerIsSpinning);
    extern const GccVTableEntry g_PlayerIsJumpingConditionVTable[] RETAIL(vt_Cond566_PlayerIsJumping);
    extern const GccVTableEntry g_HeadCanSeePlayerUnblockedConditionVTable[] RETAIL(vt_Cond567_HeadCanSeePlayerUnblocked);
    extern const GccVTableEntry g_AmIHarmfulConditionVTable[] RETAIL(vt_Cond568_AmIHarmful);
    extern const GccVTableEntry g_AttachedContextFlag8ConditionVTable[] RETAIL(vt_Cond569_Unknown);
    extern const GccVTableEntry g_DUMMY_570ConditionVTable[] RETAIL(vt_Cond570_Unknown);
    extern const GccVTableEntry g_DUMMY_571ConditionVTable[] RETAIL(vt_Cond571_Unknown);
    extern const GccVTableEntry g_CutsceneSkippedConditionVTable[] RETAIL(vt_Cond572_CutsceneSkipped);
    extern const GccVTableEntry g_IsCirclePressedConditionVTable[] RETAIL(vt_Cond573_Unknown);
    extern const GccVTableEntry g_IsSquarePressedConditionVTable[] RETAIL(vt_Cond574_Unknown);
    extern const GccVTableEntry g_IsTrianglePressedConditionVTable[] RETAIL(vt_Cond575_Unknown);
    extern const GccVTableEntry g_IsR1PressedConditionVTable[] RETAIL(vt_Cond576_Unknown);
    extern const GccVTableEntry g_CharacterVehiclePointerConditionVTable[] RETAIL(vt_Cond577_Unknown);
    extern const GccVTableEntry g_CharacterFlag23ConditionVTable[] RETAIL(vt_Cond578_Unknown);
    extern const GccVTableEntry g_IsChargedShotConditionVTable[] RETAIL(vt_Cond579_IsChargedShot);
    extern const GccVTableEntry g_IsDownBlastConditionVTable[] RETAIL(vt_Cond580_IsDownBlast);
    extern const GccVTableEntry g_GlobalInstanceOp581ConditionVTable[] RETAIL(vt_Cond581_Unknown);
    extern const GccVTableEntry g_PlayerVisibleConditionVTable[] RETAIL(vt_Cond582_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag0ConditionVTable[] RETAIL(vt_Cond583_Unknown);
    extern const GccVTableEntry g_PlayerVisible2ConditionVTable[] RETAIL(vt_Cond584_Unknown);
    extern const GccVTableEntry g_PlayerVisible3ConditionVTable[] RETAIL(vt_Cond585_Unknown);
    extern const GccVTableEntry g_CharacterFlag22ConditionVTable[] RETAIL(vt_Cond586_Unknown);
    extern const GccVTableEntry g_IsPlayerConditionVTable[] RETAIL(vt_Cond587_Unknown);
    extern const GccVTableEntry g_ObjectContextFlag17ConditionVTable[] RETAIL(vt_Cond588_Unknown);
    extern const GccVTableEntry g_HitByPunchConditionVTable[] RETAIL(vt_Cond589_HitByPunch);
    extern const GccVTableEntry g_HitByBodySlam2ConditionVTable[] RETAIL(vt_Cond590_HitByBodySlam2);
    extern const GccVTableEntry g_HitBySpinHitboxConditionVTable[] RETAIL(vt_Cond591_HitBySpinHitbox);
    extern const GccVTableEntry g_HitByBodySlamHitboxConditionVTable[] RETAIL(vt_Cond592_HitByBodySlamHitbox);
    extern const GccVTableEntry g_CharacterHasVehicleConditionVTable[] RETAIL(vt_Cond593_Unknown);
    extern const GccVTableEntry g_IsVehicleRollerbrawlConditionVTable[] RETAIL(vt_Cond594_IsVehicleRollerbrawl);
    extern const GccVTableEntry g_VehicleTypeNot2ConditionVTable[] RETAIL(vt_Cond595_Unknown);
    extern const GccVTableEntry g_IsVehicleHumiliskateConditionVTable[] RETAIL(vt_Cond596_IsVehicleHumiliskate);
    extern const GccVTableEntry g_VehicleTypeNot4ConditionVTable[] RETAIL(vt_Cond597_Unknown);
    extern const GccVTableEntry g_IsVehicle3ConditionVTable[] RETAIL(vt_Cond598_IsVehicle3);
    extern const GccVTableEntry g_SubPathPointFlag5ConditionVTable[] RETAIL(vt_Cond599_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag4ConditionVTable[] RETAIL(vt_Cond600_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag6ConditionVTable[] RETAIL(vt_Cond601_Unknown);
    extern const GccVTableEntry g_PathSegmentFlag0ConditionVTable[] RETAIL(vt_Cond602_Unknown);
    extern const GccVTableEntry g_SubPathPreviousPointFlag5ConditionVTable[] RETAIL(vt_Cond603_Unknown);
    extern const GccVTableEntry g_SubPathPreviousPointFlag4ConditionVTable[] RETAIL(vt_Cond604_Unknown);
    extern const GccVTableEntry g_SubPathPreviousPointFlag6ConditionVTable[] RETAIL(vt_Cond605_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag5bConditionVTable[] RETAIL(vt_Cond606_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag4bConditionVTable[] RETAIL(vt_Cond607_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag6bConditionVTable[] RETAIL(vt_Cond608_Unknown);
    extern const GccVTableEntry g_PlayerVectorLengthDifferenceConditionVTable[] RETAIL(vt_Cond609_Unknown);
    extern const GccVTableEntry g_HitByCortexBoltConditionVTable[] RETAIL(vt_Cond610_HitByCortexBolt);
    extern const GccVTableEntry g_ObjectContextFlag1ConditionVTable[] RETAIL(vt_Cond611_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag2ConditionVTable[] RETAIL(vt_Cond612_Unknown);
    extern const GccVTableEntry g_SubPathPreviousPointFlag2ConditionVTable[] RETAIL(vt_Cond613_Unknown);
    extern const GccVTableEntry g_SubPathPointFlag2bConditionVTable[] RETAIL(vt_Cond614_Unknown);
    extern const GccVTableEntry g_SubPathPointFlags56ConditionVTable[] RETAIL(vt_Cond615_Unknown);
    extern const GccVTableEntry g_FocusPositionToPlayerDistanceSquaredConditionVTable[] RETAIL(vt_Cond616_Unknown);
    extern const GccVTableEntry g_IsPushingObjectConditionVTable[] RETAIL(vt_Cond617_IsPushingObject);
    extern const GccVTableEntry g_PlayerSideOffsetConditionVTable[] RETAIL(vt_Cond618_Unknown);
    extern const GccVTableEntry g_PlayerNearCurrentKeyConditionVTable[] RETAIL(vt_Cond619_Unknown);
    extern const GccVTableEntry g_PlayerSplineVehicleValueConditionVTable[] RETAIL(vt_Cond620_Unknown);
    extern const GccVTableEntry g_CharacterHasHomeChunkConditionVTable[] RETAIL(vt_Cond621_Unknown);
    extern const GccVTableEntry g_PlayerFlag57ClearConditionVTable[] RETAIL(vt_Cond622_Unknown);
    extern const GccVTableEntry g_HasActorWeightConditionVTable[] RETAIL(vt_Cond623_HasActorWeight);
    extern const GccVTableEntry g_GameFlags44Is12ConditionVTable[] RETAIL(vt_Cond624_Unknown);
    extern const GccVTableEntry g_ObjectContextFlag25ConditionVTable[] RETAIL(vt_Cond625_Unknown);
    extern const GccVTableEntry g_ObjectContextFlag2ConditionVTable[] RETAIL(vt_Cond626_Unknown);
    extern const GccVTableEntry g_PlayerVehicle1ValueConditionVTable[] RETAIL(vt_Cond627_Unknown);
    extern const GccVTableEntry g_BothCharactersFlag14ConditionVTable[] RETAIL(vt_Cond628_Unknown);
    extern const GccVTableEntry g_GlobalInt3098e8ConditionVTable[] RETAIL(vt_Cond629_Unknown);
    extern const GccVTableEntry g_NodeValue134SetConditionVTable[] RETAIL(vt_Cond630_Unknown);
    extern const GccVTableEntry g_GameControllerField500HighConditionVTable[] RETAIL(vt_Cond631_Unknown);
    extern const GccVTableEntry g_GameTimer57cConditionVTable[] RETAIL(vt_Cond632_Unknown);
    extern const GccVTableEntry g_SecondCharacterGunStateConditionVTable[] RETAIL(vt_Cond633_Unknown);
    extern const GccVTableEntry g_HasAmmoConditionVTable[] RETAIL(vt_Cond634_HasAmmo);
    extern const GccVTableEntry g_CameraForwardDistanceConditionVTable[] RETAIL(vt_Cond635_Unknown);
    extern const GccVTableEntry g_ObjectContextFlag19ConditionVTable[] RETAIL(vt_Cond636_Unknown);
    extern const GccVTableEntry g_GameModeIs5ConditionVTable[] RETAIL(vt_Cond637_Unknown);
    extern const GccVTableEntry g_ObjectContextFlags3or22ConditionVTable[] RETAIL(vt_Cond638_Unknown);
    extern const GccVTableEntry g_GlobalProgressionConditionVTable[] RETAIL(vt_Cond639_GlobalProgression);
    extern const GccVTableEntry g_SecondCharacterVehicleValueConditionVTable[] RETAIL(vt_Cond640_Unknown);
    extern const GccVTableEntry g_IsMoviePlayingConditionVTable[] RETAIL(vt_Cond641_IsMoviePlaying);
    extern const GccVTableEntry g_PlayerFlag14ConditionVTable[] RETAIL(vt_Cond642_Unknown);
    extern const GccVTableEntry g_GameStateIsConditionVTable[] RETAIL(vt_Cond643_Unknown);
    extern const GccVTableEntry g_TriggeredByOtherCharacterConditionVTable[] RETAIL(vt_Cond644_Unknown);

    // The conditions' checks' translation unit's start-up: its static initialisation (initialize 1, priority 0xFFFF: the header's
    // constants, which nothing reads) and its entry in the static constructors' table
    void InitConditionChecksModule(u32 initialize, u32 priority) RETAIL(FUN_00128d50);
    void ConstructConditionChecksModule() RETAIL(FUN_0012d500);
}

