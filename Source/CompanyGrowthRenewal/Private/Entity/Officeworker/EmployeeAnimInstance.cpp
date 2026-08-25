// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Officeworker/EmployeeAnimInstance.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDebugWorkerState(
	TEXT("cg.DebugWorkerState"), 0,
	TEXT("1이면 직원별 상태를 화면에 디버그 표시 (개발 빌드 전용)"), ECVF_Default);
#endif

void UEmployeeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Owner 캐싱 (첫 호출 시 또는 무효화 시)
	if (!CachedOwner.IsValid())
	{
		if (AActor* OwnerActor = TryGetPawnOwner())
		{
			CachedOwner = Cast<AOfficeworker>(OwnerActor);
		}
	}

	// Owner가 유효하지 않으면 리턴
	if (!CachedOwner.IsValid())
	{
		return;
	}

	// BehaviorComponent 캐싱
	if (!CachedBehaviorComponent.IsValid())
	{
		CachedBehaviorComponent = CachedOwner->BehaviorComponent;
	}

	// BehaviorComponent가 유효하면 상태 동기화
	if (CachedBehaviorComponent.IsValid())
	{
		CurrentBehaviorMode = CachedBehaviorComponent->CurrentBehaviorMode;
		EmployeeState = CachedBehaviorComponent->EmployeeState;
		DanceIndex = CachedBehaviorComponent->CurrentDanceIndex;
		SeatedPoseIndex = CachedBehaviorComponent->SeatedPoseIndex;
		bIsSeated = CachedBehaviorComponent->bIsSeated;

		// 실제 캐릭터 속도 기반으로 Walking과 Speed 계산
		if (CachedOwner.IsValid())
		{
			FVector Velocity = CachedOwner->GetVelocity();
			Speed = Velocity.Size2D();  // XY 평면 속도 (cm/s)
			bWalking = Speed > 1.0f;    // 1 이상이면 걷는 중
		}
		else
		{
			bWalking = CachedBehaviorComponent->Walking;
			Speed = CachedBehaviorComponent->Speed;
		}

#if !UE_BUILD_SHIPPING
		// 디버그: 모든 직원 상태 화면에 표시 (cg.DebugWorkerState 1일 때만)
		if (CVarDebugWorkerState.GetValueOnAnyThread() != 0 && GEngine && CachedOwner.IsValid() && !CachedOwner->bIsPortraitMode)
		{
			FString StateStr;
			switch (EmployeeState)
			{
			case EEmployeeState::Typing: StateStr = TEXT("Typing"); break;
			case EEmployeeState::Sitting: StateStr = TEXT("Sitting"); break;
			case EEmployeeState::Wander: StateStr = TEXT("Wander"); break;
			case EEmployeeState::CheerSitting: StateStr = TEXT("CheerSitting"); break;
			case EEmployeeState::CheerStandUp: StateStr = TEXT("CheerStandUp"); break;
			case EEmployeeState::TypeToSit: StateStr = TEXT("TypeToSit"); break;
			case EEmployeeState::SitToStand: StateStr = TEXT("SitToStand"); break;
			case EEmployeeState::StandToSit: StateStr = TEXT("StandToSit"); break;
			case EEmployeeState::Greeting: StateStr = TEXT("Greeting"); break;
			case EEmployeeState::Dance: StateStr = FString::Printf(TEXT("Dance(%d)"), DanceIndex); break;
			default: StateStr = TEXT("Unknown"); break;
			}

			// 각 직원마다 고유 ID로 메시지 표시 (100 + EmployeeID)
			int32 EmpID = CachedOwner->GetEmployeeID();
			int32 MsgKey = 100 + EmpID;
			GEngine->AddOnScreenDebugMessage(
				MsgKey,
				0.0f,
				FColor::Yellow,
				FString::Printf(TEXT("[Employee %d] %s"), EmpID, *StateStr)
			);
		}
#endif
	}
	else
	{
		// BehaviorComponent가 없으면 기본값 유지
		CurrentBehaviorMode = EEmployeeBehaviorMode::Idle;
		EmployeeState = EEmployeeState::Sitting;
		bIsSeated = true;
		bWalking = false;
		Speed = 0.0f;
	}
}
