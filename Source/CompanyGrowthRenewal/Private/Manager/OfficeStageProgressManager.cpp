#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectTraitEventHandler.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/EmployeePotentialData.h"
#include "Data/EmployeeStatsData.h"
#include "Data/ProjectBoardData.h"
#include "Data/TierLayout.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/TrendManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "Table/StepDisplayNameTable.h"
#include "Core/CGGameInstance.h"
#include "Enum/ProjectStepType.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "EngineUtils.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"

namespace
{
	// 튜토리얼 종료 시 빌딩 Lv3(4번째 워크스테이션+강화 2종 해금) 도달용 배수 — 직원 레벨업은 별도 결손 보정이 담당

	// 튜토리얼 M9 선행 보장: 첫 프로젝트로 반드시 1회 레벨업(=SP 1)시킨다.
	// SP 를 직접 주면 "SP == 누적 레벨업" 불변식이 깨지므로, 모자란 XP 만 채워 정상 레벨업을 일으킨다.
	void TopUpExperienceForGuaranteedLevelUp(UEmployeeManager* EmployeeMgr, int32 BuildingIndex)
	{
		if (!EmployeeMgr)
		{
			return;
		}

		for (const FEmployeeInstance& Emp : EmployeeMgr->GetEmployeesInBuilding(BuildingIndex))
		{
			if (Emp.Level > 1 || Emp.AvailableSkillPoints > 0)
			{
				continue;
			}

			const int32 Required = EmployeeMgr->GetMaxExperienceForLevel(Emp.Level);
			const float Deficit = static_cast<float>(Required) - Emp.Experience;
			if (Deficit <= 0.f)
			{
				continue;
			}

			EmployeeMgr->AddExperience(Emp.EmployeeID, Deficit);
			UE_LOG(LogTemp, Log,
				TEXT("[Tutorial] M9 선행 보장 — Employee %d 에 XP %.1f 보정(요구 %d) → 레벨업"),
				Emp.EmployeeID, Deficit, Required);
		}
	}
}

UOfficeStageProgressManager::UOfficeStageProgressManager()
{
	CriticalChance = EmployeeStatTuning::BaseCritChance;   // 값 SOT = EmployeeStatTuning (★0 합계 2.0% 앵커)
	bStageInProgress = false;
}

bool UOfficeStageProgressManager::ShouldCreateSubsystem(UObject* Outer) const
{
	if (UWorld* World = Cast<UWorld>(Outer))
	{
		FString MapName = World->GetMapName();
		if (MapName.Contains(TEXT("OfficeMap")))
		{
			UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] ShouldCreateSubsystem - Creating in %s"), *MapName);
			return true;
		}
	}
	return false;
}

void UOfficeStageProgressManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ProgressData = FStageProgressData();
	ProgressData.InitializeSteps();

	// 특성/이벤트 핸들러 생성
	TraitEventHandler = NewObject<UProjectTraitEventHandler>(this);

	// Operation 완료 델리게이트 바인딩
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnOperationCompleted.AddDynamic(this, &UOfficeStageProgressManager::OnOperationCompletedHandler);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Initialized"));
}

void UOfficeStageProgressManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// CurrentManagedBuildingIndex 는 MainMap→OfficeMap 전환 시 GI 에 설정됨.
	// Subsystem::Initialize 시점엔 World 세팅만 끝난 상태라 빌딩 인덱스 미확정 가능성 있어,
	// OnWorldBeginPlay (Actors BeginPlay 직후) 에서 로드.
	LoadTierProgressFromSave();
}

void UOfficeStageProgressManager::LoadTierProgressFromSave()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0) return;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;

	if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
	{
		if (const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
		{
			TierProgress = Office->TierProgress;
			UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] TierProgress loaded for Building %d (Tier=%d)"),
				BuildingIndex, TierProgress.CurrentTier);
		}
	}
}

void UOfficeStageProgressManager::SyncTierProgressToSave()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0) return;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;

	if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
	{
		FOfficeSaveData& Office = SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingIndex);
		Office.TierProgress = TierProgress;
	}
	SaveMgr->SaveGameData();
}

void UOfficeStageProgressManager::Deinitialize()
{
	StopTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EffectDelayTimerHandle);
		World->GetTimerManager().ClearTimer(LaunchPopupTimerHandle);
		World->GetTimerManager().ClearTimer(FeverTimerHandle);
	}
	bFeverActive = false;

	// Operation 완료 델리게이트 언바인딩
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnOperationCompleted.RemoveAll(this);
		}
	}

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Deinitialized"));
}

// ===== 스테이지 시작/종료 =====

void UOfficeStageProgressManager::SelectProject(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber)
{
	// 이전 Operation 정리
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (GI)
	{
		int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();

		UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
		if (OpMgr)
		{
			OpMgr->EndAllOperationsByBuilding(BuildingIndex);
		}

		USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
		if (SaveMgr)
		{
			USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
			if (SaveData)
			{
				FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
				if (OfficeData)
				{
					OfficeData->bHasActiveOperation = false;
					OfficeData->CurrentOperation = FOperationData();
					UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Cleared previous Operation data for Building %d"), BuildingIndex);
				}
			}
		}
	}

	if (bStageInProgress)
	{
		EndCurrentStage();
	}

	// 진행 데이터 초기화
	ProgressData = FStageProgressData();
	ProgressData.ProjectNumber = ProjectNumber;
	ProgressData.StageNumber = StageNumber;
	ProgressData.ProjectID = ProjectData.ProjectIndex;
	ProgressData.ProjectName = ProjectData.ProjectName.ToString();
	ProgressData.CompanyType = ProjectData.CompanyType;
	ProgressData.bIsManufacturing = IsManufacturingType(ProjectData.CompanyType);
	ProgressData.CurrentStep = 0; // idle 상태로 시작

	// GDS 발견형 메타 — board/synth 양쪽 경로에서 ProjectData를 따라옴.
	// StepDuration 저장 → ResolveStepDuration이 우선 사용(합성 프로젝트가 테이블 재조회 없이 올바른 길이 확보).
	ProgressData.Genre = ProjectData.Genre;
	ProgressData.Material = ProjectData.Material;
	ProgressData.StepDuration = ProjectData.Duration;

	// 부스트 도박 상태 리셋 (새 프로젝트마다 1회)
	bBoostGamblePending = false;
	bBoostGambleDone = false;

	// Steps = 프로젝트 weight>0 직능만 동적 N칸 (출시는 스텝 아닌 별도 모달). 6가중치 → 직능 스텝.
	const TArray<int32> Weights = {
		ProjectData.Weight_Plan, ProjectData.Weight_Dev, ProjectData.Weight_Graphics,
		ProjectData.Weight_Sound, ProjectData.Weight_Server, ProjectData.Weight_QA
	};
	// 목표 기준 = 프로젝트 규모. 식은 FStageProgressData 가 소유 — 피치 카드가 착수 전에 같은 수치를 보여줘야 한다.
	const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(
		ProjectData.RequiredScore_Step1, ProjectData.RequiredScore_Step2, ProjectData.RequiredScore_Step3);
	ProgressData.InitializeDisciplineSteps(Weights, TargetBase);

	// 표시명 = 산업별 직능 DT(DT_DisciplineDisplay) — 슬롯 의미는 enum 고정, 표시만 산업별 재해석. 최소 통과 = 목표 절반.
	UTableManagerSubsystem* DisplayTableMgr = GetTableManager();
	for (FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE) { continue; }
		if (DisplayTableMgr)
		{
			S.StepName = DisplayTableMgr->GetDisciplineDisplayName(ProgressData.CompanyType, S.DisciplineSlot).ToString();
		}
		S.MinimumScore = S.TargetScore * FStageProgressData::MinimumScoreRatio;
	}

	bStageInProgress = true;

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Selected Project %d (Stage %d): %s"),
		ProjectNumber, StageNumber, *ProgressData.ProjectName);

	OnProjectSelected.Broadcast();
}

void UOfficeStageProgressManager::StartNewStage(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber)
{
	SelectProject(ProjectData, ProjectNumber, StageNumber);

	// 직원들에게 Stage 모드 알림
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Stage);

	// 즉시 동시 타이머 시작 (이펙트 표시)
	StartSimultaneousTimer(true);
}

void UOfficeStageProgressManager::EndCurrentStage()
{
	StopTimer();
	EndFever();
	bStageInProgress = false;

	if (TraitEventHandler)
	{
		TraitEventHandler->ClearAll();
	}

	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Idle);

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Ended Stage %d"),
		ProgressData.StageNumber);
}

// ===== 동시 진행 타이머 =====

void UOfficeStageProgressManager::StartSimultaneousTimer(bool bShowEffect)
{
	if (ProgressData.bIsTimerRunning)
	{
		return;
	}

	if (bShowEffect)
	{
		// UI에 이펙트 표시 요청
		OnStepStartEffect.Broadcast();

		const float EffectDuration = GetDefault<UOfficeStageTimingSettings>()->StepStartEffectDuration;

		// ini 로 0 을 넣으면 SetTimer 가 콜백을 영영 안 부른다 → 즉시 진행으로 폴백
		if (EffectDuration <= KINDA_SMALL_NUMBER)
		{
			StartSimultaneousTimerInternal();
			return;
		}

		// 이펙트 먼저 보여주고 EffectDuration 후 타이머 시작
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				EffectDelayTimerHandle,
				this,
				&UOfficeStageProgressManager::StartSimultaneousTimerInternal,
				EffectDuration,
				false
			);
		}

		UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Simultaneous timer effect started (%.1fs delay)"),
			EffectDuration);
	}
	else
	{
		StartSimultaneousTimerInternal();
	}
}

void UOfficeStageProgressManager::StartSimultaneousTimerInternal()
{
	// 베이스: 티어/프로젝트별 해석값. 프로젝트형은 트레이트 보정 반영
	float Duration = ResolveStepDuration();
	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType))
	{
		Duration = TraitEventHandler->GetEffectiveTimerDuration(Duration);
	}
	BeginTimerRun(Duration, /*bResetScores=*/true);
}

void UOfficeStageProgressManager::BeginTimerRun(float Duration, bool bResetScores)
{
	CachedTotalDuration = Duration;
	ProgressData.RemainingTime = Duration;
	ProgressData.CurrentStep = 1; // 동시 타이머 진행 중
	ProgressData.bIsTimerRunning = true;

	// 컨티뉴 재개는 누적 점수를 유지한다 — 미달 직능만 더 채우면 통과
	if (bResetScores)
	{
		for (FStepRoundData& S : ProgressData.Steps)
		{
			S.AcquiredScore = 0.0f;
			S.bIsCompleted = false;
		}
	}
	else
	{
		for (FStepRoundData& S : ProgressData.Steps)
		{
			S.bIsCompleted = false;
		}
	}

	// 타이머 시작 (0.1초 간격 반복)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SimultaneousTimerHandle,
			this,
			&UOfficeStageProgressManager::SimultaneousTimerTick,
			TimerTickInterval,
			true
		);
	}

	// 직원들 Stage 모드로 전환
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Stage);

	// 초기 델리게이트 브로드캐스트
	OnTimeUpdated.Broadcast(ProgressData.RemainingTime);
	BroadcastDisciplineScores();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Simultaneous timer started (%.1fs)"), Duration);
}

void UOfficeStageProgressManager::StopTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SimultaneousTimerHandle);
	}
	ProgressData.bIsTimerRunning = false;
}

void UOfficeStageProgressManager::SimultaneousTimerTick()
{
	if (!bStageInProgress || !ProgressData.bIsTimerRunning)
	{
		return;
	}

	// 프로젝트형: 이벤트 일시정지 처리
	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType) && TraitEventHandler->IsEventPaused())
	{
		TraitEventHandler->TickPause(TimerTickInterval);
		return; // 타이머 카운트다운 건너뜀
	}

	// 부스트 도박 모달 표시 중 — 개발 타이머 정지 (모달이 자체 카운트다운을 돎)
	if (bBoostGamblePending)
	{
		return;
	}

	ProgressData.RemainingTime -= TimerTickInterval;

	// 시간 업데이트 브로드캐스트 (UI 타이머 바 갱신)
	OnTimeUpdated.Broadcast(ProgressData.RemainingTime);

	// 프로젝트형: 이벤트 트리거 체크
	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType) && CachedTotalDuration > 0.0f)
	{
		float ElapsedRatio = 1.0f - (ProgressData.RemainingTime / CachedTotalDuration);
		if (TraitEventHandler->CheckEventTrigger(ElapsedRatio))
		{
			TraitEventHandler->SetEventPaused(true);
			OnProjectEventTriggered.Broadcast(TraitEventHandler->GetPendingEvent());
		}
	}

	// 부스트 도박 트리거 — 개발 ~40% 지점, 프로젝트당 1회. 시나리오/오즈는 DT_BoostGamble(산업별) 주도.
	// 티어 게이트는 여기서만 — 모달 쪽에서 막으면 bBoostGamblePending 이 남아 개발 타이머가 영구 정지한다.
	if (!bBoostGambleDone && CachedTotalDuration > 0.0f && TierProgress.CurrentTier >= BoostMinTier)
	{
		const float BoostRatio = 1.0f - (ProgressData.RemainingTime / CachedTotalDuration);
		if (BoostRatio >= BoostTriggerRatio)
		{
			bBoostGambleDone = true; // 산업 시나리오 유무와 무관하게 1회 소진 (매 틱 재시도 방지)
			UTableManagerSubsystem* TableMgr = GetTableManager();
			if (TableMgr && TableMgr->GetRandomBoostGamble(ProgressData.CompanyType, CurrentBoostGamble))
			{
				bBoostGamblePending = true;
				OnBoostGambleRequested.Broadcast();
			}
		}
	}

	// 타이머 종료 체크
	if (ProgressData.RemainingTime <= 0.0f)
	{
		ProgressData.RemainingTime = 0.0f;
		OnSimultaneousTimerEnded();
	}
}

void UOfficeStageProgressManager::OnSimultaneousTimerEnded()
{
	StopTimer();
	EndFever();  // 개발 종료 시 피버 해제 (결과 모달로 새어나가지 않게)

	// 전 직능 스텝 완료 표시
	for (FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot != INDEX_NONE) { S.bIsCompleted = true; }
	}
	// 달성률 캐시 스냅샷 (report 전파용 — rate 배열화 T4에서 배선, 현재는 앞 3직능 가드 캐시)
	if (ProgressData.Steps.Num() >= 1) { ProgressData.Step1AchievementRate = ProgressData.Steps[0].GetAchievementRate(); }
	if (ProgressData.Steps.Num() >= 2) { ProgressData.Step2AchievementRate = ProgressData.Steps[1].GetAchievementRate(); }
	if (ProgressData.Steps.Num() >= 3) { ProgressData.Step3AchievementRate = ProgressData.Steps[2].GetAchievementRate(); }

	// 리뷰 /40 + 발견 기록 (LaunchConfirm 모달이 발표, StartOperation이 피크에 주입)
	ComputeReviewAndDiscovery();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Simultaneous timer ended - Planning: %.1f%%, Dev: %.1f%%, QA: %.1f%%"),
		ProgressData.Step1AchievementRate * 100.0f,
		ProgressData.Step2AchievementRate * 100.0f,
		ProgressData.Step3AchievementRate * 100.0f);

	// Step별 경험치 분배 (3개 동시에)
	LastRoundEmployeeExp = 0.0f;
	DistributeStepExperience(1);
	DistributeStepExperience(2);
	DistributeStepExperience(3);

	// M10 진행 중에만 — 첫 프로젝트는 반드시 직원을 1회 레벨업시킨다(M9 직능 [+] 활성 선행)
	if (UCGGameInstance* TutorialGI = UCGGameInstance::GetInstance())
	{
		if (UMissionManagerSubsystem* MissionMgr = TutorialGI->GetSubsystem<UMissionManagerSubsystem>())
		{
			if (MissionMgr->GetActiveConditionType() == EMissionConditionType::LaunchFirstInHouseProject)
			{
				// LastRoundEmployeeExp 에 더하지 않는다 — M10 의 각본된 보정이지 이번 판이 벌어준 몫이 아니다
				TopUpExperienceForGuaranteedLevelUp(GetEmployeeManager(), TutorialGI->GetCurrentManagedBuildingIndex());
			}
		}
	}

	// 1) 직원 환호 — 수준 미달이면 축하 연출을 내지 않는다(실패 화면과 톤 충돌)
	if (!IsLaunchBlocked())
	{
		NotifyEmployeesCheerSitting();
	}

	// 2) 라운드 종료 통지는 실패에도 보낸다 — 수신부가 라스트스퍼트 펄스 원복을 겸하는데,
	//    컨티뉴는 BeginTimerRun 으로 직행해 OnStepStartEffect 를 안 쏘므로 다른 원복 기회가 없다.
	//    축하 연출을 낼지는 수신부가 IsLaunchBlocked 로 판단한다.
	OnStageResultEffect.Broadcast();

	// 3) 팝업 대기 (환호+이펙트 중 더 긴 쪽에 맞춤 — 기본 3초, ini 튜닝 가능)
	const float PopupDelay = GetDefault<UOfficeStageTimingSettings>()->LaunchPopupDelay;
	if (PopupDelay <= KINDA_SMALL_NUMBER)
	{
		ShowLaunchPopupAfterEffect();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			LaunchPopupTimerHandle,
			this,
			&UOfficeStageProgressManager::ShowLaunchPopupAfterEffect,
			PopupDelay,
			false
		);
	}
}

void UOfficeStageProgressManager::ShowLaunchPopupAfterEffect()
{
	// 직원 Idle로 전환
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Idle);

	// FSM 전환: Developing → LaunchPending
	if (Lifecycle == EProjectLifecycle::Developing)
	{
		TransitionTo(EProjectLifecycle::LaunchPending);
	}

	// 결과 모달 표시 (OfficeMainWidget → LaunchConfirmWidget). 탭 시 RequestLaunchConfirm →
	// InHouse=StartOperation→Operating / 수주=계약금→Idle
	OnSimultaneousTimerEnd.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Result modal triggered"));
}

// ===== 출시/재도전/포기 =====
// StartLaunch / HandleLaunchDismissed는 레거시 진입점 — RequestLaunchConfirm/Cancel로 위임

void UOfficeStageProgressManager::StartLaunch()
{
	// FSM 인텐트로 위임 — 모든 모드별 분기/리셋/전환 처리
	RequestLaunchConfirm();

	// 출시 후 상태 즉시 저장
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

void UOfficeStageProgressManager::HandleLaunchDismissed()
{
	// FSM 인텐트로 위임
	RequestLaunchCancel();

	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

// ===== 출시 실패 / 컨티뉴 =====

bool UOfficeStageProgressManager::IsLaunchBlocked() const
{
	// 품질 판정 대상은 프로젝트형 자체개발뿐 — 제조업 양산은 별도 플로우
	if (ProgressData.bIsManufacturing || ProgressData.ActiveMode != EProjectMode::InHouse)
	{
		return false;
	}

	return !ProgressData.MeetsMinimumClearScore();
}

int32 UOfficeStageProgressManager::GetRetryDiamondCost() const
{
	return GetDefault<UOfficeLaunchFailureSettings>()->RetryDiamondCost;
}

bool UOfficeStageProgressManager::CanRetryDevelopment() const
{
	return IsLaunchBlocked() && ProgressData.RetryCount <= 0;
}

bool UOfficeStageProgressManager::TryRetryDevelopment()
{
	if (Lifecycle != EProjectLifecycle::LaunchPending || !CanRetryDevelopment())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] TryRetryDevelopment ignored - 게이트 미충족 (Lifecycle=%s, Retry=%d)"),
			*LexToString(Lifecycle), ProgressData.RetryCount);
		return false;
	}

	const int32 Cost = GetRetryDiamondCost();
	UResourceItemManager* ResMgr = GetResourceItemManager();
	if (!ResMgr || !ResMgr->HasResource(EResourceType::Diamond, Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[Lifecycle] TryRetryDevelopment 취소 - 다이아 부족(%d)"), Cost);
		return false;
	}
	ResMgr->SpendResource(EResourceType::Diamond, Cost);

	++ProgressData.RetryCount;

	// 원 개발 시간의 일부만 추가 — 전체 재실행이면 실패의 무게가 사라진다
	float Duration = ResolveStepDuration();
	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType))
	{
		Duration = TraitEventHandler->GetEffectiveTimerDuration(Duration);
	}
	Duration *= GetDefault<UOfficeLaunchFailureSettings>()->RetryDurationRatio;

	TransitionTo(EProjectLifecycle::Developing);
	bStageInProgress = true;
	BeginTimerRun(Duration, /*bResetScores=*/false);

	UE_LOG(LogTemp, Log, TEXT("[Lifecycle] 컨티뉴 개시 - 다이아 %d 소모, 추가 %.1f초"), Cost, Duration);
	return true;
}


void UOfficeStageProgressManager::OnStageCompleted()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Stage %d Complete! Quality: %s (%.2f)"),
		ProgressData.StageNumber,
		*QualityGradeToAlphabetString(ProgressData.CalculateQualityGrade()),
		ProgressData.CalculateQualityScore());

	OnStageComplete.Broadcast();
	bStageInProgress = false;
}

void UOfficeStageProgressManager::OnOperationCompletedHandler(int32 BuildingID, const FOperationData& Data)
{
	// FSM 인텐트로 위임 — Operating → ReportPending 전환
	NotifyOperationCompleted();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Operation completed for Building %d → ReportPending"), BuildingID);
}

// ===== 점수 계산 =====

// 현재 오피스 빌딩의 ProjectYield 강화 배율 조회 (SaveData 기반, AddDisciplineContribution/AddBonusScore 공용)
static float GetCurrentBuildingProjectYieldMultiplier()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 1.0f;
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return 1.0f;
	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return 1.0f;

	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0) return 1.0f;

	for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
	{
		if (B.BuildingIndex == BuildingIndex)
		{
			const int32 Lv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::ProjectYield);
			return UBuildingEnhancementHelper::CalculateEffectMultiplier(
				EBuildingEnhancementType::ProjectYield, Lv);
		}
	}
	return 1.0f;
}

float UOfficeStageProgressManager::GetCriticalChance() const
{
	// [치트] 강제 오버라이드가 켜져 있으면 기본 크리율/특성/피버 전부 무시하고 그 값 반환
	if (CritChanceOverride >= 0.0f) { return FMath::Clamp(CritChanceOverride, 0.0f, 1.0f); }

	// 기본 크리율 + 현재 관리 건물의 Fortune 특성 가산 (factor 아닌 확률 가산)
	float Chance = CriticalChance;
	if (UCGGameInstance* CGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* TraitMgr = CGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			Chance += TraitMgr->GetAggregatedTraitPercent(CGI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::CritChance) / 100.0f;
		}
	}
	// 피버타임 — 전역 크리율 일시 가산 (종료 시 자동 해제)
	if (bFeverActive)
	{
		Chance += FeverCritBonus;
	}
	return FMath::Clamp(Chance, 0.0f, 1.0f);
}

void UOfficeStageProgressManager::StartFever()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const bool bWasActive = bFeverActive;
	bFeverActive = true;

	// 지속 타이머 — 이미 활성 중 재발동이면 배수 중첩 없이 시간만 갱신(단일 전역 배수 유지)
	World->GetTimerManager().SetTimer(
		FeverTimerHandle, this, &UOfficeStageProgressManager::EndFever, FeverDuration, false);

	if (!bWasActive)
	{
		OnFeverChanged.Broadcast(true);
		UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Fever STARTED (%.1fs, x%.1f, crit+%.0f%%)"),
			FeverDuration, FeverOutputMult, FeverCritBonus * 100.0f);
	}
}

void UOfficeStageProgressManager::EndFever()
{
	if (!bFeverActive) return;
	bFeverActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeverTimerHandle);
	}
	OnFeverChanged.Broadcast(false);
	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Fever ENDED"));
}

void UOfficeStageProgressManager::AddDisciplineContribution(int32 DisciplineSlot, float Score)
{
	if (!ProgressData.bIsTimerRunning || ProgressData.Steps.Num() == 0)
	{
		return;
	}

	// 이벤트/부스트 팝업 진행 중에는 직원 기여도 차단 (타이머 일시정지와 동기화)
	if (bBoostGamblePending || (TraitEventHandler && IsProjectType(ProgressData.CompanyType) && TraitEventHandler->IsEventPaused()))
	{
		return;
	}

	// 빌딩 강화 ProjectYield 배율 적용 — 같은 15초에 누적되는 점수 양 증가 (BALANCE.md Phase 2)
	const float OutputMult = GetCurrentBuildingProjectYieldMultiplier();

	for (FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot == DisciplineSlot)
		{
			S.AcquiredScore += Score * OutputMult;
			break;
		}
	}

	BroadcastDisciplineScores();
}

void UOfficeStageProgressManager::RequestScoreOrb(FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount)
{
	OnScoreOrbRequested.Broadcast(WorldPos, StepNumber, Score, bIsCritical, OrbCount);
}

void UOfficeStageProgressManager::AddBonusScore(float BonusScore)
{
	if (!ProgressData.bIsTimerRunning || ProgressData.Steps.Num() == 0)
	{
		return;
	}

	// 이벤트/부스트 팝업 진행 중에는 보너스도 차단
	if (bBoostGamblePending || (TraitEventHandler && IsProjectType(ProgressData.CompanyType) && TraitEventHandler->IsEventPaused()))
	{
		return;
	}

	// 보너스는 활성 직능 스텝에 균등 배분 + ProjectYield 배율 적용
	int32 ActiveCount = 0;
	for (const FStepRoundData& S : ProgressData.Steps) { if (S.DisciplineSlot != INDEX_NONE) { ++ActiveCount; } }
	if (ActiveCount == 0) { return; }

	const float OutputMult = GetCurrentBuildingProjectYieldMultiplier();
	const float ScorePerStep = (BonusScore / ActiveCount) * OutputMult;
	for (FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot != INDEX_NONE) { S.AcquiredScore += ScorePerStep; }
	}

	BroadcastDisciplineScores();
}

void UOfficeStageProgressManager::BroadcastDisciplineScores()
{
	TArray<float> Scores, Targets;
	for (const FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE) { continue; }
		Scores.Add(S.AcquiredScore);
		Targets.Add(S.TargetScore);
	}
	OnDisciplineScoresUpdated.Broadcast(Scores, Targets);
}

void UOfficeStageProgressManager::DistributeStepExperience(int32 CompletedStep)
{
	float BaseExp = 0.0f;
	switch (CompletedStep)
	{
	case 1: BaseExp = 20.0f; break;
	case 2: BaseExp = 30.0f; break;
	case 3: BaseExp = 25.0f; break;
	default: return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();

	UEmployeeManager* EmployeeMgr = GetEmployeeManager();
	if (EmployeeMgr)
	{
		LastRoundEmployeeExp += EmployeeMgr->DistributeExperienceToBuilding(BuildingIndex, BaseExp, EQualityGrade::B);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Step %d - Employee EXP %.1f to building %d"),
		CompletedStep, BaseExp, BuildingIndex);
}

float UOfficeStageProgressManager::CalculateRandomFactor() const
{
	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType))
	{
		return TraitEventHandler->GetRandomVariance();
	}
	return FMath::RandRange(0.8f, 1.2f);
}

float UOfficeStageProgressManager::CheckCritical() const
{
	float EffectiveChance = CriticalChance;
	float CritMultiplier = 2.0f;

	if (TraitEventHandler && IsProjectType(ProgressData.CompanyType))
	{
		EffectiveChance *= TraitEventHandler->GetCriticalChanceMultiplier();
		CritMultiplier = TraitEventHandler->GetCriticalMultiplier(2.0f);
	}

	float Roll = FMath::FRand();
	if (Roll < EffectiveChance)
	{
		return CritMultiplier;
	}
	return 1.0f;
}

// ===== 저장/로드 =====

void UOfficeStageProgressManager::SetProgressData(const FStageProgressData& InData)
{
	ProgressData = InData;

	// CurrentStep 범위 보정 (0=idle, 1=진행 중)
	// 로드 시 진행 중(1)이었으면 idle(0)로 리셋 (타이머는 짧으므로 처음부터)
	if (ProgressData.CurrentStep >= 1)
	{
		ProgressData.CurrentStep = 0;
		ProgressData.bIsTimerRunning = false;
		ProgressData.RemainingTime = ResolveStepDuration();
	}

	// Steps 배열이 비어있으면 초기화
	if (ProgressData.Steps.Num() == 0)
	{
		ProgressData.InitializeSteps();
		UE_LOG(LogTemp, Warning, TEXT("[OfficeStageProgressManager] SetProgressData - Steps was empty, initialized"));
	}

	// ProjectID가 유효하면 진행 중 상태 복원
	bStageInProgress = (ProgressData.ProjectID > 0);

	// === Lifecycle 재조정 ===
	// 세이브 데이터의 stale 플래그를 실제 게임 상태에 맞게 보정
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	int32 BuildingIndex = GI ? GI->GetCurrentManagedBuildingIndex() : -1;
	UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;
	const bool bHasActiveOp = (OpMgr && BuildingIndex >= 0 && OpMgr->GetOperationByBuildingID(BuildingIndex) != nullptr);
	const bool bHasPendingReport = (OpMgr && BuildingIndex >= 0 && OpMgr->HasPendingReport(BuildingIndex));

	EProjectLifecycle DesiredState = EProjectLifecycle::Idle;
	if (bHasActiveOp)
	{
		DesiredState = EProjectLifecycle::Operating;
	}
	else if (bHasPendingReport)
	{
		DesiredState = EProjectLifecycle::ReportPending;
	}
	else
	{
		// Operation 없고 Pending Report도 없으면 무조건 Idle (Developing/LaunchPending도 세션 간엔 보존 안 함)
		DesiredState = EProjectLifecycle::Idle;
		// stale ProgressData 정리
		ProgressData.ActiveMode = EProjectMode::None;
		ProgressData.ProjectID = 0;
		ProgressData.ProjectNumber = 0;
		ProgressData.ProjectName.Reset();
		ProgressData.bIsTimerRunning = false;
		bStageInProgress = false;
	}

	if (Lifecycle != DesiredState)
	{
		const EProjectLifecycle Old = Lifecycle;
		Lifecycle = DesiredState;
		UE_LOG(LogTemp, Log, TEXT("[Lifecycle] Reconciled %s → %s on load"),
			*LexToString(Old), *LexToString(DesiredState));
		OnLifecycleChanged.Broadcast(Old, DesiredState);
	}

	// 로드 후 UI 상태 갱신용 델리게이트
	OnProgressDataRestored.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] SetProgressData - Step=%d, InProgress=%s, Steps=%d"),
		ProgressData.CurrentStep, bStageInProgress ? TEXT("true") : TEXT("false"), ProgressData.Steps.Num());
}

// ===== 매니저 캐시 =====

UEmployeeManager* UOfficeStageProgressManager::GetEmployeeManager() const
{
	if (!CachedEmployeeManager)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedEmployeeManager = GI->GetSubsystem<UEmployeeManager>();
			}
		}
	}
	return CachedEmployeeManager;
}

UResourceItemManager* UOfficeStageProgressManager::GetResourceItemManager() const
{
	if (!CachedResourceItemManager)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedResourceItemManager = GI->GetSubsystem<UResourceItemManager>();
			}
		}
	}
	return CachedResourceItemManager;
}

UTableManagerSubsystem* UOfficeStageProgressManager::GetTableManager() const
{
	if (!CachedTableManager)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedTableManager = GI->GetSubsystem<UTableManagerSubsystem>();
			}
		}
	}
	return CachedTableManager;
}

float UOfficeStageProgressManager::ResolveStepDuration() const
{
	// 합성 프로젝트(테이블 인덱스 무관) 대비 — SelectProject가 저장한 길이 우선
	if (ProgressData.StepDuration > 0.0f)
	{
		return ProgressData.StepDuration;
	}

	UTableManagerSubsystem* TableMgr = GetTableManager();
	if (!TableMgr || ProgressData.CompanyType == ECompanyType::None || ProgressData.ProjectID <= 0)
	{
		return DefaultStepDuration;
	}

	bool bOK = false;
	const FProjectData Proj = TableMgr->GetProjectData(ProgressData.CompanyType, ProgressData.ProjectID, bOK);
	if (bOK && Proj.Duration > 0.0f)
	{
		return Proj.Duration;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeStageProgressManager] Duration missing for (%s, %d), using fallback %.1f"),
		*UEnum::GetValueAsString(ProgressData.CompanyType), ProgressData.ProjectID, DefaultStepDuration);
	return DefaultStepDuration;
}

// ===== 직원 행동 모드 =====

void UOfficeStageProgressManager::NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode NewMode)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Operation 모드일 때 1인당 수익 계산
	float PerEmployeeIncome = 0.0f;
	if (NewMode == EEmployeeBehaviorMode::Operation)
	{
		UCGGameInstance* GI = UCGGameInstance::GetInstance();
		if (GI)
		{
			int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
			if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
			{
				if (FOperationData* OpData = OpMgr->GetOperationByBuildingID(BuildingIndex))
				{
					int32 EmployeeCount = 0;
					for (TActorIterator<AOfficeworker> CountIt(World); CountIt; ++CountIt)
					{
						AOfficeworker* W = *CountIt;
						if (W && W->BehaviorComponent && !W->bIsPortraitMode)
						{
							EmployeeCount++;
						}
					}

					if (EmployeeCount > 0)
					{
						PerEmployeeIncome = OpData->ActualRevenuePerSecond / static_cast<float>(EmployeeCount);
					}
				}
			}
		}
	}

	int32 NotifiedCount = 0;

	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		AOfficeworker* Worker = *It;
		if (Worker && Worker->BehaviorComponent && !Worker->bIsPortraitMode)
		{
			Worker->BehaviorComponent->SetBehaviorMode(NewMode);

			if (NewMode == EEmployeeBehaviorMode::Operation && PerEmployeeIncome > 0.0f)
			{
				Worker->BehaviorComponent->SetOperationIncome(PerEmployeeIncome);
			}

			NotifiedCount++;
		}
	}

	const TCHAR* ModeNames[] = { TEXT("Idle"), TEXT("Stage"), TEXT("Operation") };
	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Notified %d employees with mode: %s"),
		NotifiedCount, ModeNames[(int32)NewMode]);
}

void UOfficeStageProgressManager::NotifyEmployeesFatigueFrozen(bool bFrozen)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		AOfficeworker* Worker = *It;
		if (Worker && Worker->BehaviorComponent && !Worker->bIsPortraitMode)
		{
			Worker->BehaviorComponent->SetFatigueFrozen(bFrozen);
		}
	}
}

void UOfficeStageProgressManager::NotifyEmployeesCheerSitting()
{
	UWorld* World = GetWorld();
	if (!World) return;

	int32 NotifiedCount = 0;

	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		AOfficeworker* Worker = *It;
		if (Worker && Worker->BehaviorComponent && !Worker->bIsPortraitMode)
		{
			Worker->BehaviorComponent->StartCheerSitting();
			NotifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] %d employees started CheerSitting"), NotifiedCount);
}

void UOfficeStageProgressManager::NotifyEmployeesCheerStandUp()
{
	UWorld* World = GetWorld();
	if (!World) return;

	int32 NotifiedCount = 0;

	for (TActorIterator<AOfficeworker> It(World); It; ++It)
	{
		AOfficeworker* Worker = *It;
		if (Worker && Worker->BehaviorComponent && !Worker->bIsPortraitMode)
		{
			Worker->BehaviorComponent->StartCheerStandUp();
			NotifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] %d employees started CheerStandUp"), NotifiedCount);
}

void UOfficeStageProgressManager::ComputeReviewAndDiscovery()
{
	// 리뷰/발견은 자체개발 프로젝트형 전용 (수주=계약금, 제조=양산)
	if (!IsProjectType(ProgressData.CompanyType) || ProgressData.ActiveMode != EProjectMode::InHouse)
	{
		return;
	}

	// 비평가 4점수 = 품질(개발 결과) + 랜덤. 각 1~10, 합 /40.
	// R&D 특성 — 비평가가 보는 품질에도 반영(운영 품질과 같은 값이어야 리뷰가 결과와 어긋나지 않는다)
	float QualityTraitMult = 1.0f;
	if (UCGGameInstance* RvGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* RvTraitMgr = RvGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			QualityTraitMult = RvTraitMgr->GetTraitFactor(RvGI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::Quality);
		}
	}
	const float Quality = FMath::Clamp(ProgressData.CalculateQualityScore() * QualityTraitMult, 0.5f, 2.0f);
	// 비평가별 품질 Q_i — 관심 직능(Focus)만의 가중 달성률(CalculateQualityScore 와 같은 0.5~2.0 계약).
	// Focus 빈값/이 프로젝트에 비활성 = 전체 품질(페널티 아님). 운영수익과는 절연.
	// ⚠ 표시 전용이 아니다 — ReviewScore 는 출시 전리품 밴드(ReviewScoreToTableKey 32/24)의 입력이라 이 식이 티켓 수급을 움직인다.
	UTableManagerSubsystem* FocusTableMgr = GetTableManager();
	TArray<TArray<EProductionDiscipline>> CriticFocus;
	if (FocusTableMgr) { CriticFocus = FocusTableMgr->GetCriticFocus(ProgressData.CompanyType); }

	ProgressData.CriticScores.Empty();
	int32 Total = 0;
	float CriticQ[4] = { Quality, Quality, Quality, Quality };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (CriticFocus.IsValidIndex(Index) && CriticFocus[Index].Num() > 0)
		{
			float WeightedSum = 0.0f;
			float WeightTotal = 0.0f;
			for (const FStepRoundData& S : ProgressData.Steps)
			{
				if (S.DisciplineSlot == INDEX_NONE || S.Weight <= 0) { continue; }
				if (!CriticFocus[Index].Contains(static_cast<EProductionDiscipline>(S.DisciplineSlot))) { continue; }
				WeightedSum += FMath::Min(S.GetAchievementRate(), 2.0f) * S.Weight;
				WeightTotal += S.Weight;
			}
			if (WeightTotal > 0.0f)
			{
				CriticQ[Index] = FMath::Clamp(WeightedSum / WeightTotal * QualityTraitMult, 0.5f, 2.0f);
			}
		}
		// 품질 단독으로 1~10 전 구간을 쓴다 (궁합 보정 제거분 흡수). Q 0.5->2.0 / 2.0->9.5 => 총점 8~38.
		// 노이즈 ±0.8: 직능 연동이 비평가 간 분산을 이미 만든다
		static constexpr float CriticScoreSlope = 5.0f;
		static constexpr float CriticScoreIntercept = -0.5f;
		const int32 Score = FMath::Clamp(
			FMath::RoundToInt(CriticScoreIntercept + CriticQ[Index] * CriticScoreSlope + FMath::FRandRange(-0.8f, 0.8f)), 1, 10);
		ProgressData.CriticScores.Add(Score);
		Total += Score;
	}
	ProgressData.ReviewScore = Total;

	// (장르,소재) 발견 기록 — 등급이 이 순간 공개됨
	ProgressData.bFirstDiscovery = false;
	if (ProgressData.Genre != NAME_None)
	{
		const FString ComboKey = FString::Printf(TEXT("%d|%s|%s"),
			static_cast<int32>(ProgressData.CompanyType),
			*ProgressData.Genre.ToString(), *ProgressData.Material.ToString());

		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
			{
				if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
				{
					if (!SaveData->GameData.DiscoveredCombos.Contains(ComboKey))
					{
						ProgressData.bFirstDiscovery = true;
						SaveData->GameData.DiscoveredCombos.Add(ComboKey);
					}

					// 출시작 이력 (도감 [출시작] 탭) — 같은 프로젝트를 다시 개발하면 행을 늘리지 않고 갱신.
					// 평점·등급은 최고 기록 유지, 누적매출은 이어서 쌓는다.
					const int32 ShippedIndex = ProgressData.ProjectNumber;
					// EQualityGrade 는 F=0 ... S=5 순이라 uint8 비교가 곧 등급 비교다
					const EQualityGrade NewGrade = ProgressData.CalculateQualityGrade();
					FShippedProjectRecord* Existing = SaveData->GameData.ShippedProjects.FindByPredicate(
						[ShippedIndex, this](const FShippedProjectRecord& R)
						{
							return R.ProjectIndex == ShippedIndex && R.Industry == ProgressData.CompanyType;
						});

					if (Existing)
					{
						Existing->ProjectName = ProgressData.ProjectName;
						Existing->Genre = ProgressData.Genre;
						Existing->Material = ProgressData.Material;
						if (static_cast<uint8>(NewGrade) > static_cast<uint8>(Existing->QualityGrade))
						{
							Existing->QualityGrade = NewGrade;
						}
						Existing->ReviewScore = FMath::Max(Existing->ReviewScore, ProgressData.ReviewScore);
					}
					else
					{
						FShippedProjectRecord Record;
						Record.ProjectName = ProgressData.ProjectName;
						Record.Industry = ProgressData.CompanyType;
						Record.ProjectIndex = ShippedIndex;
						Record.Genre = ProgressData.Genre;
						Record.Material = ProgressData.Material;
						Record.QualityGrade = NewGrade;
						Record.ReviewScore = ProgressData.ReviewScore;
						SaveData->GameData.ShippedProjects.Add(Record);
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Review] %d/40 (critics %d/%d/%d/%d, Q %.2f/%.2f/%.2f/%.2f, base Q %.2f) grade=%s first=%d"),
		ProgressData.ReviewScore,
		ProgressData.CriticScores[0], ProgressData.CriticScores[1], ProgressData.CriticScores[2], ProgressData.CriticScores[3],
		CriticQ[0], CriticQ[1], CriticQ[2], CriticQ[3], Quality,
		*QualityGradeToAlphabetString(ProgressData.CalculateQualityGrade()), ProgressData.bFirstDiscovery ? 1 : 0);
}

void UOfficeStageProgressManager::Debug_FillScores(float Ratio)
{
	for (FStepRoundData& S : ProgressData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE) { continue; }
		S.AcquiredScore = S.TargetScore * Ratio;
	}
	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Debug_FillScores: 전 직능 %.0f%% 세팅"), Ratio * 100.0f);
}

void UOfficeStageProgressManager::BuildTierProjectEntries(ECompanyType Industry, int32 Tier, int32 CurrentTier,
	const TArray<int32>& DevelopedProjects, TArray<FProjectTierEntry>& OutEntries)
{
	OutEntries.Reset();

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) { return; }

	int32 Start = 0, End = 0;
	FProjectTierProgress::GetTierProjectRange(Tier, Start, End);
	for (int32 Idx = Start; Idx <= End; ++Idx)
	{
		bool bOk = false;
		const FProjectData Proj = TableMgr->GetProjectData(Industry, Idx, bOk);
		if (!bOk) { continue; }

		FProjectTierEntry Entry;
		Entry.ProjectIndex = Idx;
		Entry.Tier = Tier;
		Entry.Genre = Proj.Genre;
		Entry.Material = Proj.Material;
		Entry.ProjectName = Proj.ProjectName;
		Entry.Cover = Proj.Icon;
		Entry.Duration = Proj.Duration;
		Entry.bPitchReady = Proj.IsPitchReady();

		// 판정 축은 티어가 아니라 "내가 만들었는가" — 한 번 만든 건 현재 티어여도 기록이자 재개발 대상이다.
		// (구: Tier >= CurrentTier 를 통째로 Locked 처리 → 현재 티어에서 이미 개발한 것도 도감에 '?' 로 잠겨 보였음)
		const bool bDeveloped = DevelopedProjects.Contains(Idx);
		if (bDeveloped)              { Entry.State = EProjectEntryState::Discovered; }    // 개발함 = 재개발 + 도감 공개
		else if (Tier < CurrentTier) { Entry.State = EProjectEntryState::Undiscovered; }  // 지나온 미개발 = 첫 개발
		else                         { Entry.State = EProjectEntryState::Locked; }        // 현재 티어 미개발 = 기획 보드 담당 / 미래 티어 = 미해금

		OutEntries.Add(Entry);
	}
}

void UOfficeStageProgressManager::EstimatePitchEconomy(ECompanyType Industry, float ExpectedQuality, bool bTrend,
	int64& OutRevenue, int32& OutOpTimeSec, int32 ProjectNumberOverride)
{
	OutRevenue = 0;
	OutOpTimeSec = 0;

	const int32 CurTier = TierProgress.CurrentTier;

	UTableManagerSubsystem* TableMgr = GetTableManager();
	if (!TableMgr)
	{
		return;
	}

	// 규모 인덱스 = 글로벌 프로젝트 번호 (CalculateBaseRevenueForProjectNumber 와 동일 근거)
	// — 카드가 실제 행을 알면 그 행 기준(착수와 동일 근거로 추정=실제 일치). 착수 경로는 항상 유효 override 전달.
	int32 ProjectNumber = ProjectNumberOverride;
	if (ProjectNumber == INDEX_NONE)
	{
		int32 BandStart = 0, BandEnd = 0;
		FProjectTierProgress::GetTierProjectRange(CurTier, BandStart, BandEnd);
		ProjectNumber = BandStart;   // 방어적 폴백 — 티어 시작 슬롯
	}
	// ⚠ ProjectOperationManager::CalculateBaseRevenueForProjectNumber 와 거울 — 한쪽만 바꾸면 표기와 실지급이 갈라진다.
	const float Base = FMath::Max(20.0f, FTierLayout::RevenueScaleIndex(ProjectNumber) * 200.0f);

	bool bProfOK = false;
	const FIndustryProfileRow Profile = TableMgr->GetIndustryProfile(Industry, bProfOK);
	const float PeakMult = bProfOK ? Profile.PeakMult : 1.0f;
	const float Leverage = bProfOK ? Profile.ReviewLeverage : 1.0f;
	const float TrendMult = bTrend ? UTrendManagerSubsystem::TrendPeakMult : 1.0f;

	// 착수 경로(StartOperation)와 같은 함수·같은 건물(GetCurrentManagedBuildingIndex)에서 읽는다 —
	// 카드 표기와 실제 결과가 구조적으로 갈릴 수 없다.
	const EQualityGrade Grade = QualityScoreToGrade(ExpectedQuality);
	// ⚠ 3번째 인자(안정성)를 빠뜨리면 카드만 특성을 무시해 착수 결과와 갈린다 — 현행 코드가 이미 이렇게 읽고 있다.
	float Stability = 0.0f;
	// 운영시간도 착수(StartOperation)와 같은 함수에서 읽는다 — 산식을 베끼면 하한/배율이 갈린다
	float OpSec = 0.0f;
	if (UCGGameInstance* EstGI = UCGGameInstance::GetInstance())
	{
		if (UProjectOperationManager* OpMgr = EstGI->GetSubsystem<UProjectOperationManager>())
		{
			const int32 EstBuildingID = EstGI->GetCurrentManagedBuildingIndex();
			Stability = OpMgr->GetRevenueStabilityFrac(EstBuildingID);
			OpSec = OpMgr->ComputeEffectiveOperationTime(Grade, ProjectNumber, EstBuildingID);
		}
	}
	const float QualityMult = UProjectOperationManager::ComputeQualityRevenueMult(Grade, Leverage, Stability);
	OutOpTimeSec = FMath::RoundToInt(OpSec);

	// 총 예상수익 — 조립식은 결과 화면(LaunchConfirmWidget)과 같은 함수가 소유한다. 항이 갈릴 수 없다.
	// RoundToInt(float) 는 int32 를 돌려줘 int64 승격을 무효화한다 — 반드시 RoundToInt64.
	OutRevenue = FMath::RoundToInt64(static_cast<double>(UProjectOperationManager::ComputeLaunchRevenue(
		Base, PeakMult, TrendMult, QualityMult, OpSec, bProfOK ? Profile.HalfLifeFrac : 0.5f)));
}

void UOfficeStageProgressManager::BuildContributorRoster(TArray<FEstimateWorkerInput>& OutRoster) const
{
	OutRoster.Reset();

	UWorld* RosterWorld = GetWorld();
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!RosterWorld || !EmpMgr)
	{
		return;
	}

	for (TActorIterator<AOfficeworker> It(RosterWorld); It; ++It)
	{
		const AOfficeworker* Worker = *It;
		if (!Worker || Worker->bIsPortraitMode || !Worker->BehaviorComponent)
		{
			continue;
		}
		// ⚠ FEmployeeInstance::bIsAssigned("이 빌딩 소속")로는 판정하지 말 것 — 좌석 유무와 다른 축이다.
		if (!Worker->GetAssignedWorkstation())
		{
			continue;
		}

		const FEmployeeInstance* Employee = EmpMgr->FindEmployee(Worker->GetEmployeeID());
		if (!Employee)
		{
			continue;
		}

		FEstimateWorkerInput Input;
		Input.Level = Employee->Level;
		Input.DisciplinePoints = Employee->DisciplinePoints;
		// 초당 산출은 SpeedFactor 에 비례한다(간격이 짧아진 만큼 Overflow 가 상쇄돼 케이던스는 빠진다).
		// 잠재큐브 ScoreMult 는 기여 1회마다 곱해지는 고정 배수라 같은 축에 함께 실린다.
		Input.OutputScale = Worker->BehaviorComponent->GetSpeedFactor()
			* UEmployeePotentialHelper::AggregateModifiers(Employee->PotentialAbility).ScoreMult;
		// 업무집중도 — 기여 경로(EmitScore)의 강제 지정 확률과 같은 식이어야 예상↔실제가 안 갈린다
		const int32 EffectiveFocus = Employee->Stats.Focus + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
		Input.FocusChance = FMath::Min(EffectiveFocus * EmployeeStatTuning::FocusPerPoint, EmployeeStatTuning::MaxFocusChance);
		OutRoster.Add(Input);
	}
}

FProjectOutlookEstimate UOfficeStageProgressManager::EstimateProjectOutlook(
	const TArray<FEstimateWorkerInput>& Roster, const FProjectData& ProjectRow) const
{
	const TArray<int32> Weights = { ProjectRow.Weight_Plan, ProjectRow.Weight_Dev, ProjectRow.Weight_Graphics,
	                                ProjectRow.Weight_Sound, ProjectRow.Weight_Server, ProjectRow.Weight_QA };
	const int32 R1 = ProjectRow.GetRequiredScore(1);
	const int32 R2 = ProjectRow.GetRequiredScore(2);
	const int32 R3 = ProjectRow.GetRequiredScore(3);
	// 빈 Duration 은 착수 때도 DefaultStepDuration 으로 돌아간다(ResolveStepDuration) — 여기서 0 을 그대로 넘기면
	// 추정기가 "추정 불가"로 떨어져 카드가 전 항목 최저값을 확신 있게 표시한다.
	const float DurationSec = (ProjectRow.Duration > 0.0f) ? ProjectRow.Duration : DefaultStepDuration;

	// 전원 공통 배수 = 현재 빌딩의 개발점수 특성 (기여 경로가 매 히트 곱하는 값과 같은 출처)
	float CommonScale = 1.0f;
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* TraitMgr = GI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			CommonScale = TraitMgr->GetTraitFactor(GI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::DevScore);
		}
	}

	return FStageProgressData::EstimateProjectOutlook(
		Roster, Weights, R1, R2, R3, DurationSec, CommonScale);
}

EDevelopStartResult UOfficeStageProgressManager::StartSelfDevelopFromProject(int32 ProjectIndex, EProjectDirection Direction,
	const FText& CustomName)
{
	UTableManagerSubsystem* TableMgr = GetTableManager();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeStageProgressManager] StartSelfDevelopFromProject: TableManager 없음"));
		return EDevelopStartResult::NoProjectRow;
	}

	// 산업 컨텍스트 (현재 진입 빌딩) — GameInstance 우선, ProgressData 폴백
	ECompanyType Industry = ECompanyType::None;
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		Industry = GI->GetCurrentBuildingCompanyType();
	}
	if (Industry == ECompanyType::None) { Industry = ProgressData.CompanyType; }
	if (Industry == ECompanyType::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeStageProgressManager] StartSelfDevelopFromProject: 산업 미확인 — 중단"));
		return EDevelopStartResult::NoIndustry;
	}

	bool bSuccess = false;
	FProjectData Synth = TableMgr->GetProjectData(Industry, ProjectIndex, bSuccess);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeStageProgressManager] StartSelfDevelopFromProject: 프로젝트 행 없음 (%s, %d)"),
			*UEnum::GetValueAsString(Industry), ProjectIndex);
		return EDevelopStartResult::NoProjectRow;
	}

	// 행의 장르/소재/VariantKey 를 그대로 사용(덮어쓰기 없음) — 카드가 보여준 정체성과 착수가 한 행으로 일치.
	// 커스텀 이름만 우선 반영(비면 행 기본명 유지).
	if (!CustomName.IsEmpty())
	{
		Synth.ProjectName = CustomName;
	}

	return LaunchDevelopFromRow(Synth, ProjectIndex, Direction, Industry, TierProgress.CurrentTier);
}

void UOfficeStageProgressManager::NotifyDevelopStartFailure(EDevelopStartResult Result)
{
	if (Result == EDevelopStartResult::Success)
	{
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr)
	{
		return;
	}

	// 행 없음/산업 미확인은 플레이어가 손쓸 수 있는 상태가 아니다 — 내부 사유를 나열하지 않고 상황만 알린다.
	UIMgr->ShowRejectNotification(
		NSLOCTEXT("StageProgress", "DevelopStartBlocked", "지금은 개발을 시작할 수 없습니다"));
}

EDevelopStartResult UOfficeStageProgressManager::LaunchDevelopFromRow(const FProjectData& Synth, int32 ScaleIndex,
	EProjectDirection Direction, ECompanyType Industry, int32 Tier)
{
	// 기존 개발 엔진 점화 (board 경로와 동일한 시퀀스, 슬롯 인덱싱만 제외)
	SelectProject(Synth, ScaleIndex, 1);

	ProgressData.ActiveMode = EProjectMode::InHouse;
	ProgressData.Direction = Direction;

	// 트렌드 매칭은 착수 순간 캡처 — 이후 로테이션과 무관하게 이 프로젝트에 고정
	if (UCGGameInstance* TrendGI = UCGGameInstance::GetInstance())
	{
		if (UTrendManagerSubsystem* TrendMgr = TrendGI->GetSubsystem<UTrendManagerSubsystem>())
		{
			ProgressData.bTrendMatched = TrendMgr->IsTrendMaterial(Industry, Synth.Material);
			TrendMgr->NotifyProjectLaunched(Industry);
		}
	}
	// Genre/Material/StepDuration 은 SelectProject 가 Synth 에서 이미 복사함

	// FSM: Idle → Developing + 타이머 점화
	TransitionTo(EProjectLifecycle::Developing);
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Stage);
	StartSimultaneousTimer(true);

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] 착수: '%s' [%s x %s] dir=%s slot=%d tier=%d trend=%d"),
		*ProgressData.ProjectName, *Synth.Genre.ToString(), *Synth.Material.ToString(),
		*UEnum::GetValueAsString(Direction), ScaleIndex, Tier,
		ProgressData.bTrendMatched ? 1 : 0);

	return EDevelopStartResult::Success;
}

void UOfficeStageProgressManager::CheckAndGrantCodexMilestone(int32 ClearedProjectIndex)
{
	bool bChanged = false;
	auto Toast = [](const FText& Msg)
	{
		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
			{
				UIMgr->ShowNotification(Msg);
			}
		}
	};

	// 티어 10/10 발견 완성 (한 티어의 프로젝트 전부 자체개발) — 다이아 지급 성공 시에만 기록/토스트.
	// (ResMgr null이면 미지급 상태를 세이브에 박제하지 않고 다음 클리어 때 재시도)
	const int32 Tier = FProjectTierProgress::GetTierForProject(ClearedProjectIndex);
	if (!TierProgress.MilestoneRewardedTiers.Contains(Tier)
		&& TierProgress.GetTierClearedCount(Tier) >= TierConstants::PROJECTS_PER_TIER)
	{
		if (UResourceItemManager* ResMgr = GetResourceItemManager())
		{
			ResMgr->StoreResource(EResourceType::Diamond, CodexTierMilestoneDiamond);
			TierProgress.MilestoneRewardedTiers.Add(Tier);
			bChanged = true;
			Toast(FText::Format(NSLOCTEXT("Codex", "TierMilestone", "{0}단계 도감 완성! 다이아 {1}개 획득"),
				FText::AsNumber(Tier), FText::AsNumber(CodexTierMilestoneDiamond)));
		}
	}

	// 전체 100개 발견 완성 — 동일하게 지급 성공 시에만 기록/토스트.
	if (!TierProgress.bAllProjectsMilestoneRewarded
		&& TierProgress.ClearedProjects.Num() >= TierConstants::MAX_TIER * TierConstants::PROJECTS_PER_TIER)
	{
		if (UResourceItemManager* ResMgr = GetResourceItemManager())
		{
			ResMgr->StoreResource(EResourceType::Diamond, CodexFullMilestoneDiamond);
			TierProgress.bAllProjectsMilestoneRewarded = true;
			bChanged = true;
			Toast(FText::Format(NSLOCTEXT("Codex", "FullMilestone", "전 프로젝트 도감 완성! 다이아 {0}개 획득"),
				FText::AsNumber(CodexFullMilestoneDiamond)));
		}
	}

	if (bChanged) { SyncTierProgressToSave(); }
}

void UOfficeStageProgressManager::HandleEventChoice(int32 ChoiceIndex)
{
	if (!TraitEventHandler) return;

	// 피버 트리거는 효과 적용 전에 캡처 (ApplyEventChoice 가 PendingEvent 를 비울 수 있음)
	const FProjectEventData& PendingEvent = TraitEventHandler->GetPendingEvent();
	const bool bFeverChoice = PendingEvent.Choices.IsValidIndex(ChoiceIndex)
		&& PendingEvent.Choices[ChoiceIndex].bTriggerFever;

	TraitEventHandler->ApplyEventChoice(ChoiceIndex, ProgressData);

	if (bFeverChoice)
	{
		StartFever();
	}

	// 일시정지가 아닌 경우 (PauseDuration 없는 선택지) → 즉시 타이머 재개
	if (!TraitEventHandler->IsEventPaused())
	{
		// 타이머는 이미 돌아가고 있음 (Tick에서 Paused 체크로 멈춤)
	}

	// 점수 변경 UI 갱신
	BroadcastDisciplineScores();

	UE_LOG(LogTemp, Log, TEXT("[OfficeStageProgressManager] Event choice %d applied"), ChoiceIndex);
}

bool UOfficeStageProgressManager::IsEventPaused() const
{
	return TraitEventHandler ? TraitEventHandler->IsEventPaused() : false;
}

bool UOfficeStageProgressManager::CanAffordCurrentBoostGamble() const
{
	if (CurrentBoostGamble.GoCost <= 0)
	{
		return true;
	}
	UResourceItemManager* ResMgr = GetResourceItemManager();
	return ResMgr && ResMgr->HasResource(EResourceType::Money, CurrentBoostGamble.GoCost);
}

void UOfficeStageProgressManager::ResolveBoostGamble(bool bGamble)
{
	if (!bBoostGamblePending)
	{
		return; // 이미 해결됨(중복 방지) — 모달 타임아웃과 버튼 클릭 경합 대비
	}
	bBoostGamblePending = false; // 개발 타이머 재개

	if (!bGamble)
	{
		OnBoostGambleResolved.Broadcast(false, false); // 안전 — 변화 없음
		return;
	}

	// 여력 없이 지른 경우(버튼 게이트 우회 등) → 안전 처리, 지출 없음 (백스톱)
	if (!CanAffordCurrentBoostGamble())
	{
		OnBoostGambleResolved.Broadcast(false, false);
		return;
	}

	// 프로젝트전용 특성 — 성공률 가산(%p). 리스크(손실폭)와는 다른 노브다.
	float SuccessChance = CurrentBoostGamble.SuccessChance;
	if (UCGGameInstance* GambleGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* GambleTraitMgr = GambleGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			SuccessChance += GambleTraitMgr->GetAggregatedTraitPercent(GambleGI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::GambleSuccess) / 100.0f;
		}
	}
	const bool bSuccess = FMath::FRand() < FMath::Clamp(SuccessChance, 0.0f, 0.95f);
	const EDevEventEffect Effect = CurrentBoostGamble.EffectType;

	// 확정 지출(야근수당 등) — 지른 순간 소비, 도박 결과와 무관. 여력은 위에서 확인됨.
	if (CurrentBoostGamble.GoCost > 0)
	{
		if (UResourceItemManager* ResMgr = GetResourceItemManager())
		{
			ResMgr->SpendResource(EResourceType::Money, CurrentBoostGamble.GoCost);
		}
	}

	// 이 이벤트가 세 점수에 줄 비율 (미적용 = 점수 영향 없음)
	float ScoreFrac = 0.0f;
	bool bApplyScore = false;

	// 리스크 특성 — 실패 손실 완화. 하한 30% 로 도박의 긴장을 남긴다(성공률은 건드리지 않는다).
	float FailFrac = CurrentBoostGamble.FailFrac;
	if (UCGGameInstance* RiskGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* RiskTraitMgr = RiskGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			const float Pct = RiskTraitMgr->GetAggregatedTraitPercent(RiskGI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::RiskLoss);
			FailFrac *= FMath::Max(0.3f, 1.0f - Pct / 100.0f);
		}
	}

	switch (Effect)
	{
	case EDevEventEffect::TimeExtend:
		// 야근/풀가동 — 개발 시간 실제 연장(출시 지연) + 점수 스윙. 램프 연출은 후속 주스.
		ProgressData.RemainingTime += CurrentBoostGamble.TimeSeconds;
		CachedTotalDuration += CurrentBoostGamble.TimeSeconds;
		OnTimeUpdated.Broadcast(ProgressData.RemainingTime);
		ScoreFrac = bSuccess ? CurrentBoostGamble.SuccessFrac : -FailFrac;
		bApplyScore = true;
		break;

	case EDevEventEffect::TimeCut:
		// 조기 출고 — 개발 시간 단축(빨리 출시). 성공=페널티 없음, 실패=리콜 점수 하락.
		ProgressData.RemainingTime = FMath::Max(0.0f, ProgressData.RemainingTime - CurrentBoostGamble.TimeSeconds);
		OnTimeUpdated.Broadcast(ProgressData.RemainingTime);
		if (!bSuccess)
		{
			ScoreFrac = -FailFrac;
			bApplyScore = true;
		}
		break;

	case EDevEventEffect::PayoffGamble:
		// 레버리지/재고 — 개발 점수 무관, 출시 후 운영 수익 배율에 반영.
		ProgressData.EventRewardMultiplier *= bSuccess
			? (1.0f + CurrentBoostGamble.SuccessFrac)
			: (1.0f - FailFrac);
		break;

	case EDevEventEffect::ScoreSwing:
	default:
		// 물량 투입 — 즉시 ±개발 점수 (현행 기계).
		ScoreFrac = bSuccess ? CurrentBoostGamble.SuccessFrac : -FailFrac;
		bApplyScore = true;
		break;
	}

	// 점수 반영 — 세 카테고리에 (목표합 × Frac)/3 가감(0 클램프) 후 UI 갱신
	if (bApplyScore)
	{
		// 활성 직능 스텝 전체의 목표합에 ScoreFrac 을 균등 배분(±) — 부스트 효과를 전 직능에 고르게
		float TargetSum = 0.0f;
		int32 ActiveCount = 0;
		for (const FStepRoundData& S : ProgressData.Steps)
		{
			if (S.DisciplineSlot == INDEX_NONE) { continue; }
			TargetSum += S.TargetScore;
			++ActiveCount;
		}
		if (ActiveCount > 0)
		{
			const float PerStep = (TargetSum * ScoreFrac) / ActiveCount;
			for (FStepRoundData& S : ProgressData.Steps)
			{
				if (S.DisciplineSlot == INDEX_NONE) { continue; }
				S.AcquiredScore = FMath::Max(0.0f, S.AcquiredScore + PerStep);
			}
			BroadcastDisciplineScores();
		}
	}

	OnBoostGambleResolved.Broadcast(true, bSuccess);
	UE_LOG(LogTemp, Log, TEXT("[DevEvent] effect=%d gamble=1 success=%d"), (int32)Effect, bSuccess ? 1 : 0);
}

// ===== 라이프사이클 FSM =====

bool UOfficeStageProgressManager::IsValidTransition(EProjectLifecycle From, EProjectLifecycle To, EProjectMode Mode)
{
	// 동일 상태 재진입 금지
	if (From == To) return false;

	switch (From)
	{
	case EProjectLifecycle::Idle:
		// Idle에서는 Developing으로만
		return To == EProjectLifecycle::Developing;

	case EProjectLifecycle::Developing:
		// Developing → LaunchPending (타이머 종료) 또는 Idle (취소)
		return To == EProjectLifecycle::LaunchPending || To == EProjectLifecycle::Idle;

	case EProjectLifecycle::LaunchPending:
		// 수주: 납품/취소 모두 Idle. 자체개발: 출시 → Operating 또는 ReportPending(운영 없는 새 흐름), 취소 → Idle
		if (To == EProjectLifecycle::Idle) return true;
		if (To == EProjectLifecycle::Operating) return Mode == EProjectMode::InHouse;
		if (To == EProjectLifecycle::ReportPending) return Mode == EProjectMode::InHouse;
		// 미달 시 컨티뉴(추가 개발) — 결과 모달에서 개발로 되돌아가는 유일한 역방향 전이
		if (To == EProjectLifecycle::Developing) return Mode == EProjectMode::InHouse;
		return false;

	case EProjectLifecycle::Operating:
		// Operating → ReportPending (자체개발 운영 종료)
		return To == EProjectLifecycle::ReportPending && Mode == EProjectMode::InHouse;

	case EProjectLifecycle::ReportPending:
		// ReportPending → Idle
		return To == EProjectLifecycle::Idle;

	default:
		return false;
	}
}

void UOfficeStageProgressManager::TransitionTo(EProjectLifecycle NewState)
{
	const EProjectLifecycle OldState = Lifecycle;

	if (!IsValidTransition(OldState, NewState, ProgressData.ActiveMode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] INVALID transition: %s → %s (Mode=%s)"),
			*LexToString(OldState), *LexToString(NewState),
			OldState == NewState ? TEXT("Same state") :
			(ProgressData.ActiveMode == EProjectMode::Commissioned ? TEXT("수주") :
			 ProgressData.ActiveMode == EProjectMode::InHouse ? TEXT("자체개발") : TEXT("None")));
		return;
	}

	Lifecycle = NewState;
	UE_LOG(LogTemp, Log, TEXT("[Lifecycle] %s → %s"), *LexToString(OldState), *LexToString(NewState));

	// 결과 모달(LaunchPending/ReportPending) 중엔 전 직원 피로 동결 — 플레이어가 못 만지는 구간.
	const bool bModalState = (NewState == EProjectLifecycle::LaunchPending || NewState == EProjectLifecycle::ReportPending);
	NotifyEmployeesFatigueFrozen(bModalState);

	OnLifecycleChanged.Broadcast(OldState, NewState);
}

void UOfficeStageProgressManager::ResetProjectState()
{
	// ProgressData 프로젝트 상태 일괄 초기화
	ProgressData.ActiveMode = EProjectMode::None;
	ProgressData.ActiveTraits.Empty();
	ProgressData.TraitRewardMultiplier = 1.0f;
	ProgressData.EventRewardMultiplier = 1.0f;
	ProgressData.ProjectID = 0;
	ProgressData.ProjectNumber = 0;
	ProgressData.ProjectName.Reset();
	ProgressData.CurrentStep = 0;
	ProgressData.RetryCount = 0;
	ProgressData.Step1AchievementRate = 0.0f;
	ProgressData.Step2AchievementRate = 0.0f;
	ProgressData.Step3AchievementRate = 0.0f;

	for (FStepRoundData& Step : ProgressData.Steps)
	{
		Step.AcquiredScore = 0.0f;
		Step.bIsCompleted = false;
	}

	bStageInProgress = false;
	ProgressData.bIsTimerRunning = false;

	if (TraitEventHandler)
	{
		TraitEventHandler->ClearAll();
	}
}

void UOfficeStageProgressManager::RequestLaunchConfirm()
{
	if (Lifecycle != EProjectLifecycle::LaunchPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] RequestLaunchConfirm ignored - not in LaunchPending (current=%s)"),
			*LexToString(Lifecycle));
		return;
	}

	// 수준 미달 = 출시 자체가 성립하지 않는다. 이 한 지점이 운영/EXP/미션/티어/도감 전부를 막는다.
	if (IsLaunchBlocked())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] RequestLaunchConfirm blocked - 직능별 최소 점수 미달"));
		return;
	}

	const EProjectMode Mode = ProgressData.ActiveMode;
	const bool bMetMinimum = ProgressData.MeetsMinimumClearScore();

	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	int32 BuildingIndex = GameInstance ? GameInstance->GetCurrentManagedBuildingIndex() : -1;

	// 전리품 캐시는 진입부에서 무조건 비운다 — 아래 롤을 안 도는 경로(제조 양산/최소점수 미달)가
	// 이전 출시의 캐시를 그대로 물려받아 리뷰 UI에 재표시하는 걸 순서 우연에 맡기지 않기 위해
	ULaunchLootManagerSubsystem* LootMgr = GameInstance ? GameInstance->GetSubsystem<ULaunchLootManagerSubsystem>() : nullptr;
	if (LootMgr)
	{
		LootMgr->ClearLastLaunchLoot();
	}

	// 제조업: 양산 분기
	if (ProgressData.bIsManufacturing)
	{
		if (GameInstance && BuildingIndex >= 0)
		{
			if (UProductionOrderManager* OrderMgr = GameInstance->GetSubsystem<UProductionOrderManager>())
			{
				OrderMgr->CreateOrder(ProgressData, BuildingIndex);
			}
		}

		// 제조업의 "프로젝트" 는 제품 설계이고 양산은 그 후속이다 — 설계를 통과시켰으면 클리어.
		// 전리품은 롤하지 않는다(DECISION_RECORDS §3.9 제조업 제외). 도감은 포트폴리오 표시와 맞추려 지급한다.
		if (bMetMinimum)
		{
			const bool bManufactureTierUnlocked = TierProgress.RecordProjectClear(ProgressData.ProjectID);
			SyncTierProgressToSave();
			CheckAndGrantCodexMilestone(ProgressData.ProjectID);
			if (bManufactureTierUnlocked)
			{
				OnTierUnlocked.Broadcast(TierProgress.CurrentTier);
			}
		}

		NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Idle);
		ResetProjectState();
		TransitionTo(EProjectLifecycle::Idle);
		return;
	}

	// 자체개발: 운영 시작 → Operating
	if (Mode == EProjectMode::InHouse)
	{
		bool bTierUnlocked = false;
		if (bMetMinimum)
		{
			// 승급 판정 = 현재 티어 7/10 클리어 단독
			bTierUnlocked = TierProgress.RecordProjectClear(ProgressData.ProjectID);
			SyncTierProgressToSave();
			CheckAndGrantCodexMilestone(ProgressData.ProjectID);   // 도감 마일스톤(티어 10/10, 전체 100%) 보상

			// 출시 전리품 — 보상 리빌(OfficeMainWidget::OnLaunchConfirmedHandler)이 GetLastLaunchLoot 로 동기 조회한다
			if (LootMgr)
			{
				const int32 ProjTier = FProjectTierProgress::GetTierForProject(ProgressData.ProjectID);
				LootMgr->RollLaunchLoot(ProgressData.ReviewScore, ProjTier);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Lifecycle] Self-dev not recorded for tier (metMinimum=%d)"),
				bMetMinimum ? 1 : 0);
		}

		// 출시 반응 7건 — 시장의 반응이라 최소점수 게이트와 무관하다(전리품·티어 판정과 절연). 리빌이 닫힌 뒤 레일로 드립된다.
		if (ULaunchReactionSubsystem* Reactions = GetWorld() ? GetWorld()->GetSubsystem<ULaunchReactionSubsystem>() : nullptr)
		{
			Reactions->PrepareReactions(BuildingIndex, ProgressData);
		}

		if (bTierUnlocked)
		{
			OnTierUnlocked.Broadcast(TierProgress.CurrentTier);
		}

		if (GameInstance && BuildingIndex >= 0)
		{
			if (UProjectOperationManager* OpMgr = GameInstance->GetSubsystem<UProjectOperationManager>())
			{
				OpMgr->StartOperation(ProgressData, BuildingIndex);
				if (UMissionManagerSubsystem* M = GameInstance->GetSubsystem<UMissionManagerSubsystem>())
				{
					M->NotifyProjectLaunched();
				}
			}
		}

		for (FStepRoundData& Step : ProgressData.Steps)
		{
			Step.AcquiredScore = 0.0f;
			Step.bIsCompleted = false;
		}
		ProgressData.bIsTimerRunning = false;
		bStageInProgress = false;

		NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Operation);

		TransitionTo(EProjectLifecycle::Operating);
		return;
	}
}

void UOfficeStageProgressManager::RequestLaunchCancel()
{
	if (Lifecycle != EProjectLifecycle::LaunchPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] RequestLaunchCancel ignored - not in LaunchPending"));
		return;
	}
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Idle);
	ResetProjectState();
	TransitionTo(EProjectLifecycle::Idle);
}

void UOfficeStageProgressManager::NotifyOperationCompleted()
{
	if (Lifecycle != EProjectLifecycle::Operating)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] NotifyOperationCompleted ignored - not in Operating (current=%s)"),
			*LexToString(Lifecycle));
		return;
	}
	NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode::Idle);
	TransitionTo(EProjectLifecycle::ReportPending);
}

void UOfficeStageProgressManager::NotifyReportClosed()
{
	if (Lifecycle != EProjectLifecycle::ReportPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lifecycle] NotifyReportClosed ignored - not in ReportPending"));
		return;
	}
	ResetProjectState();
	TransitionTo(EProjectLifecycle::Idle);
}

float UOfficeStageProgressManager::ComputeTierBaselineFromScores(const TArray<int32>& Step2Scores)
{
	if (Step2Scores.Num() == 0) { return 0.0f; }
	TArray<int32> Sorted = Step2Scores;
	Sorted.Sort();
	const int32 N = Sorted.Num();
	return (N % 2 == 1) ? static_cast<float>(Sorted[N / 2])
	                    : (Sorted[N / 2 - 1] + Sorted[N / 2]) * 0.5f;
}

float UOfficeStageProgressManager::ComputeTierBaseline(ECompanyType Industry, int32 Tier, const UTableManagerSubsystem* TableMgr)
{
	if (!TableMgr) { return 0.0f; }
	int32 Start = 0, End = 0;
	FTierLayout::GetRange(Tier, Start, End);
	TArray<int32> Scores;
	for (const FProjectData& Row : TableMgr->GetProjectsByCompanyType(Industry))
	{
		if (Row.ProjectIndex >= Start && Row.ProjectIndex <= End) { Scores.Add(Row.GetRequiredScore(2)); }
	}
	return ComputeTierBaselineFromScores(Scores);
}
