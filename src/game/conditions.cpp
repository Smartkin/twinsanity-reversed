#include "game/conditions.h"

#include "game/memory.h"

namespace
{
template <typename T>
T* NewScriptObject()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}
}

void NextCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsCollidableCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ElseCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RandomCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVisibleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TimeInUnitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void LinkedChunksLoadedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AnimationFinishedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsPathCompleteCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToInitPosSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToFocusSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CurrentKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsLoadZoneStateSetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GetRouteCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotKeysCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InsideEdgeStartNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InsideEdgeEndNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeFacingFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusFacingMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ClearLineOfSightToFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusAgentCanSeeMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanSeeFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeFacingRouteNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ClearLineOfSightToRouteNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeightAboveFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeFacingCameraCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CameraFacingMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InCameraFrustrumCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ClearLineOfSightToCameraCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CameraCanSeeMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadLookingAtFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadCanSeeFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadLookingAtRouteNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusHeadLookingAtMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusHeadCanSeeMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAnyFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusActorEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ActorSubtypeEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AttachedToAnAgentCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAttachedObjectCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAnyUserMessageCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotUserMessageEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CurrentKeyEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TouchingAnythingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TouchingAnyAgentCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAttachmentOnExitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotFocusObjectCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotFocusPositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAnimationTimeRemainingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CounterValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CounterValueEqualsThresholdCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsRestingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SqrMoveSpeedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusHasAttachmentCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void LostAllAttachmentsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsBusyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusIsBusyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SoftFlagSetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToCurrentKeySqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SpeedTowardsNextKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void BoxAboveIsOverlappedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotLinkedObjectCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void XCycleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void YCycleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ZCycleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotAgentRef2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1ActorEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef2ActorEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasStoredPlaceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasStoredPositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FoundCoverCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FoundNoCoverCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CoverSearchEndedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KnockCountdownCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RigidBodyOnGroundCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToAgentRef1SqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToStoredPositionSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GroundBelowFocusPositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InstanceCounterValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InstanceCounterEqualsThresholdCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadAtLimitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadYawBelowLimitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadYawAboveLimitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadPitchBelowLimitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadPitchAboveLimitCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RouteStepUncheckedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RouteStepCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToEdgeStartNodeSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RigidBodyHasMotionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RigidBodyCollidesCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToEdgeEndNodeSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RigidBodyRidesInstanceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void OnLastLinkedObjectCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeightAboveStartCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void Sense0LevelCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void Sense2LevelCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void Sense1LevelCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PresenceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AlwaysZeroCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RigidBodyAgainstWallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotChildMessageOnceEqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasXLinksCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PositionXCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PositionYCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PositionZCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1IsBusyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsAttachedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TouchedMessageSurfaceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusForwardDotCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusObjectProp0EqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TargetToSideCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathOnLastKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasInstanceIdCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathProgressCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathNumPathsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void VisibleFromFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathNumKeysCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusToAgentRef1DistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusDistanceFromStartSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeightAboveAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusAlongXAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusAlongYAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusAlongZAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1AlongXAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1AlongYAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1AlongZAxisSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TouchingWorldCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeFacingAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void VisibleFromAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadCanSeeAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusIsVisibleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsInPlayerChunkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasScriptInSlotCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CurrentKeyIsEvenCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ExitPointToFocusSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CutsceneFinishedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void UpAxisYCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IntProperty0Bit0ClearCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CutsceneMusicReadyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NearestPointEdgeDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusIsAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasPhysicsBodyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusInstanceCounterEqualsThresholdCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SplineDistanceToAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GroundBelowPointAheadCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CountedInstancesCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CountedValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RankAboveGlobalRankCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TimeSinceMarkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoOp173Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AlwaysCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NeverCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void LinkedChunksQueuedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusInSameChunkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerHitPointsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsCrouchingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsGroundedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeToPlayerSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ShadowActiveCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CrateHasWumpaCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasTouchedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasSpunCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasKneeDroppedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasSlidCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentHitPointsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanMoveForwardsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void BlockedBehindCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void BlockedLeftCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void BlockedRightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void WumpaFruitCountCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanFallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoOpWillHitLowWallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasHitByTiedPairCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasHitByThrownCharacterCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasAttackedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasJumpedOnCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasWalkedIntoCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentWasHeadbuttedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerToMyFocusSqrDistCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PayGateNumberCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void MeFacingPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerFacingMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ClearLineOfSightToPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanSeePlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerCanSeeMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerGunShotChargeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeIsAirborneCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeNeedsJumpCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeNeedsFlyingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeNeedsLongJumpCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeNeedsHighJumpCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeightAbovePlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadLookingAtPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadCanSeePlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerHeadLookingAtMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerHeadCanSeeMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsMovingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsWalkingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsRunningCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsWalkingDuplicateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsFallingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsCoOpLinkedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerHoldingMultiToolCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsSlammingTiedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsSpinningCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsSlidingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsAirborneCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadCanSeeNearPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanDamageCharacterCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoOp570Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoOp571Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoOpCutsceneSkippedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsCirclePressedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsSquarePressedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsTrianglePressedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsR1PressedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void RidesVehicleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NoGroundAheadCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerJustShotCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsDownBlastCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ShotAtMeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisibleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeBlockedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisibleFromLeftCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisibleFromRightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsTiedSecondCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByKickCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitBySpinCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByKind18Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByProjectileCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByKneeDropCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CharacterHasVehicleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleRollerbrawlCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleKind2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleHumiliskateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleKind4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleHoverboardCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag6Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeFlag8Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeEndNodeFlag5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeEndNodeFlag4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeEndNodeFlag6Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag5DuplicateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag4DuplicateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag6DuplicateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SpeedAbovePlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByElectricCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByExplosionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeEndNodeFlag2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlag2DuplicateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void EdgeStartNodeFlagsClearCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusPositionToPlayerDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsPushingObjectCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerSideOffsetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerNearerAnotherKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerHumiliskateCrouchedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerOutsideHomeChunkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerScriptFlagClearCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasActorWeightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameIsPlayingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByWaterCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByFallThroughCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerRidesRollerbrawlCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CortexDeadPlayerAliveCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ScriptGlobalFlagCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InWaterCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TimedPlayCountCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TimedPlayTimeLeftCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerGunSecondCountCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerAmmoCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CameraForwardDistanceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByHeavyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PairingIs5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByBurningCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void StoryAreaCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVehicleHeightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsMoviePlayingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsDeadCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayAreaIsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TriggeredByOtherCharacterCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void* BuildScriptCondition(void*, s32 id, s32 kind)
{
    if (kind != ObjectBuilder::ConditionKind)
    {
        return nullptr;
    }

    switch (id)
    {
    case 0:
        return MakeCondition(NewScriptObject<NextCondition>(), 0x0, g_NextConditionVTable);
    case 1:
        return MakeCondition(NewScriptObject<IsCollidableCondition>(), 0x1, g_IsCollidableConditionVTable);
    case 2:
        return MakeCondition(NewScriptObject<ElseCondition>(), 0x2, g_ElseConditionVTable);
    case 3:
        return MakeCondition(NewScriptObject<RandomCondition>(), 0x3, g_RandomConditionVTable);
    case 4:
        return MakeCondition(NewScriptObject<IsVisibleCondition>(), 0x4, g_IsVisibleConditionVTable);
    case 5:
        return MakeCondition(NewScriptObject<TimeInUnitCondition>(), 0x5, g_TimeInUnitConditionVTable);
    case 6:
        return MakeCondition(NewScriptObject<LinkedChunksLoadedCondition>(), 0x6, g_LinkedChunksLoadedConditionVTable);
    case 7:
        return MakeCondition(NewScriptObject<AnimationFinishedCondition>(), 0x7, g_AnimationFinishedConditionVTable);
    case 8:
        return MakeCondition(NewScriptObject<IsPathCompleteCondition>(), 0x8, g_IsPathCompleteConditionVTable);
    case 9:
        return MakeCondition(NewScriptObject<MeToInitPosSqrDistCondition>(), 0x9, g_MeToInitPosSqrDistConditionVTable);
    case 10:
        return MakeCondition(NewScriptObject<MeToFocusSqrDistCondition>(), 0xA, g_MeToFocusSqrDistConditionVTable);
    case 11:
        return MakeCondition(NewScriptObject<CurrentKeyCondition>(), 0xB, g_CurrentKeyConditionVTable);
    case 12:
        return MakeCondition(NewScriptObject<IsLoadZoneStateSetCondition>(), 0xC, g_IsLoadZoneStateSetConditionVTable);
    case 13:
        return MakeCondition(NewScriptObject<GetRouteCondition>(), 0xD, g_GetRouteConditionVTable);
    case 14:
        return MakeCondition(NewScriptObject<GotKeysCondition>(), 0xE, g_GotKeysConditionVTable);
    case 20:
        return MakeCondition(NewScriptObject<InsideEdgeStartNodeCondition>(), 0x14, g_InsideEdgeStartNodeConditionVTable);
    case 21:
        return MakeCondition(NewScriptObject<InsideEdgeEndNodeCondition>(), 0x15, g_InsideEdgeEndNodeConditionVTable);
    case 22:
        return MakeCondition(NewScriptObject<MeFacingFocusCondition>(), 0x16, g_MeFacingFocusConditionVTable);
    case 23:
        return MakeCondition(NewScriptObject<FocusFacingMeCondition>(), 0x17, g_FocusFacingMeConditionVTable);
    case 24:
        return MakeCondition(NewScriptObject<ClearLineOfSightToFocusCondition>(), 0x18, g_ClearLineOfSightToFocusConditionVTable);
    case 25:
        return MakeCondition(NewScriptObject<FocusAgentCanSeeMeCondition>(), 0x19, g_FocusAgentCanSeeMeConditionVTable);
    case 26:
        return MakeCondition(NewScriptObject<CanSeeFocusCondition>(), 0x1A, g_CanSeeFocusConditionVTable);
    case 27:
        return MakeCondition(NewScriptObject<MeFacingRouteNodeCondition>(), 0x1B, g_MeFacingRouteNodeConditionVTable);
    case 28:
        return MakeCondition(NewScriptObject<ClearLineOfSightToRouteNodeCondition>(), 0x1C, g_ClearLineOfSightToRouteNodeConditionVTable);
    case 29:
        return MakeCondition(NewScriptObject<HeightAboveFocusCondition>(), 0x1D, g_HeightAboveFocusConditionVTable);
    case 35:
        return MakeCondition(NewScriptObject<MeFacingCameraCondition>(), 0x23, g_MeFacingCameraConditionVTable);
    case 36:
        return MakeCondition(NewScriptObject<CameraFacingMeCondition>(), 0x24, g_CameraFacingMeConditionVTable);
    case 37:
        return MakeCondition(NewScriptObject<InCameraFrustrumCondition>(), 0x25, g_InCameraFrustrumConditionVTable);
    case 38:
        return MakeCondition(NewScriptObject<ClearLineOfSightToCameraCondition>(), 0x26, g_ClearLineOfSightToCameraConditionVTable);
    case 39:
        return MakeCondition(NewScriptObject<CameraCanSeeMeCondition>(), 0x27, g_CameraCanSeeMeConditionVTable);
    case 40:
        return MakeCondition(NewScriptObject<HeadLookingAtFocusCondition>(), 0x28, g_HeadLookingAtFocusConditionVTable);
    case 41:
        return MakeCondition(NewScriptObject<HeadCanSeeFocusCondition>(), 0x29, g_HeadCanSeeFocusConditionVTable);
    case 42:
        return MakeCondition(NewScriptObject<HeadLookingAtRouteNodeCondition>(), 0x2A, g_HeadLookingAtRouteNodeConditionVTable);
    case 43:
        return MakeCondition(NewScriptObject<FocusHeadLookingAtMeCondition>(), 0x2B, g_FocusHeadLookingAtMeConditionVTable);
    case 44:
        return MakeCondition(NewScriptObject<FocusHeadCanSeeMeCondition>(), 0x2C, g_FocusHeadCanSeeMeConditionVTable);
    case 45:
        return MakeCondition(NewScriptObject<GotAnyFocusCondition>(), 0x2D, g_GotAnyFocusConditionVTable);
    case 46:
        return MakeCondition(NewScriptObject<FocusActorEqualsCondition>(), 0x2E, g_FocusActorEqualsConditionVTable);
    case 47:
        return MakeCondition(NewScriptObject<ActorSubtypeEqualsCondition>(), 0x2F, g_ActorSubtypeEqualsConditionVTable);
    case 48:
        return MakeCondition(NewScriptObject<AttachedToAnAgentCondition>(), 0x30, g_AttachedToAnAgentConditionVTable);
    case 49:
        return MakeCondition(NewScriptObject<GotAttachedObjectCondition>(), 0x31, g_GotAttachedObjectConditionVTable);
    case 50:
        return MakeCondition(NewScriptObject<GotAnyUserMessageCondition>(), 0x32, g_GotAnyUserMessageConditionVTable);
    case 51:
        return MakeCondition(NewScriptObject<GotUserMessageEqualsCondition>(), 0x33, g_GotUserMessageEqualsConditionVTable);
    case 52:
        return MakeCondition(NewScriptObject<CurrentKeyEqualsCondition>(), 0x34, g_CurrentKeyEqualsConditionVTable);
    case 53:
        return MakeCondition(NewScriptObject<TouchingAnythingCondition>(), 0x35, g_TouchingAnythingConditionVTable);
    case 54:
        return MakeCondition(NewScriptObject<TouchingAnyAgentCondition>(), 0x36, g_TouchingAnyAgentConditionVTable);
    case 55:
        return MakeCondition(NewScriptObject<GotAttachmentOnExitCondition>(), 0x37, g_GotAttachmentOnExitConditionVTable);
    case 56:
        return MakeCondition(NewScriptObject<GotFocusObjectCondition>(), 0x38, g_GotFocusObjectConditionVTable);
    case 57:
        return MakeCondition(NewScriptObject<GotFocusPositionCondition>(), 0x39, g_GotFocusPositionConditionVTable);
    case 58:
        return MakeCondition(NewScriptObject<GotAnimationTimeRemainingCondition>(), 0x3A, g_GotAnimationTimeRemainingConditionVTable);
    case 59:
        return MakeCondition(NewScriptObject<CounterValueCondition>(), 0x3B, g_CounterValueConditionVTable);
    case 60:
        return MakeCondition(NewScriptObject<CounterValueEqualsThresholdCondition>(), 0x3C, g_CounterValueEqualsThresholdConditionVTable);
    case 61:
        return MakeCondition(NewScriptObject<IsRestingCondition>(), 0x3D, g_IsRestingConditionVTable);
    case 62:
        return MakeCondition(NewScriptObject<SqrMoveSpeedCondition>(), 0x3E, g_SqrMoveSpeedConditionVTable);
    case 63:
        return MakeCondition(NewScriptObject<FocusHasAttachmentCondition>(), 0x3F, g_FocusHasAttachmentConditionVTable);
    case 64:
        return MakeCondition(NewScriptObject<LostAllAttachmentsCondition>(), 0x40, g_LostAllAttachmentsConditionVTable);
    case 65:
        return MakeCondition(NewScriptObject<IsBusyCondition>(), 0x41, g_IsBusyConditionVTable);
    case 66:
        return MakeCondition(NewScriptObject<FocusIsBusyCondition>(), 0x42, g_FocusIsBusyConditionVTable);
    case 67:
        return MakeCondition(NewScriptObject<SoftFlagSetCondition>(), 0x43, g_SoftFlagSetConditionVTable);
    case 68:
        return MakeCondition(NewScriptObject<MeToCurrentKeySqrDistCondition>(), 0x45, g_MeToCurrentKeySqrDistConditionVTable);
    case 69:
        return MakeCondition(NewScriptObject<MeToCurrentKeySqrDistCondition>(), 0x45, g_MeToCurrentKeySqrDistConditionVTable);
    case 70:
        return MakeCondition(NewScriptObject<SpeedTowardsNextKeyCondition>(), 0x47, g_SpeedTowardsNextKeyConditionVTable);
    case 71:
        return MakeCondition(NewScriptObject<SpeedTowardsNextKeyCondition>(), 0x47, g_SpeedTowardsNextKeyConditionVTable);
    case 72:
        return MakeCondition(NewScriptObject<BoxAboveIsOverlappedCondition>(), 0x48, g_BoxAboveIsOverlappedConditionVTable);
    case 73:
        return MakeCondition(NewScriptObject<GotLinkedObjectCondition>(), 0x49, g_GotLinkedObjectConditionVTable);
    case 74:
        return MakeCondition(NewScriptObject<XCycleCondition>(), 0x4A, g_XCycleConditionVTable);
    case 75:
        return MakeCondition(NewScriptObject<YCycleCondition>(), 0x4B, g_YCycleConditionVTable);
    case 76:
        return MakeCondition(NewScriptObject<ZCycleCondition>(), 0x4C, g_ZCycleConditionVTable);
    case 77:
        return MakeCondition(NewScriptObject<GotAgentRef1Condition>(), 0x4D, g_GotAgentRef1ConditionVTable);
    case 78:
        return MakeCondition(NewScriptObject<GotAgentRef2Condition>(), 0x4E, g_GotAgentRef2ConditionVTable);
    case 79:
        return MakeCondition(NewScriptObject<AgentRef1ActorEqualsCondition>(), 0x4F, g_AgentRef1ActorEqualsConditionVTable);
    case 80:
        return MakeCondition(NewScriptObject<AgentRef2ActorEqualsCondition>(), 0x50, g_AgentRef2ActorEqualsConditionVTable);
    case 81:
        return MakeCondition(NewScriptObject<HasStoredPlaceCondition>(), 0x51, g_HasStoredPlaceConditionVTable);
    case 82:
        return MakeCondition(NewScriptObject<HasStoredPositionCondition>(), 0x52, g_HasStoredPositionConditionVTable);
    case 83:
        return MakeCondition(NewScriptObject<FoundCoverCondition>(), 0x53, g_FoundCoverConditionVTable);
    case 84:
        return MakeCondition(NewScriptObject<FoundNoCoverCondition>(), 0x54, g_FoundNoCoverConditionVTable);
    case 85:
        return MakeCondition(NewScriptObject<CoverSearchEndedCondition>(), 0x55, g_CoverSearchEndedConditionVTable);
    case 86:
        return MakeCondition(NewScriptObject<KnockCountdownCondition>(), 0x56, g_KnockCountdownConditionVTable);
    case 87:
        return MakeCondition(NewScriptObject<RigidBodyOnGroundCondition>(), 0x57, g_RigidBodyOnGroundConditionVTable);
    case 88:
        return MakeCondition(NewScriptObject<MeToAgentRef1SqrDistCondition>(), 0x58, g_MeToAgentRef1SqrDistConditionVTable);
    case 89:
        return MakeCondition(NewScriptObject<MeToStoredPositionSqrDistCondition>(), 0x59, g_MeToStoredPositionSqrDistConditionVTable);
    case 90:
        return MakeCondition(NewScriptObject<GroundBelowFocusPositionCondition>(), 0x5A, g_GroundBelowFocusPositionConditionVTable);
    case 91:
        return MakeCondition(NewScriptObject<InstanceCounterValueCondition>(), 0x5B, g_InstanceCounterValueConditionVTable);
    case 92:
        return MakeCondition(NewScriptObject<InstanceCounterEqualsThresholdCondition>(), 0x5C, g_InstanceCounterEqualsThresholdConditionVTable);
    case 93:
        return MakeCondition(NewScriptObject<HeadAtLimitCondition>(), 0x5D, g_HeadAtLimitConditionVTable);
    case 94:
        return MakeCondition(NewScriptObject<HeadYawBelowLimitCondition>(), 0x5E, g_HeadYawBelowLimitConditionVTable);
    case 95:
        return MakeCondition(NewScriptObject<HeadYawAboveLimitCondition>(), 0x5F, g_HeadYawAboveLimitConditionVTable);
    case 96:
        return MakeCondition(NewScriptObject<HeadPitchBelowLimitCondition>(), 0x60, g_HeadPitchBelowLimitConditionVTable);
    case 97:
        return MakeCondition(NewScriptObject<HeadPitchAboveLimitCondition>(), 0x61, g_HeadPitchAboveLimitConditionVTable);
    case 102:
        return MakeCondition(NewScriptObject<RouteStepUncheckedCondition>(), 0x66, g_RouteStepUncheckedConditionVTable);
    case 103:
        return MakeCondition(NewScriptObject<RouteStepCondition>(), 0x67, g_RouteStepConditionVTable);
    case 104:
        return MakeCondition(NewScriptObject<MeToEdgeStartNodeSqrDistCondition>(), 0x68, g_MeToEdgeStartNodeSqrDistConditionVTable);
    case 105:
        return MakeCondition(NewScriptObject<RigidBodyHasMotionCondition>(), 0x69, g_RigidBodyHasMotionConditionVTable);
    case 106:
        return MakeCondition(NewScriptObject<RigidBodyCollidesCondition>(), 0x6A, g_RigidBodyCollidesConditionVTable);
    case 107:
        return MakeCondition(NewScriptObject<MeToEdgeEndNodeSqrDistCondition>(), 0x6B, g_MeToEdgeEndNodeSqrDistConditionVTable);
    case 108:
        return MakeCondition(NewScriptObject<RigidBodyRidesInstanceCondition>(), 0x6C, g_RigidBodyRidesInstanceConditionVTable);
    case 109:
        return MakeCondition(NewScriptObject<OnLastLinkedObjectCondition>(), 0x6D, g_OnLastLinkedObjectConditionVTable);
    case 110:
        return MakeCondition(NewScriptObject<HeightAboveStartCondition>(), 0x6E, g_HeightAboveStartConditionVTable);
    case 111:
        return MakeCondition(NewScriptObject<Sense0LevelCondition>(), 0x6F, g_Sense0LevelConditionVTable);
    case 112:
        return MakeCondition(NewScriptObject<Sense2LevelCondition>(), 0x70, g_Sense2LevelConditionVTable);
    case 113:
        return MakeCondition(NewScriptObject<Sense1LevelCondition>(), 0x71, g_Sense1LevelConditionVTable);
    case 114:
        return MakeCondition(NewScriptObject<PresenceCondition>(), 0x72, g_PresenceConditionVTable);
    case 115:
        return MakeCondition(NewScriptObject<AlwaysZeroCondition>(), 0x73, g_AlwaysZeroConditionVTable);
    case 116:
        return MakeCondition(NewScriptObject<RigidBodyAgainstWallCondition>(), 0x74, g_RigidBodyAgainstWallConditionVTable);
    case 117:
        return MakeCondition(NewScriptObject<GotChildMessageOnceEqualsCondition>(), 0x75, g_GotChildMessageOnceEqualsConditionVTable);
    case 118:
        return MakeCondition(NewScriptObject<HasXLinksCondition>(), 0x76, g_HasXLinksConditionVTable);
    case 119:
        return MakeCondition(NewScriptObject<PositionXCondition>(), 0x77, g_PositionXConditionVTable);
    case 120:
        return MakeCondition(NewScriptObject<PositionYCondition>(), 0x78, g_PositionYConditionVTable);
    case 121:
        return MakeCondition(NewScriptObject<PositionZCondition>(), 0x79, g_PositionZConditionVTable);
    case 122:
        return MakeCondition(NewScriptObject<AgentRef1IsBusyCondition>(), 0x7A, g_AgentRef1IsBusyConditionVTable);
    case 123:
        return MakeCondition(NewScriptObject<MeToAgentRef1SqrDistCondition>(), 0x58, g_MeToAgentRef1SqrDistConditionVTable);
    case 124:
        return MakeCondition(NewScriptObject<IsAttachedCondition>(), 0x7C, g_IsAttachedConditionVTable);
    case 125:
        return MakeCondition(NewScriptObject<TouchedMessageSurfaceCondition>(), 0x7D, g_TouchedMessageSurfaceConditionVTable);
    case 126:
        return MakeCondition(NewScriptObject<FocusForwardDotCondition>(), 0x7E, g_FocusForwardDotConditionVTable);
    case 127:
        return MakeCondition(NewScriptObject<FocusObjectProp0EqualsCondition>(), 0x7F, g_FocusObjectProp0EqualsConditionVTable);
    case 128:
    {
        auto* condition = MakeCondition(NewScriptObject<TargetToSideCondition>(), 0x80, g_TargetToSideConditionVTable);
        condition->targetKind = TargetToSideCondition::TargetFocus;
        condition->targetSide = TargetToSideCondition::SideRight;
        return condition;
    }
    case 129:
    {
        auto* condition = MakeCondition(NewScriptObject<TargetToSideCondition>(), 0x81, g_TargetToSideConditionVTable);
        condition->targetKind = TargetToSideCondition::TargetFocus;
        condition->targetSide = TargetToSideCondition::SideLeft;
        return condition;
    }
    case 130:
    {
        auto* condition = MakeCondition(NewScriptObject<TargetToSideCondition>(), 0x82, g_TargetToSideConditionVTable);
        condition->targetKind = TargetToSideCondition::TargetAgentRef1;
        condition->targetSide = TargetToSideCondition::SideRight;
        return condition;
    }
    case 131:
    {
        auto* condition = MakeCondition(NewScriptObject<TargetToSideCondition>(), 0x83, g_TargetToSideConditionVTable);
        condition->targetKind = TargetToSideCondition::TargetAgentRef1;
        condition->targetSide = TargetToSideCondition::SideLeft;
        return condition;
    }
    case 132:
        return MakeCondition(NewScriptObject<KeyPathOnLastKeyCondition>(), 0x84, g_KeyPathOnLastKeyConditionVTable);
    case 133:
        return MakeCondition(NewScriptObject<HasInstanceIdCondition>(), 0x85, g_HasInstanceIdConditionVTable);
    case 134:
        return MakeCondition(NewScriptObject<KeyPathProgressCondition>(), 0x86, g_KeyPathProgressConditionVTable);
    case 135:
        return MakeCondition(NewScriptObject<KeyPathNumPathsCondition>(), 0x87, g_KeyPathNumPathsConditionVTable);
    case 136:
        return MakeCondition(NewScriptObject<VisibleFromFocusCondition>(), 0x88, g_VisibleFromFocusConditionVTable);
    case 137:
        return MakeCondition(NewScriptObject<KeyPathNumKeysCondition>(), 0x89, g_KeyPathNumKeysConditionVTable);
    case 138:
        return MakeCondition(NewScriptObject<FocusToAgentRef1DistanceSquaredCondition>(), 0x8A, g_FocusToAgentRef1DistanceSquaredConditionVTable);
    case 139:
        return MakeCondition(NewScriptObject<FocusDistanceFromStartSquaredCondition>(), 0x8B, g_FocusDistanceFromStartSquaredConditionVTable);
    case 140:
        return MakeCondition(NewScriptObject<HeightAboveAgentRef1Condition>(), 0x8C, g_HeightAboveAgentRef1ConditionVTable);
    case 141:
        return MakeCondition(NewScriptObject<FocusAlongXAxisSqrDistCondition>(), 0x8D, g_FocusAlongXAxisSqrDistConditionVTable);
    case 142:
        return MakeCondition(NewScriptObject<FocusAlongYAxisSqrDistCondition>(), 0x8E, g_FocusAlongYAxisSqrDistConditionVTable);
    case 143:
        return MakeCondition(NewScriptObject<FocusAlongZAxisSqrDistCondition>(), 0x8F, g_FocusAlongZAxisSqrDistConditionVTable);
    case 144:
        return MakeCondition(NewScriptObject<AgentRef1AlongXAxisSqrDistCondition>(), 0x90, g_AgentRef1AlongXAxisSqrDistConditionVTable);
    case 145:
        return MakeCondition(NewScriptObject<AgentRef1AlongYAxisSqrDistCondition>(), 0x91, g_AgentRef1AlongYAxisSqrDistConditionVTable);
    case 146:
        return MakeCondition(NewScriptObject<AgentRef1AlongZAxisSqrDistCondition>(), 0x92, g_AgentRef1AlongZAxisSqrDistConditionVTable);
    case 147:
        return MakeCondition(NewScriptObject<TouchingWorldCondition>(), 0x93, g_TouchingWorldConditionVTable);
    case 148:
        return MakeCondition(NewScriptObject<MeFacingAgentRef1Condition>(), 0x94, g_MeFacingAgentRef1ConditionVTable);
    case 149:
        return MakeCondition(NewScriptObject<VisibleFromAgentRef1Condition>(), 0x95, g_VisibleFromAgentRef1ConditionVTable);
    case 150:
        return MakeCondition(NewScriptObject<HeadCanSeeAgentRef1Condition>(), 0x96, g_HeadCanSeeAgentRef1ConditionVTable);
    case 151:
        return MakeCondition(NewScriptObject<FocusIsVisibleCondition>(), 0x97, g_FocusIsVisibleConditionVTable);
    case 152:
        return MakeCondition(NewScriptObject<IsInPlayerChunkCondition>(), 0x98, g_IsInPlayerChunkConditionVTable);
    case 153:
        return MakeCondition(NewScriptObject<HasScriptInSlotCondition>(), 0x99, g_HasScriptInSlotConditionVTable);
    case 154:
        return MakeCondition(NewScriptObject<CurrentKeyIsEvenCondition>(), 0x9A, g_CurrentKeyIsEvenConditionVTable);
    case 155:
        return MakeCondition(NewScriptObject<ExitPointToFocusSqrDistCondition>(), 0x9B, g_ExitPointToFocusSqrDistConditionVTable);
    case 156:
        return MakeCondition(NewScriptObject<CutsceneFinishedCondition>(), 0x9C, g_CutsceneFinishedConditionVTable);
    case 157:
        return MakeCondition(NewScriptObject<UpAxisYCondition>(), 0x9D, g_UpAxisYConditionVTable);
    case 158:
        return MakeCondition(NewScriptObject<IntProperty0Bit0ClearCondition>(), 0x9E, g_IntProperty0Bit0ClearConditionVTable);
    case 159:
        return MakeCondition(NewScriptObject<CutsceneMusicReadyCondition>(), 0x9F, g_CutsceneMusicReadyConditionVTable);
    case 160:
        return MakeCondition(NewScriptObject<NearestPointEdgeDistanceSquaredCondition>(), 0xA0, g_NearestPointEdgeDistanceSquaredConditionVTable);
    case 161:
        return MakeCondition(NewScriptObject<FocusIsAgentRef1Condition>(), 0xA1, g_FocusIsAgentRef1ConditionVTable);
    case 162:
        return MakeCondition(NewScriptObject<HasPhysicsBodyCondition>(), 0xA2, g_HasPhysicsBodyConditionVTable);
    case 163:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusInstanceCounterEqualsThresholdCondition>(), 0xA3, g_FocusInstanceCounterEqualsThresholdConditionVTable);
        condition->counter = 0;
        return condition;
    }
    case 164:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusInstanceCounterEqualsThresholdCondition>(), 0xA4, g_FocusInstanceCounterEqualsThresholdConditionVTable);
        condition->counter = 1;
        return condition;
    }
    case 165:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusInstanceCounterEqualsThresholdCondition>(), 0xA5, g_FocusInstanceCounterEqualsThresholdConditionVTable);
        condition->counter = 2;
        return condition;
    }
    case 166:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusInstanceCounterEqualsThresholdCondition>(), 0xA6, g_FocusInstanceCounterEqualsThresholdConditionVTable);
        condition->counter = 3;
        return condition;
    }
    case 167:
        return MakeCondition(NewScriptObject<SplineDistanceToAgentRef1Condition>(), 0xA7, g_SplineDistanceToAgentRef1ConditionVTable);
    case 168:
        return MakeCondition(NewScriptObject<GroundBelowPointAheadCondition>(), 0xA8, g_GroundBelowPointAheadConditionVTable);
    case 169:
        return MakeCondition(NewScriptObject<CountedInstancesCondition>(), 0xA9, g_CountedInstancesConditionVTable);
    case 170:
        return MakeCondition(NewScriptObject<CountedValueCondition>(), 0xAA, g_CountedValueConditionVTable);
    case 171:
        return MakeCondition(NewScriptObject<RankAboveGlobalRankCondition>(), 0xAB, g_RankAboveGlobalRankConditionVTable);
    case 172:
        return MakeCondition(NewScriptObject<TimeSinceMarkCondition>(), 0xAC, g_TimeSinceMarkConditionVTable);
    case 173:
        return MakeCondition(NewScriptObject<NoOp173Condition>(), 0xAD, g_NoOp173ConditionVTable);
    case 174:
        return MakeCondition(NewScriptObject<AlwaysCondition>(), 0xAE, g_AlwaysConditionVTable);
    case 175:
        return MakeCondition(NewScriptObject<NeverCondition>(), 0xAF, g_NeverConditionVTable);
    case 176:
        return MakeCondition(NewScriptObject<LinkedChunksQueuedCondition>(), 0xB0, g_LinkedChunksQueuedConditionVTable);
    case 177:
        return MakeCondition(NewScriptObject<FocusInSameChunkCondition>(), 0xB1, g_FocusInSameChunkConditionVTable);
    case 512:
        return MakeCondition(NewScriptObject<PlayerHitPointsCondition>(), 0x200, g_PlayerHitPointsConditionVTable);
    case 514:
        return MakeCondition(NewScriptObject<PlayerIsCrouchingCondition>(), 0x202, g_PlayerIsCrouchingConditionVTable);
    case 515:
        return MakeCondition(NewScriptObject<PlayerIsGroundedCondition>(), 0x203, g_PlayerIsGroundedConditionVTable);
    case 517:
        return MakeCondition(NewScriptObject<MeToPlayerSqrDistCondition>(), 0x205, g_MeToPlayerSqrDistConditionVTable);
    case 518:
        return MakeCondition(NewScriptObject<ShadowActiveCondition>(), 0x206, g_ShadowActiveConditionVTable);
    case 519:
        return MakeCondition(NewScriptObject<CrateHasWumpaCondition>(), 0x207, g_CrateHasWumpaConditionVTable);
    case 520:
        return MakeCondition(NewScriptObject<AgentWasTouchedCondition>(), 0x208, g_AgentWasTouchedConditionVTable);
    case 521:
        return MakeCondition(NewScriptObject<AgentWasSpunCondition>(), 0x209, g_AgentWasSpunConditionVTable);
    case 522:
        return MakeCondition(NewScriptObject<AgentWasKneeDroppedCondition>(), 0x20A, g_AgentWasKneeDroppedConditionVTable);
    case 523:
        return MakeCondition(NewScriptObject<AgentWasSlidCondition>(), 0x20B, g_AgentWasSlidConditionVTable);
    case 524:
        return MakeCondition(NewScriptObject<AgentHitPointsCondition>(), 0x20C, g_AgentHitPointsConditionVTable);
    case 525:
        return MakeCondition(NewScriptObject<CanMoveForwardsCondition>(), 0x20D, g_CanMoveForwardsConditionVTable);
    case 526:
        return MakeCondition(NewScriptObject<BlockedBehindCondition>(), 0x20E, g_BlockedBehindConditionVTable);
    case 527:
        return MakeCondition(NewScriptObject<BlockedLeftCondition>(), 0x20F, g_BlockedLeftConditionVTable);
    case 528:
        return MakeCondition(NewScriptObject<BlockedRightCondition>(), 0x210, g_BlockedRightConditionVTable);
    case 529:
        return MakeCondition(NewScriptObject<WumpaFruitCountCondition>(), 0x211, g_WumpaFruitCountConditionVTable);
    case 530:
        return MakeCondition(NewScriptObject<CanFallCondition>(), 0x212, g_CanFallConditionVTable);
    case 531:
        return MakeCondition(NewScriptObject<NoOpWillHitLowWallCondition>(), 0x213, g_NoOpWillHitLowWallConditionVTable);
    case 532:
        return MakeCondition(NewScriptObject<AgentWasHitByTiedPairCondition>(), 0x214, g_AgentWasHitByTiedPairConditionVTable);
    case 533:
        return MakeCondition(NewScriptObject<AgentWasHitByThrownCharacterCondition>(), 0x215, g_AgentWasHitByThrownCharacterConditionVTable);
    case 534:
        return MakeCondition(NewScriptObject<AgentWasAttackedCondition>(), 0x216, g_AgentWasAttackedConditionVTable);
    case 535:
        return MakeCondition(NewScriptObject<AgentWasJumpedOnCondition>(), 0x217, g_AgentWasJumpedOnConditionVTable);
    case 536:
        return MakeCondition(NewScriptObject<AgentWasWalkedIntoCondition>(), 0x218, g_AgentWasWalkedIntoConditionVTable);
    case 537:
        return MakeCondition(NewScriptObject<AgentWasHeadbuttedCondition>(), 0x219, g_AgentWasHeadbuttedConditionVTable);
    case 538:
        return MakeCondition(NewScriptObject<PlayerToMyFocusSqrDistCondition>(), 0x21A, g_PlayerToMyFocusSqrDistConditionVTable);
    case 539:
        return MakeCondition(NewScriptObject<PayGateNumberCondition>(), 0x21B, g_PayGateNumberConditionVTable);
    case 540:
        return MakeCondition(NewScriptObject<MeFacingPlayerCondition>(), 0x21C, g_MeFacingPlayerConditionVTable);
    case 541:
        return MakeCondition(NewScriptObject<PlayerFacingMeCondition>(), 0x21D, g_PlayerFacingMeConditionVTable);
    case 542:
        return MakeCondition(NewScriptObject<ClearLineOfSightToPlayerCondition>(), 0x21E, g_ClearLineOfSightToPlayerConditionVTable);
    case 543:
        return MakeCondition(NewScriptObject<CanSeePlayerCondition>(), 0x21F, g_CanSeePlayerConditionVTable);
    case 544:
        return MakeCondition(NewScriptObject<PlayerCanSeeMeCondition>(), 0x220, g_PlayerCanSeeMeConditionVTable);
    case 545:
        return MakeCondition(NewScriptObject<PlayerGunShotChargeCondition>(), 0x221, g_PlayerGunShotChargeConditionVTable);
    case 546:
        return MakeCondition(NewScriptObject<NodeIsAirborneCondition>(), 0x222, g_NodeIsAirborneConditionVTable);
    case 547:
        return MakeCondition(NewScriptObject<EdgeNeedsJumpCondition>(), 0x223, g_EdgeNeedsJumpConditionVTable);
    case 548:
        return MakeCondition(NewScriptObject<EdgeNeedsFlyingCondition>(), 0x224, g_EdgeNeedsFlyingConditionVTable);
    case 550:
        return MakeCondition(NewScriptObject<EdgeNeedsLongJumpCondition>(), 0x226, g_EdgeNeedsLongJumpConditionVTable);
    case 551:
        return MakeCondition(NewScriptObject<EdgeNeedsHighJumpCondition>(), 0x227, g_EdgeNeedsHighJumpConditionVTable);
    case 552:
        return MakeCondition(NewScriptObject<HeightAbovePlayerCondition>(), 0x228, g_HeightAbovePlayerConditionVTable);
    case 553:
        return MakeCondition(NewScriptObject<HeadLookingAtPlayerCondition>(), 0x229, g_HeadLookingAtPlayerConditionVTable);
    case 554:
        return MakeCondition(NewScriptObject<HeadCanSeePlayerCondition>(), 0x22A, g_HeadCanSeePlayerConditionVTable);
    case 555:
        return MakeCondition(NewScriptObject<PlayerHeadLookingAtMeCondition>(), 0x22B, g_PlayerHeadLookingAtMeConditionVTable);
    case 556:
        return MakeCondition(NewScriptObject<PlayerHeadCanSeeMeCondition>(), 0x22C, g_PlayerHeadCanSeeMeConditionVTable);
    case 557:
        return MakeCondition(NewScriptObject<PlayerIsMovingCondition>(), 0x22D, g_PlayerIsMovingConditionVTable);
    case 558:
        return MakeCondition(NewScriptObject<PlayerIsWalkingCondition>(), 0x22E, g_PlayerIsWalkingConditionVTable);
    case 559:
        return MakeCondition(NewScriptObject<PlayerIsRunningCondition>(), 0x22F, g_PlayerIsRunningConditionVTable);
    case 560:
        return MakeCondition(NewScriptObject<PlayerIsWalkingDuplicateCondition>(), 0x230, g_PlayerIsWalkingDuplicateConditionVTable);
    case 561:
        return MakeCondition(NewScriptObject<PlayerIsFallingCondition>(), 0x231, g_PlayerIsFallingConditionVTable);
    case 562:
        return MakeCondition(NewScriptObject<PlayerIsCoOpLinkedCondition>(), 0x232, g_PlayerIsCoOpLinkedConditionVTable);
    case 563:
        return MakeCondition(NewScriptObject<PlayerHoldingMultiToolCondition>(), 0x233, g_PlayerHoldingMultiToolConditionVTable);
    case 564:
        return MakeCondition(NewScriptObject<PlayerIsSlammingTiedCondition>(), 0x234, g_PlayerIsSlammingTiedConditionVTable);
    case 565:
        return MakeCondition(NewScriptObject<PlayerIsSpinningCondition>(), 0x235, g_PlayerIsSpinningConditionVTable);
    case 566:
        return MakeCondition(NewScriptObject<PlayerIsSlidingCondition>(), 0x236, g_PlayerIsSlidingConditionVTable);
    case 567:
        return MakeCondition(NewScriptObject<PlayerIsAirborneCondition>(), 0x237, g_PlayerIsAirborneConditionVTable);
    case 568:
        return MakeCondition(NewScriptObject<HeadCanSeeNearPlayerCondition>(), 0x238, g_HeadCanSeeNearPlayerConditionVTable);
    case 569:
        return MakeCondition(NewScriptObject<CanDamageCharacterCondition>(), 0x239, g_CanDamageCharacterConditionVTable);
    case 570:
        return MakeCondition(NewScriptObject<NoOp570Condition>(), 0x23A, g_NoOp570ConditionVTable);
    case 571:
        return MakeCondition(NewScriptObject<NoOp571Condition>(), 0x23B, g_NoOp571ConditionVTable);
    case 572:
        return MakeCondition(NewScriptObject<NoOpCutsceneSkippedCondition>(), 0x23C, g_NoOpCutsceneSkippedConditionVTable);
    case 573:
        return MakeCondition(NewScriptObject<IsCirclePressedCondition>(), 0x23D, g_IsCirclePressedConditionVTable);
    case 574:
        return MakeCondition(NewScriptObject<IsSquarePressedCondition>(), 0x23E, g_IsSquarePressedConditionVTable);
    case 575:
        return MakeCondition(NewScriptObject<IsTrianglePressedCondition>(), 0x23F, g_IsTrianglePressedConditionVTable);
    case 576:
        return MakeCondition(NewScriptObject<IsR1PressedCondition>(), 0x240, g_IsR1PressedConditionVTable);
    case 577:
        return MakeCondition(NewScriptObject<RidesVehicleCondition>(), 0x241, g_RidesVehicleConditionVTable);
    case 578:
        return MakeCondition(NewScriptObject<NoGroundAheadCondition>(), 0x242, g_NoGroundAheadConditionVTable);
    case 579:
        return MakeCondition(NewScriptObject<PlayerJustShotCondition>(), 0x243, g_PlayerJustShotConditionVTable);
    case 580:
        return MakeCondition(NewScriptObject<IsDownBlastCondition>(), 0x244, g_IsDownBlastConditionVTable);
    case 581:
        return MakeCondition(NewScriptObject<ShotAtMeCondition>(), 0x245, g_ShotAtMeConditionVTable);
    case 582:
        return MakeCondition(NewScriptObject<PlayerVisibleCondition>(), 0x246, g_PlayerVisibleConditionVTable);
    case 583:
        return MakeCondition(NewScriptObject<EdgeStartNodeBlockedCondition>(), 0x247, g_EdgeStartNodeBlockedConditionVTable);
    case 584:
        return MakeCondition(NewScriptObject<PlayerVisibleFromLeftCondition>(), 0x248, g_PlayerVisibleFromLeftConditionVTable);
    case 585:
        return MakeCondition(NewScriptObject<PlayerVisibleFromRightCondition>(), 0x249, g_PlayerVisibleFromRightConditionVTable);
    case 586:
        return MakeCondition(NewScriptObject<IsTiedSecondCondition>(), 0x24A, g_IsTiedSecondConditionVTable);
    case 587:
        return MakeCondition(NewScriptObject<IsPlayerCondition>(), 0x24B, g_IsPlayerConditionVTable);
    case 588:
        return MakeCondition(NewScriptObject<HitByKickCondition>(), 0x24C, g_HitByKickConditionVTable);
    case 589:
        return MakeCondition(NewScriptObject<HitBySpinCondition>(), 0x24D, g_HitBySpinConditionVTable);
    case 590:
        return MakeCondition(NewScriptObject<HitByKind18Condition>(), 0x24E, g_HitByKind18ConditionVTable);
    case 591:
        return MakeCondition(NewScriptObject<HitByProjectileCondition>(), 0x24F, g_HitByProjectileConditionVTable);
    case 592:
        return MakeCondition(NewScriptObject<HitByKneeDropCondition>(), 0x250, g_HitByKneeDropConditionVTable);
    case 593:
        return MakeCondition(NewScriptObject<CharacterHasVehicleCondition>(), 0x251, g_CharacterHasVehicleConditionVTable);
    case 594:
        return MakeCondition(NewScriptObject<IsVehicleRollerbrawlCondition>(), 0x252, g_IsVehicleRollerbrawlConditionVTable);
    case 595:
        return MakeCondition(NewScriptObject<IsVehicleKind2Condition>(), 0x253, g_IsVehicleKind2ConditionVTable);
    case 596:
        return MakeCondition(NewScriptObject<IsVehicleHumiliskateCondition>(), 0x254, g_IsVehicleHumiliskateConditionVTable);
    case 597:
        return MakeCondition(NewScriptObject<IsVehicleKind4Condition>(), 0x255, g_IsVehicleKind4ConditionVTable);
    case 598:
        return MakeCondition(NewScriptObject<IsVehicleHoverboardCondition>(), 0x256, g_IsVehicleHoverboardConditionVTable);
    case 599:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag5Condition>(), 0x257, g_EdgeStartNodeFlag5ConditionVTable);
    case 600:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag4Condition>(), 0x258, g_EdgeStartNodeFlag4ConditionVTable);
    case 601:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag6Condition>(), 0x259, g_EdgeStartNodeFlag6ConditionVTable);
    case 602:
        return MakeCondition(NewScriptObject<EdgeFlag8Condition>(), 0x25A, g_EdgeFlag8ConditionVTable);
    case 603:
        return MakeCondition(NewScriptObject<EdgeEndNodeFlag5Condition>(), 0x25B, g_EdgeEndNodeFlag5ConditionVTable);
    case 604:
        return MakeCondition(NewScriptObject<EdgeEndNodeFlag4Condition>(), 0x25C, g_EdgeEndNodeFlag4ConditionVTable);
    case 605:
        return MakeCondition(NewScriptObject<EdgeEndNodeFlag6Condition>(), 0x25D, g_EdgeEndNodeFlag6ConditionVTable);
    case 606:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag5DuplicateCondition>(), 0x25E, g_EdgeStartNodeFlag5DuplicateConditionVTable);
    case 607:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag4DuplicateCondition>(), 0x25F, g_EdgeStartNodeFlag4DuplicateConditionVTable);
    case 608:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag6DuplicateCondition>(), 0x260, g_EdgeStartNodeFlag6DuplicateConditionVTable);
    case 609:
        return MakeCondition(NewScriptObject<SpeedAbovePlayerCondition>(), 0x261, g_SpeedAbovePlayerConditionVTable);
    case 610:
        return MakeCondition(NewScriptObject<HitByElectricCondition>(), 0x262, g_HitByElectricConditionVTable);
    case 611:
        return MakeCondition(NewScriptObject<HitByExplosionCondition>(), 0x263, g_HitByExplosionConditionVTable);
    case 612:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag2Condition>(), 0x264, g_EdgeStartNodeFlag2ConditionVTable);
    case 613:
        return MakeCondition(NewScriptObject<EdgeEndNodeFlag2Condition>(), 0x265, g_EdgeEndNodeFlag2ConditionVTable);
    case 614:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlag2DuplicateCondition>(), 0x266, g_EdgeStartNodeFlag2DuplicateConditionVTable);
    case 615:
        return MakeCondition(NewScriptObject<EdgeStartNodeFlagsClearCondition>(), 0x267, g_EdgeStartNodeFlagsClearConditionVTable);
    case 616:
        return MakeCondition(NewScriptObject<FocusPositionToPlayerDistanceSquaredCondition>(), 0x268, g_FocusPositionToPlayerDistanceSquaredConditionVTable);
    case 617:
        return MakeCondition(NewScriptObject<IsPushingObjectCondition>(), 0x269, g_IsPushingObjectConditionVTable);
    case 618:
        return MakeCondition(NewScriptObject<PlayerSideOffsetCondition>(), 0x26A, g_PlayerSideOffsetConditionVTable);
    case 619:
        return MakeCondition(NewScriptObject<PlayerNearerAnotherKeyCondition>(), 0x26B, g_PlayerNearerAnotherKeyConditionVTable);
    case 620:
        return MakeCondition(NewScriptObject<PlayerHumiliskateCrouchedCondition>(), 0x26C, g_PlayerHumiliskateCrouchedConditionVTable);
    case 621:
        return MakeCondition(NewScriptObject<PlayerOutsideHomeChunkCondition>(), 0x26D, g_PlayerOutsideHomeChunkConditionVTable);
    case 622:
        return MakeCondition(NewScriptObject<PlayerScriptFlagClearCondition>(), 0x26E, g_PlayerScriptFlagClearConditionVTable);
    case 623:
        return MakeCondition(NewScriptObject<HasActorWeightCondition>(), 0x26F, g_HasActorWeightConditionVTable);
    case 624:
        return MakeCondition(NewScriptObject<GameIsPlayingCondition>(), 0x270, g_GameIsPlayingConditionVTable);
    case 625:
        return MakeCondition(NewScriptObject<HitByWaterCondition>(), 0x271, g_HitByWaterConditionVTable);
    case 626:
        return MakeCondition(NewScriptObject<HitByFallThroughCondition>(), 0x272, g_HitByFallThroughConditionVTable);
    case 627:
        return MakeCondition(NewScriptObject<PlayerRidesRollerbrawlCondition>(), 0x273, g_PlayerRidesRollerbrawlConditionVTable);
    case 628:
        return MakeCondition(NewScriptObject<CortexDeadPlayerAliveCondition>(), 0x274, g_CortexDeadPlayerAliveConditionVTable);
    case 629:
        return MakeCondition(NewScriptObject<ScriptGlobalFlagCondition>(), 0x275, g_ScriptGlobalFlagConditionVTable);
    case 630:
        return MakeCondition(NewScriptObject<InWaterCondition>(), 0x276, g_InWaterConditionVTable);
    case 631:
        return MakeCondition(NewScriptObject<TimedPlayCountCondition>(), 0x277, g_TimedPlayCountConditionVTable);
    case 632:
        return MakeCondition(NewScriptObject<TimedPlayTimeLeftCondition>(), 0x278, g_TimedPlayTimeLeftConditionVTable);
    case 633:
        return MakeCondition(NewScriptObject<PlayerGunSecondCountCondition>(), 0x279, g_PlayerGunSecondCountConditionVTable);
    case 634:
        return MakeCondition(NewScriptObject<PlayerAmmoCondition>(), 0x27A, g_PlayerAmmoConditionVTable);
    case 635:
        return MakeCondition(NewScriptObject<CameraForwardDistanceCondition>(), 0x27B, g_CameraForwardDistanceConditionVTable);
    case 636:
        return MakeCondition(NewScriptObject<HitByHeavyCondition>(), 0x27C, g_HitByHeavyConditionVTable);
    case 637:
        return MakeCondition(NewScriptObject<PairingIs5Condition>(), 0x27D, g_PairingIs5ConditionVTable);
    case 638:
        return MakeCondition(NewScriptObject<HitByBurningCondition>(), 0x27E, g_HitByBurningConditionVTable);
    case 639:
        return MakeCondition(NewScriptObject<StoryAreaCondition>(), 0x27F, g_StoryAreaConditionVTable);
    case 640:
        return MakeCondition(NewScriptObject<PlayerVehicleHeightCondition>(), 0x280, g_PlayerVehicleHeightConditionVTable);
    case 641:
        return MakeCondition(NewScriptObject<IsMoviePlayingCondition>(), 0x281, g_IsMoviePlayingConditionVTable);
    case 642:
        return MakeCondition(NewScriptObject<PlayerIsDeadCondition>(), 0x282, g_PlayerIsDeadConditionVTable);
    case 643:
        return MakeCondition(NewScriptObject<PlayAreaIsCondition>(), 0x283, g_PlayAreaIsConditionVTable);
    case 644:
        return MakeCondition(NewScriptObject<TriggeredByOtherCharacterCondition>(), 0x284, g_TriggeredByOtherCharacterConditionVTable);
    default:
        return nullptr;
    }
}
