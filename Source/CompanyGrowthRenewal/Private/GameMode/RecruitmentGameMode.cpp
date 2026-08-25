// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/RecruitmentGameMode.h"
#include "Player/RecruitmentCameraPawn.h"
#include "Entity/Door/InteractiveDoorActor.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/ModularOfficeworker.h"
#include "Entity/Officeworker/OfficeworkerMale.h"
#include "Entity/Officeworker/OfficeworkerFemale.h"
#include "Entity/Officeworker/StickOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Core/CGGameInstance.h"
#include "Manager/EmployeeManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/EmployeeTypes.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/WidgetType.h"
#include "UI/Panel/RecruitmentResultPanelWidget.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "AsyncLoadingScreenLibrary.h"

ARecruitmentGameMode::ARecruitmentGameMode()
{
	DefaultPawnClass = ARecruitmentCameraPawn::StaticClass();
	HUDClass = nullptr;
	PrimaryActorTick.bCanEverTick = false;
}

void ARecruitmentGameMode::StartPlay()
{
	Super::StartPlay();

	FindDoorActor();
	FindPortraitWorkers();

	// GameInstance에서 가챠 결과 가져오기
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (GI->HasPendingGachaResult())
		{
			CurrentGachaResult = GI->GetPendingGachaResult();
			UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 가챠 결과 수신 - 등급: %d, 성별: %d"),
				static_cast<uint8>(CurrentGachaResult.PotentialRarity),
				static_cast<uint8>(CurrentGachaResult.ResultEmployee.Gender));

			// Portrait 히어로 외형 적용. 스틱 히어로(FindPortraitWorkers)가 있으면 그대로,
			// 없으면 구 모듈러 남/녀로 폴백 선택.
			if (!ActivePortraitWorker)
			{
				ActivePortraitWorker = (CurrentGachaResult.ResultEmployee.Gender == EEmployeeGender::Male)
					? static_cast<AOfficeworker*>(PortraitMale)
					: static_cast<AOfficeworker*>(PortraitFemale);
			}

			if (ActivePortraitWorker)
			{
				ApplyAppearanceToWorker(ActivePortraitWorker);
			}

			// Walking Worker 동적 스폰
			SpawnWalkingWorker();

			// Portrait 선행 촬영 (연출과 병행 — 촬영 ~1-2초, 연출 ~5초+)
			StartPreemptivePortraitCapture();

			// 시퀀스 시작
			StartGachaSequence();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RecruitmentGameMode] 대기 중인 가챠 결과가 없습니다."));
		}
	}

	UAsyncLoadingScreenLibrary::StopLoadingScreen();
}

void ARecruitmentGameMode::FindDoorActor()
{
	for (TActorIterator<AInteractiveDoorActor> It(GetWorld()); It; ++It)
	{
		DoorActor = *It;
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 문 액터 발견: %s"), *DoorActor->GetName());
		break;
	}

	if (!DoorActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitmentGameMode] 레벨에 InteractiveDoorActor가 없습니다!"));
	}
}

void ARecruitmentGameMode::FindPortraitWorkers()
{
	// 1순위: 공개용 스틱 히어로 (bIsPortraitMode==false — 캡쳐용 워커와 구분). 유니섹스라 1개면 충분.
	for (TActorIterator<AStickOfficeworker> It(GetWorld()); It; ++It)
	{
		if (!It->bIsPortraitMode)
		{
			ActivePortraitWorker = *It;
			UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] Stick 히어로 발견: %s"), *ActivePortraitWorker->GetName());
			break;
		}
	}

	// 폴백: 구 모듈러 남/녀 (스틱 히어로 미배치 시 — strangler-fig)
	for (TActorIterator<AOfficeworkerMale> It(GetWorld()); It; ++It)
	{
		PortraitMale = *It;
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] Portrait Male 발견: %s"), *PortraitMale->GetName());
		break;
	}

	for (TActorIterator<AOfficeworkerFemale> It(GetWorld()); It; ++It)
	{
		PortraitFemale = *It;
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] Portrait Female 발견: %s"), *PortraitFemale->GetName());
		break;
	}
}

void ARecruitmentGameMode::SpawnWalkingWorker()
{
	// "EmployeeSpawn" 태그의 TargetPoint 찾기
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	bool bFoundSpawnPoint = false;

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(FName("EmployeeSpawn")))
		{
			SpawnLocation = It->GetActorLocation();
			SpawnRotation = It->GetActorRotation();
			bFoundSpawnPoint = true;
			UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] EmployeeSpawn 위치: %s"), *SpawnLocation.ToString());
			break;
		}
	}

	if (!bFoundSpawnPoint)
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitmentGameMode] 'EmployeeSpawn' 태그의 TargetPoint를 찾지 못했습니다!"));
		return;
	}

	const FEmployeeInstance& Employee = CurrentGachaResult.ResultEmployee;

	// 스틱맨 BP 우선 (OfficeMap 과 동일 외형 + 코스메틱 정합). BP 미존재 시 C++ 클래스 폴백. 1회 해석 캐시.
	if (!ResolvedStickWorkerClass)
	{
		static const TSoftClassPtr<AOfficeworker> StickBPClass(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/BP_StickOfficeworker.BP_StickOfficeworker_C")));
		UClass* LoadedBP = StickBPClass.LoadSynchronous();
		ResolvedStickWorkerClass = LoadedBP ? LoadedBP : AStickOfficeworker::StaticClass();
	}
	TSubclassOf<AOfficeworker> WorkerClass = ResolvedStickWorkerClass;

	// SpawnActorDeferred 패턴 (기존 OfficeGameMode와 동일)
	WalkingWorker = GetWorld()->SpawnActorDeferred<AOfficeworker>(
		WorkerClass,
		FTransform(SpawnRotation, SpawnLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (WalkingWorker)
	{
		WalkingWorker->SetEmployeeID(Employee.EmployeeID);
		ApplyAppearanceToWorker(WalkingWorker);
		WalkingWorker->SetActorHiddenInGame(true);

		// FinishSpawning 전에 자동 초기화 차단
		if (UEmployeeBehaviorComponent* BehaviorComp = WalkingWorker->FindComponentByClass<UEmployeeBehaviorComponent>())
		{
			BehaviorComp->bSkipAutoInit = true;
		}

		WalkingWorker->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));

		// FinishSpawning 후: tick 비활성화만 (bSkipAutoInit으로 타이머 자체가 생성되지 않음)
		if (UEmployeeBehaviorComponent* BehaviorComp = WalkingWorker->FindComponentByClass<UEmployeeBehaviorComponent>())
		{
			BehaviorComp->SetComponentTickEnabled(false);
		}

		// 중력/NavMesh 스냅 방지 — Hidden 대기 중 위치 드리프트 원천 차단
		WalkingWorker->GetCharacterMovement()->SetMovementMode(MOVE_None);

		// FinishSpawning 중 발생 가능한 위치/회전 보정을 원래 스폰 위치로 강제 복원
		WalkingWorker->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] Walking Worker 스폰 완료 (ID: %d)"), Employee.EmployeeID);
	}
}

void ARecruitmentGameMode::ApplyAppearanceToWorker(AOfficeworker* Worker)
{
	if (!Worker) return;

	const FEmployeeInstance& Employee = CurrentGachaResult.ResultEmployee;
	const FCharacterAppearance& Appearance = Employee.Appearance;
	EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);

	// 스틱 = 코스메틱 단일 진입점(데이터 모델 → 색/안경), 구 모듈러 = 헤어/모프 파이프라인.
	if (AStickOfficeworker* Stick = Cast<AStickOfficeworker>(Worker))
	{
		Stick->ApplyWorkerCosmetics(Employee.Department, Rank,
			AStickOfficeworker::ResolveGachaTier(Employee), Employee.EmployeeID,
			/*bIsGachaReveal=*/true);
	}
	else if (AModularOfficeworker* Modular = Cast<AModularOfficeworker>(Worker))
	{
		Modular->SetCharacterAppearance(Appearance, Rank, Employee.Gender, Employee.EmployeeID);
	}
}

void ARecruitmentGameMode::StartGachaSequence()
{
	if (!DoorActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitmentGameMode] DoorActor가 없어 가챠 시퀀스를 시작할 수 없습니다."));
		return;
	}

	// BP 키 입력 상호작용 차단 (가챠 시퀀스 중 수동 토글 방지)
	DoorActor->bAllowManualInteraction = false;

	// BP의 UTimelineComponent 비활성화 (C++ FTimeline만 사용)
	TArray<UTimelineComponent*> TimelineComps;
	DoorActor->GetComponents<UTimelineComponent>(TimelineComps);
	for (UTimelineComponent* TL : TimelineComps)
	{
		TL->Deactivate();
	}

	DoorActor->OnDoorOpened.AddDynamic(this, &ARecruitmentGameMode::OnDoorOpened);
	DoorActor->OnDoorClosed.AddDynamic(this, &ARecruitmentGameMode::OnDoorClosed);

	// 문 열기 전에 직원을 보이게 설정 (문 뒤에 위치하므로 문 메시에 가려짐)
	if (WalkingWorker)
	{
		WalkingWorker->SetActorHiddenInGame(false);

		UCharacterMovementComponent* MovComp = WalkingWorker->GetCharacterMovement();
		MovComp->GravityScale = 0.f;
		MovComp->SetMovementMode(MOVE_Walking);
	}

	DoorActor->OpenDoor();
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 가챠 시퀀스 시작 - 문 열기"));
}

void ARecruitmentGameMode::OnDoorOpened()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 문 열림 완료"));

	if (WalkingWorker)
	{
		// "EmployeeDestination" 태그의 TargetPoint → 도착 목적지
		FVector Destination = FVector::ZeroVector;
		for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		{
			if (It->Tags.Contains(FName("EmployeeDestination")))
			{
				Destination = It->GetActorLocation();
				UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] EmployeeDestination: %s"), *Destination.ToString());
				break;
			}
		}

		// NavMesh 기반 AI 이동 (직원은 StartGachaSequence에서 이미 보이게 설정됨)
		if (AAIController* AIC = Cast<AAIController>(WalkingWorker->GetController()))
		{
			AIC->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
				this, &ARecruitmentGameMode::OnMoveCompleted);
			AIC->MoveToLocation(Destination, 30.f);
		}
	}
}

void ARecruitmentGameMode::OnDoorClosed()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 문 닫힘 완료"));

	if (DoorActor)
	{
		DoorActor->OnDoorOpened.RemoveDynamic(this, &ARecruitmentGameMode::OnDoorOpened);
		DoorActor->OnDoorClosed.RemoveDynamic(this, &ARecruitmentGameMode::OnDoorClosed);
	}

	OnGachaSequenceFinished.Broadcast();
}

void ARecruitmentGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ARecruitmentGameMode::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// 콜백 해제
	if (WalkingWorker)
	{
		if (AAIController* AIC = Cast<AAIController>(WalkingWorker->GetController()))
		{
			AIC->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
		}
	}

	if (Result.IsSuccess() || Result.HasFlag(FPathFollowingResultFlags::AlreadyAtGoal))
	{
		OnWalkingWorkerArrived();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentGameMode] AI MoveTo 실패/Abort (Code: %d) - 무시"),
			(int32)Result.Code);
	}
}

void ARecruitmentGameMode::OnWalkingWorkerArrived()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] Walking Worker 도착 - 인사 시작"));

	if (WalkingWorker)
	{
		// 잔여 velocity 즉시 제거 (NavMesh 감속 중 Greeting 포즈 + 슬라이딩 방지)
		WalkingWorker->GetCharacterMovement()->StopMovementImmediately();

		// Greeting 시작과 동시에 Toast 표시
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			const FEmployeeInstance& Emp = CurrentGachaResult.ResultEmployee;
			FString RarityName = FLootBoxRarityUtility::GetKoreanName(CurrentGachaResult.PotentialRarity);
			// 부서명 = 산업별 표시명 DT 경유 (매핑 없으면 게터가 게임 기본값으로 폴백)
			UCGGameInstance* CGGI = Cast<UCGGameInstance>(GetGameInstance());
			UTableManagerSubsystem* DeptTableMgr = CGGI ? CGGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			FString DeptName = DeptTableMgr
				? DeptTableMgr->GetDepartmentDisplayName(CGGI->GetCurrentBuildingCompanyType(), Emp.Department).ToString()
				: DepartmentToString(Emp.Department);
			FText Msg = FText::FromString(FString::Printf(
				TEXT("%s[%s] - %s"), *Emp.EmployeeName, *RarityName, *DeptName));
			FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(CurrentGachaResult.PotentialRarity);
			UIMgr->ShowColoredNotification(Msg, 99999.0f, RarityColor);
		}

		UEmployeeBehaviorComponent* BehaviorComp = WalkingWorker->FindComponentByClass<UEmployeeBehaviorComponent>();
		if (BehaviorComp)
		{
			BehaviorComp->OnGreetingFinished.AddDynamic(this, &ARecruitmentGameMode::OnGreetingFinished);
			BehaviorComp->StartGreeting();
		}
	}
}

void ARecruitmentGameMode::OnGreetingFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 인사 완료 - 자동 채용 진행"));

	// Greeting 완료 후 직원 정지 (OnGreetingComplete→SetState(Wander)에서 Walking=true 재설정되므로 강제 리셋)
	if (WalkingWorker)
	{
		if (UEmployeeBehaviorComponent* BehaviorComp = WalkingWorker->FindComponentByClass<UEmployeeBehaviorComponent>())
		{
			BehaviorComp->SetComponentTickEnabled(false);
			BehaviorComp->Walking = false;
			BehaviorComp->Speed = 0.0f;
		}
		if (AAIController* AIC = Cast<AAIController>(WalkingWorker->GetController()))
		{
			AIC->StopMovement();
		}
		WalkingWorker->GetCharacterMovement()->SetMovementMode(MOVE_None);
	}

	// Portrait 촬영 완료 대기 또는 즉시 고용
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (EmpMgr && EmpMgr->IsPortraitCapturing())
	{
		bPendingHireConfirm = true;
		return;
	}
	PerformHireAndReturn();
}

void ARecruitmentGameMode::ReturnToOffice()
{
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		GI->ReturnFromRecruitmentMap();
	}
}

void ARecruitmentGameMode::StartPreemptivePortraitCapture()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmpMgr) return;

	const FEmployeeInstance& Employee = CurrentGachaResult.ResultEmployee;
	const FCharacterAppearance& Appearance = Employee.Appearance;
	EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);

	PortraitCaptureHandle = EmpMgr->OnEmployeeHireCompleted.AddUObject(
		this, &ARecruitmentGameMode::OnPreemptivePortraitDone);

	EmpMgr->CaptureEmployeePortrait(
		FString::FromInt(Employee.EmployeeID),
		Appearance,
		Rank,
		Employee.Gender,
		Appearance.HairCombinationType,
		Appearance.HairRandomSeed,
		Employee.Department,
		AStickOfficeworker::ResolveGachaTier(Employee)
	);

	// Portrait 워커 부재 등으로 즉시 실패 시 델리게이트 정리
	if (!EmpMgr->IsPortraitCapturing())
	{
		EmpMgr->OnEmployeeHireCompleted.Remove(PortraitCaptureHandle);
	}
}

void ARecruitmentGameMode::OnPreemptivePortraitDone(const FString& EmployeeID)
{
	// 델리게이트 정리
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>())
		{
			EmpMgr->OnEmployeeHireCompleted.Remove(PortraitCaptureHandle);
		}
	}

	if (bPendingHireConfirm)
	{
		bPendingHireConfirm = false;
		PerformHireAndReturn();
	}
}

void ARecruitmentGameMode::PerformHireAndReturn()
{
	URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();
	if (RecruitMgr)
	{
		bool bSuccess = RecruitMgr->ConfirmGachaHire(CurrentGachaResult);
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentGameMode] 채용 확정: %s (성공: %s)"),
			*CurrentGachaResult.ResultEmployee.EmployeeName,
			bSuccess ? TEXT("YES") : TEXT("NO"));
	}

	// 결과 패널 표시
	ShowResultUI();
}

void ARecruitmentGameMode::ShowResultUI()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::RecruitmentResultPanel);
	if (!WidgetClass) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	ResultPanelWidget = CreateWidget<URecruitmentResultPanelWidget>(PC, WidgetClass);
	if (ResultPanelWidget)
	{
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			ResultPanelWidget->InitResourceDisplay(GI->GetCurrentManagedBuildingIndex());
		}
		ResultPanelWidget->OnConfirmExitRequested.AddUObject(this, &ARecruitmentGameMode::OnConfirmExitRequested);
		ResultPanelWidget->AddToViewport(10);
	}
}

void ARecruitmentGameMode::CleanupResultUI()
{
	if (ResultPanelWidget)
	{
		ResultPanelWidget->OnConfirmExitRequested.RemoveAll(this);
		ResultPanelWidget->RequestClose();
		ResultPanelWidget = nullptr;
	}

	// Persistent Toast 제거
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ClearAllNotifications();
	}
}

void ARecruitmentGameMode::OnConfirmExitRequested()
{
	CleanupResultUI();

	// 마지막 채용 직원 ID 저장
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		GI->SetLastRecruitedEmployeeID(CurrentGachaResult.ResultEmployee.EmployeeID);
	}

	ReturnToOffice();
}
