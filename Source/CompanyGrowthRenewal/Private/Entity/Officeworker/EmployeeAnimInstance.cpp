// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Officeworker/EmployeeAnimInstance.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Office/SingleWorkstationActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}

	// 위상은 소유자가 잡힌 뒤 한 번만 계산한다. 곱셈 해시로 섞지 않으면 EmployeeID 가 연번이라 값이 뭉친다.
	if (!bAnimPhaseInitialized)
	{
		const int32 Id = CachedOwner->GetEmployeeID();
		if (Id >= 0)
		{
			uint32 Hash = static_cast<uint32>(Id) * 2654435761u;
			Hash ^= Hash >> 15;
			AnimPhase01 = static_cast<float>(Hash & 0xFFFFu) / 65535.0f;
			AnimRateJitter = 0.92f + static_cast<float>((Hash >> 16) & 0xFFu) / 255.0f * 0.16f;
			bAnimPhaseInitialized = true;
		}
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

		if (ReactionRunSpeed >= 0.0f)
		{
			EmployeeState = EEmployeeState::Wander;
			bIsSeated = false;
			bWalking = true;
			Speed = ReactionRunSpeed;
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

	UpdateTypingHandTargets(DeltaSeconds);
}

void UEmployeeAnimInstance::UpdateTypingHandTargets(float DeltaSeconds)
{
	if ((EmployeeState != EEmployeeState::Typing || !bIsSeated) && TypingIKAlpha <= KINDA_SMALL_NUMBER)
	{
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}
	const AOfficeworker* Worker = CachedOwner.Get();
	const USkeletalMeshComponent* WorkerMesh = GetSkelMeshComponent();
	if (!IsValid(Worker) || !IsValid(WorkerMesh) || Worker->bIsPortraitMode)
	{
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}

	const ASingleWorkstationActor* Workstation = Cast<ASingleWorkstationActor>(Worker->GetAssignedWorkstation());
	if (!IsValid(Workstation))
	{
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}

	static const FName LeftWristSocket(TEXT("TypingLeftWrist"));
	static const FName RightWristSocket(TEXT("TypingRightWrist"));
	const auto HasTypingSockets = [](const UStaticMeshComponent* Equipment)
	{
		return IsValid(Equipment) && Equipment->IsVisible() && Equipment->GetStaticMesh()
			&& Equipment->DoesSocketExist(LeftWristSocket) && Equipment->DoesSocketExist(RightWristSocket);
	};

	const UStaticMeshComponent* Equipment = HasTypingSockets(Workstation->KeyboardMesh)
		? Workstation->KeyboardMesh : Workstation->LaptopMesh_Center;
	if (!HasTypingSockets(Equipment))
	{
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}

	static const FName ContactCurve(TEXT("TypingContact"));
	const float Contact = GetCurveValue(ContactCurve);
	if (Contact <= 0.01f)
	{
		TypingIKAlpha = 0.0f;
		TypingMouseBlend = 0.0f;
		return;
	}

	const auto ReadContactCurve = [this, Contact](const TCHAR* CurveName)
	{
		return GetCurveValue(FName(CurveName)) / Contact;
	};
	const FVector LeftOffset(
		ReadContactCurve(TEXT("TypingLeftX")),
		ReadContactCurve(TEXT("TypingLeftY")),
		ReadContactCurve(TEXT("TypingLeftZ")));
	const FVector RightOffset(
		ReadContactCurve(TEXT("TypingRightX")),
		ReadContactCurve(TEXT("TypingRightY")),
		ReadContactCurve(TEXT("TypingRightZ")));
	const FRotator LeftRotationDelta(
		ReadContactCurve(TEXT("TypingLeftPitch")),
		ReadContactCurve(TEXT("TypingLeftYaw")),
		ReadContactCurve(TEXT("TypingLeftRoll")));
	const FRotator RightRotationDelta(
		ReadContactCurve(TEXT("TypingRightPitch")),
		ReadContactCurve(TEXT("TypingRightYaw")),
		ReadContactCurve(TEXT("TypingRightRoll")));
	const UStaticMeshComponent* Mouse = Workstation->MouseMesh;
	const bool bValidMouse = IsValid(Mouse) && Mouse->IsVisible() && Mouse->GetStaticMesh()
		&& Mouse->DoesSocketExist(RightWristSocket);
	TypingMouseBlend = bValidMouse
		? FMath::Clamp(ReadContactCurve(TEXT("TypingMouseWeight")), 0.0f, 1.0f) : 0.0f;
	const FTransform WorkerWorldTransform = WorkerMesh->GetComponentTransform();

	FTransform LeftTarget = Equipment->GetSocketTransform(LeftWristSocket, RTS_World);
	LeftTarget.AddToTranslation(Equipment->GetComponentTransform().TransformVector(LeftOffset));
	// 클립의 손목 회전 변화는 장비 로컬축 기준이므로 월드축으로 옮겨 소켓 회전 앞에 적용한다.
	const FQuat EquipmentRotation = Equipment->GetComponentQuat();
	LeftTarget.SetRotation(EquipmentRotation * LeftRotationDelta.Quaternion()
		* EquipmentRotation.Inverse() * LeftTarget.GetRotation());
	LeftTarget = LeftTarget.GetRelativeTransform(WorkerWorldTransform);
	TypingLeftHandLocation = LeftTarget.GetLocation();
	TypingLeftHandRotation = LeftTarget.Rotator();

	FTransform RightTarget = Equipment->GetSocketTransform(RightWristSocket, RTS_World);
	RightTarget.AddToTranslation(Equipment->GetComponentTransform().TransformVector(RightOffset));
	RightTarget.SetRotation(EquipmentRotation * RightRotationDelta.Quaternion()
		* EquipmentRotation.Inverse() * RightTarget.GetRotation());
	if (TypingMouseBlend > 0.0f)
	{
		FTransform MouseTarget = Mouse->GetSocketTransform(RightWristSocket, RTS_World);
		MouseTarget.AddToTranslation(Mouse->GetComponentTransform().TransformVector(RightOffset));
		const FQuat MouseRotation = Mouse->GetComponentQuat();
		MouseTarget.SetRotation(MouseRotation * RightRotationDelta.Quaternion()
			* MouseRotation.Inverse() * MouseTarget.GetRotation());
		const float MouseArcHeight = 4.0f * TypingMouseBlend * (1.0f - TypingMouseBlend)
			* 4.0f * FMath::Abs(Equipment->GetComponentScale().Z);
		RightTarget.SetLocation(FMath::Lerp(RightTarget.GetLocation(), MouseTarget.GetLocation(), TypingMouseBlend)
			+ FVector(0.0f, 0.0f, MouseArcHeight));
		RightTarget.SetRotation(FQuat::Slerp(RightTarget.GetRotation(), MouseTarget.GetRotation(), TypingMouseBlend));
	}
	RightTarget = RightTarget.GetRelativeTransform(WorkerWorldTransform);
	TypingRightHandLocation = RightTarget.GetLocation();
	TypingRightHandRotation = RightTarget.Rotator();

	const float TargetAlpha = EmployeeState == EEmployeeState::Typing && bIsSeated
		? FMath::Clamp(Contact, 0.0f, 1.0f) : 0.0f;
	TypingIKAlpha = FMath::FInterpTo(TypingIKAlpha, TargetAlpha, DeltaSeconds, TypingIKBlendSpeed);
}
