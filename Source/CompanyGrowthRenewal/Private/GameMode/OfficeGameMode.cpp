// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/OfficeGameMode.h"
#include "Core/CGGameInstance.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/OfficePlayerController.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Util/CoordinateUtils.h"
#include "Manager/EmployeeManager.h"
#include "Entity/Officeworker/ModularOfficeworker.h"
#include "Entity/Officeworker/OfficeworkerMale.h"
#include "Entity/Officeworker/OfficeworkerFemale.h"
#include "Entity/Officeworker/StickOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/MissionManagerSubsystem.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "AsyncLoadingScreenLibrary.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ResourceItemManager.h"
#include "Data/EntitySaveData.h"
#include "UI/Panel/OfficeLayerWidget.h"

AOfficeGameMode::AOfficeGameMode()
{
	// OfficeMap 전용 Pawn과 Controller 설정
	DefaultPawnClass = AOfficeCameraPawn::StaticClass();
	PlayerControllerClass = AOfficePlayerController::StaticClass();
}

void AOfficeGameMode::BeginPlay()
{
	Super::BeginPlay();

	// OfficeInterior 찾아서 캐시
	FindAndCacheOfficeInterior();

	// CoordinateUtils에 PlayerController 설정 (터치 입력 등을 위해 필요)
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		CoordinateUtils::SetPlayerController(PC); // OfficePlayerController는 MainMapPlayerController 상속
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] CoordinateUtils::PlayerController set"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] Failed to get OfficePlayerController!"));
	}

	// GameInstance에서 진입 모드 확인
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		CurrentMode = GameInstance->GetOfficeMode();
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] BeginPlay - Current Office Mode: %d"), (int32)CurrentMode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] GameInstance is null! Using default mode (Normal)"));
		CurrentMode = EOfficeMode::Normal;
	}
}

void AOfficeGameMode::FindAndCacheOfficeInterior()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOfficeInterior::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		CachedOfficeInterior = Cast<AOfficeInterior>(FoundActors[0]);
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] OfficeInterior cached: %s (%p)"),
			*CachedOfficeInterior->GetName(), CachedOfficeInterior);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] OfficeInterior not found in level!"));
	}
}

void AOfficeGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] ========== StartPlay BEGIN =========="));

	// Office UI 표시 (UIManager 경유)
	if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIManager->CreateOfficeLayerWidget();
	}

	// 현재 관리 중인 건물 인덱스 확인
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		int32 BuildingIndex = GameInstance->GetCurrentManagedBuildingIndex();
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] Before Load - ManagedBuildingIndex: %d"), BuildingIndex);

		// 게임 데이터 로드 (Office 확장, Decoration, Workstation 복원)
		// 직원 스폰보다 먼저 수행해야 OfficeInterior 상태가 올바르게 설정됨
		GameInstance->LoadGameAfterLevelStart();

		int32 BuildingIndexAfter = GameInstance->GetCurrentManagedBuildingIndex();
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] After Load - ManagedBuildingIndex: %d"), BuildingIndexAfter);

		// 회사 타입 체크 - SaveData에서 읽어서 GameInstance에 캐싱
		ECompanyType CurrentCompanyType = ECompanyType::None;
		if (USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				for (FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
				{
					if (Building.BuildingIndex == BuildingIndexAfter)
					{
						CurrentCompanyType = Building.BuildingData.CompanyType;
						break;
					}
				}
			}
		}

		// GameInstance에 캐싱 (Office 내에서 빠르게 접근 가능)
		GameInstance->SetCurrentBuildingCompanyType(CurrentCompanyType);

		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] CompanyType: %s"), *CompanyTypeToString(CurrentCompanyType));

		if (BuildingIndexAfter != INDEX_NONE)
		{
			StartPortraitCapture(BuildingIndexAfter);
		}
	}

	// 로드 완료 후 직원 스폰
	// NOTE: 레벨에 배치된 기존 Officeworker는 UI 렌더러용이므로 삭제하지 않음
	SpawnEmployeesInOffice();

	// 스테이지 진행 상태 로드
	LoadStageProgressFromSave();

	// 미션 가이드(트래커/오버레이) 재구축은 UIManager::OnLevelLayerReady 신호(ShowOfficeUI 레이어 생성 후)가 구동 —
	// 여기서 직접 호출하지 않는다(레이어 준비 전 빌드되던 경쟁/깜빡임 제거).

	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] ========== StartPlay END =========="));
}

// ========== Office Mode Management ==========

EOfficeMode AOfficeGameMode::GetCurrentOfficeMode() const
{
	return CurrentMode;
}

void AOfficeGameMode::SwitchOfficeMode(EOfficeMode NewMode)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Switching mode from %d to %d"), (int32)CurrentMode, (int32)NewMode);

	CurrentMode = NewMode;

	// GameInstance에도 모드 업데이트
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		GameInstance->SetOfficeMode(NewMode);
	}

}

void AOfficeGameMode::ReturnToMainMap()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Returning to Main Map"));

	// 스테이지 진행 상태 저장
	SaveStageProgressToSave();

	// GameInstance 초기화 (다음 진입을 위해)
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		// 레벨 전환 전 현재 데이터 저장
		if (USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
			UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] Game data saved before returning to MainMap"));
		}

		GameInstance->SetOfficeMode(EOfficeMode::Normal);
		GameInstance->SetCurrentManagedBuilding(nullptr);

		// MainMap으로 전환 (TransitionToLevel 사용해서 MapType 설정)
		GameInstance->TransitionToLevel(TEXT("MainMap"));
	}
}


// ========== Employee Management ==========

void AOfficeGameMode::SpawnEmployeesInOffice()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] SpawnEmployeesInOffice()"));

	// GameInstance에서 현재 관리 중인 건물 인덱스 가져오기
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] GameInstance is null!"));
		return;
	}

	int32 BuildingIndex = GameInstance->GetCurrentManagedBuildingIndex();
	if (BuildingIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] No building is currently managed (BuildingIndex is INDEX_NONE)"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] *** LOADING FROM GAMEINSTANCE ***"));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] BuildingIndex: %d"), BuildingIndex);

	// EmployeeManager에서 해당 건물의 직원 목록 가져오기
	UEmployeeManager* EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!EmployeeManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] EmployeeManager is null!"));
		return;
	}

	TArray<FEmployeeInstance> Employees = EmployeeManager->GetEmployeesInBuilding(BuildingIndex);

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Found %d employees in building %d"), Employees.Num(), BuildingIndex);

	if (Employees.Num() == 0)
	{
		// 직원이 없어도 로딩 화면 종료
		UAsyncLoadingScreenLibrary::StopLoadingScreen();

		// 직원 없어도 1초 후 수익 수집 체크
		GetWorld()->GetTimerManager().SetTimer(
			RevenueCollectionTimerHandle,
			this, &AOfficeGameMode::CheckAndCollectStoredRevenue,
			1.0f, false);
		return;
	}

	// NavSystem 확인
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] NavigationSystem not found! Using fallback spawn locations."));
	}

	// 프레임 분산 스폰을 위해 대기열에 저장
	PendingEmployeesToSpawn = Employees;
	CurrentSpawnIndex = 0;

	// 로딩 화면 먼저 종료 (World Timer가 작동하려면 로딩 화면이 꺼져야 함)
	UAsyncLoadingScreenLibrary::StopLoadingScreen();
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Loading screen stopped, starting distributed spawn"));

	// 로딩 화면 종료 후 다음 프레임에서 스폰 시작
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		SpawnNextEmployee();
	});
}

void AOfficeGameMode::SpawnNextEmployee()
{
	if (CurrentSpawnIndex >= PendingEmployeesToSpawn.Num())
	{
		// 모든 직원 스폰 완료
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Successfully spawned %d employees with roaming enabled"), SpawnedEmployees.Num());
		PendingEmployeesToSpawn.Empty();
		return;
	}

	// 현재 직원 스폰
	SpawnSingleEmployee(PendingEmployeesToSpawn[CurrentSpawnIndex]);
	CurrentSpawnIndex++;

	// 다음 직원은 2프레임 후 스폰 (GPU Skin Cache 버퍼 초기화 시간 확보)
	// 0.033초 = 약 2프레임 (30fps 기준) / 1프레임 (60fps 기준)
	if (CurrentSpawnIndex < PendingEmployeesToSpawn.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AOfficeGameMode::SpawnNextEmployee,
			0.033f,  // 약 2프레임 대기
			false
		);
	}
	else
	{
		// 마지막 직원 스폰 완료
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Successfully spawned %d employees (hidden)"), SpawnedEmployees.Num());
		PendingEmployeesToSpawn.Empty();

		// Operation 모드 복원 시 1인당 수익 분배
		if (bHasActiveOperationOnLoad)
		{
			UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
			if (GI)
			{
				int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
				if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
				{
					if (FOperationData* OpData = OpMgr->GetOperationByBuildingID(BuildingIndex))
					{
						int32 EmpCount = SpawnedEmployees.Num();
						if (EmpCount > 0)
						{
							float PerEmpIncome = OpData->ActualRevenuePerSecond / static_cast<float>(EmpCount);
							for (AOfficeworker* Worker : SpawnedEmployees)
							{
								if (Worker && Worker->BehaviorComponent)
								{
									Worker->BehaviorComponent->SetOperationIncome(PerEmpIncome);
								}
							}
							UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Operation income distributed: %.2f per employee (%d employees)"),
								PerEmpIncome, EmpCount);
						}
					}
				}
			}
		}

		// 운영 수익률 라이브 동기화 — ActualRevenuePerSecond 는 감쇠/변동으로 매초 변하는데
		// 배포가 입장 1회뿐이면 직원 플로팅 합이 표시 수익/초와 어긋남 → 1초마다 재배포
		{
			FTimerHandle IncomeSyncTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				IncomeSyncTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					UCGGameInstance* SyncGI = Cast<UCGGameInstance>(GetGameInstance());
					if (!SyncGI) return;
					UProjectOperationManager* SyncOpMgr = SyncGI->GetSubsystem<UProjectOperationManager>();
					if (!SyncOpMgr) return;
					FOperationData* SyncOp = SyncOpMgr->GetOperationByBuildingID(SyncGI->GetCurrentManagedBuildingIndex());
					if (!SyncOp) return;
					const int32 SyncEmpCount = SpawnedEmployees.Num();
					if (SyncEmpCount <= 0) return;
					const float SyncPerEmp = SyncOp->ActualRevenuePerSecond / static_cast<float>(SyncEmpCount);
					for (AOfficeworker* SyncWorker : SpawnedEmployees)
					{
						if (SyncWorker && SyncWorker->BehaviorComponent)
						{
							SyncWorker->BehaviorComponent->SetOperationIncome(SyncPerEmp);
						}
					}
				}),
				1.0f, true);
		}

		// 0.1초 후 모든 직원 표시 (GPU Skin Cache 버퍼 초기화 시간 확보)
		FTimerHandle VisibilityTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			VisibilityTimerHandle,
			[this]()
			{
				for (AOfficeworker* Worker : SpawnedEmployees)
				{
					if (Worker)
					{
						Worker->SetActorHiddenInGame(false);
					}
				}
				UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] All %d employees now visible"), SpawnedEmployees.Num());

				// 직원 표시 후 1초 뒤 수익 수집 연출 트리거
				GetWorld()->GetTimerManager().SetTimer(
					RevenueCollectionTimerHandle,
					this, &AOfficeGameMode::CheckAndCollectStoredRevenue,
					1.0f, false);
			},
			0.1f,
			false
		);
	}
}

void AOfficeGameMode::SpawnSingleEmployee(const FEmployeeInstance& Employee)
{
	UEmployeeManager* EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!EmployeeManager)
	{
		return;
	}

	// EmployeeManager에서 외형 데이터 가져오기
	FCharacterAppearance EmployeeAppearance = EmployeeManager->GetEmployeeAppearance(Employee.EmployeeID);

	// EnhancementLevel에서 Rank 계산
	EEmployeeRank CurrentRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);

	// 클래스 선택: 스틱맨(신규) vs 성별 모듈러(구). 구 경로는 그대로 보존.
	TSubclassOf<AOfficeworker> OfficeworkerClass;
	if (ShouldUseStickWorker(Employee))
	{
		// BP_StickOfficeworker 우선 — BP 에서 튜닝한 기본값(BodyScale/페이스 플레인 트랜스폼)이 적용되도록.
		// BP 미존재 시 C++ 클래스로 폴백(기능 동일, 튜닝 기본값만 없음). 1회만 해석해 캐시 — 매 스폰 동기 로드/재해석 제거.
		if (!ResolvedStickWorkerClass)
		{
			static const TSoftClassPtr<AOfficeworker> StickBPClass(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/BP_StickOfficeworker.BP_StickOfficeworker_C")));
			UClass* LoadedBP = StickBPClass.LoadSynchronous();
			ResolvedStickWorkerClass = LoadedBP ? LoadedBP : AStickOfficeworker::StaticClass();
		}
		OfficeworkerClass = ResolvedStickWorkerClass;
	}
	else if (Employee.Gender == EEmployeeGender::Male)
	{
		OfficeworkerClass = AOfficeworkerMale::StaticClass();
	}
	else
	{
		OfficeworkerClass = AOfficeworkerFemale::StaticClass();
	}

	// OfficeInterior 바닥 영역 내 랜덤 위치에서 스폰
	FVector SpawnLocation;
	if (CachedOfficeInterior)
	{
		SpawnLocation = CachedOfficeInterior->GetRandomFloorLocation();
		// Capsule 반 높이만큼 Z 조정 (캐릭터가 바닥에 서도록)
		SpawnLocation.Z += 92.04f;
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Spawn location from OfficeInterior: %s"), *SpawnLocation.ToString());
	}
	else
	{
		// OfficeInterior 없으면 폴백: 일렬 배치
		SpawnLocation = FVector(200.f + CurrentSpawnIndex * 200.f, -400.f, 92.04f);
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] OfficeInterior not found, using fallback location for employee %d"), Employee.EmployeeID);
	}

	FRotator Rotation = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);

	// SpawnActorDeferred 사용 - GPU Skin Cache 충돌 방지를 위해 초기화 분리
	AOfficeworker* SpawnedWorker = GetWorld()->SpawnActorDeferred<AOfficeworker>(
		OfficeworkerClass,
		FTransform(Rotation, SpawnLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (SpawnedWorker)
	{
		// EmployeeID 먼저 저장 (deterministic 신발 선택용)
		SpawnedWorker->SetEmployeeID(Employee.EmployeeID);

		// GPU Skin Cache 버퍼 초기화를 위해 처음에는 숨김 (FinishSpawning 전에!)
		// 첫 렌더 프레임에서 Previous/Current Position 버퍼 충돌 방지
		SpawnedWorker->SetActorHiddenInGame(true);

		// 모든 초기화 완료 후 스폰 완료 처리
		SpawnedWorker->FinishSpawning(FTransform(Rotation, SpawnLocation));

		// 코스메틱은 FinishSpawning '이후'에 적용 — PostInitializeComponents(코스메틱 컴포넌트 이름 재연결)+컴포넌트 등록이 끝나야
		// 타이/헤어 MID·바디 슬롯 MID 가 유효·등록된 컴포넌트에 적용된다. 이전엔 deferred 창에서 호출돼 멤버 null/미등록 → 색 미적용. (2026-06-12)
		if (AStickOfficeworker* StickWorker = Cast<AStickOfficeworker>(SpawnedWorker))
		{
			// 신규 경로: 데이터 모델 → 색/안경 코스메틱 (헤어/모프 파이프라인 미사용)
			const EGachaTier Tier = ResolveGachaTier(Employee);
			StickWorker->ApplyWorkerCosmetics(Employee.Department, CurrentRank, Tier, Employee.EmployeeID);
		}
		else if (AModularOfficeworker* ModularWorker = Cast<AModularOfficeworker>(SpawnedWorker))
		{
			// 구 경로: 모듈러 외형 (SetCharacterAppearance 내부에서 이미 hair 설정됨)
			ModularWorker->SetCharacterAppearance(
				EmployeeAppearance,
				CurrentRank,
				Employee.Gender,
				Employee.EmployeeID
			);
		}

		// Workstation 할당 찾기 (이미 배정된 좌석이 있으면 참조 설정)
		UOfficeManager* OfficeManager = GetWorld()->GetSubsystem<UOfficeManager>();
		if (OfficeManager)
		{
			for (AWorkstationActorBase* Workstation : OfficeManager->GetPlacedWorkstations())
			{
				if (!Workstation) continue;

				// 이 직원에게 배정된 좌석 찾기
				for (int32 i = 0; i < Workstation->GetChairCount(); ++i)
				{
					if (Workstation->GetAssignedEmployeeID(i) == Employee.EmployeeID)
					{
						SpawnedWorker->SetAssignedWorkstation(Workstation, i);
						UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Found existing assignment for employee %d at workstation seat %d"),
							Employee.EmployeeID, i);
						break;
					}
				}
			}
		}

		// Active Operation 복원: BeginPlay 0.1s 타이머 전에 모드 설정
		if (bHasActiveOperationOnLoad && SpawnedWorker->BehaviorComponent)
		{
			SpawnedWorker->BehaviorComponent->CurrentBehaviorMode = EEmployeeBehaviorMode::Operation;
		}

		// 숨긴 상태 유지 - 로딩 화면 종료 후 다음 프레임에서 표시
		// (같은 프레임에서 unhide하면 GPU Skin Cache 크래시 발생)

		// 배회 시작은 여기서 하지 않는다 — 모드별 초기 행동(Idle=배회 / Operation=착석)은
		// BehaviorComponent::BeginPlay 가 단독 소유. 스폰 직후 개입하면 그쪽 초기화와 경합한다.

		// 스폰된 직원 목록에 추가
		SpawnedEmployees.Add(SpawnedWorker);

		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Spawned employee: %d (%s) at location %s [%d/%d]"),
			Employee.EmployeeID, *Employee.EmployeeName, *SpawnLocation.ToString(),
			CurrentSpawnIndex + 1, PendingEmployeesToSpawn.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] Failed to spawn employee: %d"), Employee.EmployeeID);
	}
}

void AOfficeGameMode::SpawnWorkerForAssignment(const FEmployeeInstance& Employee, AWorkstationActorBase* Workstation, int32 SeatIndex)
{
	if (!Workstation || SeatIndex < 0)
	{
		return;
	}

	// 중복 방지: 이미 이 직원 워커가 스폰돼 있으면 좌석 참조만 갱신 (재배정/리로드 시 2중 스폰 차단)
	if (AOfficeworker* Existing = FindEmployeeActorByID(Employee.EmployeeID))
	{
		Existing->SetAssignedWorkstation(Workstation, SeatIndex);
		return;
	}

	UEmployeeManager* EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!EmployeeManager)
	{
		return;
	}

	FCharacterAppearance EmployeeAppearance = EmployeeManager->GetEmployeeAppearance(Employee.EmployeeID);
	EEmployeeRank CurrentRank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);

	// 클래스 선택: SpawnSingleEmployee 와 동일 규칙 (스틱맨 신규 / 성별 모듈러 구)
	TSubclassOf<AOfficeworker> OfficeworkerClass;
	if (ShouldUseStickWorker(Employee))
	{
		if (!ResolvedStickWorkerClass)
		{
			static const TSoftClassPtr<AOfficeworker> StickBPClass(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/Characters/StickmanCG/BP_StickOfficeworker.BP_StickOfficeworker_C")));
			UClass* LoadedBP = StickBPClass.LoadSynchronous();
			ResolvedStickWorkerClass = LoadedBP ? LoadedBP : AStickOfficeworker::StaticClass();
		}
		OfficeworkerClass = ResolvedStickWorkerClass;
	}
	else if (Employee.Gender == EEmployeeGender::Male)
	{
		OfficeworkerClass = AOfficeworkerMale::StaticClass();
	}
	else
	{
		OfficeworkerClass = AOfficeworkerFemale::StaticClass();
	}

	// 좌석 위치에 스폰 — 여기는 NavMesh 밖(책상 구멍)이라 AI 이동 시작점으로 쓸 수 없다
	const FVector SeatLocation = Workstation->GetSeatLocation(SeatIndex);
	const FRotator Rotation = Workstation->GetSeatRotation(SeatIndex);

	AOfficeworker* SpawnedWorker = GetWorld()->SpawnActorDeferred<AOfficeworker>(
		OfficeworkerClass,
		FTransform(Rotation, SeatLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (!SpawnedWorker)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] SpawnWorkerForAssignment failed for employee: %d"), Employee.EmployeeID);
		return;
	}

	SpawnedWorker->SetEmployeeID(Employee.EmployeeID);

	// 즉시 등장 (로드 시 일괄 스폰의 GPU Skin Cache 지연-표시 처리는 단건 배정에는 불필요)
	SpawnedWorker->FinishSpawning(FTransform(Rotation, SeatLocation));

	// 코스메틱은 FinishSpawning '이후' — PostInitializeComponents(컴포넌트 재연결)+등록 완료 후라야 타이/헤어 MID·바디 슬롯 색이
	// 유효 컴포넌트에 적용된다. 이전엔 deferred 창에서 호출돼 멤버 null/미등록 → 신규 배정 직원 색 미적용. (2026-06-12)
	if (AStickOfficeworker* StickWorker = Cast<AStickOfficeworker>(SpawnedWorker))
	{
		const EGachaTier Tier = ResolveGachaTier(Employee);
		StickWorker->ApplyWorkerCosmetics(Employee.Department, CurrentRank, Tier, Employee.EmployeeID);
	}
	else if (AModularOfficeworker* ModularWorker = Cast<AModularOfficeworker>(SpawnedWorker))
	{
		ModularWorker->SetCharacterAppearance(EmployeeAppearance, CurrentRank, Employee.Gender, Employee.EmployeeID);
	}

	// 좌석 참조 설정 → BehaviorComponent 가 좌석으로 이동/착석 처리
	SpawnedWorker->SetAssignedWorkstation(Workstation, SeatIndex);

	// 운영 진행 중 착석 — 로드 경로(483-487행)와 동일하게 즉시 Operation 합류 (다음 모드 전환까지 수익 0 방지)
	if (SpawnedWorker->BehaviorComponent)
	{
		UCGGameInstance* CGI = Cast<UCGGameInstance>(GetGameInstance());
		UProjectOperationManager* OpMgr = CGI ? CGI->GetSubsystem<UProjectOperationManager>() : nullptr;
		if (CGI && OpMgr && OpMgr->HasActiveOperation(CGI->GetCurrentManagedBuildingIndex()))
		{
			SpawnedWorker->BehaviorComponent->CurrentBehaviorMode = EEmployeeBehaviorMode::Operation;
		}
	}

	// 여기서 배회를 시작하지 않는다 — 좌석은 책상 NavMesh 구멍 안이라 RoamOrigin 이 도달 불가 지점으로 굳고,
	// bIsRoaming 이 걸려 0.1s 뒤 BeginPlay 의 정상 초기화(StartIdleWander = NavMesh 투영 후 배회)가 무시된다.
	// 모드별 초기 행동은 BehaviorComponent::BeginPlay 가 단독 소유.

	SpawnedEmployees.Add(SpawnedWorker);

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] SpawnWorkerForAssignment: employee %d at workstation seat %d"),
		Employee.EmployeeID, SeatIndex);
}

void AOfficeGameMode::DespawnWorkerByID(int32 EmployeeID, AWorkstationActorBase* SeatedWorkstation)
{
	AOfficeworker* Worker = FindEmployeeActorByID(EmployeeID);
	if (!Worker)
	{
		return;
	}
	if (SeatedWorkstation)
	{
		SeatedWorkstation->StandUp(Worker);
	}
	SpawnedEmployees.Remove(Worker);
	Worker->Destroy();
}

bool AOfficeGameMode::ShouldUseStickWorker(const FEmployeeInstance& /*Employee*/) const
{
	// v1: 전역 토글. 추후 "신규 채용만" 등 조건 확장 여지 (Employee 인자 사용).
	return bUseStickWorkers;
}

EGachaTier AOfficeGameMode::ResolveGachaTier(const FEmployeeInstance& Employee)
{
	// 변환 로직은 AStickOfficeworker 로 일원화 (스폰/뽑기/초상화 공용 단일 진실).
	return AStickOfficeworker::ResolveGachaTier(Employee);
}

bool AOfficeGameMode::GetRandomSpawnLocationOnNavMesh(FVector& OutLocation)
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] NavigationSystem is NULL!"));
		return false;
	}

	// NavMesh 데이터 확인
	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] No NavData found! Check if NavMeshBoundsVolume exists in level."));
		return false;
	}

	// 전체 NavMesh 영역에서 랜덤 위치 찾기
	FNavLocation NavLocation;
	bool bFound = NavSys->GetRandomPoint(NavLocation);

	if (bFound)
	{
		OutLocation = NavLocation.Location;
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Found NavMesh location: %s"), *OutLocation.ToString());
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] GetRandomPoint failed - NavMesh may not be built or is empty"));
	return false;
}

AOfficeworker* AOfficeGameMode::FindEmployeeActorByID(int32 EmployeeID) const
{
	for (AOfficeworker* Employee : SpawnedEmployees)
	{
		if (Employee && Employee->GetEmployeeID() == EmployeeID)
		{
			return Employee;
		}
	}
	return nullptr;
}

void AOfficeGameMode::OnEmployeeClicked(AOfficeworker* ClickedEmployee)
{
	if (!ClickedEmployee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] OnEmployeeClicked - ClickedEmployee is null"));
		return;
	}

	int32 EmployeeID = ClickedEmployee->GetEmployeeID();
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Employee clicked: %d"), EmployeeID);
}

// ========== Portrait 촬영 ==========

void AOfficeGameMode::StartPortraitCapture(int32 BuildingIndex)
{
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance) return;

	UEmployeeManager* EmpMgr = GameInstance->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr) return;

	TArray<FEmployeeInstance> Employees = EmpMgr->GetEmployeesInBuilding(BuildingIndex);

	PendingPortraitCards.Empty();
	PendingPortraitBuildingIndex = BuildingIndex;

	for (FEmployeeInstance& Emp : Employees)
	{
		FString PortraitPath = FPaths::ProjectSavedDir() / TEXT("Portraits") / FString::FromInt(Emp.EmployeeID) + TEXT(".png");
		if (!FPaths::FileExists(PortraitPath))
		{
			PendingPortraitCards.Add(new FEmployeeInstance(Emp));
		}
	}

	if (PendingPortraitCards.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Starting portrait capture for %d employees"), PendingPortraitCards.Num());
		CaptureNextPortrait();
	}
}

void AOfficeGameMode::CaptureNextPortrait()
{
	if (PendingPortraitCards.Num() == 0)
	{
		OnAllPortraitsCaptured();
		return;
	}

	FEmployeeInstance* Card = PendingPortraitCards[0];
	PendingPortraitCards.RemoveAt(0);

	if (!Card)
	{
		CaptureNextPortrait();
		return;
	}

	CurrentCapturingEmployeeID = Card->EmployeeID;

	// EmployeeManager의 기존 시스템 사용
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		delete Card;
		CaptureNextPortrait();
		return;
	}

	UEmployeeManager* EmpMgr = GameInstance->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr)
	{
		delete Card;
		CaptureNextPortrait();
		return;
	}

	// 기존 핸들 제거 후 새로 등록
	EmpMgr->OnEmployeeHireCompleted.Remove(PortraitCaptureHandle);
	PortraitCaptureHandle = EmpMgr->OnEmployeeHireCompleted.AddUObject(this, &AOfficeGameMode::OnPortraitCaptured);

	EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Card->EnhancementLevel);

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Capturing portrait for EmployeeID: %d"), Card->EmployeeID);

	EmpMgr->CaptureEmployeePortrait(
		FString::FromInt(Card->EmployeeID),
		Card->Appearance,
		Rank,
		Card->Gender,
		Card->Appearance.HairCombinationType,
		Card->Appearance.HairRandomSeed,
		Card->Department,
		ResolveGachaTier(*Card)
	);

	// 임시 복사본 삭제
	delete Card;
}

void AOfficeGameMode::OnPortraitCaptured(const FString& EmployeeID)
{
	int32 CapturedID = FCString::Atoi(*EmployeeID);

	// 현재 촬영 중인 ID와 일치하는지 확인
	if (CapturedID == CurrentCapturingEmployeeID)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Portrait captured for EmployeeID: %d"), CapturedID);
		CurrentCapturingEmployeeID = -1;

		// 다음 촬영
		CaptureNextPortrait();
	}
}

void AOfficeGameMode::OnAllPortraitsCaptured()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] All portraits captured for BuildingIndex: %d"), PendingPortraitBuildingIndex);

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		UEmployeeManager* EmpMgr = GameInstance->GetSubsystem<UEmployeeManager>();
		if (EmpMgr)
		{
			EmpMgr->OnEmployeeHireCompleted.Remove(PortraitCaptureHandle);
		}
	}

	PendingPortraitBuildingIndex = INDEX_NONE;
}

void AOfficeGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 수익 수집 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(RevenueCollectionTimerHandle);

	// 델리게이트 해제
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		// Portrait 촬영 델리게이트 해제
		UEmployeeManager* EmpMgr = GameInstance->GetSubsystem<UEmployeeManager>();
		if (EmpMgr)
		{
			EmpMgr->OnEmployeeHireCompleted.Remove(PortraitCaptureHandle);
		}

		// StageProgressManager 델리게이트 해제 (WorldSubsystem이므로 World에서 가져옴)
		UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
		if (StageMgr)
		{
			StageMgr->OnStageComplete.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ========== 수익 수집 연출 ==========

void AOfficeGameMode::CheckAndCollectStoredRevenue()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI) return;

	// StoredRevenue 확인
	UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>();
	if (!OfficeMgr) return;

	float StoredAmount = OfficeMgr->GetStoredRevenue();
	if (StoredAmount <= 0.0f) return;

	// 원자적 수집 (Money에 즉시 추가)
	float CollectedFloat = OfficeMgr->CollectStoredRevenue();
	int64 CollectedInt = FMath::RoundToInt64(CollectedFloat);

	if (CollectedInt <= 0) return;

	// ResourceItemManager에 Money 추가 — 수령은 탭 연타 경로라 즉시 저장 대신 지연 저장
	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	if (RMgr)
	{
		RMgr->StoreResource(EResourceType::Money, CollectedInt, /*bShouldSave=*/false);
	}
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->RequestDeferredSave();
	}

	// OfficeLayerWidget에 코인 연출 트리거
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (UIMgr)
	{
		if (UOfficeLayerWidget* OfficeLayer = UIMgr->GetOfficeLayer())
		{
			OfficeLayer->PlayStoredRevenueCollection(CollectedInt);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Collected stored revenue: %lld"), CollectedInt);
}

// ========== Stage Progress ==========

UOfficeStageProgressManager* AOfficeGameMode::GetStageProgressManager() const
{
	// WorldSubsystem이므로 World에서 가져옴
	return GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
}

void AOfficeGameMode::StartProject(const FProjectData& ProjectData)
{
	UOfficeStageProgressManager* StageMgr = GetStageProgressManager();
	if (!StageMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeGameMode] StartProject - StageProgressManager is null!"));
		return;
	}

	// 델리게이트 연결 (이미 연결되어 있으면 무시됨)
	StageMgr->OnStageComplete.AddDynamic(this, &AOfficeGameMode::OnStageCompleted);

	// 프로젝트 번호 1, 스테이지 1부터 시작
	StageMgr->StartNewStage(ProjectData, 1, 1);

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] StartProject - Project '%s' started"), *ProjectData.ProjectName.ToString());
}

void AOfficeGameMode::EndCurrentProject()
{
	UOfficeStageProgressManager* StageMgr = GetStageProgressManager();
	if (StageMgr)
	{
		StageMgr->EndCurrentStage();
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] EndCurrentProject - Project ended"));
	}
}

void AOfficeGameMode::OnStageCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] OnStageCompleted - Stage 4 completed! Show release choice UI"));

	// TODO: 출시 선택 UI 표시 (자동 운영 슬롯 vs 직접 운영)
}

void AOfficeGameMode::LoadStageProgressFromSave()
{
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}

	int32 BuildingIndex = GameInstance->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] LoadStageProgressFromSave - No BuildingIndex"));
		return;
	}

	USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr)
	{
		return;
	}

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		return;
	}

	// OfficeDataMap에서 해당 건물의 OfficeSaveData 찾기
	const FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
	if (OfficeData)
	{
		UOfficeStageProgressManager* StageMgr = GetStageProgressManager();
		if (StageMgr)
		{
			// 저장된 데이터 복원 (타이머 관련 상태는 초기화)
			FStageProgressData LoadedData = OfficeData->StageProgress;

			// 동시 진행 모드: 진행 중(1)이었으면 idle(0)로 리셋 (15초는 짧으므로 처음부터)
			LoadedData.bIsTimerRunning = false;
			LoadedData.RemainingTime = 15.0f;
			if (LoadedData.CurrentStep >= 1)
			{
				LoadedData.CurrentStep = 0;
			}

			StageMgr->SetProgressData(LoadedData);

			if (LoadedData.ProjectID > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Stage progress loaded - Stage: %d, Step: %d (waiting for user to start)"),
					LoadedData.StageNumber, LoadedData.CurrentStep);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Stage progress loaded (no active project)"));
			}
		}

		// Active Operation 상태 확인 (직원 스폰 시 Operation 모드로 설정하기 위해)
		if (OfficeData->bHasActiveOperation)
		{
			bHasActiveOperationOnLoad = true;
			UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Active operation detected on load, employees will start in Operation mode"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] No office data for BuildingIndex: %d"), BuildingIndex);
	}
}

void AOfficeGameMode::SaveStageProgressToSave()
{
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}

	int32 BuildingIndex = GameInstance->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGameMode] SaveStageProgressToSave - No BuildingIndex"));
		return;
	}

	UOfficeStageProgressManager* StageMgr = GetStageProgressManager();
	if (!StageMgr)
	{
		return;
	}

	USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr)
	{
		return;
	}

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		return;
	}

	// OfficeDataMap에서 해당 건물의 OfficeSaveData 찾아서 업데이트 (없으면 생성)
	// bStageInProgress는 저장하지 않음 - 로드 후 사용자가 수동으로 시작
	FOfficeSaveData& OfficeData = SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingIndex);
	OfficeData.StageProgress = StageMgr->GetProgressData();

	UE_LOG(LogTemp, Log, TEXT("[OfficeGameMode] Stage progress saved - Stage: %d, Step: %d"),
		OfficeData.StageProgress.StageNumber,
		OfficeData.StageProgress.CurrentStep);
}
