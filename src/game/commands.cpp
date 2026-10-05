#include "game/commands.h"

#include "game/memory.h"

namespace
{
template <typename T>
T* NewScriptObject()
{
    return static_cast<T*>(MemoryAllocate(sizeof(T)));
}
}

u32 AddTrailCommand::Size()
{
    return sizeof(AddTrailCommand);
}

void ClearTrailCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearTrailCommand::Size()
{
    return sizeof(ClearTrailCommand);
}

void PositionWarpCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PositionWarpCommand::Size()
{
    return sizeof(PositionWarpCommand);
}

void SetKeyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetKeyCommand::Size()
{
    return sizeof(SetKeyCommand);
}

void NextKeyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 NextKeyCommand::Size()
{
    return sizeof(NextKeyCommand);
}

void RestartPreviousCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestartPreviousCommand::Size()
{
    return sizeof(RestartPreviousCommand);
}

SpawnResidentAgentCommand* SpawnResidentAgentCommand::Construct(SpawnResidentAgentCommand* command, u32)
{
    return MakeCommand(command, g_SpawnResidentAgentCommandVTable);
}

void SpawnResidentAgentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SpawnResidentAgentCommand::Size()
{
    return sizeof(SpawnResidentAgentCommand);
}

DoAnimationCommand* DoAnimationCommand::Construct(DoAnimationCommand* command)
{
    return MakeCommand(command, g_DoAnimationCommandVTable);
}

void DoAnimationCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 DoAnimationCommand::Size()
{
    return sizeof(DoAnimationCommand);
}

void DoParticleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void DoParticleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 DoParticleCommand::Size()
{
    return sizeof(DoParticleCommand);
}

void DoSoundCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 DoSoundCommand::Size()
{
    return sizeof(DoSoundCommand);
}

u32 SetWobbleCommand::Size()
{
    return sizeof(SetWobbleCommand);
}

void ClearWobbleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearWobbleCommand::Size()
{
    return sizeof(ClearWobbleCommand);
}

u32 NowMoveForwardsCommand::Size()
{
    return sizeof(NowMoveForwardsCommand);
}

u32 NoOpNowMoveBackwardsCommand::Size()
{
    return sizeof(NoOpNowMoveBackwardsCommand);
}

u32 NowStrafeLeftCommand::Size()
{
    return sizeof(NowStrafeLeftCommand);
}

u32 NowStrafeRightCommand::Size()
{
    return sizeof(NowStrafeRightCommand);
}

u32 NowTurnLeftCommand::Size()
{
    return sizeof(NowTurnLeftCommand);
}

void NowTurnRightCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 NowTurnRightCommand::Size()
{
    return sizeof(NowTurnRightCommand);
}

void LinkTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 LinkTargetCommand::Size()
{
    return sizeof(LinkTargetCommand);
}

StoreCurrentSpaceCommand* StoreCurrentSpaceCommand::Construct(StoreCurrentSpaceCommand* command)
{
    return MakeCommand(command, g_StoreCurrentSpaceCommandVTable);
}

void StoreCurrentSpaceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StoreCurrentSpaceCommand::Size()
{
    return sizeof(StoreCurrentSpaceCommand);
}

void SetFocusToKeyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToKeyCommand::Size()
{
    return sizeof(SetFocusToKeyCommand);
}

void RotationWarpCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RotationWarpCommand::Size()
{
    return sizeof(RotationWarpCommand);
}

void SetRestartableCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetRestartableCommand::Size()
{
    return sizeof(SetRestartableCommand);
}

void TriggerLinkedObjectsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TriggerLinkedObjectsCommand::Size()
{
    return sizeof(TriggerLinkedObjectsCommand);
}

void SetStateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetStateCommand::Size()
{
    return sizeof(SetStateCommand);
}

void ToggleStateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ToggleStateCommand::Size()
{
    return sizeof(ToggleStateCommand);
}

void NextRouteNodeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 NextRouteNodeCommand::Size()
{
    return sizeof(NextRouteNodeCommand);
}

void DiscardRouteCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DiscardRouteCommand::Size()
{
    return sizeof(DiscardRouteCommand);
}

void SetLogicalRadiusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetLogicalRadiusCommand::Size()
{
    return sizeof(SetLogicalRadiusCommand);
}

void SetBehaviourPriorityCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetBehaviourPriorityCommand::Size()
{
    return sizeof(SetBehaviourPriorityCommand);
}

SetCollisionsCommand* SetCollisionsCommand::Construct(SetCollisionsCommand* command)
{
    return MakeCommand(command, g_SetCollisionsCommandVTable);
}

u32 SetCollisionsCommand::Size()
{
    return sizeof(SetCollisionsCommand);
}

void SetFocusToAgentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToAgentCommand::Size()
{
    return sizeof(SetFocusToAgentCommand);
}

void AttachFocusObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AttachFocusObjectCommand::Size()
{
    return sizeof(AttachFocusObjectCommand);
}

void DropAttachedObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DropAttachedObjectCommand::Size()
{
    return sizeof(DropAttachedObjectCommand);
}

u32 ThrowAttachedObjectCommand::Size()
{
    return sizeof(ThrowAttachedObjectCommand);
}

void UnsupportOverFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UnsupportOverFocusCommand::Size()
{
    return sizeof(UnsupportOverFocusCommand);
}

void UnsupportAboveCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UnsupportAboveCommand::Size()
{
    return sizeof(UnsupportAboveCommand);
}

void ClearFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearFocusCommand::Size()
{
    return sizeof(ClearFocusCommand);
}

void ClearCollisionsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearCollisionsCommand::Size()
{
    return sizeof(ClearCollisionsCommand);
}

u32 SendUserMessageCommand::Size()
{
    return sizeof(SendUserMessageCommand);
}

void BroadcastUserMessageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 BroadcastUserMessageCommand::Size()
{
    return sizeof(BroadcastUserMessageCommand);
}

ClearAnimationCommand* ClearAnimationCommand::Construct(ClearAnimationCommand* command)
{
    return MakeCommand(command, g_ClearAnimationCommandVTable);
}

u32 ClearAnimationCommand::Size()
{
    return sizeof(ClearAnimationCommand);
}

void RequestAttachmentFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RequestAttachmentFocusCommand::Size()
{
    return sizeof(RequestAttachmentFocusCommand);
}

void RequestMessengersFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RequestMessengersFocusCommand::Size()
{
    return sizeof(RequestMessengersFocusCommand);
}

void SetFocusPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionCommand::Size()
{
    return sizeof(SetFocusPositionCommand);
}

void StopMovingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopMovingCommand::Size()
{
    return sizeof(StopMovingCommand);
}

u32 AddNoiseToFocusPositionCommand::Size()
{
    return sizeof(AddNoiseToFocusPositionCommand);
}

void RestartDefaultBehaviourCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestartDefaultBehaviourCommand::Size()
{
    return sizeof(RestartDefaultBehaviourCommand);
}

void ClearUserMessageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearUserMessageCommand::Size()
{
    return sizeof(ClearUserMessageCommand);
}

void RequestMessSourceAsFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RequestMessSourceAsFocusCommand::Size()
{
    return sizeof(RequestMessSourceAsFocusCommand);
}

u32 SetCounterCommand::Size()
{
    return sizeof(SetCounterCommand);
}

u32 ModifyCounterCommand::Size()
{
    return sizeof(ModifyCounterCommand);
}

void ContinueColliderMotionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ContinueColliderMotionCommand::Size()
{
    return sizeof(ContinueColliderMotionCommand);
}

void RevertColliderMotionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RevertColliderMotionCommand::Size()
{
    return sizeof(RevertColliderMotionCommand);
}

ColliderLaunchNowCommand* ColliderLaunchNowCommand::Construct(ColliderLaunchNowCommand* command)
{
    return MakeCommand(command, g_ColliderLaunchNowCommandVTable);
}

void ColliderLaunchNowCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ColliderLaunchNowCommand::Size()
{
    return sizeof(ColliderLaunchNowCommand);
}

void ForceAnimationUpdateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ForceAnimationUpdateCommand::Size()
{
    return sizeof(ForceAnimationUpdateCommand);
}

void DestroySpawnedAttachmentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DestroySpawnedAttachmentCommand::Size()
{
    return sizeof(DestroySpawnedAttachmentCommand);
}

void ApplyImpulseCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ApplyImpulseCommand::Size()
{
    return sizeof(ApplyImpulseCommand);
}

void RequestDetachCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RequestDetachCommand::Size()
{
    return sizeof(RequestDetachCommand);
}

void SetObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetObjectCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetObjectCommand::Size()
{
    return sizeof(SetObjectCommand);
}

void KeepCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 KeepCommand::Size()
{
    return sizeof(KeepCommand);
}

void AttachSpringCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AttachSpringCommand::Size()
{
    return sizeof(AttachSpringCommand);
}

void DetachAllSpringsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DetachAllSpringsCommand::Size()
{
    return sizeof(DetachAllSpringsCommand);
}

u32 SetContactSpringyCommand::Size()
{
    return sizeof(SetContactSpringyCommand);
}

void ClearContactResponseCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearContactResponseCommand::Size()
{
    return sizeof(ClearContactResponseCommand);
}

void DestroyMeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DestroyMeCommand::Size()
{
    return sizeof(DestroyMeCommand);
}

void SetReverbCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetReverbCommand::Size()
{
    return sizeof(SetReverbCommand);
}

u32 AlterWobblePhaseCommand::Size()
{
    return sizeof(AlterWobblePhaseCommand);
}

void BeginMusicCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 BeginMusicCommand::Size()
{
    return sizeof(BeginMusicCommand);
}

void EndContextMusicCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 EndContextMusicCommand::Size()
{
    return sizeof(EndContextMusicCommand);
}

void AddLivesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AddLivesCommand::Size()
{
    return sizeof(AddLivesCommand);
}

void ReleaseAgentRef2Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ReleaseAgentRef2Command::Size()
{
    return sizeof(ReleaseAgentRef2Command);
}

u32 LaunchAgentRef2Command::Size()
{
    return sizeof(LaunchAgentRef2Command);
}

void ClearAgentRef1Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearAgentRef1Command::Size()
{
    return sizeof(ClearAgentRef1Command);
}

void ClearAgentRef2Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearAgentRef2Command::Size()
{
    return sizeof(ClearAgentRef2Command);
}

void ClearFocusPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearFocusPositionCommand::Size()
{
    return sizeof(ClearFocusPositionCommand);
}

void CacheLinkedInstanceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CacheLinkedInstanceCommand::Size()
{
    return sizeof(CacheLinkedInstanceCommand);
}

void SnapRotationCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SnapRotationCommand::Size()
{
    return sizeof(SnapRotationCommand);
}

void StopHeadTrackingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopHeadTrackingCommand::Size()
{
    return sizeof(StopHeadTrackingCommand);
}

void StartHeadTrackingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StartHeadTrackingCommand::Size()
{
    return sizeof(StartHeadTrackingCommand);
}

void DestroyHeadTrackingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DestroyHeadTrackingCommand::Size()
{
    return sizeof(DestroyHeadTrackingCommand);
}

void MakeNoiseCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 MakeNoiseCommand::Size()
{
    return sizeof(MakeNoiseCommand);
}

void SetHeadTrackingTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetHeadTrackingTargetCommand::Size()
{
    return sizeof(SetHeadTrackingTargetCommand);
}

void ClearKnockCountdownCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearKnockCountdownCommand::Size()
{
    return sizeof(ClearKnockCountdownCommand);
}

void PhysicsResetVelocityCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PhysicsResetVelocityCommand::Size()
{
    return sizeof(PhysicsResetVelocityCommand);
}

void SetFocusPositionBesidePlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionBesidePlayerCommand::Size()
{
    return sizeof(SetFocusPositionBesidePlayerCommand);
}

void CopyDesignatorCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CopyDesignatorCommand::Size()
{
    return sizeof(CopyDesignatorCommand);
}

void LinkToNearestPointCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 LinkToNearestPointCommand::Size()
{
    return sizeof(LinkToNearestPointCommand);
}

void RunScriptSlotCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RunScriptSlotCommand::Size()
{
    return sizeof(RunScriptSlotCommand);
}

void OffsetFocusPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 OffsetFocusPositionCommand::Size()
{
    return sizeof(OffsetFocusPositionCommand);
}

void NextLinkedObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 NextLinkedObjectCommand::Size()
{
    return sizeof(NextLinkedObjectCommand);
}

void PhysicsSetGravityCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PhysicsSetGravityCommand::Size()
{
    return sizeof(PhysicsSetGravityCommand);
}

void PhysicsBodyResetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PhysicsBodyResetCommand::Size()
{
    return sizeof(PhysicsBodyResetCommand);
}

void PhysicsBodyActivateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PhysicsBodyActivateCommand::Size()
{
    return sizeof(PhysicsBodyActivateCommand);
}

void SetMagnetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetMagnetCommand::Size()
{
    return sizeof(SetMagnetCommand);
}

void MagnetPullToFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 MagnetPullToFocusCommand::Size()
{
    return sizeof(MagnetPullToFocusCommand);
}

void SetLinkedObjectIndexCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetLinkedObjectIndexCommand::Size()
{
    return sizeof(SetLinkedObjectIndexCommand);
}

void ClearObjectContextTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearObjectContextTargetCommand::Size()
{
    return sizeof(ClearObjectContextTargetCommand);
}

void SetFocusToOriginatorCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToOriginatorCommand::Size()
{
    return sizeof(SetFocusToOriginatorCommand);
}

void DestroyPerceptionsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DestroyPerceptionsCommand::Size()
{
    return sizeof(DestroyPerceptionsCommand);
}

void SetPerceptionWeightCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPerceptionWeightCommand::Size()
{
    return sizeof(SetPerceptionWeightCommand);
}

void AddPerceptionWeightCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AddPerceptionWeightCommand::Size()
{
    return sizeof(AddPerceptionWeightCommand);
}

void PushFromPerceptionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PushFromPerceptionCommand::Size()
{
    return sizeof(PushFromPerceptionCommand);
}

void SetPresenceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPresenceCommand::Size()
{
    return sizeof(SetPresenceCommand);
}

void AddPresenceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AddPresenceCommand::Size()
{
    return sizeof(AddPresenceCommand);
}

void TurnSenseOffCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TurnSenseOffCommand::Size()
{
    return sizeof(TurnSenseOffCommand);
}

void TurnSenseOnCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TurnSenseOnCommand::Size()
{
    return sizeof(TurnSenseOnCommand);
}

void DisableAllPerceptionsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DisableAllPerceptionsCommand::Size()
{
    return sizeof(DisableAllPerceptionsCommand);
}

void EnableAllPerceptionsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 EnableAllPerceptionsCommand::Size()
{
    return sizeof(EnableAllPerceptionsCommand);
}

void SetParentExecutionValueCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetParentExecutionValueCommand::Size()
{
    return sizeof(SetParentExecutionValueCommand);
}

void SetFocusToLinkedObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToLinkedObjectCommand::Size()
{
    return sizeof(SetFocusToLinkedObjectCommand);
}

void PreviousKeyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PreviousKeyCommand::Size()
{
    return sizeof(PreviousKeyCommand);
}

u32 SetCycleAmplitudesCommand::Size()
{
    return sizeof(SetCycleAmplitudesCommand);
}

void RotateWithLinkedCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RotateWithLinkedCommand::Size()
{
    return sizeof(RotateWithLinkedCommand);
}

void StrafeTowardsTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StrafeTowardsTargetCommand::Size()
{
    return sizeof(StrafeTowardsTargetCommand);
}

void PreviousRouteNodeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PreviousRouteNodeCommand::Size()
{
    return sizeof(PreviousRouteNodeCommand);
}

u32 AddWobblePhaseCommand::Size()
{
    return sizeof(AddWobblePhaseCommand);
}

void MoveTowardsDesignatorCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 MoveTowardsDesignatorCommand::Size()
{
    return sizeof(MoveTowardsDesignatorCommand);
}

void SetNoiseMessageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetNoiseMessageCommand::Size()
{
    return sizeof(SetNoiseMessageCommand);
}

void UnlinkTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UnlinkTargetCommand::Size()
{
    return sizeof(UnlinkTargetCommand);
}

u32 AttachMotionBlockCommand::Size()
{
    return sizeof(AttachMotionBlockCommand);
}

void ClearTouchMessageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearTouchMessageCommand::Size()
{
    return sizeof(ClearTouchMessageCommand);
}

void StopTouchMessagesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopTouchMessagesCommand::Size()
{
    return sizeof(StopTouchMessagesCommand);
}

void SendTouchMessagesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SendTouchMessagesCommand::Size()
{
    return sizeof(SendTouchMessagesCommand);
}

u32 AddToFocusCounterCommand::Size()
{
    return sizeof(AddToFocusCounterCommand);
}

void UnlinkFromTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UnlinkFromTargetCommand::Size()
{
    return sizeof(UnlinkFromTargetCommand);
}

u32 AddToLinkedCounterCommand::Size()
{
    return sizeof(AddToLinkedCounterCommand);
}

u32 ForceVolumeControllerCommand::Size()
{
    return sizeof(ForceVolumeControllerCommand);
}

u32 NotifyInstancesWithinCommand::Size()
{
    return sizeof(NotifyInstancesWithinCommand);
}

void SetSurfaceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetSurfaceCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetSurfaceCommand::Size()
{
    return sizeof(SetSurfaceCommand);
}

u32 PushInstancesAwayCommand::Size()
{
    return sizeof(PushInstancesAwayCommand);
}

void SetFocusPositionAlongCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionAlongCommand::Size()
{
    return sizeof(SetFocusPositionAlongCommand);
}

u32 SetFocusCounterCommand::Size()
{
    return sizeof(SetFocusCounterCommand);
}

void RunSlotBehaviourOnLinkedCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RunSlotBehaviourOnLinkedCommand::Size()
{
    return sizeof(RunSlotBehaviourOnLinkedCommand);
}

void StopTargetBehaviourCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopTargetBehaviourCommand::Size()
{
    return sizeof(StopTargetBehaviourCommand);
}

void SetPathIndexCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPathIndexCommand::Size()
{
    return sizeof(SetPathIndexCommand);
}

void SetFocusToOwnerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToOwnerCommand::Size()
{
    return sizeof(SetFocusToOwnerCommand);
}

void SetAgentRef1ToOwnerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetAgentRef1ToOwnerCommand::Size()
{
    return sizeof(SetAgentRef1ToOwnerCommand);
}

void FadeOutMusicSlotCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 FadeOutMusicSlotCommand::Size()
{
    return sizeof(FadeOutMusicSlotCommand);
}

void WarpAgentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 WarpAgentCommand::Size()
{
    return sizeof(WarpAgentCommand);
}

void RotateAgentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RotateAgentCommand::Size()
{
    return sizeof(RotateAgentCommand);
}

void QueueObjectVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 QueueObjectVideoCommand::Size()
{
    return sizeof(QueueObjectVideoCommand);
}

void StartObjectVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StartObjectVideoCommand::Size()
{
    return sizeof(StartObjectVideoCommand);
}

void CancelVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CancelVideoCommand::Size()
{
    return sizeof(CancelVideoCommand);
}

void SetTargetOwnerToSelfCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetTargetOwnerToSelfCommand::Size()
{
    return sizeof(SetTargetOwnerToSelfCommand);
}

void RestoreOwnObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestoreOwnObjectCommand::Size()
{
    return sizeof(RestoreOwnObjectCommand);
}

u32 QueueVideoCommand::Size()
{
    return sizeof(QueueVideoCommand);
}

void StartQueuedVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StartQueuedVideoCommand::Size()
{
    return sizeof(StartQueuedVideoCommand);
}

void SetShadowCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetShadowCommand::Size()
{
    return sizeof(SetShadowCommand);
}

void SetShadowCircleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetShadowCircleCommand::Size()
{
    return sizeof(SetShadowCircleCommand);
}

void SetShadowMeshCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetShadowMeshCommand::Size()
{
    return sizeof(SetShadowMeshCommand);
}

void SetShadowRectangleCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetShadowRectangleCommand::Size()
{
    return sizeof(SetShadowRectangleCommand);
}

void SetShadowSlotCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetShadowSlotCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetShadowSlotCommand::Size()
{
    return sizeof(SetShadowSlotCommand);
}

void ClearShadowSlotCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ClearShadowSlotCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 ClearShadowSlotCommand::Size()
{
    return sizeof(ClearShadowSlotCommand);
}

void LaunchAtTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 LaunchAtTargetCommand::Size()
{
    return sizeof(LaunchAtTargetCommand);
}

void SetContactSoundsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetContactSoundsCommand::Size()
{
    return sizeof(SetContactSoundsCommand);
}

void StopVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopVideoCommand::Size()
{
    return sizeof(StopVideoCommand);
}

void StopSoundCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopSoundCommand::Size()
{
    return sizeof(StopSoundCommand);
}

void NoOp197Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp197Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

void NoOp197Command::ExecuteOn(GameNode*)
{
}

u32 NoOp197Command::Size()
{
    return sizeof(NoOp197Command);
}

void SetCollisionBoxSizeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetCollisionBoxSizeCommand::Size()
{
    return sizeof(SetCollisionBoxSizeCommand);
}

void NextLinkedObjectInListCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 NextLinkedObjectInListCommand::Size()
{
    return sizeof(NextLinkedObjectInListCommand);
}

void PickLinkedObjectNearPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PickLinkedObjectNearPlayerCommand::Size()
{
    return sizeof(PickLinkedObjectNearPlayerCommand);
}

void TriggerInstanceAtOwnBoxCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TriggerInstanceAtOwnBoxCommand::Size()
{
    return sizeof(TriggerInstanceAtOwnBoxCommand);
}

void SetBodyMassCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetBodyMassCommand::Size()
{
    return sizeof(SetBodyMassCommand);
}

void SetFocusPositionOffsetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionOffsetCommand::Size()
{
    return sizeof(SetFocusPositionOffsetCommand);
}

void SetStoredPositionAtAngleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetStoredPositionAtAngleCommand::Size()
{
    return sizeof(SetStoredPositionAtAngleCommand);
}

void SaveScriptStateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SaveScriptStateCommand::Size()
{
    return sizeof(SaveScriptStateCommand);
}

void ClearSavedScriptStateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearSavedScriptStateCommand::Size()
{
    return sizeof(ClearSavedScriptStateCommand);
}

u32 SetRankCommand::Size()
{
    return sizeof(SetRankCommand);
}

u32 SetTriggerRankCommand::Size()
{
    return sizeof(SetTriggerRankCommand);
}

void TriggerInstancesByRankCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TriggerInstancesByRankCommand::Size()
{
    return sizeof(TriggerInstancesByRankCommand);
}

void MarkTimeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 MarkTimeCommand::Size()
{
    return sizeof(MarkTimeCommand);
}

void ClearMarkedTimeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearMarkedTimeCommand::Size()
{
    return sizeof(ClearMarkedTimeCommand);
}

void RestartRouteCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestartRouteCommand::Size()
{
    return sizeof(RestartRouteCommand);
}

void ControllerRumbleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ControllerRumbleCommand::Size()
{
    return sizeof(ControllerRumbleCommand);
}

void SetSoundParamsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetSoundParamsCommand::Size()
{
    return sizeof(SetSoundParamsCommand);
}

void CreateCrateContentsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CreateCrateContentsCommand::Size()
{
    return sizeof(CreateCrateContentsCommand);
}

void PickUpWumpaCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void PickUpWumpaCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 PickUpWumpaCommand::Size()
{
    return sizeof(PickUpWumpaCommand);
}

void CreateDamageCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 CreateDamageCommand::Size()
{
    return sizeof(CreateDamageCommand);
}

void SetAgentCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetAgentCommand::Size()
{
    return sizeof(SetAgentCommand);
}

void SetPlayerRespawnPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPlayerRespawnPositionCommand::Size()
{
    return sizeof(SetPlayerRespawnPositionCommand);
}

void RestartFromCheckpointCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestartFromCheckpointCommand::Size()
{
    return sizeof(RestartFromCheckpointCommand);
}

void SetCrateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetCrateCommand::Size()
{
    return sizeof(SetCrateCommand);
}

void TriggerBalancedCrateFallingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TriggerBalancedCrateFallingCommand::Size()
{
    return sizeof(TriggerBalancedCrateFallingCommand);
}

void PickUpHealthCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void PickUpHealthCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 PickUpHealthCommand::Size()
{
    return sizeof(PickUpHealthCommand);
}

void SetPlayerInputCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPlayerInputCommand::Size()
{
    return sizeof(SetPlayerInputCommand);
}

void TriggerAllNitroCratesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 TriggerAllNitroCratesCommand::Size()
{
    return sizeof(TriggerAllNitroCratesCommand);
}

u32 ApplyVelocityCommand::Size()
{
    return sizeof(ApplyVelocityCommand);
}

SetKeyNearestPlayerCommand* SetKeyNearestPlayerCommand::Construct(SetKeyNearestPlayerCommand* command)
{
    return MakeCommand(command, g_SetKeyNearestPlayerCommandVTable);
}

void SetKeyNearestPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetKeyNearestPlayerCommand::Size()
{
    return sizeof(SetKeyNearestPlayerCommand);
}

RaycastFocusPositionCommand* RaycastFocusPositionCommand::Construct(RaycastFocusPositionCommand* command)
{
    return MakeCommand(command, g_RaycastFocusPositionCommandVTable);
}

void RaycastFocusPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RaycastFocusPositionCommand::Size()
{
    return sizeof(RaycastFocusPositionCommand);
}

u32 ApplyVelocityToSelfCommand::Size()
{
    return sizeof(ApplyVelocityToSelfCommand);
}

void SetChiChiGrassCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetChiChiGrassCommand::Size()
{
    return sizeof(SetChiChiGrassCommand);
}

ReduceHitPointsCommand* ReduceHitPointsCommand::Construct(ReduceHitPointsCommand* command)
{
    return MakeCommand(command, g_ReduceHitPointsCommandVTable);
}

void ReduceHitPointsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ReduceHitPointsCommand::Size()
{
    return sizeof(ReduceHitPointsCommand);
}

SetHitPointsCommand* SetHitPointsCommand::Construct(SetHitPointsCommand* command)
{
    return MakeCommand(command, g_SetHitPointsCommandVTable);
}

void SetHitPointsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetHitPointsCommand::Size()
{
    return sizeof(SetHitPointsCommand);
}

NoOpSetRayTestsCommand* NoOpSetRayTestsCommand::Construct(NoOpSetRayTestsCommand* command)
{
    return MakeCommand(command, g_NoOpSetRayTestsCommandVTable);
}

void NoOpSetRayTestsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOpSetRayTestsCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOpSetRayTestsCommand::Size()
{
    return sizeof(NoOpSetRayTestsCommand);
}

void NoOpNowGoForwardCollidableCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOpNowGoForwardCollidableCommand::Size()
{
    return sizeof(NoOpNowGoForwardCollidableCommand);
}

u32 NowGoBackCollidableCommand::Size()
{
    return sizeof(NowGoBackCollidableCommand);
}

u32 SetPlayAreaCommand::Size()
{
    return sizeof(SetPlayAreaCommand);
}

void AddCrystalCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddCrystalCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 AddCrystalCommand::Size()
{
    return sizeof(AddCrystalCommand);
}

void NoOp536Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp536Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp536Command::Size()
{
    return sizeof(NoOp536Command);
}

void AddGemCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddGemCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 AddGemCommand::Size()
{
    return sizeof(AddGemCommand);
}

void NoOp538Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp538Command::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

void NoOp538Command::ExecuteOn(GameNode*)
{
}

u32 NoOp538Command::Size()
{
    return sizeof(NoOp538Command);
}

SetCustomPickupCommand* SetCustomPickupCommand::Construct(SetCustomPickupCommand* command)
{
    return MakeCommand(command, g_SetCustomPickupCommandVTable);
}

void SetCustomPickupCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCustomPickupCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetCustomPickupCommand::Size()
{
    return sizeof(SetCustomPickupCommand);
}

void SetCustomProjectileCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SetCustomProjectileCommand::Size()
{
    return sizeof(SetCustomProjectileCommand);
}

void ShootCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ShootCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 ShootCommand::Size()
{
    return sizeof(ShootCommand);
}

GetShortRouteCommand* GetShortRouteCommand::Construct(GetShortRouteCommand* command)
{
    return MakeCommand(command, g_GetShortRouteCommandVTable);
}

u32 GetShortRouteCommand::Size()
{
    return sizeof(GetShortRouteCommand);
}

void NoOpFuelPayGateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOpFuelPayGateCommand::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOpFuelPayGateCommand::Size()
{
    return sizeof(NoOpFuelPayGateCommand);
}

void OpenAllLinkedFurnitureCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 OpenAllLinkedFurnitureCommand::Size()
{
    return sizeof(OpenAllLinkedFurnitureCommand);
}

void CloseAllLinkedFurnitureCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CloseAllLinkedFurnitureCommand::Size()
{
    return sizeof(CloseAllLinkedFurnitureCommand);
}

void AttachAllLinkedAgentsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AttachAllLinkedAgentsCommand::Size()
{
    return sizeof(AttachAllLinkedAgentsCommand);
}

void DetachAllLinkedAgentsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DetachAllLinkedAgentsCommand::Size()
{
    return sizeof(DetachAllLinkedAgentsCommand);
}

u32 SetVehicleHumiliskateCommand::Size()
{
    return sizeof(SetVehicleHumiliskateCommand);
}

void SetFocusToPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToPlayerCommand::Size()
{
    return sizeof(SetFocusToPlayerCommand);
}

RequestFocusCommand* RequestFocusCommand::Construct(RequestFocusCommand* command, u32)
{
    return MakeCommand(command, g_RequestFocusCommandVTable);
}

u32 RequestFocusCommand::Size()
{
    return sizeof(RequestFocusCommand);
}

void SetFocusPropertiesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPropertiesCommand::Size()
{
    return sizeof(SetFocusPropertiesCommand);
}

u32 SetCameraCommand::Size()
{
    return sizeof(SetCameraCommand);
}

void RestoreCameraDefaultsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 RestoreCameraDefaultsCommand::Size()
{
    return sizeof(RestoreCameraDefaultsCommand);
}

void LinkToFocusCharacterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 LinkToFocusCharacterCommand::Size()
{
    return sizeof(LinkToFocusCharacterCommand);
}

void UnlinkCharactersCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UnlinkCharactersCommand::Size()
{
    return sizeof(UnlinkCharactersCommand);
}

u32 DamageOriginatorCommand::Size()
{
    return sizeof(DamageOriginatorCommand);
}

void SetAgentRef1ToPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetAgentRef1ToPlayerCommand::Size()
{
    return sizeof(SetAgentRef1ToPlayerCommand);
}

void SetFocusPositionToPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionToPlayerCommand::Size()
{
    return sizeof(SetFocusPositionToPlayerCommand);
}

void NoOp568Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp568Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp568Command::Size()
{
    return sizeof(NoOp568Command);
}

void ExitVehicleModeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ExitVehicleModeCommand::Size()
{
    return sizeof(ExitVehicleModeCommand);
}

void SetVehicleRollerbrawlCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetVehicleRollerbrawlCommand::Size()
{
    return sizeof(SetVehicleRollerbrawlCommand);
}

void SetVehicleHoverboardCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetVehicleHoverboardCommand::Size()
{
    return sizeof(SetVehicleHoverboardCommand);
}

u32 SetMotionCommand::Size()
{
    return sizeof(SetMotionCommand);
}

void SetNearestPointFlagsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetNearestPointFlagsCommand::Size()
{
    return sizeof(SetNearestPointFlagsCommand);
}

u32 CreateHeadTrackingCommand::Size()
{
    return sizeof(CreateHeadTrackingCommand);
}

void SetFocusPositionToNearestPointCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusPositionToNearestPointCommand::Size()
{
    return sizeof(SetFocusPositionToNearestPointCommand);
}

void SetFocusToGameActorCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToGameActorCommand::Size()
{
    return sizeof(SetFocusToGameActorCommand);
}

void BecomeStickyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 BecomeStickyCommand::Size()
{
    return sizeof(BecomeStickyCommand);
}

void CountPlayerCirclingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CountPlayerCirclingCommand::Size()
{
    return sizeof(CountPlayerCirclingCommand);
}

void CountPlayerApproachCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CountPlayerApproachCommand::Size()
{
    return sizeof(CountPlayerApproachCommand);
}

void ApplyVelocityToHeldBodyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ApplyVelocityToHeldBodyCommand::Size()
{
    return sizeof(ApplyVelocityToHeldBodyCommand);
}

void BecomeNormalCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 BecomeNormalCommand::Size()
{
    return sizeof(BecomeNormalCommand);
}

void AddPerceptionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 AddPerceptionCommand::Size()
{
    return sizeof(AddPerceptionCommand);
}

void SwingAroundCameraCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SwingAroundCameraCommand::Size()
{
    return sizeof(SwingAroundCameraCommand);
}

void NoOp584Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp584Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp584Command::Size()
{
    return sizeof(NoOp584Command);
}

void NoOp586Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp586Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp586Command::Size()
{
    return sizeof(NoOp586Command);
}

void SetAttacksTakenCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetAttacksTakenCommand::Size()
{
    return sizeof(SetAttacksTakenCommand);
}

void PlayerFaceTowardsCameraCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PlayerFaceTowardsCameraCommand::Size()
{
    return sizeof(PlayerFaceTowardsCameraCommand);
}

void CutsceneStartCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CutsceneStartCommand::Size()
{
    return sizeof(CutsceneStartCommand);
}

void CutsceneEndCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CutsceneEndCommand::Size()
{
    return sizeof(CutsceneEndCommand);
}

void CutsceneCameraMoveCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CutsceneCameraMoveCommand::Size()
{
    return sizeof(CutsceneCameraMoveCommand);
}

void CameraSaveParamsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CameraSaveParamsCommand::Size()
{
    return sizeof(CameraSaveParamsCommand);
}

void ToggleCutsceneCameraCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ToggleCutsceneCameraCommand::Size()
{
    return sizeof(ToggleCutsceneCameraCommand);
}

void CutsceneCameraTargetsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CutsceneCameraTargetsCommand::Size()
{
    return sizeof(CutsceneCameraTargetsCommand);
}

u32 StartWhackawormCommand::Size()
{
    return sizeof(StartWhackawormCommand);
}

void ProgressWhackawormCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ProgressWhackawormCommand::Size()
{
    return sizeof(ProgressWhackawormCommand);
}

void EndWhackawormCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 EndWhackawormCommand::Size()
{
    return sizeof(EndWhackawormCommand);
}

void ReleasePlayerHoldCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ReleasePlayerHoldCommand::Size()
{
    return sizeof(ReleasePlayerHoldCommand);
}

void WarpToChunkLinkTowardsPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 WarpToChunkLinkTowardsPlayerCommand::Size()
{
    return sizeof(WarpToChunkLinkTowardsPlayerCommand);
}

void SetVehicleWrestleCreatureCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetVehicleWrestleCreatureCommand::Size()
{
    return sizeof(SetVehicleWrestleCreatureCommand);
}

void FadeoutScreenCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 FadeoutScreenCommand::Size()
{
    return sizeof(FadeoutScreenCommand);
}

void DisplayBottomTextCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DisplayBottomTextCommand::Size()
{
    return sizeof(DisplayBottomTextCommand);
}

void ResetCharacterFallCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ResetCharacterFallCommand::Size()
{
    return sizeof(ResetCharacterFallCommand);
}

void DismissCharacterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DismissCharacterCommand::Size()
{
    return sizeof(DismissCharacterCommand);
}

void CameraFocusObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CameraFocusObjectCommand::Size()
{
    return sizeof(CameraFocusObjectCommand);
}

void CameraStopFocusObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CameraStopFocusObjectCommand::Size()
{
    return sizeof(CameraStopFocusObjectCommand);
}

void ClearBottomTextCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearBottomTextCommand::Size()
{
    return sizeof(ClearBottomTextCommand);
}

void NoOp609Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp609Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp609Command::Size()
{
    return sizeof(NoOp609Command);
}

void NoOp610Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOp610Command::Execute(TimeClock*, BehaviourRunner*, BehaviourLevel*)
{
}

u32 NoOp610Command::Size()
{
    return sizeof(NoOp610Command);
}

void SetCharacterHomeChunkCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetCharacterHomeChunkCommand::Size()
{
    return sizeof(SetCharacterHomeChunkCommand);
}

void EnablePlayerControlCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 EnablePlayerControlCommand::Size()
{
    return sizeof(EnablePlayerControlCommand);
}

void DisablePlayerControlCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DisablePlayerControlCommand::Size()
{
    return sizeof(DisablePlayerControlCommand);
}

void StopStickingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 StopStickingCommand::Size()
{
    return sizeof(StopStickingCommand);
}

void SetPlayerScriptFlagCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPlayerScriptFlagCommand::Size()
{
    return sizeof(SetPlayerScriptFlagCommand);
}

void PlaceCharacterInChunkCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PlaceCharacterInChunkCommand::Size()
{
    return sizeof(PlaceCharacterInChunkCommand);
}

void HitInstancesInBoxesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 HitInstancesInBoxesCommand::Size()
{
    return sizeof(HitInstancesInBoxesCommand);
}

void ForceGameOverCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ForceGameOverCommand::Size()
{
    return sizeof(ForceGameOverCommand);
}

void ShowBottomTextCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ShowBottomTextCommand::Size()
{
    return sizeof(ShowBottomTextCommand);
}

void HideBottomTextCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 HideBottomTextCommand::Size()
{
    return sizeof(HideBottomTextCommand);
}

void SetFocusToCameraTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToCameraTargetCommand::Size()
{
    return sizeof(SetFocusToCameraTargetCommand);
}

void CharacterSoundProxyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CharacterSoundProxyCommand::Size()
{
    return sizeof(CharacterSoundProxyCommand);
}

void SetFollowCameraTargetCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFollowCameraTargetCommand::Size()
{
    return sizeof(SetFollowCameraTargetCommand);
}

void SetScriptGlobalFlagCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetScriptGlobalFlagCommand::Size()
{
    return sizeof(SetScriptGlobalFlagCommand);
}

void SwitchCharacterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SwitchCharacterCommand::Size()
{
    return sizeof(SwitchCharacterCommand);
}

void UseOwnFollowCamerasCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UseOwnFollowCamerasCommand::Size()
{
    return sizeof(UseOwnFollowCamerasCommand);
}

void UseTriggerCamerasCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 UseTriggerCamerasCommand::Size()
{
    return sizeof(UseTriggerCamerasCommand);
}

void SetFollowCameraRateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFollowCameraRateCommand::Size()
{
    return sizeof(SetFollowCameraRateCommand);
}

void SetCameraNodeValuesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetCameraNodeValuesCommand::Size()
{
    return sizeof(SetCameraNodeValuesCommand);
}

void SwitchBodyFlagsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SwitchBodyFlagsCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 SwitchBodyFlagsCommand::Size()
{
    return sizeof(SwitchBodyFlagsCommand);
}

void PushPlayerVehicleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PushPlayerVehicleCommand::Size()
{
    return sizeof(PushPlayerVehicleCommand);
}

void SetLinkedObjectNearestPlayerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetLinkedObjectNearestPlayerCommand::Size()
{
    return sizeof(SetLinkedObjectNearestPlayerCommand);
}

void SetPlayerModeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetPlayerModeCommand::Size()
{
    return sizeof(SetPlayerModeCommand);
}

u32 PlayMovieCommand::Size()
{
    return sizeof(PlayMovieCommand);
}

void AddAmmoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddAmmoCommand::Execute(TimeClock*, BehaviourRunner* runner, BehaviourLevel*)
{
    CallVirtual<void>(this, vtable, ExecuteOnSlot, runner->agentNode);
}

u32 AddAmmoCommand::Size()
{
    return sizeof(AddAmmoCommand);
}

void SetFocusToLinkedObjectInViewCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetFocusToLinkedObjectInViewCommand::Size()
{
    return sizeof(SetFocusToLinkedObjectInViewCommand);
}

u32 EnableBossModeCommand::Size()
{
    return sizeof(EnableBossModeCommand);
}

void DamageBossCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DamageBossCommand::Size()
{
    return sizeof(DamageBossCommand);
}

void ExitBossModeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ExitBossModeCommand::Size()
{
    return sizeof(ExitBossModeCommand);
}

void FinalBossWeaponsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 FinalBossWeaponsCommand::Size()
{
    return sizeof(FinalBossWeaponsCommand);
}

void CreateNodeControllerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CreateNodeControllerCommand::Size()
{
    return sizeof(CreateNodeControllerCommand);
}

void SetGaugeIconCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetGaugeIconCommand::Size()
{
    return sizeof(SetGaugeIconCommand);
}

u32 RaiseStoryAreaCommand::Size()
{
    return sizeof(RaiseStoryAreaCommand);
}

void SetCountedValueCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetCountedValueCommand::Size()
{
    return sizeof(SetCountedValueCommand);
}

void ClearCountedValueCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearCountedValueCommand::Size()
{
    return sizeof(ClearCountedValueCommand);
}

void HoldVehicleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 HoldVehicleCommand::Size()
{
    return sizeof(HoldVehicleCommand);
}

void ReleaseVehicleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ReleaseVehicleCommand::Size()
{
    return sizeof(ReleaseVehicleCommand);
}

void CameraTopdownModeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 CameraTopdownModeCommand::Size()
{
    return sizeof(CameraTopdownModeCommand);
}

void PlayCreditsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 PlayCreditsCommand::Size()
{
    return sizeof(PlayCreditsCommand);
}

void SetMaskControllerIdsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetMaskControllerIdsCommand::Size()
{
    return sizeof(SetMaskControllerIdsCommand);
}

void ResetMaskControllerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ResetMaskControllerCommand::Size()
{
    return sizeof(ResetMaskControllerCommand);
}

void DisplayBottomTextInstanceCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 DisplayBottomTextInstanceCommand::Size()
{
    return sizeof(DisplayBottomTextInstanceCommand);
}

u32 SetSplineControllerValuesCommand::Size()
{
    return sizeof(SetSplineControllerValuesCommand);
}

void MakeCharactersIdleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 MakeCharactersIdleCommand::Size()
{
    return sizeof(MakeCharactersIdleCommand);
}

void ClearCharacterDeadCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ClearCharacterDeadCommand::Size()
{
    return sizeof(ClearCharacterDeadCommand);
}

void SetSkateControllerIdsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 SetSkateControllerIdsCommand::Size()
{
    return sizeof(SetSkateControllerIdsCommand);
}

void ResetCameraCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

u32 ResetCameraCommand::Size()
{
    return sizeof(ResetCameraCommand);
}

void* BuildScriptCommand(void*, u32 id, s32 kind)
{
    if (kind != ObjectBuilder::CommandKind)
    {
        return nullptr;
    }

    switch (id)
    {
    case 1:
        return MakeCommand(NewScriptObject<AddTrailCommand>(), g_AddTrailCommandVTable);
    case 2:
        return MakeCommand(NewScriptObject<ClearTrailCommand>(), g_ClearTrailCommandVTable);
    case 3:
        return MakeCommand(NewScriptObject<PositionWarpCommand>(), g_PositionWarpCommandVTable);
    case 4:
        return MakeCommand(NewScriptObject<SetKeyCommand>(), g_SetKeyCommandVTable);
    case 5:
        return MakeCommand(NewScriptObject<NextKeyCommand>(), g_NextKeyCommandVTable);
    case 7:
        return MakeCommand(NewScriptObject<RestartPreviousCommand>(), g_RestartPreviousCommandVTable);
    case 8:
        return SpawnResidentAgentCommand::Construct(NewScriptObject<SpawnResidentAgentCommand>(), 0);
    case 9:
        return DoAnimationCommand::Construct(NewScriptObject<DoAnimationCommand>());
    case 10:
        return MakeCommand(NewScriptObject<DoParticleCommand>(), g_DoParticleCommandVTable);
    case 11:
        return MakeCommand(NewScriptObject<DoSoundCommand>(), g_DoSoundCommandVTable);
    case 12:
        return MakeCommand(NewScriptObject<SetWobbleCommand>(), g_SetWobbleCommandVTable);
    case 13:
        return MakeCommand(NewScriptObject<ClearWobbleCommand>(), g_ClearWobbleCommandVTable);
    case 14:
        return MakeCommand(NewScriptObject<NowMoveForwardsCommand>(), g_NowMoveForwardsCommandVTable);
    case 15:
        return MakeCommand(NewScriptObject<NoOpNowMoveBackwardsCommand>(), g_NoOpNowMoveBackwardsCommandVTable);
    case 16:
        return MakeCommand(NewScriptObject<NowStrafeLeftCommand>(), g_NowStrafeLeftCommandVTable);
    case 17:
        return MakeCommand(NewScriptObject<NowStrafeRightCommand>(), g_NowStrafeRightCommandVTable);
    case 18:
        return MakeCommand(NewScriptObject<NowTurnLeftCommand>(), g_NowTurnLeftCommandVTable);
    case 19:
        // NowTurnRight's ID, which retail makes a turn left for as well
        return MakeCommand(NewScriptObject<NowTurnLeftCommand>(), g_NowTurnLeftCommandVTable);
    case 23:
        return MakeCommand(NewScriptObject<LinkTargetCommand>(), g_LinkTargetCommandVTable);
    case 24:
        return MakeCommand(NewScriptObject<LinkTargetCommand>(), g_LinkTargetCommandVTable);
    case 27:
        return StoreCurrentSpaceCommand::Construct(NewScriptObject<StoreCurrentSpaceCommand>());
    case 28:
        return MakeCommand(NewScriptObject<SetFocusToKeyCommand>(), g_SetFocusToKeyCommandVTable);
    case 29:
        return MakeCommand(NewScriptObject<RotationWarpCommand>(), g_RotationWarpCommandVTable);
    case 31:
        return MakeCommand(NewScriptObject<SetRestartableCommand>(), g_SetRestartableCommandVTable);
    case 33:
        return MakeCommand(NewScriptObject<TriggerLinkedObjectsCommand>(), g_TriggerLinkedObjectsCommandVTable);
    case 34:
        return MakeCommand(NewScriptObject<SetStateCommand>(), g_SetStateCommandVTable);
    case 35:
        return MakeCommand(NewScriptObject<ToggleStateCommand>(), g_ToggleStateCommandVTable);
    case 36:
        return MakeCommand(NewScriptObject<NextRouteNodeCommand>(), g_NextRouteNodeCommandVTable);
    case 39:
        return MakeCommand(NewScriptObject<DiscardRouteCommand>(), g_DiscardRouteCommandVTable);
    case 40:
        return MakeCommand(NewScriptObject<SetLogicalRadiusCommand>(), g_SetLogicalRadiusCommandVTable);
    case 42:
        return MakeCommand(NewScriptObject<SetBehaviourPriorityCommand>(), g_SetBehaviourPriorityCommandVTable);
    case 44:
        return SetCollisionsCommand::Construct(NewScriptObject<SetCollisionsCommand>());
    case 45:
        return MakeCommand(NewScriptObject<SetFocusToAgentCommand>(), g_SetFocusToAgentCommandVTable);
    case 47:
        return MakeCommand(NewScriptObject<AttachFocusObjectCommand>(), g_AttachFocusObjectCommandVTable);
    case 48:
        return MakeCommand(NewScriptObject<DropAttachedObjectCommand>(), g_DropAttachedObjectCommandVTable);
    case 49:
        return MakeCommand(NewScriptObject<ThrowAttachedObjectCommand>(), g_ThrowAttachedObjectCommandVTable);
    case 50:
        return MakeCommand(NewScriptObject<UnsupportOverFocusCommand>(), g_UnsupportOverFocusCommandVTable);
    case 51:
        return MakeCommand(NewScriptObject<UnsupportAboveCommand>(), g_UnsupportAboveCommandVTable);
    case 52:
        return MakeCommand(NewScriptObject<ClearFocusCommand>(), g_ClearFocusCommandVTable);
    case 53:
        return MakeCommand(NewScriptObject<ClearCollisionsCommand>(), g_ClearCollisionsCommandVTable);
    case 54:
        return MakeCommand(NewScriptObject<SendUserMessageCommand>(), g_SendUserMessageCommandVTable);
    case 55:
        return MakeCommand(NewScriptObject<BroadcastUserMessageCommand>(), g_BroadcastUserMessageCommandVTable);
    case 56:
        return ClearAnimationCommand::Construct(NewScriptObject<ClearAnimationCommand>());
    case 57:
        return MakeCommand(NewScriptObject<RequestAttachmentFocusCommand>(), g_RequestAttachmentFocusCommandVTable);
    case 59:
        return MakeCommand(NewScriptObject<RequestMessengersFocusCommand>(), g_RequestMessengersFocusCommandVTable);
    case 60:
        return MakeCommand(NewScriptObject<SetFocusPositionCommand>(), g_SetFocusPositionCommandVTable);
    case 61:
        return MakeCommand(NewScriptObject<StopMovingCommand>(), g_StopMovingCommandVTable);
    case 62:
        return MakeCommand(NewScriptObject<AddNoiseToFocusPositionCommand>(), g_AddNoiseToFocusPositionCommandVTable);
    case 63:
        return MakeCommand(NewScriptObject<RestartDefaultBehaviourCommand>(), g_RestartDefaultBehaviourCommandVTable);
    case 64:
        return MakeCommand(NewScriptObject<AttachFocusObjectCommand>(), g_AttachFocusObjectCommandVTable);
    case 65:
        return MakeCommand(NewScriptObject<SendUserMessageCommand>(), g_SendUserMessageCommandVTable);
    case 66:
        return MakeCommand(NewScriptObject<ClearUserMessageCommand>(), g_ClearUserMessageCommandVTable);
    case 67:
        return MakeCommand(NewScriptObject<RequestMessSourceAsFocusCommand>(), g_RequestMessSourceAsFocusCommandVTable);
    case 68:
        return MakeCommand(NewScriptObject<SetCounterCommand>(), g_SetCounterCommandVTable);
    case 69:
        return MakeCommand(NewScriptObject<ModifyCounterCommand>(), g_ModifyCounterCommandVTable);
    case 70:
        return MakeCommand(NewScriptObject<ContinueColliderMotionCommand>(), g_ContinueColliderMotionCommandVTable);
    case 71:
        return MakeCommand(NewScriptObject<RevertColliderMotionCommand>(), g_RevertColliderMotionCommandVTable);
    case 72:
        return ColliderLaunchNowCommand::Construct(NewScriptObject<ColliderLaunchNowCommand>());
    case 74:
        return MakeCommand(NewScriptObject<ForceAnimationUpdateCommand>(), g_ForceAnimationUpdateCommandVTable);
    case 75:
        return MakeCommand(NewScriptObject<DestroySpawnedAttachmentCommand>(), g_DestroySpawnedAttachmentCommandVTable);
    case 76:
        return MakeCommand(NewScriptObject<ApplyImpulseCommand>(), g_ApplyImpulseCommandVTable);
    case 77:
        return MakeCommand(NewScriptObject<RequestDetachCommand>(), g_RequestDetachCommandVTable);
    case 78:
        return MakeCommand(NewScriptObject<SetObjectCommand>(), g_SetObjectCommandVTable);
    case 79:
        return MakeCommand(NewScriptObject<KeepCommand>(), g_KeepCommandVTable);
    case 80:
        return MakeCommand(NewScriptObject<AttachSpringCommand>(), g_AttachSpringCommandVTable);
    case 81:
        return MakeCommand(NewScriptObject<DetachAllSpringsCommand>(), g_DetachAllSpringsCommandVTable);
    case 82:
        return MakeCommand(NewScriptObject<SetContactSpringyCommand>(), g_SetContactSpringyCommandVTable);
    case 83:
        return MakeCommand(NewScriptObject<SetContactSpringyCommand>(), g_SetContactSpringyCommandVTable);
    case 84:
        return MakeCommand(NewScriptObject<ClearContactResponseCommand>(), g_ClearContactResponseCommandVTable);
    case 85:
        return MakeCommand(NewScriptObject<DestroyMeCommand>(), g_DestroyMeCommandVTable);
    case 86:
        return MakeCommand(NewScriptObject<SetReverbCommand>(), g_SetReverbCommandVTable);
    case 87:
        return MakeCommand(NewScriptObject<AlterWobblePhaseCommand>(), g_AlterWobblePhaseCommandVTable);
    case 88:
        return MakeCommand(NewScriptObject<BeginMusicCommand>(), g_BeginMusicCommandVTable);
    case 89:
        return MakeCommand(NewScriptObject<EndContextMusicCommand>(), g_EndContextMusicCommandVTable);
    case 90:
        return MakeCommand(NewScriptObject<SetFocusToAgentCommand>(), g_SetFocusToAgentCommandVTable);
    case 91:
        return MakeCommand(NewScriptObject<SetFocusToAgentCommand>(), g_SetFocusToAgentCommandVTable);
    case 92:
        return MakeCommand(NewScriptObject<AddLivesCommand>(), g_AddLivesCommandVTable);
    case 93:
        return MakeCommand(NewScriptObject<AddLivesCommand>(), g_AddLivesCommandVTable);
    case 94:
        return MakeCommand(NewScriptObject<AttachFocusObjectCommand>(), g_AttachFocusObjectCommandVTable);
    case 95:
        return MakeCommand(NewScriptObject<ReleaseAgentRef2Command>(), g_ReleaseAgentRef2CommandVTable);
    case 96:
        return MakeCommand(NewScriptObject<LaunchAgentRef2Command>(), g_LaunchAgentRef2CommandVTable);
    case 97:
        return MakeCommand(NewScriptObject<ClearAgentRef1Command>(), g_ClearAgentRef1CommandVTable);
    case 98:
        return MakeCommand(NewScriptObject<ClearAgentRef2Command>(), g_ClearAgentRef2CommandVTable);
    case 99:
        return MakeCommand(NewScriptObject<RequestMessengersFocusCommand>(), g_RequestMessengersFocusCommandVTable);
    case 100:
        return MakeCommand(NewScriptObject<RequestMessengersFocusCommand>(), g_RequestMessengersFocusCommandVTable);
    case 101:
        return MakeCommand(NewScriptObject<RequestMessSourceAsFocusCommand>(), g_RequestMessSourceAsFocusCommandVTable);
    case 102:
        return MakeCommand(NewScriptObject<RequestMessSourceAsFocusCommand>(), g_RequestMessSourceAsFocusCommandVTable);
    case 103:
        return MakeCommand(NewScriptObject<SetFocusToKeyCommand>(), g_SetFocusToKeyCommandVTable);
    case 104:
        return MakeCommand(NewScriptObject<SetFocusToAgentCommand>(), g_SetFocusToAgentCommandVTable);
    case 105:
        return MakeCommand(NewScriptObject<PositionWarpCommand>(), g_PositionWarpCommandVTable);
    case 106:
        return MakeCommand(NewScriptObject<RequestMessengersFocusCommand>(), g_RequestMessengersFocusCommandVTable);
    case 108:
        return MakeCommand(NewScriptObject<SetFocusPositionCommand>(), g_SetFocusPositionCommandVTable);
    case 109:
        return MakeCommand(NewScriptObject<AddNoiseToFocusPositionCommand>(), g_AddNoiseToFocusPositionCommandVTable);
    case 110:
        return MakeCommand(NewScriptObject<ClearFocusPositionCommand>(), g_ClearFocusPositionCommandVTable);
    case 111:
        return MakeCommand(NewScriptObject<CacheLinkedInstanceCommand>(), g_CacheLinkedInstanceCommandVTable);
    case 112:
        return SpawnResidentAgentCommand::Construct(NewScriptObject<SpawnResidentAgentCommand>(), 1);
    case 113:
        return MakeCommand(NewScriptObject<SnapRotationCommand>(), g_SnapRotationCommandVTable);
    case 114:
        return MakeCommand(NewScriptObject<StopHeadTrackingCommand>(), g_StopHeadTrackingCommandVTable);
    case 115:
        return MakeCommand(NewScriptObject<StartHeadTrackingCommand>(), g_StartHeadTrackingCommandVTable);
    case 116:
        return MakeCommand(NewScriptObject<DestroyHeadTrackingCommand>(), g_DestroyHeadTrackingCommandVTable);
    case 117:
        return MakeCommand(NewScriptObject<MakeNoiseCommand>(), g_MakeNoiseCommandVTable);
    case 118:
        return MakeCommand(NewScriptObject<SetHeadTrackingTargetCommand>(), g_SetHeadTrackingTargetCommandVTable);
    case 119:
        return MakeCommand(NewScriptObject<ClearKnockCountdownCommand>(), g_ClearKnockCountdownCommandVTable);
    case 120:
        return MakeCommand(NewScriptObject<PhysicsResetVelocityCommand>(), g_PhysicsResetVelocityCommandVTable);
    case 121:
        return MakeCommand(NewScriptObject<SetFocusPositionBesidePlayerCommand>(), g_SetFocusPositionBesidePlayerCommandVTable);
    case 122:
        return MakeCommand(NewScriptObject<CopyDesignatorCommand>(), g_CopyDesignatorCommandVTable);
    case 123:
        return MakeCommand(NewScriptObject<LinkToNearestPointCommand>(), g_LinkToNearestPointCommandVTable);
    case 124:
        return MakeCommand(NewScriptObject<RunScriptSlotCommand>(), g_RunScriptSlotCommandVTable);
    case 125:
        return MakeCommand(NewScriptObject<OffsetFocusPositionCommand>(), g_OffsetFocusPositionCommandVTable);
    case 126:
        return MakeCommand(NewScriptObject<NextLinkedObjectCommand>(), g_NextLinkedObjectCommandVTable);
    case 127:
        return MakeCommand(NewScriptObject<PhysicsSetGravityCommand>(), g_PhysicsSetGravityCommandVTable);
    case 128:
        return MakeCommand(NewScriptObject<PhysicsBodyResetCommand>(), g_PhysicsBodyResetCommandVTable);
    case 129:
        return MakeCommand(NewScriptObject<PhysicsBodyActivateCommand>(), g_PhysicsBodyActivateCommandVTable);
    case 130:
        return MakeCommand(NewScriptObject<SetMagnetCommand>(), g_SetMagnetCommandVTable);
    case 131:
        return MakeCommand(NewScriptObject<MagnetPullToFocusCommand>(), g_MagnetPullToFocusCommandVTable);
    case 132:
        return MakeCommand(NewScriptObject<SetLinkedObjectIndexCommand>(), g_SetLinkedObjectIndexCommandVTable);
    case 133:
        return MakeCommand(NewScriptObject<ClearObjectContextTargetCommand>(), g_ClearObjectContextTargetCommandVTable);
    case 134:
        return MakeCommand(NewScriptObject<SetFocusToOriginatorCommand>(), g_SetFocusToOriginatorCommandVTable);
    case 135:
        return MakeCommand(NewScriptObject<DestroyPerceptionsCommand>(), g_DestroyPerceptionsCommandVTable);
    case 136:
        return MakeCommand(NewScriptObject<SetPerceptionWeightCommand>(), g_SetPerceptionWeightCommandVTable);
    case 137:
        return MakeCommand(NewScriptObject<AddPerceptionWeightCommand>(), g_AddPerceptionWeightCommandVTable);
    case 138:
        return MakeCommand(NewScriptObject<PushFromPerceptionCommand>(), g_PushFromPerceptionCommandVTable);
    case 139:
        return MakeCommand(NewScriptObject<SetPresenceCommand>(), g_SetPresenceCommandVTable);
    case 140:
        return MakeCommand(NewScriptObject<AddPresenceCommand>(), g_AddPresenceCommandVTable);
    case 141:
        return MakeCommand(NewScriptObject<TurnSenseOffCommand>(), g_TurnSenseOffCommandVTable);
    case 142:
        return MakeCommand(NewScriptObject<TurnSenseOnCommand>(), g_TurnSenseOnCommandVTable);
    case 143:
        return MakeCommand(NewScriptObject<DisableAllPerceptionsCommand>(), g_DisableAllPerceptionsCommandVTable);
    case 144:
        return MakeCommand(NewScriptObject<EnableAllPerceptionsCommand>(), g_EnableAllPerceptionsCommandVTable);
    case 145:
        return MakeCommand(NewScriptObject<SetParentExecutionValueCommand>(), g_SetParentExecutionValueCommandVTable);
    case 146:
        return MakeCommand(NewScriptObject<SetFocusToLinkedObjectCommand>(), g_SetFocusToLinkedObjectCommandVTable);
    case 147:
        return MakeCommand(NewScriptObject<PreviousKeyCommand>(), g_PreviousKeyCommandVTable);
    case 148:
        return MakeCommand(NewScriptObject<SetCycleAmplitudesCommand>(), g_SetCycleAmplitudesCommandVTable);
    case 149:
        return MakeCommand(NewScriptObject<RotateWithLinkedCommand>(), g_RotateWithLinkedCommandVTable);
    case 150:
        return MakeCommand(NewScriptObject<StrafeTowardsTargetCommand>(), g_StrafeTowardsTargetCommandVTable);
    case 151:
        return MakeCommand(NewScriptObject<PreviousRouteNodeCommand>(), g_PreviousRouteNodeCommandVTable);
    case 152:
        return MakeCommand(NewScriptObject<AddWobblePhaseCommand>(), g_AddWobblePhaseCommandVTable);
    case 153:
        return MakeCommand(NewScriptObject<MoveTowardsDesignatorCommand>(), g_MoveTowardsDesignatorCommandVTable);
    case 154:
        return MakeCommand(NewScriptObject<SetFocusToLinkedObjectCommand>(), g_SetFocusToLinkedObjectCommandVTable);
    case 155:
        return MakeCommand(NewScriptObject<SetFocusToLinkedObjectCommand>(), g_SetFocusToLinkedObjectCommandVTable);
    case 156:
        return MakeCommand(NewScriptObject<SetNoiseMessageCommand>(), g_SetNoiseMessageCommandVTable);
    case 157:
        return MakeCommand(NewScriptObject<UnlinkTargetCommand>(), g_UnlinkTargetCommandVTable);
    case 158:
        return MakeCommand(NewScriptObject<AttachMotionBlockCommand>(), g_AttachMotionBlockCommandVTable);
    case 159:
        return MakeCommand(NewScriptObject<ClearTouchMessageCommand>(), g_ClearTouchMessageCommandVTable);
    case 160:
        return MakeCommand(NewScriptObject<StopTouchMessagesCommand>(), g_StopTouchMessagesCommandVTable);
    case 161:
        return MakeCommand(NewScriptObject<SendTouchMessagesCommand>(), g_SendTouchMessagesCommandVTable);
    case 162:
        return MakeCommand(NewScriptObject<AddToFocusCounterCommand>(), g_AddToFocusCounterCommandVTable);
    case 163:
        return MakeCommand(NewScriptObject<UnlinkFromTargetCommand>(), g_UnlinkFromTargetCommandVTable);
    case 164:
        return MakeCommand(NewScriptObject<AddToLinkedCounterCommand>(), g_AddToLinkedCounterCommandVTable);
    case 165:
        return MakeCommand(NewScriptObject<ForceVolumeControllerCommand>(), g_ForceVolumeControllerCommandVTable);
    case 166:
        return MakeCommand(NewScriptObject<NotifyInstancesWithinCommand>(), g_NotifyInstancesWithinCommandVTable);
    case 167:
        return MakeCommand(NewScriptObject<SetSurfaceCommand>(), g_SetSurfaceCommandVTable);
    case 168:
        return MakeCommand(NewScriptObject<PushInstancesAwayCommand>(), g_PushInstancesAwayCommandVTable);
    case 169:
        return MakeCommand(NewScriptObject<SetFocusPositionAlongCommand>(), g_SetFocusPositionAlongCommandVTable);
    case 170:
        return MakeCommand(NewScriptObject<SetFocusCounterCommand>(), g_SetFocusCounterCommandVTable);
    case 171:
        return MakeCommand(NewScriptObject<RunSlotBehaviourOnLinkedCommand>(), g_RunSlotBehaviourOnLinkedCommandVTable);
    case 172:
        return MakeCommand(NewScriptObject<StopTargetBehaviourCommand>(), g_StopTargetBehaviourCommandVTable);
    case 173:
        return MakeCommand(NewScriptObject<SetPathIndexCommand>(), g_SetPathIndexCommandVTable);
    case 174:
        return MakeCommand(NewScriptObject<SetFocusToOwnerCommand>(), g_SetFocusToOwnerCommandVTable);
    case 175:
        return MakeCommand(NewScriptObject<SetAgentRef1ToOwnerCommand>(), g_SetAgentRef1ToOwnerCommandVTable);
    case 176:
        return MakeCommand(NewScriptObject<FadeOutMusicSlotCommand>(), g_FadeOutMusicSlotCommandVTable);
    case 177:
        return MakeCommand(NewScriptObject<WarpAgentCommand>(), g_WarpAgentCommandVTable);
    case 178:
        return MakeCommand(NewScriptObject<RotateAgentCommand>(), g_RotateAgentCommandVTable);
    case 179:
        return MakeCommand(NewScriptObject<WarpAgentCommand>(), g_WarpAgentCommandVTable);
    case 180:
        return MakeCommand(NewScriptObject<QueueObjectVideoCommand>(), g_QueueObjectVideoCommandVTable);
    case 181:
        return MakeCommand(NewScriptObject<StartObjectVideoCommand>(), g_StartObjectVideoCommandVTable);
    case 182:
        return MakeCommand(NewScriptObject<CancelVideoCommand>(), g_CancelVideoCommandVTable);
    case 183:
        return MakeCommand(NewScriptObject<SetTargetOwnerToSelfCommand>(), g_SetTargetOwnerToSelfCommandVTable);
    case 184:
        return MakeCommand(NewScriptObject<RestoreOwnObjectCommand>(), g_RestoreOwnObjectCommandVTable);
    case 185:
        return MakeCommand(NewScriptObject<QueueVideoCommand>(), g_QueueVideoCommandVTable);
    case 186:
        return MakeCommand(NewScriptObject<StartQueuedVideoCommand>(), g_StartQueuedVideoCommandVTable);
    case 187:
        return MakeCommand(NewScriptObject<SetShadowCommand>(), g_SetShadowCommandVTable);
    case 188:
        return MakeCommand(NewScriptObject<SetShadowCircleCommand>(), g_SetShadowCircleCommandVTable);
    case 189:
        return MakeCommand(NewScriptObject<SetShadowMeshCommand>(), g_SetShadowMeshCommandVTable);
    case 190:
        return MakeCommand(NewScriptObject<SetShadowRectangleCommand>(), g_SetShadowRectangleCommandVTable);
    case 191:
        return MakeCommand(NewScriptObject<SetShadowSlotCommand>(), g_SetShadowSlotCommandVTable);
    case 192:
        return MakeCommand(NewScriptObject<ClearShadowSlotCommand>(), g_ClearShadowSlotCommandVTable);
    case 193:
        return MakeCommand(NewScriptObject<LaunchAtTargetCommand>(), g_LaunchAtTargetCommandVTable);
    case 194:
        return MakeCommand(NewScriptObject<SetContactSoundsCommand>(), g_SetContactSoundsCommandVTable);
    case 195:
        return MakeCommand(NewScriptObject<StopVideoCommand>(), g_StopVideoCommandVTable);
    case 196:
        return MakeCommand(NewScriptObject<StopSoundCommand>(), g_StopSoundCommandVTable);
    case 197:
        return MakeCommand(NewScriptObject<NoOp197Command>(), g_NoOp197CommandVTable);
    case 198:
        return MakeCommand(NewScriptObject<SetCollisionBoxSizeCommand>(), g_SetCollisionBoxSizeCommandVTable);
    case 199:
        return MakeCommand(NewScriptObject<NextLinkedObjectInListCommand>(), g_NextLinkedObjectInListCommandVTable);
    case 200:
        return MakeCommand(NewScriptObject<PickLinkedObjectNearPlayerCommand>(), g_PickLinkedObjectNearPlayerCommandVTable);
    case 201:
        return MakeCommand(NewScriptObject<PickLinkedObjectNearPlayerCommand>(), g_PickLinkedObjectNearPlayerCommandVTable);
    case 202:
        return MakeCommand(NewScriptObject<TriggerInstanceAtOwnBoxCommand>(), g_TriggerInstanceAtOwnBoxCommandVTable);
    case 203:
        return MakeCommand(NewScriptObject<SetBodyMassCommand>(), g_SetBodyMassCommandVTable);
    case 204:
        return MakeCommand(NewScriptObject<SetFocusPositionOffsetCommand>(), g_SetFocusPositionOffsetCommandVTable);
    case 205:
        return MakeCommand(NewScriptObject<SetStoredPositionAtAngleCommand>(), g_SetStoredPositionAtAngleCommandVTable);
    case 206:
        return MakeCommand(NewScriptObject<SaveScriptStateCommand>(), g_SaveScriptStateCommandVTable);
    case 207:
        return MakeCommand(NewScriptObject<ClearSavedScriptStateCommand>(), g_ClearSavedScriptStateCommandVTable);
    case 208:
        return MakeCommand(NewScriptObject<SetRankCommand>(), g_SetRankCommandVTable);
    case 209:
        return MakeCommand(NewScriptObject<SetTriggerRankCommand>(), g_SetTriggerRankCommandVTable);
    case 210:
        return MakeCommand(NewScriptObject<TriggerInstancesByRankCommand>(), g_TriggerInstancesByRankCommandVTable);
    case 211:
        return MakeCommand(NewScriptObject<MarkTimeCommand>(), g_MarkTimeCommandVTable);
    case 212:
        return MakeCommand(NewScriptObject<ClearMarkedTimeCommand>(), g_ClearMarkedTimeCommandVTable);
    case 213:
        return MakeCommand(NewScriptObject<RestartRouteCommand>(), g_RestartRouteCommandVTable);
    case 214:
        return MakeCommand(NewScriptObject<ControllerRumbleCommand>(), g_ControllerRumbleCommandVTable);
    case 215:
        return MakeCommand(NewScriptObject<SetSoundParamsCommand>(), g_SetSoundParamsCommandVTable);
    case 512:
        return MakeCommand(NewScriptObject<CreateCrateContentsCommand>(), g_CreateCrateContentsCommandVTable);
    case 513:
        return MakeCommand(NewScriptObject<PickUpWumpaCommand>(), g_PickUpWumpaCommandVTable);
    case 514:
        return MakeCommand(NewScriptObject<CreateDamageCommand>(), g_CreateDamageCommandVTable);
    case 515:
        return MakeCommand(NewScriptObject<SetAgentCommand>(), g_SetAgentCommandVTable);
    case 516:
        return MakeCommand(NewScriptObject<SetPlayerRespawnPositionCommand>(), g_SetPlayerRespawnPositionCommandVTable);
    case 517:
        return MakeCommand(NewScriptObject<RestartFromCheckpointCommand>(), g_RestartFromCheckpointCommandVTable);
    case 518:
        return MakeCommand(NewScriptObject<SetCrateCommand>(), g_SetCrateCommandVTable);
    case 519:
        return MakeCommand(NewScriptObject<TriggerBalancedCrateFallingCommand>(), g_TriggerBalancedCrateFallingCommandVTable);
    case 520:
        return MakeCommand(NewScriptObject<PickUpHealthCommand>(), g_PickUpHealthCommandVTable);
    case 521:
        return MakeCommand(NewScriptObject<SetPlayerInputCommand>(), g_SetPlayerInputCommandVTable);
    case 522:
        return MakeCommand(NewScriptObject<TriggerAllNitroCratesCommand>(), g_TriggerAllNitroCratesCommandVTable);
    case 523:
        return MakeCommand(NewScriptObject<ApplyVelocityCommand>(), g_ApplyVelocityCommandVTable);
    case 524:
        return SetKeyNearestPlayerCommand::Construct(NewScriptObject<SetKeyNearestPlayerCommand>());
    case 525:
        return RaycastFocusPositionCommand::Construct(NewScriptObject<RaycastFocusPositionCommand>());
    case 526:
        return MakeCommand(NewScriptObject<ApplyVelocityToSelfCommand>(), g_ApplyVelocityToSelfCommandVTable);
    case 527:
        return MakeCommand(NewScriptObject<SetChiChiGrassCommand>(), g_SetChiChiGrassCommandVTable);
    case 528:
        return ReduceHitPointsCommand::Construct(NewScriptObject<ReduceHitPointsCommand>());
    case 529:
        return SetHitPointsCommand::Construct(NewScriptObject<SetHitPointsCommand>());
    case 530:
        return NoOpSetRayTestsCommand::Construct(NewScriptObject<NoOpSetRayTestsCommand>());
    case 531:
        return MakeCommand(NewScriptObject<RestartDefaultBehaviourCommand>(), g_RestartDefaultBehaviourCommandVTable);
    case 532:
        return MakeCommand(NewScriptObject<NoOpNowGoForwardCollidableCommand>(), g_NoOpNowGoForwardCollidableCommandVTable);
    case 533:
        return MakeCommand(NewScriptObject<NowGoBackCollidableCommand>(), g_NowGoBackCollidableCommandVTable);
    case 534:
        return MakeCommand(NewScriptObject<SetPlayAreaCommand>(), g_SetPlayAreaCommandVTable);
    case 535:
        return MakeCommand(NewScriptObject<AddCrystalCommand>(), g_AddCrystalCommandVTable);
    case 536:
        return MakeCommand(NewScriptObject<NoOp536Command>(), g_NoOp536CommandVTable);
    case 537:
        return MakeCommand(NewScriptObject<AddGemCommand>(), g_AddGemCommandVTable);
    case 538:
        return MakeCommand(NewScriptObject<NoOp538Command>(), g_NoOp538CommandVTable);
    case 539:
        return SetCustomPickupCommand::Construct(NewScriptObject<SetCustomPickupCommand>());
    case 540:
        return MakeCommand(NewScriptObject<SetCustomProjectileCommand>(), g_SetCustomProjectileCommandVTable);
    case 541:
        return MakeCommand(NewScriptObject<AddTrailCommand>(), g_AddTrailCommandVTable);
    case 542:
        return MakeCommand(NewScriptObject<ClearTrailCommand>(), g_ClearTrailCommandVTable);
    case 543:
        return DoAnimationCommand::Construct(NewScriptObject<DoAnimationCommand>());
    case 544:
        return MakeCommand(NewScriptObject<DoParticleCommand>(), g_DoParticleCommandVTable);
    case 545:
        return MakeCommand(NewScriptObject<DoSoundCommand>(), g_DoSoundCommandVTable);
    case 546:
        return MakeCommand(NewScriptObject<CreateDamageCommand>(), g_CreateDamageCommandVTable);
    case 548:
        return MakeCommand(NewScriptObject<ShootCommand>(), g_ShootCommandVTable);
    case 549:
        return GetShortRouteCommand::Construct(NewScriptObject<GetShortRouteCommand>());
    case 550:
        return MakeCommand(NewScriptObject<NoOpFuelPayGateCommand>(), g_NoOpFuelPayGateCommandVTable);
    case 551:
        return MakeCommand(NewScriptObject<OpenAllLinkedFurnitureCommand>(), g_OpenAllLinkedFurnitureCommandVTable);
    case 552:
        return MakeCommand(NewScriptObject<CloseAllLinkedFurnitureCommand>(), g_CloseAllLinkedFurnitureCommandVTable);
    case 553:
        return MakeCommand(NewScriptObject<AttachAllLinkedAgentsCommand>(), g_AttachAllLinkedAgentsCommandVTable);
    case 554:
        return MakeCommand(NewScriptObject<DetachAllLinkedAgentsCommand>(), g_DetachAllLinkedAgentsCommandVTable);
    case 555:
        return MakeCommand(NewScriptObject<SetVehicleHumiliskateCommand>(), g_SetVehicleHumiliskateCommandVTable);
    case 556:
        return MakeCommand(NewScriptObject<SetFocusToPlayerCommand>(), g_SetFocusToPlayerCommandVTable);
    case 557:
        return RequestFocusCommand::Construct(NewScriptObject<RequestFocusCommand>(), 0);
    case 558:
        return MakeCommand(NewScriptObject<SetFocusPropertiesCommand>(), g_SetFocusPropertiesCommandVTable);
    case 559:
        return MakeCommand(NewScriptObject<SetCameraCommand>(), g_SetCameraCommandVTable);
    case 560:
        return MakeCommand(NewScriptObject<RestoreCameraDefaultsCommand>(), g_RestoreCameraDefaultsCommandVTable);
    case 561:
        return MakeCommand(NewScriptObject<LinkToFocusCharacterCommand>(), g_LinkToFocusCharacterCommandVTable);
    case 562:
        return MakeCommand(NewScriptObject<UnlinkCharactersCommand>(), g_UnlinkCharactersCommandVTable);
    case 563:
        return MakeCommand(NewScriptObject<DamageOriginatorCommand>(), g_DamageOriginatorCommandVTable);
    case 564:
        return RequestFocusCommand::Construct(NewScriptObject<RequestFocusCommand>(), 1);
    case 565:
        return RequestFocusCommand::Construct(NewScriptObject<RequestFocusCommand>(), 2);
    case 566:
        return MakeCommand(NewScriptObject<SetAgentRef1ToPlayerCommand>(), g_SetAgentRef1ToPlayerCommandVTable);
    case 567:
        return MakeCommand(NewScriptObject<SetFocusPositionToPlayerCommand>(), g_SetFocusPositionToPlayerCommandVTable);
    case 568:
        return MakeCommand(NewScriptObject<NoOp568Command>(), g_NoOp568CommandVTable);
    case 569:
        return MakeCommand(NewScriptObject<ExitVehicleModeCommand>(), g_ExitVehicleModeCommandVTable);
    case 570:
        return MakeCommand(NewScriptObject<SetVehicleRollerbrawlCommand>(), g_SetVehicleRollerbrawlCommandVTable);
    case 571:
        return MakeCommand(NewScriptObject<SetVehicleHoverboardCommand>(), g_SetVehicleHoverboardCommandVTable);
    case 572:
        return MakeCommand(NewScriptObject<SetMotionCommand>(), g_SetMotionCommandVTable);
    case 573:
        return MakeCommand(NewScriptObject<SetNearestPointFlagsCommand>(), g_SetNearestPointFlagsCommandVTable);
    case 574:
        return MakeCommand(NewScriptObject<CreateHeadTrackingCommand>(), g_CreateHeadTrackingCommandVTable);
    case 575:
        return MakeCommand(NewScriptObject<SetFocusPositionToNearestPointCommand>(), g_SetFocusPositionToNearestPointCommandVTable);
    case 576:
        return MakeCommand(NewScriptObject<SetFocusToGameActorCommand>(), g_SetFocusToGameActorCommandVTable);
    case 577:
        return MakeCommand(NewScriptObject<BecomeStickyCommand>(), g_BecomeStickyCommandVTable);
    case 578:
        return MakeCommand(NewScriptObject<CountPlayerCirclingCommand>(), g_CountPlayerCirclingCommandVTable);
    case 579:
        return MakeCommand(NewScriptObject<CountPlayerApproachCommand>(), g_CountPlayerApproachCommandVTable);
    case 580:
        return MakeCommand(NewScriptObject<ApplyVelocityToHeldBodyCommand>(), g_ApplyVelocityToHeldBodyCommandVTable);
    case 581:
        return MakeCommand(NewScriptObject<BecomeNormalCommand>(), g_BecomeNormalCommandVTable);
    case 582:
        return MakeCommand(NewScriptObject<AddPerceptionCommand>(), g_AddPerceptionCommandVTable);
    case 583:
        return MakeCommand(NewScriptObject<SwingAroundCameraCommand>(), g_SwingAroundCameraCommandVTable);
    case 584:
        return MakeCommand(NewScriptObject<NoOp584Command>(), g_NoOp584CommandVTable);
    case 585:
        return MakeCommand(NewScriptObject<AddLivesCommand>(), g_AddLivesCommandVTable);
    case 586:
        return MakeCommand(NewScriptObject<NoOp586Command>(), g_NoOp586CommandVTable);
    case 587:
        return MakeCommand(NewScriptObject<SetAttacksTakenCommand>(), g_SetAttacksTakenCommandVTable);
    case 588:
        return MakeCommand(NewScriptObject<PlayerFaceTowardsCameraCommand>(), g_PlayerFaceTowardsCameraCommandVTable);
    case 589:
        return MakeCommand(NewScriptObject<CutsceneStartCommand>(), g_CutsceneStartCommandVTable);
    case 590:
        return MakeCommand(NewScriptObject<CutsceneEndCommand>(), g_CutsceneEndCommandVTable);
    case 591:
        return MakeCommand(NewScriptObject<CutsceneCameraMoveCommand>(), g_CutsceneCameraMoveCommandVTable);
    case 592:
        return MakeCommand(NewScriptObject<CameraSaveParamsCommand>(), g_CameraSaveParamsCommandVTable);
    case 593:
        return MakeCommand(NewScriptObject<CameraSaveParamsCommand>(), g_CameraSaveParamsCommandVTable);
    case 594:
        return MakeCommand(NewScriptObject<ToggleCutsceneCameraCommand>(), g_ToggleCutsceneCameraCommandVTable);
    case 595:
        return MakeCommand(NewScriptObject<CutsceneCameraTargetsCommand>(), g_CutsceneCameraTargetsCommandVTable);
    case 596:
        return MakeCommand(NewScriptObject<StartWhackawormCommand>(), g_StartWhackawormCommandVTable);
    case 597:
        return MakeCommand(NewScriptObject<ProgressWhackawormCommand>(), g_ProgressWhackawormCommandVTable);
    case 598:
        return MakeCommand(NewScriptObject<EndWhackawormCommand>(), g_EndWhackawormCommandVTable);
    case 599:
        return MakeCommand(NewScriptObject<ReleasePlayerHoldCommand>(), g_ReleasePlayerHoldCommandVTable);
    case 600:
        return MakeCommand(NewScriptObject<WarpToChunkLinkTowardsPlayerCommand>(), g_WarpToChunkLinkTowardsPlayerCommandVTable);
    case 601:
        return MakeCommand(NewScriptObject<SetVehicleWrestleCreatureCommand>(), g_SetVehicleWrestleCreatureCommandVTable);
    case 602:
        return MakeCommand(NewScriptObject<FadeoutScreenCommand>(), g_FadeoutScreenCommandVTable);
    case 603:
        return MakeCommand(NewScriptObject<DisplayBottomTextCommand>(), g_DisplayBottomTextCommandVTable);
    case 604:
        return MakeCommand(NewScriptObject<ResetCharacterFallCommand>(), g_ResetCharacterFallCommandVTable);
    case 605:
        return MakeCommand(NewScriptObject<DismissCharacterCommand>(), g_DismissCharacterCommandVTable);
    case 606:
        return MakeCommand(NewScriptObject<CameraFocusObjectCommand>(), g_CameraFocusObjectCommandVTable);
    case 607:
        return MakeCommand(NewScriptObject<CameraStopFocusObjectCommand>(), g_CameraStopFocusObjectCommandVTable);
    case 608:
        return MakeCommand(NewScriptObject<ClearBottomTextCommand>(), g_ClearBottomTextCommandVTable);
    case 609:
        return MakeCommand(NewScriptObject<NoOp609Command>(), g_NoOp609CommandVTable);
    case 610:
        return MakeCommand(NewScriptObject<NoOp610Command>(), g_NoOp610CommandVTable);
    case 611:
        return MakeCommand(NewScriptObject<SetCharacterHomeChunkCommand>(), g_SetCharacterHomeChunkCommandVTable);
    case 612:
        return MakeCommand(NewScriptObject<EnablePlayerControlCommand>(), g_EnablePlayerControlCommandVTable);
    case 613:
        return MakeCommand(NewScriptObject<DisablePlayerControlCommand>(), g_DisablePlayerControlCommandVTable);
    case 614:
        return MakeCommand(NewScriptObject<StopStickingCommand>(), g_StopStickingCommandVTable);
    case 615:
        return MakeCommand(NewScriptObject<SetPlayerScriptFlagCommand>(), g_SetPlayerScriptFlagCommandVTable);
    case 616:
        return MakeCommand(NewScriptObject<PlaceCharacterInChunkCommand>(), g_PlaceCharacterInChunkCommandVTable);
    case 617:
        return MakeCommand(NewScriptObject<HitInstancesInBoxesCommand>(), g_HitInstancesInBoxesCommandVTable);
    case 618:
        return MakeCommand(NewScriptObject<ForceGameOverCommand>(), g_ForceGameOverCommandVTable);
    case 619:
        return MakeCommand(NewScriptObject<ShowBottomTextCommand>(), g_ShowBottomTextCommandVTable);
    case 620:
        return MakeCommand(NewScriptObject<HideBottomTextCommand>(), g_HideBottomTextCommandVTable);
    case 621:
        return MakeCommand(NewScriptObject<SetFocusToCameraTargetCommand>(), g_SetFocusToCameraTargetCommandVTable);
    case 622:
        return MakeCommand(NewScriptObject<CharacterSoundProxyCommand>(), g_CharacterSoundProxyCommandVTable);
    case 623:
        return MakeCommand(NewScriptObject<SetFollowCameraTargetCommand>(), g_SetFollowCameraTargetCommandVTable);
    case 624:
        return MakeCommand(NewScriptObject<SetScriptGlobalFlagCommand>(), g_SetScriptGlobalFlagCommandVTable);
    case 625:
        return MakeCommand(NewScriptObject<SwitchCharacterCommand>(), g_SwitchCharacterCommandVTable);
    case 626:
        return MakeCommand(NewScriptObject<UseOwnFollowCamerasCommand>(), g_UseOwnFollowCamerasCommandVTable);
    case 627:
        return MakeCommand(NewScriptObject<UseTriggerCamerasCommand>(), g_UseTriggerCamerasCommandVTable);
    case 628:
        return MakeCommand(NewScriptObject<SetFollowCameraRateCommand>(), g_SetFollowCameraRateCommandVTable);
    case 629:
        return MakeCommand(NewScriptObject<SetCameraNodeValuesCommand>(), g_SetCameraNodeValuesCommandVTable);
    case 630:
        return MakeCommand(NewScriptObject<SwitchBodyFlagsCommand>(), g_SwitchBodyFlagsCommandVTable);
    case 631:
        return MakeCommand(NewScriptObject<PushPlayerVehicleCommand>(), g_PushPlayerVehicleCommandVTable);
    case 632:
        return MakeCommand(NewScriptObject<SetLinkedObjectNearestPlayerCommand>(), g_SetLinkedObjectNearestPlayerCommandVTable);
    case 633:
        return MakeCommand(NewScriptObject<SetPlayerModeCommand>(), g_SetPlayerModeCommandVTable);
    case 634:
        return MakeCommand(NewScriptObject<PlayMovieCommand>(), g_PlayMovieCommandVTable);
    case 636:
        return MakeCommand(NewScriptObject<AddAmmoCommand>(), g_AddAmmoCommandVTable);
    case 637:
        return MakeCommand(NewScriptObject<SetFocusToLinkedObjectInViewCommand>(), g_SetFocusToLinkedObjectInViewCommandVTable);
    case 638:
        return MakeCommand(NewScriptObject<EnableBossModeCommand>(), g_EnableBossModeCommandVTable);
    case 639:
        return MakeCommand(NewScriptObject<DamageBossCommand>(), g_DamageBossCommandVTable);
    case 640:
        return MakeCommand(NewScriptObject<ExitBossModeCommand>(), g_ExitBossModeCommandVTable);
    case 641:
        return MakeCommand(NewScriptObject<FinalBossWeaponsCommand>(), g_FinalBossWeaponsCommandVTable);
    case 642:
        return MakeCommand(NewScriptObject<FinalBossWeaponsCommand>(), g_FinalBossWeaponsCommandVTable);
    case 643:
        return MakeCommand(NewScriptObject<FinalBossWeaponsCommand>(), g_FinalBossWeaponsCommandVTable);
    case 644:
        return MakeCommand(NewScriptObject<FinalBossWeaponsCommand>(), g_FinalBossWeaponsCommandVTable);
    case 645:
        return MakeCommand(NewScriptObject<CreateNodeControllerCommand>(), g_CreateNodeControllerCommandVTable);
    case 646:
        return MakeCommand(NewScriptObject<SetGaugeIconCommand>(), g_SetGaugeIconCommandVTable);
    case 647:
        return MakeCommand(NewScriptObject<RaiseStoryAreaCommand>(), g_RaiseStoryAreaCommandVTable);
    case 648:
        return MakeCommand(NewScriptObject<SetCountedValueCommand>(), g_SetCountedValueCommandVTable);
    case 649:
        return MakeCommand(NewScriptObject<ClearCountedValueCommand>(), g_ClearCountedValueCommandVTable);
    case 650:
        return MakeCommand(NewScriptObject<HoldVehicleCommand>(), g_HoldVehicleCommandVTable);
    case 651:
        return MakeCommand(NewScriptObject<ReleaseVehicleCommand>(), g_ReleaseVehicleCommandVTable);
    case 652:
        return MakeCommand(NewScriptObject<CameraTopdownModeCommand>(), g_CameraTopdownModeCommandVTable);
    case 653:
        return MakeCommand(NewScriptObject<FinalBossWeaponsCommand>(), g_FinalBossWeaponsCommandVTable);
    case 654:
        return MakeCommand(NewScriptObject<PlayCreditsCommand>(), g_PlayCreditsCommandVTable);
    case 655:
        return MakeCommand(NewScriptObject<SetMaskControllerIdsCommand>(), g_SetMaskControllerIdsCommandVTable);
    case 656:
        return MakeCommand(NewScriptObject<ResetMaskControllerCommand>(), g_ResetMaskControllerCommandVTable);
    case 657:
        return MakeCommand(NewScriptObject<DisplayBottomTextInstanceCommand>(), g_DisplayBottomTextInstanceCommandVTable);
    case 658:
        return MakeCommand(NewScriptObject<SetSplineControllerValuesCommand>(), g_SetSplineControllerValuesCommandVTable);
    case 659:
        return MakeCommand(NewScriptObject<MakeCharactersIdleCommand>(), g_MakeCharactersIdleCommandVTable);
    case 660:
        return MakeCommand(NewScriptObject<ClearCharacterDeadCommand>(), g_ClearCharacterDeadCommandVTable);
    case 661:
        return MakeCommand(NewScriptObject<SetSkateControllerIdsCommand>(), g_SetSkateControllerIdsCommandVTable);
    case 662:
        return MakeCommand(NewScriptObject<ResetCameraCommand>(), g_ResetCameraCommandVTable);
    default:
        return nullptr;
    }
}

// Their members (tagged values, IDs and the parts' arguments) have destructors that free nothing
void PushInstancesAwayCommand::Destroy(u32 destroyFlags)
{
    vtable = g_PushInstancesAwayCommandVTable;
    ScriptCommand::Destroy(destroyFlags);
}

void DoSoundCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetWobbleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NowMoveForwardsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AttachMotionBlockCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOpNowMoveBackwardsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddToFocusCounterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddToLinkedCounterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NotifyInstancesWithinCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NowStrafeLeftCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetFocusCounterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NowStrafeRightCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void QueueVideoCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NowTurnLeftCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddTrailCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetRankCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetTriggerRankCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCollisionsCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NoOpNowGoForwardCollidableCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void NowGoBackCollidableCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetPlayAreaCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCustomProjectileCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCameraCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ClearAnimationCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetMotionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void CreateHeadTrackingCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddNoiseToFocusPositionCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void PlayMovieCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void EnableBossModeCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void RaiseStoryAreaCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCounterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ModifyCounterCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetContactSpringyCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void BeginMusicCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ThrowAttachedObjectCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SendUserMessageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetVehicleHumiliskateCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void StartWhackawormCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void LaunchAgentRef2Command::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetCycleAmplitudesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AddWobblePhaseCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ForceVolumeControllerCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetShadowCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void DamageOriginatorCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void AlterWobblePhaseCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void DoAnimationCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetShadowMeshCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void CreateDamageCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetShadowCircleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetShadowRectangleCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void GetShortRouteCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void RequestFocusCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ApplyVelocityToSelfCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void SetSplineControllerValuesCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}

void ApplyVelocityCommand::Destroy(u32 destroyFlags)
{
    ScriptCommand::Destroy(destroyFlags);
}
