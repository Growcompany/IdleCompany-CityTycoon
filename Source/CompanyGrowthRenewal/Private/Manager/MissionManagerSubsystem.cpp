#include "Manager/MissionManagerSubsystem.h"
#include "Manager/MissionGuidePhases.h"
#include "Manager/GuideGestureRules.h"
#include "Manager/DevPresetSeeder.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Panel/OfficeRecruitmentPanelWidget.h"
#include "UI/Panel/EmployeeGachaPresentationWidget.h"
#include "UI/Panel/RewardRevealPresentationWidget.h"
#include "Data/TutorialMissionCompletionRules.h"
#include "Data/TutorialMissionAtomicCommitRules.h"
#include "Data/TutorialMissionRestoreRules.h"
#include "Data/TutorialRevenueCompletionRules.h"
#include "Data/TutorialCompletionStateRules.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Panel/WorkstationInfoWidget.h"
#include "UI/Panel/PitchBoardWidget.h"
#include "UI/Panel/LaunchConfirmWidget.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/EntityManager.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/ProjectOperationManager.h"
#include "UI/Panel/BuildingTraitGachaPanelWidget.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/EmployeeManager.h"
#include "Core/CGGameInstance.h"
#include "Utils/TutorialDisciplineSeed.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/HUD/MissionTrackerWidget.h"
#include "UI/HUD/GoalTrackerWidget.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/RewardClaimSplashWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/PlayerCamera.h"
#include "Entity/Factory/BrickFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Data/GameSaveData.h"
#include "Data/RecruitEmployeesMissionRules.h"
#include "Data/WorkstationCapacityRules.h"
#include "Global/CGDevSettings.h"
#include "Enum/WidgetType.h"
#include "Enum/NotificationType.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/World.h"

const FName UMissionManagerSubsystem::OpeningChainStartID = TEXT("M1_CollectBricks");

namespace
{
	APlayerCamera* FindPlayerCamera(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<APlayerCamera>(PC->GetPawn()) : nullptr;
	}
}

void UMissionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// DT 캐시/세이브 로드가 먼저 준비돼야 OnGameDataLoaded 시점에 미션 복원 가능
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UResourceItemManager>();
	// UIManager 가 먼저 초기화돼야 아래 OnLevelLayerReady 구독이 잡힌다 — 의존성 없이는 init 순서상
	// UIManager 가 늦어 GetSubsystem 이 null → 구독 스킵 → 트래커가 영영 안 뜨는 회귀가 났었음.
	Collection.InitializeDependency<UUIManagerSubsystem>();
	// M7 수익 수집 완료(OnRevenueCollected) 구독을 위해 먼저 초기화 보장 (init 순서 누락 시 구독 스킵 방지)
	Collection.InitializeDependency<UProjectOperationManager>();
	// 직원 강화(OnEmployeeEnhanced) 구독을 위해 먼저 초기화 보장 (init 순서 누락 시 구독 스킵 방지)
	Collection.InitializeDependency<UEmployeeManager>();

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UMissionManagerSubsystem::HandleGameDataLoaded);
	}
	// 레벨 UI 레이어 생성 완료 신호 — 트래커/가이드 빌드를 이 신호에 매단다 (StartPlay의 next-tick 경쟁 제거).
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnLevelLayerReady.AddUObject(this, &UMissionManagerSubsystem::TryStartMissionChain);
	}
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.AddUObject(this, &UMissionManagerSubsystem::HandleResourceChanged);
	}
	// M5 — 가챠 뽑기 완료 신호 구독 (p1→p2 전이)
	if (URecruitmentManagerSubsystem* RecMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>())
	{
		RecruitMgrWeak = RecMgr;
		RecMgr->OnGachaPullCompleted.AddUObject(this, &UMissionManagerSubsystem::HandleGachaPullCompleted);
	}
	// 특성 가챠 뽑기 신호 구독 (미션판 G10 안내가 참조할 최근 뽑기 ID 보관)
	if (UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>())
	{
		TraitMgr->OnTraitGachaCompleted.AddUObject(this, &UMissionManagerSubsystem::HandleTraitGachaPulled);
	}
	// M7 — 수익 수집 완료 신호 구독 (CollectFirstRevenue 완료 판정)
	if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->OnRevenueCollected.AddDynamic(this, &UMissionManagerSubsystem::HandleRevenueCollected);
	}
	// 직원 강화 시도 완료 신호 구독 (미션판 G9 완료 판정 — 구독 스킵되면 G9 가 영영 안 채워진다)
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeEnhanced.AddUObject(this, &UMissionManagerSubsystem::HandleEmployeeEnhanced);
	}

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] Initialized (OnGameDataLoaded subscribed)"));
}

void UMissionManagerSubsystem::Deinitialize()
{
	if (IsExplainPhaseActive())
	{
		ReleaseExplainPresentation();
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.RemoveAll(this);
	}
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnLevelLayerReady.RemoveAll(this);
	}
	if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResMgr->OnResourceChanged.RemoveAll(this);
	}
	if (URecruitmentManagerSubsystem* RecMgr = RecruitMgrWeak.Get())
	{
		RecMgr->OnGachaPullCompleted.RemoveAll(this);
	}
	if (UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>())
	{
		TraitMgr->OnTraitGachaCompleted.RemoveAll(this);
	}
	if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		OpMgr->OnRevenueCollected.RemoveDynamic(this, &UMissionManagerSubsystem::HandleRevenueCollected);
	}
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeEnhanced.RemoveAll(this);
	}
	// 미션판 G4/G5 — WorldSubsystem 이라 GetSubsystem 로 재조회하지 않고 마지막으로 구독한 인스턴스를 직접 해제
	if (UCityAcquisitionManager* AcqMgr = CityAcqMgrWeak.Get())
	{
		AcqMgr->OnCompanyCleared.RemoveAll(this);
	}
	// 미션판 트래커 표시 훅 (EnsureMissionWidgets 지연 구독) 해제
	if (UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>())
	{
		GoalMgr->OnGoalBoardChanged.RemoveAll(this);
		GoalMgr->OnTrackedGoalChanged.RemoveAll(this);
	}
	// 해제 = 래치 리셋 (불변식을 코드로 자명하게)
	bGoalBoardWidgetHookBound = false;
	DestroyMissionWidgets();
	Super::Deinitialize();
}

void UMissionManagerSubsystem::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	// 보상 지급 중 재진입 차단 + 클레임 대기 중엔 추가 진행 없음 (표시는 목표값으로 클램프됨)
	if (bCompletingMission || bReadyToClaim)
	{
		return;
	}

	if (!HasActiveMission())
	{
		return;
	}

	if (ActiveMissionRow.ConditionType != EMissionConditionType::CollectBricks)
	{
		return;
	}

	if (Type != EResourceType::Brick)
	{
		return;
	}

	// 노가다 중 3개 도달 → 스포트라이트 해제 + [공장] 버튼으로 패널 열기 유도 (강화 루프 학습)
	if (GuidePhase <= BrickGuide::TapFactory && NewValue >= BrickGuide::UpgradeNudgeAtBricks)
	{
		SetGuidePhase(BrickGuide::OpenFactory);
	}

	OnMissionProgressChanged.Broadcast();

	if (NewValue >= ActiveMissionRow.ConditionAmount)
	{
		SetReadyToClaim();
	}
}

void UMissionManagerSubsystem::HandleGameDataLoaded()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const FName SavedMissionID = Save ? Save->GameData.CurrentMissionID : NAME_None;
	const int32 SavedMissionProgress = Save ? Save->GameData.CurrentMissionProgress : 0;
	const bool bSavedClaimReady = Save && Save->GameData.bCurrentMissionReadyToClaim;
	const bool bSavedTutorialCompleted = Save && Save->GameData.bTutorialCompleted;

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] OnGameDataLoaded: Save=%s SavedMissionID=%s"),
		Save ? TEXT("yes") : TEXT("no(NewGame)"),
		Save ? *Save->GameData.CurrentMissionID.ToString() : TEXT("-"));

	// 세이브 없음 = 신규 게임 → 오프닝 체인 시작. 있으면 저장된 진행 복원 (NAME_None = 체인 끝)
	FName Restored = Save ? Save->GameData.CurrentMissionID : OpeningChainStartID;

	// 미션 도입 전/개발 잔재만 오프닝으로 복구한다. 명시 완료 세이브는 건물 수와 무관하게 체인 끝을 유지한다.
	if (FTutorialCompletionStateRules::ShouldRestoreOpeningChain(
		Save != nullptr,
		SavedMissionID,
		Save ? Save->GameData.Buildings.Num() : 0,
		bSavedTutorialCompleted))
	{
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 미션 None + 빌딩 0채 세이브 → 오프닝 체인으로 복구"));
		Restored = OpeningChainStartID;
	}

	// dev 시작 모드 오버라이드 (Project Settings > Game > CG Dev). Shipping 빌드에선 항상 FullTutorial.
	const ECGStartMode DevMode = UCGDevSettings::GetEffectiveStartMode();

	// dev 미션 오버라이드(스킵=None / 점프=Mn)는 **세션 첫 로드(게임 시작)에서만** 적용.
	// HandleGameDataLoaded 는 레벨 로드마다 호출되므로, 오피스 진입 등 후속 로드에서 재적용하면
	// 진행 중 미션이 None 으로 지워지거나(스킵) 이전 단계로 되돌려진다(점프) → 미션이 끊긴다.
	// 첫 로드 이후엔 세이브의 CurrentMissionID 를 그대로 복원해 M3→M5 진행을 유지한다.
	bool bDidDevJump = false;
	if (!bDevStartHandled)
	{
		bDevStartHandled = true;

		if (DevMode != ECGStartMode::FullTutorial)
		{
			// SkipTutorial / Sandbox — 오프닝 체인 진입 차단 (미션 없이 시작)
			UE_LOG(LogTemp, Warning, TEXT("[MissionManager] dev StartMode=%d → 튜토리얼 스킵 (미션 없이 시작)"), static_cast<int32>(DevMode));
			Restored = NAME_None;
		}

#if !UE_BUILD_SHIPPING
		// dev — "Mn 점프": DevStartMissionID 가 설정되면 StartMode 와 무관하게 그 미션부터 시작.
		const UCGDevSettings* DevCfg = GetDefault<UCGDevSettings>();
		if (DevCfg && !DevCfg->DevStartMissionID.IsNone())
		{
			Restored = DevCfg->DevStartMissionID;
			bDidDevJump = true;
			UE_LOG(LogTemp, Warning, TEXT("[MissionManager] dev 시작 미션 점프(세션 1회) → %s"), *Restored.ToString());
		}
#endif
	}

	bDevTutorialJumpSessionActive = FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
		bDevTutorialJumpSessionActive,
		bDidDevJump,
		/*bReachedTerminalCompletion=*/false);
	bTutorialCompleted = FTutorialCompletionStateRules::ResolveOnLoad(
		Save != nullptr,
		bSavedTutorialCompleted,
		DevMode != ECGStartMode::FullTutorial,
		bDevTutorialJumpSessionActive);

	SetActiveMission(Restored);

	DeskPlaceProgress = FWorkstationMissionProgressRules::ResolveRestoredProgress(
		SavedMissionID,
		ActiveMissionID,
		HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks,
		SavedMissionProgress,
		HasActiveMission() ? FMath::Max(1, ActiveMissionRow.ConditionAmount) : 0);
	if (DeskPlaceProgress > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 좌석 배치 진행 복원: %s %d/%d"),
			*ActiveMissionID.ToString(),
			DeskPlaceProgress,
			FMath::Max(1, ActiveMissionRow.ConditionAmount));
	}

	if (FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
		bSavedClaimReady,
		SavedMissionID,
		ActiveMissionID,
		HasActiveMission() && !ActiveMissionRow.NextMissionID.IsNone()))
	{
		bReadyToClaim = true;
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 최종 클레임 대기 상태 복원: %s"), *ActiveMissionID.ToString());
	}

	if (DevMode == ECGStartMode::Sandbox)
	{
		ApplyDevSandbox();
	}

#if !UE_BUILD_SHIPPING
	// 진행 상태 프리셋 — 세션 첫 로드에서만. next-tick 지연은 시더 내부에서 처리한다.
	if (DevMode == ECGStartMode::MidGamePreset && !bPresetSeedHandled)
	{
		bPresetSeedHandled = true;

		const UCGDevSettings* PresetCfg = GetDefault<UCGDevSettings>();
		if (PresetCfg && !PresetCfg->ProgressPresetRow.IsNone())
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UDevPresetSeeder* Seeder = GI->GetSubsystem<UDevPresetSeeder>())
				{
					Seeder->SeedStageA(PresetCfg->ProgressPresetRow);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MissionManager] MidGamePreset 인데 프리셋 행이 비어 있다 — 시드 생략"));
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	// 점프 미션의 선행상태 시드 (자원 듬뿍 + 빌딩 존재 체크) — 점프가 실제 적용된 첫 로드에서만
	if (bDidDevJump)
	{
		const UCGDevSettings* DevCfg = GetDefault<UCGDevSettings>();
		if (DevCfg && DevCfg->bSeedPrereqsForStartMission)
		{
			SeedTutorialPrereqs(DevCfg->DevStartMissionID);
		}
	}
#endif
}

void UMissionManagerSubsystem::ApplyDevSandbox()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 다음 틱으로 미룬다 — 다른 OnGameDataLoaded 핸들러(ResourceItemManager 등)가 세이브를 먼저
	// 메모리에 올린 뒤에 덮어써야 샌드박스 값이 살아남는다 (핸들러 호출 순서 비보장 회피).
	World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<UMissionManagerSubsystem>(this)]()
	{
		UMissionManagerSubsystem* Self = WeakThis.Get();
		UGameInstance* GI = Self ? Self->GetGameInstance() : nullptr;
		if (!GI)
		{
			return;
		}

		const UCGDevSettings* Dev = GetDefault<UCGDevSettings>();

		// 자원 듬뿍 — 기존 GrantTestResources 재사용 (DT_TestResourceScenarios Row, MarketCap 이 시총 게이트 개방)
		if (Dev && !Dev->SandboxResourceScenario.IsNone())
		{
			if (UResourceItemManager* Res = GI->GetSubsystem<UResourceItemManager>())
			{
				Res->GrantTestResources(Dev->SandboxResourceScenario);
			}
		}

		// 전체 해금 — HQ 레벨 강제 (HQ 게이트 콘텐츠). 세이브엔 안 쓰고 세션 메모리 + UI 브로드캐스트만.
		const int32 TargetHQ = Dev ? Dev->SandboxHQLevel : 20;
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SD = SaveMgr->GetCurrentSaveData())
			{
				if (SD->GameData.HQLevel < TargetHQ)
				{
					SD->GameData.HQLevel = TargetHQ;
					SaveMgr->OnHQLevelUp.Broadcast(TargetHQ);
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[MissionManager] DevSandbox 적용: 자원=%s, HQ=%d (티어/국가 개별 잠금은 시총 게이트 의존 — 안 열리면 시나리오 MarketCap 상향)"),
			Dev ? *Dev->SandboxResourceScenario.ToString() : TEXT("-"), TargetHQ);
	});
}

void UMissionManagerSubsystem::SeedTutorialPrereqs(FName TargetMissionID)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ApplyDevSandbox 와 동일 — 다음 틱으로 미뤄 다른 OnGameDataLoaded 핸들러(세이브 캐시)가 먼저 끝나게 한다.
	World->GetTimerManager().SetTimerForNextTick(
		[WeakThis = TWeakObjectPtr<UMissionManagerSubsystem>(this), TargetMissionID]()
	{
		UMissionManagerSubsystem* Self = WeakThis.Get();
		UGameInstance* GI = Self ? Self->GetGameInstance() : nullptr;
		if (!GI)
		{
			return;
		}

		// 자원 듬뿍 — M1 벽돌 노가다/M2 건설비용을 통째로 제거 (가장 큰 테스트 고통 해소)
		if (UResourceItemManager* Res = GI->GetSubsystem<UResourceItemManager>())
		{
			Res->GrantTestResources(TEXT("Rich"));
		}

		// 빌딩 선행: M1/M2(빌딩 만들기 전·중) 외 미션은 지어진 빌딩이 있어야 의미가 있다.
		// 빌딩 자동 스폰은 유효 플롯 확보가 필요해 v1 범위 밖 — 빌딩이 없으면 명확히 안내.
		// (개발 워크플로: 한 번 건설하면 세이브에 남아 이후 M3+ 점프를 빌딩 재건설 없이 반복 가능)
		const bool bNeedsBuilding =
			TargetMissionID != TEXT("M1_CollectBricks") &&
			TargetMissionID != TEXT("M2_BuildFirstCompany");
		if (bNeedsBuilding)
		{
			USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
			USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
			const int32 BuildingCount = Save ? Save->GameData.Buildings.Num() : 0;
			if (BuildingCount == 0)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[MissionManager] 시드: '%s'는 지어진 빌딩 선행 필요. 세이브에 빌딩 0채 — 한 번 건설 후 점프하거나 M2부터 시작할 것 (빌딩 자동 스폰은 v1.1)."),
					*TargetMissionID.ToString());
			}
		}

		// 개발자 M5 점프 전용 테스트 선행조건. 정상 플레이의 채용권 경제 공급에는 관여하지 않는다.
		if (TargetMissionID == TEXT("M5_FirstRecruit"))
		{
			if (UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>())
			{
				const int32 Owned = ItemMgr->GetItemCount(EItemType::RecruitTicketNormal);
				if (Owned < 2)
				{
					ItemMgr->AddItem(EItemType::RecruitTicketNormal, 2 - Owned);
					UE_LOG(LogTemp, Log, TEXT("[MissionManager] 시드: 일반 채용권 %d장 지급 (M5 개발자 점프)"), 2 - Owned);
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[MissionManager] SeedTutorialPrereqs 완료 (target=%s)"), *TargetMissionID.ToString());
	});
}

void UMissionManagerSubsystem::DevJumpToMission(FName MissionID)
{
	SetActiveMission(MissionID);
	if (!HasActiveMission())
	{
		UE_LOG(LogTemp, Error, TEXT("[MissionManager] DevJumpToMission 실패 — DT_Mission 에 '%s' 없음 (RowName 확인)"), *MissionID.ToString());
		return;
	}

	bDevTutorialJumpSessionActive = FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
		bDevTutorialJumpSessionActive,
		/*bJumpApplied=*/true,
		/*bReachedTerminalCompletion=*/false);
	bTutorialCompleted = false;
	SeedTutorialPrereqs(MissionID);
	TryStartMissionChain();   // 현재 맵(MainMap/OfficeMap)의 트래커/가이드 재구축
}

void UMissionManagerSubsystem::SetActiveMission(FName NewMissionID)
{
	if (IsExplainPhaseActive())
	{
		ReleaseExplainPresentation();
	}
	ActiveMissionID = NAME_None;
	ActiveMissionRow = FMissionTable();
	GuidePhase = 0;
	PhaseEnteredAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TransitionMissCount = 0;
	DeskPlaceProgress = 0;
	bReadyToClaim = false;

	if (NewMissionID.IsNone())
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	bool bFound = false;
	const FMissionTable Row = TableMgr ? TableMgr->GetMissionData(NewMissionID, bFound) : FMissionTable();
	if (!bFound)
	{
		// DT 행 누락 = loud failure — 체인을 조용히 잇지 않고 끊어서 즉시 드러나게
		UE_LOG(LogTemp, Warning, TEXT("[MissionManager] DT_Mission에 없는 미션 ID: %s (체인 중단)"), *NewMissionID.ToString());
		return;
	}

	ActiveMissionID = NewMissionID;
	ActiveMissionRow = Row;

	// 튜토리얼 첫 채용만 직능을 맞춘다. 미션 복원 경로도 이 함수를 지나므로 저장 없이 idempotent.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		if (URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>())
		{
			RecruitMgr->SeedForcedPrimaryDisciplines(
				TutorialDisciplineSeed::BuildSeedList(FMath::RandRange(0, 1)));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] SetActiveMission: %s"), *ActiveMissionID.ToString());
}

void UMissionManagerSubsystem::TryStartMissionChain()
{
	UWorld* World = GetWorld();

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] TryStartMissionChain: World=%s Map=%s Active=%s"),
		World ? TEXT("ok") : TEXT("NULL"),
		World ? *World->GetMapName() : TEXT("-"),
		*ActiveMissionID.ToString());

	// MainMap(오프닝 M1/M2/M3 가이드) + OfficeMap(M3 입장 완료 — 레벨 전환으로 파괴된 가이드 위젯 재구축) 둘 다 지원
	const bool bMainMap = World && World->GetMapName().Contains(TEXT("MainMap"));
	const bool bOfficeMap = World && World->GetMapName().Contains(TEXT("OfficeMap"));
	if (!World || (!bMainMap && !bOfficeMap))
	{
		return;
	}

	// 미션판 G4/G5 — 도시 회사는 MainMap 에만 존재. WorldSubsystem 이라 맵마다 새 인스턴스 → 여기서 재구독.
	// 미션판(G5 철거)이 체인 종료 후에도 신호를 받아야 하므로 미션 가드보다 앞에서 재구독
	if (bMainMap)
	{
		BindCityAcquisitionDelegates(World);
	}

	// 레벨 재진입 대비 — 이전 월드의 위젯 포인터 정리 후 재구성
	DestroyMissionWidgets();

	if (!HasActiveMission())
	{
		// 표시 판정은 EnsureMissionWidgets 의 bWantGoalTracker 단독 권한 — 여기서 IsUnlocked 로 한 번 더 거르면
		// 잠금 상태에서 OnGoalBoardChanged 지연 구독 래치가 영영 안 걸려 치트/후속 언락 브로드캐스트를 놓친다
		EnsureMissionWidgets();
		// 미션판 안내 밴드도 이 폴에 실린다 — 여기서 타이머를 안 걸면 체인이 없는 동안 밴드가 영영 갱신되지 않는다.
		// PollGuideProgress 의 체인 로직은 전부 HasActiveMission() 가드 안이라 미션 없이 돌려도 안전하다
		World->GetTimerManager().SetTimer(PollTimerHandle, this, &UMissionManagerSubsystem::PollGuideProgress, 0.25f, true);
		return;
	}

	// 레이어 재준비가 Explain 도중 재호출될 수 있다. 직접 리셋 전에 예약 게이지/카메라 포커스를 놓아
	// 운영 종료로 설명을 건너뛰는 분기에서도 이전 프레젠테이션이 남지 않게 한다.
	if (IsExplainPhaseActive())
	{
		ReleaseExplainPresentation();
	}
	GuidePhase = 0;
	PhaseEnteredAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TransitionMissCount = 0;

	// CollectBricks 강화 넛지 통과 감지 — 공장 강화 델리게이트 구독 (레벨별 1회, AddUnique로 중복 방지).
	// 시작 미션 타입과 무관하게 구독 — 체인 중간에 CollectBricks 가 와도 신호를 받도록 (핸들러가 타입 가드).
	// 공장은 MainMap에만 존재하므로 OfficeMap에선 구독 생략.
	if (bMainMap)
	{
		if (ABrickFactory* Factory = Cast<ABrickFactory>(
			UGameplayStatics::GetActorOfClass(World, ABrickFactory::StaticClass())))
		{
			BrickFactoryWeak = Factory;
			Factory->OnFactoryUpgraded.AddUniqueDynamic(this, &UMissionManagerSubsystem::HandleFactoryUpgraded);
		}
	}

	// UI 레이어 준비 신호(UIManager::OnLevelLayerReady) 또는 DevJumpToMission 에서 호출 — 둘 다 레이어가 준비된 시점이라
	// 자체 next-tick 없이 동기로 빌드한다. (예전엔 ShowMainMapUI 의 next-tick 레이어 생성과 경쟁해 폴백/승격이 필요했음.)
	EnsureMissionWidgets();
	World->GetTimerManager().SetTimer(PollTimerHandle, this, &UMissionManagerSubsystem::PollGuideProgress, 0.25f, true);

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] 미션 체인 시작: %s"), *ActiveMissionID.ToString());

	// 이미 조건 충족 상태(Dev 시나리오 자원 등)면 바로 완료 처리
	TryCompleteByResources();

	// M3 EnterOffice — 오피스 맵에 들어와 체인이 (재)시작되면 입장 조건 충족 → 클레임 대기.
	// 위젯 생성/구독 이후에 호출되므로 트래커가 ReadyToClaim 브로드캐스트를 받는다 (오피스 저장-재접속도 커버).
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice
		&& bOfficeMap)
	{
		SetReadyToClaim();
	}

	// M7 CollectFirstRevenue — 사무실 [뒤로]로 MainMap 도착 → 실제 운영 수익과 수집 경로를 설명한다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
		&& GuidePhase == CollectGuide::ExitOffice
		&& bMainMap)
	{
		const int32 TargetIdx = UCGGameInstance::GetInstance() ? UCGGameInstance::GetInstance()->GetCurrentManagedBuildingIndex() : INDEX_NONE;
		if (UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>())
		{
			if (ABuildingBaseActor* TargetBuilding = EntityMgr->GetBuildingByIndex(TargetIdx))
			{
				TutorialFirstBuildingWeak = TargetBuilding;
			}
		}
		UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>();

		// 운영조회·포커스·설명타겟이 서로 다른 건물을 보면 그물이 안 닫힌다.
		ABuildingBaseActor* ExplainBuilding = TutorialFirstBuildingWeak.Get();
		const int32 ExplainIdx = ExplainBuilding ? ExplainBuilding->GetBuildingIndex() : INDEX_NONE;

		// 자리를 비운 사이 운영이 끝났으면 진행 Bar 가 없어 설명이 성립하지 않는다.
		// 그대로 Explain 에 들어가면 타겟이 영영 안 모여 페이즈를 넘길 수단이 사라진다 — 건너뛴다.
		// HasActiveOperation 은 선형 스캔이라 INDEX_NONE 도 안전하게 false 를 낸다 — 건물 유효성은 아래에서 한 번만 본다
		const bool bHasOperation = OpMgr && OpMgr->HasActiveOperation(ExplainIdx);
		const bool bCanExplain = ExplainBuilding != nullptr && bHasOperation;
		// 설명을 건너뛰면 화면이 통째로 달라지는데 조용히 갈라지면 원인을 못 찾는다.
		// 두 인덱스를 함께 찍어 축이 어긋난 세이브도 로그만으로 드러나게 한다
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] M7 도착: Building=%d(설명대상=%d) 운영=%s -> %s"),
			TargetIdx, ExplainIdx,
			bHasOperation ? TEXT("있음") : TEXT("없음"),
			bCanExplain ? TEXT("Explain") : TEXT("PressCollect(설명 건너뜀)"));
		// 운영 진행 게이지가 이 건물 위에 있다 ― 화면 밖이거나 줌이 멀면 설명이 무의미해진다.
		if (bCanExplain)
		{
			if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			{
				if (UInGameLayerWidget* Layer = UIMgr->GetInGameLayer())
				{
					Layer->ReserveTutorialVaultGauge(ExplainIdx);
				}
			}
			if (APlayerCamera* Cam = FindPlayerCamera(World))
			{
				Cam->FocusOnBuilding(ExplainBuilding);
			}
		}

		SetGuidePhase(bCanExplain ? CollectGuide::Explain : CollectGuide::PressCollect);
	}
}

void UMissionManagerSubsystem::RegisterBuildOpenWidget(UBuildOpenWidget* Widget)
{
	BuildOpenWidgetWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterBuildOpenWidget(UBuildOpenWidget* Widget)
{
	if (BuildOpenWidgetWeak.Get() == Widget)
	{
		BuildOpenWidgetWeak.Reset();
	}
}

void UMissionManagerSubsystem::NotifyBuildModalOpened(UCommonActivatableWidget* Modal)
{
	if (!HasActiveMission())
	{
		return;
	}

	if (ActiveMissionRow.ConditionType == EMissionConditionType::BuildFirstBuilding)
	{
		BuildModalWeak = Modal;
		SetGuidePhase(BuildGuide::PickGameCard);
	}
}

void UMissionManagerSubsystem::RegisterFactoryPanel(UFactoryPanelWidget* Panel)
{
	FactoryPanelWeak = Panel;

	// 공장 패널 열림 = OpenFactory 페이즈 통과 → 강화 유도로 전환
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::OpenFactory)
	{
		SetGuidePhase(BrickGuide::NudgeUpgrade);
	}
}

void UMissionManagerSubsystem::RegisterBuildPlacementPanel(UBuildPlacementPanelWidget* Panel)
{
	BuildPlacementPanelWeak = Panel;
}

void UMissionManagerSubsystem::UnregisterBuildPlacementPanel(UBuildPlacementPanelWidget* Panel)
{
	if (BuildPlacementPanelWeak.Get() == Panel)
	{
		BuildPlacementPanelWeak.Reset();
	}
}

void UMissionManagerSubsystem::UnregisterFactoryPanel(UFactoryPanelWidget* Panel)
{
	if (FactoryPanelWeak.Get() == Panel)
	{
		FactoryPanelWeak.Reset();
	}
}

void UMissionManagerSubsystem::HandleFactoryUpgraded(EFactoryUpgradeType /*UpgradeType*/)
{
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::CollectBricks)
	{
		return;
	}

	// 첫 강화 = 아래 강화 유도, 둘째 강화 = 그라인드. 강화는 Money 게이트라 조기 강화 유실 방지 위해 <= 유지
	if (GuidePhase <= BrickGuide::NudgeUpgrade)
	{
		SetGuidePhase(BrickGuide::NudgeUpgrade2);
	}
	else if (GuidePhase == BrickGuide::NudgeUpgrade2)
	{
		SetGuidePhase(BrickGuide::Grind);
	}
}

void UMissionManagerSubsystem::SetReadyToClaim()
{
	if (bReadyToClaim || !HasActiveMission())
	{
		return;
	}

	const ETutorialMissionCompletionMode CompletionMode = FTutorialMissionCompletionRules::Resolve(
		!ActiveMissionRow.NextMissionID.IsNone());
	if (CompletionMode == ETutorialMissionCompletionMode::ImmediateAdvance)
	{
		UE_LOG(LogTemp, Log, TEXT("[MissionManager] 중간 미션 즉시 완료: %s"), *ActiveMissionID.ToString());
		CompleteActiveMission();
		return;
	}

	bReadyToClaim = true;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}

	// 가이드 전부 내림 — 조회 게터(하이라이트/스포트라이트)가 클레임 대기 중 null 을 반환하고,
	// 상주 알림도 여기서 해제된다.
	UpdateGuideNotification();
	OnMissionReadyToClaim.Broadcast(ActiveMissionRow);

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] 최종 보상 클레임 대기 진입: %s"), *ActiveMissionID.ToString());
}

bool UMissionManagerSubsystem::IsClaimPresentable() const
{
	// 배치 모드는 확정 후에도 이어진다(연속 배치) — 다음 탭은 배치용이지 수령용이 아니다.
	if (IsPlacementActive())
	{
		return false;
	}

	// 채용/가챠/인수 등 프롬프트 모달 위에 풀스크린 수령 버튼을 깔지 않는다.
	// 리빌·채용 모달이 떠 있는 동안 최종 수령 딤이 겹치지 않게 막는다.
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UUIBase* UIBase = UIMgr->GetUIBase())
		{
			if (UIBase->GetPromptStackCount() > 0)
			{
				return false;
			}
			// BottomStack 은 기반 바(MainMap=BuildOpen / OfficeMap=OfficeMain)가 상주해 항상 1 이다.
			// 그 위에 얹힌 패널(책상 배치·장식 배치·관리 패널 등)은 트래커를 "가리기만" 하고 Collapse 시키지 않아
			// 아래 가시성 판정으로는 안 걸린다 — 카드가 패널 뒤에 숨은 채 딤만 깔리는 상태가 이 경로였다.
			if (UIBase->GetBottomStackCount() > 1)
			{
				return false;
			}
		}
	}

	// 비출 트래커 카드가 없으면(하단 패널이 덮거나 조상이 Collapsed) 연출하지 않는다.
	// 이 경우 오버레이는 "카드 없는 풀스크린 딤 + 아무데나 탭 수령" 이 되어,
	// 플레이어에겐 누른 적 없는 미션이 저절로 수령되는 것으로 보인다.
	// BottomStack 은 BuildOpen 바가 상주해 카운트로 못 거르므로, 이 "보이는가" 판정이 일반 가드 역할을 한다.
	if (!GetClaimSpotlightTarget())
	{
		return false;
	}
	return true;
}

void UMissionManagerSubsystem::ClaimActiveMission()
{
	if (!bReadyToClaim || !HasActiveMission())
	{
		return;
	}

	// 연쇄 수령 차단 — 다음 미션 조건이 이미 충족돼 있으면 클레임 직후 곧바로 다시 대기 상태가 된다.
	// 예전엔 "작은 트래커 카드를 다시 정확히 눌러야 함" 이 제동이었는데, 클레임 탭이 풀스크린이 되면서
	// 보상 연출 보며 두어 번 탭하면 다음 미션들이 줄줄이 수령됐다. 한 번의 탭 = 최대 한 미션.
	const double Now = FPlatformTime::Seconds();
	if (LastClaimTime > 0.0 && (Now - LastClaimTime) < ClaimCooldown)
	{
		return;
	}
	const double PreviousClaimTime = LastClaimTime;
	LastClaimTime = Now;

	// 저장 성공 뒤에만 보상 연출과 완료 이벤트를 공개한다.
	const FMissionTable ClaimedMission = ActiveMissionRow;
	if (CompleteActiveMission())
	{
		ShowRewardSplash(ClaimedMission);
	}
	else
	{
		LastClaimTime = PreviousClaimTime;
	}
}

void UMissionManagerSubsystem::ShowRewardSplash(const FMissionTable& Mission)
{
	if (Mission.Rewards.Num() == 0)
	{
		return;
	}

	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	// 체인 마지막 미션 = 졸업 연출 — 미션판 유도는 클레임 직후 트래커 미션 모드가 담당
	const FText SplashTitle = Mission.NextMissionID.IsNone()
		? FText::FromString(TEXT("튜토리얼 클리어!"))
		: Mission.Title;

	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::RewardReveal);
		if (Cls)
		{
			if (URewardRevealPresentationWidget* Overlay = CreateWidget<URewardRevealPresentationWidget>(PC, Cls))
			{
				// AddToViewport(NativeConstruct: BindWidget 바인드 + 버튼 OnClicked) 먼저 → 그 다음 박스 구성
				Overlay->AddToViewport(10000); // 가이드(9000)/토스트(9999) 위
				Overlay->SetupRewards(Mission.Rewards, SplashTitle, /*bToastMode=*/false);
				return;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[MissionManager] 최종 보상 연출 위젯 미등록 — 경량 폴백"));

	// 폴백 = 경량 토스트 (팝인→홀드→페이드아웃 후 스스로 RemoveFromParent)
	URewardClaimSplashWidget* Splash = CreateWidget<URewardClaimSplashWidget>(PC, URewardClaimSplashWidget::StaticClass());
	if (Splash)
	{
		Splash->InitSplash(Mission);
		Splash->AddToViewport(9500); // 가이드 오버레이(9000) 위 / 토스트(9999) 아래
	}
}

void UMissionManagerSubsystem::NotifyBuildingPlaced(ABuildingBaseActor* Building)
{
	// 건설 채수 도달형 미션 신호 — 판정(보유 채수 절대값)은 GoalBoard 가 수행
	OnConditionSignal.Broadcast(EMissionConditionType::BuildSecondBuilding);
	OnConditionSignal.Broadcast(EMissionConditionType::BuildOnClearedPlot);

	if (!HasActiveMission())
	{
		return;
	}

	if (ActiveMissionRow.ConditionType == EMissionConditionType::BuildFirstBuilding)
	{
		// 방금 지은 회사 빌딩을 다음 미션(M3 EnterOffice)의 p0 스포트라이트 대상으로 캡처.
		// M2→M3 전환에도 살아남는다(SetActiveMission이 리셋하는 GuidePhase와 별개 멤버).
		TutorialFirstBuildingWeak = Building;

		SetReadyToClaim();
	}
}

void UMissionManagerSubsystem::NotifyManagePanelOpened(UBuildingManagePanelWidget* Panel)
{
	// [입장] 버튼 하이라이트 타겟 등록 (패널 닫히면 weak 자동 무효화 → 링 생략)
	ManagePanelWeak = Panel;

	// M3 EnterOffice — 빌딩 클릭으로 관리 패널이 열림 = ClickBuilding 통과 → [입장] 유도로 전환
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice
		&& GuidePhase == EnterOfficeGuide::ClickBuilding)
	{
		SetGuidePhase(EnterOfficeGuide::PressEnter);
	}
}

// ===== M5 RecruitEmployees =====
void UMissionManagerSubsystem::RegisterOfficeMainWidget(UOfficeMainWidget* Widget)
{
	OfficeMainWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterOfficeMainWidget(UOfficeMainWidget* Widget)
{
	if (OfficeMainWeak.Get() == Widget)
	{
		OfficeMainWeak.Reset();
	}
}

void UMissionManagerSubsystem::RegisterOfficeLayer(UOfficeLayerWidget* Widget)
{
	OfficeLayerWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterOfficeLayer(UOfficeLayerWidget* Widget)
{
	if (OfficeLayerWeak.Get() == Widget)
	{
		OfficeLayerWeak.Reset();
	}
}

void UMissionManagerSubsystem::NotifyRecruitPanelOpened(UOfficeRecruitmentPanelWidget* Panel)
{
	RecruitPanelWeak = Panel;
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase == RecruitGuide::OpenRecruit)
	{
		SetGuidePhase(RecruitGuide::PressPull);
	}
}

void UMissionManagerSubsystem::RegisterGachaPresentation(UEmployeeGachaPresentationWidget* Widget)
{
	GachaPresentationWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterGachaPresentation(UEmployeeGachaPresentationWidget* Widget)
{
	if (GachaPresentationWeak.Get() == Widget)
	{
		GachaPresentationWeak.Reset();
	}
}

void UMissionManagerSubsystem::HandleGachaPullCompleted(const FGachaResultData& /*Result*/)
{
	// 뽑기 실행 = p1 통과 → 연출 [확인] 유도. 연출 위젯은 곧 RegisterGachaPresentation 으로 자기 등록.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase <= RecruitGuide::PressPull)
	{
		SetGuidePhase(RecruitGuide::PressConfirm);
	}
}

void UMissionManagerSubsystem::NotifyHireConfirmed(const FGachaResultData& /*Result*/)
{
	// 채용 확정 → 채용 패널 [닫기] 유도 (가챠 오버레이는 닫혔지만 채용 패널이 남아 있음)
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees
		&& GuidePhase <= RecruitGuide::PressConfirm)
	{
		SetGuidePhase(RecruitGuide::PressCloseRecruit);
	}

	EvaluateRecruitEmployeesCondition();
}

void UMissionManagerSubsystem::NotifyRecruitPanelClosed()
{
	// 착석 실패/벤치 적립 경로에서도 직원 수 절대값으로 완료 여부를 복구한다.
	EvaluateRecruitEmployeesCondition();
}

// ===== M5 좌석 배정 =====
void UMissionManagerSubsystem::NotifyWorkstationPanelOpened(UWorkstationInfoWidget* Panel)
{
	WorkstationPanelWeak = Panel;
}

void UMissionManagerSubsystem::NotifyEmployeeSeated()
{
	EvaluateRecruitEmployeesCondition();
}

// ===== M6 기획 보드 =====
void UMissionManagerSubsystem::RegisterPitchBoard(UPitchBoardWidget* Widget)
{
	PitchBoardWeak = Widget;
	// 보드 열림 → 기획안 선택 유도
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase == InHouseGuide::OpenBoard)
	{
		SetGuidePhase(InHouseGuide::PickInHouse);
	}
}

void UMissionManagerSubsystem::UnregisterPitchBoard(UPitchBoardWidget* Widget)
{
	if (PitchBoardWeak.Get() == Widget)
	{
		PitchBoardWeak.Reset();
	}
}

void UMissionManagerSubsystem::RegisterLaunchConfirm(ULaunchConfirmWidget* Widget)
{
	LaunchConfirmWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterLaunchConfirm(ULaunchConfirmWidget* Widget)
{
	if (LaunchConfirmWeak.Get() == Widget)
	{
		LaunchConfirmWeak.Reset();
	}
}

// ===== M6 LaunchFirstInHouseProject =====
void UMissionManagerSubsystem::NotifyInHouseProjectSelected()
{
	// 페이즈를 안 따지는 이유 = 과락 미달로 폐기하고 재착수하는 경로. 등호(PickInHouse)로 막으면
	// 페이즈가 PressLaunch 에 남아 재개발 내내 가이드가 없는 버튼을 겨눈다. 착수했으면 항상 관찰 단계다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject)
	{
		SetGuidePhase(InHouseGuide::WatchStrip);

		// 월드 없으면 0.0 대입 — 값이 크게 뒤처져 첫 poll에서 즉시 WatchStrip을 벗어나는 폴백(관찰 스킵)이 의도임을 코드에 남긴다.
		const UWorld* GuideWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		StripWatchEnteredAt = GuideWorld ? GuideWorld->GetTimeSeconds() : 0.0;
	}
}

void UMissionManagerSubsystem::NotifyProjectLaunched()
{
	// >= WatchStrip — CompleteStep 치트나 Duration 단축 튜닝으로 관찰 4초 안에 출시가 오면 PressLaunch 전이가 아직
	// 안 됐을 수 있다. 페이즈 상수가 순서를 보장하므로 "관찰 단계 이후 언제든 출시되면 완료"로 읽힌다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase >= InHouseGuide::WatchStrip)
	{
		// 출시 즉시 미션 완료 — 운영(3분)은 백그라운드로 돌고, 대기시키지 않는다.
		// "상시 수집" 안내만 토스트로 알려줌 (수령은 자유, 강제 가이드 없음).
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(
				FText::FromString(TEXT("운영 시작! 빌딩에 수익이 쌓이면 [수집] 버튼으로 수령하세요")),
				5.0f, ENotificationType::Normal);
		}
		SetReadyToClaim();
	}
}

void UMissionManagerSubsystem::NotifyProjectOperationCompleted()
{
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject)
	{
		SetReadyToClaim();
	}
}

// ===== M7 CollectFirstRevenue =====
void UMissionManagerSubsystem::HandleRevenueCollected(int64 Amount)
{
	// 실제 양수 수익을 수집했을 때만 M7을 완료한다. 빈 수집 신호는 진행으로 인정하지 않는다.
	if (HasActiveMission()
		&& ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
		&& FTutorialRevenueCompletionRules::IsPositiveCollection(Amount))
	{
		SetReadyToClaim();
	}
}

// ===== 미션판 G10 EquipTrait =====
void UMissionManagerSubsystem::NotifyTraitEquipped()
{
	// 특성 장착은 미션판 G10 조건 — 판정은 GoalBoard 가 한다 (미션 유무와 무관하게 신호를 흘린다)
	OnConditionSignal.Broadcast(EMissionConditionType::EquipFirstTrait);
}

void UMissionManagerSubsystem::RegisterTraitGachaPanel(UBuildingTraitGachaPanelWidget* Widget)
{
	TraitGachaPanelWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterTraitGachaPanel(UBuildingTraitGachaPanelWidget* Widget)
{
	if (TraitGachaPanelWeak.Get() == Widget)
	{
		TraitGachaPanelWeak.Reset();
	}
}

void UMissionManagerSubsystem::RegisterGachaReveal(UGachaRevealPresentationWidget* Widget)
{
	GachaRevealWeak = Widget;
}

void UMissionManagerSubsystem::UnregisterGachaReveal(UGachaRevealPresentationWidget* Widget)
{
	if (GachaRevealWeak.Get() == Widget)
	{
		GachaRevealWeak.Reset();
	}
}

// 특성 흐름은 미션판 G10 으로 이관돼 페이즈 전진이 없다 — 훅은 다른 시스템의 신호원으로 유지한다.
void UMissionManagerSubsystem::NotifyGachaRevealConfirmed()
{
}

void UMissionManagerSubsystem::NotifyTraitTabOpened()
{
}

void UMissionManagerSubsystem::HandleTraitGachaPulled(const FBuildingTraitGachaResult& Result)
{
	// 방금 뽑은 특성 ID 는 계속 기억한다 — 미션판 안내가 카드를 지목할 때 쓸 수 있는 유일한 단서
	TutorialPulledTraitID = Result.ResultTraitID;
}

void UMissionManagerSubsystem::NotifySkinEquipped()
{
	// 스킨 장착은 미션판 G8 조건 — 판정은 GoalBoard 가 한다 (미션 유무와 무관하게 신호를 흘린다)
	OnConditionSignal.Broadcast(EMissionConditionType::EquipFirstSkin);
}

void UMissionManagerSubsystem::NotifyBuildingFloorUpgraded()
{
	OnConditionSignal.Broadcast(EMissionConditionType::RaiseBuildingFloor);
}

// ===== 미션판 G11 AcquireFirstPlot =====
void UMissionManagerSubsystem::NotifyPlotAcquired()
{
	OnConditionSignal.Broadcast(EMissionConditionType::AcquireFirstPlot);
}

// ===== 미션판 G2 HQLevel =====
void UMissionManagerSubsystem::NotifyHQLevelChanged()
{
	OnConditionSignal.Broadcast(EMissionConditionType::ReachHQLevel);
}

// ===== 강화 모달 등록 (미션판 G9 안내 확장 여지) =====
void UMissionManagerSubsystem::RegisterStarforceModal(UEnhanceStarforceModalWidget* InModal)
{
	StarforceModalWeak = InModal;
}

void UMissionManagerSubsystem::UnregisterStarforceModal(UEnhanceStarforceModalWidget* InModal)
{
	if (StarforceModalWeak.Get() == InModal)
	{
		StarforceModalWeak.Reset();
	}
}

// ===== 미션판 G4 AcquireCompany / G5 DemolishCompany =====
void UMissionManagerSubsystem::BindCityAcquisitionDelegates(UWorld* World)
{
	UCityAcquisitionManager* AcqMgr = World ? World->GetSubsystem<UCityAcquisitionManager>() : nullptr;
	if (!AcqMgr)
	{
		return;
	}
	// 네이티브 델리게이트엔 AddUnique 가 없어 같은 인스턴스에 두 번 붙으면 핸들러가 중복 호출된다 → 항상 해제 후 구독.
	AcqMgr->OnCompanyCleared.RemoveAll(this);
	AcqMgr->OnCompanyCleared.AddUObject(this, &UMissionManagerSubsystem::HandleCompanyCleared);
	CityAcqMgrWeak = AcqMgr;
}

void UMissionManagerSubsystem::NotifyCompanyAcquired(int32 /*Key*/)
{
	OnConditionSignal.Broadcast(EMissionConditionType::AcquireFirstCompany);
}

void UMissionManagerSubsystem::HandleCompanyCleared(int32 /*Key*/)
{
	OnConditionSignal.Broadcast(EMissionConditionType::DemolishFirstCompany);
}

void UMissionManagerSubsystem::HandleEmployeeEnhanced(int32 /*EmployeeID*/, UEmployeeManager::EEnhanceResult /*Result*/)
{
	// G9 완료 = SP 투자가 아니라 강화 실행 — 스타포스를 가르치는 지점이 게임 전체에서 여기뿐이다.
	// 확률형이라 EEnhanceResult 는 보지 않는다(성공/실패/유지 무관, 시도 1회로 인정 — 자금 고갈로 막히지 않게).
	OnConditionSignal.Broadcast(EMissionConditionType::InvestFirstStatPoint);
}

// ===== M4 PlaceDesks =====
void UMissionManagerSubsystem::NotifyPlacementModeOpened(EOfficePlacementKind Kind)
{
	if (!HasActiveMission()) return;
	if (Kind == EOfficePlacementKind::Desk
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& GuidePhase == DeskGuide::OpenPlacement)
		SetGuidePhase(DeskGuide::Confirm);
}

void UMissionManagerSubsystem::NotifyOfficePlacementCompleted(EOfficePlacementKind Kind, int32 Count)
{
	// 그림(벽장식) 배치는 미션판 G7 조건 — 미션 유무와 무관하게 신호를 흘린다
	if (Kind == EOfficePlacementKind::WallDecoration)
	{
		OnConditionSignal.Broadcast(EMissionConditionType::PlaceFirstPainting);
	}

	if (!HasActiveMission()) return;
	if (Kind != EOfficePlacementKind::Desk
		|| ActiveMissionRow.ConditionType != EMissionConditionType::PlaceDesks)
	{
		return;
	}

	DeskPlaceProgress += FWorkstationMissionProgressRules::GetProgressIncrement(Count);

	if (DeskPlaceProgress >= FMath::Max(1, ActiveMissionRow.ConditionAmount))
	{
		SetReadyToClaim();
	}
	else if (GuidePhase == DeskGuide::PlaceMore)
	{
		// 3개 이상이면 페이즈가 그대로라 SetGuidePhase 가 no-op — 남은 개수 문구는 직접 갱신한다
		OnGuidePhaseChanged.Broadcast();
		OnMissionProgressChanged.Broadcast();
	}
	else
	{
		// 연속 배치 — 배치 모드가 유지되므로 남은 개수를 안내한다(버튼 유도 복귀는 모드 이탈 시 워치독이)
		SetGuidePhase(DeskGuide::PlaceMore);
		OnMissionProgressChanged.Broadcast();
	}
}

// ===== 미션판 G9 EnhanceStat =====
void UMissionManagerSubsystem::NotifyEmployeeWindowOpened(UEmployeeWindowWidget* Window)
{
	EmployeeWindowWeak = Window;
}

bool UMissionManagerSubsystem::CompleteActiveMission()
{
	if (!HasActiveMission())
	{
		return false;
	}

	const FName CompletedID = ActiveMissionID;
	const FMissionTable CompletedRow = ActiveMissionRow;
	const bool bIsTerminalMission = CompletedRow.NextMissionID.IsNone();

	UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (bIsTerminalMission
		&& (!GoalMgr || !FTutorialMissionAtomicCommitRules::AreAllFinalRewardsGrantable(
			CompletedRow.Rewards,
			ResMgr != nullptr,
			ItemMgr != nullptr)))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[MissionManager] 피날레 클레임 거부 — 유효한 최종 보상 또는 GoalBoard 없음: %s"),
			*CompletedID.ToString());
		return false;
	}

	const TMap<EResourceType, int64> ResourceBankBeforeCommit =
		ResMgr ? ResMgr->GetAllResources() : TMap<EResourceType, int64>();
	const TMap<EItemType, int32> ItemStorageBeforeCommit =
		ItemMgr ? ItemMgr->GetAllItems() : TMap<EItemType, int32>();
	FGameSaveData CachedGameDataBeforeCommit;
	USaveGame_GameData* CachedSaveBeforeCommit = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const bool bHasCachedSaveSnapshot = CachedSaveBeforeCommit != nullptr;
	if (bHasCachedSaveSnapshot)
	{
		CachedGameDataBeforeCommit = CachedSaveBeforeCommit->GameData;
	}

	const bool bReadyBeforeCommit = bReadyToClaim;
	const bool bTutorialCompletedBeforeCommit = bTutorialCompleted;
	const bool bDevJumpSessionBeforeCommit = bDevTutorialJumpSessionActive;
	const int32 GuidePhaseBeforeCommit = GuidePhase;
	const int32 ExplainStepBeforeCommit = ExplainStep;
	const int32 TransitionMissCountBeforeCommit = TransitionMissCount;
	const int32 DeskPlaceProgressBeforeCommit = DeskPlaceProgress;
	const double PhaseEnteredAtBeforeCommit = PhaseEnteredAt;

	bCompletingMission = true;
	bReadyToClaim = false;
	const FTutorialRewardBroadcastDecision PrepareBroadcastDecision =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			bIsTerminalMission,
			/*bSaveSucceeded=*/false);

	// 지급별 자동 저장을 끄고, 다음 미션/클레임 상태까지 전환한 뒤 아래에서 한 번만 저장한다.
	for (const FMissionReward& Reward : CompletedRow.Rewards)
	{
		if (ResMgr && Reward.ResourceType != EResourceType::None && Reward.Amount > 0)
		{
			ResMgr->StoreResource(
				Reward.ResourceType,
				Reward.Amount,
				/*bShouldSave=*/false,
				PrepareBroadcastDecision.bBroadcastDuringPrepare);
		}
		if (ItemMgr && Reward.ItemType != EItemType::None && Reward.ItemAmount > 0)
		{
			ItemMgr->AddItem(
				Reward.ItemType,
				Reward.ItemAmount,
				/*bShouldSave=*/false,
				PrepareBroadcastDecision.bBroadcastDuringPrepare);
		}
	}

	if (FTutorialMissionAtomicCommitRules::ShouldMarkTutorialCompleted(CompletedRow.NextMissionID))
	{
		bTutorialCompleted = true;
		bDevTutorialJumpSessionActive = FTutorialCompletionStateRules::ResolveDevJumpSessionActive(
			bDevTutorialJumpSessionActive,
			/*bJumpApplied=*/false,
			/*bReachedTerminalCompletion=*/true);
	}
	SetActiveMission(CompletedRow.NextMissionID);
	bool bPreparedGoalUnlock = false;
	if (GoalMgr && FTutorialMissionAtomicCommitRules::ShouldPrepareGoalBoardUnlock(CompletedRow.NextMissionID))
	{
		bPreparedGoalUnlock = GoalMgr->PrepareUnlockForAtomicSave();
	}

	// 보상·다음 미션·Ready clear·GoalBoard unlock을 하나의 디스크 스냅샷으로 커밋한다.
	const bool bSaveSucceeded = SaveMgr && SaveMgr->SaveGameData();
	const FTutorialMissionSaveDecision SaveDecision =
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(bIsTerminalMission, bSaveSucceeded);
	if (!SaveDecision.bCommitPreparedState)
	{
		if (bPreparedGoalUnlock && GoalMgr)
		{
			GoalMgr->RollbackPreparedUnlock();
		}

		SetActiveMission(CompletedID);
		GuidePhase = GuidePhaseBeforeCommit;
		ExplainStep = ExplainStepBeforeCommit;
		TransitionMissCount = TransitionMissCountBeforeCommit;
		DeskPlaceProgress = DeskPlaceProgressBeforeCommit;
		PhaseEnteredAt = PhaseEnteredAtBeforeCommit;
		bTutorialCompleted = bTutorialCompletedBeforeCommit;
		bDevTutorialJumpSessionActive = bDevJumpSessionBeforeCommit;
		bReadyToClaim = SaveDecision.bRestoreReadyToClaim && bReadyBeforeCommit;

		if (bHasCachedSaveSnapshot && SaveMgr)
		{
			if (USaveGame_GameData* CachedSaveAfterFailure = SaveMgr->GetCurrentSaveData())
			{
				CachedSaveAfterFailure->GameData = CachedGameDataBeforeCommit;
			}
		}
		if (ResMgr)
		{
			ResMgr->SetAllResources(
				ResourceBankBeforeCommit,
				PrepareBroadcastDecision.bBroadcastDuringRollback);
		}
		if (ItemMgr)
		{
			ItemMgr->SetAllItems(
				ItemStorageBeforeCommit,
				PrepareBroadcastDecision.bBroadcastDuringRollback);
		}

		bCompletingMission = false;
		UpdateGuideNotification();
		UE_LOG(LogTemp, Error,
			TEXT("[MissionManager] 미션 완료 저장 실패 — 준비 상태 롤백, 재클레임 가능: %s"),
			*CompletedID.ToString());
		return false;
	}
	if (!bSaveSucceeded)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MissionManager] 중간 미션 저장 실패 — 메모리 전환 유지, 다음 저장에서 재시도: %s"),
			*CompletedID.ToString());
	}

	const FTutorialRewardBroadcastDecision CommitBroadcastDecision =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			bIsTerminalMission,
			bSaveSucceeded);
	if (CommitBroadcastDecision.bPublishRewardsAfterCommit)
	{
		for (const FMissionReward& Reward : CompletedRow.Rewards)
		{
			if (ResMgr && Reward.ResourceType != EResourceType::None && Reward.Amount > 0)
			{
				ResMgr->OnResourceChanged.Broadcast(
					Reward.ResourceType,
					ResMgr->GetResourceAmount(Reward.ResourceType));
			}
			if (ItemMgr && Reward.ItemType != EItemType::None && Reward.ItemAmount > 0)
			{
				ItemMgr->OnItemChanged.Broadcast(
					Reward.ItemType,
					ItemMgr->GetItemCount(Reward.ItemType),
					Reward.ItemAmount);
			}
		}
	}

	if (SaveDecision.bPublishCompletionEvents)
	{
		OnMissionCompleted.Broadcast(CompletedID, CompletedRow);
	}

	if (HasActiveMission())
	{
		OnMissionActivated.Broadcast(ActiveMissionRow);
	}
	else
	{
		// 체인 끝 — 폴링만 멈추고 위젯은 완료 연출 후 스스로 숨는다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}

		// 체인 종료 = 미션판 언락 (스펙 2026-08-02 §5)
		if (GoalMgr)
		{
			GoalMgr->PublishPreparedUnlock();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MissionManager] 미션 완료: %s → 다음: %s"),
		*CompletedID.ToString(), *ActiveMissionID.ToString());

	bCompletingMission = false;

	// 체인 끝이면 폴링이 멈추므로 상주 알림을 여기서 직접 정리 (다음 미션이 있으면 폴링이 갱신)
	UpdateGuideNotification();

	// 다음 미션이 이미 충족 상태면 연쇄 완료 (재귀 깊이 = 체인 길이로 유계)
	TryCompleteByResources();
	return true;
}

void UMissionManagerSubsystem::TryCompleteByResources()
{
	if (!HasActiveMission())
	{
		return;
	}
	if (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		EvaluateRecruitEmployeesCondition();
		return;
	}
	if (ActiveMissionRow.ConditionType != EMissionConditionType::CollectBricks)
	{
		return;
	}

	UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (ResMgr && ResMgr->GetResourceAmount(EResourceType::Brick) >= ActiveMissionRow.ConditionAmount)
	{
		// 완료 정책은 다음 미션 유무로 즉시 진행(M1) 또는 최종 클레임(M7)을 결정한다.
		SetReadyToClaim();
	}
}

void UMissionManagerSubsystem::EvaluateRecruitEmployeesCondition()
{
	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::RecruitEmployees)
	{
		return;
	}

	int64 Current = 0;
	int64 Target = 0;
	if (!GetMissionProgress(Current, Target))
	{
		return;
	}

	if (FRecruitEmployeesMissionRules::HasReachedTarget(Current, Target))
	{
		SetReadyToClaim();
	}
	else
	{
		// 진행바 갱신 — OnGuidePhaseChanged 는 멘토 라인만 갱신해 바가 0/N 에 멈춘다.
		OnMissionProgressChanged.Broadcast();
	}
}

double UMissionManagerSubsystem::GetPhaseElapsedSeconds() const
{
	const UWorld* CurWorld = GetWorld();
	return CurWorld ? FMath::Max(0.0, CurWorld->GetTimeSeconds() - PhaseEnteredAt) : 0.0;
}

void UMissionManagerSubsystem::SetGuidePhase(int32 NewPhase)
{
	if (GuidePhase == NewPhase)
	{
		return;
	}

	// 설명 페이즈를 벗어나면 포커스를 놓는다 — 안 놓으면 가림 고스트가 남는다(카메라 SOT).
	// 페이즈 전이가 여기 한 곳뿐이라 탈출 경로(탭/타임아웃/운영없음 스킵)를 모두 덮는다
	if (IsExplainPhaseActive())
	{
		ReleaseExplainPresentation();
	}

	GuidePhase = NewPhase;
	bExplainAdvancePending = false;
	ExplainAdvancePendingAt = 0.0;
	// 설명 페이즈에 들어올 때마다 첫 설명부터 — 스텝은 세션 상태라 재진입이면 지난 값이 이어진다
	if (IsExplainPhaseActive())
	{
		ExplainStep = 0;
	}
	PhaseEnteredAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TransitionMissCount = 0;

	OnGuidePhaseChanged.Broadcast();
	UpdateGuideNotification();
}

void UMissionManagerSubsystem::ReleaseExplainPresentation()
{
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* Layer = UIMgr->GetInGameLayer())
		{
			Layer->ReleaseTutorialVaultGauge();
		}
	}
	if (APlayerCamera* Cam = FindPlayerCamera(GetWorld()))
	{
		Cam->ClearFocusTarget();
	}
}

bool UMissionManagerSubsystem::UsesDelayedHint() const
{
	return HasActiveMission() && !IsHardGateMission();
}

void UMissionManagerSubsystem::UpdateGuideNotification()
{
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return;
	}

	// 활성 미션의 현 페이즈 멘토 라인을 상단 상주 알림으로 — 모든 단계를 차근차근 안내.
	// 클레임 대기 중엔 해제(트래커 카드가 "완료! 탭" 안내를 담당해 중복 메시지 방지).
	if (HasActiveMission() && !bReadyToClaim)
	{
		UIMgr->ShowGuideNotification(GetCurrentMentorLine());
	}
	else if (HasActiveMission() && bReadyToClaim)
	{
		// 클레임 대기 — 상주 토스트로 미션 카드 탭 유도 (계속 띄워둠)
		UIMgr->ShowGuideNotification(FText::FromString(TEXT("미션 완료! 미션 카드를 눌러 보상을 받으세요")));
	}
	else
	{
		// ⚠ 미션판 경로는 **이 else 안**이어야 한다 — 위 두 분기는 `HasActiveMission()` 가드라
		// 그 안에 끼우면 활성 미션이 없는 미션판 상황에서 영영 안 뜬다(2026-08-05 OnConditionSignal 사고와 동형).
		const FText StepLine = GetTrackedGoalStepLine();
		if (!StepLine.IsEmpty())
		{
			UIMgr->ShowGuideNotification(StepLine);
		}
		else
		{
			UIMgr->ClearGuideNotification();
		}
	}
}

void UMissionManagerSubsystem::PollGuideProgress()
{
	// 상주 알림 리프레시 — 스포트라이트 대상(공장) 캐시가 첫 틱에 늦게 잡히는 타이밍 흡수 (멱등)
	UpdateGuideNotification();

	// (폴백→임베드 승격 로직 제거 — OnLevelLayerReady 신호 뒤에 EnsureMissionWidgets 가 돌므로 레이어가 항상 준비된 상태라
	//  임베드 트래커를 처음부터 직접 잡는다. 폴백은 레이어에 임베드가 아예 없을 때만 생기는 최후수단으로, 승격 불요.)

	// CollectBricks — 패널을 먼저 연 플레이어 구제: OpenFactory 인데 패널이 이미 열려 있으면 강화 유도로
	if (HasActiveMission() && ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::OpenFactory && FactoryPanelWeak.IsValid())
	{
		SetGuidePhase(BrickGuide::NudgeUpgrade);
	}

	// M3 EnterOffice — [입장] 페이즈인데 관리 패널이 닫혔으면(미입장) 빌딩 클릭 유도로 후퇴 (2틱 보호).
	// 클레임 대기(오피스 진입 완료) 중엔 후퇴 없음.
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice)
	{
		if (GuidePhase == EnterOfficeGuide::PressEnter && !ManagePanelWeak.IsValid())
		{
			if (++TransitionMissCount >= 2)
			{
				SetGuidePhase(EnterOfficeGuide::ClickBuilding);
			}
		}
		else
		{
			TransitionMissCount = 0;
		}
		return;
	}

	// M5 — 채용 패널/연출이 닫혔으면(취소) 이전 페이즈로 후퇴 (2틱 보호)
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		if (GuidePhase == RecruitGuide::PressConfirm && !GachaPresentationWeak.IsValid())
		{
			if (++TransitionMissCount >= 2) { SetGuidePhase(RecruitGuide::PressPull); }
		}
		else if (GuidePhase == RecruitGuide::PressPull && !RecruitPanelWeak.IsValid())
		{
			if (++TransitionMissCount >= 2) { SetGuidePhase(RecruitGuide::OpenRecruit); }
		}
		else
		{
			TransitionMissCount = 0;
		}
		return;
	}

	// M4 — 연속 배치 안내 중 배치 모드를 나갔으면([닫기]/취소) 버튼 유도로 후퇴 (2틱 보호).
	// PlaceMore 는 배치 확정 직후에만 들어오는 페이즈라, 모드가 꺼졌다 = 플레이어가 배치를 끝내고 나갔다는 뜻.
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks
		&& GuidePhase == DeskGuide::PlaceMore)
	{
		if (!IsDeskPlacementActive())
		{
			if (++TransitionMissCount >= 2) { SetGuidePhase(DeskGuide::OpenPlacement); }
		}
		else
		{
			TransitionMissCount = 0;
		}
		return;
	}

	// M6 — 스트립 관찰은 누를 대상이 없어 신호가 없다. 시간으로 이탈시킨다(가르치고 비켜준다).
	if (HasActiveMission() && !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase == InHouseGuide::WatchStrip)
	{
		const UWorld* GuideWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		if (GuideWorld && GuideWorld->GetTimeSeconds() - StripWatchEnteredAt >= InHouseGuide::StripWatchSeconds)
		{
			SetGuidePhase(InHouseGuide::PressLaunch);
		}
	}

	if (!HasActiveMission() || ActiveMissionRow.ConditionType != EMissionConditionType::BuildFirstBuilding)
	{
		return;
	}

	if (GuidePhase == BuildGuide::PickGameCard)
	{
		if (BuildModalWeak.IsValid() && BuildModalWeak->IsActivated())
		{
			TransitionMissCount = 0;
		}
		else if (IsPlacementActive())
		{
			SetGuidePhase(BuildGuide::PlaceBuilding);
		}
		else if (++TransitionMissCount >= 2)
		{
			// 모달을 그냥 닫음 — 처음부터 다시 유도
			SetGuidePhase(BuildGuide::PressBuild);
		}
	}
	else if (GuidePhase == BuildGuide::PlaceBuilding)
	{
		if (IsPlacementActive())
		{
			TransitionMissCount = 0;
		}
		else if (++TransitionMissCount >= 2)
		{
			// 배치 취소 — 확정이면 NotifyBuildingPlaced가 먼저 와서 이 페이즈를 벗어남
			SetGuidePhase(BuildGuide::PressBuild);
		}
	}
	else
	{
		TransitionMissCount = 0;
	}
}

bool UMissionManagerSubsystem::IsPlacementActive() const
{
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
	return Handler && Handler->HasActivePlacement();
}

bool UMissionManagerSubsystem::IsDeskPlacementActive() const
{
	// 프리뷰 액터 유무(HasActivePlacement)가 아니라 모드로 본다 — 연속 배치는 확정 직후 프리뷰를 옆으로 재배치하는
	// 구간이 있어, 그 프레임에 프리뷰가 없다고 "나갔다"로 오판하면 안 된다. 모드는 EndWorkstationPlacement 에서만 꺼진다.
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
	return Handler && Handler->IsWorkstationMode();
}

bool UMissionManagerSubsystem::HasActiveMission() const
{
	return !ActiveMissionID.IsNone();
}

bool UMissionManagerSubsystem::GetActiveMission(FMissionTable& OutMission) const
{
	if (!HasActiveMission())
	{
		return false;
	}

	OutMission = ActiveMissionRow;
	return true;
}

namespace
{
	// 레이아웃 미산출 1프레임과 진짜 고장을 구분하기 위한 지연(약 0.5초 @60fps)
	constexpr int32 GuideTargetProblemWarnTicks = 30;
}

bool UMissionManagerSubsystem::IsGuideTargetActionable(UWidget* Target, FString& OutReason) const
{
	if (!Target)
	{
		OutReason = TEXT("null");
		return false;
	}

	const ESlateVisibility Vis = Target->GetVisibility();
	if (Vis == ESlateVisibility::Collapsed)
	{
		OutReason = TEXT("Collapsed");
		return false;
	}
	if (Vis == ESlateVisibility::Hidden)
	{
		OutReason = TEXT("Hidden");
		return false;
	}

	// 래퍼 UUserWidget(UpgradeBtnWidget 등)은 SetEnabled 가 내부 버튼만 끄고 래퍼 자신은 항상 GetIsEnabled()==true.
	// 신형("Btn")/구형("UpgradeButton") WBP 가 공존해 둘 다 시도(신형 우선 — 오버레이 언랩과 동일 순서).
	UWidget* EnableCheckTarget = Target;
	if (!Cast<UCommonButtonBase>(Target))
	{
		if (UUserWidget* Wrapper = Cast<UUserWidget>(Target))
		{
			if (UWidget* InnerBtn = Wrapper->GetWidgetFromName(TEXT("Btn")))
			{
				EnableCheckTarget = InnerBtn;
			}
			else if (UWidget* LegacyBtn = Wrapper->GetWidgetFromName(TEXT("UpgradeButton")))
			{
				EnableCheckTarget = LegacyBtn;
			}
		}
	}

	if (!EnableCheckTarget->GetIsEnabled())
	{
		OutReason = TEXT("Disabled");
		return false;
	}

	const FVector2D TargetSize = Target->GetCachedGeometry().GetLocalSize();
	if (TargetSize.X <= KINDA_SMALL_NUMBER || TargetSize.Y <= KINDA_SMALL_NUMBER)
	{
		OutReason = TEXT("ZeroSize");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool UMissionManagerSubsystem::IsInputGated() const
{
	// 미션 없음/클레임 대기 중엔 게이트 없음 (클레임은 오버레이가 풀스크린 탭으로 별도 처리).
	// 래치도 함께 정리 — 안 그러면 이 경로를 거친 뒤 같은 문제가 재발했을 때 Ticks 가 이미 임계값을
	// 넘어 있어 두 번째 경고가 영영 안 뜬다.
	if (!HasActiveMission() || bReadyToClaim)
	{
		if (!LastGuideTargetProblemKey.IsEmpty())
		{
			LastGuideTargetProblemKey.Reset();
			GuideTargetProblemTicks = 0;
		}
		return false;
	}

	// 소프트 존(bHardGate=false) — 입력을 막지 않는다. 유도는 지연 힌트(오버레이 드로잉)가 담당.
	if (!ActiveMissionRow.bHardGate)
	{
		return false;
	}

	// 타겟이 있는데 누를 수 없는 상태면 게이트를 풀어 소프트락을 막는다(비활성 [+], Hidden 등).
	// 타겟이 아예 없는 페이즈(배치 확정 등)는 아래 switch 가 판정한다.
	if (UWidget* Target = GetCurrentHighlightTarget())
	{
		FString Reason;
		if (!IsGuideTargetActionable(Target, Reason))
		{
			// ConditionType 은 여러 미션이 공유할 수 있어 키 충돌 위험 — ActiveMissionID 로 통일(로그와도 동일 기준).
			const FString Key = FString::Printf(TEXT("%s|%d|%s"),
				*ActiveMissionID.ToString(), GuidePhase, *Reason);

			if (Key != LastGuideTargetProblemKey)
			{
				LastGuideTargetProblemKey = Key;
				GuideTargetProblemTicks = 0;
			}
			else if (++GuideTargetProblemTicks == GuideTargetProblemWarnTicks)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[MissionGuide] 가이드 타겟을 누를 수 없습니다 ― Mission=%s Phase=%d Reason=%s. 입력 게이트를 해제합니다."),
					*ActiveMissionID.ToString(), GuidePhase, *Reason);
			}
			return false;
		}
	}

	if (!LastGuideTargetProblemKey.IsEmpty())
	{
		LastGuideTargetProblemKey.Reset();
		GuideTargetProblemTicks = 0;
	}

	// 월드 액터 스포트라이트 페이즈는 게이트를 걸지 않는다(공장/빌딩/도시 회사 등).
	// 구멍 밖을 막으면 카메라 이동·줌이 함께 죽어 화면 밖 대상을 찾아갈 수 없다("다 먹통").
	// 액터 클릭 자체는 구멍의 뷰포트 트레이스로 되고 딤/셰브론도 그대로 남으므로 유도력은 유지된다.
	// (WatchStrip 과 같은 "타겟은 있는데 게이트만 끄는" 부류 — 케이스별 열거 대신 일반 규칙으로 둔다.)
	if (GetCurrentSpotlightActor())
	{
		return false;
	}

	// 드래그/배치 확정 서브페이즈만 자유 조작(게이트 OFF) — 월드로 드래그·카메라 조작이 필요.
	// 배치 "버튼 누르기" 서브페이즈(PressBuild/OpenPlacement)는 게이트 ON(그 버튼만 허용).
	switch (ActiveMissionRow.ConditionType)
	{
	case EMissionConditionType::BuildFirstBuilding:
		// PickGameCard 도 OFF — 어느 빌딩 카드든 눌러야 하니 구멍 하나로 좁히면 안 된다(타겟도 없다).
		return GuidePhase != BuildGuide::PlaceBuilding && GuidePhase != BuildGuide::PickGameCard;
	case EMissionConditionType::PlaceDesks:
		// 배치/연속배치 중엔 게이트 OFF — 월드 탭·카메라 조작이 필요하다. 버튼 유도 페이즈만 ON.
		return GuidePhase == DeskGuide::OpenPlacement;
	case EMissionConditionType::LaunchFirstInHouseProject:
		// 스트립은 누를 수 없는 표시용 위젯 — 게이트를 켜면 개발 중 유일한 선택지인 부스트 도박 모달까지 막혀 타임아웃된다.
		// PickInHouse 도 OFF — 3안 중 무엇을 고를지는 플레이어 몫이라 카드 한 장으로 구멍을 좁히지 않는다.
		return GuidePhase != InHouseGuide::WatchStrip && GuidePhase != InHouseGuide::PickInHouse;
	default:
		return true;
	}
}

UWidget* UMissionManagerSubsystem::GetClaimSpotlightTarget() const
{
	// 클레임 대기 시 오버레이가 비출 대상 = 트래커 카드. 없으면 nullptr → 오버레이는 풀스크린 딤만.
	// 임베드 트래커가 조상 Collapse 로 화면에 없으면(하단 패널이 OfficeMain 을 덮은 상태 등)
	// 캐시 지오메트리가 옛 위치를 가리켜 유령 스포트라이트가 생김 → 풀스크린 딤으로 강등.
	UMissionTrackerWidget* Tracker = TrackerWidget.Get();
	if (!Tracker)
	{
		return nullptr;
	}
	for (TSharedPtr<SWidget> Cur = Tracker->GetCachedWidget(); Cur.IsValid(); Cur = Cur->GetParentWidget())
	{
		if (!Cur->GetVisibility().IsVisible())
		{
			return nullptr;
		}
	}
	return Tracker;
}

UWidget* UMissionManagerSubsystem::GetCurrentHighlightTarget() const
{
	// 클레임 대기 중엔 유도 없음 — 카드 탭만 기다린다
	if (!HasActiveMission() || bReadyToClaim)
	{
		return nullptr;
	}

	// 벽돌 노가다 유도 — 0 공장 액터 스포트라이트, 1 [공장] 버튼(패널 열기), 2 강화 버튼 넛지
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks)
	{
		if (GuidePhase == BrickGuide::OpenFactory)
		{
			// [공장] 버튼 — 공장 패널 열기 유도
			return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetFactoryOpenButtonWidget() : nullptr;
		}
		if (GuidePhase == BrickGuide::NudgeUpgrade)
		{
			// 패널이 열려 있으면 첫 강화 슬롯, 닫혀 있으면 [공장] 버튼으로 재유도
			if (FactoryPanelWeak.IsValid())
			{
				if (UWidget* Nudge = FactoryPanelWeak->GetUpgradeNudgeTarget())
				{
					return Nudge;
				}
			}
			return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetFactoryOpenButtonWidget() : nullptr;
		}
		if (GuidePhase == BrickGuide::NudgeUpgrade2)
		{
			// 아래(둘째) 강화 슬롯, 닫혀 있으면 [공장] 버튼으로 재유도
			if (FactoryPanelWeak.IsValid())
			{
				if (UWidget* Nudge = FactoryPanelWeak->GetSecondUpgradeNudgeTarget())
				{
					return Nudge;
				}
			}
			return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetFactoryOpenButtonWidget() : nullptr;
		}
		return nullptr;
	}

	// M3 EnterOffice — p0(ClickBuilding)은 빌딩 월드 스포트라이트(GetCurrentSpotlightActor)라 링 없음,
	// p1(PressEnter)에서 ManagePanel [입장] 버튼에 링.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice)
	{
		if (GuidePhase == EnterOfficeGuide::PressEnter)
		{
			return ManagePanelWeak.IsValid() ? ManagePanelWeak->GetEnterOfficeButtonWidget() : nullptr;
		}
		return nullptr;
	}

	// M5 RecruitEmployees — p0 [채용] 도크 버튼 / p1 [×N 한번에 채용] / p2 [확인] / p3 [닫기]
	if (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		if (GuidePhase == RecruitGuide::OpenRecruit)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetRecruitmentButtonWidget() : nullptr;
		if (GuidePhase == RecruitGuide::PressPull)
			return RecruitPanelWeak.IsValid() ? RecruitPanelWeak->GetPullButtonMultiWidget() : nullptr;
		if (GuidePhase == RecruitGuide::PressConfirm)
			return GachaPresentationWeak.IsValid() ? GachaPresentationWeak->GetConfirmButtonWidget() : nullptr;
		if (GuidePhase == RecruitGuide::PressCloseRecruit)
			return RecruitPanelWeak.IsValid() ? RecruitPanelWeak->GetCloseButtonWidget() : nullptr;
		return nullptr;
	}

	// M4 PlaceDesks — p0 [책상 배치] 버튼 / p1·p2 배치 바 [배치] 링 (드래그 손은 제스처 채널)
	if (ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks)
	{
		if (GuidePhase == DeskGuide::OpenPlacement)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetWorkstationOpenButtonWidget() : nullptr;
		if (GuidePhase == DeskGuide::Confirm || GuidePhase == DeskGuide::PlaceMore)
			return BuildPlacementPanelWeak.IsValid() ? BuildPlacementPanelWeak->GetPlaceButtonWidget() : nullptr;
		return nullptr;
	}

	// M6 LaunchFirstInHouseProject — p0 [새 프로젝트] / p1 기획 보드 추천 카드 CTA / p2 개발 스트립 관찰 / p3 LaunchConfirm [출시](누르면 즉시 미션 완료)
	if (ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject)
	{
		if (GuidePhase == InHouseGuide::OpenBoard)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetOpenBoardButtonWidget() : nullptr;
		// 앵커는 추천 카드 CTA 하나 — 타일 12칸 중 하나를 지목하면 선택을 대신해 버린다(스펙 §6.3)
		if (GuidePhase == InHouseGuide::PickInHouse)
			return PitchBoardWeak.IsValid() ? PitchBoardWeak->GetRecommendCardCtaWidget() : nullptr;
		if (GuidePhase == InHouseGuide::WatchStrip)
			return OfficeMainWeak.IsValid() ? OfficeMainWeak->GetStripRowWidget() : nullptr;
		if (GuidePhase == InHouseGuide::PressLaunch)
			return LaunchConfirmWeak.IsValid() ? LaunchConfirmWeak->GetConfirmButtonWidget() : nullptr;
		return nullptr;
	}

	// M7 CollectFirstRevenue — p0 사무실 [뒤로] / p1 순차 설명 / p2 [수집]
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue)
	{
		if (GuidePhase == CollectGuide::ExitOffice)
			return OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
		if (GuidePhase == CollectGuide::PressCollect)
			return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetCollectAllButtonWidget() : nullptr;
		return nullptr;
	}

	if (ActiveMissionRow.ConditionType != EMissionConditionType::BuildFirstBuilding)
	{
		return nullptr;
	}

	if (GuidePhase == BuildGuide::PressBuild)
	{
		return BuildOpenWidgetWeak.IsValid() ? BuildOpenWidgetWeak->GetBuildOpenButtonWidget() : nullptr;
	}
	if (GuidePhase == BuildGuide::PlaceBuilding)
	{
		return BuildPlacementPanelWeak.IsValid() ? BuildPlacementPanelWeak->GetPlaceButtonWidget() : nullptr;
	}
	// PickGameCard — 링/딤 없음. 어느 빌딩을 세우냐가 곧 플레이어의 선택이고, 문구도 "원하는 빌딩"이라
	// 첫 카드만 밝히면 안내와 화면이 어긋난다. 산업(게임)은 이미 기본 선택이라 모달 자체가 범위를 좁혀 준다.
	return nullptr;
}

void UMissionManagerSubsystem::GetCurrentHighlightExtraTargets(TArray<UWidget*>& OutExtras) const
{
	OutExtras.Reset();

	if (!HasActiveMission() || bReadyToClaim)
	{
		return;
	}

	// M6 WatchStrip — 점수(스트립 행)와 배속 토글을 같은 딤에서 함께 보여준다.
	// 점수만 뚫으면 "기다리는 화면"으로 읽혀 배속이 있다는 걸 못 배운 채 15초를 흘려보낸다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::LaunchFirstInHouseProject
		&& GuidePhase == InHouseGuide::WatchStrip
		&& OfficeMainWeak.IsValid())
	{
		if (UWidget* SpeedBtn = OfficeMainWeak->GetStripSpeedButtonWidget())
		{
			OutExtras.Add(SpeedBtn);
		}
	}
}

AActor* UMissionManagerSubsystem::GetCurrentSpotlightActor() const
{
	if (!HasActiveMission() || bReadyToClaim)
	{
		return nullptr;
	}

	// 벽돌 노가다 — 공장 액터 스포트라이트 (TapFactory 페이즈)
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks
		&& GuidePhase == BrickGuide::TapFactory)
	{
		if (!BrickFactoryWeak.IsValid())
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(
				GetGameInstance()->GetWorld(), ABrickFactory::StaticClass());
		}
		return BrickFactoryWeak.Get();
	}

	// M3 EnterOffice — 지은 회사 빌딩 스포트라이트 (ClickBuilding 페이즈, MainMap).
	// 캡처가 없으면(예: 오피스 직접 재접속 후 MainMap 복귀) null → 스포트라이트 생략, 멘토 라인만 유지.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::EnterOffice
		&& GuidePhase == EnterOfficeGuide::ClickBuilding)
	{
		return TutorialFirstBuildingWeak.Get();
	}

	return nullptr;
}

EGestureHintKind UMissionManagerSubsystem::GetCurrentGesture() const
{
	if (!HasActiveMission() || bReadyToClaim) return EGestureHintKind::None;
	return GuideGestureRules::ResolveGesture(ActiveMissionRow.ConditionType, GuidePhase);
}

float UMissionManagerSubsystem::GetGuideDimScale() const
{
	if (!HasActiveMission() || bReadyToClaim) return 1.f;
	return GuideGestureRules::ResolveDimScale(ActiveMissionRow.ConditionType, GuidePhase);
}

AActor* UMissionManagerSubsystem::GetCurrentGestureAnchorActor() const
{
	if (!HasActiveMission() || bReadyToClaim) return nullptr;
	switch (GuideGestureRules::ResolveAnchor(ActiveMissionRow.ConditionType, GuidePhase))
	{
	case EGestureAnchorKind::BrickFactory:
		if (!BrickFactoryWeak.IsValid())
		{
			BrickFactoryWeak = UGameplayStatics::GetActorOfClass(GetGameInstance()->GetWorld(), ABrickFactory::StaticClass());
		}
		return BrickFactoryWeak.Get();
	case EGestureAnchorKind::PlacementPreview:
	{
		// 배치 프리뷰는 플레이어 폰의 PlacementHandler 가 소유한다 (IsPlacementActive 와 같은 경로)
		UWorld* GuideWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		APlayerController* PC = GuideWorld ? GuideWorld->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UPlacementHandler* Handler = Pawn ? Pawn->FindComponentByClass<UPlacementHandler>() : nullptr;
		return Handler ? Handler->GetPlacementTargetActor() : nullptr;
	}
	default:
		return nullptr;
	}
}

bool UMissionManagerSubsystem::IsExplainPhaseActive() const
{
	return HasActiveMission()
		&& !bReadyToClaim
		&& ActiveMissionRow.ConditionType == EMissionConditionType::CollectFirstRevenue
		&& GuidePhase == CollectGuide::Explain;
}

bool UMissionManagerSubsystem::IsExplainCameraSettled() const
{
	// 카메라를 못 찾으면 기다리지 않는다 ― 못 찾는 상황에서 대기하면 타임아웃까지 화면이 딤인 채 멈춘다
	const APlayerCamera* Cam = FindPlayerCamera(GetWorld());
	return Cam == nullptr || Cam->IsFocusVisuallySettled();
}

bool UMissionManagerSubsystem::GetExplainTargetForStep(int32 Step, UWidget*& OutTarget, FGuideExplainCopy& OutCopy) const
{
	OutTarget = nullptr;
	OutCopy = FGuideExplainCopy();

	if (!IsExplainPhaseActive() || Step < 0 || Step >= ExplainStepCount)
	{
		return false;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UInGameLayerWidget* Layer = UIMgr ? UIMgr->GetInGameLayer() : nullptr;
	ABuildingBaseActor* Building = TutorialFirstBuildingWeak.Get();
	if (!Layer || !Building)
	{
		return false;
	}
	const int32 Idx = Building->GetBuildingIndex();

	// 스텝 순서 = 카피 순서. 한 번에 한 장만 뜨므로 방향이 서로 충돌하지 않는다
	UWidget* const StepTargets[ExplainStepCount] = {
		Layer->GetRevenueRateChipWidget(),
		Layer->GetVaultGaugeWidgetForBuilding(Idx),
	};
	const FGuideExplainCopy StepCopy[ExplainStepCount] = {
		{ NSLOCTEXT("Guide", "ExplainRateEyebrow", "수입"),
		  NSLOCTEXT("Guide", "ExplainRateBody",   "회사가 지금 벌어들이는 수입입니다."),
		  EGuideTooltipDir::Up },
		{ NSLOCTEXT("Guide", "ExplainBarEyebrow", "진행"),
		  NSLOCTEXT("Guide", "ExplainBarBody",    "다 차면 프로젝트가 끝납니다."),
		  EGuideTooltipDir::Right },
	};

	// 이 스텝 하나만 본다 — 둘을 한꺼번에 요구하면 Bar 하나가 없을 때 수입 설명까지 못 뜬다.
	FString Reason;
	if (!IsGuideTargetActionable(StepTargets[Step], Reason))
	{
		// 어느 스텝에서 막혔는지가 다음 진단의 출발점이다. 매 틱 찍으면 로그가 잠기므로 사유가 바뀔 때만 남긴다
		static FString LastExplainTargetReason;
		const FString Line = FString::Printf(TEXT("스텝 %d/%d X(%s)"), Step + 1, ExplainStepCount, *Reason);
		if (Line != LastExplainTargetReason)
		{
			LastExplainTargetReason = Line;
			UE_LOG(LogTemp, Log, TEXT("[MissionManager] 설명 타겟 미해결: %s"), *Line);
		}
		return false;
	}

	OutTarget = StepTargets[Step];
	OutCopy = StepCopy[Step];
	return true;
}

void UMissionManagerSubsystem::AdvanceExplainPhase(bool bWaitForNextTarget)
{
	if (!IsExplainPhaseActive() || bExplainAdvancePending)
	{
		return;
	}

	const int32 NextStep = ResolveNextExplainStep(ExplainStep, ExplainStepCount);
	if (!bWaitForNextTarget && NextStep != INDEX_NONE)
	{
		ExplainStep = NextStep;
		return;
	}
	UWidget* NextTarget = nullptr;
	FGuideExplainCopy NextCopy;
	const bool bNextTargetReady = NextStep != INDEX_NONE
		&& GetExplainTargetForStep(NextStep, NextTarget, NextCopy);

	switch (ResolveGuideExplainAdvanceAction(ExplainStep, ExplainStepCount, bNextTargetReady))
	{
	case EGuideExplainAdvanceAction::FinishExplainPhase:
		SetGuidePhase(CollectGuide::PressCollect);
		return;
	case EGuideExplainAdvanceAction::CommitNextStep:
		ExplainStep = NextStep;
		return;
	case EGuideExplainAdvanceAction::WaitForNextTarget:
		bExplainAdvancePending = true;
		ExplainAdvancePendingAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		return;
	default:
		return;
	}
}

void UMissionManagerSubsystem::ResolvePendingExplainAdvance(float TimeoutSeconds)
{
	if (!bExplainAdvancePending)
	{
		return;
	}
	if (!IsExplainPhaseActive())
	{
		bExplainAdvancePending = false;
		ExplainAdvancePendingAt = 0.0;
		return;
	}

	const int32 NextStep = ResolveNextExplainStep(ExplainStep, ExplainStepCount);
	UWidget* NextTarget = nullptr;
	FGuideExplainCopy NextCopy;
	const bool bNextTargetReady = NextStep != INDEX_NONE
		&& GetExplainTargetForStep(NextStep, NextTarget, NextCopy);
	if (ResolveGuideExplainAdvanceAction(ExplainStep, ExplainStepCount, bNextTargetReady)
		== EGuideExplainAdvanceAction::CommitNextStep)
	{
		ExplainStep = NextStep;
		bExplainAdvancePending = false;
		ExplainAdvancePendingAt = 0.0;
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : ExplainAdvancePendingAt;
	if (Now - ExplainAdvancePendingAt < TimeoutSeconds)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[MissionGuide] 다음 설명 타겟이 %.0f초간 준비되지 않아 해당 설명을 자동 통과합니다(스텝 %d)."),
		TimeoutSeconds, NextStep + 1);
	bExplainAdvancePending = false;
	ExplainAdvancePendingAt = 0.0;
	const int32 StepAfterSkipped = ResolveNextExplainStep(NextStep, ExplainStepCount);
	if (StepAfterSkipped == INDEX_NONE)
	{
		SetGuidePhase(CollectGuide::PressCollect);
	}
	else
	{
		ExplainStep = StepAfterSkipped;
	}
}

ABuildingBaseActor* UMissionManagerSubsystem::PickGuideBuilding() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	UEntityManager* EntityMgr = World ? World->GetSubsystem<UEntityManager>() : nullptr;
	if (!EntityMgr)
	{
		return nullptr;
	}
	const TArray<ABuildingBaseActor*>& Buildings = EntityMgr->GetBuildings();
	if (Buildings.Num() == 1)
	{
		return Buildings[0];
	}
	// 여러 채면 "늘 가던 곳"을 고른다 — 어느 회사든 목표는 성립하므로 틀려도 손해가 없다.
	// 기록이 없으면(세션 첫 진입) 임의로 고르지 않고 지목을 생략한다
	const int32 LastIdx = UCGGameInstance::GetInstance()
		? UCGGameInstance::GetInstance()->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	return EntityMgr->GetBuildingByIndex(LastIdx);
}

namespace CGRGoalGuideStep
{
// G9 스킬 걸음 — 2번(SP 투자) → 3번(별 강화) 전이 판정.
// 투자 이력이 생기면 넘어간다: DT 문구가 "투자하고, 강화를 1회" 이므로 1점만 넣어도 다음 걸음이다
// (전량 소진을 요구하면 SP 가 남은 플레이어는 같은 문구에 갇힌다 — 2026-08-13 신고).
// SP 가 0 이면 [+] 가 비활성이라 지목이 무의미하므로 이력이 없어도 넘어간다.
int32 ResolveStatInvestStep(bool bHasInvestedAny, bool bInvestCellEnabled)
{
	return (bHasInvestedAny || !bInvestCellEnabled) ? 3 : 2;
}

int32 ResolveRaiseFloorStep(bool bInOffice, bool bManagePanelOpen, bool bEnhancementTabActive)
{
	if (bInOffice)
	{
		return 0;
	}
	if (!bManagePanelOpen)
	{
		return 1;
	}
	return bEnhancementTabActive ? 3 : 2;
}
}

namespace
{
// 누구든 직능에 1점이라도 넣었는가 — 세이브 파생(InvestedDisciplinePoints)이라 재접속에도 유지된다.
// 로스터 게터가 사본을 주는 것은 이 클래스의 기존 관례 — 호출은 [강화] 가능한 프레임으로 한정해 상한을 둔다
bool HasAnyInvestedDisciplinePoint(const UEmployeeManager* EmpMgr)
{
	if (!EmpMgr)
	{
		return false;
	}
	for (const FEmployeeInstance& Emp : EmpMgr->GetAllEmployees())
	{
		for (int32 Invested : Emp.InvestedDisciplinePoints)
		{
			if (Invested > 0)
			{
				return true;
			}
		}
	}
	return false;
}
}

FGoalGuideStep UMissionManagerSubsystem::GetTrackedGoalStep() const
{
	FGoalGuideStep Step;

	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GoalMgr || !TableMgr || !GI)
	{
		return Step;
	}
	const FName TrackedID = GoalMgr->GetTrackedGoalID();
	FGoalTable Row;
	if (TrackedID.IsNone() || !TableMgr->GetGoalData(TrackedID, Row))
	{
		return Step;
	}

	// 판정에 쓰는 것은 전부 "지금 무엇이 열려 있는가" — 저장된 진행도가 아니다.
	// 그래서 플레이어가 어디로 새든 다음 조회에서 맞는 걸음이 나온다(후퇴 로직 불필요)
	const bool bInOffice = GI->IsInOfficeMap();
	UBuildingManagePanelWidget* Manage = ManagePanelWeak.Get();
	UBuildOpenWidget* Dock = BuildOpenWidgetWeak.Get();
	UOfficeMainWidget* OfficeDock = OfficeMainWeak.Get();

	// 0번은 항상 장소 이동 — 도시 목표인데 사무실에 있으면 [나가기] 로 되돌린다
	auto ExitOffice = [this, &Step]()
	{
		Step.Index = 0;
		Step.Anchor = OfficeLayerWeak.IsValid() ? OfficeLayerWeak->GetBackButtonWidget() : nullptr;
	};
	// 사무실 목표인데 도시에 있으면: 관리 패널이 열려 있으면 [입장], 아니면 회사 자체를 비춘다
	auto EnterOffice = [this, Manage, &Step]()
	{
		Step.Index = 0;
		if (Manage)
		{
			Step.Anchor = Manage->GetEnterOfficeButtonWidget();
		}
		else
		{
			Step.Spot = PickGuideBuilding();
		}
	};

	switch (Row.ConditionType)
	{
	case EMissionConditionType::PlaceFirstPainting:        // G7 그림 — 사무실
		if (!bInOffice) { EnterOffice(); }
		else { Step.Index = 1; Step.Anchor = OfficeDock ? OfficeDock->GetDecorationOpenButtonWidget() : nullptr; }
		break;

	case EMissionConditionType::InvestFirstStatPoint:      // G9 직원 성장 — 사무실, 완료는 [강화] 실행
		if (!bInOffice)
		{
			EnterOffice();
		}
		else if (UEnhanceStarforceModalWidget* Starforce = StarforceModalWeak.Get())
		{
			Step.Index = 3;
			Step.Anchor = Starforce->GetEnhanceActionButtonWidget();
		}
		else if (UEmployeeWindowWidget* EmpWin = EmployeeWindowWeak.Get())
		{
			// 투자 이력 판정은 세이브를 타는 파생값 — 재시작에도 유지되고 새 세이브 필드가 필요 없다 (G8/G10 보유 판정과 같은 축).
			// ⚠ 게터의 non-null 로 SP 유무를 대신 읽지 말 것: 셀은 항상 존재하고 SP 0 은 GetIsEnabled 로만 드러난다
			UWidget* Plus = EmpWin->GetFirstSkillInvestButtonWidget();
			const bool bInvestCellEnabled = Plus && Plus->GetIsEnabled();
			// SP 가 없으면 결과가 이미 3 이라 로스터를 훑지 않는다(매 틱 호출 경로)
			const bool bInvestedAny = bInvestCellEnabled
				&& HasAnyInvestedDisciplinePoint(GetGameInstance()->GetSubsystem<UEmployeeManager>());
			Step.Index = CGRGoalGuideStep::ResolveStatInvestStep(bInvestedAny, bInvestCellEnabled);
			Step.Anchor = (Step.Index == 2) ? Plus : EmpWin->GetEnhanceButtonWidget();
		}
		else
		{
			Step.Index = 1;
			Step.Anchor = OfficeDock ? OfficeDock->GetEmployeeButtonWidget() : nullptr;
		}
		break;

	case EMissionConditionType::EquipFirstSkin:            // G8 스킨 — 뽑기 → 관리패널 [스킨] 탭
	{
		// G10 특성과 같은 결함 — 뽑고 패널을 닫으면 "뽑으세요" 로 되돌아갔다(2026-08-13)
		const UBuildingSkinManagerSubsystem* SkinMgr = GetGameInstance()->GetSubsystem<UBuildingSkinManagerSubsystem>();
		const bool bOwnsSkin = SkinMgr && SkinMgr->HasAnyGachaSkin();

		if (bInOffice) { ExitOffice(); }
		else if (Manage) { Step.Index = 2; Step.Anchor = Manage->GetSkinTabButtonWidget(); }
		else if (bOwnsSkin) { Step.Index = 2; Step.Spot = PickGuideBuilding(); }
		else { Step.Index = 1; Step.Anchor = Dock ? Dock->GetGachaButtonWidget() : nullptr; }
		break;
	}

	case EMissionConditionType::EquipFirstTrait:           // G10 특성 — 뽑기 → 관리패널 [특성] 탭
	{
		// 이미 뽑아 둔 특성이 있으면 [뽑기] 걸음은 끝난 것이다 — 패널이 닫혀 있다고 뽑기로 되돌리면
		// 뽑고 나온 플레이어가 같은 문구에 갇힌다(2026-08-13 신고). 보유 판정은 세이브를 타므로 재시작에도 유지된다
		bool bOwnsTrait = false;
		if (UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			for (const TPair<FName, int32>& Owned : TraitMgr->GetInventory().OwnedTraits)
			{
				if (Owned.Value > 0) { bOwnsTrait = true; break; }
			}
		}

		if (bInOffice) { ExitOffice(); }
		else if (Manage) { Step.Index = 2; Step.Anchor = Manage->GetTraitTabButtonWidget(); }
		else if (bOwnsTrait) { Step.Index = 2; Step.Spot = PickGuideBuilding(); }
		else { Step.Index = 1; Step.Anchor = Dock ? Dock->GetGachaButtonWidget() : nullptr; }
		break;
	}

	case EMissionConditionType::RaiseBuildingFloor:        // G1 빌드업 — 도시 → 회사 → [강화] → 빌드업
		Step.Index = CGRGoalGuideStep::ResolveRaiseFloorStep(
			bInOffice,
			Manage != nullptr,
			Manage && Manage->IsEnhancementTabActive());
		if (Step.Index == 0)
		{
			ExitOffice();
		}
		else if (Step.Index == 1)
		{
			Step.Spot = PickGuideBuilding();
		}
		else if (Step.Index == 2)
		{
			Step.Anchor = Manage->GetEnhancementTabButtonWidget();
		}
		else
		{
			Step.Anchor = Manage->GetBuildingFloorUpgradeButtonWidget();
		}
		break;

	case EMissionConditionType::ReachHQLevel:              // G2 본사 — 도크 [본사] (패널 내부 앵커는 게터 없음, Desc 가 담당)
		if (bInOffice) { ExitOffice(); }
		else { Step.Index = 1; Step.Anchor = Dock ? Dock->GetHeadquartersButtonWidget() : nullptr; }
		break;

	case EMissionConditionType::BuildSecondBuilding:       // G3 / G6 건설 — 도크 [건설] → 모달(카드 지목 없음)
	case EMissionConditionType::BuildOnClearedPlot:
		if (bInOffice)
		{
			ExitOffice();
		}
		else if (BuildModalWeak.IsValid())
		{
			// 모달까지 왔으면 안내는 끝 — 어느 빌딩을 세울지는 플레이어 몫이라 카드 한 장을 지목하지 않는다.
			// (AcquireFirstCompany 와 같은 원칙. 문구는 StepLines[2] 가 담당)
			Step.Index = 2;
		}
		else
		{
			Step.Index = 1;
			Step.Anchor = Dock ? Dock->GetBuildOpenButtonWidget() : nullptr;
		}
		break;

	case EMissionConditionType::AcquireFirstCompany:       // G4 / G5 / G11 — 앵커 없음
	case EMissionConditionType::DemolishFirstCompany:
	case EMissionConditionType::AcquireFirstPlot:
		// 어느 회사를 인수·철거하고 어느 부지를 살지가 곧 게임플레이다. 지목하면 플레이어의 선택을 대신해 버린다 — 문구만 안내한다
		// (2026-08-13 G4 최저가 회사 스포트라이트를 넣었다가 같은 날 철회 — 사용자 결정)
		if (bInOffice) { ExitOffice(); }
		else { Step.Index = 1; }
		break;

	default:
		break;
	}
	return Step;
}

FText UMissionManagerSubsystem::GetTrackedGoalStepLine() const
{
	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!GoalMgr || !TableMgr)
	{
		return FText::GetEmpty();
	}
	FGoalTable Row;
	if (!TableMgr->GetGoalData(GoalMgr->GetTrackedGoalID(), Row))
	{
		return FText::GetEmpty();
	}
	const int32 Index = GetTrackedGoalStep().Index;
	return Row.StepLines.IsValidIndex(Index) ? Row.StepLines[Index] : FText::GetEmpty();
}

UWidget* UMissionManagerSubsystem::GetTrackedGoalHighlightTarget() const
{
	return GetTrackedGoalStep().Anchor;
}

FText UMissionManagerSubsystem::GetTrackedGoalPlaceHint() const
{
	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GoalMgr || !TableMgr || !GI)
	{
		return FText::GetEmpty();
	}
	const FName TrackedID = GoalMgr->GetTrackedGoalID();
	FGoalTable Row;
	if (TrackedID.IsNone() || !TableMgr->GetGoalData(TrackedID, Row))
	{
		return FText::GetEmpty();
	}

	// 장소 판정을 여기서 또 하지 않는다 — **0번 걸음이 곧 "장소가 어긋났다"** 이므로 그 결과를 그대로 읽는다.
	// (판정을 두 곳에 두면 한쪽만 고쳐져 칩과 링이 서로 다른 말을 하게 된다)
	if (GetTrackedGoalStep().Index != 0)
	{
		return FText::GetEmpty();
	}
	return GI->IsInOfficeMap()
		? NSLOCTEXT("GoalTracker", "PlaceCity", "도시에서")
		: NSLOCTEXT("GoalTracker", "PlaceOffice", "사무실에서");
}

AActor* UMissionManagerSubsystem::GetTrackedGoalSpotlightActor() const
{
	// 대상 선정은 GetTrackedGoalStep 이 소유한다 (구 버전은 체인이 캡처한 TutorialFirstBuildingWeak 를 썼는데
	// 맵을 한 번 왕복하면 액터가 재생성돼 죽는다 — PickGuideBuilding 이 그 자리를 대신한다)
	return GetTrackedGoalStep().Spot;
}

FText UMissionManagerSubsystem::GetCurrentMentorLine() const
{
	if (!HasActiveMission())
	{
		return FText::GetEmpty();
	}

	if (!ActiveMissionRow.MentorLines.IsValidIndex(GuidePhase))
	{
		return FText::GetEmpty();
	}

	const FText& Line = ActiveMissionRow.MentorLines[GuidePhase];

	// 연속 배치 안내만 남은 개수를 채운다 — 문안은 DT 가 SOT, 코드는 {0} 인자만 공급한다
	if (ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks && GuidePhase == DeskGuide::PlaceMore)
	{
		const int32 Target = FMath::Max(1, static_cast<int32>(ActiveMissionRow.ConditionAmount));
		return FText::Format(Line, FText::AsNumber(FMath::Max(0, Target - DeskPlaceProgress)));
	}

	return Line;
}

bool UMissionManagerSubsystem::GetMissionProgress(int64& OutCurrent, int64& OutTarget) const
{
	if (!HasActiveMission())
	{
		return false;
	}

	// M1 — 벽돌 보유량 진행도
	if (ActiveMissionRow.ConditionType == EMissionConditionType::CollectBricks)
	{
		UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
		OutTarget = ActiveMissionRow.ConditionAmount;
		OutCurrent = ResMgr ? FMath::Min(ResMgr->GetResourceAmount(EResourceType::Brick), OutTarget) : 0;
		return true;
	}

	if (ActiveMissionRow.ConditionType == EMissionConditionType::PlaceDesks)
	{
		OutTarget = FMath::Max(1, ActiveMissionRow.ConditionAmount);
		OutCurrent = FMath::Min(static_cast<int64>(DeskPlaceProgress), OutTarget);
		return true;
	}

	// 도달형 — 카운터가 아니라 빌딩 인원수 절대값. 벤치 재배정/재시작에도 값이 흔들리지 않는다.
	if (ActiveMissionRow.ConditionType == EMissionConditionType::RecruitEmployees)
	{
		OutTarget = FMath::Max(1, ActiveMissionRow.ConditionAmount);
		OutCurrent = 0;
		if (UCGGameInstance* CGGI = Cast<UCGGameInstance>(GetGameInstance()))
		{
			if (UEmployeeManager* EmpMgr = CGGI->GetSubsystem<UEmployeeManager>())
			{
				const int32 Count = EmpMgr->GetEmployeeCountInBuilding(CGGI->GetCurrentManagedBuildingIndex());
				OutCurrent = FMath::Min(static_cast<int64>(Count), OutTarget);
			}
		}
		return true;
	}

	return false;
}

UMissionTrackerWidget* UMissionManagerSubsystem::GetActiveTracker() const
{
	if (TrackerWidget)
	{
		return TrackerWidget.Get();
	}
	return FindEmbeddedTracker();
}

UMissionTrackerWidget* UMissionManagerSubsystem::FindEmbeddedTracker() const
{
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return nullptr;
	}

	// 이름("MissionTracker") 바인딩 우선 — 실패 시 레이어 트리에서 타입으로 첫 인스턴스 탐색(이름 불일치 배치 구제).
	// (이름이 다르면 바인딩이 안 잡혀 뷰포트 폴백이 또 생겨 카드가 2장 겹친다)
	auto ResolveInLayer = [](UUserWidget* Layer, UMissionTrackerWidget* ByName) -> UMissionTrackerWidget*
	{
		if (ByName)
		{
			return ByName;
		}
		if (!Layer || !Layer->WidgetTree)
		{
			return nullptr;
		}
		UMissionTrackerWidget* Found = nullptr;
		Layer->WidgetTree->ForEachWidget([&Found](UWidget* W)
		{
			if (!Found)
			{
				Found = Cast<UMissionTrackerWidget>(W);
			}
		});
		return Found;
	};

	// MainMap = InGameLayer / OfficeMap = OfficeLayer. 레벨 전환 시 ClearAllUI 로 한쪽만 유효.
	if (UInGameLayerWidget* InGameLayer = UIMgr->GetInGameLayer())
	{
		if (UMissionTrackerWidget* Embedded = ResolveInLayer(InGameLayer, InGameLayer->GetMissionTracker()))
		{
			return Embedded;
		}
	}
	if (UOfficeLayerWidget* OfficeLayer = UIMgr->GetOfficeLayer())
	{
		if (UMissionTrackerWidget* Embedded = ResolveInLayer(OfficeLayer, OfficeLayer->GetMissionTracker()))
		{
			return Embedded;
		}
	}
	// OfficeMap 트래커는 UI_OfficeMapMain 좌레일에 배치 (GDS 레일 개편으로 OfficeLayer 에서 이사)
	if (UOfficeMainWidget* OfficeMain = UIMgr->GetOfficeMain())
	{
		if (UMissionTrackerWidget* Embedded = ResolveInLayer(OfficeMain, OfficeMain->GetMissionTracker()))
		{
			return Embedded;
		}
	}
	return nullptr;
}

void UMissionManagerSubsystem::EnsureMissionWidgets()
{
	// 순환 의존(GoalBoardSubsystem 이 이 매니저를 InitializeDependency) 을 피한 지연 구독 — 1회 래치.
	// TableMgr/PC 가 필요 없으므로 아래 위젯 가드보다 앞 — 초기 프레임에 위젯이 없다고 구독까지 미루면 안 된다
	if (UGoalBoardSubsystem* HookMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>())
	{
		if (!bGoalBoardWidgetHookBound)
		{
			bGoalBoardWidgetHookBound = true;
			HookMgr->OnGoalBoardChanged.AddUObject(this, &UMissionManagerSubsystem::HandleGoalBoardChangedForWidgets);
			HookMgr->OnTrackedGoalChanged.AddUObject(this, &UMissionManagerSubsystem::HandleTrackedGoalChanged);
		}
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!TableMgr || !PC)
	{
		return;
	}

	if (!TrackerWidget)
	{
		// 1순위 — 디자이너가 활성 레이어(MainMap=InGameLayer / OfficeMap=OfficeLayer)에 배치한
		// 임베드 인스턴스를 그대로 사용 (소유권 = 레이어 → RemoveFromParent 금지)
		UMissionTrackerWidget* Embedded = FindEmbeddedTracker();

		if (Embedded)
		{
			TrackerWidget = Embedded;
			bTrackerEmbedded = true;
			TrackerWidget->InitTracker(this);
			UE_LOG(LogTemp, Log, TEXT("[MissionManager] 트래커 = 레이어 임베드 인스턴스 (%s)"), *Embedded->GetName());
		}
		else if (TSubclassOf<UUserWidget> TrackerClass = TableMgr->GetWidgetClass(EWidgetType::MissionTracker))
		{
			// 폴백 — 임베드 인스턴스가 없으면 매니저가 직접 생성해 좌하단 포인트 앵커로 뷰포트에 부착
			TrackerWidget = CreateWidget<UMissionTrackerWidget>(PC, TrackerClass);
			if (TrackerWidget)
			{
				bTrackerEmbedded = false;
				TrackerWidget->InitTracker(this);
				// 프롬프트 모달(UIBase, ZOrder 0) 위 / 토스트(9999)·다이얼로그(10000) 아래
				TrackerWidget->AddToViewport(8000);
				// 카드 루트엔 좌하단 앵커 캔버스가 없어 풀스크린으로 늘어나므로 뷰포트 슬롯에서 직접 고정
				TrackerWidget->SetAnchorsInViewport(FAnchors(0.f, 1.f, 0.f, 1.f));
				TrackerWidget->SetAlignmentInViewport(FVector2D(0.f, 1.f));
				TrackerWidget->SetPositionInViewport(FVector2D(40.f, -40.f), false);
				UE_LOG(LogTemp, Warning, TEXT("[MissionManager] 트래커 = 뷰포트 폴백 생성 (활성 레이어에 임베드 트래커 미발견 — WBP 배치/이름 확인)"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MissionManager] MissionTracker 위젯 클래스 없음 (DT_WidgetClass 행 확인)"));
		}
	}

	// ===== 미션판 트래커 — 체인이 끝났고 미수령 미션이 남아 있을 때만 =====
	UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>();

	const bool bWantGoalTracker = GoalMgr && !HasActiveMission() && GoalMgr->IsUnlocked() && GoalMgr->HasAnyUnclaimedGoal();

	if (bWantGoalTracker && !GoalTrackerWidget)
	{
		if (TSubclassOf<UUserWidget> GoalTrackerClass = TableMgr->GetWidgetClass(EWidgetType::GoalTracker))
		{
			GoalTrackerWidget = CreateWidget<UGoalTrackerWidget>(PC, GoalTrackerClass);
			if (GoalTrackerWidget)
			{
				GoalTrackerWidget->InitTracker();

				// 기존 체인 카드가 이미 HUD 크롬 위 / Bottom·Prompt 스택 아래의 올바른 레이어에 있다.
				// 그 부모에 형제로 붙이면 상위 CommonUI 스택을 재배치하지 않고 같은 위치·레이어를 그대로 쓴다.
				bool bPlaced = false;
				if (UMissionTrackerWidget* EmbeddedTracker = FindEmbeddedTracker())
				{
					if (UVerticalBox* TrackerColumn = Cast<UVerticalBox>(EmbeddedTracker->GetParent()))
					{
						const UVerticalBoxSlot* MissionBoxSlot = Cast<UVerticalBoxSlot>(EmbeddedTracker->Slot);
						if (UVerticalBoxSlot* GoalBoxSlot = TrackerColumn->AddChildToVerticalBox(GoalTrackerWidget))
						{
							if (MissionBoxSlot)
							{
								GoalBoxSlot->SetSize(MissionBoxSlot->GetSize());
								GoalBoxSlot->SetPadding(MissionBoxSlot->GetPadding());
								GoalBoxSlot->SetHorizontalAlignment(MissionBoxSlot->GetHorizontalAlignment());
								GoalBoxSlot->SetVerticalAlignment(MissionBoxSlot->GetVerticalAlignment());
							}
							bPlaced = true;
						}
					}
					else if (UOverlay* TrackerOverlay = Cast<UOverlay>(EmbeddedTracker->GetParent()))
					{
						const UOverlaySlot* MissionOverlaySlot = Cast<UOverlaySlot>(EmbeddedTracker->Slot);
						if (UOverlaySlot* GoalOverlaySlot = TrackerOverlay->AddChildToOverlay(GoalTrackerWidget))
						{
							if (MissionOverlaySlot)
							{
								GoalOverlaySlot->SetPadding(MissionOverlaySlot->GetPadding());
								GoalOverlaySlot->SetHorizontalAlignment(MissionOverlaySlot->GetHorizontalAlignment());
								GoalOverlaySlot->SetVerticalAlignment(MissionOverlaySlot->GetVerticalAlignment());
							}
							bPlaced = true;
						}
					}
				}
				if (!bPlaced)
				{
					// 임베드 체인 카드가 없거나 부모 타입이 바뀌면 기존 뷰포트 경로로 폴백한다.
					UE_LOG(LogTemp, Warning, TEXT("[MissionManager] 임베드 트래커 위치 재사용 실패 — 미션판을 뷰포트 폴백으로 띄웁니다"));
					GoalTrackerWidget->AddToViewport(8000);
					GoalTrackerWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
					GoalTrackerWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
					GoalTrackerWidget->SetPositionInViewport(FVector2D(40.f, 300.f), false);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MissionManager] GoalTracker 위젯 클래스 없음 (DT_WidgetClass 행 확인)"));
		}
	}
	else if (!bWantGoalTracker && GoalTrackerWidget)
	{
		// 전량 수령 / 체인 재점화 등으로 조건이 깨지면 즉시 내린다
		GoalTrackerWidget->RemoveFromParent();
		GoalTrackerWidget = nullptr;
	}

	if (!GuideOverlayWidget)
	{
		if (TSubclassOf<UUserWidget> OverlayClass = TableMgr->GetWidgetClass(EWidgetType::MissionGuideOverlay))
		{
			GuideOverlayWidget = CreateWidget<UMissionGuideOverlayWidget>(PC, OverlayClass);
			if (GuideOverlayWidget)
			{
				GuideOverlayWidget->InitGuideOverlay(this);
				GuideOverlayWidget->AddToViewport(9000);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MissionManager] MissionGuideOverlay 위젯 클래스 없음 (DT_WidgetClass 행 확인)"));
		}
	}

	// 맵 전환이 밴드를 지운 뒤(DestroyMissionWidgets) 안내 중인 미션 문구를 되살린다 — 변경 이벤트는 이미 지나갔다.
	// 체인 중엔 손대지 않는다(UpdateGuideNotification 소유). 문구가 같으면 컨테이너가 무시(멱등)
	if (GoalMgr && !HasActiveMission() && !GoalMgr->GetTrackedGoalID().IsNone())
	{
		HandleTrackedGoalChanged(GoalMgr->GetTrackedGoalID());
	}
}

void UMissionManagerSubsystem::DestroyMissionWidgets()
{
	if (TrackerWidget)
	{
		// 임베드 인스턴스는 레이어(InGame/Office) 소유라 떼어내면 안 됨 — 포인터만 비워 다음 Ensure에서 재조회
		if (!bTrackerEmbedded)
		{
			TrackerWidget->RemoveFromParent();
		}
		TrackerWidget = nullptr;
		bTrackerEmbedded = false;
	}
	if (GoalTrackerWidget)
	{
		GoalTrackerWidget->RemoveFromParent();
		GoalTrackerWidget = nullptr;
	}
	if (GuideOverlayWidget)
	{
		GuideOverlayWidget->RemoveFromParent();
		GuideOverlayWidget = nullptr;
	}

	// 가이드 상주 알림도 함께 정리 (레벨 전환/PIE 종료 시 잔류 방지)
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ClearGuideNotification();
	}
}

void UMissionManagerSubsystem::HandleGoalBoardChangedForWidgets()
{
	// 언락/수령으로 표시 조건이 바뀔 수 있다 — 재평가는 Ensure 가 전담
	EnsureMissionWidgets();
}

void UMissionManagerSubsystem::HandleTrackedGoalChanged(FName TrackedID)
{
	// 체인이 살아 있으면 밴드는 UpdateGuideNotification 소유 — 치트 재점화 중 자동 수령이 체인 밴드를 지우지 않게
	if (HasActiveMission())
	{
		return;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!UIMgr)
	{
		return;
	}
	FGoalTable Row;
	if (!TrackedID.IsNone() && TableMgr && TableMgr->GetGoalData(TrackedID, Row))
	{
		UIMgr->ShowGuideNotification(Row.Desc);
	}
	else
	{
		UIMgr->ClearGuideNotification();
	}
}
