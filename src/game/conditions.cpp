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

void IsInExternalScriptCondition::Destroy(u32 destroyFlags)
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

void TouchingTerrainCondition::Destroy(u32 destroyFlags)
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

void HasInstancePositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasFocusPositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeFlag16Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeFlag17Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeFlag15Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeByte154FractionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsBodyFlag1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void DistanceToTargetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusPositionDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GroundBelowFocusPositionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectInstanceByteAtCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void InstanceSubtypeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadTrackingFlag24Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadTrackingFlag25Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadTrackingFlag26Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadTrackingFlag27Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadTrackingFlag28Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathKeyRawCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathKeyDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsCount8cCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsHasContactsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPreviousKeyDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsHasGroundCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CurrentLinkIndexCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeightAboveStartCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasPerception0Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasPerception2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasPerception1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CharacterAnalogCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AlwaysZeroCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsBodyFlag5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GotUserMessageOnceEqualsCondition::Destroy(u32 destroyFlags)
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

void AgentRef1SpawnFlagCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsAttachedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsImpactCondition::Destroy(u32 destroyFlags)
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

void AngleToFocusCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathOnLastKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ContextValue154SetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathProgressCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void KeyPathByte42Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusVisibleCondition::Destroy(u32 destroyFlags)
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

void AgentRef1HeightDifferenceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusOffXAxisDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusHorizontalDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusOffForwardAxisDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1OffXAxisDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1HorizontalDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1OffAxisDistanceSquaredCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PhysicsTouchingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1SideOffsetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1VisibleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AgentRef1InViewConeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusFlag10Condition::Destroy(u32 destroyFlags)
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

void FocusFromExitPointCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void VideoStateIs5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void UpVectorXCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IntProp0Bit0Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void VideoReadyCondition::Destroy(u32 destroyFlags)
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

void PhysicsHasCollisionNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void FocusObjectByte0EqualsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SplineDistanceToAgentRef1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObstacleAheadCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeValue174CountCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsFullInstanceNodeCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeByte8cMinusGlobalCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void TimeSinceMarkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AlwaysZero173Condition::Destroy(u32 destroyFlags)
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

void ChunksLoadedCondition::Destroy(u32 destroyFlags)
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

void AgentIsOnGroundCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CrateHasRedWumpaCondition::Destroy(u32 destroyFlags)
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

void CanMoveBackwardsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanStrafeLeftCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanStrafeRightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanJumpForwardsCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CanFallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void WillHitLowWallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void WillHitWallCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void WillRunOffCliffCondition::Destroy(u32 destroyFlags)
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

void WumpaNeededForPayGateCondition::Destroy(u32 destroyFlags)
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

void NodeTrafficCondition::Destroy(u32 destroyFlags)
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

void PlayerIsCrawlingCondition::Destroy(u32 destroyFlags)
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

void PlayerIsSlammingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsSpinningCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerIsJumpingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HeadCanSeePlayerUnblockedCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AmIHarmfulCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void AttachedContextFlag8Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void DUMMY_570Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void DUMMY_571Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CutsceneSkippedCondition::Destroy(u32 destroyFlags)
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

void CharacterVehiclePointerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CharacterFlag23Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsChargedShotCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsDownBlastCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GlobalInstanceOp581Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisibleCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag0Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisible2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVisible3Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CharacterFlag22Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsPlayerCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlag17Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByPunchCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByBodySlam2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitBySpinHitboxCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByBodySlamHitboxCondition::Destroy(u32 destroyFlags)
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

void VehicleTypeNot2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicleHumiliskateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void VehicleTypeNot4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsVehicle3Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag6Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PathSegmentFlag0Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPreviousPointFlag5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPreviousPointFlag4Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPreviousPointFlag6Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag5bCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag4bCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag6bCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVectorLengthDifferenceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HitByCortexBoltCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlag1Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPreviousPointFlag2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlag2bCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SubPathPointFlags56Condition::Destroy(u32 destroyFlags)
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

void PlayerNearCurrentKeyCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerSplineVehicleValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CharacterHasHomeChunkCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerFlag57ClearCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasActorWeightCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameFlags44Is12Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlag25Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlag2Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerVehicle1ValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void BothCharactersFlag14Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GlobalInt3098e8Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void NodeValue134SetCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameControllerField500HighCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameTimer57cCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SecondCharacterGunStateCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void HasAmmoCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void CameraForwardDistanceCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlag19Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameModeIs5Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void ObjectContextFlags3or22Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GlobalProgressionCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void SecondCharacterVehicleValueCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void IsMoviePlayingCondition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void PlayerFlag14Condition::Destroy(u32 destroyFlags)
{
    ScriptCondition::Destroy(destroyFlags);
}

void GameStateIsCondition::Destroy(u32 destroyFlags)
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
        return MakeCondition(NewScriptObject<IsInExternalScriptCondition>(), 0x6, g_IsInExternalScriptConditionVTable);
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
        return MakeCondition(NewScriptObject<TouchingTerrainCondition>(), 0x35, g_TouchingTerrainConditionVTable);
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
        return MakeCondition(NewScriptObject<HasInstancePositionCondition>(), 0x51, g_HasInstancePositionConditionVTable);
    case 82:
        return MakeCondition(NewScriptObject<HasFocusPositionCondition>(), 0x52, g_HasFocusPositionConditionVTable);
    case 83:
        return MakeCondition(NewScriptObject<NodeFlag16Condition>(), 0x53, g_NodeFlag16ConditionVTable);
    case 84:
        return MakeCondition(NewScriptObject<NodeFlag17Condition>(), 0x54, g_NodeFlag17ConditionVTable);
    case 85:
        return MakeCondition(NewScriptObject<NodeFlag15Condition>(), 0x55, g_NodeFlag15ConditionVTable);
    case 86:
        return MakeCondition(NewScriptObject<NodeByte154FractionCondition>(), 0x56, g_NodeByte154FractionConditionVTable);
    case 87:
        return MakeCondition(NewScriptObject<PhysicsBodyFlag1Condition>(), 0x57, g_PhysicsBodyFlag1ConditionVTable);
    case 88:
        return MakeCondition(NewScriptObject<DistanceToTargetCondition>(), 0x58, g_DistanceToTargetConditionVTable);
    case 89:
        return MakeCondition(NewScriptObject<FocusPositionDistanceSquaredCondition>(), 0x59, g_FocusPositionDistanceSquaredConditionVTable);
    case 90:
        return MakeCondition(NewScriptObject<GroundBelowFocusPositionCondition>(), 0x5A, g_GroundBelowFocusPositionConditionVTable);
    case 91:
        return MakeCondition(NewScriptObject<ObjectInstanceByteAtCondition>(), 0x5B, g_ObjectInstanceByteAtConditionVTable);
    case 92:
        return MakeCondition(NewScriptObject<InstanceSubtypeCondition>(), 0x5C, g_InstanceSubtypeConditionVTable);
    case 93:
        return MakeCondition(NewScriptObject<HeadTrackingFlag24Condition>(), 0x5D, g_HeadTrackingFlag24ConditionVTable);
    case 94:
        return MakeCondition(NewScriptObject<HeadTrackingFlag25Condition>(), 0x5E, g_HeadTrackingFlag25ConditionVTable);
    case 95:
        return MakeCondition(NewScriptObject<HeadTrackingFlag26Condition>(), 0x5F, g_HeadTrackingFlag26ConditionVTable);
    case 96:
        return MakeCondition(NewScriptObject<HeadTrackingFlag27Condition>(), 0x60, g_HeadTrackingFlag27ConditionVTable);
    case 97:
        return MakeCondition(NewScriptObject<HeadTrackingFlag28Condition>(), 0x61, g_HeadTrackingFlag28ConditionVTable);
    case 102:
        return MakeCondition(NewScriptObject<SubPathKeyRawCondition>(), 0x66, g_SubPathKeyRawConditionVTable);
    case 103:
        return MakeCondition(NewScriptObject<SubPathKeyCondition>(), 0x67, g_SubPathKeyConditionVTable);
    case 104:
        return MakeCondition(NewScriptObject<SubPathKeyDistanceSquaredCondition>(), 0x68, g_SubPathKeyDistanceSquaredConditionVTable);
    case 105:
        return MakeCondition(NewScriptObject<PhysicsCount8cCondition>(), 0x69, g_PhysicsCount8cConditionVTable);
    case 106:
        return MakeCondition(NewScriptObject<PhysicsHasContactsCondition>(), 0x6A, g_PhysicsHasContactsConditionVTable);
    case 107:
        return MakeCondition(NewScriptObject<SubPathPreviousKeyDistanceSquaredCondition>(), 0x6B, g_SubPathPreviousKeyDistanceSquaredConditionVTable);
    case 108:
        return MakeCondition(NewScriptObject<PhysicsHasGroundCondition>(), 0x6C, g_PhysicsHasGroundConditionVTable);
    case 109:
        return MakeCondition(NewScriptObject<CurrentLinkIndexCondition>(), 0x6D, g_CurrentLinkIndexConditionVTable);
    case 110:
        return MakeCondition(NewScriptObject<HeightAboveStartCondition>(), 0x6E, g_HeightAboveStartConditionVTable);
    case 111:
        return MakeCondition(NewScriptObject<HasPerception0Condition>(), 0x6F, g_HasPerception0ConditionVTable);
    case 112:
        return MakeCondition(NewScriptObject<HasPerception2Condition>(), 0x70, g_HasPerception2ConditionVTable);
    case 113:
        return MakeCondition(NewScriptObject<HasPerception1Condition>(), 0x71, g_HasPerception1ConditionVTable);
    case 114:
        return MakeCondition(NewScriptObject<CharacterAnalogCondition>(), 0x72, g_CharacterAnalogConditionVTable);
    case 115:
        return MakeCondition(NewScriptObject<AlwaysZeroCondition>(), 0x73, g_AlwaysZeroConditionVTable);
    case 116:
        return MakeCondition(NewScriptObject<PhysicsBodyFlag5Condition>(), 0x74, g_PhysicsBodyFlag5ConditionVTable);
    case 117:
        return MakeCondition(NewScriptObject<GotUserMessageOnceEqualsCondition>(), 0x75, g_GotUserMessageOnceEqualsConditionVTable);
    case 118:
        return MakeCondition(NewScriptObject<HasXLinksCondition>(), 0x76, g_HasXLinksConditionVTable);
    case 119:
        return MakeCondition(NewScriptObject<PositionXCondition>(), 0x77, g_PositionXConditionVTable);
    case 120:
        return MakeCondition(NewScriptObject<PositionYCondition>(), 0x78, g_PositionYConditionVTable);
    case 121:
        return MakeCondition(NewScriptObject<PositionZCondition>(), 0x79, g_PositionZConditionVTable);
    case 122:
        return MakeCondition(NewScriptObject<AgentRef1SpawnFlagCondition>(), 0x7A, g_AgentRef1SpawnFlagConditionVTable);
    case 123:
        return MakeCondition(NewScriptObject<DistanceToTargetCondition>(), 0x58, g_DistanceToTargetConditionVTable);
    case 124:
        return MakeCondition(NewScriptObject<IsAttachedCondition>(), 0x7C, g_IsAttachedConditionVTable);
    case 125:
        return MakeCondition(NewScriptObject<PhysicsImpactCondition>(), 0x7D, g_PhysicsImpactConditionVTable);
    case 126:
        return MakeCondition(NewScriptObject<FocusForwardDotCondition>(), 0x7E, g_FocusForwardDotConditionVTable);
    case 127:
        return MakeCondition(NewScriptObject<FocusObjectProp0EqualsCondition>(), 0x7F, g_FocusObjectProp0EqualsConditionVTable);
    case 128:
    {
        auto* condition = MakeCondition(NewScriptObject<AngleToFocusCondition>(), 0x80, g_AngleToFocusConditionVTable);
        condition->unknown14 = 0;
        condition->unknown18 = 1;
        return condition;
    }
    case 129:
    {
        auto* condition = MakeCondition(NewScriptObject<AngleToFocusCondition>(), 0x81, g_AngleToFocusConditionVTable);
        condition->unknown14 = 0;
        condition->unknown18 = 0;
        return condition;
    }
    case 130:
    {
        auto* condition = MakeCondition(NewScriptObject<AngleToFocusCondition>(), 0x82, g_AngleToFocusConditionVTable);
        condition->unknown14 = 1;
        condition->unknown18 = 1;
        return condition;
    }
    case 131:
    {
        auto* condition = MakeCondition(NewScriptObject<AngleToFocusCondition>(), 0x83, g_AngleToFocusConditionVTable);
        condition->unknown14 = 1;
        condition->unknown18 = 0;
        return condition;
    }
    case 132:
        return MakeCondition(NewScriptObject<KeyPathOnLastKeyCondition>(), 0x84, g_KeyPathOnLastKeyConditionVTable);
    case 133:
        return MakeCondition(NewScriptObject<ContextValue154SetCondition>(), 0x85, g_ContextValue154SetConditionVTable);
    case 134:
        return MakeCondition(NewScriptObject<KeyPathProgressCondition>(), 0x86, g_KeyPathProgressConditionVTable);
    case 135:
        return MakeCondition(NewScriptObject<KeyPathByte42Condition>(), 0x87, g_KeyPathByte42ConditionVTable);
    case 136:
        return MakeCondition(NewScriptObject<FocusVisibleCondition>(), 0x88, g_FocusVisibleConditionVTable);
    case 137:
        return MakeCondition(NewScriptObject<KeyPathNumKeysCondition>(), 0x89, g_KeyPathNumKeysConditionVTable);
    case 138:
        return MakeCondition(NewScriptObject<FocusToAgentRef1DistanceSquaredCondition>(), 0x8A, g_FocusToAgentRef1DistanceSquaredConditionVTable);
    case 139:
        return MakeCondition(NewScriptObject<FocusDistanceFromStartSquaredCondition>(), 0x8B, g_FocusDistanceFromStartSquaredConditionVTable);
    case 140:
        return MakeCondition(NewScriptObject<AgentRef1HeightDifferenceCondition>(), 0x8C, g_AgentRef1HeightDifferenceConditionVTable);
    case 141:
        return MakeCondition(NewScriptObject<FocusOffXAxisDistanceSquaredCondition>(), 0x8D, g_FocusOffXAxisDistanceSquaredConditionVTable);
    case 142:
        return MakeCondition(NewScriptObject<FocusHorizontalDistanceSquaredCondition>(), 0x8E, g_FocusHorizontalDistanceSquaredConditionVTable);
    case 143:
        return MakeCondition(NewScriptObject<FocusOffForwardAxisDistanceSquaredCondition>(), 0x8F, g_FocusOffForwardAxisDistanceSquaredConditionVTable);
    case 144:
        return MakeCondition(NewScriptObject<AgentRef1OffXAxisDistanceSquaredCondition>(), 0x90, g_AgentRef1OffXAxisDistanceSquaredConditionVTable);
    case 145:
        return MakeCondition(NewScriptObject<AgentRef1HorizontalDistanceSquaredCondition>(), 0x91, g_AgentRef1HorizontalDistanceSquaredConditionVTable);
    case 146:
        return MakeCondition(NewScriptObject<AgentRef1OffAxisDistanceSquaredCondition>(), 0x92, g_AgentRef1OffAxisDistanceSquaredConditionVTable);
    case 147:
        return MakeCondition(NewScriptObject<PhysicsTouchingCondition>(), 0x93, g_PhysicsTouchingConditionVTable);
    case 148:
        return MakeCondition(NewScriptObject<AgentRef1SideOffsetCondition>(), 0x94, g_AgentRef1SideOffsetConditionVTable);
    case 149:
        return MakeCondition(NewScriptObject<AgentRef1VisibleCondition>(), 0x95, g_AgentRef1VisibleConditionVTable);
    case 150:
        return MakeCondition(NewScriptObject<AgentRef1InViewConeCondition>(), 0x96, g_AgentRef1InViewConeConditionVTable);
    case 151:
        return MakeCondition(NewScriptObject<FocusFlag10Condition>(), 0x97, g_FocusFlag10ConditionVTable);
    case 152:
        return MakeCondition(NewScriptObject<IsInPlayerChunkCondition>(), 0x98, g_IsInPlayerChunkConditionVTable);
    case 153:
        return MakeCondition(NewScriptObject<HasScriptInSlotCondition>(), 0x99, g_HasScriptInSlotConditionVTable);
    case 154:
        return MakeCondition(NewScriptObject<CurrentKeyIsEvenCondition>(), 0x9A, g_CurrentKeyIsEvenConditionVTable);
    case 155:
        return MakeCondition(NewScriptObject<FocusFromExitPointCondition>(), 0x9B, g_FocusFromExitPointConditionVTable);
    case 156:
        return MakeCondition(NewScriptObject<VideoStateIs5Condition>(), 0x9C, g_VideoStateIs5ConditionVTable);
    case 157:
        return MakeCondition(NewScriptObject<UpVectorXCondition>(), 0x9D, g_UpVectorXConditionVTable);
    case 158:
        return MakeCondition(NewScriptObject<IntProp0Bit0Condition>(), 0x9E, g_IntProp0Bit0ConditionVTable);
    case 159:
        return MakeCondition(NewScriptObject<VideoReadyCondition>(), 0x9F, g_VideoReadyConditionVTable);
    case 160:
        return MakeCondition(NewScriptObject<NearestPointEdgeDistanceSquaredCondition>(), 0xA0, g_NearestPointEdgeDistanceSquaredConditionVTable);
    case 161:
        return MakeCondition(NewScriptObject<FocusIsAgentRef1Condition>(), 0xA1, g_FocusIsAgentRef1ConditionVTable);
    case 162:
        return MakeCondition(NewScriptObject<PhysicsHasCollisionNodeCondition>(), 0xA2, g_PhysicsHasCollisionNodeConditionVTable);
    case 163:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusObjectByte0EqualsCondition>(), 0xA3, g_FocusObjectByte0EqualsConditionVTable);
        condition->unknown14 = 0;
        return condition;
    }
    case 164:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusObjectByte0EqualsCondition>(), 0xA4, g_FocusObjectByte0EqualsConditionVTable);
        condition->unknown14 = 1;
        return condition;
    }
    case 165:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusObjectByte0EqualsCondition>(), 0xA5, g_FocusObjectByte0EqualsConditionVTable);
        condition->unknown14 = 2;
        return condition;
    }
    case 166:
    {
        auto* condition = MakeCondition(NewScriptObject<FocusObjectByte0EqualsCondition>(), 0xA6, g_FocusObjectByte0EqualsConditionVTable);
        condition->unknown14 = 3;
        return condition;
    }
    case 167:
        return MakeCondition(NewScriptObject<SplineDistanceToAgentRef1Condition>(), 0xA7, g_SplineDistanceToAgentRef1ConditionVTable);
    case 168:
        return MakeCondition(NewScriptObject<ObstacleAheadCondition>(), 0xA8, g_ObstacleAheadConditionVTable);
    case 169:
        return MakeCondition(NewScriptObject<NodeValue174CountCondition>(), 0xA9, g_NodeValue174CountConditionVTable);
    case 170:
        return MakeCondition(NewScriptObject<IsFullInstanceNodeCondition>(), 0xAA, g_IsFullInstanceNodeConditionVTable);
    case 171:
        return MakeCondition(NewScriptObject<NodeByte8cMinusGlobalCondition>(), 0xAB, g_NodeByte8cMinusGlobalConditionVTable);
    case 172:
        return MakeCondition(NewScriptObject<TimeSinceMarkCondition>(), 0xAC, g_TimeSinceMarkConditionVTable);
    case 173:
        return MakeCondition(NewScriptObject<AlwaysZero173Condition>(), 0xAD, g_AlwaysZero173ConditionVTable);
    case 174:
        return MakeCondition(NewScriptObject<AlwaysCondition>(), 0xAE, g_AlwaysConditionVTable);
    case 175:
        return MakeCondition(NewScriptObject<NeverCondition>(), 0xAF, g_NeverConditionVTable);
    case 176:
        return MakeCondition(NewScriptObject<ChunksLoadedCondition>(), 0xB0, g_ChunksLoadedConditionVTable);
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
        return MakeCondition(NewScriptObject<AgentIsOnGroundCondition>(), 0x206, g_AgentIsOnGroundConditionVTable);
    case 519:
        return MakeCondition(NewScriptObject<CrateHasRedWumpaCondition>(), 0x207, g_CrateHasRedWumpaConditionVTable);
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
        return MakeCondition(NewScriptObject<CanMoveBackwardsCondition>(), 0x20E, g_CanMoveBackwardsConditionVTable);
    case 527:
        return MakeCondition(NewScriptObject<CanStrafeLeftCondition>(), 0x20F, g_CanStrafeLeftConditionVTable);
    case 528:
        return MakeCondition(NewScriptObject<CanStrafeRightCondition>(), 0x210, g_CanStrafeRightConditionVTable);
    case 529:
        return MakeCondition(NewScriptObject<CanJumpForwardsCondition>(), 0x211, g_CanJumpForwardsConditionVTable);
    case 530:
        return MakeCondition(NewScriptObject<CanFallCondition>(), 0x212, g_CanFallConditionVTable);
    case 531:
        return MakeCondition(NewScriptObject<WillHitLowWallCondition>(), 0x213, g_WillHitLowWallConditionVTable);
    case 532:
        return MakeCondition(NewScriptObject<WillHitWallCondition>(), 0x214, g_WillHitWallConditionVTable);
    case 533:
        return MakeCondition(NewScriptObject<WillRunOffCliffCondition>(), 0x215, g_WillRunOffCliffConditionVTable);
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
        return MakeCondition(NewScriptObject<WumpaNeededForPayGateCondition>(), 0x21B, g_WumpaNeededForPayGateConditionVTable);
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
        return MakeCondition(NewScriptObject<NodeTrafficCondition>(), 0x221, g_NodeTrafficConditionVTable);
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
        return MakeCondition(NewScriptObject<PlayerIsCrawlingCondition>(), 0x230, g_PlayerIsCrawlingConditionVTable);
    case 561:
        return MakeCondition(NewScriptObject<PlayerIsFallingCondition>(), 0x231, g_PlayerIsFallingConditionVTable);
    case 562:
        return MakeCondition(NewScriptObject<PlayerIsCoOpLinkedCondition>(), 0x232, g_PlayerIsCoOpLinkedConditionVTable);
    case 563:
        return MakeCondition(NewScriptObject<PlayerHoldingMultiToolCondition>(), 0x233, g_PlayerHoldingMultiToolConditionVTable);
    case 564:
        return MakeCondition(NewScriptObject<PlayerIsSlammingCondition>(), 0x234, g_PlayerIsSlammingConditionVTable);
    case 565:
        return MakeCondition(NewScriptObject<PlayerIsSpinningCondition>(), 0x235, g_PlayerIsSpinningConditionVTable);
    case 566:
        return MakeCondition(NewScriptObject<PlayerIsJumpingCondition>(), 0x236, g_PlayerIsJumpingConditionVTable);
    case 567:
        return MakeCondition(NewScriptObject<HeadCanSeePlayerUnblockedCondition>(), 0x237, g_HeadCanSeePlayerUnblockedConditionVTable);
    case 568:
        return MakeCondition(NewScriptObject<AmIHarmfulCondition>(), 0x238, g_AmIHarmfulConditionVTable);
    case 569:
        return MakeCondition(NewScriptObject<AttachedContextFlag8Condition>(), 0x239, g_AttachedContextFlag8ConditionVTable);
    case 570:
        return MakeCondition(NewScriptObject<DUMMY_570Condition>(), 0x23A, g_DUMMY_570ConditionVTable);
    case 571:
        return MakeCondition(NewScriptObject<DUMMY_571Condition>(), 0x23B, g_DUMMY_571ConditionVTable);
    case 572:
        return MakeCondition(NewScriptObject<CutsceneSkippedCondition>(), 0x23C, g_CutsceneSkippedConditionVTable);
    case 573:
        return MakeCondition(NewScriptObject<IsCirclePressedCondition>(), 0x23D, g_IsCirclePressedConditionVTable);
    case 574:
        return MakeCondition(NewScriptObject<IsSquarePressedCondition>(), 0x23E, g_IsSquarePressedConditionVTable);
    case 575:
        return MakeCondition(NewScriptObject<IsTrianglePressedCondition>(), 0x23F, g_IsTrianglePressedConditionVTable);
    case 576:
        return MakeCondition(NewScriptObject<IsR1PressedCondition>(), 0x240, g_IsR1PressedConditionVTable);
    case 577:
        return MakeCondition(NewScriptObject<CharacterVehiclePointerCondition>(), 0x241, g_CharacterVehiclePointerConditionVTable);
    case 578:
        return MakeCondition(NewScriptObject<CharacterFlag23Condition>(), 0x242, g_CharacterFlag23ConditionVTable);
    case 579:
        return MakeCondition(NewScriptObject<IsChargedShotCondition>(), 0x243, g_IsChargedShotConditionVTable);
    case 580:
        return MakeCondition(NewScriptObject<IsDownBlastCondition>(), 0x244, g_IsDownBlastConditionVTable);
    case 581:
        return MakeCondition(NewScriptObject<GlobalInstanceOp581Condition>(), 0x245, g_GlobalInstanceOp581ConditionVTable);
    case 582:
        return MakeCondition(NewScriptObject<PlayerVisibleCondition>(), 0x246, g_PlayerVisibleConditionVTable);
    case 583:
        return MakeCondition(NewScriptObject<SubPathPointFlag0Condition>(), 0x247, g_SubPathPointFlag0ConditionVTable);
    case 584:
        return MakeCondition(NewScriptObject<PlayerVisible2Condition>(), 0x248, g_PlayerVisible2ConditionVTable);
    case 585:
        return MakeCondition(NewScriptObject<PlayerVisible3Condition>(), 0x249, g_PlayerVisible3ConditionVTable);
    case 586:
        return MakeCondition(NewScriptObject<CharacterFlag22Condition>(), 0x24A, g_CharacterFlag22ConditionVTable);
    case 587:
        return MakeCondition(NewScriptObject<IsPlayerCondition>(), 0x24B, g_IsPlayerConditionVTable);
    case 588:
        return MakeCondition(NewScriptObject<ObjectContextFlag17Condition>(), 0x24C, g_ObjectContextFlag17ConditionVTable);
    case 589:
        return MakeCondition(NewScriptObject<HitByPunchCondition>(), 0x24D, g_HitByPunchConditionVTable);
    case 590:
        return MakeCondition(NewScriptObject<HitByBodySlam2Condition>(), 0x24E, g_HitByBodySlam2ConditionVTable);
    case 591:
        return MakeCondition(NewScriptObject<HitBySpinHitboxCondition>(), 0x24F, g_HitBySpinHitboxConditionVTable);
    case 592:
        return MakeCondition(NewScriptObject<HitByBodySlamHitboxCondition>(), 0x250, g_HitByBodySlamHitboxConditionVTable);
    case 593:
        return MakeCondition(NewScriptObject<CharacterHasVehicleCondition>(), 0x251, g_CharacterHasVehicleConditionVTable);
    case 594:
        return MakeCondition(NewScriptObject<IsVehicleRollerbrawlCondition>(), 0x252, g_IsVehicleRollerbrawlConditionVTable);
    case 595:
        return MakeCondition(NewScriptObject<VehicleTypeNot2Condition>(), 0x253, g_VehicleTypeNot2ConditionVTable);
    case 596:
        return MakeCondition(NewScriptObject<IsVehicleHumiliskateCondition>(), 0x254, g_IsVehicleHumiliskateConditionVTable);
    case 597:
        return MakeCondition(NewScriptObject<VehicleTypeNot4Condition>(), 0x255, g_VehicleTypeNot4ConditionVTable);
    case 598:
        return MakeCondition(NewScriptObject<IsVehicle3Condition>(), 0x256, g_IsVehicle3ConditionVTable);
    case 599:
        return MakeCondition(NewScriptObject<SubPathPointFlag5Condition>(), 0x257, g_SubPathPointFlag5ConditionVTable);
    case 600:
        return MakeCondition(NewScriptObject<SubPathPointFlag4Condition>(), 0x258, g_SubPathPointFlag4ConditionVTable);
    case 601:
        return MakeCondition(NewScriptObject<SubPathPointFlag6Condition>(), 0x259, g_SubPathPointFlag6ConditionVTable);
    case 602:
        return MakeCondition(NewScriptObject<PathSegmentFlag0Condition>(), 0x25A, g_PathSegmentFlag0ConditionVTable);
    case 603:
        return MakeCondition(NewScriptObject<SubPathPreviousPointFlag5Condition>(), 0x25B, g_SubPathPreviousPointFlag5ConditionVTable);
    case 604:
        return MakeCondition(NewScriptObject<SubPathPreviousPointFlag4Condition>(), 0x25C, g_SubPathPreviousPointFlag4ConditionVTable);
    case 605:
        return MakeCondition(NewScriptObject<SubPathPreviousPointFlag6Condition>(), 0x25D, g_SubPathPreviousPointFlag6ConditionVTable);
    case 606:
        return MakeCondition(NewScriptObject<SubPathPointFlag5bCondition>(), 0x25E, g_SubPathPointFlag5bConditionVTable);
    case 607:
        return MakeCondition(NewScriptObject<SubPathPointFlag4bCondition>(), 0x25F, g_SubPathPointFlag4bConditionVTable);
    case 608:
        return MakeCondition(NewScriptObject<SubPathPointFlag6bCondition>(), 0x260, g_SubPathPointFlag6bConditionVTable);
    case 609:
        return MakeCondition(NewScriptObject<PlayerVectorLengthDifferenceCondition>(), 0x261, g_PlayerVectorLengthDifferenceConditionVTable);
    case 610:
        return MakeCondition(NewScriptObject<HitByCortexBoltCondition>(), 0x262, g_HitByCortexBoltConditionVTable);
    case 611:
        return MakeCondition(NewScriptObject<ObjectContextFlag1Condition>(), 0x263, g_ObjectContextFlag1ConditionVTable);
    case 612:
        return MakeCondition(NewScriptObject<SubPathPointFlag2Condition>(), 0x264, g_SubPathPointFlag2ConditionVTable);
    case 613:
        return MakeCondition(NewScriptObject<SubPathPreviousPointFlag2Condition>(), 0x265, g_SubPathPreviousPointFlag2ConditionVTable);
    case 614:
        return MakeCondition(NewScriptObject<SubPathPointFlag2bCondition>(), 0x266, g_SubPathPointFlag2bConditionVTable);
    case 615:
        return MakeCondition(NewScriptObject<SubPathPointFlags56Condition>(), 0x267, g_SubPathPointFlags56ConditionVTable);
    case 616:
        return MakeCondition(NewScriptObject<FocusPositionToPlayerDistanceSquaredCondition>(), 0x268, g_FocusPositionToPlayerDistanceSquaredConditionVTable);
    case 617:
        return MakeCondition(NewScriptObject<IsPushingObjectCondition>(), 0x269, g_IsPushingObjectConditionVTable);
    case 618:
        return MakeCondition(NewScriptObject<PlayerSideOffsetCondition>(), 0x26A, g_PlayerSideOffsetConditionVTable);
    case 619:
        return MakeCondition(NewScriptObject<PlayerNearCurrentKeyCondition>(), 0x26B, g_PlayerNearCurrentKeyConditionVTable);
    case 620:
        return MakeCondition(NewScriptObject<PlayerSplineVehicleValueCondition>(), 0x26C, g_PlayerSplineVehicleValueConditionVTable);
    case 621:
        return MakeCondition(NewScriptObject<CharacterHasHomeChunkCondition>(), 0x26D, g_CharacterHasHomeChunkConditionVTable);
    case 622:
        return MakeCondition(NewScriptObject<PlayerFlag57ClearCondition>(), 0x26E, g_PlayerFlag57ClearConditionVTable);
    case 623:
        return MakeCondition(NewScriptObject<HasActorWeightCondition>(), 0x26F, g_HasActorWeightConditionVTable);
    case 624:
        return MakeCondition(NewScriptObject<GameFlags44Is12Condition>(), 0x270, g_GameFlags44Is12ConditionVTable);
    case 625:
        return MakeCondition(NewScriptObject<ObjectContextFlag25Condition>(), 0x271, g_ObjectContextFlag25ConditionVTable);
    case 626:
        return MakeCondition(NewScriptObject<ObjectContextFlag2Condition>(), 0x272, g_ObjectContextFlag2ConditionVTable);
    case 627:
        return MakeCondition(NewScriptObject<PlayerVehicle1ValueCondition>(), 0x273, g_PlayerVehicle1ValueConditionVTable);
    case 628:
        return MakeCondition(NewScriptObject<BothCharactersFlag14Condition>(), 0x274, g_BothCharactersFlag14ConditionVTable);
    case 629:
        return MakeCondition(NewScriptObject<GlobalInt3098e8Condition>(), 0x275, g_GlobalInt3098e8ConditionVTable);
    case 630:
        return MakeCondition(NewScriptObject<NodeValue134SetCondition>(), 0x276, g_NodeValue134SetConditionVTable);
    case 631:
        return MakeCondition(NewScriptObject<GameControllerField500HighCondition>(), 0x277, g_GameControllerField500HighConditionVTable);
    case 632:
        return MakeCondition(NewScriptObject<GameTimer57cCondition>(), 0x278, g_GameTimer57cConditionVTable);
    case 633:
        return MakeCondition(NewScriptObject<SecondCharacterGunStateCondition>(), 0x279, g_SecondCharacterGunStateConditionVTable);
    case 634:
        return MakeCondition(NewScriptObject<HasAmmoCondition>(), 0x27A, g_HasAmmoConditionVTable);
    case 635:
        return MakeCondition(NewScriptObject<CameraForwardDistanceCondition>(), 0x27B, g_CameraForwardDistanceConditionVTable);
    case 636:
        return MakeCondition(NewScriptObject<ObjectContextFlag19Condition>(), 0x27C, g_ObjectContextFlag19ConditionVTable);
    case 637:
        return MakeCondition(NewScriptObject<GameModeIs5Condition>(), 0x27D, g_GameModeIs5ConditionVTable);
    case 638:
        return MakeCondition(NewScriptObject<ObjectContextFlags3or22Condition>(), 0x27E, g_ObjectContextFlags3or22ConditionVTable);
    case 639:
        return MakeCondition(NewScriptObject<GlobalProgressionCondition>(), 0x27F, g_GlobalProgressionConditionVTable);
    case 640:
        return MakeCondition(NewScriptObject<SecondCharacterVehicleValueCondition>(), 0x280, g_SecondCharacterVehicleValueConditionVTable);
    case 641:
        return MakeCondition(NewScriptObject<IsMoviePlayingCondition>(), 0x281, g_IsMoviePlayingConditionVTable);
    case 642:
        return MakeCondition(NewScriptObject<PlayerFlag14Condition>(), 0x282, g_PlayerFlag14ConditionVTable);
    case 643:
        return MakeCondition(NewScriptObject<GameStateIsCondition>(), 0x283, g_GameStateIsConditionVTable);
    case 644:
        return MakeCondition(NewScriptObject<TriggeredByOtherCharacterCondition>(), 0x284, g_TriggeredByOtherCharacterConditionVTable);
    default:
        return nullptr;
    }
}
