#include "Office/OfficeEmployeeInspect.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Office/WorkstationActorBase.h"

namespace
{
	bool IsSeatedEmployeeState(EEmployeeState State)
	{
		return State == EEmployeeState::Typing || State == EEmployeeState::Sitting
			|| State == EEmployeeState::CheerSitting;
	}
}

bool FOfficeEmployeeInspect::Begin(AOfficeworker* InWorker, const FVector& ViewLocation)
{
	if (IsValid(InWorker) && Worker.Get() == InWorker)
	{
		FaceToward(ViewLocation);
		return true;
	}
	End();

	UEmployeeBehaviorComponent* Behavior = IsValid(InWorker) ? InWorker->BehaviorComponent : nullptr;
	// 채용 입장 연출은 자기 흐름이 자리까지 데려가야 끝난다 — 중간에 세우면 입장이 끊긴다
	if (!IsValid(InWorker) || InWorker->bIsPortraitMode || (Behavior && Behavior->IsRecruitmentArrivalActive()))
	{
		return false;
	}

	Worker = InWorker;
	AWorkstationActorBase* Desk = InWorker->GetAssignedWorkstation();
	bWasSeated = Desk && Desk->FindWorkerSeatIndex(InWorker) >= 0;
	bWasRoaming = InWorker->IsRoaming();
	SavedState = Behavior ? Behavior->GetState() : EEmployeeState::Wander;
	bBehaviorTickWasEnabled = Behavior && Behavior->IsComponentTickEnabled();

	// 행동 틱이 돌면 피로·슬랙 사이클이 다음 프레임에 다시 앉히거나 걷게 만든다
	if (Behavior)
	{
		Behavior->SetComponentTickEnabled(false);
	}
	InWorker->StopRandomRoaming();
	if (AAIController* AIC = Cast<AAIController>(InWorker->GetController()))
	{
		AIC->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = InWorker->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	if (bWasSeated)
	{
		// 좌석은 NavMesh 밖이라 책상 앞 접근 지점(이미 NavMesh 투영)으로 옮겨 세운다. Z 는 EnterSlackBolt 선례대로 유지
		const FVector Approach = InWorker->GetWorkstationApproachLocation();
		InWorker->StandUpFromWorkstation();
		const FVector Current = InWorker->GetActorLocation();
		InWorker->SetActorLocation(FVector(Approach.X, Approach.Y, Current.Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
	// Typing/Sitting 이 남아 있으면 ABP 가 착석 포즈를 유지한다
	if (Behavior && (bWasSeated || IsSeatedEmployeeState(SavedState)))
	{
		Behavior->SetState(EEmployeeState::Wander);
	}

	InWorker->SetOverheadUISuppressed(true);
	FaceToward(ViewLocation);
	return true;
}

void FOfficeEmployeeInspect::End()
{
	AOfficeworker* InspectedWorker = Worker.Get();
	Worker.Reset();
	if (!IsValid(InspectedWorker))
	{
		return;
	}

	InspectedWorker->SetOverheadUISuppressed(false);
	UEmployeeBehaviorComponent* Behavior = InspectedWorker->BehaviorComponent;
	if (Behavior && bBehaviorTickWasEnabled)
	{
		Behavior->SetComponentTickEnabled(true);
	}

	if (bWasSeated && InspectedWorker->TeleportToWorkstationAndSit())
	{
		if (Behavior)
		{
			Behavior->SetState(IsSeatedEmployeeState(SavedState) ? SavedState : EEmployeeState::Typing);
		}
		return;
	}
	// 좌석이 사라졌으면(책상 이동 등) 서 있는 채로 두지 않고 배회로 보낸다
	if (bWasRoaming || bWasSeated)
	{
		InspectedWorker->ResumeRandomRoaming();
	}
}

void FOfficeEmployeeInspect::FaceToward(const FVector& ViewLocation) const
{
	AOfficeworker* InspectedWorker = Worker.Get();
	if (!InspectedWorker)
	{
		return;
	}
	const FVector ToView = (ViewLocation - InspectedWorker->GetActorLocation()).GetSafeNormal2D();
	if (ToView.IsNearlyZero())
	{
		return;
	}
	// 메시 상대 요 -90 이 자산 정면(+Y)을 액터 +X 에 맞춰 두었으므로 액터 요만 돌리면 얼굴이 향한다
	InspectedWorker->SetActorRotation(FRotator(0.f, ToView.Rotation().Yaw, 0.f));
}
